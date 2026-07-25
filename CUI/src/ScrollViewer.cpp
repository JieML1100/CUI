#define NOMINMAX
#include "ScrollViewer.h"
#include "Window.h"
#include "Layout/OverlayLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<ScrollViewer, TValue> ScrollViewerPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DependencyPropertyEditorKind editor,
		DependencyPropertyFlags flags = DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<ScrollViewer, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto ScrollViewerPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			ScrollViewer& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName)
						handler();
				});
		};
	}

	bool CanScroll(ScrollBarVisibility visibility) noexcept
	{
		return visibility != ScrollBarVisibility::Disabled;
	}

	bool MustShowScrollBar(ScrollBarVisibility visibility) noexcept
	{
		return visibility == ScrollBarVisibility::Visible;
	}

	bool ShouldShowScrollBar(
		ScrollBarVisibility visibility, float extent, float viewport) noexcept
	{
		return visibility == ScrollBarVisibility::Visible
			|| (visibility == ScrollBarVisibility::Auto && extent > viewport);
	}
}

UIClass ScrollViewer::Type() { return UIClass::UI_ScrollViewer; }

void ScrollViewer::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		const auto layoutFlags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		auto visibilityOptions = ScrollViewerPropertyOptions(
			static_cast<int>(ScrollBarVisibility::Auto),
			L"Behavior", 110, 10,
			DependencyPropertyEditorKind::Choice, layoutFlags);
		visibilityOptions.Coerce = [](
			ScrollViewer&, const int& proposed) -> std::optional<int>
		{
			switch (static_cast<ScrollBarVisibility>(proposed))
			{
			case ScrollBarVisibility::Disabled:
			case ScrollBarVisibility::Auto:
			case ScrollBarVisibility::Hidden:
			case ScrollBarVisibility::Visible:
				return proposed;
			default:
				return std::nullopt;
			}
		};
		visibilityOptions.Design.Choices = {
			{ L"Disabled", BindingValue(static_cast<int>(ScrollBarVisibility::Disabled)) },
			{ L"Auto", BindingValue(static_cast<int>(ScrollBarVisibility::Auto)) },
			{ L"Hidden", BindingValue(static_cast<int>(ScrollBarVisibility::Hidden)) },
			{ L"Visible", BindingValue(static_cast<int>(ScrollBarVisibility::Visible)) },
		};
		DependencyPropertyRegistry::Register<ScrollViewer, int>(
			L"HorizontalScrollBarVisibility",
			[](ScrollViewer& target)
			{ return static_cast<int>(target.HorizontalScrollBarVisibility); },
			[](ScrollViewer& target, const int& value)
			{ target.HorizontalScrollBarVisibility = static_cast<ScrollBarVisibility>(value); },
			ScrollViewerPropertySubscriber(L"HorizontalScrollBarVisibility"),
			visibilityOptions);
		visibilityOptions.Design.Order = 20;
		DependencyPropertyRegistry::Register<ScrollViewer, int>(
			L"VerticalScrollBarVisibility",
			[](ScrollViewer& target)
			{ return static_cast<int>(target.VerticalScrollBarVisibility); },
			[](ScrollViewer& target, const int& value)
			{ target.VerticalScrollBarVisibility = static_cast<ScrollBarVisibility>(value); },
			ScrollViewerPropertySubscriber(L"VerticalScrollBarVisibility"),
			std::move(visibilityOptions));

		auto registerReadOnly = [](
			const wchar_t* name,
			auto getter,
			auto setter)
		{
			DependencyPropertyOptions<ScrollViewer, double> options;
			options.DefaultValue = 0.0;
			options.IsReadOnly = true;
			options.Design.Browsable = false;
			options.Design.Persistence =
				DependencyPropertyPersistence::Transient;
			DependencyPropertyRegistry::Register<ScrollViewer, double>(
				name, std::move(getter), std::move(setter),
				ScrollViewerPropertySubscriber(name), std::move(options));
		};
		registerReadOnly(L"ExtentWidth",
			[](ScrollViewer& target) { return target._extentWidth; },
			[](ScrollViewer& target, const double& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"ExtentWidth", target._extentWidth, value);
			});
		registerReadOnly(L"ExtentHeight",
			[](ScrollViewer& target) { return target._extentHeight; },
			[](ScrollViewer& target, const double& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"ExtentHeight", target._extentHeight, value);
			});
		registerReadOnly(L"ViewportWidth",
			[](ScrollViewer& target) { return target._viewportWidth; },
			[](ScrollViewer& target, const double& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"ViewportWidth", target._viewportWidth, value);
			});
		registerReadOnly(L"ViewportHeight",
			[](ScrollViewer& target) { return target._viewportHeight; },
			[](ScrollViewer& target, const double& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"ViewportHeight", target._viewportHeight, value);
			});
		registerReadOnly(L"HorizontalOffset",
			[](ScrollViewer& target) { return target._horizontalOffset; },
			[](ScrollViewer& target, const double& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"HorizontalOffset", target._horizontalOffset, value);
			});
		registerReadOnly(L"VerticalOffset",
			[](ScrollViewer& target) { return target._verticalOffset; },
			[](ScrollViewer& target, const double& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"VerticalOffset", target._verticalOffset, value);
			});
		return true;
	}();
	(void)registered;
}

