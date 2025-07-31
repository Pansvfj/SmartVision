#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <QRect>
#include <QString>

struct YoloDetection {
	QRect bbox;
	QString label;
	float confidence;
};

class YoloDetector {
public:
	YoloDetector(const std::string& modelPath, const std::string& classPath);
	std::vector<YoloDetection> detect(const cv::Mat& image);

private:
	Ort::Env env;
	std::unique_ptr<Ort::Session> session;
	Ort::SessionOptions sessionOptions;

	std::vector<std::string> classNames;

	int inputWidth = 640;
	int inputHeight = 640;
	float confThreshold = 0.25f;
	float iouThreshold = 0.45f;

	void preprocess(const cv::Mat& image, std::vector<float>& inputTensor);
	std::vector<YoloDetection> postprocess(const cv::Mat& image, std::vector<float>& output);

	float scaleX = 1.0f;
	float scaleY = 1.0f;

};

void drawDetections(cv::Mat& image, const std::vector<YoloDetection>& detections);