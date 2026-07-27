#define NOMINMAX
#include "FocusManager.h"

#include "Control.h"
#include "InputInfrastructure.h"
#include "TextCompositionManager.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace
{
	bool IsDirectional(FocusNavigationDirection direction) noexcept
	{
		switch (direction)
		{
		case FocusNavigationDirection::Left:
		case FocusNavigationDirection::Right:
		case FocusNavigationDirection::Up:
		case FocusNavigationDirection::Down:
			return true;
		default:
			return false;
		}
	}

	bool IsBoundaryMode(KeyboardNavigationMode mode) noexcept
	{
		return mode == KeyboardNavigationMode::Cycle
			|| mode == KeyboardNavigationMode::Contained;
	}

	float CenterX(const cui::core::Rect& value) noexcept
	{
		return value.x + value.width * 0.5f;
	}

	float CenterY(const cui::core::Rect& value) noexcept
	{
		return value.y + value.height * 0.5f;
	}
}

FocusManager::FocusManager(Window& owner) noexcept
	: _window(&owner)
{
}

bool FocusManager::IsDescendantOrSelf(
	Control* element,
	Control* ancestor) noexcept
{
	if (!element || !ancestor) return false;
	for (auto* current = element; current; current = current->GetRoutedParent())
	{
		if (current == ancestor) return true;
	}
	return false;
}

bool FocusManager::IsOwned(Control* element) const noexcept
{
	return element && _window
		&& (element == _window || element->GetPresentationWindow() == _window);
}

Control* FocusManager::GetFocusScope(Control* element) const noexcept
{
	if (!_window) return nullptr;
	for (auto* current = element; current && current != _window;
		current = current->GetRoutedParent())
	{
		if (current->IsFocusScope
			|| _transientRestoreTargets.contains(current)) return current;
	}
	return _window;
}

Control* FocusManager::ResolveLogicalFocus(Control* scope) const noexcept
{
	if (!_window) return nullptr;
	if (!scope) scope = _window;
	std::unordered_set<Control*> visited;
	auto* currentScope = scope;
	Control* result = nullptr;
	while (currentScope && visited.insert(currentScope).second)
	{
		const auto found = _logicalFocus.find(currentScope);
		if (found == _logicalFocus.end()) break;
		result = found->second;
		if (!result || result == currentScope
			|| (!result->IsFocusScope
				&& !_transientRestoreTargets.contains(result))) break;
		currentScope = result;
	}
	return IsOwned(result) ? result : nullptr;
}

Control* FocusManager::GetLogicalFocusedElement(Control* scope) const noexcept
{
	if (!_window) return nullptr;
	if (!scope) scope = _window;
	if (scope != _window && (!IsOwned(scope) || !scope->IsFocusScope))
		return nullptr;
	const auto found = _logicalFocus.find(scope);
	return found != _logicalFocus.end() && IsOwned(found->second)
		? found->second : nullptr;
}

void FocusManager::UpdateLogicalFocusChain(Control* element)
{
	if (!element || !_window) return;
	std::vector<std::pair<Control*, Control*>> changes;
	auto* focusedValue = element;
	auto* scope = GetFocusScope(element);
	std::unordered_set<Control*> visited;
	while (scope && visited.insert(scope).second)
	{
		const auto found = _logicalFocus.find(scope);
		auto* previous = found != _logicalFocus.end()
			? found->second : nullptr;
		if (previous != focusedValue)
		{
			_logicalFocus[scope] = focusedValue;
			changes.emplace_back(previous, focusedValue);
			++_statistics.LogicalFocusUpdates;
		}
		if (scope == _window) break;
		focusedValue = scope;
		scope = GetFocusScope(scope->GetRoutedParent());
	}
	RefreshLogicalFocusProjection();
	for (const auto& [previous, current] : changes)
		_window->PublishLogicalFocusTransition(previous, current);
}

void FocusManager::RefreshLogicalFocusProjection()
{
	std::unordered_set<Control*> next;
	for (const auto& [scope, focused] : _logicalFocus)
	{
		(void)scope;
		if (focused && IsOwned(focused)) next.insert(focused);
	}
	for (auto* previous : _logicalFocusedElements)
		if (previous && !next.contains(previous))
			cui::framework::InputAccess::PublishLogicalFocusState(
				*previous, false);
	for (auto* current : next)
		if (current && !_logicalFocusedElements.contains(current))
			cui::framework::InputAccess::PublishLogicalFocusState(
				*current, true);
	_logicalFocusedElements = std::move(next);
}