void ScrollViewer::PerformPendingLayout()
{
	if (!IsLayoutSuspended() && _contentLayoutPending)
		PerformScrollContentLayout();
}

cui::core::Size ScrollViewer::MeasureCore(
	const cui::core::Constraints& available)
{
	const auto padding = GetSpecifiedLayout().padding;
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Constraints::Unbounded(), padding);
}

bool ScrollViewer::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Left:
	case Key::Right:
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
		return true;
	default:
		return false;
	}
}

bool ScrollViewer::CanHandleMouseWheel(int delta, int localX, int localY)
{
	if (delta == 0) return false;
	PerformPendingLayout();

	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);
	if (localX < 0 || localY < 0 || localX >= this->ActualWidth || localY >= this->ActualHeight)
		return false;
	if (!layout.HasVerticalScroll || layout.MaxScrollY <= 0.0f)
		return false;
	return delta > 0
		? this->_verticalOffset > 0.0
		: this->_verticalOffset < layout.MaxScrollY;
}

ScrollViewer::ScrollViewer()
	: ContentControl()
{
}

GET_CPP(ScrollViewer, ScrollBarVisibility, HorizontalScrollBarVisibility)
{
	return _horizontalScrollBarVisibility;
}

SET_CPP(ScrollViewer, ScrollBarVisibility, HorizontalScrollBarVisibility)
{
	(void)SetPropertyField(L"HorizontalScrollBarVisibility",
		_horizontalScrollBarVisibility, value);
}

GET_CPP(ScrollViewer, ScrollBarVisibility, VerticalScrollBarVisibility)
{
	return _verticalScrollBarVisibility;
}

SET_CPP(ScrollViewer, ScrollBarVisibility, VerticalScrollBarVisibility)
{
	(void)SetPropertyField(L"VerticalScrollBarVisibility",
		_verticalScrollBarVisibility, value);
}

GET_CPP(ScrollViewer, double, ExtentWidth) { return _extentWidth; }
GET_CPP(ScrollViewer, double, ExtentHeight) { return _extentHeight; }
GET_CPP(ScrollViewer, double, ViewportWidth) { return _viewportWidth; }
GET_CPP(ScrollViewer, double, ViewportHeight) { return _viewportHeight; }
GET_CPP(ScrollViewer, double, HorizontalOffset) { return _horizontalOffset; }
GET_CPP(ScrollViewer, double, VerticalOffset) { return _verticalOffset; }

cui::core::Point ScrollViewer::GetVisualChildrenRenderOffset() const
{
	return {
		-static_cast<float>(_horizontalOffset),
		-static_cast<float>(_verticalOffset) };
}

D2D1_RECT_F ScrollViewer::GetVisualChildrenClipRect()
{
	auto layout = this->CalcScrollLayout();
	return D2D1_RECT_F{ 0.0f, 0.0f, layout.ViewportWidth, layout.ViewportHeight };
}

