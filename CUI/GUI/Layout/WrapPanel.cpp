#include "WrapPanel.h"
#include "../Form.h"
#include <algorithm>
#include <vector>

// WrapLayoutEngine 实现

SIZE WrapLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return {0, 0};
	
	SIZE desiredSize = {0, 0};
	
	if (_orientation == Orientation::Horizontal)
	{
		// 水平方向：从左到右排列，超出换行
		float lineWidth = 0.0f;
		float lineHeight = 0.0f;
		float totalHeight = 0.0f;
		float maxLineWidth = 0.0f;
		
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->MeasureCore(availableSize);
			Thickness margin = child->Margin;
			
			float itemWidth = _itemWidth > 0 ? _itemWidth : (float)childSize.cx;
			float itemHeight = _itemHeight > 0 ? _itemHeight : (float)childSize.cy;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换行
			if (lineWidth + totalItemWidth > availableSize.cx && lineWidth > 0)
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
		
		desiredSize.cx = (LONG)maxLineWidth;
		desiredSize.cy = (LONG)totalHeight;
	}
	else // Vertical
	{
		// 垂直方向：从上到下排列，超出换列
		float columnHeight = 0.0f;
		float columnWidth = 0.0f;
		float totalWidth = 0.0f;
		float maxColumnHeight = 0.0f;
		
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->MeasureCore(availableSize);
			Thickness margin = child->Margin;
			
			float itemWidth = _itemWidth > 0 ? _itemWidth : (float)childSize.cx;
			float itemHeight = _itemHeight > 0 ? _itemHeight : (float)childSize.cy;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换列
			if (columnHeight + totalItemHeight > availableSize.cy && columnHeight > 0)
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
		
		desiredSize.cx = (LONG)totalWidth;
		desiredSize.cy = (LONG)maxColumnHeight;
	}
	
	_needsLayout = false;
	return desiredSize;
}

void WrapLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	
	const float originX = finalRect.left;
	const float originY = finalRect.top;
	float containerWidth = finalRect.right - finalRect.left;
	float containerHeight = finalRect.bottom - finalRect.top;
	
	if (_orientation == Orientation::Horizontal)
	{
		// 水平布局：从左到右，自动换行
		float x = 0.0f;
		float y = 0.0f;
		float lineHeight = 0.0f;
		
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->Size;
			Thickness margin = child->Margin;
			
			float itemWidth = _itemWidth > 0 ? _itemWidth : (float)childSize.cx;
			float itemHeight = _itemHeight > 0 ? _itemHeight : (float)childSize.cy;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换行
			if (x + totalItemWidth > containerWidth && x > 0)
			{
				x = 0.0f;
				y += lineHeight;
				lineHeight = 0.0f;
			}
			
			// 设置子控件位置
			POINT loc = { (LONG)(originX + x + margin.Left), (LONG)(originY + y + margin.Top) };
			SIZE size = { (LONG)itemWidth, (LONG)itemHeight };
			child->ApplyLayout(loc, size);
			
			x += totalItemWidth;
			if (totalItemHeight > lineHeight)
				lineHeight = totalItemHeight;
		}
	}
	else // Vertical
	{
		// 垂直布局：从上到下，自动换列
		float x = 0.0f;
		float y = 0.0f;
		float columnWidth = 0.0f;
		
		for (int i = 0; i < container->Count; i++)
		{
			auto child = container->operator[](i);
			if (!child || !child->Visible) continue;
			
			SIZE childSize = child->Size;
			Thickness margin = child->Margin;
			
			float itemWidth = _itemWidth > 0 ? _itemWidth : (float)childSize.cx;
			float itemHeight = _itemHeight > 0 ? _itemHeight : (float)childSize.cy;
			float totalItemWidth = itemWidth + margin.Left + margin.Right;
			float totalItemHeight = itemHeight + margin.Top + margin.Bottom;
			
			// 检查是否需要换列
			if (y + totalItemHeight > containerHeight && y > 0)
			{
				y = 0.0f;
				x += columnWidth;
				columnWidth = 0.0f;
			}
			
			// 设置子控件位置
			POINT loc = { (LONG)(originX + x + margin.Left), (LONG)(originY + y + margin.Top) };
			SIZE size = { (LONG)itemWidth, (LONG)itemHeight };
			child->ApplyLayout(loc, size);
			
			y += totalItemHeight;
			if (totalItemWidth > columnWidth)
				columnWidth = totalItemWidth;
		}
	}
	
	_needsLayout = false;
}

// WrapPanel 实现

WrapPanel::WrapPanel()
{
	_wrapEngine = new WrapLayoutEngine();
	SetLayoutEngine(_wrapEngine);
}

WrapPanel::WrapPanel(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	_wrapEngine = new WrapLayoutEngine();
	SetLayoutEngine(_wrapEngine);
}

WrapPanel::~WrapPanel()
{
	// _wrapEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
