#pragma once

#include "LegacyCanvasLayout.h"
#include "../Control.h"
#include <algorithm>

namespace cui::layout::compat {

[[nodiscard]] inline Alignment ToLayoutAlignment(::HorizontalAlignment value) noexcept
{
    switch (value) {
    case ::HorizontalAlignment::Center: return Alignment::Center;
    case ::HorizontalAlignment::Right: return Alignment::End;
    case ::HorizontalAlignment::Stretch: return Alignment::Stretch;
    case ::HorizontalAlignment::Left:
    default: return Alignment::Start;
    }
}

[[nodiscard]] inline Alignment ToLayoutAlignment(::VerticalAlignment value) noexcept
{
    switch (value) {
    case ::VerticalAlignment::Center: return Alignment::Center;
    case ::VerticalAlignment::Bottom: return Alignment::End;
    case ::VerticalAlignment::Stretch: return Alignment::Stretch;
    case ::VerticalAlignment::Top:
    default: return Alignment::Start;
    }
}

[[nodiscard]] inline LegacyCanvasSlot GetLegacyCanvasSlot(Control& child)
{
    const POINT location = child.Location;
    const Thickness margin = child.Margin;
    const std::uint8_t anchors = child.AnchorStyles;

    LegacyCanvasSlot slot;
    slot.location = { (core::Dip)location.x, (core::Dip)location.y };
    slot.margin = { margin.Left, margin.Top, margin.Right, margin.Bottom };
    slot.horizontalAlignment = ToLayoutAlignment(child.HAlign);
    slot.verticalAlignment = ToLayoutAlignment(child.VAlign);
    slot.useAnchorMode = anchors != ::AnchorStyles::None;
    slot.anchorLeft = (anchors & ::AnchorStyles::Left) != 0;
    slot.anchorTop = (anchors & ::AnchorStyles::Top) != 0;
    slot.anchorRight = (anchors & ::AnchorStyles::Right) != 0;
    slot.anchorBottom = (anchors & ::AnchorStyles::Bottom) != 0;
    return slot;
}

inline void ArrangeLegacyCanvasChild(
    Control& child,
    core::Rect alignmentRect,
    core::Rect anchoredStretchRect,
    const core::Constraints& measureAvailable)
{
    const core::Size measured = child.Measure(measureAvailable);
    const core::Rect arranged = ArrangeLegacyCanvasItem(
        alignmentRect,
        anchoredStretchRect,
        measured,
        GetLegacyCanvasSlot(child));

    child.ApplyLayout(arranged);
}

inline void ArrangeLegacyCanvasChild(
    Control& child,
    core::Rect contentRect,
    const core::Constraints& measureAvailable)
{
    ArrangeLegacyCanvasChild(
        child, contentRect, contentRect, measureAvailable);
}

inline void ArrangeLegacyCanvasChild(
    Control& child,
    core::Rect alignmentRect,
    core::Rect anchoredStretchRect,
    SIZE measureAvailable)
{
    ArrangeLegacyCanvasChild(
        child,
        alignmentRect,
        anchoredStretchRect,
        core::Constraints{ core::Size{
            static_cast<core::Dip>((std::max)(0L, measureAvailable.cx)),
            static_cast<core::Dip>((std::max)(0L, measureAvailable.cy)) } });
}

inline void ArrangeLegacyCanvasChild(
    Control& child,
    core::Rect contentRect,
    SIZE measureAvailable)
{
    ArrangeLegacyCanvasChild(child, contentRect, contentRect, measureAvailable);
}

} // namespace cui::layout::compat
