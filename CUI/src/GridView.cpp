#pragma once
#define NOMINMAX
#include "GridView.h"
#include "DropDownPopup.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <utility>
#include <unordered_set>
#pragma comment(lib, "Imm32.lib")

namespace
{
	static bool GridColorEquals(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b)
	{
		return std::fabs(a.r - b.r) < 1e-6f &&
			std::fabs(a.g - b.g) < 1e-6f &&
			std::fabs(a.b - b.b) < 1e-6f &&
			std::fabs(a.a - b.a) < 1e-6f;
	}

	template<typename TValue>
	ControlPropertyOptions<GridView, TValue> GridViewPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<GridView, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto GridViewPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			GridView& target,
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

	ControlPropertyOptions<GridView, float> GridViewMetricOptions(
		float defaultValue,
		int order)
	{
		auto options = GridViewPropertyOptions(
			defaultValue, L"Layout", 100, order,
			ControlPropertyEditorKind::Number);
		options.Coerce = [](
			GridView&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	ControlPropertyOptions<GridView, D2D1_COLOR_F> GridViewColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = GridViewPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = GridColorEquals;
		return options;
	}
}

CellValue::CellValue() : Text(L"null"), Image(nullptr), Tag(0)
{
}
CellValue::CellValue(std::wstring s) : Text(s), Tag(0), Image(nullptr)
{
}
CellValue::CellValue(wchar_t* s) :Text(s), Tag(0), Image(nullptr)
{
}
CellValue::CellValue(const wchar_t* s) : Text(s), Tag(0), Image(nullptr)
{
}
CellValue::CellValue(std::shared_ptr<BitmapSource> img) : Text(L""), Tag(0), Image(std::move(img))
{
}
CellValue::CellValue(__int64 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(bool tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(__int32 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(unsigned __int32 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(unsigned __int64 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(PVOID tag) : Image(nullptr)
{
	Tag = reinterpret_cast<__int64>(tag);
	Text.resize(sizeof(PVOID) * 2);
	swprintf_s(&Text[0], Text.size(), L"%p", tag);
}

std::wstring CellValue::GetText() const
{
	return Text;
}

void CellValue::SetText(const std::wstring& text)
{
	Text = text;
}

__int64 CellValue::GetTag() const
{
	return Tag;
}

void CellValue::SetTag(__int64 tag)
{
	Tag = tag;
	Text = std::to_wstring(tag);
}

bool CellValue::GetBool() const
{
	return Tag != 0;
}

void CellValue::SetBool(bool value)
{
	Tag = value ? 1 : 0;
	Text = value ? L"1" : L"0";
}

PVOID CellValue::GetPointer() const
{
	return reinterpret_cast<PVOID>(Tag);
}

void CellValue::SetPointer(PVOID value)
{
	Tag = reinterpret_cast<__int64>(value);
	Text.resize(sizeof(PVOID) * 2);
	swprintf_s(&Text[0], Text.size(), L"%p", value);
}

void CellValue::SetComboSelection(int selectedIndex, const std::wstring& selectedText)
{
	Tag = selectedIndex;
	Text = selectedText;
}

ID2D1Bitmap* CellValue::GetImageBitmap(D2DGraphics* render)
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
CellValue& GridViewRow::operator[](int index)
{
	return Cells[index];
}
GridViewColumn::GridViewColumn(std::wstring name, float width, ColumnType type, bool canEdit)
{
	Name = name;
	Width = width;
	Type = type;
	CanEdit = canEdit;
}
UIClass GridView::Type() { return UIClass::UI_GridView; }

void GridView::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
#define CUI_REGISTER_GRID_BOOL(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<GridView, bool>(propertyName, \
			[](GridView& target) { return target.name; }, \
			[](GridView& target, const bool& value) { target.name = value; }, \
			GridViewPropertySubscriber(propertyName), \
			GridViewPropertyOptions(defaultValue, L"Behavior", 110, order, \
				ControlPropertyEditorKind::Boolean))

		CUI_REGISTER_GRID_BOOL(FullRowSelect, L"FullRowSelect", true, 10);

		{
			auto options = GridViewPropertyOptions(
				false, L"Behavior", 110, 20,
				ControlPropertyEditorKind::Boolean);
			options.Changed = [](GridView& target, const bool&, const bool&)
			{
				target.RequestRefresh();
				target.NotifyAccessibilityScrollChanged();
			};
			BindingPropertyRegistry::Register<GridView, bool>(L"AllowUserToAddRows",
				[](GridView& target) { return target.AllowUserToAddRows; },
				[](GridView& target, const bool& value) { target.AllowUserToAddRows = value; },
				GridViewPropertySubscriber(L"AllowUserToAddRows"), std::move(options));
		}
		CUI_REGISTER_GRID_BOOL(AllowUserToDeleteRows, L"AllowUserToDeleteRows", true, 30);
		CUI_REGISTER_GRID_BOOL(MultiSelect, L"MultiSelect", false, 40);

#undef CUI_REGISTER_GRID_BOOL

#define CUI_REGISTER_GRID_METRIC(name, propertyName, defaultValue, order) \
		{ \
			auto options = GridViewMetricOptions(defaultValue, order); \
			options.Changed = [](GridView& target, const float&, const float&) \
			{ target.RequestRefresh(); target.NotifyAccessibilityScrollChanged(); }; \
			BindingPropertyRegistry::Register<GridView, float>(propertyName, \
				[](GridView& target) { return target.name; }, \
				[](GridView& target, const float& value) { target.name = value; }, \
				GridViewPropertySubscriber(propertyName), std::move(options)); \
		}

		CUI_REGISTER_GRID_METRIC(HeadHeight, L"HeadHeight", 0.0f, 10);
		CUI_REGISTER_GRID_METRIC(RowHeight, L"RowHeight", 0.0f, 20);
		CUI_REGISTER_GRID_METRIC(BorderThickness, L"BorderThickness", 1.5f, 30);
		CUI_REGISTER_GRID_METRIC(CellCornerRadius, L"CellCornerRadius", 6.0f, 40);
		CUI_REGISTER_GRID_METRIC(CellHorizontalPadding, L"CellHorizontalPadding", 8.0f, 50);
		CUI_REGISTER_GRID_METRIC(CellVerticalPadding, L"CellVerticalPadding", 3.0f, 60);
		CUI_REGISTER_GRID_METRIC(SelectedAccentWidth, L"SelectedAccentWidth", 3.0f, 70);
		CUI_REGISTER_GRID_METRIC(EditTextMargin, L"EditTextMargin", 3.0f, 80);
		CUI_REGISTER_GRID_METRIC(ScrollBarSize, L"ScrollBarSize", 8.0f, 90);

#undef CUI_REGISTER_GRID_METRIC

		auto transientIntOptions = GridViewPropertyOptions(
			-1, L"Behavior", 110, 100,
			ControlPropertyEditorKind::Number);
		transientIntOptions.Design.Browsable = false;
		transientIntOptions.Design.Persistence = ControlPropertyPersistence::Transient;

		{
			auto options = transientIntOptions;
			options.Coerce = [](GridView& target, const int& proposed) -> std::optional<int>
			{
				if (proposed < 0) return -1;
				return target.Columns.empty()
					? proposed
					: (std::min)(proposed, static_cast<int>(target.Columns.size()) - 1);
			};
			options.Changed = [](GridView& target, const int&, const int&)
			{
				target.OnSelectionPropertyChanged();
			};
			BindingPropertyRegistry::Register<GridView, int>(L"SelectedColumnIndex",
				[](GridView& target) { return target.SelectedColumnIndex; },
				[](GridView& target, const int& value) { target.SelectedColumnIndex = value; },
				GridViewPropertySubscriber(L"SelectedColumnIndex"), std::move(options));
		}
		{
			auto options = transientIntOptions;
			options.Coerce = [](GridView& target, const int& proposed) -> std::optional<int>
			{
				if (proposed < 0) return -1;
				return target.Rows.empty()
					? proposed
					: (std::min)(proposed, static_cast<int>(target.Rows.size()) - 1);
			};
			options.Changed = [](GridView& target, const int&, const int&)
			{
				target.OnSelectionPropertyChanged();
			};
			BindingPropertyRegistry::Register<GridView, int>(L"SelectedRowIndex",
				[](GridView& target) { return target.SelectedRowIndex; },
				[](GridView& target, const int& value) { target.SelectedRowIndex = value; },
				GridViewPropertySubscriber(L"SelectedRowIndex"), std::move(options));
		}

		{
			auto options = transientIntOptions;
			options.Coerce = [](GridView& target, const int& proposed) -> std::optional<int>
			{
				if (target.Columns.empty() || proposed < 0) return -1;
				return (std::min)(proposed, static_cast<int>(target.Columns.size()) - 1);
			};
			BindingPropertyRegistry::Register<GridView, int>(L"UnderMouseColumnIndex",
				[](GridView& target) { return target.UnderMouseColumnIndex; },
				[](GridView& target, const int& value) { target.UnderMouseColumnIndex = value; },
				GridViewPropertySubscriber(L"UnderMouseColumnIndex"), std::move(options));
		}
		{
			auto options = transientIntOptions;
			options.Coerce = [](GridView& target, const int& proposed) -> std::optional<int>
			{
				if (target.Rows.empty() || proposed < 0) return -1;
				return (std::min)(proposed, static_cast<int>(target.Rows.size()) - 1);
			};
			BindingPropertyRegistry::Register<GridView, int>(L"UnderMouseRowIndex",
				[](GridView& target) { return target.UnderMouseRowIndex; },
				[](GridView& target, const int& value) { target.UnderMouseRowIndex = value; },
				GridViewPropertySubscriber(L"UnderMouseRowIndex"), std::move(options));
		}
		{
			auto options = transientIntOptions;
			options.Coerce = [](GridView& target, const int& proposed) -> std::optional<int>
			{
				if (proposed < 0) return -1;
				return target.Columns.empty()
					? proposed
					: (std::min)(proposed, static_cast<int>(target.Columns.size()) - 1);
			};
			BindingPropertyRegistry::Register<GridView, int>(L"SortedColumnIndex",
				[](GridView& target) { return target.SortedColumnIndex; },
				[](GridView& target, const int& value) { target.SortedColumnIndex = value; },
				GridViewPropertySubscriber(L"SortedColumnIndex"), std::move(options));
		}

		{
			auto options = GridViewPropertyOptions(
				true, L"Behavior", 110, 110,
				ControlPropertyEditorKind::Boolean);
			options.Design.Browsable = false;
			options.Design.Persistence = ControlPropertyPersistence::Transient;
			BindingPropertyRegistry::Register<GridView, bool>(L"SortAscending",
				[](GridView& target) { return target.SortAscending; },
				[](GridView& target, const bool& value) { target.SortAscending = value; },
				GridViewPropertySubscriber(L"SortAscending"), std::move(options));
		}

		{
			auto options = GridViewPropertyOptions(
				0, L"Behavior", 110, 120,
				ControlPropertyEditorKind::Number);
			options.Coerce = [](GridView& target, const int& proposed) -> std::optional<int>
			{
				const auto layout = target.CalcScrollLayout();
				return (std::clamp)(proposed, 0, layout.MaxScrollRow);
			};
			options.Changed = [](GridView& target, const int&, const int& value)
			{
				if (target._scrollUpdateInProgress) return;
				target._scrollUpdateInProgress = true;
				target.SetCurrentScrollYOffset(value * target.GetRowHeightPx());
				target._scrollUpdateInProgress = false;
				target.ScrollChanged(&target);
				target.NotifyAccessibilityScrollChanged();
			};
			options.Design.Browsable = false;
			options.Design.Persistence = ControlPropertyPersistence::Transient;
			BindingPropertyRegistry::Register<GridView, int>(L"ScrollRowPosition",
				[](GridView& target) { return target.ScrollRowPosition; },
				[](GridView& target, const int& value) { target.ScrollRowPosition = value; },
				GridViewPropertySubscriber(L"ScrollRowPosition"), std::move(options));
		}

		{
			auto options = GridViewPropertyOptions(
				0.0f, L"Behavior", 110, 130,
				ControlPropertyEditorKind::Number);
			options.Coerce = [](GridView& target, const float& proposed) -> std::optional<float>
			{
				if (!std::isfinite(proposed)) return std::nullopt;
				const auto layout = target.CalcScrollLayout();
				return (std::clamp)(proposed, 0.0f, layout.MaxScrollY);
			};
			options.Changed = [](GridView& target, const float&, const float& value)
			{
				if (target._scrollUpdateInProgress) return;
				target._scrollUpdateInProgress = true;
				const float rowHeight = target.GetRowHeightPx();
				target.SetCurrentScrollRowPosition(rowHeight > 0.0f
					? static_cast<int>(std::floor(value / rowHeight)) : 0);
				target._scrollUpdateInProgress = false;
				target.ScrollChanged(&target);
				target.NotifyAccessibilityScrollChanged();
			};
			options.Design.Browsable = false;
			options.Design.Persistence = ControlPropertyPersistence::Transient;
			BindingPropertyRegistry::Register<GridView, float>(L"ScrollYOffset",
				[](GridView& target) { return target.ScrollYOffset; },
				[](GridView& target, const float& value) { target.ScrollYOffset = value; },
				GridViewPropertySubscriber(L"ScrollYOffset"), std::move(options));
		}
		{
			auto options = GridViewPropertyOptions(
				0.0f, L"Behavior", 110, 140,
				ControlPropertyEditorKind::Number);
			options.Coerce = [](GridView& target, const float& proposed) -> std::optional<float>
			{
				if (!std::isfinite(proposed)) return std::nullopt;
				const auto layout = target.CalcScrollLayout();
				return (std::clamp)(proposed, 0.0f, layout.MaxScrollX);
			};
			options.Changed = [](GridView& target, const float&, const float&)
			{
				target.ScrollChanged(&target);
				target.NotifyAccessibilityScrollChanged();
			};
			options.Design.Browsable = false;
			options.Design.Persistence = ControlPropertyPersistence::Transient;
			BindingPropertyRegistry::Register<GridView, float>(L"ScrollXOffset",
				[](GridView& target) { return target.ScrollXOffset; },
				[](GridView& target, const float& value) { target.ScrollXOffset = value; },
				GridViewPropertySubscriber(L"ScrollXOffset"), std::move(options));
		}

#define CUI_REGISTER_GRID_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<GridView, D2D1_COLOR_F>(propertyName, \
			[](GridView& target) { return target.name; }, \
			[](GridView& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			GridViewPropertySubscriber(propertyName), \
			GridViewColorOptions(defaultValue, order))

		CUI_REGISTER_GRID_COLOR(HeadBackColor, L"HeadBackColor", cui::theme::palette::SurfaceMuted, 10);
		CUI_REGISTER_GRID_COLOR(HeadForeColor, L"HeadForeColor", cui::theme::palette::TextPrimary, 20);
		CUI_REGISTER_GRID_COLOR(HeadHoverBackColor, L"HeadHoverBackColor", cui::theme::palette::AccentSoft, 30);
		CUI_REGISTER_GRID_COLOR(GridLineColor, L"GridLineColor", cui::theme::palette::Border, 40);
		CUI_REGISTER_GRID_COLOR(AccentColor, L"AccentColor", cui::theme::palette::Accent, 50);
		CUI_REGISTER_GRID_COLOR(ButtonBackColor, L"ButtonBackColor", cui::theme::palette::Surface, 60);
		CUI_REGISTER_GRID_COLOR(ButtonCheckedColor, L"ButtonCheckedColor", cui::theme::palette::AccentSelected, 70);
		CUI_REGISTER_GRID_COLOR(ButtonHoverBackColor, L"ButtonHoverBackColor", cui::theme::palette::AccentSoft, 80);
		CUI_REGISTER_GRID_COLOR(ButtonPressedBackColor, L"ButtonPressedBackColor", cui::theme::palette::AccentSelected, 90);
		CUI_REGISTER_GRID_COLOR(ButtonBorderDarkColor, L"ButtonBorderDarkColor", cui::theme::palette::BorderStrong, 100);
		CUI_REGISTER_GRID_COLOR(ButtonBorderLightColor, L"ButtonBorderLightColor", cui::theme::palette::Surface, 110);
		CUI_REGISTER_GRID_COLOR(SelectedItemBackColor, L"SelectedItemBackColor", cui::theme::palette::AccentSelected, 120);
		CUI_REGISTER_GRID_COLOR(SelectedItemForeColor, L"SelectedItemForeColor", cui::theme::palette::TextPrimary, 130);
		CUI_REGISTER_GRID_COLOR(UnderMouseItemBackColor, L"UnderMouseItemBackColor", cui::theme::palette::AccentSoft, 140);
		CUI_REGISTER_GRID_COLOR(UnderMouseItemForeColor, L"UnderMouseItemForeColor", cui::theme::palette::TextPrimary, 150);
		CUI_REGISTER_GRID_COLOR(LinkedTextColor, L"LinkedTextColor", cui::theme::palette::Accent, 160);
		CUI_REGISTER_GRID_COLOR(LinkedTextHoverColor, L"LinkedTextHoverColor", cui::theme::palette::AccentHover, 170);
		CUI_REGISTER_GRID_COLOR(ScrollBackColor, L"ScrollBackColor", cui::theme::palette::ScrollTrack, 180);
		CUI_REGISTER_GRID_COLOR(ScrollForeColor, L"ScrollForeColor", cui::theme::palette::ScrollThumb, 190);
		CUI_REGISTER_GRID_COLOR(EditBackColor, L"EditBackColor", cui::theme::palette::Surface, 200);
		CUI_REGISTER_GRID_COLOR(EditForeColor, L"EditForeColor", cui::theme::palette::TextPrimary, 210);
		CUI_REGISTER_GRID_COLOR(EditSelectedBackColor, L"EditSelectedBackColor", cui::theme::palette::SelectionBack, 220);
		CUI_REGISTER_GRID_COLOR(EditSelectedForeColor, L"EditSelectedForeColor", cui::theme::palette::TextPrimary, 230);
		CUI_REGISTER_GRID_COLOR(NewRowBackColor, L"NewRowBackColor", cui::theme::palette::SurfaceSubtle, 240);
		CUI_REGISTER_GRID_COLOR(NewRowForeColor, L"NewRowForeColor", cui::theme::palette::TextMuted, 250);
		CUI_REGISTER_GRID_COLOR(NewRowIndicatorColor, L"NewRowIndicatorColor", cui::theme::palette::Accent, 260);

#undef CUI_REGISTER_GRID_COLOR
		return true;
	}();
	(void)registered;
}

#define CUI_GRID_VIEW_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(GridView, type, name) { return field; } \
	SET_CPP(GridView, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_GRID_VIEW_PROPERTY_IMPL(bool, FullRowSelect, _fullRowSelect, L"FullRowSelect")
CUI_GRID_VIEW_PROPERTY_IMPL(bool, AllowUserToAddRows, _allowUserToAddRows, L"AllowUserToAddRows")
CUI_GRID_VIEW_PROPERTY_IMPL(bool, AllowUserToDeleteRows, _allowUserToDeleteRows, L"AllowUserToDeleteRows")
CUI_GRID_VIEW_PROPERTY_IMPL(float, HeadHeight, _headHeight, L"HeadHeight")
CUI_GRID_VIEW_PROPERTY_IMPL(float, RowHeight, _rowHeight, L"RowHeight")
CUI_GRID_VIEW_PROPERTY_IMPL(float, BorderThickness, _borderThickness, L"BorderThickness")
CUI_GRID_VIEW_PROPERTY_IMPL(float, CellCornerRadius, _cellCornerRadius, L"CellCornerRadius")
CUI_GRID_VIEW_PROPERTY_IMPL(float, CellHorizontalPadding, _cellHorizontalPadding, L"CellHorizontalPadding")
CUI_GRID_VIEW_PROPERTY_IMPL(float, CellVerticalPadding, _cellVerticalPadding, L"CellVerticalPadding")
CUI_GRID_VIEW_PROPERTY_IMPL(float, SelectedAccentWidth, _selectedAccentWidth, L"SelectedAccentWidth")
CUI_GRID_VIEW_PROPERTY_IMPL(float, EditTextMargin, _editTextMargin, L"EditTextMargin")
CUI_GRID_VIEW_PROPERTY_IMPL(float, ScrollBarSize, _scrollBarSize, L"ScrollBarSize")
CUI_GRID_VIEW_PROPERTY_IMPL(float, ScrollYOffset, _scrollYOffset, L"ScrollYOffset")
CUI_GRID_VIEW_PROPERTY_IMPL(float, ScrollXOffset, _scrollXOffset, L"ScrollXOffset")
CUI_GRID_VIEW_PROPERTY_IMPL(int, ScrollRowPosition, _scrollRowPosition, L"ScrollRowPosition")
CUI_GRID_VIEW_PROPERTY_IMPL(int, SelectedColumnIndex, _selectedColumnIndex, L"SelectedColumnIndex")
CUI_GRID_VIEW_PROPERTY_IMPL(int, SelectedRowIndex, _selectedRowIndex, L"SelectedRowIndex")
CUI_GRID_VIEW_PROPERTY_IMPL(int, SortedColumnIndex, _sortedColumnIndex, L"SortedColumnIndex")
CUI_GRID_VIEW_PROPERTY_IMPL(bool, SortAscending, _sortAscending, L"SortAscending")
CUI_GRID_VIEW_PROPERTY_IMPL(int, UnderMouseColumnIndex, _underMouseColumnIndex, L"UnderMouseColumnIndex")
CUI_GRID_VIEW_PROPERTY_IMPL(int, UnderMouseRowIndex, _underMouseRowIndex, L"UnderMouseRowIndex")
GET_CPP(GridView, bool, MultiSelect) { return _multiSelect; }
SET_CPP(GridView, bool, MultiSelect)
{
	if (!SetPropertyField(L"MultiSelect", _multiSelect, value)) return;
	if (!value)
	{
		// 关闭多选：收敛为仅焦点行，避免集合与单选语义漂移。
		_selectedRows.clear();
		_selectionAnchorRow = -1;
	}
	else if (_selectedRowIndex >= 0)
	{
		// 开启多选：将当前焦点行并入集合，保持视觉连续。
		_selectedRows.insert(_selectedRowIndex);
		_selectionAnchorRow = _selectedRowIndex;
	}
	RequestRefresh(false);
}
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, HeadBackColor, _headBackColor, L"HeadBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, HeadForeColor, _headForeColor, L"HeadForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, HeadHoverBackColor, _headHoverBackColor, L"HeadHoverBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, GridLineColor, _gridLineColor, L"GridLineColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ButtonBackColor, _buttonBackColor, L"ButtonBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ButtonCheckedColor, _buttonCheckedColor, L"ButtonCheckedColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ButtonHoverBackColor, _buttonHoverBackColor, L"ButtonHoverBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ButtonPressedBackColor, _buttonPressedBackColor, L"ButtonPressedBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ButtonBorderDarkColor, _buttonBorderDarkColor, L"ButtonBorderDarkColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ButtonBorderLightColor, _buttonBorderLightColor, L"ButtonBorderLightColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, SelectedItemBackColor, _selectedItemBackColor, L"SelectedItemBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, SelectedItemForeColor, _selectedItemForeColor, L"SelectedItemForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, UnderMouseItemBackColor, _underMouseItemBackColor, L"UnderMouseItemBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, UnderMouseItemForeColor, _underMouseItemForeColor, L"UnderMouseItemForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, LinkedTextColor, _linkedTextColor, L"LinkedTextColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, LinkedTextHoverColor, _linkedTextHoverColor, L"LinkedTextHoverColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ScrollBackColor, _scrollBackColor, L"ScrollBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ScrollForeColor, _scrollForeColor, L"ScrollForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, EditBackColor, _editBackColor, L"EditBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, EditForeColor, _editForeColor, L"EditForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, EditSelectedBackColor, _editSelectedBackColor, L"EditSelectedBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, EditSelectedForeColor, _editSelectedForeColor, L"EditSelectedForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, NewRowBackColor, _newRowBackColor, L"NewRowBackColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, NewRowForeColor, _newRowForeColor, L"NewRowForeColor")
CUI_GRID_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, NewRowIndicatorColor, _newRowIndicatorColor, L"NewRowIndicatorColor")

#undef CUI_GRID_VIEW_PROPERTY_IMPL

GridView::UpdateScope::UpdateScope(GridView& owner) noexcept
	: _owner(&owner)
{
	_owner->BeginUpdate();
}

GridView::UpdateScope::~UpdateScope()
{
	Commit();
}

GridView::UpdateScope::UpdateScope(UpdateScope&& other) noexcept
	: _owner(other._owner)
{
	other._owner = nullptr;
}

GridView::UpdateScope& GridView::UpdateScope::operator=(UpdateScope&& other) noexcept
{
	if (this == &other) return *this;
	Commit();
	_owner = other._owner;
	other._owner = nullptr;
	return *this;
}

void GridView::UpdateScope::Commit() noexcept
{
	if (!_owner) return;
	auto* owner = _owner;
	_owner = nullptr;
	owner->EndUpdate();
}

static D2D1_RECT_F GridInsetRect(float x, float y, float w, float h, float insetX, float insetY)
{
	return D2D1::RectF(x + insetX, y + insetY, x + w - insetX, y + h - insetY);
}

static void DrawGridCellLines(D2DGraphics* d2d, float x, float y, float w, float h, D2D1_COLOR_F color)
{
	if (!d2d || w <= 0.0f || h <= 0.0f) return;
	d2d->DrawLine(x, y + h - 0.5f, x + w, y + h - 0.5f, color, 1.0f);
	d2d->DrawLine(x + w - 0.5f, y + 5.0f, x + w - 0.5f, y + h - 5.0f, color, 1.0f);
}

static void DrawGridCellState(GridView* grid, D2DGraphics* d2d, float x, float y, float w, float h, bool selected, bool hovered)
{
	if (!grid || !d2d || w <= 0.0f || h <= 0.0f) return;
	const float insetX = (std::max)(2.0f, grid->CellHorizontalPadding * 0.35f);
	const float insetY = (std::max)(2.0f, grid->CellVerticalPadding);
	D2D1_RECT_F stateRect = GridInsetRect(x, y, w, h, insetX, insetY);
	if (stateRect.right <= stateRect.left || stateRect.bottom <= stateRect.top) return;
	if (selected)
	{
		d2d->FillRoundRect(stateRect, grid->SelectedItemBackColor, grid->CellCornerRadius);
		const float accentW = (std::max)(2.0f, grid->SelectedAccentWidth);
		const float accentH = (std::max)(5.0f, (stateRect.bottom - stateRect.top) - 10.0f);
		d2d->FillRoundRect(stateRect.left, stateRect.top + 5.0f, accentW, accentH, grid->AccentColor, accentW * 0.5f);
	}
	else if (hovered)
	{
		d2d->FillRoundRect(stateRect, grid->UnderMouseItemBackColor, grid->CellCornerRadius);
	}
}

static void DrawGridLinkedText(GridView* grid, D2DGraphics* d2d, Font* font,
	const std::wstring& text, float x, float y, float w, float h, float textTop, bool hovered)
{
	if (!grid || !d2d || !font || text.empty() || w <= 0.0f || h <= 0.0f) return;
	const float textX = x + grid->CellHorizontalPadding;
	const float maxTextWidth = (std::max)(1.0f, w - grid->CellHorizontalPadding * 2.0f);
	const auto color = hovered ? grid->LinkedTextHoverColor : grid->LinkedTextColor;
	d2d->DrawString(text, textX, y + textTop, maxTextWidth, font->FontHeight + 2.0f, color, font);

	auto textSize = font->GetTextSize(text);
	const float underlineWidth = (std::min)(maxTextWidth, textSize.width);
	if (underlineWidth <= 0.0f) return;
	float underlineY = y + textTop + textSize.height - 1.0f;
	underlineY = (std::min)(underlineY, y + h - 3.0f);
	d2d->DrawLine(textX, underlineY, textX + underlineWidth, underlineY, color, 1.0f);
}

static D2D1_POINT_2F RotateGridPoint(const D2D1_POINT_2F& point, float cx, float cy, float angle)
{
	const float dx = point.x - cx;
	const float dy = point.y - cy;
	const float s = std::sin(angle);
	const float c = std::cos(angle);
	return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
}

static void DrawGridChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
{
	if (!d2d) return;
	progress = std::clamp(progress, 0.0f, 1.0f);
	const float halfW = size * 0.42f;
	const float halfH = size * 0.26f;
	const float angle = progress * 3.14159265359f;
	auto p1 = D2D1::Point2F(cx - halfW, cy - halfH);
	auto p2 = D2D1::Point2F(cx, cy + halfH);
	auto p3 = D2D1::Point2F(cx + halfW, cy - halfH);
	p1 = RotateGridPoint(p1, cx, cy, angle);
	p2 = RotateGridPoint(p2, cx, cy, angle);
	p3 = RotateGridPoint(p3, cx, cy, angle);
	d2d->DrawLine(p1, p2, color, 1.7f);
	d2d->DrawLine(p2, p3, color, 1.7f);
}

static void DrawGridChevron(D2DGraphics* d2d, float cx, float cy, float size, bool up, D2D1_COLOR_F color)
{
	DrawGridChevron(d2d, cx, cy, size, up ? 1.0f : 0.0f, color);
}

bool GridView::HandlesNavigationKey(WPARAM key) const
{
	if (this->Editing)
	{
		switch (key)
		{
		case VK_LEFT:
		case VK_RIGHT:
		case VK_HOME:
		case VK_END:
		case VK_PRIOR:
		case VK_NEXT:
			return true;
		default:
			return false;
		}
	}

	switch (key)
	{
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
	case VK_HOME:
	case VK_END:
		return true;
	default:
		return false;
	}
}
GridView::GridView(int x, int y, int width, int height)
{
	Rows.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnRowsCollectionChanged(change); });
	Columns.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnColumnsCollectionChanged(change); });
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
}