void ScrollViewer::PerformScrollContentLayout()
{
	const float scrollBarThickness = _scrollBarThickness;
	const auto viewportSize = this->GetActualSizeDip();
	auto performLayoutPass = [&](bool reserveVerticalScrollBar, bool reserveHorizontalScrollBar)
		{
			const auto padding = GetSpecifiedLayout().padding;
			const float viewportWidth = (std::max)(0.0f,
				viewportSize.width - padding.Horizontal()
				- (reserveVerticalScrollBar ? scrollBarThickness : 0.0f));
			const float viewportHeight = (std::max)(0.0f,
				viewportSize.height - padding.Vertical()
				- (reserveHorizontalScrollBar ? scrollBarThickness : 0.0f));
			const auto requested = MeasureContentSizeDip();
			cui::layout::ArrangeOverlayChildren(
				GetLayoutChildrenView(),
				cui::core::Rect{
					padding.left,
					padding.top,
					(std::max)(viewportWidth,
						requested.width - padding.Horizontal()),
					(std::max)(viewportHeight,
						requested.height - padding.Vertical()) });
		};

	bool needsVerticalScroll =
		MustShowScrollBar(_verticalScrollBarVisibility);
	bool needsHorizontalScroll =
		MustShowScrollBar(_horizontalScrollBarVisibility);
	for (int iter = 0; iter < 3; ++iter)
	{
		performLayoutPass(needsVerticalScroll, needsHorizontalScroll);

		cui::core::Size content = MeasureContentSizeDip();
		content = content.NonNegative();

		float viewportWidth = std::max(0.0f, viewportSize.width - (needsVerticalScroll ? scrollBarThickness : 0.0f));
		float viewportHeight = std::max(0.0f, viewportSize.height - (needsHorizontalScroll ? scrollBarThickness : 0.0f));
		bool nextNeedsHorizontalScroll = ShouldShowScrollBar(
			_horizontalScrollBarVisibility, content.width, viewportWidth);
		bool nextNeedsVerticalScroll = ShouldShowScrollBar(
			_verticalScrollBarVisibility, content.height, viewportHeight);
		if (nextNeedsHorizontalScroll == needsHorizontalScroll && nextNeedsVerticalScroll == needsVerticalScroll)
		{
			break;
		}

		needsHorizontalScroll = nextNeedsHorizontalScroll;
		needsVerticalScroll = nextNeedsVerticalScroll;
	}

	_contentLayoutPending = false;
	auto layout = CalcScrollLayout();
	ClampScrollOffsets(layout);
}

cui::core::Size ScrollViewer::MeasureContentSizeDip()
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Constraints::Unbounded(),
		GetSpecifiedLayout().padding);
}

