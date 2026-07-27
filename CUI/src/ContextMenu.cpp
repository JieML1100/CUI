#include "ContextMenu.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "Window.h"
#include "WindowInfrastructure.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>

namespace
{
	// Private fallback-presenter metrics. Templates own the public appearance.
	constexpr float ItemHorizontalPadding = 12.0f;
	constexpr float PopupVerticalPadding = 6.0f;
	constexpr float PopupItemExtent = 28.0f;
	constexpr float PopupBorderThickness = 1.0f;
	constexpr float PopupCornerRadius = 8.0f;
	constexpr float PopupItemCornerRadius = 6.0f;
	constexpr float PopupItemHorizontalInset = 6.0f;
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

	std::optional<DependencyPropertyKey>&
	PlacementTargetPropertyKeyStorage()
	{
		static std::optional<DependencyPropertyKey> key;
		return key;
	}

	const DependencyPropertyKey& PlacementTargetPropertyKey(
		ContextMenu& target)
	{
		target.EnsureBindingPropertiesRegistered();
		return PlacementTargetPropertyKeyStorage().value();
	}

	D2D1_COLOR_F BoostAlpha(D2D1_COLOR_F color, float factor)
	{
		color.a = (std::clamp)(color.a * factor, 0.0f, 1.0f);
		return color;
	}

	D2D1_SIZE_F LogicalPopupExtent(Window* form)
	{
		if (!form) return D2D1::SizeF(0.0f, 0.0f);
		const auto viewport = form->GetContentViewportSizeDip();
		return D2D1::SizeF(viewport.width, viewport.height);
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

UIClass ContextMenu::Type() { return UIClass::UI_ContextMenu; }
GET_CPP(ContextMenu, Control*, PlacementTarget) { return _placementTarget.Get(); }

void ContextMenu::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
	MenuItem::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto subscriber = [](const wchar_t* propertyName)
		{
			return [name = std::wstring(propertyName)](
				ContextMenu& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[name, handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.PropertyName == name)
							handler();
					});
			};
		};

		DependencyPropertyOptions<ContextMenu, bool> openOptions;
		openOptions.DefaultValue = false;
		openOptions.Flags = DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		openOptions.Design.Category = L"Behavior";
		openOptions.Design.CategoryOrder = 300;
		openOptions.Design.Order = 10;
		openOptions.Design.Editor = DependencyPropertyEditorKind::Boolean;
		openOptions.Design.Persistence = DependencyPropertyPersistence::Metadata;
		openOptions.Changed = [](
			ContextMenu& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsOpenChange(oldValue, newValue);
		};
		DependencyPropertyRegistry::Register<ContextMenu, bool>(L"IsOpen",
			[](ContextMenu& target) { return target.GetIsOpen(); },
			[](ContextMenu& target, const bool& value)
			{ target.SetIsOpen(value); },
			subscriber(L"IsOpen"), std::move(openOptions));

		DependencyPropertyOptions<ContextMenu, bool> staysOpenOptions;
		staysOpenOptions.DefaultValue = false;
		staysOpenOptions.Flags = DependencyPropertyFlags::None;
		staysOpenOptions.Design.Category = L"Behavior";
		staysOpenOptions.Design.CategoryOrder = 300;
		staysOpenOptions.Design.Order = 20;
		staysOpenOptions.Design.Editor = DependencyPropertyEditorKind::Boolean;
		staysOpenOptions.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<ContextMenu, bool>(L"StaysOpen",
			[](ContextMenu& target) { return target.GetStaysOpen(); },
			[](ContextMenu& target, const bool& value)
			{ target.SetStaysOpen(value); },
			subscriber(L"StaysOpen"), std::move(staysOpenOptions));

		DependencyPropertyOptions<ContextMenu, ControlWeakReference> targetOptions;
		targetOptions.DefaultValue = {};
		targetOptions.Flags = DependencyPropertyFlags::None;
		targetOptions.Design.Category = L"State";
		targetOptions.Design.CategoryOrder = 70;
		targetOptions.Design.Order = 30;
		targetOptions.Design.Editor = DependencyPropertyEditorKind::Auto;
		targetOptions.Design.Persistence =
			DependencyPropertyPersistence::Transient;
		targetOptions.Design.Browsable = false;
		PlacementTargetPropertyKeyStorage().emplace(
			DependencyPropertyRegistry::RegisterReadOnly<
				ContextMenu, ControlWeakReference>(
				L"PlacementTarget",
				[](ContextMenu& target) { return target._placementTarget; },
				[](ContextMenu& target, const ControlWeakReference& value)
				{
					(void)target.SetReadOnlyPropertyField(
						L"PlacementTarget", target._placementTarget, value);
				},
				subscriber(L"PlacementTarget"), std::move(targetOptions)));
		return true;
	}();
	(void)registered;
}

