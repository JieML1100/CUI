#define NOMINMAX
#include "ListView.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
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

	template<typename TValue>
	ControlPropertyOptions<ListView, TValue> ListViewPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<ListView, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto ListViewPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			ListView& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}

	ControlPropertyOptions<ListView, float> ListViewMetricOptions(
		float defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order)
	{
		auto options = ListViewPropertyOptions(
			defaultValue, category, categoryOrder, order,
			ControlPropertyEditorKind::Number);
		options.Coerce = [](
			ListView&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	ControlPropertyOptions<ListView, D2D1_COLOR_F> ListViewColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ListViewPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = ColorEquals;
		return options;
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

void ListView::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto viewModeOptions = ListViewPropertyOptions(
			static_cast<int>(ListViewViewMode::List),
			L"Layout", 100, 10, ControlPropertyEditorKind::Choice);
		viewModeOptions.Coerce = [](
			ListView& target, const int& proposed) -> std::optional<int>
		{
			if (target.IsListBox())
				return static_cast<int>(ListViewViewMode::List);
			switch (static_cast<ListViewViewMode>(proposed))
			{
			case ListViewViewMode::List:
			case ListViewViewMode::Details:
			case ListViewViewMode::Tile:
			case ListViewViewMode::Icon:
				return proposed;
			default:
				return std::nullopt;
			}
		};
		viewModeOptions.Changed = [](
			ListView& target, const int&, const int&)
		{
			target.ClampScrollToRange();
			if (target.FocusedIndex >= 0)
				target.EnsureVisible(target.FocusedIndex);
			target.NotifyAccessibilityStructureChanged();
			target.NotifyAccessibilityScrollChanged();
		};
		viewModeOptions.Design.Choices = {
			{ L"List", BindingValue(static_cast<int>(ListViewViewMode::List)) },
			{ L"Details", BindingValue(static_cast<int>(ListViewViewMode::Details)) },
			{ L"Tile", BindingValue(static_cast<int>(ListViewViewMode::Tile)) },
			{ L"Icon", BindingValue(static_cast<int>(ListViewViewMode::Icon)) }
		};
		viewModeOptions.Design.BrowsableWhen = [](Control& target)
		{
			return target.Type() == UIClass::UI_ListView;
		};
		BindingPropertyRegistry::Register<ListView, int>(L"ViewMode",
			[](ListView& target) { return static_cast<int>(target.ViewMode); },
			[](ListView& target, const int& value)
			{ target.ViewMode = static_cast<ListViewViewMode>(value); },
			ListViewPropertySubscriber(L"ViewMode"), std::move(viewModeOptions));

		auto selectionModeOptions = ListViewPropertyOptions(
			static_cast<int>(ListViewSelectionMode::Single),
			L"Behavior", 110, 10, ControlPropertyEditorKind::Choice);
		selectionModeOptions.Coerce = [](
			ListView&, const int& proposed) -> std::optional<int>
		{
			if (proposed != static_cast<int>(ListViewSelectionMode::Single)
				&& proposed != static_cast<int>(ListViewSelectionMode::Multiple))
				return std::nullopt;
			return proposed;
		};
		selectionModeOptions.Changed = [](
			ListView& target, const int&, const int&)
		{
			target.NormalizeSelectionForMode();
		};
		selectionModeOptions.Design.Choices = {
			{ L"Single", BindingValue(static_cast<int>(ListViewSelectionMode::Single)) },
			{ L"Multiple", BindingValue(static_cast<int>(ListViewSelectionMode::Multiple)) }
		};
		BindingPropertyRegistry::Register<ListView, int>(L"SelectionMode",
			[](ListView& target) { return static_cast<int>(target.SelectionMode); },
			[](ListView& target, const int& value)
			{ target.SelectionMode = static_cast<ListViewSelectionMode>(value); },
			ListViewPropertySubscriber(L"SelectionMode"),
			std::move(selectionModeOptions));

		auto selectedIndexOptions = ListViewPropertyOptions(
			-1, L"Behavior", 110, 100,
			ControlPropertyEditorKind::Number);
		selectedIndexOptions.Coerce = [](
			ListView& target, const int& proposed) -> std::optional<int>
		{
			if (proposed < 0) return -1;
			return target.Items.empty()
				? proposed
				: (std::min)(proposed, static_cast<int>(target.Items.size()) - 1);
		};
		selectedIndexOptions.Changed = [](
			ListView& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectedIndexChange(oldValue, newValue);
		};
		selectedIndexOptions.Design.Browsable = false;
		selectedIndexOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<ListView, int>(L"SelectedIndex",
			[](ListView& target) { return target.SelectedIndex; },
			[](ListView& target, const int& value) { target.SelectedIndex = value; },
			ListViewPropertySubscriber(L"SelectedIndex"),
			std::move(selectedIndexOptions));

		auto indexStateOptions = ListViewPropertyOptions(
			-1, L"Behavior", 110, 110,
			ControlPropertyEditorKind::Number);
		indexStateOptions.Coerce = [](
			ListView& target, const int& proposed) -> std::optional<int>
		{
			if (target.Items.empty() || proposed < 0) return -1;
			return (std::min)(proposed, static_cast<int>(target.Items.size()) - 1);
		};
		indexStateOptions.Design.Browsable = false;
		indexStateOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<ListView, int>(L"HoveredIndex",
			[](ListView& target) { return target.HoveredIndex; },
			[](ListView& target, const int& value) { target.HoveredIndex = value; },
			ListViewPropertySubscriber(L"HoveredIndex"), indexStateOptions);
		BindingPropertyRegistry::Register<ListView, int>(L"FocusedIndex",
			[](ListView& target) { return target.FocusedIndex; },
			[](ListView& target, const int& value) { target.FocusedIndex = value; },
			ListViewPropertySubscriber(L"FocusedIndex"), std::move(indexStateOptions));

		auto scrollOptions = ListViewPropertyOptions(
			0.0f, L"Behavior", 110, 120,
			ControlPropertyEditorKind::Number);
		scrollOptions.Coerce = [](
			ListView& target, const float& proposed) -> std::optional<float>
		{
			if (!std::isfinite(proposed)) return std::nullopt;
			const auto layout = target.CalcLayout();
			return (std::clamp)(proposed, 0.0f, layout.MaxScrollY);
		};
		scrollOptions.Changed = [](
			ListView& target, const float&, const float&)
		{
			target.ScrollChanged(&target);
			target.NotifyAccessibilityScrollChanged();
		};
		scrollOptions.Design.Browsable = false;
		scrollOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<ListView, float>(L"ScrollYOffset",
			[](ListView& target) { return target.ScrollYOffset; },
			[](ListView& target, const float& value) { target.ScrollYOffset = value; },
			ListViewPropertySubscriber(L"ScrollYOffset"), std::move(scrollOptions));

		auto showColumnHeadersOptions = ListViewPropertyOptions(
			true, L"Layout", 100, 20, ControlPropertyEditorKind::Boolean);
		showColumnHeadersOptions.Coerce = [](
			ListView& target, const bool& proposed) -> std::optional<bool>
		{
			return target.IsListBox() ? false : proposed;
		};
		showColumnHeadersOptions.Changed = [](
			ListView& target, const bool&, const bool&)
		{
			target.ClampScrollToRange();
			target.NotifyAccessibilityScrollChanged();
		};
		showColumnHeadersOptions.Design.BrowsableWhen = [](Control& target)
		{
			return target.Type() == UIClass::UI_ListView;
		};
		BindingPropertyRegistry::Register<ListView, bool>(L"ShowColumnHeaders",
			[](ListView& target) { return target.ShowColumnHeaders; },
			[](ListView& target, const bool& value) { target.ShowColumnHeaders = value; },
			ListViewPropertySubscriber(L"ShowColumnHeaders"),
			std::move(showColumnHeadersOptions));

#define CUI_REGISTER_LIST_BOOL(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<ListView, bool>(propertyName, \
			[](ListView& target) { return target.name; }, \
			[](ListView& target, const bool& value) { target.name = value; }, \
			ListViewPropertySubscriber(propertyName), \
			ListViewPropertyOptions(defaultValue, L"Behavior", 110, order, \
				ControlPropertyEditorKind::Boolean))

		CUI_REGISTER_LIST_BOOL(ShowCheckBoxes, L"ShowCheckBoxes", false, 20);
		CUI_REGISTER_LIST_BOOL(AlternatingRows, L"AlternatingRows", false, 30);
		CUI_REGISTER_LIST_BOOL(FullRowSelect, L"FullRowSelect", true, 40);
		CUI_REGISTER_LIST_BOOL(HideSelectionWhenLostFocus, L"HideSelectionWhenLostFocus", false, 50);

#undef CUI_REGISTER_LIST_BOOL

		auto wheelOptions = ListViewPropertyOptions(
			48, L"Behavior", 110, 60, ControlPropertyEditorKind::Number);
		wheelOptions.Coerce = [](
			ListView&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		wheelOptions.Design.Minimum = 0.0;
		wheelOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<ListView, int>(L"MouseWheelStep",
			[](ListView& target) { return target.MouseWheelStep; },
			[](ListView& target, const int& value) { target.MouseWheelStep = value; },
			ListViewPropertySubscriber(L"MouseWheelStep"), std::move(wheelOptions));

#define CUI_REGISTER_LIST_METRIC(name, propertyName, defaultValue, order) \
		{ \
			auto options = ListViewMetricOptions(defaultValue, L"Layout", 100, order); \
			options.Changed = [](ListView& target, const float&, const float&) \
			{ target.ClampScrollToRange(); target.NotifyAccessibilityScrollChanged(); }; \
			BindingPropertyRegistry::Register<ListView, float>(propertyName, \
				[](ListView& target) { return target.name; }, \
				[](ListView& target, const float& value) { target.name = value; }, \
				ListViewPropertySubscriber(propertyName), std::move(options)); \
		}

		CUI_REGISTER_LIST_METRIC(Border, L"Border", 1.0f, 30);
		CUI_REGISTER_LIST_METRIC(CornerRadius, L"CornerRadius", 6.0f, 40);
		CUI_REGISTER_LIST_METRIC(HeaderHeight, L"HeaderHeight", 30.0f, 50);
		CUI_REGISTER_LIST_METRIC(RowHeight, L"RowHeight", 30.0f, 60);
		CUI_REGISTER_LIST_METRIC(TileHeight, L"TileHeight", 58.0f, 70);
		CUI_REGISTER_LIST_METRIC(IconItemWidth, L"IconItemWidth", 96.0f, 80);
		CUI_REGISTER_LIST_METRIC(IconItemHeight, L"IconItemHeight", 82.0f, 90);
		CUI_REGISTER_LIST_METRIC(IconSize, L"IconSize", 32.0f, 100);
		CUI_REGISTER_LIST_METRIC(CheckBoxSize, L"CheckBoxSize", 14.0f, 110);
		CUI_REGISTER_LIST_METRIC(ItemPaddingX, L"ItemPaddingX", 8.0f, 120);
		CUI_REGISTER_LIST_METRIC(ItemPaddingY, L"ItemPaddingY", 3.0f, 130);
		CUI_REGISTER_LIST_METRIC(ItemGap, L"ItemGap", 8.0f, 140);
		CUI_REGISTER_LIST_METRIC(SelectedAccentWidth, L"SelectedAccentWidth", 3.0f, 150);
		CUI_REGISTER_LIST_METRIC(ScrollBarSize, L"ScrollBarSize", 8.0f, 160);

#undef CUI_REGISTER_LIST_METRIC

#define CUI_REGISTER_LIST_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<ListView, D2D1_COLOR_F>(propertyName, \
			[](ListView& target) { return target.name; }, \
			[](ListView& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			ListViewPropertySubscriber(propertyName), \
			ListViewColorOptions(defaultValue, order))

		CUI_REGISTER_LIST_COLOR(HeaderBackColor, L"HeaderBackColor", (D2D1_COLOR_F{ 0.18f, 0.22f, 0.28f, 0.95f }), 10);
		CUI_REGISTER_LIST_COLOR(HeaderForeColor, L"HeaderForeColor", (D2D1_COLOR_F{ 0.90f, 0.93f, 0.98f, 1.0f }), 20);
		CUI_REGISTER_LIST_COLOR(GridLineColor, L"GridLineColor", (D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.24f }), 30);
		CUI_REGISTER_LIST_COLOR(AlternateItemBackColor, L"AlternateItemBackColor", (D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f }), 40);
		CUI_REGISTER_LIST_COLOR(SelectedItemBackColor, L"SelectedItemBackColor", (D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.32f }), 50);
		CUI_REGISTER_LIST_COLOR(SelectedItemForeColor, L"SelectedItemForeColor", Colors::Black, 60);
		CUI_REGISTER_LIST_COLOR(UnderMouseItemBackColor, L"UnderMouseItemBackColor", (D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.12f }), 70);
		CUI_REGISTER_LIST_COLOR(DisabledItemForeColor, L"DisabledItemForeColor", (D2D1_COLOR_F{ 0.50f, 0.52f, 0.58f, 1.0f }), 80);
		CUI_REGISTER_LIST_COLOR(MutedTextColor, L"MutedTextColor", (D2D1_COLOR_F{ 0.58f, 0.62f, 0.70f, 1.0f }), 90);
		CUI_REGISTER_LIST_COLOR(AccentColor, L"AccentColor", (D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f }), 100);
		CUI_REGISTER_LIST_COLOR(CheckBackColor, L"CheckBackColor", Colors::White, 110);
		CUI_REGISTER_LIST_COLOR(CheckBorderColor, L"CheckBorderColor", (D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 1.0f }), 120);
		CUI_REGISTER_LIST_COLOR(ScrollBackColor, L"ScrollBackColor", Colors::LightGray, 130);
		CUI_REGISTER_LIST_COLOR(ScrollForeColor, L"ScrollForeColor", Colors::DimGrey, 140);

