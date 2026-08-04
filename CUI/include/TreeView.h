#pragma once

#include "ControlTemplate.h"
#include "HeaderedItemsControl.h"

#include <functional>
#include <unordered_set>
#include <vector>

class TreeView;
class ScrollViewer;
class ToggleButton;

enum class TreeViewDropPosition : uint8_t
{
	None,
	Before,
	Inside,
	After
};

/**
 * A real hierarchical item container.
 *
 * Its Header is presented by HeaderedItemsControl and its children remain in
 * this container's own ItemsHost. There is no flattened shadow node tree and
 * no synthetic visual parent.
 */
class TreeViewItem final : public HeaderedItemsControl
{
public:
	using UIElement::Expanded;
	using UIElement::Collapsed;
	using UIElement::Selected;
	using UIElement::Unselected;
	TreeViewItem();
	UIClass Type() override { return UIClass::UI_TreeViewItem; }
	static const DependencyProperty& IsExpandedProperty();
	static const DependencyProperty& HasItemsProperty();
	static const DependencyProperty& IsSelectedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	void SetIsExpanded(bool value);
	bool GetIsExpanded() const noexcept { return _expanded; }
	__declspec(property(get = GetIsExpanded, put = SetIsExpanded))
		bool IsExpanded;
	bool GetHasItems() const noexcept { return _hasItems; }
	__declspec(property(get = GetHasItems)) bool HasItems;
	int GetLevel() const noexcept { return _level; }
	__declspec(property(get = GetLevel)) int Level;
	void SetIsSelected(bool value);
	bool GetIsSelected() const noexcept { return _selected; }
	__declspec(property(get = GetIsSelected, put = SetIsSelected))
		bool IsSelected;
	bool HandlesNavigationKey(Key key) const override;

	BindingListReference GetItemsSource() const noexcept override;
	void SetItemsSource(BindingListReference value) override;
	size_t ItemCount() const noexcept override;
	TreeViewItem* ContainerFromIndex(size_t index) const noexcept;

protected:
	bool ProcessInput(const InputReport& input) override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TreeViewItemAutomationPeer>(*this);
	}
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRealized() override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnControlTemplatePresentationChanged() override;
	cui::core::Insets GetHeaderPresentationInsets() const noexcept override;
	cui::core::Insets GetItemsPresentationInsets() const noexcept override;
	void OnIsMouseOverChanged(bool previous, bool current) override;

private:
#if CUI_ENABLE_DYNAMIC_XAML
	static void RegisterDesignDependencyProperties();
#endif
	static const DependencyPropertyKey& HasItemsPropertyKey();
	static const DependencyPropertyKey& LevelPropertyKey();

	TreeView* _owner = nullptr;
	TreeViewItem* _parentItem = nullptr;
	BindingSourceReference _dataItem;
	BindingListReference _hierarchicalItemsSource;
	ItemTemplateReference _headerDataTemplate;
	BindingPathObservation _hierarchicalItemsObservation;
	EventConnection _hierarchicalItemsChanged;
	bool _expanded = false;
	bool _hasItems = false;
	bool _selected = false;
	bool _generatedContainer = false;
	bool _initializingGeneratedContainer = false;
	// The first expansion materializes this item's child generator.  Its
	// lifecycle callbacks must not force a second whole-TreeView hierarchy
	// walk; TreeView attaches just that newly materialized branch afterward.
	bool _materializingExpansionChildren = false;
	int _level = 0;
	uint32_t _accessibilityId = 0;
	Event<void(TreeViewItem*)> _hasItemsChanged;
	Event<void(TreeViewItem*)> _levelChanged;
	Event<void(TreeViewItem*)> _selectedChanged;
	Event<void(TreeViewItem*)> _expandedChanged;
	ToggleButton* _expanderPart = nullptr;
	EventConnection _expanderClickConnection;

	bool InitializeGenerated(
		TreeView& owner,
		const ItemsControl& projectionOwner,
		const BindingSourceReference& item,
		ItemTemplateReference headerTemplate,
		CompiledBindingPathView displayMemberPath,
		int level,
		std::wstring* outError);
	bool RefreshHierarchicalItemsSource(std::wstring* outError = nullptr);
	void RefreshHierarchicalCollectionObservation();
	bool EnsureChildrenRealized();
	bool HierarchyMigrationInProgress() const noexcept;
	void CollectMaterializedChildContainers(
		std::vector<TreeViewItem*>& output) const;
	void BindHierarchy(TreeView& owner, TreeViewItem* parent, int level);
	void UnbindHierarchy() noexcept;
	void ApplyIsExpandedValue(bool value);
	void SetCurrentIsExpanded(bool value);
	bool ShouldRestoreFocusAfterCollapse() const noexcept;
	void RestoreFocusAfterCollapse(bool requested);
	void SyncHasItems();
	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(bool value);
	void ApplyExpansionPresentation();
	Control* GetHeaderInteractionVisual() noexcept;
	bool IsExpanderSource(Control* source) const noexcept;
	void HandleHeaderPointer(MouseEventArgs& args, bool doubleClick);
	BindingListReference GetMaterializedItemsSource() const noexcept
	{
		return HeaderedItemsControl::GetItemsSource();
	}

	friend class TreeView;
};

