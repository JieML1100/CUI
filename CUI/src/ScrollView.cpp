#define NOMINMAX
#include "ScrollView.h"
#include "Form.h"
#include "Layout/LegacyCanvasAdapter.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<ScrollView, TValue> ScrollViewPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<ScrollView, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto ScrollViewPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			ScrollView& target,
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

	ControlPropertyOptions<ScrollView, float> ScrollViewMetricOptions(
		float defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		auto options = ScrollViewPropertyOptions(
			defaultValue, category, categoryOrder, order,
			ControlPropertyEditorKind::Number, flags);
		options.Coerce = [](
			ScrollView&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	bool ScrollViewColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<ScrollView, D2D1_COLOR_F> ScrollViewColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ScrollViewPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = ScrollViewColorsEqual;
		return options;
	}
}

UIClass ScrollView::Type() { return UIClass::UI_ScrollView; }

void ScrollView::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		const auto layoutFlags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsRender;
		auto autoContentOptions = ScrollViewPropertyOptions(
			true, L"Layout", 100, 180,
			ControlPropertyEditorKind::Boolean, layoutFlags);
		BindingPropertyRegistry::Register<ScrollView, bool>(L"AutoContentSize",
			[](ScrollView& target) { return target.AutoContentSize; },
			[](ScrollView& target, const bool& value) { target.AutoContentSize = value; },
			ScrollViewPropertySubscriber(L"AutoContentSize"),
			std::move(autoContentOptions));

		auto contentSizeOptions = ScrollViewPropertyOptions(
			SIZE{ 0, 0 }, L"Layout", 100, 190,
			ControlPropertyEditorKind::Size, layoutFlags);
		contentSizeOptions.Coerce = [](
			ScrollView&, const SIZE& proposed) -> std::optional<SIZE>
		{
			return SIZE{
				(std::max)(0L, proposed.cx),
				(std::max)(0L, proposed.cy) };
		};
		contentSizeOptions.Equals = [](
			const SIZE& left, const SIZE& right)
		{
			return left.cx == right.cx && left.cy == right.cy;
		};
		BindingPropertyRegistry::Register<ScrollView, SIZE>(L"ContentSize",
			[](ScrollView& target) { return target.ContentSize; },
			[](ScrollView& target, const SIZE& value) { target.ContentSize = value; },
			ScrollViewPropertySubscriber(L"ContentSize"),
			std::move(contentSizeOptions));

		BindingPropertyRegistry::Register<ScrollView, bool>(L"AlwaysShowVScroll",
			[](ScrollView& target) { return target.AlwaysShowVScroll; },
			[](ScrollView& target, const bool& value) { target.AlwaysShowVScroll = value; },
			ScrollViewPropertySubscriber(L"AlwaysShowVScroll"),
			ScrollViewPropertyOptions(false, L"Layout", 100, 200,
				ControlPropertyEditorKind::Boolean, layoutFlags));
		BindingPropertyRegistry::Register<ScrollView, bool>(L"AlwaysShowHScroll",
			[](ScrollView& target) { return target.AlwaysShowHScroll; },
			[](ScrollView& target, const bool& value) { target.AlwaysShowHScroll = value; },
			ScrollViewPropertySubscriber(L"AlwaysShowHScroll"),
			ScrollViewPropertyOptions(false, L"Layout", 100, 210,
				ControlPropertyEditorKind::Boolean, layoutFlags));
		BindingPropertyRegistry::Register<ScrollView, float>(L"ScrollBarThickness",
			[](ScrollView& target) { return target.ScrollBarThickness; },
			[](ScrollView& target, const float& value) { target.ScrollBarThickness = value; },
			ScrollViewPropertySubscriber(L"ScrollBarThickness"),
			ScrollViewMetricOptions(8.0f, L"Layout", 100, 220, layoutFlags));

		auto wheelOptions = ScrollViewPropertyOptions(
			48, L"Behavior", 110, 10,
			ControlPropertyEditorKind::Number, ControlPropertyFlags::None);
		wheelOptions.Coerce = [](
			ScrollView&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(0, proposed);
		};
		wheelOptions.Design.Minimum = 0.0;
		wheelOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<ScrollView, int>(L"MouseWheelStep",
			[](ScrollView& target) { return target.MouseWheelStep; },
			[](ScrollView& target, const int& value) { target.MouseWheelStep = value; },
			ScrollViewPropertySubscriber(L"MouseWheelStep"),
			std::move(wheelOptions));

		BindingPropertyRegistry::Register<ScrollView, D2D1_COLOR_F>(L"ScrollBackColor",
			[](ScrollView& target) { return target.ScrollBackColor; },
			[](ScrollView& target, const D2D1_COLOR_F& value) { target.ScrollBackColor = value; },
			ScrollViewPropertySubscriber(L"ScrollBackColor"),
			ScrollViewColorOptions(cui::theme::palette::ScrollTrack, 20));
		BindingPropertyRegistry::Register<ScrollView, D2D1_COLOR_F>(L"ScrollForeColor",
			[](ScrollView& target) { return target.ScrollForeColor; },
			[](ScrollView& target, const D2D1_COLOR_F& value) { target.ScrollForeColor = value; },
			ScrollViewPropertySubscriber(L"ScrollForeColor"),
			ScrollViewColorOptions(cui::theme::palette::ScrollThumb, 30));

		using Handler = BindingPropertyMetadata::ChangeHandler;
		auto subscriber = [](ScrollView& target, Handler handler, DataSourceUpdateMode)
		{
			return target.OnScrollChanged.Subscribe(
				[handler = std::move(handler)](Control*) { handler(); });
		};
		ControlPropertyOptions<ScrollView, int> offsetOptions;
		offsetOptions.DefaultValue = 0;
		offsetOptions.Design.Browsable = false;
		offsetOptions.Design.Category = L"Behavior";
		offsetOptions.Design.CategoryOrder = 300;
		offsetOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<ScrollView, int>(L"ScrollXOffset",
			[](ScrollView& target) { return target.ScrollXOffset; },
			[](ScrollView& target, const int& value)
			{
				target.SetScrollOffset(value, target.ScrollYOffset);
			}, subscriber, offsetOptions);
		BindingPropertyRegistry::Register<ScrollView, int>(L"ScrollYOffset",
			[](ScrollView& target) { return target.ScrollYOffset; },
			[](ScrollView& target, const int& value)
			{
				target.SetScrollOffset(target.ScrollXOffset, value);
			}, subscriber, std::move(offsetOptions));
		return true;
	}();
	(void)registered;
}

