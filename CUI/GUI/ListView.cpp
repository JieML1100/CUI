#define NOMINMAX
#include "ListView.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <utility>

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

	static bool ColorEquals(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b)
	{
		return std::fabs(a.r - b.r) < 1e-6f &&
			std::fabs(a.g - b.g) < 1e-6f &&
			std::fabs(a.b - b.b) < 1e-6f &&
			std::fabs(a.a - b.a) < 1e-6f;
	}

	static D2D1_COLOR_F FadeColor(D2D1_COLOR_F c, float alphaScale)
	{
		c.a *= alphaScale;
		return c;
	}

	static float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + std::max(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	static float AlignTextX(Font* font, const std::wstring& text, const D2D1_RECT_F& rect, ListViewCellAlign align, float pad)
	{
		if (align == ListViewCellAlign::Left || !font)
			return rect.left + pad;
		auto textSize = font->GetTextSize(text);
		if (align == ListViewCellAlign::Center)
			return rect.left + std::max(0.0f, (RectWidth(rect) - textSize.width) * 0.5f);
		return rect.right - pad - textSize.width;
	}
}

ListViewColumn::ListViewColumn(std::wstring header, float width, ListViewCellAlign align)
	: Header(std::move(header)), Width(width), Align(align)
{
}

ListViewItem::ListViewItem(std::wstring text)
	: Text(std::move(text))
{
}

ListViewItem::ListViewItem(std::wstring text, std::wstring subText)
	: Text(std::move(text)), SubText(std::move(subText))
{
}

ID2D1Bitmap* ListViewItem::GetImageBitmap(D2DGraphics* render)
{
	if (!render || !Image)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (ImageCache && ImageCacheTarget == target && ImageCacheSource == Image.get())
		return ImageCache.Get();
	ImageCache.Reset();
	ImageCacheTarget = target;
	ImageCacheSource = Image.get();
	auto* bmp = render->CreateBitmap(Image);
	if (!bmp)
		return nullptr;
	ImageCache.Attach(bmp);
	return ImageCache.Get();
}

UIClass ListView::Type()
{
	return UIClass::UI_ListView;
}

ListView::ListView(int x, int y, int width, int height)
{
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.0f };
	this->BorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 0.72f };
}

void ListView::Clear()
{
	ClearItems();
	ClearColumns();
}

void ListView::ClearItems()
{
	this->Items.clear();
	this->SelectedIndex = -1;
	this->FocusedIndex = -1;
	this->HoveredIndex = -1;
	this->_anchorIndex = -1;
	this->ScrollYOffset = 0.0f;
	this->SelectionChanged(this);
	this->InvalidateVisual();
}

void ListView::ClearColumns()
{
	this->Columns.clear();
	this->InvalidateVisual();
}

int ListView::AddItem(const ListViewItem& item)
{
	this->Items.push_back(item);
	this->InvalidateVisual();
	return (int)this->Items.size() - 1;
}

void ListView::AddColumn(const ListViewColumn& column)
{
	this->Columns.push_back(column);
	this->InvalidateVisual();
}

bool ListView::RemoveItemAt(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	this->Items.erase(this->Items.begin() + index);
	if (this->SelectedIndex == index) this->SelectedIndex = -1;
	else if (this->SelectedIndex > index) this->SelectedIndex--;
	if (this->FocusedIndex == index) this->FocusedIndex = this->SelectedIndex;
	else if (this->FocusedIndex > index) this->FocusedIndex--;
	if (_anchorIndex == index) _anchorIndex = this->SelectedIndex;
	else if (_anchorIndex > index) _anchorIndex--;
	SyncSelectedIndexFromItems();
	this->SelectionChanged(this);
	this->InvalidateVisual();
	return true;
}

bool ListView::SwapItems(int indexA, int indexB)
{
	if (indexA < 0 || indexB < 0 || indexA >= (int)this->Items.size() || indexB >= (int)this->Items.size()) return false;
	if (indexA == indexB) return true;
	std::swap(this->Items[indexA], this->Items[indexB]);
	SyncSelectedIndexFromItems();
	this->InvalidateVisual();
	return true;
}

size_t ListView::ItemCount() const
{
	return this->Items.size();
}

size_t ListView::ColumnCount() const
{
	return this->Columns.size();
}

ListViewItem* ListView::SelectedItem()
{
	return (this->SelectedIndex >= 0 && this->SelectedIndex < (int)this->Items.size()) ? &this->Items[this->SelectedIndex] : nullptr;
}

