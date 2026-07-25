#include "Menu.h"
#include "DependencyPropertyInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "Window.h"
#include "WindowInfrastructure.h"
#include "XamlInfrastructure.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_set>

namespace
{
	// Private fallback-presenter metrics. They are deliberately not dependency
	// properties: a ControlTemplate/ItemsPanel owns authored menu chrome.
	constexpr float MenuItemHorizontalPadding = 10.0f;
	constexpr float PopupVerticalPadding = 6.0f;
	constexpr float PopupItemExtent = 26.0f;
	constexpr float MenuItemCornerRadius = 6.0f;
	constexpr float PopupCornerRadius = 8.0f;
	constexpr float PopupItemCornerRadius = 6.0f;
	constexpr float PopupItemHorizontalInset = 6.0f;
	constexpr D2D1_COLOR_F MenuBarBackground = cui::theme::palette::Surface;
	constexpr D2D1_COLOR_F MenuBarBorder = cui::theme::palette::Border;
	constexpr D2D1_COLOR_F MenuItemHighlight = cui::theme::palette::AccentSoft;
	constexpr D2D1_COLOR_F MenuItemActive = cui::theme::palette::AccentSelected;
	constexpr D2D1_COLOR_F PopupBackground = cui::theme::palette::Surface;
	constexpr D2D1_COLOR_F PopupBorder = cui::theme::palette::Border;
	constexpr D2D1_COLOR_F PopupHighlight = cui::theme::palette::AccentSelected;
	constexpr D2D1_COLOR_F PopupText = cui::theme::palette::TextPrimary;
	constexpr D2D1_COLOR_F PopupSeparator = cui::theme::palette::Border;

	struct ScopeExit
	{
		std::function<void()> Action;
		~ScopeExit() { if (Action) Action(); }
	};

	struct MenuPanel
	{
		MenuItem* Owner = nullptr;
		std::span<Control* const> Items;
		float X = 0;
		float Y = 0;
		float W = 0;
		float H = 0;
		bool OpenedToLeft = false;               // 相对上一层面板是否向左展开
	};

	D2D1_COLOR_F BoostAlpha(D2D1_COLOR_F color, float factor)
	{
		color.a = (std::clamp)(color.a * factor, 0.0f, 1.0f);
		return color;
	}

