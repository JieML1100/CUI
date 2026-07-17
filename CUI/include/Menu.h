#pragma once
#include "Control.h"
#include "ObservableCollection.h"
#include <vector>

typedef Event<void(class Control*, int)> MenuCommandEvent;

/**
 * @file Menu.h
 * @brief Menu/MenuItem：菜单栏与下拉菜单控件。
 *
 * 设计：
 * - Menu 是复合控件：顶层子控件为 MenuItem（菜单栏项）。
 * - 每个 MenuItem 可通过 SubItems 形成多级子菜单。
 * - Menu 会根据鼠标 hover/open 路径绘制下拉面板，并在点击叶子项时触发 OnMenuCommand。
 */

/**
 * @brief 菜单项。
 *
 * 所有权：
 * - MenuItem::SubItems 由该 MenuItem 拥有；~MenuItem 会 delete 所有 SubItems。
 * - 通过 AddSubItem/AddSeparator 创建的项无需外部释放。
 */
class MenuItem : public Control
{
private:
	MenuItem* _parentItem = nullptr;
	std::vector<MenuItem*> _observedSubItems;
	std::function<void()> _structureChanged;
	bool CanAdopt(const MenuItem* item) const noexcept;
	void OnSubItemsChanged(const CollectionChangedEventArgs& change);
	void SetStructureChangedHandler(std::function<void()> handler);
	friend class Menu;
	friend class ContextMenu;

public:
	using SubItemCollection = ObservableCollection<MenuItem*>;
	virtual UIClass Type() override;
	bool DefaultSelectOnLeftButtonDown() const override { return false; }
	/** @brief 业务命令 Id（由调用方定义，0 通常表示无命令）。 */
	int Id = 0;
	/** @brief 是否为分隔符（Separator=true 时通常不可交互）。 */
	bool Separator = false;
	/** @brief 快捷键显示文本（仅展示，不自动绑定热键）。 */
	std::wstring Shortcut;
	/**
	 * @brief 可观察子菜单集合（集合内对象由本 MenuItem 拥有）。
	 *
	 * 直接 erase 会分离对象并把释放责任交给调用方；需要明确转移所有权时
	 * 优先使用 DetachSubItemAt/RemoveSubItem/ClearSubItems。
	 */
	SubItemCollection SubItems;

	MenuItem(std::wstring text = L"", int id = 0);
	~MenuItem();

	/**
	 * @brief 添加一个子菜单项。
	 * @return 新创建的子项指针（所有权属于本 MenuItem）。
	 */
	MenuItem* AddSubItem(std::wstring text, int id = 0);
	MenuItem* AddSubItem(std::unique_ptr<MenuItem> item);
	MenuItem* InsertSubItem(
		int index, std::unique_ptr<MenuItem> item);
	/** @brief 添加一个分隔符子项。 */
	MenuItem* AddSeparator();
	std::unique_ptr<MenuItem> DetachSubItemAt(int index);
	std::unique_ptr<MenuItem> DetachSubItem(MenuItem* item);
	bool RemoveSubItemAt(int index);
	bool RemoveSubItem(MenuItem* item);
	void ClearSubItems();
	MenuItem* GetSubItem(int index) const noexcept;
	int IndexOfSubItem(const MenuItem* item) const noexcept;
	MenuItem* ParentItem() const noexcept { return _parentItem; }
	/** @brief 创建一个分隔符项。 */
	static MenuItem* CreateSeparator();

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

	D2D1_COLOR_F HoverBackColor = D2D1_COLOR_F{ 0.20f,0.46f,0.90f,0.18f };
	D2D1_COLOR_F ActiveBackColor = D2D1_COLOR_F{ 0.20f,0.46f,0.90f,0.26f };
	float CornerRadius = 6.0f;
};

/**
 * @brief 菜单控件。
 *
 * 通常作为 Form::MainMenu 使用：
 * - Menu::Update 会尝试将 ParentForm->MainMenu 指向自身
 * - 下拉面板的绘制高度可能覆盖整个 Client 区（用于捕获/处理鼠标）
 */
class Menu : public Control
{
private:
	bool _expand = false;
	int _expandIndex = -1;
	int _hoverTopIndex = -1;
	std::vector<int> _hoverPath;
	std::vector<int> _openPath;
	float _popupProgress = 0.0f;
	float _popupStartProgress = 0.0f;
	float _popupTargetProgress = 0.0f;
	ULONGLONG _popupAnimStartTick = 0;
	bool _popupAnimating = false;

	float ItemPaddingX = 10.0f;
	float DropPaddingY = 6.0f;

	float DropLeftLocal();
	float DropTopLocal() { return (float)BarHeight; }
	float DropWidthLocal();
	float DropHeightLocal();
	int DropCount();
	bool HasSubMenu(int dropIndex);
	float CurrentPopupProgress();
	void BeginPopupReveal(float startProgress = 0.08f);
	void OnItemTreeChanged();
	void AttachItemTree(MenuItem* item);
	bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren) override;

public:
	virtual UIClass Type() override;
	bool DefaultSelectOnLeftButtonDown() const override { return false; }

	/**
	 * @brief 菜单命令事件。
	 *
	 * 当用户点击一个“叶子”菜单项（无子菜单且非分隔符）时触发。
	 * 参数为 MenuItem::Id。
	 */
	MenuCommandEvent OnMenuCommand;

	int BarHeight = 28;
	int DropItemHeight = 26;
	float BorderThickness = 1.0f;

	D2D1_COLOR_F BarBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F BarBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F BarItemHoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F BarItemActiveColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F DropBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F DropBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F DropHoverColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F DropTextColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F DropSeparatorColor = cui::theme::palette::Border;
	float BarItemCornerRadius = 6.0f;
	float DropCornerRadius = 8.0f;
	float DropItemCornerRadius = 6.0f;
	float DropItemHorizontalInset = 6.0f;
	UINT PopupAnimationDurationMs = 95;

	Menu(int x, int y, int width, int height = 28);

	/**
	 * @brief 添加一个顶层菜单项（菜单栏项）。
	 * @return 新建 MenuItem 指针（所有权属于 Menu）。
	 */
	MenuItem* AddItem(std::wstring text);
	MenuItem* AddItem(std::unique_ptr<MenuItem> item);
	MenuItem* InsertItem(int index, std::wstring text);
	MenuItem* InsertItem(int index, std::unique_ptr<MenuItem> item);
	MenuItem* AddSeparator();
	MenuItem* GetItem(int index) const noexcept;
	int IndexOfItem(const MenuItem* item) const noexcept;
	std::unique_ptr<MenuItem> DetachItemAt(int index);
	std::unique_ptr<MenuItem> DetachItem(MenuItem* item);
	bool RemoveItemAt(int index);
	bool RemoveItem(MenuItem* item);
	void ClearItems();

	bool ContainsPoint(int localX, int localY) override;
	bool AutoCloseOnOutsideClick() const override { return _expand; }
	bool AutoCloseOnFormFocusLoss() const override { return _expand; }
	void ClosePopup() override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};

