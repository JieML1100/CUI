#pragma once

#include "Event.h"
#include "RichTextDocument.h"
#include "TextElement.h"
#include "TextPointer.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class FlowDocument;
class LineBreak;
class Paragraph;
class RichTextBox;
class Run;
class Span;

/** Base identity for top-level flow content. */
class Block : public TextElement
{
protected:
	Block() = default;

public:
	~Block() override = default;

	__declspec(property(get = GetTextAlignment, put = SetTextAlignment))
		::TextAlignment TextAlignment;
	::TextAlignment GetTextAlignment() const;
	void SetTextAlignment(::TextAlignment value);
	static const DependencyProperty& TextAlignmentProperty();

	__declspec(property(get = GetFlowDirection, put = SetFlowDirection))
		::FlowDirection FlowDirection;
	::FlowDirection GetFlowDirection() const;
	void SetFlowDirection(::FlowDirection value);
	static const DependencyProperty& FlowDirectionProperty();
};

/** Base identity for content hosted inside a Paragraph. */
class Inline : public TextElement
{
protected:
	Inline() = default;

public:
	~Inline() override = default;
};

/** Live, single-parent collection returned by FlowDocument.Blocks. */
class BlockCollection final
{
public:
	using Storage = std::vector<std::unique_ptr<Block>>;
	using const_iterator = Storage::const_iterator;

	explicit BlockCollection(FlowDocument& owner) noexcept;
	~BlockCollection();
	BlockCollection(const BlockCollection&) = delete;
	BlockCollection& operator=(const BlockCollection&) = delete;

	std::size_t Count() const noexcept { return _items.size(); }
	bool Empty() const noexcept { return _items.empty(); }
	Block* At(std::size_t index) noexcept;
	const Block* At(std::size_t index) const noexcept;
	const Storage& Items() const noexcept { return _items; }
	const_iterator begin() const noexcept { return _items.begin(); }
	const_iterator end() const noexcept { return _items.end(); }

	Block& Add(std::unique_ptr<Block>&& value);
	Block& Insert(std::size_t index, std::unique_ptr<Block>&& value);
	std::unique_ptr<Block> Remove(Block& value);
	std::unique_ptr<Block> RemoveAt(std::size_t index);
	void Clear();
	Paragraph& AddParagraph();

private:
	FlowDocument* _owner = nullptr;
	Storage _items;

	friend class FlowDocument;
};

/** Live, single-parent collection returned by Paragraph/Span.Inlines. */
class InlineCollection final
{
public:
	using Storage = std::vector<std::unique_ptr<Inline>>;
	using const_iterator = Storage::const_iterator;

	explicit InlineCollection(TextElement& owner) noexcept;
	~InlineCollection();
	InlineCollection(const InlineCollection&) = delete;
	InlineCollection& operator=(const InlineCollection&) = delete;

	std::size_t Count() const noexcept { return _items.size(); }
	bool Empty() const noexcept { return _items.empty(); }
	Inline* At(std::size_t index) noexcept;
	const Inline* At(std::size_t index) const noexcept;
	const Storage& Items() const noexcept { return _items; }
	const_iterator begin() const noexcept { return _items.begin(); }
	const_iterator end() const noexcept { return _items.end(); }

	Inline& Add(std::unique_ptr<Inline>&& value);
	Inline& Insert(std::size_t index, std::unique_ptr<Inline>&& value);
	template<typename TInline,
		std::enable_if_t<
			std::is_base_of_v<Inline, TInline>
				&& !std::is_same_v<Inline, TInline>, int> = 0>
	Inline& Add(std::unique_ptr<TInline>&& value)
	{
		return Insert(_items.size(), std::move(value));
	}
	template<typename TInline,
		std::enable_if_t<
			std::is_base_of_v<Inline, TInline>
				&& !std::is_same_v<Inline, TInline>, int> = 0>
	Inline& Insert(std::size_t index, std::unique_ptr<TInline>&& value)
	{
		ValidateInsertion(index, value.get());
		std::unique_ptr<Inline> converted(std::move(value));
		return InsertValidated(index, std::move(converted));
	}
	std::unique_ptr<Inline> Remove(Inline& value);
	std::unique_ptr<Inline> RemoveAt(std::size_t index);
	void Clear();
	Run& AddRun(std::wstring text = {});
	LineBreak& AddLineBreak();

private:
	void ValidateInsertion(
		std::size_t index, const Inline* value) const;
	Inline& InsertValidated(
		std::size_t index, std::unique_ptr<Inline>&& value);
	TextElement* _owner = nullptr;
	Storage _items;

	friend class FlowDocument;
	friend class Paragraph;
	friend class Span;
};

