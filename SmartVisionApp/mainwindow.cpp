#include "stdafx.h"
#include <QSize>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QTextBrowser>
#include <QApplication>
#include <QRegularExpression>
#include <QDebug>
#include <QFile>

#include "mainwindow.h"
#include "ModelRunner.h"
#include "translate_baidu.h"
#include "YoloDetector.h"
#include "ModelWork.h"
#include "YoloWork.h"
#include "QDialogHint.h"
#include "CameraWindow.h"

MainWindow* g_mainWindow = nullptr;

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
{
	setMinimumSize(QSize(800, 600));
	setWindowTitle(tr("Smart Vision"));

	QWidget* centralWidget = new QWidget(this);
	setCentralWidget(centralWidget);
	QVBoxLayout* layout = new QVBoxLayout(centralWidget);
	layout->setMargin(0);
	layout->setSpacing(0);

	QPushButton* pbHead = new QPushButton(tr("Open"), this);
	QLineEdit* leHead = new QLineEdit(this);
	leHead->setEnabled(false);

	QPushButton* pbCamera = new QPushButton(tr("Camera"), this);
	m_cameraWindow = new CameraWindow;
	connect(pbCamera, &QPushButton::clicked, this, [=]() {
		if (m_cameraWindow->isVisible()) {
			m_cameraWindow->close();
		}
		else {
			m_cameraWindow->show();
		}
	});

	QHBoxLayout* hlHead = new QHBoxLayout();
	hlHead->addWidget(pbHead);
	hlHead->addWidget(leHead);
	hlHead->addWidget(pbCamera);

	// === Inference Toolbar (Provider + DeviceId + Apply) ===
	m_providerBox = new QComboBox(this);
	m_providerBox->addItem("CPU");
	m_providerBox->addItem("GPU");
	m_providerBox->setCurrentIndex(1); // 默认 GPU
	m_deviceSpin = new QSpinBox(this);
	m_deviceSpin->setRange(0, 15);
	m_deviceSpin->setValue(0);
	m_applyInferBtn = new QPushButton(tr("Apply"), this);

	// 摆到头部栏
	hlHead->addSpacing(12);
	hlHead->addWidget(new QLabel(tr("Provider:"), this));
	hlHead->addWidget(m_providerBox);
	hlHead->addWidget(new QLabel(tr("DeviceId:"), this));
	hlHead->addWidget(m_deviceSpin);
	hlHead->addWidget(m_applyInferBtn);

	// 连接
	connect(m_applyInferBtn, &QPushButton::clicked, this, [=]() {
		m_useGPU = (m_providerBox->currentIndex() == 1);
		m_deviceId = m_deviceSpin->value();

		// === 应用到三个推理对象 ===
		if (m_model) {
			m_model->setUseGPU(m_useGPU);
			m_model->setDeviceId(m_deviceId);
			m_model->recreateSession("D:/Projects/SmartVision/model/mobilenetv2-7.onnx");
		}
		if (m_generalYoloDetector) {
			m_generalYoloDetector->setUseGPU(m_useGPU);
			m_generalYoloDetector->setDeviceId(m_deviceId);
			m_generalYoloDetector->recreateSession("D:/Projects/SmartVision/model/yolov5s.onnx");
		}
		if (m_fishYoloDetector) {
			m_fishYoloDetector->setUseGPU(m_useGPU);
			m_fishYoloDetector->setDeviceId(m_deviceId);
			m_fishYoloDetector->recreateSession("D:/Projects/SmartVision/model/fish.onnx");
		}

		// 友好提示
		QDialogHint::create(tr("Switched to %1 (device %2)")
			.arg(m_useGPU ? "GPU" : "CPU").arg(m_deviceId), 1600, this);
		});


	m_lbImage = new QLabel(this);
	m_lbImage->setAlignment(Qt::AlignCenter);
	m_lbImage->setMinimumHeight(400);

	m_lbYoloImage = new QLabel(this);
	m_lbYoloImage->setAlignment(Qt::AlignCenter);
	m_lbYoloImage->setMinimumHeight(400);

	m_txtBrowser = new QTextBrowser(this);
	m_txtBrowser->setFixedHeight(120);

	m_pbYolo = new QPushButton(tr("yolov5s"), this);
	m_txtBrowserYolo = new QTextBrowser(this);
	m_txtBrowserYolo->setFixedHeight(120);

	m_pbYoloFish = new QPushButton(tr("鱼类检测"), this);
	m_txtBrowserFish = new QTextBrowser(this);
	m_txtBrowserFish->setFixedHeight(120);

	QVBoxLayout* vlayoutYolo = new QVBoxLayout();
	vlayoutYolo->addWidget(m_pbYolo);
	vlayoutYolo->addWidget(m_txtBrowserYolo);

	QVBoxLayout* vlayoutFish = new QVBoxLayout();
	vlayoutFish->addWidget(m_pbYoloFish);
	vlayoutFish->addWidget(m_txtBrowserFish);

	QHBoxLayout* hlayoutText = new QHBoxLayout();
	hlayoutText->addWidget(m_txtBrowser);
	hlayoutText->addLayout(vlayoutYolo);
	hlayoutText->addLayout(vlayoutFish);

	m_model = new ModelRunner("D:/Projects/SmartVision/model/mobilenetv2-7.onnx",
		"D:/Projects/SmartVision/model/imagenet_classes.txt");

	m_generalYoloDetector = new YoloDetector("D:/Projects/SmartVision/model/yolov5s.onnx",
		"D:/Projects/SmartVision/model/coco.names");

	m_fishYoloDetector = new YoloDetector("D:/Projects/SmartVision/model/fish.onnx",
		"D:/Projects/SmartVision/model/fish.names");

	// 翻译
	m_translator = new TranslateTask(qApp->applicationDirPath() + "/translate_cache.txt");
	connect(m_translator, &TranslateTask::signalTranslationFinished,
		this, &MainWindow::onTranslationFinished, Qt::QueuedConnection);
	m_textToSpeech = new QTextToSpeech(this);
	m_textToSpeech->setRate(0);
	m_textToSpeech->setPitch(1);
	m_textToSpeech->setVolume(1.0);

	// 初始化模型推理线程
	m_modelWork = new ModelWork(nullptr, m_model);
	connect(m_modelWork, &ModelWork::signalGetResult, this, &MainWindow::onModelResultReceived);
	connect(this, &MainWindow::signalStartModelWork, m_modelWork, &ModelWork::doWork);
	m_modelWork->moveToThread(&m_modelThread);
	m_modelThread.start();

	// 初始化 YOLO 检测线程
	m_generalYoloWork = new YoloWork(nullptr /*must! 要不都会在主线程执行*/, m_generalYoloDetector);
	connect(m_generalYoloWork, &YoloWork::signalGetResult, this, &MainWindow::onYoloResultReceived
		, Qt::QueuedConnection);
	connect(this, &MainWindow::signalStartGeneralWork, m_generalYoloWork, &YoloWork::doWork
		, Qt::QueuedConnection);
	m_generalYoloWork->moveToThread(&m_generalYoloThread);
	m_generalYoloThread.start();

	// 初始化鱼类 YOLO 检测线程
	m_fishYoloWork = new YoloWork(nullptr, m_fishYoloDetector);
	connect(m_fishYoloWork, &YoloWork::signalGetResult, this, &MainWindow::onYoloResultReceived
		, Qt::QueuedConnection);
	connect(this, &MainWindow::signalStartFishYoloWork, m_fishYoloWork, &YoloWork::doWork
		, Qt::QueuedConnection);
	m_fishYoloWork->moveToThread(&m_fishYoloThread);
	m_fishYoloThread.start();

	// 打开图片按钮逻辑
	connect(pbHead, &QPushButton::clicked, this, [=]() {
		m_txtBrowser->setText(tr("Scanning ..."));
		m_filePath = QFileDialog::getOpenFileName(this, tr("Select Image"), "D:/Projects/SmartVision/images", "Images (*.png *.jpg *.jpeg)");
		if (!m_filePath.isEmpty()) {			
			leHead->setText(m_filePath);
			m_lbImage->setText(tr("正在处理图片"));
			emit signalStartModelWork(m_filePath);
		}
		else {
			m_txtBrowserYolo->clear();
			m_txtBrowserFish->clear();
			m_txtBrowser->clear();

			m_lbImage->setPixmap(QPixmap());
			m_lbYoloImage->setPixmap(QPixmap());
			leHead->setText("");
		}
	});

	// YOLOv5 检测按钮
	connect(m_pbYolo, &QPushButton::clicked, this, [=]() {
		if (m_filePath.isEmpty()) {
			QDialogHint::create(tr("文件名为空"), 2000, this);
			return;
		}
		m_txtBrowserYolo->setText(tr("Scanning ..."));
		emit signalStartGeneralWork(m_filePath);
	});

	// 鱼类 YOLO 检测按钮
	connect(m_pbYoloFish, &QPushButton::clicked, this, [=]() {
		if (m_filePath.isEmpty()) {
			QDialogHint::create(tr("文件名为空"), 2000, this);
			return;
		}
		m_txtBrowserFish->setText(tr("Scanning ..."));

		emit signalStartFishYoloWork(m_filePath);
	});

	QHBoxLayout* imgHLayout = new QHBoxLayout();
	imgHLayout->addWidget(m_lbImage);
	imgHLayout->addSpacing(4);
	imgHLayout->addWidget(m_lbYoloImage);

	layout->addLayout(hlHead);
	layout->addLayout(imgHLayout);
	layout->addLayout(hlayoutText);

	g_mainWindow = this;

	for (auto& p : Ort::GetAvailableProviders())
		qDebug() << "[ORT] provider:" << p.c_str();
}

