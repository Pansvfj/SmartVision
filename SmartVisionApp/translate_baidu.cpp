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

TranslateHelper::TranslateHelper(const QString& cacheFilePath)
	: m_cachePath(cacheFilePath)
{
	loadCacheFromFile();
}

QString TranslateHelper::translate(const QString& word)
{
	if (m_cache.contains(word))
		return m_cache[word];

	QString translated = fetchFromBaiduAPI(word);
	if (!translated.isEmpty()) {
		m_cache[word] = translated;
		saveEntryToFile(word, translated);
	}
	return translated;
}

void TranslateHelper::loadCacheFromFile()
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

void TranslateHelper::saveEntryToFile(const QString& word, const QString& result)
{
	QFile file(m_cachePath);
	if (!file.open(QIODevice::Append | QIODevice::Text))
		return;

	QTextStream out(&file);
	out << word << "=" << result << "\n";
	file.close();
}

QString TranslateHelper::fetchFromBaiduAPI(const QString& word)
{
	QString appid = "20250727002417024";
	QString key = "LncIR992BVZYE9czk_An";
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

	QNetworkAccessManager manager;
	QNetworkRequest request(url);
	QNetworkReply* reply = manager.get(request);

	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	QString result = word;
	if (reply->error() == QNetworkReply::NoError) {
		QByteArray data = reply->readAll();
		QJsonDocument doc = QJsonDocument::fromJson(data);
		auto array = doc["trans_result"].toArray();
		if (!array.isEmpty()) {
			result = array[0].toObject()["dst"].toString();
		}
	}
	else {
		qDebug() << "Error fetching translation:" << reply->errorString();
	}

	reply->deleteLater();

	return result;
}
