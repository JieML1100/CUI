#include "Menu.h"
#include "ContextMenu.h"
#include "DependencyPropertyInfrastructure.h"
#include "EventInfrastructure.h"
#include "Popup.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "TreeInfrastructure.h"
#include "Window.h"
#include <algorithm>

namespace
{
	DependencyPropertyOptions<MenuItem, bool> MenuItemBooleanOptions(
		bool defaultValue CUI_DESIGN_METADATA_ARGUMENTS(int order),
		DependencyPropertyFlags flags = DependencyPropertyFlags::None)
	{
		DependencyPropertyOptions<MenuItem, bool> options;
		options.DefaultValue = defaultValue;
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	MenuItem* AsMenuItem(Control* item)
	{
		return dynamic_cast<MenuItem*>(item);
	}

	bool IsInteractive(Control* item)
	{
		auto* menuItem = AsMenuItem(item);
		return menuItem && menuItem->IsVisible
			&& menuItem->IsEffectivelyEnabled();
	}

	bool FindMenuItemPath(
		std::span<Control* const> items,
		const MenuItem* target,
		std::vector<int>& path)
	{
		for (int index = 0; index < static_cast<int>(items.size()); ++index)
		{
			auto* item = AsMenuItem(items[static_cast<size_t>(index)]);
			if (!item) continue;
			path.push_back(index);
			if (item == target
				|| FindMenuItemPath(item->GetMenuItemsView(), target, path))
				return true;
			path.pop_back();
		}
		return false;
	}
}

UIClass MenuItem::Type() { return UIClass::UI_MenuItem; }

const DependencyProperty& MenuItem::CommandProperty()
{
	static const auto registration = []
	{
		auto options = DependencyPropertyOptions<MenuItem, std::wstring>{
			std::wstring{}, DependencyPropertyFlags::None };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			MenuItem& target, const std::wstring&, const std::wstring&)
		{
			target.RefreshCommandSource();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			MenuItem, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Command"),
				[](MenuItem& target) { return target.Command; },
				[](MenuItem& target, const std::wstring& value)
				{ target.Command = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MenuItem::CommandParameterProperty()
{
	static const auto registration = []
	{
		auto options = DependencyPropertyOptions<MenuItem, std::wstring>{
			std::wstring{}, DependencyPropertyFlags::None };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			MenuItem& target, const std::wstring&, const std::wstring&)
		{
			target.RefreshCommandSource();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			MenuItem, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"CommandParameter"),
				[](MenuItem& target) { return target.CommandParameter; },
				[](MenuItem& target, const std::wstring& value)
				{ target.CommandParameter = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MenuItem::InputGestureTextProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<MenuItem, std::wstring> options{
			std::wstring{}, DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 25;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			MenuItem& target, const std::wstring&, const std::wstring&)
		{
			if (target._structureChanged) target._structureChanged();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			MenuItem, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"InputGestureText"),
				[](MenuItem& target) { return target.InputGestureText; },
				[](MenuItem& target, const std::wstring& value)
				{ target.InputGestureText = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MenuItem::IconProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<MenuItem, BindingValue> options;
		options.DefaultValue = BindingValue{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 15;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			MenuItem, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"Icon"),
				[](MenuItem& target) { return target.GetIcon(); },
				[](MenuItem& target, const BindingValue& value)
				{ target.SetIcon(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MenuItem::IsCheckableProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<MenuItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsCheckable"),
			[](MenuItem& target) { return target.IsCheckable; },
			[](MenuItem& target, const bool& value)
			{ target.IsCheckable = value; }, {},
			MenuItemBooleanOptions(false CUI_DESIGN_METADATA_ARGUMENTS(30)));
	return *registration;
}

const DependencyProperty& MenuItem::IsCheckedProperty()
{
	static const auto registration = []
	{
		auto options = MenuItemBooleanOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(35),
			DependencyPropertyFlags::AffectsRender);
		options.Changed = [](
			MenuItem& target, const bool&, const bool& value)
		{
			const ControlWeakReference lifetime(&target);
			target.SetStyleState(ControlStyleState::Checked, value);
			RoutedEventArgs args;
			if (value) target.Checked(&target, args);
			else target.Unchecked(&target, args);
			if (auto* source = dynamic_cast<MenuItem*>(lifetime.Get()))
				source->NotifyAccessibilityStateChanged();
		};
		return DependencyPropertyRegistry::RegisterStatic<MenuItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsChecked"),
			[](MenuItem& target) { return target.IsChecked; },
			[](MenuItem& target, const bool& value)
			{ target.IsChecked = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MenuItem::StaysOpenOnClickProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<MenuItem, bool>(
			DependencyPropertyRegistrationLiteral(L"StaysOpenOnClick"),
			[](MenuItem& target) { return target.StaysOpenOnClick; },
			[](MenuItem& target, const bool& value)
			{ target.StaysOpenOnClick = value; }, {},
			MenuItemBooleanOptions(false CUI_DESIGN_METADATA_ARGUMENTS(40)));
	return *registration;
}

const DependencyProperty& MenuItem::IsHighlightedProperty()
{
	return IsHighlightedPropertyKey().Property();
}

const DependencyProperty& MenuItem::IsPressedProperty()
{
	return IsPressedPropertyKey().Property();
}

const DependencyProperty& MenuItem::RoleProperty()
{
	return RolePropertyKey().Property();
}

const DependencyPropertyKey& MenuItem::IsHighlightedPropertyKey()
{
	static const auto registration = []
	{
		auto options = MenuItemBooleanOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(45),
			DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<MenuItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsHighlighted"),
			[](MenuItem& target) { return target.IsHighlighted; },
			[](MenuItem& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					IsHighlightedPropertyKey(), target._isHighlighted, value);
			}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyPropertyKey& MenuItem::IsPressedPropertyKey()
{
	static const auto registration = []
	{
		auto options = MenuItemBooleanOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(47),
			DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<MenuItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsPressed"),
			[](MenuItem& target) { return target.IsPressed; },
			[](MenuItem& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					IsPressedPropertyKey(), target._isPressed, value);
			}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyPropertyKey& MenuItem::RolePropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<MenuItem, MenuItemRole> options;
		options.DefaultValue = MenuItemRole::TopLevelItem;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 49;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		options.Design.Choices = {
			{ L"TopLevelItem", BindingValue(MenuItemRole::TopLevelItem) },
			{ L"TopLevelHeader", BindingValue(MenuItemRole::TopLevelHeader) },
			{ L"SubmenuItem", BindingValue(MenuItemRole::SubmenuItem) },
			{ L"SubmenuHeader", BindingValue(MenuItemRole::SubmenuHeader) }
		};
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			MenuItem, MenuItemRole>(
				DependencyPropertyRegistrationLiteral(L"Role"),
				[](MenuItem& target) { return target.Role; },
				[](MenuItem& target, const MenuItemRole& value)
				{
					(void)target.SetReadOnlyPropertyField(
						RolePropertyKey(), target._role, value);
				}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyProperty& MenuItem::IsSubmenuOpenProperty()
{
	static const auto registration = []
	{
		auto options = MenuItemBooleanOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(50),
			DependencyPropertyFlags::AffectsRender);
		options.Changed = [](
			MenuItem& target, const bool&, const bool& value)
		{
			const ControlWeakReference lifetime(&target);
			target.ConfigureSubmenuPopup(target.ResolveSubmenuPopup());
			if (!value)
				for (auto* entry : target._items)
					if (auto* child = AsMenuItem(entry))
					{
						child->SetIsSubmenuOpenCore(false);
						child->SetIsHighlightedCore(false);
					}
			RoutedEventArgs args;
			if (value) target.SubmenuOpened(&target, args);
			else target.SubmenuClosed(&target, args);
			auto* source = dynamic_cast<MenuItem*>(lifetime.Get());
			if (source && !source->_projectingInteractionState
				&& source->_interactionStateChanged)
				source->_interactionStateChanged(*source);
		};
		return DependencyPropertyRegistry::RegisterStatic<MenuItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSubmenuOpen"),
			[](MenuItem& target) { return target.IsSubmenuOpen; },
			[](MenuItem& target, const bool& value)
			{ target.IsSubmenuOpen = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& MenuItem::CommandTargetProperty()
{
	static const auto registration = []
	{
		auto options =
			DependencyPropertyOptions<MenuItem, ControlWeakReference>{
				ControlWeakReference{}, DependencyPropertyFlags::None };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 60;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			MenuItem, ControlWeakReference>(
				DependencyPropertyRegistrationLiteral(L"CommandTarget"),
				[](MenuItem& target) { return target._commandTarget; },
				[](MenuItem& target, const ControlWeakReference& value)
				{ target.ApplyCommandTarget(value); }, {}, std::move(options));
	}();
	return *registration;
}

void MenuItem::RegisterDependencyProperties()
{
	HeaderedItemsControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)CommandProperty();
	(void)CommandParameterProperty();
	(void)InputGestureTextProperty();
	(void)IconProperty();
	(void)IsCheckableProperty();
	(void)IsCheckedProperty();
	(void)StaysOpenOnClickProperty();
	(void)IsHighlightedProperty();
	(void)IsPressedProperty();
	(void)RoleProperty();
	(void)IsSubmenuOpenProperty();
	(void)CommandTargetProperty();
#endif
}

GET_CPP(MenuItem, std::wstring, Command) { return _command; }
SET_CPP(MenuItem, std::wstring, Command)
{
	SetPropertyField(CommandProperty(), _command, value);
}
GET_CPP(MenuItem, std::wstring, CommandParameter)
{
	return _commandParameter;
}
SET_CPP(MenuItem, std::wstring, CommandParameter)
{
	SetPropertyField(CommandParameterProperty(), _commandParameter, value);
}
void MenuItem::SetIcon(BindingValue value)
{
	(void)SetPropertyField(IconProperty(), _icon, std::move(value));
}
GET_CPP(MenuItem, std::wstring, InputGestureText)
{
	return _inputGestureText;
}
SET_CPP(MenuItem, std::wstring, InputGestureText)
{
	(void)SetPropertyField(
		InputGestureTextProperty(), _inputGestureText, std::move(value));
}
GET_CPP(MenuItem, bool, IsCheckable) { return _isCheckable; }
SET_CPP(MenuItem, bool, IsCheckable)
{
	(void)SetPropertyField(IsCheckableProperty(), _isCheckable, value);
}
GET_CPP(MenuItem, bool, IsChecked) { return _isChecked; }
SET_CPP(MenuItem, bool, IsChecked)
{
	(void)SetPropertyField(IsCheckedProperty(), _isChecked, value);
}
GET_CPP(MenuItem, bool, StaysOpenOnClick) { return _staysOpenOnClick; }
SET_CPP(MenuItem, bool, StaysOpenOnClick)
{
	(void)SetPropertyField(
		StaysOpenOnClickProperty(), _staysOpenOnClick, value);
}
GET_CPP(MenuItem, bool, IsHighlighted) { return _isHighlighted; }
GET_CPP(MenuItem, bool, IsPressed) { return _isPressed; }
GET_CPP(MenuItem, MenuItemRole, Role) { return _role; }
GET_CPP(MenuItem, bool, IsSubmenuOpen) { return _isSubmenuOpen; }
SET_CPP(MenuItem, bool, IsSubmenuOpen)
{
	(void)SetPropertyField(
		IsSubmenuOpenProperty(), _isSubmenuOpen,
		value && !_items.empty());
}

void MenuItem::SetIsHighlightedCore(bool value)
{
	if (_isHighlighted == value) return;
	if (!SetReadOnlyPropertyField(
		IsHighlightedPropertyKey(), _isHighlighted, value)) return;
	SetStyleState(ControlStyleState::Hovered, value);
}

void MenuItem::SetIsSubmenuOpenCore(bool value)
{
	if (value && _items.empty()) value = false;
	if (_isSubmenuOpen == value) return;
	if (value) ConfigureSubmenuPopup(ResolveSubmenuPopup());
	_projectingInteractionState = true;
	(void)SetCurrentPropertyField(
		IsSubmenuOpenProperty(), _isSubmenuOpen, value);
	_projectingInteractionState = false;
	if (!value)
		for (auto* entry : _items)
			if (auto* child = AsMenuItem(entry))
			{
				child->SetIsSubmenuOpenCore(false);
				child->SetIsHighlightedCore(false);
			}
	if (_interactionStateChanged)
		_interactionStateChanged(*this);
}

void MenuItem::SetIsPressedCore(bool value)
{
	if (_isPressed == value) return;
	if (!SetReadOnlyPropertyField(
		IsPressedPropertyKey(), _isPressed, value)) return;
	SetStyleState(ControlStyleState::Pressed, value);
}

void MenuItem::SetHeader(BindingValue value)
{
	HeaderedItemsControl::SetHeader(value);
	if (_structureChanged) _structureChanged();
}

GET_CPP(MenuItem, Control*, CommandTarget) { return _commandTarget.Get(); }
SET_CPP(MenuItem, Control*, CommandTarget)
{
	const ControlWeakReference sourceLifetime(this);
	if (value)
	{
		(void)TrySetPropertyValue(
			CommandTargetProperty(),
			BindingValue(ControlWeakReference(value)),
			DependencyPropertyValueSource::Local);
		return;
	}
	if (ClearPropertyValue(
		CommandTargetProperty(), DependencyPropertyValueSource::Local))
		return;
	if (auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get()))
		source->ApplyCommandTarget({});
}

void MenuItem::ClearCommandTarget()
{
	SetCommandTarget(nullptr);
}

void MenuItem::ApplyCommandTarget(const ControlWeakReference& value)
{
	const ControlWeakReference sourceLifetime(this);
	if (_commandTarget == value) return;
	_commandTarget = value;
	if (auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get()))
		source->RefreshCommandSource();
}

MenuItem::MenuItem()
	: HeaderedItemsControl()
{
	RegisterDependencyProperties();
	if (auto* itemsHost = GetItemsHost())
		cui::framework::TemplateAccess::
			SetParticipatesInPresentationScene(*itemsHost, false);
	RetainEventConnection(OnPropertyValueChanged.Subscribe(
		[this](DependencyObject*, const DependencyPropertyChangedEventArgs& args)
		{
			if ((args.Property == &Control::IsEnabledProperty()
				|| args.Property == &Control::VisibilityProperty()
				|| args.Property == &Control::IsVisibleProperty())
				&& _interactionStateChanged)
				_interactionStateChanged(*this);
		}));
	this->_backcolor = D2D1_COLOR_F{ 0,0,0,0 };
	this->_bordercolor = D2D1_COLOR_F{ 0,0,0,0 };
	this->_forecolor = cui::theme::palette::TextPrimary;
	(void)TrySetPropertyValue(
		Control::CursorProperty(), BindingValue(CursorKind::Hand),
		DependencyPropertyValueSource::Theme);
	RetainEventConnection(OnMouseEnter.Subscribe(
		[this](Control*, MouseEventArgs&)
		{
			if (!IsEffectivelyEnabled()) return;
			SetIsHighlightedCore(true);
			if (ShouldOpenSubmenuOnPointerMove())
				SetIsSubmenuOpenCore(true);
		}));
	RetainEventConnection(OnMouseLeave.Subscribe(
		[this](Control*, MouseEventArgs&)
		{
			if (!_isSubmenuOpen)
				SetIsHighlightedCore(false);
		}));
	UpdateRole();
}

MenuItem::~MenuItem()
{
	_structureChanged = {};
	_interactionStateChanged = {};
	_commandCanExecuteConnection.Disconnect();
	_submenuPopupOpened.Disconnect();
	_submenuPopupClosed.Disconnect();
	for (auto* entry : _items)
	{
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		item->SetStructureChangedHandler({});
		item->SetInteractionStateChangedHandler({});
		item->DetachCommandHost(*this);
		item->_parentItem = nullptr;
	}
	_items.clear();
}

void MenuItem::SetStructureChangedHandler(std::function<void()> handler)
{
	_structureChanged = std::move(handler);
	for (auto* entry : _items)
	{
		auto* child = AsMenuItem(entry);
		if (!child) continue;
		child->_parentItem = this;
		child->SetStructureChangedHandler(_structureChanged);
	}
}

void MenuItem::SetInteractionStateChangedHandler(
	std::function<void(MenuItem&)> handler)
{
	_interactionStateChanged = std::move(handler);
	for (auto* entry : _items)
	{
		auto* child = AsMenuItem(entry);
		if (!child) continue;
		child->SetInteractionStateChangedHandler(
			_interactionStateChanged);
	}
}

void MenuItem::RefreshCommandSource()
{
	const auto refreshVersion = ++_commandSourceRefreshVersion;
	_commandCanExecuteConnection.Disconnect();
	if (_command.empty())
	{
		ClearCommandCanExecuteState();
		return;
	}
	const ControlWeakReference sourceLifetime(this);
	auto connection = RoutedCommandManager::ObserveCanExecute(
		*this,
		RoutedCommandSourceQuery{
			RoutedCommand(_command), _commandParameter,
			EffectiveCommandTarget() },
		[sourceLifetime, refreshVersion](
			Control& source, const RoutedCommandCanExecuteResult& result)
		{
			auto* current = dynamic_cast<MenuItem*>(sourceLifetime.Get());
			if (current != &source
				|| current->_commandSourceRefreshVersion != refreshVersion)
				return;
			current->SetCommandCanExecuteState(result.CanExecute);
		});
	// Initial CanExecute is synchronous and may re-enter authored properties or
	// destroy the MenuItem.  Only the still-current source owns the connection.
	auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (!source || source->_commandSourceRefreshVersion != refreshVersion)
		return;
	source->_commandCanExecuteConnection = std::move(connection);
}

void MenuItem::SynchronizeCommandContext(
	Window* window,
	ControlWeakReference defaultCommandTarget,
	bool commandRouteChanged)
{
	const ControlWeakReference sourceLifetime(this);
	const ControlWeakReference windowLifetime(window);
	const bool hostChanged = GetPresentationWindow() != window;
	_defaultCommandTarget = defaultCommandTarget;
	Control::PropagatePresentationWindow(this, window);
	auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (!source || hostChanged) return;
	if (commandRouteChanged)
		source->RefreshCommandSource();
	source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (!source) return;
	std::vector<ControlWeakReference> children;
	children.reserve(source->_items.size());
	for (auto* entry : source->_items)
		if (auto* child = AsMenuItem(entry)) children.emplace_back(child);
	for (const auto& childLifetime : children)
	{
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		auto* child = dynamic_cast<MenuItem*>(childLifetime.Get());
		if (!source || !child || source->IndexOfSubItem(child) < 0)
			continue;
		child->_parentItem = source;
		if (child->GetLogicalParent() != source)
			cui::framework::TreeAccess::SetLogicalParent(*child, source);
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		child = dynamic_cast<MenuItem*>(childLifetime.Get());
		if (!source || !child || source->IndexOfSubItem(child) < 0)
			continue;
		auto* currentWindow = window
			? dynamic_cast<Window*>(windowLifetime.Get()) : nullptr;
		if (window && !currentWindow) return;
		child->SynchronizeCommandContext(
			currentWindow, defaultCommandTarget,
			commandRouteChanged);
	}
}

void MenuItem::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	(void)previousWindow;
	const ControlWeakReference sourceLifetime(this);
	const ControlWeakReference windowLifetime(currentWindow);
	RefreshCommandSource();
	auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (!source) return;
	std::vector<ControlWeakReference> children;
	children.reserve(source->_items.size());
	for (auto* entry : source->_items)
		if (auto* child = AsMenuItem(entry)) children.emplace_back(child);
	for (const auto& childLifetime : children)
	{
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		auto* child = dynamic_cast<MenuItem*>(childLifetime.Get());
		if (!source || !child || source->IndexOfSubItem(child) < 0)
			continue;
		child->_parentItem = source;
		if (child->GetLogicalParent() != source)
			cui::framework::TreeAccess::SetLogicalParent(*child, source);
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		child = dynamic_cast<MenuItem*>(childLifetime.Get());
		if (!source || !child || source->IndexOfSubItem(child) < 0)
			continue;
		auto* liveWindow = currentWindow
			? dynamic_cast<Window*>(windowLifetime.Get()) : nullptr;
		if (currentWindow && !liveWindow) return;
		child->SynchronizeCommandContext(
			liveWindow, source->_defaultCommandTarget, true);
	}
}

void MenuItem::AttachCommandHost(
	Control& routedOwner, ControlWeakReference defaultCommandTarget)
{
	const ControlWeakReference sourceLifetime(this);
	const ControlWeakReference ownerLifetime(&routedOwner);
	if (GetLogicalParent() != &routedOwner)
		SetLogicalParent(&routedOwner);
	auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	auto* owner = ownerLifetime.Get();
	if (!source || !owner) return;
	auto* window = owner->GetPresentationWindow();
	for (auto* current = owner; !window && current;
		current = current->GetRoutedParent())
		window = current->GetPresentationWindow();
	source->SynchronizeCommandContext(
		window, defaultCommandTarget, true);
	source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (source) source->UpdateRole();
}

void MenuItem::DetachCommandHost(Control& routedOwner)
{
	const ControlWeakReference sourceLifetime(this);
	if (GetLogicalParent() == &routedOwner)
		SetLogicalParent(nullptr);
	if (auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get()))
	{
		source->SynchronizeCommandContext(nullptr, nullptr, true);
		source->UpdateRole();
	}
}

bool MenuItem::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const MenuItem*>(&item)
		|| dynamic_cast<const Separator*>(&item)) return true;
	error = "MenuItem Items can contain MenuItem or Separator controls only";
	return false;
}

void MenuItem::OnBeforeGeneratedItemsRebuilt()

{
	_generatedItemsRebuildSnapshot.clear();
	_generatedItemsRebuildSnapshot.reserve(_items.size());
	for (auto* entry : _items)
		_generatedItemsRebuildSnapshot.emplace_back(entry);
	_generatedItemsRebuildPending = true;
	for (auto* entry : _items)
	{
		auto* child = AsMenuItem(entry);
		if (!child) continue;
		child->SetStructureChangedHandler({});
		child->SetInteractionStateChangedHandler({});
		child->DetachCommandHost(*this);
		if (child->_parentItem == this) child->_parentItem = nullptr;
	}
	_items.clear();
}

void MenuItem::SynchronizeItems()

{
	std::vector<Control*> current;
	current.reserve(ItemCount());
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		auto* child = GetGeneratedItem(index);
		if (child) current.push_back(child);
	}
	bool structureChanged = current != _items;
	if (_generatedItemsRebuildPending)
	{
		structureChanged = current.size()
			!= _generatedItemsRebuildSnapshot.size();
		if (!structureChanged)
			for (size_t index = 0; index < current.size(); ++index)
				if (_generatedItemsRebuildSnapshot[index].Get()
					!= current[index])
				{
					structureChanged = true;
					break;
				}
		_generatedItemsRebuildSnapshot.clear();
		_generatedItemsRebuildPending = false;
	}

	for (auto* entry : _items)
	{
		if (std::find(current.begin(), current.end(), entry)
			!= current.end()) continue;
		auto* child = AsMenuItem(entry);
		if (!child) continue;
		child->SetStructureChangedHandler({});
		child->SetInteractionStateChangedHandler({});
		child->DetachCommandHost(*this);
		if (child->_parentItem == this) child->_parentItem = nullptr;
	}
	_items = std::move(current);
	for (auto* entry : _items)
	{
		auto* child = AsMenuItem(entry);
		if (!child) continue;
		child->_parentItem = this;
		child->SetStructureChangedHandler(_structureChanged);
		child->SetInteractionStateChangedHandler(
			_interactionStateChanged);
		child->AttachCommandHost(*this, _defaultCommandTarget);
	}
	UpdateRole();
	if (structureChanged && _structureChanged) _structureChanged();
}

void MenuItem::OnAuthoredItemsChanged() noexcept
{
	try { SynchronizeItems(); }
	catch (...) { _items.clear(); }
}

void MenuItem::OnGeneratedItemsRebuilt()
{
	SynchronizeItems();
}

std::unique_ptr<Panel> MenuItem::CreateItemsHost() const
{
	auto itemsHost = HeaderedItemsControl::CreateItemsHost();
	if (itemsHost)
		cui::framework::TemplateAccess::
			SetParticipatesInPresentationScene(
				*itemsHost, ItemsHostUsesSubmenuPopup());
	return itemsHost;
}

void MenuItem::ConfigureHeaderVisual(Control& child)
{
	HeaderedItemsControl::ConfigureHeaderVisual(child);
	cui::framework::TemplateAccess::
		SetParticipatesInPresentationScene(child, false);
}

void MenuItem::ReleaseHeaderVisual(Control& child)
{
	cui::framework::TemplateAccess::
		SetParticipatesInPresentationScene(child, true);
	HeaderedItemsControl::ReleaseHeaderVisual(child);
}

void MenuItem::SynchronizeItemsHostPresentation()
{
	if (auto* itemsHost = GetItemsHost())
		cui::framework::TemplateAccess::
			SetParticipatesInPresentationScene(
				*itemsHost, ItemsHostUsesSubmenuPopup());
}

void MenuItem::OnControlTemplatePresentationChanged()
{
	HeaderedItemsControl::OnControlTemplatePresentationChanged();
	SynchronizeItemsHostPresentation();
	_submenuPopupOpened.Disconnect();
	_submenuPopupClosed.Disconnect();
	_submenuPopup = nullptr;
	ConfigureSubmenuPopup(ResolveSubmenuPopup());
	UpdateRole();
}

void MenuItem::OnApplyTemplate()
{
	// Generated templates publish their root before attaching the complete
	// Popup/ItemsPresenter subtree. ItemsControl commits a pending presenter in
	// OnApplyTemplate, so this is the first lifecycle point where the WPF
	// submenu host ancestry is final and safe to project into the transient scene.
	HeaderedItemsControl::OnApplyTemplate();
	SynchronizeItemsHostPresentation();
}

void MenuItem::UpdateRole()
{
	const bool hasItems = !_items.empty();
	MenuItemRole value;
	if (_parentItem)
		value = hasItems
			? MenuItemRole::SubmenuHeader
			: MenuItemRole::SubmenuItem;
	else if (dynamic_cast<Menu*>(ResolveMenuHost()))
		value = hasItems
			? MenuItemRole::TopLevelHeader
			: MenuItemRole::TopLevelItem;
	else
		value = hasItems
			? MenuItemRole::SubmenuHeader
			: MenuItemRole::SubmenuItem;
	if (_role != value)
		(void)SetReadOnlyPropertyField(
			RolePropertyKey(), _role, value);
	if (!hasItems && _isSubmenuOpen)
		SetIsSubmenuOpenCore(false);
}

Popup* MenuItem::ResolveSubmenuPopup() const noexcept
{
	return dynamic_cast<Popup*>(
		const_cast<MenuItem*>(this)
			->FindDeclarativeTemplatePart(
				MakeTemplatePartToken(L"PART_Popup")));
}

bool MenuItem::ItemsHostUsesSubmenuPopup() const noexcept
{
	auto* popup = ResolveSubmenuPopup();
	if (!popup) return false;
	const Control* current = GetTemplateItemsPresenter();
	if (!current) current = GetItemsHost();
	for (; current && current != this;
		current = current->GetVisualParent())
		if (current == popup) return true;
	return false;
}

void MenuItem::ConfigureSubmenuPopup(Popup* popup)
{
	// Opening can revisit the same Popup after ItemsControl has committed its
	// pending ItemsPresenter. Refresh before the identity fast path so that
	// late template attachment cannot leave the submenu host pruned from the
	// transient presentation scene.
	SynchronizeItemsHostPresentation();
	if (_submenuPopup == popup)
	{
		if (_submenuPopup)
			(void)_submenuPopup->TrySetCurrentPropertyValue(
				Popup::IsOpenProperty(), BindingValue(_isSubmenuOpen));
		return;
	}
	_submenuPopupOpened.Disconnect();
	_submenuPopupClosed.Disconnect();
	if (_submenuPopup && _submenuPopup != popup)
		(void)_submenuPopup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(false));
	_submenuPopup = popup;
	if (!_submenuPopup) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_submenuPopup, Popup::PlacementTargetProperty(),
		BindingValue(ControlWeakReference(this)),
		DependencyPropertyValueSource::Template);
	_submenuPopupOpened = _submenuPopup->Opened.Subscribe(
		[this](Popup*)
		{
			if (!_isSubmenuOpen)
				(void)SetCurrentPropertyField(
					IsSubmenuOpenProperty(), _isSubmenuOpen, true);
		});
	_submenuPopupClosed = _submenuPopup->Closed.Subscribe(
		[this](Popup*)
		{
			if (_isSubmenuOpen)
				(void)SetCurrentPropertyField(
					IsSubmenuOpenProperty(), _isSubmenuOpen, false);
		});
	(void)_submenuPopup->TrySetCurrentPropertyValue(
		Popup::IsOpenProperty(), BindingValue(_isSubmenuOpen));
}

Control* MenuItem::ResolveMenuHost() const noexcept
{
	for (auto* current = GetLogicalParent(); current;
		current = current->GetRoutedParent())
		if (dynamic_cast<Menu*>(current)
			|| dynamic_cast<ContextMenu*>(current))
			return current;
	return nullptr;
}

bool MenuItem::IsInMenuMode() const noexcept
{
	if (const auto* menu = dynamic_cast<const Menu*>(ResolveMenuHost()))
		return menu->IsMenuModeActive();
	if (const auto* menu =
		dynamic_cast<const ContextMenu*>(ResolveMenuHost()))
		return menu->GetIsOpen();
	return false;
}

bool MenuItem::ShouldOpenSubmenuOnPointerMove() const noexcept
{
	return !_items.empty() && IsInMenuMode();
}

MenuItem* MenuItem::FindNavigableChild(bool last) const noexcept
{
	if (_items.empty()) return nullptr;
	if (last)
	{
		for (auto current = _items.rbegin();
			current != _items.rend(); ++current)
			if (IsInteractive(*current))
				return AsMenuItem(*current);
		return nullptr;
	}
	for (auto* current : _items)
		if (IsInteractive(current))
			return AsMenuItem(current);
	return nullptr;
}

MenuItem* MenuItem::FindNavigableSibling(
	int direction, bool edge) const noexcept
{
	if (direction == 0) return nullptr;
	std::vector<MenuItem*> candidates;
	if (_parentItem)
	{
		for (auto* current : _parentItem->_items)
			if (auto* item = AsMenuItem(current);
				item && IsInteractive(item))
				candidates.push_back(item);
	}
	else if (auto* menu = dynamic_cast<Menu*>(ResolveMenuHost()))
	{
		for (int index = 0;; ++index)
		{
			auto* item = menu->GetItem(index);
			if (!item) break;
			if (IsInteractive(item)) candidates.push_back(item);
		}
	}
	else if (auto* menu =
		dynamic_cast<ContextMenu*>(ResolveMenuHost()))
	{
		for (int index = 0;; ++index)
		{
			auto* item = menu->GetItem(index);
			if (!item) break;
			if (IsInteractive(item)) candidates.push_back(item);
		}
	}
	if (candidates.empty()) return nullptr;
	if (edge) return direction < 0
		? candidates.back() : candidates.front();
	const auto found = std::find(
		candidates.begin(), candidates.end(), this);
	if (found == candidates.end()) return direction < 0
		? candidates.back() : candidates.front();
	const auto index = static_cast<int>(found - candidates.begin());
	const auto next = (index + direction
		+ static_cast<int>(candidates.size()))
		% static_cast<int>(candidates.size());
	return candidates[static_cast<size_t>(next)];
}

MenuItem* MenuItem::RootTopLevelItem() noexcept
{
	auto* result = this;
	while (result->_parentItem) result = result->_parentItem;
	return result;
}

bool MenuItem::FocusForMenuNavigation()
{
	if (!IsInteractive(this)) return false;
	SetIsHighlightedCore(true);
	return Focus();
}

bool MenuItem::FocusSiblingFromKeyboard(
	int direction, bool edge)
{
	auto* sibling = FindNavigableSibling(direction, edge);
	if (!sibling) return false;
	if (!_parentItem && _isSubmenuOpen)
	{
		SetIsSubmenuOpenCore(false);
		if (!sibling->_items.empty())
			sibling->SetIsSubmenuOpenCore(true);
	}
	SetIsHighlightedCore(false);
	return sibling->FocusForMenuNavigation();
}

bool MenuItem::OpenSubmenuFromKeyboard(bool selectFirst)
{
	if (_items.empty()) return false;
	SetIsSubmenuOpenCore(true);
	if (!selectFirst) return true;
	if (auto* child = FindNavigableChild(false))
		return child->FocusForMenuNavigation();
	return true;
}

bool MenuItem::CloseKeyboardLevel()
{
	if (_parentItem)
	{
		auto* parent = _parentItem;
		parent->SetIsSubmenuOpenCore(false);
		SetIsHighlightedCore(false);
		return parent->FocusForMenuNavigation();
	}
	if (auto* menu = dynamic_cast<Menu*>(ResolveMenuHost()))
	{
		menu->ClosePopup();
		return true;
	}
	if (auto* menu = dynamic_cast<ContextMenu*>(ResolveMenuHost()))
	{
		menu->Hide();
		return true;
	}
	return false;
}

bool MenuItem::InvokeLeafAndDismiss()
{
	const ControlWeakReference sourceLifetime(this);
	const bool staysOpen = _staysOpenOnClick;
	const bool invoked = Invoke();
	auto* source = dynamic_cast<MenuItem*>(
		sourceLifetime.Get());
	if (!source) return invoked;
	if (!invoked || staysOpen) return invoked;
	if (auto* menu = dynamic_cast<Menu*>(
		source->ResolveMenuHost()))
		menu->ClosePopup();
	else if (auto* menu =
		dynamic_cast<ContextMenu*>(
			source->ResolveMenuHost()))
		menu->Hide();
	return true;
}

std::unique_ptr<Control> MenuItem::WrapGeneratedItem(
	std::unique_ptr<Control> visual,
	const BindingSourceReference& item,
	size_t)
{
	auto container = std::make_unique<MenuItem>();
	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	if (visual) container->SetVisualHeader(std::move(visual));
	else container->SetHeader(BindingValue(GetDisplayMemberText(item)));
	return container;
}

MenuItem* MenuItem::AddSubItem(std::unique_ptr<MenuItem> item)
{
	return InsertSubItem(static_cast<int>(AuthoredItemCount()), std::move(item));
}

MenuItem* MenuItem::InsertSubItem(
	int index, std::unique_ptr<MenuItem> item)
{
	if (index < 0 || index > static_cast<int>(AuthoredItemCount()) || !item)
		return nullptr;
	return static_cast<MenuItem*>(InsertItemControl(
		static_cast<size_t>(index), std::move(item)));
}

Separator* MenuItem::AddSeparator()
{
	return static_cast<Separator*>(AddItemControl(
		std::make_unique<Separator>()));
}

std::unique_ptr<Control> MenuItem::DetachSubItemAt(int index)
{
	if (index < 0 || index >= static_cast<int>(AuthoredItemCount()))
		return {};
	return DetachItemControlAt(static_cast<size_t>(index));
}

std::unique_ptr<MenuItem> MenuItem::DetachSubItem(MenuItem* item)
{
	const auto index = IndexOfSubItem(item);
	if (index < 0) return {};
	auto detached = DetachItemControlAt(static_cast<size_t>(index));
	return std::unique_ptr<MenuItem>(
		static_cast<MenuItem*>(detached.release()));
}

bool MenuItem::RemoveSubItemAt(int index)
{
	auto item = DetachSubItemAt(index);
	return item != nullptr;
}

bool MenuItem::RemoveSubItem(MenuItem* item)
{
	return RemoveSubItemAt(IndexOfSubItem(item));
}

void MenuItem::ClearSubItems()
{
	ClearItemControls();
}

MenuItem* MenuItem::GetSubItem(int index) const noexcept
{
	if (index < 0 || static_cast<size_t>(index) >= _items.size())
		return nullptr;
	return AsMenuItem(_items[static_cast<size_t>(index)]);
}

int MenuItem::IndexOfSubItem(const MenuItem* item) const noexcept
{
	if (!item) return -1;
	auto found = std::find(_items.begin(), _items.end(), item);
	return found == _items.end()
		? -1 : static_cast<int>(found - _items.begin());
}

bool MenuItem::Invoke()
{
	const ControlWeakReference sourceLifetime(this);
	const auto snapshot = GetAccessibilitySnapshot();
	if (!_items.empty()
		|| !snapshot.Enabled || !snapshot.Visible)
		return false;
	RoutedEventArgs args;
	Click(this, args);
	auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (!source) return true;
	if (source->_isCheckable)
	{
		(void)source->SetCurrentPropertyField(
			IsCheckedProperty(), source->_isChecked, !source->_isChecked);
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		if (!source) return true;
	}
	if (source->_command.empty()) return true;
	return RoutedCommandManager::ExecuteCommandSource(
		*source,
		RoutedCommandSourceQuery{
			RoutedCommand(source->_command), source->_commandParameter,
			source->EffectiveCommandTarget() }).Executed;
}

bool MenuItem::OnAccessKey(bool)
{
	if (!FocusForMenuNavigation()) return false;
	return _items.empty()
		? InvokeLeafAndDismiss()
		: OpenSubmenuFromKeyboard(true);
}

bool MenuItem::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Left:
	case Key::Right:
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::Return:
	case Key::Space:
	case Key::Escape:
		return true;
	default:
		return HeaderedItemsControl::HandlesNavigationKey(key);
	}
}

bool MenuItem::ProcessInput(const InputReport& input)
{
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	if (input.Kind == InputReportKind::PointerMove)
	{
		SetIsHighlightedCore(true);
		if (ShouldOpenSubmenuOnPointerMove())
			SetIsSubmenuOpenCore(true);
	}
	else if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		_pointerPressActive = true;
		SetIsPressedCore(true);
		(void)CaptureMouse();
		(void)FocusForMenuNavigation();
		return true;
	}
	else if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left)
	{
		const bool activate = _pointerPressActive
			&& ContainsPoint(input.X, input.Y);
		_pointerPressActive = false;
		SetIsPressedCore(false);
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		if (activate)
		{
			if (_items.empty()) (void)InvokeLeafAndDismiss();
			else SetIsSubmenuOpenCore(!_isSubmenuOpen);
		}
		return true;
	}
	else if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
	{
		_pointerPressActive = false;
		SetIsPressedCore(false);
		if (input.Kind == InputReportKind::Cancel
			&& IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return true;
	}
	else if (input.Kind == InputReportKind::KeyDown)
	{
		bool handled = true;
		switch (input.Key)
		{
		case Key::Down:
			if (_role == MenuItemRole::TopLevelHeader)
				handled = OpenSubmenuFromKeyboard(true);
			else
				handled = FocusSiblingFromKeyboard(1);
			break;
		case Key::Up:
			if (_role == MenuItemRole::TopLevelHeader)
			{
				handled = OpenSubmenuFromKeyboard(false);
				if (handled)
					if (auto* child = FindNavigableChild(true))
						handled = child->FocusForMenuNavigation();
			}
			else handled = FocusSiblingFromKeyboard(-1);
			break;
		case Key::Right:
			if (_role == MenuItemRole::SubmenuHeader)
				handled = OpenSubmenuFromKeyboard(true);
			else if (!_parentItem)
				handled = FocusSiblingFromKeyboard(1);
			else
			{
				auto* root = RootTopLevelItem();
				handled = root
					? root->FocusSiblingFromKeyboard(1) : false;
			}
			break;
		case Key::Left:
			if (_parentItem)
				handled = CloseKeyboardLevel();
			else handled = FocusSiblingFromKeyboard(-1);
			break;
		case Key::Home:
			handled = FocusSiblingFromKeyboard(1, true);
			break;
		case Key::End:
			handled = FocusSiblingFromKeyboard(-1, true);
			break;
		case Key::Return:
		case Key::Space:
			handled = _items.empty()
				? InvokeLeafAndDismiss()
				: OpenSubmenuFromKeyboard(true);
			break;
		case Key::Escape:
			handled = CloseKeyboardLevel();
			break;
		default:
			handled = false;
			break;
		}
		if (handled) return true;
	}
	return HeaderedItemsControl::ProcessInput(input);
}