	bool IsSeparator(const Control* item)
	{
		return dynamic_cast<const Separator*>(item) != nullptr;
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

void MenuItem::RegisterDependencyProperties()
{
	HeaderedItemsControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto options = DependencyPropertyOptions<MenuItem, std::wstring>{
			std::wstring{}, DependencyPropertyFlags::None };
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		options.Changed = [](
			MenuItem& target, const std::wstring&, const std::wstring&)
		{
			target.RefreshCommandSource();
		};
		DependencyPropertyRegistry::Register<MenuItem, std::wstring>(
			L"Command",
			[](MenuItem& target) { return target.Command; },
			[](MenuItem& target, const std::wstring& value)
			{ target.Command = value; },
			{}, options);
		options.Design.Order = 20;
		DependencyPropertyRegistry::Register<MenuItem, std::wstring>(
			L"CommandParameter",
			[](MenuItem& target) { return target.CommandParameter; },
			[](MenuItem& target, const std::wstring& value)
			{ target.CommandParameter = value; },
			{}, options);
		DependencyPropertyOptions<MenuItem, std::wstring> gestureOptions{
			std::wstring{}, DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender };
		gestureOptions.Design.Category = L"Behavior";
		gestureOptions.Design.CategoryOrder = 300;
		gestureOptions.Design.Order = 25;
		gestureOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		gestureOptions.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		gestureOptions.Changed = [](
			MenuItem& target, const std::wstring&, const std::wstring&)
		{
			if (target._structureChanged) target._structureChanged();
		};
		DependencyPropertyRegistry::Register<MenuItem, std::wstring>(
			L"InputGestureText",
			[](MenuItem& target) { return target.InputGestureText; },
			[](MenuItem& target, const std::wstring& value)
			{ target.InputGestureText = value; },
			{}, std::move(gestureOptions));

		auto booleanOptions = [](bool defaultValue, int order,
			DependencyPropertyFlags flags = DependencyPropertyFlags::None)
		{
			DependencyPropertyOptions<MenuItem, bool> result;
			result.DefaultValue = defaultValue;
			result.Flags = flags;
			result.Design.Category = L"Behavior";
			result.Design.CategoryOrder = 300;
			result.Design.Order = order;
			result.Design.Editor = DependencyPropertyEditorKind::Boolean;
			result.Design.Persistence = DependencyPropertyPersistence::Metadata;
			return result;
		};
		auto checkableOptions = booleanOptions(false, 30);
		DependencyPropertyRegistry::Register<MenuItem, bool>(L"IsCheckable",
			[](MenuItem& target) { return target.IsCheckable; },
			[](MenuItem& target, const bool& value)
			{ target.IsCheckable = value; },
			{}, std::move(checkableOptions));

		auto checkedOptions = booleanOptions(
			false, 35, DependencyPropertyFlags::AffectsRender);
		checkedOptions.Changed = [](
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
		DependencyPropertyRegistry::Register<MenuItem, bool>(L"IsChecked",
			[](MenuItem& target) { return target.IsChecked; },
			[](MenuItem& target, const bool& value)
			{ target.IsChecked = value; },
			{}, std::move(checkedOptions));

		auto staysOpenOptions = booleanOptions(false, 40);
		DependencyPropertyRegistry::Register<MenuItem, bool>(L"StaysOpenOnClick",
			[](MenuItem& target) { return target.StaysOpenOnClick; },
			[](MenuItem& target, const bool& value)
			{ target.StaysOpenOnClick = value; },
			{}, std::move(staysOpenOptions));

		auto highlightedOptions = booleanOptions(
			false, 45, DependencyPropertyFlags::AffectsRender);
		highlightedOptions.IsReadOnly = true;
		highlightedOptions.Flags = DependencyPropertyFlags::AffectsRender;
		highlightedOptions.Design.Category = L"State";
		highlightedOptions.Design.CategoryOrder = 70;
		highlightedOptions.Design.Persistence =
			DependencyPropertyPersistence::Transient;
		highlightedOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<MenuItem, bool>(L"IsHighlighted",
			[](MenuItem& target) { return target.IsHighlighted; },
			[](MenuItem& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsHighlighted", target._isHighlighted, value);
			},
			{}, std::move(highlightedOptions));

		auto submenuOptions = booleanOptions(
			false, 50, DependencyPropertyFlags::AffectsRender);
		submenuOptions.Changed = [](
			MenuItem& target, const bool&, const bool& value)
		{
			const ControlWeakReference lifetime(&target);
			RoutedEventArgs args;
			if (value) target.SubmenuOpened(&target, args);
			else target.SubmenuClosed(&target, args);
			auto* source = dynamic_cast<MenuItem*>(lifetime.Get());
			if (source && !source->_projectingInteractionState
				&& source->_interactionStateChanged)
				source->_interactionStateChanged(*source);
		};
		DependencyPropertyRegistry::Register<MenuItem, bool>(L"IsSubmenuOpen",
			[](MenuItem& target) { return target.IsSubmenuOpen; },
			[](MenuItem& target, const bool& value)
			{ target.IsSubmenuOpen = value; },
			{}, std::move(submenuOptions));
		auto targetOptions =
			DependencyPropertyOptions<MenuItem, ControlWeakReference>{
			ControlWeakReference{},
			DependencyPropertyFlags::None };
		targetOptions.Design.Category = L"Behavior";
		targetOptions.Design.CategoryOrder = 300;
		targetOptions.Design.Order = 60;
		targetOptions.Design.Editor = DependencyPropertyEditorKind::Auto;
		targetOptions.Design.Persistence =
			DependencyPropertyPersistence::Native;
		DependencyPropertyRegistry::Register<
			MenuItem, ControlWeakReference>(
			L"CommandTarget",
			[](MenuItem& target) { return target._commandTarget; },
			[](MenuItem& target, const ControlWeakReference& value)
			{ target.ApplyCommandTarget(value); },
			{}, std::move(targetOptions));
		return true;
	}();
	(void)registered;
}

GET_CPP(MenuItem, std::wstring, Command) { return _command; }
SET_CPP(MenuItem, std::wstring, Command)
{
	SetPropertyField(L"Command", _command, value);
}
GET_CPP(MenuItem, std::wstring, CommandParameter)
{
	return _commandParameter;
}
SET_CPP(MenuItem, std::wstring, CommandParameter)
{
	SetPropertyField(L"CommandParameter", _commandParameter, value);
}
GET_CPP(MenuItem, std::wstring, InputGestureText)
{
	return _inputGestureText;
}
SET_CPP(MenuItem, std::wstring, InputGestureText)
{
	(void)SetPropertyField(
		L"InputGestureText", _inputGestureText, std::move(value));
}
GET_CPP(MenuItem, bool, IsCheckable) { return _isCheckable; }
SET_CPP(MenuItem, bool, IsCheckable)
{
	(void)SetPropertyField(L"IsCheckable", _isCheckable, value);
}
GET_CPP(MenuItem, bool, IsChecked) { return _isChecked; }
SET_CPP(MenuItem, bool, IsChecked)
{
	(void)SetPropertyField(L"IsChecked", _isChecked, value);
}
GET_CPP(MenuItem, bool, StaysOpenOnClick) { return _staysOpenOnClick; }
SET_CPP(MenuItem, bool, StaysOpenOnClick)
{
	(void)SetPropertyField(L"StaysOpenOnClick", _staysOpenOnClick, value);
}
GET_CPP(MenuItem, bool, IsHighlighted) { return _isHighlighted; }
GET_CPP(MenuItem, bool, IsSubmenuOpen) { return _isSubmenuOpen; }
SET_CPP(MenuItem, bool, IsSubmenuOpen)
{
	(void)SetPropertyField(L"IsSubmenuOpen", _isSubmenuOpen, value);
}

void MenuItem::SetIsHighlightedCore(bool value)
{
	if (_isHighlighted == value) return;
	if (!SetReadOnlyPropertyField(
		L"IsHighlighted", _isHighlighted, value)) return;
	SetStyleState(ControlStyleState::Hovered, value);
}

void MenuItem::SetIsSubmenuOpenCore(bool value)
{
	if (_isSubmenuOpen == value) return;
	_projectingInteractionState = true;
	(void)SetCurrentPropertyField(L"IsSubmenuOpen", _isSubmenuOpen, value);
	_projectingInteractionState = false;
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
			L"CommandTarget",
			BindingValue(ControlWeakReference(value)),
			DependencyPropertyValueSource::Local);
		return;
	}
	if (ClearPropertyValue(
		L"CommandTarget", DependencyPropertyValueSource::Local))
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
	RetainEventConnection(OnPropertyValueChanged.Subscribe(
		[this](DependencyObject*, const DependencyPropertyChangedEventArgs& args)
		{
			if ((args.PropertyName == L"IsEnabled"
				|| args.PropertyName == L"Visibility"
				|| args.PropertyName == L"IsVisible")
				&& _interactionStateChanged)
				_interactionStateChanged(*this);
		}));
	this->_backcolor = D2D1_COLOR_F{ 0,0,0,0 };
	this->_bordercolor = D2D1_COLOR_F{ 0,0,0,0 };
	this->_forecolor = cui::theme::palette::TextPrimary;
	(void)TrySetPropertyValue(
		L"Cursor", BindingValue(CursorKind::Hand),
		DependencyPropertyValueSource::Theme);
	SuppressItemsPresentation();
}

MenuItem::~MenuItem()
{
	_structureChanged = {};
	_interactionStateChanged = {};
	_commandCanExecuteConnection.Disconnect();
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
	Window* window, ControlWeakReference defaultCommandTarget)
{
	const ControlWeakReference sourceLifetime(this);
	const ControlWeakReference windowLifetime(window);
	const bool hostChanged = GetPresentationWindow() != window;
	_defaultCommandTarget = defaultCommandTarget;
	Control::PropagatePresentationWindow(this, window);
	auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
	if (!source || hostChanged) return;
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
			cui::framework::XamlAccess::SetLogicalParent(*child, source);
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		child = dynamic_cast<MenuItem*>(childLifetime.Get());
		if (!source || !child || source->IndexOfSubItem(child) < 0)
			continue;
		auto* currentWindow = window
			? dynamic_cast<Window*>(windowLifetime.Get()) : nullptr;
		if (window && !currentWindow) return;
		child->SynchronizeCommandContext(
			currentWindow, defaultCommandTarget);
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
			cui::framework::XamlAccess::SetLogicalParent(*child, source);
		source = dynamic_cast<MenuItem*>(sourceLifetime.Get());
		child = dynamic_cast<MenuItem*>(childLifetime.Get());
		if (!source || !child || source->IndexOfSubItem(child) < 0)
			continue;
		auto* liveWindow = currentWindow
			? dynamic_cast<Window*>(windowLifetime.Get()) : nullptr;
		if (currentWindow && !liveWindow) return;
		child->SynchronizeCommandContext(
			liveWindow, source->_defaultCommandTarget);
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
	source->SynchronizeCommandContext(window, defaultCommandTarget);
}

