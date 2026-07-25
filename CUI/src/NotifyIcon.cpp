#include "NotifyIcon.h"
#include "EventInfrastructure.h"

#include "Window.h"
#include "WindowInfrastructure.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <utility>

#include <shellapi.h>

namespace
{
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

	UINT NotifyIconCallbackMessage()
	{
		static const UINT message =
			RegisterWindowMessageW(L"CUI.NotifyIcon.Callback");
		return message;
	}

	UINT& NextIconIdentity()
	{
		static UINT next = 1;
		return next;
	}

	void RegisterIcon(NotifyIcon* icon)
	{
		std::scoped_lock lock(IconRegistryMutex());
		auto& icons = IconRegistry();
		if (std::find(icons.begin(), icons.end(), icon) == icons.end())
			icons.push_back(icon);
	}

	void UnregisterIcon(NotifyIcon* icon)
	{
		std::scoped_lock lock(IconRegistryMutex());
		auto& icons = IconRegistry();
		icons.erase(std::remove(icons.begin(), icons.end(), icon), icons.end());
	}

	bool IsValidMenuTree(const NotifyIconMenuItem& item)
	{
		if (item.IsSeparator)
			return item.Text.empty() && item.Command.Empty() && item.Items.empty();
		if (item.Text.empty()) return false;
		if (!item.Items.empty())
		{
			if (!item.Command.Empty()) return false;
			return std::all_of(
				item.Items.begin(), item.Items.end(), IsValidMenuTree);
		}
		return !item.Command.Empty();
	}

	std::size_t CountMenuTree(const NotifyIconMenuItem& item)
	{
		std::size_t count = 1;
		for (const auto& child : item.Items)
			count += CountMenuTree(child);
		return count;
	}

	struct PopupCommandMap final
	{
		std::vector<NotifyIconMenuItem*> Items;

		UINT Add(NotifyIconMenuItem& item)
		{
			Items.push_back(&item);
			return static_cast<UINT>(Items.size());
		}

		NotifyIconMenuItem* Resolve(UINT transientId) const noexcept
		{
			return transientId > 0 && transientId <= Items.size()
				? Items[transientId - 1] : nullptr;
		}
	};

	bool AppendMenuTree(
		HMENU menu,
		NotifyIconMenuItem& item,
		Window& owner,
		PopupCommandMap& commands,
		const std::function<bool()>& operationValid)
	{
		if (!operationValid()) return false;
		if (item.IsSeparator)
			return AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) != FALSE;

		bool enabled = item.IsEnabled;
		if (!item.Items.empty())
		{
			HMENU submenu = CreatePopupMenu();
			if (!submenu) return false;
			for (auto& child : item.Items)
			{
				if (!AppendMenuTree(
					submenu, child, owner, commands, operationValid))
				{
					DestroyMenu(submenu);
					return false;
				}
			}
			const UINT flags = MF_STRING | MF_POPUP
				| (enabled ? MF_ENABLED : MF_GRAYED);
			if (!AppendMenuW(
				menu, flags, reinterpret_cast<UINT_PTR>(submenu),
				item.Text.c_str()))
			{
				DestroyMenu(submenu);
				return false;
			}
			return true;
		}

		RoutedCommandSourceQuery query{
			item.Command, item.CommandParameter, item.CommandTarget };
		if (enabled)
		{
			enabled = cui::framework::WindowAccess::Commands(owner)
				.QueryCommandSource(owner, query).CanExecute;
			if (!operationValid()) return false;
		}
		const UINT transientId = commands.Add(item);
		const UINT flags = MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED);
		return AppendMenuW(
			menu, flags, static_cast<UINT_PTR>(transientId),
			item.Text.c_str()) != FALSE;
	}
}

NotifyIconMenuItem::NotifyIconMenuItem(
	std::wstring text,
	RoutedCommand command,
	std::any commandParameter,
	bool isEnabled)
	: Text(std::move(text)),
	Command(std::move(command)),
	CommandParameter(std::move(commandParameter)),
	IsEnabled(isEnabled)
{
}

NotifyIconMenuItem NotifyIconMenuItem::Separator()
{
	NotifyIconMenuItem result;
	result.IsSeparator = true;
	return result;
}

NotifyIconMenuItem NotifyIconMenuItem::Submenu(
	std::wstring text,
	std::vector<NotifyIconMenuItem> items)
{
	NotifyIconMenuItem result;
	result.Text = std::move(text);
	result.Items = std::move(items);
	return result;
}

