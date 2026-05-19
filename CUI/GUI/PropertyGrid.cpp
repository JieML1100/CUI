#define NOMINMAX
#include "PropertyGrid.h"
#include "ColorPickerPopup.h"
#include "DropDownPopup.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <utility>

#pragma comment(lib, "Imm32.lib")

namespace
{
	static float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	static float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	static bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	static D2D1_COLOR_F FadeColor(D2D1_COLOR_F c, float alphaScale)
	{
		c.a *= alphaScale;
		return c;
	}

	static float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	static float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - std::pow(1.0f - t, 3.0f);
	}

	static D2D1_POINT_2F RotatePoint(const D2D1_POINT_2F& point, float cx, float cy, float angle)
	{
		const float dx = point.x - cx;
		const float dy = point.y - cy;
		const float s = std::sin(angle);
		const float c = std::cos(angle);
		return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
	}

	static D2D1_RECT_F IntersectRectF(const D2D1_RECT_F& a, const D2D1_RECT_F& b)
	{
		return D2D1::RectF(
			std::max(a.left, b.left),
			std::max(a.top, b.top),
			std::min(a.right, b.right),
			std::min(a.bottom, b.bottom));
	}

	static bool IsEmptyRectF(const D2D1_RECT_F& rect)
	{
		return rect.right <= rect.left || rect.bottom <= rect.top;
	}

	static void DrawDropChevron(D2DGraphics* d2d, float cx, float cy, float progress, D2D1_COLOR_F color)
	{
		progress = std::clamp(progress, 0.0f, 1.0f);
		const float angle = progress * 3.14159265359f;
		auto p1 = D2D1::Point2F(cx - 4.0f, cy - 2.0f);
		auto p2 = D2D1::Point2F(cx, cy + 3.0f);
		auto p3 = D2D1::Point2F(cx + 4.0f, cy - 2.0f);
		p1 = RotatePoint(p1, cx, cy, angle);
		p2 = RotatePoint(p2, cx, cy, angle);
		p3 = RotatePoint(p3, cx, cy, angle);
		d2d->DrawLine(p1, p2, color, 1.5f);
		d2d->DrawLine(p2, p3, color, 1.5f);
	}

	static void DrawCategoryChevron(D2DGraphics* d2d, float cx, float cy, float progress, D2D1_COLOR_F color)
	{
		progress = std::clamp(progress, 0.0f, 1.0f);
		const float angle = progress * 1.57079632679f;
		auto p1 = D2D1::Point2F(cx - 3.2f, cy - 5.0f);
		auto p2 = D2D1::Point2F(cx + 3.2f, cy);
		auto p3 = D2D1::Point2F(cx - 3.2f, cy + 5.0f);
		p1 = RotatePoint(p1, cx, cy, angle);
		p2 = RotatePoint(p2, cx, cy, angle);
		p3 = RotatePoint(p3, cx, cy, angle);
		d2d->DrawLine(p1, p2, color, 1.7f);
		d2d->DrawLine(p2, p3, color, 1.7f);
	}

	static float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + std::max(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	static std::wstring Trim(std::wstring s)
	{
		while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
		while (!s.empty() && iswspace(s.back())) s.pop_back();
		return s;
	}

	static std::wstring Lower(std::wstring s)
	{
		for (auto& ch : s)
			ch = (wchar_t)towlower(ch);
		return s;
	}

	static bool TextToBool(const std::wstring& value)
	{
		auto s = Lower(Trim(value));
		return s == L"true" || s == L"1" || s == L"yes" || s == L"on" || s == L"checked";
	}

}

PropertyGridItem::PropertyGridItem(std::wstring category, std::wstring name, std::wstring value, PropertyGridValueType type)
	: Category(std::move(category)), Name(std::move(name)), Value(std::move(value)), ValueType(type)
{
}

UIClass PropertyGridView::Type()
{
	return UIClass::UI_PropertyGrid;
}

PropertyGridView::PropertyGridView(int x, int y, int width, int height)
{
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.0f };
	this->BorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 0.72f };
}

PropertyGridView::~PropertyGridView()
{
	CloseColorPickerEditor();
	CloseDropDownEditor(true);
	if (this->_dropDownPopup)
	{
		delete this->_dropDownPopup;
		this->_dropDownPopup = NULL;
	}
	this->_dropDownPopupIndex = -1;
	if (this->_colorPicker)
	{
		delete this->_colorPicker;
		this->_colorPicker = NULL;
	}
	this->_colorPickerIndex = -1;
}

void PropertyGridView::Clear()
{
	this->Items.clear();
	this->SelectedIndex = -1;
	this->HoveredIndex = -1;
	this->ScrollYOffset = 0.0f;
	this->_collapsedCategories.clear();
	this->_categoryAnimations.clear();
	CloseColorPickerEditor();
	this->_colorPickerIndex = -1;
	CloseDropDownEditor();
	CancelEdit();
	this->SelectionChanged(this, -1);
	this->InvalidateVisual();
}

int PropertyGridView::AddItem(const PropertyGridItem& item)
{
	this->Items.push_back(item);
	this->InvalidateVisual();
	return (int)this->Items.size() - 1;
}

int PropertyGridView::AddProperty(const std::wstring& category, const std::wstring& name, const std::wstring& value, PropertyGridValueType type)
{
	return AddItem(PropertyGridItem(category, name, value, type));
}

bool PropertyGridView::RemoveItemAt(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	this->Items.erase(this->Items.begin() + index);
	if (this->SelectedIndex == index) this->SelectedIndex = -1;
	else if (this->SelectedIndex > index) this->SelectedIndex--;
	if (this->HoveredIndex == index) this->HoveredIndex = -1;
	else if (this->HoveredIndex > index) this->HoveredIndex--;
	if (_editingIndex == index) CancelEdit();
	else if (_editingIndex > index) _editingIndex--;
	if (_dropDownPopupIndex == index) CloseDropDownEditor();
	else if (_dropDownPopupIndex > index) _dropDownPopupIndex--;
	if (_colorPickerIndex == index)
	{
		CloseColorPickerEditor();
		_colorPickerIndex = -1;
	}
	else if (_colorPickerIndex > index) _colorPickerIndex--;
	this->SelectionChanged(this, this->SelectedIndex);
	this->InvalidateVisual();
	return true;
}

size_t PropertyGridView::ItemCount() const
{
	return this->Items.size();
}

PropertyGridItem* PropertyGridView::SelectedItem()
{
	return (this->SelectedIndex >= 0 && this->SelectedIndex < (int)this->Items.size()) ? &this->Items[this->SelectedIndex] : nullptr;
}

const PropertyGridItem* PropertyGridView::SelectedItem() const
{
	return (this->SelectedIndex >= 0 && this->SelectedIndex < (int)this->Items.size()) ? &this->Items[this->SelectedIndex] : nullptr;
}

bool PropertyGridView::SetValue(int index, const std::wstring& value)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	auto& item = this->Items[index];
	if (item.Value == value) return false;
	auto oldValue = item.Value;
	item.Value = value;
	this->OnValueChanged(this, index, oldValue, value);
	this->InvalidateVisual();
	return true;
}

std::wstring PropertyGridView::GetValue(int index) const
{
	return (index >= 0 && index < (int)this->Items.size()) ? this->Items[index].Value : L"";
}

void PropertyGridView::CollapseCategory(const std::wstring& category, bool collapsed)
{
	auto it = std::find(_collapsedCategories.begin(), _collapsedCategories.end(), category);
	if (collapsed)
	{
		if (it == _collapsedCategories.end())
			_collapsedCategories.push_back(category);
	}
	else if (it != _collapsedCategories.end())
	{
		_collapsedCategories.erase(it);
	}
	SetScrollOffset(this->ScrollYOffset);
	this->InvalidateVisual();
}

bool PropertyGridView::IsCategoryCollapsed(const std::wstring& category) const
{
	return std::find(_collapsedCategories.begin(), _collapsedCategories.end(), category) != _collapsedCategories.end();
}

void PropertyGridView::ToggleCategory(const std::wstring& category)
{
	bool collapsing = !IsCategoryCollapsed(category);
	StartCategoryAnimation(category, collapsing);
	CollapseCategory(category, collapsing);
}

void PropertyGridView::ExpandAll()
{
	for (const auto& category : _collapsedCategories)
		StartCategoryAnimation(category, false);
	_collapsedCategories.clear();
	this->InvalidateVisual();
}

