#define NOMINMAX
#include "NavigationView.h"
#include "Form.h"
#include "AdvancedControlPropertyRegistration.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + (std::max)(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	D2D1_COLOR_F FadeColor(D2D1_COLOR_F color, float alphaScale)
	{
		color.a *= alphaScale;
		return color;
	}

	D2D1_COLOR_F BlendColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return D2D1_COLOR_F{
			a.r + (b.r - a.r) * t,
			a.g + (b.g - a.g) * t,
			a.b + (b.b - a.b) * t,
			a.a + (b.a - a.a) * t };
	}

	void DrawHamburger(D2DGraphics* d2d, const D2D1_RECT_F& rect, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		float cx = rect.left + RectWidth(rect) * 0.5f;
		float y = rect.top + RectHeight(rect) * 0.5f - 6.0f;
		float half = 7.0f;
		for (int i = 0; i < 3; ++i)
		{
			d2d->DrawLine(cx - half, y + i * 6.0f, cx + half, y + i * 6.0f, color, 1.7f);
		}
	}

	void DrawBreadcrumbChevron(D2DGraphics* d2d, float cx, float cy, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		const float halfW = 3.4f;
		const float halfH = 5.2f;
		d2d->DrawLine(D2D1::Point2F(cx - halfW, cy - halfH), D2D1::Point2F(cx + halfW, cy), color, 1.6f);
		d2d->DrawLine(D2D1::Point2F(cx + halfW, cy), D2D1::Point2F(cx - halfW, cy + halfH), color, 1.6f);
	}
}

NavigationViewItem::NavigationViewItem(std::wstring text, std::wstring value, std::shared_ptr<BitmapSource> icon)
	: Text(std::move(text)), Value(std::move(value)), Icon(std::move(icon))
{
}

NavigationViewItem NavigationViewItem::Header(std::wstring text)
{
	NavigationViewItem item;
	item.Text = std::move(text);
	item.Kind = NavigationViewItemKind::Header;
	item.Enabled = false;
	return item;
}

NavigationViewItem NavigationViewItem::Separator()
{
	NavigationViewItem item;
	item.Kind = NavigationViewItemKind::Separator;
	item.Enabled = false;
	return item;
}

ID2D1Bitmap* NavigationViewItem::GetIconBitmap(D2DGraphics* render)
{
	if (!render || !Icon)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (IconCache && IconCacheTarget == target && IconCacheSource == Icon.get())
		return IconCache.Get();
	IconCache.Reset();
	IconCacheTarget = target;
	IconCacheSource = Icon.get();
	auto* bmp = render->CreateBitmap(Icon);
	if (!bmp)
		return nullptr;
	IconCache.Attach(bmp);
	return IconCache.Get();
}

UIClass NavigationView::Type()
{
	return UIClass::UI_NavigationView;
}

