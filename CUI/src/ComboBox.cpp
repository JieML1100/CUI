#include "ComboBox.h"
#include "DependencyPropertyInfrastructure.h"
#include "StyleInfrastructure.h"

#include "ItemsPresenter.h"
#include "Popup.h"
#include "ScrollViewer.h"
#include "TemplateInfrastructure.h"
#include "Window.h"
#include "XamlInfrastructure.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace
{
	const ItemsPanelTemplateReference& DefaultComboBoxItemsPanel()
	{
		static const auto definition = []
		{
			auto value = std::make_shared<ItemsPanelTemplate>();
			value->Kind = ItemsPanelKind::VirtualizingStack;
			value->Orientation = Orientation::Vertical;
			value->ItemHeight = 28.0f;
			value->CacheLength = 1.0f;
			return ItemsPanelTemplateReference(std::move(value));
		}();
		return definition;
	}

	template<typename TValue>
	DependencyPropertyOptions<ComboBox, TValue> ComboBoxOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DependencyPropertyEditorKind editor,
		DependencyPropertyFlags flags)
	{
		DependencyPropertyOptions<ComboBox, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto ComboBoxSubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			ComboBox& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName) handler();
				});
		};
	}

	bool Intersects(const D2D1_RECT_F& left, const D2D1_RECT_F& right) noexcept
	{
		return left.left < right.right && left.right > right.left
			&& left.top < right.bottom && left.bottom > right.top;
	}
}

ComboBoxItem::ComboBoxItem() = default;

void ComboBoxItem::RegisterDependencyProperties()
{
	ListBoxItem::RegisterDependencyProperties();
}

void ComboBoxItem::ActivateItem()
{
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (!owner) return;
	(void)owner->SelectItem(static_cast<int>(ItemIndex()));
	(void)owner->TrySetCurrentPropertyValue(
		L"IsDropDownOpen", BindingValue(false));
}

void ComboBoxItem::FocusOwner()
{
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (owner && owner->GetPresentationWindow())
		owner->GetPresentationWindow()->SetKeyboardFocus(owner, true);
}

void ComboBoxItem::OnIsSelectedRequested(bool value)
{
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (!owner) return;
	const int index = static_cast<int>(ItemIndex());
	if (value) (void)owner->SelectIndex(index);
	else if (owner->GetSelectedIndex() == index)
		(void)owner->SelectIndex(-1);
}

void ComboBox::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
	static const bool registered = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<ComboBox, std::wstring> textOptions;
		textOptions.DefaultValue = std::wstring{};
		textOptions.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		textOptions.Design.Category = L"Common";
		textOptions.Design.CategoryOrder = 0;
		textOptions.Design.Order = 10;
		textOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		textOptions.Design.Persistence =
			DependencyPropertyPersistence::Native;
		DependencyPropertyRegistry::Register<ComboBox, std::wstring>(L"Text",
			[](ComboBox& target) { return target.Text; },
			[](ComboBox& target, const std::wstring& value)
			{ target.Text = value; },
			[](ComboBox& target, Handler handler, DataSourceUpdateMode mode)
			{
				if (mode == DataSourceUpdateMode::OnValidation)
					return target.OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*) { handler(); });
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.PropertyName == L"Text")
							handler();
					});
			}, std::move(textOptions));

		auto openOptions = ComboBoxOptions(
			false, L"Behavior", 110, 10,
			DependencyPropertyEditorKind::Boolean,
			DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		openOptions.Changed = [](
			ComboBox& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsDropDownOpenChange(oldValue, newValue);
		};
		DependencyPropertyRegistry::Register<ComboBox, bool>(L"IsDropDownOpen",
			[](ComboBox& target) { return target.GetIsDropDownOpen(); },
			[](ComboBox& target, const bool& value)
			{ target.SetIsDropDownOpen(value); },
			ComboBoxSubscriber(L"IsDropDownOpen"), std::move(openOptions));

		auto heightOptions = ComboBoxOptions(
			320.0f, L"Layout", 100, 10,
			DependencyPropertyEditorKind::Number,
			DependencyPropertyFlags::AffectsArrange);
		heightOptions.Coerce = [](
			ComboBox&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		heightOptions.Changed = [](
			ComboBox& target, const float&, const float&)
		{
			target.ApplyMaxDropDownHeight();
		};
		heightOptions.Design.Minimum = 0.0;
		heightOptions.Design.Step = 1.0;
		DependencyPropertyRegistry::Register<ComboBox, float>(
			L"MaxDropDownHeight",
			[](ComboBox& target) { return target.GetMaxDropDownHeight(); },
			[](ComboBox& target, const float& value)
			{ target.SetMaxDropDownHeight(value); },
			ComboBoxSubscriber(L"MaxDropDownHeight"),
			std::move(heightOptions));

		RegisterControlBorderThicknessMetadata<ComboBox>(1.0f, 30);
		return true;
	}();
	(void)registered;
}

