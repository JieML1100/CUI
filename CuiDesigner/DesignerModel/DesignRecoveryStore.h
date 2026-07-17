#pragma once

#include "DesignDocument.h"
#include <cstdint>
#include <string>
#include <vector>

namespace DesignerModel
{
struct DesignRecoverySnapshot
{
	uint32_t OwnerProcessId = 0;
	uint64_t OwnerProcessStartTime = 0;
	std::wstring OriginalFilePath;
	DesignDocument Document;
};

struct DesignRecoveryFile
{
	std::wstring Path;
	uint64_t LastWriteTime = 0;
};

/** Versioned, single-file recovery envelope around a normal DesignDocument XML payload. */
class DesignRecoveryStore
{
public:
	static bool SaveToFile(
		const DesignRecoverySnapshot& snapshot,
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
	static bool LoadFromFile(
		const std::wstring& filePath,
		DesignRecoverySnapshot& snapshot,
		std::wstring* outError = nullptr);
	static bool DeleteFile(
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
	static bool QuarantineFile(
		const std::wstring& filePath,
		std::wstring* outQuarantinePath = nullptr,
		std::wstring* outError = nullptr);

	static bool GetDefaultDirectory(
		std::wstring& directory,
		std::wstring* outError = nullptr);
	static std::wstring MakeSessionFilePath(
		const std::wstring& directory,
		uint32_t processId,
		uint64_t processStartTime);
	static bool EnumerateRecoveryFiles(
		const std::wstring& directory,
		std::vector<DesignRecoveryFile>& files,
		std::wstring* outError = nullptr);

	static uint64_t GetCurrentProcessStartTime() noexcept;
	static bool IsOwnerProcessRunning(
		const DesignRecoverySnapshot& snapshot) noexcept;
};
}
