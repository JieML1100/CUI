#include "Layout/DockPanel.h"
#include "Form.h"
#include <algorithm>

// DockLayoutEngine 实现

namespace
{
	int LastVisibleChildIndex(const LayoutContext& context)
	{
		for (int index = context.ChildCount() - 1; index >= 0; --index)
		{
			auto* child = context.ChildAt(index);
			if (child && child->Visible) return index;
		}
		return -1;
	}

	cui::core::Size DeflateDockSize(cui::core::Size available, const Thickness& margin)
	{
		return cui::core::Constraints{ available }.Deflate(cui::core::Insets{
			margin.Left, margin.Top, margin.Right, margin.Bottom }).maximum;
	}
}

SIZE DockLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return SIZE{ 0, 0 };
	LayoutContext context(container);
	const auto desired = Measure(context, cui::core::Constraints{ cui::core::Size{
		static_cast<float>((std::max)(0L, availableSize.cx)),
		static_cast<float>((std::max)(0L, availableSize.cy)) } });
	return SIZE{ static_cast<LONG>(std::ceil(desired.width)), static_cast<LONG>(std::ceil(desired.height)) };
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
		if (!child || !child->Visible) continue;

		const Thickness margin = child->Margin;
		const auto childSize = child->Measure(cui::core::Constraints{
			DeflateDockSize(remainingSize, margin) });
		const float childWidth = (std::max)(
			0.0f, childSize.width + margin.Left + margin.Right);
		const float childHeight = (std::max)(
			0.0f, childSize.height + margin.Top + margin.Bottom);
		const Dock dock = _lastChildFill && childIndex == lastVisibleIndex
			? Dock::Fill
			: child->DockPosition;
		
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
			
		case Dock::Fill:
			desiredSize.width = (std::max)(desiredSize.width,
				accumulatedWidth + childWidth);
			desiredSize.height = (std::max)(desiredSize.height,
				accumulatedHeight + childHeight);
			break;
		}
	}
	
	_needsLayout = false;
	return desiredSize;
}

void DockLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	LayoutContext context(container);
	Arrange(context, finalRect);
}

