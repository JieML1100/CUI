#pragma once
#include "TabControl.h"
#include "Panel.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <limits>
#pragma comment(lib, "Imm32.lib")

UIClass TabPage::Type() { return UIClass::UI_TabPage; }
TabPage::TabPage()
{
	this->Text = L"Page";
	this->BorderThickness = 0.0f;
}
TabPage::TabPage(std::wstring text)
	: TabPage()
{
	this->Text = text;
}

void TabPage::SetHeaderImage(std::shared_ptr<BitmapSource> value)
{
	if (value == this->HeaderImage)
		return;
	this->HeaderImage = std::move(value);
	this->_headerImageCacheSource.reset();
	this->_headerImageCache.Reset();
	this->_headerImageCacheTarget = nullptr;
	this->InvalidateVisual();
}

ID2D1Bitmap* TabPage::EnsureHeaderImageCache()
{
	if (!this->HeaderImage || !this->ParentForm || !this->ParentForm->Render)
		return nullptr;
	auto* target = this->ParentForm->Render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (this->_headerImageCache && this->_headerImageCacheTarget == target && this->_headerImageCacheSource == this->HeaderImage)
		return this->_headerImageCache.Get();
	this->_headerImageCache.Reset();
	this->_headerImageCacheTarget = target;
	this->_headerImageCacheSource = this->HeaderImage;
	auto* bmp = this->ParentForm->Render->CreateBitmap(this->HeaderImage);
	if (!bmp)
		return nullptr;
	this->_headerImageCache.Attach(bmp);
	return this->_headerImageCache.Get();
}

void TabPage::Update()
{
	const bool transparentInDCompTab =
		this->Parent &&
		this->Parent->Type() == UIClass::UI_TabControl &&
		this->ParentForm &&
		this->ParentForm->IsDCompSceneRenderActive();
	if (!transparentInDCompTab)
	{
		Panel::Update();
		return;
	}

	auto oldBackColor = this->BackColor;
	this->BackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	Panel::Update();
	this->BackColor = oldBackColor;
}

UIClass TabControl::Type() { return UIClass::UI_TabControl; }

static float GetMaxTitleScrollOffset(TabControl* tabs);

namespace
{
	template<typename TValue>
	ControlPropertyOptions<TabControl, TValue> TabControlPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<TabControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto TabControlPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			TabControl& target,
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

	ControlPropertyOptions<TabControl, float> TabControlMetricOptions(
		float defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		auto options = TabControlPropertyOptions(
			defaultValue, category, categoryOrder, order,
			ControlPropertyEditorKind::Number, flags);
		options.Coerce = [](
			TabControl&, const float& proposed) -> std::optional<float>
		{
			if (!std::isfinite(proposed)) return std::nullopt;
			return (std::max)(0.0f, proposed);
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	bool TabControlColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<TabControl, D2D1_COLOR_F> TabControlColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = TabControlPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = TabControlColorsEqual;
		return options;
	}
}

void TabControl::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto selectedIndexOptions = TabControlPropertyOptions(
			0, L"Behavior", 110, 10,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsArrange
				| ControlPropertyFlags::AffectsRender);
		selectedIndexOptions.Coerce = [](
			TabControl& target, const int& proposed) -> std::optional<int>
		{
			const int nonNegative = (std::max)(0, proposed);
			return target.Count > 0
				? (std::min)(nonNegative, target.Count - 1)
				: nonNegative;
		};
		selectedIndexOptions.Changed = [](
			TabControl& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectedIndexChange(oldValue, newValue);
		};
		selectedIndexOptions.Design.Minimum = 0.0;
		selectedIndexOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<TabControl, int>(L"SelectedIndex",
			[](TabControl& target) { return target.SelectedIndex; },
			[](TabControl& target, const int& value) { target.SelectedIndex = value; },
			TabControlPropertySubscriber(L"SelectedIndex"),
			std::move(selectedIndexOptions));

		auto animationModeOptions = TabControlPropertyOptions(
			static_cast<int>(TabControlAnimationMode::DirectReplace),
			L"Behavior", 110, 20,
			ControlPropertyEditorKind::Choice);
		animationModeOptions.Coerce = [](
			TabControl&, const int& proposed) -> std::optional<int>
		{
			if (proposed != static_cast<int>(TabControlAnimationMode::DirectReplace)
				&& proposed != static_cast<int>(TabControlAnimationMode::SlideHorizontal))
				return std::nullopt;
			return proposed;
		};
		animationModeOptions.Changed = [](
			TabControl& target, const int&, const int& newValue)
		{
			if (newValue == static_cast<int>(TabControlAnimationMode::DirectReplace)
				&& target.IsAnimationRunning())
				target.FinishTransition();
		};
		animationModeOptions.Design.Choices = {
			{ L"DirectReplace", BindingValue(static_cast<int>(TabControlAnimationMode::DirectReplace)) },
			{ L"SlideHorizontal", BindingValue(static_cast<int>(TabControlAnimationMode::SlideHorizontal)) }
		};
		BindingPropertyRegistry::Register<TabControl, int>(L"AnimationMode",
			[](TabControl& target) { return static_cast<int>(target.AnimationMode); },
			[](TabControl& target, const int& value)
			{ target.AnimationMode = static_cast<TabControlAnimationMode>(value); },
			TabControlPropertySubscriber(L"AnimationMode"), std::move(animationModeOptions));

		auto animationDurationOptions = TabControlPropertyOptions(
			180, L"Behavior", 110, 30,
			ControlPropertyEditorKind::Number);
		animationDurationOptions.Coerce = [](
			TabControl&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		animationDurationOptions.Design.Minimum = 0.0;
		animationDurationOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<TabControl, int>(L"AnimationDurationMs",
			[](TabControl& target) { return static_cast<int>(target.AnimationDurationMs); },
			[](TabControl& target, const int& value)
			{ target.AnimationDurationMs = static_cast<UINT>(value); },
			TabControlPropertySubscriber(L"AnimationDurationMs"),
			std::move(animationDurationOptions));

		auto enableTitleScrollOptions = TabControlPropertyOptions(
			true, L"Behavior", 110, 40,
			ControlPropertyEditorKind::Boolean);
		enableTitleScrollOptions.Changed = [](
			TabControl& target, const bool&, const bool& enabled)
		{
			if (!enabled) target.SetCurrentTitleScrollOffset(0.0f);
			else target.ClampTitleScrollOffset();
		};
		BindingPropertyRegistry::Register<TabControl, bool>(L"EnableTitleScroll",
			[](TabControl& target) { return target.EnableTitleScroll; },
			[](TabControl& target, const bool& value) { target.EnableTitleScroll = value; },
			TabControlPropertySubscriber(L"EnableTitleScroll"),
			std::move(enableTitleScrollOptions));

		BindingPropertyRegistry::Register<TabControl, float>(L"TitleScrollMouseWheelStep",
			[](TabControl& target) { return target.TitleScrollMouseWheelStep; },
			[](TabControl& target, const float& value) { target.TitleScrollMouseWheelStep = value; },
			TabControlPropertySubscriber(L"TitleScrollMouseWheelStep"),
			TabControlMetricOptions(64.0f, L"Behavior", 110, 50));

		auto titlePositionOptions = TabControlPropertyOptions(
			static_cast<int>(TabControlTitlePosition::Top),
			L"Layout", 100, 10,
			ControlPropertyEditorKind::Choice,
			ControlPropertyFlags::AffectsArrange
				| ControlPropertyFlags::AffectsRender);
		titlePositionOptions.Coerce = [](
			TabControl&, const int& proposed) -> std::optional<int>
		{
			switch (static_cast<TabControlTitlePosition>(proposed))
			{
			case TabControlTitlePosition::Top:
			case TabControlTitlePosition::Bottom:
			case TabControlTitlePosition::Left:
			case TabControlTitlePosition::Right:
				return proposed;
			default:
				return std::nullopt;
			}
		};
		titlePositionOptions.Changed = [](
			TabControl& target, const int&, const int&)
		{
			target.ClampTitleScrollOffset();
			target._lastTitleEnsurePosition = target.TitlePosition;
			target._lastTitleEnsureIndex = -1;
		};
		titlePositionOptions.Design.Choices = {
			{ L"Top", BindingValue(static_cast<int>(TabControlTitlePosition::Top)) },
			{ L"Bottom", BindingValue(static_cast<int>(TabControlTitlePosition::Bottom)) },
			{ L"Left", BindingValue(static_cast<int>(TabControlTitlePosition::Left)) },
			{ L"Right", BindingValue(static_cast<int>(TabControlTitlePosition::Right)) }
		};
		BindingPropertyRegistry::Register<TabControl, int>(L"TitlePosition",
			[](TabControl& target) { return static_cast<int>(target.TitlePosition); },
			[](TabControl& target, const int& value)
			{ target.TitlePosition = static_cast<TabControlTitlePosition>(value); },
			TabControlPropertySubscriber(L"TitlePosition"), std::move(titlePositionOptions));

		const auto titleGeometryFlags = ControlPropertyFlags::AffectsArrange
			| ControlPropertyFlags::AffectsRender;
#define CUI_REGISTER_TAB_METRIC(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<TabControl, float>(propertyName, \
			[](TabControl& target) { return target.name; }, \
			[](TabControl& target, const float& value) { target.name = value; }, \
			TabControlPropertySubscriber(propertyName), \
			TabControlMetricOptions(defaultValue, L"Layout", 100, order, titleGeometryFlags))

