#include <initguid.h>

#include "MediaPlayer.h"
#include "Form.h"
#include <CppUtils/Graphics/Graphics1.h>
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

#include <atomic>
#include <windows.h>

// 常量定义
static constexpr double HNS_PER_SEC = 10000000.0;  // 100-nanosecond 单位与秒的转换
static constexpr LONGLONG VIDEO_TS_REORDER_TOLERANCE_HNS = 20000; // 2ms：允许轻微抖动，避免误丢帧

static float ClampRate(float rate)
{
	if (!(rate > 0.0f)) return 1.0f;
	return (float)std::clamp(rate, 0.25f, 4.0f);
}

// 调试输出函数
static void DebugOutputHr(const wchar_t* context, HRESULT hr)
{
	wchar_t sysMsg[256] = {};
	DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	DWORD len = FormatMessageW(flags, nullptr, (DWORD)hr, 0, sysMsg, (DWORD)_countof(sysMsg), nullptr);
	// 去掉末尾换行，避免重复换行
	if (len > 0)
	{
		while (len > 0 && (sysMsg[len - 1] == L'\r' || sysMsg[len - 1] == L'\n'))
		{
			sysMsg[len - 1] = 0;
			len--;
		}
	}

	wchar_t buf[768] = {};
	if (sysMsg[0])
		swprintf_s(buf, L"%s: 0x%08X (%s)\n", context ? context : L"", (unsigned)hr, sysMsg);
	else
		swprintf_s(buf, L"%s: 0x%08X\n", context ? context : L"", (unsigned)hr);
	OutputDebugStringW(buf);
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
		LONGLONG llSampleTime,
		LONGLONG,
		const BYTE* pSampleBuffer,
		DWORD dwSampleSize)
	{
		if (guidMajorMediaType != MFMediaType_Video) return S_OK;
		MediaPlayer* player = _player;
		if (!player) return S_OK;
		if (!pSampleBuffer || dwSampleSize == 0) return S_OK;
		player->OnVideoFrame(pSampleBuffer, dwSampleSize, llSampleTime);
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
	// 先停止后台打开线程，避免其在 MFShutdown 后仍调用 MF API。
	_openThreadExit = true;
	_openCv.notify_all();
	if (_openThread.joinable())
		_openThread.join();
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

void MediaPlayer::EnsureOpenWorker()
{
	if (_openThread.joinable()) return;
	_openThreadExit = false;
	_openThread = std::thread([this] { OpenWorkerMain(); });
}

void MediaPlayer::OpenWorkerMain()
{
	// SourceReader 创建/解析通常需要 COM。
	HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool didCo = SUCCEEDED(hrCo);

	while (!_openThreadExit.load())
	{
		std::wstring file;
		UINT64 serial = 0;
		{
			std::unique_lock lk(_openMutex);
			_openCv.wait(lk, [&] { return _openThreadExit.load() || _openHasRequest; });
			if (_openThreadExit.load()) break;
			file = _openRequestFile;
			serial = _openSerial.load(std::memory_order_acquire);
			_openHasRequest = false;
		}

		// 如果期间有更新的请求，尽量丢弃旧请求。
		if (serial != _openSerial.load(std::memory_order_acquire))
			continue;

		// 停掉旧播放（可能会阻塞），但在后台线程执行，不再卡 UI。
		if (_playThread.joinable() || _threadPlaying.load() || _sourceReader)
			StopSourceReaderPlayback(true);

		if (serial != _openSerial.load(std::memory_order_acquire))
			continue;

		// 重新初始化 SourceReader
		if (!InitSourceReader(file))
		{
			_mediaLoaded = false;
			_playState = PlayState::Stopped;
			OnMediaFailed(this);
			PostRender();
			continue;
		}

		if (serial != _openSerial.load(std::memory_order_acquire))
		{
			// 已有更新的打开请求：当前 reader 会在下一轮 Stop/Shutdown 里回收。
			continue;
		}

		_mediaFile = file;
		_mediaLoaded = true;
		_position = 0.0;
		_playState = PlayState::Stopped;
		OnMediaOpened(this);
		PostRender();

		if (_autoPlay)
			Play();
	}

	if (didCo)
		CoUninitialize();
}

HRESULT MediaPlayer::InitializeMF()
{
	HRESULT hr = S_OK;

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

	hr = MFStartup(MF_VERSION, 0);
	if (FAILED(hr)) return hr;

	// 创建 Media Session + 事件回调
	hr = CreateMediaSession();
	if (FAILED(hr)) return hr;

	// 初始化 Direct3D
	hr = InitializeD3D();
	if (FAILED(hr)) return hr;

	// 初始化 DXGI Device Manager（零拷贝硬件加速关键）
	hr = InitializeDXGIDeviceManager();
	if (FAILED(hr))
	{
		DebugOutputHr(L"InitializeDXGIDeviceManager failed, fallback to CPU copy", hr);
		_useZeroCopy = false;  // 降级到CPU拷贝模式
	}

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

	// ========== 硬件加速路径：使用原生格式（NV12/YUV）==========
	if (_usingHardwareDecoding)
	{
		// 获取原生媒体类型（以便获取尺寸/帧率信息）
		ComPtr<IMFMediaType> nativeType;
		HRESULT hr = _sourceReader->GetNativeMediaType(_srVideoStream, 0, &nativeType);
		if (FAILED(hr) || !nativeType)
		{
			DebugOutputHr(L"SourceReader: Failed to get native media type", hr);
			return false;
		}

		UINT32 width = 0, height = 0;
		MFGetAttributeSize(nativeType.Get(), MF_MT_FRAME_SIZE, &width, &height);

		struct VideoFormat { GUID subtype; const wchar_t* name; };
		VideoFormat preferredFormats[8];
		int formatCount = 0;

		const bool preferGpuYuv = (_useZeroCopy && _dxgiDeviceManager);
		if (!_videoProcessingEnabled)
		{
			DebugOutputHr(L"SourceReader: Video Processing disabled, strictly preferring Native/NV12", S_OK);
			
			GUID nativeSubtype = GUID_NULL;
			if (SUCCEEDED(nativeType->GetGUID(MF_MT_SUBTYPE, &nativeSubtype)))
			{
				// 仅当 nativeSubtype 本身就是未压缩 YUV 时，才把它放进候选。
				if (nativeSubtype == MFVideoFormat_NV12)
					preferredFormats[formatCount++] = { MFVideoFormat_NV12, L"NV12" };
				else if (nativeSubtype == MFVideoFormat_P010)
					preferredFormats[formatCount++] = { MFVideoFormat_P010, L"P010" };
				else if (nativeSubtype == MFVideoFormat_YUY2)
					preferredFormats[formatCount++] = { MFVideoFormat_YUY2, L"YUY2" };
			}
			// 确保 NV12 在列表里
			preferredFormats[formatCount++] = { MFVideoFormat_NV12, L"NV12" };
			preferredFormats[formatCount++] = { MFVideoFormat_P010, L"P010" };
			preferredFormats[formatCount++] = { MFVideoFormat_YUY2, L"YUY2" };
			// 兜底：某些机器即便关闭 video processing，也可能仍支持输出 RGB
			preferredFormats[formatCount++] = { MFVideoFormat_RGB32, L"RGB32" };
			preferredFormats[formatCount++] = { MFVideoFormat_ARGB32, L"ARGB32" };
		}
		else if (preferGpuYuv)
		{
			preferredFormats[formatCount++] = { MFVideoFormat_NV12, L"NV12" };
			preferredFormats[formatCount++] = { MFVideoFormat_P010, L"P010" };
			preferredFormats[formatCount++] = { MFVideoFormat_YUY2, L"YUY2" };
			preferredFormats[formatCount++] = { MFVideoFormat_RGB32, L"RGB32" };
			preferredFormats[formatCount++] = { MFVideoFormat_ARGB32, L"ARGB32" };
		}
		else
		{
			// 非零拷贝场景下，优先 RGB32 以便 CPU/传统 D2D 路径直接显示
			preferredFormats[formatCount++] = { MFVideoFormat_RGB32, L"RGB32" };
			preferredFormats[formatCount++] = { MFVideoFormat_ARGB32, L"ARGB32" };
			preferredFormats[formatCount++] = { MFVideoFormat_NV12, L"NV12" };
			preferredFormats[formatCount++] = { MFVideoFormat_P010, L"P010" };
			preferredFormats[formatCount++] = { MFVideoFormat_YUY2, L"YUY2" };
		}

		// 正常偏好：优先 RGB32 以支持零拷贝 D2D 渲染
		for (int i = 0; i < formatCount; i++)
		{
			const auto& fmt = preferredFormats[i];
			ComPtr<IMFMediaType> outputType;
			hr = MFCreateMediaType(&outputType);
			if (FAILED(hr)) continue;

			outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			outputType->SetGUID(MF_MT_SUBTYPE, fmt.subtype);

			// 复制尺寸信息
			if (width > 0 && height > 0)
			{
				MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width, height);
			}

			// 复制帧率
			UINT32 numerator = 0, denominator = 0;
			if (SUCCEEDED(MFGetAttributeRatio(nativeType.Get(), MF_MT_FRAME_RATE, &numerator, &denominator)))
			{
				MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, numerator, denominator);
			}

			// 复制像素宽高比
			if (SUCCEEDED(MFGetAttributeRatio(nativeType.Get(), MF_MT_PIXEL_ASPECT_RATIO, &numerator, &denominator)))
			{
				MFSetAttributeRatio(outputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, numerator, denominator);
			}

			// 尝试设置
			hr = _sourceReader->SetCurrentMediaType(_srVideoStream, nullptr, outputType.Get());
			if (SUCCEEDED(hr))
			{
				wchar_t msg[256];
				swprintf_s(msg, L"SourceReader: Using accelerated format: %s", fmt.name);
				DebugOutputHr(msg, S_OK);
				
				// 刷新视频格式信息
				UpdateVideoFormatFromSourceReader();
				return true;
			}
		}

		DebugOutputHr(L"SourceReader: Failed to set any accelerated format, falling back to software RGB32", E_FAIL);
	}

	GUID formats[] = {
		MFVideoFormat_RGB32,
		MFVideoFormat_ARGB32,
		MFVideoFormat_RGB24,
	};

	for (int i = 0; i < (int)(sizeof(formats) / sizeof(formats[0])); i++)
	{
		ComPtr<IMFMediaType> mt;
		if (FAILED(MFCreateMediaType(&mt)) || !mt) continue;
		if (FAILED(mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) continue;
		if (FAILED(mt->SetGUID(MF_MT_SUBTYPE, formats[i]))) continue;

		(void)mt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		(void)mt->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);

		HRESULT hr = _sourceReader->SetCurrentMediaType(_srVideoStream, nullptr, mt.Get());
		if (SUCCEEDED(hr))
		{
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
			const wchar_t* subtypeName = L"Unknown";
			if (subtype == MFVideoFormat_RGB32 || subtype == MFVideoFormat_ARGB32)
			{
				// RGB32/ARGB32: 每像素4字节
				bytesPerPixel = 4;
				subtypeName = (subtype == MFVideoFormat_RGB32) ? L"RGB32" : L"ARGB32";
				if (stride == 0) stride = w * 4;
			}
			else if (subtype == MFVideoFormat_RGB24)
			{
				// RGB24: 每像素3字节，但需要对齐到4字节边界
				bytesPerPixel = 3;
				subtypeName = L"RGB24";
				if (stride == 0) stride = ((w * 3 + 3) / 4) * 4;
			}
			else if (subtype == MFVideoFormat_NV12)
			{
				// NV12: YUV 4:2:0 (1.5 bytes/pixel overall)
				bytesPerPixel = 0;
				subtypeName = L"NV12";
				if (stride == 0) stride = w;
			}
			else if (subtype == MFVideoFormat_P010)
			{
				// P010: 10-bit 4:2:0 (2 bytes per Y sample, overall not an integer bpp)
				bytesPerPixel = 0;
				subtypeName = L"P010";
				if (stride == 0) stride = w * 2;
			}
			else if (subtype == MFVideoFormat_YUY2)
			{
				// YUY2: 4:2:2 packed (2 bytes/pixel)
				bytesPerPixel = 2;
				subtypeName = L"YUY2";
				if (stride == 0) stride = w * 2;
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
			swprintf_s(dbgMsg, L"Video format: %dx%d, subtype=%s, stride=%u, bpp=%u, bottomUp=%d\n", w, h, subtypeName, stride, bytesPerPixel, bottomUp ? 1 : 0);
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

	ComPtr<IMFAttributes> attr;
	HRESULT hr = MFCreateAttributes(&attr, 10);
	if (FAILED(hr)) { DebugOutputHr(L"SourceReader: MFCreateAttributes", hr); return false; }
	
	bool videoProcessingEnabled = false;
	_usingHardwareDecoding = false;

	if (_useZeroCopy && _dxgiDeviceManager)
	{
		hr = attr->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, _dxgiDeviceManager.Get());
		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		videoProcessingEnabled = false;
		_usingHardwareDecoding = true;
		DebugOutputHr(L"SourceReader: DXGI Device Manager attached (Hardware Transforms Enabled)", S_OK);
	}
	else
	{
		(void)attr->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
		(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
		videoProcessingEnabled = true;
		_usingHardwareDecoding = false;
		DebugOutputHr(L"SourceReader: Software decode (CPU) mode", S_OK);
	}

	hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);

	if (FAILED(hr) && _usingHardwareDecoding && hr == E_INVALIDARG)
	{
		DebugOutputHr(L"SourceReader: Hardware creation failed with E_INVALIDARG, retrying without HW_TRANSFORMS flag...", hr);
		attr.Reset();
		if (SUCCEEDED(MFCreateAttributes(&attr, 10)))
		{
			(void)attr->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, _dxgiDeviceManager.Get());
			hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);
		}
	}
	
	if (FAILED(hr) && _usingHardwareDecoding)
	{
		DebugOutputHr(L"SourceReader: Hardware creation failed. Fallback to SOFTWARE mode...", hr);
		
		attr.Reset();
		if (SUCCEEDED(MFCreateAttributes(&attr, 5)))
		{
			(void)attr->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
			(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
			(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
			videoProcessingEnabled = true;
			_usingHardwareDecoding = false;

			hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);
		}
	}

	if (FAILED(hr))
	{
		DebugOutputHr(L"SourceReader: MFCreateSourceReaderFromURL failed (final)", hr);
		_lastMfError = hr;
		return false;
	}

	_videoProcessingEnabled = videoProcessingEnabled;

	// 使用“真实流索引”(0..N-1)进行选择与后续 ReadSample，避免某些源/驱动对 FIRST_* 常量兼容性不佳。
	DWORD videoIndex = (DWORD)-1;
	DWORD audioIndex = (DWORD)-1;
	for (DWORD i = 0; ; i++)
	{
		ComPtr<IMFMediaType> mt;
		HRESULT hrNt = _sourceReader->GetNativeMediaType(i, 0, &mt);
		if (hrNt == MF_E_INVALIDSTREAMNUMBER) break;
		if (FAILED(hrNt) || !mt) continue;
		GUID major{};
		if (FAILED(mt->GetGUID(MF_MT_MAJOR_TYPE, &major))) continue;
		if (major == MFMediaType_Video && videoIndex == (DWORD)-1) videoIndex = i;
		if (major == MFMediaType_Audio && audioIndex == (DWORD)-1) audioIndex = i;
		if (videoIndex != (DWORD)-1 && audioIndex != (DWORD)-1) break;
	}

	(void)_sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	if (videoIndex != (DWORD)-1)
	{
		_srVideoStream = videoIndex;
		_hasVideo = true;
		DebugOutputHr(L"SourceReader: Video stream discovered", S_OK);
		(void)_sourceReader->SetStreamSelection(_srVideoStream, TRUE);
	}
	else
	{
		_hasVideo = false;
		DebugOutputHr(L"SourceReader: No video stream discovered", S_FALSE);
	}
	if (audioIndex != (DWORD)-1)
	{
		_srAudioStream = audioIndex;
		_hasAudio = true;
		DebugOutputHr(L"SourceReader: Audio stream discovered", S_OK);
		(void)_sourceReader->SetStreamSelection(_srAudioStream, TRUE);
	}
	else
	{
		_hasAudio = false;
		DebugOutputHr(L"SourceReader: No audio stream discovered", S_FALSE);
	}

	if (_hasVideo)
	{
		if (!ConfigureSourceReaderVideoType())
			_hasVideo = false;
		else
			UpdateVideoFormatFromSourceReader();
	}
	if (_hasAudio)
	{
		if (InitWasapi() && ConfigureSourceReaderAudioTypeFromMixFormat())
			_hasAudio = true;
		else
			_hasAudio = false;
	}

	_actualVideoStreamIndex = _hasVideo ? _srVideoStream : (DWORD)-1;
	_actualAudioStreamIndex = _hasAudio ? _srAudioStream : (DWORD)-1;

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
	// 关键：Load() 会在 UI 线程同步等待 join。
	// 先 Flush/取消流选择，尽量打断正在进行的 ReadSample 管线，降低“打开新文件卡住”的时间。
	if (_sourceReader)
	{
		(void)_sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
		(void)_sourceReader->Flush(MF_SOURCE_READER_ALL_STREAMS);
	}
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
	return true;
}

