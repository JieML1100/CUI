#pragma once
#include "Control.h"
#include <wrl/client.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfidl.h>
#include <evr.h>
#include <d3d11.h>
#include <dxgi1_2.h>
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

using Microsoft::WRL::ComPtr;

#if defined(_MSC_VER)
// Media Foundation + EVR
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "evr.lib")

// D3D/DXGI
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// WASAPI / MMCSS
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "avrt.lib")

// misc
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#endif

// ============================================================================
// MediaPlayer - Windows 原生媒体播放器控件
// ============================================================================
// 基于 Windows Media Foundation 实现的高性能媒体播放器
// 支持视频和音频播放，采用 Direct2D 渲染视频帧
// 支持常见格式：MP4, MKV, AVI, MOV, WMV, MP3, WAV, FLAC, M4A, WMA, AAC
// ============================================================================

// 前向声明
class MediaPlayer;
class MediaPlayerCallback;
class VideoSampleGrabberCallback;
class WsolaTimeStretch;

// ============================================================================
// MediaPlayerCallback - Media Foundation 异步事件回调
// ============================================================================
// 实现 IMFAsyncCallback 接口，处理媒体播放过程中的各种事件：
// - MESessionTopologyStatus: 拓扑就绪状态
// - MESessionStarted: 播放开始
// - MESessionPaused: 播放暂停
// - MESessionStopped: 播放停止
// - MESessionEnded: 播放结束
// - MEError: 错误事件
// ============================================================================
class MediaPlayerCallback : public IMFAsyncCallback
{
public:
	MediaPlayerCallback(MediaPlayer* player);
	virtual ~MediaPlayerCallback();
	void DetachPlayer();

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
	STDMETHODIMP Invoke(IMFAsyncResult* pResult);

private:
	LONG _refCount;
	MediaPlayer* _player;
};

inline void MediaPlayerCallback::DetachPlayer() { _player = nullptr; }

// 媒体播放器事件定义
typedef Event<void(class Control*)> MediaOpenedEvent;              // 媒体加载完成事件
typedef Event<void(class Control*)> MediaEndedEvent;               // 媒体播放结束事件
typedef Event<void(class Control*)> MediaFailedEvent;              // 媒体加载/播放失败事件
typedef Event<void(class Control*, double)> MediaPositionChangedEvent;  // 播放位置变化事件

// ============================================================================
// MediaPlayer - 媒体播放器控件
// ============================================================================
class MediaPlayer : public Control
{
	friend class MediaPlayerCallback; // 允许回调访问私有成员
	friend class VideoSampleGrabberCallback; // 允许视频帧回调访问私有成员
public:
	// 播放状态枚举
	enum class PlayState
	{
		Stopped,  // 停止
		Playing,  // 播放中
		Paused    // 暂停
	};

	// 视频渲染模式
	enum class VideoRenderMode
	{
		Fit,        // 适应（等比缩放，居中显示，默认）
		Fill,       // 填充（等比缩放，裁剪以填满控件）
		Stretch,    // 拉伸（填满控件，可能变形）
		Center,     // 居中（原始尺寸，居中显示）
		UniformToFill // 均匀填充（等比缩放，填满控件）
	};

private:
	// ========== 播放状态 ==========
	PlayState _playState = PlayState::Stopped;
	std::wstring _mediaFile;          // 当前加载的媒体文件路径
	bool _autoPlay = true;            // 是否自动播放
	bool _loop = false;               // 是否循环播放
	double _position = 0.0;           // 当前播放位置（秒）
	double _duration = 0.0;           // 媒体总时长（秒）
	std::atomic<double> _volume{ 1.0 }; // 音量 (0.0-1.0)
	std::atomic<float> _playbackRate{ 1.0f }; // 播放速率
	VideoRenderMode _renderMode = VideoRenderMode::Fit; // 视频渲染模式

	// ========== Windows Media Foundation 接口 ==========
	ComPtr<IMFMediaSession> _mediaSession;            // 媒体会话
	ComPtr<IMFMediaSource> _mediaSource;              // 媒体源
	ComPtr<IMFPresentationClock> _presentationClock;  // 呈现时钟
	ComPtr<IMFTopology> _topology;                    // 媒体拓扑
	ComPtr<MediaPlayerCallback> _eventCallback;       // 事件回调
	
