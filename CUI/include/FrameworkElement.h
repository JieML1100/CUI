#ifndef CUI_FRAMEWORK_ELEMENT_H_INCLUDED
#define CUI_FRAMEWORK_ELEMENT_H_INCLUDED
#pragma once

#include "Binding.h"
#include "CuiBuildFeatures.h"
#include "Layout/CanvasLayout.h"
#include "Layout/LayoutDeferral.h"
#include "Layout/LayoutTypes.h"
#include "UIElement.h"

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

class BindingCollection;
class BindingSourceProxy;
class Control;
class ControlStyleSheet;

namespace cui::framework
{
	struct TreeAccess;
}

enum class ControlStyleState : std::uint32_t
{
	None = 0,
	Hovered = 1u << 0,
	Focused = 1u << 1,
	Pressed = 1u << 2,
	Disabled = 1u << 3,
	Checked = 1u << 4,
	Selected = 1u << 5,
	LogicalFocused = 1u << 6,
	KeyboardFocusWithin = 1u << 7
};

inline ControlStyleState operator|(
	ControlStyleState left, ControlStyleState right) noexcept
{
	return static_cast<ControlStyleState>(
		static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

inline ControlStyleState operator&(
	ControlStyleState left, ControlStyleState right) noexcept
{
	return static_cast<ControlStyleState>(
		static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

inline ControlStyleState operator~(ControlStyleState value) noexcept
{
	return static_cast<ControlStyleState>(~static_cast<std::uint32_t>(value));
}

inline ControlStyleState& operator|=(
	ControlStyleState& left, ControlStyleState right) noexcept
{
	left = left | right;
	return left;
}

inline ControlStyleState& operator&=(
	ControlStyleState& left, ControlStyleState right) noexcept
{
	left = left & right;
	return left;
}

inline bool HasControlStyleState(
	ControlStyleState value, ControlStyleState state) noexcept
{
	return (value & state) == state;
}

using ParentChangedEvent = Event<void(Control*, Control*, Control*)>;

/**
 * Owns author-facing element semantics: logical/template relations, layout
 * declarations, resources, styles and inherited DataContext.
 */
class FrameworkElement : public UIElement
{
private:
	friend struct cui::framework::TreeAccess;

protected:
	Control* _logicalParent = nullptr;
	Control* _templatedParent = nullptr;
	std::vector<Control*> _logicalChildren;
	std::vector<Control*> _inheritanceChildren;

	cui::layout::LayoutDeferral _layoutDeferral;

	BindingSourceReference _effectiveDataContext;
	std::unique_ptr<BindingSourceProxy> _dataContextSource;
	PropertyChangedEvent _dataContextChanged;
	std::vector<EventConnection> _retainedEventConnections;

	std::wstring _styleResourceKey;
	// A StaticResource captured while expanding a framework Theme template is
	// resolved in that defining dictionary; consuming document dictionaries do
	// not get to reinterpret the same textual key.
	bool _styleResourceKeyCapturedFromTheme = false;
	// Container preparation can assign a Style resource reference that is not
	// authored on the element and must not be persisted by Designer round-trips.
	bool _styleResourceKeyIsAutomatic = false;
	ControlStyleState _styleState = ControlStyleState::None;
	std::shared_ptr<const ControlStyleSheet> _themeStyleSheet;
	std::shared_ptr<const ControlStyleSheet> _styleSheet;
	std::shared_ptr<const ControlStyleSheet> _resourceDictionary;
#if CUI_ENABLE_DYNAMIC_XAML
	EventConnection _themeStyleConnection;
	EventConnection _styleSheetConnection;
	EventConnection _resourceDictionaryConnection;
#endif
	EventConnection _stylePropertyConditionConnection;
	std::vector<EventConnection> _styleStateConnections;
	std::vector<EventConnection> _styleDataContextConnections;
	/** Object and list path segments only need shared lifetime retention. */
	std::vector<std::shared_ptr<void>> _styleDataContextOwners;
	std::array<std::vector<DependencyPropertyReference>, 2>
		_styleSheetProperties;
	bool _refreshingStyleValues = false;
	bool _styleRefreshPending = false;
	bool _refreshingDynamicResources = false;
	ParentChangedEvent OnLogicalParentChanged;
	ParentChangedEvent OnTemplatedParentChanged;

	/** Internal invalidation batching state; intentionally not a public API. */
	bool IsLayoutSuspended() const noexcept
	{
		return _layoutDeferral.IsSuspended();
	}

public:
	FrameworkElement() = default;
	~FrameworkElement() override = default;

	Control* GetLogicalParent() const noexcept { return _logicalParent; }
	Control* GetTemplatedParent() const noexcept { return _templatedParent; }
	Control* GetInheritanceParent() const noexcept
	{
		// WPF FrameworkParent is logical-first, then the containing visual.
		// TemplatedParent is a separate template relationship and must not be
		// used as a substitute for either tree.
		return _logicalParent ? _logicalParent : _visualParent;
	}
	Control* GetRoutedParent() const noexcept
	{
		return _visualParent ? _visualParent
			: (_logicalParent ? _logicalParent : _templatedParent);
	}
	std::span<Control* const> GetLogicalChildrenView() noexcept
	{
		return { _logicalChildren.data(), _logicalChildren.size() };
	}
	std::span<Control* const> GetLogicalChildrenView() const noexcept
	{
		return { _logicalChildren.data(), _logicalChildren.size() };
	}

	virtual bool SetDataContext(BindingSourceReference value) = 0;
	virtual bool ClearDataContext() = 0;
	virtual IBindingSource& DataContextSource() = 0;
};

#endif // CUI_FRAMEWORK_ELEMENT_H_INCLUDED
