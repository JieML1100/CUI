#include "PresentationRenderHost.h"

#include "DCompLayeredHost.h"
#include "Graphics.h"

#include <algorithm>
#include <cmath>
#include <d2derr.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

PresentationRenderHost::PresentationRenderHost() = default;

PresentationRenderHost::~PresentationRenderHost()
{
	Detach();
}

bool PresentationRenderHost::Attach(HWND window, UINT dpi)
{
	Detach();
	if (!window || !::IsWindow(window)) return false;

	_window = window;
	_dpi = dpi == 0 ? 96 : dpi;
	RECT client{};
	::GetClientRect(_window, &client);
	_width = static_cast<UINT>((std::max)(
		LONG{ 1 }, client.right - client.left));
	_height = static_cast<UINT>((std::max)(
		LONG{ 1 }, client.bottom - client.top));
	_primary = std::make_unique<HwndGraphics>(_window);
	if (!_primary || !_primary->GetDeviceContextRaw())
	{
		_primary.reset();
		_window = nullptr;
		return false;
	}
	_primary->SetDpi(static_cast<FLOAT>(_dpi), static_cast<FLOAT>(_dpi));
	// The device context is owned for the host lifetime, but it is exposed to
	// native render hooks only while a frame surface is open.
	_active = nullptr;
	AdvanceResourceGeneration();
	InvalidateFrameHistory();
	return true;
}

void PresentationRenderHost::Detach() noexcept
{
	for (auto iterator = _openContexts.rbegin();
		iterator != _openContexts.rend(); ++iterator)
	{
		if (!iterator->Context) continue;
		if (iterator->CommandRecording)
		{
			iterator->Context->AbortCommandRecording();
			continue;
		}
		iterator->Context->ClearTransform();
		if (iterator->ClipPushed) iterator->Context->PopDrawRect();
		iterator->Context->EndRender();
	}
	_openContexts.clear();
	_active = nullptr;
	_overlay.reset();
	ReleaseSceneLayers();
	_primary.reset();
	_composition.reset();
	_window = nullptr;
	_width = 1;
	_height = 1;
	_dpi = 96;
	_recoveringDevice = false;
	_deviceResetRequested = false;
	_hasPresentedFrame = false;
	_transactionOpen = false;
	_activeTransactionSequence = 0;
	_pendingDamage = {};
	_hasPendingDamage = false;
	_fullDamagePending = false;
	_lastPrimaryDirty = {};
	_lastPrimaryWasFull = false;
	_hasLastPrimaryFrame = false;
	_lastSurfaceFailureSequence = 0;
	_lastFailedSurfaceRole = SurfaceRole::Primary;
	_lastFailedEndDrawHr = S_OK;
	_lastFailedPresentHr = S_OK;
}

bool PresentationRenderHost::IsAttached() const noexcept
{
	return _window && ::IsWindow(_window) && _primary
		&& _primary->GetDeviceContextRaw();
}

bool PresentationRenderHost::OwnsContext(
	const D2DGraphics* context) const noexcept
{
	if (!context) return false;
	if (context == _primary.get() || context == _overlay.get()) return true;
	for (const auto& layer : _sceneLayers)
	{
		if (context == layer.Graphics.get()
			|| context == layer.Recorder.get()) return true;
	}
	return false;
}

bool PresentationRenderHost::Activate(D2DGraphics* context) noexcept
{
	if (!context || !OwnsContext(context)) return false;
	_active = context;
	return true;
}

void PresentationRenderHost::Resize(UINT width, UINT height)
{
	width = (std::max)(UINT{ 1 }, width);
	height = (std::max)(UINT{ 1 }, height);
	if (width == _width && height == _height) return;
	_width = width;
	_height = height;
	if (_transactionOpen)
	{
		_deviceResetRequested = true;
		InvalidateFrameHistory();
		return;
	}
	if (_composition) _composition->UpdateD2DLayerSize(_width, _height);
	if (_primary) _primary->ReSize(_width, _height);
	if (_overlay) _overlay->ReSize(_width, _height);
	for (auto& layer : _sceneLayers)
	{
		if (layer.Graphics) layer.Graphics->ReSize(_width, _height);
	}
	SetDpi(_dpi);
	AdvanceResourceGeneration();
	if (IsDeviceLost()) _deviceResetRequested = true;
	InvalidateFrameHistory();
}