void FocusManager::RefreshKeyboardFocusProjection(
	Control* previous,
	Control* current)
{
	if (previous && previous != current)
		cui::framework::InputAccess::PublishKeyboardFocusState(
			*previous, false);
	if (current && previous != current)
		cui::framework::InputAccess::PublishKeyboardFocusState(
			*current, true);

	std::unordered_set<Control*> next;
	for (auto* value = current; value; value = value->GetRoutedParent())
	{
		if (!IsOwned(value)) break;
		next.insert(value);
		if (value == _window) break;
	}
	for (auto* value : _keyboardFocusWithinElements)
		if (value && !next.contains(value))
			cui::framework::InputAccess::PublishKeyboardFocusWithinState(
				*value, false);
	for (auto* value : next)
		if (value && !_keyboardFocusWithinElements.contains(value))
			cui::framework::InputAccess::PublishKeyboardFocusWithinState(
				*value, true);
	_keyboardFocusWithinElements = std::move(next);
}

bool FocusManager::SetLogicalFocus(
	Control* scope,
	Control* element,
	bool moveKeyboardFocus,
	bool invalidateVisual)
{
	if (!_window) return false;
	if (!scope) scope = _window;
	if ((scope != _window && (!IsOwned(scope) || !scope->IsFocusScope))
		|| (element && (!IsOwned(element)
			|| !IsDescendantOrSelf(element, scope))))
		return false;
	const auto found = _logicalFocus.find(scope);
	auto* previous = found != _logicalFocus.end()
		? found->second : nullptr;
	if (previous != element)
	{
		_logicalFocus[scope] = element;
		++_statistics.LogicalFocusUpdates;
		RefreshLogicalFocusProjection();
		_window->PublishLogicalFocusTransition(previous, element);
	}
	if (!moveKeyboardFocus) return true;
	return element && SetKeyboardFocus(
		element, invalidateVisual, FocusChangeReason::Programmatic);
}

bool FocusManager::SetKeyboardFocusCore(
	Control* element,
	bool invalidateVisual,
	FocusChangeReason reason,
	bool updateLogicalFocus)
{
	if (!_window || (element && !IsOwned(element))) return false;
	if (!_windowActive && element
		&& reason != FocusChangeReason::WindowActivation)
	{
		if (updateLogicalFocus) UpdateLogicalFocusChain(element);
		_suspendedKeyboardFocus = element;
		return false;
	}
	if (_keyboardFocus == element) return true;
	auto* previous = _keyboardFocus;
	auto commit = [&]
	{
		if (_window->_textCompositionManager)
			_window->_textCompositionManager->BeforeKeyboardFocusCommit(
				previous, element, reason);
		// Native composition completion can synchronously publish TextInput, and
		// an application handler may move focus again. Never overwrite that newer
		// accepted transition with this now-stale request.
		if (_keyboardFocus != previous) return false;
		_keyboardFocus = element;
		RefreshKeyboardFocusProjection(previous, element);
		if (updateLogicalFocus && element) UpdateLogicalFocusChain(element);
		++_statistics.KeyboardTransitions;
		_window->PublishKeyboardFocusTransition(
			previous, element, invalidateVisual);
		return true;
	};

	if (_window->_inputManager)
	{
		const bool cancelable = element != nullptr
			|| (reason != FocusChangeReason::WindowActivation
				&& reason != FocusChangeReason::TreeDetach
				&& reason != FocusChangeReason::EligibilityChanged);
		auto transition =
			_window->_inputManager->BeginKeyboardFocusTransition(
				previous, element, cancelable);
		if (!transition.Accepted)
		{
			++_statistics.KeyboardTransitionsCanceled;
			return false;
		}
		if (_keyboardFocus != previous)
		{
			_window->_inputManager->CancelKeyboardFocusTransition(
				transition);
			++_statistics.KeyboardTransitionsCanceled;
			return false;
		}
		if (!commit())
		{
			_window->_inputManager->CancelKeyboardFocusTransition(
				transition);
			++_statistics.KeyboardTransitionsCanceled;
			return false;
		}
		_window->_inputManager->CompleteKeyboardFocusTransition(
			transition);
	}
	else if (!commit()) return false;
	return _keyboardFocus == element;
}

bool FocusManager::SetKeyboardFocus(
	Control* element,
	bool invalidateVisual,
	FocusChangeReason reason)
{
	if (element && !element->CanReceiveKeyboardFocus()) return false;
	return SetKeyboardFocusCore(
		element, invalidateVisual, reason, element != nullptr);
}

