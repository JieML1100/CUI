#include "MediaElement.h"
#include "EventInfrastructure.h"
#include "Window.h"
#include "Core/Threading.h"
#include "Graphics.h"
#include <d2d1helper.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <mferror.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <cstdio>
#include <limits>
#include <utility>

#include <atomic>

// ============================================================================
// MediaElement - Windows 原生媒体播放器控件实现
// ============================================================================
// 基于 Windows Media Foundation 实现的高性能媒体播放器
// 支持以下功能：
// - 视频和音频播放
// - 播放控制：播放、暂停、停止、进度跳转

// 常量定义
static constexpr double HNS_PER_SEC = 10000000.0;  // 100-nanosecond 单位与秒的转换
static const GUID kMFAudioFormatMpegHeaac = { 0x00001610, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

static void PrintLogWide(const wchar_t* text)
{
	if (!text) return;
	OutputDebugStringW(text);
	const int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	if (len <= 0) return;
	std::string utf8((size_t)len, '\0');
	if (WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), len, nullptr, nullptr) <= 0)
		return;
	printf("%s", utf8.c_str());
	fflush(stdout);
}

static float ClampRate(float rate)
{
	if (!(rate > 0.0f)) return 1.0f;
	return (float)std::clamp(rate, 0.10f, 4.0f);
}

namespace
{
	bool IsValidMediaState(MediaState value) noexcept
	{
		switch (value)
		{
		case MediaState::Manual:
		case MediaState::Play:
		case MediaState::Close:
		case MediaState::Pause:
		case MediaState::Stop:
			return true;
		default:
			return false;
		}
	}

	template<typename TValue>
	DependencyPropertyOptions<MediaElement, TValue> MediaElementPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags = DependencyPropertyFlags::None)
	{
		DependencyPropertyOptions<MediaElement, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

}

static std::vector<float> BuildTimeStretchStageRates(float rate)
{
	return { ClampRate(rate) };
}

// 调试输出函数
static void DebugOutputHr(const wchar_t* context, HRESULT hr)
{
	wchar_t buf[512] = {};
	swprintf_s(buf, L"%s: 0x%08X\n", context ? context : L"", (unsigned)hr);
	PrintLogWide(buf);
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

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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

static size_t WaveFormatByteSize(const WAVEFORMATEX* wf)
{
	if (!wf) return 0;
	return sizeof(WAVEFORMATEX) + wf->cbSize;
}

static WAVEFORMATEX* CloneWaveFormat(const WAVEFORMATEX* wf)
{
	if (!wf) return nullptr;
	const size_t bytes = WaveFormatByteSize(wf);
	auto* copy = (WAVEFORMATEX*)CoTaskMemAlloc(bytes);
	if (!copy) return nullptr;
	memcpy(copy, wf, bytes);
	return copy;
}

static bool IsPcmLikeAudioSubtype(const GUID& subtype)
{
	return subtype == MFAudioFormat_PCM || subtype == MFAudioFormat_Float;
}

static bool ShouldFallbackToMediaSessionForAudioSubtype(const GUID& subtype)
{
	if (subtype == GUID_NULL) return false;
	if (IsPcmLikeAudioSubtype(subtype)) return false;
	if (subtype == kMFAudioFormatMpegHeaac) return true;
	return false;
}

static GUID GetMfAudioSubtypeFromWaveFormat(const WAVEFORMATEX* wf)
{
	if (!wf) return GUID_NULL;
	if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return MFAudioFormat_Float;
	if (wf->wFormatTag == WAVE_FORMAT_PCM) return MFAudioFormat_PCM;
	if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		auto* ext = (const WAVEFORMATEXTENSIBLE*)wf;
		if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) return MFAudioFormat_Float;
		if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) return MFAudioFormat_PCM;
	}
	return GUID_NULL;
}

static bool PopulateMfAudioTypeFromWaveFormat(IMFMediaType* mt, const WAVEFORMATEX* wf)
{
	if (!mt || !wf) return false;
	const GUID subtype = GetMfAudioSubtypeFromWaveFormat(wf);
	if (subtype == GUID_NULL) return false;
	if (FAILED(mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))) return false;
	if (FAILED(mt->SetGUID(MF_MT_SUBTYPE, subtype))) return false;
	if (FAILED(mt->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wf->nChannels))) return false;
	if (FAILED(mt->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wf->nSamplesPerSec))) return false;
	if (FAILED(mt->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, wf->wBitsPerSample))) return false;
	if (FAILED(mt->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wf->nBlockAlign))) return false;
	if (FAILED(mt->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, wf->nAvgBytesPerSec))) return false;
	(void)mt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
	return true;
}

static GUID GetAudioSubtypeFromMediaType(IMFMediaType* mt)
{
	if (!mt) return GUID_NULL;
	GUID subtype = GUID_NULL;
	if (FAILED(mt->GetGUID(MF_MT_SUBTYPE, &subtype)))
		return GUID_NULL;
	return subtype;
}

static WAVEFORMATEX* ResolveSharedModeWaveFormat(const WAVEFORMATEX* requested)
{
	if (!requested) return nullptr;
	ComPtr<IMMDeviceEnumerator> enumerator;
	ComPtr<IMMDevice> device;
	ComPtr<IAudioClient> audioClient;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
	if (FAILED(hr)) return nullptr;
	hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	if (FAILED(hr)) return nullptr;
	hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
	if (FAILED(hr)) return nullptr;
	WAVEFORMATEX* closest = nullptr;
	hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, requested, &closest);
	if (hr == S_OK)
	{
		if (closest) CoTaskMemFree(closest);
		return CloneWaveFormat(requested);
	}
	if (hr == S_FALSE && closest)
	{
		WAVEFORMATEX* resolved = CloneWaveFormat(closest);
		CoTaskMemFree(closest);
		return resolved;
	}
	if (closest) CoTaskMemFree(closest);
	return nullptr;
}

static void FillPcmWaveFormat(WAVEFORMATEXTENSIBLE& wf, WORD channels, DWORD sampleRate, WORD bitsPerSample, bool isFloat)
{
	ZeroMemory(&wf, sizeof(wf));
	wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
	wf.Format.nChannels = channels;
	wf.Format.nSamplesPerSec = sampleRate;
	wf.Format.wBitsPerSample = bitsPerSample;
	wf.Format.nBlockAlign = (WORD)(channels * (bitsPerSample / 8));
	wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
	wf.Samples.wValidBitsPerSample = bitsPerSample;
	wf.dwChannelMask = (channels == 1) ? SPEAKER_FRONT_CENTER : ((channels == 2) ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : 0);
	wf.SubFormat = isFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
}

static void FillSimpleWaveFormat(WAVEFORMATEX& wf, WORD channels, DWORD sampleRate, WORD bitsPerSample, bool isFloat)
{
	ZeroMemory(&wf, sizeof(wf));
	wf.wFormatTag = isFloat ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
	wf.cbSize = 0;
	wf.nChannels = channels;
	wf.nSamplesPerSec = sampleRate;
	wf.wBitsPerSample = bitsPerSample;
	wf.nBlockAlign = (WORD)(channels * (bitsPerSample / 8));
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
}

static bool WaveFormatsEquivalent(const WAVEFORMATEX* a, const WAVEFORMATEX* b)
{
	if (!a || !b) return false;
	if (a->wFormatTag != b->wFormatTag) return false;
	if (a->nChannels != b->nChannels) return false;
	if (a->nSamplesPerSec != b->nSamplesPerSec) return false;
	if (a->wBitsPerSample != b->wBitsPerSample) return false;
	if (a->nBlockAlign != b->nBlockAlign) return false;
	if (a->nAvgBytesPerSec != b->nAvgBytesPerSec) return false;
	if (a->cbSize != b->cbSize) return false;
	if (a->wFormatTag == WAVE_FORMAT_EXTENSIBLE && a->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
	{
		auto* ea = (const WAVEFORMATEXTENSIBLE*)a;
		auto* eb = (const WAVEFORMATEXTENSIBLE*)b;
		if (ea->SubFormat != eb->SubFormat) return false;
		if (ea->dwChannelMask != eb->dwChannelMask) return false;
		if (ea->Samples.wValidBitsPerSample != eb->Samples.wValidBitsPerSample) return false;
	}
	return true;
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
		// 标准 WSOLA 结构：较长的输出序列 + 较短的重叠区。
		// 旧实现用 50% 重叠，每 20ms 都把两段相近音频混合一次，容易形成随音色变化的梳状/调制噪声。
		_hopOutFrames = (UINT32)std::clamp((int)std::lround((double)sampleRate * 0.040), 512, 4096);
		const int overlapMax = std::max(64, (int)_hopOutFrames / 2);
		_overlapFrames = (UINT32)std::clamp((int)std::lround((double)sampleRate * 0.008), 64, overlapMax);
		_windowFrames = _hopOutFrames + _overlapFrames;
		_searchFrames = (UINT32)std::clamp((int)std::lround((double)sampleRate * 0.015), 128, 2048);
		Reset();
	}

	void Reset()
	{
		_in.clear();
		_baseFrame = 0;
		_lastAnalysisFrame = 0;
		_nextPredFrame = 0;
		_lastSearchOffsetFrames = 0;
		_hasSearchOffset = false;
		_lastMatchNcc = 1.0;
		_lastEdgePenalty = 0.0;
		_hasTail = false;
		_tail.clear();
		_out.clear();
	}

	bool Matches(UINT32 sampleRate, UINT32 channels, bool isFloat, UINT32 bitsPerSample) const
	{
		return _sampleRate == sampleRate && _channels == channels && _isFloat == isFloat && _bitsPerSample == bitsPerSample;
	}

	static bool SupportsFormat(UINT32 bitsPerSample, bool isFloat)
	{
		if (isFloat) return bitsPerSample == 32;
		return bitsPerSample == 8 || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32;
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
		if (!SupportsFormat(_bitsPerSample, _isFloat)) return false;
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
		return TakeOutput(volume, outBytes);
	}

	bool Drain(float tempo, float volume, std::vector<uint8_t>& outBytes)
	{
		outBytes.clear();
		if (_channels == 0) return true;
		if (!SupportsFormat(_bitsPerSample, _isFloat)) return false;
		SetTempo(tempo);

		if (!_in.empty())
		{
			const size_t silenceFrames = (size_t)_windowFrames + (size_t)_searchFrames + (size_t)_hopInFrames + (size_t)_overlapFrames;
			std::vector<float> silence(silenceFrames * (size_t)_channels, 0.0f);
			if (!silence.empty())
				AppendInput(silence.data(), silenceFrames);
			Generate();
		}

		if (_hasTail && !_tail.empty())
		{
			const size_t tailFrames = _tail.size() / (size_t)_channels;
			for (size_t frameIndex = 0; frameIndex < tailFrames; frameIndex++)
			{
				float fade = 1.0f;
				if (tailFrames > 1)
					fade = 1.0f - ((float)frameIndex / (float)(tailFrames - 1));
				for (UINT32 channelIndex = 0; channelIndex < _channels; channelIndex++)
				{
					_out.push_back(_tail[frameIndex * (size_t)_channels + channelIndex] * fade);
				}
			}
		}

		bool ok = TakeOutput(volume, outBytes);
		Reset();
		return ok;
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
	size_t _lastAnalysisFrame = 0;
	size_t _nextPredFrame = 0;         // absolute predicted start for next segment
	ptrdiff_t _lastSearchOffsetFrames = 0;
	bool _hasSearchOffset = false;
	double _lastMatchNcc = 1.0;
	double _lastEdgePenalty = 0.0;

	bool _hasTail = false;
	std::vector<float> _tail;          // overlapFrames * channels (tail of last segment)
	std::vector<float> _out;           // synthesized output frames, interleaved

	std::vector<float> _tmpInFloat;
	std::vector<uint8_t> _tmpOutBytes;

	bool TakeOutput(float volume, std::vector<uint8_t>& outBytes)
	{
		outBytes.clear();
		if (_out.empty()) return true;

		if (volume < 0.999f)
		{
			volume = (float)std::clamp(volume, 0.0f, 1.0f);
			for (float& sampleValue : _out)
				sampleValue *= volume;
		}

		_tmpOutBytes.clear();
		FloatToBytes(_out.data(), _out.size() / _channels, _channels, _bitsPerSample, _isFloat, _tmpOutBytes);
		_out.clear();
		outBytes = std::move(_tmpOutBytes);
		return true;
	}

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
		if (bits == 8 && !isFloat)
		{
			for (size_t sampleIndex = 0; sampleIndex < frames * (size_t)channels; sampleIndex++)
				out[sampleIndex] = ((float)p[sampleIndex] - 128.0f) / 128.0f;
			return true;
		}
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
		if (bits == 24 && !isFloat)
		{
			const size_t totalSamples = frames * (size_t)channels;
			for (size_t sampleIndex = 0; sampleIndex < totalSamples; sampleIndex++)
			{
				const size_t byteIndex = sampleIndex * 3;
				int32_t sampleValue = (int32_t)p[byteIndex] | ((int32_t)p[byteIndex + 1] << 8) | ((int32_t)p[byteIndex + 2] << 16);
				if (sampleValue & 0x00800000)
					sampleValue |= ~0x00FFFFFF;
				out[sampleIndex] = (float)((double)sampleValue / 8388608.0);
			}
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
		if (bits == 8 && !isFloat)
		{
			out.resize(total);
			for (size_t sampleIndex = 0; sampleIndex < total; sampleIndex++)
			{
				float sampleValue = std::clamp(in[sampleIndex], -1.0f, 1.0f);
				int encoded = (int)std::lround(sampleValue * 127.0f + 128.0f);
				out[sampleIndex] = (uint8_t)std::clamp(encoded, 0, 255);
			}
			return;
		}
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
		if (bits == 24 && !isFloat)
		{
			out.resize(total * 3);
			for (size_t sampleIndex = 0; sampleIndex < total; sampleIndex++)
			{
				float sampleValue = std::clamp(in[sampleIndex], -1.0f, 1.0f);
				int32_t encoded = (int32_t)std::clamp((int)std::lround(sampleValue * 8388607.0f), -8388608, 8388607);
				uint32_t packed = (uint32_t)encoded;
				const size_t byteIndex = sampleIndex * 3;
				out[byteIndex] = (uint8_t)(packed & 0xFF);
				out[byteIndex + 1] = (uint8_t)((packed >> 8) & 0xFF);
				out[byteIndex + 2] = (uint8_t)((packed >> 16) & 0xFF);
			}
			return;
		}
		if (bits == 32 && !isFloat)
		{
			out.resize(total * sizeof(int32_t));
			auto* destination = (int32_t*)out.data();
			for (size_t sampleIndex = 0; sampleIndex < total; sampleIndex++)
			{
				float sampleValue = std::clamp(in[sampleIndex], -1.0f, 1.0f);
				double encoded = (double)sampleValue * 2147483647.0;
				destination[sampleIndex] = (int32_t)std::clamp(encoded, (double)INT32_MIN, (double)INT32_MAX);
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
		const size_t searchRadius = (_tempo > 1.0f)
			? std::min<size_t>((size_t)_searchFrames, std::max<size_t>(16, (size_t)_hopOutFrames / 8))
			: std::min<size_t>((size_t)_searchFrames, std::max<size_t>(64, (size_t)_hopOutFrames / 4));
		const size_t minAdvance = std::max<size_t>(1, (size_t)std::floor((double)_hopInFrames * 0.70));
		const size_t maxAdvance = std::max<size_t>(minAdvance, (size_t)std::ceil((double)_hopInFrames * 1.30));
		const size_t minAllowedStart = _lastAnalysisFrame + minAdvance;
		const size_t maxAllowedStart = _lastAnalysisFrame + maxAdvance;

		size_t startMin = (predStartAbs > searchRadius) ? (predStartAbs - searchRadius) : _baseFrame;
		if (startMin < _baseFrame) startMin = _baseFrame;
		if (startMin < minAllowedStart) startMin = minAllowedStart;
		size_t startMax = predStartAbs + searchRadius;
		if (startMax > maxStart) startMax = maxStart;
		if (startMax > maxAllowedStart) startMax = maxAllowedStart;
		if (startMin > startMax) startMin = startMax;

		// 归一化相关（NCC）比简单点积更稳，能显著降低撕裂感；并用 coarse-to-fine 降低 CPU。
		const UINT32 overlap = _overlapFrames;
		const UINT32 chs = _channels;
		const size_t strideSamples = (size_t)chs;

		// 采样步长：overlap 越大越稀疏（降低运算量），但保留足够判别力。
		const UINT32 iStep = (overlap >= 1024) ? 4u : (overlap >= 512 ? 2u : 1u);
		const size_t sCoarseStep = (overlap >= 512) ? 2u : 1u;

		auto signedFrameDiff = [](size_t a, size_t b) -> ptrdiff_t
		{
			return (a >= b) ? (ptrdiff_t)(a - b) : -(ptrdiff_t)(b - a);
		};

		struct MatchScore
		{
			double Total = -1e300;
			double Ncc = -1.0;
			double EdgePenalty = 1.0;
			double TransientPenalty = 0.0;
		};

		auto nccScore = [&](size_t startAbs, UINT32 localIStep) -> MatchScore
		{
			MatchScore result;
			double sumA = 0.0;
			double sumB = 0.0;
			double count = 0.0;
			for (UINT32 i = 0; i < overlap; i += localIStep)
			{
				const float* a = _tail.data() + (size_t)i * strideSamples;
				const float* b = FramePtrAbs(startAbs + i);
				for (UINT32 c = 0; c < chs; c++)
				{
					sumA += (double)a[c];
					sumB += (double)b[c];
					count += 1.0;
				}
			}
			if (count <= 0.0) return result;
			const double meanA = sumA / count;
			const double meanB = sumB / count;

			double dot = 0.0;
			double ea = 0.0;
			double eb = 0.0;
			for (UINT32 i = 0; i < overlap; i += localIStep)
			{
				const float* a = _tail.data() + (size_t)i * strideSamples;
				const float* b = FramePtrAbs(startAbs + i);
				for (UINT32 c = 0; c < chs; c++)
				{
					double av = (double)a[c] - meanA;
					double bv = (double)b[c] - meanB;
					dot += av * bv;
					ea += av * av;
					eb += bv * bv;
				}
			}
			const double denom = std::sqrt(ea * eb) + 1e-12;
			result.Ncc = dot / denom;

			double edgeError = 0.0;
			double edgeScale = 1e-9;
			double transientError = 0.0;
			double transientScale = 1e-9;
			const UINT32 edgeFrames = std::min<UINT32>(overlap, 32u);
			for (UINT32 i = 0; i < edgeFrames; i++)
			{
				const float* a = _tail.data() + (size_t)i * strideSamples;
				const float* b = FramePtrAbs(startAbs + i);
				for (UINT32 c = 0; c < chs; c++)
				{
					const double av = (double)a[c];
					const double bv = (double)b[c];
					const double diff = av - bv;
					edgeError += diff * diff;
					edgeScale += av * av + bv * bv;
				}
			}
			if (edgeFrames > 1)
			{
				for (UINT32 i = 1; i < edgeFrames; i++)
				{
					const float* a0 = _tail.data() + (size_t)(i - 1) * strideSamples;
					const float* a1 = _tail.data() + (size_t)i * strideSamples;
					const float* b0 = FramePtrAbs(startAbs + i - 1);
					const float* b1 = FramePtrAbs(startAbs + i);
					for (UINT32 c = 0; c < chs; c++)
					{
						const double da = (double)a1[c] - (double)a0[c];
						const double db = (double)b1[c] - (double)b0[c];
						const double diff = da - db;
						edgeError += diff * diff;
						edgeScale += da * da + db * db;
					}
				}
			}
			if (edgeFrames > 2)
			{
				for (UINT32 i = 2; i < edgeFrames; i++)
				{
					const float* a0 = _tail.data() + (size_t)(i - 2) * strideSamples;
					const float* a1 = _tail.data() + (size_t)(i - 1) * strideSamples;
					const float* a2 = _tail.data() + (size_t)i * strideSamples;
					const float* b0 = FramePtrAbs(startAbs + i - 2);
					const float* b1 = FramePtrAbs(startAbs + i - 1);
					const float* b2 = FramePtrAbs(startAbs + i);
					for (UINT32 c = 0; c < chs; c++)
					{
						const double tailCurvature = ((double)a2[c] - (double)a1[c]) - ((double)a1[c] - (double)a0[c]);
						const double headCurvature = ((double)b2[c] - (double)b1[c]) - ((double)b1[c] - (double)b0[c]);
						const double diff = tailCurvature - headCurvature;
						transientError += diff * diff;
						transientScale += tailCurvature * tailCurvature + headCurvature * headCurvature;
					}
				}
			}
			result.EdgePenalty = std::min(1.0, edgeError / edgeScale);
			result.TransientPenalty = std::min(1.0, transientError / transientScale);

			const double radius = std::max(1.0, (double)searchRadius);
			const ptrdiff_t offset = signedFrameDiff(startAbs, predStartAbs);
			const double predictionPenalty = std::min(1.0, std::abs((double)offset) / radius);
			double offsetPenalty = predictionPenalty * 0.025;
			if (_hasSearchOffset)
			{
				const double offsetDelta = std::abs((double)(offset - _lastSearchOffsetFrames)) / radius;
				offsetPenalty += std::min(1.0, offsetDelta) * 0.080;
			}

			result.Total = result.Ncc - result.EdgePenalty * 0.35 - result.TransientPenalty * 0.18 - offsetPenalty;
			return result;
		};

		MatchScore bestScore;
		size_t best = startMin;
		for (size_t s = startMin; s <= startMax; s += sCoarseStep)
		{
			MatchScore score = nccScore(s, iStep);
			if (score.Total > bestScore.Total)
			{
				bestScore = score;
				best = s;
			}
			if (s >= startMax - sCoarseStep) break; // size_t 防溢出
		}

		// 精细搜索：在 coarse 最优点附近用更密集采样再对齐一次。
		{
			const size_t refineRadius = (size_t)std::min<UINT32>(16u, overlap / 4u + 1u);
			size_t r0 = (best > refineRadius) ? (best - refineRadius) : startMin;
			size_t r1 = best + refineRadius;
			if (r0 < startMin) r0 = startMin;
			if (r1 > startMax) r1 = startMax;

			bestScore = {};
			for (size_t s = r0; s <= r1; s++)
			{
				MatchScore score = nccScore(s, 1u);
				if (score.Total > bestScore.Total)
				{
					bestScore = score;
					best = s;
				}
				if (s == r1) break;
			}
		}

		if (bestScore.Ncc < 0.10 || bestScore.EdgePenalty > 0.55 || bestScore.TransientPenalty > 0.65)
		{
			best = std::clamp(predStartAbs, startMin, startMax);
			bestScore = nccScore(best, 1u);
		}
		_lastSearchOffsetFrames = signedFrameDiff(best, predStartAbs);
		_hasSearchOffset = true;
		_lastMatchNcc = bestScore.Ncc;
		_lastEdgePenalty = std::max(bestScore.EdgePenalty, bestScore.TransientPenalty * 0.75);
		return best;
	}

	UINT32 ActiveOverlapFrames() const
	{
		if (_overlapFrames <= 1) return _overlapFrames;
		if (_lastMatchNcc < 0.20 || _lastEdgePenalty > 0.40)
			return std::max<UINT32>(16u, _overlapFrames / 3u);
		if (_lastMatchNcc < 0.38 || _lastEdgePenalty > 0.25)
			return std::max<UINT32>(32u, _overlapFrames / 2u);
		return _overlapFrames;
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
		const UINT32 activeOverlapFrames = std::min<UINT32>(_overlapFrames, ActiveOverlapFrames());
		// 先输出融合后的 overlap
		if (activeOverlapFrames > 0)
		{
			for (UINT32 i = 0; i < activeOverlapFrames; i++)
			{
				// Raised-cosine crossfade（比线性更不容易产生撕裂/毛刺）
				float w = 1.0f;
				if (activeOverlapFrames > 1)
				{
					const float x = (float)i / (float)(activeOverlapFrames - 1);
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
		const UINT32 midStart = activeOverlapFrames;
		const UINT32 midEnd = (_windowFrames > _overlapFrames) ? (_windowFrames - _overlapFrames) : _overlapFrames;
		if (midEnd > midStart)
		{
			const float* p0 = seg + (size_t)midStart * (size_t)_channels;
			const float* p1 = seg + (size_t)midEnd * (size_t)_channels;
			for (const float* p = p0; p < p1; p++)
				_out.push_back(*p);
		}
		_tail.assign(seg + (size_t)(_windowFrames - _overlapFrames) * (size_t)_channels, seg + (size_t)_windowFrames * (size_t)_channels);
		_hasTail = true;
	}

	void MaybeDropOldInput()
	{
		// 保留 search 窗口之前的一点余量即可
		if (_nextPredFrame <= _baseFrame) return;
		size_t keepFrom = (_nextPredFrame > _searchFrames) ? (_nextPredFrame - _searchFrames) : _baseFrame;
		const size_t availableFrames = AvailableFrames();
		const size_t availableEnd = _baseFrame + availableFrames;
		// High tempos can predict beyond PCM that has actually arrived. Never
		// move the absolute input coordinate into that future range.
		if (keepFrom > availableEnd) keepFrom = availableEnd;
		if (keepFrom <= _baseFrame) return;
		size_t dropFrames = keepFrom - _baseFrame;
		// 不要频繁 erase；累计到一定规模再 compact
		if (dropFrames < 4096) return;
		const size_t dropSamples = dropFrames * (size_t)_channels;
		if (dropSamples >= _in.size())
		{
			_in.clear();
			_baseFrame = availableEnd;
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
			_lastAnalysisFrame = _baseFrame;
			_nextPredFrame = _lastAnalysisFrame + _hopInFrames;
			MaybeDropOldInput();
		}

		for (;;)
		{
			// 需要保证候选段可用
			if (availAbsEnd < _nextPredFrame + _windowFrames) break;
			size_t bestStart = FindBestStart(_nextPredFrame);
			const size_t minStart = _lastAnalysisFrame + std::max<size_t>(1, (size_t)std::floor((double)_hopInFrames * 0.70));
			if (bestStart < minStart) bestStart = minStart;
			if (availAbsEnd < bestStart + _windowFrames) break;
			const float* seg = FramePtrAbs(bestStart);
			EmitNext(seg);
			_lastAnalysisFrame = bestStart;
			_nextPredFrame += _hopInFrames;
			MaybeDropOldInput();
		}
	}
};

UINT64 MediaElement::MeasureWsolaOutputFramesForTesting(
	float rate, UINT32 inputFrames, UINT32 chunkFrames)
{
	if (inputFrames == 0 || chunkFrames == 0) return 0;
	constexpr UINT32 sampleRate = 48'000;
	constexpr UINT32 channels = 2;
	WsolaTimeStretch stretch(sampleRate, channels, true, 32);
	std::vector<float> input(
		static_cast<size_t>(chunkFrames) * channels, 0.0f);
	std::vector<uint8_t> output;
	UINT64 outputFrames = 0;
	UINT32 consumedFrames = 0;
	while (consumedFrames < inputFrames)
	{
		const UINT32 frames = (std::min)(
			chunkFrames, inputFrames - consumedFrames);
		if (!stretch.ProcessChunk(
			input.data(), static_cast<size_t>(frames) * channels * sizeof(float),
			rate, 1.0f, output))
		{
			return 0;
		}
		outputFrames += output.size() / (channels * sizeof(float));
		consumedFrames += frames;
	}
	if (!stretch.Drain(rate, 1.0f, output)) return 0;
	outputFrames += output.size() / (channels * sizeof(float));
	return outputFrames;
}

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

static bool TryGetVideoAperture(IMFMediaType* mt, MFVideoArea& area)
{
	if (!mt) return false;
	UINT32 panScanEnabled = FALSE;
	if (SUCCEEDED(mt->GetUINT32(
		MF_MT_PAN_SCAN_ENABLED, &panScanEnabled)) && panScanEnabled)
	{
		UINT32 blobSize = sizeof(MFVideoArea);
		if (SUCCEEDED(mt->GetBlob(
			MF_MT_PAN_SCAN_APERTURE, reinterpret_cast<UINT8*>(&area),
			blobSize, &blobSize)) && blobSize == sizeof(MFVideoArea))
			return true;
	}
	UINT32 blobSize = sizeof(MFVideoArea);
	if (SUCCEEDED(mt->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, blobSize, &blobSize)) && blobSize == sizeof(MFVideoArea))
		return true;
	blobSize = sizeof(MFVideoArea);
	if (SUCCEEDED(mt->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area, blobSize, &blobSize)) && blobSize == sizeof(MFVideoArea))
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
// MediaElementCallback 实现
// ========================================

MediaElementCallback::MediaElementCallback(
	MediaElement* player, UINT64 mediaLoadGeneration)
	: _refCount(1), _player(player),
	_mediaLoadGeneration(mediaLoadGeneration) {}
MediaElementCallback::~MediaElementCallback() {}

STDMETHODIMP MediaElementCallback::QueryInterface(REFIID riid, void** ppv)
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

STDMETHODIMP_(ULONG) MediaElementCallback::AddRef() { return InterlockedIncrement(&_refCount); }
STDMETHODIMP_(ULONG) MediaElementCallback::Release() { ULONG count = InterlockedDecrement(&_refCount); if (count == 0) delete this; return count; }

STDMETHODIMP MediaElementCallback::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
	if (pdwFlags == nullptr || pdwQueue == nullptr) return E_POINTER;
	*pdwFlags = 0; 
	*pdwQueue = MFASYNC_CALLBACK_QUEUE_STANDARD;
	return S_OK;
}

STDMETHODIMP MediaElementCallback::Invoke(IMFAsyncResult* pResult)
{
	std::scoped_lock playerLock(_playerMutex);
	MediaElement* player = _player;
	if (!player) return S_OK;
	if (!player->_mediaSession) return S_OK;

	HRESULT hr = S_OK;
	ComPtr<IMFMediaEvent> pEvent;
	hr = player->_mediaSession->EndGetEvent(pResult, &pEvent);
	if (FAILED(hr)) return S_OK;
	if (_mediaLoadGeneration != player->CurrentMediaLoadGeneration())
		return S_OK;

	MediaEventType eventType;
	hr = pEvent->GetType(&eventType);
	if (FAILED(hr)) return S_OK;

	switch (eventType)
	{
	case MESessionTopologyStatus:
	{
		UINT32 status = 0;
		HRESULT topoHr = pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &status);
		if (SUCCEEDED(topoHr))
		{
			if (status == MF_TOPOSTATUS_READY)
			{
				player->MarkTopologyReady();
				player->RefreshVideoFormatFromSource();
				if (player->_mediaLoaded.load(std::memory_order_acquire))
				{
					player->SetVolumeImpl(player->_volume);
					std::weak_ptr<std::atomic_bool> weakLifetime =
						player->_lifetimeToken;
					player->DispatchToOwner([player, weakLifetime]()
					{
						auto lifetime = weakLifetime.lock();
						if (!lifetime || !*lifetime) return;
						UINT64 startEpoch = 0;
						UINT64 explicitGeneration = 0;
						UINT64 mediaGeneration = 0;
						const HRESULT startHr =
							player->StartPendingPlaybackIfAllowed(
								&startEpoch, &explicitGeneration,
								&mediaGeneration);
						if (FAILED(startHr))
						{
							if (player->_useSourceReader
								&& player->_useMediaSessionAudioCompanion
								&& startEpoch != 0)
							{
								player->HandleCompanionSessionFailure(
									startHr, startEpoch);
							}
							else
							{
								MediaElement::StandaloneSessionCommandToken
									failureToken{};
								failureToken.Sequence =
									(std::numeric_limits<UINT64>::max)();
								failureToken.ExplicitCommandGeneration =
									explicitGeneration;
								failureToken.MediaLoadGeneration =
									mediaGeneration;
								player->HandleStandaloneSessionFailure(
									startHr, failureToken);
							}
						}
					});
				}
			}
		}
		else
		{
			DebugOutputHr(L"[MediaElement] topology status attribute missing", topoHr);
		}
		break;
	}
	case MESessionStarted:
	{
		HRESULT eventStatus = S_OK;
		const HRESULT statusResult = pEvent->GetStatus(&eventStatus);
		if (FAILED(statusResult)) eventStatus = statusResult;
		if (player->_useSourceReader
			&& player->_useMediaSessionAudioCompanion)
		{
			UINT64 observedEpoch = 0;
			const auto observation = player->ObserveCompanionSessionStarted(
				eventStatus, &observedEpoch);
			if (observation
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleCompanionSessionFailure(
					eventStatus, observedEpoch);
			}
			else if (observation
				== MediaElement::CompanionSessionObservation::Accepted)
			{
				std::weak_ptr<std::atomic_bool> weakLifetime =
					player->_lifetimeToken;
				player->DispatchToOwner(
					[player, observedEpoch, weakLifetime]
					{
						auto lifetime = weakLifetime.lock();
						if (!lifetime || !*lifetime) return;
						player->StartSourceReaderWorkerAfterCompanionStarted(
							observedEpoch);
					});
			}
		}
		else if (FAILED(eventStatus))
		{
			MediaElement::StandaloneSessionCommandToken token{};
			const auto observation = player->ObserveStandaloneSessionCommand(
				MediaElement::StandaloneSessionCommandKind::Start,
				eventStatus, &token);
			if (observation
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleStandaloneSessionFailure(eventStatus, token);
			}
		}
		else
		{
			MediaElement::StandaloneSessionCommandToken token{};
			(void)player->ObserveStandaloneSessionCommand(
				MediaElement::StandaloneSessionCommandKind::Start,
				eventStatus, &token);
		}
		break;
	}
	case MESessionPaused:
	{
		HRESULT eventStatus = S_OK;
		const HRESULT statusResult = pEvent->GetStatus(&eventStatus);
		if (FAILED(statusResult)) eventStatus = statusResult;
		if (player->_useSourceReader
			&& player->_useMediaSessionAudioCompanion)
		{
			UINT64 observedEpoch = 0;
			if (player->ObserveCompanionSessionControl(
				MediaElement::CompanionSessionControlKind::Pause,
				eventStatus, &observedEpoch)
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleCompanionSessionFailure(
					eventStatus, observedEpoch);
			}
		}
		else
		{
			MediaElement::StandaloneSessionCommandToken token{};
			const auto observation = player->ObserveStandaloneSessionCommand(
				MediaElement::StandaloneSessionCommandKind::Pause,
				eventStatus, &token);
			if (observation
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleStandaloneSessionFailure(eventStatus, token);
			}
		}
		break;
	}
	case MESessionStopped:
	{
		HRESULT eventStatus = S_OK;
		const HRESULT statusResult = pEvent->GetStatus(&eventStatus);
		if (FAILED(statusResult)) eventStatus = statusResult;
		if (player->_useSourceReader
			&& player->_useMediaSessionAudioCompanion)
		{
			UINT64 observedEpoch = 0;
			if (player->ObserveCompanionSessionControl(
				MediaElement::CompanionSessionControlKind::Stop,
				eventStatus, &observedEpoch)
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleCompanionSessionFailure(
					eventStatus, observedEpoch);
			}
		}
		else
		{
			MediaElement::StandaloneSessionCommandToken token{};
			const auto observation = player->ObserveStandaloneSessionCommand(
				MediaElement::StandaloneSessionCommandKind::Stop,
				eventStatus, &token);
			if (observation
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleStandaloneSessionFailure(eventStatus, token);
			}
		}
		break;
	}
	case MESessionEnded:
	{
		HRESULT eventStatus = S_OK;
		const HRESULT statusResult = pEvent->GetStatus(&eventStatus);
		if (FAILED(statusResult)) eventStatus = statusResult;
		if (player->_useSourceReader
			&& player->_useMediaSessionAudioCompanion)
		{
			// The SourceReader video stream and this audio companion participate
			// in one epoch/mask coordinator.  Neither side owns Ended or Loop.
			UINT64 observedEpoch = 0;
			if (player->ObserveCompanionSessionEnded(
				eventStatus, &observedEpoch)
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleCompanionSessionFailure(
					eventStatus, observedEpoch);
			}
		}
		else if (FAILED(eventStatus))
		{
			MediaElement::StandaloneSessionCommandToken token{};
			const auto observation = player->ObserveStandaloneSessionEnded(
				eventStatus, &token);
			if (observation
				== MediaElement::CompanionSessionObservation::FailedCurrent)
			{
				player->HandleStandaloneSessionFailure(eventStatus, token);
			}
		}
		else
		{
			MediaElement::StandaloneSessionCommandToken token{};
			if (player->ObserveStandaloneSessionEnded(
				eventStatus, &token)
				== MediaElement::CompanionSessionObservation::Accepted)
			{
				player->QueueStandaloneSessionCompletion(token);
			}
		}
		break;
	}
	case MEError:
	{
		// 发生错误
		HRESULT statusHr = S_OK;
		(void)pEvent->GetStatus(&statusHr);
		DebugOutputHr(L"MEError", statusHr);
		if (player->_useSourceReader
			&& player->_useMediaSessionAudioCompanion)
		{
			player->HandleCompanionSessionFailure(
				statusHr,
				player->CaptureCompanionSessionFailureEpoch());
		}
		else
		{
			auto token = player->CaptureStandaloneSessionFailureToken();
			if (token.Sequence != 0)
				player->HandleStandaloneSessionFailure(statusHr, token);
		}
		break;
	}
	}

	// 事件处理器可能同步 Close 播放器并使回调解绑定。
	if (_player != player) return S_OK;

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
	VideoSampleGrabberCallback(MediaElement* player) : _refCount(1), _player(player) {}
	virtual ~VideoSampleGrabberCallback() {}
	void DetachPlayer()
	{
		std::scoped_lock lock(_playerMutex);
		_player = nullptr;
	}

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
		std::scoped_lock playerLock(_playerMutex);
		MediaElement* player = _player;
		if (!player) return S_OK;
		if (!pSampleBuffer || dwSampleSize == 0) return S_OK;
		player->OnVideoFrame(pSampleBuffer, dwSampleSize);
		return S_OK;
	}

	STDMETHODIMP OnShutdown() { return S_OK; }

private:
	LONG _refCount;
	std::mutex _playerMutex;
	MediaElement* _player;
};

// ========================================
// MediaElement 实现
// ========================================

UIClass MediaElement::Type() { return UIClass::UI_MediaElement; }

const DependencyProperty& MediaElement::SourceProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			std::wstring{} CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 10, DependencyPropertyEditorKind::Text),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			MediaElement, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Source"),
				[](MediaElement& target) { return target.Source; },
				[](MediaElement& target, const std::wstring& value)
				{ target.Source = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MediaElement::VolumeProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			0.5 CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 30, DependencyPropertyEditorKind::Number));
		options.Validate = [](const double& proposed)
		{ return std::isfinite(proposed); };
		options.Coerce = [](MediaElement&, const double& proposed)
			-> std::optional<double>
		{
			return (std::clamp)(proposed, 0.0, 1.0);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Maximum = 1.0;
		options.Design.Step = 0.01;
		)
		return DependencyPropertyRegistry::RegisterStatic<MediaElement, double>(
			DependencyPropertyRegistrationLiteral(L"Volume"),
			[](MediaElement& target) { return target.Volume; },
			[](MediaElement& target, const double& value)
			{ target.Volume = value; }, {}, std::move(options));
	}();
	return *registration;
}

