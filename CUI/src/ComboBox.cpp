#pragma once
#include "ComboBox.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#pragma comment(lib, "Imm32.lib")
#define COMBO_MIN_SCROLL_BLOCK 16
UIClass ComboBox::Type() { return UIClass::UI_ComboBox; }

namespace
{
	template<typename TValue>
	ControlPropertyOptions<ComboBox, TValue> ComboBoxPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<ComboBox, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto ComboBoxPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			ComboBox& target,
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

	bool ComboBoxColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<ComboBox, D2D1_COLOR_F> ComboBoxColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ComboBoxPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = ComboBoxColorsEqual;
		return options;
	}

	ControlPropertyOptions<ComboBox, float> ComboBoxNonNegativeFloatOptions(
		float defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order)
	{
		auto options = ComboBoxPropertyOptions(
			defaultValue, category, categoryOrder, order,
			ControlPropertyEditorKind::Number);
		options.Coerce = [](
			ComboBox&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}
}

void ComboBox::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto selectedIndexOptions = ComboBoxPropertyOptions(
			0, L"Behavior", 110, 10,
			ControlPropertyEditorKind::Number);
		selectedIndexOptions.Coerce = [](
			ComboBox& target, const int& proposed) -> std::optional<int>
		{
			const int nonNegative = (std::max)(0, proposed);
			return target.GetItems().empty()
				? nonNegative
				: (std::min)(nonNegative,
					static_cast<int>(target.GetItems().size()) - 1);
		};
		selectedIndexOptions.Changed = [](
			ComboBox& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectedIndexChange(oldValue, newValue);
		};
		selectedIndexOptions.Design.Minimum = 0.0;
		selectedIndexOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<ComboBox, int>(L"SelectedIndex",
			[](ComboBox& target) { return target.SelectedIndex; },
			[](ComboBox& target, const int& value) { target.SelectedIndex = value; },
			ComboBoxPropertySubscriber(L"SelectedIndex"),
			std::move(selectedIndexOptions));

		auto expandCountOptions = ComboBoxPropertyOptions(
			4, L"Behavior", 110, 20,
			ControlPropertyEditorKind::Number);
		expandCountOptions.Coerce = [](
			ComboBox&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(1, proposed);
		};
		expandCountOptions.Changed = [](
			ComboBox& target, const int&, const int&)
		{
			target.EnsureScrollInRange();
			target.EnsureSelectedItemVisible();
			target.NotifyAccessibilityScrollChanged();
		};
		expandCountOptions.Design.Minimum = 1.0;
		expandCountOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<ComboBox, int>(L"ExpandCount",
			[](ComboBox& target) { return target.ExpandCount; },
			[](ComboBox& target, const int& value) { target.ExpandCount = value; },
			ComboBoxPropertySubscriber(L"ExpandCount"),
			std::move(expandCountOptions));

		auto animationOptions = ComboBoxPropertyOptions(
			180, L"Behavior", 110, 30,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::None);
		animationOptions.Coerce = [](
			ComboBox&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		animationOptions.Design.Minimum = 0.0;
		animationOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<ComboBox, int>(L"AnimationDurationMs",
			[](ComboBox& target) { return static_cast<int>(target.AnimationDurationMs); },
			[](ComboBox& target, const int& value)
			{ target.AnimationDurationMs = static_cast<UINT>(value); },
			ComboBoxPropertySubscriber(L"AnimationDurationMs"),
			std::move(animationOptions));

		auto expandOptions = ComboBoxPropertyOptions(
			false, L"Behavior", 110, 40,
			ControlPropertyEditorKind::Boolean);
		expandOptions.Changed = [](
			ComboBox& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyExpandedStateChange(oldValue, newValue);
		};
		expandOptions.Design.Browsable = false;
		expandOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<ComboBox, bool>(L"Expand",
			[](ComboBox& target) { return target.Expand; },
			[](ComboBox& target, const bool& value) { target.Expand = value; },
			ComboBoxPropertySubscriber(L"Expand"), std::move(expandOptions));

		auto expandScrollOptions = ComboBoxPropertyOptions(
			0, L"Behavior", 110, 50,
			ControlPropertyEditorKind::Number);
		expandScrollOptions.Coerce = [](
			ComboBox& target, const int& proposed) -> std::optional<int>
		{
			const int visibleCount = target.VisibleItemCount();
			const int maximum = (std::max)(
				0, static_cast<int>(target.GetItems().size()) - visibleCount);
			return (std::clamp)(proposed, 0, maximum);
		};
		expandScrollOptions.Changed = [](
			ComboBox& target, const int&, const int&)
		{
			target.OnScrollChanged(&target);
			target.NotifyAccessibilityScrollChanged();
		};
		expandScrollOptions.Design.Browsable = false;
		expandScrollOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<ComboBox, int>(L"ExpandScroll",
			[](ComboBox& target) { return target.ExpandScroll; },
			[](ComboBox& target, const int& value) { target.ExpandScroll = value; },
			ComboBoxPropertySubscriber(L"ExpandScroll"),
			std::move(expandScrollOptions));

		BindingPropertyRegistry::Register<ComboBox, float>(L"CornerRadius",
			[](ComboBox& target) { return target.CornerRadius; },
			[](ComboBox& target, const float& value) { target.CornerRadius = value; },
			ComboBoxPropertySubscriber(L"CornerRadius"),
			ComboBoxNonNegativeFloatOptions(6.0f, L"Appearance", 200, 10));
		BindingPropertyRegistry::Register<ComboBox, float>(L"DropCornerRadius",
			[](ComboBox& target) { return target.DropCornerRadius; },
			[](ComboBox& target, const float& value) { target.DropCornerRadius = value; },
			ComboBoxPropertySubscriber(L"DropCornerRadius"),
			ComboBoxNonNegativeFloatOptions(7.0f, L"Appearance", 200, 20));
		BindingPropertyRegistry::Register<ComboBox, float>(L"DropGap",
			[](ComboBox& target) { return target.DropGap; },
			[](ComboBox& target, const float& value) { target.DropGap = value; },
			ComboBoxPropertySubscriber(L"DropGap"),
			ComboBoxNonNegativeFloatOptions(4.0f, L"Layout", 100, 10));
		BindingPropertyRegistry::Register<ComboBox, float>(L"ItemHorizontalPadding",
			[](ComboBox& target) { return target.ItemHorizontalPadding; },
			[](ComboBox& target, const float& value) { target.ItemHorizontalPadding = value; },
			ComboBoxPropertySubscriber(L"ItemHorizontalPadding"),
			ComboBoxNonNegativeFloatOptions(10.0f, L"Layout", 100, 20));
		BindingPropertyRegistry::Register<ComboBox, float>(L"ItemVerticalPadding",
			[](ComboBox& target) { return target.ItemVerticalPadding; },
			[](ComboBox& target, const float& value) { target.ItemVerticalPadding = value; },
			ComboBoxPropertySubscriber(L"ItemVerticalPadding"),
			ComboBoxNonNegativeFloatOptions(3.0f, L"Layout", 100, 30));
		BindingPropertyRegistry::Register<ComboBox, float>(L"ChevronSize",
			[](ComboBox& target) { return target.ChevronSize; },
			[](ComboBox& target, const float& value) { target.ChevronSize = value; },
			ComboBoxPropertySubscriber(L"ChevronSize"),
			ComboBoxNonNegativeFloatOptions(10.0f, L"Layout", 100, 40));
		BindingPropertyRegistry::Register<ComboBox, float>(L"ScrollBarWidth",
			[](ComboBox& target) { return target.ScrollBarWidth; },
			[](ComboBox& target, const float& value) { target.ScrollBarWidth = value; },
			ComboBoxPropertySubscriber(L"ScrollBarWidth"),
			ComboBoxNonNegativeFloatOptions(6.0f, L"Layout", 100, 50));
		BindingPropertyRegistry::Register<ComboBox, float>(L"BorderThickness",
			[](ComboBox& target) { return target.BorderThickness; },
			[](ComboBox& target, const float& value) { target.BorderThickness = value; },
			ComboBoxPropertySubscriber(L"BorderThickness"),
			ComboBoxNonNegativeFloatOptions(1.5f, L"Appearance", 200, 30));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"AccentColor",
			[](ComboBox& target) { return target.AccentColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.AccentColor = value; },
			ComboBoxPropertySubscriber(L"AccentColor"),
			ComboBoxColorOptions(cui::theme::palette::Accent, 40));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"HeaderHoverBackColor",
			[](ComboBox& target) { return target.HeaderHoverBackColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.HeaderHoverBackColor = value; },
			ComboBoxPropertySubscriber(L"HeaderHoverBackColor"),
			ComboBoxColorOptions(cui::theme::palette::AccentSoft, 50));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"DropBackColor",
			[](ComboBox& target) { return target.DropBackColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.DropBackColor = value; },
			ComboBoxPropertySubscriber(L"DropBackColor"),
			ComboBoxColorOptions(cui::theme::palette::Surface, 60));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"DropBorderColor",
			[](ComboBox& target) { return target.DropBorderColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.DropBorderColor = value; },
			ComboBoxPropertySubscriber(L"DropBorderColor"),
			ComboBoxColorOptions(cui::theme::palette::Border, 70));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"SelectedItemBackColor",
			[](ComboBox& target) { return target.SelectedItemBackColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.SelectedItemBackColor = value; },
			ComboBoxPropertySubscriber(L"SelectedItemBackColor"),
			ComboBoxColorOptions(cui::theme::palette::AccentSelected, 80));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"UnderMouseBackColor",
			[](ComboBox& target) { return target.UnderMouseBackColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.UnderMouseBackColor = value; },
			ComboBoxPropertySubscriber(L"UnderMouseBackColor"),
			ComboBoxColorOptions(cui::theme::palette::AccentSoft, 90));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"SelectedItemForeColor",
			[](ComboBox& target) { return target.SelectedItemForeColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.SelectedItemForeColor = value; },
			ComboBoxPropertySubscriber(L"SelectedItemForeColor"),
			ComboBoxColorOptions(cui::theme::palette::TextPrimary, 100));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"UnderMouseForeColor",
			[](ComboBox& target) { return target.UnderMouseForeColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.UnderMouseForeColor = value; },
			ComboBoxPropertySubscriber(L"UnderMouseForeColor"),
			ComboBoxColorOptions(cui::theme::palette::TextPrimary, 110));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"ScrollBackColor",
			[](ComboBox& target) { return target.ScrollBackColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.ScrollBackColor = value; },
			ComboBoxPropertySubscriber(L"ScrollBackColor"),
			ComboBoxColorOptions(cui::theme::palette::ScrollTrack, 120));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"ScrollForeColor",
			[](ComboBox& target) { return target.ScrollForeColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.ScrollForeColor = value; },
			ComboBoxPropertySubscriber(L"ScrollForeColor"),
			ComboBoxColorOptions(cui::theme::palette::ScrollThumb, 130));
		BindingPropertyRegistry::Register<ComboBox, D2D1_COLOR_F>(L"ButtonBackColor",
			[](ComboBox& target) { return target.ButtonBackColor; },
			[](ComboBox& target, const D2D1_COLOR_F& value) { target.ButtonBackColor = value; },
			ComboBoxPropertySubscriber(L"ButtonBackColor"),
			ComboBoxColorOptions(cui::theme::palette::SurfaceMuted, 140));
		return true;
	}();
	(void)registered;
}

