#pragma once

#include "DependencyObject.h"
#include "RichTextDocument.h"
#include "TextPointer.h"

#include <string>

class FlowDocument;

enum class TextRangePropertyValueKind
{
	Unset,
	Value,
	Mixed
};

/** Result of querying one TextElement formatting property across a range. */
struct TextRangePropertyValue
{
	TextRangePropertyValueKind Kind = TextRangePropertyValueKind::Unset;
	BindingValue Value;
};

// Compatibility aliases for the original selection-only result names.
using TextSelectionPropertyValueKind = TextRangePropertyValueKind;
using TextSelectionPropertyValue = TextRangePropertyValue;

/**
 * A live range over one FlowDocument.
 *
 * Endpoints follow document mutations through TextPointer gravity. TextRange
 * content continues to use the canonical visible UTF-16 projection; callers
 * that need exact element edges can navigate the endpoints' SymbolOffset and
 * TextPointerContext independently.
 */
class TextRange
{
public:
	TextRange(
		const TextPointer& position1,
		const TextPointer& position2);
	virtual ~TextRange() = default;
	TextRange(const TextRange&) = delete;
	TextRange& operator=(const TextRange&) = delete;
	TextRange(TextRange&&) = delete;
	TextRange& operator=(TextRange&&) = delete;

	virtual TextPointer GetStart() const;
	virtual TextPointer GetEnd() const;
	__declspec(property(get = GetStart)) TextPointer Start;
	__declspec(property(get = GetEnd)) TextPointer End;

	bool GetIsEmpty() const;
	__declspec(property(get = GetIsEmpty)) bool IsEmpty;
	bool Contains(const TextPointer& position) const;
	virtual void Select(
		const TextPointer& position1,
		const TextPointer& position2);

	virtual std::wstring GetText() const;
	virtual void SetText(std::wstring value);
	__declspec(property(get = GetText, put = SetText)) std::wstring Text;

	virtual bool ApplyPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value);
	virtual TextRangePropertyValue GetPropertyValue(
		const DependencyProperty& property) const;
	virtual void ClearAllProperties();

protected:
	TextRange() noexcept = default;
	static bool TryBuildFormatDelta(
		const DependencyProperty& property,
		const BindingValue& value,
		RichTextFormatDelta& delta);
	static bool TryBuildParagraphFormatDelta(
		const DependencyProperty& property,
		const BindingValue& value,
		RichTextParagraphFormatDelta& delta);

private:
	FlowDocument& RequireDocument() const;
	RichTextRange GetNormalizedRange() const;
	bool ApplyFormatDelta(const RichTextFormatDelta& delta);
	bool ApplyParagraphFormatDelta(
		const RichTextParagraphFormatDelta& delta);
	void SetNormalizedPositions(
		FlowDocument& document,
		std::size_t start,
		std::size_t end);

	TextPointer _start;
	TextPointer _end;
};
