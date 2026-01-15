#include <initguid.h>

#include "MediaPlayer.h"
#include "Form.h"
#include <d3d11_1.h>
#include <d2d1helper.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <mferror.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>

#include <ppl.h>

#include <atomic>

// ============================================================================
// MediaPlayer - Windows 原生媒体播放器控件实现
// ============================================================================
// 基于 Windows Media Foundation 实现的高性能媒体播放器
// 支持以下功能：
// - 视频和音频播放
// - 播放控制：播放、暂停、停止、进度跳转

// 常量定义
static constexpr double HNS_PER_SEC = 10000000.0;  // 100-nanosecond 单位与秒的转换

static float ClampRate(float rate)
{
	if (!(rate > 0.0f)) return 1.0f;
	return (float)std::clamp(rate, 0.25f, 4.0f);
}

// 调试输出函数
static void DebugOutputHr(const wchar_t* context, HRESULT hr)
{
	wchar_t buf[512] = {};
	swprintf_s(buf, L"%s: 0x%08X\n", context ? context : L"", (unsigned)hr);
	OutputDebugStringW(buf);
}

static LARGE_INTEGER QpcNow()
{
	LARGE_INTEGER t{};
	QueryPerformanceCounter(&t);
	return t;
}

static LARGE_INTEGER QpcFreq()
{
	static LARGE_INTEGER f{};
	static std::atomic<bool> inited{ false };
	bool expected = false;
	if (inited.compare_exchange_strong(expected, true))
		QueryPerformanceFrequency(&f);
	return f;
}

static double QpcTicksToMs(UINT64 ticks)
{
	const auto f = QpcFreq();
	if (f.QuadPart <= 0) return 0.0;
	return (double)ticks * 1000.0 / (double)f.QuadPart;
}

static inline uint8_t Clip8(int v)
{
	return (uint8_t)((v < 0) ? 0 : (v > 255 ? 255 : v));
}

// NV12 (Y + interleaved UV) -> BGRA
// 说明：这是 CPU 后备/分析路径，目标是把 MF 的 video processing 从 ReadSample 里挪出来，便于定位瓶颈。
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
	if (!nv12 || yStride == 0 || width == 0 || height == 0 || visibleW == 0 || visibleH == 0) return;
	// 需要保证 UV 对齐：NV12 的 UV 采样为 2x2
	const UINT32 cx = cropX & ~1u;
	const UINT32 cy = cropY & ~1u;
	UINT32 w = (visibleW & ~1u);
	UINT32 h = (visibleH & ~1u);
	if (cx >= width || cy >= height) return;
	if (cy + h > height) h = (height - cy) & ~1u;
	if (cx + w > width) w = (width - cx) & ~1u;
	if (w == 0 || h == 0) return;

	// stride 需要能覆盖可视宽度，否则会越界
	if (yStride <= cx) return;
	UINT32 maxWByStride = (yStride - cx) & ~1u;
	if (w > maxWByStride) w = maxWByStride;
	if (w == 0) return;

	const UINT32 uvStride = yStride;
	if (uvStride <= cx) return;
	UINT32 maxWByUvStride = (uvStride - cx) & ~1u;
	if (w > maxWByUvStride) w = maxWByUvStride;
	if (w == 0) return;

	// 检查 buffer 大小：Y plane + UV plane
	const UINT32 uvRows = (height + 1) / 2;
	const size_t yBytes = (size_t)yStride * (size_t)height;
	const size_t uvBytes = (size_t)uvStride * (size_t)uvRows;
	if (yBytes > nv12Bytes) return;
	if (uvBytes > (nv12Bytes - yBytes)) return;

	const uint8_t* yPlane = nv12;
	const uint8_t* uvPlane = nv12 + yBytes;

	outBgra.resize((size_t)w * (size_t)h * 4);

	auto convertRow = [&](UINT32 row)
	{
		const uint8_t* yRow = yPlane + (size_t)(cy + row) * (size_t)yStride + cx;
		const uint8_t* uvRow = uvPlane + (size_t)((cy + row) / 2) * (size_t)uvStride + cx;
		uint8_t* dst = outBgra.data() + (size_t)row * (size_t)w * 4;

		for (UINT32 col = 0; col < w; col += 2)
		{
			const int U = (int)uvRow[col + 0] - 128;
			const int V = (int)uvRow[col + 1] - 128;
			const int c0 = (int)yRow[col + 0] - 16;
			const int c1 = (int)yRow[col + 1] - 16;
			const int C0 = (c0 < 0) ? 0 : c0;
			const int C1 = (c1 < 0) ? 0 : c1;

			// BT.601-ish integer approximation.
			const int rAdd = 409 * V;
			const int gAdd = -100 * U - 208 * V;
			const int bAdd = 516 * U;

			int r = (298 * C0 + rAdd + 128) >> 8;
			int g = (298 * C0 + gAdd + 128) >> 8;
			int b = (298 * C0 + bAdd + 128) >> 8;
			dst[(size_t)col * 4 + 0] = Clip8(b);
			dst[(size_t)col * 4 + 1] = Clip8(g);
			dst[(size_t)col * 4 + 2] = Clip8(r);
			dst[(size_t)col * 4 + 3] = 0xFF;

			r = (298 * C1 + rAdd + 128) >> 8;
			g = (298 * C1 + gAdd + 128) >> 8;
			b = (298 * C1 + bAdd + 128) >> 8;
			dst[(size_t)(col + 1) * 4 + 0] = Clip8(b);
			dst[(size_t)(col + 1) * 4 + 1] = Clip8(g);
			dst[(size_t)(col + 1) * 4 + 2] = Clip8(r);
			dst[(size_t)(col + 1) * 4 + 3] = 0xFF;
		}
	};

	// 4K 帧行数大，按行并行可明显降低 VConv；小帧保持串行减少调度开销。
	if (h >= 256)
	{
		Concurrency::parallel_for(0, (int)h, [&](int r) { convertRow((UINT32)r); });
	}
	else
	{
		for (UINT32 row = 0; row < h; row++) convertRow(row);
	}
}

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "avrt.lib")

static bool IsFloatMixFormat(const WAVEFORMATEX* wf)
{
	if (!wf) return false;
	if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
	if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		auto* ext = (const WAVEFORMATEXTENSIBLE*)wf;
		return ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	}
	return false;
}

class WsolaTimeStretch
{
public:
	WsolaTimeStretch(UINT32 sampleRate, UINT32 channels, bool isFloat, UINT32 bitsPerSample)
		: _sampleRate(sampleRate), _channels(channels), _isFloat(isFloat), _bitsPerSample(bitsPerSample)
	{
		Configure(sampleRate, channels);
	}

	void Configure(UINT32 sampleRate, UINT32 channels)
	{
		_sampleRate = sampleRate;
		_channels = channels;
		// 典型 WSOLA 参数：20ms 窗，10ms overlap，10ms search
		_windowFrames = (UINT32)std::clamp((int)std::lround((double)sampleRate * 0.020), 256, 4096);
		_overlapFrames = (UINT32)std::clamp((int)std::lround((double)sampleRate * 0.010), 128, (int)_windowFrames / 2);
		_searchFrames = (UINT32)std::clamp((int)std::lround((double)sampleRate * 0.010), 64, (int)_windowFrames);
		_hopOutFrames = _windowFrames - _overlapFrames;
		Reset();
	}

	void Reset()
	{
		_in.clear();
		_baseFrame = 0;
		_nextPredFrame = 0;
		_hasTail = false;
		_tail.clear();
		_out.clear();
	}

	void SetTempo(float tempo)
	{
		_tempo = ClampRate(tempo);
		_hopInFrames = (UINT32)std::clamp((int)std::lround((double)_hopOutFrames * (double)_tempo), 1, (int)(_windowFrames * 4));
	}

	// 输入一段 PCM（mix format），输出尽可能多的已合成 PCM（同格式）。
	bool ProcessChunk(const void* inData, size_t inBytes, float tempo, float volume, std::vector<uint8_t>& outBytes)
	{
		outBytes.clear();
		if (!inData || inBytes == 0 || _channels == 0) return true;
		SetTempo(tempo);

		// bytes -> float frames
		_tmpInFloat.clear();
		if (!BytesToFloat(inData, inBytes, _channels, _bitsPerSample, _isFloat, _tmpInFloat))
			return false;
		const size_t inFrames = _tmpInFloat.size() / _channels;
		if (inFrames == 0) return true;
		AppendInput(_tmpInFloat.data(), inFrames);

		// 生成输出（float）
		Generate();
		if (_out.empty()) return true;

		// 音量（float域）
		if (volume < 0.999f)
		{
			volume = (float)std::clamp(volume, 0.0f, 1.0f);
			for (float& v : _out)
				v *= volume;
		}

		// float -> bytes
		_tmpOutBytes.clear();
		FloatToBytes(_out.data(), _out.size() / _channels, _channels, _bitsPerSample, _isFloat, _tmpOutBytes);
		_out.clear();
		outBytes = std::move(_tmpOutBytes);
		return true;
	}

private:
	UINT32 _sampleRate = 0;
	UINT32 _channels = 0;
	bool _isFloat = false;
	UINT32 _bitsPerSample = 0;

	float _tempo = 1.0f;
	UINT32 _windowFrames = 0;
	UINT32 _overlapFrames = 0;
	UINT32 _searchFrames = 0;
	UINT32 _hopOutFrames = 0;
	UINT32 _hopInFrames = 0;

	std::vector<float> _in;            // interleaved
	size_t _baseFrame = 0;             // absolute frame index for _in[0]
	size_t _nextPredFrame = 0;         // absolute predicted start for next segment

	bool _hasTail = false;
	std::vector<float> _tail;          // overlapFrames * channels (tail of last segment)
	std::vector<float> _out;           // synthesized output frames, interleaved

	std::vector<float> _tmpInFloat;
	std::vector<uint8_t> _tmpOutBytes;

	static bool BytesToFloat(const void* data, size_t bytes, UINT32 channels, UINT32 bits, bool isFloat, std::vector<float>& out)
	{
		const size_t bps = bits / 8;
		if (bps == 0) return false;
		const size_t frameBytes = bps * (size_t)channels;
		if (frameBytes == 0) return false;
		const size_t frames = bytes / frameBytes;
		if (frames == 0) return true;
		out.resize(frames * (size_t)channels);

		const uint8_t* p = (const uint8_t*)data;
		if (bits == 32 && isFloat)
		{
			memcpy(out.data(), p, frames * (size_t)channels * sizeof(float));
			return true;
		}
		if (bits == 16)
		{
			const int16_t* s = (const int16_t*)p;
			for (size_t i = 0; i < frames * (size_t)channels; i++)
				out[i] = (float)s[i] / 32768.0f;
			return true;
		}
		if (bits == 32 && !isFloat)
		{
			const int32_t* s = (const int32_t*)p;
			for (size_t i = 0; i < frames * (size_t)channels; i++)
				out[i] = (float)((double)s[i] / 2147483648.0);
			return true;
		}
		return false;
	}

	static void FloatToBytes(const float* in, size_t frames, UINT32 channels, UINT32 bits, bool isFloat, std::vector<uint8_t>& out)
	{
		const size_t total = frames * (size_t)channels;
		if (bits == 32 && isFloat)
		{
			out.resize(total * sizeof(float));
			memcpy(out.data(), in, out.size());
			return;
		}
		if (bits == 16)
		{
			out.resize(total * sizeof(int16_t));
			auto* d = (int16_t*)out.data();
			for (size_t i = 0; i < total; i++)
			{
				float v = std::clamp(in[i], -1.0f, 1.0f);
				int iv = (int)std::lround(v * 32767.0f);
				d[i] = (int16_t)std::clamp(iv, -32768, 32767);
			}
			return;
		}
		// fallback：直接静音输出
		out.assign(total * (bits / 8), 0);
	}

	void AppendInput(const float* frames, size_t frameCount)
	{
		const size_t old = _in.size();
		_in.resize(old + frameCount * (size_t)_channels);
		memcpy(_in.data() + old, frames, frameCount * (size_t)_channels * sizeof(float));
	}

	size_t AvailableFrames() const
	{
		return _in.size() / (size_t)_channels;
	}

	const float* FramePtrAbs(size_t absFrame) const
	{
		const size_t rel = absFrame - _baseFrame;
		return _in.data() + rel * (size_t)_channels;
	}

