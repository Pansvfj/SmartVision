#pragma once

#include <onnxruntime_cxx_api.h>  // ONNX Runtime C++ API������ģ������
#include <opencv2/opencv.hpp>     // OpenCV ͼ�����
#include <string>
#include <vector>

// ModelRunner����װͨ��ͼ�����ģ�ͣ��� MobileNet���ļ���������
class ModelRunner
{
public:
	// ���캯��������ģ��·���ͱ�ǩ�ļ�·��
	ModelRunner(const std::string& modelPath, const std::string& labelPath);

	// Ĭ������������ONNX ��Դ�Զ��ͷ�
	~ModelRunner() = default;

	// ͼ������ӿڣ����� OpenCV ͼ����������ǩ�����Ŷȶԣ�Top-K��
	std::vector<std::pair<std::string, float>> infer(const cv::Mat& image);

private:
	// ���ر�ǩ�ļ����� imagenet_classes.txt����ÿ��һ�����
	void loadLabels(const std::string& labelPath);

	// ONNX �������ͻỰ����
	Ort::Env m_env;                       // ������
	Ort::Session m_session;              // ����Ự
	Ort::SessionOptions m_sessionOptions;// �Ự������
	Ort::AllocatorWithDefaultOptions m_allocator; // Ĭ���ڴ������

	// ONNX ģ�͵�������������ַ�����ʽ�� const char* ��ʽ��
	std::vector<std::string> m_inputNamesStr;     // ��������string��
	std::vector<const char*> m_inputNamesCStr;    // ��������C�ַ�����
	std::vector<std::string> m_outputNamesStr;    // �������string��
	std::vector<const char*> m_outputNamesCStr;   // �������C�ַ�����

	// �����ǩ����
	std::vector<std::string> m_labels;
};
