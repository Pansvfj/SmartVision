#include "stdafx.h"
#include "YoloDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <QDebug>

// 构造函数：初始化 ONNX Session，并读取类别名称
YoloDetector::YoloDetector(const std::string& modelPath, const std::string& classPath)
	: env(ORT_LOGGING_LEVEL_WARNING, "YoloDetector"), sessionOptions()
{
	// 启用图优化
	sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

	// 设置多线程加速（根据 CPU 核心数可调）
	sessionOptions.SetIntraOpNumThreads(4);   // 每个算子最多使用 4 个线程
	sessionOptions.SetInterOpNumThreads(2);   // 同时运行两个算子

	// 加载模型文件
	std::wstring wModelPath(modelPath.begin(), modelPath.end());
	session = std::make_unique<Ort::Session>(env, wModelPath.c_str(), sessionOptions);

	// 加载类别名称
	std::ifstream file(classPath);
	std::string line;
	while (std::getline(file, line)) {
		classNames.push_back(line);
	}
}

// 图像预处理：resize、BGR→RGB、归一化、展平为 NCHW Tensor
void YoloDetector::preprocess(const cv::Mat& image, std::vector<float>& inputTensor)
{
	cv::Mat resized, rgb;

	// 记录缩放比例（用于后处理还原坐标）
	scaleX = static_cast<float>(inputWidth) / image.cols;
	scaleY = static_cast<float>(inputHeight) / image.rows;

	// 缩放输入图像
	cv::resize(image, resized, cv::Size(inputWidth, inputHeight));
	// OpenCV 默认是 BGR → 转成 RGB
	cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

	// 生成归一化后的 NCHW 格式张量
	inputTensor.resize(3 * inputHeight * inputWidth);
	int idx = 0;
	for (int c = 0; c < 3; ++c) {
		for (int y = 0; y < rgb.rows; ++y) {
			for (int x = 0; x < rgb.cols; ++x) {
				inputTensor[idx++] = rgb.at<cv::Vec3b>(y, x)[c] / 255.0f;
			}
		}
	}
}

// 推理主函数：调用 ONNX Session，获取 raw 输出，再进行后处理
std::vector<YoloDetection> YoloDetector::detect(const cv::Mat& image)
{
	std::vector<float> inputTensor;
	preprocess(image, inputTensor);

	// 创建输入张量
	Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	std::array<int64_t, 4> inputShape = { 1, 3, inputHeight, inputWidth };
	auto inputTensorOrt = Ort::Value::CreateTensor<float>(
		memInfo, inputTensor.data(), inputTensor.size(), inputShape.data(), inputShape.size());

	// YOLOv5 的默认输入输出名称
	static const char* inputNames[] = { "images" };
	static const char* outputNames[] = { "output0" };

	// 记录执行耗时
	std::chrono::time_point start = std::chrono::high_resolution_clock::now();
	auto outputTensor = session->Run(Ort::RunOptions{ nullptr }, inputNames, &inputTensorOrt, 1, outputNames, 1);
	std::chrono::time_point end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> elapsed = end - start;
	qDebug() << "session Run:" << elapsed.count() << " ms";

	// 读取输出张量数据
	float* output = outputTensor.front().GetTensorMutableData<float>();
	auto outputShape = outputTensor.front().GetTensorTypeAndShapeInfo().GetShape();

	std::vector<float> outputData(output, output + outputShape[1] * outputShape[2]);

	// 进入后处理阶段（解码 + NMS）
	return postprocess(image, outputData);
}

// 后处理函数：将 raw tensor 转为检测结果，执行 NMS，生成 YoloDetection 列表
std::vector<YoloDetection> YoloDetector::postprocess(const cv::Mat& image, std::vector<float>& output)
{
	std::vector<YoloDetection> detections;
	const int rows = image.rows;
	const int cols = image.cols;

	const size_t numClasses = classNames.size();
	const size_t numElementsPerBox = 5 + numClasses;  // cx, cy, w, h, obj_conf + class_confs
	const size_t numBoxes = output.size() / numElementsPerBox;

	std::vector<cv::Rect> boxes;
	std::vector<float> scores;
	std::vector<int> classIds;

	// 遍历每个预测框
	for (size_t i = 0; i < numBoxes; ++i) {
		float objConf = output[i * numElementsPerBox + 4];
		if (objConf < confThreshold) continue;

		// 获取每个类的得分，挑出最大类
		float* classScores = &output[i * numElementsPerBox + 5];
		int classId = std::max_element(classScores, classScores + numClasses) - classScores;
		float classConf = classScores[classId];

		float confidence = objConf * classConf;
		if (confidence < confThreshold) continue;

		// 中心点坐标 + 宽高
		float cx = output[i * numElementsPerBox + 0];
		float cy = output[i * numElementsPerBox + 1];
		float w = output[i * numElementsPerBox + 2];
		float h = output[i * numElementsPerBox + 3];

		// 将推理坐标还原到原图坐标
		int x = static_cast<int>((cx - w / 2.0f) / scaleX);
		int y = static_cast<int>((cy - h / 2.0f) / scaleY);
		int width = static_cast<int>(w / scaleX);
		int height = static_cast<int>(h / scaleY);

		// 边界保护，避免越界
		x = std::max(0, x);
		y = std::max(0, y);
		width = std::min(width, cols - x);
		height = std::min(height, rows - y);

		boxes.emplace_back(cv::Rect(x, y, width, height));
		scores.push_back(confidence);
		classIds.push_back(classId);
	}

	// 执行非极大值抑制（NMS）
	std::vector<int> indices;
	cv::dnn::NMSBoxes(boxes, scores, confThreshold, iouThreshold, indices);

	// 构造最终检测结果
	std::vector<YoloDetection> results;
	for (int idx : indices) {
		YoloDetection det;
		det.bbox = QRect(boxes[idx].x, boxes[idx].y, boxes[idx].width, boxes[idx].height);
		det.label = QString::fromStdString(classNames[classIds[idx]]);
		det.confidence = scores[idx];
		results.push_back(det);
	}

	return results;
}

// 绘制检测结果（红框 + 标签 + 置信度），用于可视化
void drawDetections(cv::Mat& image, const std::vector<YoloDetection>& detections)
{
	for (const auto& det : detections) {
		cv::Rect rect(det.bbox.x(), det.bbox.y(), det.bbox.width(), det.bbox.height());
		cv::rectangle(image, rect, cv::Scalar(0, 0, 255), 2);  // 红色边框

		std::string label = det.label.toStdString() + " " + std::to_string(int(det.confidence * 100)) + "%";

		// 设置文字字体、粗细、背景框大小
		double fontScale = 1.6;
		int thickness = 2;
		int padding = 6;
		int baseline = 0;

		cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);

		// 红色背景框
		cv::Point bg_tl(rect.x, rect.y - textSize.height - padding * 2);
		cv::Point bg_br(rect.x + textSize.width + padding * 2, rect.y);
		cv::rectangle(image, bg_tl, bg_br, cv::Scalar(0, 0, 255), cv::FILLED);

		// 白色文字
		cv::putText(image, label,
			cv::Point(rect.x + padding, rect.y - padding),
			cv::FONT_HERSHEY_SIMPLEX, fontScale,
			cv::Scalar(255, 255, 255), thickness);
	}
}
