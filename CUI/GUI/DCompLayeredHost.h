#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <windows.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Graphics/Factory.h>
/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
// (using external CppUtils)
class DCompLayeredHost
{
public:
	explicit DCompLayeredHost(HWND hwnd);
	~DCompLayeredHost();

	HRESULT Initialize();

	IDXGISwapChain1* GetBaseSwapChain() const { return _baseSwapChain.Get(); }
	IDXGISwapChain1* GetOverlaySwapChain() const { return _overlaySwapChain.Get(); }

	IDCompositionDevice* GetDCompDevice() const { return _dcompDevice.Get(); }
	IDCompositionVisual* GetWebContainerVisual() const { return _webVisual.Get(); }

	HRESULT Commit();

private:
	HRESULT CreateSwapChains(UINT width, UINT height);

private:
	HWND _hwnd = nullptr;

	Microsoft::WRL::ComPtr<IDCompositionDevice> _dcompDevice;
	Microsoft::WRL::ComPtr<IDCompositionTarget> _target;

	Microsoft::WRL::ComPtr<IDCompositionVisual> _rootVisual;
	Microsoft::WRL::ComPtr<IDCompositionVisual> _baseVisual;
	Microsoft::WRL::ComPtr<IDCompositionVisual> _webVisual;
	Microsoft::WRL::ComPtr<IDCompositionVisual> _overlayVisual;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> _baseSwapChain;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> _overlaySwapChain;
};