void ScrollView::PerformPendingLayout()
{
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
		PerformScrollContentLayout();
}

bool ScrollView::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}

bool ScrollView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	if (delta == 0) return false;
	if (!this->IsLayoutSuspended() &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
	{
		PerformScrollContentLayout();
	}

	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);
	if (localX < 0 || localY < 0 || localX >= this->Width || localY >= this->Height)
		return false;
	if (!layout.HasVerticalScroll || layout.MaxScrollY <= 0.0f)
		return false;
	return delta > 0
		? this->ScrollYOffset > 0
		: this->ScrollYOffset < (int)std::ceil(layout.MaxScrollY);
}

static bool FindDeepestWheelTarget(Control* root, int localX, int localY, Control*& outTarget, int& outX, int& outY)
{
	if (!root || !root->Visible || !root->Enable) return false;
	if (!root->ShouldHitTestChildrenAt(localX, localY))
	{
		outTarget = root;
		outX = localX;
		outY = localY;
		return true;
	}

	auto childOffset = root->GetChildrenRenderOffset();
	for (auto child : root->GetChildrenInReverseZOrder())
	{
		if (!child || !child->Visible || !child->Enable) continue;
		const auto childLocation = child->GetActualLocationDip();
		const auto childSize = child->GetActualSizeDip();
		const float drawX = childLocation.x + (float)childOffset.x;
		const float drawY = childLocation.y + (float)childOffset.y;
		const cui::core::Rect childRect{ drawX, drawY, childSize.width, childSize.height };
		if (!childRect.Contains(cui::core::Point{ (float)localX, (float)localY }))
			continue;
		if (FindDeepestWheelTarget(
			child,
			static_cast<int>(std::floor((float)localX - drawX)),
			static_cast<int>(std::floor((float)localY - drawY)),
			outTarget, outX, outY))
			return true;
	}

	outTarget = root;
	outX = localX;
	outY = localY;
	return true;
}

