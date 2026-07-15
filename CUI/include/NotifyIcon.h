#pragma once

#include "Control.h"

#include <shellapi.h>

#include <memory>
#include <string>
#include <vector>

/** Value-semantic Unicode menu description used by NotifyIcon. */
class NotifyIconMenuItem
{
public:
	std::wstring Text;
	int ID = 0;
	bool Enabled = true;
	bool Separator = false;
	bool HasSubMenu = false;
	/** Transient handle while a popup is open; never owned by the item. */
	HMENU SubMenu = nullptr;
	std::vector<NotifyIconMenuItem> SubItems;

	NotifyIconMenuItem(std::wstring text, int id, bool enabled = true);
	NotifyIconMenuItem(const wchar_t* text, int id, bool enabled = true);
	/** Compatibility overload; narrow text is interpreted as UTF-8, then ACP. */
	NotifyIconMenuItem(const std::string& text, int id, bool enabled = true);
	NotifyIconMenuItem(const char* text, int id, bool enabled = true);
	NotifyIconMenuItem(const NotifyIconMenuItem& other);
	NotifyIconMenuItem& operator=(const NotifyIconMenuItem& other);
	NotifyIconMenuItem(NotifyIconMenuItem&& other) noexcept;
	NotifyIconMenuItem& operator=(NotifyIconMenuItem&& other) noexcept;
	~NotifyIconMenuItem() = default;

	static NotifyIconMenuItem CreateSeparator();
	bool TryAddSubItem(const NotifyIconMenuItem& item);
	void AddSubItem(const NotifyIconMenuItem& item);
};

/**
 * Unicode system-tray icon wrapper. HICON remains caller-owned.
 * All Try* methods report Shell/Win32 failures through GetLastError().
 */
class NotifyIcon
{
public:
	using NotifyIconMouseDownEvent = Event<void(NotifyIcon*, MouseEventArgs)>;
	using NotifyIconMenuClickEvent = Event<void(NotifyIcon*, int)>;

	NotifyIcon();
	~NotifyIcon();
	NotifyIcon(const NotifyIcon&) = delete;
	NotifyIcon& operator=(const NotifyIcon&) = delete;
	NotifyIcon(NotifyIcon&&) = delete;
	NotifyIcon& operator=(NotifyIcon&&) = delete;

	/** Compatibility host handle. Prefer TryInitialize() when changing it. */
	HWND hWnd = nullptr;
	/** Most recently shown icon, retained for legacy code. Dispatch supports many. */
	static NotifyIcon* Instance;

	bool TryInitialize(
		HWND window, UINT iconId,
		UINT callbackMessage = WM_USER + 1);
	void InitNotifyIcon(HWND window, int iconId);
	bool IsInitialized() const noexcept;
	bool IsVisible() const noexcept;
	UINT GetIconId() const noexcept;
	UINT GetCallbackMessage() const noexcept;
	HRESULT GetLastError() const noexcept;

	bool TrySetIcon(HICON icon);
	void SetIcon(HICON icon);
	HICON GetIcon() const noexcept;

	bool TryShow();
	bool TryHide();
	void ShowNotifyIcon();
	void HideNotifyIcon();

	bool TrySetToolTip(const std::wstring& text);
	void SetToolTip(const wchar_t* text);
	void SetToolTip(const char* text);
	std::wstring GetToolTip() const;

	bool TryShowBalloonTip(
		const std::wstring& title,
		const std::wstring& text,
		DWORD timeout = 5000,
		DWORD type = NIIF_INFO);
	void ShowBalloonTip(
		const wchar_t* title, const wchar_t* text,
		int timeout = 5000, int type = NIIF_INFO);
	void ShowBalloonTip(
		const char* title, const char* text,
		int timeout = 5000, int type = NIIF_INFO);

	bool TryAddMenuItem(const NotifyIconMenuItem& item);
	void AddMenuItem(const NotifyIconMenuItem& item);
	bool TryAddMenuSeparator();
	void AddMenuSeparator();
	void ClearMenu();
	size_t MenuItemCount(bool recursive = false) const noexcept;
	bool RemoveMenuItem(int id);

	bool TryEnableMenuItem(int id, bool enable);
	void EnableMenuItem(int id, bool enable);
	bool TrySetMenuItemText(int id, const std::wstring& text);
	void SetMenuItemText(int id, const std::wstring& text);
	void SetMenuItemText(int id, const std::string& text);

	NotifyIconMenuItem* CreateSubMenu(const std::wstring& text, int id = 0);
	NotifyIconMenuItem* CreateSubMenu(const std::string& text);
	bool TryAddSubMenuItem(int parentId, const NotifyIconMenuItem& item);
	void AddSubMenuItem(int parentId, const NotifyIconMenuItem& item);

	NotifyIconMenuItem* FindMenuItem(
		int id, std::vector<NotifyIconMenuItem>& items);
	const NotifyIconMenuItem* FindMenuItem(
		int id, const std::vector<NotifyIconMenuItem>& items) const;
	NotifyIconMenuItem* FindMenuItem(int id);
	const NotifyIconMenuItem* FindMenuItem(int id) const;

	bool TryShowContextMenu(int x, int y);
	void ShowContextMenu(int x, int y);

	/** Called by Form's window procedure; supports multiple icons and Explorer restart. */
	static bool DispatchWindowMessage(
		HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	NotifyIconMouseDownEvent OnNotifyIconMouseDown;
	NotifyIconMenuClickEvent OnNotifyIconMenuClick;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};