bool NotifyIconMenuItem::AddItem(NotifyIconMenuItem item)
{
	if (IsSeparator || !Command.Empty() || !IsValidMenuTree(item)) return false;
	Items.push_back(std::move(item));
	return true;
}

struct NotifyIcon::Impl final
{
	NOTIFYICONDATAW Data{};
	Window* Owner = nullptr;
	bool Initialized = false;
	bool Visible = false;
	bool DesiredVisible = false;
	HRESULT LastError = E_PENDING;
	std::wstring ToolTip;
	std::vector<NotifyIconMenuItem> MenuItems;
	EventConnection OwnerClosedConnection;
	bool PopupActive = false;
};

NotifyIcon::NotifyIcon()
	: _impl(std::make_shared<Impl>())
{
	_impl->Data.cbSize = sizeof(NOTIFYICONDATAW);
	_impl->Data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

NotifyIcon::~NotifyIcon()
{
	DetachOwner(true);
}

void NotifyIcon::DetachOwner(bool removeNativeIcon) noexcept
{
	if (removeNativeIcon && _impl->Visible && _impl->Data.hWnd)
		(void)Shell_NotifyIconW(NIM_DELETE, &_impl->Data);
	_impl->DesiredVisible = false;
	_impl->Visible = false;
	_impl->Initialized = false;
	_impl->Owner = nullptr;
	_impl->Data.hWnd = nullptr;
	_impl->OwnerClosedConnection = {};
	UnregisterIcon(this);
}

bool NotifyIcon::TryInitialize(Window& owner)
{
	const HWND window = owner.GetHandle();
	if (_impl->PopupActive)
	{
		_impl->LastError = HRESULT_FROM_WIN32(ERROR_BUSY);
		return false;
	}
	const UINT callbackMessage = NotifyIconCallbackMessage();
	if (callbackMessage == 0)
	{
		_impl->LastError = LastWin32Failure();
		return false;
	}
	if (!window || !IsWindow(window))
	{
		_impl->LastError = E_HANDLE;
		return false;
	}
	if (_impl->Visible && !TryHide()) return false;
	DetachOwner(false);

	_impl->Owner = &owner;
	_impl->Data.hWnd = window;
	_impl->Data.uCallbackMessage = callbackMessage;
	std::weak_ptr<Impl> weakOwner = _impl;
	_impl->OwnerClosedConnection = owner.OnWindowClosed.Subscribe(
		[weakOwner, identity = this, expectedOwner = &owner](Window* closedOwner)
		{
			auto state = weakOwner.lock();
			if (!state || closedOwner != expectedOwner
				|| state->Owner != expectedOwner) return;
			if (state->Visible && state->Data.hWnd)
				(void)Shell_NotifyIconW(NIM_DELETE, &state->Data);
			state->DesiredVisible = false;
			state->Visible = false;
			state->Initialized = false;
			state->Owner = nullptr;
			state->Data.hWnd = nullptr;
			state->OwnerClosedConnection = {};
			UnregisterIcon(identity);
		});
	{
		std::scoped_lock lock(IconRegistryMutex());
		auto& next = NextIconIdentity();
		for (;;)
		{
			const UINT candidate = next++;
			if (next == 0) next = 1;
			if (candidate == 0) continue;
			const bool occupied = std::any_of(
				IconRegistry().begin(), IconRegistry().end(),
				[&](const NotifyIcon* icon)
				{
					return icon && icon != this && icon->_impl->Initialized
						&& icon->_impl->Data.hWnd == window
						&& icon->_impl->Data.uID == candidate;
				});
			if (!occupied)
			{
				_impl->Data.uID = candidate;
				break;
			}
		}
		_impl->Initialized = true;
		auto& icons = IconRegistry();
		if (std::find(icons.begin(), icons.end(), this) == icons.end())
			icons.push_back(this);
	}
	_impl->LastError = S_OK;
	// Attachment registration lets a failed NIM_ADD be retried when Explorer
	// broadcasts TaskbarCreated; the native id never leaves this implementation.
	return true;
}

bool NotifyIcon::IsInitialized() const noexcept { return _impl->Initialized; }
bool NotifyIcon::IsVisible() const noexcept { return _impl->Visible; }
Window* NotifyIcon::GetOwner() const noexcept { return _impl->Owner; }
HRESULT NotifyIcon::GetLastError() const noexcept { return _impl->LastError; }

bool NotifyIcon::TrySetIcon(HICON icon)
{
	if (!icon)
	{
		_impl->LastError = E_POINTER;
		return false;
	}
	_impl->Data.hIcon = icon;
	if (!_impl->Visible)
	{
		_impl->LastError = S_OK;
		return true;
	}
	if (!Shell_NotifyIconW(NIM_MODIFY, &_impl->Data))
	{
		_impl->LastError = LastWin32Failure();
		return false;
	}
	_impl->LastError = S_OK;
	return true;
}

HICON NotifyIcon::GetIcon() const noexcept { return _impl->Data.hIcon; }

bool NotifyIcon::TryShow()
{
	_impl->DesiredVisible = true;
	if (!_impl->Initialized || !_impl->Owner
		|| !_impl->Data.hWnd || !IsWindow(_impl->Data.hWnd))
	{
		_impl->LastError = E_HANDLE;
		return false;
	}
	if (!_impl->Data.hIcon)
	{
		_impl->LastError = E_POINTER;
		return false;
	}

	bool added = !_impl->Visible;
	if (_impl->Visible
		&& !Shell_NotifyIconW(NIM_MODIFY, &_impl->Data))
	{
		// Explorer may have discarded the icon before TaskbarCreated reached us.
		// Fall back to a fresh add instead of pinning the service in stale state.
		_impl->Visible = false;
		added = true;
	}
	if (added && !Shell_NotifyIconW(NIM_ADD, &_impl->Data))
	{
		_impl->LastError = LastWin32Failure();
		return false;
	}
	if (added)
	{
		NOTIFYICONDATAW versionData = _impl->Data;
		versionData.uVersion = NOTIFYICON_VERSION;
		(void)Shell_NotifyIconW(NIM_SETVERSION, &versionData);
	}
	_impl->Visible = true;
	_impl->LastError = S_OK;
	RegisterIcon(this);
	return true;
}

bool NotifyIcon::TryHide()
{
	_impl->DesiredVisible = false;
	if (!_impl->Visible)
	{
		_impl->LastError = S_OK;
		return true;
	}
	if (!Shell_NotifyIconW(NIM_DELETE, &_impl->Data))
	{
		_impl->LastError = LastWin32Failure();
		return false;
	}
	_impl->Visible = false;
	_impl->LastError = S_OK;
	return true;
}

bool NotifyIcon::TrySetToolTip(const std::wstring& text)
{
	_impl->ToolTip = text;
	wcsncpy_s(
		_impl->Data.szTip, _countof(_impl->Data.szTip),
		text.c_str(), _TRUNCATE);
	if (!_impl->Visible)
	{
		_impl->LastError = S_OK;
		return true;
	}
	if (!Shell_NotifyIconW(NIM_MODIFY, &_impl->Data))
	{
		_impl->LastError = LastWin32Failure();
		return false;
	}
	_impl->LastError = S_OK;
	return true;
}

std::wstring NotifyIcon::GetToolTip() const { return _impl->ToolTip; }

bool NotifyIcon::TryShowBalloonTip(
	const std::wstring& title,
	const std::wstring& text,
	DWORD timeout,
	DWORD type)
{
	if (!_impl->Visible)
	{
		_impl->LastError = E_UNEXPECTED;
		return false;
	}
	NOTIFYICONDATAW balloon = _impl->Data;
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
		_impl->LastError = LastWin32Failure();
		return false;
	}
	_impl->LastError = S_OK;
	return true;
}

