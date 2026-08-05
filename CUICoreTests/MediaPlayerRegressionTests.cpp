#include "MediaPlayerRegressionTests.h"

#include "TestRunner.h"
#include <Graphics.h>
#include <MediaPlayer.h>
#include <PresentationRenderHost.h>
#include <Window.h>
#include <WindowInfrastructure.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <Core/Threading.h>

struct MediaPlayerRegressionTestAccess final
{
	static UINT64 MeasureWsolaOutputFrames(
		float rate, UINT32 inputFrames, UINT32 chunkFrames)
	{
		return MediaPlayer::MeasureWsolaOutputFramesForTesting(
			rate, inputFrames, chunkFrames);
	}
	static constexpr UINT8 ReaderVideo() noexcept
	{
		return MediaPlayer::PlaybackEndReaderVideo;
	}
	static constexpr UINT8 ReaderAudio() noexcept
	{
		return MediaPlayer::PlaybackEndReaderAudio;
	}
	static constexpr UINT8 CompanionSession() noexcept
	{
		return MediaPlayer::PlaybackEndCompanionSession;
	}
	static void Configure(
		MediaPlayer& player, bool companion, bool loaded = true) noexcept
	{
		player._useSourceReader.store(true, std::memory_order_release);
		player._useMediaSessionAudioCompanion.store(
			companion, std::memory_order_release);
		player._mediaLoaded.store(loaded, std::memory_order_release);
		player._loop.store(false, std::memory_order_release);
	}
	static UINT64 Begin(MediaPlayer& player, UINT8 expectedMask) noexcept
	{
		return player.BeginPlaybackEndEpoch(expectedMask, false);
	}
	static UINT64 QueueSessionStart(MediaPlayer& player) noexcept
	{
		return player.QueueCompanionSessionStartEpoch();
	}
	static void ObserveSessionStarted(MediaPlayer& player) noexcept
	{
		(void)player.ObserveCompanionSessionStarted(S_OK);
	}
	static UINT64 PendingReaderWorkerStart(MediaPlayer& player) noexcept
	{
		std::scoped_lock lock(player._playbackEndMutex);
		return player._pendingSourceReaderWorkerStartEpoch;
	}
	static bool TakeReaderWorkerStart(
		MediaPlayer& player, UINT64 epoch) noexcept
	{
		return player.TakeSourceReaderWorkerStartForEpoch(epoch);
	}
	static bool ObserveSessionStartFailed(MediaPlayer& player) noexcept
	{
		return player.ObserveCompanionSessionStarted(E_FAIL)
			== MediaPlayer::CompanionSessionObservation::FailedCurrent;
	}
	static void HandleSessionFailure(
		MediaPlayer& player, HRESULT error, UINT64 epoch)
	{
		player.HandleCompanionSessionFailure(error, epoch);
	}
	static void ObserveSessionEnded(MediaPlayer& player)
	{
		(void)player.ObserveCompanionSessionEnded(S_OK);
	}
	static UINT64 CaptureSessionFailureEpoch(MediaPlayer& player) noexcept
	{
		return player.CaptureCompanionSessionFailureEpoch();
	}
	static UINT64 QueueSessionPause(MediaPlayer& player) noexcept
	{
		return player.QueueCompanionSessionControlEpoch(
			MediaPlayer::CompanionSessionControlKind::Pause);
	}
	static UINT64 QueueSessionStop(MediaPlayer& player) noexcept
	{
		return player.QueueCompanionSessionControlEpoch(
			MediaPlayer::CompanionSessionControlKind::Stop);
	}
	static bool ObserveSessionPause(MediaPlayer& player, HRESULT status) noexcept
	{
		return player.ObserveCompanionSessionControl(
			MediaPlayer::CompanionSessionControlKind::Pause, status)
			== MediaPlayer::CompanionSessionObservation::Accepted;
	}
	static bool ObserveSessionStopFailed(MediaPlayer& player) noexcept
	{
		return player.ObserveCompanionSessionControl(
			MediaPlayer::CompanionSessionControlKind::Stop, E_FAIL)
			== MediaPlayer::CompanionSessionObservation::FailedCurrent;
	}
	static void ConfigureStandalone(MediaPlayer& player) noexcept
	{
		player._useSourceReader.store(false, std::memory_order_release);
		player._mediaLoaded.store(true, std::memory_order_release);
	}
	static UINT64 CurrentExplicit(MediaPlayer& player) noexcept
	{
		return player.CurrentExplicitPlaybackCommandGeneration();
	}
	static UINT64 AdvanceExplicit(MediaPlayer& player) noexcept
	{
		return player.AdvanceExplicitPlaybackCommandGeneration();
	}
	static UINT64 QueueAcceptedStandaloneStart(MediaPlayer& player) noexcept
	{
		auto token = player.QueueStandaloneSessionCommand(
			MediaPlayer::StandaloneSessionCommandKind::Start);
		player.CommitStandaloneSessionCommandSuccess(token);
		return token.Sequence;
	}
	static bool ObserveStandaloneStart(MediaPlayer& player) noexcept
	{
		return player.ObserveStandaloneSessionCommand(
			MediaPlayer::StandaloneSessionCommandKind::Start, S_OK)
			== MediaPlayer::CompanionSessionObservation::Accepted;
	}
	static bool ObserveStandaloneEnd(MediaPlayer& player) noexcept
	{
		return player.ObserveStandaloneSessionEnded(S_OK)
			== MediaPlayer::CompanionSessionObservation::Accepted;
	}
	static std::pair<UINT64, UINT64>
		CompleteStandaloneStartBeforeSynchronousReturn(
			MediaPlayer& player) noexcept
	{
		const auto token = player.QueueStandaloneSessionCommand(
			MediaPlayer::StandaloneSessionCommandKind::Start);
		(void)player.ObserveStandaloneSessionCommand(
			MediaPlayer::StandaloneSessionCommandKind::Start, S_OK);
		(void)player.ObserveStandaloneSessionEnded(S_OK);
		player.CommitStandaloneSessionCommandSuccess(token);
		const auto completion =
			player.CaptureStandaloneSessionCompletionToken();
		return { token.Sequence, completion.Sequence };
	}
	static std::pair<UINT64, UINT64> CaptureStandaloneFailure(
		MediaPlayer& player) noexcept
	{
		const auto token = player.CaptureStandaloneSessionFailureToken();
		return { token.Sequence, token.ExplicitCommandGeneration };
	}
	static std::pair<UINT64, UINT64> ActiveStandalone(
		MediaPlayer& player) noexcept
	{
		std::scoped_lock lock(player._sessionStateMutex);
		return {
			player._activeStandaloneSessionPlayback.Sequence,
			player._activeStandaloneSessionPlayback
				.ExplicitCommandGeneration };
	}
	static void FailStandaloneStopSynchronously(MediaPlayer& player) noexcept
	{
		const auto token = player.QueueStandaloneSessionCommand(
			MediaPlayer::StandaloneSessionCommandKind::Stop);
		player.RestoreStandaloneSessionIdentityAfterCommandFailure(token);
	}
	static bool Signal(MediaPlayer& player, UINT8 mask, UINT64 epoch)
	{
		return player.SignalPlaybackEnd(mask, epoch);
	}
};

