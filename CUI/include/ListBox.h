#pragma once

#include "ControlWeakReference.h"
#include "Selector.h"

#include <vector>

/** WPF ListBox selection policy. */
enum class SelectionMode : int
{
	Single = 0,
	Multiple = 1,
	Extended = 2,
};

/** Templated list selector. ListView specializes its item container. */
class ListBox : public Selector
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ListBoxAutomationPeer>(*this);
	}

public:
	ListBox();
	UIClass Type() override { return UIClass::UI_ListBox; }
	/** WPF ListBox.SelectionMode property identity. */
	static const DependencyProperty& SelectionModeProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	SelectionMode GetSelectionMode() const noexcept
	{
		return _selectionMode;
	}
	void SetSelectionMode(SelectionMode value);
	__declspec(property(get = GetSelectionMode, put = SetSelectionMode))
		SelectionMode SelectionModeValue;

	const std::vector<int>& GetSelectedIndices() const noexcept
	{
		return _selectedIndices;
	}
	std::vector<BindingValue> GetSelectedItems() const;
	void SelectAll();
	void UnselectAll();
	bool SelectIndex(int value) override;
	bool IsIndexSelected(size_t index) const noexcept override;

	/** Framework-facing item-container callbacks. */
	void NotifyItemClicked(
		size_t index, MouseButton button, ModifierKeys modifiers);
	void RequestItemSelection(size_t index, bool selected);
	bool ProcessItemKey(size_t itemIndex, const InputReport& input);

	bool HandlesNavigationKey(Key key) const override;
	// ListBox owns an item viewport even when a theme does not supply the
	// conventional ScrollViewer template. Generated containers must not paint
	// beyond the selector's arranged slot.
	bool ClipsChildren() override { return true; }

protected:
	bool ProcessInput(const InputReport& input) override;
	void OnSelectedIndexChanged(int oldValue, int newValue) override;
	void OnGeneratedItemsRebuilt() override;
	void OnAuthoredItemsChanged() noexcept override;

private:
	SelectionMode _selectionMode = SelectionMode::Single;
	std::vector<int> _selectedIndices;
	int _anchorIndex = -1;
	int _focusedIndex = -1;
	bool _applyingSelection = false;
	bool _restoringSelectionIdentities = false;
	std::vector<BindingSourceReference> _selectedSourceIdentities;
	std::vector<ControlWeakReference> _selectedAuthoredIdentities;
	BindingSourceReference _primarySourceIdentity;
	ControlWeakReference _primaryAuthoredIdentity;

	void ApplySelectionModeChange(
		SelectionMode oldValue, SelectionMode newValue);
	bool ApplySelection(
		std::vector<int> indices,
		int preferredPrimary,
		int actionIndex);
	void SelectOnly(int index);
	void ToggleIndex(int index);
	void SelectRange(int index, bool clearCurrent);
	void MakeKeyboardSelection(int index, ModifierKeys modifiers);
	bool HandleSelectionKey(
		int itemIndex, const InputReport& input, bool fromItem);
	void FocusIndex(int index);
	void UpdateAnchor(int index) noexcept;
	void CaptureSelectionIdentities() noexcept;
	void RestoreSelectionIdentities();
};