Control* FocusManager::PreferredOnceTarget(
	Control* group,
	bool tabOnly) const
{
	if (!group) return nullptr;
	if (auto* logical = ResolveLogicalFocus(group);
		logical && logical != group && IsDescendantOrSelf(logical, group)
		&& (tabOnly ? logical->CanParticipateInTabNavigation()
			: logical->CanReceiveKeyboardFocus()))
		return logical;
	std::size_t order = 0;
	std::vector<TabEntry> descendants;
	for (auto* child : group->GetVisualChildrenView())
		CollectTabEntries(child, false, tabOnly, order, descendants);
	if (descendants.empty()) return nullptr;
	std::stable_sort(descendants.begin(), descendants.end(),
		[](const TabEntry& left, const TabEntry& right)
		{
			return left.TabIndex != right.TabIndex
				? left.TabIndex < right.TabIndex
				: left.TreeOrder < right.TreeOrder;
		});
	return descendants.front().Value;
}

void FocusManager::CollectTabEntries(
	Control* root,
	bool honorOnce,
	bool tabOnly,
	std::size_t& treeOrder,
	std::vector<TabEntry>& entries) const
{
	if (!root) return;
	const auto currentOrder = treeOrder++;
	const bool eligible = tabOnly
		? root->CanParticipateInTabNavigation()
		: root->CanReceiveKeyboardFocus();
	if (eligible)
		entries.push_back(TabEntry{ root, root->TabIndex, currentOrder });

	const auto mode = tabOnly
		? root->TabNavigation : root->DirectionalNavigation;
	if (mode == KeyboardNavigationMode::None) return;
	if (honorOnce && mode == KeyboardNavigationMode::Once)
	{
		if (!eligible)
		{
			if (auto* preferred = PreferredOnceTarget(root, tabOnly))
				entries.push_back(TabEntry{
					preferred, root->TabIndex, currentOrder });
		}
		return;
	}
	for (auto* child : root->GetVisualChildrenView())
		CollectTabEntries(child, honorOnce, tabOnly, treeOrder, entries);
}

std::vector<Control*> FocusManager::BuildTabOrderFor(
	Control* scope,
	bool honorOnce,
	bool tabOnly) const
{
	std::vector<TabEntry> entries;
	std::size_t treeOrder = 0;
	if (scope && scope != _window)
		CollectTabEntries(scope, honorOnce, tabOnly, treeOrder, entries);
	else if (_window)
		for (auto* root : _window->GetVisualChildrenView())
			CollectTabEntries(root, honorOnce, tabOnly, treeOrder, entries);
	std::stable_sort(entries.begin(), entries.end(),
		[](const TabEntry& left, const TabEntry& right)
		{
			return left.TabIndex != right.TabIndex
				? left.TabIndex < right.TabIndex
				: left.TreeOrder < right.TreeOrder;
		});
	std::vector<Control*> result;
	result.reserve(entries.size());
	std::unordered_set<Control*> emitted;
	for (const auto& entry : entries)
		if (entry.Value && emitted.insert(entry.Value).second)
			result.push_back(entry.Value);
	return result;
}

std::vector<Control*> FocusManager::GetTabOrder(Control* scope) const
{
	if (scope && scope != _window && !IsOwned(scope)) return {};
	return BuildTabOrderFor(scope, true, true);
}

std::vector<Control*> FocusManager::GetFocusableOrder(Control* scope) const
{
	if (scope && scope != _window && !IsOwned(scope)) return {};
	return BuildTabOrderFor(scope, false, false);
}

std::vector<Control*> FocusManager::BuildTabOrder(
	std::span<Control* const> roots)
{
	std::vector<TabEntry> entries;
	std::size_t treeOrder = 0;
	auto collect = [&](Control* root, const auto& self) -> void
	{
		if (!root) return;
		const auto currentOrder = treeOrder++;
		if (root->CanParticipateInTabNavigation())
			entries.push_back(TabEntry{ root, root->TabIndex, currentOrder });
		if (root->TabNavigation == KeyboardNavigationMode::None) return;
		for (auto* child : root->GetVisualChildrenView()) self(child, self);
	};
	for (auto* root : roots) collect(root, collect);
	std::stable_sort(entries.begin(), entries.end(),
		[](const TabEntry& left, const TabEntry& right)
		{
			return left.TabIndex != right.TabIndex
				? left.TabIndex < right.TabIndex
				: left.TreeOrder < right.TreeOrder;
		});
	std::vector<Control*> result;
	result.reserve(entries.size());
	for (const auto& entry : entries) result.push_back(entry.Value);
	return result;
}