void PropertyGridView::CollapseAll()
{
	std::vector<std::wstring> categories;
	for (const auto& item : this->Items)
	{
		if (!item.Category.empty() &&
			std::find(categories.begin(), categories.end(), item.Category) == categories.end())
			categories.push_back(item.Category);
	}
	for (const auto& category : categories)
	{
		if (!IsCategoryCollapsed(category))
			StartCategoryAnimation(category, true);
	}
	_collapsedCategories.clear();
	for (const auto& item : this->Items)
	{
		if (!item.Category.empty() &&
			std::find(_collapsedCategories.begin(), _collapsedCategories.end(), item.Category) == _collapsedCategories.end())
			_collapsedCategories.push_back(item.Category);
	}
	SetScrollOffset(this->ScrollYOffset);
	this->InvalidateVisual();
}

std::vector<PropertyGridView::RowInfo> PropertyGridView::BuildRows() const
{
	std::vector<RowInfo> rows;
	rows.reserve(this->Items.size() + 8);
	float y = 0.0f;
	for (int i = 0; i < (int)this->Items.size();)
	{
		std::wstring category = this->ShowCategories ? this->Items[i].Category : L"";
		if (this->ShowCategories && !category.empty())
		{
			RowInfo row;
			row.IsCategory = true;
			row.Category = category;
			row.Top = y;
			row.Height = std::max(18.0f, this->CategoryHeight);
			rows.push_back(row);
			y += row.Height;
		}

		std::vector<int> groupItems;
		float groupFullHeight = 0.0f;
		while (i < (int)this->Items.size())
		{
			std::wstring itemCategory = this->ShowCategories ? this->Items[i].Category : L"";
			if (itemCategory != category) break;
			groupItems.push_back(i);
			groupFullHeight += std::max(20.0f, this->RowHeight);
			i++;
		}

		const bool hasAnimatedCategory = this->ShowCategories && !category.empty();
		const bool collapsed = hasAnimatedCategory ? IsCategoryCollapsed(category) : false;
		const float progress = hasAnimatedCategory ? CategoryContentProgress(category, collapsed) : 1.0f;
		const float contentTop = y;
		const float visibleHeight = groupFullHeight * progress;
		if (visibleHeight > 0.001f)
		{
			float localY = 0.0f;
			for (int itemIndex : groupItems)
			{
				float itemHeight = std::max(20.0f, this->RowHeight);
				if (localY < visibleHeight)
				{
					RowInfo itemRow;
					itemRow.ItemIndex = itemIndex;
					itemRow.Category = category;
					itemRow.Top = contentTop + localY;
					itemRow.Height = itemHeight;
					itemRow.HasClip = hasAnimatedCategory && progress < 0.999f;
					itemRow.ClipTop = contentTop;
					itemRow.ClipBottom = contentTop + visibleHeight;
					rows.push_back(itemRow);
				}
				localY += itemHeight;
			}
		}
		y += visibleHeight;
	}
	return rows;
}

PropertyGridView::Layout PropertyGridView::CalcLayout(const std::vector<RowInfo>& rows) const
{
	Layout layout{};
	const float width = (float)this->_size.cx;
	const float height = (float)this->_size.cy;
	const float headerH = this->ShowHeader ? std::min(height, std::max(0.0f, this->HeaderHeight)) : 0.0f;
	layout.HeaderRect = D2D1::RectF(0.0f, 0.0f, width, headerH);
	layout.ContentRect = D2D1::RectF(0.0f, headerH, width, height);
	layout.ScrollBarSize = std::max(6.0f, this->ScrollBarSize);
	layout.ContentHeight = 0.0f;
	for (const auto& row : rows)
	{
		layout.ContentHeight = std::max(layout.ContentHeight, row.HasClip ? row.ClipBottom : row.Top + row.Height);
	}
	layout.NeedVScroll = layout.ContentHeight > RectHeight(layout.ContentRect) + 0.5f;
	if (layout.NeedVScroll)
	{
		layout.ContentRect.right = std::max(layout.ContentRect.left, layout.ContentRect.right - layout.ScrollBarSize);
		layout.HeaderRect.right = layout.ContentRect.right;
		layout.MaxScrollY = std::max(0.0f, layout.ContentHeight - RectHeight(layout.ContentRect));
		layout.ScrollTrackRect = D2D1::RectF(layout.ContentRect.right, layout.ContentRect.top, width, layout.ContentRect.bottom);
		float trackH = RectHeight(layout.ScrollTrackRect);
		float thumbH = layout.ContentHeight > 0.0f ? (RectHeight(layout.ContentRect) / layout.ContentHeight) * trackH : trackH;
		thumbH = std::clamp(thumbH, std::min(trackH, 18.0f), trackH);
		float offset = std::clamp(this->ScrollYOffset, 0.0f, layout.MaxScrollY);
		float thumbTop = layout.ScrollTrackRect.top;
		if (layout.MaxScrollY > 0.0f && trackH > thumbH)
			thumbTop += (offset / layout.MaxScrollY) * (trackH - thumbH);
		layout.ScrollThumbRect = D2D1::RectF(layout.ScrollTrackRect.left, thumbTop, layout.ScrollTrackRect.right, thumbTop + thumbH);
	}
	return layout;
}

D2D1_RECT_F PropertyGridView::GetRowRect(const RowInfo& row, const Layout& layout) const
{
	return D2D1::RectF(layout.ContentRect.left, layout.ContentRect.top + row.Top - this->ScrollYOffset,
		layout.ContentRect.right, layout.ContentRect.top + row.Top + row.Height - this->ScrollYOffset);
}

D2D1_RECT_F PropertyGridView::GetVisibleRowRect(const RowInfo& row, const Layout& layout) const
{
	auto rect = GetRowRect(row, layout);
	if (!row.HasClip)
		return rect;
	auto clipRect = D2D1::RectF(layout.ContentRect.left, layout.ContentRect.top + row.ClipTop - this->ScrollYOffset,
		layout.ContentRect.right, layout.ContentRect.top + row.ClipBottom - this->ScrollYOffset);
	return IntersectRectF(rect, clipRect);
}

D2D1_RECT_F PropertyGridView::GetNameRect(const D2D1_RECT_F& rowRect) const
{
	float split = rowRect.left + std::clamp(this->NameColumnWidth, 48.0f, std::max(48.0f, RectWidth(rowRect) - 48.0f));
	return D2D1::RectF(rowRect.left, rowRect.top, split, rowRect.bottom);
}

D2D1_RECT_F PropertyGridView::GetValueRect(const D2D1_RECT_F& rowRect) const
{
	auto nameRect = GetNameRect(rowRect);
	return D2D1::RectF(nameRect.right + this->SplitterWidth * 0.5f, rowRect.top, rowRect.right, rowRect.bottom);
}

void PropertyGridView::ClampScroll(Layout& layout)
{
	float clamped = std::clamp(this->ScrollYOffset, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 0.1f)
		this->ScrollYOffset = clamped;
}

void PropertyGridView::SetScrollOffset(float offsetY)
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	float clamped = std::clamp(offsetY, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 0.1f)
	{
		this->ScrollYOffset = clamped;
		this->ScrollChanged(this);
		this->InvalidateVisual();
	}
}

void PropertyGridView::EnsureVisible(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	for (const auto& row : rows)
	{
		if (row.ItemIndex != index) continue;
		auto rect = GetRowRect(row, layout);
		if (rect.top < layout.ContentRect.top)
			SetScrollOffset(this->ScrollYOffset - (layout.ContentRect.top - rect.top));
		else if (rect.bottom > layout.ContentRect.bottom)
			SetScrollOffset(this->ScrollYOffset + (rect.bottom - layout.ContentRect.bottom));
		break;
	}
}

int PropertyGridView::HitTestItem(int xof, int yof) const
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	for (const auto& row : rows)
	{
		if (row.IsCategory) continue;
		auto rect = GetVisibleRowRect(row, layout);
		if (PtInRectF(rect, (float)xof, (float)yof))
			return row.ItemIndex;
	}
	return -1;
}

bool PropertyGridView::GetValueRectForItem(int index, const std::vector<RowInfo>& rows, const Layout& layout, D2D1_RECT_F& outRect) const
{
	for (const auto& row : rows)
	{
		if (row.IsCategory || row.ItemIndex != index) continue;
		auto rect = GetRowRect(row, layout);
		outRect = GetValueRect(rect);
		return true;
	}
	outRect = D2D1::RectF();
	return false;
}

bool PropertyGridView::IsValueCell(int xof, int yof, const std::vector<RowInfo>& rows, const Layout& layout, int& itemIndex) const
{
	itemIndex = -1;
	for (const auto& row : rows)
	{
		if (row.IsCategory) continue;
		auto rect = GetRowRect(row, layout);
		auto visibleRect = GetVisibleRowRect(row, layout);
		auto valueRect = IntersectRectF(GetValueRect(rect), visibleRect);
		if (PtInRectF(valueRect, (float)xof, (float)yof))
		{
			itemIndex = row.ItemIndex;
			return true;
		}
	}
	return false;
}

