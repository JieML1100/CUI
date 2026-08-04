#include "Layout/DockPanel.h"
#include "Window.h"
#include <algorithm>

// DockLayoutEngine 实现

namespace
{
	int LastVisibleChildIndex(const LayoutContext& context)
	{
		for (int index = context.ChildCount() - 1; index >= 0; --index)
		{
			auto* child = context.ChildAt(index);
			if (child && !child->IsCollapsed()) return index;
		}
		return -1;
	}

	cui::core::Size DeflateDockSize(cui::core::Size available, const Thickness& margin)
	{
		return cui::core::Constraints{ available }.Deflate(cui::core::Insets{
			margin.Left, margin.Top, margin.Right, margin.Bottom }).maximum;
	}
}

cui::core::Size DockLayoutEngine::Measure(LayoutContext& context, const cui::core::Constraints& available)
{
	cui::core::Size desiredSize{};
	auto remainingSize = available.Normalized().maximum;
	float accumulatedWidth = 0.0f;
	float accumulatedHeight = 0.0f;
	const int lastVisibleIndex = LastVisibleChildIndex(context);
	
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;

		const Thickness margin = child->Margin;
		const auto childSize = child->Measure(cui::core::Constraints{
			DeflateDockSize(remainingSize, margin) });
		const float childWidth = (std::max)(
			0.0f, childSize.width + margin.Left + margin.Right);
		const float childHeight = (std::max)(
			0.0f, childSize.height + margin.Top + margin.Bottom);
		const bool fillsRemaining =
			_lastChildFill && childIndex == lastVisibleIndex;
		const Dock dock = DockPanel::GetDock(*child);
		if (fillsRemaining)
		{
			desiredSize.width = (std::max)(desiredSize.width,
				accumulatedWidth + childWidth);
			desiredSize.height = (std::max)(desiredSize.height,
				accumulatedHeight + childHeight);
			continue;
		}
		
		switch (dock)
		{
		case Dock::Left:
		case Dock::Right:
			desiredSize.width = (std::max)(desiredSize.width,
				accumulatedWidth + childWidth);
			desiredSize.height = (std::max)(desiredSize.height,
				accumulatedHeight + childHeight);
			accumulatedWidth += childWidth;
			remainingSize.width = (std::max)(0.0f, remainingSize.width - childWidth);
			break;
			
		case Dock::Top:
		case Dock::Bottom:
			desiredSize.width = (std::max)(desiredSize.width,
				accumulatedWidth + childWidth);
			desiredSize.height = (std::max)(desiredSize.height,
				accumulatedHeight + childHeight);
			accumulatedHeight += childHeight;
			remainingSize.height = (std::max)(0.0f, remainingSize.height - childHeight);
			break;
			
		}
	}
	
	_needsLayout = false;
	return desiredSize;
}