void MenuItem::DetachCommandHost(Control& routedOwner)
{
	const ControlWeakReference sourceLifetime(this);
	if (GetLogicalParent() == &routedOwner)
		SetLogicalParent(nullptr);
	if (auto* source = dynamic_cast<MenuItem*>(sourceLifetime.Get()))
		source->SynchronizeCommandContext(nullptr, nullptr);
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
	SuppressItemsPresentation();
	if (_structureChanged) _structureChanged();
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

void MenuItem::SuppressItemsPresentation()
{
	// Submenu rows are projected by the owning Menu/ContextMenu popup surface.
	// Their ItemsHost remains the WPF logical/generator owner, but must not also
	// enter the flattened retained scene at its unarranged content location.
	if (auto* host = GetItemsHost())
		cui::framework::TemplateAccess::SetParticipatesInPresentationScene(
			*host, false);
}

void MenuItem::OnControlTemplatePresentationChanged()
{
	HeaderedItemsControl::OnControlTemplatePresentationChanged();
	SuppressItemsPresentation();
}

void MenuItem::ConfigureHeaderVisual(Control& child)
{
	HeaderedItemsControl::ConfigureHeaderVisual(child);
	// The fallback MenuItem renderer owns its compact header chrome. The
	// Header slot remains materialized for WPF logical/template semantics and
	// measurement, but must not paint a second copy of the text.
	cui::framework::TemplateAccess::SetParticipatesInPresentationScene(
		child, false);
}

void MenuItem::ReleaseHeaderVisual(Control& child)
{
	cui::framework::TemplateAccess::SetParticipatesInPresentationScene(
		child, true);
	HeaderedItemsControl::ReleaseHeaderVisual(child);
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
	else container->SetHeader(BindingValue(GetBindingRecordText(
		item, GetDisplayMemberPath())));
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
			L"IsChecked", source->_isChecked, !source->_isChecked);
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

void MenuItem::OnRender()
{
	if (!this->IsVisible) return;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	{
		const bool enabled = IsEffectivelyEnabled();
		const bool hover = enabled && IsHighlighted;
		const bool active = IsChecked || IsSubmenuOpen || HasControlStyleState(
			GetStyleState(), ControlStyleState::Checked);
		if (active || hover)
		{
			const float insetX = 3.0f;
			const float insetY = 3.0f;
			auto itemRect = D2D1::RectF(insetX, insetY,
				(std::max)(insetX, actualWidth - insetX),
				(std::max)(insetY, actualHeight - insetY));
			const auto stateColor = active ? MenuItemActive : MenuItemHighlight;
			d2d->FillRoundRect(itemRect, stateColor, MenuItemCornerRadius);
			d2d->DrawRoundRect(itemRect, BoostAlpha(stateColor, 1.9f),
				1.0f, MenuItemCornerRadius);
			const float stripeH = (std::max)(0.0f, itemRect.bottom - itemRect.top - 8.0f);
			if (stripeH > 0.0f)
				d2d->FillRoundRect(itemRect.left + 3.0f, itemRect.top + 4.0f, 3.0f, stripeH, BoostAlpha(stateColor, 3.0f), 1.5f);
		}

		auto font = this->GetRenderFont();
		const auto displayText = GetDisplayText();
		auto ts = font->GetTextSize(displayText);
		float tx = 10.0f;
		float ty = (actualHeight - ts.height) * 0.5f;
		if (ty < 0) ty = 0;
		auto textColor = this->RendererForegroundColor;
		if (!enabled) textColor.a *= 0.45f;
		d2d->DrawString(displayText, tx, ty, textColor, font);
	}
	this->EndRender();
}

bool MenuItem::ProcessInput(const InputReport& input)
{
	if (!this->IsEffectivelyEnabled() || !this->IsVisible) return true;
	return Control::ProcessInput(input);
}

UIClass Menu::Type() { return UIClass::UI_Menu; }

Menu::Menu()
	: ItemsControl()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->RendererBorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	auto panel = std::make_shared<ItemsPanelTemplate>();
	panel->Kind = ItemsPanelKind::Stack;
	panel->Orientation = Orientation::Horizontal;
	(void)TrySetPropertyValue(
		L"ItemsPanel",
		BindingValue(ItemsPanelTemplateReference(std::move(panel))),
		DependencyPropertyValueSource::Theme);
}

