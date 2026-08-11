#define NOMINMAX
#include "TextRange.h"

#include "FlowDocument.h"
#include "RichTextBox.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
	bool EqualTextToken(
		std::wstring_view left, std::wstring_view right) noexcept
	{
		if (left.size() != right.size()) return false;
		for (std::size_t index = 0; index < left.size(); ++index)
			if (towlower(left[index]) != towlower(right[index])) return false;
		return true;
	}

	RichTextCharacterStyle DocumentEffectiveBaseline(
		const FlowDocument& document)
	{
		RichTextCharacterStyle style;
		style.Foreground = document.GetForeground();
		style.Background = document.GetBackground();
		style.FontFamily = document.GetFontFamily();
		style.Language = document.GetLanguage();
		style.FontSize = static_cast<float>(document.GetFontSize());
		style.FontWeight = document.GetFontWeight();
		style.FontStretch = document.GetFontStretch();
		style.FontStyle = document.GetFontStyle();
		style.Underline = document.GetUnderline();
		style.Strikethrough = document.GetStrikethrough();
		return style;
	}

	RichTextParagraphStyle DocumentParagraphBaseline(
		const FlowDocument& document)
	{
		RichTextParagraphStyle style;
		style.TextAlignment = document.GetTextAlignment();
		style.FlowDirection = document.GetFlowDirection();
		return style;
	}

	RichTextCharacterStyle ResolveDocumentEffectiveStyle(
		const FlowDocument& document,
		RichTextCharacterStyle style)
	{
		const auto baseline = DocumentEffectiveBaseline(document);
		if (!style.Foreground) style.Foreground = baseline.Foreground;
		if (!style.Background) style.Background = baseline.Background;
		if (!style.FontFamily) style.FontFamily = baseline.FontFamily;
		if (!style.Language) style.Language = baseline.Language;
		if (!style.FontSize) style.FontSize = baseline.FontSize;
		if (!style.FontWeight) style.FontWeight = baseline.FontWeight;
		if (!style.FontStretch) style.FontStretch = baseline.FontStretch;
		if (!style.FontStyle) style.FontStyle = baseline.FontStyle;
		if (!style.Underline) style.Underline = baseline.Underline;
		if (!style.Strikethrough)
			style.Strikethrough = baseline.Strikethrough;
		return style;
	}

	RichTextCharacterStyle InsertionStyle(
		const FlowDocument& document,
		const RichTextDocument& flat,
		RichTextRange range)
	{
		if (!range.Empty()) return flat.StyleAt(range.Start);
		RichTextCharacterStyle paragraphStyle;
		if (document.TryGetParagraphInsertionStyleAt(
			range.Start, paragraphStyle))
		{
			return paragraphStyle;
		}
		if (flat.Empty())
		{
			RichTextCharacterStyle rootStyle;
			document.ApplyLocalCharacterStyle(rootStyle);
			return rootStyle;
		}
		return flat.InsertionStyleAt(
			range.Start, RichTextBoundaryAffinity::Backward);
	}

	RichTextFormatDelta ClearInlineFormattingDelta()
	{
		RichTextFormatDelta delta;
		delta.Foreground =
			RichTextFormatChange<cui::drawing::Brush>::Clear();
		delta.Background =
			RichTextFormatChange<cui::drawing::Brush>::Clear();
		delta.FontFamily = RichTextFormatChange<std::wstring>::Clear();
		delta.Language = RichTextFormatChange<std::wstring>::Clear();
		delta.FontSize = RichTextFormatChange<float>::Clear();
		delta.FontWeight =
			RichTextFormatChange<DWRITE_FONT_WEIGHT>::Clear();
		delta.FontStretch =
			RichTextFormatChange<DWRITE_FONT_STRETCH>::Clear();
		delta.FontStyle =
			RichTextFormatChange<DWRITE_FONT_STYLE>::Clear();
		delta.Underline = RichTextFormatChange<bool>::Clear();
		delta.Strikethrough = RichTextFormatChange<bool>::Clear();
		return delta;
	}
}

