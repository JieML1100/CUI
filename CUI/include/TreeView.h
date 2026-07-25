#pragma once

#include "ControlTemplate.h"
#include "HeaderedItemsControl.h"

#include <functional>

class TreeView;

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
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

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

	BindingListReference GetItemsSource() const noexcept override;
	void SetItemsSource(BindingListReference value) override;
	size_t ItemCount() const noexcept override;
	TreeViewItem* ContainerFromIndex(size_t index) const noexcept;

protected:
	void OnRender() override;
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
	cui::core::Insets GetHeaderPresentationInsets() const noexcept override;
	cui::core::Insets GetItemsPresentationInsets() const noexcept override;
	void OnIsMouseOverChanged(bool previous, bool current) override;

private:
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
	int _level = 0;
	uint32_t _accessibilityId = 0;
	Event<void(TreeViewItem*)> _hasItemsChanged;
	Event<void(TreeViewItem*)> _levelChanged;
	Event<void(TreeViewItem*)> _selectedChanged;
	Event<void(TreeViewItem*)> _expandedChanged;

	bool InitializeGenerated(
		TreeView& owner,
		const BindingSourceReference& item,
		ItemTemplateReference headerTemplate,
		const std::wstring& displayMemberPath,
		int level,
		std::wstring* outError);
	bool RefreshHierarchicalItemsSource(std::wstring* outError = nullptr);
	void RefreshHierarchicalCollectionObservation();
	bool EnsureChildrenRealized();
	void BindHierarchy(TreeView& owner, TreeViewItem* parent, int level);
	void UnbindHierarchy() noexcept;
	void ApplyIsExpandedValue(bool value);
	void SetCurrentIsExpanded(bool value);
	void SyncHasItems();
	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(bool value);
	void ApplyExpansionPresentation();
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
	using ImplicitItemTemplateResolver = std::function<ItemTemplateReference(
		const std::wstring& itemTypeName)>;

	TreeView();
	~TreeView() override;
	UIClass Type() override { return UIClass::UI_TreeView; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	void SetItemsSource(BindingListReference value) override;
	void SetItemTemplate(ItemTemplateReference value) override;
	void SetDisplayMemberPath(std::wstring value) override;
	BindingValue GetSelectedItem() const;
	BindingValue GetSelectedValue() const;
	TreeViewItem* GetSelectedContainer() const noexcept
	{
		return _selectedContainer;
	}
	TreeViewItem* ContainerFromIndex(size_t index) const noexcept;
	const std::wstring& GetSelectedValuePath() const noexcept
	{
		return _selectedValuePath;
	}
	void SetSelectedValuePath(std::wstring value);
	bool SelectItem(TreeViewItem* item, bool bringIntoView = true);
	void SetImplicitItemTemplateResolver(ImplicitItemTemplateResolver value);
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
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Tree, L"TreeView");
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
	TreeViewItem* _selectedContainer = nullptr;
	TreeViewItem* _hoveredContainer = nullptr;
	TreeViewItem* _dropTarget = nullptr;
	TreeViewDropPosition _dropPosition = TreeViewDropPosition::None;
	ControlTemplateReference _itemContainerTemplate;
	std::wstring _selectedValuePath;
	BindingPathObservation _selectedItemObservation;
	Event<void(TreeView*)> _selectedItemChanged;
	Event<void(TreeView*)> _selectedValueChanged;
	ImplicitItemTemplateResolver _implicitItemTemplateResolver;
	BindingSourceReference _selectionRestoreIdentity;
	ControlWeakReference _selectionRestoreAuthored;
	std::vector<ControlWeakReference> _boundContainers;
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
		const std::wstring& displayMemberPath,
		const std::wstring& containerStyle,
		int level,
		std::wstring* outError);
	void PrepareHierarchyMutation();
	void CompleteHierarchyMutation();
	void RefreshHierarchy(bool restoreSelection = true);
	void CollectContainers(
		ItemsControl& owner,
		TreeViewItem* parent,
		int level,
		std::vector<TreeViewItem*>& output);
	void CollectVisibleContainers(std::vector<TreeViewItem*>& output);
	bool ContainsContainer(const TreeViewItem* item) const noexcept;
	bool ApplySelection(TreeViewItem* item, bool bringIntoView);
	void RefreshSelectedItemObservation();
	void NotifySelectionProjectionChanged(bool itemChanged);
	bool BringItemIntoView(TreeViewItem& item, bool expandAncestors);
	void UpdateHover(TreeViewItem* item);
	void RebuildAuthoredDataDescendants();
	void DrawDropIndicator();
	void SetHierarchyError(std::wstring value)
	{
		SetLastTemplateError(std::move(value));
	}

	friend class TreeViewItem;
};
