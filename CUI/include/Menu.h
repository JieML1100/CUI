#pragma once
#include "HeaderedItemsControl.h"
#include "Separator.h"
#include <cstdint>
#include <vector>

class Popup;

/** WPF role used by one MenuItem template for menu-bar and popup rows. */
enum class MenuItemRole : int
{
	TopLevelItem = 0,
	TopLevelHeader,
	SubmenuItem,
	SubmenuHeader
};

/**
 * @file Menu.h
 * @brief Menu/MenuItem：菜单栏与下拉菜单控件。
 *
 * 设计：
 * - Menu 的 Items 是顶层 MenuItem（菜单栏项）。
 * - 每个 MenuItem 也是 HeaderedItemsControl，其 Items 形成多级子菜单。
 * - MenuItem 的 ControlTemplate 通过 Popup + ItemsPresenter 呈现子菜单；
 *   native class 只维护 role/open/highlight/command 行为。
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
		return std::make_unique<MenuItemAutomationPeer>(*this);
	}

private:
	static const DependencyPropertyKey& IsHighlightedPropertyKey();
	static const DependencyPropertyKey& IsPressedPropertyKey();
	static const DependencyPropertyKey& RolePropertyKey();

	MenuItem* _parentItem = nullptr;
	std::vector<Control*> _items;
	std::vector<ControlWeakReference> _generatedItemsRebuildSnapshot;
	bool _generatedItemsRebuildPending = false;
	std::function<void()> _structureChanged;
	std::function<void(MenuItem&)> _interactionStateChanged;
	std::wstring _command;
	std::wstring _commandParameter;
	std::wstring _inputGestureText;
	BindingValue _icon;
	bool _isCheckable = false;
	bool _isChecked = false;
	bool _staysOpenOnClick = false;
	bool _isHighlighted = false;
	bool _isPressed = false;
	bool _isSubmenuOpen = false;
	bool _projectingInteractionState = false;
	bool _pointerPressActive = false;
	MenuItemRole _role = MenuItemRole::TopLevelItem;
	Popup* _submenuPopup = nullptr;
	EventConnection _submenuPopupOpened;
	EventConnection _submenuPopupClosed;
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
		Window* window,
		ControlWeakReference defaultCommandTarget,
		bool commandRouteChanged = false);
	void RefreshCommandSource();
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	friend class Menu;
	friend class ContextMenu;
	friend class MenuItemAutomationPeer;
	bool ValidateAuthoredItemControl(
		const Control& item, std::string& error) const override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnBeforeGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRebuilt() override;
	std::unique_ptr<Control> WrapGeneratedItem(
		std::unique_ptr<Control> visual,
		const BindingSourceReference& item,
		size_t index) override;
	std::unique_ptr<Panel> CreateItemsHost() const override;
	void ConfigureHeaderVisual(Control& child) override;
	void ReleaseHeaderVisual(Control& child) override;
	void SynchronizeItems();
	void SynchronizeItemsHostPresentation();
	void OnControlTemplatePresentationChanged() override;
	void OnApplyTemplate() override;
	void SetIsHighlightedCore(bool value);
	void SetIsSubmenuOpenCore(bool value);
	void UpdateRole();
	Popup* ResolveSubmenuPopup() const noexcept;
	bool ItemsHostUsesSubmenuPopup() const noexcept;
	void ConfigureSubmenuPopup(Popup* popup);
	bool ShouldOpenSubmenuOnPointerMove() const noexcept;
	bool IsInMenuMode() const noexcept;
	Control* ResolveMenuHost() const noexcept;
	MenuItem* FindNavigableChild(bool last) const noexcept;
	MenuItem* FindNavigableSibling(
		int direction, bool edge = false) const noexcept;
	MenuItem* RootTopLevelItem() noexcept;
	bool FocusForMenuNavigation();
	bool FocusSiblingFromKeyboard(int direction, bool edge = false);
	bool OpenSubmenuFromKeyboard(bool selectFirst);
	bool CloseKeyboardLevel();
	bool InvokeLeafAndDismiss();
	void SetIsPressedCore(bool value);

public:
	using UIElement::Click;
	using UIElement::Checked;
	using UIElement::Unchecked;
	using UIElement::SubmenuOpened;
	using UIElement::SubmenuClosed;

	virtual UIClass Type() override;
	static const DependencyProperty& CommandProperty();
	static const DependencyProperty& CommandParameterProperty();
	static const DependencyProperty& InputGestureTextProperty();
	static const DependencyProperty& IconProperty();
	static const DependencyProperty& IsCheckableProperty();
	static const DependencyProperty& IsCheckedProperty();
	static const DependencyProperty& StaysOpenOnClickProperty();
	static const DependencyProperty& IsHighlightedProperty();
	static const DependencyProperty& IsPressedProperty();
	static const DependencyProperty& RoleProperty();
	static const DependencyProperty& IsSubmenuOpenProperty();
	static const DependencyProperty& CommandTargetProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	bool DefaultSelectOnLeftButtonDown() const override { return false; }
	bool HitTestChildren() const override { return false; }
	void SetHeader(BindingValue value) override;
	/** @brief XAML 定义的路由命令 identity。 */
	PROPERTY(std::wstring, Command);
	GET(std::wstring, Command);
	SET(std::wstring, Command);
	/** @brief XAML 标量命令参数。 */
	PROPERTY(std::wstring, CommandParameter);
	GET(std::wstring, CommandParameter);
	SET(std::wstring, CommandParameter);
	/** WPF MenuItem.Icon data slot; primitives are presented by the template. */
	BindingValue GetIcon() const { return _icon; }
	void SetIcon(BindingValue value);
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
	/** WPF read-only press state for pointer interaction. */
	READONLY_PROPERTY(bool, IsPressed);
	GET(bool, IsPressed);
	/** WPF template role: top-level item/header or submenu item/header. */
	READONLY_PROPERTY(MenuItemRole, Role);
	GET(MenuItemRole, Role);
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
	bool OnAccessKey(bool isMultiple) override;
	bool HandlesNavigationKey(Key key) const override;
	bool ProcessInput(const InputReport& input) override;
};

/**
 * @brief 菜单控件。
 *
 * The menu bar is an ordinary ItemsControl. Each header owns its Popup through
 * the MenuItem ControlTemplate.
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
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
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
	void ClosePopup();
	bool IsMenuModeActive() const noexcept { return _expand; }
	cui::core::Size GetRenderSizeDip() override;
protected:
	void PreparePresentation() override;
	bool ProcessInput(const InputReport& input) override;
};

