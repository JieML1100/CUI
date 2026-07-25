#pragma once

#include "Binding.h"
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
	struct DesignIdentityAccess;
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
	// Editor/compiler identity is infrastructure metadata, never an author DP.
	int _designId = 0;
	friend struct cui::framework::DesignIdentityAccess;
	friend struct cui::framework::TreeAccess;

protected:
	Control* _logicalParent = nullptr;
	Control* _templatedParent = nullptr;
	std::vector<Control*> _logicalChildren;
	std::vector<Control*> _inheritanceChildren;

	// Intrinsic measurement input only. Specified geometry lives in
	// LayoutStyle and computed geometry lives in LayoutState.
	cui::core::Size _naturalSize{ 120.0f, 20.0f };
	float _canvasLeft = cui::layout::UnsetCanvasOffset;
	float _canvasTop = cui::layout::UnsetCanvasOffset;
	float _canvasRight = cui::layout::UnsetCanvasOffset;
	float _canvasBottom = cui::layout::UnsetCanvasOffset;
	Thickness _margin;
	Thickness _padding;
	HorizontalAlignment _horizontalAlignment = HorizontalAlignment::Stretch;
	VerticalAlignment _verticalAlignment = VerticalAlignment::Stretch;
	int _gridRow = 0;
	int _gridColumn = 0;
	int _gridRowSpan = 1;
	int _gridColumnSpan = 1;
	Dock _dock = Dock::Left;
	cui::layout::LayoutStyle _layoutStyle;
	cui::layout::LayoutDeferral _layoutDeferral;

	BindingSourceReference _effectiveDataContext;
	std::unique_ptr<BindingSourceProxy> _dataContextSource;
	PropertyChangedEvent _dataContextChanged;
	std::vector<EventConnection> _retainedEventConnections;

	std::wstring _styleResourceKey;
	ControlStyleState _styleState = ControlStyleState::None;
	std::shared_ptr<const ControlStyleSheet> _themeStyleSheet;
	std::shared_ptr<const ControlStyleSheet> _styleSheet;
	std::shared_ptr<const ControlStyleSheet> _resourceDictionary;
	EventConnection _themeStyleConnection;
	EventConnection _styleSheetConnection;
	EventConnection _resourceDictionaryConnection;
	EventConnection _stylePropertyConditionConnection;
	std::vector<EventConnection> _styleStateConnections;
	std::vector<EventConnection> _styleDataContextConnections;
	std::vector<std::shared_ptr<IBindingSource>> _styleDataContextOwners;
	std::array<std::vector<std::wstring>, 2> _styleSheetProperties;
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

	int GetDesignId() const noexcept { return _designId; }

	Control* GetLogicalParent() const noexcept { return _logicalParent; }
	Control* GetTemplatedParent() const noexcept { return _templatedParent; }
	Control* GetInheritanceParent() const noexcept
	{
		return _logicalParent ? _logicalParent : _templatedParent;
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

namespace cui::framework
{
	/** Narrow bridge used only by XAML materialization, codegen and designer identity maps. */
	struct DesignIdentityAccess final
	{
		static void Set(FrameworkElement& target, int value) noexcept
		{
			target._designId = value > 0 ? value : 0;
		}
	};
}
