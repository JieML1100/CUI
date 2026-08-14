#include "Layout/WrapPanel.h"
#include "Layout/StackPanel.h"
#include "Window.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<WrapPanel, TValue> WrapLayoutOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor))
	{
		DependencyPropertyOptions<WrapPanel, TValue> options;
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

	DependencyPropertyOptions<WrapPanel, float> WrapItemSizeOptions(
		float defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			const wchar_t* displayName))
	{
		auto options = WrapLayoutOptions(defaultValue
			CUI_DESIGN_METADATA_ARGUMENTS(
				order,
				DependencyPropertyEditorKind::Number));
		options.Coerce = [](WrapPanel&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed) && proposed > 0.0f ? proposed : 0.0f;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.DisplayName = displayName;
		options.Design.Minimum = 0.0;
		options.Design.Step = 1.0;
		)
		return options;
	}

	const DependencyPropertyMetadataRegistration&
		WrapPanelOrientationMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			auto options = WrapLayoutOptions(
				Orientation::Horizontal
				CUI_DESIGN_METADATA_ARGUMENTS(
					10, DependencyPropertyEditorKind::Choice));
			CUI_DESIGN_METADATA_ONLY(
			options.Design.Choices = {
				{ L"Horizontal", BindingValue(Orientation::Horizontal) },
				{ L"Vertical", BindingValue(Orientation::Vertical) }
			};
			)
			return DependencyPropertyRegistry::AddOwnerStatic<
				WrapPanel, Orientation>(StackPanel::OrientationProperty(),
				[](WrapPanel& target) { return target.GetOrientation(); },
				[](WrapPanel& target, const Orientation& value)
				{ target.SetOrientation(value); }, {}, std::move(options));
		}();
		return relation;
	}
}

// WrapLayoutEngine 实现

namespace
{
	float DeflateWrapExtent(float extent, float before, float after)
	{
		return std::isfinite(extent)
			? (std::max)(0.0f, extent - before - after)
			: cui::core::Infinity;
	}

	cui::core::Size MeasureWrapItem(
		Control& child,
		const Thickness& margin,
		cui::core::Size available,
		float itemWidth,
		float itemHeight)
	{
		const float childWidth = itemWidth > 0.0f
			? itemWidth
			: DeflateWrapExtent(available.width, margin.Left, margin.Right);
		const float childHeight = itemHeight > 0.0f
			? itemHeight
			: DeflateWrapExtent(available.height, margin.Top, margin.Bottom);
		auto desired = child.Measure(cui::core::Constraints{
			cui::core::Size{ childWidth, childHeight } });
		if (itemWidth > 0.0f) desired.width = itemWidth;
		if (itemHeight > 0.0f) desired.height = itemHeight;
		return desired;
	}
}

cui::core::Size WrapLayoutEngine::Measure(LayoutContext& context, const cui::core::Constraints& available)
{
	const auto availableSize = available.Normalized().maximum;
	cui::core::Size desiredSize{};
	
	if (_orientation == Orientation::Horizontal)
	{
		// 水平方向：从左到右排列，超出换行
		float lineWidth = 0.0f;
		float lineHeight = 0.0f;
		float totalHeight = 0.0f;
		float maxLineWidth = 0.0f;
		
		for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
		{
			auto child = context.ChildAt(childIndex);
			if (!child || child->IsCollapsed()) continue;
			
			Thickness margin = child->Margin;
			const auto childSize = MeasureWrapItem(
				*child, margin, availableSize, _itemWidth, _itemHeight);
			
			float itemWidth = childSize.width;
			float itemHeight = childSize.height;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换行
			if (lineWidth + totalItemWidth > availableSize.width && lineWidth > 0)
			{
				// 换行
				if (lineWidth > maxLineWidth)
					maxLineWidth = lineWidth;
				totalHeight += lineHeight;
				lineWidth = totalItemWidth;
				lineHeight = totalItemHeight;
			}
			else
			{
				lineWidth += totalItemWidth;
				if (totalItemHeight > lineHeight)
					lineHeight = totalItemHeight;
			}
		}
		
		// 最后一行
		if (lineWidth > maxLineWidth)
			maxLineWidth = lineWidth;
		totalHeight += lineHeight;
		
		desiredSize.width = maxLineWidth;
		desiredSize.height = totalHeight;
	}
	else // Vertical
	{
		// 垂直方向：从上到下排列，超出换列
		float columnHeight = 0.0f;
		float columnWidth = 0.0f;
		float totalWidth = 0.0f;
		float maxColumnHeight = 0.0f;
		
		for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
		{
			auto child = context.ChildAt(childIndex);
			if (!child || child->IsCollapsed()) continue;
			
			Thickness margin = child->Margin;
			const auto childSize = MeasureWrapItem(
				*child, margin, availableSize, _itemWidth, _itemHeight);
			
			float itemWidth = childSize.width;
			float itemHeight = childSize.height;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换列
			if (columnHeight + totalItemHeight > availableSize.height && columnHeight > 0)
			{
				// 换列
				if (columnHeight > maxColumnHeight)
					maxColumnHeight = columnHeight;
				totalWidth += columnWidth;
				columnHeight = totalItemHeight;
				columnWidth = totalItemWidth;
			}
			else
			{
				columnHeight += totalItemHeight;
				if (totalItemWidth > columnWidth)
					columnWidth = totalItemWidth;
			}
		}
		
		// 最后一列
		if (columnHeight > maxColumnHeight)
			maxColumnHeight = columnHeight;
		totalWidth += columnWidth;
		
		desiredSize.width = totalWidth;
		desiredSize.height = maxColumnHeight;
	}
	
	_needsLayout = false;
	return desiredSize;
}

