#include "GridPanel.h"
#include "../Form.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

// GridLayoutEngine 实现

void GridLayoutEngine::CalculateColumnWidths(Control* container, float availableWidth)
{
	if (_columnDefinitions.empty())
	{
		_columnDefinitions.push_back(ColumnDefinition(GridLength::Star(1.0f)));
	}
	
	size_t colCount = _columnDefinitions.size();
	_columnWidths.resize(colCount, 0.0f);
	_columnPositions.resize(colCount + 1, 0.0f);
	
	float totalFixed = 0.0f;
	float totalStar = 0.0f;
	
	// 第一遍：计算固定尺寸（Pixel）
	for (size_t i = 0; i < colCount; i++)
	{
		auto& colDef = _columnDefinitions[i];
		float minW = colDef.MinWidth;
		float maxW = colDef.MaxWidth;
		if (!std::isfinite(minW) || minW < 0.f) minW = 0.f;
		if (!std::isfinite(maxW) || maxW < 0.f || maxW < minW) maxW = FLT_MAX;
		colDef.MinWidth = minW;
		colDef.MaxWidth = maxW;

		if (colDef.Width.IsPixel())
		{
			float width = colDef.Width.Value;
			width = (std::max)(width, colDef.MinWidth);
			width = (std::min)(width, colDef.MaxWidth);
			_columnWidths[i] = width;
			totalFixed += width;
		}
		else if (colDef.Width.IsStar())
		{
			totalStar += colDef.Width.Value;
		}
	}
	
	// 第二遍：计算 Auto 尺寸（根据子控件内容）
	for (size_t i = 0; i < colCount; i++)
	{
		auto& colDef = _columnDefinitions[i];
		if (colDef.Width.IsAuto())
		{
			float maxWidth = colDef.MinWidth;
			
			// 遍历所有在此列的子控件
			if (container)
			{
				for (int j = 0; j < container->Count; j++)
				{
					auto child = container->operator[](j);
					if (!child || !child->Visible) continue;
					
					int childCol = child->GridColumn;
					int childColSpan = child->GridColumnSpan;
					
					if (childCol == (int)i && childColSpan == 1)
					{
						SIZE childSize = child->MeasureCore({INT_MAX, INT_MAX});
						Thickness margin = child->Margin;
						float childWidth = childSize.cx + margin.Left + margin.Right;
						maxWidth = (std::max)(maxWidth, childWidth);
					}
				}
			}
			
			maxWidth = (std::min)(maxWidth, colDef.MaxWidth);
			_columnWidths[i] = maxWidth;
			totalFixed += maxWidth;
		}
	}
	
	// 第三遍：按比例分配剩余空间给 Star 列
	float remainingWidth = availableWidth - totalFixed;
	if (remainingWidth < 0) remainingWidth = 0;
	
	if (totalStar > 0)
	{
		float starUnit = remainingWidth / totalStar;
		for (size_t i = 0; i < colCount; i++)
		{
			auto& colDef = _columnDefinitions[i];
			if (colDef.Width.IsStar())
			{
				float width = starUnit * colDef.Width.Value;
				width = (std::max)(width, colDef.MinWidth);
				width = (std::min)(width, colDef.MaxWidth);
				_columnWidths[i] = width;
			}
		}
	}
	
	// 计算列起始位置
	_columnPositions[0] = 0.0f;
	for (size_t i = 0; i < colCount; i++)
	{
		_columnPositions[i + 1] = _columnPositions[i] + _columnWidths[i];
	}
}

