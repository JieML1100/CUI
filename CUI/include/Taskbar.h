#pragma once

#include <windows.h>
#include <shobjidl.h>

#include <memory>

/**
 * @file Taskbar.h
 * @brief ITaskbarList3 progress wrapper with per-instance COM ownership.
 */
class Taskbar
{
public:
	enum class ProgressState : int
	{
		NoProgress = TBPF_NOPROGRESS,
		Indeterminate = TBPF_INDETERMINATE,
		Normal = TBPF_NORMAL,
		Error = TBPF_ERROR,
		Paused = TBPF_PAUSED
	};

	explicit Taskbar(HWND handle = nullptr);
	~Taskbar();
	Taskbar(const Taskbar&) = delete;
	Taskbar& operator=(const Taskbar&) = delete;
	Taskbar(Taskbar&&) = delete;
	Taskbar& operator=(Taskbar&&) = delete;

	/** Compatibility field. Prefer Initialize() when changing the target. */
	HWND Handle = nullptr;

	/** Initializes COM/taskbar services and attaches a target window. */
	bool Initialize(HWND handle);
	bool IsAvailable() const noexcept;
	HRESULT GetInitializationError() const noexcept;
	HRESULT GetLastError() const noexcept;

	bool TrySetValue(ULONGLONG value, ULONGLONG total);
	bool TrySetState(ProgressState state);
	bool TrySetState(TBPFLAG state);
	bool TryClear();
	bool TrySetIndeterminate();
	bool TrySetPaused();
	bool TrySetError();
	bool TrySetNormal();

	/** Compatibility operations; failures remain available through GetLastError(). */
	void SetValue(ULONGLONG value, ULONGLONG total);
	void SetState(ProgressState state);
	void SetState(TBPFLAG state);
	void Clear();
	void SetIndeterminate();
	void SetPaused();
	void SetError();
	void SetNormal();

	ULONGLONG GetValue() const noexcept;
	ULONGLONG GetTotal() const noexcept;
	ProgressState GetState() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};