/**
 * WPF-style hierarchical ItemsControl.
 *
 * Authored and generated TreeViewItem instances form the sole semantic,
 * logical, and visual hierarchy. ItemsSource generation uses the common
 * ItemsControl generator at every level; TreeView keeps only selection and
 * hierarchy coordination state.
 */
class TreeView : public ItemsControl
{
public:
	using UIElement::SelectedItemChanged;
#if CUI_ENABLE_DYNAMIC_XAML
	using ImplicitItemTemplateResolver = std::function<ItemTemplateReference(
		const std::wstring& itemTypeName)>;
#endif
	using CompiledImplicitItemTemplateResolver =
		std::function<ItemTemplateReference(DataTypeToken itemType)>;

	TreeView();
	~TreeView() override;
	UIClass Type() override { return UIClass::UI_TreeView; }
#if CUI_ENABLE_DYNAMIC_XAML
	static const DependencyProperty& SelectedValuePathProperty();
#endif
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	void SetItemsSource(BindingListReference value) override;
	void SetItemTemplate(ItemTemplateReference value) override;
#if CUI_ENABLE_DYNAMIC_XAML
	void SetDisplayMemberPath(std::wstring value) override;
#endif
	void SetCompiledDisplayMemberPath(
		CompiledBindingPathView value) override;
	BindingValue GetSelectedItem() const;
	BindingValue GetSelectedValue() const;
	TreeViewItem* GetSelectedContainer() const noexcept
	{
		return _selectedContainer;
	}
	TreeViewItem* ContainerFromIndex(size_t index) const noexcept;
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
	bool SelectItem(TreeViewItem* item, bool bringIntoView = true);
#if CUI_ENABLE_DYNAMIC_XAML
	void SetImplicitItemTemplateResolver(ImplicitItemTemplateResolver value);
#endif
	void SetCompiledImplicitItemTemplateResolver(
		CompiledImplicitItemTemplateResolver value);
	ControlTemplateReference GetItemContainerTemplate() const noexcept
	{
		return _itemContainerTemplate;
	}
	void SetItemContainerTemplate(ControlTemplateReference value);

	TreeViewItem* HitTestItem(
		float localX, float localY, float* relativeRowY = nullptr);
	void SetDropTarget(TreeViewItem* item, TreeViewDropPosition position);
	void ClearDropTarget();

	bool HandlesNavigationKey(Key key) const override;
	// TreeView is a viewport-owning selector. Expanded descendants remain
	// clipped to its arranged bounds unless a theme narrows that viewport with
	// an inner ScrollViewer.
	bool ClipsChildren() override { return true; }
protected:
	bool ProcessInput(const InputReport& input) override;
	void OnRender() override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TreeViewAutomationPeer>(*this);
	}
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	bool ApplyItemContainerStyle() override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRealized() override;
	void OnAuthoredItemsChanged() noexcept override;

private:
#if CUI_ENABLE_DYNAMIC_XAML
	static void RegisterDesignDependencyProperties();