TextRange::TextRange(
	const TextPointer& position1,
	const TextPointer& position2)
{
	Select(position1, position2);
}

TextPointer TextRange::GetStart() const
{
	return _start;
}

TextPointer TextRange::GetEnd() const
{
	return _end;
}

FlowDocument& TextRange::RequireDocument() const
{
	const auto start = GetStart();
	const auto end = GetEnd();
	if (!start.IsValid() || !end.IsValid())
		throw std::logic_error(
			"TextRange is not attached to a live FlowDocument.");
	auto* document = start.GetDocument();
	if (!document || end.GetDocument() != document)
		throw std::invalid_argument(
			"TextRange endpoints must belong to the same FlowDocument.");
	return *document;
}

RichTextRange TextRange::GetNormalizedRange() const
{
	auto& document = RequireDocument();
	const auto start = GetStart().GetTextOffset();
	const auto end = GetEnd().GetTextOffset();
	const auto lower = (std::min)(start, end);
	const auto upper = (std::max)(start, end);
	RichTextDocument flat(document.Flatten());
	return flat.NormalizeRange(lower, upper - lower);
}

void TextRange::SetNormalizedPositions(
	FlowDocument& document,
	std::size_t start,
	std::size_t end)
{
	_start = document.CreateTextPointerAtTextOffset(
		start, LogicalDirection::Backward);
	_end = document.CreateTextPointerAtTextOffset(
		end, LogicalDirection::Forward);
}

bool TextRange::GetIsEmpty() const
{
	return GetStart().GetTextOffset() == GetEnd().GetTextOffset();
}

bool TextRange::Contains(const TextPointer& position) const
{
	auto& document = RequireDocument();
	if (!position.IsValid() || position.GetDocument() != &document)
		throw std::invalid_argument(
			"TextRange containment requires a position in the same document.");
	const auto range = GetNormalizedRange();
	const auto offset = position.GetTextOffset();
	return range.Start <= offset && offset <= range.End();
}

void TextRange::Select(
	const TextPointer& position1,
	const TextPointer& position2)
{
	if (!position1.IsValid() || !position2.IsValid())
		throw std::invalid_argument(
			"TextRange endpoints must be attached to a live document.");
	auto* document = position1.GetDocument();
	if (!document || position2.GetDocument() != document)
		throw std::invalid_argument(
			"TextRange endpoints must belong to the same FlowDocument.");

	RichTextDocument flat(document->Flatten());
	const auto first = position1.GetTextOffset();
	const auto second = position2.GetTextOffset();
	if (first == second)
	{
		const auto affinity = position2.GetLogicalDirection()
			== LogicalDirection::Backward
			? RichTextBoundaryAffinity::Backward
			: RichTextBoundaryAffinity::Forward;
		const auto snapped = flat.SnapToBoundary(first, affinity);
		SetNormalizedPositions(*document, snapped, snapped);
		return;
	}
	const auto lower = (std::min)(first, second);
	const auto upper = (std::max)(first, second);
	const auto range = flat.NormalizeRange(lower, upper - lower);
	SetNormalizedPositions(*document, range.Start, range.End());
}

std::wstring TextRange::GetText() const
{
	auto& document = RequireDocument();
	RichTextDocument flat(document.Flatten());
	const auto range = GetNormalizedRange();
	return flat.Extract(range.Start, range.Length).Text;
}