GridView::~GridView()
{
	CloseDropDownEditor(true);
	if (this->_dropDownPopup)
	{
		delete this->_dropDownPopup;
		this->_dropDownPopup = nullptr;
	}
	this->_dropDownPopupColumnIndex = -1;
	this->_dropDownPopupRowIndex = -1;

	auto baseFont = this->Font;
	if (this->HeadFont && this->HeadFont != baseFont && this->HeadFont != GetDefaultFontObject())
	{
		delete this->HeadFont;
	}
	this->HeadFont = nullptr;
}

float GridView::GetTotalColumnsWidth() const
{
	float sum = 0.0f;
	for (int i = 0; i < (int)this->Columns.size(); i++)
	{
		if (_hiddenColumns.find(i) != _hiddenColumns.end()) continue;
		sum += this->Columns[i].Width;
	}
	return sum;
}

// ---- 运行时列管理 ----
bool GridView::IsColumnVisible(int col) const
{
	if (col < 0 || col >= static_cast<int>(Columns.size())) return false;
	return _hiddenColumns.find(col) == _hiddenColumns.end();
}

bool GridView::SetColumnVisible(int col, bool visible)
{
	if (col < 0 || col >= static_cast<int>(Columns.size())) return false;
	bool changed = false;
	if (visible)
		changed = _hiddenColumns.erase(col) > 0;
	else
		changed = _hiddenColumns.insert(col).second;
	if (!changed) return false;
	// 隐藏当前编辑/选中列时，先提交编辑以免悬空。
	if (!visible && Editing && EditingColumnIndex == col)
		CommitEdit();
	RequestRefresh(true);
	return true;
}

bool GridView::MoveColumn(int fromIndex, int toIndex)
{
	const int count = static_cast<int>(Columns.size());
	if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count)
		return false;
	if (fromIndex == toIndex) return false;

	CommitEdit();
	// 移动列定义。
	GridViewColumn column = std::move(Columns[fromIndex]);
	Columns.erase(Columns.begin() + fromIndex);
	Columns.insert(Columns.begin() + toIndex, std::move(column));
	// 同步移动每一行对应单元格。
	for (size_t r = 0; r < Rows.size(); ++r)
	{
		auto& cells = Rows[r].Cells;
		if (fromIndex < static_cast<int>(cells.size()))
		{
			CellValue cell = std::move(cells[fromIndex]);
			cells.erase(cells.begin() + fromIndex);
			const int insertAt = (std::min)(toIndex, static_cast<int>(cells.size()));
			cells.insert(cells.begin() + insertAt, std::move(cell));
		}
	}
	// 重映射隐藏列索引。
	std::set<int> remapped;
	for (int hidden : _hiddenColumns)
	{
		int idx = hidden;
		if (idx == fromIndex) idx = toIndex;
		else if (fromIndex < toIndex && idx > fromIndex && idx <= toIndex) --idx;
		else if (toIndex < fromIndex && idx >= toIndex && idx < fromIndex) ++idx;
		remapped.insert(idx);
	}
	_hiddenColumns = std::move(remapped);
	// 重映射选中/排序列。
	auto remap = [&](int idx) -> int
	{
		if (idx == fromIndex) return toIndex;
		if (fromIndex < toIndex && idx > fromIndex && idx <= toIndex) return idx - 1;
		if (toIndex < fromIndex && idx >= toIndex && idx < fromIndex) return idx + 1;
		return idx;
	};
	if (_selectedColumnIndex >= 0) SetCurrentSelectedColumnIndex(remap(_selectedColumnIndex));
	if (_sortedColumnIndex >= 0) SetCurrentSortedColumnIndex(remap(_sortedColumnIndex));
	RequestRefresh(true);
	return true;
}

GridView::ScrollLayout GridView::CalcScrollLayout() const
{
	ScrollLayout l{};
	l.ScrollBarSize = _scrollBarSize;
	l.HeadHeight = this->GetHeadHeightPx();
	l.RowHeight = this->GetRowHeightPx();
	l.TotalColumnsWidth = GetTotalColumnsWidth();

	bool needV = false;
	bool needH = false;
	for (int iter = 0; iter < 3; iter++)
	{
		float renderW = static_cast<float>(_size.cx) - (needV ? l.ScrollBarSize : 0.0f);
		float renderH = static_cast<float>(_size.cy) - (needH ? l.ScrollBarSize : 0.0f);
		if (renderW < 0.0f) renderW = 0.0f;
		if (renderH < 0.0f) renderH = 0.0f;

		float contentH = renderH - l.HeadHeight;
		if (contentH < 0.0f) contentH = 0.0f;
		int visibleRows = 0;
		if (l.RowHeight > 0.0f && contentH > 0.0f)
			visibleRows = (int)std::ceil(contentH / l.RowHeight) + 1;
		if (visibleRows < 0) visibleRows = 0;

		// 计算新行区域高度（如果有的话）
		float newRowAreaHeight = (_allowUserToAddRows && !Columns.empty()) ? l.RowHeight : 0.0f;
		float totalRowsH = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->Rows.size()) : 0.0f;
		totalRowsH += newRowAreaHeight;  // 加上新行区域高度

		bool newNeedV = (totalRowsH > contentH);
		bool newNeedH = (l.TotalColumnsWidth > renderW);

		if (newNeedV == needV && newNeedH == needH)
		{
			l.NeedV = needV;
			l.NeedH = needH;
			l.RenderWidth = renderW;
			l.RenderHeight = renderH;
			l.ContentHeight = contentH;
			l.TotalRowsHeight = totalRowsH;
			l.MaxScrollY = std::max(0.0f, totalRowsH - contentH);
			l.VisibleRows = visibleRows;
			l.MaxScrollRow = std::max(0, (int)this->Rows.size() - visibleRows);
			l.MaxScrollX = std::max(0.0f, l.TotalColumnsWidth - renderW);
			return l;
		}
		needV = newNeedV;
		needH = newNeedH;
	}

	l.NeedV = needV;
	l.NeedH = needH;
	l.RenderWidth = static_cast<float>(_size.cx) - (needV ? l.ScrollBarSize : 0.0f);
	l.RenderHeight = static_cast<float>(_size.cy) - (needH ? l.ScrollBarSize : 0.0f);
	float contentH = l.RenderHeight - l.HeadHeight;
	if (contentH < 0.0f) contentH = 0.0f;
	l.ContentHeight = contentH;

	// 计算新行区域高度
	float newRowAreaHeight = (_allowUserToAddRows && !Columns.empty()) ? l.RowHeight : 0.0f;
	l.TotalRowsHeight = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->Rows.size()) : 0.0f;
	l.TotalRowsHeight += newRowAreaHeight;  // 加上新行区域高度
	l.MaxScrollY = std::max(0.0f, l.TotalRowsHeight - contentH);
	l.VisibleRows = (l.RowHeight > 0.0f && contentH > 0.0f) ? ((int)std::ceil(contentH / l.RowHeight) + 1) : 0;
	if (l.VisibleRows < 0) l.VisibleRows = 0;
	l.MaxScrollRow = std::max(0, (int)this->Rows.size() - l.VisibleRows);
	l.MaxScrollX = std::max(0.0f, l.TotalColumnsWidth - l.RenderWidth);
	return l;
}

CursorKind GridView::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;
	if (this->_resizingColumn) return CursorKind::SizeWE;

	{
		auto l = this->CalcScrollLayout();
		const int renderW = (int)l.RenderWidth;
		const int renderH = (int)l.RenderHeight;
		if (l.NeedH && localY >= renderH && localX >= 0 && localX < renderW)
			return CursorKind::SizeWE;
		if (l.NeedV && localX >= renderW && localY >= 0 && localY < renderH)
			return CursorKind::SizeNS;
	}

	if (HitTestHeaderDivider(localX, localY) >= 0)
		return CursorKind::SizeWE;

	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
		if (undermouseIndex.y >= 0 && undermouseIndex.x >= 0 &&
			undermouseIndex.y < static_cast<LONG>(this->Rows.size()) && undermouseIndex.x < static_cast<LONG>(this->Columns.size()))
		{
			auto type = this->Columns[static_cast<size_t>(undermouseIndex.x)].Type;
			if (type == ColumnType::Button || type == ColumnType::Check || type == ColumnType::ComboBox || type == ColumnType::LinkedText)
				return CursorKind::Hand;
			if (IsEditableTextCell(undermouseIndex.x, undermouseIndex.y))
				return CursorKind::IBeam;
		}
	}

	if (this->Editing && this->IsSelected())
	{
		D2D1_RECT_F rect{};
		if (TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
		{
			if ((float)localX >= rect.left && (float)localX <= rect.right &&
				(float)localY >= rect.top && (float)localY <= rect.bottom)
			{
				return CursorKind::IBeam;
			}
		}
	}

	// 检查是否在新行区域
	if (this->AllowUserToAddRows)
	{
		int newRowCol = -1;
		if (HitTestNewRow(localX, localY, newRowCol) >= 0 && newRowCol >= 0)
		{
			return CursorKind::IBeam;  // 在新行区域显示IBeam光标表示可以编辑
		}
	}

	return CursorKind::Arrow;
}
GridViewRow& GridView::operator[](int index)
{
	return Rows[index];
}

void GridView::BeginUpdate() noexcept
{
	if (_updateDepth++ == 0)
	{
		// Grid invariants (especially column-to-cell alignment) must advance for
		// every mutation, while public collection observers still receive one
		// coalesced Reset when the batch completes.
		Rows.BeginUpdate(true);
		Columns.BeginUpdate(true);
	}
}

void GridView::EndUpdate() noexcept
{
	if (_updateDepth == 0) return;
	if (_updateDepth > 1)
	{
		--_updateDepth;
		return;
	}
	// The collections keep Grid internals synchronized for every mutation but
	// publish their coalesced Reset only below. Rebuild the logical-id index now
	// so public observers see the final row/column structure. Keep EndUpdate's
	// no-throw contract: a failed allocation leaves the index dirty and the next
	// accessibility query can retry the lazy rebuild.
	try
	{
		EnsureAccessibilityIds();
	}
	catch (...)
	{
		_accessibilityIdsDirty = true;
	}
	// Apply column changes first so every public row notification observes
	// cells that are already aligned with the final column collection.
	Columns.EndUpdate();
	Rows.EndUpdate();
	_updateDepth = 0;

	const bool adjustScroll = _updatePendingAdjustScroll;
	const bool invalidate = _updatePendingInvalidate;
	_updatePendingAdjustScroll = false;
	_updatePendingInvalidate = false;
	if (_collectionStructurePending)
	{
		_collectionStructurePending = false;
		NotifyAccessibilityStructureChanged();
		NotifyAccessibilityScrollChanged();
	}
	if (adjustScroll)
		AdjustScrollPosition();
	if (invalidate)
		InvalidateVisual();
}

void GridView::RequestRefresh(bool adjustScroll)
{
	if (_updateDepth > 0)
	{
		_updatePendingAdjustScroll = _updatePendingAdjustScroll || adjustScroll;
		_updatePendingInvalidate = true;
		return;
	}
	if (adjustScroll)
		AdjustScrollPosition();
	InvalidateVisual();
}

