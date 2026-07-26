#pragma once
#include "Label.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <float.h>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Label, TValue> TextPropertyOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyFlags flags =
			DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<Label, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = L"Text";
		options.Design.CategoryOrder = 40;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		return options;
	}

	template<typename TValue>
	DependencyPropertyChoice TextChoice(
		const wchar_t* displayName,
		TValue value)
	{
		return { displayName, BindingValue(std::move(value)) };
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

GET_CPP(Label, std::wstring, Text) { return Control::GetText(); }
SET_CPP(Label, std::wstring, Text) { Control::SetText(std::move(value)); }
GET_CPP(Label, ::TextAlignment, TextAlignment)
{
	return _textAlignment;
}
SET_CPP(Label, ::TextAlignment, TextAlignment)
{
	if (!SetPropertyField(L"TextAlignment", _textAlignment, value)) return;
	InvalidateFormattedText();
}
GET_CPP(Label, ::TextWrapping, TextWrapping)
{
	return _textWrapping;
}
SET_CPP(Label, ::TextWrapping, TextWrapping)
{
	if (!SetPropertyField(L"TextWrapping", _textWrapping, value)) return;
	InvalidateFormattedText();
}
GET_CPP(Label, ::TextTrimming, TextTrimming)
{
	return _textTrimming;
}
SET_CPP(Label, ::TextTrimming, TextTrimming)
{
	if (!SetPropertyField(L"TextTrimming", _textTrimming, value)) return;
	InvalidateFormattedText();
}

void Label::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<Label, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		DependencyPropertyRegistry::Register<Label, std::wstring>(L"Text",
			[](Label& target) { return target.Text; },
			[](Label& target, const std::wstring& value)
			{ target.Text = value; },
			[](Label& target, Handler handler, DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.PropertyName == L"Text")
							handler();
					});
			}, std::move(options));

		auto alignmentOptions = TextPropertyOptions(
			::TextAlignment::Left, 20,
			DependencyPropertyFlags::Inherits
				| DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		alignmentOptions.Design.Choices = {
			TextChoice(L"Left", ::TextAlignment::Left),
			TextChoice(L"Right", ::TextAlignment::Right),
			TextChoice(L"Center", ::TextAlignment::Center),
			TextChoice(L"Justify", ::TextAlignment::Justify)
		};
		alignmentOptions.Coerce = [](
			Label&, const ::TextAlignment& value)
			-> std::optional<::TextAlignment>
		{
			switch (value)
			{
			case ::TextAlignment::Left:
			case ::TextAlignment::Right:
			case ::TextAlignment::Center:
			case ::TextAlignment::Justify:
				return value;
			default:
				return std::nullopt;
			}
		};
		DependencyPropertyRegistry::Register<Label, ::TextAlignment>(
			L"TextAlignment",
			[](Label& target) { return target.TextAlignment; },
			[](Label& target, const ::TextAlignment& value)
			{ target.TextAlignment = value; },
			{}, std::move(alignmentOptions));

		auto wrappingOptions = TextPropertyOptions(
			::TextWrapping::NoWrap, 30);
		wrappingOptions.Design.Choices = {
			TextChoice(L"NoWrap", ::TextWrapping::NoWrap),
			TextChoice(L"Wrap", ::TextWrapping::Wrap),
			TextChoice(L"WrapWithOverflow", ::TextWrapping::WrapWithOverflow)
		};
		wrappingOptions.Coerce = [](
			Label&, const ::TextWrapping& value)
			-> std::optional<::TextWrapping>
		{
			switch (value)
			{
			case ::TextWrapping::NoWrap:
			case ::TextWrapping::Wrap:
			case ::TextWrapping::WrapWithOverflow:
				return value;
			default:
				return std::nullopt;
			}
		};
		DependencyPropertyRegistry::Register<Label, ::TextWrapping>(
			L"TextWrapping",
			[](Label& target) { return target.TextWrapping; },
			[](Label& target, const ::TextWrapping& value)
			{ target.TextWrapping = value; },
			{}, std::move(wrappingOptions));

		auto trimmingOptions = TextPropertyOptions(
			::TextTrimming::None, 40);
		trimmingOptions.Design.Choices = {
			TextChoice(L"None", ::TextTrimming::None),
			TextChoice(L"CharacterEllipsis", ::TextTrimming::CharacterEllipsis),
			TextChoice(L"WordEllipsis", ::TextTrimming::WordEllipsis)
		};
		trimmingOptions.Coerce = [](
			Label&, const ::TextTrimming& value)
			-> std::optional<::TextTrimming>
		{
			switch (value)
			{
			case ::TextTrimming::None:
			case ::TextTrimming::CharacterEllipsis:
			case ::TextTrimming::WordEllipsis:
				return value;
			default:
				return std::nullopt;
			}
		};
		DependencyPropertyRegistry::Register<Label, ::TextTrimming>(
			L"TextTrimming",
			[](Label& target) { return target.TextTrimming; },
			[](Label& target, const ::TextTrimming& value)
			{ target.TextTrimming = value; },
			{}, std::move(trimmingOptions));
		return true;
	}();
	(void)registered;
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
	const cui::core::Insets padding{
		_padding.Left, _padding.Top, _padding.Right, _padding.Bottom };
	const auto contentBounds =
		available.Normalized().Deflate(padding).maximum;
	auto* layout = EnsureFormattedText(contentBounds, true);
	const auto textSize = layout
		? font->GetTextSize(layout)
		: D2D1_SIZE_F{ 0.0f, 0.0f };
	return cui::core::Size{
		textSize.width + _padding.Left + _padding.Right,
		textSize.height + _padding.Top + _padding.Bottom };
}
void Label::OnRender()
{
	if (this->IsVisible == false) return;
	auto d2d = this->GetDrawingContext();
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	this->BeginRender(size.width, size.height);
	{
		Microsoft::WRL::ComPtr<ID2D1Brush> background;
		background.Attach(CreateBackgroundBrush(
			*d2d, D2D1::SizeF(size.width, size.height)));
		if (background)
			d2d->FillRect(
				0.0f, 0.0f, size.width, size.height, background.Get());

		Microsoft::WRL::ComPtr<ID2D1Brush> foreground;
		foreground.Attach(CreateForegroundBrush(
			*d2d, D2D1::SizeF(size.width, size.height)));
		const cui::core::Size contentBounds{
			(std::max)(0.0f,
				size.width - _padding.Left - _padding.Right),
			(std::max)(0.0f,
				size.height - _padding.Top - _padding.Bottom)
		};
		auto* layout = contentBounds.width > 0.0f
			&& contentBounds.height > 0.0f
			? EnsureFormattedText(contentBounds, false)
			: nullptr;
		if (layout && foreground)
			d2d->DrawStringLayout(
				layout, _padding.Left, _padding.Top, foreground.Get());
		else if (layout)
			d2d->DrawStringLayout(
				layout, _padding.Left, _padding.Top,
				this->RendererForegroundColor);
	}
	this->EndRender();
}
