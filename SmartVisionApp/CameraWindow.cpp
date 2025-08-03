#include "stdafx.h"
#include "CameraWindow.h"
#include "CameraConfigDialog.h"
#include <QDebug>
#include <QMetaObject>
#include <QCoreApplication>
#include <QTime>

CameraWindow::CameraWindow(QWidget* parent)
	: QWidget(parent)
{
	// UI 初始化
	m_videoLabel = new QLabel("", this);
	m_videoLabel->setMinimumSize(640, 480);
	m_videoLabel->setAlignment(Qt::AlignCenter);
	m_videoLabel->setStyleSheet("QLabel { background-color: black; color: white; font-size: 20px; }");

	m_btnOpen = new QPushButton("打开摄像头", this);
	m_btnClose = new QPushButton("关闭摄像头", this);
	m_btnConfig = new QPushButton("摄像头设置", this);
	m_btnToggleYolo = new QPushButton("开启识别", this);

	QHBoxLayout* btnLayout = new QHBoxLayout();
	btnLayout->addWidget(m_btnOpen);
	btnLayout->addWidget(m_btnClose);
	btnLayout->addWidget(m_btnConfig);
	btnLayout->addWidget(m_btnToggleYolo);

	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->addWidget(m_videoLabel);
	mainLayout->addLayout(btnLayout);
	setLayout(mainLayout);

	// 初始化线程和工作对象
	m_cameraWorker = new CameraWorker();
	m_cameraWorker->moveToThread(&m_cameraWorkerThread);
	//connect(&m_cameraWorkerThread, &QThread::finished, m_cameraWorker, &QObject::deleteLater);
	m_cameraWorkerThread.start();
	connect(this, &CameraWindow::signalGetCameraDetail, m_cameraWorker, &CameraWorker::slotRequestCameraDetail);

	m_yolo = new YoloDetector("D:/Projects/SmartVision/model/yolov5s.onnx",
		"D:/Projects/SmartVision/model/coco.names");
	m_yoloWork = new YoloStreamWork(nullptr, m_yolo);
	m_yoloWork->moveToThread(&m_yoloThread);
	//connect(&m_yoloThread, &QThread::finished, m_yoloWork, &QObject::deleteLater);
	m_yoloThread.start();

	// 信号连接
	connect(m_btnOpen, &QPushButton::clicked, this, &CameraWindow::onOpenCamera);
	connect(m_btnClose, &QPushButton::clicked, this, &CameraWindow::onCloseCamera);
	connect(m_btnConfig, &QPushButton::clicked, this, &CameraWindow::onConfigCamera);
	connect(m_btnToggleYolo, &QPushButton::clicked, this, &CameraWindow::onToggleYolo);

	connect(m_cameraWorker, &CameraWorker::signalFrameReady, this, &CameraWindow::onFrameCaptured);
	connect(m_cameraWorker, &CameraWorker::signalYoloFrameReady, this, &CameraWindow::onYoloFrameCaptured);
	connect(m_cameraWorker, &CameraWorker::signalCameraError, this, [=]() {
		m_videoLabel->setPixmap(QPixmap());
		m_videoLabel->setText(tr("摄像头启动失败"));
	});
	connect(m_yoloWork, &YoloStreamWork::signalGetResult, this, &CameraWindow::onYoloResult);
}

CameraWindow::~CameraWindow()
{
	if (!m_cameraClosed) {
		onCloseCamera();  // 自动清理
	}

	delete m_cameraWorker;
	delete m_yoloWork;
	delete m_yolo;
}

void CameraWindow::closeEvent(QCloseEvent* event)
{
	QWidget::closeEvent(event);
	onCloseCamera();
}

void CameraWindow::onOpenCamera()
{
	m_cameraClosed = false;
	m_videoLabel->setText(tr("摄像头启动中"));
	if (m_cameraWorker) {
		if (!m_cameraWorkerThread.isRunning()) {
			m_cameraWorkerThread.start();
		}
		QMetaObject::invokeMethod(m_cameraWorker, "slotStartCamera", Qt::QueuedConnection);
	}
}

