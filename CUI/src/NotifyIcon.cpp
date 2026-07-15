#include "NotifyIcon.h"

#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <utility>

#include <shellapi.h>

NotifyIcon* NotifyIcon::Instance = nullptr;

namespace
{
	std::wstring NarrowToWide(const char* text)
	{
		if (!text || !*text) return L"";
		auto convert = [text](UINT codePage, DWORD flags) -> std::wstring
		{
			const int length = MultiByteToWideChar(
				codePage, flags, text, -1, nullptr, 0);
			if (length <= 1) return L"";
			std::wstring result(static_cast<size_t>(length), L'\0');
			(void)MultiByteToWideChar(
				codePage, flags, text, -1, result.data(), length);
			result.pop_back();
			return result;
		};
		auto result = convert(CP_UTF8, MB_ERR_INVALID_CHARS);
		return result.empty() ? convert(CP_ACP, 0) : result;
	}

	HRESULT LastWin32Failure()
	{
		const DWORD error = ::GetLastError();
		return error == ERROR_SUCCESS ? E_FAIL : HRESULT_FROM_WIN32(error);
	}

	std::mutex& IconRegistryMutex()
	{
		static auto* mutex = new std::mutex();
		return *mutex;
	}

	std::vector<NotifyIcon*>& IconRegistry()
	{
		static auto* icons = new std::vector<NotifyIcon*>();
		return *icons;
	}

	void RegisterIcon(NotifyIcon* icon)
	{
		std::scoped_lock lock(IconRegistryMutex());
		auto& icons = IconRegistry();
		if (std::find(icons.begin(), icons.end(), icon) == icons.end())
			icons.push_back(icon);
		NotifyIcon::Instance = icon;
	}

	void UnregisterIcon(NotifyIcon* icon)
	{
		std::scoped_lock lock(IconRegistryMutex());
		auto& icons = IconRegistry();
		icons.erase(std::remove(icons.begin(), icons.end(), icon), icons.end());
		if (NotifyIcon::Instance == icon)
			NotifyIcon::Instance = icons.empty() ? nullptr : icons.back();
	}

	bool TreeHasDuplicateOrInvalidIds(
		const NotifyIconMenuItem& item,
		std::unordered_set<int>& ids)
	{
		if (item.Separator) return false;
		const bool hasChildren = item.HasSubMenu || !item.SubItems.empty();
		if (!hasChildren && item.ID <= 0) return true;
		if (item.ID > 0 && !ids.insert(item.ID).second) return true;
		for (const auto& child : item.SubItems)
		{
			if (TreeHasDuplicateOrInvalidIds(child, ids)) return true;
		}
		return false;
	}

	size_t CountMenuTree(const NotifyIconMenuItem& item)
	{
		size_t count = 1;
		for (const auto& child : item.SubItems)
			count += CountMenuTree(child);
		return count;
	}

	bool RemoveMenuTree(std::vector<NotifyIconMenuItem>& items, int id)
	{
		for (auto it = items.begin(); it != items.end(); ++it)
		{
			if (!it->Separator && it->ID == id)
			{
				items.erase(it);
				return true;
			}
			if (RemoveMenuTree(it->SubItems, id)) return true;
		}
		return false;
	}

	void ClearTransientHandles(std::vector<NotifyIconMenuItem>& items)
	{
		for (auto& item : items)
		{
			item.SubMenu = nullptr;
			ClearTransientHandles(item.SubItems);
		}
	}

	bool AppendMenuTree(HMENU menu, NotifyIconMenuItem& item)
	{
		if (item.Separator)
			return AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) != FALSE;