void GridView::SetCurrentSelectedColumnIndex(int value)
{
	if (_selectedColumnIndex == value) return;
	(void)SetCurrentPropertyField(L"SelectedColumnIndex", _selectedColumnIndex, value);
}

void GridView::SetCurrentSelectedRowIndex(int value)
{
	if (_selectedRowIndex == value) return;
	(void)SetCurrentPropertyField(L"SelectedRowIndex", _selectedRowIndex, value);
}

void GridView::SetCurrentUnderMouseColumnIndex(int value)
{
	if (_underMouseColumnIndex == value) return;
	(void)SetCurrentPropertyField(L"UnderMouseColumnIndex", _underMouseColumnIndex, value);
}

void GridView::SetCurrentUnderMouseRowIndex(int value)
{
	if (_underMouseRowIndex == value) return;
	(void)SetCurrentPropertyField(L"UnderMouseRowIndex", _underMouseRowIndex, value);
}

void GridView::SetCurrentScrollYOffset(float value)
{
	if (std::fabs(_scrollYOffset - value) <= 1e-6f) return;
	(void)SetCurrentPropertyField(L"ScrollYOffset", _scrollYOffset, value);
}

void GridView::SetCurrentScrollXOffset(float value)
{
	if (std::fabs(_scrollXOffset - value) <= 1e-6f) return;
	(void)SetCurrentPropertyField(L"ScrollXOffset", _scrollXOffset, value);
}

void GridView::SetCurrentScrollRowPosition(int value)
{
	if (_scrollRowPosition == value) return;
	(void)SetCurrentPropertyField(L"ScrollRowPosition", _scrollRowPosition, value);
}

void GridView::SetCurrentSortedColumnIndex(int value)
{
	if (_sortedColumnIndex == value) return;
	(void)SetCurrentPropertyField(L"SortedColumnIndex", _sortedColumnIndex, value);
}

void GridView::SetCurrentSortAscending(bool value)
{
	if (_sortAscending == value) return;
	(void)SetCurrentPropertyField(L"SortAscending", _sortAscending, value);
}

void GridView::OnSelectionPropertyChanged()
{
	if (_selectionUpdateInProgress) return;
	CaptureSelectionAccessibilityIds();
	AdjustScrollPosition();
	SelectionChanged(this);
	RequestRefresh(false);
}

void GridView::SetCurrentSelection(
	int col, int row, bool ensureVisible, bool raiseEvent)
{
	const bool changed = _selectedColumnIndex != col || _selectedRowIndex != row;
	if (!changed)
	{
		CaptureSelectionAccessibilityIds();
		if (ensureVisible) AdjustScrollPosition();
		return;
	}
	_selectionUpdateInProgress = true;
	SetCurrentSelectedColumnIndex(col);
	SetCurrentSelectedRowIndex(row);
	_selectionUpdateInProgress = false;
	CaptureSelectionAccessibilityIds();
	if (ensureVisible)
		AdjustScrollPosition();
	if (raiseEvent)
		SelectionChanged(this);
	RequestRefresh(false);
}

void GridView::ClearRows()
{
	this->Rows.clear();
}

void GridView::ClearColumns()
{
	this->Columns.clear();
}

void GridView::AddRow(const GridViewRow& row)
{
	this->Rows.push_back(row);
}

void GridView::SetRows(const std::vector<GridViewRow>& rows)
{
	std::vector<GridViewRow> copy = rows;
	SetRows(std::move(copy));
}

void GridView::SetRows(std::vector<GridViewRow>&& rows)
{
	CommitEdit();
	// 一次批量更新：整个替换只触发最外层的一次重排/重绘/可访问性通知。
	auto scope = DeferUpdates();
	this->Rows.clear();
	this->Rows.reserve(rows.size());
	for (auto& row : rows)
		this->Rows.push_back(std::move(row));
	// 清空选择锚点，避免指向已失效的行索引。
	_selectedRows.clear();
	_selectionAnchorRow = -1;
}

void GridView::AddColumn(const GridViewColumn& column)
{
	this->Columns.push_back(column);
}

size_t GridView::RowCount() const
{
	return this->Rows.size();
}

size_t GridView::ColumnCount() const
{
	return this->Columns.size();
}

GridViewRow& GridView::RowAt(int index)
{
	return this->Rows.at((size_t)index);
}

const GridViewRow& GridView::RowAt(int index) const
{
	return this->Rows.at((size_t)index);
}

GridViewColumn& GridView::ColumnAt(int index)
{
	return this->Columns.at((size_t)index);
}

const GridViewColumn& GridView::ColumnAt(int index) const
{
	return this->Columns.at((size_t)index);
}

void GridView::SwapRows(int indexA, int indexB)
{
	if (indexA < 0 || indexB < 0) return;
	if (indexA >= (int)this->Rows.size() || indexB >= (int)this->Rows.size()) return;
	if (indexA == indexB) return;
	(void)Rows.SwapIndices(
		static_cast<size_t>(indexA), static_cast<size_t>(indexB));
}

bool GridView::RemoveRowAt(int index)
{
	if (index < 0 || index >= (int)this->Rows.size()) return false;
	this->Rows.erase(this->Rows.begin() + index);
	return true;
}

bool GridView::RemoveColumnAt(int index)
{
	if (index < 0 || index >= static_cast<int>(this->Columns.size())) return false;
	this->Columns.erase(this->Columns.begin() + index);
	return true;
}

CellValue* GridView::GetCell(int col, int row) noexcept
{
	if (col < 0 || row < 0 ||
		col >= static_cast<int>(Columns.size()) ||
		row >= static_cast<int>(Rows.size())) return nullptr;
	auto& cells = Rows[static_cast<size_t>(row)].Cells;
	if (col >= static_cast<int>(cells.size())) return nullptr;
	return &cells[static_cast<size_t>(col)];
}

const CellValue* GridView::GetCell(int col, int row) const noexcept
{
	if (col < 0 || row < 0 ||
		col >= static_cast<int>(Columns.size()) ||
		row >= static_cast<int>(Rows.size())) return nullptr;
	const auto& cells = Rows[static_cast<size_t>(row)].Cells;
	if (col >= static_cast<int>(cells.size())) return nullptr;
	return &cells[static_cast<size_t>(col)];
}

bool GridView::SetCellValue(int col, int row, const CellValue& value)
{
	if (col < 0 || row < 0 ||
		col >= static_cast<int>(Columns.size()) ||
		row >= static_cast<int>(Rows.size()))
		return false;
	auto& cells = Rows[static_cast<size_t>(row)].Cells;
	if (cells.size() <= static_cast<size_t>(col))
		cells.resize(static_cast<size_t>(col) + 1);
	const uint32_t accessibilityId =
		cells[static_cast<size_t>(col)].AccessibilityId;
	cells[static_cast<size_t>(col)] = value;
	if (accessibilityId != 0)
		cells[static_cast<size_t>(col)].AccessibilityId = accessibilityId;
	if (Editing && EditingColumnIndex == col && EditingRowIndex == row)
	{
		EditingText = value.Text;
		EditingOriginalText = value.Text;
		EditSelectionStart = EditSelectionEnd = static_cast<int>(EditingText.size());
	}
	RequestRefresh(false);
	return true;
}

bool GridView::SelectCell(int col, int row, bool ensureVisible)
{
	if (col < 0 || row < 0 ||
		col >= static_cast<int>(Columns.size()) ||
		row >= static_cast<int>(Rows.size()))
		return false;
	if (Editing && (EditingColumnIndex != col || EditingRowIndex != row))
		CommitEdit();
	SetCurrentSelection(col, row, ensureVisible);
	return true;
}

bool GridView::SelectRow(int row, bool ensureVisible)
{
	if (row < 0 || row >= static_cast<int>(Rows.size()) || Columns.empty())
		return false;
	const int col = SelectedColumnIndex >= 0 &&
		SelectedColumnIndex < static_cast<int>(Columns.size())
		? SelectedColumnIndex : 0;
	return SelectCell(col, row, ensureVisible);
}

bool GridView::ClearSelection()
{
	CommitEdit();
	const bool hadMulti = !_selectedRows.empty();
	_selectedRows.clear();
	_selectionAnchorRow = -1;
	if (SelectedColumnIndex < 0 && SelectedRowIndex < 0)
	{
		if (hadMulti) { SelectionChanged(this); RequestRefresh(false); return true; }
		return false;
	}
	SetCurrentSelection(-1, -1, false);
	return true;
}

// ---- 多选 API ----
std::vector<int> GridView::GetSelectedRows() const
{
	if (!_multiSelect)
	{
		std::vector<int> result;
		if (_selectedRowIndex >= 0 && _selectedRowIndex < static_cast<int>(Rows.size()))
			result.push_back(_selectedRowIndex);
		return result;
	}
	return std::vector<int>(_selectedRows.begin(), _selectedRows.end());
}

int GridView::GetSelectedRowCount() const
{
	if (!_multiSelect)
		return (_selectedRowIndex >= 0 && _selectedRowIndex < static_cast<int>(Rows.size())) ? 1 : 0;
	return static_cast<int>(_selectedRows.size());
}

bool GridView::IsRowSelected(int row) const
{
	if (row < 0 || row >= static_cast<int>(Rows.size())) return false;
	if (!_multiSelect)
		return row == _selectedRowIndex;
	return _selectedRows.find(row) != _selectedRows.end();
}

bool GridView::SetRowSelected(int row, bool selected, bool ensureVisible)
{
	if (row < 0 || row >= static_cast<int>(Rows.size())) return false;
	if (!_multiSelect)
	{
		if (selected) return SelectRow(row, ensureVisible);
		if (row == _selectedRowIndex) { ClearSelection(); return true; }
		return false;
	}

	bool changed = false;
	if (selected)
		changed = _selectedRows.insert(row).second;
	else
		changed = _selectedRows.erase(row) > 0;
	if (!changed) return false;

	// 保持焦点行与集合一致：选中时聚焦到该行，取消时若焦点被移除则迁移。
	if (selected)
	{
		SetCurrentSelectedRowIndex(row);
		_selectionAnchorRow = row;
		if (SelectedColumnIndex < 0 && !Columns.empty())
			SetCurrentSelectedColumnIndex(0);
	}
	else if (_selectedRowIndex == row)
	{
		const int next = _selectedRows.empty() ? -1 : *_selectedRows.begin();
		SetCurrentSelectedRowIndex(next);
		_selectionAnchorRow = next;
	}
	if (ensureVisible) AdjustScrollPosition();
	SelectionChanged(this);
	RequestRefresh(false);
	return true;
}

bool GridView::SelectRowRange(int startRow, int endRow, bool ensureVisible)
{
	if (Rows.empty()) return false;
	const int maxRow = static_cast<int>(Rows.size()) - 1;
	startRow = (std::clamp)(startRow, 0, maxRow);
	endRow = (std::clamp)(endRow, 0, maxRow);
	if (startRow > endRow) std::swap(startRow, endRow);
	if (!_multiSelect)
		return SelectRow(endRow, ensureVisible);

	bool changed = false;
	for (int r = startRow; r <= endRow; ++r)
		changed = _selectedRows.insert(r).second || changed;
	if (changed)
	{
		SetCurrentSelectedRowIndex(endRow);
		if (SelectedColumnIndex < 0 && !Columns.empty())
			SetCurrentSelectedColumnIndex(0);
		if (ensureVisible) AdjustScrollPosition();
		SelectionChanged(this);
		RequestRefresh(false);
	}
	return changed;
}

void GridView::SelectAllRows()
{
	if (Rows.empty()) return;
	if (!_multiSelect) { SelectRow(0, false); return; }
	bool changed = false;
	const int count = static_cast<int>(Rows.size());
	for (int r = 0; r < count; ++r)
		changed = _selectedRows.insert(r).second || changed;
	if (changed)
	{
		SelectionChanged(this);
		RequestRefresh(false);
	}
}

GridViewRow& GridView::SelectedRow()
{
	static GridViewRow default_;
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < static_cast<int>(this->Rows.size()))
	{
		return this->Rows[static_cast<size_t>(this->SelectedRowIndex)];
	}
	return default_;
}
std::wstring& GridView::SelectedValue()
{
	static std::wstring default_;
	auto* cell = GetCell(SelectedColumnIndex, SelectedRowIndex);
	if (cell) return cell->Text;
	return default_;
}
void GridView::Clear()
{
	auto update = DeferUpdates();
	ClearRows();
	ClearColumns();
}

static int CompareWStringDefault(const std::wstring& a, const std::wstring& b)
{
	if (a == b) return 0;
	return (a < b) ? -1 : 1;
}

static std::wstring CellToStringDefault(const CellValue* v)
{
	if (!v) return L"";
	if (!v->Text.empty()) return v->Text;
	return std::to_wstring((__int64)v->Tag);
}

void GridView::SortByColumn(int col, bool ascending)
{
	if (col < 0 || col >= static_cast<int>(this->Columns.size())) return;

	CommitEdit();

	if (this->SortRequestHandler && this->SortRequestHandler(this, col, ascending))
		return;

	if (this->Rows.size() <= 1) return;

	const auto sortFunc = this->Columns[col].SortFunc;
	this->Rows.Sort(
		[&](const GridViewRow& a, const GridViewRow& b) -> bool
		{
			const int aCount = (int)a.Cells.size();
			const int bCount = (int)b.Cells.size();
			const CellValue* av = (aCount > col) ? (a.Cells.data() + col) : nullptr;
			const CellValue* bv = (bCount > col) ? (b.Cells.data() + col) : nullptr;

			int cmp = 0;
			if (sortFunc)
			{
				static CellValue empty;
				cmp = sortFunc(av ? *av : empty, bv ? *bv : empty);
			}
			else
			{
				cmp = CompareWStringDefault(CellToStringDefault(av), CellToStringDefault(bv));
			}

			if (ascending)
				return cmp < 0;
			return cmp > 0;
		});

	SetCurrentSortedColumnIndex(col);
	SetCurrentSortAscending(ascending);
}
#pragma region _GridView_
POINT GridView::GetGridViewUnderMouseItem(int x, int y, GridView* ct)
{
	auto layout = ct->CalcScrollLayout();
	float renderWidth = layout.RenderWidth;
	float renderHeight = layout.RenderHeight;
	if (x < 0 || y < 0) return { -1,-1 };
	if (x >= (int)renderWidth || y >= (int)renderHeight) return { -1,-1 };

	float rowHeight = layout.RowHeight;
	float headerHeight = layout.HeadHeight;
	if (y < headerHeight)
	{
		return { -1,-1 };
	}
	float virtualX = (float)x + ct->ScrollXOffset;
	int columnIndex = -1;
	int rowIndex = -1;
	float columnLeft = 0.0f;
	for (unsigned int column = 0; column < ct->Columns.size(); column++)
	{
		if (ct->_hiddenColumns.find(static_cast<int>(column)) != ct->_hiddenColumns.end())
			continue;
		float cellWidth = ct->Columns[column].Width;
		if (virtualX >= columnLeft && virtualX < columnLeft + cellWidth)
		{
			columnIndex = column;
			break;
		}
		columnLeft += ct->Columns[column].Width;
	}
	const float virtualY = ((float)y - headerHeight) + ct->ScrollYOffset;
	if (virtualY >= 0.0f && rowHeight > 0.0f)
	{
		const int rowIndexFromOffset = (int)(virtualY / rowHeight);
		if (rowIndexFromOffset >= 0 && rowIndexFromOffset < static_cast<int>(ct->Rows.size())) rowIndex = rowIndexFromOffset;
	}
	return { columnIndex,rowIndex };
}

int GridView::HitTestHeaderColumn(int x, int y)
{
	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;
	if (y < 0 || y >= (int)headHeight) return -1;
	if (x < 0 || x >= (int)renderWidth) return -1;

	const float virtualX = (float)x + this->ScrollXOffset;
	float xf = 0.0f;
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		if (_hiddenColumns.find(i) != _hiddenColumns.end()) continue;
		float cWidth = this->Columns[static_cast<size_t>(i)].Width;
		if (virtualX >= xf && virtualX < xf + cWidth)
			return i;
		xf += this->Columns[static_cast<size_t>(i)].Width;
	}
	return -1;
}

int GridView::HitTestHeaderDivider(int x, int y)
{
	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;
	if (y < 0 || y >= (int)headHeight) return -1;
	if (x < 0 || x >= (int)renderWidth) return -1;

	const float hitPx = 3.0f;
	const float virtualX = (float)x + this->ScrollXOffset;
	float xf = 0.0f;
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		if (_hiddenColumns.find(i) != _hiddenColumns.end()) continue;
		const float cWidth = this->Columns[static_cast<size_t>(i)].Width;
		const float rightEdge = xf + cWidth;
		if (std::abs(virtualX - rightEdge) <= hitPx)
			return i;

		xf += this->Columns[static_cast<size_t>(i)].Width;
	}
	return -1;
}
D2D1_RECT_F GridView::GetGridViewScrollBlockRect(GridView* ct)
{
	const auto absoluteLocation = ct->GetAbsoluteLocationDip();
	auto layout = ct->CalcScrollLayout();
	float renderWidth = layout.RenderWidth;
	float renderHeight = layout.RenderHeight;
	float rowHeight = layout.RowHeight;
	float headerHeight = layout.HeadHeight;
	const float viewportBodyHeight = std::max(0.0f, renderHeight - headerHeight);
	const float totalRowsHeight = (rowHeight > 0.0f) ? (rowHeight * (float)ct->Rows.size()) : 0.0f;
	if (totalRowsHeight > viewportBodyHeight && viewportBodyHeight > 0.0f)
	{
		float thumbHeight = renderHeight * (viewportBodyHeight / totalRowsHeight);
		const float minThumbH = renderHeight * 0.1f;
		if (thumbHeight < minThumbH) thumbHeight = minThumbH;
		if (thumbHeight > renderHeight) thumbHeight = renderHeight;

		const float maxScrollY = std::max(0.0f, totalRowsHeight - viewportBodyHeight);
		const float thumbMoveSpace = std::max(0.0f, renderHeight - thumbHeight);
		float scrollRatio = 0.0f;
		if (maxScrollY > 0.0f)
			scrollRatio = std::clamp(ct->ScrollYOffset / maxScrollY, 0.0f, 1.0f);
		const float thumbTop = scrollRatio * thumbMoveSpace;
		return { (float)absoluteLocation.x + renderWidth, (float)absoluteLocation.y + thumbTop, layout.ScrollBarSize, thumbHeight };
	}
	return { 0,0,0,0 };
}
int GridView::GetGridViewRenderRowCount(GridView* ct)
{
	auto l = ct->CalcScrollLayout();
	return l.VisibleRows;
}
void GridView::DrawScroll()
{
	auto d2d = this->ParentForm->Render;

	auto l = this->CalcScrollLayout();

	if (l.NeedV && this->Rows.size() > 0)
	{
		float renderWidth = l.RenderWidth;
		float renderHeight = l.RenderHeight;
		const float rowHeight = this->GetRowHeightPx();
		const float headerHeight = this->GetHeadHeightPx();
		const float viewportBodyHeight = std::max(0.0f, renderHeight - headerHeight);
		const float totalRowsHeight = (rowHeight > 0.0f) ? (rowHeight * (float)this->Rows.size()) : 0.0f;
		if (totalRowsHeight > viewportBodyHeight && viewportBodyHeight > 0.0f)
		{
			float thumbHeight = renderHeight * (viewportBodyHeight / totalRowsHeight);
			const float minThumbH = renderHeight * 0.1f;
			if (thumbHeight < minThumbH) thumbHeight = minThumbH;
			if (thumbHeight > renderHeight) thumbHeight = renderHeight;

			const float maxScrollY = std::max(0.0f, totalRowsHeight - viewportBodyHeight);
			const float thumbMoveSpace = std::max(0.0f, renderHeight - thumbHeight);
			float scrollRatio = 0.0f;
			if (maxScrollY > 0.0f)
				scrollRatio = std::clamp(this->ScrollYOffset / maxScrollY, 0.0f, 1.0f);
			const float thumbTop = scrollRatio * thumbMoveSpace;

			const float barW = (std::max)(5.0f, l.ScrollBarSize - 2.0f);
			const float barX = renderWidth + (l.ScrollBarSize - barW) * 0.5f;
			d2d->FillRoundRect(barX, 4.0f, barW, (std::max)(0.0f, renderHeight - 8.0f), this->ScrollBackColor, barW * 0.5f);
			d2d->FillRoundRect(barX, thumbTop + 2.0f, barW, (std::max)(4.0f, thumbHeight - 4.0f), this->ScrollForeColor, barW * 0.5f);
		}
	}

	if (l.NeedH)
		DrawHScroll(l);
	if (l.NeedH && l.NeedV)
		DrawCorner(l);
}

