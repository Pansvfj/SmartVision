#include "stdafx.h"
#include "CameraWindow.h"
#include "CameraConfigDialog.h"
#include <QDebug>
#include <QMetaObject>
#include <QCoreApplication>

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

	m_cameraWorker->slotRequestCameraDetail();
	connect(m_cameraWorker, &CameraWorker::signalCameraDetailReady, this, [=](const QString& info) {
		int w = m_cameraWorker->getWidth();
		int h = m_cameraWorker->getHeight();
		float fps = m_cameraWorker->getFps();

		CameraConfigDialog* dialog = new CameraConfigDialog(w, h, fps, info, this);
		connect(dialog, &CameraConfigDialog::configSelected, this, [=](int nw, int nh, int nfps) {
			m_cameraWorker->setConfig(nw, nh, nfps);
			QMetaObject::invokeMethod(m_cameraWorker, "slotStopCamera", Qt::BlockingQueuedConnection);
			QMetaObject::invokeMethod(m_cameraWorker, "slotStartCamera", Qt::QueuedConnection);
			});
		dialog->exec();
	}, Qt::UniqueConnection);
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
		qDebug() << "yolo busy!";
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

void CameraWindow::onYoloResult(const cv::Mat& result, const std::vector<YoloDetection>&)
{
	QImage img(result.data, result.cols, result.rows, result.step, QImage::Format_BGR888);
	emit m_cameraWorker->signalFrameReady(img.copy());
	m_yoloBusy = false;
}