ScrollViewer::ScrollLayout ScrollViewer::CalcScrollLayout()
{
	ScrollLayout layout{};
	layout.ScrollBarThickness = _scrollBarThickness;

	cui::core::Size content = MeasureContentSizeDip();
	content = content.NonNegative();
	const auto viewportSize = this->GetActualSizeDip();

	bool needsVerticalScroll =
		MustShowScrollBar(_verticalScrollBarVisibility);
	bool needsHorizontalScroll =
		MustShowScrollBar(_horizontalScrollBarVisibility);
	for (int iter = 0; iter < 3; ++iter)
	{
		float viewportWidth = viewportSize.width - (needsVerticalScroll ? layout.ScrollBarThickness : 0.0f);
		float viewportHeight = viewportSize.height - (needsHorizontalScroll ? layout.ScrollBarThickness : 0.0f);
		if (viewportWidth < 0.0f) viewportWidth = 0.0f;
		if (viewportHeight < 0.0f) viewportHeight = 0.0f;

		bool nextNeedsHorizontalScroll = ShouldShowScrollBar(
			_horizontalScrollBarVisibility, content.width, viewportWidth);
		bool nextNeedsVerticalScroll = ShouldShowScrollBar(
			_verticalScrollBarVisibility, content.height, viewportHeight);
		if (nextNeedsHorizontalScroll == needsHorizontalScroll && nextNeedsVerticalScroll == needsVerticalScroll)
		{
			layout.HasHorizontalScroll = needsHorizontalScroll;
			layout.HasVerticalScroll = needsVerticalScroll;
			layout.ViewportWidth = viewportWidth;
			layout.ViewportHeight = viewportHeight;
			layout.ContentWidth = content.width;
			layout.ContentHeight = content.height;
			layout.MaxScrollX = CanScroll(_horizontalScrollBarVisibility)
				? std::max(0.0f, layout.ContentWidth - viewportWidth) : 0.0f;
			layout.MaxScrollY = CanScroll(_verticalScrollBarVisibility)
				? std::max(0.0f, layout.ContentHeight - viewportHeight) : 0.0f;
			return layout;
		}
		needsHorizontalScroll = nextNeedsHorizontalScroll;
		needsVerticalScroll = nextNeedsVerticalScroll;
	}

	layout.HasHorizontalScroll = needsHorizontalScroll;
	layout.HasVerticalScroll = needsVerticalScroll;
	layout.ViewportWidth = std::max(0.0f, viewportSize.width - (needsVerticalScroll ? layout.ScrollBarThickness : 0.0f));
	layout.ViewportHeight = std::max(0.0f, viewportSize.height - (needsHorizontalScroll ? layout.ScrollBarThickness : 0.0f));
	layout.ContentWidth = content.width;
	layout.ContentHeight = content.height;
	layout.MaxScrollX = CanScroll(_horizontalScrollBarVisibility)
		? std::max(0.0f, layout.ContentWidth - layout.ViewportWidth) : 0.0f;
	layout.MaxScrollY = CanScroll(_verticalScrollBarVisibility)
		? std::max(0.0f, layout.ContentHeight - layout.ViewportHeight) : 0.0f;
	return layout;
}

void ScrollViewer::ClampScrollOffsets(const ScrollLayout& layout)
{
	PublishScrollState(layout, _horizontalOffset, _verticalOffset);
}

void ScrollViewer::PublishScrollState(
	const ScrollLayout& layout,
	double horizontalOffset,
	double verticalOffset)
{
	const double oldHorizontalOffset = _horizontalOffset;
	const double oldVerticalOffset = _verticalOffset;
	const double oldExtentWidth = _extentWidth;
	const double oldExtentHeight = _extentHeight;
	const double oldViewportWidth = _viewportWidth;
	const double oldViewportHeight = _viewportHeight;
	const double nextHorizontalOffset = std::clamp(
		horizontalOffset, 0.0, static_cast<double>(layout.MaxScrollX));
	const double nextVerticalOffset = std::clamp(
		verticalOffset, 0.0, static_cast<double>(layout.MaxScrollY));

	auto update = [this](const wchar_t* name, double oldValue, double value)
	{
		if (std::fabs(oldValue - value) <= 0.000001) return false;
		return TrySetReadOnlyPropertyValue(name, BindingValue(value));
	};
	const bool changed =
		update(L"ExtentWidth", oldExtentWidth, layout.ContentWidth)
		| update(L"ExtentHeight", oldExtentHeight, layout.ContentHeight)
		| update(L"ViewportWidth", oldViewportWidth, layout.ViewportWidth)
		| update(L"ViewportHeight", oldViewportHeight, layout.ViewportHeight)
		| update(L"HorizontalOffset", oldHorizontalOffset, nextHorizontalOffset)
		| update(L"VerticalOffset", oldVerticalOffset, nextVerticalOffset);
	if (!changed) return;

	ScrollChangedEventArgs args;
	args.HorizontalOffset = _horizontalOffset;
	args.HorizontalChange = _horizontalOffset - oldHorizontalOffset;
	args.VerticalOffset = _verticalOffset;
	args.VerticalChange = _verticalOffset - oldVerticalOffset;
	args.ExtentWidth = _extentWidth;
	args.ExtentWidthChange = _extentWidth - oldExtentWidth;
	args.ExtentHeight = _extentHeight;
	args.ExtentHeightChange = _extentHeight - oldExtentHeight;
	args.ViewportWidth = _viewportWidth;
	args.ViewportWidthChange = _viewportWidth - oldViewportWidth;
	args.ViewportHeight = _viewportHeight;
	args.ViewportHeightChange = _viewportHeight - oldViewportHeight;
	OnScrollChanged(this, args);
}

