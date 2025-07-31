#include "stdafx.h"
#include "ModelRunner.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

ModelRunner::ModelRunner(const std::string& modelPath, const std::string& labelPath)
	: m_env(ORT_LOGGING_LEVEL_WARNING, "SmartVision"),
	m_session(nullptr),
	m_sessionOptions()
{
	m_sessionOptions.SetIntraOpNumThreads(1);
	m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

	std::wstring wModelPath(modelPath.begin(), modelPath.end());
	if (!std::ifstream(wModelPath).good()) {
		throw std::runtime_error("模型文件不存在: " + modelPath);
	}

	try {
		m_session = Ort::Session(m_env, wModelPath.c_str(), m_sessionOptions);
	}
	catch (const Ort::Exception& e) {
		throw std::runtime_error("ONNX模型加载失败: " + std::string(e.what()));
	}

	// 提取输入名称
	size_t inputCount = m_session.GetInputCount();
	for (size_t i = 0; i < inputCount; ++i) {
		Ort::AllocatedStringPtr name = m_session.GetInputNameAllocated(i, m_allocator);
		m_inputNamesStr.push_back(name.get());
	}
	for (const auto& s : m_inputNamesStr) m_inputNamesCStr.push_back(s.c_str());

	// 提取输出名称
	size_t outputCount = m_session.GetOutputCount();
	for (size_t i = 0; i < outputCount; ++i) {
		Ort::AllocatedStringPtr name = m_session.GetOutputNameAllocated(i, m_allocator);
		m_outputNamesStr.push_back(name.get());
	}
	for (const auto& s : m_outputNamesStr) m_outputNamesCStr.push_back(s.c_str());

	loadLabels(labelPath);
}

void ModelRunner::loadLabels(const std::string& labelPath)
{
	std::ifstream file(labelPath);
	std::string line;
	while (std::getline(file, line)) {
		m_labels.push_back(line);
	}
}

std::vector<std::pair<std::string, float>> ModelRunner::infer(const cv::Mat& image)
{
	// 图像预处理
	cv::Mat resized;
	cv::resize(image, resized, cv::Size(224, 224));
	resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);

	cv::Mat channels[3];
	cv::split(resized, channels);

	const float mean[3] = { 0.485f, 0.456f, 0.406f };
	const float std[3] = { 0.229f, 0.224f, 0.225f };
	for (int i = 0; i < 3; ++i)
		channels[i] = (channels[i] - mean[i]) / std[i];
	cv::merge(channels, 3, resized);

	// NCHW 格式转换
	std::vector<float> inputTensorValues(3 * 224 * 224);
	size_t idx = 0;
	for (int c = 0; c < 3; ++c)
		for (int y = 0; y < 224; ++y)
			for (int x = 0; x < 224; ++x)
				inputTensorValues[idx++] = resized.at<cv::Vec3f>(y, x)[c];

	std::vector<int64_t> inputDims = { 1, 3, 224, 224 };
	Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memInfo,
		inputTensorValues.data(), inputTensorValues.size(),
		inputDims.data(), inputDims.size());

	Ort::RunOptions runOptions;
	auto outputTensors = m_session.Run(runOptions,
		m_inputNamesCStr.data(), &inputTensor, 1,
		m_outputNamesCStr.data(), 1);

	if (outputTensors.empty() || !outputTensors.front().IsTensor()) {
		throw std::runtime_error(("onnx is empty or type error"));
	}

	float* scores = outputTensors.front().GetTensorMutableData<float>();

	// softmax 归一化
	std::vector<float> prob(m_labels.size());
	float sum = 0.0f;
	for (size_t i = 0; i < m_labels.size(); ++i) {
		prob[i] = std::exp(scores[i]);
		sum += prob[i];
	}
	for (float& p : prob) p /= sum;

	// 获取 Top-5
	std::vector<std::pair<int, float>> sorted;
	for (int i = 0; i < static_cast<int>(prob.size()); ++i)
		sorted.emplace_back(i, prob[i]);

	std::partial_sort(sorted.begin(), sorted.begin() + 5, sorted.end(),
		[](const std::pair<int, float>& a, const std::pair<int, float>& b) {
			return a.second > b.second;
		});

	std::vector<std::pair<std::string, float>> top5;
	for (int i = 0; i < 5; ++i) {
		int idx = sorted[i].first;
		float conf = sorted[i].second;
		std::string label = (idx < m_labels.size()) ? m_labels[idx] : "Unknown";
		top5.emplace_back(label, conf);
	}

	return top5;
}