		UINT flags = MF_STRING | (item.Enabled ? MF_ENABLED : MF_GRAYED);
		if (item.HasSubMenu || !item.SubItems.empty())
		{
			HMENU subMenu = CreatePopupMenu();
			if (!subMenu) return false;
			item.SubMenu = subMenu;
			for (auto& child : item.SubItems)
			{
				if (!AppendMenuTree(subMenu, child))
				{
					DestroyMenu(subMenu);
					item.SubMenu = nullptr;
					return false;
				}
			}
			if (!AppendMenuW(
				menu, flags | MF_POPUP,
				reinterpret_cast<UINT_PTR>(subMenu), item.Text.c_str()))
			{
				DestroyMenu(subMenu);
				item.SubMenu = nullptr;
				return false;
			}
			return true;
		}
		return AppendMenuW(
			menu, flags, static_cast<UINT_PTR>(item.ID), item.Text.c_str()) != FALSE;
	}
}

NotifyIconMenuItem::NotifyIconMenuItem(
	std::wstring text, int id, bool enabled)
	: Text(std::move(text)), ID(id), Enabled(enabled)
{
}

NotifyIconMenuItem::NotifyIconMenuItem(
	const wchar_t* text, int id, bool enabled)
	: NotifyIconMenuItem(text ? std::wstring(text) : std::wstring{}, id, enabled)
{
}

NotifyIconMenuItem::NotifyIconMenuItem(
	const std::string& text, int id, bool enabled)
	: NotifyIconMenuItem(NarrowToWide(text.c_str()), id, enabled)
{
}

NotifyIconMenuItem::NotifyIconMenuItem(
	const char* text, int id, bool enabled)
	: NotifyIconMenuItem(NarrowToWide(text), id, enabled)
{
}

NotifyIconMenuItem::NotifyIconMenuItem(const NotifyIconMenuItem& other)
	: Text(other.Text), ID(other.ID), Enabled(other.Enabled),
	Separator(other.Separator),
	HasSubMenu(other.HasSubMenu || !other.SubItems.empty()),
	SubMenu(nullptr), SubItems(other.SubItems)
{
}

NotifyIconMenuItem& NotifyIconMenuItem::operator=(const NotifyIconMenuItem& other)
{
	if (this == &other) return *this;
	Text = other.Text;
	ID = other.ID;
	Enabled = other.Enabled;
	Separator = other.Separator;
	HasSubMenu = other.HasSubMenu || !other.SubItems.empty();
	SubMenu = nullptr;
	SubItems = other.SubItems;
	return *this;
}

NotifyIconMenuItem::NotifyIconMenuItem(NotifyIconMenuItem&& other) noexcept
	: Text(std::move(other.Text)), ID(other.ID), Enabled(other.Enabled),
	Separator(other.Separator),
	HasSubMenu(other.HasSubMenu || !other.SubItems.empty()),
	SubMenu(nullptr), SubItems(std::move(other.SubItems))
{
	other.SubMenu = nullptr;
}

NotifyIconMenuItem& NotifyIconMenuItem::operator=(NotifyIconMenuItem&& other) noexcept
{
	if (this == &other) return *this;
	Text = std::move(other.Text);
	ID = other.ID;
	Enabled = other.Enabled;
	Separator = other.Separator;
	HasSubMenu = other.HasSubMenu || !other.SubItems.empty();
	SubMenu = nullptr;
	SubItems = std::move(other.SubItems);
	other.SubMenu = nullptr;
	return *this;
}

NotifyIconMenuItem NotifyIconMenuItem::CreateSeparator()
{
	NotifyIconMenuItem item(L"", -1);
	item.Separator = true;
	return item;
}

bool NotifyIconMenuItem::TryAddSubItem(const NotifyIconMenuItem& item)
{
	std::unordered_set<int> ids;
	for (const auto& existing : SubItems)
	{
		if (TreeHasDuplicateOrInvalidIds(existing, ids)) return false;
	}
	if (TreeHasDuplicateOrInvalidIds(item, ids)) return false;
	HasSubMenu = true;
	SubItems.push_back(item);
	return true;
}

void NotifyIconMenuItem::AddSubItem(const NotifyIconMenuItem& item)
{
	(void)TryAddSubItem(item);
}