void WrapLayoutEngine::Arrange(LayoutContext& context, cui::core::Rect finalRect)
{
	finalRect = finalRect.Normalized();
	const float originX = finalRect.x;
	const float originY = finalRect.y;
	const float containerWidth = finalRect.width;
	const float containerHeight = finalRect.height;
	const cui::core::Size availableSize{ containerWidth, containerHeight };
	
	if (_orientation == Orientation::Horizontal)
	{
		// 水平布局：从左到右，自动换行
		float currentX = 0.0f;
		float currentY = 0.0f;
		float lineHeight = 0.0f;
		
		for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
		{
			auto child = context.ChildAt(childIndex);
			if (!child || child->IsCollapsed()) continue;
			
			Thickness margin = child->Margin;
			const auto childSize = MeasureWrapItem(
				*child, margin, availableSize, _itemWidth, _itemHeight);
			
			float itemWidth = childSize.width;
			float itemHeight = childSize.height;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换行
			if (currentX + totalItemWidth > containerWidth && currentX > 0)
			{
				currentX = 0.0f;
				currentY += lineHeight;
				lineHeight = 0.0f;
			}
			
			// 设置子控件位置
			child->Arrange(cui::core::Rect{
				originX + currentX + margin.Left,
				originY + currentY + margin.Top,
				itemWidth,
				itemHeight });
			
			currentX += totalItemWidth;
			if (totalItemHeight > lineHeight)
				lineHeight = totalItemHeight;
		}
	}
	else // Vertical
	{
		// 垂直布局：从上到下，自动换列
		float currentX = 0.0f;
		float currentY = 0.0f;
		float columnWidth = 0.0f;
		
		for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
		{
			auto child = context.ChildAt(childIndex);
			if (!child || child->IsCollapsed()) continue;
			
			Thickness margin = child->Margin;
			const auto childSize = MeasureWrapItem(
				*child, margin, availableSize, _itemWidth, _itemHeight);
			
			float itemWidth = childSize.width;
			float itemHeight = childSize.height;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换列
			if (currentY + totalItemHeight > containerHeight && currentY > 0)
			{
				currentY = 0.0f;
				currentX += columnWidth;
				columnWidth = 0.0f;
			}
			
			// 设置子控件位置
			child->Arrange(cui::core::Rect{
				originX + currentX + margin.Left,
				originY + currentY + margin.Top,
				itemWidth,
				itemHeight });
			
			currentY += totalItemHeight;
			if (totalItemWidth > columnWidth)
				columnWidth = totalItemWidth;
		}
	}
	
	_needsLayout = false;
}

// WrapPanel 实现

const DependencyProperty& WrapPanel::OrientationProperty()
{
	return WrapPanelOrientationMetadataRelation().Property();
}

const DependencyProperty& WrapPanel::ItemWidthProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<WrapPanel, float>(
			DependencyPropertyRegistrationLiteral(L"ItemWidth"),
			[](WrapPanel& target) { return target.GetItemWidth(); },
			[](WrapPanel& target, const float& value)
			{ target.SetItemWidth(value); }, {},
			WrapItemSizeOptions(0.0f CUI_DESIGN_METADATA_ARGUMENTS(
				20, L"Item Width (0 = Auto)")));
	}();
	return *registration;
}

const DependencyProperty& WrapPanel::ItemHeightProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<WrapPanel, float>(
			DependencyPropertyRegistrationLiteral(L"ItemHeight"),
			[](WrapPanel& target) { return target.GetItemHeight(); },
			[](WrapPanel& target, const float& value)
			{ target.SetItemHeight(value); }, {},
			WrapItemSizeOptions(0.0f CUI_DESIGN_METADATA_ARGUMENTS(
				30, L"Item Height (0 = Auto)")));
	}();
	return *registration;
}

void WrapPanel::RegisterDependencyProperties()
{
	Panel::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)ItemWidthProperty();
	(void)ItemHeightProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)WrapPanelOrientationMetadataRelation();
	)
}

const DependencyPropertyMetadata*
WrapPanel::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &StackPanel::OrientationProperty())
		return &WrapPanelOrientationMetadataRelation().Metadata();
	return Panel::ResolveExactDependencyPropertyMetadata(property);
}

WrapPanel::WrapPanel()
{
	_wrapEngine = new WrapLayoutEngine();
	SetLayoutEngine(_wrapEngine);
}

WrapPanel::~WrapPanel()
{
	// _wrapEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