Control* FocusManager::FindNavigationBoundary(
	Control* origin,
	bool directional,
	KeyboardNavigationMode& mode) const noexcept
{
	mode = KeyboardNavigationMode::Continue;
	if (!_window) return nullptr;
	for (auto* current = origin ? origin->GetRoutedParent() : nullptr;
		current && current != _window; current = current->GetRoutedParent())
	{
		const auto candidate = directional
			? current->DirectionalNavigation : current->TabNavigation;
		if (IsBoundaryMode(candidate))
		{
			mode = candidate;
			return current;
		}
	}
	return _window;
}

Control* FocusManager::FindTabTarget(
	FocusNavigationDirection direction) const
{
	KeyboardNavigationMode boundaryMode = KeyboardNavigationMode::Continue;
	auto* boundary = FindNavigationBoundary(
		_keyboardFocus, false, boundaryMode);
	auto order = BuildTabOrderFor(
		boundary == _window ? nullptr : boundary, true, true);
	if (order.empty()) return nullptr;

	if (direction == FocusNavigationDirection::First) return order.front();
	if (direction == FocusNavigationDirection::Last) return order.back();
	const bool forward = direction == FocusNavigationDirection::Next;
	auto current = std::find(order.begin(), order.end(), _keyboardFocus);
	if (current == order.end()) return forward ? order.front() : order.back();
	const auto index = static_cast<std::size_t>(current - order.begin());
	if (forward && index + 1 < order.size()) return order[index + 1];
	if (!forward && index > 0) return order[index - 1];
	const bool wraps = boundary == _window
		|| boundaryMode == KeyboardNavigationMode::Cycle;
	return wraps ? (forward ? order.front() : order.back()) : nullptr;
}

Control* FocusManager::FindDirectionalTarget(
	FocusNavigationDirection direction) const
{
	if (!_keyboardFocus) return FindTabTarget(FocusNavigationDirection::First);
	KeyboardNavigationMode boundaryMode = KeyboardNavigationMode::Continue;
	auto* boundary = FindNavigationBoundary(
		_keyboardFocus, true, boundaryMode);
	auto candidates = BuildTabOrderFor(
		boundary == _window ? nullptr : boundary, false, false);
	const auto originRect = _keyboardFocus->GetAbsoluteRectDip();
	const float originX = CenterX(originRect);
	const float originY = CenterY(originRect);
	Control* best = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (auto* candidate : candidates)
	{
		if (!candidate || candidate == _keyboardFocus) continue;
		const auto rect = candidate->GetAbsoluteRectDip();
		const float dx = CenterX(rect) - originX;
		const float dy = CenterY(rect) - originY;
		float primary = 0.0f;
		float secondary = 0.0f;
		switch (direction)
		{
		case FocusNavigationDirection::Left:
			if (dx >= -0.001f) continue;
			primary = -dx; secondary = std::fabs(dy); break;
		case FocusNavigationDirection::Right:
			if (dx <= 0.001f) continue;
			primary = dx; secondary = std::fabs(dy); break;
		case FocusNavigationDirection::Up:
			if (dy >= -0.001f) continue;
			primary = -dy; secondary = std::fabs(dx); break;
		case FocusNavigationDirection::Down:
			if (dy <= 0.001f) continue;
			primary = dy; secondary = std::fabs(dx); break;
		default: continue;
		}
		const float score = primary * 1024.0f + secondary;
		if (score < bestScore)
		{
			bestScore = score;
			best = candidate;
		}
	}
	if (best || boundaryMode != KeyboardNavigationMode::Cycle)
		return best;

	// Cycle wraps to the opposite geometric edge while preserving the closest
	// perpendicular lane.
	float edgeScore = (std::numeric_limits<float>::max)();
	for (auto* candidate : candidates)
	{
		if (!candidate || candidate == _keyboardFocus) continue;
		const auto rect = candidate->GetAbsoluteRectDip();
		const float x = CenterX(rect);
		const float y = CenterY(rect);
		float edge = 0.0f;
		float lane = 0.0f;
		switch (direction)
		{
		case FocusNavigationDirection::Left:
			edge = -x; lane = std::fabs(y - originY); break;
		case FocusNavigationDirection::Right:
			edge = x; lane = std::fabs(y - originY); break;
		case FocusNavigationDirection::Up:
			edge = -y; lane = std::fabs(x - originX); break;
		case FocusNavigationDirection::Down:
			edge = y; lane = std::fabs(x - originX); break;
		default: continue;
		}
		const float score = edge * 1024.0f + lane;
		if (score < edgeScore)
		{
			edgeScore = score;
			best = candidate;
		}
	}
	return best;
}