void DockLayoutEngine::Arrange(LayoutContext& context, D2D1_RECT_F finalRect)
{
	
	// 维护剩余可用空间
	D2D1_RECT_F remaining = finalRect;
	
	const int childCount = context.ChildCount();
	const int lastVisibleIndex = LastVisibleChildIndex(context);
	
	// 遍历子控件并排列
	for (int childIndex = 0; childIndex < childCount; childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || !child->Visible) continue;
		
		Dock dock = child->DockPosition;
		const Thickness margin = child->Margin;
		const HorizontalAlignment horizontalAlignment = child->HAlign;
		const VerticalAlignment verticalAlignment = child->VAlign;
		const float remainingWidth = (std::max)(0.0f, remaining.right - remaining.left);
		const float remainingHeight = (std::max)(0.0f, remaining.bottom - remaining.top);
		const auto innerSize = DeflateDockSize(
			cui::core::Size{ remainingWidth, remainingHeight }, margin);
		const auto childSize = child->Measure(cui::core::Constraints{ innerSize });
		
		// 最后一个子控件如果启用 LastChildFill，则填充剩余空间
		bool isLastAndFill = (childIndex == lastVisibleIndex && _lastChildFill);
		if (isLastAndFill)
		{
			dock = Dock::Fill;
		}
		
		float finalX = 0.0f;
		float finalY = 0.0f;
		float finalWidth = 0.0f;
		float finalHeight = 0.0f;
		
		switch (dock)
		{
		case Dock::Left:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalWidth = (std::min)(childSize.width, availableWidth);
			finalHeight = (verticalAlignment == VerticalAlignment::Stretch) ? availableHeight : childSize.height;
			if (finalHeight > availableHeight) finalHeight = availableHeight;

			finalX = remaining.left + margin.Left;
			if (verticalAlignment == VerticalAlignment::Bottom)
				finalY = remaining.bottom - margin.Bottom - finalHeight;
			else if (verticalAlignment == VerticalAlignment::Center)
				finalY = remaining.top + margin.Top + (availableHeight - finalHeight) / 2.0f;
			else
				finalY = remaining.top + margin.Top;

			// 更新剩余空间
			remaining.left = (std::clamp)(
				remaining.left + finalWidth + margin.Left + margin.Right,
				remaining.left, remaining.right);
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
				finalX = remaining.right - margin.Right - finalWidth;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				finalX = remaining.left + margin.Left + (availableWidth - finalWidth) / 2.0f;
			else
				finalX = remaining.left + margin.Left;
			finalY = remaining.top + margin.Top;

			// 更新剩余空间
			remaining.top = (std::clamp)(
				remaining.top + finalHeight + margin.Top + margin.Bottom,
				remaining.top, remaining.bottom);
		}
			break;
			
		case Dock::Right:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalWidth = (std::min)(childSize.width, availableWidth);
			finalHeight = (verticalAlignment == VerticalAlignment::Stretch) ? availableHeight : childSize.height;
			if (finalHeight > availableHeight) finalHeight = availableHeight;

			finalX = remaining.right - finalWidth - margin.Right;
			if (verticalAlignment == VerticalAlignment::Bottom)
				finalY = remaining.bottom - margin.Bottom - finalHeight;
			else if (verticalAlignment == VerticalAlignment::Center)
				finalY = remaining.top + margin.Top + (availableHeight - finalHeight) / 2.0f;
			else
				finalY = remaining.top + margin.Top;
			
			// 更新剩余空间
			remaining.right = (std::clamp)(
				remaining.right - finalWidth - margin.Left - margin.Right,
				remaining.left, remaining.right);
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
				finalX = remaining.right - margin.Right - finalWidth;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				finalX = remaining.left + margin.Left + (availableWidth - finalWidth) / 2.0f;
			else
				finalX = remaining.left + margin.Left;
			finalY = remaining.bottom - finalHeight - margin.Bottom;
			
			// 更新剩余空间
			remaining.bottom = (std::clamp)(
				remaining.bottom - finalHeight - margin.Top - margin.Bottom,
				remaining.top, remaining.bottom);
		}
			break;
			
		case Dock::Fill:
		{
			const float availableWidth = innerSize.width;
			const float availableHeight = innerSize.height;
			finalX = remaining.left + margin.Left;
			finalY = remaining.top + margin.Top;
			finalWidth = availableWidth;
			finalHeight = availableHeight;
		}
			break;
		}
		
		// 确保尺寸非负
		if (finalWidth < 0) finalWidth = 0;
		if (finalHeight < 0) finalHeight = 0;
		
		child->ApplyLayout(cui::core::Rect{
			finalX, finalY, finalWidth, finalHeight });
	}
	
	_needsLayout = false;
}

// DockPanel 实现

void DockPanel::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		ControlPropertyOptions<DockPanel, bool> options{
			true,
			ControlPropertyFlags::AffectsMeasure
				| ControlPropertyFlags::AffectsArrange
				| ControlPropertyFlags::TracksLocalValue };
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = 10;
		options.Design.Editor = ControlPropertyEditorKind::Boolean;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		BindingPropertyRegistry::Register<DockPanel, bool>(L"LastChildFill",
			[](DockPanel& target) { return target.GetLastChildFill(); },
			[](DockPanel& target, const bool& value) { target.SetLastChildFill(value); },
			[](DockPanel& target, BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						Control*, const ControlPropertyChangedEventArgs& args)
					{
						if (_wcsicmp(args.PropertyName.c_str(), L"LastChildFill") == 0)
							handler();
					});
			},
			std::move(options));
		return true;
	}();
	(void)registered;
}

void DockPanel::SetLastChildFill(bool value)
{
	if (!SetPropertyField(L"LastChildFill", _lastChildFill, value)) return;
	_dockEngine->SetLastChildFill(_lastChildFill);
	InvalidateLayout();
}

DockPanel::DockPanel()
{
	_dockEngine = new DockLayoutEngine();
	SetLayoutEngine(_dockEngine);
}

DockPanel::DockPanel(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	_dockEngine = new DockLayoutEngine();
	SetLayoutEngine(_dockEngine);
}

DockPanel::~DockPanel()
{
	// _dockEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
