#pragma once

#include <onnxruntime_cxx_api.h>  // ONNX Runtime C++ API，用于模型推理
#include <opencv2/opencv.hpp>     // OpenCV 图像处理库
#include <string>
#include <vector>

// ModelRunner：封装通用图像分类模型（如 MobileNet）的加载与推理
class ModelRunner
{
public:
	// 构造函数，传入模型路径和标签文件路径
	ModelRunner(const std::string& modelPath, const std::string& labelPath);

	// 默认析构函数，ONNX 资源自动释放
	~ModelRunner() = default;

	// 图像推理接口，输入 OpenCV 图像，输出分类标签与置信度对（Top-K）
	std::vector<std::pair<std::string, float>> infer(const cv::Mat& image);

	// === 新增：推理设备控制 ===
	void setUseGPU(bool on) { m_useGPU = on; }
	void setDeviceId(int id) { m_deviceId = id; }
	bool useGPU() const { return m_useGPU; }
	int  deviceId() const { return m_deviceId; }
	bool recreateSession(const std::string& modelPath);

private:
	bool createOrtSession(const std::string& modelPath);

private:
	// 加载标签文件（如 imagenet_classes.txt），每行一个类别
	void loadLabels(const std::string& labelPath);

	// ONNX 推理环境和会话对象
	Ort::Env m_env;                       // 推理环境
	Ort::Session m_session;              // 推理会话
	Ort::SessionOptions m_sessionOptions;// 会话配置项
	Ort::AllocatorWithDefaultOptions m_allocator; // 默认内存分配器

	// ONNX 模型的输入输出名（字符串形式和 const char* 形式）
	std::vector<std::string> m_inputNamesStr;     // 输入名（string）
	std::vector<const char*> m_inputNamesCStr;    // 输入名（C字符串）
	std::vector<std::string> m_outputNamesStr;    // 输出名（string）
	std::vector<const char*> m_outputNamesCStr;   // 输出名（C字符串）

	// 分类标签名称
	std::vector<std::string> m_labels;

	bool m_useGPU = true;
	int  m_deviceId = 0;
	std::string m_modelPath; // 记住路径便于热重建
};
