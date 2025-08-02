#pragma once

#include <QObject>
#include <QTimer>
#include <QImage>
#include <opencv2/opencv.hpp>

class YoloDetector;

class CameraWorker : public QObject
{
	Q_OBJECT
public:
	explicit CameraWorker(QObject* parent = nullptr);
	~CameraWorker();

	void setConfig(int w, int h, int f) { m_width = w; m_height = h; m_fps = f; }

	void setYolo(YoloDetector* detector) { m_detector = detector; }

	int getWidth() const { return m_width; }
	int getHeight() const { return m_height; }
	float getFps() const { return m_fps; }


signals:
	void signalYoloFrameReady(const cv::Mat& frame);
	void signalStopped();



public slots:
	void slotStartCamera();
	void slotStopCamera();
	void slotRequestCameraDetail();


signals:
	void signalFrameReady(const QImage& frame);
	void signalCameraError(const QString& msg);
	void signalCameraDetailReady(const QString& detail);

private:
	QTimer* m_timer = nullptr;
	cv::VideoCapture m_cap;
	bool m_running = false;

	// ÷°¬ œ‡πÿ
	int m_frameCount = 0;
	double m_lastTick = 0;

	void capture();

	int m_width = 640;
	int m_height = 480;
	float m_fps = 30.0;

	YoloDetector* m_detector = nullptr;
};
