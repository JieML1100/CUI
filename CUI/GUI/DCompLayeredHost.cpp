#include "DCompLayeredHost.h"
/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Graphics/Graphics1.h>
/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
#include <algorithm>
#include <d3d11.h>
#include <dxgi1_2.h>

using Microsoft::WRL::ComPtr;

DCompLayeredHost::DCompLayeredHost(HWND hwnd)
	: _hwnd(hwnd)
{
}

DCompLayeredHost::~DCompLayeredHost()
{
}

HRESULT DCompLayeredHost::Initialize()
{
	if (!_hwnd) return E_INVALIDARG;

	RECT rc{};
	::GetClientRect(_hwnd, &rc);
	UINT width = std::max<UINT>(1, (UINT)(rc.right - rc.left));
	UINT height = std::max<UINT>(1, (UINT)(rc.bottom - rc.top));

	ComPtr<IDXGIDevice> dxgiDevice = Graphics1_GetSharedDXGIDevice();
	if (!dxgiDevice) return E_FAIL;

	HRESULT hr = ::DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&_dcompDevice));
	if (FAILED(hr)) return hr;

	hr = _dcompDevice->CreateTargetForHwnd(_hwnd, TRUE, &_target);
	if (FAILED(hr)) return hr;

	hr = _dcompDevice->CreateVisual(&_rootVisual);
	if (FAILED(hr)) return hr;
	hr = _dcompDevice->CreateVisual(&_baseVisual);
	if (FAILED(hr)) return hr;
	hr = _dcompDevice->CreateVisual(&_webVisual);
	if (FAILED(hr)) return hr;
	hr = _dcompDevice->CreateVisual(&_overlayVisual);
	if (FAILED(hr)) return hr;

	hr = CreateSwapChains(width, height);
	if (FAILED(hr)) return hr;

	hr = _baseVisual->SetContent(_baseSwapChain.Get());
	if (FAILED(hr)) return hr;
	hr = _overlayVisual->SetContent(_overlaySwapChain.Get());
	if (FAILED(hr)) return hr;

	hr = _rootVisual->AddVisual(_baseVisual.Get(), FALSE, nullptr);
	if (FAILED(hr)) return hr;
	hr = _rootVisual->AddVisual(_webVisual.Get(), TRUE, _baseVisual.Get());
	if (FAILED(hr)) return hr;
	hr = _rootVisual->AddVisual(_overlayVisual.Get(), TRUE, _webVisual.Get());
	if (FAILED(hr)) return hr;

	hr = _target->SetRoot(_rootVisual.Get());
	if (FAILED(hr)) return hr;

	return Commit();
}

HRESULT DCompLayeredHost::CreateSwapChains(UINT width, UINT height)
{
	ComPtr<IDXGIDevice> dxgiDevice = Graphics1_GetSharedDXGIDevice();
	if (!dxgiDevice) return E_FAIL;
	ComPtr<IDXGIAdapter> adapter;
	HRESULT hr = dxgiDevice->GetAdapter(&adapter);
	if (FAILED(hr)) return hr;
	ComPtr<IDXGIFactory2> factory;
	hr = adapter->GetParent(IID_PPV_ARGS(&factory));
	if (FAILED(hr)) return hr;

	DXGI_SWAP_CHAIN_DESC1 baseDesc{};
	baseDesc.Width = width;
	baseDesc.Height = height;
	baseDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	baseDesc.SampleDesc.Count = 1;
	baseDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	baseDesc.BufferCount = 2;
	baseDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	baseDesc.Scaling = DXGI_SCALING_STRETCH;
	baseDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	DXGI_SWAP_CHAIN_DESC1 overlayDesc = baseDesc;
	overlayDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

	ComPtr<IDXGISwapChain1> baseSc;
	ComPtr<IDXGISwapChain1> overlaySc;
	hr = factory->CreateSwapChainForComposition(Graphics1_GetSharedD3DDevice(), &baseDesc, nullptr, &baseSc);
	if (FAILED(hr)) return hr;
	hr = factory->CreateSwapChainForComposition(Graphics1_GetSharedD3DDevice(), &overlayDesc, nullptr, &overlaySc);
	if (FAILED(hr)) return hr;

	_baseSwapChain = baseSc;
	_overlaySwapChain = overlaySc;
	return S_OK;
}

HRESULT DCompLayeredHost::Commit()
{
	if (!_dcompDevice) return E_FAIL;
	return _dcompDevice->Commit();
}