	// ========== Direct3D/Direct2D 互操作 ==========
	ComPtr<ID3D11Device> _d3dDevice;                  // D3D11设备
	ComPtr<ID3D11DeviceContext> _d3dContext;          // D3D11上下文
	ComPtr<IDXGISwapChain1> _swapChain;               // DXGI交换链
	ComPtr<IMFVideoDisplayControl> _videoDisplayControl;  // 视频显示控制
	ComPtr<VideoSampleGrabberCallback> _videoSampleCallback;  // 视频帧回调
	std::vector<uint8_t> _videoFrame;                 // 视频帧数据缓冲
	UINT32 _videoStride = 0;                          // 视频帧步长
	GUID _videoSubtype = GUID_NULL;                   // SourceReader 实际视频子类型
	UINT32 _videoBytesPerPixel = 4;                   // 视频像素字节数（3=RGB24, 4=RGB32/ARGB32）
	bool _videoBottomUp = false;                      // 是否为倒置图像（stride<0）
	SIZE _videoFrameSize = { 0, 0 };                  // 解码输出帧尺寸（可能包含对齐padding）
	UINT32 _videoCropX = 0;                           // 可视区域X偏移（像素）
	UINT32 _videoCropY = 0;                           // 可视区域Y偏移（像素）
	std::mutex _videoFrameMutex;                      // 视频帧互斥锁
	bool _videoFrameReady = false;                    // 视频帧就绪标志
	
	// ========== 媒体信息 ==========
	bool _hasVideo = false;           // 是否包含视频
	bool _hasAudio = false;           // 是否包含音频
	SIZE _videoSize = { 0, 0 };       // 视频尺寸
	bool _initialized = false;        // 是否已初始化
	bool _mediaLoaded = false;        // 媒体是否已加载
	bool _topologyReady = false;      // 拓扑是否就绪
	bool _pendingStart = false;       // 是否有待处理的启动
	bool _hasPendingStartPosition = false;  // 是否有待处理的起始位置
	double _pendingStartPosition = 0.0;     // 待处理的起始位置
	HRESULT _coInitHr = E_UNEXPECTED;       // COM初始化结果
	bool _didCoInit = false;                // 是否执行了COM初始化
	HRESULT _lastMfError = S_OK;            // 最后一个 Media Foundation 错误

	// ========== SourceReader + WASAPI 后端（软件解码后备方案） ==========
	bool _useSourceReader = true;                     // 是否使用SourceReader模式
	ComPtr<IMFSourceReader> _sourceReader;            // 媒体源读取器
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

	// Pitch-preserving time-stretch (WSOLA)
	std::unique_ptr<WsolaTimeStretch> _timeStretch;

	// ========== SourceReader/WASAPI 内部方法 ==========
	bool InitSourceReader(const std::wstring& url);   // 初始化SourceReader
	void ShutdownSourceReader();                      // 关闭SourceReader
	bool InitWasapi();                                // 初始化WASAPI音频输出
	void ShutdownWasapi();                            // 关闭WASAPI
	void PlaybackThreadMain();                        // 播放线程主函数
	bool ConfigureSourceReaderVideoType();            // 配置SourceReader视频类型
	bool ConfigureSourceReaderAudioTypeFromMixFormat(); // 配置SourceReader音频类型
	void UpdateVideoFormatFromSourceReader();         // 从sourceReader更新视频格式
	bool WriteAudioToWasapi(const BYTE* data, UINT32 bytes);  // 将音频数据写入WASAPI
	void StopSourceReaderPlayback(bool shutdown);     // 停止SourceReader播放（可选关闭WASAPI/Reader）

	// ========== 视频渲染 ==========
	ID2D1Bitmap* _videoBitmap = nullptr;              // 视频位图
	bool _ownsVideoBitmap = false;                    // 是否拥有位图

