#include "NotifyIcon.h"
NotifyIcon* NotifyIcon::Instance = NULL;


NotifyIconMenuItem NotifyIconMenuItem::CreateSeparator()
{
    NotifyIconMenuItem item("", -1);
    item.Separator = true;
    return item;
}
void NotifyIconMenuItem::AddSubItem(const NotifyIconMenuItem& item)
{
    if (!HasSubMenu)
    {
        SubMenu = CreatePopupMenu();
        HasSubMenu = true;
    }

    if (item.Separator)
    {
        AppendMenuA(SubMenu, MF_SEPARATOR, 0, NULL);
    }
    else
    {
        UINT flags = MF_STRING;
        if (!item.Enabled)
            flags |= MF_GRAYED;

        if (item.HasSubMenu)
            AppendMenuA(SubMenu, flags | MF_POPUP, (UINT_PTR)item.SubMenu, item.Text.c_str());
        else
            AppendMenuA(SubMenu, flags, item.ID, item.Text.c_str());
    }

    SubItems.push_back(item);
}
NotifyIconMenuItem* NotifyIcon::CreateSubMenu(const std::string& text)
{
    
    NotifyIconMenuItem* subMenu = new NotifyIconMenuItem(text, 0);
    subMenu->HasSubMenu = true;
    subMenu->SubMenu = CreatePopupMenu();
    return subMenu;
}
NotifyIconMenuItem* NotifyIcon::FindMenuItem(int id)
{
    return FindMenuItem(id, menuItems);
}

NotifyIconMenuItem* NotifyIcon::FindMenuItem(int id, std::vector<NotifyIconMenuItem>& items)
{
    for (auto& item : items)
    {
        if (item.ID == id)
            return &item;

        if (item.HasSubMenu)
        {
            NotifyIconMenuItem* found = FindMenuItem(id, item.SubItems);
            if (found)
                return found;
        }
    }
    return nullptr;
}
void NotifyIcon::AddSubMenuItem(int parentId, const NotifyIconMenuItem& item)
{
    NotifyIconMenuItem* parent = FindMenuItem(parentId);
    if (parent)
    {
        parent->AddSubItem(item);
    }
}
NotifyIcon::NotifyIcon()
{
    ZeroMemory(&NotifyIconData, sizeof(NOTIFYICONDATAA));
    NotifyIconData.cbSize = sizeof(NOTIFYICONDATAA);
    NotifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    NotifyIconData.uCallbackMessage = WM_USER + 1;
    NotifyIconData.hIcon = NULL;
    NotifyIconData.hWnd = NULL;
    NotifyIconData.uID = 0;
    NotifyIconData.uVersion = NOTIFYICON_VERSION;

    
    popupMenu = CreatePopupMenu();
}

NotifyIcon::~NotifyIcon()
{
    HideNotifyIcon();
    if (popupMenu)
    {
        DestroyMenu(popupMenu);
        popupMenu = NULL;
    }
}

void NotifyIcon::InitNotifyIcon(HWND hWnd, int iconID)
{
    this->hWnd = hWnd;
    this->iconID = iconID;
    NotifyIconData.hWnd = hWnd;
    NotifyIconData.uID = iconID;
}

void NotifyIcon::SetIcon(HICON hIcon)
{
    NotifyIconData.hIcon = hIcon;
}

void NotifyIcon::ShowNotifyIcon()
{
    Shell_NotifyIconA(NIM_ADD, &NotifyIconData);
    Instance = this;
}

void NotifyIcon::HideNotifyIcon()
{
    Shell_NotifyIconA(NIM_DELETE, &NotifyIconData);
    if (Instance == this)
        Instance = NULL;
}

void NotifyIcon::SetToolTip(const char* text)
{
    strncpy_s(NotifyIconData.szTip, text, sizeof(NotifyIconData.szTip));
}

void NotifyIcon::ShowBalloonTip(const char* title, const char* text, int timeout, int type)
{
    NotifyIconData.uFlags |= NIF_INFO;
    NotifyIconData.dwInfoFlags = type;
    strncpy_s(NotifyIconData.szInfoTitle, title, sizeof(NotifyIconData.szInfoTitle));
    strncpy_s(NotifyIconData.szInfo, text, sizeof(NotifyIconData.szInfo));
    NotifyIconData.uTimeout = timeout;
    Shell_NotifyIconA(NIM_MODIFY, &NotifyIconData);
}


void NotifyIcon::AddMenuItem(const NotifyIconMenuItem& item)
{
    if (item.Separator)
    {
        AppendMenuA(popupMenu, MF_SEPARATOR, 0, NULL);
    }
    else
    {
        UINT flags = MF_STRING;
        if (!item.Enabled)
            flags |= MF_GRAYED;

        if (item.HasSubMenu)
            AppendMenuA(popupMenu, flags | MF_POPUP, (UINT_PTR)item.SubMenu, item.Text.c_str());
        else
            AppendMenuA(popupMenu, flags, item.ID, item.Text.c_str());
    }

    menuItems.push_back(item);
}

void NotifyIcon::AddMenuSeparator()
{
    AddMenuItem(NotifyIconMenuItem::CreateSeparator());
}

void NotifyIcon::ShowContextMenu(int x, int y)
{
    
    SetForegroundWindow(hWnd);

    
    UINT flags = TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD;
    int cmdId = TrackPopupMenu(popupMenu, flags, x, y, 0, hWnd, NULL);

    
    if (cmdId > 0)
    {
        OnNotifyIconMenuClick(this, cmdId);
    }

    
    PostMessage(hWnd, WM_NULL, 0, 0);
}

void NotifyIcon::ClearMenu()
{
    while (GetMenuItemCount(popupMenu) > 0)
    {
        DeleteMenu(popupMenu, 0, MF_BYPOSITION);
    }
    menuItems.clear();
}

void NotifyIcon::EnableMenuItem(int id, bool enable)
{
    UINT flags = enable ? MF_ENABLED : MF_GRAYED;
    ::EnableMenuItem(popupMenu, id, flags);

    
    for (auto& item : menuItems)
    {
        if (item.ID == id)
        {
            item.Enabled = enable;
            break;
        }
    }
}

void NotifyIcon::SetMenuItemText(int id, const std::string& text)
{
    MENUITEMINFOA info = { 0 };
    info.cbSize = sizeof(MENUITEMINFOA);
    info.fMask = MIIM_STRING;
    info.dwTypeData = (LPSTR)text.c_str();

    SetMenuItemInfoA(popupMenu, id, FALSE, &info);

    
    for (auto& item : menuItems)
    {
        if (item.ID == id)
        {
            item.Text = text;
            break;
        }
    }
}