void ContextMenu::SetIsOpen(bool value)
{
	(void)SetPropertyField(L"IsOpen", _isOpen, value);
}

void ContextMenu::SetStaysOpen(bool value)
{
	if (!SetPropertyField(L"StaysOpen", _staysOpen, value)) return;
	if (_isPresented && GetPresentationWindow())
	{
		TransientPresentationOptions options;
		options.DismissOnOutsidePointerDown = !_staysOpen;
		options.DismissOnWindowDeactivation = !_staysOpen;
		(void)cui::framework::WindowAccess::OpenTransientPresentation(
			*GetPresentationWindow(), this, options,
			[](Control& root)
			{ static_cast<ContextMenu&>(root).Hide(); });
	}
}

ContextMenu::ContextMenu()
	: ItemsControl()
{
	RegisterDependencyProperties();
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->RendererBorderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->RendererForegroundColor = PopupText;
	SuppressItemsPresentation();
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
			if (_isOpen) Hide();
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
	SuppressItemsPresentation();
	ClearHoverState();
	if (_isOpen) Hide();
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

void ContextMenu::SuppressItemsPresentation()
{
	// ContextMenu draws one transient popup projection. Its generator host is
	// still the logical owner of rows, but those rows cannot simultaneously be
	// flattened into the retained overlay scene.
	if (auto* host = GetItemsHost())
		cui::framework::TemplateAccess::SetParticipatesInPresentationScene(
			*host, false);
}

void ContextMenu::OnControlTemplatePresentationChanged()
{
	ItemsControl::OnControlTemplatePresentationChanged();
	SuppressItemsPresentation();
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
	else container->SetHeader(BindingValue(GetBindingRecordText(
		item, GetDisplayMemberPath())));
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
	if (previousWindow)
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*previousWindow, this);
	_isPresented = false;
	if (_placementTarget.HasValue())
	{
		auto* placementTarget = _placementTarget.Get();
		if (!placementTarget
			|| placementTarget->GetPresentationWindow() != currentWindow)
			(void)ClearReadOnlyPropertyValue(
				PlacementTargetPropertyKey(*this));
	}
	SynchronizeItemCommandHosts();
	if (_isOpen && currentWindow)
		PresentCore();
}

void ContextMenu::OnItemInteractionStateChanged(MenuItem& source)
{
	std::vector<int> path;
	if (!FindMenuItemPath(_items, &source, path) || path.empty()) return;
	if (!IsInteractive(&source))
	{
		ClearHoverState();
	}
	else if (_isOpen && source.IsSubmenuOpen
		&& !source.GetMenuItemsView().empty())
	{
		_openPath = path;
		_hoverPath = path;
	}
	else if (_isOpen)
	{
		const auto openDepth = path.size();
		if (_openPath.size() >= openDepth
			&& std::equal(path.begin(), path.end(), _openPath.begin()))
		{
			_openPath.resize(openDepth - 1);
			if (_hoverPath.size() > openDepth - 1)
				_hoverPath.resize(openDepth - 1);
		}
	}
	SynchronizeInteractionProjection();
	InvalidateVisual();
}