static UINT32 QueryPresentationRateLimitHz(HWND window) noexcept
{
	constexpr UINT32 fallbackRateHz = 60;
	if (!window) return fallbackRateHz;
	const HMONITOR monitor = MonitorFromWindow(
		window, MONITOR_DEFAULTTONEAREST);
	if (!monitor) return fallbackRateHz;
	MONITORINFOEXW monitorInfo{};
	monitorInfo.cbSize = sizeof(monitorInfo);
	if (!GetMonitorInfoW(monitor, &monitorInfo)) return fallbackRateHz;
	DEVMODEW mode{};
	mode.dmSize = sizeof(mode);
	if (!EnumDisplaySettingsW(
		monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &mode))
		return fallbackRateHz;
	const UINT32 refreshRateHz = mode.dmDisplayFrequency;
	return refreshRateHz >= 24 && refreshRateHz <= 1000
		? refreshRateHz : fallbackRateHz;
}

const DependencyProperty& MediaElement::SpeedRatioProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			1.0 CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 40, DependencyPropertyEditorKind::Number));
		options.Validate = [](const double& proposed)
		{ return std::isfinite(proposed); };
		options.Coerce = [](MediaElement&, const double& proposed)
			-> std::optional<double>
		{
			return static_cast<double>(ClampRate(static_cast<float>(proposed)));
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.10;
		options.Design.Maximum = 4.0;
		options.Design.Step = 0.10;
		)
		return DependencyPropertyRegistry::RegisterStatic<MediaElement, double>(
			DependencyPropertyRegistrationLiteral(L"SpeedRatio"),
			[](MediaElement& target) { return target.SpeedRatio; },
			[](MediaElement& target, const double& value)
			{ target.SpeedRatio = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MediaElement::LoadedBehaviorProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			MediaState::Play CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 20, DependencyPropertyEditorKind::Choice));
		options.Validate = IsValidMediaState;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Manual", BindingValue(MediaState::Manual) },
			{ L"Play", BindingValue(MediaState::Play) },
			{ L"Close", BindingValue(MediaState::Close) },
			{ L"Pause", BindingValue(MediaState::Pause) },
			{ L"Stop", BindingValue(MediaState::Stop) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<MediaElement, MediaState>(
			DependencyPropertyRegistrationLiteral(L"LoadedBehavior"),
			[](MediaElement& target) { return target.LoadedBehavior; },
			[](MediaElement& target, const MediaState& value)
			{ target.LoadedBehavior = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MediaElement::UnloadedBehaviorProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			MediaState::Close CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 30, DependencyPropertyEditorKind::Choice));
		options.Validate = IsValidMediaState;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Manual", BindingValue(MediaState::Manual) },
			{ L"Play", BindingValue(MediaState::Play) },
			{ L"Close", BindingValue(MediaState::Close) },
			{ L"Pause", BindingValue(MediaState::Pause) },
			{ L"Stop", BindingValue(MediaState::Stop) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<MediaElement, MediaState>(
			DependencyPropertyRegistrationLiteral(L"UnloadedBehavior"),
			[](MediaElement& target) { return target.UnloadedBehavior; },
			[](MediaElement& target, const MediaState& value)
			{ target.UnloadedBehavior = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MediaElement::LoopProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<MediaElement, bool>(
			DependencyPropertyRegistrationLiteral(L"Loop"),
			[](MediaElement& target) { return target.Loop; },
			[](MediaElement& target, const bool& value)
			{ target.Loop = value; }, {},
			MediaElementPropertyOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					L"Media", 160, 20,
					DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& MediaElement::EnableHardwareDecodeProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<MediaElement, bool>(
			DependencyPropertyRegistrationLiteral(L"EnableHardwareDecode"),
			[](MediaElement& target) { return target.EnableHardwareDecode; },
			[](MediaElement& target, const bool& value)
			{ target.EnableHardwareDecode = value; }, {},
			MediaElementPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					L"Media", 160, 50,
					DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& MediaElement::EnableDxgiVideoOutputProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<MediaElement, bool>(
			DependencyPropertyRegistrationLiteral(L"EnableDxgiVideoOutput"),
			[](MediaElement& target) { return target.EnableDxgiVideoOutput; },
			[](MediaElement& target, const bool& value)
			{ target.EnableDxgiVideoOutput = value; }, {},
			MediaElementPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					L"Media", 160, 55,
					DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& MediaElement::PreferNv12VideoOutputProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<MediaElement, bool>(
			DependencyPropertyRegistrationLiteral(L"PreferNv12VideoOutput"),
			[](MediaElement& target) { return target.PreferNv12VideoOutput; },
			[](MediaElement& target, const bool& value)
			{ target.PreferNv12VideoOutput = value; }, {},
			MediaElementPropertyOptions(
				true CUI_DESIGN_METADATA_ARGUMENTS(
					L"Media", 160, 60,
					DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& MediaElement::StretchProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			::Stretch::Uniform
			CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 70, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		options.Validate = [](::Stretch value)
		{
			switch (value)
			{
			case ::Stretch::None:
			case ::Stretch::Fill:
			case ::Stretch::Uniform:
			case ::Stretch::UniformToFill:
				return true;
			default:
				return false;
			}
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"None", BindingValue(::Stretch::None) },
			{ L"Fill", BindingValue(::Stretch::Fill) },
			{ L"Uniform", BindingValue(::Stretch::Uniform) },
			{ L"UniformToFill", BindingValue(
				::Stretch::UniformToFill) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<MediaElement,
			::Stretch>(
				DependencyPropertyRegistrationLiteral(L"Stretch"),
				[](MediaElement& target) { return target.Stretch; },
				[](MediaElement& target,
					const ::Stretch& value)
				{ target.Stretch = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MediaElement::StretchDirectionProperty()
{
	static const auto registration = []
	{
		auto options = MediaElementPropertyOptions(
			::StretchDirection::Both CUI_DESIGN_METADATA_ARGUMENTS(
				L"Media", 160, 80, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		options.Validate = [](::StretchDirection value)
		{
			return value == ::StretchDirection::UpOnly
				|| value == ::StretchDirection::DownOnly
				|| value == ::StretchDirection::Both;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"UpOnly", BindingValue(::StretchDirection::UpOnly) },
			{ L"DownOnly", BindingValue(::StretchDirection::DownOnly) },
			{ L"Both", BindingValue(::StretchDirection::Both) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<MediaElement,
			::StretchDirection>(
				DependencyPropertyRegistrationLiteral(L"StretchDirection"),
				[](MediaElement& target) { return target.StretchDirection; },
				[](MediaElement& target, const ::StretchDirection& value)
				{ target.StretchDirection = value; }, {}, std::move(options));
	}();
	return *registration;
}

void MediaElement::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)SourceProperty();
	(void)VolumeProperty();
	(void)SpeedRatioProperty();
	(void)LoadedBehaviorProperty();
	(void)UnloadedBehaviorProperty();
	(void)LoopProperty();
	(void)EnableHardwareDecodeProperty();
	(void)EnableDxgiVideoOutputProperty();
	(void)PreferNv12VideoOutputProperty();
	(void)StretchProperty();
	(void)StretchDirectionProperty();
#endif
}

void MediaElement::SetPlaybackState(PlaybackState value)
{
	PlaybackState oldValue = PlaybackState::Stopped;
	if (!CommitPlaybackState(value, oldValue)) return;
	RaisePlaybackStateChanged(oldValue, value);
}

bool MediaElement::CommitPlaybackState(
	PlaybackState value, PlaybackState& oldValue) noexcept
{
	oldValue = _playState.exchange(value, std::memory_order_acq_rel);
	return oldValue != value;
}

void MediaElement::RaisePlaybackStateChanged(
	PlaybackState oldValue, PlaybackState value)
{
	// 状态变更可能来自播放工作线程；事件必须在 UI 线程上 invoke，
	// 避免用户处理器在错误线程触碰其他 UI 控件。
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner([this, oldValue, value, weakLifetime]() {
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		RequestVisualInvalidation();
		cui::framework::EventAccess::Raise(
			OnPlaybackStateChanged, this, oldValue, value);
	});
}

MediaElement::PlaybackTransitionLease::~PlaybackTransitionLease()
{
	if (_owner) _owner->EndPlaybackTransition();
}

MediaElement::PlaybackTransitionLease::PlaybackTransitionLease(
	PlaybackTransitionLease&& other) noexcept
	: _owner(std::exchange(other._owner, nullptr))
{
}

MediaElement::PlaybackTransitionLease&
MediaElement::PlaybackTransitionLease::operator=(
	PlaybackTransitionLease&& other) noexcept
{
	if (this == &other) return *this;
	if (_owner) _owner->EndPlaybackTransition();
	_owner = std::exchange(other._owner, nullptr);
	return *this;
}

MediaElement::PlaybackTransitionLease MediaElement::AcquirePlaybackTransition(
	PlaybackTransitionOrigin origin) noexcept
{
	_playbackCommandMutex.lock();
	std::scoped_lock lock(_threadMutex);
	if (_playbackGate == PlaybackGateState::Quiescing)
	{
		_playbackCommandMutex.unlock();
		return {};
	}
	if (origin == PlaybackTransitionOrigin::Automatic
		&& _playbackGate != PlaybackGateState::Open)
	{
		_playbackCommandMutex.unlock();
		return {};
	}
	if (origin == PlaybackTransitionOrigin::ExplicitPlay)
		_playbackGate = PlaybackGateState::Open;
	++_playTransitionsInFlight;
	return PlaybackTransitionLease(this);
}

UINT64 MediaElement::AdvanceExplicitPlaybackCommandGeneration() noexcept
{
	UINT64 next = _explicitPlaybackCommandGeneration.fetch_add(
		1, std::memory_order_acq_rel) + 1;
	if (next == 0)
	{
		_explicitPlaybackCommandGeneration.store(1, std::memory_order_release);
		next = 1;
	}
	return next;
}

UINT64 MediaElement::CurrentExplicitPlaybackCommandGeneration() const noexcept
{
	return _explicitPlaybackCommandGeneration.load(std::memory_order_acquire);
}

UINT64 MediaElement::AdvanceMediaLoadGeneration() noexcept
{
	UINT64 next = _mediaLoadGeneration.fetch_add(
		1, std::memory_order_acq_rel) + 1;
	if (next == 0)
	{
		_mediaLoadGeneration.store(1, std::memory_order_release);
		next = 1;
	}
	return next;
}

UINT64 MediaElement::CurrentMediaLoadGeneration() const noexcept
{
	return _mediaLoadGeneration.load(std::memory_order_acquire);
}

void MediaElement::EndPlaybackTransition() noexcept
{
	{
		std::scoped_lock lock(_threadMutex);
		if (_playTransitionsInFlight > 0)
			--_playTransitionsInFlight;
	}
	_threadIdleCv.notify_all();
	_playbackCommandMutex.unlock();
}

void MediaElement::BeginPlaybackQuiescence() noexcept
{
	std::unique_lock lock(_threadMutex);
	_playbackGate = PlaybackGateState::Quiescing;
	_threadIdleCv.wait(lock, [this]
	{
		return _playTransitionsInFlight == 0;
	});
	_threadPlaying = false;
	WakePlaybackThread();
	if (_playThread.joinable()
		&& _playThread.get_id() != std::this_thread::get_id())
	{
		_threadIdleCv.wait(lock, [this]
		{
			return !_playbackWorkerActive;
		});
	}
}

void MediaElement::CompletePlaybackQuiescence() noexcept
{
	{
		std::scoped_lock lock(_threadMutex);
		_playbackGate = PlaybackGateState::Quiesced;
	}
	_threadIdleCv.notify_all();
}

void MediaElement::OpenPlaybackGate() noexcept
{
	{
		std::scoped_lock lock(_threadMutex);
		_playbackGate = PlaybackGateState::Open;
	}
	_threadIdleCv.notify_all();
}

bool MediaElement::QueuePendingStartIfTopologyNotReady(
	bool usePosition, double position) noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	if (_topologyReady)
	{
		// A direct owner-thread command supersedes a request that READY has
		// not drained yet.
		_pendingStart = false;
		_hasPendingStartPosition = false;
		_pendingStartPosition = 0.0;
		return false;
	}
	_pendingStartPosition = position;
	_hasPendingStartPosition = usePosition;
	_pendingStart = true;
	return true;
}

void MediaElement::MarkTopologyReady() noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	_topologyReady = true;
}

bool MediaElement::TakePendingStartIfTopologyReady(
	bool& usePosition, double& position) noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	if (!_topologyReady || !_pendingStart) return false;
	usePosition = _hasPendingStartPosition;
	position = _pendingStartPosition;
	_pendingStart = false;
	_hasPendingStartPosition = false;
	_pendingStartPosition = 0.0;
	return true;
}

bool MediaElement::ClearPendingStart() noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	const bool hadPendingStart = _pendingStart;
	_pendingStart = false;
	_hasPendingStartPosition = false;
	_pendingStartPosition = 0.0;
	return hadPendingStart;
}

void MediaElement::ResetTopologyState() noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	_topologyReady = false;
	_pendingStart = false;
	_hasPendingStartPosition = false;
	_pendingStartPosition = 0.0;
	_pendingStandaloneSessionCommands.clear();
	_activeStandaloneSessionPlayback = {};
	_completedStandaloneSessionPlayback = {};
}

UINT8 MediaElement::GetSourceReaderPlaybackEndMask() const noexcept
{
	UINT8 mask = 0;
	if (_actualVideoStreamIndex != static_cast<DWORD>(-1))
		mask |= PlaybackEndReaderVideo;
	if (_useMediaSessionAudioCompanion)
	{
		mask |= PlaybackEndCompanionSession;
	}
	else if (_actualAudioStreamIndex != static_cast<DWORD>(-1))
	{
		mask |= PlaybackEndReaderAudio;
	}
	return mask;
}

UINT64 MediaElement::BeginPlaybackEndEpoch(
	UINT8 expectedMask, bool discardPendingSessionStarts) noexcept
{
	std::scoped_lock lock(_playbackEndMutex);
	++_playbackEndEpoch;
	if (_playbackEndEpoch == 0) ++_playbackEndEpoch;
	_playbackEndExpectedMask = expectedMask;
	_playbackEndObservedMask = 0;
	_playbackEndCompletionQueued = false;
	_pendingSourceReaderWorkerStartEpoch = 0;
	if (discardPendingSessionStarts)
	{
		_activeCompanionSessionEpoch = 0;
		_pendingCompanionSessionStartEpochs.clear();
		_pendingCompanionSessionPauseEpochs.clear();
		_pendingCompanionSessionStopEpochs.clear();
	}
	return _playbackEndEpoch;
}

UINT64 MediaElement::CurrentPlaybackEndEpoch() const noexcept
{
	std::scoped_lock lock(_playbackEndMutex);
	return _playbackEndEpoch;
}

UINT64 MediaElement::QueueCompanionSessionStartEpoch() noexcept
{
	if (!_useSourceReader || !_useMediaSessionAudioCompanion) return 0;
	std::scoped_lock lock(_playbackEndMutex);
	if ((_playbackEndExpectedMask & PlaybackEndCompanionSession) == 0)
		return 0;
	_pendingCompanionSessionStartEpochs.push_back(_playbackEndEpoch);
	_pendingSourceReaderWorkerStartEpoch = _playbackEndEpoch;
	return _playbackEndEpoch;
}

void MediaElement::CancelCompanionSessionStartEpoch(UINT64 epoch) noexcept
{
	if (epoch == 0) return;
	std::scoped_lock lock(_playbackEndMutex);
	// Playback transitions serialize Start calls.  Older events can remain at
	// the front, while the request being cancelled is the newest queue entry.
	if (!_pendingCompanionSessionStartEpochs.empty()
		&& _pendingCompanionSessionStartEpochs.back() == epoch)
	{
		_pendingCompanionSessionStartEpochs.pop_back();
	}
	if (_pendingSourceReaderWorkerStartEpoch == epoch)
		_pendingSourceReaderWorkerStartEpoch = 0;
}

UINT64 MediaElement::CaptureCompanionSessionFailureEpoch() const noexcept
{
	if (!_useSourceReader.load(std::memory_order_acquire)
		|| !_useMediaSessionAudioCompanion)
	{
		return 0;
	}
	std::scoped_lock lock(_playbackEndMutex);
	// Prefer the oldest event provenance.  In particular, while a replacement
	// Start is pending, a generic MEError queued by the superseded active run
	// must remain bound to that old epoch rather than stop the replacement.
	UINT64 capturedEpoch = _activeCompanionSessionEpoch;
	auto captureOlder = [&capturedEpoch](const std::deque<UINT64>& pending)
	{
		if (!pending.empty()
			&& (capturedEpoch == 0 || pending.front() < capturedEpoch))
		{
			capturedEpoch = pending.front();
		}
	};
	captureOlder(_pendingCompanionSessionStartEpochs);
	captureOlder(_pendingCompanionSessionPauseEpochs);
	captureOlder(_pendingCompanionSessionStopEpochs);
	return capturedEpoch;
}

UINT64 MediaElement::QueueCompanionSessionControlEpoch(
	CompanionSessionControlKind kind) noexcept
{
	if (!_useSourceReader.load(std::memory_order_acquire)
		|| !_useMediaSessionAudioCompanion)
	{
		return 0;
	}
	std::scoped_lock lock(_playbackEndMutex);
	if ((_playbackEndExpectedMask & PlaybackEndCompanionSession) == 0)
		return 0;
	auto& pending = kind == CompanionSessionControlKind::Pause
		? _pendingCompanionSessionPauseEpochs
		: _pendingCompanionSessionStopEpochs;
	pending.push_back(_playbackEndEpoch);
	return _playbackEndEpoch;
}

void MediaElement::CancelCompanionSessionControlEpoch(
	CompanionSessionControlKind kind, UINT64 epoch) noexcept
{
	if (epoch == 0) return;
	std::scoped_lock lock(_playbackEndMutex);
	auto& pending = kind == CompanionSessionControlKind::Pause
		? _pendingCompanionSessionPauseEpochs
		: _pendingCompanionSessionStopEpochs;
	if (!pending.empty() && pending.back() == epoch)
		pending.pop_back();
}

MediaElement::CompanionSessionObservation
MediaElement::ObserveCompanionSessionControl(
	CompanionSessionControlKind kind, HRESULT eventStatus,
	UINT64* observedEpoch) noexcept
{
	if (observedEpoch) *observedEpoch = 0;
	if (!_useSourceReader.load(std::memory_order_acquire)
		|| !_useMediaSessionAudioCompanion)
	{
		return CompanionSessionObservation::Ignored;
	}
	std::scoped_lock lock(_playbackEndMutex);
	auto& pending = kind == CompanionSessionControlKind::Pause
		? _pendingCompanionSessionPauseEpochs
		: _pendingCompanionSessionStopEpochs;
	if (pending.empty()) return CompanionSessionObservation::Ignored;
	const UINT64 commandEpoch = pending.front();
	pending.pop_front();
	if (commandEpoch != _playbackEndEpoch)
		return CompanionSessionObservation::Ignored;
	if (observedEpoch) *observedEpoch = commandEpoch;
	if (FAILED(eventStatus))
		return CompanionSessionObservation::FailedCurrent;
	_activeCompanionSessionEpoch = 0;
	return CompanionSessionObservation::Accepted;
}

MediaElement::CompanionSessionObservation
MediaElement::ObserveCompanionSessionStarted(
	HRESULT eventStatus, UINT64* observedEpoch) noexcept
{
	if (observedEpoch) *observedEpoch = 0;
	if (!_useSourceReader || !_useMediaSessionAudioCompanion)
		return CompanionSessionObservation::Ignored;
	std::scoped_lock lock(_playbackEndMutex);
	if (_pendingCompanionSessionStartEpochs.empty())
	{
		if (_activeCompanionSessionEpoch != _playbackEndEpoch)
			_activeCompanionSessionEpoch = 0;
		return CompanionSessionObservation::Ignored;
	}
	const UINT64 startedEpoch =
		_pendingCompanionSessionStartEpochs.front();
	_pendingCompanionSessionStartEpochs.pop_front();
	if (startedEpoch != _playbackEndEpoch)
	{
		if (_activeCompanionSessionEpoch != _playbackEndEpoch)
			_activeCompanionSessionEpoch = 0;
		return CompanionSessionObservation::Ignored;
	}
	if (observedEpoch) *observedEpoch = startedEpoch;
	if (FAILED(eventStatus))
	{
		_activeCompanionSessionEpoch = 0;
		if (_pendingSourceReaderWorkerStartEpoch == startedEpoch)
			_pendingSourceReaderWorkerStartEpoch = 0;
		return CompanionSessionObservation::FailedCurrent;
	}
	_activeCompanionSessionEpoch = startedEpoch;
	_statCompanionSessionStartedEvents.fetch_add(
		1, std::memory_order_relaxed);
	return CompanionSessionObservation::Accepted;
}

bool MediaElement::TakeSourceReaderWorkerStartForEpoch(
	UINT64 epoch) noexcept
{
	if (epoch == 0) return false;
	std::scoped_lock lock(_playbackEndMutex);
	if (epoch != _playbackEndEpoch
		|| _activeCompanionSessionEpoch != epoch
		|| _pendingSourceReaderWorkerStartEpoch != epoch)
	{
		return false;
	}
	_pendingSourceReaderWorkerStartEpoch = 0;
	return true;
}

void MediaElement::StartSourceReaderWorkerAfterCompanionStarted(
	UINT64 epoch)
{
	if (!CheckAccess())
	{
		std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
		DispatchToOwner([this, epoch, weakLifetime]
		{
			auto lifetime = weakLifetime.lock();
			if (!lifetime || !*lifetime) return;
			StartSourceReaderWorkerAfterCompanionStarted(epoch);
		});
		return;
	}

	auto transition = AcquirePlaybackTransition(
		PlaybackTransitionOrigin::Automatic);
	if (!transition
		|| !_mediaLoaded.load(std::memory_order_acquire)
		|| !_sourceReader
		|| !_useMediaSessionAudioCompanion.load(std::memory_order_acquire)
		|| _playState.load(std::memory_order_acquire)
			!= PlaybackState::Playing
		|| !TakeSourceReaderWorkerStartForEpoch(epoch))
	{
		return;
	}
	if (!_playThread.joinable())
	{
		_threadExit = false;
		_playThread = std::thread([this] { PlaybackThreadMain(); });
	}
	_threadPlaying.store(true, std::memory_order_release);
	WakePlaybackThread();
}

MediaElement::CompanionSessionObservation
MediaElement::ObserveCompanionSessionEnded(
	HRESULT eventStatus, UINT64* observedEpoch)
{
	if (observedEpoch) *observedEpoch = 0;
	UINT64 endedEpoch = 0;
	{
		std::scoped_lock lock(_playbackEndMutex);
		if (_activeCompanionSessionEpoch == _playbackEndEpoch
			&& (_playbackEndExpectedMask
				& PlaybackEndCompanionSession) != 0)
		{
			endedEpoch = _activeCompanionSessionEpoch;
		}
		_activeCompanionSessionEpoch = 0;
	}
	if (endedEpoch == 0) return CompanionSessionObservation::Ignored;
	if (observedEpoch) *observedEpoch = endedEpoch;
	if (FAILED(eventStatus))
		return CompanionSessionObservation::FailedCurrent;
	(void)SignalPlaybackEnd(PlaybackEndCompanionSession, endedEpoch);
	return CompanionSessionObservation::Accepted;
}

void MediaElement::HandleCompanionSessionFailure(
	HRESULT error, UINT64 expectedEpoch)
{
	if (expectedEpoch == 0) return;
	if (SUCCEEDED(error)) error = E_FAIL;
	if (!CheckAccess())
	{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
		DispatchToOwner([this, error, expectedEpoch, weakLifetime]
		{
			auto lifetime = weakLifetime.lock();
			if (!lifetime || !*lifetime) return;
			HandleCompanionSessionFailure(error, expectedEpoch);
		});
		return;
	}

	// The session callback is asynchronous.  A Seek/Stop/Load can advance the
	// playback transaction before this owner-thread continuation runs; an old
	// failure must never stop or report against the replacement transaction.
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (CurrentPlaybackEndEpoch() != expectedEpoch) return;

	// Publish failure only after the old reader transaction has left
	// ReadSample.  An OnMediaFailed handler may immediately retry Play, and it
	// must never race the old worker clearing the new run's playing flag.
	BeginPlaybackQuiescence();
	_needSyncReset.store(true, std::memory_order_release);
	DeferredPlaybackNotifications notifications{};
	CommitTerminalSourceReaderFailure(error, notifications);
	CompletePlaybackQuiescence();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications), true);
}

void MediaElement::HandleSourceReaderFailure(
	HRESULT error, UINT64 expectedEpoch,
	UINT64 expectedExplicitCommandGeneration,
	UINT64 expectedMediaLoadGeneration)
{
	if (expectedEpoch == 0 || expectedExplicitCommandGeneration == 0
		|| expectedMediaLoadGeneration == 0)
	{
		return;
	}
	if (SUCCEEDED(error)) error = E_FAIL;
	if (!CheckAccess())
	{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
		DispatchToOwner([this, error, expectedEpoch,
			expectedExplicitCommandGeneration,
			expectedMediaLoadGeneration, weakLifetime]
		{
			auto lifetime = weakLifetime.lock();
			if (!lifetime || !*lifetime) return;
			HandleSourceReaderFailure(
				error, expectedEpoch,
				expectedExplicitCommandGeneration,
				expectedMediaLoadGeneration);
		});
		return;
	}

	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (CurrentPlaybackEndEpoch() != expectedEpoch
		|| CurrentExplicitPlaybackCommandGeneration()
			!= expectedExplicitCommandGeneration
		|| CurrentMediaLoadGeneration() != expectedMediaLoadGeneration)
	{
		return;
	}
	BeginPlaybackQuiescence();
	_needSyncReset.store(true, std::memory_order_release);
	DeferredPlaybackNotifications notifications{};
	CommitTerminalSourceReaderFailure(error, notifications);
	CompletePlaybackQuiescence();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications), true);
}

MediaElement::StandaloneSessionCommandToken
MediaElement::QueueStandaloneSessionCommand(
	StandaloneSessionCommandKind kind) noexcept
{
	StandaloneSessionCommandToken token{};
	if (_useSourceReader.load(std::memory_order_acquire)) return token;
	token.Kind = kind;
	token.ExplicitCommandGeneration =
		CurrentExplicitPlaybackCommandGeneration();
	token.MediaLoadGeneration = CurrentMediaLoadGeneration();
	std::scoped_lock lock(_sessionStateMutex);
	++_nextStandaloneSessionCommandSequence;
	if (_nextStandaloneSessionCommandSequence == 0)
		++_nextStandaloneSessionCommandSequence;
	token.Sequence = _nextStandaloneSessionCommandSequence;
	_pendingStandaloneSessionCommands.push_back(token);
	return token;
}

void MediaElement::CancelStandaloneSessionCommand(
	const StandaloneSessionCommandToken& token) noexcept
{
	if (token.Sequence == 0) return;
	std::scoped_lock lock(_sessionStateMutex);
	const auto found = std::find_if(
		_pendingStandaloneSessionCommands.rbegin(),
		_pendingStandaloneSessionCommands.rend(),
		[&token](const StandaloneSessionCommandToken& pending)
		{
			return pending.Sequence == token.Sequence;
		});
	if (found != _pendingStandaloneSessionCommands.rend())
		_pendingStandaloneSessionCommands.erase(std::next(found).base());
}

void MediaElement::RestoreStandaloneSessionIdentityAfterCommandFailure(
	const StandaloneSessionCommandToken& token) noexcept
{
	if (token.Sequence == 0) return;
	std::scoped_lock lock(_sessionStateMutex);
	const auto found = std::find_if(
		_pendingStandaloneSessionCommands.rbegin(),
		_pendingStandaloneSessionCommands.rend(),
		[&token](const StandaloneSessionCommandToken& pending)
		{
			return pending.Sequence == token.Sequence;
		});
	if (found != _pendingStandaloneSessionCommands.rend())
		_pendingStandaloneSessionCommands.erase(std::next(found).base());

	if (token.MediaLoadGeneration != CurrentMediaLoadGeneration()
		|| token.ExplicitCommandGeneration
			!= CurrentExplicitPlaybackCommandGeneration())
	{
		return;
	}
	auto restorePriorIdentity = [&token](StandaloneSessionCommandToken& prior)
	{
		if (prior.Sequence != 0 && prior.Sequence < token.Sequence
			&& prior.MediaLoadGeneration == token.MediaLoadGeneration)
		{
			prior.ExplicitCommandGeneration =
				token.ExplicitCommandGeneration;
		}
	};
	for (auto& pending : _pendingStandaloneSessionCommands)
		restorePriorIdentity(pending);
	restorePriorIdentity(_activeStandaloneSessionPlayback);
	restorePriorIdentity(_completedStandaloneSessionPlayback);
}

void MediaElement::CommitStandaloneSessionCommandSuccess(
	const StandaloneSessionCommandToken& token) noexcept
{
	if (token.Sequence == 0) return;
	std::scoped_lock lock(_sessionStateMutex);
	// IMFMediaSession commands are asynchronous.  S_OK only means accepted;
	// MESessionStarted/Paused/Stopped is the point at which the active identity
	// may change.  Publishing a Start here lets an old queued MESessionEnded be
	// mistaken for the newly requested run.  A very short session may also have
	// delivered Started and Ended synchronously before Start returned; preserve
	// that completion when it belongs to this exact command.
	if (_completedStandaloneSessionPlayback.Sequence != token.Sequence
		|| _completedStandaloneSessionPlayback.ExplicitCommandGeneration
			!= token.ExplicitCommandGeneration
		|| _completedStandaloneSessionPlayback.MediaLoadGeneration
			!= token.MediaLoadGeneration)
	{
		_completedStandaloneSessionPlayback = {};
	}
}

MediaElement::CompanionSessionObservation
MediaElement::ObserveStandaloneSessionCommand(
	StandaloneSessionCommandKind kind, HRESULT eventStatus,
	StandaloneSessionCommandToken* observedToken) noexcept
{
	if (observedToken) *observedToken = {};
	StandaloneSessionCommandToken token{};
	{
		std::scoped_lock lock(_sessionStateMutex);
		const auto found = std::find_if(
			_pendingStandaloneSessionCommands.begin(),
			_pendingStandaloneSessionCommands.end(),
			[kind](const StandaloneSessionCommandToken& pending)
			{
				return pending.Kind == kind;
			});
		if (found == _pendingStandaloneSessionCommands.end())
			return CompanionSessionObservation::Ignored;
		token = *found;
		_pendingStandaloneSessionCommands.erase(found);
		if (token.ExplicitCommandGeneration
				!= CurrentExplicitPlaybackCommandGeneration()
			|| token.MediaLoadGeneration != CurrentMediaLoadGeneration())
		{
			return CompanionSessionObservation::Ignored;
		}
		if (observedToken) *observedToken = token;
		if (FAILED(eventStatus))
			return CompanionSessionObservation::FailedCurrent;
		if (kind == StandaloneSessionCommandKind::Start)
		{
			_activeStandaloneSessionPlayback = token;
		}
		else
		{
			_activeStandaloneSessionPlayback = {};
		}
		_completedStandaloneSessionPlayback = {};
	}
	return CompanionSessionObservation::Accepted;
}

MediaElement::CompanionSessionObservation
MediaElement::ObserveStandaloneSessionEnded(
	HRESULT eventStatus,
	StandaloneSessionCommandToken* observedToken) noexcept
{
	if (observedToken) *observedToken = {};
	std::scoped_lock lock(_sessionStateMutex);
	const StandaloneSessionCommandToken token =
		_activeStandaloneSessionPlayback;
	if (token.Sequence == 0
		|| token.ExplicitCommandGeneration
			!= CurrentExplicitPlaybackCommandGeneration()
		|| token.MediaLoadGeneration != CurrentMediaLoadGeneration()
		|| std::any_of(
			_pendingStandaloneSessionCommands.begin(),
			_pendingStandaloneSessionCommands.end(),
			[&token](const StandaloneSessionCommandToken& pending)
			{
				return pending.Kind
						== StandaloneSessionCommandKind::Start
					&& pending.Sequence >= token.Sequence;
			}))
	{
		return CompanionSessionObservation::Ignored;
	}
	_activeStandaloneSessionPlayback = {};
	if (observedToken) *observedToken = token;
	if (FAILED(eventStatus))
	{
		_completedStandaloneSessionPlayback = {};
		return CompanionSessionObservation::FailedCurrent;
	}
	_completedStandaloneSessionPlayback = token;
	return CompanionSessionObservation::Accepted;
}

bool MediaElement::IsStandaloneSessionCompletionCurrent(
	const StandaloneSessionCommandToken& token) const noexcept
{
	if (token.Sequence == 0
		|| token.MediaLoadGeneration != CurrentMediaLoadGeneration())
	{
		return false;
	}
	std::scoped_lock lock(_sessionStateMutex);
	return _completedStandaloneSessionPlayback.Sequence == token.Sequence
		&& _completedStandaloneSessionPlayback.ExplicitCommandGeneration
			== token.ExplicitCommandGeneration
		&& _activeStandaloneSessionPlayback.Sequence == 0;
}

MediaElement::StandaloneSessionCommandToken
MediaElement::CaptureStandaloneSessionFailureToken() const noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	StandaloneSessionCommandToken token =
		_activeStandaloneSessionPlayback;
	for (const auto& pending : _pendingStandaloneSessionCommands)
	{
		if (token.Sequence == 0 || pending.Sequence < token.Sequence)
		{
			token = pending;
		}
	}
	return token;
}

MediaElement::StandaloneSessionCommandToken
MediaElement::CaptureStandaloneSessionCompletionToken() const noexcept
{
	std::scoped_lock lock(_sessionStateMutex);
	const auto token = _completedStandaloneSessionPlayback;
	if (token.Sequence == 0
		|| token.ExplicitCommandGeneration
			!= CurrentExplicitPlaybackCommandGeneration()
		|| token.MediaLoadGeneration != CurrentMediaLoadGeneration())
	{
		return {};
	}
	return token;
}

void MediaElement::HandleStandaloneSessionFailure(
	HRESULT error, StandaloneSessionCommandToken token)
{
	if (token.Sequence == 0) return;
	if (SUCCEEDED(error)) error = E_FAIL;
	if (!CheckAccess())
	{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
		DispatchToOwner([this, error, token, weakLifetime]
		{
			auto lifetime = weakLifetime.lock();
			if (!lifetime || !*lifetime) return;
			HandleStandaloneSessionFailure(error, token);
		});
		return;
	}

	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (token.ExplicitCommandGeneration
			!= CurrentExplicitPlaybackCommandGeneration()
		|| token.MediaLoadGeneration != CurrentMediaLoadGeneration())
	{
		return;
	}
	BeginPlaybackQuiescence();
	CompletePlaybackQuiescence();
	ReportMediaFailure(error, true);
}

void MediaElement::QueueStandaloneSessionCompletion(
	StandaloneSessionCommandToken token)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner([this, token, weakLifetime]() mutable
	{
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;

		DeferredPlaybackNotifications notifications{};
		{
			std::unique_lock<std::recursive_mutex> commandLock(
				_playbackCommandMutex);
			if (!_mediaLoaded.load(std::memory_order_acquire)
				|| CurrentExplicitPlaybackCommandGeneration()
					!= token.ExplicitCommandGeneration
				|| CurrentMediaLoadGeneration()
					!= token.MediaLoadGeneration
				|| !IsStandaloneSessionCompletionCurrent(token))
			{
				return;
			}
			notifications.ExpectedExplicitCommandGeneration =
				token.ExplicitCommandGeneration;
			notifications.ExpectedMediaLoadGeneration =
				token.MediaLoadGeneration;
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Stopped, notifications.OldState);
			notifications.NewState = PlaybackState::Stopped;
		}

		RaiseDeferredPlaybackNotifications(std::move(notifications));
		lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime
			|| CurrentExplicitPlaybackCommandGeneration()
				!= token.ExplicitCommandGeneration
			|| CurrentMediaLoadGeneration()
				!= token.MediaLoadGeneration
			|| !IsStandaloneSessionCompletionCurrent(token))
		{
			return;
		}
		FireMediaEnded();
		lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime
			|| CurrentExplicitPlaybackCommandGeneration()
				!= token.ExplicitCommandGeneration
			|| CurrentMediaLoadGeneration()
				!= token.MediaLoadGeneration
			|| !IsStandaloneSessionCompletionCurrent(token))
		{
			return;
		}
		if (_loop.load(std::memory_order_acquire))
		{
			(void)TryRestartAfterMediaEnded(
				token.ExplicitCommandGeneration,
				token.MediaLoadGeneration, 0, token.Sequence);
		}
	});
}

