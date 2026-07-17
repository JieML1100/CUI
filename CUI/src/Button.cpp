#include "Button.h"
#include "Form.h"
#include <algorithm>
UIClass Button::Type() { return UIClass::UI_Button; }

namespace
{
	bool ButtonColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<Button, D2D1_COLOR_F> ButtonColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ControlPropertyOptions<Button, D2D1_COLOR_F>{
			defaultValue,
			ControlPropertyFlags::AffectsRender
				| ControlPropertyFlags::TracksLocalValue,
			{}, {}, ButtonColorsEqual };
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Color;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	ControlPropertyOptions<Button, float> ButtonNonNegativeFloatOptions(
		float defaultValue,
		int order)
	{
		auto options = ControlPropertyOptions<Button, float>{
			defaultValue,
			ControlPropertyFlags::AffectsRender
				| ControlPropertyFlags::TracksLocalValue,
			[](Button&, const float& proposed) -> std::optional<float>
			{
				return (std::max)(0.0f, proposed);
			} };
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Number;
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}
}

void Button::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		BindingPropertyRegistry::Register<Button, D2D1_COLOR_F>(L"UnderMouseColor",
			[](Button& target) { return target.UnderMouseColor; },
			[](Button& target, const D2D1_COLOR_F& value) { target.UnderMouseColor = value; },
			{}, ButtonColorOptions(cui::theme::palette::AccentSoft, 10));
		BindingPropertyRegistry::Register<Button, D2D1_COLOR_F>(L"CheckedColor",
			[](Button& target) { return target.CheckedColor; },
			[](Button& target, const D2D1_COLOR_F& value) { target.CheckedColor = value; },
			{}, ButtonColorOptions(cui::theme::palette::AccentSelected, 20));
		BindingPropertyRegistry::Register<Button, D2D1_COLOR_F>(L"HighlightColor",
			[](Button& target) { return target.HighlightColor; },
			[](Button& target, const D2D1_COLOR_F& value) { target.HighlightColor = value; },
			{}, ButtonColorOptions(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f }, 30));
		BindingPropertyRegistry::Register<Button, D2D1_COLOR_F>(L"ShadowColor",
			[](Button& target) { return target.ShadowColor; },
			[](Button& target, const D2D1_COLOR_F& value) { target.ShadowColor = value; },
			{}, ButtonColorOptions(cui::theme::palette::Shadow, 40));
		BindingPropertyRegistry::Register<Button, D2D1_COLOR_F>(L"DisabledOverlayColor",
			[](Button& target) { return target.DisabledOverlayColor; },
			[](Button& target, const D2D1_COLOR_F& value) { target.DisabledOverlayColor = value; },
			{}, ButtonColorOptions(cui::theme::palette::DisabledOverlay, 50));
		BindingPropertyRegistry::Register<Button, bool>(L"Raised",
			[](Button& target) { return target.Raised; },
			[](Button& target, const bool& value) { target.Raised = value; },
			{}, []
			{
				auto options = ControlPropertyOptions<Button, bool>{
				false, ControlPropertyFlags::AffectsRender
					| ControlPropertyFlags::TracksLocalValue };
				options.Design.Category = L"Appearance";
				options.Design.CategoryOrder = 200;
				options.Design.Order = 60;
				options.Design.Editor = ControlPropertyEditorKind::Boolean;
				options.Design.Persistence = ControlPropertyPersistence::Metadata;
				return options;
			}());
		BindingPropertyRegistry::Register<Button, float>(L"BorderThickness",
			[](Button& target) { return target.BorderThickness; },
			[](Button& target, const float& value) { target.BorderThickness = value; },
			{}, ButtonNonNegativeFloatOptions(1.5f, 70));
		BindingPropertyRegistry::Register<Button, float>(L"Round",
			[](Button& target) { return target.Round; },
			[](Button& target, const float& value) { target.Round = value; },
			{}, ButtonNonNegativeFloatOptions(7.0f, 80));
		return true;
	}();
	(void)registered;
}

