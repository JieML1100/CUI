#pragma once
#include "Control.h"
#include "Brush.h"
#include "Layout/LayoutTypes.h"
#include <wrl/client.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfidl.h>
#include <d3d11.h>
#include <evr.h>
#include <shlwapi.h>
#include <mutex>
#include <vector>
#include <memory>
#include <objbase.h>

#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <array>
#include <deque>

using Microsoft::WRL::ComPtr;

/**
 * @file MediaElement.h
 * @brief MediaElement：基于 Windows Media Foundation 的媒体播放器控件。
 *
 * 设计概览：
 * - 通过 Media Foundation 进行解复用/解码与时钟驱动
 * - 视频侧通过 SourceReader 解码，并由 Direct2D 位图呈现
 * - 音频侧包含 WASAPI 输出与（可选）变速保音调处理（WSOLA）
 *
 * 注意：该头文件包含较多平台相关依赖（MF/EVR/WASAPI），仅在 Windows/MSVC 环境下使用。
 */

// ============================================================================
// MediaElement - Windows 原生媒体播放器控件
// ============================================================================
// 基于 Windows Media Foundation 实现的高性能媒体播放器
// 支持视频和音频播放，采用 Direct2D 渲染视频帧
// 支持常见格式：MP4, MKV, AVI, MOV, WMV, MP3, WAV, FLAC, M4A, WMA, AAC
// ============================================================================

// 前向声明
class MediaElement;
class MediaElementCallback;
class VideoSampleGrabberCallback;
class WsolaTimeStretch;

/**
 * WPF-compatible behavior applied when a MediaElement enters or leaves a
 * presentation tree. This is intentionally distinct from playback state.
 */
enum class MediaState
{
	Manual = 0,
	Play = 1,
	Close = 2,
	Pause = 3,
	Stop = 4
};

// ============================================================================
// MediaElementCallback - Media Foundation 异步事件回调
// ============================================================================
// 实现 IMFAsyncCallback 接口，处理媒体播放过程中的各种事件：
// - MESessionTopologyStatus: 拓扑就绪状态
// - MESessionStarted: 播放开始
// - MESessionPaused: 播放暂停
// - MESessionStopped: 播放停止
// - MESessionEnded: 播放结束
// - MEError: 错误事件
// ============================================================================
class MediaElementCallback : public IMFAsyncCallback
{
public:
	/** @brief 构造回调对象（由 MediaElement 创建并管理）。 */
	MediaElementCallback(MediaElement* player, UINT64 mediaLoadGeneration);
	virtual ~MediaElementCallback();
	/**
	 * @brief 与宿主 MediaElement 解绑定。
	 *
	 * 用于析构/关闭流程，避免回调线程访问已释放对象。
	 */
	void DetachPlayer();

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
	STDMETHODIMP Invoke(IMFAsyncResult* pResult);

private:
	LONG _refCount;
	std::recursive_mutex _playerMutex;
	MediaElement* _player;
	UINT64 _mediaLoadGeneration = 0;
};

inline void MediaElementCallback::DetachPlayer()
{
	std::scoped_lock lock(_playerMutex);
	_player = nullptr;
}

/** @brief 媒体加载完成事件。 */
typedef Event<void(class Control*)> MediaOpenedEvent;
/** @brief 媒体播放结束事件。 */
typedef Event<void(class Control*)> MediaEndedEvent;
/** @brief 媒体加载/播放失败事件。 */
typedef Event<void(class Control*)> MediaFailedEvent;
/** @brief 播放位置变化事件（秒）。 */
typedef Event<void(class Control*, double)> MediaPositionChangedEvent;

// ============================================================================
// MediaElement - 媒体播放器控件
// ============================================================================

/**
 * @brief MediaElement：媒体播放器控件。
 *
 * 对外契约：
 * - Source 声明媒体；Play/Pause/Stop/Close 控制播放
 * - 通过 OnMediaOpened/OnMediaEnded/OnMediaFailed/OnPositionChanged 观察状态
 * - Stretch/StretchDirection 控制视频缩放
 */
class MediaElement : public Control
{
	friend class MediaElementCallback; // 允许回调访问私有成员
	friend class VideoSampleGrabberCallback; // 允许视频帧回调访问私有成员
	friend struct MediaElementRegressionTestAccess;
public:
	/**
	 * @brief Thread-safe cumulative counters for one media load.
	 *
	 * QPC fields use QpcFrequency as their denominator.  The snapshot remains
	 * available even when periodic diagnostic logging is disabled.
	 */
	struct PerformanceSnapshot final
	{
		UINT64 QpcFrequency = 0;
		UINT64 MeasurementQpcTicks = 0;
		UINT32 VideoPresentationRateLimitHz = 0;
		LONGLONG VideoFrameDurationHns = 0;
		bool VideoFrameRateKnown = false;
		UINT64 ReadSampleCalls = 0;
		UINT64 ReadSampleQpcTicks = 0;
		UINT64 SamplesToContiguousBufferCalls = 0;
		UINT64 SamplesToContiguousBufferQpcTicks = 0;
		UINT64 DxgiVideoSamples = 0;
		UINT64 GpuVideoProcessorFrames = 0;
		UINT64 GpuSurfaceImportFailures = 0;
		UINT64 CpuFallbackVideoFrames = 0;
		UINT64 GpuDeviceRebinds = 0;
		UINT64 StaleGenerationFrames = 0;
		UINT64 SharedDeviceGeneration = 0;
		UINT64 AdapterLuid = 0;
		bool DxgiDeviceManagerActive = false;
		UINT64 DecodedVideoFrames = 0;
		UINT64 ConvertedVideoFrames = 0;
		UINT64 VideoConvertQpcTicks = 0;
		UINT64 VideoConvertBytes = 0;
		UINT64 SubmittedVideoFrames = 0;
		UINT64 DroppedLateVideoFrames = 0;
		UINT64 ThinnedVideoFrames = 0;
		UINT64 OverwrittenVideoFrames = 0;
		UINT64 MaximumVideoLatenessQpcTicks = 0;
		UINT64 SubmittedFrameIntervalSamples = 0;
		double SubmittedFrameIntervalP95Ms = 0.0;
		double SubmittedFrameIntervalP99Ms = 0.0;
		UINT64 VisualInvalidationRequests = 0;
		UINT64 CoalescedVisualInvalidations = 0;
		UINT64 AudioWriteCalls = 0;
		UINT64 AudioWriteQpcTicks = 0;
		UINT64 AudioWriteBytes = 0;
		UINT64 CompanionSessionStartedEvents = 0;
		UINT64 RenderUpdates = 0;
		UINT64 VideoUploadCalls = 0;
		UINT64 VideoUploadQpcTicks = 0;
		UINT64 VideoUploadBytes = 0;
		UINT64 DrawBitmapCalls = 0;
		UINT64 DrawBitmapQpcTicks = 0;
	};

