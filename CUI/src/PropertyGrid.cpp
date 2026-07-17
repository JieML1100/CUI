#define NOMINMAX
#include "PropertyGrid.h"
#include "AnchorPickerPopup.h"
#include "ColorPickerPopup.h"
#include "DropDownPopup.h"
#include "Form.h"
#include "TextEditCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <sstream>
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

	static void DrawResetGlyph(D2DGraphics* d2d, const D2D1_RECT_F& rect, D2D1_COLOR_F color)
	{
		const float cx = (rect.left + rect.right) * 0.5f;
		const float cy = (rect.top + rect.bottom) * 0.5f;
		const float radius = std::max(2.0f,
			std::min(RectWidth(rect), RectHeight(rect)) * 0.27f);
		d2d->DrawArc(D2D1::Point2F(cx, cy), radius,
			45.0f, 320.0f, color, 1.35f);
		auto tip = D2D1::Point2F(cx - radius - 0.4f, cy - radius * 0.10f);
		d2d->DrawLine(tip,
			D2D1::Point2F(tip.x + 4.0f, tip.y - 2.6f), color, 1.35f);
		d2d->DrawLine(tip,
			D2D1::Point2F(tip.x + 2.8f, tip.y + 3.2f), color, 1.35f);
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

	static bool TryParseDouble(const std::wstring& text, double& value)
	{
		try
		{
			size_t used = 0;
			value = std::stod(Trim(text), &used);
			return used != 0 && std::isfinite(value);
		}
		catch (...)
		{
			return false;
		}
	}

	static std::wstring FormatSliderValue(double value)
	{
		std::wostringstream stream;
		stream << std::fixed << std::setprecision(6) << value;
		auto result = stream.str();
		while (!result.empty() && result.back() == L'0') result.pop_back();
		if (!result.empty() && result.back() == L'.') result.pop_back();
		return result.empty() || result == L"-0" ? L"0" : result;
	}

	static CuiTextEdit::EditOptions PropertyGridEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = false;
		return options;
	}

	static bool IsNumberEditCandidate(const std::wstring& text)
	{
		if (text.empty())
			return true;

		size_t i = 0;
		if (text[i] == L'+' || text[i] == L'-')
			i++;

		bool hasDecimalPoint = false;
		for (; i < text.size(); i++)
		{
			const wchar_t ch = text[i];
			if (iswdigit(ch))
				continue;
			if (ch == L'.' && !hasDecimalPoint)
			{
				hasDecimalPoint = true;
				continue;
			}
			return false;
		}
		return true;
	}

	static bool TryReadClipboardText(HWND owner, std::wstring& text)
	{
		text.clear();
		if (!OpenClipboard(owner))
			return false;

		bool success = false;
		if (IsClipboardFormatAvailable(CF_UNICODETEXT))
		{
			HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
			const wchar_t* clipboardText = hClip ? static_cast<const wchar_t*>(GlobalLock(hClip)) : nullptr;
			if (clipboardText)
			{
				text = clipboardText;
				GlobalUnlock(hClip);
				success = true;
			}
		}
		CloseClipboard();
		return success;
	}

	static bool WriteClipboardText(HWND owner, const std::wstring& text)
	{
		if (text.empty() || !OpenClipboard(owner))
			return false;

		bool success = false;
		if (EmptyClipboard())
		{
			const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
			HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (hData)
			{
				wchar_t* data = static_cast<wchar_t*>(GlobalLock(hData));
				if (data)
				{
					memcpy(data, text.c_str(), bytes);
					GlobalUnlock(hData);
					if (SetClipboardData(CF_UNICODETEXT, hData))
					{
						success = true;
						hData = nullptr;
					}
				}
				if (hData)
					GlobalFree(hData);
			}
		}

		CloseClipboard();
		return success;
	}

	static bool PropertyGridColorEquals(
		const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
	{
		return std::fabs(left.r - right.r) < 1e-6f
			&& std::fabs(left.g - right.g) < 1e-6f
			&& std::fabs(left.b - right.b) < 1e-6f
			&& std::fabs(left.a - right.a) < 1e-6f;
	}

	template<typename TValue>
	ControlPropertyOptions<PropertyGridView, TValue> PropertyGridPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<PropertyGridView, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto PropertyGridPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			PropertyGridView& target,
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

	ControlPropertyOptions<PropertyGridView, float> PropertyGridMetricOptions(
		float defaultValue, int order)
	{
		auto options = PropertyGridPropertyOptions(
			defaultValue, L"Layout", 100, order,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsArrange | ControlPropertyFlags::AffectsRender);
		options.Coerce = [](
			PropertyGridView&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	ControlPropertyOptions<PropertyGridView, D2D1_COLOR_F> PropertyGridColorOptions(
		D2D1_COLOR_F defaultValue, int order)
	{
		auto options = PropertyGridPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = PropertyGridColorEquals;
		return options;
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

void PropertyGridView::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
#define CUI_REGISTER_PROPERTY_GRID_BOOL(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<PropertyGridView, bool>(propertyName, \
			[](PropertyGridView& target) { return target.name; }, \
			[](PropertyGridView& target, const bool& value) { target.name = value; }, \
			PropertyGridPropertySubscriber(propertyName), \
			PropertyGridPropertyOptions(defaultValue, L"Behavior", 110, order, \
				ControlPropertyEditorKind::Boolean))

		CUI_REGISTER_PROPERTY_GRID_BOOL(ShowHeader, L"ShowHeader", true, 10);
		CUI_REGISTER_PROPERTY_GRID_BOOL(ShowCategories, L"ShowCategories", true, 20);
		CUI_REGISTER_PROPERTY_GRID_BOOL(AlternatingRows, L"AlternatingRows", true, 30);
		{
			auto options = PropertyGridPropertyOptions(
				true, L"Behavior", 110, 40,
				ControlPropertyEditorKind::Boolean);
			options.Changed = [](
				PropertyGridView& target, const bool&, const bool& value)
			{
				if (!value) target.CancelEdit();
			};
			BindingPropertyRegistry::Register<PropertyGridView, bool>(L"AllowEditing",
				[](PropertyGridView& target) { return target.AllowEditing; },
				[](PropertyGridView& target, const bool& value) { target.AllowEditing = value; },
				PropertyGridPropertySubscriber(L"AllowEditing"), std::move(options));
		}

#undef CUI_REGISTER_PROPERTY_GRID_BOOL

#define CUI_REGISTER_PROPERTY_GRID_METRIC(name, propertyName, defaultValue, order) \
		{ \
			auto options = PropertyGridMetricOptions(defaultValue, order); \
			options.Changed = [](PropertyGridView& target, const float&, const float&) \
			{ target.SetScrollOffset(target.ScrollYOffset); }; \
			BindingPropertyRegistry::Register<PropertyGridView, float>(propertyName, \
				[](PropertyGridView& target) { return target.name; }, \
				[](PropertyGridView& target, const float& value) { target.name = value; }, \
				PropertyGridPropertySubscriber(propertyName), std::move(options)); \
		}

		CUI_REGISTER_PROPERTY_GRID_METRIC(Border, L"Border", 1.0f, 10);
		CUI_REGISTER_PROPERTY_GRID_METRIC(CornerRadius, L"CornerRadius", 6.0f, 20);
		CUI_REGISTER_PROPERTY_GRID_METRIC(HeaderHeight, L"HeaderHeight", 28.0f, 30);
		CUI_REGISTER_PROPERTY_GRID_METRIC(CategoryHeight, L"CategoryHeight", 26.0f, 40);
		CUI_REGISTER_PROPERTY_GRID_METRIC(RowHeight, L"RowHeight", 28.0f, 50);
		CUI_REGISTER_PROPERTY_GRID_METRIC(NameColumnWidth, L"NameColumnWidth", 130.0f, 60);
		CUI_REGISTER_PROPERTY_GRID_METRIC(SplitterWidth, L"SplitterWidth", 5.0f, 70);
		CUI_REGISTER_PROPERTY_GRID_METRIC(ScrollBarSize, L"ScrollBarSize", 8.0f, 80);
		CUI_REGISTER_PROPERTY_GRID_METRIC(CellPaddingX, L"CellPaddingX", 8.0f, 90);
		CUI_REGISTER_PROPERTY_GRID_METRIC(EditTextMargin, L"EditTextMargin", 3.0f, 100);

#undef CUI_REGISTER_PROPERTY_GRID_METRIC

		auto wheelOptions = PropertyGridPropertyOptions(
			48, L"Behavior", 110, 50,
			ControlPropertyEditorKind::Number);
		wheelOptions.Coerce = [](
			PropertyGridView&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		wheelOptions.Design.Minimum = 0.0;
		wheelOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<PropertyGridView, int>(L"MouseWheelStep",
			[](PropertyGridView& target) { return target.MouseWheelStep; },
			[](PropertyGridView& target, const int& value) { target.MouseWheelStep = value; },
			PropertyGridPropertySubscriber(L"MouseWheelStep"), std::move(wheelOptions));

		auto indexOptions = PropertyGridPropertyOptions(
			-1, L"Behavior", 110, 100,
			ControlPropertyEditorKind::Number);
		indexOptions.Coerce = [](
			PropertyGridView& target, const int& proposed) -> std::optional<int>
		{
			if (proposed < 0) return -1;
			return target.Items.empty()
				? proposed
				: (std::min)(proposed, static_cast<int>(target.Items.size()) - 1);
		};
		indexOptions.Changed = [](
			PropertyGridView& target, const int&, const int& value)
		{
			target.SelectionChanged(&target, value);
		};
		indexOptions.Design.Browsable = false;
		indexOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<PropertyGridView, int>(L"SelectedIndex",
			[](PropertyGridView& target) { return target.SelectedIndex; },
			[](PropertyGridView& target, const int& value) { target.SelectedIndex = value; },
			PropertyGridPropertySubscriber(L"SelectedIndex"), std::move(indexOptions));

		auto hoverOptions = PropertyGridPropertyOptions(
			-1, L"Behavior", 110, 110,
			ControlPropertyEditorKind::Number);
		hoverOptions.Coerce = [](
			PropertyGridView& target, const int& proposed) -> std::optional<int>
		{
			if (target.Items.empty() || proposed < 0) return -1;
			return (std::min)(proposed, static_cast<int>(target.Items.size()) - 1);
		};
		hoverOptions.Design.Browsable = false;
		hoverOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<PropertyGridView, int>(L"HoveredIndex",
			[](PropertyGridView& target) { return target.HoveredIndex; },
			[](PropertyGridView& target, const int& value) { target.HoveredIndex = value; },
			PropertyGridPropertySubscriber(L"HoveredIndex"), std::move(hoverOptions));

		auto scrollOptions = PropertyGridPropertyOptions(
			0.0f, L"Behavior", 110, 120,
			ControlPropertyEditorKind::Number);
		scrollOptions.Coerce = [](
			PropertyGridView& target, const float& proposed) -> std::optional<float>
		{
			if (!std::isfinite(proposed)) return std::nullopt;
			const auto rows = target.BuildRows();
			const auto layout = target.CalcLayout(rows);
			return (std::clamp)(proposed, 0.0f, layout.MaxScrollY);
		};
		scrollOptions.Changed = [](
			PropertyGridView& target, const float&, const float&)
		{
			target.ScrollChanged(&target);
		};
		scrollOptions.Design.Browsable = false;
		scrollOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<PropertyGridView, float>(L"ScrollYOffset",
			[](PropertyGridView& target) { return target.ScrollYOffset; },
			[](PropertyGridView& target, const float& value) { target.ScrollYOffset = value; },
			PropertyGridPropertySubscriber(L"ScrollYOffset"), std::move(scrollOptions));

#define CUI_REGISTER_PROPERTY_GRID_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<PropertyGridView, D2D1_COLOR_F>(propertyName, \
			[](PropertyGridView& target) { return target.name; }, \
			[](PropertyGridView& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			PropertyGridPropertySubscriber(propertyName), \
			PropertyGridColorOptions(defaultValue, order))

		CUI_REGISTER_PROPERTY_GRID_COLOR(HeaderBackColor, L"HeaderBackColor",
			(D2D1_COLOR_F{ 0.18f, 0.22f, 0.28f, 0.95f }), 10);
		CUI_REGISTER_PROPERTY_GRID_COLOR(HeaderForeColor, L"HeaderForeColor",
			(D2D1_COLOR_F{ 0.90f, 0.93f, 0.98f, 1.0f }), 20);
		CUI_REGISTER_PROPERTY_GRID_COLOR(CategoryBackColor, L"CategoryBackColor",
			(D2D1_COLOR_F{ 0.20f, 0.23f, 0.29f, 0.82f }), 30);
		CUI_REGISTER_PROPERTY_GRID_COLOR(CategoryForeColor, L"CategoryForeColor",
			(D2D1_COLOR_F{ 0.92f, 0.94f, 0.98f, 1.0f }), 40);
		CUI_REGISTER_PROPERTY_GRID_COLOR(GridLineColor, L"GridLineColor",
			(D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.28f }), 50);
		CUI_REGISTER_PROPERTY_GRID_COLOR(AlternateRowBackColor, L"AlternateRowBackColor",
			(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f }), 60);
		CUI_REGISTER_PROPERTY_GRID_COLOR(SelectedItemBackColor, L"SelectedItemBackColor",
			(D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.28f }), 70);
		CUI_REGISTER_PROPERTY_GRID_COLOR(UnderMouseItemBackColor, L"UnderMouseItemBackColor",
			(D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.12f }), 80);
		CUI_REGISTER_PROPERTY_GRID_COLOR(ReadOnlyForeColor, L"ReadOnlyForeColor",
			(D2D1_COLOR_F{ 0.58f, 0.62f, 0.70f, 1.0f }), 90);
		CUI_REGISTER_PROPERTY_GRID_COLOR(AccentColor, L"AccentColor",
			(D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f }), 100);
		CUI_REGISTER_PROPERTY_GRID_COLOR(EditBackColor, L"EditBackColor", Colors::White, 110);
		CUI_REGISTER_PROPERTY_GRID_COLOR(EditForeColor, L"EditForeColor", Colors::Black, 120);
		CUI_REGISTER_PROPERTY_GRID_COLOR(EditSelectedBackColor, L"EditSelectedBackColor",
			(D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.30f }), 130);
		CUI_REGISTER_PROPERTY_GRID_COLOR(EditSelectedForeColor, L"EditSelectedForeColor", Colors::Black, 140);
		CUI_REGISTER_PROPERTY_GRID_COLOR(CheckBackColor, L"CheckBackColor", Colors::White, 150);
		CUI_REGISTER_PROPERTY_GRID_COLOR(CheckBorderColor, L"CheckBorderColor",
			(D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 1.0f }), 160);
		CUI_REGISTER_PROPERTY_GRID_COLOR(ScrollBackColor, L"ScrollBackColor", Colors::LightGray, 170);
		CUI_REGISTER_PROPERTY_GRID_COLOR(ScrollForeColor, L"ScrollForeColor", Colors::DimGrey, 180);