MainWindow::~MainWindow() {

	if (m_modelThread.isRunning()) {
		m_modelThread.quit();
		m_modelThread.wait();
	}
	delete m_model;

	if (m_generalYoloThread.isRunning()) {
		m_generalYoloThread.quit();
		m_generalYoloThread.wait();
	}
	delete m_generalYoloDetector;

	if (m_fishYoloThread.isRunning()) {
		m_fishYoloThread.quit();
		m_fishYoloThread.wait();
	}
	delete m_fishYoloDetector;

	if (m_translator->isRunning()) {
		m_translator->quit();
		m_translator->wait();
	}
	m_translator->deleteLater();

	if (m_cameraWindow) {
		delete m_cameraWindow;           // 调用析构，释放线程资源
		m_cameraWindow = nullptr;
	}
}
MainWindow* MainWindow::getMainWindow()
{
	return g_mainWindow;
}

void MainWindow::resizeEvent(QResizeEvent* event) {
	QMainWindow::resizeEvent(event);
	if (!m_pixmap.isNull())
		m_lbImage->setPixmap(m_pixmap.scaled(m_lbImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	if (!m_YoloPixmap.isNull())
		m_lbYoloImage->setPixmap(m_YoloPixmap.scaled(m_lbYoloImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::closeEvent(QCloseEvent* e)
{
	QMainWindow::closeEvent(e);
	if (m_cameraWindow)
		m_cameraWindow->close();
}

void MainWindow::playText(const QString& text) {
	if (m_textToSpeech)
		m_textToSpeech->say(text);
}

void MainWindow::onModelResultReceived(bool success, const GetModelResultType& result, const QImage& resImg)
{
	m_txtBrowserYolo->clear();
	m_txtBrowserFish->clear();
	m_txtBrowser->clear();
	m_lbImage->setText("");

	m_pixmap = QPixmap::fromImage(resImg);
	m_lbImage->setPixmap(m_pixmap.scaled(m_lbImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

	if (!success) {
		QString errStr = tr("模型推理失败");
		qDebug() << errStr;
		QDialogHint::create(errStr, 2000, this);
		return;
	}
	
	QStringList toTranslateStrList;
	std::vector<float> scores;
	for (const auto& [label, score] : result) {
		QString pureStr = QString::fromStdString(label).replace(QRegularExpression(R"([\p{P}\p{S}])"), "");
		toTranslateStrList.append(pureStr);
		scores.push_back(score);
	}
	m_translator->translate(toTranslateStrList, scores);
	m_translator->start();
}

void MainWindow::onYoloResultReceived(bool success, const QPair<QImage, QStringList>& result)
{
	m_YoloPixmap = QPixmap::fromImage(result.first);
	m_lbYoloImage->setPixmap(m_YoloPixmap.scaled(m_lbYoloImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

	QString spoken = result.second.isEmpty() ?
		tr("No target detected") :
		tr("Detected: ") + result.second.join(", ");

	if (sender() == m_generalYoloWork) {
		m_txtBrowserYolo->clear();
		m_txtBrowserYolo->append(spoken);
		
	} else if (sender() == m_fishYoloWork) {
		m_txtBrowserFish->clear();
		m_txtBrowserFish->append(spoken);
	}
	playText(spoken);
}

void MainWindow::onTranslationFinished(const QString& result)
{
	m_txtBrowser->append(result);
	playText(m_txtBrowser->toPlainText());
}