#undef CUI_REGISTER_LIST_COLOR
		return true;
	}();
	(void)registered;
}

#define CUI_LIST_VIEW_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(ListView, type, name) { return field; } \
	SET_CPP(ListView, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_LIST_VIEW_PROPERTY_IMPL(bool, ShowCheckBoxes, _showCheckBoxes, L"ShowCheckBoxes")
CUI_LIST_VIEW_PROPERTY_IMPL(bool, ShowColumnHeaders, _showColumnHeaders, L"ShowColumnHeaders")
CUI_LIST_VIEW_PROPERTY_IMPL(bool, AlternatingRows, _alternatingRows, L"AlternatingRows")
CUI_LIST_VIEW_PROPERTY_IMPL(bool, FullRowSelect, _fullRowSelect, L"FullRowSelect")
CUI_LIST_VIEW_PROPERTY_IMPL(bool, HideSelectionWhenLostFocus, _hideSelectionWhenLostFocus, L"HideSelectionWhenLostFocus")
CUI_LIST_VIEW_PROPERTY_IMPL(float, Border, _border, L"Border")
CUI_LIST_VIEW_PROPERTY_IMPL(float, CornerRadius, _cornerRadius, L"CornerRadius")
CUI_LIST_VIEW_PROPERTY_IMPL(float, HeaderHeight, _headerHeight, L"HeaderHeight")
CUI_LIST_VIEW_PROPERTY_IMPL(float, RowHeight, _rowHeight, L"RowHeight")
CUI_LIST_VIEW_PROPERTY_IMPL(float, TileHeight, _tileHeight, L"TileHeight")
CUI_LIST_VIEW_PROPERTY_IMPL(float, IconItemWidth, _iconItemWidth, L"IconItemWidth")
CUI_LIST_VIEW_PROPERTY_IMPL(float, IconItemHeight, _iconItemHeight, L"IconItemHeight")
CUI_LIST_VIEW_PROPERTY_IMPL(float, IconSize, _iconSize, L"IconSize")
CUI_LIST_VIEW_PROPERTY_IMPL(float, CheckBoxSize, _checkBoxSize, L"CheckBoxSize")
CUI_LIST_VIEW_PROPERTY_IMPL(float, ItemPaddingX, _itemPaddingX, L"ItemPaddingX")
CUI_LIST_VIEW_PROPERTY_IMPL(float, ItemPaddingY, _itemPaddingY, L"ItemPaddingY")
CUI_LIST_VIEW_PROPERTY_IMPL(float, ItemGap, _itemGap, L"ItemGap")
CUI_LIST_VIEW_PROPERTY_IMPL(float, SelectedAccentWidth, _selectedAccentWidth, L"SelectedAccentWidth")
CUI_LIST_VIEW_PROPERTY_IMPL(float, ScrollBarSize, _scrollBarSize, L"ScrollBarSize")
CUI_LIST_VIEW_PROPERTY_IMPL(int, MouseWheelStep, _mouseWheelStep, L"MouseWheelStep")
CUI_LIST_VIEW_PROPERTY_IMPL(int, SelectedIndex, _selectedIndex, L"SelectedIndex")
CUI_LIST_VIEW_PROPERTY_IMPL(int, HoveredIndex, _hoveredIndex, L"HoveredIndex")
CUI_LIST_VIEW_PROPERTY_IMPL(int, FocusedIndex, _focusedIndex, L"FocusedIndex")
CUI_LIST_VIEW_PROPERTY_IMPL(float, ScrollYOffset, _scrollYOffset, L"ScrollYOffset")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, HeaderBackColor, _headerBackColor, L"HeaderBackColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, HeaderForeColor, _headerForeColor, L"HeaderForeColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, GridLineColor, _gridLineColor, L"GridLineColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, AlternateItemBackColor, _alternateItemBackColor, L"AlternateItemBackColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, SelectedItemBackColor, _selectedItemBackColor, L"SelectedItemBackColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, SelectedItemForeColor, _selectedItemForeColor, L"SelectedItemForeColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, UnderMouseItemBackColor, _underMouseItemBackColor, L"UnderMouseItemBackColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, DisabledItemForeColor, _disabledItemForeColor, L"DisabledItemForeColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, MutedTextColor, _mutedTextColor, L"MutedTextColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, CheckBackColor, _checkBackColor, L"CheckBackColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, CheckBorderColor, _checkBorderColor, L"CheckBorderColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ScrollBackColor, _scrollBackColor, L"ScrollBackColor")
CUI_LIST_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ScrollForeColor, _scrollForeColor, L"ScrollForeColor")

#undef CUI_LIST_VIEW_PROPERTY_IMPL

GET_CPP(ListView, ListViewViewMode, ViewMode)
{
	return static_cast<ListViewViewMode>(_viewMode);
}

SET_CPP(ListView, ListViewViewMode, ViewMode)
{
	(void)SetPropertyField(L"ViewMode", _viewMode, static_cast<int>(value));
}

GET_CPP(ListView, ListViewSelectionMode, SelectionMode)
{
	return static_cast<ListViewSelectionMode>(_selectionMode);
}

SET_CPP(ListView, ListViewSelectionMode, SelectionMode)
{
	(void)SetPropertyField(
		L"SelectionMode", _selectionMode, static_cast<int>(value));
}

ListView::UpdateScope::UpdateScope(ListView& owner) noexcept
	: _owner(&owner)
{
	_owner->BeginUpdate();
}

ListView::UpdateScope::~UpdateScope()
{
	Commit();
}

ListView::UpdateScope::UpdateScope(UpdateScope&& other) noexcept
	: _owner(other._owner)
{
	other._owner = nullptr;
}

ListView::UpdateScope& ListView::UpdateScope::operator=(
	UpdateScope&& other) noexcept
{
	if (this == &other) return *this;
	Commit();
	_owner = other._owner;
	other._owner = nullptr;
	return *this;
}

void ListView::UpdateScope::Commit() noexcept
{
	if (!_owner) return;
	auto* owner = _owner;
	_owner = nullptr;
	owner->EndUpdate();
}

ListView::ListView(int x, int y, int width, int height)
{
	Items.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnItemsCollectionChanged(change); });
	Columns.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnColumnsCollectionChanged(change); });
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.0f };
	this->BorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 0.72f };
}

void ListView::BeginUpdate() noexcept
{
	if (_updateDepth++ != 0) return;
	// Internal identity, selection, and geometry state advances per mutation;
	// public collection listeners still receive one coalesced Reset.
	Items.BeginUpdate(true);
	Columns.BeginUpdate(true);
}

void ListView::EndUpdate() noexcept
{
	if (_updateDepth == 0) return;
	if (_updateDepth > 1)
	{
		--_updateDepth;
		return;
	}
	if (_updatePendingCollectionRefresh)
		ClampScrollToRange();
	Items.EndUpdate();
	Columns.EndUpdate();
	_updateDepth = 0;
	if (!_updatePendingCollectionRefresh) return;
	_updatePendingCollectionRefresh = false;
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
}

