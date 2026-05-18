#pragma once
#include <shobjidl.h>

#pragma comment(lib, "Comctl32.lib")

/**
 * @file Taskbar.h
 * @brief Taskbar：Windows 任务栏进度条封装（ITaskbarList3）。
 *
 * 典型用法：
 * - 用窗口句柄构造 Taskbar
 * - SetValue(value, total) 在任务栏按钮上显示进度
 */
class Taskbar
{
    static ITaskbarList3* pTaskbarList;
public:
	/** @brief 关联窗口句柄。 */
    HWND Handle = NULL;
	/** @brief 绑定到指定窗口。 */
    Taskbar(HWND handle);
	/** @brief 设置进度值与总量。 */
    void SetValue(ULONGLONG value, ULONGLONG total);
    ~Taskbar();
};