float ContextMenu::CalcPanelWidth(std::span<Control* const> items)
{
	float w = 120.0f;
	auto font = this->GetRenderFont();
	for (auto* entry : items)
	{
		auto* it = AsMenuItem(entry);
		if (!it) continue;
		auto ts = font->GetTextSize(it->GetDisplayText());
		float tw = ts.width + ItemHorizontalPadding * 2.0f + 42.0f;
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
	if (this->GetPresentationWindow())
	{
		float maxW = LogicalPopupExtent(this->GetPresentationWindow()).width - 8.0f;
		if (w > maxW) w = maxW;
	}
	return w;
}

std::vector<ContextMenu::PopupPanel> ContextMenu::BuildPanels()
{
	std::vector<PopupPanel> panels;
	if (!_isOpen || _items.empty())
		return panels;

	auto clampPanelXY = [&](float& x, float& y, float w, float h)
		{
			if (!this->GetPresentationWindow()) return;
			const auto extent = LogicalPopupExtent(this->GetPresentationWindow());
			float maxX = extent.width;
			float maxY = extent.height;
			if (x < 0.0f) x = 0.0f;
			if (y < 0.0f) y = 0.0f;
			if (x + w > maxX) x = (std::max)(0.0f, maxX - w);
			if (y + h > maxY) y = (std::max)(0.0f, maxY - h);
		};

	PopupPanel root;
	root.Items = _items;
	root.X = _anchor.x;
	root.Y = _anchor.y;
	root.W = CalcPanelWidth(root.Items);
	root.H = PopupVerticalPadding * 2.0f + (float)root.Items.size() * (float)PopupItemExtent;
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

		PopupPanel p;
		p.Items = owner->GetMenuItemsView();
		p.W = CalcPanelWidth(p.Items);
		p.H = PopupVerticalPadding * 2.0f + (float)p.Items.size() * (float)PopupItemExtent;
		p.X = prev.X + prev.W - 1.0f;
		p.Y = prev.Y + PopupVerticalPadding + (float)openIdx * (float)PopupItemExtent;
		if (this->GetPresentationWindow())
		{
			float maxX = LogicalPopupExtent(this->GetPresentationWindow()).width;
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

	return panels;
}

void ContextMenu::ClearHoverState()
{
	_hoverPath.clear();
	_openPath.clear();
	SynchronizeInteractionProjection();
}

void ContextMenu::SynchronizeInteractionProjection()
{
	std::unordered_set<MenuItem*> highlightedItems;
	std::unordered_set<MenuItem*> openedItems;
	if (_isOpen)
	{
		std::span<Control* const> current{ _items.data(), _items.size() };
		const size_t depth = (std::max)(_hoverPath.size(), _openPath.size());
		for (size_t level = 0; level < depth; ++level)
		{
			if (level < _hoverPath.size())
			{
				const int highlighted = _hoverPath[level];
				if (highlighted >= 0
					&& highlighted < static_cast<int>(current.size()))
					if (auto* item = AsMenuItem(current[highlighted]))
						highlightedItems.insert(item);
			}
			if (level >= _openPath.size()) break;
			const int opened = _openPath[level];
			if (opened < 0 || opened >= static_cast<int>(current.size())) break;
			auto* item = AsMenuItem(current[opened]);
			if (!item) break;
			openedItems.insert(item);
			current = item->GetMenuItemsView();
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

cui::core::Size ContextMenu::GetRenderSizeDip()
{
	if (!this->GetPresentationWindow())
		return {};
	return this->GetPresentationWindow()->GetContentViewportSizeDip();
}

bool ContextMenu::ContainsPoint(int localX, int localY)
{
	if (!_isOpen)
		return false;
	auto panels = BuildPanels();
	for (const auto& pn : panels)
	{
		if (localX >= pn.X && localX <= pn.X + pn.W && localY >= pn.Y && localY <= pn.Y + pn.H)
			return true;
	}
	return false;
}

void ContextMenu::OnRender()
{
	if (!this->IsVisible || !_isOpen || !this->GetPresentationWindow() || _items.empty())
		return;
	SynchronizeInteractionProjection();

	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	auto font = this->GetRenderFont();
	auto panels = BuildPanels();

	this->BeginRender(size.width, size.height);
	{
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
				PopupBorder, PopupBorderThickness, PopupCornerRadius);

			int hoverIdx = (level < _hoverPath.size() ? _hoverPath[level] : -1);
			int openIdx = (level < _openPath.size() ? _openPath[level] : -1);
			for (int i = 0; i < (int)pn.Items.size(); i++)
			{
				auto* entry = pn.Items[i];
				float iy = pn.Y + PopupVerticalPadding + (float)i * (float)PopupItemExtent;
				if (IsSeparator(entry))
				{
					float y = iy + (float)PopupItemExtent * 0.5f;
					d2d->DrawLine(pn.X + 12.0f, y,
						pn.X + pn.W - 12.0f, y, PopupSeparator, 1.0f);
					continue;
				}
				auto* it = AsMenuItem(entry);
				const bool itemEnabled = IsInteractive(it);
				if (itemEnabled && (i == hoverIdx || i == openIdx))
				{
					const float inset = (std::max)(0.0f, PopupItemHorizontalInset);
					auto itemRect = D2D1::RectF(pn.X + inset, iy + 2.0f, pn.X + pn.W - inset, iy + (float)PopupItemExtent - 2.0f);
					const auto hoverColor = PopupHighlight;
					d2d->FillRoundRect(itemRect, hoverColor, PopupItemCornerRadius);
					d2d->DrawRoundRect(itemRect, BoostAlpha(hoverColor, i == openIdx ? 2.1f : 1.7f), 1.0f, PopupItemCornerRadius);
					const float stripeH = (std::max)(0.0f, itemRect.bottom - itemRect.top - 8.0f);
					if (stripeH > 0.0f)
						d2d->FillRoundRect(itemRect.left + 4.0f, itemRect.top + 4.0f, 3.0f, stripeH, BoostAlpha(hoverColor, 3.0f), 1.5f);
				}
				if (!it || !it->IsVisible) continue;
				auto textColor = PopupText;
				if (!itemEnabled) textColor.a *= 0.45f;

				auto ts = font->GetTextSize(it->GetDisplayText());
				float ty = iy + ((float)PopupItemExtent - ts.height) * 0.5f;
				if (ty < iy) ty = iy;
				const float checkSlot = 18.0f;
				if (it->IsChecked)
				{
					auto checkSize = font->GetTextSize(L"\u2713");
					const float checkY = iy
						+ ((float)PopupItemExtent - checkSize.height) * 0.5f;
					d2d->DrawString(
						L"\u2713", pn.X + ItemHorizontalPadding, checkY,
						textColor, font);
				}
				d2d->DrawString(
					it->GetDisplayText(), pn.X + ItemHorizontalPadding + checkSlot, ty,
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
	this->EndRender();
}

bool ContextMenu::ProcessInput(const InputReport& input)
{
	const ControlWeakReference hostLifetime(this);
	ScopeExit projectOnExit{ [hostLifetime]
		{
			if (auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get()))
				host->SynchronizeInteractionProjection();
		} };
	if (!this->IsEffectivelyEnabled() || !this->IsVisible || !_isOpen)
		return true;

	if (_ignoreNextMouseUp && input.Kind == InputReportKind::PointerDown)
	{
		_ignoreNextMouseUp = false;
	}

	if (_ignoreNextMouseUp && input.Kind == InputReportKind::PointerUp)
	{
		_ignoreNextMouseUp = false;
		return true;
	}

	auto panels = BuildPanels();
	auto pointInRect = [&](float x, float y, const PopupPanel& pn) -> bool
		{
			return x >= pn.X && x <= pn.X + pn.W && y >= pn.Y && y <= pn.Y + pn.H;
		};
	auto ensureSize = [](std::vector<int>& values, size_t count)
		{
			if (values.size() < count) values.resize(count, -1);
		};

	int hitLevel = -1;
	for (int i = (int)panels.size() - 1; i >= 0; i--)
	{
		if (pointInRect((float)input.X, (float)input.Y, panels[i]))
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
		float bridgeL = (std::min)(a.X + a.W - 2.0f, b.X + 2.0f);
		float bridgeR = (std::max)(a.X + a.W - 2.0f, b.X + 2.0f);
		float bridgeT = b.Y;
		float bridgeB = b.Y + b.H;
		if ((float)input.X >= bridgeL && (float)input.X <= bridgeR
			&& (float)input.Y >= bridgeT && (float)input.Y <= bridgeB)
		{
			inBridge = true;
			break;
		}
	}

	if (input.Kind == InputReportKind::PointerMove)
	{
		auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
		if (!host || !host->GetPresentationWindow()) return true;
		if (hitLevel >= 0)
		{
			const auto& pn = panels[hitLevel];
			int itemIndex = (int)(((float)input.Y
				- (pn.Y + PopupVerticalPadding)) / (float)PopupItemExtent);
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
			if (IsInteractive(hovered)
				&& !hovered->GetMenuItemsView().empty())
				newOpen = itemIndex;

			if (_openPath[hitLevel] != newOpen)
			{
				_openPath[hitLevel] = newOpen;
				needsUpdate = true;
			}
			if (needsUpdate) InvalidateVisual();
		}
		else if (!inBridge)
		{
			if (!_hoverPath.empty() || !_openPath.empty())
			{
				ClearHoverState();
				InvalidateVisual();
			}
		}
		return true;
	}

	if (input.Kind == InputReportKind::PointerUp
		&& (input.ChangedButton == MouseButton::Left
			|| input.ChangedButton == MouseButton::Right))
	{
		if (hitLevel >= 0)
		{
			const auto& pn = panels[hitLevel];
			int itemIndex = (int)(((float)input.Y
				- (pn.Y + PopupVerticalPadding)) / (float)PopupItemExtent);
			int itemCount = static_cast<int>(pn.Items.size());
			if (itemIndex >= 0 && itemIndex < itemCount)
			{
				auto* item = AsMenuItem(pn.Items[itemIndex]);
				if (IsInteractive(item))
				{
					if (!item->GetMenuItemsView().empty())
					{
						ensureSize(_openPath, (size_t)hitLevel + 1);
						_openPath[hitLevel] = itemIndex;
						InvalidateVisual();
					}
					else
					{
						const bool staysOpen = item->StaysOpenOnClick;
						const bool invoked = item->Invoke();
						auto* host = dynamic_cast<ContextMenu*>(
							hostLifetime.Get());
						if (!host) return true;
						if (invoked && !staysOpen) host->Hide();
					}
				}
			}
		}
		return true;
	}

	return true;
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

void ContextMenu::ShowAtCore(
	Control* placementTarget,
	int x, int y,
	bool ignoreNextMouseUp)
{
	const ControlWeakReference hostLifetime(this);
	const ControlWeakReference placementLifetime(placementTarget);
	if (!this->GetPresentationWindow() || _items.empty())
		return;
	if (placementTarget
		&& placementTarget->GetPresentationWindow() != this->GetPresentationWindow())
		return;
	(void)TrySetReadOnlyPropertyValue(
		PlacementTargetPropertyKey(*this),
		BindingValue(ControlWeakReference(placementTarget)));
	auto* host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
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
	(void)host->TrySetCurrentPropertyValue(L"IsOpen", BindingValue(true));
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
		return;
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
	TransientPresentationOptions options;
	options.DismissOnOutsidePointerDown = !host->_staysOpen;
	options.DismissOnWindowDeactivation = !host->_staysOpen;
	const bool presented = cui::framework::WindowAccess::OpenTransientPresentation(
		*host->GetPresentationWindow(), host, options,
		[](Control& root)
		{
			static_cast<ContextMenu&>(root).Hide();
		});
	host = dynamic_cast<ContextMenu*>(hostLifetime.Get());
	if (!host || !host->GetPresentationWindow()) return;
	if (!presented)
	{
		host->Hide();
		return;
	}
	host->_isPresented = true;
	host->SynchronizeInteractionProjection();
	if (!wasPresented)
	{
		cui::framework::EventAccess::Raise(host->Opened, host);
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
	ClearHoverState();
	SynchronizeInteractionProjection();
	(void)ClearReadOnlyPropertyValue(
		PlacementTargetPropertyKey(*this));
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
	if (!host || host->_isOpen) return;
	if (host->GetPresentationWindow())
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*host->GetPresentationWindow(), host);
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
	const auto menuAbs = this->GetAbsoluteLocationDip();
	ShowAtCore(relativeTo,
		static_cast<int>(std::round(relativeAbs.x - menuAbs.x + (float)x)),
		static_cast<int>(std::round(relativeAbs.y - menuAbs.y + (float)y)),
		ignoreNextMouseUp);
}

void ContextMenu::Hide()
{
	if (!_isOpen && !_isPresented)
		return;
	(void)TrySetCurrentPropertyValue(L"IsOpen", BindingValue(false));
	// A failed current-value commit can only occur for invalid metadata. Keep
	// transient presentation coherent even in that defensive path.
	if (!_isOpen && _isPresented) DismissPresentationCore();
}