void ListView::Clear()
{
	auto update = DeferUpdates();
	ClearItems();
	ClearColumns();
}

void ListView::ClearItems()
{
	this->Items.clear();
}

void ListView::SetItems(std::vector<ListViewItem> items)
{
	this->Items = std::move(items);
}

void ListView::ClearColumns()
{
	this->Columns.clear();
}

int ListView::AddItem(const ListViewItem& item)
{
	this->Items.push_back(item);
	return static_cast<int>(this->Items.size()) - 1;
}

void ListView::AddColumn(const ListViewColumn& column)
{
	this->Columns.push_back(column);
}

bool ListView::RemoveItemAt(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	this->Items.erase(this->Items.begin() + index);
	return true;
}

bool ListView::SwapItems(int indexA, int indexB)
{
	if (indexA < 0 || indexB < 0 || indexA >= (int)this->Items.size() || indexB >= (int)this->Items.size()) return false;
	if (indexA == indexB) return true;
	return Items.SwapIndices(
		static_cast<size_t>(indexA), static_cast<size_t>(indexB));
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
	return (_selectedIndex >= 0 && _selectedIndex < (int)this->Items.size())
		? &this->Items[static_cast<size_t>(_selectedIndex)] : nullptr;
}

bool ListView::SelectItem(int index, bool additive, bool range)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	if (!this->Items[index].Enabled) return false;
	EnsureAccessibilityItemIds();
	_lastSelectionUpdateWork = 0;

	bool changed = false;
	if (this->SelectionMode == ListViewSelectionMode::Single || (!additive && !range))
	{
		const uint32_t targetId = Items[static_cast<size_t>(index)].AccessibilityId;
		if (_selectedItemIds.size() != 1
			|| !_selectedItemIds.contains(targetId))
		{
			changed = ClearCachedSelection();
			changed = SetCachedItemSelected(
				static_cast<size_t>(index), true) || changed;
		}
		_anchorIndex = index;
	}
	else if (range)
	{
		int anchor = _anchorIndex >= 0 ? _anchorIndex : (this->SelectedIndex >= 0 ? this->SelectedIndex : index);
		int first = std::min(anchor, index);
		int last = std::max(anchor, index);
		size_t wantedCount = 0;
		bool alreadyMatches = true;
		for (int i = first; i <= last; ++i)
		{
			++_lastSelectionUpdateWork;
			if (!Items[static_cast<size_t>(i)].Enabled) continue;
			++wantedCount;
			alreadyMatches = alreadyMatches
				&& _selectedItemIds.contains(
					Items[static_cast<size_t>(i)].AccessibilityId);
		}
		alreadyMatches = alreadyMatches
			&& wantedCount == _selectedItemIds.size();
		if (!alreadyMatches)
		{
			changed = ClearCachedSelection();
			for (int i = first; i <= last; ++i)
			{
				if (Items[static_cast<size_t>(i)].Enabled)
					changed = SetCachedItemSelected(
						static_cast<size_t>(i), true) || changed;
			}
		}
	}
	else
	{
		changed = SetCachedItemSelected(
			static_cast<size_t>(index), !this->Items[index].Selected);
		_anchorIndex = index;
	}

	int newSelectedIndex = index;
	if (this->SelectionMode == ListViewSelectionMode::Multiple && additive && !range && !this->Items[index].Selected)
		newSelectedIndex = FindFirstCachedSelectedIndex();

	if (changed || this->SelectedIndex != newSelectedIndex || this->FocusedIndex != index)
	{
		CommitPreparedSelection(newSelectedIndex, index, changed);
		return true;
	}

	return false;
}

void ListView::ClearSelection()
{
	EnsureAccessibilityItemIds();
	_lastSelectionUpdateWork = 0;
	const bool changed = ClearCachedSelection();
	this->_anchorIndex = -1;
	CommitPreparedSelection(-1, -1, changed);
}

std::vector<int> ListView::GetSelectedIndices() const
{
	EnsureAccessibilityItemIds();
	std::vector<int> indices;
	indices.reserve(_selectedItemIds.size());
	for (const uint32_t id : _selectedItemIds)
	{
		const auto found = _accessibilityItemIndexById.find(id);
		if (found != _accessibilityItemIndexById.end()
			&& found->second <= static_cast<size_t>(
				(std::numeric_limits<int>::max)()))
			indices.push_back(static_cast<int>(found->second));
	}
	std::sort(indices.begin(), indices.end());
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
	if (!std::isfinite(offsetY)) return;
	auto layout = CalcLayout();
	const float target = (std::clamp)(offsetY, 0.0f, layout.MaxScrollY);
	if (std::fabs(target - this->ScrollYOffset) <= 1e-6f) return;
	SetCurrentScrollYOffset(target);
}

void ListView::GetVisibleItemRange(int& start, int& end) const noexcept
{
	GetVisibleItemRange(CalcLayout(), start, end);
}

int ListView::HitTestItem(int localX, int localY) const
{
	auto layout = CalcLayout();
	if (!PtInRectF(layout.ContentRect, (float)localX, (float)localY)) return -1;

	if (static_cast<ListViewViewMode>(_viewMode) == ListViewViewMode::Icon
		&& !IsListBox())
	{
		const float itemWidth = GetItemPrimaryExtent();
		const float itemHeight = GetItemSecondaryExtent();
		if (itemWidth <= 0.0f || itemHeight <= 0.0f
			|| layout.ColumnsPerRow <= 0) return -1;
		const float contentX = static_cast<float>(localX)
			- layout.ContentRect.left;
		const float contentY = static_cast<float>(localY)
			- layout.ContentRect.top + _scrollYOffset;
		if (contentX < 0.0f || contentY < 0.0f) return -1;
		const double columnValue = std::floor(
			static_cast<double>(contentX) / itemWidth);
		const double rowValue = std::floor(
			static_cast<double>(contentY) / itemHeight);
		if (!std::isfinite(columnValue) || !std::isfinite(rowValue)
			|| columnValue < 0.0 || rowValue < 0.0
			|| columnValue >= layout.ColumnsPerRow
			|| rowValue > (std::numeric_limits<int>::max)())
			return -1;
		const int64_t candidate = static_cast<int64_t>(rowValue)
			* layout.ColumnsPerRow + static_cast<int>(columnValue);
		if (candidate < 0
			|| static_cast<size_t>(candidate) >= Items.size()
			|| candidate > (std::numeric_limits<int>::max)()) return -1;
		const int index = static_cast<int>(candidate);
		return PtInRectF(GetItemRect(index, layout),
			static_cast<float>(localX), static_cast<float>(localY))
			? index : -1;
	}

	const float itemH = GetItemSecondaryExtent();
	if (itemH <= 0.0f) return -1;
	const double indexValue = std::floor((
		static_cast<double>(localY) - layout.ContentRect.top
		+ _scrollYOffset) / itemH);
	if (!std::isfinite(indexValue) || indexValue < 0.0
		|| indexValue > (std::numeric_limits<int>::max)()) return -1;
	const int index = static_cast<int>(indexValue);
	return static_cast<size_t>(index) < Items.size() ? index : -1;
}

void ListView::EnsureAccessibilityItemIds() const
{
	if (!_accessibilityItemIdsDirty
		&& _accessibilityItemIdsByIndex.size() == Items.size()
		&& _accessibilityItemIndexById.size() == Items.size()) return;
	_accessibilityItemIdsDirty = true;
	_accessibilityItemIdsByIndex.clear();
	_accessibilityItemIdsByIndex.resize(Items.size());
	_accessibilityItemIndexById.clear();
	_accessibilityItemIndexById.reserve(Items.size());
	for (size_t index = 0; index < Items.size(); ++index)
	{
		const auto& item = Items[index];
		uint32_t id = item.AccessibilityId;
		while (id == 0
			|| _accessibilityColumnIndexById.contains(id)
			|| _accessibilityCellKeyById.contains(id)
			|| !_accessibilityItemIndexById.emplace(id, index).second)
			id = AllocateAccessibilityVirtualId();
		item.AccessibilityId = id;
		_accessibilityItemIdsByIndex[index] = id;
	}
	_lastAccessibilityIndexUpdateWork = Items.size();
	_accessibilityItemIdsDirty = false;
}

