#include "stdafx.h"
#include "ModelRunner.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

// 构造函数：加载 ONNX 模型并读取标签文件
// ==== 构造函数：支持 GPU/CPU 切换，失败自动回退到 CPU ====
ModelRunner::ModelRunner(const std::string& modelPath, const std::string& labelPath)
	: m_env(ORT_LOGGING_LEVEL_WARNING, "SmartVision"),
	m_session(nullptr),
	m_sessionOptions(),
	m_allocator()
{
	// 1) ORT 基本优化/线程
	m_sessionOptions.SetIntraOpNumThreads(1);
	m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

	// 2) 记录模型路径，尝试按当前配置创建会话（优先 GPU）
	m_modelPath = modelPath;

	// （可选）存在性检查，保持与你原来的实现一致
	{
		std::wstring wModelPath(modelPath.begin(), modelPath.end());
		if (!std::ifstream(wModelPath).good()) {
			throw std::runtime_error("模型文件不存在: " + modelPath);
		}
	}
	if (!createOrtSession(modelPath)) {
		// 若 GPU 创建失败（CUDA 依赖或 Provider 不可用），回退到 CPU
		m_useGPU = false;
		if (!createOrtSession(modelPath)) {
			throw std::runtime_error("ONNX model loading failed (CPU/GPU unavailable)");
		}
	}

	// 3) 获取输入节点名称（保持你原来的做法）
	{
		size_t inputCount = m_session.GetInputCount();
		for (size_t i = 0; i < inputCount; ++i) {
			Ort::AllocatedStringPtr name = m_session.GetInputNameAllocated(i, m_allocator);
			m_inputNamesStr.push_back(name.get());
		}
		for (const auto& s : m_inputNamesStr)
			m_inputNamesCStr.push_back(s.c_str());
	}

	// 4) 获取输出节点名称（保持你原来的做法）
	{
		size_t outputCount = m_session.GetOutputCount();
		for (size_t i = 0; i < outputCount; ++i) {
			Ort::AllocatedStringPtr name = m_session.GetOutputNameAllocated(i, m_allocator);
			m_outputNamesStr.push_back(name.get());
		}
		for (const auto& s : m_outputNamesStr)
			m_outputNamesCStr.push_back(s.c_str());
	}

	// 5) 加载标签文件（保持你原来的做法）
	loadLabels(labelPath);
}


// 加载标签文件，每一行对应一个类别名称
void ModelRunner::loadLabels(const std::string& labelPath)
{
	std::ifstream file(labelPath);
	std::string line;
	while (std::getline(file, line)) {
		m_labels.push_back(line);
	}
}

// 模型推理：输入一张图像，返回 top-5 的分类结果及置信度
std::vector<std::pair<std::string, float>> ModelRunner::infer(const cv::Mat& image)
{
	// 预处理：缩放到 224x224，归一化到 [0,1]
	cv::Mat resized;
	cv::resize(image, resized, cv::Size(224, 224));
	resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);  // 转 float32 并归一化

	// 通道分离并归一化（标准化）
	cv::Mat channels[3];
	cv::split(resized, channels);

	const float mean[3] = { 0.485f, 0.456f, 0.406f };
	const float std[3] = { 0.229f, 0.224f, 0.225f };
	for (int i = 0; i < 3; ++i)
		channels[i] = (channels[i] - mean[i]) / std[i];  // 标准化
	cv::merge(channels, 3, resized);  // 重新合并

	// 转为 NCHW 格式（Tensor格式）
	std::vector<float> inputTensorValues(3 * 224 * 224);
	size_t idx = 0;
	for (int c = 0; c < 3; ++c)
		for (int y = 0; y < 224; ++y)
			for (int x = 0; x < 224; ++x)
				inputTensorValues[idx++] = resized.at<cv::Vec3f>(y, x)[c];

	// 创建输入张量
	std::vector<int64_t> inputDims = { 1, 3, 224, 224 };
	Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
		memInfo, inputTensorValues.data(), inputTensorValues.size(),
		inputDims.data(), inputDims.size());

	// 执行推理
	Ort::RunOptions runOptions;
	auto outputTensors = m_session.Run(
		runOptions,
		m_inputNamesCStr.data(), &inputTensor, 1,
		m_outputNamesCStr.data(), 1);

	// 检查输出合法性
	if (outputTensors.empty() || !outputTensors.front().IsTensor()) {
		throw std::runtime_error("onnx is empty or type error");
	}

	// 获取输出分数指针
	float* scores = outputTensors.front().GetTensorMutableData<float>();

	// softmax归一化处理
	std::vector<float> prob(m_labels.size());
	float sum = 0.0f;
	for (size_t i = 0; i < m_labels.size(); ++i) {
		prob[i] = std::exp(scores[i]);
		sum += prob[i];
	}
	for (float& p : prob)
		p /= sum;

	// 排序并选取 top-5
	std::vector<std::pair<int, float>> sorted;
	for (int i = 0; i < static_cast<int>(prob.size()); ++i)
		sorted.emplace_back(i, prob[i]);

	std::partial_sort(sorted.begin(), sorted.begin() + 5, sorted.end(),
		[](const std::pair<int, float>& a, const std::pair<int, float>& b) {
			return a.second > b.second;
		});

	// 构造最终输出：标签 + 置信度
	std::vector<std::pair<std::string, float>> top5;
	for (int i = 0; i < 5; ++i) {
		int idx = sorted[i].first;
		float conf = sorted[i].second;
		std::string label = (idx < m_labels.size()) ? m_labels[idx] : "Unknown";
		top5.emplace_back(label, conf);
	}

	return top5;
}

// 同样不要拷贝成员 m_sessionOptions，直接重建一份本地 so
bool ModelRunner::createOrtSession(const std::string& modelPath) {
	try {
		Ort::SessionOptions so;
		so.SetIntraOpNumThreads(1);
		so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

		if (m_useGPU) {
			qDebug() << "[ModelRunner] Prefer accel, device =" << m_deviceId;
			AppendWithFallback(so, m_deviceId);   // <<<<<< 这里用 m_deviceId
		}
		else {
			qDebug() << "[ModelRunner] Force CPU";
		}

		std::wstring wModelPath(modelPath.begin(), modelPath.end());
		m_session = Ort::Session(m_env, wModelPath.c_str(), so);
		return true;
	}
	catch (const Ort::Exception& e) {
		qDebug() << "[ModelRunner] createOrtSession failed:" << e.what();
		return false;
	}
}

bool ModelRunner::recreateSession(const std::string& modelPath) {
	return createOrtSession(modelPath);
}

