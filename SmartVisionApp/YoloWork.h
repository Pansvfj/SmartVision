#pragma once

#include <QObject>
#include <opencv2/opencv.hpp>
#include "YoloDetector.h"

class YoloWork : public QObject
{
	Q_OBJECT
public:
	explicit YoloWork(QObject* parent = nullptr, YoloDetector* detector = nullptr);
	~YoloWork();

signals:
	void signalGetResult(bool success, const QPair<QImage, QStringList>& result);

public slots:
	void doWork(const QString& filePath);

private:
	YoloDetector* m_detector = nullptr;
};