void NavigationView::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		using namespace cui::advanced_properties;
		RegisterEnumField(L"DisplayMode", &NavigationView::DisplayMode,
			NavigationViewDisplayMode::Expanded, L"Layout", 100, 10,
			{ { L"Expanded", NavigationViewDisplayMode::Expanded },
			  { L"Compact", NavigationViewDisplayMode::Compact } });
		RegisterField(L"IsPaneOpen", &NavigationView::IsPaneOpen, true,
			L"Behavior", 110, 10, ControlPropertyEditorKind::Boolean);
		RegisterField(L"ShowToggleButton", &NavigationView::ShowToggleButton, true,
			L"Behavior", 110, 20, ControlPropertyEditorKind::Boolean);
		RegisterField(L"ShowHeader", &NavigationView::ShowHeader, true,
			L"Behavior", 110, 30, ControlPropertyEditorKind::Boolean);
		RegisterField(L"ShowIconPlaceholder", &NavigationView::ShowIconPlaceholder, true,
			L"Behavior", 110, 40, ControlPropertyEditorKind::Boolean);
		RegisterField(L"AutoSelectOnClick", &NavigationView::AutoSelectOnClick, true,
			L"Behavior", 110, 50, ControlPropertyEditorKind::Boolean);
		RegisterField(L"HeaderText", &NavigationView::HeaderText,
			std::wstring(L"Navigation"), L"Data", 600, 10,
			ControlPropertyEditorKind::Text);
		RegisterField(L"FooterText", &NavigationView::FooterText,
			std::wstring{}, L"Data", 600, 20, ControlPropertyEditorKind::Text);
		RegisterMetric(L"Border", &NavigationView::Border, 1.0f,
			L"Appearance", 200, 10);
		RegisterMetric(L"CornerRadius", &NavigationView::CornerRadius, 8.0f,
			L"Appearance", 200, 20);
		RegisterMetric(L"HeaderHeight", &NavigationView::HeaderHeight, 44.0f,
			L"Layout", 100, 20);
		RegisterMetric(L"ItemHeight", &NavigationView::ItemHeight, 34.0f,
			L"Layout", 100, 30);
		RegisterMetric(L"HeaderItemHeight", &NavigationView::HeaderItemHeight, 24.0f,
			L"Layout", 100, 40);
		RegisterMetric(L"SeparatorHeight", &NavigationView::SeparatorHeight, 10.0f,
			L"Layout", 100, 50);
		RegisterMetric(L"ItemGap", &NavigationView::ItemGap, 4.0f,
			L"Layout", 100, 60);
		RegisterMetric(L"ItemPaddingX", &NavigationView::ItemPaddingX, 8.0f,
			L"Layout", 100, 70);
		RegisterMetric(L"IconSize", &NavigationView::IconSize, 18.0f,
			L"Layout", 100, 80);
		RegisterMetric(L"SelectedAccentWidth", &NavigationView::SelectedAccentWidth, 3.0f,
			L"Layout", 100, 90);
		RegisterMetric(L"ScrollBarSize", &NavigationView::ScrollBarSize, 8.0f,
			L"Layout", 100, 100);
		RegisterIntMetric(L"MouseWheelStep", &NavigationView::MouseWheelStep, 48,
			L"Behavior", 110, 60, 1);
		RegisterColor(L"SurfaceColor", &NavigationView::SurfaceColor,
			cui::theme::palette::Surface, 100);
		RegisterColor(L"HeaderBackColor", &NavigationView::HeaderBackColor,
			cui::theme::palette::SurfaceMuted, 110);
		RegisterColor(L"MutedTextColor", &NavigationView::MutedTextColor,
			cui::theme::palette::TextMuted, 120);
		RegisterColor(L"SelectedItemBackColor", &NavigationView::SelectedItemBackColor,
			cui::theme::palette::AccentSelected, 130);
		RegisterColor(L"SelectedItemForeColor", &NavigationView::SelectedItemForeColor,
			cui::theme::palette::TextPrimary, 140);
		RegisterColor(L"UnderMouseItemBackColor", &NavigationView::UnderMouseItemBackColor,
			cui::theme::palette::AccentSoft, 150);
		RegisterColor(L"AccentColor", &NavigationView::AccentColor,
			cui::theme::palette::Accent, 160);
		RegisterColor(L"IconPlaceholderColor", &NavigationView::IconPlaceholderColor,
			cui::theme::palette::AccentSelected, 170);
		RegisterColor(L"BadgeBackColor", &NavigationView::BadgeBackColor,
			cui::theme::palette::Accent, 180);
		RegisterColor(L"BadgeForeColor", &NavigationView::BadgeForeColor,
			cui::theme::palette::OnAccent, 190);
		RegisterColor(L"SeparatorColor", &NavigationView::SeparatorColor,
			cui::theme::palette::Border, 200);
		RegisterColor(L"ScrollBackColor", &NavigationView::ScrollBackColor,
			cui::theme::palette::ScrollTrack, 210);
		RegisterColor(L"ScrollForeColor", &NavigationView::ScrollForeColor,
			cui::theme::palette::ScrollThumb, 220);

		auto selectedOptions = Options<NavigationView, int>(-1,
			L"Behavior", 110, 70, ControlPropertyEditorKind::Number);
		selectedOptions.Design.Minimum = -1.0;
		BindingPropertyRegistry::Register<NavigationView, int>(L"SelectedIndex",
			[](NavigationView& target) { return target.SelectedIndex; },
			[](NavigationView& target, const int& value)
			{
				if (value < 0) target.ClearSelection();
				else if (target.Items.empty()) target.SelectedIndex = value;
				else target.SelectItem(value);
			},
			[](NavigationView& target,
				BindingPropertyMetadata::ChangeHandler handler, DataSourceUpdateMode)
			{
				return target.SelectionChanged.Subscribe(
					[handler = std::move(handler)](Control*) { handler(); });
			}, std::move(selectedOptions));
		return true;
	}();
	(void)registered;
}

NavigationView::NavigationView(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
}

bool NavigationView::IsCompactMode() const
{
	return !IsPaneOpen || DisplayMode == NavigationViewDisplayMode::Compact;
}

int NavigationView::AddItem(const NavigationViewItem& item)
{
	this->Items.push_back(item);
	SyncSelectedIndexFromItems();
	InvalidateVisual();
	return (int)this->Items.size() - 1;
}

int NavigationView::AddItem(const std::wstring& text, const std::wstring& value, std::shared_ptr<BitmapSource> icon)
{
	return AddItem(NavigationViewItem(text, value, std::move(icon)));
}

int NavigationView::AddHeader(const std::wstring& text)
{
	return AddItem(NavigationViewItem::Header(text));
}

int NavigationView::AddSeparator()
{
	return AddItem(NavigationViewItem::Separator());
}

void NavigationView::ClearItems()
{
	this->Items.clear();
	this->SelectedIndex = -1;
	this->HoveredIndex = -1;
	this->FocusedIndex = -1;
	this->ScrollYOffset = 0.0f;
	this->SelectionChanged(this);
	this->InvalidateVisual();
}

