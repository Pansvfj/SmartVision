#include "stdafx.h"
#include "YoloDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <QDebug>

// ���캯������ʼ�� ONNX Session������ȡ�������
YoloDetector::YoloDetector(const std::string& modelPath, const std::string& classPath)
	: env(ORT_LOGGING_LEVEL_WARNING, "YoloDetector"), sessionOptions()
{
	// ����ͼ�Ż�
	sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

	// ���ö��̼߳��٣����� CPU �������ɵ���
	sessionOptions.SetIntraOpNumThreads(4);   // ÿ���������ʹ�� 4 ���߳�
	sessionOptions.SetInterOpNumThreads(2);   // ͬʱ������������

	// ����ģ���ļ�
	std::wstring wModelPath(modelPath.begin(), modelPath.end());
	session = std::make_unique<Ort::Session>(env, wModelPath.c_str(), sessionOptions);

	// �����������
	std::ifstream file(classPath);
	std::string line;
	while (std::getline(file, line)) {
		classNames.push_back(line);
	}
}

// ͼ��Ԥ����resize��BGR��RGB����һ����չƽΪ NCHW Tensor
void YoloDetector::preprocess(const cv::Mat& image, std::vector<float>& inputTensor)
{
	cv::Mat resized, rgb;

	// ��¼���ű��������ں���ԭ���꣩
	scaleX = static_cast<float>(inputWidth) / image.cols;
	scaleY = static_cast<float>(inputHeight) / image.rows;

	// ��������ͼ��
	cv::resize(image, resized, cv::Size(inputWidth, inputHeight));
	// OpenCV Ĭ���� BGR �� ת�� RGB
	cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

	// ���ɹ�һ����� NCHW ��ʽ����
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

// ���������������� ONNX Session����ȡ raw ������ٽ��к���
std::vector<YoloDetection> YoloDetector::detect(const cv::Mat& image)
{
	std::vector<float> inputTensor;
	preprocess(image, inputTensor);

	// ������������
	Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	std::array<int64_t, 4> inputShape = { 1, 3, inputHeight, inputWidth };
	auto inputTensorOrt = Ort::Value::CreateTensor<float>(
		memInfo, inputTensor.data(), inputTensor.size(), inputShape.data(), inputShape.size());

	// YOLOv5 ��Ĭ�������������
	static const char* inputNames[] = { "images" };
	static const char* outputNames[] = { "output0" };

	// ��¼ִ�к�ʱ
	std::chrono::time_point start = std::chrono::high_resolution_clock::now();
	auto outputTensor = session->Run(Ort::RunOptions{ nullptr }, inputNames, &inputTensorOrt, 1, outputNames, 1);
	std::chrono::time_point end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> elapsed = end - start;
	qDebug() << "session Run:" << elapsed.count() << " ms";

	// ��ȡ�����������
	float* output = outputTensor.front().GetTensorMutableData<float>();
	auto outputShape = outputTensor.front().GetTensorTypeAndShapeInfo().GetShape();

	std::vector<float> outputData(output, output + outputShape[1] * outputShape[2]);

	// �������׶Σ����� + NMS��
	return postprocess(image, outputData);
}

// ���������� raw tensor תΪ�������ִ�� NMS������ YoloDetection �б�
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

	// ����ÿ��Ԥ���
	for (size_t i = 0; i < numBoxes; ++i) {
		float objConf = output[i * numElementsPerBox + 4];
		if (objConf < confThreshold) continue;

		// ��ȡÿ����ĵ÷֣����������
		float* classScores = &output[i * numElementsPerBox + 5];
		int classId = std::max_element(classScores, classScores + numClasses) - classScores;
		float classConf = classScores[classId];

		float confidence = objConf * classConf;
		if (confidence < confThreshold) continue;

		// ���ĵ����� + ���
		float cx = output[i * numElementsPerBox + 0];
		float cy = output[i * numElementsPerBox + 1];
		float w = output[i * numElementsPerBox + 2];
		float h = output[i * numElementsPerBox + 3];

		// ���������껹ԭ��ԭͼ����
		int x = static_cast<int>((cx - w / 2.0f) / scaleX);
		int y = static_cast<int>((cy - h / 2.0f) / scaleY);
		int width = static_cast<int>(w / scaleX);
		int height = static_cast<int>(h / scaleY);

		// �߽籣��������Խ��
		x = std::max(0, x);
		y = std::max(0, y);
		width = std::min(width, cols - x);
		height = std::min(height, rows - y);

		boxes.emplace_back(cv::Rect(x, y, width, height));
		scores.push_back(confidence);
		classIds.push_back(classId);
	}

	// ִ�зǼ���ֵ���ƣ�NMS��
	std::vector<int> indices;
	cv::dnn::NMSBoxes(boxes, scores, confThreshold, iouThreshold, indices);

	// �������ռ����
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

// ���Ƽ��������� + ��ǩ + ���Ŷȣ������ڿ��ӻ�
void drawDetections(cv::Mat& image, const std::vector<YoloDetection>& detections)
{
	for (const auto& det : detections) {
		cv::Rect rect(det.bbox.x(), det.bbox.y(), det.bbox.width(), det.bbox.height());
		cv::rectangle(image, rect, cv::Scalar(0, 0, 255), 2);  // ��ɫ�߿�

		std::string label = det.label.toStdString() + " " + std::to_string(int(det.confidence * 100)) + "%";

		// �����������塢��ϸ���������С
		double fontScale = 1.6;
		int thickness = 2;
		int padding = 6;
		int baseline = 0;

		cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);

		// ��ɫ������
		cv::Point bg_tl(rect.x, rect.y - textSize.height - padding * 2);
		cv::Point bg_br(rect.x + textSize.width + padding * 2, rect.y);
		cv::rectangle(image, bg_tl, bg_br, cv::Scalar(0, 0, 255), cv::FILLED);

		// ��ɫ����
		cv::putText(image, label,
			cv::Point(rect.x + padding, rect.y - padding),
			cv::FONT_HERSHEY_SIMPLEX, fontScale,
			cv::Scalar(255, 255, 255), thickness);
	}
}
