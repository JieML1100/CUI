#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <vector>
#include <algorithm>

// GridPanel 布局引擎
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
	bool TryGetCellAtPoint(Control* container, float x, float y, int& outRow, int& outCol);
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

// GridPanel 控件类
class GridPanel : public Panel {
private:
    GridLayoutEngine* _gridEngine;
    
public:
    GridPanel();
    GridPanel(int x, int y, int width, int height);
    virtual ~GridPanel();
    
    UIClass Type() override { return UIClass::UI_GridPanel; }
    
    // 行列管理
    void AddRow(GridLength height, float minHeight = 0.0f, float maxHeight = FLT_MAX) {
        RowDefinition row(height, minHeight, maxHeight);
        _gridEngine->AddRow(row);
    }
    
    void AddColumn(GridLength width, float minWidth = 0.0f, float maxWidth = FLT_MAX) {
        ColumnDefinition col(width, minWidth, maxWidth);
        _gridEngine->AddColumn(col);
    }
    
    void ClearRows() {
        _gridEngine->ClearRows();
    }
    
    void ClearColumns() {
        _gridEngine->ClearColumns();
    }

    const std::vector<RowDefinition>& GetRows() const { return _gridEngine->GetRows(); }
    const std::vector<ColumnDefinition>& GetColumns() const { return _gridEngine->GetColumns(); }

    bool TryGetCellAtPoint(POINT local, int& outRow, int& outCol) {
        return _gridEngine->TryGetCellAtPoint(this, (float)local.x, (float)local.y, outRow, outCol);
    }
};