GET_CPP(ComboBox, std::wstring, Text) { return Control::GetText(); }
SET_CPP(ComboBox, std::wstring, Text)
{
	Control::SetText(std::move(value));
}

ComboBox::ComboBox()
	: Selector()
{
	RegisterDependencyProperties();
	InitializeControlBorderThicknessDefault(1.0f);
	RendererBackgroundColor = cui::theme::palette::Surface;
	RendererBorderColor = cui::theme::palette::BorderStrong;
	RendererForegroundColor = cui::theme::palette::TextPrimary;
	(void)TrySetPropertyValue(
		L"Cursor", BindingValue(CursorKind::Hand),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"ItemsPanel", BindingValue(DefaultComboBoxItemsPanel()),
		DependencyPropertyValueSource::Theme);
	if (auto* host = GetItemsHost())
		cui::framework::TemplateAccess::SetPresentationSuppressed(*host, true);
	RefreshItems();
}

ComboBox::~ComboBox()
{
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	if (_popup)
		(void)_popup->TrySetCurrentPropertyValue(
			L"IsOpen", BindingValue(false));
}

void ComboBox::SetIsDropDownOpen(bool value)
{
	(void)SetPropertyField(L"IsDropDownOpen", _isDropDownOpen, value);
}

void ComboBox::SetMaxDropDownHeight(float value)
{
	(void)SetPropertyField(
		L"MaxDropDownHeight", _maxDropDownHeight, value);
}

void ComboBox::ApplyIsDropDownOpenChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (newValue)
	{
		if (EnsureDropDownInfrastructure() && _popup)
		{
			UpdateItemsHostPresentation();
			ApplyMaxDropDownHeight();
			(void)_popup->TrySetCurrentPropertyValue(
				L"IsOpen", BindingValue(true));
			_popup->UpdatePlacement();
			if (SelectedIndex >= 0)
				(void)BringItemIntoView(static_cast<size_t>(SelectedIndex));
		}
	}
	else if (_popup)
	{
		(void)_popup->TrySetCurrentPropertyValue(
			L"IsOpen", BindingValue(false));
	}
	NotifyAccessibilityStateChanged();
	InvalidateVisual();
}

void ComboBox::ApplyMaxDropDownHeight()
{
	if (!_popup) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, L"MaxHeight", BindingValue(_maxDropDownHeight),
		DependencyPropertyValueSource::Template);
	// Popup constrains the transient surface, while ScrollViewer owns the
	// viewport/extent contract. Constrain both so layout and automation observe
	// the same drop-down viewport.
	if (_dropDownScroll)
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*_dropDownScroll, L"MaxHeight", BindingValue(_maxDropDownHeight),
			DependencyPropertyValueSource::Template);
	if (_popup->GetIsOpen()) _popup->UpdatePlacement();
}

Popup* ComboBox::ResolvePopupPart() const noexcept
{
	auto* presenter = GetTemplateItemsPresenter();
	for (auto* current = presenter ? presenter->GetVisualParent() : nullptr;
		current && current != this; current = current->GetVisualParent())
		if (auto* popup = dynamic_cast<Popup*>(current)) return popup;
	return dynamic_cast<Popup*>(GetControlTemplateRoot());
}

ScrollViewer* ComboBox::ResolveScrollOwner() const noexcept
{
	auto* presenter = GetTemplateItemsPresenter();
	for (auto* current = presenter ? presenter->GetVisualParent() : nullptr;
		current && current != this; current = current->GetVisualParent())
	{
		if (current == _popup) break;
		if (auto* scroll = dynamic_cast<ScrollViewer*>(current)) return scroll;
	}
	return nullptr;
}

void ComboBox::ConfigurePopupPart(Popup* popup)
{
	if (_popup == popup)
	{
		_dropDownScroll = ResolveScrollOwner();
		ApplyMaxDropDownHeight();
		return;
	}
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	if (_popup && _popup != popup)
		(void)_popup->TrySetCurrentPropertyValue(
			L"IsOpen", BindingValue(false));
	_popup = popup;
	_dropDownScroll = nullptr;
	if (!_popup) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, L"PlacementTarget", BindingValue(ControlWeakReference(this)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, L"Placement", BindingValue(PlacementMode::Bottom),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, L"StaysOpen", BindingValue(false),
		DependencyPropertyValueSource::Template);
	_popupOpened = _popup->Opened.Subscribe([this](Popup*)
	{
		if (!_isDropDownOpen)
			(void)SetCurrentPropertyField(
				L"IsDropDownOpen", _isDropDownOpen, true);
	});
	_popupClosed = _popup->Closed.Subscribe([this](Popup*)
	{
		if (_isDropDownOpen)
			(void)SetCurrentPropertyField(
				L"IsDropDownOpen", _isDropDownOpen, false);
	});
	_dropDownScroll = ResolveScrollOwner();
	ApplyMaxDropDownHeight();
}

