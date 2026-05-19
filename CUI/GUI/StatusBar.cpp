#include "StatusBar.h"
#include "Form.h"

#include <algorithm>
#include <cmath>

UIClass StatusBar::Type() { return UIClass::UI_StatusBar; }

void StatusBar::UpdateCompatPointers()
{
	_leftLabel = nullptr;
	_rightLabel = nullptr;
	if (_parts.empty()) return;
	_leftLabel = _parts.front().LabelCtrl;
	_rightLabel = _parts.back().LabelCtrl;
}

void StatusBar::EnsureDefaultParts()
{
	if (!_parts.empty()) return;
	AddPart(L"", -1);
	AddPart(L"", 0);
}

StatusBar::StatusBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };

	this->BackColor = D2D1_COLOR_F{ 1, 1, 1, 0.08f };
	this->BorderColor = D2D1_COLOR_F{ 1, 1, 1, 0.12f };
	this->BorderThickness = 1.0f;
	this->ForeColor = Colors::WhiteSmoke;
}

int StatusBar::AddPart(const std::wstring& text, int width)
{
	auto label = this->AddControl(new Label(text, 0, 0));
	label->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	label->ForeColor = this->ForeColor;
	_parts.push_back(Part{ label, width });
	UpdateCompatPointers();
	return (int)_parts.size() - 1;
}

void StatusBar::ClearParts()
{
	for (auto& p : _parts)
	{
		if (p.LabelCtrl)
		{
			this->RemoveControl(p.LabelCtrl);
			delete p.LabelCtrl;
			p.LabelCtrl = nullptr;
		}
	}
	_parts.clear();
	_separatorsX.clear();
	_partRects.clear();
	UpdateCompatPointers();
}

int StatusBar::PartCount() const
{
	return (int)_parts.size();
}

void StatusBar::SetPartText(int index, const std::wstring& text)
{
	if (index < 0 || index >= (int)_parts.size()) return;
	if (_parts[index].LabelCtrl) _parts[index].LabelCtrl->Text = text;
}

std::wstring StatusBar::GetPartText(int index) const
{
	if (index < 0 || index >= (int)_parts.size()) return L"";
	auto lbl = _parts[index].LabelCtrl;
	return lbl ? lbl->Text : L"";
}

int StatusBar::GetPartWidth(int index) const
{
	if (index < 0 || index >= (int)_parts.size()) return 0;
	return _parts[index].Width;
}

void StatusBar::SetPartWidth(int index, int width)
{
	if (index < 0 || index >= (int)_parts.size()) return;
	_parts[index].Width = width;
}

void StatusBar::SetLeftText(const std::wstring& text)
{
	EnsureDefaultParts();
	UpdateCompatPointers();
	if (_leftLabel) _leftLabel->Text = text;
}

void StatusBar::SetRightText(const std::wstring& text)
{
	EnsureDefaultParts();
	UpdateCompatPointers();
	if (_rightLabel) _rightLabel->Text = text;
}

std::wstring StatusBar::GetLeftText() const
{
	return _leftLabel ? _leftLabel->Text : L"";
}

std::wstring StatusBar::GetRightText() const
{
	return _rightLabel ? _rightLabel->Text : L"";
}