ScrollView::ScrollView()
	: Panel()
{
}

ScrollView::ScrollView(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
}

#define CUI_SCROLL_VIEW_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(ScrollView, type, name) { return field; } \
	SET_CPP(ScrollView, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_SCROLL_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ScrollBackColor, _scrollBackColor, L"ScrollBackColor")
CUI_SCROLL_VIEW_PROPERTY_IMPL(D2D1_COLOR_F, ScrollForeColor, _scrollForeColor, L"ScrollForeColor")
CUI_SCROLL_VIEW_PROPERTY_IMPL(float, ScrollBarThickness, _scrollBarThickness, L"ScrollBarThickness")
CUI_SCROLL_VIEW_PROPERTY_IMPL(bool, AlwaysShowVScroll, _alwaysShowVScroll, L"AlwaysShowVScroll")
CUI_SCROLL_VIEW_PROPERTY_IMPL(bool, AlwaysShowHScroll, _alwaysShowHScroll, L"AlwaysShowHScroll")
CUI_SCROLL_VIEW_PROPERTY_IMPL(bool, AutoContentSize, _autoContentSize, L"AutoContentSize")
CUI_SCROLL_VIEW_PROPERTY_IMPL(SIZE, ContentSize, _contentSize, L"ContentSize")
CUI_SCROLL_VIEW_PROPERTY_IMPL(int, MouseWheelStep, _mouseWheelStep, L"MouseWheelStep")

#undef CUI_SCROLL_VIEW_PROPERTY_IMPL

POINT ScrollView::GetChildrenRenderOffset() const
{
	return POINT{ -this->ScrollXOffset, -this->ScrollYOffset };
}

D2D1_RECT_F ScrollView::GetChildrenClipRect()
{
	auto layout = this->CalcScrollLayout();
	return D2D1_RECT_F{ 0.0f, 0.0f, layout.ViewportWidth, layout.ViewportHeight };
}

void ScrollView::PerformScrollContentLayout()
{
	const float scrollBarThickness = ScrollBarThickness;
	const auto viewportSize = this->GetActualSizeDip();
	auto performLayoutPass = [&](bool reserveVerticalScrollBar, bool reserveHorizontalScrollBar)
		{
			Thickness padding = this->Padding;
			float contentLeft = padding.Left;
			float contentTop = padding.Top;
			float contentWidth = viewportSize.width - padding.Left - padding.Right - (reserveVerticalScrollBar ? scrollBarThickness : 0.0f);
			float contentHeight = viewportSize.height - padding.Top - padding.Bottom - (reserveHorizontalScrollBar ? scrollBarThickness : 0.0f);
			if (contentWidth < 0) contentWidth = 0;
			if (contentHeight < 0) contentHeight = 0;

			if (this->_layoutEngine)
			{
				LayoutContext context(this);
				const cui::core::Constraints availableSize{ cui::core::Size{
					contentWidth, contentHeight } };
				this->_layoutEngine->Measure(context, availableSize);

				D2D1_RECT_F finalRect = {
					padding.Left,
					padding.Top,
					padding.Left + contentWidth,
					padding.Top + contentHeight
				};
				this->_layoutEngine->Arrange(context, finalRect);
				return;
			}

			const cui::core::Rect contentRect{
				contentLeft,
				contentTop,
				contentWidth,
				contentHeight
			};
			for (auto* child : this->Children)
			{
				if (!child || !child->Visible) continue;
				cui::layout::compat::ArrangeLegacyCanvasChild(
					*child, contentRect, cui::core::Constraints::Unbounded());
			}
		};

	bool needsVerticalScroll = this->AlwaysShowVScroll;
	bool needsHorizontalScroll = this->AlwaysShowHScroll;
	for (int iter = 0; iter < 3; ++iter)
	{
		performLayoutPass(needsVerticalScroll, needsHorizontalScroll);

		cui::core::Size content = this->AutoContentSize
			? MeasureContentSizeDip()
			: cui::core::Size{ (float)this->ContentSize.cx, (float)this->ContentSize.cy };
		content = content.NonNegative();

		float viewportWidth = std::max(0.0f, viewportSize.width - (needsVerticalScroll ? scrollBarThickness : 0.0f));
		float viewportHeight = std::max(0.0f, viewportSize.height - (needsHorizontalScroll ? scrollBarThickness : 0.0f));
		bool nextNeedsHorizontalScroll = this->AlwaysShowHScroll || (content.width > viewportWidth);
		bool nextNeedsVerticalScroll = this->AlwaysShowVScroll || (content.height > viewportHeight);
		if (nextNeedsHorizontalScroll == needsHorizontalScroll && nextNeedsVerticalScroll == needsVerticalScroll)
		{
			break;
		}

		needsHorizontalScroll = nextNeedsHorizontalScroll;
		needsVerticalScroll = nextNeedsVerticalScroll;
	}

	this->_needsLayout = false;
}

