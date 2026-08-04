#include "ContextMenu.h"
#include "EventInfrastructure.h"
#include "Popup.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "Window.h"
#include "WindowInfrastructure.h"
#include <algorithm>
#include <cmath>

namespace
{
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

UIClass ContextMenu::Type() { return UIClass::UI_ContextMenu; }
GET_CPP(ContextMenu, Control*, PlacementTarget) { return _placementTarget.Get(); }

const DependencyProperty& ContextMenu::IsOpenProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			ContextMenu& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsOpenChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<ContextMenu, bool>(
			DependencyPropertyRegistrationLiteral(L"IsOpen"),
			[](ContextMenu& target) { return target.GetIsOpen(); },
			[](ContextMenu& target, const bool& value)
			{ target.SetIsOpen(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::StaysOpenProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::None;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<ContextMenu, bool>(
			DependencyPropertyRegistrationLiteral(L"StaysOpen"),
			[](ContextMenu& target) { return target.GetStaysOpen(); },
			[](ContextMenu& target, const bool& value)
			{ target.SetStaysOpen(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::PlacementTargetProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, ControlWeakReference> options;
		options.DefaultValue = {};
		options.Flags = DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ContextMenu, ControlWeakReference>(
				DependencyPropertyRegistrationLiteral(L"PlacementTarget"),
				[](ContextMenu& target) { return target._placementTarget; },
				[](ContextMenu& target, const ControlWeakReference& value)
				{ target.ApplyPlacementTarget(value); }, {},
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::HorizontalOffsetProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, float> options;
		options.DefaultValue = 0.0f;
		options.Flags = DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 40;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Step = 0.5;
		)
		options.Validate = [](const float& value)
		{ return std::isfinite(value); };
		options.Changed = [](
			ContextMenu& target, const float&, const float&)
		{
			if (target._isOpen) target.ArrangePopupSurface();
		};
		return DependencyPropertyRegistry::RegisterStatic<ContextMenu, float>(
			DependencyPropertyRegistrationLiteral(L"HorizontalOffset"),
			[](ContextMenu& target) { return target.HorizontalOffset; },
			[](ContextMenu& target, const float& value)
			{ target.HorizontalOffset = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::VerticalOffsetProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, float> options;
		options.DefaultValue = 0.0f;
		options.Flags = DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 50;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Step = 0.5;
		)
		options.Validate = [](const float& value)
		{ return std::isfinite(value); };
		options.Changed = [](
			ContextMenu& target, const float&, const float&)
		{
			if (target._isOpen) target.ArrangePopupSurface();
		};
		return DependencyPropertyRegistry::RegisterStatic<ContextMenu, float>(
			DependencyPropertyRegistrationLiteral(L"VerticalOffset"),
			[](ContextMenu& target) { return target.VerticalOffset; },
			[](ContextMenu& target, const float& value)
			{ target.VerticalOffset = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::PlacementRectangleProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, cui::core::Rect> options;
		options.DefaultValue = {};
		options.Flags = DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 60;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			ContextMenu& target,
			const cui::core::Rect&,
			const cui::core::Rect&)
		{
			if (target._isOpen) target.ArrangePopupSurface();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			ContextMenu, cui::core::Rect>(
				DependencyPropertyRegistrationLiteral(L"PlacementRectangle"),
				[](ContextMenu& target) { return target.PlacementRectangle; },
				[](ContextMenu& target, const cui::core::Rect& value)
				{ target.PlacementRectangle = value; }, {},
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::PlacementProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, PlacementMode> options;
		options.DefaultValue = PlacementMode::MousePoint;
		options.Flags = DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 70;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Absolute", BindingValue(PlacementMode::Absolute) },
			{ L"Bottom", BindingValue(PlacementMode::Bottom) },
			{ L"Top", BindingValue(PlacementMode::Top) },
			{ L"Left", BindingValue(PlacementMode::Left) },
			{ L"Right", BindingValue(PlacementMode::Right) },
			{ L"Center", BindingValue(PlacementMode::Center) },
			{ L"MousePoint", BindingValue(PlacementMode::MousePoint) }
		};
		)
		options.Changed = [](
			ContextMenu& target,
			const PlacementMode&, const PlacementMode&)
		{
			if (target._isOpen) target.ArrangePopupSurface();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			ContextMenu, PlacementMode>(
				DependencyPropertyRegistrationLiteral(L"Placement"),
				[](ContextMenu& target) { return target.Placement; },
				[](ContextMenu& target, const PlacementMode& value)
				{ target.Placement = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ContextMenu::HasDropShadowProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ContextMenu, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 10;
		options.Design.Order = 80;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<ContextMenu, bool>(
			DependencyPropertyRegistrationLiteral(L"HasDropShadow"),
			[](ContextMenu& target) { return target.HasDropShadow; },
			[](ContextMenu& target, const bool& value)
			{ target.HasDropShadow = value; }, {}, std::move(options));
	}();
	return *registration;
}

void ContextMenu::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
	MenuItem::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsOpenProperty();
	(void)StaysOpenProperty();
	(void)PlacementTargetProperty();
	(void)HorizontalOffsetProperty();
	(void)VerticalOffsetProperty();
	(void)PlacementRectangleProperty();
	(void)PlacementProperty();
	(void)HasDropShadowProperty();
#endif
}

void ContextMenu::SetIsOpen(bool value)
{
	(void)SetPropertyField(IsOpenProperty(), _isOpen, value);
}

void ContextMenu::SetStaysOpen(bool value)
{
	const ControlWeakReference hostLifetime(this);
	if (!SetPropertyField(
		StaysOpenProperty(), _staysOpen, value)) return;
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (host && host->_isPresented && host->GetPresentationWindow())
	{
		TransientPresentationOptions options;
		options.DismissOnOutsidePointerDown = !host->_staysOpen;
		options.DismissOnWindowDeactivation = !host->_staysOpen;
		(void)cui::framework::WindowAccess::OpenTransientPresentation(
			*host->GetPresentationWindow(), host, options,
			[](Control& root)
			{ static_cast<ContextMenu&>(root).Hide(); });
	}
}

SET_CPP(ContextMenu, Control*, PlacementTarget)
{
	const ControlWeakReference hostLifetime(this);
	ClearServicePlacementTarget();
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	(void)host->TrySetPropertyValue(
		PlacementTargetProperty(),
		BindingValue(ControlWeakReference(value)),
		DependencyPropertyValueSource::Local);
}

GET_CPP(ContextMenu, float, HorizontalOffset)
{
	return _horizontalOffset;
}

SET_CPP(ContextMenu, float, HorizontalOffset)
{
	(void)SetPropertyField(
		HorizontalOffsetProperty(), _horizontalOffset,
		std::isfinite(value) ? value : 0.0f);
}

GET_CPP(ContextMenu, float, VerticalOffset)
{
	return _verticalOffset;
}

SET_CPP(ContextMenu, float, VerticalOffset)
{
	(void)SetPropertyField(
		VerticalOffsetProperty(), _verticalOffset,
		std::isfinite(value) ? value : 0.0f);
}

GET_CPP(ContextMenu, cui::core::Rect, PlacementRectangle)
{
	return _placementRectangle;
}

SET_CPP(ContextMenu, cui::core::Rect, PlacementRectangle)
{
	(void)SetPropertyField(
		PlacementRectangleProperty(), _placementRectangle, value);
}

GET_CPP(ContextMenu, PlacementMode, Placement)
{
	return _placement;
}

SET_CPP(ContextMenu, PlacementMode, Placement)
{
	(void)SetPropertyField(PlacementProperty(), _placement, value);
}

GET_CPP(ContextMenu, bool, HasDropShadow)
{
	return _hasDropShadow;
}

SET_CPP(ContextMenu, bool, HasDropShadow)
{
	(void)SetPropertyField(
		HasDropShadowProperty(), _hasDropShadow, value);
}

void ContextMenu::ApplyPlacementTarget(
	const ControlWeakReference& value)
{
	const ControlWeakReference hostLifetime(this);
	if (!SetPropertyField(
		PlacementTargetProperty(), _placementTarget, value)) return;
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->SynchronizeItemCommandHosts();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (host && host->_isOpen) host->ArrangePopupSurface();
}

bool ContextMenu::ApplyServicePlacementTarget(Control* value)
{
	const ControlWeakReference hostLifetime(this);
	if (!value || _servicePlacementTargetActive
		|| _placementTarget.HasValue()) return false;
	_servicePlacementTarget = ControlWeakReference(value);
	_servicePlacementTargetActive = true;
	if (TrySetCurrentPropertyValue(
		PlacementTargetProperty(),
		BindingValue(_servicePlacementTarget)))
		return true;
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return false;
	host->_servicePlacementTarget = ControlWeakReference{};
	host->_servicePlacementTargetActive = false;
	return false;
}

void ContextMenu::ClearServicePlacementTarget()
{
	if (!_servicePlacementTargetActive) return;
	const auto serviceTarget = _servicePlacementTarget;
	_servicePlacementTarget = ControlWeakReference{};
	_servicePlacementTargetActive = false;
	if (_placementTarget == serviceTarget)
		(void)TrySetCurrentPropertyValue(
			PlacementTargetProperty(),
			BindingValue(ControlWeakReference{}));
}

ContextMenu::ContextMenu()
	: ItemsControl(),
	_placement(PlacementMode::MousePoint)
{
	RegisterDependencyProperties();
	if (auto* itemsHost = GetItemsHost())
		cui::framework::TemplateAccess::
			SetParticipatesInPresentationScene(*itemsHost, false);
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->RendererBorderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
	SetPresentationSuppressed(true);
}

ContextMenu::~ContextMenu()
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

void ContextMenu::AttachItemTree(MenuItem* item)
{
	if (!item) return;
	item->_parentItem = nullptr;
	item->SetStructureChangedHandler([this]()
		{
			ClearHoverState();
			if (_isOpen)
				Hide();
			else InvalidateVisual();
		});
	item->SetInteractionStateChangedHandler(
		[this](MenuItem& source) { OnItemInteractionStateChanged(source); });
	item->AttachCommandHost(*this, _placementTarget);
}

bool ContextMenu::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const MenuItem*>(&item)
		|| dynamic_cast<const Separator*>(&item)) return true;
	error = "ContextMenu Items can contain MenuItem or Separator controls only";
	return false;
}

void ContextMenu::OnBeforeGeneratedItemsRebuilt()
{
	_generatedItemsRebuildSnapshot.clear();
	_generatedItemsRebuildSnapshot.reserve(_items.size());
	for (auto* entry : _items)
		_generatedItemsRebuildSnapshot.emplace_back(entry);
	_generatedItemsRebuildPending = true;
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

void ContextMenu::SynchronizeItems()
{
	std::vector<Control*> current;
	current.reserve(ItemCount());
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		auto* item = GetGeneratedItem(index);
		if (item) current.push_back(item);
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
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		item->SetStructureChangedHandler({});
		item->SetInteractionStateChangedHandler({});
		item->DetachCommandHost(*this);
	}
	_items = std::move(current);
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) AttachItemTree(item);
	if (!structureChanged)
	{
		InvalidateVisual();
		return;
	}
	ClearHoverState();
	// Initial template/container realization happens after IsOpen becomes true
	// but before the transient root is presented.  That is not a live menu
	// mutation and must not cancel the first ShowAt transaction.
	if (_isPresented)
		Hide();
	else InvalidateVisual();
}

void ContextMenu::OnAuthoredItemsChanged() noexcept
{
	try { SynchronizeItems(); }
	catch (...) { _items.clear(); }
}

void ContextMenu::OnGeneratedItemsRebuilt()
{
	SynchronizeItems();
}

std::unique_ptr<Panel> ContextMenu::CreateItemsHost() const
{
	auto itemsHost = ItemsControl::CreateItemsHost();
	if (itemsHost)
		cui::framework::TemplateAccess::
			SetParticipatesInPresentationScene(
				*itemsHost, GetControlTemplateRoot() != nullptr);
	return itemsHost;
}

void ContextMenu::OnControlTemplatePresentationChanged()
{
	ItemsControl::OnControlTemplatePresentationChanged();
	if (auto* itemsHost = GetItemsHost())
		cui::framework::TemplateAccess::
			SetParticipatesInPresentationScene(
				*itemsHost, GetControlTemplateRoot() != nullptr);
	if (_isOpen) ArrangePopupSurface();
}

std::unique_ptr<Control> ContextMenu::WrapGeneratedItem(
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

void ContextMenu::SynchronizeItemCommandHosts()
{
	const ControlWeakReference hostLifetime(this);
	std::vector<ControlWeakReference> items;
	items.reserve(_items.size());
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) items.emplace_back(item);
	for (const auto& itemLifetime : items)
	{
		auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		auto* item = dynamic_cast<MenuItem*>(itemLifetime.Get());
		if (!host || !item || host->IndexOfItem(item) < 0)
			continue;
		host->AttachItemTree(item);
	}
}

void ContextMenu::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	const ControlWeakReference hostLifetime(this);
	const ControlWeakReference previousWindowLifetime(previousWindow);
	const ControlWeakReference currentWindowLifetime(currentWindow);
	Control::OnPresentationWindowChanged(previousWindow, currentWindow);
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	auto* livePreviousWindow = previousWindow
		? dynamic_cast<Window*>(previousWindowLifetime.Get()) : nullptr;
	if (previousWindow && !livePreviousWindow) return;
	if (livePreviousWindow)
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*livePreviousWindow, host);
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->_isPresented = false;
	if (host->_servicePlacementTargetActive)
	{
		auto* placementTarget = host->_servicePlacementTarget.Get();
		auto* liveCurrentWindow = currentWindow
			? dynamic_cast<Window*>(currentWindowLifetime.Get()) : nullptr;
		if (currentWindow && !liveCurrentWindow) return;
		if (!placementTarget
			|| placementTarget->GetPresentationWindow() != liveCurrentWindow)
			host->ClearServicePlacementTarget();
	}
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->SynchronizeItemCommandHosts();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	auto* liveCurrentWindow = currentWindow
		? dynamic_cast<Window*>(currentWindowLifetime.Get()) : nullptr;
	if (currentWindow && !liveCurrentWindow) return;
	if (host->_isOpen && liveCurrentWindow)
		host->PresentCore();
}

void ContextMenu::OnItemInteractionStateChanged(MenuItem& source)
{
	std::vector<int> path;
	if (!FindMenuItemPath(_items, &source, path) || path.empty()) return;
	if (!IsInteractive(&source))
	{
		source.SetIsSubmenuOpenCore(false);
		return;
	}
	if (_isOpen && source.IsSubmenuOpen
		&& !source.GetMenuItemsView().empty())
	{
		auto* root = source.RootTopLevelItem();
		for (auto* entry : _items)
		{
			auto* item = AsMenuItem(entry);
			if (!item || item == root) continue;
			item->SetIsSubmenuOpenCore(false);
			item->SetIsHighlightedCore(false);
		}
	}
	InvalidateVisual();
}


void ContextMenu::ClearHoverState()
{
	const ControlWeakReference hostLifetime(this);
	_hoverPath.clear();
	_openPath.clear();
	std::vector<ControlWeakReference> items;
	auto snapshot = [&](auto&& self, MenuItem& item) -> void
	{
		items.emplace_back(&item);
		for (auto* child : item.GetMenuItemsView())
			if (auto* menuItem = AsMenuItem(child))
				self(self, *menuItem);
	};
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) snapshot(snapshot, *item);
	for (const auto& itemLifetime : items)
	{
		auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		auto* item = dynamic_cast<MenuItem*>(itemLifetime.Get());
		std::vector<int> path;
		if (!host || !item
			|| !FindMenuItemPath(host->_items, item, path)) continue;
		item->SetIsSubmenuOpenCore(false);
		host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		item = dynamic_cast<MenuItem*>(itemLifetime.Get());
		path.clear();
		if (!host || !item
			|| !FindMenuItemPath(host->_items, item, path)) continue;
		item->SetIsHighlightedCore(false);
	}
}

void ContextMenu::SynchronizeInteractionProjection()
{
	const ControlWeakReference hostLifetime(this);
	std::vector<ControlWeakReference> items;
	items.reserve(_items.size());
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) items.emplace_back(item);
	for (const auto& itemLifetime : items)
	{
		auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		auto* item = dynamic_cast<MenuItem*>(itemLifetime.Get());
		if (!host || !item || host->IndexOfItem(item) < 0) continue;
		item->UpdateRole();
	}
}


cui::core::Size ContextMenu::GetRenderSizeDip()
{
	return _isOpen ? ItemsControl::GetRenderSizeDip()
		: cui::core::Size{};
}

bool ContextMenu::ContainsPoint(int localX, int localY)
{
	return _isOpen
		&& ItemsControl::ContainsPoint(localX, localY);
}

void ContextMenu::PreparePresentation()
{
	ItemsControl::PreparePresentation();
	if (_isOpen) ArrangePopupSurface();
}

bool ContextMenu::HandlesNavigationKey(Key key) const
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
		return ItemsControl::HandlesNavigationKey(key);
	}
}