void TextRange::SetText(std::wstring value)
{
	auto& document = RequireDocument();
	RichTextDocument flat(document.Flatten());
	const auto range = GetNormalizedRange();
	SetNormalizedPositions(document, range.Start, range.End());
	auto replacement = RichTextDocumentFragment::FromPlainText(
		std::move(value), InsertionStyle(document, flat, range));
	if (auto* owner = document.GetOwner())
	{
		(void)owner->ReplaceTextRangeContent(
			range.Start, range.Length, std::move(replacement));
		return;
	}

	auto change = flat.Replace(
		range.Start, range.Length, replacement);
	if (!change.Changed()) return;
	std::wstring error;
	if (!document.ReplaceFromFragment(
		flat.ToFragment(), &error, true,
		change.TextChanged()
			? std::optional<TextPointerTextChange>(TextPointerTextChange{
				change.Start,
				change.Before.Text.size(),
				change.After.Text.size() })
			: std::nullopt))
	{
		throw std::runtime_error("Unable to replace TextRange content.");
	}
}

bool TextRange::TryBuildFormatDelta(
	const DependencyProperty& property,
	const BindingValue& value,
	RichTextFormatDelta& delta)
{
	if (&property == &TextElement::ForegroundProperty()
		|| &property == &TextElement::BackgroundProperty())
	{
		cui::drawing::Brush brush;
		if (!value.TryGet(brush))
		{
			D2D1_COLOR_F color{};
			if (!value.TryGet(color)) return false;
			brush = cui::drawing::MakeSolidColorBrush(color);
		}
		if (&property == &TextElement::ForegroundProperty())
			delta.Foreground =
				RichTextFormatChange<cui::drawing::Brush>::Set(
					std::move(brush));
		else
			delta.Background =
				RichTextFormatChange<cui::drawing::Brush>::Set(
					std::move(brush));
		return true;
	}
	if (&property == &TextElement::FontFamilyProperty())
	{
		std::wstring family;
		if (!value.TryGet(family) || family.empty()) return false;
		delta.FontFamily =
			RichTextFormatChange<std::wstring>::Set(std::move(family));
		return true;
	}
	if (&property == &TextElement::LanguageProperty())
	{
		std::wstring language;
		if (!value.TryGet(language)) return false;
		auto normalized = NormalizeRichTextLanguageTag(language);
		if (!normalized) return false;
		delta.Language = RichTextFormatChange<std::wstring>::Set(
			std::move(*normalized));
		return true;
	}
	if (&property == &TextElement::FontSizeProperty())
	{
		double size = 0.0;
		float floatSize = 0.0f;
		int integerSize = 0;
		if (value.TryGet(size)) floatSize = static_cast<float>(size);
		else if (!value.TryGet(floatSize))
		{
			if (!value.TryGet(integerSize)) return false;
			floatSize = static_cast<float>(integerSize);
		}
		if (!std::isfinite(floatSize)
			|| floatSize < (1.0f / 300.0f)
			|| floatSize > 160000.0f) return false;
		delta.FontSize = RichTextFormatChange<float>::Set(floatSize);
		return true;
	}
	if (&property == &TextElement::FontWeightProperty())
	{
		DWRITE_FONT_WEIGHT weight{};
		int numeric = 0;
		std::wstring token;
		if (!value.TryGet(weight))
		{
			if (value.TryGet(numeric))
				weight = static_cast<DWRITE_FONT_WEIGHT>(numeric);
			else if (value.TryGet(token))
			{
				if (EqualTextToken(token, L"Normal"))
					weight = DWRITE_FONT_WEIGHT_NORMAL;
				else if (EqualTextToken(token, L"SemiBold"))
					weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
				else if (EqualTextToken(token, L"Bold"))
					weight = DWRITE_FONT_WEIGHT_BOLD;
				else return false;
			}
			else return false;
		}
		if (static_cast<int>(weight) < 1
			|| static_cast<int>(weight) > 999) return false;
		delta.FontWeight =
			RichTextFormatChange<DWRITE_FONT_WEIGHT>::Set(weight);
		return true;
	}
	if (&property == &TextElement::FontStretchProperty())
	{
		DWRITE_FONT_STRETCH stretch{};
		int numeric = 0;
		std::wstring token;
		if (!value.TryGet(stretch))
		{
			if (value.TryGet(numeric))
				stretch = static_cast<DWRITE_FONT_STRETCH>(numeric);
			else if (value.TryGet(token))
			{
				if (EqualTextToken(token, L"UltraCondensed"))
					stretch = DWRITE_FONT_STRETCH_ULTRA_CONDENSED;
				else if (EqualTextToken(token, L"ExtraCondensed"))
					stretch = DWRITE_FONT_STRETCH_EXTRA_CONDENSED;
				else if (EqualTextToken(token, L"Condensed"))
					stretch = DWRITE_FONT_STRETCH_CONDENSED;
				else if (EqualTextToken(token, L"SemiCondensed"))
					stretch = DWRITE_FONT_STRETCH_SEMI_CONDENSED;
				else if (EqualTextToken(token, L"Normal")
					|| EqualTextToken(token, L"Medium"))
					stretch = DWRITE_FONT_STRETCH_NORMAL;
				else if (EqualTextToken(token, L"SemiExpanded"))
					stretch = DWRITE_FONT_STRETCH_SEMI_EXPANDED;
				else if (EqualTextToken(token, L"Expanded"))
					stretch = DWRITE_FONT_STRETCH_EXPANDED;
				else if (EqualTextToken(token, L"ExtraExpanded"))
					stretch = DWRITE_FONT_STRETCH_EXTRA_EXPANDED;
				else if (EqualTextToken(token, L"UltraExpanded"))
					stretch = DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
				else return false;
			}
			else return false;
		}
		if (stretch < DWRITE_FONT_STRETCH_ULTRA_CONDENSED
			|| stretch > DWRITE_FONT_STRETCH_ULTRA_EXPANDED)
			return false;
		delta.FontStretch =
			RichTextFormatChange<DWRITE_FONT_STRETCH>::Set(stretch);
		return true;
	}
	if (&property == &TextElement::FontStyleProperty())
	{
		DWRITE_FONT_STYLE style{};
		int numeric = 0;
		std::wstring token;
		if (!value.TryGet(style))
		{
			if (value.TryGet(numeric))
				style = static_cast<DWRITE_FONT_STYLE>(numeric);
			else if (value.TryGet(token))
			{
				if (EqualTextToken(token, L"Normal"))
					style = DWRITE_FONT_STYLE_NORMAL;
				else if (EqualTextToken(token, L"Oblique"))
					style = DWRITE_FONT_STYLE_OBLIQUE;
				else if (EqualTextToken(token, L"Italic"))
					style = DWRITE_FONT_STYLE_ITALIC;
				else return false;
			}
			else return false;
		}
		if (style != DWRITE_FONT_STYLE_NORMAL
			&& style != DWRITE_FONT_STYLE_OBLIQUE
			&& style != DWRITE_FONT_STYLE_ITALIC) return false;
		delta.FontStyle =
			RichTextFormatChange<DWRITE_FONT_STYLE>::Set(style);
		return true;
	}
	if (&property == &TextElement::UnderlineProperty()
		|| &property == &TextElement::StrikethroughProperty())
	{
		bool enabled = false;
		if (!value.TryGet(enabled)) return false;
		if (&property == &TextElement::UnderlineProperty())
			delta.Underline = RichTextFormatChange<bool>::Set(enabled);
		else
			delta.Strikethrough = RichTextFormatChange<bool>::Set(enabled);
		return true;
	}
	return false;
}

