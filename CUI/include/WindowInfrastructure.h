#pragma once

#include "PresentationScene.h"
#include "Window.h"

namespace cui::framework
{
	/** Narrow framework/test bridge for Window-owned mutable services. */
	struct WindowAccess final
	{
		static RoutedCommandManager& Commands(Window& target) noexcept
		{
			return *target._commandManager;
		}

		static const RoutedCommandManager& Commands(
			const Window& target) noexcept
		{
			return *target._commandManager;
		}

		static TextCompositionManager& TextComposition(
			Window& target) noexcept
		{
			return *target._textCompositionManager;
		}

		static void ApplySystemVisualPreferences(
			Window& target,
			SystemVisualPreferences preferences)
		{
			target.ApplySystemVisualPreferences(preferences);
		}

		static bool OpenTransientPresentation(
			Window& target,
			Control* root,
			TransientPresentationOptions options,
			TransientPresentationDismissHandler dismiss)
		{
			return target.OpenTransientPresentation(root, options, dismiss);
		}

		static bool CloseTransientPresentation(
			Window& target, Control* root)
		{
			return target.CloseTransientPresentation(root);
		}

		static bool IsTransientPresentationOpen(
			const Window& target, const Control* root) noexcept
		{
			return target.IsTransientPresentationOpen(root);
		}

		static Control* GetTopmostTransientPresentation(
			const Window& target) noexcept
		{
			return target.GetTopmostTransientPresentation();
		}

		static size_t GetTransientPresentationCount(
			const Window& target) noexcept
		{
			return target.GetTransientPresentationCount();
		}

		static void DismissTransientPresentationsForPointer(
			Window& target, Control* hitControl)
		{
			target.DismissTransientPresentationsForPointer(hitControl);
		}

		static D2D1_COLOR_F EffectiveControlBackColor(
			const Window& target, D2D1_COLOR_F configured) noexcept
		{
			return target.GetEffectiveControlBackColor(configured);
		}

		static D2D1_COLOR_F EffectiveControlForeColor(
			const Window& target, D2D1_COLOR_F configured) noexcept
		{
			return target.GetEffectiveControlForeColor(configured);
		}

		static std::vector<Control*> BuildTabOrder(
			std::span<Control* const> roots)
		{
			return Window::BuildTabOrder(roots);
		}

		static std::vector<Control*> AccessibleControlsForTesting(
			const Window& target)
		{
			return target.GetAccessibleControls();
		}

		static Control* ResolveAccessibleControlForTesting(
			const Window& target, Control* candidate) noexcept
		{
			return target.ResolveAccessibleControl(candidate);
		}

		static bool TryGetAccessibilityVirtualFocusedNodeForTesting(
			Window& target, Control*& owner, uint32_t& virtualId)
		{
			return target.TryGetAccessibilityVirtualFocusedNode(
				owner, virtualId);
		}

		static bool ProcessAccessKey(Window& target, wchar_t key)
		{
			return target.ProcessAccessKey(key);
		}

		static Control* HitTestControlAt(
			Window& target, int contentX, int contentY)
		{
			return target.HitTestControlAt(POINT{
				static_cast<LONG>(contentX),
				static_cast<LONG>(contentY) });
		}

#if CUI_RUNTIME_FLAVOR_DESIGN
		static void UpdateMouseOverProjectionForTesting(
			Window& target,
			Control* directlyOver,
			int contentX,
			int contentY,
			bool raiseDirectEvents = true)
		{
			target.UpdateMouseOverProjection(
				directlyOver,
				POINT{ static_cast<LONG>(contentX),
					static_cast<LONG>(contentY) },
				raiseDirectEvents);
		}
#endif

		static void UpdateCursorFromCurrentMouse(Window& target)
		{
			target.UpdateCursorFromCurrentMouse();
		}

		/** Implements Window.DialogCancelCommand for Button.IsCancel. */
		static bool TryCancelDialog(Window& target)
		{
			if (!target._showingAsDialog) return false;
			target.DialogResult = false;
			return true;
		}

		static InputStagingStatistics InputStatistics(
			const Window& target) noexcept
		{
			return target.GetInputStagingStatistics();
		}

		static FocusManagerStatistics FocusStatistics(
			const Window& target) noexcept
		{
			return target.GetFocusManagerStatistics();
		}

		static TextCompositionSnapshot TextCompositionState(
			const Window& target)
		{
			return target.GetTextCompositionSnapshot();
		}

		static TextCompositionStatistics TextCompositionStatisticsOf(
			const Window& target) noexcept
		{
			return target.GetTextCompositionStatistics();
		}

		static bool HasPendingRenderWork(const Window& target) noexcept
		{
			return target.HasPendingRenderWork();
		}

		/** Completes the normal Show-time DPI setup without activating a test HWND. */
		static void EnsureInitialDpiForTesting(Window& target)
		{
			target.EnsureInitialDpiApplied();
		}

