#pragma once

#include <opencv2/opencv.hpp>       // OpenCV ͼ����
#include <onnxruntime_cxx_api.h>    // ONNX Runtime C++ API
#include <QRect>                    // Qt �еľ��ο�
#include <QString>                  // Qt �ַ�����

// YOLO ������ṹ���� stdafx.h �е� YoloDetection ���壺���� QRect bbox, QString label, float confidence

// YoloDetector �ࣺ���ڷ�װ YOLOv5 �����ܣ����� ONNXRuntime��
class YoloDetector {
public:
	// ���캯��������ģ���ļ���.onnx����������ƣ�.names��
	YoloDetector(const std::string& modelPath, const std::string& classPath);

	// ִ��Ŀ���⣬���ؼ��������
	std::vector<YoloDetection> detect(const cv::Mat& image);

private:
	// ONNXRuntime ����������
	Ort::Env env;                         // ��������������־�ȼ��������ģ�
	std::unique_ptr<Ort::Session> session;// �Ự����ִ��ʵ�ʵ�����
	Ort::SessionOptions sessionOptions;   // �Ự�������ã��߳�����ͼ�Ż��ȣ�

	std::vector<std::string> classNames;  // ��������б��� .names �ļ��ж�ȡ

	// ����������ز���
	int inputWidth = 640;   // ����ͼ���ȣ�YOLO ģ��Ҫ��
	int inputHeight = 640;  // ����ͼ��߶�
	float confThreshold = 0.25f;  // ���Ŷ���ֵ�����ڴ�ֵ��Ŀ�꽫������
	float iouThreshold = 0.45f;   // �Ǽ���ֵ���Ƶ� IOU ��ֵ

	// ͼ��Ԥ����resize����һ����ͨ������������������������
	void preprocess(const cv::Mat& image, std::vector<float>& inputTensor);

	// ��������������߽��������Ŷȡ�NMS ����
	std::vector<YoloDetection> postprocess(const cv::Mat& image, std::vector<float>& output);

	// ����ͼ���������ӣ���������ӳ�䣩
	float scaleX = 1.0f;
	float scaleY = 1.0f;
};

// ���ߺ�������ͼ���ϻ��Ƽ�������߿� + ��ǩ + ���Ŷȣ�
void drawDetections(cv::Mat& image, const std::vector<YoloDetection>& detections);