/** WPF-style inline grouping node with Inlines as its content member. */
class Span : public Inline
{
public:
	Span();
	explicit Span(std::unique_ptr<Inline> inlineValue);
	~Span() override;
	Span(const Span&) = delete;
	Span& operator=(const Span&) = delete;

	__declspec(property(get = GetInlines)) InlineCollection& Inlines;
	InlineCollection& GetInlines() noexcept { return _inlines; }
	const InlineCollection& GetInlines() const noexcept { return _inlines; }

protected:
	void OnFlowDocumentChanged(
		FlowDocument* oldDocument, FlowDocument* newDocument) override;

private:
	InlineCollection _inlines;

	friend class FlowDocument;
	friend class InlineCollection;
};

/** Type-style equivalent of Span with FontWeight=Bold. */
class Bold final : public Span
{
public:
	Bold();
	explicit Bold(std::unique_ptr<Inline> inlineValue);
};

/** Type-style equivalent of Span with FontStyle=Italic. */
class Italic final : public Span
{
public:
	Italic();
	explicit Italic(std::unique_ptr<Inline> inlineValue);
};

/** Type-style equivalent of Span with Underline=true. */
class Underline final : public Span
{
public:
	Underline();
	explicit Underline(std::unique_ptr<Inline> inlineValue);
};

/** WPF-style explicit inline hard break. It contributes one canonical CRLF
 *  while remaining inside its containing Paragraph. */
class LineBreak final : public Inline
{
public:
	LineBreak() = default;
	~LineBreak() override = default;
	LineBreak(const LineBreak&) = delete;
	LineBreak& operator=(const LineBreak&) = delete;
};

/** The minimum WPF flow block: a live collection of Inline children. */
class Paragraph final : public Block
{
public:
	Paragraph();
	explicit Paragraph(std::unique_ptr<Inline> inlineValue);
	~Paragraph() override;
	Paragraph(const Paragraph&) = delete;
	Paragraph& operator=(const Paragraph&) = delete;

	__declspec(property(get = GetInlines)) InlineCollection& Inlines;
	InlineCollection& GetInlines() noexcept { return _inlines; }
	const InlineCollection& GetInlines() const noexcept { return _inlines; }

protected:
	void OnFlowDocumentChanged(
		FlowDocument* oldDocument, FlowDocument* newDocument) override;

private:
	InlineCollection _inlines;
	/** Separator-local formatting retained when the flat editor rebuilds the
	 *  block tree. It is applied over the current Paragraph formatting so a
	 *  CRLF-only selection stays isolated without freezing inherited values. */
	std::optional<RichTextCharacterStyle> _reconstructedBreakStyle;
	std::uint64_t _breakStructureId = AllocateRichTextStructureId();

	friend class InlineCollection;
	friend class FlowDocument;
};

/** Terminal uniformly formatted text node. */
class Run final : public Inline
{
public:
	Run();
	explicit Run(std::wstring text);
	~Run() override = default;
	Run(const Run&) = delete;
	Run& operator=(const Run&) = delete;

	__declspec(property(get = GetText, put = SetText)) std::wstring Text;
	std::wstring GetText() const;
	void SetText(std::wstring value);

	static const DependencyProperty& TextProperty();
	static void RegisterDependencyProperties();

#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		TextElement::RegisterDependencyProperties();
		RegisterDependencyProperties();
	}
#endif
};

/**
 * Non-visual document root. A document may be empty and may be attached to at
 * most one RichTextBox; the control, not FlowDocument, creates its implicit
 * default Paragraph.
 */
class FlowDocument final : public TextElement
{
public:
	FlowDocument();
	explicit FlowDocument(std::unique_ptr<Block> block);
	~FlowDocument() override;
	FlowDocument(const FlowDocument&) = delete;
	FlowDocument& operator=(const FlowDocument&) = delete;
	FlowDocument(FlowDocument&&) = delete;
	FlowDocument& operator=(FlowDocument&&) = delete;

	__declspec(property(get = GetBlocks)) BlockCollection& Blocks;
	BlockCollection& GetBlocks() noexcept { return _blocks; }
	const BlockCollection& GetBlocks() const noexcept { return _blocks; }

	__declspec(property(get = GetTextAlignment, put = SetTextAlignment))
		::TextAlignment TextAlignment;
	::TextAlignment GetTextAlignment() const;
	void SetTextAlignment(::TextAlignment value);
	static const DependencyProperty& TextAlignmentProperty();

	__declspec(property(get = GetFlowDirection, put = SetFlowDirection))
		::FlowDirection FlowDirection;
	::FlowDirection GetFlowDirection() const;
	void SetFlowDirection(::FlowDirection value);
	static const DependencyProperty& FlowDirectionProperty();

