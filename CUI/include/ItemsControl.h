#pragma once

#include "ItemTemplate.h"
#include "GroupStyle.h"
#include "ItemContainerGenerator.h"
#include "ItemsPanelTemplate.h"
#include "ItemsPresenter.h"
#include "ScrollViewer.h"

#include <functional>

namespace cui::framework
{
	struct TemplateAccess;
}

/**
 * Generic templated collection presenter.
 *
 * Items are either authored UIElement instances or records supplied by
 * ItemsSource, never both. In either mode the sole direct visual child is an
 * internal ItemsHost; authored items are logical children of this control and
 * visual children of that host. Item records remain ordinary IBindingSource
 * objects, so bindings inside a template receive the item as their DataContext
 * without introducing an untyped object bag.
 */
class ItemsControl : public Control
{
public:
	using GeneratedContainerInitializer =
		std::function<bool(Control&, std::wstring*)>;

	ItemsControl();
	UIClass Type() override { return UIClass::UI_ItemsControl; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

	virtual BindingListReference GetItemsSource() const noexcept
	{
		return _itemsSource;
	}
	virtual void SetItemsSource(BindingListReference value);
	virtual ItemTemplateReference GetItemTemplate() const noexcept
	{
		return _itemTemplate;
	}
	virtual void SetItemTemplate(ItemTemplateReference value);
	GroupStyleReference GetGroupStyle() const noexcept { return _groupStyle; }
	void SetGroupStyle(GroupStyleReference value);
	ItemsPanelTemplateReference GetItemsPanel() const noexcept
	{
		return _itemsPanel;
	}
	void SetItemsPanel(ItemsPanelTemplateReference value);
	virtual const std::wstring& GetDisplayMemberPath() const noexcept
	{
		return _displayMemberPath;
	}
	virtual void SetDisplayMemberPath(std::wstring value);
	const std::wstring& GetItemContainerStyle() const noexcept
	{
		return _itemContainerStyle;
	}
	void SetItemContainerStyle(std::wstring value);
	/**
	 * Installs the XAML-schema initializer used for lazily generated containers.
	 * The runtime owns schema knowledge; ItemsControl only invokes this boundary
	 * before a generated container enters the logical/visual tree.
	 */
	void SetGeneratedContainerInitializer(
		GeneratedContainerInitializer value);
	const std::wstring& LastTemplateError() const noexcept
	{
		return _lastTemplateError;
	}
	size_t GeneratedItemCount() const noexcept;
	size_t RecycledItemCount() const noexcept
	{
		return _generator.RecycledCount();
	}
	virtual size_t ItemCount() const noexcept;
	Control* GetGeneratedItem(size_t index) const noexcept;
	/**
	 * Adds one authored UIElement item. Authored Items and ItemsSource are
	 * mutually exclusive, matching WPF ItemCollection semantics.
	 */
	Control* AddItemControl(std::unique_ptr<Control> item);
	Control* InsertItemControl(size_t index, std::unique_ptr<Control> item);
	Control* AdoptItemControl(Control* item);
	Control* InsertItemControl(size_t index, Control* item);
	std::unique_ptr<Control> DetachItemControlAt(size_t index);
	std::unique_ptr<Control> DetachItemControl(Control* item);
	bool RemoveItemControlAt(size_t index);
	bool RemoveItemControl(Control* item);
	bool MoveItemControl(size_t oldIndex, size_t newIndex);
	void ClearItemControls();
	Control* GetAuthoredItem(size_t index) const noexcept;
	size_t AuthoredItemCount() const noexcept { return _authoredItems.size(); }

	template<typename T>
	T* AddItem()
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		return static_cast<T*>(AddItemControl(
			std::make_unique<T>()));
	}
	bool IsVirtualizing() const noexcept;
protected:
	void PreparePresentation() override;
	void OnRender() override;
public:
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::List, L"ItemsControl");
	}
	Panel* GetItemsHost() const noexcept { return _itemsHost; }
	ItemsPresenter* GetTemplateItemsPresenter() const noexcept
	{
		return _templateItemsPresenter;
	}
	Control* GetControlTemplateRoot() const noexcept override
	{
		return _controlTemplateRoot;
	}
	/** Framework hook used by a ControlTemplate ItemsPresenter slot. */
	bool RegisterTemplateItemsPresenter(ItemsPresenter* presenter);
	Control* SetControlTemplateRoot(std::unique_ptr<Control> value) override;
	std::unique_ptr<Control> DetachVisualChildTemplateRoot() override;

	class AuthoredItemsUpdateScope final
	{
	public:
		AuthoredItemsUpdateScope(AuthoredItemsUpdateScope&& other) noexcept;
		AuthoredItemsUpdateScope& operator=(
			AuthoredItemsUpdateScope&& other) noexcept;
		AuthoredItemsUpdateScope(const AuthoredItemsUpdateScope&) = delete;
		AuthoredItemsUpdateScope& operator=(
			const AuthoredItemsUpdateScope&) = delete;
		~AuthoredItemsUpdateScope();

	private:
		explicit AuthoredItemsUpdateScope(ItemsControl& owner) noexcept;
		ItemsControl* _owner = nullptr;
		friend class ItemsControl;
	};
	AuthoredItemsUpdateScope DeferAuthoredItemsChanges() noexcept;
	/**
	 * Per-operation token for state owned by a derived item control.
	 *
	 * Source replacement and live-change rollback restore the base source,
	 * subscriptions and generated tree before handing this token back. Tokens
	 * stay local to the operation so rejected reentry cannot overwrite a shared
	 * rollback slot in the derived control.
	 */
	struct ItemsSourceTransactionState
	{
		virtual ~ItemsSourceTransactionState() = default;
	};
	virtual std::unique_ptr<ItemsSourceTransactionState>
		CaptureItemsSourceTransactionState()
	{
		return {};
	}
	virtual void RestoreItemsSourceTransactionState(
		ItemsSourceTransactionState&) noexcept {}
	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	virtual std::unique_ptr<Control> WrapGeneratedItem(
		std::unique_ptr<Control> visual,
		const BindingSourceReference& item,
		size_t index);
	bool InitializeGeneratedContainer(Control& container);
	virtual bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const
	{
		(void)item;
		(void)error;
		return true;
	}
	virtual void OnAuthoredItemsChanged() noexcept {}
	virtual std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation);
	virtual void OnBeforeGeneratedItemsRebuilt() {}
	virtual void OnGeneratedItemsRebuilt() {}
	virtual void OnGeneratedItemsRealized() {}
	virtual void OnGeneratedItemIndexChanged(
		Control&, size_t, size_t) {}
	virtual bool ApplyItemContainerStyle();
	virtual void OnItemsSourceChanged(
		const BindingListReference&,
		const BindingListReference&) {}
	virtual std::unique_ptr<Panel> CreateItemsHost() const;
	/**
	 * A virtualizing host normally realizes all items when it has no viewport.
	 * Popup-backed controls may defer realization until their ItemsPresenter is
	 * attached to a ScrollViewer with a finite viewport.
	 */
	virtual bool ShouldRealizeVirtualItemsWithoutViewport() const noexcept
	{
		return true;
	}
	/** Called after the active ControlTemplate root changes. */
	void OnControlTemplatePresentationChanged() override {}
	/** Replaces the framework ItemsHost before any items are attached. */
	void ReplaceItemsHostCore(std::unique_ptr<Panel> host);
	bool IsItemsSourceUpdateInProgress() const noexcept
	{
		return _itemsSourceUpdateDepth != 0;
	}
	bool IsChangingItemsInfrastructure() const noexcept
	{
		return _changingItemsHost || _changingTemplateInfrastructure;
	}
	/** Effective records consumed by the generator. A derived control may use
	 *  an internal authored-items view without exposing it as ItemsSource. */
	const BindingListReference& GetItemsView() const noexcept
	{
		return _itemsSource;
	}
	bool BringItemIntoView(size_t index);
	const ItemContainerGenerator::RealizedMap& GetRealizedItems() const noexcept
	{
		return _generator.RealizedItems();
	}
	bool RebuildGeneratedItems();
	void SetLastTemplateError(std::wstring value)
	{
		_lastTemplateError = std::move(value);
	}
	std::wstring& MutableLastTemplateError() noexcept
	{
		return _lastTemplateError;
	}
	/**
	 * Commits canonical ItemsControl property storage for a derived control
	 * whose projection is not the flat ItemContainerGenerator pipeline.
	 * The derived projection remains responsible for subscriptions, rollback
	 * and visual realization; the public property still has exactly one value.
	 */
	void SetCustomProjectionItemsSource(BindingListReference value);
	void SetCustomProjectionItemTemplate(ItemTemplateReference value) noexcept
	{
		_itemTemplate = std::move(value);
	}
	void SetCustomProjectionDisplayMemberPath(std::wstring value)
	{
		_displayMemberPath = std::move(value);
	}

