#include "Layout/StackPanel.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<StackPanel, TValue> StackLayoutOptions(
		TValue defaultValue,
		int order,
		ControlPropertyEditorKind editor)
	{
		ControlPropertyOptions<StackPanel, TValue> options;
		options.DefaultValue = defaultValue;
		options.Flags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsArrange;
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	ControlPropertyOptions<StackPanel, float> StackSpacingOptions()
	{
		auto options = StackLayoutOptions(0.0f, 20, ControlPropertyEditorKind::Number);
		options.Coerce = [](StackPanel&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed) && proposed > 0.0f ? proposed : 0.0f;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 1.0;
		return options;
	}
}

// StackLayoutEngine 实现

namespace
{
	struct StackItem final
	{
		Control* Child = nullptr;
		Thickness Margin{};
		cui::core::Size Desired{};
	};

	float DeflateExtent(float extent, float before, float after)
	{
		return std::isfinite(extent)
			? (std::max)(0.0f, extent - before - after)
			: cui::core::Infinity;
	}

	float HorizontalOffset(float available, float extent, HorizontalAlignment alignment)
	{
		const float freeSpace = (std::max)(0.0f, available - extent);
		if (alignment == HorizontalAlignment::Center) return freeSpace * 0.5f;
		if (alignment == HorizontalAlignment::Right) return freeSpace;
		return 0.0f;
	}

	float VerticalOffset(float available, float extent, VerticalAlignment alignment)
	{
		const float freeSpace = (std::max)(0.0f, available - extent);
		if (alignment == VerticalAlignment::Center) return freeSpace * 0.5f;
		if (alignment == VerticalAlignment::Bottom) return freeSpace;
		return 0.0f;
	}
}

SIZE StackLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return SIZE{ 0, 0 };
	LayoutContext context(container);
	const auto desired = Measure(context, cui::core::Constraints{ cui::core::Size{
		static_cast<float>((std::max)(0L, availableSize.cx)),
		static_cast<float>((std::max)(0L, availableSize.cy)) } });
	return SIZE{ static_cast<LONG>(std::ceil(desired.width)), static_cast<LONG>(std::ceil(desired.height)) };
}

cui::core::Size StackLayoutEngine::Measure(LayoutContext& context, const cui::core::Constraints& available)
{
	const auto maximum = available.Normalized().maximum;
	cui::core::Size desiredSize{};
	int visibleCount = 0;

	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || !child->Visible) continue;
		const Thickness margin = child->Margin;
		const cui::core::Constraints childConstraints{ cui::core::Size{
			_orientation == Orientation::Vertical
				? DeflateExtent(maximum.width, margin.Left, margin.Right)
				: cui::core::Infinity,
			_orientation == Orientation::Horizontal
				? DeflateExtent(maximum.height, margin.Top, margin.Bottom)
				: cui::core::Infinity } };
		const auto childSize = child->Measure(childConstraints);
		const float outerWidth = childSize.width + margin.Left + margin.Right;
		const float outerHeight = childSize.height + margin.Top + margin.Bottom;

		if (_orientation == Orientation::Vertical)
		{
			desiredSize.width = (std::max)(desiredSize.width, outerWidth);
			desiredSize.height += outerHeight;
		}
		else
		{
			desiredSize.width += outerWidth;
			desiredSize.height = (std::max)(desiredSize.height, outerHeight);
		}
		++visibleCount;
	}

	if (visibleCount > 1)
	{
		const float totalSpacing = _spacing * static_cast<float>(visibleCount - 1);
		if (_orientation == Orientation::Vertical)
			desiredSize.height += totalSpacing;
		else
			desiredSize.width += totalSpacing;
	}

	_needsLayout = false;
	return desiredSize;
}

void StackLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	LayoutContext context(container);
	Arrange(context, finalRect);
}