const ListViewItem* ListView::SelectedItem() const
{
	return (this->SelectedIndex >= 0 && this->SelectedIndex < (int)this->Items.size()) ? &this->Items[this->SelectedIndex] : nullptr;
}

bool ListView::SelectItem(int index, bool additive, bool range)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	if (!this->Items[index].Enabled) return false;

	bool changed = false;
	if (this->SelectionMode == ListViewSelectionMode::Single || (!additive && !range))
	{
		for (int i = 0; i < (int)this->Items.size(); i++)
		{
			bool selected = (i == index);
			if (this->Items[i].Selected != selected)
			{
				this->Items[i].Selected = selected;
				changed = true;
			}
		}
		_anchorIndex = index;
	}
	else if (range)
	{
		int anchor = _anchorIndex >= 0 ? _anchorIndex : (this->SelectedIndex >= 0 ? this->SelectedIndex : index);
		int first = std::min(anchor, index);
		int last = std::max(anchor, index);
		for (int i = 0; i < (int)this->Items.size(); i++)
		{
			bool selected = i >= first && i <= last && this->Items[i].Enabled;
			if (this->Items[i].Selected != selected)
			{
				this->Items[i].Selected = selected;
				changed = true;
			}
		}
	}
	else
	{
		this->Items[index].Selected = !this->Items[index].Selected;
		changed = true;
		_anchorIndex = index;
	}

	int newSelectedIndex = index;
	if (this->SelectionMode == ListViewSelectionMode::Multiple && additive && !range && !this->Items[index].Selected)
	{
		newSelectedIndex = -1;
		for (int i = 0; i < (int)this->Items.size(); i++)
		{
			if (this->Items[i].Selected)
			{
				newSelectedIndex = i;
				break;
			}
		}
	}

	if (changed || this->SelectedIndex != newSelectedIndex || this->FocusedIndex != index)
	{
		this->SelectedIndex = newSelectedIndex;
		this->FocusedIndex = index;
		EnsureVisible(index);
		this->SelectionChanged(this);
		this->InvalidateVisual();
		return true;
	}

	return false;
}

void ListView::ClearSelection()
{
	bool changed = false;
	for (auto& item : this->Items)
	{
		if (item.Selected)
		{
			item.Selected = false;
			changed = true;
		}
	}
	this->SelectedIndex = -1;
	this->FocusedIndex = -1;
	this->_anchorIndex = -1;
	if (changed)
	{
		this->SelectionChanged(this);
		this->InvalidateVisual();
	}
}

std::vector<int> ListView::GetSelectedIndices() const
{
	std::vector<int> indices;
	for (int i = 0; i < (int)this->Items.size(); i++)
	{
		if (this->Items[i].Selected)
			indices.push_back(i);
	}
	return indices;
}

void ListView::EnsureVisible(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	auto layout = CalcLayout();
	ClampScroll(layout);
	auto rect = GetItemRect(index, layout);
	if (rect.top < layout.ContentRect.top)
	{
		SetScrollOffset(this->ScrollYOffset - (layout.ContentRect.top - rect.top));
	}
	else if (rect.bottom > layout.ContentRect.bottom)
	{
		SetScrollOffset(this->ScrollYOffset + (rect.bottom - layout.ContentRect.bottom));
	}
}

void ListView::SetScrollOffset(float offsetY)
{
	auto layout = CalcLayout();
	float old = this->ScrollYOffset;
	this->ScrollYOffset = std::clamp(offsetY, 0.0f, layout.MaxScrollY);
	if (std::fabs(old - this->ScrollYOffset) > 0.5f)
	{
		this->ScrollChanged(this);
		this->InvalidateVisual();
	}
}

int ListView::HitTestItem(int localX, int localY) const
{
	auto layout = CalcLayout();
	if (!PtInRectF(layout.ContentRect, (float)localX, (float)localY)) return -1;

	if (this->ViewMode == ListViewViewMode::Icon)
	{
		for (int i = 0; i < (int)this->Items.size(); i++)
		{
			auto rect = GetItemRect(i, layout);
			if (PtInRectF(rect, (float)localX, (float)localY))
				return i;
		}
		return -1;
	}

	const float itemH = GetItemSecondaryExtent();
	if (itemH <= 0.0f) return -1;
	int index = (int)std::floor(((float)localY - layout.ContentRect.top + this->ScrollYOffset) / itemH);
	return index >= 0 && index < (int)this->Items.size() ? index : -1;
}