void ScrollViewer::SetScrollOffsetCore(
	double horizontalOffset, double verticalOffset)
{
	auto layout = this->CalcScrollLayout();
	const double oldX = _horizontalOffset;
	const double oldY = _verticalOffset;
	PublishScrollState(layout, horizontalOffset, verticalOffset);
	if (std::fabs(oldX - _horizontalOffset) > 0.000001
		|| std::fabs(oldY - _verticalOffset) > 0.000001)
	{
		// Scroll offset is an ancestor render transform. The viewport damage is
		// local, but every descendant's cached bounds and command transform must
		// be recomputed before replay.
		InvalidateDescendantRenderGeometry();
		InvalidateVisual();
	}
}

void ScrollViewer::LineLeft() { SetScrollOffsetCore(_horizontalOffset - _mouseWheelStep, _verticalOffset); }
void ScrollViewer::LineRight() { SetScrollOffsetCore(_horizontalOffset + _mouseWheelStep, _verticalOffset); }
void ScrollViewer::LineUp() { SetScrollOffsetCore(_horizontalOffset, _verticalOffset - _mouseWheelStep); }
void ScrollViewer::LineDown() { SetScrollOffsetCore(_horizontalOffset, _verticalOffset + _mouseWheelStep); }
void ScrollViewer::PageLeft() { SetScrollOffsetCore(_horizontalOffset - (std::max)(16.0, _viewportWidth - _mouseWheelStep), _verticalOffset); }
void ScrollViewer::PageRight() { SetScrollOffsetCore(_horizontalOffset + (std::max)(16.0, _viewportWidth - _mouseWheelStep), _verticalOffset); }
void ScrollViewer::PageUp() { SetScrollOffsetCore(_horizontalOffset, _verticalOffset - (std::max)(16.0, _viewportHeight - _mouseWheelStep)); }
void ScrollViewer::PageDown() { SetScrollOffsetCore(_horizontalOffset, _verticalOffset + (std::max)(16.0, _viewportHeight - _mouseWheelStep)); }
void ScrollViewer::ScrollToHorizontalOffset(double offset) { SetScrollOffsetCore(offset, _verticalOffset); }
void ScrollViewer::ScrollToVerticalOffset(double offset) { SetScrollOffsetCore(_horizontalOffset, offset); }
void ScrollViewer::ScrollToHome() { SetScrollOffsetCore(0.0, 0.0); }
void ScrollViewer::ScrollToEnd()
{
	auto layout = CalcScrollLayout();
	SetScrollOffsetCore(layout.MaxScrollX, layout.MaxScrollY);
}

bool ScrollViewer::BringDescendantIntoView(Control* descendant)
{
	if (!descendant || descendant == this) return false;
	bool owned = false;
	for (auto* current = descendant->GetVisualParent(); current;
		current = current->GetVisualParent())
		if (current == this)
		{
			owned = true;
			break;
		}
	if (!owned) return false;
	PerformPendingLayout();
	std::vector<Control*> layoutPath;
	for (auto* current = descendant->GetVisualParent();
		current && current != this; current = current->GetVisualParent())
		layoutPath.push_back(current);
	for (auto current = layoutPath.rbegin(); current != layoutPath.rend(); ++current)
		(*current)->UpdateLayout();

	const auto layout = CalcScrollLayout();
	const auto owner = GetAbsoluteLocationDip();
	const auto target = descendant->GetAbsoluteRectDip();
	const float left = target.x - owner.x;
	const float top = target.y - owner.y;
	const float right = left + target.width;
	const float bottom = top + target.height;
	double x = _horizontalOffset;
	double y = _verticalOffset;
	if (left < 0.0f) x += static_cast<int>(std::floor(left));
	else if (right > layout.ViewportWidth)
		x += static_cast<int>(std::ceil(right - layout.ViewportWidth));
	if (top < 0.0f) y += static_cast<int>(std::floor(top));
	else if (bottom > layout.ViewportHeight)
		y += static_cast<int>(std::ceil(bottom - layout.ViewportHeight));
	const double oldX = _horizontalOffset;
	const double oldY = _verticalOffset;
	SetScrollOffsetCore(x, y);
	return std::fabs(oldX - _horizontalOffset) > 0.000001
		|| std::fabs(oldY - _verticalOffset) > 0.000001;
}

