#pragma once

#include "HeaderedContentControl.h"
#include "Selector.h"

/**
 * One WPF-style TabControl item. Header is presented by the owning tab strip;
 * Content remains the page visual. Selection is a framework-owned projection.
 */
class TabItem : public HeaderedContentControl
{
private:
	friend class TabControl;
	bool _isSelected = false;
	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(bool value);

public:
	using UIElement::Selected;
	using UIElement::Unselected;

	UIClass Type() override { return UIClass::UI_TabItem; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	PROPERTY(bool, IsSelected);
	GET(bool, IsSelected);
	SET(bool, IsSelected);

	TabItem();

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TabItemAutomationPeer>(*this);
	}
	void ConfigureHeaderVisual(Control& child) override;
	void ReleaseHeaderVisual(Control& child) override;
	float GetHeaderSlotHeightDip(float availableWidth) override;
};

/**
 * WPF-compatible single-selection page control.
 *
 * Public state is intentionally small: Selector owns selection and
 * TabStripPlacement owns strip location. Header metrics, chrome, scrolling and
 * transition policy are theme/panel implementation details rather than control
 * dependency properties.
 */
class TabControl : public Selector
{
private:
	Dock _tabStripPlacement = Dock::Top;
	TabItem* _selectedTabIdentity = nullptr;
	int _hoveredHeaderIndex = -1;
	int _pressedHeaderIndex = -1;

	static constexpr float HorizontalStripExtent = 28.0f;
	static constexpr float VerticalStripExtent = 120.0f;

	void PrepareItemMutation();
	void ReconcileItemsAfterMutation(TabItem* previouslySelectedItem);
	void SynchronizeSelectionProjection();
	void ArrangePage(TabItem* page);
	D2D1_RECT_F GetTabStripRect() const noexcept;
	D2D1_RECT_F GetTabHeaderRect(int index) const noexcept;

public:
	UIClass Type() override { return UIClass::UI_TabControl; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	PROPERTY(Dock, TabStripPlacement);
	GET(Dock, TabStripPlacement);
	SET(Dock, TabStripPlacement);

	TabControl();

	TabItem* AddItem(std::unique_ptr<TabItem> page);
	TabItem* InsertItem(int index, std::unique_ptr<TabItem> page);
	TabItem* GetItem(int index) const noexcept;
	int IndexOfItem(const TabItem* page) const noexcept;
	std::unique_ptr<TabItem> DetachItemAt(int index);
	std::unique_ptr<TabItem> DetachItem(TabItem* page);
	bool RemoveItemAt(int index);
	bool RemoveItem(TabItem* page);
	bool MoveItem(int oldIndex, int newIndex);
	void ClearItems();
	bool SelectItem(int index);

	BindingValue GetSelectedItem() const override;
	void SetSelectedItem(const BindingValue& value) override;
	BindingValue GetSelectedValue() const override;
	void SetSelectedValue(const BindingValue& value) override;

	/** Designer/native behavior hit testing; not a XAML member. */
	bool TryGetTabHeaderIndexAt(
		int localX, int localY, int& outIndex) const noexcept;
	D2D1_RECT_F GetContentRect() const noexcept;
	bool ClipsChildren() override { return true; }
	D2D1_RECT_F GetVisualChildrenClipRect() override;
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
protected:
	bool ProcessInput(const InputReport& input) override;
	void OnRender() override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TabControlAutomationPeer>(*this);
	}
	size_t SelectionItemCount() const noexcept override { return ItemCount(); }
	void OnSelectedIndexChanged(int oldValue, int newValue) override;
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnGeneratedItemsRebuilt() override;
	std::unique_ptr<Panel> CreateItemsHost() const override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void PreparePresentation() override;
	void PerformPendingLayout() override;
};
