#include "DCompLayeredHost.h"
#include "Graphics.h"

#ifdef CUI_ENABLE_WEBVIEW2
#include <dcomp.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <algorithm>
#include <atomic>
#include <vector>
#include <wrl/client.h>

#if defined(_MSC_VER)
#pragma comment(lib, "dxgi.lib")
#endif

namespace
{
    using DCompositionCreateDeviceProc = HRESULT(WINAPI*)(IDXGIDevice*, REFIID, void**);
	using DCompositionCreateDevice2Proc = HRESULT(WINAPI*)(IUnknown*, REFIID, void**);

    DCompositionCreateDeviceProc ResolveDCompositionCreateDevice()
    {
        static HMODULE dcompModule = ::LoadLibraryW(L"dcomp.dll");
        if (!dcompModule)
            return nullptr;
        return reinterpret_cast<DCompositionCreateDeviceProc>(::GetProcAddress(dcompModule, "DCompositionCreateDevice"));
    }

	DCompositionCreateDevice2Proc ResolveDCompositionCreateDevice2()
	{
		HMODULE dcompModule = ::GetModuleHandleW(L"dcomp.dll");
		if (!dcompModule) dcompModule = ::LoadLibraryW(L"dcomp.dll");
		if (!dcompModule) return nullptr;
		return reinterpret_cast<DCompositionCreateDevice2Proc>(
			::GetProcAddress(dcompModule, "DCompositionCreateDevice2"));
	}

	std::atomic<bool> FailNextVisualTopologyBatchCommit{ false };

	class ScopedMicrosecondAccumulator final
	{
	public:
		explicit ScopedMicrosecondAccumulator(double* accumulator) noexcept
			: _accumulator(accumulator)
		{
			(void)::QueryPerformanceCounter(&_start);
		}

		~ScopedMicrosecondAccumulator()
		{
			if (!_accumulator) return;
			LARGE_INTEGER end{};
			if (!::QueryPerformanceCounter(&end)) return;
			static const double frequency = []() noexcept
			{
				LARGE_INTEGER value{};
				return ::QueryPerformanceFrequency(&value) && value.QuadPart > 0
					? static_cast<double>(value.QuadPart) : 0.0;
			}();
			if (frequency <= 0.0) return;
			*_accumulator += static_cast<double>(
				end.QuadPart - _start.QuadPart) * 1'000'000.0 / frequency;
		}

	private:
		double* _accumulator = nullptr;
		LARGE_INTEGER _start{};
	};

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
	VisualTopologyStatistics topologyStatistics;
	bool topologyBatchActive = false;
	bool topologyBatchDirty = false;
	std::vector<LayerVisual> topologyBatchSnapshot;
	unsigned long long topologyBatchNextSequence = 1;
	bool topologyBatchRestoreFailed = false;

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
		++topologyStatistics.StackRebuildCount;
		topologyStatistics.StackRebuildEntryCount += layerVisuals.size();
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
	bool supportsD2DSurfaceDeviceContexts = false;
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

