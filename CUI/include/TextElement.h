#pragma once

#include "Brush.h"
#include "DependencyObject.h"
#include "RichTextDocument.h"
#include "TextPointer.h"

#include <string>

class BlockCollection;
class FlowDocument;
class InlineCollection;
class Paragraph;
class Span;

/**
 * WPF TextElement property owner.
 *
 * TextElement is a non-visual DependencyObject. Document elements keep a
 * separate single-parent tree; they never participate in Control visual or
 * logical ownership merely to reuse dependency-property infrastructure.
 */
class TextElement : public DependencyObject
{
private:
	DependencyObject* _textParent = nullptr;
	FlowDocument* _flowDocument = nullptr;
	std::uint64_t _richTextStructureId = AllocateRichTextStructureId();
	bool _refreshingTextInheritance = false;
	bool _restoringTextTree = false;

	void SetTextTreeParent(
		DependencyObject* parent, FlowDocument* document);
	/**
	 * Restores a failed text-tree transaction without publishing another
	 * property/content callback.  The structural pointers are repaired for the
	 * complete subtree before inherited values are recomputed.
	 */
	void RestoreTextTreeParentNoThrow(
		DependencyObject* parent, FlowDocument* document) noexcept;
	void SetTextTreeParentRawRecursive(
		DependencyObject* parent, FlowDocument* document) noexcept;
	void SetTextTreeRestoreSuppressedRecursive(bool value) noexcept;
	void RefreshInheritedTextProperty(
		const DependencyProperty& property);
	void RefreshInheritedTextPropertyRecursive(
		const DependencyProperty& property);
	void RefreshInheritedTextPropertiesRecursive();
	void RestoreRichTextStructureId(std::uint64_t value) noexcept
	{
		_richTextStructureId = value;
	}

public:
	/** Publishes a content/formatting mutation to the containing document. */
	void NotifyTextElementChanged();

protected:
	/** Rejects public text-tree mutations while the containing document is
	 *  committing or publishing another mutation. */
	void EnsureTextMutationAllowed() const;
	void ApplyPropertyMetadataChange(
		const DeclarativePropertyMetadata& metadata,
		const BindingValue& oldValue,
		const BindingValue& newValue) override;
	void VerifyPropertyMutationAllowed(
		const DependencyProperty& property) const override;
	/** Lets composite text elements propagate a document attachment. */
	virtual void OnFlowDocumentChanged(
		FlowDocument* oldDocument, FlowDocument* newDocument)
	{
		(void)oldDocument;
		(void)newDocument;
	}

	friend class BlockCollection;
	friend class FlowDocument;
	friend class InlineCollection;
	friend class Paragraph;
	friend class Span;

public:
	TextElement() = default;
	~TextElement() override = default;
	TextElement(const TextElement&) = delete;
	TextElement& operator=(const TextElement&) = delete;
	TextElement(TextElement&&) = delete;
	TextElement& operator=(TextElement&&) = delete;

	DependencyObject* GetParent() const noexcept { return _textParent; }
	FlowDocument* GetFlowDocument() const noexcept { return _flowDocument; }
	std::uint64_t GetRichTextStructureId() const noexcept
	{
		return _richTextStructureId;
	}

	TextPointer GetElementStart() const;
	TextPointer GetContentStart() const;
	TextPointer GetContentEnd() const;
	TextPointer GetElementEnd() const;
	__declspec(property(get = GetElementStart)) TextPointer ElementStart;
	__declspec(property(get = GetContentStart)) TextPointer ContentStart;
	__declspec(property(get = GetContentEnd)) TextPointer ContentEnd;
	__declspec(property(get = GetElementEnd)) TextPointer ElementEnd;

	__declspec(property(get = GetForeground, put = SetForeground))
		cui::drawing::Brush Foreground;
	cui::drawing::Brush GetForeground() const;
	void SetForeground(cui::drawing::Brush value);

	__declspec(property(get = GetBackground, put = SetBackground))
		cui::drawing::Brush Background;
	cui::drawing::Brush GetBackground() const;
	void SetBackground(cui::drawing::Brush value);

	__declspec(property(get = GetFontFamily, put = SetFontFamily))
		std::wstring FontFamily;
	std::wstring GetFontFamily() const;
	void SetFontFamily(std::wstring value);
	__declspec(property(get = GetLanguage, put = SetLanguage))
		std::wstring Language;
	std::wstring GetLanguage() const;
	void SetLanguage(std::wstring value);

	__declspec(property(get = GetFontSize, put = SetFontSize)) double FontSize;
	double GetFontSize() const;
	void SetFontSize(double value);

	__declspec(property(get = GetFontWeight, put = SetFontWeight))
		DWRITE_FONT_WEIGHT FontWeight;
	DWRITE_FONT_WEIGHT GetFontWeight() const;
	void SetFontWeight(DWRITE_FONT_WEIGHT value);

	__declspec(property(get = GetFontStretch, put = SetFontStretch))
		DWRITE_FONT_STRETCH FontStretch;
	DWRITE_FONT_STRETCH GetFontStretch() const;
	void SetFontStretch(DWRITE_FONT_STRETCH value);

	__declspec(property(get = GetFontStyle, put = SetFontStyle))
		DWRITE_FONT_STYLE FontStyle;
	DWRITE_FONT_STYLE GetFontStyle() const;
	void SetFontStyle(DWRITE_FONT_STYLE value);

	__declspec(property(get = GetUnderline, put = SetUnderline)) bool Underline;
	bool GetUnderline() const;
	void SetUnderline(bool value);

	__declspec(property(get = GetStrikethrough, put = SetStrikethrough))
		bool Strikethrough;
	bool GetStrikethrough() const;
	void SetStrikethrough(bool value);

	static const DependencyProperty& ForegroundProperty();
	static const DependencyProperty& BackgroundProperty();
	static const DependencyProperty& FontFamilyProperty();
	static const DependencyProperty& LanguageProperty();
	static const DependencyProperty& FontSizeProperty();
	static const DependencyProperty& FontWeightProperty();
	static const DependencyProperty& FontStretchProperty();
	static const DependencyProperty& FontStyleProperty();
	static const DependencyProperty& UnderlineProperty();
	static const DependencyProperty& StrikethroughProperty();
	/** Shared owner identity used by Block and FlowDocument. */
	static const DependencyProperty& TextAlignmentProperty();
	/** Shared inheritable paragraph reading-direction identity. */
	static const DependencyProperty& FlowDirectionProperty();
	static void RegisterDependencyProperties();

	/** Applies only explicitly supplied values to a fragment style. */
	void ApplyLocalCharacterStyle(RichTextCharacterStyle& style) const;
	/** Applies an explicitly authored FlowDocument/Block alignment. */
	void ApplyLocalParagraphStyle(RichTextParagraphStyle& style) const;

#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
};