bool ComboBox::EnsureDropDownInfrastructure()
{
	if (auto* resolved = ResolvePopupPart())
	{
		ConfigurePopupPart(resolved);
		_dropDownScroll = ResolveScrollOwner();
		UpdateItemsHostPresentation();
		return true;
	}
	if (GetControlTemplateRoot())
	{
		SetLastTemplateError(
			L"ComboBox ControlTemplate 必须包含承载 ItemsPresenter 的 Popup。");
		UpdateItemsHostPresentation();
		return false;
	}
	if (!GetPresentationWindow()) return false;

	auto popup = std::make_unique<Popup>();
	auto* popupRaw = popup.get();
	cui::framework::XamlAccess::SetTemplatedParent(*popupRaw, this);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*popupRaw, L"PlacementTarget", BindingValue(ControlWeakReference(this)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*popupRaw, L"Placement", BindingValue(PlacementMode::Bottom),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*popupRaw, L"StaysOpen", BindingValue(false),
		DependencyPropertyValueSource::Template);

	auto scroll = std::make_unique<ScrollViewer>();
	auto* scrollRaw = scroll.get();
	cui::framework::XamlAccess::SetTemplatedParent(*scrollRaw, this);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, L"VerticalAlignment", BindingValue(VerticalAlignment::Top),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, L"Background",
		BindingValue(cui::drawing::MakeSolidColorBrush(
			cui::theme::palette::Surface)),
		DependencyPropertyValueSource::Theme);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, L"BorderBrush",
		BindingValue(cui::drawing::MakeSolidColorBrush(
			cui::theme::palette::Border)),
		DependencyPropertyValueSource::Theme);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, L"BorderThickness", BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, L"HorizontalScrollBarVisibility",
		BindingValue(ScrollBarVisibility::Disabled),
		DependencyPropertyValueSource::Template);

	auto presenter = std::make_unique<ItemsPresenter>();
	auto* presenterRaw = presenter.get();
	cui::framework::XamlAccess::SetTemplatedParent(*presenterRaw, this);
	scrollRaw->SetVisualContent(std::move(presenter));
	popupRaw->SetChild(std::move(scroll));

	_defaultPopup = popupRaw;
	_buildingDropDownInfrastructure = true;
	try
	{
		cui::framework::TemplateAccess::SetTemplateRoot(*this, std::move(popup));
		_buildingDropDownInfrastructure = false;
	}
	catch (...)
	{
		_buildingDropDownInfrastructure = false;
		throw;
	}
	if (!cui::framework::TemplateAccess::RegisterItemsPresenter(
		*this, presenterRaw))
		throw std::logic_error(
			"ComboBox fallback ItemsPresenter registration failed");
	ConfigurePopupPart(popupRaw);
	_dropDownScroll = scrollRaw;
	SetLastTemplateError({});
	UpdateItemsHostPresentation();
	return true;
}

void ComboBox::UpdateItemsHostPresentation()
{
	auto* host = GetItemsHost();
	if (!host) return;
	bool inPopup = false;
	if (_popup)
	{
		for (auto* current = host->GetVisualParent(); current;
			current = current->GetVisualParent())
		{
			if (current == _popup)
			{
				inPopup = true;
				break;
			}
			if (current == this) break;
		}
	}
	cui::framework::TemplateAccess::SetPresentationSuppressed(
		*host, !inPopup);
}

void ComboBox::OnControlTemplatePresentationChanged()
{
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	if (_popup)
		(void)_popup->TrySetCurrentPropertyValue(
			L"IsOpen", BindingValue(false));
	_popup = nullptr;
	_dropDownScroll = nullptr;
	if (GetControlTemplateRoot() != _defaultPopup)
		_defaultPopup = nullptr;
	ConfigurePopupPart(ResolvePopupPart());
	UpdateItemsHostPresentation();
	if (_popup && !_buildingDropDownInfrastructure)
		(void)_popup->TrySetCurrentPropertyValue(
			L"IsOpen", BindingValue(_isDropDownOpen));
}

void ComboBox::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	Selector::OnPresentationWindowChanged(previousWindow, currentWindow);
	if (_isDropDownOpen && currentWindow
		&& EnsureDropDownInfrastructure() && _popup)
		(void)_popup->TrySetCurrentPropertyValue(
			L"IsOpen", BindingValue(true));
}

std::unique_ptr<Control> ComboBox::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	observation = {};
	std::unique_ptr<ComboBoxItem> container;
	const auto containerTemplate = GetItemContainerTemplate();
	if (containerTemplate)
	{
		if (containerTemplate.Get()->TargetType()
			!= UIClass::UI_ComboBoxItem)
		{
			SetLastTemplateError(
				L"ItemContainerTemplate TargetType 必须是 ComboBoxItem。");
			return {};
		}
		std::wstring error;
		auto built = containerTemplate.Get()->Build(&error);
		auto* itemContainer = dynamic_cast<ComboBoxItem*>(built.get());
		if (!itemContainer)
		{
			SetLastTemplateError(error.empty()
				? L"ItemContainerTemplate 未生成 ComboBoxItem。"
				: std::move(error));
			return {};
		}
		container.reset(static_cast<ComboBoxItem*>(built.release()));
	}
	else container = std::make_unique<ComboBoxItem>();

	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	std::wstring error;
	if (!container->InitializeItem(
		item, GetItemTemplate(), GetDisplayMemberPath(),
		index, L"ComboBoxItem", &error))
	{
		SetLastTemplateError(error.empty()
			? L"ComboBoxItem 内容初始化失败。" : std::move(error));
		return {};
	}
	return container;
}