namespace
{
	constexpr wchar_t kChildMediaPathVariable[] =
		L"CUI_TEST_MEDIA_VIDEO_ONLY_CHILD_PATH";
	constexpr wchar_t kSharedAcquireChildVariable[] =
		L"CUI_TEST_SHARED_GRAPHICS_ACQUIRE_CHILD";
	constexpr wchar_t kTestFilterVariable[] = L"CUI_TEST_FILTER";
	constexpr char kVideoOnlyTestName[] =
		"MediaPlayer video-only source load completes without audio negotiation spin";
	constexpr wchar_t kVideoOnlyTestFilter[] =
		L"MediaPlayer video-only source load completes without audio negotiation spin";
	constexpr char kSharedAcquireTestName[] =
		"Shared graphics device acquire is strong atomic and stable";
	constexpr wchar_t kSharedAcquireChildArgument[] =
		L"--cui-shared-graphics-acquire-child";

	std::wstring WidenAscii(const char* value)
	{
		std::wstring result;
		if (!value) return result;
		while (*value != '\0')
			result.push_back(static_cast<unsigned char>(*value++));
		return result;
	}

	std::wstring ReadEnvironmentVariable(
		const wchar_t* name, bool* wasPresent = nullptr)
	{
		::SetLastError(ERROR_SUCCESS);
		const DWORD required = ::GetEnvironmentVariableW(name, nullptr, 0);
		if (required == 0)
		{
			if (wasPresent)
				*wasPresent = ::GetLastError() != ERROR_ENVVAR_NOT_FOUND;
			return {};
		}
		std::wstring value(required, L'\0');
		const DWORD written = ::GetEnvironmentVariableW(
			name, value.data(), static_cast<DWORD>(value.size()));
		if (written == 0 || written >= value.size()) return {};
		value.resize(written);
		if (wasPresent) *wasPresent = true;
		return value;
	}

	class ScopedEnvironmentVariable final
	{
	public:
		ScopedEnvironmentVariable(const wchar_t* name, const std::wstring& value)
			: _name(name)
		{
			_previous = ReadEnvironmentVariable(name, &_previousWasPresent);
			if (!::SetEnvironmentVariableW(_name.c_str(), value.c_str()))
				throw std::runtime_error("SetEnvironmentVariableW failed");
		}

		~ScopedEnvironmentVariable()
		{
			(void)::SetEnvironmentVariableW(
				_name.c_str(), _previousWasPresent ? _previous.c_str() : nullptr);
		}

		ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
		ScopedEnvironmentVariable& operator=(
			const ScopedEnvironmentVariable&) = delete;

	private:
		std::wstring _name;
		std::wstring _previous;
		bool _previousWasPresent = false;
	};

	class ScopedMediaFoundation final
	{
	public:
		ScopedMediaFoundation()
			: Result(::MFStartup(MF_VERSION, MFSTARTUP_FULL))
		{
		}

		~ScopedMediaFoundation()
		{
			if (SUCCEEDED(Result)) (void)::MFShutdown();
		}

		HRESULT Result;
	};

	bool HasSameComIdentity(IUnknown* left, IUnknown* right)
	{
		if (!left || !right) return false;
		Microsoft::WRL::ComPtr<IUnknown> leftIdentity;
		Microsoft::WRL::ComPtr<IUnknown> rightIdentity;
		return SUCCEEDED(left->QueryInterface(
			IID_PPV_ARGS(leftIdentity.GetAddressOf())))
			&& SUCCEEDED(right->QueryInterface(
				IID_PPV_ARGS(rightIdentity.GetAddressOf())))
			&& leftIdentity.Get() == rightIdentity.Get();
	}

	struct SharedAcquireState final
	{
		Microsoft::WRL::ComPtr<ID3D11Device> ExpectedDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> ExpectedContext;
		Microsoft::WRL::ComPtr<IDXGIDevice> ExpectedDxgiDevice;
		Microsoft::WRL::ComPtr<ID2D1Device> ExpectedD2DDevice;
		GraphicsSharedD3DDeviceInfo ExpectedInfo{};
		std::atomic<bool> Succeeded{ true };
	};

	struct SharedAcquireThreadContext final
	{
		std::shared_ptr<SharedAcquireState> State;
	};

	DWORD WINAPI SharedAcquireThreadProc(void* parameter)
	{
		std::unique_ptr<SharedAcquireThreadContext> context(
			static_cast<SharedAcquireThreadContext*>(parameter));
		if (!context || !context->State) return ERROR_INVALID_PARAMETER;
		const auto state = context->State;
		for (unsigned iteration = 0; iteration < 32; ++iteration)
		{
			Microsoft::WRL::ComPtr<ID3D11Device> device;
			Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
			Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
			Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
			GraphicsSharedD3DDeviceInfo info{};
			if (FAILED(Graphics_AcquireSharedD3DDevice(
				device.GetAddressOf(), immediateContext.GetAddressOf(),
				dxgiDevice.GetAddressOf(), d2dDevice.GetAddressOf(), &info))
				|| !device || !immediateContext || !dxgiDevice || !d2dDevice
				|| !HasSameComIdentity(device.Get(), state->ExpectedDevice.Get())
				|| !HasSameComIdentity(
					immediateContext.Get(), state->ExpectedContext.Get())
				|| !HasSameComIdentity(
					dxgiDevice.Get(), state->ExpectedDxgiDevice.Get())
				|| !HasSameComIdentity(
					d2dDevice.Get(), state->ExpectedD2DDevice.Get())
				|| info.Generation == 0
				|| info.Generation != state->ExpectedInfo.Generation
				|| info.SupportsVideo != state->ExpectedInfo.SupportsVideo
				|| info.IsHardware != state->ExpectedInfo.IsHardware)
			{
				state->Succeeded.store(false, std::memory_order_release);
				return ERROR_INVALID_STATE;
			}

			Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
			immediateContext->GetDevice(contextDevice.GetAddressOf());
			Microsoft::WRL::ComPtr<IDXGIDevice> queriedDxgiDevice;
			if (!contextDevice
				|| FAILED(device.As(&queriedDxgiDevice)) || !queriedDxgiDevice
				|| !HasSameComIdentity(contextDevice.Get(), device.Get())
				|| !HasSameComIdentity(dxgiDevice.Get(), queriedDxgiDevice.Get()))
			{
				state->Succeeded.store(false, std::memory_order_release);
				return ERROR_INVALID_STATE;
			}
		}
		return ERROR_SUCCESS;
	}

	struct PlaybackEndSignalThreadContext final
	{
		MediaPlayer* Player = nullptr;
		UINT8 Mask = 0;
		UINT64 Epoch = 0;
		std::atomic<int>* CompletionClaims = nullptr;
	};

	DWORD WINAPI PlaybackEndSignalThreadProc(void* parameter)
	{
		std::unique_ptr<PlaybackEndSignalThreadContext> context(
			static_cast<PlaybackEndSignalThreadContext*>(parameter));
		if (!context || !context->Player || !context->CompletionClaims)
			return ERROR_INVALID_PARAMETER;
		if (MediaPlayerRegressionTestAccess::Signal(
			*context->Player, context->Mask, context->Epoch))
		{
			context->CompletionClaims->fetch_add(
				1, std::memory_order_relaxed);
		}
		return ERROR_SUCCESS;
	}

	HANDLE StartPlaybackEndSignalThread(
		MediaPlayer& player, UINT8 mask, UINT64 epoch,
		std::atomic<int>& completionClaims)
	{
		auto context = std::make_unique<PlaybackEndSignalThreadContext>();
		context->Player = &player;
		context->Mask = mask;
		context->Epoch = epoch;
		context->CompletionClaims = &completionClaims;
		auto* parameter = context.release();
		HANDLE thread = ::CreateThread(
			nullptr, 0, PlaybackEndSignalThreadProc, parameter, 0, nullptr);
		if (!thread) delete parameter;
		return thread;
	}

