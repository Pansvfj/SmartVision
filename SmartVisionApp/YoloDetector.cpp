#include "stdafx.h"
#include "YoloDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <QDebug>
#include <QPainter>

// 构造函数：初始化 ONNX Session，并读取类别名称
YoloDetector::YoloDetector(const std::string& modelPath, const std::string& classPath)
	: env(ORT_LOGGING_LEVEL_WARNING, "YoloDetector"), sessionOptions()
{
	sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
	sessionOptions.SetIntraOpNumThreads(4);
	sessionOptions.SetInterOpNumThreads(2);

	std::wstring wModelPath(modelPath.begin(), modelPath.end());
	session = std::make_unique<Ort::Session>(env, wModelPath.c_str(), sessionOptions);

	// 读取类别
	std::ifstream file(classPath);
	for (std::string line; std::getline(file, line); )
		if (!line.empty()) classNames.push_back(line);

	// ☆ 关键：从模型读取 I/O 名和输入形状
	initIoFromModel();
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
	// 若模型是动态，按当前帧自适应一个 32 的倍数（并可设置上限）
	if (dynamicInput) {
		int longer = std::max(image.cols, image.rows);
		int stride = 32;
		int s = (longer + stride - 1) / stride * stride; // 向上取整到 32 的倍数
		s = std::min(std::max(s, 320), 1280);            // 可选：限制在 [320,1280]
		inputWidth = s;
		inputHeight = s;
	}

	// ↓↓↓ 下面就是你现有的流程（预处理 -> ORT.Run -> 后处理）
	std::vector<float> inputTensor;
	preprocess(image, inputTensor);

	Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
	std::array<int64_t, 4> shape = { 1,3,inputHeight,inputWidth };
	Ort::Value in = Ort::Value::CreateTensor<float>(mem, inputTensor.data(), inputTensor.size(),
		shape.data(), shape.size());

	auto outputs = session->Run(Ort::RunOptions{ nullptr },
		inNames.data(), &in, 1,
		outNames.data(), outNames.size());

	auto& out0 = outputs.front();
	auto  shp = out0.GetTensorTypeAndShapeInfo().GetShape(); // [1, N, 5+nc]
	size_t elems = 1; for (size_t i = 1; i < shp.size(); ++i) elems *= (size_t)shp[i];
	float* ptr = out0.GetTensorMutableData<float>();
	std::vector<float> outputData(ptr, ptr + elems);

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

// cv::Mat (BGR) <-> QImage (RGB) 转换
static QImage cvMatToQImage(const cv::Mat& bgr) {
	cv::Mat rgb; cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
	return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}
static cv::Mat qImageToCvMat(const QImage& img) {
	QImage rgb = img.convertToFormat(QImage::Format_RGB888);
	cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
		const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine());
	cv::Mat bgr; cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
	return bgr.clone();
}

// 绘制检测结果（红框 + 标签 + 置信度），用于可视化
void drawDetections(cv::Mat& image, const std::vector<YoloDetection>& detections)
{
#if 0 // 使用 Qt 绘图中文
	// 1) Mat -> QImage
	QImage qimg = cvMatToQImage(image);

	// 2) 用 QPainter 画框和中文
	QPainter p(&qimg);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::TextAntialiasing);

	// 选择支持中文的字体（Windows 下“微软雅黑”；跨平台可用 Noto Sans CJK）
	QFont font("Microsoft YaHei", 16, QFont::DemiBold);
	p.setFont(font);
	QFontMetrics fm(font);

	for (const auto& det : detections) {
		QRect r(det.bbox.x(), det.bbox.y(), det.bbox.width(), det.bbox.height());

		// 边框
		p.setPen(QPen(QColor(255, 0, 0), 2));
		p.setBrush(Qt::NoBrush);
		p.drawRect(r);

		// 标签 & 置信度（中文 OK）
		QString text = QString("%1 %2%%")
			.arg(det.label)
			.arg(qRound(det.confidence * 100.0));

		// 文字背景条（红色），避免整块遮挡
		QRect tb = fm.boundingRect(text).adjusted(-6, -4, 6, 4);
		tb.moveTopLeft(QPoint(r.x(), std::max(0, r.y() - tb.height())));
		p.fillRect(tb, QColor(255, 0, 0));

		// 白色文字
		p.setPen(Qt::white);
		p.drawText(tb.adjusted(6, 4, -6, -4), text);
	}
	p.end();

	// 3) QImage -> Mat
	image = qImageToCvMat(qimg);
#else
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
#endif
}

// YoloDetector.cpp
void YoloDetector::initIoFromModel()
{
	Ort::AllocatorWithDefaultOptions alloc;

	// 输入/输出名
	inNamesStr.clear(); outNamesStr.clear();
	inNames.clear();    outNames.clear();

	size_t ni = session->GetInputCount();
	for (size_t i = 0; i < ni; ++i) {
		Ort::AllocatedStringPtr n = session->GetInputNameAllocated(i, alloc);
		inNamesStr.emplace_back(n.get());
	}
	size_t no = session->GetOutputCount();
	for (size_t i = 0; i < no; ++i) {
		Ort::AllocatedStringPtr n = session->GetOutputNameAllocated(i, alloc);
		outNamesStr.emplace_back(n.get());
	}
	for (auto& s : inNamesStr)  inNames.push_back(s.c_str());
	for (auto& s : outNamesStr) outNames.push_back(s.c_str());

	// 输入形状（通常是 [1,3,H,W] 或 [-1,3,-1,-1]）
	auto ti = session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
	auto shp = ti.GetShape();

	dynamicInput = true; // 先假设是动态
	if (shp.size() == 4) {
		int64_t H = shp[2];
		int64_t W = shp[3];
		if (H > 0 && W > 0) {           // 静态尺寸
			inputHeight = (int)H;
			inputWidth = (int)W;
			dynamicInput = false;
		}
	}

	// 兜底（仍不确定时），默认 896
	if (inputWidth == 0 || inputHeight == 0) {
		inputWidth = 896;
		inputHeight = 896;
		dynamicInput = true;            // 允许后续按帧自适应
	}

#ifdef _DEBUG
	qDebug() << "Model IO:";
	for (size_t i = 0; i < inNamesStr.size(); ++i)  qDebug() << "  In" << i << inNamesStr[i].c_str();
	for (size_t i = 0; i < outNamesStr.size(); ++i) qDebug() << "  Out" << i << outNamesStr[i].c_str();
	qDebug() << "Input shape:" << (int)shp[0] << (int)shp[1]
		<< (shp.size() > 2 ? (int)shp[2] : -1) << (shp.size() > 3 ? (int)shp[3] : -1)
		<< " dynamic?" << (dynamicInput ? "yes" : "no")
		<< " using:" << inputWidth << "x" << inputHeight;
#endif
}

