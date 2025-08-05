# 中文介绍
# 🧠 SmartVision 万物识别系统

基于 C++ + Qt + OpenCV + ONNX Runtime 开发的智能图像识别平台，支持本地图像分类、YOLO 实时目标检测、摄像头采集识别、多线程异步处理、语音播报和翻译结果展示。

---

## 🚀 项目亮点

- ✅ **图像分类**：支持 MobileNetV2 模型，Top-5 分类输出并实时翻译
- ✅ **目标检测**：内置 YOLOv5 + 自训练鱼类检测模型，支持多目标框选
- ✅ **实时摄像头识别**：支持摄像头采集与识别切换（静态模式 / YOLO 模式）
- ✅ **多线程异步**：模型推理在独立线程执行，界面不卡顿
- ✅ **语音播报**：识别结果可自动朗读
- ✅ **中英文混合支持**：标签中文翻译（百度翻译），结果更直观

---

## 📂 工程结构

```bash
SmartVision/
├── main.cpp                    # 程序入口
├── mainwindow.{h,cpp}         # 主窗口界面与控制逻辑
├── ModelRunner.{h,cpp}        # 图像分类模型加载与推理
├── YoloDetector.{h,cpp}       # YOLO 模型加载与推理逻辑
├── ModelWork.{h,cpp}          # 图像分类异步工作线程
├── YoloWork.{h,cpp}           # 静态图 YOLO 推理线程
├── YoloStreamWork.{h,cpp}     # 实时摄像头 YOLO 推理线程
├── CameraWindow.{h,cpp}       # 摄像头识别界面与逻辑
├── CameraWorker.{h,cpp}       # 摄像头采集线程
├── CameraConfigDialog.{h,cpp} # 摄像头参数配置窗口
├── translate_baidu.h          # 百度翻译线程支持
├── stdafx.h                   # 公共头文件与类型定义
├── model/                     # 存放模型文件
│   ├── mobilenetv2-7.onnx
│   ├── imagenet_classes.txt
│   ├── yolov5s.onnx
│   ├── coco.names
│   ├── fish.onnx
│   └── fish.names
└── images/                    # 示例图像
```

---

## 🛠️ 环境要求

- **操作系统**：Windows 10/11 x64
- **开发工具**：Visual Studio 2019/2022 + Qt VS Tools
- **Qt版本**：Qt 5.15.x (建议 MSVC x64)
- **ONNX Runtime**：1.15+
- **OpenCV**：建议 4.5+，含 `opencv_world` 模块
- **百度翻译API（可选）**：用于英文标签中文翻译（带缓存）

---

## ⚙️ 编译配置说明

1. 使用 Qt VS Tools 打开 `.sln` 工程。
2. 项目使用 C++17，请确保启用：
   - C/C++ → 语言 → C++语言标准：`ISO C++17`
3. 配置环境变量或项目属性：
   - 添加 ONNX Runtime 依赖库（DLL 和 Lib）
   - 添加 OpenCV 头文件与库路径
4. 编译并运行 `SmartVisionApp`。

---

## 🖼️ 使用说明

### ✅ 图像识别
1. 点击 “Open” 按钮选择图片。
2. 自动进行分类推理，并显示 Top-5 标签及中文翻译。

### ✅ YOLO 检测
- 点击按钮：
  - `yolov5s`：使用通用 coco 模型进行目标检测
  - `鱼类检测`：使用自训练 YOLO 模型识别鱼类

### ✅ 摄像头识别
- 点击 `Camera` 打开实时识别窗口
- 支持分辨率切换 / 识别开关（可选 YOLO）

---

## 🔁 待拓展

- ✅ 添加模型切换功能（支持 ResNet / EfficientNet）
- ✅ 支持视频流文件分析（.mp4/.avi）
- ✅ 接入 HTTP 图像流（远程监控）
- ✅ 引入 OCR 识别 / 手势识别 模型



# English instructions
# 🧠 SmartVision: Universal Recognition System

SmartVision is an intelligent vision platform built with **C++**, **Qt**, **OpenCV**, and **ONNX Runtime**. It supports image classification, YOLO-based object detection, real-time camera recognition, asynchronous multi-threading, voice broadcast, and result translation.

---

## 🚀 Features

- ✅ **Image Classification**: Based on MobileNetV2 with Top-5 output and real-time translation
- ✅ **Object Detection**: YOLOv5 + custom fish detection model, supporting multiple targets
- ✅ **Real-time Camera Recognition**: Switch between static or YOLO detection modes
- ✅ **Multithreading**: AI inference runs in separate threads for a smooth UI
- ✅ **Text-to-Speech**: Auto narration of results
- ✅ **Chinese Translation Support**: Labels translated via Baidu API with caching

---

## 📂 Project Structure

```bash
SmartVision/
├── main.cpp                    # Application entry
├── mainwindow.{h,cpp}         # Main UI window logic
├── ModelRunner.{h,cpp}        # Image classification model
├── YoloDetector.{h,cpp}       # YOLO ONNX model and detection logic
├── ModelWork.{h,cpp}          # Classification inference thread
├── YoloWork.{h,cpp}           # Static image YOLO inference
├── YoloStreamWork.{h,cpp}     # YOLO detection for camera stream
├── CameraWindow.{h,cpp}       # Camera window UI logic
├── CameraWorker.{h,cpp}       # Camera frame grabbing worker
├── CameraConfigDialog.{h,cpp} # Camera configuration popup
├── translate_baidu.h          # Baidu translation thread
├── stdafx.h                   # Common type definitions
├── model/                     # Model files
│   ├── mobilenetv2-7.onnx
│   ├── imagenet_classes.txt
│   ├── yolov5s.onnx
│   ├── coco.names
│   ├── fish.onnx
│   └── fish.names
└── images/                    # Example images
```

---

## 🛠️ Requirements

- **OS**: Windows 10/11 x64
- **Compiler**: Visual Studio 2019/2022 + Qt VS Tools
- **Qt**: Qt 5.15.x (MSVC x64 recommended)
- **ONNX Runtime**: 1.15+
- **OpenCV**: 4.5+ (with opencv_world)
- **Baidu Translate API (optional)**: For label translation

---

## ⚙️ Build Instructions

1. Open the `.sln` file using **Qt VS Tools**.
2. Make sure C++17 is enabled:
   - C/C++ → Language → C++ Language Standard: `ISO C++17`
3. Set up ONNX Runtime and OpenCV paths:
   - Add include & lib directories for ONNX + OpenCV
4. Build and run `SmartVisionApp`.

---

## 🖼️ Usage Guide

### ✅ Image Classification
- Click “Open” to select an image
- Automatically performs inference and shows top-5 labels with translation

### ✅ YOLO Detection
- Click:
  - `yolov5s`: Generic COCO model
  - `Fish Detection`: Custom trained YOLO fish model

### ✅ Real-time Camera
- Click `Camera` to open live recognition window
- Supports resolution switching and toggle for YOLO

---

## 🔁 TODO Expansion

- ✅ Add model selection UI (ResNet, EfficientNet)
- ✅ Support for video file input (.mp4/.avi)
- ✅ HTTP video stream input (e.g. RTSP)
- ✅ Add OCR / gesture recognition modules