bool NavigationView::RemoveItemAt(int index)
{
	if (index < 0 || index >= (int)this->Items.size())
		return false;
	bool removedSelected = this->Items[index].Selected || this->SelectedIndex == index;
	this->Items.erase(this->Items.begin() + index);
	if (this->HoveredIndex == index) this->HoveredIndex = -1;
	if (this->FocusedIndex == index) this->FocusedIndex = -1;
	if (this->HoveredIndex > index) --this->HoveredIndex;
	if (this->FocusedIndex > index) --this->FocusedIndex;
	SyncSelectedIndexFromItems();
	if (removedSelected)
		this->SelectionChanged(this);
	this->InvalidateVisual();
	return true;
}

NavigationViewItem* NavigationView::SelectedItem()
{
	if (this->SelectedIndex < 0 || this->SelectedIndex >= (int)this->Items.size())
		return nullptr;
	return &this->Items[this->SelectedIndex];
}

const NavigationViewItem* NavigationView::SelectedItem() const
{
	if (this->SelectedIndex < 0 || this->SelectedIndex >= (int)this->Items.size())
		return nullptr;
	return &this->Items[this->SelectedIndex];
}

bool NavigationView::SelectItem(int index)
{
	if (index < 0 || index >= (int)this->Items.size())
		return false;
	auto& item = this->Items[index];
	if (item.Kind != NavigationViewItemKind::Item || !item.Enabled)
		return false;

	bool changed = this->SelectedIndex != index || !item.Selected;
	for (auto& it : this->Items)
		it.Selected = false;
	item.Selected = true;
	this->SelectedIndex = index;
	this->FocusedIndex = index;
	if (changed)
		this->SelectionChanged(this);
	this->InvalidateVisual();
	return true;
}

void NavigationView::ClearSelection()
{
	bool changed = this->SelectedIndex >= 0;
	for (auto& item : this->Items)
		item.Selected = false;
	this->SelectedIndex = -1;
	this->FocusedIndex = -1;
	if (changed)
		this->SelectionChanged(this);
	this->InvalidateVisual();
}

void NavigationView::SetPaneOpen(bool value)
{
	if (this->IsPaneOpen == value)
		return;
	this->IsPaneOpen = value;
	this->InvalidateVisual();
}

void NavigationView::TogglePane()
{
	SetPaneOpen(!this->IsPaneOpen);
}

float NavigationView::GetRowHeight(const NavigationViewItem& item) const
{
	if (item.Kind == NavigationViewItemKind::Header)
		return IsCompactMode() ? 0.0f : (std::max)(16.0f, this->HeaderItemHeight);
	if (item.Kind == NavigationViewItemKind::Separator)
		return (std::max)(4.0f, this->SeparatorHeight);
	class Font* font = this->_font ? this->_font : GetDefaultFontObject();
	const float fontHeight = font ? font->FontHeight : 16.0f;
	return (std::max)(this->ItemHeight, fontHeight + 10.0f);
}

