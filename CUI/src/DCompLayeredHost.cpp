#include "DCompLayeredHost.h"
#include "Graphics.h"

#ifdef CUI_ENABLE_WEBVIEW2
#include <dcomp.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <algorithm>
#include <vector>
#include <wrl/client.h>

#if defined(_MSC_VER)
#pragma comment(lib, "dxgi.lib")
#endif

namespace
{
    using DCompositionCreateDeviceProc = HRESULT(WINAPI*)(IDXGIDevice*, REFIID, void**);

    DCompositionCreateDeviceProc ResolveDCompositionCreateDevice()
    {
        static HMODULE dcompModule = ::LoadLibraryW(L"dcomp.dll");
        if (!dcompModule)
            return nullptr;
        return reinterpret_cast<DCompositionCreateDeviceProc>(::GetProcAddress(dcompModule, "DCompositionCreateDevice"));
    }

}
#endif

class DCompLayeredHost::Impl
{
public:
#ifdef CUI_ENABLE_WEBVIEW2
    struct LayerVisual
    {
        IDCompositionVisual* visual = nullptr;
        int layer = 0;
        int order = 0;
        unsigned long long sequence = 0;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
    GraphicsSharedD3DDeviceInfo sharedDeviceInfo{};
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> webContainerVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> d2dVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> overlayVisual;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> overlaySwapChain;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1 layerSwapChainDesc{};
    std::vector<LayerVisual> layerVisuals;
    unsigned long long nextSequence = 1;

    LayerVisual* FindLayerVisual(IDCompositionVisual* visual)
    {
        auto it = std::find_if(layerVisuals.begin(), layerVisuals.end(), [visual](const LayerVisual& item)
            {
                return item.visual == visual;
            });
        return it == layerVisuals.end() ? nullptr : &(*it);
    }

    HRESULT RebuildVisualStack()
    {
        if (!rootVisual)
            return E_NOT_VALID_STATE;
        std::stable_sort(layerVisuals.begin(), layerVisuals.end(), [](const LayerVisual& a, const LayerVisual& b)
            {
                if (a.layer != b.layer) return a.layer < b.layer;
                if (a.order != b.order) return a.order < b.order;
                return a.sequence < b.sequence;
            });

		HRESULT hr = rootVisual->RemoveAllVisuals();
		if (FAILED(hr)) return hr;
        for (const auto& item : layerVisuals)
        {
            if (item.visual)
			{
				// The list is sorted bottom-to-top.  With no reference visual,
				// insertAbove=FALSE places the new visual above all existing
				// siblings, preserving that order.
				hr = rootVisual->AddVisual(item.visual, FALSE, nullptr);
				if (FAILED(hr)) return hr;
			}
        }
		return S_OK;
    }
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
    // A previous initialization attempt may have failed after creating only a
    // prefix of the composition graph. Retry from a clean device-domain state.
    Cleanup();
    if (!hwnd || !::IsWindow(hwnd))
        return false;

    _impl->hwnd = hwnd;

    // DirectComposition、D2D 和媒体解码必须处在同一 D3D11 设备域，
    // 这样 DXGI 视频表面才能无 CPU 拷贝地交给任意一种呈现宿主。
    HRESULT hr = Graphics_AcquireSharedD3DDevice(
        _impl->d3dDevice.ReleaseAndGetAddressOf(),
        nullptr,
        _impl->dxgiDevice.ReleaseAndGetAddressOf(),
        _impl->d2dDevice.ReleaseAndGetAddressOf(),
        &_impl->sharedDeviceInfo);
    if (FAILED(hr) || !_impl->d3dDevice || !_impl->dxgiDevice || !_impl->d2dDevice)
        return false;

    // 创建 DComp 设备
    auto createDCompositionDevice = ResolveDCompositionCreateDevice();
    if (!createDCompositionDevice)
        return false;

    hr = createDCompositionDevice(
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
	hr = _impl->dcompTarget->SetRoot(_impl->rootVisual.Get());
	if (FAILED(hr)) return false;

    // 创建 D2D Visual（底层，用于自绘渲染）
    hr = _impl->dcompDevice->CreateVisual(_impl->d2dVisual.GetAddressOf());
    if (FAILED(hr))
        return false;

    // Obtain the factory through the shared device's adapter so all
    // composition swap chains remain in the same adapter domain.
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = _impl->dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr))
        return false;
    hr = adapter->GetParent(
        __uuidof(IDXGIFactory2),
        reinterpret_cast<void**>(_impl->dxgiFactory.ReleaseAndGetAddressOf()));
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
    _impl->layerSwapChainDesc = desc;

