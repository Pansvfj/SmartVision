#pragma once

#include <QObject>
#include <opencv2/opencv.hpp>
#include "YoloDetector.h"

class YoloStreamWork : public QObject
{
	Q_OBJECT
public:
	YoloStreamWork(QObject* parent = nullptr, YoloDetector* detector = nullptr);
	~YoloStreamWork();

public slots:
	void doYoloDetect(const cv::Mat& frame);

signals:
	void signalGetResult(const cv::Mat& frame, const std::vector<YoloDetection>& detections);

private:
	YoloDetector* m_detector = nullptr;
};
