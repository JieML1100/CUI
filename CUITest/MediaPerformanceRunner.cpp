#include "MediaPerformanceRunner.h"

#include <Application.h>
#include <MediaElement.h>
#include <Window.h>
#include <WindowInfrastructure.h>

#include <Windows.h>
#include <psapi.h>
#include <shellapi.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Psapi.lib")

namespace
{
	constexpr UINT_PTR MeasurementTimerId = 0x4D50;
	constexpr UINT MeasurementTimerIntervalMs = 25;
	constexpr double StartupTimeoutSeconds = 15.0;
	constexpr double WatchdogStartupGraceSeconds = 5.0;
	constexpr double WatchdogCompletionGraceSeconds = 15.0;
	constexpr UINT CompleteAfterMediaErrorMessage = WM_APP + 0x4D;
	constexpr UINT64 MinimumPostRecoveryFrames = 5;
	constexpr UINT64 RecoveryFrameCapacityMargin = 2;

	double QuerySeconds(LARGE_INTEGER start, LARGE_INTEGER end)
	{
		LARGE_INTEGER frequency{};
		if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
			return 0.0;
		return static_cast<double>(end.QuadPart - start.QuadPart)
			/ static_cast<double>(frequency.QuadPart);
	}

	LARGE_INTEGER QueryCounter()
	{
		LARGE_INTEGER value{};
		(void)QueryPerformanceCounter(&value);
		return value;
	}

	struct ProcessSample final
	{
		UINT64 Cpu100Nanoseconds = 0;
		SIZE_T WorkingSetBytes = 0;
		SIZE_T PeakWorkingSetBytes = 0;
	};

	class UniqueHandle final
	{
	public:
		explicit UniqueHandle(HANDLE value = nullptr) noexcept : _value(value) {}
		~UniqueHandle()
		{
			if (_value && _value != INVALID_HANDLE_VALUE)
				(void)CloseHandle(_value);
		}
		UniqueHandle(const UniqueHandle&) = delete;
		UniqueHandle& operator=(const UniqueHandle&) = delete;
		HANDLE Get() const noexcept { return _value; }
		explicit operator bool() const noexcept
		{
			return _value && _value != INVALID_HANDLE_VALUE;
		}

	private:
		HANDLE _value = nullptr;
	};

	struct MediaWatchdogContext final
	{
		HANDLE Started = nullptr;
		HANDLE CompletionStarted = nullptr;
		HANDLE Completed = nullptr;
		double DurationSeconds = 0.0;
	};

	DWORD WaitForWatchdogCompletion(
		const MediaWatchdogContext& context) noexcept
	{
		const DWORD completionWait = WaitForSingleObject(
			context.Completed,
			static_cast<DWORD>(WatchdogCompletionGraceSeconds * 1000.0));
		if (completionWait == WAIT_OBJECT_0) return ERROR_SUCCESS;
		(void)TerminateProcess(GetCurrentProcess(), 8);
		return ERROR_TIMEOUT;
	}

	DWORD WINAPI MediaWatchdogThreadProc(void* parameter) noexcept
	{
		auto* context = static_cast<MediaWatchdogContext*>(parameter);
		if (!context) return ERROR_INVALID_PARAMETER;
		HANDLE startupWaitHandles[] = {
			context->Completed, context->Started, context->CompletionStarted };
		const DWORD startupWait = WaitForMultipleObjects(
			3,
			startupWaitHandles, FALSE,
			static_cast<DWORD>((StartupTimeoutSeconds
				+ WatchdogStartupGraceSeconds) * 1000.0));
		if (startupWait == WAIT_OBJECT_0) return ERROR_SUCCESS;
		if (startupWait == WAIT_OBJECT_0 + 2)
			return WaitForWatchdogCompletion(*context);
		if (startupWait != WAIT_OBJECT_0 + 1)
		{
			(void)TerminateProcess(GetCurrentProcess(), 8);
			return ERROR_TIMEOUT;
		}

		HANDLE runtimeWaitHandles[] = {
			context->Completed, context->CompletionStarted };
		const DWORD runtimeTimeoutMilliseconds = static_cast<DWORD>(std::ceil(
			(context->DurationSeconds + WatchdogCompletionGraceSeconds) * 1000.0));
		const DWORD runtimeWait = WaitForMultipleObjects(
			2, runtimeWaitHandles, FALSE, runtimeTimeoutMilliseconds);
		if (runtimeWait == WAIT_OBJECT_0) return ERROR_SUCCESS;
		if (runtimeWait == WAIT_OBJECT_0 + 1)
			return WaitForWatchdogCompletion(*context);
		if (runtimeWait != WAIT_OBJECT_0)
		{
			(void)TerminateProcess(GetCurrentProcess(), 8);
			return ERROR_TIMEOUT;
		}
		return ERROR_SUCCESS;
	}

	struct MediaCompletionSnapshot final
	{
		bool Captured = false;
		cui::core::Size VideoSize{};
		MediaElement::PerformanceSnapshot Performance{};
		ProcessSample Process{};
		ProcessSample ProcessAfterClose{};
		double ActualRate = 0.0;
		double PositionSeconds = 0.0;
		double DurationSeconds = 0.0;
		MediaElement::PlaybackState State = MediaElement::PlaybackState::Stopped;
		bool HasVideo = false;
		bool HasAudio = false;
		bool UsingHardwareDecode = false;
		bool UsingNv12Output = false;
		bool UsingDxgiOutput = false;
		HRESULT MediaError = E_UNEXPECTED;
		UINT64 PresentationResourceGeneration = 0;
		UINT64 PresentationDeviceRecoveries = 0;
		UINT64 PresentationCommittedFrames = 0;
		double TimelineAdvanceSeconds = 0.0;
		UINT64 TimelineWrapsObserved = 0;
		UINT64 TimelineBackwardDiscontinuities = 0;
	};

	UINT64 FileTimeValue(const FILETIME& value) noexcept
	{
		ULARGE_INTEGER result{};
		result.LowPart = value.dwLowDateTime;
		result.HighPart = value.dwHighDateTime;
		return result.QuadPart;
	}

	ProcessSample QueryProcessSample() noexcept
	{
		ProcessSample result;
		FILETIME created{}, exited{}, kernel{}, user{};
		if (GetProcessTimes(GetCurrentProcess(),
			&created, &exited, &kernel, &user))
		{
			result.Cpu100Nanoseconds =
				FileTimeValue(kernel) + FileTimeValue(user);
		}
		PROCESS_MEMORY_COUNTERS counters{};
		counters.cb = sizeof(counters);
		if (GetProcessMemoryInfo(GetCurrentProcess(),
			&counters, static_cast<DWORD>(sizeof(counters))))
		{
			result.WorkingSetBytes = counters.WorkingSetSize;
			result.PeakWorkingSetBytes = counters.PeakWorkingSetSize;
		}
		return result;
	}

	bool TryParseFiniteDouble(
		const std::wstring& text, double& value) noexcept
	{
		if (text.empty()) return false;
		wchar_t* end = nullptr;
		errno = 0;
		const double parsed = std::wcstod(text.c_str(), &end);
		if (errno == ERANGE || end == text.c_str() || !end || *end != L'\0'
			|| !std::isfinite(parsed))
			return false;
		value = parsed;
		return true;
	}

