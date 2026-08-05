#pragma once

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class D2DGraphics;
class DCompLayeredHost;
struct ID2D1CommandList;
struct IDCompositionDevice;
struct IDCompositionVisual;

/**
 * Owns every device-dependent resource for one native presentation source.
 *
 * A frame is an explicit transaction. Primary, retained scene and overlay
 * surfaces may only be opened through that transaction, and DirectComposition
 * is committed only after every surface closes successfully. Any EndDraw,
 * Present, DComp, exception or injected-device failure aborts the whole frame,
 * advances the device-resource generation during recovery and forces a full
 * retained resubmission.
 */
class PresentationRenderHost final
{
public:
	enum class SurfaceRole : uint8_t
	{
		Primary,
		Scene,
		Overlay
	};

	struct SurfaceFrame
	{
		D2DGraphics* Context = nullptr;
		SurfaceRole Role = SurfaceRole::Primary;
		RECT LogicalDirty{};
		bool Open = false;
	};

	struct FrameTransaction
	{
		uint64_t Sequence = 0;
		uint64_t ResourceGeneration = 0;
		RECT LogicalClient{};
		RECT LogicalDirty{};
		float DpiScale = 1.0f;
		bool FullFrame = false;
		bool Open = false;
		bool Failed = false;
		SurfaceFrame Primary;
	};

	struct TransactionStatistics
	{
		uint64_t LastSequence = 0;
		uint64_t ResourceGeneration = 0;
		uint64_t CommittedFrames = 0;
		uint64_t AbortedFrames = 0;
		uint64_t DeviceRecoveries = 0;
		uint64_t InjectedDeviceLosses = 0;
		uint64_t LastSurfaceFailureSequence = 0;
		SurfaceRole LastFailedSurfaceRole = SurfaceRole::Primary;
		HRESULT LastFailedEndDrawHr = S_OK;
		HRESULT LastFailedPresentHr = S_OK;
	};

	PresentationRenderHost();
	~PresentationRenderHost();
	PresentationRenderHost(const PresentationRenderHost&) = delete;
	PresentationRenderHost& operator=(const PresentationRenderHost&) = delete;
	PresentationRenderHost(PresentationRenderHost&&) = delete;
	PresentationRenderHost& operator=(PresentationRenderHost&&) = delete;

	bool Attach(HWND window, UINT dpi = 96);
	void Detach() noexcept;
	bool IsAttached() const noexcept;
	/** One-shot test seam for the real transient initial-attach recovery path. */
	static void FailNextPrimaryAttachForTesting() noexcept;

	D2DGraphics* DrawingContext() const noexcept { return _active; }
	D2DGraphics* PrimaryContext() const noexcept { return _primary.get(); }
	D2DGraphics* OverlayContext() const noexcept { return _overlay.get(); }
	bool Activate(D2DGraphics* context) noexcept;

	void Resize(UINT width, UINT height);
	void ResizeToClient();
	void SetDpi(UINT dpi);
	UINT Dpi() const noexcept { return _dpi; }

	bool BeginFrameTransaction(
		const RECT& physicalDirty,
		bool force,
		FrameTransaction& transaction);
	bool OpenSceneSurface(
		FrameTransaction& transaction,
		D2DGraphics* context,
		const RECT& logicalDirty,
		SurfaceFrame& surface);
	bool OpenOverlaySurface(
		FrameTransaction& transaction,
		SurfaceFrame& surface);
	bool CloseSurface(
		FrameTransaction& transaction,
		SurfaceFrame& surface) noexcept;
	bool RecordDrawingCommands(
		FrameTransaction& transaction,
		D2DGraphics* presentationContext,
		const std::function<void()>& draw,
		ID2D1CommandList** commandList) noexcept;
	bool ReplayDrawingCommands(
		FrameTransaction& transaction,
		D2DGraphics* presentationContext,
		ID2D1CommandList* commandList) noexcept;
	bool CommitFrameTransaction(FrameTransaction& transaction) noexcept;
	void AbortFrameTransaction(FrameTransaction& transaction) noexcept;
	bool IsTransactionActive(const FrameTransaction& transaction) const noexcept;