NavigationView::Layout NavigationView::CalcLayout(std::vector<RowInfo>* rows) const
{
	Layout layout{};
	auto size = this->_size;
	const float width = (float)size.cx;
	const float height = (float)size.cy;
	const float border = (std::max)(0.0f, this->Border);
	const float pad = (std::max)(0.0f, this->ItemPaddingX);
	float top = border;

	const bool showTop = this->ShowToggleButton || (this->ShowHeader && !this->HeaderText.empty() && !IsCompactMode());
	if (showTop)
	{
		layout.HeaderRect = D2D1::RectF(border, border, (std::max)(border, width - border), (std::min)(height - border, border + this->HeaderHeight));
		layout.ToggleRect = D2D1::RectF(pad, layout.HeaderRect.top + 6.0f, pad + 32.0f, layout.HeaderRect.bottom - 6.0f);
		top = layout.HeaderRect.bottom;
	}

	float bottom = height - border;
	if (!this->FooterText.empty() && !IsCompactMode())
	{
		const float footerH = (std::max)(28.0f, this->ItemHeight);
		layout.FooterRect = D2D1::RectF(border, (std::max)(top, bottom - footerH), width - border, bottom);
		bottom = layout.FooterRect.top;
	}

	layout.ContentRect = D2D1::RectF(border, top, (std::max)(border, width - border), (std::max)(top, bottom));

	float cursor = 0.0f;
	if (rows)
		rows->clear();
	for (int i = 0; i < (int)this->Items.size(); ++i)
	{
		const float h = GetRowHeight(this->Items[i]);
		if (h <= 0.001f)
			continue;
		if (rows)
			rows->push_back(RowInfo{ i, cursor, h });
		cursor += h + (this->Items[i].Kind == NavigationViewItemKind::Item ? (std::max)(0.0f, this->ItemGap) : 0.0f);
	}
	layout.ContentHeight = cursor;
	layout.MaxScrollY = (std::max)(0.0f, layout.ContentHeight - RectHeight(layout.ContentRect));
	layout.NeedVScroll = layout.MaxScrollY > 0.5f;
	if (layout.NeedVScroll)
	{
		const float bar = (std::max)(4.0f, this->ScrollBarSize);
		layout.ScrollTrackRect = D2D1::RectF((std::max)(layout.ContentRect.left, layout.ContentRect.right - bar - 2.0f),
			layout.ContentRect.top + 4.0f, layout.ContentRect.right - 2.0f, layout.ContentRect.bottom - 4.0f);
		const float trackH = (std::max)(1.0f, RectHeight(layout.ScrollTrackRect));
		const float thumbH = (std::clamp)(trackH * RectHeight(layout.ContentRect) / (std::max)(1.0f, layout.ContentHeight), 24.0f, trackH);
		const float range = (std::max)(0.0f, trackH - thumbH);
		const float t = layout.MaxScrollY > 0.0f ? (std::clamp)(this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
		layout.ScrollThumbRect = D2D1::RectF(layout.ScrollTrackRect.left, layout.ScrollTrackRect.top + range * t,
			layout.ScrollTrackRect.right, layout.ScrollTrackRect.top + range * t + thumbH);
		layout.ContentRect.right -= bar + 4.0f;
	}
	return layout;
}

D2D1_RECT_F NavigationView::GetRowRect(const RowInfo& row, const Layout& layout) const
{
	const float left = layout.ContentRect.left + (std::max)(0.0f, this->ItemPaddingX);
	const float right = layout.ContentRect.right - (std::max)(0.0f, this->ItemPaddingX);
	const float top = layout.ContentRect.top + row.Top - this->ScrollYOffset;
	return D2D1::RectF(left, top, (std::max)(left, right), top + row.Height);
}

void NavigationView::ClampScroll(Layout& layout)
{
	float clamped = (std::clamp)(this->ScrollYOffset, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 0.01f)
		const_cast<NavigationView*>(this)->ScrollYOffset = clamped;
}

void NavigationView::SetScrollOffset(float offsetY)
{
	auto layout = CalcLayout();
	float next = (std::clamp)(offsetY, 0.0f, layout.MaxScrollY);
	if (std::fabs(next - this->ScrollYOffset) < 0.01f)
		return;
	this->ScrollYOffset = next;
	this->ScrollChanged(this);
	this->InvalidateVisual();
}

bool NavigationView::HitTestToggle(const Layout& layout, int localX, int localY) const
{
	if (!this->ShowToggleButton)
		return false;
	return PtInRectF(layout.ToggleRect, (float)localX, (float)localY);
}

int NavigationView::HitTestItem(int localX, int localY) const
{
	std::vector<RowInfo> rows;
	auto layout = CalcLayout(&rows);
	if (!PtInRectF(layout.ContentRect, (float)localX, (float)localY))
		return -1;
	for (const auto& row : rows)
	{
		if (row.Index < 0 || row.Index >= (int)this->Items.size())
			continue;
		const auto& item = this->Items[row.Index];
		if (item.Kind != NavigationViewItemKind::Item || !item.Enabled)
			continue;
		auto rect = GetRowRect(row, layout);
		if (PtInRectF(rect, (float)localX, (float)localY))
			return row.Index;
	}
	return -1;
}

CursorKind NavigationView::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;
	std::vector<RowInfo> rows;
	auto layout = CalcLayout(&rows);
	if (HitTestToggle(layout, localX, localY))
		return CursorKind::Hand;
	if (layout.NeedVScroll && PtInRectF(layout.ScrollTrackRect, (float)localX, (float)localY))
		return CursorKind::SizeNS;
	return HitTestItem(localX, localY) >= 0 ? CursorKind::Hand : CursorKind::Arrow;
}

bool NavigationView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	auto layout = CalcLayout();
	if (!layout.NeedVScroll) return false;
	return delta > 0 ? this->ScrollYOffset > 0.0f : this->ScrollYOffset < layout.MaxScrollY;
}

bool NavigationView::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END || key == VK_RETURN || key == VK_SPACE;
}

void NavigationView::DrawHeader(D2DGraphics* d2d, const Layout& layout)
{
	if (!d2d || RectHeight(layout.HeaderRect) <= 0.0f)
		return;
	if (HeaderBackColor.a > 0.0f)
		d2d->FillRoundRect(layout.HeaderRect, HeaderBackColor, (std::min)(CornerRadius, 8.0f));
	if (ShowToggleButton)
	{
		d2d->FillRoundRect(layout.ToggleRect, UnderMouseItemBackColor, 6.0f);
		DrawHamburger(d2d, layout.ToggleRect, ForeColor);
	}
	if (!IsCompactMode() && ShowHeader && !HeaderText.empty())
	{
		float left = ShowToggleButton ? layout.ToggleRect.right + 8.0f : layout.HeaderRect.left + 12.0f;
		D2D1_RECT_F textRect{ left, layout.HeaderRect.top, layout.HeaderRect.right - 10.0f, layout.HeaderRect.bottom };
		d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
		d2d->DrawString(HeaderText, textRect.left, TextTop(Font, textRect), ForeColor, Font);
		d2d->PopDrawRect();
	}
}

