#include "RuntimeDocumentFileWatcher.h"

#include <Windows.h>

#include <algorithm>
#include <utility>

namespace DesignerModel
{
namespace
{
	uint64_t Combine(uint32_t high, uint32_t low) noexcept
	{
		return (static_cast<uint64_t>(high) << 32) | low;
	}

	std::wstring FormatWindowsError(uint32_t errorCode)
	{
		wchar_t* buffer = nullptr;
		const DWORD length = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER
				| FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			errorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<wchar_t*>(&buffer),
			0,
			nullptr);
		std::wstring message = length && buffer
			? std::wstring(buffer, length) : std::wstring{};
		if (buffer) LocalFree(buffer);
		while (!message.empty()
			&& (message.back() == L'\r' || message.back() == L'\n'))
			message.pop_back();
		return message;
	}
}

RuntimeDocumentFileWatcher::RuntimeDocumentFileWatcher(
	std::chrono::milliseconds debounce)
	: _debounce((std::max)(debounce, std::chrono::milliseconds::zero()))
{
}

void RuntimeDocumentFileWatcher::SetDebounce(
	std::chrono::milliseconds value) noexcept
{
	_debounce = (std::max)(value, std::chrono::milliseconds::zero());
}

RuntimeDocumentFileWatcher::FileSignature
RuntimeDocumentFileWatcher::Observe(const std::wstring& filePath)
{
	FileSignature result;
	const HANDLE file = CreateFileW(
		filePath.c_str(),
		FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (file == INVALID_HANDLE_VALUE)
	{
		result.ErrorCode = GetLastError();
		return result;
	}

	BY_HANDLE_FILE_INFORMATION information{};
	if (!GetFileInformationByHandle(file, &information))
	{
		result.ErrorCode = GetLastError();
		CloseHandle(file);
		return result;
	}
	CloseHandle(file);

	result.Available = true;
	result.VolumeSerial = information.dwVolumeSerialNumber;
	result.FileId = Combine(information.nFileIndexHigh, information.nFileIndexLow);
	result.LastWrite = Combine(
		information.ftLastWriteTime.dwHighDateTime,
		information.ftLastWriteTime.dwLowDateTime);
	result.Size = Combine(information.nFileSizeHigh, information.nFileSizeLow);
	return result;
}

std::wstring RuntimeDocumentFileWatcher::DescribeObservationFailure(
	const std::wstring& filePath,
	uint32_t errorCode)
{
	auto message = FormatWindowsError(errorCode);
	std::wstring result = L"无法读取动态文档或资源依赖状态：“" + filePath + L"”";
	if (!message.empty()) result += L"：" + message;
	result += L"（错误 " + std::to_wstring(errorCode) + L"）";
	return result;
}

bool RuntimeDocumentFileWatcher::Start(
	const std::wstring& filePath,
	std::wstring* outError)
{
	if (filePath.empty())
	{
		if (outError) *outError = L"文件监视路径不能为空。";
		return false;
	}
	const auto signature = Observe(filePath);
	if (!signature.Available)
	{
		if (outError)
			*outError = DescribeObservationFailure(filePath, signature.ErrorCode);
		return false;
	}

	_filePath = filePath;
	_watchedFiles = { WatchedFile{ filePath, signature } };
	_changedAt = Clock::now();
	_pending = false;
	_failed = false;
	_resourceChangePending = false;
	_lastError.clear();
	if (outError) outError->clear();
	return true;
}

void RuntimeDocumentFileWatcher::Stop() noexcept
{
	_filePath.clear();
	_watchedFiles.clear();
	_pending = false;
	_failed = false;
	_resourceChangePending = false;
	_lastError.clear();
}

RuntimeDocumentWatchResult RuntimeDocumentFileWatcher::Poll(
	RuntimeDocument& document,
	const RuntimeDocumentLoadOptions& options)
{
	return PollAt(document, options, Clock::now());
}

RuntimeDocumentWatchResult RuntimeDocumentFileWatcher::PollAt(
	RuntimeDocument& document,
	const RuntimeDocumentLoadOptions& options,
	TimePoint now)
{
	RuntimeDocumentWatchResult result;
	if (!IsWatching() || _watchedFiles.empty())
	{
		result.State = RuntimeDocumentWatchState::Stopped;
		return result;
	}

	SyncDocumentDependencies(document);
	bool changed = false;
	for (auto& watched : _watchedFiles)
	{
		const auto signature = Observe(watched.Path);
		if (signature == watched.Signature) continue;
		watched.Signature = signature;
		if (_wcsicmp(watched.Path.c_str(), _filePath.c_str()) != 0)
			_resourceChangePending = true;
		changed = true;
	}
	if (changed)
	{
		_changedAt = now;
		_pending = true;
		_failed = false;
		_lastError.clear();
		result.State = RuntimeDocumentWatchState::Debouncing;
		return result;
	}

	if (_failed)
	{
		result.State = RuntimeDocumentWatchState::Failed;
		result.Error = _lastError;
		return result;
	}
	if (!_pending)
	{
		result.State = RuntimeDocumentWatchState::Idle;
		return result;
	}
	if (now - _changedAt < _debounce)
	{
		result.State = RuntimeDocumentWatchState::Debouncing;
		return result;
	}

	_pending = false;
	result.ReloadAttempted = true;
	const auto unavailable = std::find_if(
		_watchedFiles.begin(), _watchedFiles.end(),
		[](const auto& watched) { return !watched.Signature.Available; });
	if (unavailable != _watchedFiles.end())
	{
		_failed = true;
		_lastError = DescribeObservationFailure(
			unavailable->Path, unavailable->Signature.ErrorCode);
		result.State = RuntimeDocumentWatchState::Failed;
		result.Error = _lastError;
		return result;
	}

	std::wstring error;
	auto reloadOptions = options;
	reloadOptions.ForceResourceRefresh =
		reloadOptions.ForceResourceRefresh || _resourceChangePending;
	if (!RuntimeDocumentLoader::ReloadFile(
		_filePath, document, reloadOptions, &result.ReloadMode, &error))
	{
		_failed = true;
		_lastError = std::move(error);
		result.State = RuntimeDocumentWatchState::Failed;
		result.Error = _lastError;
		return result;
	}

	_failed = false;
	_resourceChangePending = false;
	_lastError.clear();
	SyncDocumentDependencies(document);
	result.State = result.ReloadMode == RuntimeDocumentReloadMode::Unchanged
		? RuntimeDocumentWatchState::Unchanged
		: RuntimeDocumentWatchState::Reloaded;
	return result;
}

void RuntimeDocumentFileWatcher::RequestRetry()
{
	RequestRetryAt(Clock::now());
}

void RuntimeDocumentFileWatcher::RequestRetryAt(TimePoint now) noexcept
{
	if (!IsWatching() || _watchedFiles.empty()) return;
	_pending = true;
	_failed = false;
	_lastError.clear();
	_changedAt = now;
}

void RuntimeDocumentFileWatcher::SetDependencies(
	const std::vector<ResourceDependency>& dependencies)
{
	if (!IsWatching()) return;
	std::vector<std::wstring> paths{ _filePath };
	for (const auto& dependency : dependencies)
	{
		if (dependency.WatchPath.empty()) continue;
		if (std::none_of(paths.begin(), paths.end(),
			[&](const auto& current)
			{
				return _wcsicmp(current.c_str(), dependency.WatchPath.c_str()) == 0;
			}))
			paths.push_back(dependency.WatchPath);
	}

	std::vector<WatchedFile> next;
	next.reserve(paths.size());
	for (auto& path : paths)
	{
		const auto existing = std::find_if(
			_watchedFiles.begin(), _watchedFiles.end(),
			[&](const auto& watched)
			{
				return _wcsicmp(watched.Path.c_str(), path.c_str()) == 0;
			});
		next.push_back(existing == _watchedFiles.end()
			? WatchedFile{ path, Observe(path) }
			: *existing);
	}
	_watchedFiles = std::move(next);
}

std::vector<std::wstring> RuntimeDocumentFileWatcher::WatchedFiles() const
{
	std::vector<std::wstring> result;
	result.reserve(_watchedFiles.size());
	for (const auto& watched : _watchedFiles) result.push_back(watched.Path);
	return result;
}

void RuntimeDocumentFileWatcher::SyncDocumentDependencies(
	const RuntimeDocument& document)
{
	SetDependencies(document.ResourceDependencies());
}
}