void MediaPlayer::PlaybackThreadMain()
{
	HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	LONGLONG firstVideoPts = -1;
	LONGLONG lastVideoPts = LLONG_MIN;
	LONGLONG lastPtsObserved = LLONG_MIN;
	double avgFrameDurationHns = 0.0;
	LONGLONG maxVideoPtsSeen = LLONG_MIN;

	struct PendingVideoFrame
	{
		LONGLONG pts = 0;
		UINT subresourceIndex = 0;
		UINT64 seq = 0;
		ComPtr<IMFSample> sample;
		ComPtr<ID3D11Texture2D> texture;
	};
	std::vector<PendingVideoFrame> pendingVideo;
	pendingVideo.reserve(8);
	UINT64 pendingSeq = 0;
	UINT32 debugVideoTsLogCount = 0;
	static constexpr UINT32 kDebugVideoTsLogFirstN = 120;
	bool triedRecoverInvalidStream = false;
	bool triedRecoverResetPipeline = false;
	LARGE_INTEGER freq{};
	QueryPerformanceFrequency(&freq);
	LARGE_INTEGER startQpc{};
	QueryPerformanceCounter(&startQpc);

	uint64_t lastSeekSerial = _seekSerial.load(std::memory_order_acquire);
	LONGLONG activeSeekTargetHns = _seekTargetHns.load(std::memory_order_acquire);
	bool waitingFirstFrameAfterSeek = false;

	auto PaceVideoToPts = [&](LONGLONG pts)
	{
		if (firstVideoPts < 0)
		{
			firstVideoPts = pts;
			QueryPerformanceCounter(&startQpc);
		}
		float rate = _playbackRate.load();
		if (rate < 0.01f) rate = 1.0f;
		const double relSec = (double)(pts - firstVideoPts) / HNS_PER_SEC;
		double targetElapsedSec = relSec / rate;
		for (;;)
		{
			if (_threadExit || !_threadPlaying) break;
			if (_needSyncReset) break;
			LARGE_INTEGER now{};
			QueryPerformanceCounter(&now);
			double elapsedSec = (double)(now.QuadPart - startQpc.QuadPart) / (double)freq.QuadPart;
			double delta = targetElapsedSec - elapsedSec;
			if (delta <= 0.005) break;

			DWORD ms = (DWORD)std::clamp(delta * 1000.0, 1.0, 50.0);
			Sleep(ms);
			rate = _playbackRate.load();
			if (rate < 0.01f) rate = 1.0f;
			targetElapsedSec = relSec / rate;
		}
	};

	auto UpdateAvgFrameDuration = [&](LONGLONG pts, IMFSample* s)
	{
		LONGLONG dur = 0;
		if (s && SUCCEEDED(s->GetSampleDuration(&dur)) && dur > 0)
		{
			// 使用样本 duration（若可用），通常更可靠。
			if (avgFrameDurationHns <= 0.0) avgFrameDurationHns = (double)dur;
			else avgFrameDurationHns = avgFrameDurationHns * 0.90 + (double)dur * 0.10;
			return;
		}
		if (lastPtsObserved != LLONG_MIN)
		{
			LONGLONG diff = pts - lastPtsObserved;
			if (diff > 0 && diff < (LONGLONG)(HNS_PER_SEC * 2.0))
			{
				if (avgFrameDurationHns <= 0.0) avgFrameDurationHns = (double)diff;
				else avgFrameDurationHns = avgFrameDurationHns * 0.95 + (double)diff * 0.05;
			}
		}
		lastPtsObserved = pts;
	};


	auto ComputeReorderDelayHns = [&]() -> LONGLONG
	{
		// 固定“前视”延迟，用于吸收 B 帧/时间戳抖动。
		// 取 ~1.5 帧时长，限制在 5ms..150ms，避免引入过大 AV 延迟。
		double d = avgFrameDurationHns;
		if (!(d > 0.0)) d = HNS_PER_SEC / 60.0;
		LONGLONG w = (LONGLONG)std::llround(d * 1.5);
		w = (LONGLONG)std::clamp<double>((double)w, 50000.0, 150000.0);
		return w;
	};

	auto SelectVideoPts = [&](IMFSample* s, LONGLONG fallbackTs) -> LONGLONG
	{
		LONGLONG pts = fallbackTs;
		LONGLONG st = 0;
		bool hasSt = (s && SUCCEEDED(s->GetSampleTime(&st)));
		if (hasSt) pts = st;

		// 某些链路下 GetSampleTime 可能给的是 DTS/解码顺序时间。
		// 如果 DecodeTimestamp 存在且与 sampleTime 相同，而 ReadSample 的 ts 又不同，则更倾向使用 ReadSample ts。
		UINT64 dec = 0;
		bool hasDec = (s && SUCCEEDED(s->GetUINT64(MFSampleExtension_DecodeTimestamp, &dec)));
		if (hasSt && hasDec && (LONGLONG)dec == st && fallbackTs != st)
		{
			pts = fallbackTs;
		}

		if (s && debugVideoTsLogCount < kDebugVideoTsLogFirstN)
		{
			LONGLONG dur = 0;
			bool hasDur = (SUCCEEDED(s->GetSampleDuration(&dur)));
			wchar_t msg[512]{};
			swprintf_s(
				msg,
				L"[MediaPlayer TS] ts=%lld st=%s%lld dts=%s%llu dur=%s%lld -> pts=%lld\n",
				(long long)fallbackTs,
				hasSt ? L"" : L"(na)", (long long)(hasSt ? st : 0),
				hasDec ? L"" : L"(na)", (unsigned long long)(hasDec ? dec : 0),
				hasDur ? L"" : L"(na)", (long long)(hasDur ? dur : 0),
				(long long)pts);
			OutputDebugStringW(msg);
			debugVideoTsLogCount++;
		}

		return pts;
	};

	// 以 PTS（其次 seq）有序插入，确保输出稳定且按呈现时间。
	auto InsertPendingVideoSorted = [&](PendingVideoFrame&& f)
	{
		const auto it = std::upper_bound(
			pendingVideo.begin(), pendingVideo.end(), f,
			[](const PendingVideoFrame& a, const PendingVideoFrame& b)
			{
				if (a.pts != b.pts) return a.pts < b.pts;
				return a.seq < b.seq;
			});
		pendingVideo.insert(it, std::move(f));
	};

	while (!_threadExit)
	{
		{
			std::unique_lock lk(_threadMutex);
			_threadCv.wait(lk, [&] { return _threadExit || _threadPlaying.load(); });
			if (_threadExit) break;
		}

		firstVideoPts = -1;
		lastVideoPts = LLONG_MIN;
		lastPtsObserved = LLONG_MIN;
		avgFrameDurationHns = 0.0;
		maxVideoPtsSeen = LLONG_MIN;
		pendingVideo.clear();
		pendingSeq = 0;
		debugVideoTsLogCount = 0;
		triedRecoverInvalidStream = false;
		triedRecoverResetPipeline = false;

		if (_audioClient && _hasAudio)
			(void)_audioClient->Start();

		while (_threadPlaying && !_threadExit)
		{
			{
				const uint64_t currentSeekSerial = _seekSerial.load(std::memory_order_acquire);
				if (currentSeekSerial != lastSeekSerial)
				{
					lastSeekSerial = currentSeekSerial;
					activeSeekTargetHns = _seekTargetHns.load(std::memory_order_acquire);
					waitingFirstFrameAfterSeek = true;
					_needSyncReset = true;
					pendingVideo.clear();
					maxVideoPtsSeen = LLONG_MIN;
				}
			}
		if (_needSyncReset)
		{
			firstVideoPts = -1;
			lastVideoPts = LLONG_MIN;
			lastPtsObserved = LLONG_MIN;
			avgFrameDurationHns = 0.0;
			maxVideoPtsSeen = LLONG_MIN;
			pendingVideo.clear();
			if (_timeStretch) _timeStretch->Reset();
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
		HRESULT hr = _sourceReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &ts, &sample);
		if (FAILED(hr) && !triedRecoverInvalidStream && (hr == MF_E_INVALIDSTREAMNUMBER || hr == (HRESULT)0xC00D36B3))
		{
			triedRecoverInvalidStream = true;
			DebugOutputHr(L"SourceReader: ReadSample invalid-stream, retrying after reselection...", hr);
			if (_sourceReader)
			{
				(void)_sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
				if (_hasVideo) (void)_sourceReader->SetStreamSelection(_srVideoStream, TRUE);
				if (_hasAudio) (void)_sourceReader->SetStreamSelection(_srAudioStream, TRUE);
			}
			hr = _sourceReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &ts, &sample);
		}
		if (FAILED(hr) && !triedRecoverResetPipeline && _sourceReader)
		{
			triedRecoverResetPipeline = true;
			DebugOutputHr(L"SourceReader: ReadSample failed, attempting one-time pipeline reset...", hr);
			(void)_sourceReader->Flush(MF_SOURCE_READER_ALL_STREAMS);
			(void)_sourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
			if (_hasVideo) (void)_sourceReader->SetStreamSelection(_srVideoStream, TRUE);
			if (_hasAudio) (void)_sourceReader->SetStreamSelection(_srAudioStream, TRUE);

			if (_hasVideo)
				(void)ConfigureSourceReaderVideoType();
			if (_hasAudio && _audioMixFormat)
				(void)ConfigureSourceReaderAudioTypeFromMixFormat();

			PROPVARIANT var;
			PropVariantInit(&var);
			var.vt = VT_I8;
			var.hVal.QuadPart = (LONGLONG)std::llround(_position * HNS_PER_SEC);
			HRESULT hrSeek = _sourceReader->SetCurrentPosition(GUID_NULL, var);
			PropVariantClear(&var);
			if (FAILED(hrSeek))
			{
				// 若 reset 后仍无法定位，避免在后续循环里不断触发异常/错误。
				DebugOutputHr(L"SourceReader: pipeline reset SetCurrentPosition failed", hrSeek);
			}
			_needSyncReset = true;
			hr = _sourceReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &ts, &sample);
		}
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
				HRESULT hrFlush = _sourceReader->Flush(MF_SOURCE_READER_ALL_STREAMS);
				if (FAILED(hrFlush))
					DebugOutputHr(L"SourceReader: Flush on loop failed", hrFlush);
				if (_timeStretch) _timeStretch->Reset();
				PROPVARIANT var;
				PropVariantInit(&var);
				var.vt = VT_I8;
				var.hVal.QuadPart = 0;
				HRESULT hrSeek0 = _sourceReader->SetCurrentPosition(GUID_NULL, var);
				PropVariantClear(&var);
				if (FAILED(hrSeek0))
				{
					// 某些媒体源不支持 seek；禁用 loop，按正常 ended 结束，避免反复异常。
					DebugOutputHr(L"SourceReader: loop SetCurrentPosition(0) failed (loop disabled)", hrSeek0);
					_loop = false;
					_threadPlaying = false;
					_playState = PlayState::Stopped;
					OnMediaEnded(this);
					break;
				}

				// 关键：上一轮末尾缓存的 sampleTime 通常很大；新一轮从 0 开始会被判定为“时间倒退”而被丢弃。
				// 手动拖动进度条之所以能恢复，是因为 Seek() 会清空这些状态；loop 也必须做同样的清空。
				{
					std::scoped_lock lock(_textureMutex);
					_currentVideoSample.Reset();
					_currentVideoTexture.Reset();
					_currentVideoTextureFrameId = 0;
					_currentVideoTextureSampleTime = (std::numeric_limits<LONGLONG>::min)();
					_lastBgraConvertedFrameId = 0;
					_d2dVideoBitmap.Reset();
					_d2dVideoBitmapSourceTexture.Reset();
				}
				{
					std::scoped_lock lock(_videoFrameMutex);
					_videoFrameReady = false;
					_videoFrameSampleTime = (std::numeric_limits<LONGLONG>::min)();
					_videoFrame.clear();
				}
				this->PostRender();

				_needSyncReset = true;
				// Loop: 让下一轮尽快输出第一帧，避免第二遍开头长时间“停在最后一帧”。
				waitingFirstFrameAfterSeek = true;
				activeSeekTargetHns = 0;
				pendingVideo.clear();
				maxVideoPtsSeen = LLONG_MIN;
				_position = 0.0;
				OnPositionChanged(this, _position);
				OnMediaEnded(this);
				continue;
			}
			_threadPlaying = false;
			_playState = PlayState::Stopped;
			OnMediaEnded(this);
			break;
		}

		if (!sample) continue;
		GUID majorType{};
		if (_sourceReader)
		{
			ComPtr<IMFMediaType> mt;
			if (SUCCEEDED(_sourceReader->GetCurrentMediaType(streamIndex, &mt)) && mt)
				(void)mt->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
		}
		const bool isVideo = (majorType == MFMediaType_Video);
		const bool isAudio = (majorType == MFMediaType_Audio);

		// 注意：ReadSample 返回的 ts 在不同链路下可能并非 PTS。
		// 这里仅对视频样本做“呈现时间”选择/诊断输出，避免音频混入导致判断错误。
		LONGLONG pts = ts;
		if (isVideo)
		{
			pts = SelectVideoPts(sample.Get(), ts);
			UpdateAvgFrameDuration(pts, sample.Get());
		}

		// Seek 后快速丢弃目标之前的 preroll 视频帧，避免长时间堆积/等待导致画面“卡住”。
		if (isVideo && waitingFirstFrameAfterSeek && activeSeekTargetHns != (std::numeric_limits<LONGLONG>::min)())
		{
			// 容忍一定范围的“略早于目标”，避免永远等不到第一帧。
			static constexpr LONGLONG kSeekToleranceHns = 2500000; // 250ms
			if (pts + kSeekToleranceHns < activeSeekTargetHns)
				continue;
		}

		// 位置必须单调递增：SourceReader 的 ReadSample 可能跨流返回非时间戳顺序的 sample。
		// 如果直接用 ts 覆盖，会导致进度与画面出现“倒退”。
		{
			double newPos = (double)pts / HNS_PER_SEC;
			if (newPos > _position)
			{
				_position = newPos;
				OnPositionChanged(this, _position);
			}
		}

		if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0 && isVideo)
		{
			UpdateVideoFormatFromSourceReader();
		}

		if (isVideo)
		{
			_actualVideoStreamIndex = streamIndex;
			if (_videoSize.cx <= 0 || _videoSize.cy <= 0)
				UpdateVideoFormatFromSourceReader();

			bool textureProcessed = false;
			if (_useZeroCopy && _dxgiDeviceManager && _usingHardwareDecoding)
			{
				DWORD bufferCount = 0;
				HRESULT lastHr = sample->GetBufferCount(&bufferCount);
				if (SUCCEEDED(lastHr) && bufferCount > 0)
				{
					for (DWORD i = 0; i < bufferCount; i++)
					{
						ComPtr<IMFMediaBuffer> mediaBuffer;
						lastHr = sample->GetBufferByIndex(i, &mediaBuffer);
						if (FAILED(lastHr) || !mediaBuffer) continue;

						ComPtr<IMFDXGIBuffer> dxgiBuffer;
						lastHr = mediaBuffer.As(&dxgiBuffer);
						if (FAILED(lastHr) || !dxgiBuffer) continue;

						ComPtr<ID3D11Texture2D> texture;
						UINT subresourceIndex = 0;
						lastHr = dxgiBuffer->GetResource(__uuidof(ID3D11Texture2D), (void**)&texture);
						if (SUCCEEDED(lastHr) && texture)
						{
							HRESULT hrSub = dxgiBuffer->GetSubresourceIndex(&subresourceIndex);
							if (FAILED(hrSub))
								subresourceIndex = 0;
						}
						
						if (FAILED(lastHr) || !texture) continue;

						// 有些硬解码链路可能按“解码顺序”输出（B 帧），导致 PTS 非单调。
						// 这里先按 PTS 缓冲重排，再按正确顺序节拍/呈现，避免顺序错乱。
						PendingVideoFrame pv;
						pv.pts = pts;
						pv.subresourceIndex = subresourceIndex;
						pv.seq = ++pendingSeq;
						pv.sample = sample;
						pv.texture = texture;
						InsertPendingVideoSorted(std::move(pv));
						if (maxVideoPtsSeen == LLONG_MIN || pts > maxVideoPtsSeen)
							maxVideoPtsSeen = pts;
						textureProcessed = true;

						// Seek 后第一帧：跳过重排窗口，尽快输出一帧到 UI。
						if (waitingFirstFrameAfterSeek && !pendingVideo.empty())
						{
							PendingVideoFrame frame = std::move(pendingVideo.back());
							pendingVideo.clear();
							firstVideoPts = -1;
							lastVideoPts = LLONG_MIN;
							maxVideoPtsSeen = frame.pts;
							waitingFirstFrameAfterSeek = false;
							PaceVideoToPts(frame.pts);
							OnVideoFrameTexture(frame.texture.Get(), frame.subresourceIndex, frame.pts, frame.sample.Get());
							break;
						}

						// 恒定前视重排：当最早帧已经“足够早于”maxPtsSeen 时就输出它。
						// 这样既能纠正 B 帧/抖动，又避免过大缓存导致 AV 不同步。
						static constexpr size_t kMaxReorderFrames = 32;
						const LONGLONG reorderDelay = ComputeReorderDelayHns();
						for (;;)
						{
							if (pendingVideo.empty()) break;
							const LONGLONG minPts = pendingVideo.front().pts;
							const bool ready = (maxVideoPtsSeen != LLONG_MIN) && (minPts <= (maxVideoPtsSeen - reorderDelay));
							const bool tooMany = pendingVideo.size() > kMaxReorderFrames;
							if (!ready && !tooMany)
								
								break;

							PendingVideoFrame frame = std::move(pendingVideo.front());
							pendingVideo.erase(pendingVideo.begin());

							if (lastVideoPts != LLONG_MIN && frame.pts < lastVideoPts)
							{
								// 小幅回退更像时间戳量化/抖动：夹紧到 lastVideoPts 而非丢帧。
								const LONGLONG back = lastVideoPts - frame.pts;
								if (back <= (VIDEO_TS_REORDER_TOLERANCE_HNS * 6))
								{
									frame.pts = lastVideoPts;
								}
								else
								{
									// 大幅回退通常是 seek/discontinuity：重置节拍基准而不是持续乱序。
									firstVideoPts = frame.pts;
									QueryPerformanceCounter(&startQpc);
									lastVideoPts = frame.pts;
									PaceVideoToPts(frame.pts);
									OnVideoFrameTexture(frame.texture.Get(), frame.subresourceIndex, frame.pts, frame.sample.Get());
									continue;
								}
							}
							lastVideoPts = frame.pts;

							PaceVideoToPts(frame.pts);
							OnVideoFrameTexture(frame.texture.Get(), frame.subresourceIndex, frame.pts, frame.sample.Get());
						}

						static bool firstSuccess = true;
						if (firstSuccess)
						{
							D3D11_TEXTURE2D_DESC desc;
							texture->GetDesc(&desc);
							wchar_t msg[256];
							swprintf_s(msg, L"Zero-copy GPU texture obtained: %dx%d, Format: %d, Index: %u", desc.Width, desc.Height, desc.Format, subresourceIndex);
							DebugOutputHr(msg, S_OK);
							firstSuccess = false;
						}
						break;
					}
				}

				if (!textureProcessed)
				{
					static bool warnedOnce = false;
					if (!warnedOnce)
					{
						DebugOutputHr(L"Zero-copy: Failed to extract GPU texture from sample (no IMFDXGIBuffer), falling back to CPU", lastHr);
						warnedOnce = true;
					}
				}
			}

			if (textureProcessed) continue;

			// 非零拷贝路径（CPU / RGB）仍按当前帧 PTS 节拍并确保单调。
			PaceVideoToPts(pts);
			if (lastVideoPts != LLONG_MIN && pts < lastVideoPts)
				continue;
			lastVideoPts = pts;
		}

		ComPtr<IMFMediaBuffer> buf;
		hr = sample->ConvertToContiguousBuffer(&buf);
		if (FAILED(hr) || !buf) continue;
		BYTE* p = nullptr;
		DWORD maxLen = 0, curLen = 0;
		hr = buf->Lock(&p, &maxLen, &curLen);
		if (FAILED(hr) || !p || curLen == 0) continue;

		if (isVideo)
		{
				const LONG frameW = _videoFrameSize.cx;
				const LONG frameH = _videoFrameSize.cy;
				const LONG w = _videoSize.cx;
				const LONG h = _videoSize.cy;
				const UINT32 cropX = _videoCropX;
				const UINT32 cropY = _videoCropY;
				if (frameW > 0 && frameH > 0 && w > 0 && h > 0)
				{
					UINT32 srcStride = 0;
					UINT32 bpp = 4;
					bool bottomUp = false;
					{
						std::scoped_lock lock(_videoFrameMutex);
						srcStride = _videoStride;
						bpp = (_videoBytesPerPixel == 0) ? 4 : _videoBytesPerPixel;
						bottomUp = _videoBottomUp;
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
							_videoStride = (UINT32)w * 4;
							_videoFrameReady = true;
							_videoFrameSampleTime = pts;
						}
						this->PostRender();
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

	// 尽量复用 CppUtils/Graphics1 的共享 D3D11 设备。
	// 这可以保证 D2D 的 DeviceContext 与视频纹理属于同一个 DXGI device，
	// 从而让 CreateBitmapFromDxgiSurface（零拷贝）可用。
	_d3dDevice.Reset();
	_d3dContext.Reset();
	if (SUCCEEDED(Graphics1_EnsureSharedD3DDevice()))
	{
		if (auto* sharedDev = Graphics1_GetSharedD3DDevice())
		{
			_d3dDevice = sharedDev;
			sharedDev->GetImmediateContext(&_d3dContext);
			DebugOutputHr(L"D3D11: Using shared Graphics1 device", S_OK);
		}
	}
	if (_d3dDevice && _d3dContext)
	{
		hr = S_OK;
	}
	else
	{

		// 创建独立 D3D11 设备（关键：添加 VIDEO_SUPPORT 标志以启用硬件视频解码）
		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
		D3D_FEATURE_LEVEL featureLevel;
		UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;  // BGRA互操作 + 视频解码支持

#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			createDeviceFlags,
			featureLevels,
			4,
			D3D11_SDK_VERSION,
			&_d3dDevice,
			&featureLevel,
			&_d3dContext
		);

		if (FAILED(hr))
		{
			DebugOutputHr(L"D3D11: Hardware device creation failed, trying WARP", hr);
			// 如果硬件加速失败，尝试 WARP（但WARP不支持DXVA，需要移除VIDEO_SUPPORT标志）
			createDeviceFlags &= ~D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
			hr = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				0,
				createDeviceFlags,
				featureLevels,
				4,
				D3D11_SDK_VERSION,
				&_d3dDevice,
				&featureLevel,
				&_d3dContext
			);
			if (SUCCEEDED(hr))
			{
				_useZeroCopy = false;  // WARP不支持零拷贝
				DebugOutputHr(L"D3D11: WARP device created (zero-copy disabled)", S_OK);
			}
		}
		else
		{
			DebugOutputHr(L"D3D11: Hardware device created with VIDEO_SUPPORT", S_OK);
		}
	}

	// 初始化 D3D11 Video Processor 接口（用于 NV12/P010/YUY2 -> BGRA 转换，供 D2D 渲染）
	_videoDevice.Reset();
	_videoContext.Reset();
	_videoProcessorEnum.Reset();
	_videoProcessor.Reset();
	_vpWidth = 0;
	_vpHeight = 0;
	_vpInputFormat = DXGI_FORMAT_UNKNOWN;
	if (SUCCEEDED(hr) && _d3dDevice && _d3dContext)
	{
		(void)_d3dDevice.As(&_videoDevice);
		(void)_d3dContext.As(&_videoContext);

		// 创建 GPU 同步查询对象
		D3D11_QUERY_DESC queryDesc{};
		queryDesc.Query = D3D11_QUERY_EVENT;
		queryDesc.MiscFlags = 0;
		if (FAILED(_d3dDevice->CreateQuery(&queryDesc, &_videoGpuSyncQuery)))
		{
			_videoGpuSyncQuery.Reset();
		}
	}

	return hr;
}