		CUI_REGISTER_TAB_METRIC(TitleHeight, L"TitleHeight", 24.0f, 20);
		CUI_REGISTER_TAB_METRIC(TitleWidth, L"TitleWidth", 120.0f, 30);
		CUI_REGISTER_TAB_METRIC(TitleGap, L"TitleGap", 3.0f, 40);
		CUI_REGISTER_TAB_METRIC(TitleInset, L"TitleInset", 2.0f, 50);
		CUI_REGISTER_TAB_METRIC(TitleCornerRadius, L"TitleCornerRadius", 7.0f, 60);
		CUI_REGISTER_TAB_METRIC(SelectedAccentSize, L"SelectedAccentSize", 3.0f, 70);
		CUI_REGISTER_TAB_METRIC(TitleScrollButtonSize, L"TitleScrollButtonSize", 24.0f, 80);
		CUI_REGISTER_TAB_METRIC(BorderThickness, L"BorderThickness", 1.5f, 90);

#undef CUI_REGISTER_TAB_METRIC

		auto titleScrollOffsetOptions = TabControlMetricOptions(
			0.0f, L"Runtime", 1000, 10);
		titleScrollOffsetOptions.Design.Browsable = false;
		titleScrollOffsetOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		titleScrollOffsetOptions.Coerce = [](
			TabControl& target, const float& proposed) -> std::optional<float>
		{
			if (!std::isfinite(proposed)) return std::nullopt;
			return (std::clamp)(
				proposed, 0.0f, GetMaxTitleScrollOffset(&target));
		};
		titleScrollOffsetOptions.Changed = [](
			TabControl& target, const float&, const float&)
		{
			target.OnScrollChanged(&target);
		};
		BindingPropertyRegistry::Register<TabControl, float>(L"TitleScrollOffset",
			[](TabControl& target) { return target.TitleScrollOffset; },
			[](TabControl& target, const float& value) { target.TitleScrollOffset = value; },
			TabControlPropertySubscriber(L"TitleScrollOffset"),
			std::move(titleScrollOffsetOptions));

#define CUI_REGISTER_TAB_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<TabControl, D2D1_COLOR_F>(propertyName, \
			[](TabControl& target) { return target.name; }, \
			[](TabControl& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			TabControlPropertySubscriber(propertyName), TabControlColorOptions(defaultValue, order))

		CUI_REGISTER_TAB_COLOR(TitleBackColor, L"TitleBackColor", (D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f }), 10);
		CUI_REGISTER_TAB_COLOR(SelectedTitleBackColor, L"SelectedTitleBackColor", (D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.14f }), 20);
		CUI_REGISTER_TAB_COLOR(TitleHoverBackColor, L"TitleHoverBackColor", (D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.08f }), 30);
		CUI_REGISTER_TAB_COLOR(AccentColor, L"AccentColor", (D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 1.0f }), 40);
		CUI_REGISTER_TAB_COLOR(TitleMutedForeColor, L"TitleMutedForeColor", Colors::DimGrey, 50);
		CUI_REGISTER_TAB_COLOR(TitleScrollTrackColor, L"TitleScrollTrackColor", (D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.12f }), 60);
		CUI_REGISTER_TAB_COLOR(TitleScrollThumbColor, L"TitleScrollThumbColor", (D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.58f }), 70);
		CUI_REGISTER_TAB_COLOR(TitleScrollButtonBackColor, L"TitleScrollButtonBackColor", (D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.76f }), 80);
		CUI_REGISTER_TAB_COLOR(TitleScrollButtonHoverBackColor, L"TitleScrollButtonHoverBackColor", (D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.18f }), 90);

#undef CUI_REGISTER_TAB_COLOR
		return true;
	}();
	(void)registered;
}

#define CUI_TAB_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(TabControl, type, name) { return field; } \
	SET_CPP(TabControl, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleBackColor, _titleBackColor, L"TitleBackColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, SelectedTitleBackColor, _selectedTitleBackColor, L"SelectedTitleBackColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleHoverBackColor, _titleHoverBackColor, L"TitleHoverBackColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleMutedForeColor, _titleMutedForeColor, L"TitleMutedForeColor")
CUI_TAB_PROPERTY_IMPL(float, TitleCornerRadius, _titleCornerRadius, L"TitleCornerRadius")
CUI_TAB_PROPERTY_IMPL(float, TitleGap, _titleGap, L"TitleGap")
CUI_TAB_PROPERTY_IMPL(float, TitleInset, _titleInset, L"TitleInset")
CUI_TAB_PROPERTY_IMPL(float, SelectedAccentSize, _selectedAccentSize, L"SelectedAccentSize")
CUI_TAB_PROPERTY_IMPL(bool, EnableTitleScroll, _enableTitleScroll, L"EnableTitleScroll")
CUI_TAB_PROPERTY_IMPL(float, TitleScrollOffset, _titleScrollOffset, L"TitleScrollOffset")
CUI_TAB_PROPERTY_IMPL(float, TitleScrollMouseWheelStep, _titleScrollMouseWheelStep, L"TitleScrollMouseWheelStep")
CUI_TAB_PROPERTY_IMPL(float, TitleScrollButtonSize, _titleScrollButtonSize, L"TitleScrollButtonSize")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleScrollTrackColor, _titleScrollTrackColor, L"TitleScrollTrackColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleScrollThumbColor, _titleScrollThumbColor, L"TitleScrollThumbColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleScrollButtonBackColor, _titleScrollButtonBackColor, L"TitleScrollButtonBackColor")
CUI_TAB_PROPERTY_IMPL(D2D1_COLOR_F, TitleScrollButtonHoverBackColor, _titleScrollButtonHoverBackColor, L"TitleScrollButtonHoverBackColor")
CUI_TAB_PROPERTY_IMPL(float, TitleHeight, _titleHeight, L"TitleHeight")
CUI_TAB_PROPERTY_IMPL(float, TitleWidth, _titleWidth, L"TitleWidth")
CUI_TAB_PROPERTY_IMPL(float, BorderThickness, _borderThickness, L"BorderThickness")

#undef CUI_TAB_PROPERTY_IMPL

GET_CPP(TabControl, TabControlAnimationMode, AnimationMode)
{
	return static_cast<TabControlAnimationMode>(_animationMode);
}

SET_CPP(TabControl, TabControlAnimationMode, AnimationMode)
{
	(void)SetPropertyField(
		L"AnimationMode", _animationMode, static_cast<int>(value));
}

GET_CPP(TabControl, TabControlTitlePosition, TitlePosition)
{
	return static_cast<TabControlTitlePosition>(_titlePosition);
}

SET_CPP(TabControl, TabControlTitlePosition, TitlePosition)
{
	(void)SetPropertyField(
		L"TitlePosition", _titlePosition, static_cast<int>(value));
}

GET_CPP(TabControl, int, SelectedIndex)
{
	return _selectedIndex;
}

SET_CPP(TabControl, int, SelectedIndex)
{
	if (_animating && value != _selectedIndex)
		FinishTransition();
	(void)SetPropertyField(L"SelectedIndex", _selectedIndex, value);
}

GET_CPP(TabControl, UINT, AnimationDurationMs)
{
	return static_cast<UINT>(_animationDurationMs);
}

SET_CPP(TabControl, UINT, AnimationDurationMs)
{
	const auto maximum = static_cast<UINT>((std::numeric_limits<int>::max)());
	const int proposed = static_cast<int>((std::min)(value, maximum));
	(void)SetPropertyField(L"AnimationDurationMs", _animationDurationMs, proposed);
}

TabControl::TabControl(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}

static float EaseOutCubic01(float t)
{
	t = (std::clamp)(t, 0.0f, 1.0f);
	return 1.0f - std::pow(1.0f - t, 3.0f);
}

static void SyncNativeChildWindowsRecursive(Control* root)
{
	if (!root) return;
	if (root->Type() == UIClass::UI_WebBrowser)
	{
		root->SyncNativeSurface();
	}
	for (auto* child : root->Children)
	{
		SyncNativeChildWindowsRecursive(child);
	}
}

static void SyncNativeChildWindowsForAllPages(TabControl* tabControl)
{
	if (!tabControl) return;
	for (int i = 0; i < tabControl->Count; i++)
	{
		auto page = tabControl->operator[](i);
		SyncNativeChildWindowsRecursive(page);
	}
}

static D2D1_RECT_F InsetRect(const D2D1_RECT_F& rect, float insetX, float insetY)
{
	return D2D1::RectF(rect.left + insetX, rect.top + insetY, rect.right - insetX, rect.bottom - insetY);
}