		static UINT PresentationDispatchMessageForTesting() noexcept
		{
			return Window::GetPresentationDispatchMessageForTesting();
		}

		static bool HasPendingPresentationDamage(
			const Window& target) noexcept
		{
			return target.HasPendingPresentationDamage();
		}

		static bool TryGetLastRenderDirtyRect(
			const Window& target, RECT& logicalDirty, bool& fullFrame) noexcept
		{
			return target.TryGetLastRenderDirtyRect(logicalDirty, fullFrame);
		}

		/** Test seam for the damage clear versus stable client-frame geometry. */
		static RECT PresentationBackdropDamageForTesting(
			const RECT& logicalDirty,
			const RECT& logicalClient) noexcept
		{
			return Window::ResolvePresentationBackdropGeometry(
				logicalDirty, logicalClient).Damage;
		}

		static RECT PresentationClientFrameForTesting(
			const RECT& logicalDirty,
			const RECT& logicalClient) noexcept
		{
			return Window::ResolvePresentationBackdropGeometry(
				logicalDirty, logicalClient).ClientFrame;
		}

		static uint64_t PresentationSceneRevision(
			const Window& target) noexcept
		{
			return target.GetPresentationSceneRevision();
		}

		/** Synchronizes the retained scene and returns a control's paint order. */
		static int PresentationOrder(
			Window& target, Control* control)
		{
			return target.GetPresentationOrder(control);
		}

		static uint64_t PresentationContentRevision(
			const Window& target) noexcept
		{
			return target.GetPresentationContentRevision();
		}

		static uint64_t PresentationGeometryRevision(
			const Window& target) noexcept
		{
			return target.GetPresentationGeometryRevision();
		}

		static uint64_t PresentationCompositionRevision(
			const Window& target) noexcept
		{
			return target.GetPresentationCompositionRevision();
		}

		static uint64_t PresentationResourceGeneration(
			const Window& target) noexcept
		{
			return target.GetPresentationResourceGeneration();
		}

		static uint64_t PresentationTransactionSequence(
			const Window& target) noexcept
		{
			return target.GetPresentationTransactionSequence();
		}

		static uint64_t PresentationCommittedFrameCount(
			const Window& target) noexcept
		{
			return target.GetPresentationCommittedFrameCount();
		}

		static uint64_t PresentationAbortedFrameCount(
			const Window& target) noexcept
		{
			return target.GetPresentationAbortedFrameCount();
		}

		static uint64_t PresentationDeviceRecoveryCount(
			const Window& target) noexcept
		{
			return target.GetPresentationDeviceRecoveryCount();
		}

		static PresentationRenderHost::ResourceSnapshot
			PresentationResourcesForTesting(const Window& target) noexcept
		{
			return target._renderHost
				? target._renderHost->Resources()
				: PresentationRenderHost::ResourceSnapshot{};
		}

		static void FailSceneLayerAllocationAfterForTesting(
			size_t successfulCreates) noexcept
		{
			PresentationRenderHost::
				FailSceneLayerAllocationAfterForTesting(successfulCreates);
		}

		static void ClearSceneLayerAllocationFailureForTesting() noexcept
		{
			PresentationRenderHost::
				ClearSceneLayerAllocationFailureForTesting();
		}

		static void FailNextSceneLayerTopologyBatchCommitForTesting() noexcept
		{
			PresentationRenderHost::
				FailNextSceneLayerTopologyBatchCommitForTesting();
		}

		static void ClearSceneLayerTopologyBatchCommitFailureForTesting() noexcept
		{
			PresentationRenderHost::
				ClearSceneLayerTopologyBatchCommitFailureForTesting();
		}

		static void FailNextSceneLayerGroupTopologyStageForTesting() noexcept
		{
			PresentationRenderHost::
				FailNextSceneLayerGroupTopologyStageForTesting();
		}

		static void ClearSceneLayerGroupTopologyStageFailureForTesting() noexcept
		{
			PresentationRenderHost::
				ClearSceneLayerGroupTopologyStageFailureForTesting();
		}

		static bool TryGetPresentationSceneLayerPixelDigestForTesting(
			const Window& target,
			size_t index,
			UINT& width,
			UINT& height,
			uint64_t& digest,
			size_t& nonTransparentPixels) noexcept
		{
			return target.TryGetPresentationSceneLayerPixelDigestForTesting(
				index, width, height, digest, nonTransparentPixels);
		}

		static bool AcquirePresentationSceneLayerPixelReadbackLeaseForTesting(
			Window& target) noexcept
		{
			return target.
				AcquirePresentationSceneLayerPixelReadbackLeaseForTesting();
		}

		static void ReleasePresentationSceneLayerPixelReadbackLeaseForTesting(
			Window& target) noexcept
		{
			target.ReleasePresentationSceneLayerPixelReadbackLeaseForTesting();
		}

		static uint64_t PresentationLastSurfaceFailureSequence(
			const Window& target) noexcept
		{
			return target.GetPresentationLastSurfaceFailureSequence();
		}