bool ListView::ApplyAccessibilityItemCollectionChange(
	const CollectionChangedEventArgs& change)
{
	_lastAccessibilityIndexUpdateWork = 0;
	const bool coherentBefore = _accessibilityItemIdsByIndex.size()
		== change.OldSize
		&& _accessibilityItemIndexById.size() == change.OldSize
		&& (!_accessibilityItemIdsDirty || change.OldSize == 0);
	if (!coherentBefore)
	{
		_accessibilityItemIdsDirty = true;
		EnsureAccessibilityItemIds();
		return false;
	}
	// Any allocation failure leaves a visible recovery marker; the next query
	// or structural notification will rebuild from the authoritative Items.
	_accessibilityItemIdsDirty = true;

	auto repairItemId = [this](size_t index)
	{
		auto& item = Items[index];
		uint32_t id = item.AccessibilityId;
		while (id == 0 || _accessibilityItemIndexById.contains(id)
			|| _accessibilityColumnIndexById.contains(id)
			|| _accessibilityCellKeyById.contains(id))
			id = AllocateAccessibilityVirtualId();
		item.AccessibilityId = id;
		_accessibilityItemIdsByIndex[index] = id;
		_accessibilityItemIndexById.emplace(id, index);
	};
	auto updateRange = [this](size_t first, size_t end)
	{
		for (size_t index = first; index < end; ++index)
		{
			const uint32_t id = _accessibilityItemIdsByIndex[index];
			Items[index].AccessibilityId = id;
			_accessibilityItemIndexById[id] = index;
			++_lastAccessibilityIndexUpdateWork;
		}
	};
	auto rebuild = [this]()
	{
		_accessibilityItemIdsDirty = true;
		EnsureAccessibilityItemIds();
		return false;
	};

	switch (change.Action)
	{
	case CollectionChangeAction::Add:
	{
		if (change.NewIndex == CollectionChangedEventArgs::Npos
			|| change.NewIndex > _accessibilityItemIdsByIndex.size()
			|| Items.size() < change.OldSize
			|| change.NewCount != Items.size() - change.OldSize)
			return rebuild();
		_accessibilityItemIdsByIndex.insert(
			_accessibilityItemIdsByIndex.begin()
				+ static_cast<ptrdiff_t>(change.NewIndex),
			change.NewCount, 0);
		const size_t addedEnd = change.NewIndex + change.NewCount;
		for (size_t index = change.NewIndex; index < addedEnd; ++index)
		{
			repairItemId(index);
			if (Items[index].Selected)
				_selectedItemIds.insert(Items[index].AccessibilityId);
			++_lastAccessibilityIndexUpdateWork;
		}
		updateRange(addedEnd, Items.size());
		break;
	}
	case CollectionChangeAction::Remove:
	{
		if (change.OldIndex == CollectionChangedEventArgs::Npos
			|| change.OldIndex > _accessibilityItemIdsByIndex.size()
			|| change.OldCount > _accessibilityItemIdsByIndex.size()
				- change.OldIndex
			|| change.NewSize != Items.size()) return rebuild();
		const auto first = _accessibilityItemIdsByIndex.begin()
			+ static_cast<ptrdiff_t>(change.OldIndex);
		const auto last = first + static_cast<ptrdiff_t>(change.OldCount);
		for (auto current = first; current != last; ++current)
		{
			_accessibilityItemIndexById.erase(*current);
			_selectedItemIds.erase(*current);
			++_lastAccessibilityIndexUpdateWork;
		}
		_accessibilityItemIdsByIndex.erase(first, last);
		updateRange(change.OldIndex, Items.size());
		break;
	}
	case CollectionChangeAction::Move:
	{
		if (change.OldIndex >= _accessibilityItemIdsByIndex.size()
			|| change.NewIndex >= _accessibilityItemIdsByIndex.size())
			return rebuild();
		if (change.OldIndex < change.NewIndex)
			std::rotate(
				_accessibilityItemIdsByIndex.begin() + change.OldIndex,
				_accessibilityItemIdsByIndex.begin() + change.OldIndex + 1,
				_accessibilityItemIdsByIndex.begin() + change.NewIndex + 1);
		else if (change.NewIndex < change.OldIndex)
			std::rotate(
				_accessibilityItemIdsByIndex.begin() + change.NewIndex,
				_accessibilityItemIdsByIndex.begin() + change.OldIndex,
				_accessibilityItemIdsByIndex.begin() + change.OldIndex + 1);
		const size_t first = (std::min)(change.OldIndex, change.NewIndex);
		const size_t end = (std::max)(change.OldIndex, change.NewIndex) + 1;
		updateRange(first, end);
		break;
	}
	case CollectionChangeAction::Swap:
	{
		if (change.OldIndex >= _accessibilityItemIdsByIndex.size()
			|| change.NewIndex >= _accessibilityItemIdsByIndex.size())
			return rebuild();
		std::swap(_accessibilityItemIdsByIndex[change.OldIndex],
			_accessibilityItemIdsByIndex[change.NewIndex]);
		updateRange(change.OldIndex, change.OldIndex + 1);
		if (change.NewIndex != change.OldIndex)
			updateRange(change.NewIndex, change.NewIndex + 1);
		break;
	}
	case CollectionChangeAction::Replace:
	{
		if (change.NewIndex >= Items.size()
			|| change.NewIndex >= _accessibilityItemIdsByIndex.size())
			return rebuild();
		const uint32_t previousId =
			_accessibilityItemIdsByIndex[change.NewIndex];
		_accessibilityItemIndexById.erase(previousId);
		_selectedItemIds.erase(previousId);
		repairItemId(change.NewIndex);
		if (Items[change.NewIndex].Selected)
			_selectedItemIds.insert(Items[change.NewIndex].AccessibilityId);
		_lastAccessibilityIndexUpdateWork = 1;
		break;
	}
	case CollectionChangeAction::Reset:
	default:
		return rebuild();
	}

	_accessibilityItemIdsDirty = false;
	PruneAccessibilityCellsForMissingItems();
	return true;
}

void ListView::PruneAccessibilityCellsForMissingItems() const
{
	for (auto current = _accessibilityCellIds.begin();
		current != _accessibilityCellIds.end();)
	{
		const uint32_t rowId = static_cast<uint32_t>(current->first >> 32);
		if (_accessibilityItemIndexById.contains(rowId))
		{
			++current;
			continue;
		}
		_accessibilityCellKeyById.erase(current->second);
		current = _accessibilityCellIds.erase(current);
	}
}

void ListView::EnsureAccessibilityDetailsIds() const
{
	EnsureAccessibilityItemIds();
	if (!_accessibilityDetailsIdsDirty) return;
	std::unordered_set<uint32_t> used;
	used.reserve(Items.size() + Columns.size()
		+ _accessibilityCellIds.size() + 1);
	for (const auto& item : Items)
		used.insert(item.AccessibilityId);

	_accessibilityColumnIndexById.clear();
	_accessibilityColumnIndexById.reserve((std::max)(size_t{ 1 }, Columns.size()));
	if (Columns.empty())
	{
		while (_accessibilityImplicitColumnId == 0
			|| !used.insert(_accessibilityImplicitColumnId).second)
			_accessibilityImplicitColumnId = AllocateAccessibilityVirtualId();
		_accessibilityColumnIndexById.emplace(_accessibilityImplicitColumnId, 0);
	}
	else
	{
		for (size_t index = 0; index < Columns.size(); ++index)
		{
			const auto& column = Columns[index];
			while (column.AccessibilityId == 0
				|| !used.insert(column.AccessibilityId).second)
				column.AccessibilityId = AllocateAccessibilityVirtualId();
			_accessibilityColumnIndexById.emplace(column.AccessibilityId, index);
		}
	}

	std::unordered_map<uint64_t, uint32_t> next;
	std::unordered_map<uint32_t, uint64_t> nextReverse;
	next.reserve(_accessibilityCellIds.size());
	nextReverse.reserve(_accessibilityCellIds.size());
	for (const auto& entry : _accessibilityCellIds)
	{
		const uint32_t rowId = static_cast<uint32_t>(entry.first >> 32);
		const uint32_t columnId = static_cast<uint32_t>(entry.first);
		if (!_accessibilityItemIndexById.contains(rowId)
			|| !_accessibilityColumnIndexById.contains(columnId)) continue;
		uint32_t id = entry.second;
		while (id == 0 || !used.insert(id).second)
			id = AllocateAccessibilityVirtualId();
		next.emplace(entry.first, id);
		nextReverse.emplace(id, entry.first);
	}
	_accessibilityCellIds.swap(next);
	_accessibilityCellKeyById.swap(nextReverse);
	_accessibilityDetailsIdsDirty = false;
}

uint32_t ListView::EnsureAccessibilityCellId(
	uint32_t rowId, uint32_t columnId) const
{
	EnsureAccessibilityDetailsIds();
	if (!_accessibilityItemIndexById.contains(rowId)
		|| !_accessibilityColumnIndexById.contains(columnId)) return 0;
	const uint64_t key = (static_cast<uint64_t>(rowId) << 32) | columnId;
	if (const auto found = _accessibilityCellIds.find(key);
		found != _accessibilityCellIds.end()) return found->second;
	uint32_t id = AllocateAccessibilityVirtualId();
	while (id == 0 || _accessibilityItemIndexById.contains(id)
		|| _accessibilityColumnIndexById.contains(id)
		|| _accessibilityCellKeyById.contains(id))
		id = AllocateAccessibilityVirtualId();
	_accessibilityCellIds.emplace(key, id);
	_accessibilityCellKeyById.emplace(id, key);
	return id;
}

int ListView::FindAccessibilityItem(uint32_t id) const
{
	if (id == 0) return -1;
	EnsureAccessibilityItemIds();
	const auto found = _accessibilityItemIndexById.find(id);
	return found == _accessibilityItemIndexById.end()
		? -1 : static_cast<int>(found->second);
}

void ListView::GetAccessibilityVirtualChildren(
	uint32_t parentId, std::vector<uint32_t>& result)
{
	result.clear();
	const bool details = !IsListBox()
		&& ViewMode == ListViewViewMode::Details;
	if (!details)
	{
		if (parentId != 0) return;
		EnsureAccessibilityItemIds();
		result.reserve(Items.size());
		for (const auto& item : Items)
			result.push_back(item.AccessibilityId);
		return;
	}

	EnsureAccessibilityDetailsIds();
	if (parentId == 0)
	{
		result.reserve(Columns.size() + Items.size() + (Columns.empty() ? 1 : 0));
		if (Columns.empty()) result.push_back(_accessibilityImplicitColumnId);
		else for (const auto& column : Columns)
			result.push_back(column.AccessibilityId);
		for (const auto& item : Items)
			result.push_back(item.AccessibilityId);
		return;
	}
	const auto rowPosition = _accessibilityItemIndexById.find(parentId);
	if (rowPosition == _accessibilityItemIndexById.end()) return;
	const auto& row = Items[rowPosition->second];
	const size_t columnCount = (std::max)(size_t{ 1 }, Columns.size());
	result.reserve(columnCount);
	for (size_t column = 0; column < columnCount; ++column)
	{
		const uint32_t columnId = Columns.empty()
			? _accessibilityImplicitColumnId : Columns[column].AccessibilityId;
		const uint64_t key = (static_cast<uint64_t>(row.AccessibilityId) << 32)
			| columnId;
		const uint32_t cell = EnsureAccessibilityCellId(
			row.AccessibilityId, columnId);
		if (cell != 0) result.push_back(cell);
	}
}

size_t ListView::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	const bool details = !IsListBox() && ViewMode == ListViewViewMode::Details;
	if (!details) return parentId == 0 ? Items.size() : 0;
	EnsureAccessibilityDetailsIds();
	const size_t columnCount = (std::max)(size_t{ 1 }, Columns.size());
	if (parentId == 0) return columnCount + Items.size();
	return _accessibilityItemIndexById.contains(parentId) ? columnCount : 0;
}

bool ListView::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	const bool details = !IsListBox() && ViewMode == ListViewViewMode::Details;
	if (!details)
	{
		if (parentId != 0 || index >= Items.size()) return false;
		EnsureAccessibilityItemIds();
		result = Items[index].AccessibilityId;
		return result != 0;
	}

	EnsureAccessibilityDetailsIds();
	const size_t columnCount = (std::max)(size_t{ 1 }, Columns.size());
	if (parentId == 0)
	{
		if (index < columnCount)
			result = Columns.empty() ? _accessibilityImplicitColumnId
				: Columns[index].AccessibilityId;
		else
		{
			const size_t row = index - columnCount;
			if (row >= Items.size()) return false;
			result = Items[row].AccessibilityId;
		}
		return result != 0;
	}
	if (index >= columnCount
		|| !_accessibilityItemIndexById.contains(parentId)) return false;
	const uint32_t columnId = Columns.empty() ? _accessibilityImplicitColumnId
		: Columns[index].AccessibilityId;
	const uint64_t key = (static_cast<uint64_t>(parentId) << 32) | columnId;
	result = EnsureAccessibilityCellId(parentId, columnId);
	return result != 0;
}

