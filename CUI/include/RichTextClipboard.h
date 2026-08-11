#pragma once

#include "RichTextDocument.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cui::richtext::clipboard
{
/**
 * Clipboard data read in one OpenClipboard transaction. Attributed contains
 * CUI's private portable payload, Rtf is the standard cross-application rich
 * fallback, and PlainText is Unicode (or ANSI when Unicode is unavailable).
 */
struct DataObject
{
	std::optional<std::vector<std::uint8_t>> Attributed;
	/** Standard Rich Text Format fallback for non-CUI applications. */
	std::optional<std::string> Rtf;
	std::optional<std::wstring> PlainText;
};

/** Internal transport seam; production uses Win32 and tests may inject memory. */
class Backend
{
public:
	virtual ~Backend() = default;
	virtual bool Publish(void* owner, const DataObject& data) noexcept = 0;
	virtual std::optional<DataObject> Read(void* owner) noexcept = 0;
	virtual bool CanPaste() noexcept = 0;
};

/** Thread-local, nestable backend override restored automatically on exit. */
class ScopedBackendOverride final
{
public:
	explicit ScopedBackendOverride(Backend& backend) noexcept;
	~ScopedBackendOverride();
	ScopedBackendOverride(const ScopedBackendOverride&) = delete;
	ScopedBackendOverride& operator=(const ScopedBackendOverride&) = delete;

private:
	Backend* _installed = nullptr;
	Backend* _previous = nullptr;
};

/**
 * Rewrites source tree provenance into a detached portable structure. Source
 * IDs/root identity are discarded; effectiveSpans are preserved exactly and
 * terminal local values compensate for the detached formatting baseline.
 */
std::optional<RichTextDocumentFragment> MakePortableStructuredFragment(
	const RichTextDocumentFragment& source,
	std::vector<RichTextStyleSpan> effectiveSpans) noexcept;

/**
 * Imports a decoded portable structure into one target FlowDocument root.
 * Every Paragraph/Inline receives a fresh process-local identity.
 */
std::optional<RichTextDocumentFragment> RebaseStructureForInsertion(
	const RichTextDocumentFragment& source,
	const RichTextCharacterStyle& targetRootStyle,
	const RichTextParagraphStyle& targetRootParagraphStyle,
	std::uint64_t targetRootId) noexcept;

/** Encode portable Text/Spans plus v7 detached Inline structure, empty
 *  structure markers, TextAlignment, FlowDirection, FontStretch, and Language.
 *  Process-local root and node identities are never put on the wire. */
std::optional<std::vector<std::uint8_t>> Encode(
	const RichTextDocumentFragment& fragment) noexcept;

/** Decode v1 attributed text, v2 non-empty structure, v3 structure markers,
 *  v4 alignment, v5 paragraph direction, v6 FontStretch, or v7 Language with
 *  fresh process-local IDs. */
std::optional<RichTextDocumentFragment> Decode(
	const std::vector<std::uint8_t>& bytes) noexcept;

/**
 * Publish CF_UNICODETEXT plus, when encodable, CUI attributed data and RTF in
 * one EmptyClipboard transaction. owner is an optional native HWND.
 */
bool Publish(
	void* owner,
	const RichTextDocumentFragment& fragment) noexcept;

/** Low-level data-object publication seam used by codec/fallback tests. */
bool Publish(void* owner, const DataObject& data) noexcept;

/** Read CUI rich, RTF, plus Unicode/ANSI fallback in one transaction. */
std::optional<DataObject> Read(void* owner) noexcept;

/** True when any supported CUI rich, RTF, Unicode, or ANSI format exists. */
bool CanPaste() noexcept;
}