void GridLayoutEngine::CalculateRowHeights(Control* container, float availableHeight)
{
	if (_rowDefinitions.empty())
	{
		_rowDefinitions.push_back(RowDefinition(GridLength::Star(1.0f)));
	}
	
	size_t rowCount = _rowDefinitions.size();
	_rowHeights.resize(rowCount, 0.0f);
	_rowPositions.resize(rowCount + 1, 0.0f);
	
	float totalFixed = 0.0f;
	float totalStar = 0.0f;
	
	// 第一遍：计算固定尺寸（Pixel）
	for (size_t i = 0; i < rowCount; i++)
	{
		auto& rowDef = _rowDefinitions[i];
		float minH = rowDef.MinHeight;
		float maxH = rowDef.MaxHeight;
		if (!std::isfinite(minH) || minH < 0.f) minH = 0.f;
		if (!std::isfinite(maxH) || maxH < 0.f || maxH < minH) maxH = FLT_MAX;
		rowDef.MinHeight = minH;
		rowDef.MaxHeight = maxH;

		if (rowDef.Height.IsPixel())
		{
			float height = rowDef.Height.Value;
			height = (std::max)(height, rowDef.MinHeight);
			height = (std::min)(height, rowDef.MaxHeight);
			_rowHeights[i] = height;
			totalFixed += height;
		}
		else if (rowDef.Height.IsStar())
		{
			totalStar += rowDef.Height.Value;
		}
	}
	
	// 第二遍：计算 Auto 尺寸（根据子控件内容）
	for (size_t i = 0; i < rowCount; i++)
	{
		auto& rowDef = _rowDefinitions[i];
		if (rowDef.Height.IsAuto())
		{
			float maxHeight = rowDef.MinHeight;
			
			// 遍历所有在此行的子控件
			if (container)
			{
				for (int j = 0; j < container->Count; j++)
				{
					auto child = container->operator[](j);
					if (!child || !child->Visible) continue;
					
					int childRow = child->GridRow;
					int childRowSpan = child->GridRowSpan;
					
					if (childRow == (int)i && childRowSpan == 1)
					{
						SIZE childSize = child->MeasureCore({INT_MAX, INT_MAX});
						Thickness margin = child->Margin;
						float childHeight = childSize.cy + margin.Top + margin.Bottom;
						maxHeight = (std::max)(maxHeight, childHeight);
					}
				}
			}
			
			maxHeight = (std::min)(maxHeight, rowDef.MaxHeight);
			_rowHeights[i] = maxHeight;
			totalFixed += maxHeight;
		}
	}
	
	// 第三遍：按比例分配剩余空间给 Star 行
	float remainingHeight = availableHeight - totalFixed;
	if (remainingHeight < 0) remainingHeight = 0;
	
	if (totalStar > 0)
	{
		float starUnit = remainingHeight / totalStar;
		for (size_t i = 0; i < rowCount; i++)
		{
			auto& rowDef = _rowDefinitions[i];
			if (rowDef.Height.IsStar())
			{
				float height = starUnit * rowDef.Height.Value;
				height = (std::max)(height, rowDef.MinHeight);
				height = (std::min)(height, rowDef.MaxHeight);
				_rowHeights[i] = height;
			}
		}
	}
	
	// 计算行起始位置
	_rowPositions[0] = 0.0f;
	for (size_t i = 0; i < rowCount; i++)
	{
		_rowPositions[i + 1] = _rowPositions[i] + _rowHeights[i];
	}
}

SIZE GridLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return {0, 0};
	
	CalculateColumnWidths(container, (float)availableSize.cx);
	CalculateRowHeights(container, (float)availableSize.cy);
	
	// 计算总尺寸
	float totalWidth = 0.0f;
	for (float w : _columnWidths)
		totalWidth += w;
	
	float totalHeight = 0.0f;
	for (float h : _rowHeights)
		totalHeight += h;
	
	_needsLayout = false;
	return { (LONG)totalWidth, (LONG)totalHeight };
}

bool GridLayoutEngine::TryGetCellAtPoint(Control* container, float x, float y, int& outRow, int& outCol)
{
	outRow = 0;
	outCol = 0;
	if (!container) return false;

	// Panel 在 Arrange 时会把 finalRect 设置为内容区（即加上 Padding 偏移），
	// 这里的 x/y 约定为容器本地坐标（0,0 在 GridPanel 左上角），因此需要扣掉 Padding。
	Thickness padding = container->Padding;
	x -= padding.Left;
	y -= padding.Top;

	// 使用当前容器内容区尺寸计算（与 Arrange 一致）
	auto size = container->Size;
	float contentW = (float)size.cx - padding.Left - padding.Right;
	float contentH = (float)size.cy - padding.Top - padding.Bottom;
	if (contentW < 0.0f) contentW = 0.0f;
	if (contentH < 0.0f) contentH = 0.0f;
	CalculateColumnWidths(container, contentW);
	CalculateRowHeights(container, contentH);

	if (_columnPositions.size() < 2 || _rowPositions.size() < 2) return false;

	// Clamp 到有效区域
	if (x < 0.0f) x = 0.0f;
	if (y < 0.0f) y = 0.0f;
	float maxX = _columnPositions.back();
	float maxY = _rowPositions.back();
	if (x > maxX) x = maxX;
	if (y > maxY) y = maxY;

	// 找列
	outCol = (int)_columnWidths.size() - 1;
	for (size_t i = 0; i + 1 < _columnPositions.size(); i++)
	{
		if (x >= _columnPositions[i] && x <= _columnPositions[i + 1])
		{
			outCol = (int)i;
			break;
		}
	}

	// 找行
	outRow = (int)_rowHeights.size() - 1;
	for (size_t i = 0; i + 1 < _rowPositions.size(); i++)
	{
		if (y >= _rowPositions[i] && y <= _rowPositions[i + 1])
		{
			outRow = (int)i;
			break;
		}
	}

	if (outCol < 0) outCol = 0;
	if (outRow < 0) outRow = 0;
	return true;
}

void GridLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	
	float containerWidth = finalRect.right - finalRect.left;
	float containerHeight = finalRect.bottom - finalRect.top;
	
	// 重新计算（如果容器尺寸与测量时不同）
	CalculateColumnWidths(container, containerWidth);
	CalculateRowHeights(container, containerHeight);
	
	// 排列子控件
	for (int i = 0; i < container->Count; i++)
	{
		auto child = container->operator[](i);
		if (!child || !child->Visible) continue;
		
		int row = child->GridRow;
		int col = child->GridColumn;
		int rowSpan = child->GridRowSpan;
		int colSpan = child->GridColumnSpan;
		
		// 确保行列索引有效
		if (row < 0) row = 0;
		if (col < 0) col = 0;
		if (row >= (int)_rowDefinitions.size()) row = (int)_rowDefinitions.size() - 1;
		if (col >= (int)_columnDefinitions.size()) col = (int)_columnDefinitions.size() - 1;
		if (rowSpan < 1) rowSpan = 1;
		if (colSpan < 1) colSpan = 1;
		
		// 计算单元格区域
		float cellX = finalRect.left + _columnPositions[col];
		float cellY = finalRect.top + _rowPositions[row];
		float cellWidth = 0.0f;
		float cellHeight = 0.0f;
		
		// 计算跨列宽度
		for (int c = col; c < col + colSpan && c < (int)_columnWidths.size(); c++)
		{
			cellWidth += _columnWidths[c];
		}
		
		// 计算跨行高度
		for (int r = row; r < row + rowSpan && r < (int)_rowHeights.size(); r++)
		{
			cellHeight += _rowHeights[r];
		}
		
		// 应用边距
		Thickness margin = child->Margin;
		float contentX = cellX + margin.Left;
		float contentY = cellY + margin.Top;
		float contentWidth = cellWidth - margin.Left - margin.Right;
		float contentHeight = cellHeight - margin.Top - margin.Bottom;
		
		if (contentWidth < 0) contentWidth = 0;
		if (contentHeight < 0) contentHeight = 0;
		
		// 应用对齐
		HorizontalAlignment hAlign = child->HAlign;
		VerticalAlignment vAlign = child->VAlign;
		
		SIZE childSize = child->Size;
		float finalWidth = (float)childSize.cx;
		float finalHeight = (float)childSize.cy;
		
		// 水平对齐
		if (hAlign == HorizontalAlignment::Stretch)
		{
			finalWidth = contentWidth;
		}
		else
		{
			if (finalWidth > contentWidth) finalWidth = contentWidth;
			
			if (hAlign == HorizontalAlignment::Center)
			{
				contentX += (contentWidth - finalWidth) / 2.0f;
			}
			else if (hAlign == HorizontalAlignment::Right)
			{
				contentX += contentWidth - finalWidth;
			}
		}
		
		// 垂直对齐
		if (vAlign == VerticalAlignment::Stretch)
		{
			finalHeight = contentHeight;
		}
		else
		{
			if (finalHeight > contentHeight) finalHeight = contentHeight;
			
			if (vAlign == VerticalAlignment::Center)
			{
				contentY += (contentHeight - finalHeight) / 2.0f;
			}
			else if (vAlign == VerticalAlignment::Bottom)
			{
				contentY += contentHeight - finalHeight;
			}
		}
		
		// 应用布局
		POINT loc = { (LONG)contentX, (LONG)contentY };
		SIZE size = { (LONG)finalWidth, (LONG)finalHeight };
		child->ApplyLayout(loc, size);
	}
	
	_needsLayout = false;
}

// GridPanel 实现

GridPanel::GridPanel()
{
	_gridEngine = new GridLayoutEngine();
	SetLayoutEngine(_gridEngine);
}

GridPanel::GridPanel(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	_gridEngine = new GridLayoutEngine();
	SetLayoutEngine(_gridEngine);
}

GridPanel::~GridPanel()
{
	// _gridEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