bool ListView::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	result = 0;
	const bool details = !IsListBox() && ViewMode == ListViewViewMode::Details;
	if (!details)
	{
		if (parentId != 0) return false;
		const int index = FindAccessibilityItem(id);
		const int sibling = next ? index + 1 : index - 1;
		return index >= 0 && sibling >= 0
			&& TryGetAccessibilityVirtualChildAt(
				0, static_cast<size_t>(sibling), result);
	}

	EnsureAccessibilityDetailsIds();
	const size_t columnCount = (std::max)(size_t{ 1 }, Columns.size());
	size_t index = 0;
	if (parentId == 0)
	{
		if (const auto header = _accessibilityColumnIndexById.find(id);
			header != _accessibilityColumnIndexById.end())
			index = header->second;
		else if (const auto row = _accessibilityItemIndexById.find(id);
			row != _accessibilityItemIndexById.end())
			index = columnCount + row->second;
		else return false;
	}
	else
	{
		const auto cell = _accessibilityCellKeyById.find(id);
		if (cell == _accessibilityCellKeyById.end()
			|| static_cast<uint32_t>(cell->second >> 32) != parentId) return false;
		const uint32_t columnId = static_cast<uint32_t>(cell->second);
		const auto column = _accessibilityColumnIndexById.find(columnId);
		if (column == _accessibilityColumnIndexById.end()) return false;
		index = column->second;
	}
	if (!next && index == 0) return false;
	const size_t sibling = next ? index + 1 : index - 1;
	return sibling < GetAccessibilityVirtualChildCount(parentId)
		&& TryGetAccessibilityVirtualChildAt(parentId, sibling, result);
}

bool ListView::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	const auto layout = CalcLayout();
	const bool details = !IsListBox() && ViewMode == ListViewViewMode::Details;
	if (details && ShowColumnHeaders && PtInRectF(layout.HeaderRect, localX, localY))
	{
		EnsureAccessibilityDetailsIds();
		if (Columns.empty())
		{
			result = _accessibilityImplicitColumnId;
			return result != 0;
		}
		float left = layout.HeaderRect.left;
		for (size_t column = 0; column < Columns.size(); ++column)
		{
			const float right = left + (std::max)(16.0f, Columns[column].Width);
			if (localX >= left && localX < right)
			{
				result = Columns[column].AccessibilityId;
				return result != 0;
			}
			left = right;
		}
		return false;
	}
	if (!PtInRectF(layout.ContentRect, localX, localY)) return false;
	const int row = HitTestItem(
		static_cast<int>(std::floor(localX)), static_cast<int>(std::floor(localY)));
	if (row < 0 || row >= static_cast<int>(Items.size())) return false;
	EnsureAccessibilityItemIds();
	if (!details)
	{
		result = Items[static_cast<size_t>(row)].AccessibilityId;
		return result != 0;
	}

	EnsureAccessibilityDetailsIds();
	const uint32_t rowId = Items[static_cast<size_t>(row)].AccessibilityId;
	const size_t columnCount = (std::max)(size_t{ 1 }, Columns.size());
	float left = layout.ContentRect.left;
	for (size_t column = 0; column < columnCount; ++column)
	{
		const float width = Columns.empty() ? RectWidth(layout.ContentRect)
			: (std::max)(16.0f, Columns[column].Width);
		if (localX >= left && localX < left + width)
		{
			const uint32_t columnId = Columns.empty()
				? _accessibilityImplicitColumnId : Columns[column].AccessibilityId;
			result = EnsureAccessibilityCellId(rowId, columnId);
			return result != 0;
		}
		left += width;
	}
	result = rowId;
	return result != 0;
}

bool ListView::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	const bool details = !IsListBox()
		&& ViewMode == ListViewViewMode::Details;
	if (details)
	{
		EnsureAccessibilityDetailsIds();
		const auto layout = CalcLayout();
		const auto ownerId = GetAccessibilitySnapshot().AutomationId;
		auto automationId = [&](const wchar_t* kind)
		{
			const std::wstring suffix = std::wstring(kind) + L"-" + std::to_wstring(id);
			return ownerId.empty() ? suffix : ownerId + L"." + suffix;
		};
		const size_t columnCount = (std::max)(size_t{ 1 }, Columns.size());
		if (const auto header = _accessibilityColumnIndexById.find(id);
			header != _accessibilityColumnIndexById.end())
		{
			const size_t column = header->second;
			const uint32_t columnId = Columns.empty()
				? _accessibilityImplicitColumnId : Columns[column].AccessibilityId;
			if (columnId != id) return false;
			float left = layout.HeaderRect.left;
			for (size_t index = 0; index < column; ++index)
				left += Columns.empty() ? RectWidth(layout.HeaderRect)
					: (std::max)(16.0f, Columns[index].Width);
			const float width = Columns.empty() ? RectWidth(layout.HeaderRect)
				: (std::max)(16.0f, Columns[column].Width);
			result = {};
			result.Id = id;
			result.Role = AccessibleRole::HeaderItem;
			result.Name = Columns.empty() ? static_cast<std::wstring>(Text)
				: Columns[column].Header;
			result.AutomationId = automationId(L"header");
			result.BoundsDip = D2D1::RectF(
				left, layout.HeaderRect.top, left + width, layout.HeaderRect.bottom);
			result.Enabled = Enable;
			result.Visible = Visible && ShowColumnHeaders
				&& RectHeight(layout.HeaderRect) > 0.0f
				&& left + width > layout.HeaderRect.left
				&& left < layout.HeaderRect.right;
			result.Column = static_cast<int>(column);
			result.Level = 1;
			return true;
		}

		if (const auto rowPosition = _accessibilityItemIndexById.find(id);
			rowPosition != _accessibilityItemIndexById.end())
		{
			const int row = static_cast<int>(rowPosition->second);
			const auto& item = Items[rowPosition->second];
			const auto rowBounds = GetItemRect(row, layout);
			const bool rowVisible = Visible
				&& rowBounds.bottom > layout.ContentRect.top
				&& rowBounds.top < layout.ContentRect.bottom;
			result = {};
			result.Id = id;
			result.Role = AccessibleRole::DataItem;
			result.Patterns = AccessibilityVirtualPattern::SelectionItem
				| AccessibilityVirtualPattern::ScrollItem
				| AccessibilityVirtualPattern::VirtualizedItem;
			if (ShowCheckBoxes)
				result.Patterns |= AccessibilityVirtualPattern::Toggle;
			result.Name = item.Text;
			result.Description = item.SubText;
			result.AutomationId = automationId(L"row");
			result.BoundsDip = rowBounds;
			result.Enabled = Enable && item.Enabled;
			result.Visible = rowVisible;
			result.Selected = item.Selected;
			result.Checked = item.Checked;
			result.Row = row;
			result.Column = -1;
			result.Level = 1;
			return true;
		}

		const auto cellPosition = _accessibilityCellKeyById.find(id);
		if (cellPosition == _accessibilityCellKeyById.end()) return false;
		const uint32_t rowId = static_cast<uint32_t>(cellPosition->second >> 32);
		const uint32_t columnId = static_cast<uint32_t>(cellPosition->second);
		const auto rowPosition = _accessibilityItemIndexById.find(rowId);
		const auto columnPosition = _accessibilityColumnIndexById.find(columnId);
		if (rowPosition == _accessibilityItemIndexById.end()
			|| columnPosition == _accessibilityColumnIndexById.end()) return false;
		const int row = static_cast<int>(rowPosition->second);
		const size_t column = columnPosition->second;
		const auto& item = Items[rowPosition->second];
		const auto rowBounds = GetItemRect(row, layout);
		const bool rowVisible = Visible
			&& rowBounds.bottom > layout.ContentRect.top
			&& rowBounds.top < layout.ContentRect.bottom;
		float left = layout.ContentRect.left;
		for (size_t index = 0; index < column; ++index)
			left += Columns.empty() ? RectWidth(layout.ContentRect)
				: (std::max)(16.0f, Columns[index].Width);
		const float width = Columns.empty() ? RectWidth(layout.ContentRect)
			: (std::max)(16.0f, Columns[column].Width);
		std::wstring text;
		if (column == 0) text = item.Text;
		else if (column - 1 < item.SubItems.size()) text = item.SubItems[column - 1];
		else if (column == 1) text = item.SubText;
		result = {};
		result.Id = id;
		result.ParentId = rowId;
		result.Role = AccessibleRole::DataItem;
		result.Patterns = AccessibilityVirtualPattern::ScrollItem
			| AccessibilityVirtualPattern::VirtualizedItem
			| AccessibilityVirtualPattern::GridItem
			| AccessibilityVirtualPattern::TableItem;
		result.Name = text;
		result.Description = Columns.empty()
			? static_cast<std::wstring>(Text) : Columns[column].Header;
		result.Value = text;
		result.AutomationId = automationId(L"cell");
		result.BoundsDip = D2D1::RectF(
			left, rowBounds.top, left + width, rowBounds.bottom);
		result.Enabled = Enable && item.Enabled;
		result.Visible = rowVisible && left + width > layout.ContentRect.left
			&& left < layout.ContentRect.right;
		result.Selected = item.Selected;
		result.Row = row;
		result.Column = static_cast<int>(column);
		result.Level = 2;
		return true;
	}

	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	const auto& item = Items[static_cast<size_t>(index)];
	const auto layout = CalcLayout();
	const auto bounds = GetItemRect(index, layout);
	result = {};
	result.Id = id;
	result.Role = AccessibleRole::ListItem;
	result.Patterns = AccessibilityVirtualPattern::SelectionItem
		| AccessibilityVirtualPattern::ScrollItem
		| AccessibilityVirtualPattern::VirtualizedItem;
	if (ShowCheckBoxes)
		result.Patterns |= AccessibilityVirtualPattern::Toggle;
	result.Name = item.Text;
	result.Description = item.SubText;
	result.Value = item.SubText;
	const auto ownerId = GetAccessibilitySnapshot().AutomationId;
	result.AutomationId = ownerId.empty()
		? L"item-" + std::to_wstring(id)
		: ownerId + L".item-" + std::to_wstring(id);
	result.BoundsDip = bounds;
	result.Enabled = Enable && item.Enabled;
	result.Visible = Visible
		&& bounds.right > layout.ContentRect.left
		&& bounds.left < layout.ContentRect.right
		&& bounds.bottom > layout.ContentRect.top
		&& bounds.top < layout.ContentRect.bottom;
	result.Selected = item.Selected;
	result.Checked = item.Checked;
	result.Row = index;
	result.Column = 0;
	return true;
}