bool ScrollViewer::HitVerticalScrollBar(int localX, int localY, const ScrollLayout& layout) const
{
	if (!layout.HasVerticalScroll) return false;
	const auto size = const_cast<ScrollViewer*>(this)->GetActualSizeDip();
	return (float)localX >= layout.ViewportWidth && (float)localX < size.width
		&& localY >= 0 && (float)localY < layout.ViewportHeight;
}

bool ScrollViewer::HitHorizontalScrollBar(int localX, int localY, const ScrollLayout& layout) const
{
	if (!layout.HasHorizontalScroll) return false;
	const auto size = const_cast<ScrollViewer*>(this)->GetActualSizeDip();
	return (float)localY >= layout.ViewportHeight && (float)localY < size.height
		&& localX >= 0 && (float)localX < layout.ViewportWidth;
}

CursorKind ScrollViewer::QueryCursor(int localX, int localY)
{
	if (!this->IsEnabled) return CursorKind::Arrow;
	auto layout = this->CalcScrollLayout();
	if (HitVerticalScrollBar(localX, localY, layout)) return CursorKind::SizeNS;
	if (HitHorizontalScrollBar(localX, localY, layout)) return CursorKind::SizeWE;
	return Control::QueryCursor(localX, localY);
}

bool ScrollViewer::ShouldHitTestChildrenAt(int localX, int localY) const
{
	if (!this->HitTestChildren()) return false;
	auto layout = const_cast<ScrollViewer*>(this)->CalcScrollLayout();
	if (localX < 0 || localY < 0) return false;
	if (localX >= (int)layout.ViewportWidth || localY >= (int)layout.ViewportHeight) return false;
	return true;
}

bool ScrollViewer::HitChild(Control* child, int localX, int localY, int& childX, int& childY) const
{
	if (!child || !child->IsVisible || !child->IsEnabled) return false;
	const auto childLocation = child->GetActualLocationDip();
	const auto childSize = child->GetActualSizeDip();
	const float drawX = childLocation.x - static_cast<float>(_horizontalOffset);
	const float drawY = childLocation.y - static_cast<float>(_verticalOffset);
	const cui::core::Rect childRect{ drawX, drawY, childSize.width, childSize.height };
	if (!childRect.Contains(cui::core::Point{ (float)localX, (float)localY }))
		return false;
	childX = static_cast<int>(std::floor((float)localX - drawX));
	childY = static_cast<int>(std::floor((float)localY - drawY));
	return true;
}