GET_CPP(ComboBox, float, CornerRadius) { return _cornerRadius; }
SET_CPP(ComboBox, float, CornerRadius) { SetPropertyField(L"CornerRadius", _cornerRadius, value); }
GET_CPP(ComboBox, float, DropCornerRadius) { return _dropCornerRadius; }
SET_CPP(ComboBox, float, DropCornerRadius) { SetPropertyField(L"DropCornerRadius", _dropCornerRadius, value); }
GET_CPP(ComboBox, float, DropGap) { return _dropGap; }
SET_CPP(ComboBox, float, DropGap) { SetPropertyField(L"DropGap", _dropGap, value); }
GET_CPP(ComboBox, float, ItemHorizontalPadding) { return _itemHorizontalPadding; }
SET_CPP(ComboBox, float, ItemHorizontalPadding) { SetPropertyField(L"ItemHorizontalPadding", _itemHorizontalPadding, value); }
GET_CPP(ComboBox, float, ItemVerticalPadding) { return _itemVerticalPadding; }
SET_CPP(ComboBox, float, ItemVerticalPadding) { SetPropertyField(L"ItemVerticalPadding", _itemVerticalPadding, value); }
GET_CPP(ComboBox, float, ChevronSize) { return _chevronSize; }
SET_CPP(ComboBox, float, ChevronSize) { SetPropertyField(L"ChevronSize", _chevronSize, value); }
GET_CPP(ComboBox, float, ScrollBarWidth) { return _scrollBarWidth; }
SET_CPP(ComboBox, float, ScrollBarWidth) { SetPropertyField(L"ScrollBarWidth", _scrollBarWidth, value); }
GET_CPP(ComboBox, float, BorderThickness) { return _borderThickness; }
SET_CPP(ComboBox, float, BorderThickness) { SetPropertyField(L"BorderThickness", _borderThickness, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, AccentColor) { return _accentColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, AccentColor) { SetPropertyField(L"AccentColor", _accentColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, HeaderHoverBackColor) { return _headerHoverBackColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, HeaderHoverBackColor) { SetPropertyField(L"HeaderHoverBackColor", _headerHoverBackColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, DropBackColor) { return _dropBackColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, DropBackColor) { SetPropertyField(L"DropBackColor", _dropBackColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, DropBorderColor) { return _dropBorderColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, DropBorderColor) { SetPropertyField(L"DropBorderColor", _dropBorderColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, SelectedItemBackColor) { return _selectedItemBackColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, SelectedItemBackColor) { SetPropertyField(L"SelectedItemBackColor", _selectedItemBackColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, UnderMouseBackColor) { return _underMouseBackColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, UnderMouseBackColor) { SetPropertyField(L"UnderMouseBackColor", _underMouseBackColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, SelectedItemForeColor) { return _selectedItemForeColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, SelectedItemForeColor) { SetPropertyField(L"SelectedItemForeColor", _selectedItemForeColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, UnderMouseForeColor) { return _underMouseForeColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, UnderMouseForeColor) { SetPropertyField(L"UnderMouseForeColor", _underMouseForeColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, ScrollBackColor) { return _scrollBackColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, ScrollBackColor) { SetPropertyField(L"ScrollBackColor", _scrollBackColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, ScrollForeColor) { return _scrollForeColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, ScrollForeColor) { SetPropertyField(L"ScrollForeColor", _scrollForeColor, value); }
GET_CPP(ComboBox, D2D1_COLOR_F, ButtonBackColor) { return _buttonBackColor; }
SET_CPP(ComboBox, D2D1_COLOR_F, ButtonBackColor) { SetPropertyField(L"ButtonBackColor", _buttonBackColor, value); }

GET_CPP(ComboBox, int, ExpandCount) { return _expandCount; }
SET_CPP(ComboBox, int, ExpandCount) { (void)SetPropertyField(L"ExpandCount", _expandCount, value); }
GET_CPP(ComboBox, int, ExpandScroll) { return _expandScroll; }
SET_CPP(ComboBox, int, ExpandScroll) { (void)SetPropertyField(L"ExpandScroll", _expandScroll, value); }
GET_CPP(ComboBox, bool, Expand) { return _expand; }
SET_CPP(ComboBox, bool, Expand) { (void)SetPropertyField(L"Expand", _expand, value); }
GET_CPP(ComboBox, int, SelectedIndex) { return _selectedIndex; }
SET_CPP(ComboBox, int, SelectedIndex) { (void)SetPropertyField(L"SelectedIndex", _selectedIndex, value); }
GET_CPP(ComboBox, UINT, AnimationDurationMs)
{
	return static_cast<UINT>(_animationDurationMs);
}
SET_CPP(ComboBox, UINT, AnimationDurationMs)
{
	const auto maximum = static_cast<UINT>((std::numeric_limits<int>::max)());
	const int proposed = static_cast<int>((std::min)(value, maximum));
	(void)SetPropertyField(L"AnimationDurationMs", _animationDurationMs, proposed);
}

static D2D1_POINT_2F RotatePoint(const D2D1_POINT_2F& point, float cx, float cy, float angle)
{
	const float dx = point.x - cx;
	const float dy = point.y - cy;
	const float s = std::sin(angle);
	const float c = std::cos(angle);
	return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
}

static void DrawComboChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
{
	if (!d2d) return;
	progress = (std::clamp)(progress, 0.0f, 1.0f);
	const float halfW = size * 0.42f;
	const float halfH = size * 0.26f;
	const float angle = progress * 3.14159265359f;
	D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
	D2D1_POINT_2F p2 = D2D1::Point2F(cx, cy + halfH);
	D2D1_POINT_2F p3 = D2D1::Point2F(cx + halfW, cy - halfH);
	p1 = RotatePoint(p1, cx, cy, angle);
	p2 = RotatePoint(p2, cx, cy, angle);
	p3 = RotatePoint(p3, cx, cy, angle);
	d2d->DrawLine(p1, p2, color, 1.8f);
	d2d->DrawLine(p2, p3, color, 1.8f);
}

bool ComboBox::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0 || !this->Expand) return false;
	const int visibleCount = VisibleItemCount();
	const int maxScroll = std::max(0, static_cast<int>(this->values.size()) - visibleCount);
	if (maxScroll <= 0) return false;
	EnsureScrollInRange();
	return delta > 0
		? this->ExpandScroll > 0
		: this->ExpandScroll < maxScroll;
}

bool ComboBox::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_UP:
	case VK_DOWN:
	case VK_HOME:
	case VK_END:
	case VK_RETURN:
	case VK_SPACE:
	case VK_ESCAPE:
	case VK_F4:
		return true;
	default:
		return false;
	}
}

ComboBox::ItemCollection& ComboBox::GetItems()
{
	return this->values;
}
void ComboBox::SetItems(const std::vector<std::wstring>& value)
{
	this->values = value;
}

void ComboBox::OnItemsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	auto remapIndex = [&](int current, bool keepNearest)
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
					&& index < change.OldIndex + change.OldCount)
					return keepNearest && !values.empty()
						? (std::min)(current,
							static_cast<int>(values.size()) - 1)
						: -1;
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
		return index < values.size() ? current : -1;
	};

	bool preciseIds = _accessibilityItemIds.size() == change.OldSize
		&& _accessibilityItemTexts.size() == change.OldSize;
	if (preciseIds)
	{
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			preciseIds = change.NewIndex != CollectionChangedEventArgs::Npos
				&& change.NewIndex <= _accessibilityItemIds.size()
				&& change.NewIndex + change.NewCount <= values.size();
			if (preciseIds)
				_accessibilityItemIds.insert(
					_accessibilityItemIds.begin() + change.NewIndex,
					change.NewCount, 0);
			break;
		case CollectionChangeAction::Remove:
			preciseIds = change.OldIndex != CollectionChangedEventArgs::Npos
				&& change.OldIndex + change.OldCount
				<= _accessibilityItemIds.size();
			if (preciseIds)
				_accessibilityItemIds.erase(
					_accessibilityItemIds.begin() + change.OldIndex,
					_accessibilityItemIds.begin()
						+ change.OldIndex + change.OldCount);
			break;
		case CollectionChangeAction::Replace:
			preciseIds = change.NewIndex != CollectionChangedEventArgs::Npos
				&& change.NewIndex + change.NewCount
				<= _accessibilityItemIds.size();
			if (preciseIds)
				std::fill_n(_accessibilityItemIds.begin() + change.NewIndex,
					change.NewCount, 0);
			break;
		case CollectionChangeAction::Move:
			preciseIds = change.OldIndex < _accessibilityItemIds.size()
				&& change.NewIndex < _accessibilityItemIds.size();
			if (preciseIds && change.OldIndex < change.NewIndex)
				std::rotate(_accessibilityItemIds.begin() + change.OldIndex,
					_accessibilityItemIds.begin() + change.OldIndex + 1,
					_accessibilityItemIds.begin() + change.NewIndex + 1);
			else if (preciseIds && change.NewIndex < change.OldIndex)
				std::rotate(_accessibilityItemIds.begin() + change.NewIndex,
					_accessibilityItemIds.begin() + change.OldIndex,
					_accessibilityItemIds.begin() + change.OldIndex + 1);
			break;
		case CollectionChangeAction::Swap:
			preciseIds = change.OldIndex < _accessibilityItemIds.size()
				&& change.NewIndex < _accessibilityItemIds.size();
			if (preciseIds)
				std::swap(_accessibilityItemIds[change.OldIndex],
					_accessibilityItemIds[change.NewIndex]);
			break;
		case CollectionChangeAction::Reset:
			preciseIds = false;
			break;
		}
	}
	if (preciseIds)
	{
		_accessibilityItemTexts.assign(values.begin(), values.end());
		for (auto& id : _accessibilityItemIds)
		{
			if (id == 0) id = AllocateAccessibilityVirtualId();
		}
		RebuildAccessibilityItemIndex();
	}
	else
	{
		ReconcileAccessibilityItemIds();
	}

	// SelectedIndex/ExpandScroll coercion depends on the collection.
	// Re-evaluate declarative Local/Binding values when Items becomes available
	// or changes its range, instead of losing a value that arrived first.
	int selectedByIdentity = -1;
	if (_selectedAccessibilityItemId != 0)
	{
		const auto selected = std::find(_accessibilityItemIds.begin(),
			_accessibilityItemIds.end(), _selectedAccessibilityItemId);
		if (selected != _accessibilityItemIds.end())
			selectedByIdentity = static_cast<int>(
				selected - _accessibilityItemIds.begin());
	}
	if (selectedByIdentity >= 0)
		SetCurrentSelectedIndex(selectedByIdentity);
	else if (_selectedAccessibilityItemId != 0)
		SetCurrentSelectedIndex(remapIndex(SelectedIndex, true));
	else if (GetPropertyValueSource(L"SelectedIndex")
		!= ControlPropertyValueSource::Default)
		(void)ReevaluatePropertyValue(L"SelectedIndex");
	else
		SetCurrentSelectedIndex(remapIndex(SelectedIndex, true));
	EnsureSelectionInRange();
	if (GetPropertyValueSource(L"ExpandScroll")
		== ControlPropertyValueSource::Default)
		SetCurrentExpandScroll(remapIndex(ExpandScroll, true));
	else
		(void)ReevaluatePropertyValue(L"ExpandScroll");
	_underMouseIndex = change.Action == CollectionChangeAction::Reset
		? -1 : remapIndex(_underMouseIndex, false);
	EnsureScrollInRange();
	EnsureSelectedItemVisible();
	_selectedAccessibilityItemId = SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < _accessibilityItemIds.size()
		? _accessibilityItemIds[static_cast<size_t>(SelectedIndex)] : 0;
	if (this->values.empty() && this->Expand)
		SetExpanded(false);
	else if (this->Expand && this->ParentForm)
		this->ParentForm->ForegroundControl = this;
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
}

int ComboBox::VisibleItemCount()
{
	if (this->values.size() <= 0) return 0;
	int maxVisible = this->ExpandCount;
	if (maxVisible < 1) maxVisible = 1;
	return std::min(maxVisible, (int)this->values.size());
}

float ComboBox::FullDropdownHeight()
{
	return (float)(this->Height * VisibleItemCount());
}

float ComboBox::CurrentDropProgress()
{
	if (!_animating)
		return _dropProgress;

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(
		static_cast<UINT>((std::max)(0, _animationDurationMs)));
	float t = duration > 0
		? (float)elapsed / (float)duration
		: 1.0f;
	if (t >= 1.0f)
	{
		const bool wasCollapsing = (_animTargetProgress <= 0.001f && _dropProgress > 0.001f);
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (wasCollapsing)
			_collapseCleanupPending = true;
		if (_dropProgress <= 0.0f && this->ParentForm && this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = nullptr;
		return _dropProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_dropProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _dropProgress;
}

float ComboBox::CurrentDropdownHeight()
{
	return FullDropdownHeight() * CurrentDropProgress();
}

float ComboBox::DropdownTop()
{
	return (float)this->Height + (std::max)(0.0f, this->DropGap);
}

bool ComboBox::IsDropDownVisible()
{
	return this->Expand || _animating || _dropProgress > 0.001f;
}

bool ComboBox::IsDropDownInteractive()
{
	return this->Expand && CurrentDropdownHeight() > 0.5f;
}

bool ComboBox::IsHeaderHit(int localX, int localY)
{
	return localX >= 0 && localX <= this->Width && localY >= 0 && localY < this->Height;
}

bool ComboBox::IsDropdownHit(int localX, int localY, float dropdownHeight)
{
	const float top = DropdownTop();
	return localX >= 0 && localX <= this->Width &&
		(float)localY >= top &&
		(float)localY < (top + dropdownHeight);
}

void ComboBox::EnsureSelectionInRange()
{
	if (this->values.empty())
	{
		SyncTextWithSelection();
		return;
	}
	const int lastIndex = static_cast<int>(this->values.size()) - 1;
	const int clamped = (std::clamp)(this->SelectedIndex, 0, lastIndex);
	if (clamped != this->SelectedIndex)
		SetCurrentSelectedIndex(clamped);
	else
		SyncTextWithSelection();
}

void ComboBox::EnsureScrollInRange()
{
	const int visibleCount = VisibleItemCount();
	const int maxScroll = std::max(0, (int)this->values.size() - visibleCount);
	const int clamped = (std::clamp)(this->ExpandScroll, 0, maxScroll);
	if (clamped != this->ExpandScroll)
		SetCurrentExpandScroll(clamped);
	if (_underMouseIndex >= static_cast<int>(this->values.size())) _underMouseIndex = -1;
}

void ComboBox::EnsureSelectedItemVisible()
{
	if (this->values.empty()) return;
	const int visibleCount = VisibleItemCount();
	if (visibleCount <= 0) return;
	int offset = this->ExpandScroll;
	if (this->SelectedIndex < offset)
		offset = this->SelectedIndex;
	else if (this->SelectedIndex >= offset + visibleCount)
		offset = this->SelectedIndex - visibleCount + 1;
	if (offset != this->ExpandScroll)
		SetCurrentExpandScroll(offset);
}

void ComboBox::SyncTextWithSelection()
{
	if (this->values.empty()
		|| this->SelectedIndex < 0
		|| static_cast<size_t>(this->SelectedIndex) >= this->values.size())
	{
		this->Text = L"";
		return;
	}
	this->Text = this->values[static_cast<size_t>(this->SelectedIndex)];
}

void ComboBox::SetCurrentSelectedIndex(int value)
{
	if (_selectedIndex == value) return;
	(void)SetCurrentPropertyField(L"SelectedIndex", _selectedIndex, value);
}

void ComboBox::SetCurrentExpandScroll(int value)
{
	if (_expandScroll == value) return;
	(void)SetCurrentPropertyField(L"ExpandScroll", _expandScroll, value);
}

void ComboBox::SetCurrentExpanded(bool value)
{
	if (_expand == value) return;
	(void)SetCurrentPropertyField(L"Expand", _expand, value);
}

void ComboBox::ApplySelectedIndexChange(int oldValue, int newValue)
{
	if (oldValue == newValue || this->values.empty()) return;
	SyncTextWithSelection();
	EnsureSelectedItemVisible();
	if (_accessibilityItemIds.size() != values.size()
		|| _accessibilityItemTexts.size() != values.size())
		ReconcileAccessibilityItemIds();
	_selectedAccessibilityItemId = newValue >= 0
		&& static_cast<size_t>(newValue) < _accessibilityItemIds.size()
		? _accessibilityItemIds[static_cast<size_t>(newValue)] : 0;
	this->OnSelectionChanged(this);
}

void ComboBox::ApplyExpandedStateChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	CurrentDropProgress();
	if (newValue)
	{
		EnsureSelectionInRange();
		EnsureScrollInRange();
		EnsureSelectedItemVisible();
		if (VisibleItemCount() > 0 && this->ParentForm)
			this->ParentForm->ForegroundControl = this;
	}
	else
	{
		_underMouseIndex = -1;
		isDraggingScroll = false;
	}

	_animStartProgress = _dropProgress;
	_animTargetProgress = newValue ? 1.0f : 0.0f;
	_collapseCleanupPending = false;
	if (EffectiveAnimationDuration(
		static_cast<UINT>((std::max)(0, _animationDurationMs))) == 0
		|| std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (!newValue && this->ParentForm
			&& this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = nullptr;
	}
	else
	{
		_animStartTick = ::GetTickCount64();
		_animating = true;
	}
	if (this->ParentForm)
		this->ParentForm->Invalidate(false);
}
CursorKind ComboBox::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;

	const bool hasVScroll = (IsDropDownVisible() && static_cast<int>(this->values.size()) > VisibleItemCount());
	const float dropHeight = CurrentDropdownHeight();
	const float dropTop = DropdownTop();
	if (hasVScroll && localX >= (this->Width - 12) && localY >= dropTop && (float)localY <= (dropTop + dropHeight))
		return CursorKind::SizeNS;

	return this->Cursor;
}
ComboBox::ComboBox(std::wstring text, int x, int y, int width, int height)
{
	values.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnItemsCollectionChanged(change); });
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->Cursor = CursorKind::Hand;
}

bool ComboBox::IsAnimationRunning()
{
	CurrentDropProgress();
	return _animating || _collapseCleanupPending;
}

bool ComboBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsDropDownVisible() && !_collapseCleanupPending) return false;
	auto abs = this->AbsRect;
	outRect = abs;
	outRect.bottom += (LONG)std::ceil((std::max)(0.0f, this->DropGap) + FullDropdownHeight());
	return true;
}

