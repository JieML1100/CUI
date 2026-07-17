#include "AtomicFile.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <limits>
#include <cwctype>
#include <vector>

namespace DesignerModel
{
namespace
{
	std::atomic_uint64_t nextTemporaryId{ 0 };

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

	std::wstring MakeSiblingPath(
		const std::wstring& filePath,
		const wchar_t* role,
		const wchar_t* extension)
	{
		const auto id = ++nextTemporaryId;
		return filePath
			+ L".~cui-" + role + L"-"
			+ std::to_wstring(::GetCurrentProcessId())
			+ L"-" + std::to_wstring(::GetTickCount64())
			+ L"-" + std::to_wstring(id) + extension;
	}

	bool WriteSiblingTemporary(
		const std::wstring& filePath,
		std::string_view content,
		const wchar_t* role,
		std::wstring& temporaryPath,
		std::wstring& error)
	{
		HANDLE file = INVALID_HANDLE_VALUE;
		DWORD createError = ERROR_SUCCESS;
		for (int attempt = 0; attempt < 64; ++attempt)
		{
			temporaryPath = MakeSiblingPath(
				filePath, role, L".tmp");
			file = ::CreateFileW(
				temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
				nullptr);
			if (file != INVALID_HANDLE_VALUE) break;
			createError = ::GetLastError();
			if (createError != ERROR_FILE_EXISTS
				&& createError != ERROR_ALREADY_EXISTS) break;
		}
		if (file == INVALID_HANDLE_VALUE)
		{
			error = FormatFileError(
				L"Failed to create temporary file for “"
				+ filePath + L"”.", createError);
			return false;
		}

		bool succeeded = true;
		DWORD writeError = ERROR_SUCCESS;
		size_t offset = 0;
		while (offset < content.size())
		{
			const DWORD chunk = static_cast<DWORD>((std::min)(
				content.size() - offset,
				static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
			DWORD written = 0;
			if (!::WriteFile(file, content.data() + offset,
				chunk, &written, nullptr) || written != chunk)
			{
				succeeded = false;
				writeError = ::GetLastError();
				if (writeError == ERROR_SUCCESS) writeError = ERROR_WRITE_FAULT;
				break;
			}
			offset += written;
		}
		if (succeeded && !::FlushFileBuffers(file))
		{
			succeeded = false;
			writeError = ::GetLastError();
		}
		if (!::CloseHandle(file) && succeeded)
		{
			succeeded = false;
			writeError = ::GetLastError();
		}

		if (!succeeded)
		{
			(void)::DeleteFileW(temporaryPath.c_str());
			temporaryPath.clear();
			error = FormatFileError(
				L"Failed to write temporary file for “"
				+ filePath + L"”.", writeError);
			return false;
		}
		return true;
	}

	std::wstring NormalizePathForComparison(const std::wstring& value)
	{
		auto normalized = std::filesystem::absolute(
			std::filesystem::path(value)).lexically_normal().wstring();
		std::transform(normalized.begin(), normalized.end(), normalized.begin(),
			[](wchar_t character)
			{
				return static_cast<wchar_t>(std::towlower(character));
			});
		return normalized;
	}

	void AppendFailure(std::wstring& target, std::wstring value)
	{
		if (value.empty()) return;
		if (!target.empty()) target += L"\n";
		target += std::move(value);
	}

	bool FileContentMatches(
		const std::wstring& filePath,
		std::string_view content) noexcept
	{
		const HANDLE file = ::CreateFileW(
			filePath.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (file == INVALID_HANDLE_VALUE) return false;

		LARGE_INTEGER size{};
		bool matches = ::GetFileSizeEx(file, &size) != FALSE
			&& size.QuadPart >= 0
			&& static_cast<unsigned long long>(size.QuadPart)
				== static_cast<unsigned long long>(content.size());
		size_t offset = 0;
		while (matches && offset < content.size())
		{
			const DWORD chunk = static_cast<DWORD>((std::min)(
				content.size() - offset, static_cast<size_t>(64 * 1024)));
			char buffer[64 * 1024];
			DWORD read = 0;
			if (!::ReadFile(file, buffer, chunk, &read, nullptr)
				|| read != chunk
				|| !std::equal(buffer, buffer + read, content.data() + offset))
			{
				matches = false;
				break;
			}
			offset += read;
		}
		(void)::CloseHandle(file);
		return matches;
	}

	bool ReadFileSnapshot(
		const std::wstring& filePath,
		AtomicFileSnapshotEntry& entry,
		std::wstring& error)
	{
		entry = {};
		entry.FilePath = filePath;
		const DWORD attributes = ::GetFileAttributesW(filePath.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			const auto attributeError = ::GetLastError();
			if (attributeError == ERROR_FILE_NOT_FOUND
				|| attributeError == ERROR_PATH_NOT_FOUND)
				return true;
			error = FormatFileError(
				L"Failed to inspect snapshot target “" + filePath + L"”.",
				attributeError);
			return false;
		}
		if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			error = L"Snapshot target is a directory: " + filePath;
			return false;
		}

		const HANDLE file = ::CreateFileW(
			filePath.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			error = FormatFileError(
				L"Failed to open snapshot target “" + filePath + L"”.",
				::GetLastError());
			return false;
		}

		LARGE_INTEGER size{};
		const bool sizeRead = ::GetFileSizeEx(file, &size) != FALSE;
		DWORD readError = sizeRead ? ERROR_SUCCESS : ::GetLastError();
		bool succeeded = sizeRead && size.QuadPart >= 0
			&& static_cast<unsigned long long>(size.QuadPart)
				<= static_cast<unsigned long long>(
					(std::numeric_limits<size_t>::max)());
		if (sizeRead && !succeeded) readError = ERROR_FILE_TOO_LARGE;
		if (succeeded)
		{
			try
			{
				entry.Content.resize(static_cast<size_t>(size.QuadPart));
			}
			catch (...)
			{
				succeeded = false;
				readError = ERROR_NOT_ENOUGH_MEMORY;
			}
		}
		size_t offset = 0;
		while (succeeded && offset < entry.Content.size())
		{
			const DWORD chunk = static_cast<DWORD>((std::min)(
				entry.Content.size() - offset,
				static_cast<size_t>(64 * 1024)));
			DWORD read = 0;
			if (!::ReadFile(file, entry.Content.data() + offset,
				chunk, &read, nullptr) || read != chunk)
			{
				succeeded = false;
				readError = ::GetLastError();
				if (readError == ERROR_SUCCESS) readError = ERROR_READ_FAULT;
				break;
			}
			offset += read;
		}
		(void)::CloseHandle(file);
		if (!succeeded)
		{
			entry.Content.clear();
			error = FormatFileError(
				L"Failed to read snapshot target “" + filePath + L"”.",
				readError);
			return false;
		}
		entry.Existed = true;
		return true;
	}
}

bool AtomicFile::Write(
	const std::wstring& filePath,
	std::string_view content,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (filePath.empty())
	{
		if (outError) *outError = L"File path is empty.";
		return false;
	}

	std::wstring temporaryPath;
	std::wstring error;
	if (!WriteSiblingTemporary(
		filePath, content, L"write", temporaryPath, error))
	{
		if (outError) *outError = std::move(error);
		return false;
	}

	if (!::MoveFileExW(
		temporaryPath.c_str(), filePath.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		const DWORD replaceError = ::GetLastError();
		(void)::DeleteFileW(temporaryPath.c_str());
		if (outError) *outError = FormatFileError(
			L"Failed to replace file.", replaceError);
		return false;
	}
	return true;
}

bool AtomicFile::WriteBatch(
	const std::vector<AtomicFileWriteEntry>& entries,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (entries.empty()) return true;

	struct State
	{
		const AtomicFileWriteEntry* Entry = nullptr;
		std::wstring TemporaryPath;
		std::wstring BackupPath;
		bool Existed = false;
		bool Unchanged = false;
		bool BackupCreated = false;
		bool NewInstalled = false;
	};

	std::vector<State> states;
	try
	{
		states.reserve(entries.size());
		std::vector<std::wstring> normalizedPaths;
		normalizedPaths.reserve(entries.size());
		for (const auto& entry : entries)
		{
			if (entry.FilePath.empty())
			{
				if (outError) *outError = L"Batch file path is empty.";
				return false;
			}
			auto normalized = NormalizePathForComparison(entry.FilePath);
			if (std::find(normalizedPaths.begin(), normalizedPaths.end(),
				normalized) != normalizedPaths.end())
			{
				if (outError) *outError =
					L"Batch contains a duplicate target: " + entry.FilePath;
				return false;
			}
			normalizedPaths.push_back(std::move(normalized));

			bool existed = false;
			bool unchanged = false;
			if (entry.RequireExpectedState)
			{
				AtomicFileSnapshotEntry current;
				std::wstring snapshotError;
				if (!ReadFileSnapshot(entry.FilePath, current, snapshotError))
				{
					if (outError) *outError = std::move(snapshotError);
					return false;
				}
				if (current.Existed != entry.ExpectedExisted
					|| (current.Existed
						&& current.Content != entry.ExpectedContent))
				{
					if (outError) *outError =
						L"Batch target changed after its plan was created: "
						+ entry.FilePath;
					return false;
				}
				existed = current.Existed;
				unchanged = entry.RemoveTarget
					? !existed
					: existed && current.Content == entry.Content;
			}
			else
			{
				const DWORD attributes =
					::GetFileAttributesW(entry.FilePath.c_str());
				existed = attributes != INVALID_FILE_ATTRIBUTES;
				if (!existed)
				{
					const auto attributeError = ::GetLastError();
					if (attributeError != ERROR_FILE_NOT_FOUND
						&& attributeError != ERROR_PATH_NOT_FOUND)
					{
						if (outError) *outError = FormatFileError(
							L"Failed to inspect batch target “"
							+ entry.FilePath + L"”.", attributeError);
						return false;
					}
				}
				else if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					if (outError) *outError =
						L"Batch target is a directory: " + entry.FilePath;
					return false;
				}
				unchanged = entry.RemoveTarget
					? !existed
					: existed && FileContentMatches(
						entry.FilePath, entry.Content);
			}
			states.push_back(State{
				&entry, {}, {}, existed, unchanged });
		}
	}
	catch (...)
	{
		if (outError) *outError = L"Failed to prepare atomic file batch.";
		return false;
	}

	auto cleanupTemporaries = [&]() noexcept
	{
		for (auto& state : states)
			if (!state.TemporaryPath.empty())
			{
				(void)::DeleteFileW(state.TemporaryPath.c_str());
				state.TemporaryPath.clear();
			}
	};

	std::wstring failure;
	try
	{
		for (auto& state : states)
		{
			if (state.Unchanged || state.Entry->RemoveTarget) continue;
			if (!WriteSiblingTemporary(
				state.Entry->FilePath,
				state.Entry->Content,
				L"batch-new",
				state.TemporaryPath,
				failure))
			{
				cleanupTemporaries();
				if (outError) *outError = std::move(failure);
				return false;
			}
		}
	}
	catch (...)
	{
		cleanupTemporaries();
		if (outError) *outError = L"Failed while staging atomic file batch.";
		return false;
	}

	auto rollback = [&]() noexcept
	{
		std::wstring rollbackError;
		auto recordFailure = [&rollbackError](
			const wchar_t* prefix,
			const std::wstring& path,
			DWORD error) noexcept
		{
			try
			{
				AppendFailure(rollbackError, FormatFileError(
					std::wstring(prefix) + path + L"”.", error));
			}
			catch (...) {}
		};
		for (auto it = states.rbegin(); it != states.rend(); ++it)
		{
			auto& state = *it;
			if (state.NewInstalled)
			{
				if (!::DeleteFileW(state.Entry->FilePath.c_str()))
				{
					const auto error = ::GetLastError();
					if (error != ERROR_FILE_NOT_FOUND)
						recordFailure(
							L"Failed to remove rolled-back target “",
							state.Entry->FilePath, error);
				}
				state.NewInstalled = false;
			}
			if (state.BackupCreated)
			{
				if (!::MoveFileExW(
					state.BackupPath.c_str(),
					state.Entry->FilePath.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					recordFailure(
						L"Failed to restore batch backup “",
						state.BackupPath, ::GetLastError());
				}
				else
				{
					state.BackupCreated = false;
					state.BackupPath.clear();
				}
			}
		}
		cleanupTemporaries();
		return rollbackError;
	};

	try
	{
		for (auto& state : states)
		{
			if (state.Entry->RequireExpectedState)
			{
				AtomicFileSnapshotEntry current;
				std::wstring snapshotError;
				if (!ReadFileSnapshot(
					state.Entry->FilePath, current, snapshotError))
				{
					failure = std::move(snapshotError);
					const auto rollbackError = rollback();
					AppendFailure(failure, rollbackError);
					if (outError) *outError = std::move(failure);
					return false;
				}
				if (current.Existed != state.Entry->ExpectedExisted
					|| (current.Existed
						&& current.Content != state.Entry->ExpectedContent))
				{
					failure =
						L"Batch target changed while the plan was being committed: "
						+ state.Entry->FilePath;
					const auto rollbackError = rollback();
					AppendFailure(failure, rollbackError);
					if (outError) *outError = std::move(failure);
					return false;
				}
			}
			if (state.Unchanged) continue;
			if (state.Existed)
			{
				DWORD backupError = ERROR_ALREADY_EXISTS;
				for (int attempt = 0; attempt < 64; ++attempt)
				{
					state.BackupPath = MakeSiblingPath(
						state.Entry->FilePath, L"batch-old", L".bak");
					if (::MoveFileExW(
						state.Entry->FilePath.c_str(),
						state.BackupPath.c_str(),
						MOVEFILE_WRITE_THROUGH))
					{
						state.BackupCreated = true;
						break;
					}
					backupError = ::GetLastError();
					if (backupError != ERROR_FILE_EXISTS
						&& backupError != ERROR_ALREADY_EXISTS) break;
				}
				if (!state.BackupCreated)
				{
					failure = FormatFileError(
						L"Failed to preserve batch target “"
						+ state.Entry->FilePath + L"”.", backupError);
					const auto rollbackError = rollback();
					AppendFailure(failure, rollbackError);
					if (outError) *outError = std::move(failure);
					return false;
				}
				if (state.Entry->RequireExpectedState
					&& state.Entry->ExpectedExisted
					&& !FileContentMatches(
						state.BackupPath,
						state.Entry->ExpectedContent))
				{
					failure =
						L"Batch target changed during atomic preservation: "
						+ state.Entry->FilePath;
					const auto rollbackError = rollback();
					AppendFailure(failure, rollbackError);
					if (outError) *outError = std::move(failure);
					return false;
				}
			}

			if (state.Entry->RemoveTarget) continue;

			if (!::MoveFileExW(
				state.TemporaryPath.c_str(),
				state.Entry->FilePath.c_str(),
				MOVEFILE_WRITE_THROUGH))
			{
				failure = FormatFileError(
					L"Failed to install batch target “"
					+ state.Entry->FilePath + L"”.", ::GetLastError());
				const auto rollbackError = rollback();
				AppendFailure(failure, rollbackError);
				if (outError) *outError = std::move(failure);
				return false;
			}
			state.TemporaryPath.clear();
			state.NewInstalled = true;
		}

		for (auto& state : states)
			if (state.BackupCreated)
			{
				(void)::DeleteFileW(state.BackupPath.c_str());
				state.BackupCreated = false;
				state.BackupPath.clear();
			}
		return true;
	}
	catch (...)
	{
		const auto rollbackError = rollback();
		try
		{
			failure = L"Unexpected failure while committing atomic file batch.";
			AppendFailure(failure, rollbackError);
			if (outError) *outError = std::move(failure);
		}
		catch (...) {}
		return false;
	}
}

bool AtomicFileBatchSnapshot::Capture(
	const std::vector<std::wstring>& filePaths,
	AtomicFileBatchSnapshot& snapshot,
	std::wstring* outError)
{
	snapshot = {};
	if (outError) outError->clear();
	try
	{
		AtomicFileBatchSnapshot candidate;
		candidate._entries.reserve(filePaths.size());
		std::vector<std::wstring> normalizedPaths;
		normalizedPaths.reserve(filePaths.size());
		for (const auto& filePath : filePaths)
		{
			if (filePath.empty())
			{
				if (outError) *outError = L"Snapshot file path is empty.";
				return false;
			}
			auto normalized = NormalizePathForComparison(filePath);
			if (std::find(normalizedPaths.begin(), normalizedPaths.end(),
				normalized) != normalizedPaths.end())
			{
				if (outError) *outError =
					L"Snapshot contains a duplicate target: " + filePath;
				return false;
			}
			normalizedPaths.push_back(std::move(normalized));

			AtomicFileSnapshotEntry entry;
			std::wstring error;
			if (!ReadFileSnapshot(filePath, entry, error))
			{
				if (outError) *outError = std::move(error);
				return false;
			}
			candidate._entries.push_back(std::move(entry));
		}
		snapshot = std::move(candidate);
		return true;
	}
	catch (...)
	{
		if (outError) *outError = L"Failed to capture atomic file batch snapshot.";
		return false;
	}
}

bool AtomicFileBatchSnapshot::Restore(std::wstring* outError) const
{
	if (outError) outError->clear();
	try
	{
		std::vector<AtomicFileWriteEntry> writes;
		writes.reserve(_entries.size());
		for (const auto& entry : _entries)
			writes.push_back({
				entry.FilePath, entry.Content, !entry.Existed });
		return AtomicFile::WriteBatch(writes, outError);
	}
	catch (...)
	{
		if (outError) *outError = L"Failed to restore atomic file batch snapshot.";
		return false;
	}
}

bool AtomicFileBatchSnapshot::RestoreIfCurrentMatches(
	const AtomicFileBatchSnapshot& expectedCurrent,
	std::wstring* outError) const
{
	if (outError) outError->clear();
	try
	{
		if (_entries.size() != expectedCurrent._entries.size())
		{
			if (outError) *outError =
				L"Conditional snapshot restore target count does not match.";
			return false;
		}
		std::vector<AtomicFileWriteEntry> writes;
		writes.reserve(_entries.size());
		for (const auto& original : _entries)
		{
			const auto normalized =
				NormalizePathForComparison(original.FilePath);
			const auto expected = std::find_if(
				expectedCurrent._entries.begin(),
				expectedCurrent._entries.end(),
				[&](const auto& entry)
				{
					return NormalizePathForComparison(entry.FilePath)
						== normalized;
				});
			if (expected == expectedCurrent._entries.end())
			{
				if (outError) *outError =
					L"Conditional snapshot restore is missing target: "
					+ original.FilePath;
				return false;
			}
			AtomicFileWriteEntry write;
			write.FilePath = original.FilePath;
			write.Content = original.Content;
			write.RemoveTarget = !original.Existed;
			write.RequireExpectedState = true;
			write.ExpectedExisted = expected->Existed;
			write.ExpectedContent = expected->Content;
			writes.push_back(std::move(write));
		}
		return AtomicFile::WriteBatch(writes, outError);
	}
	catch (...)
	{
		if (outError) *outError =
			L"Failed to prepare conditional atomic snapshot restore.";
		return false;
	}
}
}
