#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <opencv2/opencv.hpp>

#include "CameraWorker.h"
#include "YoloDetector.h"
#include "YoloStreamWork.h"

class CameraWindow : public QWidget
{
	Q_OBJECT

public:
	explicit CameraWindow(QWidget* parent = nullptr);
	~CameraWindow();


private slots:
	void onOpenCamera();
	void onCloseCamera();
	void onConfigCamera();
	void onToggleYolo();

	void onFrameCaptured(const QImage& img);
	void onYoloFrameCaptured(const cv::Mat& frame);
	void onYoloResult(const cv::Mat& frame, const std::vector<YoloDetection>& detections);

private:
	// UI 控件
	QLabel* m_videoLabel = nullptr;
	QPushButton* m_btnOpen = nullptr;
	QPushButton* m_btnClose = nullptr;
	QPushButton* m_btnConfig = nullptr;
	QPushButton* m_btnToggleYolo = nullptr;

	// 摄像头线程
	CameraWorker* m_cameraWorker = nullptr;
	QThread m_cameraWorkerThread;

	// YOLO 推理线程
	YoloStreamWork* m_yoloWork = nullptr;
	QThread m_yoloThread;
	YoloDetector* m_yolo = nullptr;

	// 状态控制
	bool m_yoloEnabled = false;
	bool m_yoloBusy = false;
};