	// 搜索与 tail 最匹配的候选起点
	size_t FindBestStart(size_t predStartAbs)
	{
		if (!_hasTail || _overlapFrames == 0) return predStartAbs;
		const size_t availAbsEnd = _baseFrame + AvailableFrames();
		if (availAbsEnd <= _baseFrame + _windowFrames) return predStartAbs;
		const size_t maxStart = availAbsEnd - _windowFrames;

		size_t startMin = (predStartAbs > _searchFrames) ? (predStartAbs - _searchFrames) : _baseFrame;
		if (startMin < _baseFrame) startMin = _baseFrame;
		size_t startMax = predStartAbs + _searchFrames;
		if (startMax > maxStart) startMax = maxStart;
		if (startMin > startMax) startMin = startMax;

		// 归一化相关（NCC）比简单点积更稳，能显著降低撕裂感；并用 coarse-to-fine 降低 CPU。
		const UINT32 overlap = _overlapFrames;
		const UINT32 chs = _channels;
		const size_t strideSamples = (size_t)chs;

		// 采样步长：overlap 越大越稀疏（降低运算量），但保留足够判别力。
		const UINT32 iStep = (overlap >= 1024) ? 4u : (overlap >= 512 ? 2u : 1u);
		const size_t sCoarseStep = (overlap >= 512) ? 2u : 1u;

		auto nccScore = [&](size_t startAbs, UINT32 localIStep) -> double
		{
			double dot = 0.0;
			double ea = 0.0;
			double eb = 0.0;
			for (UINT32 i = 0; i < overlap; i += localIStep)
			{
				const float* a = _tail.data() + (size_t)i * strideSamples;
				const float* b = FramePtrAbs(startAbs + i);
				for (UINT32 c = 0; c < chs; c++)
				{
					double av = (double)a[c];
					double bv = (double)b[c];
					dot += av * bv;
					ea += av * av;
					eb += bv * bv;
				}
			}
			const double denom = std::sqrt(ea * eb) + 1e-12;
			return dot / denom;
		};

		double bestScore = -1e300;
		size_t best = startMin;
		for (size_t s = startMin; s <= startMax; s += sCoarseStep)
		{
			double score = nccScore(s, iStep);
			if (score > bestScore)
			{
				bestScore = score;
				best = s;
			}
			if (s >= startMax - sCoarseStep) break; // size_t 防溢出
		}

		// 精细搜索：在 coarse 最优点附近用更密集采样再对齐一次。
		{
			const size_t refineRadius = (size_t)std::min<UINT32>(8u, overlap / 8u + 1u);
			size_t r0 = (best > refineRadius) ? (best - refineRadius) : startMin;
			size_t r1 = best + refineRadius;
			if (r0 < startMin) r0 = startMin;
			if (r1 > startMax) r1 = startMax;

			bestScore = -1e300;
			for (size_t s = r0; s <= r1; s++)
			{
				double score = nccScore(s, 1u);
				if (score > bestScore)
				{
					bestScore = score;
					best = s;
				}
				if (s == r1) break;
			}
		}

		return best;
	}

	void EmitFirst(const float* seg)
	{
		// 输出 window-overlap，尾部 overlap 先缓存，等待下次与新段融合后再输出
		const size_t emitFrames = _windowFrames - _overlapFrames;
		_out.insert(_out.end(), seg, seg + emitFrames * (size_t)_channels);
		_tail.assign(seg + emitFrames * (size_t)_channels, seg + (size_t)_windowFrames * (size_t)_channels);
		_hasTail = true;
	}

	void EmitNext(const float* seg)
	{
		// 先输出融合后的 overlap
		if (_overlapFrames > 0)
		{
			for (UINT32 i = 0; i < _overlapFrames; i++)
			{
				// Raised-cosine crossfade（比线性更不容易产生撕裂/毛刺）
				float w = 1.0f;
				if (_overlapFrames > 1)
				{
					const float x = (float)i / (float)(_overlapFrames - 1);
					w = 0.5f - 0.5f * std::cos(3.14159265358979323846f * x);
				}
				for (UINT32 ch = 0; ch < _channels; ch++)
				{
					float a = _tail[(size_t)i * (size_t)_channels + ch];
					float b = seg[(size_t)i * (size_t)_channels + ch];
					_out.push_back(a * (1.0f - w) + b * w);
				}
			}
		}

		// 输出中间部分（window - 2*overlap），尾部 overlap 缓存
		const UINT32 midStart = _overlapFrames;
		const UINT32 midEnd = (_windowFrames > _overlapFrames) ? (_windowFrames - _overlapFrames) : _overlapFrames;
		if (midEnd > midStart)
		{
			const float* p0 = seg + (size_t)midStart * (size_t)_channels;
			const float* p1 = seg + (size_t)midEnd * (size_t)_channels;
			_out.insert(_out.end(), p0, p1);
		}
		_tail.assign(seg + (size_t)(_windowFrames - _overlapFrames) * (size_t)_channels, seg + (size_t)_windowFrames * (size_t)_channels);
		_hasTail = true;
	}

	void MaybeDropOldInput()
	{
		// 保留 search 窗口之前的一点余量即可
		if (_nextPredFrame <= _baseFrame) return;
		size_t keepFrom = (_nextPredFrame > _searchFrames) ? (_nextPredFrame - _searchFrames) : _baseFrame;
		if (keepFrom <= _baseFrame) return;
		size_t dropFrames = keepFrom - _baseFrame;
		// 不要频繁 erase；累计到一定规模再 compact
		if (dropFrames < 4096) return;
		const size_t dropSamples = dropFrames * (size_t)_channels;
		if (dropSamples >= _in.size())
		{
			_in.clear();
			_baseFrame = keepFrom;
			return;
		}
		_in.erase(_in.begin(), _in.begin() + (ptrdiff_t)dropSamples);
		_baseFrame = keepFrom;
	}

	void Generate()
	{
		if (_windowFrames == 0 || _hopOutFrames == 0) return;
		const size_t availAbsEnd = _baseFrame + AvailableFrames();

		if (!_hasTail)
		{
			if (availAbsEnd < _baseFrame + _windowFrames) return;
			const float* seg = FramePtrAbs(_baseFrame);
			EmitFirst(seg);
			_nextPredFrame = _baseFrame + _hopInFrames;
			MaybeDropOldInput();
		}

		for (;;)
		{
			// 需要保证候选段可用
			if (availAbsEnd < _nextPredFrame + _windowFrames) break;
			size_t bestStart = FindBestStart(_nextPredFrame);
			// 关键：保证起点单调前进。
			// 在慢速(tempo<1)时，相关性搜索可能回跳到更早的位置，导致 _nextPredFrame 不前进，
			// 从而在同一段输入上无限生成输出，最终表现为“进度卡住/音频异常”。
			if (bestStart < _nextPredFrame) bestStart = _nextPredFrame;
			if (availAbsEnd < bestStart + _windowFrames) break;
			const float* seg = FramePtrAbs(bestStart);
			EmitNext(seg);
			_nextPredFrame = bestStart + _hopInFrames;
			MaybeDropOldInput();
		}
	}
};

static void ApplyVolume(void* data, size_t bytes, UINT32 bitsPerSample, float volume, bool isFloat)
{
	if (!data || bytes == 0) return;
	if (volume >= 0.999f) return;
	volume = (float)std::clamp(volume, 0.0f, 1.0f);

	if (bitsPerSample == 16)
	{
		auto* samples = (int16_t*)data;
		size_t sampleCount = bytes / sizeof(int16_t);
		for (size_t i = 0; i < sampleCount; i++)
		{
			float v = (float)samples[i] * volume;
			v = std::clamp(v, -32768.0f, 32767.0f);
			samples[i] = (int16_t)v;
		}
		return;
	}

	if (bitsPerSample == 32)
	{
		if (isFloat)
		{
			auto* samples = (float*)data;
			size_t sampleCount = bytes / sizeof(float);
			for (size_t i = 0; i < sampleCount; i++)
				samples[i] *= volume;
			return;
		}
		else
		{
			auto* samples = (int32_t*)data;
			size_t sampleCount = bytes / sizeof(int32_t);
			for (size_t i = 0; i < sampleCount; i++)
			{
				double v = (double)samples[i] * (double)volume;
				v = std::clamp(v, (double)INT32_MIN, (double)INT32_MAX);
				samples[i] = (int32_t)v;
			}
			return;
		}
	}
}

static bool TimeScaleInterleavedPcm(
	const void* inData,
	size_t inBytes,
	UINT32 channels,
	UINT32 bitsPerSample,
	bool isFloat,
	float rate,
	std::vector<uint8_t>& out)
{
	if (!inData || inBytes == 0 || channels == 0) return false;
	rate = ClampRate(rate);

	const size_t bytesPerSample = bitsPerSample / 8;
	if (bytesPerSample == 0) return false;
	const size_t bytesPerFrame = bytesPerSample * (size_t)channels;
	if (bytesPerFrame == 0) return false;
	const size_t inFrames = inBytes / bytesPerFrame;
	if (inFrames == 0) return false;

	const size_t outFrames = (size_t)std::max(1.0, std::floor((double)inFrames / (double)rate));
	const size_t outBytes = outFrames * bytesPerFrame;
	out.resize(outBytes);

	auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };

	for (size_t of = 0; of < outFrames; of++)
	{
		double srcPos = (double)of * (double)rate;
		if (srcPos < 0.0) srcPos = 0.0;
		if (srcPos > (double)(inFrames - 1)) srcPos = (double)(inFrames - 1);
		size_t i0 = (size_t)srcPos;
		size_t i1 = (i0 + 1 < inFrames) ? (i0 + 1) : i0;
		double t = srcPos - (double)i0;

		const uint8_t* f0 = (const uint8_t*)inData + i0 * bytesPerFrame;
		const uint8_t* f1 = (const uint8_t*)inData + i1 * bytesPerFrame;
		uint8_t* fo = out.data() + of * bytesPerFrame;

		if (bitsPerSample == 16)
		{
			for (UINT32 ch = 0; ch < channels; ch++)
			{
				int16_t s0 = ((const int16_t*)f0)[ch];
				int16_t s1 = ((const int16_t*)f1)[ch];
				double v = lerp((double)s0, (double)s1, t);
				v = std::clamp(v, -32768.0, 32767.0);
				((int16_t*)fo)[ch] = (int16_t)v;
			}
		}
		else if (bitsPerSample == 32)
		{
			if (isFloat)
			{
				for (UINT32 ch = 0; ch < channels; ch++)
				{
					float s0 = ((const float*)f0)[ch];
					float s1 = ((const float*)f1)[ch];
					((float*)fo)[ch] = (float)lerp((double)s0, (double)s1, t);
				}
			}
			else
			{
				for (UINT32 ch = 0; ch < channels; ch++)
				{
					int32_t s0 = ((const int32_t*)f0)[ch];
					int32_t s1 = ((const int32_t*)f1)[ch];
					double v = lerp((double)s0, (double)s1, t);
					v = std::clamp(v, (double)INT32_MIN, (double)INT32_MAX);
					((int32_t*)fo)[ch] = (int32_t)v;
				}
			}
		}
		else
		{
			memcpy(fo, f0, bytesPerFrame);
		}
	}

	return true;
}

static bool TryGetVideoAperture(IMFMediaType* mt, MFVideoArea& area)
{
	if (!mt) return false;
	UINT32 cb = sizeof(MFVideoArea);
	if (SUCCEEDED(mt->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, cb, &cb)) && cb == sizeof(MFVideoArea))
		return true;
	cb = sizeof(MFVideoArea);
	if (SUCCEEDED(mt->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area, cb, &cb)) && cb == sizeof(MFVideoArea))
		return true;
	cb = sizeof(MFVideoArea);
	if (SUCCEEDED(mt->GetBlob(MF_MT_PAN_SCAN_APERTURE, (UINT8*)&area, cb, &cb)) && cb == sizeof(MFVideoArea))
		return true;
	return false;
}

static void ApplyVideoCropFromMediaType(IMFMediaType* mt, UINT32 frameW, UINT32 frameH, UINT32& cropX, UINT32& cropY, UINT32& visibleW, UINT32& visibleH)
{
	cropX = 0;
	cropY = 0;
	visibleW = frameW;
	visibleH = frameH;
	if (!mt || frameW == 0 || frameH == 0) return;

	MFVideoArea area{};
	if (!TryGetVideoAperture(mt, area)) return;

	// MFVideoArea: OffsetX/OffsetY 指定可视区域左上角(整数部分在 value)，Area(cx,cy) 指定宽高。
	int left = (int)area.OffsetX.value;
	int top = (int)area.OffsetY.value;
	int w = (int)area.Area.cx;
	int h = (int)area.Area.cy;
	if (w <= 0 || h <= 0) return;

	// clamp 到帧范围
	left = std::clamp(left, 0, (int)frameW);
	top = std::clamp(top, 0, (int)frameH);
	w = std::clamp(w, 0, (int)frameW - left);
	h = std::clamp(h, 0, (int)frameH - top);
	if (w <= 0 || h <= 0) return;

	cropX = (UINT32)left;
	cropY = (UINT32)top;
	visibleW = (UINT32)w;
	visibleH = (UINT32)h;
}

// ========================================
// MediaPlayerCallback 实现
// ========================================

MediaPlayerCallback::MediaPlayerCallback(MediaPlayer* player) : _refCount(1), _player(player) {}
MediaPlayerCallback::~MediaPlayerCallback() {}

STDMETHODIMP MediaPlayerCallback::QueryInterface(REFIID riid, void** ppv)
{
	if (ppv == nullptr) return E_POINTER;
	if (riid == __uuidof(IMFAsyncCallback) || riid == __uuidof(IUnknown))
	{
		*ppv = static_cast<IMFAsyncCallback*>(this);
		AddRef();
		return S_OK;
	}
	*ppv = nullptr;
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MediaPlayerCallback::AddRef() { return InterlockedIncrement(&_refCount); }
STDMETHODIMP_(ULONG) MediaPlayerCallback::Release() { ULONG count = InterlockedDecrement(&_refCount); if (count == 0) delete this; return count; }

STDMETHODIMP MediaPlayerCallback::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
	if (pdwFlags == nullptr || pdwQueue == nullptr) return E_POINTER;
	*pdwFlags = 0; 
	*pdwQueue = MFASYNC_CALLBACK_QUEUE_STANDARD;
	return S_OK;
}

