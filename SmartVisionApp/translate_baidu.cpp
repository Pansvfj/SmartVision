#include "stdafx.h"
#include "translate_baidu.h"
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRandomGenerator>

TranslateTask::TranslateTask(const QString& cacheFilePath): m_cachePath(cacheFilePath)
{
	loadCacheFromFile();
	//connect(this, &QThread::finished, this, &QThread::deleteLater);
}

void TranslateTask::translate(const QStringList& word, std::vector<float>& scores)
{
	QMutexLocker locker(&m_mutex);  // 加锁保护
	m_words = word;
	m_scores = std::move(scores);
}

void TranslateTask::loadCacheFromFile()
{
	QFile file(m_cachePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	QTextStream in(&file);
	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if (line.isEmpty() || !line.contains('=')) continue;
		QStringList parts = line.split('=');
		if (parts.size() == 2)
			m_cache[parts[0].trimmed()] = parts[1].trimmed();
	}
	file.close();
}

void TranslateTask::saveEntryToFile(const QString& word, const QString& result)
{
	QFile file(m_cachePath);
	if (!file.open(QIODevice::Append | QIODevice::Text))
		return;

	QTextStream out(&file);
	out << word << "=" << result << "\n";
	file.close();
}

QString TranslateTask::fetchFromBaiduAPI(const QString& word)
{
	const QString appid = "20250727002417024";
	const QString key = "LncIR992BVZYE9czk_An";

	const int maxRetries = 3;
	const int timeoutMs = 3000;

	QString salt = QString::number(QRandomGenerator::global()->generate());
	QString signSrc = appid + word + salt + key;
	QString sign = QCryptographicHash::hash(signSrc.toUtf8(), QCryptographicHash::Md5).toHex();

	QUrl url("https://fanyi-api.baidu.com/api/trans/vip/translate");
	QUrlQuery query;
	query.addQueryItem("q", word);
	query.addQueryItem("from", "en");
	query.addQueryItem("to", "zh");
	query.addQueryItem("appid", appid);
	query.addQueryItem("salt", salt);
	query.addQueryItem("sign", sign);
	url.setQuery(query);

	QString result = word;
	int retry = 0;

	while (retry < maxRetries) {
		QNetworkAccessManager manager;
		QNetworkRequest request(url);
		QNetworkReply* reply = manager.get(request);

		QEventLoop loop;
		QTimer timeoutTimer;
		timeoutTimer.setSingleShot(true);

		// 超时触发 quit
		QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
		QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

		timeoutTimer.start(timeoutMs);
		loop.exec();

		if (timeoutTimer.isActive() && reply->error() == QNetworkReply::NoError) {
			QByteArray data = reply->readAll();
			QJsonDocument doc = QJsonDocument::fromJson(data);
			auto array = doc["trans_result"].toArray();
			if (!array.isEmpty()) {
				result = array[0].toObject()["dst"].toString();
			}
			reply->deleteLater();
			break; // 成功
		}
		else {
			qWarning() << "翻译失败:" << reply->errorString() << "，第" << retry + 1 << "次重试";
			reply->abort();
			reply->deleteLater();
			++retry;
		}
	}

	return result;
}

void TranslateTask::run()
{
	QStringList localWords;
	std::vector<float> localScores;
	{
		QMutexLocker locker(&m_mutex);
		localWords = m_words;
		localScores = m_scores;
	}

	int wordSize = localWords.size();
	int scoreSize = localScores.size();

	if (wordSize != scoreSize) {
		emit signalTranslationFinished("");
		return;
	}

	QString resultWordss;
	for (int i = 0; i < wordSize; i++) {

		QString resultWord;

		if (m_cache.contains(localWords[i])) {
			resultWord = QString("%1 %2%").arg(m_cache[localWords[i]]).arg(localScores[i] * 100, 0, 'f', 2);
			resultWordss = resultWordss.append(resultWord).append("\n");
			continue;
		}

		QString translated = fetchFromBaiduAPI(localWords[i]);
		if (!translated.isEmpty()) {
			m_cache[localWords[i]] = translated;
			saveEntryToFile(localWords[i], translated);
			resultWord = QString("%1 %2%").arg(translated).arg(localScores[i] * 100, 0, 'f', 2);
			resultWordss = resultWordss.append(resultWord).append("\n");
		}
	}
	emit signalTranslationFinished(resultWordss);
}
