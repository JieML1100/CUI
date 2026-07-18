#pragma once

#include "DesignDocument.h"
#include <optional>
#include <string>
#include <vector>

namespace DesignerModel
{
struct DesignClipboardPasteResult
{
	std::vector<int> RootIds;
	std::vector<std::wstring> RootNames;
	std::vector<int> NodeIds;
};

/** Destination of one top-level root in a portable clipboard fragment. */
struct DesignClipboardRootTarget
{
	int FragmentRootId = 0;
	// ParentId == 0 and an empty ParentRef means the form root.  A non-empty
	// ParentRef identifies a synthetic parent such as TabControlName#page0.
	int ParentId = 0;
	std::wstring ParentRef;
	// nullopt preserves the fragment root's region (used by Duplicate).
	// "" clears it; "panel1"/"panel2" selects a SplitContainer region.
	std::optional<std::string> SplitRegion = std::string{};
	// Optional zero-based position among the destination's existing children.
	// The index is evaluated before any roots in this paste are inserted.  When
	// several roots use the same index their fragment order is preserved.
	std::optional<int> InsertIndex;
};

/**
 * Pure document-model clipboard operations.
 *
 * A captured fragment is itself a valid DesignDocument, so it can be written
 * as readable CUI XAML and exchanged through the Windows text clipboard. Paste
 * never reuses stable IDs and never mutates either input on failure.
 */
class DesignDocumentClipboard final
{
public:
	static bool Capture(
		const DesignDocument& source,
		const std::vector<int>& selectedNodeIds,
		DesignDocument& fragment,
		std::wstring* outError = nullptr);

	static bool PasteAtRoot(
		const DesignDocument& target,
		const DesignDocument& fragment,
		int offsetX,
		int offsetY,
		DesignDocument& merged,
		DesignClipboardPasteResult* outResult = nullptr,
		std::wstring* outError = nullptr);

	/**
	 * Pastes each fragment root into an explicit destination.  An empty target
	 * list is equivalent to PasteAtRoot.  Every non-empty list must cover every
	 * fragment root exactly once.
	 */
	static bool Paste(
		const DesignDocument& target,
		const DesignDocument& fragment,
		const std::vector<DesignClipboardRootTarget>& rootTargets,
		int offsetX,
		int offsetY,
		DesignDocument& merged,
		DesignClipboardPasteResult* outResult = nullptr,
		std::wstring* outError = nullptr);
};
}