void PresentationRenderHost::ResizeToClient()
{
	if (!_window || !::IsWindow(_window)) return;
	RECT client{};
	::GetClientRect(_window, &client);
	Resize(
		static_cast<UINT>((std::max)(
			LONG{ 1 }, client.right - client.left)),
		static_cast<UINT>((std::max)(
			LONG{ 1 }, client.bottom - client.top)));
}

void PresentationRenderHost::SetDpi(UINT dpi)
{
	const UINT value = dpi == 0 ? 96 : dpi;
	const bool changed = value != _dpi;
	_dpi = value;
	const auto nativeDpi = static_cast<FLOAT>(_dpi);
	if (_primary) _primary->SetDpi(nativeDpi, nativeDpi);
	if (_overlay) _overlay->SetDpi(nativeDpi, nativeDpi);
	for (auto& layer : _sceneLayers)
	{
		if (layer.Graphics) layer.Graphics->SetDpi(nativeDpi, nativeDpi);
		if (layer.Recorder) layer.Recorder->SetDpi(nativeDpi, nativeDpi);
	}
	if (changed)
	{
		AdvanceResourceGeneration();
		InvalidateFrameHistory();
	}
}

RECT PresentationRenderHost::ToLogicalRect(
	const RECT& physical) const noexcept
{
	const float scale = _dpi > 0 ? (_dpi / 96.0f) : 1.0f;
	return RECT{
		static_cast<LONG>(std::floor(physical.left / scale)),
		static_cast<LONG>(std::floor(physical.top / scale)),
		static_cast<LONG>(std::ceil(physical.right / scale)),
		static_cast<LONG>(std::ceil(physical.bottom / scale)) };
}

bool PresentationRenderHost::IsTransactionActive(
	const FrameTransaction& transaction) const noexcept
{
	return _transactionOpen && transaction.Open
		&& transaction.Sequence != 0
		&& transaction.Sequence == _activeTransactionSequence;
}

bool PresentationRenderHost::OpenSurface(
	FrameTransaction& transaction,
	D2DGraphics* context,
	SurfaceRole role,
	const RECT& logicalDirty,
	bool clearTransparent,
	SurfaceFrame& surface)
{
	surface = {};
	if (!IsTransactionActive(transaction) || transaction.Failed
		|| !context || !context->GetDeviceContextRaw()
		|| logicalDirty.right <= logicalDirty.left
		|| logicalDirty.bottom <= logicalDirty.top) return false;
	for (const auto& open : _openContexts)
		if (open.Context == context) return false;
	if (!Activate(context)) return false;

	const RECT effectiveDirty = context->RequiresFullPresentFrame()
		? transaction.LogicalClient : logicalDirty;
	context->SetPresentDirtyRect(effectiveDirty);
	context->BeginRender();
	if (FAILED(context->GetLastEndDrawHr()))
	{
		MarkTransactionFailed(transaction);
		return false;
	}
	context->ClearTransform();
	context->PushDrawRect(
		static_cast<float>(effectiveDirty.left),
		static_cast<float>(effectiveDirty.top),
		static_cast<float>(effectiveDirty.right - effectiveDirty.left),
		static_cast<float>(effectiveDirty.bottom - effectiveDirty.top));
	if (clearTransparent)
		context->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
	_openContexts.push_back(OpenContext{
		context, role, false, true });
	surface.Context = context;
	surface.Role = role;
	surface.LogicalDirty = effectiveDirty;
	surface.Open = true;
	return true;
}

bool PresentationRenderHost::BeginFrameTransaction(
	const RECT& physicalDirty,
	bool force,
	FrameTransaction& transaction)
{
	transaction = {};
	if (_transactionOpen || !IsAttached() || IsDeviceLost()
		|| physicalDirty.right <= physicalDirty.left
		|| physicalDirty.bottom <= physicalDirty.top) return false;

	RECT physicalClient{};
	::GetClientRect(_window, &physicalClient);
	if (++_nextFrameSequence == 0) ++_nextFrameSequence;
	transaction.Sequence = _nextFrameSequence;
	transaction.ResourceGeneration = _resourceGeneration;
	transaction.LogicalClient = ToLogicalRect(physicalClient);
	transaction.FullFrame = NeedsFullFrame(force);
	transaction.LogicalDirty = transaction.FullFrame
		? transaction.LogicalClient : ToLogicalRect(physicalDirty);
	if (!transaction.FullFrame)
	{
		RECT clipped{};
		if (!::IntersectRect(&clipped, &transaction.LogicalDirty,
			&transaction.LogicalClient)) return false;
		transaction.LogicalDirty = clipped;
	}
	transaction.DpiScale = _dpi > 0 ? (_dpi / 96.0f) : 1.0f;
	transaction.Open = true;
	_transactionOpen = true;
	_activeTransactionSequence = transaction.Sequence;
	if (!OpenSurface(transaction, _primary.get(), SurfaceRole::Primary,
		transaction.LogicalDirty, false, transaction.Primary))
	{
		AbortFrameTransaction(transaction);
		return false;
	}
	return true;
}

