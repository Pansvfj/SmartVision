#pragma once

class ModelRunner;

class ModelWork : public QObject
{
	Q_OBJECT
public:
	explicit ModelWork(QObject* parent, ModelRunner* m_model);
	~ModelWork();

signals:
	void signalGetResult(bool success, const GetModelResultType& result);

public slots:
	void doWork(const QString& filePath);

private:
	ModelRunner* m_model = nullptr;
};