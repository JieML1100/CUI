#pragma once
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

struct IDCompositionDevice;
struct IDCompositionVisual;

// Deliberately value-only so the DirectComposition stack policy can be tested
// without a desktop compositor or an HWND.  Tokens are opaque visual pointers
// at the host boundary and must never escape into the public control API.
namespace cui::dcomp_detail
{
	struct VisualStackEntry final
	{
		uintptr_t Token = 0;
		int Layer = 0;
		int Order = 0;
		uint64_t Sequence = 0;
	};

	struct VisualStackInsertion final
	{
		uintptr_t Token = 0;
		bool InsertAbove = false;
		uintptr_t ReferenceToken = 0;
	};

	inline std::vector<VisualStackInsertion> BuildVisualStackInsertionPlan(
		std::span<const VisualStackEntry> source)
	{
		std::vector<VisualStackEntry> entries(source.begin(), source.end());
		std::stable_sort(entries.begin(), entries.end(),
			[](const VisualStackEntry& left, const VisualStackEntry& right)
			{
				if (left.Layer != right.Layer)
					return left.Layer < right.Layer;
				if (left.Order != right.Order)
					return left.Order < right.Order;
				return left.Sequence < right.Sequence;
			});

		std::vector<VisualStackInsertion> result;
		result.reserve(entries.size());
		uintptr_t previous = 0;
		for (const auto& entry : entries)
		{
			if (entry.Token == 0) continue;
			result.push_back(VisualStackInsertion{
				entry.Token, previous != 0, previous });
			previous = entry.Token;
		}
		return result;
	}
}

/**
 * @file DCompLayeredHost.h
 * @brief Window 的 DirectComposition 宿主，用于承载 WebView2 Composition 模式。
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

    HRESULT CommitComposition();
    bool IsInitialized() const;

private:
    class Impl;
    Impl* _impl;
};