UIClass Menu::Type() { return UIClass::UI_Menu; }

void Menu::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
}

Menu::Menu()
	: ItemsControl()
{
	RegisterDependencyProperties();
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->RendererBorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	auto panel = std::make_shared<ItemsPanelTemplate>();
	panel->Kind = ItemsPanelKind::Stack;
	panel->Orientation = Orientation::Horizontal;
	(void)TrySetPropertyValue(
		ItemsControl::ItemsPanelProperty(),
		BindingValue(ItemsPanelTemplateReference(std::move(panel))),
		DependencyPropertyValueSource::Theme);
}

Menu::~Menu()
{
	for (auto* entry : _items)
	{
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		item->SetStructureChangedHandler({});
		item->SetInteractionStateChangedHandler({});
		item->DetachCommandHost(*this);
	}
	_items.clear();
}

void Menu::OnItemTreeChanged()
{
	const ControlWeakReference hostLifetime(this);
	ClosePopup();
	auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
	if (!host) return;
	host->RequestLayout();
	host = dynamic_cast<Menu*>(hostLifetime.Get());
	if (!host) return;
	host->NotifyAccessibilityStructureChanged();
	host = dynamic_cast<Menu*>(hostLifetime.Get());
	if (host) host->InvalidateVisual();
}

void Menu::OnItemInteractionStateChanged(MenuItem& source)
{
	std::vector<int> path;
	if (!FindMenuItemPath(_items, &source, path) || path.empty()) return;
	if (!IsInteractive(&source))
	{
		source.SetIsSubmenuOpenCore(false);
		return;
	}

	if (source.IsSubmenuOpen && !source.GetMenuItemsView().empty())
	{
		_expand = true;
		_expandIndex = path.front();
		_hoverTopIndex = path.front();
		for (int index = 0;
			index < static_cast<int>(_items.size()); ++index)
		{
			auto* item = GetItem(index);
			if (!item || item == source.RootTopLevelItem()) continue;
			item->SetIsSubmenuOpenCore(false);
			item->SetIsHighlightedCore(false);
		}
	}
	else
	{
		_expand = false;
		_expandIndex = -1;
		for (int index = 0;
			index < static_cast<int>(_items.size()); ++index)
		{
			auto* item = GetItem(index);
			if (item && item->IsSubmenuOpen)
			{
				_expand = true;
				_expandIndex = index;
				break;
			}
		}
	}
	InvalidateVisual();
}