	/** @brief 播放状态。 */
	enum class PlaybackState
	{
		Stopped,  // 停止
		Playing,  // 播放中
		Paused    // 暂停
	};

	/** @brief 播放状态发生变化。 */
	using PlaybackStateChangedEvent = Event<void(
		class MediaElement*, PlaybackState oldState, PlaybackState newState)>;
	/** @brief 携带 HRESULT 的详细媒体错误事件。 */
	using MediaErrorEvent = Event<void(class MediaElement*, HRESULT error)>;

private:
	enum class PlaybackGateState
	{
		Open,
		Quiescing,
		Quiesced
	};

	enum class PlaybackTransitionOrigin
	{
		ExplicitPlay,
		ExplicitSeek,
		Automatic
	};

	class PlaybackTransitionLease final
	{
		friend class MediaElement;
		explicit PlaybackTransitionLease(MediaElement* owner) noexcept
			: _owner(owner) {}

	public:
		PlaybackTransitionLease() noexcept = default;
		~PlaybackTransitionLease();
		PlaybackTransitionLease(const PlaybackTransitionLease&) = delete;
		PlaybackTransitionLease& operator=(
			const PlaybackTransitionLease&) = delete;
		PlaybackTransitionLease(PlaybackTransitionLease&& other) noexcept;
		PlaybackTransitionLease& operator=(
			PlaybackTransitionLease&& other) noexcept;
		explicit operator bool() const noexcept { return _owner != nullptr; }

	private:
		MediaElement* _owner = nullptr;
	};
	struct DeferredPlaybackNotifications final
	{
		bool StateChanged = false;
		PlaybackState OldState = PlaybackState::Stopped;
		PlaybackState NewState = PlaybackState::Stopped;
		bool StateVisualInvalidationNeeded = true;
		bool PositionChanged = false;
		double Position = 0.0;
		bool PositionFirst = false;
		bool MediaOpened = false;
		bool MediaFailed = false;
		bool MediaError = false;
		HRESULT Error = S_OK;
		UINT64 ExpectedExplicitCommandGeneration = 0;
		UINT64 ExpectedMediaLoadGeneration = 0;
		bool ApplyLoadedBehavior = false;
		UINT64 LoadedBehaviorExplicitCommandGeneration = 0;
	};

	// ========== 播放状态 ==========
	std::atomic<PlaybackState> _playState{ PlaybackState::Stopped };
	std::wstring _source;
	std::wstring _mediaFile;          // 当前加载的媒体文件路径
	ComPtr<IStream> _memoryStream;    // 内存媒体的 IStream（用于 MFCreateSourceReaderFromByteStream）
	ComPtr<IMFByteStream> _memoryByteStream; // 内存媒体字节流
	MediaState _loadedBehavior = MediaState::Play;
	MediaState _unloadedBehavior = MediaState::Close;
	std::atomic<MediaState> _requestedState{ MediaState::Close };
	bool _loopValue = false;          // 属性系统 backing；工作线程读取原子镜像
	std::atomic<bool> _loop{ false }; // 是否循环播放
	bool _enableHardwareDecode = true; // 是否尝试启用硬件解码/硬件变换（SourceReader/DXVA；失败自动回退）
	bool _enableDxgiVideoOutput = true; // 是否允许 SourceReader 输出共享设备上的 DXGI surface
	bool _usingHardwareDecode = false; // 本次 InitSourceReader 是否以“允许DXVA+硬件变换(best-effort)”模式创建成功
	bool _preferNv12VideoOutput = true; // 是否优先让 SourceReader 输出 NV12（关闭 MF video processing，降低 ReadSample 负担；失败回退 RGB）
	bool _usingNv12VideoOutput = false; // 当前视频输出是否为 NV12
	std::atomic<double> _position{ 0.0 }; // 当前播放位置（秒，可由解码线程更新）
	double _duration = 0.0;           // 媒体总时长（秒）
	double _volumeValue = 0.5;          // 属性系统 backing；播放线程读取下方原子镜像
	std::atomic<double> _volume{ 0.5 }; // 音量 (0.0-1.0)
	double _speedRatioValue = 1.0;    // WPF public surface; worker uses float mirror
	std::atomic<float> _speedRatio{ 1.0f }; // 播放速率
	::Stretch _stretch =
		::Stretch::Uniform;
	::StretchDirection _stretchDirection = ::StretchDirection::Both;

	// ========== Windows Media Foundation 接口 ==========
	ComPtr<IMFMediaSession> _mediaSession;            // 媒体会话
	ComPtr<IMFMediaSource> _mediaSource;              // 媒体源
	ComPtr<IMFPresentationClock> _presentationClock;  // 呈现时钟
	ComPtr<IMFTopology> _topology;                    // 媒体拓扑
	ComPtr<MediaElementCallback> _eventCallback;       // 事件回调
	