Menu::~Menu()
{
	if (GetPresentationWindow())
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*GetPresentationWindow(), this);
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
		if (_expandIndex == path.front()) ClosePopup();
		else
		{
			_hoverPath.clear();
			_openPath.clear();
			SynchronizeInteractionProjection();
			InvalidateVisual();
		}
		return;
	}

	if (source.IsSubmenuOpen && !source.GetMenuItemsView().empty())
	{
		_expand = true;
		_expandIndex = path.front();
		_hoverTopIndex = path.front();
		_openPath.assign(path.begin() + 1, path.end());
		_hoverPath = _openPath;
		if (GetPresentationWindow())
		{
			(void)cui::framework::WindowAccess::OpenTransientPresentation(
				*GetPresentationWindow(), this,
				TransientPresentationOptions{},
				[](Control& root)
				{ static_cast<Menu&>(root).ClosePopup(); });
		}
	}
	else if (_expandIndex == path.front())
	{
		if (path.size() == 1)
		{
			ClosePopup();
			return;
		}
		const auto openDepth = path.size() - 1;
		if (_openPath.size() >= openDepth
			&& std::equal(path.begin() + 1, path.end(), _openPath.begin()))
		{
			_openPath.resize(openDepth - 1);
			if (_hoverPath.size() > openDepth - 1)
				_hoverPath.resize(openDepth - 1);
		}
	}
	SynchronizeInteractionProjection();
	this->InvalidateVisual();
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
	if (previousWindow)
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*previousWindow, this);
	if (previousWindow != currentWindow && _expand)
		ClosePopup();
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
	else container->SetHeader(BindingValue(GetBindingRecordText(
		item, GetDisplayMemberPath())));
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
	if (localX >= 0 && localX <= GetActualSizeDip().width
		&& localY >= 0 && localY < MenuBarExtent())
		return true;

	if (!_expand || DropCount() <= 0)
		return false;
	if (_expandIndex < 0 || _expandIndex >= static_cast<int>(_items.size()))
		return false;

	auto* top = GetItem(_expandIndex);
	if (!top)
		return false;

	auto calcPanelWidth = [&](std::span<Control* const> items) -> float
		{
			float w = 120.0f;
			auto font = this->GetRenderFont();
			for (auto* entry : items)
			{
				auto* it = AsMenuItem(entry);
				if (!it) continue;
				auto ts = font->GetTextSize(it->GetDisplayText());
				float tw = ts.width + 24.0f;
				if (!it->InputGestureText.empty())
				{
					auto ss = font->GetTextSize(it->InputGestureText);
					tw += ss.width + 20.0f;
				}
				if (!it->GetMenuItemsView().empty())
					tw += 18.0f;
				if (tw > w) w = tw;
			}
			if (w < 80.0f) w = 80.0f;
			return w;
		};

	auto clampPanelXY = [&](float& x, float& y, float w, float h)
		{
			if (!this->GetPresentationWindow()) return;
			const auto viewport = this->GetPresentationWindow()->GetContentViewportSizeDip();
			float maxX = viewport.width;
			float maxY = viewport.height;
			if (x < 0.0f) x = 0.0f;
			if (y < 0.0f) y = 0.0f;
			if (x + w > maxX) x = std::max(0.0f, maxX - w);
			if (y + h > maxY) y = std::max(0.0f, maxY - h);
		};

	std::vector<MenuPanel> panels;
	panels.reserve(8);
	MenuPanel root;
	root.Owner = top;
	root.Items = top->GetMenuItemsView();
	root.X = DropLeftLocal();
	root.Y = DropTopLocal();
	root.W = DropWidthLocal();
	root.H = PopupVerticalPadding * 2.0f
		+ static_cast<float>(root.Items.size()) * PopupItemExtent;
	clampPanelXY(root.X, root.Y, root.W, root.H);
	panels.push_back(root);

	for (size_t level = 0; level < _openPath.size(); level++)
	{
		int openIdx = _openPath[level];
		if (openIdx < 0) break;
		const auto& prev = panels.back();
		if (prev.Items.empty()) break;
		if (openIdx >= (int)prev.Items.size()) break;
		auto* owner = AsMenuItem(prev.Items[openIdx]);
		if (!IsInteractive(owner) || owner->GetMenuItemsView().empty()) break;

		MenuPanel panel;
		panel.Owner = owner;
		panel.Items = owner->GetMenuItemsView();
		panel.W = calcPanelWidth(panel.Items);
		panel.H = PopupVerticalPadding * 2.0f
			+ static_cast<float>(panel.Items.size()) * PopupItemExtent;
		panel.X = prev.X + prev.W - 1.0f;
		panel.Y = prev.Y + PopupVerticalPadding
			+ static_cast<float>(openIdx) * PopupItemExtent;

		if (this->GetPresentationWindow())
		{
			float maxX = this->GetPresentationWindow()->GetContentViewportSizeDip().width;
			if (panel.X + panel.W > maxX)
			{
				panel.X = prev.X - panel.W - 4.0f;
				panel.OpenedToLeft = true;
			}
			if (panel.X < 0.0f) panel.X = 0.0f;
		}
		clampPanelXY(panel.X, panel.Y, panel.W, panel.H);
		panels.push_back(panel);
		if (panels.size() > 32) break;
	}

	for (const auto& panel : panels)
	{
		if (localX >= panel.X && localX <= panel.X + panel.W && localY >= panel.Y && localY <= panel.Y + panel.H)
			return true;
	}

	return false;
}

void Menu::ClosePopup()
{
	const ControlWeakReference hostLifetime(this);
	auto* window = GetPresentationWindow();
	const bool wasExpanded = _expand;

	_expand = false;
	_expandIndex = -1;
	_hoverPath.clear();
	_openPath.clear();
	SynchronizeInteractionProjection();
	if (window)
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*window, this);
	if (!wasExpanded) return;
	auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
	if (!host) return;
	if (host->GetPresentationWindow() && host->GetPresentationWindow()->GetKeyboardFocusedElement())
	{
		for (Control* selected = host->GetPresentationWindow()->GetKeyboardFocusedElement(); selected;
			selected = selected->GetRoutedParent())
		{
			if (selected == host)
			{
				host->GetPresentationWindow()->SetKeyboardFocus(nullptr, false);
				host = dynamic_cast<Menu*>(hostLifetime.Get());
				if (!host) return;
				break;
			}
		}
	}
	host = dynamic_cast<Menu*>(hostLifetime.Get());
	if (!host) return;
	host->InvalidateVisual();
}

int Menu::DropCount()
{
	if (!_expand) return 0;
	if (_expandIndex < 0 || _expandIndex >= static_cast<int>(_items.size())) return 0;
	auto* top = GetItem(_expandIndex);
	if (!top) return 0;
	return static_cast<int>(top->GetMenuItemsView().size());
}

float Menu::DropLeftLocal()
{
	if (_expandIndex < 0 || _expandIndex >= static_cast<int>(_items.size())) return 0.0f;
	auto* top = GetItem(_expandIndex);
	if (!top) return 0.0f;
	return top->GetActualLocationDip().x;
}

float Menu::DropWidthLocal()
{
	if (!_expand) return 0.0f;
	if (_expandIndex < 0 || _expandIndex >= static_cast<int>(_items.size())) return 0.0f;
	auto* top = GetItem(_expandIndex);
	if (!top) return 0.0f;
	float w = 120.0f;
	auto font = this->GetRenderFont();
	for (auto* entry : top->GetMenuItemsView())
	{
		auto* it = AsMenuItem(entry);
		if (!it) continue;
		auto ts = font->GetTextSize(it->GetDisplayText());
		float tw = ts.width + 24.0f;
		if (!it->InputGestureText.empty())
		{
			auto ss = font->GetTextSize(it->InputGestureText);
			tw += ss.width + 20.0f;
		}
		if (tw > w) w = tw;
	}
	float maxw = GetActualSizeDip().width - DropLeftLocal();
	if (w > maxw) w = maxw;
	if (w < 80.0f) w = 80.0f;
	return w;
}