MenuItem* ContextMenu::HitTopLevelItem(
	int rootX, int rootY) const noexcept
{
	for (auto* entry : _items)
	{
		auto* item = AsMenuItem(entry);
		if (!IsInteractive(item)) continue;
		// ContextMenu is a transient presentation root. Host-level input uses
		// Window content coordinates (the same coordinate space as placement),
		// while a MenuItem reached by normal scene hit testing receives its own
		// retargeted local report and never comes through this fallback.
		if (item->GetAbsoluteRectDip().Contains(
			cui::core::Point{
				static_cast<float>(rootX),
				static_cast<float>(rootY) }))
			return item;
	}
	return nullptr;
}

bool ContextMenu::ProcessInput(const InputReport& input)
{
	if (!IsEffectivelyEnabled() || !IsVisible || !_isOpen)
		return true;
	if (_ignoreNextMouseUp
		&& input.Kind == InputReportKind::PointerDown)
		_ignoreNextMouseUp = false;
	if (_ignoreNextMouseUp
		&& input.Kind == InputReportKind::PointerUp)
	{
		_ignoreNextMouseUp = false;
		return true;
	}
	if (input.Kind == InputReportKind::PointerMove)
	{
		if (auto* item = HitTopLevelItem(input.X, input.Y))
		{
			const ControlWeakReference itemLifetime(item);
			item->SetIsHighlightedCore(true);
			item = dynamic_cast<MenuItem*>(itemLifetime.Get());
			if (item && !item->GetMenuItemsView().empty())
				item->SetIsSubmenuOpenCore(true);
		}
		return true;
	}
	if (input.Kind == InputReportKind::PointerUp
		&& (input.ChangedButton == MouseButton::Left
			|| input.ChangedButton == MouseButton::Right))
	{
		if (auto* item = HitTopLevelItem(input.X, input.Y))
		{
			if (!item->GetMenuItemsView().empty())
				item->SetIsSubmenuOpenCore(true);
			else
				(void)item->InvokeLeafAndDismiss();
		}
		return true;
	}
	if (input.Kind == InputReportKind::KeyDown)
	{
		if (input.Key == Key::Escape)
		{
			Hide();
			return true;
		}
		if ((input.Key == Key::Down
			|| input.Key == Key::Home)
			&& !_items.empty())
			for (auto* entry : _items)
				if (auto* item = AsMenuItem(entry);
					IsInteractive(item))
				{
					(void)item->FocusForMenuNavigation();
					return true;
				}
	}
	return ItemsControl::ProcessInput(input);
}