void GridView::DrawHScroll(const ScrollLayout& l)
{
	auto d2d = this->ParentForm->Render;

	const float barX = 0.0f;
	const float barY = l.RenderHeight;
	const float barW = l.RenderWidth;
	const float barH = l.ScrollBarSize;

	if (barW <= 0.0f || barH <= 0.0f) return;
	if (l.TotalColumnsWidth <= barW) return;

	const float maxScrollX = std::max(0.0f, l.TotalColumnsWidth - barW);
	float scrollRatio = 0.0f;
	if (maxScrollX > 0.0f)
		scrollRatio = std::clamp(this->ScrollXOffset / maxScrollX, 0.0f, 1.0f);

	float thumbWidth = (barW * barW) / l.TotalColumnsWidth;
	const float minThumbW = barW * 0.1f;
	if (thumbWidth < minThumbW) thumbWidth = minThumbW;
	if (thumbWidth > barW) thumbWidth = barW;

	const float thumbMoveSpace = barW - thumbWidth;
	const float thumbX = barX + (scrollRatio * thumbMoveSpace);

	const float trackH = (std::max)(5.0f, barH - 2.0f);
	const float trackY = barY + (barH - trackH) * 0.5f;
	d2d->FillRoundRect(barX + 4.0f, trackY, (std::max)(0.0f, barW - 8.0f), trackH, this->ScrollBackColor, trackH * 0.5f);
	d2d->FillRoundRect(thumbX + 2.0f, trackY, (std::max)(4.0f, thumbWidth - 4.0f), trackH, this->ScrollForeColor, trackH * 0.5f);
}

void GridView::DrawCorner(const ScrollLayout& l)
{
	auto d2d = this->ParentForm->Render;
	const float x = l.RenderWidth;
	const float y = l.RenderHeight;
	d2d->FillRoundRect(x + 1.0f, y + 1.0f, l.ScrollBarSize - 2.0f, l.ScrollBarSize - 2.0f, this->ScrollBackColor, 3.0f);
}

void GridView::SetScrollByPos(float localY)
{
	const int rowCount = static_cast<int>(this->Rows.size());
	if (rowCount == 0) return;

	auto l = this->CalcScrollLayout();
	const float renderingHeight = l.RenderHeight;
	const float rowHeight = this->GetRowHeightPx();
	const float headHeight = this->GetHeadHeightPx();
	const float contentHeight = std::max(0.0f, renderingHeight - headHeight);
	const float totalHeight = (rowHeight > 0.0f) ? (rowHeight * (float)rowCount) : 0.0f;
	const float maxScrollY = std::max(0.0f, totalHeight - contentHeight);

	if (maxScrollY > 0.0f && contentHeight > 0.0f)
	{
		float thumbHeight = renderingHeight * (contentHeight / totalHeight);
		const float minThumbH = renderingHeight * 0.1f;
		if (thumbHeight < minThumbH) thumbHeight = minThumbH;
		if (thumbHeight > renderingHeight) thumbHeight = renderingHeight;

		const float thumbMoveSpace = std::max(0.0f, renderingHeight - thumbHeight);
		float thumbGrabOffset = std::clamp(_vScrollThumbGrabOffsetY, 0.0f, thumbHeight);
		if (thumbGrabOffset <= 0.0f) thumbGrabOffset = thumbHeight * 0.5f;
		float targetThumbTop = localY - thumbGrabOffset;
		targetThumbTop = std::clamp(targetThumbTop, 0.0f, thumbMoveSpace);
		const float scrollRatio = (thumbMoveSpace > 0.0f) ? (targetThumbTop / thumbMoveSpace) : 0.0f;
		SetCurrentScrollYOffset(std::clamp(scrollRatio * maxScrollY, 0.0f, maxScrollY));
	}
	else
	{
		SetCurrentScrollYOffset(0.0f);
	}
}

static bool IsGridCellSelected(GridView* grid, int col, int row)
{
	if (!grid) return false;
	// 多选：整行按集合判定。
	if (grid->MultiSelect)
		return grid->IsRowSelected(row) && (grid->FullRowSelect || col == grid->SelectedColumnIndex);
	if (row != grid->SelectedRowIndex) return false;
	return grid->FullRowSelect || col == grid->SelectedColumnIndex;
}

void GridView::SetHScrollByPos(float localX)
{
	auto l = this->CalcScrollLayout();
	if (!l.NeedH) return;
	if (l.TotalColumnsWidth <= l.RenderWidth) { SetCurrentScrollXOffset(0.0f); return; }

	const float barW = l.RenderWidth;
	const float maxScrollX = std::max(0.0f, l.TotalColumnsWidth - barW);
	if (maxScrollX <= 0.0f) { SetCurrentScrollXOffset(0.0f); return; }

	float thumbWidth = (barW * barW) / l.TotalColumnsWidth;
	const float minThumbW = barW * 0.1f;
	if (thumbWidth < minThumbW) thumbWidth = minThumbW;
	if (thumbWidth > barW) thumbWidth = barW;

	const float thumbMoveSpace = barW - thumbWidth;
	if (thumbMoveSpace <= 0.0f) { SetCurrentScrollXOffset(0.0f); return; }

	float thumbGrabOffset = std::clamp(_hScrollThumbGrabOffsetX, 0.0f, thumbWidth);
	if (thumbGrabOffset <= 0.0f) thumbGrabOffset = thumbWidth * 0.5f;
	float targetThumbLeft = localX - thumbGrabOffset;
	targetThumbLeft = std::clamp(targetThumbLeft, 0.0f, thumbMoveSpace);
	float scrollRatio = targetThumbLeft / thumbMoveSpace;
	SetCurrentScrollXOffset(std::clamp(scrollRatio * maxScrollX, 0.0f, maxScrollX));
}

void GridView::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	bool caretBlinkStateUpdated = false;
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(this->CellCornerRadius);
		}
		auto font = this->Font;
		auto headerFont = HeadFont ? HeadFont : font;
		{
			auto l = this->CalcScrollLayout();
			float renderWidth = l.RenderWidth;
			float renderHeight = l.RenderHeight;
			float fontHeight = font->FontHeight;
			float headerFontHeight = headerFont->FontHeight;
			float rowHeight = l.RowHeight;
			float textTop = (rowHeight - fontHeight) * 0.5f;
			if (textTop < 0) textTop = 0;
			SetCurrentScrollYOffset((std::clamp)(this->ScrollYOffset, 0.0f, l.MaxScrollY));
			SetCurrentScrollXOffset((std::clamp)(this->ScrollXOffset, 0.0f, l.MaxScrollX));

			int startColumn = 0;
			int startRow = this->ScrollRowPosition;
			float headerHeight = l.HeadHeight;
			float rowOffset = (rowHeight > 0.0f) ? std::fmod(this->ScrollYOffset, rowHeight) : 0.0f;
			float yf = headerHeight - rowOffset;
			float xf = -this->ScrollXOffset;
			int i = startColumn;
			for (; i < static_cast<int>(this->Columns.size()); i++)
			{
				if (_hiddenColumns.find(i) != _hiddenColumns.end()) continue;
				float colW = this->Columns[i].Width;
				if (xf >= renderWidth) break;
				if (xf + colW <= 0.0f) { xf += colW; continue; }

				float drawX = xf;
				float cellWidth = colW;
				if (drawX < 0.0f) { cellWidth += drawX; drawX = 0.0f; }
				if (drawX + cellWidth > renderWidth) cellWidth = renderWidth - drawX;
				if (cellWidth <= 0.0f) { xf += colW; continue; }
				const float clipX = drawX;
				const float clipW = cellWidth;
				drawX = xf;
				cellWidth = colW;

				float headerTextOffsetY = (headerHeight - headerFontHeight) / 2.0f;
				if (headerTextOffsetY < 0)headerTextOffsetY = 0;
				d2d->PushDrawRect(clipX, 0, clipW, headerHeight);
				{
					const bool sorted = (i == this->SortedColumnIndex);
					D2D1_RECT_F headRect = D2D1::RectF(drawX + 2.0f, 3.0f, drawX + cellWidth - 2.0f, headerHeight - 3.0f);
					d2d->FillRoundRect(headRect, sorted ? this->HeadHoverBackColor : this->HeadBackColor, this->CellCornerRadius);
					if (sorted)
					{
						d2d->FillRoundRect(headRect.left + 6.0f, headRect.bottom - 3.0f,
							(std::max)(6.0f, (headRect.right - headRect.left) - 12.0f), 2.0f,
							this->AccentColor, 1.0f);
					}
					DrawGridCellLines(d2d, drawX, 0.0f, cellWidth, headerHeight, this->GridLineColor);
					const float sortReserve = sorted ? 16.0f : 0.0f;
					const float textWidth = (std::max)(1.0f, cellWidth - 16.0f - sortReserve);
					d2d->DrawString(this->Columns[i].Name,
						drawX + 8.0f,
						headerTextOffsetY,
						textWidth,
						headerFontHeight + 2.0f,
						this->HeadForeColor, headerFont);
					if (sorted)
					{
						DrawGridChevron(d2d, drawX + cellWidth - 11.0f, headerHeight * 0.5f, 9.0f,
							this->SortAscending, this->AccentColor);
					}
				}
				d2d->PopDrawRect();
				xf += colW;
			}

			const int maxRows = l.VisibleRows;
			i = 0;
	for (int r = startRow; r < static_cast<int>(this->Rows.size()) && i < maxRows; r++, i++)
	{
		GridViewRow& row = this->Rows[static_cast<size_t>(r)];
				float clipY = yf;
				float clipH = rowHeight;
				if (clipY < headerHeight)
				{
					clipH -= (headerHeight - clipY);
					clipY = headerHeight;
				}
				if (clipY + clipH > renderHeight)
					clipH = renderHeight - clipY;
				if (clipH <= 0.0f)
				{
					yf += rowHeight;
					continue;
				}
				float xf = -this->ScrollXOffset;
		for (int c = startColumn; c < static_cast<int>(this->Columns.size()); c++)
		{
			if (_hiddenColumns.find(c) != _hiddenColumns.end()) continue;
			float colW = this->Columns[static_cast<size_t>(c)].Width;
					if (xf >= renderWidth) break;
					if (xf + colW <= 0.0f) { xf += colW; continue; }

					float drawX = xf;
					float cellWidth = colW;
					if (drawX < 0.0f) { cellWidth += drawX; drawX = 0.0f; }
					if (drawX + cellWidth > renderWidth) cellWidth = renderWidth - drawX;
					if (cellWidth <= 0.0f) { xf += colW; continue; }
					const float clipX = drawX;
					const float clipW = cellWidth;
					drawX = xf;
					cellWidth = colW;

					float currentRowHeight = rowHeight;
					d2d->PushDrawRect(clipX, clipY, clipW, clipH);
					{
						switch (this->Columns[c].Type)
						{
						case ColumnType::Text:
						{
							if (IsGridCellSelected(this, c, r))
							{
								if (this->Editing && this->EditingColumnIndex == c && this->EditingRowIndex == r && this->ParentForm->Selected == this)
								{
									D2D1_RECT_F cellLocal{};
									if (!TryGetCellRectLocal(c, r, cellLocal))
									{
										SaveCurrentEditingCell(true);
										ResetEditingState();
									}
									else
									{
										float renderHeight = currentRowHeight - (this->EditTextMargin * 2.0f);
										if (renderHeight < 0.0f) renderHeight = 0.0f;

										EditEnsureSelectionInRange();
										EditUpdateScroll(clipW);

										auto textSize = font->GetTextSize(this->EditingText, FLT_MAX, renderHeight);
										float offsetY = (currentRowHeight - textSize.height) * 0.5f;
										if (offsetY < 0.0f) offsetY = 0.0f;

										auto editRect = GridInsetRect(drawX, yf, cellWidth, currentRowHeight, 3.0f, 3.0f);
										d2d->FillRoundRect(editRect, this->EditBackColor, this->CellCornerRadius);
										d2d->DrawRoundRect(editRect.left, editRect.top, editRect.right - editRect.left,
											editRect.bottom - editRect.top, this->AccentColor, 1.2f, this->CellCornerRadius);
										DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);

										int sels = EditSelectionStart <= EditSelectionEnd ? EditSelectionStart : EditSelectionEnd;
										int sele = EditSelectionEnd >= EditSelectionStart ? EditSelectionEnd : EditSelectionStart;
										int selLen = sele - sels;
										auto selRange = font->HitTestTextRange(this->EditingText, (UINT32)sels, (UINT32)selLen);
										bool caretRectValid = false;
										D2D1_RECT_F caretRect{};
										const auto absoluteLocation = this->GetAbsoluteLocationDip();

										if (selLen != 0)
										{
											for (auto sr : selRange)
											{
												d2d->FillRect(
													sr.left + drawX + this->EditTextMargin - this->EditOffsetX,
													(sr.top + yf) + offsetY,
													sr.width, sr.height,
													this->EditSelectedBackColor);
											}
										}
										else
										{
											if (!selRange.empty())
											{
												const float caretX = selRange[0].left + drawX + this->EditTextMargin - this->EditOffsetX;
												const float caretTop = (selRange[0].top + yf) + offsetY;
												const float caretBottom = (selRange[0].top + yf + selRange[0].height) + offsetY;
												caretRect = { absoluteLocation.x + caretX - 2.0f, absoluteLocation.y + caretTop - 2.0f, absoluteLocation.x + caretX + 2.0f, absoluteLocation.y + caretBottom + 2.0f };
												caretRectValid = true;
											}
										}

										UpdateCaretBlinkState(isSelected, this->EditSelectionStart, this->EditSelectionEnd, caretRectValid, caretRectValid ? &caretRect : nullptr);
										caretBlinkStateUpdated = true;
										if (caretRectValid && IsCaretBlinkVisible())
										{
											d2d->DrawLine(
												{ caretRect.left - absoluteLocation.x + 2.0f, caretRect.top - absoluteLocation.y + 2.0f },
												{ caretRect.left - absoluteLocation.x + 2.0f, caretRect.bottom - absoluteLocation.y - 2.0f },
												Colors::Black);
										}

										if (!caretBlinkStateUpdated)
										{
											UpdateCaretBlinkState(false, 0, 1, false, nullptr);
										}
										auto textLayout = Factory::CreateStringLayout(this->EditingText, FLT_MAX, renderHeight, font->FontObject);
										if (textLayout) {
											if (selLen != 0)
											{
												d2d->DrawStringLayoutEffect(textLayout,
													drawX + this->EditTextMargin - this->EditOffsetX, (yf)+offsetY,
													this->EditForeColor,
													DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
													this->EditSelectedForeColor,
													font);
											}
											else
											{
												d2d->DrawStringLayout(textLayout,
													drawX + this->EditTextMargin - this->EditOffsetX, (yf)+offsetY,
													this->EditForeColor);
											}
											textLayout->Release();
										}
									}
								}
								else
								{
									DrawGridCellState(this, d2d, drawX, yf, cellWidth, currentRowHeight, true, false);
									DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
									if (row.Cells.size() > static_cast<size_t>(c))
										d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
											drawX + this->CellHorizontalPadding,
											yf + textTop,
											(std::max)(1.0f, cellWidth - this->CellHorizontalPadding * 2.0f),
											fontHeight + 2.0f,
											this->SelectedItemForeColor, font);
								}
							}
							else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								DrawGridCellState(this, d2d, drawX, yf, cellWidth, currentRowHeight, false, true);
								DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
									d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
										drawX + this->CellHorizontalPadding,
										yf + textTop,
										(std::max)(1.0f, cellWidth - this->CellHorizontalPadding * 2.0f),
										fontHeight + 2.0f,
										this->UnderMouseItemForeColor, font);
							}
							else
							{
								DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
									d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
										drawX + this->CellHorizontalPadding,
										yf + textTop,
										(std::max)(1.0f, cellWidth - this->CellHorizontalPadding * 2.0f),
										fontHeight + 2.0f,
										this->ForeColor, font);
							}
						}
						break;
						case ColumnType::LinkedText:
						{
							const bool selectedCell = IsGridCellSelected(this, c, r);
							const bool hoverCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							if (selectedCell || hoverCell)
								DrawGridCellState(this, d2d, drawX, yf, cellWidth, currentRowHeight, selectedCell, hoverCell);
							DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
							if (row.Cells.size() > static_cast<size_t>(c))
							{
								DrawGridLinkedText(this, d2d, font, row.Cells[static_cast<size_t>(c)].Text,
									drawX, yf, cellWidth, currentRowHeight, textTop, hoverCell);
							}
						}
						break;
						case ColumnType::Button:
						{
							// Button：独立样式（WinForms-like），不使用普通单元格的"选中底色"
							const bool isHot = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							const bool isPressed = (this->_buttonMouseDown && isHot &&
								this->_buttonDownColumnIndex == c && this->_buttonDownRowIndex == r &&
								(GetAsyncKeyState(VK_LBUTTON) & 0x8000));

							D2D1_COLOR_F back = this->ButtonBackColor;
							if (isPressed) back = this->ButtonPressedBackColor;
							else if (isHot) back = this->ButtonHoverBackColor;

							DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
							D2D1_RECT_F buttonRect = GridInsetRect(drawX, yf, cellWidth, currentRowHeight, 6.0f, 4.0f);
							d2d->FillRoundRect(buttonRect, back, this->CellCornerRadius);
							d2d->DrawRoundRect(buttonRect.left, buttonRect.top, buttonRect.right - buttonRect.left,
								buttonRect.bottom - buttonRect.top,
								isPressed ? this->AccentColor : this->ButtonBorderDarkColor, 1.0f, this->CellCornerRadius);

							// Text center (+ pressed offset)
							// 使用列的ButtonText作为按钮文字
							const std::wstring& buttonText = this->Columns[c].ButtonText;
							if (!buttonText.empty())
							{
								auto textSize = font->GetTextSize(buttonText);
								float tx = (cellWidth - textSize.width) * 0.5f;
								float ty = (currentRowHeight - textSize.height) * 0.5f;
								if (tx < 0.0f) tx = 0.0f;
								if (ty < 0.0f) ty = 0.0f;
								if (isPressed) { tx += 1.0f; ty += 1.0f; }
								d2d->DrawString(buttonText,
									drawX + tx,
									yf + ty,
									(std::max)(1.0f, cellWidth - 12.0f),
									fontHeight + 2.0f,
									this->ForeColor, font);
							}
						}
						break;
						case ColumnType::ComboBox:
						{
							EnsureComboBoxCellDefaultSelection(c, r);
							float dropDownProgress = 0.0f;
							if (this->_dropDownPopup &&
								this->_dropDownPopupColumnIndex == c &&
								this->_dropDownPopupRowIndex == r)
							{
								dropDownProgress = this->_dropDownPopup->CurrentDropProgress();
							}
							D2D1_COLOR_F fore = this->ForeColor;
							bool fill = false;

							if (IsGridCellSelected(this, c, r))
							{
								fore = this->SelectedItemForeColor;
								fill = true;
							}
							else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								fore = this->UnderMouseItemForeColor;
								fill = true;
							}

							if (fill)
								DrawGridCellState(this, d2d, drawX, yf, cellWidth, currentRowHeight,
									IsGridCellSelected(this, c, r),
									c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
							if (row.Cells.size() > static_cast<size_t>(c))
							{
								d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
									drawX + this->CellHorizontalPadding,
									yf + textTop,
									(std::max)(1.0f, cellWidth - this->CellHorizontalPadding * 2.0f - 14.0f),
									fontHeight + 2.0f,
									fore, font);
							}

							// Draw drop arrow on right
							{
								const float h = currentRowHeight;
								float iconSize = h * 0.38f;
								if (iconSize < 8.0f) iconSize = 8.0f;
								if (iconSize > 14.0f) iconSize = 14.0f;
								const float padRight = 8.0f;
								const float cx = drawX + cellWidth - padRight - iconSize * 0.5f;
								const float cy = yf + h * 0.5f;
								DrawGridChevron(d2d, cx, cy, iconSize, dropDownProgress, fore);
							}
						}
						break;
						case ColumnType::Image:
						{
							float _size = cellWidth < rowHeight ? cellWidth : rowHeight;
							if (_size > 22.0f) _size = 22.0f;
							float left = (cellWidth - _size) / 2.0f;
							float top = (rowHeight - _size) / 2.0f;
							const bool selectedCell = IsGridCellSelected(this, c, r);
							const bool hoverCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							if (selectedCell || hoverCell)
							{
								DrawGridCellState(this, d2d, drawX, yf, cellWidth, currentRowHeight, selectedCell, hoverCell);
								DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = row.Cells[static_cast<size_t>(c)].GetImageBitmap(d2d))
										d2d->DrawBitmap(bmp,
											drawX + left,
											yf + top,
											_size, _size
										);
								}
							}
							else
							{
								DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = row.Cells[static_cast<size_t>(c)].GetImageBitmap(d2d))
										d2d->DrawBitmap(bmp,
											drawX + left,
											yf + top,
											_size, _size
										);
								}
							}
						}
						break;
						case ColumnType::Check:
						{
							float _size = cellWidth < rowHeight ? cellWidth : rowHeight;
							if (_size > 22)_size = 22;
							float left = (cellWidth - _size) / 2.0f;
							float top = (rowHeight - _size) / 2.0f;
							float _rsize = _size;
							const bool selectedCell = IsGridCellSelected(this, c, r);
							const bool hoverCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							if (selectedCell || hoverCell)
								DrawGridCellState(this, d2d, drawX, yf, cellWidth, currentRowHeight, selectedCell, hoverCell);
							DrawGridCellLines(d2d, drawX, yf, cellWidth, currentRowHeight, this->GridLineColor);
							if (row.Cells.size() > static_cast<size_t>(c))
							{
								D2D1_RECT_F box = D2D1::RectF(
									drawX + left + (_rsize * 0.18f),
									yf + top + (_rsize * 0.18f),
									drawX + left + (_rsize * 0.82f),
									yf + top + (_rsize * 0.82f));
								const bool checked = row.Cells[static_cast<size_t>(c)].Tag != 0;
								d2d->FillRoundRect(box, checked ? this->AccentColor : this->EditBackColor, 4.0f);
								d2d->DrawRoundRect(box.left, box.top, box.right - box.left, box.bottom - box.top,
									checked ? this->AccentColor : this->GridLineColor, 1.2f, 4.0f);
								if (checked)
								{
									d2d->DrawLine(
										D2D1::Point2F(box.left + (box.right - box.left) * 0.26f, box.top + (box.bottom - box.top) * 0.53f),
										D2D1::Point2F(box.left + (box.right - box.left) * 0.44f, box.top + (box.bottom - box.top) * 0.70f),
										Colors::White, 1.8f);
									d2d->DrawLine(
										D2D1::Point2F(box.left + (box.right - box.left) * 0.44f, box.top + (box.bottom - box.top) * 0.70f),
										D2D1::Point2F(box.left + (box.right - box.left) * 0.76f, box.top + (box.bottom - box.top) * 0.32f),
										Colors::White, 1.8f);
								}
							}
						}
						break;
						default:
							break;
						}
					}
					d2d->PopDrawRect();
					xf += colW;
				}
				yf += rowHeight;
			}

			// 渲染新行区域（如果启用）
			if (this->AllowUserToAddRows && this->Columns.size() > 0)
			{
				float newRowY = yf;
				if (newRowY < headerHeight) newRowY = headerHeight;

				// 确保新行在可视区域内
				if (newRowY < renderHeight)
				{
					float newRowHeight = rowHeight;
					if (newRowY + newRowHeight > renderHeight)
						newRowHeight = renderHeight - newRowY;

					if (newRowHeight > 0.0f)
					{
						float xf = -this->ScrollXOffset;
						for (int c = 0; c < static_cast<int>(this->Columns.size()); c++)
						{
							if (_hiddenColumns.find(c) != _hiddenColumns.end()) continue;
							float colW = this->Columns[static_cast<size_t>(c)].Width;
							if (xf >= renderWidth) break;
							if (xf + colW <= 0.0f) { xf += colW; continue; }

							float drawX = xf;
							float cellWidth = colW;
							if (drawX < 0.0f) { cellWidth += drawX; drawX = 0.0f; }
							if (drawX + cellWidth > renderWidth) cellWidth = renderWidth - drawX;
							if (cellWidth <= 0.0f) { xf += colW; continue; }

							const float clipX = drawX;
							const float clipW = cellWidth;

							d2d->PushDrawRect(clipX, newRowY, clipW, newRowHeight);
							{
								D2D1_RECT_F rowRect = GridInsetRect(drawX, newRowY, cellWidth, newRowHeight, 3.0f, 3.0f);
								d2d->FillRoundRect(rowRect, this->_isUnderNewRow ? this->UnderMouseItemBackColor : this->NewRowBackColor, this->CellCornerRadius);

								// 绘制新行单元格内容（空单元格样式）
								if (c == 0)
								{
									// 在第一列显示新行指示符 (*)
									float asteriskSize = fontHeight * 0.5f;
									float asteriskX = drawX + textTop;
									float asteriskY = newRowY + textTop;

									// 绘制星号
									d2d->DrawString(L"*",
										asteriskX,
										asteriskY,
										this->NewRowIndicatorColor, font);

									// 绘制提示文字
									std::wstring hintText = L"点击添加新行";
									auto hintSize = font->GetTextSize(hintText);
									d2d->DrawString(hintText,
										asteriskX + asteriskSize + 4.0f,
										asteriskY,
										this->NewRowForeColor, font);
								}

								DrawGridCellLines(d2d, drawX, newRowY, cellWidth, newRowHeight, this->GridLineColor);
							}
							d2d->PopDrawRect();
							xf += colW;
						}
					}
				}
			}

			d2d->PushDrawRect(
				0.0f,
				0.0f,
				actualWidth,
				actualHeight);
			{
				if (this->ParentForm->UnderMouse == this)
				{
					d2d->DrawRoundRect(1.0f, 1.0f, actualWidth - 2.0f, actualHeight - 2.0f, this->BorderColor, 1.5f, this->CellCornerRadius);
				}
				else
				{
					d2d->DrawRoundRect(1.0f, 1.0f, actualWidth - 2.0f, actualHeight - 2.0f, this->BorderColor, 1.0f, this->CellCornerRadius);
				}
			}
			d2d->PopDrawRect();
			this->DrawScroll();
		}
		d2d->DrawRoundRect(this->BorderThickness * 0.5f, this->BorderThickness * 0.5f,
			actualWidth - this->BorderThickness, actualHeight - this->BorderThickness,
			this->BorderColor, this->BorderThickness, this->CellCornerRadius);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool GridView::IsNewRowArea(int x, int y)
{
	if (!this->AllowUserToAddRows) return false;
	if (this->Columns.size() <= 0) return false;

	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;
	const float renderHeight = l.RenderHeight;

	// 检查是否在渲染区域内
	if (x < 0 || x >= (int)renderWidth) return false;
	if (y < 0 || y >= (int)renderHeight) return false;

	// 检查是否在表头下方
	if (y <= (int)headHeight) return false;

	// 计算新行区域的位置
	const float rowHeight = this->GetRowHeightPx();
	const float totalRowsHeight = rowHeight * (float)this->Rows.size();
	const float newRowY = headHeight + totalRowsHeight;

	// 检查鼠标是否在新行区域内
	const float virtualY = ((float)y - headHeight) + this->ScrollYOffset;
	if (virtualY >= totalRowsHeight && virtualY < totalRowsHeight + rowHeight)
	{
		return true;
	}

	return false;
}

