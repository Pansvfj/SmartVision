#include "stdafx.h"
#include "YoloWork.h"
#include "YoloDetector.h"

YoloWork::YoloWork(YoloDetector* detector) : m_detector(detector) 
{
}

YoloWork::~YoloWork()
{
}

void YoloWork::doWork(const QString& filePath)
{
	if (!m_detector)  {
		emit signalGetResult(false, {});
		return;
	}

	if (filePath.isEmpty()) {
		emit signalGetResult(false, {});
		return;
	}

	cv::Mat img = imreadWithChinese(filePath);
	if (img.empty()) {
		emit signalGetResult(false, {});
		return;
	}

	std::vector<YoloDetection> detections = m_detector->detect(img);
	drawDetections(img, detections);

	QStringList labelWithScores;
	for (const auto& det : detections) {
		QString item = QString("%1 (%2%)").arg(det.label).arg(det.confidence * 100, 0, 'f', 1);
		labelWithScores << item;
	}

	QImage resultImg(img.data, img.cols, img.rows, img.step, QImage::Format_BGR888);
	emit signalGetResult(true, { resultImg.copy() /*²»¼Ócopy±ÀÀ£*/, labelWithScores});
}