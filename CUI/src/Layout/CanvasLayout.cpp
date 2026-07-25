#include "Layout/CanvasLayout.h"

#include "Canvas.h"
#include "Control.h"

namespace cui::layout {

CanvasSlot GetCanvasSlot(Control& child) noexcept
{
    const Thickness margin = child.Margin;
    return CanvasSlot{
        Canvas::GetLeft(child),
        Canvas::GetTop(child),
        Canvas::GetRight(child),
        Canvas::GetBottom(child),
        core::Insets{
            margin.Left, margin.Top, margin.Right, margin.Bottom } };
}

core::Size MeasureCanvasChild(Control& child)
{
    return child.Measure(core::Constraints::Unbounded());
}

core::Rect ArrangeCanvasChild(Control& child, core::Rect contentRect)
{
    const auto arranged = ArrangeCanvasItem(
        contentRect,
        MeasureCanvasChild(child),
        GetCanvasSlot(child));
    child.Arrange(arranged);
    return arranged;
}

} // namespace cui::layout