void StatusBar::LayoutItems()
{
	if (!this->ParentForm) return;
	UpdateCompatPointers();
	if (_parts.empty())
	{
		_separatorsX.clear();
		_partRects.clear();
		return;
	}

	_separatorsX.clear();
	_partRects.assign(_parts.size(), D2D1::RectF(0, 0, 0, 0));

	const int gapTotal = (std::max)(0, (int)_parts.size() - 1) * Gap;
	const int contentWidth = (std::max)(0, this->Width - Padding * 2 - gapTotal);
	std::vector<int> springIndices;
	int fixedSum = 0;
	std::vector<int> computedWidths;
	computedWidths.reserve(_parts.size());

	for (int i = 0; i < (int)_parts.size(); i++)
	{
		const auto& part = _parts[i];
		int w = part.Width;
		if (w < 0)
		{
			springIndices.push_back(i);
			computedWidths.push_back(0);
			continue;
		}
		if (!part.LabelCtrl)
		{
			computedWidths.push_back(0);
			continue;
		}

		if (w == 0)
		{
			auto ts = part.LabelCtrl->ActualSize();
			w = (int)ts.cx + _partInnerPadding * 2;
		}
		computedWidths.push_back((std::max)(0, w));
		fixedSum += (std::max)(0, w);
	}

	if (springIndices.empty() && !_parts.empty())
	{
		springIndices.push_back(0);
	}

	int remaining = contentWidth - fixedSum;
	if (remaining < 0) remaining = 0;
	if (!springIndices.empty())
	{
		int each = remaining / (int)springIndices.size();
		int extra = remaining - each * (int)springIndices.size();
		for (size_t si = 0; si < springIndices.size(); si++)
		{
			int idx = springIndices[si];
			computedWidths[idx] = each + ((si == springIndices.size() - 1) ? extra : 0);
		}
	}

	int x = Padding;
	const float partTop = 4.0f;
	const float partBottom = (std::max)(partTop, (float)this->Height - 3.0f);
	for (int i = 0; i < (int)_parts.size(); i++)
	{
		auto& part = _parts[i];
		if (!part.LabelCtrl) continue;
		int w = computedWidths[i];
		if (w < 0) w = 0;

		auto ts = part.LabelCtrl->ActualSize();
		auto textSize = part.LabelCtrl->Font->GetTextSize(part.LabelCtrl->Text);
		int y = (this->Height - (int)ts.cy) / 2;
		if (y < 0) y = 0;

		float textX = (float)(x + _partInnerPadding);
		if (part.Width >= 0 && w > (int)textSize.width + _partInnerPadding * 2)
			textX = (float)x + ((float)w - textSize.width) * 0.5f;
		part.LabelCtrl->SetRuntimeLocation(POINT{ (int)std::lround(textX), y });
		part.LabelCtrl->ForeColor = this->ForeColor;
		if (i >= 0 && i < (int)_partRects.size())
			_partRects[i] = D2D1::RectF((float)x, partTop, (float)(x + w), partBottom);

		x += w;
		if (i != (int)_parts.size() - 1)
		{
			_separatorsX.push_back((float)x);
			x += Gap;
		}
	}
}

void StatusBar::Update()
{
	if (this->ParentForm && this->TopMost)
	{
		this->ParentForm->MainStatusBar = this;
	}

	if (this->IsVisual == false || !this->ParentForm) return;

	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformLayout();
	}

	LayoutItems();

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		const float border = (std::max)(0.0f, this->BorderThickness);
		const D2D1_RECT_F surface = D2D1::RectF(border * 0.5f, border * 0.5f,
			(std::max)(border * 0.5f, actualWidth - border * 0.5f),
			(std::max)(border * 0.5f, actualHeight - border * 0.5f));
		if (this->CornerRadius > 0.0f)
			d2d->FillRoundRect(surface, this->BackColor, this->CornerRadius);
		else
			d2d->FillRect(surface, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(this->CornerRadius);
		}
		for (int i = 0; i < (int)_parts.size() && i < (int)_partRects.size(); i++)
		{
			if (!this->UsePartPills || _parts[i].Width < 0) continue;
			const auto& rect = _partRects[i];
			const float rectW = rect.right - rect.left;
			const float rectH = rect.bottom - rect.top;
			if (rectW <= 0.0f || rectH <= 0.0f) continue;
			if (this->PartBackColor.a > 0.0f)
				d2d->FillRoundRect(rect, this->PartBackColor, this->PartCornerRadius);
			if (this->PartBorderColor.a > 0.0f)
				d2d->DrawRoundRect(rect, this->PartBorderColor, 1.0f, this->PartCornerRadius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (int i = 0; i < this->Count; i++)
			{
				auto c = this->operator[](i);
				if (!c) continue;
				c->Update();
			}
		}
		for (float sx : _separatorsX)
		{
			float x = sx + (float)(Gap / 2);
			float y = 8.0f;
			float h = (std::max)(1.0f, actualHeight - y * 2.0f);
			if (this->SeparatorColor.a > 0.0f)
				d2d->FillRoundRect(x, y, 1.0f, h, this->SeparatorColor, 0.5f);
		}
		if (this->ShowTopLine && this->TopLineColor.a > 0.0f)
		{
			d2d->DrawLine(0.0f, 0.5f, actualWidth, 0.5f, this->TopLineColor, 1.0f);
		}
		if (this->ShowBorder && border > 0.0f && this->BorderColor.a > 0.0f)
		{
			if (this->CornerRadius > 0.0f)
				d2d->DrawRoundRect(surface, this->BorderColor, border, this->CornerRadius);
			else
				d2d->DrawRect(surface, this->BorderColor, border);
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
