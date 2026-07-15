#pragma once

#include <limits>

namespace cui::core {

// All values in this header are device-independent pixels (DIPs). Conversion
// to physical pixels belongs at the platform/window boundary.
using Dip = float;

inline constexpr Dip Infinity = (std::numeric_limits<Dip>::infinity)();

namespace detail {

[[nodiscard]] constexpr bool IsNaN(Dip value) noexcept
{
    return value != value;
}

[[nodiscard]] constexpr Dip Min(Dip left, Dip right) noexcept
{
    return left < right ? left : right;
}

[[nodiscard]] constexpr Dip Max(Dip left, Dip right) noexcept
{
    return left > right ? left : right;
}

[[nodiscard]] constexpr Dip NonNegative(Dip value) noexcept
{
    return IsNaN(value) || value < 0.0f ? 0.0f : value;
}

[[nodiscard]] constexpr Dip Clamp(Dip value, Dip minimum, Dip maximum) noexcept
{
    if (IsNaN(value)) {
        return minimum;
    }
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

} // namespace detail

struct Point final {
    Dip x = 0.0f;
    Dip y = 0.0f;

    constexpr Point() noexcept = default;
    constexpr Point(Dip xValue, Dip yValue) noexcept
        : x(xValue), y(yValue)
    {
    }

    friend constexpr bool operator==(const Point&, const Point&) noexcept = default;

    constexpr Point& operator+=(Point offset) noexcept
    {
        x += offset.x;
        y += offset.y;
        return *this;
    }

    constexpr Point& operator-=(Point offset) noexcept
    {
        x -= offset.x;
        y -= offset.y;
        return *this;
    }
};

[[nodiscard]] constexpr Point operator+(Point point, Point offset) noexcept
{
    point += offset;
    return point;
}

[[nodiscard]] constexpr Point operator-(Point point, Point offset) noexcept
{
    point -= offset;
    return point;
}

struct Size final {
    Dip width = 0.0f;
    Dip height = 0.0f;

    constexpr Size() noexcept = default;
    constexpr Size(Dip widthValue, Dip heightValue) noexcept
        : width(widthValue), height(heightValue)
    {
    }

    friend constexpr bool operator==(const Size&, const Size&) noexcept = default;

    [[nodiscard]] constexpr bool IsEmpty() const noexcept
    {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] constexpr Size NonNegative() const noexcept
    {
        return { detail::NonNegative(width), detail::NonNegative(height) };
    }
};

struct Insets final {
    Dip left = 0.0f;
    Dip top = 0.0f;
    Dip right = 0.0f;
    Dip bottom = 0.0f;

    constexpr Insets() noexcept = default;

    constexpr explicit Insets(Dip all) noexcept
        : left(all), top(all), right(all), bottom(all)
    {
    }

    constexpr Insets(Dip horizontal, Dip vertical) noexcept
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical)
    {
    }

    constexpr Insets(Dip leftValue, Dip topValue, Dip rightValue, Dip bottomValue) noexcept
        : left(leftValue), top(topValue), right(rightValue), bottom(bottomValue)
    {
    }

    friend constexpr bool operator==(const Insets&, const Insets&) noexcept = default;

    [[nodiscard]] constexpr Dip Horizontal() const noexcept
    {
        return left + right;
    }

    [[nodiscard]] constexpr Dip Vertical() const noexcept
    {
        return top + bottom;
    }

    [[nodiscard]] constexpr bool IsZero() const noexcept
    {
        return left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f;
    }
};

struct Rect final {
    Dip x = 0.0f;
    Dip y = 0.0f;
    Dip width = 0.0f;
    Dip height = 0.0f;

    constexpr Rect() noexcept = default;

    constexpr Rect(Dip xValue, Dip yValue, Dip widthValue, Dip heightValue) noexcept
        : x(xValue), y(yValue), width(widthValue), height(heightValue)
    {
    }

    constexpr Rect(Point origin, Size size) noexcept
        : x(origin.x), y(origin.y), width(size.width), height(size.height)
    {
    }

    friend constexpr bool operator==(const Rect&, const Rect&) noexcept = default;

    [[nodiscard]] static constexpr Rect FromLTRB(
        Dip left, Dip top, Dip right, Dip bottom) noexcept
    {
        return { left, top, right - left, bottom - top };
    }

    [[nodiscard]] constexpr Dip Left() const noexcept { return x; }
    [[nodiscard]] constexpr Dip Top() const noexcept { return y; }
    [[nodiscard]] constexpr Dip Right() const noexcept { return x + width; }
    [[nodiscard]] constexpr Dip Bottom() const noexcept { return y + height; }
    [[nodiscard]] constexpr Point Origin() const noexcept { return { x, y }; }
    [[nodiscard]] constexpr Size Extent() const noexcept { return { width, height }; }

    [[nodiscard]] constexpr bool IsEmpty() const noexcept
    {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] constexpr Rect Normalized() const noexcept
    {
        const Dip left = width < 0.0f ? x + width : x;
        const Dip top = height < 0.0f ? y + height : y;
        return {
            left,
            top,
            detail::NonNegative(width < 0.0f ? -width : width),
            detail::NonNegative(height < 0.0f ? -height : height)
        };
    }

    // Hit testing uses half-open bounds: [left, right) x [top, bottom).
    [[nodiscard]] constexpr bool Contains(Point point) const noexcept
    {
        return !IsEmpty()
            && point.x >= Left() && point.x < Right()
            && point.y >= Top() && point.y < Bottom();
    }

    [[nodiscard]] constexpr bool Contains(Rect other) const noexcept
    {
        return !other.IsEmpty()
            && other.Left() >= Left() && other.Right() <= Right()
            && other.Top() >= Top() && other.Bottom() <= Bottom();
    }

