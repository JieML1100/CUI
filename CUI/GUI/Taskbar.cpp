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
    pTaskbarList->SetProgressValue(Handle, value, total);
}
Taskbar::~Taskbar()
{
    pTaskbarList->SetProgressState(Handle, TBPF_NOPROGRESS);
    pTaskbarList->Release();
    
}