AccessibilityVirtualContainerInfo
ListView::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AccessibilityVirtualPattern::Selection
		| AccessibilityVirtualPattern::Scroll;
	if (!IsListBox()
		&& static_cast<ListViewViewMode>(_viewMode) == ListViewViewMode::Details)
		result.Patterns |= AccessibilityVirtualPattern::Grid
			| AccessibilityVirtualPattern::Table;
	result.CanSelectMultiple = static_cast<ListViewSelectionMode>(_selectionMode)
		== ListViewSelectionMode::Multiple;
	result.IsSelectionRequired = false;
	result.RowCount = static_cast<int>(Items.size());
	result.ColumnCount = !IsListBox()
		&& static_cast<ListViewViewMode>(_viewMode) == ListViewViewMode::Details
		? static_cast<int>((std::max)(size_t{ 1 }, Columns.size())) : 1;
	return result;
}

void ListView::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
	EnsureAccessibilityItemIds();
	for (const auto& item : Items)
	{
		if (item.Selected) result.push_back(item.AccessibilityId);
	}
}

bool ListView::GetAccessibilityVirtualItemAt(
	int row, int column, uint32_t& id)
{
	id = 0;
	if (IsListBox() || ViewMode != ListViewViewMode::Details
		|| row < 0 || column < 0
		|| row >= static_cast<int>(Items.size())
		|| column >= static_cast<int>((std::max)(size_t{ 1 }, Columns.size())))
		return false;
	EnsureAccessibilityDetailsIds();
	const auto& item = Items[static_cast<size_t>(row)];
	const uint32_t columnId = Columns.empty()
		? _accessibilityImplicitColumnId
		: Columns[static_cast<size_t>(column)].AccessibilityId;
	id = EnsureAccessibilityCellId(item.AccessibilityId, columnId);
	return id != 0;
}

void ListView::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result)
{
	result.clear();
	if (IsListBox() || ViewMode != ListViewViewMode::Details) return;
	EnsureAccessibilityDetailsIds();
	if (Columns.empty()) result.push_back(_accessibilityImplicitColumnId);
	else
	{
		result.reserve(Columns.size());
		for (const auto& column : Columns)
			result.push_back(column.AccessibilityId);
	}
}

bool ListView::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0 || !Enable || !Items[static_cast<size_t>(index)].Enabled)
		return false;
	if (action == AccessibilitySelectionAction::Remove)
	{
		auto& item = Items[static_cast<size_t>(index)];
		if (!item.Selected) return true;
		item.Selected = false;
		SyncSelectedIndexFromItems();
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
		InvalidateVisual();
		return true;
	}
	auto& item = Items[static_cast<size_t>(index)];
	if (action == AccessibilitySelectionAction::Add && item.Selected)
		return true;
	const bool additive = action == AccessibilitySelectionAction::Add
		&& SelectionMode == ListViewSelectionMode::Multiple;
	const bool changed = SelectItem(index, additive, false);
	if (changed)
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
	return item.Selected;
}

bool ListView::ToggleAccessibilityVirtualNode(uint32_t id)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0 || !ShowCheckBoxes || !Enable
		|| !Items[static_cast<size_t>(index)].Enabled) return false;
	ToggleCheckAt(index);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Toggle);
	return true;
}

bool ListView::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	int index = FindAccessibilityItem(id);
	if (index < 0 && !IsListBox() && ViewMode == ListViewViewMode::Details)
	{
		EnsureAccessibilityDetailsIds();
		const auto cell = _accessibilityCellKeyById.find(id);
		if (cell != _accessibilityCellKeyById.end())
		{
			const uint32_t rowId = static_cast<uint32_t>(cell->second >> 32);
			const auto row = _accessibilityItemIndexById.find(rowId);
			if (row != _accessibilityItemIndexById.end())
				index = static_cast<int>(row->second);
		}
	}
	if (index < 0) return false;
	EnsureVisible(index);
	return true;
}

bool ListView::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	const auto layout = CalcLayout();
	result = {};
	result.VerticallyScrollable = layout.MaxScrollY > 0.0f;
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent = (std::clamp)(
			static_cast<double>(_scrollYOffset / layout.MaxScrollY) * 100.0,
			0.0, 100.0);
		result.VerticalViewSize = layout.ContentHeight > 0.0f
			? (std::clamp)(static_cast<double>(RectHeight(layout.ContentRect)
				/ layout.ContentHeight) * 100.0, 0.0, 100.0)
			: 100.0;
	}
	return true;
}

bool ListView::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	if (horizontal != AccessibilityScrollAmount::NoAmount) return false;
	const auto layout = CalcLayout();
	if (vertical == AccessibilityScrollAmount::NoAmount) return true;
	if (layout.MaxScrollY <= 0.0f) return false;
	float delta = 0.0f;
	switch (vertical)
	{
	case AccessibilityScrollAmount::LargeDecrement:
		delta = -RectHeight(layout.ContentRect); break;
	case AccessibilityScrollAmount::SmallDecrement:
		delta = -GetItemSecondaryExtent(); break;
	case AccessibilityScrollAmount::LargeIncrement:
		delta = RectHeight(layout.ContentRect); break;
	case AccessibilityScrollAmount::SmallIncrement:
		delta = GetItemSecondaryExtent(); break;
	case AccessibilityScrollAmount::NoAmount: return true;
	}
	SetScrollOffset(ScrollYOffset + delta);
	return true;
}

bool ListView::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	if (horizontalPercent != AccessibilityScrollNoChange) return false;
	if (verticalPercent == AccessibilityScrollNoChange) return true;
	if (!std::isfinite(verticalPercent)
		|| verticalPercent < 0.0 || verticalPercent > 100.0) return false;
	const auto layout = CalcLayout();
	if (layout.MaxScrollY <= 0.0f) return false;
	SetScrollOffset(static_cast<float>(
		layout.MaxScrollY * verticalPercent / 100.0));
	return true;
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
	if (this->MouseWheelStep <= 0) return false;
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
	const bool details = static_cast<ListViewViewMode>(_viewMode)
		== ListViewViewMode::Details && !IsListBox();
	const float headerH = (details && _showColumnHeaders)
		? (std::max)(0.0f, _headerHeight) : 0.0f;
	layout.HeaderRect = D2D1::RectF(0.0f, 0.0f, width, std::min(height, headerH));
	layout.ContentRect = D2D1::RectF(0.0f, headerH, width, height);
	layout.ScrollBarSize = (std::max)(6.0f, _scrollBarSize);

	const float availableWidth = (std::max)(0.0f, width - _border * 2.0f);
	if (static_cast<ListViewViewMode>(_viewMode) == ListViewViewMode::Icon
		&& !IsListBox())
	{
		layout.ColumnsPerRow = (std::max)(1, (int)std::floor(
			(std::max)(1.0f, availableWidth)
			/ (std::max)(1.0f, _iconItemWidth + _itemGap)));
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
			thumbTop += (_scrollYOffset / layout.MaxScrollY) * (trackH - thumbH);
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
	return (std::max)(_rowHeight, fontHeight + 10.0f);
}

float ListView::GetItemPrimaryExtent() const
{
	if (static_cast<ListViewViewMode>(_viewMode) == ListViewViewMode::Icon
		&& !IsListBox())
		return (std::max)(48.0f, _iconItemWidth);
	return (float)this->_size.cx;
}

float ListView::GetItemSecondaryExtent() const
{
	if (IsListBox()) return GetEffectiveRowHeight();
	switch (static_cast<ListViewViewMode>(_viewMode))
	{
	case ListViewViewMode::Tile:
		return (std::max)(_tileHeight, _iconSize + 16.0f);
	case ListViewViewMode::Icon:
		return (std::max)(_iconItemHeight, _iconSize + 34.0f);
	case ListViewViewMode::Details:
	case ListViewViewMode::List:
	default:
		return GetEffectiveRowHeight();
	}
}

void ListView::GetVisibleItemRange(
	const Layout& layout, int& start, int& end) const noexcept
{
	start = 0;
	end = 0;
	const size_t cappedItemCount = (std::min)(
		Items.size(), static_cast<size_t>((std::numeric_limits<int>::max)()));
	const int itemCount = static_cast<int>(cappedItemCount);
	const double viewportHeight = static_cast<double>(
		(std::max)(0.0f, RectHeight(layout.ContentRect)));
	const double itemExtent = static_cast<double>(GetItemSecondaryExtent());
	if (itemCount == 0 || viewportHeight <= 0.0
		|| !std::isfinite(itemExtent) || itemExtent <= 0.0) return;

	const bool icon = static_cast<ListViewViewMode>(_viewMode)
		== ListViewViewMode::Icon && !IsListBox();
	const int columns = icon ? (std::max)(1, layout.ColumnsPerRow) : 1;
	const int64_t totalRows = icon
		? (static_cast<int64_t>(itemCount) + columns - 1) / columns
		: itemCount;
	const double maxScroll = std::isfinite(layout.MaxScrollY)
		? (std::max)(0.0, static_cast<double>(layout.MaxScrollY))
		: (std::numeric_limits<double>::max)();
	const double proposedScroll = std::isfinite(_scrollYOffset)
		? static_cast<double>(_scrollYOffset) : 0.0;
	const double scroll = (std::clamp)(proposedScroll, 0.0, maxScroll);
	const double firstRowValue = (std::clamp)(
		std::floor(scroll / itemExtent), 0.0, static_cast<double>(totalRows));
	const double endRowValue = (std::clamp)(
		std::ceil((scroll + viewportHeight) / itemExtent),
		firstRowValue, static_cast<double>(totalRows));
	const int64_t firstRow = static_cast<int64_t>(firstRowValue);
	const int64_t endRow = static_cast<int64_t>(endRowValue);
	start = static_cast<int>((std::min)(
		static_cast<int64_t>(itemCount), firstRow * columns));
	end = static_cast<int>((std::min)(
		static_cast<int64_t>(itemCount), endRow * columns));
}

