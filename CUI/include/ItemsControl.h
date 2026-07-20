#pragma once

#include "ItemTemplate.h"
#include "GroupStyle.h"
#include "ItemContainerGenerator.h"
#include "ItemsPanelTemplate.h"
#include "ItemsPresenter.h"
#include "ScrollView.h"

/**
 * Generic templated collection presenter.
 *
 * GeneratedItemCount/GetGeneratedItem expose visuals created from ItemsSource
 * and ItemTemplate; the sole direct child is an internal ItemsHost. It is
 * intentionally not a hand-authored container. Item records remain ordinary
 * IBindingSource objects, so bindings inside a template receive the item as
 * their DataContext without introducing an untyped object bag.
 */
class ItemsControl : public ScrollView
{
public:
	ItemsControl(int x = 0, int y = 0, int width = 240, int height = 200);
	UIClass Type() override { return UIClass::UI_ItemsControl; }
	void EnsureBindingPropertiesRegistered() override;

	BindingListReference GetItemsSource() const noexcept { return _itemsSource; }
	void SetItemsSource(BindingListReference value);
	ItemTemplateReference GetItemTemplate() const noexcept { return _itemTemplate; }
	void SetItemTemplate(ItemTemplateReference value);
	GroupStyleReference GetGroupStyle() const noexcept { return _groupStyle; }
	void SetGroupStyle(GroupStyleReference value);
	ItemsPanelTemplateReference GetItemsPanel() const noexcept
	{
		return _itemsPanel;
	}
	void SetItemsPanel(ItemsPanelTemplateReference value);
	const std::wstring& GetDisplayMemberPath() const noexcept
	{
		return _displayMemberPath;
	}
	void SetDisplayMemberPath(std::wstring value);
	const std::wstring& LastTemplateError() const noexcept
	{
		return _lastTemplateError;
	}
	size_t GeneratedItemCount() const noexcept
	{
		return _generator.RealizedCount();
	}
	size_t RecycledItemCount() const noexcept
	{
		return _generator.RecycledCount();
	}
	size_t ItemCount() const noexcept
	{
		return _itemsSource ? _itemsSource.Get()->Count() : 0;
	}
	Control* GetGeneratedItem(size_t index) const noexcept;
	Panel* GetItemsHost() const noexcept { return _itemsHost; }
	ItemsPresenter* GetTemplateItemsPresenter() const noexcept
	{
		return _templateItemsPresenter;
	}
	Control* GetControlTemplateRoot() const noexcept
	{
		return _controlTemplateRoot;
	}
	/** Framework hook used by a ControlTemplate ItemsPresenter slot. */
	bool RegisterTemplateItemsPresenter(ItemsPresenter* presenter);
	Control* SetControlTemplateRoot(std::unique_ptr<Control> value);
	std::unique_ptr<Control> DetachControlTemplateRoot();
	bool IsVirtualizing() const noexcept;
	void Update() override;
	bool ProcessMessage(
		UINT message, WPARAM wParam, LPARAM lParam,
		int localX, int localY) override;
	bool HandlesMouseWheel() const override
	{
		return _controlTemplateRoot == nullptr;
	}
	bool HandlesNavigationKey(WPARAM key) const override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	POINT GetChildrenRenderOffset() const override;
	D2D1_RECT_F GetChildrenClipRect() override;

protected:
	void PerformPendingLayout() override;
	bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	virtual std::unique_ptr<Control> WrapGeneratedItem(
		std::unique_ptr<Control> visual,
		const BindingSourceReference& item,
		size_t index);
	virtual std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation);
	virtual void OnBeforeGeneratedItemsRebuilt() {}
	virtual void OnGeneratedItemsRebuilt() {}
	virtual void OnGeneratedItemsRealized() {}
	virtual void OnGeneratedItemIndexChanged(
		Control&, size_t, size_t) {}
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

private:
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
	ItemTemplateReference _itemTemplate;
	GroupStyleReference _groupStyle;
	ItemsPanelTemplateReference _itemsPanel;
	EventConnection _itemsSourceChanged;
	EventConnection _groupsChanged;
	EventConnection _scrollChanged;
	ItemContainerGenerator _generator;
	std::wstring _displayMemberPath;
	std::wstring _lastTemplateError;
	Panel* _itemsHost = nullptr;
	ItemsPresenter* _templateItemsPresenter = nullptr;
	Control* _controlTemplateRoot = nullptr;
	ScrollView* _itemsScrollOwner = nullptr;
	std::unique_ptr<Panel> _detachedItemsHost;
	bool _changingItemsHost = false;
	bool _changingTemplateInfrastructure = false;
	bool _realizingViewport = false;
	bool _applyingCollectionChange = false;
	EventConnection _itemsPresenterParentChanged;

	const ItemsPanelTemplate& EffectiveItemsPanel() const noexcept;
	std::unique_ptr<Panel> CreateItemsHost() const;
	bool ReplaceItemsHost(ItemsPanelTemplateReference value);
	std::unique_ptr<Panel> TakeItemsHost();
	void PlaceItemsHost(std::unique_ptr<Panel> host);
	void RefreshItemsScrollOwner();
	ScrollView* ItemsScrollOwner() const noexcept
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
	bool IsGroupingActive() const noexcept;
	PreparedGroupHeaders BuildGroupHeaders(
		size_t index, const BindingSourceReference& item);
	void RefreshGroupHeaders();
	static Control* UnwrapGeneratedItem(Control* visual) noexcept;
	void RefreshGeneratedItem(
		const std::weak_ptr<IBindingSource>& itemIdentity);
};
