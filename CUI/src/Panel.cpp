#pragma once
#include "Panel.h"
#include "Window.h"
#include "Layout/CanvasLayout.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	std::optional<cui::drawing::Brush> ConvertPanelBrush(
		const BindingValue& value)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) return brush;
		D2D1_COLOR_F color{};
		if (value.TryGet(color))
			return cui::drawing::MakeSolidColorBrush(color);
		return std::nullopt;
	}
}

UIClass Panel::Type() { return UIClass::UI_Panel; }

const DependencyProperty& Panel::BackgroundProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<Panel, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertPanelBrush;
		options.Design.Browsable = false;
		options.Design.DisplayName = L"Background";
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		return DependencyPropertyRegistry::Register<
			Panel, cui::drawing::Brush>(
				L"Background", std::move(options));
	}();
	return *property;
}

void Panel::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	(void)BackgroundProperty();
}

Panel::Panel()
{
}

Panel::~Panel() = default;

void Panel::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	PerformPendingLayout();
}

cui::core::Size Panel::MeasureCore(const cui::core::Constraints& available)
{
	cui::core::Size contentDesired{};

	if (_layoutEngine)
	{
		LayoutContext context(this);
		contentDesired = _layoutEngine->Measure(context, available);
	}
	else
	{
		for (auto* child : this->GetVisualChildrenView())
		{
			if (!child || child->IsCollapsed()) continue;
			(void)cui::layout::MeasureCanvasChild(*child);
		}
	}
	_needsMeasure = false;
	_needsArrange = true;

	return contentDesired;
}

void Panel::SetLayoutEngine(class LayoutEngine* engine)
{
	if (_layoutEngine.get() == engine)
	{
		InvalidateLayout();
		return;
	}
	_layoutEngine.reset(engine);
	InvalidateLayout();
}

void Panel::RequestLayout()
{
	InvalidateLayout();
}

void Panel::OnComputedLayoutSizeChanged()
{
	// A new arrange slot changes the space offered to descendants, but it does
	// not invalidate this panel's own measure result.  Descendant arrange is
	// completed synchronously by Panel::Arrange, keeping layout independent of
	// the later render/update pass.
	_needsArrange = true;
	if (_layoutEngine)
		_layoutEngine->Invalidate();
}

void Panel::PerformPendingLayout()
{
	if (this->IsLayoutSuspended()) return;
	if (auto* templateRoot = GetControlTemplateRoot())
	{
		// Control::Arrange owns the template root's complete slot.  Running the
		// Panel policy over that infrastructure child a second time makes a
		// templated Canvas shrink the root back to its intrinsic desired size.
		// ComponentDefinition may legally use a Panel base, so keep the native
		// panel policy dormant while the XAML template owns presentation.
		templateRoot->UpdateLayout();
		_needsMeasure = false;
		_needsArrange = false;
		return;
	}
	if (_needsMeasure || _needsArrange
		|| (_layoutEngine && _layoutEngine->NeedsLayout()))
		PerformLayout();
}

void Panel::InvalidateLayout()
{
	_needsMeasure = true;
	_needsArrange = true;
	if (_layoutEngine)
	{
		_layoutEngine->Invalidate();
	}

	// A child's desired geometry can affect every auto-sized ancestor. Bubble
	// the request to the root and ensure the window schedules a new frame.
	Control::RequestLayout();
}

void Panel::InvalidateArrangeLayout()
{
	_needsArrange = true;
	if (_layoutEngine)
		_layoutEngine->Invalidate();

	// Preserve the measure result while scheduling the parent policy that
	// assigns child slots.
	Control::RequestArrange();
}

void Panel::PerformLayout()
{
	const bool measure = _needsMeasure;
	const bool arrange = _needsArrange || measure
		|| (_layoutEngine && _layoutEngine->NeedsLayout());
	if (!arrange) return;

	if (!_layoutEngine)
	{
		// A plain Panel uses the Canvas policy: children are measured unbounded,
		// do not contribute to desired size, and use attached edge offsets.
		const auto containerSize = this->GetActualSizeDip();
		const cui::core::Rect contentRect{
			0.0f,
			0.0f,
			containerSize.width,
			containerSize.height
		};
		for (auto* child : this->GetVisualChildrenView())
		{
			if (!child || child->IsCollapsed()) continue;
			(void)cui::layout::ArrangeCanvasChild(*child, contentRect);
		}
	}
	else
	{
		LayoutContext context(this);
		const auto actualSize = this->GetActualSizeDip();
		if (measure)
		{
			const cui::core::Constraints availableSize{ actualSize };
			_layoutEngine->Measure(context, availableSize);
		}
		const cui::core::Rect finalRect{
			0.0f, 0.0f, actualSize.width, actualSize.height };
		_layoutEngine->Arrange(context, finalRect);
	}

	_needsMeasure = false;
	_needsArrange = false;
}

void Panel::OnRender()
{
	if (this->IsVisible == false) return;

	auto d2d = this->GetDrawingContext();
	auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	{
		if (auto* background = CreateBackgroundBrush(
			*d2d, D2D1_SIZE_F{ actualWidth, actualHeight }))
		{
			d2d->FillRect(0, 0, actualWidth, actualHeight, background);
			background->Release();
		}
	}
	this->EndRender();
}
