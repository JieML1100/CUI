#pragma once
#include "Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <vector>
#include <algorithm>
#include <utility>

/**
 * @file Grid.h
 * @brief Grid：按行/列定义摆放子控件的容器。
 */

/**
 * @brief Grid 布局引擎。
 *
 * 支持 Row/Column 的 Pixel/Auto/Star 策略，并缓存计算后的行高/列宽与起始位置。
 * 子控件通过 Grid attached properties 指定单元格位置。
 */
class GridLayoutEngine : public LayoutEngine {
private:
    std::vector<RowDefinition> _rowDefinitions;
    std::vector<ColumnDefinition> _columnDefinitions;
    
    // 缓存计算结果
    std::vector<float> _rowHeights;
    std::vector<float> _columnWidths;
    std::vector<float> _rowPositions;
    std::vector<float> _columnPositions;
    
    void CalculateRowHeights(LayoutContext& context, float availableHeight);
    void CalculateColumnWidths(LayoutContext& context, float availableWidth);
    
public:
    const std::vector<RowDefinition>& GetRows() const { return _rowDefinitions; }
    const std::vector<ColumnDefinition>& GetColumns() const { return _columnDefinitions; }

    void AddRow(const RowDefinition& row) { 
        _rowDefinitions.push_back(row); 
        Invalidate(); 
    }
    
    void AddColumn(const ColumnDefinition& column) {
        _columnDefinitions.push_back(column);
        Invalidate(); 
    }
    
    void ClearRows() {
        _rowDefinitions.clear();
        Invalidate();
    }
    
    void ClearColumns() {
        _columnDefinitions.clear();
        Invalidate();
    }

    void ReplaceColumns(std::vector<ColumnDefinition> columns) {
        _columnDefinitions = std::move(columns);
        Invalidate();
    }

	// 根据当前容器尺寸与行列定义，将点映射到单元格索引。
	// localX/localY 为容器本地坐标（0,0 在 Grid 左上角）。
    /**
     * @brief 将容器本地坐标映射为 Grid 单元格索引。
     * @param container Grid 容器。
     * @param localX 容器本地 X（像素）。
     * @param localY 容器本地 Y（像素）。
     * @param outRow 输出行索引。
     * @param outCol 输出列索引。
     * @return true 表示命中有效单元格。
     */
	bool TryGetCellAtPoint(Control* container, float localX, float localY, int& outRow, int& outCol);
    
    cui::core::Size Measure(LayoutContext& context, const cui::core::Constraints& available) override;
    void Arrange(LayoutContext& context, cui::core::Rect finalRect) override;
};

/**
 * @brief Grid 控件类。
 *
 * - 通过 AddRow/AddColumn 配置行列定义
 * - 子控件通过 Grid::SetRow/SetColumn/SetRowSpan/SetColumnSpan 指定占用区域
 */
class Grid : public Panel {
private:
    GridLayoutEngine* _gridEngine;
    
public:
	Grid();
    virtual ~Grid();
    
	UIClass Type() override { return UIClass::UI_Grid; }

	static int GetRow(Control& element) noexcept
	{
		return element.GetGridRow();
	}
	static void SetRow(Control& element, int value)
	{
		element.SetGridRow(value);
	}
	static int GetColumn(Control& element) noexcept
	{
		return element.GetGridColumn();
	}
	static void SetColumn(Control& element, int value)
	{
		element.SetGridColumn(value);
	}
	static int GetRowSpan(Control& element) noexcept
	{
		return element.GetGridRowSpan();
	}
	static void SetRowSpan(Control& element, int value)
	{
		element.SetGridRowSpan(value);
	}
	static int GetColumnSpan(Control& element) noexcept
	{
		return element.GetGridColumnSpan();
	}
	static void SetColumnSpan(Control& element, int value)
	{
		element.SetGridColumnSpan(value);
	}
    
    /** @brief 添加一行定义。 */
    void AddRow(GridLength height, float minHeight = 0.0f, float maxHeight = FLT_MAX) {
        RowDefinition row(height, minHeight, maxHeight);
        _gridEngine->AddRow(row);
        InvalidateLayout();
    }
    
    /** @brief 添加一列定义。 */
    void AddColumn(GridLength width, float minWidth = 0.0f, float maxWidth = FLT_MAX) {
        ColumnDefinition column(width, minWidth, maxWidth);
        _gridEngine->AddColumn(column);
        InvalidateLayout();
    }
    
    /** @brief 清空所有行定义。 */
    void ClearRows() {
        _gridEngine->ClearRows();
        InvalidateLayout();
    }
    
    /** @brief 清空所有列定义。 */
    void ClearColumns() {
        _gridEngine->ClearColumns();
        InvalidateLayout();
    }

    /** Replace every column definition as one layout transaction. */
    void ReplaceColumns(
        std::vector<ColumnDefinition> columns,
        bool propagateLayoutInvalidation = true) {
        _gridEngine->ReplaceColumns(std::move(columns));
        if (propagateLayoutInvalidation)
            InvalidateLayout();
        else
        {
            // A parent currently inside Measure already owns the root layout
            // transaction.  Mark this Grid locally without asking Window for a
            // redundant second full-frame layout after that transaction ends.
            InvalidateMeasureSubtree();
            _needsMeasure = true;
            _needsArrange = true;
        }
    }

    const std::vector<RowDefinition>& GetRows() const { return _gridEngine->GetRows(); }
    const std::vector<ColumnDefinition>& GetColumns() const { return _gridEngine->GetColumns(); }

    /**
     * @brief 将本地坐标映射为单元格索引。
     * @param local 以 Grid 左上角为原点的坐标。
     */
    bool TryGetCellAtPoint(cui::core::Point local, int& outRow, int& outCol) {
        return _gridEngine->TryGetCellAtPoint(this, local.x, local.y, outRow, outCol);
    }
};
