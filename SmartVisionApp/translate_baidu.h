#pragma once

#include <QString>
#include <QMap>

class TranslateHelper {
public:
	TranslateHelper(const QString& cacheFilePath);

	QString translate(const QString& word);

private:
	void loadCacheFromFile();
	void saveEntryToFile(const QString& word, const QString& result);
	QString fetchFromBaiduAPI(const QString& word);

	QString m_cachePath;
	QMap<QString, QString> m_cache;
};