bool NotifyIcon::TryAddMenuItem(NotifyIconMenuItem item)
{
	if (!IsValidMenuTree(item))
	{
		_impl->LastError = E_INVALIDARG;
		return false;
	}
	_impl->MenuItems.push_back(std::move(item));
	_impl->LastError = S_OK;
	return true;
}

bool NotifyIcon::TryAddMenuSeparator()
{
	return TryAddMenuItem(NotifyIconMenuItem::Separator());
}

bool NotifyIcon::TryRemoveMenuItemAt(std::size_t index)
{
	if (index >= _impl->MenuItems.size())
	{
		_impl->LastError = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		return false;
	}
	_impl->MenuItems.erase(_impl->MenuItems.begin()
		+ static_cast<std::ptrdiff_t>(index));
	_impl->LastError = S_OK;
	return true;
}

void NotifyIcon::ClearMenu() noexcept
{
	_impl->MenuItems.clear();
	_impl->LastError = S_OK;
}

std::size_t NotifyIcon::MenuItemCount(bool recursive) const noexcept
{
	if (!recursive) return _impl->MenuItems.size();
	std::size_t count = 0;
	for (const auto& item : _impl->MenuItems)
		count += CountMenuTree(item);
	return count;
}

std::span<const NotifyIconMenuItem> NotifyIcon::GetMenuItems() const noexcept
{
	return std::span<const NotifyIconMenuItem>{ _impl->MenuItems };
}