	// ========== 视频帧与 Direct2D 呈现 ==========
	ComPtr<IMFVideoDisplayControl> _videoDisplayControl;  // 视频显示控制
	ComPtr<VideoSampleGrabberCallback> _videoSampleCallback;  // 视频帧回调
	std::vector<uint8_t> _videoFrame;                 // 视频帧数据缓冲
	std::vector<uint8_t> _videoFrameSpare;            // UI/解码线程间复用的备用帧缓冲
	std::vector<uint8_t> _lastPresentedVideoFrame;    // 设备恢复时可重新上传的最后一帧
	UINT32 _videoFrameStride = 0;                     // 当前 _videoFrame 的步长（用于 UI 上传；通常为 width*4）
	SIZE _videoFrameVideoSize = { 0, 0 };             // 与当前 _videoFrame 一起发布的可视尺寸
	UINT32 _lastPresentedVideoFrameStride = 0;
	SIZE _lastPresentedVideoFrameVideoSize = { 0, 0 };
	UINT32 _videoStride = 0;                          // 解码输出 stride（来自 MF_MT_DEFAULT_STRIDE；NV12 时为 Y plane stride）
	GUID _videoSubtype = GUID_NULL;                   // SourceReader 实际视频子类型
	UINT32 _videoBytesPerPixel = 4;                   // 视频像素字节数（3=RGB24, 4=RGB32/ARGB32）
	bool _videoBottomUp = false;                      // 是否为倒置图像（stride<0）
	MFVideoTransferMatrix _videoTransferMatrix = MFVideoTransferMatrix_Unknown;
	MFNominalRange _videoNominalRange = MFNominalRange_Unknown;
	D3D11_VIDEO_FRAME_FORMAT _videoD3DFrameFormat =
		D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	SIZE _videoFrameSize = { 0, 0 };                  // 解码输出帧尺寸（可能包含对齐padding）
	UINT32 _videoCropX = 0;                           // 可视区域X偏移（像素）
	UINT32 _videoCropY = 0;                           // 可视区域Y偏移（像素）
	mutable std::mutex _videoFrameMutex;              // 视频帧/格式元数据互斥锁
	bool _videoFrameReady = false;                    // 视频帧就绪标志
	std::atomic<LONGLONG> _videoFrameDurationHns{ 333333 }; // 源视频单帧时长（100ns）
	std::atomic<bool> _videoFrameRateKnown{ false };
	std::atomic<UINT32> _videoPresentationRateLimitHz{ 60 }; // 当前宿主显示器刷新率
	HMONITOR _presentationRateMonitor = nullptr;      // UI线程观察到的当前宿主显示器
	LONGLONG _lastPresentationRateRefreshQpc = 0;     // UI线程刷新显示模式的节流时间
	
	// ========== 媒体信息 ==========
	bool _hasVideo = false;           // 是否包含视频
	bool _hasAudio = false;           // 是否包含音频
	SIZE _videoSize = { 0, 0 };       // 视频尺寸
	UINT32 _videoPixelAspectNumerator = 1;   // 像素宽高比，受 _videoFrameMutex 保护
	UINT32 _videoPixelAspectDenominator = 1;
	bool _initialized = false;        // 是否已初始化
	std::atomic<bool> _mediaLoaded{ false }; // 媒体是否已加载
	// Topology readiness and its pending Start request form one state machine.
	// They must be read/updated under the same lock so the READY callback cannot
	// race between a UI-thread readiness check and publishing the request.
	mutable std::mutex _sessionStateMutex;
	bool _topologyReady = false; // 拓扑是否就绪
	bool _pendingStart = false; // 是否有待处理的启动
	bool _hasPendingStartPosition = false; // 是否有待处理的起始位置
	double _pendingStartPosition = 0.0; // 待处理的起始位置
	// Construction must remain cheap: a declarative tree can contain a hidden
	// MediaElement page.  Media Foundation and its D3D/session worker resources
	// are acquired only by the first actual Load operation.
	bool _initializationAttempted = false;
	HRESULT _initializationHr = E_PENDING;
	bool _mfStarted = false;
	HRESULT _coInitHr = E_UNEXPECTED;       // COM初始化结果
	bool _didCoInit = false;                // 是否执行了COM初始化
	std::atomic<HRESULT> _lastMfError{ S_OK }; // 最后一个 Media Foundation 错误

