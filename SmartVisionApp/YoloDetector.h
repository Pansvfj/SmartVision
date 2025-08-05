#pragma once

#include <opencv2/opencv.hpp>       // OpenCV 图像处理
#include <onnxruntime_cxx_api.h>    // ONNX Runtime C++ API
#include <QRect>                    // Qt 中的矩形框
#include <QString>                  // Qt 字符串类

// YOLO 检测结果结构体由 stdafx.h 中的 YoloDetection 定义：包含 QRect bbox, QString label, float confidence

// YoloDetector 类：用于封装 YOLOv5 推理功能（基于 ONNXRuntime）
class YoloDetector {
public:
	// 构造函数：加载模型文件（.onnx）和类别名称（.names）
	YoloDetector(const std::string& modelPath, const std::string& classPath);

	// 执行目标检测，返回检测结果数组
	std::vector<YoloDetection> detect(const cv::Mat& image);

private:
	// ONNXRuntime 推理核心组件
	Ort::Env env;                         // 推理环境（负责日志等级、上下文）
	std::unique_ptr<Ort::Session> session;// 会话对象，执行实际的推理
	Ort::SessionOptions sessionOptions;   // 会话参数配置（线程数、图优化等）

	std::vector<std::string> classNames;  // 类别名称列表，从 .names 文件中读取

	// 推理输入相关参数
	int inputWidth = 640;   // 输入图像宽度（YOLO 模型要求）
	int inputHeight = 640;  // 输入图像高度
	float confThreshold = 0.25f;  // 置信度阈值，低于此值的目标将被丢弃
	float iouThreshold = 0.45f;   // 非极大值抑制的 IOU 阈值

	// 图像预处理：resize、归一化、通道调整，生成输入张量数据
	void preprocess(const cv::Mat& image, std::vector<float>& inputTensor);

	// 推理结果后处理：解码边界框、类别置信度、NMS 抑制
	std::vector<YoloDetection> postprocess(const cv::Mat& image, std::vector<float>& output);

	// 输入图像缩放因子（用于坐标映射）
	float scaleX = 1.0f;
	float scaleY = 1.0f;
};

// 工具函数：在图像上绘制检测结果（边框 + 标签 + 置信度）
void drawDetections(cv::Mat& image, const std::vector<YoloDetection>& detections);
