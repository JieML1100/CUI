#pragma once

#include "Brush.h"
#include "FlowDirection.h"
#include "TextAlignment.h"

#include <dwrite.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * WPF XmlLanguage-compatible RFC 3066 subset used by rich text. Tags are
 * ASCII, use 1-8 character subtags, and are canonicalized to lower case.
 * The empty tag is valid and represents the invariant language.
 */
bool IsCanonicalRichTextLanguageTag(std::wstring_view value) noexcept;
std::optional<std::wstring> NormalizeRichTextLanguageTag(
	std::wstring_view value);

/** Which legal text-element boundary an invalid UTF-16 position prefers. */
enum class RichTextBoundaryAffinity
{
	Backward,
	Forward
};

/** A direct character-format override. Empty fields inherit from the host. */
struct RichTextCharacterStyle
{
	std::optional<cui::drawing::Brush> Foreground;
	std::optional<cui::drawing::Brush> Background;
	std::optional<std::wstring> FontFamily;
	std::optional<std::wstring> Language;
	std::optional<float> FontSize;
	std::optional<DWRITE_FONT_WEIGHT> FontWeight;
	std::optional<DWRITE_FONT_STRETCH> FontStretch;
	std::optional<DWRITE_FONT_STYLE> FontStyle;
	std::optional<bool> Underline;
	std::optional<bool> Strikethrough;

	bool operator==(const RichTextCharacterStyle&) const = default;
	bool Validate() const noexcept;
};

/** Direct paragraph-format overrides. Empty fields inherit from the document. */
struct RichTextParagraphStyle
{
	std::optional<::TextAlignment> TextAlignment;
	std::optional<::FlowDirection> FlowDirection;

	bool operator==(const RichTextParagraphStyle&) const = default;
	bool Validate() const noexcept;
};

enum class RichTextFormatDeltaMode
{
	Keep,
	Set,
	Clear
};

/**
 * A three-state format update: Keep leaves the field untouched, Set installs
 * Value, and Clear removes the direct override so the field inherits again.
 */
template<typename T>
struct RichTextFormatChange
{
	RichTextFormatDeltaMode Mode = RichTextFormatDeltaMode::Keep;
	std::optional<T> Value;

	static RichTextFormatChange Keep()
	{
		return {};
	}

	static RichTextFormatChange Set(T value)
	{
		RichTextFormatChange result;
		result.Mode = RichTextFormatDeltaMode::Set;
		result.Value = std::move(value);
		return result;
	}

	static RichTextFormatChange Clear()
	{
		RichTextFormatChange result;
		result.Mode = RichTextFormatDeltaMode::Clear;
		return result;
	}

	bool Validate() const noexcept
	{
		return Mode == RichTextFormatDeltaMode::Set
			? Value.has_value()
			: !Value.has_value();
	}

	bool operator==(const RichTextFormatChange&) const = default;
};

struct RichTextFormatDelta
{
	RichTextFormatChange<cui::drawing::Brush> Foreground;
	RichTextFormatChange<cui::drawing::Brush> Background;
	RichTextFormatChange<std::wstring> FontFamily;
	RichTextFormatChange<std::wstring> Language;
	RichTextFormatChange<float> FontSize;
	RichTextFormatChange<DWRITE_FONT_WEIGHT> FontWeight;
	RichTextFormatChange<DWRITE_FONT_STRETCH> FontStretch;
	RichTextFormatChange<DWRITE_FONT_STYLE> FontStyle;
	RichTextFormatChange<bool> Underline;
	RichTextFormatChange<bool> Strikethrough;

	bool Empty() const noexcept;
	bool Validate() const noexcept;
	RichTextCharacterStyle ApplyTo(
		const RichTextCharacterStyle& source) const;
	bool operator==(const RichTextFormatDelta&) const = default;
};

struct RichTextParagraphFormatDelta
{
	RichTextFormatChange<::TextAlignment> TextAlignment;
	RichTextFormatChange<::FlowDirection> FlowDirection;

	bool Empty() const noexcept;
	bool Validate() const noexcept;
	RichTextParagraphStyle ApplyTo(
		const RichTextParagraphStyle& source) const;
	bool operator==(const RichTextParagraphFormatDelta&) const = default;
};