    hr = _impl->dxgiFactory->CreateSwapChainForComposition(
        _impl->d3dDevice.Get(),
        &desc,
        nullptr,
        _impl->swapChain.GetAddressOf());
    if (FAILED(hr))
        return false;

	hr = _impl->d2dVisual->SetContent(_impl->swapChain.Get());
	if (FAILED(hr)) return false;
    _impl->layerVisuals.push_back({ _impl->d2dVisual.Get(), 0, 0, _impl->nextSequence++ });

    // 创建 WebContainer Visual（中间层，用于挂载 WebView2）
    hr = _impl->dcompDevice->CreateVisual(_impl->webContainerVisual.GetAddressOf());
    if (FAILED(hr))
        return false;

    _impl->layerVisuals.push_back({ _impl->webContainerVisual.Get(), 100000, 0, _impl->nextSequence++ });

    // 创建 Overlay Visual（顶层，用于前景控件/菜单，必须覆盖 WebView2）
    hr = _impl->dcompDevice->CreateVisual(_impl->overlayVisual.GetAddressOf());
    if (FAILED(hr))
        return false;

    DXGI_SWAP_CHAIN_DESC1 overlayDesc = desc;
    hr = _impl->dxgiFactory->CreateSwapChainForComposition(
        _impl->d3dDevice.Get(),
        &overlayDesc,
        nullptr,
        _impl->overlaySwapChain.GetAddressOf());
    if (FAILED(hr))
        return false;

	hr = _impl->overlayVisual->SetContent(_impl->overlaySwapChain.Get());
	if (FAILED(hr)) return false;
    _impl->layerVisuals.push_back({ _impl->overlayVisual.Get(), 200000, 0, _impl->nextSequence++ });
	if (FAILED(_impl->RebuildVisualStack()))
		return false;

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
    if (_impl->overlaySwapChain)
        _impl->overlaySwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    _impl->layerSwapChainDesc.Width = width;
    _impl->layerSwapChainDesc.Height = height;
#else
    (void)width;
    (void)height;
#endif
}

void DCompLayeredHost::UpdateD2DLayerSize(UINT width, UINT height)
{
    (width);
    (height);
#ifdef CUI_ENABLE_WEBVIEW2
    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    _impl->layerSwapChainDesc.Width = width;
    _impl->layerSwapChainDesc.Height = height;
#else
    (void)width;
    (void)height;
#endif
}