CursorKind ListView::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->Enable) return CursorKind::Arrow;
	auto layout = CalcLayout();
	if (layout.NeedVScroll && localX >= (int)layout.ScrollTrackRect.left && localX <= (int)layout.ScrollTrackRect.right)
		return CursorKind::SizeNS;
	int index = HitTestItem(localX, localY);
	if (index >= 0)
		return CursorKind::Hand;
	return CursorKind::Arrow;
}

bool ListView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	auto layout = CalcLayout();
	if (!layout.NeedVScroll || layout.MaxScrollY <= 0.0f) return false;
	return delta > 0 ? this->ScrollYOffset > 0.0f : this->ScrollYOffset < layout.MaxScrollY;
}

bool ListView::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_SPACE:
		return true;
	default:
		return false;
	}
}

ListView::Layout ListView::CalcLayout() const
{
	Layout layout{};
	const float width = (float)this->_size.cx;
	const float height = (float)this->_size.cy;
	const bool details = this->ViewMode == ListViewViewMode::Details && !IsListBox();
	const float headerH = (details && this->ShowColumnHeaders) ? std::max(0.0f, this->HeaderHeight) : 0.0f;
	layout.HeaderRect = D2D1::RectF(0.0f, 0.0f, width, std::min(height, headerH));
	layout.ContentRect = D2D1::RectF(0.0f, headerH, width, height);
	layout.ScrollBarSize = std::max(6.0f, this->ScrollBarSize);

	const float availableWidth = std::max(0.0f, width - Border * 2.0f);
	if (this->ViewMode == ListViewViewMode::Icon && !IsListBox())
	{
		layout.ColumnsPerRow = std::max(1, (int)std::floor(std::max(1.0f, availableWidth) / std::max(1.0f, this->IconItemWidth + this->ItemGap)));
		int rows = this->Items.empty() ? 0 : (int)std::ceil((float)this->Items.size() / (float)layout.ColumnsPerRow);
		layout.ContentHeight = (float)rows * GetItemSecondaryExtent();
		layout.ContentWidth = (float)layout.ColumnsPerRow * GetItemPrimaryExtent();
	}
	else
	{
		layout.ColumnsPerRow = 1;
		layout.ContentHeight = (float)this->Items.size() * GetItemSecondaryExtent();
		layout.ContentWidth = width;
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
		float thumbTop = layout.ScrollTrackRect.top;
		if (layout.MaxScrollY > 0.0f && trackH > thumbH)
			thumbTop += (this->ScrollYOffset / layout.MaxScrollY) * (trackH - thumbH);
		layout.ScrollThumbRect = D2D1::RectF(layout.ScrollTrackRect.left, thumbTop, layout.ScrollTrackRect.right, thumbTop + thumbH);
	}
	else
	{
		layout.MaxScrollY = 0.0f;
	}

	return layout;
}

float ListView::GetEffectiveRowHeight() const
{
	const float fontHeight = this->_font ? this->_font->FontHeight : 16.0f;
	return std::max(this->RowHeight, fontHeight + 10.0f);
}

float ListView::GetItemPrimaryExtent() const
{
	if (this->ViewMode == ListViewViewMode::Icon && !IsListBox())
		return std::max(48.0f, this->IconItemWidth);
	return (float)this->_size.cx;
}

float ListView::GetItemSecondaryExtent() const
{
	if (IsListBox()) return GetEffectiveRowHeight();
	switch (this->ViewMode)
	{
	case ListViewViewMode::Tile:
		return std::max(this->TileHeight, this->IconSize + 16.0f);
	case ListViewViewMode::Icon:
		return std::max(this->IconItemHeight, this->IconSize + 34.0f);
	case ListViewViewMode::Details:
	case ListViewViewMode::List:
	default:
		return GetEffectiveRowHeight();
	}
}

D2D1_RECT_F ListView::GetItemRect(int index, const Layout& layout) const
{
	if (index < 0 || index >= (int)this->Items.size()) return D2D1::RectF();
	if (this->ViewMode == ListViewViewMode::Icon && !IsListBox())
	{
		const int col = index % layout.ColumnsPerRow;
		const int row = index / layout.ColumnsPerRow;
		const float itemW = GetItemPrimaryExtent();
		const float itemH = GetItemSecondaryExtent();
		const float left = layout.ContentRect.left + (float)col * itemW + this->ItemGap * 0.5f;
		const float top = layout.ContentRect.top + (float)row * itemH - this->ScrollYOffset + this->ItemGap * 0.5f;
		return D2D1::RectF(left, top, left + itemW - this->ItemGap, top + itemH - this->ItemGap);
	}

	const float itemH = GetItemSecondaryExtent();
	const float top = layout.ContentRect.top + (float)index * itemH - this->ScrollYOffset;
	return D2D1::RectF(layout.ContentRect.left, top, layout.ContentRect.right, top + itemH);
}

