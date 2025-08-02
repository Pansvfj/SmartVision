#include "stdafx.h"
#include "CameraConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

CameraConfigDialog::CameraConfigDialog(int width, int height, float fps, const QString& name, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("摄像头配置");
	resize(400, 200);

	m_labelName = new QLabel(this);
	m_labelResolution = new QLabel(this);
	m_labelFps = new QLabel(this);

	m_labelName->setText("设备名称: " + name);
	m_labelResolution->setText(QString("分辨率: %1x%2").arg(width).arg(height));
	m_labelFps->setText(QString("帧率: %1fps").arg(fps, 0, 'f', 1));

	m_comboConfigs = new QComboBox(this);
	m_comboConfigs->addItem("640x480 @30fps", QVariant::fromValue(QSize(640, 480)));
	m_comboConfigs->addItem("1280x720 @15fps", QVariant::fromValue(QSize(1280, 720)));
	m_comboConfigs->addItem("1920x1080 @10fps", QVariant::fromValue(QSize(1920, 1080)));

	QPushButton* btnApply = new QPushButton("应用配置", this);
	connect(btnApply, &QPushButton::clicked, this, &CameraConfigDialog::onApplyClicked);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addWidget(m_labelName);
	layout->addWidget(m_labelResolution);
	layout->addWidget(m_labelFps);
	layout->addWidget(m_comboConfigs);
	layout->addWidget(btnApply);
}

void CameraConfigDialog::onApplyClicked()
{
	int index = m_comboConfigs->currentIndex();
	QSize size = m_comboConfigs->itemData(index).toSize();
	int fps = 30;
	if (index == 1) fps = 15;
	else if (index == 2) fps = 10;

	emit configSelected(size.width(), size.height(), fps);
	accept();
}