bool NotifyIcon::TryShowContextMenu(int screenX, int screenY)
{
	auto operation = _impl;
	if (operation->PopupActive)
	{
		operation->LastError = HRESULT_FROM_WIN32(ERROR_BUSY);
		return false;
	}
	if (!operation->Owner || !operation->Initialized
		|| !operation->Data.hWnd || !IsWindow(operation->Data.hWnd))
	{
		operation->LastError = E_HANDLE;
		return false;
	}
	if (operation->MenuItems.empty())
	{
		operation->LastError = E_INVALIDARG;
		return false;
	}
	operation->PopupActive = true;
	struct PopupActivity final
	{
		std::shared_ptr<Impl> Operation;
		~PopupActivity() { Operation->PopupActive = false; }
	} popupActivity{ operation };
	Window* const owner = operation->Owner;
	const HWND window = operation->Data.hWnd;
	const auto operationValid = [operation, owner, window]
	{
		return operation->Initialized && operation->Owner == owner
			&& operation->Data.hWnd == window && IsWindow(window);
	};

	HMENU menu = CreatePopupMenu();
	if (!menu)
	{
		operation->LastError = LastWin32Failure();
		return false;
	}
	// Freeze one value snapshot before CanExecute starts routing. Handlers may
	// mutate the service's future menu without invalidating this native popup
	// or the transient-id map used by the current transaction.
	auto popupItems = operation->MenuItems;
	PopupCommandMap commands;
	for (auto& item : popupItems)
	{
		if (!AppendMenuTree(
			menu, item, *owner, commands, operationValid))
		{
			operation->LastError = operationValid()
				? LastWin32Failure() : E_HANDLE;
			DestroyMenu(menu);
			return false;
		}
	}
	if (!operationValid())
	{
		DestroyMenu(menu);
		operation->LastError = E_HANDLE;
		return false;
	}

	(void)SetForegroundWindow(window);
	::SetLastError(ERROR_SUCCESS);
	const UINT transientId = TrackPopupMenu(
		menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
		screenX, screenY, 0, window, nullptr);
	const DWORD trackError = ::GetLastError();
	DestroyMenu(menu);
	if (IsWindow(window)) (void)PostMessageW(window, WM_NULL, 0, 0);

	if (transientId == 0 && trackError != ERROR_SUCCESS)
	{
		operation->LastError = HRESULT_FROM_WIN32(trackError);
		return false;
	}
	if (!operationValid())
	{
		operation->LastError = E_HANDLE;
		return false;
	}
	operation->LastError = S_OK;
	if (const auto* item = commands.Resolve(transientId);
		item && operationValid())
	{
		RoutedCommandSourceQuery query{
			item->Command, item->CommandParameter, item->CommandTarget };
		(void)cui::framework::WindowAccess::Commands(
			*owner).ExecuteCommandSource(*owner, query);
	}
	return true;
}

bool NotifyIcon::HandlePlatformWindowMessage(
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
			if (!icon || !icon->_impl->Initialized
				|| icon->_impl->Data.hWnd != window) continue;
			const bool restore = icon->_impl->DesiredVisible;
			icon->_impl->Visible = false;
			if (restore) (void)icon->TryShow();
		}
		return false;
	}

	NotifyIcon* target = nullptr;
	for (auto* icon : icons)
	{
		if (icon && icon->_impl->Initialized && icon->_impl->Visible
			&& icon->_impl->DesiredVisible
			&& icon->_impl->Data.hWnd == window
			&& icon->_impl->Data.uCallbackMessage == message
			&& icon->_impl->Data.uID == static_cast<UINT>(wParam))
		{
			target = icon;
			break;
		}
	}
	if (!target) return false;

	MouseButton button = MouseButton::None;
	if (lParam == WM_LBUTTONDOWN) button = MouseButton::Left;
	else if (lParam == WM_RBUTTONDOWN) button = MouseButton::Right;
	else if (lParam == WM_MBUTTONDOWN) button = MouseButton::Middle;
	if (button != MouseButton::None)
	{
		POINT location{};
		(void)GetCursorPos(&location);
		cui::framework::EventAccess::Raise(
			target->OnMouseDown, target,
			MouseEventArgs(button, MouseButtonState::Pressed,
				1, location.x, location.y, 0));
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
