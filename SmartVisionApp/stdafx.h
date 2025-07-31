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

#include <opencv2/imgcodecs.hpp>

using GetModelResultType = std::vector<std::pair<std::string, float>>;
using GetYoloResultType = QPair<QImage, QStringList>;

Q_DECLARE_METATYPE(GetModelResultType)
Q_DECLARE_METATYPE(GetYoloResultType)

// 解决中文路径图像读取
cv::Mat imreadWithChinese(const QString& filePath);

int getTextWidth(const QFont& font, const QString& str);

#endif	// STDAFX_H