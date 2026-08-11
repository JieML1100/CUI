#include "RichTextDocument.h"
#include "TextBoundary.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace
{
	bool IsRichTextLanguageTagSyntax(
		std::wstring_view value, bool requireLowerCase) noexcept
	{
		if (value.empty()) return true;
		bool primary = true;
		std::size_t subtagLength = 0;
		for (const wchar_t character : value)
		{
			if (character == L'-')
			{
				if (subtagLength == 0 || subtagLength > 8) return false;
				primary = false;
				subtagLength = 0;
				continue;
			}
			if (character >= L'a' && character <= L'z')
			{
				// Canonical form already uses lower-case ASCII.
			}
			else if (character >= L'A' && character <= L'Z')
			{
				if (requireLowerCase) return false;
			}
			else if (character >= L'0' && character <= L'9')
			{
				if (primary) return false;
			}
			else return false;
			if (++subtagLength > 8) return false;
		}
		return subtagLength != 0;
	}

	bool IsHighSurrogate(wchar_t value) noexcept
	{
		const auto code = static_cast<unsigned int>(value);
		return code >= 0xD800 && code <= 0xDBFF;
	}

	bool IsLowSurrogate(wchar_t value) noexcept
	{
		const auto code = static_cast<unsigned int>(value);
		return code >= 0xDC00 && code <= 0xDFFF;
	}

	bool IsAtomicPair(wchar_t left, wchar_t right) noexcept
	{
		return (left == L'\r' && right == L'\n')
			|| (IsHighSurrogate(left) && IsLowSurrogate(right));
	}

	bool IsAtomicInterior(
		const std::wstring& text, std::size_t position) noexcept
	{
		return position > 0 && position < text.size()
			&& IsAtomicPair(text[position - 1], text[position]);
	}

	bool ValidateNormalizedText(const std::wstring& text) noexcept
	{
		for (std::size_t index = 0; index < text.size(); ++index)
		{
			const wchar_t value = text[index];
			if (value == L'\r')
			{
				if (index + 1 >= text.size() || text[index + 1] != L'\n')
					return false;
				++index;
				continue;
			}
			if (value == L'\n')
				return false;
			if (IsHighSurrogate(value))
			{
				if (index + 1 >= text.size()
					|| !IsLowSurrogate(text[index + 1]))
					return false;
				++index;
				continue;
			}
			if (IsLowSurrogate(value))
				return false;
		}
		return true;
	}

	std::wstring NormalizePlainText(std::wstring text)
	{
		std::wstring result;
		result.reserve(text.size());
		for (std::size_t index = 0; index < text.size(); ++index)
		{
			const wchar_t value = text[index];
			if (value == L'\r')
			{
				result.append(L"\r\n");
				if (index + 1 < text.size() && text[index + 1] == L'\n')
					++index;
			}
			else if (value == L'\n')
			{
				result.append(L"\r\n");
			}
			else
			{
				result.push_back(value);
			}
		}
		return result;
	}

	void AppendSpan(
		std::vector<RichTextStyleSpan>& spans,
		RichTextStyleSpan span)
	{
		if (span.Length == 0) return;
		if (!spans.empty())
		{
			auto& previous = spans.back();
			if (previous.End() != span.Start)
				throw std::logic_error(
					"Rich-text spans must remain gap-free while editing.");
			if (previous.Style == span.Style)
			{
				previous.Length += span.Length;
				return;
			}
		}
		spans.push_back(std::move(span));
	}

	void CanonicalizeSpans(
		const std::wstring& text,
		std::vector<RichTextStyleSpan>& spans)
	{
		if (text.empty())
		{
			spans.clear();
			return;
		}

		std::vector<RichTextStyleSpan> merged;
		merged.reserve(spans.size());
		for (auto& span : spans)
			AppendSpan(merged, std::move(span));
		spans = std::move(merged);

		// A replacement can place two previously separate text pieces next to
		// each other. If that join forms CRLF/a surrogate pair, keep the pair in
		// the left span so no formatting boundary bisects the atomic element.
		std::size_t index = 0;
		while (index + 1 < spans.size())
		{
			auto& left = spans[index];
			auto& right = spans[index + 1];
			if (IsAtomicInterior(text, left.End()))
			{
				++left.Length;
				++right.Start;
				--right.Length;
				if (right.Length == 0)
					spans.erase(spans.begin()
						+ static_cast<std::ptrdiff_t>(index + 1));
				if (index + 1 < spans.size()
					&& spans[index].Style == spans[index + 1].Style)
				{
					spans[index].Length += spans[index + 1].Length;
					spans.erase(spans.begin()
						+ static_cast<std::ptrdiff_t>(index + 1));
				}
				continue;
			}
			++index;
		}
	}

	void OverlayStyle(
		RichTextCharacterStyle& target,
		const RichTextCharacterStyle& source)
	{
		if (source.Foreground) target.Foreground = source.Foreground;
		if (source.Background) target.Background = source.Background;
		if (source.FontFamily) target.FontFamily = source.FontFamily;
		if (source.Language) target.Language = source.Language;
		if (source.FontSize) target.FontSize = source.FontSize;
		if (source.FontWeight) target.FontWeight = source.FontWeight;
		if (source.FontStretch) target.FontStretch = source.FontStretch;
		if (source.FontStyle) target.FontStyle = source.FontStyle;
		if (source.Underline) target.Underline = source.Underline;
		if (source.Strikethrough)
			target.Strikethrough = source.Strikethrough;
	}

	bool IsValidTextAlignment(::TextAlignment value) noexcept
	{
		switch (value)
		{
		case ::TextAlignment::Left:
		case ::TextAlignment::Right:
		case ::TextAlignment::Center:
		case ::TextAlignment::Justify:
			return true;
		default:
			return false;
		}
	}

	bool IsValidFlowDirection(::FlowDirection value) noexcept
	{
		switch (value)
		{
		case ::FlowDirection::LeftToRight:
		case ::FlowDirection::RightToLeft:
			return true;
		default:
			return false;
		}
	}

	void OverlayParagraphStyle(
		RichTextParagraphStyle& target,
		const RichTextParagraphStyle& source)
	{
		if (source.TextAlignment)
			target.TextAlignment = source.TextAlignment;
		if (source.FlowDirection)
			target.FlowDirection = source.FlowDirection;
	}

	RichTextParagraphStyle EffectiveParagraphStyle(
		const std::optional<RichTextParagraphStyle>& rootStyle,
		const std::vector<RichTextStructureNode>& path,
		const RichTextParagraphStyle* effectiveBaseline = nullptr)
	{
		RichTextParagraphStyle result = effectiveBaseline
			? *effectiveBaseline : RichTextParagraphStyle{};
		if (rootStyle) OverlayParagraphStyle(result, *rootStyle);
		if (!path.empty())
			OverlayParagraphStyle(result, path.front().LocalParagraphStyle);
		return result;
	}

	struct ParagraphRecord
	{
		std::uint64_t Id = 0;
		std::size_t Start = 0;
		std::size_t End = 0;
		RichTextParagraphStyle Style;
	};

	std::vector<ParagraphRecord> CollectParagraphRecords(
		const RichTextDocumentFragment& fragment,
		const RichTextParagraphStyle& effectiveBaseline)
	{
		std::vector<ParagraphRecord> result;
		auto append = [&](const std::vector<RichTextStructureNode>& path,
			std::size_t start, std::size_t end)
		{
			if (path.empty()) return;
			const auto id = path.front().Id;
			if (result.empty() || result.back().Id != id)
			{
				result.push_back(ParagraphRecord{
					id, start, end,
					EffectiveParagraphStyle(fragment.RootParagraphStyle,
						path, &effectiveBaseline) });
			}
			else
			{
				result.back().End = (std::max)(result.back().End, end);
			}
		};

		std::size_t spanIndex = 0;
		std::size_t markerIndex = 0;
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
				const auto& marker = fragment.StructureMarkers[markerIndex++];
				append(marker.Path, marker.Position, marker.Position);
			}
			else
			{
				const auto& span = fragment.StructureSpans[spanIndex++];
				append(span.Path, span.Start, span.End());
			}
		}
		return result;
	}

	std::vector<std::uint64_t> ParagraphIdsForRange(
		const RichTextDocumentFragment& fragment,
		RichTextRange range,
		const RichTextParagraphStyle& effectiveBaseline)
	{
		const auto records = CollectParagraphRecords(fragment, effectiveBaseline);
		std::vector<std::uint64_t> result;
		if (records.empty()) return result;
		if (range.Empty())
		{
			const ParagraphRecord* selected = nullptr;
			for (const auto& record : records)
			{
				if (record.Start == range.Start)
					selected = &record; // Forward gravity prefers the later paragraph.
				else if (record.Start < range.Start && range.Start < record.End)
					selected = &record;
				else if (record.Start > range.Start)
					break;
			}
			if (!selected) selected = &records.back();
			result.push_back(selected->Id);
			return result;
		}

		for (const auto& record : records)
		{
			const bool visibleIntersection = record.End > range.Start
				&& record.Start < range.End();
			const bool selectedEmpty = record.Start == record.End
				&& record.Start >= range.Start && record.Start < range.End();
			if (visibleIntersection || selectedEmpty)
				result.push_back(record.Id);
		}
		return result;
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

	RichTextCharacterStyle EffectiveStructureStyle(
		const std::optional<RichTextCharacterStyle>& rootStyle,
		const std::vector<RichTextStructureNode>& path,
		const RichTextCharacterStyle* effectiveBaseline = nullptr)
	{
		RichTextCharacterStyle result = effectiveBaseline
			? *effectiveBaseline : RichTextCharacterStyle{};
		if (rootStyle) OverlayStyle(result, *rootStyle);
		for (const auto& node : path)
		{
			ApplyIntrinsicStyle(node.Kind, result);
			OverlayStyle(result, node.LocalStyle);
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

	bool IsWrapperKind(RichTextStructureKind kind) noexcept
	{
		return kind == RichTextStructureKind::Span
			|| kind == RichTextStructureKind::Bold
			|| kind == RichTextStructureKind::Italic
			|| kind == RichTextStructureKind::Underline;
	}

	void AppendStructureSpan(
		std::vector<RichTextStructureSpan>& spans,
		RichTextStructureSpan span)
	{
		if (span.Length == 0) return;
		if (!spans.empty())
		{
			auto& previous = spans.back();
			if (previous.End() != span.Start)
				throw std::logic_error(
					"Rich-text structure spans must remain gap-free.");
			if (previous.Path == span.Path)
			{
				previous.Length += span.Length;
				return;
			}
		}
		spans.push_back(std::move(span));
	}

	void CanonicalizeStructureSpans(
		const std::wstring& text,
		std::vector<RichTextStructureSpan>& spans)
	{
		if (spans.empty()) return;
		std::vector<RichTextStructureSpan> merged;
		merged.reserve(spans.size());
		for (auto& span : spans)
			AppendStructureSpan(merged, std::move(span));
		spans = std::move(merged);

		std::size_t index = 0;
		while (index + 1 < spans.size())
		{
			auto& left = spans[index];
			auto& right = spans[index + 1];
			if (IsAtomicInterior(text, left.End()))
			{
				++left.Length;
				++right.Start;
				--right.Length;
				if (right.Length == 0)
					spans.erase(spans.begin()
						+ static_cast<std::ptrdiff_t>(index + 1));
				if (index + 1 < spans.size()
					&& spans[index].Path == spans[index + 1].Path)
				{
					spans[index].Length += spans[index + 1].Length;
					spans.erase(spans.begin()
						+ static_cast<std::ptrdiff_t>(index + 1));
				}
				continue;
			}
			++index;
		}
	}

	void CanonicalizeStructureMarkers(
		const std::wstring& text,
		std::vector<RichTextStructureMarker>& markers)
	{
		for (auto& marker : markers)
		{
			marker.Position = (std::min)(marker.Position, text.size());
			// A marker precedes the following visible leaf.  If a replacement
			// joins two halves into one atomic element, move that zero-width leaf
			// after the pair instead of leaving it inside CRLF/a surrogate.
			if (IsAtomicInterior(text, marker.Position)) ++marker.Position;
		}
		std::stable_sort(markers.begin(), markers.end(),
			[](const RichTextStructureMarker& left,
				const RichTextStructureMarker& right)
			{
				return left.Position < right.Position;
			});
	}

	void NormalizeDisconnectedStructureIdentities(
		std::vector<RichTextStructureSpan>& spans,
		std::vector<RichTextStructureMarker>& markers)
	{
		std::vector<RichTextStructureNode> previousOriginal;
		std::vector<RichTextStructureNode> previousAssigned;
		std::unordered_set<std::uint64_t> closed;
		std::size_t spanIndex = 0;
		std::size_t markerIndex = 0;
		while (spanIndex < spans.size() || markerIndex < markers.size())
		{
			const bool useMarker = markerIndex < markers.size()
				&& (spanIndex >= spans.size()
					|| markers[markerIndex].Position
						<= spans[spanIndex].Start);
			auto& path = useMarker
				? markers[markerIndex++].Path
				: spans[spanIndex++].Path;
			auto original = path;
			std::size_t shared = 0;
			while (shared < previousOriginal.size()
				&& shared < original.size()
				&& previousOriginal[shared] == original[shared])
				++shared;
			for (std::size_t depth = previousOriginal.size();
				depth > shared; --depth)
				closed.insert(previousOriginal[depth - 1].Id);

			for (std::size_t depth = 0; depth < shared; ++depth)
				path[depth].Id = previousAssigned[depth].Id;
			for (std::size_t depth = shared; depth < path.size(); ++depth)
			{
				if (closed.contains(path[depth].Id))
					path[depth].Id = AllocateRichTextStructureId();
			}
			previousOriginal = std::move(original);
			previousAssigned = path;
		}
	}

	void NormalizeParagraphStructure(
		const std::wstring& text,
		std::vector<RichTextStructureSpan>& spans,
		std::vector<RichTextStructureMarker>& markers)
	{
		if (spans.empty() && markers.empty()) return;
		RichTextStructureNode paragraph;
		std::uint64_t previousParagraphId = 0;
		bool startParagraph = true;
		std::size_t spanIndex = 0;
		std::size_t markerIndex = 0;
		while (spanIndex < spans.size() || markerIndex < markers.size())
		{
			const bool useMarker = markerIndex < markers.size()
				&& (spanIndex >= spans.size()
					|| markers[markerIndex].Position
						<= spans[spanIndex].Start);
			auto* span = useMarker ? nullptr : &spans[spanIndex++];
			auto& path = useMarker
				? markers[markerIndex++].Path : span->Path;
			if (path.empty())
				throw std::logic_error("Structured text path is empty.");
			if (startParagraph)
			{
				paragraph = path.front();
				if (paragraph.Kind != RichTextStructureKind::Paragraph)
					throw std::logic_error(
						"Structured text path has no Paragraph root.");
				if (paragraph.Id == previousParagraphId)
					paragraph.Id = AllocateRichTextStructureId();
				startParagraph = false;
			}
			path.front() = paragraph;

			const bool isParagraphBreak = span
				&& path.back().Kind
				== RichTextStructureKind::ParagraphBreak;
			if (isParagraphBreak)
			{
				if (span->Length != 2
					|| text.compare(span->Start, 2, L"\r\n") != 0)
					throw std::logic_error(
						"ParagraphBreak must cover one canonical CRLF.");
				RichTextStructureNode breakNode{
					AllocateRichTextStructureId(),
					RichTextStructureKind::ParagraphBreak, {} };
				breakNode = path.back();
				path = { paragraph, std::move(breakNode) };
				previousParagraphId = paragraph.Id;
				startParagraph = true;
			}
		}
	}

	template<typename T>
	void ApplyFormatChange(
		std::optional<T>& target,
		const RichTextFormatChange<T>& change)
	{
		switch (change.Mode)
		{
		case RichTextFormatDeltaMode::Keep:
			break;
		case RichTextFormatDeltaMode::Set:
			target = change.Value;
			break;
		case RichTextFormatDeltaMode::Clear:
			target.reset();
			break;
		}
	}

	template<typename T>
	void ApplyStructuredFormatChange(
		std::optional<T>& target,
		const std::optional<T>& effective,
		const RichTextFormatChange<T>& change)
	{
		switch (change.Mode)
		{
		case RichTextFormatDeltaMode::Keep:
			break;
		case RichTextFormatDeltaMode::Set:
			// WPF direct formatting is a no-op when the requested value is
			// already effective through an ancestor/type style.  Do not create
			// a redundant Run-local override that would freeze inheritance.
			if (effective != change.Value) target = change.Value;
			break;
		case RichTextFormatDeltaMode::Clear:
			target.reset();
			break;
		}
	}

	void ApplyStructuredFormatDelta(
		RichTextCharacterStyle& target,
		const RichTextCharacterStyle& effective,
		const RichTextFormatDelta& delta)
	{
		ApplyStructuredFormatChange(
			target.Foreground, effective.Foreground, delta.Foreground);
		ApplyStructuredFormatChange(
			target.Background, effective.Background, delta.Background);
		ApplyStructuredFormatChange(
			target.FontFamily, effective.FontFamily, delta.FontFamily);
		ApplyStructuredFormatChange(
			target.Language, effective.Language, delta.Language);
		ApplyStructuredFormatChange(
			target.FontSize, effective.FontSize, delta.FontSize);
		ApplyStructuredFormatChange(
			target.FontWeight, effective.FontWeight, delta.FontWeight);
		ApplyStructuredFormatChange(
			target.FontStretch, effective.FontStretch, delta.FontStretch);
		ApplyStructuredFormatChange(
			target.FontStyle, effective.FontStyle, delta.FontStyle);
		ApplyStructuredFormatChange(
			target.Underline, effective.Underline, delta.Underline);
		ApplyStructuredFormatChange(target.Strikethrough,
			effective.Strikethrough, delta.Strikethrough);
	}
}

bool IsCanonicalRichTextLanguageTag(std::wstring_view value) noexcept
{
	return IsRichTextLanguageTagSyntax(value, true);
}

std::optional<std::wstring> NormalizeRichTextLanguageTag(
	std::wstring_view value)
{
	if (!IsRichTextLanguageTagSyntax(value, false)) return std::nullopt;
	std::wstring result(value);
	for (auto& character : result)
		if (character >= L'A' && character <= L'Z')
			character = static_cast<wchar_t>(character - L'A' + L'a');
	return result;
}

std::uint64_t AllocateRichTextStructureId() noexcept
{
	static std::atomic<std::uint64_t> next{ 1 };
	auto value = next.fetch_add(1, std::memory_order_relaxed);
	if (value == 0)
		value = next.fetch_add(1, std::memory_order_relaxed);
	return value;
}

bool RichTextCharacterStyle::Validate() const noexcept
{
	if (FontFamily && FontFamily->empty()) return false;
	if (Language && !IsCanonicalRichTextLanguageTag(*Language)) return false;
	if (FontSize && (!std::isfinite(*FontSize)
		|| *FontSize < (1.0f / 300.0f) || *FontSize > 160000.0f))
		return false;
	if (FontStretch
		&& (*FontStretch < DWRITE_FONT_STRETCH_ULTRA_CONDENSED
			|| *FontStretch > DWRITE_FONT_STRETCH_ULTRA_EXPANDED))
		return false;
	return true;
}

bool RichTextParagraphStyle::Validate() const noexcept
{
	return (!TextAlignment || IsValidTextAlignment(*TextAlignment))
		&& (!FlowDirection || IsValidFlowDirection(*FlowDirection));
}

bool RichTextFormatDelta::Empty() const noexcept
{
	return Foreground.Mode == RichTextFormatDeltaMode::Keep
		&& Background.Mode == RichTextFormatDeltaMode::Keep
		&& FontFamily.Mode == RichTextFormatDeltaMode::Keep
		&& Language.Mode == RichTextFormatDeltaMode::Keep
		&& FontSize.Mode == RichTextFormatDeltaMode::Keep
		&& FontWeight.Mode == RichTextFormatDeltaMode::Keep
		&& FontStretch.Mode == RichTextFormatDeltaMode::Keep
		&& FontStyle.Mode == RichTextFormatDeltaMode::Keep
		&& Underline.Mode == RichTextFormatDeltaMode::Keep
		&& Strikethrough.Mode == RichTextFormatDeltaMode::Keep;
}

bool RichTextFormatDelta::Validate() const noexcept
{
	if (!Foreground.Validate() || !Background.Validate()
		|| !FontFamily.Validate() || !Language.Validate()
		|| !FontSize.Validate()
		|| !FontWeight.Validate() || !FontStretch.Validate()
		|| !FontStyle.Validate()
		|| !Underline.Validate() || !Strikethrough.Validate())
		return false;
	if (FontFamily.Mode == RichTextFormatDeltaMode::Set
		&& FontFamily.Value->empty())
		return false;
	if (Language.Mode == RichTextFormatDeltaMode::Set
		&& !IsCanonicalRichTextLanguageTag(*Language.Value))
		return false;
	if (FontSize.Mode == RichTextFormatDeltaMode::Set
		&& (!std::isfinite(*FontSize.Value)
			|| *FontSize.Value < (1.0f / 300.0f)
			|| *FontSize.Value > 160000.0f))
		return false;
	if (FontStretch.Mode == RichTextFormatDeltaMode::Set
		&& (*FontStretch.Value < DWRITE_FONT_STRETCH_ULTRA_CONDENSED
			|| *FontStretch.Value > DWRITE_FONT_STRETCH_ULTRA_EXPANDED))
		return false;
	return true;
}

RichTextCharacterStyle RichTextFormatDelta::ApplyTo(
	const RichTextCharacterStyle& source) const
{
	if (!Validate())
		throw std::invalid_argument("Invalid rich-text format delta.");
	RichTextCharacterStyle result = source;
	ApplyFormatChange(result.Foreground, Foreground);
	ApplyFormatChange(result.Background, Background);
	ApplyFormatChange(result.FontFamily, FontFamily);
	ApplyFormatChange(result.Language, Language);
	ApplyFormatChange(result.FontSize, FontSize);
	ApplyFormatChange(result.FontWeight, FontWeight);
	ApplyFormatChange(result.FontStretch, FontStretch);
	ApplyFormatChange(result.FontStyle, FontStyle);
	ApplyFormatChange(result.Underline, Underline);
	ApplyFormatChange(result.Strikethrough, Strikethrough);
	return result;
}

bool RichTextParagraphFormatDelta::Empty() const noexcept
{
	return TextAlignment.Mode == RichTextFormatDeltaMode::Keep
		&& FlowDirection.Mode == RichTextFormatDeltaMode::Keep;
}

bool RichTextParagraphFormatDelta::Validate() const noexcept
{
	return TextAlignment.Validate() && FlowDirection.Validate()
		&& (TextAlignment.Mode != RichTextFormatDeltaMode::Set
			|| IsValidTextAlignment(*TextAlignment.Value))
		&& (FlowDirection.Mode != RichTextFormatDeltaMode::Set
			|| IsValidFlowDirection(*FlowDirection.Value));
}

RichTextParagraphStyle RichTextParagraphFormatDelta::ApplyTo(
	const RichTextParagraphStyle& source) const
{
	if (!Validate())
		throw std::invalid_argument("Invalid rich-text paragraph format delta.");
	RichTextParagraphStyle result = source;
	ApplyFormatChange(result.TextAlignment, TextAlignment);
	ApplyFormatChange(result.FlowDirection, FlowDirection);
	return result;
}

RichTextDocumentFragment RichTextDocumentFragment::FromPlainText(
	std::wstring text,
	RichTextCharacterStyle style)
{
	if (!style.Validate())
		throw std::invalid_argument("Invalid rich-text character style.");
	RichTextDocumentFragment result;
	result.Text = NormalizePlainText(std::move(text));
	if (!ValidateNormalizedText(result.Text))
		throw std::invalid_argument("Plain text is not valid UTF-16.");
	if (!result.Text.empty())
		result.Spans.push_back(
			RichTextStyleSpan{ 0, result.Text.size(), std::move(style) });
	return result;
}

bool RichTextDocumentFragment::ValidateCanonical() const noexcept
{
	if (!ValidateNormalizedText(Text)) return false;
	if (RootStyle && !RootStyle->Validate()) return false;
	if (RootParagraphStyle && !RootParagraphStyle->Validate()) return false;
	if (StructureRootId && *StructureRootId == 0) return false;
	const bool hasStructure = !StructureSpans.empty()
		|| !StructureMarkers.empty();
	if (Text.empty())
	{
		if (!Spans.empty() || !StructureSpans.empty()) return false;
	}
	else
	{
		if (Spans.empty()) return false;
		std::size_t cursor = 0;
		const RichTextCharacterStyle* previousStyle = nullptr;
		for (const auto& span : Spans)
		{
			if (!span.Style.Validate() || span.Length == 0
				|| span.Start != cursor || span.Start > Text.size()
				|| span.Length > Text.size() - span.Start)
				return false;
			if (IsAtomicInterior(Text, span.Start)
				|| IsAtomicInterior(Text, span.End()))
				return false;
			if (previousStyle && *previousStyle == span.Style)
				return false;
			cursor = span.End();
			previousStyle = &span.Style;
		}
		if (cursor != Text.size()) return false;
	}
	// Portable attributed strings carry complete character values in Spans and
	// no process-local tree identity. Structured fragments always carry both
	// the authored root style and their originating FlowDocument token.
	if (!hasStructure)
		return !RootStyle && !StructureRootId && !RootParagraphStyle;
	if (!RootStyle || !StructureRootId) return false;

	try
	{
		struct NodeIdentity
		{
			RichTextStructureKind Kind = RichTextStructureKind::Run;
			RichTextCharacterStyle LocalStyle;
			RichTextParagraphStyle LocalParagraphStyle;
			std::uint64_t ParentId = 0;
		};
		std::unordered_map<std::uint64_t, NodeIdentity> identities;
		std::unordered_set<std::uint64_t> closedIdentities;
		std::vector<std::uint64_t> activePath;
		std::size_t structureCursor = 0;
		std::size_t styleIndex = 0;
		std::uint64_t currentParagraphId = 0;
		std::uint64_t previousParagraphId = 0;
		bool expectNewParagraph = true;
		std::size_t structureIndex = 0;
		std::size_t markerIndex = 0;
		std::size_t previousMarkerPosition = 0;
		bool hasPreviousMarker = false;
		std::vector<RichTextStructureNode> previousEventPath;
		while (structureIndex < StructureSpans.size()
			|| markerIndex < StructureMarkers.size())
		{
			const bool useMarker = markerIndex < StructureMarkers.size()
				&& (structureIndex >= StructureSpans.size()
					|| StructureMarkers[markerIndex].Position
						<= StructureSpans[structureIndex].Start);
			const RichTextStructureSpan* span = useMarker
				? nullptr : &StructureSpans[structureIndex++];
			const RichTextStructureMarker* marker = useMarker
				? &StructureMarkers[markerIndex++] : nullptr;
			const auto& path = useMarker ? marker->Path : span->Path;
			if (path.empty()
				|| path.front().Kind != RichTextStructureKind::Paragraph)
				return false;

			if (marker)
			{
				if (marker->Position > Text.size()
					|| IsAtomicInterior(Text, marker->Position)
					|| (hasPreviousMarker
						&& marker->Position < previousMarkerPosition))
					return false;
				previousMarkerPosition = marker->Position;
				hasPreviousMarker = true;
				const auto terminal = path.back().Kind;
				if ((terminal == RichTextStructureKind::Paragraph
						&& path.size() != 1)
					|| (terminal != RichTextStructureKind::Paragraph
						&& terminal != RichTextStructureKind::Run
						&& !IsWrapperKind(terminal)))
					return false;
			}
			else
			{
				if (span->Length == 0 || span->Start != structureCursor
					|| span->Start > Text.size()
					|| span->Length > Text.size() - span->Start
					|| IsAtomicInterior(Text, span->Start)
					|| IsAtomicInterior(Text, span->End()))
					return false;

				const auto terminalKind = path.back().Kind;
				if (terminalKind == RichTextStructureKind::ParagraphBreak)
				{
					if (path.size() != 2 || span->Length != 2
						|| Text.compare(span->Start, 2, L"\r\n") != 0)
						return false;
				}
				else if (terminalKind == RichTextStructureKind::LineBreak)
				{
					if (path.size() < 2 || span->Length != 2
						|| Text.compare(span->Start, 2, L"\r\n") != 0)
						return false;
				}
				else if (terminalKind != RichTextStructureKind::Run)
					return false;
			}

			const auto paragraphId = path.front().Id;
			if (expectNewParagraph)
			{
				if (paragraphId == previousParagraphId) return false;
				currentParagraphId = paragraphId;
				expectNewParagraph = false;
			}
			else if (paragraphId != currentParagraphId)
				return false;
			if (span && path.back().Kind
				== RichTextStructureKind::ParagraphBreak)
			{
				previousParagraphId = currentParagraphId;
				expectNewParagraph = true;
			}

			std::size_t sharedDepth = 0;
			while (sharedDepth < activePath.size()
				&& sharedDepth < path.size()
				&& activePath[sharedDepth] == path[sharedDepth].Id)
				++sharedDepth;
			for (std::size_t depth = activePath.size();
				depth > sharedDepth; --depth)
				closedIdentities.insert(activePath[depth - 1]);
			activePath.resize(sharedDepth);

			for (std::size_t depth = 0; depth < path.size(); ++depth)
			{
				const auto& node = path[depth];
				if (node.Id == 0 || !node.LocalStyle.Validate()
					|| !node.LocalParagraphStyle.Validate()) return false;
				if (node.Kind != RichTextStructureKind::Paragraph
					&& (node.LocalParagraphStyle.TextAlignment
						|| node.LocalParagraphStyle.FlowDirection)) return false;
				if (depth >= sharedDepth && closedIdentities.contains(node.Id))
					return false;
				if (depth > 0 && depth + 1 < path.size()
					&& !IsWrapperKind(node.Kind)) return false;
				const std::uint64_t parentId = depth == 0
					? 0 : path[depth - 1].Id;
				const auto [found, inserted] = identities.emplace(
					node.Id, NodeIdentity{
						node.Kind, node.LocalStyle,
						node.LocalParagraphStyle, parentId });
				if (!inserted
					&& (found->second.Kind != node.Kind
						|| found->second.LocalStyle != node.LocalStyle
						|| found->second.LocalParagraphStyle
							!= node.LocalParagraphStyle
						|| found->second.ParentId != parentId))
					return false;
				if (depth >= activePath.size()) activePath.push_back(node.Id);
			}
			if (!previousEventPath.empty() && previousEventPath == path)
				return false;
			previousEventPath = path;

			if (span)
			{
				const auto effective = EffectiveStructureStyle(
					RootStyle, path);
				while (styleIndex < Spans.size()
					&& Spans[styleIndex].End() <= span->Start)
					++styleIndex;
				std::size_t overlappingStyle = styleIndex;
				while (overlappingStyle < Spans.size()
					&& Spans[overlappingStyle].Start < span->End())
				{
					if (Spans[overlappingStyle].Style != effective)
						return false;
					++overlappingStyle;
				}
				structureCursor = span->End();
			}
		}
		return structureCursor == Text.size();
	}
	catch (...)
	{
		return false;
	}
}

RichTextDocument::RichTextDocument(
	std::wstring text,
	RichTextCharacterStyle style)
	: RichTextDocument(RichTextDocumentFragment::FromPlainText(
		std::move(text), std::move(style)))
{
}

RichTextDocument::RichTextDocument(RichTextDocumentFragment fragment)
{
	if (!fragment.ValidateCanonical())
		throw std::invalid_argument("Rich-text fragment is not canonical.");
	_text = std::move(fragment.Text);
	_spans = std::move(fragment.Spans);
	_structureSpans = std::move(fragment.StructureSpans);
	_rootStyle = std::move(fragment.RootStyle);
	_structureRootId = fragment.StructureRootId;
	_structureMarkers = std::move(fragment.StructureMarkers);
	_rootParagraphStyle = std::move(fragment.RootParagraphStyle);
}

RichTextDocumentFragment RichTextDocument::ToFragment() const
{
	return RichTextDocumentFragment{
		_text, _spans, _structureSpans, _rootStyle, _structureRootId,
		_structureMarkers, _rootParagraphStyle };
}

bool RichTextDocument::ValidateCanonical() const noexcept
{
	return ToFragment().ValidateCanonical();
}

std::size_t RichTextDocument::SnapToBoundary(
	std::size_t position,
	RichTextBoundaryAffinity affinity) const noexcept
{
	position = (std::min)(position, _text.size());
	return CuiTextBoundary::SnapToTextElementBoundary(
		_text, position,
		affinity == RichTextBoundaryAffinity::Forward, true);
}

RichTextRange RichTextDocument::NormalizeRange(
	std::size_t start,
	std::size_t length,
	RichTextBoundaryAffinity collapsedAffinity) const noexcept
{
	start = (std::min)(start, _text.size());
	length = (std::min)(length, _text.size() - start);
	if (length == 0)
		return RichTextRange{
			SnapToBoundary(start, collapsedAffinity), 0 };

	std::size_t end = start + length;
	start = CuiTextBoundary::SnapToTextElementBoundary(
		_text, start, false, true);
	end = CuiTextBoundary::SnapToTextElementBoundary(
		_text, end, true, true);
	return RichTextRange{ start, end - start };
}

RichTextCharacterStyle RichTextDocument::StyleAt(
	std::size_t textIndex) const
{
	if (textIndex >= _text.size())
		throw std::out_of_range("Rich-text style index is outside the document.");
	auto it = std::upper_bound(
		_spans.begin(), _spans.end(), textIndex,
		[](std::size_t value, const RichTextStyleSpan& span)
		{
			return value < span.Start;
		});
	if (it == _spans.begin())
		throw std::logic_error("Rich-text spans do not cover the document.");
	--it;
	return it->Style;
}

RichTextCharacterStyle RichTextDocument::InsertionStyleAt(
	std::size_t position,
	RichTextBoundaryAffinity affinity) const
{
	if (_text.empty()) return {};
	position = SnapToBoundary(position, affinity);
	if (affinity == RichTextBoundaryAffinity::Backward)
		return StyleAt(position > 0 ? position - 1 : 0);
	return StyleAt(position < _text.size() ? position : _text.size() - 1);
}

RichTextDocumentFragment RichTextDocument::ExtractExact(
	RichTextRange range) const
{
	RichTextDocumentFragment result;
	if (range.Start > _text.size()
		|| range.Length > _text.size() - range.Start)
		throw std::out_of_range("Rich-text extraction range is invalid.");
	if (range.Empty()) return result;

	result.Text = _text.substr(range.Start, range.Length);
	const std::size_t end = range.End();
	for (const auto& span : _spans)
	{
		const std::size_t intersectionStart =
			(std::max)(span.Start, range.Start);
		const std::size_t intersectionEnd =
			(std::min)(span.End(), end);
		if (intersectionEnd <= intersectionStart) continue;
		AppendSpan(result.Spans, RichTextStyleSpan{
			intersectionStart - range.Start,
			intersectionEnd - intersectionStart,
			span.Style });
	}
	if (!_structureSpans.empty() || !_structureMarkers.empty())
	{
		result.RootStyle = _rootStyle;
		result.StructureRootId = _structureRootId;
		result.RootParagraphStyle = _rootParagraphStyle;
		for (const auto& span : _structureSpans)
		{
			const std::size_t intersectionStart =
				(std::max)(span.Start, range.Start);
			const std::size_t intersectionEnd =
				(std::min)(span.End(), end);
			if (intersectionEnd <= intersectionStart) continue;
			AppendStructureSpan(result.StructureSpans,
				RichTextStructureSpan{
					intersectionStart - range.Start,
					intersectionEnd - intersectionStart,
					span.Path });
		}
		const bool wholeDocument = range.Start == 0
			&& range.End() == _text.size();
		for (const auto& marker : _structureMarkers)
		{
			if (!wholeDocument
				&& (marker.Position <= range.Start
					|| marker.Position >= range.End()))
				continue;
			result.StructureMarkers.push_back(RichTextStructureMarker{
				marker.Position - range.Start, marker.Path });
		}
		if (result.StructureSpans.empty()
			&& result.StructureMarkers.empty())
		{
			result.RootStyle.reset();
			result.StructureRootId.reset();
			result.RootParagraphStyle.reset();
		}
	}
	if (!result.ValidateCanonical())
		throw std::logic_error(
			"Rich-text extraction produced a non-canonical fragment.");
	return result;
}

RichTextDocumentFragment RichTextDocument::Extract(
	std::size_t start,
	std::size_t length) const
{
	return ExtractExact(NormalizeRange(start, length));
}

RichTextDocumentFragment RichTextDocument::CreateLineBreakFragment(
	std::size_t position,
	const RichTextCharacterStyle& style,
	RichTextBoundaryAffinity affinity) const
{
	if (!style.Validate())
		throw std::invalid_argument("LineBreak style is invalid.");
	if (_structureSpans.empty() && _structureMarkers.empty())
		return RichTextDocumentFragment::FromPlainText(L"\r\n", style);
	position = SnapToBoundary(position, affinity);
	auto findAt = [&](std::size_t index)
		-> const RichTextStructureSpan*
	{
		const auto found = std::lower_bound(
			_structureSpans.begin(), _structureSpans.end(), index,
			[](const RichTextStructureSpan& span, std::size_t value)
			{
				return span.End() <= value;
			});
		return found != _structureSpans.end()
			&& found->Start <= index && index < found->End()
			? &*found : nullptr;
	};
	const RichTextStructureSpan* reference = nullptr;
	if (affinity == RichTextBoundaryAffinity::Backward && position > 0)
		reference = findAt(position - 1);
	if (!reference && position < _text.size()) reference = findAt(position);
	if (!reference && position > 0) reference = findAt(position - 1);
	const RichTextStructureMarker* markerReference = nullptr;
	if (!reference)
	{
		for (const auto& marker : _structureMarkers)
		{
			if (marker.Position < position) continue;
			if (marker.Position > position) break;
			markerReference = &marker;
			if (affinity == RichTextBoundaryAffinity::Forward) break;
		}
	}
	const auto* referencePath = reference ? &reference->Path
		: markerReference ? &markerReference->Path : nullptr;
	if (!referencePath || referencePath->empty())
		throw std::logic_error("LineBreak has no adjacent structure path.");
	auto path = *referencePath;
	const auto terminal = path.back().Kind;
	if (terminal == RichTextStructureKind::Run
		|| terminal == RichTextStructureKind::LineBreak
		|| terminal == RichTextStructureKind::ParagraphBreak)
	{
		path.pop_back();
	}
	if (path.empty()
		|| path.front().Kind != RichTextStructureKind::Paragraph)
		throw std::logic_error("LineBreak has no Paragraph context.");
	const auto inherited = EffectiveStructureStyle(_rootStyle, path);
	path.push_back(RichTextStructureNode{
		AllocateRichTextStructureId(), RichTextStructureKind::LineBreak,
		LocalOverridesAgainst(inherited, style) });
	RichTextDocumentFragment result;
	result.Text = L"\r\n";
	result.Spans = { RichTextStyleSpan{ 0, 2, style } };
	result.StructureSpans = {
		RichTextStructureSpan{ 0, 2, std::move(path) } };
	result.RootStyle = _rootStyle;
	result.StructureRootId = _structureRootId;
	result.RootParagraphStyle = _rootParagraphStyle;
	if (!result.ValidateCanonical())
		throw std::logic_error("LineBreak fragment is not canonical.");
	return result;
}

RichTextDocumentChange RichTextDocument::Replace(
	std::size_t start,
	std::size_t length,
	const RichTextDocumentFragment& replacement,
	RichTextBoundaryAffinity collapsedAffinity)
{
	if (!replacement.ValidateCanonical())
		throw std::invalid_argument("Replacement fragment is not canonical.");
	const RichTextRange range =
		NormalizeRange(start, length, collapsedAffinity);
	RichTextDocumentFragment inserted = replacement;
	const bool currentStructured = !_structureSpans.empty()
		|| !_structureMarkers.empty();
	const bool insertedStructured = !inserted.StructureSpans.empty()
		|| !inserted.StructureMarkers.empty();
	if (range.Empty() && inserted.Text.empty()
		&& !inserted.StructureMarkers.empty())
	{
		throw std::invalid_argument(
			"Zero-width structure insertion requires a text-tree collection API.");
	}
	// A partial edit cannot graft a text-tree path into an existing portable
	// attributed string because its untouched prefix/suffix have no structural
	// coverage. Keep the complete effective styles and deliberately discard the
	// process-local provenance. Empty/whole-document adoption remains lossless.
	if (!currentStructured && insertedStructured
		&& !_text.empty()
		&& (range.Start != 0 || range.Length != _text.size()))
	{
		inserted.StructureSpans.clear();
		inserted.StructureMarkers.clear();
		inserted.RootStyle.reset();
		inserted.StructureRootId.reset();
		inserted.RootParagraphStyle.reset();
	}

	// Portable/plain fragments intentionally carry no process-local tree ids.
	// When editing a structured document, infer the common Paragraph/Span path
	// and create a terminal Run whose local values reproduce the requested
	// effective style. Matching in-Run typing reuses that Run identity.
	std::optional<std::uint64_t> consumedMarkerId;
	if (currentStructured && insertedStructured && range.Empty()
		&& _structureSpans.empty() && !_structureMarkers.empty())
	{
		const RichTextStructureMarker* candidate = nullptr;
		for (const auto& marker : _structureMarkers)
		{
			if (marker.Position < range.Start) continue;
			if (marker.Position > range.Start) break;
			if (!candidate
				|| collapsedAffinity == RichTextBoundaryAffinity::Backward)
				candidate = &marker;
			if (collapsedAffinity == RichTextBoundaryAffinity::Forward) break;
		}
		if (candidate) consumedMarkerId = candidate->Path.back().Id;
	}
	if (currentStructured && !inserted.Text.empty()
		&& !insertedStructured)
	{
		auto findStructureAt = [&](std::size_t index)
			-> const RichTextStructureSpan*
		{
			const auto found = std::lower_bound(
				_structureSpans.begin(), _structureSpans.end(), index,
				[](const RichTextStructureSpan& span, std::size_t value)
				{
					return span.End() <= value;
				});
			return found != _structureSpans.end()
				&& found->Start <= index && index < found->End()
				? &*found : nullptr;
		};
		auto isInlineContentPath = [](
			const std::vector<RichTextStructureNode>* path)
		{
			return path && !path->empty()
				&& (path->back().Kind == RichTextStructureKind::Run
					|| path->back().Kind
						== RichTextStructureKind::LineBreak);
		};
		auto isRunPath = [](
			const std::vector<RichTextStructureNode>* path)
		{
			return path && !path->empty()
				&& path->back().Kind == RichTextStructureKind::Run;
		};

		const RichTextStructureSpan* reference = nullptr;
		const RichTextStructureMarker* markerReference = nullptr;
		if (!range.Empty())
		{
			for (const auto& span : _structureSpans)
			{
				if (span.End() <= range.Start) continue;
				if (span.Start >= range.End()) break;
				if (isInlineContentPath(&span.Path))
				{
					reference = &span;
					break;
				}
			}
		}
		else
		{
			if (range.Start > 0)
			{
				const auto* left = findStructureAt(range.Start - 1);
				if (left && isInlineContentPath(&left->Path)) reference = left;
			}
			if (!reference && range.Start < _text.size())
			{
				const auto* right = findStructureAt(range.Start);
				if (right && isInlineContentPath(&right->Path)) reference = right;
			}
			if (!reference)
			{
				for (const auto& marker : _structureMarkers)
				{
					if (marker.Position < range.Start) continue;
					if (marker.Position > range.Start) break;
					if (!markerReference
						|| collapsedAffinity
							== RichTextBoundaryAffinity::Backward)
						markerReference = &marker;
					if (collapsedAffinity
						== RichTextBoundaryAffinity::Forward) break;
				}
			}
		}
		const auto* referencePath = reference ? &reference->Path
			: markerReference ? &markerReference->Path : nullptr;
		if (markerReference)
			consumedMarkerId = markerReference->Path.back().Id;

		std::vector<RichTextStructureNode> commonPrefix;
		if (referencePath)
		{
			commonPrefix = *referencePath;
			if (!commonPrefix.empty()
				&& (commonPrefix.back().Kind == RichTextStructureKind::Run
					|| commonPrefix.back().Kind
						== RichTextStructureKind::LineBreak))
				commonPrefix.pop_back();
			if (!range.Empty())
			{
				for (const auto& span : _structureSpans)
				{
					if (span.End() <= range.Start) continue;
					if (span.Start >= range.End()) break;
					if (!isInlineContentPath(&span.Path))
					{
						commonPrefix.clear();
						break;
					}
					std::size_t shared = 0;
					const auto candidateCount = span.Path.size() - 1;
					while (shared < commonPrefix.size()
						&& shared < candidateCount
						&& commonPrefix[shared].Id
							== span.Path[shared].Id)
						++shared;
					commonPrefix.resize(shared);
				}
			}
		}
		if (commonPrefix.empty())
			commonPrefix.push_back(RichTextStructureNode{
				AllocateRichTextStructureId(),
				RichTextStructureKind::Paragraph, {} });

		bool canReuseRun = isRunPath(referencePath);
		if (canReuseRun && !range.Empty())
		{
			for (const auto& span : _structureSpans)
			{
				if (span.End() <= range.Start) continue;
				if (span.Start >= range.End()) break;
				if (!isRunPath(&span.Path)
					|| span.Path.back().Id != referencePath->back().Id)
				{
					canReuseRun = false;
					break;
				}
			}
		}
		if (inserted.Text.find_first_of(L"\r\n") != std::wstring::npos)
			canReuseRun = false;
		if (canReuseRun)
		{
			const auto referenceStyle = EffectiveStructureStyle(
				_rootStyle, *referencePath);
			for (const auto& span : inserted.Spans)
				if (span.Style != referenceStyle)
					canReuseRun = false;
		}

		inserted.RootStyle = _rootStyle;
		inserted.StructureRootId = _structureRootId;
		inserted.RootParagraphStyle = _rootParagraphStyle;
		std::vector<RichTextStructureNode> paragraphPrefix = commonPrefix;
		std::size_t cursor = 0;
		std::size_t styleIndex = 0;
		while (cursor < inserted.Text.size())
		{
			while (styleIndex + 1 < inserted.Spans.size()
				&& inserted.Spans[styleIndex].End() <= cursor)
				++styleIndex;
			const auto& styleSpan = inserted.Spans[styleIndex];
			if (inserted.Text[cursor] == L'\r'
				&& cursor + 1 < inserted.Text.size()
				&& inserted.Text[cursor + 1] == L'\n')
			{
				auto paragraphPath = std::vector<RichTextStructureNode>{
					paragraphPrefix.front() };
				const auto paragraphStyle = EffectiveStructureStyle(
					inserted.RootStyle, paragraphPath);
				paragraphPath.push_back(RichTextStructureNode{
					AllocateRichTextStructureId(),
					RichTextStructureKind::ParagraphBreak,
					LocalOverridesAgainst(
						paragraphStyle, styleSpan.Style) });
				AppendStructureSpan(inserted.StructureSpans,
					RichTextStructureSpan{ cursor, 2,
						std::move(paragraphPath) });
				cursor += 2;
				paragraphPrefix = commonPrefix;
				for (auto& node : paragraphPrefix)
					node.Id = AllocateRichTextStructureId();
				continue;
			}

			const auto nextBreak = inserted.Text.find_first_of(
				L"\r\n", cursor);
			const auto chunkEnd = (std::min)(styleSpan.End(),
				nextBreak == std::wstring::npos
					? inserted.Text.size() : nextBreak);
			if (chunkEnd <= cursor)
				throw std::logic_error(
					"Structured replacement did not make progress.");
			auto path = paragraphPrefix;
			if (canReuseRun)
				path = *referencePath;
			else
			{
				const auto inherited = EffectiveStructureStyle(
					inserted.RootStyle, path);
				path.push_back(RichTextStructureNode{
					AllocateRichTextStructureId(),
					RichTextStructureKind::Run,
					LocalOverridesAgainst(inherited, styleSpan.Style) });
			}
			AppendStructureSpan(inserted.StructureSpans,
				RichTextStructureSpan{ cursor, chunkEnd - cursor,
					std::move(path) });
			cursor = chunkEnd;
		}
		if (!inserted.ValidateCanonical())
			throw std::logic_error(
				"Unable to infer canonical rich-text structure.");
	}
	else if (currentStructured
		&& (!inserted.StructureSpans.empty()
			|| !inserted.StructureMarkers.empty())
		&& (inserted.RootStyle != _rootStyle
			|| inserted.StructureRootId != _structureRootId
			|| inserted.RootParagraphStyle != _rootParagraphStyle))
	{
		throw std::invalid_argument(
			"Structured fragments cannot cross FlowDocument roots.");
	}
	RichTextDocumentChange change;
	change.Start = range.Start;
	change.Before = ExtractExact(range);
	if (consumedMarkerId)
	{
		const auto found = std::find_if(
			_structureMarkers.begin(), _structureMarkers.end(),
			[&](const RichTextStructureMarker& marker)
			{
				return !marker.Path.empty()
					&& marker.Path.back().Id == *consumedMarkerId;
			});
		if (found == _structureMarkers.end())
			throw std::logic_error(
				"Consumed zero-width structure marker disappeared.");
		change.Before.RootStyle = _rootStyle;
		change.Before.StructureRootId = _structureRootId;
		change.Before.RootParagraphStyle = _rootParagraphStyle;
		change.Before.StructureMarkers.push_back(
			RichTextStructureMarker{ 0, found->Path });
	}
	change.After = inserted;
	if (change.Before == inserted) return change;

	std::wstring nextText = _text;
	nextText.replace(
		range.Start, range.Length, inserted.Text);

	std::vector<RichTextStyleSpan> nextSpans;
	nextSpans.reserve(_spans.size() + inserted.Spans.size() + 2);
	for (const auto& span : _spans)
	{
		if (span.Start >= range.Start) break;
		const std::size_t leftEnd = (std::min)(span.End(), range.Start);
		AppendSpan(nextSpans, RichTextStyleSpan{
			span.Start, leftEnd - span.Start, span.Style });
	}
	for (const auto& span : inserted.Spans)
	{
		AppendSpan(nextSpans, RichTextStyleSpan{
			range.Start + span.Start, span.Length, span.Style });
	}
	const std::size_t oldEnd = range.End();
	const std::size_t insertedEnd = range.Start + inserted.Text.size();
	for (const auto& span : _spans)
	{
		if (span.End() <= oldEnd) continue;
		const std::size_t sourceStart = (std::max)(span.Start, oldEnd);
		AppendSpan(nextSpans, RichTextStyleSpan{
			insertedEnd + (sourceStart - oldEnd),
			span.End() - sourceStart,
			span.Style });
	}
	CanonicalizeSpans(nextText, nextSpans);

	std::vector<RichTextStructureSpan> nextStructures;
	std::vector<RichTextStructureMarker> nextMarkers;
	std::optional<RichTextCharacterStyle> nextRootStyle;
	std::optional<RichTextParagraphStyle> nextRootParagraphStyle;
	std::optional<std::uint64_t> nextStructureRootId;
	const bool finalInsertedStructured = !inserted.StructureSpans.empty()
		|| !inserted.StructureMarkers.empty();
	if (currentStructured || finalInsertedStructured)
	{
		nextRootStyle = currentStructured
			? _rootStyle : inserted.RootStyle;
		nextRootParagraphStyle = currentStructured
			? _rootParagraphStyle : inserted.RootParagraphStyle;
		nextStructureRootId = currentStructured
			? _structureRootId : inserted.StructureRootId;
		nextStructures.reserve(_structureSpans.size()
			+ inserted.StructureSpans.size() + 2);
		for (const auto& span : _structureSpans)
		{
			if (span.Start >= range.Start) break;
			const auto leftEnd = (std::min)(span.End(), range.Start);
			AppendStructureSpan(nextStructures,
				RichTextStructureSpan{
					span.Start, leftEnd - span.Start, span.Path });
		}
		for (const auto& span : inserted.StructureSpans)
			AppendStructureSpan(nextStructures,
				RichTextStructureSpan{
					range.Start + span.Start, span.Length, span.Path });
		for (const auto& span : _structureSpans)
		{
			if (span.End() <= oldEnd) continue;
			const auto sourceStart = (std::max)(span.Start, oldEnd);
			AppendStructureSpan(nextStructures,
				RichTextStructureSpan{
					insertedEnd + (sourceStart - oldEnd),
					span.End() - sourceStart, span.Path });
		}

		nextMarkers.reserve(_structureMarkers.size()
			+ inserted.StructureMarkers.size());
		const bool replacingWholeDocument = !range.Empty()
			&& range.Start == 0 && oldEnd == _text.size();
		auto isConsumed = [&](const RichTextStructureMarker& marker)
		{
			return consumedMarkerId && !marker.Path.empty()
				&& marker.Path.back().Id == *consumedMarkerId;
		};
		for (const auto& marker : _structureMarkers)
		{
			if (replacingWholeDocument || isConsumed(marker)) continue;
			if (marker.Position > range.Start) continue;
			nextMarkers.push_back(marker);
		}
		for (const auto& marker : inserted.StructureMarkers)
			nextMarkers.push_back(RichTextStructureMarker{
				range.Start + marker.Position, marker.Path });
		for (const auto& marker : _structureMarkers)
		{
			if (replacingWholeDocument || isConsumed(marker)
				|| marker.Position <= range.Start
				|| marker.Position < oldEnd)
				continue;
			nextMarkers.push_back(RichTextStructureMarker{
				insertedEnd + (marker.Position - oldEnd), marker.Path });
		}
		CanonicalizeStructureSpans(nextText, nextStructures);
		CanonicalizeStructureMarkers(nextText, nextMarkers);
		NormalizeParagraphStructure(
			nextText, nextStructures, nextMarkers);
		CanonicalizeStructureSpans(nextText, nextStructures);
		CanonicalizeStructureMarkers(nextText, nextMarkers);
		NormalizeDisconnectedStructureIdentities(
			nextStructures, nextMarkers);
		if (nextStructures.empty() && nextMarkers.empty())
		{
			nextRootStyle.reset();
			nextRootParagraphStyle.reset();
			nextStructureRootId.reset();
		}
	}

	RichTextDocumentFragment next{
		std::move(nextText), std::move(nextSpans),
		std::move(nextStructures), std::move(nextRootStyle),
		std::move(nextStructureRootId), std::move(nextMarkers),
		std::move(nextRootParagraphStyle) };
	if (!next.ValidateCanonical())
		throw std::logic_error(
			"Rich-text replacement violated document invariants.");
	_text = std::move(next.Text);
	_spans = std::move(next.Spans);
	_structureSpans = std::move(next.StructureSpans);
	_rootStyle = std::move(next.RootStyle);
	_structureRootId = next.StructureRootId;
	_structureMarkers = std::move(next.StructureMarkers);
	_rootParagraphStyle = std::move(next.RootParagraphStyle);
	change.After = inserted.Text.empty()
		&& !inserted.StructureMarkers.empty()
		? inserted
		: ExtractExact(
			RichTextRange{ range.Start, inserted.Text.size() });
	return change;
}

RichTextDocumentChange RichTextDocument::ApplyFormat(
	std::size_t start,
	std::size_t length,
	const RichTextFormatDelta& delta,
	const RichTextCharacterStyle& effectiveBaseline)
{
	if (!delta.Validate())
		throw std::invalid_argument("Invalid rich-text format delta.");
	const RichTextRange range = NormalizeRange(start, length);
	RichTextDocumentChange unchanged;
	unchanged.Start = range.Start;
	unchanged.Before = ExtractExact(range);
	unchanged.After = unchanged.Before;
	if (range.Empty() || delta.Empty()) return unchanged;

	RichTextDocumentFragment formatted = unchanged.Before;
	if (!formatted.StructureSpans.empty())
	{
		const bool clearAll =
			delta.Foreground.Mode == RichTextFormatDeltaMode::Clear
			&& delta.Background.Mode == RichTextFormatDeltaMode::Clear
			&& delta.FontFamily.Mode == RichTextFormatDeltaMode::Clear
			&& delta.Language.Mode == RichTextFormatDeltaMode::Clear
			&& delta.FontSize.Mode == RichTextFormatDeltaMode::Clear
			&& delta.FontWeight.Mode == RichTextFormatDeltaMode::Clear
			&& delta.FontStretch.Mode == RichTextFormatDeltaMode::Clear
			&& delta.FontStyle.Mode == RichTextFormatDeltaMode::Clear
			&& delta.Underline.Mode == RichTextFormatDeltaMode::Clear
			&& delta.Strikethrough.Mode == RichTextFormatDeltaMode::Clear;
		formatted.Spans.clear();
		for (auto& span : formatted.StructureSpans)
		{
			const auto effectiveBefore = EffectiveStructureStyle(
				formatted.RootStyle, span.Path, &effectiveBaseline);
			const auto terminalKind = span.Path.back().Kind;
			if (clearAll
				&& (terminalKind == RichTextStructureKind::Run
					|| terminalKind == RichTextStructureKind::LineBreak)
				&& (span.Path.size() > 2
					|| span.Path.back().LocalStyle
						!= RichTextCharacterStyle{}))
			{
				span.Path = {
					span.Path.front(),
					RichTextStructureNode{
						AllocateRichTextStructureId(),
						terminalKind, {} } };
			}
			else
			{
				auto& terminal = span.Path.back();
				ApplyStructuredFormatDelta(
					terminal.LocalStyle, effectiveBefore, delta);
			}
			AppendSpan(formatted.Spans, RichTextStyleSpan{
				span.Start, span.Length,
				EffectiveStructureStyle(
					formatted.RootStyle, span.Path) });
		}
	}
	else
	{
		for (auto& span : formatted.Spans)
			span.Style = delta.ApplyTo(span.Style);
	}
	CanonicalizeSpans(formatted.Text, formatted.Spans);
	if (!formatted.ValidateCanonical())
		throw std::logic_error(
			"Rich-text formatting produced a non-canonical fragment.");
	return Replace(range.Start, range.Length, formatted);
}

RichTextDocumentChange RichTextDocument::AdjustFontSize(
	std::size_t start,
	std::size_t length,
	float amount,
	float minimum,
	float maximum,
	const RichTextCharacterStyle& effectiveBaseline)
{
	if (!std::isfinite(amount) || amount == 0.0f
		|| !std::isfinite(minimum) || !std::isfinite(maximum)
		|| minimum <= 0.0f || maximum < minimum
		|| !effectiveBaseline.Validate())
	{
		throw std::invalid_argument(
			"Invalid relative rich-text font-size adjustment.");
	}
	const RichTextRange range = NormalizeRange(start, length);
	RichTextDocumentChange unchanged;
	unchanged.Start = range.Start;
	unchanged.Before = ExtractExact(range);
	unchanged.After = unchanged.Before;
	if (range.Empty()) return unchanged;

	RichTextDocumentFragment formatted = unchanged.Before;
	auto adjustedSize = [amount, minimum, maximum](
		const std::optional<float>& current) -> std::optional<float>
	{
		if (!current) return std::nullopt;
		return (std::clamp)(*current + amount, minimum, maximum);
	};
	if (!formatted.StructureSpans.empty())
	{
		formatted.Spans.clear();
		for (auto& span : formatted.StructureSpans)
		{
			const auto effectiveBefore = EffectiveStructureStyle(
				formatted.RootStyle, span.Path, &effectiveBaseline);
			const auto target = adjustedSize(effectiveBefore.FontSize);
			if (target)
			{
				RichTextFormatDelta delta;
				delta.FontSize =
					RichTextFormatChange<float>::Set(*target);
				ApplyStructuredFormatDelta(
					span.Path.back().LocalStyle,
					effectiveBefore, delta);
			}
			AppendSpan(formatted.Spans, RichTextStyleSpan{
				span.Start, span.Length,
				EffectiveStructureStyle(
					formatted.RootStyle, span.Path) });
		}
	}
	else
	{
		for (auto& span : formatted.Spans)
		{
			const auto current = span.Style.FontSize
				? span.Style.FontSize : effectiveBaseline.FontSize;
			if (const auto target = adjustedSize(current))
				span.Style.FontSize = *target;
		}
	}
	CanonicalizeSpans(formatted.Text, formatted.Spans);
	if (!formatted.ValidateCanonical())
		throw std::logic_error(
			"Relative font-size formatting produced a non-canonical fragment.");
	return Replace(range.Start, range.Length, formatted);
}

std::vector<RichTextParagraphStyle> RichTextDocument::ParagraphStylesInRange(
	std::size_t start,
	std::size_t length,
	const RichTextParagraphStyle& effectiveBaseline) const
{
	if (!effectiveBaseline.Validate())
		throw std::invalid_argument("Invalid paragraph effective baseline.");
	const auto range = NormalizeRange(start, length);
	const auto fragment = ToFragment();
	const auto ids = ParagraphIdsForRange(fragment, range, effectiveBaseline);
	const auto records = CollectParagraphRecords(fragment, effectiveBaseline);
	std::vector<RichTextParagraphStyle> result;
	result.reserve(ids.size());
	for (const auto id : ids)
	{
		const auto found = std::find_if(records.begin(), records.end(),
			[id](const ParagraphRecord& record) { return record.Id == id; });
		if (found != records.end()) result.push_back(found->Style);
	}
	return result;
}

std::vector<RichTextRange> RichTextDocument::ParagraphRanges() const
{
	const auto records = CollectParagraphRecords(
		ToFragment(), RichTextParagraphStyle{});
	std::vector<RichTextRange> result;
	result.reserve(records.size());
	for (const auto& record : records)
		result.push_back(RichTextRange{
			record.Start, record.End - record.Start });
	return result;
}

RichTextDocumentChange RichTextDocument::ApplyParagraphFormat(
	std::size_t start,
	std::size_t length,
	const RichTextParagraphFormatDelta& delta,
	const RichTextParagraphStyle& effectiveBaseline)
{
	if (!delta.Validate() || !effectiveBaseline.Validate())
		throw std::invalid_argument("Invalid rich-text paragraph formatting.");
	const auto range = NormalizeRange(start, length);
	RichTextDocumentChange change;
	change.Start = 0;
	change.Before = ToFragment();
	change.After = change.Before;
	if (delta.Empty()
		|| (_structureSpans.empty() && _structureMarkers.empty()))
		return change;

	const auto ids = ParagraphIdsForRange(
		change.Before, range, effectiveBaseline);
	if (ids.empty()) return change;
	const std::unordered_set<std::uint64_t> selected(ids.begin(), ids.end());
	auto formatted = change.Before;
	auto updatePath = [&](std::vector<RichTextStructureNode>& path)
	{
		if (path.empty() || !selected.contains(path.front().Id)) return;
		auto& paragraph = path.front();
		const auto effective = EffectiveParagraphStyle(
			formatted.RootParagraphStyle, path, &effectiveBaseline);
		auto applicable = delta;
		if (applicable.TextAlignment.Mode == RichTextFormatDeltaMode::Set
			&& effective.TextAlignment == applicable.TextAlignment.Value)
			applicable.TextAlignment =
				RichTextFormatChange<::TextAlignment>::Keep();
		if (applicable.FlowDirection.Mode == RichTextFormatDeltaMode::Set
			&& effective.FlowDirection == applicable.FlowDirection.Value)
			applicable.FlowDirection =
				RichTextFormatChange<::FlowDirection>::Keep();
		if (applicable.Empty()) return;
		paragraph.LocalParagraphStyle = applicable.ApplyTo(
			paragraph.LocalParagraphStyle);
	};
	for (auto& span : formatted.StructureSpans) updatePath(span.Path);
	for (auto& marker : formatted.StructureMarkers) updatePath(marker.Path);
	if (!formatted.ValidateCanonical())
		throw std::logic_error(
			"Paragraph formatting produced a non-canonical fragment.");
	if (formatted == change.Before) return change;

	_text = formatted.Text;
	_spans = formatted.Spans;
	_structureSpans = formatted.StructureSpans;
	_rootStyle = formatted.RootStyle;
	_structureRootId = formatted.StructureRootId;
	_structureMarkers = formatted.StructureMarkers;
	_rootParagraphStyle = formatted.RootParagraphStyle;
	change.After = std::move(formatted);
	return change;
}