    // Prefer the v2 factory associated with the shared Direct2D device. This
    // keeps the v1 device interface used by the rest of the host while allowing
    // surface BeginDraw to return an already-targeted ID2D1DeviceContext. Older
    // systems retain the existing DXGI-backed v1 fallback.
	if (auto createDCompositionDevice2 = ResolveDCompositionCreateDevice2())
	{
		hr = createDCompositionDevice2(
			_impl->d2dDevice.Get(),
			__uuidof(IDCompositionDevice),
			reinterpret_cast<void**>(
				_impl->dcompDevice.ReleaseAndGetAddressOf()));
		_impl->supportsD2DSurfaceDeviceContexts = SUCCEEDED(hr);
	}
	else
		hr = E_NOINTERFACE;
	if (FAILED(hr))
	{
		auto createDCompositionDevice = ResolveDCompositionCreateDevice();
		if (!createDCompositionDevice) return false;
		hr = createDCompositionDevice(
			_impl->dxgiDevice.Get(),
			__uuidof(IDCompositionDevice),
			reinterpret_cast<void**>(
				_impl->dcompDevice.ReleaseAndGetAddressOf()));
		_impl->supportsD2DSurfaceDeviceContexts = false;
	}
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
	_impl->topologyStatistics = {};
	_impl->topologyBatchActive = false;
	_impl->topologyBatchDirty = false;
	_impl->topologyBatchSnapshot.clear();
	_impl->topologyBatchNextSequence = 1;
	_impl->topologyBatchRestoreFailed = false;
	_impl->supportsD2DSurfaceDeviceContexts = false;
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

ID2D1Device* DCompLayeredHost::GetD2DDevice() const
{
#ifdef CUI_ENABLE_WEBVIEW2
	return _impl->d2dDevice.Get();
#else
	return nullptr;
#endif
}

bool DCompLayeredHost::SupportsD2DSurfaceDeviceContexts() const noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	return _impl && _impl->supportsD2DSurfaceDeviceContexts;
#else
	return false;
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

bool DCompLayeredHost::CreateD2DLayer(
    void** outSwapChain,
    IDCompositionVisual** outRootVisual,
    IDCompositionVisual** outContentVisual,
    UINT width,
    UINT height,
    int layer,
    int order,
	D2DLayerCreationTimings* timings)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (timings) *timings = {};
    if (outSwapChain) *outSwapChain = nullptr;
    if (outRootVisual) *outRootVisual = nullptr;
    if (outContentVisual) *outContentVisual = nullptr;
    if (!_impl->d3dDevice || !_impl->dcompDevice || !_impl->dxgiFactory
        || !outSwapChain || !outRootVisual || !outContentVisual)
        return false;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    auto desc = _impl->layerSwapChainDesc;
    desc.Width = (std::max)(UINT{ 1 }, width);
    desc.Height = (std::max)(UINT{ 1 }, height);
	HRESULT hr = S_OK;
	{
		ScopedMicrosecondAccumulator timing(timings
			? &timings->SwapChainMicroseconds : nullptr);
		hr = _impl->dxgiFactory->CreateSwapChainForComposition(
			_impl->d3dDevice.Get(),
			&desc,
			nullptr,
			swapChain.GetAddressOf());
	}
    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<IDCompositionVisual> rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual> contentVisual;
	{
		ScopedMicrosecondAccumulator timing(timings
			? &timings->VisualCreationMicroseconds : nullptr);
		hr = _impl->dcompDevice->CreateVisual(rootVisual.GetAddressOf());
		if (SUCCEEDED(hr) && rootVisual)
			hr = _impl->dcompDevice->CreateVisual(contentVisual.GetAddressOf());
	}
	if (FAILED(hr) || !rootVisual || !contentVisual) return false;
	{
		ScopedMicrosecondAccumulator timing(timings
			? &timings->VisualBindingMicroseconds : nullptr);
		hr = contentVisual->SetContent(swapChain.Get());
		if (SUCCEEDED(hr))
			hr = rootVisual->AddVisual(contentVisual.Get(), FALSE, nullptr);
		if (SUCCEEDED(hr) && !RegisterVisual(rootVisual.Get(), layer, order))
			hr = E_FAIL;
	}
	if (FAILED(hr)) return false;
    *outSwapChain = swapChain.Detach();
    *outRootVisual = rootVisual.Detach();
    *outContentVisual = contentVisual.Detach();
    return true;
#else
    if (outSwapChain) *outSwapChain = nullptr;
    if (outRootVisual) *outRootVisual = nullptr;
    if (outContentVisual) *outContentVisual = nullptr;
    (void)layer;
    (void)order;
    (void)width;
    (void)height;
	(void)timings;
    return false;
#endif
}

bool DCompLayeredHost::CreateD2DSurfaceLayer(
	IDCompositionSurface** outSurface,
	IDCompositionVisual** outRootVisual,
	IDCompositionVisual** outContentVisual,
	UINT width,
	UINT height,
	int layer,
	int order,
	D2DLayerCreationTimings* timings)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (timings) *timings = {};
	if (outSurface) *outSurface = nullptr;
	if (outRootVisual) *outRootVisual = nullptr;
	if (outContentVisual) *outContentVisual = nullptr;
	if (!_impl->dcompDevice || !outSurface || !outRootVisual
		|| !outContentVisual) return false;

	Microsoft::WRL::ComPtr<IDCompositionVirtualSurface> virtualSurface;
	HRESULT hr = S_OK;
	{
		ScopedMicrosecondAccumulator timing(timings
			? &timings->SurfaceMicroseconds : nullptr);
		hr = _impl->dcompDevice->CreateVirtualSurface(
			(std::max)(UINT{ 1 }, width),
			(std::max)(UINT{ 1 }, height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			DXGI_ALPHA_MODE_PREMULTIPLIED,
			virtualSurface.GetAddressOf());
	}
	if (FAILED(hr) || !virtualSurface) return false;
	Microsoft::WRL::ComPtr<IDCompositionSurface> surface;
	if (FAILED(virtualSurface.As(&surface)) || !surface) return false;

	Microsoft::WRL::ComPtr<IDCompositionVisual> rootVisual;
	Microsoft::WRL::ComPtr<IDCompositionVisual> contentVisual;
	{
		ScopedMicrosecondAccumulator timing(timings
			? &timings->VisualCreationMicroseconds : nullptr);
		hr = _impl->dcompDevice->CreateVisual(rootVisual.GetAddressOf());
		if (SUCCEEDED(hr) && rootVisual)
			hr = _impl->dcompDevice->CreateVisual(contentVisual.GetAddressOf());
	}
	if (FAILED(hr) || !rootVisual || !contentVisual) return false;
	{
		ScopedMicrosecondAccumulator timing(timings
			? &timings->VisualBindingMicroseconds : nullptr);
		hr = contentVisual->SetContent(surface.Get());
		if (SUCCEEDED(hr))
			hr = rootVisual->AddVisual(contentVisual.Get(), FALSE, nullptr);
		if (SUCCEEDED(hr) && !RegisterVisual(rootVisual.Get(), layer, order))
			hr = E_FAIL;
	}
	if (FAILED(hr)) return false;
	*outSurface = surface.Detach();
	*outRootVisual = rootVisual.Detach();
	*outContentVisual = contentVisual.Detach();
	return true;
#else
	if (outSurface) *outSurface = nullptr;
	if (outRootVisual) *outRootVisual = nullptr;
	if (outContentVisual) *outContentVisual = nullptr;
	(void)width;
	(void)height;
	(void)layer;
	(void)order;
	(void)timings;
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
		if (item->layer == layer && item->order == order) return true;
		const int previousLayer = item->layer;
		const int previousOrder = item->order;
		const uint64_t sequence = item->sequence;
        item->layer = layer;
        item->order = order;
		if (_impl->topologyBatchActive)
		{
			_impl->topologyBatchDirty = true;
			++_impl->topologyStatistics.DeferredMutationCount;
			return true;
		}
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
	if (_impl->topologyBatchActive)
	{
		_impl->topologyBatchDirty = true;
		++_impl->topologyStatistics.DeferredMutationCount;
		return true;
	}
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
		if (_impl->topologyBatchActive)
		{
			_impl->topologyBatchDirty = true;
			++_impl->topologyStatistics.DeferredMutationCount;
			return;
		}
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
	{
		if (_impl->topologyBatchActive)
		{
			_impl->topologyBatchDirty = true;
			++_impl->topologyStatistics.DeferredMutationCount;
		}
		else (void)_impl->RebuildVisualStack();
	}
#else
    (void)visual;
#endif
}

