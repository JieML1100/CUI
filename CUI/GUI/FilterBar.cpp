#include "FilterBar.h"
#include "Form.h"
#include <algorithm>
#include <utility>

namespace
{
	bool PointInRect(float x, float y, const D2D1_RECT_F& rect)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	float RectWidth(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.right - rect.left);
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.bottom - rect.top);
	}
}

FilterBarItem::FilterBarItem(std::wstring text, std::wstring value, bool selected)
	: Text(std::move(text)), Value(std::move(value)), Selected(selected)
{
}

UIClass FilterBar::Type()
{
	return UIClass::UI_FilterBar;
}

FilterBar::FilterBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BolderColor = D2D1_COLOR_F{ 0.60f, 0.66f, 0.76f, 0.42f };
	this->ForeColor = D2D1_COLOR_F{ 0.90f, 0.92f, 0.96f, 1.0f };
	this->Cursor = CursorKind::Arrow;
}

int FilterBar::AddItem(const FilterBarItem& item)
{
	Items.push_back(item);
	PostRender();
	return (int)Items.size() - 1;
}

void FilterBar::ClearItems()
{
	Items.clear();
	PostRender();
}

void FilterBar::ClearSelection()
{
	for (auto& item : Items)
		item.Selected = false;
	PostRender();
}

std::vector<std::wstring> FilterBar::GetSelectedValues() const
{
	std::vector<std::wstring> values;
	for (const auto& item : Items)
	{
		if (!item.Selected) continue;
		values.push_back(item.Value.empty() ? item.Text : item.Value);
	}
	return values;
}

void FilterBar::SetQueryText(const std::wstring& text)
{
	if (QueryText == text) return;
	std::wstring old = QueryText;
	QueryText = text;
	NotifyQueryChanged(old);
	PostRender();
}

CursorKind FilterBar::QueryCursor(int xof, int yof)
{
	if (!Enable) return CursorKind::Arrow;
	auto size = ActualSize();
	BuildLayout((float)size.cx, (float)size.cy);
	auto hit = HitTest(xof, yof);
	if (hit.Kind == HitKind::Search)
		return CursorKind::IBeam;
	if (hit.Kind == HitKind::Chip || hit.Kind == HitKind::Apply || hit.Kind == HitKind::Reset)
		return CursorKind::Hand;
	return CursorKind::Arrow;
}

void FilterBar::BuildLayout(float width, float height)
{
	_regions.clear();
	_searchRect = D2D1::RectF(0, 0, 0, 0);

	const float pad = 10.0f;
	const float gap = 8.0f;
	const float cy = height * 0.5f;
	float right = width - pad;

	if (ShowActions)
	{
		float actionH = (std::min)(30.0f, (std::max)(24.0f, height - pad * 1.6f));
		float actionY = cy - actionH * 0.5f;
		float applyW = 64.0f;
		float resetW = 58.0f;
		_regions.push_back(HitRegion{ HitKind::Apply, -1, D2D1::RectF(right - applyW, actionY, right, actionY + actionH) });
		right -= applyW + gap;
		_regions.push_back(HitRegion{ HitKind::Reset, -1, D2D1::RectF(right - resetW, actionY, right, actionY + actionH) });
		right -= resetW + gap;
	}

	float x = pad;
	if (ShowSearchBox)
	{
		float searchW = (std::min)(SearchBoxWidth, (std::max)(120.0f, right - x));
		float searchH = (std::min)(32.0f, (std::max)(24.0f, height - pad * 1.4f));
		_searchRect = D2D1::RectF(x, cy - searchH * 0.5f, x + searchW, cy + searchH * 0.5f);
		_regions.push_back(HitRegion{ HitKind::Search, -1, _searchRect });
		x += searchW + gap;
	}

	float chipH = (std::min)(ChipHeight, (std::max)(22.0f, height - pad * 1.7f));
	float chipY = cy - chipH * 0.5f;
	for (int i = 0; i < (int)Items.size(); ++i)
	{
		if (!Items[i].Enabled) continue;
		auto textSize = Font->GetTextSize(Items[i].Text);
		float chipW = std::clamp(textSize.width + 22.0f, 52.0f, 150.0f);
		if (x + chipW > right)
			break;
		_regions.push_back(HitRegion{ HitKind::Chip, i, D2D1::RectF(x, chipY, x + chipW, chipY + chipH) });
		x += chipW + gap;
	}
}

FilterBar::HitRegion FilterBar::HitTest(int xof, int yof)
{
	float x = (float)xof;
	float y = (float)yof;
	for (auto it = _regions.rbegin(); it != _regions.rend(); ++it)
	{
		if (PointInRect(x, y, it->Rect))
			return *it;
	}
	return HitRegion{};
}

void FilterBar::NotifyQueryChanged(const std::wstring& oldText)
{
	OnQueryChanged(this, QueryText);
	OnTextChanged(this, oldText, QueryText);
}

void FilterBar::ResetFilters()
{
	std::wstring old = QueryText;
	QueryText.clear();
	for (auto& item : Items)
		item.Selected = false;
	if (old != QueryText)
		NotifyQueryChanged(old);
	OnReset(this);
	PostRender();
}