STDMETHODIMP MediaPlayerCallback::Invoke(IMFAsyncResult* pResult)
{
	MediaPlayer* player = _player;
	if (!player) return S_OK;
	if (!player->_mediaSession) return S_OK;

	HRESULT hr = S_OK;
	ComPtr<IMFMediaEvent> pEvent;
	hr = player->_mediaSession->EndGetEvent(pResult, &pEvent);
	if (FAILED(hr)) return S_OK;

	MediaEventType eventType;
	hr = pEvent->GetType(&eventType);
	if (FAILED(hr)) return S_OK;

	switch (eventType)
	{
	case MESessionTopologyStatus:
	{
		UINT32 status = 0;
		if (SUCCEEDED(pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &status)))
		{
			if (status == MF_TOPOSTATUS_READY)
			{
				player->_topologyReady = true;
				player->RefreshVideoFormatFromSource();
				if (player->_mediaLoaded)
				{
					player->SetVolumeImpl(player->_volume);
					player->SetPlaybackRateImpl(player->_playbackRate);
					if (player->_pendingStart)
					{
						player->_pendingStart = false;
						const bool usePos = player->_hasPendingStartPosition;
						const double pos = player->_pendingStartPosition;
						player->_hasPendingStartPosition = false;
						player->StartPlaybackInternal(usePos, pos);
					}
				}
			}
		}
		break;
	}
	case MESessionStarted:
	{
		player->_playState = MediaPlayer::PlayState::Playing;
		player->UpdatePositionFromClock(true);
		player->PostRender();
		break;
	}
	case MESessionPaused:
	{
		player->_playState = MediaPlayer::PlayState::Paused;
		player->UpdatePositionFromClock(true);
		player->PostRender();
		break;
	}
	case MESessionStopped:
	{
		player->_playState = MediaPlayer::PlayState::Stopped;
		player->UpdatePositionFromClock(true);
		player->PostRender();
		break;
	}
	case MESessionEnded:
	{
		// 播放结束
		player->_playState = MediaPlayer::PlayState::Stopped;
		player->UpdatePositionFromClock(true);
		player->OnMediaEnded(player);
		if (player->_loop)
		{
			player->Seek(0.0);
			player->Play();
		}
		break;
	}
	case MEError:
	{
		// 发生错误
		player->_playState = MediaPlayer::PlayState::Stopped;
		HRESULT statusHr = S_OK;
		(void)pEvent->GetStatus(&statusHr);
		player->_lastMfError = statusHr;
		DebugOutputHr(L"MEError", statusHr);
		player->OnMediaFailed(player);
		player->PostRender();
		break;
	}
	}

	// 继续获取事件
	if (player->_mediaSession)
	{
		player->_mediaSession->BeginGetEvent(this, nullptr);
	}
	return S_OK;
}

// ========================================
// VideoSampleGrabberCallback 实现（完全自渲染视频帧）
// ========================================

class VideoSampleGrabberCallback : public IMFSampleGrabberSinkCallback
{
public:
	VideoSampleGrabberCallback(MediaPlayer* player) : _refCount(1), _player(player) {}
	virtual ~VideoSampleGrabberCallback() {}
	void DetachPlayer() { _player = nullptr; }

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		if (!ppv) return E_POINTER;
		if (riid == __uuidof(IMFSampleGrabberSinkCallback) || riid == __uuidof(IMFClockStateSink) || riid == __uuidof(IUnknown))
		{
			*ppv = static_cast<IMFSampleGrabberSinkCallback*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef() { return (ULONG)InterlockedIncrement(&_refCount); }
	STDMETHODIMP_(ULONG) Release()
	{
		ULONG count = (ULONG)InterlockedDecrement(&_refCount);
		if (count == 0) delete this;
		return count;
	}

	// IMFClockStateSink
	STDMETHODIMP OnClockStart(MFTIME, LONGLONG) { return S_OK; }
	STDMETHODIMP OnClockStop(MFTIME) { return S_OK; }
	STDMETHODIMP OnClockPause(MFTIME) { return S_OK; }
	STDMETHODIMP OnClockRestart(MFTIME) { return S_OK; }
	STDMETHODIMP OnClockSetRate(MFTIME, float) { return S_OK; }

	// IMFSampleGrabberSinkCallback
	STDMETHODIMP OnSetPresentationClock(IMFPresentationClock*) { return S_OK; }

	STDMETHODIMP OnProcessSample(
		REFGUID guidMajorMediaType,
		DWORD,
		LONGLONG,
		LONGLONG,
		const BYTE* pSampleBuffer,
		DWORD dwSampleSize)
	{
		if (guidMajorMediaType != MFMediaType_Video) return S_OK;
		MediaPlayer* player = _player;
		if (!player) return S_OK;
		if (!pSampleBuffer || dwSampleSize == 0) return S_OK;
		player->OnVideoFrame(pSampleBuffer, dwSampleSize);
		return S_OK;
	}

	STDMETHODIMP OnShutdown() { return S_OK; }

private:
	LONG _refCount;
	MediaPlayer* _player;
};

// ========================================
// MediaPlayer 实现
// ========================================

UIClass MediaPlayer::Type() { return UIClass::UI_MediaPlayer; }

MediaPlayer::MediaPlayer(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
	this->BolderColor = D2D1_COLOR_F{ 0.3f, 0.3f, 0.3f, 1.0f };

	HRESULT hr = InitializeMF();
	if (FAILED(hr))
	{
		_initialized = false;
	}
	else
	{
		_initialized = true;
	}
}

MediaPlayer::~MediaPlayer()
{
	if (_eventCallback) _eventCallback->DetachPlayer();
	_threadExit = true;
	_threadPlaying = false;
	_threadCv.notify_all();
	if (_playThread.joinable())
		_playThread.join();
	ShutdownWasapi();
	ShutdownSourceReader();
	ReleaseResources();
	MFShutdown();
	if (_didCoInit)
	{
		::CoUninitialize();
		_didCoInit = false;
	}
}

HRESULT MediaPlayer::InitializeMF()
{
	HRESULT hr = S_OK;

	// Media Foundation 组件/解析器/渲染器大量依赖 COM。
	// CUI 工程不保证入口处已初始化 COM，因此这里做一次兼容式初始化。
	_coInitHr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (_coInitHr == RPC_E_CHANGED_MODE)
	{
		// 已经以 STA 初始化（例如 WebBrowser），则退化为 STA。
		_coInitHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	}
	_didCoInit = (_coInitHr == S_OK || _coInitHr == S_FALSE);
	if (FAILED(_coInitHr) && _coInitHr != RPC_E_CHANGED_MODE)
	{
		DebugOutputHr(L"CoInitializeEx failed", _coInitHr);
		return _coInitHr;
	}

	// 初始化 Media Foundation
	// 注意：MFSTARTUP_LITE 可能导致 Media Session 的工作队列/异步事件不完整，
	// 从而出现“拓扑永远不 Ready、Play/Seek 无反应、无音视频输出”的现象。
	// 这里使用完整启动以确保 Media Session 正常工作。
	hr = MFStartup(MF_VERSION, 0);
	if (FAILED(hr)) return hr;

	// 创建 Media Session + 事件回调
	hr = CreateMediaSession();
	if (FAILED(hr)) return hr;

	// 初始化 Direct3D
	hr = InitializeD3D();
	if (FAILED(hr)) return hr;

	return hr;
}

bool MediaPlayer::InitWasapi()
{
	ShutdownWasapi();
	HRESULT hr = S_OK;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_mmDeviceEnumerator));
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: MMDeviceEnumerator", hr); return false; }

	hr = _mmDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &_audioDevice);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: GetDefaultAudioEndpoint", hr); return false; }

	hr = _audioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&_audioClient);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: Activate IAudioClient", hr); return false; }

	hr = _audioClient->GetMixFormat(&_audioMixFormat);
	if (FAILED(hr) || !_audioMixFormat) { DebugOutputHr(L"WASAPI: GetMixFormat", hr); return false; }

	// Use shared mode, event-driven not required.
	REFERENCE_TIME bufferDuration = 1000000; // 100ms
	hr = _audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, _audioMixFormat, nullptr);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: Initialize", hr); return false; }

	hr = _audioClient->GetBufferSize(&_audioBufferFrameCount);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: GetBufferSize", hr); return false; }

	hr = _audioClient->GetService(IID_PPV_ARGS(&_audioRenderClient));
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: GetService IAudioRenderClient", hr); return false; }

	_audioChannels = _audioMixFormat->nChannels;
	_audioSamplesPerSec = _audioMixFormat->nSamplesPerSec;
	_audioBitsPerSample = _audioMixFormat->wBitsPerSample;
	_audioBlockAlign = _audioMixFormat->nBlockAlign;
	_audioBytesPerSec = _audioMixFormat->nAvgBytesPerSec;
	_timeStretch.reset();

	return true;
}

void MediaPlayer::ShutdownWasapi()
{
	if (_audioClient)
		(void)_audioClient->Stop();
	_audioRenderClient.Reset();
	_audioClient.Reset();
	_audioDevice.Reset();
	_mmDeviceEnumerator.Reset();
	_timeStretch.reset();
	if (_audioMixFormat)
	{
		CoTaskMemFree(_audioMixFormat);
		_audioMixFormat = nullptr;
	}
	_audioBufferFrameCount = 0;
}

bool MediaPlayer::ConfigureSourceReaderVideoType()
{
	if (!_sourceReader) return false;

	// 尝试多种视频格式以提高兼容性
	// RGB32 (BGRA) 是最兼容的格式，Direct2D 原生支持
	GUID formats[] = {
		MFVideoFormat_NV12,
		MFVideoFormat_RGB32,
		MFVideoFormat_ARGB32,
		MFVideoFormat_RGB24,
	};

	for (int i = 0; i < (int)(sizeof(formats) / sizeof(formats[0])); i++)
	{
		if (!_preferNv12VideoOutput && formats[i] == MFVideoFormat_NV12)
			continue;
		ComPtr<IMFMediaType> mt;
		if (FAILED(MFCreateMediaType(&mt)) || !mt) continue;
		if (FAILED(mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) continue;
		if (FAILED(mt->SetGUID(MF_MT_SUBTYPE, formats[i]))) continue;

		(void)mt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		(void)mt->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);

		HRESULT hr = _sourceReader->SetCurrentMediaType(_srVideoStream, nullptr, mt.Get());
		if (SUCCEEDED(hr))
		{
			_usingNv12VideoOutput = (formats[i] == MFVideoFormat_NV12);
			// 刷新当前视频格式信息（尺寸/stride/像素格式/aperture）
			UpdateVideoFormatFromSourceReader();
			return true;
		}
	}

	DebugOutputHr(L"SourceReader: All video format attempts failed", E_FAIL);
	return false;
}

bool MediaPlayer::ConfigureSourceReaderAudioTypeFromMixFormat()
{
	if (!_sourceReader) return false;
	if (!_audioMixFormat) return false;

	ComPtr<IMFMediaType> mt;
	if (FAILED(MFCreateMediaType(&mt)) || !mt) return false;
	if (FAILED(mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))) return false;

	GUID subtype = MFAudioFormat_PCM;
	if (_audioMixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || _audioMixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		// If extensible, use SubFormat.
		if (_audioMixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			auto* ext = (WAVEFORMATEXTENSIBLE*)_audioMixFormat;
			if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
				subtype = MFAudioFormat_Float;
			else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
				subtype = MFAudioFormat_PCM;
		}
		else
		{
			subtype = MFAudioFormat_Float;
		}
	}
	if (FAILED(mt->SetGUID(MF_MT_SUBTYPE, subtype))) return false;

	(void)mt->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, _audioMixFormat->nChannels);
	(void)mt->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, _audioMixFormat->nSamplesPerSec);
	(void)mt->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, _audioMixFormat->wBitsPerSample);
	(void)mt->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, _audioMixFormat->nBlockAlign);
	(void)mt->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, _audioMixFormat->nAvgBytesPerSec);
	(void)mt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	HRESULT hr = _sourceReader->SetCurrentMediaType(_srAudioStream, nullptr, mt.Get());
	if (FAILED(hr))
	{
		DebugOutputHr(L"SourceReader: SetCurrentMediaType(audio from mix) failed", hr);
		return false;
	}
	return true;
}

