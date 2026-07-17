#pragma once
#include <windows.h>

struct IDCompositionDevice;
struct IDCompositionVisual;

/**
 * @file DCompLayeredHost.h
 * @brief Form 的 DirectComposition 宿主，用于承载 WebView2 Composition 模式。
 *
 * 当定义了 CUI_ENABLE_WEBVIEW2 且运行环境支持时，本类会创建 DComp 设备、交换链和 Visual 树；
 * 未定义或运行时不可用时操作会失败/为空，普通窗口仍可走传统 D2D 渲染路径。
 */
class DCompLayeredHost
{
public:
    DCompLayeredHost();
    ~DCompLayeredHost();

    // 裸 Impl* PIMPL：按值拷贝会双重释放，禁止拷贝（移动操作也随之隐式抑制）。
    DCompLayeredHost(const DCompLayeredHost&) = delete;
    DCompLayeredHost& operator=(const DCompLayeredHost&) = delete;

    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Resize(UINT width, UINT height);
    void UpdateD2DLayerSize(UINT width, UINT height);
    void Cleanup();

    IDCompositionDevice* GetDCompDevice() const;
    IDCompositionVisual* GetRootVisual() const;
    IDCompositionVisual* GetWebContainerVisual() const;
    bool CreateD2DLayer(void** outSwapChain, IDCompositionVisual** outVisual, int layer, int order);
    void DestroyD2DLayer(IDCompositionVisual* visual);
    bool RegisterVisual(IDCompositionVisual* visual, int layer, int order);
    void UpdateVisualOrder(IDCompositionVisual* visual, int layer, int order);
    void UnregisterVisual(IDCompositionVisual* visual);
    void* GetSwapChain() const; // 实际类型为 IDXGISwapChain1*
    void* GetOverlaySwapChain() const; // 实际类型为 IDXGISwapChain1*

    void CommitComposition();
    bool IsInitialized() const;

private:
    class Impl;
    Impl* _impl;
};
