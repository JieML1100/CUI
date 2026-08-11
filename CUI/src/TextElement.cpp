#include "TextElement.h"
#include "DependencyProperty.h"
#include "FlowDocument.h"

#include <atomic>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	std::optional<cui::drawing::Brush> ConvertTextBrush(
		const BindingValue& value)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) return brush;
		D2D1_COLOR_F color{};
		if (value.TryGet(color))
			return cui::drawing::MakeSolidColorBrush(color);
		return std::nullopt;
	}

	template<typename TValue>
	void NotifyTextPropertyChanged(
		TextElement& target, const TValue&, const TValue&)
	{
		target.NotifyTextElementChanged();
	}

	bool HasExplicitValue(
		const TextElement& element, const DependencyProperty& property)
	{
		return const_cast<TextElement&>(element)
			.HasPropertyValue(
				property, DependencyPropertyValueSource::Local);
	}

#if CUI_ENABLE_DESIGN_METADATA
	DependencyPropertyDesignMetadata TextPropertyDesign(
		int order,
		const wchar_t* displayName,
		DependencyPropertyEditorKind editor)
	{
		DependencyPropertyDesignMetadata design;
		design.Browsable = false;
		design.DisplayName = displayName;
		design.Category = L"Appearance";
		design.CategoryOrder = 200;
		design.Order = order;
		design.Editor = editor;
		design.Persistence = DependencyPropertyPersistence::Metadata;
		return design;
	}
#endif
}

TextPointer TextElement::GetElementStart() const
{
	VerifyAccess();
	if (!_flowDocument
		|| static_cast<const TextElement*>(_flowDocument) == this)
	{
		throw std::logic_error(
			"TextElement is not attached below a FlowDocument root.");
	}
	return _flowDocument->CreateTextPointerAtElementEdge(
		*this, FlowDocument::TextElementEdge::BeforeStart);
}

TextPointer TextElement::GetContentStart() const
{
	VerifyAccess();
	if (!_flowDocument
		|| static_cast<const TextElement*>(_flowDocument) == this)
	{
		throw std::logic_error(
			"TextElement is not attached below a FlowDocument root.");
	}
	return _flowDocument->CreateTextPointerAtElementEdge(
		*this, FlowDocument::TextElementEdge::AfterStart);
}

TextPointer TextElement::GetContentEnd() const
{
	VerifyAccess();
	if (!_flowDocument
		|| static_cast<const TextElement*>(_flowDocument) == this)
	{
		throw std::logic_error(
			"TextElement is not attached below a FlowDocument root.");
	}
	return _flowDocument->CreateTextPointerAtElementEdge(
		*this, FlowDocument::TextElementEdge::BeforeEnd);
}

TextPointer TextElement::GetElementEnd() const
{
	VerifyAccess();
	if (!_flowDocument
		|| static_cast<const TextElement*>(_flowDocument) == this)
	{
		throw std::logic_error(
			"TextElement is not attached below a FlowDocument root.");
	}
	return _flowDocument->CreateTextPointerAtElementEdge(
		*this, FlowDocument::TextElementEdge::AfterEnd);
}

void TextElement::SetTextTreeParent(
	DependencyObject* parent, FlowDocument* document)
{
	if (_textParent == parent && _flowDocument == document)
	{
		// A transactional reconcile can retain this container object while
		// replacing its child storage. Revisit the subtree so newly inserted
		// children receive the retained container as their real parent.
		OnFlowDocumentChanged(document, document);
		RefreshInheritedTextPropertiesRecursive();
		return;
	}
	auto* oldDocument = _flowDocument;
	_textParent = parent;
	_flowDocument = document;
	if (oldDocument != document)
		OnFlowDocumentChanged(oldDocument, document);
	RefreshInheritedTextPropertiesRecursive();
}