void MediaPlayer::UpdateVideoFormatFromSourceReader()
{
	if (!_sourceReader) return;
	ComPtr<IMFMediaType> mt;
	if (FAILED(_sourceReader->GetCurrentMediaType(_srVideoStream, &mt)) || !mt) return;
	
	UINT32 w = 0, h = 0;
	if (SUCCEEDED(MFGetAttributeSize(mt.Get(), MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0)
	{
		UINT32 cropX = 0, cropY = 0, visibleW = w, visibleH = h;
		ApplyVideoCropFromMediaType(mt.Get(), w, h, cropX, cropY, visibleW, visibleH);

		_videoFrameSize = SIZE{ (LONG)w, (LONG)h };
		_videoSize = SIZE{ (LONG)visibleW, (LONG)visibleH };
		_videoCropX = cropX;
		_videoCropY = cropY;
		
		// 获取实际的stride（步长）
		UINT32 strideAttr = MFGetAttributeUINT32(mt.Get(), MF_MT_DEFAULT_STRIDE, 0);
		INT32 signedStride = (INT32)strideAttr;
		bool bottomUp = (signedStride < 0);
		UINT32 stride = bottomUp ? (UINT32)(-signedStride) : strideAttr;
		
		// 检查格式以确定正确的stride
		GUID subtype;
		if (SUCCEEDED(mt->GetGUID(MF_MT_SUBTYPE, &subtype)))
		{
			UINT32 bytesPerPixel = 4;
			if (subtype == MFVideoFormat_RGB32 || subtype == MFVideoFormat_ARGB32)
			{
				// RGB32/ARGB32: 每像素4字节
				bytesPerPixel = 4;
				if (stride == 0) stride = w * 4;
			}
			else if (subtype == MFVideoFormat_NV12)
			{
				// NV12: 8-bit 4:2:0 (Y + interleaved UV). 这里的 stride 指的是 Y plane stride。
				// NV12 不应该按 bottom-up 解释（负 stride 常见于 RGB 位图）。
				bottomUp = false;
				bytesPerPixel = 1;
				if (stride == 0 || stride < w) stride = w;
			}
			else if (subtype == MFVideoFormat_RGB24)
			{
				// RGB24: 每像素3字节，但需要对齐到4字节边界
				bytesPerPixel = 3;
				if (stride == 0) stride = ((w * 3 + 3) / 4) * 4;
			}
			else
			{
				// 其他格式，假设为4字节对齐
				bytesPerPixel = 4;
				if (stride == 0) stride = w * 4;
			}

			{
				std::scoped_lock lock(_videoFrameMutex);
				_videoSubtype = subtype;
				_videoBytesPerPixel = bytesPerPixel;
				_videoBottomUp = bottomUp;
			}
			
			wchar_t dbgMsg[256];
			swprintf_s(dbgMsg, L"Video format: %dx%d, stride=%u, bpp=%u, bottomUp=%d\n", w, h, stride, bytesPerPixel, bottomUp ? 1 : 0);
			OutputDebugStringW(dbgMsg);
		}
		else
		{
			// 无法获取格式，使用默认值
			if (stride == 0) stride = w * 4;
			std::scoped_lock lock(_videoFrameMutex);
			_videoSubtype = GUID_NULL;
			_videoBytesPerPixel = 4;
			_videoBottomUp = false;
		}
		
		_videoStride = stride;
	}
}

bool MediaPlayer::InitSourceReader(const std::wstring& url)
{
	ShutdownSourceReader();

	HRESULT hr = S_OK;
	_usingHardwareDecode = false;
	_usingNv12VideoOutput = false;

	// 第一阶段：尽量在 Win7 环境也可用的方式启用“硬件变换/硬解”(由系统解码器+驱动决定)，
	// 但仍保持输出为 RGB32（通过 SourceReader 的 video processing），以便复用现有 CPU->D2D 位图渲染链路。
	if (_enableHardwareDecode)
	{
		ComPtr<IMFAttributes> attr;
		hr = MFCreateAttributes(&attr, 10);
		if (FAILED(hr)) { DebugOutputHr(L"SourceReader: MFCreateAttributes(HW)", hr); return false; }

		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		// 不设置 MF_SOURCE_READER_DISABLE_DXVA，让 MF 自行选择 DXVA/软件路径。
		// 若优先 NV12，则关闭 MF video processing（否则会把颜色转换成本算进 ReadSample）。
		(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, _preferNv12VideoOutput ? FALSE : TRUE);

		hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);
		if (FAILED(hr) && hr == E_INVALIDARG)
		{
			// 某些系统/解码器对 HW_TRANSFORMS 属性不接受（返回 E_INVALIDARG），做一次温和降级再试。
			DebugOutputHr(L"SourceReader: HW init got E_INVALIDARG, retry without HW_TRANSFORMS", hr);
			attr.Reset();
			if (SUCCEEDED(MFCreateAttributes(&attr, 8)))
			{
				(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, _preferNv12VideoOutput ? FALSE : TRUE);
				hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);
			}
		}
		if (SUCCEEDED(hr) && _sourceReader)
		{
			_usingHardwareDecode = true;
			DebugOutputHr(L"SourceReader: HW transforms/DXVA mode (best-effort)", S_OK);
		}
	}

	// 失败回退：维持原先强制软解配置（最稳）
	if (!_sourceReader)
	{
		ComPtr<IMFAttributes> attr;
		hr = MFCreateAttributes(&attr, 8);
		if (FAILED(hr)) { DebugOutputHr(L"SourceReader: MFCreateAttributes(SW)", hr); return false; }
		(void)attr->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
		(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

		hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);
		if (FAILED(hr))
		{
			DebugOutputHr(L"SourceReader: MFCreateSourceReaderFromURL failed (final)", hr);
			_lastMfError = hr;
			return false;
		}
		DebugOutputHr(L"SourceReader: SW decode mode", S_OK);
	}

	// Ensure streams are selected.
	(void)_sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	(void)_sourceReader->SetStreamSelection(_srVideoStream, TRUE);
	(void)_sourceReader->SetStreamSelection(_srAudioStream, TRUE);

	if (!ConfigureSourceReaderVideoType())
	{
		// Some files are audio-only.
		_hasVideo = false;
	}
	else
	{
		_hasVideo = true;
		UpdateVideoFormatFromSourceReader();
	}

	if (InitWasapi() && ConfigureSourceReaderAudioTypeFromMixFormat())
	{
		_hasAudio = true;
	}
	else
	{
		_hasAudio = false;
	}

	// Find actual stream indices
	_actualVideoStreamIndex = (DWORD)-1;
	_actualAudioStreamIndex = (DWORD)-1;
	for (DWORD i = 0; ; i++)
	{
		ComPtr<IMFMediaType> mt;
		HRESULT hr = _sourceReader->GetCurrentMediaType(i, &mt);
		if (FAILED(hr)) break;

		BOOL selected = FALSE;
		if (SUCCEEDED(_sourceReader->GetStreamSelection(i, &selected)) && selected)
		{
			GUID majorType;
			if (SUCCEEDED(mt->GetMajorType(&majorType)))
			{
				if (majorType == MFMediaType_Video && _actualVideoStreamIndex == (DWORD)-1)
					_actualVideoStreamIndex = i;
				else if (majorType == MFMediaType_Audio && _actualAudioStreamIndex == (DWORD)-1)
					_actualAudioStreamIndex = i;
			}
		}
	}

	// Duration
	PROPVARIANT var;
	PropVariantInit(&var);
	if (SUCCEEDED(_sourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)))
	{
		if (var.vt == VT_UI8)
			_duration = (double)var.uhVal.QuadPart / HNS_PER_SEC;
		else if (var.vt == VT_I8)
			_duration = (double)var.hVal.QuadPart / HNS_PER_SEC;
	}
	PropVariantClear(&var);

	return true;
}

void MediaPlayer::ShutdownSourceReader()
{
	_sourceReader.Reset();
}

void MediaPlayer::StopSourceReaderPlayback(bool shutdown)
{
	_threadPlaying = false;
	_needSyncReset = true;
	if (_audioClient) (void)_audioClient->Stop();
	if (_playThread.joinable())
	{
		_threadExit = true;
		_threadCv.notify_all();
		_playThread.join();
		_threadExit = false;
	}
	if (shutdown)
	{
		ShutdownWasapi();
		ShutdownSourceReader();
	}
}

bool MediaPlayer::WriteAudioToWasapi(const BYTE* data, UINT32 bytes)
{
	if (!_audioClient || !_audioRenderClient || !_audioMixFormat) return false;
	if (!data || bytes == 0) return true;

	_statAudioWriteCalls.fetch_add(1, std::memory_order_relaxed);
	_statAudioWriteBytes.fetch_add(bytes, std::memory_order_relaxed);
	const LARGE_INTEGER t0 = QpcNow();

	// 防止卡死：若音频引擎未推进（padding 永远满）或外部已请求停止，避免在此无限等待。
	const ULONGLONG startTick = GetTickCount64();

	UINT32 offset = 0;
	while (offset < bytes)
	{
		if (_threadExit || !_threadPlaying.load())
			return false;

		UINT32 padding = 0;
		HRESULT hr = _audioClient->GetCurrentPadding(&padding);
		if (FAILED(hr)) { DebugOutputHr(L"WASAPI: GetCurrentPadding", hr); return false; }
		UINT32 availableFrames = _audioBufferFrameCount - padding;
		if (availableFrames == 0)
		{
			if (GetTickCount64() - startTick > 2000)
			{
				DebugOutputHr(L"WASAPI: write timeout (buffer never drains)", E_FAIL);
				return false;
			}
			Sleep(1);
			continue;
		}
		UINT32 bytesPerFrame = _audioMixFormat->nBlockAlign;
		if (bytesPerFrame == 0) return false;
		UINT32 availableBytes = availableFrames * bytesPerFrame;
		UINT32 toWrite = (std::min)(availableBytes, bytes - offset);
		UINT32 framesToWrite = toWrite / bytesPerFrame;
		if (framesToWrite == 0)
		{
			if (GetTickCount64() - startTick > 2000)
			{
				DebugOutputHr(L"WASAPI: write timeout (framesToWrite==0)", E_FAIL);
				return false;
			}
			Sleep(1);
			continue;
		}
		BYTE* pData = nullptr;
		hr = _audioRenderClient->GetBuffer(framesToWrite, &pData);
		if (FAILED(hr)) { DebugOutputHr(L"WASAPI: GetBuffer", hr); return false; }
		memcpy(pData, data + offset, framesToWrite * bytesPerFrame);
		hr = _audioRenderClient->ReleaseBuffer(framesToWrite, 0);
		if (FAILED(hr)) { DebugOutputHr(L"WASAPI: ReleaseBuffer", hr); return false; }
		offset += framesToWrite * bytesPerFrame;
	}
	const LARGE_INTEGER t1 = QpcNow();
	_statAudioWriteQpcTicks.fetch_add((UINT64)(t1.QuadPart - t0.QuadPart), std::memory_order_relaxed);
	return true;
}