	// ========== SourceReader + WASAPI 后端（软件解码后备方案） ==========
	bool _preferSourceReader = true;                  // 默认优先使用SourceReader模式
	std::atomic<bool> _useSourceReader{ true };       // 当前媒体是否使用SourceReader模式
	std::atomic<bool> _useMediaSessionAudioCompanion{ false }; // SourceReader 视频 + MediaSession 音频伴随模式
	GUID _sourceReaderAudioSubtype = GUID_NULL;      // SourceReader 当前音频子类型
	bool _sourceReaderAudioNegotiationFailed = false; // SourceReader 音频输出协商是否失败
	ComPtr<IMFSourceReader> _sourceReader;            // 媒体源读取器
	ComPtr<IMFDXGIDeviceManager> _dxgiDeviceManager;  // SourceReader GPU surface allocator
	ComPtr<ID3D11Device> _mediaD3DDevice;             // 与 D2D/DComp 共用的设备快照
	ComPtr<ID3D11DeviceContext> _mediaD3DContext;     // 已启用 multithread protection
	UINT _dxgiDeviceResetToken = 0;
	std::atomic<UINT64> _dxgiDeviceGeneration{ 0 };
	std::atomic<UINT64> _dxgiAdapterLuid{ 0 };
	std::atomic<bool> _dxgiDeviceManagerActive{ false };
	std::atomic<UINT64> _dxgiPresentationFailureGeneration{ 0 };
	UINT32 _consecutiveGpuSurfaceImportFailures = 0;   // UI thread only
	UINT32 _consecutiveCpuVideoBufferLockFailures = 0; // playback worker only
	mutable std::mutex _dxgiStateMutex;
	DWORD _srVideoStream = (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM;  // 视频流索引
	DWORD _srAudioStream = (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;  // 音频流索引
	DWORD _actualVideoStreamIndex = (DWORD)-1;        // 实际视频流索引
	DWORD _actualAudioStreamIndex = (DWORD)-1;        // 实际音频流索引
	std::thread _playThread;                          // 播放线程
	std::atomic<bool> _threadExit{ false };           // 线程退出标志
	std::atomic<bool> _threadPlaying{ false };        // 线程播放标志
	std::atomic<bool> _needSyncReset{ false };        // 需要重置同步标志
	std::mutex _threadMutex;                          // 线程互斥锁
	std::condition_variable _threadCv;                // 线程条件变量
	std::condition_variable _threadIdleCv;            // 播放循环/启动事务空闲通知
	HANDLE _playbackWakeEvent = nullptr;               // 唤醒高精度节拍/音频等待
	std::recursive_mutex _playbackCommandMutex;        // 串行 Play/Seek/自动重启；允许同线程事件重入
	std::atomic<UINT64> _explicitPlaybackCommandGeneration{ 1 };
	std::atomic<UINT64> _mediaLoadGeneration{ 1 };
	bool _playbackWorkerActive = false;                // 受 _threadMutex 保护
	PlaybackGateState _playbackGate = PlaybackGateState::Open; // 受 _threadMutex 保护
	UINT32 _playTransitionsInFlight = 0;               // 受 _threadMutex 保护

	// SourceReader streams and the optional MediaSession audio companion share
	// one end-of-presentation transaction.  Epochs reject delayed MF events
	// after seek/stop/close; the bit mask makes Ended/Loop exactly-once even
	// when audio and video have different durations.
	static constexpr UINT8 PlaybackEndReaderVideo = 0x01;
	static constexpr UINT8 PlaybackEndReaderAudio = 0x02;
	static constexpr UINT8 PlaybackEndCompanionSession = 0x04;
	enum class CompanionSessionObservation : UINT8
	{
		Ignored,
		Accepted,
		FailedCurrent
	};
	enum class CompanionSessionControlKind : UINT8
	{
		Pause,
		Stop
	};
	enum class StandaloneSessionCommandKind : UINT8
	{
		Start,
		Pause,
		Stop
	};
	struct StandaloneSessionCommandToken final
	{
		StandaloneSessionCommandKind Kind =
			StandaloneSessionCommandKind::Start;
		UINT64 Sequence = 0;
		UINT64 ExplicitCommandGeneration = 0;
		UINT64 MediaLoadGeneration = 0;
	};
	mutable std::mutex _playbackEndMutex;
	UINT64 _playbackEndEpoch = 1;
	UINT8 _playbackEndExpectedMask = 0;
	UINT8 _playbackEndObservedMask = 0;
	bool _playbackEndCompletionQueued = false;
	UINT64 _activeCompanionSessionEpoch = 0;
	// SourceReader video must not run ahead of the optional MediaSession audio
	// companion.  The worker is opened only after MESessionStarted confirms the
	// exact playback epoch that requested it.
	UINT64 _pendingSourceReaderWorkerStartEpoch = 0;
	std::deque<UINT64> _pendingCompanionSessionStartEpochs;
	std::deque<UINT64> _pendingCompanionSessionPauseEpochs;
	std::deque<UINT64> _pendingCompanionSessionStopEpochs;
	std::deque<StandaloneSessionCommandToken>
		_pendingStandaloneSessionCommands;
	StandaloneSessionCommandToken _activeStandaloneSessionPlayback{};
	StandaloneSessionCommandToken _completedStandaloneSessionPlayback{};
	UINT64 _nextStandaloneSessionCommandSequence = 0;

	// WASAPI 音频输出
	ComPtr<IMMDeviceEnumerator> _mmDeviceEnumerator;  // 音频设备枚举器
	ComPtr<IMMDevice> _audioDevice;                   // 音频设备
	ComPtr<IAudioClient> _audioClient;                // 音频客户端
	ComPtr<IAudioRenderClient> _audioRenderClient;    // 音频渲染客户端
	WAVEFORMATEX* _audioMixFormat = nullptr;          // 音频混音格式 (CoTaskMemFree)
	UINT32 _audioBlockAlign = 0;                      // 音频块对齐
	UINT32 _audioBytesPerSec = 0;                     // 音频每秒字节数
	UINT32 _audioChannels = 0;                        // 音频声道数
	UINT32 _audioSamplesPerSec = 0;                   // 音频采样率
	UINT32 _audioBitsPerSample = 0;                   // 音频每样本位数
	UINT32 _audioBufferFrameCount = 0;                // 音频缓冲帧数
	HANDLE _audioReadyEvent = nullptr;                 // WASAPI event-driven refill

	// Pitch-preserving time-stretch (WSOLA)
	std::unique_ptr<WsolaTimeStretch> _timeStretch;
	std::vector<std::unique_ptr<WsolaTimeStretch>> _timeStretchChain;
	float _timeStretchChainRate = 1.0f;

	// ========== SourceReader/WASAPI 内部方法 ==========
	bool InitSourceReader(const std::wstring& url);   // 初始化SourceReader
	bool InitSourceReaderFromByteStream(IMFByteStream* byteStream); // 从字节流初始化SourceReader
	void ShutdownSourceReader();                      // 关闭SourceReader
	bool InitWasapi();                                // 初始化WASAPI音频输出
	bool InitWasapiWithFormat(const WAVEFORMATEX* format); // 使用指定格式初始化WASAPI
	void ShutdownWasapi();                            // 关闭WASAPI
	void PlaybackThreadMain();                        // 播放线程主函数
	static UINT64 MeasureWsolaOutputFramesForTesting(
		float rate, UINT32 inputFrames, UINT32 chunkFrames);
	enum class DxgiVideoSampleDisposition : UINT8
	{
		Published,
		CpuFallbackEligible,
		DropStale
	};
	bool ConfigureSourceReaderVideoType();            // 配置SourceReader视频类型
	bool ConfigureSourceReaderAudioTypeFromMixFormat(); // 配置SourceReader音频类型
	void UpdateVideoFormatFromSourceReader();         // 从sourceReader更新视频格式
	HRESULT WriteAudioToWasapi(const BYTE* data, UINT32 bytes, bool dropRemainderIfFull = false);  // 将音频数据写入WASAPI
	HRESULT WaitForWasapiDrain();
	void WakePlaybackThread() noexcept;
	void StopSourceReaderPlayback(bool shutdown);     // 停止SourceReader播放（可选关闭WASAPI/Reader）
	bool ConfigureSourceReaderDxgiManager(IMFAttributes* attributes);
	void ReleaseSourceReaderDxgiManager() noexcept;
	DxgiVideoSampleDisposition TryPublishDxgiVideoSample(IMFSample* sample);
	bool TryRebindDxgiDeviceManager();
	UINT8 GetSourceReaderPlaybackEndMask() const noexcept;
	UINT64 BeginPlaybackEndEpoch(
		UINT8 expectedMask, bool discardPendingSessionStarts = false) noexcept;
	UINT64 CurrentPlaybackEndEpoch() const noexcept;
	UINT64 QueueCompanionSessionStartEpoch() noexcept;
	void CancelCompanionSessionStartEpoch(UINT64 epoch) noexcept;
	UINT64 CaptureCompanionSessionFailureEpoch() const noexcept;
	UINT64 QueueCompanionSessionControlEpoch(
		CompanionSessionControlKind kind) noexcept;
	void CancelCompanionSessionControlEpoch(
		CompanionSessionControlKind kind, UINT64 epoch) noexcept;
	CompanionSessionObservation ObserveCompanionSessionControl(
		CompanionSessionControlKind kind, HRESULT eventStatus,
		UINT64* observedEpoch = nullptr) noexcept;
	CompanionSessionObservation ObserveCompanionSessionStarted(
		HRESULT eventStatus, UINT64* observedEpoch = nullptr) noexcept;
	bool TakeSourceReaderWorkerStartForEpoch(UINT64 epoch) noexcept;
	void StartSourceReaderWorkerAfterCompanionStarted(UINT64 epoch);
	CompanionSessionObservation ObserveCompanionSessionEnded(
		HRESULT eventStatus, UINT64* observedEpoch = nullptr);
	void HandleCompanionSessionFailure(HRESULT error, UINT64 expectedEpoch);
	void HandleSourceReaderFailure(
		HRESULT error, UINT64 expectedEpoch,
		UINT64 expectedExplicitCommandGeneration,
		UINT64 expectedMediaLoadGeneration);
	StandaloneSessionCommandToken QueueStandaloneSessionCommand(
		StandaloneSessionCommandKind kind) noexcept;
	void CancelStandaloneSessionCommand(
		const StandaloneSessionCommandToken& token) noexcept;
	void RestoreStandaloneSessionIdentityAfterCommandFailure(
		const StandaloneSessionCommandToken& token) noexcept;
	void CommitStandaloneSessionCommandSuccess(
		const StandaloneSessionCommandToken& token) noexcept;
	CompanionSessionObservation ObserveStandaloneSessionCommand(
		StandaloneSessionCommandKind kind, HRESULT eventStatus,
		StandaloneSessionCommandToken* observedToken = nullptr) noexcept;
	CompanionSessionObservation ObserveStandaloneSessionEnded(
		HRESULT eventStatus,
		StandaloneSessionCommandToken* observedToken = nullptr) noexcept;
	bool IsStandaloneSessionCompletionCurrent(
		const StandaloneSessionCommandToken& token) const noexcept;
	StandaloneSessionCommandToken CaptureStandaloneSessionFailureToken()
		const noexcept;
	StandaloneSessionCommandToken CaptureStandaloneSessionCompletionToken()
		const noexcept;
	void HandleStandaloneSessionFailure(
		HRESULT error, StandaloneSessionCommandToken token);
	void QueueStandaloneSessionCompletion(
		StandaloneSessionCommandToken token);
	bool SignalPlaybackEnd(UINT8 streamMask, UINT64 epoch);
	bool IsPlaybackEndCompletionCurrent(UINT64 epoch) const noexcept;
	void QueuePlaybackEndCompletion(
		UINT64 epoch, UINT64 explicitCommandGeneration,
		UINT64 mediaLoadGeneration);

	// ========== 视频渲染 ==========
	ID2D1Bitmap* _videoBitmap = nullptr;              // 视频位图
	bool _ownsVideoBitmap = false;                    // 是否拥有位图
	bool _videoBitmapUsesGpuSurface = false;
	ComPtr<IMFSample> _gpuVideoSample;                 // 单槽解码 surface mailbox
	UINT64 _gpuVideoSampleGeneration = 0;
	bool _gpuVideoSampleReady = false;
	ComPtr<IMFSample> _lastPresentedGpuSample;         // 暂停/呈现恢复用输入 surface
	UINT64 _lastPresentedGpuSampleGeneration = 0;
	ComPtr<ID3D11VideoDevice> _gpuVideoDevice;
	ComPtr<ID3D11VideoContext> _gpuVideoContext;
	ComPtr<ID3D11VideoProcessorEnumerator> _gpuVideoProcessorEnumerator;
	ComPtr<ID3D11VideoProcessor> _gpuVideoProcessor;
	static constexpr size_t GpuOutputBufferCount = 3;
	std::array<ComPtr<ID3D11Texture2D>, GpuOutputBufferCount>
		_gpuOutputTextures;
	std::array<ComPtr<ID3D11VideoProcessorOutputView>, GpuOutputBufferCount>
		_gpuOutputViews;
	std::array<ComPtr<ID2D1Bitmap1>, GpuOutputBufferCount>
		_gpuOutputBitmaps;
	size_t _gpuOutputSlot = GpuOutputBufferCount - 1;
	UINT64 _gpuVideoFrameIndex = 0;
	UINT64 _gpuPresentationDeviceGeneration = 0;
	UINT32 _gpuProcessorInputWidth = 0;
	UINT32 _gpuProcessorInputHeight = 0;
	DXGI_FORMAT _gpuProcessorInputFormat = DXGI_FORMAT_UNKNOWN;
	D3D11_VIDEO_FRAME_FORMAT _gpuProcessorFrameFormat =
		D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	LONGLONG _gpuProcessorFrameDurationHns = 0;
	UINT32 _gpuProcessorOutputWidth = 0;
	UINT32 _gpuProcessorOutputHeight = 0;
	bool TryProcessDxgiVideoSample(
		IMFSample* sample, UINT64 generation, D2DGraphics* graphics,
		bool& staleGeneration);
	void ReleaseGpuPresentationResources(bool releaseBitmap = true) noexcept;

	// ========== Media Foundation 内部方法 ==========
	bool EnsureInitialized();
	HRESULT InitializeMF();                           // 初始化Media Foundation
	HRESULT CreateMediaSession();                     // 创建媒体会话
	void ShutdownMediaSession();                      // 关闭媒体会话
	HRESULT EnsureVideoDisplayControl();              // 确保视频显示控制器
	void UpdatePositionFromClock(
		bool forceEvent, bool deferNotification = false); // 从时钟更新位置
	HRESULT CreateMediaSource(const std::wstring& url);  // 创建媒体源
	HRESULT CreateTopology();                         // 创建媒体拓扑
	HRESULT CreateAudioOnlyTopology();                // 创建仅音频拓扑
	HRESULT InitializeVideoRenderer();                // 初始化视频渲染器
	void OnVideoFrame(const BYTE* data, DWORD size);  // 视频帧回调
	void RefreshVideoFormatFromSource();              // 从媒体源刷新视频格式
	HRESULT StartPlayback();                          // 开始播放
	HRESULT StartPlaybackInternal(
		bool usePosition, double positionSeconds,
		UINT64* companionStartEpoch = nullptr);          // 内部开始播放
	HRESULT StartPendingPlaybackIfAllowed(
		UINT64* companionStartEpoch = nullptr,
		UINT64* explicitCommandGeneration = nullptr,
		UINT64* mediaLoadGeneration = nullptr);          // READY 后受闸门保护的延迟启动
	HRESULT PausePlayback();                          // 暂停播放
	HRESULT StopPlayback();                           // 停止播放
	HRESULT SetPositionImpl(double seconds);          // 设置播放位置实现
	HRESULT SetVolumeImpl(double volume);             // 设置音量实现
	HRESULT SetSpeedRatioImpl(float rate);          // 设置播放速率实现
	void ReleaseResources();                          // 释放资源
	void UpdateVideoBitmap();                         // 更新视频位图
	void ReportPerfStatsIfDue();                      // 输出每秒性能统计（调试用）
	bool CommitPlaybackState(PlaybackState value, PlaybackState& oldValue) noexcept;
	void SetPlaybackState(PlaybackState value);
	void RaisePlaybackStateChanged(PlaybackState oldValue, PlaybackState value);
	bool CommitObservedPosition(
		double value, bool notify, bool forceEvent,
		double& committedValue) noexcept;
	void RaiseDeferredPlaybackNotifications(
		DeferredPlaybackNotifications notifications,
		bool deferOnOwner = false);
	bool QueuePendingStartIfTopologyNotReady(
		bool usePosition, double position) noexcept;
	void MarkTopologyReady() noexcept;
	bool TakePendingStartIfTopologyReady(
		bool& usePosition, double& position) noexcept;
	bool ClearPendingStart() noexcept;
	void ResetTopologyState() noexcept;
	PlaybackTransitionLease AcquirePlaybackTransition(
		PlaybackTransitionOrigin origin) noexcept;
	UINT64 AdvanceExplicitPlaybackCommandGeneration() noexcept;
	UINT64 CurrentExplicitPlaybackCommandGeneration() const noexcept;
	UINT64 AdvanceMediaLoadGeneration() noexcept;
	UINT64 CurrentMediaLoadGeneration() const noexcept;
	void EndPlaybackTransition() noexcept;
	void BeginPlaybackQuiescence() noexcept;
	void CompletePlaybackQuiescence() noexcept;
	void OpenPlaybackGate() noexcept;
	bool TryPlayCore(
		HRESULT& startError, bool startMediaSessionCompanion = true,
		bool overrideCompanionPosition = false,
		double companionPosition = 0.0);
	bool TryPlayImpl(bool requirePausedState);
	bool TryPlayState();
	bool TryPauseState();
	bool TryStopState();
	void CloseCore();
	bool TryApplyLoadedBehaviorAfterOpen(
		UINT64 expectedExplicitCommandGeneration,
		UINT64 expectedMediaLoadGeneration);
	bool LoadSourceCore(const std::wstring& mediaFile);
	void ApplyLoadedBehaviorOnTree();
	void ApplyMediaState(MediaState behavior);
	bool TrySeekCore(
		double seconds, HRESULT& seekError,
		bool* companionStartIssued = nullptr);
	bool TryRestartAfterMediaEnded(
		UINT64 expectedExplicitCommandGeneration,
		UINT64 expectedMediaLoadGeneration,
		UINT64 expectedSourceReaderEpoch,
		UINT64 expectedStandaloneCompletionSequence);
	void SetObservedPosition(
		double value, bool notify = true, bool forceEvent = false);
	void ReportMediaFailure(
		HRESULT error, bool deferOnOwner = false,
		bool stopPlaybackState = true);
	void CommitMediaFailure(
		HRESULT error, DeferredPlaybackNotifications& notifications,
		bool stopPlaybackState = true);
	void CommitTerminalSourceReaderFailure(
		HRESULT error, DeferredPlaybackNotifications& notifications);
	void DispatchToOwner(std::function<void()> callback);
	void RequestVisualInvalidation();
	std::vector<uint8_t> AcquireVideoFrameBuffer();
	void PublishVideoFrame(
		std::vector<uint8_t>&& frame, UINT32 stride, SIZE videoSize);
	void RecycleVideoFrame(std::vector<uint8_t>& frame);
	void PreservePresentedVideoFrame(
		std::vector<uint8_t>& frame, UINT32 stride, SIZE videoSize);
	void PreserveGpuPresentedFrameForRecovery() noexcept;
	void ReleaseVideoFrameBuffers() noexcept;
	void RefreshPresentationRateLimit() noexcept;
	void RecordSubmittedVideoFrame() noexcept;

	// ---- 跨线程事件封送助手 ----
	// 播放/解码工作线程上触发的事件统一经这些助手封送回 UI 线程 invoke，
	// 避免用户事件处理器在错误线程触碰其他 UI 控件。已用 _lifetimeToken 防护
	// 控件在回调执行前销毁的悬空访问。
	void FireMediaOpened();
	void FireMediaEnded();
	void FirePositionChanged(
		double value, UINT64 expectedPlaybackEpoch = 0,
		UINT64 expectedExplicitCommandGeneration = 0,
		UINT64 expectedMediaLoadGeneration = 0);
	void FireMediaError(HRESULT error);

public:
	// ========== 构造/析构 ==========
	/// <summary>
	/// 构造函数
	/// </summary>
	/// <param name="x">控件X坐标</param>
	/// <param name="y">控件Y坐标</param>
	/// <param name="width">控件宽度</param>
	/// <param name="height">控件高度</param>
	/** @brief 创建媒体播放器控件。 */
	MediaElement();
	virtual ~MediaElement();

	// ========== 事件 ==========
	/** @brief 媒体加载完成时触发。 */
	MediaOpenedEvent OnMediaOpened;
	/** @brief 媒体播放结束时触发。 */
	MediaEndedEvent OnMediaEnded;
	/** @brief 媒体加载/播放失败时触发。 */
	MediaFailedEvent OnMediaFailed;
	/** @brief 播放位置变化时触发（秒）。 */
	MediaPositionChangedEvent OnPositionChanged;
	/** @brief 播放状态实际变化时触发。 */
	PlaybackStateChangedEvent OnPlaybackStateChanged;
	/** @brief 发生媒体错误时触发，并提供原始 HRESULT。 */
	MediaErrorEvent OnMediaError;

	static bool ConvertNV12ToBGRA(
		const uint8_t* nv12, size_t nv12Bytes, UINT32 nv12Stride,
		UINT32 srcW, UINT32 srcH, UINT32 cropX, UINT32 cropY,
		UINT32 w, UINT32 h, MFVideoTransferMatrix transferMatrix,
		MFNominalRange nominalRange, std::vector<uint8_t>& outBGRA);
	virtual UIClass Type() override;
	static void RegisterDependencyProperties();
	static const DependencyProperty& SourceProperty();
	static const DependencyProperty& VolumeProperty();
	static const DependencyProperty& SpeedRatioProperty();
	static const DependencyProperty& LoadedBehaviorProperty();
	static const DependencyProperty& UnloadedBehaviorProperty();
	static const DependencyProperty& LoopProperty();
	static const DependencyProperty& EnableHardwareDecodeProperty();
	static const DependencyProperty& EnableDxgiVideoOutputProperty();
	static const DependencyProperty& PreferNv12VideoOutputProperty();
	static const DependencyProperty& StretchProperty();
	static const DependencyProperty& StretchDirectionProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Custom, L"MediaElement");
	}
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void OnPresentationWindowChanged(
		PresentationWindow* previousWindow,
		PresentationWindow* currentWindow) override;
	void OnRender() override;
	void NotifyDeviceResourcesInvalidated() noexcept override;
public:
	// ========== 媒体控制方法 ==========
	/// <summary>
	/// 加载媒体文件
	/// </summary>
	/// <param name="mediaFile">媒体文件路径</param>
	/// <returns>加载成功返回true，否则返回false</returns>
	bool Load(const std::wstring& mediaFile);

