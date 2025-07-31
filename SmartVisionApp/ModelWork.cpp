#include "stdafx.h"
#include "ModelWork.h"
#include "ModelRunner.h"
#include "translate_baidu.h"
#include "YoloDetector.h"

ModelWork::ModelWork(ModelRunner* model) : m_model(model)
{
}

ModelWork::~ModelWork()
{
}

void ModelWork::doWork(const QString& filePath)
{
	if (m_model == nullptr || filePath.isEmpty()) {
		emit signalGetResult(false, {});
		return;
	}
	cv::Mat img = imreadWithChinese(filePath);
	if (img.empty()) {
		emit signalGetResult(false, {});
		return;
	}
	auto results = m_model->infer(img);
	emit signalGetResult(true, results);
}