void MediaPlayer::PlaybackThreadMain()
{
	HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	// Simple A/V sync based on timestamps.
	LONGLONG firstTs = -1;
	LARGE_INTEGER freq{};
	QueryPerformanceFrequency(&freq);
	LARGE_INTEGER startQpc{};
	QueryPerformanceCounter(&startQpc);

	while (!_threadExit)
	{
		// Wait until playing.
		{
			std::unique_lock lk(_threadMutex);
			_threadCv.wait(lk, [&] { return _threadExit || _threadPlaying.load(); });
			if (_threadExit) break;
		}

		firstTs = -1;

		// SourceReader 后端：始终保持音频输出开启；倍速由我们对 PCM 做时间缩放（允许变调但不断音）。
		if (_audioClient && _hasAudio)
			(void)_audioClient->Start();

		while (_threadPlaying && !_threadExit)
		{
		if (_needSyncReset)
		{
			firstTs = -1;
			if (_timeStretch) _timeStretch->Reset();
			// 倍速/Seek 切换时，WASAPI 缓冲里可能还残留上一次 time-stretch 的音频。
			// 这里通过 Stop+Reset 清空缓冲，避免回到 1.0x 后仍听到“被拉长的片段”。
			if (_audioClient && _hasAudio)
			{
				(void)_audioClient->Stop();
				(void)_audioClient->Reset();
				(void)_audioClient->Start();
			}
			_needSyncReset = false;
		}

		DWORD streamIndex = 0;
		DWORD flags = 0;
		LONGLONG ts = 0;
		ComPtr<IMFSample> sample;
		_statReadSampleCalls.fetch_add(1, std::memory_order_relaxed);
		const LARGE_INTEGER tRead0 = QpcNow();
		HRESULT hr = _sourceReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &ts, &sample);
		const LARGE_INTEGER tRead1 = QpcNow();
		const UINT64 readTicks = (UINT64)(tRead1.QuadPart - tRead0.QuadPart);
		_statReadSampleQpcTicks.fetch_add(readTicks, std::memory_order_relaxed);
		if (FAILED(hr))
		{
			_lastMfError = hr;
			DebugOutputHr(L"SourceReader: ReadSample failed", hr);
			_threadPlaying = false;
			_playState = PlayState::Stopped;
			OnMediaFailed(this);
			break;
		}

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			if (_loop && _sourceReader)
			{
				// Loop: flush & seek to start, then continue.
				(void)_sourceReader->Flush(MF_SOURCE_READER_ALL_STREAMS);
				if (_timeStretch) _timeStretch->Reset();
				PROPVARIANT var;
				PropVariantInit(&var);
				var.vt = VT_I8;
				var.hVal.QuadPart = 0;
				(void)_sourceReader->SetCurrentPosition(GUID_NULL, var);
				PropVariantClear(&var);
				_needSyncReset = true;
				_position = 0.0;
				OnPositionChanged(this, _position);
				// 仍然触发 ended 事件，便于 UI 更新
				OnMediaEnded(this);
				continue;
			}
			_threadPlaying = false;
			_playState = PlayState::Stopped;
			OnMediaEnded(this);
			break;
		}

		if (!sample) continue;
		if (firstTs < 0)
		{
			firstTs = ts;
			QueryPerformanceCounter(&startQpc);
		}

		// Pace based on sample timestamp with playback rate
		float rate = _playbackRate.load();
		if (rate < 0.01f) rate = 1.0f; // 防止除零
		
		const double relSec = (double)(ts - firstTs) / HNS_PER_SEC;
		double targetElapsedSec = relSec / rate;
		
		// 允许长时间等待（慢速播放），但保持可中断与对实时速率变化敏感。
		for (;;)
		{
			if (_threadExit || !_threadPlaying) break;
			if (_needSyncReset) break;
			LARGE_INTEGER now{};
			QueryPerformanceCounter(&now);
			double elapsedSec = (double)(now.QuadPart - startQpc.QuadPart) / (double)freq.QuadPart;
			double delta = targetElapsedSec - elapsedSec;
			if (delta <= 0.005) break;

			// 以小步 sleep，避免一次 Sleep 很久导致停止/换片不响应
			DWORD ms = (DWORD)std::clamp(delta * 1000.0, 1.0, 50.0);
			Sleep(ms);
			// 若用户在等待期间调整了播放速率，下一轮会立刻按新速率重新评估
			rate = _playbackRate.load();
			if (rate < 0.01f) rate = 1.0f;
			targetElapsedSec = relSec / rate;
		}

		_position = (double)ts / HNS_PER_SEC;
		OnPositionChanged(this, _position);

		ComPtr<IMFMediaBuffer> buf;
		_statSamplesToContigCalls.fetch_add(1, std::memory_order_relaxed);
		const LARGE_INTEGER tContig0 = QpcNow();
		hr = sample->ConvertToContiguousBuffer(&buf);
		const LARGE_INTEGER tContig1 = QpcNow();
		_statSamplesToContigQpcTicks.fetch_add((UINT64)(tContig1.QuadPart - tContig0.QuadPart), std::memory_order_relaxed);
		if (FAILED(hr) || !buf) continue;
		BYTE* p = nullptr;
		DWORD maxLen = 0, curLen = 0;
		hr = buf->Lock(&p, &maxLen, &curLen);
		if (FAILED(hr) || !p || curLen == 0) continue;

		// 关键修复：不要仅依赖 _actualVideoStreamIndex/_actualAudioStreamIndex。
		// 对某些文件/解码器，streamIndex 的映射或类型会变化；若误把音频当视频会出现严重花屏。
		GUID majorType{};
		if (_sourceReader)
		{
			ComPtr<IMFMediaType> mt;
			if (SUCCEEDED(_sourceReader->GetCurrentMediaType(streamIndex, &mt)) && mt)
				(void)mt->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
		}
		const bool isVideo = (majorType == MFMediaType_Video);
		const bool isAudio = (majorType == MFMediaType_Audio);
		if (isVideo)
		{
			_statReadSampleVideoCalls.fetch_add(1, std::memory_order_relaxed);
			_statReadSampleVideoQpcTicks.fetch_add(readTicks, std::memory_order_relaxed);
		}
		else if (isAudio)
		{
			_statReadSampleAudioCalls.fetch_add(1, std::memory_order_relaxed);
			_statReadSampleAudioQpcTicks.fetch_add(readTicks, std::memory_order_relaxed);
		}

		// 若发生类型变化，刷新视频格式信息（尺寸/stride/像素格式）。
		if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0 && isVideo)
		{
			UpdateVideoFormatFromSourceReader();
		}

		if (isVideo)
		{
			_actualVideoStreamIndex = streamIndex;
			_hasVideo = true;
			if (_videoSize.cx <= 0 || _videoSize.cy <= 0)
				UpdateVideoFormatFromSourceReader();
			
			// 统一转换为 BGRA32，消除像素格式/stride 差异导致的花屏。
			// 注意：部分解码链路会输出带 padding 的帧，并通过 aperture 指定真实可视区域。
			const LONG frameW = _videoFrameSize.cx;
			const LONG frameH = _videoFrameSize.cy;
			const LONG w = _videoSize.cx;
			const LONG h = _videoSize.cy;
			const UINT32 cropX = _videoCropX;
			const UINT32 cropY = _videoCropY;
			if (frameW > 0 && frameH > 0 && w > 0 && h > 0)
			{
				_statDecodedVideoFrames.fetch_add(1, std::memory_order_relaxed);
				_statVideoConvertCalls.fetch_add(1, std::memory_order_relaxed);
				const LARGE_INTEGER tVid0 = QpcNow();
				GUID subtype{};
				UINT32 srcStride = 0;
				UINT32 bpp = 4;
				bool bottomUp = false;
				{
					std::scoped_lock lock(_videoFrameMutex);
					subtype = _videoSubtype;
					srcStride = _videoStride;
					bpp = (_videoBytesPerPixel == 0) ? 4 : _videoBytesPerPixel;
					bottomUp = _videoBottomUp;
				}

				if (subtype == MFVideoFormat_NV12)
				{
					// NV12: p points to NV12 contiguous buffer.
					if (srcStride == 0) srcStride = (UINT32)frameW;
					std::vector<uint8_t> converted;
					ConvertNV12ToBGRA(p, (size_t)curLen, srcStride, (UINT32)frameW, (UINT32)frameH, cropX, cropY, (UINT32)w, (UINT32)h, converted);
					if (!converted.empty())
					{
						{
							std::scoped_lock lock(_videoFrameMutex);
							_videoFrame = std::move(converted);
							_videoFrameStride = (UINT32)w * 4;
							_videoFrameReady = true;
						}
						const LARGE_INTEGER tVid1 = QpcNow();
						_statVideoConvertQpcTicks.fetch_add((UINT64)(tVid1.QuadPart - tVid0.QuadPart), std::memory_order_relaxed);
						_statVideoConvertBytes.fetch_add((UINT64)w * (UINT64)h * 4ULL, std::memory_order_relaxed);
						this->PostRender();
					}
					else
					{
						const LARGE_INTEGER tVid1 = QpcNow();
						_statVideoConvertQpcTicks.fetch_add((UINT64)(tVid1.QuadPart - tVid0.QuadPart), std::memory_order_relaxed);
					}
					continue;
				}


				const UINT32 minStride = (UINT32)frameW * bpp;
				if (srcStride == 0) srcStride = minStride;
				if (srcStride < minStride) srcStride = minStride;
				const UINT32 needed = srcStride * (UINT32)frameH;
				if (curLen >= needed)
				{
					std::vector<uint8_t> converted;
					converted.resize((size_t)w * (size_t)h * 4);
					const UINT32 cropWBytes = (UINT32)w * 4;
					for (LONG row = 0; row < h; row++)
					{
						LONG rawRow = (LONG)cropY + row;
						if (rawRow < 0 || rawRow >= frameH) break;
						LONG srcRow = bottomUp ? (frameH - 1 - rawRow) : rawRow;
						const BYTE* srcRowPtr = p + (size_t)srcRow * (size_t)srcStride + (size_t)cropX * (size_t)bpp;
						uint8_t* dstRowPtr = converted.data() + (size_t)row * (size_t)w * 4;

						if (bpp == 4)
						{
							memcpy(dstRowPtr, srcRowPtr, (size_t)cropWBytes);
						}
						else if (bpp == 3)
						{
							// MFVideoFormat_RGB24 在 Windows 上通常为 BGR24
							for (LONG x = 0; x < w; x++)
							{
								const BYTE* s = srcRowPtr + (size_t)x * 3;
								uint8_t* d = dstRowPtr + (size_t)x * 4;
								d[0] = s[0];
								d[1] = s[1];
								d[2] = s[2];
								d[3] = 0xFF;
							}
						}
						else
						{
							// Unknown: best effort treat as 32bpp.
							memcpy(dstRowPtr, srcRowPtr, (size_t)cropWBytes);
						}
					}

					{
						std::scoped_lock lock(_videoFrameMutex);
						_videoFrame = std::move(converted);
						_videoFrameStride = (UINT32)w * 4;
						_videoFrameReady = true;
					}
					const LARGE_INTEGER tVid1 = QpcNow();
					_statVideoConvertQpcTicks.fetch_add((UINT64)(tVid1.QuadPart - tVid0.QuadPart), std::memory_order_relaxed);
					_statVideoConvertBytes.fetch_add((UINT64)w * (UINT64)h * 4ULL, std::memory_order_relaxed);
					this->PostRender();
				}
				else
				{
					const LARGE_INTEGER tVid1 = QpcNow();
					_statVideoConvertQpcTicks.fetch_add((UINT64)(tVid1.QuadPart - tVid0.QuadPart), std::memory_order_relaxed);
				}
			}
		}
		else if (isAudio)
		{
			_actualAudioStreamIndex = streamIndex;
			_hasAudio = true;

			float rate = ClampRate(_playbackRate.load());
			const bool isFloat = IsFloatMixFormat(_audioMixFormat);
			const float vol = (float)_volume.load();
			const UINT32 sampleRate = _audioMixFormat ? (UINT32)_audioMixFormat->nSamplesPerSec : 0;
			const UINT32 channels = (UINT32)_audioChannels;
			const UINT32 bits = (UINT32)_audioBitsPerSample;

			// rate≈1 时无需 time-stretch，直接输出可显著降低 CPU 并避免“看起来卡死”。
			if (std::fabs(rate - 1.0f) < 0.0005f)
			{
				ApplyVolume(p, (size_t)curLen, _audioBitsPerSample, vol, isFloat);
				(void)WriteAudioToWasapi(p, curLen);
			}
			else
			{

				// WSOLA: 需要采样率/通道数；若格式不支持则回退到旧实现
				std::vector<uint8_t> stretched;
				bool wsolaOk = false;
				if (sampleRate != 0 && channels != 0 && (bits == 16 || bits == 32))
				{
					if (!_timeStretch)
						_timeStretch = std::make_unique<WsolaTimeStretch>(sampleRate, channels, isFloat, bits);
					wsolaOk = _timeStretch->ProcessChunk(p, (size_t)curLen, rate, vol, stretched);
				}

				if (wsolaOk && !stretched.empty())
				{
					(void)WriteAudioToWasapi(stretched.data(), (UINT32)stretched.size());
				}
				else
				{
					// fallback：旧的时间缩放（会变调），或最后原样输出
					std::vector<uint8_t> scaled;
					if (TimeScaleInterleavedPcm(p, (size_t)curLen, _audioChannels, _audioBitsPerSample, isFloat, rate, scaled) && !scaled.empty())
					{
						ApplyVolume(scaled.data(), scaled.size(), _audioBitsPerSample, vol, isFloat);
						(void)WriteAudioToWasapi(scaled.data(), (UINT32)scaled.size());
					}
					else
					{
						ApplyVolume(p, (size_t)curLen, _audioBitsPerSample, vol, isFloat);
						(void)WriteAudioToWasapi(p, curLen);
					}
				}
			}
		}

		buf->Unlock();
		}

		if (_audioClient && _hasAudio)
			(void)_audioClient->Stop();
	}

	if (SUCCEEDED(hrCo))
		CoUninitialize();
}

HRESULT MediaPlayer::CreateMediaSession()
{
	ShutdownMediaSession();

	HRESULT hr = MFCreateMediaSession(nullptr, &_mediaSession);
	if (FAILED(hr)) return hr;

	_eventCallback = new (std::nothrow) MediaPlayerCallback(this);
	if (_eventCallback)
	{
		hr = _mediaSession->BeginGetEvent(_eventCallback.Get(), nullptr);
		if (FAILED(hr)) return hr;
	}

	_presentationClock.Reset();
	_videoDisplayControl.Reset();
	return S_OK;
}

void MediaPlayer::ShutdownMediaSession()
{
	if (_eventCallback) _eventCallback->DetachPlayer();
	_eventCallback.Reset();
	if (_videoSampleCallback) _videoSampleCallback->DetachPlayer();
	_videoSampleCallback.Reset();

	_presentationClock.Reset();
	_videoDisplayControl.Reset();

	if (_mediaSession)
	{
		_mediaSession->Shutdown();
		_mediaSession.Reset();
	}
}

HRESULT MediaPlayer::EnsureVideoDisplayControl()
{
	// 完全自渲染视频时不需要该服务
	if (_videoSampleCallback) return S_OK;
	if (_videoDisplayControl) return S_OK;
	if (!_mediaSession) return E_NOT_VALID_STATE;

	// 注意：MR_VIDEO_RENDER_SERVICE 只有在视频渲染器已在拓扑中创建后才可用。
	return MFGetService(_mediaSession.Get(), MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&_videoDisplayControl));
}

void MediaPlayer::UpdatePositionFromClock(bool forceEvent)
{
	if (!_mediaLoaded) return;
	if (!_mediaSession) return;

	if (!_presentationClock)
	{
		ComPtr<IMFClock> clock;
		if (SUCCEEDED(_mediaSession->GetClock(&clock)) && clock)
		{
			clock.As(&_presentationClock);
		}
	}
	if (!_presentationClock) return;

	MFTIME t = 0;
	if (FAILED(_presentationClock->GetTime(&t))) return;

	double newPos = (double)t / HNS_PER_SEC;
	if (_duration > 0.0) newPos = std::max(0.0, std::min(newPos, _duration));

	if (forceEvent || std::abs(newPos - _position) >= 0.10)
	{
		_position = newPos;
		OnPositionChanged(this, _position);
	}
}