int GridView::HitTestNewRow(int x, int y, int& outColumnIndex)
{
	if (!this->AllowUserToAddRows) return -1;
	if (this->Columns.size() <= 0) return -1;

	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;

	if (x < 0 || x >= (int)renderWidth) return -1;
	if (y <= (int)headHeight) return -1;

	const float rowHeight = this->GetRowHeightPx();
	const float totalRowsHeight = rowHeight * (float)this->Rows.size();
	const float virtualY = ((float)y - headHeight) + this->ScrollYOffset;

	// 检查是否在新行区域内
	if (virtualY < totalRowsHeight || virtualY >= totalRowsHeight + rowHeight)
		return -1;

	// 确定鼠标在哪一列
	const float virtualX = (float)x + this->ScrollXOffset;
	float acc = 0.0f;
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		if (virtualX >= acc && virtualX < acc + this->Columns[static_cast<size_t>(i)].Width)
		{
			outColumnIndex = i;
			return static_cast<int>(this->Rows.size());  // 返回Rows.size()作为新行的索引
		}
		acc += this->Columns[static_cast<size_t>(i)].Width;
	}

	return -1;
}

void GridView::AddNewRow()
{
	if (!this->AllowUserToAddRows) return;
	bool cancel = false;
	this->OnUserAddingRow(this, cancel);
	if (cancel) return;

	// 创建新行
	GridViewRow newRow;
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		CellValue cell;
		newRow.Cells.push_back(cell);
	}

	// 添加到Rows列表
	int newRowIndex = static_cast<int>(this->Rows.size());
	this->Rows.push_back(newRow);

	// 触发新行添加事件
	this->OnUserAddedRow(this, newRowIndex);

	// 自动选中新行的第一列并开始编辑
	if (this->Columns.size() > 0)
	{
		SetCurrentSelection(0, newRowIndex, true);
		StartEditingCell(0, newRowIndex);
	}

}

void GridView::ReSizeRows(int count)
{
	if (count < 0) count = 0;
	CommitEdit();
	this->Rows.resize((size_t)count);
}
void GridView::AutoSizeColumn(int col)
{
	if (col >= 0 && col < static_cast<int>(this->Columns.size()))
	{
		auto font = this->Font;
		float fontHeight = font->FontHeight;
		float rowHeight = fontHeight + 2.0f;
		if (RowHeight != 0.0f)
		{
			rowHeight = RowHeight;
		}
		auto& column = this->Columns[static_cast<size_t>(col)];
		column.Width = 10.0f;
		for (int i = 0; i < static_cast<int>(this->Rows.size()); i++)
		{
			auto& r = this->Rows[static_cast<size_t>(i)];
			if (r.Cells.size() > static_cast<size_t>(col))
			{
				if (this->Columns[static_cast<size_t>(col)].Type == ColumnType::Text ||
					this->Columns[static_cast<size_t>(col)].Type == ColumnType::LinkedText ||
					this->Columns[static_cast<size_t>(col)].Type == ColumnType::Button ||
					this->Columns[static_cast<size_t>(col)].Type == ColumnType::ComboBox)
				{
					// Button列使用列的ButtonText来计算宽度
					std::wstring textToMeasure;
					if (this->Columns[static_cast<size_t>(col)].Type == ColumnType::Button && !this->Columns[static_cast<size_t>(col)].ButtonText.empty())
					{
						textToMeasure = this->Columns[static_cast<size_t>(col)].ButtonText;
					}
					else
					{
						textToMeasure = r.Cells[static_cast<size_t>(col)].Text;
					}
					auto width = font->GetTextSize(textToMeasure.c_str()).width;
					if (column.Width < width)
					{
						column.Width = width;
					}
				}
				else
				{
					column.Width = rowHeight;
				}
			}
		}
		RequestRefresh();
	}
}
void GridView::ToggleCheckState(int col, int row)
{
	auto& cell = this->Rows[row].Cells[col];
	cell.Tag = __int64(!cell.Tag);
	this->OnGridViewCheckStateChanged(this, col, row, cell.Tag != 0);
}

void GridView::RaiseLinkedTextClick(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
	if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::LinkedText) return;

	std::wstring text;
	auto& rowObj = this->Rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() > static_cast<size_t>(col))
		text = rowObj.Cells[static_cast<size_t>(col)].Text;
	this->OnGridViewLinkedTextClick(this, col, row, text);
}

void GridView::EnsureComboBoxCellDefaultSelection(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
	if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;

	auto& column = this->Columns[static_cast<size_t>(col)];
	if (column.ComboBoxItems.size() <= 0) return;
	auto& rowObj = this->Rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() <= static_cast<size_t>(col))
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[static_cast<size_t>(col)];

	const __int64 selectedTagIndex = cell.Tag;
	if (selectedTagIndex < 0 || selectedTagIndex >= static_cast<__int64>(column.ComboBoxItems.size()))
	{
		cell.Tag = 0;
		cell.Text = column.ComboBoxItems[0];
	}
	else
	{
		// Keep Text in sync with index if needed
		const auto& selectedText = column.ComboBoxItems[static_cast<size_t>(selectedTagIndex)];
		if (cell.Text != selectedText)
			cell.Text = selectedText;
	}
}

void GridView::CloseDropDownEditor(bool immediate)
{
	if (!this->_dropDownPopup) return;

	this->_dropDownPopup->Hide(!immediate, immediate);
	if (immediate)
	{
		this->_dropDownPopupColumnIndex = -1;
		this->_dropDownPopupRowIndex = -1;
	}
}

void GridView::ToggleDropDownEditor(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
	if (!this->ParentForm) return;
	if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;

	EnsureComboBoxCellDefaultSelection(col, row);

	if (this->_dropDownPopup &&
		this->_dropDownPopup->IsOpen() &&
		this->_dropDownPopupColumnIndex == col &&
		this->_dropDownPopupRowIndex == row)
	{
		CloseDropDownEditor();
		this->ParentForm->Invalidate(true);
		this->InvalidateVisual();
		return;
	}

	// Commit text edit when switching modes
	CommitEdit();
	SetCurrentSelection(col, row, true);

	if (!this->_dropDownPopup)
	{
		this->_dropDownPopup = new DropDownPopup();
	}

	D2D1_RECT_F cellLocal{};
	if (!TryGetCellRectLocal(col, row, cellLocal)) return;

	const auto absoluteLocation = this->GetAbsoluteLocationDip();
	const int x = (int)std::round((float)absoluteLocation.x + cellLocal.left);
	const int y = (int)std::round((float)absoluteLocation.y + cellLocal.top);
	const int w = (int)std::round(cellLocal.right - cellLocal.left);
	const int h = (int)std::round(cellLocal.bottom - cellLocal.top);

	auto& column = this->Columns[static_cast<size_t>(col)];
	auto& rowObj = this->Rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() <= static_cast<size_t>(col))
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[static_cast<size_t>(col)];

	int selectedIndex = (int)cell.Tag;
	if (selectedIndex < 0) selectedIndex = 0;
	if (selectedIndex >= static_cast<int>(column.ComboBoxItems.size()))
		selectedIndex = column.ComboBoxItems.empty() ? -1 : static_cast<int>(column.ComboBoxItems.size()) - 1;

	this->_dropDownPopup->SetFontEx(this->Font, false);
	this->_dropDownPopup->DropBackColor = this->EditBackColor;
	this->_dropDownPopup->ForeColor = this->EditForeColor;
	this->_dropDownPopup->DropBorderColor = D2D1_COLOR_F{ 0.74f, 0.77f, 0.84f, 0.95f };
	this->_dropDownPopup->AccentColor = this->AccentColor;
	this->_dropDownPopup->SelectedItemBackColor = this->SelectedItemBackColor;
	this->_dropDownPopup->SelectedItemForeColor = this->SelectedItemForeColor;
	this->_dropDownPopup->UnderMouseBackColor = this->UnderMouseItemBackColor;
	this->_dropDownPopup->UnderMouseForeColor = this->UnderMouseItemForeColor;
	this->_dropDownPopup->ScrollBackColor = this->ScrollBackColor;
	this->_dropDownPopup->ScrollForeColor = this->ScrollForeColor;
	this->_dropDownPopup->MinWidth = 80.0f;
	this->_dropDownPopup->CornerRadius = 6.0f;
	this->_dropDownPopup->ItemHeight = (float)(h > 0 ? h : 24);

	this->_dropDownPopup->SelectionChanged.Clear();
	this->_dropDownPopup->SelectionChanged += [this, col, row](DropDownPopup* sender, int selectedIndex, std::wstring selectedText)
		{
			(void)sender;
			(void)selectedText;
			if (col < 0 || row < 0) return;
			if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
			if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;
			auto& column2 = this->Columns[static_cast<size_t>(col)];
			if (column2.ComboBoxItems.size() <= 0) return;
			int clampedSelectedIndex = selectedIndex;
			if (clampedSelectedIndex < 0) clampedSelectedIndex = 0;
			if (clampedSelectedIndex >= static_cast<int>(column2.ComboBoxItems.size())) clampedSelectedIndex = static_cast<int>(column2.ComboBoxItems.size()) - 1;
			auto& cell2 = this->Rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(col)];
			cell2.Tag = (__int64)clampedSelectedIndex;
			cell2.Text = column2.ComboBoxItems[static_cast<size_t>(clampedSelectedIndex)];
			this->OnGridViewComboBoxSelectionChanged(this, col, row, clampedSelectedIndex, cell2.Text);
			this->InvalidateVisual();
		};
	this->_dropDownPopup->Closed.Clear();
	this->_dropDownPopup->Closed += [this](DropDownPopup* sender)
		{
			(void)sender;
			this->_dropDownPopupColumnIndex = -1;
			this->_dropDownPopupRowIndex = -1;
			this->InvalidateVisual();
		};

	this->_dropDownPopupColumnIndex = col;
	this->_dropDownPopupRowIndex = row;
	this->_dropDownPopup->ShowAt(this->ParentForm, this,
		D2D1::RectF((float)x, (float)y, (float)(x + (w > 0 ? w : 1)), (float)(y + (h > 0 ? h : 1))),
		column.ComboBoxItems, selectedIndex, (float)(w > 0 ? w : 1), (float)(h > 0 ? h : 24), 4);
	this->ParentForm->Invalidate(true);
	this->InvalidateVisual();
}
void GridView::StartEditingCell(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;

	SelectCell(col, row);
	if (IsEditableTextCell(col, row))
		BeginEdit(col, row);
	else
		CommitEdit();
}

bool GridView::BeginEdit(int col, int row)
{
	if (!IsEditableTextCell(col, row)) return false;
	if (Editing && EditingColumnIndex == col && EditingRowIndex == row)
		return true;
	if (Editing)
		CommitEdit();
	if (!SelectCell(col, row)) return false;

	auto& cells = Rows[static_cast<size_t>(row)].Cells;
	if (cells.size() <= static_cast<size_t>(col))
		cells.resize(static_cast<size_t>(col) + 1);
	Editing = true;
	EditingColumnIndex = col;
	EditingRowIndex = row;
	EditingText = cells[static_cast<size_t>(col)].Text;
	EditingOriginalText = EditingText;
	EditSelectionStart = 0;
	EditSelectionEnd = static_cast<int>(EditingText.size());
	EditOffsetX = 0.0f;
	if (ParentForm)
		ParentForm->Selected = this;
	EditSetImeCompositionWindow();
	RequestRefresh(false);
	return true;
}

void GridView::ResetEditingState() noexcept
{
	Editing = false;
	EditingColumnIndex = -1;
	EditingRowIndex = -1;
	EditingText.clear();
	EditingOriginalText.clear();
	EditSelectionStart = EditSelectionEnd = 0;
	EditOffsetX = 0.0f;
}

bool GridView::CommitEdit()
{
	if (!Editing) return false;
	SaveCurrentEditingCell(true);
	ResetEditingState();
	RequestRefresh(false);
	return true;
}

bool GridView::CancelEdit()
{
	if (!Editing) return false;
	if (auto* cell = GetCell(EditingColumnIndex, EditingRowIndex))
		cell->Text = EditingOriginalText;
	ResetEditingState();
	RequestRefresh(false);
	return true;
}

bool GridView::SetEditingText(const std::wstring& text)
{
	if (!Editing) return false;
	EditingText = text;
	for (auto& ch : EditingText)
	{
		if (ch == L'\r' || ch == L'\n') ch = L' ';
	}
	EditSelectionStart = EditSelectionEnd = static_cast<int>(EditingText.size());
	if (auto* cell = GetCell(EditingColumnIndex, EditingRowIndex))
		cell->Text = EditingText;
	RequestRefresh(false);
	return true;
}

static int RemapGridCollectionIndex(
	int current, const CollectionChangedEventArgs& change, size_t newSize)
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
	return index < newSize ? current : -1;
}

void GridView::CaptureSelectionAccessibilityIds()
{
	_selectedRowAccessibilityId = 0;
	_selectedColumnAccessibilityId = 0;
	if (SelectedRowIndex < 0 || SelectedColumnIndex < 0
		|| SelectedRowIndex >= static_cast<int>(Rows.size())
		|| SelectedColumnIndex >= static_cast<int>(Columns.size())) return;
	EnsureAccessibilityIds();
	_selectedRowAccessibilityId =
		Rows[static_cast<size_t>(SelectedRowIndex)].AccessibilityId;
	_selectedColumnAccessibilityId =
		Columns[static_cast<size_t>(SelectedColumnIndex)].AccessibilityId;
}

