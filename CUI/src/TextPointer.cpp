#include "TextPointer.h"

#include "FlowDocument.h"
#include "TextEditCore.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace
{
	std::size_t SnapInsertionOffset(
		const std::wstring& text,
		std::size_t offset,
		LogicalDirection direction) noexcept
	{
		offset = (std::min)(offset, text.size());
		return CuiTextBoundary::SnapToTextElementBoundary(
			text, offset,
			direction == LogicalDirection::Forward, true);
	}
}

bool TextPointer::IsValid() const noexcept
{
	return _state && _state->Document;
}

FlowDocument* TextPointer::GetDocument() const noexcept
{
	return _state ? _state->Document : nullptr;
}

const TextPointer::State& TextPointer::RequireState() const
{
	if (!IsValid())
		throw std::logic_error(
			"TextPointer is not attached to a live FlowDocument.");
	_state->Document->VerifyAccess();
	return *_state;
}

LogicalDirection TextPointer::GetLogicalDirection() const
{
	return RequireState().Direction;
}

std::size_t TextPointer::GetTextOffset() const
{
	(void)RequireState();
	auto& state = *_state;
	(void)state.Document->ResolveTextPointerSymbolOffset(state);
	return state.TextOffset;
}

std::size_t TextPointer::GetSymbolOffset() const
{
	(void)RequireState();
	auto& state = *_state;
	return state.Document->ResolveTextPointerSymbolOffset(state);
}

int TextPointer::CompareTo(const TextPointer& position) const
{
	const auto& left = RequireState();
	const auto& right = position.RequireState();
	if (left.Document != right.Document)
		throw std::invalid_argument(
			"TextPointers from different FlowDocuments cannot be compared.");
	const auto leftOffset = GetSymbolOffset();
	const auto rightOffset = position.GetSymbolOffset();
	if (leftOffset < rightOffset) return -1;
	if (leftOffset > rightOffset) return 1;
	return 0;
}

std::ptrdiff_t TextPointer::GetTextOffsetToPosition(
	const TextPointer& position) const
{
	const auto& left = RequireState();
	const auto& right = position.RequireState();
	if (left.Document != right.Document)
		throw std::invalid_argument(
			"TextPointers from different FlowDocuments cannot be measured.");
	const auto leftOffset = GetTextOffset();
	const auto rightOffset = position.GetTextOffset();
	constexpr auto maximum =
		static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
	if (leftOffset > maximum || rightOffset > maximum)
		throw std::overflow_error("TextPointer distance is out of range.");
	return static_cast<std::ptrdiff_t>(rightOffset)
		- static_cast<std::ptrdiff_t>(leftOffset);
}

int TextPointer::CompareSymbolPositionTo(
	const TextPointer& position) const
{
	return CompareTo(position);
}

std::ptrdiff_t TextPointer::GetSymbolOffsetToPosition(
	const TextPointer& position) const
{
	const auto& left = RequireState();
	const auto& right = position.RequireState();
	if (left.Document != right.Document)
		throw std::invalid_argument(
			"TextPointers from different FlowDocuments cannot be measured.");
	const auto leftOffset = GetSymbolOffset();
	const auto rightOffset = position.GetSymbolOffset();
	constexpr auto maximum =
		static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)());
	if (leftOffset > maximum || rightOffset > maximum)
		throw std::overflow_error("TextPointer symbol distance is out of range.");
	return static_cast<std::ptrdiff_t>(rightOffset)
		- static_cast<std::ptrdiff_t>(leftOffset);
}

std::optional<TextPointer> TextPointer::GetPositionAtTextOffset(
	std::ptrdiff_t offset) const
{
	return GetPositionAtTextOffset(offset, GetLogicalDirection());
}

std::optional<TextPointer> TextPointer::GetPositionAtTextOffset(
	std::ptrdiff_t offset, LogicalDirection direction) const
{
	const auto& state = RequireState();
	const auto currentOffset = GetTextOffset();
	const auto length = state.Document->Flatten().Text.size();
	if (currentOffset
		> static_cast<std::size_t>(
			(std::numeric_limits<std::ptrdiff_t>::max)()))
	{
		return std::nullopt;
	}
	const auto current = static_cast<std::ptrdiff_t>(currentOffset);
	if ((offset > 0
			&& current > (std::numeric_limits<std::ptrdiff_t>::max)() - offset)
		|| (offset < 0
			&& current < (std::numeric_limits<std::ptrdiff_t>::min)() - offset))
	{
		return std::nullopt;
	}
	const auto requested = current + offset;
	if (requested < 0
		|| static_cast<std::size_t>(requested) > length)
	{
		return std::nullopt;
	}
	return state.Document->CreateTextPointerAtTextOffset(
		static_cast<std::size_t>(requested), direction);
}

