#pragma once

class YoloDetector;

class YoloWork : public QObject
{
	Q_OBJECT
public:
	YoloWork(QObject* parent, YoloDetector* m_detector);
	~YoloWork();

signals:
	void signalGetResult(bool success, const QPair<QImage, QStringList>& result);

public slots:
	void doWork(const QString& filePath);

private:
	YoloDetector* m_detector = nullptr;
};