HRESULT MediaPlayer::InitializeD3D()
{
	HRESULT hr = S_OK;

	// 创建 D3D11 设备
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL featureLevel;
	UINT createDeviceFlags = 0;

	hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		0,
		createDeviceFlags,
		featureLevels,
		2,
		D3D11_SDK_VERSION,
		&_d3dDevice,
		&featureLevel,
		&_d3dContext
	);

	if (FAILED(hr))
	{
		// 如果硬件加速失败，尝试 WARP
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			0,
			createDeviceFlags,
			featureLevels,
			2,
			D3D11_SDK_VERSION,
			&_d3dDevice,
			&featureLevel,
			&_d3dContext
		);
	}

	return hr;
}

HRESULT MediaPlayer::CreateMediaSource(const std::wstring& url)
{
	HRESULT hr = S_OK;
	ComPtr<IMFSourceResolver> pSourceResolver;
	hr = MFCreateSourceResolver(&pSourceResolver);
	if (FAILED(hr)) return hr;

	MF_OBJECT_TYPE objectType;
	ComPtr<IUnknown> pSource;
	hr = pSourceResolver->CreateObjectFromURL(
		url.c_str(),
		MF_RESOLUTION_MEDIASOURCE,
		nullptr,
		&objectType,
		&pSource
	);

	if (FAILED(hr)) return hr;

	hr = pSource.As(&_mediaSource);
	return hr;
}

HRESULT MediaPlayer::CreateTopology()
{
	HRESULT hr = S_OK;
	ComPtr<IMFPresentationDescriptor> pSourcePD;
	ComPtr<IMFTopology> pTopology;

	// 创建拓扑
	hr = MFCreateTopology(&pTopology);
	if (FAILED(hr)) return hr;

	// 获取媒体源描述符
	hr = _mediaSource->CreatePresentationDescriptor(&pSourcePD);
	if (FAILED(hr)) return hr;

	// 获取流数量
	DWORD cSourceStreams = 0;
	hr = pSourcePD->GetStreamDescriptorCount(&cSourceStreams);
	if (FAILED(hr)) return hr;

	for (DWORD i = 0; i < cSourceStreams; i++)
	{
		BOOL fSelected = FALSE;
		ComPtr<IMFStreamDescriptor> pSourceSD;

		hr = pSourcePD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD);
		if (FAILED(hr)) break;

		ComPtr<IMFMediaTypeHandler> pTypeHandler;
		hr = pSourceSD->GetMediaTypeHandler(&pTypeHandler);
		if (FAILED(hr)) break;

		GUID majorType;
		hr = pTypeHandler->GetMajorType(&majorType);
		if (FAILED(hr)) break;

		// 显式选择音/视频流（某些源默认不选中会导致无输出）
		if (!fSelected && (majorType == MFMediaType_Video || majorType == MFMediaType_Audio))
		{
			(void)pSourcePD->SelectStream(i);
			fSelected = TRUE;
		}
		if (!fSelected) continue;

		// 创建源节点
		ComPtr<IMFTopologyNode> pSourceNode;
		hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode);
		if (FAILED(hr)) break;

		hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, _mediaSource.Get());
		if (FAILED(hr)) break;

		hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pSourcePD.Get());
		if (FAILED(hr)) break;

		hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSourceSD.Get());
		if (FAILED(hr)) break;

		hr = pTopology->AddNode(pSourceNode.Get());
		if (FAILED(hr)) break;

		// 创建输出节点
		ComPtr<IMFTopologyNode> pOutputNode;
		if (majorType == MFMediaType_Video)
		{
			_hasVideo = true;
			// 预先记录视频尺寸（供帧拷贝与 D2D 位图创建使用）
			ComPtr<IMFMediaType> currentType;
			if (SUCCEEDED(pTypeHandler->GetCurrentMediaType(&currentType)) && currentType)
			{
				UINT32 w = 0, h = 0;
				if (SUCCEEDED(MFGetAttributeSize(currentType.Get(), MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0)
				{
					_videoSize = SIZE{ (LONG)w, (LONG)h };
					_videoStride = w * 4;
				}
			}
			hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);
			if (FAILED(hr)) break;

			// 使用 Sample Grabber Sink 获取视频帧（完全自渲染，不依赖子窗口/EVR）
			ComPtr<IMFMediaType> pVideoType;
			hr = MFCreateMediaType(&pVideoType);
			if (FAILED(hr)) break;
			hr = pVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			if (FAILED(hr)) break;
			// 选用 MFVideoFormat_RGB32：在部分机器/解码器组合下比 ARGB32 更稳定
			// （仍然是 32bpp，内存布局可按 BGRA 处理，alpha 通道通常可忽略）
			hr = pVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			if (FAILED(hr)) break;
			// 尽量把期望的尺寸/步幅明确写入（否则某些解码链路会协商出带对齐的 stride，
			// 导致我们按 width*4 取帧时出现错位或黑屏）
			if (_videoSize.cx > 0 && _videoSize.cy > 0)
			{
				(void)MFSetAttributeSize(pVideoType.Get(), MF_MT_FRAME_SIZE, (UINT32)_videoSize.cx, (UINT32)_videoSize.cy);
				(void)pVideoType->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)_videoSize.cx * 4);
			}
			(void)pVideoType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

			_videoSampleCallback.Reset();
			_videoSampleCallback.Attach(new VideoSampleGrabberCallback(this));
			ComPtr<IMFActivate> pActivate;
			hr = MFCreateSampleGrabberSinkActivate(pVideoType.Get(), _videoSampleCallback.Get(), &pActivate);
			if (FAILED(hr)) break;
			// 让 Media Session 的时钟驱动 SampleGrabber：这样倍速(IMFRateControl)才能正确工作。
			(void)pActivate->SetUINT32(MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, FALSE);

			hr = pOutputNode->SetObject(pActivate.Get());
			if (FAILED(hr)) break;

			hr = pTopology->AddNode(pOutputNode.Get());
			if (FAILED(hr)) break;

			hr = pSourceNode->ConnectOutput(0, pOutputNode.Get(), 0);
			if (FAILED(hr)) break;

			// 该路径不使用 IMFVideoDisplayControl
		}
		else if (majorType == MFMediaType_Audio)
		{
			_hasAudio = true;
			hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);
			if (FAILED(hr)) break;

			ComPtr<IMFActivate> pActivate;
			hr = MFCreateAudioRendererActivate(&pActivate);
			if (FAILED(hr)) break;

			hr = pOutputNode->SetObject(pActivate.Get());
			if (FAILED(hr)) break;

			hr = pTopology->AddNode(pOutputNode.Get());
			if (FAILED(hr)) break;

			hr = pSourceNode->ConnectOutput(0, pOutputNode.Get(), 0);
			if (FAILED(hr)) break;
		}
	}

	_topology = pTopology;
	return hr;
}

HRESULT MediaPlayer::InitializeVideoRenderer()
{
	// 完全自渲染路径不依赖 EVR/窗口
	return S_OK;
}

void MediaPlayer::OnVideoFrame(const BYTE* data, DWORD size)
{
	if (!data || size == 0) return;
	if (_videoSize.cx <= 0 || _videoSize.cy <= 0) return;
	if (_videoFrameSize.cx <= 0 || _videoFrameSize.cy <= 0) return;

	// SampleGrabber 的输出可能带行对齐 padding；推断 stride 并规范化成连续 BGRA32(width*4)。
	const UINT32 frameW = (UINT32)_videoFrameSize.cx;
	const UINT32 frameH = (UINT32)_videoFrameSize.cy;
	const UINT32 w = (UINT32)_videoSize.cx;
	const UINT32 h = (UINT32)_videoSize.cy;
	const UINT32 cropX = _videoCropX;
	const UINT32 cropY = _videoCropY;
	const UINT32 expectedStride = w * 4;
	const UINT32 expectedSize = expectedStride * h;
	if (w == 0 || h == 0 || frameH == 0 || frameW == 0) return;

	UINT32 srcStride = frameW * 4;
	if ((size % frameH) == 0)
	{
		UINT32 strideCandidate = size / frameH;
		if (strideCandidate >= frameW * 4)
			srcStride = strideCandidate;
	}
	const size_t needed = (size_t)srcStride * (size_t)frameH;
	if (size < needed)
		return;

	std::vector<uint8_t> normalized;
	normalized.resize(expectedSize);
	for (UINT32 row = 0; row < h; row++)
	{
		const UINT32 rawRow = cropY + row;
		if (rawRow >= frameH) break;
		if (cropX + w > frameW) break;
		const BYTE* src = data + (size_t)rawRow * (size_t)srcStride + (size_t)cropX * 4;
		uint8_t* dst = normalized.data() + (size_t)row * (size_t)expectedStride;
		memcpy(dst, src, expectedStride);
	}

	{
		std::scoped_lock lock(_videoFrameMutex);
		_videoFrameStride = expectedStride;
		_videoFrame = std::move(normalized);
		_videoFrameReady = true;
	}
	// CUI 是完全自渲染框架：需要主动 Invalidate 才会刷新画面。
	this->PostRender();
}

void MediaPlayer::RefreshVideoFormatFromSource()
{
	if (!_mediaSource) return;

	ComPtr<IMFPresentationDescriptor> pSourcePD;
	if (FAILED(_mediaSource->CreatePresentationDescriptor(&pSourcePD)) || !pSourcePD) return;

	DWORD cSourceStreams = 0;
	if (FAILED(pSourcePD->GetStreamDescriptorCount(&cSourceStreams))) return;

	for (DWORD i = 0; i < cSourceStreams; i++)
	{
		BOOL fSelected = FALSE;
		ComPtr<IMFStreamDescriptor> pSourceSD;
		HRESULT hr = pSourcePD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD);
		if (FAILED(hr) || !fSelected || !pSourceSD) continue;

		ComPtr<IMFMediaTypeHandler> pTypeHandler;
		hr = pSourceSD->GetMediaTypeHandler(&pTypeHandler);
		if (FAILED(hr) || !pTypeHandler) continue;

		GUID majorType{};
		hr = pTypeHandler->GetMajorType(&majorType);
		if (FAILED(hr) || majorType != MFMediaType_Video) continue;

		ComPtr<IMFMediaType> pMediaType;
		hr = pTypeHandler->GetCurrentMediaType(&pMediaType);
		if (FAILED(hr) || !pMediaType) continue;

		UINT32 width = 0, height = 0;
		hr = MFGetAttributeSize(pMediaType.Get(), MF_MT_FRAME_SIZE, &width, &height);
		if (SUCCEEDED(hr) && width > 0 && height > 0)
		{
			UINT32 cropX = 0, cropY = 0, visibleW = width, visibleH = height;
			ApplyVideoCropFromMediaType(pMediaType.Get(), width, height, cropX, cropY, visibleW, visibleH);
			_videoFrameSize = SIZE{ (LONG)width, (LONG)height };
			_videoSize = SIZE{ (LONG)visibleW, (LONG)visibleH };
			_videoCropX = cropX;
			_videoCropY = cropY;
			_videoStride = width * 4;
		}
		break;
	}
}

bool MediaPlayer::Load(const std::wstring& mediaFile)
{
	if (!_initialized) return false;
	if (!this->ParentForm) return false;

	// 若之前在 SourceReader 后端播放，先彻底停掉线程/音频，避免后续后端切换时状态互相干扰。
	if (_playThread.joinable() || _threadPlaying.load() || _sourceReader)
	{
		StopSourceReaderPlayback(true);
	}

	// Prefer SourceReader+WASAPI path for maximum compatibility in a self-rendered UI.
	if (_useSourceReader)
	{
		// 上面已 StopSourceReaderPlayback(true)
		_mediaFile = mediaFile;
		_mediaLoaded = false;
		_position = 0.0;
		_duration = 0.0;
		_hasVideo = false;
		_hasAudio = false;
		_videoSize = SIZE{ 0,0 };
		{
			std::scoped_lock lock(_videoFrameMutex);
			_videoFrame.clear();
			_videoFrameReady = false;
			_videoFrameStride = 0;
			_videoStride = 0;
			_videoSubtype = GUID_NULL;
			_videoBytesPerPixel = 4;
			_videoBottomUp = false;
			_videoFrameSize = SIZE{ 0,0 };
			_videoCropX = 0;
			_videoCropY = 0;
		}
		if (_videoBitmap && _ownsVideoBitmap)
			_videoBitmap->Release();
		_videoBitmap = nullptr;
		_ownsVideoBitmap = false;

		if (!InitSourceReader(mediaFile))
		{
			_mediaLoaded = false;
			OnMediaFailed(this);
			return false;
		}
		_mediaLoaded = true;
		_playState = PlayState::Stopped;
		OnMediaOpened(this);
		this->PostRender();

		// AutoPlay
		if (_autoPlay)
			Play();
		return true;
	}

	// 每次加载重建 session，避免旧拓扑状态残留
	if (FAILED(CreateMediaSession())) return false;

	HRESULT hr = S_OK;
	_mediaFile = mediaFile;
	_mediaLoaded = false;
	_topologyReady = false;
	_pendingStart = false;
	_hasPendingStartPosition = false;
	_pendingStartPosition = 0.0;

	// 清理之前的资源（不再 Shutdown session）
	_mediaSource.Reset();
	_topology.Reset();
	_videoDisplayControl.Reset();
	_presentationClock.Reset();
	if (_videoBitmap && _ownsVideoBitmap)
	{
		_videoBitmap->Release();
	}
	_videoBitmap = nullptr;
	_ownsVideoBitmap = false;
	{
		std::scoped_lock lock(_videoFrameMutex);
		_videoFrame.clear();
		_videoFrameStride = 0;
		_videoStride = 0;
		_videoFrameReady = false;
	}
	_hasVideo = false;
	_hasAudio = false;
	_position = 0.0;
	_duration = 0.0;
	_videoSize = SIZE{ 0, 0 };

	// 创建媒体源
	hr = CreateMediaSource(mediaFile);
	if (FAILED(hr)) return false;

	// 创建拓扑
	hr = CreateTopology();
	if (FAILED(hr)) return false;

	// 设置拓扑
	hr = _mediaSession->SetTopology(0, _topology.Get());
	if (FAILED(hr)) return false;

	// 完全自渲染：不使用 EVR，不需要 VideoDisplayControl

	_mediaLoaded = true;
	OnMediaOpened(this);

	// 获取时长
	ComPtr<IMFPresentationDescriptor> pSourcePD;
	hr = _mediaSource->CreatePresentationDescriptor(&pSourcePD);
	if (SUCCEEDED(hr))
	{
		UINT64 duration = 0;
		hr = pSourcePD->GetUINT64(MF_PD_DURATION, &duration);
		if (SUCCEEDED(hr))
		{
							_duration = (double)duration / HNS_PER_SEC;  // 转换为秒
		}
	}

	// 获取视频尺寸
	if (_hasVideo)
	{
		RefreshVideoFormatFromSource();
	}

	// 自动播放：允许在拓扑未 Ready 时先 Start（Media Session 会排队等待拓扑完成）
	if (_autoPlay)
	{
		HRESULT startHr = StartPlayback();
		if (SUCCEEDED(startHr))
		{
			_playState = PlayState::Playing;
			this->PostRender();
		}
		else
		{
			_pendingStart = true;
			DebugOutputHr(L"AutoPlay Start failed (will retry on topology ready)", startHr);
		}
	}

	return true;
}

