#pragma once
#include "Label.h"
#include "TextElement.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <float.h>
#include <stdexcept>
#include <typeindex>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Label, TValue> TextPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(int order),
		DependencyPropertyFlags flags =
			DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<Label, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Text";
		options.Design.CategoryOrder = 40;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return options;
	}

#if CUI_ENABLE_DESIGN_METADATA
	template<typename TValue>
	DependencyPropertyChoice TextChoice(
		const wchar_t* displayName,
		TValue value)
	{
		return { displayName, BindingValue(std::move(value)) };
	}

	DependencyPropertyDesignMetadata LabelBrushDesign(
		int order, const wchar_t* displayName)
	{
		DependencyPropertyDesignMetadata design;
		design.Browsable = false;
		design.DisplayName = displayName;
		design.Category = L"Appearance";
		design.CategoryOrder = 200;
		design.Order = order;
		design.Editor = DependencyPropertyEditorKind::Text;
		design.Persistence = DependencyPropertyPersistence::Metadata;
		return design;
	}
#endif

	std::optional<cui::drawing::Brush> ConvertLabelBrush(
		const BindingValue& value)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) return brush;
		D2D1_COLOR_F color{};
		if (value.TryGet(color))
			return cui::drawing::MakeSolidColorBrush(color);
		return std::nullopt;
	}

	float DirectWriteExtent(float value) noexcept
	{
		if (!std::isfinite(value)) return FLT_MAX;
		// DirectWrite rejects a zero maximum extent. Layout still clamps the
		// public DesiredSize to zero when the parent offers no space.
		return (std::max)(0.01f, value);
	}

	DWRITE_TEXT_ALIGNMENT ToDirectWriteAlignment(
		TextAlignment value) noexcept
	{
		switch (value)
		{
		case ::TextAlignment::Right:
			return DWRITE_TEXT_ALIGNMENT_TRAILING;
		case ::TextAlignment::Center:
			return DWRITE_TEXT_ALIGNMENT_CENTER;
		case ::TextAlignment::Justify:
			return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
		case ::TextAlignment::Left:
		default:
			return DWRITE_TEXT_ALIGNMENT_LEADING;
		}
	}

	DWRITE_WORD_WRAPPING ToDirectWriteWrapping(
		TextWrapping value) noexcept
	{
		switch (value)
		{
		case ::TextWrapping::Wrap:
			return DWRITE_WORD_WRAPPING_WRAP;
		case ::TextWrapping::WrapWithOverflow:
			return DWRITE_WORD_WRAPPING_WHOLE_WORD;
		case ::TextWrapping::NoWrap:
		default:
			return DWRITE_WORD_WRAPPING_NO_WRAP;
		}
	}
}

UIClass Label::Type() { return UIClass::UI_Label; }

GET_CPP(Label, std::wstring, Text)
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

std::wstring Label::GetSemanticText() const
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

SET_CPP(Label, std::wstring, Text)
{
	(void)SetDependencyPropertyValue(TextProperty(), std::move(value));
}
GET_CPP(Label, ::TextAlignment, TextAlignment)
{
	return _textAlignment;
}
SET_CPP(Label, ::TextAlignment, TextAlignment)
{
	if (!SetPropertyField(
		TextAlignmentProperty(), _textAlignment, value)) return;
	InvalidateFormattedText();
}
GET_CPP(Label, ::TextWrapping, TextWrapping)
{
	return _textWrapping;
}
SET_CPP(Label, ::TextWrapping, TextWrapping)
{
	if (!SetPropertyField(TextWrappingProperty(), _textWrapping, value)) return;
	InvalidateFormattedText();
}
GET_CPP(Label, ::TextTrimming, TextTrimming)
{
	return _textTrimming;
}
SET_CPP(Label, ::TextTrimming, TextTrimming)
{
	if (!SetPropertyField(TextTrimmingProperty(), _textTrimming, value)) return;
	InvalidateFormattedText();
}

const DependencyProperty& Label::TextProperty()
{
	static const auto registration = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<Label, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		options.Changed = [](
			Label& target, const std::wstring&, const std::wstring&)
		{
			target.InvalidateFormattedText();
		};
		return DependencyPropertyRegistry::RegisterStatic<Label, std::wstring>(
			DependencyPropertyRegistrationLiteral(L"Text"),
			[](Label& target, Handler handler, DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.Property == &Label::TextProperty()) handler();
					});
			}, std::move(options));
	}();
	return *registration;
}

const DependencyPropertyMetadataRegistration&
Label::ForegroundPropertyMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		DependencyPropertyOptions<Label, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::MakeSolidColorBrush(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertLabelBrush;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = LabelBrushDesign(21, L"Foreground");
		)
		return DependencyPropertyRegistry::AddOwnerStatic<
			Label, cui::drawing::Brush>(
				TextElement::ForegroundProperty(), std::move(options));
	}();
	return relation;
}

