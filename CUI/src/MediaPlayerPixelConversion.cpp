#include "MediaPlayer.h"

#include <algorithm>
#include <ppl.h>

namespace
{
	uint8_t Clip8(int value) noexcept
	{
		return static_cast<uint8_t>(
			value < 0 ? 0 : (value > 255 ? 255 : value));
	}
}

// Platform-independent CPU fallback kept outside the Media Foundation member.
// Callers that only need pixel conversion no longer select the decoder/session
// backend or its platform import directives from the CUI archive.
bool MediaPlayer::ConvertNV12ToBGRA(
	const uint8_t* nv12,
	size_t nv12Bytes,
	UINT32 yStride,
	UINT32 width,
	UINT32 height,
	UINT32 cropX,
	UINT32 cropY,
	UINT32 visibleW,
	UINT32 visibleH,
	MFVideoTransferMatrix transferMatrix,
	MFNominalRange nominalRange,
	std::vector<uint8_t>& outBgra)
{
	outBgra.clear();
	if (!nv12 || yStride == 0 || width == 0 || height == 0
		|| visibleW == 0 || visibleH == 0) return false;
	if (transferMatrix == MFVideoTransferMatrix_BT2020_10
		|| transferMatrix == MFVideoTransferMatrix_BT2020_12)
		return false;
	if (cropX >= width || cropY >= height || cropX >= yStride)
		return false;
	const UINT32 w = (std::min)(visibleW, width - cropX);
	const UINT32 h = (std::min)(visibleH, height - cropY);
	if (w == 0 || h == 0 || w > yStride - cropX) return false;

	const UINT32 uvStride = yStride;
	if (uvStride < 2) return false;

	const UINT32 uvRows = (height + 1) / 2;
	const size_t yBytes = static_cast<size_t>(yStride) * height;
	const size_t uvBytes = static_cast<size_t>(uvStride) * uvRows;
	if (yBytes > nv12Bytes || uvBytes > nv12Bytes - yBytes) return false;

	const uint8_t* yPlane = nv12;
	const uint8_t* uvPlane = nv12 + yBytes;
	outBgra.resize(static_cast<size_t>(w) * h * 4);
	const bool useBt709 = transferMatrix == MFVideoTransferMatrix_BT709
		|| (transferMatrix == MFVideoTransferMatrix_Unknown && height >= 720);
	const bool fullRange = nominalRange == MFNominalRange_0_255;
	const int yOffset = fullRange ? 0 : 16;
	const int yScale = fullRange ? 256 : 298;
	const int rV = useBt709
		? (fullRange ? 403 : 459)
		: (fullRange ? 359 : 409);
	const int gU = useBt709
		? (fullRange ? -48 : -55)
		: (fullRange ? -88 : -100);
	const int gV = useBt709
		? (fullRange ? -120 : -136)
		: (fullRange ? -183 : -208);
	const int bU = useBt709
		? (fullRange ? 475 : 541)
		: (fullRange ? 454 : 516);

	auto convertRow = [&](UINT32 row)
	{
		const uint8_t* yRow = yPlane
			+ static_cast<size_t>(cropY + row) * yStride;
		const uint8_t* uvRow = uvPlane
			+ static_cast<size_t>((cropY + row) / 2) * uvStride;
		uint8_t* dst = outBgra.data() + static_cast<size_t>(row) * w * 4;

		for (UINT32 col = 0; col < w; ++col)
		{
			const UINT32 sourceX = cropX + col;
			const UINT32 uvX = (std::min)(
				sourceX & ~1u, uvStride - 2);
			const int u = static_cast<int>(uvRow[uvX]) - 128;
			const int v = static_cast<int>(uvRow[uvX + 1]) - 128;
			const int y = (std::max)(
				0, static_cast<int>(yRow[sourceX]) - yOffset);
			uint8_t* pixel = dst + static_cast<size_t>(col) * 4;
			pixel[0] = Clip8((yScale * y + bU * u + 128) >> 8);
			pixel[1] = Clip8(
				(yScale * y + gU * u + gV * v + 128) >> 8);
			pixel[2] = Clip8((yScale * y + rV * v + 128) >> 8);
			pixel[3] = 0xFF;
		}
	};

	if (h >= 256)
		Concurrency::parallel_for(
			0, static_cast<int>(h),
			[&](int row) { convertRow(static_cast<UINT32>(row)); });
	else
		for (UINT32 row = 0; row < h; ++row) convertRow(row);
	return true;
}
