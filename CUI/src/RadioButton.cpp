#include "RadioButton.h"
#include "TreeInfrastructure.h"
#include "Layout/OverlayLayout.h"
#include "Window.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <unordered_set>
#include <vector>

namespace
{
	D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return D2D1_COLOR_F{
			from.r + (to.r - from.r) * t,
			from.g + (to.g - from.g) * t,
			from.b + (to.b - from.b) * t,
			from.a + (to.a - from.a) * t
		};
	}

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a *= (std::clamp)(alpha, 0.0f, 1.0f);
		return color;
	}
}

UIClass RadioButton::Type() { return UIClass::UI_RadioButton; }

RadioButton::RadioButton()
{
	RegisterDependencyProperties();
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	const ControlWeakReference lifetime(this);
	RetainEventConnection(
		cui::framework::TreeAccess::SubscribeLogicalParentChanged(
			*this, [lifetime](Control*, Control*, Control*)
			{
				auto* radio = dynamic_cast<RadioButton*>(lifetime.Get());
				if (radio && radio->IsChecked)
					radio->UpdateRadioButtonGroup();
			}));
	RetainEventConnection(
		cui::framework::TreeAccess::SubscribeVisualParentChanged(
			*this, [lifetime](Control*, Control*, Control*)
			{
				auto* radio = dynamic_cast<RadioButton*>(lifetime.Get());
				if (radio && radio->IsChecked
					&& !radio->GetLogicalParent())
					radio->UpdateRadioButtonGroup();
			}));
}

void RadioButton::RegisterDependencyProperties()
{
	ToggleButton::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<RadioButton, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Changed = [](RadioButton& target,
			const std::wstring&, const std::wstring&)
		{
			if (target.IsChecked)
				target.UpdateRadioButtonGroup();
		};
		DependencyPropertyRegistry::Register<RadioButton, std::wstring>(
			L"GroupName",
			[](RadioButton& target) { return target.GroupName; },
			[](RadioButton& target, const std::wstring& value)
			{ target.GroupName = value; },
			{}, std::move(options));
		return true;
	}();
	(void)registered;
}

void RadioButton::ConfigureContentVisual(Control& child)
{
	ContentControl::ConfigureContentVisual(child);
}

void RadioButton::StartSelectionAnimation(bool checked)
{
	if (IsChecked == checked) return;
	this->IsChecked = checked;
}

void RadioButton::OnIsCheckedChanged(bool oldValue, bool newValue)
{
	if (_animating)
		CurrentSelectionProgress();
	else
		_selectProgress = oldValue ? 1.0f : 0.0f;
	_animStartProgress = _selectProgress;
	_animTargetProgress = newValue ? 1.0f : 0.0f;
	if (EffectiveAnimationDuration(_animDurationMs) == 0
		|| std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_selectProgress = _animTargetProgress;
		_animating = false;
	}
	else
	{
		_animStartTick = ::GetTickCount64();
		_animating = true;
	}
	InvalidateVisual();
	if (newValue)
		UpdateRadioButtonGroup();
}

void RadioButton::UpdateRadioButtonGroup()
{
	if (!IsChecked) return;

	const ControlWeakReference selfReference(this);
	std::vector<ControlWeakReference> peers;
	auto remember = [&](Control* candidate)
	{
		auto* radio = dynamic_cast<RadioButton*>(candidate);
		if (radio && radio != this && radio->IsChecked
			&& radio->GroupName == _groupName)
			peers.emplace_back(radio);
	};

	if (_groupName.empty())
	{
		if (auto* parent = GetLogicalParent())
		{
			for (auto* child : parent->GetLogicalChildrenView())
				remember(child);
		}
		else if (auto* parent = GetVisualParent())
		{
			for (auto* child : parent->GetVisualChildrenView())
				remember(child);
		}
	}
	else
	{
		Control* root = this;
		while (auto* parent = root->GetRoutedParent())
			root = parent;
		std::unordered_set<Control*> visited;
		std::function<void(Control*)> collect = [&](Control* node)
		{
			if (!node || !visited.insert(node).second) return;
			remember(node);
			for (auto* child : node->GetVisualChildrenView())
				collect(child);
			for (auto* child : node->GetLogicalChildrenView())
				collect(child);
		};
		collect(root);
	}

	for (const auto& peerReference : peers)
	{
		if (!selfReference.Get()) return;
		auto* peer = dynamic_cast<RadioButton*>(peerReference.Get());
		if (!peer || !peer->IsChecked || peer->GroupName != _groupName)
			continue;
		peer->IsChecked = false;
	}
}

