#include "stdafx.h"
#include "YoloStreamWork.h"
#include "YoloDetector.h"

YoloStreamWork::YoloStreamWork(QObject* parent, YoloDetector* detector)
	: QObject(parent), m_detector(detector)
{
}

YoloStreamWork::~YoloStreamWork()
{
}

void YoloStreamWork::doYoloDetect(const cv::Mat& frame)
{
	if (!m_detector) return;

	cv::Mat input = frame.clone();
	auto detections = m_detector->detect(input);

	cv::Mat draw = input.clone();
	drawDetections(draw, detections);

	emit signalGetResult(draw, detections);
}
