#pragma once
#include <cstdint>
#include <float.h>

// 布局方向
enum class Orientation : uint8_t {
    Horizontal,
    Vertical
};

// 水平对齐
enum class HorizontalAlignment : uint8_t {
    Left,
    Center,
    Right,
    Stretch
};

// 垂直对齐
enum class VerticalAlignment : uint8_t {
    Top,
    Center,
    Bottom,
    Stretch
};

// 停靠位置
enum class Dock : uint8_t {
    Left,
    Top,
    Right,
    Bottom,
    Fill
};

// 锚点标志位
enum AnchorStyles : uint8_t {
    None = 0,
    Left = 1,
    Top = 2,
    Right = 4,
    Bottom = 8
};

// 尺寸单位类型
enum class SizeUnit : uint8_t {
    Pixel,      // 像素
    Percent,    // 百分比
    Auto,       // 自动
    Star        // 星号(*)，用于Grid的比例分配
};

// 边距结构
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

// 尺寸定义(用于Grid行列定义)
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

// 行定义
struct RowDefinition {
    GridLength Height;
    float MinHeight;
    float MaxHeight;
    
    RowDefinition(GridLength height = GridLength::Auto(), 
                  float minHeight = 0.0f, 
                  float maxHeight = FLT_MAX)
        : Height(height), MinHeight(minHeight), MaxHeight(maxHeight) {}
};

// 列定义
struct ColumnDefinition {
    GridLength Width;
    float MinWidth;
    float MaxWidth;
    
    ColumnDefinition(GridLength width = GridLength::Auto(), 
                     float minWidth = 0.0f, 
                     float maxWidth = FLT_MAX)
        : Width(width), MinWidth(minWidth), MaxWidth(maxWidth) {}
};