void NavigationView::DrawRows(D2DGraphics* d2d, const std::vector<RowInfo>& rows, const Layout& layout)
{
	if (!d2d) return;
	const bool compact = IsCompactMode();
	d2d->PushDrawRect(layout.ContentRect.left, layout.ContentRect.top, RectWidth(layout.ContentRect), RectHeight(layout.ContentRect));
	for (const auto& row : rows)
	{
		if (row.Index < 0 || row.Index >= (int)this->Items.size())
			continue;
		const auto& item = this->Items[row.Index];
		auto rect = GetRowRect(row, layout);
		if (rect.bottom < layout.ContentRect.top || rect.top > layout.ContentRect.bottom)
			continue;

		if (item.Kind == NavigationViewItemKind::Header)
		{
			D2D1_RECT_F headerRect = rect;
			headerRect.left += 4.0f;
			d2d->DrawString(item.Text, headerRect.left, TextTop(Font, headerRect), MutedTextColor, Font);
			continue;
		}
		if (item.Kind == NavigationViewItemKind::Separator)
		{
			float cy = rect.top + RectHeight(rect) * 0.5f;
			d2d->DrawLine(rect.left + 8.0f, cy, rect.right - 8.0f, cy, SeparatorColor, 1.0f);
			continue;
		}

		D2D1_RECT_F itemRect = D2D1::RectF(rect.left, rect.top + 2.0f, rect.right, rect.bottom - 2.0f);
		D2D1_COLOR_F textColor = item.Enabled ? ForeColor : FadeColor(MutedTextColor, 0.70f);
		if (item.Selected || row.Index == SelectedIndex)
		{
			d2d->FillRoundRect(itemRect, SelectedItemBackColor, 7.0f);
			d2d->FillRoundRect(itemRect.left, itemRect.top + 6.0f, SelectedAccentWidth,
				(std::max)(6.0f, RectHeight(itemRect) - 12.0f), AccentColor, SelectedAccentWidth * 0.5f);
			textColor = item.Enabled ? SelectedItemForeColor : FadeColor(SelectedItemForeColor, 0.65f);
		}
		else if (row.Index == HoveredIndex)
		{
			d2d->FillRoundRect(itemRect, UnderMouseItemBackColor, 7.0f);
		}

		float iconX = itemRect.left + 10.0f;
		float iconY = itemRect.top + (RectHeight(itemRect) - IconSize) * 0.5f;
		if (compact)
			iconX = itemRect.left + (RectWidth(itemRect) - IconSize) * 0.5f;
		if (auto* bmp = const_cast<NavigationViewItem&>(item).GetIconBitmap(d2d))
		{
			d2d->DrawBitmap(bmp, iconX, iconY, IconSize, IconSize);
		}
		else if (ShowIconPlaceholder)
		{
			d2d->FillRoundRect(iconX, iconY, IconSize, IconSize, IconPlaceholderColor, 5.0f);
			if (!item.Text.empty() && IconSize >= 15.0f)
			{
				std::wstring letter(1, item.Text[0]);
				d2d->DrawStringCentered(letter, iconX + IconSize * 0.5f, iconY + IconSize * 0.5f, AccentColor, Font);
			}
		}

		if (!compact)
		{
			float textLeft = iconX + IconSize + 10.0f;
			float textRight = itemRect.right - 10.0f;
			if (!item.BadgeText.empty())
			{
				auto badgeSize = Font->GetTextSize(item.BadgeText);
				float badgeW = (std::max)(22.0f, badgeSize.width + 12.0f);
				float badgeH = 20.0f;
				const float badgeGap = 8.0f;
				const float badgeRightPadding = 8.0f;
				const float minTextVisibleWidth = 36.0f;
				const float minBadgeLeft = textLeft + minTextVisibleWidth + badgeGap;
				const float badgeRight = itemRect.right - badgeRightPadding;
				if (badgeRight - minBadgeLeft >= 12.0f)
				{
					float badgeLeft = (std::max)(badgeRight - badgeW, minBadgeLeft);
					D2D1_RECT_F badgeRect{ badgeLeft, itemRect.top + (RectHeight(itemRect) - badgeH) * 0.5f,
						badgeLeft + badgeW, itemRect.top + (RectHeight(itemRect) - badgeH) * 0.5f + badgeH };
					const float clipRight = (std::min)(badgeRect.right, badgeRight);
					d2d->PushDrawRect(badgeRect.left, badgeRect.top, (std::max)(1.0f, clipRight - badgeRect.left), RectHeight(badgeRect));
					d2d->FillRoundRect(badgeRect, BadgeBackColor, badgeH * 0.5f);
					d2d->DrawStringCentered(item.BadgeText, badgeRect.left + RectWidth(badgeRect) * 0.5f,
						badgeRect.top + RectHeight(badgeRect) * 0.5f, BadgeForeColor, Font);
					d2d->PopDrawRect();
					textRight = badgeRect.left - badgeGap;
				}
			}
			D2D1_RECT_F textRect{ textLeft, itemRect.top, (std::max)(textLeft + 1.0f, textRight), itemRect.bottom };
			d2d->PushDrawRect(textRect.left, textRect.top, RectWidth(textRect), RectHeight(textRect));
			d2d->DrawString(item.Text, textRect.left, TextTop(Font, textRect), textColor, Font);
			d2d->PopDrawRect();
		}
	}
	d2d->PopDrawRect();

	if (!FooterText.empty() && !compact && RectHeight(layout.FooterRect) > 0.0f)
	{
		d2d->DrawLine(layout.FooterRect.left + 10.0f, layout.FooterRect.top, layout.FooterRect.right - 10.0f, layout.FooterRect.top, SeparatorColor, 1.0f);
		D2D1_RECT_F textRect{ layout.FooterRect.left + 12.0f, layout.FooterRect.top, layout.FooterRect.right - 12.0f, layout.FooterRect.bottom };
		d2d->PushDrawRect(textRect.left, textRect.top, RectWidth(textRect), RectHeight(textRect));
		d2d->DrawString(FooterText, textRect.left, TextTop(Font, textRect), MutedTextColor, Font);
		d2d->PopDrawRect();
	}
}