void Menu::AttachItemTree(MenuItem* item)
{
	if (!item) return;
	item->_parentItem = nullptr;
	item->SetStructureChangedHandler(
		[this]() { OnItemTreeChanged(); });
	item->SetInteractionStateChangedHandler(
		[this](MenuItem& source) { OnItemInteractionStateChanged(source); });
	item->AttachCommandHost(*this, nullptr);
}

void Menu::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	Control::OnPresentationWindowChanged(previousWindow, currentWindow);
	if (previousWindow != currentWindow && _expand)
		ClosePopup();
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry))
			item->SynchronizeCommandContext(
				currentWindow, nullptr, true);
}

bool Menu::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const MenuItem*>(&item)
		|| dynamic_cast<const Separator*>(&item)) return true;
	error = "Menu Items can contain MenuItem or Separator controls only";
	return false;
}

void Menu::OnBeforeGeneratedItemsRebuilt()
{
	for (auto* entry : _items)
	{
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		item->SetStructureChangedHandler({});
		item->SetInteractionStateChangedHandler({});
		item->DetachCommandHost(*this);
	}
	_items.clear();
}

void Menu::SynchronizeItems()
{
	std::vector<Control*> current;
	current.reserve(ItemCount());
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		auto* item = GetGeneratedItem(index);
		if (item) current.push_back(item);
	}
	for (auto* entry : _items)
	{
		if (std::find(current.begin(), current.end(), entry)
			!= current.end()) continue;
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		item->SetStructureChangedHandler({});
		item->SetInteractionStateChangedHandler({});
		item->DetachCommandHost(*this);
	}
	_items = std::move(current);
	for (auto* entry : _items)
	{
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		AttachItemTree(item);
	}
	OnItemTreeChanged();
}