bool PropertyGridView::IsOverSplitter(int xof, int yof) const
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	if (!PtInRectF(layout.ContentRect, (float)xof, (float)yof) && !PtInRectF(layout.HeaderRect, (float)xof, (float)yof))
		return false;
	float splitX = std::clamp(this->NameColumnWidth, 48.0f, std::max(48.0f, RectWidth(layout.ContentRect) - 48.0f));
	return std::fabs((float)xof - splitX) <= std::max(3.0f, this->SplitterWidth);
}

CursorKind PropertyGridView::QueryCursor(int xof, int yof)
{
	if (!this->Enable)
		return CursorKind::Arrow;
	if (_dragSplitter)
		return CursorKind::SizeWE;
	if (_dragVScroll)
		return CursorKind::SizeNS;
	if (IsOverSplitter(xof, yof))
		return CursorKind::SizeWE;

	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	if (layout.NeedVScroll && PtInRectF(layout.ScrollTrackRect, (float)xof, (float)yof))
		return CursorKind::SizeNS;

	for (const auto& row : rows)
	{
		auto rect = GetRowRect(row, layout);
		auto visibleRect = GetVisibleRowRect(row, layout);
		if (IsEmptyRectF(visibleRect) || !PtInRectF(visibleRect, (float)xof, (float)yof))
			continue;
		if (row.IsCategory)
			return CursorKind::Hand;

		auto valueRect = IntersectRectF(GetValueRect(rect), visibleRect);
		if (!PtInRectF(valueRect, (float)xof, (float)yof))
			return CursorKind::Arrow;
		if (!IsEditableItem(row.ItemIndex))
			return CursorKind::Arrow;

		const auto& item = this->Items[static_cast<size_t>(row.ItemIndex)];
		switch (item.ValueType)
		{
		case PropertyGridValueType::Text:
		case PropertyGridValueType::Number:
			return CursorKind::IBeam;
		case PropertyGridValueType::Bool:
		case PropertyGridValueType::Color:
			return CursorKind::Hand;
		case PropertyGridValueType::Enum:
			return item.Options.empty() ? CursorKind::Arrow : CursorKind::Hand;
		default:
			return CursorKind::Arrow;
		}
	}
	return CursorKind::Arrow;
}

bool PropertyGridView::CanHandleMouseWheel(int delta, int xof, int yof)
{
	(void)delta;
	(void)xof;
	(void)yof;
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	return layout.NeedVScroll;
}

bool PropertyGridView::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_UP:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_HOME:
	case VK_END:
	case VK_RETURN:
	case VK_ESCAPE:
	case VK_SPACE:
	case VK_F2:
	case VK_LEFT:
	case VK_RIGHT:
	case VK_BACK:
	case VK_DELETE:
		return true;
	default:
		return false;
	}
}

bool PropertyGridView::IsAnimationRunning()
{
	return !_categoryAnimations.empty() || IsCaretBlinkAnimating();
}

bool PropertyGridView::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (GetCaretBlinkInvalidRect(outRect))
		return true;
	outRect = this->AbsRect;
	return true;
}

void PropertyGridView::StartCategoryAnimation(const std::wstring& category, bool collapsing)
{
	if (category.empty()) return;
	float current = CategoryContentProgress(category, IsCategoryCollapsed(category));
	_categoryAnimations.erase(
		std::remove_if(_categoryAnimations.begin(), _categoryAnimations.end(),
			[&](const CategoryAnimation& anim) { return anim.Category == category; }),
		_categoryAnimations.end());
	CategoryAnimation anim;
	anim.Category = category;
	anim.Collapsing = collapsing;
	anim.StartProgress = current;
	anim.TargetProgress = collapsing ? 0.0f : 1.0f;
	if (std::fabs(anim.TargetProgress - anim.StartProgress) < 0.001f)
		return;
	anim.StartTick = GetTickCount64();
	_categoryAnimations.push_back(anim);
}

bool PropertyGridView::PruneCategoryAnimations()
{
	if (_categoryAnimations.empty()) return false;
	UINT64 now = GetTickCount64();
	size_t before = _categoryAnimations.size();
	_categoryAnimations.erase(
		std::remove_if(_categoryAnimations.begin(), _categoryAnimations.end(),
			[&](const CategoryAnimation& anim) {
				return now - anim.StartTick >= anim.DurationMs;
			}),
		_categoryAnimations.end());
	return before != _categoryAnimations.size();
}

float PropertyGridView::CategoryContentProgress(const std::wstring& category, bool collapsed) const
{
	float target = collapsed ? 0.0f : 1.0f;
	UINT64 now = GetTickCount64();
	for (const auto& anim : _categoryAnimations)
	{
		if (anim.Category != category) continue;
		float progress = anim.DurationMs > 0
			? std::clamp((float)(now - anim.StartTick) / (float)anim.DurationMs, 0.0f, 1.0f)
			: 1.0f;
		progress = EaseOutCubic(progress);
		return Lerp(anim.StartProgress, anim.TargetProgress, progress);
	}
	return target;
}

float PropertyGridView::CategoryChevronProgress(const std::wstring& category, bool collapsed) const
{
	return CategoryContentProgress(category, collapsed);
}

void PropertyGridView::DrawHeader(D2DGraphics* d2d, const Layout& layout)
{
	if (!this->ShowHeader || RectHeight(layout.HeaderRect) <= 0.0f) return;
	d2d->FillRoundRect(layout.HeaderRect, this->HeaderBackColor, this->CornerRadius);
	auto nameRect = GetNameRect(layout.HeaderRect);
	auto valueRect = GetValueRect(layout.HeaderRect);
	class Font* fontObj = this->Font;
	d2d->DrawString(L"Property", nameRect.left + this->CellPaddingX, TextTop(fontObj, nameRect),
		std::max(1.0f, RectWidth(nameRect) - this->CellPaddingX * 2.0f), RectHeight(nameRect), this->HeaderForeColor, fontObj);
	d2d->DrawString(L"Value", valueRect.left + this->CellPaddingX, TextTop(fontObj, valueRect),
		std::max(1.0f, RectWidth(valueRect) - this->CellPaddingX * 2.0f), RectHeight(valueRect), this->HeaderForeColor, fontObj);
	d2d->DrawLine(nameRect.right, layout.HeaderRect.top + 5.0f, nameRect.right, layout.HeaderRect.bottom - 5.0f, this->GridLineColor, 1.0f);
	d2d->DrawLine(layout.HeaderRect.left, layout.HeaderRect.bottom - 0.5f, layout.HeaderRect.right, layout.HeaderRect.bottom - 0.5f, this->GridLineColor, 1.0f);
}

void PropertyGridView::DrawCategoryRow(D2DGraphics* d2d, const RowInfo& row, const D2D1_RECT_F& rect)
{
	d2d->FillRoundRect(rect, this->CategoryBackColor, this->CornerRadius);
	const bool collapsed = IsCategoryCollapsed(row.Category);
	const float t = CategoryChevronProgress(row.Category, collapsed);
	float cx = rect.left + 12.0f;
	float cy = rect.top + RectHeight(rect) * 0.5f;
	DrawCategoryChevron(d2d, cx, cy, t, this->CategoryForeColor);
	class Font* fontObj = this->Font;
	d2d->DrawString(row.Category, rect.left + 24.0f, TextTop(fontObj, rect),
		std::max(1.0f, RectWidth(rect) - 32.0f), RectHeight(rect), this->CategoryForeColor, fontObj);
}

void PropertyGridView::DrawCheckBox(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool checked)
{
	d2d->FillRoundRect(rect, checked ? this->AccentColor : this->CheckBackColor, 3.0f);
	d2d->DrawRoundRect(rect, checked ? this->AccentColor : this->CheckBorderColor, 1.2f, 3.0f);
	if (checked)
	{
		D2D1_COLOR_F mark = Colors::White;
		auto p1 = D2D1::Point2F(rect.left + RectWidth(rect) * 0.24f, rect.top + RectHeight(rect) * 0.54f);
		auto p2 = D2D1::Point2F(rect.left + RectWidth(rect) * 0.43f, rect.bottom - RectHeight(rect) * 0.25f);
		auto p3 = D2D1::Point2F(rect.right - RectWidth(rect) * 0.20f, rect.top + RectHeight(rect) * 0.25f);
		d2d->DrawLine(p1, p2, mark, 1.8f);
		d2d->DrawLine(p2, p3, mark, 1.8f);
	}
}