bool MediaElement::SignalPlaybackEnd(UINT8 streamMask, UINT64 epoch)
{
	bool queueCompletion = false;
	{
		std::scoped_lock lock(_playbackEndMutex);
		if (epoch == 0 || epoch != _playbackEndEpoch
			|| (streamMask & _playbackEndExpectedMask) == 0)
		{
			return false;
		}
		_playbackEndObservedMask |=
			streamMask & _playbackEndExpectedMask;
		if (_playbackEndExpectedMask != 0
			&& _playbackEndObservedMask == _playbackEndExpectedMask
			&& !_playbackEndCompletionQueued)
		{
			_playbackEndCompletionQueued = true;
			queueCompletion = true;
		}
	}
	if (queueCompletion)
	{
		QueuePlaybackEndCompletion(
			epoch, CurrentExplicitPlaybackCommandGeneration(),
			CurrentMediaLoadGeneration());
	}
	return queueCompletion;
}

bool MediaElement::IsPlaybackEndCompletionCurrent(UINT64 epoch) const noexcept
{
	std::scoped_lock lock(_playbackEndMutex);
	return epoch != 0 && epoch == _playbackEndEpoch
		&& _playbackEndCompletionQueued
		&& _playbackEndExpectedMask != 0
		&& _playbackEndObservedMask == _playbackEndExpectedMask;
}

void MediaElement::QueuePlaybackEndCompletion(
	UINT64 epoch, UINT64 explicitCommandGeneration,
	UINT64 mediaLoadGeneration)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner(
		[this, epoch, explicitCommandGeneration,
			mediaLoadGeneration, weakLifetime]() mutable
	{
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;

		DeferredPlaybackNotifications notifications{};
		{
			std::unique_lock<std::recursive_mutex> commandLock(
				_playbackCommandMutex);
			if (!_mediaLoaded.load(std::memory_order_acquire)
				|| !IsPlaybackEndCompletionCurrent(epoch))
			{
				return;
			}
			if (CurrentExplicitPlaybackCommandGeneration()
					!= explicitCommandGeneration
				|| CurrentMediaLoadGeneration() != mediaLoadGeneration)
			{
				return;
			}
			notifications.ExpectedExplicitCommandGeneration =
				explicitCommandGeneration;
			notifications.ExpectedMediaLoadGeneration =
				mediaLoadGeneration;
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Stopped, notifications.OldState);
			notifications.NewState = PlaybackState::Stopped;
		}

		RaiseDeferredPlaybackNotifications(std::move(notifications));
		lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime
			|| CurrentExplicitPlaybackCommandGeneration()
				!= explicitCommandGeneration
			|| CurrentMediaLoadGeneration() != mediaLoadGeneration
			|| !IsPlaybackEndCompletionCurrent(epoch))
		{
			return;
		}

		FireMediaEnded();
		lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime
			|| CurrentExplicitPlaybackCommandGeneration()
				!= explicitCommandGeneration
			|| CurrentMediaLoadGeneration() != mediaLoadGeneration
			|| !IsPlaybackEndCompletionCurrent(epoch))
		{
			return;
		}

		// Event handlers may close/seek the player or disable Loop.  Revalidate
		// the captured epoch before the sole automatic restart transaction.
		if (_loop.load(std::memory_order_acquire)
			&& IsPlaybackEndCompletionCurrent(epoch))
		{
			(void)TryRestartAfterMediaEnded(
				explicitCommandGeneration, mediaLoadGeneration,
				epoch, 0);
		}
	});
}

void MediaElement::SetObservedPosition(
	double value, bool notify, bool forceEvent)
{
	double committedValue = 0.0;
	if (CommitObservedPosition(
		value, notify, forceEvent, committedValue))
	{
		FirePositionChanged(committedValue);
	}
}

bool MediaElement::CommitObservedPosition(
	double value, bool notify, bool forceEvent,
	double& committedValue) noexcept
{
	if (!std::isfinite(value)) return false;
	value = (std::max)(0.0, value);
	if (_duration > 0.0) value = (std::min)(value, _duration);
	committedValue = value;
	const double oldValue = _position.exchange(
		value, std::memory_order_acq_rel);
	const bool changed = std::fabs(value - oldValue) > 1e-9;
	return notify && (changed || forceEvent);
}

void MediaElement::RaiseDeferredPlaybackNotifications(
	DeferredPlaybackNotifications notifications, bool deferOnOwner)
{
	if (!notifications.StateChanged && !notifications.PositionChanged
		&& !notifications.MediaOpened && !notifications.MediaFailed
		&& !notifications.MediaError)
	{
		return;
	}
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	auto callback = [this, notifications, weakLifetime]() mutable
	{
		auto isAlive = [&weakLifetime]()
		{
			auto lifetime = weakLifetime.lock();
			return lifetime && *lifetime;
		};
		auto isCurrent = [this, &notifications, &isAlive]()
		{
			if (!isAlive()) return false;
			return (notifications.ExpectedExplicitCommandGeneration == 0
				|| CurrentExplicitPlaybackCommandGeneration()
					== notifications.ExpectedExplicitCommandGeneration)
				&& (notifications.ExpectedMediaLoadGeneration == 0
					|| CurrentMediaLoadGeneration()
						== notifications.ExpectedMediaLoadGeneration);
		};
		auto raisePosition = [this, &notifications, &isCurrent]()
		{
			if (!notifications.PositionChanged || !isCurrent()) return false;
			cui::framework::EventAccess::Raise(
				OnPositionChanged, this, notifications.Position);
			return isCurrent();
		};
		auto raiseState = [this, &notifications, &isCurrent]()
		{
			if (!notifications.StateChanged || !isCurrent()) return false;
			if (notifications.StateVisualInvalidationNeeded)
				RequestVisualInvalidation();
			cui::framework::EventAccess::Raise(
				OnPlaybackStateChanged, this,
				notifications.OldState, notifications.NewState);
			return isCurrent();
		};

		if (notifications.PositionFirst)
		{
			if (notifications.PositionChanged && !raisePosition()) return;
			if (notifications.StateChanged && !raiseState()) return;
		}
		else
		{
			if (notifications.StateChanged && !raiseState()) return;
			if (notifications.PositionChanged && !raisePosition()) return;
		}
		if (notifications.MediaOpened)
		{
			if (!isCurrent()) return;
			cui::framework::EventAccess::Raise(OnMediaOpened, this);
			if (!isCurrent()) return;
		}
		if (notifications.MediaFailed)
		{
			if (!isCurrent()) return;
			RequestVisualInvalidation();
			cui::framework::EventAccess::Raise(OnMediaFailed, this);
			if (!isCurrent()) return;
		}
		if (notifications.MediaError)
		{
			if (!isCurrent()) return;
			cui::framework::EventAccess::Raise(
				OnMediaError, this, notifications.Error);
			if (!isCurrent()) return;
		}
		if (notifications.ApplyLoadedBehavior)
		{
			if (!isAlive()) return;
			(void)TryApplyLoadedBehaviorAfterOpen(
				notifications.LoadedBehaviorExplicitCommandGeneration,
				notifications.ExpectedMediaLoadGeneration);
		}
	};
	if (deferOnOwner)
	{
		// PostToUIThread can report a PostMessage failure after retaining the
		// callback in its queue.  Never fall back to inline dispatch here: doing
		// so could both reenter a live command transaction and later deliver the
		// same callback a second time when the queue is drained.
		(void)TryPost(std::move(callback));
		return;
	}
	DispatchToOwner(std::move(callback));
}

void MediaElement::ReportMediaFailure(
	HRESULT error, bool deferOnOwner, bool stopPlaybackState)
{
	DeferredPlaybackNotifications notifications{};
	CommitMediaFailure(error, notifications, stopPlaybackState);
	// Failure can be discovered inside a larger Load/Seek transaction.  Always
	// post the user callbacks so reentrant recovery cannot resume inside that
	// partially committed command or destroy the object under its stack frame.
	RaiseDeferredPlaybackNotifications(
		std::move(notifications), deferOnOwner);
}

void MediaElement::CommitMediaFailure(
	HRESULT error, DeferredPlaybackNotifications& notifications,
	bool stopPlaybackState)
{
	if (SUCCEEDED(error)) error = E_FAIL;
	_lastMfError.store(error);
	if (stopPlaybackState)
	{
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Stopped, notifications.OldState);
		notifications.NewState = PlaybackState::Stopped;
	}
	notifications.MediaFailed = true;
	notifications.MediaError = true;
	notifications.Error = error;
	if (notifications.ExpectedExplicitCommandGeneration == 0)
	{
		notifications.ExpectedExplicitCommandGeneration =
			CurrentExplicitPlaybackCommandGeneration();
	}
	if (notifications.ExpectedMediaLoadGeneration == 0)
	{
		notifications.ExpectedMediaLoadGeneration =
			CurrentMediaLoadGeneration();
	}
}

void MediaElement::CommitTerminalSourceReaderFailure(
	HRESULT error, DeferredPlaybackNotifications& notifications)
{
	if (SUCCEEDED(error)) error = E_FAIL;
	_lastMfError.store(error);
	notifications.ExpectedExplicitCommandGeneration =
		CurrentExplicitPlaybackCommandGeneration();
	notifications.ExpectedMediaLoadGeneration =
		CurrentMediaLoadGeneration();
	_mediaLoaded.store(false, std::memory_order_release);
	StopSourceReaderPlayback(true);
	notifications.StateChanged = CommitPlaybackState(
		PlaybackState::Stopped, notifications.OldState);
	notifications.NewState = PlaybackState::Stopped;
	notifications.MediaFailed = true;
	notifications.MediaError = true;
	notifications.Error = error;
}

void MediaElement::DispatchToOwner(std::function<void()> callback)
{
	if (!callback) return;
	if (CheckAccess())
	{
		callback();
		return;
	}
	(void)TryPost(std::move(callback));
}

void MediaElement::RequestVisualInvalidation()
{
	_statVisualInvalidationRequests.fetch_add(1, std::memory_order_relaxed);
	if (CheckAccess())
	{
		_visualInvalidationPending.store(false, std::memory_order_release);
		Control::InvalidateVisual();
		return;
	}
	bool expected = false;
	if (!_visualInvalidationPending.compare_exchange_strong(
		expected, true, std::memory_order_acq_rel))
	{
		_statCoalescedVisualInvalidations.fetch_add(
			1, std::memory_order_relaxed);
		return;
	}
	std::weak_ptr<std::atomic_bool> weakLifetime = LifetimeToken();
	if (!TryPost([this, weakLifetime]
	{
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		_visualInvalidationPending.store(false, std::memory_order_release);
		Control::InvalidateVisual();
	}))
	{
		_visualInvalidationPending.store(false, std::memory_order_release);
	}
}

std::vector<uint8_t> MediaElement::AcquireVideoFrameBuffer()
{
	std::vector<uint8_t> frame;
	std::scoped_lock lock(_videoFrameMutex);
	frame.swap(_videoFrameSpare);
	return frame;
}

void MediaElement::PublishVideoFrame(
	std::vector<uint8_t>&& frame, UINT32 stride, SIZE videoSize)
{
	if (frame.empty() || stride == 0
		|| videoSize.cx <= 0 || videoSize.cy <= 0)
		return;
	bool overwritten = false;
	ComPtr<IMFSample> replacedGpuSample;
	{
		std::scoped_lock lock(_videoFrameMutex);
		overwritten = _videoFrameReady || _gpuVideoSampleReady;
		replacedGpuSample = std::move(_gpuVideoSample);
		_gpuVideoSampleGeneration = 0;
		_gpuVideoSampleReady = false;
		_videoFrame.swap(frame);
		_videoFrameStride = stride;
		_videoFrameVideoSize = videoSize;
		_videoFrameReady = true;
		if (frame.capacity() > _videoFrameSpare.capacity())
			_videoFrameSpare.swap(frame);
	}
	if (overwritten)
		_statOverwrittenVideoFrames.fetch_add(1, std::memory_order_relaxed);
	RequestVisualInvalidation();
}

void MediaElement::RecycleVideoFrame(std::vector<uint8_t>& frame)
{
	if (frame.capacity() == 0) return;
	std::scoped_lock lock(_videoFrameMutex);
	if (frame.capacity() > _videoFrameSpare.capacity())
		_videoFrameSpare.swap(frame);
}

void MediaElement::PreservePresentedVideoFrame(
	std::vector<uint8_t>& frame, UINT32 stride, SIZE videoSize)
{
	if (frame.empty() || stride == 0
		|| videoSize.cx <= 0 || videoSize.cy <= 0)
	{
		RecycleVideoFrame(frame);
		return;
	}
	std::scoped_lock lock(_videoFrameMutex);
	_lastPresentedGpuSample.Reset();
	_lastPresentedGpuSampleGeneration = 0;
	_lastPresentedVideoFrame.swap(frame);
	_lastPresentedVideoFrameStride = stride;
	_lastPresentedVideoFrameVideoSize = videoSize;
	if (frame.capacity() > _videoFrameSpare.capacity())
		_videoFrameSpare.swap(frame);
}

void MediaElement::PreserveGpuPresentedFrameForRecovery() noexcept
{
	try
	{
		if (!_videoBitmapUsesGpuSurface
			|| _gpuOutputSlot >= GpuOutputBufferCount
			|| !_gpuOutputTextures[_gpuOutputSlot]) return;
		ComPtr<ID3D11Texture2D> source =
			_gpuOutputTextures[_gpuOutputSlot];
		ComPtr<ID3D11Device> device;
		source->GetDevice(device.GetAddressOf());
		if (!device) return;
		ComPtr<ID3D11DeviceContext> context;
		device->GetImmediateContext(context.GetAddressOf());
		if (!context) return;

		D3D11_TEXTURE2D_DESC description{};
		source->GetDesc(&description);
		if (description.Width == 0 || description.Height == 0
			|| description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) return;
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;
		ComPtr<ID3D11Texture2D> staging;
		if (FAILED(device->CreateTexture2D(
			&description, nullptr, staging.GetAddressOf())) || !staging) return;
		context->CopyResource(staging.Get(), source.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(
			staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)) || !mapped.pData)
			return;
		struct UnmapScope final
		{
			ID3D11DeviceContext* Context = nullptr;
			ID3D11Texture2D* Texture = nullptr;
			~UnmapScope()
			{
				if (Context && Texture) Context->Unmap(Texture, 0);
			}
		} unmap{ context.Get(), staging.Get() };

		const UINT32 stride = description.Width * 4u;
		std::vector<uint8_t> snapshot(
			static_cast<size_t>(stride) * description.Height);
		for (UINT32 row = 0; row < description.Height; ++row)
		{
			memcpy(snapshot.data() + static_cast<size_t>(row) * stride,
				static_cast<const uint8_t*>(mapped.pData)
					+ static_cast<size_t>(row) * mapped.RowPitch,
				stride);
		}
		std::scoped_lock lock(_videoFrameMutex);
		_lastPresentedVideoFrame = std::move(snapshot);
		_lastPresentedVideoFrameStride = stride;
		_lastPresentedVideoFrameVideoSize = SIZE{
			static_cast<LONG>(description.Width),
			static_cast<LONG>(description.Height) };
	}
	catch (...)
	{
		// Recovery snapshots are best effort; never make Pause throw.
	}
}

void MediaElement::ReleaseVideoFrameBuffers() noexcept
{
	ComPtr<IMFSample> releasedGpuSample;
	std::scoped_lock lock(_videoFrameMutex);
	releasedGpuSample = std::move(_gpuVideoSample);
	_lastPresentedGpuSample.Reset();
	_lastPresentedGpuSampleGeneration = 0;
	_gpuVideoSampleGeneration = 0;
	_gpuVideoSampleReady = false;
	std::vector<uint8_t>{}.swap(_videoFrame);
	std::vector<uint8_t>{}.swap(_videoFrameSpare);
	std::vector<uint8_t>{}.swap(_lastPresentedVideoFrame);
	_videoFrameReady = false;
	_videoFrameStride = 0;
	_videoFrameVideoSize = {};
	_lastPresentedVideoFrameStride = 0;
	_lastPresentedVideoFrameVideoSize = {};
	_videoStride = 0;
	_videoSubtype = GUID_NULL;
	_videoBytesPerPixel = 4;
	_videoBottomUp = false;
	_videoTransferMatrix = MFVideoTransferMatrix_Unknown;
	_videoNominalRange = MFNominalRange_Unknown;
	_videoD3DFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	_videoFrameSize = {};
	_videoSize = {};
	_videoPixelAspectNumerator = 1;
	_videoPixelAspectDenominator = 1;
	_videoCropX = 0;
	_videoCropY = 0;
}

void MediaElement::RefreshPresentationRateLimit() noexcept
{
	auto* presentationWindow = GetPresentationWindow();
	const HWND window = presentationWindow ? presentationWindow->Handle : nullptr;
	const HMONITOR monitor = window
		? MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) : nullptr;
	const LARGE_INTEGER now = QpcNow();
	const LARGE_INTEGER frequency = QpcFreq();
	if (monitor == _presentationRateMonitor
		&& _lastPresentationRateRefreshQpc > 0
		&& frequency.QuadPart > 0
		&& now.QuadPart - _lastPresentationRateRefreshQpc < frequency.QuadPart)
		return;
	_presentationRateMonitor = monitor;
	_lastPresentationRateRefreshQpc = now.QuadPart;
	_videoPresentationRateLimitHz.store(
		QueryPresentationRateLimitHz(window), std::memory_order_relaxed);
}

namespace
{
	UINT64 PackAdapterLuid(ID3D11Device* device) noexcept
	{
		if (!device) return 0;
		ComPtr<IDXGIDevice> dxgiDevice;
		ComPtr<IDXGIAdapter> adapter;
		DXGI_ADAPTER_DESC description{};
		if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))
			|| !dxgiDevice
			|| FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))
			|| !adapter
			|| FAILED(adapter->GetDesc(&description)))
			return 0;
		return (static_cast<UINT64>(
			static_cast<UINT32>(description.AdapterLuid.HighPart)) << 32)
			| static_cast<UINT64>(description.AdapterLuid.LowPart);
	}
}

bool MediaElement::ConfigureSourceReaderDxgiManager(IMFAttributes* attributes)
{
	if (!attributes || !_enableHardwareDecode || !_enableDxgiVideoOutput
		|| !_preferNv12VideoOutput)
		return false;

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	GraphicsSharedD3DDeviceInfo deviceInfo{};
	HRESULT hr = Graphics_AcquireSharedD3DDevice(
		device.GetAddressOf(), context.GetAddressOf(), nullptr, nullptr,
		&deviceInfo);
	if (FAILED(hr) || !device || !context || !deviceInfo.SupportsVideo
		|| !deviceInfo.IsHardware)
		return false;

	UINT resetToken = 0;
	ComPtr<IMFDXGIDeviceManager> manager;
	hr = MFCreateDXGIDeviceManager(&resetToken, manager.GetAddressOf());
	if (FAILED(hr) || !manager) return false;
	hr = manager->ResetDevice(device.Get(), resetToken);
	if (FAILED(hr)) return false;
	hr = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, manager.Get());
	if (FAILED(hr)) return false;

	{
		std::scoped_lock lock(_dxgiStateMutex);
		_dxgiDeviceManager = manager;
		_mediaD3DDevice = device;
		_mediaD3DContext = context;
		_dxgiDeviceResetToken = resetToken;
		_dxgiDeviceGeneration.store(
			deviceInfo.Generation, std::memory_order_release);
		_dxgiAdapterLuid.store(
			PackAdapterLuid(device.Get()), std::memory_order_release);
		_dxgiDeviceManagerActive.store(true, std::memory_order_release);
		_dxgiPresentationFailureGeneration.store(
			0, std::memory_order_release);
	}
	_consecutiveGpuSurfaceImportFailures = 0;
	_consecutiveCpuVideoBufferLockFailures = 0;
	return true;
}

void MediaElement::ReleaseSourceReaderDxgiManager() noexcept
{
	ComPtr<IMFDXGIDeviceManager> manager;
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	{
		std::scoped_lock lock(_dxgiStateMutex);
		manager = std::move(_dxgiDeviceManager);
		device = std::move(_mediaD3DDevice);
		context = std::move(_mediaD3DContext);
		_dxgiDeviceResetToken = 0;
		_dxgiDeviceManagerActive.store(false, std::memory_order_release);
		_dxgiDeviceGeneration.store(0, std::memory_order_release);
		_dxgiAdapterLuid.store(0, std::memory_order_release);
		_dxgiPresentationFailureGeneration.store(
			0, std::memory_order_release);
	}
	_consecutiveGpuSurfaceImportFailures = 0;
	_consecutiveCpuVideoBufferLockFailures = 0;
}

bool MediaElement::TryRebindDxgiDeviceManager()
{
	if (!_dxgiDeviceManagerActive.load(std::memory_order_acquire))
		return false;

	ComPtr<ID3D11Device> currentDevice;
	ComPtr<IMFDXGIDeviceManager> manager;
	UINT resetToken = 0;
	UINT64 boundGeneration = 0;
	{
		std::scoped_lock lock(_dxgiStateMutex);
		currentDevice = _mediaD3DDevice;
		manager = _dxgiDeviceManager;
		resetToken = _dxgiDeviceResetToken;
		boundGeneration =
			_dxgiDeviceGeneration.load(std::memory_order_acquire);
	}
	if (!manager) return false;
	const UINT64 publishedGeneration =
		Graphics_GetSharedD3DDeviceGeneration();
	if (currentDevice && boundGeneration != 0
		&& publishedGeneration == boundGeneration
		&& currentDevice->GetDeviceRemovedReason() == S_OK)
	{
		return true;
	}

	ComPtr<ID3D11Device> replacementDevice;
	ComPtr<ID3D11DeviceContext> replacementContext;
	GraphicsSharedD3DDeviceInfo deviceInfo{};
	HRESULT hr = Graphics_AcquireSharedD3DDevice(
		replacementDevice.GetAddressOf(), replacementContext.GetAddressOf(),
		nullptr, nullptr, &deviceInfo);
	if (FAILED(hr) || !replacementDevice || !replacementContext
		|| !deviceInfo.SupportsVideo || !deviceInfo.IsHardware)
		return false;
	if (currentDevice
		&& currentDevice->GetDeviceRemovedReason() == S_OK
		&& currentDevice.Get() == replacementDevice.Get()
		&& boundGeneration == deviceInfo.Generation)
	{
		return true;
	}
	hr = manager->ResetDevice(replacementDevice.Get(), resetToken);
	if (FAILED(hr)) return false;

	ComPtr<IMFSample> staleSample;
	{
		// Publish device, context and generation as one epoch, while clearing the
		// old-device mailbox under the same lock order used by the producer.
		std::scoped_lock lock(_dxgiStateMutex, _videoFrameMutex);
		if (_dxgiDeviceManager.Get() != manager.Get()
			|| _dxgiDeviceResetToken != resetToken)
			return false;
		_mediaD3DDevice = replacementDevice;
		_mediaD3DContext = replacementContext;
		_dxgiDeviceGeneration.store(
			deviceInfo.Generation, std::memory_order_release);
		_dxgiAdapterLuid.store(
			PackAdapterLuid(replacementDevice.Get()),
			std::memory_order_release);
		_dxgiPresentationFailureGeneration.store(
			0, std::memory_order_release);
		staleSample = std::move(_gpuVideoSample);
		_gpuVideoSampleGeneration = 0;
		_gpuVideoSampleReady = false;
	}
	_consecutiveGpuSurfaceImportFailures = 0;
	_statGpuDeviceRebinds.fetch_add(1, std::memory_order_relaxed);
	ReleaseGpuPresentationResources(true);
	return true;
}