void ScrollViewer::DrawScrollBars(const ScrollLayout& layout)
{
	auto d2d = this->GetDrawingContext();
	if (layout.HasVerticalScroll && layout.ViewportHeight > 0.0f && layout.ContentHeight > layout.ViewportHeight)
	{
		float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
		float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
		thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
		float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
		float per = (layout.MaxScrollY > 0.0f) ? std::clamp(static_cast<float>(_verticalOffset) / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
		float thumbTop = per * moveSpace;
		d2d->FillRoundRect(layout.ViewportWidth, 0.0f, layout.ScrollBarThickness, layout.ViewportHeight, _scrollBackColor, 4.0f);
		d2d->FillRoundRect(layout.ViewportWidth, thumbTop, layout.ScrollBarThickness, thumbH, _scrollForeColor, 4.0f);
	}

	if (layout.HasHorizontalScroll && layout.ViewportWidth > 0.0f && layout.ContentWidth > layout.ViewportWidth)
	{
		float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
		float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
		thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
		float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
		float per = (layout.MaxScrollX > 0.0f) ? std::clamp(static_cast<float>(_horizontalOffset) / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
		float thumbLeft = per * moveSpace;
		d2d->FillRoundRect(0.0f, layout.ViewportHeight, layout.ViewportWidth, layout.ScrollBarThickness, _scrollBackColor, 4.0f);
		d2d->FillRoundRect(thumbLeft, layout.ViewportHeight, thumbW, layout.ScrollBarThickness, _scrollForeColor, 4.0f);
	}

	if (layout.HasHorizontalScroll && layout.HasVerticalScroll)
	{
		d2d->FillRect(layout.ViewportWidth, layout.ViewportHeight, layout.ScrollBarThickness, layout.ScrollBarThickness, _scrollBackColor);
	}
}

void ScrollViewer::UpdateVerticalScrollByThumb(float localY, const ScrollLayout& layout)
{
	if (!layout.HasVerticalScroll || layout.ContentHeight <= layout.ViewportHeight || layout.ViewportHeight <= 0.0f)
		return;
	float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
	float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
	thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
	float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
	if (moveSpace <= 0.0f) return;
	float grab = std::clamp(this->_verticalScrollThumbGrabOffset, 0.0f, thumbH);
	if (grab <= 0.0f) grab = thumbH * 0.5f;
	float target = std::clamp(localY - grab, 0.0f, moveSpace);
	float per = target / moveSpace;
	SetScrollOffsetCore(_horizontalOffset, per * layout.MaxScrollY);
}

void ScrollViewer::UpdateHorizontalScrollByThumb(float localX, const ScrollLayout& layout)
{
	if (!layout.HasHorizontalScroll || layout.ContentWidth <= layout.ViewportWidth || layout.ViewportWidth <= 0.0f)
		return;
	float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
	float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
	thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
	float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
	if (moveSpace <= 0.0f) return;
	float grab = std::clamp(this->_horizontalScrollThumbGrabOffset, 0.0f, thumbW);
	if (grab <= 0.0f) grab = thumbW * 0.5f;
	float target = std::clamp(localX - grab, 0.0f, moveSpace);
	float per = target / moveSpace;
	SetScrollOffsetCore(per * layout.MaxScrollX, _verticalOffset);
}

void ScrollViewer::OnRender()
{
	if (this->IsVisible == false) return;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float border = this->BorderThickness.MaxEdge();
	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);

	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->RendererBackgroundColor);

		DrawScrollBars(layout);
		if (border > 0.0f && this->RendererBorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			d2d->DrawRect(border * 0.5f, border * 0.5f,
				drawW, drawH, this->RendererBorderColor, border);
		}
	}
	if (!this->IsEnabled)
		d2d->FillRect(0, 0, actualWidth, actualHeight,
			cui::theme::palette::DisabledOverlay);
	this->EndRender();
}

