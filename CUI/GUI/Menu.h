#pragma once
#include "Control.h"
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
public:
	virtual UIClass Type() override;
	/** @brief 业务命令 Id（由调用方定义，0 通常表示无命令）。 */
	int Id = 0;
	/** @brief 是否为分隔符（Separator=true 时通常不可交互）。 */
	bool Separator = false;
	/** @brief 快捷键显示文本（仅展示，不自动绑定热键）。 */
	std::wstring Shortcut;
	/** @brief 子菜单项列表（由本 MenuItem 拥有并负责释放）。 */
	std::vector<MenuItem*> SubItems;

	MenuItem(std::wstring text = L"", int id = 0);
	~MenuItem();

	/**
	 * @brief 添加一个子菜单项。
	 * @return 新创建的子项指针（所有权属于本 MenuItem）。
	 */
	MenuItem* AddSubItem(std::wstring text, int id = 0);
	/** @brief 添加一个分隔符子项。 */
	MenuItem* AddSeparator();
	/** @brief 创建一个分隔符项。 */
	static MenuItem* CreateSeparator();

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	D2D1_COLOR_F HoverBackColor = D2D1_COLOR_F{ 1,1,1,0.20f };
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

	float ItemPaddingX = 10.0f;
	float DropPaddingY = 6.0f;

	float DropLeftLocal();
	float DropTopLocal() { return (float)BarHeight; }
	float DropWidthLocal();
	float DropHeightLocal();
	int DropCount();
	bool HasSubMenu(int dropIndex);

public:
	virtual UIClass Type() override;

	/**
	 * @brief 菜单命令事件。
	 *
	 * 当用户点击一个“叶子”菜单项（无子菜单且非分隔符）时触发。
	 * 参数为 MenuItem::Id。
	 */
	MenuCommandEvent OnMenuCommand;

	int BarHeight = 28;
	int DropItemHeight = 26;
	float Boder = 1.0f;

	D2D1_COLOR_F BarBackColor = D2D1_COLOR_F{ 1,1,1,0.10f };
	D2D1_COLOR_F BarBorderColor = D2D1_COLOR_F{ 1,1,1,0.18f };
	D2D1_COLOR_F DropBackColor = D2D1_COLOR_F{ 0.12f,0.12f,0.12f,0.92f };
	D2D1_COLOR_F DropBorderColor = D2D1_COLOR_F{ 1,1,1,0.22f };
	D2D1_COLOR_F DropHoverColor = D2D1_COLOR_F{ 1,1,1,0.18f };
	D2D1_COLOR_F DropTextColor = Colors::WhiteSmoke;
	D2D1_COLOR_F DropSeparatorColor = D2D1_COLOR_F{ 1,1,1,0.16f };

	Menu(int x, int y, int width, int height = 28);

	/**
	 * @brief 添加一个顶层菜单项（菜单栏项）。
	 * @return 新建 MenuItem 指针（所有权属于 Menu）。
	 */
	MenuItem* AddItem(std::wstring text);

	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};

