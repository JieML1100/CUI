#pragma once
#include <cstdint>
#include <float.h>

/**
 * @file LayoutTypes.h
 * @brief CUI 布局系统使用的基础类型与枚举。
 *
 * 这些类型用于描述控件在容器中的排布规则：对齐、停靠、锚点、边距、Grid 行列定义等。
 * 坐标/尺寸单位通常为像素；部分类型支持 Auto/Star/Percent 等策略（见 SizeUnit）。
 */

/** @brief 布局方向（主轴方向）。 */
enum class Orientation : uint8_t {
    Horizontal,
    Vertical
};

/** @brief 水平对齐方式。 */
enum class HorizontalAlignment : uint8_t {
    Left,
    Center,
    Right,
    Stretch
};

/** @brief 垂直对齐方式。 */
enum class VerticalAlignment : uint8_t {
    Top,
    Center,
    Bottom,
    Stretch
};

/** @brief 停靠位置（DockPanel 等使用）。 */
enum class Dock : uint8_t {
    Left,
    Top,
    Right,
    Bottom,
    Fill
};

/**
 * @brief 锚点标志位（可组合）。
 *
 * 用于在父容器尺寸变化时保持与对应边的距离，常见于 Anchor/Margin 布局。
 */
enum AnchorStyles : uint8_t {
    None = 0,
    Left = 1,
    Top = 2,
    Right = 4,
    Bottom = 8
};

/**
 * @brief 尺寸单位/策略。
 *
 * - Pixel：固定像素
 * - Percent：按可用空间百分比
 * - Auto：根据内容/子元素测量决定
 * - Star：按比例分配（常用于 Grid）
 */
enum class SizeUnit : uint8_t {
    Pixel,      // 像素
    Percent,    // 百分比
    Auto,       // 自动
    Star        // 星号(*)，用于Grid的比例分配
};

/**
 * @brief 边距/内边距结构。
 *
 * 约定：四个方向均为非负像素值（若出现负值，其行为由具体布局引擎决定）。
 */
struct Thickness {
    float Left, Top, Right, Bottom;
    
    Thickness(float all = 0.0f) 
        : Left(all), Top(all), Right(all), Bottom(all) {}
    
    Thickness(float horizontal, float vertical) 
        : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical) {}
    
    Thickness(float left, float top, float right, float bottom) 
        : Left(left), Top(top), Right(right), Bottom(bottom) {}
    
    bool operator==(const Thickness& other) const {
        return Left == other.Left && Top == other.Top && 
               Right == other.Right && Bottom == other.Bottom;
    }
    
    bool operator!=(const Thickness& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Grid 行/列的尺寸定义。
 *
 * Value 与 Unit 共同定义高度/宽度：
 * - Pixel：Value 为像素
 * - Star：Value 为比例因子（默认 1.0）
 * - Auto：由内容决定（Value 通常忽略）
 */
struct GridLength {
    float Value;
    SizeUnit Unit;
    
    GridLength(float value = 0.0f, SizeUnit unit = SizeUnit::Pixel) 
        : Value(value), Unit(unit) {}
    
    static GridLength Auto() { 
        return GridLength(0.0f, SizeUnit::Auto); 
    }
    
    static GridLength Star(float factor = 1.0f) { 
        return GridLength(factor, SizeUnit::Star); 
    }
    
    static GridLength Pixels(float px) { 
        return GridLength(px, SizeUnit::Pixel); 
    }
    
    bool IsAuto() const { return Unit == SizeUnit::Auto; }
    bool IsStar() const { return Unit == SizeUnit::Star; }
    bool IsPixel() const { return Unit == SizeUnit::Pixel; }
};

/** @brief Grid 行定义。 */
struct RowDefinition {
    GridLength Height;
    float MinHeight;
    float MaxHeight;
    
    RowDefinition(GridLength height = GridLength::Auto(), 
                  float minHeight = 0.0f, 
                  float maxHeight = FLT_MAX)
        : Height(height), MinHeight(minHeight), MaxHeight(maxHeight) {}
};

/** @brief Grid 列定义。 */
struct ColumnDefinition {
    GridLength Width;
    float MinWidth;
    float MaxWidth;
    
    ColumnDefinition(GridLength width = GridLength::Auto(), 
                     float minWidth = 0.0f, 
                     float maxWidth = FLT_MAX)
        : Width(width), MinWidth(minWidth), MaxWidth(maxWidth) {}
};