MenuItem* ContextMenu::AddItem(std::unique_ptr<MenuItem> item)
{
	return InsertItem(static_cast<int>(AuthoredItemCount()), std::move(item));
}

MenuItem* ContextMenu::InsertItem(
	int index, std::unique_ptr<MenuItem> item)
{
	if (index < 0 || index > static_cast<int>(AuthoredItemCount()) || !item)
		return nullptr;
	if (_isOpen) Hide();
	return static_cast<MenuItem*>(InsertItemControl(
		static_cast<size_t>(index), std::move(item)));
}

Separator* ContextMenu::AddSeparator()
{
	return static_cast<Separator*>(AddItemControl(
		std::make_unique<Separator>()));
}

MenuItem* ContextMenu::GetItem(int index) const noexcept
{
	if (index < 0 || static_cast<size_t>(index) >= _items.size())
		return nullptr;
	return AsMenuItem(_items[static_cast<size_t>(index)]);
}

int ContextMenu::IndexOfItem(const MenuItem* item) const noexcept
{
	if (!item) return -1;
	auto found = std::find(_items.begin(), _items.end(), item);
	return found == _items.end()
		? -1 : static_cast<int>(found - _items.begin());
}

static MenuItem* FindContextMenuItemByCommand(
	std::span<Control* const> items,
	const std::wstring& command, bool recursive) noexcept
{
	for (auto* entry : items)
	{
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		if (item->Command == command) return item;
		if (recursive)
		{
				auto* found = FindContextMenuItemByCommand(
					item->GetMenuItemsView(), command, true);
			if (found) return found;
		}
	}
	return nullptr;
}

