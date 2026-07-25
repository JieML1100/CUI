#pragma once
#include "HeaderedItemsControl.h"
#include "Separator.h"
#include <cstdint>
#include <vector>

/**
 * @file Menu.h
 * @brief Menu/MenuItem：菜单栏与下拉菜单控件。
 *
 * 设计：
 * - Menu 的 Items 是顶层 MenuItem（菜单栏项）。
 * - 每个 MenuItem 也是 HeaderedItemsControl，其 Items 形成多级子菜单。
 * - Menu 会根据鼠标 hover/open 路径绘制下拉面板，叶子项统一执行 RoutedCommand。
 */

/**
 * @brief 菜单项。
 *
 * 所有权完全遵循 ItemsControl：authored Items 与 ItemsSource 互斥，
 * authored MenuItem 的逻辑父级是当前 MenuItem，视觉父级是 ItemsHost。
 * 外观、尺寸和 popup 动画属于主题/模板实现，不是 MenuItem 的公共状态。
 */
class MenuItem : public HeaderedItemsControl
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<InvokeAutomationPeer>(
			*this, AutomationControlType::MenuItem, L"MenuItem");
	}

private:
	MenuItem* _parentItem = nullptr;
	std::vector<Control*> _items;
	std::function<void()> _structureChanged;
	std::function<void(MenuItem&)> _interactionStateChanged;
	std::wstring _command;
	std::wstring _commandParameter;
	std::wstring _inputGestureText;
	bool _isCheckable = false;
	bool _isChecked = false;
	bool _staysOpenOnClick = false;
	bool _isHighlighted = false;
	bool _isSubmenuOpen = false;
	bool _projectingInteractionState = false;
	ControlWeakReference _commandTarget;
	ControlWeakReference _defaultCommandTarget;
	EventConnection _commandCanExecuteConnection;
	std::uint64_t _commandSourceRefreshVersion = 0;
	void ApplyCommandTarget(const ControlWeakReference& value);
	ControlWeakReference EffectiveCommandTarget() const noexcept
	{
		return _commandTarget.HasValue()
			? _commandTarget : _defaultCommandTarget;
	}
	void SetStructureChangedHandler(std::function<void()> handler);
	void SetInteractionStateChangedHandler(
		std::function<void(MenuItem&)> handler);
	void SynchronizeCommandContext(
		Window* window, ControlWeakReference defaultCommandTarget);
	void RefreshCommandSource();
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	friend class Menu;
	friend class ContextMenu;
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	std::unique_ptr<Control> WrapGeneratedItem(
		std::unique_ptr<Control> visual,
		const BindingSourceReference& item,
		size_t index) override;
	void SynchronizeItems();
	void SuppressItemsPresentation();
	void OnControlTemplatePresentationChanged() override;
	void ConfigureHeaderVisual(Control& child) override;
	void ReleaseHeaderVisual(Control& child) override;
	void SetIsHighlightedCore(bool value);
	void SetIsSubmenuOpenCore(bool value);

