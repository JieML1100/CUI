#pragma once

#include <cstddef>
#include <string>

/**
 * Optional code-behind side of a document-wide handler rename. The command
 * stores only stable identity, path, and signature metadata; source bytes are
 * read and transformed at Execute/Undo/Redo time so unrelated user edits are
 * preserved.
 */
struct DesignerEventHandlerCodeMigration
{
	std::wstring OutputBasePath;
	std::wstring ClassName;
	/** Exact user .h or .cpp containing the unique compatible definition. */
	std::wstring UserCodePath;
	std::string ParameterList;
	std::wstring OldName;
	std::wstring NewName;

	bool Enabled() const noexcept
	{
		return !OutputBasePath.empty() && !ClassName.empty()
			&& !UserCodePath.empty()
			&& !OldName.empty()
			&& !NewName.empty() && OldName != NewName;
	}

	size_t GetEstimatedMemoryUsage() const noexcept;
};