bool DCompLayeredHost::BeginVisualTopologyBatch() noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!_impl || !_impl->rootVisual || _impl->topologyBatchActive)
		return false;
	try
	{
		_impl->topologyBatchSnapshot = _impl->layerVisuals;
	}
	catch (...)
	{
		return false;
	}
	_impl->topologyBatchNextSequence = _impl->nextSequence;
	_impl->topologyBatchDirty = false;
	_impl->topologyBatchActive = true;
	return true;
#else
	return false;
#endif
}

bool DCompLayeredHost::CommitVisualTopologyBatch() noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!_impl || !_impl->topologyBatchActive) return false;
	const bool dirty = _impl->topologyBatchDirty;
	_impl->topologyBatchActive = false;
	_impl->topologyBatchDirty = false;
	if (!dirty)
	{
		_impl->topologyBatchSnapshot.clear();
		++_impl->topologyStatistics.BatchCommitCount;
		return true;
	}
	const bool injectedFailure =
		FailNextVisualTopologyBatchCommit.exchange(
			false, std::memory_order_acq_rel);
	const HRESULT result = injectedFailure
		? E_FAIL : _impl->RebuildVisualStack();
	if (SUCCEEDED(result))
	{
		_impl->topologyBatchSnapshot.clear();
		++_impl->topologyStatistics.BatchCommitCount;
		return true;
	}
	_impl->layerVisuals = std::move(_impl->topologyBatchSnapshot);
	_impl->nextSequence = _impl->topologyBatchNextSequence;
	// The injected failure occurs before root mutation. A real DComp failure can
	// leave a partially rebuilt tree, so explicitly restore the accepted stack.
	if (!injectedFailure && FAILED(_impl->RebuildVisualStack()))
	{
		_impl->topologyBatchRestoreFailed = true;
		++_impl->topologyStatistics.BatchRollbackFailureCount;
	}
	++_impl->topologyStatistics.BatchRollbackCount;
	return false;
#else
	return false;
#endif
}

void DCompLayeredHost::RollbackVisualTopologyBatch() noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!_impl || !_impl->topologyBatchActive) return;
	_impl->layerVisuals = std::move(_impl->topologyBatchSnapshot);
	_impl->nextSequence = _impl->topologyBatchNextSequence;
	_impl->topologyBatchActive = false;
	_impl->topologyBatchDirty = false;
	++_impl->topologyStatistics.BatchRollbackCount;
#endif
}

bool DCompLayeredHost::IsVisualTopologyBatchHealthy() const noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	return _impl && !_impl->topologyBatchRestoreFailed;
#else
	return false;
#endif
}

void DCompLayeredHost::FailNextVisualTopologyBatchCommitForTesting() noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	FailNextVisualTopologyBatchCommit.store(true, std::memory_order_release);
#endif
}

void DCompLayeredHost::
ClearVisualTopologyBatchCommitFailureForTesting() noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	FailNextVisualTopologyBatchCommit.store(false, std::memory_order_release);
#endif
}

DCompLayeredHost::VisualTopologyStatistics
DCompLayeredHost::GetVisualTopologyStatistics() const noexcept
{
#ifdef CUI_ENABLE_WEBVIEW2
	return _impl ? _impl->topologyStatistics : VisualTopologyStatistics{};
#else
	return {};
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