	/// <summary>
	/// 从内存加载媒体
	/// </summary>
	/// <param name="data">媒体数据指针</param>
	/// <param name="size">媒体数据大小</param>
	/// <param name="nameHint">用于识别格式的名称/扩展名提示（可为空）</param>
	/// <returns>加载成功返回true，否则返回false</returns>
	bool Load(const void* data, size_t size, const std::wstring& nameHint = L"memory");
	
	/// <summary>
	/// 播放媒体（如果当前处于暂停状态，则继续播放）
	/// </summary>
	/**
	 * WPF manual-control contract: Play/Pause/Stop/Resume/Close are accepted
	 * only when LoadedBehavior or UnloadedBehavior is Manual. The behavior for
	 * the element's current tree state still has priority; Try* reports false
	 * and the void wrapper is a no-op when the request cannot take effect now.
	 */
	void Play();
	
	/// <summary>
	/// 暂停播放
	/// </summary>
	void Pause();
	
	/// <summary>
	/// 停止播放并重置到起始位置
	/// </summary>
	void Stop();
	
	/// <summary>
	/// 继续播放（从暂停状态恢复）
	/// </summary>
	void Resume();
	
	/// <summary>
	/// 跳转到指定位置
	/// </summary>
	/// <param name="seconds">位置（秒）</param>
	void Seek(double seconds);

