#include "DesignRecoveryStore.h"
#include "AtomicFile.h"
#include "DesignDocumentSerializer.h"
#include <Convert.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>

namespace DesignerModel
{
namespace
{
	constexpr std::string_view RecoveryHeader =
		"CUI-DESIGNER-RECOVERY-V1\n";
	constexpr uint64_t MaximumRecoveryFileSize = 128ull * 1024ull * 1024ull;

	uint64_t FileTimeValue(const FILETIME& value) noexcept
	{
		return (static_cast<uint64_t>(value.dwHighDateTime) << 32)
			| value.dwLowDateTime;
	}

	std::wstring FormatFileError(
		const std::wstring& prefix,
		DWORD error)
	{
		wchar_t* message = nullptr;
		const DWORD length = ::FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER
				| FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, error, 0,
			reinterpret_cast<wchar_t*>(&message), 0, nullptr);
		std::wstring result = prefix;
		if (length != 0 && message)
		{
			result += L" ";
			result.append(message, length);
			while (!result.empty()
				&& (result.back() == L'\r' || result.back() == L'\n'))
				result.pop_back();
		}
		else
		{
			result += L" (Win32 error " + std::to_wstring(error) + L")";
		}
		if (message) ::LocalFree(message);
		return result;
	}

	bool EnsureDirectory(
		const std::wstring& path,
		std::wstring* outError)
	{
		if (::CreateDirectoryW(path.c_str(), nullptr)) return true;
		const DWORD error = ::GetLastError();
		if (error == ERROR_ALREADY_EXISTS)
		{
			const DWORD attributes = ::GetFileAttributesW(path.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES
				&& (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				return true;
		}
		if (outError) *outError = FormatFileError(
			L"Failed to create recovery directory.", error);
		return false;
	}

	template<typename T>
	bool ParseUnsignedLine(
		std::string_view content,
		size_t& cursor,
		T& value)
	{
		const size_t end = content.find('\n', cursor);
		if (end == std::string_view::npos || end == cursor) return false;
		const char* first = content.data() + cursor;
		const char* last = content.data() + end;
		T parsed{};
		const auto result = std::from_chars(first, last, parsed);
		if (result.ec != std::errc{} || result.ptr != last) return false;
		value = parsed;
		cursor = end + 1;
		return true;
	}

	bool ReadCompleteFile(
		const std::wstring& filePath,
		std::string& content,
		std::wstring* outError)
	{
		HANDLE file = ::CreateFileW(
			filePath.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			if (outError) *outError = FormatFileError(
				L"Failed to open recovery file.", ::GetLastError());
			return false;
		}

		LARGE_INTEGER size{};
		if (!::GetFileSizeEx(file, &size)
			|| size.QuadPart <= 0
			|| static_cast<uint64_t>(size.QuadPart) > MaximumRecoveryFileSize)
		{
			const DWORD error = ::GetLastError();
			(void)::CloseHandle(file);
			if (outError)
			{
				*outError = size.QuadPart > 0
					? L"Recovery file is too large."
					: FormatFileError(L"Recovery file is empty or unreadable.", error);
			}
			return false;
		}

		content.resize(static_cast<size_t>(size.QuadPart));
		size_t offset = 0;
		bool succeeded = true;
		DWORD readError = ERROR_SUCCESS;
		while (offset < content.size())
		{
			const DWORD chunk = static_cast<DWORD>((std::min)(
				content.size() - offset,
				static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
			DWORD read = 0;
			if (!::ReadFile(file, content.data() + offset,
				chunk, &read, nullptr) || read == 0)
			{
				succeeded = false;
				readError = ::GetLastError();
				if (readError == ERROR_SUCCESS) readError = ERROR_HANDLE_EOF;
				break;
			}
			offset += read;
		}
		(void)::CloseHandle(file);
		if (!succeeded)
		{
			content.clear();
			if (outError) *outError = FormatFileError(
				L"Failed to read recovery file.", readError);
			return false;
		}
		return true;
	}
}

bool DesignRecoveryStore::SaveToFile(
	const DesignRecoverySnapshot& snapshot,
	const std::wstring& filePath,
	std::wstring* outError)
{
	try
	{
		if (outError) outError->clear();
		const std::string originalPath =
			Convert::UnicodeToUtf8(snapshot.OriginalFilePath);
		const std::string xml =
			DesignDocumentSerializer::ToXml(snapshot.Document);
		std::string content;
		content.reserve(RecoveryHeader.size() + originalPath.size()
			+ xml.size() + 96);
		content.append(RecoveryHeader);
		content += std::to_string(snapshot.OwnerProcessId) + '\n';
		content += std::to_string(snapshot.OwnerProcessStartTime) + '\n';
		content += std::to_string(originalPath.size()) + '\n';
		content += std::to_string(xml.size()) + '\n';
		content += originalPath;
		content += xml;
		if (content.size() > MaximumRecoveryFileSize)
		{
			if (outError) *outError = L"Recovery file is too large.";
			return false;
		}
		return AtomicFile::Write(filePath, content, outError);
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"Recovery save failed: "
			+ Convert::Utf8ToUnicode(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"Recovery save failed: unknown error.";
		return false;
	}
}

bool DesignRecoveryStore::LoadFromFile(
	const std::wstring& filePath,
	DesignRecoverySnapshot& snapshot,
	std::wstring* outError)
{
	try
	{
		if (outError) outError->clear();
		std::string content;
		if (!ReadCompleteFile(filePath, content, outError)) return false;
		if (!std::string_view(content).starts_with(RecoveryHeader))
		{
			if (outError) *outError = L"Unsupported recovery file header.";
			return false;
		}

		size_t cursor = RecoveryHeader.size();
		uint32_t processId = 0;
		uint64_t processStartTime = 0;
		uint64_t originalPathSize = 0;
		uint64_t xmlSize = 0;
		if (!ParseUnsignedLine(content, cursor, processId)
			|| !ParseUnsignedLine(content, cursor, processStartTime)
			|| !ParseUnsignedLine(content, cursor, originalPathSize)
			|| !ParseUnsignedLine(content, cursor, xmlSize)
			|| originalPathSize > content.size() - cursor
			|| xmlSize > content.size() - cursor - originalPathSize
			|| originalPathSize + xmlSize != content.size() - cursor)
		{
			if (outError) *outError = L"Recovery file envelope is malformed.";
			return false;
		}

		DesignRecoverySnapshot parsed;
		parsed.OwnerProcessId = processId;
		parsed.OwnerProcessStartTime = processStartTime;
		parsed.OriginalFilePath = Convert::Utf8ToUnicode(
			content.substr(cursor, static_cast<size_t>(originalPathSize)));
		cursor += static_cast<size_t>(originalPathSize);
		std::wstring xmlError;
		if (!DesignDocumentSerializer::FromXml(
			content.substr(cursor, static_cast<size_t>(xmlSize)),
			parsed.Document, &xmlError))
		{
			if (outError) *outError = L"Recovery document is invalid: " + xmlError;
			return false;
		}
		snapshot = std::move(parsed);
		return true;
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"Recovery load failed: "
			+ Convert::Utf8ToUnicode(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"Recovery load failed: unknown error.";
		return false;
	}
}

bool DesignRecoveryStore::DeleteFile(
	const std::wstring& filePath,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (filePath.empty()) return true;
	if (::DeleteFileW(filePath.c_str())) return true;
	const DWORD error = ::GetLastError();
	if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
		return true;
	if (outError) *outError = FormatFileError(
		L"Failed to delete recovery file.", error);
	return false;
}

bool DesignRecoveryStore::QuarantineFile(
	const std::wstring& filePath,
	std::wstring* outQuarantinePath,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (outQuarantinePath) outQuarantinePath->clear();
	FILETIME now{};
	::GetSystemTimeAsFileTime(&now);
	const auto suffix = L".corrupt-" + std::to_wstring(FileTimeValue(now));
	for (int attempt = 0; attempt < 32; ++attempt)
	{
		const auto target = filePath + suffix
			+ (attempt == 0 ? std::wstring{}
				: L"-" + std::to_wstring(attempt));
		if (::MoveFileExW(filePath.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
		{
			if (outQuarantinePath) *outQuarantinePath = target;
			return true;
		}
		const DWORD error = ::GetLastError();
		if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
		{
			if (outError) *outError = FormatFileError(
				L"Failed to quarantine recovery file.", error);
			return false;
		}
	}
	if (outError) *outError = L"Failed to choose a unique quarantine path.";
	return false;
}

bool DesignRecoveryStore::GetDefaultDirectory(
	std::wstring& directory,
	std::wstring* outError)
{
	if (outError) outError->clear();
	directory.clear();
	wchar_t localAppData[MAX_PATH]{};
	const HRESULT hr = ::SHGetFolderPathW(
		nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
		nullptr, SHGFP_TYPE_CURRENT, localAppData);
	if (FAILED(hr) || localAppData[0] == L'\0')
	{
		if (outError) *outError = L"Local application data directory is unavailable.";
		return false;
	}

	std::wstring current = localAppData;
	for (const auto* component : { L"CUI", L"Designer", L"Recovery" })
	{
		current += L"\\";
		current += component;
		if (!EnsureDirectory(current, outError)) return false;
	}
	directory = std::move(current);
	return true;
}

std::wstring DesignRecoveryStore::MakeSessionFilePath(
	const std::wstring& directory,
	uint32_t processId,
	uint64_t processStartTime)
{
	if (directory.empty()) return {};
	return directory + L"\\session-" + std::to_wstring(processId)
		+ L"-" + std::to_wstring(processStartTime) + L".cui-recovery";
}

bool DesignRecoveryStore::EnumerateRecoveryFiles(
	const std::wstring& directory,
	std::vector<DesignRecoveryFile>& files,
	std::wstring* outError)
{
	if (outError) outError->clear();
	files.clear();
	if (directory.empty()) return true;
	WIN32_FIND_DATAW data{};
	const auto pattern = directory + L"\\session-*.cui-recovery";
	HANDLE find = ::FindFirstFileW(pattern.c_str(), &data);
	if (find == INVALID_HANDLE_VALUE)
	{
		const DWORD error = ::GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			return true;
		if (outError) *outError = FormatFileError(
			L"Failed to enumerate recovery files.", error);
		return false;
	}
	do
	{
		if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
		files.push_back({
			directory + L"\\" + data.cFileName,
			FileTimeValue(data.ftLastWriteTime)
		});
	} while (::FindNextFileW(find, &data));
	const DWORD error = ::GetLastError();
	(void)::FindClose(find);
	if (error != ERROR_NO_MORE_FILES)
	{
		files.clear();
		if (outError) *outError = FormatFileError(
			L"Failed while enumerating recovery files.", error);
		return false;
	}
	std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
		return left.LastWriteTime > right.LastWriteTime;
	});
	return true;
}

uint64_t DesignRecoveryStore::GetCurrentProcessStartTime() noexcept
{
	FILETIME creation{}, exit{}, kernel{}, user{};
	if (!::GetProcessTimes(
		::GetCurrentProcess(), &creation, &exit, &kernel, &user))
		return 0;
	return FileTimeValue(creation);
}

bool DesignRecoveryStore::IsOwnerProcessRunning(
	const DesignRecoverySnapshot& snapshot) noexcept
{
	if (snapshot.OwnerProcessId == 0
		|| snapshot.OwnerProcessStartTime == 0)
		return false;
	HANDLE process = ::OpenProcess(
		PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
		FALSE, snapshot.OwnerProcessId);
	if (!process) return ::GetLastError() == ERROR_ACCESS_DENIED;
	FILETIME creation{}, exit{}, kernel{}, user{};
	const bool sameProcess = ::GetProcessTimes(
		process, &creation, &exit, &kernel, &user)
		&& FileTimeValue(creation) == snapshot.OwnerProcessStartTime
		&& ::WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
	(void)::CloseHandle(process);
	return sameProcess;
}
}