void NavigationView::DrawScrollBar(D2DGraphics* d2d, const Layout& layout)
{
	if (!d2d || !layout.NeedVScroll)
		return;
	d2d->FillRoundRect(layout.ScrollTrackRect, ScrollBackColor, RectWidth(layout.ScrollTrackRect) * 0.5f);
	d2d->FillRoundRect(layout.ScrollThumbRect, ScrollForeColor, RectWidth(layout.ScrollThumbRect) * 0.5f);
}

void NavigationView::UpdateHover(int localX, int localY)
{
	int hit = HitTestItem(localX, localY);
	if (hit != this->HoveredIndex)
	{
		this->HoveredIndex = hit;
		this->InvalidateVisual();
	}
}

void NavigationView::UpdateScrollByThumb(float localY)
{
	auto layout = CalcLayout();
	if (!layout.NeedVScroll) return;
	const float trackH = RectHeight(layout.ScrollTrackRect);
	const float thumbH = RectHeight(layout.ScrollThumbRect);
	const float range = trackH - thumbH;
	if (range <= 0.0f || layout.MaxScrollY <= 0.0f) return;
	float targetTop = localY - _scrollThumbGrabOffsetY;
	float t = (targetTop - layout.ScrollTrackRect.top) / range;
	SetScrollOffset((std::clamp)(t, 0.0f, 1.0f) * layout.MaxScrollY);
}

void NavigationView::MoveSelectionBy(int delta)
{
	if (this->Items.empty()) return;
	int start = this->FocusedIndex >= 0 ? this->FocusedIndex : this->SelectedIndex;
	if (start < 0)
		start = delta >= 0 ? -1 : (int)this->Items.size();
	int index = start;
	for (;;)
	{
		index += delta;
		if (index < 0 || index >= (int)this->Items.size())
			break;
		if (this->Items[index].Kind == NavigationViewItemKind::Item && this->Items[index].Enabled)
		{
			SelectItem(index);
			return;
		}
	}
}

void NavigationView::SyncSelectedIndexFromItems()
{
	this->SelectedIndex = -1;
	for (int i = 0; i < (int)this->Items.size(); ++i)
	{
		if (this->Items[i].Selected)
		{
			this->SelectedIndex = i;
			this->FocusedIndex = i;
			return;
		}
	}
}

void NavigationView::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	auto size = this->_size;
	float width = (float)size.cx;
	float height = (float)size.cy;
	std::vector<RowInfo> rows;
	auto layout = CalcLayout(&rows);
	ClampScroll(layout);

	this->BeginRender();
	{
		D2D1_COLOR_F surface = this->BackColor.a > 0.0f ? this->BackColor : this->SurfaceColor;
		d2d->FillRoundRect(Border * 0.5f, Border * 0.5f,
			(std::max)(0.0f, width - Border), (std::max)(0.0f, height - Border),
			surface, CornerRadius);
		if (this->Image)
			this->RenderImage(CornerRadius);
		DrawHeader(d2d, layout);
		DrawRows(d2d, rows, layout);
		DrawScrollBar(d2d, layout);
		if (Border > 0.0f && this->BorderColor.a > 0.0f)
			d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f,
				(std::max)(0.0f, width - Border), (std::max)(0.0f, height - Border),
				this->BorderColor, Border, CornerRadius);
		if (!this->Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.48f }, CornerRadius);
	}
	this->EndRender();
}