void StackLayoutEngine::Arrange(LayoutContext& context, D2D1_RECT_F finalRect)
{
	const float originX = finalRect.left;
	const float originY = finalRect.top;
	float containerWidth = finalRect.right - finalRect.left;
	float containerHeight = finalRect.bottom - finalRect.top;
	if (containerWidth < 0.0f) containerWidth = 0.0f;
	if (containerHeight < 0.0f) containerHeight = 0.0f;

	std::vector<StackItem> items;
	items.reserve(static_cast<size_t>((std::max)(0, context.ChildCount())));
	for (int childIndex = 0; childIndex < context.ChildCount(); ++childIndex)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || !child->Visible) continue;
		const Thickness margin = child->Margin;
		const auto desired = child->Measure(cui::core::Constraints{ cui::core::Size{
			_orientation == Orientation::Vertical
				? DeflateExtent(containerWidth, margin.Left, margin.Right)
				: cui::core::Infinity,
			_orientation == Orientation::Horizontal
				? DeflateExtent(containerHeight, margin.Top, margin.Bottom)
				: cui::core::Infinity } });
		items.push_back(StackItem{ child, margin, desired });
	}

	if (_orientation == Orientation::Vertical)
	{
		float naturalBandWidth = 0.0f;
		for (const auto& item : items)
			naturalBandWidth = (std::max)(naturalBandWidth,
				item.Desired.width + item.Margin.Left + item.Margin.Right);
		const float bandWidth = _horizontalContentAlignment == HorizontalAlignment::Stretch
			? containerWidth
			: (std::min)(containerWidth, naturalBandWidth);
		const float bandX = HorizontalOffset(
			containerWidth, bandWidth, _horizontalContentAlignment);

		float contentHeight = items.empty()
			? 0.0f
			: _spacing * static_cast<float>(items.size() - 1);
		for (auto& item : items)
		{
			item.Desired = item.Child->Measure(cui::core::Constraints{ cui::core::Size{
				DeflateExtent(bandWidth, item.Margin.Left, item.Margin.Right),
				cui::core::Infinity } });
			contentHeight += item.Desired.height + item.Margin.Top + item.Margin.Bottom;
		}

		float currentY = originY + VerticalOffset(
			containerHeight, contentHeight, _verticalContentAlignment);
		for (const auto& item : items)
		{
			const float availableWidth = DeflateExtent(
				bandWidth, item.Margin.Left, item.Margin.Right);
			float childWidth = item.Desired.width;
			if (item.Child->HAlign == HorizontalAlignment::Stretch)
			{
				childWidth = availableWidth;
			}

			float childX = item.Margin.Left;
			if (item.Child->HAlign == HorizontalAlignment::Center)
			{
				childX += (availableWidth - childWidth) * 0.5f;
			}
			else if (item.Child->HAlign == HorizontalAlignment::Right)
			{
				childX += availableWidth - childWidth;
			}

			const float childHeight = item.Desired.height;
			item.Child->ApplyLayout(cui::core::Rect{
				originX + bandX + childX,
				currentY + item.Margin.Top,
				childWidth,
				childHeight });
			currentY += childHeight + item.Margin.Top + item.Margin.Bottom + _spacing;
		}
	}
	else // Horizontal
	{
		float naturalBandHeight = 0.0f;
		for (const auto& item : items)
			naturalBandHeight = (std::max)(naturalBandHeight,
				item.Desired.height + item.Margin.Top + item.Margin.Bottom);
		const float bandHeight = _verticalContentAlignment == VerticalAlignment::Stretch
			? containerHeight
			: (std::min)(containerHeight, naturalBandHeight);
		const float bandY = VerticalOffset(
			containerHeight, bandHeight, _verticalContentAlignment);

		float contentWidth = items.empty()
			? 0.0f
			: _spacing * static_cast<float>(items.size() - 1);
		for (auto& item : items)
		{
			item.Desired = item.Child->Measure(cui::core::Constraints{ cui::core::Size{
				cui::core::Infinity,
				DeflateExtent(bandHeight, item.Margin.Top, item.Margin.Bottom) } });
			contentWidth += item.Desired.width + item.Margin.Left + item.Margin.Right;
		}

		float currentX = originX + HorizontalOffset(
			containerWidth, contentWidth, _horizontalContentAlignment);
		for (const auto& item : items)
		{
			const float availableHeight = DeflateExtent(
				bandHeight, item.Margin.Top, item.Margin.Bottom);
			float childHeight = item.Desired.height;
			if (item.Child->VAlign == VerticalAlignment::Stretch)
			{
				childHeight = availableHeight;
			}

			float childY = item.Margin.Top;
			if (item.Child->VAlign == VerticalAlignment::Center)
			{
				childY += (availableHeight - childHeight) * 0.5f;
			}
			else if (item.Child->VAlign == VerticalAlignment::Bottom)
			{
				childY += availableHeight - childHeight;
			}

			const float childWidth = item.Desired.width;
			item.Child->ApplyLayout(cui::core::Rect{
				currentX + item.Margin.Left,
				originY + bandY + childY,
				childWidth,
				childHeight });
			currentX += childWidth + item.Margin.Left + item.Margin.Right + _spacing;
		}
	}

	_needsLayout = false;
}

