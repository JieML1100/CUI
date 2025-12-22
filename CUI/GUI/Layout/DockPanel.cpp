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
	for (int i = 0; i < container->Count; i++)
	{
		auto child = container->operator[](i);
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
	for (int i = 0; i < childCount; i++)
	{
		auto child = container->operator[](i);
		if (!child || !child->Visible) continue;
		
		Dock dock = child->DockPosition;
		Thickness margin = child->Margin;
		SIZE childSize = child->Size;
		
		// 最后一个子控件如果启用 LastChildFill，则填充剩余空间
		bool isLastAndFill = (i == lastIndex && _lastChildFill);
		if (isLastAndFill)
		{
			dock = Dock::Fill;
		}
		
		float x = 0, y = 0, width = 0, height = 0;
		
		switch (dock)
		{
		case Dock::Left:
			x = remaining.left + margin.Left;
			y = remaining.top + margin.Top;
			width = (float)childSize.cx;
			height = remaining.bottom - remaining.top - margin.Top - margin.Bottom;
			
			// 更新剩余空间
			remaining.left += width + margin.Left + margin.Right;
			break;
			
		case Dock::Top:
			x = remaining.left + margin.Left;
			y = remaining.top + margin.Top;
			width = remaining.right - remaining.left - margin.Left - margin.Right;
			height = (float)childSize.cy;
			
			// 更新剩余空间
			remaining.top += height + margin.Top + margin.Bottom;
			break;
			
		case Dock::Right:
			width = (float)childSize.cx;
			height = remaining.bottom - remaining.top - margin.Top - margin.Bottom;
			x = remaining.right - width - margin.Right;
			y = remaining.top + margin.Top;
			
			// 更新剩余空间
			remaining.right -= width + margin.Left + margin.Right;
			break;
			
		case Dock::Bottom:
			x = remaining.left + margin.Left;
			width = remaining.right - remaining.left - margin.Left - margin.Right;
			height = (float)childSize.cy;
			y = remaining.bottom - height - margin.Bottom;
			
			// 更新剩余空间
			remaining.bottom -= height + margin.Top + margin.Bottom;
			break;
			
		case Dock::Fill:
			x = remaining.left + margin.Left;
			y = remaining.top + margin.Top;
			width = remaining.right - remaining.left - margin.Left - margin.Right;
			height = remaining.bottom - remaining.top - margin.Top - margin.Bottom;
			break;
		}
		
		// 确保尺寸非负
		if (width < 0) width = 0;
		if (height < 0) height = 0;
		
		// 应用布局
		POINT loc = { (LONG)x, (LONG)y };
		SIZE size = { (LONG)width, (LONG)height };
		child->ApplyLayout(loc, size);
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