MediaElement::DxgiVideoSampleDisposition
MediaElement::TryPublishDxgiVideoSample(IMFSample* sample)
{
	if (!sample) return DxgiVideoSampleDisposition::CpuFallbackEligible;
	UINT64 currentGeneration = 0;
	UINT64 failedGeneration = 0;
	ComPtr<ID3D11Device> expectedDevice;
	{
		std::scoped_lock lock(_dxgiStateMutex);
		if (!_dxgiDeviceManagerActive.load(std::memory_order_acquire))
			return DxgiVideoSampleDisposition::CpuFallbackEligible;
		currentGeneration =
			_dxgiDeviceGeneration.load(std::memory_order_acquire);
		failedGeneration = _dxgiPresentationFailureGeneration.load(
			std::memory_order_acquire);
		expectedDevice = _mediaD3DDevice;
	}
	if (failedGeneration != 0)
	{
		if (failedGeneration == currentGeneration)
			return DxgiVideoSampleDisposition::CpuFallbackEligible;
		UINT64 expected = failedGeneration;
		(void)_dxgiPresentationFailureGeneration.compare_exchange_strong(
			expected, 0, std::memory_order_acq_rel);
	}

	DWORD bufferCount = 0;
	if (FAILED(sample->GetBufferCount(&bufferCount)))
		return DxgiVideoSampleDisposition::CpuFallbackEligible;
	ComPtr<ID3D11Texture2D> texture;
	for (DWORD index = 0; index < bufferCount && !texture; ++index)
	{
		ComPtr<IMFMediaBuffer> buffer;
		ComPtr<IMFDXGIBuffer> dxgiBuffer;
		if (FAILED(sample->GetBufferByIndex(index, buffer.GetAddressOf()))
			|| !buffer
			|| FAILED(buffer.As(&dxgiBuffer)) || !dxgiBuffer)
			continue;
		(void)dxgiBuffer->GetResource(
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(texture.GetAddressOf()));
	}
	if (!texture) return DxgiVideoSampleDisposition::CpuFallbackEligible;

	ComPtr<ID3D11Device> sampleDevice;
	texture->GetDevice(sampleDevice.GetAddressOf());
	if (!sampleDevice || !expectedDevice
		|| sampleDevice.Get() != expectedDevice.Get())
	{
		_statStaleGenerationFrames.fetch_add(1, std::memory_order_relaxed);
		return DxgiVideoSampleDisposition::DropStale;
	}

	ComPtr<IMFSample> retainedSample = sample;
	ComPtr<IMFSample> replacedSample;
	std::vector<uint8_t> replacedCpuFrame;
	bool overwritten = false;
	{
		// Revalidate the epoch while publishing so ResetDevice cannot clear a
		// newly published replacement-device sample, nor relabel an old sample.
		std::scoped_lock lock(_dxgiStateMutex, _videoFrameMutex);
		if (!_dxgiDeviceManagerActive.load(std::memory_order_acquire)
			|| _mediaD3DDevice.Get() != expectedDevice.Get()
			|| _dxgiDeviceGeneration.load(std::memory_order_acquire)
				!= currentGeneration)
		{
			_statStaleGenerationFrames.fetch_add(
				1, std::memory_order_relaxed);
			return DxgiVideoSampleDisposition::DropStale;
		}
		if (_dxgiPresentationFailureGeneration.load(
			std::memory_order_acquire) == currentGeneration)
			return DxgiVideoSampleDisposition::CpuFallbackEligible;
		overwritten = _gpuVideoSampleReady || _videoFrameReady;
		replacedSample = std::move(_gpuVideoSample);
		if (_videoFrameReady)
			replacedCpuFrame.swap(_videoFrame);
		_videoFrameReady = false;
		_videoFrameStride = 0;
		_videoFrameVideoSize = {};
		_gpuVideoSample = retainedSample;
		_gpuVideoSampleGeneration = currentGeneration;
		_gpuVideoSampleReady = true;
	}
	RecycleVideoFrame(replacedCpuFrame);
	_statDxgiVideoSamples.fetch_add(1, std::memory_order_relaxed);
	if (overwritten)
		_statOverwrittenVideoFrames.fetch_add(1, std::memory_order_relaxed);
	RequestVisualInvalidation();
	return DxgiVideoSampleDisposition::Published;
}

// ========== 跨线程事件封送助手 ==========
void MediaElement::FireMediaOpened()
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner([this, weakLifetime]() {
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		cui::framework::EventAccess::Raise(OnMediaOpened, this);
	});
}

void MediaElement::FireMediaEnded()
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner([this, weakLifetime]() {
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		cui::framework::EventAccess::Raise(OnMediaEnded, this);
	});
}

void MediaElement::FirePositionChanged(
	double value, UINT64 expectedPlaybackEpoch,
	UINT64 expectedExplicitCommandGeneration,
	UINT64 expectedMediaLoadGeneration)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner([this, value, expectedPlaybackEpoch,
		expectedExplicitCommandGeneration,
		expectedMediaLoadGeneration, weakLifetime]() {
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		if ((expectedPlaybackEpoch != 0
				&& CurrentPlaybackEndEpoch() != expectedPlaybackEpoch)
			|| (expectedExplicitCommandGeneration != 0
				&& CurrentExplicitPlaybackCommandGeneration()
					!= expectedExplicitCommandGeneration)
			|| (expectedMediaLoadGeneration != 0
				&& CurrentMediaLoadGeneration()
					!= expectedMediaLoadGeneration))
		{
			return;
		}
		cui::framework::EventAccess::Raise(OnPositionChanged, this, value);
	});
}

void MediaElement::FireMediaError(HRESULT error)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	DispatchToOwner([this, error, weakLifetime]() {
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		cui::framework::EventAccess::Raise(OnMediaError, this, error);
	});
}

MediaElement::MediaElement()
{
	_playbackWakeEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

cui::core::Size MediaElement::MeasureCore(
	const cui::core::Constraints& available)
{
	std::scoped_lock lock(_videoFrameMutex);
	if (!_hasVideo || _videoSize.cx <= 0 || _videoSize.cy <= 0)
		return available.Constrain({});
	const float aspect = _videoPixelAspectDenominator != 0
		? static_cast<float>(_videoPixelAspectNumerator)
			/ static_cast<float>(_videoPixelAspectDenominator)
		: 1.0f;
	const cui::core::Size natural{
		static_cast<float>(_videoSize.cx) * aspect,
		static_cast<float>(_videoSize.cy) };
	const auto scale = cui::layout::ComputeStretchScaleFactor(
		available.Normalized().maximum, natural, _stretch, _stretchDirection);
	return { natural.width * scale.width, natural.height * scale.height };
}

void MediaElement::ApplyLoadedBehaviorOnTree()
{
	if (!GetPresentationWindow()) return;
	if (_loadedBehavior == MediaState::Manual)
	{
		ApplyMediaState(MediaState::Manual);
		return;
	}
	if (_loadedBehavior == MediaState::Close)
	{
		CloseCore();
		return;
	}
	if (!_source.empty()
		&& (!_mediaLoaded.load(std::memory_order_acquire)
			|| _mediaFile != _source))
	{
		(void)LoadSourceCore(_source);
		return;
	}
	ApplyMediaState(_loadedBehavior);
}

void MediaElement::ApplyMediaState(MediaState behavior)
{
	if (behavior == MediaState::Manual)
		behavior = _requestedState.load(std::memory_order_acquire);
	switch (behavior)
	{
	case MediaState::Manual:
		break;
	case MediaState::Play:
		(void)TryPlayState();
		break;
	case MediaState::Close:
		CloseCore();
		break;
	case MediaState::Pause:
		(void)TryPauseState();
		break;
	case MediaState::Stop:
		(void)TryStopState();
		break;
	}
}

void MediaElement::OnPresentationWindowChanged(
	PresentationWindow* previousWindow,
	PresentationWindow* currentWindow)
{
	(void)previousWindow;
	if (!currentWindow)
	{
		ApplyMediaState(_unloadedBehavior);
		return;
	}
	ApplyLoadedBehaviorOnTree();
}

MediaElement::~MediaElement()
{
	// Invalidate derived callbacks before any MediaElement storage is torn down.
	// Base destructors do this too, but that is too late for queued callbacks
	// that could otherwise enter this half-destroyed derived object.
	InvalidateLifetimeToken();
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (_eventCallback) _eventCallback->DetachPlayer();
	_threadExit = true;
	_threadPlaying = false;
	WakePlaybackThread();
	if (_playThread.joinable())
		_playThread.join();
	ShutdownWasapi();
	ShutdownSourceReader();
	ReleaseResources();
	if (_mfStarted)
	{
		MFShutdown();
		_mfStarted = false;
	}
	if (_didCoInit)
	{
		::CoUninitialize();
		_didCoInit = false;
	}
	if (_playbackWakeEvent)
	{
		::CloseHandle(_playbackWakeEvent);
		_playbackWakeEvent = nullptr;
	}
}

void MediaElement::WakePlaybackThread() noexcept
{
	_threadCv.notify_all();
	if (_playbackWakeEvent) (void)::SetEvent(_playbackWakeEvent);
}

bool MediaElement::EnsureInitialized()
{
	if (_initialized) return true;
	if (_initializationAttempted) return false;

	_initializationAttempted = true;
	_initializationHr = InitializeMF();
	_initialized = SUCCEEDED(_initializationHr);
	if (!_initialized)
		_lastMfError.store(_initializationHr);
	return _initialized;
}

HRESULT MediaElement::InitializeMF()
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
	_mfStarted = true;

	return S_OK;
}

bool MediaElement::InitWasapi()
{
	return InitWasapiWithFormat(nullptr);
}

bool MediaElement::InitWasapiWithFormat(const WAVEFORMATEX* format)
{
	ShutdownWasapi();
	HRESULT hr = S_OK;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_mmDeviceEnumerator));
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: MMDeviceEnumerator", hr); return false; }

	hr = _mmDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &_audioDevice);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: GetDefaultAudioEndpoint", hr); return false; }

	hr = _audioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&_audioClient);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: Activate IAudioClient", hr); return false; }

	if (format)
	{
		WAVEFORMATEX* closest = nullptr;
		hr = _audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, format, &closest);
		if (hr == S_OK)
		{
			if (closest) CoTaskMemFree(closest);
			_audioMixFormat = CloneWaveFormat(format);
		}
		else if (hr == S_FALSE && closest)
		{
			_audioMixFormat = CloneWaveFormat(closest);
			CoTaskMemFree(closest);
		}
		else
		{
			if (closest) CoTaskMemFree(closest);
			DebugOutputHr(L"WASAPI: IsFormatSupported", hr);
			return false;
		}
		if (!_audioMixFormat) return false;
	}
	else
	{
		hr = _audioClient->GetMixFormat(&_audioMixFormat);
		if (FAILED(hr) || !_audioMixFormat) { DebugOutputHr(L"WASAPI: GetMixFormat", hr); return false; }
	}

	_audioReadyEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!_audioReadyEvent)
	{
		DebugOutputHr(L"WASAPI: CreateEvent", HRESULT_FROM_WIN32(GetLastError()));
		return false;
	}

	// Event-driven refill avoids a Sleep(1) polling loop on the same worker that
	// schedules video frames and performs time stretching.
	REFERENCE_TIME bufferDuration = 1000000; // 100ms
	hr = _audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		bufferDuration, 0, _audioMixFormat, nullptr);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: Initialize", hr); return false; }
	hr = _audioClient->SetEventHandle(_audioReadyEvent);
	if (FAILED(hr)) { DebugOutputHr(L"WASAPI: SetEventHandle", hr); return false; }

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

