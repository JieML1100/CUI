#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <map>
#include <vector>

/**
 * @file RelativePanel.h
 * @brief RelativePanel：通过相对约束进行定位的容器。
 */

/**
 * @brief 相对定位约束。
 *
 * 该结构描述 child 与其它兄弟控件/面板边界之间的关系。
 * 注意：若约束存在循环依赖，布局引擎会尝试检测并避免无穷递归（实现中采用拓扑排序）。
 */
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

/**
 * @brief RelativePanel 布局引擎。
 *
 * - 约束以 child 为 key 进行存储
 * - 排列阶段会根据依赖关系进行拓扑排序
 */
class RelativeLayoutEngine : public LayoutEngine {
private:
    std::map<Control*, RelativeConstraints> _constraints;
    
    // 拓扑排序以解决依赖关系
    std::vector<Control*> TopologicalSort(Control* container);
    bool HasCycle(Control* start, Control* current, std::map<Control*, int>& visited);
    
public:
    /** @brief 设置某个子控件的相对约束。 */
    void SetConstraints(Control* child, const RelativeConstraints& constraints) {
        _constraints[child] = constraints;
        Invalidate();
    }
    
    /**
     * @brief 获取某个子控件的相对约束。
     * @return 不存在时返回 nullptr。
     */
    RelativeConstraints* GetConstraints(Control* child) {
        auto it = _constraints.find(child);
        if (it != _constraints.end())
            return &(it->second);
        return nullptr;
    }
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

/**
 * @brief RelativePanel 控件类。
 */
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