D2D1_RECT_F ListView::GetItemRect(int index, const Layout& layout) const
{
	if (index < 0 || index >= (int)this->Items.size()) return D2D1::RectF();
	if (static_cast<ListViewViewMode>(_viewMode) == ListViewViewMode::Icon
		&& !IsListBox())
	{
		const int col = index % layout.ColumnsPerRow;
		const int row = index / layout.ColumnsPerRow;
		const float itemW = GetItemPrimaryExtent();
		const float itemH = GetItemSecondaryExtent();
		const float left = layout.ContentRect.left + (float)col * itemW + _itemGap * 0.5f;
		const float top = layout.ContentRect.top + (float)row * itemH - _scrollYOffset + _itemGap * 0.5f;
		return D2D1::RectF(left, top, left + itemW - _itemGap, top + itemH - _itemGap);
	}

	const float itemH = GetItemSecondaryExtent();
	const float top = layout.ContentRect.top + (float)index * itemH - _scrollYOffset;
	return D2D1::RectF(layout.ContentRect.left, top, layout.ContentRect.right, top + itemH);
}

D2D1_RECT_F ListView::GetCheckRect(const D2D1_RECT_F& itemRect) const
{
	const float size = (std::max)(10.0f, _checkBoxSize);
	const float x = itemRect.left + _itemPaddingX;
	const float y = itemRect.top + std::max(0.0f, (RectHeight(itemRect) - size) * 0.5f);
	return D2D1::RectF(x, y, x + size, y + size);
}

void ListView::ClampScroll(Layout& layout)
{
	const float clamped = (std::clamp)(
		this->ScrollYOffset, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 1e-6f)
		SetCurrentScrollYOffset(clamped);
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
	int start = 0;
	int end = 0;
	GetVisibleItemRange(layout, start, end);
	for (int i = start; i < end; ++i)
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
	const bool drawSelection = ShouldDrawSelection(item);
	D2D1_RECT_F itemRect = D2D1::RectF(rect.left + 3.0f, rect.top + this->ItemPaddingY, rect.right - 3.0f, rect.bottom - this->ItemPaddingY);
	if (drawSelection)
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

	D2D1_COLOR_F color = item.Enabled ? (drawSelection ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
	D2D1_RECT_F textRect = D2D1::RectF(x, rect.top, rect.right - this->ItemPaddingX, rect.bottom);
	d2d->PushDrawRect(textRect.left, textRect.top, std::max(1.0f, RectWidth(textRect)), RectHeight(textRect));
	d2d->DrawString(item.Text, textRect.left, TextTop(this->Font, textRect), color, this->Font);
	d2d->PopDrawRect();
}

void ListView::DrawDetailsItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect)
{
	if (!d2d || index < 0 || index >= (int)this->Items.size()) return;
	auto& item = this->Items[index];
	const bool drawSelection = ShouldDrawSelection(item);
	D2D1_RECT_F itemRect = D2D1::RectF(rect.left + 3.0f, rect.top + this->ItemPaddingY, rect.right - 3.0f, rect.bottom - this->ItemPaddingY);
	if (drawSelection)
	{
		D2D1_RECT_F selectionRect = itemRect;
		if (!this->FullRowSelect)
		{
			const float firstColumnWidth = this->Columns.empty()
				? RectWidth(rect)
				: (std::max)(16.0f, this->Columns.front().Width);
			selectionRect.right = (std::min)(
				selectionRect.right, rect.left + firstColumnWidth - 3.0f);
		}
		if (selectionRect.right > selectionRect.left)
			d2d->FillRoundRect(selectionRect, this->SelectedItemBackColor, this->CornerRadius);
	}
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

		const bool selectedCell = drawSelection && (this->FullRowSelect || col == 0);
		D2D1_COLOR_F color = item.Enabled ? (selectedCell ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
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
	const bool drawSelection = ShouldDrawSelection(item);
	D2D1_RECT_F itemRect = D2D1::RectF(rect.left + 4.0f, rect.top + 4.0f, rect.right - 4.0f, rect.bottom - 4.0f);
	if (drawSelection)
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

	D2D1_COLOR_F color = item.Enabled ? (drawSelection ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
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
	const bool drawSelection = ShouldDrawSelection(item);
	if (drawSelection)
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

	D2D1_COLOR_F color = item.Enabled ? (drawSelection ? this->SelectedItemForeColor : this->ForeColor) : this->DisabledItemForeColor;
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
	SetCurrentHoveredIndex(HitTestItem(localX, localY));
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

bool ListView::SetCachedItemSelected(size_t index, bool selected)
{
	if (index >= Items.size()) return false;
	EnsureAccessibilityItemIds();
	++_lastSelectionUpdateWork;
	auto& item = Items[index];
	const bool changed = item.Selected != selected;
	item.Selected = selected;
	if (selected)
		_selectedItemIds.insert(item.AccessibilityId);
	else
		_selectedItemIds.erase(item.AccessibilityId);
	return changed;
}

bool ListView::ClearCachedSelection()
{
	bool changed = false;
	for (const uint32_t id : _selectedItemIds)
	{
		++_lastSelectionUpdateWork;
		const auto found = _accessibilityItemIndexById.find(id);
		if (found == _accessibilityItemIndexById.end()
			|| found->second >= Items.size()) continue;
		auto& item = Items[found->second];
		changed = item.Selected || changed;
		item.Selected = false;
	}
	_selectedItemIds.clear();
	return changed;
}

int ListView::FindFirstCachedSelectedIndex() const
{
	size_t first = Items.size();
	for (const uint32_t id : _selectedItemIds)
	{
		++_lastSelectionUpdateWork;
		const auto found = _accessibilityItemIndexById.find(id);
		if (found != _accessibilityItemIndexById.end())
			first = (std::min)(first, found->second);
	}
	return first < Items.size()
		&& first <= static_cast<size_t>((std::numeric_limits<int>::max)())
		? static_cast<int>(first) : -1;
}

void ListView::SyncSelectedIndexFromItems(bool raiseEvent)
{
	EnsureAccessibilityItemIds();
	_selectedItemIds.clear();
	_lastSelectionUpdateWork = 0;
	int selectedIndex = -1;
	bool normalized = false;
	for (int index = 0; index < static_cast<int>(this->Items.size()); ++index)
	{
		++_lastSelectionUpdateWork;
		if (!this->Items[static_cast<size_t>(index)].Selected) continue;
		if (selectedIndex < 0)
		{
			selectedIndex = index;
			_selectedItemIds.insert(
				Items[static_cast<size_t>(index)].AccessibilityId);
			continue;
		}
		if (this->SelectionMode == ListViewSelectionMode::Single)
		{
			this->Items[static_cast<size_t>(index)].Selected = false;
			normalized = true;
		}
		else
			_selectedItemIds.insert(
				Items[static_cast<size_t>(index)].AccessibilityId);
	}
	const int focusedIndex = this->FocusedIndex >= 0
		&& this->FocusedIndex < static_cast<int>(this->Items.size())
		? this->FocusedIndex
		: selectedIndex;
	CommitPreparedSelection(
		selectedIndex, focusedIndex, raiseEvent || normalized);
}

void ListView::ApplySelectedIndexChange(int oldValue, int newValue)
{
	if (oldValue == newValue) return;
	if (!_selectionItemsPrepared)
		_selectedIndexFollowsItems = false;
	if (!_selectionItemsPrepared)
	{
		EnsureAccessibilityItemIds();
		_lastSelectionUpdateWork = 0;
		const uint32_t wantedId = newValue >= 0
			&& newValue < static_cast<int>(Items.size())
			? Items[static_cast<size_t>(newValue)].AccessibilityId : 0;
		if ((wantedId == 0 && !_selectedItemIds.empty())
			|| (wantedId != 0 && (_selectedItemIds.size() != 1
				|| !_selectedItemIds.contains(wantedId))))
		{
			ClearCachedSelection();
			if (wantedId != 0)
				SetCachedItemSelected(static_cast<size_t>(newValue), true);
		}
		_anchorIndex = newValue >= 0
			&& newValue < static_cast<int>(this->Items.size())
			? newValue : -1;
		SetCurrentFocusedIndex(_anchorIndex);
	}
	if (newValue >= 0 && newValue < static_cast<int>(this->Items.size()))
		EnsureVisible(newValue);
	SelectionChanged(this);
}

void ListView::NormalizeSelectionForMode()
{
	if (this->SelectionMode != ListViewSelectionMode::Single)
	{
		InvalidateVisual();
		return;
	}
	EnsureAccessibilityItemIds();
	_lastSelectionUpdateWork = 0;
	int selectedIndex = this->SelectedIndex;
	if (selectedIndex < 0
		|| selectedIndex >= static_cast<int>(this->Items.size())
		|| !_selectedItemIds.contains(
			Items[static_cast<size_t>(selectedIndex)].AccessibilityId))
		selectedIndex = FindFirstCachedSelectedIndex();
	const uint32_t selectedId = selectedIndex >= 0
		? Items[static_cast<size_t>(selectedIndex)].AccessibilityId : 0;
	const bool alreadyNormalized = selectedId == 0
		? _selectedItemIds.empty()
		: _selectedItemIds.size() == 1
			&& _selectedItemIds.contains(selectedId);
	bool changed = false;
	if (!alreadyNormalized)
	{
		changed = ClearCachedSelection();
		if (selectedId != 0)
			changed = SetCachedItemSelected(
				static_cast<size_t>(selectedIndex), true) || changed;
	}
	_anchorIndex = selectedIndex;
	CommitPreparedSelection(selectedIndex, selectedIndex, changed);
}

void ListView::CommitPreparedSelection(
	int selectedIndex,
	int focusedIndex,
	bool selectionItemsChanged)
{
	_selectedIndexFollowsItems = true;
	const bool indexChanged = this->SelectedIndex != selectedIndex;
	const bool focusChanged = this->FocusedIndex != focusedIndex;
	if (focusChanged) SetCurrentFocusedIndex(focusedIndex);
	if (indexChanged)
	{
		_selectionItemsPrepared = true;
		SetCurrentSelectedIndex(selectedIndex);
		_selectionItemsPrepared = false;
		if (this->SelectedIndex == selectedIndex)
		{
			_selectedIndexProjectionSource =
				GetPropertyValueSource(L"SelectedIndex");
			return;
		}
	}
	if (selectionItemsChanged || focusChanged)
	{
		if (focusedIndex >= 0
			&& focusedIndex < static_cast<int>(this->Items.size()))
			EnsureVisible(focusedIndex);
		SelectionChanged(this);
		InvalidateVisual();
	}
	_selectedIndexProjectionSource =
		GetPropertyValueSource(L"SelectedIndex");
}

void ListView::SetCurrentSelectedIndex(int value)
{
	if (_selectedIndex == value) return;
	(void)SetCurrentPropertyField(L"SelectedIndex", _selectedIndex, value);
}

void ListView::SetCurrentFocusedIndex(int value)
{
	if (_focusedIndex == value) return;
	(void)SetCurrentPropertyField(L"FocusedIndex", _focusedIndex, value);
}

void ListView::SetCurrentHoveredIndex(int value)
{
	if (_hoveredIndex == value) return;
	(void)SetCurrentPropertyField(L"HoveredIndex", _hoveredIndex, value);
}

void ListView::SetCurrentScrollYOffset(float value)
{
	if (std::fabs(_scrollYOffset - value) <= 1e-6f) return;
	(void)SetCurrentPropertyField(L"ScrollYOffset", _scrollYOffset, value);
}

void ListView::ClampScrollToRange()
{
	auto layout = CalcLayout();
	const float clamped = (std::clamp)(
		this->ScrollYOffset, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 1e-6f)
		SetCurrentScrollYOffset(clamped);
}

void ListView::RequestCollectionRefresh()
{
	if (_updateDepth != 0)
	{
		_updatePendingCollectionRefresh = true;
		return;
	}
	ClampScrollToRange();
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
}

void ListView::OnComputedLayoutSizeChanged()
{
	ClampScrollToRange();
	NotifyAccessibilityScrollChanged();
}

void ListView::OnItemsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	auto remapIndex = [&](int current)
	{
		if (current < 0) return -1;
		const size_t index = static_cast<size_t>(current);
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			if (change.NewIndex != CollectionChangedEventArgs::Npos
				&& index >= change.NewIndex)
				return static_cast<int>(index + change.NewCount);
			break;
		case CollectionChangeAction::Remove:
			if (change.OldIndex != CollectionChangedEventArgs::Npos)
			{
				if (index >= change.OldIndex
					&& index < change.OldIndex + change.OldCount) return -1;
				if (index >= change.OldIndex + change.OldCount)
					return static_cast<int>(index - change.OldCount);
			}
			break;
		case CollectionChangeAction::Move:
			if (change.OldIndex != CollectionChangedEventArgs::Npos
				&& change.NewIndex != CollectionChangedEventArgs::Npos)
			{
				if (index == change.OldIndex)
					return static_cast<int>(change.NewIndex);
				if (change.OldIndex < change.NewIndex
					&& index > change.OldIndex && index <= change.NewIndex)
					return current - 1;
				if (change.NewIndex < change.OldIndex
					&& index >= change.NewIndex && index < change.OldIndex)
					return current + 1;
			}
			break;
		case CollectionChangeAction::Swap:
			if (index == change.OldIndex)
				return static_cast<int>(change.NewIndex);
			if (index == change.NewIndex)
				return static_cast<int>(change.OldIndex);
			break;
		default:
			break;
		}
		return current < static_cast<int>(Items.size()) ? current : -1;
	};
	const bool identityUpdatedIncrementally =
		ApplyAccessibilityItemCollectionChange(change);
	if (!identityUpdatedIncrementally)
		PruneAccessibilityCellsForMissingItems();

	if (change.Action == CollectionChangeAction::Reset)
	{
		SetCurrentHoveredIndex(-1);
		SetCurrentFocusedIndex(-1);
		_anchorIndex = -1;
		SetCurrentScrollYOffset(0.0f);
	}
	else
	{
		SetCurrentHoveredIndex(remapIndex(HoveredIndex));
		SetCurrentFocusedIndex(remapIndex(FocusedIndex));
		_anchorIndex = remapIndex(_anchorIndex);
	}

	bool selectionItemsChanged = false;
	const auto source = GetPropertyValueSource(L"SelectedIndex");
	const bool sourceIsAuthoritative =
		source != ControlPropertyValueSource::Default
		&& (!_selectedIndexFollowsItems
			|| source != _selectedIndexProjectionSource);
	if (sourceIsAuthoritative)
	{
		const bool canRemapExistingSelection =
			change.Action != CollectionChangeAction::Reset
			&& SelectedIndex >= 0
			&& static_cast<size_t>(SelectedIndex) < change.OldSize;
		int selectedIndex = SelectedIndex;
		if (canRemapExistingSelection)
			selectedIndex = remapIndex(SelectedIndex);
		else
		{
			(void)ReevaluatePropertyValue(L"SelectedIndex");
			selectedIndex = this->SelectedIndex;
		}
		_lastSelectionUpdateWork = 0;
		if (!identityUpdatedIncrementally)
		{
			_selectedItemIds.clear();
			for (int index = 0; index < static_cast<int>(Items.size()); ++index)
			{
				++_lastSelectionUpdateWork;
				const bool selected = index == selectedIndex;
				if (Items[static_cast<size_t>(index)].Selected != selected)
				{
					Items[static_cast<size_t>(index)].Selected = selected;
					selectionItemsChanged = true;
				}
				if (selected)
					_selectedItemIds.insert(
						Items[static_cast<size_t>(index)].AccessibilityId);
			}
		}
		else
		{
			const uint32_t wantedId = selectedIndex >= 0
				&& selectedIndex < static_cast<int>(Items.size())
				? Items[static_cast<size_t>(selectedIndex)].AccessibilityId : 0;
			const bool alreadyApplied = wantedId == 0
				? _selectedItemIds.empty()
				: _selectedItemIds.size() == 1
					&& _selectedItemIds.contains(wantedId);
			if (!alreadyApplied)
			{
				selectionItemsChanged = ClearCachedSelection();
				if (wantedId != 0)
					selectionItemsChanged = SetCachedItemSelected(
						static_cast<size_t>(selectedIndex), true)
						|| selectionItemsChanged;
			}
		}
		_anchorIndex = selectedIndex >= 0
			&& selectedIndex < static_cast<int>(Items.size())
			? selectedIndex : -1;
		const int focusedIndex = FocusedIndex >= 0 ? FocusedIndex : _anchorIndex;
		CommitPreparedSelection(
			selectedIndex, focusedIndex, selectionItemsChanged);
		_selectedIndexFollowsItems = false;
	}
	else
	{
		if (!identityUpdatedIncrementally)
			SyncSelectedIndexFromItems(selectionItemsChanged);
		else
		{
			_lastSelectionUpdateWork = 0;
			int selectedIndex = -1;
			if (change.Action == CollectionChangeAction::Add
				&& change.NewIndex != CollectionChangedEventArgs::Npos)
			{
				int firstAddedSelection = -1;
				int lastAddedSelection = -1;
				const size_t end = (std::min)(Items.size(),
					change.NewIndex + change.NewCount);
				for (size_t index = change.NewIndex; index < end; ++index)
				{
					++_lastSelectionUpdateWork;
					if (!Items[index].Selected) continue;
					if (firstAddedSelection < 0)
						firstAddedSelection = static_cast<int>(index);
					lastAddedSelection = static_cast<int>(index);
				}
				if (SelectionMode == ListViewSelectionMode::Single
					&& lastAddedSelection >= 0)
				{
					selectionItemsChanged = ClearCachedSelection();
					selectionItemsChanged = SetCachedItemSelected(
						static_cast<size_t>(lastAddedSelection), true)
						|| selectionItemsChanged;
					selectedIndex = lastAddedSelection;
				}
				else
				{
					selectedIndex = remapIndex(SelectedIndex);
					if (SelectionMode == ListViewSelectionMode::Multiple
						&& firstAddedSelection >= 0
						&& (selectedIndex < 0
							|| firstAddedSelection < selectedIndex))
						selectedIndex = firstAddedSelection;
				}
			}
			else
			{
				selectedIndex = FindFirstCachedSelectedIndex();
				if (SelectionMode == ListViewSelectionMode::Single
					&& _selectedItemIds.size() > 1)
				{
					selectionItemsChanged = ClearCachedSelection();
					if (selectedIndex >= 0)
						selectionItemsChanged = SetCachedItemSelected(
							static_cast<size_t>(selectedIndex), true)
							|| selectionItemsChanged;
				}
			}
			const int focusedIndex = FocusedIndex >= 0
				&& FocusedIndex < static_cast<int>(Items.size())
				? FocusedIndex : selectedIndex;
			CommitPreparedSelection(
				selectedIndex, focusedIndex, selectionItemsChanged);
		}
		if (_anchorIndex < 0
			|| _anchorIndex >= static_cast<int>(Items.size()))
			_anchorIndex = SelectedIndex;
	}

	RequestCollectionRefresh();
}

void ListView::OnColumnsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	(void)change;
	_accessibilityDetailsIdsDirty = true;
	RequestCollectionRefresh();
}