void MediaElement::ShutdownWasapi()
{
	if (_audioClient)
		(void)_audioClient->Stop();
	_audioRenderClient.Reset();
	_audioClient.Reset();
	if (_audioReadyEvent)
	{
		::CloseHandle(_audioReadyEvent);
		_audioReadyEvent = nullptr;
	}
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

bool MediaElement::ConfigureSourceReaderVideoType()
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

bool MediaElement::ConfigureSourceReaderAudioTypeFromMixFormat()
{
	if (!_sourceReader) return false;
	if (!_audioMixFormat) return false;
	_sourceReaderAudioSubtype = GUID_NULL;
	{
		ComPtr<IMFMediaType> currentType;
		const HRESULT currentTypeHr =
			_sourceReader->GetCurrentMediaType(_srAudioStream, &currentType);
		if (currentTypeHr == MF_E_INVALIDSTREAMNUMBER)
			return false;
		if (SUCCEEDED(currentTypeHr) && currentType)
			_sourceReaderAudioSubtype = GetAudioSubtypeFromMediaType(currentType.Get());
	}

	auto tryFormat = [&](const WAVEFORMATEX* waveFormat, const wchar_t* failContext) -> bool
	{
		WAVEFORMATEX* resolvedFormat = ResolveSharedModeWaveFormat(waveFormat);
		if (!resolvedFormat) return false;
		ComPtr<IMFMediaType> mt;
		if (FAILED(MFCreateMediaType(&mt)) || !mt) { CoTaskMemFree(resolvedFormat); return false; }
		if (!PopulateMfAudioTypeFromWaveFormat(mt.Get(), resolvedFormat)) { CoTaskMemFree(resolvedFormat); return false; }
		HRESULT hr = _sourceReader->SetCurrentMediaType(_srAudioStream, nullptr, mt.Get());
		if (FAILED(hr))
		{
			DebugOutputHr(failContext, hr);
			CoTaskMemFree(resolvedFormat);
			return false;
		}
		if (!InitWasapiWithFormat(resolvedFormat))
		{
			CoTaskMemFree(resolvedFormat);
			return false;
		}
		CoTaskMemFree(resolvedFormat);
		return true;
	};

	WAVEFORMATEX* originalMix = CloneWaveFormat(_audioMixFormat);
	if (!originalMix) return false;

	if (tryFormat(originalMix, L"SourceReader: SetCurrentMediaType(audio from mix) failed"))
	{
		CoTaskMemFree(originalMix);
		return true;
	}

	for (DWORD typeIndex = 0; ; typeIndex++)
	{
		ComPtr<IMFMediaType> nativeType;
		HRESULT nativeHr = _sourceReader->GetNativeMediaType(_srAudioStream, typeIndex, &nativeType);
		if (nativeHr == MF_E_NO_MORE_TYPES) break;
		// MF_E_INVALIDSTREAMNUMBER means the source has no audio stream.  More
		// generally, a failed enumeration call cannot become successful merely
		// by incrementing the media-type index; continuing here used to spin the
		// UI thread forever for video-only files.
		if (FAILED(nativeHr) || !nativeType) break;

		WAVEFORMATEX* nativeWave = nullptr;
		UINT32 nativeWaveSize = 0;
		if (FAILED(MFCreateWaveFormatExFromMFMediaType(nativeType.Get(), &nativeWave, &nativeWaveSize, MFWaveFormatExConvertFlag_Normal)) || !nativeWave)
			continue;

		WAVEFORMATEX* resolvedNative = ResolveSharedModeWaveFormat(nativeWave);
		if (resolvedNative && WaveFormatsEquivalent(nativeWave, resolvedNative))
		{
			if (SUCCEEDED(_sourceReader->SetCurrentMediaType(_srAudioStream, nullptr, nativeType.Get())) && InitWasapiWithFormat(nativeWave))
			{
				CoTaskMemFree(resolvedNative);
				CoTaskMemFree(nativeWave);
				CoTaskMemFree(originalMix);
				return true;
			}
		}
		if (resolvedNative) CoTaskMemFree(resolvedNative);
		CoTaskMemFree(nativeWave);
	}

	const WORD mixChannels = originalMix->nChannels ? originalMix->nChannels : 2;
	const DWORD mixRate = originalMix->nSamplesPerSec ? originalMix->nSamplesPerSec : 48000;
	WAVEFORMATEX candidates[8]{};
	FillSimpleWaveFormat(candidates[0], mixChannels, mixRate, 16, false);
	FillSimpleWaveFormat(candidates[1], 2, mixRate, 16, false);
	FillSimpleWaveFormat(candidates[2], 2, 48000, 16, false);
	FillSimpleWaveFormat(candidates[3], 2, 44100, 16, false);
	FillSimpleWaveFormat(candidates[4], 1, 44100, 16, false);
	FillSimpleWaveFormat(candidates[5], 1, 22050, 16, false);
	FillSimpleWaveFormat(candidates[6], 2, 48000, 32, true);
	FillSimpleWaveFormat(candidates[7], 2, 44100, 32, true);

	for (int i = 0; i < 8; i++)
	{
		if (tryFormat(&candidates[i], L"SourceReader: SetCurrentMediaType(audio fallback) failed"))
		{
			CoTaskMemFree(originalMix);
			return true;
		}
	}

	CoTaskMemFree(originalMix);
	return false;
}

void MediaElement::UpdateVideoFormatFromSourceReader()
{
	if (!_sourceReader) return;
	_videoFrameRateKnown.store(false, std::memory_order_relaxed);
	ComPtr<IMFMediaType> mt;
	if (FAILED(_sourceReader->GetCurrentMediaType(_srVideoStream, &mt)) || !mt) return;
	UINT32 frameRateNumerator = 0;
	UINT32 frameRateDenominator = 0;
	if (SUCCEEDED(MFGetAttributeRatio(
		mt.Get(), MF_MT_FRAME_RATE,
		&frameRateNumerator, &frameRateDenominator))
		&& frameRateNumerator > 0 && frameRateDenominator > 0)
	{
		const LONGLONG duration = std::max<LONGLONG>(1,
			(10'000'000LL * frameRateDenominator) / frameRateNumerator);
		_videoFrameDurationHns.store(duration, std::memory_order_relaxed);
		_videoFrameRateKnown.store(true, std::memory_order_relaxed);
	}
	
	UINT32 w = 0, h = 0;
	if (SUCCEEDED(MFGetAttributeSize(mt.Get(), MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0)
	{
		UINT32 pixelAspectNumerator = 1;
		UINT32 pixelAspectDenominator = 1;
		if (FAILED(MFGetAttributeRatio(
			mt.Get(), MF_MT_PIXEL_ASPECT_RATIO,
			&pixelAspectNumerator, &pixelAspectDenominator))
			|| pixelAspectNumerator == 0 || pixelAspectDenominator == 0)
		{
			pixelAspectNumerator = 1;
			pixelAspectDenominator = 1;
		}
		UINT32 cropX = 0, cropY = 0, visibleW = w, visibleH = h;
		ApplyVideoCropFromMediaType(mt.Get(), w, h, cropX, cropY, visibleW, visibleH);
		
		// 获取实际的stride（步长）
		UINT32 strideAttr = MFGetAttributeUINT32(mt.Get(), MF_MT_DEFAULT_STRIDE, 0);
		INT32 signedStride = (INT32)strideAttr;
		bool bottomUp = (signedStride < 0);
		UINT32 stride = bottomUp ? (UINT32)(-signedStride) : strideAttr;
		
		// 检查格式以确定正确的stride
		GUID subtype = GUID_NULL;
		UINT32 bytesPerPixel = 4;
		const bool hasSubtype = SUCCEEDED(mt->GetGUID(MF_MT_SUBTYPE, &subtype));
		const auto transferMatrix = static_cast<MFVideoTransferMatrix>(
			MFGetAttributeUINT32(mt.Get(), MF_MT_YUV_MATRIX,
				MFVideoTransferMatrix_Unknown));
		const auto nominalRange = static_cast<MFNominalRange>(
			MFGetAttributeUINT32(mt.Get(), MF_MT_VIDEO_NOMINAL_RANGE,
				MFNominalRange_Unknown));
		const auto interlaceMode = static_cast<MFVideoInterlaceMode>(
			MFGetAttributeUINT32(mt.Get(), MF_MT_INTERLACE_MODE,
				MFVideoInterlace_Progressive));
		D3D11_VIDEO_FRAME_FORMAT d3dFrameFormat =
			D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
		if (interlaceMode == MFVideoInterlace_FieldInterleavedUpperFirst
			|| interlaceMode == MFVideoInterlace_FieldSingleUpper)
			d3dFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
		else if (interlaceMode == MFVideoInterlace_FieldInterleavedLowerFirst
			|| interlaceMode == MFVideoInterlace_FieldSingleLower)
			d3dFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
		if (hasSubtype)
		{
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
		}
		else
		{
			// 无法获取格式，使用默认值
			if (stride == 0) stride = w * 4;
			bottomUp = false;
		}

		{
			std::scoped_lock lock(_videoFrameMutex);
			_videoFrameSize = SIZE{ (LONG)w, (LONG)h };
			_videoSize = SIZE{ (LONG)visibleW, (LONG)visibleH };
			_videoPixelAspectNumerator = pixelAspectNumerator;
			_videoPixelAspectDenominator = pixelAspectDenominator;
			_videoCropX = cropX;
			_videoCropY = cropY;
			_videoStride = stride;
			_videoSubtype = hasSubtype ? subtype : GUID_NULL;
			_videoBytesPerPixel = bytesPerPixel;
			_videoBottomUp = bottomUp;
			_videoTransferMatrix = transferMatrix;
			_videoNominalRange = nominalRange;
			_videoD3DFrameFormat = d3dFrameFormat;
		}
		// A media-type change is a new GPU compatibility boundary. A previous
		// unsupported format must not blacklist a later progressive SDR type on
		// the same D3D device generation.
		_dxgiPresentationFailureGeneration.store(
			0, std::memory_order_release);

		wchar_t dbgMsg[256];
		swprintf_s(dbgMsg,
			L"Video format: %dx%d, stride=%u, bpp=%u, bottomUp=%d\n",
			w, h, stride, bytesPerPixel, bottomUp ? 1 : 0);
		PrintLogWide(dbgMsg);
	}
}

bool MediaElement::InitSourceReader(const std::wstring& url)
{
	ShutdownSourceReader();

	HRESULT hr = S_OK;
	_usingHardwareDecode = false;
	_usingNv12VideoOutput = false;
	_sourceReaderAudioSubtype = GUID_NULL;
	_sourceReaderAudioNegotiationFailed = false;
	_useMediaSessionAudioCompanion = false;

	// 第一阶段：尽量在 Win7 环境也可用的方式启用“硬件变换/硬解”(由系统解码器+驱动决定)，
	// 但仍保持输出为 RGB32（通过 SourceReader 的 video processing），以便复用现有 CPU->D2D 位图渲染链路。
	if (_enableHardwareDecode)
	{
		ComPtr<IMFAttributes> attr;
		hr = MFCreateAttributes(&attr, 10);
		if (FAILED(hr)) { DebugOutputHr(L"SourceReader: MFCreateAttributes(HW)", hr); return false; }

		const bool usingDxgiManager = ConfigureSourceReaderDxgiManager(attr.Get());
		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		// 不设置 MF_SOURCE_READER_DISABLE_DXVA，让 MF 自行选择 DXVA/软件路径。
		// 若优先 NV12，则关闭 MF video processing（否则会把颜色转换成本算进 ReadSample）。
		(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
			(usingDxgiManager || _preferNv12VideoOutput) ? FALSE : TRUE);

		hr = MFCreateSourceReaderFromURL(url.c_str(), attr.Get(), &_sourceReader);
		if (FAILED(hr) && hr == E_INVALIDARG)
		{
			// 某些系统/解码器对 HW_TRANSFORMS 属性不接受（返回 E_INVALIDARG），做一次温和降级再试。
			DebugOutputHr(L"SourceReader: HW init got E_INVALIDARG, retry without HW_TRANSFORMS", hr);
			ReleaseSourceReaderDxgiManager();
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
		ReleaseSourceReaderDxgiManager();
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
		_sourceReaderAudioNegotiationFailed = (_sourceReaderAudioSubtype != GUID_NULL);
		(void)_sourceReader->SetStreamSelection(_srAudioStream, FALSE);
		ShutdownWasapi();
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

	if (_actualVideoStreamIndex == static_cast<DWORD>(-1)
		&& _actualAudioStreamIndex == static_cast<DWORD>(-1))
	{
		_lastMfError.store(MF_E_INVALIDMEDIATYPE);
		return false;
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

bool MediaElement::InitSourceReaderFromByteStream(IMFByteStream* byteStream)
{
	ShutdownSourceReader();
	if (!byteStream) return false;
	_memoryByteStream = byteStream;

	HRESULT hr = S_OK;
	_usingHardwareDecode = false;
	_usingNv12VideoOutput = false;
	_sourceReaderAudioSubtype = GUID_NULL;
	_sourceReaderAudioNegotiationFailed = false;
	_useMediaSessionAudioCompanion = false;

	// 第一阶段：尽量启用硬件变换/硬解（最佳努力），保持与 URL 路径一致的策略。
	if (_enableHardwareDecode)
	{
		ComPtr<IMFAttributes> attr;
		hr = MFCreateAttributes(&attr, 10);
		if (FAILED(hr)) { DebugOutputHr(L"SourceReader: MFCreateAttributes(HW, mem)", hr); return false; }

		const bool usingDxgiManager = ConfigureSourceReaderDxgiManager(attr.Get());
		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
			(usingDxgiManager || _preferNv12VideoOutput) ? FALSE : TRUE);

		hr = MFCreateSourceReaderFromByteStream(_memoryByteStream.Get(), attr.Get(), &_sourceReader);
		if (FAILED(hr) && hr == E_INVALIDARG)
		{
			DebugOutputHr(L"SourceReader: HW init got E_INVALIDARG (mem), retry without HW_TRANSFORMS", hr);
			ReleaseSourceReaderDxgiManager();
			attr.Reset();
			if (SUCCEEDED(MFCreateAttributes(&attr, 8)))
			{
				(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, _preferNv12VideoOutput ? FALSE : TRUE);
				hr = MFCreateSourceReaderFromByteStream(_memoryByteStream.Get(), attr.Get(), &_sourceReader);
			}
		}
		if (SUCCEEDED(hr) && _sourceReader)
		{
			_usingHardwareDecode = true;
			DebugOutputHr(L"SourceReader: HW transforms/DXVA mode (mem, best-effort)", S_OK);
		}
	}

	// 失败回退：强制软解
	if (!_sourceReader)
	{
		ReleaseSourceReaderDxgiManager();
		ComPtr<IMFAttributes> attr;
		hr = MFCreateAttributes(&attr, 8);
		if (FAILED(hr)) { DebugOutputHr(L"SourceReader: MFCreateAttributes(SW, mem)", hr); return false; }
		(void)attr->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
		(void)attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
		(void)attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

		hr = MFCreateSourceReaderFromByteStream(_memoryByteStream.Get(), attr.Get(), &_sourceReader);
		if (FAILED(hr))
		{
			DebugOutputHr(L"SourceReader: MFCreateSourceReaderFromByteStream failed (final)", hr);
			_lastMfError = hr;
			return false;
		}
		DebugOutputHr(L"SourceReader: SW decode mode (mem)", S_OK);
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
		_sourceReaderAudioNegotiationFailed = (_sourceReaderAudioSubtype != GUID_NULL);
		(void)_sourceReader->SetStreamSelection(_srAudioStream, FALSE);
		ShutdownWasapi();
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

	if (_actualVideoStreamIndex == static_cast<DWORD>(-1)
		&& _actualAudioStreamIndex == static_cast<DWORD>(-1))
	{
		_lastMfError.store(MF_E_INVALIDMEDIATYPE);
		return false;
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

void MediaElement::ShutdownSourceReader()
{
	ComPtr<IMFSample> pendingGpuSample;
	{
		std::scoped_lock lock(_videoFrameMutex);
		pendingGpuSample = std::move(_gpuVideoSample);
		_gpuVideoSampleGeneration = 0;
		_gpuVideoSampleReady = false;
		_lastPresentedGpuSample.Reset();
		_lastPresentedGpuSampleGeneration = 0;
	}
	ReleaseGpuPresentationResources(true);
	pendingGpuSample.Reset();
	_sourceReader.Reset();
	ReleaseSourceReaderDxgiManager();
	_memoryByteStream.Reset();
	_memoryStream.Reset();
}

void MediaElement::StopSourceReaderPlayback(bool shutdown)
{
	if (shutdown)
		(void)BeginPlaybackEndEpoch(0, true);
	_threadPlaying = false;
	_needSyncReset = true;
	if (_audioClient) (void)_audioClient->Stop();
	if (_useMediaSessionAudioCompanion && _mediaSession)
		(void)StopPlayback();
	if (_playThread.joinable())
	{
		_threadExit = true;
		WakePlaybackThread();
		_playThread.join();
		_threadExit = false;
	}
	if (shutdown)
	{
		ShutdownWasapi();
		ShutdownSourceReader();
		ShutdownMediaSession();
		_mediaSource.Reset();
		_topology.Reset();
		ResetTopologyState();
		_useMediaSessionAudioCompanion = false;
	}
}

HRESULT MediaElement::WriteAudioToWasapi(
	const BYTE* data, UINT32 bytes, bool dropRemainderIfFull)
{
	if (!_audioClient || !_audioRenderClient || !_audioMixFormat
		|| !_audioReadyEvent) return E_NOT_VALID_STATE;
	if (!data || bytes == 0) return S_OK;

	_statAudioWriteCalls.fetch_add(1, std::memory_order_relaxed);
	_statAudioWriteBytes.fetch_add(bytes, std::memory_order_relaxed);
	const LARGE_INTEGER t0 = QpcNow();
	auto finish = [this, t0](HRESULT result)
	{
		const LARGE_INTEGER t1 = QpcNow();
		_statAudioWriteQpcTicks.fetch_add(
			static_cast<UINT64>(t1.QuadPart - t0.QuadPart),
			std::memory_order_relaxed);
		return result;
	};

	// 防止卡死：若音频引擎长时间完全不接收新帧，避免在此无限等待。
	ULONGLONG lastProgressTick = GetTickCount64();

	UINT32 offset = 0;
	while (offset < bytes)
	{
		if (_threadExit || !_threadPlaying.load()
			|| _needSyncReset.load(std::memory_order_acquire))
			return finish(S_FALSE);

		UINT32 padding = 0;
		HRESULT hr = _audioClient->GetCurrentPadding(&padding);
		if (FAILED(hr))
		{
			DebugOutputHr(L"WASAPI: GetCurrentPadding", hr);
			return finish(hr);
		}
		UINT32 availableFrames = padding < _audioBufferFrameCount
			? _audioBufferFrameCount - padding : 0;
		if (availableFrames == 0)
		{
			if (dropRemainderIfFull)
				return finish(S_OK);
			const ULONGLONG stalledMilliseconds =
				GetTickCount64() - lastProgressTick;
			if (stalledMilliseconds >= 2000)
			{
				const HRESULT timeout = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
				DebugOutputHr(
					L"WASAPI: write timeout (buffer never drains)", timeout);
				return finish(timeout);
			}
			HANDLE waits[2] = { _audioReadyEvent, _playbackWakeEvent };
			const DWORD waitCount = _playbackWakeEvent ? 2u : 1u;
			const DWORD waitResult = ::WaitForMultipleObjects(
				waitCount, waits, FALSE,
				static_cast<DWORD>(2000 - stalledMilliseconds));
			if (waitResult == WAIT_OBJECT_0) continue;
			if (waitCount == 2 && waitResult == WAIT_OBJECT_0 + 1)
			{
				if (_threadExit || !_threadPlaying.load()
					|| _needSyncReset.load(std::memory_order_acquire))
					return finish(S_FALSE);
				continue;
			}
			if (waitResult == WAIT_TIMEOUT)
				return finish(HRESULT_FROM_WIN32(WAIT_TIMEOUT));
			return finish(HRESULT_FROM_WIN32(GetLastError()));
		}
		UINT32 bytesPerFrame = _audioMixFormat->nBlockAlign;
		if (bytesPerFrame == 0) return finish(MF_E_INVALIDMEDIATYPE);
		if ((bytes - offset) < bytesPerFrame)
			return finish(E_INVALIDARG);
		UINT32 availableBytes = availableFrames * bytesPerFrame;
		UINT32 toWrite = (std::min)(availableBytes, bytes - offset);
		UINT32 framesToWrite = toWrite / bytesPerFrame;
		if (framesToWrite == 0)
		{
			if (dropRemainderIfFull) return finish(S_OK);
			continue;
		}
		BYTE* pData = nullptr;
		hr = _audioRenderClient->GetBuffer(framesToWrite, &pData);
		if (FAILED(hr))
		{
			DebugOutputHr(L"WASAPI: GetBuffer", hr);
			return finish(hr);
		}
		memcpy(pData, data + offset, framesToWrite * bytesPerFrame);
		hr = _audioRenderClient->ReleaseBuffer(framesToWrite, 0);
		if (FAILED(hr))
		{
			DebugOutputHr(L"WASAPI: ReleaseBuffer", hr);
			return finish(hr);
		}
		offset += framesToWrite * bytesPerFrame;
		lastProgressTick = GetTickCount64();
	}
	return finish(S_OK);
}

HRESULT MediaElement::WaitForWasapiDrain()
{
	if (!_audioClient || !_audioReadyEvent) return E_NOT_VALID_STATE;
	const ULONGLONG started = GetTickCount64();
	for (;;)
	{
		if (_threadExit || !_threadPlaying.load(std::memory_order_acquire)
			|| _needSyncReset.load(std::memory_order_acquire)) return S_FALSE;
		UINT32 padding = 0;
		const HRESULT paddingResult =
			_audioClient->GetCurrentPadding(&padding);
		if (FAILED(paddingResult)) return paddingResult;
		if (padding == 0) return S_OK;
		const ULONGLONG elapsed = GetTickCount64() - started;
		if (elapsed >= 2000) return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
		HANDLE waits[2] = { _audioReadyEvent, _playbackWakeEvent };
		const DWORD waitCount = _playbackWakeEvent ? 2u : 1u;
		const DWORD waitResult = ::WaitForMultipleObjects(
			waitCount, waits, FALSE, static_cast<DWORD>(2000 - elapsed));
		if (waitResult == WAIT_OBJECT_0) continue;
		if (waitCount == 2 && waitResult == WAIT_OBJECT_0 + 1)
		{
			if (_threadExit || !_threadPlaying.load(std::memory_order_acquire)
				|| _needSyncReset.load(std::memory_order_acquire)) return S_FALSE;
			continue;
		}
		if (waitResult == WAIT_TIMEOUT)
			return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
		return HRESULT_FROM_WIN32(GetLastError());
	}
}

void MediaElement::PlaybackThreadMain()
{
	HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	HANDLE pacingTimer = ::CreateWaitableTimerExW(
		nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_MODIFY_STATE | SYNCHRONIZE);
	if (!pacingTimer)
	{
		pacingTimer = ::CreateWaitableTimerExW(
			nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
	}
	auto waitForPacing = [this, pacingTimer](double seconds)
	{
		if (seconds <= 0.0) return;
		if (pacingTimer)
		{
			LARGE_INTEGER due{};
			due.QuadPart = -(std::max<LONGLONG>)(
				1, static_cast<LONGLONG>(std::llround(
					seconds * static_cast<double>(HNS_PER_SEC))));
			if (::SetWaitableTimerEx(
				pacingTimer, &due, 0, nullptr, nullptr, nullptr, 0))
			{
				HANDLE waits[2] = { pacingTimer, _playbackWakeEvent };
				const DWORD waitCount = _playbackWakeEvent ? 2u : 1u;
				(void)::WaitForMultipleObjects(
					waitCount, waits, FALSE, INFINITE);
				(void)::CancelWaitableTimer(pacingTimer);
				return;
			}
		}
		std::unique_lock lock(_threadMutex);
		(void)_threadCv.wait_for(
			lock, std::chrono::duration<double>(seconds), [this]
			{
				return _threadExit.load(std::memory_order_acquire)
					|| !_threadPlaying.load(std::memory_order_acquire)
					|| _needSyncReset.load(std::memory_order_acquire);
			});
	};
	// Simple A/V sync based on timestamps.
	LONGLONG firstVideoTs = -1;
	LARGE_INTEGER freq{};
	QueryPerformanceFrequency(&freq);
	LARGE_INTEGER startQpc{};
	QueryPerformanceCounter(&startQpc);
	LONGLONG lastPositionEventQpc = 0;
	float syncRate = 1.0f;
	double nextVideoPresentationTargetSeconds = 0.0;
	bool hasVideoPresentationCadence = false;
	bool videoEndOfStream = false;
	bool audioEndOfStream = false;
	UINT64 playbackEndEpoch = CurrentPlaybackEndEpoch();
	UINT64 playbackExplicitCommandGeneration =
		CurrentExplicitPlaybackCommandGeneration();
	UINT64 playbackMediaLoadGeneration = CurrentMediaLoadGeneration();

	while (!_threadExit)
	{
		// Wait until playing.
		{
			std::unique_lock lk(_threadMutex);
			_threadCv.wait(lk, [&] { return _threadExit || _threadPlaying.load(); });
			if (_threadExit) break;
			_playbackWorkerActive = true;
		}

		firstVideoTs = -1;
		lastPositionEventQpc = 0;
		syncRate = ClampRate(_speedRatio.load());
		hasVideoPresentationCadence = false;
		videoEndOfStream = false;
		audioEndOfStream = false;
		playbackEndEpoch = CurrentPlaybackEndEpoch();
		playbackExplicitCommandGeneration =
			CurrentExplicitPlaybackCommandGeneration();
		playbackMediaLoadGeneration = CurrentMediaLoadGeneration();
		auto failAudioPlayback = [this, &playbackEndEpoch,
			&playbackExplicitCommandGeneration,
			&playbackMediaLoadGeneration](HRESULT error)
		{
			if (SUCCEEDED(error)) error = E_FAIL;
			DebugOutputHr(L"WASAPI: terminal playback failure", error);
			_threadPlaying.store(false, std::memory_order_release);
			HandleSourceReaderFailure(
				error, playbackEndEpoch,
				playbackExplicitCommandGeneration,
				playbackMediaLoadGeneration);
		};

		// SourceReader 后端：始终保持音频输出开启；倍速由 WSOLA 对 PCM 做变速不变调处理。
		if (_audioClient && _hasAudio)
		{
			const HRESULT audioStart = _audioClient->Start();
			if (FAILED(audioStart)) failAudioPlayback(audioStart);
		}

		while (_threadPlaying && !_threadExit)
		{
		if (_needSyncReset)
		{
			firstVideoTs = -1;
			lastPositionEventQpc = 0;
			syncRate = ClampRate(_speedRatio.load());
			hasVideoPresentationCadence = false;
			videoEndOfStream = false;
			audioEndOfStream = false;
			playbackEndEpoch = CurrentPlaybackEndEpoch();
			playbackExplicitCommandGeneration =
				CurrentExplicitPlaybackCommandGeneration();
			playbackMediaLoadGeneration = CurrentMediaLoadGeneration();
			if (_timeStretch) _timeStretch->Reset();
			for (auto& stretchStage : _timeStretchChain)
				if (stretchStage) stretchStage->Reset();
			// 倍速/Seek 切换时，WASAPI 缓冲里可能还残留上一次 time-stretch 的音频。
			// 这里通过 Stop+Reset 清空缓冲，避免回到 1.0x 后仍听到“被拉长的片段”。
			if (_audioClient && _hasAudio)
			{
				HRESULT audioControl = _audioClient->Stop();
				if (SUCCEEDED(audioControl))
					audioControl = _audioClient->Reset();
				if (SUCCEEDED(audioControl))
					audioControl = _audioClient->Start();
				if (FAILED(audioControl))
				{
					failAudioPlayback(audioControl);
					break;
				}
			}
			_needSyncReset = false;
		}

		DWORD streamIndex = 0;
		DWORD flags = 0;
		LONGLONG ts = 0;
		ComPtr<IMFSample> sample;
		_statReadSampleCalls.fetch_add(1, std::memory_order_relaxed);
		const LARGE_INTEGER tRead0 = QpcNow();
		DWORD requestedStream = MF_SOURCE_READER_ANY_STREAM;
		if (videoEndOfStream && !audioEndOfStream
			&& _actualAudioStreamIndex != static_cast<DWORD>(-1))
		{
			requestedStream = _actualAudioStreamIndex;
		}
		else if (audioEndOfStream && !videoEndOfStream
			&& _actualVideoStreamIndex != static_cast<DWORD>(-1))
		{
			requestedStream = _actualVideoStreamIndex;
		}
		HRESULT hr = _sourceReader->ReadSample(
			requestedStream, 0, &streamIndex, &flags, &ts, &sample);
		const LARGE_INTEGER tRead1 = QpcNow();
		const UINT64 readTicks = (UINT64)(tRead1.QuadPart - tRead0.QuadPart);
		_statReadSampleQpcTicks.fetch_add(readTicks, std::memory_order_relaxed);
		if (FAILED(hr))
		{
			DebugOutputHr(L"SourceReader: ReadSample failed", hr);
			_threadPlaying = false;
			HandleSourceReaderFailure(
				hr, playbackEndEpoch,
				playbackExplicitCommandGeneration,
				playbackMediaLoadGeneration);
			break;
		}
		if ((flags & MF_SOURCE_READERF_ERROR) != 0)
		{
			// MF may return S_OK with this terminal flag and no sample. Once it is
			// observed the SourceReader is in an error state and must not be read
			// again.
			constexpr HRESULT sourceReaderFlagError = E_FAIL;
			DebugOutputHr(
				L"SourceReader: terminal error flag", sourceReaderFlagError);
			_threadPlaying = false;
			HandleSourceReaderFailure(
				sourceReaderFlagError, playbackEndEpoch,
				playbackExplicitCommandGeneration,
				playbackMediaLoadGeneration);
			break;
		}

		// Flags and output type changes are valid even when ReadSample returns a
		// null sample. Resolve the stream before handling EOS or deciding whether
		// there is pixel/PCM data to process.
		bool isVideo = streamIndex == _actualVideoStreamIndex;
		bool isAudio = streamIndex == _actualAudioStreamIndex;
		if ((!isVideo && !isAudio)
			|| (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0
			|| (flags & MF_SOURCE_READERF_NEWSTREAM) != 0)
		{
			GUID majorType{};
			ComPtr<IMFMediaType> mt;
			if (_sourceReader
				&& SUCCEEDED(_sourceReader->GetCurrentMediaType(
					streamIndex, &mt))
				&& mt
				&& SUCCEEDED(mt->GetGUID(MF_MT_MAJOR_TYPE, &majorType)))
			{
				isVideo = majorType == MFMediaType_Video;
				isAudio = majorType == MFMediaType_Audio;
				if (isVideo) _actualVideoStreamIndex = streamIndex;
				if (isAudio) _actualAudioStreamIndex = streamIndex;
			}
		}
		if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0
			&& isVideo)
		{
			UpdateVideoFormatFromSourceReader();
		}

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			const bool sourceReaderHasVideo =
				_actualVideoStreamIndex != static_cast<DWORD>(-1);
			const bool sourceReaderHasAudio =
				!_useMediaSessionAudioCompanion
				&& _actualAudioStreamIndex != static_cast<DWORD>(-1);
			if (isVideo) videoEndOfStream = true;
			if (isAudio) audioEndOfStream = true;
			if (!isVideo && !isAudio)
			{
				if (sourceReaderHasVideo && !sourceReaderHasAudio)
					videoEndOfStream = true;
				if (sourceReaderHasAudio && !sourceReaderHasVideo)
					audioEndOfStream = true;
			}
			const bool allSelectedStreamsEnded =
				(!sourceReaderHasVideo || videoEndOfStream)
				&& (!sourceReaderHasAudio || audioEndOfStream);
			if (!allSelectedStreamsEnded) continue;

			HRESULT audioDrainResult = S_OK;
			if (_hasAudio && _audioClient && _audioRenderClient && _audioMixFormat)
			{
				const bool isFloat = IsFloatMixFormat(_audioMixFormat);
				const UINT32 bits = (UINT32)_audioBitsPerSample;
				if (WsolaTimeStretch::SupportsFormat(bits, isFloat))
				{
					std::vector<uint8_t> stageInput;
					std::vector<uint8_t> stageOutput;
					bool hasInput = false;
					const auto stageRates = BuildTimeStretchStageRates(_timeStretchChainRate);
					for (size_t stageIndex = 0; stageIndex < _timeStretchChain.size(); stageIndex++)
					{
						if (!_timeStretchChain[stageIndex]) continue;
						stageOutput.clear();
						if (hasInput && !stageInput.empty())
						{
							const float stageRate = (stageIndex < stageRates.size()) ? stageRates[stageIndex] : 1.0f;
							const float stageVolume = (stageIndex + 1 == _timeStretchChain.size()) ? (float)_volume.load() : 1.0f;
							if (!_timeStretchChain[stageIndex]->ProcessChunk(stageInput.data(), stageInput.size(), stageRate, stageVolume, stageOutput))
							{
								audioDrainResult = E_FAIL;
								break;
							}
						}
						std::vector<uint8_t> drainedAudio;
						const float drainRate = (stageIndex < stageRates.size()) ? stageRates[stageIndex] : 1.0f;
						const float drainVolume = (stageIndex + 1 == _timeStretchChain.size()) ? (float)_volume.load() : 1.0f;
						if (!_timeStretchChain[stageIndex]->Drain(drainRate, drainVolume, drainedAudio))
						{
							audioDrainResult = E_FAIL;
							break;
						}
						if (!drainedAudio.empty())
							stageOutput.insert(stageOutput.end(), drainedAudio.begin(), drainedAudio.end());
						if (stageOutput.empty())
						{
							hasInput = false;
							stageInput.clear();
							continue;
						}
						if (stageIndex + 1 == _timeStretchChain.size())
							audioDrainResult = WriteAudioToWasapi(
								stageOutput.data(), (UINT32)stageOutput.size());
						else
						{
							stageInput = std::move(stageOutput);
							hasInput = true;
						}
					}
				}
			}
			if (audioDrainResult == S_OK && _hasAudio && _audioClient)
				audioDrainResult = WaitForWasapiDrain();
			if (audioDrainResult == S_OK && _hasAudio && _audioClient)
				audioDrainResult = _audioClient->Stop();
			if (audioDrainResult != S_OK)
			{
				if (FAILED(audioDrainResult))
					failAudioPlayback(audioDrainResult);
				break;
			}
			_threadPlaying = false;
			UINT8 endedMask = 0;
			if (videoEndOfStream)
				endedMask |= PlaybackEndReaderVideo;
			if (audioEndOfStream)
				endedMask |= PlaybackEndReaderAudio;
			(void)SignalPlaybackEnd(endedMask, playbackEndEpoch);
			break;
		}

		if (!sample) continue;
		float currentRateForSync = ClampRate(_speedRatio.load());
		if (std::fabs(currentRateForSync - syncRate) > 0.0005f)
		{
			firstVideoTs = -1;
			syncRate = currentRateForSync;
			hasVideoPresentationCadence = false;
			QueryPerformanceCounter(&startQpc);
		}
		if (isVideo && firstVideoTs < 0)
		{
			firstVideoTs = ts;
			QueryPerformanceCounter(&startQpc);
		}

		if (isVideo)
		{
			_statReadSampleVideoCalls.fetch_add(1, std::memory_order_relaxed);
			_statReadSampleVideoQpcTicks.fetch_add(
				readTicks, std::memory_order_relaxed);
		}
		else if (isAudio)
		{
			_statReadSampleAudioCalls.fetch_add(1, std::memory_order_relaxed);
			_statReadSampleAudioQpcTicks.fetch_add(
				readTicks, std::memory_order_relaxed);
		}

		// SourceReader 返回的是解码样本。音频样本由 WASAPI 缓冲自然限速；若再按原始时间戳 sleep，
		// 变速后的 PCM 会被二次节拍，表现为卡顿、不同步或倍速失效。视频仍按时间戳节拍。
		double videoLatenessSeconds = 0.0;
		double videoTargetElapsedSeconds = 0.0;
		if (isVideo)
		{
			float rate = currentRateForSync;
			if (rate < 0.01f) rate = 1.0f;

			const double relSec =
				(double)(ts - firstVideoTs) / HNS_PER_SEC;
			double targetElapsedSec = relSec / rate;
			for (;;)
			{
				if (_threadExit || !_threadPlaying) break;
				if (_needSyncReset) break;
				LARGE_INTEGER now{};
				QueryPerformanceCounter(&now);
				double elapsedSec = (double)(now.QuadPart - startQpc.QuadPart) / (double)freq.QuadPart;
				double delta = targetElapsedSec - elapsedSec;
				if (delta <= 0.00025) break;

				waitForPacing(delta);
				rate = ClampRate(_speedRatio.load());
				if (rate < 0.01f) rate = 1.0f;
				targetElapsedSec = relSec / rate;
			}
			LARGE_INTEGER now{};
			QueryPerformanceCounter(&now);
			const double elapsedSec =
				(double)(now.QuadPart - startQpc.QuadPart)
				/ (double)freq.QuadPart;
			videoLatenessSeconds = std::max(
				0.0, elapsedSec - targetElapsedSec);
			videoTargetElapsedSeconds = targetElapsedSec;
		}
		// Pause/seek/rate changes wake the high-resolution timer.  The sample was
		// read under the old clock transaction and must not update position or be
		// published after that cancellation.
		if (_threadExit || !_threadPlaying.load(std::memory_order_acquire))
			break;
		if (_needSyncReset.load(std::memory_order_acquire))
			continue;

		SetObservedPosition((double)ts / HNS_PER_SEC, false);
		LARGE_INTEGER positionNow{};
		QueryPerformanceCounter(&positionNow);
		if (lastPositionEventQpc == 0 || (positionNow.QuadPart - lastPositionEventQpc) >= (freq.QuadPart / 20))
		{
			lastPositionEventQpc = positionNow.QuadPart;
			FirePositionChanged(
				_position.load(), playbackEndEpoch,
				playbackExplicitCommandGeneration,
				playbackMediaLoadGeneration);
		}

		if (isVideo)
		{
			_statDecodedVideoFrames.fetch_add(1, std::memory_order_relaxed);
			const UINT64 latenessTicks = videoLatenessSeconds > 0.0
				? static_cast<UINT64>(videoLatenessSeconds * freq.QuadPart)
				: 0;
			UINT64 previousMaximum =
				_statMaxVideoLatenessQpcTicks.load(std::memory_order_relaxed);
			while (latenessTicks > previousMaximum
				&& !_statMaxVideoLatenessQpcTicks.compare_exchange_weak(
					previousMaximum, latenessTicks,
					std::memory_order_relaxed))
			{
			}

			const float rate = ClampRate(_speedRatio.load());
			const double sourceFrameSeconds = std::max(
				0.001, (double)_videoFrameDurationHns.load(
					std::memory_order_relaxed) / HNS_PER_SEC);
			const double lateDropThresholdSeconds = std::max(
				0.005, sourceFrameSeconds / std::max(0.1f, rate));
			if (videoLatenessSeconds > lateDropThresholdSeconds)
			{
				_statDroppedLateVideoFrames.fetch_add(
					1, std::memory_order_relaxed);
				continue;
			}

			// Do not convert/upload samples faster than the hosting monitor can
			// present them.  Thin on the media timeline before touching CPU pixels,
			// while preserving 120 fps playback on a high-refresh display.
			const UINT32 presentationRateLimitHz = std::max<UINT32>(
				1, _videoPresentationRateLimitHz.load(std::memory_order_relaxed));
			const double minimumPresentationIntervalSeconds =
				1.0 / static_cast<double>(presentationRateLimitHz);
			constexpr double presentationIntervalToleranceSeconds = 0.0001;
			if (hasVideoPresentationCadence
				&& videoTargetElapsedSeconds + presentationIntervalToleranceSeconds
					< nextVideoPresentationTargetSeconds)
			{
				_statThinnedVideoFrames.fetch_add(
					1, std::memory_order_relaxed);
				continue;
			}
			if (!hasVideoPresentationCadence)
			{
				nextVideoPresentationTargetSeconds =
					videoTargetElapsedSeconds + minimumPresentationIntervalSeconds;
				hasVideoPresentationCadence = true;
			}
			else
			{
				do
				{
					nextVideoPresentationTargetSeconds +=
						minimumPresentationIntervalSeconds;
				} while (nextVideoPresentationTargetSeconds
					<= videoTargetElapsedSeconds
						+ presentationIntervalToleranceSeconds);
			}
		}

		// A SourceReader bound to the shared MF DXGI manager yields decoder-owned
		// surfaces. Retain the sample in a one-slot mailbox and let the UI thread
		// perform one GPU VideoProcessor conversion into a stable BGRA texture.
		// The CPU path below remains the compatibility fallback for system-memory
		// samples and for decoders that ignore the device-manager request.
		const DxgiVideoSampleDisposition dxgiDisposition = isVideo
			? TryPublishDxgiVideoSample(sample.Get())
			: DxgiVideoSampleDisposition::CpuFallbackEligible;
		if (isVideo
			&& dxgiDisposition == DxgiVideoSampleDisposition::Published)
		{
			_actualVideoStreamIndex = streamIndex;
			continue;
		}
		if (isVideo
			&& dxgiDisposition == DxgiVideoSampleDisposition::DropStale)
		{
			// A decoder can release a small number of old-device samples after
			// ResetDevice.  They cannot be safely locked/read back and are not a
			// CPU fallback; drop them until the new generation arrives.
			_actualVideoStreamIndex = streamIndex;
			continue;
		}
		if (isVideo)
			_statCpuFallbackVideoFrames.fetch_add(1, std::memory_order_relaxed);

		ComPtr<IMFMediaBuffer> buf;
		ComPtr<IMF2DBuffer2> buffer2D;
		_statSamplesToContigCalls.fetch_add(1, std::memory_order_relaxed);
		const LARGE_INTEGER tContig0 = QpcNow();
		hr = sample->ConvertToContiguousBuffer(&buf);
		const LARGE_INTEGER tContig1 = QpcNow();
		const UINT64 contigTicks = (UINT64)(tContig1.QuadPart - tContig0.QuadPart);
		_statSamplesToContigQpcTicks.fetch_add(contigTicks, std::memory_order_relaxed);
		if ((FAILED(hr) || !buf) && isVideo)
		{
			// Decoder-owned DXGI samples need not support the linear Lock contract.
			// Keep the original 2D buffer and use its explicit readback lock below.
			DWORD sourceBufferCount = 0;
			if (SUCCEEDED(sample->GetBufferCount(&sourceBufferCount)))
			{
				for (DWORD index = 0;
					index < sourceBufferCount && !buffer2D; ++index)
				{
					ComPtr<IMFMediaBuffer> candidate;
					ComPtr<IMF2DBuffer2> candidate2D;
					if (SUCCEEDED(sample->GetBufferByIndex(
						index, candidate.GetAddressOf()))
						&& candidate
						&& SUCCEEDED(candidate.As(&candidate2D))
						&& candidate2D)
					{
						buf = candidate;
						buffer2D = candidate2D;
					}
				}
			}
		}
		if (!buf)
		{
			const HRESULT bufferError = FAILED(hr) ? hr : E_FAIL;
			if (isVideo
				&& ++_consecutiveCpuVideoBufferLockFailures >= 3)
			{
				DebugOutputHr(
					L"SourceReader: CPU video buffer unavailable",
					bufferError);
				_threadPlaying = false;
				HandleSourceReaderFailure(
					bufferError, playbackEndEpoch,
					playbackExplicitCommandGeneration,
					playbackMediaLoadGeneration);
				break;
			}
			continue;
		}
		BYTE* p = nullptr;
		DWORD maxLen = 0, curLen = 0;
		BYTE* bufferStart = nullptr;
		LONG lockedPitch = 0;
		bool lockedAs2D = false;
		if (isVideo)
		{
			if (!buffer2D) (void)buf.As(&buffer2D);
			if (buffer2D)
			{
				hr = buffer2D->Lock2DSize(
					MF2DBuffer_LockFlags_Read, &p, &lockedPitch,
					&bufferStart, &curLen);
				if (SUCCEEDED(hr))
				{
					lockedAs2D = true;
					maxLen = curLen;
				}
			}
		}
		if (!lockedAs2D)
			hr = buf->Lock(&p, &maxLen, &curLen);
		if (FAILED(hr))
		{
			if (isVideo
				&& ++_consecutiveCpuVideoBufferLockFailures >= 3)
			{
				DebugOutputHr(
					L"SourceReader: CPU video buffer lock failed", hr);
				_threadPlaying = false;
				HandleSourceReaderFailure(
					hr, playbackEndEpoch,
					playbackExplicitCommandGeneration,
					playbackMediaLoadGeneration);
				break;
			}
			continue;
		}
		struct MediaBufferUnlockGuard
		{
			IMFMediaBuffer* Buffer = nullptr;
			IMF2DBuffer2* Buffer2D = nullptr;
			bool LockedAs2D = false;
			~MediaBufferUnlockGuard()
			{
				if (LockedAs2D && Buffer2D)
					(void)Buffer2D->Unlock2D();
				else if (Buffer)
					(void)Buffer->Unlock();
			}
		} bufferUnlockGuard{ buf.Get(), buffer2D.Get(), lockedAs2D };
		if (!p || curLen == 0)
		{
			if (isVideo
				&& ++_consecutiveCpuVideoBufferLockFailures >= 3)
			{
				constexpr HRESULT emptyBufferError = E_FAIL;
				DebugOutputHr(
					L"SourceReader: CPU video buffer is empty",
					emptyBufferError);
				_threadPlaying = false;
				HandleSourceReaderFailure(
					emptyBufferError, playbackEndEpoch,
					playbackExplicitCommandGeneration,
					playbackMediaLoadGeneration);
				break;
			}
			continue;
		}

		if (isVideo)
		{
			_consecutiveCpuVideoBufferLockFailures = 0;
			_actualVideoStreamIndex = streamIndex;

			// 统一转换为 BGRA32，消除像素格式/stride 差异导致的花屏。
			// 注意：部分解码链路会输出带 padding 的帧，并通过 aperture 指定真实可视区域。
			LONG frameW = 0;
			LONG frameH = 0;
			LONG w = 0;
			LONG h = 0;
			UINT32 cropX = 0;
			UINT32 cropY = 0;
			GUID subtype{};
			UINT32 srcStride = 0;
			UINT32 bpp = 4;
			bool bottomUp = false;
			MFVideoTransferMatrix transferMatrix =
				MFVideoTransferMatrix_Unknown;
			MFNominalRange nominalRange = MFNominalRange_Unknown;
			auto snapshotVideoFormat = [&]
			{
				std::scoped_lock lock(_videoFrameMutex);
				frameW = _videoFrameSize.cx;
				frameH = _videoFrameSize.cy;
				w = _videoSize.cx;
				h = _videoSize.cy;
				cropX = _videoCropX;
				cropY = _videoCropY;
				subtype = _videoSubtype;
				srcStride = _videoStride;
				bpp = (_videoBytesPerPixel == 0) ? 4 : _videoBytesPerPixel;
				bottomUp = _videoBottomUp;
				transferMatrix = _videoTransferMatrix;
				nominalRange = _videoNominalRange;
			};
			snapshotVideoFormat();
			if (w <= 0 || h <= 0)
			{
				UpdateVideoFormatFromSourceReader();
				snapshotVideoFormat();
			}
			size_t accessibleBytes = curLen;
			if (lockedAs2D && lockedPitch != 0 && frameH > 0)
			{
				const UINT32 pitchMagnitude = static_cast<UINT32>(
					lockedPitch < 0
						? -static_cast<int64_t>(lockedPitch)
						: lockedPitch);
				srcStride = pitchMagnitude;
				bottomUp = false;
				if (bufferStart)
				{
					const uintptr_t bufferBegin =
						reinterpret_cast<uintptr_t>(bufferStart);
					const uintptr_t bufferEnd = bufferBegin + curLen;
					const uintptr_t scanline = reinterpret_cast<uintptr_t>(p);
					if (scanline < bufferBegin || scanline >= bufferEnd)
						accessibleBytes = 0;
					else
						accessibleBytes = bufferEnd - scanline;
				}
			}
			if (frameW > 0 && frameH > 0 && w > 0 && h > 0)
			{
				_statVideoConvertCalls.fetch_add(1, std::memory_order_relaxed);
				const LARGE_INTEGER tVid0 = QpcNow();

				if (subtype == MFVideoFormat_NV12)
				{
					// NV12 uses a positive Y-plane pitch followed by the UV
					// plane. A negative multi-plane pitch has no compatible
					// linear contract in this CPU converter.
					if (lockedAs2D && lockedPitch < 0)
					{
						constexpr HRESULT pitchError =
							MF_E_INVALIDMEDIATYPE;
						DebugOutputHr(
							L"SourceReader: negative NV12 pitch unsupported",
							pitchError);
						_threadPlaying = false;
						HandleSourceReaderFailure(
							pitchError, playbackEndEpoch,
							playbackExplicitCommandGeneration,
							playbackMediaLoadGeneration);
						break;
					}
					if (srcStride == 0) srcStride = (UINT32)frameW;
					auto converted = AcquireVideoFrameBuffer();
					const bool convertedSuccessfully = ConvertNV12ToBGRA(
						p, accessibleBytes, srcStride,
						static_cast<UINT32>(frameW),
						static_cast<UINT32>(frameH), cropX, cropY,
						static_cast<UINT32>(w), static_cast<UINT32>(h),
						transferMatrix, nominalRange, converted);
					if (convertedSuccessfully && !converted.empty())
					{
						const LARGE_INTEGER tVid1 = QpcNow();
						const UINT64 vConvTicks = (UINT64)(tVid1.QuadPart - tVid0.QuadPart);
						_statVideoConvertQpcTicks.fetch_add(vConvTicks, std::memory_order_relaxed);
						_statVideoConvertBytes.fetch_add((UINT64)w * (UINT64)h * 4ULL, std::memory_order_relaxed);
						PublishVideoFrame(
							std::move(converted), (UINT32)w * 4, SIZE{ w, h });
					}
					else
					{
						const LARGE_INTEGER tVid1 = QpcNow();
						_statVideoConvertQpcTicks.fetch_add((UINT64)(tVid1.QuadPart - tVid0.QuadPart), std::memory_order_relaxed);
						constexpr HRESULT conversionError =
							MF_E_INVALIDMEDIATYPE;
						DebugOutputHr(
							L"SourceReader: NV12 CPU conversion failed",
							conversionError);
						_threadPlaying = false;
						HandleSourceReaderFailure(
							conversionError, playbackEndEpoch,
							playbackExplicitCommandGeneration,
							playbackMediaLoadGeneration);
						break;
					}
					continue;
				}


				const UINT32 minStride = (UINT32)frameW * bpp;
				if (srcStride == 0) srcStride = minStride;
				if (srcStride < minStride) srcStride = minStride;
				const UINT64 needed =
					static_cast<UINT64>(srcStride)
					* static_cast<UINT32>(frameH);
				if (lockedAs2D || static_cast<UINT64>(curLen) >= needed)
				{
					auto converted = AcquireVideoFrameBuffer();
					converted.resize((size_t)w * (size_t)h * 4);
					const UINT32 cropWBytes = (UINT32)w * 4;
					for (LONG row = 0; row < h; row++)
					{
						LONG rawRow = (LONG)cropY + row;
						if (rawRow < 0 || rawRow >= frameH) break;
						LONG srcRow = bottomUp
							? (frameH - 1 - rawRow) : rawRow;
						const BYTE* srcRowPtr = lockedAs2D
							? p + static_cast<ptrdiff_t>(rawRow)
								* static_cast<ptrdiff_t>(lockedPitch)
								+ static_cast<size_t>(cropX) * bpp
							: p + static_cast<size_t>(srcRow) * srcStride
								+ static_cast<size_t>(cropX) * bpp;
						uint8_t* dstRowPtr = converted.data() + (size_t)row * (size_t)w * 4;
						if (lockedAs2D && bufferStart)
						{
							const uintptr_t begin =
								reinterpret_cast<uintptr_t>(bufferStart);
							const uintptr_t end = begin + curLen;
							const uintptr_t rowBegin =
								reinterpret_cast<uintptr_t>(srcRowPtr);
							const UINT64 rowBytes =
								static_cast<UINT64>(w) * bpp;
							if (rowBegin < begin || rowBegin > end
								|| rowBytes > end - rowBegin)
							{
								converted.clear();
								break;
							}
						}

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

					if (converted.empty())
					{
						constexpr HRESULT boundsError = E_FAIL;
						DebugOutputHr(
							L"SourceReader: CPU video row outside buffer",
							boundsError);
						_threadPlaying = false;
						HandleSourceReaderFailure(
							boundsError, playbackEndEpoch,
							playbackExplicitCommandGeneration,
							playbackMediaLoadGeneration);
						break;
					}
					const LARGE_INTEGER tVid1 = QpcNow();
					const UINT64 vConvTicks = (UINT64)(tVid1.QuadPart - tVid0.QuadPart);
					_statVideoConvertQpcTicks.fetch_add(vConvTicks, std::memory_order_relaxed);
					_statVideoConvertBytes.fetch_add((UINT64)w * (UINT64)h * 4ULL, std::memory_order_relaxed);
					PublishVideoFrame(
						std::move(converted), (UINT32)w * 4, SIZE{ w, h });
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
			if (!_hasAudio || !_audioClient || !_audioRenderClient || !_audioMixFormat)
			{
				continue;
			}

			float rate = ClampRate(_speedRatio.load());
			const bool isFloat = IsFloatMixFormat(_audioMixFormat);
			const float vol = (float)_volume.load();
			const UINT32 sampleRate = _audioMixFormat ? (UINT32)_audioMixFormat->nSamplesPerSec : 0;
			const UINT32 channels = (UINT32)_audioChannels;
			const UINT32 bits = (UINT32)_audioBitsPerSample;

			// rate≈1 时无需 time-stretch，直接输出可显著降低 CPU 并避免“看起来卡死”。
			if (std::fabs(rate - 1.0f) < 0.0005f)
			{
				ApplyVolume(p, (size_t)curLen, _audioBitsPerSample, vol, isFloat);
				const HRESULT audioWrite =
					WriteAudioToWasapi(p, curLen, false);
				if (audioWrite != S_OK)
				{
					if (FAILED(audioWrite)) failAudioPlayback(audioWrite);
					if (FAILED(audioWrite)) break;
					continue;
				}
			}
			else
			{

				// WSOLA: 变速不变调。若当前块暂时攒不出输出，等待后续块补足窗口，不能回退到重采样变调路径。
				std::vector<uint8_t> stretched;
				const bool canUseWsola = sampleRate != 0 && channels != 0 && WsolaTimeStretch::SupportsFormat(bits, isFloat);
				if (canUseWsola)
				{
					std::vector<float> stageRates = BuildTimeStretchStageRates(rate);

					if (_timeStretchChain.size() != stageRates.size() || std::fabs(_timeStretchChainRate - rate) > 0.0005f)
					{
						_timeStretchChain.clear();
						_timeStretchChain.reserve(stageRates.size());
						for (size_t stageIndex = 0; stageIndex < stageRates.size(); stageIndex++)
							_timeStretchChain.push_back(std::make_unique<WsolaTimeStretch>(sampleRate, channels, isFloat, bits));
						_timeStretchChainRate = rate;
					}
					else
					{
						for (auto& stretchStage : _timeStretchChain)
						{
							if (!stretchStage || !stretchStage->Matches(sampleRate, channels, isFloat, bits))
							{
								_timeStretchChain.clear();
								_timeStretchChain.reserve(stageRates.size());
								for (size_t stageIndex = 0; stageIndex < stageRates.size(); stageIndex++)
									_timeStretchChain.push_back(std::make_unique<WsolaTimeStretch>(sampleRate, channels, isFloat, bits));
								_timeStretchChainRate = rate;
								break;
							}
						}
					}

					const BYTE* stageData = p;
					size_t stageBytes = (size_t)curLen;
					std::vector<uint8_t> stageBufferA;
					std::vector<uint8_t> stageBufferB;
					bool stretchOk = !_timeStretchChain.empty();
					for (size_t stageIndex = 0; stageIndex < _timeStretchChain.size(); stageIndex++)
					{
						std::vector<uint8_t>& stageOut = (stageIndex % 2 == 0) ? stageBufferA : stageBufferB;
						stageOut.clear();
						const float stageVolume = (stageIndex + 1 == _timeStretchChain.size()) ? vol : 1.0f;
						if (!_timeStretchChain[stageIndex]->ProcessChunk(stageData, stageBytes, stageRates[stageIndex], stageVolume, stageOut))
						{
							stretchOk = false;
							break;
						}
						if (stageOut.empty())
						{
							stretchOk = true;
							stageBytes = 0;
							break;
						}
						stageData = stageOut.data();
						stageBytes = stageOut.size();
					}

					if (stretchOk)
					{
						HRESULT audioWrite = S_OK;
						if (stageBytes > 0)
							audioWrite = WriteAudioToWasapi(
								stageData, (UINT32)stageBytes, false);
						if (audioWrite != S_OK)
						{
							if (FAILED(audioWrite))
								failAudioPlayback(audioWrite);
							if (FAILED(audioWrite)) break;
						}
						continue;
					}
				}

				// 最后兜底：不支持的 PCM 格式保持原音调/原速播放，避免错误的重采样变调。
				ApplyVolume(p, (size_t)curLen, _audioBitsPerSample, vol, isFloat);
				const HRESULT audioWrite =
					WriteAudioToWasapi(p, curLen, false);
				if (audioWrite != S_OK)
				{
					if (FAILED(audioWrite)) failAudioPlayback(audioWrite);
					if (FAILED(audioWrite)) break;
					continue;
				}
			}
		}

		}

		{
			std::scoped_lock lock(_threadMutex);
			_playbackWorkerActive = false;
		}
		_threadIdleCv.notify_all();

		if (_audioClient && _hasAudio)
			(void)_audioClient->Stop();
	}

	if (SUCCEEDED(hrCo))
		CoUninitialize();
	if (pacingTimer) ::CloseHandle(pacingTimer);
}

HRESULT MediaElement::CreateMediaSession()
{
	ShutdownMediaSession();

	HRESULT hr = MFCreateMediaSession(nullptr, &_mediaSession);
	if (FAILED(hr)) return hr;

	_eventCallback = new (std::nothrow) MediaElementCallback(
		this, CurrentMediaLoadGeneration());
	if (_eventCallback)
	{
		hr = _mediaSession->BeginGetEvent(_eventCallback.Get(), nullptr);
		if (FAILED(hr)) return hr;
	}

	_presentationClock.Reset();
	_videoDisplayControl.Reset();
	return S_OK;
}

void MediaElement::ShutdownMediaSession()
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

HRESULT MediaElement::EnsureVideoDisplayControl()
{
	// 完全自渲染视频时不需要该服务
	if (_videoSampleCallback) return S_OK;
	if (_videoDisplayControl) return S_OK;
	if (!_mediaSession) return E_NOT_VALID_STATE;

	// 注意：MR_VIDEO_RENDER_SERVICE 只有在视频渲染器已在拓扑中创建后才可用。
	return MFGetService(_mediaSession.Get(), MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&_videoDisplayControl));
}

void MediaElement::UpdatePositionFromClock(
	bool forceEvent, bool deferNotification)
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

	if (forceEvent || std::abs(newPos - _position.load()) >= 0.10)
	{
		DeferredPlaybackNotifications notifications{};
		notifications.PositionChanged = CommitObservedPosition(
			newPos, true, forceEvent, notifications.Position);
		notifications.ExpectedExplicitCommandGeneration =
			CurrentExplicitPlaybackCommandGeneration();
		notifications.ExpectedMediaLoadGeneration =
			CurrentMediaLoadGeneration();
		RaiseDeferredPlaybackNotifications(
			std::move(notifications), deferNotification);
	}
}

HRESULT MediaElement::CreateMediaSource(const std::wstring& url)
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

HRESULT MediaElement::CreateTopology()
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
					std::scoped_lock lock(_videoFrameMutex);
					_videoFrameSize = SIZE{ (LONG)w, (LONG)h };
					_videoSize = SIZE{ (LONG)w, (LONG)h };
					_videoCropX = 0;
					_videoCropY = 0;
					_videoStride = w * 4;
					_videoSubtype = MFVideoFormat_RGB32;
					_videoBytesPerPixel = 4;
					_videoBottomUp = false;
				}
			}
			hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);
			if (FAILED(hr)) break;

			// 使用 Sample Grabber Sink 获取视频帧（完全自渲染，不依赖子窗口/EVR）
			ComPtr<IMFMediaType> pVideoType;
			hr = MFCreateMediaType(&pVideoType);
			if (FAILED(hr)) break;
			if (currentType)
				(void)currentType->CopyAllItems(pVideoType.Get());
			hr = pVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			if (FAILED(hr)) break;
			// 选用 MFVideoFormat_RGB32：在部分机器/解码器组合下比 ARGB32 更稳定
			// （仍然是 32bpp，内存布局可按 BGRA 处理，alpha 通道通常可忽略）
			hr = pVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			if (FAILED(hr)) break;
			if (currentType)
			{
				UINT64 frameRate = 0;
				UINT64 pixelAspect = 0;
				UINT32 interlaceMode = MFVideoInterlace_Unknown;
				if (SUCCEEDED(currentType->GetUINT64(MF_MT_FRAME_RATE, &frameRate)))
					(void)pVideoType->SetUINT64(MF_MT_FRAME_RATE, frameRate);
				if (SUCCEEDED(currentType->GetUINT64(MF_MT_PIXEL_ASPECT_RATIO, &pixelAspect)))
					(void)pVideoType->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO, pixelAspect);
				if (SUCCEEDED(currentType->GetUINT32(MF_MT_INTERLACE_MODE, &interlaceMode)))
					(void)pVideoType->SetUINT32(MF_MT_INTERLACE_MODE, interlaceMode);
			}
			// 尽量把期望的尺寸/步幅明确写入（否则某些解码链路会协商出带对齐的 stride，
			// 导致我们按 width*4 取帧时出现错位或黑屏）
			if (_videoSize.cx > 0 && _videoSize.cy > 0)
			{
				(void)MFSetAttributeSize(pVideoType.Get(), MF_MT_FRAME_SIZE, (UINT32)_videoSize.cx, (UINT32)_videoSize.cy);
				(void)pVideoType->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)_videoSize.cx * 4);
				(void)pVideoType->SetUINT32(MF_MT_SAMPLE_SIZE, (UINT32)_videoSize.cx * (UINT32)_videoSize.cy * 4);
			}
			(void)pVideoType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
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

HRESULT MediaElement::CreateAudioOnlyTopology()
{
	HRESULT hr = S_OK;
	ComPtr<IMFPresentationDescriptor> pSourcePD;
	ComPtr<IMFTopology> pTopology;

	hr = MFCreateTopology(&pTopology);
	if (FAILED(hr)) return hr;

	hr = _mediaSource->CreatePresentationDescriptor(&pSourcePD);
	if (FAILED(hr)) return hr;

	DWORD cSourceStreams = 0;
	hr = pSourcePD->GetStreamDescriptorCount(&cSourceStreams);
	if (FAILED(hr)) return hr;

	bool addedAudio = false;
	for (DWORD i = 0; i < cSourceStreams; i++)
	{
		BOOL fSelected = FALSE;
		ComPtr<IMFStreamDescriptor> pSourceSD;
		hr = pSourcePD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD);
		if (FAILED(hr) || !pSourceSD) break;

		ComPtr<IMFMediaTypeHandler> pTypeHandler;
		hr = pSourceSD->GetMediaTypeHandler(&pTypeHandler);
		if (FAILED(hr) || !pTypeHandler) break;

		GUID majorType = GUID_NULL;
		hr = pTypeHandler->GetMajorType(&majorType);
		if (FAILED(hr)) break;

		if (majorType != MFMediaType_Audio)
		{
			if (fSelected)
				(void)pSourcePD->DeselectStream(i);
			continue;
		}

		if (!fSelected)
		{
			(void)pSourcePD->SelectStream(i);
			fSelected = TRUE;
		}
		if (!fSelected) continue;

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

		ComPtr<IMFTopologyNode> pOutputNode;
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

		addedAudio = true;
	}

	if (FAILED(hr)) return hr;
	if (!addedAudio) return MF_E_INVALIDMEDIATYPE;
	_topology = pTopology;
	return S_OK;
}

HRESULT MediaElement::InitializeVideoRenderer()
{
	// 完全自渲染路径不依赖 EVR/窗口
	return S_OK;
}

void MediaElement::OnVideoFrame(const BYTE* data, DWORD size)
{
	if (!data || size == 0) return;

	// SampleGrabber 的输出可能带行对齐 padding；推断 stride 并规范化成连续 BGRA32(width*4)。
	UINT32 frameW = 0;
	UINT32 frameH = 0;
	UINT32 w = 0;
	UINT32 h = 0;
	UINT32 cropX = 0;
	UINT32 cropY = 0;
	{
		std::scoped_lock lock(_videoFrameMutex);
		frameW = static_cast<UINT32>((std::max)(0L, _videoFrameSize.cx));
		frameH = static_cast<UINT32>((std::max)(0L, _videoFrameSize.cy));
		w = static_cast<UINT32>((std::max)(0L, _videoSize.cx));
		h = static_cast<UINT32>((std::max)(0L, _videoSize.cy));
		cropX = _videoCropX;
		cropY = _videoCropY;
	}
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

	auto normalized = AcquireVideoFrameBuffer();
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

	PublishVideoFrame(
		std::move(normalized), expectedStride,
		SIZE{ static_cast<LONG>(w), static_cast<LONG>(h) });
}

void MediaElement::RefreshVideoFormatFromSource()
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
			std::scoped_lock lock(_videoFrameMutex);
			_videoFrameSize = SIZE{ (LONG)width, (LONG)height };
			_videoSize = SIZE{ (LONG)visibleW, (LONG)visibleH };
			_videoCropX = cropX;
			_videoCropY = cropY;
			_videoStride = width * 4;
		}
		break;
	}
}