cui::core::Size ScrollView::MeasureContentSizeDip()
{
	float maxRight = this->_padding.Left;
	float maxBottom = this->_padding.Top;
	for (size_t i = 0; i < this->Children.size(); i++)
	{
		auto child = this->Children[i];
		if (!child || !child->Visible) continue;
		const auto childLocation = child->GetActualLocationDip();
		const auto childSize = child->GetActualSizeDip();
		maxRight = std::max(maxRight, childLocation.x + childSize.width);
		maxBottom = std::max(maxBottom, childLocation.y + childSize.height);
	}
	return cui::core::Size{
		std::max(0.0f, maxRight + this->_padding.Right),
		std::max(0.0f, maxBottom + this->_padding.Bottom) };
}

ScrollView::ScrollLayout ScrollView::CalcScrollLayout()
{
	ScrollLayout layout{};
	layout.ScrollBarThickness = ScrollBarThickness;

	cui::core::Size content = this->AutoContentSize
		? MeasureContentSizeDip()
		: cui::core::Size{ (float)this->ContentSize.cx, (float)this->ContentSize.cy };
	content = content.NonNegative();
	const auto viewportSize = this->GetActualSizeDip();

	bool needsVerticalScroll = this->AlwaysShowVScroll;
	bool needsHorizontalScroll = this->AlwaysShowHScroll;
	for (int iter = 0; iter < 3; ++iter)
	{
		float viewportWidth = viewportSize.width - (needsVerticalScroll ? layout.ScrollBarThickness : 0.0f);
		float viewportHeight = viewportSize.height - (needsHorizontalScroll ? layout.ScrollBarThickness : 0.0f);
		if (viewportWidth < 0.0f) viewportWidth = 0.0f;
		if (viewportHeight < 0.0f) viewportHeight = 0.0f;

		bool nextNeedsHorizontalScroll = this->AlwaysShowHScroll || (content.width > viewportWidth);
		bool nextNeedsVerticalScroll = this->AlwaysShowVScroll || (content.height > viewportHeight);
		if (nextNeedsHorizontalScroll == needsHorizontalScroll && nextNeedsVerticalScroll == needsVerticalScroll)
		{
			layout.HasHorizontalScroll = needsHorizontalScroll;
			layout.HasVerticalScroll = needsVerticalScroll;
			layout.ViewportWidth = viewportWidth;
			layout.ViewportHeight = viewportHeight;
			layout.ContentWidth = content.width;
			layout.ContentHeight = content.height;
			layout.MaxScrollX = std::max(0.0f, layout.ContentWidth - viewportWidth);
			layout.MaxScrollY = std::max(0.0f, layout.ContentHeight - viewportHeight);
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
	layout.MaxScrollX = std::max(0.0f, layout.ContentWidth - layout.ViewportWidth);
	layout.MaxScrollY = std::max(0.0f, layout.ContentHeight - layout.ViewportHeight);
	return layout;
}

void ScrollView::ClampScrollOffsets(const ScrollLayout& layout)
{
	int oldX = this->ScrollXOffset;
	int oldY = this->ScrollYOffset;
	this->ScrollXOffset = std::clamp(this->ScrollXOffset, 0, (int)std::ceil(layout.MaxScrollX));
	this->ScrollYOffset = std::clamp(this->ScrollYOffset, 0, (int)std::ceil(layout.MaxScrollY));
	if (oldX != this->ScrollXOffset || oldY != this->ScrollYOffset)
	{
		this->OnScrollChanged(this);
	}
}

void ScrollView::ScrollBy(int deltaX, int deltaY)
{
	SetScrollOffset(this->ScrollXOffset + deltaX, this->ScrollYOffset + deltaY);
}

void ScrollView::SetScrollOffset(int offsetX, int offsetY)
{
	auto layout = this->CalcScrollLayout();
	int newX = std::clamp(offsetX, 0, (int)std::ceil(layout.MaxScrollX));
	int newY = std::clamp(offsetY, 0, (int)std::ceil(layout.MaxScrollY));
	if (newX == this->ScrollXOffset && newY == this->ScrollYOffset)
		return;
	this->ScrollXOffset = newX;
	this->ScrollYOffset = newY;
	this->OnScrollChanged(this);
	this->InvalidateVisual();
}

bool ScrollView::HitVerticalScrollBar(int localX, int localY, const ScrollLayout& layout) const
{
	if (!layout.HasVerticalScroll) return false;
	const auto size = const_cast<ScrollView*>(this)->GetActualSizeDip();
	return (float)localX >= layout.ViewportWidth && (float)localX < size.width
		&& localY >= 0 && (float)localY < layout.ViewportHeight;
}

bool ScrollView::HitHorizontalScrollBar(int localX, int localY, const ScrollLayout& layout) const
{
	if (!layout.HasHorizontalScroll) return false;
	const auto size = const_cast<ScrollView*>(this)->GetActualSizeDip();
	return (float)localY >= layout.ViewportHeight && (float)localY < size.height
		&& localX >= 0 && (float)localX < layout.ViewportWidth;
}

CursorKind ScrollView::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;
	auto layout = this->CalcScrollLayout();
	if (HitVerticalScrollBar(localX, localY, layout)) return CursorKind::SizeNS;
	if (HitHorizontalScrollBar(localX, localY, layout)) return CursorKind::SizeWE;
	return this->Cursor;
}

bool ScrollView::ShouldHitTestChildrenAt(int localX, int localY) const
{
	if (!this->HitTestChildren()) return false;
	auto layout = const_cast<ScrollView*>(this)->CalcScrollLayout();
	if (localX < 0 || localY < 0) return false;
	if (localX >= (int)layout.ViewportWidth || localY >= (int)layout.ViewportHeight) return false;
	return true;
}

bool ScrollView::HitChild(Control* child, int localX, int localY, int& childX, int& childY) const
{
	if (!child || !child->Visible || !child->Enable) return false;
	const auto childLocation = child->GetActualLocationDip();
	const auto childSize = child->GetActualSizeDip();
	const float drawX = childLocation.x - (float)this->ScrollXOffset;
	const float drawY = childLocation.y - (float)this->ScrollYOffset;
	const cui::core::Rect childRect{ drawX, drawY, childSize.width, childSize.height };
	if (!childRect.Contains(cui::core::Point{ (float)localX, (float)localY }))
		return false;
	childX = static_cast<int>(std::floor((float)localX - drawX));
	childY = static_cast<int>(std::floor((float)localY - drawY));
	return true;
}

void ScrollView::DrawScrollBars(const ScrollLayout& layout)
{
	auto d2d = this->ParentForm->Render;
	if (layout.HasVerticalScroll && layout.ViewportHeight > 0.0f && layout.ContentHeight > layout.ViewportHeight)
	{
		float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
		float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
		thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
		float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
		float per = (layout.MaxScrollY > 0.0f) ? std::clamp((float)this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
		float thumbTop = per * moveSpace;
		d2d->FillRoundRect(layout.ViewportWidth, 0.0f, layout.ScrollBarThickness, layout.ViewportHeight, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(layout.ViewportWidth, thumbTop, layout.ScrollBarThickness, thumbH, this->ScrollForeColor, 4.0f);
	}

	if (layout.HasHorizontalScroll && layout.ViewportWidth > 0.0f && layout.ContentWidth > layout.ViewportWidth)
	{
		float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
		float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
		thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
		float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
		float per = (layout.MaxScrollX > 0.0f) ? std::clamp((float)this->ScrollXOffset / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
		float thumbLeft = per * moveSpace;
		d2d->FillRoundRect(0.0f, layout.ViewportHeight, layout.ViewportWidth, layout.ScrollBarThickness, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(thumbLeft, layout.ViewportHeight, thumbW, layout.ScrollBarThickness, this->ScrollForeColor, 4.0f);
	}

	if (layout.HasHorizontalScroll && layout.HasVerticalScroll)
	{
		d2d->FillRect(layout.ViewportWidth, layout.ViewportHeight, layout.ScrollBarThickness, layout.ScrollBarThickness, this->ScrollBackColor);
	}
}

void ScrollView::UpdateVerticalScrollByThumb(float localY, const ScrollLayout& layout)
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
	SetScrollOffset(this->ScrollXOffset, (int)std::lround(per * layout.MaxScrollY));
}

void ScrollView::UpdateHorizontalScrollByThumb(float localX, const ScrollLayout& layout)
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
	SetScrollOffset((int)std::lround(per * layout.MaxScrollX), this->ScrollYOffset);
}

void ScrollView::Update()
{
	if (this->IsVisual == false) return;
	if (!this->IsLayoutSuspended() &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
	{
		PerformScrollContentLayout();
	}

	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);

	this->BeginRender();
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}

		d2d->PushDrawRect(0.0f, 0.0f, layout.ViewportWidth, layout.ViewportHeight);
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (auto c : this->GetChildrenInZOrder())
			{
				if (!c || !c->Visible) continue;
				c->Update();
			}
		}
		d2d->PopDrawRect();

		DrawScrollBars(layout);
		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor);
	}
	this->EndRender();
}