void ComboBox::OnGeneratedItemsRebuilt()
{
	Selector::OnGeneratedItemsRebuilt();
	RefreshItems();
}

void ComboBox::OnGeneratedItemsRealized()
{
	Selector::OnGeneratedItemsRealized();
	UpdateGeneratedItemStates();
}

void ComboBox::OnGeneratedItemIndexChanged(
	Control& visual, size_t oldIndex, size_t newIndex)
{
	Selector::OnGeneratedItemIndexChanged(visual, oldIndex, newIndex);
	if (auto* item = dynamic_cast<ComboBoxItem*>(&visual))
		item->SetItemIndex(newIndex);
}

bool ComboBox::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const ComboBoxItem*>(&item)) return true;
	error = "ComboBox authored Items must be ComboBoxItem controls";
	return false;
}

void ComboBox::OnAuthoredItemsChanged() noexcept
{
	Selector::OnAuthoredItemsChanged();
	try { RefreshItems(); }
	catch (...) {}
}

void ComboBox::OnItemsSourceChanged(
	const BindingListReference& oldValue,
	const BindingListReference& newValue)
{
	Selector::OnItemsSourceChanged(oldValue, newValue);
	RefreshItems();
}

std::wstring ComboBox::GetAuthoredItemText(size_t index) const
{
	auto* item = dynamic_cast<ComboBoxItem*>(GetAuthoredItem(index));
	if (!item) return {};
	std::wstring text;
	if (item->GetContent().TryGet(text)) return text;
	if (auto* content = item->GetVisualContent())
	{
		const auto displayText = content->GetDisplayText();
		if (!displayText.empty()) return displayText;
	}
	const auto displayText = item->GetDisplayText();
	if (!displayText.empty()) return displayText;
	return item->GetContent().ToString();
}

std::wstring ComboBox::GetItemDisplayText(size_t index) const
{
	const auto source = GetItemsView();
	if (!source) return index < AuthoredItemCount()
		? GetAuthoredItemText(index) : std::wstring{};
	if (index >= source.Get()->Count()) return {};
	BindingSourceReference item;
	if (!source.Get()->TryGetItem(index, item) || !item) return {};
	return GetBindingRecordText(
		item, GetDisplayMemberPath());
}

void ComboBox::RefreshItems()
{
	_itemSourceObservations.clear();
	_authoredItemChanges.clear();
	const auto source = GetItemsView();
	if (source)
	{
		_itemSourceObservations.reserve(source.Get()->Count());
		for (size_t index = 0; index < source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			(void)source.Get()->TryGetItem(index, item);
			_itemSourceObservations.push_back(ObserveBindingPaths(
				item, { GetDisplayMemberPath(), GetSelectedValuePath() },
				[this, index] { RefreshDataItem(index); }));
		}
	}
	else
	{
		_authoredItemChanges.reserve(AuthoredItemCount());
		for (size_t index = 0; index < AuthoredItemCount(); ++index)
		{
			auto* item = dynamic_cast<ComboBoxItem*>(GetAuthoredItem(index));
			if (!item) continue;
			item->SetItemIndex(index);
			if (cui::framework::StyleAccess::ResourceKey(*item).empty()
				&& !GetItemContainerStyle().empty())
				cui::framework::StyleAccess::SetResourceKey(
					*item, GetItemContainerStyle());
			_authoredItemChanges.push_back(
				item->OnPropertyValueChanged.Subscribe(
					[this, index](DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.PropertyName == L"Content"
							|| args.PropertyName == L"Text")
							RefreshDataItem(index);
					}));
		}
	}
	ReconcileAccessibilityItemIds();
	SyncTextWithSelection();
	UpdateGeneratedItemStates();
	_selectedAccessibilityItemId = SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < _accessibilityItemIds.size()
		? _accessibilityItemIds[static_cast<size_t>(SelectedIndex)] : 0;
	UpdateItemsHostPresentation();
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
}

void ComboBox::RefreshDataItem(size_t index)
{
	const auto source = GetItemsView();
	if (source)
	{
		if (index >= source.Get()->Count()
			|| index >= _itemSourceObservations.size())
		{
			RefreshItems();
			return;
		}
		BindingSourceReference item;
		(void)source.Get()->TryGetItem(index, item);
		_itemSourceObservations[index] = ObserveBindingPaths(
			item, { GetDisplayMemberPath(), GetSelectedValuePath() },
			[this, index] { RefreshDataItem(index); });
	}
	if (static_cast<int>(index) == SelectedIndex)
		SyncTextWithSelection();
	if (index < _accessibilityItemIds.size())
		NotifyAccessibilityVirtualChanged(
			_accessibilityItemIds[index], AccessibilityChange::Name);
	InvalidateVisual();
}