#undef CUI_REGISTER_PROPERTY_GRID_COLOR

		return true;
	}();
	(void)registered;
}

#define CUI_PROPERTY_GRID_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(PropertyGridView, type, name) { return field; } \
	type PropertyGridView::Get##name() const { return field; } \
	SET_CPP(PropertyGridView, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_PROPERTY_GRID_PROPERTY_IMPL(bool, ShowHeader, _showHeader, L"ShowHeader")
CUI_PROPERTY_GRID_PROPERTY_IMPL(bool, ShowCategories, _showCategories, L"ShowCategories")
CUI_PROPERTY_GRID_PROPERTY_IMPL(bool, AlternatingRows, _alternatingRows, L"AlternatingRows")
CUI_PROPERTY_GRID_PROPERTY_IMPL(bool, AllowEditing, _allowEditing, L"AllowEditing")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, Border, _border, L"Border")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, CornerRadius, _cornerRadius, L"CornerRadius")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, HeaderHeight, _headerHeight, L"HeaderHeight")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, CategoryHeight, _categoryHeight, L"CategoryHeight")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, RowHeight, _rowHeight, L"RowHeight")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, NameColumnWidth, _nameColumnWidth, L"NameColumnWidth")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, SplitterWidth, _splitterWidth, L"SplitterWidth")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, ScrollBarSize, _scrollBarSize, L"ScrollBarSize")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, CellPaddingX, _cellPaddingX, L"CellPaddingX")
CUI_PROPERTY_GRID_PROPERTY_IMPL(int, MouseWheelStep, _mouseWheelStep, L"MouseWheelStep")
CUI_PROPERTY_GRID_PROPERTY_IMPL(int, SelectedIndex, _selectedIndex, L"SelectedIndex")
CUI_PROPERTY_GRID_PROPERTY_IMPL(int, HoveredIndex, _hoveredIndex, L"HoveredIndex")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, ScrollYOffset, _scrollYOffset, L"ScrollYOffset")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, HeaderBackColor, _headerBackColor, L"HeaderBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, HeaderForeColor, _headerForeColor, L"HeaderForeColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, CategoryBackColor, _categoryBackColor, L"CategoryBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, CategoryForeColor, _categoryForeColor, L"CategoryForeColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, GridLineColor, _gridLineColor, L"GridLineColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, AlternateRowBackColor, _alternateRowBackColor, L"AlternateRowBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, SelectedItemBackColor, _selectedItemBackColor, L"SelectedItemBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, UnderMouseItemBackColor, _underMouseItemBackColor, L"UnderMouseItemBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, ReadOnlyForeColor, _readOnlyForeColor, L"ReadOnlyForeColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, EditBackColor, _editBackColor, L"EditBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, EditForeColor, _editForeColor, L"EditForeColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, EditSelectedBackColor, _editSelectedBackColor, L"EditSelectedBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, EditSelectedForeColor, _editSelectedForeColor, L"EditSelectedForeColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, CheckBackColor, _checkBackColor, L"CheckBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, CheckBorderColor, _checkBorderColor, L"CheckBorderColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, ScrollBackColor, _scrollBackColor, L"ScrollBackColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(D2D1_COLOR_F, ScrollForeColor, _scrollForeColor, L"ScrollForeColor")
CUI_PROPERTY_GRID_PROPERTY_IMPL(float, EditTextMargin, _editTextMargin, L"EditTextMargin")