public:
	using UIElement::Click;
	using UIElement::Checked;
	using UIElement::Unchecked;
	using UIElement::SubmenuOpened;
	using UIElement::SubmenuClosed;

	virtual UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
	bool DefaultSelectOnLeftButtonDown() const override { return false; }
	void SetHeader(BindingValue value) override;
	/** @brief XAML 定义的路由命令 identity。 */
	PROPERTY(std::wstring, Command);
	GET(std::wstring, Command);
	SET(std::wstring, Command);
	/** @brief XAML 标量命令参数。 */
	PROPERTY(std::wstring, CommandParameter);
	GET(std::wstring, CommandParameter);
	SET(std::wstring, CommandParameter);
	/** Whether invocation toggles the WPF check state. */
	PROPERTY(bool, IsCheckable);
	GET(bool, IsCheckable);
	SET(bool, IsCheckable);
	/** WPF MenuItem check state; independent from transient popup-open state. */
	PROPERTY(bool, IsChecked);
	GET(bool, IsChecked);
	SET(bool, IsChecked);
	/** Keeps the owning Menu/ContextMenu open after a leaf invocation. */
	PROPERTY(bool, StaysOpenOnClick);
	GET(bool, StaysOpenOnClick);
	SET(bool, StaysOpenOnClick);
	/** Framework-projected pointer/keyboard highlight state. */
	READONLY_PROPERTY(bool, IsHighlighted);
	GET(bool, IsHighlighted);
	/** WPF submenu state; interaction uses SetCurrentValue semantics. */
	PROPERTY(bool, IsSubmenuOpen);
	GET(bool, IsSubmenuOpen);
	SET(bool, IsSubmenuOpen);
	bool IsCheckedForAccessibility() const noexcept override
	{
		return _isChecked;
	}
	/** Optional authored target overriding the owning popup/menu target. */
	PROPERTY(class Control*, CommandTarget);
	GET(class Control*, CommandTarget);
	SET(class Control*, CommandTarget);
	bool HasAuthoredCommandTarget() const noexcept
	{
		return _commandTarget.HasValue();
	}
	/** Removes the authored override and resumes host/default target resolution. */
	void ClearCommandTarget();
	/** @brief 菜单中的手势提示；实际触发由同名 KeyBinding 负责。 */
	PROPERTY(std::wstring, InputGestureText);
	GET(std::wstring, InputGestureText);
	std::wstring GetInputGestureText() const { return _inputGestureText; }
	SET(std::wstring, InputGestureText);
	MenuItem();
	~MenuItem() override;

	/** Adds an explicitly constructed MenuItem to this item's Items. */
	MenuItem* AddSubItem(std::unique_ptr<MenuItem> item);
	MenuItem* InsertSubItem(
		int index, std::unique_ptr<MenuItem> item);
	/** @brief 添加一个分隔符子项。 */
	Separator* AddSeparator();
	std::unique_ptr<Control> DetachSubItemAt(int index);
	std::unique_ptr<MenuItem> DetachSubItem(MenuItem* item);
	bool RemoveSubItemAt(int index);
	bool RemoveSubItem(MenuItem* item);
	void ClearSubItems();
	MenuItem* GetSubItem(int index) const noexcept;
	int IndexOfSubItem(const MenuItem* item) const noexcept;
	std::span<Control* const> GetMenuItemsView() const noexcept
	{
		return { _items.data(), _items.size() };
	}
	MenuItem* ParentItem() const noexcept { return _parentItem; }
	/** Establishes the logical route and command domain for this owned subtree. */
	void AttachCommandHost(
		Control& routedOwner,
		ControlWeakReference defaultCommandTarget = {});
	/** Detaches this owned subtree from one Menu/ContextMenu command domain. */
	void DetachCommandHost(Control& routedOwner);
	bool Invoke() override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
};

/**
 * @brief 菜单控件。
 *
 * The menu bar is an ordinary element. While expanded it uses Window's popup
 * overlay projection so dropdown panels can render and hit-test above content.
 */
class Menu : public ItemsControl
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Menu, L"Menu");
	}

private:
	bool _expand = false;
	int _expandIndex = -1;
	int _hoverTopIndex = -1;
	std::vector<int> _hoverPath;
	std::vector<int> _openPath;
	std::vector<Control*> _items;

	float DropLeftLocal();
	float DropTopLocal() const noexcept { return MenuBarExtent(); }
	float DropWidthLocal();
	float DropHeightLocal();
	int DropCount();
	bool HasSubMenu(int dropIndex);
	float MenuBarExtent() const noexcept;
	void SynchronizeInteractionProjection();
	void OnItemTreeChanged();
	void OnItemInteractionStateChanged(MenuItem& source);
	void AttachItemTree(MenuItem* item);
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	std::unique_ptr<Control> WrapGeneratedItem(
		std::unique_ptr<Control> visual,
		const BindingSourceReference& item,
		size_t index) override;
	void SynchronizeItems();

public:
	virtual UIClass Type() override;
	bool DefaultSelectOnLeftButtonDown() const override { return false; }

	Menu();
	~Menu();

	/** Adds an explicitly constructed MenuItem to the menu's Items. */
	MenuItem* AddItem(std::unique_ptr<MenuItem> item);
	MenuItem* InsertItem(int index, std::unique_ptr<MenuItem> item);
	Separator* AddSeparator();
	MenuItem* GetItem(int index) const noexcept;
	int IndexOfItem(const MenuItem* item) const noexcept;
	std::unique_ptr<Control> DetachItemAt(int index);
	std::unique_ptr<MenuItem> DetachItem(MenuItem* item);
	bool RemoveItemAt(int index);
	bool RemoveItem(MenuItem* item);
	void ClearItems();

	bool ContainsPoint(int localX, int localY) override;
	/** Popup rows are projected by Menu itself, so Menu is their input surface. */
	bool HitTestChildren() const override { return false; }
	void ClosePopup();
	cui::core::Size GetRenderSizeDip() override;
protected:
	void PreparePresentation() override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
};

