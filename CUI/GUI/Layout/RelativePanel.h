#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <map>
#include <vector>

// 相对定位约束
struct RelativeConstraints {
    Control* AlignLeftWith = nullptr;
    Control* AlignRightWith = nullptr;
    Control* AlignTopWith = nullptr;
    Control* AlignBottomWith = nullptr;
    Control* LeftOf = nullptr;
    Control* RightOf = nullptr;
    Control* Above = nullptr;
    Control* Below = nullptr;
    bool AlignLeftWithPanel = false;
    bool AlignRightWithPanel = false;
    bool AlignTopWithPanel = false;
    bool AlignBottomWithPanel = false;
    bool CenterHorizontal = false;
    bool CenterVertical = false;
};

// RelativePanel 布局引擎
class RelativeLayoutEngine : public LayoutEngine {
private:
    std::map<Control*, RelativeConstraints> _constraints;
    
    // 拓扑排序以解决依赖关系
    std::vector<Control*> TopologicalSort(Control* container);
    bool HasCycle(Control* start, Control* current, std::map<Control*, int>& visited);
    
public:
    void SetConstraints(Control* child, const RelativeConstraints& constraints) {
        _constraints[child] = constraints;
        Invalidate();
    }
    
    RelativeConstraints* GetConstraints(Control* child) {
        auto it = _constraints.find(child);
        if (it != _constraints.end())
            return &(it->second);
        return nullptr;
    }
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

// RelativePanel 控件类
class RelativePanel : public Panel {
private:
    RelativeLayoutEngine* _relativeEngine;
    
public:
    RelativePanel();
    RelativePanel(int x, int y, int width, int height);
    virtual ~RelativePanel();
    
    UIClass Type() override { return UIClass::UI_RelativePanel; }
    
    void SetConstraints(Control* child, const RelativeConstraints& constraints) {
        _relativeEngine->SetConstraints(child, constraints);
    }
    
    RelativeConstraints* GetConstraints(Control* child) {
        return _relativeEngine->GetConstraints(child);
    }
};