void PropertyGridView::DrawItemRow(D2DGraphics* d2d, const RowInfo& row, const D2D1_RECT_F& rect, int visibleItemOrdinal)
{
	if (row.ItemIndex < 0 || row.ItemIndex >= (int)this->Items.size()) return;
	const auto& item = this->Items[row.ItemIndex];
	if (this->AlternatingRows && visibleItemOrdinal % 2 == 1)
		d2d->FillRoundRect(rect, this->AlternateRowBackColor, this->CornerRadius);
	if (row.ItemIndex == this->SelectedIndex)
		d2d->FillRoundRect(rect, this->SelectedItemBackColor, this->CornerRadius);
	else if (row.ItemIndex == this->HoveredIndex)
		d2d->FillRoundRect(rect, this->UnderMouseItemBackColor, this->CornerRadius);

	auto nameRect = GetNameRect(rect);
	auto valueRect = GetValueRect(rect);
	class Font* fontObj = this->Font;
	D2D1_COLOR_F nameColor = item.ReadOnly || item.ValueType == PropertyGridValueType::ReadOnly ? this->ReadOnlyForeColor : this->ForeColor;
	d2d->DrawString(item.Name, nameRect.left + this->CellPaddingX, TextTop(fontObj, nameRect),
		std::max(1.0f, RectWidth(nameRect) - this->CellPaddingX * 2.0f), RectHeight(nameRect), nameColor, fontObj);

	D2D1_COLOR_F valueColor = item.ReadOnly || item.ValueType == PropertyGridValueType::ReadOnly ? this->ReadOnlyForeColor : this->ForeColor;
	const bool editable = IsEditableItem(row.ItemIndex);
	if (_editing && _editingIndex == row.ItemIndex)
	{
		auto editRect = D2D1::RectF(valueRect.left + 3.0f, valueRect.top + 3.0f, valueRect.right - 3.0f, valueRect.bottom - 3.0f);
		d2d->FillRoundRect(editRect, this->EditBackColor, 4.0f);
		d2d->DrawRoundRect(editRect, this->AccentColor, 1.2f, 4.0f);
		d2d->DrawLine(nameRect.right, rect.top + 4.0f, nameRect.right, rect.bottom - 4.0f, this->GridLineColor, 1.0f);

		float renderHeight = RectHeight(editRect) - (this->EditTextMargin * 2.0f);
		if (renderHeight < 0.0f) renderHeight = 0.0f;
		EditEnsureSelectionInRange();
		EditUpdateScroll(RectWidth(editRect));

		float offsetY = 0.0f;
		if (fontObj)
		{
			auto textSize = fontObj->GetTextSize(_editingText, FLT_MAX, renderHeight);
			offsetY = std::max(0.0f, (RectHeight(editRect) - textSize.height) * 0.5f);
		}

		int sels = _editSelectionStart <= _editSelectionEnd ? _editSelectionStart : _editSelectionEnd;
		int sele = _editSelectionEnd >= _editSelectionStart ? _editSelectionEnd : _editSelectionStart;
		int selLen = sele - sels;
		bool caretRectValid = false;
		D2D1_RECT_F caretRect{};
		if (fontObj)
		{
			auto selRange = fontObj->HitTestTextRange(_editingText, (UINT32)sels, (UINT32)selLen);
			if (selLen != 0)
			{
				for (auto sr : selRange)
				{
					d2d->FillRect(
						sr.left + editRect.left + this->EditTextMargin - _editOffsetX,
						sr.top + editRect.top + offsetY,
						sr.width, sr.height,
						this->EditSelectedBackColor);
				}
			}
			else if (!selRange.empty())
			{
				const float caretX = selRange[0].left + editRect.left + this->EditTextMargin - _editOffsetX;
				const float caretTop = selRange[0].top + editRect.top + offsetY;
				const float caretBottom = selRange[0].top + editRect.top + selRange[0].height + offsetY;
				auto abs = this->AbsLocation;
				caretRect = { abs.x + caretX - 2.0f, abs.y + caretTop - 2.0f, abs.x + caretX + 2.0f, abs.y + caretBottom + 2.0f };
				caretRectValid = true;
			}
		}
		const bool focused = this->ParentForm && this->ParentForm->Selected == this;
		UpdateCaretBlinkState(focused, _editSelectionStart, _editSelectionEnd, caretRectValid, caretRectValid ? &caretRect : nullptr);

		d2d->PushDrawRect(editRect.left, editRect.top, RectWidth(editRect), RectHeight(editRect));
		auto layoutText = fontObj ? Factory::CreateStringLayout(_editingText, FLT_MAX, renderHeight, fontObj->FontObject) : nullptr;
		if (layoutText)
		{
			if (selLen != 0)
			{
				d2d->DrawStringLayoutEffect(layoutText,
					editRect.left + this->EditTextMargin - _editOffsetX, editRect.top + offsetY,
					this->EditForeColor,
					DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
					this->EditSelectedForeColor,
					fontObj);
			}
			else
			{
				d2d->DrawStringLayout(layoutText,
					editRect.left + this->EditTextMargin - _editOffsetX, editRect.top + offsetY,
					this->EditForeColor);
			}
			layoutText->Release();
		}
		if (caretRectValid && IsCaretBlinkVisible())
		{
			d2d->DrawLine(
				D2D1::Point2F(caretRect.left - this->AbsLocation.x + 2.0f, caretRect.top - this->AbsLocation.y + 2.0f),
				D2D1::Point2F(caretRect.left - this->AbsLocation.x + 2.0f, caretRect.bottom - this->AbsLocation.y - 2.0f),
				Colors::Black, 1.0f);
		}
		d2d->PopDrawRect();
	}
	else if (item.ValueType == PropertyGridValueType::Bool)
	{
		float box = std::min(15.0f, RectHeight(valueRect) - 8.0f);
		auto boxRect = D2D1::RectF(valueRect.left + this->CellPaddingX, valueRect.top + (RectHeight(valueRect) - box) * 0.5f,
			valueRect.left + this->CellPaddingX + box, valueRect.top + (RectHeight(valueRect) + box) * 0.5f);
		DrawCheckBox(d2d, boxRect, TextToBool(item.Value));
		d2d->DrawString(TextToBool(item.Value) ? L"True" : L"False", boxRect.right + 7.0f, TextTop(fontObj, valueRect),
			std::max(1.0f, valueRect.right - boxRect.right - 12.0f), RectHeight(valueRect), valueColor, fontObj);
	}
	else
	{
		auto contentRect = valueRect;
		float textLeft = contentRect.left + this->CellPaddingX;
		float textRight = contentRect.right - this->CellPaddingX;
		const bool hasDropArrow = editable &&
			((item.ValueType == PropertyGridValueType::Enum && !item.Options.empty()) || item.ValueType == PropertyGridValueType::Color);
		if (hasDropArrow)
			textRight -= 18.0f;
		if (item.ValueType == PropertyGridValueType::Color)
		{
			D2D1_COLOR_F swatch{};
			if (ColorPickerPopup::TryParseColor(item.Value, swatch))
			{
				auto swatchRect = D2D1::RectF(contentRect.left + this->CellPaddingX, contentRect.top + 5.0f,
					contentRect.left + this->CellPaddingX + 22.0f, contentRect.bottom - 5.0f);
				d2d->FillRoundRect(swatchRect, swatch, 3.0f);
				d2d->DrawRoundRect(swatchRect, this->GridLineColor, 1.0f, 3.0f);
				textLeft = swatchRect.right + 7.0f;
			}
		}
		d2d->DrawString(item.Value, textLeft, TextTop(fontObj, contentRect),
			std::max(1.0f, textRight - textLeft), RectHeight(contentRect), valueColor, fontObj);
		if (hasDropArrow)
		{
			float cx = contentRect.right - 12.0f;
			float cy = contentRect.top + RectHeight(contentRect) * 0.5f;
			float arrowProgress = 0.0f;
			if (item.ValueType == PropertyGridValueType::Color &&
				this->_colorPicker &&
				this->_colorPickerIndex == row.ItemIndex)
			{
				arrowProgress = this->_colorPicker->CurrentDropProgress();
			}
			else if (item.ValueType == PropertyGridValueType::Enum && IsDropDownEditorOpenFor(row.ItemIndex))
			{
				arrowProgress = this->_dropDownPopup ? this->_dropDownPopup->CurrentDropProgress() : 0.0f;
			}
			DrawDropChevron(d2d, cx, cy, arrowProgress, valueColor);
		}
	}

	d2d->DrawLine(nameRect.right, rect.top + 4.0f, nameRect.right, rect.bottom - 4.0f, this->GridLineColor, 1.0f);
	d2d->DrawLine(rect.left, rect.bottom - 0.5f, rect.right, rect.bottom - 0.5f, FadeColor(this->GridLineColor, 0.75f), 1.0f);
}