bool TextRange::TryBuildParagraphFormatDelta(
	const DependencyProperty& property,
	const BindingValue& value,
	RichTextParagraphFormatDelta& delta)
{
	if (&property == &Block::FlowDirectionProperty())
	{
		::FlowDirection direction{};
		int numeric = 0;
		std::wstring token;
		if (!value.TryGet(direction))
		{
			if (value.TryGet(numeric))
				direction = static_cast<::FlowDirection>(numeric);
			else if (value.TryGet(token))
			{
				if (EqualTextToken(token, L"LeftToRight"))
					direction = ::FlowDirection::LeftToRight;
				else if (EqualTextToken(token, L"RightToLeft"))
					direction = ::FlowDirection::RightToLeft;
				else return false;
			}
			else return false;
		}
		if (direction != ::FlowDirection::LeftToRight
			&& direction != ::FlowDirection::RightToLeft) return false;
		delta.FlowDirection =
			RichTextFormatChange<::FlowDirection>::Set(direction);
		return true;
	}
	if (&property != &Block::TextAlignmentProperty()) return false;
	::TextAlignment alignment{};
	int numeric = 0;
	std::wstring token;
	if (!value.TryGet(alignment))
	{
		if (value.TryGet(numeric))
			alignment = static_cast<::TextAlignment>(numeric);
		else if (value.TryGet(token))
		{
			if (EqualTextToken(token, L"Left"))
				alignment = ::TextAlignment::Left;
			else if (EqualTextToken(token, L"Right"))
				alignment = ::TextAlignment::Right;
			else if (EqualTextToken(token, L"Center"))
				alignment = ::TextAlignment::Center;
			else if (EqualTextToken(token, L"Justify"))
				alignment = ::TextAlignment::Justify;
			else return false;
		}
		else return false;
	}
	switch (alignment)
	{
	case ::TextAlignment::Left:
	case ::TextAlignment::Right:
	case ::TextAlignment::Center:
	case ::TextAlignment::Justify:
		delta.TextAlignment =
			RichTextFormatChange<::TextAlignment>::Set(alignment);
		return true;
	default:
		return false;
	}
}