GET_CPP(Button, D2D1_COLOR_F, UnderMouseColor) { return _underMouseColor; }
SET_CPP(Button, D2D1_COLOR_F, UnderMouseColor) { SetPropertyField(L"UnderMouseColor", _underMouseColor, value); }
GET_CPP(Button, D2D1_COLOR_F, CheckedColor) { return _checkedColor; }
SET_CPP(Button, D2D1_COLOR_F, CheckedColor) { SetPropertyField(L"CheckedColor", _checkedColor, value); }
GET_CPP(Button, D2D1_COLOR_F, HighlightColor) { return _highlightColor; }
SET_CPP(Button, D2D1_COLOR_F, HighlightColor) { SetPropertyField(L"HighlightColor", _highlightColor, value); }
GET_CPP(Button, D2D1_COLOR_F, ShadowColor) { return _shadowColor; }
SET_CPP(Button, D2D1_COLOR_F, ShadowColor) { SetPropertyField(L"ShadowColor", _shadowColor, value); }
GET_CPP(Button, D2D1_COLOR_F, DisabledOverlayColor) { return _disabledOverlayColor; }
SET_CPP(Button, D2D1_COLOR_F, DisabledOverlayColor) { SetPropertyField(L"DisabledOverlayColor", _disabledOverlayColor, value); }
GET_CPP(Button, bool, Raised) { return _raised; }
SET_CPP(Button, bool, Raised) { SetPropertyField(L"Raised", _raised, value); }
GET_CPP(Button, float, BorderThickness) { return _borderThickness; }
SET_CPP(Button, float, BorderThickness) { SetPropertyField(L"BorderThickness", _borderThickness, value); }
GET_CPP(Button, float, Round) { return _round; }
SET_CPP(Button, float, Round) { SetPropertyField(L"Round", _round, value); }

Button::Button(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->Cursor = CursorKind::Hand;
}

bool Button::Invoke()
{
	if (!this->Enable || !this->Visible) return false;
	const auto size = GetActualSizeDip();
	MouseEventArgs eventArgs(MouseButtons::None, 0,
		static_cast<int>(size.width * 0.5f),
		static_cast<int>(size.height * 0.5f), 0);
	BeforeDefaultClick(WM_LBUTTONUP, eventArgs);
	OnMouseClick(this, eventArgs);
	InvalidateVisual();
	return true;
}

void Button::Update()
{
	if (!this->IsVisual) return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	const bool isPressed = HasControlStyleState(
		this->GetStyleState(), ControlStyleState::Pressed);
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	{
		const float border = (std::max)(0.0f, this->BorderThickness);
		const bool hasSurface = this->BackColor.a > 0.0f || this->Checked;
		const bool raised = this->Raised;
		const float pressOffset = (raised && isPressed) ? 1.0f : 0.0f;
		const float surfaceX = border * 0.5f;
		const float surfaceY = border * 0.5f + pressOffset;
		const float surfaceW = (std::max)(0.0f, actualWidth - border);
		const float surfaceH = (std::max)(0.0f, actualHeight - border - pressOffset);
		const float roundVal = (std::min)((std::max)(0.0f, this->Round), (std::min)(surfaceW, surfaceH) * 0.5f);
		const auto baseColor = this->Checked ? this->CheckedColor : this->BackColor;
		if (raised && hasSurface && ShadowColor.a > 0.0f && !isPressed)
			d2d->FillRoundRect(surfaceX, surfaceY + 1.0f, surfaceW, surfaceH, ShadowColor, roundVal);
		if (hasSurface)
		{
			d2d->FillRoundRect(surfaceX, surfaceY, surfaceW, surfaceH, baseColor, roundVal);
			if (raised && HighlightColor.a > 0.0f)
			{
				const float highlightH = (std::max)(1.0f, surfaceH * 0.34f);
				d2d->FillRoundRect(surfaceX + 1.0f, surfaceY + 1.0f, (std::max)(0.0f, surfaceW - 2.0f), highlightH, HighlightColor, (std::min)(roundVal, highlightH * 0.5f));
			}
		}
		if (isUnderMouse && this->UnderMouseColor.a > 0.0f)
			d2d->FillRoundRect(surfaceX, surfaceY, surfaceW, surfaceH, isPressed ? this->CheckedColor : this->UnderMouseColor, roundVal);
		if (this->Image)
			this->RenderImage(roundVal);
		const auto displayText = this->GetDisplayText();
		auto textSize = this->Font->GetTextSize(displayText);
		const float horizontalPad = 6.0f;
		const float textWidth = (std::max)(1.0f, actualWidth - horizontalPad * 2.0f);
		float drawLeft = actualWidth > textSize.width ? (actualWidth - textSize.width) / 2.0f : horizontalPad;
		if (drawLeft < horizontalPad) drawLeft = horizontalPad;
		float drawTop = actualHeight > textSize.height ? (actualHeight - textSize.height) / 2.0f + pressOffset * 0.5f : 0.0f;
		d2d->DrawString(displayText, drawLeft, drawTop, textWidth, textSize.height + 2.0f, this->ForeColor, this->Font);
		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			d2d->DrawRoundRect(surfaceX, surfaceY,
				surfaceW, surfaceH,
				this->BorderColor, border, roundVal);
		}
	}

	if (!this->Enable)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, actualHeight * 0.28f);
	this->EndRender();
}