HRESULT MediaPlayer::InitializeDXGIDeviceManager()
{
	if (!_d3dDevice) return E_POINTER;

	HRESULT hr = S_OK;

	// 创建 DXGI Device Manager（零拷贝关键组件）
	hr = MFCreateDXGIDeviceManager(&_dxgiResetToken, &_dxgiDeviceManager);
	if (FAILED(hr))
	{
		DebugOutputHr(L"MFCreateDXGIDeviceManager failed", hr);
		return hr;
	}

	// 将 D3D11 设备注册到 Device Manager
	hr = _dxgiDeviceManager->ResetDevice(_d3dDevice.Get(), _dxgiResetToken);
	if (FAILED(hr))
	{
		DebugOutputHr(L"DXGI ResetDevice failed", hr);
		_dxgiDeviceManager.Reset();
		return hr;
	}

	return S_OK;
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
	return S_OK;
}

void MediaPlayer::OnVideoFrame(const BYTE* data, DWORD size, LONGLONG sampleTime)
{
	if (!data || size == 0) return;
	if (_videoSize.cx <= 0 || _videoSize.cy <= 0) return;
	if (_videoFrameSize.cx <= 0 || _videoFrameSize.cy <= 0) return;

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
		if (sampleTime != (std::numeric_limits<LONGLONG>::min)() &&
			_videoFrameSampleTime != (std::numeric_limits<LONGLONG>::min)() &&
			(sampleTime + VIDEO_TS_REORDER_TOLERANCE_HNS) < _videoFrameSampleTime)
		{
			return;
		}
		_videoStride = expectedStride;
		_videoFrame = std::move(normalized);
		_videoFrameReady = true;
		_videoFrameSampleTime = sampleTime;
	}
	// CUI 是完全自渲染框架：需要主动 Invalidate 才会刷新画面。
	this->PostRender();
}