void Menu::OnAuthoredItemsChanged() noexcept
{
	try { SynchronizeItems(); }
	catch (...) { _items.clear(); }
}

void Menu::OnGeneratedItemsRebuilt()
{
	SynchronizeItems();
}

std::unique_ptr<Control> Menu::WrapGeneratedItem(
	std::unique_ptr<Control> visual,
	const BindingSourceReference& item,
	size_t)
{
	auto container = std::make_unique<MenuItem>();
	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	if (visual) container->SetVisualHeader(std::move(visual));
	else container->SetHeader(BindingValue(GetDisplayMemberText(item)));
	return container;
}

MenuItem* Menu::AddItem(std::unique_ptr<MenuItem> item)
{
	return InsertItem(static_cast<int>(AuthoredItemCount()), std::move(item));
}

MenuItem* Menu::InsertItem(
	int index, std::unique_ptr<MenuItem> item)
{
	if (index < 0 || index > static_cast<int>(AuthoredItemCount()) || !item)
		return nullptr;
	ClosePopup();
	return static_cast<MenuItem*>(InsertItemControl(
		static_cast<size_t>(index), std::move(item)));
}

Separator* Menu::AddSeparator()
{
	return static_cast<Separator*>(AddItemControl(
		std::make_unique<Separator>()));
}

