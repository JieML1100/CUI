#include "FlowDocument.h"
#include "DependencyProperty.h"
#include "EventInfrastructure.h"
#include "RichTextBox.h"
#include "TextEditCore.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
	// TextElement exposes an MSVC __declspec(property) named Underline.  Keep
	// the document element type behind a namespace-scope alias so member
	// functions do not resolve the inherited property as a template argument.
	using UnderlineElement = ::Underline;

	void AppendStyledText(
		RichTextDocumentFragment& fragment,
		std::wstring_view text,
		const RichTextCharacterStyle& style)
	{
		if (text.empty()) return;
		const auto start = fragment.Text.size();
		fragment.Text.append(text);
		if (!fragment.Spans.empty()
			&& fragment.Spans.back().End() == start
			&& fragment.Spans.back().Style == style)
		{
			fragment.Spans.back().Length += text.size();
			return;
		}
		fragment.Spans.push_back(RichTextStyleSpan{
			start, text.size(), style });
	}

	RichTextStructureKind StructureKindOf(const Inline& inlineValue)
	{
		if (dynamic_cast<const LineBreak*>(&inlineValue))
			return RichTextStructureKind::LineBreak;
		if (dynamic_cast<const Bold*>(&inlineValue))
			return RichTextStructureKind::Bold;
		if (dynamic_cast<const Italic*>(&inlineValue))
			return RichTextStructureKind::Italic;
		if (dynamic_cast<const UnderlineElement*>(&inlineValue))
			return RichTextStructureKind::Underline;
		if (dynamic_cast<const Span*>(&inlineValue))
			return RichTextStructureKind::Span;
		return RichTextStructureKind::Run;
	}

	void ApplyIntrinsicStyle(
		RichTextStructureKind kind,
		RichTextCharacterStyle& style)
	{
		switch (kind)
		{
		case RichTextStructureKind::Bold:
			style.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
			break;
		case RichTextStructureKind::Italic:
			style.FontStyle = DWRITE_FONT_STYLE_ITALIC;
			break;
		case RichTextStructureKind::Underline:
			style.Underline = true;
			break;
		default:
			break;
		}
	}

	RichTextCharacterStyle LocalStyleOf(const TextElement& element)
	{
		RichTextCharacterStyle result;
		element.ApplyLocalCharacterStyle(result);
		return result;
	}

	RichTextParagraphStyle LocalParagraphStyleOf(
		const TextElement& element)
	{
		RichTextParagraphStyle result;
		element.ApplyLocalParagraphStyle(result);
		return result;
	}

	void ApplyParagraphStyle(
		Block& element,
		const RichTextParagraphStyle& style)
	{
		if (style.TextAlignment)
			element.SetTextAlignment(*style.TextAlignment);
		if (style.FlowDirection)
			element.SetFlowDirection(*style.FlowDirection);
	}

	std::size_t InlineNodeCount(const Inline& inlineValue) noexcept
	{
		std::size_t result = 1;
		if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
			for (const auto& child : span->GetInlines().Items())
				if (child) result += InlineNodeCount(*child);
		return result;
	}

	bool TryFindRunTextOffset(
		const Inline& inlineValue,
		const Run& target,
		std::size_t& current,
		std::size_t& result)
	{
		if (const auto* run = dynamic_cast<const Run*>(&inlineValue))
		{
			if (run == &target)
			{
				result = current;
				return true;
			}
			current += run->GetText().size();
			return false;
		}
		if (dynamic_cast<const LineBreak*>(&inlineValue))
		{
			current += 2;
			return false;
		}
		if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
		{
			for (const auto& child : span->GetInlines().Items())
				if (child && TryFindRunTextOffset(
					*child, target, current, result)) return true;
		}
		return false;
	}

	bool SameInlineSubtree(
		const Inline& current, const Inline& desired)
	{
		if (current.GetRichTextStructureId()
				!= desired.GetRichTextStructureId()
			|| typeid(current) != typeid(desired)
			|| LocalStyleOf(current) != LocalStyleOf(desired))
		{
			return false;
		}
		if (const auto* currentRun = dynamic_cast<const Run*>(&current))
		{
			const auto* desiredRun = dynamic_cast<const Run*>(&desired);
			return desiredRun
				&& currentRun->GetText() == desiredRun->GetText();
		}
		if (dynamic_cast<const LineBreak*>(&current))
			return dynamic_cast<const LineBreak*>(&desired) != nullptr;
		const auto* currentSpan = dynamic_cast<const Span*>(&current);
		const auto* desiredSpan = dynamic_cast<const Span*>(&desired);
		if (!currentSpan || !desiredSpan
			|| currentSpan->GetInlines().Count()
				!= desiredSpan->GetInlines().Count())
		{
			return false;
		}
		for (std::size_t index = 0;
			index < currentSpan->GetInlines().Count(); ++index)
		{
			const auto* currentChild =
				currentSpan->GetInlines().At(index);
			const auto* desiredChild =
				desiredSpan->GetInlines().At(index);
			if (!currentChild || !desiredChild
				|| !SameInlineSubtree(*currentChild, *desiredChild))
			{
				return false;
			}
		}
		return true;
	}

	void AppendStructuredText(
		RichTextDocumentFragment& fragment,
		std::wstring_view text,
		const RichTextCharacterStyle& style,
		const std::vector<RichTextStructureNode>& path)
	{
		if (text.empty()) return;
		const auto start = fragment.Text.size();
		AppendStyledText(fragment, text, style);
		if (!fragment.StructureSpans.empty()
			&& fragment.StructureSpans.back().End() == start
			&& fragment.StructureSpans.back().Path == path)
		{
			fragment.StructureSpans.back().Length += text.size();
		}
		else
		{
			fragment.StructureSpans.push_back(RichTextStructureSpan{
				start, text.size(), path });
		}
	}

	void AppendStructureMarker(
		RichTextDocumentFragment& fragment,
		const std::vector<RichTextStructureNode>& path)
	{
		if (path.empty())
			throw std::logic_error(
				"Zero-width rich-text structure path is empty.");
		fragment.StructureMarkers.push_back(RichTextStructureMarker{
			fragment.Text.size(), path });
	}

	void FlattenInline(
		const Inline& inlineValue,
		const RichTextCharacterStyle& inheritedStyle,
		std::vector<RichTextStructureNode>& path,
		RichTextDocumentFragment& result)
	{
		const auto kind = StructureKindOf(inlineValue);
		auto style = inheritedStyle;
		ApplyIntrinsicStyle(kind, style);
		inlineValue.ApplyLocalCharacterStyle(style);
		path.push_back(RichTextStructureNode{
			inlineValue.GetRichTextStructureId(), kind,
			LocalStyleOf(inlineValue) });
		if (const auto* run = dynamic_cast<const Run*>(&inlineValue))
		{
			const auto text = run->GetText();
			if (text.empty()) AppendStructureMarker(result, path);
			else AppendStructuredText(result, text, style, path);
		}
		else if (dynamic_cast<const LineBreak*>(&inlineValue))
		{
			AppendStructuredText(result, L"\r\n", style, path);
		}
		else if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
		{
			if (span->GetInlines().Empty())
			{
				AppendStructureMarker(result, path);
			}
			else
			{
				for (const auto& child : span->GetInlines().Items())
					if (child) FlattenInline(*child, style, path, result);
			}
		}
		path.pop_back();
	}

	bool TryFirstInlineStyle(
		const Inline& inlineValue,
		const RichTextCharacterStyle& inheritedStyle,
		RichTextCharacterStyle& outStyle)
	{
		auto style = inheritedStyle;
		const auto kind = StructureKindOf(inlineValue);
		ApplyIntrinsicStyle(kind, style);
		inlineValue.ApplyLocalCharacterStyle(style);
		if (dynamic_cast<const Run*>(&inlineValue)
			|| dynamic_cast<const LineBreak*>(&inlineValue))
		{
			outStyle = std::move(style);
			return true;
		}
		if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
			for (const auto& child : span->GetInlines().Items())
				if (child && TryFirstInlineStyle(*child, style, outStyle))
					return true;
		return false;
	}

	void ApplyStyle(TextElement& element, const RichTextCharacterStyle& style)
	{
		if (style.Foreground) element.SetForeground(*style.Foreground);
		if (style.Background) element.SetBackground(*style.Background);
		if (style.FontFamily) element.SetFontFamily(*style.FontFamily);
		if (style.Language) element.SetLanguage(*style.Language);
		if (style.FontSize) element.SetFontSize(*style.FontSize);
		if (style.FontWeight) element.SetFontWeight(*style.FontWeight);
		if (style.FontStretch) element.SetFontStretch(*style.FontStretch);
		if (style.FontStyle) element.SetFontStyle(*style.FontStyle);
		if (style.Underline) element.SetUnderline(*style.Underline);
		if (style.Strikethrough)
			element.SetStrikethrough(*style.Strikethrough);
	}

	template<typename TValue>
	void KeepCommonValue(
		std::optional<TValue>& candidate,
		const std::optional<TValue>& value)
	{
		if (candidate != value) candidate.reset();
	}

	RichTextCharacterStyle CommonStyleInRange(
		const RichTextDocumentFragment& fragment,
		std::size_t start,
		std::size_t length)
	{
		RichTextCharacterStyle result;
		const auto end = start + length;
		bool initialized = false;
		for (const auto& span : fragment.Spans)
		{
			if (span.End() <= start) continue;
			if (span.Start >= end) break;
			if (!initialized)
			{
				result = span.Style;
				initialized = true;
				continue;
			}
			KeepCommonValue(result.Foreground, span.Style.Foreground);
			KeepCommonValue(result.Background, span.Style.Background);
			KeepCommonValue(result.FontFamily, span.Style.FontFamily);
			KeepCommonValue(result.Language, span.Style.Language);
			KeepCommonValue(result.FontSize, span.Style.FontSize);
			KeepCommonValue(result.FontWeight, span.Style.FontWeight);
			KeepCommonValue(result.FontStretch, span.Style.FontStretch);
			KeepCommonValue(result.FontStyle, span.Style.FontStyle);
			KeepCommonValue(result.Underline, span.Style.Underline);
			KeepCommonValue(
				result.Strikethrough, span.Style.Strikethrough);
		}
		return result;
	}

	template<typename TValue>
	void CopyDifferentValue(
		std::optional<TValue>& target,
		const std::optional<TValue>& inherited,
		const std::optional<TValue>& desired)
	{
		if (desired && desired != inherited) target = desired;
	}

	RichTextCharacterStyle LocalOverridesAgainst(
		const RichTextCharacterStyle& inherited,
		const RichTextCharacterStyle& desired)
	{
		RichTextCharacterStyle result;
		CopyDifferentValue(
			result.Foreground, inherited.Foreground, desired.Foreground);
		CopyDifferentValue(
			result.Background, inherited.Background, desired.Background);
		CopyDifferentValue(
			result.FontFamily, inherited.FontFamily, desired.FontFamily);
		CopyDifferentValue(
			result.Language, inherited.Language, desired.Language);
		CopyDifferentValue(
			result.FontSize, inherited.FontSize, desired.FontSize);
		CopyDifferentValue(
			result.FontWeight, inherited.FontWeight, desired.FontWeight);
		CopyDifferentValue(
			result.FontStretch, inherited.FontStretch, desired.FontStretch);
		CopyDifferentValue(
			result.FontStyle, inherited.FontStyle, desired.FontStyle);
		CopyDifferentValue(
			result.Underline, inherited.Underline, desired.Underline);
		CopyDifferentValue(result.Strikethrough,
			inherited.Strikethrough, desired.Strikethrough);
		return result;
	}

	void OverlayStyle(
		RichTextCharacterStyle& target,
		const RichTextCharacterStyle& overrides)
	{
		if (overrides.Foreground) target.Foreground = overrides.Foreground;
		if (overrides.Background) target.Background = overrides.Background;
		if (overrides.FontFamily) target.FontFamily = overrides.FontFamily;
		if (overrides.Language) target.Language = overrides.Language;
		if (overrides.FontSize) target.FontSize = overrides.FontSize;
		if (overrides.FontWeight) target.FontWeight = overrides.FontWeight;
		if (overrides.FontStretch) target.FontStretch = overrides.FontStretch;
		if (overrides.FontStyle) target.FontStyle = overrides.FontStyle;
		if (overrides.Underline) target.Underline = overrides.Underline;
		if (overrides.Strikethrough)
			target.Strikethrough = overrides.Strikethrough;
	}

	void AppendParagraphRuns(
		Paragraph& paragraph,
		const RichTextDocumentFragment& fragment,
		std::size_t textStart,
		std::size_t textLength,
		const RichTextCharacterStyle& inheritedStyle)
	{
		if (textLength == 0) return;
		const auto textEnd = textStart + textLength;
		for (const auto& span : fragment.Spans)
		{
			const auto overlapStart = (std::max)(textStart, span.Start);
			const auto overlapEnd = (std::min)(textEnd, span.End());
			if (overlapEnd <= overlapStart) continue;
			auto run = std::make_unique<Run>(fragment.Text.substr(
				overlapStart, overlapEnd - overlapStart));
			ApplyStyle(*run,
				LocalOverridesAgainst(inheritedStyle, span.Style));
			paragraph.GetInlines().Add(std::move(run));
		}
	}

	const RichTextCharacterStyle* FindStyleAt(
		const RichTextDocumentFragment& fragment,
		std::size_t index) noexcept
	{
		const auto found = std::lower_bound(
			fragment.Spans.begin(), fragment.Spans.end(), index,
			[](const RichTextStyleSpan& span, std::size_t offset)
			{
				return span.End() <= offset;
			});
		return found != fragment.Spans.end()
			&& found->Start <= index && index < found->End()
			? &found->Style : nullptr;
	}

	std::size_t InlineTextLength(const Inline& inlineValue)
	{
		if (const auto* run = dynamic_cast<const Run*>(&inlineValue))
			return run->GetText().size();
		if (dynamic_cast<const LineBreak*>(&inlineValue)) return 2;
		std::size_t length = 0;
		if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
			for (const auto& child : span->GetInlines().Items())
				if (child) length += InlineTextLength(*child);
		return length;
	}

	std::size_t ParagraphTextLength(const Paragraph& paragraph)
	{
		std::size_t result = 0;
		for (const auto& inlineValue : paragraph.GetInlines().Items())
			if (inlineValue) result += InlineTextLength(*inlineValue);
		return result;
	}

	bool TryFindInlineOwnerStart(
		const Inline& inlineValue,
		const TextElement& owner,
		std::size_t& current,
		std::size_t& result)
	{
		if (&inlineValue == &owner)
		{
			result = current;
			return true;
		}
		if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
		{
			for (const auto& child : span->GetInlines().Items())
				if (child && TryFindInlineOwnerStart(
					*child, owner, current, result)) return true;
			return false;
		}
		current += InlineTextLength(inlineValue);
		return false;
	}

	bool TryFindInlineCollectionStart(
		const FlowDocument& document,
		const TextElement& owner,
		std::size_t& result)
	{
		std::size_t current = 0;
		for (std::size_t blockIndex = 0;
			blockIndex < document.GetBlocks().Count(); ++blockIndex)
		{
			const auto* paragraph = dynamic_cast<const Paragraph*>(
				document.GetBlocks().At(blockIndex));
			if (!paragraph) continue;
			if (paragraph == &owner)
			{
				result = current;
				return true;
			}
			for (const auto& inlineValue : paragraph->GetInlines().Items())
				if (inlineValue && TryFindInlineOwnerStart(
					*inlineValue, owner, current, result)) return true;
			if (blockIndex + 1 < document.GetBlocks().Count()) current += 2;
		}
		return false;
	}

	std::size_t InlinePrefixLength(
		const InlineCollection& collection, std::size_t index)
	{
		std::size_t result = 0;
		for (std::size_t current = 0;
			current < index && current < collection.Count(); ++current)
		{
			const auto* inlineValue = collection.At(current);
			if (inlineValue) result += InlineTextLength(*inlineValue);
		}
		return result;
	}

	std::size_t BlockStartOffset(
		const BlockCollection& collection, std::size_t index)
	{
		std::size_t result = 0;
		for (std::size_t current = 0;
			current < index && current < collection.Count(); ++current)
		{
			const auto* paragraph = dynamic_cast<const Paragraph*>(
				collection.At(current));
			if (paragraph) result += ParagraphTextLength(*paragraph);
			if (current + 1 < collection.Count()) result += 2;
		}
		return result;
	}

	void PreserveCorrespondingEmptyParagraphStyles(
		const BlockCollection::Storage& oldBlocks,
		std::vector<std::unique_ptr<Block>>& newBlocks,
		const std::wstring& oldText,
		const std::wstring& newText,
		const RichTextCharacterStyle& documentStyle)
	{
		// A sole empty Paragraph has no code unit from which BuildBlocks can
		// recreate its identity.  Editor-originated replacements retain it;
		// compatibility Text replacement keeps the default false behavior.
		if (newBlocks.empty() && newText.empty() && oldText.empty()
			&& oldBlocks.size() == 1)
		{
			const auto* oldParagraph = dynamic_cast<const Paragraph*>(
				oldBlocks.front().get());
			if (oldParagraph && ParagraphTextLength(*oldParagraph) == 0)
				newBlocks.push_back(std::make_unique<Paragraph>());
		}

		struct NewEmptyParagraph
		{
			std::size_t Start = 0;
			Paragraph* Value = nullptr;
		};
		std::vector<NewEmptyParagraph> newEmptyParagraphs;
		std::size_t newStart = 0;
		for (std::size_t index = 0; index < newBlocks.size(); ++index)
		{
			auto* paragraph = dynamic_cast<Paragraph*>(newBlocks[index].get());
			if (!paragraph) continue;
			const auto length = ParagraphTextLength(*paragraph);
			if (length == 0)
				newEmptyParagraphs.push_back({ newStart, paragraph });
			newStart += length;
			if (index + 1 < newBlocks.size()) newStart += 2;
		}

		std::size_t commonPrefix = 0;
		while (commonPrefix < oldText.size()
			&& commonPrefix < newText.size()
			&& oldText[commonPrefix] == newText[commonPrefix])
		{
			++commonPrefix;
		}
		std::size_t commonSuffix = 0;
		while (commonSuffix < oldText.size() - commonPrefix
			&& commonSuffix < newText.size() - commonPrefix
			&& oldText[oldText.size() - 1 - commonSuffix]
				== newText[newText.size() - 1 - commonSuffix])
		{
			++commonSuffix;
		}
		const std::size_t oldChangeEnd = oldText.size() - commonSuffix;

		std::size_t oldStart = 0;
		for (std::size_t index = 0; index < oldBlocks.size(); ++index)
		{
			const auto* paragraph = dynamic_cast<const Paragraph*>(
				oldBlocks[index].get());
			if (!paragraph) continue;
			const auto length = ParagraphTextLength(*paragraph);
			// Non-terminal empty Paragraph formatting is represented by its
			// following CRLF.  Only a terminal empty Paragraph is zero-width in
			// the flat model and needs this sideband preservation.
			if (length == 0 && index + 1 == oldBlocks.size())
			{
				std::optional<std::size_t> mappedStart;
				if (oldText == newText || oldStart < commonPrefix)
				{
					mappedStart = oldStart;
				}
				else if (oldStart >= oldChangeEnd)
				{
					if (newText.size() >= oldText.size())
						mappedStart = oldStart
							+ (newText.size() - oldText.size());
					else
					{
						const auto removedLength =
							oldText.size() - newText.size();
						if (oldStart >= removedLength)
							mappedStart = oldStart - removedLength;
					}
				}

				if (mappedStart)
				{
					const auto found = std::lower_bound(
						newEmptyParagraphs.begin(), newEmptyParagraphs.end(),
						*mappedStart,
						[](const NewEmptyParagraph& candidate,
							std::size_t position)
						{
							return candidate.Start < position;
						});
					if (found != newEmptyParagraphs.end()
						&& found->Start == *mappedStart)
					{
						auto insertionStyle = documentStyle;
						paragraph->ApplyLocalCharacterStyle(insertionStyle);
						for (const auto& inlineValue :
							paragraph->GetInlines().Items())
						{
							if (inlineValue
								&& TryFirstInlineStyle(*inlineValue,
									insertionStyle, insertionStyle))
								break;
						}
						ApplyStyle(*found->Value,
							LocalOverridesAgainst(
								documentStyle, insertionStyle));
					}
				}
			}
			oldStart += length;
			if (index + 1 < oldBlocks.size()) oldStart += 2;
		}
	}

	bool Fail(std::wstring* outError, std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	bool IsAtomicTextBoundary(
		const std::wstring& text, std::size_t position) noexcept
	{
		return CuiTextBoundary::IsTextElementBoundary(
			text, position, true);
	}

	TextPointerTextChange InferTextPointerChange(
		const std::wstring& before, const std::wstring& after) noexcept
	{
		std::size_t prefix = 0;
		while (prefix < before.size() && prefix < after.size()
			&& before[prefix] == after[prefix])
		{
			++prefix;
		}
		while (prefix > 0
			&& (!IsAtomicTextBoundary(before, prefix)
				|| !IsAtomicTextBoundary(after, prefix)))
		{
			--prefix;
		}

		std::size_t suffix = 0;
		while (suffix < before.size() - prefix
			&& suffix < after.size() - prefix
			&& before[before.size() - suffix - 1]
				== after[after.size() - suffix - 1])
		{
			++suffix;
		}
		while (suffix > 0
			&& (!IsAtomicTextBoundary(before, before.size() - suffix)
				|| !IsAtomicTextBoundary(after, after.size() - suffix)))
		{
			--suffix;
		}
		return TextPointerTextChange{
			prefix,
			before.size() - prefix - suffix,
			after.size() - prefix - suffix };
	}

	bool ValidateTextPointerChange(
		const TextPointerTextChange& change,
		const std::wstring& before,
		const std::wstring& after) noexcept
	{
		if (change.Start > before.size()
			|| change.RemovedLength > before.size() - change.Start
			|| change.Start > after.size()
			|| change.InsertedLength > after.size() - change.Start
			|| before.size() - change.RemovedLength
				> (std::numeric_limits<std::size_t>::max)()
					- change.InsertedLength
			|| before.size() - change.RemovedLength
				+ change.InsertedLength != after.size()
			|| !IsAtomicTextBoundary(before, change.Start)
			|| !IsAtomicTextBoundary(
				before, change.Start + change.RemovedLength)
			|| !IsAtomicTextBoundary(after, change.Start)
			|| !IsAtomicTextBoundary(
				after, change.Start + change.InsertedLength))
		{
			return false;
		}
		return before.compare(0, change.Start, after, 0, change.Start) == 0
			&& before.compare(
				change.Start + change.RemovedLength,
				before.size() - change.Start - change.RemovedLength,
				after,
				change.Start + change.InsertedLength,
				after.size() - change.Start - change.InsertedLength) == 0;
	}

	std::size_t RebaseTextPointerOffset(
		std::size_t position,
		LogicalDirection direction,
		const TextPointerTextChange& change) noexcept
	{
		const auto end = change.Start + change.RemovedLength;
		if (position < change.Start) return position;
		if (position > end)
		{
			return position - change.RemovedLength
				+ change.InsertedLength;
		}
		return direction == LogicalDirection::Backward
			? change.Start : change.Start + change.InsertedLength;
	}
}