D2D1_RECT_F ListView::GetCheckRect(const D2D1_RECT_F& itemRect) const
{
	const float size = std::max(10.0f, this->CheckBoxSize);
	const float x = itemRect.left + this->ItemPaddingX;
	const float y = itemRect.top + std::max(0.0f, (RectHeight(itemRect) - size) * 0.5f);
	return D2D1::RectF(x, y, x + size, y + size);
}

void ListView::ClampScroll(Layout& layout)
{
	float old = this->ScrollYOffset;
	this->ScrollYOffset = std::clamp(this->ScrollYOffset, 0.0f, layout.MaxScrollY);
	if (std::fabs(old - this->ScrollYOffset) > 0.5f)
		this->ScrollChanged(this);
	layout = CalcLayout();
}

void ListView::DrawHeader(D2DGraphics* d2d, const Layout& layout)
{
	if (!d2d || RectHeight(layout.HeaderRect) <= 0.0f) return;
	d2d->FillRect(layout.HeaderRect, this->HeaderBackColor);
	const float pad = std::max(4.0f, this->ItemPaddingX);
	float x = layout.HeaderRect.left;
	for (int i = 0; i < (int)this->Columns.size(); i++)
	{
		auto& col = this->Columns[i];
		float w = std::max(16.0f, col.Width);
		D2D1_RECT_F rect = D2D1::RectF(x, layout.HeaderRect.top, std::min(x + w, layout.HeaderRect.right), layout.HeaderRect.bottom);
		if (rect.right <= rect.left) break;
		d2d->PushDrawRect(rect.left, rect.top, RectWidth(rect), RectHeight(rect));
		d2d->DrawString(col.Header, AlignTextX(this->Font, col.Header, rect, col.Align, pad), TextTop(this->Font, rect), this->HeaderForeColor, this->Font);
		d2d->PopDrawRect();
		d2d->DrawLine(rect.right - 0.5f, rect.top + 5.0f, rect.right - 0.5f, rect.bottom - 5.0f, this->GridLineColor, 1.0f);
		x += w;
	}
	if (this->Columns.empty())
		d2d->DrawString(this->Text, layout.HeaderRect.left + pad, TextTop(this->Font, layout.HeaderRect), this->HeaderForeColor, this->Font);
	d2d->DrawLine(layout.HeaderRect.left, layout.HeaderRect.bottom - 0.5f, layout.HeaderRect.right, layout.HeaderRect.bottom - 0.5f, this->GridLineColor, 1.0f);
}

void ListView::DrawItems(D2DGraphics* d2d, const Layout& layout)
{
	if (!d2d || RectWidth(layout.ContentRect) <= 0.0f || RectHeight(layout.ContentRect) <= 0.0f) return;

	d2d->PushDrawRect(layout.ContentRect.left, layout.ContentRect.top, RectWidth(layout.ContentRect), RectHeight(layout.ContentRect));
	for (int i = 0; i < (int)this->Items.size(); i++)
	{
		auto rect = GetItemRect(i, layout);
		if (rect.bottom < layout.ContentRect.top || rect.top > layout.ContentRect.bottom)
			continue;
		switch (IsListBox() ? ListViewViewMode::List : this->ViewMode)
		{
		case ListViewViewMode::Details:
			DrawDetailsItem(d2d, i, rect);
			break;
		case ListViewViewMode::Tile:
			DrawTileItem(d2d, i, rect);
			break;
		case ListViewViewMode::Icon:
			DrawIconItem(d2d, i, rect);
			break;
		case ListViewViewMode::List:
		default:
			DrawListItem(d2d, i, rect);
			break;
		}
	}
	d2d->PopDrawRect();
}