struct NotifyIcon::Impl
{
	NOTIFYICONDATAW data{};
	UINT iconId = 0;
	UINT callbackMessage = WM_USER + 1;
	bool initialized = false;
	bool visible = false;
	bool desiredVisible = false;
	HRESULT lastError = E_PENDING;
	std::wstring toolTip;
	std::vector<NotifyIconMenuItem> menuItems;
	std::vector<std::unique_ptr<NotifyIconMenuItem>> detachedMenus;
	HMENU activePopup = nullptr;
};

NotifyIcon::NotifyIcon()
	: _impl(std::make_unique<Impl>())
{
	_impl->data.cbSize = sizeof(NOTIFYICONDATAW);
	_impl->data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	_impl->data.uCallbackMessage = _impl->callbackMessage;
}

NotifyIcon::~NotifyIcon()
{
	(void)TryHide();
	UnregisterIcon(this);
	if (_impl->activePopup)
	{
		DestroyMenu(_impl->activePopup);
		_impl->activePopup = nullptr;
	}
}

bool NotifyIcon::TryInitialize(HWND window, UINT iconId, UINT callbackMessage)
{
	if (iconId == 0 || callbackMessage < WM_USER)
	{
		_impl->lastError = E_INVALIDARG;
		return false;
	}
	if (!window || !IsWindow(window))
	{
		_impl->lastError = E_HANDLE;
		return false;
	}
	if (_impl->visible && !TryHide()) return false;

	hWnd = window;
	_impl->iconId = iconId;
	_impl->callbackMessage = callbackMessage;
	_impl->data.hWnd = window;
	_impl->data.uID = iconId;
	_impl->data.uCallbackMessage = callbackMessage;
	_impl->initialized = true;
	_impl->lastError = S_OK;
	return true;
}

void NotifyIcon::InitNotifyIcon(HWND window, int iconId)
{
	(void)TryInitialize(
		window, iconId > 0 ? static_cast<UINT>(iconId) : 0U);
}

bool NotifyIcon::IsInitialized() const noexcept { return _impl->initialized; }
bool NotifyIcon::IsVisible() const noexcept { return _impl->visible; }
UINT NotifyIcon::GetIconId() const noexcept { return _impl->iconId; }
UINT NotifyIcon::GetCallbackMessage() const noexcept
{
	return _impl->callbackMessage;
}
HRESULT NotifyIcon::GetLastError() const noexcept { return _impl->lastError; }

bool NotifyIcon::TrySetIcon(HICON icon)
{
	if (!icon)
	{
		_impl->lastError = E_POINTER;
		return false;
	}
	_impl->data.hIcon = icon;
	if (!_impl->visible)
	{
		_impl->lastError = S_OK;
		return true;
	}
	if (!Shell_NotifyIconW(NIM_MODIFY, &_impl->data))
	{
		_impl->lastError = LastWin32Failure();
		return false;
	}
	_impl->lastError = S_OK;
	return true;
}

void NotifyIcon::SetIcon(HICON icon) { (void)TrySetIcon(icon); }
HICON NotifyIcon::GetIcon() const noexcept { return _impl->data.hIcon; }

bool NotifyIcon::TryShow()
{
	_impl->desiredVisible = true;
	if (!_impl->initialized || !hWnd)
	{
		_impl->lastError = E_HANDLE;
		return false;
	}
	if (!_impl->data.hIcon)
	{
		_impl->lastError = E_POINTER;
		return false;
	}

	const DWORD operation = _impl->visible ? NIM_MODIFY : NIM_ADD;
	if (!Shell_NotifyIconW(operation, &_impl->data))
	{
		_impl->lastError = LastWin32Failure();
		return false;
	}
	if (!_impl->visible)
	{
		NOTIFYICONDATAW versionData = _impl->data;
		versionData.uVersion = NOTIFYICON_VERSION;
		(void)Shell_NotifyIconW(NIM_SETVERSION, &versionData);
	}
	_impl->visible = true;
	_impl->lastError = S_OK;
	RegisterIcon(this);
	return true;
}