bool ScrollView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	if (!this->IsLayoutSuspended() &&
		(_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout())))
	{
		PerformScrollContentLayout();
	}

	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);

	if (WM_LBUTTONDOWN == message && this->ParentForm)
	{
		this->ParentForm->SetSelectedControl(this, false);
	}

	if (_draggingVerticalScrollBar && message == WM_MOUSEMOVE)
	{
		UpdateVerticalScrollByThumb((float)localY, layout);
		return true;
	}
	if (_draggingHorizontalScrollBar && message == WM_MOUSEMOVE)
	{
		UpdateHorizontalScrollByThumb((float)localX, layout);
		return true;
	}
	if ((_draggingVerticalScrollBar || _draggingHorizontalScrollBar) && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_draggingVerticalScrollBar = false;
		_draggingHorizontalScrollBar = false;
	}

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (localX >= 0 && localY >= 0 && localX < (int)layout.ViewportWidth && localY < (int)layout.ViewportHeight)
		{
			Control* wheelTarget = nullptr;
			int targetLocalX = localX;
			int targetLocalY = localY;
			if (FindDeepestWheelTarget(this, localX, localY, wheelTarget, targetLocalX, targetLocalY) && wheelTarget && wheelTarget != this)
			{
				const auto viewLocation = this->GetAbsoluteLocationDip();
				const cui::core::Point mouseInForm{
					viewLocation.x + (float)localX,
					viewLocation.y + (float)localY };
				for (Control* target = wheelTarget; target && target != this; target = target->Parent)
				{
					if (!target->HandlesMouseWheel()) continue;
					const auto targetLocation = target->GetAbsoluteLocationDip();
					const int targetX = static_cast<int>(std::floor(mouseInForm.x - targetLocation.x));
					const int targetY = static_cast<int>(std::floor(mouseInForm.y - targetLocation.y));
					if (target->CanHandleMouseWheel(delta, targetX, targetY))
					{
						target->ProcessMessage(message, wParam, lParam, targetX, targetY);
						return true;
					}
				}
			}
		}

		if (!this->CanHandleMouseWheel(delta, localX, localY))
			return false;

		int steps = delta / WHEEL_DELTA;
		if (steps != 0)
		{
			SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - (steps * this->MouseWheelStep));
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, delta);
		this->OnMouseWheel(this, eventArgs);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (HitVerticalScrollBar(localX, localY, layout) && layout.ContentHeight > layout.ViewportHeight)
		{
			float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
			float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
			thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
			float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
			float scrollRatio = (layout.MaxScrollY > 0.0f) ? std::clamp((float)this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
			float thumbTop = scrollRatio * moveSpace;
			float pointerY = (float)localY;
			bool hitThumb = pointerY >= thumbTop && pointerY <= (thumbTop + thumbH);
			this->_verticalScrollThumbGrabOffset = hitThumb ? (pointerY - thumbTop) : (thumbH * 0.5f);
			this->_draggingVerticalScrollBar = true;
			UpdateVerticalScrollByThumb(pointerY, layout);
			return true;
		}
		if (HitHorizontalScrollBar(localX, localY, layout) && layout.ContentWidth > layout.ViewportWidth)
		{
			float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
			float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
			thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
			float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
			float scrollRatio = (layout.MaxScrollX > 0.0f) ? std::clamp((float)this->ScrollXOffset / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
			float thumbLeft = scrollRatio * moveSpace;
			float pointerX = (float)localX;
			bool hitThumb = pointerX >= thumbLeft && pointerX <= (thumbLeft + thumbW);
			this->_horizontalScrollThumbGrabOffset = hitThumb ? (pointerX - thumbLeft) : (thumbW * 0.5f);
			this->_draggingHorizontalScrollBar = true;
			UpdateHorizontalScrollByThumb(pointerX, layout);
			return true;
		}
	}
	break;
	case WM_KEYDOWN:
	{
		const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		const int lineStepY = std::max(16, this->MouseWheelStep);
		const int lineStepX = std::max(16, this->MouseWheelStep);
		const int pageStepY = std::max(16, (int)layout.ViewportHeight - lineStepY);
		const int pageStepX = std::max(16, (int)layout.ViewportWidth - lineStepX);
		bool handledScroll = false;

		switch (wParam)
		{
		case VK_UP:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - lineStepY);
				handledScroll = true;
			}
			break;
		case VK_DOWN:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset + lineStepY);
				handledScroll = true;
			}
			break;
		case VK_LEFT:
			if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset - lineStepX, this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		case VK_RIGHT:
			if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset + lineStepX, this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		case VK_PRIOR:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - pageStepY);
				handledScroll = true;
			}
			break;
		case VK_NEXT:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset + pageStepY);
				handledScroll = true;
			}
			break;
		case VK_HOME:
			if (ctrlDown)
			{
				if (layout.MaxScrollX > 0.0f || layout.MaxScrollY > 0.0f)
				{
					SetScrollOffset(0, 0);
					handledScroll = true;
				}
			}
			else if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, 0);
				handledScroll = true;
			}
			else if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset(0, this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		case VK_END:
			if (ctrlDown)
			{
				if (layout.MaxScrollX > 0.0f || layout.MaxScrollY > 0.0f)
				{
					SetScrollOffset((int)std::ceil(layout.MaxScrollX), (int)std::ceil(layout.MaxScrollY));
					handledScroll = true;
				}
			}
			else if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, (int)std::ceil(layout.MaxScrollY));
				handledScroll = true;
			}
			else if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset((int)std::ceil(layout.MaxScrollX), this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		}

		if (handledScroll)
		{
			KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyDown(this, eventArgs);
			return true;
		}
	}
	break;
	}

	if (localX >= 0 && localY >= 0 && localX < (int)layout.ViewportWidth && localY < (int)layout.ViewportHeight)
	{
		for (auto child : this->GetChildrenInReverseZOrder())
		{
			int childX = 0;
			int childY = 0;
			if (!HitChild(child, localX, localY, childX, childY)) continue;
			child->ProcessMessage(message, wParam, lParam, childX, childY);
			break;
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
		for (int fileIndex = 0; fileIndex < (int)fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
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
