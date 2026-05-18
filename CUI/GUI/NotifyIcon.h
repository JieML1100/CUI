#pragma once
#include <string>
#include <vector>
#include "Control.h"
#include <Shellapi.h>

/**
 * @file NotifyIcon.h
 * @brief NotifyIcon：系统托盘图标与右键菜单封装。
 *
 * 说明：
 * - 使用 Win32 NOTIFYICONDATAA 与 Shell_NotifyIcon（实现见 cpp）
 * - 需要先 InitNotifyIcon 绑定宿主窗口句柄与资源 ID，再 ShowNotifyIcon
 * - 菜单点击与鼠标事件通过 OnNotifyIconMenuClick / OnNotifyIconMouseDown 对外抛出
 */

class NotifyIconMenuItem
{
public:
	/** @brief 显示文本（ANSI）。 */
    std::string Text;
	/** @brief 菜单项 ID（用于点击回调与 Enable/SetText）。 */
    int ID;
	/** @brief 是否可用。 */
    bool Enabled;
	/** @brief 是否为分隔线项。 */
    bool Separator;
	/** @brief 是否包含子菜单。 */
    bool HasSubMenu;
	/** @brief 子菜单句柄（若 HasSubMenu=true）。 */
    HMENU SubMenu;
	/** @brief 子菜单项（值语义存储）。 */
    std::vector<NotifyIconMenuItem> SubItems;

    NotifyIconMenuItem(const std::string& text, int id, bool enabled = true)
        : Text(text), ID(id), Enabled(enabled), Separator(false), HasSubMenu(false), SubMenu(NULL) {
    }

    static NotifyIconMenuItem CreateSeparator();
    
	/** @brief 添加一个子菜单项（仅修改数据结构；实际 HMENU 构建由实现负责）。 */
    void AddSubItem(const NotifyIconMenuItem& item);
};

class NotifyIcon
{
private:
    NOTIFYICONDATAA NotifyIconData;
    int iconID;
    HMENU popupMenu;
    std::vector<NotifyIconMenuItem> menuItems;

public:
	/** @brief 关联的宿主窗口句柄。 */
    HWND hWnd;
	/** @brief 单例指针（由实现维护）。 */
    static class NotifyIcon* Instance;
    NotifyIcon();
    ~NotifyIcon();
	/** @brief 初始化托盘图标数据（绑定窗口与资源 ID）。 */
    void InitNotifyIcon(HWND hWnd, int iconID);
	/** @brief 设置托盘图标句柄。 */
    void SetIcon(HICON hIcon);
	/** @brief 显示托盘图标。 */
    void ShowNotifyIcon();
	/** @brief 隐藏托盘图标。 */
    void HideNotifyIcon();
	/** @brief 设置提示文本（tooltip）。 */
    void SetToolTip(const char* text);
	/** @brief 显示气泡提示。 */
    void ShowBalloonTip(const char* title, const char* text, int timeout = 5000, int type = NIIF_INFO);

	/** @brief 添加菜单项。 */
    void AddMenuItem(const NotifyIconMenuItem& item);
	/** @brief 添加分隔线。 */
    void AddMenuSeparator();
	/** @brief 在屏幕坐标 (x,y) 处显示上下文菜单。 */
    void ShowContextMenu(int x, int y);
	/** @brief 清空菜单项。 */
    void ClearMenu();
	/** @brief 启用/禁用指定菜单项。 */
    void EnableMenuItem(int id, bool enable);
	/** @brief 设置指定菜单项显示文本。 */
    void SetMenuItemText(int id, const std::string& text);

    
	/** @brief 创建一个子菜单根项并返回其指针（用于后续 AddSubMenuItem）。 */
    NotifyIconMenuItem* CreateSubMenu(const std::string& text);
	/** @brief 向指定父菜单项追加子项。 */
    void AddSubMenuItem(int parentId, const NotifyIconMenuItem& item);

    
    NotifyIconMenuItem* FindMenuItem(int id, std::vector<NotifyIconMenuItem>& items);
    NotifyIconMenuItem* FindMenuItem(int id);

    
	/** @brief 托盘图标鼠标按下事件。 */
    typedef Event<void(class NotifyIcon*, MouseEventArgs)> NotifyIconMouseDownEvent;
    NotifyIconMouseDownEvent OnNotifyIconMouseDown;

	/** @brief 菜单点击事件，参数为菜单项 ID。 */
    typedef Event<void(class NotifyIcon*, int)> NotifyIconMenuClickEvent;
    NotifyIconMenuClickEvent OnNotifyIconMenuClick;
};
