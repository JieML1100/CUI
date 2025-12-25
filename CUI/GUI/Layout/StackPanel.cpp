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
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
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
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
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
	
	if (_orientation == Orientation::Vertical)
	{
		// 垂直排列
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->Size;
			Thickness margin = child->Margin;
			HorizontalAlignment hAlign = child->HAlign;
			
			// 计算实际宽度（考虑对齐方式）
			float childWidth = (float)childSize.cx;
			if (hAlign == HorizontalAlignment::Stretch)
			{
				childWidth = containerWidth - margin.Left - margin.Right;
			}
			
			// 计算 X 位置（根据水平对齐）
			float childX = margin.Left;
			if (hAlign == HorizontalAlignment::Center)
			{
				childX = (containerWidth - childWidth) / 2.0f;
			}
			else if (hAlign == HorizontalAlignment::Right)
			{
				childX = containerWidth - childWidth - margin.Right;
			}
			
			// 设置位置和尺寸
			POINT loc = { (LONG)(originX + childX), (LONG)(currentY + margin.Top) };
			SIZE size = { (LONG)childWidth, childSize.cy };
			child->ApplyLayout(loc, size);
			
			// 移动到下一个位置
			currentY += childSize.cy + margin.Top + margin.Bottom + _spacing;
		}
	}
	else // Horizontal
	{
		// 水平排列
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->Size;
			Thickness margin = child->Margin;
			VerticalAlignment vAlign = child->VAlign;
			
			// 计算实际高度（考虑对齐方式）
			float childHeight = (float)childSize.cy;
			if (vAlign == VerticalAlignment::Stretch)
			{
				childHeight = containerHeight - margin.Top - margin.Bottom;
			}
			
			// 计算 Y 位置（根据垂直对齐）
			float childY = margin.Top;
			if (vAlign == VerticalAlignment::Center)
			{
				childY = (containerHeight - childHeight) / 2.0f;
			}
			else if (vAlign == VerticalAlignment::Bottom)
			{
				childY = containerHeight - childHeight - margin.Bottom;
			}
			
			// 设置位置和尺寸
			POINT loc = { (LONG)(currentX + margin.Left), (LONG)(originY + childY) };
			SIZE size = { childSize.cx, (LONG)childHeight };
			child->ApplyLayout(loc, size);
			
			// 移动到下一个位置
			currentX += childSize.cx + margin.Left + margin.Right + _spacing;
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