	RichTextBox* GetOwner() const noexcept { return _owner; }
	bool TryAttachOwner(RichTextBox* owner) noexcept;
	void DetachOwner(RichTextBox* owner) noexcept;

	/** Coalesces any nested mutations into one Changed notification. */
	void BeginChange();
	void EndChange();
	bool IsChanging() const noexcept { return _changeDepth != 0; }
	void NotifyContentChanged();

	/** Creates a live pointer at an exact UTF-16 text offset. The returned raw
	 *  position may require GetInsertionPosition before editing. */
	TextPointer CreateTextPointerAtTextOffset(
		std::size_t offset,
		LogicalDirection direction = LogicalDirection::Forward) const;
	/** Creates an exact WPF-style text-tree position. */
	TextPointer CreateTextPointerAtSymbolOffset(
		std::size_t offset,
		LogicalDirection direction = LogicalDirection::Forward) const;
	std::size_t GetSymbolCount() const;
	TextPointer GetContentStart() const;
	TextPointer GetContentEnd() const;
	__declspec(property(get = GetContentStart)) TextPointer ContentStart;
	__declspec(property(get = GetContentEnd)) TextPointer ContentEnd;

	RichTextDocumentFragment Flatten() const;
	/**
	 * Returns the effective typing style when position is exactly a Paragraph
	 * start, including a zero-width Paragraph whose style has no flat span.
	 * The first Run contributes when one exists.
	 */
	bool TryGetParagraphInsertionStyleAt(
		std::size_t position, RichTextCharacterStyle& outStyle) const;
	bool ReplaceFromFragment(
		const RichTextDocumentFragment& fragment,
		std::wstring* outError = nullptr,
		bool preserveEmptyParagraphStyles = false,
		std::optional<TextPointerTextChange> textChange = std::nullopt);

	Event<void(FlowDocument*)> Changed;

private:
	enum class TextElementEdge
	{
		BeforeStart,
		AfterStart,
		BeforeEnd,
		AfterEnd
	};
	struct OwnerProjectionTransaction;
	struct TextTreeSymbol;
	struct TextTreeSymbolMap;
	std::vector<std::unique_ptr<Block>> BuildBlocks(
		const RichTextDocumentFragment& fragment) const;
	void ThrowIfMutationDisallowed() const;
	void EndChangeCore();
	void NotifyContentChangedCore(
		std::optional<TextPointerTextChange> textChange = std::nullopt);
	void NotifyRunTextChanged(
		const Run& run,
		const std::wstring& oldText,
		const std::wstring& newText);
	void PublishChanged();
	void SynchronizeTextPointers(
		std::optional<TextPointerTextChange> textChange = std::nullopt);
	TextPointer CreateTextPointerAtElementEdge(
		const TextElement& element, TextElementEdge edge) const;
	TextTreeSymbolMap BuildTextTreeSymbolMap() const;
	void SetTextPointerAnchorAtSymbolOffset(
		TextPointer::State& state,
		const TextTreeSymbolMap& map,
		std::size_t symbolOffset) const;
	std::size_t ResolveTextPointerSymbolOffset(
		TextPointer::State& state) const;
	TextPointerContext GetTextPointerContext(
		TextPointer::State& state, LogicalDirection direction) const;
	DependencyObject* GetTextPointerAdjacentElement(
		TextPointer::State& state, LogicalDirection direction) const;
	std::optional<TextPointer> GetNextTextPointerContextPosition(
		TextPointer::State& state, LogicalDirection direction) const;
	std::size_t GetTextPointerRunLength(
		TextPointer::State& state, LogicalDirection direction) const;
	std::wstring GetTextPointerRun(
		TextPointer::State& state, LogicalDirection direction) const;
	void InvalidateTextPointers() noexcept;
	void BeginOwnerProjectionTransaction();
	void CommitOwnerProjectionTransaction();
	void RollbackOwnerProjectionTransaction() noexcept;
	bool IsMutationDisallowed() const noexcept
	{
		return _mutationActive || _publishingChanged;
	}
	BlockCollection _blocks;
	RichTextBox* _owner = nullptr;
	std::size_t _changeDepth = 0;
	bool _changePending = false;
	/** Protects ownership journals and public Changed publication from
	 *  same-document high-level mutation reentry. */
	bool _mutationActive = false;
	bool _publishingChanged = false;
	std::unique_ptr<OwnerProjectionTransaction> _ownerProjectionTransaction;
	mutable std::vector<std::weak_ptr<TextPointer::State>> _textPointers;
	mutable std::wstring _textPointerSnapshotText;

	friend class BlockCollection;
	friend class InlineCollection;
	friend class RichTextBox;
	friend class Run;
	friend class TextPointer;
	friend class TextElement;
};
