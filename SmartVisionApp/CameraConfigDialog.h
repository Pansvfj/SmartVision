#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

class CameraConfigDialog : public QDialog {
	Q_OBJECT
public:
	CameraConfigDialog(int width, int height, float fps, const QString& name, QWidget* parent = nullptr);

signals:
	void configSelected(int width, int height, int fps);

private slots:
	void onApplyClicked();

private:
	QLabel* m_labelName;
	QLabel* m_labelResolution;
	QLabel* m_labelFps;
	QComboBox* m_comboConfigs;
};
