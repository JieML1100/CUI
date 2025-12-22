#pragma once

#include <d2d1.h>
#include <dxgiformat.h>
#include <memory>
#include <string>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

class BitmapSource : public std::enable_shared_from_this<BitmapSource> {
public:
	static std::shared_ptr<BitmapSource> FromWicBitmap(IWICBitmap* bitmap, bool takeOwnership = false);
	static std::shared_ptr<BitmapSource> FromFile(const std::wstring& path);
	static std::shared_ptr<BitmapSource> FromBuffer(const void* data, size_t size);
	static std::shared_ptr<BitmapSource> FromHBitmap(HBITMAP bitmap);
	static std::shared_ptr<BitmapSource> FromHIcon(HICON icon);
	static std::shared_ptr<BitmapSource> CreateEmpty(int width, int height);

	static std::vector<std::shared_ptr<BitmapSource>> FromGifFile(const std::wstring& path);
	static std::vector<std::shared_ptr<BitmapSource>> FromGifFile(const char* path);
	static std::vector<std::shared_ptr<BitmapSource>> FromGifBuffer(const void* data, size_t size);

	[[nodiscard]] IWICBitmap* GetWicBitmap() const;
	[[nodiscard]] D2D1_SIZE_U GetPixelSize() const;
	[[nodiscard]] GUID GetPixelFormat() const;
	[[nodiscard]] FLOAT GetDpiX() const;
	[[nodiscard]] FLOAT GetDpiY() const;
	[[nodiscard]] DXGI_FORMAT GetDxgiFormat() const;
	[[nodiscard]] std::vector<uint8_t> CopyPixels(UINT* stride = nullptr) const;

	[[nodiscard]] Microsoft::WRL::ComPtr<ID2D1Bitmap> CreateD2DBitmap(ID2D1RenderTarget* target, D2D1_BITMAP_PROPERTIES* bitmapProperties = nullptr) const;


	[[nodiscard]] bool Save(const std::wstring& path, const GUID fileFormat = GUID_ContainerFormatPng) const;
	[[nodiscard]] bool Save(std::vector<uint8_t>& buffer, const GUID fileFormat = GUID_ContainerFormatPng) const;

private:
	BitmapSource() = default;

	static std::shared_ptr<BitmapSource> FromBitmapInternal(IWICBitmap* bitmap, bool takeOwnership);
	static Microsoft::WRL::ComPtr<IWICBitmap> EnsureBitmapFormat(IWICBitmapSource* source);
	static Microsoft::WRL::ComPtr<IWICBitmap> EnsureBitmapFormat(IWICBitmap* bitmap);

	Microsoft::WRL::ComPtr<IWICBitmap> wicBitmap;
};

