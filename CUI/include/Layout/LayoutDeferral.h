#pragma once

#include "Core/Geometry.h"

namespace cui::layout {

struct DeferredLayoutWork final {
    bool ready = false;
    bool layoutRequested = false;
    bool visualRequested = false;
    bool fullVisual = false;
    bool immediate = false;
    cui::core::Rect visualBounds{};
};

/**
 * Collects layout and paint work while a Control or Window is suspended.
 *
 * The state is deliberately platform independent: visualBounds uses the
 * caller's coordinate space (Control/Window currently store client pixels),
 * while nesting and coalescing semantics are shared and unit-testable.
 */
class LayoutDeferral final {
public:
    void Suspend() noexcept
    {
        ++_depth;
    }

    [[nodiscard]] bool IsSuspended() const noexcept
    {
        return _depth > 0;
    }

    [[nodiscard]] int Depth() const noexcept
    {
        return _depth;
    }

    void QueueLayout() noexcept
    {
        _layoutRequested = true;
    }

    void QueueVisual(cui::core::Rect bounds, bool immediate = false) noexcept
    {
        _immediate = _immediate || immediate;
        if (bounds.IsEmpty()) {
            return;
        }
        if (!_fullVisual) {
            _visualBounds = _visualRequested
                ? _visualBounds.Union(bounds)
                : bounds;
        }
        _visualRequested = true;
    }

    void QueueFullVisual(bool immediate = false) noexcept
    {
        _visualRequested = true;
        _fullVisual = true;
        _immediate = _immediate || immediate;
        _visualBounds = {};
    }

    [[nodiscard]] DeferredLayoutWork Resume() noexcept
    {
        if (_depth <= 0) {
            return {};
        }

        --_depth;
        if (_depth > 0) {
            return {};
        }

        DeferredLayoutWork work{
            .ready = true,
            .layoutRequested = _layoutRequested,
            .visualRequested = _visualRequested,
            .fullVisual = _fullVisual,
            .immediate = _immediate,
            .visualBounds = _visualBounds
        };
        _layoutRequested = false;
        _visualRequested = false;
        _fullVisual = false;
        _immediate = false;
        _visualBounds = {};
        return work;
    }

private:
    int _depth = 0;
    bool _layoutRequested = false;
    bool _visualRequested = false;
    bool _fullVisual = false;
    bool _immediate = false;
    cui::core::Rect _visualBounds{};
};

} // namespace cui::layout