void PropertyGridView::DrawRows(D2DGraphics* d2d, const std::vector<RowInfo>& rows, const Layout& layout)
{
	int visibleItemOrdinal = 0;
	for (const auto& row : rows)
	{
		auto rect = GetRowRect(row, layout);
		auto visibleRect = GetVisibleRowRect(row, layout);
		if (IsEmptyRectF(visibleRect) || visibleRect.bottom < layout.ContentRect.top || visibleRect.top > layout.ContentRect.bottom)
		{
			if (!row.IsCategory) visibleItemOrdinal++;
			continue;
		}
		if (row.IsCategory)
			DrawCategoryRow(d2d, row, rect);
		else
		{
			auto clipRect = IntersectRectF(visibleRect, layout.ContentRect);
			if (!IsEmptyRectF(clipRect))
			{
				d2d->PushDrawRect(clipRect.left, clipRect.top, RectWidth(clipRect), RectHeight(clipRect));
				DrawItemRow(d2d, row, rect, visibleItemOrdinal);
				d2d->PopDrawRect();
			}
			visibleItemOrdinal++;
		}
	}
}

void PropertyGridView::DrawScrollBar(D2DGraphics* d2d, const Layout& layout)
{
	if (!layout.NeedVScroll) return;
	d2d->FillRoundRect(layout.ScrollTrackRect, this->ScrollBackColor, RectWidth(layout.ScrollTrackRect) * 0.5f);
	d2d->FillRoundRect(layout.ScrollThumbRect, this->ScrollForeColor, RectWidth(layout.ScrollThumbRect) * 0.5f);
}

void PropertyGridView::UpdateHover(int xof, int yof)
{
	int index = HitTestItem(xof, yof);
	if (index != this->HoveredIndex)
	{
		this->HoveredIndex = index;
		this->InvalidateVisual();
	}
}

void PropertyGridView::UpdateScrollByThumb(float yof)
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	if (!layout.NeedVScroll) return;
	float trackH = RectHeight(layout.ScrollTrackRect);
	float thumbH = RectHeight(layout.ScrollThumbRect);
	float movable = std::max(1.0f, trackH - thumbH);
	float newTop = std::clamp(yof - _scrollThumbGrabOffsetY, layout.ScrollTrackRect.top, layout.ScrollTrackRect.bottom - thumbH);
	SetScrollOffset(((newTop - layout.ScrollTrackRect.top) / movable) * layout.MaxScrollY);
}

void PropertyGridView::SelectItem(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	if (this->SelectedIndex == index) return;
	this->SelectedIndex = index;
	EnsureVisible(index);
	this->SelectionChanged(this, index);
	this->InvalidateVisual();
}

bool PropertyGridView::IsEditableItem(int index) const
{
	if (!this->AllowEditing) return false;
	if (index < 0 || index >= (int)this->Items.size()) return false;
	const auto& item = this->Items[index];
	return !item.ReadOnly && item.ValueType != PropertyGridValueType::ReadOnly;
}

void PropertyGridView::BeginEdit(int index)
{
	if (!IsEditableItem(index)) return;
	if (_editing && _editingIndex == index)
	{
		if (this->ParentForm)
			this->ParentForm->Selected = this;
		EditSetImeCompositionWindow();
		this->InvalidateVisual();
		return;
	}
	auto& item = this->Items[index];
	if (item.ValueType == PropertyGridValueType::Bool ||
		item.ValueType == PropertyGridValueType::Enum ||
		item.ValueType == PropertyGridValueType::Color)
		return;
	CloseDropDownEditor();
	_editing = true;
	_editingIndex = index;
	_editingText = item.Value;
	_editingOriginalText = item.Value;
	_editCaret = (int)_editingText.size();
	_editSelectionStart = 0;
	_editSelectionEnd = (int)_editingText.size();
	_editOffsetX = 0.0f;
	_imeCommittedTextToSuppress.clear();
	_imeCommitSuppressTick = 0;
	if (this->ParentForm)
		this->ParentForm->Selected = this;
	EditSetImeCompositionWindow();
	this->InvalidateVisual();
}

void PropertyGridView::CommitEdit()
{
	if (!_editing) return;
	int index = _editingIndex;
	std::wstring value = _editingText;
	_editing = false;
	_editingIndex = -1;
	_editingText.clear();
	_editingOriginalText.clear();
	_editCaret = 0;
	_dragEditSelection = false;
	_editSelectionStart = _editSelectionEnd = 0;
	_editOffsetX = 0.0f;
	_imeCommittedTextToSuppress.clear();
	_imeCommitSuppressTick = 0;
	SetValue(index, value);
	this->InvalidateVisual();
}

void PropertyGridView::CancelEdit()
{
	if (!_editing) return;
	_editing = false;
	_editingIndex = -1;
	_editingText.clear();
	_editingOriginalText.clear();
	_editCaret = 0;
	_dragEditSelection = false;
	_editSelectionStart = _editSelectionEnd = 0;
	_editOffsetX = 0.0f;
	_imeCommittedTextToSuppress.clear();
	_imeCommitSuppressTick = 0;
	this->InvalidateVisual();
}

void PropertyGridView::InsertEditChar(wchar_t ch)
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	int insertPos = _editSelectionStart <= _editSelectionEnd ? _editSelectionStart : _editSelectionEnd;
	int replaceEnd = _editSelectionEnd >= _editSelectionStart ? _editSelectionEnd : _editSelectionStart;
	if (_editingIndex >= 0 && _editingIndex < (int)this->Items.size() &&
		this->Items[_editingIndex].ValueType == PropertyGridValueType::Number)
	{
		bool allowed = iswdigit(ch) || ch == L'.' || ch == L'-' || ch == L'+';
		if (!allowed) return;
		std::wstring candidate = _editingText;
		if (replaceEnd > insertPos)
			candidate.erase((size_t)insertPos, (size_t)(replaceEnd - insertPos));
		if (ch == L'.' && candidate.find(L'.') != std::wstring::npos) return;
		if ((ch == L'-' || ch == L'+') && (insertPos != 0 || (!candidate.empty() && (candidate[0] == L'-' || candidate[0] == L'+'))))
			return;
	}
	std::wstring s;
	s.push_back(ch);
	int sels = insertPos;
	int sele = replaceEnd;
	if (sele > sels)
		_editingText.erase((size_t)sels, (size_t)(sele - sels));
	_editingText.insert((size_t)sels, s);
	_editSelectionStart = _editSelectionEnd = sels + 1;
	_editCaret = _editSelectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::BackspaceEdit()
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	int sels = _editSelectionStart <= _editSelectionEnd ? _editSelectionStart : _editSelectionEnd;
	int sele = _editSelectionEnd >= _editSelectionStart ? _editSelectionEnd : _editSelectionStart;
	if (sele > sels)
		_editingText.erase((size_t)sels, (size_t)(sele - sels));
	else if (sels > 0)
	{
		_editingText.erase((size_t)sels - 1, 1);
		sels--;
	}
	_editSelectionStart = _editSelectionEnd = sels;
	_editCaret = _editSelectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::DeleteEdit()
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	int sels = _editSelectionStart <= _editSelectionEnd ? _editSelectionStart : _editSelectionEnd;
	int sele = _editSelectionEnd >= _editSelectionStart ? _editSelectionEnd : _editSelectionStart;
	if (sele > sels)
		_editingText.erase((size_t)sels, (size_t)(sele - sels));
	else if (sels < (int)_editingText.size())
		_editingText.erase((size_t)sels, 1);
	_editSelectionStart = _editSelectionEnd = sels;
	_editCaret = _editSelectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::MoveEditCaret(int delta)
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	_editSelectionEnd = std::clamp(_editSelectionEnd + delta, 0, (int)_editingText.size());
	if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
		_editSelectionStart = _editSelectionEnd;
	_editCaret = _editSelectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::EditEnsureSelectionInRange()
{
	if (_editSelectionStart < 0) _editSelectionStart = 0;
	if (_editSelectionEnd < 0) _editSelectionEnd = 0;
	int maxLen = (int)_editingText.size();
	if (_editSelectionStart > maxLen) _editSelectionStart = maxLen;
	if (_editSelectionEnd > maxLen) _editSelectionEnd = maxLen;
	_editCaret = _editSelectionEnd;
}