void GridView::NotifyCollectionStructureChanged()
{
	if (_updateDepth != 0)
	{
		_collectionStructurePending = true;
		return;
	}
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
}

void GridView::OnRowsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	CommitEdit();
	_accessibilityIdsDirty = true;

	int nextRow = -1;
	if (_selectedRowAccessibilityId != 0)
	{
		const auto selected = std::find_if(Rows.begin(), Rows.end(),
			[this](const GridViewRow& row)
			{ return row.AccessibilityId == _selectedRowAccessibilityId; });
		if (selected != Rows.end())
			nextRow = static_cast<int>(selected - Rows.begin());
	}
	if (nextRow < 0 && _selectedRowAccessibilityId == 0)
		nextRow = RemapGridCollectionIndex(
			SelectedRowIndex, change, Rows.size());
	int nextColumn = SelectedColumnIndex;
	if (nextRow < 0 || nextColumn < 0
		|| nextColumn >= static_cast<int>(Columns.size()))
	{
		nextRow = -1;
		nextColumn = -1;
	}
	SetCurrentSelection(nextColumn, nextRow, false);

	const int hovered = change.Action == CollectionChangeAction::Reset
		? -1
		: RemapGridCollectionIndex(
			UnderMouseRowIndex, change, Rows.size());
	SetCurrentUnderMouseRowIndex(hovered);
	if (Rows.empty())
	{
		SetCurrentScrollYOffset(0.0f);
		SetCurrentScrollRowPosition(0);
	}
	if (_updateDepth == 0)
		EnsureAccessibilityIds();
	NotifyCollectionStructureChanged();
	RequestRefresh();
}

void GridView::OnColumnsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	CommitEdit();
	_accessibilityIdsDirty = true;
	for (auto& row : Rows)
	{
		auto& cells = row.Cells;
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			if (change.NewIndex != CollectionChangedEventArgs::Npos)
			{
				if (cells.size() < change.NewIndex)
					cells.resize(change.NewIndex);
				cells.insert(cells.begin() + static_cast<ptrdiff_t>(change.NewIndex),
					change.NewCount, CellValue{});
			}
			break;
		case CollectionChangeAction::Remove:
			if (change.OldIndex != CollectionChangedEventArgs::Npos
				&& change.OldIndex < cells.size())
			{
				const size_t end = (std::min)(
					cells.size(), change.OldIndex + change.OldCount);
				cells.erase(cells.begin() + static_cast<ptrdiff_t>(change.OldIndex),
					cells.begin() + static_cast<ptrdiff_t>(end));
			}
			break;
		case CollectionChangeAction::Move:
			cells.resize(Columns.size());
			if (change.OldIndex < cells.size()
				&& change.NewIndex < cells.size())
			{
				if (change.OldIndex < change.NewIndex)
					std::rotate(cells.begin() + change.OldIndex,
						cells.begin() + change.OldIndex + 1,
						cells.begin() + change.NewIndex + 1);
				else
					std::rotate(cells.begin() + change.NewIndex,
						cells.begin() + change.OldIndex,
						cells.begin() + change.OldIndex + 1);
			}
			break;
		case CollectionChangeAction::Swap:
			cells.resize(Columns.size());
			if (change.OldIndex < cells.size()
				&& change.NewIndex < cells.size())
				std::swap(cells[change.OldIndex], cells[change.NewIndex]);
			break;
		case CollectionChangeAction::Reset:
			cells.resize(Columns.size());
			break;
		default:
			break;
		}
	}
	int nextColumn = -1;
	if (_selectedColumnAccessibilityId != 0)
	{
		const auto selected = std::find_if(Columns.begin(), Columns.end(),
			[this](const GridViewColumn& column)
			{ return column.AccessibilityId == _selectedColumnAccessibilityId; });
		if (selected != Columns.end())
			nextColumn = static_cast<int>(selected - Columns.begin());
	}
	if (nextColumn < 0 && !Columns.empty() && SelectedRowIndex >= 0)
	{
		const int remapped = RemapGridCollectionIndex(
			SelectedColumnIndex, change, Columns.size());
		nextColumn = remapped >= 0
			? remapped
			: (std::min)(SelectedColumnIndex,
				static_cast<int>(Columns.size()) - 1);
	}
	const int nextRow = nextColumn >= 0
		&& SelectedRowIndex >= 0
		&& SelectedRowIndex < static_cast<int>(Rows.size())
		? SelectedRowIndex : -1;
	SetCurrentSelection(nextColumn, nextRow, false);

	const int hovered = change.Action == CollectionChangeAction::Reset
		? -1
		: RemapGridCollectionIndex(
			UnderMouseColumnIndex, change, Columns.size());
	SetCurrentUnderMouseColumnIndex(hovered);
	const int sorted = change.Action == CollectionChangeAction::Reset
		? -1
		: RemapGridCollectionIndex(
			SortedColumnIndex, change, Columns.size());
	SetCurrentSortedColumnIndex(sorted);
	if (Columns.empty()) SetCurrentScrollXOffset(0.0f);
	if (_updateDepth == 0)
		EnsureAccessibilityIds();
	NotifyCollectionStructureChanged();
	RequestRefresh();
}

void GridView::EnsureAccessibilityIds()
{
	if (!_accessibilityIdsDirty) return;
	std::unordered_set<uint32_t> used;
	used.reserve(Columns.size() + Rows.size()
		+ _accessibilityCellIds.size());
	std::unordered_map<uint32_t, AccessibilityNodeLocation> locations;
	locations.reserve(Columns.size() + Rows.size()
		+ _accessibilityCellIds.size());
	auto ensure = [&used](uint32_t& id)
	{
		while (id == 0 || !used.insert(id).second)
			id = AllocateAccessibilityVirtualId();
	};
	for (size_t column = 0; column < Columns.size(); ++column)
	{
		ensure(Columns[column].AccessibilityId);
		locations.emplace(Columns[column].AccessibilityId,
			AccessibilityNodeLocation{
				AccessibilityNodeLocation::Kind::Header, 0, column });
	}
	for (size_t row = 0; row < Rows.size(); ++row)
	{
		auto& candidate = Rows[row];
		ensure(candidate.AccessibilityId);
		locations.emplace(candidate.AccessibilityId,
			AccessibilityNodeLocation{
				AccessibilityNodeLocation::Kind::Row, row, 0 });
	}

	std::unordered_map<uint64_t, uint32_t> retainedCells;
	std::unordered_map<uint32_t, uint64_t> retainedCellKeys;
	retainedCells.reserve(_accessibilityCellIds.size());
	retainedCellKeys.reserve(_accessibilityCellIds.size());
	for (const auto& entry : _accessibilityCellIds)
	{
		const uint32_t rowId = static_cast<uint32_t>(entry.first >> 32);
		const uint32_t columnId = static_cast<uint32_t>(entry.first);
		const auto rowLocation = locations.find(rowId);
		const auto columnLocation = locations.find(columnId);
		if (rowLocation == locations.end()
			|| rowLocation->second.NodeKind != AccessibilityNodeLocation::Kind::Row
			|| columnLocation == locations.end()
			|| columnLocation->second.NodeKind
				!= AccessibilityNodeLocation::Kind::Header) continue;
		const size_t row = rowLocation->second.Row;
		const size_t column = columnLocation->second.Column;
		if (row >= Rows.size() || column >= Columns.size()
			|| column >= Rows[row].Cells.size()) continue;
		uint32_t id = entry.second;
		ensure(id);
		const uint64_t key = (static_cast<uint64_t>(rowId) << 32) | columnId;
		retainedCells.emplace(key, id);
		retainedCellKeys.emplace(id, key);
		locations.emplace(id, AccessibilityNodeLocation{
			AccessibilityNodeLocation::Kind::Cell, row, column });
		Rows[row].Cells[column].AccessibilityId = id;
	}
	_accessibilityNodeLocationById.swap(locations);
	_accessibilityCellIds.swap(retainedCells);
	_accessibilityCellKeyById.swap(retainedCellKeys);
	_accessibilityIdsDirty = false;
}

uint32_t GridView::EnsureAccessibilityCellId(size_t row, size_t column)
{
	EnsureAccessibilityIds();
	if (row >= Rows.size() || column >= Columns.size()
		|| column >= Rows[row].Cells.size()) return 0;
	const uint32_t rowId = Rows[row].AccessibilityId;
	const uint32_t columnId = Columns[column].AccessibilityId;
	const uint64_t key = (static_cast<uint64_t>(rowId) << 32) | columnId;
	if (const auto found = _accessibilityCellIds.find(key);
		found != _accessibilityCellIds.end()) return found->second;
	auto& cell = Rows[row].Cells[column];
	uint32_t id = cell.AccessibilityId;
	while (id == 0 || _accessibilityNodeLocationById.contains(id)
		|| _accessibilityCellKeyById.contains(id))
		id = AllocateAccessibilityVirtualId();
	cell.AccessibilityId = id;
	_accessibilityCellIds.emplace(key, id);
	_accessibilityCellKeyById.emplace(id, key);
	_accessibilityNodeLocationById.emplace(id, AccessibilityNodeLocation{
		AccessibilityNodeLocation::Kind::Cell, row, column });
	return id;
}

void GridView::GetAccessibilityVirtualChildren(
	uint32_t parentId, std::vector<uint32_t>& result)
{
	result.clear();
	EnsureAccessibilityIds();
	if (parentId == 0)
	{
		result.reserve(Columns.size() + Rows.size());
		for (const auto& column : Columns)
			result.push_back(column.AccessibilityId);
		for (const auto& row : Rows)
			result.push_back(row.AccessibilityId);
		return;
	}
	const auto rowPosition = _accessibilityNodeLocationById.find(parentId);
	if (rowPosition == _accessibilityNodeLocationById.end()
		|| rowPosition->second.NodeKind != AccessibilityNodeLocation::Kind::Row)
		return;
	const size_t rowIndex = rowPosition->second.Row;
	const auto& row = Rows[rowIndex];
	const size_t count = (std::min)(row.Cells.size(), Columns.size());
	result.reserve(count);
	for (size_t column = 0; column < count; ++column)
	{
		const uint32_t id = EnsureAccessibilityCellId(rowIndex, column);
		if (id != 0) result.push_back(id);
	}
}

size_t GridView::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	EnsureAccessibilityIds();
	if (parentId == 0) return Columns.size() + Rows.size();
	const auto location = _accessibilityNodeLocationById.find(parentId);
	if (location == _accessibilityNodeLocationById.end()
		|| location->second.NodeKind != AccessibilityNodeLocation::Kind::Row)
		return 0;
	return (std::min)(Rows[location->second.Row].Cells.size(), Columns.size());
}

bool GridView::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	EnsureAccessibilityIds();
	if (parentId == 0)
	{
		if (index < Columns.size()) result = Columns[index].AccessibilityId;
		else
		{
			const size_t row = index - Columns.size();
			if (row >= Rows.size()) return false;
			result = Rows[row].AccessibilityId;
		}
		return result != 0;
	}
	const auto location = _accessibilityNodeLocationById.find(parentId);
	if (location == _accessibilityNodeLocationById.end()
		|| location->second.NodeKind != AccessibilityNodeLocation::Kind::Row)
		return false;
	const auto& cells = Rows[location->second.Row].Cells;
	if (index >= cells.size() || index >= Columns.size()) return false;
	result = EnsureAccessibilityCellId(location->second.Row, index);
	return result != 0;
}

bool GridView::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	result = 0;
	EnsureAccessibilityIds();
	const auto location = _accessibilityNodeLocationById.find(id);
	if (location == _accessibilityNodeLocationById.end()) return false;
	size_t index = 0;
	if (parentId == 0)
	{
		if (location->second.NodeKind == AccessibilityNodeLocation::Kind::Header)
			index = location->second.Column;
		else if (location->second.NodeKind == AccessibilityNodeLocation::Kind::Row)
			index = Columns.size() + location->second.Row;
		else return false;
	}
	else
	{
		if (location->second.NodeKind != AccessibilityNodeLocation::Kind::Cell
			|| location->second.Row >= Rows.size()
			|| Rows[location->second.Row].AccessibilityId != parentId) return false;
		index = location->second.Column;
	}
	if (!next && index == 0) return false;
	const size_t sibling = next ? index + 1 : index - 1;
	return sibling < GetAccessibilityVirtualChildCount(parentId)
		&& TryGetAccessibilityVirtualChildAt(parentId, sibling, result);
}

bool GridView::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	const auto layout = CalcScrollLayout();
	if (localX < 0.0f || localY < 0.0f
		|| localX >= layout.RenderWidth || localY >= layout.RenderHeight)
		return false;
	EnsureAccessibilityIds();
	if (localY < layout.HeadHeight)
	{
		const int column = HitTestHeaderColumn(
			static_cast<int>(std::floor(localX)),
			static_cast<int>(std::floor(localY)));
		if (column < 0 || column >= static_cast<int>(Columns.size())) return false;
		result = Columns[static_cast<size_t>(column)].AccessibilityId;
		return result != 0;
	}
	const POINT cell = GetGridViewUnderMouseItem(
		static_cast<int>(std::floor(localX)),
		static_cast<int>(std::floor(localY)), this);
	if (cell.y < 0 || cell.y >= static_cast<LONG>(Rows.size())) return false;
	const auto& row = Rows[static_cast<size_t>(cell.y)];
	if (cell.x >= 0 && cell.x < static_cast<LONG>(Columns.size())
		&& cell.x < static_cast<LONG>(row.Cells.size()))
		result = EnsureAccessibilityCellId(
			static_cast<size_t>(cell.y), static_cast<size_t>(cell.x));
	else result = row.AccessibilityId;
	return result != 0;
}

AccessibilityVirtualContainerInfo
GridView::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AccessibilityVirtualPattern::Selection
		| AccessibilityVirtualPattern::Grid
		| AccessibilityVirtualPattern::Table
		| AccessibilityVirtualPattern::Scroll;
	result.CanSelectMultiple = false;
	result.IsSelectionRequired = false;
	result.RowCount = static_cast<int>(Rows.size());
	result.ColumnCount = static_cast<int>(Columns.size());
	return result;
}

bool GridView::GetAccessibilityVirtualItemAt(
	int row, int column, uint32_t& id)
{
	id = 0;
	if (row < 0 || column < 0
		|| row >= static_cast<int>(Rows.size())
		|| column >= static_cast<int>(Columns.size())
		|| column >= static_cast<int>(
			Rows[static_cast<size_t>(row)].Cells.size())) return false;
	id = EnsureAccessibilityCellId(
		static_cast<size_t>(row), static_cast<size_t>(column));
	return id != 0;
}

void GridView::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result)
{
	EnsureAccessibilityIds();
	result.clear();
	result.reserve(Columns.size());
	for (const auto& column : Columns)
		result.push_back(column.AccessibilityId);
}

void GridView::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
	uint32_t id = 0;
	if (GetAccessibilityVirtualItemAt(
		SelectedRowIndex, SelectedColumnIndex, id))
		result.push_back(id);
}

bool GridView::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	if (id == 0) return false;
	EnsureAccessibilityIds();
	const auto ownerId = GetAccessibilitySnapshot().AutomationId;
	auto automationId = [&](const wchar_t* kind)
	{
		const std::wstring suffix = std::wstring(kind) + L"-" + std::to_wstring(id);
		return ownerId.empty() ? suffix : ownerId + L"." + suffix;
	};
	auto layout = CalcScrollLayout();
	const auto position = _accessibilityNodeLocationById.find(id);
	if (position == _accessibilityNodeLocationById.end()) return false;
	const auto& location = position->second;
	if (location.NodeKind == AccessibilityNodeLocation::Kind::Header)
	{
		if (location.Column >= Columns.size()) return false;
		const int column = static_cast<int>(location.Column);
		const auto& candidate = Columns[location.Column];
		float left = -ScrollXOffset;
		for (int index = 0; index < column; ++index)
			left += Columns[static_cast<size_t>(index)].Width;
		const float right = left + candidate.Width;
		result = {};
		result.Id = id;
		result.Role = AccessibleRole::HeaderItem;
		result.Patterns = AccessibilityVirtualPattern::Invoke;
		result.Name = candidate.Name;
		result.AutomationId = automationId(L"header");
		result.BoundsDip = D2D1::RectF(left, 0.0f, right, layout.HeadHeight);
		result.Enabled = Enable;
		result.Visible = Visible && right > 0.0f
			&& left < layout.RenderWidth && layout.HeadHeight > 0.0f;
		result.Row = -1;
		result.Column = column;
		result.Level = 1;
		return true;
	}

	if (location.Row >= Rows.size()) return false;
	const int row = static_cast<int>(location.Row);
	auto& candidate = Rows[location.Row];
	const float top = layout.HeadHeight
		+ static_cast<float>(row) * layout.RowHeight - ScrollYOffset;
	const float bottom = top + layout.RowHeight;
	if (location.NodeKind == AccessibilityNodeLocation::Kind::Row)
	{
		result = {};
		result.Id = id;
		result.Role = AccessibleRole::DataItem;
		result.Patterns = AccessibilityVirtualPattern::ScrollItem
			| AccessibilityVirtualPattern::VirtualizedItem;
		result.Name = L"Row " + std::to_wstring(row + 1);
		result.AutomationId = automationId(L"row");
		result.BoundsDip = D2D1::RectF(
			0.0f, top, layout.RenderWidth, bottom);
		result.Enabled = Enable;
		result.Visible = Visible && bottom > layout.HeadHeight
			&& top < layout.RenderHeight;
		result.Selected = row == SelectedRowIndex;
		result.Row = row;
		result.Column = -1;
		result.Level = 1;
		return true;
	}
	if (location.NodeKind != AccessibilityNodeLocation::Kind::Cell
		|| location.Column >= Columns.size()
		|| location.Column >= candidate.Cells.size()) return false;
	const size_t column = location.Column;
	float left = -ScrollXOffset;
	for (size_t index = 0; index < column; ++index)
		left += Columns[index].Width;
	auto& cell = candidate.Cells[column];
	const float right = left + Columns[column].Width;
	const auto& definition = Columns[column];
	result = {};
	result.Id = id;
	result.ParentId = candidate.AccessibilityId;
	result.Role = AccessibleRole::DataItem;
	result.Patterns = AccessibilityVirtualPattern::SelectionItem
		| AccessibilityVirtualPattern::ScrollItem
		| AccessibilityVirtualPattern::VirtualizedItem
		| AccessibilityVirtualPattern::GridItem
		| AccessibilityVirtualPattern::TableItem;
	switch (definition.Type)
	{
	case ColumnType::Check:
		result.Role = AccessibleRole::CheckBox;
		result.Patterns |= AccessibilityVirtualPattern::Toggle;
		break;
	case ColumnType::Button:
		result.Role = AccessibleRole::Button;
		result.Patterns |= AccessibilityVirtualPattern::Invoke;
		break;
	case ColumnType::LinkedText:
		result.Role = AccessibleRole::Link;
		result.Patterns |= AccessibilityVirtualPattern::Invoke;
		break;
	case ColumnType::ComboBox:
		result.Role = AccessibleRole::ComboBox;
		result.Patterns |= AccessibilityVirtualPattern::Invoke
			| AccessibilityVirtualPattern::Value;
		break;
	case ColumnType::Image:
		result.Role = AccessibleRole::Image;
		break;
	case ColumnType::Text:
	default:
		result.Patterns |= AccessibilityVirtualPattern::Value;
		break;
	}
	result.Name = definition.Type == ColumnType::Button
		&& !definition.ButtonText.empty()
		? definition.ButtonText : cell.GetText();
	result.Description = definition.Name;
	result.Value = cell.GetText();
	result.AutomationId = automationId(L"cell");
	result.BoundsDip = D2D1::RectF(left, top, right, bottom);
	result.Enabled = Enable;
	result.Visible = Visible && right > 0.0f
		&& left < layout.RenderWidth
		&& bottom > layout.HeadHeight
		&& top < layout.RenderHeight;
	result.Selected = row == SelectedRowIndex
		&& static_cast<int>(column) == SelectedColumnIndex;
	result.Checked = definition.Type == ColumnType::Check && cell.GetBool();
	result.ReadOnly = definition.Type != ColumnType::Text || !definition.CanEdit;
	result.Row = row;
	result.Column = static_cast<int>(column);
	result.Level = 2;
	return true;
}