		static uint8_t PresentationLastFailedSurfaceRole(
			const Window& target) noexcept
		{
			return target.GetPresentationLastFailedSurfaceRole();
		}

		static HRESULT PresentationLastFailedEndDrawHr(
			const Window& target) noexcept
		{
			return target.GetPresentationLastFailedEndDrawHr();
		}

		static HRESULT PresentationLastFailedPresentHr(
			const Window& target) noexcept
		{
			return target.GetPresentationLastFailedPresentHr();
		}

		static size_t PresentationNodeCount(const Window& target) noexcept
		{
			return target.GetPresentationNodeCount();
		}

		static size_t PresentationDrawingLayerCount(
			const Window& target) noexcept
		{
			return target.GetPresentationDrawingLayerCount();
		}

		static size_t PresentationOpacityGroupCount(
			const Window& target) noexcept
		{
			return target.GetPresentationOpacityGroupCount();
		}

		static size_t PresentationGroupedNativeVisualCount(
			const Window& target) noexcept
		{
			return target.GetPresentationGroupedNativeVisualCount();
		}

		static int TitleBarHeightPixelsForTesting(
			const Window& target) noexcept
		{
			return target.GetTitleBarHeightPixels();
		}

		static IDCompositionDevice* CompositionDevice(Window& target)
		{
			return target.GetDCompDevice();
		}

		static bool RegisterCompositionVisual(
			Window& target,
			IDCompositionVisual* visual,
			int layer,
			int order)
		{
			return target.RegisterDCompVisual(visual, layer, order);
		}

		static void UpdateCompositionVisualOrder(
			Window& target,
			IDCompositionVisual* visual,
			int layer,
			int order)
		{
			target.UpdateDCompVisualOrder(visual, layer, order);
		}

		static void UnregisterCompositionVisual(
			Window& target,
			IDCompositionVisual* visual)
		{
			target.UnregisterDCompVisual(visual);
		}

		static bool CommitComposition(Window& target)
		{
			return target.CommitComposition();
		}

		static bool PresentationRequiresComposition(
			const Window& target) noexcept
		{
			return target._presentationScene
				&& target._presentationScene->RequiresComposition();
		}

		static PresentationFrameStatistics PresentationFrame(
			const Window& target) noexcept
		{
			return target.GetPresentationFrameStatistics();
		}

		static bool TryGetPresentationNodeSnapshot(
			const Window& target,
			const Control* control,
			PresentationNodeSnapshot& out) noexcept
		{
			out = {};
			return target._presentationScene
				&& target._presentationScene->TryGetNodeSnapshot(
					control, out);
		}

		static void TickPresentationAnimationsForTesting(Window& target)
		{
			target.InvalidateAnimatedControls(false);
		}

		static void TickPresentationAnimationsForTesting(
			Window& target,
			unsigned long long nowMilliseconds)
		{
			target.InvalidateAnimatedControlsAt(nowMilliseconds, false);
		}

		/** Test seam for the window-owned native/declarative animation scheduler. */
		static UINT AnimationTimerIntervalForTesting(
			const Window& target) noexcept
		{
			return target._animIntervalMs;
		}

		static bool AnimationFrameSchedulerRunningForTesting(
			const Window& target) noexcept
		{
			return target.IsAnimationFrameSchedulerRunningForTesting();
		}

		static bool AnimationUsesLegacyTimerForTesting(
			const Window& target) noexcept
		{
			return target._animationUsesLegacyTimer;
		}

		static size_t RegisteredDeclarativeAnimationControlCountForTesting(
			Window& target)
		{
			return target.GetRegisteredDeclarativeAnimationControls().size();
		}

		static size_t RegisteredNativeAnimationControlCountForTesting(
			Window& target)
		{
			return target.GetRegisteredNativeAnimationControls().size();
		}

		static size_t ActiveNativeAnimationControlCountForTesting(
			Window& target)
		{
			return target.GetActiveRegisteredNativeAnimationControls().size();
		}

		static bool AnimationRegistryDegradedForTesting(
			const Window& target) noexcept
		{
			return target._animationRegistryDegraded;
		}

		static void TickRegisteredDeclarativeAnimationsForTesting(
			Window& target,
			unsigned long long nowMilliseconds)
		{
			auto controls =
				target.GetActiveRegisteredDeclarativeAnimationControls();
			target.AdvanceRegisteredDeclarativeAnimationControls(
				controls, nowMilliseconds, false);
		}

		static bool TryGetCloseCaptionRectForTesting(
			Window& target, RECT& out) noexcept
		{
			return target.TryGetCaptionButtonRect(
				Window::CaptionButtonKind::Close, out);
		}

		static void InjectPresentationDeviceLossForTesting(Window& target)
		{
			target.InjectPresentationDeviceLossForTesting();
		}

		static bool InjectSharedGraphicsDeviceRotationForTesting(Window& target)
		{
			return target.InjectSharedGraphicsDeviceRotationForTesting();
		}
	};
}
