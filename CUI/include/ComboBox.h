#pragma once

#include "ControlWeakReference.h"
#include "Selector.h"

#include <unordered_map>

class ComboBox;
class Popup;
class ScrollViewer;
class TextBox;

/** WPF-style content container generated for one ComboBox item. */
class ComboBoxItem final : public ListBoxItem
{
public:
	ComboBoxItem();
	UIClass Type() override { return UIClass::UI_ComboBoxItem; }
	static const DependencyProperty& IsHighlightedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	bool GetIsHighlighted() const noexcept { return _isHighlighted; }

private:
	friend class ComboBox;
	static const DependencyPropertyKey& IsHighlightedPropertyKey();
	bool ProcessInput(const InputReport& input) override;
	void ActivateItem(
		MouseButton button, ModifierKeys modifiers) override;
	void FocusOwner() override;
	void OnIsSelectedRequested(bool value) override;
	bool ActivatesOnPointerUp() const noexcept override { return true; }
	void OnIsMouseOverChanged(bool oldValue, bool newValue) override;
	void SetIsHighlighted(bool value);

	bool _isHighlighted = false;
};

/**
 * Single-selection ItemsControl whose drop-down presentation is supplied by a
 * Popup + ItemsPresenter in its ControlTemplate.
 *
 * The native fallback creates exactly the same standard primitive tree on
 * first open. There is no string-side collection, per-row immediate drawing,
 * item-count scrolling model, or ComboBox-owned foreground renderer.
 */
class ComboBox : public Selector
{
protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ComboBoxAutomationPeer>(*this);
	}

public:
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	ComboBox();
	~ComboBox() override;
	UIClass Type() override { return UIClass::UI_ComboBox; }
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& TextProperty();
	static const DependencyProperty& IsDropDownOpenProperty();
	static const DependencyProperty& IsEditableProperty();
	static const DependencyProperty& IsReadOnlyProperty();
	static const DependencyProperty& StaysOpenOnEditProperty();
	static const DependencyProperty&
		ShouldPreserveUserEnteredPrefixProperty();
	static const DependencyProperty& MaxDropDownHeightProperty();
	static const DependencyProperty& SelectionBoxItemProperty();
	static const DependencyProperty& SelectionBoxItemTemplateProperty();
	static const DependencyProperty& SelectionBoxItemStringFormatProperty();
	static const DependencyProperty& IsSelectionBoxHighlightedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	std::wstring GetSemanticText() const override;

	bool GetIsDropDownOpen() const noexcept { return _isDropDownOpen; }
	void SetIsDropDownOpen(bool value);
	__declspec(property(get = GetIsDropDownOpen, put = SetIsDropDownOpen))
		bool IsDropDownOpen;
	bool GetIsEditable() const noexcept { return _isEditable; }
	void SetIsEditable(bool value);
	bool GetIsReadOnly() const noexcept { return _isReadOnly; }
	void SetIsReadOnly(bool value);
	bool GetStaysOpenOnEdit() const noexcept { return _staysOpenOnEdit; }
	void SetStaysOpenOnEdit(bool value);
	bool GetShouldPreserveUserEnteredPrefix() const noexcept
	{
		return _shouldPreserveUserEnteredPrefix;
	}
	void SetShouldPreserveUserEnteredPrefix(bool value);
	const BindingValue& GetSelectionBoxItem() const noexcept
	{
		return _selectionBoxItem;
	}
	ItemTemplateReference GetSelectionBoxItemTemplate() const noexcept
	{
		return _selectionBoxItemTemplate;
	}
	const std::wstring& GetSelectionBoxItemStringFormat() const noexcept
	{
		return _selectionBoxItemStringFormat;
	}
	bool GetIsSelectionBoxHighlighted() const noexcept
	{
		return _isSelectionBoxHighlighted;
	}

	Event<void(ComboBox*)> DropDownOpened;
	Event<void(ComboBox*)> DropDownClosed;

	float GetMaxDropDownHeight() const noexcept { return _maxDropDownHeight; }
	void SetMaxDropDownHeight(float value);
	__declspec(property(get = GetMaxDropDownHeight,
		put = SetMaxDropDownHeight)) float MaxDropDownHeight;

	size_t GeneratedItemCount() const noexcept
	{
		return ItemsControl::GeneratedItemCount();
	}
	ComboBoxItem* GetGeneratedItem(size_t index) const noexcept
	{
		return dynamic_cast<ComboBoxItem*>(
			ItemsControl::GetGeneratedItem(index));
	}

	/** Selects through Selector's SetCurrentValue-preserving interaction path. */
	bool SelectItem(int index);
	bool ProcessItemKey(size_t itemIndex, const InputReport& input);

	// Typed ownership surface corresponding to WPF's Items object model.
	ComboBoxItem* AddItem(std::unique_ptr<ComboBoxItem> item);
	ComboBoxItem* InsertItem(
		int index, std::unique_ptr<ComboBoxItem> item);
	ComboBoxItem* GetItem(int index) const noexcept;
	int IndexOfItem(const ComboBoxItem* item) const noexcept;
	std::unique_ptr<ComboBoxItem> DetachItemAt(int index);
	std::unique_ptr<ComboBoxItem> DetachItem(ComboBoxItem* item);
	bool RemoveItemAt(int index);
	bool RemoveItem(ComboBoxItem* item);
	void ClearItems();

	bool HandlesNavigationKey(Key key) const override;
	/** The closed selector face is one interaction surface; Popup is a separate root. */
	bool HitTestChildren() const override { return _isEditable; }
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;
protected:
	void PreparePresentation() override;
	bool ProcessInput(const InputReport& input) override;