struct FlowDocument::TextTreeSymbol
{
	enum class Kind
	{
		ElementStart,
		Text,
		ElementEnd
	};

	Kind Type = Kind::Text;
	const TextElement* Element = nullptr;
	std::uint64_t StructureId = 0;
	std::size_t TextIndex = 0;
	wchar_t Character = L'\0';
};

struct FlowDocument::TextTreeSymbolMap
{
	std::vector<TextTreeSymbol> Symbols;
	/** Visible UTF-16 offset at every boundary; size is Symbols.size()+1. */
	std::vector<std::size_t> TextOffsets{ 0 };
};

struct FlowDocument::OwnerProjectionTransaction
{
	struct PointerSnapshot
	{
		std::shared_ptr<TextPointer::State> Pointer;
		TextPointer::State Saved;
	};

	RichTextBox* Owner = nullptr;
	std::unique_ptr<RichTextBox::DocumentProjectionTransactionState> State;
	std::wstring PointerText;
	std::vector<PointerSnapshot> Pointers;
};

std::vector<std::unique_ptr<Block>> FlowDocument::BuildBlocks(
	const RichTextDocumentFragment& fragment) const
{
	std::vector<std::unique_ptr<Block>> result;
	if (fragment.Text.empty() && fragment.StructureMarkers.empty()) return result;
	if (!fragment.StructureSpans.empty()
		|| !fragment.StructureMarkers.empty())
	{
		struct ActiveSpan
		{
			std::uint64_t Id = 0;
			Span* Value = nullptr;
		};
		Paragraph* paragraph = nullptr;
		std::uint64_t paragraphId = 0;
		std::vector<ActiveSpan> activeSpans;
		auto isWrapperKind = [](RichTextStructureKind kind) noexcept
		{
			return kind == RichTextStructureKind::Span
				|| kind == RichTextStructureKind::Bold
				|| kind == RichTextStructureKind::Italic
				|| kind == RichTextStructureKind::Underline;
		};
		auto materialize = [&](const std::vector<RichTextStructureNode>& path,
			const RichTextStructureSpan* structureSpan)
		{
			if (path.empty()
				|| path.front().Kind != RichTextStructureKind::Paragraph)
				throw std::invalid_argument(
					"Structured fragment has no Paragraph root.");
			if (!paragraph || paragraphId != path.front().Id)
			{
				auto value = std::make_unique<Paragraph>();
				value->RestoreRichTextStructureId(path.front().Id);
				ApplyStyle(*value, path.front().LocalStyle);
				ApplyParagraphStyle(
					*value, path.front().LocalParagraphStyle);
				paragraph = value.get();
				paragraphId = path.front().Id;
				activeSpans.clear();
				result.push_back(std::move(value));
			}

			if (structureSpan && path.back().Kind
				== RichTextStructureKind::ParagraphBreak)
			{
				if (path.size() != 2)
					throw std::invalid_argument(
						"ParagraphBreak cannot be nested in an Inline.");
				paragraph->_breakStructureId = path.back().Id;
				paragraph->_reconstructedBreakStyle =
					path.back().LocalStyle;
				activeSpans.clear();
				return;
			}
			const auto terminalKind = path.back().Kind;
			if (!structureSpan
				&& terminalKind == RichTextStructureKind::Paragraph)
			{
				if (path.size() != 1)
					throw std::invalid_argument(
						"Empty Paragraph marker is nested.");
				activeSpans.clear();
				return;
			}
			const bool terminalWrapper = !structureSpan
				&& isWrapperKind(terminalKind);
			if (!terminalWrapper
				&& terminalKind != RichTextStructureKind::Run
				&& terminalKind != RichTextStructureKind::LineBreak)
				throw std::invalid_argument(
					"Structured fragment has no terminal Inline.");
			if (!structureSpan
				&& terminalKind == RichTextStructureKind::LineBreak)
				throw std::invalid_argument(
					"LineBreak cannot be zero-width.");

			const auto wrapperCount = terminalWrapper
				? path.size() - 1 : path.size() - 2;
			std::size_t shared = 0;
			while (shared < activeSpans.size()
				&& shared < wrapperCount
				&& activeSpans[shared].Id == path[shared + 1].Id)
				++shared;
			activeSpans.resize(shared);
			InlineCollection* target = activeSpans.empty()
				? &paragraph->GetInlines()
				: &activeSpans.back().Value->GetInlines();
			for (std::size_t depth = shared; depth < wrapperCount; ++depth)
			{
				const auto& definition = path[depth + 1];
				std::unique_ptr<Span> value;
				switch (definition.Kind)
				{
				case RichTextStructureKind::Span:
					value = std::make_unique<Span>();
					break;
				case RichTextStructureKind::Bold:
					value = std::make_unique<Bold>();
					break;
				case RichTextStructureKind::Italic:
					value = std::make_unique<Italic>();
					break;
				case RichTextStructureKind::Underline:
					value = std::make_unique<UnderlineElement>();
					break;
				default:
					throw std::invalid_argument(
						"Structured fragment contains an invalid Span kind.");
				}
				value->RestoreRichTextStructureId(definition.Id);
				ApplyStyle(*value, definition.LocalStyle);
				auto* raw = value.get();
				target->Add(std::move(value));
				activeSpans.push_back({ definition.Id, raw });
				target = &raw->GetInlines();
			}
			if (terminalWrapper) return;

			const auto& terminalDefinition = path.back();
			if (terminalKind == RichTextStructureKind::LineBreak)
			{
				auto lineBreak = std::make_unique<LineBreak>();
				lineBreak->RestoreRichTextStructureId(terminalDefinition.Id);
				ApplyStyle(*lineBreak, terminalDefinition.LocalStyle);
				target->Add(std::move(lineBreak));
			}
			else
			{
				auto run = std::make_unique<Run>(structureSpan
					? fragment.Text.substr(
						structureSpan->Start, structureSpan->Length)
					: std::wstring{});
				run->RestoreRichTextStructureId(terminalDefinition.Id);
				ApplyStyle(*run, terminalDefinition.LocalStyle);
				target->Add(std::move(run));
			}
		};

		std::size_t spanIndex = 0;
		std::size_t markerIndex = 0;
		bool lastEventWasParagraphBreak = false;
		while (spanIndex < fragment.StructureSpans.size()
			|| markerIndex < fragment.StructureMarkers.size())
		{
			const bool useMarker = markerIndex
				< fragment.StructureMarkers.size()
				&& (spanIndex >= fragment.StructureSpans.size()
					|| fragment.StructureMarkers[markerIndex].Position
						<= fragment.StructureSpans[spanIndex].Start);
			if (useMarker)
			{
				materialize(fragment.StructureMarkers[markerIndex++].Path,
					nullptr);
				lastEventWasParagraphBreak = false;
			}
			else
			{
				const auto& span = fragment.StructureSpans[spanIndex++];
				materialize(span.Path, &span);
				lastEventWasParagraphBreak = !span.Path.empty()
					&& span.Path.back().Kind
						== RichTextStructureKind::ParagraphBreak;
			}
		}
		if (lastEventWasParagraphBreak)
			result.push_back(std::make_unique<Paragraph>());
		return result;
	}
	RichTextCharacterStyle documentStyle;
	ApplyLocalCharacterStyle(documentStyle);

	std::size_t paragraphStart = 0;
	while (true)
	{
		const auto breakStart = fragment.Text.find_first_of(
			L"\r\n", paragraphStart);
		const auto paragraphEnd = breakStart == std::wstring::npos
			? fragment.Text.size() : breakStart;
		std::size_t breakLength = 0;
		if (breakStart != std::wstring::npos)
		{
			breakLength = 1;
			if (fragment.Text[breakStart] == L'\r'
				&& breakStart + 1 < fragment.Text.size()
				&& fragment.Text[breakStart + 1] == L'\n')
			{
				breakLength = 2;
			}
		}
		auto paragraph = std::make_unique<Paragraph>();
		const auto commonStyle = CommonStyleInRange(
			fragment, paragraphStart,
			paragraphEnd + breakLength - paragraphStart);
		const auto paragraphLocal =
			LocalOverridesAgainst(documentStyle, commonStyle);
		ApplyStyle(*paragraph, paragraphLocal);
		auto paragraphStyle = documentStyle;
		OverlayStyle(paragraphStyle, paragraphLocal);
		if (breakStart != std::wstring::npos)
		{
			if (const auto* separatorStyle =
				FindStyleAt(fragment, breakStart))
			{
				paragraph->_reconstructedBreakStyle =
					LocalOverridesAgainst(
						paragraphStyle, *separatorStyle);
			}
		}
		AppendParagraphRuns(*paragraph, fragment, paragraphStart,
			paragraphEnd - paragraphStart, paragraphStyle);
		result.push_back(std::move(paragraph));

		if (breakStart == std::wstring::npos) break;
		paragraphStart = breakStart + breakLength;
		if (paragraphStart == fragment.Text.size())
		{
			result.push_back(std::make_unique<Paragraph>());
			break;
		}
	}
	return result;
}

