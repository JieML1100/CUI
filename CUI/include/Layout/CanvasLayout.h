#pragma once

#include "../Core/Geometry.h"

#include <limits>

class Control;

namespace cui::layout {

inline constexpr core::Dip UnsetCanvasOffset =
    (std::numeric_limits<core::Dip>::quiet_NaN)();

/** Values of Canvas.Left/Top/Right/Bottom stored on a child element. */
struct CanvasSlot final {
    core::Dip left = UnsetCanvasOffset;
    core::Dip top = UnsetCanvasOffset;
    core::Dip right = UnsetCanvasOffset;
    core::Dip bottom = UnsetCanvasOffset;
    core::Insets margin {};
};

[[nodiscard]] constexpr bool IsCanvasOffsetSet(core::Dip value) noexcept
{
    return value == value
        && value != core::Infinity
        && value != -core::Infinity;
}

/**
 * Applies WPF Canvas precedence: Left wins over Right and Top wins over
 * Bottom. With neither edge specified, the child starts at the Canvas origin.
 * Canvas never stretches a child and alignment does not participate.
 */
[[nodiscard]] constexpr core::Rect ArrangeCanvasItem(
    core::Rect contentRect,
    core::Size desiredSize,
    const CanvasSlot& slot) noexcept
{
    contentRect = contentRect.Normalized();
    desiredSize = desiredSize.NonNegative();
    const core::Dip outerWidth = desiredSize.width + slot.margin.Horizontal();
    const core::Dip outerHeight = desiredSize.height + slot.margin.Vertical();

    core::Dip x = contentRect.x;
    if (IsCanvasOffsetSet(slot.left)) {
        x += slot.left;
    } else if (IsCanvasOffsetSet(slot.right)) {
        x = contentRect.Right() - slot.right - outerWidth;
    }

    core::Dip y = contentRect.y;
    if (IsCanvasOffsetSet(slot.top)) {
        y += slot.top;
    } else if (IsCanvasOffsetSet(slot.bottom)) {
        y = contentRect.Bottom() - slot.bottom - outerHeight;
    }

    return {
        x + slot.margin.left,
        y + slot.margin.top,
        desiredSize.width,
        desiredSize.height };
}

[[nodiscard]] CanvasSlot GetCanvasSlot(Control& child) noexcept;

/** Measures a Canvas child with unbounded space, as WPF Canvas does. */
[[nodiscard]] core::Size MeasureCanvasChild(Control& child);

/** Measures and commits one child rectangle using its Canvas attached values. */
[[nodiscard]] core::Rect ArrangeCanvasChild(
    Control& child,
    core::Rect contentRect);

} // namespace cui::layout