#undef CUI_PROPERTY_GRID_PROPERTY_IMPL

PropertyGridView::PropertyGridView(int x, int y, int width, int height)
{
	Items.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnItemsCollectionChanged(change); });
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.0f };
	this->BorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 0.72f };
}

PropertyGridView::~PropertyGridView()
{
	CloseAnchorPickerEditor();
	CloseColorPickerEditor();
	CloseDropDownEditor(true);
	if (this->_dropDownPopup)
	{
		delete this->_dropDownPopup;
		this->_dropDownPopup = nullptr;
	}
	this->_dropDownPopupIndex = -1;
	if (this->_colorPicker)
	{
		delete this->_colorPicker;
		this->_colorPicker = nullptr;
	}
	this->_colorPickerIndex = -1;
	if (this->_anchorPicker)
	{
		delete this->_anchorPicker;
		this->_anchorPicker = nullptr;
	}
	this->_anchorPickerIndex = -1;
}

static int RemapPropertyGridIndex(
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

void PropertyGridView::EnsureItemIds()
{
	std::unordered_set<uint32_t> used;
	for (auto& item : Items)
	{
		while (item.CollectionId == 0
			|| !used.insert(item.CollectionId).second)
			item.CollectionId = AllocateAccessibilityVirtualId();
	}
}

void PropertyGridView::OnItemsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	auto oldIdAt = [this](int index) -> uint32_t
	{
		return index >= 0
			&& static_cast<size_t>(index) < _knownItemIds.size()
			? _knownItemIds[static_cast<size_t>(index)] : 0;
	};
	const uint32_t selectedId = oldIdAt(SelectedIndex);
	const uint32_t hoveredId = oldIdAt(HoveredIndex);
	const uint32_t editingId = oldIdAt(_editingIndex);
	const uint32_t dropDownId = oldIdAt(_dropDownPopupIndex);
	const uint32_t colorPickerId = oldIdAt(_colorPickerIndex);
	const uint32_t anchorPickerId = oldIdAt(_anchorPickerIndex);

	EnsureItemIds();
	auto findId = [this](uint32_t id) -> int
	{
		if (id == 0) return -1;
		const auto found = std::find_if(Items.begin(), Items.end(),
			[id](const PropertyGridItem& item)
			{ return item.CollectionId == id; });
		return found == Items.end()
			? -1 : static_cast<int>(found - Items.begin());
	};

	int nextSelected = findId(selectedId);
	if (nextSelected < 0)
	{
		const auto source = GetPropertyValueSource(L"SelectedIndex");
		if (selectedId != 0
			&& (change.Action == CollectionChangeAction::Remove
				|| change.Action == CollectionChangeAction::Replace))
			nextSelected = -1;
		else if (change.Action == CollectionChangeAction::Reset
			|| (selectedId == 0
				&& source != ControlPropertyValueSource::Default))
		{
			(void)ReevaluatePropertyValue(L"SelectedIndex");
			nextSelected = SelectedIndex;
		}
		else
			nextSelected = RemapPropertyGridIndex(
				SelectedIndex, change, Items.size());
	}
	if (nextSelected < -1
		|| nextSelected >= static_cast<int>(Items.size()))
		nextSelected = -1;
	SetCurrentSelectedIndex(nextSelected);

	int nextHovered = findId(hoveredId);
	if (nextHovered < 0)
		nextHovered = change.Action == CollectionChangeAction::Reset
			? -1 : RemapPropertyGridIndex(
				HoveredIndex, change, Items.size());
	SetCurrentHoveredIndex(nextHovered);

	if (_editing)
	{
		const int nextEditing = findId(editingId);
		if (nextEditing >= 0) _editingIndex = nextEditing;
		else CancelEdit();
	}
	if (_dropDownPopupIndex >= 0)
	{
		const int nextDropDown = findId(dropDownId);
		if (nextDropDown >= 0) _dropDownPopupIndex = nextDropDown;
		else CloseDropDownEditor(true);
	}
	if (_colorPickerIndex >= 0)
	{
		const int nextColorPicker = findId(colorPickerId);
		if (nextColorPicker >= 0) _colorPickerIndex = nextColorPicker;
		else
		{
			CloseColorPickerEditor();
			_colorPickerIndex = -1;
		}
	}
	if (_anchorPickerIndex >= 0)
	{
		const int nextAnchorPicker = findId(anchorPickerId);
		if (nextAnchorPicker >= 0) _anchorPickerIndex = nextAnchorPicker;
		else
		{
			CloseAnchorPickerEditor();
			_anchorPickerIndex = -1;
		}
	}

	_knownItemIds.clear();
	_knownItemIds.reserve(Items.size());
	std::unordered_set<std::wstring> categories;
	for (const auto& item : Items)
	{
		_knownItemIds.push_back(item.CollectionId);
		categories.insert(item.Category);
	}
	_collapsedCategories.erase(std::remove_if(
		_collapsedCategories.begin(), _collapsedCategories.end(),
		[&categories](const std::wstring& category)
		{ return !categories.contains(category); }),
		_collapsedCategories.end());
	_categoryAnimations.erase(std::remove_if(
		_categoryAnimations.begin(), _categoryAnimations.end(),
		[&categories](const CategoryAnimation& animation)
		{ return !categories.contains(animation.Category); }),
		_categoryAnimations.end());
	SetScrollOffset(ScrollYOffset);
	InvalidateVisual();
}

void PropertyGridView::Clear()
{
	this->Items.clear();
}

void PropertyGridView::SetItems(std::vector<PropertyGridItem> items)
{
	Items = std::move(items);
}

int PropertyGridView::AddItem(const PropertyGridItem& item)
{
	this->Items.push_back(item);
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
	if (item.Value == value && !item.IsMixed) return false;
	auto oldValue = item.Value;
	item.Value = value;
	item.IsMixed = false;
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

D2D1_RECT_F PropertyGridView::GetResetRect(const D2D1_RECT_F& rowRect) const
{
	auto nameRect = GetNameRect(rowRect);
	const float size = std::max(14.0f,
		std::min(22.0f, RectHeight(nameRect) - 4.0f));
	return D2D1::RectF(
		nameRect.right - size - 3.0f,
		nameRect.top + (RectHeight(nameRect) - size) * 0.5f,
		nameRect.right - 3.0f,
		nameRect.top + (RectHeight(nameRect) + size) * 0.5f);
}

void PropertyGridView::ClampScroll(Layout& layout)
{
	float clamped = std::clamp(this->ScrollYOffset, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 0.1f)
		SetCurrentScrollYOffset(clamped);
}

void PropertyGridView::SetScrollOffset(float offsetY)
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	float clamped = std::clamp(offsetY, 0.0f, layout.MaxScrollY);
	if (std::fabs(clamped - this->ScrollYOffset) > 0.1f)
		SetCurrentScrollYOffset(clamped);
}

void PropertyGridView::SetCurrentSelectedIndex(int value)
{
	if (_selectedIndex == value) return;
	(void)SetCurrentPropertyField(L"SelectedIndex", _selectedIndex, value);
}

