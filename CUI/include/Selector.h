#pragma once

#include "ControlTemplate.h"
#include "ItemContainer.h"
#include "ItemsControl.h"

class Selector;

/** WPF-style content container produced for one ListBox item. */
class SelectorItem final : public ItemContainerControl
{
public:
	SelectorItem();
	bool Initialize(
		Selector& owner,
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
		size_t index,
		std::wstring* outError = nullptr);
	UIClass Type() override { return UIClass::UI_SelectorItem; }
	void EnsureBindingPropertiesRegistered() override;

private:
	Selector* _owner = nullptr;
	void ActivateItem() override;
	void FocusOwner() override;
};

/** WPF-facing C++ name while preserving the established ABI type. */
using ListBoxItem = SelectorItem;

/**
 * Shared single-selection model for templated item controls.
 *
 * Selection is expressed against ItemsSource records. SelectedItem preserves
 * record identity; SelectedValue optionally projects a typed record property.
 */
class Selector : public ItemsControl
{
public:
	Selector(int x = 0, int y = 0, int width = 240, int height = 200);
	UIClass Type() override { return UIClass::UI_Base; }
	void EnsureBindingPropertiesRegistered() override;

	int GetSelectedIndex() const noexcept { return _selectedIndex; }
	void SetSelectedIndex(int value);
	__declspec(property(get = GetSelectedIndex, put = SetSelectedIndex))
		int SelectedIndex;

	BindingValue GetSelectedItem() const;
	void SetSelectedItem(const BindingValue& value);
	const std::wstring& GetSelectedValuePath() const noexcept
	{
		return _selectedValuePath;
	}
	void SetSelectedValuePath(std::wstring value);
	BindingValue GetSelectedValue() const;
	void SetSelectedValue(const BindingValue& value);
	const std::wstring& GetItemContainerStyle() const noexcept
	{
		return _itemContainerStyle;
	}
	void SetItemContainerStyle(std::wstring value);
	ControlTemplateReference GetItemContainerTemplate() const noexcept
	{
		return _itemContainerTemplate;
	}
	void SetItemContainerTemplate(ControlTemplateReference value);

	bool SelectIndex(int value);
	SelectionChangedEvent OnSelectionChanged;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool ProcessMessage(
		UINT message, WPARAM wParam, LPARAM lParam,
		int localX, int localY) override;

protected:
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void OnGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRealized() override;
	void OnGeneratedItemIndexChanged(
		Control& visual, size_t oldIndex, size_t newIndex) override;

private:
	int _selectedIndex = -1;
	std::wstring _selectedValuePath;
	std::wstring _itemContainerStyle;
	ControlTemplateReference _itemContainerTemplate;
	BindingSourceReference _selectedItemIdentity;
	BindingPathObservation _selectedItemObservation;
	Event<void(Selector*)> _selectedItemChanged;
	Event<void(Selector*)> _selectedValueChanged;

	int ClampIndex(int value) const noexcept;
	void SetCurrentSelectedIndex(int value);
	void ApplySelectedIndexChange(int oldValue, int newValue);
	void RestoreSelectionAfterRebuild();
	void RefreshSelectedItemState(bool raiseSelectionChanged);
	void UpdateContainerSelection();
	void NotifySelectionProjectionsChanged();
	void EnsureSelectedItemVisible();
};
