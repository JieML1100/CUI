#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class DependencyObject;
class FlowDocument;

/**
 * Specifies which neighboring content a TextPointer remains associated with
 * when content is inserted or removed at the pointer position.
 */
enum class LogicalDirection
{
	Backward,
	Forward
};

/** Category of the text-tree symbol adjacent to a TextPointer. */
enum class TextPointerContext
{
	None,
	Text,
	EmbeddedElement,
	ElementStart,
	ElementEnd
};

/** Exact visible-text replacement used to update live pointer gravity. */
struct TextPointerTextChange
{
	std::size_t Start = 0;
	std::size_t RemovedLength = 0;
	std::size_t InsertedLength = 0;

	bool operator==(const TextPointerTextChange&) const = default;
};

/**
 * A live position in one FlowDocument.
 *
 * TextOffset is the canonical visible UTF-16 projection used by CUI editing.
 * SymbolOffset is the WPF-style text-tree coordinate: every TextElement start
 * and end edge and every UTF-16 code unit inside a Run contributes one symbol.
 * Consequently several distinct symbol positions can share one TextOffset.
 * Paragraph and LineBreak projections consume CRLF in TextOffset while their
 * element end edge remains one symbol. Raw text positions may sit inside a
 * composed Unicode text element; insertion helpers snap to the enclosing
 * caret boundary without changing the raw symbol position.
 */
class TextPointer final
{
public:
	TextPointer() noexcept = default;

	bool IsValid() const noexcept;
	FlowDocument* GetDocument() const noexcept;
	__declspec(property(get = GetDocument)) FlowDocument* Document;
	LogicalDirection GetLogicalDirection() const;
	__declspec(property(get = GetLogicalDirection))
		LogicalDirection Direction;
	std::size_t GetTextOffset() const;
	__declspec(property(get = GetTextOffset)) std::size_t TextOffset;
	std::size_t GetSymbolOffset() const;
	__declspec(property(get = GetSymbolOffset)) std::size_t SymbolOffset;

	/** Compares exact text-tree positions in the same document. */
	int CompareTo(const TextPointer& position) const;
	/** Returns the signed UTF-16 text-unit distance to another position. */
	std::ptrdiff_t GetTextOffsetToPosition(
		const TextPointer& position) const;
	/** Explicit alias of CompareTo for symbol-oriented call sites. */
	int CompareSymbolPositionTo(const TextPointer& position) const;
	/** Returns the signed WPF-style symbol distance to another position. */
	std::ptrdiff_t GetSymbolOffsetToPosition(
		const TextPointer& position) const;
	/** Returns null when the requested relative text offset is out of range. */
	std::optional<TextPointer> GetPositionAtTextOffset(
		std::ptrdiff_t offset) const;
	std::optional<TextPointer> GetPositionAtTextOffset(
		std::ptrdiff_t offset, LogicalDirection direction) const;
	std::optional<TextPointer> GetPositionAtSymbolOffset(
		std::ptrdiff_t offset) const;
	std::optional<TextPointer> GetPositionAtSymbolOffset(
		std::ptrdiff_t offset, LogicalDirection direction) const;

	TextPointerContext GetPointerContext(
		LogicalDirection direction) const;
	DependencyObject* GetAdjacentElement(
		LogicalDirection direction) const;
	std::optional<TextPointer> GetNextContextPosition(
		LogicalDirection direction) const;
	std::size_t GetTextRunLength(LogicalDirection direction) const;
	std::wstring GetTextInRun(LogicalDirection direction) const;

	bool IsAtInsertionPosition() const;
	TextPointer GetInsertionPosition(LogicalDirection direction) const;
	std::optional<TextPointer> GetNextInsertionPosition(
		LogicalDirection direction) const;

	bool operator==(const TextPointer& other) const;

private:
	enum class SymbolTokenKind
	{
		None,
		ElementStart,
		Text,
		ElementEnd
	};
	enum class SymbolAnchorKind
	{
		TextProjection,
		DocumentStart,
		DocumentEnd,
		BeforeToken,
		AfterToken
	};
	struct State
	{
		FlowDocument* Document = nullptr;
		std::size_t TextOffset = 0;
		LogicalDirection Direction = LogicalDirection::Forward;
		SymbolAnchorKind AnchorKind = SymbolAnchorKind::TextProjection;
		SymbolTokenKind TokenKind = SymbolTokenKind::None;
		std::uint64_t StructureId = 0;
		std::size_t TextIndex = 0;
	};

	explicit TextPointer(std::shared_ptr<State> state) noexcept
		: _state(std::move(state)) {}
	const State& RequireState() const;

	std::shared_ptr<State> _state;

	friend class FlowDocument;
};