void ComboBox::SetExpanded(bool expanded)
{
	const bool wantExpand = expanded && VisibleItemCount() > 0;
	if (this->Expand == wantExpand) return;
	SetCurrentExpanded(wantExpand);
}

bool ComboBox::SelectItem(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= this->values.size())
		return false;
	if (this->SelectedIndex == index)
	{
		EnsureSelectedItemVisible();
		return true;
	}
	SetCurrentSelectedIndex(index);
	return true;
}

void ComboBox::ScrollBy(int itemDelta)
{
	if (itemDelta == 0) return;
	const int visibleCount = VisibleItemCount();
	const int maximum = (std::max)(
		0, static_cast<int>(this->values.size()) - visibleCount);
	const int target = (std::clamp)(
		this->ExpandScroll + itemDelta, 0, maximum);
	if (target != this->ExpandScroll)
		SetCurrentExpandScroll(target);
}

// ---- 项操作便捷方法 ----
int ComboBox::GetItemCount()
{
	return static_cast<int>(this->values.size());
}

std::wstring ComboBox::GetSelectedItem()
{
	const int index = this->GetSelectedIndex();
	if (index < 0 || static_cast<size_t>(index) >= this->values.size())
		return L"";
	return this->values[static_cast<size_t>(index)];
}

int ComboBox::FindItem(const std::wstring& text)
{
	for (size_t i = 0; i < this->values.size(); ++i)
	{
		if (this->values[i] == text)
			return static_cast<int>(i);
	}
	return -1;
}