bool NotifyIcon::TryHide()
{
	_impl->desiredVisible = false;
	if (!_impl->visible)
	{
		_impl->lastError = S_OK;
		UnregisterIcon(this);
		return true;
	}
	if (!Shell_NotifyIconW(NIM_DELETE, &_impl->data))
	{
		_impl->lastError = LastWin32Failure();
		return false;
	}
	_impl->visible = false;
	_impl->lastError = S_OK;
	UnregisterIcon(this);
	return true;
}

void NotifyIcon::ShowNotifyIcon() { (void)TryShow(); }
void NotifyIcon::HideNotifyIcon() { (void)TryHide(); }

bool NotifyIcon::TrySetToolTip(const std::wstring& text)
{
	_impl->toolTip = text;
	wcsncpy_s(
		_impl->data.szTip, _countof(_impl->data.szTip),
		text.c_str(), _TRUNCATE);
	if (!_impl->visible)
	{
		_impl->lastError = S_OK;
		return true;
	}
	if (!Shell_NotifyIconW(NIM_MODIFY, &_impl->data))
	{
		_impl->lastError = LastWin32Failure();
		return false;
	}
	_impl->lastError = S_OK;
	return true;
}

void NotifyIcon::SetToolTip(const wchar_t* text)
{
	(void)TrySetToolTip(text ? text : L"");
}
void NotifyIcon::SetToolTip(const char* text)
{
	(void)TrySetToolTip(NarrowToWide(text));
}
std::wstring NotifyIcon::GetToolTip() const { return _impl->toolTip; }

bool NotifyIcon::TryShowBalloonTip(
	const std::wstring& title,
	const std::wstring& text,
	DWORD timeout,
	DWORD type)
{
	if (!_impl->visible)
	{
		_impl->lastError = E_UNEXPECTED;
		return false;
	}
	NOTIFYICONDATAW balloon = _impl->data;
	balloon.uFlags |= NIF_INFO;
	balloon.dwInfoFlags = type;
	balloon.uTimeout = timeout;
	wcsncpy_s(
		balloon.szInfoTitle, _countof(balloon.szInfoTitle),
		title.c_str(), _TRUNCATE);
	wcsncpy_s(
		balloon.szInfo, _countof(balloon.szInfo),
		text.c_str(), _TRUNCATE);
	if (!Shell_NotifyIconW(NIM_MODIFY, &balloon))
	{
		_impl->lastError = LastWin32Failure();
		return false;
	}
	_impl->lastError = S_OK;
	return true;
}

void NotifyIcon::ShowBalloonTip(
	const wchar_t* title, const wchar_t* text, int timeout, int type)
{
	(void)TryShowBalloonTip(
		title ? title : L"", text ? text : L"",
		static_cast<DWORD>((std::max)(0, timeout)), static_cast<DWORD>(type));
}

void NotifyIcon::ShowBalloonTip(
	const char* title, const char* text, int timeout, int type)
{
	(void)TryShowBalloonTip(
		NarrowToWide(title), NarrowToWide(text),
		static_cast<DWORD>((std::max)(0, timeout)), static_cast<DWORD>(type));
}

bool NotifyIcon::TryAddMenuItem(const NotifyIconMenuItem& item)
{
	std::unordered_set<int> ids;
	for (const auto& existing : _impl->menuItems)
	{
		if (TreeHasDuplicateOrInvalidIds(existing, ids))
		{
			_impl->lastError = E_UNEXPECTED;
			return false;
		}
	}
	if (TreeHasDuplicateOrInvalidIds(item, ids))
	{
		_impl->lastError = E_INVALIDARG;
		return false;
	}
	_impl->menuItems.push_back(item);
	_impl->lastError = S_OK;
	return true;
}

void NotifyIcon::AddMenuItem(const NotifyIconMenuItem& item)
{
	(void)TryAddMenuItem(item);
}
bool NotifyIcon::TryAddMenuSeparator()
{
	return TryAddMenuItem(NotifyIconMenuItem::CreateSeparator());
}
void NotifyIcon::AddMenuSeparator() { (void)TryAddMenuSeparator(); }