	/** @brief 尝试开始或继续播放；操作未被接受时返回 false。 */
	bool TryPlay();
	/** @brief 尝试暂停；Manual 模式下会按需打开 Source。 */
	bool TryPause();
	/** @brief 尝试停止并回到起点。 */
	bool TryStop();
	/** @brief 尝试从暂停状态继续播放。 */
	bool TryResume();
	/** @brief 尝试跳转到指定秒数。 */
	bool TrySeek(double seconds);
	/** @brief 在播放与暂停之间切换。 */
	bool TogglePlayback();
	/** @brief 相对当前播放位置跳转。 */
	bool SeekBy(double secondsDelta);
	/** @brief 按 0..1 的归一化进度跳转。 */
	bool SetProgress(double progress);
	/** @brief Manual 状态下关闭媒体并保留 Source，后续控制可按需重新打开。 */
	void Close();
	
	/// <summary>
	/// 检查是否可以播放
	/// </summary>
	/// <returns>如果已加载媒体则返回true，否则返回false</returns>
	bool CanPlay() const { return _mediaLoaded; }
	bool IsLoaded() const { return _mediaLoaded; }
	bool IsPlaying() const { return _playState.load() == PlaybackState::Playing; }
	bool IsPaused() const { return _playState.load() == PlaybackState::Paused; }
	bool IsStopped() const { return _playState.load() == PlaybackState::Stopped; }
	bool HasMediaError() const { return FAILED(_lastMfError.load()); }
	HRESULT GetLastMediaError() const { return _lastMfError.load(); }
	void ClearMediaError() { _lastMfError.store(S_OK); }
	/** Returns cumulative performance counters for the current media load. */
	PerformanceSnapshot GetPerformanceSnapshot() const noexcept;
	/** Pauses playback, waits for the SourceReader worker to quiesce, then snapshots counters. */
	PerformanceSnapshot PauseAndGetPerformanceSnapshot();
	/** Resets performance counters without changing playback state. */
	void ResetPerformanceCounters() noexcept;
	/** Enables or disables the once-per-second diagnostic log. */
	void SetPerformanceReportingEnabled(bool value) noexcept
	{
		_performanceReportingEnabled.store(value);
	}
	bool IsPerformanceReportingEnabled() const noexcept
	{
		return _performanceReportingEnabled.load();
	}

