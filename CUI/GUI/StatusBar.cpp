#include "StatusBar.h"
#include "Form.h"

#include <algorithm>

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
	this->BolderColor = D2D1_COLOR_F{ 1, 1, 1, 0.16f };
	this->Boder = 1.0f;
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
		return;
	}

	_separatorsX.clear();

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
	for (int i = 0; i < (int)_parts.size(); i++)
	{
		auto& part = _parts[i];
		if (!part.LabelCtrl) continue;
		int w = computedWidths[i];
		if (w < 0) w = 0;

		auto ts = part.LabelCtrl->ActualSize();
		int y = (this->Height - (int)ts.cy) / 2;
		if (y < 0) y = 0;

		part.LabelCtrl->Location = POINT{ x + _partInnerPadding, y };
		part.LabelCtrl->ForeColor = this->ForeColor;

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

	LayoutItems();
	Panel::Update();

	if (!this->IsVisual || !this->ParentForm) return;
	auto d2d = this->ParentForm->Render;
	auto abs = this->AbsLocation;
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		for (float sx : _separatorsX)
		{
			float x = (float)abs.x + sx + (float)(Gap / 2);
			float y1 = (float)abs.y + 5.0f;
			float y2 = (float)abs.y + (float)this->Height - 5.0f;
			d2d->DrawLine(x, y1, x, y2, _separatorColor, 1.0f);
		}
	}
	d2d->PopDrawRect();
}
