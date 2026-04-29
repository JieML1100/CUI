#include "Taskbar.h"

ITaskbarList3* Taskbar::pTaskbarList = NULL;
Taskbar::Taskbar(HWND handle)
{
    this->Handle = handle;
    if (!pTaskbarList)
    {
        CoInitialize(NULL);
        CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pTaskbarList));
    }
}
void Taskbar::SetValue(ULONGLONG value, ULONGLONG total)
{
    if (pTaskbarList && Handle)
        pTaskbarList->SetProgressValue(Handle, value, total);
}

void Taskbar::SetState(TBPFLAG state)
{
    if (pTaskbarList && Handle)
        pTaskbarList->SetProgressState(Handle, state);
}

void Taskbar::Clear()
{
    SetState(TBPF_NOPROGRESS);
}

void Taskbar::SetIndeterminate()
{
    SetState(TBPF_INDETERMINATE);
}

void Taskbar::SetPaused()
{
    SetState(TBPF_PAUSED);
}

void Taskbar::SetError()
{
    SetState(TBPF_ERROR);
}

void Taskbar::SetNormal()
{
    SetState(TBPF_NORMAL);
}
Taskbar::~Taskbar()
{
    Clear();
}