	// ========== Media Foundation 内部方法 ==========
	HRESULT InitializeMF();                           // 初始化Media Foundation
	HRESULT CreateMediaSession();                     // 创建媒体会话
	void ShutdownMediaSession();                      // 关闭媒体会话
	HRESULT EnsureVideoDisplayControl();              // 确保视频显示控制器
	void UpdatePositionFromClock(bool forceEvent);    // 从时钟更新位置
	HRESULT CreateMediaSource(const std::wstring& url);  // 创建媒体源
	HRESULT CreateTopology();                         // 创建媒体拓扑
	HRESULT InitializeD3D();                          // 初始化Direct3D
	HRESULT InitializeVideoRenderer();                // 初始化视频渲染器
	void OnVideoFrame(const BYTE* data, DWORD size);  // 视频帧回调
	void RefreshVideoFormatFromSource();              // 从媒体源刷新视频格式
	HRESULT StartPlayback();                          // 开始播放
	HRESULT StartPlaybackInternal(bool usePosition, double positionSeconds);  // 内部开始播放
	HRESULT PausePlayback();                          // 暂停播放
	HRESULT StopPlayback();                           // 停止播放
	HRESULT SetPositionImpl(double seconds);          // 设置播放位置实现
	HRESULT SetVolumeImpl(double volume);             // 设置音量实现
	HRESULT SetPlaybackRateImpl(float rate);          // 设置播放速率实现
	void ReleaseResources();                          // 释放资源
	void UpdateVideoBitmap();                         // 更新视频位图

public:
	// ========== 构造/析构 ==========
	/// <summary>
	/// 构造函数
	/// </summary>
	/// <param name="x">控件X坐标</param>
	/// <param name="y">控件Y坐标</param>
	/// <param name="width">控件宽度</param>
	/// <param name="height">控件高度</param>
	MediaPlayer(int x, int y, int width = 640, int height = 360);
	virtual ~MediaPlayer();

	// ========== 事件 ==========
	MediaOpenedEvent OnMediaOpened;              // 媒体加载完成时触发
	MediaEndedEvent OnMediaEnded;                // 媒体播放结束时触发
	MediaFailedEvent OnMediaFailed;              // 媒体加载/播放失败时触发
	MediaPositionChangedEvent OnPositionChanged; // 播放位置变化时触发

	// ========== 重写基类方法 ==========
	virtual UIClass Type() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	// ========== 媒体控制方法 ==========
	/// <summary>
	/// 加载媒体文件
	/// </summary>
	/// <param name="mediaFile">媒体文件路径</param>
	/// <returns>加载成功返回true，否则返回false</returns>
	bool Load(const std::wstring& mediaFile);
	
	/// <summary>
	/// 播放媒体（如果当前处于暂停状态，则继续播放）
	/// </summary>
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
	
	/// <summary>
	/// 检查是否可以播放
	/// </summary>
	/// <returns>如果已加载媒体则返回true，否则返回false</returns>
	bool CanPlay() const { return _mediaLoaded; }

	// ========== 属性 ==========
	// 播放状态（只读）
	PROPERTY(PlayState, State);
	GET(PlayState, State);

	// 媒体文件路径（只读）
	PROPERTY(std::wstring, MediaFile);
	GET(std::wstring, MediaFile);

	// 当前播放位置（秒），可读写
	PROPERTY(double, Position);
	GET(double, Position);
	SET(double, Position);

	// 媒体总时长（秒）（只读）
	PROPERTY(double, Duration);
	GET(double, Duration);

	// 音量 (0.0-1.0)，可读写
	PROPERTY(double, Volume);
	GET(double, Volume);
	SET(double, Volume);

	// 播放速率（默认1.0为正常速度），可读写
	PROPERTY(float, PlaybackRate);
	GET(float, PlaybackRate);
	SET(float, PlaybackRate);

	// 是否自动播放，可读写
	PROPERTY(bool, AutoPlay);
	GET(bool, AutoPlay);
	SET(bool, AutoPlay);

	// 是否循环播放，可读写
	PROPERTY(bool, Loop);
	GET(bool, Loop);
	SET(bool, Loop);

	// 是否包含视频（只读）
	PROPERTY(bool, HasVideo);
	GET(bool, HasVideo);

	// 是否包含音频（只读）
	PROPERTY(bool, HasAudio);
	GET(bool, HasAudio);

	// 视频尺寸（只读）
	PROPERTY(SIZE, VideoSize);
	GET(SIZE, VideoSize);

	// 播放进度 (0.0 - 1.0)（只读）
	READONLY_PROPERTY(double, Progress);
	GET(double, Progress);

	// 视频渲染模式，可读写
	PROPERTY(VideoRenderMode, RenderMode);
	GET(VideoRenderMode, RenderMode);
	SET(VideoRenderMode, RenderMode);
};

