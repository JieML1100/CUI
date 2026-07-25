#pragma once
#include "Label.h"
#include "Window.h"
UIClass Label::Type() { return UIClass::UI_Label; }

GET_CPP(Label, std::wstring, Text) { return Control::GetText(); }
SET_CPP(Label, std::wstring, Text) { Control::SetText(std::move(value)); }

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
		return true;
	}();
	(void)registered;
}

Label::Label()
{
	RegisterDependencyProperties();
	this->RendererBackgroundColor = D2D1_COLOR_F{ .0f,.0f,.0f,.0f };
}
cui::core::Size Label::MeasureCore(const cui::core::Constraints& available)
{
	(void)available;
	auto font = this->GetRenderFont();
	auto textSize = font->GetTextSize(this->Text);
	return cui::core::Size{
		textSize.width + _padding.Left + _padding.Right,
		textSize.height + _padding.Top + _padding.Bottom };
}
void Label::OnRender()
{
	if (this->IsVisible == false)return;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	auto font = this->GetRenderFont();
	// TextBlock participates in the same layout-slot clipping contract as every
	// FrameworkElement. The previous unbounded WinForms-style paint extent let
	// text escape buttons, expanders and other content controls.
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
		if (foreground)
			d2d->DrawString(
				this->Text, _padding.Left, _padding.Top,
				foreground.Get(), font);
		else
			d2d->DrawString(
				this->Text, _padding.Left, _padding.Top,
				this->RendererForegroundColor, font);
	}
	this->EndRender();
}