bool PresentationRenderHost::OpenSceneSurface(
	FrameTransaction& transaction,
	D2DGraphics* context,
	const RECT& logicalDirty,
	SurfaceFrame& surface)
{
	return OpenSurface(transaction, context, SurfaceRole::Scene,
		logicalDirty, true, surface);
}

bool PresentationRenderHost::OpenOverlaySurface(
	FrameTransaction& transaction,
	SurfaceFrame& surface)
{
	if (!_overlay) return false;
	return OpenSurface(transaction, _overlay.get(), SurfaceRole::Overlay,
		transaction.LogicalClient, true, surface);
}

bool PresentationRenderHost::ValidateClosedContext(
	D2DGraphics& context) noexcept
{
	return SUCCEEDED(context.GetLastEndDrawHr())
		&& SUCCEEDED(context.GetLastPresentHr())
		&& !context.IsDeviceLost();
}

void PresentationRenderHost::RemoveOpenContext(
	D2DGraphics* context,
	bool commandRecording) noexcept
{
	const auto found = std::find_if(_openContexts.rbegin(),
		_openContexts.rend(), [=](const OpenContext& open)
		{
			return open.Context == context
				&& open.CommandRecording == commandRecording;
		});
	if (found == _openContexts.rend()) return;
	_openContexts.erase(std::next(found).base());
}

void PresentationRenderHost::MarkTransactionFailed(
	FrameTransaction& transaction) noexcept
{
	transaction.Failed = true;
	_deviceResetRequested = true;
	InvalidateFrameHistory();
}

bool PresentationRenderHost::CloseSurface(
	FrameTransaction& transaction,
	SurfaceFrame& surface) noexcept
{
	if (!surface.Open || !surface.Context
		|| !IsTransactionActive(transaction)) return false;
	auto* context = surface.Context;
	context->ClearTransform();
	context->PopDrawRect();
	context->EndRender();
	RemoveOpenContext(context, false);
	surface.Open = false;
	const bool succeeded = ValidateClosedContext(*context);
	if (!succeeded)
	{
		_lastSurfaceFailureSequence = transaction.Sequence;
		_lastFailedSurfaceRole = surface.Role;
		_lastFailedEndDrawHr = context->GetLastEndDrawHr();
		_lastFailedPresentHr = context->GetLastPresentHr();
		MarkTransactionFailed(transaction);
	}
	_active = _openContexts.empty()
		? nullptr : _openContexts.back().Context;
	return succeeded;
}

D2DGraphics* PresentationRenderHost::FindCommandRecorder(
	D2DGraphics* presentationContext) noexcept
{
	for (auto& layer : _sceneLayers)
		if (layer.Graphics.get() == presentationContext)
			return layer.Recorder.get();
	return nullptr;
}

bool PresentationRenderHost::RecordDrawingCommands(
	FrameTransaction& transaction,
	D2DGraphics* presentationContext,
	const std::function<void()>& draw,
	ID2D1CommandList** commandList) noexcept
{
	if (commandList) *commandList = nullptr;
	if (!commandList || !draw || !IsTransactionActive(transaction)
		|| transaction.Failed) return false;
	auto* recorder = FindCommandRecorder(presentationContext);
	if (!recorder || !recorder->BeginCommandRecording())
	{
		MarkTransactionFailed(transaction);
		return false;
	}
	auto* previous = _active;
	_openContexts.push_back(OpenContext{
		recorder, SurfaceRole::Scene, true, false });
	(void)Activate(recorder);
	try
	{
		draw();
	}
	catch (...)
	{
		recorder->AbortCommandRecording();
		RemoveOpenContext(recorder, true);
		(void)Activate(previous);
		MarkTransactionFailed(transaction);
		return false;
	}
	const HRESULT result = recorder->EndCommandRecording(commandList);
	RemoveOpenContext(recorder, true);
	(void)Activate(previous);
	if (FAILED(result) || !*commandList)
	{
		MarkTransactionFailed(transaction);
		return false;
	}
	return true;
}