::TextAlignment Block::GetTextAlignment() const
{
	return GetDependencyPropertyValue<::TextAlignment>(TextAlignmentProperty());
}

void Block::SetTextAlignment(::TextAlignment value)
{
	EnsureTextMutationAllowed();
	const auto& property = TextAlignmentProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetTextAlignment() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

const DependencyProperty& Block::TextAlignmentProperty()
{
	return TextElement::TextAlignmentProperty();
}

::FlowDirection Block::GetFlowDirection() const
{
	return GetDependencyPropertyValue<::FlowDirection>(FlowDirectionProperty());
}

void Block::SetFlowDirection(::FlowDirection value)
{
	EnsureTextMutationAllowed();
	const auto& property = FlowDirectionProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFlowDirection() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

const DependencyProperty& Block::FlowDirectionProperty()
{
	return TextElement::FlowDirectionProperty();
}

BlockCollection::BlockCollection(FlowDocument& owner) noexcept
	: _owner(&owner)
{
}

BlockCollection::~BlockCollection() = default;

Block* BlockCollection::At(std::size_t index) noexcept
{
	return index < _items.size() ? _items[index].get() : nullptr;
}

const Block* BlockCollection::At(std::size_t index) const noexcept
{
	return index < _items.size() ? _items[index].get() : nullptr;
}

Block& BlockCollection::Add(std::unique_ptr<Block>&& value)
{
	return Insert(_items.size(), std::move(value));
}

Block& BlockCollection::Insert(
	std::size_t index, std::unique_ptr<Block>&& value)
{
	_owner->ThrowIfMutationDisallowed();
	if (!value) throw std::invalid_argument("Block cannot be null.");
	if (index > _items.size())
		throw std::out_of_range("Block insertion index is out of range.");
	if (value->GetParent())
		throw std::invalid_argument("Block already belongs to a text tree.");
	if (!dynamic_cast<Paragraph*>(value.get()))
		throw std::invalid_argument(
			"Only Paragraph blocks are supported by this FlowDocument.");

	const auto previousChangeDepth = _owner->_changeDepth;
	const bool previousChangePending = _owner->_changePending;
	const auto oldBlockCount = _items.size();
	const auto paragraphLength = ParagraphTextLength(
		*static_cast<const Paragraph*>(value.get()));
	const auto paragraphStart = BlockStartOffset(*this, index);
	const auto insertedLength = oldBlockCount == 0
		? paragraphLength
		: paragraphLength + 2;
	const auto changeStart = paragraphStart;
	auto* inserted = value.get();
	bool insertedInCollection = false;
	bool publicationCommitted = false;
	_owner->BeginOwnerProjectionTransaction();
	_owner->_mutationActive = true;
	try
	{
		_items.insert(
			_items.begin() + static_cast<std::ptrdiff_t>(index),
			std::move(value));
		insertedInCollection = true;
		inserted->SetTextTreeParent(_owner, _owner);
		_owner->NotifyContentChangedCore(insertedLength != 0
			? std::optional<TextPointerTextChange>(TextPointerTextChange{
				changeStart, 0, insertedLength })
			: std::nullopt);
		publicationCommitted = true;
		_owner->CommitOwnerProjectionTransaction();
		_owner->_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			_owner->_mutationActive = false;
			throw;
		}
		if (insertedInCollection)
		{
			auto rollback = std::move(_items[index]);
			_items.erase(
				_items.begin() + static_cast<std::ptrdiff_t>(index));
			rollback->RestoreTextTreeParentNoThrow(nullptr, nullptr);
			value = std::move(rollback);
		}
		_owner->_changeDepth = previousChangeDepth;
		_owner->_changePending = previousChangePending;
		_owner->RollbackOwnerProjectionTransaction();
		_owner->_mutationActive = false;
		throw;
	}
	return *inserted;
}

std::unique_ptr<Block> BlockCollection::Remove(Block& value)
{
	const auto found = std::find_if(
		_items.begin(), _items.end(),
		[&value](const auto& candidate) { return candidate.get() == &value; });
	if (found == _items.end()) return {};
	return RemoveAt(static_cast<std::size_t>(found - _items.begin()));
}

std::unique_ptr<Block> BlockCollection::RemoveAt(std::size_t index)
{
	_owner->ThrowIfMutationDisallowed();
	if (index >= _items.size()) return {};
	const auto previousChangeDepth = _owner->_changeDepth;
	const bool previousChangePending = _owner->_changePending;
	const auto oldBlockCount = _items.size();
	const auto* removedParagraph = dynamic_cast<const Paragraph*>(
		_items[index].get());
	const auto paragraphLength = removedParagraph
		? ParagraphTextLength(*removedParagraph) : 0;
	const auto paragraphStart = BlockStartOffset(*this, index);
	const auto changeStart = oldBlockCount > 1
		&& index + 1 == oldBlockCount
		? paragraphStart - 2 : paragraphStart;
	const auto removedLength = oldBlockCount > 1
		? paragraphLength + 2 : paragraphLength;
	std::unique_ptr<Block> result;
	bool removedFromCollection = false;
	bool publicationCommitted = false;
	_owner->BeginOwnerProjectionTransaction();
	_owner->_mutationActive = true;
	try
	{
		result = std::move(_items[index]);
		_items.erase(_items.begin() + static_cast<std::ptrdiff_t>(index));
		removedFromCollection = true;
		result->SetTextTreeParent(nullptr, nullptr);
		_owner->NotifyContentChangedCore(removedLength != 0
			? std::optional<TextPointerTextChange>(TextPointerTextChange{
				changeStart, removedLength, 0 })
			: std::nullopt);
		publicationCommitted = true;
		_owner->CommitOwnerProjectionTransaction();
		_owner->_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			_owner->_mutationActive = false;
			throw;
		}
		if (removedFromCollection)
		{
			result->RestoreTextTreeParentNoThrow(_owner, _owner);
			_items.insert(
				_items.begin() + static_cast<std::ptrdiff_t>(index),
				std::move(result));
		}
		_owner->_changeDepth = previousChangeDepth;
		_owner->_changePending = previousChangePending;
		_owner->RollbackOwnerProjectionTransaction();
		_owner->_mutationActive = false;
		throw;
	}
	return result;
}

