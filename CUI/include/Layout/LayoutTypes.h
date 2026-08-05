#pragma once
#include <algorithm>
#include <cstdint>
#include <float.h>
#include <Core/Geometry.h>

/**
 * @file LayoutTypes.h
 * @brief CUI 布局系统使用的基础类型与枚举。
 *
 * 这些类型用于描述控件在容器中的排布规则：对齐、停靠、锚点、边距、Grid 行列定义等。
 * 坐标/尺寸统一使用 DIP（96 DPI 设计单位）；Grid 长度遵循 Pixel/Auto/Star 语义。
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

/** WPF content scaling mode shared by ImageBrush, Image and MediaElement. */
enum class Stretch {
    None,
    Fill,
    Uniform,
    UniformToFill
};

/** WPF scaling constraint shared by Image and MediaElement. */
enum class StretchDirection {
    UpOnly,
    DownOnly,
    Both
};

namespace cui::layout
{
/**
 * Computes WPF Viewbox scale factors for an available and natural size.
 * Infinite axes borrow the scale from the constrained axis; Stretch.None
 * always preserves the natural scale regardless of StretchDirection.
 */
inline cui::core::Size ComputeStretchScaleFactor(
    cui::core::Size available,
    cui::core::Size natural,
    ::Stretch stretch,
    ::StretchDirection direction) noexcept
{
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    const bool widthBounded = available.width < cui::core::Infinity;
    const bool heightBounded = available.height < cui::core::Infinity;
    if (stretch != ::Stretch::None && (widthBounded || heightBounded))
    {
        scaleX = natural.width > 0.0f
            ? available.width / natural.width : 0.0f;
        scaleY = natural.height > 0.0f
            ? available.height / natural.height : 0.0f;
        if (!widthBounded) scaleX = scaleY;
        else if (!heightBounded) scaleY = scaleX;
        else if (stretch == ::Stretch::Uniform)
            scaleX = scaleY = (std::min)(scaleX, scaleY);
        else if (stretch == ::Stretch::UniformToFill)
            scaleX = scaleY = (std::max)(scaleX, scaleY);

        if (direction == ::StretchDirection::UpOnly)
        {
            scaleX = (std::max)(scaleX, 1.0f);
            scaleY = (std::max)(scaleY, 1.0f);
        }
        else if (direction == ::StretchDirection::DownOnly)
        {
            scaleX = (std::min)(scaleX, 1.0f);
            scaleY = (std::min)(scaleY, 1.0f);
        }
    }
    return { scaleX, scaleY };
}
}

/** @brief 停靠位置（DockPanel 等使用）。 */
enum class Dock : uint8_t {
    Left,
    Top,
    Right,
    Bottom
};

/**
 * @brief 尺寸单位/策略。
 *
 * - Pixel：固定 DIP
 * - Auto：根据内容/子元素测量决定
 * - Star：按比例分配（用于 Grid）
 */
enum class SizeUnit : uint8_t {
    Pixel,
    Auto,
    Star
};

/**
 * @brief 边距/内边距结构。
 *
 * 约定：四个方向均为非负 DIP（若出现负值，其行为由具体布局引擎决定）。
 */
struct Thickness {
    float Left, Top, Right, Bottom;
    
    constexpr Thickness(float all = 0.0f) noexcept
        : Left(all), Top(all), Right(all), Bottom(all) {}
    
    constexpr Thickness(float horizontal, float vertical) noexcept
        : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical) {}
    
    constexpr Thickness(
        float left, float top, float right, float bottom) noexcept
        : Left(left), Top(top), Right(right), Bottom(bottom) {}
    
    constexpr bool operator==(const Thickness& other) const noexcept {
        return Left == other.Left && Top == other.Top && 
               Right == other.Right && Bottom == other.Bottom;
    }
    
    constexpr bool operator!=(const Thickness& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Returns the widest edge.
     *
     * Native fallback renderers that only support a uniform stroke must opt in
     * to this lossy projection explicitly. The public property system retains
     * all four WPF Thickness components.
     */
    constexpr float MaxEdge() const noexcept {
        return (std::max)(
            (std::max)(Left, Right),
            (std::max)(Top, Bottom));
    }
};

/**
 * @brief WPF CornerRadius value, in top-left, top-right, bottom-right,
 * bottom-left order.
 *
 * Validation belongs to the dependency property that consumes the value.  The
 * value type intentionally preserves every authored component so Border can
 * apply WPF's overlap scaling during rendering.
 */
struct CornerRadius {
    float TopLeft, TopRight, BottomRight, BottomLeft;

    constexpr CornerRadius(float uniformRadius = 0.0f) noexcept
        : TopLeft(uniformRadius), TopRight(uniformRadius),
          BottomRight(uniformRadius), BottomLeft(uniformRadius) {}

    constexpr CornerRadius(
        float topLeft,
        float topRight,
        float bottomRight,
        float bottomLeft) noexcept
        : TopLeft(topLeft), TopRight(topRight),
          BottomRight(bottomRight), BottomLeft(bottomLeft) {}

    constexpr bool operator==(const CornerRadius& other) const noexcept {
        return TopLeft == other.TopLeft && TopRight == other.TopRight
            && BottomRight == other.BottomRight
            && BottomLeft == other.BottomLeft;
    }

    constexpr bool operator!=(const CornerRadius& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Grid 行/列的尺寸定义。
 *
 * Value 与 Unit 共同定义高度/宽度：
 * - Pixel：Value 为固定 DIP
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