void PropertyGridView::SetCurrentHoveredIndex(int value)
{
	if (_hoveredIndex == value) return;
	(void)SetCurrentPropertyField(L"HoveredIndex", _hoveredIndex, value);
}

void PropertyGridView::SetCurrentScrollYOffset(float value)
{
	if (std::fabs(_scrollYOffset - value) <= 1e-6f) return;
	(void)SetCurrentPropertyField(L"ScrollYOffset", _scrollYOffset, value);
}

void PropertyGridView::SetCurrentNameColumnWidth(float value)
{
	if (std::fabs(_nameColumnWidth - value) <= 1e-6f) return;
	(void)SetCurrentPropertyField(L"NameColumnWidth", _nameColumnWidth, value);
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

int PropertyGridView::HitTestItem(int localX, int localY) const
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	for (const auto& row : rows)
	{
		if (row.IsCategory) continue;
		auto rect = GetVisibleRowRect(row, layout);
		if (PtInRectF(rect, (float)localX, (float)localY))
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

bool PropertyGridView::IsValueCell(int localX, int localY, const std::vector<RowInfo>& rows, const Layout& layout, int& itemIndex) const
{
	itemIndex = -1;
	for (const auto& row : rows)
	{
		if (row.IsCategory) continue;
		auto rect = GetRowRect(row, layout);
		auto visibleRect = GetVisibleRowRect(row, layout);
		auto valueRect = IntersectRectF(GetValueRect(rect), visibleRect);
		if (PtInRectF(valueRect, (float)localX, (float)localY))
		{
			itemIndex = row.ItemIndex;
			return true;
		}
	}
	return false;
}

bool PropertyGridView::IsOverSplitter(int localX, int localY) const
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	if (!PtInRectF(layout.ContentRect, (float)localX, (float)localY) && !PtInRectF(layout.HeaderRect, (float)localX, (float)localY))
		return false;
	float splitX = std::clamp(this->NameColumnWidth, 48.0f, std::max(48.0f, RectWidth(layout.ContentRect) - 48.0f));
	return std::fabs((float)localX - splitX) <= std::max(3.0f, this->SplitterWidth);
}

CursorKind PropertyGridView::QueryCursor(int localX, int localY)
{
	if (!this->Enable)
		return CursorKind::Arrow;
	if (_dragSplitter)
		return CursorKind::SizeWE;
	if (_dragVScroll)
		return CursorKind::SizeNS;
	if (IsOverSplitter(localX, localY))
		return CursorKind::SizeWE;

	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	if (layout.NeedVScroll && PtInRectF(layout.ScrollTrackRect, (float)localX, (float)localY))
		return CursorKind::SizeNS;

	for (const auto& row : rows)
	{
		auto rect = GetRowRect(row, layout);
		auto visibleRect = GetVisibleRowRect(row, layout);
		if (IsEmptyRectF(visibleRect) || !PtInRectF(visibleRect, (float)localX, (float)localY))
			continue;
		if (row.IsCategory)
			return CursorKind::Hand;
		const auto& item = this->Items[static_cast<size_t>(row.ItemIndex)];
		if (item.CanReset && PtInRectF(GetResetRect(rect), (float)localX, (float)localY))
			return CursorKind::Hand;
		if (item.ValueType == PropertyGridValueType::Action)
			return CursorKind::Hand;

		auto valueRect = IntersectRectF(GetValueRect(rect), visibleRect);
		if (!PtInRectF(valueRect, (float)localX, (float)localY))
			return CursorKind::Arrow;
		if (!IsEditableItem(row.ItemIndex))
			return CursorKind::Arrow;

		switch (item.ValueType)
		{
		case PropertyGridValueType::Text:
		case PropertyGridValueType::Number:
			return CursorKind::IBeam;
		case PropertyGridValueType::EditableEnum:
			return localX >= valueRect.right - 24.0f
				? CursorKind::Hand : CursorKind::IBeam;
		case PropertyGridValueType::Bool:
		case PropertyGridValueType::Color:
		case PropertyGridValueType::Anchor:
		case PropertyGridValueType::Action:
		case PropertyGridValueType::Slider:
			return CursorKind::Hand;
		case PropertyGridValueType::Enum:
			return item.Options.empty() ? CursorKind::Arrow : CursorKind::Hand;
		default:
			return CursorKind::Arrow;
		}
	}
	return CursorKind::Arrow;
}

bool PropertyGridView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)delta;
	(void)localX;
	(void)localY;
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
	case VK_F4:
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
		const UINT duration = EffectiveAnimationDuration(anim.DurationMs);
		float progress = duration > 0
			? std::clamp((float)(now - anim.StartTick) / (float)duration, 0.0f, 1.0f)
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
	d2d->DrawString(_nameHeaderLabel, nameRect.left + this->CellPaddingX, TextTop(fontObj, nameRect),
		std::max(1.0f, RectWidth(nameRect) - this->CellPaddingX * 2.0f), RectHeight(nameRect), this->HeaderForeColor, fontObj);
	d2d->DrawString(_valueHeaderLabel, valueRect.left + this->CellPaddingX, TextTop(fontObj, valueRect),
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

void PropertyGridView::DrawCheckBox(
	D2DGraphics* d2d,
	const D2D1_RECT_F& rect,
	bool checked,
	bool indeterminate)
{
	const bool active = checked || indeterminate;
	d2d->FillRoundRect(rect, active ? this->AccentColor : this->CheckBackColor, 3.0f);
	d2d->DrawRoundRect(rect, active ? this->AccentColor : this->CheckBorderColor, 1.2f, 3.0f);
	if (indeterminate)
	{
		const float cy = (rect.top + rect.bottom) * 0.5f;
		d2d->DrawLine(
			D2D1::Point2F(rect.left + RectWidth(rect) * 0.24f, cy),
			D2D1::Point2F(rect.right - RectWidth(rect) * 0.24f, cy),
			Colors::White, 1.8f);
	}
	else if (checked)
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
	const float nameTextRight = item.CanReset
		? GetResetRect(rect).left - 2.0f
		: nameRect.right - this->CellPaddingX;
	d2d->DrawString(item.Name, nameRect.left + this->CellPaddingX, TextTop(fontObj, nameRect),
		std::max(1.0f, nameTextRight - nameRect.left - this->CellPaddingX), RectHeight(nameRect), nameColor, fontObj);
	if (item.CanReset)
	{
		const bool emphasized = row.ItemIndex == this->SelectedIndex
			|| row.ItemIndex == this->HoveredIndex;
		DrawResetGlyph(d2d, GetResetRect(rect),
			item.ReadOnly || !emphasized
				? this->ReadOnlyForeColor : this->AccentColor);
	}

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
			const float textHeight = _editingText.empty() ? fontObj->FontHeight : textSize.height;
			offsetY = std::max(0.0f, (RectHeight(editRect) - textHeight) * 0.5f);
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
				const auto abs = this->GetAbsoluteLocationDip();
				caretRect = { abs.x + caretX - 2.0f, abs.y + caretTop - 2.0f, abs.x + caretX + 2.0f, abs.y + caretBottom + 2.0f };
				caretRectValid = true;
			}
			else
			{
				const float caretX = editRect.left + this->EditTextMargin - _editOffsetX;
				const float caretTop = editRect.top + offsetY;
				const float caretBottom = caretTop + fontObj->FontHeight;
				const auto abs = this->GetAbsoluteLocationDip();
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
			const auto absoluteLocation = this->GetAbsoluteLocationDip();
			d2d->DrawLine(
				D2D1::Point2F(caretRect.left - absoluteLocation.x + 2.0f, caretRect.top - absoluteLocation.y + 2.0f),
				D2D1::Point2F(caretRect.left - absoluteLocation.x + 2.0f, caretRect.bottom - absoluteLocation.y - 2.0f),
				Colors::Black, 1.0f);
		}
		d2d->PopDrawRect();
	}
	else if (item.ValueType == PropertyGridValueType::Bool)
	{
		float box = std::min(15.0f, RectHeight(valueRect) - 8.0f);
		auto boxRect = D2D1::RectF(valueRect.left + this->CellPaddingX, valueRect.top + (RectHeight(valueRect) - box) * 0.5f,
			valueRect.left + this->CellPaddingX + box, valueRect.top + (RectHeight(valueRect) + box) * 0.5f);
		DrawCheckBox(d2d, boxRect, TextToBool(item.Value), item.IsMixed);
		const std::wstring boolText = item.IsMixed
			? (item.Value.empty() ? std::wstring(L"\u2014") : item.Value)
			: (TextToBool(item.Value) ? std::wstring(L"True") : std::wstring(L"False"));
		d2d->DrawString(boolText, boxRect.right + 7.0f, TextTop(fontObj, valueRect),
			std::max(1.0f, valueRect.right - boxRect.right - 12.0f), RectHeight(valueRect), valueColor, fontObj);
	}
	else if (item.ValueType == PropertyGridValueType::Slider)
	{
		const double minimum = std::isfinite(item.Minimum) ? item.Minimum : 0.0;
		const double maximum = std::isfinite(item.Maximum) && item.Maximum > minimum
			? item.Maximum : minimum + 1.0;
		double current = minimum;
		TryParseDouble(item.Value, current);
		current = std::clamp(current, minimum, maximum);
		const float textWidth = std::min(54.0f,
			std::max(36.0f, RectWidth(valueRect) * 0.36f));
		const float trackLeft = valueRect.left + this->CellPaddingX;
		const float trackRight = std::max(
			trackLeft + 1.0f, valueRect.right - textWidth - 5.0f);
		const float trackY = valueRect.top + RectHeight(valueRect) * 0.5f;
		const float ratio = item.IsMixed ? 0.5f : static_cast<float>(
			(current - minimum) / (maximum - minimum));
		d2d->DrawLine(trackLeft, trackY, trackRight, trackY,
			FadeColor(this->GridLineColor, 1.8f), 3.0f);
		d2d->DrawLine(trackLeft, trackY,
			trackLeft + (trackRight - trackLeft) * ratio, trackY,
			this->AccentColor, 3.0f);
		const float thumbX = trackLeft + (trackRight - trackLeft) * ratio;
		d2d->FillEllipse(thumbX, trackY, 4.5f, 4.5f,
			item.IsMixed ? this->ReadOnlyForeColor : this->AccentColor);
		d2d->DrawEllipse(thumbX, trackY, 5.3f, 5.3f,
			this->BackColor, 1.0f);
		d2d->DrawString(item.Value, trackRight + 6.0f,
			TextTop(fontObj, valueRect),
			std::max(1.0f, valueRect.right - trackRight - 8.0f),
			RectHeight(valueRect), valueColor, fontObj);
	}
	else if (item.ValueType == PropertyGridValueType::Action)
	{
		auto actionRect = D2D1::RectF(
			valueRect.left + 4.0f, valueRect.top + 4.0f,
			valueRect.right - 4.0f, valueRect.bottom - 4.0f);
		d2d->FillRoundRect(actionRect,
			FadeColor(this->AccentColor, row.ItemIndex == this->HoveredIndex ? 0.18f : 0.09f), 4.0f);
		d2d->DrawRoundRect(actionRect, FadeColor(this->AccentColor, 0.75f), 1.0f, 4.0f);
		d2d->DrawString(item.Value.empty() ? L"\u7f16\u8f91\u2026" : item.Value,
			actionRect.left + this->CellPaddingX, TextTop(fontObj, actionRect),
			std::max(1.0f, RectWidth(actionRect) - this->CellPaddingX * 2.0f),
			RectHeight(actionRect), valueColor, fontObj);
	}
	else
	{
		auto contentRect = valueRect;
		float textLeft = contentRect.left + this->CellPaddingX;
		float textRight = contentRect.right - this->CellPaddingX;
		const bool hasDropArrow = editable &&
			(((item.ValueType == PropertyGridValueType::Enum ||
				item.ValueType == PropertyGridValueType::EditableEnum) && !item.Options.empty())
				|| item.ValueType == PropertyGridValueType::Color
				|| item.ValueType == PropertyGridValueType::Anchor);
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
		std::wstring displayValue = item.Value;
		if (item.ValueType == PropertyGridValueType::Anchor && !item.IsMixed)
		{
			uint8_t anchors = AnchorStyles::None;
			if (AnchorPickerPopup::TryParseAnchors(item.Value, anchors))
			{
				const float iconSize = std::min(18.0f,
					std::max(12.0f, RectHeight(contentRect) - 10.0f));
				const auto iconRect = D2D1::RectF(
					contentRect.left + this->CellPaddingX,
					contentRect.top + (RectHeight(contentRect) - iconSize) * 0.5f,
					contentRect.left + this->CellPaddingX + iconSize,
					contentRect.top + (RectHeight(contentRect) + iconSize) * 0.5f);
				d2d->DrawRect(iconRect, this->GridLineColor, 1.0f);
				const auto childRect = D2D1::RectF(
					iconRect.left + iconSize * 0.31f,
					iconRect.top + iconSize * 0.31f,
					iconRect.right - iconSize * 0.31f,
					iconRect.bottom - iconSize * 0.31f);
				d2d->FillRect(childRect, FadeColor(this->AccentColor, 0.48f));
				const float cx = (iconRect.left + iconRect.right) * 0.5f;
				const float cy = (iconRect.top + iconRect.bottom) * 0.5f;
				if ((anchors & AnchorStyles::Top) != 0)
					d2d->DrawLine(cx, iconRect.top, cx, childRect.top,
						this->AccentColor, 1.6f);
				if ((anchors & AnchorStyles::Left) != 0)
					d2d->DrawLine(iconRect.left, cy, childRect.left, cy,
						this->AccentColor, 1.6f);
				if ((anchors & AnchorStyles::Right) != 0)
					d2d->DrawLine(childRect.right, cy, iconRect.right, cy,
						this->AccentColor, 1.6f);
				if ((anchors & AnchorStyles::Bottom) != 0)
					d2d->DrawLine(cx, childRect.bottom, cx, iconRect.bottom,
						this->AccentColor, 1.6f);
				textLeft = iconRect.right + 7.0f;
				displayValue = AnchorPickerPopup::AnchorToString(anchors);
			}
		}
		d2d->DrawString(displayValue, textLeft, TextTop(fontObj, contentRect),
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
			else if (item.ValueType == PropertyGridValueType::Anchor &&
				this->_anchorPicker &&
				this->_anchorPickerIndex == row.ItemIndex)
			{
				arrowProgress = this->_anchorPicker->CurrentDropProgress();
			}
			else if ((item.ValueType == PropertyGridValueType::Enum ||
				item.ValueType == PropertyGridValueType::EditableEnum) &&
				IsDropDownEditorOpenFor(row.ItemIndex))
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

void PropertyGridView::UpdateHover(int localX, int localY)
{
	int index = HitTestItem(localX, localY);
	if (index != this->HoveredIndex)
		SetCurrentHoveredIndex(index);
}

void PropertyGridView::UpdateScrollByThumb(float localY)
{
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	if (!layout.NeedVScroll) return;
	float trackH = RectHeight(layout.ScrollTrackRect);
	float thumbH = RectHeight(layout.ScrollThumbRect);
	float movable = std::max(1.0f, trackH - thumbH);
	float newTop = std::clamp(localY - _scrollThumbGrabOffsetY, layout.ScrollTrackRect.top, layout.ScrollTrackRect.bottom - thumbH);
	SetScrollOffset(((newTop - layout.ScrollTrackRect.top) / movable) * layout.MaxScrollY);
}

bool PropertyGridView::SelectItem(int index, bool ensureVisible)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	const bool changed = this->SelectedIndex != index;
	SetCurrentSelectedIndex(index);
	if (ensureVisible) EnsureVisible(index);
	return changed;
}

bool PropertyGridView::ClearSelection()
{
	if (this->SelectedIndex < 0) return false;
	SetCurrentSelectedIndex(-1);
	return true;
}

bool PropertyGridView::ActivateItem(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	SelectItem(index);
	this->OnItemClick(this, index);
	return true;
}

bool PropertyGridView::RequestReset(int index)
{
	if (index < 0 || index >= (int)this->Items.size()) return false;
	const auto& item = this->Items[index];
	if (!item.CanReset || item.ReadOnly
		|| item.ValueType == PropertyGridValueType::ReadOnly) return false;
	SelectItem(index);
	this->OnResetRequested(this, index);
	return true;
}

void PropertyGridView::SetHeaderLabels(
	std::wstring nameCaption,
	std::wstring valueCaption)
{
	if (_nameHeaderLabel == nameCaption && _valueHeaderLabel == valueCaption)
		return;
	_nameHeaderLabel = std::move(nameCaption);
	_valueHeaderLabel = std::move(valueCaption);
	InvalidateVisual();
}

bool PropertyGridView::IsEditableItem(int index) const
{
	if (!this->AllowEditing) return false;
	if (index < 0 || index >= (int)this->Items.size()) return false;
	const auto& item = this->Items[index];
	return !item.ReadOnly && item.ValueType != PropertyGridValueType::ReadOnly;
}

bool PropertyGridView::BeginEdit(int index)
{
	if (!IsEditableItem(index)) return false;
	if (_editing && _editingIndex == index)
	{
		if (this->ParentForm)
			this->ParentForm->Selected = this;
		EditSetImeCompositionWindow();
		this->InvalidateVisual();
		return true;
	}
	auto& item = this->Items[index];
	if (item.ValueType == PropertyGridValueType::Bool ||
		item.ValueType == PropertyGridValueType::Enum ||
		item.ValueType == PropertyGridValueType::Color ||
		item.ValueType == PropertyGridValueType::Anchor ||
		item.ValueType == PropertyGridValueType::Action ||
		item.ValueType == PropertyGridValueType::Slider)
		return false;
	CloseDropDownEditor();
	_editing = true;
	_editingIndex = index;
	_editingText = item.Value;
	_editingOriginalText = item.Value;
	_editSelectionStart = 0;
	_editSelectionEnd = (int)_editingText.size();
	_editOffsetX = 0.0f;
	if (this->ParentForm)
		this->ParentForm->Selected = this;
	EditSetImeCompositionWindow();
	this->InvalidateVisual();
	return true;
}

bool PropertyGridView::CommitEdit()
{
	if (!_editing) return false;
	int index = _editingIndex;
	std::wstring value = _editingText;
	_editing = false;
	_editingIndex = -1;
	_editingText.clear();
	_editingOriginalText.clear();
	_dragEditSelection = false;
	_editSelectionStart = _editSelectionEnd = 0;
	_editOffsetX = 0.0f;
	SetValue(index, value);
	this->InvalidateVisual();
	return true;
}

bool PropertyGridView::CancelEdit()
{
	if (!_editing) return false;
	_editing = false;
	_editingIndex = -1;
	_editingText.clear();
	_editingOriginalText.clear();
	_dragEditSelection = false;
	_editSelectionStart = _editSelectionEnd = 0;
	_editOffsetX = 0.0f;
	this->InvalidateVisual();
	return true;
}

bool PropertyGridView::SetEditingText(const std::wstring& text)
{
	if (!_editing || !IsEditingTextAllowed(text)) return false;
	if (_editingText == text) return true;
	_editingText = text;
	_editSelectionStart = _editSelectionEnd = (int)_editingText.size();
	InvalidateVisual();
	return true;
}

bool PropertyGridView::IsEditingTextAllowed(const std::wstring& text) const
{
	if (_editingIndex < 0 || _editingIndex >= (int)this->Items.size())
		return true;
	if (this->Items[(size_t)_editingIndex].ValueType != PropertyGridValueType::Number)
		return true;
	return IsNumberEditCandidate(text);
}

void PropertyGridView::InputEditText(std::wstring input)
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	std::wstring newText = _editingText;
	int selectionStart = _editSelectionStart;
	int selectionEnd = _editSelectionEnd;
	auto result = CuiTextEdit::ReplaceSelection(newText, selectionStart, selectionEnd, input, PropertyGridEditOptions());
	if (!result.applied || !IsEditingTextAllowed(newText))
		return;
	_editingText = std::move(newText);
	_editSelectionStart = selectionStart;
	_editSelectionEnd = selectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::BackspaceEdit()
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	std::wstring newText = _editingText;
	int selectionStart = _editSelectionStart;
	int selectionEnd = _editSelectionEnd;
	auto result = CuiTextEdit::Backspace(newText, selectionStart, selectionEnd, PropertyGridEditOptions());
	if (!result.applied || !IsEditingTextAllowed(newText))
		return;
	_editingText = std::move(newText);
	_editSelectionStart = selectionStart;
	_editSelectionEnd = selectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::DeleteEdit()
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	std::wstring newText = _editingText;
	int selectionStart = _editSelectionStart;
	int selectionEnd = _editSelectionEnd;
	auto result = CuiTextEdit::DeleteForward(newText, selectionStart, selectionEnd, PropertyGridEditOptions());
	if (!result.applied || !IsEditingTextAllowed(newText))
		return;
	_editingText = std::move(newText);
	_editSelectionStart = selectionStart;
	_editSelectionEnd = selectionEnd;
	this->InvalidateVisual();
}