void PropertyGridView::EditUpdateScroll(float cellWidth)
{
	if (!_editing) return;
	float renderWidth = cellWidth - (this->EditTextMargin * 2.0f);
	if (renderWidth <= 1.0f) return;
	EditEnsureSelectionInRange();
	class Font* fontObj = this->Font;
	if (!fontObj) return;
	auto hit = fontObj->HitTestTextRange(_editingText, (UINT32)_editSelectionEnd, (UINT32)0);
	if (hit.empty()) return;
	auto caret = hit[0];
	if ((caret.left + caret.width) - _editOffsetX > renderWidth)
		_editOffsetX = (caret.left + caret.width) - renderWidth;
	if (caret.left - _editOffsetX < 0.0f)
		_editOffsetX = caret.left;
	if (_editOffsetX < 0.0f) _editOffsetX = 0.0f;
}

int PropertyGridView::EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y)
{
	class Font* fontObj = this->Font;
	if (!fontObj) return 0;
	float renderHeight = cellHeight - (this->EditTextMargin * 2.0f);
	if (renderHeight < 0.0f) renderHeight = 0.0f;
	return fontObj->HitTestTextPosition(_editingText, FLT_MAX, renderHeight,
		(x - this->EditTextMargin) + _editOffsetX, y - this->EditTextMargin);
}

bool PropertyGridView::SetEditingCaretFromMousePoint(int xof, int yof, const D2D1_RECT_F& valueRect)
{
	if (!_editing) return false;
	auto editRect = D2D1::RectF(valueRect.left + 3.0f, valueRect.top + 3.0f, valueRect.right - 3.0f, valueRect.bottom - 3.0f);
	float cellWidth = RectWidth(editRect);
	float cellHeight = RectHeight(editRect);
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, (float)xof - editRect.left, (float)yof - editRect.top);
	_editSelectionStart = _editSelectionEnd = std::clamp(pos, 0, (int)_editingText.size());
	_editCaret = _editSelectionEnd;
	EditUpdateScroll(cellWidth);
	this->InvalidateVisual();
	return true;
}

bool PropertyGridView::UpdateEditingSelectionFromMousePoint(int xof, int yof, const D2D1_RECT_F& valueRect)
{
	if (!_editing) return false;
	auto editRect = D2D1::RectF(valueRect.left + 3.0f, valueRect.top + 3.0f, valueRect.right - 3.0f, valueRect.bottom - 3.0f);
	float cellWidth = RectWidth(editRect);
	float cellHeight = RectHeight(editRect);
	float localX = std::clamp((float)xof, editRect.left, editRect.right) - editRect.left;
	float localY = std::clamp((float)yof, editRect.top, editRect.bottom) - editRect.top;
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, localX, localY);
	_editSelectionEnd = std::clamp(pos, 0, (int)_editingText.size());
	_editCaret = _editSelectionEnd;
	EditUpdateScroll(cellWidth);
	this->InvalidateVisual();
	return true;
}

std::wstring PropertyGridView::EditGetSelectedString() const
{
	int sels = _editSelectionStart <= _editSelectionEnd ? _editSelectionStart : _editSelectionEnd;
	int sele = _editSelectionEnd >= _editSelectionStart ? _editSelectionEnd : _editSelectionStart;
	if (sele > sels && sels >= 0 && sele <= (int)_editingText.size())
		return _editingText.substr((size_t)sels, (size_t)(sele - sels));
	return L"";
}

void PropertyGridView::EditSetImeCompositionWindow()
{
	if (!this->ParentForm || !this->ParentForm->Handle || !_editing) return;
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	D2D1_RECT_F valueRect{};
	if (!GetValueRectForItem(_editingIndex, rows, layout, valueRect)) return;
	auto pos = this->AbsLocation;
	this->ParentForm->SetImeCompositionWindowFromLogicalRect(
		D2D1_RECT_F{
			(float)pos.x + valueRect.left,
			(float)pos.y + valueRect.top,
			(float)pos.x + valueRect.right,
			(float)pos.y + valueRect.bottom
		});
}

void PropertyGridView::HandleImeComposition(LPARAM lParam)
{
	if (!_editing || !this->ParentForm || this->ParentForm->Selected != this) return;
	if (lParam & GCS_RESULTSTR)
	{
		HIMC hIMC = ImmGetContext(this->ParentForm->Handle);
		if (hIMC)
		{
			LONG bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
			if (bytes > 0)
			{
				int wcharCount = bytes / (int)sizeof(wchar_t);
				std::wstring buffer;
				buffer.resize(wcharCount);
				ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buffer.data(), bytes);
				std::wstring committed;
				for (wchar_t ch : buffer)
				{
					if (ch >= 32)
						committed.push_back(ch);
				}
				if (!committed.empty())
				{
					_imeCommittedTextToSuppress = committed;
					_imeCommitSuppressTick = GetTickCount64();
					for (wchar_t ch : committed)
					{
						_editSelectionStart = _editSelectionEnd;
						InsertEditChar(ch);
					}
				}
			}
			ImmReleaseContext(this->ParentForm->Handle, hIMC);
		}
		this->InvalidateVisual();
	}
}

void PropertyGridView::ToggleBool(int index)
{
	if (!IsEditableItem(index)) return;
	SetValue(index, TextToBool(this->Items[index].Value) ? L"False" : L"True");
}

void PropertyGridView::CycleEnum(int index, int direction)
{
	if (!IsEditableItem(index)) return;
	auto& item = this->Items[index];
	if (item.Options.empty()) return;
	int current = 0;
	for (int i = 0; i < (int)item.Options.size(); i++)
	{
		if (item.Options[i] == item.Value)
		{
			current = i;
			break;
		}
	}
	int next = (current + direction) % (int)item.Options.size();
	if (next < 0) next += (int)item.Options.size();
	SetValue(index, item.Options[next]);
}

void PropertyGridView::CloseDropDownEditor(bool immediate)
{
	if (!this->_dropDownPopup) return;
	this->_dropDownPopup->Hide(!immediate, immediate);
	if (immediate)
		this->_dropDownPopupIndex = -1;
}

bool PropertyGridView::IsDropDownEditorOpenFor(int index) const
{
	return this->_dropDownPopup &&
		this->_dropDownPopup->IsOpen() &&
		this->_dropDownPopupIndex == index;
}

void PropertyGridView::CloseColorPickerEditor()
{
	if (!this->_colorPicker) return;
	this->_colorPicker->Hide(false);
}

void PropertyGridView::OpenColorPickerEditor(int index, const D2D1_RECT_F& valueRect)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	if (!this->ParentForm) return;
	if (!IsEditableItem(index)) return;
	if (this->Items[index].ValueType != PropertyGridValueType::Color) return;

	if (this->_colorPicker &&
		this->ParentForm->ForegroundControl == this->_colorPicker &&
		this->_colorPicker->Visible &&
		this->_colorPickerIndex == index)
	{
		CloseColorPickerEditor();
		this->ParentForm->Invalidate(true);
		this->InvalidateVisual();
		return;
	}

	CommitEdit();
	CloseDropDownEditor();

	if (!this->_colorPicker)
		this->_colorPicker = new ColorPickerPopup();

	D2D1_COLOR_F initial = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 1.0f };
	ColorPickerPopup::TryParseColor(this->Items[index].Value, initial);

	this->_colorPicker->ParentForm = this->ParentForm;
	this->_colorPicker->SetFontEx(this->Font, false);
	this->_colorPicker->AccentColor = this->AccentColor;

	this->_colorPicker->OnColorChanged.Clear();
	this->_colorPicker->OnColorConfirmed.Clear();
	this->_colorPicker->OnCleared.Clear();
	this->_colorPicker->OnCancelled.Clear();
	this->_colorPicker->OnColorConfirmed += [this, index](ColorPickerPopup* sender, D2D1_COLOR_F color, std::wstring value)
		{
			(void)sender;
			(void)color;
			if (index >= 0 && index < (int)this->Items.size())
				SetValue(index, value);
		};
	this->_colorPicker->OnCleared += [this, index](ColorPickerPopup* sender)
		{
			(void)sender;
			if (index >= 0 && index < (int)this->Items.size())
				SetValue(index, L"");
		};
	this->_colorPicker->OnCancelled += [this](ColorPickerPopup* sender)
		{
			(void)sender;
		};

	this->_colorPickerIndex = index;
	this->SelectedIndex = index;
	this->_colorPicker->ShowAt(this, valueRect, initial);
	this->InvalidateVisual();
}