struct RichTextRange
{
	std::size_t Start = 0;
	std::size_t Length = 0;

	std::size_t End() const noexcept { return Start + Length; }
	bool Empty() const noexcept { return Length == 0; }
	bool operator==(const RichTextRange&) const = default;
};

struct RichTextStyleSpan
{
	std::size_t Start = 0;
	std::size_t Length = 0;
	RichTextCharacterStyle Style;

	std::size_t End() const noexcept { return Start + Length; }
	bool operator==(const RichTextStyleSpan&) const = default;
};

/**
 * Internal text-tree identity carried beside the portable attributed text.
 * It is intentionally optional: plain text and clipboard payloads remain
 * canonical without it, while editor-originated fragments can preserve the
 * authored Paragraph/Inline tree across replacement and Undo/Redo.
 */
enum class RichTextStructureKind : unsigned char
{
	Paragraph,
	Run,
	Span,
	Bold,
	Italic,
	Underline,
	LineBreak,
	ParagraphBreak
};

struct RichTextStructureNode
{
	std::uint64_t Id = 0;
	RichTextStructureKind Kind = RichTextStructureKind::Run;
	RichTextCharacterStyle LocalStyle;
	/** Only Paragraph nodes may carry block formatting. */
	RichTextParagraphStyle LocalParagraphStyle;

	bool operator==(const RichTextStructureNode&) const = default;
};

/** One contiguous text range and its Paragraph-to-leaf structural path. */
struct RichTextStructureSpan
{
	std::size_t Start = 0;
	std::size_t Length = 0;
	std::vector<RichTextStructureNode> Path;

	std::size_t End() const noexcept { return Start + Length; }
	bool operator==(const RichTextStructureSpan&) const = default;
};

/**
 * One zero-width structural leaf in document traversal order.
 *
 * StructureSpans cover every visible UTF-16 unit.  They cannot represent an
 * empty Run/Span or a terminal empty Paragraph, so those leaves are carried
 * separately at their visible-text position.  Markers at the same position
 * are ordered as stored and precede the non-empty leaf that starts there.
 */
struct RichTextStructureMarker
{
	std::size_t Position = 0;
	std::vector<RichTextStructureNode> Path;

	bool operator==(const RichTextStructureMarker&) const = default;
};

/** Process-local identity allocator; identities are never serialized. */
std::uint64_t AllocateRichTextStructureId() noexcept;

/**
 * A self-contained attributed string. Non-empty fragments use canonical
 * spans: sorted, gap-free, non-overlapping, full-covering, and merged when
 * adjacent styles are equal.
 */
struct RichTextDocumentFragment
{
	std::wstring Text;
	std::vector<RichTextStyleSpan> Spans;
	std::vector<RichTextStructureSpan> StructureSpans;
	/** Authored local style at the FlowDocument root. */
	std::optional<RichTextCharacterStyle> RootStyle;
	/** Process-local FlowDocument identity. Never serialized or copied to the
	 *  portable clipboard format. Present when spans or markers carry tree
	 *  provenance. */
	std::optional<std::uint64_t> StructureRootId;
	/** Ordered zero-width leaves; process-local structure provenance. */
	std::vector<RichTextStructureMarker> StructureMarkers;
	/** Authored block formatting at the FlowDocument root. */
	std::optional<RichTextParagraphStyle> RootParagraphStyle;

	static RichTextDocumentFragment FromPlainText(
		std::wstring text,
		RichTextCharacterStyle style = {});
	bool ValidateCanonical() const noexcept;
	bool Empty() const noexcept { return Text.empty(); }
	bool operator==(const RichTextDocumentFragment&) const = default;
};

/** Attributed before/after payload suitable for an editor undo record. */
struct RichTextDocumentChange
{
	std::size_t Start = 0;
	RichTextDocumentFragment Before;
	RichTextDocumentFragment After;