void NotifyIcon::ClearMenu()
{
	_impl->menuItems.clear();
	_impl->detachedMenus.clear();
	_impl->lastError = S_OK;
}

size_t NotifyIcon::MenuItemCount(bool recursive) const noexcept
{
	if (!recursive) return _impl->menuItems.size();
	size_t count = 0;
	for (const auto& item : _impl->menuItems)
		count += CountMenuTree(item);
	return count;
}

bool NotifyIcon::RemoveMenuItem(int id)
{
	const bool removed = RemoveMenuTree(_impl->menuItems, id);
	_impl->lastError = removed ? S_OK : HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	return removed;
}

bool NotifyIcon::TryEnableMenuItem(int id, bool enable)
{
	auto* item = FindMenuItem(id);
	if (!item)
	{
		_impl->lastError = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		return false;
	}
	item->Enabled = enable;
	_impl->lastError = S_OK;
	return true;
}
void NotifyIcon::EnableMenuItem(int id, bool enable)
{
	(void)TryEnableMenuItem(id, enable);
}

bool NotifyIcon::TrySetMenuItemText(int id, const std::wstring& text)
{
	auto* item = FindMenuItem(id);
	if (!item || item->Separator)
	{
		_impl->lastError = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		return false;
	}
	item->Text = text;
	_impl->lastError = S_OK;
	return true;
}
void NotifyIcon::SetMenuItemText(int id, const std::wstring& text)
{
	(void)TrySetMenuItemText(id, text);
}
void NotifyIcon::SetMenuItemText(int id, const std::string& text)
{
	(void)TrySetMenuItemText(id, NarrowToWide(text.c_str()));
}

NotifyIconMenuItem* NotifyIcon::CreateSubMenu(
	const std::wstring& text, int id)
{
	auto menu = std::make_unique<NotifyIconMenuItem>(text, id);
	menu->HasSubMenu = true;
	auto* result = menu.get();
	_impl->detachedMenus.push_back(std::move(menu));
	return result;
}

NotifyIconMenuItem* NotifyIcon::CreateSubMenu(const std::string& text)
{
	return CreateSubMenu(NarrowToWide(text.c_str()), 0);
}

bool NotifyIcon::TryAddSubMenuItem(
	int parentId, const NotifyIconMenuItem& item)
{
	auto* parent = FindMenuItem(parentId);
	if (!parent)
	{
		_impl->lastError = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		return false;
	}

	std::unordered_set<int> ids;
	for (const auto& existing : _impl->menuItems)
	{
		if (TreeHasDuplicateOrInvalidIds(existing, ids))
		{
			_impl->lastError = E_UNEXPECTED;
			return false;
		}
	}
	if (TreeHasDuplicateOrInvalidIds(item, ids))
	{
		_impl->lastError = E_INVALIDARG;
		return false;
	}
	parent->HasSubMenu = true;
	parent->SubItems.push_back(item);
	_impl->lastError = S_OK;
	return true;
}

void NotifyIcon::AddSubMenuItem(
	int parentId, const NotifyIconMenuItem& item)
{
	(void)TryAddSubMenuItem(parentId, item);
}

NotifyIconMenuItem* NotifyIcon::FindMenuItem(
	int id, std::vector<NotifyIconMenuItem>& items)
{
	for (auto& item : items)
	{
		if (!item.Separator && item.ID == id) return &item;
		if (auto* found = FindMenuItem(id, item.SubItems)) return found;
	}
	return nullptr;
}

const NotifyIconMenuItem* NotifyIcon::FindMenuItem(
	int id, const std::vector<NotifyIconMenuItem>& items) const
{
	for (const auto& item : items)
	{
		if (!item.Separator && item.ID == id) return &item;
		if (auto* found = FindMenuItem(id, item.SubItems)) return found;
	}
	return nullptr;
}