static D2D1_RECT_F FitBitmapRect(ID2D1Bitmap* bmp, const D2D1_RECT_F& box)
{
	if (!bmp)
		return box;
	const auto size = bmp->GetSize();
	const float boxW = (std::max)(0.0f, box.right - box.left);
	const float boxH = (std::max)(0.0f, box.bottom - box.top);
	if (size.width <= 0.0f || size.height <= 0.0f || boxW <= 0.0f || boxH <= 0.0f)
		return box;
	const float scale = (std::min)(boxW / size.width, boxH / size.height);
	const float w = size.width * scale;
	const float h = size.height * scale;
	const float x = box.left + (boxW - w) * 0.5f;
	const float y = box.top + (boxH - h) * 0.5f;
	return D2D1::RectF(x, y, x + w, y + h);
}

static float RectWidth(const D2D1_RECT_F& rect)
{
	return (std::max)(0.0f, rect.right - rect.left);
}

static float RectHeight(const D2D1_RECT_F& rect)
{
	return (std::max)(0.0f, rect.bottom - rect.top);
}

static bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
{
	return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

static bool RectIntersects(const D2D1_RECT_F& a, const D2D1_RECT_F& b)
{
	return a.right > b.left && a.left < b.right && a.bottom > b.top && a.top < b.bottom;
}

static D2D1_COLOR_F ScaleAlpha(D2D1_COLOR_F color, float scale)
{
	color.a = (std::clamp)(color.a * scale, 0.0f, 1.0f);
	return color;
}

static bool IsSideTitlePosition(TabControlTitlePosition position)
{
	return position == TabControlTitlePosition::Left || position == TabControlTitlePosition::Right;
}

static bool IsSideTitle(TabControl* tabs)
{
	return tabs && IsSideTitlePosition(tabs->TitlePosition);
}

static float GetTitleItemLength(TabControl* tabs)
{
	if (!tabs) return 0.0f;
	return IsSideTitle(tabs)
		? (std::max)(0.0f, tabs->TitleHeight)
		: (std::max)(0.0f, tabs->TitleWidth);
}

static float GetTitleViewportLength(TabControl* tabs)
{
	if (!tabs) return 0.0f;
	auto viewport = tabs->GetTitleViewportRect();
	return IsSideTitle(tabs) ? RectHeight(viewport) : RectWidth(viewport);
}

static float GetTitleContentLength(TabControl* tabs)
{
	if (!tabs || tabs->Count <= 0) return 0.0f;
	return GetTitleItemLength(tabs) * static_cast<float>(tabs->Count);
}

static float GetMaxTitleScrollOffset(TabControl* tabs)
{
	if (!tabs || !tabs->EnableTitleScroll) return 0.0f;
	const float overflow = GetTitleContentLength(tabs) - GetTitleViewportLength(tabs);
	return (std::max)(0.0f, overflow);
}

static int GetTitleAxisPos(TabControl* tabs, int localX, int localY)
{
	return IsSideTitle(tabs) ? localY : localX;
}

static bool GetTitleScrollButtonRects(TabControl* tabs, D2D1_RECT_F& backward, D2D1_RECT_F& forward)
{
	if (!tabs || !tabs->IsTitleOverflowing())
		return false;
	auto viewport = tabs->GetTitleViewportRect();
	const float viewportW = RectWidth(viewport);
	const float viewportH = RectHeight(viewport);
	if (viewportW <= 8.0f || viewportH <= 8.0f)
		return false;

	const bool sideTitles = IsSideTitle(tabs);
	const float cross = sideTitles ? viewportW : viewportH;
	const float maxButton = (std::max)(12.0f, cross - 4.0f);
	const float buttonSize = (std::clamp)(tabs->TitleScrollButtonSize, 12.0f, maxButton);
	const float pad = 2.0f;

	if (sideTitles)
	{
		const float x = viewport.left + (viewportW - buttonSize) * 0.5f;
		backward = D2D1::RectF(x, viewport.top + pad, x + buttonSize, viewport.top + pad + buttonSize);
		forward = D2D1::RectF(x, viewport.bottom - pad - buttonSize, x + buttonSize, viewport.bottom - pad);
	}
	else
	{
		const float y = viewport.top + (viewportH - buttonSize) * 0.5f;
		backward = D2D1::RectF(viewport.left + pad, y, viewport.left + pad + buttonSize, y + buttonSize);
		forward = D2D1::RectF(viewport.right - pad - buttonSize, y, viewport.right - pad, y + buttonSize);
	}
	return true;
}

static bool CanScrollTitleInDirection(TabControl* tabs, int direction)
{
	if (!tabs || direction == 0 || !tabs->IsTitleOverflowing())
		return false;
	const float maxOffset = GetMaxTitleScrollOffset(tabs);
	const float offset = (std::clamp)(tabs->TitleScrollOffset, 0.0f, maxOffset);
	return direction < 0 ? offset > 0.0f : offset < maxOffset;
}

static void DrawTitleScrollChevron(D2DGraphics* d2d, const D2D1_RECT_F& rect, int direction, bool vertical, D2D1_COLOR_F color)
{
	if (!d2d) return;
	const float w = RectWidth(rect);
	const float h = RectHeight(rect);
	const float cx = rect.left + w * 0.5f;
	const float cy = rect.top + h * 0.5f;
	const float half = (std::max)(3.5f, (std::min)(w, h) * 0.22f);
	const float stroke = 1.8f;
	D2D1_POINT_2F p1{};
	D2D1_POINT_2F p2{};
	D2D1_POINT_2F p3{};

	if (vertical)
	{
		if (direction < 0)
		{
			p1 = D2D1::Point2F(cx - half, cy + half * 0.45f);
			p2 = D2D1::Point2F(cx, cy - half * 0.55f);
			p3 = D2D1::Point2F(cx + half, cy + half * 0.45f);
		}
		else
		{
			p1 = D2D1::Point2F(cx - half, cy - half * 0.45f);
			p2 = D2D1::Point2F(cx, cy + half * 0.55f);
			p3 = D2D1::Point2F(cx + half, cy - half * 0.45f);
		}
	}
	else
	{
		if (direction < 0)
		{
			p1 = D2D1::Point2F(cx + half * 0.45f, cy - half);
			p2 = D2D1::Point2F(cx - half * 0.55f, cy);
			p3 = D2D1::Point2F(cx + half * 0.45f, cy + half);
		}
		else
		{
			p1 = D2D1::Point2F(cx - half * 0.45f, cy - half);
			p2 = D2D1::Point2F(cx + half * 0.55f, cy);
			p3 = D2D1::Point2F(cx - half * 0.45f, cy + half);
		}
	}

	d2d->DrawLine(p1, p2, color, stroke);
	d2d->DrawLine(p2, p3, color, stroke);
}

static void DrawTitleScrollButton(TabControl* tabs, D2DGraphics* d2d, const D2D1_RECT_F& rect, int direction)
{
	if (!tabs || !d2d || RectWidth(rect) <= 0.0f || RectHeight(rect) <= 0.0f)
		return;
	if (!CanScrollTitleInDirection(tabs, direction))
		return;
	const bool hovered = tabs->_hoverTitleScrollButton == direction;
	const bool pressed = tabs->_pressedTitleScrollButton == direction;
	const float radius = (std::min)(RectWidth(rect), RectHeight(rect)) * 0.5f;
	D2D1_COLOR_F back = hovered || pressed ? tabs->TitleScrollButtonHoverBackColor : tabs->TitleScrollButtonBackColor;
	if (pressed)
		back = ScaleAlpha(tabs->AccentColor, 0.24f);
	d2d->FillRoundRect(rect, back, radius);
	d2d->DrawRoundRect(rect, ScaleAlpha(tabs->AccentColor, 0.28f), 1.0f, radius);
	DrawTitleScrollChevron(d2d, rect, direction, IsSideTitle(tabs), tabs->AccentColor);
}

static void DrawTitleScrollChrome(TabControl* tabs, D2DGraphics* d2d)
{
	if (!tabs || !d2d || !tabs->IsTitleOverflowing())
		return;

	auto viewport = tabs->GetTitleViewportRect();
	const float viewportW = RectWidth(viewport);
	const float viewportH = RectHeight(viewport);
	const float viewportLen = GetTitleViewportLength(tabs);
	const float contentLen = GetTitleContentLength(tabs);
	const float maxOffset = GetMaxTitleScrollOffset(tabs);
	if (viewportW <= 0.0f || viewportH <= 0.0f || viewportLen <= 0.0f || contentLen <= viewportLen || maxOffset <= 0.0f)
		return;

	const bool sideTitles = IsSideTitle(tabs);
	const float per = (std::clamp)(tabs->TitleScrollOffset / maxOffset, 0.0f, 1.0f);
	const bool canBack = tabs->TitleScrollOffset > 0.0f;
	const bool canForward = tabs->TitleScrollOffset < maxOffset;
	const float cue = (std::min)(18.0f, (sideTitles ? viewportH : viewportW) * 0.16f);

	if (canBack)
	{
		if (sideTitles)
			d2d->FillRoundRect(viewport.left + 2.0f, viewport.top, (std::max)(0.0f, viewportW - 4.0f), cue,
				ScaleAlpha(tabs->AccentColor, 0.055f), 6.0f);
		else
			d2d->FillRoundRect(viewport.left, viewport.top + 2.0f, cue, (std::max)(0.0f, viewportH - 4.0f),
				ScaleAlpha(tabs->AccentColor, 0.055f), 6.0f);
	}
	if (canForward)
	{
		if (sideTitles)
			d2d->FillRoundRect(viewport.left + 2.0f, viewport.bottom - cue, (std::max)(0.0f, viewportW - 4.0f), cue,
				ScaleAlpha(tabs->AccentColor, 0.055f), 6.0f);
		else
			d2d->FillRoundRect(viewport.right - cue, viewport.top + 2.0f, cue, (std::max)(0.0f, viewportH - 4.0f),
				ScaleAlpha(tabs->AccentColor, 0.055f), 6.0f);
	}

	const float trackInset = 34.0f;
	if (sideTitles)
	{
		float trackX = tabs->TitlePosition == TabControlTitlePosition::Left ? viewport.right - 4.0f : viewport.left + 2.0f;
		D2D1_RECT_F track = D2D1::RectF(trackX, viewport.top + trackInset, trackX + 2.0f, viewport.bottom - trackInset);
		if (RectHeight(track) > 24.0f)
		{
			const float thumbH = (std::clamp)(RectHeight(track) * viewportLen / contentLen, 20.0f, RectHeight(track));
			const float travel = (std::max)(0.0f, RectHeight(track) - thumbH);
			const float thumbTop = track.top + per * travel;
			d2d->FillRoundRect(track, tabs->TitleScrollTrackColor, 1.0f);
			d2d->FillRoundRect(track.left, thumbTop, RectWidth(track), thumbH, tabs->TitleScrollThumbColor, 1.0f);
		}
	}
	else
	{
		float trackY = tabs->TitlePosition == TabControlTitlePosition::Bottom ? viewport.top + 2.0f : viewport.bottom - 4.0f;
		D2D1_RECT_F track = D2D1::RectF(viewport.left + trackInset, trackY, viewport.right - trackInset, trackY + 2.0f);
		if (RectWidth(track) > 24.0f)
		{
			const float thumbW = (std::clamp)(RectWidth(track) * viewportLen / contentLen, 24.0f, RectWidth(track));
			const float travel = (std::max)(0.0f, RectWidth(track) - thumbW);
			const float thumbLeft = track.left + per * travel;
			d2d->FillRoundRect(track, tabs->TitleScrollTrackColor, 1.0f);
			d2d->FillRoundRect(thumbLeft, track.top, thumbW, RectHeight(track), tabs->TitleScrollThumbColor, 1.0f);
		}
	}

	D2D1_RECT_F backward{}, forward{};
	if (GetTitleScrollButtonRects(tabs, backward, forward))
	{
		if (canBack)
			DrawTitleScrollButton(tabs, d2d, backward, -1);
		if (canForward)
			DrawTitleScrollButton(tabs, d2d, forward, 1);
	}
}

static void DrawTabAccent(TabControl* tabs, D2DGraphics* d2d, const D2D1_RECT_F& rect)
{
	if (!tabs || !d2d) return;
	const float accent = (std::max)(2.0f, tabs->SelectedAccentSize);
	const float width = (std::max)(0.0f, rect.right - rect.left);
	const float height = (std::max)(0.0f, rect.bottom - rect.top);
	const float padY = (std::min)(9.0f, (std::max)(4.0f, height * 0.25f));
	const float accentH = (std::max)(5.0f, height - padY * 2.0f);
	switch (tabs->TitlePosition)
	{
	case TabControlTitlePosition::Bottom:
	case TabControlTitlePosition::Top:
		d2d->FillRoundRect(rect.left + 7.0f, rect.top + padY,
			accent, accentH, tabs->AccentColor, accent * 0.5f);
		break;
	case TabControlTitlePosition::Left:
		d2d->FillRoundRect((std::max)(rect.left + 2.0f, rect.right - accent - 5.0f), rect.top + padY,
			accent, accentH,
			tabs->AccentColor, accent * 0.5f);
		break;
	case TabControlTitlePosition::Right:
		d2d->FillRoundRect(rect.left + 5.0f, rect.top + padY,
			accent, accentH,
			tabs->AccentColor, accent * 0.5f);
		break;
	default:
		break;
	}
}

static void UpdateTabPageContent(TabControl* tabs, TabPage* page)
{
	if (!tabs || !page) return;
	auto oldBackColor = page->BackColor;
	page->BackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	page->Update();
	page->BackColor = oldBackColor;
}

static D2D1_COLOR_F GetTabContentBackColor(TabControl* tabs)
{
	if (!tabs || tabs->Count <= 0)
		return D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	int index = tabs->_displayIndex;
	if (tabs->_animating && tabs->_animToIndex >= 0 && tabs->_animToIndex < tabs->Count)
		index = tabs->_animToIndex;
	if (index < 0 || index >= tabs->Count)
		index = (std::clamp)(tabs->SelectedIndex, 0, tabs->Count - 1);
	auto* page = (TabPage*)tabs->operator[](index);
	return page ? page->BackColor : D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
}

D2D1_RECT_F TabControl::GetContentRect()
{
	const auto size = this->GetActualSizeDip().NonNegative();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float titleWidth = (std::max)(0.0f, this->TitleWidth);
	const float titleHeight = (std::max)(0.0f, this->TitleHeight);
	D2D1_RECT_F rect{ 0.0f, 0.0f, actualWidth, actualHeight };

	switch (this->TitlePosition)
	{
	case TabControlTitlePosition::Bottom:
		rect.bottom = (std::max)(0.0f, actualHeight - titleHeight);
		break;
	case TabControlTitlePosition::Left:
		rect.left = (std::min)(actualWidth, titleWidth);
		break;
	case TabControlTitlePosition::Right:
		rect.right = (std::max)(0.0f, actualWidth - titleWidth);
		break;
	case TabControlTitlePosition::Top:
	default:
		rect.top = (std::min)(actualHeight, titleHeight);
		break;
	}

	return rect;
}

D2D1_RECT_F TabControl::GetTitleViewportRect()
{
	const auto size = this->GetActualSizeDip().NonNegative();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float titleWidth = (std::max)(0.0f, this->TitleWidth);
	const float titleHeight = (std::max)(0.0f, this->TitleHeight);

	switch (this->TitlePosition)
	{
	case TabControlTitlePosition::Bottom:
		return D2D1::RectF(0.0f, (std::max)(0.0f, actualHeight - titleHeight), actualWidth, actualHeight);
	case TabControlTitlePosition::Left:
		return D2D1::RectF(0.0f, 0.0f, (std::min)(actualWidth, titleWidth), actualHeight);
	case TabControlTitlePosition::Right:
		return D2D1::RectF((std::max)(0.0f, actualWidth - titleWidth), 0.0f, actualWidth, actualHeight);
	case TabControlTitlePosition::Top:
	default:
		return D2D1::RectF(0.0f, 0.0f, actualWidth, (std::min)(actualHeight, titleHeight));
	}
}

bool TabControl::IsTitleOverflowing()
{
	if (!this->EnableTitleScroll || this->Count <= 0)
		return false;
	return GetMaxTitleScrollOffset(this) > 0.0f;
}

void TabControl::ClampTitleScrollOffset()
{
	const float maxOffset = GetMaxTitleScrollOffset(this);
	SetCurrentTitleScrollOffset(
		(std::clamp)(this->TitleScrollOffset, 0.0f, maxOffset));
}

void TabControl::SetCurrentTitleScrollOffset(float value)
{
	value = (std::clamp)(value, 0.0f, GetMaxTitleScrollOffset(this));
	if (_titleScrollOffset == value) return;
	(void)SetCurrentPropertyField(
		L"TitleScrollOffset", _titleScrollOffset, value);
}

void TabControl::ScrollTitleBy(float delta)
{
	if (delta == 0.0f) return;
	SetCurrentTitleScrollOffset(this->TitleScrollOffset + delta);
}

void TabControl::EnsureTitleVisible(int index)
{
	if (index < 0 || index >= this->Count)
	{
		ClampTitleScrollOffset();
		return;
	}

	const float itemLen = GetTitleItemLength(this);
	const float viewportLen = GetTitleViewportLength(this);
	if (itemLen <= 0.0f || viewportLen <= 0.0f)
	{
		ClampTitleScrollOffset();
		return;
	}

	const float itemStart = itemLen * (float)index;
	const float itemEnd = itemStart + itemLen;
	const float currentStart = this->TitleScrollOffset;
	const float currentEnd = currentStart + viewportLen;
	const float pad = (std::min)(24.0f, viewportLen * 0.16f);
	float target = currentStart;
	if (itemStart < currentStart + pad)
		target = itemStart - pad;
	else if (itemEnd > currentEnd - pad)
		target = itemEnd - viewportLen + pad;

	SetCurrentTitleScrollOffset(target);
	ClampTitleScrollOffset();
}

int TabControl::HitTestTitleScrollButton(int localX, int localY)
{
	D2D1_RECT_F backward{}, forward{};
	if (!GetTitleScrollButtonRects(this, backward, forward))
		return 0;
	if (CanScrollTitleInDirection(this, -1) && PtInRectF(backward, (float)localX, (float)localY))
		return -1;
	if (CanScrollTitleInDirection(this, 1) && PtInRectF(forward, (float)localX, (float)localY))
		return 1;
	return 0;
}

D2D1_RECT_F TabControl::GetChildrenClipRect()
{
	return const_cast<TabControl*>(this)->GetContentRect();
}

std::vector<Control*> TabControl::GetVisibleScenePages()
{
	std::vector<Control*> pages;
	EnsureSelectionState();
	if (_animating && this->AnimationMode == TabControlAnimationMode::SlideHorizontal &&
		_animFromIndex >= 0 && _animFromIndex < this->Count && _animToIndex >= 0 && _animToIndex < this->Count)
	{
		const auto contentRect = GetContentRect();
		const float contentWidth = (std::max)(0.0f, contentRect.right - contentRect.left);
		const float contentHeight = (std::max)(0.0f, contentRect.bottom - contentRect.top);
		const float progress = CurrentTransitionProgress();
		const int direction = (_animToIndex >= _animFromIndex) ? 1 : -1;
		const float animationWidth = contentWidth;
		const float animationHeight = contentHeight;
		const bool verticalAnimation = this->TitlePosition == TabControlTitlePosition::Left || this->TitlePosition == TabControlTitlePosition::Right;
		auto* fromPage = (TabPage*)this->operator[](_animFromIndex);
		auto* toPage = (TabPage*)this->operator[](_animToIndex);
		if (verticalAnimation)
		{
			LayoutPage(fromPage, 0.0f, -(float)direction * progress * animationHeight);
			LayoutPage(toPage, 0.0f, (float)direction * (1.0f - progress) * animationHeight);
		}
		else
		{
			LayoutPage(fromPage, -(float)direction * progress * animationWidth);
			LayoutPage(toPage, (float)direction * (1.0f - progress) * animationWidth);
		}
		if (fromPage) pages.push_back(fromPage);
		if (toPage && toPage != fromPage) pages.push_back(toPage);
		return pages;
	}

	if (_displayIndex >= 0 && _displayIndex < this->Count)
	{
		auto* page = (TabPage*)this->operator[](_displayIndex);
		LayoutPage(page, 0);
		if (page) pages.push_back(page);
		if (this->_lastSelectIndex != _displayIndex)
		{
			SyncNativeChildWindowsForAllPages(this);
			this->_lastSelectIndex = _displayIndex;
		}
	}
	return pages;
}

D2D1_RECT_F TabControl::GetTitleRect(int index)
{
	if (index < 0) return D2D1_RECT_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	ClampTitleScrollOffset();
	const auto size = this->GetActualSizeDip().NonNegative();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float titleWidth = (std::max)(0.0f, this->TitleWidth);
	const float titleHeight = (std::max)(0.0f, this->TitleHeight);
	const float titleGap = (std::max)(0.0f, this->TitleGap);
	const float scroll = this->EnableTitleScroll ? this->TitleScrollOffset : 0.0f;
	const float indexStart = static_cast<float>(index);
	const float indexEnd = static_cast<float>(index + 1);

	switch (this->TitlePosition)
	{
	case TabControlTitlePosition::Bottom:
		return D2D1_RECT_F{ titleWidth * indexStart - scroll + titleGap * 0.5f, (std::max)(0.0f, actualHeight - titleHeight) + titleGap * 0.5f,
			titleWidth * indexEnd - scroll - titleGap * 0.5f, actualHeight - titleGap * 0.5f };
	case TabControlTitlePosition::Left:
		return D2D1_RECT_F{ titleGap * 0.5f, titleHeight * indexStart - scroll + titleGap * 0.5f,
			(std::min)(actualWidth, titleWidth) - titleGap * 0.5f, titleHeight * indexEnd - scroll - titleGap * 0.5f };
	case TabControlTitlePosition::Right:
		return D2D1_RECT_F{ (std::max)(0.0f, actualWidth - titleWidth) + titleGap * 0.5f, titleHeight * indexStart - scroll + titleGap * 0.5f,
			actualWidth - titleGap * 0.5f, titleHeight * indexEnd - scroll - titleGap * 0.5f };
	case TabControlTitlePosition::Top:
	default:
		return D2D1_RECT_F{ titleWidth * indexStart - scroll + titleGap * 0.5f, titleGap * 0.5f,
			titleWidth * indexEnd - scroll - titleGap * 0.5f, (std::min)(actualHeight, titleHeight) - titleGap * 0.5f };
	}
}

bool TabControl::TryGetTitleIndexAt(int localX, int localY, int& outIndex)
{
	outIndex = -1;
	if (this->Count <= 0) return false;
	if (this->TitleWidth <= 0 || this->TitleHeight <= 0) return false;
	auto viewport = this->GetTitleViewportRect();
	if (!PtInRectF(viewport, (float)localX, (float)localY)) return false;
	if (this->HitTestTitleScrollButton(localX, localY) != 0) return false;

	for (int i = 0; i < this->Count; i++)
	{
		auto rect = this->GetTitleRect(i);
		if (rect.right <= rect.left || rect.bottom <= rect.top) continue;
		if (!RectIntersects(rect, viewport)) continue;
		if ((float)localX >= rect.left && (float)localX < rect.right && (float)localY >= rect.top && (float)localY < rect.bottom)
		{
			outIndex = i;
			return true;
		}
	}

	return false;
}

void TabControl::ClampSelectedIndex()
{
	if (this->Count <= 0) return;
	SetCurrentSelectedIndex(
		(std::clamp)(this->SelectedIndex, 0, this->Count - 1));
}

void TabControl::SetCurrentSelectedIndex(int value)
{
	if (_selectedIndex == value) return;
	if (_animating && value != _selectedIndex)
		FinishTransition();
	(void)SetCurrentPropertyField(L"SelectedIndex", _selectedIndex, value);
}

void TabControl::ApplySelectedIndexChange(int oldValue, int newValue)
{
	if (oldValue == newValue) return;
	if (this->Count <= 0)
	{
		_displayIndex = -1;
		_animating = false;
		return;
	}
	StartTransitionTo(newValue);
	this->OnSelectedChanged(this);
}

void TabControl::LayoutPage(TabPage* page, float offsetX, float offsetY)
{
	if (!page) return;
	auto contentRect = this->GetContentRect();
	page->ApplyLayout(cui::core::Rect::FromLTRB(
		contentRect.left + offsetX,
		contentRect.top + offsetY,
		contentRect.right + offsetX,
		contentRect.bottom + offsetY));
}

void TabControl::PerformPendingLayout()
{
	ClampSelectedIndex();
	for (int i = 0; i < this->Count; ++i)
	{
		auto* page = (TabPage*)this->operator[](i);
		LayoutPage(page, 0.0f);
		if (page && !page->IsLayoutSuspended())
			page->PerformLayout();
	}
	SyncPageVisibility();
}

void TabControl::SyncPageVisibility()
{
	for (int i = 0; i < this->Count; i++)
	{
		auto* page = this->operator[](i);
		if (!page) continue;
		if (_animating)
			page->Visible = (i == _animFromIndex || i == _animToIndex);
		else
			page->Visible = (i == _displayIndex);
	}
}

void TabControl::FinishTransition()
{
	if (this->Count <= 0)
	{
		_animating = false;
		_animFromIndex = -1;
		_animToIndex = -1;
		_displayIndex = -1;
		return;
	}
	ClampSelectedIndex();
	_animating = false;
	_animProgress = 1.0f;
	_displayIndex = this->SelectedIndex;
	_animFromIndex = _displayIndex;
	_animToIndex = _displayIndex;
	SyncPageVisibility();
	if (_displayIndex >= 0 && _displayIndex < this->Count)
		LayoutPage((TabPage*)this->operator[](_displayIndex), 0);
	SyncNativeChildWindowsForAllPages(this);
	_lastSelectIndex = _displayIndex;
}

void TabControl::StartTransitionTo(int newIndex)
{
	if (this->Count <= 0)
	{
		FinishTransition();
		return;
	}
	if (_animating)
		FinishTransition();
	if (_displayIndex < 0 || _displayIndex >= this->Count)
		_displayIndex = (std::clamp)(this->SelectedIndex, 0, this->Count - 1);
	newIndex = (std::clamp)(newIndex, 0, this->Count - 1);
	EnsureTitleVisible(newIndex);
	_lastTitleEnsureIndex = newIndex;
	_lastTitleEnsureCount = this->Count;
	_lastTitleEnsureViewportLength = GetTitleViewportLength(this);
	_lastTitleEnsurePosition = this->TitlePosition;
	if (this->AnimationMode == TabControlAnimationMode::DirectReplace
		|| EffectiveAnimationDuration(AnimationDurationMs) == 0
		|| _displayIndex == newIndex)
	{
		FinishTransition();
		return;
	}
	_animFromIndex = _displayIndex;
	_animToIndex = newIndex;
	_animStartTick = ::GetTickCount64();
	_animProgress = 0.0f;
	_animating = true;
	SyncPageVisibility();
	LayoutPage((TabPage*)this->operator[](_animFromIndex), 0);
	LayoutPage((TabPage*)this->operator[](_animToIndex), 0);
	SyncNativeChildWindowsForAllPages(this);
	_lastSelectIndex = -1;
}

void TabControl::EnsureSelectionState()
{
	if (this->Count <= 0)
	{
		_displayIndex = -1;
		_animating = false;
		return;
	}
	ClampSelectedIndex();
	ClampTitleScrollOffset();
	const float titleViewportLength = GetTitleViewportLength(this);
	if (_lastTitleEnsureIndex != this->SelectedIndex ||
		_lastTitleEnsureCount != this->Count ||
		_lastTitleEnsurePosition != this->TitlePosition ||
		std::fabs(_lastTitleEnsureViewportLength - titleViewportLength) > 0.5f)
	{
		EnsureTitleVisible(this->SelectedIndex);
		_lastTitleEnsureIndex = this->SelectedIndex;
		_lastTitleEnsureCount = this->Count;
		_lastTitleEnsureViewportLength = titleViewportLength;
		_lastTitleEnsurePosition = this->TitlePosition;
	}
	if (_displayIndex < 0 || _displayIndex >= this->Count)
		_displayIndex = this->SelectedIndex;
	if (_animating)
	{
		CurrentTransitionProgress();
		return;
	}
	if (this->SelectedIndex != _displayIndex)
		StartTransitionTo(this->SelectedIndex);
	else
		SyncPageVisibility();
}

float TabControl::CurrentTransitionProgress()
{
	if (!_animating) return _animProgress;
	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(AnimationDurationMs);
	float t = duration > 0
		? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		FinishTransition();
		return 1.0f;
	}
	_animProgress = EaseOutCubic01(t);
	return _animProgress;
}

