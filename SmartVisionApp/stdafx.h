#ifndef STDAFX_H
#define STDAFX_H

#pragma execution_character_set("utf-8")

#include <vector>
#include <utility>
#include <string>

#include <QObject>
#include <QMetaType>
#include <QFile>
#include <QTextBrowser>
#include <QTimer>
#include <QStyle>
#include <QFontMetrics>
#include <QFont>
#include <QVBoxLayout>
#include <QImage>
#include <QStringList>
#include <QThread>

#include <QDebug>

#include <opencv2/imgcodecs.hpp>


struct YoloDetection {
	QRect bbox;
	QString label;
	float confidence;
};

using GetModelResultType = std::vector<std::pair<std::string, float>>;
using GetYoloResultType = QPair<QImage, QStringList>;

Q_DECLARE_METATYPE(GetModelResultType)
Q_DECLARE_METATYPE(GetYoloResultType)
Q_DECLARE_METATYPE(std::vector<float>)
Q_DECLARE_METATYPE(std::vector<YoloDetection>)
Q_DECLARE_METATYPE(cv::Mat)

// 解决中文路径图像读取
cv::Mat imreadWithChinese(const QString& filePath);

int getTextWidth(const QFont& font, const QString& str);

#endif	// STDAFX_H