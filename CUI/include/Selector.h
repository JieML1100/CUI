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
#if CUI_ENABLE_DYNAMIC_XAML
	bool Initialize(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
		size_t index,
		std::wstring* outError = nullptr);
#endif
	bool Initialize(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		CompiledBindingPathView displayMemberPath,
		size_t index,
		std::wstring* outError = nullptr);
	UIClass Type() override { return UIClass::UI_ListBoxItem; }
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	bool HandlesNavigationKey(Key key) const override;

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override;
	bool ProcessInput(const InputReport& input) override;
	void ActivateItem(
		MouseButton button, ModifierKeys modifiers) override;
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
	static const DependencyProperty& SelectedIndexProperty();
	static const DependencyProperty& SelectedItemProperty();
	static const DependencyProperty& SelectedValueProperty();
	static const DependencyProperty& IsSynchronizedWithCurrentItemProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	static const DependencyProperty& SelectedValuePathProperty();
#endif
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	/**
	 * Publishes WPF's inheritable Selector.IsSelectionActive attached state.
	 *
	 * TreeView is not a Selector in WPF, but shares this read-only attached
	 * property with Selector-derived controls for container visual states.
	 */
	static void SetIsSelectionActive(Control& target, bool value);

	int GetSelectedIndex() const noexcept { return _selectedIndex; }
	void SetSelectedIndex(int value);
	__declspec(property(get = GetSelectedIndex, put = SetSelectedIndex))
		int SelectedIndex;

	virtual BindingValue GetSelectedItem() const;
	virtual void SetSelectedItem(const BindingValue& value);
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& GetSelectedValuePath() const noexcept
	{
		return _selectedValuePath;
	}
	void SetSelectedValuePath(std::wstring value);
#endif
	[[nodiscard]] CompiledBindingPathView
		GetCompiledSelectedValuePath() const noexcept
	{
		return _compiledSelectedValuePath;
	}
	void SetCompiledSelectedValuePath(CompiledBindingPathView value);
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

	virtual bool SelectIndex(int value);
	bool HandlesNavigationKey(Key key) const override;

protected:
	void VisitDeclaredInheritedProperties(
		void* context, InheritedPropertyVisitor visitor) const override;
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
	void OnTextSearchMatch(size_t index) override;
	bool ApplyItemContainerStyle() override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnItemsSourceChanged(
		const BindingListReference& oldValue,
		const BindingListReference& newValue) override;
	virtual size_t SelectionItemCount() const noexcept;
	virtual bool IsIndexSelected(size_t index) const noexcept;
	virtual void OnSelectedIndexChanged(int oldValue, int newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	int ClampIndex(int value) const noexcept;
	void SetCurrentSelectedIndex(int value);
	void SetCurrentSelectedIndexWithoutSelectionChanged(int value);
	void RefreshSelectedItemState(
		bool raiseSelectionChanged,
		std::optional<int> previousIndex = std::nullopt);
	void UpdateContainerSelection();
	void UpdateSelectionActiveState();
	BindingValue GetSelectionItemAt(size_t index) const;
	bool HasSelectedValuePath() const noexcept;
	bool TryReadSelectedValue(
		IBindingSource& item, BindingValue& value) const;
	BindingPathObservation ObserveItemProjectionPaths(
		const BindingSourceReference& item,
		std::function<void()> changed) const;
	void RaiseSelectionChanged(
		int oldIndex,
		int newIndex,
		std::vector<BindingValue> removedItems,
		std::vector<BindingValue> addedItems);

private:
#if CUI_ENABLE_DYNAMIC_XAML
	static void RegisterDesignDependencyProperties();
#endif
	static const DependencyPropertyKey& IsSelectionActivePropertyKey();

	struct SelectionItemsSourceTransactionState final
		: ItemsSourceTransactionState
	{
		int SelectedIndex = -1;
		BindingSourceReference SelectedItemIdentity;
		BindingPathObservation SelectedItemObservation;
	};

	int _selectedIndex = -1;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _selectedValuePath;
#endif
	CompiledBindingPathView _compiledSelectedValuePath;
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
	bool _suppressSelectionChanged = false;
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
#if CUI_ENABLE_DYNAMIC_XAML
	bool HasAuthoredSelectedValuePath() const noexcept;
	bool TryReadAuthoredSelectedValue(
		IBindingSource& item, BindingValue& value) const;
	BindingPathObservation ObserveAuthoredSelectedValuePath(
		const BindingSourceReference& item,
		std::function<void()> changed) const;
	bool TryReadAuthoredSelectedValueAt(
		size_t index, BindingValue& value) const;
	int FindAuthoredSelectedValue(const BindingValue& value) const;
	bool InitializeAuthoredGeneratedContainer(
		ListBoxItem& container,
		const BindingSourceReference& item,
		size_t index,
		std::wstring& error) const;
#endif
};
