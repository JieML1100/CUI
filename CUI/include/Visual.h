#pragma once

#include "DependencyObject.h"
#include "Geometry.h"
#include "ObservableCollection.h"
#include "Transform.h"

#include <d2d1.h>
#include <cstdint>
#include <span>
#include <vector>

class Control;
class Window;

namespace cui::framework
{
	struct TreeAccess;
}

/** Owned visual-child collection. Mutations are synchronized by Visual. */
class VisualCollection final : public ObservableCollection<Control*>
{
public:
	using CollectionBase = ObservableCollection<Control*>;
	VisualCollection() = default;
	VisualCollection(const VisualCollection&) = delete;
	VisualCollection(VisualCollection&&) = delete;
	VisualCollection& operator=(const VisualCollection&) = delete;
	VisualCollection& operator=(VisualCollection&&) = delete;
	VisualCollection& operator=(std::initializer_list<Control*>) = delete;

private:
	void RestoreRejectedMutation(std::span<Control* const> snapshot)
	{
		RestoreOwnerSnapshot(snapshot);
	}
	using CollectionBase::SetOwnerChangedHandler;
	using CollectionBase::SetOwnerSynchronizationDuringUpdates;
	friend class Control;
};

using VisualParentChangedEvent = Event<void(Control*, Control*, Control*)>;

/** How a visual contributes pixels to the retained presentation scene. */
enum class PresentationSurfaceKind : uint8_t
{
	Drawing,
	NativeComposition,
};

/** Independent retained-scene invalidation lanes for one Visual. */
enum class PresentationInvalidationKind : uint8_t
{
	None = 0,
	Content = 1u << 0,
	Geometry = 1u << 1,
	Composition = 1u << 2,
};

inline PresentationInvalidationKind operator|(
	PresentationInvalidationKind left,
	PresentationInvalidationKind right) noexcept
{
	return static_cast<PresentationInvalidationKind>(
		static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

inline bool HasPresentationInvalidation(
	PresentationInvalidationKind value,
	PresentationInvalidationKind lane) noexcept
{
	return (static_cast<uint8_t>(value) & static_cast<uint8_t>(lane)) != 0;
}

/** Monotonic local revisions consumed by PresentationScene nodes. */
struct PresentationRevisionSnapshot
{
	uint64_t Content = 1;
	uint64_t Geometry = 1;
	uint64_t Composition = 1;
};

/** Classification of work performed by the most recent scene frame. */
struct PresentationFrameStatistics
{
	uint64_t Frame = 0;
	uint64_t Transaction = 0;
	uint64_t ResourceGeneration = 0;
	size_t ContentDirtyNodes = 0;
	size_t GeometryDirtyNodes = 0;
	size_t CompositionDirtyNodes = 0;
	size_t GeometryRecomputedNodes = 0;
	size_t ImmediateDrawNodes = 0;
	size_t DamageReplayNodes = 0;
	size_t CommandRecordedNodes = 0;
	size_t CommandReplayedNodes = 0;
	size_t CommandCacheHitNodes = 0;
	size_t CommandCacheInvalidatedNodes = 0;
	size_t NativeCommitNodes = 0;
	size_t CulledNodes = 0;
};

/** Owns the retained visual relation, transform, clip and dirty-region state. */
class Visual : public DependencyObject
{
private:
	VisualCollection _visualChildren;
	friend class Control;
	friend struct cui::framework::TreeAccess;

protected:
	Window* _presentationWindow = nullptr;
	void SetPresentationWindowCore(Window* value) noexcept
	{
		_presentationWindow = value;
	}
	std::vector<Control*> _observedVisualChildren;
	Control* _visualParent = nullptr;
	int _zIndex = 0;
	D2D1_RECT_F _lastInvalidatedClientRect{ 0, 0, 0, 0 };
	bool _hasLastInvalidatedClientRect = false;
	bool _hasPresentationOrderOverride = false;
	int _presentationOrderOverride = 0;
	bool _isWindowRoot = false;
	std::optional<cui::drawing::Geometry> _clip;
	std::optional<cui::drawing::Transform> _renderTransform;
	D2D1_POINT_2F _renderTransformOrigin{ 0.0f, 0.0f };
	size_t _activeGeometryClipCount = 0;
	PresentationRevisionSnapshot _presentationRevisions;
	VisualParentChangedEvent OnVisualParentChanged;

public:
	Visual() = default;
	~Visual() override = default;

	/** Read-only projection of the native presentation host; tree attachment owns mutation. */
	Window* GetPresentationWindow() const noexcept
	{
		return _presentationWindow;
	}
	/** Read-only visual-tree projection; mutations must use Control ownership APIs. */
	std::span<Control* const> GetVisualChildrenView() const noexcept
	{
		return { _visualChildren.data(), _visualChildren.size() };
	}
	__declspec(property(put = SetZIndex, get = GetZIndex)) int ZIndex;
	int GetZIndex();
	void SetZIndex(int value);

	Control* GetVisualParent() const noexcept { return _visualParent; }
	PresentationRevisionSnapshot GetPresentationRevisions() const noexcept
	{
		return _presentationRevisions;
	}
	virtual void InvalidateVisual() = 0;
};
