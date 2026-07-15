#include "Taskbar.h"

#include <algorithm>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#if defined(_MSC_VER)
#pragma comment(lib, "Ole32.lib")
#endif

struct Taskbar::Impl
{
	ComPtr<ITaskbarList3> taskbar;
	HRESULT initializationError = E_PENDING;
	HRESULT lastError = E_PENDING;
	bool comInitializationAttempted = false;
	bool comUsable = false;
	bool ownsComInitialization = false;
	ULONGLONG value = 0;
	ULONGLONG total = 0;
	ProgressState state = ProgressState::NoProgress;
};

namespace
{
	bool IsValidTaskbarState(TBPFLAG state)
	{
		switch (state)
		{
		case TBPF_NOPROGRESS:
		case TBPF_INDETERMINATE:
		case TBPF_NORMAL:
		case TBPF_ERROR:
		case TBPF_PAUSED:
			return true;
		default:
			return false;
		}
	}
}

Taskbar::Taskbar(HWND handle)
	: Handle(handle), _impl(std::make_unique<Impl>())
{
	(void)Initialize(handle);
}

Taskbar::~Taskbar()
{
	if (_impl->taskbar && Handle)
		(void)_impl->taskbar->SetProgressState(Handle, TBPF_NOPROGRESS);
	_impl->taskbar.Reset();
	if (_impl->ownsComInitialization)
		CoUninitialize();
}

bool Taskbar::Initialize(HWND handle)
{
	if (_impl->taskbar && Handle && Handle != handle)
		(void)_impl->taskbar->SetProgressState(Handle, TBPF_NOPROGRESS);
	Handle = handle;

	if (!_impl->taskbar)
	{
		if (!_impl->comInitializationAttempted)
		{
			_impl->comInitializationAttempted = true;
			const HRESULT comHr =
				CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			if (SUCCEEDED(comHr))
			{
				_impl->ownsComInitialization = true;
				_impl->comUsable = true;
			}
			else if (comHr == RPC_E_CHANGED_MODE)
			{
				_impl->comUsable = true;
			}
			else
			{
				_impl->initializationError = comHr;
				_impl->lastError = comHr;
				return false;
			}
		}
		if (!_impl->comUsable)
		{
			_impl->lastError = _impl->initializationError;
			return false;
		}

		ComPtr<ITaskbarList3> taskbar;
		HRESULT hr = CoCreateInstance(
			CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&taskbar));
		if (SUCCEEDED(hr) && taskbar)
			hr = taskbar->HrInit();
		if (FAILED(hr) || !taskbar)
		{
			_impl->initializationError = FAILED(hr) ? hr : E_NOINTERFACE;
			_impl->lastError = _impl->initializationError;
			return false;
		}
		_impl->taskbar = std::move(taskbar);
	}

	_impl->initializationError = S_OK;
	_impl->lastError = Handle ? S_OK : E_HANDLE;
	return Handle != nullptr;
}

bool Taskbar::IsAvailable() const noexcept
{
	return _impl->taskbar != nullptr;
}

HRESULT Taskbar::GetInitializationError() const noexcept
{
	return _impl->initializationError;
}

HRESULT Taskbar::GetLastError() const noexcept
{
	return _impl->lastError;
}

bool Taskbar::TrySetValue(ULONGLONG value, ULONGLONG total)
{
	if (!_impl->taskbar)
	{
		_impl->lastError = FAILED(_impl->initializationError)
			? _impl->initializationError : E_NOINTERFACE;
		return false;
	}
	if (!Handle)
	{
		_impl->lastError = E_HANDLE;
		return false;
	}
	if (total == 0)
	{
		_impl->lastError = E_INVALIDARG;
		return false;
	}

	value = (std::min)(value, total);
	if (_impl->state == ProgressState::NoProgress
		|| _impl->state == ProgressState::Indeterminate)
	{
		const HRESULT stateHr =
			_impl->taskbar->SetProgressState(Handle, TBPF_NORMAL);
		if (FAILED(stateHr))
		{
			_impl->lastError = stateHr;
			return false;
		}
		_impl->state = ProgressState::Normal;
	}

	const HRESULT hr = _impl->taskbar->SetProgressValue(Handle, value, total);
	_impl->lastError = hr;
	if (FAILED(hr)) return false;
	_impl->value = value;
	_impl->total = total;
	return true;
}

bool Taskbar::TrySetState(ProgressState state)
{
	return TrySetState(static_cast<TBPFLAG>(state));
}

bool Taskbar::TrySetState(TBPFLAG state)
{
	if (!IsValidTaskbarState(state))
	{
		_impl->lastError = E_INVALIDARG;
		return false;
	}
	if (!_impl->taskbar)
	{
		_impl->lastError = FAILED(_impl->initializationError)
			? _impl->initializationError : E_NOINTERFACE;
		return false;
	}
	if (!Handle)
	{
		_impl->lastError = E_HANDLE;
		return false;
	}

	const HRESULT hr = _impl->taskbar->SetProgressState(Handle, state);
	_impl->lastError = hr;
	if (FAILED(hr)) return false;
	_impl->state = static_cast<ProgressState>(state);
	if (state == TBPF_NOPROGRESS)
	{
		_impl->value = 0;
		_impl->total = 0;
	}
	return true;
}

bool Taskbar::TryClear() { return TrySetState(ProgressState::NoProgress); }
bool Taskbar::TrySetIndeterminate()
{
	return TrySetState(ProgressState::Indeterminate);
}
bool Taskbar::TrySetPaused() { return TrySetState(ProgressState::Paused); }
bool Taskbar::TrySetError() { return TrySetState(ProgressState::Error); }
bool Taskbar::TrySetNormal() { return TrySetState(ProgressState::Normal); }

void Taskbar::SetValue(ULONGLONG value, ULONGLONG total)
{
	(void)TrySetValue(value, total);
}
void Taskbar::SetState(ProgressState state) { (void)TrySetState(state); }
void Taskbar::SetState(TBPFLAG state) { (void)TrySetState(state); }
void Taskbar::Clear() { (void)TryClear(); }
void Taskbar::SetIndeterminate() { (void)TrySetIndeterminate(); }
void Taskbar::SetPaused() { (void)TrySetPaused(); }
void Taskbar::SetError() { (void)TrySetError(); }
void Taskbar::SetNormal() { (void)TrySetNormal(); }

ULONGLONG Taskbar::GetValue() const noexcept { return _impl->value; }
ULONGLONG Taskbar::GetTotal() const noexcept { return _impl->total; }
Taskbar::ProgressState Taskbar::GetState() const noexcept
{
	return _impl->state;
}