void PropertyGridView::MoveEditCaret(int delta)
{
	if (!_editing) return;
	EditEnsureSelectionInRange();
	if (delta == 0) return;
	const bool extendSelection = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	auto span = CuiTextEdit::NormalizeSelection(_editSelectionStart, _editSelectionEnd, _editingText.size());
	if (!extendSelection && span.HasSelection())
	{
		_editSelectionEnd = delta < 0 ? span.start : span.end;
		_editSelectionStart = _editSelectionEnd;
	}
	else
	{
		_editSelectionEnd = delta < 0
			? CuiTextEdit::GetPreviousCaretIndex(_editingText, _editSelectionEnd, false)
			: CuiTextEdit::GetNextCaretIndex(_editingText, _editSelectionEnd, false);
		if (!extendSelection)
			_editSelectionStart = _editSelectionEnd;
	}
	this->InvalidateVisual();
}

void PropertyGridView::EditEnsureSelectionInRange()
{
	if (_editSelectionStart < 0) _editSelectionStart = 0;
	if (_editSelectionEnd < 0) _editSelectionEnd = 0;
	int maxLen = (int)_editingText.size();
	if (_editSelectionStart > maxLen) _editSelectionStart = maxLen;
	if (_editSelectionEnd > maxLen) _editSelectionEnd = maxLen;
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

bool PropertyGridView::SetEditingCaretFromMousePoint(int localX, int localY, const D2D1_RECT_F& valueRect)
{
	if (!_editing) return false;
	auto editRect = D2D1::RectF(valueRect.left + 3.0f, valueRect.top + 3.0f, valueRect.right - 3.0f, valueRect.bottom - 3.0f);
	float cellWidth = RectWidth(editRect);
	float cellHeight = RectHeight(editRect);
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, (float)localX - editRect.left, (float)localY - editRect.top);
	_editSelectionStart = _editSelectionEnd = std::clamp(pos, 0, (int)_editingText.size());
	EditUpdateScroll(cellWidth);
	this->InvalidateVisual();
	return true;
}