	struct CompanionFailureThreadContext final
	{
		MediaPlayer* Player = nullptr;
		HRESULT Error = E_FAIL;
		UINT64 Epoch = 0;
	};

	DWORD WINAPI CompanionFailureThreadProc(void* parameter)
	{
		std::unique_ptr<CompanionFailureThreadContext> context(
			static_cast<CompanionFailureThreadContext*>(parameter));
		if (!context || !context->Player) return ERROR_INVALID_PARAMETER;
		MediaPlayerRegressionTestAccess::HandleSessionFailure(
			*context->Player, context->Error, context->Epoch);
		return ERROR_SUCCESS;
	}

	HANDLE StartCompanionFailureThread(
		MediaPlayer& player, HRESULT error, UINT64 epoch)
	{
		auto context = std::make_unique<CompanionFailureThreadContext>();
		context->Player = &player;
		context->Error = error;
		context->Epoch = epoch;
		auto* parameter = context.release();
		HANDLE thread = ::CreateThread(
			nullptr, 0, CompanionFailureThreadProc, parameter, 0, nullptr);
		if (!thread) delete parameter;
		return thread;
	}

	HRESULT ConfigureVideoType(
		IMFMediaType* type, const GUID& subtype, UINT32 width, UINT32 height)
	{
		if (!type) return E_POINTER;
		HRESULT hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_SUBTYPE, subtype);
		if (SUCCEEDED(hr)) hr = ::MFSetAttributeSize(
			type, MF_MT_FRAME_SIZE, width, height);
		if (SUCCEEDED(hr)) hr = ::MFSetAttributeRatio(
			type, MF_MT_FRAME_RATE, 30, 1);
		if (SUCCEEDED(hr)) hr = ::MFSetAttributeRatio(
			type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		if (SUCCEEDED(hr)) hr = type->SetUINT32(
			MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		return hr;
	}

	HRESULT CreateVideoOnlyMp4(const std::filesystem::path& outputPath)
	{
		ScopedMediaFoundation mediaFoundation;
		if (FAILED(mediaFoundation.Result)) return mediaFoundation.Result;

		constexpr UINT32 width = 64;
		constexpr UINT32 height = 64;
		constexpr UINT32 frameBytes = width * height * 4;
		constexpr LONGLONG frameDuration = 10'000'000LL / 30;

		Microsoft::WRL::ComPtr<IMFAttributes> attributes;
		HRESULT hr = ::MFCreateAttributes(&attributes, 1);
		if (SUCCEEDED(hr)) hr = attributes->SetUINT32(
			MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

		Microsoft::WRL::ComPtr<IMFSinkWriter> writer;
		if (SUCCEEDED(hr)) hr = ::MFCreateSinkWriterFromURL(
			outputPath.c_str(), nullptr, attributes.Get(), &writer);

		Microsoft::WRL::ComPtr<IMFMediaType> outputType;
		if (SUCCEEDED(hr)) hr = ::MFCreateMediaType(&outputType);
		if (SUCCEEDED(hr)) hr = ConfigureVideoType(
			outputType.Get(), MFVideoFormat_H264, width, height);
		if (SUCCEEDED(hr)) hr = outputType->SetUINT32(
			MF_MT_AVG_BITRATE, 128'000);

		DWORD streamIndex = 0;
		if (SUCCEEDED(hr)) hr = writer->AddStream(
			outputType.Get(), &streamIndex);

		Microsoft::WRL::ComPtr<IMFMediaType> inputType;
		if (SUCCEEDED(hr)) hr = ::MFCreateMediaType(&inputType);
		if (SUCCEEDED(hr)) hr = ConfigureVideoType(
			inputType.Get(), MFVideoFormat_RGB32, width, height);
		if (SUCCEEDED(hr)) hr = inputType->SetUINT32(
			MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		if (SUCCEEDED(hr)) hr = writer->SetInputMediaType(
			streamIndex, inputType.Get(), nullptr);
		if (SUCCEEDED(hr)) hr = writer->BeginWriting();

		for (UINT32 frame = 0; SUCCEEDED(hr) && frame < 2; ++frame)
		{
			Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
			hr = ::MFCreateMemoryBuffer(frameBytes, &buffer);
			BYTE* pixels = nullptr;
			DWORD capacity = 0;
			if (SUCCEEDED(hr)) hr = buffer->Lock(
				&pixels, &capacity, nullptr);
			if (SUCCEEDED(hr))
			{
				for (UINT32 pixel = 0; pixel < width * height; ++pixel)
				{
					pixels[pixel * 4 + 0] = static_cast<BYTE>(frame * 40);
					pixels[pixel * 4 + 1] = static_cast<BYTE>(pixel % width);
					pixels[pixel * 4 + 2] = static_cast<BYTE>(pixel / width);
					pixels[pixel * 4 + 3] = 0xff;
				}
				(void)buffer->Unlock();
				hr = buffer->SetCurrentLength(frameBytes);
			}

			Microsoft::WRL::ComPtr<IMFSample> sample;
			if (SUCCEEDED(hr)) hr = ::MFCreateSample(&sample);
			if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer.Get());
			if (SUCCEEDED(hr)) hr = sample->SetSampleTime(frame * frameDuration);
			if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(frameDuration);
			if (SUCCEEDED(hr)) hr = writer->WriteSample(streamIndex, sample.Get());
		}

		if (SUCCEEDED(hr)) hr = writer->Finalize();
		return hr;
	}

	std::filesystem::path CurrentExecutablePath()
	{
		std::wstring buffer(32768, L'\0');
		const DWORD written = ::GetModuleFileNameW(
			nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (written == 0 || written >= buffer.size()) return {};
		buffer.resize(written);
		return std::filesystem::path(std::move(buffer));
	}

	DWORD RunFilteredChild(
		const std::filesystem::path& executable,
		const wchar_t* childVariable,
		const std::wstring& childValue,
		const wchar_t* filterValue,
		const wchar_t* commandLineArgument,
		DWORD timeoutMilliseconds,
		bool& timedOut)
	{
		const ScopedEnvironmentVariable childMode(childVariable, childValue);
		const ScopedEnvironmentVariable filter(
			kTestFilterVariable, filterValue);

		std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
		if (commandLineArgument && *commandLineArgument)
		{
			commandLine.push_back(L' ');
			commandLine.append(commandLineArgument);
		}
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		if (!::CreateProcessW(
			executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
			0, nullptr, nullptr, &startup, &process))
			throw std::runtime_error("CreateProcessW failed");

		const DWORD waitResult = ::WaitForSingleObject(
			process.hProcess, timeoutMilliseconds);
		timedOut = waitResult == WAIT_TIMEOUT;
		const bool waitFailed = waitResult == WAIT_FAILED;
		if (timedOut || waitFailed)
		{
			(void)::TerminateProcess(process.hProcess, ERROR_TIMEOUT);
			(void)::WaitForSingleObject(process.hProcess, 5'000);
		}

		DWORD exitCode = STILL_ACTIVE;
		(void)::GetExitCodeProcess(process.hProcess, &exitCode);
		(void)::CloseHandle(process.hThread);
		(void)::CloseHandle(process.hProcess);
		if (waitFailed)
			throw std::runtime_error("WaitForSingleObject failed");
		return exitCode;
	}

	DWORD RunVideoOnlyLoadChild(
		const std::filesystem::path& executable,
		const std::filesystem::path& mediaPath,
		bool& timedOut)
	{
		return RunFilteredChild(
			executable, kChildMediaPathVariable, mediaPath.wstring(),
			kVideoOnlyTestFilter, nullptr, 15'000, timedOut);
	}
}

void RegisterMediaPlayerRegressionTests(cui::test::Runner& runner)
{
	runner.Add("MediaPlayer WSOLA compaction never advances beyond received PCM", []
	{
		constexpr UINT32 inputFrames = 48'000 * 5;
		constexpr UINT32 chunkFrames = 1'024;
		const UINT64 output2x =
			MediaPlayerRegressionTestAccess::MeasureWsolaOutputFrames(
				2.0f, inputFrames, chunkFrames);
		const UINT64 output4x =
			MediaPlayerRegressionTestAccess::MeasureWsolaOutputFrames(
				4.0f, inputFrames, chunkFrames);

		// Drain contributes a bounded tail, so admit 15% over the ideal
		// duration. The old future-base compaction produced about 1.6x at 4x
		// and exceeded this upper bound by more than 2x.
		CUI_EXPECT_TRUE(output2x >= 120'000);
		CUI_EXPECT_TRUE(output2x <= 138'000);
		CUI_EXPECT_TRUE(output4x >= 60'000);
		CUI_EXPECT_TRUE(output4x <= 69'000);
	});

	runner.Add("MediaPlayer end coordinator is epoch-safe and exactly once", []
	{
		MediaPlayer sourceReaderPlayer;
		MediaPlayerRegressionTestAccess::Configure(
			sourceReaderPlayer, false);
		int sourceReaderEnded = 0;
		auto sourceReaderConnection =
			sourceReaderPlayer.OnMediaEnded.Subscribe(
				[&](Control*) { ++sourceReaderEnded; });
		const UINT8 readerMask =
			MediaPlayerRegressionTestAccess::ReaderVideo()
			| MediaPlayerRegressionTestAccess::ReaderAudio();
		const UINT64 staleReaderEpoch =
			MediaPlayerRegressionTestAccess::Begin(
				sourceReaderPlayer, readerMask);
		std::atomic<int> completionClaims{ 0 };
		HANDLE staleCompletion = StartPlaybackEndSignalThread(
			sourceReaderPlayer, readerMask, staleReaderEpoch,
			completionClaims);
		CUI_EXPECT_TRUE(staleCompletion != nullptr);
		CUI_EXPECT_EQ(static_cast<DWORD>(WAIT_OBJECT_0),
			::WaitForSingleObject(staleCompletion, INFINITE));
		(void)::CloseHandle(staleCompletion);
		CUI_EXPECT_EQ(1, completionClaims.load(std::memory_order_relaxed));

		// The worker queued completion to the owner, but a seek wins before the
		// dispatcher executes it.  The captured epoch must make that callback a
		// no-op, and a late reader signal from the old epoch must also be ignored.
		const UINT64 readerEpoch = MediaPlayerRegressionTestAccess::Begin(
			sourceReaderPlayer, readerMask);
		CUI_EXPECT_TRUE(readerEpoch > staleReaderEpoch);
		CUI_EXPECT_FALSE(MediaPlayerRegressionTestAccess::Signal(
			sourceReaderPlayer, readerMask, staleReaderEpoch));
		cui::PumpUIThreadCallbacks();
		CUI_EXPECT_EQ(0, sourceReaderEnded);

		completionClaims.store(0, std::memory_order_relaxed);
		HANDLE endSignals[2]{
			StartPlaybackEndSignalThread(
				sourceReaderPlayer,
				MediaPlayerRegressionTestAccess::ReaderAudio(), readerEpoch,
				completionClaims),
			StartPlaybackEndSignalThread(
				sourceReaderPlayer,
				MediaPlayerRegressionTestAccess::ReaderVideo(), readerEpoch,
				completionClaims)
		};
		CUI_EXPECT_TRUE(endSignals[0] != nullptr);
		CUI_EXPECT_TRUE(endSignals[1] != nullptr);
		CUI_EXPECT_EQ(static_cast<DWORD>(WAIT_OBJECT_0),
			::WaitForMultipleObjects(2, endSignals, TRUE, INFINITE));
		(void)::CloseHandle(endSignals[0]);
		(void)::CloseHandle(endSignals[1]);
		CUI_EXPECT_EQ(1, completionClaims.load(std::memory_order_relaxed));
		cui::PumpUIThreadCallbacks();
		CUI_EXPECT_EQ(1, sourceReaderEnded);
		CUI_EXPECT_FALSE(MediaPlayerRegressionTestAccess::Signal(
			sourceReaderPlayer, readerMask, readerEpoch));
		CUI_EXPECT_EQ(1, sourceReaderEnded);
		sourceReaderPlayer.Close();

		MediaPlayer companionPlayer;
		MediaPlayerRegressionTestAccess::Configure(companionPlayer, true);
		int companionEnded = 0;
		int companionFailures = 0;
		auto companionConnection = companionPlayer.OnMediaEnded.Subscribe(
			[&](Control*) { ++companionEnded; });
		auto companionFailureConnection = companionPlayer.OnMediaFailed.Subscribe(
			[&](Control*) { ++companionFailures; });
		const UINT8 companionMask =
			MediaPlayerRegressionTestAccess::ReaderVideo()
			| MediaPlayerRegressionTestAccess::CompanionSession();
		const UINT64 staleEpoch = MediaPlayerRegressionTestAccess::Begin(
			companionPlayer, companionMask);
		CUI_EXPECT_EQ(staleEpoch,
			MediaPlayerRegressionTestAccess::QueueSessionStart(
				companionPlayer));
		CUI_EXPECT_EQ(staleEpoch,
			MediaPlayerRegressionTestAccess::PendingReaderWorkerStart(
				companionPlayer));

		// A seek creates a new epoch before the old Started/Ended pair arrives.
		const UINT64 currentEpoch = MediaPlayerRegressionTestAccess::Begin(
			companionPlayer, companionMask);
		CUI_EXPECT_TRUE(currentEpoch > staleEpoch);
		CUI_EXPECT_EQ(static_cast<UINT64>(0),
			MediaPlayerRegressionTestAccess::PendingReaderWorkerStart(
				companionPlayer));
		MediaPlayerRegressionTestAccess::ObserveSessionStarted(companionPlayer);
		MediaPlayerRegressionTestAccess::ObserveSessionEnded(companionPlayer);
		CUI_EXPECT_EQ(0, companionEnded);
		CUI_EXPECT_FALSE(MediaPlayerRegressionTestAccess::Signal(
			companionPlayer,
			MediaPlayerRegressionTestAccess::ReaderVideo(), currentEpoch));
		CUI_EXPECT_EQ(currentEpoch,
			MediaPlayerRegressionTestAccess::QueueSessionStart(
				companionPlayer));
		CUI_EXPECT_EQ(currentEpoch,
			MediaPlayerRegressionTestAccess::PendingReaderWorkerStart(
				companionPlayer));
		MediaPlayerRegressionTestAccess::ObserveSessionStarted(companionPlayer);
		CUI_EXPECT_FALSE(
			MediaPlayerRegressionTestAccess::TakeReaderWorkerStart(
				companionPlayer, staleEpoch));
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::TakeReaderWorkerStart(
				companionPlayer, currentEpoch));
		CUI_EXPECT_FALSE(
			MediaPlayerRegressionTestAccess::TakeReaderWorkerStart(
				companionPlayer, currentEpoch));
		MediaPlayerRegressionTestAccess::ObserveSessionEnded(companionPlayer);
		CUI_EXPECT_EQ(1, companionEnded);
		MediaPlayerRegressionTestAccess::ObserveSessionEnded(companionPlayer);
		CUI_EXPECT_FALSE(MediaPlayerRegressionTestAccess::Signal(
			companionPlayer, companionMask, currentEpoch));
		CUI_EXPECT_EQ(1, companionEnded);

		const UINT64 failedStartEpoch =
			MediaPlayerRegressionTestAccess::Begin(
				companionPlayer, companionMask);
		CUI_EXPECT_EQ(failedStartEpoch,
			MediaPlayerRegressionTestAccess::QueueSessionStart(
				companionPlayer));
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::ObserveSessionStartFailed(
				companionPlayer));
		CUI_EXPECT_EQ(static_cast<UINT64>(0),
			MediaPlayerRegressionTestAccess::PendingReaderWorkerStart(
				companionPlayer));
		CUI_EXPECT_FALSE(MediaPlayerRegressionTestAccess::Signal(
			companionPlayer,
			MediaPlayerRegressionTestAccess::ReaderVideo(), failedStartEpoch));
		MediaPlayerRegressionTestAccess::ObserveSessionEnded(companionPlayer);
		CUI_EXPECT_EQ(1, companionEnded);

		// A callback thread can post a failure just before a replacement seek.
		// The owner continuation must bind to the captured transaction instead
		// of stopping or reporting against the replacement epoch.
		const UINT64 queuedFailureEpoch =
			MediaPlayerRegressionTestAccess::Begin(
				companionPlayer, companionMask);
		HANDLE queuedFailure = StartCompanionFailureThread(
			companionPlayer, E_ABORT, queuedFailureEpoch);
		CUI_EXPECT_TRUE(queuedFailure != nullptr);
		CUI_EXPECT_EQ(static_cast<DWORD>(WAIT_OBJECT_0),
			::WaitForSingleObject(queuedFailure, INFINITE));
		(void)::CloseHandle(queuedFailure);
		const UINT64 replacementEpoch =
			MediaPlayerRegressionTestAccess::Begin(
				companionPlayer, companionMask);
		CUI_EXPECT_TRUE(replacementEpoch > queuedFailureEpoch);
		cui::PumpUIThreadCallbacks();
		CUI_EXPECT_EQ(0, companionFailures);

		// The same failure remains terminal while its captured epoch is current.
		MediaPlayerRegressionTestAccess::HandleSessionFailure(
			companionPlayer, E_ABORT, replacementEpoch);
		CUI_EXPECT_EQ(0, companionFailures);
		cui::PumpUIThreadCallbacks();
		CUI_EXPECT_EQ(1, companionFailures);
		CUI_EXPECT_FALSE(MediaPlayerRegressionTestAccess::Signal(
			companionPlayer, companionMask, replacementEpoch));
		companionPlayer.Close();
	});

	runner.Add("MediaPlayer async session provenance rejects stale callbacks", []
	{
		MediaPlayer standalone;
		MediaPlayerRegressionTestAccess::ConfigureStandalone(standalone);
		const UINT64 firstGeneration =
			MediaPlayerRegressionTestAccess::CurrentExplicit(standalone);
		const UINT64 firstStart =
			MediaPlayerRegressionTestAccess::QueueAcceptedStandaloneStart(
				standalone);
		// Synchronous IMFMediaSession::Start success is only acceptance; the
		// active run must remain unpublished until MESessionStarted arrives.
		CUI_EXPECT_EQ(static_cast<UINT64>(0),
			MediaPlayerRegressionTestAccess::ActiveStandalone(standalone).first);
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::ObserveStandaloneStart(standalone));
		CUI_EXPECT_EQ(firstStart,
			MediaPlayerRegressionTestAccess::ActiveStandalone(standalone).first);

		const UINT64 replacementGeneration =
			MediaPlayerRegressionTestAccess::AdvanceExplicit(standalone);
		const UINT64 replacementStart =
			MediaPlayerRegressionTestAccess::QueueAcceptedStandaloneStart(
				standalone);
		CUI_EXPECT_EQ(firstStart,
			MediaPlayerRegressionTestAccess::ActiveStandalone(standalone).first);
		const auto staleFailure =
			MediaPlayerRegressionTestAccess::CaptureStandaloneFailure(standalone);
		CUI_EXPECT_EQ(firstStart, staleFailure.first);
		CUI_EXPECT_EQ(firstGeneration, staleFailure.second);
		CUI_EXPECT_FALSE(
			MediaPlayerRegressionTestAccess::ObserveStandaloneEnd(standalone));
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::ObserveStandaloneStart(standalone));
		const auto replacementActive =
			MediaPlayerRegressionTestAccess::ActiveStandalone(standalone);
		CUI_EXPECT_EQ(replacementStart, replacementActive.first);
		CUI_EXPECT_EQ(replacementGeneration, replacementActive.second);

		// A command rejected synchronously leaves the previous playback alive;
		// rebind it to the new explicit generation so its eventual Ended remains
		// observable instead of being discarded as stale.
		const UINT64 failedCommandGeneration =
			MediaPlayerRegressionTestAccess::AdvanceExplicit(standalone);
		MediaPlayerRegressionTestAccess::FailStandaloneStopSynchronously(
			standalone);
		const auto restoredActive =
			MediaPlayerRegressionTestAccess::ActiveStandalone(standalone);
		CUI_EXPECT_EQ(replacementStart, restoredActive.first);
		CUI_EXPECT_EQ(failedCommandGeneration, restoredActive.second);
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::ObserveStandaloneEnd(standalone));

		MediaPlayer synchronousCompletion;
		MediaPlayerRegressionTestAccess::ConfigureStandalone(
			synchronousCompletion);
		const auto synchronousStart = MediaPlayerRegressionTestAccess::
			CompleteStandaloneStartBeforeSynchronousReturn(
				synchronousCompletion);
		CUI_EXPECT_TRUE(synchronousStart.first != 0);
		CUI_EXPECT_EQ(synchronousStart.first, synchronousStart.second);

		MediaPlayer companion;
		MediaPlayerRegressionTestAccess::Configure(companion, true);
		const UINT8 companionMask =
			MediaPlayerRegressionTestAccess::ReaderVideo()
			| MediaPlayerRegressionTestAccess::CompanionSession();
		const UINT64 activeEpoch = MediaPlayerRegressionTestAccess::Begin(
			companion, companionMask);
		CUI_EXPECT_EQ(activeEpoch,
			MediaPlayerRegressionTestAccess::QueueSessionStart(companion));
		MediaPlayerRegressionTestAccess::ObserveSessionStarted(companion);
		const UINT64 pauseEpoch = MediaPlayerRegressionTestAccess::Begin(
			companion, companionMask);
		CUI_EXPECT_EQ(pauseEpoch,
			MediaPlayerRegressionTestAccess::QueueSessionPause(companion));
		// A generic error delivered before the Pause status is still owned by
		// the superseded active run and must be rejected by the new epoch.
		CUI_EXPECT_EQ(activeEpoch,
			MediaPlayerRegressionTestAccess::CaptureSessionFailureEpoch(
				companion));
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::ObserveSessionPause(
				companion, S_OK));
		const UINT64 stopEpoch = MediaPlayerRegressionTestAccess::Begin(
			companion, companionMask);
		CUI_EXPECT_EQ(stopEpoch,
			MediaPlayerRegressionTestAccess::QueueSessionStop(companion));
		CUI_EXPECT_TRUE(
			MediaPlayerRegressionTestAccess::ObserveSessionStopFailed(
				companion));
	});

	runner.Add("Presentation host retries a transient initial attach failure", []
	{
		using WindowAccess = cui::framework::WindowAccess;
		PresentationRenderHost::FailNextPrimaryAttachForTesting();
		Window window;
		CUI_EXPECT_TRUE(window.Handle != nullptr);
		CUI_EXPECT_EQ(static_cast<UINT64>(0),
			WindowAccess::PresentationResourceGeneration(window));
		CUI_EXPECT_EQ(static_cast<UINT64>(0),
			WindowAccess::PresentationDeviceRecoveryCount(window));
		CUI_EXPECT_EQ(static_cast<UINT64>(0),
			WindowAccess::PresentationCommittedFrameCount(window));

		for (unsigned attempt = 0; attempt < 4
			&& WindowAccess::PresentationResourceGeneration(window) == 0;
			++attempt)
		{
			CUI_EXPECT_TRUE(
				::InvalidateRect(window.Handle, nullptr, FALSE) != FALSE);
			(void)::SendMessageW(window.Handle, WM_PAINT, 0, 0);
			cui::PumpUIThreadCallbacks();
		}
		CUI_EXPECT_TRUE(
			WindowAccess::PresentationResourceGeneration(window) > 0);
		CUI_EXPECT_EQ(static_cast<UINT64>(1),
			WindowAccess::PresentationDeviceRecoveryCount(window));
		CUI_EXPECT_TRUE(
			WindowAccess::PresentationCommittedFrameCount(window) > 0);
		RECT lastDirty{};
		bool fullFrame = false;
		CUI_EXPECT_TRUE(WindowAccess::TryGetLastRenderDirtyRect(
			window, lastDirty, fullFrame));
		CUI_EXPECT_TRUE(fullFrame);

		const UINT64 committed =
			WindowAccess::PresentationCommittedFrameCount(window);
		CUI_EXPECT_TRUE(::InvalidateRect(window.Handle, nullptr, FALSE) != FALSE);
		(void)::SendMessageW(window.Handle, WM_PAINT, 0, 0);
		CUI_EXPECT_EQ(static_cast<UINT64>(1),
			WindowAccess::PresentationDeviceRecoveryCount(window));
		CUI_EXPECT_TRUE(
			WindowAccess::PresentationCommittedFrameCount(window) > committed);
	});

	runner.Add("MediaPlayer NV12 fallback preserves odd aperture geometry", []
	{
		constexpr UINT32 width = 4;
		constexpr UINT32 height = 4;
		constexpr UINT32 stride = 4;
		std::vector<uint8_t> nv12(stride * height + stride * 2, 128);
		for (UINT32 row = 0; row < height; ++row)
		{
			for (UINT32 column = 0; column < width; ++column)
				nv12[row * stride + column] = static_cast<uint8_t>(
					32 + row * 24 + column * 4);
		}

		std::vector<uint8_t> bgra;
		CUI_EXPECT_TRUE(MediaPlayer::ConvertNV12ToBGRA(
			nv12.data(), nv12.size(), stride, width, height,
			1, 1, 3, 3, MFVideoTransferMatrix_BT601,
			MFNominalRange_16_235, bgra));
		CUI_EXPECT_EQ(static_cast<size_t>(3 * 3 * 4), bgra.size());
		for (size_t pixel = 0; pixel < 9; ++pixel)
		{
			const size_t offset = pixel * 4;
			CUI_EXPECT_EQ(bgra[offset], bgra[offset + 1]);
			CUI_EXPECT_EQ(bgra[offset + 1], bgra[offset + 2]);
			CUI_EXPECT_EQ(static_cast<uint8_t>(0xFF), bgra[offset + 3]);
		}
		CUI_EXPECT_TRUE(bgra.front() != bgra[8 * 4]);
	});

	runner.Add("MediaPlayer NV12 fallback honors matrix and range", []
	{
		std::vector<uint8_t> nv12{
			100, 100,
			100, 100,
			80, 200 };
		std::vector<uint8_t> bt601Limited;
		std::vector<uint8_t> bt709Limited;
		std::vector<uint8_t> bt601Full;
		CUI_EXPECT_TRUE(MediaPlayer::ConvertNV12ToBGRA(
			nv12.data(), nv12.size(), 2, 2, 2, 0, 0, 2, 2,
			MFVideoTransferMatrix_BT601, MFNominalRange_16_235,
			bt601Limited));
		CUI_EXPECT_TRUE(MediaPlayer::ConvertNV12ToBGRA(
			nv12.data(), nv12.size(), 2, 2, 2, 0, 0, 2, 2,
			MFVideoTransferMatrix_BT709, MFNominalRange_16_235,
			bt709Limited));
		CUI_EXPECT_TRUE(MediaPlayer::ConvertNV12ToBGRA(
			nv12.data(), nv12.size(), 2, 2, 2, 0, 0, 2, 2,
			MFVideoTransferMatrix_BT601, MFNominalRange_0_255,
			bt601Full));
		constexpr size_t expectedBgraBytes = 2 * 2 * 4;
		CUI_EXPECT_EQ(expectedBgraBytes, bt601Limited.size());
		CUI_EXPECT_EQ(expectedBgraBytes, bt709Limited.size());
		CUI_EXPECT_EQ(expectedBgraBytes, bt601Full.size());
		CUI_EXPECT_TRUE(bt601Limited != bt709Limited);
		CUI_EXPECT_TRUE(bt601Limited != bt601Full);

		std::vector<uint8_t> unsupported{ 1, 2, 3 };
		CUI_EXPECT_FALSE(MediaPlayer::ConvertNV12ToBGRA(
			nv12.data(), nv12.size(), 2, 2, 2, 0, 0, 2, 2,
			MFVideoTransferMatrix_BT2020_10,
			MFNominalRange_16_235, unsupported));
		CUI_EXPECT_TRUE(unsupported.empty());
	});

	runner.Add(kSharedAcquireTestName, []
	{
		const bool isIsolatedChild =
			ReadEnvironmentVariable(kSharedAcquireChildVariable) == L"1"
			&& std::wstring_view(::GetCommandLineW()).find(
				kSharedAcquireChildArgument) != std::wstring_view::npos;
		if (!isIsolatedChild)
		{
			const auto executable = CurrentExecutablePath();
			CUI_EXPECT_FALSE(executable.empty());
			bool timedOut = false;
			const std::wstring sharedAcquireFilter =
				WidenAscii(kSharedAcquireTestName);
			const DWORD exitCode = RunFilteredChild(
				executable, kSharedAcquireChildVariable, L"1",
				sharedAcquireFilter.c_str(),
				kSharedAcquireChildArgument, 15'000, timedOut);
			CUI_EXPECT_FALSE(timedOut);
			CUI_EXPECT_EQ(static_cast<DWORD>(0), exitCode);
			return;
		}

		Microsoft::WRL::ComPtr<ID3D11Device> firstDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> firstContext;
		Microsoft::WRL::ComPtr<IDXGIDevice> firstDxgiDevice;
		Microsoft::WRL::ComPtr<ID2D1Device> firstD2DDevice;
		GraphicsSharedD3DDeviceInfo firstInfo{};
		const HRESULT firstResult = Graphics_AcquireSharedD3DDevice(
			firstDevice.GetAddressOf(), firstContext.GetAddressOf(),
			firstDxgiDevice.GetAddressOf(), firstD2DDevice.GetAddressOf(),
			&firstInfo);
		CUI_EXPECT_TRUE(SUCCEEDED(firstResult));
		CUI_EXPECT_TRUE(firstDevice != nullptr);
		CUI_EXPECT_TRUE(firstContext != nullptr);
		CUI_EXPECT_TRUE(firstDxgiDevice != nullptr);
		CUI_EXPECT_TRUE(firstD2DDevice != nullptr);
		CUI_EXPECT_TRUE(firstInfo.Generation > 0);

		Microsoft::WRL::ComPtr<ID3D11Device> secondDevice;
		GraphicsSharedD3DDeviceInfo secondInfo{};
		CUI_EXPECT_TRUE(SUCCEEDED(Graphics_AcquireSharedD3DDevice(
			secondDevice.GetAddressOf(), nullptr, nullptr, nullptr, &secondInfo)));
		CUI_EXPECT_EQ(firstDevice.Get(), secondDevice.Get());
		CUI_EXPECT_EQ(firstInfo.Generation, secondInfo.Generation);

		auto acquireState = std::make_shared<SharedAcquireState>();
		acquireState->ExpectedDevice = firstDevice;
		acquireState->ExpectedContext = firstContext;
		acquireState->ExpectedDxgiDevice = firstDxgiDevice;
		acquireState->ExpectedD2DDevice = firstD2DDevice;
		acquireState->ExpectedInfo = firstInfo;
		HANDLE workers[8]{};
		size_t createdWorkerCount = 0;
		for (; createdWorkerCount < _countof(workers); ++createdWorkerCount)
		{
			auto context = std::make_unique<SharedAcquireThreadContext>();
			context->State = acquireState;
			auto* parameter = context.release();
			workers[createdWorkerCount] = ::CreateThread(
				nullptr, 0, SharedAcquireThreadProc, parameter, 0, nullptr);
			if (!workers[createdWorkerCount])
			{
				delete parameter;
				break;
			}
		}

		const bool allThreadsCreated =
			createdWorkerCount == _countof(workers);
		DWORD concurrentWaitResult = WAIT_FAILED;
		if (createdWorkerCount > 0)
		{
			concurrentWaitResult = ::WaitForMultipleObjects(
				static_cast<DWORD>(createdWorkerCount), workers, TRUE, 10'000);
		}

		// Even when the bounded aggregate wait times out or fails, do not let an
		// assertion unwind this stack while a worker can still access its state.
		// The shared state is independently owned as a final safety net, and each
		// successfully created thread is drained before any expectation executes.
		bool allThreadsDrained = true;
		bool allThreadExitCodesSucceeded = true;
		for (size_t index = 0; index < createdWorkerCount; ++index)
		{
			const DWORD drainResult =
				::WaitForSingleObject(workers[index], INFINITE);
			allThreadsDrained = allThreadsDrained
				&& drainResult == WAIT_OBJECT_0;
			DWORD exitCode = STILL_ACTIVE;
			if (!::GetExitCodeThread(workers[index], &exitCode)
				|| exitCode != ERROR_SUCCESS)
			{
				allThreadExitCodesSucceeded = false;
			}
			(void)::CloseHandle(workers[index]);
			workers[index] = nullptr;
		}

		CUI_EXPECT_TRUE(allThreadsCreated);
		CUI_EXPECT_EQ(
			static_cast<DWORD>(WAIT_OBJECT_0), concurrentWaitResult);
		CUI_EXPECT_TRUE(allThreadsDrained);
		CUI_EXPECT_TRUE(allThreadExitCodesSucceeded);
		CUI_EXPECT_TRUE(
			acquireState->Succeeded.load(std::memory_order_acquire));
	});

	runner.Add("Shared graphics device rotation advances generation and replaces device", []
	{
		Microsoft::WRL::ComPtr<ID3D11Device> rotatedDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> rotatedContext;
		Microsoft::WRL::ComPtr<IDXGIDevice> rotatedDxgiDevice;
		Microsoft::WRL::ComPtr<ID2D1Device> rotatedD2DDevice;
		GraphicsSharedD3DDeviceInfo rotatedInfo{};
		{
			Microsoft::WRL::ComPtr<ID3D11Device> baselineDevice;
			Microsoft::WRL::ComPtr<ID3D11DeviceContext> baselineContext;
			Microsoft::WRL::ComPtr<IDXGIDevice> baselineDxgiDevice;
			Microsoft::WRL::ComPtr<ID2D1Device> baselineD2DDevice;
			GraphicsSharedD3DDeviceInfo baselineInfo{};
			CUI_EXPECT_TRUE(SUCCEEDED(Graphics_AcquireSharedD3DDevice(
				baselineDevice.GetAddressOf(), baselineContext.GetAddressOf(),
				baselineDxgiDevice.GetAddressOf(),
				baselineD2DDevice.GetAddressOf(), &baselineInfo)));
			CUI_EXPECT_TRUE(baselineDevice != nullptr);
			CUI_EXPECT_TRUE(baselineContext != nullptr);
			CUI_EXPECT_TRUE(baselineDxgiDevice != nullptr);
			CUI_EXPECT_TRUE(baselineD2DDevice != nullptr);
			CUI_EXPECT_TRUE(baselineInfo.Generation > 0);

			Microsoft::WRL::ComPtr<ID3D11Device> baselineContextDevice;
			baselineContext->GetDevice(baselineContextDevice.GetAddressOf());
			Microsoft::WRL::ComPtr<IDXGIDevice> baselineQueriedDxgiDevice;
			CUI_EXPECT_TRUE(SUCCEEDED(
				baselineDevice.As(&baselineQueriedDxgiDevice)));
			CUI_EXPECT_TRUE(HasSameComIdentity(
				baselineContextDevice.Get(), baselineDevice.Get()));
			CUI_EXPECT_TRUE(HasSameComIdentity(
				baselineQueriedDxgiDevice.Get(), baselineDxgiDevice.Get()));

			GraphicsSharedD3DDeviceInfo rotationInfo{};
			CUI_EXPECT_TRUE(SUCCEEDED(
				Graphics_RotateSharedD3DDeviceForTesting(&rotationInfo)));
			CUI_EXPECT_TRUE(
				rotationInfo.Generation > baselineInfo.Generation);
			CUI_EXPECT_TRUE(SUCCEEDED(Graphics_AcquireSharedD3DDevice(
				rotatedDevice.GetAddressOf(), rotatedContext.GetAddressOf(),
				rotatedDxgiDevice.GetAddressOf(),
				rotatedD2DDevice.GetAddressOf(), &rotatedInfo)));
			CUI_EXPECT_TRUE(rotatedDevice != nullptr);
			CUI_EXPECT_TRUE(rotatedContext != nullptr);
			CUI_EXPECT_TRUE(rotatedDxgiDevice != nullptr);
			CUI_EXPECT_TRUE(rotatedD2DDevice != nullptr);
			CUI_EXPECT_TRUE(
				rotatedInfo.Generation > baselineInfo.Generation);
			CUI_EXPECT_EQ(rotationInfo.Generation, rotatedInfo.Generation);
			CUI_EXPECT_EQ(rotationInfo.SupportsVideo, rotatedInfo.SupportsVideo);
			CUI_EXPECT_EQ(rotationInfo.IsHardware, rotatedInfo.IsHardware);
			CUI_EXPECT_FALSE(HasSameComIdentity(
				baselineDevice.Get(), rotatedDevice.Get()));

			Microsoft::WRL::ComPtr<ID3D11Device> rotatedContextDevice;
			rotatedContext->GetDevice(rotatedContextDevice.GetAddressOf());
			Microsoft::WRL::ComPtr<IDXGIDevice> rotatedQueriedDxgiDevice;
			CUI_EXPECT_TRUE(SUCCEEDED(
				rotatedDevice.As(&rotatedQueriedDxgiDevice)));
			CUI_EXPECT_TRUE(HasSameComIdentity(
				rotatedContextDevice.Get(), rotatedDevice.Get()));
			CUI_EXPECT_TRUE(HasSameComIdentity(
				rotatedQueriedDxgiDevice.Get(), rotatedDxgiDevice.Get()));
		}

		// The baseline snapshot and all derived COM references are gone before
		// continuing against the rotated registry domain.
		Microsoft::WRL::ComPtr<ID3D11Device> stableDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> stableContext;
		Microsoft::WRL::ComPtr<IDXGIDevice> stableDxgiDevice;
		Microsoft::WRL::ComPtr<ID2D1Device> stableD2DDevice;
		GraphicsSharedD3DDeviceInfo stableInfo{};
		CUI_EXPECT_TRUE(SUCCEEDED(Graphics_AcquireSharedD3DDevice(
			stableDevice.GetAddressOf(), stableContext.GetAddressOf(),
			stableDxgiDevice.GetAddressOf(), stableD2DDevice.GetAddressOf(),
			&stableInfo)));
		CUI_EXPECT_TRUE(stableDevice != nullptr);
		CUI_EXPECT_TRUE(stableContext != nullptr);
		CUI_EXPECT_TRUE(stableDxgiDevice != nullptr);
		CUI_EXPECT_TRUE(stableD2DDevice != nullptr);
		CUI_EXPECT_EQ(rotatedInfo.Generation, stableInfo.Generation);
		CUI_EXPECT_EQ(rotatedInfo.SupportsVideo, stableInfo.SupportsVideo);
		CUI_EXPECT_EQ(rotatedInfo.IsHardware, stableInfo.IsHardware);
		CUI_EXPECT_TRUE(HasSameComIdentity(
			rotatedDevice.Get(), stableDevice.Get()));
		CUI_EXPECT_TRUE(HasSameComIdentity(
			rotatedContext.Get(), stableContext.Get()));
		CUI_EXPECT_TRUE(HasSameComIdentity(
			rotatedDxgiDevice.Get(), stableDxgiDevice.Get()));
		CUI_EXPECT_TRUE(HasSameComIdentity(
			rotatedD2DDevice.Get(), stableD2DDevice.Get()));

		Microsoft::WRL::ComPtr<ID3D11Device> stableContextDevice;
		stableContext->GetDevice(stableContextDevice.GetAddressOf());
		Microsoft::WRL::ComPtr<IDXGIDevice> stableQueriedDxgiDevice;
		CUI_EXPECT_TRUE(SUCCEEDED(stableDevice.As(&stableQueriedDxgiDevice)));
		CUI_EXPECT_TRUE(HasSameComIdentity(
			stableContextDevice.Get(), stableDevice.Get()));
		CUI_EXPECT_TRUE(HasSameComIdentity(
			stableQueriedDxgiDevice.Get(), stableDxgiDevice.Get()));
	});

	runner.Add(kVideoOnlyTestName, []
	{
		const auto childMediaPath = ReadEnvironmentVariable(
			kChildMediaPathVariable);
		if (!childMediaPath.empty())
		{
			Window host;
			auto mediaOwner = std::make_unique<MediaPlayer>();
			auto* mediaPlayer = mediaOwner.get();
			host.SetVisualContent(std::move(mediaOwner));
			mediaPlayer->AutoPlay = false;
			CUI_EXPECT_TRUE(mediaPlayer->Load(childMediaPath));
			CUI_EXPECT_TRUE(mediaPlayer->IsLoaded());
			CUI_EXPECT_TRUE(mediaPlayer->HasVideo);
			CUI_EXPECT_FALSE(mediaPlayer->HasAudio);

			mediaPlayer->Loop = true;
			CUI_EXPECT_TRUE(mediaPlayer->TryPlay());
			const auto firstReadDeadline = std::chrono::steady_clock::now()
				+ std::chrono::seconds(2);
			while (mediaPlayer->GetPerformanceSnapshot().ReadSampleCalls == 0
				&& std::chrono::steady_clock::now() < firstReadDeadline)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			CUI_EXPECT_TRUE(
				mediaPlayer->GetPerformanceSnapshot().ReadSampleCalls > 0);

			const auto pausedSnapshot =
				mediaPlayer->PauseAndGetPerformanceSnapshot();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			const auto stableSnapshot = mediaPlayer->GetPerformanceSnapshot();
			CUI_EXPECT_EQ(
				pausedSnapshot.ReadSampleCalls, stableSnapshot.ReadSampleCalls);
			CUI_EXPECT_EQ(
				pausedSnapshot.DecodedVideoFrames,
				stableSnapshot.DecodedVideoFrames);
			CUI_EXPECT_FALSE(mediaPlayer->IsPlaying());

			// A later explicit command must reopen a quiesced gate.
			CUI_EXPECT_TRUE(mediaPlayer->TryPlay());
			const auto resumedReadDeadline = std::chrono::steady_clock::now()
				+ std::chrono::seconds(2);
			while (mediaPlayer->GetPerformanceSnapshot().ReadSampleCalls
					== stableSnapshot.ReadSampleCalls
				&& std::chrono::steady_clock::now() < resumedReadDeadline)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			CUI_EXPECT_TRUE(
				mediaPlayer->GetPerformanceSnapshot().ReadSampleCalls
					> stableSnapshot.ReadSampleCalls);

			// A playing seek must first quiesce the synchronous SourceReader and
			// then reopen the worker without racing a pending ReadSample.
			const auto beforeSeek = mediaPlayer->GetPerformanceSnapshot();
			CUI_EXPECT_TRUE(mediaPlayer->TrySeek(0.0));
			const auto seekResumeDeadline = std::chrono::steady_clock::now()
				+ std::chrono::seconds(2);
			while (mediaPlayer->GetPerformanceSnapshot().ReadSampleCalls
					== beforeSeek.ReadSampleCalls
				&& std::chrono::steady_clock::now() < seekResumeDeadline)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			CUI_EXPECT_TRUE(
				mediaPlayer->GetPerformanceSnapshot().ReadSampleCalls
					> beforeSeek.ReadSampleCalls);
			CUI_EXPECT_TRUE(mediaPlayer->IsPlaying());
			CUI_EXPECT_FALSE(mediaPlayer->HasMediaError());
			(void)mediaPlayer->PauseAndGetPerformanceSnapshot();
			mediaPlayer->Close();
			return;
		}

		const auto uniqueValue = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		const auto mediaPath = std::filesystem::temp_directory_path()
			/ (L"cui-video-only-" + std::to_wstring(::GetCurrentProcessId())
				+ L"-" + std::to_wstring(uniqueValue) + L".mp4");
		struct TemporaryFile final
		{
			std::filesystem::path Path;
			~TemporaryFile()
			{
				std::error_code error;
				(void)std::filesystem::remove(Path, error);
			}
		} temporaryFile{ mediaPath };

		const HRESULT createResult = CreateVideoOnlyMp4(mediaPath);
		CUI_EXPECT_TRUE(SUCCEEDED(createResult));
		CUI_EXPECT_TRUE(std::filesystem::exists(mediaPath));
		CUI_EXPECT_TRUE(std::filesystem::file_size(mediaPath) > 0);

		const auto executable = CurrentExecutablePath();
		CUI_EXPECT_FALSE(executable.empty());
		bool timedOut = false;
		const DWORD exitCode = RunVideoOnlyLoadChild(
			executable, mediaPath, timedOut);
		CUI_EXPECT_FALSE(timedOut);
		CUI_EXPECT_EQ(static_cast<DWORD>(0), exitCode);
	});
}