// StackPanel 实现

void StackPanel::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto orientationOptions = StackLayoutOptions(
			Orientation::Vertical, 10, ControlPropertyEditorKind::Choice);
		orientationOptions.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		BindingPropertyRegistry::Register<StackPanel, Orientation>(L"Orientation",
			[](StackPanel& target) { return target.GetOrientation(); },
			[](StackPanel& target, const Orientation& value) { target.SetOrientation(value); },
			{}, std::move(orientationOptions));
		BindingPropertyRegistry::Register<StackPanel, float>(L"Spacing",
			[](StackPanel& target) { return target.GetSpacing(); },
			[](StackPanel& target, const float& value) { target.SetSpacing(value); },
			{}, StackSpacingOptions());
		auto horizontalOptions = StackLayoutOptions(
			HorizontalAlignment::Stretch, 30, ControlPropertyEditorKind::Choice);
		horizontalOptions.Design.Choices = {
			{ L"Left", BindingValue(HorizontalAlignment::Left) },
			{ L"Center", BindingValue(HorizontalAlignment::Center) },
			{ L"Right", BindingValue(HorizontalAlignment::Right) },
			{ L"Stretch", BindingValue(HorizontalAlignment::Stretch) }
		};
		BindingPropertyRegistry::Register<StackPanel, HorizontalAlignment>(L"HorizontalContentAlignment",
			[](StackPanel& target) { return target.GetHorizontalContentAlignment(); },
			[](StackPanel& target, const HorizontalAlignment& value)
			{ target.SetHorizontalContentAlignment(value); },
			{}, std::move(horizontalOptions));
		auto verticalOptions = StackLayoutOptions(
			VerticalAlignment::Stretch, 40, ControlPropertyEditorKind::Choice);
		verticalOptions.Design.Choices = {
			{ L"Top", BindingValue(VerticalAlignment::Top) },
			{ L"Center", BindingValue(VerticalAlignment::Center) },
			{ L"Bottom", BindingValue(VerticalAlignment::Bottom) },
			{ L"Stretch", BindingValue(VerticalAlignment::Stretch) }
		};
		BindingPropertyRegistry::Register<StackPanel, VerticalAlignment>(L"VerticalContentAlignment",
			[](StackPanel& target) { return target.GetVerticalContentAlignment(); },
			[](StackPanel& target, const VerticalAlignment& value)
			{ target.SetVerticalContentAlignment(value); },
			{}, std::move(verticalOptions));
		return true;
	}();
	(void)registered;
}

StackPanel::StackPanel()
{
	_stackEngine = new StackLayoutEngine();
	SetLayoutEngine(_stackEngine);
}

StackPanel::StackPanel(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	_stackEngine = new StackLayoutEngine();
	SetLayoutEngine(_stackEngine);
}

StackPanel::~StackPanel()
{
	// _stackEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
	// 所以这里不需要再删除
}