void MediaPlayer::OnVideoFrameTexture(ID3D11Texture2D* texture, UINT subresourceIndex, LONGLONG sampleTime, IMFSample* sample)
{
	if (!texture || !_d3dDevice) return;

	// 注意：IMFDXGIBuffer::GetSubresourceIndex 返回的是 D3D11 subresource index（mip + array slice）。
	// VideoProcessor InputView 需要的是 ArraySlice；这里将 subresourceIndex 转换为 array slice。
	D3D11_TEXTURE2D_DESC desc{};
	texture->GetDesc(&desc);
	UINT mipLevels = desc.MipLevels ? desc.MipLevels : 1;
	UINT arraySlice = subresourceIndex / mipLevels;
	if (desc.ArraySize > 0 && arraySlice >= desc.ArraySize)
	{
		static bool warnedOnce = false;
		if (!warnedOnce)
		{
			wchar_t msg[256];
			swprintf_s(msg, L"Zero-copy: subresourceIndex->ArraySlice out of range (sub=%u, mip=%u, slice=%u, array=%u); fallback to 0", subresourceIndex, mipLevels, arraySlice, desc.ArraySize);
			DebugOutputHr(msg, S_OK);
			warnedOnce = true;
		}
		arraySlice = 0;
	}
	if (arraySlice > 0xFF)
	{
		static bool warnedOnce = false;
		if (!warnedOnce)
		{
			wchar_t msg[256];
			swprintf_s(msg, L"Zero-copy: ArraySlice=%u cannot be encoded in frameId high 8 bits; fallback to 0", arraySlice);
			DebugOutputHr(msg, S_OK);
			warnedOnce = true;
		}
		arraySlice = 0;
	}

	const UINT64 frameId = _frameCounter.fetch_add(1, std::memory_order_relaxed) + 1;
	_statDecodedFrames.fetch_add(1, std::memory_order_relaxed);
	const UINT64 encodedFrameId = (frameId & 0x00FFFFFFFFFFFFFF) | ((UINT64)arraySlice << 56);

	// 零拷贝路径：直接使用解码器输出的 D3D11 纹理
	{
		std::scoped_lock lock(_textureMutex);
		if (sampleTime != (std::numeric_limits<LONGLONG>::min)() &&
			_currentVideoTextureSampleTime != (std::numeric_limits<LONGLONG>::min)() &&
			(sampleTime + VIDEO_TS_REORDER_TOLERANCE_HNS) < _currentVideoTextureSampleTime)
		{
			return;
		}
		// 保持 sample 存活，避免解码器过早复用底层 surface 造成“内容与 PTS 不匹配”。
		_currentVideoSample.Reset();
		if (sample)
			_currentVideoSample = sample;
		// 显式管理引用计数：AddRef 后 Attach，确保恰好持有 1 个引用。
		texture->AddRef();
		_currentVideoTexture.Attach(texture);
		_currentVideoTextureFrameId = encodedFrameId;
		_currentVideoTextureSampleTime = sampleTime;
	}
}