void TextElement::SetTextTreeParentRawRecursive(
	DependencyObject* parent, FlowDocument* document) noexcept
{
	_textParent = parent;
	_flowDocument = document;
	if (auto* flowDocument = dynamic_cast<FlowDocument*>(this))
	{
		for (const auto& child : flowDocument->GetBlocks().Items())
		{
			if (child)
				child->SetTextTreeParentRawRecursive(flowDocument, document);
		}
	}
	else if (auto* paragraph = dynamic_cast<Paragraph*>(this))
	{
		for (const auto& child : paragraph->GetInlines().Items())
		{
			if (child)
				child->SetTextTreeParentRawRecursive(paragraph, document);
		}
	}
	else if (auto* span = dynamic_cast<Span*>(this))
	{
		for (const auto& child : span->GetInlines().Items())
		{
			if (child)
				child->SetTextTreeParentRawRecursive(span, document);
		}
	}
}

void TextElement::SetTextTreeRestoreSuppressedRecursive(bool value) noexcept
{
	_restoringTextTree = value;
	if (auto* flowDocument = dynamic_cast<FlowDocument*>(this))
	{
		for (const auto& child : flowDocument->GetBlocks().Items())
		{
			if (child)
				child->SetTextTreeRestoreSuppressedRecursive(value);
		}
	}
	else if (auto* paragraph = dynamic_cast<Paragraph*>(this))
	{
		for (const auto& child : paragraph->GetInlines().Items())
		{
			if (child)
				child->SetTextTreeRestoreSuppressedRecursive(value);
		}
	}
	else if (auto* span = dynamic_cast<Span*>(this))
	{
		for (const auto& child : span->GetInlines().Items())
		{
			if (child)
				child->SetTextTreeRestoreSuppressedRecursive(value);
		}
	}
}

void TextElement::RestoreTextTreeParentNoThrow(
	DependencyObject* parent, FlowDocument* document) noexcept
{
	SetTextTreeParentRawRecursive(parent, document);
	SetTextTreeRestoreSuppressedRecursive(true);
	try
	{
		RefreshInheritedTextPropertiesRecursive();
	}
	catch (...)
	{
		// Structural rollback must itself be non-throwing.  Property callbacks
		// are suppressed, so only an infrastructure/allocation failure can
		// reach this path; the already-restored ownership remains authoritative.
	}
	SetTextTreeRestoreSuppressedRecursive(false);
}

void TextElement::EnsureTextMutationAllowed() const
{
	if (_flowDocument)
		_flowDocument->ThrowIfMutationDisallowed();
}

void TextElement::VerifyPropertyMutationAllowed(
	const DependencyProperty& property) const
{
	(void)property;
	if (!_refreshingTextInheritance && !_restoringTextTree)
		EnsureTextMutationAllowed();
}

void TextElement::NotifyTextElementChanged()
{
	if (!_refreshingTextInheritance && !_restoringTextTree && _flowDocument)
		_flowDocument->NotifyContentChanged();
}

void TextElement::RefreshInheritedTextProperty(
	const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
	if (!metadata || !HasDependencyPropertyFlag(
		metadata->Flags(), DependencyPropertyFlags::Inherits))
	{
		(void)ClearPropertyValue(
			property, DependencyPropertyValueSource::Inherited);
		return;
	}

	BindingValue inherited;
	auto* parent = dynamic_cast<TextElement*>(_textParent);
	if (parent && parent->TryGetPropertyValue(property, inherited))
	{
		(void)TrySetPropertyValue(
			property, inherited, DependencyPropertyValueSource::Inherited);
	}
	else
	{
		(void)ClearPropertyValue(
			property, DependencyPropertyValueSource::Inherited);
	}
}

void TextElement::RefreshInheritedTextPropertyRecursive(
	const DependencyProperty& property)
{
	const bool previous = _refreshingTextInheritance;
	_refreshingTextInheritance = true;
	try
	{
		RefreshInheritedTextProperty(property);

		std::vector<std::pair<TextElement*,
			std::weak_ptr<const std::atomic_bool>>> children;
		if (auto* document = dynamic_cast<FlowDocument*>(this))
		{
			children.reserve(document->GetBlocks().Count());
			for (const auto& child : document->GetBlocks().Items())
				if (child) children.emplace_back(
					child.get(), child->WeakLifetimeToken());
		}
		else if (auto* paragraph = dynamic_cast<Paragraph*>(this))
		{
			children.reserve(paragraph->GetInlines().Count());
			for (const auto& child : paragraph->GetInlines().Items())
				if (child) children.emplace_back(
					child.get(), child->WeakLifetimeToken());
		}
		else if (auto* span = dynamic_cast<Span*>(this))
		{
			children.reserve(span->GetInlines().Count());
			for (const auto& child : span->GetInlines().Items())
				if (child) children.emplace_back(
					child.get(), child->WeakLifetimeToken());
		}
		for (const auto& [child, lifetime] : children)
		{
			const auto token = lifetime.lock();
			if (token && token->load(std::memory_order_acquire)
				&& child->GetParent() == this)
			{
				child->RefreshInheritedTextPropertyRecursive(property);
			}
		}
	}
	catch (...)
	{
		_refreshingTextInheritance = previous;
		throw;
	}
	_refreshingTextInheritance = previous;
}