bool FocusManager::MoveFocus(FocusNavigationDirection direction)
{
	++_statistics.NavigationRequests;
	Control* target = IsDirectional(direction)
		? FindDirectionalTarget(direction) : FindTabTarget(direction);
	if (!target || !target->CanReceiveKeyboardFocus()) return false;
	if (_window->Handle && ::GetFocus() != _window->Handle)
		::SetFocus(_window->Handle);
	if (!SetKeyboardFocus(
		target, true, FocusChangeReason::KeyboardNavigation)) return false;
	++_statistics.NavigationSucceeded;
	return true;
}

void FocusManager::DeactivateWindow()
{
	if (!_windowActive && !_keyboardFocus) return;
	_windowActive = false;
	if (_keyboardFocus) _suspendedKeyboardFocus = _keyboardFocus;
	(void)SetKeyboardFocusCore(
		nullptr, true, FocusChangeReason::WindowActivation, false);
}

bool FocusManager::ActivateWindow()
{
	if (_windowActive && _keyboardFocus) return true;
	_windowActive = true;
	auto* restore = _suspendedKeyboardFocus;
	_suspendedKeyboardFocus = nullptr;
	if (!IsOwned(restore) || !restore->CanReceiveKeyboardFocus())
		restore = ResolveLogicalFocus(_window);
	if (!restore || !restore->CanReceiveKeyboardFocus()) return false;
	if (!SetKeyboardFocusCore(
		restore, true, FocusChangeReason::WindowActivation, false)) return false;
	++_statistics.ActivationRestores;
	return true;
}

void FocusManager::OpenTransientScope(Control* scope)
{
	if (!scope || !IsOwned(scope)) return;
	auto* restore = _keyboardFocus;
	if (restore && IsDescendantOrSelf(restore, scope))
	{
		auto* parentScope = GetFocusScope(scope->GetRoutedParent());
		restore = ResolveLogicalFocus(parentScope);
	}
	_transientRestoreTargets[scope] = restore;
}

void FocusManager::CloseTransientScope(Control* scope)
{
	if (!scope) return;
	const auto found = _transientRestoreTargets.find(scope);
	if (found == _transientRestoreTargets.end()) return;
	auto* restore = found->second;
	_transientRestoreTargets.erase(found);
	const bool keyboardInside = _keyboardFocus
		&& IsDescendantOrSelf(_keyboardFocus, scope);
	const bool suspendedInside = _suspendedKeyboardFocus
		&& IsDescendantOrSelf(_suspendedKeyboardFocus, scope);
	if (!keyboardInside && !suspendedInside) return;
	if (!IsOwned(restore) || !restore->CanReceiveKeyboardFocus())
	{
		auto* parentScope = GetFocusScope(scope->GetRoutedParent());
		restore = ResolveLogicalFocus(parentScope);
	}
	if (!restore) return;
	if (suspendedInside)
	{
		_suspendedKeyboardFocus = restore;
		UpdateLogicalFocusChain(restore);
		++_statistics.PopupRestores;
	}
	else if (SetKeyboardFocus(
		restore, false, FocusChangeReason::PopupRestore))
	{
		++_statistics.PopupRestores;
	}
}

void FocusManager::DetachVisualChild(Control* root)
{
	if (!root) return;
	if (IsDescendantOrSelf(_keyboardFocus, root))
		(void)SetKeyboardFocusCore(
			nullptr, true, FocusChangeReason::TreeDetach, false);
	if (IsDescendantOrSelf(_suspendedKeyboardFocus, root))
		_suspendedKeyboardFocus = nullptr;
	for (auto it = _logicalFocus.begin(); it != _logicalFocus.end();)
	{
		if (IsDescendantOrSelf(it->first, root)
			|| IsDescendantOrSelf(it->second, root))
			it = _logicalFocus.erase(it);
		else ++it;
	}
	RefreshLogicalFocusProjection();
	for (auto it = _transientRestoreTargets.begin();
		it != _transientRestoreTargets.end();)
	{
		if (IsDescendantOrSelf(it->first, root)
			|| IsDescendantOrSelf(it->second, root))
			it = _transientRestoreTargets.erase(it);
		else ++it;
	}
}

void FocusManager::Reset() noexcept
{
	try
	{
		RefreshKeyboardFocusProjection(_keyboardFocus, nullptr);
		_logicalFocus.clear();
		RefreshLogicalFocusProjection();
	}
	catch (...)
	{
	}
	_keyboardFocus = nullptr;
	_suspendedKeyboardFocus = nullptr;
	_logicalFocusedElements.clear();
	_keyboardFocusWithinElements.clear();
	_transientRestoreTargets.clear();
	_windowActive = false;
}