std::optional<TextPointer> TextPointer::GetPositionAtSymbolOffset(
	std::ptrdiff_t offset) const
{
	return GetPositionAtSymbolOffset(offset, GetLogicalDirection());
}

std::optional<TextPointer> TextPointer::GetPositionAtSymbolOffset(
	std::ptrdiff_t offset, LogicalDirection direction) const
{
	const auto& state = RequireState();
	const auto currentOffset = GetSymbolOffset();
	if (currentOffset > static_cast<std::size_t>(
		(std::numeric_limits<std::ptrdiff_t>::max)()))
	{
		return std::nullopt;
	}
	const auto current = static_cast<std::ptrdiff_t>(currentOffset);
	if ((offset > 0
		&& current > (std::numeric_limits<std::ptrdiff_t>::max)() - offset)
		|| (offset < 0
			&& current < (std::numeric_limits<std::ptrdiff_t>::min)() - offset))
	{
		return std::nullopt;
	}
	const auto requested = current + offset;
	if (requested < 0
		|| static_cast<std::size_t>(requested)
			> state.Document->GetSymbolCount())
	{
		return std::nullopt;
	}
	return state.Document->CreateTextPointerAtSymbolOffset(
		static_cast<std::size_t>(requested), direction);
}

TextPointerContext TextPointer::GetPointerContext(
	LogicalDirection direction) const
{
	(void)RequireState();
	auto& state = *_state;
	return state.Document->GetTextPointerContext(state, direction);
}

DependencyObject* TextPointer::GetAdjacentElement(
	LogicalDirection direction) const
{
	(void)RequireState();
	auto& state = *_state;
	return state.Document->GetTextPointerAdjacentElement(state, direction);
}

std::optional<TextPointer> TextPointer::GetNextContextPosition(
	LogicalDirection direction) const
{
	(void)RequireState();
	auto& state = *_state;
	return state.Document->GetNextTextPointerContextPosition(
		state, direction);
}

std::size_t TextPointer::GetTextRunLength(
	LogicalDirection direction) const
{
	(void)RequireState();
	auto& state = *_state;
	return state.Document->GetTextPointerRunLength(state, direction);
}

std::wstring TextPointer::GetTextInRun(
	LogicalDirection direction) const
{
	(void)RequireState();
	auto& state = *_state;
	return state.Document->GetTextPointerRun(state, direction);
}

bool TextPointer::IsAtInsertionPosition() const
{
	const auto& state = RequireState();
	const auto text = state.Document->Flatten().Text;
	return SnapInsertionOffset(
		text, GetTextOffset(), state.Direction) == GetTextOffset();
}

TextPointer TextPointer::GetInsertionPosition(
	LogicalDirection direction) const
{
	const auto& state = RequireState();
	const auto text = state.Document->Flatten().Text;
	return state.Document->CreateTextPointerAtTextOffset(
		SnapInsertionOffset(text, GetTextOffset(), direction), direction);
}

std::optional<TextPointer> TextPointer::GetNextInsertionPosition(
	LogicalDirection direction) const
{
	const auto& state = RequireState();
	const auto text = state.Document->Flatten().Text;
	const auto insertion = SnapInsertionOffset(
		text, GetTextOffset(), direction);
	std::size_t next = insertion;
	if (direction == LogicalDirection::Forward)
	{
		if (insertion >= text.size()) return std::nullopt;
		next = CuiTextBoundary::GetNextTextElementBoundary(
			text, insertion, true);
	}
	else
	{
		if (insertion == 0) return std::nullopt;
		next = CuiTextBoundary::GetPreviousTextElementBoundary(
			text, insertion, true);
	}
	return state.Document->CreateTextPointerAtTextOffset(next, direction);
}

bool TextPointer::operator==(const TextPointer& other) const
{
	if (!IsValid() || !other.IsValid())
		return !IsValid() && !other.IsValid();
	return _state->Document == other._state->Document
		&& GetSymbolOffset() == other.GetSymbolOffset()
		&& _state->Direction == other._state->Direction;
}
