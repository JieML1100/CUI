#pragma once

#include "ControlTemplate.h"
#include "ItemContainer.h"
#include "ItemsControl.h"

class Selector;

/** WPF-style content container produced for one ListBox item. */
class ListBoxItem : public ItemContainerControl
{
public:
	ListBoxItem();
	bool Initialize(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
		size_t index,
		std::wstring* outError = nullptr);
	UIClass Type() override { return UIClass::UI_ListBoxItem; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

private:
	void ActivateItem() override;
	void FocusOwner() override;
	void OnIsSelectedRequested(bool value) override;
};

/**
 * Shared single-selection model for templated item controls.
 *
 * Selection is expressed against ItemsSource records. SelectedItem preserves
 * record identity; SelectedValue optionally projects a typed record property.
 */
class Selector : public ItemsControl
{
public:
	using UIElement::SelectionChanged;
	Selector();
	UIClass Type() override { return UIClass::UI_Selector; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

	int GetSelectedIndex() const noexcept { return _selectedIndex; }
	void SetSelectedIndex(int value);
	__declspec(property(get = GetSelectedIndex, put = SetSelectedIndex))
		int SelectedIndex;

	virtual BindingValue GetSelectedItem() const;
	virtual void SetSelectedItem(const BindingValue& value);
	const std::wstring& GetSelectedValuePath() const noexcept
	{
		return _selectedValuePath;
	}
	void SetSelectedValuePath(std::wstring value);
	virtual BindingValue GetSelectedValue() const;
	virtual void SetSelectedValue(const BindingValue& value);
	bool GetIsSynchronizedWithCurrentItem() const noexcept
	{
		return _isSynchronizedWithCurrentItem;
	}
	void SetIsSynchronizedWithCurrentItem(bool value);
	ControlTemplateReference GetItemContainerTemplate() const noexcept
	{
		return _itemContainerTemplate;
	}
	void SetItemContainerTemplate(ControlTemplateReference value);

	bool SelectIndex(int value);
	bool HandlesNavigationKey(Key key) const override;

protected:
	bool ProcessInput(const InputReport& input) override;
	std::unique_ptr<ItemsSourceTransactionState>
		CaptureItemsSourceTransactionState() override;
	void RestoreItemsSourceTransactionState(
		ItemsSourceTransactionState& state) noexcept override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void OnGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRealized() override;
	void OnGeneratedItemIndexChanged(
		Control& visual, size_t oldIndex, size_t newIndex) override;
	bool ApplyItemContainerStyle() override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnItemsSourceChanged(
		const BindingListReference& oldValue,
		const BindingListReference& newValue) override;
	virtual size_t SelectionItemCount() const noexcept;
	virtual void OnSelectedIndexChanged(int oldValue, int newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	int ClampIndex(int value) const noexcept;
	void SetCurrentSelectedIndex(int value);
	void RefreshSelectedItemState(
		bool raiseSelectionChanged,
		std::optional<int> previousIndex = std::nullopt);
	void UpdateContainerSelection();

private:
	struct SelectionItemsSourceTransactionState final
		: ItemsSourceTransactionState
	{
		int SelectedIndex = -1;
		BindingSourceReference SelectedItemIdentity;
		BindingPathObservation SelectedItemObservation;
	};

	int _selectedIndex = -1;
	std::wstring _selectedValuePath;
	bool _isSynchronizedWithCurrentItem = false;
	ControlTemplateReference _itemContainerTemplate;
	BindingSourceReference _selectedItemIdentity;
	Control* _selectedAuthoredItemIdentity = nullptr;
	BindingPathObservation _selectedItemObservation;
	Event<void(Selector*)> _selectedItemChanged;
	Event<void(Selector*)> _selectedValueChanged;
	EventConnection _currentViewChanged;
	enum class CurrentItemSynchronizationDirection : unsigned char
	{
		None,
		SelectionFromView,
		CurrentViewFromSelection,
	};
	bool _synchronizingCurrentItem = false;
	bool _rollingBackCurrentItemSynchronizationProperty = false;
	CurrentItemSynchronizationDirection _pendingCurrentItemSynchronization =
		CurrentItemSynchronizationDirection::None;

	void ApplySelectedIndexChange(int oldValue, int newValue);
	void RestoreSelectionAfterRebuild();
	void NotifySelectionProjectionsChanged();
	void EnsureSelectedItemVisible();
	void ReconnectCurrentView();
	void SynchronizeSelectionFromCurrentView();
	void SynchronizeCurrentViewFromSelection();
	void RequestCurrentItemSynchronization(
		CurrentItemSynchronizationDirection direction);
	void ApplySelectionFromCurrentView();
	void ApplyCurrentViewFromSelection();
};