#endif
	static const DependencyPropertyKey& SelectedItemPropertyKey();
	static const DependencyPropertyKey& SelectedValuePropertyKey();

	TreeViewItem* _selectedContainer = nullptr;
	TreeViewItem* _hoveredContainer = nullptr;
	TreeViewItem* _dropTarget = nullptr;
	TreeViewDropPosition _dropPosition = TreeViewDropPosition::None;
	ControlTemplateReference _itemContainerTemplate;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _selectedValuePath;
#endif
	CompiledBindingPathView _compiledSelectedValuePath;
	BindingPathObservation _selectedItemObservation;
	Event<void(TreeView*)> _selectedItemChanged;
	Event<void(TreeView*)> _selectedValueChanged;
	CompiledImplicitItemTemplateResolver _compiledImplicitItemTemplateResolver;
#if CUI_ENABLE_DYNAMIC_XAML
	ImplicitItemTemplateResolver _implicitItemTemplateResolver;
#endif
	BindingSourceReference _selectionRestoreIdentity;
	ControlWeakReference _selectionRestoreAuthored;
	std::vector<ControlWeakReference> _boundContainers;
	// Membership mirrors _boundContainers and keeps hot selection/navigation
	// checks O(1).  It is rebuilt whenever a source mutation replaces a branch.
	std::unordered_set<TreeViewItem*> _boundContainerIndex;
	size_t _hierarchyMutationDepth = 0;

	ItemTemplateReference ResolveDataItemTemplate(
		const BindingListReference& source,
		const ItemTemplateReference& localTemplate,
		int level,
		std::wstring* outError = nullptr) const;
	std::unique_ptr<TreeViewItem> CreateGeneratedContainer(
		const BindingListReference& source,
		const BindingSourceReference& item,
		const ItemTemplateReference& localTemplate,
		const ItemsControl& projectionOwner,
		CompiledBindingPathView displayMemberPath,
		const std::wstring& containerStyle,
		int level,
		std::wstring* outError);
	void PrepareHierarchyMutation();
	void CompleteHierarchyMutation();
	void RefreshHierarchy(bool restoreSelection = true);
	void BindExpandedSubtree(TreeViewItem& parent);
	void CollectMaterializedChildren(
		TreeViewItem* parent,
		std::vector<TreeViewItem*>& output) const;
	void CollectContainers(
		TreeViewItem* parent,
		int level,
		std::vector<TreeViewItem*>& output);
	void CollectVisibleContainers(std::vector<TreeViewItem*>& output);
	bool ContainsContainer(const TreeViewItem* item) const noexcept;
	bool ApplySelection(TreeViewItem* item, bool bringIntoView);
	bool FocusAndSelectItem(TreeViewItem* item, bool bringIntoView);
	bool HandleSelectionAndCollapsed(TreeViewItem& collapsed);
	void RestoreFocusOnPointerDown();
	bool ProcessItemNavigationKey(
		TreeViewItem* origin, const InputReport& input);
	bool HandleScrollKey(Key key);
	bool ExpandSubtree(TreeViewItem* item);
	ScrollViewer* GetScrollHost() noexcept;
	void RefreshSelectedItemObservation();
	void NotifySelectionProjectionChanged(bool itemChanged);
	bool BringItemIntoView(TreeViewItem& item, bool expandAncestors);
	void UpdateHover(TreeViewItem* item);
	void RebuildAuthoredDataDescendants();
	void DrawDropIndicator();
	bool AuthoredHierarchyMigrationInProgress() const noexcept
	{
		return IsAuthoredItemsMigrationInProgress();
	}
	void SetHierarchyError(std::wstring value)
	{
		SetLastTemplateError(std::move(value));
	}
#if CUI_ENABLE_DYNAMIC_XAML
	ItemTemplateReference ResolveAuthoredImplicitItemTemplate(
		const BindingListReference& source) const;
	void AppendAuthoredItemTypeDiagnostic(
		const BindingListReference& source,
		std::wstring& error) const;
	void ApplyAuthoredGeneratedContainerProjection(
		TreeViewItem& container,
		const ItemsControl& projectionOwner) const;
	bool HasAuthoredSelectedValuePath() const noexcept;
	BindingValue ReadAuthoredSelectedValue() const;
	BindingPathObservation ObserveAuthoredSelectedValuePath(
		std::function<void()> changed) const;
#endif

	friend class TreeViewItem;
};
