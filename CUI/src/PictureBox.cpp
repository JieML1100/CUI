#pragma once
#include "PictureBox.h"
#include "Form.h"
#include <algorithm>
UIClass PictureBox::Type() { return UIClass::UI_PictureBox; }

void PictureBox::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		ControlPropertyOptions<PictureBox, ImageSizeMode> options;
		options.DefaultValue = ImageSizeMode::Zoom;
		options.Flags = ControlPropertyFlags::AffectsRender;
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 40;
		options.Design.Editor = ControlPropertyEditorKind::Choice;
		options.Design.Persistence = ControlPropertyPersistence::Legacy;
		options.Design.Choices = {
			{ L"Normal", BindingValue(ImageSizeMode::Normal) },
			{ L"CenterImage", BindingValue(ImageSizeMode::CenterImage) },
			{ L"Stretch", BindingValue(ImageSizeMode::StretchImage) },
			{ L"Zoom", BindingValue(ImageSizeMode::Zoom) }
		};
		BindingPropertyRegistry::Register<PictureBox, ImageSizeMode>(L"SizeMode",
			[](PictureBox& target) { return target.SizeMode; },
			[](PictureBox& target, const ImageSizeMode& value)
			{
				target.SizeMode = value;
				target.InvalidateVisual();
			}, {}, std::move(options));
		return true;
	}();
	(void)registered;
}
PictureBox::PictureBox(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
}
void PictureBox::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
	this->BeginRender();
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (radius > 0.0f)
			d2d->DrawRoundRect(this->BorderThickness * 0.5f, this->BorderThickness * 0.5f,
				(std::max)(0.0f, actualWidth - this->BorderThickness), (std::max)(0.0f, actualHeight - this->BorderThickness),
				this->BorderColor, this->BorderThickness, radius);
		else
			d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BorderColor, this->BorderThickness);
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f }, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