void ListView::DrawListItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect)
{
	if (!d2d || index < 0 || index >= (int)this->Items.size()) return;
	auto& item = this->Items[index];
	D2D1_RECT_F itemRect = D2D1::RectF(rect.left + 3.0f, rect.top + this->ItemPaddingY, rect.right - 3.0f, rect.bottom - this->ItemPaddingY);
	if (item.Selected)
	{
		d2d->FillRoundRect(itemRect, this->SelectedItemBackColor, this->CornerRadius);
		d2d->FillRoundRect(itemRect.left + 4.0f, itemRect.top + 5.0f, this->SelectedAccentWidth, std::max(6.0f, RectHeight(itemRect) - 10.0f), this->AccentColor, this->SelectedAccentWidth * 0.5f);
	}
	else if (index == this->HoveredIndex)
	{
		d2d->FillRoundRect(itemRect, this->UnderMouseItemBackColor, this->CornerRadius);
	}
	else if (this->AlternatingRows && (index % 2) == 1)
	{
		d2d->FillRect(rect, this->AlternateItemBackColor);
	}

	float x = rect.left + this->ItemPaddingX;
	if (this->ShowCheckBoxes)
	{
		auto checkRect = GetCheckRect(rect);
		DrawCheckBox(d2d, checkRect, item.Checked, item.Enabled);
		x = checkRect.right + this->ItemPaddingX;
	}
	if (auto* bmp = item.GetImageBitmap(d2d))
	{
		float imageSize = std::min(this->IconSize, std::max(12.0f, RectHeight(rect) - 8.0f));
		float imageY = rect.top + (RectHeight(rect) - imageSize) * 0.5f;
		d2d->DrawBitmap(bmp, x, imageY, imageSize, imageSize);
		x += imageSize + this->ItemPaddingX;
	}

	D2D1_COLOR_F color = item.Enabled ? (item.Selected ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
	D2D1_RECT_F textRect = D2D1::RectF(x, rect.top, rect.right - this->ItemPaddingX, rect.bottom);
	d2d->PushDrawRect(textRect.left, textRect.top, std::max(1.0f, RectWidth(textRect)), RectHeight(textRect));
	d2d->DrawString(item.Text, textRect.left, TextTop(this->Font, textRect), color, this->Font);
	d2d->PopDrawRect();
}

void ListView::DrawDetailsItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect)
{
	if (!d2d || index < 0 || index >= (int)this->Items.size()) return;
	auto& item = this->Items[index];
	D2D1_RECT_F itemRect = D2D1::RectF(rect.left + 3.0f, rect.top + this->ItemPaddingY, rect.right - 3.0f, rect.bottom - this->ItemPaddingY);
	if (item.Selected)
		d2d->FillRoundRect(itemRect, this->SelectedItemBackColor, this->CornerRadius);
	else if (index == this->HoveredIndex)
		d2d->FillRoundRect(itemRect, this->UnderMouseItemBackColor, this->CornerRadius);
	else if (this->AlternatingRows && (index % 2) == 1)
		d2d->FillRect(rect, this->AlternateItemBackColor);

	float x = rect.left;
	const float pad = std::max(4.0f, this->ItemPaddingX);
	int colCount = std::max(1, (int)this->Columns.size());
	for (int col = 0; col < colCount; col++)
	{
		float w = this->Columns.empty() ? RectWidth(rect) : std::max(16.0f, this->Columns[col].Width);
		D2D1_RECT_F cell = D2D1::RectF(x, rect.top, std::min(x + w, rect.right), rect.bottom);
		if (cell.right <= cell.left) break;

		std::wstring text;
		ListViewCellAlign align = ListViewCellAlign::Left;
		if (col == 0)
			text = item.Text;
		else if (col - 1 < (int)item.SubItems.size())
			text = item.SubItems[col - 1];
		else if (col == 1)
			text = item.SubText;
		if (!this->Columns.empty())
			align = this->Columns[col].Align;

		float textLeft = cell.left + pad;
		if (col == 0)
		{
			if (this->ShowCheckBoxes)
			{
				auto checkRect = GetCheckRect(cell);
				DrawCheckBox(d2d, checkRect, item.Checked, item.Enabled);
				textLeft = checkRect.right + pad;
			}
			if (auto* bmp = item.GetImageBitmap(d2d))
			{
				float imageSize = std::min(this->IconSize, std::max(12.0f, RectHeight(rect) - 8.0f));
				float imageY = rect.top + (RectHeight(rect) - imageSize) * 0.5f;
				d2d->DrawBitmap(bmp, textLeft, imageY, imageSize, imageSize);
				textLeft += imageSize + pad;
			}
		}
		else
		{
			textLeft = AlignTextX(this->Font, text, cell, align, pad);
		}

		D2D1_COLOR_F color = item.Enabled ? (item.Selected ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
		d2d->PushDrawRect(cell.left + 1.0f, cell.top, std::max(1.0f, RectWidth(cell) - 2.0f), RectHeight(cell));
		d2d->DrawString(text, textLeft, TextTop(this->Font, cell), color, this->Font);
		d2d->PopDrawRect();
		d2d->DrawLine(cell.right - 0.5f, cell.top + 5.0f, cell.right - 0.5f, cell.bottom - 5.0f, FadeColor(this->GridLineColor, 0.75f), 1.0f);
		x += w;
	}
	d2d->DrawLine(rect.left, rect.bottom - 0.5f, rect.right, rect.bottom - 0.5f, FadeColor(this->GridLineColor, 0.75f), 1.0f);
}

void ListView::DrawTileItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect)
{
	if (!d2d || index < 0 || index >= (int)this->Items.size()) return;
	auto& item = this->Items[index];
	D2D1_RECT_F itemRect = D2D1::RectF(rect.left + 4.0f, rect.top + 4.0f, rect.right - 4.0f, rect.bottom - 4.0f);
	if (item.Selected)
		d2d->FillRoundRect(itemRect, this->SelectedItemBackColor, this->CornerRadius);
	else if (index == this->HoveredIndex)
		d2d->FillRoundRect(itemRect, this->UnderMouseItemBackColor, this->CornerRadius);
	else if (this->AlternatingRows && (index % 2) == 1)
		d2d->FillRoundRect(itemRect, this->AlternateItemBackColor, this->CornerRadius);

	float x = itemRect.left + this->ItemPaddingX;
	if (this->ShowCheckBoxes)
	{
		auto checkRect = GetCheckRect(itemRect);
		DrawCheckBox(d2d, checkRect, item.Checked, item.Enabled);
		x = checkRect.right + this->ItemPaddingX;
	}
	const float imageSize = std::min(this->IconSize, RectHeight(itemRect) - 12.0f);
	if (auto* bmp = item.GetImageBitmap(d2d))
	{
		d2d->DrawBitmap(bmp, x, itemRect.top + (RectHeight(itemRect) - imageSize) * 0.5f, imageSize, imageSize);
	}
	else
	{
		d2d->FillRoundRect(x, itemRect.top + (RectHeight(itemRect) - imageSize) * 0.5f, imageSize, imageSize, FadeColor(this->AccentColor, 0.18f), 6.0f);
	}
	x += imageSize + this->ItemPaddingX;

	D2D1_COLOR_F color = item.Enabled ? (item.Selected ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
	D2D1_RECT_F titleRect = D2D1::RectF(x, itemRect.top + 8.0f, itemRect.right - this->ItemPaddingX, itemRect.top + RectHeight(itemRect) * 0.5f);
	D2D1_RECT_F subRect = D2D1::RectF(x, itemRect.top + RectHeight(itemRect) * 0.5f, itemRect.right - this->ItemPaddingX, itemRect.bottom - 6.0f);
	d2d->PushDrawRect(x, itemRect.top, std::max(1.0f, itemRect.right - x), RectHeight(itemRect));
	d2d->DrawString(item.Text, titleRect.left, titleRect.top, color, this->Font);
	if (!item.SubText.empty())
		d2d->DrawString(item.SubText, subRect.left, subRect.top, item.Enabled ? this->MutedTextColor : this->DisabledItemForeColor, this->Font);
	d2d->PopDrawRect();
}

void ListView::DrawIconItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect)
{
	if (!d2d || index < 0 || index >= (int)this->Items.size()) return;
	auto& item = this->Items[index];
	if (item.Selected)
		d2d->FillRoundRect(rect, this->SelectedItemBackColor, this->CornerRadius);
	else if (index == this->HoveredIndex)
		d2d->FillRoundRect(rect, this->UnderMouseItemBackColor, this->CornerRadius);

	const float imageSize = std::min(this->IconSize, RectHeight(rect) - 32.0f);
	const float imageX = rect.left + (RectWidth(rect) - imageSize) * 0.5f;
	const float imageY = rect.top + 8.0f;
	if (auto* bmp = item.GetImageBitmap(d2d))
	{
		d2d->DrawBitmap(bmp, imageX, imageY, imageSize, imageSize);
	}
	else
	{
		d2d->FillRoundRect(imageX, imageY, imageSize, imageSize, FadeColor(this->AccentColor, 0.18f), 7.0f);
	}

	if (this->ShowCheckBoxes)
	{
		D2D1_RECT_F check = D2D1::RectF(rect.left + 6.0f, rect.top + 6.0f, rect.left + 6.0f + this->CheckBoxSize, rect.top + 6.0f + this->CheckBoxSize);
		DrawCheckBox(d2d, check, item.Checked, item.Enabled);
	}

	D2D1_COLOR_F color = item.Enabled ? (item.Selected ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
	D2D1_RECT_F textRect = D2D1::RectF(rect.left + 4.0f, imageY + imageSize + 6.0f, rect.right - 4.0f, rect.bottom - 3.0f);
	d2d->PushDrawRect(textRect.left, textRect.top, std::max(1.0f, RectWidth(textRect)), RectHeight(textRect));
	auto textSize = this->Font ? this->Font->GetTextSize(item.Text) : D2D1_SIZE_F{ 0,0 };
	float textX = textRect.left + std::max(0.0f, (RectWidth(textRect) - textSize.width) * 0.5f);
	d2d->DrawString(item.Text, textX, textRect.top, color, this->Font);
	d2d->PopDrawRect();
}

void ListView::DrawCheckBox(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool checked, bool enabled)
{
	if (!d2d) return;
	D2D1_COLOR_F back = enabled ? this->CheckBackColor : FadeColor(this->CheckBackColor, 0.55f);
	D2D1_COLOR_F border = enabled ? this->CheckBorderColor : FadeColor(this->CheckBorderColor, 0.55f);
	d2d->FillRoundRect(rect, back, 3.0f);
	d2d->DrawRoundRect(rect, checked && enabled ? this->AccentColor : border, 1.2f, 3.0f);
	if (!checked) return;
	D2D1_COLOR_F mark = enabled ? this->AccentColor : FadeColor(this->AccentColor, 0.55f);
	const float w = RectWidth(rect);
	const float h = RectHeight(rect);
	D2D1_POINT_2F p1 = D2D1::Point2F(rect.left + w * 0.24f, rect.top + h * 0.54f);
	D2D1_POINT_2F p2 = D2D1::Point2F(rect.left + w * 0.43f, rect.top + h * 0.72f);
	D2D1_POINT_2F p3 = D2D1::Point2F(rect.left + w * 0.78f, rect.top + h * 0.30f);
	d2d->DrawLine(p1, p2, mark, 1.8f);
	d2d->DrawLine(p2, p3, mark, 1.8f);
}

void ListView::DrawScrollBar(D2DGraphics* d2d, const Layout& layout)
{
	if (!d2d || !layout.NeedVScroll) return;
	d2d->FillRoundRect(layout.ScrollTrackRect, this->ScrollBackColor, RectWidth(layout.ScrollTrackRect) * 0.5f);
	d2d->FillRoundRect(layout.ScrollThumbRect, this->ScrollForeColor, RectWidth(layout.ScrollThumbRect) * 0.5f);
}

void ListView::UpdateHover(int localX, int localY)
{
	int old = this->HoveredIndex;
	this->HoveredIndex = HitTestItem(localX, localY);
	if (old != this->HoveredIndex)
		this->InvalidateVisual();
}

void ListView::UpdateScrollByThumb(float localY)
{
	auto layout = CalcLayout();
	if (!layout.NeedVScroll) return;
	const float trackH = RectHeight(layout.ScrollTrackRect);
	const float thumbH = RectHeight(layout.ScrollThumbRect);
	const float range = trackH - thumbH;
	if (range <= 0.0f || layout.MaxScrollY <= 0.0f) return;
	float targetTop = localY - _scrollThumbGrabOffsetY;
	float t = (targetTop - layout.ScrollTrackRect.top) / range;
	t = std::clamp(t, 0.0f, 1.0f);
	SetScrollOffset(t * layout.MaxScrollY);
}

void ListView::ToggleCheckAt(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	auto& item = this->Items[index];
	if (!item.Enabled) return;
	item.Checked = !item.Checked;
	this->OnItemCheckChanged(this, index, item.Checked);
	this->InvalidateVisual();
}

void ListView::MoveSelectionBy(int delta)
{
	if (this->Items.empty()) return;
	int index = this->FocusedIndex >= 0 ? this->FocusedIndex : this->SelectedIndex;
	if (index < 0) index = delta >= 0 ? 0 : (int)this->Items.size() - 1;
	else index = std::clamp(index + delta, 0, (int)this->Items.size() - 1);
	SelectItem(index, false, false);
}

void ListView::PageSelection(int direction)
{
	auto layout = CalcLayout();
	float itemH = std::max(1.0f, GetItemSecondaryExtent());
	int page = std::max(1, (int)std::floor(RectHeight(layout.ContentRect) / itemH));
	MoveSelectionBy(page * direction);
}

void ListView::SyncSelectedIndexFromItems()
{
	this->SelectedIndex = -1;
	for (int i = 0; i < (int)this->Items.size(); i++)
	{
		if (this->Items[i].Selected)
		{
			this->SelectedIndex = i;
			break;
		}
	}
}

void ListView::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	auto size = this->ActualSize();
	float width = (float)size.cx;
	float height = (float)size.cy;
	auto layout = CalcLayout();
	ClampScroll(layout);

	this->BeginRender();
	{
		d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, std::max(0.0f, width - Border), std::max(0.0f, height - Border), this->BackColor, this->CornerRadius);
		if (this->Image)
			this->RenderImage();
		DrawHeader(d2d, layout);
		DrawItems(d2d, layout);
		DrawScrollBar(d2d, layout);
		if (Border > 0.0f)
			d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, std::max(0.0f, width - Border), std::max(0.0f, height - Border), this->BorderColor, Border, this->CornerRadius);
		if (!this->Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.48f }, this->CornerRadius);
	}
	this->EndRender();
}