MenuItem* Menu::GetItem(int index) const noexcept
{
	if (index < 0 || static_cast<size_t>(index) >= _items.size())
		return nullptr;
	return AsMenuItem(_items[static_cast<size_t>(index)]);
}

int Menu::IndexOfItem(const MenuItem* item) const noexcept
{
	if (!item) return -1;
	auto found = std::find(_items.begin(), _items.end(), item);
	return found == _items.end()
		? -1 : static_cast<int>(found - _items.begin());
}

std::unique_ptr<Control> Menu::DetachItemAt(int index)
{
	if (index < 0 || index >= static_cast<int>(AuthoredItemCount()))
		return {};
	ClosePopup();
	return DetachItemControlAt(static_cast<size_t>(index));
}

std::unique_ptr<MenuItem> Menu::DetachItem(MenuItem* item)
{
	const auto index = IndexOfItem(item);
	if (index < 0) return {};
	ClosePopup();
	auto detached = DetachItemControlAt(static_cast<size_t>(index));
	return std::unique_ptr<MenuItem>(
		static_cast<MenuItem*>(detached.release()));
}

bool Menu::RemoveItemAt(int index)
{
	auto item = DetachItemAt(index);
	return item != nullptr;
}

bool Menu::RemoveItem(MenuItem* item)
{
	return RemoveItemAt(IndexOfItem(item));
}