bool NavigationView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (delta != 0)
		{
			const float step = (float)(std::max)(8, this->MouseWheelStep);
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
		if (HitTestToggle(layout, localX, localY))
		{
			TogglePane();
			return true;
		}
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
		int hit = HitTestItem(localX, localY);
		if (hit >= 0)
			this->FocusedIndex = hit;
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
			int hit = HitTestItem(localX, localY);
			if (hit >= 0)
			{
				if (AutoSelectOnClick)
					SelectItem(hit);
				this->OnItemClick(this, hit);
				this->OnMouseClick(this, e);
			}
		}
		return true;
	}
	case WM_LBUTTONDBLCLK:
	{
		int hit = HitTestItem(localX, localY);
		MouseEventArgs e(MouseButtons::Left, 2, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, e);
		if (hit >= 0)
			this->OnItemDoubleClick(this, hit);
		return true;
	}
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_UP: MoveSelectionBy(-1); break;
		case VK_DOWN: MoveSelectionBy(1); break;
		case VK_HOME:
			for (int i = 0; i < (int)Items.size(); ++i)
			{
				if (Items[i].Kind == NavigationViewItemKind::Item && Items[i].Enabled)
				{
					SelectItem(i);
					break;
				}
			}
			break;
		case VK_END:
			for (int i = (int)Items.size() - 1; i >= 0; --i)
			{
				if (Items[i].Kind == NavigationViewItemKind::Item && Items[i].Enabled)
				{
					SelectItem(i);
					break;
				}
			}
			break;
		case VK_RETURN:
		case VK_SPACE:
			if (FocusedIndex >= 0)
				SelectItem(FocusedIndex);
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

UIClass SideBar::Type()
{
	return UIClass::UI_SideBar;
}

SideBar::SideBar(int x, int y, int width, int height)
	: NavigationView(x, y, width, height)
{
	this->ShowToggleButton = false;
	this->ShowHeader = false;
	this->HeaderText.clear();
	this->CornerRadius = 6.0f;
}

BreadcrumbBarItem::BreadcrumbBarItem(std::wstring text, std::wstring value)
	: Text(std::move(text)), Value(std::move(value))
{
}

UIClass BreadcrumbBar::Type()
{
	return UIClass::UI_BreadcrumbBar;
}

void BreadcrumbBar::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		using namespace cui::advanced_properties;
		RegisterMetric(L"Border", &BreadcrumbBar::Border, 1.0f,
			L"Appearance", 200, 10);
		RegisterMetric(L"CornerRadius", &BreadcrumbBar::CornerRadius, 7.0f,
			L"Appearance", 200, 20);
		RegisterMetric(L"ItemPaddingX", &BreadcrumbBar::ItemPaddingX, 10.0f,
			L"Layout", 100, 10);
		RegisterMetric(L"ItemGap", &BreadcrumbBar::ItemGap, 4.0f,
			L"Layout", 100, 20);
		RegisterMetric(L"SeparatorWidth", &BreadcrumbBar::SeparatorWidth, 18.0f,
			L"Layout", 100, 30);
		RegisterColor(L"SurfaceColor", &BreadcrumbBar::SurfaceColor,
			cui::theme::palette::Surface, 100);
		RegisterColor(L"HoverBackColor", &BreadcrumbBar::HoverBackColor,
			cui::theme::palette::AccentSoft, 110);
		RegisterColor(L"SelectedBackColor", &BreadcrumbBar::SelectedBackColor,
			cui::theme::palette::AccentSelected, 120);
		RegisterColor(L"MutedTextColor", &BreadcrumbBar::MutedTextColor,
			cui::theme::palette::TextMuted, 130);
		RegisterColor(L"AccentColor", &BreadcrumbBar::AccentColor,
			cui::theme::palette::Accent, 140);
		auto selectedOptions = Options<BreadcrumbBar, int>(-1,
			L"Behavior", 110, 10, ControlPropertyEditorKind::Number);
		selectedOptions.Design.Minimum = -1.0;
		BindingPropertyRegistry::Register<BreadcrumbBar, int>(L"SelectedIndex",
			[](BreadcrumbBar& target) { return target.SelectedIndex; },
			[](BreadcrumbBar& target, const int& value)
			{
				if (value < 0) target.SelectedIndex = -1;
				else if (target.Items.empty()) target.SelectedIndex = value;
				else target.SelectItem(value);
			}, Subscriber<BreadcrumbBar>(L"SelectedIndex"),
			std::move(selectedOptions));
		return true;
	}();
	(void)registered;
}

BreadcrumbBar::BreadcrumbBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
}

int BreadcrumbBar::AddItem(const BreadcrumbBarItem& item)
{
	Items.push_back(item);
	if (SelectedIndex < 0)
		SelectedIndex = (int)Items.size() - 1;
	InvalidateVisual();
	return (int)Items.size() - 1;
}

int BreadcrumbBar::AddItem(const std::wstring& text, const std::wstring& value)
{
	return AddItem(BreadcrumbBarItem(text, value));
}

void BreadcrumbBar::SetPath(const std::vector<std::wstring>& path)
{
	Items.clear();
	for (const auto& item : path)
		Items.push_back(BreadcrumbBarItem(item));
	SelectedIndex = Items.empty() ? -1 : (int)Items.size() - 1;
	InvalidateVisual();
}

void BreadcrumbBar::ClearItems()
{
	Items.clear();
	SelectedIndex = -1;
	HoveredIndex = -1;
	SelectionChanged(this);
	InvalidateVisual();
}