NotifyIconMenuItem* NotifyIcon::FindMenuItem(int id)
{
	return FindMenuItem(id, _impl->menuItems);
}
const NotifyIconMenuItem* NotifyIcon::FindMenuItem(int id) const
{
	return FindMenuItem(id, _impl->menuItems);
}

bool NotifyIcon::TryShowContextMenu(int x, int y)
{
	if (!hWnd || !_impl->initialized)
	{
		_impl->lastError = E_HANDLE;
		return false;
	}
	if (_impl->menuItems.empty())
	{
		_impl->lastError = E_INVALIDARG;
		return false;
	}
	if (_impl->activePopup)
	{
		DestroyMenu(_impl->activePopup);
		_impl->activePopup = nullptr;
		ClearTransientHandles(_impl->menuItems);
	}

	HMENU menu = CreatePopupMenu();
	if (!menu)
	{
		_impl->lastError = LastWin32Failure();
		return false;
	}
	_impl->activePopup = menu;
	for (auto& item : _impl->menuItems)
	{
		if (!AppendMenuTree(menu, item))
		{
			_impl->lastError = LastWin32Failure();
			DestroyMenu(menu);
			_impl->activePopup = nullptr;
			ClearTransientHandles(_impl->menuItems);
			return false;
		}
	}

	(void)SetForegroundWindow(hWnd);
	::SetLastError(ERROR_SUCCESS);
	const UINT command = TrackPopupMenu(
		menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
		x, y, 0, hWnd, nullptr);
	const DWORD trackError = ::GetLastError();
	DestroyMenu(menu);
	_impl->activePopup = nullptr;
	ClearTransientHandles(_impl->menuItems);
	(void)PostMessageW(hWnd, WM_NULL, 0, 0);

	if (command == 0 && trackError != ERROR_SUCCESS)
	{
		_impl->lastError = HRESULT_FROM_WIN32(trackError);
		return false;
	}
	_impl->lastError = S_OK;
	if (command != 0)
		OnNotifyIconMenuClick(this, static_cast<int>(command));
	return true;
}

void NotifyIcon::ShowContextMenu(int x, int y)
{
	(void)TryShowContextMenu(x, y);
}

bool NotifyIcon::DispatchWindowMessage(
	HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message < WM_USER) return false;

	std::vector<NotifyIcon*> icons;
	{
		std::scoped_lock lock(IconRegistryMutex());
		icons = IconRegistry();
	}

	static const UINT taskbarCreated =
		RegisterWindowMessageW(L"TaskbarCreated");
	if (message == taskbarCreated)
	{
		for (auto* icon : icons)
		{
			if (!icon || icon->hWnd != window
				|| !icon->_impl->desiredVisible)
				continue;
			icon->_impl->visible = false;
			(void)icon->TryShow();
		}
		return false;
	}

	NotifyIcon* target = nullptr;
	for (auto* icon : icons)
	{
		if (icon && icon->hWnd == window
			&& icon->_impl->callbackMessage == message
			&& icon->_impl->iconId == static_cast<UINT>(wParam))
		{
			target = icon;
			break;
		}
	}
	if (!target) return false;

	MouseButtons button = MouseButtons::None;
	if (lParam == WM_LBUTTONDOWN) button = MouseButtons::Left;
	else if (lParam == WM_RBUTTONDOWN) button = MouseButtons::Right;
	else if (lParam == WM_MBUTTONDOWN) button = MouseButtons::Middle;
	if (button != MouseButtons::None)
	{
		POINT location{};
		(void)GetCursorPos(&location);
		target->OnNotifyIconMouseDown(
			target, MouseEventArgs(button, 1, location.x, location.y, 0));
		return true;
	}
	if (lParam == WM_RBUTTONUP && target->MenuItemCount() > 0)
	{
		POINT location{};
		(void)GetCursorPos(&location);
		(void)target->TryShowContextMenu(location.x, location.y);
		return true;
	}
	return true;
}
