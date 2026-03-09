#include "BitmapSource.h"
#include "Factory.h"
#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {
	ComPtr<IWICBitmap> CreateBitmapFromSource(IWICBitmapSource* source) {
		if (!source) {
			return {};
		}
		ComPtr<IWICBitmap> bitmap;
		if (FAILED(_ImageFactory->CreateBitmapFromSource(source, WICBitmapCacheOnLoad, &bitmap))) {
			return {};
		}
		return bitmap;
	}

	ComPtr<IWICBitmap> ConvertTo32bppBitmap(IWICBitmapSource* source) {
		if (!source) {
			return {};
		}

		ComPtr<IWICFormatConverter> converter;
		if (FAILED(_ImageFactory->CreateFormatConverter(&converter))) {
			return {};
		}

		HRESULT hr = converter->Initialize(
			source,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(hr)) {
			return {};
		}

		return CreateBitmapFromSource(converter.Get());
	}

	DXGI_FORMAT DxgiFormatFromWic(const GUID& format) {
		if (format == GUID_WICPixelFormat32bppPBGRA) {
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		}
		if (format == GUID_WICPixelFormat32bppBGRA) {
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		}
		if (format == GUID_WICPixelFormat32bppRGBA) {
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}

std::shared_ptr<BitmapSource> BitmapSource::FromBitmapInternal(IWICBitmap* bitmap, bool takeOwnership) {
	if (!bitmap) {
		return {};
	}

	auto adjusted = EnsureBitmapFormat(bitmap);
	if (!adjusted) {
		return {};
	}

	auto result = std::shared_ptr<BitmapSource>(new BitmapSource());
	if (takeOwnership) {
		result->wicBitmap.Attach(adjusted.Detach());
	}
	else {
		result->wicBitmap = adjusted;
	}
	return result;
}

ComPtr<IWICBitmap> BitmapSource::EnsureBitmapFormat(IWICBitmapSource* source) {
	if (!source) {
		return {};
	}
	return ConvertTo32bppBitmap(source);
}

ComPtr<IWICBitmap> BitmapSource::EnsureBitmapFormat(IWICBitmap* bitmap) {
	if (!bitmap) {
		return {};
	}

	GUID pixelFormat{};
	if (FAILED(bitmap->GetPixelFormat(&pixelFormat))) {
		return {};
	}

	if (pixelFormat == GUID_WICPixelFormat32bppPBGRA) {
		return bitmap;
	}

	return ConvertTo32bppBitmap(bitmap);
}

std::shared_ptr<BitmapSource> BitmapSource::FromWicBitmap(IWICBitmap* bitmap, bool takeOwnership) {
	return FromBitmapInternal(bitmap, takeOwnership);
}

std::shared_ptr<BitmapSource> BitmapSource::FromFile(const std::wstring& path) {
	if (path.empty()) {
		return {};
	}
	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(_ImageFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) {
		return {};
	}
	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame))) {
		return {};
	}
	auto bitmap = EnsureBitmapFormat(frame.Get());
	if (!bitmap) {
		return {};
	}
	return FromBitmapInternal(bitmap.Detach(), true);
}

std::shared_ptr<BitmapSource> BitmapSource::FromBuffer(const void* data, size_t size) {
	if (!data || size == 0) {
		return {};
	}

	ComPtr<IWICStream> stream;
	if (FAILED(_ImageFactory->CreateStream(&stream))) {
		return {};
	}
	if (FAILED(stream->InitializeFromMemory(static_cast<WICInProcPointer>(const_cast<void*>(data)), static_cast<DWORD>(size)))) {
		return {};
	}

	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(_ImageFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) {
		return {};
	}
	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame))) {
		return {};
	}

	auto bitmap = EnsureBitmapFormat(frame.Get());
	if (!bitmap) {
		return {};
	}
	return FromBitmapInternal(bitmap.Detach(), true);
}

std::shared_ptr<BitmapSource> BitmapSource::FromHBitmap(HBITMAP bitmap) {
	if (!bitmap) {
		return {};
	}
	ComPtr<IWICBitmap> wicBitmap;
	if (FAILED(_ImageFactory->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapUsePremultipliedAlpha, &wicBitmap))) {
		return {};
	}
	return FromBitmapInternal(wicBitmap.Detach(), true);
}

std::shared_ptr<BitmapSource> BitmapSource::FromHIcon(HICON icon) {
	if (!icon) {
		return {};
	}
	ComPtr<IWICBitmap> wicBitmap;
	if (FAILED(_ImageFactory->CreateBitmapFromHICON(icon, &wicBitmap))) {
		return {};
	}
	return FromBitmapInternal(wicBitmap.Detach(), true);
}