	// ========== 属性 ==========
	// 播放状态（只读）
	READONLY_PROPERTY(PlaybackState, State);
	GET(PlaybackState, State);

	// WPF-compatible declarative source.
	PROPERTY(std::wstring, Source);
	GET(std::wstring, Source);
	SET(std::wstring, Source);

	// 当前已打开的媒体路径（CUI 诊断扩展，只读）
	READONLY_PROPERTY(std::wstring, MediaFile);
	GET(std::wstring, MediaFile);

	// 当前播放位置（秒），可读写
	PROPERTY(double, Position);
	GET(double, Position);
	SET(double, Position);

	// WPF 名称；当前以秒表示，待 CUI 引入 Duration/TimeSpan 值类型。
	READONLY_PROPERTY(double, NaturalDuration);
	GET(double, NaturalDuration);

	// CUI 兼容名称（秒）（只读）
	PROPERTY(double, Duration);
	GET(double, Duration);

	// 音量 (0.0-1.0)，可读写
	PROPERTY(double, Volume);
	GET(double, Volume);
	SET(double, Volume);

	// WPF SpeedRatio（默认 1.0），可读写
	PROPERTY(double, SpeedRatio);
	GET(double, SpeedRatio);
	SET(double, SpeedRatio);

	PROPERTY(MediaState, LoadedBehavior);
	GET(MediaState, LoadedBehavior);
	SET(MediaState, LoadedBehavior);
	PROPERTY(MediaState, UnloadedBehavior);
	GET(MediaState, UnloadedBehavior);
	SET(MediaState, UnloadedBehavior);