void CameraWindow::onCloseCamera()
{
	if (m_cameraClosed) return;
	m_cameraClosed = true;

	if (m_cameraWorker) {
		QMetaObject::invokeMethod(m_cameraWorker, "slotStopCamera", Qt::QueuedConnection);
	}
	if (m_yoloEnabled) {
		m_cameraWorker->setYolo(nullptr);
	}
	m_yoloEnabled = false;

	if (m_cameraWorkerThread.isRunning()) {
		m_cameraWorkerThread.quit();
		m_cameraWorkerThread.wait();
	}
	if (m_yoloThread.isRunning()) {
		m_yoloThread.quit();
		m_yoloThread.wait();
	}

	QTimer::singleShot(100, this, [=]() {
		m_videoLabel->setPixmap(QPixmap());
		m_videoLabel->setText(tr("摄像头已关闭"));
		m_btnToggleYolo->setText(tr("已关闭识别"));
	});
}


void CameraWindow::onConfigCamera()
{
	if (!m_cameraWorker) return;

	connect(m_cameraWorker, &CameraWorker::signalCameraDetailReady, this, [=](const QString& info) {
		if (m_dlgCameraConfig && m_dlgCameraConfig->isVisible()) {
			return;
		}

		int w = m_cameraWorker->getWidth();
		int h = m_cameraWorker->getHeight();
		float fps = m_cameraWorker->getFps();

		m_dlgCameraConfig = new CameraConfigDialog(w, h, fps, info, this);
		connect(m_dlgCameraConfig, &CameraConfigDialog::configSelected, this, [=](int nw, int nh, int nfps) {
			m_cameraWorker->setConfig(nw, nh, nfps);
			QMetaObject::invokeMethod(m_cameraWorker, "slotStopCamera", Qt::BlockingQueuedConnection);
			QMetaObject::invokeMethod(m_cameraWorker, "slotStartCamera", Qt::QueuedConnection);
			});
		m_dlgCameraConfig->exec();
	}, Qt::UniqueConnection);

	emit signalGetCameraDetail();
}

void CameraWindow::onToggleYolo()
{
	m_yoloEnabled = !m_yoloEnabled;
	if (m_yoloEnabled) {
		m_cameraWorker->setYolo(m_yolo);
		if (!m_yoloThread.isRunning()) {
			m_yoloThread.start();
		}
	}
	else {
		m_cameraWorker->setYolo(nullptr);
	}
	m_btnToggleYolo->setText(m_yoloEnabled ? tr("已开启识别") :  tr("已关闭识别"));
}

void CameraWindow::onFrameCaptured(const QImage& img)
{
	m_videoLabel->setPixmap(QPixmap::fromImage(img).scaled(m_videoLabel->size(), Qt::KeepAspectRatio));
}

void CameraWindow::onYoloFrameCaptured(const cv::Mat& frame)
{
	if (m_yoloBusy) {
		//qDebug() << "yolo busy!";
		return;
	}

	if (!m_yoloEnabled) {
		qDebug() << "yolo NOT enabled!";
	}

	m_yoloBusy = true;
	QMetaObject::invokeMethod(m_yoloWork, "doYoloDetect",
		Qt::QueuedConnection,
		Q_ARG(cv::Mat, frame.clone()));
}

void CameraWindow::onYoloResult(const cv::Mat& result, const std::vector<YoloDetection>&) {
	// 静态变量用于 FPS 统计
	static double m_lastTick = 0;
	static float fps = 0.0f;

	double now = static_cast<double>(cv::getTickCount());
	double dt = (now - m_lastTick) / cv::getTickFrequency();
	m_lastTick = now;
	if (dt > 0) {
		fps = 0.9f * fps + 0.1f * (1.0f / dt);
	}

	// 文本内容
	QString fpsText = QString("FPS: %1").arg(fps, 0, 'f', 1);
	QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");

	// 克隆图像用于绘制
	cv::Mat frame = result.clone();

	// 背景框
	cv::rectangle(frame, cv::Point(5, 5), cv::Point(200, 50), cv::Scalar(0, 0, 0), cv::FILLED);
	// 帧率（绿色）
	cv::putText(frame, fpsText.toStdString(), cv::Point(10, 25),
		cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 128), 1);
	// 时间（黄色）
	cv::putText(frame, timeStr.toStdString(), cv::Point(10, 45),
		cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);

	// 显示到 UI
	QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
	emit m_cameraWorker->signalFrameReady(img.copy());

	m_yoloBusy = false;
}