bool TabControl::IsAnimationRunning()
{
	EnsureSelectionState();
	return _animating;
}

bool TabControl::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = this->AbsRect;
	return true;
}

CursorKind TabControl::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;
	auto viewport = this->GetTitleViewportRect();
	if (PtInRectF(viewport, (float)localX, (float)localY))
		return CursorKind::Hand;
	return this->Cursor;
}

bool TabControl::CanHandleMouseWheel(int delta, int localX, int localY)
{
	if (delta == 0 || !this->IsTitleOverflowing())
		return false;
	auto viewport = this->GetTitleViewportRect();
	if (!PtInRectF(viewport, (float)localX, (float)localY))
		return false;
	const float maxOffset = GetMaxTitleScrollOffset(this);
	return delta > 0 ? this->TitleScrollOffset > 0 : this->TitleScrollOffset < maxOffset;
}

bool TabControl::HandlesNavigationKey(WPARAM key) const
{
	const bool sideTitles = IsSideTitlePosition(
		static_cast<TabControlTitlePosition>(_titlePosition));
	switch (key)
	{
	case VK_LEFT:
	case VK_RIGHT:
		return !sideTitles;
	case VK_UP:
	case VK_DOWN:
		return sideTitles;
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}

TabPage* TabControl::AddPage(std::wstring name)
{
	return InsertPage(this->Count, std::make_unique<TabPage>(std::move(name)));
}

TabPage* TabControl::AddPage(std::unique_ptr<TabPage> page)
{
	return InsertPage(this->Count, std::move(page));
}

TabPage* TabControl::InsertPage(int index, std::wstring name)
{
	if (index < 0 || index > this->Count)
		return nullptr;
	return InsertPage(index, std::make_unique<TabPage>(std::move(name)));
}

void TabControl::PreparePageMutation()
{
	if (_animating)
		FinishTransition();
	_capturedChild = nullptr;
	_hoverTitleIndex = -1;
	_hoverTitleScrollButton = 0;
	_pressedTitleScrollButton = 0;
	_pressedTitleIndex = -1;
	_dragTitleStrip = false;
	_titleStripDragMoved = false;
}

void TabControl::ReconcilePagesAfterMutation(
	Control* previouslySelectedPage, int previousSelectedIndex)
{
	int targetIndex = this->SelectedIndex;
	if (previouslySelectedPage)
	{
		auto found = std::find(
			this->Children.begin(), this->Children.end(), previouslySelectedPage);
		if (found != this->Children.end())
		{
			targetIndex = static_cast<int>(found - this->Children.begin());
		}
		else if (this->Count > 0)
		{
			targetIndex = (std::clamp)(
				previousSelectedIndex, 0, this->Count - 1);
		}
		else
		{
			targetIndex = 0;
		}
	}

	const bool targetIsValid =
		targetIndex >= 0 && targetIndex < this->Count;
	Control* selectedPageAfter = targetIsValid
		? this->Children[static_cast<size_t>(targetIndex)] : nullptr;
	const bool logicalSelectionChanged = previouslySelectedPage
		&& previouslySelectedPage != selectedPageAfter;
	const bool indexChanged = this->SelectedIndex != targetIndex;

	// 结构变化不应以旧索引为起点启动页切换动画。先把显示状态定位到
	// 新索引，再通过属性系统更新 SelectedIndex/TwoWay binding。
	_animating = false;
	_animProgress = 1.0f;
	_displayIndex = this->Count > 0
		? (std::clamp)(targetIndex, 0, this->Count - 1) : -1;
	_animFromIndex = _displayIndex;
	_animToIndex = _displayIndex;
	if (indexChanged)
		SetCurrentSelectedIndex(targetIndex);
	// SelectedIndex 数值未变（例如移除选中页后由同索引下一页接替）时，
	// 仍需发布逻辑选择变化。空集合上的属性回调也不会发布该事件。
	if (logicalSelectionChanged && (!indexChanged || this->Count <= 0))
		this->OnSelectedChanged(this);

	if (!this->IsLayoutSuspended())
	{
		for (int i = 0; i < this->Count; ++i)
			LayoutPage(static_cast<TabPage*>(this->Children[i]), 0.0f);
	}
	SyncPageVisibility();
	EnsureTitleVisible(targetIsValid ? targetIndex : -1);
	_lastTitleEnsureIndex = targetIsValid ? targetIndex : -1;
	_lastTitleEnsureCount = this->Count;
	_lastTitleEnsureViewportLength = GetTitleViewportLength(this);
	_lastTitleEnsurePosition = this->TitlePosition;
	SyncNativeChildWindowsForAllPages(this);
	_lastSelectIndex = _displayIndex;
	this->InvalidateVisual();
}

bool TabControl::ValidateChildCollection(
	std::span<Control* const> children, std::string& error) const
{
	for (auto* child : children)
	{
		if (dynamic_cast<TabPage*>(child)) continue;
		error = "TabControl can contain TabPage children only";
		return false;
	}
	return true;
}

void TabControl::OnChildCollectionChanged(
	const CollectionChangedEventArgs& change,
	std::span<Control* const> previousChildren)
{
	(void)change;
	const int previousSelectedIndex = this->SelectedIndex;
	Control* previouslySelectedPage = previousSelectedIndex >= 0
		&& static_cast<size_t>(previousSelectedIndex) < previousChildren.size()
		? previousChildren[static_cast<size_t>(previousSelectedIndex)] : nullptr;
	// A direct collection mutation can happen while an old transition is active.
	// Its indices no longer describe the new collection, so cancel without
	// dereferencing them and reconcile from the previous page identity.
	_animating = false;
	_animProgress = 1.0f;
	_animFromIndex = -1;
	_animToIndex = -1;
	_capturedChild = nullptr;
	_hoverTitleIndex = -1;
	_hoverTitleScrollButton = 0;
	_pressedTitleScrollButton = 0;
	_pressedTitleIndex = -1;
	_dragTitleStrip = false;
	_titleStripDragMoved = false;
	ReconcilePagesAfterMutation(
		previouslySelectedPage, previousSelectedIndex);
}

TabPage* TabControl::InsertPage(
	int index, std::unique_ptr<TabPage> page)
{
	if (!page)
		throw std::invalid_argument("不能添加空页");
	if (index < 0 || index > this->Count)
		throw std::out_of_range("页索引超出范围");

	PreparePageMutation();
	page->BackColor = this->BackColor;
	return this->InsertOwned(index, std::move(page));
}

TabPage* TabControl::GetPage(int index) const noexcept
{
	if (index < 0 || static_cast<size_t>(index) >= this->Children.size())
		return nullptr;
	return static_cast<TabPage*>(
		this->Children[static_cast<size_t>(index)]);
}

int TabControl::IndexOfPage(const TabPage* page) const noexcept
{
	if (!page) return -1;
	auto found = std::find(
		this->Children.begin(), this->Children.end(), page);
	return found == this->Children.end()
		? -1 : static_cast<int>(found - this->Children.begin());
}

std::unique_ptr<TabPage> TabControl::DetachPageAt(int index)
{
	if (index < 0 || index >= this->Count)
		return {};
	PreparePageMutation();
	auto* page = static_cast<TabPage*>(
		this->Children[static_cast<size_t>(index)]);
	auto detached = this->DetachControl(page);
	return std::unique_ptr<TabPage>(
		static_cast<TabPage*>(detached.release()));
}

std::unique_ptr<TabPage> TabControl::DetachPage(TabPage* page)
{
	return DetachPageAt(IndexOfPage(page));
}

bool TabControl::RemovePageAt(int index)
{
	auto page = DetachPageAt(index);
	return page != nullptr;
}

bool TabControl::RemovePage(TabPage* page)
{
	return RemovePageAt(IndexOfPage(page));
}

void TabControl::ClearPages()
{
	if (this->Children.empty()) return;
	PreparePageMutation();
	this->ClearControls();
}

bool TabControl::SelectPage(int index)
{
	if (index < 0 || index >= this->Count)
		return false;
	if (this->SelectedIndex == index)
	{
		EnsureTitleVisible(index);
		return true;
	}
	SetCurrentSelectedIndex(index);
	return true;
}

GET_CPP(TabControl, int, PageCount)
{
	return this->Count;
}
GET_CPP(TabControl, Control::ChildCollection&, Pages)
{
	return this->Children;
}
void TabControl::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	auto contentRect = this->GetContentRect();
	const float contentWidth = (std::max)(0.0f, contentRect.right - contentRect.left);
	const float contentHeight = (std::max)(0.0f, contentRect.bottom - contentRect.top);
	EnsureSelectionState();
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(this->TitleCornerRadius);
		}
		
		if (this->Count > 0)
		{
			ClampSelectedIndex();
			ClampTitleScrollOffset();
			const auto titleViewport = this->GetTitleViewportRect();
			const float titleViewportWidth = RectWidth(titleViewport);
			const float titleViewportHeight = RectHeight(titleViewport);

			if (titleViewportWidth > 0.0f && titleViewportHeight > 0.0f)
			{
				d2d->PushDrawRect(titleViewport.left, titleViewport.top, titleViewportWidth, titleViewportHeight);
				for (int i = 0; i < this->Count; i++)
				{
					auto titleRect = this->GetTitleRect(i);
					if (!RectIntersects(titleRect, titleViewport)) continue;
					const float titleWidth = (std::max)(0.0f, titleRect.right - titleRect.left);
					const float titleHeight = (std::max)(0.0f, titleRect.bottom - titleRect.top);
					if (titleWidth <= 0.0f || titleHeight <= 0.0f) continue;
					auto* page = (TabPage*)this->operator[](i);
					const auto& titleText = page->Text;
					const bool hasText = !titleText.empty();
					auto textsize = hasText ? font->GetTextSize(titleText) : D2D1_SIZE_F{ 0.0f, 0.0f };
					auto* headerBmp = page->EnsureHeaderImageCache();
					const bool hasHeaderImage = headerBmp != nullptr;
					const float inset = (std::max)(0.0f, this->TitleInset);
					auto visualRect = InsetRect(titleRect, inset, (std::min)(inset, titleHeight * 0.18f));
					const float visualWidth = (std::max)(0.0f, visualRect.right - visualRect.left);
					const float visualHeight = (std::max)(0.0f, visualRect.bottom - visualRect.top);
					if (visualWidth <= 0.0f || visualHeight <= 0.0f) continue;
					const bool selected = i == this->SelectedIndex;
					const bool hovered = i == this->_hoverTitleIndex;
					const bool sideTitles = this->TitlePosition == TabControlTitlePosition::Left || this->TitlePosition == TabControlTitlePosition::Right;
					float leftPad = sideTitles ? 12.0f : 10.0f;
					float rightPad = sideTitles ? 12.0f : 10.0f;
					if (selected)
					{
						const float accentReserve = (std::max)(2.0f, this->SelectedAccentSize) + 10.0f;
						if (this->TitlePosition == TabControlTitlePosition::Left)
							rightPad += accentReserve;
						else
							leftPad += accentReserve;
					}
					const float contentLeft = visualRect.left + leftPad;
					const float contentRight = visualRect.right - rightPad;
					const float contentWidth = (std::max)(1.0f, contentRight - contentLeft);
					const float iconMax = (std::max)(1.0f, (std::min)(visualWidth, visualHeight) - 8.0f);
					const float iconSize = hasHeaderImage ? (std::min)((std::max)(1.0f, page->HeaderImageSize), iconMax) : 0.0f;
					const float imageGap = (hasHeaderImage && hasText) ? (std::max)(0.0f, page->HeaderImageGap) : 0.0f;
					float iconX = contentLeft;
					float iconY = visualRect.top + (visualHeight - iconSize) * 0.5f;
					float textX = contentLeft;
					float maxTextWidth = contentWidth;
					if (hasHeaderImage && !hasText)
					{
						iconX = visualRect.left + (visualWidth - iconSize) * 0.5f;
					}
					else if (hasHeaderImage)
					{
						const float textAvailable = (std::max)(1.0f, contentWidth - iconSize - imageGap);
						const float desiredWidth = iconSize + imageGap + (std::min)(textsize.width, textAvailable);
						float startX = contentLeft;
						if (!sideTitles && desiredWidth < contentWidth)
							startX = contentLeft + (contentWidth - desiredWidth) * 0.5f;
						iconX = startX;
						textX = startX + iconSize + imageGap;
						maxTextWidth = (std::max)(1.0f, contentRight - textX);
					}
					else if (hasText)
					{
						if (!sideTitles && textsize.width < maxTextWidth)
							textX = contentLeft + (maxTextWidth - textsize.width) * 0.5f;
					}
					float textY = visualRect.top + (visualHeight - textsize.height) * 0.5f;
					if (textY < visualRect.top) textY = visualRect.top;
					d2d->PushDrawRect(titleRect.left, titleRect.top, titleWidth, titleHeight);
					if (selected)
					{
						d2d->FillRoundRect(visualRect, this->SelectedTitleBackColor, this->TitleCornerRadius);
						DrawTabAccent(this, d2d, visualRect);
					}
					else
					{
						d2d->FillRoundRect(visualRect, hovered ? this->TitleHoverBackColor : this->TitleBackColor, this->TitleCornerRadius);
					}
					if (hasHeaderImage)
					{
						const auto iconBox = D2D1::RectF(iconX, iconY, iconX + iconSize, iconY + iconSize);
						d2d->DrawBitmap(headerBmp, FitBitmapRect(headerBmp, iconBox));
					}
					if (hasText)
					{
						d2d->DrawString(titleText, textX, textY, maxTextWidth, textsize.height + 2.0f,
							selected ? this->ForeColor : this->TitleMutedForeColor, font);
					}
					d2d->PopDrawRect();
				}
				d2d->PopDrawRect();
				DrawTitleScrollChrome(this, d2d);
			}
			if (contentWidth > 0.0f && contentHeight > 0.0f)
			{
				const auto contentBackColor = GetTabContentBackColor(this);
				if (contentBackColor.a > 0.0f)
				{
					const float contentRadius = (std::clamp)(this->TitleCornerRadius, 0.0f, (std::min)(contentWidth, contentHeight) * 0.5f);
					d2d->FillRoundRect(contentRect.left, contentRect.top, contentWidth, contentHeight, contentBackColor, contentRadius);
				}
			}
			if (contentWidth > 0.0f && contentHeight > 0.0f && (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive()))
			{
				d2d->PushDrawRect(contentRect.left, contentRect.top, contentWidth, contentHeight);
				if (_animating && this->AnimationMode == TabControlAnimationMode::SlideHorizontal &&
					_animFromIndex >= 0 && _animFromIndex < this->Count && _animToIndex >= 0 && _animToIndex < this->Count)
				{
					const float progress = CurrentTransitionProgress();
					const int direction = (_animToIndex >= _animFromIndex) ? 1 : -1;
					const float animationWidth = contentWidth;
					const float animationHeight = contentHeight;
					const bool verticalAnimation = this->TitlePosition == TabControlTitlePosition::Left || this->TitlePosition == TabControlTitlePosition::Right;
					auto* fromPage = (TabPage*)this->operator[](_animFromIndex);
					auto* toPage = (TabPage*)this->operator[](_animToIndex);
					if (verticalAnimation)
					{
						LayoutPage(fromPage, 0.0f, -(float)direction * progress * animationHeight);
						LayoutPage(toPage, 0.0f, (float)direction * (1.0f - progress) * animationHeight);
					}
					else
					{
						LayoutPage(fromPage, -(float)direction * progress * animationWidth);
						LayoutPage(toPage, (float)direction * (1.0f - progress) * animationWidth);
					}
					UpdateTabPageContent(this, fromPage);
					UpdateTabPageContent(this, toPage);
				}
				else if (_displayIndex >= 0 && _displayIndex < this->Count)
				{
					auto* page = (TabPage*)this->operator[](_displayIndex);
					LayoutPage(page, 0);
					UpdateTabPageContent(this, page);
					if (this->_lastSelectIndex != _displayIndex)
					{
						SyncNativeChildWindowsForAllPages(this);
						this->_lastSelectIndex = _displayIndex;
					}
				}
				d2d->PopDrawRect();
			}
		}
		if (contentWidth > 0.0f && contentHeight > 0.0f)
		{
			const float border = (std::max)(1.0f, this->BorderThickness);
			d2d->DrawRoundRect(contentRect.left + border * 0.5f, contentRect.top + border * 0.5f,
				(std::max)(0.0f, contentWidth - border), (std::max)(0.0f, contentHeight - border),
				this->BorderColor, border, this->TitleCornerRadius);
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
bool TabControl::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;

	if (this->Count > 0)
	{
		EnsureSelectionState();
		auto contentRect = this->GetContentRect();
		const int contentX = localX - (int)std::lround(contentRect.left);
		const int contentY = localY - (int)std::lround(contentRect.top);

		bool handledTitleClick = false;
		auto releaseCaptureIfOwned = [&]()
			{
				if (this->ParentForm && GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
			};
		auto captureMouse = [&]()
			{
				if (this->ParentForm && this->ParentForm->Handle)
					SetCapture(this->ParentForm->Handle);
			};
		auto selectTitleIndex = [&](int index)
			{
				(void)SelectPage(index);
			};

		if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			int hoverIndex = -1;
			int hoverButton = this->HitTestTitleScrollButton(localX, localY);
			if (hoverButton == 0 && !this->_titleStripDragMoved)
				TryGetTitleIndexAt(localX, localY, hoverIndex);
			if (this->_hoverTitleIndex != hoverIndex || this->_hoverTitleScrollButton != hoverButton)
			{
				this->_hoverTitleIndex = hoverIndex;
				this->_hoverTitleScrollButton = hoverButton;
				this->InvalidateVisual();
			}
		}
		else if (message == WM_MOUSELEAVE && (this->_hoverTitleIndex != -1 || this->_hoverTitleScrollButton != 0))
		{
			this->_hoverTitleIndex = -1;
			this->_hoverTitleScrollButton = 0;
			this->InvalidateVisual();
		}

		if (message == WM_MOUSEWHEEL)
		{
			const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			if (CanHandleMouseWheel(delta, localX, localY))
			{
				int steps = delta / WHEEL_DELTA;
				float scrollDelta = 0.0f;
				if (steps != 0)
					scrollDelta = -steps * (std::max)(1.0f, this->TitleScrollMouseWheelStep);
				else
					scrollDelta = delta < 0 ? (std::max)(1.0f, this->TitleScrollMouseWheelStep) : -(std::max)(1.0f, this->TitleScrollMouseWheelStep);
				ScrollTitleBy(scrollDelta);
				handledTitleClick = true;
			}
		}
#ifdef WM_MOUSEHWHEEL
		else if (message == WM_MOUSEHWHEEL && this->IsTitleOverflowing())
		{
			auto titleViewport = this->GetTitleViewportRect();
			if (PtInRectF(titleViewport, (float)localX, (float)localY))
			{
				const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				ScrollTitleBy(delta > 0 ? (std::max)(1.0f, this->TitleScrollMouseWheelStep) : -(std::max)(1.0f, this->TitleScrollMouseWheelStep));
				handledTitleClick = true;
			}
		}
#endif
		else if (message == WM_KEYDOWN && HandlesNavigationKey(wParam))
		{
			const bool sideTitles = IsSideTitle(this);
			int nextIndex = this->SelectedIndex;
			bool handledKey = true;
			switch (wParam)
			{
			case VK_LEFT:
				if (!sideTitles) nextIndex--;
				else handledKey = false;
				break;
			case VK_RIGHT:
				if (!sideTitles) nextIndex++;
				else handledKey = false;
				break;
			case VK_UP:
				if (sideTitles) nextIndex--;
				else handledKey = false;
				break;
			case VK_DOWN:
				if (sideTitles) nextIndex++;
				else handledKey = false;
				break;
			case VK_HOME:
				nextIndex = 0;
				break;
			case VK_END:
				nextIndex = this->Count - 1;
				break;
			case VK_PRIOR:
				ScrollTitleBy(-GetTitleViewportLength(this));
				break;
			case VK_NEXT:
				ScrollTitleBy(GetTitleViewportLength(this));
				break;
			default:
				handledKey = false;
				break;
			}
			if (handledKey)
			{
				if (nextIndex != this->SelectedIndex)
					selectTitleIndex((std::clamp)(nextIndex, 0, this->Count - 1));
				handledTitleClick = true;
			}
		}

		if (this->_dragTitleStrip && message == WM_MOUSEMOVE)
		{
			const int axis = GetTitleAxisPos(this, localX, localY);
			const int dragDelta = axis - this->_titleDragStartPos;
			if (std::abs(dragDelta) > 3 || this->_titleStripDragMoved)
			{
				this->_titleStripDragMoved = true;
				if (this->_hoverTitleIndex != -1)
				{
					this->_hoverTitleIndex = -1;
					this->InvalidateVisual();
				}
				SetCurrentTitleScrollOffset(this->_titleDragStartOffset - dragDelta);
			}
			handledTitleClick = true;
		}
		else if (this->_dragTitleStrip && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
		{
			const bool clickSelect = !this->_titleStripDragMoved;
			const int pressedIndex = this->_pressedTitleIndex;
			this->_dragTitleStrip = false;
			this->_titleStripDragMoved = false;
			this->_pressedTitleIndex = -1;
			releaseCaptureIfOwned();
			if (clickSelect)
			{
				int hitIndex = -1;
				if (pressedIndex >= 0 && TryGetTitleIndexAt(localX, localY, hitIndex) && hitIndex == pressedIndex)
					selectTitleIndex(hitIndex);
			}
			this->InvalidateVisual();
			handledTitleClick = true;
		}
		else if (this->_pressedTitleScrollButton != 0 && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
		{
			this->_pressedTitleScrollButton = 0;
			releaseCaptureIfOwned();
			this->InvalidateVisual();
			handledTitleClick = true;
		}
		else if (this->_pressedTitleScrollButton != 0 && message == WM_MOUSEMOVE)
		{
			handledTitleClick = true;
		}

		if (!handledTitleClick && message == WM_LBUTTONDOWN)
		{
			int scrollButton = HitTestTitleScrollButton(localX, localY);
			if (scrollButton != 0)
			{
				this->_pressedTitleScrollButton = scrollButton;
				this->_capturedChild = nullptr;
				ScrollTitleBy(scrollButton * (std::max)(1.0f, this->TitleScrollMouseWheelStep));
				captureMouse();
				this->InvalidateVisual();
				handledTitleClick = true;
			}
			else
			{
				int newSelected = -1;
				if (TryGetTitleIndexAt(localX, localY, newSelected))
				{
					this->_capturedChild = nullptr;
					this->_pressedTitleIndex = newSelected;
					if (IsTitleOverflowing())
					{
						this->_dragTitleStrip = true;
						this->_titleStripDragMoved = false;
						this->_titleDragStartPos = GetTitleAxisPos(this, localX, localY);
						this->_titleDragStartOffset = this->TitleScrollOffset;
						captureMouse();
					}
					else
					{
						selectTitleIndex(newSelected);
					}
					handledTitleClick = true;
				}
			}
		}

		// Content 区域坐标
		TabPage* page = (_displayIndex >= 0 && _displayIndex < this->Count)
			? (TabPage*)this->operator[](_displayIndex)
			: (TabPage*)this->operator[](this->SelectedIndex);

		auto forwardToChild = [&](Control* c)
			{
				if (!c) return;
				const auto location = c->GetActualLocationDip();
				c->ProcessMessage(
					message, wParam, lParam,
					static_cast<int>(std::floor((float)contentX - location.x)),
					static_cast<int>(std::floor((float)contentY - location.y)));
			};

		// 鼠标按住期间：持续转发到按下时命中的子控件（解决拖动/松开丢失）
		bool mousePressed = (wParam & MK_LBUTTON) || (wParam & MK_RBUTTON) || (wParam & MK_MBUTTON);
		if (_animating)
		{
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			{
				this->_capturedChild = nullptr;
				if (this->ParentForm && GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
			}
		}
		else if ((message == WM_MOUSEMOVE || message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) && this->_capturedChild)
		{
			forwardToChild(this->_capturedChild);
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			{
				this->_capturedChild = nullptr;
				if (this->ParentForm && GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
			}
		}
		else if ((message == WM_MOUSEMOVE && mousePressed) && this->_capturedChild)
		{
			forwardToChild(this->_capturedChild);
		}
		else if (!handledTitleClick)
		{
			// 按下时：命中哪个子控件就捕获它
			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
				message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP ||
				message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDBLCLK ||
				message == WM_MOUSEMOVE || message == WM_MOUSEWHEEL)
			{
				// 只在 content 区域才命中子控件
				if ((float)localX >= contentRect.left && (float)localX < contentRect.right &&
					(float)localY >= contentRect.top && (float)localY < contentRect.bottom)
				{
					Control* hit = nullptr;
					for (auto c : page->GetChildrenInReverseZOrder())
					{
						if (!c || !c->Visible || !c->Enable) continue;
						const auto loc = c->GetActualLocationDip();
						const cui::core::Rect childRect{ loc, c->GetActualSizeDip() };
						if (childRect.Contains(cui::core::Point{
							(float)contentX, (float)contentY }))
						{
							hit = c;
							break;
						}
					}

					if (hit)
					{
						// 捕获鼠标，确保鼠标移出窗口也能持续收到 move/up（拖动选中/下拉框等）
						if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
						{
							this->_capturedChild = hit;
							if (this->ParentForm && this->ParentForm->Handle)
								SetCapture(this->ParentForm->Handle);
						}
						forwardToChild(hit);
					}
					else if (page)
					{
						page->ProcessMessage(message, wParam, lParam, contentX, contentY);
					}
				}
			}
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
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
		if (!files.empty())
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		// 防御：如果捕获还在，释放掉
		if (this->_capturedChild && this->ParentForm && (GetCapture() == this->ParentForm->Handle))
			ReleaseCapture();
		this->_capturedChild = nullptr;
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
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