bool GridView::InvokeAccessibilityVirtualNode(uint32_t id)
{
	AccessibilityVirtualNode node;
	if (!Enable || !TryGetAccessibilityVirtualNode(id, node)) return false;
	if (node.Role == AccessibleRole::HeaderItem && node.Column >= 0)
	{
		SortByColumn(node.Column,
			SortedColumnIndex == node.Column ? !SortAscending : true);
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Invoke);
		return true;
	}
	if (node.Row < 0 || node.Column < 0) return false;
	if (!HasAccessibilityVirtualPattern(
		node.Patterns, AccessibilityVirtualPattern::Invoke)) return false;
	SelectCell(node.Column, node.Row, true);
	HandleCellClick(node.Column, node.Row);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Invoke);
	return true;
}

bool GridView::ToggleAccessibilityVirtualNode(uint32_t id)
{
	AccessibilityVirtualNode node;
	if (!Enable || !TryGetAccessibilityVirtualNode(id, node)
		|| node.Row < 0 || node.Column < 0
		|| !HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::Toggle)) return false;
	ToggleCheckState(node.Column, node.Row);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Toggle);
	return true;
}

bool GridView::SetAccessibilityVirtualNodeValue(
	uint32_t id, const std::wstring& value)
{
	AccessibilityVirtualNode node;
	if (!Enable || !TryGetAccessibilityVirtualNode(id, node)
		|| node.Row < 0 || node.Column < 0 || node.ReadOnly
		|| !HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::Value)) return false;
	auto* cell = GetCell(node.Column, node.Row);
	if (!cell) return false;
	cell->SetText(value);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Value);
	RequestRefresh(false);
	return true;
}

bool GridView::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	AccessibilityVirtualNode node;
	if (!Enable || !TryGetAccessibilityVirtualNode(id, node)
		|| node.Row < 0 || node.Column < 0
		|| !HasAccessibilityVirtualPattern(
			node.Patterns, AccessibilityVirtualPattern::SelectionItem)) return false;
	if (action == AccessibilitySelectionAction::Remove)
	{
		if (SelectedRowIndex == node.Row && SelectedColumnIndex == node.Column)
			ClearSelection();
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
		return true;
	}
	const bool selected = SelectCell(node.Column, node.Row, true);
	if (selected)
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
	return selected;
}

bool GridView::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	AccessibilityVirtualNode node;
	if (!Enable || !TryGetAccessibilityVirtualNode(id, node)
		|| node.Row < 0) return false;
	const auto layout = CalcScrollLayout();
	if (layout.RowHeight <= 0.0f) return false;
	const float rowTop = static_cast<float>(node.Row) * layout.RowHeight;
	const float rowBottom = rowTop + layout.RowHeight;
	const float viewportHeight = (std::max)(
		0.0f, layout.RenderHeight - layout.HeadHeight);
	float nextY = ScrollYOffset;
	if (rowTop < nextY) nextY = rowTop;
	else if (rowBottom > nextY + viewportHeight)
		nextY = rowBottom - viewportHeight;
	SetCurrentScrollYOffset((std::clamp)(nextY, 0.0f, layout.MaxScrollY));

	if (node.Column >= 0
		&& node.Column < static_cast<int>(Columns.size()))
	{
		float columnLeft = 0.0f;
		for (int column = 0; column < node.Column; ++column)
		{
			if (_hiddenColumns.find(column) != _hiddenColumns.end()) continue;
			columnLeft += Columns[static_cast<size_t>(column)].Width;
		}
		const float columnRight = columnLeft
			+ Columns[static_cast<size_t>(node.Column)].Width;
		float nextX = ScrollXOffset;
		if (columnLeft < nextX) nextX = columnLeft;
		else if (columnRight > nextX + layout.RenderWidth)
			nextX = columnRight - layout.RenderWidth;
		SetCurrentScrollXOffset((std::clamp)(
			nextX, 0.0f, layout.MaxScrollX));
	}
	InvalidateVisual();
	return true;
}

bool GridView::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	const auto layout = CalcScrollLayout();
	result = {};
	result.HorizontallyScrollable = layout.MaxScrollX > 0.0f;
	if (result.HorizontallyScrollable)
	{
		result.HorizontalScrollPercent = (std::clamp)(
			static_cast<double>(_scrollXOffset / layout.MaxScrollX) * 100.0,
			0.0, 100.0);
		result.HorizontalViewSize = layout.TotalColumnsWidth > 0.0f
			? (std::clamp)(static_cast<double>(layout.RenderWidth
				/ layout.TotalColumnsWidth) * 100.0, 0.0, 100.0)
			: 100.0;
	}
	result.VerticallyScrollable = layout.MaxScrollY > 0.0f;
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent = (std::clamp)(
			static_cast<double>(_scrollYOffset / layout.MaxScrollY) * 100.0,
			0.0, 100.0);
		result.VerticalViewSize = layout.TotalRowsHeight > 0.0f
			? (std::clamp)(static_cast<double>(layout.ContentHeight
				/ layout.TotalRowsHeight) * 100.0, 0.0, 100.0)
			: 100.0;
	}
	return true;
}

bool GridView::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	const auto layout = CalcScrollLayout();
	if (horizontal != AccessibilityScrollAmount::NoAmount
		&& layout.MaxScrollX <= 0.0f) return false;
	if (vertical != AccessibilityScrollAmount::NoAmount
		&& layout.MaxScrollY <= 0.0f) return false;
	auto delta = [](AccessibilityScrollAmount amount,
		float smallStep, float largeStep) -> float
	{
		switch (amount)
		{
		case AccessibilityScrollAmount::LargeDecrement: return -largeStep;
		case AccessibilityScrollAmount::SmallDecrement: return -smallStep;
		case AccessibilityScrollAmount::LargeIncrement: return largeStep;
		case AccessibilityScrollAmount::SmallIncrement: return smallStep;
		case AccessibilityScrollAmount::NoAmount: return 0.0f;
		}
		return 0.0f;
	};
	if (horizontal != AccessibilityScrollAmount::NoAmount)
	{
		const float next = (std::clamp)(ScrollXOffset + delta(horizontal,
			48.0f, layout.RenderWidth), 0.0f, layout.MaxScrollX);
		SetCurrentScrollXOffset(next);
	}
	if (vertical != AccessibilityScrollAmount::NoAmount)
	{
		const float next = (std::clamp)(ScrollYOffset + delta(vertical,
			layout.RowHeight, layout.ContentHeight), 0.0f, layout.MaxScrollY);
		SetCurrentScrollYOffset(next);
	}
	return true;
}

bool GridView::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	if ((horizontalPercent != AccessibilityScrollNoChange
			&& (!std::isfinite(horizontalPercent)
				|| horizontalPercent < 0.0 || horizontalPercent > 100.0))
		|| (verticalPercent != AccessibilityScrollNoChange
			&& (!std::isfinite(verticalPercent)
				|| verticalPercent < 0.0 || verticalPercent > 100.0)))
		return false;
	const auto layout = CalcScrollLayout();
	if (horizontalPercent != AccessibilityScrollNoChange)
	{
		if (layout.MaxScrollX <= 0.0f) return false;
		SetCurrentScrollXOffset(static_cast<float>(
			layout.MaxScrollX * horizontalPercent / 100.0));
	}
	if (verticalPercent != AccessibilityScrollNoChange)
	{
		if (layout.MaxScrollY <= 0.0f) return false;
		SetCurrentScrollYOffset(static_cast<float>(
			layout.MaxScrollY * verticalPercent / 100.0));
	}
	return true;
}

void GridView::OnComputedLayoutSizeChanged()
{
	const auto layout = CalcScrollLayout();
	SetCurrentScrollXOffset((std::clamp)(
		_scrollXOffset, 0.0f, layout.MaxScrollX));
	SetCurrentScrollYOffset((std::clamp)(
		_scrollYOffset, 0.0f, layout.MaxScrollY));
	NotifyAccessibilityScrollChanged();
}