	/**
	 * Coalesces physical-client damage and schedules the matching HWND update
	 * region. The retained queue owns what to repaint; WM_PAINT owns when.
	 */
	void QueueDamage(const RECT& physicalDirty) noexcept;
	/** Promotes all pending damage to the complete physical client. */
	void QueueFullDamage() noexcept;
	/** Atomically transfers the currently coalesced physical-client damage. */
	bool TakePendingDamage(RECT& physicalDirty) noexcept;
	bool HasPendingDamage() const noexcept { return _hasPendingDamage; }
	bool TryGetLastPrimaryFrame(
		RECT& logicalDirty,
		bool& fullFrame) const noexcept;
	bool NeedsFullFrame(bool force = false) const noexcept
	{
		return force || !_hasPresentedFrame;
	}
	void InvalidateFrameHistory() noexcept;

	bool IsDeviceLost() const noexcept;
	bool RecoverDevice();
	/** Test seam: recovery follows the exact production generation path. */
	void InjectDeviceLossForTesting() noexcept;
	uint64_t ResourceGeneration() const noexcept
	{
		return _resourceGeneration;
	}
	TransactionStatistics Statistics() const noexcept;

	bool EnsureComposition();
	bool UsesComposition() const noexcept;
	D2DGraphics* AcquireSceneLayer(size_t index, int layer, int order);
	void TrimSceneLayers(size_t usedCount) noexcept;
	void ReleaseSceneLayers() noexcept;

	IDCompositionDevice* CompositionDevice() const noexcept;
	IDCompositionVisual* WebContainerVisual() const noexcept;
	bool RegisterCompositionVisual(IDCompositionVisual* visual, int layer, int order);
	void UpdateCompositionVisualOrder(IDCompositionVisual* visual, int layer, int order);
	void UnregisterCompositionVisual(IDCompositionVisual* visual);
	bool CommitComposition() noexcept;

private:
	struct SceneLayer
	{
		IDCompositionVisual* Visual = nullptr;
		std::unique_ptr<D2DGraphics> Graphics;
		std::unique_ptr<D2DGraphics> Recorder;
	};

	struct OpenContext
	{
		D2DGraphics* Context = nullptr;
		SurfaceRole Role = SurfaceRole::Primary;
		bool CommandRecording = false;
		bool ClipPushed = false;
	};

	HWND _window = nullptr;
	UINT _width = 1;
	UINT _height = 1;
	UINT _dpi = 96;
	std::unique_ptr<D2DGraphics> _primary;
	std::unique_ptr<D2DGraphics> _overlay;
	std::unique_ptr<DCompLayeredHost> _composition;
	std::vector<SceneLayer> _sceneLayers;
	std::vector<OpenContext> _openContexts;
	D2DGraphics* _active = nullptr;
	bool _recoveringDevice = false;
	bool _deviceResetRequested = false;
	bool _hasPresentedFrame = false;
	bool _transactionOpen = false;
	uint64_t _activeTransactionSequence = 0;
	uint64_t _nextFrameSequence = 0;
	uint64_t _resourceGeneration = 0;
	uint64_t _sharedDeviceGeneration = 0;
	uint64_t _committedFrames = 0;
	uint64_t _abortedFrames = 0;
	uint64_t _deviceRecoveries = 0;
	uint64_t _injectedDeviceLosses = 0;
	uint64_t _lastSurfaceFailureSequence = 0;
	SurfaceRole _lastFailedSurfaceRole = SurfaceRole::Primary;
	HRESULT _lastFailedEndDrawHr = S_OK;
	HRESULT _lastFailedPresentHr = S_OK;
	RECT _pendingDamage{};
	bool _hasPendingDamage = false;
	bool _fullDamagePending = false;
	RECT _lastPrimaryDirty{};
	bool _lastPrimaryWasFull = false;
	bool _hasLastPrimaryFrame = false;
	static std::atomic<bool> _failNextPrimaryAttachForTesting;

	bool OwnsContext(const D2DGraphics* context) const noexcept;
	RECT ToLogicalRect(const RECT& physical) const noexcept;
	bool OpenSurface(
		FrameTransaction& transaction,
		D2DGraphics* context,
		SurfaceRole role,
		const RECT& logicalDirty,
		bool clearTransparent,
		SurfaceFrame& surface);
	bool ValidateClosedContext(D2DGraphics& context) noexcept;
	void MarkTransactionFailed(FrameTransaction& transaction) noexcept;
	void RemoveOpenContext(D2DGraphics* context, bool commandRecording) noexcept;
	D2DGraphics* FindCommandRecorder(D2DGraphics* presentationContext) noexcept;
	bool CreateCompositionResources();
	void AdvanceResourceGeneration() noexcept;
};