void ComboBox::AddItem(const std::wstring& text)
{
	this->values.push_back(text);
}

void ComboBox::InsertItem(int index, const std::wstring& text)
{
	index = (std::clamp)(index, 0, static_cast<int>(this->values.size()));
	this->values.insert(this->values.begin() + index, text);
}

void ComboBox::RemoveItemAt(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= this->values.size())
		return;
	this->values.erase(this->values.begin() + index);
}

void ComboBox::ClearItems()
{
	this->values.clear();
}

void ComboBox::ReconcileAccessibilityItemIds()
{
	std::unordered_map<std::wstring, std::vector<uint32_t>> reusableIds;
	reusableIds.reserve(_accessibilityItemTexts.size());
	for (size_t old = _accessibilityItemTexts.size(); old > 0; --old)
	{
		const size_t index = old - 1;
		if (index < _accessibilityItemIds.size()
			&& _accessibilityItemIds[index] != 0)
			reusableIds[_accessibilityItemTexts[index]].push_back(
				_accessibilityItemIds[index]);
	}
	std::vector<uint32_t> nextIds(values.size(), 0);
	for (size_t next = 0; next < values.size(); ++next)
	{
		auto reusable = reusableIds.find(values[next]);
		if (reusable != reusableIds.end() && !reusable->second.empty())
		{
			nextIds[next] = reusable->second.back();
			reusable->second.pop_back();
		}
		if (nextIds[next] == 0)
			nextIds[next] = AllocateAccessibilityVirtualId();
	}
	_accessibilityItemIds = std::move(nextIds);
	_accessibilityItemTexts = values;
	RebuildAccessibilityItemIndex();
}