bool PropertyGridView::UpdateEditingSelectionFromMousePoint(int localX, int localY, const D2D1_RECT_F& valueRect)
{
	if (!_editing) return false;
	auto editRect = D2D1::RectF(valueRect.left + 3.0f, valueRect.top + 3.0f, valueRect.right - 3.0f, valueRect.bottom - 3.0f);
	float cellWidth = RectWidth(editRect);
	float cellHeight = RectHeight(editRect);
	float editLocalX = std::clamp((float)localX, editRect.left, editRect.right) - editRect.left;
	float editLocalY = std::clamp((float)localY, editRect.top, editRect.bottom) - editRect.top;
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, editLocalX, editLocalY);
	_editSelectionEnd = std::clamp(pos, 0, (int)_editingText.size());
	EditUpdateScroll(cellWidth);
	this->InvalidateVisual();
	return true;
}

std::wstring PropertyGridView::EditGetSelectedString() const
{
	auto span = CuiTextEdit::NormalizeSelection(_editSelectionStart, _editSelectionEnd, _editingText.size());
	if (!span.HasSelection())
		return L"";
	return _editingText.substr((size_t)span.start, (size_t)span.Length());
}

void PropertyGridView::EditSetImeCompositionWindow()
{
	if (!this->ParentForm || !this->ParentForm->Handle || !_editing) return;
	auto rows = BuildRows();
	auto layout = CalcLayout(rows);
	D2D1_RECT_F valueRect{};
	if (!GetValueRectForItem(_editingIndex, rows, layout, valueRect)) return;
	EditEnsureSelectionInRange();

	auto editRect = D2D1::RectF(valueRect.left + 3.0f, valueRect.top + 3.0f, valueRect.right - 3.0f, valueRect.bottom - 3.0f);
	float renderHeight = RectHeight(editRect) - (this->EditTextMargin * 2.0f);
	if (renderHeight < 0.0f) renderHeight = 0.0f;
	class Font* fontObj = this->Font;
	float caretX = editRect.left + this->EditTextMargin - _editOffsetX;
	float caretTop = editRect.top + this->EditTextMargin;
	float caretBottom = caretTop + (fontObj ? fontObj->FontHeight : 16.0f);
	if (fontObj)
	{
		float offsetY = 0.0f;
		auto textSize = fontObj->GetTextSize(_editingText, FLT_MAX, renderHeight);
		const float textHeight = _editingText.empty() ? fontObj->FontHeight : textSize.height;
		offsetY = std::max(0.0f, (RectHeight(editRect) - textHeight) * 0.5f);
		auto hit = fontObj->HitTestTextRange(_editingText, (UINT32)_editSelectionEnd, (UINT32)0);
		if (!hit.empty())
		{
			caretX = editRect.left + this->EditTextMargin + hit[0].left - _editOffsetX;
			caretTop = editRect.top + offsetY + hit[0].top;
			caretBottom = caretTop + (hit[0].height > 0.0f ? hit[0].height : fontObj->FontHeight);
		}
		else
		{
			caretTop = editRect.top + offsetY;
			caretBottom = caretTop + fontObj->FontHeight;
		}
	}

	const auto pos = this->GetAbsoluteLocationDip();
	this->ParentForm->SetImeCompositionWindowFromLogicalRect(
		D2D1_RECT_F{
			(float)pos.x + caretX,
			(float)pos.y + caretTop,
			(float)pos.x + caretX + 1.0f,
			(float)pos.y + caretBottom
		});
}