bool TextRange::ApplyFormatDelta(const RichTextFormatDelta& delta)
{
	auto& document = RequireDocument();
	const auto range = GetNormalizedRange();
	if (range.Empty()) return true;
	SetNormalizedPositions(document, range.Start, range.End());
	if (auto* owner = document.GetOwner())
		return owner->ApplyTextRangeFormat(
			range.Start, range.Length, delta);

	RichTextDocument flat(document.Flatten());
	const auto change = flat.ApplyFormat(
		range.Start,
		range.Length,
		delta,
		DocumentEffectiveBaseline(document));
	if (!change.Changed()) return true;
	std::wstring error;
	if (!document.ReplaceFromFragment(flat.ToFragment(), &error, true))
	{
		throw std::runtime_error("Unable to format TextRange content.");
	}
	return true;
}

bool TextRange::ApplyParagraphFormatDelta(
	const RichTextParagraphFormatDelta& delta)
{
	auto& document = RequireDocument();
	const auto range = GetNormalizedRange();
	SetNormalizedPositions(document, range.Start, range.End());
	if (auto* owner = document.GetOwner())
		return owner->ApplyTextRangeParagraphFormat(
			range.Start, range.Length, delta);

	RichTextDocument flat(document.Flatten());
	const auto change = flat.ApplyParagraphFormat(
		range.Start, range.Length, delta,
		DocumentParagraphBaseline(document));
	if (!change.Changed()) return true;
	std::wstring error;
	if (!document.ReplaceFromFragment(flat.ToFragment(), &error, true))
		throw std::runtime_error("Unable to format TextRange paragraphs.");
	return true;
}

bool TextRange::ApplyPropertyValue(
	const DependencyProperty& property,
	const BindingValue& value)
{
	RichTextParagraphFormatDelta paragraphDelta;
	if (TryBuildParagraphFormatDelta(property, value, paragraphDelta))
		return ApplyParagraphFormatDelta(paragraphDelta);
	RichTextFormatDelta delta;
	return TryBuildFormatDelta(property, value, delta)
		&& ApplyFormatDelta(delta);
}