void MediaPlayer::Play()
{
	if (!_mediaLoaded) return;
	if (_useSourceReader)
	{
		if (!_sourceReader) return;
		if (!_playThread.joinable())
		{
			_threadExit = false;
			_playThread = std::thread([this] { PlaybackThreadMain(); });
		}
		_playState = PlayState::Playing;
		_threadPlaying = true;
		_threadCv.notify_all();
		this->PostRender();
		return;
	}
	HRESULT hr = StartPlayback();
	if (SUCCEEDED(hr))
	{
		_playState = PlayState::Playing;
		this->PostRender();
	}
	else
	{
		_pendingStart = true;
		DebugOutputHr(L"Play Start failed (pending)", hr);
	}
}

void MediaPlayer::Pause()
{
	if (!_mediaLoaded || _playState != PlayState::Playing) return;
	if (_useSourceReader)
	{
		_playState = PlayState::Paused;
		_threadPlaying = false;
		if (_audioClient) (void)_audioClient->Stop();
		this->PostRender();
		return;
	}

	HRESULT hr = PausePlayback();
	if (SUCCEEDED(hr))
	{
		_playState = PlayState::Paused;
		this->PostRender();
	}
}

void MediaPlayer::Stop()
{
	if (!_mediaLoaded) return;
	if (_useSourceReader)
	{
		_threadPlaying = false;
		_playState = PlayState::Stopped;
		_position = 0.0;
		if (_timeStretch) _timeStretch->Reset();
		// Seek to start
		if (_sourceReader)
		{
			PROPVARIANT var;
			PropVariantInit(&var);
			var.vt = VT_I8;
			var.hVal.QuadPart = 0;
			(void)_sourceReader->SetCurrentPosition(GUID_NULL, var);
			PropVariantClear(&var);
		}
		this->PostRender();
		return;
	}

	HRESULT hr = StopPlayback();
	if (SUCCEEDED(hr))
	{
		_playState = PlayState::Stopped;
		_position = 0.0;
		this->PostRender();
	}
}

void MediaPlayer::Resume()
{
	if (_playState == PlayState::Paused)
	{
		Play();
	}
}

void MediaPlayer::Seek(double seconds)
{
	if (!_mediaLoaded) return;
	if (_useSourceReader)
	{
		if (!_sourceReader) return;
		seconds = std::max(0.0, std::min(seconds, _duration));
		if (_timeStretch) _timeStretch->Reset();
		PROPVARIANT var;
		PropVariantInit(&var);
		var.vt = VT_I8;
		var.hVal.QuadPart = (LONGLONG)(seconds * HNS_PER_SEC);
		HRESULT hr = _sourceReader->SetCurrentPosition(GUID_NULL, var);
		PropVariantClear(&var);
		if (FAILED(hr))
			DebugOutputHr(L"SourceReader: SetCurrentPosition failed", hr);
		_position = seconds;
		_needSyncReset = true;
		OnPositionChanged(this, _position);
		this->PostRender();
		return;
	}
	if (!_topologyReady)
	{
		_pendingStart = true;
		_hasPendingStartPosition = true;
		_pendingStartPosition = seconds;
		HRESULT startHr = StartPlaybackInternal(true, seconds);
		if (FAILED(startHr))
		{
			DebugOutputHr(L"Seek Start failed (pending)", startHr);
		}
		return;
	}

	HRESULT hr = SetPositionImpl(seconds);
	if (SUCCEEDED(hr))
	{
		_position = std::max(0.0, std::min(seconds, _duration));
		OnPositionChanged(this, _position);
		this->PostRender();
	}
	else
	{
		DebugOutputHr(L"SetPositionImpl failed", hr);
	}
}

HRESULT MediaPlayer::StartPlayback()
{
	return StartPlaybackInternal(false, 0.0);
}

HRESULT MediaPlayer::StartPlaybackInternal(bool usePosition, double positionSeconds)
{
	if (!_mediaSession) return E_NOT_VALID_STATE;

	PROPVARIANT varStart;
	PropVariantInit(&varStart);
	if (usePosition)
	{
		varStart.vt = VT_I8;
		varStart.hVal.QuadPart = (LONGLONG)(positionSeconds * HNS_PER_SEC);
	}
	else
	{
		varStart.vt = VT_EMPTY;
	}

	HRESULT hr = _mediaSession->Start(nullptr, &varStart);
	PropVariantClear(&varStart);
	if (SUCCEEDED(hr))
	{
		SetVolumeImpl(_volume);
		SetPlaybackRateImpl(_playbackRate);
	}
	else
	{
		DebugOutputHr(L"IMFMediaSession::Start failed", hr);
	}
	return hr;
}

HRESULT MediaPlayer::PausePlayback()
{
	HRESULT hr = _mediaSession->Pause();
	return hr;
}

HRESULT MediaPlayer::StopPlayback()
{
	PROPVARIANT varStop;
	PropVariantInit(&varStop);
	varStop.vt = VT_EMPTY;

	HRESULT hr = _mediaSession->Stop();
	PropVariantClear(&varStop);

	return hr;
}

HRESULT MediaPlayer::SetPositionImpl(double seconds)
{
	if (!_mediaSession) return E_NOT_VALID_STATE;

	PROPVARIANT var;
	PropVariantInit(&var);
	var.vt = VT_I8;
	var.hVal.QuadPart = (LONGLONG)(seconds * HNS_PER_SEC);  // 100ns

	HRESULT hr = _mediaSession->Start(nullptr, &var);
	PropVariantClear(&var);
	return hr;
}

HRESULT MediaPlayer::SetVolumeImpl(double volume)
{
	if (!_mediaSession) return E_NOT_VALID_STATE;

	ComPtr<IMFAudioStreamVolume> pAudioVolume;
	HRESULT hr = MFGetService(_mediaSession.Get(), MR_STREAM_VOLUME_SERVICE, IID_PPV_ARGS(&pAudioVolume));
	if (FAILED(hr)) return hr;

	UINT32 channels = 0;
	hr = pAudioVolume->GetChannelCount(&channels);
	if (FAILED(hr)) return hr;

	float fVolume = (float)std::max(0.0, std::min(1.0, volume));
	for (UINT32 i = 0; i < channels; i++)
	{
		hr = pAudioVolume->SetChannelVolume(i, fVolume);
		if (FAILED(hr)) break;
	}

	return hr;
}

HRESULT MediaPlayer::SetPlaybackRateImpl(float rate)
{
	if (!_mediaSession) return E_NOT_VALID_STATE;
	if (rate < 0.01f) rate = 1.0f;

	ComPtr<IMFRateControl> pRateControl;
	HRESULT hr = MFGetService(_mediaSession.Get(), MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS(&pRateControl));
	if (FAILED(hr)) return hr;

	// 优先不 thinning；若不支持则尝试 thinning（有些解码链路只在 thinning 下支持 >1x）。
	hr = pRateControl->SetRate(FALSE, rate);
	if (FAILED(hr))
	{
		HRESULT hr2 = pRateControl->SetRate(TRUE, rate);
		if (SUCCEEDED(hr2)) hr = hr2;
	}
	return hr;
}

void MediaPlayer::ReleaseResources()
{
	ShutdownMediaSession();
	_mediaSource.Reset();
	_topology.Reset();
	_videoDisplayControl.Reset();
	{
		std::scoped_lock lock(_videoFrameMutex);
		_videoFrame.clear();
		_videoFrameStride = 0;
		_videoStride = 0;
		_videoFrameReady = false;
	}
	_mediaLoaded = false;
	_hasVideo = false;
	_hasAudio = false;
	_position = 0.0;
	_duration = 0.0;
	_videoSize = SIZE{ 0, 0 };

	if (_videoBitmap && _ownsVideoBitmap)
	{
		_videoBitmap->Release();
	}
	_videoBitmap = nullptr;
	_ownsVideoBitmap = false;
}

void MediaPlayer::UpdateVideoBitmap()
{
	// 旧 EVR 路径已移除（完全自渲染）
}

