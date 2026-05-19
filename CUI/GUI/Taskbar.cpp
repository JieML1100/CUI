#include "Taskbar.h"

ITaskbarList3* Taskbar::pTaskbarList = nullptr;
Taskbar::Taskbar(HWND handle)
{
    this->Handle = handle;
    if (!pTaskbarList)
    {
        CoInitialize(nullptr);
        CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pTaskbarList));
    }
}
void Taskbar::SetValue(ULONGLONG value, ULONGLONG total)
{
    pTaskbarList->SetProgressValue(Handle, value, total);
}
Taskbar::~Taskbar()
{
    pTaskbarList->SetProgressState(Handle, TBPF_NOPROGRESS);
    pTaskbarList->Release();
    
}