void BlockCollection::Clear()
{
	_owner->ThrowIfMutationDisallowed();
	if (_items.empty()) return;
	const auto previousChangeDepth = _owner->_changeDepth;
	const bool previousChangePending = _owner->_changePending;
	const auto removedLength = _owner->Flatten().Text.size();
	Storage removed;
	bool transferred = false;
	bool publicationCommitted = false;
	_owner->BeginOwnerProjectionTransaction();
	_owner->_mutationActive = true;
	try
	{
		for (auto& item : _items)
			item->SetTextTreeParent(nullptr, nullptr);
		removed.swap(_items);
		transferred = true;
		_owner->NotifyContentChangedCore(removedLength != 0
			? std::optional<TextPointerTextChange>(TextPointerTextChange{
				0, removedLength, 0 })
			: std::nullopt);
		publicationCommitted = true;
		_owner->CommitOwnerProjectionTransaction();
		_owner->_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			_owner->_mutationActive = false;
			throw;
		}
		if (transferred) _items.swap(removed);
		for (auto& item : _items)
			item->RestoreTextTreeParentNoThrow(_owner, _owner);
		_owner->_changeDepth = previousChangeDepth;
		_owner->_changePending = previousChangePending;
		_owner->RollbackOwnerProjectionTransaction();
		_owner->_mutationActive = false;
		throw;
	}
}

Paragraph& BlockCollection::AddParagraph()
{
	auto paragraph = std::make_unique<Paragraph>();
	auto* result = paragraph.get();
	Add(std::move(paragraph));
	return *result;
}

InlineCollection::InlineCollection(TextElement& owner) noexcept
	: _owner(&owner)
{
}

InlineCollection::~InlineCollection() = default;

Inline* InlineCollection::At(std::size_t index) noexcept
{
	return index < _items.size() ? _items[index].get() : nullptr;
}

const Inline* InlineCollection::At(std::size_t index) const noexcept
{
	return index < _items.size() ? _items[index].get() : nullptr;
}

Inline& InlineCollection::Add(std::unique_ptr<Inline>&& value)
{
	return Insert(_items.size(), std::move(value));
}

Inline& InlineCollection::Insert(
	std::size_t index, std::unique_ptr<Inline>&& value)
{
	ValidateInsertion(index, value.get());
	return InsertValidated(index, std::move(value));
}

void InlineCollection::ValidateInsertion(
	std::size_t index, const Inline* value) const
{
	if (auto* document = _owner->GetFlowDocument())
		document->ThrowIfMutationDisallowed();
	if (!value) throw std::invalid_argument("Inline cannot be null.");
	if (index > _items.size())
		throw std::out_of_range("Inline insertion index is out of range.");
	if (value->GetParent())
		throw std::invalid_argument("Inline already belongs to a text tree.");
	for (const TextElement* ancestor = _owner;
		ancestor != nullptr;
		ancestor = dynamic_cast<const TextElement*>(ancestor->GetParent()))
	{
		if (ancestor == value)
			throw std::invalid_argument(
				"Inline insertion would create a text-tree cycle.");
	}
	if (!dynamic_cast<const Run*>(value)
		&& !dynamic_cast<const LineBreak*>(value)
		&& !dynamic_cast<const Span*>(value))
		throw std::invalid_argument(
			"Only Run, LineBreak and Span-derived inlines are supported.");
}

Inline& InlineCollection::InsertValidated(
	std::size_t index, std::unique_ptr<Inline>&& value)
{
	auto* document = _owner->GetFlowDocument();
	const auto previousChangeDepth = document ? document->_changeDepth : 0;
	const bool previousChangePending = document
		? document->_changePending : false;
	std::optional<TextPointerTextChange> textChange;
	if (document)
	{
		std::size_t collectionStart = 0;
		if (TryFindInlineCollectionStart(
			*document, *_owner, collectionStart))
		{
			const auto insertedLength = InlineTextLength(*value);
			if (insertedLength != 0)
			{
				textChange = TextPointerTextChange{
					collectionStart + InlinePrefixLength(*this, index),
					0, insertedLength };
			}
		}
	}
	auto* inserted = value.get();
	bool insertedInCollection = false;
	bool publicationCommitted = false;
	if (document) document->BeginOwnerProjectionTransaction();
	if (document) document->_mutationActive = true;
	try
	{
		_items.insert(
			_items.begin() + static_cast<std::ptrdiff_t>(index),
			std::move(value));
		insertedInCollection = true;
		inserted->SetTextTreeParent(_owner, document);
		if (document) document->NotifyContentChangedCore(textChange);
		publicationCommitted = document != nullptr;
		if (document) document->CommitOwnerProjectionTransaction();
		if (document) document->_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			document->_mutationActive = false;
			throw;
		}
		if (insertedInCollection)
		{
			auto rollback = std::move(_items[index]);
			_items.erase(
				_items.begin() + static_cast<std::ptrdiff_t>(index));
			rollback->RestoreTextTreeParentNoThrow(nullptr, nullptr);
			value = std::move(rollback);
		}
		if (document)
		{
			document->_changeDepth = previousChangeDepth;
			document->_changePending = previousChangePending;
			document->RollbackOwnerProjectionTransaction();
			document->_mutationActive = false;
		}
		throw;
	}
	return *inserted;
}

std::unique_ptr<Inline> InlineCollection::Remove(Inline& value)
{
	const auto found = std::find_if(
		_items.begin(), _items.end(),
		[&value](const auto& candidate) { return candidate.get() == &value; });
	if (found == _items.end()) return {};
	return RemoveAt(static_cast<std::size_t>(found - _items.begin()));
}

std::unique_ptr<Inline> InlineCollection::RemoveAt(std::size_t index)
{
	if (auto* activeDocument = _owner->GetFlowDocument())
		activeDocument->ThrowIfMutationDisallowed();
	if (index >= _items.size()) return {};
	auto* document = _owner->GetFlowDocument();
	const auto previousChangeDepth = document ? document->_changeDepth : 0;
	const bool previousChangePending = document
		? document->_changePending : false;
	std::optional<TextPointerTextChange> textChange;
	if (document)
	{
		std::size_t collectionStart = 0;
		if (TryFindInlineCollectionStart(
			*document, *_owner, collectionStart))
		{
			const auto removedLength = InlineTextLength(*_items[index]);
			if (removedLength != 0)
			{
				textChange = TextPointerTextChange{
					collectionStart + InlinePrefixLength(*this, index),
					removedLength, 0 };
			}
		}
	}
	std::unique_ptr<Inline> result;
	bool removedFromCollection = false;
	bool publicationCommitted = false;
	if (document) document->BeginOwnerProjectionTransaction();
	if (document) document->_mutationActive = true;
	try
	{
		result = std::move(_items[index]);
		_items.erase(_items.begin() + static_cast<std::ptrdiff_t>(index));
		removedFromCollection = true;
		result->SetTextTreeParent(nullptr, nullptr);
		if (document) document->NotifyContentChangedCore(textChange);
		publicationCommitted = document != nullptr;
		if (document) document->CommitOwnerProjectionTransaction();
		if (document) document->_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			document->_mutationActive = false;
			throw;
		}
		if (removedFromCollection)
		{
			result->RestoreTextTreeParentNoThrow(_owner, document);
			_items.insert(
				_items.begin() + static_cast<std::ptrdiff_t>(index),
				std::move(result));
		}
		if (document)
		{
			document->_changeDepth = previousChangeDepth;
			document->_changePending = previousChangePending;
			document->RollbackOwnerProjectionTransaction();
			document->_mutationActive = false;
		}
		throw;
	}
	return result;
}