bool PresentationRenderHost::ReplayDrawingCommands(
	FrameTransaction& transaction,
	D2DGraphics* presentationContext,
	ID2D1CommandList* commandList) noexcept
{
	if (!IsTransactionActive(transaction) || transaction.Failed
		|| !presentationContext || !commandList) return false;
	presentationContext->DrawCommandList(commandList);
	return true;
}

bool PresentationRenderHost::CommitFrameTransaction(
	FrameTransaction& transaction) noexcept
{
	if (!IsTransactionActive(transaction)) return false;
	if (transaction.Primary.Open)
		(void)CloseSurface(transaction, transaction.Primary);
	if (!_openContexts.empty()) MarkTransactionFailed(transaction);
	if (!transaction.Failed && !IsDeviceLost() && _composition)
	{
		const HRESULT result = _composition->CommitComposition();
		if (FAILED(result)) MarkTransactionFailed(transaction);
	}
	if (transaction.Failed || IsDeviceLost())
	{
		AbortFrameTransaction(transaction);
		return false;
	}

	_lastPrimaryDirty = transaction.LogicalDirty;
	_lastPrimaryWasFull = transaction.FullFrame;
	_hasLastPrimaryFrame = true;
	_hasPresentedFrame = true;
	++_committedFrames;
	transaction.Open = false;
	_transactionOpen = false;
	_activeTransactionSequence = 0;
	_active = nullptr;
	return true;
}

void PresentationRenderHost::AbortFrameTransaction(
	FrameTransaction& transaction) noexcept
{
	if (!IsTransactionActive(transaction))
	{
		transaction.Open = false;
		transaction.Failed = true;
		return;
	}
	for (auto iterator = _openContexts.rbegin();
		iterator != _openContexts.rend(); ++iterator)
	{
		auto* context = iterator->Context;
		if (!context) continue;
		if (iterator->CommandRecording)
		{
			context->AbortCommandRecording();
			continue;
		}
		context->ClearTransform();
		if (iterator->ClipPushed) context->PopDrawRect();
		context->EndRender();
	}
	_openContexts.clear();
	transaction.Primary.Open = false;
	transaction.Open = false;
	transaction.Failed = true;
	_transactionOpen = false;
	_activeTransactionSequence = 0;
	_deviceResetRequested = true;
	++_abortedFrames;
	InvalidateFrameHistory();
	_active = nullptr;
}

void PresentationRenderHost::QueueDamage(
	const RECT& physicalDirty) noexcept
{
	if (physicalDirty.right <= physicalDirty.left
		|| physicalDirty.bottom <= physicalDirty.top) return;
	RECT client{ 0, 0, static_cast<LONG>(_width),
		static_cast<LONG>(_height) };
	RECT clipped{};
	if (!::IntersectRect(&clipped, &physicalDirty, &client)) return;
	if (_fullDamagePending) return;
	if (!_hasPendingDamage)
	{
		_pendingDamage = clipped;
		_hasPendingDamage = true;
		if (_window && ::IsWindow(_window))
			::InvalidateRect(_window, &clipped, FALSE);
		return;
	}
	RECT combined{};
	::UnionRect(&combined, &_pendingDamage, &clipped);
	_pendingDamage = combined;
	if (_window && ::IsWindow(_window))
		::InvalidateRect(_window, &clipped, FALSE);
}

void PresentationRenderHost::QueueFullDamage() noexcept
{
	_pendingDamage = RECT{
		0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };
	_hasPendingDamage = true;
	_fullDamagePending = true;
	// The retained damage queue and the HWND update region are one scheduling
	// contract. Internal scene/device invalidations must never leave work queued
	// until an unrelated input or animation happens to produce WM_PAINT.
	if (_window && ::IsWindow(_window))
		::InvalidateRect(_window, nullptr, FALSE);
}

bool PresentationRenderHost::TakePendingDamage(
	RECT& physicalDirty) noexcept
{
	physicalDirty = {};
	if (!_hasPendingDamage) return false;
	physicalDirty = _fullDamagePending
		? RECT{ 0, 0, static_cast<LONG>(_width),
			static_cast<LONG>(_height) }
		: _pendingDamage;
	_pendingDamage = {};
	_hasPendingDamage = false;
	_fullDamagePending = false;
	return physicalDirty.right > physicalDirty.left
		&& physicalDirty.bottom > physicalDirty.top;
}

