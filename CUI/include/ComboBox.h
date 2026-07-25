#pragma once

#include "ControlWeakReference.h"
#include "Selector.h"

#include <unordered_map>

class ComboBox;
class Popup;
class ScrollViewer;

/** WPF-style content container generated for one ComboBox item. */
class ComboBoxItem final : public ListBoxItem
{
public:
	ComboBoxItem();
	UIClass Type() override { return UIClass::UI_ComboBoxItem; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

private:
	void ActivateItem() override;
	void FocusOwner() override;
	void OnIsSelectedRequested(bool value) override;
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
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	bool GetIsDropDownOpen() const noexcept { return _isDropDownOpen; }
	void SetIsDropDownOpen(bool value);
	__declspec(property(get = GetIsDropDownOpen, put = SetIsDropDownOpen))
		bool IsDropDownOpen;

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

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
	/** The closed selector face is one interaction surface; Popup is a separate root. */
	bool HitTestChildren() const override { return false; }
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;
protected:
	void PreparePresentation() override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
private:
	friend class ComboBoxAutomationPeer;

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
	void OnControlTemplatePresentationChanged() override;
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;

private:
	bool _isDropDownOpen = false;
	bool _pointerPressActive = false;
	float _maxDropDownHeight = 320.0f;
	Popup* _popup = nullptr;
	Popup* _defaultPopup = nullptr;
	ScrollViewer* _dropDownScroll = nullptr;
	bool _buildingDropDownInfrastructure = false;
	EventConnection _popupOpened;
	EventConnection _popupClosed;
	std::vector<BindingPathObservation> _itemSourceObservations;
	std::vector<EventConnection> _authoredItemChanges;

	std::vector<uint32_t> _accessibilityItemIds;
	std::vector<BindingSourceReference> _accessibilitySourceIdentities;
	std::vector<ControlWeakReference> _accessibilityAuthoredIdentities;
	std::unordered_map<uint32_t, size_t> _accessibilityItemIndexById;
	uint32_t _selectedAccessibilityItemId = 0;

	void ApplyIsDropDownOpenChange(bool oldValue, bool newValue);
	void SetCurrentIsDropDownOpen(bool value)
	{
		(void)TrySetCurrentPropertyValue(
			L"IsDropDownOpen", BindingValue(value));
	}
	void ApplyMaxDropDownHeight();
	bool EnsureDropDownInfrastructure();
	void ConfigurePopupPart(Popup* popup);
	Popup* ResolvePopupPart() const noexcept;
	ScrollViewer* ResolveScrollOwner() const noexcept;
	void UpdateItemsHostPresentation();
	void SyncTextWithSelection();
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
