#include "StackPanel.h"
#include "../Form.h"
#include <algorithm>

// StackLayoutEngine 实现

SIZE StackLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return {0, 0};
	
	SIZE desiredSize = {0, 0};
	int visibleCount = 0;
	
	if (_orientation == Orientation::Vertical)
	{
		// 垂直堆叠：高度累加，宽度取最大
		for (int childIndex = 0; childIndex < container->Count; childIndex++)
		{
			auto child = container->operator[](childIndex);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->MeasureCore(availableSize);
			Thickness margin = child->Margin;
			
			// 计算包含边距的尺寸
			LONG childWidth = childSize.cx + (LONG)(margin.Left + margin.Right);
			LONG childHeight = childSize.cy + (LONG)(margin.Top + margin.Bottom);
			
			if (childWidth > desiredSize.cx)
				desiredSize.cx = childWidth;
			
			desiredSize.cy += childHeight;
			visibleCount++;
		}
		
		// 添加间距
		if (visibleCount > 1)
		{
			desiredSize.cy += (LONG)(_spacing * (visibleCount - 1));
		}
	}
	else // Horizontal
	{
		// 水平堆叠：宽度累加，高度取最大
		for (int childIndex = 0; childIndex < container->Count; childIndex++)
		{
			auto child = container->operator[](childIndex);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->MeasureCore(availableSize);
			Thickness margin = child->Margin;
			
			// 计算包含边距的尺寸
			LONG childWidth = childSize.cx + (LONG)(margin.Left + margin.Right);
			LONG childHeight = childSize.cy + (LONG)(margin.Top + margin.Bottom);
			
			desiredSize.cx += childWidth;
			
			if (childHeight > desiredSize.cy)
				desiredSize.cy = childHeight;
			
			visibleCount++;
		}
		
		// 添加间距
		if (visibleCount > 1)
		{
			desiredSize.cx += (LONG)(_spacing * (visibleCount - 1));
		}
	}
	
	_needsLayout = false;
	return desiredSize;
}

void StackLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	
	const float originX = finalRect.left;
	const float originY = finalRect.top;
	float currentX = originX;
	float currentY = originY;
	float containerWidth = finalRect.right - finalRect.left;
	float containerHeight = finalRect.bottom - finalRect.top;
	if (containerWidth < 0.0f) containerWidth = 0.0f;
	if (containerHeight < 0.0f) containerHeight = 0.0f;
	
	if (_orientation == Orientation::Vertical)
	{
		// 垂直排列
		for (int childIndex = 0; childIndex < container->Count; childIndex++)
		{
			auto child = container->operator[](childIndex);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->MeasureCore({ (LONG)containerWidth, INT_MAX });
			Thickness margin = child->Margin;
			HorizontalAlignment horizontalAlignment = child->HAlign;
			float availableWidth = containerWidth - margin.Left - margin.Right;
			if (availableWidth < 0.0f) availableWidth = 0.0f;
			
			float childWidth = (float)childSize.cx;
			if (horizontalAlignment == HorizontalAlignment::Stretch)
			{
				childWidth = availableWidth;
			}
			
			float childX = margin.Left;
			if (horizontalAlignment == HorizontalAlignment::Center)
			{
				childX = margin.Left + (availableWidth - childWidth) / 2.0f;
			}
			else if (horizontalAlignment == HorizontalAlignment::Right)
			{
				childX = containerWidth - margin.Right - childWidth;
			}
			
			POINT childLocation = { (LONG)(originX + childX), (LONG)(currentY + margin.Top) };
			float childHeight = (float)childSize.cy;
			SIZE arrangedSize = { (LONG)childWidth, (LONG)childHeight };
			child->ApplyLayout(childLocation, arrangedSize);
			
			// 移动到下一个位置
			currentY += childHeight + margin.Top + margin.Bottom + _spacing;
		}
	}
	else // Horizontal
	{
		// 水平排列
		for (int childIndex = 0; childIndex < container->Count; childIndex++)
		{
			auto child = container->operator[](childIndex);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->MeasureCore({ INT_MAX, (LONG)containerHeight });
			Thickness margin = child->Margin;
			VerticalAlignment verticalAlignment = child->VAlign;
			float availableHeight = containerHeight - margin.Top - margin.Bottom;
			if (availableHeight < 0.0f) availableHeight = 0.0f;
			
			float childHeight = (float)childSize.cy;
			if (verticalAlignment == VerticalAlignment::Stretch)
			{
				childHeight = availableHeight;
			}
			
			float childY = margin.Top;
			if (verticalAlignment == VerticalAlignment::Center)
			{
				childY = margin.Top + (availableHeight - childHeight) / 2.0f;
			}
			else if (verticalAlignment == VerticalAlignment::Bottom)
			{
				childY = containerHeight - margin.Bottom - childHeight;
			}
			
			POINT childLocation = { (LONG)(currentX + margin.Left), (LONG)(originY + childY) };
			float childWidth = (float)childSize.cx;
			SIZE arrangedSize = { (LONG)childWidth, (LONG)childHeight };
			child->ApplyLayout(childLocation, arrangedSize);
			
			// 移动到下一个位置
			currentX += childWidth + margin.Left + margin.Right + _spacing;
		}
	}
	
	_needsLayout = false;
}

// StackPanel 实现

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
