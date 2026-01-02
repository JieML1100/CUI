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

#if defined(_MSC_VER)
#pragma comment(lib, "dcomp.lib")
#endif

/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Graphics/Factory.h>
/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
// (using external CppUtils)

/**
 * @file DCompLayeredHost.h
 * @brief DirectComposition 分层宿主：为窗口提供 base/web/overlay 三层视觉树。
 *
 * 该类用于在一个 HWND 上构建 DirectComposition 视觉树，并创建两套 SwapChain：
 * - BaseSwapChain：用于主渲染（通常是 D2D/应用 UI）
 * - OverlaySwapChain：用于叠加层（例如浮层/覆盖渲染）
 *
 * 同时提供一个 Web 容器 Visual（GetWebContainerVisual），用于挂载 WebView2 的 RootVisualTarget。
 */
class DCompLayeredHost
{
public:
	/** @param hwnd 目标窗口句柄（客户区将作为 DComp 输出）。 */
	explicit DCompLayeredHost(HWND hwnd);
	~DCompLayeredHost();

	/**
	 * @brief 初始化 DirectComposition 设备、视觉树与 swapchains。
	 * @return HRESULT。
	 */
	HRESULT Initialize();

	IDXGISwapChain1* GetBaseSwapChain() const { return _baseSwapChain.Get(); }
	IDXGISwapChain1* GetOverlaySwapChain() const { return _overlaySwapChain.Get(); }

	/** @brief 获取 DirectComposition 设备（不转移所有权）。 */
	IDCompositionDevice* GetDCompDevice() const { return _dcompDevice.Get(); }
	/** @brief 获取 Web 容器 Visual（不转移所有权）。 */
	IDCompositionVisual* GetWebContainerVisual() const { return _webVisual.Get(); }

	/** @brief 提交 DComp 变更（Commit）。 */
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