void ComboBox::RebuildAccessibilityItemIndex()
{
	_accessibilityItemIndexById.clear();
	_accessibilityItemIndexById.reserve(_accessibilityItemIds.size());
	for (size_t index = 0; index < _accessibilityItemIds.size(); ++index)
	{
		auto& id = _accessibilityItemIds[index];
		while (id == 0
			|| !_accessibilityItemIndexById.emplace(id, index).second)
			id = AllocateAccessibilityVirtualId();
	}
}

int ComboBox::FindAccessibilityItem(uint32_t id)
{
	if (id == 0) return -1;
	if (_accessibilityItemIds.size() != values.size()
		|| _accessibilityItemTexts.size() != values.size()
		|| _accessibilityItemIndexById.size() != _accessibilityItemIds.size())
		ReconcileAccessibilityItemIds();
	const auto position = _accessibilityItemIndexById.find(id);
	return position == _accessibilityItemIndexById.end()
		? -1 : static_cast<int>(position->second);
}

void ComboBox::GetAccessibilityVirtualChildren(
	uint32_t parentId, std::vector<uint32_t>& result)
{
	result.clear();
	if (parentId != 0) return;
	if (_accessibilityItemIndexById.size() != _accessibilityItemIds.size())
		RebuildAccessibilityItemIndex();
	result = _accessibilityItemIds;
}