void ComboBox::SyncTextWithSelection()
{
	Text = SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < ItemCount()
		? GetItemDisplayText(static_cast<size_t>(SelectedIndex)) : std::wstring{};
}

void ComboBox::UpdateGeneratedItemStates()
{
	UpdateContainerSelection();
}

void ComboBox::OnSelectedIndexChanged(int oldValue, int newValue)
{
	if (oldValue == newValue) return;
	SyncTextWithSelection();
	if (_accessibilityItemIds.size() != ItemCount())
		ReconcileAccessibilityItemIds();
	_selectedAccessibilityItemId = newValue >= 0
		&& static_cast<size_t>(newValue) < _accessibilityItemIds.size()
		? _accessibilityItemIds[static_cast<size_t>(newValue)] : 0;
	UpdateGeneratedItemStates();
	if (_isDropDownOpen && newValue >= 0)
		(void)BringItemIntoView(static_cast<size_t>(newValue));
}

bool ComboBox::SelectItem(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= ItemCount()) return false;
	if (SelectedIndex == index)
	{
		if (_isDropDownOpen)
			(void)BringItemIntoView(static_cast<size_t>(index));
		return true;
	}
	return SelectIndex(index);
}

ComboBoxItem* ComboBox::AddItem(std::unique_ptr<ComboBoxItem> item)
{
	return static_cast<ComboBoxItem*>(AddItemControl(std::move(item)));
}

ComboBoxItem* ComboBox::InsertItem(
	int index, std::unique_ptr<ComboBoxItem> item)
{
	if (index < 0 || static_cast<size_t>(index) > AuthoredItemCount())
		throw std::out_of_range("ComboBox item index is out of range");
	return static_cast<ComboBoxItem*>(InsertItemControl(
		static_cast<size_t>(index), std::move(item)));
}

ComboBoxItem* ComboBox::GetItem(int index) const noexcept
{
	return index < 0 ? nullptr : dynamic_cast<ComboBoxItem*>(
		GetAuthoredItem(static_cast<size_t>(index)));
}

int ComboBox::IndexOfItem(const ComboBoxItem* item) const noexcept
{
	if (!item) return -1;
	for (size_t index = 0; index < AuthoredItemCount(); ++index)
		if (GetAuthoredItem(index) == item) return static_cast<int>(index);
	return -1;
}

std::unique_ptr<ComboBoxItem> ComboBox::DetachItemAt(int index)
{
	if (index < 0) return {};
	auto owner = DetachItemControlAt(static_cast<size_t>(index));
	if (!owner) return {};
	auto* item = static_cast<ComboBoxItem*>(owner.release());
	return std::unique_ptr<ComboBoxItem>(item);
}

std::unique_ptr<ComboBoxItem> ComboBox::DetachItem(ComboBoxItem* item)
{
	return DetachItemAt(IndexOfItem(item));
}

bool ComboBox::RemoveItemAt(int index)
{
	return static_cast<bool>(DetachItemAt(index));
}

bool ComboBox::RemoveItem(ComboBoxItem* item)
{
	return static_cast<bool>(DetachItem(item));
}

void ComboBox::ClearItems()
{
	ClearItemControls();
}

CursorKind ComboBox::QueryCursor(int, int)
{
	return IsEffectivelyEnabled() ? CursorKind::Hand : CursorKind::Arrow;
}

bool ComboBox::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Return:
	case Key::Space:
	case Key::Escape:
	case Key::F4:
		return true;
	default:
		return Selector::HandlesNavigationKey(key);
	}
}

cui::core::Size ComboBox::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot()
		&& GetControlTemplateRoot() != _defaultPopup)
		return ItemsControl::MeasureCore(available);
	const auto padding = GetSpecifiedLayout().padding;
	cui::core::Size textSize{};
	if (GetRenderFont())
	{
		const auto measured = GetRenderFont()->GetTextSize(Text);
		textSize = { measured.width, measured.height };
	}
	return available.Constrain({
		textSize.width + padding.Horizontal() + 28.0f,
		(std::max)(24.0f, textSize.height + padding.Vertical()) });
}

void ComboBox::Arrange(cui::core::Rect finalRect)
{
	if (GetControlTemplateRoot()
		&& GetControlTemplateRoot() != _defaultPopup)
		ItemsControl::Arrange(finalRect);
	else
		Control::Arrange(finalRect);
	if (_isDropDownOpen && EnsureDropDownInfrastructure() && _popup)
		_popup->UpdatePlacement();
}

