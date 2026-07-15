#pragma once

#include "Core/Geometry.h"

#include <exception>
#include <utility>

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
 * Collects layout and paint work while a Control or Form is suspended.
 *
 * The state is deliberately platform independent: visualBounds uses the
 * caller's coordinate space (Control/Form currently store client pixels),
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

/**
 * Exception-safe owner for one SuspendLayout()/ResumeLayout() pair.
 * During stack unwinding it resumes without forcing synchronous layout; the
 * queued work is still propagated and will be committed by the next frame.
 */
template<typename Host>
class LayoutScope final {
public:
    explicit LayoutScope(Host& host, bool performLayout = true)
        : _host(&host),
          _performLayout(performLayout),
          _uncaughtOnEntry(std::uncaught_exceptions())
    {
        _host->SuspendLayout();
    }

    LayoutScope(const LayoutScope&) = delete;
    LayoutScope& operator=(const LayoutScope&) = delete;

    LayoutScope(LayoutScope&& other) noexcept
        : _host(std::exchange(other._host, nullptr)),
          _performLayout(other._performLayout),
          _uncaughtOnEntry(other._uncaughtOnEntry)
    {
    }

    LayoutScope& operator=(LayoutScope&&) = delete;

    ~LayoutScope() noexcept
    {
        if (!_host) {
            return;
        }
        Host* host = std::exchange(_host, nullptr);
        const bool unwinding = std::uncaught_exceptions() > _uncaughtOnEntry;
        try {
            host->ResumeLayout(unwinding ? false : _performLayout);
        }
        catch (...) {
            // Destructors must not mask an active exception or terminate UI
            // teardown. Explicit Commit() remains available when propagation
            // of a resume-time failure is desired.
        }
    }

    void Commit()
    {
        if (!_host) {
            return;
        }
        Host* host = std::exchange(_host, nullptr);
        host->ResumeLayout(_performLayout);
    }

    [[nodiscard]] bool IsActive() const noexcept
    {
        return _host != nullptr;
    }

private:
    Host* _host = nullptr;
    bool _performLayout = true;
    int _uncaughtOnEntry = 0;
};

template<typename Host>
[[nodiscard]] LayoutScope<Host> DeferLayout(
    Host& host, bool performLayout = true)
{
    return LayoutScope<Host>{ host, performLayout };
}

} // namespace cui::layout