static MenuItem* FindContextMenuItemByText(
	std::span<Control* const> items,
	const std::wstring& text, bool recursive) noexcept
{
	for (auto* entry : items)
	{
		auto* item = AsMenuItem(entry);
		if (!item) continue;
		if (item->GetDisplayText() == text) return item;
		if (recursive)
		{
				auto* found = FindContextMenuItemByText(
					item->GetMenuItemsView(), text, true);
			if (found) return found;
		}
	}
	return nullptr;
}

MenuItem* ContextMenu::FindItemByCommand(
	const std::wstring& command, bool recursive) const noexcept
{
	return FindContextMenuItemByCommand(_items, command, recursive);
}

MenuItem* ContextMenu::FindItemByText(
	const std::wstring& text, bool recursive) const noexcept
{
	return FindContextMenuItemByText(_items, text, recursive);
}

std::unique_ptr<Control> ContextMenu::DetachItemAt(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= AuthoredItemCount())
		return {};
	if (_isOpen) Hide();
	return DetachItemControlAt(static_cast<size_t>(index));
}

std::unique_ptr<MenuItem> ContextMenu::DetachItem(MenuItem* item)
{
	if (!item) return {};
	auto* root = item;
	while (root->ParentItem()) root = root->ParentItem();
	if (IndexOfItem(root) < 0) return {};
	if (auto* parent = item->ParentItem())
		return parent->DetachSubItem(item);
	const auto index = IndexOfItem(item);
	if (index < 0) return {};
	if (_isOpen) Hide();
	auto detached = DetachItemControlAt(static_cast<size_t>(index));
	return std::unique_ptr<MenuItem>(
		static_cast<MenuItem*>(detached.release()));
}