size_t ComboBox::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	return parentId == 0 ? values.size() : 0;
}

bool ComboBox::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	if (parentId != 0 || index >= values.size()) return false;
	if (_accessibilityItemIds.size() != values.size()
		|| _accessibilityItemTexts.size() != values.size())
		ReconcileAccessibilityItemIds();
	result = _accessibilityItemIds[index];
	return result != 0;
}

bool ComboBox::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	result = 0;
	if (parentId != 0) return false;
	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	const int sibling = next ? index + 1 : index - 1;
	if (sibling < 0 || sibling >= static_cast<int>(_accessibilityItemIds.size()))
		return false;
	result = _accessibilityItemIds[static_cast<size_t>(sibling)];
	return result != 0;
}

bool ComboBox::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	if (!IsDropDownVisible() || localX < 0.0f
		|| localX >= static_cast<float>(Width)) return false;
	const float itemHeight = static_cast<float>(Height);
	const float top = DropdownTop();
	const float bottom = top + CurrentDropdownHeight();
	if (itemHeight <= 0.0f || localY < top || localY >= bottom) return false;
	const int index = ExpandScroll
		+ static_cast<int>(std::floor((localY - top) / itemHeight));
	if (index < 0 || index >= static_cast<int>(values.size())) return false;
	return TryGetAccessibilityVirtualChildAt(
		0, static_cast<size_t>(index), result);
}

bool ComboBox::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	const float itemHeight = static_cast<float>(Height);
	const float top = DropdownTop()
		+ static_cast<float>(index - ExpandScroll) * itemHeight;
	const float currentBottom = DropdownTop() + CurrentDropdownHeight();
	const bool inRealizedRange = IsDropDownVisible()
		&& index >= ExpandScroll
		&& index < ExpandScroll + VisibleItemCount()
		&& top < currentBottom && top + itemHeight > DropdownTop();
	result = {};
	result.Id = id;
	result.Role = AccessibleRole::ListItem;
	result.Patterns = AccessibilityVirtualPattern::SelectionItem
		| AccessibilityVirtualPattern::ScrollItem
		| AccessibilityVirtualPattern::VirtualizedItem;
	result.Name = values[static_cast<size_t>(index)];
	result.Value = result.Name;
	const auto ownerId = GetAccessibilitySnapshot().AutomationId;
	result.AutomationId = ownerId.empty()
		? L"item-" + std::to_wstring(id)
		: ownerId + L".item-" + std::to_wstring(id);
	result.BoundsDip = inRealizedRange
		? D2D1::RectF(0.0f, top, static_cast<float>(Width), top + itemHeight)
		: D2D1::RectF(0, 0, 0, 0);
	result.Enabled = Enable;
	result.Visible = Visible && inRealizedRange;
	result.Selected = index == SelectedIndex;
	result.Row = index;
	result.Column = 0;
	return true;
}

AccessibilityVirtualContainerInfo
ComboBox::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AccessibilityVirtualPattern::Selection
		| AccessibilityVirtualPattern::Scroll;
	result.CanSelectMultiple = false;
	result.IsSelectionRequired = !values.empty();
	result.RowCount = static_cast<int>(values.size());
	result.ColumnCount = 1;
	return result;
}

void ComboBox::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
	if (_accessibilityItemIds.size() != values.size()
		|| _accessibilityItemTexts.size() != values.size())
		ReconcileAccessibilityItemIds();
	if (SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < _accessibilityItemIds.size())
		result.push_back(_accessibilityItemIds[static_cast<size_t>(SelectedIndex)]);
}

bool ComboBox::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	if (action == AccessibilitySelectionAction::Remove) return false;
	const int index = FindAccessibilityItem(id);
	const bool selected = Enable && index >= 0 && SelectItem(index);
	if (selected)
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
	return selected;
}

bool ComboBox::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0 || !Enable) return false;
	SetExpanded(true);
	const int visibleCount = VisibleItemCount();
	if (index < ExpandScroll)
		SetCurrentExpandScroll(index);
	else if (visibleCount > 0 && index >= ExpandScroll + visibleCount)
		SetCurrentExpandScroll(index - visibleCount + 1);
	return true;
}

bool ComboBox::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	const int count = static_cast<int>(values.size());
	const int visible = count <= 0 ? 0
		: (std::min)((std::max)(1, _expandCount), count);
	const int maximum = (std::max)(0, count - visible);
	result.VerticallyScrollable = maximum > 0;
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent = (std::clamp)(
			static_cast<double>(_expandScroll) / maximum * 100.0, 0.0, 100.0);
		result.VerticalViewSize = (std::clamp)(
			static_cast<double>(visible) / count * 100.0, 0.0, 100.0);
	}
	return true;
}

bool ComboBox::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	if (horizontal != AccessibilityScrollAmount::NoAmount) return false;
	if (vertical == AccessibilityScrollAmount::NoAmount) return true;
	AccessibilityScrollInfo info;
	GetAccessibilityScrollInfo(info);
	if (!info.VerticallyScrollable) return false;
	SetExpanded(true);
	const int visible = (std::max)(1, VisibleItemCount());
	int delta = 0;
	switch (vertical)
	{
	case AccessibilityScrollAmount::LargeDecrement: delta = -visible; break;
	case AccessibilityScrollAmount::SmallDecrement: delta = -1; break;
	case AccessibilityScrollAmount::LargeIncrement: delta = visible; break;
	case AccessibilityScrollAmount::SmallIncrement: delta = 1; break;
	case AccessibilityScrollAmount::NoAmount: return true;
	}
	ScrollBy(delta);
	return true;
}

bool ComboBox::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	if (horizontalPercent != AccessibilityScrollNoChange) return false;
	if (verticalPercent == AccessibilityScrollNoChange) return true;
	if (!std::isfinite(verticalPercent)
		|| verticalPercent < 0.0 || verticalPercent > 100.0) return false;
	const int maximum = (std::max)(0,
		static_cast<int>(values.size()) - VisibleItemCount());
	if (maximum <= 0) return false;
	SetExpanded(true);
	SetCurrentExpandScroll((std::clamp)(
		static_cast<int>(std::lround(maximum * verticalPercent / 100.0)),
		0, maximum));
	return true;
}
SIZE ComboBox::ActualSize()
{
	return this->Size;
}

bool ComboBox::ContainsForegroundPoint(int localX, int localY)
{
	return IsDropDownVisible() && IsDropdownHit(localX, localY, CurrentDropdownHeight());
}

void ComboBox::InvalidateVisual()
{
	if (!this->IsVisual || !this->ParentForm)
	{
		Control::InvalidateVisual();
		return;
	}
	auto currentRect = this->AbsRect;
	if (IsDropDownVisible() || _collapseCleanupPending)
	{
		currentRect.bottom += (LONG)std::ceil((std::max)(0.0f, this->DropGap) + FullDropdownHeight());
	}
	this->InvalidateVisualRect(currentRect);
}