void PropertyGridView::ToggleBool(int index)
{
	if (!IsEditableItem(index)) return;
	SetValue(index, TextToBool(this->Items[index].Value) ? L"False" : L"True");
}

void PropertyGridView::UpdateSliderFromPoint(
	int index,
	float localX,
	const D2D1_RECT_F& valueRect)
{
	if (!IsEditableItem(index)
		|| index < 0 || index >= static_cast<int>(Items.size())) return;
	auto& item = Items[static_cast<size_t>(index)];
	if (item.ValueType != PropertyGridValueType::Slider) return;
	const double minimum = std::isfinite(item.Minimum) ? item.Minimum : 0.0;
	const double maximum = std::isfinite(item.Maximum) && item.Maximum > minimum
		? item.Maximum : minimum + 1.0;
	const double step = std::isfinite(item.Step) && item.Step > 0.0
		? item.Step : (maximum - minimum) / 100.0;
	const float textWidth = std::min(54.0f,
		std::max(36.0f, RectWidth(valueRect) * 0.36f));
	const float trackLeft = valueRect.left + this->CellPaddingX;
	const float trackRight = std::max(
		trackLeft + 1.0f, valueRect.right - textWidth - 5.0f);
	const double ratio = std::clamp(
		(static_cast<double>(localX) - trackLeft)
		/ std::max(1.0, static_cast<double>(trackRight - trackLeft)),
		0.0, 1.0);
	double value = minimum + (maximum - minimum) * ratio;
	value = minimum + std::round((value - minimum) / step) * step;
	value = std::clamp(value, minimum, maximum);
	SetValue(index, FormatSliderValue(value));
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
	CloseAnchorPickerEditor();

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
	SetCurrentSelectedIndex(index);
	this->_colorPicker->ShowAt(this, valueRect, initial);
	this->InvalidateVisual();
}

void PropertyGridView::CloseAnchorPickerEditor()
{
	if (!this->_anchorPicker) return;
	this->_anchorPicker->Hide(false);
}

void PropertyGridView::OpenAnchorPickerEditor(
	int index, const D2D1_RECT_F& valueRect)
{
	if (index < 0 || index >= static_cast<int>(this->Items.size())) return;
	if (!this->ParentForm || !IsEditableItem(index)) return;
	if (this->Items[static_cast<size_t>(index)].ValueType
		!= PropertyGridValueType::Anchor) return;

	if (this->_anchorPicker
		&& this->ParentForm->ForegroundControl == this->_anchorPicker
		&& this->_anchorPicker->Visible
		&& this->_anchorPickerIndex == index)
	{
		CloseAnchorPickerEditor();
		this->ParentForm->Invalidate(true);
		this->InvalidateVisual();
		return;
	}

	CommitEdit();
	CloseDropDownEditor();
	CloseColorPickerEditor();

	if (!this->_anchorPicker)
		this->_anchorPicker = new AnchorPickerPopup();

	uint8_t initial = AnchorStyles::None;
	AnchorPickerPopup::TryParseAnchors(
		this->Items[static_cast<size_t>(index)].Value, initial);
	this->_anchorPicker->ParentForm = this->ParentForm;
	this->_anchorPicker->SetFontEx(this->Font, false);
	this->_anchorPicker->AccentColor = this->AccentColor;
	this->_anchorPicker->OnAnchorChanged.Clear();
	this->_anchorPicker->OnAnchorConfirmed.Clear();
	this->_anchorPicker->OnCancelled.Clear();
	this->_anchorPicker->OnAnchorConfirmed +=
		[this, index](AnchorPickerPopup* sender,
			uint8_t anchors, std::wstring value)
		{
			(void)sender;
			(void)anchors;
			if (index >= 0 && index < static_cast<int>(this->Items.size()))
				SetValue(index, value);
		};
	this->_anchorPicker->OnCancelled +=
		[this](AnchorPickerPopup* sender) { (void)sender; };

	this->_anchorPickerIndex = index;
	SetCurrentSelectedIndex(index);
	this->_anchorPicker->ShowAt(this, valueRect, initial);
	this->InvalidateVisual();
}