	bool Changed() const noexcept { return Before != After; }
	bool TextChanged() const noexcept { return Before.Text != After.Text; }
	bool FormattingChanged() const noexcept
	{
		return Before.Spans != After.Spans
			|| Before.StructureSpans != After.StructureSpans
			|| Before.StructureMarkers != After.StructureMarkers
			|| Before.RootStyle != After.RootStyle
			|| Before.RootParagraphStyle != After.RootParagraphStyle
			|| Before.StructureRootId != After.StructureRootId;
	}
};

/**
 * Flat attributed-document editing core for RichTextBox. Positions are UTF-16
 * code-unit offsets. Edit ranges expand to complete Unicode text elements;
 * structural/style spans remain free to cross combining-sequence boundaries.
 */
class RichTextDocument
{
public:
	RichTextDocument() = default;
	explicit RichTextDocument(
		std::wstring text,
		RichTextCharacterStyle style = {});
	explicit RichTextDocument(RichTextDocumentFragment fragment);

	const std::wstring& GetText() const noexcept { return _text; }
	const std::vector<RichTextStyleSpan>& GetSpans() const noexcept
	{
		return _spans;
	}
	std::size_t Length() const noexcept { return _text.size(); }
	bool Empty() const noexcept { return _text.empty(); }

	RichTextDocumentFragment ToFragment() const;
	bool ValidateCanonical() const noexcept;

	std::size_t SnapToBoundary(
		std::size_t position,
		RichTextBoundaryAffinity affinity) const noexcept;
	RichTextRange NormalizeRange(
		std::size_t start,
		std::size_t length,
		RichTextBoundaryAffinity collapsedAffinity =
			RichTextBoundaryAffinity::Forward) const noexcept;

	/** Returns the style covering an existing UTF-16 code unit. */
	RichTextCharacterStyle StyleAt(std::size_t textIndex) const;
	/** Returns the preferred adjacent style for newly inserted plain text. */
	RichTextCharacterStyle InsertionStyleAt(
		std::size_t position,
		RichTextBoundaryAffinity affinity =
			RichTextBoundaryAffinity::Backward) const;

	RichTextDocumentFragment Extract(
		std::size_t start,
		std::size_t length) const;
	/** Creates a same-root explicit LineBreak using the adjacent Inline path. */
	RichTextDocumentFragment CreateLineBreakFragment(
		std::size_t position,
		const RichTextCharacterStyle& style,
		RichTextBoundaryAffinity affinity =
			RichTextBoundaryAffinity::Backward) const;
	RichTextDocumentChange Replace(
		std::size_t start,
		std::size_t length,
		const RichTextDocumentFragment& replacement,
		RichTextBoundaryAffinity collapsedAffinity =
			RichTextBoundaryAffinity::Forward);
	RichTextDocumentChange ApplyFormat(
		std::size_t start,
		std::size_t length,
		const RichTextFormatDelta& delta,
		const RichTextCharacterStyle& effectiveBaseline = {});
	/** Applies a relative size change to every effective formatting run. */
	RichTextDocumentChange AdjustFontSize(
		std::size_t start,
		std::size_t length,
		float amount,
		float minimum,
		float maximum,
		const RichTextCharacterStyle& effectiveBaseline = {});
	RichTextDocumentChange ApplyParagraphFormat(
		std::size_t start,
		std::size_t length,
		const RichTextParagraphFormatDelta& delta,
		const RichTextParagraphStyle& effectiveBaseline = {});
	std::vector<RichTextParagraphStyle> ParagraphStylesInRange(
		std::size_t start,
		std::size_t length,
		const RichTextParagraphStyle& effectiveBaseline = {}) const;
	/** Canonical visible-text ranges for the authored Paragraph sequence. */
	std::vector<RichTextRange> ParagraphRanges() const;

private:
	std::wstring _text;
	std::vector<RichTextStyleSpan> _spans;
	std::vector<RichTextStructureSpan> _structureSpans;
	std::optional<RichTextCharacterStyle> _rootStyle;
	std::optional<std::uint64_t> _structureRootId;
	std::vector<RichTextStructureMarker> _structureMarkers;
	std::optional<RichTextParagraphStyle> _rootParagraphStyle;

	RichTextDocumentFragment ExtractExact(RichTextRange range) const;
};
