#pragma once
#include <windows.h>
#include <d2d1.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

struct IDCompositionDevice;
struct IDCompositionSurface;
struct IDCompositionVisual;
struct ID2D1Device;

// Deliberately value-only so the DirectComposition stack policy can be tested
// without a desktop compositor or an HWND.  Tokens are opaque visual pointers
// at the host boundary and must never escape into the public control API.
namespace cui::dcomp_detail
{
	/** Converts a DIP-space affine transform to DComp physical pixels. */
	inline D2D1_MATRIX_3X2_F DipTransformToPhysicalPixels(
		D2D1_MATRIX_3X2_F value,
		float dpiScale,
		float titleBarOffsetPixels = 0.0f) noexcept
	{
		if (!std::isfinite(dpiScale) || dpiScale <= 0.0f)
			dpiScale = 1.0f;
		value._31 *= dpiScale;
		value._32 = value._32 * dpiScale + titleBarOffsetPixels;
		return value;
	}

	/** Physical-pixel radii for a DComp rounded rectangle clip. */
	struct RoundedClipRadii final
	{
		float TopLeft = 0.0f;
		float TopRight = 0.0f;
		float BottomRight = 0.0f;
		float BottomLeft = 0.0f;
	};

	/**
	 * Converts authored DIP corner radii and scales overlapping corners as one
	 * shape.  Keeping one common scale factor preserves the original proportions
	 * and avoids relying on compositor-specific overlap handling.
	 */
	inline RoundedClipRadii ResolveRoundedClipRadii(
		float widthPixels,
		float heightPixels,
		float dpiScale,
		float topLeftDip,
		float topRightDip,
		float bottomRightDip,
		float bottomLeftDip) noexcept
	{
		widthPixels = std::isfinite(widthPixels)
			? (std::max)(0.0f, widthPixels) : 0.0f;
		heightPixels = std::isfinite(heightPixels)
			? (std::max)(0.0f, heightPixels) : 0.0f;
		if (!std::isfinite(dpiScale) || dpiScale <= 0.0f)
			dpiScale = 1.0f;
		auto radius = [dpiScale](float value)
		{
			return std::isfinite(value)
				? (std::max)(0.0f, value) * dpiScale : 0.0f;
		};
		RoundedClipRadii result{
			radius(topLeftDip), radius(topRightDip),
			radius(bottomRightDip), radius(bottomLeftDip) };
		float scale = 1.0f;
		auto constrain = [&scale](float available, float requested)
		{
			if (requested > available && requested > 0.0f)
				scale = (std::min)(scale, available / requested);
		};
		constrain(widthPixels, result.TopLeft + result.TopRight);
		constrain(widthPixels, result.BottomLeft + result.BottomRight);
		constrain(heightPixels, result.TopLeft + result.BottomLeft);
		constrain(heightPixels, result.TopRight + result.BottomRight);
		result.TopLeft *= scale;
		result.TopRight *= scale;
		result.BottomRight *= scale;
		result.BottomLeft *= scale;
		return result;
	}

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
	struct VisualTopologyStatistics final
	{
		uint64_t StackRebuildCount = 0;
		uint64_t StackRebuildEntryCount = 0;
		uint64_t DeferredMutationCount = 0;
		uint64_t BatchCommitCount = 0;
		uint64_t BatchRollbackCount = 0;
		uint64_t BatchRollbackFailureCount = 0;
	};

	struct D2DLayerCreationTimings final
	{
		double SwapChainMicroseconds = 0.0;
		double SurfaceMicroseconds = 0.0;
		double VisualCreationMicroseconds = 0.0;
		double VisualBindingMicroseconds = 0.0;
	};

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
	ID2D1Device* GetD2DDevice() const;
	/** True when BeginDraw can return a pre-targeted ID2D1DeviceContext. */
	bool SupportsD2DSurfaceDeviceContexts() const noexcept;
    IDCompositionVisual* GetRootVisual() const;
    IDCompositionVisual* GetWebContainerVisual() const;
    bool CreateD2DLayer(
        void** outSwapChain,
        IDCompositionVisual** outRootVisual,
        IDCompositionVisual** outContentVisual,
        UINT width,
        UINT height,
        int layer,
        int order,
		D2DLayerCreationTimings* timings = nullptr);
	bool CreateD2DSurfaceLayer(
		IDCompositionSurface** outSurface,
		IDCompositionVisual** outRootVisual,
		IDCompositionVisual** outContentVisual,
		UINT width,
		UINT height,
		int layer,
		int order,
		D2DLayerCreationTimings* timings = nullptr);
    void DestroyD2DLayer(IDCompositionVisual* visual);
    bool RegisterVisual(IDCompositionVisual* visual, int layer, int order);
    void UpdateVisualOrder(IDCompositionVisual* visual, int layer, int order);
    void UnregisterVisual(IDCompositionVisual* visual);
	bool BeginVisualTopologyBatch() noexcept;
	bool CommitVisualTopologyBatch() noexcept;
	void RollbackVisualTopologyBatch() noexcept;
	bool IsVisualTopologyBatchHealthy() const noexcept;
	static void FailNextVisualTopologyBatchCommitForTesting() noexcept;
	static void ClearVisualTopologyBatchCommitFailureForTesting() noexcept;
	VisualTopologyStatistics GetVisualTopologyStatistics() const noexcept;
    void* GetSwapChain() const; // 实际类型为 IDXGISwapChain1*
    void* GetOverlaySwapChain() const; // 实际类型为 IDXGISwapChain1*

    HRESULT CommitComposition();
    bool IsInitialized() const;
    bool IsDeviceLost() const noexcept;

private:
    class Impl;
    Impl* _impl;
};
