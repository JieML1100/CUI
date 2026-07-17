#define NOMINMAX
#include "Expander.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<Expander, TValue> ExpanderPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<Expander, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto ExpanderPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Expander& target,
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

	ControlPropertyOptions<Expander, float> ExpanderMetricOptions(
		float defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		auto options = ExpanderPropertyOptions(
			defaultValue, category, categoryOrder, order,
			ControlPropertyEditorKind::Number, flags);
		options.Coerce = [](
			Expander&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	bool ExpanderColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<Expander, D2D1_COLOR_F> ExpanderColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ExpanderPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = ExpanderColorsEqual;
		return options;
	}

	float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + (std::max)(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	D2D1_COLOR_F ScaleAlpha(D2D1_COLOR_F color, float scale)
	{
		color.a *= (std::clamp)(scale, 0.0f, 1.0f);
		return color;
	}

	D2D1_POINT_2F RotateAround(const D2D1_POINT_2F& point, float cx, float cy, float angle)
	{
		const float dx = point.x - cx;
		const float dy = point.y - cy;
		const float s = std::sin(angle);
		const float c = std::cos(angle);
		return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
	}

	void DrawExpanderChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		progress = (std::clamp)(progress, 0.0f, 1.0f);
		const float angle = progress * 1.57079632679f;
		const float halfW = size * 0.28f;
		const float halfH = size * 0.46f;
		D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
		D2D1_POINT_2F p2 = D2D1::Point2F(cx + halfW, cy);
		D2D1_POINT_2F p3 = D2D1::Point2F(cx - halfW, cy + halfH);
		p1 = RotateAround(p1, cx, cy, angle);
		p2 = RotateAround(p2, cx, cy, angle);
		p3 = RotateAround(p3, cx, cy, angle);
		d2d->DrawLine(p1, p2, color, 1.8f);
		d2d->DrawLine(p2, p3, color, 1.8f);
	}
}

UIClass Expander::Type()
{
	return UIClass::UI_Expander;
}

void Expander::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto expandedOptions = ExpanderPropertyOptions(
			true, L"Behavior", 110, 20,
			ControlPropertyEditorKind::Boolean,
			ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsRender);
		expandedOptions.Changed = [](
			Expander& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyExpandedStateChange(oldValue, newValue);
		};
		BindingPropertyRegistry::Register<Expander, bool>(L"IsExpanded",
			[](Expander& target) { return target.IsExpanded; },
			[](Expander& target, const bool& value) { target.IsExpanded = value; },
			ExpanderPropertySubscriber(L"IsExpanded"), std::move(expandedOptions));

		auto animationOptions = ExpanderPropertyOptions(
			160, L"Behavior", 110, 10,
			ControlPropertyEditorKind::Number);
		animationOptions.Coerce = [](
			Expander&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		animationOptions.Design.Minimum = 0.0;
		animationOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<Expander, int>(L"AnimationDurationMs",
			[](Expander& target) { return static_cast<int>(target.AnimationDurationMs); },
			[](Expander& target, const int& value)
			{
				target.AnimationDurationMs = static_cast<UINT>(value);
			},
			ExpanderPropertySubscriber(L"AnimationDurationMs"),
			std::move(animationOptions));

		BindingPropertyRegistry::Register<Expander, float>(L"HeaderHeight",
			[](Expander& target) { return target.HeaderHeight; },
			[](Expander& target, const float& value) { target.HeaderHeight = value; },
			ExpanderPropertySubscriber(L"HeaderHeight"),
			ExpanderMetricOptions(36.0f, L"Layout", 100, 180,
				ControlPropertyFlags::AffectsMeasure
				| ControlPropertyFlags::AffectsRender));
		BindingPropertyRegistry::Register<Expander, float>(L"HeaderPaddingX",
			[](Expander& target) { return target.HeaderPaddingX; },
			[](Expander& target, const float& value) { target.HeaderPaddingX = value; },
			ExpanderPropertySubscriber(L"HeaderPaddingX"),
			ExpanderMetricOptions(12.0f, L"Layout", 100, 190));
		BindingPropertyRegistry::Register<Expander, float>(L"ChevronSize",
			[](Expander& target) { return target.ChevronSize; },
			[](Expander& target, const float& value) { target.ChevronSize = value; },
			ExpanderPropertySubscriber(L"ChevronSize"),
			ExpanderMetricOptions(13.0f, L"Layout", 100, 200));
		BindingPropertyRegistry::Register<Expander, float>(L"Border",
			[](Expander& target) { return target.Border; },
			[](Expander& target, const float& value) { target.Border = value; },
			ExpanderPropertySubscriber(L"Border"),
			ExpanderMetricOptions(1.0f, L"Appearance", 200, 10));
		RegisterPanelCornerRadiusMetadata<Expander>(7.0f);

#define CUI_REGISTER_EXPANDER_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<Expander, D2D1_COLOR_F>(propertyName, \
			[](Expander& target) { return target.name; }, \
			[](Expander& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			ExpanderPropertySubscriber(propertyName), ExpanderColorOptions(defaultValue, order))

		CUI_REGISTER_EXPANDER_COLOR(SurfaceColor, L"SurfaceColor", cui::theme::palette::Surface, 30);
		CUI_REGISTER_EXPANDER_COLOR(HeaderBackColor, L"HeaderBackColor", cui::theme::palette::SurfaceMuted, 40);
		CUI_REGISTER_EXPANDER_COLOR(HeaderHoverBackColor, L"HeaderHoverBackColor", cui::theme::palette::AccentSoft, 50);
		CUI_REGISTER_EXPANDER_COLOR(ContentBackColor, L"ContentBackColor", cui::theme::palette::SurfaceSubtle, 60);
		CUI_REGISTER_EXPANDER_COLOR(AccentColor, L"AccentColor", cui::theme::palette::Accent, 70);
		CUI_REGISTER_EXPANDER_COLOR(MutedTextColor, L"MutedTextColor", cui::theme::palette::TextMuted, 80);
		CUI_REGISTER_EXPANDER_COLOR(DisabledOverlayColor, L"DisabledOverlayColor", cui::theme::palette::DisabledOverlay, 90);

#undef CUI_REGISTER_EXPANDER_COLOR
		return true;
	}();
	(void)registered;
}