bool ContextMenu::RemoveItemAt(int index)
{
	auto item = DetachItemAt(index);
	return item != nullptr;
}

bool ContextMenu::RemoveItem(MenuItem* item)
{
	auto removed = DetachItem(item);
	return removed != nullptr;
}

bool ContextMenu::RemoveItemByCommand(
	const std::wstring& command, bool recursive)
{
	return RemoveItem(FindItemByCommand(command, recursive));
}

void ContextMenu::ClearItems()
{
	if (_isOpen) Hide();
	ClearItemControls();
	ClearHoverState();
}

void ContextMenu::ArrangePopupSurface()
{
	if (!_isOpen || !GetPresentationWindow()) return;
	const auto viewport =
		GetPresentationWindow()->GetContentViewportSizeDip();
	const float edge = 2.0f;
	const float maximumWidth =
		(std::max)(1.0f, viewport.width - edge * 2.0f);
	const float maximumHeight =
		(std::max)(1.0f, viewport.height - edge * 2.0f);
	const auto desired = Measure(cui::core::Constraints{
		{ 0.0f, 0.0f },
		{ maximumWidth, maximumHeight } });
	const float width = (std::clamp)(
		desired.width, 1.0f, maximumWidth);
	const float height = (std::clamp)(
		desired.height, 1.0f, maximumHeight);

	cui::core::Rect targetRect{};
	if (auto* target = _placementTarget.Get())
		targetRect = target->GetAbsoluteRectDip();
	if (_placementRectangle != cui::core::Rect{})
	{
		targetRect = _placementRectangle.Normalized();
		if (auto* target = _placementTarget.Get())
		{
			const auto absolute = target->GetAbsoluteLocationDip();
			targetRect.x += absolute.x;
			targetRect.y += absolute.y;
		}
	}

	float x = _anchor.x;
	float y = _anchor.y;
	switch (_placement)
	{
	case PlacementMode::Bottom:
		x = targetRect.x;
		y = targetRect.y + targetRect.height;
		if (y + height > viewport.height - edge
			&& targetRect.y - height >= edge)
			y = targetRect.y - height;
		break;
	case PlacementMode::Top:
		x = targetRect.x;
		y = targetRect.y - height;
		if (y < edge
			&& targetRect.y + targetRect.height + height
				<= viewport.height - edge)
			y = targetRect.y + targetRect.height;
		break;
	case PlacementMode::Left:
		x = targetRect.x - width;
		y = targetRect.y;
		if (x < edge
			&& targetRect.x + targetRect.width + width
				<= viewport.width - edge)
			x = targetRect.x + targetRect.width;
		break;
	case PlacementMode::Right:
		x = targetRect.x + targetRect.width;
		y = targetRect.y;
		if (x + width > viewport.width - edge
			&& targetRect.x - width >= edge)
			x = targetRect.x - width;
		break;
	case PlacementMode::Center:
		x = targetRect.x
			+ (targetRect.width - width) * 0.5f;
		y = targetRect.y
			+ (targetRect.height - height) * 0.5f;
		break;
	case PlacementMode::Absolute:
		if (_placementRectangle != cui::core::Rect{})
		{
			x = targetRect.x;
			y = targetRect.y;
		}
		break;
	case PlacementMode::MousePoint:
	default:
		break;
	}
	x += _horizontalOffset;
	y += _verticalOffset;
	x = (std::clamp)(
		x, edge, (std::max)(edge,
			viewport.width - width - edge));
	y = (std::clamp)(
		y, edge, (std::max)(edge,
			viewport.height - height - edge));

	cui::core::Point parentAbsolute{};
	if (auto* parent = GetVisualParent())
		parentAbsolute = parent->GetAbsoluteLocationDip();
	ItemsControl::Arrange({
		x - parentAbsolute.x,
		y - parentAbsolute.y,
		width,
		height });
}