TextRangePropertyValue TextRange::GetPropertyValue(
	const DependencyProperty& property) const
{
	auto& document = RequireDocument();
	RichTextDocument flat(document.Flatten());
	const auto range = GetNormalizedRange();
	if (&property == &Block::TextAlignmentProperty()
		|| &property == &Block::FlowDirectionProperty())
	{
		const auto styles = flat.ParagraphStylesInRange(
			range.Start, range.Length, DocumentParagraphBaseline(document));
		if (styles.empty()) return {};
		if (&property == &Block::TextAlignmentProperty())
		{
			const auto common = styles.front().TextAlignment;
			for (const auto& style : styles)
				if (style.TextAlignment != common)
					return { TextRangePropertyValueKind::Mixed, {} };
			return common
				? TextRangePropertyValue{
					TextRangePropertyValueKind::Value, BindingValue(*common) }
				: TextRangePropertyValue{};
		}
		const auto common = styles.front().FlowDirection;
		for (const auto& style : styles)
			if (style.FlowDirection != common)
				return { TextRangePropertyValueKind::Mixed, {} };
		return common
			? TextRangePropertyValue{
				TextRangePropertyValueKind::Value, BindingValue(*common) }
			: TextRangePropertyValue{};
	}
	RichTextDocumentFragment fragment;
	if (!range.Empty())
	{
		fragment = flat.Extract(range.Start, range.Length);
	}
	else
	{
		RichTextCharacterStyle style;
		if (document.TryGetParagraphInsertionStyleAt(range.Start, style))
		{
			// Keep the structural effective style.
		}
		else if (!flat.Empty())
		{
			style = flat.InsertionStyleAt(
				range.Start, RichTextBoundaryAffinity::Backward);
		}
		fragment = RichTextDocumentFragment::FromPlainText(L"x", style);
	}
	for (auto& span : fragment.Spans)
	{
		span.Style = document.GetOwner()
			? document.GetOwner()->ResolveEffectiveCharacterStyle(
				std::move(span.Style))
			: ResolveDocumentEffectiveStyle(
				document, std::move(span.Style));
	}

	auto query = [&]<typename TValue>(
		std::optional<TValue> RichTextCharacterStyle::* member)
		-> TextRangePropertyValue
	{
		std::optional<TValue> common;
		bool initialized = false;
		for (const auto& span : fragment.Spans)
		{
			const auto& candidate = span.Style.*member;
			if (!initialized)
			{
				common = candidate;
				initialized = true;
			}
			else if (common != candidate)
			{
				return { TextRangePropertyValueKind::Mixed, {} };
			}
		}
		if (!initialized || !common)
			return { TextRangePropertyValueKind::Unset, {} };
		return { TextRangePropertyValueKind::Value,
			BindingValue(*common) };
	};

	if (&property == &TextElement::ForegroundProperty())
		return query(&RichTextCharacterStyle::Foreground);
	if (&property == &TextElement::BackgroundProperty())
		return query(&RichTextCharacterStyle::Background);
	if (&property == &TextElement::FontFamilyProperty())
		return query(&RichTextCharacterStyle::FontFamily);
	if (&property == &TextElement::LanguageProperty())
		return query(&RichTextCharacterStyle::Language);
	if (&property == &TextElement::FontSizeProperty())
		return query(&RichTextCharacterStyle::FontSize);
	if (&property == &TextElement::FontWeightProperty())
		return query(&RichTextCharacterStyle::FontWeight);
	if (&property == &TextElement::FontStretchProperty())
		return query(&RichTextCharacterStyle::FontStretch);
	if (&property == &TextElement::FontStyleProperty())
		return query(&RichTextCharacterStyle::FontStyle);
	if (&property == &TextElement::UnderlineProperty())
		return query(&RichTextCharacterStyle::Underline);
	if (&property == &TextElement::StrikethroughProperty())
		return query(&RichTextCharacterStyle::Strikethrough);
	return {};
}

void TextRange::ClearAllProperties()
{
	(void)ApplyFormatDelta(ClearInlineFormattingDelta());
}