Expander::Expander()
	: Panel()
{
	InitializePanelCornerRadiusDefault(7.0f);
	InitializePanelDisabledOverlayColorDefault(
		cui::theme::palette::DisabledOverlay);
	this->Text = L"Expander";
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->Cursor = CursorKind::Arrow;
	this->OnTextChanged += [this](Control* sender, std::wstring oldText, std::wstring newText)
		{
			(void)sender;
			(void)oldText;
			(void)newText;
			InvalidateVisual();
		};
}

Expander::Expander(std::wstring text, int x, int y, int width, int height)
	: Expander()
{
	this->Text = text;
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

GET_CPP(Expander, bool, IsExpanded)
{
	return _isExpanded;
}

SET_CPP(Expander, bool, IsExpanded)
{
	(void)SetPropertyField(L"IsExpanded", _isExpanded, value);
}

#define CUI_EXPANDER_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(Expander, type, name) { return field; } \
	SET_CPP(Expander, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_EXPANDER_PROPERTY_IMPL(float, HeaderHeight, _headerHeight, L"HeaderHeight")
CUI_EXPANDER_PROPERTY_IMPL(float, HeaderPaddingX, _headerPaddingX, L"HeaderPaddingX")
CUI_EXPANDER_PROPERTY_IMPL(float, ChevronSize, _chevronSize, L"ChevronSize")
CUI_EXPANDER_PROPERTY_IMPL(float, Border, _border, L"Border")
CUI_EXPANDER_PROPERTY_IMPL(D2D1_COLOR_F, SurfaceColor, _surfaceColor, L"SurfaceColor")
CUI_EXPANDER_PROPERTY_IMPL(D2D1_COLOR_F, HeaderBackColor, _headerBackColor, L"HeaderBackColor")
CUI_EXPANDER_PROPERTY_IMPL(D2D1_COLOR_F, HeaderHoverBackColor, _headerHoverBackColor, L"HeaderHoverBackColor")
CUI_EXPANDER_PROPERTY_IMPL(D2D1_COLOR_F, ContentBackColor, _contentBackColor, L"ContentBackColor")
CUI_EXPANDER_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")
CUI_EXPANDER_PROPERTY_IMPL(D2D1_COLOR_F, MutedTextColor, _mutedTextColor, L"MutedTextColor")

GET_CPP(Expander, float, CornerRadius)
{
	return Panel::GetCornerRadius();
}

SET_CPP(Expander, float, CornerRadius)
{
	Panel::SetCornerRadius(value);
}

GET_CPP(Expander, D2D1_COLOR_F, DisabledOverlayColor)
{
	return Panel::GetDisabledOverlayColor();
}

SET_CPP(Expander, D2D1_COLOR_F, DisabledOverlayColor)
{
	Panel::SetDisabledOverlayColor(value);
}

#undef CUI_EXPANDER_PROPERTY_IMPL

GET_CPP(Expander, UINT, AnimationDurationMs)
{
	return static_cast<UINT>(_animationDurationMs);
}

SET_CPP(Expander, UINT, AnimationDurationMs)
{
	const auto maximum = static_cast<UINT>((std::numeric_limits<int>::max)());
	const int proposed = static_cast<int>((std::min)(value, maximum));
	(void)SetPropertyField(L"AnimationDurationMs", _animationDurationMs, proposed);
}

float Expander::CurrentExpandProgress()
{
	if (!_animating)
	{
		_expandProgress = _isExpanded ? 1.0f : 0.0f;
		return _expandProgress;
	}

	ULONGLONG currentTick = ::GetTickCount64();
	ULONGLONG elapsedMs = currentTick >= _animStartTick ? currentTick - _animStartTick : 0;
	const UINT duration = EffectiveAnimationDuration(AnimationDurationMs);
	float normalizedTime = duration > 0 ? (float)elapsedMs / (float)duration : 1.0f;
	if (normalizedTime >= 1.0f)
	{
		_expandProgress = _animTargetProgress;
		_animating = false;
		return _expandProgress;
	}

	normalizedTime = 1.0f - std::pow(1.0f - (std::clamp)(normalizedTime, 0.0f, 1.0f), 3.0f);
	_expandProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * normalizedTime;
	return _expandProgress;
}

void Expander::PerformExpanderLayoutIfNeeded()
{
	if (this->IsLayoutSuspended()) return;
	if (!_needsLayout && !(_layoutEngine && _layoutEngine->NeedsLayout()))
		return;
	Thickness originalPadding = this->Padding;
	_padding.Top += HeaderHeight;
	PerformLayout();
	_padding = originalPadding;
}

void Expander::PerformPendingLayout()
{
	PerformExpanderLayoutIfNeeded();
}

bool Expander::HeaderHitTest(int localX, int localY) const
{
	return localX >= 0 && localY >= 0 && localX <= _size.cx && localY <= _headerHeight;
}

void Expander::ApplyExpandedStateChange(bool oldValue, bool newValue)
{
	if (_animating)
		CurrentExpandProgress();
	else
		_expandProgress = oldValue ? 1.0f : 0.0f;
	_animStartProgress = _expandProgress;
	_animTargetProgress = newValue ? 1.0f : 0.0f;
	if (EffectiveAnimationDuration(AnimationDurationMs) == 0
		|| std::fabs(_animTargetProgress - _animStartProgress) <= 0.001f)
	{
		_expandProgress = _animTargetProgress;
		_animating = false;
	}
	else
	{
		_animStartTick = ::GetTickCount64();
		_animating = true;
	}
	if (!newValue && ParentForm && ParentForm->Selected && ParentForm->Selected->Parent == this)
		ParentForm->SetSelectedControl(nullptr, false);
	if (ParentForm)
		ParentForm->Invalidate(true);
	InvalidateVisual();
	OnExpandedChanged(this, newValue);
}

void Expander::SetCurrentExpanded(bool value)
{
	(void)SetCurrentPropertyField(L"IsExpanded", _isExpanded, value);
}

void Expander::SetExpanded(bool value)
{
	IsExpanded = value;
}

void Expander::Toggle()
{
	SetCurrentExpanded(!_isExpanded);
}

SIZE Expander::ActualSize()
{
	SIZE size = this->_size;
	float headerHeight = (std::clamp)(HeaderHeight, 0.0f, (float)size.cy);
	float contentHeight = (std::max)(0.0f, (float)size.cy - headerHeight);
	size.cy = (LONG)std::ceil(headerHeight + contentHeight * CurrentExpandProgress());
	return size;
}

CursorKind Expander::QueryCursor(int localX, int localY)
{
	if (!Enable) return CursorKind::Arrow;
	if (HeaderHitTest(localX, localY))
		return CursorKind::Hand;
	return this->Cursor;
}

bool Expander::ShouldHitTestChildrenAt(int localX, int localY) const
{
	if (!HitTestChildren())
		return false;
	if (localX < 0 || localX > _size.cx)
		return false;
	const float progress = const_cast<Expander*>(this)->CurrentExpandProgress();
	const float headerHeight = (std::clamp)(_headerHeight, 0.0f, (float)_size.cy);
	const float visibleContentHeight = (std::max)(0.0f, ((float)_size.cy - headerHeight) * progress);
	return localY >= (int)std::floor(headerHeight) && localY <= (int)std::ceil(headerHeight + visibleContentHeight);
}

D2D1_RECT_F Expander::GetChildrenClipRect()
{
	const float progress = CurrentExpandProgress();
	const float headerHeight = (std::clamp)(HeaderHeight, 0.0f, (float)_size.cy);
	const float visibleContentHeight = (std::max)(0.0f, ((float)_size.cy - headerHeight) * progress);
	return D2D1::RectF(0.0f, headerHeight, (float)_size.cx, headerHeight + visibleContentHeight);
}

bool Expander::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_RETURN || key == VK_SPACE;
}

bool Expander::IsAnimationRunning()
{
	CurrentExpandProgress();
	return _animating;
}

bool Expander::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!_animating)
		return false;
	outRect = this->AbsRect;
	outRect.bottom = outRect.top + (float)this->_size.cy;
	return true;
}