void ContextMenu::ShowAtCore(
	Control* placementTarget,
	int x, int y,
	bool ignoreNextMouseUp)
{
	const ControlWeakReference hostLifetime(this);
	const ControlWeakReference placementLifetime(placementTarget);
	if (!this->GetPresentationWindow() || _items.empty()) return;
	if (placementTarget
		&& placementTarget->GetPresentationWindow() != this->GetPresentationWindow())
		return;
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->ClearServicePlacementTarget();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow()) return;
	if (placementTarget)
		(void)host->ApplyServicePlacementTarget(placementTarget);
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow()) return;
	if (placementTarget
		&& (!placementLifetime
			|| placementLifetime.Get()->GetPresentationWindow() != host->GetPresentationWindow()))
		return;
	host->_anchor = cui::core::Point{
		static_cast<float>(x), static_cast<float>(y) };
	host->_ignoreNextMouseUp = ignoreNextMouseUp;
	if (host->_isOpen)
	{
		host->PresentCore();
		return;
	}
	(void)host->TrySetCurrentPropertyValue(
		IsOpenProperty(), BindingValue(true));
}

void ContextMenu::ApplyIsOpenChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (newValue) PresentCore();
	else DismissPresentationCore();
}

void ContextMenu::PresentCore()
{
	const ControlWeakReference hostLifetime(this);
	const ControlWeakReference placementLifetime(_placementTarget.Get());
	if (!_isOpen || !GetPresentationWindow() || _items.empty()) return;
	std::vector<ControlWeakReference> items;
	items.reserve(_items.size());
	for (auto* entry : _items)
		if (auto* item = AsMenuItem(entry)) items.emplace_back(item);
	for (const auto& itemLifetime : items)
	{
		auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		auto* item = dynamic_cast<MenuItem*>(itemLifetime.Get());
		if (!host || !item || host->IndexOfItem(item) < 0)
			continue;
		host->AttachItemTree(item);
	}
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
	if (host->_placementTarget.HasValue()
		&& (!placementLifetime
			|| placementLifetime.Get()->GetPresentationWindow() != host->GetPresentationWindow()))
	{
		host->Hide();
		return;
	}
	const bool wasPresented = host->_isPresented;
	host->ClearHoverState();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
	if (host->_placementTarget.HasValue()
		&& (!placementLifetime
			|| placementLifetime.Get()->GetPresentationWindow() != host->GetPresentationWindow()))
	{
		host->Hide();
		return;
	}
	host->SetPresentationSuppressed(false);
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
	host->ArrangePopupSurface();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
	TransientPresentationOptions options;
	options.DismissOnOutsidePointerDown = !host->_staysOpen;
	options.DismissOnWindowDeactivation = !host->_staysOpen;
	const bool presented = cui::framework::WindowAccess::OpenTransientPresentation(
		*host->GetPresentationWindow(), host, options,
		[](Control& root)
		{ static_cast<ContextMenu&>(root).Hide(); });
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow()) return;
	if (!presented)
	{
		host->Hide();
		return;
	}
	host->_isPresented = true;
	host->SynchronizeInteractionProjection();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
	if (!wasPresented)
	{
		cui::framework::EventAccess::Raise(host->Opened, host);
		host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
		ControlWeakReference focusCandidate;
		for (auto* entry : host->_items)
			if (auto* item = AsMenuItem(entry);
				IsInteractive(item))
			{
				focusCandidate = item;
				break;
			}
		auto* item = dynamic_cast<MenuItem*>(focusCandidate.Get());
		if (item && host->IndexOfItem(item) >= 0)
			(void)item->FocusForMenuNavigation();
		host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		if (!host || !host->GetPresentationWindow() || !host->_isOpen) return;
	}
	host->InvalidateVisual();
}