float RadioButton::CurrentSelectionProgress()
{
	if (!_animating)
	{
		_selectProgress = this->IsChecked ? 1.0f : 0.0f;
		return _selectProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(_animDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		_selectProgress = _animTargetProgress;
		_animating = false;
		return _selectProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_selectProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _selectProgress;
}

bool RadioButton::IsAnimationRunning()
{
	CurrentSelectionProgress();
	return _animating;
}

bool RadioButton::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = GetAbsoluteBoundsDip();
	return true;
}

cui::core::Size RadioButton::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot())
		return ContentControl::MeasureCore(available);
	const auto padding = GetSpecifiedLayout().padding;
	const auto inner = available.Deflate(padding).Normalized();
	const float fontHeight = GetRenderFont() ? GetRenderFont()->FontHeight : 14.0f;
	const float circle = (std::max)(14.0f, fontHeight * 0.82f);
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	cui::core::Size desired{};
	const float contentOffset = content ? circle + TextGap : circle;
	if (content && !content->IsCollapsed())
	{
		const auto margin = content->GetSpecifiedLayout().margin;
		desired = content->Measure(cui::core::Constraints{
			cui::core::Size{},
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width
					- contentOffset - margin.Horizontal()),
				(std::max)(0.0f, inner.maximum.height
					- margin.Vertical()) } });
		desired.width += margin.Horizontal();
		desired.height += margin.Vertical();
	}
	return {
		contentOffset + desired.width + padding.Horizontal(),
		(std::max)(desired.height, circle + 2.0f) + padding.Vertical() };
}

void RadioButton::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	if (GetControlTemplateRoot())
	{
		ContentControl::PerformPendingLayout();
		return;
	}
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content)
	{
		const auto size = GetActualSizeDip();
		const auto padding = GetSpecifiedLayout().padding;
		const float fontHeight = GetRenderFont() ? GetRenderFont()->FontHeight : 14.0f;
		const float circle = (std::max)(14.0f, fontHeight * 0.82f);
		const float offset = circle + TextGap;
		const std::array<Control*, 1> children{ content };
		cui::layout::ArrangeOverlayChildren(children, cui::core::Rect{
			padding.left + offset,
			padding.top,
			(std::max)(0.0f,
				size.width - padding.Horizontal() - offset),
			(std::max)(0.0f, size.height - padding.Vertical()) });
	}
	_contentLayoutPending = false;
}

void RadioButton::OnRender()
{
	if (this->IsVisible == false)return;
	const bool isUnderMouse = this->IsMouseOver;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	float clipW = lastMeasuredWidth > size.width ? lastMeasuredWidth : size.width;
	this->BeginRender(clipW, size.height);
	{
		auto font = this->GetRenderFont();
		const float progress = CurrentSelectionProgress();
		const float circle = (std::max)(14.0f, font->FontHeight * 0.82f);
		const float radius = circle * 0.5f;
		const float cx = radius + 0.5f;
		const float cy = size.height * 0.5f;
		const auto backColor = LerpColor(CircleBackColor, WithAlpha(SelectedColor, 0.18f), progress);
		const auto borderColor = LerpColor(CircleBorderColor, SelectedColor, progress);

		d2d->FillEllipse(cx, cy, radius, radius, backColor);
		if (isUnderMouse && UnderMouseColor.a > 0.0f)
			d2d->FillEllipse(cx, cy, radius, radius, UnderMouseColor);
		const float border = BorderThickness.MaxEdge() > 0.0f
			? BorderThickness.MaxEdge() : 1.5f;
		if (borderColor.a > 0.0f)
			d2d->DrawEllipse(cx, cy, radius, radius, borderColor, border);

		if (progress > 0.001f)
		{
			const float dotRadius = radius * (0.24f + 0.22f * progress);
			d2d->FillEllipse(cx, cy, dotRadius, dotRadius, WithAlpha(SelectedColor, progress));
			const float innerDot = dotRadius * 0.52f;
			d2d->FillEllipse(cx, cy, innerDot, innerDot, WithAlpha(DotColor, progress));
		}

	}
	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, clipW, size.height, DisabledOverlayColor, 4.0f);
	}
	this->EndRender();
	lastMeasuredWidth = size.width;
}

void RadioButton::BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress)
{
	(void)e;
	if (button == MouseButton::Left && hasMatchingPress && this->IsChecked == false)
		StartSelectionAnimation(true);
}

bool RadioButton::Invoke()
{
	if (!IsEnabled || !IsVisible) return false;
	if (!IsChecked)
		StartSelectionAnimation(true);
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	InvalidateVisual();
	return true;
}

void RadioButton::SetChecked(bool checked)
{
	if (IsChecked == checked) return;
	StartSelectionAnimation(checked);
	InvalidateVisual();
}

GET_CPP(RadioButton, std::wstring, GroupName)
{
	return _groupName;
}

SET_CPP(RadioButton, std::wstring, GroupName)
{
	(void)SetPropertyField(L"GroupName", _groupName, std::move(value));
}