float Menu::DropHeightLocal()
{
	int c = DropCount();
	if (c <= 0) return 0.0f;
	return PopupVerticalPadding * 2.0f
		+ static_cast<float>(c) * PopupItemExtent;
}

bool Menu::HasSubMenu(int dropIndex)
{
	if (_expandIndex < 0 || _expandIndex >= static_cast<int>(_items.size())) return false;
	auto* top = GetItem(_expandIndex);
	if (!top) return false;
	if (dropIndex < 0
		|| dropIndex >= static_cast<int>(top->GetMenuItemsView().size()))
		return false;
	auto* item = AsMenuItem(
		top->GetMenuItemsView()[static_cast<size_t>(dropIndex)]);
	return IsInteractive(item) && !item->GetMenuItemsView().empty();
}

float Menu::MenuBarExtent() const noexcept
{
	return (std::max)(0.0f, GetActualSizeDip().height);
}

void Menu::SynchronizeInteractionProjection()
{
	std::unordered_set<MenuItem*> highlightedItems;
	std::unordered_set<MenuItem*> openedItems;
	if (_hoverTopIndex >= 0)
		if (auto* item = GetItem(_hoverTopIndex))
			highlightedItems.insert(item);
	if (_expand)
	{
		auto* current = GetItem(_expandIndex);
		if (current) openedItems.insert(current);
		for (size_t level = 0; current; ++level)
		{
			const auto items = current->GetMenuItemsView();
			if (level < _hoverPath.size())
			{
				const int highlighted = _hoverPath[level];
				if (highlighted >= 0
					&& highlighted < static_cast<int>(items.size()))
					if (auto* item = AsMenuItem(items[highlighted]))
						highlightedItems.insert(item);
			}
			if (level >= _openPath.size()) break;
			const int opened = _openPath[level];
			if (opened < 0 || opened >= static_cast<int>(items.size())) break;
			current = AsMenuItem(items[opened]);
			if (!current) break;
			openedItems.insert(current);
		}
	}
	auto apply = [&](auto&& self, MenuItem& item) -> void
	{
		item.SetIsHighlightedCore(highlightedItems.contains(&item));
		item.SetIsSubmenuOpenCore(openedItems.contains(&item));
		for (auto* child : item.GetMenuItemsView())
			if (auto* menuItem = AsMenuItem(child)) self(self, *menuItem);
	};
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) apply(apply, *item);
}

cui::core::Size Menu::GetRenderSizeDip()
{
	auto size = Control::GetRenderSizeDip();
	if (_expand)
	{
		const float menuExtent = MenuBarExtent();
		if (this->GetPresentationWindow())
		{
			float contentH = this->GetPresentationWindow()->GetContentViewportSizeDip().height;
			if (contentH < menuExtent) contentH = menuExtent;
			size.height = contentH;
		}
		else
		{
			size.height = menuExtent + DropHeightLocal();
		}
	}
	else
	{
		size.height = MenuBarExtent();
	}
	return size;
}

void Menu::PreparePresentation()
{
	Control::PreparePresentation();
	SynchronizeInteractionProjection();
	float x = 6.0f;
	auto font = GetRenderFont();
	const float menuExtent = MenuBarExtent();
	for (int index = 0; index < static_cast<int>(_items.size()); ++index)
	{
		auto* entry = _items[static_cast<size_t>(index)];
		if (auto* separator = dynamic_cast<Separator*>(entry))
		{
			separator->Arrange(cui::core::Rect{
				x, 4.0f, 8.0f,
				(std::max)(0.0f, menuExtent - 8.0f) });
			(void)cui::framework::DependencyPropertyAccess::SetValue(
				*separator, L"BorderBrush",
				BindingValue(cui::drawing::MakeSolidColorBrush(MenuBarBorder)),
				DependencyPropertyValueSource::Theme);
			x += 8.0f;
			continue;
		}
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		auto textSize = font->GetTextSize(item->GetDisplayText());
		int width = static_cast<int>(
			textSize.width + MenuItemHorizontalPadding * 2.0f);
		if (width < 50) width = 50;
		item->Arrange(cui::core::Rect{
			x, 0.0f, static_cast<float>(width), menuExtent });
		x += static_cast<float>(width);
		(void)item->TrySetPropertyValue(
			L"Foreground", BindingValue(GetComputedForegroundBrush()),
			DependencyPropertyValueSource::Theme);
		item->SetStyleState(
			ControlStyleState::Checked, _expand && index == _expandIndex);
	}
}