ID3D11Texture2D* MediaPlayer::ConvertTextureToBgraForD2D(ID3D11Texture2D* srcTexture, UINT64 sharedFrameId)
{
	if (!srcTexture || !_d3dDevice || !_d3dContext) return nullptr;
	if (!_videoDevice || !_videoContext) return nullptr;

	D3D11_TEXTURE2D_DESC srcDesc{};
	srcTexture->GetDesc(&srcDesc);
	if (srcDesc.Width == 0 || srcDesc.Height == 0) return nullptr;

	if (srcDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
	{
		srcTexture->AddRef();
		return srcTexture;
	}

	const bool isYuv =
		srcDesc.Format == DXGI_FORMAT_NV12 ||
		srcDesc.Format == DXGI_FORMAT_P010 ||
		srcDesc.Format == DXGI_FORMAT_YUY2 ||
		srcDesc.Format == DXGI_FORMAT_420_OPAQUE;
	if (!isYuv) return nullptr;

	UINT arraySlice = (UINT)(sharedFrameId >> 56);
	if (srcDesc.ArraySize > 0 && arraySlice >= srcDesc.ArraySize)
		arraySlice = 0;
	if (!_videoProcessorEnum || !_videoProcessor || _vpWidth != srcDesc.Width || _vpHeight != srcDesc.Height || _vpInputFormat != srcDesc.Format)
	{
		_videoProcessor.Reset();
		_videoProcessorEnum.Reset();
		_vpWidth = srcDesc.Width;
		_vpHeight = srcDesc.Height;
		_vpInputFormat = srcDesc.Format;

		D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc{};
		contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
		contentDesc.InputWidth = srcDesc.Width;
		contentDesc.InputHeight = srcDesc.Height;
		contentDesc.OutputWidth = srcDesc.Width;
		contentDesc.OutputHeight = srcDesc.Height;
		contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		HRESULT hr = _videoDevice->CreateVideoProcessorEnumerator(&contentDesc, &_videoProcessorEnum);
		if (FAILED(hr) || !_videoProcessorEnum)
		{
			DebugOutputHr(L"VideoProcessor: CreateVideoProcessorEnumerator failed", hr);
			return nullptr;
		}

		hr = _videoDevice->CreateVideoProcessor(_videoProcessorEnum.Get(), 0, &_videoProcessor);
		if (FAILED(hr) || !_videoProcessor)
		{
			DebugOutputHr(L"VideoProcessor: CreateVideoProcessor failed", hr);
			return nullptr;
		}
	}

	ComPtr<ID3D11Texture2D> outTex;
	bool createdNewOutput = true;

	ID3D11Texture2D* pooled = AcquireTextureFromPool(srcDesc.Width, srcDesc.Height);
	if (!pooled) return nullptr;
	outTex.Attach(pooled);

	ComPtr<ID3D11VideoProcessorOutputView> outView;
	{
		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ov{};
		ov.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
		ov.Texture2D.MipSlice = 0;
		HRESULT hr = _videoDevice->CreateVideoProcessorOutputView(outTex.Get(), _videoProcessorEnum.Get(), &ov, &outView);
		if (FAILED(hr) || !outView)
		{
			DebugOutputHr(L"VideoProcessor: CreateVideoProcessorOutputView failed", hr);
			ReleaseTextureToPool(outTex.Get());
			return nullptr;
		}
	}

	ComPtr<ID3D11VideoProcessorInputView> inView;
	{
		D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv{};
		iv.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
		iv.Texture2D.MipSlice = 0;
		iv.Texture2D.ArraySlice = arraySlice;
		HRESULT hr = _videoDevice->CreateVideoProcessorInputView(srcTexture, _videoProcessorEnum.Get(), &iv, &inView);
		if (FAILED(hr) || !inView)
		{
			DebugOutputHr(L"VideoProcessor: CreateVideoProcessorInputView failed", hr);
			ReleaseTextureToPool(outTex.Get());
			return nullptr;
		}
	}

	D3D11_VIDEO_PROCESSOR_STREAM stream{};
	stream.Enable = TRUE;
	stream.pInputSurface = inView.Get();

	LARGE_INTEGER __qpcBefore{};
	const BOOL __hasBefore = QueryPerformanceCounter(&__qpcBefore);
	HRESULT hr = _videoContext->VideoProcessorBlt(_videoProcessor.Get(), outView.Get(), 0, 1, &stream);
	LARGE_INTEGER __qpcAfter{};
	if (__hasBefore && QueryPerformanceCounter(&__qpcAfter))
	{
		_statYuvToBgraCalls.fetch_add(1, std::memory_order_relaxed);
		_statYuvToBgraQpcTicks.fetch_add((UINT64)(__qpcAfter.QuadPart - __qpcBefore.QuadPart), std::memory_order_relaxed);
	}
	if (FAILED(hr))
	{
		DebugOutputHr(L"VideoProcessor: VideoProcessorBlt failed", hr);
		ReleaseTextureToPool(outTex.Get());
		return nullptr;
	}

	if (_forceVideoGpuSync)
	{
		_d3dContext->Flush();
	}

	if (_currentVideoTextureBgra && _currentVideoTextureBgraFromPool)
	{
		if (_currentVideoTextureBgra.Get() != outTex.Get())
		{
			ReleaseTextureToPool(_currentVideoTextureBgra.Get());
		}
	}

	if (!_currentVideoTextureBgra || _currentVideoTextureBgra.Get() != outTex.Get())
	{
		_currentVideoTextureBgra = outTex;
		_currentVideoTextureBgraFromPool = true;
		_currentVideoTextureBgraFrame = _frameCounter.load(std::memory_order_relaxed);
	}
	if (createdNewOutput)
	{
		_d2dVideoBitmap.Reset();
		_d2dVideoBitmapSourceTexture.Reset();
	}

	return outTex.Detach();
}

ID3D11Texture2D* MediaPlayer::AcquireTextureFromPool(UINT width, UINT height)
{
	if (!_d3dDevice || width == 0 || height == 0) return nullptr;
	
	std::scoped_lock lock(_texturePoolMutex);
	
	for (auto& entry : _texturePool)
	{
		if (!entry.inUse && entry.texture)
		{
			D3D11_TEXTURE2D_DESC desc;
			entry.texture->GetDesc(&desc);
			if (desc.Width == width && desc.Height == height)
			{
				entry.inUse = true;
				entry.lastUsedFrame = _frameCounter.load(std::memory_order_relaxed);
				entry.texture->AddRef();
				return entry.texture.Get();
			}
		}
	}
	
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.MiscFlags = 0;  // 同一 D3D11 device 上 D2D 可直接从 DXGI surface 读取，无需 shared 纹理
	
	ComPtr<ID3D11Texture2D> newTexture;
	HRESULT hr = _d3dDevice->CreateTexture2D(&desc, nullptr, &newTexture);
	if (FAILED(hr))
	{
		DebugOutputHr(L"CreateTexture2D for pool failed", hr);
		return nullptr;
	}
	
	if (_texturePool.size() < MAX_TEXTURE_POOL_SIZE)
	{
		TexturePoolEntry entry;
		entry.texture = newTexture;
		entry.inUse = true;
		entry.lastUsedFrame = _frameCounter.load(std::memory_order_relaxed);
		_texturePool.push_back(entry);
	}
	
	newTexture->AddRef();
	return newTexture.Get();
}

void MediaPlayer::ReleaseTextureToPool(ID3D11Texture2D* texture)
{
	if (!texture) return;
	
	std::scoped_lock lock(_texturePoolMutex);
	
	for (auto& entry : _texturePool)
	{
		if (entry.texture.Get() == texture)
		{
			entry.inUse = false;
			return;
		}
	}
}

void MediaPlayer::CleanupTexturePool()
{
	std::scoped_lock lock(_texturePoolMutex);
	
	const UINT64 now = _frameCounter.load(std::memory_order_relaxed);
	const UINT64 threshold = (now > 60) ? (now - 60) : 0;
	
	_texturePool.erase(
		std::remove_if(_texturePool.begin(), _texturePool.end(),
			[threshold](const TexturePoolEntry& entry) {
				return !entry.inUse && entry.lastUsedFrame < threshold;
			}),
		_texturePool.end()
	);
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

	const bool switchingWhileActive = (_useSourceReader && (_playThread.joinable() || _threadPlaying.load() || _sourceReader));
	if (switchingWhileActive)
	{
		// 关键：播放中切换文件时，Stop+join+MFCreateSourceReaderFromURL 可能会同步卡住 UI。
		// 这里改为：UI 线程只清状态并投递请求，后台线程串行执行 stop+open。
		EnsureOpenWorker();
		_openSerial.fetch_add(1, std::memory_order_acq_rel);
		{
			std::scoped_lock lk(_openMutex);
			_openRequestFile = mediaFile;
			_openHasRequest = true;
		}
		_openCv.notify_one();

		// 立即停止播放与清空旧画面（用户感知：立刻响应，而不是卡住）。
		_threadPlaying = false;
		_needSyncReset = true;
		if (_audioClient) (void)_audioClient->Stop();
		_mediaFile = mediaFile;
		_mediaLoaded = false;
		_position = 0.0;
		_duration = 0.0;
		_hasVideo = false;
		_hasAudio = false;
		_videoSize = SIZE{ 0,0 };
		_playState = PlayState::Stopped;
		{
			std::scoped_lock lock(_videoFrameMutex);
			_videoFrame.clear();
			_videoFrameReady = false;
			_videoStride = 0;
			_videoSubtype = GUID_NULL;
			_videoBytesPerPixel = 4;
			_videoBottomUp = false;
			_videoFrameSize = SIZE{ 0,0 };
			_videoCropX = 0;
			_videoCropY = 0;
		}
		{
			std::scoped_lock lock(_textureMutex);
			_currentVideoSample.Reset();
			_currentVideoTexture.Reset();
			_currentVideoTextureFrameId = 0;
			_currentVideoTextureSampleTime = (std::numeric_limits<LONGLONG>::min)();
			_lastBgraConvertedFrameId = 0;
			_d2dVideoBitmap.Reset();
			_d2dVideoBitmapSourceTexture.Reset();
		}
		if (_videoBitmap && _ownsVideoBitmap)
			_videoBitmap->Release();
		_videoBitmap = nullptr;
		_ownsVideoBitmap = false;
		PostRender();
		return true;
	}

	if (_playThread.joinable() || _threadPlaying.load() || _sourceReader)
	{
		StopSourceReaderPlayback(true);
	}

	if (_useSourceReader)
	{
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

		if (_autoPlay)
			Play();
		return true;
	}

	if (FAILED(CreateMediaSession())) return false;

	HRESULT hr = S_OK;
	_mediaFile = mediaFile;
	_mediaLoaded = false;
	_topologyReady = false;
	_pendingStart = false;
	_hasPendingStartPosition = false;
	_pendingStartPosition = 0.0;

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
		this->Position = this->Position; // 触发 OnPositionChanged
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

		// 发布 seek 目标：播放线程会丢弃目标之前的 preroll 帧，并在命中目标时立刻重置节拍基准。
		const LONGLONG targetHns = (LONGLONG)std::llround(seconds * HNS_PER_SEC);
		_seekTargetHns.store(targetHns, std::memory_order_release);
		_seekSerial.fetch_add(1, std::memory_order_acq_rel);
		_needSyncReset = true;

		// 清空旧画面（避免 UI 长时间停留在上一帧造成“卡住”错觉）。
		{
			std::scoped_lock lock(_textureMutex);
			_currentVideoSample.Reset();
			_currentVideoTexture.Reset();
			_currentVideoTextureFrameId = 0;
			_currentVideoTextureSampleTime = (std::numeric_limits<LONGLONG>::min)();
			_lastBgraConvertedFrameId = 0;
			_d2dVideoBitmap.Reset();
			_d2dVideoBitmapSourceTexture.Reset();
		}
		{
			std::scoped_lock lock(_videoFrameMutex);
			_videoFrameReady = false;
			_videoFrameSampleTime = (std::numeric_limits<LONGLONG>::min)();
			_videoFrame.clear();
		}

		// Flush：丢弃 SourceReader 内部已排队的旧 sample（Seek 后否则会继续吐旧帧一段时间）。
		(void)_sourceReader->Flush(MF_SOURCE_READER_ALL_STREAMS);
		PROPVARIANT var;
		PropVariantInit(&var);
		var.vt = VT_I8;
		var.hVal.QuadPart = targetHns;
		HRESULT hr = _sourceReader->SetCurrentPosition(GUID_NULL, var);
		PropVariantClear(&var);
		if (FAILED(hr))
			DebugOutputHr(L"SourceReader: SetCurrentPosition failed", hr);
		_position = seconds;
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
	
	// 释放零拷贝资源
	{
		std::scoped_lock lock(_textureMutex);
		_currentVideoSample.Reset();
		_currentVideoTexture.Reset();
		_currentVideoTextureFrameId = 0;
		_currentVideoTextureSampleTime = (std::numeric_limits<LONGLONG>::min)();
		if (_currentVideoTextureBgra && _currentVideoTextureBgraFromPool)
		{
			ReleaseTextureToPool(_currentVideoTextureBgra.Get());
		}
		_currentVideoTextureBgra.Reset();
		_currentVideoTextureBgraFromPool = false;
		_currentVideoTextureBgraFrame = 0;
		_lastBgraConvertedFrameId = 0;
		_d2dVideoBitmap.Reset();
		_d2dVideoBitmapSourceTexture.Reset();
	}
	
	{
		std::scoped_lock lock(_videoFrameMutex);
		_videoFrame.clear();
		_videoStride = 0;
		_videoFrameReady = false;
		_videoFrameSampleTime = (std::numeric_limits<LONGLONG>::min)();
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

	// 诊断：每秒输出一次 FPS/耗时统计（尽量低开销）
	auto MaybeReportPerf = [this]()
	{
		LARGE_INTEGER freq{};
		if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) return;
		LARGE_INTEGER now{};
		if (!QueryPerformanceCounter(&now)) return;

		LONGLONG last = _statLastReportQpc.load(std::memory_order_relaxed);
		if (last == 0)
		{
			_statLastReportQpc.store(now.QuadPart, std::memory_order_relaxed);
			return;
		}

		const double elapsedSec = double(now.QuadPart - last) / double(freq.QuadPart);
		if (elapsedSec < 1.0) return;

		// 取并清零本周期计数
		const UINT64 decoded = _statDecodedFrames.exchange(0, std::memory_order_relaxed);
		const UINT64 updates = _statRenderUpdates.exchange(0, std::memory_order_relaxed);
		const UINT64 y2bCalls = _statYuvToBgraCalls.exchange(0, std::memory_order_relaxed);
		const UINT64 y2bTicks = _statYuvToBgraQpcTicks.exchange(0, std::memory_order_relaxed);
		const UINT64 bmpCalls = _statCreateD2DBitmapCalls.exchange(0, std::memory_order_relaxed);
		const UINT64 bmpTicks = _statCreateD2DBitmapQpcTicks.exchange(0, std::memory_order_relaxed);

		double y2bMsTotal = (y2bTicks ? (double(y2bTicks) * 1000.0 / double(freq.QuadPart)) : 0.0);
		double bmpMsTotal = (bmpTicks ? (double(bmpTicks) * 1000.0 / double(freq.QuadPart)) : 0.0);
		double y2bMsAvg = (y2bCalls ? (y2bMsTotal / double(y2bCalls)) : 0.0);
		double bmpMsAvg = (bmpCalls ? (bmpMsTotal / double(bmpCalls)) : 0.0);

		wchar_t buf[512]{};
		swprintf_s(
			buf,
			L"[MediaPlayer Perf] decoded=%.1ffps, update=%.1ffps, yuv2bgra=%llu (%.2fms avg, %.1fms/s), d2dbitmap=%llu (%.2fms avg, %.1fms/s)\n",
			double(decoded) / elapsedSec,
			double(updates) / elapsedSec,
			(unsigned long long)y2bCalls,
			y2bMsAvg,
			y2bMsTotal / elapsedSec,
			(unsigned long long)bmpCalls,
			bmpMsAvg,
			bmpMsTotal / elapsedSec);
		OutputDebugStringW(buf);
				_videoFrameSampleTime = (std::numeric_limits<LONGLONG>::min)();

		_statLastReportQpc.store(now.QuadPart, std::memory_order_relaxed);
	};

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
	
	// 定期清理纹理池（每60帧清理一次）
	const UINT64 frameCounter = _frameCounter.load(std::memory_order_relaxed);
	if (_useZeroCopy && (frameCounter % 60 == 0))
	{
		CleanupTexturePool();
	}

	// 背景
	d2d->FillRect(abs.x, abs.y, size.cx, size.cy, this->BackColor);

	// 有视频：尝试更新并绘制最新帧
	if (_hasVideo && _mediaLoaded)
	{
		_statRenderUpdates.fetch_add(1, std::memory_order_relaxed);
		MaybeReportPerf();

		// ========== 零拷贝GPU路径 ==========
		ComPtr<ID3D11Texture2D> currentTexture;
		ComPtr<IMFSample> currentSample;
		UINT64 currentTextureFrameId = 0;
		{
			std::scoped_lock lock(_textureMutex);
			currentTexture = _currentVideoTexture;
			currentSample = _currentVideoSample;
			currentTextureFrameId = _currentVideoTextureFrameId;
		}
		if (_useZeroCopy && currentTexture)
		{
			// 若解码器输出为 NV12/P010/YUY2，先转成 BGRA 纹理（仍在 GPU 上），再交给 D2D 绘制。
			ComPtr<ID3D11Texture2D> textureForD2D;
			textureForD2D = currentTexture;
			D3D11_TEXTURE2D_DESC srcDesc{};
			currentTexture->GetDesc(&srcDesc);
			const bool isYuv =
				srcDesc.Format == DXGI_FORMAT_NV12 ||
				srcDesc.Format == DXGI_FORMAT_P010 ||
				srcDesc.Format == DXGI_FORMAT_YUY2 ||
				srcDesc.Format == DXGI_FORMAT_420_OPAQUE;
			if (isYuv)
			{
				// 避免在同一帧上重复做 YUV->BGRA（CUI 可能会在无新帧时多次 Update/重绘）
				if (currentTextureFrameId != 0 && _lastBgraConvertedFrameId == currentTextureFrameId && _currentVideoTextureBgra)
				{
					textureForD2D = _currentVideoTextureBgra;
				}
				else
				{
					// 必须传入 currentTextureFrameId，因为其中包含了 subresource index，
					// 且必须使用锁内获取的快照值，防止与后台线程产生竞争。
					ID3D11Texture2D* bgra = ConvertTextureToBgraForD2D(currentTexture.Get(), currentTextureFrameId);
					if (bgra)
					{
						textureForD2D.Attach(bgra);
						_lastBgraConvertedFrameId = currentTextureFrameId;
						// YUV->BGRA 输出纹理可复用：尽量不要每帧销毁 bitmap。
					}
					else
					{
						static bool warnedOnce = false;
						if (!warnedOnce)
						{
							DebugOutputHr(L"Zero-copy: YUV->BGRA conversion failed, falling back to CPU", E_FAIL);
							warnedOnce = true;
						}
						_useZeroCopy = false;
					}
				}
			}
			
			auto rt = d2d->GetRenderTargetRaw();
			if (rt)
			{
				// 尝试获取 ID2D1DeviceContext 以支持 D3D11 互操作
				ComPtr<ID2D1DeviceContext> d2dContext;
				if (SUCCEEDED(rt->QueryInterface(__uuidof(ID2D1DeviceContext), (void**)&d2dContext)))
				{
					// 如果 D2D 位图不存在或源纹理变化，则创建共享位图
					// 如果启用了 _recreateD2DBitmapEveryFrame，则强制每帧重建（兼容性）
					if (_recreateD2DBitmapEveryFrame || !_d2dVideoBitmap || !_d2dVideoBitmapSourceTexture || _d2dVideoBitmapSourceTexture.Get() != textureForD2D.Get())
					{
						_d2dVideoBitmap.Reset();
						_d2dVideoBitmapSourceTexture.Reset();
						// 获取纹理的 DXGI Surface
						ComPtr<IDXGISurface> dxgiSurface;
						if (textureForD2D && SUCCEEDED(textureForD2D->QueryInterface(__uuidof(IDXGISurface), (void**)&dxgiSurface)))
						{
							// 我们在这里保证传给 D2D 的一定是 BGRA 纹理。
							DXGI_FORMAT d2dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
							D2D1_ALPHA_MODE alphaMode = D2D1_ALPHA_MODE_IGNORE;
							
							// 从 DXGI Surface 创建 D2D 位图（零拷贝！）
							D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
								D2D1_BITMAP_OPTIONS_NONE,
								D2D1::PixelFormat(d2dFormat, alphaMode)
							);
							
							LARGE_INTEGER __qpcBefore{};
							const BOOL __hasBefore = QueryPerformanceCounter(&__qpcBefore);
							HRESULT hr = d2dContext->CreateBitmapFromDxgiSurface(
								dxgiSurface.Get(),
								&bitmapProperties,
								&_d2dVideoBitmap
							);
							LARGE_INTEGER __qpcAfter{};
							if (__hasBefore && QueryPerformanceCounter(&__qpcAfter))
							{
								_statCreateD2DBitmapCalls.fetch_add(1, std::memory_order_relaxed);
								_statCreateD2DBitmapQpcTicks.fetch_add((UINT64)(__qpcAfter.QuadPart - __qpcBefore.QuadPart), std::memory_order_relaxed);
							}
							
							if (FAILED(hr))
							{
								static bool warnedOnce = false;
								if (!warnedOnce)
								{
									DebugOutputHr(L"CreateBitmapFromDxgiSurface failed", hr);
									warnedOnce = true;
								}
								_useZeroCopy = false;  // 降级到CPU路径
							}
							else
							{
								_d2dVideoBitmapSourceTexture = textureForD2D;
							}
						}
					}
					
					if (_d2dVideoBitmap)
					{
						// 计算渲染矩形
						float destX = (float)abs.x;
						float destY = (float)abs.y;
						float destWidth = (float)size.cx;
						float destHeight = (float)size.cy;
						
						float videoWidth = (float)_videoSize.cx;
						float videoHeight = (float)_videoSize.cy;
						
						switch (_renderMode)
						{
						case VideoRenderMode::Fit:
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
						case VideoRenderMode::Fill:
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
						case VideoRenderMode::Center:
							{
								destX += (destWidth - videoWidth) * 0.5f;
								destY += (destHeight - videoHeight) * 0.5f;
								destWidth = videoWidth;
								destHeight = videoHeight;
							}
							break;
						case VideoRenderMode::UniformToFill:
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
						case VideoRenderMode::Stretch:
						default:
							break;
						}
						
						// 零拷贝绘制：直接从GPU纹理渲染，无CPU内存拷贝！
						D2D1_RECT_F destRect = D2D1::RectF(destX, destY, destX + destWidth, destY + destHeight);
						d2dContext->DrawBitmap(_d2dVideoBitmap.Get(), destRect);
						return;
					}
				}
			}
		}
		
		// ========== CPU后备路径（兼容性）==========
		std::vector<uint8_t> frame;
		UINT32 stride = 0;
		{
			std::scoped_lock lock(_videoFrameMutex);
			if (_videoFrameReady)
			{
				frame = _videoFrame;
				stride = _videoStride;
			}
		}

		if (!frame.empty() && _videoSize.cx > 0 && _videoSize.cy > 0 && stride > 0)
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
				
				d2d->DrawBitmap(_videoBitmap, destX, destY, destWidth, destHeight);
				return;
			}
		}
	}
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
