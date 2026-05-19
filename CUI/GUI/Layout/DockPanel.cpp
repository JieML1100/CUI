#include "DockPanel.h"
#include "../Form.h"
#include <algorithm>

// DockLayoutEngine 实现

SIZE DockLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return {0, 0};
	
	SIZE desiredSize = {0, 0};
	SIZE remainingSize = availableSize;
	
	// 遍历所有子控件，按停靠位置累计尺寸
	for (int childIndex = 0; childIndex < container->Count; childIndex++)
	{
		auto child = container->operator[](childIndex);
		if (!child || !child->Visible) continue;
		
		SIZE childSize = child->MeasureCore(remainingSize);
		Thickness margin = child->Margin;
		Dock dock = child->DockPosition;
		
		LONG childWidth = childSize.cx + (LONG)(margin.Left + margin.Right);
		LONG childHeight = childSize.cy + (LONG)(margin.Top + margin.Bottom);
		
		switch (dock)
		{
		case Dock::Left:
		case Dock::Right:
			desiredSize.cx += childWidth;
			if (childHeight > desiredSize.cy)
				desiredSize.cy = childHeight;
			remainingSize.cx -= childWidth;
			if (remainingSize.cx < 0) remainingSize.cx = 0;
			break;
			
		case Dock::Top:
		case Dock::Bottom:
			if (childWidth > desiredSize.cx)
				desiredSize.cx = childWidth;
			desiredSize.cy += childHeight;
			remainingSize.cy -= childHeight;
			if (remainingSize.cy < 0) remainingSize.cy = 0;
			break;
			
		case Dock::Fill:
			if (childWidth > desiredSize.cx)
				desiredSize.cx = childWidth;
			if (childHeight > desiredSize.cy)
				desiredSize.cy = childHeight;
			break;
		}
	}
	
	_needsLayout = false;
	return desiredSize;
}

void DockLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	
	// 维护剩余可用空间
	D2D1_RECT_F remaining = finalRect;
	
	int childCount = container->Count;
	int lastIndex = childCount - 1;
	
	// 遍历子控件并排列
	for (int childIndex = 0; childIndex < childCount; childIndex++)
	{
		auto child = container->operator[](childIndex);
		if (!child || !child->Visible) continue;
		
		Dock dock = child->DockPosition;
		Thickness margin = child->Margin;
		HorizontalAlignment horizontalAlignment = child->HAlign;
		VerticalAlignment verticalAlignment = child->VAlign;
		SIZE childSize = child->MeasureCore({ (LONG)(remaining.right - remaining.left), (LONG)(remaining.bottom - remaining.top) });
		
		// 最后一个子控件如果启用 LastChildFill，则填充剩余空间
		bool isLastAndFill = (childIndex == lastIndex && _lastChildFill);
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
			float availableHeight = remaining.bottom - remaining.top - margin.Top - margin.Bottom;
			if (availableHeight < 0) availableHeight = 0;
			finalWidth = (float)childSize.cx;
			finalHeight = (verticalAlignment == VerticalAlignment::Stretch) ? availableHeight : (float)childSize.cy;
			if (finalHeight > availableHeight) finalHeight = availableHeight;

			finalX = remaining.left + margin.Left;
			if (verticalAlignment == VerticalAlignment::Bottom)
				finalY = remaining.bottom - margin.Bottom - finalHeight;
			else if (verticalAlignment == VerticalAlignment::Center)
				finalY = remaining.top + margin.Top + (availableHeight - finalHeight) / 2.0f;
			else
				finalY = remaining.top + margin.Top;

			// 更新剩余空间
			remaining.left += finalWidth + margin.Left + margin.Right;
		}
			break;
			
		case Dock::Top:
		{
			float availableWidth = remaining.right - remaining.left - margin.Left - margin.Right;
			if (availableWidth < 0) availableWidth = 0;
			finalWidth = (horizontalAlignment == HorizontalAlignment::Stretch) ? availableWidth : (float)childSize.cx;
			if (finalWidth > availableWidth) finalWidth = availableWidth;
			finalHeight = (float)childSize.cy;

			if (horizontalAlignment == HorizontalAlignment::Right)
				finalX = remaining.right - margin.Right - finalWidth;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				finalX = remaining.left + margin.Left + (availableWidth - finalWidth) / 2.0f;
			else
				finalX = remaining.left + margin.Left;
			finalY = remaining.top + margin.Top;

			// 更新剩余空间
			remaining.top += finalHeight + margin.Top + margin.Bottom;
		}
			break;
			
		case Dock::Right:
		{
			float availableHeight = remaining.bottom - remaining.top - margin.Top - margin.Bottom;
			if (availableHeight < 0) availableHeight = 0;
			finalWidth = (float)childSize.cx;
			finalHeight = (verticalAlignment == VerticalAlignment::Stretch) ? availableHeight : (float)childSize.cy;
			if (finalHeight > availableHeight) finalHeight = availableHeight;

			finalX = remaining.right - finalWidth - margin.Right;
			if (verticalAlignment == VerticalAlignment::Bottom)
				finalY = remaining.bottom - margin.Bottom - finalHeight;
			else if (verticalAlignment == VerticalAlignment::Center)
				finalY = remaining.top + margin.Top + (availableHeight - finalHeight) / 2.0f;
			else
				finalY = remaining.top + margin.Top;
			
			// 更新剩余空间
			remaining.right -= finalWidth + margin.Left + margin.Right;
		}
			break;
			
		case Dock::Bottom:
		{
			float availableWidth = remaining.right - remaining.left - margin.Left - margin.Right;
			if (availableWidth < 0) availableWidth = 0;
			finalWidth = (horizontalAlignment == HorizontalAlignment::Stretch) ? availableWidth : (float)childSize.cx;
			if (finalWidth > availableWidth) finalWidth = availableWidth;
			finalHeight = (float)childSize.cy;

			if (horizontalAlignment == HorizontalAlignment::Right)
				finalX = remaining.right - margin.Right - finalWidth;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				finalX = remaining.left + margin.Left + (availableWidth - finalWidth) / 2.0f;
			else
				finalX = remaining.left + margin.Left;
			finalY = remaining.bottom - finalHeight - margin.Bottom;
			
			// 更新剩余空间
			remaining.bottom -= finalHeight + margin.Top + margin.Bottom;
		}
			break;
			
		case Dock::Fill:
		{
			float availableWidth = remaining.right - remaining.left - margin.Left - margin.Right;
			float availableHeight = remaining.bottom - remaining.top - margin.Top - margin.Bottom;
			if (availableWidth < 0) availableWidth = 0;
			if (availableHeight < 0) availableHeight = 0;
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
		
		// 应用布局
		POINT finalLocation = { (LONG)finalX, (LONG)finalY };
		SIZE finalSize = { (LONG)finalWidth, (LONG)finalHeight };
		child->ApplyLayout(finalLocation, finalSize);
	}
	
	_needsLayout = false;
}

// DockPanel 实现

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