void Menu::OnRender()
{
	if (!this->IsVisible) return;
	auto d2d = this->GetDrawingContext();
	const auto size = GetRenderSizeDip();
	const float menuExtent = MenuBarExtent();
	this->BeginRender(size.width, size.height);
	{
		d2d->FillRect(0, 0, GetActualSizeDip().width,
			menuExtent, MenuBarBackground);
		const float border = BorderThickness.MaxEdge();
		if (border > 0.0f && MenuBarBorder.a > 0.0f)
			d2d->DrawLine(0.0f, menuExtent - 0.5f,
				GetActualSizeDip().width, menuExtent - 0.5f,
				MenuBarBorder, border);

		auto font = this->GetRenderFont();
		if (_expand && DropCount() > 0)
		{
			auto* top = GetItem(_expandIndex);
			if (top)
			{
				auto calcPanelWidth = [&](std::span<Control* const> items, float maxW) -> float
					{
						float w = 120.0f;
						for (auto* entry : items)
						{
							auto* it = AsMenuItem(entry);
							if (!it) continue;
							auto ts = font->GetTextSize(it->GetDisplayText());
							float tw = ts.width + 24.0f;
							if (!it->InputGestureText.empty())
							{
								auto ss = font->GetTextSize(it->InputGestureText);
								tw += ss.width + 20.0f;
							}
							// 预留子菜单指示符空间
							if (!it->GetMenuItemsView().empty())
								tw += 18.0f;
							if (tw > w) w = tw;
						}
						if (w < 80.0f) w = 80.0f;
						if (maxW > 0.0f && w > maxW) w = maxW;
						return w;
					};

				auto clampPanelXY = [&](float& x, float& y, float w, float h)
					{
						if (!this->GetPresentationWindow()) return;
						const auto viewport = this->GetPresentationWindow()->GetContentViewportSizeDip();
						float maxX = viewport.width;
						float maxY = viewport.height;
						if (x < 0.0f) x = 0.0f;
						if (y < 0.0f) y = 0.0f;
						if (x + w > maxX) x = std::max(0.0f, maxX - w);
						if (y + h > maxY) y = std::max(0.0f, maxY - h);
					};

				// build panels based on open path
				std::vector<MenuPanel> panels;
				panels.reserve(8);

				MenuPanel p0;
				p0.Owner = top;
				p0.Items = top->GetMenuItemsView();
				p0.X = DropLeftLocal();
				p0.Y = DropTopLocal();
				{
					float maxw = GetActualSizeDip().width - DropLeftLocal();
					p0.W = calcPanelWidth(p0.Items, maxw);
					p0.H = PopupVerticalPadding * 2.0f
						+ static_cast<float>(p0.Items.size()) * PopupItemExtent;
					clampPanelXY(p0.X, p0.Y, p0.W, p0.H);
				}
				panels.push_back(p0);

				for (size_t level = 0; level < _openPath.size(); level++)
				{
					int openIdx = _openPath[level];
					if (openIdx < 0) break;
					const auto& prev = panels.back();
					if (prev.Items.empty()) break;
					if (openIdx >= (int)prev.Items.size()) break;
					auto* owner = AsMenuItem(prev.Items[openIdx]);
					if (!IsInteractive(owner)
						|| owner->GetMenuItemsView().empty()) break;

					MenuPanel p;
					p.Owner = owner;
					p.Items = owner->GetMenuItemsView();
					p.W = calcPanelWidth(p.Items, 0.0f);
					p.H = PopupVerticalPadding * 2.0f
						+ static_cast<float>(p.Items.size()) * PopupItemExtent;
					p.X = prev.X + prev.W - 1.0f;
					p.Y = prev.Y + PopupVerticalPadding
						+ static_cast<float>(openIdx) * PopupItemExtent;

					if (this->GetPresentationWindow())
					{
						float maxX = this->GetPresentationWindow()->GetContentViewportSizeDip().width;
						if (p.X + p.W > maxX)
						{
							p.X = prev.X - p.W - 4.0f;
							p.OpenedToLeft = true;
						}
						if (p.X < 0.0f) p.X = 0.0f;
					}
					clampPanelXY(p.X, p.Y, p.W, p.H);
					panels.push_back(p);
					if (panels.size() > 32) break;
				}

				for (size_t level = 0; level < panels.size(); level++)
				{
					const auto& pn = panels[level];
					if (pn.Items.empty()) continue;
					d2d->PushDrawRect(pn.X, pn.Y, pn.W, pn.H);
					d2d->FillRoundRect(
						pn.X, pn.Y, pn.W, pn.H,
						PopupBackground, PopupCornerRadius);
					d2d->DrawRoundRect(
						pn.X, pn.Y, pn.W, pn.H,
						PopupBorder, 1.0f, PopupCornerRadius);

					int hoverIdx = (level < _hoverPath.size() ? _hoverPath[level] : -1);
					int openIdx = (level < _openPath.size() ? _openPath[level] : -1);
					for (int i = 0; i < (int)pn.Items.size(); i++)
					{
						auto* entry = pn.Items[i];
						float iy = pn.Y + PopupVerticalPadding
							+ static_cast<float>(i) * PopupItemExtent;
						if (IsSeparator(entry))
						{
							float y = iy + PopupItemExtent * 0.5f;
							d2d->DrawLine(pn.X + 12.0f, y,
								pn.X + pn.W - 12.0f, y, PopupSeparator, 1.0f);
							continue;
						}
						auto* it = AsMenuItem(entry);
						const bool itemEnabled = IsInteractive(it);
						if (itemEnabled && (i == hoverIdx || i == openIdx))
						{
							const float inset = PopupItemHorizontalInset;
							auto itemRect = D2D1::RectF(pn.X + inset,
								iy + 2.0f, pn.X + pn.W - inset,
								iy + PopupItemExtent - 2.0f);
							const auto hoverColor = PopupHighlight;
							d2d->FillRoundRect(
								itemRect, hoverColor, PopupItemCornerRadius);
							d2d->DrawRoundRect(itemRect,
								BoostAlpha(hoverColor,
									i == openIdx ? 2.1f : 1.7f),
								1.0f, PopupItemCornerRadius);
							const float stripeH = (std::max)(0.0f, itemRect.bottom - itemRect.top - 8.0f);
							if (stripeH > 0.0f)
								d2d->FillRoundRect(itemRect.left + 4.0f, itemRect.top + 4.0f, 3.0f, stripeH, BoostAlpha(hoverColor, 3.0f), 1.5f);
						}

						if (!it || !it->IsVisible) continue;
						auto textColor = PopupText;
						if (!itemEnabled) textColor.a *= 0.45f;
						auto ts = font->GetTextSize(it->GetDisplayText());
						float ty = iy + (PopupItemExtent - ts.height) * 0.5f;
						if (ty < iy) ty = iy;
						d2d->DrawString(it->GetDisplayText(), pn.X + 14.0f, ty,
							textColor, font);
						const float arrowReserve =
							!it->GetMenuItemsView().empty() ? 18.0f : 0.0f;
						if (!it->InputGestureText.empty())
						{
							auto ss = font->GetTextSize(it->InputGestureText);
							float sx = pn.X + pn.W - 14.0f - arrowReserve - ss.width;
							d2d->DrawString(it->InputGestureText, sx, ty,
								textColor, font);
						}

						if (!it->GetMenuItemsView().empty())
						{
							std::wstring arrow = L"\u203A";
							if (i == openIdx && (level + 1) < panels.size() && panels[level + 1].OpenedToLeft)
								arrow = L"\u2039";
							auto as = font->GetTextSize(arrow);
							float ax = pn.X + pn.W - 14.0f - as.width;
							d2d->DrawString(arrow, ax, ty,
								textColor, font);
						}
					}
					d2d->PopDrawRect();
				}
			}
		}
	}
	this->EndRender();
}