bool ListView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (delta != 0)
		{
			const float step = (float)std::max(8, this->MouseWheelStep);
			SetScrollOffset(this->ScrollYOffset + (delta < 0 ? step : -step));
		}
		MouseEventArgs e(MouseButtons::None, 0, localX, localY, delta);
		this->OnMouseWheel(this, e);
		return true;
	}
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm)
			this->ParentForm->UnderMouse = this;
		if (_dragVScroll)
			UpdateScrollByThumb((float)localY);
		else
			UpdateHover(localX, localY);
		MouseEventArgs e(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (this->ParentForm)
			this->ParentForm->SetSelectedControl(this, false);
		auto layout = CalcLayout();
		if (layout.NeedVScroll && PtInRectF(layout.ScrollThumbRect, (float)localX, (float)localY))
		{
			_dragVScroll = true;
			_scrollThumbGrabOffsetY = (float)localY - layout.ScrollThumbRect.top;
			return true;
		}
		if (layout.NeedVScroll && PtInRectF(layout.ScrollTrackRect, (float)localX, (float)localY))
		{
			SetScrollOffset(this->ScrollYOffset + ((float)localY < layout.ScrollThumbRect.top ? -RectHeight(layout.ContentRect) : RectHeight(layout.ContentRect)));
			return true;
		}

		int index = HitTestItem(localX, localY);
		if (index >= 0)
		{
			auto rect = GetItemRect(index, layout);
			if (this->ShowCheckBoxes && PtInRectF(GetCheckRect(rect), (float)localX, (float)localY))
				ToggleCheckAt(index);

			bool isControlKeyDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			SelectItem(index, isControlKeyDown, shift);
		}
		else if (this->SelectionMode == ListViewSelectionMode::Single)
		{
			ClearSelection();
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		bool wasDragging = _dragVScroll;
		_dragVScroll = false;
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, e);
		if (!wasDragging)
		{
			int index = HitTestItem(localX, localY);
			if (index >= 0)
			{
				this->OnItemClick(this, index);
				this->OnMouseClick(this, e);
			}
		}
		return true;
	}
	case WM_LBUTTONDBLCLK:
	{
		int index = HitTestItem(localX, localY);
		MouseEventArgs e(MouseButtons::Left, 2, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, e);
		if (index >= 0)
			this->OnItemDoubleClick(this, index);
		return true;
	}
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_UP: MoveSelectionBy(-1); break;
		case VK_DOWN: MoveSelectionBy(1); break;
		case VK_LEFT:
			if (this->ViewMode == ListViewViewMode::Icon && !IsListBox())
				MoveSelectionBy(-1);
			break;
		case VK_RIGHT:
			if (this->ViewMode == ListViewViewMode::Icon && !IsListBox())
				MoveSelectionBy(1);
			break;
		case VK_HOME:
			if (!this->Items.empty()) SelectItem(0, false, false);
			break;
		case VK_END:
			if (!this->Items.empty()) SelectItem((int)this->Items.size() - 1, false, false);
			break;
		case VK_PRIOR: PageSelection(-1); break;
		case VK_NEXT: PageSelection(1); break;
		case VK_SPACE:
			if (this->ShowCheckBoxes && this->FocusedIndex >= 0)
				ToggleCheckAt(this->FocusedIndex);
			break;
		default:
			break;
		}
		KeyEventArgs e((Keys)(wParam | 0));
		this->OnKeyDown(this, e);
		return true;
	}
	case WM_KEYUP:
	{
		KeyEventArgs e((Keys)(wParam | 0));
		this->OnKeyUp(this, e);
		return true;
	}
	default:
		break;
	}

	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}

UIClass ListBox::Type()
{
	return UIClass::UI_ListBox;
}

ListBox::ListBox(int x, int y, int width, int height)
	: ListView(x, y, width, height)
{
	this->ViewMode = ListViewViewMode::List;
	this->ShowColumnHeaders = false;
	this->ShowCheckBoxes = false;
	this->FullRowSelect = true;
}
