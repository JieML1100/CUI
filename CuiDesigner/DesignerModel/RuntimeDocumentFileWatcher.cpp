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
	std::wstring result = L"无法读取动态文档文件状态：“" + filePath + L"”";
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
	_observed = signature;
	_changedAt = Clock::now();
	_pending = false;
	_failed = false;
	_lastError.clear();
	if (outError) outError->clear();
	return true;
}

void RuntimeDocumentFileWatcher::Stop() noexcept
{
	_filePath.clear();
	_observed.reset();
	_pending = false;
	_failed = false;
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
	if (!IsWatching() || !_observed)
	{
		result.State = RuntimeDocumentWatchState::Stopped;
		return result;
	}

	const auto signature = Observe(_filePath);
	if (signature != *_observed)
	{
		_observed = signature;
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
	if (!signature.Available)
	{
		_failed = true;
		_lastError = DescribeObservationFailure(_filePath, signature.ErrorCode);
		result.State = RuntimeDocumentWatchState::Failed;
		result.Error = _lastError;
		return result;
	}

	std::wstring error;
	if (!RuntimeDocumentLoader::ReloadFile(
		_filePath, document, options, &result.ReloadMode, &error))
	{
		_failed = true;
		_lastError = std::move(error);
		result.State = RuntimeDocumentWatchState::Failed;
		result.Error = _lastError;
		return result;
	}

	_failed = false;
	_lastError.clear();
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
	if (!IsWatching() || !_observed) return;
	_pending = true;
	_failed = false;
	_lastError.clear();
	_changedAt = now;
}
}
