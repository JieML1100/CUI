#pragma once

#include "ControlWeakReference.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Control;
class Window;

/** WPF-compatible traversal policy applied to a navigation container. */
enum class KeyboardNavigationMode : unsigned char
{
	Continue,
	Once,
	Cycle,
	None,
	Contained,
	Local,
};

/** Logical directions understood by the focus-navigation service. */
enum class FocusNavigationDirection : unsigned char
{
	Next,
	Previous,
	First,
	Last,
	Left,
	Right,
	Up,
	Down,
};

enum class FocusChangeReason : unsigned char
{
	Programmatic,
	KeyboardNavigation,
	Pointer,
	WindowActivation,
	PopupRestore,
	TreeDetach,
	EligibilityChanged,
};

struct FocusManagerStatistics final
{
	std::uint64_t KeyboardTransitions = 0;
	std::uint64_t KeyboardTransitionsCanceled = 0;
	std::uint64_t LogicalFocusUpdates = 0;
	std::uint64_t NavigationRequests = 0;
	std::uint64_t NavigationSucceeded = 0;
	std::uint64_t ActivationRestores = 0;
	std::uint64_t PopupRestores = 0;
};

/**
 * Owns keyboard focus, logical focus scopes and geometry-based navigation for
 * one Window. Platform focus is projected by Window; controls never interpret
 * WM_SETFOCUS/WM_KILLFOCUS individually.
 */
class FocusManager final
{
public:
	explicit FocusManager(Window& owner) noexcept;

	Control* KeyboardFocusedElement() const noexcept
	{
		return _keyboardFocus.Get();
	}
	Control* GetFocusScope(Control* element) const noexcept;
	Control* GetLogicalFocusedElement(Control* scope = nullptr) const noexcept;
	bool SetLogicalFocus(
		Control* scope,
		Control* element,
		bool moveKeyboardFocus = false,
		bool invalidateVisual = true);
	bool SetKeyboardFocus(
		Control* element,
		bool invalidateVisual = true,
		FocusChangeReason reason = FocusChangeReason::Programmatic);

	std::vector<Control*> GetTabOrder(Control* scope = nullptr) const;
	/** Focusable order without applying IsTabStop; used by access-key search. */
	std::vector<Control*> GetFocusableOrder(Control* scope = nullptr) const;
	static std::vector<Control*> BuildTabOrder(
		std::span<Control* const> roots);
	bool MoveFocus(FocusNavigationDirection direction);

	void DeactivateWindow();
	bool ActivateWindow();
	void OpenTransientScope(Control* scope);
	void CloseTransientScope(Control* scope);
	void DetachVisualChild(Control* root);
	void Reset() noexcept;

	const FocusManagerStatistics& Statistics() const noexcept
	{
		return _statistics;
	}

private:
	struct TabEntry final
	{
		Control* Value = nullptr;
		int TabIndex = 0;
		std::size_t TreeOrder = 0;
	};

	Window* _window = nullptr;
	ControlWeakReference _keyboardFocus;
	ControlWeakReference _keyboardFocusStateOwner;
	std::uint64_t _keyboardFocusVersion = 0;
	ControlWeakReference _suspendedKeyboardFocus;
	bool _windowActive = true;
	std::unordered_map<Control*, Control*> _logicalFocus;
	std::unordered_map<Control*, Control*> _transientRestoreTargets;
	std::unordered_set<Control*> _logicalFocusedElements;
	FocusManagerStatistics _statistics;

	bool SetKeyboardFocusCore(
		Control* element,
		bool invalidateVisual,
		FocusChangeReason reason,
		bool updateLogicalFocus);
	void UpdateLogicalFocusChain(Control* element);
	void RefreshLogicalFocusProjection();
	void RefreshKeyboardFocusProjection(
		Control* previous, Control* current);
	void ReconcileKeyboardFocusedState();
	Control* ResolveLogicalFocus(Control* scope) const noexcept;
	Control* FindNavigationBoundary(
		Control* origin,
		bool directional,
		KeyboardNavigationMode& mode) const noexcept;
	std::vector<Control*> BuildTabOrderFor(
		Control* scope,
		bool honorOnce,
		bool tabOnly) const;
	void CollectTabEntries(
		Control* root,
		bool honorOnce,
		bool tabOnly,
		std::size_t& treeOrder,
		std::vector<TabEntry>& entries) const;
	Control* PreferredOnceTarget(Control* group, bool tabOnly) const;
	Control* FindTabTarget(FocusNavigationDirection direction) const;
	Control* FindDirectionalTarget(FocusNavigationDirection direction) const;
	bool IsOwned(Control* element) const noexcept;
	static bool IsDescendantOrSelf(
		Control* element,
		Control* ancestor) noexcept;
};
