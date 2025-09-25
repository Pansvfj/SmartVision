#include "stdafx.h"

bool g_useCUDA = false;

QString GetWinErrorMessage(DWORD errorCode) {
	LPWSTR buf = nullptr;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&buf,
		0,
		nullptr);

	QString msg;
	if (buf) {
		msg = QString::fromWCharArray(buf);
		LocalFree(buf);  // 释放
	}
	else {
		msg = QString("Unknown error code: %1").arg(errorCode);
	}
	return msg;
}

bool LoadDLL(const QString& dllPath) {
	HMODULE hModule = LoadLibrary(dllPath.toStdWString().c_str());
	auto err = GetLastError();
	qDebug() << "LoadLibraryEx failed. GetLastError() =" << err << GetWinErrorMessage(err);
	if (hModule == NULL) {
		qDebug() << "Failed to load DLL:" << dllPath;
		return false;
	}
	return true;
}

cv::Mat imreadWithChinese(const QString& filePath) {
	QFile file(filePath);
	if (!file.exists()) return cv::Mat();

	if (!file.open(QIODevice::ReadOnly)) return cv::Mat();

	QByteArray imageData = file.readAll();
	file.close();

	std::vector<uchar> buffer(imageData.begin(), imageData.end());
	return cv::imdecode(buffer, cv::IMREAD_COLOR);
}

int getTextWidth(const QFont& font, const QString& str)
{
	QFontMetrics metrics(font);
	return metrics.horizontalAdvance(str);
}

bool TryAppendCUDA(Ort::SessionOptions& so, int device_id) {

	if (!LoadDLL("onnxruntime_providers_cuda.dll")) {
		qDebug() << "Failed to load CUDA provider DLL";
		return false;
	}

#if ORT_API_VERSION >= 14
	OrtCUDAProviderOptionsV2* cuda_opts = nullptr;
	OrtStatus* st = Ort::GetApi().CreateCUDAProviderOptions(&cuda_opts);
	if (st) { 
		qDebug() << "[ORT][CUDA] CreateOpts:" << Ort::GetApi().GetErrorMessage(st); Ort::GetApi().ReleaseStatus(st);
		return false;
	}
	const std::string dev = std::to_string(device_id);
	const char* keys[] = { "device_id", "arena_extend_strategy", "cudnn_conv_algo_search", "do_copy_in_default_stream" };
	const char* values[] = { dev.c_str(), "kNextPowerOfTwo",        "DEFAULT",                "1" };
	st = Ort::GetApi().UpdateCUDAProviderOptions(cuda_opts, keys, values, 4);
	if (st) {
		qDebug() << "[ORT][CUDA] UpdateOpts:" << Ort::GetApi().GetErrorMessage(st);
		Ort::GetApi().ReleaseStatus(st); Ort::GetApi().ReleaseCUDAProviderOptions(cuda_opts);
		return false;
	}
	st = Ort::GetApi().SessionOptionsAppendExecutionProvider_CUDA_V2(so, cuda_opts);
	if (st) {
		qDebug() << "[ORT][CUDA] AppendEP:" << Ort::GetApi().GetErrorMessage(st); Ort::GetApi().ReleaseStatus(st); 
	}
	Ort::GetApi().ReleaseCUDAProviderOptions(cuda_opts);
	if (!st) {
		qDebug() << "[ORT][CUDA] OK, device =" << device_id;
		return true; 
	}
	return false;
#else
	OrtCUDAProviderOptions opts{};
	opts.device_id = device_id;
	opts.arena_extend_strategy = 0;
	opts.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchDefault;
	opts.do_copy_in_default_stream = 1;
	OrtStatus* st = OrtSessionOptionsAppendExecutionProvider_CUDA(&so, &opts);
	if (st) {
		qDebug() << "[ORT][CUDA] AppendEP:" << Ort::GetApi().GetErrorMessage(st); Ort::GetApi().ReleaseStatus(st); return false;
	}
	qDebug() << "[ORT][CUDA] OK, device =" << device_id;
	return true;
#endif
}

bool LoadDmlProviderDllsOnce() {
	static bool tried = false, ok = false;
	if (!tried) {
		tried = true;
		// 先确保 DLL 实际被加载（也可不写，放同目录通常会被自动加载）
		if (LoadLibraryW(L"onnxruntime_providers_shared.dll") /*&& LoadLibraryW(L"onnxruntime_providers_dml.dll")*/) {
			ok = true;
		}
	}
	return ok;
}

bool TryAppendDML(Ort::SessionOptions& so, int device_id /*=0*/) {

	// 1) 确保 DLL 在位（同目录）并可加载
	if (!LoadDmlProviderDllsOnce()) {
		qDebug() << "[DML] providers dll not found or failed to load.";
		return false;
	}

	const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
	const void* ep_api_void = nullptr;

	// 2) 正确获取 EP 的 API 表
	OrtStatus* st = api->GetExecutionProviderApi("DML", ORT_API_VERSION, &ep_api_void);
	if (st != nullptr) {
		Ort::Status s{ st };
		qDebug() << "[DML] GetExecutionProviderApi failed:" << s.GetErrorMessage().c_str();
		return false;
	}
	if (!ep_api_void) {
		qDebug() << "[DML] ep_api_void is null.";
		return false;
	}

	const OrtDmlApi* dml = reinterpret_cast<const OrtDmlApi*>(ep_api_void);
	if (dml->SessionOptionsAppendExecutionProvider_DML == nullptr) {
		qDebug() << "[DML] SessionOptionsAppendExecutionProvider_DML ptr is null.";
		return false;
	}

	// 3) 追加 DML EP
	OrtStatus* st2 = dml->SessionOptionsAppendExecutionProvider_DML(so, device_id);
	if (st2 != nullptr) {
		Ort::Status s{ st2 };
		qDebug() << "[DML] AppendExecutionProvider_DML failed:" << s.GetErrorMessage().c_str();
		return false;
	}

	qDebug() << "[DML] appended successfully. device_id =" << device_id;
	return true;
}

// 你要用的统一入口：根据 m_deviceId 依次尝试 CUDA -> DML -> CPU
void AppendWithFallback(Ort::SessionOptions& so, int device_id) {

	if (g_useCUDA) {
		if (TryAppendCUDA(so, device_id))
			return;
	} else {
		if (TryAppendDML(so, device_id))
			return;
	}
	qDebug() << "[ORT] Fallback CPU.";
}