void FilterBar::Update()
{
	if (!IsVisual) return;
	auto d2d = ParentForm->Render;
	auto size = ActualSize();
	float width = (float)size.cx;
	float height = (float)size.cy;
	BuildLayout(width, height);

	BeginRender();
	d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, SurfaceColor, CornerRadius);
	d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, BolderColor, Border, CornerRadius);

	for (const auto& region : _regions)
	{
		if (region.Kind == HitKind::Search)
		{
			d2d->FillRoundRect(region.Rect, InputBackColor, 6.0f);
			d2d->DrawRoundRect(region.Rect, _searchFocused ? AccentColor : BolderColor, _searchFocused ? 1.4f : 1.0f, 6.0f);
			std::wstring text = QueryText.empty() ? Placeholder : QueryText;
			D2D1_COLOR_F color = QueryText.empty() ? MutedTextColor : ForeColor;
			d2d->PushDrawRect(region.Rect.left + 10.0f, region.Rect.top, RectWidth(region.Rect) - 20.0f, RectHeight(region.Rect));
			d2d->DrawString(text, region.Rect.left + 10.0f, region.Rect.top + 6.0f, color, Font);
			if (_searchFocused && ParentForm && ParentForm->Selected == this)
			{
				auto textSize = Font->GetTextSize(QueryText);
				float caretX = (std::min)(region.Rect.right - 10.0f, region.Rect.left + 11.0f + textSize.width);
				d2d->DrawLine(caretX, region.Rect.top + 7.0f, caretX, region.Rect.bottom - 7.0f, AccentColor, 1.2f);
			}
			d2d->PopDrawRect();
			continue;
		}

		if (region.Kind == HitKind::Chip)
		{
			const auto& item = Items[region.Index];
			D2D1_COLOR_F back = item.Selected ? ChipSelectedBackColor : ChipBackColor;
			d2d->FillRoundRect(region.Rect, back, RectHeight(region.Rect) * 0.5f);
			if (_hoverKind == HitKind::Chip && _hoverIndex == region.Index)
				d2d->FillRoundRect(region.Rect, HoverColor, RectHeight(region.Rect) * 0.5f);
			d2d->DrawRoundRect(region.Rect, item.Selected ? AccentColor : BolderColor, item.Selected ? 1.2f : 1.0f, RectHeight(region.Rect) * 0.5f);
			d2d->DrawString(item.Text, region.Rect.left + 11.0f, region.Rect.top + 5.0f, item.Selected ? ForeColor : MutedTextColor, Font);
			continue;
		}

		if (region.Kind == HitKind::Reset || region.Kind == HitKind::Apply)
		{
			bool apply = region.Kind == HitKind::Apply;
			D2D1_COLOR_F back = apply ? ButtonBackColor : ChipBackColor;
			D2D1_COLOR_F text = apply ? ButtonTextColor : MutedTextColor;
			d2d->FillRoundRect(region.Rect, back, 6.0f);
			if (_hoverKind == region.Kind)
				d2d->FillRoundRect(region.Rect, HoverColor, 6.0f);
			d2d->DrawRoundRect(region.Rect, apply ? AccentColor : BolderColor, 1.0f, 6.0f);
			d2d->DrawStringCentered(apply ? L"Apply" : L"Reset", region.Rect.left + RectWidth(region.Rect) * 0.5f, region.Rect.top + RectHeight(region.Rect) * 0.5f, text, Font);
		}
	}

	if (!Enable)
		d2d->FillRoundRect(0, 0, width, height, D2D1_COLOR_F{ 1,1,1,0.45f }, CornerRadius);
	EndRender();
}

bool FilterBar::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!Enable || !Visible) return true;
	(void)lParam;
	auto size = ActualSize();
	BuildLayout((float)size.cx, (float)size.cy);

	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		auto hit = HitTest(xof, yof);
		int oldIndex = _hoverIndex;
		HitKind oldKind = _hoverKind;
		_hoverKind = hit.Kind;
		_hoverIndex = hit.Index;
		if (oldIndex != _hoverIndex || oldKind != _hoverKind)
			PostRender();
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		OnMouseMove(this, e);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		auto hit = HitTest(xof, yof);
		_searchFocused = hit.Kind == HitKind::Search;
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		if (hit.Kind == HitKind::Chip && hit.Index >= 0 && hit.Index < (int)Items.size())
		{
			Items[hit.Index].Selected = !Items[hit.Index].Selected;
			OnFilterChanged(this, hit.Index, Items[hit.Index].Selected);
			if (ApplyOnFilterChange)
				OnApply(this);
		}
		else if (hit.Kind == HitKind::Apply)
		{
			OnApply(this);
		}
		else if (hit.Kind == HitKind::Reset)
		{
			ResetFilters();
		}
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseDown(this, e);
		if (hit.Kind != HitKind::Reset)
			PostRender();
		break;
	}
	case WM_LBUTTONUP:
	{
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseUp(this, e);
		break;
	}
	case WM_CHAR:
	{
		if (_searchFocused)
		{
			wchar_t ch = (wchar_t)wParam;
			std::wstring old = QueryText;
			if (ch == 8)
			{
				if (!QueryText.empty())
					QueryText.pop_back();
			}
			else if (ch == 13)
			{
				OnApply(this);
			}
			else if (ch >= 32 && ch != 127)
			{
				QueryText.push_back(ch);
			}
			if (old != QueryText)
			{
				NotifyQueryChanged(old);
				PostRender();
			}
		}
		break;
	}
	case WM_KEYDOWN:
	{
		if (_searchFocused && wParam == VK_RETURN)
			OnApply(this);
		if (_searchFocused && wParam == VK_ESCAPE)
		{
			_searchFocused = false;
			if (ParentForm && ParentForm->Selected == this)
				ParentForm->SetSelectedControl(NULL, false);
			PostRender();
		}
		KeyEventArgs e((Keys)(wParam | 0));
		OnKeyDown(this, e);
		break;
	}
	default:
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}
	return true;
}