void ComboBox::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	const int visibleItemCount = VisibleItemCount();
	const float renderHeight = CurrentDropdownHeight();
	if (this->values.size() > 0 && visibleItemCount > 0 && renderHeight > 0.0f)
	{
		const int itemCount = static_cast<int>(this->values.size());
		if (visibleItemCount < itemCount)
		{
			int maxScroll = itemCount - visibleItemCount;
			float scrollThumbHeight = ((float)visibleItemCount / (float)this->values.size()) * renderHeight;
			if (scrollThumbHeight < COMBO_MIN_SCROLL_BLOCK)scrollThumbHeight = COMBO_MIN_SCROLL_BLOCK;
			if (scrollThumbHeight > renderHeight) scrollThumbHeight = renderHeight;
			float scrollThumbMoveSpace = renderHeight - scrollThumbHeight;
			float scrollRatio = (float)this->ExpandScroll / (float)maxScroll;
			float scrollThumbTop = scrollRatio * scrollThumbMoveSpace;
			const float barW = (std::max)(4.0f, this->ScrollBarWidth);
			const float barX = (float)this->Width - barW - 5.0f;
			const float barY = DropdownTop() + 5.0f;
			const float barH = (std::max)(0.0f, renderHeight - 10.0f);
			if (barH <= 0.0f) return;
			if (scrollThumbHeight > barH) scrollThumbHeight = barH;
			const float moveSpace = (std::max)(0.0f, barH - scrollThumbHeight);
			scrollThumbTop = scrollRatio * moveSpace;
			d2d->FillRoundRect(barX, barY, barW, barH, this->ScrollBackColor, barW * 0.5f);
			d2d->FillRoundRect(barX, barY + scrollThumbTop, barW, scrollThumbHeight, this->ScrollForeColor, barW * 0.5f);
		}
	}
}
void ComboBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	int visibleItemCount = VisibleItemCount();
	float renderHeight = CurrentDropdownHeight();
	if (visibleItemCount <= 0 || renderHeight <= 0.0f) return;
	int maxScroll = static_cast<int>(this->values.size()) - visibleItemCount;
	const float barInset = 5.0f;
	const float barHeight = (std::max)(0.0f, renderHeight - barInset * 2.0f);
	if (barHeight <= 0.0f) return;
	float scrollBlockHeight = ((float)visibleItemCount / (float)this->values.size()) * barHeight;
	if (scrollBlockHeight < COMBO_MIN_SCROLL_BLOCK)scrollBlockHeight = COMBO_MIN_SCROLL_BLOCK;
	if (scrollBlockHeight > barHeight) scrollBlockHeight = barHeight;
	float scrollHeight = barHeight - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - barInset - grab;
	float per = targetTop / scrollHeight;
	per = std::clamp(per, 0.0f, 1.0f);
	int newScroll = static_cast<int>(per * maxScroll);
	SetCurrentExpandScroll((std::clamp)(newScroll, 0, maxScroll));
}
void ComboBox::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	const float actualWidth = static_cast<float>(this->Width);
	const float actualHeight = static_cast<float>(this->Height);
	const float controlWidth = static_cast<float>(this->Width);
	const float controlHeight = static_cast<float>(this->Height);
	CurrentDropProgress();
	this->BeginRender(actualWidth, actualHeight);
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		const float border = (std::max)(1.0f, this->BorderThickness);
		const D2D1_RECT_F headerRect = D2D1::RectF(border * 0.5f, border * 0.5f, controlWidth - border * 0.5f, controlHeight - border * 0.5f);
		d2d->FillRoundRect(headerRect, this->BackColor, this->CornerRadius);
		if ((this->ParentForm && this->ParentForm->UnderMouse == this) || IsDropDownVisible())
			d2d->FillRoundRect(headerRect, this->HeaderHoverBackColor, this->CornerRadius);
		if (this->Image)
		{
			this->RenderImage(this->CornerRadius);
		}
		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		const float drawLeft = (std::max)(4.0f, this->ItemHorizontalPadding);
		const float drawTop = (std::max)(0.0f, (controlHeight - textSize.height) * 0.5f);
		const float textRightPad = 30.0f;
		d2d->DrawString(this->Text, drawLeft, drawTop, (std::max)(1.0f, controlWidth - drawLeft - textRightPad), textSize.height + 2.0f, this->ForeColor, font);
		{
			float iconSize = (std::max)(6.0f, this->ChevronSize);
			if (iconSize < 8.0f) iconSize = 8.0f;
			if (iconSize > 14.0f) iconSize = 14.0f;
			const float cx = controlWidth - 16.0f;
			const float cy = controlHeight * 0.5f;
			DrawComboChevron(d2d, cx, cy, iconSize, _dropProgress, this->ForeColor);
		}
		const auto borderColor = IsDropDownVisible() ? this->AccentColor : this->BorderColor;
		d2d->DrawRoundRect(headerRect.left, headerRect.top, headerRect.right - headerRect.left,
			headerRect.bottom - headerRect.top, borderColor, border, this->CornerRadius);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	if (!_animating && _dropProgress <= 0.001f)
		_collapseCleanupPending = false;
}

void ComboBox::UpdateForeground()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	EnsureSelectionInRange();
	EnsureScrollInRange();

	const float controlWidth = static_cast<float>(this->Width);
	const float controlHeight = static_cast<float>(this->Height);
	const float dropHeight = CurrentDropdownHeight();
	const int visibleCount = VisibleItemCount();
	if (dropHeight <= 0.0f || visibleCount <= 0)
	{
		if (!_animating && _dropProgress <= 0.001f)
			_collapseCleanupPending = false;
		return;
	}

	const float dropTop = DropdownTop();
	this->BeginRender(controlWidth, dropTop + dropHeight);
	{
		const float border = (std::max)(1.0f, this->BorderThickness);
		const bool hasScroll = static_cast<int>(this->values.size()) > visibleCount;
		const float itemRight = hasScroll ? controlWidth - (std::max)(4.0f, this->ScrollBarWidth) - 11.0f : controlWidth;
		const D2D1_RECT_F dropRect = D2D1::RectF(0.0f, dropTop, controlWidth, dropTop + dropHeight);
		const float drawLeft = (std::max)(4.0f, this->ItemHorizontalPadding);
		auto font = this->Font;

		d2d->PushDrawRect(0.0f, dropTop, controlWidth, dropHeight);
		d2d->FillRoundRect(dropRect, this->DropBackColor, this->DropCornerRadius);
		d2d->DrawRoundRect(dropRect.left + border * 0.5f, dropRect.top + border * 0.5f,
			(dropRect.right - dropRect.left) - border, (dropRect.bottom - dropRect.top) - border,
			this->DropBorderColor, border, this->DropCornerRadius);
		const int itemCount = static_cast<int>(this->values.size());
		for (int i = this->ExpandScroll; i < this->ExpandScroll + visibleCount && i < itemCount; i++)
		{
			const int viewIndex = i - this->ExpandScroll;
			const float itemTop = dropTop + static_cast<float>(viewIndex) * controlHeight;
			const float itemBottom = itemTop + controlHeight;
			const D2D1_RECT_F itemRect = D2D1::RectF(6.0f, itemTop + this->ItemVerticalPadding,
				(std::max)(7.0f, itemRight - 6.0f), itemBottom - this->ItemVerticalPadding);
			const bool isSelected = i == this->SelectedIndex;
			const bool isHovered = i == this->_underMouseIndex;
			if (isSelected)
			{
				d2d->FillRoundRect(itemRect, this->SelectedItemBackColor, this->CornerRadius);
				const float accentW = 3.0f;
				const float accentTop = itemRect.top + 5.0f;
				const float accentH = (std::max)(5.0f, (itemRect.bottom - itemRect.top) - 10.0f);
				d2d->FillRoundRect(itemRect.left, accentTop, accentW, accentH, this->AccentColor, accentW * 0.5f);
			}
			if (isHovered)
			{
				d2d->FillRoundRect(itemRect, this->UnderMouseBackColor, this->CornerRadius);
			}
			auto itemTextSize = font->GetTextSize(this->values[static_cast<size_t>(i)]);
			const float itemTextY = itemTop + (std::max)(0.0f, (controlHeight - itemTextSize.height) * 0.5f);
			const auto itemTextColor = isHovered ? this->UnderMouseForeColor : (isSelected ? this->SelectedItemForeColor : this->ForeColor);
			d2d->DrawString(
				this->values[static_cast<size_t>(i)],
				drawLeft,
				itemTextY,
				(std::max)(1.0f, itemRight - drawLeft - 8.0f),
				itemTextSize.height + 2.0f,
				itemTextColor, font);
		}
		d2d->PopDrawRect();
		this->DrawScroll();
	}
	this->EndRender();
}