	// 是否循环播放，可读写
	PROPERTY(bool, Loop);
	GET(bool, Loop);
	SET(bool, Loop);

	// 是否尝试启用硬件解码/硬件变换（SourceReader/DXVA），可读写
	PROPERTY(bool, EnableHardwareDecode);
	GET(bool, EnableHardwareDecode);
	SET(bool, EnableHardwareDecode);

	// 是否允许 GPU DXGI surface 呈现；关闭时仍可保留硬件解码策略用于 A/B 与兼容回退
	PROPERTY(bool, EnableDxgiVideoOutput);
	GET(bool, EnableDxgiVideoOutput);
	SET(bool, EnableDxgiVideoOutput);

	// 当前是否处于“best-effort 硬件模式”（只读；仅表示 SourceReader 创建策略，最终是否真走硬解取决于系统解码器/驱动）
	READONLY_PROPERTY(bool, UsingHardwareDecode);
	GET(bool, UsingHardwareDecode);

	// 是否优先使用 NV12 视频输出（可读写，best-effort；失败会回退 RGB）
	PROPERTY(bool, PreferNv12VideoOutput);
	GET(bool, PreferNv12VideoOutput);
	SET(bool, PreferNv12VideoOutput);

	// 当前是否正在使用 NV12 视频输出（只读）
	READONLY_PROPERTY(bool, UsingNv12VideoOutput);
	GET(bool, UsingNv12VideoOutput);

	// 当前 SourceReader 是否绑定共享 DXGI 设备管理器（实际 GPU 帧以性能计数为准）
	READONLY_PROPERTY(bool, UsingDxgiVideoOutput);
	GET(bool, UsingDxgiVideoOutput);

	// 是否包含视频（只读）
	READONLY_PROPERTY(bool, HasVideo);
	GET(bool, HasVideo);

	// 是否包含音频（只读）
	READONLY_PROPERTY(bool, HasAudio);
	GET(bool, HasAudio);

	// 视频尺寸（只读）
	READONLY_PROPERTY(cui::core::Size, VideoSize);
	GET(cui::core::Size, VideoSize);
	READONLY_PROPERTY(int, NaturalVideoWidth);
	GET(int, NaturalVideoWidth);
	READONLY_PROPERTY(int, NaturalVideoHeight);
	GET(int, NaturalVideoHeight);
	READONLY_PROPERTY(bool, CanPause);
	GET(bool, CanPause);

	// 播放进度 (0.0 - 1.0)（只读）
	READONLY_PROPERTY(double, Progress);
	GET(double, Progress);

	PROPERTY(::Stretch, Stretch);
	GET(::Stretch, Stretch);
	SET(::Stretch, Stretch);
	PROPERTY(::StretchDirection, StretchDirection);
	GET(::StretchDirection, StretchDirection);
	SET(::StretchDirection, StretchDirection);

private:
	// ========== 诊断：性能统计（每秒输出一次） ==========
	std::atomic<UINT64> _statReadSampleCalls{ 0 };
	std::atomic<UINT64> _statReadSampleQpcTicks{ 0 };
	std::atomic<UINT64> _statReadSampleVideoCalls{ 0 };
	std::atomic<UINT64> _statReadSampleVideoQpcTicks{ 0 };
	std::atomic<UINT64> _statReadSampleAudioCalls{ 0 };
	std::atomic<UINT64> _statReadSampleAudioQpcTicks{ 0 };
	std::atomic<UINT64> _statSamplesToContigCalls{ 0 };
	std::atomic<UINT64> _statSamplesToContigQpcTicks{ 0 };
	std::atomic<UINT64> _statDxgiVideoSamples{ 0 };
	std::atomic<UINT64> _statGpuVideoProcessorFrames{ 0 };
	std::atomic<UINT64> _statGpuSurfaceImportFailures{ 0 };
	std::atomic<UINT64> _statCpuFallbackVideoFrames{ 0 };
	std::atomic<UINT64> _statGpuDeviceRebinds{ 0 };
	std::atomic<UINT64> _statStaleGenerationFrames{ 0 };

	std::atomic<UINT64> _statDecodedVideoFrames{ 0 };
	std::atomic<UINT64> _statVideoConvertCalls{ 0 };
	std::atomic<UINT64> _statVideoConvertQpcTicks{ 0 };
	std::atomic<UINT64> _statVideoConvertBytes{ 0 };
	std::atomic<UINT64> _statSubmittedVideoFrames{ 0 };
	static constexpr size_t SubmittedIntervalHistogramBucketCount = 256;
	std::array<std::atomic<UINT64>, SubmittedIntervalHistogramBucketCount>
		_statSubmittedIntervalHistogram{};
	std::atomic<LONGLONG> _statLastSubmittedFrameQpc{ 0 };
	std::atomic<UINT64> _statDroppedLateVideoFrames{ 0 };
	std::atomic<UINT64> _statThinnedVideoFrames{ 0 };
	std::atomic<UINT64> _statOverwrittenVideoFrames{ 0 };
	std::atomic<UINT64> _statMaxVideoLatenessQpcTicks{ 0 };
	std::atomic<UINT64> _statVisualInvalidationRequests{ 0 };
	std::atomic<UINT64> _statCoalescedVisualInvalidations{ 0 };
	std::atomic<bool> _visualInvalidationPending{ false };

	std::atomic<UINT64> _statAudioWriteCalls{ 0 };
	std::atomic<UINT64> _statAudioWriteQpcTicks{ 0 };
	std::atomic<UINT64> _statAudioWriteBytes{ 0 };
	std::atomic<UINT64> _statCompanionSessionStartedEvents{ 0 };

	std::atomic<UINT64> _statRenderUpdates{ 0 };
	std::atomic<UINT64> _statVideoUploadCalls{ 0 };
	std::atomic<UINT64> _statVideoUploadQpcTicks{ 0 };
	std::atomic<UINT64> _statVideoUploadBytes{ 0 };
	std::atomic<UINT64> _statDrawBitmapCalls{ 0 };
	std::atomic<UINT64> _statDrawBitmapQpcTicks{ 0 };

	std::atomic<LONGLONG> _statLastReportQpc{ 0 };
	std::atomic<LONGLONG> _statMeasurementStartQpc{ 0 };
	std::atomic<bool> _performanceReportingEnabled{ false };
};

