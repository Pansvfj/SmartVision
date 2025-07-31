#include "stdafx.h"

cv::Mat imreadWithChinese(const QString& filePath) {
	QFile file(filePath);
	if (!file.exists()) return cv::Mat();

	if (!file.open(QIODevice::ReadOnly)) return cv::Mat();

	QByteArray imageData = file.readAll();
	file.close();

	std::vector<uchar> buffer(imageData.begin(), imageData.end());
	return cv::imdecode(buffer, cv::IMREAD_COLOR);
}

int getTextWidth(const QFont& font, const QString& str)
{
	QFontMetrics metrics(font);
	return metrics.horizontalAdvance(str);
}