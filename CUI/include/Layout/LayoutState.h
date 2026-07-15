#pragma once

#include "../Core/Geometry.h"

#include <cstdint>

namespace cui::layout {

using core::Constraints;
using core::Dip;
using core::Infinity;
using core::Insets;
using core::Rect;
using core::Size;

enum class LengthUnit : std::uint8_t {
    Auto,
    Dip
};

// Length is intentionally limited to intrinsic (Auto) and fixed DIP sizing.
// Parent-specific concepts such as Grid Star and Canvas slots do not belong in
// the base element's layout properties.
struct Length final {
    LengthUnit unit = LengthUnit::Auto;
    Dip value = 0.0f;

    constexpr Length() noexcept = default;
    constexpr Length(LengthUnit unitValue, Dip valueValue) noexcept
        : unit(unitValue), value(valueValue)
    {
    }

    friend constexpr bool operator==(const Length&, const Length&) noexcept = default;

    [[nodiscard]] static constexpr Length Auto() noexcept
    {
        return {};
    }

    [[nodiscard]] static constexpr Length Fixed(Dip value) noexcept
    {
        return { LengthUnit::Dip, value < 0.0f || value != value ? 0.0f : value };
    }

    [[nodiscard]] constexpr bool IsAuto() const noexcept
    {
        return unit == LengthUnit::Auto;
    }

    [[nodiscard]] constexpr bool IsFixed() const noexcept
    {
        return unit == LengthUnit::Dip;
    }
};

enum class Alignment : std::uint8_t {
    Start,
    Center,
    End,
    Stretch
};

// User-specified layout properties. Arrange never writes into this object.
// Container-owned placement data (Grid cell, Canvas coordinates, Dock slot,
// etc.) is deliberately kept out of the base style.
struct LayoutStyle final {
    Length width = Length::Auto();
    Length height = Length::Auto();
    Size minimumSize { 0.0f, 0.0f };
    Size maximumSize { Infinity, Infinity };
    Insets margin {};
    Insets padding {};
    Alignment horizontalAlignment = Alignment::Stretch;
    Alignment verticalAlignment = Alignment::Stretch;

    friend constexpr bool operator==(const LayoutStyle&, const LayoutStyle&) noexcept = default;

    [[nodiscard]] constexpr Constraints SizeConstraints() const noexcept
    {
        return Constraints { minimumSize, maximumSize }.Normalized();
    }
};

// Alias that makes the specified/computed distinction explicit at call sites
// where LayoutStyle would otherwise be ambiguous.
using LayoutSpecifiedState = LayoutStyle;

enum class DirtyFlags : std::uint8_t {
    None = 0,
    Measure = 1u << 0,
    Arrange = 1u << 1,
    Paint = 1u << 2,
    All = 0x07
};

[[nodiscard]] constexpr DirtyFlags operator|(DirtyFlags left, DirtyFlags right) noexcept
{
    return static_cast<DirtyFlags>(
        static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr DirtyFlags operator&(DirtyFlags left, DirtyFlags right) noexcept
{
    return static_cast<DirtyFlags>(
        static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr DirtyFlags operator~(DirtyFlags value) noexcept
{
    return static_cast<DirtyFlags>(~static_cast<std::uint8_t>(value));
}

constexpr DirtyFlags& operator|=(DirtyFlags& left, DirtyFlags right) noexcept
{
    left = left | right;
    return left;
}

constexpr DirtyFlags& operator&=(DirtyFlags& left, DirtyFlags right) noexcept
{
    left = left & right;
    return left;
}

[[nodiscard]] constexpr bool HasAny(DirtyFlags value, DirtyFlags flags) noexcept
{
    return (value & flags) != DirtyFlags::None;
}

// Invalidating measure necessarily invalidates arrange; invalidating either
// geometry phase also requires repainting the affected visual.
[[nodiscard]] constexpr DirtyFlags ExpandDirtyDependencies(DirtyFlags flags) noexcept
{
    if (HasAny(flags, DirtyFlags::Measure)) {
        flags |= DirtyFlags::Arrange | DirtyFlags::Paint;
    } else if (HasAny(flags, DirtyFlags::Arrange)) {
        flags |= DirtyFlags::Paint;
    }
    return flags;
}

// Runtime-computed layout data. It never overwrites LayoutStyle, so public
// Width/Height declarations can remain stable while layout results change.
struct LayoutState final {
    Size desiredSize {};
    Rect arrangedRect {};
    Constraints lastMeasureConstraints {};
    DirtyFlags dirty = DirtyFlags::All;
    bool hasMeasured = false;
    bool hasArranged = false;

    [[nodiscard]] constexpr bool NeedsMeasure() const noexcept
    {
        return !hasMeasured || HasAny(dirty, DirtyFlags::Measure);
    }

    [[nodiscard]] constexpr bool NeedsArrange() const noexcept
    {
        return NeedsMeasure()
            || !hasArranged
            || HasAny(dirty, DirtyFlags::Arrange);
    }

    [[nodiscard]] constexpr bool NeedsPaint() const noexcept
    {
        return NeedsArrange() || HasAny(dirty, DirtyFlags::Paint);
    }

    constexpr void Invalidate(DirtyFlags flags) noexcept
    {
        dirty |= ExpandDirtyDependencies(flags);
    }

    constexpr void InvalidateMeasure() noexcept
    {
        Invalidate(DirtyFlags::Measure);
    }

    constexpr void InvalidateArrange() noexcept
    {
        Invalidate(DirtyFlags::Arrange);
    }

    constexpr void InvalidatePaint() noexcept
    {
        Invalidate(DirtyFlags::Paint);
    }

    constexpr void CommitMeasure(Size desired, Constraints constraints) noexcept
    {
        const Size normalizedDesired = desired.NonNegative();
        const Constraints normalizedConstraints = constraints.Normalized();
        if (!hasMeasured || desiredSize != normalizedDesired
            || lastMeasureConstraints != normalizedConstraints) {
            dirty |= DirtyFlags::Arrange | DirtyFlags::Paint;
        }

        desiredSize = normalizedDesired;
        lastMeasureConstraints = normalizedConstraints;
        hasMeasured = true;
        dirty &= ~DirtyFlags::Measure;
    }

    constexpr void CommitArrange(Rect finalRect) noexcept
    {
        const Rect normalizedRect = finalRect.Normalized();
        if (!hasArranged || arrangedRect != normalizedRect) {
            dirty |= DirtyFlags::Paint;
        }

        arrangedRect = normalizedRect;
        hasArranged = true;
        dirty &= ~DirtyFlags::Arrange;
    }

    constexpr void CommitPaint() noexcept
    {
        dirty &= ~DirtyFlags::Paint;
    }

    constexpr void Reset() noexcept
    {
        *this = {};
    }
};

} // namespace cui::layout
