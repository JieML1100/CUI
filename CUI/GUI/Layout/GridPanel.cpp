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
	
	size_t columnCount = _columnDefinitions.size();
	_columnWidths.resize(columnCount, 0.0f);
	_columnPositions.resize(columnCount + 1, 0.0f);
	
	float totalFixed = 0.0f;
	float totalStar = 0.0f;
	
	// 第一遍：计算固定尺寸（Pixel）
	for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
	{
		auto& columnDefinition = _columnDefinitions[columnIndex];
		float minWidth = columnDefinition.MinWidth;
		float maxWidth = columnDefinition.MaxWidth;
		if (!std::isfinite(minWidth) || minWidth < 0.f) minWidth = 0.f;
		if (!std::isfinite(maxWidth) || maxWidth < 0.f || maxWidth < minWidth) maxWidth = FLT_MAX;
		columnDefinition.MinWidth = minWidth;
		columnDefinition.MaxWidth = maxWidth;

		if (columnDefinition.Width.IsPixel())
		{
			float width = columnDefinition.Width.Value;
			width = (std::max)(width, columnDefinition.MinWidth);
			width = (std::min)(width, columnDefinition.MaxWidth);
			_columnWidths[columnIndex] = width;
			totalFixed += width;
		}
		else if (columnDefinition.Width.IsStar())
		{
			totalStar += columnDefinition.Width.Value;
		}
	}
	
	// 第二遍：计算 Auto 尺寸（根据子控件内容）
	for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
	{
		auto& columnDefinition = _columnDefinitions[columnIndex];
		if (columnDefinition.Width.IsAuto())
		{
			float maxWidth = columnDefinition.MinWidth;
			
			// 遍历所有在此列的子控件
			if (container)
			{
				for (int childIndex = 0; childIndex < container->Count; childIndex++)
				{
					auto child = container->operator[](childIndex);
					if (!child || !child->Visible) continue;
					
					int childColumn = child->GridColumn;
					int childColumnSpan = child->GridColumnSpan;
					
					if (childColumn == (int)columnIndex && childColumnSpan == 1)
					{
						SIZE childSize = child->MeasureCore({INT_MAX, INT_MAX});
						Thickness margin = child->Margin;
						float childWidth = childSize.cx + margin.Left + margin.Right;
						maxWidth = (std::max)(maxWidth, childWidth);
					}
				}
			}
			
			maxWidth = (std::min)(maxWidth, columnDefinition.MaxWidth);
			_columnWidths[columnIndex] = maxWidth;
			totalFixed += maxWidth;
		}
	}
	
	// 第三遍：按比例分配剩余空间给 Star 列
	float remainingWidth = availableWidth - totalFixed;
	if (remainingWidth < 0) remainingWidth = 0;
	
	if (totalStar > 0)
	{
		float starUnit = remainingWidth / totalStar;
		for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
		{
			auto& columnDefinition = _columnDefinitions[columnIndex];
			if (columnDefinition.Width.IsStar())
			{
				float width = starUnit * columnDefinition.Width.Value;
				width = (std::max)(width, columnDefinition.MinWidth);
				width = (std::min)(width, columnDefinition.MaxWidth);
				_columnWidths[columnIndex] = width;
			}
		}
	}
	
	// 计算列起始位置
	_columnPositions[0] = 0.0f;
	for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
	{
		_columnPositions[columnIndex + 1] = _columnPositions[columnIndex] + _columnWidths[columnIndex];
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
	for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		auto& rowDefinition = _rowDefinitions[rowIndex];
		float minHeight = rowDefinition.MinHeight;
		float maxHeight = rowDefinition.MaxHeight;
		if (!std::isfinite(minHeight) || minHeight < 0.f) minHeight = 0.f;
		if (!std::isfinite(maxHeight) || maxHeight < 0.f || maxHeight < minHeight) maxHeight = FLT_MAX;
		rowDefinition.MinHeight = minHeight;
		rowDefinition.MaxHeight = maxHeight;

		if (rowDefinition.Height.IsPixel())
		{
			float height = rowDefinition.Height.Value;
			height = (std::max)(height, rowDefinition.MinHeight);
			height = (std::min)(height, rowDefinition.MaxHeight);
			_rowHeights[rowIndex] = height;
			totalFixed += height;
		}
		else if (rowDefinition.Height.IsStar())
		{
			totalStar += rowDefinition.Height.Value;
		}
	}
	
	// 第二遍：计算 Auto 尺寸（根据子控件内容）
	for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		auto& rowDefinition = _rowDefinitions[rowIndex];
		if (rowDefinition.Height.IsAuto())
		{
			float maxHeight = rowDefinition.MinHeight;
			
			// 遍历所有在此行的子控件
			if (container)
			{
				for (int childIndex = 0; childIndex < container->Count; childIndex++)
				{
					auto child = container->operator[](childIndex);
					if (!child || !child->Visible) continue;
					
					int childRow = child->GridRow;
					int childRowSpan = child->GridRowSpan;
					
					if (childRow == (int)rowIndex && childRowSpan == 1)
					{
						SIZE childSize = child->MeasureCore({INT_MAX, INT_MAX});
						Thickness margin = child->Margin;
						float childHeight = childSize.cy + margin.Top + margin.Bottom;
						maxHeight = (std::max)(maxHeight, childHeight);
					}
				}
			}
			
			maxHeight = (std::min)(maxHeight, rowDefinition.MaxHeight);
			_rowHeights[rowIndex] = maxHeight;
			totalFixed += maxHeight;
		}
	}
	
	// 第三遍：按比例分配剩余空间给 Star 行
	float remainingHeight = availableHeight - totalFixed;
	if (remainingHeight < 0) remainingHeight = 0;
	
	if (totalStar > 0)
	{
		float starUnit = remainingHeight / totalStar;
		for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++)
		{
			auto& rowDefinition = _rowDefinitions[rowIndex];
			if (rowDefinition.Height.IsStar())
			{
				float height = starUnit * rowDefinition.Height.Value;
				height = (std::max)(height, rowDefinition.MinHeight);
				height = (std::min)(height, rowDefinition.MaxHeight);
				_rowHeights[rowIndex] = height;
			}
		}
	}
	
	// 计算行起始位置
	_rowPositions[0] = 0.0f;
	for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		_rowPositions[rowIndex + 1] = _rowPositions[rowIndex] + _rowHeights[rowIndex];
	}
}