bool MediaElement::Load(const std::wstring& mediaFile)
{
	_requestedState.store(MediaState::Stop, std::memory_order_release);
	if (mediaFile.empty()) return LoadSourceCore(mediaFile);
	if (_source != mediaFile)
	{
		std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
		Source = mediaFile;
		auto lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime || _source != mediaFile) return false;
		if (!GetPresentationWindow()) return true;
		if (_mediaLoaded.load(std::memory_order_acquire)
			&& _mediaFile == mediaFile) return true;
		return LoadSourceCore(mediaFile);
	}
	if (!GetPresentationWindow()) return true;
	return LoadSourceCore(mediaFile);
}

bool MediaElement::LoadSourceCore(const std::wstring& mediaFile)
{
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	DeferredPlaybackNotifications notifications{};
	notifications.PositionFirst = true;
	auto failPrecondition = [this, &commandLock, &notifications](HRESULT error)
	{
		CommitMediaFailure(error, notifications, false);
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return false;
	};
	if (mediaFile.empty())
	{
		return failPrecondition(E_INVALIDARG);
	}
	auto* presentationWindow = this->GetPresentationWindow();
	if (!presentationWindow)
	{
		return failPrecondition(E_HANDLE);
	}
	if (!EnsureInitialized())
	{
		const HRESULT initializationError = _initializationHr;
		return failPrecondition(initializationError);
	}
	const UINT64 explicitCommandGeneration =
		AdvanceExplicitPlaybackCommandGeneration();
	const UINT64 mediaLoadGeneration = AdvanceMediaLoadGeneration();
	notifications.ExpectedMediaLoadGeneration = mediaLoadGeneration;
	auto failLoad = [this, &commandLock, &notifications](HRESULT error)
	{
		CommitMediaFailure(error, notifications, true);
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(
			std::move(notifications), true);
		return false;
	};
	BeginPlaybackQuiescence();
	_mediaLoaded = false;
	WakePlaybackThread();
	CompletePlaybackQuiescence();
	ClearMediaError();
	ResetPerformanceCounters();
	_videoFrameDurationHns.store(333333, std::memory_order_relaxed);
	_videoFrameRateKnown.store(false, std::memory_order_relaxed);
	_presentationRateMonitor = nullptr;
	_lastPresentationRateRefreshQpc = 0;
	RefreshPresentationRateLimit();
	const bool preferSourceReader = _preferSourceReader;

	// 若之前在 SourceReader 后端播放，先彻底停掉线程/音频，避免后续后端切换时状态互相干扰。
	if (_playThread.joinable() || _threadPlaying.load() || _sourceReader)
	{
		StopSourceReaderPlayback(true);
	}
	if (_mediaSession || _mediaSource || _topology)
	{
		ReleaseResources();
	}

	// Prefer SourceReader+WASAPI path for maximum compatibility in a self-rendered UI.
	if (preferSourceReader)
	{
		_useSourceReader = true;
		// 上面已 StopSourceReaderPlayback(true)
		_mediaFile = mediaFile;
		_mediaLoaded = false;
		notifications.PositionChanged = CommitObservedPosition(
			0.0, true, false, notifications.Position);
		_duration = 0.0;
		_hasVideo = false;
		_hasAudio = false;
		ReleaseVideoFrameBuffers();
		_memoryByteStream.Reset();
		_memoryStream.Reset();
		if (_videoBitmap && _ownsVideoBitmap)
			_videoBitmap->Release();
		_videoBitmap = nullptr;
		_ownsVideoBitmap = false;

		if (!InitSourceReader(mediaFile))
		{
			_mediaLoaded = false;
			return failLoad(_lastMfError.load());
		}

		if (_hasVideo && !_hasAudio && _sourceReaderAudioNegotiationFailed && ShouldFallbackToMediaSessionForAudioSubtype(_sourceReaderAudioSubtype))
		{
			if (FAILED(CreateMediaSession()) || FAILED(CreateMediaSource(mediaFile)) || FAILED(CreateAudioOnlyTopology()))
			{
				ShutdownMediaSession();
				_mediaSource.Reset();
				_topology.Reset();
				ResetTopologyState();
			}
			else
			{
				ResetTopologyState();
				HRESULT topologyHr = _mediaSession->SetTopology(0, _topology.Get());
				if (SUCCEEDED(topologyHr))
				{
					_useMediaSessionAudioCompanion = true;
					_hasAudio = true;
				}
				else
				{
					DebugOutputHr(L"MediaSession audio companion SetTopology failed", topologyHr);
					ShutdownMediaSession();
					_mediaSource.Reset();
					_topology.Reset();
					ResetTopologyState();
				}
			}
		}

		(void)BeginPlaybackEndEpoch(
			GetSourceReaderPlaybackEndMask(), false);
		_mediaLoaded = true;
		OpenPlaybackGate();
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Stopped, notifications.OldState);
		notifications.NewState = PlaybackState::Stopped;
		notifications.ExpectedExplicitCommandGeneration =
			explicitCommandGeneration;
		DeferredPlaybackNotifications openedNotifications{};
		openedNotifications.MediaOpened = true;
		openedNotifications.ExpectedMediaLoadGeneration =
			mediaLoadGeneration;
		openedNotifications.ApplyLoadedBehavior = true;
		openedNotifications.LoadedBehaviorExplicitCommandGeneration =
			explicitCommandGeneration;
		RequestLayout();
		RequestVisualInvalidation();
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(
			std::move(notifications), true);
		RaiseDeferredPlaybackNotifications(
			std::move(openedNotifications), true);
		return true;
	}

	// 每次加载重建 session，避免旧拓扑状态残留
	_useSourceReader = false;
	HRESULT hr = CreateMediaSession();
	if (FAILED(hr)) return failLoad(hr);

	_mediaFile = mediaFile;
	_mediaLoaded = false;
	ResetTopologyState();

	// 清理之前的资源（不再 Shutdown session）
	_mediaSource.Reset();
	_topology.Reset();
	_videoDisplayControl.Reset();
	_presentationClock.Reset();
	ReleaseGpuPresentationResources(true);
	ReleaseVideoFrameBuffers();
	_hasVideo = false;
	_hasAudio = false;
	notifications.PositionChanged = CommitObservedPosition(
		0.0, true, false, notifications.Position);
	_duration = 0.0;

	// 创建媒体源
	hr = CreateMediaSource(mediaFile);
	if (FAILED(hr)) return failLoad(hr);

	// 创建拓扑
	hr = CreateTopology();
	if (FAILED(hr)) return failLoad(hr);

	// 设置拓扑
	hr = _mediaSession->SetTopology(0, _topology.Get());
	if (FAILED(hr)) return failLoad(hr);

	// 完全自渲染：不使用 EVR，不需要 VideoDisplayControl

	_mediaLoaded = true;
	OpenPlaybackGate();

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

	notifications.StateChanged = CommitPlaybackState(
		PlaybackState::Stopped, notifications.OldState);
	notifications.NewState = PlaybackState::Stopped;
	notifications.ExpectedExplicitCommandGeneration =
		explicitCommandGeneration;
	DeferredPlaybackNotifications openedNotifications{};
	openedNotifications.MediaOpened = true;
	openedNotifications.ExpectedMediaLoadGeneration = mediaLoadGeneration;
	openedNotifications.ApplyLoadedBehavior = true;
	openedNotifications.LoadedBehaviorExplicitCommandGeneration =
		explicitCommandGeneration;
	RequestLayout();
	RequestVisualInvalidation();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications), true);
	RaiseDeferredPlaybackNotifications(
		std::move(openedNotifications), true);
	return true;
}

bool MediaElement::Load(const void* data, size_t size, const std::wstring& nameHint)
{
	_requestedState.store(MediaState::Stop, std::memory_order_release);
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	DeferredPlaybackNotifications notifications{};
	notifications.PositionFirst = true;
	auto failPrecondition = [this, &commandLock, &notifications](HRESULT error)
	{
		CommitMediaFailure(error, notifications, false);
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return false;
	};
	if (!data || size == 0)
	{
		return failPrecondition(E_INVALIDARG);
	}
	auto* presentationWindow = this->GetPresentationWindow();
	if (!presentationWindow)
	{
		return failPrecondition(E_HANDLE);
	}
	if (!EnsureInitialized())
	{
		const HRESULT initializationError = _initializationHr;
		return failPrecondition(initializationError);
	}
	const UINT64 explicitCommandGeneration =
		AdvanceExplicitPlaybackCommandGeneration();
	const UINT64 mediaLoadGeneration = AdvanceMediaLoadGeneration();
	notifications.ExpectedMediaLoadGeneration = mediaLoadGeneration;
	auto failLoad = [this, &commandLock, &notifications](HRESULT error)
	{
		CommitMediaFailure(error, notifications, true);
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(
			std::move(notifications), true);
		return false;
	};
	BeginPlaybackQuiescence();
	_mediaLoaded = false;
	WakePlaybackThread();
	CompletePlaybackQuiescence();
	ClearMediaError();
	ResetPerformanceCounters();
	_videoFrameDurationHns.store(333333, std::memory_order_relaxed);
	_videoFrameRateKnown.store(false, std::memory_order_relaxed);
	_presentationRateMonitor = nullptr;
	_lastPresentationRateRefreshQpc = 0;
	RefreshPresentationRateLimit();

	// 强制使用 SourceReader 路径（内存流仅支持 SourceReader）
	_useSourceReader = true;

	// 若之前在 SourceReader 后端播放，先彻底停掉线程/音频
	if (_playThread.joinable() || _threadPlaying.load() || _sourceReader)
	{
		StopSourceReaderPlayback(true);
	}

	_mediaFile = nameHint;
	_mediaLoaded = false;
	notifications.PositionChanged = CommitObservedPosition(
		0.0, true, false, notifications.Position);
	_duration = 0.0;
	_hasVideo = false;
	_hasAudio = false;
	ReleaseVideoFrameBuffers();
	if (_videoBitmap && _ownsVideoBitmap)
		_videoBitmap->Release();
	_videoBitmap = nullptr;
	_ownsVideoBitmap = false;
	_memoryByteStream.Reset();
	_memoryStream.Reset();

	// 构建内存 IStream
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
	if (!hMem) return failLoad(E_OUTOFMEMORY);
	void* pMem = GlobalLock(hMem);
	if (!pMem)
	{
		GlobalFree(hMem);
		return failLoad(E_OUTOFMEMORY);
	}
	memcpy(pMem, data, size);
	GlobalUnlock(hMem);

	ComPtr<IStream> memStream;
	HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &memStream);
	if (FAILED(hr) || !memStream)
	{
		GlobalFree(hMem);
		DebugOutputHr(L"CreateStreamOnHGlobal failed", hr);
		return failLoad(hr);
	}

	ComPtr<IMFByteStream> byteStream;
	hr = MFCreateMFByteStreamOnStream(memStream.Get(), &byteStream);
	if (FAILED(hr) || !byteStream)
	{
		DebugOutputHr(L"MFCreateMFByteStreamOnStream failed", hr);
		return failLoad(hr);
	}

	// 设置名称提示，帮助识别格式（如 .mp4）
	ComPtr<IMFAttributes> bsAttr;
	if (!nameHint.empty() && SUCCEEDED(byteStream.As(&bsAttr)) && bsAttr)
	{
		(void)bsAttr->SetString(MF_BYTESTREAM_ORIGIN_NAME, nameHint.c_str());
	}

	_memoryStream = memStream;
	_memoryByteStream = byteStream;

	if (!InitSourceReaderFromByteStream(byteStream.Get()))
	{
		_mediaLoaded = false;
		return failLoad(_lastMfError.load());
	}

	(void)BeginPlaybackEndEpoch(
		GetSourceReaderPlaybackEndMask(), false);
	_mediaLoaded = true;
	OpenPlaybackGate();
	notifications.StateChanged = CommitPlaybackState(
		PlaybackState::Stopped, notifications.OldState);
	notifications.NewState = PlaybackState::Stopped;
	notifications.ExpectedExplicitCommandGeneration =
		explicitCommandGeneration;
	DeferredPlaybackNotifications openedNotifications{};
	openedNotifications.MediaOpened = true;
	openedNotifications.ExpectedMediaLoadGeneration = mediaLoadGeneration;
	openedNotifications.ApplyLoadedBehavior = true;
	openedNotifications.LoadedBehaviorExplicitCommandGeneration =
		explicitCommandGeneration;
	RequestLayout();
	RequestVisualInvalidation();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications), true);
	RaiseDeferredPlaybackNotifications(
		std::move(openedNotifications), true);
	return true;
}

void MediaElement::Play() { (void)TryPlay(); }

bool MediaElement::TryPlayCore(
	HRESULT& startError, bool startMediaSessionCompanion,
	bool overrideCompanionPosition, double companionPosition)
{
	startError = S_OK;
	if (!_mediaLoaded) return false;
	if (_useSourceReader)
	{
		if (!_sourceReader) return false;
		if (startMediaSessionCompanion
			&& _useMediaSessionAudioCompanion && _mediaSession)
		{
			const double position = overrideCompanionPosition
				? companionPosition
				: _position.load(std::memory_order_acquire);
			if (!QueuePendingStartIfTopologyNotReady(true, position))
			{
				const HRESULT hr = StartPlaybackInternal(true, position);
				if (FAILED(hr))
				{
					startError = hr;
					return false;
				}
			}
		}
		if (_useMediaSessionAudioCompanion && _mediaSession)
		{
			// Start/seek has either queued the topology request or issued the
			// companion Start.  MESessionStarted for this exact epoch opens the
			// reader worker so video cannot establish an early clock lead.
			return true;
		}
		if (!_playThread.joinable())
		{
			_threadExit = false;
			_playThread = std::thread([this] { PlaybackThreadMain(); });
		}
		_threadPlaying = true;
		WakePlaybackThread();
		return true;
	}
	if (QueuePendingStartIfTopologyNotReady(false, 0.0))
		return true;
	const HRESULT hr = StartPlayback();
	if (SUCCEEDED(hr)) return true;
	ClearPendingStart();
	startError = hr;
	return false;
}

bool MediaElement::TryApplyLoadedBehaviorAfterOpen(
	UINT64 expectedExplicitCommandGeneration,
	UINT64 expectedMediaLoadGeneration)
{
	if (expectedExplicitCommandGeneration == 0
		|| expectedMediaLoadGeneration == 0
		|| CurrentExplicitPlaybackCommandGeneration()
			!= expectedExplicitCommandGeneration
		|| CurrentMediaLoadGeneration() != expectedMediaLoadGeneration
		|| !_mediaLoaded.load(std::memory_order_acquire))
	{
		return false;
	}
	MediaState effectiveBehavior = GetPresentationWindow()
		? _loadedBehavior : _unloadedBehavior;
	if (effectiveBehavior == MediaState::Manual)
		effectiveBehavior = _requestedState.load(std::memory_order_acquire);
	if (effectiveBehavior == MediaState::Pause)
	{
		DeferredPlaybackNotifications notifications{};
		notifications.ExpectedExplicitCommandGeneration =
			expectedExplicitCommandGeneration;
		notifications.ExpectedMediaLoadGeneration =
			expectedMediaLoadGeneration;
		{
			auto transition = AcquirePlaybackTransition(
				PlaybackTransitionOrigin::Automatic);
			if (!transition
				|| CurrentExplicitPlaybackCommandGeneration()
					!= expectedExplicitCommandGeneration
				|| CurrentMediaLoadGeneration()
					!= expectedMediaLoadGeneration
				|| !_mediaLoaded.load(std::memory_order_acquire))
			{
				return false;
			}
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Paused, notifications.OldState);
			notifications.NewState = PlaybackState::Paused;
		}
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return true;
	}
	if (effectiveBehavior != MediaState::Play)
	{
		ApplyMediaState(effectiveBehavior);
		return true;
	}
	HRESULT startError = S_OK;
	bool started = false;
	DeferredPlaybackNotifications notifications{};
	notifications.ExpectedExplicitCommandGeneration =
		expectedExplicitCommandGeneration;
	notifications.ExpectedMediaLoadGeneration =
		expectedMediaLoadGeneration;
	{
		auto transition = AcquirePlaybackTransition(
			PlaybackTransitionOrigin::Automatic);
		if (!transition
			|| CurrentExplicitPlaybackCommandGeneration()
				!= expectedExplicitCommandGeneration
			|| CurrentMediaLoadGeneration() != expectedMediaLoadGeneration
			|| !_mediaLoaded.load(std::memory_order_acquire))
		{
			return false;
		}
		if (_playState.load(std::memory_order_acquire)
			== PlaybackState::Playing)
		{
			return true;
		}
		if (_useSourceReader && _useMediaSessionAudioCompanion)
		{
			(void)BeginPlaybackEndEpoch(
				GetSourceReaderPlaybackEndMask(), false);
		}
		started = TryPlayCore(startError);
		if (started)
		{
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Playing, notifications.OldState);
			notifications.NewState = PlaybackState::Playing;
		}
		if (FAILED(startError))
		{
			_lastMfError.store(startError);
			notifications.MediaError = true;
			notifications.Error = startError;
		}
	}
	if (FAILED(startError))
		DebugOutputHr(L"LoadedBehavior Play failed", startError);
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return started;
}

bool MediaElement::TryPlay()
{
	if (_loadedBehavior != MediaState::Manual
		&& _unloadedBehavior != MediaState::Manual)
	{
		return false;
	}
	_requestedState.store(MediaState::Play, std::memory_order_release);
	const MediaState effectiveBehavior = GetPresentationWindow()
		? _loadedBehavior : _unloadedBehavior;
	if (effectiveBehavior != MediaState::Manual)
	{
		ApplyMediaState(effectiveBehavior);
		return false;
	}
	return TryPlayState();
}

bool MediaElement::TryPlayState()
{
	if (!_mediaLoaded.load(std::memory_order_acquire)
		&& _loadedBehavior == MediaState::Manual
		&& GetPresentationWindow() && !_source.empty()
		&& !LoadSourceCore(_source))
	{
		return false;
	}
	return TryPlayImpl(false);
}

bool MediaElement::TryPlayImpl(bool requirePausedState)
{
	HRESULT startError = S_OK;
	bool started = false;
	bool replayedCompletedMedia = false;
	bool completedReplayAttempted = false;
	DeferredPlaybackNotifications notifications{};
	notifications.PositionFirst = true;
	{
		auto transition = AcquirePlaybackTransition(
			PlaybackTransitionOrigin::ExplicitPlay);
		if (!transition) return false;
		if (!_mediaLoaded.load(std::memory_order_acquire)) return false;
		if (requirePausedState
			&& _playState.load(std::memory_order_acquire)
				!= PlaybackState::Paused)
		{
			return false;
		}
		if (_useSourceReader)
		{
			const UINT64 completedEpoch = CurrentPlaybackEndEpoch();
			if (IsPlaybackEndCompletionCurrent(completedEpoch))
			{
				completedReplayAttempted = true;
				notifications.ExpectedExplicitCommandGeneration =
					AdvanceExplicitPlaybackCommandGeneration();
				bool companionStartIssued = false;
				replayedCompletedMedia = TrySeekCore(
					0.0, startError, &companionStartIssued);
				if (replayedCompletedMedia)
					started = TryPlayCore(
						startError, !companionStartIssued, true, 0.0);
			}
			else if (_playState.load(std::memory_order_acquire)
				== PlaybackState::Playing)
			{
				// Play is idempotent.  In particular, do not issue a second
				// companion Start while the first Started event is still pending.
				notifications.ExpectedExplicitCommandGeneration =
					CurrentExplicitPlaybackCommandGeneration();
				started = true;
			}
			else
			{
				notifications.ExpectedExplicitCommandGeneration =
					AdvanceExplicitPlaybackCommandGeneration();
				if (_useMediaSessionAudioCompanion)
				{
					(void)BeginPlaybackEndEpoch(
						GetSourceReaderPlaybackEndMask(), false);
				}
				started = TryPlayCore(startError);
			}
		}
		else
		{
			const StandaloneSessionCommandToken completedToken =
				CaptureStandaloneSessionCompletionToken();
			if (completedToken.Sequence != 0)
			{
				completedReplayAttempted = true;
				notifications.ExpectedExplicitCommandGeneration =
					AdvanceExplicitPlaybackCommandGeneration();
				replayedCompletedMedia =
					TrySeekCore(0.0, startError);
				started = replayedCompletedMedia;
			}
			else if (_playState.load(std::memory_order_acquire)
				== PlaybackState::Playing)
			{
				notifications.ExpectedExplicitCommandGeneration =
					CurrentExplicitPlaybackCommandGeneration();
				started = true;
			}
			else
			{
				notifications.ExpectedExplicitCommandGeneration =
					AdvanceExplicitPlaybackCommandGeneration();
				started = TryPlayCore(startError);
			}
		}
		notifications.ExpectedMediaLoadGeneration =
			CurrentMediaLoadGeneration();
		if (started)
		{
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Playing, notifications.OldState);
			notifications.NewState = PlaybackState::Playing;
		}
		if (replayedCompletedMedia)
		{
			notifications.PositionChanged = CommitObservedPosition(
				0.0, true, false, notifications.Position);
			_needSyncReset.store(true, std::memory_order_release);
			RequestVisualInvalidation();
		}
		if (completedReplayAttempted && !started && SUCCEEDED(startError))
			startError = E_FAIL;
		if (FAILED(startError))
		{
			_lastMfError.store(startError);
			notifications.MediaError = true;
			notifications.Error = startError;
			if (completedReplayAttempted)
			{
				// Advancing the explicit generation intentionally invalidates the
				// queued old EOS callback.  If rewind/start then fails, publish a
				// terminal public state here so that callback cannot leave a ghost
				// Playing state with no live worker/session.
				_threadPlaying.store(false, std::memory_order_release);
				WakePlaybackThread();
				if (_audioClient) (void)_audioClient->Stop();
				notifications.StateChanged = CommitPlaybackState(
					PlaybackState::Stopped, notifications.OldState);
				notifications.NewState = PlaybackState::Stopped;
			}
		}
	}
	if (FAILED(startError))
		DebugOutputHr(L"Play Start failed", startError);
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return started;
}

void MediaElement::Pause() { (void)TryPause(); }

bool MediaElement::TryPause()
{
	if (_loadedBehavior != MediaState::Manual
		&& _unloadedBehavior != MediaState::Manual)
	{
		return false;
	}
	_requestedState.store(MediaState::Pause, std::memory_order_release);
	const MediaState effectiveBehavior = GetPresentationWindow()
		? _loadedBehavior : _unloadedBehavior;
	if (effectiveBehavior != MediaState::Manual)
	{
		ApplyMediaState(effectiveBehavior);
		return false;
	}
	return TryPauseState();
}

bool MediaElement::TryPauseState()
{
	if (!_mediaLoaded.load(std::memory_order_acquire)
		&& _loadedBehavior == MediaState::Manual
		&& GetPresentationWindow() && !_source.empty()
		&& !LoadSourceCore(_source))
	{
		return false;
	}
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (!_mediaLoaded)
	{
		return false;
	}
	const PlaybackState currentState =
		_playState.load(std::memory_order_acquire);
	if (currentState == PlaybackState::Paused) return true;
	DeferredPlaybackNotifications notifications{};
	notifications.ExpectedExplicitCommandGeneration =
		AdvanceExplicitPlaybackCommandGeneration();
	notifications.ExpectedMediaLoadGeneration =
		CurrentMediaLoadGeneration();
	if (currentState == PlaybackState::Stopped)
	{
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Paused, notifications.OldState);
		notifications.NewState = PlaybackState::Paused;
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return true;
	}
	if (currentState != PlaybackState::Playing) return false;
	BeginPlaybackQuiescence();
	PreserveGpuPresentedFrameForRecovery();
	if (_useSourceReader)
	{
		(void)BeginPlaybackEndEpoch(
			GetSourceReaderPlaybackEndMask(), false);
	}
	WakePlaybackThread();
	const bool cancelledPendingStart = ClearPendingStart();
	if (_useSourceReader)
	{
		HRESULT pauseError = _audioClient
			? _audioClient->Stop() : S_OK;
		if (_useMediaSessionAudioCompanion && _mediaSession
			&& !cancelledPendingStart && SUCCEEDED(pauseError))
		{
			pauseError = PausePlayback();
		}
		if (FAILED(pauseError))
		{
			CommitTerminalSourceReaderFailure(
				pauseError, notifications);
			CompletePlaybackQuiescence();
			commandLock.unlock();
			RaiseDeferredPlaybackNotifications(
				std::move(notifications));
			return false;
		}
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Paused, notifications.OldState);
		notifications.NewState = PlaybackState::Paused;
		CompletePlaybackQuiescence();
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return true;
	}
	if (cancelledPendingStart)
	{
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Paused, notifications.OldState);
		notifications.NewState = PlaybackState::Paused;
		CompletePlaybackQuiescence();
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return true;
	}

	const HRESULT hr = PausePlayback();
	if (FAILED(hr))
	{
		OpenPlaybackGate();
		_lastMfError.store(hr);
		notifications.MediaError = true;
		notifications.Error = hr;
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return false;
	}
	notifications.StateChanged = CommitPlaybackState(
		PlaybackState::Paused, notifications.OldState);
	notifications.NewState = PlaybackState::Paused;
	CompletePlaybackQuiescence();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return true;
}

void MediaElement::Stop() { (void)TryStop(); }

bool MediaElement::TryStop()
{
	if (_loadedBehavior != MediaState::Manual
		&& _unloadedBehavior != MediaState::Manual)
	{
		return false;
	}
	_requestedState.store(MediaState::Stop, std::memory_order_release);
	const MediaState effectiveBehavior = GetPresentationWindow()
		? _loadedBehavior : _unloadedBehavior;
	if (effectiveBehavior != MediaState::Manual)
	{
		ApplyMediaState(effectiveBehavior);
		return false;
	}
	return TryStopState();
}

