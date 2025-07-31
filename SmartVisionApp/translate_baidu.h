#pragma once

#include <QString>
#include <QMap>
#include <QThread>
#include <QMutex>

class TranslateTask : public QThread {
	Q_OBJECT
public:
	TranslateTask(const QString& cacheFilePath);
	~TranslateTask() = default;

	void translate(const QStringList& word, std::vector<float>& scores);
signals:
	void signalTranslationFinished(QString result);

protected:
	void run() override;

private:
	

	void loadCacheFromFile();
	void saveEntryToFile(const QString& word, const QString& result);
	QString fetchFromBaiduAPI(const QString& word);

	QString m_cachePath;
	QMap<QString, QString> m_cache;
	QStringList m_words;
	std::vector<float> m_scores; // 用于存储翻译分数
	QMutex m_mutex; // 保护 m_words 和 m_scores 的互斥锁
};