void PropertyGridView::ToggleDropDownEditor(int index, const D2D1_RECT_F& valueRect)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	if (!this->ParentForm) return;
	if (!IsEditableItem(index)) return;
	auto& item = this->Items[index];
	if (item.ValueType != PropertyGridValueType::Enum || item.Options.empty()) return;

	if (IsDropDownEditorOpenFor(index))
	{
		CloseDropDownEditor();
		this->ParentForm->Invalidate(true);
		this->InvalidateVisual();
		return;
	}

	CommitEdit();
	CloseColorPickerEditor();

	if (!this->_dropDownPopup)
		this->_dropDownPopup = new DropDownPopup();

	int selected = 0;
	for (int i = 0; i < (int)item.Options.size(); i++)
	{
		if (item.Options[i] == item.Value)
		{
			selected = i;
			break;
		}
	}

	const auto abs = this->AbsLocation;
	const float x = (float)abs.x + valueRect.left + 3.0f;
	const float y = (float)abs.y + valueRect.top + 3.0f;
	const float w = std::max(1.0f, RectWidth(valueRect) - 6.0f);
	const float h = std::max(1.0f, RectHeight(valueRect) - 6.0f);

	this->_dropDownPopup->SetFontEx(this->Font, false);
	this->_dropDownPopup->DropBackColor = this->EditBackColor;
	this->_dropDownPopup->ForeColor = this->EditForeColor;
	this->_dropDownPopup->DropBorderColor = D2D1_COLOR_F{ 0.74f, 0.77f, 0.84f, 0.95f };
	this->_dropDownPopup->AccentColor = this->AccentColor;
	this->_dropDownPopup->SelectedItemBackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.14f };
	this->_dropDownPopup->SelectedItemForeColor = this->EditForeColor;
	this->_dropDownPopup->UnderMouseBackColor = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.09f };
	this->_dropDownPopup->UnderMouseForeColor = this->EditForeColor;
	this->_dropDownPopup->ScrollBackColor = this->ScrollBackColor;
	this->_dropDownPopup->ScrollForeColor = this->ScrollForeColor;
	this->_dropDownPopup->MinWidth = 118.0f;
	this->_dropDownPopup->CornerRadius = 6.0f;

	this->_dropDownPopup->SelectionChanged.Clear();
	this->_dropDownPopup->SelectionChanged += [this, index](DropDownPopup* sender, int selectedIndex, std::wstring selectedText)
		{
			(void)sender;
			(void)selectedText;
			if (index < 0 || index >= (int)this->Items.size()) return;
			auto& item2 = this->Items[index];
			if (item2.Options.empty()) return;
			if (selectedIndex < 0) selectedIndex = 0;
			if (selectedIndex >= (int)item2.Options.size()) selectedIndex = (int)item2.Options.size() - 1;
			SetValue(index, item2.Options[(size_t)selectedIndex]);
		};
	this->_dropDownPopup->Closed.Clear();
	this->_dropDownPopup->Closed += [this](DropDownPopup* sender)
		{
			(void)sender;
			this->_dropDownPopupIndex = -1;
			this->InvalidateVisual();
		};

	this->_dropDownPopupIndex = index;
	this->SelectedIndex = index;
	this->_dropDownPopup->ShowAt(this->ParentForm, this,
		D2D1::RectF(x, y, x + w, y + h),
		item.Options, selected, w, h, 4);
	this->ParentForm->Invalidate(true);
	this->InvalidateVisual();
}

void PropertyGridView::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	PruneCategoryAnimations();
	auto size = this->ActualSize();
	float width = (float)size.cx;
	float height = (float)size.cy;
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	ClampScroll(layout);
	layout = CalcLayout(rows);

	this->BeginRender();
	{
		d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, std::max(0.0f, width - Border), std::max(0.0f, height - Border), this->BackColor, this->CornerRadius);
		DrawHeader(d2d, layout);
		DrawRows(d2d, rows, layout);
		DrawScrollBar(d2d, layout);
		if (Border > 0.0f)
			d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, std::max(0.0f, width - Border), std::max(0.0f, height - Border), this->BorderColor, Border, this->CornerRadius);
		if (!this->Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.48f }, this->CornerRadius);
	}
	this->EndRender();

	if (!_categoryAnimations.empty())
		this->InvalidateVisual();
}