void PropertyGridView::ToggleDropDownEditor(int index, const D2D1_RECT_F& valueRect)
{
	if (index < 0 || index >= (int)this->Items.size()) return;
	if (!this->ParentForm) return;
	if (!IsEditableItem(index)) return;
	auto& item = this->Items[index];
	if ((item.ValueType != PropertyGridValueType::Enum &&
		item.ValueType != PropertyGridValueType::EditableEnum) || item.Options.empty()) return;

	if (IsDropDownEditorOpenFor(index))
	{
		CloseDropDownEditor();
		this->ParentForm->Invalidate(true);
		this->InvalidateVisual();
		return;
	}

	CommitEdit();
	CloseColorPickerEditor();
	CloseAnchorPickerEditor();

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

	const auto abs = this->GetAbsoluteLocationDip();
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
	SetCurrentSelectedIndex(index);
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
	const auto size = this->GetActualSizeDip();
	float width = size.width;
	float height = size.height;
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

bool PropertyGridView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		CloseColorPickerEditor();
		CloseAnchorPickerEditor();
		CloseDropDownEditor();
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
		if (_dragSlider && _sliderDragIndex >= 0)
		{
			auto rows = BuildRows();
			auto layout = CalcLayout(rows);
			D2D1_RECT_F valueRect{};
			if (GetValueRectForItem(
				_sliderDragIndex, rows, layout, valueRect))
				UpdateSliderFromPoint(
					_sliderDragIndex, static_cast<float>(localX), valueRect);
		}
		else if (_dragEditSelection && _editing)
		{
			auto rows = BuildRows();
			auto layout = CalcLayout(rows);
			D2D1_RECT_F valueRect{};
			if (GetValueRectForItem(_editingIndex, rows, layout, valueRect))
				UpdateEditingSelectionFromMousePoint(localX, localY, valueRect);
		}
		else if (_dragVScroll)
			UpdateScrollByThumb((float)localY);
		else if (_dragSplitter)
		{
			CloseDropDownEditor();
			CloseColorPickerEditor();
			CloseAnchorPickerEditor();
			auto rows = BuildRows();
			auto layout = CalcLayout(rows);
			SetCurrentNameColumnWidth(std::clamp((float)localX, 48.0f,
				std::max(48.0f, RectWidth(layout.ContentRect) - 48.0f)));
		}
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
		auto rows = BuildRows();
		auto layout = CalcLayout(rows);
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
		if (IsOverSplitter(localX, localY))
		{
			_dragSplitter = true;
			return true;
		}

		bool handledRow = false;
		for (const auto& row : rows)
		{
			auto rect = GetRowRect(row, layout);
			if (!PtInRectF(rect, (float)localX, (float)localY)) continue;
			handledRow = true;
			if (row.IsCategory)
			{
				CancelEdit();
				CloseDropDownEditor();
				CloseColorPickerEditor();
				CloseAnchorPickerEditor();
				ToggleCategory(row.Category);
			}
			else
			{
				int valueIndex = -1;
				bool inValue = IsValueCell(localX, localY, rows, layout, valueIndex);
				const bool canReset = this->Items[row.ItemIndex].CanReset;
				const auto itemType = this->Items[row.ItemIndex].ValueType;
				if (canReset && PtInRectF(
					GetResetRect(rect), (float)localX, (float)localY))
				{
					CancelEdit();
					CloseDropDownEditor();
					CloseColorPickerEditor();
					CloseAnchorPickerEditor();
					RequestReset(row.ItemIndex);
					break;
				}
				ActivateItem(row.ItemIndex);
				if (itemType == PropertyGridValueType::Action)
				{
					CancelEdit();
					CloseDropDownEditor();
					CloseColorPickerEditor();
					CloseAnchorPickerEditor();
					break;
				}
				if (itemType == PropertyGridValueType::Slider
					&& inValue && IsEditableItem(row.ItemIndex))
				{
					CancelEdit();
					CloseDropDownEditor();
					CloseColorPickerEditor();
					CloseAnchorPickerEditor();
					_sliderDragIndex = row.ItemIndex;
					_dragSlider = true;
					this->OnEditStarted(this, row.ItemIndex);
					UpdateSliderFromPoint(
						row.ItemIndex, static_cast<float>(localX), GetValueRect(rect));
					if (this->ParentForm && this->ParentForm->Handle)
						SetCapture(this->ParentForm->Handle);
					break;
				}
				if (inValue && IsEditableItem(row.ItemIndex))
				{
					auto type = this->Items[row.ItemIndex].ValueType;
					if (type == PropertyGridValueType::Bool)
					{
						CloseDropDownEditor();
						CloseColorPickerEditor();
						CloseAnchorPickerEditor();
						ToggleBool(row.ItemIndex);
					}
					else if (type == PropertyGridValueType::Enum)
						ToggleDropDownEditor(row.ItemIndex, GetValueRect(rect));
					else if (type == PropertyGridValueType::EditableEnum &&
						localX >= GetValueRect(rect).right - 24.0f)
						ToggleDropDownEditor(row.ItemIndex, GetValueRect(rect));
					else if (type == PropertyGridValueType::Color)
					{
						CloseDropDownEditor();
						OpenColorPickerEditor(row.ItemIndex, GetValueRect(rect));
					}
					else if (type == PropertyGridValueType::Anchor)
					{
						CloseDropDownEditor();
						OpenAnchorPickerEditor(row.ItemIndex, GetValueRect(rect));
					}
					else
					{
						CloseDropDownEditor();
						CloseColorPickerEditor();
						CloseAnchorPickerEditor();
						if (_editing && _editingIndex == row.ItemIndex)
						{
							SetEditingCaretFromMousePoint(localX, localY, GetValueRect(rect));
							_dragEditSelection = true;
							if (this->ParentForm && this->ParentForm->Handle)
								SetCapture(this->ParentForm->Handle);
						}
						else
						{
							BeginEdit(row.ItemIndex);
							if (_editing && _editingIndex == row.ItemIndex)
								SetEditingCaretFromMousePoint(localX, localY, GetValueRect(rect));
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
				if (!inValue && _anchorPickerIndex != row.ItemIndex)
					CloseAnchorPickerEditor();
			}
			break;
		}
		if (!handledRow && _editing)
			CommitEdit();
		if (!handledRow)
		{
			CloseDropDownEditor();
			CloseColorPickerEditor();
			CloseAnchorPickerEditor();
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		_dragVScroll = false;
		_dragSplitter = false;
		if (_dragSlider)
		{
			const int sliderIndex = _sliderDragIndex;
			auto rows = BuildRows();
			auto layout = CalcLayout(rows);
			D2D1_RECT_F valueRect{};
			if (GetValueRectForItem(sliderIndex, rows, layout, valueRect))
				UpdateSliderFromPoint(
					sliderIndex, static_cast<float>(localX), valueRect);
			_dragSlider = false;
			_sliderDragIndex = -1;
			ReleaseCapture();
			if (sliderIndex >= 0)
				this->OnEditCompleted(this, sliderIndex);
		}
		if (_dragEditSelection)
		{
			_dragEditSelection = false;
			ReleaseCapture();
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, e);
		return true;
	}
	case WM_LBUTTONDBLCLK:
	{
		auto rows = BuildRows();
		auto layout = CalcLayout(rows);
		int index = HitTestItem(localX, localY);
		if (index >= 0)
		{
			D2D1_RECT_F valueRect{};
			auto type = this->Items[index].ValueType;
			if (_editing && _editingIndex == index &&
				GetValueRectForItem(index, rows, layout, valueRect) &&
				IsEditableItem(index) &&
				type != PropertyGridValueType::Enum &&
				type != PropertyGridValueType::Color &&
				type != PropertyGridValueType::Anchor &&
				type != PropertyGridValueType::Bool)
			{
				_editSelectionStart = 0;
				_editSelectionEnd = (int)_editingText.size();
				_dragEditSelection = false;
				this->InvalidateVisual();
			}
			else
			if (GetValueRectForItem(index, rows, layout, valueRect) && IsEditableItem(index) &&
				(type == PropertyGridValueType::Enum
					|| type == PropertyGridValueType::Color
					|| type == PropertyGridValueType::Anchor))
			{
				if (type == PropertyGridValueType::Enum)
					ToggleDropDownEditor(index, valueRect);
				else if (type == PropertyGridValueType::Color)
					OpenColorPickerEditor(index, valueRect);
				else
					OpenAnchorPickerEditor(index, valueRect);
			}
			else
				BeginEdit(index);
		}
		MouseEventArgs e(MouseButtons::Left, 2, localX, localY, HIWORD(wParam));
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
				this->InvalidateVisual();
				break;
			case VK_END:
				_editSelectionEnd = (int)_editingText.size();
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					_editSelectionStart = _editSelectionEnd;
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
			if (type == PropertyGridValueType::Enum ||
				type == PropertyGridValueType::EditableEnum)
			{
				ToggleDropDownEditor(this->SelectedIndex, valueRect);
				return true;
			}
			if (type == PropertyGridValueType::Color)
			{
				OpenColorPickerEditor(this->SelectedIndex, valueRect);
				return true;
			}
			if (type == PropertyGridValueType::Anchor)
			{
				OpenAnchorPickerEditor(this->SelectedIndex, valueRect);
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
			if (this->SelectedIndex >= 0)
			{
				if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::Action)
					ActivateItem(this->SelectedIndex);
				else if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::EditableEnum)
					BeginEdit(this->SelectedIndex);
				else if (!openSelectedDropDown())
					BeginEdit(this->SelectedIndex);
			}
			break;
		case VK_F4:
			openSelectedDropDown();
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
				else if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::Anchor)
					openSelectedDropDown();
				else if (this->Items[this->SelectedIndex].ValueType == PropertyGridValueType::Action)
					ActivateItem(this->SelectedIndex);
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
		wchar_t ch = (wchar_t)wParam;
		if (!_editing && CuiTextEdit::IsTextInputChar(ch) && this->SelectedIndex >= 0 && this->SelectedIndex < (int)this->Items.size())
		{
			auto type = this->Items[this->SelectedIndex].ValueType;
			if (IsEditableItem(this->SelectedIndex) &&
				(type == PropertyGridValueType::Text ||
					type == PropertyGridValueType::Number ||
					type == PropertyGridValueType::EditableEnum))
			{
				BeginEdit(this->SelectedIndex);
			}
		}
		if (_editing && CuiTextEdit::IsTextInputChar(ch))
		{
			const wchar_t input[] = { ch, L'\0' };
			InputEditText(input);
		}
		else if (_editing && wParam == 1)
		{
			_editSelectionStart = 0;
			_editSelectionEnd = (int)_editingText.size();
			InvalidateVisual();
		}
		else if (_editing && wParam == 8)
		{
			BackspaceEdit();
		}
		else if (_editing && wParam == 22)
		{
			std::wstring clipboardText;
			if (TryReadClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, clipboardText))
				InputEditText(clipboardText);
		}
		else if (_editing && (wParam == 3 || wParam == 24))
		{
			std::wstring s = EditGetSelectedString();
			WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, s);
			if (wParam == 24)
				BackspaceEdit();
		}
		this->OnCharInput(this, (wchar_t)wParam);
		return true;
	}
	case WM_IME_COMPOSITION:
	{
		if (lParam & GCS_RESULTSTR)
		{
			// Unicode windows also deliver committed IME text via WM_CHAR.
			// Keep mutations on the same path as TextBox so selection replacement is stable.
			this->InvalidateVisual();
		}
		return true;
	}
	case WM_CANCELMODE:
	case WM_CAPTURECHANGED:
	{
		if (_dragSlider)
		{
			const int index = _sliderDragIndex;
			_dragSlider = false;
			_sliderDragIndex = -1;
			if (index >= 0) this->OnEditCanceled(this, index);
		}
		_dragVScroll = false;
		_dragSplitter = false;
		_dragEditSelection = false;
		return true;
	}
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}