void ComboBox::PreparePresentation()
{
	Selector::PreparePresentation();
	if (_isDropDownOpen) (void)EnsureDropDownInfrastructure();
	else ConfigurePopupPart(ResolvePopupPart());
	_dropDownScroll = ResolveScrollOwner();
	UpdateItemsHostPresentation();
	if (_popup && _popup->GetIsOpen()) _popup->UpdatePlacement();
}

void ComboBox::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	if (GetControlTemplateRoot()
		&& GetControlTemplateRoot() != _defaultPopup)
		return;

	auto* d2d = GetDrawingContext();
	const auto size = GetActualSizeDip();
	const float border = BorderThickness.MaxEdge();
	BeginRender();
	{
		d2d->FillRect(0.0f, 0.0f, size.width, size.height, RendererBackgroundColor);
		if (border > 0.0f && RendererBorderColor.a > 0.0f)
			d2d->DrawRect(border * 0.5f, border * 0.5f,
				(std::max)(0.0f, size.width - border),
				(std::max)(0.0f, size.height - border),
				RendererBorderColor, border);
		if (GetRenderFont())
		{
			const auto textSize = GetRenderFont()->GetTextSize(Text);
			const float left = Padding.Left;
			const float top = (std::max)(Padding.Top,
				(size.height - textSize.height) * 0.5f);
			d2d->DrawString(Text, left, top,
				(std::max)(1.0f, size.width - left - Padding.Right - 24.0f),
				textSize.height + 2.0f, RendererForegroundColor, GetRenderFont());
		}
		const float cx = size.width - 13.0f;
		const float cy = size.height * 0.5f;
		const float direction = _isDropDownOpen ? -1.0f : 1.0f;
		d2d->DrawLine(
			D2D1::Point2F(cx - 4.0f, cy - 2.0f * direction),
			D2D1::Point2F(cx, cy + 2.0f * direction), RendererForegroundColor, 1.5f);
		d2d->DrawLine(
			D2D1::Point2F(cx, cy + 2.0f * direction),
			D2D1::Point2F(cx + 4.0f, cy - 2.0f * direction), RendererForegroundColor, 1.5f);
		if (!IsEffectivelyEnabled())
			d2d->FillRect(0.0f, 0.0f, size.width, size.height,
				cui::theme::palette::DisabledOverlay);
	}
	EndRender();
}

bool ComboBox::ProcessInput(const InputReport& input)
{
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		_pointerPressActive = true;
		(void)CaptureMouse();
		if (GetPresentationWindow())
			GetPresentationWindow()->SetKeyboardFocus(this, true);
	}

	if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left)
	{
		const bool activate = _pointerPressActive
			&& ContainsPoint(input.X, input.Y);
		_pointerPressActive = false;
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		if (activate) SetCurrentIsDropDownOpen(!_isDropDownOpen);
		return Selector::ProcessInput(input);
	}
	if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
	{
		_pointerPressActive = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Selector::ProcessInput(input);
	}

	if (input.Kind == InputReportKind::KeyDown)
	{
		bool handled = true;
		switch (input.Key)
		{
		case Key::F4:
		case Key::Return:
		case Key::Space:
			SetCurrentIsDropDownOpen(!_isDropDownOpen);
			break;
		case Key::Escape:
			if (_isDropDownOpen) SetCurrentIsDropDownOpen(false);
			else handled = false;
			break;
		case Key::Down:
			if (input.HasModifier(ModifierKeys::Alt))
				SetCurrentIsDropDownOpen(true);
			else handled = false;
			break;
		case Key::Up:
			if (input.HasModifier(ModifierKeys::Alt))
				SetCurrentIsDropDownOpen(false);
			else handled = false;
			break;
		default:
			handled = false;
			break;
		}
		if (handled)
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyDown(this, args);
			return true;
		}
	}
	return Selector::ProcessInput(input);
}

void ComboBox::ReconcileAccessibilityItemIds()
{
	const size_t count = ItemCount();
	std::vector<uint32_t> nextIds(count, 0);
	std::vector<BindingSourceReference> nextSources(count);
	std::vector<ControlWeakReference> nextAuthored(count);
	const auto source = GetItemsView();
	struct ReusableSourceIds final
	{
		std::vector<size_t> Indices;
		size_t Next = 0;
	};
	std::unordered_map<IBindingSource*, ReusableSourceIds> reusableSourceIds;
	std::vector<bool> usedAuthoredIds;
	if (source)
	{
		reusableSourceIds.reserve(_accessibilitySourceIdentities.size());
		for (size_t old = 0;
			old < _accessibilityItemIds.size()
				&& old < _accessibilitySourceIdentities.size(); ++old)
		{
			reusableSourceIds[_accessibilitySourceIdentities[old].Get()]
				.Indices.push_back(old);
		}
	}
	else
	{
		usedAuthoredIds.resize(_accessibilityItemIds.size(), false);
	}
	for (size_t index = 0; index < count; ++index)
	{
		if (source)
		{
			(void)source.Get()->TryGetItem(index, nextSources[index]);
			const auto reusable = reusableSourceIds.find(
				nextSources[index].Get());
			if (reusable != reusableSourceIds.end()
				&& reusable->second.Next < reusable->second.Indices.size())
			{
				nextIds[index] = _accessibilityItemIds[
					reusable->second.Indices[reusable->second.Next++]];
			}
		}
		else
		{
			nextAuthored[index] = GetAuthoredItem(index);
			for (size_t old = 0; old < _accessibilityItemIds.size(); ++old)
			{
				if (usedAuthoredIds[old]
					|| old >= _accessibilityAuthoredIdentities.size()
					|| _accessibilityAuthoredIdentities[old]
						!= nextAuthored[index]) continue;
				nextIds[index] = _accessibilityItemIds[old];
				usedAuthoredIds[old] = true;
				break;
			}
		}
		if (nextIds[index] == 0)
			nextIds[index] = AllocateAccessibilityVirtualId();
	}
	_accessibilityItemIds = std::move(nextIds);
	_accessibilitySourceIdentities = std::move(nextSources);
	_accessibilityAuthoredIdentities = std::move(nextAuthored);
	RebuildAccessibilityItemIndex();
}

