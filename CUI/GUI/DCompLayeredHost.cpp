#include "DCompLayeredHost.h"

#ifdef CUI_ENABLE_WEBVIEW2
#include <dcomp.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <wrl/client.h>

#if defined(_MSC_VER)
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif
#endif

class DCompLayeredHost::Impl
{
public:
#ifdef CUI_ENABLE_WEBVIEW2
    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> webContainerVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> d2dVisual;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
#endif
    bool initialized = false;
    HWND hwnd = nullptr;
};

DCompLayeredHost::DCompLayeredHost()
    : _impl(new Impl())
{
}

DCompLayeredHost::~DCompLayeredHost()
{
    Cleanup();
    delete _impl;
}

bool DCompLayeredHost::Initialize(HWND hwnd, UINT width, UINT height)
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (_impl->initialized)
        return true;
    if (!hwnd || !::IsWindow(hwnd))
        return false;

    _impl->hwnd = hwnd;

    // 创建 D3D11 设备
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL obtainedFeatureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        static_cast<UINT>(_countof(featureLevels)),
        D3D11_SDK_VERSION,
        _impl->d3dDevice.GetAddressOf(),
        &obtainedFeatureLevel,
        nullptr);

    if (FAILED(hr))
    {
        // 回退到 WARP
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevels,
            static_cast<UINT>(_countof(featureLevels)),
            D3D11_SDK_VERSION,
            &_impl->d3dDevice,
            &obtainedFeatureLevel,
            nullptr);
        if (FAILED(hr))
            return false;
    }

    hr = _impl->d3dDevice.As(&_impl->dxgiDevice);
    if (FAILED(hr))
        return false;

    // 创建 D2D 设备（与 D2DGraphics 共享 DXGI 设备）
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory.GetAddressOf());
    if (FAILED(hr))
        return false;

    hr = d2dFactory->CreateDevice(_impl->dxgiDevice.Get(), _impl->d2dDevice.GetAddressOf());
    if (FAILED(hr))
        return false;

    // 创建 DComp 设备
    hr = DCompositionCreateDevice(
        _impl->dxgiDevice.Get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(_impl->dcompDevice.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
        return false;

    // 创建 DComp 目标并绑定到 HWND
    hr = _impl->dcompDevice->CreateTargetForHwnd(hwnd, FALSE, _impl->dcompTarget.GetAddressOf());
    if (FAILED(hr))
        return false;

    // 创建根 Visual
    hr = _impl->dcompDevice->CreateVisual(_impl->rootVisual.GetAddressOf());
    if (FAILED(hr))
        return false;
    _impl->dcompTarget->SetRoot(_impl->rootVisual.Get());

    // 创建 D2D Visual（底层，用于自绘渲染）
    hr = _impl->dcompDevice->CreateVisual(_impl->d2dVisual.GetAddressOf());
    if (FAILED(hr))
        return false;

    // 创建 Composition 交换链
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()));
    if (FAILED(hr))
        return false;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = dxgiFactory->CreateSwapChainForComposition(
        _impl->d3dDevice.Get(),
        &desc,
        nullptr,
        _impl->swapChain.GetAddressOf());
    if (FAILED(hr))
        return false;

    _impl->d2dVisual->SetContent(_impl->swapChain.Get());
    _impl->rootVisual->AddVisual(_impl->d2dVisual.Get(), FALSE, nullptr);

    // 创建 WebContainer Visual（顶层，用于挂载 WebView2）
    hr = _impl->dcompDevice->CreateVisual(_impl->webContainerVisual.GetAddressOf());
    if (FAILED(hr))
        return false;

    // webContainerVisual 在 d2dVisual 之上
    _impl->rootVisual->AddVisual(_impl->webContainerVisual.Get(), TRUE, _impl->d2dVisual.Get());

    _impl->initialized = true;
    return true;
#else
    (void)hwnd;
    (void)width;
    (void)height;
    return false;
#endif
}

void DCompLayeredHost::Resize(UINT width, UINT height)
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (!_impl->swapChain)
        return;
    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    _impl->swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
#else
    (void)width;
    (void)height;
#endif
}

void DCompLayeredHost::Cleanup()
{
#ifdef CUI_ENABLE_WEBVIEW2
    _impl->webContainerVisual.Reset();
    _impl->d2dVisual.Reset();
    _impl->rootVisual.Reset();
    _impl->dcompTarget.Reset();
    _impl->swapChain.Reset();
    _impl->dcompDevice.Reset();
    _impl->d2dDevice.Reset();
    _impl->dxgiDevice.Reset();
    _impl->d3dDevice.Reset();
#endif
    _impl->initialized = false;
    _impl->hwnd = nullptr;
}

IDCompositionDevice* DCompLayeredHost::GetDCompDevice() const
{
#ifdef CUI_ENABLE_WEBVIEW2
    return _impl->dcompDevice.Get();
#else
    return nullptr;
#endif
}

IDCompositionVisual* DCompLayeredHost::GetRootVisual() const
{
#ifdef CUI_ENABLE_WEBVIEW2
    return _impl->rootVisual.Get();
#else
    return nullptr;
#endif
}

IDCompositionVisual* DCompLayeredHost::GetWebContainerVisual() const
{
#ifdef CUI_ENABLE_WEBVIEW2
    return _impl->webContainerVisual.Get();
#else
    return nullptr;
#endif
}

void* DCompLayeredHost::GetSwapChain() const
{
#ifdef CUI_ENABLE_WEBVIEW2
    return _impl->swapChain.Get();
#else
    return nullptr;
#endif
}

void DCompLayeredHost::CommitComposition()
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (_impl->dcompDevice)
    {
        _impl->dcompDevice->Commit();
    }
#endif
}

bool DCompLayeredHost::IsInitialized() const
{
    return _impl->initialized;
}
