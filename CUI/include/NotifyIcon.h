#pragma once

#include <Windows.h>
#include "RoutedCommand.h"

#include <shellapi.h>

#include <any>
#include <memory>
#include <span>
#include <string>
#include <vector>

class Window;
class Control;

/**
 * Value-semantic command source projected into a native tray popup menu.
 * Win32 menu identifiers are deliberately absent: they exist only while one
 * popup is open and never become application command identity.
 */
class NotifyIconMenuItem final
{
public:
	std::wstring Text;
	RoutedCommand Command;
	std::any CommandParameter;
	ControlWeakReference CommandTarget;
	bool IsEnabled = true;
	bool IsSeparator = false;
	std::vector<NotifyIconMenuItem> Items;

	NotifyIconMenuItem() = default;
	NotifyIconMenuItem(
		std::wstring text,
		RoutedCommand command,
		std::any commandParameter = {},
		bool isEnabled = true);

	static NotifyIconMenuItem Separator();
	static NotifyIconMenuItem Submenu(
		std::wstring text,
		std::vector<NotifyIconMenuItem> items = {});

	/** Adds a valid command leaf, separator or submenu to this submenu. */
	bool AddItem(NotifyIconMenuItem item);
	bool IsSubmenu() const noexcept { return !Items.empty(); }
};

/**
 * Unicode system-tray service owned by a Window. HICON remains caller-owned.
 * Tray menu leaves execute the same routed-command transaction as Button,
 * MenuItem and InputBinding; native numeric ids are transient implementation
 * details and are never published as application events.
 */
class NotifyIcon final
{
public:
	using MouseDownEvent = Event<void(NotifyIcon*, const MouseEventArgs&)>;

	NotifyIcon();
	~NotifyIcon();
	NotifyIcon(const NotifyIcon&) = delete;
	NotifyIcon& operator=(const NotifyIcon&) = delete;
	NotifyIcon(NotifyIcon&&) = delete;
	NotifyIcon& operator=(NotifyIcon&&) = delete;

	/** Attaches this service to one live Window; native identity stays internal. */
	bool TryInitialize(Window& owner);
	bool IsInitialized() const noexcept;
	bool IsVisible() const noexcept;
	Window* GetOwner() const noexcept;
	HRESULT GetLastError() const noexcept;

	bool TrySetIcon(HICON icon);
	HICON GetIcon() const noexcept;
	bool TryShow();
	bool TryHide();

	bool TrySetToolTip(const std::wstring& text);
	std::wstring GetToolTip() const;
	bool TryShowBalloonTip(
		const std::wstring& title,
		const std::wstring& text,
		DWORD timeout = 5000,
		DWORD type = NIIF_INFO);

	bool TryAddMenuItem(NotifyIconMenuItem item);
	bool TryAddMenuSeparator();
	bool TryRemoveMenuItemAt(std::size_t index);
	void ClearMenu() noexcept;
	std::size_t MenuItemCount(bool recursive = false) const noexcept;
	std::span<const NotifyIconMenuItem> GetMenuItems() const noexcept;

	bool TryShowContextMenu(int screenX, int screenY);

	MouseDownEvent OnMouseDown;

private:
	struct Impl;
	/** Platform-only callback and Explorer-recovery projection. */
	static bool HandlePlatformWindowMessage(
		HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	void DetachOwner(bool removeNativeIcon) noexcept;
	std::shared_ptr<Impl> _impl;
};