void ComboBox::RebuildAccessibilityItemIndex()
{
	_accessibilityItemIndexById.clear();
	for (size_t index = 0; index < _accessibilityItemIds.size(); ++index)
	{
		auto& id = _accessibilityItemIds[index];
		while (id == 0
			|| !_accessibilityItemIndexById.emplace(id, index).second)
			id = AllocateAccessibilityVirtualId();
	}
}

int ComboBox::FindAccessibilityItem(uint32_t id)
{
	if (id == 0) return -1;
	if (_accessibilityItemIds.size() != ItemCount()
		|| _accessibilityItemIndexById.size() != _accessibilityItemIds.size())
		ReconcileAccessibilityItemIds();
	const auto found = _accessibilityItemIndexById.find(id);
	return found == _accessibilityItemIndexById.end()
		? -1 : static_cast<int>(found->second);
}

bool ComboBox::TryGetItemBounds(
	size_t index, D2D1_RECT_F& bounds, bool& visible) const noexcept
{
	bounds = D2D1::RectF();
	visible = false;
	if (!_popup || !_popup->GetIsOpen()) return false;
	auto* item = GetGeneratedItem(index);
	if (!item) return true;
	const auto itemRect = item->GetAbsoluteRectDip();
	const auto owner = GetAbsoluteLocationDip();
	bounds = D2D1::RectF(
		itemRect.x - owner.x,
		itemRect.y - owner.y,
		itemRect.x - owner.x + itemRect.width,
		itemRect.y - owner.y + itemRect.height);
	const auto popupRect = _popup->GetAbsoluteRectDip();
	const D2D1_RECT_F itemAbsolute = D2D1::RectF(
		itemRect.x, itemRect.y,
		itemRect.x + itemRect.width, itemRect.y + itemRect.height);
	const D2D1_RECT_F popupAbsolute = D2D1::RectF(
		popupRect.x, popupRect.y,
		popupRect.x + popupRect.width, popupRect.y + popupRect.height);
	visible = item->IsVisible && Intersects(itemAbsolute, popupAbsolute);
	if (!visible) bounds = D2D1::RectF();
	return true;
}

size_t ComboBox::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	return parentId == 0 ? ItemCount() : 0;
}

bool ComboBox::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	if (parentId != 0 || index >= ItemCount()) return false;
	if (_accessibilityItemIds.size() != ItemCount())
		ReconcileAccessibilityItemIds();
	result = _accessibilityItemIds[index];
	return result != 0;
}

bool ComboBox::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	result = 0;
	if (parentId != 0) return false;
	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	const int sibling = next ? index + 1 : index - 1;
	if (sibling < 0
		|| sibling >= static_cast<int>(_accessibilityItemIds.size()))
		return false;
	result = _accessibilityItemIds[static_cast<size_t>(sibling)];
	return result != 0;
}

bool ComboBox::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	if (!_popup || !_popup->GetIsOpen()) return false;
	const size_t count = ItemCount();
	for (size_t index = 0; index < count; ++index)
	{
		D2D1_RECT_F bounds{};
		bool visible = false;
		if (!TryGetItemBounds(index, bounds, visible) || !visible) continue;
		if (localX < bounds.left || localX > bounds.right
			|| localY < bounds.top || localY > bounds.bottom) continue;
		return TryGetAccessibilityVirtualChildAt(0, index, result);
	}
	return false;
}

