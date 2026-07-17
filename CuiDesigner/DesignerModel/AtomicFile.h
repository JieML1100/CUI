#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace DesignerModel
{
struct AtomicFileWriteEntry
{
	std::wstring FilePath;
	std::string Content;
};

/** Writes a complete sibling temporary file, flushes it, then atomically replaces the target. */
class AtomicFile
{
public:
	static bool Write(
		const std::wstring& filePath,
		std::string_view content,
		std::wstring* outError = nullptr);

	/**
	 * Stages every file before changing any target, then commits the set in
	 * order. A commit failure restores every already-replaced target in reverse
	 * order and removes targets that did not exist before the transaction.
	 * Existing targets whose bytes already match are left untouched, preserving
	 * their timestamps and avoiding unnecessary downstream rebuilds.
	 *
	 * Files may live in different directories, but every temporary/backup file
	 * remains beside its target so each individual rename is volume-local.
	 */
	static bool WriteBatch(
		const std::vector<AtomicFileWriteEntry>& entries,
		std::wstring* outError = nullptr);
};
}
