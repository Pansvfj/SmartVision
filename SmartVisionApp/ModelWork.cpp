#include "stdafx.h"
#include "ModelWork.h"
#include "ModelRunner.h"
#include "translate_baidu.h"
#include "YoloDetector.h"

ModelWork::ModelWork(QObject* parent, ModelRunner* model) :
	QObject(parent), m_model(model)
{
}

ModelWork::~ModelWork()
{
}

void ModelWork::doWork(const QString& filePath)
{
	QImage resImg(filePath);
	if (m_model == nullptr || filePath.isEmpty()) {
		emit signalGetResult(false, {}, resImg);
		return;
	}
	cv::Mat img = imreadWithChinese(filePath);
	if (img.empty()) {
		emit signalGetResult(false, {}, resImg);
		return;
	}
	auto results = m_model->infer(img);
	emit signalGetResult(true, results, resImg);
}