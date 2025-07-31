#pragma once

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class ModelRunner
{
public:
	ModelRunner(const std::string& modelPath, const std::string& labelPath);
	~ModelRunner() = default;

	std::vector<std::pair<std::string, float>> infer(const cv::Mat& image);

private:
	void loadLabels(const std::string& labelPath);

	Ort::Env m_env;
	Ort::Session m_session;
	Ort::SessionOptions m_sessionOptions;
	Ort::AllocatorWithDefaultOptions m_allocator;

	std::vector<std::string> m_inputNamesStr;
	std::vector<const char*> m_inputNamesCStr;
	std::vector<std::string> m_outputNamesStr;
	std::vector<const char*> m_outputNamesCStr;

	std::vector<std::string> m_labels;
};