std::shared_ptr<BitmapSource> BitmapSource::CreateEmpty(int width, int height) {
	if (width <= 0 || height <= 0) {
		return {};
	}
	ComPtr<IWICBitmap> bitmap;
	if (FAILED(_ImageFactory->CreateBitmap(static_cast<UINT>(width), static_cast<UINT>(height), GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap))) {
		return {};
	}
	return FromBitmapInternal(bitmap.Detach(), true);
}

std::vector<std::shared_ptr<BitmapSource>> BitmapSource::FromGifFile(const std::wstring& path) {
	if (path.empty()) {
		return {};
	}
	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(_ImageFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) {
		return {};
	}

	UINT frameCount = 0;
	if (FAILED(decoder->GetFrameCount(&frameCount)) || frameCount == 0) {
		return {};
	}

	std::vector<std::shared_ptr<BitmapSource>> frames;
	frames.reserve(frameCount);
	for (UINT i = 0; i < frameCount; ++i) {
		ComPtr<IWICBitmapFrameDecode> frame;
		if (FAILED(decoder->GetFrame(i, &frame))) {
			continue;
		}

		auto bitmap = EnsureBitmapFormat(frame.Get());
		if (!bitmap) {
			continue;
		}
		auto resource = FromBitmapInternal(bitmap.Detach(), true);
		if (resource) {
			frames.push_back(std::move(resource));
		}
	}
	return frames;
}

std::vector<std::shared_ptr<BitmapSource>> BitmapSource::FromGifFile(const char* path) {
	if (!path) {
		return {};
	}
	auto MultiByteToWide_ = [](const std::string& str, uint32_t codePage)->std::wstring {
		int len = MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0);
		std::wstring wstr(len, L'\0');
		MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), &wstr[0], len);
		return wstr;
		};
	return FromGifFile(MultiByteToWide_(path, CP_ACP));
}

std::vector<std::shared_ptr<BitmapSource>> BitmapSource::FromGifBuffer(const void* data, size_t size) {
	if (!data || size == 0) {
		return {};
	}

	ComPtr<IWICStream> stream;
	if (FAILED(_ImageFactory->CreateStream(&stream))) {
		return {};
	}
	if (FAILED(stream->InitializeFromMemory(static_cast<WICInProcPointer>(const_cast<void*>(data)), static_cast<DWORD>(size)))) {
		return {};
	}

	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(_ImageFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) {
		return {};
	}

	UINT frameCount = 0;
	if (FAILED(decoder->GetFrameCount(&frameCount)) || frameCount == 0) {
		return {};
	}

	std::vector<std::shared_ptr<BitmapSource>> frames;
	frames.reserve(frameCount);
	for (UINT i = 0; i < frameCount; ++i) {
		ComPtr<IWICBitmapFrameDecode> frame;
		if (FAILED(decoder->GetFrame(i, &frame))) {
			continue;
		}
		auto bitmap = EnsureBitmapFormat(frame.Get());
		if (!bitmap) {
			continue;
		}
		auto resource = FromBitmapInternal(bitmap.Detach(), true);
		if (resource) {
			frames.push_back(std::move(resource));
		}
	}
	return frames;
}

IWICBitmap* BitmapSource::GetWicBitmap() const {
	return wicBitmap.Get();
}

D2D1_SIZE_U BitmapSource::GetPixelSize() const {
	if (!wicBitmap) {
		return D2D1::SizeU(0, 0);
	}
	UINT width = 0;
	UINT height = 0;
	wicBitmap->GetSize(&width, &height);
	return D2D1::SizeU(width, height);
}

GUID BitmapSource::GetPixelFormat() const {
	GUID format = GUID_WICPixelFormat32bppPBGRA;
	if (wicBitmap) {
		wicBitmap->GetPixelFormat(&format);
	}
	return format;
}

FLOAT BitmapSource::GetDpiX() const {
	if (!wicBitmap) {
		return 96.0f;
	}
	double dpiX = 96.0;
	double dpiY = 96.0;
	if (FAILED(wicBitmap->GetResolution(&dpiX, &dpiY))) {
		return 96.0f;
	}
	return static_cast<FLOAT>(dpiX);
}

FLOAT BitmapSource::GetDpiY() const {
	if (!wicBitmap) {
		return 96.0f;
	}
	double dpiX = 96.0;
	double dpiY = 96.0;
	if (FAILED(wicBitmap->GetResolution(&dpiX, &dpiY))) {
		return 96.0f;
	}
	return static_cast<FLOAT>(dpiY);
}

DXGI_FORMAT BitmapSource::GetDxgiFormat() const {
	return DxgiFormatFromWic(GetPixelFormat());
}

