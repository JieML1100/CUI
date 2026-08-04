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
void MediaPlayer::ConvertNV12ToBGRA(
	const uint8_t* nv12,
	size_t nv12Bytes,
	UINT32 yStride,
	UINT32 width,
	UINT32 height,
	UINT32 cropX,
	UINT32 cropY,
	UINT32 visibleW,
	UINT32 visibleH,
	std::vector<uint8_t>& outBgra)
{
	if (!nv12 || yStride == 0 || width == 0 || height == 0
		|| visibleW == 0 || visibleH == 0) return;
	const UINT32 cx = cropX & ~1u;
	const UINT32 cy = cropY & ~1u;
	UINT32 w = visibleW & ~1u;
	UINT32 h = visibleH & ~1u;
	if (cx >= width || cy >= height) return;
	if (cy + h > height) h = (height - cy) & ~1u;
	if (cx + w > width) w = (width - cx) & ~1u;
	if (w == 0 || h == 0) return;

	if (yStride <= cx) return;
	UINT32 maxWByStride = (yStride - cx) & ~1u;
	if (w > maxWByStride) w = maxWByStride;
	if (w == 0) return;

	const UINT32 uvStride = yStride;
	if (uvStride <= cx) return;
	UINT32 maxWByUvStride = (uvStride - cx) & ~1u;
	if (w > maxWByUvStride) w = maxWByUvStride;
	if (w == 0) return;

	const UINT32 uvRows = (height + 1) / 2;
	const size_t yBytes = static_cast<size_t>(yStride) * height;
	const size_t uvBytes = static_cast<size_t>(uvStride) * uvRows;
	if (yBytes > nv12Bytes || uvBytes > nv12Bytes - yBytes) return;

	const uint8_t* yPlane = nv12;
	const uint8_t* uvPlane = nv12 + yBytes;
	outBgra.resize(static_cast<size_t>(w) * h * 4);

	auto convertRow = [&](UINT32 row)
	{
		const uint8_t* yRow = yPlane
			+ static_cast<size_t>(cy + row) * yStride + cx;
		const uint8_t* uvRow = uvPlane
			+ static_cast<size_t>((cy + row) / 2) * uvStride + cx;
		uint8_t* dst = outBgra.data() + static_cast<size_t>(row) * w * 4;

		for (UINT32 col = 0; col < w; col += 2)
		{
			const int u = static_cast<int>(uvRow[col]) - 128;
			const int v = static_cast<int>(uvRow[col + 1]) - 128;
			const int c0 = (std::max)(0, static_cast<int>(yRow[col]) - 16);
			const int c1 = (std::max)(0, static_cast<int>(yRow[col + 1]) - 16);
			const int rAdd = 409 * v;
			const int gAdd = -100 * u - 208 * v;
			const int bAdd = 516 * u;

			auto writePixel = [&](UINT32 pixel, int y)
			{
				dst[static_cast<size_t>(pixel) * 4] =
					Clip8((298 * y + bAdd + 128) >> 8);
				dst[static_cast<size_t>(pixel) * 4 + 1] =
					Clip8((298 * y + gAdd + 128) >> 8);
				dst[static_cast<size_t>(pixel) * 4 + 2] =
					Clip8((298 * y + rAdd + 128) >> 8);
				dst[static_cast<size_t>(pixel) * 4 + 3] = 0xFF;
			};
			writePixel(col, c0);
			writePixel(col + 1, c1);
		}
	};

	if (h >= 256)
		Concurrency::parallel_for(
			0, static_cast<int>(h),
			[&](int row) { convertRow(static_cast<UINT32>(row)); });
	else
		for (UINT32 row = 0; row < h; ++row) convertRow(row);
}