bool PropertyGridView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		CloseColorPickerEditor();
		CloseDropDownEditor();
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (delta != 0)
		{
			const float step = (float)std::max(8, this->MouseWheelStep);
			SetScrollOffset(this->ScrollYOffset + (delta < 0 ? step : -step));
		}
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, delta);
		this->OnMouseWheel(this, e);
		return true;
	}
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm)
			this->ParentForm->UnderMouse = this;
		if (_dragEditSelection && _editing)
		{
			auto rows = BuildRows();
			auto layout = CalcLayout(rows);
			D2D1_RECT_F valueRect{};
			if (GetValueRectForItem(_editingIndex, rows, layout, valueRect))
				UpdateEditingSelectionFromMousePoint(xof, yof, valueRect);
		}
		else if (_dragVScroll)
			UpdateScrollByThumb((float)yof);
		else if (_dragSplitter)
		{
			CloseDropDownEditor();
			CloseColorPickerEditor();
			auto rows = BuildRows();
			auto layout = CalcLayout(rows);
			this->NameColumnWidth = std::clamp((float)xof, 48.0f, std::max(48.0f, RectWidth(layout.ContentRect) - 48.0f));
			this->InvalidateVisual();
		}
		else
			UpdateHover(xof, yof);
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (this->ParentForm)
			this->ParentForm->SetSelectedControl(this, false);
		auto rows = BuildRows();
		auto layout = CalcLayout(rows);
		if (layout.NeedVScroll && PtInRectF(layout.ScrollThumbRect, (float)xof, (float)yof))
		{
			_dragVScroll = true;
			_scrollThumbGrabOffsetY = (float)yof - layout.ScrollThumbRect.top;
			return true;
		}
		if (layout.NeedVScroll && PtInRectF(layout.ScrollTrackRect, (float)xof, (float)yof))
		{
			SetScrollOffset(this->ScrollYOffset + ((float)yof < layout.ScrollThumbRect.top ? -RectHeight(layout.ContentRect) : RectHeight(layout.ContentRect)));
			return true;
		}
		if (IsOverSplitter(xof, yof))
		{
			_dragSplitter = true;
			return true;
		}

		bool handledRow = false;
		for (const auto& row : rows)
		{
			auto rect = GetRowRect(row, layout);
			if (!PtInRectF(rect, (float)xof, (float)yof)) continue;
			handledRow = true;
			if (row.IsCategory)
			{
				CancelEdit();
				CloseDropDownEditor();
				CloseColorPickerEditor();
				ToggleCategory(row.Category);
			}
			else
			{
				int valueIndex = -1;
				bool inValue = IsValueCell(xof, yof, rows, layout, valueIndex);
				SelectItem(row.ItemIndex);
				this->OnItemClick(this, row.ItemIndex);
				if (inValue && IsEditableItem(row.ItemIndex))
				{
					auto type = this->Items[row.ItemIndex].ValueType;
					if (type == PropertyGridValueType::Bool)
					{
						CloseDropDownEditor();
						CloseColorPickerEditor();
						ToggleBool(row.ItemIndex);
					}
					else if (type == PropertyGridValueType::Enum)
						ToggleDropDownEditor(row.ItemIndex, GetValueRect(rect));
					else if (type == PropertyGridValueType::Color)
					{
						CloseDropDownEditor();
						OpenColorPickerEditor(row.ItemIndex, GetValueRect(rect));
					}
					else
					{
						CloseDropDownEditor();
						CloseColorPickerEditor();
						if (_editing && _editingIndex == row.ItemIndex)
						{
							SetEditingCaretFromMousePoint(xof, yof, GetValueRect(rect));
							_dragEditSelection = true;
							if (this->ParentForm && this->ParentForm->Handle)
								SetCapture(this->ParentForm->Handle);
						}
						else
						{
							BeginEdit(row.ItemIndex);
							if (_editing && _editingIndex == row.ItemIndex)
								SetEditingCaretFromMousePoint(xof, yof, GetValueRect(rect));
							_dragEditSelection = true;
							if (this->ParentForm && this->ParentForm->Handle)
								SetCapture(this->ParentForm->Handle);
						}
					}
				}
				else if (_editing && _editingIndex != row.ItemIndex)
					CommitEdit();
				if (!inValue && _dropDownPopupIndex != row.ItemIndex)
					CloseDropDownEditor();
				if (!inValue && _colorPickerIndex != row.ItemIndex)
					CloseColorPickerEditor();
			}
			break;
		}
		if (!handledRow && _editing)
			CommitEdit();
		if (!handledRow)
		{
			CloseDropDownEditor();
			CloseColorPickerEditor();
		}
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		_dragVScroll = false;
		_dragSplitter = false;
		if (_dragEditSelection)
		{
			_dragEditSelection = false;
			ReleaseCapture();
		}
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, e);
		return true;
	}
	case WM_LBUTTONDBLCLK:
	{
		auto rows = BuildRows();
		auto layout = CalcLayout(rows);
		int index = HitTestItem(xof, yof);
		if (index >= 0)
		{
			D2D1_RECT_F valueRect{};
			auto type = this->Items[index].ValueType;
			if (_editing && _editingIndex == index &&
				GetValueRectForItem(index, rows, layout, valueRect) &&
				IsEditableItem(index) &&
				type != PropertyGridValueType::Enum &&
				type != PropertyGridValueType::Color &&
				type != PropertyGridValueType::Bool)
			{
				_editSelectionStart = 0;
				_editSelectionEnd = (int)_editingText.size();
				_editCaret = _editSelectionEnd;
				_dragEditSelection = false;
				this->InvalidateVisual();
			}
			else
			if (GetValueRectForItem(index, rows, layout, valueRect) && IsEditableItem(index) &&
				(type == PropertyGridValueType::Enum || type == PropertyGridValueType::Color))
			{
				if (type == PropertyGridValueType::Enum)
					ToggleDropDownEditor(index, valueRect);
				else
					OpenColorPickerEditor(index, valueRect);
			}
			else
				BeginEdit(index);
		}
		MouseEventArgs e(MouseButtons::Left, 2, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, e);
		return true;
	}
	case WM_KEYDOWN:
	{
		if (_editing)
		{
			EditSetImeCompositionWindow();
			EditEnsureSelectionInRange();
			switch (wParam)
			{
			case VK_RETURN: CommitEdit(); break;
			case VK_ESCAPE: CancelEdit(); break;
			case VK_BACK: BackspaceEdit(); break;
			case VK_DELETE: DeleteEdit(); break;
			case VK_LEFT: MoveEditCaret(-1); break;
			case VK_RIGHT: MoveEditCaret(1); break;
			case VK_HOME:
				_editSelectionEnd = 0;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					_editSelectionStart = _editSelectionEnd;
				_editCaret = _editSelectionEnd;
				this->InvalidateVisual();
				break;
			case VK_END:
				_editSelectionEnd = (int)_editingText.size();
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					_editSelectionStart = _editSelectionEnd;
				_editCaret = _editSelectionEnd;
				this->InvalidateVisual();
				break;
			default: break;
			}
			KeyEventArgs e((Keys)(wParam | 0));
			this->OnKeyDown(this, e);
			return true;
		}

		auto rows = BuildRows();
		auto layout = CalcLayout(rows);
		std::vector<int> visibleItems;
		for (const auto& row : rows)
			if (!row.IsCategory && row.ItemIndex >= 0)
				visibleItems.push_back(row.ItemIndex);
		auto selectVisible = [&](int pos) {
			if (visibleItems.empty()) return;
			pos = std::clamp(pos, 0, (int)visibleItems.size() - 1);
			SelectItem(visibleItems[pos]);
			};
		auto it = std::find(visibleItems.begin(), visibleItems.end(), this->SelectedIndex);
		int pos = it == visibleItems.end() ? -1 : (int)(it - visibleItems.begin());
		auto openSelectedDropDown = [&]() {
			if (this->SelectedIndex < 0 || this->SelectedIndex >= (int)this->Items.size()) return false;
			D2D1_RECT_F valueRect{};
			if (!GetValueRectForItem(this->SelectedIndex, rows, layout, valueRect)) return false;
			auto type = this->Items[this->SelectedIndex].ValueType;
			if (type == PropertyGridValueType::Enum)
			{
				ToggleDropDownEditor(this->SelectedIndex, valueRect);
				return true;
			}
			if (type == PropertyGridValueType::Color)
			{
				OpenColorPickerEditor(this->SelectedIndex, valueRect);
				return true;
			}
			return false;
			};
		switch (wParam)
		{
		case VK_UP: selectVisible(pos <= 0 ? 0 : pos - 1); break;
		case VK_DOWN: selectVisible(pos < 0 ? 0 : pos + 1); break;
		case VK_HOME: selectVisible(0); break;
		case VK_END: selectVisible((int)visibleItems.size() - 1); break;
		case VK_PRIOR: selectVisible(std::max(0, pos - 8)); break;
		case VK_NEXT: selectVisible(pos < 0 ? 0 : pos + 8); break;
		case VK_RETURN:
		case VK_F2:
			if (this->SelectedIndex >= 0 && !openSelectedDropDown()) BeginEdit(this->SelectedIndex);
			break;
		case VK_SPACE:
			if (this->SelectedIndex >= 0)
			{
				if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::Bool)
					ToggleBool(this->SelectedIndex);
				else if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::Enum)
					openSelectedDropDown();
				else if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::Color)
					openSelectedDropDown();
			}
			break;
		default:
			break;
		}
		KeyEventArgs e((Keys)(wParam | 0));
		this->OnKeyDown(this, e);
		return true;
	}
	case WM_CHAR:
	{
		if (!_editing && wParam >= 32 && this->SelectedIndex >= 0 && this->SelectedIndex < (int)this->Items.size())
		{
			auto type = this->Items[this->SelectedIndex].ValueType;
			if (IsEditableItem(this->SelectedIndex) &&
				(type == PropertyGridValueType::Text || type == PropertyGridValueType::Number))
			{
				BeginEdit(this->SelectedIndex);
				_editSelectionStart = _editSelectionEnd = 0;
			}
		}
		if (_editing && wParam >= 32)
		{
			bool suppressImeEcho = false;
			wchar_t ch = (wchar_t)wParam;
			if (!_imeCommittedTextToSuppress.empty())
			{
				UINT64 now = GetTickCount64();
				if (_imeCommitSuppressTick == 0 || now - _imeCommitSuppressTick > 1000)
				{
					_imeCommittedTextToSuppress.clear();
					_imeCommitSuppressTick = 0;
				}
				else if (_imeCommittedTextToSuppress.front() == ch)
				{
					suppressImeEcho = true;
					_imeCommittedTextToSuppress.erase(_imeCommittedTextToSuppress.begin());
					if (_imeCommittedTextToSuppress.empty())
						_imeCommitSuppressTick = 0;
				}
				else
				{
					_imeCommittedTextToSuppress.clear();
					_imeCommitSuppressTick = 0;
				}
			}
			if (!suppressImeEcho)
				InsertEditChar(ch);
		}
		else if (_editing && wParam == 1)
		{
			_imeCommittedTextToSuppress.clear();
			_imeCommitSuppressTick = 0;
			_editSelectionStart = 0;
			_editSelectionEnd = (int)_editingText.size();
			InvalidateVisual();
		}
		else if (_editing && wParam == 8)
		{
			_imeCommittedTextToSuppress.clear();
			_imeCommitSuppressTick = 0;
			BackspaceEdit();
		}
		else if (_editing && wParam == 22)
		{
			_imeCommittedTextToSuppress.clear();
			_imeCommitSuppressTick = 0;
			if (OpenClipboard(this->ParentForm->Handle))
			{
				if (IsClipboardFormatAvailable(CF_UNICODETEXT))
				{
					HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
					if (hClip)
					{
						const wchar_t* pBuf = (const wchar_t*)GlobalLock(hClip);
						if (pBuf)
						{
							for (wchar_t ch : std::wstring(pBuf))
								InsertEditChar(ch);
							GlobalUnlock(hClip);
						}
					}
				}
				CloseClipboard();
			}
		}
		else if (_editing && (wParam == 3 || wParam == 24))
		{
			_imeCommittedTextToSuppress.clear();
			_imeCommitSuppressTick = 0;
			std::wstring s = EditGetSelectedString();
			if (!s.empty() && OpenClipboard(this->ParentForm->Handle))
			{
				EmptyClipboard();
				size_t bytes = (s.size() + 1) * sizeof(wchar_t);
				HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
				if (hData)
				{
					wchar_t* pData = (wchar_t*)GlobalLock(hData);
					if (pData)
					{
						memcpy(pData, s.c_str(), bytes);
						GlobalUnlock(hData);
						SetClipboardData(CF_UNICODETEXT, hData);
					}
				}
				CloseClipboard();
			}
			if (wParam == 24)
				BackspaceEdit();
		}
		this->OnCharInput(this, (wchar_t)wParam);
		return true;
	}
	case WM_IME_COMPOSITION:
	{
		HandleImeComposition(lParam);
		return true;
	}
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}
