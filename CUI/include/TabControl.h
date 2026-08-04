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
	static const DependencyPropertyKey& TabStripPlacementPropertyKey();

	bool _isSelected = false;
	Dock _tabStripPlacement = Dock::Top;
	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(bool value);
	void SetTabStripPlacementProjection(Dock value);
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);
	bool IsOriginalSourceWithinHeader(Control* source) const noexcept;
	std::unique_ptr<Control> DetachVisualContentForProjection()
	{
		return DetachVisualContentPreservingLogicalParent();
	}
	bool RestoreVisualContentFromProjection(
		std::unique_ptr<Control>& value) noexcept
	{
		return ContentControl::TrySetVisualContent(value);
	}

public:
	using UIElement::Selected;
	using UIElement::Unselected;

	UIClass Type() override { return UIClass::UI_TabItem; }
	static const DependencyProperty& IsSelectedProperty();
	static const DependencyProperty& TabStripPlacementProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	PROPERTY(bool, IsSelected);
	GET(bool, IsSelected);
	SET(bool, IsSelected);
	READONLY_PROPERTY(Dock, TabStripPlacement);
	GET(Dock, TabStripPlacement);

	TabItem();
	/** Includes visual Content temporarily projected by the owning TabControl. */
	Control* GetVisualContent() const noexcept override;
	Control* SetVisualContent(std::unique_ptr<Control> value) override;
	bool TrySetVisualContent(
		std::unique_ptr<Control>& value) noexcept override;
	std::unique_ptr<Control> DetachVisualContent() override;
	bool HandlesNavigationKey(Key key) const override;

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TabItemAutomationPeer>(*this);
	}
	bool ProcessInput(const InputReport& input) override;
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
	friend class TabItem;
	static const DependencyPropertyKey& SelectedContentPropertyKey();
	static const DependencyPropertyKey& SelectedContentTemplatePropertyKey();

	Dock _tabStripPlacement = Dock::Top;
	ItemTemplateReference _contentTemplate;
	BindingValue _selectedContent;
	ItemTemplateReference _selectedContentTemplate;
	TabItem* _selectedTabIdentity = nullptr;
	ControlWeakReference _selectedContentHost;
	ControlWeakReference _projectedVisualItem;
	ControlWeakReference _projectedVisualContent;
	ControlWeakReference _observedSelectedContentItem;
	EventConnection _selectedContentProjectionObservation;
	bool _synchronizingSelectionProjection = false;
	bool _selectionProjectionPending = true;
	bool _synchronizingContentProjection = false;
	bool _restoringContentProjection = false;
	bool _contentProjectionPending = true;
	bool _templateAbortDeferred = false;

	static constexpr float DefaultHeaderExtent = 28.0f;
	static constexpr float DefaultVerticalStripExtent = 120.0f;

	void PrepareItemMutation();
	void ReconcileItemsAfterMutation(TabItem* previouslySelectedItem);
	void ObserveSelectedContentProjection(TabItem* item);
	void SynchronizeSelectionProjection();
	void RefreshSelectedContentProjection();
	void SynchronizeSelectedContentHost();
	void RestoreProjectedVisualContent();
	void CompleteDeferredTemplateAbort() noexcept;
	bool HasProjectedVisualContent(const TabItem* item) const noexcept;
	D2D1_RECT_F GetTabStripRect() const noexcept;
	D2D1_RECT_F GetTabHeaderRect(int index) const noexcept;
	int FindNextEligibleTab(
		int startIndex, int direction, bool wrap) const noexcept;
	bool FocusAndSelectItem(int index);
	bool ProcessTabNavigationKey(const InputReport& input);
	static void EnsureClassHandlers();
	static void HandleRoutedPointerPress(
		Control* sender, RoutedEventArgs& args);

public:
	UIClass Type() override { return UIClass::UI_TabControl; }
	static const DependencyProperty& TabStripPlacementProperty();
	static const DependencyProperty& ContentTemplateProperty();
	static const DependencyProperty& SelectedContentProperty();
	static const DependencyProperty& SelectedContentTemplateProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	PROPERTY(Dock, TabStripPlacement);
	GET(Dock, TabStripPlacement);
	SET(Dock, TabStripPlacement);
	ItemTemplateReference GetContentTemplate() const noexcept
	{
		return _contentTemplate;
	}
	void SetContentTemplate(ItemTemplateReference value);
	READONLY_PROPERTY(BindingValue, SelectedContent);
	BindingValue GetSelectedContent() const { return _selectedContent; }
	READONLY_PROPERTY(ItemTemplateReference, SelectedContentTemplate);
	ItemTemplateReference GetSelectedContentTemplate() const noexcept
	{
		return _selectedContentTemplate;
	}

	TabControl();
	~TabControl() override;

	TabItem* AddItem(std::unique_ptr<TabItem> page);
	TabItem* InsertItem(int index, std::unique_ptr<TabItem> page);
	TabItem* GetItem(int index) const noexcept;
	int IndexOfItem(const TabItem* page) const noexcept;
	std::unique_ptr<Control> DetachItemControlAt(size_t index) override;
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
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	bool HandlesNavigationKey(Key key) const override;
protected:
	bool ProcessInput(const InputReport& input) override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TabControlAutomationPeer>(*this);
	}
	size_t SelectionItemCount() const noexcept override { return ItemCount(); }
	void OnSelectedIndexChanged(int oldValue, int newValue) override;
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	std::unique_ptr<Panel> CreateItemsHost() const override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void PreparePresentation() override;
	void PerformPendingLayout() override;
	std::unique_ptr<Control> DetachVisualChildTemplateRoot() override;
	void OnControlTemplatePresentationChanged() override;
	void OnTemplateChanged(
		const ControlTemplateReference& oldTemplate,
		const ControlTemplateReference& newTemplate) override;
};
