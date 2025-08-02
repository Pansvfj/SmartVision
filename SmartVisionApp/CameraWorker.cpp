#include "stdafx.h"
#include "CameraWorker.h"
#include <QDebug>
#include <QMetaType>
#include <QCameraInfo>
#include <QThreadPool>
#include <QTime>

#include "YoloDetector.h"

CameraWorker::CameraWorker(QObject* parent)
	: QObject(parent)
{
	m_timer = new QTimer(this);
	connect(m_timer, &QTimer::timeout, this, &CameraWorker::capture);
}

CameraWorker::~CameraWorker()
{
	slotStopCamera();
}

void CameraWorker::slotStartCamera()
{
	if (m_running) return;

	if (!m_cap.open(0, cv::CAP_DSHOW)){
		emit signalCameraError(tr("摄像头打开失败"));
		return;
	}

	m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_width);
	m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
	m_cap.set(cv::CAP_PROP_FPS, m_fps);

	qDebug() << "设置后的分辨率:" << m_cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x" << m_cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	qDebug() << "设置后的帧率:" << m_cap.get(cv::CAP_PROP_FPS);

	m_running = true;
	m_lastTick = static_cast<double>(cv::getTickCount());
	m_frameCount = 0;
	m_timer->start(30);
}


void CameraWorker::slotStopCamera()
{
	if (!m_running) return;

	m_timer->stop();
	if (m_cap.isOpened())
		m_cap.release();

	m_running = false;

	emit signalStopped();

}

void CameraWorker::slotRequestCameraDetail()
{
	if (!m_cap.isOpened()) {
		emit signalCameraDetailReady("摄像头未打开");
		return;
	}

	double width = m_cap.get(cv::CAP_PROP_FRAME_WIDTH);
	double height = m_cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	double fps = m_cap.get(cv::CAP_PROP_FPS);

	QString name = "Unknown Device";
	const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
	for (int i = 0; i < cameras.size(); ++i) {
		cv::VideoCapture test(i);
		if (!test.isOpened()) continue;

		double w = test.get(cv::CAP_PROP_FRAME_WIDTH);
		double h = test.get(cv::CAP_PROP_FRAME_HEIGHT);

		if (qAbs(w - width) < 2 && qAbs(h - height) < 2) {
			name = cameras[i].description();
			test.release();
			break;
		}
		test.release();
	}

	QString detail = QString("Camera: %1\nResolution: %2x%3\nFps: %4 fps")
		.arg(name).arg(width).arg(height).arg(fps, 0, 'f', 1);

	emit signalCameraDetailReady(detail);
}


void CameraWorker::capture()
{
	if (!m_cap.isOpened()) return;

	cv::Mat frame;
	m_cap >> frame;
	if (frame.empty()) return;

	// 始终统计帧率
	double now = static_cast<double>(cv::getTickCount());
	double dt = (now - m_lastTick) / cv::getTickFrequency();
	m_lastTick = now;
	if (dt > 0) {
		m_fps = 0.9 * m_fps + 0.1 * (1.0 / dt);
	}

	// 绘制 FPS 与时间戳
	QString fpsText = QString("FPS: %1").arg(m_fps, 0, 'f', 1);
	QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");

	cv::rectangle(frame, cv::Point(5, 5), cv::Point(200, 50), cv::Scalar(0, 0, 0), cv::FILLED);
	cv::putText(frame, fpsText.toStdString(), cv::Point(10, 25),
		cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 128), 1);
	cv::putText(frame, timeStr.toStdString(), cv::Point(10, 45),
		cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);

	// YOLO 实时识别（异步交给 YoloWork）
	if (m_detector) {
		emit signalYoloFrameReady(frame.clone());
		// 注意：YOLO 路径下不 emit signalFrameReady
		return;
	}

	// 发送帧图像到主线程显示
	QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
	emit signalFrameReady(img.copy());
}