bool MediaElement::TryStopState()
{
	if (!_mediaLoaded.load(std::memory_order_acquire)
		&& _loadedBehavior == MediaState::Manual
		&& GetPresentationWindow() && !_source.empty()
		&& !LoadSourceCore(_source))
	{
		return false;
	}
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (!_mediaLoaded) return false;
	DeferredPlaybackNotifications notifications{};
	notifications.ExpectedExplicitCommandGeneration =
		AdvanceExplicitPlaybackCommandGeneration();
	notifications.ExpectedMediaLoadGeneration =
		CurrentMediaLoadGeneration();
	BeginPlaybackQuiescence();
	WakePlaybackThread();
	const bool cancelledPendingStart = ClearPendingStart();
	if (_useSourceReader)
	{
		(void)BeginPlaybackEndEpoch(
			GetSourceReaderPlaybackEndMask(), false);
		if (_timeStretch) _timeStretch->Reset();
		HRESULT companionStopError = S_OK;
		if (_useMediaSessionAudioCompanion && _mediaSession
			&& !cancelledPendingStart)
		{
			companionStopError = StopPlayback();
		}
		HRESULT stopError = S_OK;
		if (_sourceReader)
		{
			PROPVARIANT var;
			PropVariantInit(&var);
			var.vt = VT_I8;
			var.hVal.QuadPart = 0;
			stopError = _sourceReader->SetCurrentPosition(GUID_NULL, var);
			PropVariantClear(&var);
		}
		const HRESULT failure = FAILED(companionStopError)
			? companionStopError : stopError;
		if (FAILED(failure))
		{
			notifications.PositionChanged = CommitObservedPosition(
				0.0, true, false, notifications.Position);
			CommitTerminalSourceReaderFailure(failure, notifications);
			RequestVisualInvalidation();
			CompletePlaybackQuiescence();
			commandLock.unlock();
			RaiseDeferredPlaybackNotifications(
				std::move(notifications));
			return false;
		}
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Stopped, notifications.OldState);
		notifications.NewState = PlaybackState::Stopped;
		notifications.PositionChanged = CommitObservedPosition(
			0.0, true, false, notifications.Position);
		RequestVisualInvalidation();
		CompletePlaybackQuiescence();
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return true;
	}
	if (cancelledPendingStart)
	{
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Stopped, notifications.OldState);
		notifications.NewState = PlaybackState::Stopped;
		notifications.PositionChanged = CommitObservedPosition(
			0.0, true, false, notifications.Position);
		CompletePlaybackQuiescence();
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return true;
	}

	const HRESULT hr = StopPlayback();
	if (FAILED(hr))
	{
		OpenPlaybackGate();
		_lastMfError.store(hr);
		notifications.MediaError = true;
		notifications.Error = hr;
		commandLock.unlock();
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return false;
	}
	notifications.StateChanged = CommitPlaybackState(
		PlaybackState::Stopped, notifications.OldState);
	notifications.NewState = PlaybackState::Stopped;
	notifications.PositionChanged = CommitObservedPosition(
		0.0, true, false, notifications.Position);
	CompletePlaybackQuiescence();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return true;
}

void MediaElement::Resume() { (void)TryResume(); }

bool MediaElement::TryResume()
{
	if (_loadedBehavior != MediaState::Manual
		&& _unloadedBehavior != MediaState::Manual)
	{
		return false;
	}
	_requestedState.store(MediaState::Play, std::memory_order_release);
	const MediaState effectiveBehavior = GetPresentationWindow()
		? _loadedBehavior : _unloadedBehavior;
	if (effectiveBehavior != MediaState::Manual)
	{
		ApplyMediaState(effectiveBehavior);
		return false;
	}
	return TryPlayImpl(true);
}

void MediaElement::Seek(double seconds) { (void)TrySeek(seconds); }

bool MediaElement::TrySeekCore(
	double seconds, HRESULT& seekError, bool* companionStartIssued)
{
	seekError = S_OK;
	if (companionStartIssued) *companionStartIssued = false;
	if (!_mediaLoaded) return false;
	if (_useSourceReader)
	{
		if (!_sourceReader) return false;
		// The reader can reach EOS and clear its worker flag just before a
		// playing seek acquires the transition.  The coordinated UI completion
		// may still be queued, so preserve the public Playing intent as well as
		// the instantaneous worker flag.
		const bool resumeWorker =
			_threadPlaying.exchange(false, std::memory_order_acq_rel)
			|| _playState.load(std::memory_order_acquire)
				== PlaybackState::Playing;
		WakePlaybackThread();
		if (_playThread.joinable()
			&& _playThread.get_id() != std::this_thread::get_id())
		{
			std::unique_lock lock(_threadMutex);
			_threadIdleCv.wait(lock, [this]
			{
				return !_playbackWorkerActive;
			});
		}
		auto resumeSourceReaderWorker = [this, resumeWorker]
		{
			if (!resumeWorker) return;
			bool mayResume = false;
			{
				std::scoped_lock lock(_threadMutex);
				mayResume = _playbackGate == PlaybackGateState::Open;
				if (mayResume) _threadPlaying = true;
			}
			if (mayResume) WakePlaybackThread();
		};
		auto finishFailedSeek = [this, &resumeSourceReaderWorker]
		{
			if (!_useMediaSessionAudioCompanion)
			{
				_needSyncReset.store(true, std::memory_order_release);
				resumeSourceReaderWorker();
				return;
			}
			// A companion seek is one A/V transaction.  Never resume only the
			// reader after either half fails, or the epoch would wait forever for
			// a companion Ended event that cannot arrive.
			_threadPlaying.store(false, std::memory_order_release);
			_needSyncReset.store(true, std::memory_order_release);
			(void)BeginPlaybackEndEpoch(
				GetSourceReaderPlaybackEndMask(), false);
			if (_mediaSession) (void)StopPlayback();
		};
		if (_timeStretch) _timeStretch->Reset();
		(void)BeginPlaybackEndEpoch(
			GetSourceReaderPlaybackEndMask(), false);
		PROPVARIANT var;
		PropVariantInit(&var);
		var.vt = VT_I8;
		var.hVal.QuadPart = (LONGLONG)(seconds * HNS_PER_SEC);
		const HRESULT hr = _sourceReader->SetCurrentPosition(GUID_NULL, var);
		PropVariantClear(&var);
		if (FAILED(hr))
		{
			finishFailedSeek();
			seekError = hr;
			return false;
		}
		if (resumeWorker && _useMediaSessionAudioCompanion
			&& _mediaSession)
		{
			if (QueuePendingStartIfTopologyNotReady(true, seconds))
			{
				if (companionStartIssued) *companionStartIssued = true;
			}
			else
			{
				const HRESULT companionResult = SetPositionImpl(seconds);
				if (FAILED(companionResult))
				{
					finishFailedSeek();
					seekError = companionResult;
					return false;
				}
				if (companionStartIssued) *companionStartIssued = true;
			}
		}
		_needSyncReset = true;
		// For a companion seek, SetPositionImpl (or the READY continuation)
		// arms the worker for the new epoch.  Resume only after its Started
		// event; otherwise high playback rates amplify the initial A/V skew.
		if (!(resumeWorker && _useMediaSessionAudioCompanion
			&& _mediaSession))
		{
			resumeSourceReaderWorker();
		}
		return true;
	}
	if (QueuePendingStartIfTopologyNotReady(true, seconds))
		return true;

	const HRESULT hr = SetPositionImpl(seconds);
	if (FAILED(hr))
	{
		seekError = hr;
		return false;
	}
	return true;
}

bool MediaElement::TrySeek(double seconds)
{
	HRESULT seekError = S_OK;
	bool seeked = false;
	bool sourceReader = false;
	DeferredPlaybackNotifications notifications{};
	{
		auto transition = AcquirePlaybackTransition(
			PlaybackTransitionOrigin::ExplicitSeek);
		if (!transition) return false;
		if (!_mediaLoaded || !std::isfinite(seconds)) return false;
		seconds = (std::max)(0.0, _duration > 0.0
			? (std::min)(seconds, _duration) : seconds);
		notifications.ExpectedExplicitCommandGeneration =
			AdvanceExplicitPlaybackCommandGeneration();
		notifications.ExpectedMediaLoadGeneration =
			CurrentMediaLoadGeneration();
		sourceReader = _useSourceReader;
		seeked = TrySeekCore(seconds, seekError);
		if (FAILED(seekError))
		{
			if (sourceReader && _useMediaSessionAudioCompanion)
			{
				CommitTerminalSourceReaderFailure(
					seekError, notifications);
			}
			else
			{
				_lastMfError.store(seekError);
				notifications.MediaError = true;
				notifications.Error = seekError;
			}
		}
		else if (seeked)
		{
			notifications.PositionChanged = CommitObservedPosition(
				seconds, true, false, notifications.Position);
			if (sourceReader)
				_needSyncReset.store(true, std::memory_order_release);
			RequestVisualInvalidation();
		}
	}
	if (FAILED(seekError))
	{
		DebugOutputHr(sourceReader
			? L"SourceReader: SetCurrentPosition failed"
			: L"SetPositionImpl failed", seekError);
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return false;
	}
	if (!seeked) return false;
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return true;
}

bool MediaElement::TryRestartAfterMediaEnded(
	UINT64 expectedExplicitCommandGeneration,
	UINT64 expectedMediaLoadGeneration,
	UINT64 expectedSourceReaderEpoch,
	UINT64 expectedStandaloneCompletionSequence)
{
	HRESULT seekError = S_OK;
	HRESULT startError = S_OK;
	bool seeked = false;
	bool started = false;
	bool sourceReader = false;
	DeferredPlaybackNotifications notifications{};
	notifications.PositionFirst = true;
	{
		auto transition = AcquirePlaybackTransition(
			PlaybackTransitionOrigin::Automatic);
		if (!transition
			|| expectedExplicitCommandGeneration == 0
			|| expectedMediaLoadGeneration == 0
			|| CurrentExplicitPlaybackCommandGeneration()
				!= expectedExplicitCommandGeneration
			|| CurrentMediaLoadGeneration() != expectedMediaLoadGeneration
			|| !_loop.load(std::memory_order_acquire) || !_mediaLoaded)
			return false;
		notifications.ExpectedExplicitCommandGeneration =
			expectedExplicitCommandGeneration;
		notifications.ExpectedMediaLoadGeneration =
			expectedMediaLoadGeneration;
		sourceReader = _useSourceReader;
		if (sourceReader)
		{
			if (expectedSourceReaderEpoch == 0
				|| !IsPlaybackEndCompletionCurrent(
					expectedSourceReaderEpoch))
			{
				return false;
			}
		}
		else
		{
			StandaloneSessionCommandToken completedToken{};
			completedToken.Sequence =
				expectedStandaloneCompletionSequence;
			completedToken.ExplicitCommandGeneration =
				expectedExplicitCommandGeneration;
			completedToken.MediaLoadGeneration =
				expectedMediaLoadGeneration;
			if (!IsStandaloneSessionCompletionCurrent(completedToken))
				return false;
		}
		bool companionStartIssued = false;
		seeked = TrySeekCore(
			0.0, seekError, &companionStartIssued);
		if (seeked)
		{
			// Seek already starts/queues the MediaSession at position zero.
			// SourceReader still needs its worker flag reopened, but must not
			// issue a second companion Start that overwrites that position.
			started = sourceReader
				? TryPlayCore(
					startError, !companionStartIssued, true, 0.0)
				: true;
			notifications.PositionChanged = CommitObservedPosition(
				0.0, true, false, notifications.Position);
			if (sourceReader)
				_needSyncReset.store(true, std::memory_order_release);
			RequestVisualInvalidation();
		}
		if (started)
		{
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Playing, notifications.OldState);
			notifications.NewState = PlaybackState::Playing;
		}
		if ((!seeked || !started)
			&& SUCCEEDED(seekError) && SUCCEEDED(startError))
		{
			startError = E_FAIL;
		}
		const HRESULT failure =
			FAILED(seekError) ? seekError : startError;
		if (FAILED(failure))
		{
			_threadPlaying.store(false, std::memory_order_release);
			WakePlaybackThread();
			if (_audioClient) (void)_audioClient->Stop();
			notifications.StateChanged = CommitPlaybackState(
				PlaybackState::Stopped, notifications.OldState);
			notifications.NewState = PlaybackState::Stopped;
			_lastMfError.store(failure);
			notifications.MediaError = true;
			notifications.Error = failure;
		}
	}
	const HRESULT failure = FAILED(seekError) ? seekError : startError;
	if (FAILED(failure))
	{
		DebugOutputHr(L"Loop restart failed", failure);
	}
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return seeked && started;
}

bool MediaElement::TogglePlayback()
{
	return IsPlaying() ? TryPause() : TryPlay();
}

bool MediaElement::SeekBy(double secondsDelta)
{
	return std::isfinite(secondsDelta) && TrySeek(_position.load() + secondsDelta);
}

bool MediaElement::SetProgress(double progress)
{
	if (!_mediaLoaded || !std::isfinite(progress) || _duration <= 0.0) return false;
	return TrySeek((std::clamp)(progress, 0.0, 1.0) * _duration);
}

void MediaElement::Close()
{
	if (_loadedBehavior != MediaState::Manual
		&& _unloadedBehavior != MediaState::Manual)
	{
		return;
	}
	_requestedState.store(MediaState::Close, std::memory_order_release);
	const MediaState effectiveBehavior = GetPresentationWindow()
		? _loadedBehavior : _unloadedBehavior;
	if (effectiveBehavior != MediaState::Manual)
	{
		ApplyMediaState(effectiveBehavior);
		return;
	}
	CloseCore();
}

void MediaElement::CloseCore()
{
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	DeferredPlaybackNotifications notifications{};
	notifications.ExpectedExplicitCommandGeneration =
		AdvanceExplicitPlaybackCommandGeneration();
	(void)AdvanceMediaLoadGeneration();
	BeginPlaybackQuiescence();
	WakePlaybackThread();
	_mediaLoaded = false;
	StopSourceReaderPlayback(true);
	ReleaseResources();
	_memoryByteStream.Reset();
	_memoryStream.Reset();
	_mediaFile.clear();
	notifications.StateChanged = CommitPlaybackState(
		PlaybackState::Stopped, notifications.OldState);
	notifications.NewState = PlaybackState::Stopped;
	notifications.PositionChanged = CommitObservedPosition(
		0.0, true, false, notifications.Position);
	ClearMediaError();
	RequestLayout();
	RequestVisualInvalidation();
	CompletePlaybackQuiescence();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications));
}

HRESULT MediaElement::StartPlayback()
{
	return StartPlaybackInternal(false, 0.0);
}

HRESULT MediaElement::StartPlaybackInternal(
	bool usePosition, double positionSeconds, UINT64* companionStartEpoch)
{
	if (companionStartEpoch) *companionStartEpoch = 0;
	if (!_mediaSession) return E_NOT_VALID_STATE;
	const UINT64 queuedCompanionStartEpoch =
		QueueCompanionSessionStartEpoch();
	if (companionStartEpoch)
		*companionStartEpoch = queuedCompanionStartEpoch;
	// The self-rendered SourceReader worker must not publish a new speed until
	// the audio companion has accepted that exact rate.  Preflight before Start
	// so an unsupported rate cannot launch audio at its previous speed.
	const HRESULT rateResult = SetSpeedRatioImpl(
		_speedRatio.load(std::memory_order_acquire));
	if (FAILED(rateResult))
	{
		CancelCompanionSessionStartEpoch(queuedCompanionStartEpoch);
		return rateResult;
	}

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

	const StandaloneSessionCommandToken standaloneCommand =
		QueueStandaloneSessionCommand(
			StandaloneSessionCommandKind::Start);
	HRESULT hr = _mediaSession->Start(nullptr, &varStart);
	PropVariantClear(&varStart);
	if (SUCCEEDED(hr))
	{
		CommitStandaloneSessionCommandSuccess(standaloneCommand);
		SetVolumeImpl(_volume);
	}
	else
	{
		CancelCompanionSessionStartEpoch(queuedCompanionStartEpoch);
		RestoreStandaloneSessionIdentityAfterCommandFailure(
			standaloneCommand);
		DebugOutputHr(L"IMFMediaSession::Start failed", hr);
	}
	return hr;
}

HRESULT MediaElement::StartPendingPlaybackIfAllowed(
	UINT64* companionStartEpoch,
	UINT64* explicitCommandGeneration,
	UINT64* mediaLoadGeneration)
{
	if (companionStartEpoch) *companionStartEpoch = 0;
	if (explicitCommandGeneration) *explicitCommandGeneration = 0;
	if (mediaLoadGeneration) *mediaLoadGeneration = 0;
	auto transition = AcquirePlaybackTransition(
		PlaybackTransitionOrigin::Automatic);
	if (!transition || !_mediaLoaded) return S_FALSE;
	bool usePosition = false;
	double positionSeconds = 0.0;
	// READY and direct Seek/Play are serialized by the command lease.  A newer
	// direct command either wins first and clears this pending request, or runs
	// after this exact request has been started; an old position can never be
	// applied to a newer playback epoch.
	if (!TakePendingStartIfTopologyReady(usePosition, positionSeconds))
		return S_FALSE;
	if (explicitCommandGeneration)
		*explicitCommandGeneration =
			CurrentExplicitPlaybackCommandGeneration();
	if (mediaLoadGeneration)
		*mediaLoadGeneration = CurrentMediaLoadGeneration();
	return StartPlaybackInternal(
		usePosition, positionSeconds, companionStartEpoch);
}

HRESULT MediaElement::PausePlayback()
{
	const UINT64 companionCommandEpoch =
		QueueCompanionSessionControlEpoch(
			CompanionSessionControlKind::Pause);
	const StandaloneSessionCommandToken standaloneCommand =
		QueueStandaloneSessionCommand(
			StandaloneSessionCommandKind::Pause);
	HRESULT hr = _mediaSession->Pause();
	if (SUCCEEDED(hr))
		CommitStandaloneSessionCommandSuccess(standaloneCommand);
	else
	{
		CancelCompanionSessionControlEpoch(
			CompanionSessionControlKind::Pause,
			companionCommandEpoch);
		RestoreStandaloneSessionIdentityAfterCommandFailure(
			standaloneCommand);
	}
	return hr;
}

HRESULT MediaElement::StopPlayback()
{
	PROPVARIANT varStop;
	PropVariantInit(&varStop);
	varStop.vt = VT_EMPTY;

	const UINT64 companionCommandEpoch =
		QueueCompanionSessionControlEpoch(
			CompanionSessionControlKind::Stop);
	const StandaloneSessionCommandToken standaloneCommand =
		QueueStandaloneSessionCommand(
			StandaloneSessionCommandKind::Stop);
	HRESULT hr = _mediaSession->Stop();
	PropVariantClear(&varStop);
	if (SUCCEEDED(hr))
		CommitStandaloneSessionCommandSuccess(standaloneCommand);
	else
	{
		CancelCompanionSessionControlEpoch(
			CompanionSessionControlKind::Stop,
			companionCommandEpoch);
		RestoreStandaloneSessionIdentityAfterCommandFailure(
			standaloneCommand);
	}

	return hr;
}

HRESULT MediaElement::SetPositionImpl(double seconds)
{
	if (!_mediaSession) return E_NOT_VALID_STATE;

	PROPVARIANT var;
	PropVariantInit(&var);
	var.vt = VT_I8;
	var.hVal.QuadPart = (LONGLONG)(seconds * HNS_PER_SEC);  // 100ns

	const UINT64 companionStartEpoch =
		QueueCompanionSessionStartEpoch();
	const StandaloneSessionCommandToken standaloneCommand =
		QueueStandaloneSessionCommand(
			StandaloneSessionCommandKind::Start);
	HRESULT hr = _mediaSession->Start(nullptr, &var);
	PropVariantClear(&var);
	if (SUCCEEDED(hr))
	{
		CommitStandaloneSessionCommandSuccess(standaloneCommand);
	}
	else
	{
		CancelCompanionSessionStartEpoch(companionStartEpoch);
		RestoreStandaloneSessionIdentityAfterCommandFailure(
			standaloneCommand);
	}
	return hr;
}

HRESULT MediaElement::SetVolumeImpl(double volume)
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

HRESULT MediaElement::SetSpeedRatioImpl(float rate)
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

void MediaElement::ReleaseResources()
{
	ShutdownMediaSession();
	_mediaSource.Reset();
	_topology.Reset();
	_videoDisplayControl.Reset();
	ReleaseVideoFrameBuffers();
	ResetTopologyState();
	_mediaLoaded = false;
	_hasVideo = false;
	_hasAudio = false;
	_duration = 0.0;

	ReleaseGpuPresentationResources(true);
}

void MediaElement::UpdateVideoBitmap()
{
	// 旧 EVR 路径已移除（完全自渲染）
}

void MediaElement::ReleaseGpuPresentationResources(bool releaseBitmap) noexcept
{
	if (_videoBitmapUsesGpuSurface)
	{
		_videoBitmap = nullptr;
		_ownsVideoBitmap = false;
	}
	else if (releaseBitmap && _videoBitmap && _ownsVideoBitmap)
	{
		_videoBitmap->Release();
		_videoBitmap = nullptr;
		_ownsVideoBitmap = false;
	}
	_videoBitmapUsesGpuSurface = false;
	for (auto& bitmap : _gpuOutputBitmaps) bitmap.Reset();
	for (auto& view : _gpuOutputViews) view.Reset();
	for (auto& texture : _gpuOutputTextures) texture.Reset();
	_gpuOutputSlot = GpuOutputBufferCount - 1;
	_gpuVideoFrameIndex = 0;
	_gpuVideoProcessor.Reset();
	_gpuVideoProcessorEnumerator.Reset();
	_gpuVideoContext.Reset();
	_gpuVideoDevice.Reset();
	_gpuPresentationDeviceGeneration = 0;
	_gpuProcessorInputWidth = 0;
	_gpuProcessorInputHeight = 0;
	_gpuProcessorInputFormat = DXGI_FORMAT_UNKNOWN;
	_gpuProcessorFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	_gpuProcessorFrameDurationHns = 0;
	_gpuProcessorOutputWidth = 0;
	_gpuProcessorOutputHeight = 0;
}

bool MediaElement::TryProcessDxgiVideoSample(
	IMFSample* sample, UINT64 generation, D2DGraphics* graphics,
	bool& staleGeneration)
{
	staleGeneration = false;
	if (!sample || !graphics || !TryRebindDxgiDeviceManager())
		return false;

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	UINT64 currentGeneration = 0;
	{
		std::scoped_lock lock(_dxgiStateMutex);
		device = _mediaD3DDevice;
		context = _mediaD3DContext;
		currentGeneration =
			_dxgiDeviceGeneration.load(std::memory_order_acquire);
	}
	if (!device || !context || generation == 0
		|| generation != currentGeneration)
	{
		_statStaleGenerationFrames.fetch_add(1, std::memory_order_relaxed);
		staleGeneration = true;
		return false;
	}

	ComPtr<IMFDXGIBuffer> dxgiBuffer;
	DWORD bufferCount = 0;
	if (FAILED(sample->GetBufferCount(&bufferCount))) return false;
	for (DWORD index = 0; index < bufferCount && !dxgiBuffer; ++index)
	{
		ComPtr<IMFMediaBuffer> buffer;
		if (SUCCEEDED(sample->GetBufferByIndex(index, buffer.GetAddressOf()))
			&& buffer)
			(void)buffer.As(&dxgiBuffer);
	}
	if (!dxgiBuffer) return false;

	ComPtr<ID3D11Texture2D> inputTexture;
	if (FAILED(dxgiBuffer->GetResource(
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(inputTexture.GetAddressOf())))
		|| !inputTexture)
		return false;
	UINT inputSubresource = 0;
	if (FAILED(dxgiBuffer->GetSubresourceIndex(&inputSubresource)))
		return false;
	ComPtr<ID3D11Device> inputDevice;
	inputTexture->GetDevice(inputDevice.GetAddressOf());
	if (!inputDevice || inputDevice.Get() != device.Get())
	{
		_statStaleGenerationFrames.fetch_add(1, std::memory_order_relaxed);
		staleGeneration = true;
		return false;
	}

	D3D11_TEXTURE2D_DESC inputDescription{};
	inputTexture->GetDesc(&inputDescription);
	if (inputDescription.Width == 0 || inputDescription.Height == 0
		|| inputDescription.Format == DXGI_FORMAT_UNKNOWN)
		return false;

	UINT32 cropX = 0;
	UINT32 cropY = 0;
	UINT32 outputWidth = 0;
	UINT32 outputHeight = 0;
	MFVideoTransferMatrix transferMatrix = MFVideoTransferMatrix_Unknown;
	MFNominalRange nominalRange = MFNominalRange_Unknown;
	D3D11_VIDEO_FRAME_FORMAT frameFormat =
		D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	{
		std::scoped_lock lock(_videoFrameMutex);
		cropX = _videoCropX;
		cropY = _videoCropY;
		outputWidth = static_cast<UINT32>((std::max)(0L, _videoSize.cx));
		outputHeight = static_cast<UINT32>((std::max)(0L, _videoSize.cy));
		transferMatrix = _videoTransferMatrix;
		nominalRange = _videoNominalRange;
		frameFormat = _videoD3DFrameFormat;
	}
	if (cropX >= inputDescription.Width || cropY >= inputDescription.Height)
		return false;
	outputWidth = (std::min)(outputWidth, inputDescription.Width - cropX);
	outputHeight = (std::min)(outputHeight, inputDescription.Height - cropY);
	if (outputWidth == 0 || outputHeight == 0) return false;
	// This fast path currently guarantees progressive SDR YUV conversion. Keep
	// interlaced field cadence and BT.2020/HDR out of the legacy color-space API
	// instead of silently presenting them with the wrong matrix.
	if (frameFormat != D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE
		|| transferMatrix == MFVideoTransferMatrix_BT2020_10
		|| transferMatrix == MFVideoTransferMatrix_BT2020_12)
		return false;

	const LONGLONG frameDurationHns = (std::max<LONGLONG>)(
		1, _videoFrameDurationHns.load(std::memory_order_relaxed));
	const bool recreateProcessor =
		!_gpuVideoProcessor || !_gpuOutputTextures[0] || !_gpuOutputViews[0]
		|| !_gpuOutputBitmaps[0]
		|| _gpuPresentationDeviceGeneration != currentGeneration
		|| _gpuProcessorInputWidth != inputDescription.Width
		|| _gpuProcessorInputHeight != inputDescription.Height
		|| _gpuProcessorInputFormat != inputDescription.Format
		|| _gpuProcessorFrameFormat != frameFormat
		|| _gpuProcessorFrameDurationHns != frameDurationHns
		|| _gpuProcessorOutputWidth != outputWidth
		|| _gpuProcessorOutputHeight != outputHeight;

	ComPtr<ID3D11VideoDevice> videoDevice = _gpuVideoDevice;
	ComPtr<ID3D11VideoContext> videoContext = _gpuVideoContext;
	ComPtr<ID3D11VideoProcessorEnumerator> processorEnumerator =
		_gpuVideoProcessorEnumerator;
	ComPtr<ID3D11VideoProcessor> processor = _gpuVideoProcessor;
	auto outputTextures = _gpuOutputTextures;
	auto outputViews = _gpuOutputViews;
	auto outputBitmaps = _gpuOutputBitmaps;
	UINT64 videoFrameIndex = recreateProcessor ? 0 : _gpuVideoFrameIndex;
	const size_t outputSlot = recreateProcessor
		? 0 : ((_gpuOutputSlot + 1) % GpuOutputBufferCount);
	if (recreateProcessor)
	{
		videoDevice.Reset();
		videoContext.Reset();
		processorEnumerator.Reset();
		processor.Reset();
		for (auto& bitmap : outputBitmaps) bitmap.Reset();
		for (auto& view : outputViews) view.Reset();
		for (auto& texture : outputTextures) texture.Reset();
		if (FAILED(device.As(&videoDevice)) || !videoDevice
			|| FAILED(context.As(&videoContext)) || !videoContext)
			return false;

		D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDescription{};
		contentDescription.InputFrameFormat = frameFormat;
		contentDescription.InputWidth = inputDescription.Width;
		contentDescription.InputHeight = inputDescription.Height;
		const UINT frameDuration = static_cast<UINT>(frameDurationHns);
		contentDescription.InputFrameRate = { 10'000'000u, frameDuration };
		contentDescription.OutputWidth = outputWidth;
		contentDescription.OutputHeight = outputHeight;
		contentDescription.OutputFrameRate =
			contentDescription.InputFrameRate;
		contentDescription.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		HRESULT hr = videoDevice->CreateVideoProcessorEnumerator(
			&contentDescription, processorEnumerator.GetAddressOf());
		if (FAILED(hr) || !processorEnumerator) return false;
		UINT inputFormatSupport = 0;
		UINT outputFormatSupport = 0;
		if (FAILED(processorEnumerator->CheckVideoProcessorFormat(
			inputDescription.Format, &inputFormatSupport))
			|| !(inputFormatSupport &
				D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)
			|| FAILED(processorEnumerator->CheckVideoProcessorFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM, &outputFormatSupport))
			|| !(outputFormatSupport &
				D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
			return false;

		hr = videoDevice->CreateVideoProcessor(
			processorEnumerator.Get(), 0, processor.GetAddressOf());
		if (FAILED(hr) || !processor) return false;

		D3D11_TEXTURE2D_DESC outputDescription{};
		outputDescription.Width = outputWidth;
		outputDescription.Height = outputHeight;
		outputDescription.MipLevels = 1;
		outputDescription.ArraySize = 1;
		outputDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		outputDescription.SampleDesc.Count = 1;
		outputDescription.Usage = D3D11_USAGE_DEFAULT;
		outputDescription.BindFlags =
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDescription{};
		outputViewDescription.ViewDimension =
			D3D11_VPOV_DIMENSION_TEXTURE2D;
		outputViewDescription.Texture2D.MipSlice = 0;
		for (size_t slot = 0; slot < GpuOutputBufferCount; ++slot)
		{
			hr = device->CreateTexture2D(
				&outputDescription, nullptr,
				outputTextures[slot].GetAddressOf());
			if (FAILED(hr) || !outputTextures[slot]) return false;
			hr = videoDevice->CreateVideoProcessorOutputView(
				outputTextures[slot].Get(), processorEnumerator.Get(),
				&outputViewDescription, outputViews[slot].GetAddressOf());
			if (FAILED(hr) || !outputViews[slot]) return false;

			ComPtr<IDXGISurface> outputSurface;
			if (FAILED(outputTextures[slot].As(&outputSurface))
				|| !outputSurface)
				return false;
			outputBitmaps[slot].Attach(
				graphics->CreateBitmapFromDxgiSurface(outputSurface.Get()));
			if (!outputBitmaps[slot]) return false;
		}
	}

	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDescription{};
	inputViewDescription.FourCC = 0;
	inputViewDescription.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
	const UINT mipLevels = (std::max)(1u, inputDescription.MipLevels);
	inputViewDescription.Texture2D.MipSlice = inputSubresource % mipLevels;
	inputViewDescription.Texture2D.ArraySlice = inputSubresource / mipLevels;
	if (inputViewDescription.Texture2D.ArraySlice >= inputDescription.ArraySize)
		return false;
	ComPtr<ID3D11VideoProcessorInputView> inputView;
	HRESULT hr = videoDevice->CreateVideoProcessorInputView(
		inputTexture.Get(), processorEnumerator.Get(),
		&inputViewDescription, inputView.GetAddressOf());
	if (FAILED(hr) || !inputView) return false;
	const RECT sourceRect{
		static_cast<LONG>(cropX), static_cast<LONG>(cropY),
		static_cast<LONG>(cropX + outputWidth),
		static_cast<LONG>(cropY + outputHeight) };
	const RECT outputRect{
		0, 0, static_cast<LONG>(outputWidth),
		static_cast<LONG>(outputHeight) };
	videoContext->VideoProcessorSetStreamFrameFormat(
		processor.Get(), 0, frameFormat);
	D3D11_VIDEO_PROCESSOR_COLOR_SPACE inputColorSpace{};
	// The legacy D3D11 color-space descriptor can express BT.601 vs BT.709.
	// For unknown metadata, HD content follows the normal BT.709 convention.
	inputColorSpace.YCbCr_Matrix =
		transferMatrix == MFVideoTransferMatrix_BT709
		|| (transferMatrix == MFVideoTransferMatrix_Unknown
			&& inputDescription.Height >= 720)
		? 1u : 0u;
	if (nominalRange == MFNominalRange_0_255)
		inputColorSpace.Nominal_Range =
			D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
	else if (nominalRange == MFNominalRange_16_235
		|| nominalRange == MFNominalRange_Unknown)
		inputColorSpace.Nominal_Range =
			D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
	videoContext->VideoProcessorSetStreamColorSpace(
		processor.Get(), 0, &inputColorSpace);
	D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace{};
	outputColorSpace.Nominal_Range =
		D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
	videoContext->VideoProcessorSetOutputColorSpace(
		processor.Get(), &outputColorSpace);
	videoContext->VideoProcessorSetOutputAlphaFillMode(
		processor.Get(),
		D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_OPAQUE, 0);
	videoContext->VideoProcessorSetStreamAutoProcessingMode(
		processor.Get(), 0, FALSE);
	videoContext->VideoProcessorSetStreamSourceRect(
		processor.Get(), 0, TRUE, &sourceRect);
	videoContext->VideoProcessorSetStreamDestRect(
		processor.Get(), 0, TRUE, &outputRect);
	videoContext->VideoProcessorSetOutputTargetRect(
		processor.Get(), TRUE, &outputRect);

	D3D11_VIDEO_PROCESSOR_STREAM stream{};
	stream.Enable = TRUE;
	stream.OutputIndex = 0;
	stream.InputFrameOrField = static_cast<UINT>(videoFrameIndex);
	stream.pInputSurface = inputView.Get();
	hr = videoContext->VideoProcessorBlt(
		processor.Get(), outputViews[outputSlot].Get(),
		static_cast<UINT>(videoFrameIndex), 1, &stream);
	if (FAILED(hr)) return false;

	if (recreateProcessor)
	{
		ReleaseGpuPresentationResources(true);
		_gpuVideoDevice = videoDevice;
		_gpuVideoContext = videoContext;
		_gpuVideoProcessorEnumerator = processorEnumerator;
		_gpuVideoProcessor = processor;
		_gpuOutputTextures = outputTextures;
		_gpuOutputViews = outputViews;
		_gpuOutputBitmaps = outputBitmaps;
		_gpuPresentationDeviceGeneration = currentGeneration;
		_gpuProcessorInputWidth = inputDescription.Width;
		_gpuProcessorInputHeight = inputDescription.Height;
		_gpuProcessorInputFormat = inputDescription.Format;
		_gpuProcessorFrameFormat = frameFormat;
		_gpuProcessorFrameDurationHns = frameDurationHns;
		_gpuProcessorOutputWidth = outputWidth;
		_gpuProcessorOutputHeight = outputHeight;
	}
	_gpuOutputSlot = outputSlot;
	_gpuVideoFrameIndex = videoFrameIndex + 1;
	_videoBitmap = _gpuOutputBitmaps[outputSlot].Get();
	_ownsVideoBitmap = false;
	_videoBitmapUsesGpuSurface = true;

	_statGpuVideoProcessorFrames.fetch_add(1, std::memory_order_relaxed);
	return true;
}

void MediaElement::OnRender()
{
	RefreshPresentationRateLimit();
	if (!this->IsVisible) return;
	_statRenderUpdates.fetch_add(1, std::memory_order_relaxed);

	const auto size = this->GetActualSizeDip();
	auto d2d = this->GetDrawingContext();
	ComPtr<IMFSample> gpuSample;
	UINT64 gpuSampleGeneration = 0;
	{
		std::scoped_lock lock(_videoFrameMutex);
		if (_gpuVideoSampleReady)
		{
			gpuSample = std::move(_gpuVideoSample);
			gpuSampleGeneration = _gpuVideoSampleGeneration;
			_gpuVideoSampleGeneration = 0;
			_gpuVideoSampleReady = false;
		}
	}
	bool staleGpuSample = false;
	const bool processedGpuFrame = gpuSample
		&& TryProcessDxgiVideoSample(
			gpuSample.Get(), gpuSampleGeneration, d2d, staleGpuSample);
	if (processedGpuFrame)
	{
		{
			std::scoped_lock lock(_videoFrameMutex);
			_lastPresentedGpuSample = gpuSample;
			_lastPresentedGpuSampleGeneration = gpuSampleGeneration;
		}
		_consecutiveGpuSurfaceImportFailures = 0;
	}
	else if (gpuSample && staleGpuSample)
	{
		// Device rotation deliberately drops surfaces from the retired
		// generation. They are recovery bookkeeping, not import failures, and
		// must not trip gpu-required or the consecutive-failure fallback.
		_consecutiveGpuSurfaceImportFailures = 0;
	}
	else if (gpuSample)
	{
		_statGpuSurfaceImportFailures.fetch_add(1, std::memory_order_relaxed);
		++_consecutiveGpuSurfaceImportFailures;
		if (_consecutiveGpuSurfaceImportFailures >= 3
			&& gpuSampleGeneration != 0)
		{
			_dxgiPresentationFailureGeneration.store(
				gpuSampleGeneration, std::memory_order_release);
		}
	}
	this->BeginRender();

	// 更新播放位置（用于进度条）
	// 关键修复：SourceReader 模式下 position 由播放线程驱动，不能再用 MediaSession 时钟覆盖，否则会来回跳。
	if (!_useSourceReader && _mediaLoaded && _playState == PlaybackState::Playing)
	{
		// Rendering must never synchronously invoke user code: a position
		// handler can delete this control while BeginRender is active.
		UpdatePositionFromClock(false, true);
	}

	// 有视频：尝试更新并绘制最新帧
	if (_hasVideo && _mediaLoaded)
	{
		std::vector<uint8_t> frame;
		UINT32 stride = 0;
		SIZE frameVideoSize{};
		bool hasNewFrame = false;
		bool uploadedNewFrame = processedGpuFrame;
		{
			std::scoped_lock lock(_videoFrameMutex);
			if (_videoFrameReady)
			{
				frame.swap(_videoFrame);
				stride = _videoFrameStride;
				frameVideoSize = _videoFrameVideoSize;
				_videoFrameReady = false;
				hasNewFrame = true;
			}
		}

		// 只有在有新帧时才上传；否则继续绘制上一帧，避免闪烁（背景黑屏）。
		if (hasNewFrame && !frame.empty()
			&& frameVideoSize.cx > 0 && frameVideoSize.cy > 0 && stride > 0)
		{
			HRESULT uploadResult = E_FAIL;
			if (_videoBitmapUsesGpuSurface)
				ReleaseGpuPresentationResources(true);
			// 如果视频尺寸发生变化，必须重建 bitmap，否则右侧/下侧可能残留旧像素（常见表现为绿色条）。
			if (_videoBitmap)
			{
				auto ps = _videoBitmap->GetPixelSize();
				if (ps.width != (UINT32)frameVideoSize.cx
					|| ps.height != (UINT32)frameVideoSize.cy)
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
					rt->CreateBitmap(
						D2D1::SizeU((UINT32)frameVideoSize.cx,
							(UINT32)frameVideoSize.cy),
						nullptr, 0, &props, &_videoBitmap);
					_ownsVideoBitmap = true;
				}
			}

			if (_videoBitmap)
			{
				_statVideoUploadCalls.fetch_add(1, std::memory_order_relaxed);
				_statVideoUploadBytes.fetch_add((UINT64)frame.size(), std::memory_order_relaxed);
				const LARGE_INTEGER tUp0 = QpcNow();
				const UINT32 w = (UINT32)frameVideoSize.cx;
				const UINT32 h = (UINT32)frameVideoSize.cy;
				const UINT32 expectedStride = w * 4;
				const size_t expectedSize = (size_t)expectedStride * (size_t)h;

				if (frame.size() >= expectedSize && stride == expectedStride)
				{
					uploadResult = _videoBitmap->CopyFromMemory(
						nullptr, frame.data(), expectedStride);
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
					uploadResult = _videoBitmap->CopyFromMemory(
						nullptr, normalized.data(), expectedStride);
				}
				else if (frame.size() >= expectedSize)
				{
					// stride 元数据不可信但数据是连续的：按 expectedStride 写入
					uploadResult = _videoBitmap->CopyFromMemory(
						nullptr, frame.data(), expectedStride);
				}
				const LARGE_INTEGER tUp1 = QpcNow();
				const UINT64 uploadTicks = (UINT64)(tUp1.QuadPart - tUp0.QuadPart);
				_statVideoUploadQpcTicks.fetch_add(uploadTicks, std::memory_order_relaxed);
				uploadedNewFrame = SUCCEEDED(uploadResult);
			}
		}
		if (uploadedNewFrame)
			PreservePresentedVideoFrame(frame, stride, frameVideoSize);
		else
			RecycleVideoFrame(frame);

		// 没有新帧也要继续绘制上一帧，避免闪烁
		if (_videoBitmap)
		{
			const auto bitmapSize = _videoBitmap->GetPixelSize();
			if (bitmapSize.width == 0 || bitmapSize.height == 0)
			{
				this->EndRender();
				ReportPerfStatsIfDue();
				return;
			}
			// WPF Viewbox semantics determine the rendered video rectangle.
			float destX = 0.0f;
			float destY = 0.0f;
			float destWidth = size.width;
			float destHeight = size.height;
			
			UINT32 pixelAspectNumerator = 1;
			UINT32 pixelAspectDenominator = 1;
			{
				std::scoped_lock lock(_videoFrameMutex);
				pixelAspectNumerator = _videoPixelAspectNumerator;
				pixelAspectDenominator = _videoPixelAspectDenominator;
			}
			const float pixelAspect = pixelAspectDenominator != 0
				? static_cast<float>(pixelAspectNumerator)
					/ static_cast<float>(pixelAspectDenominator)
				: 1.0f;
			float videoWidth = (float)bitmapSize.width * pixelAspect;
			float videoHeight = (float)bitmapSize.height;
			
			const auto scale = cui::layout::ComputeStretchScaleFactor(
				{ destWidth, destHeight }, { videoWidth, videoHeight },
				_stretch, _stretchDirection);
			const float scaleX = scale.width;
			const float scaleY = scale.height;
			const float scaledWidth = videoWidth * scaleX;
			const float scaledHeight = videoHeight * scaleY;
			destX += (destWidth - scaledWidth) * 0.5f;
			destY += (destHeight - scaledHeight) * 0.5f;
			destWidth = scaledWidth;
			destHeight = scaledHeight;
			
			_statDrawBitmapCalls.fetch_add(1, std::memory_order_relaxed);
			const LARGE_INTEGER tDraw0 = QpcNow();
			d2d->DrawBitmap(_videoBitmap, destX, destY, destWidth, destHeight);
			const LARGE_INTEGER tDraw1 = QpcNow();
			const UINT64 drawTicks = (UINT64)(tDraw1.QuadPart - tDraw0.QuadPart);
			_statDrawBitmapQpcTicks.fetch_add(drawTicks, std::memory_order_relaxed);
			this->EndRender();
			if (uploadedNewFrame) RecordSubmittedVideoFrame();
			ReportPerfStatsIfDue();
			return;
		}
	}
	this->EndRender();
	ReportPerfStatsIfDue();
}