void TextElement::RefreshInheritedTextPropertiesRecursive()
{
	for (const auto* property : {
		&ForegroundProperty(), &FontFamilyProperty(), &FontSizeProperty(),
		&LanguageProperty(), &FontWeightProperty(), &FontStretchProperty(),
		&FontStyleProperty(),
		&UnderlineProperty(),
		&StrikethroughProperty(), &TextAlignmentProperty(),
		&FlowDirectionProperty() })
	{
		RefreshInheritedTextPropertyRecursive(*property);
	}
}

void TextElement::ApplyPropertyMetadataChange(
	const DeclarativePropertyMetadata& metadata,
	const BindingValue& oldValue,
	const BindingValue& newValue)
{
	if (_restoringTextTree) return;
	if (!_refreshingTextInheritance
		&& HasDependencyPropertyFlag(
			metadata.Flags(), DependencyPropertyFlags::Inherits))
	{
		std::vector<std::pair<TextElement*,
			std::weak_ptr<const std::atomic_bool>>> children;
		if (auto* document = dynamic_cast<FlowDocument*>(this))
		{
			children.reserve(document->GetBlocks().Count());
			for (const auto& child : document->GetBlocks().Items())
				if (child) children.emplace_back(
					child.get(), child->WeakLifetimeToken());
		}
		else if (auto* paragraph = dynamic_cast<Paragraph*>(this))
		{
			children.reserve(paragraph->GetInlines().Count());
			for (const auto& child : paragraph->GetInlines().Items())
				if (child) children.emplace_back(
					child.get(), child->WeakLifetimeToken());
		}
		else if (auto* span = dynamic_cast<Span*>(this))
		{
			children.reserve(span->GetInlines().Count());
			for (const auto& child : span->GetInlines().Items())
				if (child) children.emplace_back(
					child.get(), child->WeakLifetimeToken());
		}
		for (const auto& [child, lifetime] : children)
		{
			const auto token = lifetime.lock();
			if (token && token->load(std::memory_order_acquire)
				&& child->GetParent() == this)
			{
				child->RefreshInheritedTextPropertyRecursive(
					metadata.Property());
			}
		}
	}
	DependencyObject::ApplyPropertyMetadataChange(
		metadata, oldValue, newValue);
}

cui::drawing::Brush TextElement::GetForeground() const
{
	return GetDependencyPropertyValue<cui::drawing::Brush>(
		ForegroundProperty());
}

void TextElement::SetForeground(cui::drawing::Brush value)
{
	EnsureTextMutationAllowed();
	const auto& property = ForegroundProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetForeground() == value;
	if (SetDependencyPropertyValue(property, std::move(value))
		&& sourceOnlyChange)
	{
		NotifyTextElementChanged();
	}
}

cui::drawing::Brush TextElement::GetBackground() const
{
	return GetDependencyPropertyValue<cui::drawing::Brush>(
		BackgroundProperty());
}

void TextElement::SetBackground(cui::drawing::Brush value)
{
	EnsureTextMutationAllowed();
	const auto& property = BackgroundProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetBackground() == value;
	if (SetDependencyPropertyValue(property, std::move(value))
		&& sourceOnlyChange)
	{
		NotifyTextElementChanged();
	}
}

std::wstring TextElement::GetFontFamily() const
{
	return GetDependencyPropertyValue<std::wstring>(FontFamilyProperty());
}

void TextElement::SetFontFamily(std::wstring value)
{
	EnsureTextMutationAllowed();
	const auto& property = FontFamilyProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFontFamily() == value;
	if (SetDependencyPropertyValue(property, std::move(value))
		&& sourceOnlyChange)
	{
		NotifyTextElementChanged();
	}
}

