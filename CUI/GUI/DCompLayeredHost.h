#pragma once
#include <windows.h>

struct IDCompositionDevice;
struct IDCompositionVisual;

/**
 * @file DCompLayeredHost.h
 * @brief Form 的 DirectComposition 宿主，用于承载 WebView2 Composition 模式。
 *
 * 当定义了 CUI_ENABLE_WEBVIEW2 时，本类会创建 DComp 设备、交换链和 Visual 树；
 * 未定义时所有操作均为空实现，确保在 Windows 7 等无 DComp 环境下可正常编译。
 */
class DCompLayeredHost
{
public:
    DCompLayeredHost();
    ~DCompLayeredHost();

    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Resize(UINT width, UINT height);
    void Cleanup();

    IDCompositionDevice* GetDCompDevice() const;
    IDCompositionVisual* GetRootVisual() const;
    IDCompositionVisual* GetWebContainerVisual() const;
    void* GetSwapChain() const; // 实际类型为 IDXGISwapChain1*

    void CommitComposition();
    bool IsInitialized() const;

private:
    class Impl;
    Impl* _impl;
};