void ContextMenu::DismissPresentationCore()
{
	const ControlWeakReference hostLifetime(this);
	const bool wasPresented = _isPresented;
	_isPresented = false;
	_ignoreNextMouseUp = false;
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->ClearHoverState();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->SynchronizeInteractionProjection();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	std::vector<ControlWeakReference> items;
	items.reserve(host->_items.size());
	for (auto* entry : host->_items)
		if (auto* item = AsMenuItem(entry)) items.emplace_back(item);
	for (const auto& itemLifetime : items)
	{
		host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		auto* item = dynamic_cast<MenuItem*>(itemLifetime.Get());
		if (!host || !item || host->IndexOfItem(item) < 0)
			continue;
		host->AttachItemTree(item);
	}
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || host->_isOpen) return;
	if (host->GetPresentationWindow())
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*host->GetPresentationWindow(), host);
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->SetPresentationSuppressed(true);
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	host->ClearServicePlacementTarget();
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	if (wasPresented)
	{
		cui::framework::EventAccess::Raise(host->Closed, host);
		host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		if (!host) return;
	}
	host->InvalidateVisual();
}

void ContextMenu::ShowAt(int x, int y, bool ignoreNextMouseUp)
{
	ShowAtCore(nullptr, x, y, ignoreNextMouseUp);
}

void ContextMenu::ShowAt(class Control* relativeTo, int x, int y, bool ignoreNextMouseUp)
{
	if (!relativeTo)
	{
		ShowAtCore(nullptr, x, y, ignoreNextMouseUp);
		return;
	}
	if (!this->GetPresentationWindow()
		|| relativeTo->GetPresentationWindow() != this->GetPresentationWindow())
		return;
	const auto relativeAbs = relativeTo->GetAbsoluteLocationDip();
	ShowAtCore(relativeTo,
		static_cast<int>(std::round(relativeAbs.x + (float)x)),
		static_cast<int>(std::round(relativeAbs.y + (float)y)),
		ignoreNextMouseUp);
}

void ContextMenu::Hide()
{
	const ControlWeakReference hostLifetime(this);
	if (!_isOpen && !_isPresented)
		return;
	(void)TrySetCurrentPropertyValue(
		IsOpenProperty(), BindingValue(false));
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host) return;
	// A failed current-value commit can only occur for invalid metadata. Keep
	// transient presentation coherent even in that defensive path.
	if (!host->_isOpen && host->_isPresented)
		host->DismissPresentationCore();
}