bool Menu::ProcessInput(const InputReport& input)
{
	const ControlWeakReference hostLifetime(this);
	ScopeExit projectOnExit{ [hostLifetime]
		{
			if (auto* host = dynamic_cast<Menu*>(hostLifetime.Get()))
				host->SynchronizeInteractionProjection();
		} };
	if (!this->IsEffectivelyEnabled() || !this->IsVisible) return true;
	const int localX = input.X;
	const int localY = input.Y;

	// route to top items (bar area only)
	if (localY >= 0 && localY < MenuBarExtent())
	{
		_hoverTopIndex = -1;
		MenuItem* hoveredTop = nullptr;
		for (int i = 0; i < static_cast<int>(_items.size()); i++)
		{
			auto* it = GetItem(i);
			if (!it) continue;
			const auto loc = it->GetActualLocationDip();
			const cui::core::Rect itemRect{ loc, it->GetActualSizeDip() };
			if (itemRect.Contains(cui::core::Point{ (float)localX, (float)localY }))
			{
				if (IsInteractive(it))
				{
					_hoverTopIndex = i;
					hoveredTop = it;
				}
				break;
			}
		}

		if (input.Kind == InputReportKind::PointerMove)
		{
			if (!hostLifetime) return true;
			// hover切换展开的一级菜单
			if (_expand && _hoverTopIndex >= 0 && _hoverTopIndex != _expandIndex)
			{
				_expandIndex = _hoverTopIndex;
				_hoverPath.clear();
				_openPath.clear();
				this->InvalidateVisual();
			}
		}
		else if (input.Kind == InputReportKind::PointerUp
			&& input.ChangedButton == MouseButton::Left)
		{
			if (_hoverTopIndex >= 0)
			{
				auto* item = GetItem(_hoverTopIndex);
				if (!item)
					return Control::ProcessInput(input);
				const ControlWeakReference itemLifetime(item);
				if (!item->GetMenuItemsView().empty()
					&& _expand && _expandIndex == _hoverTopIndex)
				{
					ClosePopup();
					if (!hostLifetime) return true;
				}
				else if (!item->GetMenuItemsView().empty())
				{
					auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
					auto* expandedItem = dynamic_cast<MenuItem*>(
						itemLifetime.Get());
					if (!host || !expandedItem
						|| expandedItem->GetMenuItemsView().empty()) return true;
					const int expandedIndex = host->IndexOfItem(expandedItem);
					if (expandedIndex < 0) return true;
					host->_expand = true;
					host->_hoverTopIndex = expandedIndex;
					host->_expandIndex = expandedIndex;
					host->_hoverPath.clear();
					host->_openPath.clear();
					if (host->GetPresentationWindow()
						&& !cui::framework::WindowAccess::OpenTransientPresentation(
							*host->GetPresentationWindow(), host,
							TransientPresentationOptions{},
							[](Control& root)
							{
								static_cast<Menu&>(root).ClosePopup();
							}))
					{
						host->ClosePopup();
						return true;
					}
					host = dynamic_cast<Menu*>(hostLifetime.Get());
					if (!host) return true;
				}
				else
				{
					const bool staysOpen = item->StaysOpenOnClick;
					const bool invoked = item->Invoke();
					auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
					if (!host) return true;
					if (invoked && !staysOpen && host->_expand)
					{
						host->ClosePopup();
						host = dynamic_cast<Menu*>(hostLifetime.Get());
						if (!host) return true;
					}
				}
				auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
				if (!host) return true;
				host->InvalidateVisual();
			}
		}

		auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
		return host ? host->Control::ProcessInput(input) : true;
	}

	// dropdown interactions (支持任意层级)
	if (_expand && DropCount() > 0)
	{
			auto* top = GetItem(_expandIndex);
		if (top)
		{
			auto calcPanelWidth = [&](std::span<Control* const> items) -> float
				{
					float w = 120.0f;
					auto font = this->GetRenderFont();
					for (auto* entry : items)
					{
						auto* it = AsMenuItem(entry);
						if (!it) continue;
						auto ts = font->GetTextSize(it->GetDisplayText());
						float tw = ts.width + 24.0f;
						if (!it->InputGestureText.empty())
						{
							auto ss = font->GetTextSize(it->InputGestureText);
							tw += ss.width + 20.0f;
						}
						if (!it->GetMenuItemsView().empty())
							tw += 18.0f;
						if (tw > w) w = tw;
					}
					if (w < 80.0f) w = 80.0f;
					return w;
				};

			auto clampPanelXY = [&](float& x, float& y, float w, float h)
				{
					if (!this->GetPresentationWindow()) return;
					const auto viewport = this->GetPresentationWindow()->GetContentViewportSizeDip();
					float maxX = viewport.width;
					float maxY = viewport.height;
					if (x < 0.0f) x = 0.0f;
					if (y < 0.0f) y = 0.0f;
					if (x + w > maxX) x = std::max(0.0f, maxX - w);
					if (y + h > maxY) y = std::max(0.0f, maxY - h);
				};

			// build panels (local coords)
			std::vector<MenuPanel> panels;
			panels.reserve(8);
			MenuPanel p0;
			p0.Owner = top;
			p0.Items = top->GetMenuItemsView();
			p0.X = DropLeftLocal();
			p0.Y = DropTopLocal();
			p0.W = DropWidthLocal();
			p0.H = PopupVerticalPadding * 2.0f
				+ static_cast<float>(p0.Items.size()) * PopupItemExtent;
			clampPanelXY(p0.X, p0.Y, p0.W, p0.H);
			panels.push_back(p0);

			for (size_t level = 0; level < _openPath.size(); level++)
			{
				int openIdx = _openPath[level];
				if (openIdx < 0) break;
				const auto& prev = panels.back();
				if (prev.Items.empty()) break;
				if (openIdx >= (int)prev.Items.size()) break;
				auto* owner = AsMenuItem(prev.Items[openIdx]);
				if (!IsInteractive(owner)
					|| owner->GetMenuItemsView().empty()) break;
				MenuPanel p;
				p.Owner = owner;
				p.Items = owner->GetMenuItemsView();
				p.W = calcPanelWidth(p.Items);
				p.H = PopupVerticalPadding * 2.0f
					+ static_cast<float>(p.Items.size()) * PopupItemExtent;
				p.X = prev.X + prev.W - 1.0f;
				p.Y = prev.Y + PopupVerticalPadding
					+ static_cast<float>(openIdx) * PopupItemExtent;

				if (this->GetPresentationWindow())
				{
					float maxX = this->GetPresentationWindow()->GetContentViewportSizeDip().width;
					if (p.X + p.W > maxX)
					{
						p.X = prev.X - p.W - 4.0f;
						p.OpenedToLeft = true;
					}
					if (p.X < 0.0f) p.X = 0.0f;
				}
				clampPanelXY(p.X, p.Y, p.W, p.H);
				panels.push_back(p);
				if (panels.size() > 32) break;
			}

			auto pointInRect = [&](float x, float y, const MenuPanel& pn) -> bool
				{
					return (x >= pn.X && x <= pn.X + pn.W && y >= pn.Y && y <= pn.Y + pn.H);
				};

			int hitLevel = -1;
			for (int i = (int)panels.size() - 1; i >= 0; i--)
			{
				if (pointInRect((float)localX, (float)localY, panels[i]))
				{
					hitLevel = i;
					break;
				}
			}

			bool inBridge = false;
			for (size_t i = 0; i + 1 < panels.size(); i++)
			{
				const auto& a = panels[i];
				const auto& b = panels[i + 1];
				float bridgeL = std::min(a.X + a.W - 2.0f, b.X + 2.0f);
				float bridgeR = std::max(a.X + a.W - 2.0f, b.X + 2.0f);
				float bridgeT = b.Y;
				float bridgeB = b.Y + b.H;
				if ((float)localX >= bridgeL && (float)localX <= bridgeR && (float)localY >= bridgeT && (float)localY <= bridgeB)
				{
					inBridge = true;
					break;
				}
			}

			auto ensureSize = [](std::vector<int>& v, size_t n)
				{
					if (v.size() < n) v.resize(n, -1);
				};

			auto itemHasSubMenu = [](MenuItem* it) -> bool
				{
					return IsInteractive(it)
						&& !it->GetMenuItemsView().empty();
				};

			if (input.Kind == InputReportKind::PointerMove)
			{
				if (hitLevel >= 0)
				{
					const auto& pn = panels[hitLevel];
					int itemIndex = static_cast<int>(((float)localY
						- (pn.Y + PopupVerticalPadding)) / PopupItemExtent);
					int itemCount = static_cast<int>(pn.Items.size());
					if (itemIndex < 0 || itemIndex >= itemCount) itemIndex = -1;
					MenuItem* hovered = nullptr;
					if (itemIndex >= 0)
						hovered = AsMenuItem(pn.Items[itemIndex]);
					if (!IsInteractive(hovered))
					{
						hovered = nullptr;
						itemIndex = -1;
					}
					bool needsUpdate = false;

					ensureSize(_hoverPath, (size_t)hitLevel + 1);
					ensureSize(_openPath, (size_t)hitLevel + 1);
					// 清理更深层状态（鼠标在更浅层活动时）
					if (_hoverPath.size() > (size_t)hitLevel + 1)
						_hoverPath.resize((size_t)hitLevel + 1, -1);
					if (_openPath.size() > (size_t)hitLevel + 1)
						_openPath.resize((size_t)hitLevel + 1, -1);

					if (_hoverPath[hitLevel] != itemIndex)
					{
						_hoverPath[hitLevel] = itemIndex;
						needsUpdate = true;
					}

					int newOpen = -1;
					if (itemHasSubMenu(hovered))
						newOpen = itemIndex;

					if (_openPath[hitLevel] != newOpen)
					{
						_openPath[hitLevel] = newOpen;
						needsUpdate = true;
					}
					if (needsUpdate) this->InvalidateVisual();
				}
				else if (inBridge)
				{
					// 桥接区：不断开即可
				}
				else
				{
					// 离开所有面板：清空 hover/open（保持展开）
					if (!_hoverPath.empty() || !_openPath.empty())
					{
						_hoverPath.clear();
						_openPath.clear();
						this->InvalidateVisual();
					}
				}
			}
			else if (input.Kind == InputReportKind::PointerUp
				&& input.ChangedButton == MouseButton::Left)
			{
				if (hitLevel >= 0)
				{
					const auto& pn = panels[hitLevel];
					int itemIndex = static_cast<int>(((float)localY
						- (pn.Y + PopupVerticalPadding)) / PopupItemExtent);
					int itemCount = static_cast<int>(pn.Items.size());
					if (itemIndex >= 0 && itemIndex < itemCount)
					{
						auto* item = AsMenuItem(pn.Items[itemIndex]);
						if (IsInteractive(item))
						{
							// 点击有子菜单项：展开下一层但不触发命令
							if (!item->GetMenuItemsView().empty())
							{
								ensureSize(_hoverPath, (size_t)hitLevel + 1);
								ensureSize(_openPath, (size_t)hitLevel + 1);
								_hoverPath.resize((size_t)hitLevel + 1, -1);
								_openPath.resize((size_t)hitLevel + 1, -1);
								_hoverPath[hitLevel] = itemIndex;
								_openPath[hitLevel] = itemIndex;
								this->InvalidateVisual();
								auto* host = dynamic_cast<Menu*>(
									hostLifetime.Get());
								return host
									? host->Control::ProcessInput(input)
									: true;
							}
							// 叶子项自己发布 Click 并执行统一命令路由。
							const bool staysOpen = item->StaysOpenOnClick;
							const bool invoked = item->Invoke();
							auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
							if (!host) return true;
							if (invoked && !staysOpen)
							{
								host->ClosePopup();
								host = dynamic_cast<Menu*>(
									hostLifetime.Get());
								if (!host) return true;
							}
							return host->Control::ProcessInput(input);
						}
					}
					// 点击分隔符：不处理
				}
				else
				{
					// 点击到下拉外区域：只收起
					ClosePopup();
					auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
					return host
						? host->Control::ProcessInput(input) : true;
				}
			}
		}
	}
	// 展开时点击菜单栏/下拉之外：收起（配合 ActualSize 覆盖内容区）
	else if (_expand && input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left)
	{
		ClosePopup();
	}

	auto* host = dynamic_cast<Menu*>(hostLifetime.Get());
	return host ? host->Control::ProcessInput(input) : true;
}