std::vector<uint8_t> BitmapSource::CopyPixels(UINT* strideOut) const {
	std::vector<uint8_t> pixels;
	if (!wicBitmap) {
		return pixels;
	}
	D2D1_SIZE_U size = GetPixelSize();
	if (size.width == 0 || size.height == 0) {
		return pixels;
	}

	WICRect rect{ 0,0, static_cast<INT>(size.width), static_cast<INT>(size.height) };
	ComPtr<IWICBitmapLock> lock;
	if (FAILED(wicBitmap->Lock(&rect, WICBitmapLockRead, &lock))) {
		return pixels;
	}

	UINT bufferSize = 0;
	BYTE* buffer = nullptr;
	UINT stride = 0;
	if (FAILED(lock->GetStride(&stride))) {
		return pixels;
	}
	if (FAILED(lock->GetDataPointer(&bufferSize, &buffer))) {
		return pixels;
	}

	if (buffer && bufferSize > 0) {
		pixels.assign(buffer, buffer + bufferSize);
	}
	if (strideOut) {
		*strideOut = stride;
	}
	return pixels;
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> BitmapSource::CreateD2DBitmap(ID2D1RenderTarget* target, D2D1_BITMAP_PROPERTIES* bitmapProperties) const {
	ComPtr<ID2D1Bitmap> bitmap;
	if (!target || !wicBitmap) {
		return bitmap;
	}
	target->CreateBitmapFromWicBitmap(wicBitmap.Get(), bitmapProperties, &bitmap);
	return bitmap;
}

bool BitmapSource::Save(const std::wstring& path, const GUID fileFormat) const {
	if (!wicBitmap) {
		return false;
	}

	if (path.empty()) {
		return false;
	}

	UINT w = 0, h = 0;
	if (FAILED(wicBitmap->GetSize(&w, &h))) {
		return false;
	}

	ComPtr<IWICStream> stream;
	if (FAILED(_ImageFactory->CreateStream(&stream))) {
		return false;
	}

	if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) {
		return false;
	}

	ComPtr<IWICBitmapEncoder> encoder;
	if (FAILED(_ImageFactory->CreateEncoder(fileFormat, nullptr, &encoder))) {
		return false;
	}

	if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
		return false;
	}

	ComPtr<IWICBitmapFrameEncode> frameEncode;
	if (FAILED(encoder->CreateNewFrame(&frameEncode, nullptr))) {
		return false;
	}

	if (FAILED(frameEncode->Initialize(nullptr))) {
		return false;
	}

	if (FAILED(frameEncode->SetSize(w, h))) {
		return false;
	}

	WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
	if (FAILED(frameEncode->SetPixelFormat(&format))) {
		return false;
	}

	if (FAILED(frameEncode->WriteSource(wicBitmap.Get(), nullptr))) {
		return false;
	}

	if (FAILED(frameEncode->Commit())) {
		return false;
	}

	if (FAILED(encoder->Commit())) {
		return false;
	}

	return true;
}

bool BitmapSource::Save(std::vector<uint8_t>& buffer, const GUID fileFormat) const {
	if (!wicBitmap)
		return false;

	UINT w = 0, h = 0;
	if (FAILED(wicBitmap->GetSize(&w, &h)))
		return false;

	ComPtr<IStream> stream;
	if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
		return false;

	ComPtr<IWICBitmapEncoder> encoder;
	if (FAILED(_ImageFactory->CreateEncoder(fileFormat, nullptr, &encoder)))
		return false;

	if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
		return false;

	ComPtr<IWICBitmapFrameEncode> frameEncode;
	if (FAILED(encoder->CreateNewFrame(&frameEncode, nullptr)))
		return false;

	if (FAILED(frameEncode->Initialize(nullptr)))
		return false;

	if (FAILED(frameEncode->SetSize(w, h)))
		return false;

	WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
	if (FAILED(frameEncode->SetPixelFormat(&format)))
		return false;

	if (FAILED(frameEncode->WriteSource(wicBitmap.Get(), nullptr)))
		return false;

	if (FAILED(frameEncode->Commit()))
		return false;

	if (FAILED(encoder->Commit()))
		return false;

	STATSTG stat;
	if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)))
		return false;

	ULONGLONG size = stat.cbSize.QuadPart;
	if (size > SIZE_MAX) // 防止 vector 超限
		return false;

	stream->Seek({ 0 }, STREAM_SEEK_SET, nullptr);
	buffer.resize(static_cast<size_t>(size));

	ULONG bytesRead = 0;
	HRESULT hr = stream->Read(buffer.data(), static_cast<ULONG>(size), &bytesRead);
	if (FAILED(hr) || bytesRead != size)
		return false;

	return true;
}