void Menu::ClearItems()
{
	if (AuthoredItemCount() == 0) return;
	ClosePopup();
	ClearItemControls();
}


bool Menu::ContainsPoint(int localX, int localY)
{
	return ItemsControl::ContainsPoint(localX, localY);
}

void Menu::ClosePopup()
{
	const bool wasExpanded = _expand;
	_expand = false;
	_expandIndex = -1;
	_hoverTopIndex = -1;
	_hoverPath.clear();
	_openPath.clear();
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry))
		{
			item->SetIsSubmenuOpenCore(false);
			item->SetIsHighlightedCore(false);
		}
	if (wasExpanded) InvalidateVisual();
}

void Menu::SynchronizeInteractionProjection()
{
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) item->UpdateRole();
}


cui::core::Size Menu::GetRenderSizeDip()
{
	return ItemsControl::GetRenderSizeDip();
}

void Menu::PreparePresentation()
{
	ItemsControl::PreparePresentation();
	SynchronizeInteractionProjection();
}

bool Menu::ProcessInput(const InputReport& input)
{
	const ControlWeakReference hostLifetime(this);
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	auto hitTopLevelItem = [this](int localX, int localY) noexcept
	{
		for (auto* entry : _items)
		{
			auto* item = AsMenuItem(entry);
			if (!IsInteractive(item)) continue;
			const auto location = item->GetActualLocationDip();
			const auto size = item->GetActualSizeDip();
			if (cui::core::Rect{ location, size }.Contains(
				cui::core::Point{
					static_cast<float>(localX),
					static_cast<float>(localY) }))
				return item;
		}
		return static_cast<MenuItem*>(nullptr);
	};

	if (input.Kind == InputReportKind::PointerMove)
	{
		if (auto* item = hitTopLevelItem(input.X, input.Y))
		{
			const ControlWeakReference itemLifetime(item);
			item->SetIsHighlightedCore(true);
			item = dynamic_cast<MenuItem*>(itemLifetime.Get());
			if (item && item->ShouldOpenSubmenuOnPointerMove())
				item->SetIsSubmenuOpenCore(true);
		}
		return true;
	}
	if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left)
	{
		if (auto* item = hitTopLevelItem(input.X, input.Y))
		{
			if (!item->GetMenuItemsView().empty())
				item->SetIsSubmenuOpenCore(!item->IsSubmenuOpen);
			else
				(void)item->InvokeLeafAndDismiss();
		}
		// Item state and command callbacks may synchronously destroy the menu.
		// The pointer transaction is already consumed, so never fall through to
		// the base dispatcher or touch the old host address afterward.
		return true;
	}
	if (input.Kind == InputReportKind::KeyDown
		&& input.Key == Key::Escape && _expand)
	{
		ClosePopup();
		return true;
	}
	auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
	return host ? host->ItemsControl::ProcessInput(input) : true;
}