bool ComboBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	const int visibleCount = VisibleItemCount();
	const float dropdownHeight = CurrentDropdownHeight();
	if (WM_LBUTTONDOWN == message)
	{
		if (this->ParentForm->Selected && this->ParentForm->Selected != this)
		{
			auto se = this->ParentForm->Selected;
			this->ParentForm->Selected = this;
			se->InvalidateVisual();
		}
	}
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT i = 0; i < fileCount; i++)
		{
			DragQueryFile(hDropInfo, i, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		if (this->Expand)
			ScrollBy(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1);
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (IsDropDownVisible())
		{
			bool needsUpdate = false;
			if (isDraggingScroll)
			{
				UpdateScrollDrag(static_cast<float>(localY) - DropdownTop());
				needsUpdate = true;
			}
			else
			{
				if (IsDropdownHit(localX, localY, dropdownHeight))
				{
					int visibleItemIndex = int(((float)localY - DropdownTop()) / (float)this->Height);
					if (visibleItemIndex < visibleCount)
					{
						int itemIndex = visibleItemIndex + this->ExpandScroll;
						if (itemIndex < static_cast<int>(this->values.size()))
						{
							if (itemIndex != this->_underMouseIndex)
							{
								needsUpdate = true;
							}
							this->_underMouseIndex = itemIndex;
						}
					}
				}
				else if (this->_underMouseIndex != -1)
				{
					this->_underMouseIndex = -1;
					needsUpdate = true;
				}
			}
			if (needsUpdate)this->InvalidateVisual();
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			const float dropTop = DropdownTop();
			if (this->Expand && localX >= (Width - 12) && localX <= Width && (float)localY >= dropTop && (float)localY <= (dropTop + dropdownHeight))
			{
				const int visibleItemCount = visibleCount;
				if (visibleItemCount > 0 && static_cast<int>(this->values.size()) > visibleItemCount)
				{
					const int maxScroll = static_cast<int>(this->values.size()) - visibleItemCount;
					const float barInset = 5.0f;
					const float renderH = (std::max)(0.0f, dropdownHeight - barInset * 2.0f);
					float thumbH = ((float)visibleItemCount / (float)this->values.size()) * renderH;
					if (thumbH < COMBO_MIN_SCROLL_BLOCK) thumbH = COMBO_MIN_SCROLL_BLOCK;
					if (thumbH > renderH) thumbH = renderH;
					const float moveSpace = std::max(0.0f, renderH - thumbH);
					float per = 0.0f;
					if (maxScroll > 0) per = std::clamp((float)this->ExpandScroll / (float)maxScroll, 0.0f, 1.0f);
					const float thumbTop = per * moveSpace;
					const float dropdownLocalY = (float)localY - dropTop;
					const float barLocalY = dropdownLocalY - barInset;
					const bool hitThumb = (barLocalY >= thumbTop && barLocalY <= (thumbTop + thumbH));
					_scrollThumbGrabOffsetY = hitThumb ? (barLocalY - thumbTop) : (thumbH * 0.5f);
					isDraggingScroll = true;
					UpdateScrollDrag(dropdownLocalY);
				}
			}
			this->ParentForm->Selected = this;
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (WM_LBUTTONUP == message && this->ParentForm->Selected == this)
		{
			if (isDraggingScroll) {
				isDraggingScroll = false;
			}
			else
			{
				if (IsHeaderHit(localX, localY))
				{
					SetExpanded(!this->Expand);
					if (this->ParentForm)
						this->ParentForm->Invalidate(true);
					this->InvalidateVisual();
					this->ParentForm->Selected = nullptr;
					MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
					this->OnMouseUp(this, eventArgs);
					break;
				}
				else if (this->Expand && IsDropdownHit(localX, localY, dropdownHeight))
				{
					int visibleItemIndex = int(((float)localY - DropdownTop()) / (float)this->Height);
					if (visibleItemIndex < visibleCount)
					{
						int itemIndex = visibleItemIndex + this->ExpandScroll;
						if (itemIndex < static_cast<int>(this->values.size()))
						{
							this->_underMouseIndex = itemIndex;
							(void)SelectItem(this->_underMouseIndex);
							SetExpanded(false);
							if (this->ParentForm)
								this->ParentForm->Invalidate(true);
							this->InvalidateVisual();
						}
					}
				}
			}
		}
		this->ParentForm->Selected = nullptr;
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
	}
	break;
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_UP:
			if (!this->values.empty())
				(void)SelectItem((std::max)(0, this->SelectedIndex - 1));
			break;
		case VK_DOWN:
			if (!this->values.empty())
				(void)SelectItem((std::min)(
					static_cast<int>(this->values.size()) - 1,
					this->SelectedIndex + 1));
			break;
		case VK_HOME:
			if (!this->values.empty()) (void)SelectItem(0);
			break;
		case VK_END:
			if (!this->values.empty())
				(void)SelectItem(static_cast<int>(this->values.size()) - 1);
			break;
		case VK_RETURN:
		case VK_SPACE:
		case VK_F4:
			SetExpanded(!this->Expand);
			break;
		case VK_ESCAPE:
			SetExpanded(false);
			break;
		default:
			break;
		}
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
	}
	break;
	}
	return true;
}