	std::string WideToUtf8(const std::wstring& value)
	{
		if (value.empty()) return {};
		const int size = WideCharToMultiByte(CP_UTF8, 0,
			value.data(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		if (size <= 0) return {};
		std::string result(static_cast<size_t>(size), '\0');
		(void)WideCharToMultiByte(CP_UTF8, 0,
			value.data(), static_cast<int>(value.size()),
			result.data(), size, nullptr, nullptr);
		return result;
	}

	std::string JsonEscape(const std::wstring& value)
	{
		const auto utf8 = WideToUtf8(value);
		std::ostringstream output;
		output << '"';
		for (const unsigned char character : utf8)
		{
			switch (character)
			{
			case '"': output << "\\\""; break;
			case '\\': output << "\\\\"; break;
			case '\b': output << "\\b"; break;
			case '\f': output << "\\f"; break;
			case '\n': output << "\\n"; break;
			case '\r': output << "\\r"; break;
			case '\t': output << "\\t"; break;
			default:
				if (character < 0x20)
				{
					output << "\\u" << std::hex << std::setw(4)
						<< std::setfill('0') << static_cast<unsigned>(character)
						<< std::dec << std::setfill(' ');
				}
				else
				{
					output << static_cast<char>(character);
				}
				break;
			}
		}
		output << '"';
		return output.str();
	}

	bool WriteParentConsole(const std::string& text)
	{
		HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!output || output == INVALID_HANDLE_VALUE)
		{
			(void)AttachConsole(ATTACH_PARENT_PROCESS);
			output = GetStdHandle(STD_OUTPUT_HANDLE);
		}
		if (!output || output == INVALID_HANDLE_VALUE) return false;
		DWORD written = 0;
		const std::string line = text + "\r\n";
		return WriteFile(output, line.data(),
			static_cast<DWORD>(line.size()), &written, nullptr)
			&& written == static_cast<DWORD>(line.size());
	}

	const wchar_t* PlaybackStateName(MediaElement::PlaybackState state) noexcept
	{
		switch (state)
		{
		case MediaElement::PlaybackState::Playing: return L"playing";
		case MediaElement::PlaybackState::Paused: return L"paused";
		default: return L"stopped";
		}
	}

	const wchar_t* VideoPathName(MediaPerformanceVideoPath path) noexcept
	{
		switch (path)
		{
		case MediaPerformanceVideoPath::Cpu: return L"cpu";
		case MediaPerformanceVideoPath::GpuRequired: return L"gpu-required";
		default: return L"auto";
		}
	}

	class MediaPerformanceWindow final : public Window
	{
	public:
		explicit MediaPerformanceWindow(
			MediaPerformanceOptions options,
			HANDLE watchdogStartedEvent,
			HANDLE watchdogCompletionStartedEvent)
			: _options(std::move(options)),
			_watchdogStartedEvent(watchdogStartedEvent),
			_watchdogCompletionStartedEvent(watchdogCompletionStartedEvent),
			_createdAt(QueryCounter())
		{
			Title = L"CUI MediaElement deterministic performance run";
			Width = 960.0f;
			Height = 540.0f;
			ShowInTaskbar = false;

			auto player = std::make_unique<MediaElement>();
			_player = player.get();
			_player->LoadedBehavior = MediaState::Manual;
			_player->Loop = true;
			_player->EnableHardwareDecode = true;
			_player->EnableDxgiVideoOutput =
				_options.VideoPath != MediaPerformanceVideoPath::Cpu;
			_player->SpeedRatio = static_cast<float>(_options.Rate);
			SetVisualContent(std::move(player));

			_contentRendered = ContentRendered.Subscribe(
				[this](Window*) { StartMeasurement(); });
			_mediaError = _player->OnMediaError.Subscribe(
				[this](MediaElement*, HRESULT error)
				{
					_lastMediaError = error;
					if (_measurementStarted)
						(void)PostMessageW(Handle,
							CompleteAfterMediaErrorMessage, 0, 0);
				});
			_mediaEnded = _player->OnMediaEnded.Subscribe(
				[this](Control*) { ++_mediaEndedEvents; });
			_positionChanged = _player->OnPositionChanged.Subscribe(
				[this](Control*, double position)
				{
					ObserveTimelinePosition(position);
				});
			_closing = OnClosing.Subscribe(
				[this](Window*, CancelEventArgs& args)
				{
					if (_finishing) return;
					args.Cancel = true;
					if (_closeRequested) return;
					_closeRequested = true;
					_resultCode = 9;
					_resultStatus = L"cancelled";
					_resultError = L"The media performance window was closed before completion.";
					if (!TryPost([this] { Complete(); })) Complete();
				});

			if (!SetTimer(Handle, MeasurementTimerId,
				MeasurementTimerIntervalMs, nullptr))
				throw std::runtime_error(
					"SetTimer failed for the media performance runner");
		}

		~MediaPerformanceWindow() override
		{
			if (Handle) (void)KillTimer(Handle, MeasurementTimerId);
		}

		int ResultCode() const noexcept { return _resultCode; }
		const std::wstring& ResultError() const noexcept { return _resultError; }

	protected:
		std::optional<LRESULT> OnPlatformMessage(
			UINT message, WPARAM wParam, LPARAM lParam) override
		{
			if (message == WM_TIMER && wParam == MeasurementTimerId)
			{
				const auto now = QueryCounter();
				if (!_startAttempted
					&& QuerySeconds(_createdAt, now) >= StartupTimeoutSeconds)
				{
					_resultCode = 8;
					_resultStatus = L"startup_timeout";
					_resultError = L"The media performance window did not render within 15 seconds.";
					Complete();
				}
				else if (_measurementStarted)
				{
					const double elapsed =
						QuerySeconds(_measurementStartedAt, now);
					if (!_presentationDeviceLossInjected
						&& _options.InjectPresentationDeviceLossAtSeconds > 0.0
						&& elapsed
							>= _options.InjectPresentationDeviceLossAtSeconds)
					{
						const auto before = _player
							? _player->GetPerformanceSnapshot()
							: MediaElement::PerformanceSnapshot{};
						_gpuFramesAtPresentationDeviceLoss =
							before.GpuVideoProcessorFrames;
						_submittedFramesAtPresentationDeviceLoss =
							before.SubmittedVideoFrames;
						_presentationGenerationAtDeviceLoss =
							cui::framework::WindowAccess::
								PresentationResourceGeneration(*this);
						_presentationRecoveriesAtDeviceLoss =
							cui::framework::WindowAccess::
								PresentationDeviceRecoveryCount(*this);
						_presentationCommittedFramesAtDeviceLoss =
							cui::framework::WindowAccess::
								PresentationCommittedFrameCount(*this);
						cui::framework::WindowAccess::
							InjectPresentationDeviceLossForTesting(*this);
						_presentationDeviceLossInjected = true;
					}
					if (!_sharedDeviceRotationInjected
						&& _options.InjectSharedDeviceRotationAtSeconds > 0.0
						&& elapsed >= _options.InjectSharedDeviceRotationAtSeconds)
					{
						const auto before = _player
							? _player->GetPerformanceSnapshot()
							: MediaElement::PerformanceSnapshot{};
						_sharedDeviceGenerationAtRotation =
							before.SharedDeviceGeneration;
						_gpuDeviceRebindsAtSharedDeviceRotation =
							before.GpuDeviceRebinds;
						_gpuFramesAtSharedDeviceRotation =
							before.GpuVideoProcessorFrames;
						_submittedFramesAtSharedDeviceRotation =
							before.SubmittedVideoFrames;
						_presentationGenerationAtSharedDeviceRotation =
							cui::framework::WindowAccess::
								PresentationResourceGeneration(*this);
						_presentationRecoveriesAtSharedDeviceRotation =
							cui::framework::WindowAccess::
								PresentationDeviceRecoveryCount(*this);
						_presentationCommittedFramesAtSharedDeviceRotation =
							cui::framework::WindowAccess::
								PresentationCommittedFrameCount(*this);
						if (!cui::framework::WindowAccess::
							InjectSharedGraphicsDeviceRotationForTesting(*this))
						{
							_resultCode = 10;
							_resultStatus =
								L"shared_device_rotation_injection_failed";
							_resultError = L"The shared graphics device rotation "
								L"test seam could not create a replacement device.";
							Complete();
							return LRESULT{ 0 };
						}
						_sharedDeviceRotationInjected = true;
					}
					if (elapsed >= _options.DurationSeconds)
					{
						_resultStatus = L"completed";
						Complete();
					}
				}
				return LRESULT{ 0 };
			}
			if (message == CompleteAfterMediaErrorMessage)
			{
				_resultCode = 6;
				_resultStatus = L"media_error";
				_resultError = L"MediaElement reported a playback error.";
				Complete();
				return LRESULT{ 0 };
			}
			return Window::OnPlatformMessage(message, wParam, lParam);
		}

	private:
		void ObserveTimelinePosition(double position) noexcept
		{
			if (!_timelineMeasurementOpen || !std::isfinite(position)) return;
			position = (std::max)(0.0, position);
			if (!_timelinePositionObserved)
			{
				_lastTimelinePositionSeconds = position;
				_timelinePositionObserved = true;
				return;
			}
			if (position >= _lastTimelinePositionSeconds)
			{
				_timelineAdvanceSeconds +=
					position - _lastTimelinePositionSeconds;
				_lastTimelinePositionSeconds = position;
			}
			else if (_player)
			{
				const double duration = _player->Duration;
				if (duration > 0.0
					&& _mediaEndedEvents > _timelineEndedEventsConsumed
					&& _lastTimelinePositionSeconds
						>= duration - _timelineWrapWindowSeconds
					&& position <= _timelineWrapWindowSeconds)
				{
					_timelineAdvanceSeconds +=
						(std::max)(0.0,
							duration - _lastTimelinePositionSeconds)
						+ position;
					++_timelineWrapsObserved;
					_timelineEndedEventsConsumed = _mediaEndedEvents;
					_lastTimelinePositionSeconds = position;
				}
				else if (_lastTimelinePositionSeconds - position
					> _timelineRegressionToleranceSeconds)
				{
					++_timelineBackwardDiscontinuities;
				}
			}
		}

		double CaptureTimelineAdvance(
			double finalPosition, double duration) const noexcept
		{
			double advance = _timelineAdvanceSeconds;
			if (!_timelinePositionObserved || !std::isfinite(finalPosition))
				return advance;
			finalPosition = (std::max)(0.0, finalPosition);
			if (finalPosition >= _lastTimelinePositionSeconds)
			{
				return advance
					+ finalPosition - _lastTimelinePositionSeconds;
			}
			if (duration > 0.0
				&& _mediaEndedEvents > _timelineEndedEventsConsumed
				&& _lastTimelinePositionSeconds
					>= duration - _timelineWrapWindowSeconds
				&& finalPosition <= _timelineWrapWindowSeconds)
			{
				return advance + (std::max)(0.0,
					duration - _lastTimelinePositionSeconds)
					+ finalPosition;
			}
			return advance;
		}

		void StartMeasurement()
		{
			if (_startAttempted || _finishing) return;
			_startAttempted = true;
			if (!_player)
			{
				_resultCode = 5;
				_resultStatus = L"player_initialization_failed";
				_resultError = L"MediaElement was not created.";
				Complete();
				return;
			}

			if (!_player->Load(_options.MediaPath.wstring()))
			{
				_lastMediaError = _player->GetLastMediaError();
				_resultCode = 5;
				_resultStatus = L"load_failed";
				_resultError = L"MediaElement failed to load the requested media file.";
				Complete();
				return;
			}
			if (_options.VideoPath == MediaPerformanceVideoPath::GpuRequired
				&& !_player->UsingDxgiVideoOutput)
			{
				_resultCode = 10;
				_resultStatus = L"gpu_path_unavailable";
				_resultError = L"MediaElement could not bind the shared DXGI device manager.";
				Complete();
				return;
			}
			if (_options.RequireAudio && !_player->HasAudio)
			{
				_resultCode = 11;
				_resultStatus = L"required_audio_unavailable";
				_resultError = L"The requested A/V performance run did not expose an audio stream.";
				Complete();
				return;
			}

			_player->ResetPerformanceCounters();
			const auto initialPerformance =
				_player->GetPerformanceSnapshot();
			const double sourceFrameSeconds =
				initialPerformance.VideoFrameDurationHns > 0
				? static_cast<double>(initialPerformance.VideoFrameDurationHns)
					/ 10000000.0 : 0.0;
			const double recoveryAtSeconds = (std::max)(
				_options.InjectPresentationDeviceLossAtSeconds,
				_options.InjectSharedDeviceRotationAtSeconds);
			if (recoveryAtSeconds > 0.0)
			{
				const double sourceFramesPerSecond =
					initialPerformance.VideoFrameRateKnown
						&& sourceFrameSeconds > 0.0
					? 1.0 / sourceFrameSeconds : 0.0;
				const double expectedFramesPerSecond =
					sourceFramesPerSecond > 0.0
					? (std::min)(sourceFramesPerSecond * _options.Rate,
						static_cast<double>(initialPerformance.
							VideoPresentationRateLimitHz)) : 0.0;
				const double recoveryFramesAvailable =
					(_options.DurationSeconds - recoveryAtSeconds)
						* expectedFramesPerSecond;
				if (expectedFramesPerSecond <= 0.0
					|| recoveryFramesAvailable
						< static_cast<double>(MinimumPostRecoveryFrames
							+ RecoveryFrameCapacityMargin))
				{
					_resultCode = 4;
					_resultStatus = L"recovery_window_too_short";
					_resultError = L"The requested recovery window cannot contain five post-recovery frames plus scheduling margin.";
					Complete();
					return;
				}
			}
			_measurementStartedAt = QueryCounter();
			_processStarted = QueryProcessSample();
			_timelineAdvanceSeconds = 0.0;
			_lastTimelinePositionSeconds = _player->Position;
			_timelinePositionObserved = true;
			_timelineWrapsObserved = 0;
			_timelineBackwardDiscontinuities = 0;
			_timelineEndedEventsConsumed = _mediaEndedEvents;
			_timelineRegressionToleranceSeconds =
				(std::max)(0.25, sourceFrameSeconds * 2.0);
			_timelineWrapWindowSeconds =
				(std::max)(0.5, sourceFrameSeconds * 3.0);
			_timelineMeasurementOpen = true;
			_presentationGenerationStarted =
				cui::framework::WindowAccess::
					PresentationResourceGeneration(*this);
			_presentationCommittedFramesStarted =
				cui::framework::WindowAccess::
					PresentationCommittedFrameCount(*this);
			_measurementStarted = true;
			if (!_player->TryPlay())
			{
				_resultCode = 6;
				_resultStatus = L"play_rejected";
				_resultError = L"MediaElement rejected TryPlay after a successful load.";
				Complete();
				return;
			}
			if (_watchdogStartedEvent)
				(void)SetEvent(_watchdogStartedEvent);
		}

		std::string BuildJson() const
		{
			const auto completedAt = _completedAt.QuadPart != 0
				? _completedAt : QueryCounter();
			const double elapsed = _measurementStarted
				? QuerySeconds(_measurementStartedAt, completedAt) : 0.0;
			const auto processCompleted = _completionSnapshot.Captured
				? _completionSnapshot.Process : QueryProcessSample();
			const UINT64 processCpuTicks = _measurementStarted
				&& processCompleted.Cpu100Nanoseconds
					>= _processStarted.Cpu100Nanoseconds
				? processCompleted.Cpu100Nanoseconds
					- _processStarted.Cpu100Nanoseconds
				: 0;
			const double processCpuSeconds =
				static_cast<double>(processCpuTicks) / 10000000.0;
			const double processCpuCoreEquivalents = elapsed > 0.0
				? processCpuSeconds / elapsed : 0.0;
			SYSTEM_INFO systemInfo{};
			GetSystemInfo(&systemInfo);
			const double processCpuPercent = systemInfo.dwNumberOfProcessors > 0
				? processCpuCoreEquivalents * 100.0
					/ static_cast<double>(systemInfo.dwNumberOfProcessors)
				: 0.0;
			const auto videoSize = _completionSnapshot.Captured
				? _completionSnapshot.VideoSize
				: (_player ? _player->VideoSize : cui::core::Size{});
			const auto performance = _completionSnapshot.Captured
				? _completionSnapshot.Performance
				: (_player ? _player->GetPerformanceSnapshot()
					: MediaElement::PerformanceSnapshot{});
			const double actualRate = _completionSnapshot.Captured
				? _completionSnapshot.ActualRate
				: (_player ? _player->SpeedRatio : 0.0);
			const double positionSeconds = _completionSnapshot.Captured
				? _completionSnapshot.PositionSeconds
				: (_player ? _player->Position : 0.0);
			const double mediaDurationSeconds = _completionSnapshot.Captured
				? _completionSnapshot.DurationSeconds
				: (_player ? _player->Duration : 0.0);
			const auto playState = _completionSnapshot.Captured
				? _completionSnapshot.State
				: (_player ? _player->State : MediaElement::PlaybackState::Stopped);
			const bool hasVideo = _completionSnapshot.Captured
				? _completionSnapshot.HasVideo : (_player && _player->HasVideo);
			const bool hasAudio = _completionSnapshot.Captured
				? _completionSnapshot.HasAudio : (_player && _player->HasAudio);
			const bool usingHardwareDecode = _completionSnapshot.Captured
				? _completionSnapshot.UsingHardwareDecode
				: (_player && _player->UsingHardwareDecode);
			const bool usingNv12Output = _completionSnapshot.Captured
				? _completionSnapshot.UsingNv12Output
				: (_player && _player->UsingNv12VideoOutput);
			const bool usingDxgiOutput = _completionSnapshot.Captured
				? _completionSnapshot.UsingDxgiOutput
				: (_player && _player->UsingDxgiVideoOutput);
			const auto qpcSeconds = [&performance](UINT64 ticks)
			{
				return performance.QpcFrequency > 0
					? static_cast<double>(ticks)
						/ static_cast<double>(performance.QpcFrequency)
					: 0.0;
			};
			const auto averageMilliseconds = [&qpcSeconds](
				UINT64 ticks, UINT64 calls)
			{
				return calls > 0
					? qpcSeconds(ticks) * 1000.0
						/ static_cast<double>(calls)
					: 0.0;
			};
			const double performanceMeasurementSeconds =
				qpcSeconds(performance.MeasurementQpcTicks);
			const HRESULT mediaError = _completionSnapshot.Captured
				? _completionSnapshot.MediaError
				: (FAILED(_lastMediaError) ? _lastMediaError
					: (_player ? _player->GetLastMediaError() : E_UNEXPECTED));
			const double submittedFramesPerSecond =
				performanceMeasurementSeconds > 0.0
				? static_cast<double>(performance.SubmittedVideoFrames)
					/ performanceMeasurementSeconds : 0.0;
			const double sourceFramesPerSecond =
				performance.VideoFrameDurationHns > 0
				? 10000000.0
					/ static_cast<double>(performance.VideoFrameDurationHns)
				: 0.0;
			const double expectedSubmittedFramesPerSecond =
				sourceFramesPerSecond > 0.0
				? (std::min)(
					sourceFramesPerSecond * _options.Rate,
					static_cast<double>(
						performance.VideoPresentationRateLimitHz))
				: 0.0;
			const double mediaTimelineAdvanceSeconds =
				_completionSnapshot.TimelineAdvanceSeconds;
			const double expectedTimelineAdvanceSeconds =
				performanceMeasurementSeconds * _options.Rate;
			const double timelineAdvanceRatio =
				expectedTimelineAdvanceSeconds > 0.0
				? mediaTimelineAdvanceSeconds
					/ expectedTimelineAdvanceSeconds : 0.0;
			const double timelineToleranceSeconds = (std::max)(
				expectedTimelineAdvanceSeconds * 0.10,
				(std::max)(0.05,
					static_cast<double>(performance.VideoFrameDurationHns)
						* 2.0 / 10000000.0));
			const double unintentionalFrameLossRatio =
				performance.DecodedVideoFrames > 0
				? static_cast<double>(performance.DroppedLateVideoFrames
					+ performance.OverwrittenVideoFrames)
					/ static_cast<double>(performance.DecodedVideoFrames)
				: 0.0;
			const UINT64 presentationCommittedFrames =
				_completionSnapshot.PresentationCommittedFrames
					>= _presentationCommittedFramesStarted
				? _completionSnapshot.PresentationCommittedFrames
					- _presentationCommittedFramesStarted : 0;
			const double presentationCommittedFramesPerSecond =
				performanceMeasurementSeconds > 0.0
				? static_cast<double>(presentationCommittedFrames)
					/ performanceMeasurementSeconds : 0.0;
#if defined(_DEBUG)
			constexpr const wchar_t* buildConfiguration = L"Debug";
#else
			constexpr const wchar_t* buildConfiguration = L"Release";
#endif
#if defined(_M_X64)
			constexpr const wchar_t* buildArchitecture = L"x64";
#elif defined(_M_ARM64)
			constexpr const wchar_t* buildArchitecture = L"arm64";
#else
			constexpr const wchar_t* buildArchitecture = L"unknown";
#endif

			std::ostringstream output;
			output.imbue(std::locale::classic());
			output << std::boolalpha << std::fixed << std::setprecision(6);
			output << "{\n"
				<< "  \"schema\": \"cui.media-performance.v3\",\n"
				<< "  \"build_configuration\": "
				<< JsonEscape(buildConfiguration) << ",\n"
				<< "  \"build_architecture\": "
				<< JsonEscape(buildArchitecture) << ",\n"
				<< "  \"status\": " << JsonEscape(_resultStatus) << ",\n"
				<< "  \"error\": " << JsonEscape(_resultError) << ",\n"
				<< "  \"media_path\": "
				<< JsonEscape(_options.MediaPath.wstring()) << ",\n"
				<< "  \"requested_rate\": " << _options.Rate << ",\n"
				<< "  \"actual_rate\": " << actualRate << ",\n"
				<< "  \"requested_duration_seconds\": "
				<< _options.DurationSeconds << ",\n"
				<< "  \"inject_presentation_device_loss_at_seconds\": "
				<< _options.InjectPresentationDeviceLossAtSeconds << ",\n"
				<< "  \"inject_shared_device_rotation_at_seconds\": "
				<< _options.InjectSharedDeviceRotationAtSeconds << ",\n"
				<< "  \"requested_video_path\": "
				<< JsonEscape(VideoPathName(_options.VideoPath)) << ",\n"
				<< "  \"require_audio\": " << _options.RequireAudio << ",\n"
				<< "  \"expected_video_width\": "
				<< _options.ExpectedVideoWidth << ",\n"
				<< "  \"expected_video_height\": "
				<< _options.ExpectedVideoHeight << ",\n"
				<< "  \"expected_video_frames_per_second\": "
				<< _options.ExpectedVideoFramesPerSecond << ",\n"
				<< "  \"elapsed_seconds\": " << elapsed << ",\n"
				<< "  \"position_seconds\": " << positionSeconds << ",\n"
				<< "  \"media_duration_seconds\": "
				<< mediaDurationSeconds << ",\n"
				<< "  \"play_state\": "
				<< JsonEscape(_player ? PlaybackStateName(playState) : L"unavailable")
				<< ",\n"
				<< "  \"has_video\": " << hasVideo << ",\n"
				<< "  \"has_audio\": " << hasAudio << ",\n"
				<< "  \"video_width\": " << videoSize.width << ",\n"
				<< "  \"video_height\": " << videoSize.height << ",\n"
				<< "  \"using_hardware_decode_strategy\": "
				<< usingHardwareDecode << ",\n"
				<< "  \"using_nv12_output\": "
				<< usingNv12Output << ",\n"
				<< "  \"using_dxgi_device_manager\": "
				<< usingDxgiOutput << ",\n"
				<< "  \"media_ended_events\": " << _mediaEndedEvents << ",\n"
				<< "  \"timeline_wraps_observed\": "
				<< _completionSnapshot.TimelineWrapsObserved << ",\n"
				<< "  \"timeline_backward_discontinuities\": "
				<< _completionSnapshot.TimelineBackwardDiscontinuities << ",\n"
				<< "  \"media_timeline_advance_seconds\": "
				<< mediaTimelineAdvanceSeconds << ",\n"
				<< "  \"expected_timeline_advance_seconds\": "
				<< expectedTimelineAdvanceSeconds << ",\n"
				<< "  \"timeline_advance_ratio\": "
				<< timelineAdvanceRatio << ",\n"
				<< "  \"timeline_tolerance_seconds\": "
				<< timelineToleranceSeconds << ",\n"
				<< "  \"presentation_device_loss_injected\": "
				<< _presentationDeviceLossInjected << ",\n"
				<< "  \"presentation_generation_measurement_started\": "
				<< _presentationGenerationStarted << ",\n"
				<< "  \"presentation_generation_at_device_loss_injection\": "
				<< _presentationGenerationAtDeviceLoss << ",\n"
				<< "  \"presentation_generation_completed\": "
				<< _completionSnapshot.PresentationResourceGeneration << ",\n"
				<< "  \"presentation_device_recoveries_at_device_loss_injection\": "
				<< _presentationRecoveriesAtDeviceLoss << ",\n"
				<< "  \"presentation_device_recoveries_completed\": "
				<< _completionSnapshot.PresentationDeviceRecoveries << ",\n"
				<< "  \"presentation_committed_frames_measurement_started\": "
				<< _presentationCommittedFramesStarted << ",\n"
				<< "  \"presentation_committed_frames_completed\": "
				<< _completionSnapshot.PresentationCommittedFrames << ",\n"
				<< "  \"presentation_committed_frames\": "
				<< presentationCommittedFrames << ",\n"
				<< "  \"presentation_committed_frames_per_second\": "
				<< presentationCommittedFramesPerSecond << ",\n"
				<< "  \"gpu_frames_after_presentation_device_loss\": "
				<< (_presentationDeviceLossInjected
					&& performance.GpuVideoProcessorFrames
						>= _gpuFramesAtPresentationDeviceLoss
					? performance.GpuVideoProcessorFrames
						- _gpuFramesAtPresentationDeviceLoss : 0)
				<< ",\n"
				<< "  \"submitted_frames_after_presentation_device_loss\": "
				<< (_presentationDeviceLossInjected
					&& performance.SubmittedVideoFrames
						>= _submittedFramesAtPresentationDeviceLoss
					? performance.SubmittedVideoFrames
						- _submittedFramesAtPresentationDeviceLoss : 0)
				<< ",\n"
				<< "  \"committed_frames_after_presentation_device_loss\": "
				<< (_presentationDeviceLossInjected
					&& _completionSnapshot.PresentationCommittedFrames
						>= _presentationCommittedFramesAtDeviceLoss
					? _completionSnapshot.PresentationCommittedFrames
						- _presentationCommittedFramesAtDeviceLoss : 0)
				<< ",\n"
				<< "  \"shared_device_rotation_injected\": "
				<< _sharedDeviceRotationInjected << ",\n"
				<< "  \"shared_device_generation_at_shared_device_rotation_injection\": "
				<< _sharedDeviceGenerationAtRotation << ",\n"
				<< "  \"shared_device_generation_completed\": "
				<< performance.SharedDeviceGeneration << ",\n"
				<< "  \"gpu_device_rebinds_at_shared_device_rotation_injection\": "
				<< _gpuDeviceRebindsAtSharedDeviceRotation << ",\n"
				<< "  \"gpu_device_rebinds_completed\": "
				<< performance.GpuDeviceRebinds << ",\n"
				<< "  \"presentation_generation_at_shared_device_rotation_injection\": "
				<< _presentationGenerationAtSharedDeviceRotation << ",\n"
				<< "  \"presentation_device_recoveries_at_shared_device_rotation_injection\": "
				<< _presentationRecoveriesAtSharedDeviceRotation << ",\n"
				<< "  \"gpu_frames_after_shared_device_rotation\": "
				<< (_sharedDeviceRotationInjected
					&& performance.GpuVideoProcessorFrames
						>= _gpuFramesAtSharedDeviceRotation
					? performance.GpuVideoProcessorFrames
						- _gpuFramesAtSharedDeviceRotation : 0)
				<< ",\n"
				<< "  \"submitted_frames_after_shared_device_rotation\": "
				<< (_sharedDeviceRotationInjected
					&& performance.SubmittedVideoFrames
						>= _submittedFramesAtSharedDeviceRotation
					? performance.SubmittedVideoFrames
						- _submittedFramesAtSharedDeviceRotation : 0)
				<< ",\n"
				<< "  \"committed_frames_after_shared_device_rotation\": "
				<< (_sharedDeviceRotationInjected
					&& _completionSnapshot.PresentationCommittedFrames
						>= _presentationCommittedFramesAtSharedDeviceRotation
					? _completionSnapshot.PresentationCommittedFrames
						- _presentationCommittedFramesAtSharedDeviceRotation : 0)
				<< ",\n"
				<< "  \"process_cpu_seconds\": " << processCpuSeconds << ",\n"
				<< "  \"process_cpu_core_equivalents\": "
				<< processCpuCoreEquivalents << ",\n"
				<< "  \"process_cpu_percent_of_machine\": "
				<< processCpuPercent << ",\n"
				<< "  \"working_set_bytes\": "
				<< processCompleted.WorkingSetBytes << ",\n"
				<< "  \"working_set_after_close_bytes\": "
				<< (_completionSnapshot.Captured
					? _completionSnapshot.ProcessAfterClose.WorkingSetBytes
					: processCompleted.WorkingSetBytes) << ",\n"
				<< "  \"peak_working_set_bytes\": "
				<< processCompleted.PeakWorkingSetBytes << ",\n"
				<< "  \"media_hresult\": \"0x" << std::uppercase << std::hex
				<< std::setw(8) << std::setfill('0')
				<< static_cast<unsigned long>(mediaError)
				<< std::nouppercase << std::dec << "\",\n"
				<< "  \"performance\": {\n"
				<< "    \"qpc_frequency\": "
				<< performance.QpcFrequency << ",\n"
				<< "    \"measurement_seconds\": "
				<< performanceMeasurementSeconds << ",\n"
				<< "    \"video_presentation_rate_limit_hz\": "
				<< performance.VideoPresentationRateLimitHz << ",\n"
				<< "    \"video_frame_duration_hns\": "
				<< performance.VideoFrameDurationHns << ",\n"
				<< "    \"video_frame_rate_known\": "
				<< performance.VideoFrameRateKnown << ",\n"
				<< "    \"source_frames_per_second\": "
				<< sourceFramesPerSecond << ",\n"
				<< "    \"expected_submitted_frames_per_second\": "
				<< expectedSubmittedFramesPerSecond << ",\n"
				<< "    \"read_sample_calls\": "
				<< performance.ReadSampleCalls << ",\n"
				<< "    \"read_sample_seconds\": "
				<< qpcSeconds(performance.ReadSampleQpcTicks) << ",\n"
				<< "    \"read_sample_average_ms\": "
				<< averageMilliseconds(performance.ReadSampleQpcTicks,
					performance.ReadSampleCalls) << ",\n"
				<< "    \"contiguous_buffer_calls\": "
				<< performance.SamplesToContiguousBufferCalls << ",\n"
				<< "    \"contiguous_buffer_seconds\": "
				<< qpcSeconds(
					performance.SamplesToContiguousBufferQpcTicks) << ",\n"
				<< "    \"dxgi_video_samples\": "
				<< performance.DxgiVideoSamples << ",\n"
				<< "    \"gpu_video_processor_frames\": "
				<< performance.GpuVideoProcessorFrames << ",\n"
				<< "    \"gpu_surface_import_failures\": "
				<< performance.GpuSurfaceImportFailures << ",\n"
				<< "    \"cpu_fallback_video_frames\": "
				<< performance.CpuFallbackVideoFrames << ",\n"
				<< "    \"gpu_device_rebinds\": "
				<< performance.GpuDeviceRebinds << ",\n"
				<< "    \"stale_generation_frames\": "
				<< performance.StaleGenerationFrames << ",\n"
				<< "    \"shared_device_generation\": "
				<< performance.SharedDeviceGeneration << ",\n"
				<< "    \"adapter_luid\": "
				<< performance.AdapterLuid << ",\n"
				<< "    \"dxgi_device_manager_active\": "
				<< performance.DxgiDeviceManagerActive << ",\n"
				<< "    \"decoded_video_frames\": "
				<< performance.DecodedVideoFrames << ",\n"
				<< "    \"converted_video_frames\": "
				<< performance.ConvertedVideoFrames << ",\n"
				<< "    \"submitted_video_frames\": "
				<< performance.SubmittedVideoFrames << ",\n"
				<< "    \"submitted_frames_per_second\": "
				<< submittedFramesPerSecond << ",\n"
				<< "    \"dropped_late_video_frames\": "
				<< performance.DroppedLateVideoFrames << ",\n"
				<< "    \"thinned_video_frames\": "
				<< performance.ThinnedVideoFrames << ",\n"
				<< "    \"overwritten_video_frames\": "
				<< performance.OverwrittenVideoFrames << ",\n"
				<< "    \"unintentional_frame_loss_ratio\": "
				<< unintentionalFrameLossRatio << ",\n"
				<< "    \"maximum_video_lateness_ms\": "
				<< qpcSeconds(performance.MaximumVideoLatenessQpcTicks)
					* 1000.0 << ",\n"
				<< "    \"submitted_frame_interval_samples\": "
				<< performance.SubmittedFrameIntervalSamples << ",\n"
				<< "    \"submitted_frame_interval_p95_ms\": "
				<< performance.SubmittedFrameIntervalP95Ms << ",\n"
				<< "    \"submitted_frame_interval_p99_ms\": "
				<< performance.SubmittedFrameIntervalP99Ms << ",\n"
				<< "    \"video_convert_seconds\": "
				<< qpcSeconds(performance.VideoConvertQpcTicks) << ",\n"
				<< "    \"video_convert_average_ms\": "
				<< averageMilliseconds(performance.VideoConvertQpcTicks,
					performance.ConvertedVideoFrames) << ",\n"
				<< "    \"video_convert_bytes\": "
				<< performance.VideoConvertBytes << ",\n"
				<< "    \"visual_invalidation_requests\": "
				<< performance.VisualInvalidationRequests << ",\n"
				<< "    \"coalesced_visual_invalidations\": "
				<< performance.CoalescedVisualInvalidations << ",\n"
				<< "    \"audio_write_calls\": "
				<< performance.AudioWriteCalls << ",\n"
				<< "    \"audio_write_seconds\": "
				<< qpcSeconds(performance.AudioWriteQpcTicks) << ",\n"
				<< "    \"audio_write_bytes\": "
				<< performance.AudioWriteBytes << ",\n"
				<< "    \"companion_session_started_events\": "
				<< performance.CompanionSessionStartedEvents << ",\n"
				<< "    \"render_updates\": "
				<< performance.RenderUpdates << ",\n"
				<< "    \"video_upload_calls\": "
				<< performance.VideoUploadCalls << ",\n"
				<< "    \"video_upload_seconds\": "
				<< qpcSeconds(performance.VideoUploadQpcTicks) << ",\n"
				<< "    \"video_upload_average_ms\": "
				<< averageMilliseconds(performance.VideoUploadQpcTicks,
					performance.VideoUploadCalls) << ",\n"
				<< "    \"video_upload_bytes\": "
				<< performance.VideoUploadBytes << ",\n"
				<< "    \"draw_bitmap_calls\": "
				<< performance.DrawBitmapCalls << ",\n"
				<< "    \"draw_bitmap_seconds\": "
				<< qpcSeconds(performance.DrawBitmapQpcTicks) << ",\n"
				<< "    \"draw_bitmap_average_ms\": "
				<< averageMilliseconds(performance.DrawBitmapQpcTicks,
					performance.DrawBitmapCalls) << "\n"
				<< "  }\n"
				<< "}";
			return output.str();
		}

		void CaptureCompletionSnapshot()
		{
			if (_completionSnapshot.Captured) return;
			// Freeze the measurement boundary before Close joins the playback
			// worker and releases large frame buffers.  The copied snapshot stays
			// stable while teardown proceeds, without charging teardown time to FPS.
			if (_player)
			{
				// Preserve the public intent before the measurement pause changes it,
				// then take counters and position from the same quiesced boundary.
				_completionSnapshot.State = _player->State;
				_completionSnapshot.Performance = _measurementStarted
					? _player->PauseAndGetPerformanceSnapshot()
					: _player->GetPerformanceSnapshot();
				_completionSnapshot.VideoSize = _player->VideoSize;
				_completionSnapshot.ActualRate = _player->SpeedRatio;
				_completionSnapshot.PositionSeconds = _player->Position;
				_completionSnapshot.DurationSeconds = _player->Duration;
				_completionSnapshot.HasVideo = _player->HasVideo;
				_completionSnapshot.HasAudio = _player->HasAudio;
				_completionSnapshot.UsingHardwareDecode =
					_player->UsingHardwareDecode;
				_completionSnapshot.UsingNv12Output =
					_player->UsingNv12VideoOutput;
				_completionSnapshot.UsingDxgiOutput =
					_player->UsingDxgiVideoOutput;
				// Pause may itself reveal an audio/session failure; read the backend
				// error only after the quiesced snapshot has completed.
				_completionSnapshot.MediaError = FAILED(_lastMediaError)
					? _lastMediaError : _player->GetLastMediaError();
				_completionSnapshot.PresentationResourceGeneration =
					cui::framework::WindowAccess::
						PresentationResourceGeneration(*this);
				_completionSnapshot.PresentationDeviceRecoveries =
					cui::framework::WindowAccess::
						PresentationDeviceRecoveryCount(*this);
				_completionSnapshot.PresentationCommittedFrames =
					cui::framework::WindowAccess::
						PresentationCommittedFrameCount(*this);
				_completionSnapshot.TimelineAdvanceSeconds =
					CaptureTimelineAdvance(
						_completionSnapshot.PositionSeconds,
						_completionSnapshot.DurationSeconds);
				_completionSnapshot.TimelineWrapsObserved =
					_timelineWrapsObserved;
				_completionSnapshot.TimelineBackwardDiscontinuities =
					_timelineBackwardDiscontinuities;
				_timelineMeasurementOpen = false;
				_completedAt = QueryCounter();
				_completionSnapshot.Process = QueryProcessSample();
				_player->Close();
			}
			else
			{
				_completedAt = QueryCounter();
				_completionSnapshot.Process = QueryProcessSample();
			}
			_completionSnapshot.ProcessAfterClose = QueryProcessSample();
			_completionSnapshot.Captured = true;
		}

		bool WriteJsonFile(const std::string& json)
		{
			if (_options.PerfJsonPath.empty()) return false;
			std::error_code pathError;
			const bool outputExists =
				std::filesystem::exists(_options.PerfJsonPath, pathError);
			if (pathError) return false;
			if (outputExists)
			{
				const bool aliasesInput = std::filesystem::equivalent(
					_options.MediaPath, _options.PerfJsonPath, pathError);
				if (pathError || aliasesInput) return false;
			}

			std::filesystem::path temporaryPath;
			HANDLE temporaryFile = INVALID_HANDLE_VALUE;
			const auto uniqueSeed = GetTickCount64();
			for (unsigned attempt = 0; attempt < 32; ++attempt)
			{
				temporaryPath = _options.PerfJsonPath;
				temporaryPath += L".tmp-"
					+ std::to_wstring(GetCurrentProcessId()) + L"-"
					+ std::to_wstring(uniqueSeed) + L"-"
					+ std::to_wstring(attempt);
				temporaryFile = CreateFileW(
					temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
					CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
				if (temporaryFile != INVALID_HANDLE_VALUE) break;
				const DWORD createError = GetLastError();
				if (createError != ERROR_FILE_EXISTS
					&& createError != ERROR_ALREADY_EXISTS)
					return false;
			}
			if (temporaryFile == INVALID_HANDLE_VALUE) return false;

			bool writeSucceeded = false;
			{
				UniqueHandle output(temporaryFile);
				const std::string payload = json + "\n";
				if (payload.size() <= (std::numeric_limits<DWORD>::max)())
				{
					DWORD written = 0;
					writeSucceeded = WriteFile(output.Get(), payload.data(),
						static_cast<DWORD>(payload.size()), &written, nullptr)
						&& written == static_cast<DWORD>(payload.size())
						&& FlushFileBuffers(output.Get());
				}
			}
			if (!writeSucceeded)
			{
				(void)std::filesystem::remove(temporaryPath, pathError);
				return false;
			}
			if (!MoveFileExW(
				temporaryPath.c_str(), _options.PerfJsonPath.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				(void)std::filesystem::remove(temporaryPath, pathError);
				return false;
			}
			return true;
		}

		void Complete()
		{
			if (_finishing) return;
			_finishing = true;
			if (_watchdogCompletionStartedEvent)
				(void)SetEvent(_watchdogCompletionStartedEvent);
			(void)KillTimer(Handle, MeasurementTimerId);
			CaptureCompletionSnapshot();
			const auto& performance = _completionSnapshot.Performance;
			const bool exercisedGpuPath =
				performance.DxgiVideoSamples > 0
				&& performance.GpuVideoProcessorFrames > 0;
			const bool requireGpuRecovery =
				_options.VideoPath
					== MediaPerformanceVideoPath::GpuRequired
				|| exercisedGpuPath;
			const bool presentationDeviceRecoveryFailed =
				_options.InjectPresentationDeviceLossAtSeconds > 0.0
				&& (!_presentationDeviceLossInjected
					|| _completionSnapshot.PresentationResourceGeneration
						<= _presentationGenerationAtDeviceLoss
					|| _completionSnapshot.PresentationDeviceRecoveries
						<= _presentationRecoveriesAtDeviceLoss
					|| performance.SubmittedVideoFrames
						< _submittedFramesAtPresentationDeviceLoss
							+ MinimumPostRecoveryFrames
					|| _completionSnapshot.PresentationCommittedFrames
						< _presentationCommittedFramesAtDeviceLoss
							+ MinimumPostRecoveryFrames
					|| (requireGpuRecovery
						&& performance.GpuVideoProcessorFrames
							< _gpuFramesAtPresentationDeviceLoss
								+ MinimumPostRecoveryFrames));
			const bool sharedDeviceRotationRecoveryFailed =
				_options.InjectSharedDeviceRotationAtSeconds > 0.0
				&& (!_sharedDeviceRotationInjected
					|| _completionSnapshot.PresentationResourceGeneration
						<= _presentationGenerationAtSharedDeviceRotation
					|| _completionSnapshot.PresentationDeviceRecoveries
						<= _presentationRecoveriesAtSharedDeviceRotation
					|| performance.SubmittedVideoFrames
						< _submittedFramesAtSharedDeviceRotation
							+ MinimumPostRecoveryFrames
					|| _completionSnapshot.PresentationCommittedFrames
						< _presentationCommittedFramesAtSharedDeviceRotation
							+ MinimumPostRecoveryFrames
					|| (requireGpuRecovery
						&& (performance.SharedDeviceGeneration
							<= _sharedDeviceGenerationAtRotation
							|| performance.GpuDeviceRebinds
								<= _gpuDeviceRebindsAtSharedDeviceRotation
							|| performance.GpuVideoProcessorFrames
								< _gpuFramesAtSharedDeviceRotation
									+ MinimumPostRecoveryFrames)));
			const double measurementSeconds =
				performance.QpcFrequency > 0
				? static_cast<double>(performance.MeasurementQpcTicks)
					/ static_cast<double>(performance.QpcFrequency) : 0.0;
			const double sourceFramesPerSecond =
				performance.VideoFrameDurationHns > 0
				? 10000000.0
					/ static_cast<double>(performance.VideoFrameDurationHns)
				: 0.0;
			const double expectedFramesPerSecond = sourceFramesPerSecond > 0.0
				? (std::min)(sourceFramesPerSecond * _options.Rate,
					static_cast<double>(
						performance.VideoPresentationRateLimitHz)) : 0.0;
			const double submittedFramesPerSecond = measurementSeconds > 0.0
				? static_cast<double>(performance.SubmittedVideoFrames)
					/ measurementSeconds : 0.0;
			const double expectedTimelineAdvance =
				measurementSeconds * _options.Rate;
			const double timelineAdvance =
				_completionSnapshot.TimelineAdvanceSeconds;
			const double timelineRatio = expectedTimelineAdvance > 0.0
				? timelineAdvance / expectedTimelineAdvance : 0.0;
			const double timelineToleranceSeconds = (std::max)(
				expectedTimelineAdvance * 0.10,
				(std::max)(0.05,
					static_cast<double>(performance.VideoFrameDurationHns)
						* 2.0 / 10000000.0));
			const double lostFrameRatio = performance.DecodedVideoFrames > 0
				? static_cast<double>(performance.DroppedLateVideoFrames
					+ performance.OverwrittenVideoFrames)
					/ static_cast<double>(performance.DecodedVideoFrames) : 0.0;
			const UINT64 presentationCommittedFrames =
				_completionSnapshot.PresentationCommittedFrames
					>= _presentationCommittedFramesStarted
				? _completionSnapshot.PresentationCommittedFrames
					- _presentationCommittedFramesStarted : 0;
			const double presentationFramesPerSecond =
				measurementSeconds > 0.0
				? static_cast<double>(presentationCommittedFrames)
					/ measurementSeconds : 0.0;
			const UINT64 processTicks =
				_completionSnapshot.Process.Cpu100Nanoseconds
					>= _processStarted.Cpu100Nanoseconds
				? _completionSnapshot.Process.Cpu100Nanoseconds
					- _processStarted.Cpu100Nanoseconds : 0;
			const double cpuCoreEquivalents = measurementSeconds > 0.0
				? (static_cast<double>(processTicks) / 10000000.0)
					/ measurementSeconds : 0.0;
			const double expectedFrameIntervalMs =
				expectedFramesPerSecond > 0.0
				? 1000.0 / expectedFramesPerSecond : 0.0;
			const double p99LimitMs = (std::max)(
				50.0, expectedFrameIntervalMs * 4.0);
			const bool audioPathExercised =
				(performance.AudioWriteCalls > 0
					&& performance.AudioWriteBytes > 0)
				|| performance.CompanionSessionStartedEvents > 0;
			const bool requiredAudioFailed = _options.RequireAudio
				&& (!_completionSnapshot.HasAudio || !audioPathExercised);
			const double expectedFpsTolerance =
				(std::max)(0.5,
					_options.ExpectedVideoFramesPerSecond * 0.02);
			const bool inputContractFailed =
				((_options.ExpectedVideoWidth != 0
					|| _options.ExpectedVideoHeight != 0
					|| _options.ExpectedVideoFramesPerSecond > 0.0)
					&& !_completionSnapshot.HasVideo)
				|| (_options.ExpectedVideoWidth != 0
					&& std::fabs(_completionSnapshot.VideoSize.width
						- static_cast<float>(_options.ExpectedVideoWidth)) > 0.5f)
				|| (_options.ExpectedVideoHeight != 0
					&& std::fabs(_completionSnapshot.VideoSize.height
						- static_cast<float>(_options.ExpectedVideoHeight)) > 0.5f)
				|| (_options.ExpectedVideoFramesPerSecond > 0.0
					&& (!performance.VideoFrameRateKnown
						|| sourceFramesPerSecond <= 0.0
						|| std::fabs(sourceFramesPerSecond
							- _options.ExpectedVideoFramesPerSecond)
							> expectedFpsTolerance));
			const bool gpuPathFailed =
				_options.VideoPath == MediaPerformanceVideoPath::GpuRequired
				&& (!_completionSnapshot.UsingDxgiOutput
					|| !performance.DxgiDeviceManagerActive
					|| performance.DxgiVideoSamples == 0
					|| performance.GpuVideoProcessorFrames == 0
					|| performance.CpuFallbackVideoFrames != 0
					|| performance.GpuSurfaceImportFailures != 0);
			const bool throughputFailed = _completionSnapshot.HasVideo
				&& measurementSeconds >= 2.0
				&& (expectedFramesPerSecond <= 0.0
					|| submittedFramesPerSecond
						< expectedFramesPerSecond * 0.90
					|| presentationFramesPerSecond
						< expectedFramesPerSecond * 0.90);
			const bool cadenceFailed = _completionSnapshot.HasVideo
				&& performance.SubmittedFrameIntervalSamples >= 30
				&& performance.SubmittedFrameIntervalP99Ms > p99LimitMs;
			const bool playbackProgressFailed = measurementSeconds >= 1.0
				&& (std::fabs(timelineAdvance - expectedTimelineAdvance)
						> timelineToleranceSeconds
					|| std::fabs(_completionSnapshot.ActualRate
						- _options.Rate) > 0.01
					|| _completionSnapshot.State
						!= MediaElement::PlaybackState::Playing);
			const bool qualityFailed = _completionSnapshot.HasVideo
				&& (lostFrameRatio > 0.05
					|| _completionSnapshot.
						TimelineBackwardDiscontinuities != 0
					|| (performance.QpcFrequency > 0
						&& static_cast<double>(
							performance.MaximumVideoLatenessQpcTicks)
							* 1000.0 / performance.QpcFrequency > 25.0));
			const bool gpuCpuBudgetFailed =
				_options.VideoPath == MediaPerformanceVideoPath::GpuRequired
				&& cpuCoreEquivalents > 2.0;
			if (_resultCode == 0 && requiredAudioFailed)
			{
				_resultCode = 11;
				_resultStatus = L"required_audio_path_not_exercised";
				_resultError = L"The required audio stream produced neither WASAPI PCM writes nor an accepted companion-session Start.";
			}
			else if (_resultCode == 0 && inputContractFailed)
			{
				_resultCode = 13;
				_resultStatus = L"media_contract_mismatch";
				_resultError = L"The loaded media did not match the required video width, height, or frame rate.";
			}
			else if (_resultCode == 0
				&& FAILED(_completionSnapshot.MediaError))
			{
				_resultCode = 10;
				_resultStatus = L"media_error_observed";
				_resultError = L"The media backend reported a failed HRESULT during the measurement.";
			}
			else if (_resultCode == 0
				&& (presentationDeviceRecoveryFailed
					|| sharedDeviceRotationRecoveryFailed))
			{
				_resultCode = 10;
				if (sharedDeviceRotationRecoveryFailed)
				{
					_resultStatus =
						L"shared_device_rotation_recovery_not_realized";
					_resultError = L"The shared graphics device rotated, but "
						L"the GPU and presentation paths did not fully recover.";
				}
				else if (presentationDeviceRecoveryFailed)
				{
					_resultStatus =
						L"presentation_device_recovery_not_realized";
					_resultError = L"Presentation recovery did not sustain at least five post-recovery frames.";
				}
				else
				{
					_resultStatus = L"recovery_not_realized";
					_resultError = L"The requested recovery path was not sustained.";
				}
			}
			else if (_resultCode == 0 && gpuPathFailed)
			{
				_resultCode = 10;
				_resultStatus = L"gpu_path_not_realized";
				_resultError = L"The required GPU surface path was not sustained for the measurement.";
			}
			else if (_resultCode == 0 && (throughputFailed || cadenceFailed
				|| playbackProgressFailed || qualityFailed
				|| gpuCpuBudgetFailed))
			{
				_resultCode = 12;
				_resultStatus = L"performance_gate_failed";
				_resultError = L"Playback failed the timeline, throughput, cadence, frame-loss, lateness, or GPU CPU-budget gate.";
			}
			auto json = BuildJson();
			if (_options.PerfJsonPath.empty())
			{
				if (!WriteParentConsole(json))
				{
					_resultCode = 7;
					_resultStatus = L"output_failed";
					_resultError = L"No writable console was available for the performance JSON.";
				}
			}
			else if (!WriteJsonFile(json))
			{
				_resultCode = 7;
				_resultStatus = L"output_failed";
				_resultError = L"The performance JSON file could not be written atomically.";
				json = BuildJson();
				(void)WriteParentConsole(json);
			}
			else
			{
				(void)WriteParentConsole(json);
			}
			if (auto* application = Application::Current())
				application->Shutdown(_resultCode);
		}

		MediaPerformanceOptions _options;
		HANDLE _watchdogStartedEvent = nullptr;
		HANDLE _watchdogCompletionStartedEvent = nullptr;
		MediaElement* _player = nullptr;
		EventConnection _contentRendered;
		EventConnection _mediaError;
		EventConnection _mediaEnded;
		EventConnection _positionChanged;
		EventConnection _closing;
		LARGE_INTEGER _createdAt{};
		LARGE_INTEGER _measurementStartedAt{};
		LARGE_INTEGER _completedAt{};
		ProcessSample _processStarted{};
		MediaCompletionSnapshot _completionSnapshot{};
		bool _startAttempted = false;
		bool _measurementStarted = false;
		bool _closeRequested = false;
		bool _finishing = false;
		int _resultCode = 0;
		std::wstring _resultStatus = L"not_started";
		std::wstring _resultError;
		HRESULT _lastMediaError = S_OK;
		UINT64 _mediaEndedEvents = 0;
		double _timelineAdvanceSeconds = 0.0;
		double _lastTimelinePositionSeconds = 0.0;
		bool _timelinePositionObserved = false;
		bool _timelineMeasurementOpen = false;
		UINT64 _timelineWrapsObserved = 0;
		UINT64 _timelineBackwardDiscontinuities = 0;
		UINT64 _timelineEndedEventsConsumed = 0;
		double _timelineRegressionToleranceSeconds = 0.25;
		double _timelineWrapWindowSeconds = 0.5;
		UINT64 _presentationGenerationStarted = 0;
		UINT64 _presentationCommittedFramesStarted = 0;
		UINT64 _presentationGenerationAtDeviceLoss = 0;
		UINT64 _presentationRecoveriesAtDeviceLoss = 0;
		UINT64 _presentationCommittedFramesAtDeviceLoss = 0;
		UINT64 _gpuFramesAtPresentationDeviceLoss = 0;
		UINT64 _submittedFramesAtPresentationDeviceLoss = 0;
		bool _presentationDeviceLossInjected = false;
		UINT64 _sharedDeviceGenerationAtRotation = 0;
		UINT64 _gpuDeviceRebindsAtSharedDeviceRotation = 0;
		UINT64 _presentationGenerationAtSharedDeviceRotation = 0;
		UINT64 _presentationRecoveriesAtSharedDeviceRotation = 0;
		UINT64 _presentationCommittedFramesAtSharedDeviceRotation = 0;
		UINT64 _gpuFramesAtSharedDeviceRotation = 0;
		UINT64 _submittedFramesAtSharedDeviceRotation = 0;
		bool _sharedDeviceRotationInjected = false;
	};
}

MediaPerformanceCommandLine ParseMediaPerformanceCommandLine()
{
	MediaPerformanceCommandLine result;
	int argumentCount = 0;
	wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (!arguments || argumentCount <= 1)
	{
		if (arguments) LocalFree(arguments);
		return result;
	}

	struct ArgumentArray final
	{
		wchar_t** Value = nullptr;
		~ArgumentArray() { if (Value) LocalFree(Value); }
	} argumentOwner{ arguments };

	bool mediaSeen = false;
	bool rateSeen = false;
	bool durationSeen = false;
	bool injectPresentationDeviceLossSeen = false;
	bool injectSharedDeviceRotationSeen = false;
	bool jsonSeen = false;
	bool videoPathSeen = false;
	bool requireAudioSeen = false;
	bool expectedWidthSeen = false;
	bool expectedHeightSeen = false;
	bool expectedFpsSeen = false;
	bool requested = false;
	for (int index = 1; index < argumentCount; ++index)
	{
		const std::wstring_view argument(arguments[index]);
		if (argument == L"--media" || argument == L"--rate"
			|| argument == L"--duration" || argument == L"--perf-json"
			|| argument == L"--video-path"
			|| argument == L"--require-audio"
			|| argument == L"--expect-width"
			|| argument == L"--expect-height"
			|| argument == L"--expect-fps"
			|| argument == L"--inject-presentation-device-loss-at"
			|| argument == L"--inject-shared-device-rotation-at")
		{
			requested = true;
			break;
		}
	}
	if (!requested) return result;
	result.State = MediaPerformanceParseState::Invalid;

	for (int index = 1; index < argumentCount; ++index)
	{
		const std::wstring_view argument(arguments[index]);
		if (argument == L"--require-audio")
		{
			if (requireAudioSeen)
			{
				result.Error = L"--require-audio may be specified only once.";
				return result;
			}
			requireAudioSeen = true;
			result.Options.RequireAudio = true;
			continue;
		}
		if (argument == L"--media" || argument == L"--rate"
			|| argument == L"--duration" || argument == L"--perf-json"
			|| argument == L"--video-path"
			|| argument == L"--expect-width"
			|| argument == L"--expect-height"
			|| argument == L"--expect-fps"
			|| argument == L"--inject-presentation-device-loss-at"
			|| argument == L"--inject-shared-device-rotation-at")
		{
			if (index + 1 >= argumentCount)
			{
				result.Error = std::wstring(argument) + L" requires a value.";
				return result;
			}
			const std::wstring value(arguments[++index]);
			if (argument == L"--media")
			{
				if (mediaSeen)
				{
					result.Error = L"--media may be specified only once.";
					return result;
				}
				mediaSeen = true;
				result.Options.MediaPath = value;
			}
			else if (argument == L"--rate")
			{
				if (rateSeen || !TryParseFiniteDouble(value, result.Options.Rate))
				{
					result.Error = L"--rate requires one finite numeric value.";
					return result;
				}
				rateSeen = true;
			}
			else if (argument == L"--duration")
			{
				if (durationSeen
					|| !TryParseFiniteDouble(value,
						result.Options.DurationSeconds))
				{
					result.Error = L"--duration requires one finite numeric value.";
					return result;
				}
				durationSeen = true;
			}
			else if (argument == L"--expect-width"
				|| argument == L"--expect-height")
			{
				bool& seen = argument == L"--expect-width"
					? expectedWidthSeen : expectedHeightSeen;
				double parsed = 0.0;
				if (seen || !TryParseFiniteDouble(value, parsed)
					|| parsed < 1.0
					|| parsed > static_cast<double>(
						(std::numeric_limits<std::uint32_t>::max)())
					|| std::floor(parsed) != parsed)
				{
					result.Error = std::wstring(argument)
						+ L" requires one positive integer value.";
					return result;
				}
				seen = true;
				if (argument == L"--expect-width")
					result.Options.ExpectedVideoWidth =
						static_cast<std::uint32_t>(parsed);
				else
					result.Options.ExpectedVideoHeight =
						static_cast<std::uint32_t>(parsed);
			}
			else if (argument == L"--expect-fps")
			{
				if (expectedFpsSeen
					|| !TryParseFiniteDouble(value,
						result.Options.ExpectedVideoFramesPerSecond)
					|| result.Options.ExpectedVideoFramesPerSecond <= 0.0
					|| result.Options.ExpectedVideoFramesPerSecond > 1000.0)
				{
					result.Error = L"--expect-fps requires one value greater than zero and no more than 1000.";
					return result;
				}
				expectedFpsSeen = true;
			}
			else if (argument == L"--inject-presentation-device-loss-at")
			{
				if (injectPresentationDeviceLossSeen
					|| !TryParseFiniteDouble(value,
						result.Options.InjectPresentationDeviceLossAtSeconds))
				{
					result.Error = L"--inject-presentation-device-loss-at requires one finite numeric value.";
					return result;
				}
				injectPresentationDeviceLossSeen = true;
			}
			else if (argument == L"--inject-shared-device-rotation-at")
			{
				if (injectSharedDeviceRotationSeen
					|| !TryParseFiniteDouble(value,
						result.Options.InjectSharedDeviceRotationAtSeconds))
				{
					result.Error = L"--inject-shared-device-rotation-at requires one finite numeric value.";
					return result;
				}
				injectSharedDeviceRotationSeen = true;
			}
			else if (argument == L"--perf-json")
			{
				if (jsonSeen)
				{
					result.Error = L"--perf-json may be specified only once.";
					return result;
				}
				if (value.empty())
				{
					result.Error = L"--perf-json requires a non-empty path.";
					return result;
				}
				jsonSeen = true;
				result.Options.PerfJsonPath = value;
			}
			else
			{
				if (videoPathSeen)
				{
					result.Error = L"--video-path may be specified only once.";
					return result;
				}
				videoPathSeen = true;
				if (value == L"auto")
					result.Options.VideoPath = MediaPerformanceVideoPath::Auto;
				else if (value == L"cpu")
					result.Options.VideoPath = MediaPerformanceVideoPath::Cpu;
				else if (value == L"gpu-required")
					result.Options.VideoPath = MediaPerformanceVideoPath::GpuRequired;
				else
				{
					result.Error = L"--video-path must be auto, cpu, or gpu-required.";
					return result;
				}
			}
			continue;
		}

		result.Error = L"Unsupported media performance argument: "
			+ std::wstring(argument);
		return result;
	}
	if (!mediaSeen)
	{
		result.Error = L"--media <path> is required for a media performance run.";
		return result;
	}
	if (result.Options.Rate < 0.1 || result.Options.Rate > 4.0)
	{
		result.Error = L"--rate must be in the CUITest range 0.1 through 4.0.";
		return result;
	}
	if (result.Options.DurationSeconds < 2.0
		|| result.Options.DurationSeconds > 3600.0)
	{
		result.Error = L"--duration must be between 2 and 3600 seconds for a meaningful performance gate.";
		return result;
	}
	if (injectPresentationDeviceLossSeen
		&& (result.Options.InjectPresentationDeviceLossAtSeconds <= 0.0
			|| result.Options.InjectPresentationDeviceLossAtSeconds
				> result.Options.DurationSeconds - 0.5))
	{
		result.Error = L"--inject-presentation-device-loss-at must be greater than zero and leave at least 0.5 seconds for recovery.";
		return result;
	}
	if (injectSharedDeviceRotationSeen
		&& (result.Options.InjectSharedDeviceRotationAtSeconds <= 0.0
			|| result.Options.InjectSharedDeviceRotationAtSeconds
				> result.Options.DurationSeconds - 0.5))
	{
		result.Error = L"--inject-shared-device-rotation-at must be greater than zero and leave at least 0.5 seconds for recovery.";
		return result;
	}
	if (injectPresentationDeviceLossSeen && injectSharedDeviceRotationSeen)
	{
		result.Error = L"--inject-presentation-device-loss-at and "
			L"--inject-shared-device-rotation-at are mutually exclusive.";
		return result;
	}

	std::error_code pathError;
	result.Options.MediaPath = std::filesystem::absolute(
		result.Options.MediaPath, pathError).lexically_normal();
	if (pathError || !std::filesystem::is_regular_file(
		result.Options.MediaPath, pathError) || pathError)
	{
		result.Error = L"--media does not name a readable regular file.";
		return result;
	}
	if (!result.Options.PerfJsonPath.empty())
	{
		pathError.clear();
		result.Options.PerfJsonPath = std::filesystem::absolute(
			result.Options.PerfJsonPath, pathError).lexically_normal();
		const auto parent = result.Options.PerfJsonPath.parent_path();
		if (pathError || (!parent.empty()
			&& !std::filesystem::is_directory(parent, pathError)) || pathError)
		{
			result.Error = L"The parent directory for --perf-json does not exist.";
			return result;
		}
		if (_wcsicmp(result.Options.MediaPath.c_str(),
			result.Options.PerfJsonPath.c_str()) == 0)
		{
			result.Error = L"--perf-json must not overwrite the input media file.";
			return result;
		}
		pathError.clear();
		if (std::filesystem::exists(result.Options.PerfJsonPath, pathError))
		{
			if (pathError || std::filesystem::equivalent(
				result.Options.MediaPath,
				result.Options.PerfJsonPath, pathError) || pathError)
			{
				result.Error = pathError
					? L"The --perf-json target could not be inspected safely."
					: L"--perf-json must not alias the input media file.";
				return result;
			}
		}
		else if (pathError)
		{
			result.Error = L"The --perf-json target could not be inspected safely.";
			return result;
		}
	}

	result.State = MediaPerformanceParseState::Ready;
	result.Error.clear();
	return result;
}

int RunMediaPerformance(
	const MediaPerformanceOptions& options,
	std::wstring* error)
{
	UniqueHandle watchdogStarted(CreateEventW(nullptr, TRUE, FALSE, nullptr));
	UniqueHandle watchdogCompletionStarted(
		CreateEventW(nullptr, TRUE, FALSE, nullptr));
	UniqueHandle watchdogCompleted(CreateEventW(nullptr, TRUE, FALSE, nullptr));
	if (!watchdogStarted || !watchdogCompletionStarted || !watchdogCompleted)
		throw std::runtime_error("CreateEvent failed for media performance watchdog");

	MediaWatchdogContext watchdogContext{
		watchdogStarted.Get(), watchdogCompletionStarted.Get(),
		watchdogCompleted.Get(), options.DurationSeconds };
	UniqueHandle watchdogThread(CreateThread(
		nullptr, 0, &MediaWatchdogThreadProc, &watchdogContext, 0, nullptr));
	if (!watchdogThread)
		throw std::runtime_error("CreateThread failed for media performance watchdog");

	try
	{
		// Start the native watchdog before any CUI object construction so a
		// constructor-time deadlock is bounded as well as Load/TryPlay/runtime.
		int result = 0;
		int windowResult = 0;
		std::wstring windowError;
		{
			Application application;
			MediaPerformanceWindow window(
				options, watchdogStarted.Get(), watchdogCompletionStarted.Get());
			try
			{
				result = application.Run(window);
				windowResult = window.ResultCode();
				windowError = window.ResultError();
			}
			catch (...)
			{
				// Signal before stack unwinding destroys Window/Application so an
				// exceptional teardown receives the same bounded completion grace.
				(void)SetEvent(watchdogCompletionStarted.Get());
				throw;
			}
		}
		// Keep the watchdog alive through Window/Application destruction.
		(void)SetEvent(watchdogCompleted.Get());
		(void)WaitForSingleObject(watchdogThread.Get(), INFINITE);
		if (error) *error = std::move(windowError);
		return result != 0 ? result : windowResult;
	}
	catch (...)
	{
		(void)SetEvent(watchdogCompleted.Get());
		(void)WaitForSingleObject(watchdogThread.Get(), INFINITE);
		throw;
	}
}
