#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <vector>
#include <algorithm>

/**
 * @file GridPanel.h
 * @brief GridPanel：按行/列定义摆放子控件的容器。
 */

/**
 * @brief GridPanel 布局引擎。
 *
 * 支持 Row/Column 的 Pixel/Auto/Star 等策略，并缓存计算后的行高/列宽与起始位置。
 * 子控件通过 Control::GridRow/GridColumn/GridRowSpan/GridColumnSpan 指定单元格位置。
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
    
    void CalculateRowHeights(Control* container, float availableHeight);
    void CalculateColumnWidths(Control* container, float availableWidth);
    
public:
    const std::vector<RowDefinition>& GetRows() const { return _rowDefinitions; }
    const std::vector<ColumnDefinition>& GetColumns() const { return _columnDefinitions; }

    void AddRow(const RowDefinition& row) { 
        _rowDefinitions.push_back(row); 
        Invalidate(); 
    }
    
    void AddColumn(const ColumnDefinition& col) { 
        _columnDefinitions.push_back(col); 
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

	// 根据当前容器尺寸与行列定义，将点映射到单元格索引。
	// x/y 为容器本地坐标（0,0 在 GridPanel 左上角）。
    /**
     * @brief 将容器本地坐标映射为 Grid 单元格索引。
     * @param container GridPanel 容器。
     * @param x 容器本地 X（像素）。
     * @param y 容器本地 Y（像素）。
     * @param outRow 输出行索引。
     * @param outCol 输出列索引。
     * @return true 表示命中有效单元格。
     */
	bool TryGetCellAtPoint(Control* container, float x, float y, int& outRow, int& outCol);
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

/**
 * @brief GridPanel 控件类。
 *
 * - 通过 AddRow/AddColumn 配置行列定义
 * - 子控件通过 GridRow/GridColumn/GridRowSpan/GridColumnSpan 指定占用区域
 */
class GridPanel : public Panel {
private:
    GridLayoutEngine* _gridEngine;
    
public:
    GridPanel();
    GridPanel(int x, int y, int width, int height);
    virtual ~GridPanel();
    
    UIClass Type() override { return UIClass::UI_GridPanel; }
    
    /** @brief 添加一行定义。 */
    void AddRow(GridLength height, float minHeight = 0.0f, float maxHeight = FLT_MAX) {
        RowDefinition row(height, minHeight, maxHeight);
        _gridEngine->AddRow(row);
    }
    
    /** @brief 添加一列定义。 */
    void AddColumn(GridLength width, float minWidth = 0.0f, float maxWidth = FLT_MAX) {
        ColumnDefinition col(width, minWidth, maxWidth);
        _gridEngine->AddColumn(col);
    }
    
    /** @brief 清空所有行定义。 */
    void ClearRows() {
        _gridEngine->ClearRows();
    }
    
    /** @brief 清空所有列定义。 */
    void ClearColumns() {
        _gridEngine->ClearColumns();
    }

    const std::vector<RowDefinition>& GetRows() const { return _gridEngine->GetRows(); }
    const std::vector<ColumnDefinition>& GetColumns() const { return _gridEngine->GetColumns(); }

    /**
     * @brief 将本地坐标映射为单元格索引。
     * @param local 以 GridPanel 左上角为原点的坐标。
     */
    bool TryGetCellAtPoint(POINT local, int& outRow, int& outCol) {
        return _gridEngine->TryGetCellAtPoint(this, (float)local.x, (float)local.y, outRow, outCol);
    }
};