const DependencyProperty& Label::ForegroundProperty()
{
	return ForegroundPropertyMetadataRelation().Property();
}

const DependencyPropertyMetadataRegistration&
Label::BackgroundPropertyMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		DependencyPropertyOptions<Label, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertLabelBrush;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = LabelBrushDesign(10, L"Background");
		)
		return DependencyPropertyRegistry::AddOwnerStatic<
			Label, cui::drawing::Brush>(
				TextElement::BackgroundProperty(), std::move(options));
	}();
	return relation;
}

const DependencyProperty& Label::BackgroundProperty()
{
	return BackgroundPropertyMetadataRelation().Property();
}

const DependencyProperty& Label::TextAlignmentProperty()
{
	static const auto registration = []
	{
		auto options = TextPropertyOptions(
			::TextAlignment::Left CUI_DESIGN_METADATA_ARGUMENTS(20),
			DependencyPropertyFlags::Inherits
				| DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			TextChoice(L"Left", ::TextAlignment::Left),
			TextChoice(L"Right", ::TextAlignment::Right),
			TextChoice(L"Center", ::TextAlignment::Center),
			TextChoice(L"Justify", ::TextAlignment::Justify)
		};
		)
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
		return DependencyPropertyRegistry::RegisterStatic<Label, ::TextAlignment>(
			DependencyPropertyRegistrationLiteral(L"TextAlignment"),
			[](Label& target) { return target.TextAlignment; },
			[](Label& target, const ::TextAlignment& value)
			{ target.TextAlignment = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Label::TextWrappingProperty()
{
	static const auto registration = []
	{
		auto options = TextPropertyOptions(
			::TextWrapping::NoWrap CUI_DESIGN_METADATA_ARGUMENTS(30));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			TextChoice(L"NoWrap", ::TextWrapping::NoWrap),
			TextChoice(L"Wrap", ::TextWrapping::Wrap),
			TextChoice(L"WrapWithOverflow", ::TextWrapping::WrapWithOverflow)
		};
		)
		options.Validate = [](const ::TextWrapping& value)
		{
			switch (value)
			{
			case ::TextWrapping::NoWrap:
			case ::TextWrapping::Wrap:
			case ::TextWrapping::WrapWithOverflow:
				return true;
			default:
				return false;
			}
		};
		return DependencyPropertyRegistry::RegisterStatic<Label, ::TextWrapping>(
			DependencyPropertyRegistrationLiteral(L"TextWrapping"),
			[](Label& target) { return target.TextWrapping; },
			[](Label& target, const ::TextWrapping& value)
			{ target.TextWrapping = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Label::TextTrimmingProperty()
{
	static const auto registration = []
	{
		auto options = TextPropertyOptions(
			::TextTrimming::None CUI_DESIGN_METADATA_ARGUMENTS(40));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			TextChoice(L"None", ::TextTrimming::None),
			TextChoice(L"CharacterEllipsis", ::TextTrimming::CharacterEllipsis),
			TextChoice(L"WordEllipsis", ::TextTrimming::WordEllipsis)
		};
		)
		options.Validate = [](const ::TextTrimming& value)
		{
			switch (value)
			{
			case ::TextTrimming::None:
			case ::TextTrimming::CharacterEllipsis:
			case ::TextTrimming::WordEllipsis:
				return true;
			default:
				return false;
			}
		};
		return DependencyPropertyRegistry::RegisterStatic<Label, ::TextTrimming>(
			DependencyPropertyRegistrationLiteral(L"TextTrimming"),
			[](Label& target) { return target.TextTrimming; },
			[](Label& target, const ::TextTrimming& value)
			{ target.TextTrimming = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyPropertyMetadata*
Label::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &TextElement::ForegroundProperty())
		return &ForegroundPropertyMetadataRelation().Metadata();
	if (&property == &TextElement::BackgroundProperty())
		return &BackgroundPropertyMetadataRelation().Metadata();
	return Control::ResolveExactDependencyPropertyMetadata(property);
}

void Label::VisitDeclaredInheritedProperties(
	void* context, InheritedPropertyVisitor visitor) const
{
	Control::VisitDeclaredInheritedProperties(context, visitor);
	if (visitor) visitor(context, TextAlignmentProperty());
}

GET_CPP(Label, cui::drawing::Brush, Background)
{
	return GetDependencyPropertyValue<cui::drawing::Brush>(
		BackgroundProperty());
}

SET_CPP(Label, cui::drawing::Brush, Background)
{
	(void)SetDependencyPropertyValue(
		BackgroundProperty(), std::move(value));
}

void Label::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TextProperty();
	(void)TextAlignmentProperty();
	(void)TextWrappingProperty();
	(void)TextTrimmingProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)ForegroundPropertyMetadataRelation();
	(void)BackgroundPropertyMetadataRelation();
	)
}

Label::Label()
{
	RegisterDependencyProperties();
	this->RendererBackgroundColor = D2D1_COLOR_F{ .0f,.0f,.0f,.0f };
}

void Label::InvalidateFormattedText() noexcept
{
	_formattedText.Reset();
	_formattedTextValue.clear();
	_formattedTextFormat = nullptr;
	_formattedTextFontFamily.clear();
	_formattedTextFontSize = 0.0f;
	_formattedTextBounds = { -1.0f, -1.0f };
}

IDWriteTextLayout* Label::EnsureFormattedText(
	cui::core::Size bounds,
	bool forMeasure)
{
	auto* font = GetRenderFont();
	if (!font || !font->FontObject) return nullptr;
	bounds = {
		DirectWriteExtent(bounds.width),
		DirectWriteExtent(bounds.height)
	};
	if (_formattedText
		&& _formattedTextValue == Text
		&& _formattedTextFormat == font->FontObject
		&& _formattedTextFontFamily == font->FontFamily
		&& _formattedTextFontSize == font->FontSize
		&& _formattedTextBounds == bounds
		&& _formattedTextAlignment == TextAlignment
		&& _formattedTextWrapping == TextWrapping
		&& _formattedTextTrimming == TextTrimming
		&& _formattedTextForMeasure == forMeasure)
		return _formattedText.Get();

	Microsoft::WRL::ComPtr<IDWriteTextLayout> replacement;
	replacement.Attach(Factory::CreateStringLayout(
		Text, bounds.width, bounds.height, font->FontObject));
	if (!replacement) return nullptr;

	replacement->SetWordWrapping(
		ToDirectWriteWrapping(TextWrapping));
	// WPF ignores TextAlignment while discovering natural line widths, then
	// reapplies it against the final arrange width.
	replacement->SetTextAlignment(forMeasure
		? DWRITE_TEXT_ALIGNMENT_LEADING
		: ToDirectWriteAlignment(TextAlignment));
	replacement->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

	if (TextTrimming != ::TextTrimming::None)
	{
		DWRITE_TRIMMING trimming{
			TextTrimming == ::TextTrimming::CharacterEllipsis
				? DWRITE_TRIMMING_GRANULARITY_CHARACTER
				: DWRITE_TRIMMING_GRANULARITY_WORD,
			0,
			0
		};
		Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
		if (SUCCEEDED(_DWriteFactory->CreateEllipsisTrimmingSign(
			font->FontObject, &ellipsis)))
			replacement->SetTrimming(&trimming, ellipsis.Get());
	}

	_formattedText = std::move(replacement);
	_formattedTextValue = Text;
	_formattedTextFormat = font->FontObject;
	_formattedTextFontFamily = font->FontFamily;
	_formattedTextFontSize = font->FontSize;
	_formattedTextBounds = bounds;
	_formattedTextAlignment = TextAlignment;
	_formattedTextWrapping = TextWrapping;
	_formattedTextTrimming = TextTrimming;
	_formattedTextForMeasure = forMeasure;
	return _formattedText.Get();
}

cui::core::Size Label::MeasureCore(const cui::core::Constraints& available)
{
	auto font = this->GetRenderFont();
	if (!font) return {};
	const auto padding = GetSpecifiedLayout().padding;
	const auto contentBounds =
		available.Normalized().Deflate(padding).maximum;
	auto* layout = EnsureFormattedText(contentBounds, true);
	const auto textSize = layout
		? font->GetTextSize(layout)
		: D2D1_SIZE_F{ 0.0f, 0.0f };
	return cui::core::Size{
		textSize.width + padding.Horizontal(),
		textSize.height + padding.Vertical() };
}
void Label::OnRender()
{
	if (this->IsVisible == false) return;
	auto d2d = this->GetDrawingContext();
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	this->BeginRender(size.width, size.height);
	{
		Microsoft::WRL::ComPtr<ID2D1Brush> background;
		const auto backgroundValue = Background;
		if (backgroundValue.Kind != cui::drawing::BrushKind::None)
			background.Attach(backgroundValue.CreateBrush(
				*d2d, D2D1::SizeF(size.width, size.height)));
		if (background)
			d2d->FillRect(
				0.0f, 0.0f, size.width, size.height, background.Get());

		Microsoft::WRL::ComPtr<ID2D1Brush> foreground;
		foreground.Attach(CreateForegroundBrush(
			*d2d, D2D1::SizeF(size.width, size.height)));
		const cui::core::Size contentBounds{
			(std::max)(0.0f,
				size.width - padding.Horizontal()),
			(std::max)(0.0f,
				size.height - padding.Vertical())
		};
		auto* layout = contentBounds.width > 0.0f
			&& contentBounds.height > 0.0f
			? EnsureFormattedText(contentBounds, false)
			: nullptr;
		if (layout && foreground)
			d2d->DrawStringLayout(
				layout, padding.left, padding.top, foreground.Get());
		else if (layout)
			d2d->DrawStringLayout(
				layout, padding.left, padding.top,
				this->RendererForegroundColor);
	}
	this->EndRender();
}