void Expander::Update()
{
	if (this->IsVisual == false) return;
	PerformExpanderLayoutIfNeeded();
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;

	const float progress = CurrentExpandProgress();
	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	const float fullHeight = (float)this->_size.cy;
	const float headerHeight = (std::clamp)(HeaderHeight, 0.0f, fullHeight);
	const float border = (std::max)(0.0f, Border);
	const float radius = (std::clamp)(CornerRadius, 0.0f, (std::min)(width, (std::max)(headerHeight, height)) * 0.5f);

	this->BeginRender(width, height);
	{
		D2D1_COLOR_F surface = SurfaceColor.a > 0.0f ? SurfaceColor : this->BackColor;
		d2d->FillRoundRect(0.0f, 0.0f, width, height, surface, radius);
		if (ContentBackColor.a > 0.0f && height > headerHeight)
			d2d->FillRect(0.0f, headerHeight, width, height - headerHeight, ContentBackColor);
		d2d->FillRoundRect(0.0f, 0.0f, width, (std::min)(headerHeight, height), HeaderBackColor, radius);
		if (_hoverHeader)
			d2d->FillRoundRect(1.0f, 1.0f, (std::max)(0.0f, width - 2.0f), (std::max)(0.0f, headerHeight - 2.0f),
				HeaderHoverBackColor, (std::max)(0.0f, radius - 1.0f));

		const D2D1_COLOR_F headerForeColor = Enable ? ForeColor : MutedTextColor;
		const float chevronCenterX = HeaderPaddingX + ChevronSize * 0.5f;
		const float chevronCenterY = headerHeight * 0.5f;
		DrawExpanderChevron(d2d, chevronCenterX, chevronCenterY, ChevronSize, progress, headerForeColor);

		D2D1_RECT_F textRect{
			HeaderPaddingX + ChevronSize + 9.0f,
			0.0f,
			(std::max)(HeaderPaddingX + ChevronSize + 9.0f, width - HeaderPaddingX),
			headerHeight
		};
		d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
		d2d->DrawString(GetDisplayText(), textRect.left, TextTop(Font, textRect),
			(std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect), headerForeColor, Font);
		d2d->PopDrawRect();

		if (progress > 0.001f && height > headerHeight)
		{
			D2D1_RECT_F clip = GetChildrenClipRect();
			d2d->PushDrawRect(clip.left, clip.top, RectWidth(clip), RectHeight(clip));
			if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
			{
				for (auto child : this->GetChildrenInZOrder())
				{
					if (!child || !child->Visible) continue;
					child->Update();
				}
			}
			d2d->PopDrawRect();
		}

		if (headerHeight < height)
			d2d->DrawLine(HeaderPaddingX, headerHeight, (std::max)(HeaderPaddingX, width - HeaderPaddingX), headerHeight, ScaleAlpha(BorderColor, 0.62f), 1.0f);
		if (border > 0.0f && BorderColor.a > 0.0f)
			d2d->DrawRoundRect(border * 0.5f, border * 0.5f,
				(std::max)(0.0f, width - border), (std::max)(0.0f, height - border),
				BorderColor, border, radius);
		if (AccentColor.a > 0.0f)
		{
			float accentHeight = (std::max)(6.0f, headerHeight - 14.0f);
			d2d->FillRoundRect(2.0f, (headerHeight - accentHeight) * 0.5f, 3.0f, accentHeight, AccentColor, 1.5f);
		}
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, DisabledOverlayColor, radius);
	}
	this->EndRender();
}

bool Expander::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	PerformExpanderLayoutIfNeeded();

	const bool isHeaderHit = HeaderHitTest(localX, localY);
	switch (message)
	{
	case WM_MOUSEMOVE:
		if (ParentForm) ParentForm->UnderMouse = this;
		if (_hoverHeader != isHeaderHit)
		{
			_hoverHeader = isHeaderHit;
			InvalidateVisual();
		}
		break;
	case WM_LBUTTONDOWN:
		if (ParentForm)
			ParentForm->SetSelectedControl(this, false);
		if (isHeaderHit)
		{
			OnMouseDown(this, MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam)));
			InvalidateVisual();
			return true;
		}
		break;
	case WM_LBUTTONUP:
		if (isHeaderHit)
		{
			Toggle();
			MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			OnMouseUp(this, eventArgs);
			OnMouseClick(this, eventArgs);
			return true;
		}
		break;
	case WM_KEYDOWN:
		if (ParentForm && ParentForm->Selected == this && (wParam == VK_RETURN || wParam == VK_SPACE))
		{
			Toggle();
			OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
			return true;
		}
		break;
	default:
		break;
	}

	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}
