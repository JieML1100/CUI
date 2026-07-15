#pragma once

#include "LayoutState.h"

namespace cui::layout {

/**
 * Placement data used by the compatibility Canvas/Anchor layout.
 *
 * This is intentionally a parent-child slot instead of element state. New
 * layout containers can therefore keep their own placement metadata while
 * the public WinForms-like Location/Anchor API remains available as an
 * adapter during migration.
 */
struct LegacyCanvasSlot final {
    core::Point location {};
    core::Insets margin {};
    Alignment horizontalAlignment = Alignment::Start;
    Alignment verticalAlignment = Alignment::Start;
    // Preserves the legacy rule that any non-zero AnchorStyles value, even an
    // unknown bit, disables alignment on both axes.
    bool useAnchorMode = false;
    bool anchorLeft = false;
    bool anchorTop = false;
    bool anchorRight = false;
    bool anchorBottom = false;
};

/**
 * Computes a child's arranged rectangle in parent-local DIPs.
 *
 * The rules deliberately match the existing retained-mode containers:
 * anchors take precedence over alignment; Location supplies the Start axis;
 * right/bottom margins supply the anchored edge distance. A distinct stretch
 * rectangle preserves Form's historic status-bar rule without duplicating the
 * placement algorithm.
 */
[[nodiscard]] constexpr core::Rect ArrangeLegacyCanvasItem(
    core::Rect alignmentRect,
    core::Rect anchoredStretchRect,
    core::Size desiredSize,
    const LegacyCanvasSlot& slot) noexcept
{
    alignmentRect = alignmentRect.Normalized();
    anchoredStretchRect = anchoredStretchRect.Normalized();
    desiredSize = desiredSize.NonNegative();
    const auto nonNegative = [](core::Dip value) constexpr noexcept {
        return value > 0.0f ? value : 0.0f;
    };

    core::Dip x = alignmentRect.x + slot.margin.left;
    core::Dip y = alignmentRect.y + slot.margin.top;
    core::Dip width = desiredSize.width;
    core::Dip height = desiredSize.height;

    const bool hasAnyAnchor = slot.useAnchorMode
        || slot.anchorLeft || slot.anchorTop
        || slot.anchorRight || slot.anchorBottom;
    if (hasAnyAnchor) {
        if (slot.anchorLeft && slot.anchorRight) {
            x = anchoredStretchRect.x + slot.location.x;
            width = nonNegative(
                anchoredStretchRect.width - slot.location.x - slot.margin.right);
        } else if (slot.anchorRight) {
            x = alignmentRect.Right() - slot.margin.right - width;
        } else {
            x = alignmentRect.x + slot.location.x;
        }

        if (slot.anchorTop && slot.anchorBottom) {
            y = anchoredStretchRect.y + slot.location.y;
            height = nonNegative(
                anchoredStretchRect.height - slot.location.y - slot.margin.bottom);
        } else if (slot.anchorBottom) {
            y = alignmentRect.Bottom() - slot.margin.bottom - height;
        } else {
            y = alignmentRect.y + slot.location.y;
        }
    } else {
        const core::Dip availableWidth = nonNegative(
            alignmentRect.width - slot.margin.Horizontal());
        switch (slot.horizontalAlignment) {
        case Alignment::Stretch:
            x = alignmentRect.x + slot.margin.left;
            width = availableWidth;
            break;
        case Alignment::Center:
            x = alignmentRect.x + slot.margin.left + (availableWidth - width) * 0.5f;
            break;
        case Alignment::End:
            x = alignmentRect.Right() - slot.margin.right - width;
            break;
        case Alignment::Start:
        default:
            x = alignmentRect.x + slot.location.x;
            break;
        }

        const core::Dip availableHeight = nonNegative(
            alignmentRect.height - slot.margin.Vertical());
        switch (slot.verticalAlignment) {
        case Alignment::Stretch:
            y = alignmentRect.y + slot.margin.top;
            height = availableHeight;
            break;
        case Alignment::Center:
            y = alignmentRect.y + slot.margin.top + (availableHeight - height) * 0.5f;
            break;
        case Alignment::End:
            y = alignmentRect.Bottom() - slot.margin.bottom - height;
            break;
        case Alignment::Start:
        default:
            y = alignmentRect.y + slot.location.y;
            break;
        }
    }

    return { x, y, nonNegative(width), nonNegative(height) };
}

[[nodiscard]] constexpr core::Rect ArrangeLegacyCanvasItem(
    core::Rect contentRect,
    core::Size desiredSize,
    const LegacyCanvasSlot& slot) noexcept
{
    return ArrangeLegacyCanvasItem(
        contentRect, contentRect, desiredSize, slot);
}

} // namespace cui::layout
