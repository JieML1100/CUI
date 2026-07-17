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
	/** When true, the target is removed transactionally and Content is ignored. */
	bool RemoveTarget = false;
	/**
	 * When true, the batch commits only if the target still has the captured
	 * existence and bytes. This protects a plan from overwriting an external
	 * editor change made after the plan was built.
	 */
	bool RequireExpectedState = false;
	bool ExpectedExisted = false;
	std::string ExpectedContent;
};

struct AtomicFileSnapshotEntry
{
	std::wstring FilePath;
	bool Existed = false;
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
	 * Entries marked RemoveTarget participate in the same backup/rollback
	 * transaction and are absent only after the whole batch commits.
	 * Entries with RequireExpectedState are checked before staging, immediately
	 * before their mutation, and again through the preserved backup. A mismatch
	 * aborts the whole batch instead of overwriting newer external content.
	 *
	 * Files may live in different directories, but every temporary/backup file
	 * remains beside its target so each individual rename is volume-local.
	 */
	static bool WriteBatch(
		const std::vector<AtomicFileWriteEntry>& entries,
		std::wstring* outError = nullptr);
};

/** Exact existence/content snapshot that can restore a code-generation file set. */
class AtomicFileBatchSnapshot final
{
public:
	static bool Capture(
		const std::vector<std::wstring>& filePaths,
		AtomicFileBatchSnapshot& snapshot,
		std::wstring* outError = nullptr);

	bool Restore(std::wstring* outError = nullptr) const;
	/**
	 * Restores only when every target still matches expectedCurrent. This is
	 * the rollback form for workflows that must not overwrite edits made after
	 * their own successful file commit.
	 */
	bool RestoreIfCurrentMatches(
		const AtomicFileBatchSnapshot& expectedCurrent,
		std::wstring* outError = nullptr) const;
	[[nodiscard]] bool Empty() const noexcept { return _entries.empty(); }
	[[nodiscard]] const std::vector<AtomicFileSnapshotEntry>& Entries() const noexcept
	{
		return _entries;
	}

private:
	std::vector<AtomicFileSnapshotEntry> _entries;
};
}