private:
	friend struct cui::framework::TemplateAccess;

	struct PreparedItem final
	{
		size_t Index = 0;
		std::unique_ptr<Control> Visual;
		BindingPathObservation Observation;
		bool WasRecycled = false;
	};
	struct PreparedGroupHeaders final
	{
		std::vector<std::unique_ptr<Control>> Visuals;
		std::vector<BindingSourceReference> Contexts;
	};

	BindingListReference _itemsSource;
	// Immutable projection of the last ItemsSource state whose generated tree
	// committed successfully. A live collection notification cannot generally
	// be undone by IBindingList, so this snapshot is the rollback value when a
	// newly added/replaced item cannot be materialized.
	BindingListReference _materializedItemsSourceSnapshot;
	ItemTemplateReference _itemTemplate;
	GroupStyleReference _groupStyle;
	ItemsPanelTemplateReference _itemsPanel;
	EventConnection _itemsSourceChanged;
	EventConnection _groupsChanged;
	EventConnection _scrollChanged;
	ItemContainerGenerator _generator;
	std::vector<Control*> _authoredItems;
	std::wstring _displayMemberPath;
	std::wstring _itemContainerStyle;
	std::wstring _lastTemplateError;
	GeneratedContainerInitializer _generatedContainerInitializer;
	Panel* _itemsHost = nullptr;
	ItemsPresenter* _templateItemsPresenter = nullptr;
	Control* _controlTemplateRoot = nullptr;
	ScrollViewer* _itemsScrollOwner = nullptr;
	std::unique_ptr<Panel> _detachedItemsHost;
	bool _changingItemsHost = false;
	bool _changingTemplateInfrastructure = false;
	bool _itemsLayoutPending = true;
	bool _realizingViewport = false;
	bool _applyingCollectionChange = false;
	size_t _itemsSourceUpdateDepth = 0;
	size_t _authoredItemsUpdateDepth = 0;
	bool _authoredItemsChangedPending = false;
	EventConnection _itemsPresenterParentChanged;

	const ItemsPanelTemplate& EffectiveItemsPanel() const noexcept;
	bool ReplaceItemsHost(ItemsPanelTemplateReference value);
	std::unique_ptr<Panel> TakeItemsHost();
	void PlaceItemsHost(std::unique_ptr<Panel> host);
	void RefreshItemsScrollOwner();
	ScrollViewer* ItemsScrollOwner() const noexcept
	{
		return _itemsScrollOwner;
	}
	bool PrepareGeneratedItem(
		size_t index, PreparedItem& output, bool allowRecycle = true);
	void AttachPreparedItem(PreparedItem&& item);
	void ReorderRealizedChildren();
	void ClearRealizedItems(bool keepForRecycle);
	bool RealizeVirtualViewport();
	bool RealizeVirtualRange(size_t first, size_t last);
	std::pair<size_t, size_t> VirtualRangeForViewport() const noexcept;
	std::pair<size_t, size_t> VirtualRangeForOffset(
		float offset) const noexcept;
	void TrimRecyclePool(size_t first, size_t last);
	void ConfigureVirtualHost();
	bool ApplyCollectionChange(const CollectionChangedEventArgs& change);
	void HandleItemsSourceChange(const CollectionChangedEventArgs& change);
	bool IsGroupingActive() const noexcept;
	PreparedGroupHeaders BuildGroupHeaders(
		size_t index, const BindingSourceReference& item);
	void RefreshGroupHeaders();
	static Control* UnwrapGeneratedItem(Control* visual) noexcept;
	void BeginAuthoredItemsUpdate() noexcept;
	void EndAuthoredItemsUpdate() noexcept;
	void NotifyAuthoredItemsChanged();
	void RefreshGeneratedItem(
		const std::weak_ptr<IBindingSource>& itemIdentity);
};