void MediaElement::RecordSubmittedVideoFrame() noexcept
{
	const LARGE_INTEGER now = QpcNow();
	const LONGLONG previous = _statLastSubmittedFrameQpc.exchange(
		now.QuadPart, std::memory_order_relaxed);
	if (previous > 0 && now.QuadPart > previous)
	{
		const LARGE_INTEGER frequency = QpcFreq();
		if (frequency.QuadPart > 0)
		{
			const double milliseconds =
				static_cast<double>(now.QuadPart - previous) * 1000.0
				/ static_cast<double>(frequency.QuadPart);
			const size_t bucket = (std::min)(
				SubmittedIntervalHistogramBucketCount - 1,
				static_cast<size_t>((std::max)(0.0, milliseconds)));
			_statSubmittedIntervalHistogram[bucket].fetch_add(
				1, std::memory_order_relaxed);
		}
	}
	_statSubmittedVideoFrames.fetch_add(1, std::memory_order_relaxed);
}

void MediaElement::NotifyDeviceResourcesInvalidated() noexcept
{
	bool restoredCpuFrame = false;
	{
		std::scoped_lock lock(_videoFrameMutex);
		if (!_videoFrameReady && !_lastPresentedVideoFrame.empty())
		{
			_videoFrame.swap(_lastPresentedVideoFrame);
			_videoFrameStride = _lastPresentedVideoFrameStride;
			_videoFrameVideoSize = _lastPresentedVideoFrameVideoSize;
			_videoFrameReady = true;
			restoredCpuFrame = true;
			_lastPresentedVideoFrameStride = 0;
			_lastPresentedVideoFrameVideoSize = {};
		}
	}
	// A presentation generation change is also the retry boundary for transient
	// D2D-surface import failures. If the shared D3D device was genuinely lost,
	// the recovered host has already created its replacement, so ResetDevice can
	// be performed here even while playback is paused or the mailbox is empty.
	_dxgiPresentationFailureGeneration.store(0, std::memory_order_release);
	_consecutiveGpuSurfaceImportFailures = 0;
	(void)TryRebindDxgiDeviceManager();
	const UINT64 currentGeneration =
		_dxgiDeviceGeneration.load(std::memory_order_acquire);
	if (!restoredCpuFrame && currentGeneration != 0)
	{
		std::scoped_lock lock(_videoFrameMutex);
		if (!_videoFrameReady && !_gpuVideoSampleReady
			&& _lastPresentedGpuSample
			&& _lastPresentedGpuSampleGeneration == currentGeneration)
		{
			_gpuVideoSample = _lastPresentedGpuSample;
			_gpuVideoSampleGeneration = currentGeneration;
			_gpuVideoSampleReady = true;
		}
	}
	ReleaseGpuPresentationResources(true);
	Control::NotifyDeviceResourcesInvalidated();
}

MediaElement::PerformanceSnapshot
MediaElement::GetPerformanceSnapshot() const noexcept
{
	PerformanceSnapshot snapshot;
	const LARGE_INTEGER frequency = QpcFreq();
	snapshot.QpcFrequency = frequency.QuadPart > 0
		? static_cast<UINT64>(frequency.QuadPart) : 0;
	snapshot.VideoPresentationRateLimitHz =
		_videoPresentationRateLimitHz.load(std::memory_order_relaxed);
	snapshot.VideoFrameDurationHns =
		_videoFrameDurationHns.load(std::memory_order_relaxed);
	snapshot.VideoFrameRateKnown =
		_videoFrameRateKnown.load(std::memory_order_relaxed);
	snapshot.ReadSampleCalls =
		_statReadSampleCalls.load(std::memory_order_relaxed);
	snapshot.ReadSampleQpcTicks =
		_statReadSampleQpcTicks.load(std::memory_order_relaxed);
	snapshot.SamplesToContiguousBufferCalls =
		_statSamplesToContigCalls.load(std::memory_order_relaxed);
	snapshot.SamplesToContiguousBufferQpcTicks =
		_statSamplesToContigQpcTicks.load(std::memory_order_relaxed);
	snapshot.DxgiVideoSamples =
		_statDxgiVideoSamples.load(std::memory_order_relaxed);
	snapshot.GpuVideoProcessorFrames =
		_statGpuVideoProcessorFrames.load(std::memory_order_relaxed);
	snapshot.GpuSurfaceImportFailures =
		_statGpuSurfaceImportFailures.load(std::memory_order_relaxed);
	snapshot.CpuFallbackVideoFrames =
		_statCpuFallbackVideoFrames.load(std::memory_order_relaxed);
	snapshot.GpuDeviceRebinds =
		_statGpuDeviceRebinds.load(std::memory_order_relaxed);
	snapshot.StaleGenerationFrames =
		_statStaleGenerationFrames.load(std::memory_order_relaxed);
	snapshot.SharedDeviceGeneration =
		_dxgiDeviceGeneration.load(std::memory_order_relaxed);
	snapshot.AdapterLuid =
		_dxgiAdapterLuid.load(std::memory_order_relaxed);
	snapshot.DxgiDeviceManagerActive =
		_dxgiDeviceManagerActive.load(std::memory_order_relaxed);
	snapshot.DecodedVideoFrames =
		_statDecodedVideoFrames.load(std::memory_order_relaxed);
	snapshot.ConvertedVideoFrames =
		_statVideoConvertCalls.load(std::memory_order_relaxed);
	snapshot.VideoConvertQpcTicks =
		_statVideoConvertQpcTicks.load(std::memory_order_relaxed);
	snapshot.VideoConvertBytes =
		_statVideoConvertBytes.load(std::memory_order_relaxed);
	snapshot.SubmittedVideoFrames =
		_statSubmittedVideoFrames.load(std::memory_order_relaxed);
	snapshot.DroppedLateVideoFrames =
		_statDroppedLateVideoFrames.load(std::memory_order_relaxed);
	snapshot.ThinnedVideoFrames =
		_statThinnedVideoFrames.load(std::memory_order_relaxed);
	snapshot.OverwrittenVideoFrames =
		_statOverwrittenVideoFrames.load(std::memory_order_relaxed);
	snapshot.MaximumVideoLatenessQpcTicks =
		_statMaxVideoLatenessQpcTicks.load(std::memory_order_relaxed);
	std::array<UINT64, SubmittedIntervalHistogramBucketCount>
		intervalHistogram{};
	for (size_t index = 0; index < intervalHistogram.size(); ++index)
	{
		intervalHistogram[index] =
			_statSubmittedIntervalHistogram[index].load(
				std::memory_order_relaxed);
		snapshot.SubmittedFrameIntervalSamples += intervalHistogram[index];
	}
	auto percentileMilliseconds = [&](double percentile)
	{
		if (snapshot.SubmittedFrameIntervalSamples == 0) return 0.0;
		const UINT64 target = static_cast<UINT64>(std::ceil(
			static_cast<double>(snapshot.SubmittedFrameIntervalSamples)
				* percentile));
		UINT64 cumulative = 0;
		for (size_t index = 0; index < intervalHistogram.size(); ++index)
		{
			cumulative += intervalHistogram[index];
			if (cumulative >= target)
				return static_cast<double>(index + 1);
		}
		return static_cast<double>(intervalHistogram.size());
	};
	snapshot.SubmittedFrameIntervalP95Ms =
		percentileMilliseconds(0.95);
	snapshot.SubmittedFrameIntervalP99Ms =
		percentileMilliseconds(0.99);
	snapshot.VisualInvalidationRequests =
		_statVisualInvalidationRequests.load(std::memory_order_relaxed);
	snapshot.CoalescedVisualInvalidations =
		_statCoalescedVisualInvalidations.load(std::memory_order_relaxed);
	snapshot.AudioWriteCalls =
		_statAudioWriteCalls.load(std::memory_order_relaxed);
	snapshot.AudioWriteQpcTicks =
		_statAudioWriteQpcTicks.load(std::memory_order_relaxed);
	snapshot.AudioWriteBytes =
		_statAudioWriteBytes.load(std::memory_order_relaxed);
	snapshot.CompanionSessionStartedEvents =
		_statCompanionSessionStartedEvents.load(std::memory_order_relaxed);
	snapshot.RenderUpdates =
		_statRenderUpdates.load(std::memory_order_relaxed);
	snapshot.VideoUploadCalls =
		_statVideoUploadCalls.load(std::memory_order_relaxed);
	snapshot.VideoUploadQpcTicks =
		_statVideoUploadQpcTicks.load(std::memory_order_relaxed);
	snapshot.VideoUploadBytes =
		_statVideoUploadBytes.load(std::memory_order_relaxed);
	snapshot.DrawBitmapCalls =
		_statDrawBitmapCalls.load(std::memory_order_relaxed);
	snapshot.DrawBitmapQpcTicks =
		_statDrawBitmapQpcTicks.load(std::memory_order_relaxed);
	// Timestamp after the counter loads so no value in this snapshot is
	// charged to a measurement interval that ended before it was observed.
	const LONGLONG started =
		_statMeasurementStartQpc.load(std::memory_order_relaxed);
	if (started > 0)
	{
		const LARGE_INTEGER now = QpcNow();
		snapshot.MeasurementQpcTicks = now.QuadPart > started
			? static_cast<UINT64>(now.QuadPart - started) : 0;
	}
	return snapshot;
}

MediaElement::PerformanceSnapshot
MediaElement::PauseAndGetPerformanceSnapshot()
{
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	if (!_useSourceReader) return GetPerformanceSnapshot();
	DeferredPlaybackNotifications notifications{};
	notifications.ExpectedExplicitCommandGeneration =
		AdvanceExplicitPlaybackCommandGeneration();

	// Close the start gate before touching worker/session state.  If an
	// automatic loop or topology-ready continuation already owns a transition,
	// BeginPlaybackQuiescence waits for it and this stop wins afterwards.
	BeginPlaybackQuiescence();
	PreserveGpuPresentedFrameForRecovery();
	(void)BeginPlaybackEndEpoch(
		GetSourceReaderPlaybackEndMask(), false);
	WakePlaybackThread();
	const bool cancelledPendingStart = ClearPendingStart();
	HRESULT pauseError = _audioClient
		? _audioClient->Stop() : S_OK;
	if (_useMediaSessionAudioCompanion && _mediaSession
		&& !cancelledPendingStart && SUCCEEDED(pauseError))
	{
		pauseError = PausePlayback();
	}

	if (_playThread.joinable())
	{
		std::unique_lock lock(_threadMutex);
		_threadIdleCv.wait(lock, [this]
		{
			return !_playbackWorkerActive;
		});
	}
	if (FAILED(pauseError))
	{
		CommitTerminalSourceReaderFailure(pauseError, notifications);
	}
	else if (_mediaLoaded.load(std::memory_order_acquire))
	{
		notifications.StateChanged = CommitPlaybackState(
			PlaybackState::Paused, notifications.OldState);
		notifications.NewState = PlaybackState::Paused;
		if (notifications.StateChanged)
		{
			RequestVisualInvalidation();
			notifications.StateVisualInvalidationNeeded = false;
		}
	}
	// SetPlaybackState may request one final visual invalidation.  Include that
	// deterministic bookkeeping in the closed measurement interval.
	const PerformanceSnapshot snapshot = GetPerformanceSnapshot();
	CompletePlaybackQuiescence();
	commandLock.unlock();
	RaiseDeferredPlaybackNotifications(std::move(notifications));
	return snapshot;
}

void MediaElement::ResetPerformanceCounters() noexcept
{
	auto reset = [](auto& counter)
	{
		counter.store(0, std::memory_order_relaxed);
	};
	reset(_statReadSampleCalls);
	reset(_statReadSampleQpcTicks);
	reset(_statReadSampleVideoCalls);
	reset(_statReadSampleVideoQpcTicks);
	reset(_statReadSampleAudioCalls);
	reset(_statReadSampleAudioQpcTicks);
	reset(_statSamplesToContigCalls);
	reset(_statSamplesToContigQpcTicks);
	reset(_statDxgiVideoSamples);
	reset(_statGpuVideoProcessorFrames);
	reset(_statGpuSurfaceImportFailures);
	reset(_statCpuFallbackVideoFrames);
	reset(_statGpuDeviceRebinds);
	reset(_statStaleGenerationFrames);
	reset(_statDecodedVideoFrames);
	reset(_statVideoConvertCalls);
	reset(_statVideoConvertQpcTicks);
	reset(_statVideoConvertBytes);
	reset(_statSubmittedVideoFrames);
	for (auto& bucket : _statSubmittedIntervalHistogram) reset(bucket);
	_statLastSubmittedFrameQpc.store(0, std::memory_order_relaxed);
	reset(_statDroppedLateVideoFrames);
	reset(_statThinnedVideoFrames);
	reset(_statOverwrittenVideoFrames);
	reset(_statMaxVideoLatenessQpcTicks);
	reset(_statVisualInvalidationRequests);
	reset(_statCoalescedVisualInvalidations);
	reset(_statAudioWriteCalls);
	reset(_statAudioWriteQpcTicks);
	reset(_statAudioWriteBytes);
	reset(_statCompanionSessionStartedEvents);
	reset(_statRenderUpdates);
	reset(_statVideoUploadCalls);
	reset(_statVideoUploadQpcTicks);
	reset(_statVideoUploadBytes);
	reset(_statDrawBitmapCalls);
	reset(_statDrawBitmapQpcTicks);
	_visualInvalidationPending.store(false, std::memory_order_relaxed);
	const LARGE_INTEGER now = QpcNow();
	_statMeasurementStartQpc.store(now.QuadPart, std::memory_order_relaxed);
	_statLastReportQpc.store(now.QuadPart, std::memory_order_relaxed);
}

void MediaElement::ReportPerfStatsIfDue()
{
	if (!_performanceReportingEnabled.load(std::memory_order_relaxed)) return;
	const LARGE_INTEGER frequency = QpcFreq();
	if (frequency.QuadPart <= 0) return;
	const LARGE_INTEGER now = QpcNow();
	LONGLONG previous = _statLastReportQpc.load(std::memory_order_relaxed);
	if (previous > 0 && now.QuadPart - previous < frequency.QuadPart) return;
	if (!_statLastReportQpc.compare_exchange_strong(
		previous, now.QuadPart, std::memory_order_relaxed))
		return;

	const PerformanceSnapshot snapshot = GetPerformanceSnapshot();
	const double elapsedSeconds = snapshot.QpcFrequency > 0
		? static_cast<double>(snapshot.MeasurementQpcTicks)
			/ static_cast<double>(snapshot.QpcFrequency) : 0.0;
	auto averageMilliseconds = [&](UINT64 ticks, UINT64 calls)
	{
		return calls > 0 ? QpcTicksToMs(ticks) / calls : 0.0;
	};
	wchar_t message[1024]{};
	swprintf_s(message,
		L"[MediaElementPerf] elapsed=%.2fs decoded=%llu dxgi=%llu gpu=%llu converted=%llu "
		L"submitted=%llu dropped=%llu thinned=%llu overwritten=%llu read=%.3fms "
		L"convert=%.3fms upload=%.3fms invalidations=%llu coalesced=%llu\n",
		elapsedSeconds,
		static_cast<unsigned long long>(snapshot.DecodedVideoFrames),
		static_cast<unsigned long long>(snapshot.DxgiVideoSamples),
		static_cast<unsigned long long>(snapshot.GpuVideoProcessorFrames),
		static_cast<unsigned long long>(snapshot.ConvertedVideoFrames),
		static_cast<unsigned long long>(snapshot.SubmittedVideoFrames),
		static_cast<unsigned long long>(snapshot.DroppedLateVideoFrames),
		static_cast<unsigned long long>(snapshot.ThinnedVideoFrames),
		static_cast<unsigned long long>(snapshot.OverwrittenVideoFrames),
		averageMilliseconds(
			snapshot.ReadSampleQpcTicks, snapshot.ReadSampleCalls),
		averageMilliseconds(
			snapshot.VideoConvertQpcTicks, snapshot.ConvertedVideoFrames),
		averageMilliseconds(
			snapshot.VideoUploadQpcTicks, snapshot.VideoUploadCalls),
		static_cast<unsigned long long>(snapshot.VisualInvalidationRequests),
		static_cast<unsigned long long>(snapshot.CoalescedVisualInvalidations));
	PrintLogWide(message);
}

// ========================================
// 属性实现
// ========================================

GET_CPP(MediaElement, MediaElement::PlaybackState, State)
{
	return MediaElement::_playState.load();
}

GET_CPP(MediaElement, std::wstring, Source)
{
	return _source;
}

SET_CPP(MediaElement, std::wstring, Source)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	if (!SetPropertyField(SourceProperty(), _source, std::move(value))) return;
	auto lifetime = weakLifetime.lock();
	if (!lifetime || !*lifetime) return;
	if (_source.empty())
	{
		CloseCore();
		return;
	}
	if (_mediaLoaded.load(std::memory_order_acquire)
		&& _mediaFile != _source)
	{
		CloseCore();
		lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
	}
	if (!GetPresentationWindow()) return;
	if (_loadedBehavior == MediaState::Manual)
	{
		ApplyMediaState(MediaState::Manual);
		return;
	}
	ApplyLoadedBehaviorOnTree();
}

GET_CPP(MediaElement, std::wstring, MediaFile)
{
	return _mediaFile;
}

GET_CPP(MediaElement, double, Position)
{
	return _position.load();
}

SET_CPP(MediaElement, double, Position)
{
	if (_mediaLoaded)
	{
		Seek(value);
	}
}

GET_CPP(MediaElement, double, Duration)
{
	return _duration;
}

GET_CPP(MediaElement, double, NaturalDuration)
{
	return _duration;
}

GET_CPP(MediaElement, double, Volume)
{
	return _volumeValue;
}

SET_CPP(MediaElement, double, Volume)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	if (!SetPropertyField(VolumeProperty(), _volumeValue, value)) return;
	auto lifetime = weakLifetime.lock();
	if (!lifetime || !*lifetime) return;
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	_volume.store(_volumeValue);
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
			if (_useMediaSessionAudioCompanion && _mediaSession)
			{
				SetVolumeImpl(_volume.load());
			}
		}
		else
		{
			// Media Session模式
			SetVolumeImpl(_volume.load());
		}
	}
}

GET_CPP(MediaElement, double, SpeedRatio)
{
	return _speedRatioValue;
}

SET_CPP(MediaElement, double, SpeedRatio)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	const float previousRate = _speedRatio.load(
		std::memory_order_acquire);
	if (!SetPropertyField(
		SpeedRatioProperty(), _speedRatioValue, value))
	{
		return;
	}
	auto lifetime = weakLifetime.lock();
	if (!lifetime || !*lifetime) return;
	const float requestedRate = static_cast<float>(_speedRatioValue);
	std::unique_lock<std::recursive_mutex> commandLock(
		_playbackCommandMutex);
	HRESULT rateError = S_OK;
	if (_mediaLoaded.load(std::memory_order_acquire))
	{
		if (_useSourceReader)
		{
			// SourceReader+WASAPI performs rate conversion in software.  An
			// audio companion must accept the exact same rate first.
			if (_useMediaSessionAudioCompanion && _mediaSession)
				rateError = SetSpeedRatioImpl(requestedRate);
		}
		else if (_mediaSession)
		{
			rateError = SetSpeedRatioImpl(requestedRate);
		}
	}
	if (FAILED(rateError))
	{
		_speedRatio.store(previousRate, std::memory_order_release);
		_lastMfError.store(rateError);
		DeferredPlaybackNotifications notifications{};
		notifications.MediaError = true;
		notifications.Error = rateError;
		notifications.ExpectedExplicitCommandGeneration =
			CurrentExplicitPlaybackCommandGeneration();
		notifications.ExpectedMediaLoadGeneration =
			CurrentMediaLoadGeneration();
		commandLock.unlock();
		(void)SetPropertyField(
			SpeedRatioProperty(), _speedRatioValue,
			static_cast<double>(previousRate));
		lifetime = weakLifetime.lock();
		if (!lifetime || !*lifetime) return;
		if (_speedRatio.load(std::memory_order_acquire)
			!= previousRate)
		{
			return;
		}
		RaiseDeferredPlaybackNotifications(std::move(notifications));
		return;
	}
	_speedRatio.store(requestedRate, std::memory_order_release);
	_needSyncReset.store(true, std::memory_order_release);
	WakePlaybackThread();
}

GET_CPP(MediaElement, MediaState, LoadedBehavior)
{
	return _loadedBehavior;
}

SET_CPP(MediaElement, MediaState, LoadedBehavior)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	if (!SetPropertyField(
		LoadedBehaviorProperty(), _loadedBehavior, value)) return;
	auto lifetime = weakLifetime.lock();
	if (!lifetime || !*lifetime || !GetPresentationWindow()) return;
	ApplyLoadedBehaviorOnTree();
}

GET_CPP(MediaElement, MediaState, UnloadedBehavior)
{
	return _unloadedBehavior;
}

SET_CPP(MediaElement, MediaState, UnloadedBehavior)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	if (!SetPropertyField(
		UnloadedBehaviorProperty(), _unloadedBehavior, value)) return;
	auto lifetime = weakLifetime.lock();
	if (!lifetime || !*lifetime || GetPresentationWindow()) return;
	ApplyMediaState(_unloadedBehavior);
}

GET_CPP(MediaElement, bool, Loop)
{
	return _loop.load(std::memory_order_acquire);
}

SET_CPP(MediaElement, bool, Loop)
{
	std::weak_ptr<std::atomic_bool> weakLifetime = _lifetimeToken;
	if (!SetPropertyField(LoopProperty(), _loopValue, value)) return;
	auto lifetime = weakLifetime.lock();
	if (!lifetime || !*lifetime) return;
	_loop.store(_loopValue, std::memory_order_release);
}

GET_CPP(MediaElement, bool, EnableHardwareDecode)
{
	return _enableHardwareDecode;
}

SET_CPP(MediaElement, bool, EnableHardwareDecode)
{
	(void)SetPropertyField(
		EnableHardwareDecodeProperty(), _enableHardwareDecode, value);
}

GET_CPP(MediaElement, bool, UsingHardwareDecode)
{
	return _usingHardwareDecode;
}

GET_CPP(MediaElement, bool, PreferNv12VideoOutput)
{
	return _preferNv12VideoOutput;
}

SET_CPP(MediaElement, bool, PreferNv12VideoOutput)
{
	(void)SetPropertyField(
		PreferNv12VideoOutputProperty(), _preferNv12VideoOutput, value);
}

GET_CPP(MediaElement, bool, EnableDxgiVideoOutput)
{
	return _enableDxgiVideoOutput;
}

SET_CPP(MediaElement, bool, EnableDxgiVideoOutput)
{
	(void)SetPropertyField(
		EnableDxgiVideoOutputProperty(), _enableDxgiVideoOutput, value);
}

GET_CPP(MediaElement, bool, UsingNv12VideoOutput)
{
	return _usingNv12VideoOutput;
}

GET_CPP(MediaElement, bool, UsingDxgiVideoOutput)
{
	return _dxgiDeviceManagerActive.load(std::memory_order_acquire);
}

GET_CPP(MediaElement, bool, HasVideo)
{
	return _hasVideo;
}

GET_CPP(MediaElement, bool, HasAudio)
{
	return _hasAudio;
}

GET_CPP(MediaElement, cui::core::Size, VideoSize)
{
	std::scoped_lock lock(_videoFrameMutex);
	return cui::core::Size{
		static_cast<float>(_videoSize.cx),
		static_cast<float>(_videoSize.cy) };
}

GET_CPP(MediaElement, int, NaturalVideoWidth)
{
	std::scoped_lock lock(_videoFrameMutex);
	return static_cast<int>(_videoSize.cx);
}

GET_CPP(MediaElement, int, NaturalVideoHeight)
{
	std::scoped_lock lock(_videoFrameMutex);
	return static_cast<int>(_videoSize.cy);
}

GET_CPP(MediaElement, bool, CanPause)
{
	return _mediaLoaded.load(std::memory_order_acquire);
}

GET_CPP(MediaElement, double, Progress)
{
	if (_duration > 0.0)
	{
		return _position.load() / _duration;
	}
	return 0.0;
}

GET_CPP(MediaElement, ::Stretch, Stretch)
{
	return _stretch;
}

SET_CPP(MediaElement, ::Stretch, Stretch)
{
	(void)SetPropertyField(StretchProperty(), _stretch, value);
}

GET_CPP(MediaElement, ::StretchDirection, StretchDirection)
{
	return _stretchDirection;
}

SET_CPP(MediaElement, ::StretchDirection, StretchDirection)
{
	(void)SetPropertyField(
		StretchDirectionProperty(), _stretchDirection, value);
}