void InlineCollection::Clear()
{
	if (auto* activeDocument = _owner->GetFlowDocument())
		activeDocument->ThrowIfMutationDisallowed();
	if (_items.empty()) return;
	auto* document = _owner->GetFlowDocument();
	const auto previousChangeDepth = document ? document->_changeDepth : 0;
	const bool previousChangePending = document
		? document->_changePending : false;
	std::optional<TextPointerTextChange> textChange;
	if (document)
	{
		std::size_t collectionStart = 0;
		if (TryFindInlineCollectionStart(
			*document, *_owner, collectionStart))
		{
			std::size_t removedLength = 0;
			for (const auto& inlineValue : _items)
				if (inlineValue)
					removedLength += InlineTextLength(*inlineValue);
			if (removedLength != 0)
				textChange = TextPointerTextChange{
					collectionStart, removedLength, 0 };
		}
	}
	Storage removed;
	bool transferred = false;
	bool publicationCommitted = false;
	if (document) document->BeginOwnerProjectionTransaction();
	if (document) document->_mutationActive = true;
	try
	{
		for (auto& item : _items)
			item->SetTextTreeParent(nullptr, nullptr);
		removed.swap(_items);
		transferred = true;
		if (document) document->NotifyContentChangedCore(textChange);
		publicationCommitted = document != nullptr;
		if (document) document->CommitOwnerProjectionTransaction();
		if (document) document->_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			document->_mutationActive = false;
			throw;
		}
		if (transferred) _items.swap(removed);
		for (auto& item : _items)
			item->RestoreTextTreeParentNoThrow(_owner, document);
		if (document)
		{
			document->_changeDepth = previousChangeDepth;
			document->_changePending = previousChangePending;
			document->RollbackOwnerProjectionTransaction();
			document->_mutationActive = false;
		}
		throw;
	}
}

Run& InlineCollection::AddRun(std::wstring text)
{
	auto run = std::make_unique<Run>(std::move(text));
	auto* result = run.get();
	Add(std::move(run));
	return *result;
}

LineBreak& InlineCollection::AddLineBreak()
{
	auto lineBreak = std::make_unique<LineBreak>();
	auto* result = lineBreak.get();
	Add(std::move(lineBreak));
	return *result;
}

Span::Span()
	: _inlines(*this)
{
}

Span::Span(std::unique_ptr<Inline> inlineValue)
	: Span()
{
	_inlines.Add(std::move(inlineValue));
}

Span::~Span() = default;

void Span::OnFlowDocumentChanged(
	FlowDocument* oldDocument, FlowDocument* newDocument)
{
	(void)oldDocument;
	for (auto& inlineValue : _inlines._items)
		inlineValue->SetTextTreeParent(this, newDocument);
}

Bold::Bold()
{
	(void)TrySetPropertyValue(
		FontWeightProperty(), BindingValue(DWRITE_FONT_WEIGHT_BOLD),
		DependencyPropertyValueSource::Style);
}

Bold::Bold(std::unique_ptr<Inline> inlineValue)
	: Bold()
{
	GetInlines().Add(std::move(inlineValue));
}

Italic::Italic()
{
	(void)TrySetPropertyValue(
		FontStyleProperty(), BindingValue(DWRITE_FONT_STYLE_ITALIC),
		DependencyPropertyValueSource::Style);
}

Italic::Italic(std::unique_ptr<Inline> inlineValue)
	: Italic()
{
	GetInlines().Add(std::move(inlineValue));
}

Underline::Underline()
{
	(void)TrySetPropertyValue(
		UnderlineProperty(), BindingValue(true),
		DependencyPropertyValueSource::Style);
}

Underline::Underline(std::unique_ptr<Inline> inlineValue)
	: Underline()
{
	GetInlines().Add(std::move(inlineValue));
}

Paragraph::Paragraph()
	: _inlines(*this)
{
}

Paragraph::Paragraph(std::unique_ptr<Inline> inlineValue)
	: Paragraph()
{
	_inlines.Add(std::move(inlineValue));
}

Paragraph::~Paragraph() = default;

void Paragraph::OnFlowDocumentChanged(
	FlowDocument* oldDocument, FlowDocument* newDocument)
{
	(void)oldDocument;
	for (auto& inlineValue : _inlines._items)
		inlineValue->SetTextTreeParent(this, newDocument);
}

Run::Run()
{
	RegisterDependencyProperties();
}

Run::Run(std::wstring text)
	: Run()
{
	Text = std::move(text);
}

std::wstring Run::GetText() const
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

void Run::SetText(std::wstring value)
{
	EnsureTextMutationAllowed();
	(void)SetDependencyPropertyValue(TextProperty(), std::move(value));
}

const DependencyProperty& Run::TextProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<Run, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::BindsTwoWayByDefault
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](Run&, const std::wstring& proposed)
			-> std::optional<std::wstring>
		{
			try
			{
				return RichTextDocumentFragment::FromPlainText(proposed).Text;
			}
			catch (const std::invalid_argument&)
			{
				return std::nullopt;
			}
		};
		options.Changed = [](
			Run& target,
			const std::wstring& oldValue,
			const std::wstring& newValue)
		{
			if (auto* document = target.GetFlowDocument())
				document->NotifyRunTextChanged(
					target, oldValue, newValue);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		options.Design.DisplayName = L"Text";
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<Run, std::wstring>(
			DependencyPropertyRegistrationLiteral(L"Text"),
			std::move(options));
	}();
	return *registration;
}

void Run::RegisterDependencyProperties()
{
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TextProperty();
#endif
}

FlowDocument::FlowDocument()
	: _blocks(*this)
{
	// The document is the root of its own non-visual text tree.  This makes
	// TextElement formatting changes publish through FlowDocument::Changed
	// without pretending that the document participates in the Control tree.
	SetTextTreeParent(nullptr, this);
	_textPointerSnapshotText.clear();
}

FlowDocument::FlowDocument(std::unique_ptr<Block> block)
	: FlowDocument()
{
	_blocks.Add(std::move(block));
}

::TextAlignment FlowDocument::GetTextAlignment() const
{
	return GetDependencyPropertyValue<::TextAlignment>(TextAlignmentProperty());
}