std::wstring TextElement::GetLanguage() const
{
	return GetDependencyPropertyValue<std::wstring>(LanguageProperty());
}

void TextElement::SetLanguage(std::wstring value)
{
	EnsureTextMutationAllowed();
	const auto normalized = NormalizeRichTextLanguageTag(value);
	if (!normalized) return;
	const auto& property = LanguageProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetLanguage() == *normalized;
	if (SetDependencyPropertyValue(property, *normalized)
		&& sourceOnlyChange)
	{
		NotifyTextElementChanged();
	}
}

double TextElement::GetFontSize() const
{
	return GetDependencyPropertyValue<double>(FontSizeProperty());
}

void TextElement::SetFontSize(double value)
{
	EnsureTextMutationAllowed();
	const auto& property = FontSizeProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFontSize() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

DWRITE_FONT_WEIGHT TextElement::GetFontWeight() const
{
	return GetDependencyPropertyValue<DWRITE_FONT_WEIGHT>(FontWeightProperty());
}

void TextElement::SetFontWeight(DWRITE_FONT_WEIGHT value)
{
	EnsureTextMutationAllowed();
	const auto& property = FontWeightProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFontWeight() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

DWRITE_FONT_STRETCH TextElement::GetFontStretch() const
{
	return GetDependencyPropertyValue<DWRITE_FONT_STRETCH>(
		FontStretchProperty());
}

void TextElement::SetFontStretch(DWRITE_FONT_STRETCH value)
{
	EnsureTextMutationAllowed();
	const auto& property = FontStretchProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFontStretch() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

DWRITE_FONT_STYLE TextElement::GetFontStyle() const
{
	return GetDependencyPropertyValue<DWRITE_FONT_STYLE>(FontStyleProperty());
}

void TextElement::SetFontStyle(DWRITE_FONT_STYLE value)
{
	EnsureTextMutationAllowed();
	const auto& property = FontStyleProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetFontStyle() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

bool TextElement::GetUnderline() const
{
	return GetDependencyPropertyValue<bool>(UnderlineProperty());
}

void TextElement::SetUnderline(bool value)
{
	EnsureTextMutationAllowed();
	const auto& property = UnderlineProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetUnderline() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

bool TextElement::GetStrikethrough() const
{
	return GetDependencyPropertyValue<bool>(StrikethroughProperty());
}

void TextElement::SetStrikethrough(bool value)
{
	EnsureTextMutationAllowed();
	const auto& property = StrikethroughProperty();
	const bool sourceOnlyChange = !HasPropertyValue(
		property, DependencyPropertyValueSource::Local)
		&& GetStrikethrough() == value;
	if (SetDependencyPropertyValue(property, value) && sourceOnlyChange)
		NotifyTextElementChanged();
}

const DependencyProperty& TextElement::ForegroundProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::MakeSolidColorBrush(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertTextBrush;
		options.Changed = NotifyTextPropertyChanged<cui::drawing::Brush>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			21, L"Foreground", DependencyPropertyEditorKind::Text);
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"Foreground"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::BackgroundProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertTextBrush;
		options.Changed = NotifyTextPropertyChanged<cui::drawing::Brush>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			10, L"Background", DependencyPropertyEditorKind::Text);
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"Background"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::FontFamilyProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, std::wstring> options;
		options.DefaultValue = std::wstring(L"Segoe UI");
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](TextElement&, const std::wstring& proposed)
			-> std::optional<std::wstring>
		{
			return proposed.empty()
				? std::nullopt
				: std::optional<std::wstring>(proposed);
		};
		options.Changed = NotifyTextPropertyChanged<std::wstring>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			30, L"Font family", DependencyPropertyEditorKind::Text);
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"FontFamily"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::LanguageProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, std::wstring> options;
		options.DefaultValue = std::wstring(L"en-us");
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](TextElement&, const std::wstring& proposed)
			-> std::optional<std::wstring>
		{
			return NormalizeRichTextLanguageTag(proposed);
		};
		options.Changed = NotifyTextPropertyChanged<std::wstring>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			35, L"Language", DependencyPropertyEditorKind::Text);
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Language"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::FontSizeProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, double> options;
		options.DefaultValue = 12.0;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const double& value)
		{
			return std::isfinite(value)
				&& value >= (1.0 / 300.0) && value <= 160000.0;
		};
		options.Changed = NotifyTextPropertyChanged<double>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			40, L"Font size", DependencyPropertyEditorKind::Number);
		options.Design.Minimum = 1.0 / 300.0;
		options.Design.Maximum = 160000.0;
		options.Design.Step = 0.5;
		)
		return DependencyPropertyRegistry::RegisterStatic<TextElement, double>(
			DependencyPropertyRegistrationLiteral(L"FontSize"),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::FontWeightProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, DWRITE_FONT_WEIGHT> options;
		options.DefaultValue = DWRITE_FONT_WEIGHT_NORMAL;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Changed = NotifyTextPropertyChanged<DWRITE_FONT_WEIGHT>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			50, L"Font weight", DependencyPropertyEditorKind::Choice);
		options.Design.Choices = {
			{ L"Normal", BindingValue(DWRITE_FONT_WEIGHT_NORMAL) },
			{ L"SemiBold", BindingValue(DWRITE_FONT_WEIGHT_SEMI_BOLD) },
			{ L"Bold", BindingValue(DWRITE_FONT_WEIGHT_BOLD) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, DWRITE_FONT_WEIGHT>(
				DependencyPropertyRegistrationLiteral(L"FontWeight"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::FontStretchProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, DWRITE_FONT_STRETCH> options;
		options.DefaultValue = DWRITE_FONT_STRETCH_NORMAL;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const DWRITE_FONT_STRETCH& value)
		{
			return value >= DWRITE_FONT_STRETCH_ULTRA_CONDENSED
				&& value <= DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
		};
		options.Changed = NotifyTextPropertyChanged<DWRITE_FONT_STRETCH>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			60, L"Font stretch", DependencyPropertyEditorKind::Choice);
		options.Design.Choices = {
			{ L"Ultra condensed", BindingValue(DWRITE_FONT_STRETCH_ULTRA_CONDENSED) },
			{ L"Extra condensed", BindingValue(DWRITE_FONT_STRETCH_EXTRA_CONDENSED) },
			{ L"Condensed", BindingValue(DWRITE_FONT_STRETCH_CONDENSED) },
			{ L"Semi condensed", BindingValue(DWRITE_FONT_STRETCH_SEMI_CONDENSED) },
			{ L"Normal", BindingValue(DWRITE_FONT_STRETCH_NORMAL) },
			{ L"Semi expanded", BindingValue(DWRITE_FONT_STRETCH_SEMI_EXPANDED) },
			{ L"Expanded", BindingValue(DWRITE_FONT_STRETCH_EXPANDED) },
			{ L"Extra expanded", BindingValue(DWRITE_FONT_STRETCH_EXTRA_EXPANDED) },
			{ L"Ultra expanded", BindingValue(DWRITE_FONT_STRETCH_ULTRA_EXPANDED) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, DWRITE_FONT_STRETCH>(
				DependencyPropertyRegistrationLiteral(L"FontStretch"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::FontStyleProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, DWRITE_FONT_STYLE> options;
		options.DefaultValue = DWRITE_FONT_STYLE_NORMAL;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Changed = NotifyTextPropertyChanged<DWRITE_FONT_STYLE>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			60, L"Font style", DependencyPropertyEditorKind::Choice);
		options.Design.Choices = {
			{ L"Normal", BindingValue(DWRITE_FONT_STYLE_NORMAL) },
			{ L"Oblique", BindingValue(DWRITE_FONT_STYLE_OBLIQUE) },
			{ L"Italic", BindingValue(DWRITE_FONT_STYLE_ITALIC) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, DWRITE_FONT_STYLE>(
				DependencyPropertyRegistrationLiteral(L"FontStyle"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::UnderlineProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		options.Changed = NotifyTextPropertyChanged<bool>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			70, L"Underline", DependencyPropertyEditorKind::Boolean);
		)
		return DependencyPropertyRegistry::RegisterStatic<TextElement, bool>(
			DependencyPropertyRegistrationLiteral(L"Underline"),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::StrikethroughProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		options.Changed = NotifyTextPropertyChanged<bool>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			80, L"Strikethrough", DependencyPropertyEditorKind::Boolean);
		)
		return DependencyPropertyRegistry::RegisterStatic<TextElement, bool>(
			DependencyPropertyRegistrationLiteral(L"Strikethrough"),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::TextAlignmentProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, ::TextAlignment> options;
		options.DefaultValue = ::TextAlignment::Left;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const ::TextAlignment& value)
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
		};
		options.Changed = NotifyTextPropertyChanged<::TextAlignment>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			90, L"Text alignment", DependencyPropertyEditorKind::Choice);
		options.Design.Choices = {
			{ L"Left", BindingValue(::TextAlignment::Left) },
			{ L"Right", BindingValue(::TextAlignment::Right) },
			{ L"Center", BindingValue(::TextAlignment::Center) },
			{ L"Justify", BindingValue(::TextAlignment::Justify) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, ::TextAlignment>(
				DependencyPropertyRegistrationLiteral(L"TextAlignment"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::FlowDirectionProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, ::FlowDirection> options;
		options.DefaultValue = ::FlowDirection::LeftToRight;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const ::FlowDirection& value)
		{
			return value == ::FlowDirection::LeftToRight
				|| value == ::FlowDirection::RightToLeft;
		};
		options.Changed = NotifyTextPropertyChanged<::FlowDirection>;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextPropertyDesign(
			100, L"Flow direction", DependencyPropertyEditorKind::Choice);
		options.Design.Choices = {
			{ L"Left to right", BindingValue(::FlowDirection::LeftToRight) },
			{ L"Right to left", BindingValue(::FlowDirection::RightToLeft) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, ::FlowDirection>(
				DependencyPropertyRegistrationLiteral(L"FlowDirection"),
				std::move(options));
	}();
	return *registration;
}

void TextElement::RegisterDependencyProperties()
{
#if CUI_ENABLE_DYNAMIC_XAML
	(void)ForegroundProperty();
	(void)BackgroundProperty();
	(void)FontFamilyProperty();
	(void)LanguageProperty();
	(void)FontSizeProperty();
	(void)FontWeightProperty();
	(void)FontStretchProperty();
	(void)FontStyleProperty();
	(void)UnderlineProperty();
	(void)StrikethroughProperty();
	(void)TextAlignmentProperty();
	(void)FlowDirectionProperty();
#endif
}

void TextElement::ApplyLocalCharacterStyle(
	RichTextCharacterStyle& style) const
{
	if (HasExplicitValue(*this, ForegroundProperty()))
		style.Foreground = GetForeground();
	if (HasExplicitValue(*this, BackgroundProperty()))
		style.Background = GetBackground();
	if (HasExplicitValue(*this, FontFamilyProperty()))
		style.FontFamily = GetFontFamily();
	if (HasExplicitValue(*this, LanguageProperty()))
		style.Language = GetLanguage();
	if (HasExplicitValue(*this, FontSizeProperty()))
		style.FontSize = static_cast<float>(GetFontSize());
	if (HasExplicitValue(*this, FontWeightProperty()))
		style.FontWeight = GetFontWeight();
	if (HasExplicitValue(*this, FontStretchProperty()))
		style.FontStretch = GetFontStretch();
	if (HasExplicitValue(*this, FontStyleProperty()))
		style.FontStyle = GetFontStyle();
	if (HasExplicitValue(*this, UnderlineProperty()))
		style.Underline = GetUnderline();
	if (HasExplicitValue(*this, StrikethroughProperty()))
		style.Strikethrough = GetStrikethrough();
}

void TextElement::ApplyLocalParagraphStyle(
	RichTextParagraphStyle& style) const
{
	if (HasExplicitValue(*this, TextAlignmentProperty()))
	{
		style.TextAlignment =
			GetDependencyPropertyValue<::TextAlignment>(TextAlignmentProperty());
	}
	if (HasExplicitValue(*this, FlowDirectionProperty()))
	{
		style.FlowDirection =
			GetDependencyPropertyValue<::FlowDirection>(FlowDirectionProperty());
	}
}