bool BreadcrumbBar::SelectItem(int index)
{
	if (index < 0 || index >= (int)Items.size() || !Items[index].Enabled)
		return false;
	bool changed = SelectedIndex != index;
	SelectedIndex = index;
	if (changed)
		SelectionChanged(this);
	InvalidateVisual();
	return true;
}

std::vector<BreadcrumbBar::ItemRegion> BreadcrumbBar::BuildLayout() const
{
	std::vector<ItemRegion> regions;
	auto size = this->_size;
	float x = Border + 6.0f;
	float y = Border + 4.0f;
	float h = (std::max)(0.0f, (float)size.cy - Border * 2.0f - 8.0f);
	float rightLimit = (float)size.cx - Border - 6.0f;
	for (int i = 0; i < (int)Items.size(); ++i)
	{
		const auto& item = Items[i];
		class Font* font = this->_font ? this->_font : GetDefaultFontObject();
		auto ts = font->GetTextSize(item.Text);
		float w = (std::clamp)(ts.width + ItemPaddingX * 2.0f, 36.0f, 180.0f);
		if (x + w > rightLimit)
		{
			w = (std::max)(0.0f, rightLimit - x);
		}
		if (w <= 1.0f)
			break;
		regions.push_back(ItemRegion{ i, D2D1::RectF(x, y, x + w, y + h) });
		x += w;
		if (i < (int)Items.size() - 1)
			x += SeparatorWidth + ItemGap;
	}
	return regions;
}

int BreadcrumbBar::HitTestItem(int localX, int localY) const
{
	auto regions = BuildLayout();
	for (auto it = regions.rbegin(); it != regions.rend(); ++it)
	{
		if (PtInRectF(it->Rect, (float)localX, (float)localY))
			return it->Index;
	}
	return -1;
}

CursorKind BreadcrumbBar::QueryCursor(int localX, int localY)
{
	if (!Enable) return CursorKind::Arrow;
	int hit = HitTestItem(localX, localY);
	return hit >= 0 && Items[hit].Enabled ? CursorKind::Hand : CursorKind::Arrow;
}

void BreadcrumbBar::DrawChevron(D2DGraphics* d2d, float cx, float cy, D2D1_COLOR_F color)
{
	DrawBreadcrumbChevron(d2d, cx, cy, color);
}

void BreadcrumbBar::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	auto regions = BuildLayout();

	this->BeginRender();
	{
		D2D1_COLOR_F surface = this->BackColor.a > 0.0f ? this->BackColor : this->SurfaceColor;
		d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, (std::max)(0.0f, width - Border),
			(std::max)(0.0f, height - Border), surface, CornerRadius);
		for (size_t i = 0; i < regions.size(); ++i)
		{
			const auto& region = regions[i];
			const auto& item = Items[region.Index];
			bool selected = region.Index == SelectedIndex;
			bool hovered = region.Index == HoveredIndex;
			if (selected)
				d2d->FillRoundRect(region.Rect, SelectedBackColor, 6.0f);
			else if (hovered)
				d2d->FillRoundRect(region.Rect, HoverBackColor, 6.0f);
			D2D1_COLOR_F textColor = item.Enabled ? (selected ? ForeColor : MutedTextColor) : FadeColor(MutedTextColor, 0.58f);
			d2d->PushDrawRect(region.Rect.left + ItemPaddingX, region.Rect.top, (std::max)(1.0f, RectWidth(region.Rect) - ItemPaddingX * 2.0f), RectHeight(region.Rect));
			d2d->DrawString(item.Text, region.Rect.left + ItemPaddingX, TextTop(Font, region.Rect), textColor, Font);
			d2d->PopDrawRect();
			if (i + 1 < regions.size())
			{
				float sepX = region.Rect.right + SeparatorWidth * 0.5f + ItemGap * 0.5f;
				DrawChevron(d2d, sepX, region.Rect.top + RectHeight(region.Rect) * 0.5f, MutedTextColor);
			}
		}
		if (Border > 0.0f && this->BorderColor.a > 0.0f)
			d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, (std::max)(0.0f, width - Border),
				(std::max)(0.0f, height - Border), this->BorderColor, Border, CornerRadius);
		if (!this->Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.48f }, CornerRadius);
	}
	this->EndRender();
}

bool BreadcrumbBar::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	(void)lParam;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm)
			this->ParentForm->UnderMouse = this;
		int hit = HitTestItem(localX, localY);
		if (hit != HoveredIndex)
		{
			HoveredIndex = hit;
			InvalidateVisual();
		}
		MouseEventArgs e(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (this->ParentForm)
			this->ParentForm->SetSelectedControl(this, false);
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseUp(this, e);
		int hit = HitTestItem(localX, localY);
		if (hit >= 0 && Items[hit].Enabled)
		{
			SelectItem(hit);
			OnItemClick(this, hit);
			OnMouseClick(this, e);
		}
		return true;
	}
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}