void FlowDocument::SetTextAlignment(::TextAlignment value)
{
	EnsureTextMutationAllowed();
	const auto& property = TextAlignmentProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetTextAlignment() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

const DependencyProperty& FlowDocument::TextAlignmentProperty()
{
	return TextElement::TextAlignmentProperty();
}

::FlowDirection FlowDocument::GetFlowDirection() const
{
	return GetDependencyPropertyValue<::FlowDirection>(FlowDirectionProperty());
}

void FlowDocument::SetFlowDirection(::FlowDirection value)
{
	EnsureTextMutationAllowed();
	const auto& property = FlowDirectionProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFlowDirection() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

const DependencyProperty& FlowDocument::FlowDirectionProperty()
{
	return TextElement::FlowDirectionProperty();
}

FlowDocument::~FlowDocument()
{
	InvalidateTextPointers();
}

bool FlowDocument::TryAttachOwner(RichTextBox* owner) noexcept
{
	if (!owner) return false;
	if (IsMutationDisallowed()) return false;
	if (_owner && _owner != owner) return false;
	_owner = owner;
	return true;
}

void FlowDocument::DetachOwner(RichTextBox* owner) noexcept
{
	if (IsMutationDisallowed()) return;
	if (_owner == owner) _owner = nullptr;
}

void FlowDocument::BeginChange()
{
	ThrowIfMutationDisallowed();
	++_changeDepth;
}

void FlowDocument::EndChange()
{
	ThrowIfMutationDisallowed();
	EndChangeCore();
}

void FlowDocument::EndChangeCore()
{
	if (_changeDepth == 0)
		throw std::logic_error("FlowDocument change scope is unbalanced.");
	--_changeDepth;
	if (_changeDepth == 0 && _changePending)
	{
		_changePending = false;
		PublishChanged();
	}
}

void FlowDocument::NotifyContentChanged()
{
	ThrowIfMutationDisallowed();
	NotifyContentChangedCore();
}

FlowDocument::TextTreeSymbolMap FlowDocument::BuildTextTreeSymbolMap() const
{
	TextTreeSymbolMap result;
	auto append = [&](TextTreeSymbol::Kind kind,
		const TextElement& element,
		std::size_t textIndex = 0,
		wchar_t character = L'\0',
		std::size_t projectedLength = 0)
	{
		if (result.TextOffsets.back()
			> (std::numeric_limits<std::size_t>::max)()
				- projectedLength)
		{
			throw std::overflow_error(
				"FlowDocument text-tree projection is out of range.");
		}
		result.Symbols.push_back(TextTreeSymbol{
			kind, &element, element.GetRichTextStructureId(),
			textIndex, character });
		result.TextOffsets.push_back(
			result.TextOffsets.back() + projectedLength);
	};

	auto appendInline = [&](auto&& self, const Inline& inlineValue) -> void
	{
		append(TextTreeSymbol::Kind::ElementStart, inlineValue);
		if (const auto* run = dynamic_cast<const Run*>(&inlineValue))
		{
			const auto text = run->GetText();
			for (std::size_t index = 0; index < text.size(); ++index)
			{
				append(TextTreeSymbol::Kind::Text, inlineValue,
					index, text[index], 1);
			}
		}
		else if (const auto* span = dynamic_cast<const Span*>(&inlineValue))
		{
			for (const auto& child : span->GetInlines().Items())
				if (child) self(self, *child);
		}
		const auto projectedEnd = dynamic_cast<const LineBreak*>(
			&inlineValue) ? 2ULL : 0ULL;
		append(TextTreeSymbol::Kind::ElementEnd, inlineValue,
			0, L'\0', static_cast<std::size_t>(projectedEnd));
	};

	for (std::size_t blockIndex = 0;
		blockIndex < _blocks._items.size(); ++blockIndex)
	{
		const auto* paragraph = dynamic_cast<const Paragraph*>(
			_blocks._items[blockIndex].get());
		if (!paragraph) continue;
		append(TextTreeSymbol::Kind::ElementStart, *paragraph);
		for (const auto& inlineValue : paragraph->GetInlines().Items())
			if (inlineValue) appendInline(appendInline, *inlineValue);
		append(TextTreeSymbol::Kind::ElementEnd, *paragraph,
			0, L'\0', blockIndex + 1 < _blocks._items.size() ? 2 : 0);
	}
	if (result.TextOffsets.size() != result.Symbols.size() + 1
		|| result.TextOffsets.back() != Flatten().Text.size())
	{
		throw std::logic_error(
			"FlowDocument text-tree symbols disagree with its text projection.");
	}
	return result;
}

void FlowDocument::SetTextPointerAnchorAtSymbolOffset(
	TextPointer::State& state,
	const TextTreeSymbolMap& map,
	std::size_t symbolOffset) const
{
	if (symbolOffset > map.Symbols.size())
		throw std::out_of_range("TextPointer symbol offset is out of range.");
	state.TokenKind = TextPointer::SymbolTokenKind::None;
	state.StructureId = 0;
	state.TextIndex = 0;
	if (map.Symbols.empty())
	{
		state.AnchorKind = state.Direction == LogicalDirection::Forward
			? TextPointer::SymbolAnchorKind::DocumentStart
			: TextPointer::SymbolAnchorKind::DocumentEnd;
		return;
	}
	const bool before = state.Direction == LogicalDirection::Forward
		&& symbolOffset < map.Symbols.size();
	const bool after = !before && state.Direction == LogicalDirection::Backward
		&& symbolOffset > 0;
	if (!before && !after)
	{
		state.AnchorKind = symbolOffset == 0
			? TextPointer::SymbolAnchorKind::DocumentStart
			: TextPointer::SymbolAnchorKind::DocumentEnd;
		return;
	}
	const auto& token = map.Symbols[before
		? symbolOffset : symbolOffset - 1];
	state.AnchorKind = before
		? TextPointer::SymbolAnchorKind::BeforeToken
		: TextPointer::SymbolAnchorKind::AfterToken;
	switch (token.Type)
	{
	case TextTreeSymbol::Kind::ElementStart:
		state.TokenKind = TextPointer::SymbolTokenKind::ElementStart;
		break;
	case TextTreeSymbol::Kind::Text:
		state.TokenKind = TextPointer::SymbolTokenKind::Text;
		break;
	case TextTreeSymbol::Kind::ElementEnd:
		state.TokenKind = TextPointer::SymbolTokenKind::ElementEnd;
		break;
	}
	state.StructureId = token.StructureId;
	state.TextIndex = token.TextIndex;
}

TextPointer FlowDocument::CreateTextPointerAtTextOffset(
	std::size_t offset, LogicalDirection direction) const
{
	VerifyAccess();
	auto currentText = Flatten().Text;
	if (_textPointerSnapshotText != currentText)
	{
		// A pointer requested from within an explicit BeginChange scope must see
		// the current tree even though public Changed publication is deferred.
		const_cast<FlowDocument*>(this)->SynchronizeTextPointers();
	}
	if (offset > _textPointerSnapshotText.size())
		throw std::out_of_range("TextPointer text offset is out of range.");
	auto state = std::make_shared<TextPointer::State>();
	state->Document = const_cast<FlowDocument*>(this);
	state->TextOffset = offset;
	state->Direction = direction;
	const auto map = BuildTextTreeSymbolMap();
	const auto lower = std::lower_bound(
		map.TextOffsets.begin(), map.TextOffsets.end(), offset);
	if (lower != map.TextOffsets.end() && *lower == offset)
	{
		const auto symbolOffset = direction == LogicalDirection::Backward
			? static_cast<std::size_t>(lower - map.TextOffsets.begin())
			: static_cast<std::size_t>(std::upper_bound(
				map.TextOffsets.begin(), map.TextOffsets.end(), offset)
				- map.TextOffsets.begin() - 1);
		SetTextPointerAnchorAtSymbolOffset(*state, map, symbolOffset);
	}
	_textPointers.emplace_back(state);
	return TextPointer(std::move(state));
}

TextPointer FlowDocument::CreateTextPointerAtSymbolOffset(
	std::size_t offset, LogicalDirection direction) const
{
	VerifyAccess();
	if (_textPointerSnapshotText != Flatten().Text)
		const_cast<FlowDocument*>(this)->SynchronizeTextPointers();
	const auto map = BuildTextTreeSymbolMap();
	if (offset > map.Symbols.size())
		throw std::out_of_range("TextPointer symbol offset is out of range.");
	auto state = std::make_shared<TextPointer::State>();
	state->Document = const_cast<FlowDocument*>(this);
	state->TextOffset = map.TextOffsets[offset];
	state->Direction = direction;
	SetTextPointerAnchorAtSymbolOffset(*state, map, offset);
	_textPointers.emplace_back(state);
	return TextPointer(std::move(state));
}

std::size_t FlowDocument::GetSymbolCount() const
{
	VerifyAccess();
	return BuildTextTreeSymbolMap().Symbols.size();
}

TextPointer FlowDocument::GetContentStart() const
{
	return CreateTextPointerAtSymbolOffset(0, LogicalDirection::Forward);
}

TextPointer FlowDocument::GetContentEnd() const
{
	return CreateTextPointerAtSymbolOffset(
		GetSymbolCount(), LogicalDirection::Backward);
}

std::size_t FlowDocument::ResolveTextPointerSymbolOffset(
	TextPointer::State& state) const
{
	VerifyAccess();
	if (state.Document != this)
		throw std::invalid_argument(
			"TextPointer does not belong to this FlowDocument.");
	if (_textPointerSnapshotText != Flatten().Text)
		const_cast<FlowDocument*>(this)->SynchronizeTextPointers();
	const auto map = BuildTextTreeSymbolMap();
	if (state.AnchorKind == TextPointer::SymbolAnchorKind::DocumentStart)
	{
		state.TextOffset = 0;
		return 0;
	}
	if (state.AnchorKind == TextPointer::SymbolAnchorKind::DocumentEnd)
	{
		state.TextOffset = map.TextOffsets.back();
		return map.Symbols.size();
	}
	if (state.AnchorKind == TextPointer::SymbolAnchorKind::BeforeToken
		|| state.AnchorKind == TextPointer::SymbolAnchorKind::AfterToken)
	{
		const auto expectedKind = [&]()
		{
			switch (state.TokenKind)
			{
			case TextPointer::SymbolTokenKind::ElementStart:
				return TextTreeSymbol::Kind::ElementStart;
			case TextPointer::SymbolTokenKind::Text:
				return TextTreeSymbol::Kind::Text;
			case TextPointer::SymbolTokenKind::ElementEnd:
				return TextTreeSymbol::Kind::ElementEnd;
			default:
				return TextTreeSymbol::Kind::Text;
			}
		}();
		for (std::size_t index = 0; index < map.Symbols.size(); ++index)
		{
			const auto& token = map.Symbols[index];
			if (token.Type != expectedKind
				|| token.StructureId != state.StructureId
				|| (token.Type == TextTreeSymbol::Kind::Text
					&& token.TextIndex != state.TextIndex))
			{
				continue;
			}
			const auto position = state.AnchorKind
				== TextPointer::SymbolAnchorKind::BeforeToken
				? index : index + 1;
			state.TextOffset = map.TextOffsets[position];
			return position;
		}
		state.AnchorKind = TextPointer::SymbolAnchorKind::TextProjection;
		state.TokenKind = TextPointer::SymbolTokenKind::None;
		state.StructureId = 0;
		state.TextIndex = 0;
	}

	const auto lower = std::lower_bound(
		map.TextOffsets.begin(), map.TextOffsets.end(), state.TextOffset);
	if (lower != map.TextOffsets.end() && *lower == state.TextOffset)
	{
		return state.Direction == LogicalDirection::Backward
			? static_cast<std::size_t>(lower - map.TextOffsets.begin())
			: static_cast<std::size_t>(std::upper_bound(
				map.TextOffsets.begin(), map.TextOffsets.end(),
				state.TextOffset) - map.TextOffsets.begin() - 1);
	}
	if (state.Direction == LogicalDirection::Forward)
		return static_cast<std::size_t>(lower - map.TextOffsets.begin());
	return lower == map.TextOffsets.begin()
		? 0 : static_cast<std::size_t>(lower - map.TextOffsets.begin() - 1);
}

TextPointer FlowDocument::CreateTextPointerAtElementEdge(
	const TextElement& element, TextElementEdge edge) const
{
	VerifyAccess();
	if (element.GetFlowDocument() != this
		|| static_cast<const TextElement*>(this) == &element)
	{
		throw std::invalid_argument(
			"TextElement is not attached below this FlowDocument.");
	}
	const auto map = BuildTextTreeSymbolMap();
	const auto expectedKind = edge == TextElementEdge::BeforeStart
		|| edge == TextElementEdge::AfterStart
		? TextTreeSymbol::Kind::ElementStart
		: TextTreeSymbol::Kind::ElementEnd;
	for (std::size_t index = 0; index < map.Symbols.size(); ++index)
	{
		if (map.Symbols[index].Type != expectedKind
			|| map.Symbols[index].Element != &element)
		{
			continue;
		}
		const bool after = edge == TextElementEdge::AfterStart
			|| edge == TextElementEdge::AfterEnd;
		return CreateTextPointerAtSymbolOffset(
			index + (after ? 1 : 0),
			after ? LogicalDirection::Backward
				: LogicalDirection::Forward);
	}
	throw std::logic_error(
		"Attached TextElement is missing from the text-tree symbol map.");
}

TextPointerContext FlowDocument::GetTextPointerContext(
	TextPointer::State& state, LogicalDirection direction) const
{
	const auto position = ResolveTextPointerSymbolOffset(state);
	const auto map = BuildTextTreeSymbolMap();
	if ((direction == LogicalDirection::Forward
		&& position >= map.Symbols.size())
		|| (direction == LogicalDirection::Backward && position == 0))
	{
		return TextPointerContext::None;
	}
	const auto& token = map.Symbols[direction == LogicalDirection::Forward
		? position : position - 1];
	switch (token.Type)
	{
	case TextTreeSymbol::Kind::ElementStart:
		return TextPointerContext::ElementStart;
	case TextTreeSymbol::Kind::Text:
		return TextPointerContext::Text;
	case TextTreeSymbol::Kind::ElementEnd:
		return TextPointerContext::ElementEnd;
	}
	return TextPointerContext::None;
}

DependencyObject* FlowDocument::GetTextPointerAdjacentElement(
	TextPointer::State& state, LogicalDirection direction) const
{
	const auto position = ResolveTextPointerSymbolOffset(state);
	const auto map = BuildTextTreeSymbolMap();
	if ((direction == LogicalDirection::Forward
		&& position >= map.Symbols.size())
		|| (direction == LogicalDirection::Backward && position == 0))
	{
		return nullptr;
	}
	const auto& token = map.Symbols[direction == LogicalDirection::Forward
		? position : position - 1];
	if (token.Type == TextTreeSymbol::Kind::Text) return nullptr;
	return const_cast<TextElement*>(token.Element);
}

std::optional<TextPointer> FlowDocument::GetNextTextPointerContextPosition(
	TextPointer::State& state, LogicalDirection direction) const
{
	const auto position = ResolveTextPointerSymbolOffset(state);
	const auto map = BuildTextTreeSymbolMap();
	std::size_t next = position;
	if (direction == LogicalDirection::Forward)
	{
		if (position >= map.Symbols.size()) return std::nullopt;
		if (map.Symbols[position].Type == TextTreeSymbol::Kind::Text)
		{
			while (next < map.Symbols.size()
				&& map.Symbols[next].Type == TextTreeSymbol::Kind::Text)
			{
				++next;
			}
		}
		else ++next;
	}
	else
	{
		if (position == 0) return std::nullopt;
		next = position - 1;
		if (map.Symbols[next].Type == TextTreeSymbol::Kind::Text)
		{
			while (next > 0
				&& map.Symbols[next - 1].Type
					== TextTreeSymbol::Kind::Text)
			{
				--next;
			}
		}
	}
	return CreateTextPointerAtSymbolOffset(next, direction);
}

std::size_t FlowDocument::GetTextPointerRunLength(
	TextPointer::State& state, LogicalDirection direction) const
{
	const auto position = ResolveTextPointerSymbolOffset(state);
	const auto map = BuildTextTreeSymbolMap();
	std::size_t count = 0;
	if (direction == LogicalDirection::Forward)
	{
		for (auto index = position;
			index < map.Symbols.size()
				&& map.Symbols[index].Type == TextTreeSymbol::Kind::Text;
			++index)
		{
			++count;
		}
	}
	else
	{
		for (auto index = position;
			index > 0
				&& map.Symbols[index - 1].Type
					== TextTreeSymbol::Kind::Text;
			--index)
		{
			++count;
		}
	}
	return count;
}

std::wstring FlowDocument::GetTextPointerRun(
	TextPointer::State& state, LogicalDirection direction) const
{
	const auto position = ResolveTextPointerSymbolOffset(state);
	const auto map = BuildTextTreeSymbolMap();
	std::wstring result;
	if (direction == LogicalDirection::Forward)
	{
		for (auto index = position;
			index < map.Symbols.size()
				&& map.Symbols[index].Type == TextTreeSymbol::Kind::Text;
			++index)
		{
			result.push_back(map.Symbols[index].Character);
		}
	}
	else
	{
		auto begin = position;
		while (begin > 0
			&& map.Symbols[begin - 1].Type == TextTreeSymbol::Kind::Text)
		{
			--begin;
		}
		result.reserve(position - begin);
		for (auto index = begin; index < position; ++index)
			result.push_back(map.Symbols[index].Character);
	}
	return result;
}

void FlowDocument::NotifyContentChangedCore(
	std::optional<TextPointerTextChange> textChange)
{
	SynchronizeTextPointers(std::move(textChange));
	if (_changeDepth != 0)
	{
		_changePending = true;
		return;
	}
	PublishChanged();
}

void FlowDocument::NotifyRunTextChanged(
	const Run& run,
	const std::wstring& oldText,
	const std::wstring& newText)
{
	std::size_t current = 0;
	std::size_t start = 0;
	bool found = false;
	for (std::size_t blockIndex = 0;
		blockIndex < _blocks._items.size() && !found; ++blockIndex)
	{
		const auto* paragraph = dynamic_cast<const Paragraph*>(
			_blocks._items[blockIndex].get());
		if (paragraph)
		{
			for (const auto& inlineValue : paragraph->GetInlines().Items())
			{
				if (inlineValue && TryFindRunTextOffset(
					*inlineValue, run, current, start))
				{
					found = true;
					break;
				}
			}
		}
		if (!found && blockIndex + 1 < _blocks._items.size()) current += 2;
	}
	NotifyContentChangedCore(found
		? std::optional<TextPointerTextChange>(TextPointerTextChange{
			start, oldText.size(), newText.size() })
		: std::nullopt);
}

void FlowDocument::ThrowIfMutationDisallowed() const
{
	if (IsMutationDisallowed())
		throw std::logic_error(
			"FlowDocument cannot be mutated during a document transaction callback.");
}

void FlowDocument::PublishChanged()
{
	if (_publishingChanged)
		throw std::logic_error(
			"FlowDocument Changed publication cannot be reentered.");
	_publishingChanged = true;
	try
	{
		SynchronizeTextPointers();
		if (_owner) _owner->OnFlowDocumentChangedInternal();
		cui::framework::EventAccess::Raise(Changed, this);
	}
	catch (...)
	{
		_publishingChanged = false;
		throw;
	}
	_publishingChanged = false;
}

void FlowDocument::SynchronizeTextPointers(
	std::optional<TextPointerTextChange> textChange)
{
	auto currentText = Flatten().Text;
	if (_textPointerSnapshotText == currentText && !textChange)
	{
		_textPointers.erase(std::remove_if(
			_textPointers.begin(), _textPointers.end(),
			[](const auto& candidate) { return candidate.expired(); }),
			_textPointers.end());
		return;
	}

	const auto change = textChange
		&& ValidateTextPointerChange(
			*textChange, _textPointerSnapshotText, currentText)
		? *textChange
		: InferTextPointerChange(_textPointerSnapshotText, currentText);
	for (auto pointer = _textPointers.begin();
		pointer != _textPointers.end();)
	{
		auto state = pointer->lock();
		if (!state)
		{
			pointer = _textPointers.erase(pointer);
			continue;
		}
		state->TextOffset = (std::min)(
			RebaseTextPointerOffset(
				state->TextOffset, state->Direction, change),
			currentText.size());
		if ((change.RemovedLength != 0 || change.InsertedLength != 0)
			&& state->TokenKind == TextPointer::SymbolTokenKind::Text)
		{
			state->AnchorKind =
				TextPointer::SymbolAnchorKind::TextProjection;
			state->TokenKind = TextPointer::SymbolTokenKind::None;
			state->StructureId = 0;
			state->TextIndex = 0;
		}
		++pointer;
	}
	if (_owner)
		_owner->RebaseSelectionForDocumentChange(change, currentText.size());
	_textPointerSnapshotText.swap(currentText);
}

void FlowDocument::InvalidateTextPointers() noexcept
{
	for (auto& pointer : _textPointers)
		if (auto state = pointer.lock()) state->Document = nullptr;
	_textPointers.clear();
	_textPointerSnapshotText.clear();
}

void FlowDocument::BeginOwnerProjectionTransaction()
{
	if (_changeDepth != 0) return;
	if (_ownerProjectionTransaction)
		throw std::logic_error(
			"FlowDocument owner projection transaction is already active.");
	auto transaction = std::make_unique<OwnerProjectionTransaction>();
	transaction->PointerText = _textPointerSnapshotText;
	transaction->Pointers.reserve(_textPointers.size());
	for (auto pointer = _textPointers.begin();
		pointer != _textPointers.end();)
	{
		auto state = pointer->lock();
		if (!state)
		{
			pointer = _textPointers.erase(pointer);
			continue;
		}
		transaction->Pointers.push_back(
			OwnerProjectionTransaction::PointerSnapshot{
				state, *state });
		++pointer;
	}
	transaction->Owner = _owner;
	if (_owner)
	{
		transaction->State =
			_owner->BeginFlowDocumentProjectionTransaction();
	}
	_ownerProjectionTransaction = std::move(transaction);
}

void FlowDocument::CommitOwnerProjectionTransaction()
{
	if (!_ownerProjectionTransaction) return;
	auto transaction = std::move(_ownerProjectionTransaction);
	if (transaction->Owner && transaction->State)
	{
		transaction->Owner->CommitFlowDocumentProjectionTransaction(
			*transaction->State);
	}
}

void FlowDocument::RollbackOwnerProjectionTransaction() noexcept
{
	if (!_ownerProjectionTransaction) return;
	auto transaction = std::move(_ownerProjectionTransaction);
	const auto reverseChange = InferTextPointerChange(
		_textPointerSnapshotText, transaction->PointerText);
	for (auto pointer = _textPointers.begin();
		pointer != _textPointers.end();)
	{
		auto state = pointer->lock();
		if (!state)
		{
			pointer = _textPointers.erase(pointer);
			continue;
		}
		const auto snapshot = std::find_if(
			transaction->Pointers.begin(), transaction->Pointers.end(),
			[&](const auto& candidate)
			{ return candidate.Pointer.get() == state.get(); });
		if (snapshot != transaction->Pointers.end())
		{
			*state = snapshot->Saved;
		}
		else
		{
			state->TextOffset = (std::min)(RebaseTextPointerOffset(
				state->TextOffset, state->Direction, reverseChange),
				transaction->PointerText.size());
			state->AnchorKind =
				TextPointer::SymbolAnchorKind::TextProjection;
			state->TokenKind = TextPointer::SymbolTokenKind::None;
			state->StructureId = 0;
			state->TextIndex = 0;
		}
		++pointer;
	}
	_textPointerSnapshotText.swap(transaction->PointerText);
	if (transaction->Owner && transaction->State)
	{
		transaction->Owner->RollbackFlowDocumentProjectionTransaction(
			*transaction->State);
	}
}

RichTextDocumentFragment FlowDocument::Flatten() const
{
	RichTextDocumentFragment result;
	RichTextCharacterStyle documentStyle;
	ApplyLocalCharacterStyle(documentStyle);
	result.RootStyle = documentStyle;
	RichTextParagraphStyle documentParagraphStyle;
	ApplyLocalParagraphStyle(documentParagraphStyle);
	result.RootParagraphStyle = documentParagraphStyle;
	result.StructureRootId = GetRichTextStructureId();
	for (std::size_t blockIndex = 0;
		blockIndex < _blocks._items.size(); ++blockIndex)
	{
		const auto* paragraph = dynamic_cast<const Paragraph*>(
			_blocks._items[blockIndex].get());
		if (!paragraph) continue;

		auto paragraphStyle = documentStyle;
		paragraph->ApplyLocalCharacterStyle(paragraphStyle);
		std::vector<RichTextStructureNode> path{
			RichTextStructureNode{
				paragraph->GetRichTextStructureId(),
				RichTextStructureKind::Paragraph,
				LocalStyleOf(*paragraph),
				LocalParagraphStyleOf(*paragraph) } };
		if (paragraph->GetInlines().Empty())
			AppendStructureMarker(result, path);
		for (const auto& inlineValue : paragraph->GetInlines()._items)
		{
			if (inlineValue)
				FlattenInline(*inlineValue, paragraphStyle, path, result);
		}

		if (blockIndex + 1 < _blocks._items.size())
		{
			auto separatorStyle = paragraphStyle;
			if (paragraph->_reconstructedBreakStyle)
				OverlayStyle(separatorStyle,
					*paragraph->_reconstructedBreakStyle);
			auto breakPath = path;
			breakPath.push_back(RichTextStructureNode{
				paragraph->_breakStructureId,
				RichTextStructureKind::ParagraphBreak,
				paragraph->_reconstructedBreakStyle.value_or(
					RichTextCharacterStyle{}) });
			AppendStructuredText(
				result, L"\r\n", separatorStyle, breakPath);
		}
	}
	if (result.Text.empty() && result.StructureMarkers.empty())
	{
		result.StructureSpans.clear();
		result.RootStyle.reset();
		result.RootParagraphStyle.reset();
		result.StructureRootId.reset();
	}
	else if (!result.ValidateCanonical())
	{
		throw std::logic_error(
			"FlowDocument produced invalid rich-text structure provenance.");
	}
	return result;
}

bool FlowDocument::TryGetParagraphInsertionStyleAt(
	std::size_t position, RichTextCharacterStyle& outStyle) const
{
	RichTextCharacterStyle documentStyle;
	ApplyLocalCharacterStyle(documentStyle);
	std::size_t paragraphStart = 0;
	for (std::size_t blockIndex = 0;
		blockIndex < _blocks._items.size(); ++blockIndex)
	{
		const auto* paragraph = dynamic_cast<const Paragraph*>(
			_blocks._items[blockIndex].get());
		if (!paragraph) continue;
		const auto paragraphLength = ParagraphTextLength(*paragraph);
		if (position == paragraphStart)
		{
			auto style = documentStyle;
			paragraph->ApplyLocalCharacterStyle(style);
			for (const auto& inlineValue : paragraph->GetInlines()._items)
			{
				if (inlineValue
					&& TryFirstInlineStyle(
						*inlineValue, style, style)) break;
			}
			outStyle = std::move(style);
			return true;
		}
		paragraphStart += paragraphLength;
		if (blockIndex + 1 < _blocks._items.size()) paragraphStart += 2;
	}
	return false;
}

bool FlowDocument::ReplaceFromFragment(
	const RichTextDocumentFragment& fragment,
	std::wstring* outError,
	bool preserveEmptyParagraphStyles,
	std::optional<TextPointerTextChange> textChange)
{
	ThrowIfMutationDisallowed();
	if (!fragment.ValidateCanonical())
		return Fail(outError,
			L"RichTextDocumentFragment is not canonical.");
	if (fragment.RootStyle
		&& *fragment.RootStyle != LocalStyleOf(*this))
	{
		return Fail(outError,
			L"Structured rich text belongs to a different FlowDocument root style.");
	}
	if (fragment.RootParagraphStyle
		&& *fragment.RootParagraphStyle != LocalParagraphStyleOf(*this))
	{
		return Fail(outError,
			L"Structured rich text belongs to different FlowDocument block formatting.");
	}
	if ((!fragment.StructureSpans.empty()
		|| !fragment.StructureMarkers.empty())
		&& fragment.StructureRootId != GetRichTextStructureId())
	{
		return Fail(outError,
			L"Structured rich text belongs to a different FlowDocument root.");
	}
	if (fragment == Flatten())
	{
		if (outError) outError->clear();
		return true;
	}

	std::vector<std::unique_ptr<Block>> replacement;
	try
	{
		replacement = BuildBlocks(fragment);
		// Structured fragments already carry exact zero-width Paragraph/Inline
		// provenance. Re-inferring styles from flat text would overwrite that
		// authored state and break attributed Undo/Redo equality. Keep this
		// compatibility path only for plain fragments without structure data.
		if (preserveEmptyParagraphStyles
			&& fragment.StructureSpans.empty()
			&& fragment.StructureMarkers.empty())
		{
			const auto oldText = Flatten().Text;
			RichTextCharacterStyle documentStyle;
			ApplyLocalCharacterStyle(documentStyle);
			PreserveCorrespondingEmptyParagraphStyles(
				_blocks._items, replacement, oldText, fragment.Text,
				documentStyle);
		}
	}
	catch (const std::exception&)
	{
		return Fail(outError,
			L"Unable to materialize the rich-text fragment.");
	}

	if (!fragment.StructureSpans.empty()
		|| !fragment.StructureMarkers.empty())
	{
		enum class JournalActionKind
		{
			BlockOwnership,
			InlineOwnership,
			InlineStorage,
			ParagraphMetadata
		};
		struct JournalAction
		{
			JournalActionKind Kind = JournalActionKind::InlineOwnership;
			void* Current = nullptr;
			void* Desired = nullptr;
		};
		std::vector<JournalAction> journal;
		std::size_t desiredInlineCount = 0;
		for (const auto& desiredBlock : replacement)
		{
			const auto* paragraph = dynamic_cast<const Paragraph*>(
				desiredBlock.get());
			if (!paragraph)
				return Fail(outError,
					L"Structured reconstruction produced a non-Paragraph.");
			for (const auto& child : paragraph->GetInlines().Items())
				if (child) desiredInlineCount += InlineNodeCount(*child);
		}
		try
		{
			const auto maximum =
				(std::numeric_limits<std::size_t>::max)();
			if (replacement.size() > maximum / 3)
			{
				return Fail(outError,
					L"Structured ownership journal is too large.");
			}
			const auto blockActionCount = replacement.size() * 3;
			if (desiredInlineCount
				> (maximum - blockActionCount) / 2)
			{
				return Fail(outError,
					L"Structured ownership journal is too large.");
			}
			journal.reserve(
				blockActionCount + desiredInlineCount * 2);
		}
		catch (const std::exception&)
		{
			return Fail(outError,
				L"Unable to allocate the structured ownership journal.");
		}

		auto swapInlineOwnership = [&](std::unique_ptr<Inline>& current,
			std::unique_ptr<Inline>& desired)
		{
			journal.push_back(JournalAction{
				JournalActionKind::InlineOwnership,
				&current, &desired });
			current.swap(desired);
		};
		auto reconcileInlines = [&](auto&& self,
			InlineCollection& current, InlineCollection& desired) -> void
		{
			for (auto& desiredItem : desired._items)
			{
				if (!desiredItem) continue;
				const auto desiredId =
					desiredItem->GetRichTextStructureId();
				auto found = std::find_if(
					current._items.begin(), current._items.end(),
					[&](const auto& candidate)
					{
						return candidate
							&& candidate->GetRichTextStructureId()
								== desiredId
							&& typeid(*candidate) == typeid(*desiredItem)
							&& LocalStyleOf(*candidate)
								== LocalStyleOf(*desiredItem);
					});
				if (found == current._items.end()) continue;
				if (SameInlineSubtree(**found, *desiredItem))
				{
					swapInlineOwnership(*found, desiredItem);
					continue;
				}
				auto* currentSpan = dynamic_cast<Span*>(found->get());
				auto* desiredSpan = dynamic_cast<Span*>(desiredItem.get());
				if (currentSpan && desiredSpan)
				{
					self(self, currentSpan->GetInlines(),
						desiredSpan->GetInlines());
					journal.push_back(JournalAction{
						JournalActionKind::InlineStorage,
						&currentSpan->GetInlines()._items,
						&desiredSpan->GetInlines()._items });
					currentSpan->GetInlines()._items.swap(
						desiredSpan->GetInlines()._items);
					swapInlineOwnership(*found, desiredItem);
				}
			}
		};
		auto sameParagraphSubtree = [&](const Paragraph& current,
			std::size_t currentIndex, const Paragraph& desired,
			std::size_t desiredIndex)
		{
			if (current.GetRichTextStructureId()
					!= desired.GetRichTextStructureId()
				|| LocalStyleOf(current) != LocalStyleOf(desired)
				|| LocalParagraphStyleOf(current)
					!= LocalParagraphStyleOf(desired))
			{
				return false;
			}
			const bool currentHasBreak =
				currentIndex + 1 < _blocks._items.size();
			const bool desiredHasBreak =
				desiredIndex + 1 < replacement.size();
			if (currentHasBreak != desiredHasBreak) return false;
			if (currentHasBreak
				&& (current._breakStructureId != desired._breakStructureId
					|| current._reconstructedBreakStyle
						!= desired._reconstructedBreakStyle))
			{
				return false;
			}
			if (current.GetInlines().Count() != desired.GetInlines().Count())
				return false;
			for (std::size_t index = 0;
				index < current.GetInlines().Count(); ++index)
			{
				const auto* currentChild = current.GetInlines().At(index);
				const auto* desiredChild = desired.GetInlines().At(index);
				if (!currentChild || !desiredChild
					|| !SameInlineSubtree(*currentChild, *desiredChild))
				{
					return false;
				}
			}
			return true;
		};
		auto swapBlockOwnership = [&](std::unique_ptr<Block>& current,
			std::unique_ptr<Block>& desired)
		{
			journal.push_back(JournalAction{
				JournalActionKind::BlockOwnership,
				&current, &desired });
			current.swap(desired);
		};
		auto undoJournalAction = [](const JournalAction& action) noexcept
		{
			switch (action.Kind)
			{
			case JournalActionKind::BlockOwnership:
				static_cast<std::unique_ptr<Block>*>(action.Current)->swap(
					*static_cast<std::unique_ptr<Block>*>(action.Desired));
				break;
			case JournalActionKind::InlineOwnership:
				static_cast<std::unique_ptr<Inline>*>(action.Current)->swap(
					*static_cast<std::unique_ptr<Inline>*>(action.Desired));
				break;
			case JournalActionKind::InlineStorage:
				static_cast<InlineCollection::Storage*>(action.Current)->swap(
					*static_cast<InlineCollection::Storage*>(action.Desired));
				break;
			case JournalActionKind::ParagraphMetadata:
			{
				auto* current = static_cast<Paragraph*>(action.Current);
				auto* desired = static_cast<Paragraph*>(action.Desired);
				std::swap(current->_reconstructedBreakStyle,
					desired->_reconstructedBreakStyle);
				std::swap(current->_breakStructureId,
					desired->_breakStructureId);
				break;
			}
			}
		};

		const auto previousChangeDepth = _changeDepth;
		const bool previousChangePending = _changePending;
		bool rootSwapped = false;
		bool publicationCommitted = false;
		BeginOwnerProjectionTransaction();
		BeginChange();
		_mutationActive = true;
		try
		{
			for (std::size_t desiredIndex = 0;
				desiredIndex < replacement.size(); ++desiredIndex)
			{
				auto& desiredBlock = replacement[desiredIndex];
				auto* desiredParagraph =
					dynamic_cast<Paragraph*>(desiredBlock.get());
				const auto desiredId =
					desiredParagraph->GetRichTextStructureId();
				auto found = std::find_if(
					_blocks._items.begin(), _blocks._items.end(),
					[&](const auto& candidate)
					{
						return candidate
							&& candidate->GetRichTextStructureId()
								== desiredId
							&& typeid(*candidate) == typeid(Paragraph)
							&& LocalStyleOf(*candidate)
								== LocalStyleOf(*desiredParagraph)
							&& LocalParagraphStyleOf(*candidate)
								== LocalParagraphStyleOf(*desiredParagraph);
					});
				if (found == _blocks._items.end()) continue;
				auto* currentParagraph =
					static_cast<Paragraph*>(found->get());
				const auto currentIndex = static_cast<std::size_t>(
					found - _blocks._items.begin());
				if (sameParagraphSubtree(
					*currentParagraph, currentIndex,
					*desiredParagraph, desiredIndex))
				{
					swapBlockOwnership(*found, desiredBlock);
				}
				else
				{
					reconcileInlines(reconcileInlines,
						currentParagraph->GetInlines(),
						desiredParagraph->GetInlines());
					journal.push_back(JournalAction{
						JournalActionKind::InlineStorage,
						&currentParagraph->GetInlines()._items,
						&desiredParagraph->GetInlines()._items });
					currentParagraph->GetInlines()._items.swap(
						desiredParagraph->GetInlines()._items);
					journal.push_back(JournalAction{
						JournalActionKind::ParagraphMetadata,
						currentParagraph, desiredParagraph });
					std::swap(
						currentParagraph->_reconstructedBreakStyle,
						desiredParagraph->_reconstructedBreakStyle);
					std::swap(currentParagraph->_breakStructureId,
						desiredParagraph->_breakStructureId);
					swapBlockOwnership(*found, desiredBlock);
				}
			}

			_blocks._items.swap(replacement);
			rootSwapped = true;
			for (auto& newBlock : _blocks._items)
				if (newBlock) newBlock->SetTextTreeParent(this, this);
			for (auto& oldBlock : replacement)
				if (oldBlock) oldBlock->SetTextTreeParent(nullptr, nullptr);
			SynchronizeTextPointers(textChange);
			_changePending = true;
			EndChangeCore();
			publicationCommitted = true;
			CommitOwnerProjectionTransaction();
			_mutationActive = false;
		}
		catch (...)
		{
			if (publicationCommitted)
			{
				_mutationActive = false;
				throw;
			}
			if (rootSwapped) _blocks._items.swap(replacement);
			for (auto action = journal.rbegin();
				action != journal.rend(); ++action)
				undoJournalAction(*action);
			for (auto& original : _blocks._items)
				if (original)
					original->RestoreTextTreeParentNoThrow(this, this);
			for (auto& desired : replacement)
				if (desired)
					desired->RestoreTextTreeParentNoThrow(nullptr, nullptr);
			_changeDepth = previousChangeDepth;
			_changePending = previousChangePending;
			RollbackOwnerProjectionTransaction();
			_mutationActive = false;
			throw;
		}
		if (outError) outError->clear();
		return true;
	}

	const auto previousChangeDepth = _changeDepth;
	const bool previousChangePending = _changePending;
	bool swapped = false;
	bool publicationCommitted = false;
	BeginOwnerProjectionTransaction();
	BeginChange();
	_mutationActive = true;
	try
	{
		for (auto& oldBlock : _blocks._items)
			oldBlock->SetTextTreeParent(nullptr, nullptr);
		for (auto& newBlock : replacement)
			newBlock->SetTextTreeParent(this, this);
		_blocks._items.swap(replacement);
		swapped = true;
		SynchronizeTextPointers(textChange);
		_changePending = true;
		EndChangeCore();
		publicationCommitted = true;
		CommitOwnerProjectionTransaction();
		_mutationActive = false;
	}
	catch (...)
	{
		if (publicationCommitted)
		{
			_mutationActive = false;
			throw;
		}
		if (swapped) _blocks._items.swap(replacement);
		for (auto& oldBlock : _blocks._items)
			oldBlock->RestoreTextTreeParentNoThrow(this, this);
		for (auto& newBlock : replacement)
			newBlock->RestoreTextTreeParentNoThrow(nullptr, nullptr);
		_changeDepth = previousChangeDepth;
		_changePending = previousChangePending;
		RollbackOwnerProjectionTransaction();
		_mutationActive = false;
		throw;
	}
	if (outError) outError->clear();
	return true;
}