SIZE GridLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return {0, 0};
	
	CalculateColumnWidths(container, (float)availableSize.cx);
	CalculateRowHeights(container, (float)availableSize.cy);
	
	// 计算总尺寸
	float totalWidth = 0.0f;
	for (float columnWidth : _columnWidths)
		totalWidth += columnWidth;
	
	float totalHeight = 0.0f;
	for (float rowHeight : _rowHeights)
		totalHeight += rowHeight;
	
	_needsLayout = false;
	return { (LONG)totalWidth, (LONG)totalHeight };
}

bool GridLayoutEngine::TryGetCellAtPoint(Control* container, float localX, float localY, int& outRow, int& outCol)
{
	outRow = 0;
	outCol = 0;
	if (!container) return false;

	// Panel 在 Arrange 时会把 finalRect 设置为内容区（即加上 Padding 偏移），
	// 这里的 localX/localY 约定为容器本地坐标（0,0 在 GridPanel 左上角），因此需要扣掉 Padding。
	Thickness padding = container->Padding;
	localX -= padding.Left;
	localY -= padding.Top;

	// 使用当前容器内容区尺寸计算（与 Arrange 一致）
	auto containerSize = container->Size;
	float contentWidth = (float)containerSize.cx - padding.Left - padding.Right;
	float contentHeight = (float)containerSize.cy - padding.Top - padding.Bottom;
	if (contentWidth < 0.0f) contentWidth = 0.0f;
	if (contentHeight < 0.0f) contentHeight = 0.0f;
	CalculateColumnWidths(container, contentWidth);
	CalculateRowHeights(container, contentHeight);

	if (_columnPositions.size() < 2 || _rowPositions.size() < 2) return false;

	// Clamp 到有效区域
	if (localX < 0.0f) localX = 0.0f;
	if (localY < 0.0f) localY = 0.0f;
	float maxLocalX = _columnPositions.back();
	float maxLocalY = _rowPositions.back();
	if (localX > maxLocalX) localX = maxLocalX;
	if (localY > maxLocalY) localY = maxLocalY;

	// 找列
	outCol = (int)_columnWidths.size() - 1;
	for (size_t columnIndex = 0; columnIndex + 1 < _columnPositions.size(); columnIndex++)
	{
		if (localX >= _columnPositions[columnIndex] && localX <= _columnPositions[columnIndex + 1])
		{
			outCol = (int)columnIndex;
			break;
		}
	}

	// 找行
	outRow = (int)_rowHeights.size() - 1;
	for (size_t rowIndex = 0; rowIndex + 1 < _rowPositions.size(); rowIndex++)
	{
		if (localY >= _rowPositions[rowIndex] && localY <= _rowPositions[rowIndex + 1])
		{
			outRow = (int)rowIndex;
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
	for (int childIndex = 0; childIndex < container->Count; childIndex++)
	{
		auto child = container->operator[](childIndex);
		if (!child || !child->Visible) continue;
		
		int row = child->GridRow;
		int column = child->GridColumn;
		int rowSpan = child->GridRowSpan;
		int columnSpan = child->GridColumnSpan;
		
		// 确保行列索引有效
		if (row < 0) row = 0;
		if (column < 0) column = 0;
		if (row >= (int)_rowDefinitions.size()) row = (int)_rowDefinitions.size() - 1;
		if (column >= (int)_columnDefinitions.size()) column = (int)_columnDefinitions.size() - 1;
		if (rowSpan < 1) rowSpan = 1;
		if (columnSpan < 1) columnSpan = 1;
		
		// 计算单元格区域
		float cellX = finalRect.left + _columnPositions[column];
		float cellY = finalRect.top + _rowPositions[row];
		float cellWidth = 0.0f;
		float cellHeight = 0.0f;
		
		// 计算跨列宽度
		for (int columnIndex = column; columnIndex < column + columnSpan && columnIndex < (int)_columnWidths.size(); columnIndex++)
		{
			cellWidth += _columnWidths[columnIndex];
		}
		
		// 计算跨行高度
		for (int rowIndex = row; rowIndex < row + rowSpan && rowIndex < (int)_rowHeights.size(); rowIndex++)
		{
			cellHeight += _rowHeights[rowIndex];
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
		HorizontalAlignment horizontalAlignment = child->HAlign;
		VerticalAlignment verticalAlignment = child->VAlign;
		SIZE childSize = child->MeasureCore({ (LONG)cellWidth, (LONG)cellHeight });
		float finalWidth = (float)childSize.cx;
		float finalHeight = (float)childSize.cy;
		
		// 水平对齐
		if (horizontalAlignment == HorizontalAlignment::Stretch)
		{
			contentX = cellX + margin.Left;
			finalWidth = contentWidth;
		}
		else
		{
			if (horizontalAlignment == HorizontalAlignment::Center)
			{
				contentX = cellX + margin.Left + (contentWidth - finalWidth) / 2.0f;
			}
			else if (horizontalAlignment == HorizontalAlignment::Right)
			{
				contentX = cellX + cellWidth - margin.Right - finalWidth;
			}
		}
		
		// 垂直对齐
		if (verticalAlignment == VerticalAlignment::Stretch)
		{
			contentY = cellY + margin.Top;
			finalHeight = contentHeight;
		}
		else
		{
			if (verticalAlignment == VerticalAlignment::Center)
			{
				contentY = cellY + margin.Top + (contentHeight - finalHeight) / 2.0f;
			}
			else if (verticalAlignment == VerticalAlignment::Bottom)
			{
				contentY = cellY + cellHeight - margin.Bottom - finalHeight;
			}
		}
		
		// 应用布局
		POINT finalLocation = { (LONG)contentX, (LONG)contentY };
		SIZE finalSize = { (LONG)finalWidth, (LONG)finalHeight };
		child->ApplyLayout(finalLocation, finalSize);
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
