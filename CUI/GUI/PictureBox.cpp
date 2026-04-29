#pragma once
#include "PictureBox.h"
#include "Form.h"
UIClass PictureBox::Type() { return UIClass::UI_PictureBox; }
PictureBox::PictureBox(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
}

bool PictureBox::LoadFromFile(const std::wstring& path)
{
	auto image = BitmapSource::FromFile(path);
	if (!image) return false;
	this->Image = image;
	return true;
}

void PictureBox::ClearImage()
{
	this->Image = nullptr;
}

bool PictureBox::SizeToImage()
{
	if (!this->Image) return false;
	auto pixelSize = this->Image->GetPixelSize();
	this->Size = SIZE{ static_cast<LONG>(pixelSize.width), static_cast<LONG>(pixelSize.height) };
	return true;
}

void PictureBox::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