bool ComboBox::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	D2D1_RECT_F bounds{};
	bool visible = false;
	(void)TryGetItemBounds(static_cast<size_t>(index), bounds, visible);
	result = {};
	result.Id = id;
	result.ControlType = AutomationControlType::ListItem;
	result.Patterns = AutomationPattern::SelectionItem
		| AutomationPattern::ScrollItem
		| AutomationPattern::VirtualizedItem;
	result.Name = GetItemDisplayText(static_cast<size_t>(index));
	result.Value = result.Name;
	const auto ownerId = GetAccessibilitySnapshot().AutomationId;
	result.AutomationId = ownerId.empty()
		? L"item-" + std::to_wstring(id)
		: ownerId + L".item-" + std::to_wstring(id);
	result.BoundsDip = bounds;
	result.Enabled = IsEffectivelyEnabled();
	result.Visible = IsVisible && visible;
	result.Selected = index == SelectedIndex;
	result.Row = index;
	result.Column = 0;
	return true;
}

AccessibilityVirtualContainerInfo
ComboBox::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AutomationPattern::Selection
		| AutomationPattern::Scroll;
	result.CanSelectMultiple = false;
	result.IsSelectionRequired = ItemCount() != 0;
	result.RowCount = static_cast<int>(ItemCount());
	result.ColumnCount = 1;
	return result;
}

void ComboBox::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
	if (_accessibilityItemIds.size() != ItemCount())
		ReconcileAccessibilityItemIds();
	if (SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < _accessibilityItemIds.size())
		result.push_back(
			_accessibilityItemIds[static_cast<size_t>(SelectedIndex)]);
}

bool ComboBox::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	if (action == AccessibilitySelectionAction::Remove) return false;
	const int index = FindAccessibilityItem(id);
	const bool selected = IsEffectivelyEnabled()
		&& index >= 0 && SelectItem(index);
	if (selected)
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
	return selected;
}

bool ComboBox::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0 || !IsEffectivelyEnabled()) return false;
	SetCurrentIsDropDownOpen(true);
	if (!EnsureDropDownInfrastructure()) return false;
	if (_popup) _popup->UpdatePlacement();
	return BringItemIntoView(static_cast<size_t>(index));
}

bool ComboBox::GetScrollMetrics(
	float& extent, float& viewport, float& offset) const noexcept
{
	extent = viewport = offset = 0.0f;
	if (!_dropDownScroll) return false;
	try
	{
		auto* scroll = const_cast<ScrollViewer*>(_dropDownScroll);
		scroll->UpdateLayout();
		extent = static_cast<float>(scroll->ExtentHeight);
		viewport = static_cast<float>(scroll->ViewportHeight);
		offset = static_cast<float>(scroll->VerticalOffset);
		return extent > viewport && viewport > 0.0f;
	}
	catch (...)
	{
		return false;
	}
}

bool ComboBox::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	float extent = 0.0f;
	float viewport = 0.0f;
	float offset = 0.0f;
	if (!GetScrollMetrics(extent, viewport, offset)) return true;
	const float maximum = (std::max)(0.0f, extent - viewport);
	result.VerticallyScrollable = maximum > 0.0f;
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent = (std::clamp)(
			static_cast<double>(offset / maximum * 100.0f), 0.0, 100.0);
		result.VerticalViewSize = (std::clamp)(
			static_cast<double>(viewport / extent * 100.0f), 0.0, 100.0);
	}
	return true;
}

bool ComboBox::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	if (horizontal != AccessibilityScrollAmount::NoAmount) return false;
	if (vertical == AccessibilityScrollAmount::NoAmount) return true;
	SetCurrentIsDropDownOpen(true);
	if (!EnsureDropDownInfrastructure() || !_dropDownScroll) return false;
	float extent = 0.0f;
	float viewport = 0.0f;
	float offset = 0.0f;
	if (!GetScrollMetrics(extent, viewport, offset)) return false;
	const int line = 48;
	const int page = (std::max)(line,
		static_cast<int>(std::floor(viewport)) - line);
	int delta = 0;
	switch (vertical)
	{
	case AccessibilityScrollAmount::LargeDecrement: delta = -page; break;
	case AccessibilityScrollAmount::SmallDecrement: delta = -line; break;
	case AccessibilityScrollAmount::LargeIncrement: delta = page; break;
	case AccessibilityScrollAmount::SmallIncrement: delta = line; break;
	case AccessibilityScrollAmount::NoAmount: return true;
	}
	_dropDownScroll->ScrollToVerticalOffset(
		_dropDownScroll->VerticalOffset + static_cast<double>(delta));
	return true;
}

bool ComboBox::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	if (horizontalPercent != AccessibilityScrollNoChange) return false;
	if (verticalPercent == AccessibilityScrollNoChange) return true;
	if (!std::isfinite(verticalPercent)
		|| verticalPercent < 0.0 || verticalPercent > 100.0) return false;
	SetCurrentIsDropDownOpen(true);
	if (!EnsureDropDownInfrastructure() || !_dropDownScroll) return false;
	float extent = 0.0f;
	float viewport = 0.0f;
	float offset = 0.0f;
	if (!GetScrollMetrics(extent, viewport, offset)) return false;
	const float maximum = (std::max)(0.0f, extent - viewport);
	_dropDownScroll->ScrollToVerticalOffset(maximum
		* static_cast<float>(verticalPercent / 100.0));
	return true;
}
