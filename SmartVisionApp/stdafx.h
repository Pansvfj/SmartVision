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
#include <onnxruntime_cxx_api.h>

#define NOMINMAX       // 禁止 Windows.h 定义 min/max 宏
//#include <windows.h>   // 如果你项目里需要用到 Windows API


// 可选：检测 DML 工厂头是否存在（有的 ORT 包没有 DML 头，避免编译报错）
#if defined(_WIN32)
#  if __has_include(<onnxruntime/core/providers/dml/dml_provider_factory.h>)
#    include <onnxruntime/core/providers/dml/dml_provider_factory.h>
#    define HAS_ORT_DML 1
#  elif __has_include(<dml_provider_factory.h>)
#    include <dml_provider_factory.h>
#    define HAS_ORT_DML 1
#  else
#    define HAS_ORT_DML 0
#  endif
#else
#  define HAS_ORT_DML 0
#endif

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

extern bool g_useCUDA;

// 解决中文路径图像读取
cv::Mat imreadWithChinese(const QString& filePath);

int getTextWidth(const QFont& font, const QString& str);

void AppendWithFallback(Ort::SessionOptions& so, int device_id);

bool LoadDLL(const QString& dllPath);

#endif	// STDAFX_H