bool PresentationRenderHost::TryGetLastPrimaryFrame(
	RECT& logicalDirty,
	bool& fullFrame) const noexcept
{
	logicalDirty = _lastPrimaryDirty;
	fullFrame = _lastPrimaryWasFull;
	return _hasLastPrimaryFrame;
}

void PresentationRenderHost::InvalidateFrameHistory() noexcept
{
	_hasPresentedFrame = false;
	_hasLastPrimaryFrame = false;
	QueueFullDamage();
}

bool PresentationRenderHost::IsDeviceLost() const noexcept
{
	if (_deviceResetRequested) return true;
	if (_primary && _primary->IsDeviceLost()) return true;
	if (_overlay && _overlay->IsDeviceLost()) return true;
	for (const auto& layer : _sceneLayers)
	{
		if ((layer.Graphics && layer.Graphics->IsDeviceLost())
			|| (layer.Recorder && layer.Recorder->IsDeviceLost())) return true;
	}
	return false;
}

bool PresentationRenderHost::RecoverDevice()
{
	if (_recoveringDevice || _transactionOpen || !_window
		|| !::IsWindow(_window) || !IsDeviceLost()) return false;
	_recoveringDevice = true;
	const bool restoreComposition = UsesComposition();

	_active = nullptr;
	_overlay.reset();
	ReleaseSceneLayers();
	_primary.reset();
	_composition.reset();
	_primary = std::make_unique<HwndGraphics>(_window);
	bool restored = _primary && _primary->GetDeviceContextRaw();
	if (restored)
	{
		_primary->SetDpi(static_cast<FLOAT>(_dpi), static_cast<FLOAT>(_dpi));
		_active = nullptr;
		if (restoreComposition) restored = CreateCompositionResources();
	}
	if (restored)
	{
		_deviceResetRequested = false;
		AdvanceResourceGeneration();
		++_deviceRecoveries;
		InvalidateFrameHistory();
	}
	else
	{
		_deviceResetRequested = true;
	}
	_recoveringDevice = false;
	return restored;
}

void PresentationRenderHost::InjectDeviceLossForTesting() noexcept
{
	++_injectedDeviceLosses;
	_deviceResetRequested = true;
	InvalidateFrameHistory();
}

PresentationRenderHost::TransactionStatistics
PresentationRenderHost::Statistics() const noexcept
{
	return TransactionStatistics{
		_nextFrameSequence,
		_resourceGeneration,
		_committedFrames,
		_abortedFrames,
		_deviceRecoveries,
		_injectedDeviceLosses,
		_lastSurfaceFailureSequence,
		_lastFailedSurfaceRole,
		_lastFailedEndDrawHr,
		_lastFailedPresentHr };
}

bool PresentationRenderHost::CreateCompositionResources()
{
#ifdef CUI_ENABLE_WEBVIEW2
	auto composition = std::make_unique<DCompLayeredHost>();
	if (!composition->Initialize(_window, _width, _height)) return false;
	auto* swapChain = static_cast<IDXGISwapChain1*>(
		composition->GetSwapChain());
	if (!swapChain) return false;

	auto primary = std::make_unique<CompositionSwapChainGraphics>(swapChain);
	if (!primary->GetDeviceContextRaw()) return false;
	std::unique_ptr<D2DGraphics> overlay;
	if (auto* overlaySwapChain = static_cast<IDXGISwapChain1*>(
		composition->GetOverlaySwapChain()))
	{
		overlay = std::make_unique<CompositionSwapChainGraphics>(
			overlaySwapChain);
		if (!overlay->GetDeviceContextRaw()) return false;
	}

	const auto dpi = static_cast<FLOAT>(_dpi);
	primary->SetDpi(dpi, dpi);
	if (overlay) overlay->SetDpi(dpi, dpi);
	_overlay = std::move(overlay);
	_primary = std::move(primary);
	_composition = std::move(composition);
	_active = nullptr;
	return true;
#else
	return false;
#endif
}

bool PresentationRenderHost::EnsureComposition()
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (UsesComposition()) return true;
	if (!IsAttached() || _transactionOpen) return false;
	if (!CreateCompositionResources()) return false;
	AdvanceResourceGeneration();
	InvalidateFrameHistory();
	return true;
#else
	return false;
#endif
}

