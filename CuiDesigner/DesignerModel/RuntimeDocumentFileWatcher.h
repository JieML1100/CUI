#pragma once

#include "RuntimeDocument.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace DesignerModel
{
enum class RuntimeDocumentWatchState
{
	Stopped,
	Idle,
	Debouncing,
	Reloaded,
	Unchanged,
	Failed,
};

struct RuntimeDocumentWatchResult
{
	RuntimeDocumentWatchState State = RuntimeDocumentWatchState::Stopped;
	RuntimeDocumentReloadMode ReloadMode = RuntimeDocumentReloadMode::Unchanged;
	bool ReloadAttempted = false;
	std::wstring Error;
};

/**
 * UI-thread-friendly file change detector for RuntimeDocument.
 *
 * The watcher owns no thread and posts no window messages. Hosts call Poll()
 * from their normal UI tick/timer, so parsing, event resolution, and control
 * mutation remain on the host thread. A file signature combines identity,
 * write time, and size, which detects both direct writes and atomic replace.
 */
class RuntimeDocumentFileWatcher final
{
public:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	explicit RuntimeDocumentFileWatcher(
		std::chrono::milliseconds debounce = std::chrono::milliseconds{ 200 });

	bool Start(const std::wstring& filePath, std::wstring* outError = nullptr);
	void Stop() noexcept;
	bool IsWatching() const noexcept { return !_filePath.empty(); }
	const std::wstring& FilePath() const noexcept { return _filePath; }

	std::chrono::milliseconds Debounce() const noexcept { return _debounce; }
	void SetDebounce(std::chrono::milliseconds value) noexcept;
	bool HasPendingChange() const noexcept { return _pending; }
	const std::wstring& LastError() const noexcept { return _lastError; }

	RuntimeDocumentWatchResult Poll(
		RuntimeDocument& document,
		const RuntimeDocumentLoadOptions& options = {});
	RuntimeDocumentWatchResult PollAt(
		RuntimeDocument& document,
		const RuntimeDocumentLoadOptions& options,
		TimePoint now);

	/** Retries the current stable signature after a prior reload failure. */
	void RequestRetry();
	void RequestRetryAt(TimePoint now) noexcept;

private:
	struct FileSignature
	{
		bool Available = false;
		uint32_t ErrorCode = 0;
		uint32_t VolumeSerial = 0;
		uint64_t FileId = 0;
		uint64_t LastWrite = 0;
		uint64_t Size = 0;

		bool operator==(const FileSignature&) const = default;
	};

	std::wstring _filePath;
	std::chrono::milliseconds _debounce;
	std::optional<FileSignature> _observed;
	TimePoint _changedAt{};
	bool _pending = false;
	bool _failed = false;
	std::wstring _lastError;

	static FileSignature Observe(const std::wstring& filePath);
	static std::wstring DescribeObservationFailure(
		const std::wstring& filePath,
		uint32_t errorCode);
};
}