    [[nodiscard]] constexpr bool Intersects(Rect other) const noexcept
    {
        return !IsEmpty() && !other.IsEmpty()
            && Left() < other.Right() && other.Left() < Right()
            && Top() < other.Bottom() && other.Top() < Bottom();
    }

    [[nodiscard]] constexpr Rect Intersection(Rect other) const noexcept
    {
        const Dip left = detail::Max(Left(), other.Left());
        const Dip top = detail::Max(Top(), other.Top());
        const Dip right = detail::Min(Right(), other.Right());
        const Dip bottom = detail::Min(Bottom(), other.Bottom());
        return right <= left || bottom <= top
            ? Rect { left, top, 0.0f, 0.0f }
            : FromLTRB(left, top, right, bottom);
    }

    [[nodiscard]] constexpr Rect Union(Rect other) const noexcept
    {
        if (IsEmpty()) {
            return other;
        }
        if (other.IsEmpty()) {
            return *this;
        }
        return FromLTRB(
            detail::Min(Left(), other.Left()),
            detail::Min(Top(), other.Top()),
            detail::Max(Right(), other.Right()),
            detail::Max(Bottom(), other.Bottom()));
    }

    [[nodiscard]] constexpr Rect Offset(Point offset) const noexcept
    {
        return { x + offset.x, y + offset.y, width, height };
    }

    [[nodiscard]] constexpr Rect Inset(Insets insets) const noexcept
    {
        return {
            x + insets.left,
            y + insets.top,
            detail::NonNegative(width - insets.Horizontal()),
            detail::NonNegative(height - insets.Vertical())
        };
    }

    [[nodiscard]] constexpr Rect Outset(Insets insets) const noexcept
    {
        return {
            x - insets.left,
            y - insets.top,
            detail::NonNegative(width + insets.Horizontal()),
            detail::NonNegative(height + insets.Vertical())
        };
    }
};

// A normalized constraint has 0 <= minimum <= maximum on each axis.
// Positive infinity is a valid maximum and represents an unbounded axis.
struct Constraints final {
    Size minimum { 0.0f, 0.0f };
    Size maximum { Infinity, Infinity };

    constexpr Constraints() noexcept = default;

    constexpr explicit Constraints(Size maximumSize) noexcept
        : maximum(maximumSize)
    {
    }

    constexpr Constraints(Size minimumSize, Size maximumSize) noexcept
        : minimum(minimumSize), maximum(maximumSize)
    {
    }

    friend constexpr bool operator==(const Constraints&, const Constraints&) noexcept = default;

    [[nodiscard]] static constexpr Constraints Unbounded() noexcept
    {
        return {};
    }

    [[nodiscard]] static constexpr Constraints Tight(Size size) noexcept
    {
        const Size normalized = size.NonNegative();
        return { normalized, normalized };
    }

    [[nodiscard]] constexpr bool IsNormalized() const noexcept
    {
        return !detail::IsNaN(minimum.width) && !detail::IsNaN(minimum.height)
            && !detail::IsNaN(maximum.width) && !detail::IsNaN(maximum.height)
            && minimum.width >= 0.0f && minimum.height >= 0.0f
            && maximum.width >= minimum.width
            && maximum.height >= minimum.height;
    }

    [[nodiscard]] constexpr Constraints Normalized() const noexcept
    {
        const Size normalizedMinimum = minimum.NonNegative();
        const Dip maximumWidth = detail::IsNaN(maximum.width)
            ? normalizedMinimum.width
            : detail::Max(normalizedMinimum.width, maximum.width);
        const Dip maximumHeight = detail::IsNaN(maximum.height)
            ? normalizedMinimum.height
            : detail::Max(normalizedMinimum.height, maximum.height);
        return {
            normalizedMinimum,
            { maximumWidth, maximumHeight }
        };
    }

    [[nodiscard]] constexpr bool IsTight() const noexcept
    {
        const Constraints normalized = Normalized();
        return normalized.minimum == normalized.maximum;
    }

    [[nodiscard]] constexpr bool IsWidthBounded() const noexcept
    {
        return Normalized().maximum.width < Infinity;
    }

    [[nodiscard]] constexpr bool IsHeightBounded() const noexcept
    {
        return Normalized().maximum.height < Infinity;
    }

    [[nodiscard]] constexpr Size Constrain(Size requested) const noexcept
    {
        const Constraints normalized = Normalized();
        return {
            detail::Clamp(
                requested.width,
                normalized.minimum.width,
                normalized.maximum.width),
            detail::Clamp(
                requested.height,
                normalized.minimum.height,
                normalized.maximum.height)
        };
    }

    [[nodiscard]] constexpr bool Contains(Size size) const noexcept
    {
        const Constraints normalized = Normalized();
        return !detail::IsNaN(size.width) && !detail::IsNaN(size.height)
            && size.width >= normalized.minimum.width
            && size.width <= normalized.maximum.width
            && size.height >= normalized.minimum.height
            && size.height <= normalized.maximum.height;
    }

    [[nodiscard]] constexpr Constraints Deflate(Insets insets) const noexcept
    {
        const Constraints normalized = Normalized();
        const Dip horizontal = insets.Horizontal();
        const Dip vertical = insets.Vertical();

        const auto deflate = [](Dip value, Dip amount) constexpr noexcept {
            return value == Infinity
                ? Infinity
                : detail::NonNegative(value - amount);
        };

        return Constraints {
            {
                deflate(normalized.minimum.width, horizontal),
                deflate(normalized.minimum.height, vertical)
            },
            {
                deflate(normalized.maximum.width, horizontal),
                deflate(normalized.maximum.height, vertical)
            }
        }.Normalized();
    }
};

} // namespace cui::core
