#pragma once

/**
 * @file Taskbar.h
 * @brief Taskbar：Windows 任务栏进度条封装（Legacy）。
 */
#include <shobjidl.h>

#pragma comment(lib, "Comctl32.lib")
class Taskbar
{
    static ITaskbarList3* pTaskbarList;
public:
    HWND Handle = NULL;
    Taskbar(HWND handle);
    void SetValue(ULONGLONG value, ULONGLONG total);
    ~Taskbar();
};

