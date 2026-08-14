#include "Layout/StackPanel.h"
#include "Window.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<StackPanel, TValue> StackLayoutOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor))
	{
		DependencyPropertyOptions<StackPanel, TValue> options;
		options.DefaultValue = defaultValue;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
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

}

cui::core::Size StackLayoutEngine::Measure(LayoutContext& context, const cui::core::Constraints& available)
{
	const auto maximum = available.Normalized().maximum;
	cui::core::Size desiredSize{};
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
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
	}

	_needsLayout = false;
	return desiredSize;
}

void StackLayoutEngine::Arrange(LayoutContext& context, cui::core::Rect finalRect)
{
	finalRect = finalRect.Normalized();
	const float originX = finalRect.x;
	const float originY = finalRect.y;
	const float containerWidth = finalRect.width;
	const float containerHeight = finalRect.height;

	std::vector<StackItem> items;
	items.reserve(static_cast<size_t>((std::max)(0, context.ChildCount())));
	for (int childIndex = 0; childIndex < context.ChildCount(); ++childIndex)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
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
		float currentY = originY;
		for (const auto& item : items)
		{
			const float availableWidth = DeflateExtent(
				containerWidth, item.Margin.Left, item.Margin.Right);
			const auto horizontalAlignment =
				cui::layout::ResolveHorizontalArrangeAlignment(*item.Child);
			float childWidth = item.Desired.width;
			if (horizontalAlignment == HorizontalAlignment::Stretch)
				childWidth = availableWidth;

			float childX = item.Margin.Left;
			if (horizontalAlignment == HorizontalAlignment::Center)
			{
				childX += (availableWidth - childWidth) * 0.5f;
			}
			else if (horizontalAlignment == HorizontalAlignment::Right)
			{
				childX += availableWidth - childWidth;
			}

			const float childHeight = item.Desired.height;
			item.Child->Arrange(cui::core::Rect{
				originX + childX,
				currentY + item.Margin.Top,
				childWidth,
				childHeight });
			currentY += childHeight + item.Margin.Top + item.Margin.Bottom;
		}
	}
	else // Horizontal
	{
		float currentX = originX;
		for (const auto& item : items)
		{
			const float availableHeight = DeflateExtent(
				containerHeight, item.Margin.Top, item.Margin.Bottom);
			const auto verticalAlignment =
				cui::layout::ResolveVerticalArrangeAlignment(*item.Child);
			float childHeight = item.Desired.height;
			if (verticalAlignment == VerticalAlignment::Stretch)
			{
				childHeight = availableHeight;
			}

			float childY = item.Margin.Top;
			if (verticalAlignment == VerticalAlignment::Center)
			{
				childY += (availableHeight - childHeight) * 0.5f;
			}
			else if (verticalAlignment == VerticalAlignment::Bottom)
			{
				childY += availableHeight - childHeight;
			}

			const float childWidth = item.Desired.width;
			item.Child->Arrange(cui::core::Rect{
				currentX + item.Margin.Left,
				originY + childY,
				childWidth,
				childHeight });
			currentX += childWidth + item.Margin.Left + item.Margin.Right;
		}
	}

	_needsLayout = false;
}

// StackPanel 实现

const DependencyProperty& StackPanel::OrientationProperty()
{
	static const auto registration = []
	{
		auto options = StackLayoutOptions(
			Orientation::Vertical
			CUI_DESIGN_METADATA_ARGUMENTS(
				10,
				DependencyPropertyEditorKind::Choice));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			StackPanel, Orientation>(
				DependencyPropertyRegistrationLiteral(L"Orientation"),
				[](StackPanel& target) { return target.GetOrientation(); },
				[](StackPanel& target, const Orientation& value)
				{ target.SetOrientation(value); },
				{}, std::move(options));
	}();
	return *registration;
}

void StackPanel::RegisterDependencyProperties()
{
	Panel::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)OrientationProperty();
#endif
}

StackPanel::StackPanel()
{
	_stackEngine = new StackLayoutEngine();
	SetLayoutEngine(_stackEngine);
}

StackPanel::~StackPanel()
{
	// _stackEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
	// 所以这里不需要再删除
}