bool PresentationRenderHost::UsesComposition() const noexcept
{
	return _composition && _composition->IsInitialized();
}

D2DGraphics* PresentationRenderHost::AcquireSceneLayer(
	size_t index,
	int layer,
	int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!UsesComposition() || _transactionOpen) return nullptr;
	while (_sceneLayers.size() <= index)
	{
		void* swapChainPointer = nullptr;
		IDCompositionVisual* visual = nullptr;
		if (!_composition->CreateD2DLayer(
			&swapChainPointer, &visual, layer, order)) return nullptr;
		auto* swapChain = static_cast<IDXGISwapChain1*>(swapChainPointer);
		auto graphics = std::make_unique<CompositionSwapChainGraphics>(
			swapChain);
		if (swapChain) swapChain->Release();
		if (!graphics->GetDeviceContextRaw())
		{
			if (visual)
			{
				_composition->DestroyD2DLayer(visual);
				visual->Release();
			}
			return nullptr;
		}
		auto recorder = std::make_unique<D2DGraphics>(
			graphics->GetDeviceRaw());
		if (!recorder->GetDeviceContextRaw())
		{
			if (visual)
			{
				_composition->DestroyD2DLayer(visual);
				visual->Release();
			}
			return nullptr;
		}
		const auto dpi = static_cast<FLOAT>(_dpi);
		graphics->SetDpi(dpi, dpi);
		recorder->SetDpi(dpi, dpi);
		_sceneLayers.push_back(SceneLayer{
			visual, std::move(graphics), std::move(recorder) });
		AdvanceResourceGeneration();
		InvalidateFrameHistory();
	}

	auto& item = _sceneLayers[index];
	if (item.Visual)
		_composition->UpdateVisualOrder(item.Visual, layer, order);
	return item.Graphics.get();
#else
	(void)index;
	(void)layer;
	(void)order;
	return nullptr;
#endif
}

void PresentationRenderHost::TrimSceneLayers(size_t usedCount) noexcept
{
	if (usedCount >= _sceneLayers.size() || _transactionOpen) return;
	while (_sceneLayers.size() > usedCount)
	{
		auto& item = _sceneLayers.back();
		item.Recorder.reset();
		item.Graphics.reset();
		if (item.Visual)
		{
			if (_composition) _composition->DestroyD2DLayer(item.Visual);
			item.Visual->Release();
			item.Visual = nullptr;
		}
		_sceneLayers.pop_back();
	}
	AdvanceResourceGeneration();
	InvalidateFrameHistory();
}

void PresentationRenderHost::ReleaseSceneLayers() noexcept
{
	for (auto& item : _sceneLayers)
	{
		item.Recorder.reset();
		item.Graphics.reset();
		if (item.Visual)
		{
			if (_composition) _composition->DestroyD2DLayer(item.Visual);
			item.Visual->Release();
			item.Visual = nullptr;
		}
	}
	_sceneLayers.clear();
}

IDCompositionDevice* PresentationRenderHost::CompositionDevice() const noexcept
{
	return _composition ? _composition->GetDCompDevice() : nullptr;
}

IDCompositionVisual* PresentationRenderHost::WebContainerVisual() const noexcept
{
	return _composition ? _composition->GetWebContainerVisual() : nullptr;
}

bool PresentationRenderHost::RegisterCompositionVisual(
	IDCompositionVisual* visual,
	int layer,
	int order)
{
	return _composition
		&& _composition->RegisterVisual(visual, layer, order);
}

void PresentationRenderHost::UpdateCompositionVisualOrder(
	IDCompositionVisual* visual,
	int layer,
	int order)
{
	if (_composition) _composition->UpdateVisualOrder(visual, layer, order);
}

void PresentationRenderHost::UnregisterCompositionVisual(
	IDCompositionVisual* visual)
{
	if (_composition) _composition->UnregisterVisual(visual);
}

bool PresentationRenderHost::CommitComposition() noexcept
{
	if (!_composition) return true;
	// Native visuals may request a commit while their node is being submitted.
	// The active frame transaction owns the single atomic commit point.
	if (_transactionOpen) return true;
	const HRESULT result = _composition->CommitComposition();
	if (SUCCEEDED(result)) return true;
	_deviceResetRequested = true;
	InvalidateFrameHistory();
	return false;
}

void PresentationRenderHost::AdvanceResourceGeneration() noexcept
{
	if (++_resourceGeneration == 0) ++_resourceGeneration;
}