void DockLayoutEngine::Arrange(LayoutContext& context, cui::core::Rect finalRect)
{
	// 维护剩余可用空间
	auto remaining = finalRect.Normalized();
	
	const int childCount = context.ChildCount();
	const int lastVisibleIndex = LastVisibleChildIndex(context);
	
	// 遍历子控件并排列
	for (int childIndex = 0; childIndex < childCount; childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
		
		const Dock dock = DockPanel::GetDock(*child);
		const Thickness margin = child->Margin;
		const auto horizontalAlignment =
			cui::layout::ResolveHorizontalArrangeAlignment(*child);
		const auto verticalAlignment =
			cui::layout::ResolveVerticalArrangeAlignment(*child);
		const float remainingWidth = remaining.width;
		const float remainingHeight = remaining.height;
		const auto innerSize = DeflateDockSize(
			cui::core::Size{ remainingWidth, remainingHeight }, margin);
		const auto childSize = child->Measure(cui::core::Constraints{ innerSize });
		
		const bool fillsRemaining =
			childIndex == lastVisibleIndex && _lastChildFill;
		
		float finalX = 0.0f;
		float finalY = 0.0f;
		float finalWidth = 0.0f;
		float finalHeight = 0.0f;
		if (fillsRemaining)
		{
			finalWidth = horizontalAlignment == HorizontalAlignment::Stretch
				? innerSize.width
				: (std::min)(childSize.width, innerSize.width);
			finalHeight = verticalAlignment == VerticalAlignment::Stretch
				? innerSize.height
				: (std::min)(childSize.height, innerSize.height);
			finalX = remaining.x + margin.Left;
			finalY = remaining.y + margin.Top;
			if (horizontalAlignment == HorizontalAlignment::Center)
				finalX += (innerSize.width - finalWidth) * 0.5f;
			else if (horizontalAlignment == HorizontalAlignment::Right)
				finalX += innerSize.width - finalWidth;
			if (verticalAlignment == VerticalAlignment::Center)
				finalY += (innerSize.height - finalHeight) * 0.5f;
			else if (verticalAlignment == VerticalAlignment::Bottom)
				finalY += innerSize.height - finalHeight;
			child->Arrange(cui::core::Rect{
				finalX, finalY,
				(std::max)(0.0f, finalWidth),
				(std::max)(0.0f, finalHeight) });
			continue;
		}
		
		switch (dock)
		{
		case Dock::Left:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalWidth = (std::min)(childSize.width, availableWidth);
			finalHeight = (verticalAlignment == VerticalAlignment::Stretch) ? availableHeight : childSize.height;
			if (finalHeight > availableHeight) finalHeight = availableHeight;

			finalX = remaining.x + margin.Left;
			if (verticalAlignment == VerticalAlignment::Bottom)
				finalY = remaining.Bottom() - margin.Bottom - finalHeight;
			else if (verticalAlignment == VerticalAlignment::Center)
				finalY = remaining.y + margin.Top + (availableHeight - finalHeight) / 2.0f;
			else
				finalY = remaining.y + margin.Top;

			// 更新剩余空间
			const float consumed = (std::min)(remaining.width,
				finalWidth + margin.Left + margin.Right);
			remaining.x += consumed;
			remaining.width -= consumed;
		}
			break;
			
		case Dock::Top:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalWidth = (horizontalAlignment == HorizontalAlignment::Stretch) ? availableWidth : childSize.width;
			if (finalWidth > availableWidth) finalWidth = availableWidth;
			finalHeight = (std::min)(childSize.height, availableHeight);

			if (horizontalAlignment == HorizontalAlignment::Right)
				finalX = remaining.Right() - margin.Right - finalWidth;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				finalX = remaining.x + margin.Left + (availableWidth - finalWidth) / 2.0f;
			else
				finalX = remaining.x + margin.Left;
			finalY = remaining.y + margin.Top;

			// 更新剩余空间
			const float consumed = (std::min)(remaining.height,
				finalHeight + margin.Top + margin.Bottom);
			remaining.y += consumed;
			remaining.height -= consumed;
		}
			break;
			
		case Dock::Right:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalWidth = (std::min)(childSize.width, availableWidth);
			finalHeight = (verticalAlignment == VerticalAlignment::Stretch) ? availableHeight : childSize.height;
			if (finalHeight > availableHeight) finalHeight = availableHeight;

			finalX = remaining.Right() - finalWidth - margin.Right;
			if (verticalAlignment == VerticalAlignment::Bottom)
				finalY = remaining.Bottom() - margin.Bottom - finalHeight;
			else if (verticalAlignment == VerticalAlignment::Center)
				finalY = remaining.y + margin.Top + (availableHeight - finalHeight) / 2.0f;
			else
				finalY = remaining.y + margin.Top;
			
			// 更新剩余空间
			remaining.width -= (std::min)(remaining.width,
				finalWidth + margin.Left + margin.Right);
		}
			break;
			
		case Dock::Bottom:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalWidth = (horizontalAlignment == HorizontalAlignment::Stretch) ? availableWidth : childSize.width;
			if (finalWidth > availableWidth) finalWidth = availableWidth;
			finalHeight = (std::min)(childSize.height, availableHeight);

			if (horizontalAlignment == HorizontalAlignment::Right)
				finalX = remaining.Right() - margin.Right - finalWidth;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				finalX = remaining.x + margin.Left + (availableWidth - finalWidth) / 2.0f;
			else
				finalX = remaining.x + margin.Left;
			finalY = remaining.Bottom() - finalHeight - margin.Bottom;
			
			// 更新剩余空间
			remaining.height -= (std::min)(remaining.height,
				finalHeight + margin.Top + margin.Bottom);
		}
			break;
			
		}
		
		// 确保尺寸非负
		if (finalWidth < 0) finalWidth = 0;
		if (finalHeight < 0) finalHeight = 0;
		
		child->Arrange(cui::core::Rect{
			finalX, finalY, finalWidth, finalHeight });
	}
	
	_needsLayout = false;
}

// DockPanel 实现

const DependencyProperty& DockPanel::LastChildFillProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<DockPanel, bool> options{
			true,
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<DockPanel, bool>(
			DependencyPropertyRegistrationLiteral(L"LastChildFill"),
			[](DockPanel& target) { return target.GetLastChildFill(); },
			[](DockPanel& target, const bool& value)
			{ target.SetLastChildFill(value); },
			[](DockPanel& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.Property
							== &DockPanel::LastChildFillProperty())
							handler();
					});
			},
			std::move(options));
	}();
	return *registration;
}

void DockPanel::RegisterDependencyProperties()
{
	Panel::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)LastChildFillProperty();
#endif
}

void DockPanel::SetLastChildFill(bool value)
{
	if (!SetPropertyField(LastChildFillProperty(), _lastChildFill, value)) return;
	_dockEngine->SetLastChildFill(_lastChildFill);
	InvalidateLayout();
}

DockPanel::DockPanel()
{
	_dockEngine = new DockLayoutEngine();
	SetLayoutEngine(_dockEngine);
}

DockPanel::~DockPanel()
{
	// _dockEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