private:
	friend class ComboBoxAutomationPeer;
	friend class ComboBoxItem;
	static const DependencyPropertyKey& SelectionBoxItemPropertyKey();
	static const DependencyPropertyKey& SelectionBoxItemTemplatePropertyKey();
	static const DependencyPropertyKey&
		SelectionBoxItemStringFormatPropertyKey();
	static const DependencyPropertyKey&
		IsSelectionBoxHighlightedPropertyKey();
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);
	static void HandleDescendantPointerRelease(
		Control* sender, RoutedEventArgs& args);
	bool IsOriginalSourceWithinTemplatePart(
		Control* source, TemplatePartToken part) const noexcept;
	void BeginPointerPress(MouseEventArgs& args);
	bool CompletePointerPress(MouseEventArgs& args);

	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result);
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId);
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result);
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result);
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result);
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result);
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action);
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id);
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical);
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent);

protected:
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void OnGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRealized() override;
	void OnGeneratedItemIndexChanged(
		Control& visual, size_t oldIndex, size_t newIndex) override;
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnItemsSourceChanged(
		const BindingListReference& oldValue,
		const BindingListReference& newValue) override;
	bool ShouldRealizeVirtualItemsWithoutViewport() const noexcept override
	{
		return false;
	}
	void OnSelectedIndexChanged(int oldValue, int newValue) override;
	std::wstring GetTextSearchItemText(
		size_t index) const override;
	void OnTextSearchMatch(size_t index) override;
	void OnControlTemplatePresentationChanged() override;
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;

private:
	bool _isDropDownOpen = false;
	bool _isEditable = false;
	bool _isReadOnly = false;
	bool _staysOpenOnEdit = false;
	bool _shouldPreserveUserEnteredPrefix = false;
	bool _isSelectionBoxHighlighted = false;
	bool _pointerPressActive = false;
	float _maxDropDownHeight = 320.0f;
	int _highlightedIndex = -1;
	int _selectionBeforeDropDown = -1;
	BindingValue _selectionBoxItem = BindingValue(std::wstring{});
	ItemTemplateReference _selectionBoxItemTemplate;
	std::wstring _selectionBoxItemStringFormat;
	Popup* _popup = nullptr;
	Popup* _defaultPopup = nullptr;
	ScrollViewer* _dropDownScroll = nullptr;
	TextBox* _editableTextBox = nullptr;
	bool _buildingDropDownInfrastructure = false;
	bool _updatingTextFromSelection = false;
	bool _updatingEditableTextBox = false;
	bool _preserveTextDuringSelection = false;
	EventConnection _popupOpened;
	EventConnection _popupClosed;
	EventConnection _editableTextChanged;
	std::vector<BindingPathObservation> _itemSourceObservations;
	std::vector<EventConnection> _authoredItemChanges;

	std::vector<uint32_t> _accessibilityItemIds;
	std::vector<BindingSourceReference> _accessibilitySourceIdentities;
	std::vector<ControlWeakReference> _accessibilityAuthoredIdentities;
	std::unordered_map<uint32_t, size_t> _accessibilityItemIndexById;
	uint32_t _selectedAccessibilityItemId = 0;

	void ApplyIsDropDownOpenChange(bool oldValue, bool newValue);
	void ApplyIsEditableChange(bool oldValue, bool newValue);
	void ApplyIsReadOnlyChange(bool oldValue, bool newValue);
	void ApplyTextChange(
		const std::wstring& oldValue, const std::wstring& newValue);
	void SetCurrentIsDropDownOpen(bool value)
	{
		(void)TrySetCurrentPropertyValue(
			IsDropDownOpenProperty(), BindingValue(value));
	}
	void ApplyMaxDropDownHeight();
	bool EnsureDropDownInfrastructure();
	void ConfigurePopupPart(Popup* popup);
	Popup* ResolvePopupPart() const noexcept;
	ScrollViewer* ResolveScrollOwner() const noexcept;
	void UpdateItemsHostPresentation();
	void SyncTextWithSelection();
	void SyncEditableTextBox();
	void UpdateSelectionBoxState();
	void UpdateSelectionBoxHighlightState();
	void SetHighlightedIndex(int value, bool focusItem);
	void CommitHighlightedSelection();
	void CloseDropDown(bool commitSelection);
	int FindItemByTextPrefix(
		const std::wstring& text, bool exact) const;
	void NotifyItemHighlighted(size_t index);
	void RefreshItems();
	void RefreshDataItem(size_t index);
	std::wstring GetAuthoredItemText(size_t index) const;
	std::wstring GetItemDisplayText(size_t index) const;
	void UpdateGeneratedItemStates();
	void ReconcileAccessibilityItemIds();
	void RebuildAccessibilityItemIndex();
	int FindAccessibilityItem(uint32_t id);
	bool TryGetItemBounds(
		size_t index, D2D1_RECT_F& bounds, bool& visible) const noexcept;
	bool GetScrollMetrics(
		float& extent, float& viewport, float& offset) const noexcept;
};