void GridView::CancelEditing(bool revert)
{
	if (revert) CancelEdit();
	else CommitEdit();
	ClearSelection();
}
void GridView::SaveCurrentEditingCell(bool commit)
{
	if (!this->Editing) return;
	if (!commit) return;
	if (this->EditingColumnIndex < 0 || this->EditingRowIndex < 0) return;
	if (this->EditingRowIndex >= static_cast<int>(this->Rows.size())) return;
	if (this->EditingColumnIndex >= static_cast<int>(this->Columns.size())) return;
	this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
}
void GridView::AdjustScrollPosition()
{
	auto l = this->CalcScrollLayout();
	const float rowH = this->GetRowHeightPx();
	const float headH = this->GetHeadHeightPx();
	const float contentH = std::max(0.0f, l.RenderHeight - headH);
	const float totalH = (rowH > 0.0f) ? (rowH * (float)this->Rows.size()) : 0.0f;
	const float maxScrollY = std::max(0.0f, totalH - contentH);

	if (this->SelectedRowIndex < 0 || this->SelectedRowIndex >= static_cast<int>(this->Rows.size())) return;
	if (rowH <= 0.0f) return;

	const float rowTop = rowH * (float)this->SelectedRowIndex;
	const float rowBottom = rowTop + rowH;
	const float viewTop = this->ScrollYOffset;
	const float viewBottom = this->ScrollYOffset + contentH;
	float offset = this->ScrollYOffset;

	if (rowTop < viewTop)
		offset = rowTop;
	else if (rowBottom > viewBottom)
		offset = rowBottom - contentH;

	SetCurrentScrollYOffset(std::clamp(offset, 0.0f, maxScrollY));
}
bool GridView::CanScrollDown()
{
	auto l = this->CalcScrollLayout();
	return this->ScrollYOffset < l.MaxScrollY;
}
bool GridView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	auto l = this->CalcScrollLayout();
	if ((GetKeyState(VK_SHIFT) & 0x8000) && l.NeedH && l.MaxScrollX > 0.0f)
	{
		return delta > 0
			? this->ScrollXOffset > 0.0f
			: this->ScrollXOffset < l.MaxScrollX;
	}
	if (!l.NeedV || l.MaxScrollY <= 0.0f)
		return false;
	return delta > 0
		? this->ScrollYOffset > 0.0f
		: this->ScrollYOffset < l.MaxScrollY;
}
void GridView::UpdateUnderMouseIndices(int localX, int localY)
{
	POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
	SetCurrentUnderMouseColumnIndex(undermouseIndex.x);
	SetCurrentUnderMouseRowIndex(undermouseIndex.y);
}
void GridView::ChangeEditionSelected(int col, int row)
{
	CommitEdit();
	StartEditingCell(col, row);
}
void GridView::HandleDropFiles(WPARAM wParam)
{
	HDROP hDropInfo = HDROP(wParam);
	UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
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
void GridView::HandleMouseWheel(WPARAM wParam, int localX, int localY)
{
	bool needUpdate = false;
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);
	auto l = this->CalcScrollLayout();

	if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && l.NeedH)
	{
		float step = 40.0f;
		const float next = delta < 0
			? this->ScrollXOffset + step
			: this->ScrollXOffset - step;
		SetCurrentScrollXOffset((std::clamp)(next, 0.0f, l.MaxScrollX));
		needUpdate = true;

		UpdateUnderMouseIndices(localX, localY);
		MouseEventArgs eventArgs(MouseButtons::None, 0, localX, localY, delta);
		this->OnMouseWheel(this, eventArgs);
		if (needUpdate) this->InvalidateVisual();
		return;
	}

	if (delta < 0)
	{
		if (CanScrollDown())
		{
			needUpdate = true;
			const float rowH = this->GetRowHeightPx();
			const float step = (rowH > 0.0f) ? rowH : 16.0f;
			SetCurrentScrollYOffset(std::min(this->ScrollYOffset + step, l.MaxScrollY));
		}
	}
	else
	{
		if (this->ScrollYOffset > 0.0f)
		{
			needUpdate = true;
			const float rowH = this->GetRowHeightPx();
			const float step = (rowH > 0.0f) ? rowH : 16.0f;
			SetCurrentScrollYOffset(std::max(0.0f, this->ScrollYOffset - step));
		}
	}

	UpdateUnderMouseIndices(localX, localY);
	MouseEventArgs eventArgs(MouseButtons::None, 0, localX, localY, delta);
	this->OnMouseWheel(this, eventArgs);

	if (needUpdate)
	{
		this->InvalidateVisual();
	}
}
void GridView::HandleMouseMove(int localX, int localY)
{
	this->ParentForm->UnderMouse = this;
	bool needUpdate = false;

	if (this->_resizingColumn)
	{
		float dx = (float)localX - this->_resizeStartX;
		float newWidth = this->_resizeStartWidth + dx;
		if (newWidth < this->_minColumnWidth) newWidth = this->_minColumnWidth;
		if (this->_resizeColumnIndex >= 0 && this->_resizeColumnIndex < static_cast<int>(this->Columns.size()))
		{
			if (this->Columns[static_cast<size_t>(this->_resizeColumnIndex)].Width != newWidth)
			{
				this->Columns[static_cast<size_t>(this->_resizeColumnIndex)].Width = newWidth;
				needUpdate = true;
			}
		}
		MouseEventArgs eventArgs(MouseButtons::None, 0, localX, localY, 0);
		this->OnMouseMove(this, eventArgs);
		if (needUpdate) this->InvalidateVisual();
		return;
	}

	if (this->InScroll)
	{
		needUpdate = true;
		SetScrollByPos(static_cast<float>(localY));
	}
	else if (this->InHScroll)
	{
		needUpdate = true;
		SetHScrollByPos((float)localX);
	}
	else
	{
		if (this->Editing && this->ParentForm->Selected == this && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			D2D1_RECT_F rect{};
			if (TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
			{
				float cellWidth = rect.right - rect.left;
				float cellHeight = rect.bottom - rect.top;
				float lx = (float)localX - rect.left;
				float ly = (float)localY - rect.top;
				this->EditSelectionEnd = EditHitTestTextPosition(cellWidth, cellHeight, lx, ly);
				EditUpdateScroll(cellWidth);
				needUpdate = true;
			}
		}
		POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
		if (this->UnderMouseColumnIndex != undermouseIndex.x ||
			this->UnderMouseRowIndex != undermouseIndex.y)
		{
			needUpdate = true;
		}
		SetCurrentUnderMouseColumnIndex(undermouseIndex.x);
		SetCurrentUnderMouseRowIndex(undermouseIndex.y);

		// 检查是否在新行区域
		if (this->AllowUserToAddRows)
		{
			int newRowCol = -1;
			int hitResult = HitTestNewRow(localX, localY, newRowCol);
			bool isUnderNewRow = (hitResult >= 0 && newRowCol >= 0);
			if (this->_isUnderNewRow != isUnderNewRow)
			{
				this->_isUnderNewRow = isUnderNewRow;
				this->_newRowAreaHitTest = newRowCol;
				needUpdate = true;
			}
		}
	}

	MouseEventArgs eventArgs(MouseButtons::None, 0, localX, localY, 0);
	this->OnMouseMove(this, eventArgs);

	if (needUpdate)
	{
		this->InvalidateVisual();
	}
}
void GridView::HandleLeftButtonDown(int localX, int localY)
{
	auto lastSelected = this->ParentForm->Selected;
	this->ParentForm->Selected = this;

	if (lastSelected && lastSelected != this)
	{
		lastSelected->InvalidateVisual();
	}

	auto l = this->CalcScrollLayout();
	const int renderW = (int)l.RenderWidth;
	const int renderH = (int)l.RenderHeight;

	if (l.NeedH && localY >= renderH && localX >= 0 && localX < renderW)
	{
		CancelEditing(true);
		this->InHScroll = true;
		if (l.TotalColumnsWidth > l.RenderWidth && l.RenderWidth > 0.0f)
		{
			const float barW = l.RenderWidth;
			const float maxScrollX = std::max(0.0f, l.TotalColumnsWidth - barW);
			float thumbW = (barW * barW) / l.TotalColumnsWidth;
			const float minThumbW = barW * 0.1f;
			if (thumbW < minThumbW) thumbW = minThumbW;
			if (thumbW > barW) thumbW = barW;
			const float moveSpace = std::max(0.0f, barW - thumbW);
			float per = 0.0f;
			if (maxScrollX > 0.0f) per = std::clamp(this->ScrollXOffset / maxScrollX, 0.0f, 1.0f);
			const float thumbX = per * moveSpace;
			const float pointerX = (float)localX;
			const bool hitThumb = (pointerX >= thumbX && pointerX <= (thumbX + thumbW));
			_hScrollThumbGrabOffsetX = hitThumb ? (pointerX - thumbX) : (thumbW * 0.5f);
		}
		else
		{
			_hScrollThumbGrabOffsetX = 0.0f;
		}
		SetHScrollByPos((float)localX);
		SetCapture(this->ParentForm->Handle);
		MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
		return;
	}

	if (l.NeedV && localX >= renderW && localY >= 0 && localY < renderH)
	{
		CancelEditing(true);
		this->InScroll = true;
		if (this->Rows.size() > 0 && l.MaxScrollY > 0.0f && l.RenderHeight > 0.0f && l.ContentHeight > 0.0f)
		{
			const float renderingHeight = l.RenderHeight;
			const float totalHeight = l.TotalRowsHeight;
			float thumbH = renderingHeight * (l.ContentHeight / totalHeight);
			const float minThumbH = renderingHeight * 0.1f;
			if (thumbH < minThumbH) thumbH = minThumbH;
			if (thumbH > renderingHeight) thumbH = renderingHeight;
			const float moveSpace = std::max(0.0f, renderingHeight - thumbH);
			float per = std::clamp(this->ScrollYOffset / l.MaxScrollY, 0.0f, 1.0f);
			const float thumbTop = per * moveSpace;
			const float pointerY = (float)localY;
			const bool hitThumb = (pointerY >= thumbTop && pointerY <= (thumbTop + thumbH));
			_vScrollThumbGrabOffsetY = hitThumb ? (pointerY - thumbTop) : (thumbH * 0.5f);
		}
		else
		{
			_vScrollThumbGrabOffsetY = 0.0f;
		}
		SetScrollByPos((float)localY);
		SetCapture(this->ParentForm->Handle);
		MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
		return;
	}

	if (localX < renderW && localY < renderH)
	{
		int divCol = HitTestHeaderDivider(localX, localY);
		if (divCol >= 0)
		{
			CancelEditing(true);
			this->_resizingColumn = true;
			this->_resizeColumnIndex = divCol;
			this->_resizeStartX = (float)localX;
			this->_resizeStartWidth = this->Columns[divCol].Width;
			SetCapture(this->ParentForm->Handle);
			MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
			this->OnMouseDown(this, eventArgs);
			return;
		}

		int headCol = HitTestHeaderColumn(localX, localY);
		if (headCol >= 0)
		{
			CancelEditing(true);
			bool ascending = true;
			if (this->SortedColumnIndex == headCol)
				ascending = !this->SortAscending;
			SortByColumn(headCol, ascending);

			MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
			this->OnMouseDown(this, eventArgs);
			return;
		}

		POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
		if (undermouseIndex.y >= 0 && undermouseIndex.x >= 0 &&
			undermouseIndex.y < static_cast<LONG>(this->Rows.size()) && undermouseIndex.x < static_cast<LONG>(this->Columns.size()))
		{
			// Keep hover index in sync even if we didn't get a prior WM_MOUSEMOVE.
			SetCurrentUnderMouseColumnIndex(undermouseIndex.x);
			SetCurrentUnderMouseRowIndex(undermouseIndex.y);

			if (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button)
			{
				CommitEdit();
				CloseDropDownEditor();

				SetCurrentSelection(undermouseIndex.x, undermouseIndex.y, true);

				this->_buttonMouseDown = true;
				this->_buttonDownColumnIndex = undermouseIndex.x;
				this->_buttonDownRowIndex = undermouseIndex.y;
				SetCapture(this->ParentForm->Handle);

				MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
				this->OnMouseDown(this, eventArgs);
				this->InvalidateVisual();
				return;
			}

			if (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::LinkedText)
			{
				CommitEdit();
				CloseDropDownEditor();

				SetCurrentSelection(undermouseIndex.x, undermouseIndex.y, true);

				this->_linkedTextMouseDown = true;
				this->_linkedTextDownColumnIndex = undermouseIndex.x;
				this->_linkedTextDownRowIndex = undermouseIndex.y;
				SetCapture(this->ParentForm->Handle);

				MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
				this->OnMouseDown(this, eventArgs);
				this->InvalidateVisual();
				return;
			}

			if (this->Editing && undermouseIndex.x == this->EditingColumnIndex && undermouseIndex.y == this->EditingRowIndex)
			{
				SetEditingCaretFromMousePoint(localX, localY);
			}
			else
			{
				HandleCellClick(undermouseIndex.x, undermouseIndex.y);
				if (this->Editing && undermouseIndex.x == this->EditingColumnIndex && undermouseIndex.y == this->EditingRowIndex)
				{
					SetEditingCaretFromMousePoint(localX, localY);
				}
			}
		}
		else
		{
			CancelEditing(false);
		}

		// 处理新行点击
		if (this->AllowUserToAddRows && undermouseIndex.y < 0 && undermouseIndex.x >= 0)
		{
			int newRowCol = -1;
			int hitResult = HitTestNewRow(localX, localY, newRowCol);
			if (hitResult >= 0 && newRowCol >= 0 && newRowCol < static_cast<int>(this->Columns.size()))
			{
				CancelEditing(true);
				AddNewRow();
				return;
			}
		}
	}

	MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
	this->OnMouseDown(this, eventArgs);
	this->InvalidateVisual();
}
void GridView::HandleLeftButtonUp(int localX, int localY)
{
	if (this->_resizingColumn)
	{
		this->_resizingColumn = false;
		this->_resizeColumnIndex = -1;
		ReleaseCapture();
		MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
		return;
	}

	if (this->_buttonMouseDown)
	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
		const bool hitSameCell = (undermouseIndex.x == this->_buttonDownColumnIndex && undermouseIndex.y == this->_buttonDownRowIndex);
		const bool validCell = (undermouseIndex.x >= 0 && undermouseIndex.y >= 0 &&
			undermouseIndex.x < static_cast<LONG>(this->Columns.size()) && undermouseIndex.y < static_cast<LONG>(this->Rows.size()));
		const bool isButtonCell = validCell && (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button);

		this->_buttonMouseDown = false;
		this->_buttonDownColumnIndex = -1;
		this->_buttonDownRowIndex = -1;

		this->InScroll = false;
		this->InHScroll = false;
		ReleaseCapture();
		MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
		this->OnMouseUp(this, eventArgs);

		if (hitSameCell && isButtonCell)
		{
			this->OnGridViewButtonClick(this, undermouseIndex.x, undermouseIndex.y);
		}
		this->InvalidateVisual();
		return;
	}

	if (this->_linkedTextMouseDown)
	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
		const bool hitSameCell = (undermouseIndex.x == this->_linkedTextDownColumnIndex && undermouseIndex.y == this->_linkedTextDownRowIndex);
		const bool validCell = (undermouseIndex.x >= 0 && undermouseIndex.y >= 0 &&
			undermouseIndex.x < static_cast<LONG>(this->Columns.size()) && undermouseIndex.y < static_cast<LONG>(this->Rows.size()));
		const bool isLinkedTextCell = validCell && (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::LinkedText);

		this->_linkedTextMouseDown = false;
		this->_linkedTextDownColumnIndex = -1;
		this->_linkedTextDownRowIndex = -1;

		this->InScroll = false;
		this->InHScroll = false;
		ReleaseCapture();
		MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
		this->OnMouseUp(this, eventArgs);

		if (hitSameCell && isLinkedTextCell)
		{
			RaiseLinkedTextClick(undermouseIndex.x, undermouseIndex.y);
		}
		this->InvalidateVisual();
		return;
	}

	this->InScroll = false;
	this->InHScroll = false;
	ReleaseCapture();
	MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, 0);
	this->OnMouseUp(this, eventArgs);
	this->InvalidateVisual();
}
void GridView::HandleLeftButtonDoubleClick(WPARAM wParam, int localX, int localY)
{
	POINT undermouseIndex = GetGridViewUnderMouseItem(localX, localY, this);
	if (undermouseIndex.x >= 0 && undermouseIndex.y >= 0 &&
		undermouseIndex.x < static_cast<LONG>(this->Columns.size()) &&
		undermouseIndex.y < static_cast<LONG>(this->Rows.size()) &&
		IsEditableTextCell(undermouseIndex.x, undermouseIndex.y))
	{
		StartEditingCell(undermouseIndex.x, undermouseIndex.y);
		if (this->Editing &&
			this->EditingColumnIndex == undermouseIndex.x &&
			this->EditingRowIndex == undermouseIndex.y)
		{
			this->EditSelectionStart = 0;
			this->EditSelectionEnd = (int)this->EditingText.size();
			this->EditOffsetX = 0.0f;
			if (this->ParentForm)
				this->ParentForm->Selected = this;
			EditSetImeCompositionWindow();
			this->InvalidateVisual();
		}
	}

	MouseEventArgs eventArgs(MouseButtons::Left, 2, localX, localY, HIWORD(wParam));
	this->OnMouseDoubleClick(this, eventArgs);
}
void GridView::HandleKeyDown(WPARAM wParam)
{
	if (this->Editing && this->ParentForm->Selected == this)
	{
		EditSetImeCompositionWindow();
		EditEnsureSelectionInRange();

		if (wParam == VK_ESCAPE)
		{
			CancelEdit();
			this->InvalidateVisual();
			return;
		}
		if (wParam == VK_RETURN)
		{
			const int column = this->SelectedColumnIndex;
			const int row = this->SelectedRowIndex;
			CommitEdit();
			if (this->SelectedRowIndex < static_cast<int>(this->Rows.size()) - 1)
			{
				int nextRow = row + 1;
				BeginEdit(column, nextRow);
				this->EditSelectionStart = 0;
				this->EditSelectionEnd = (int)this->EditingText.size();
				AdjustScrollPosition();
			}
			this->InvalidateVisual();
			return;
		}

		if (wParam == VK_DELETE)
		{
			EditInputDelete();
			this->InvalidateVisual();
			return;
		}
		if (wParam == VK_RIGHT)
		{
			if (this->EditSelectionEnd < (int)this->EditingText.size())
			{
				this->EditSelectionEnd += 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					this->EditSelectionStart = this->EditSelectionEnd;
			}
			this->InvalidateVisual();
			return;
		}
		if (wParam == VK_LEFT)
		{
			if (this->EditSelectionEnd > 0)
			{
				this->EditSelectionEnd -= 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					this->EditSelectionStart = this->EditSelectionEnd;
			}
			this->InvalidateVisual();
			return;
		}
		if (wParam == VK_HOME)
		{
			this->EditSelectionEnd = 0;
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->EditSelectionStart = this->EditSelectionEnd;
			this->InvalidateVisual();
			return;
		}
		if (wParam == VK_END)
		{
			this->EditSelectionEnd = (int)this->EditingText.size();
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->EditSelectionStart = this->EditSelectionEnd;
			this->InvalidateVisual();
			return;
		}

		KeyEventArgs eventArgs(static_cast<Keys>(wParam));
		this->OnKeyDown(this, eventArgs);
		this->InvalidateVisual();
		return;
	}

	if (wParam == VK_DELETE && this->AllowUserToDeleteRows &&
		this->SelectedRowIndex >= 0 &&
		this->SelectedRowIndex < static_cast<int>(this->Rows.size()))
	{
		RemoveRowAt(this->SelectedRowIndex);
		KeyEventArgs eventArgs(static_cast<Keys>(wParam));
		this->OnKeyDown(this, eventArgs);
		return;
	}

	int nextColumn = this->SelectedColumnIndex;
	int nextRow = this->SelectedRowIndex;
	if ((nextColumn < 0 || nextRow < 0) &&
		!this->Columns.empty() && !this->Rows.empty())
	{
		nextColumn = 0;
		nextRow = 0;
	}
	switch (wParam)
	{
	case VK_RIGHT:
		if (nextColumn < static_cast<int>(this->Columns.size()) - 1) nextColumn++;
		break;
	case VK_LEFT:
		if (nextColumn > 0) nextColumn--;
		break;
	case VK_DOWN:
		if (nextRow < static_cast<int>(this->Rows.size()) - 1) nextRow++;
		break;
	case VK_UP:
		if (nextRow > 0) nextRow--;
		break;
	case VK_HOME:
		nextColumn = 0;
		break;
	case VK_END:
		if (!this->Columns.empty())
			nextColumn = static_cast<int>(this->Columns.size()) - 1;
		break;
	default:
		break;
	}
	if (nextColumn >= 0 && nextRow >= 0)
		SetCurrentSelection(nextColumn, nextRow, true);
	KeyEventArgs eventArgs(static_cast<Keys>(wParam));
	this->OnKeyDown(this, eventArgs);
	this->InvalidateVisual();
}
void GridView::HandleKeyUp(WPARAM wParam)
{
	KeyEventArgs eventArgs(static_cast<Keys>(wParam));
	this->OnKeyUp(this, eventArgs);
}
void GridView::HandleCharInput(WPARAM wParam)
{
	if (!this->Enable || !this->Visible) return;
	wchar_t ch = (wchar_t)wParam;

	if (!this->Editing)
	{
		if (ch >= 32 && ch <= 126 && this->SelectedColumnIndex >= 0 && this->SelectedRowIndex >= 0)
		{
			if (IsEditableTextCell(this->SelectedColumnIndex, this->SelectedRowIndex))
			{
				StartEditingCell(this->SelectedColumnIndex, this->SelectedRowIndex);
				this->EditSelectionStart = this->EditSelectionEnd = 0;
			}
		}
	}

	if (!this->Editing || this->ParentForm->Selected != this) return;

	if (ch >= 32 && ch <= 126)
	{
		const wchar_t buf[2] = { ch, L'\0' };
		EditInputText(buf);
	}
	else if (ch == 1) {
		this->EditSelectionStart = 0;
		this->EditSelectionEnd = (int)this->EditingText.size();
	}
	else if (ch == 8) {
		EditInputBack();
	}
	else if (ch == 22) {
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
						EditInputText(std::wstring(pBuf));
						GlobalUnlock(hClip);
					}
				}
			}
			CloseClipboard();
		}
	}
	else if (ch == 3 || ch == 24) {
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
		if (ch == 24) {
			EditInputBack();
		}
	}

	this->InvalidateVisual();
}
void GridView::HandleImeComposition(LPARAM lParam)
{
	if (!this->Editing || this->ParentForm->Selected != this) return;
	if (lParam & GCS_RESULTSTR)
	{
		HIMC hIMC = ImmGetContext(this->ParentForm->Handle);
		if (hIMC)
		{
			LONG bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0);
			if (bytes > 0)
			{
				int wcharCount = bytes / (int)sizeof(wchar_t);
				std::wstring buffer;
				buffer.resize(wcharCount);
				ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buffer.data(), bytes);

				std::wstring filtered;
				filtered.reserve(buffer.size());
				for (wchar_t c : buffer)
				{
					if (c > 0xFF)
						filtered.push_back(c);
				}
				if (!filtered.empty())
				{
					EditInputText(filtered);
				}
			}
			ImmReleaseContext(this->ParentForm->Handle, hIMC);
		}
		this->InvalidateVisual();
	}
}
void GridView::HandleCellClick(int col, int row)
{
	// 多选交互：Ctrl 切换单行，Shift 从锚点扩展范围。仅文本/可编辑单元格路径
	// 会走到这里；Check/Button/ComboBox 列有自己的点击语义，不参与多选。
	const bool isTextLike =
		this->Columns[col].Type != ColumnType::Check &&
		this->Columns[col].Type != ColumnType::Button &&
		this->Columns[col].Type != ColumnType::ComboBox;
	if (_multiSelect && isTextLike)
	{
		const bool ctrlDown = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool shiftDown = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
		if (ctrlDown || shiftDown)
		{
			CommitEdit();
			if (shiftDown)
			{
				const int anchor = (_selectionAnchorRow >= 0) ? _selectionAnchorRow
					: (_selectedRowIndex >= 0 ? _selectedRowIndex : row);
				if (!ctrlDown) _selectedRows.clear();
				const int lo = (std::min)(anchor, row);
				const int hi = (std::max)(anchor, row);
				for (int r = lo; r <= hi; ++r) _selectedRows.insert(r);
				SetCurrentSelectedColumnIndex(col);
				SetCurrentSelectedRowIndex(row);
			}
			else // Ctrl：切换单行
			{
				if (_selectedRows.erase(row) == 0)
					_selectedRows.insert(row);
				_selectionAnchorRow = row;
				SetCurrentSelectedColumnIndex(col);
				SetCurrentSelectedRowIndex(row);
			}
			SelectionChanged(this);
			RequestRefresh(false);
			return;
		}
		// 无修饰键：收敛为单选该行，并把它设为锚点。
		_selectedRows.clear();
		_selectedRows.insert(row);
		_selectionAnchorRow = row;
	}

	if (this->Columns[col].Type == ColumnType::Check)
	{
		ToggleCheckState(col, row);
	}
	else if (this->Columns[col].Type == ColumnType::Button)
	{
		// Button click is handled on mouse-up (WinForms-like)
		CommitEdit();
		SetCurrentSelection(col, row, true);
	}
	else if (this->Columns[col].Type == ColumnType::ComboBox)
	{
		ToggleDropDownEditor(col, row);
	}
	else
	{
		StartEditingCell(col, row);
	}
}
bool GridView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
		HandleDropFiles(wParam);
		break;

	case WM_MOUSEWHEEL:
		HandleMouseWheel(wParam, localX, localY);
		break;

	case WM_MOUSEMOVE:
		HandleMouseMove(localX, localY);
		break;

	case WM_LBUTTONDOWN:
		HandleLeftButtonDown(localX, localY);
		break;

	case WM_LBUTTONUP:
		HandleLeftButtonUp(localX, localY);
		break;

	case WM_LBUTTONDBLCLK:
		HandleLeftButtonDoubleClick(wParam, localX, localY);
		break;

	case WM_KEYDOWN:
		HandleKeyDown(wParam);
		break;

	case WM_KEYUP:
		HandleKeyUp(wParam);
		break;

	case WM_CHAR:
		HandleCharInput(wParam);
		break;

	case WM_IME_COMPOSITION:
		HandleImeComposition(lParam);
		break;

	default:
		break;
	}
	return true;
}

float GridView::GetRowHeightPx() const
{
	auto* font = _font ? _font : GetDefaultFontObject();
	float rowHeight = font->FontHeight + 10.0f;
	if (_rowHeight != 0.0f) rowHeight = _rowHeight;
	return rowHeight;
}
float GridView::GetHeadHeightPx() const
{
	auto* font = _font ? _font : GetDefaultFontObject();
	auto* headFont = HeadFont ? HeadFont : font;
	float headHeight = (_headHeight == 0.0f)
		? headFont->FontHeight + 12.0f : _headHeight;
	return headHeight;
}
bool GridView::TryGetCellRectLocal(int col, int row, D2D1_RECT_F& outRect)
{
	if (col < 0 || row < 0) return false;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return false;

	auto l = this->CalcScrollLayout();
	float renderWidth = l.RenderWidth;
	float rowHeight = GetRowHeightPx();
	float headHeight = GetHeadHeightPx();
	if (rowHeight <= 0.0f) return false;

	const int firstRow = (int)std::floor(this->ScrollYOffset / rowHeight);
	const float rowOffsetY = std::fmod(this->ScrollYOffset, rowHeight);
	int drawIndex = row - firstRow;
	if (drawIndex < 0) return false;
	float top = headHeight + (rowHeight * (float)drawIndex) - rowOffsetY;
	float bottom = top + rowHeight;
	if (bottom <= headHeight || top >= l.RenderHeight) return false;

	float left = -this->ScrollXOffset;
	for (int i = 0; i < col; i++)
	{
		if (_hiddenColumns.find(i) != _hiddenColumns.end()) continue;
		left += this->Columns[static_cast<size_t>(i)].Width;
	}
	float width = this->Columns[static_cast<size_t>(col)].Width;
	const float clipLeft = std::max(0.0f, left);
	const float clipRight = std::min(renderWidth, left + width);
	const float clipTop = std::max(headHeight, top);
	const float clipBottom = std::min(l.RenderHeight, bottom);
	if (clipRight <= clipLeft) return false;
	if (clipBottom <= clipTop) return false;

	outRect = D2D1_RECT_F{ clipLeft, clipTop, clipRight, clipBottom };
	return true;
}
bool GridView::IsEditableTextCell(int col, int row)
{
	if (col < 0 || row < 0) return false;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return false;
	return this->Columns[static_cast<size_t>(col)].Type == ColumnType::Text && this->Columns[static_cast<size_t>(col)].CanEdit;
}
bool GridView::SetEditingCaretFromMousePoint(int localX, int localY)
{
	if (!this->Editing) return false;
	D2D1_RECT_F rect{};
	if (!TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect)) return false;

	float cellWidth = rect.right - rect.left;
	float cellHeight = rect.bottom - rect.top;
	float lx = (float)localX - rect.left;
	float ly = (float)localY - rect.top;
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, lx, ly);
	this->EditSelectionStart = this->EditSelectionEnd = pos;
	EditUpdateScroll(cellWidth);
	return true;
}
void GridView::EditEnsureSelectionInRange()
{
	if (this->EditSelectionStart < 0) this->EditSelectionStart = 0;
	if (this->EditSelectionEnd < 0) this->EditSelectionEnd = 0;
	int maxLen = (int)this->EditingText.size();
	if (this->EditSelectionStart > maxLen) this->EditSelectionStart = maxLen;
	if (this->EditSelectionEnd > maxLen) this->EditSelectionEnd = maxLen;
}
void GridView::EditInputText(const std::wstring& input)
{
	if (!this->Editing) return;
	std::wstring old = this->EditingText;

	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen > 0)
	{
		this->EditingText.erase((size_t)sels, (size_t)selLen);
	}
	this->EditingText.insert((size_t)sels, input);
	this->EditSelectionStart = this->EditSelectionEnd = sels + (int)input.size();

	for (auto& ch : this->EditingText)
	{
		if (ch == L'\r' || ch == L'\n') ch = L' ';
	}

	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
	{
		this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
	}
}
void GridView::EditInputBack()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen > 0)
	{
		this->EditingText.erase((size_t)sels, (size_t)selLen);
		this->EditSelectionStart = this->EditSelectionEnd = sels;
	}
	else if (sels > 0)
	{
		this->EditingText.erase((size_t)sels - 1, 1);
		this->EditSelectionStart = this->EditSelectionEnd = sels - 1;
	}

	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
	{
		this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
	}
}
void GridView::EditInputDelete()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen > 0)
	{
		this->EditingText.erase((size_t)sels, (size_t)selLen);
		this->EditSelectionStart = this->EditSelectionEnd = sels;
	}
	else if (sels < (int)this->EditingText.size())
	{
		this->EditingText.erase((size_t)sels, 1);
		this->EditSelectionStart = this->EditSelectionEnd = sels;
	}

	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
	{
		this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
	}
}
void GridView::EditUpdateScroll(float cellWidth)
{
	if (!this->Editing) return;
	float renderWidth = cellWidth - (this->EditTextMargin * 2.0f);
	if (renderWidth <= 1.0f) return;

	EditEnsureSelectionInRange();
	auto font = this->Font;
	auto hit = font->HitTestTextRange(this->EditingText, (UINT32)this->EditSelectionEnd, (UINT32)0);
	if (hit.empty()) return;
	auto caret = hit[0];
	if ((caret.left + caret.width) - this->EditOffsetX > renderWidth)
	{
		this->EditOffsetX = (caret.left + caret.width) - renderWidth;
	}
	if (caret.left - this->EditOffsetX < 0.0f)
	{
		this->EditOffsetX = caret.left;
	}
	if (this->EditOffsetX < 0.0f) this->EditOffsetX = 0.0f;
}
int GridView::EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y)
{
	auto font = this->Font;
	float renderHeight = cellHeight - (this->EditTextMargin * 2.0f);
	if (renderHeight < 0.0f) renderHeight = 0.0f;
	return font->HitTestTextPosition(this->EditingText, FLT_MAX, renderHeight, (x - this->EditTextMargin) + this->EditOffsetX, y - this->EditTextMargin);
}
std::wstring GridView::EditGetSelectedString()
{
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	if (sele > sels && sels >= 0 && sele <= (int)this->EditingText.size())
	{
		return this->EditingText.substr((size_t)sels, (size_t)(sele - sels));
	}
	return L"";
}
void GridView::EditSetImeCompositionWindow()
{
	if (!this->ParentForm || !this->ParentForm->Handle) return;
	if (!this->Editing) return;
	D2D1_RECT_F rect{};
	if (!TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect)) return;

	const auto pos = this->GetAbsoluteLocationDip();
	this->ParentForm->SetImeCompositionWindowFromLogicalRect(
		D2D1_RECT_F{
			(float)pos.x + rect.left,
			(float)pos.y + rect.top,
			(float)pos.x + rect.right,
			(float)pos.y + rect.bottom
		});
}
#pragma endregion