void MediaPlayer::Update()
{
	if (!this->IsVisual) return;
	_statRenderUpdates.fetch_add(1, std::memory_order_relaxed);

	auto abs = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	auto d2d = this->ParentForm->Render;

	// 更新播放位置（用于进度条）
	// 关键修复：SourceReader 模式下 position 由播放线程驱动，不能再用 MediaSession 时钟覆盖，否则会来回跳。
	if (!_useSourceReader && _mediaLoaded && _playState == PlayState::Playing)
	{
		UpdatePositionFromClock(false);
	}

	// 背景
	d2d->FillRect(abs.x, abs.y, size.cx, size.cy, this->BackColor);

	// 有视频：尝试更新并绘制最新帧
	if (_hasVideo && _mediaLoaded)
	{
		std::vector<uint8_t> frame;
		UINT32 stride = 0;
		bool hasNewFrame = false;
		{
			std::scoped_lock lock(_videoFrameMutex);
			if (_videoFrameReady)
			{
				frame.swap(_videoFrame);
				stride = _videoFrameStride;
				_videoFrameReady = false;
				hasNewFrame = true;
			}
		}

		// 只有在有新帧时才上传；否则继续绘制上一帧，避免闪烁（背景黑屏）。
		if (hasNewFrame && !frame.empty() && _videoSize.cx > 0 && _videoSize.cy > 0 && stride > 0)
		{
			// 如果视频尺寸发生变化，必须重建 bitmap，否则右侧/下侧可能残留旧像素（常见表现为绿色条）。
			if (_videoBitmap)
			{
				auto ps = _videoBitmap->GetPixelSize();
				if (ps.width != (UINT32)_videoSize.cx || ps.height != (UINT32)_videoSize.cy)
				{
					if (_videoBitmap && _ownsVideoBitmap)
						_videoBitmap->Release();
					_videoBitmap = nullptr;
					_ownsVideoBitmap = false;
				}
			}

			if (!_videoBitmap)
			{
				auto rt = d2d->GetRenderTargetRaw();
				if (rt)
				{
					D2D1_BITMAP_PROPERTIES props{};
					props.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
					props.dpiX = 96.0f;
					props.dpiY = 96.0f;
					rt->CreateBitmap(D2D1::SizeU((UINT32)_videoSize.cx, (UINT32)_videoSize.cy), nullptr, 0, &props, &_videoBitmap);
					_ownsVideoBitmap = true;
				}
			}

			if (_videoBitmap)
			{
				_statVideoUploadCalls.fetch_add(1, std::memory_order_relaxed);
				_statVideoUploadBytes.fetch_add((UINT64)frame.size(), std::memory_order_relaxed);
				const LARGE_INTEGER tUp0 = QpcNow();
				const UINT32 w = (UINT32)_videoSize.cx;
				const UINT32 h = (UINT32)_videoSize.cy;
				const UINT32 expectedStride = w * 4;
				const size_t expectedSize = (size_t)expectedStride * (size_t)h;

				if (frame.size() >= expectedSize && stride == expectedStride)
				{
					_videoBitmap->CopyFromMemory(nullptr, frame.data(), expectedStride);
				}
				else if (stride >= expectedStride && frame.size() >= (size_t)stride * (size_t)h)
				{
					std::vector<uint8_t> normalized;
					normalized.resize(expectedSize);
					for (UINT32 row = 0; row < h; row++)
					{
						const uint8_t* src = frame.data() + (size_t)row * (size_t)stride;
						uint8_t* dst = normalized.data() + (size_t)row * (size_t)expectedStride;
						memcpy(dst, src, expectedStride);
					}
					_videoBitmap->CopyFromMemory(nullptr, normalized.data(), expectedStride);
				}
				else if (frame.size() >= expectedSize)
				{
					// stride 元数据不可信但数据是连续的：按 expectedStride 写入
					_videoBitmap->CopyFromMemory(nullptr, frame.data(), expectedStride);
				}
				const LARGE_INTEGER tUp1 = QpcNow();
				_statVideoUploadQpcTicks.fetch_add((UINT64)(tUp1.QuadPart - tUp0.QuadPart), std::memory_order_relaxed);
			}
		}

		// 没有新帧也要继续绘制上一帧，避免闪烁
		if (_videoBitmap && _videoSize.cx > 0 && _videoSize.cy > 0)
		{
			// 根据渲染模式计算目标矩形
			float destX = (float)abs.x;
			float destY = (float)abs.y;
			float destWidth = (float)size.cx;
			float destHeight = (float)size.cy;
			
			float videoWidth = (float)_videoSize.cx;
			float videoHeight = (float)_videoSize.cy;
			
			switch (_renderMode)
			{
			case VideoRenderMode::Fit: // 等比缩放，居中显示（默认）
				{
					float scaleX = destWidth / videoWidth;
					float scaleY = destHeight / videoHeight;
					float scale = (scaleX < scaleY) ? scaleX : scaleY;
					
					float scaledWidth = videoWidth * scale;
					float scaledHeight = videoHeight * scale;
					
					destX += (destWidth - scaledWidth) * 0.5f;
					destY += (destHeight - scaledHeight) * 0.5f;
					destWidth = scaledWidth;
					destHeight = scaledHeight;
				}
				break;
				
			case VideoRenderMode::Fill: // 等比缩放，裁剪以填满
				{
					float scaleX = destWidth / videoWidth;
					float scaleY = destHeight / videoHeight;
					float scale = (scaleX > scaleY) ? scaleX : scaleY;
					
					float scaledWidth = videoWidth * scale;
					float scaledHeight = videoHeight * scale;
					
					destX += (destWidth - scaledWidth) * 0.5f;
					destY += (destHeight - scaledHeight) * 0.5f;
					destWidth = scaledWidth;
					destHeight = scaledHeight;
				}
				break;
				
			case VideoRenderMode::Stretch: // 拉伸填充，可能变形
				// 使用控件完整尺寸，不需要调整
				break;
				
			case VideoRenderMode::Center: // 原始尺寸，居中显示
				{
					destX += (destWidth - videoWidth) * 0.5f;
					destY += (destHeight - videoHeight) * 0.5f;
					destWidth = videoWidth;
					destHeight = videoHeight;
				}
				break;
				
			case VideoRenderMode::UniformToFill: // 均匀填充（同Fill）
				{
					float scaleX = destWidth / videoWidth;
					float scaleY = destHeight / videoHeight;
					float scale = (scaleX > scaleY) ? scaleX : scaleY;
					
					float scaledWidth = videoWidth * scale;
					float scaledHeight = videoHeight * scale;
					
					destX += (destWidth - scaledWidth) * 0.5f;
					destY += (destHeight - scaledHeight) * 0.5f;
					destWidth = scaledWidth;
					destHeight = scaledHeight;
				}
				break;
			}
			
			_statDrawBitmapCalls.fetch_add(1, std::memory_order_relaxed);
			const LARGE_INTEGER tDraw0 = QpcNow();
			d2d->DrawBitmap(_videoBitmap, destX, destY, destWidth, destHeight);
			const LARGE_INTEGER tDraw1 = QpcNow();
			_statDrawBitmapQpcTicks.fetch_add((UINT64)(tDraw1.QuadPart - tDraw0.QuadPart), std::memory_order_relaxed);
			ReportPerfStatsIfDue();
			return;
		}
	}
	ReportPerfStatsIfDue();
}

void MediaPlayer::ReportPerfStatsIfDue()
{
	if (!_mediaLoaded) return;

	const auto freq = QpcFreq();
	if (freq.QuadPart <= 0) return;

	const LONGLONG now = QpcNow().QuadPart;
	LONGLONG last = _statLastReportQpc.load(std::memory_order_relaxed);
	if (last != 0 && (now - last) < freq.QuadPart) return;
	if (!_statLastReportQpc.compare_exchange_strong(last, now, std::memory_order_relaxed))
		return;

	const double intervalSec = (last == 0) ? 1.0 : (double)(now - last) / (double)freq.QuadPart;
	if (intervalSec <= 0.0) return;

	const UINT64 rsCalls = _statReadSampleCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 rsTicks = _statReadSampleQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 rsVC = _statReadSampleVideoCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 rsVT = _statReadSampleVideoQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 rsAC = _statReadSampleAudioCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 rsAT = _statReadSampleAudioQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 contigCalls = _statSamplesToContigCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 contigTicks = _statSamplesToContigQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 vFrames = _statDecodedVideoFrames.exchange(0, std::memory_order_relaxed);
	const UINT64 vConvCalls = _statVideoConvertCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 vConvTicks = _statVideoConvertQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 vConvBytes = _statVideoConvertBytes.exchange(0, std::memory_order_relaxed);
	const UINT64 aCalls = _statAudioWriteCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 aTicks = _statAudioWriteQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 aBytes = _statAudioWriteBytes.exchange(0, std::memory_order_relaxed);
	const UINT64 uCalls = _statVideoUploadCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 uTicks = _statVideoUploadQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 uBytes = _statVideoUploadBytes.exchange(0, std::memory_order_relaxed);
	const UINT64 dCalls = _statDrawBitmapCalls.exchange(0, std::memory_order_relaxed);
	const UINT64 dTicks = _statDrawBitmapQpcTicks.exchange(0, std::memory_order_relaxed);
	const UINT64 updCalls = _statRenderUpdates.exchange(0, std::memory_order_relaxed);

	const double fps = (intervalSec > 0.0) ? (double)vFrames / intervalSec : 0.0;
	const double readAvgMs = (rsCalls > 0) ? QpcTicksToMs(rsTicks) / (double)rsCalls : 0.0;
	const double readVAvgMs = (rsVC > 0) ? QpcTicksToMs(rsVT) / (double)rsVC : 0.0;
	const double readAAvgMs = (rsAC > 0) ? QpcTicksToMs(rsAT) / (double)rsAC : 0.0;
	const double contigAvgMs = (contigCalls > 0) ? QpcTicksToMs(contigTicks) / (double)contigCalls : 0.0;
	const double vConvAvgMs = (vConvCalls > 0) ? QpcTicksToMs(vConvTicks) / (double)vConvCalls : 0.0;
	const double aAvgMs = (aCalls > 0) ? QpcTicksToMs(aTicks) / (double)aCalls : 0.0;
	const double upAvgMs = (uCalls > 0) ? QpcTicksToMs(uTicks) / (double)uCalls : 0.0;
	const double drawAvgMs = (dCalls > 0) ? QpcTicksToMs(dTicks) / (double)dCalls : 0.0;
	const double vConvMBs = (intervalSec > 0.0) ? ((double)vConvBytes / (1024.0 * 1024.0)) / intervalSec : 0.0;
	const double upMBs = (intervalSec > 0.0) ? ((double)uBytes / (1024.0 * 1024.0)) / intervalSec : 0.0;
	const double aMBs = (intervalSec > 0.0) ? ((double)aBytes / (1024.0 * 1024.0)) / intervalSec : 0.0;

	wchar_t buf[512] = {};
	swprintf_s(
		buf,
		L"[MediaPlayer][Legacy][%.2fs] mode=%s nv12=%s upd=%llu fps=%.1f | ReadSample %llux %.3fms (V:%llux %.3fms A:%llux %.3fms) | Contig %llux %.3fms | VConv %llux %.3fms %.1fMB/s | Upload %llux %.3fms %.1fMB/s | Draw %llux %.3fms | Audio %llux %.3fms %.1fMB/s\n",
		intervalSec,
		(_usingHardwareDecode ? L"HW" : L"SW"),
		(_usingNv12VideoOutput ? L"Y" : L"N"),
		(unsigned long long)updCalls,
		fps,
		(unsigned long long)rsCalls,
		readAvgMs,
		(unsigned long long)rsVC,
		readVAvgMs,
		(unsigned long long)rsAC,
		readAAvgMs,
		(unsigned long long)contigCalls,
		contigAvgMs,
		(unsigned long long)vConvCalls,
		vConvAvgMs,
		vConvMBs,
		(unsigned long long)uCalls,
		upAvgMs,
		upMBs,
		(unsigned long long)dCalls,
		drawAvgMs,
		(unsigned long long)aCalls,
		aAvgMs,
		aMBs);
	OutputDebugStringW(buf);
}

bool MediaPlayer::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		if (uFileNum > 0)
		{
			TCHAR strFileName[MAX_PATH];
			DragQueryFile(hDropInfo, 0, strFileName, MAX_PATH);
			DragFinish(hDropInfo);

			// 加载第一个文件
			Load(strFileName);
		}
	}
	return true;
	case WM_LBUTTONDBLCLK:
	{
		// 双击切换播放/暂停
		if (_mediaLoaded)
		{
			if (_playState == PlayState::Playing)
			{
				Pause();
			}
			else
			{
				Play();
			}
		}
	}
	break;
	default:
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}

	return true;
}

// ========================================
// 属性实现
// ========================================

GET_CPP(MediaPlayer, MediaPlayer::PlayState, State)
{
	return MediaPlayer::_playState;
}

GET_CPP(MediaPlayer, std::wstring, MediaFile)
{
	return _mediaFile;
}

GET_CPP(MediaPlayer, double, Position)
{
	return _position;
}

SET_CPP(MediaPlayer, double, Position)
{
	if (_mediaLoaded)
	{
		Seek(value);
	}
}

GET_CPP(MediaPlayer, double, Duration)
{
	return _duration;
}

GET_CPP(MediaPlayer, double, Volume)
{
	return _volume.load();
}

SET_CPP(MediaPlayer, double, Volume)
{
	_volume.store(std::max(0.0, std::min(1.0, value)));
	if (_mediaLoaded)
	{
		if (_useSourceReader)
		{
			// SourceReader模式：通过WASAPI设置音量
			if (_audioClient)
			{
				ComPtr<ISimpleAudioVolume> pVolume;
				if (SUCCEEDED(_audioClient->GetService(__uuidof(ISimpleAudioVolume), (void**)&pVolume)))
				{
					pVolume->SetMasterVolume((float)_volume.load(), nullptr);
				}
			}
		}
		else
		{
			// Media Session模式
			SetVolumeImpl(_volume.load());
		}
	}
}

GET_CPP(MediaPlayer, float, PlaybackRate)
{
	return _playbackRate.load();
}

SET_CPP(MediaPlayer, float, PlaybackRate)
{
	_playbackRate.store(value);
	if (_mediaLoaded)
	{
		if (_useSourceReader)
		{
			// SourceReader 模式：视频节奏由时间戳/倍速控制；音频由 PCM 时间缩放输出（允许变调但不断音）。
			_needSyncReset = true;
		}
		else
		{
			// Media Session模式
			SetPlaybackRateImpl(_playbackRate.load());
		}
	}
}

GET_CPP(MediaPlayer, bool, AutoPlay)
{
	return _autoPlay;
}

SET_CPP(MediaPlayer, bool, AutoPlay)
{
	_autoPlay = value;
}

GET_CPP(MediaPlayer, bool, Loop)
{
	return _loop;
}

SET_CPP(MediaPlayer, bool, Loop)
{
	_loop = value;
}

GET_CPP(MediaPlayer, bool, EnableHardwareDecode)
{
	return _enableHardwareDecode;
}

SET_CPP(MediaPlayer, bool, EnableHardwareDecode)
{
	_enableHardwareDecode = value;
}

GET_CPP(MediaPlayer, bool, UsingHardwareDecode)
{
	return _usingHardwareDecode;
}

GET_CPP(MediaPlayer, bool, PreferNv12VideoOutput)
{
	return _preferNv12VideoOutput;
}

SET_CPP(MediaPlayer, bool, PreferNv12VideoOutput)
{
	_preferNv12VideoOutput = value;
}

GET_CPP(MediaPlayer, bool, UsingNv12VideoOutput)
{
	return _usingNv12VideoOutput;
}

GET_CPP(MediaPlayer, bool, HasVideo)
{
	return _hasVideo;
}

GET_CPP(MediaPlayer, bool, HasAudio)
{
	return _hasAudio;
}

GET_CPP(MediaPlayer, SIZE, VideoSize)
{
	return _videoSize;
}

GET_CPP(MediaPlayer, double, Progress)
{
	if (_duration > 0.0)
	{
		return _position / _duration;
	}
	return 0.0;
}

GET_CPP(MediaPlayer, MediaPlayer::VideoRenderMode, RenderMode)
{
	return _renderMode;
}

SET_CPP(MediaPlayer, MediaPlayer::VideoRenderMode, RenderMode)
{
	if (_renderMode != value)
	{
		_renderMode = value;
		this->PostRender();
	}
}