bool ScrollViewer::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	PerformPendingLayout();

	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);
	if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
	{
		_draggingVerticalScrollBar = false;
		_draggingHorizontalScrollBar = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Control::ProcessInput(input);
	}

	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left && this->GetPresentationWindow())
	{
		this->GetPresentationWindow()->SetKeyboardFocus(this, false);
	}

	if (_draggingVerticalScrollBar
		&& input.Kind == InputReportKind::PointerMove)
	{
		UpdateVerticalScrollByThumb((float)input.Y, layout);
		return true;
	}
	if (_draggingHorizontalScrollBar
		&& input.Kind == InputReportKind::PointerMove)
	{
		UpdateHorizontalScrollByThumb((float)input.X, layout);
		return true;
	}
	if ((_draggingVerticalScrollBar || _draggingHorizontalScrollBar)
		&& input.Kind == InputReportKind::PointerUp)
	{
		_draggingVerticalScrollBar = false;
		_draggingHorizontalScrollBar = false;
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
	}

	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	{
		const int delta = input.WheelDelta;

		if (!this->CanHandleMouseWheel(delta, input.X, input.Y))
			return false;

		int steps = delta / InputReport::WheelDeltaUnit;
		if (steps != 0)
		{
			SetScrollOffsetCore(
				_horizontalOffset,
				_verticalOffset - (steps * _mouseWheelStep));
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseWheel(this, eventArgs);
		return true;
	}
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton != MouseButton::Left) break;
		if (HitVerticalScrollBar(input.X, input.Y, layout)
			&& layout.ContentHeight > layout.ViewportHeight)
		{
			float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
			float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
			thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
			float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
			float scrollRatio = (layout.MaxScrollY > 0.0f) ? std::clamp(static_cast<float>(_verticalOffset) / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
			float thumbTop = scrollRatio * moveSpace;
			float pointerY = (float)input.Y;
			bool hitThumb = pointerY >= thumbTop && pointerY <= (thumbTop + thumbH);
			this->_verticalScrollThumbGrabOffset = hitThumb ? (pointerY - thumbTop) : (thumbH * 0.5f);
			this->_draggingVerticalScrollBar = true;
			(void)CaptureMouse();
			UpdateVerticalScrollByThumb(pointerY, layout);
			return true;
		}
		if (HitHorizontalScrollBar(input.X, input.Y, layout)
			&& layout.ContentWidth > layout.ViewportWidth)
		{
			float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
			float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
			thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
			float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
			float scrollRatio = (layout.MaxScrollX > 0.0f) ? std::clamp(static_cast<float>(_horizontalOffset) / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
			float thumbLeft = scrollRatio * moveSpace;
			float pointerX = (float)input.X;
			bool hitThumb = pointerX >= thumbLeft && pointerX <= (thumbLeft + thumbW);
			this->_horizontalScrollThumbGrabOffset = hitThumb ? (pointerX - thumbLeft) : (thumbW * 0.5f);
			this->_draggingHorizontalScrollBar = true;
			(void)CaptureMouse();
			UpdateHorizontalScrollByThumb(pointerX, layout);
			return true;
		}
	}
	break;
	case InputReportKind::KeyDown:
	{
		const bool ctrlDown = input.HasModifier(ModifierKeys::Control);
		const int lineStepY = std::max(16, _mouseWheelStep);
		const int lineStepX = std::max(16, _mouseWheelStep);
		const int pageStepY = std::max(16, (int)layout.ViewportHeight - lineStepY);
		const int pageStepX = std::max(16, (int)layout.ViewportWidth - lineStepX);
		bool handledScroll = false;

		switch (input.Key)
		{
		case Key::Up:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset, _verticalOffset - lineStepY);
				handledScroll = true;
			}
			break;
		case Key::Down:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset, _verticalOffset + lineStepY);
				handledScroll = true;
			}
			break;
		case Key::Left:
			if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset - lineStepX, _verticalOffset);
				handledScroll = true;
			}
			break;
		case Key::Right:
			if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset + lineStepX, _verticalOffset);
				handledScroll = true;
			}
			break;
		case Key::PageUp:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset, _verticalOffset - pageStepY);
				handledScroll = true;
			}
			break;
		case Key::PageDown:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset, _verticalOffset + pageStepY);
				handledScroll = true;
			}
			break;
		case Key::Home:
			if (ctrlDown)
			{
				if (layout.MaxScrollX > 0.0f || layout.MaxScrollY > 0.0f)
				{
					SetScrollOffsetCore(0.0, 0.0);
					handledScroll = true;
				}
			}
			else if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset, 0.0);
				handledScroll = true;
			}
			else if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffsetCore(0.0, _verticalOffset);
				handledScroll = true;
			}
			break;
		case Key::End:
			if (ctrlDown)
			{
				if (layout.MaxScrollX > 0.0f || layout.MaxScrollY > 0.0f)
				{
					SetScrollOffsetCore(layout.MaxScrollX, layout.MaxScrollY);
					handledScroll = true;
				}
			}
			else if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffsetCore(_horizontalOffset, layout.MaxScrollY);
				handledScroll = true;
			}
			else if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffsetCore(layout.MaxScrollX, _verticalOffset);
				handledScroll = true;
			}
			break;
		}

		if (handledScroll)
		{
			auto eventArgs = input.CreateKeyEventArgs();
			this->OnKeyDown(this, eventArgs);
			return true;
		}
	}
	break;
	}

	return Control::ProcessInput(input);
}
