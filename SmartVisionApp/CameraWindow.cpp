#include "stdafx.h"
#include "CameraWindow.h"
#include "CameraConfigDialog.h"
#include <QDebug>
#include <QMetaObject>

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
	m_worker = new CameraWorker();
	m_worker->moveToThread(&m_workerThread);
	connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
	m_workerThread.start();

	m_yolo = new YoloDetector("D:/Projects/SmartVision/model/yolov5s.onnx",
		"D:/Projects/SmartVision/model/coco.names");
	m_yoloWork = new YoloStreamWork(nullptr, m_yolo);
	m_yoloWork->moveToThread(&m_yoloThread);
	m_yoloThread.start();

	// 信号连接
	connect(m_btnOpen, &QPushButton::clicked, this, &CameraWindow::onOpenCamera);
	connect(m_btnClose, &QPushButton::clicked, this, &CameraWindow::onCloseCamera);
	connect(m_btnConfig, &QPushButton::clicked, this, &CameraWindow::onConfigCamera);
	connect(m_btnToggleYolo, &QPushButton::clicked, this, &CameraWindow::onToggleYolo);

	connect(m_worker, &CameraWorker::signalFrameReady, this, &CameraWindow::onFrameCaptured);
	connect(m_worker, &CameraWorker::signalYoloFrameReady, this, &CameraWindow::onYoloFrameCaptured);
	connect(m_yoloWork, &YoloStreamWork::signalGetResult, this, &CameraWindow::onYoloResult);
}

CameraWindow::~CameraWindow()
{
	QMetaObject::invokeMethod(m_worker, "slotStopCamera", Qt::BlockingQueuedConnection);
	m_workerThread.quit();
	m_workerThread.wait();

	m_yoloThread.quit();
	m_yoloThread.wait();

	delete m_yolo;
}

void CameraWindow::onOpenCamera()
{
	m_videoLabel->setText(tr("摄像头启动中"));
	if (m_worker) {
		QMetaObject::invokeMethod(m_worker, "slotStartCamera", Qt::QueuedConnection);
	}
}

void CameraWindow::onCloseCamera()
{
	m_videoLabel->setText(tr("摄像头已关闭"));
	if (m_worker) {
		QMetaObject::invokeMethod(m_worker, "slotStopCamera", Qt::BlockingQueuedConnection);
	}
}

void CameraWindow::onConfigCamera()
{
	if (!m_worker) return;

	m_worker->slotRequestCameraDetail();
	connect(m_worker, &CameraWorker::signalCameraDetailReady, this, [=](const QString& info) {
		int w = m_worker->getWidth();
		int h = m_worker->getHeight();
		float fps = m_worker->getFps();

		CameraConfigDialog* dialog = new CameraConfigDialog(w, h, fps, info, this);
		connect(dialog, &CameraConfigDialog::configSelected, this, [=](int nw, int nh, int nfps) {
			m_worker->setConfig(nw, nh, nfps);
			QMetaObject::invokeMethod(m_worker, "slotStopCamera", Qt::BlockingQueuedConnection);
			QMetaObject::invokeMethod(m_worker, "slotStartCamera", Qt::QueuedConnection);
			});
		dialog->exec();
		});
}

void CameraWindow::onToggleYolo()
{
	m_yoloEnabled = !m_yoloEnabled;
	if (m_yoloEnabled) {
		m_worker->setYolo(m_yolo);
	}
	else {
		m_worker->setYolo(nullptr);
	}
	m_btnToggleYolo->setText(m_yoloEnabled ? tr("已开启识别") :  tr("已关闭识别"));
}

void CameraWindow::onFrameCaptured(const QImage& img)
{
	m_videoLabel->setPixmap(QPixmap::fromImage(img).scaled(m_videoLabel->size(), Qt::KeepAspectRatio));
}

void CameraWindow::onYoloFrameCaptured(const cv::Mat& frame)
{
	if (!m_yoloEnabled || m_yoloBusy) return;

	m_yoloBusy = true;
	QMetaObject::invokeMethod(m_yoloWork, "doYoloDetect",
		Qt::QueuedConnection,
		Q_ARG(cv::Mat, frame.clone()));
}

void CameraWindow::onYoloResult(const cv::Mat& result, const std::vector<YoloDetection>&)
{
	QImage img(result.data, result.cols, result.rows, result.step, QImage::Format_BGR888);
	emit m_worker->signalFrameReady(img.copy());
	m_yoloBusy = false;
}