void DCompLayeredHost::Cleanup()
{
#ifdef CUI_ENABLE_WEBVIEW2
    _impl->layerVisuals.clear();
    _impl->overlayVisual.Reset();
    _impl->webContainerVisual.Reset();
    _impl->d2dVisual.Reset();
    _impl->rootVisual.Reset();
    _impl->dcompTarget.Reset();
    _impl->overlaySwapChain.Reset();
    _impl->swapChain.Reset();
    _impl->dxgiFactory.Reset();
    _impl->dcompDevice.Reset();
    _impl->d2dDevice.Reset();
    _impl->dxgiDevice.Reset();
    _impl->d3dDevice.Reset();
    _impl->sharedDeviceInfo = {};
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

bool DCompLayeredHost::CreateD2DLayer(void** outSwapChain, IDCompositionVisual** outVisual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (outSwapChain) *outSwapChain = nullptr;
    if (outVisual) *outVisual = nullptr;
    if (!_impl->d3dDevice || !_impl->dcompDevice || !_impl->dxgiFactory || !outSwapChain || !outVisual)
        return false;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    auto desc = _impl->layerSwapChainDesc;
    if (desc.Width == 0) desc.Width = 1;
    if (desc.Height == 0) desc.Height = 1;
    HRESULT hr = _impl->dxgiFactory->CreateSwapChainForComposition(
        _impl->d3dDevice.Get(),
        &desc,
        nullptr,
        swapChain.GetAddressOf());
    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<IDCompositionVisual> visual;
    hr = _impl->dcompDevice->CreateVisual(visual.GetAddressOf());
    if (FAILED(hr) || !visual)
        return false;

	hr = visual->SetContent(swapChain.Get());
	if (FAILED(hr)) return false;
	if (!RegisterVisual(visual.Get(), layer, order))
		return false;
    *outSwapChain = swapChain.Detach();
    *outVisual = visual.Detach();
    return true;
#else
    if (outSwapChain) *outSwapChain = nullptr;
    if (outVisual) *outVisual = nullptr;
    (void)layer;
    (void)order;
    return false;
#endif
}

void DCompLayeredHost::DestroyD2DLayer(IDCompositionVisual* visual)
{
#ifdef CUI_ENABLE_WEBVIEW2
    UnregisterVisual(visual);
#else
    (void)visual;
#endif
}

bool DCompLayeredHost::RegisterVisual(IDCompositionVisual* visual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (!_impl->rootVisual || !visual)
        return false;
    if (auto* item = _impl->FindLayerVisual(visual))
    {
		const int previousLayer = item->layer;
		const int previousOrder = item->order;
		const uint64_t sequence = item->sequence;
        item->layer = layer;
        item->order = order;
		if (SUCCEEDED(_impl->RebuildVisualStack())) return true;
		const auto restore = std::find_if(
			_impl->layerVisuals.begin(), _impl->layerVisuals.end(),
			[sequence](const Impl::LayerVisual& candidate)
			{ return candidate.sequence == sequence; });
		if (restore != _impl->layerVisuals.end())
		{
			restore->layer = previousLayer;
			restore->order = previousOrder;
		}
		(void)_impl->RebuildVisualStack();
		return false;
    }
	const uint64_t sequence = _impl->nextSequence++;
	_impl->layerVisuals.push_back({
		visual, layer, order, sequence });
	if (SUCCEEDED(_impl->RebuildVisualStack())) return true;
	_impl->layerVisuals.erase(std::remove_if(
		_impl->layerVisuals.begin(), _impl->layerVisuals.end(),
		[sequence](const Impl::LayerVisual& candidate)
		{ return candidate.sequence == sequence; }),
		_impl->layerVisuals.end());
	(void)_impl->RebuildVisualStack();
	return false;
#else
    (void)visual;
    (void)layer;
    (void)order;
    return false;
#endif
}

void DCompLayeredHost::UpdateVisualOrder(IDCompositionVisual* visual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (!_impl->rootVisual || !visual)
        return;
    if (auto* item = _impl->FindLayerVisual(visual))
    {
        if (item->layer == layer && item->order == order)
            return;
		const int previousLayer = item->layer;
		const int previousOrder = item->order;
		const uint64_t sequence = item->sequence;
		item->layer = layer;
		item->order = order;
		if (FAILED(_impl->RebuildVisualStack()))
		{
			const auto restore = std::find_if(
				_impl->layerVisuals.begin(), _impl->layerVisuals.end(),
				[sequence](const Impl::LayerVisual& candidate)
				{ return candidate.sequence == sequence; });
			if (restore != _impl->layerVisuals.end())
			{
				restore->layer = previousLayer;
				restore->order = previousOrder;
			}
			(void)_impl->RebuildVisualStack();
		}
    }
    else
    {
        RegisterVisual(visual, layer, order);
    }
#else
    (void)visual;
    (void)layer;
    (void)order;
#endif
}

void DCompLayeredHost::UnregisterVisual(IDCompositionVisual* visual)
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (!visual)
        return;
    auto oldSize = _impl->layerVisuals.size();
    _impl->layerVisuals.erase(
        std::remove_if(_impl->layerVisuals.begin(), _impl->layerVisuals.end(), [visual](const Impl::LayerVisual& item)
            {
                return item.visual == visual;
            }),
        _impl->layerVisuals.end());
    if (oldSize != _impl->layerVisuals.size())
		(void)_impl->RebuildVisualStack();
#else
    (void)visual;
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

void* DCompLayeredHost::GetOverlaySwapChain() const
{
#ifdef CUI_ENABLE_WEBVIEW2
    return _impl->overlaySwapChain.Get();
#else
    return nullptr;
#endif
}

HRESULT DCompLayeredHost::CommitComposition()
{
#ifdef CUI_ENABLE_WEBVIEW2
    if (_impl->dcompDevice)
    {
        return _impl->dcompDevice->Commit();
    }
#endif
	return S_OK;
}

bool DCompLayeredHost::IsInitialized() const
{
    return _impl->initialized;
}

bool DCompLayeredHost::IsDeviceLost() const noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_impl->sharedDeviceInfo.Generation != 0
		&& Graphics_GetSharedD3DDeviceGeneration()
			!= _impl->sharedDeviceInfo.Generation)
	{
		return true;
	}
    return _impl->d3dDevice
        && FAILED(_impl->d3dDevice->GetDeviceRemovedReason());
#else
    return false;
#endif
}