bool ListView::ShouldDrawSelection(const ListViewItem& item) const
{
	if (!item.Selected) return false;
	return !_hideSelectionWhenLostFocus
		|| HasControlStyleState(this->GetStyleState(), ControlStyleState::Focused);
}

void ListView::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	float width = size.width;
	float height = size.height;
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
		if (delta != 0 && this->MouseWheelStep > 0)
		{
			const float step = static_cast<float>(this->MouseWheelStep);
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
	InitializeListBoxDefaults();
}

void ListView::InitializeListBoxDefaults() noexcept
{
	_viewMode = static_cast<int>(ListViewViewMode::List);
	_showColumnHeaders = false;
}

void ListBox::EnsureBindingPropertiesRegistered()
{
	ListView::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		ControlPropertyOptions<ListBox, bool> options;
		options.DefaultValue = false;
		options.Flags = ControlPropertyFlags::AffectsRender
			| ControlPropertyFlags::TracksLocalValue;
		options.Coerce = [](
			ListBox&, const bool&) -> std::optional<bool>
		{
			return false;
		};
		options.Changed = [](
			ListBox& target, const bool&, const bool&)
		{
			target.InvalidateVisual();
		};
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = 20;
		options.Design.Editor = ControlPropertyEditorKind::Boolean;
		options.Design.Browsable = false;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		BindingPropertyRegistry::Register<ListBox, bool>(L"ShowColumnHeaders",
			[](ListBox& target) { return target.ShowColumnHeaders; },
			[](ListBox& target, const bool& value)
			{ target.ShowColumnHeaders = value; },
			[](ListBox& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						Control*, const ControlPropertyChangedEventArgs& args)
					{
						if (_wcsicmp(args.PropertyName.c_str(), L"ShowColumnHeaders") == 0)
							handler();
					});
			},
			std::move(options));
		return true;
	}();
	(void)registered;
}
