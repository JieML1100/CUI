#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wrl/client.h>

#include "PresentationRenderHost.h"
#include "Visual.h"
#include "ControlWeakReference.h"

class Control;
class D2DGraphics;
struct ID2D1CommandList;

/** Shared DComp layer used by both retained drawing segments and native visuals. */
inline constexpr int PresentationSceneContentLayer = 1000;

/** Read-only retained-node snapshot used by framework diagnostics and tests. */
struct PresentationNodeSnapshot final
{
	bool ContentDirty = false;
	bool GeometryDirty = false;
	bool CompositionDirty = false;
	bool HasGeometry = false;
	bool HasPresented = false;
	bool HasDrawingCommands = false;
	bool NativeComposition = false;
	bool Overlay = false;
	D2D1_RECT_F RenderedBounds{};
};

/**
 * Retained, derived presentation snapshot for one Window visual tree.
 *
 * The snapshot owns no Controls and carries no document identity. Structural
 * mutations invalidate its topology; content, geometry and composition use
 * independent node revisions. Rendered bounds are cached per geometry revision,
 * while animated ancestor clips and damage remain live frame inputs. This keeps
 * tree discovery and D2D/native segmentation outside the native Window shell.
 */
class PresentationScene final
{
public:
	PresentationScene() = default;
	PresentationScene(const PresentationScene&) = delete;
	PresentationScene& operator=(const PresentationScene&) = delete;

	void InvalidateStructure() noexcept { _structureDirty = true; }
	void InvalidateNode(
		Control* control,
		PresentationInvalidationKind kind) noexcept;
	bool Synchronize(
		std::span<Control* const> roots,
		std::span<Control* const> transientRoots);
	/** Drops all device-domain caches when the host generation changes. */
	void SynchronizeResourceGeneration(uint64_t generation) noexcept;

	bool RequiresComposition() const noexcept { return _requiresComposition; }
	bool IsRendering() const noexcept { return _rendering; }
	uint64_t Revision() const noexcept { return _revision; }
	uint64_t ContentRevision() const noexcept { return _contentRevision; }
	uint64_t GeometryRevision() const noexcept { return _geometryRevision; }
	uint64_t CompositionRevision() const noexcept
	{
		return _compositionRevision;
	}
	size_t NodeCount() const noexcept { return _nodes.size(); }
	size_t DrawingLayerCount() const noexcept { return _segments.size(); }
	PresentationFrameStatistics FrameStatistics() const noexcept
	{
		return _frameStatistics;
	}
	bool TryGetNodeSnapshot(
		const Control* control,
		PresentationNodeSnapshot& out) const noexcept;

	/** Allocates/orders every retained D2D segment before a frame begins. */
	bool PrepareComposition(PresentationRenderHost& host);
	/** Draws the flattened scene into retained D2D layers and native visuals. */
	bool RenderComposition(
		PresentationRenderHost& host,
		PresentationRenderHost::FrameTransaction& transaction,
		const RECT& contentDirty,
		int titleBarOffset,
		float logicalWidth,
		float logicalHeight);
	/** Draws flattened content nodes into the active raster surface. */
	void RenderRaster(const RECT& contentDirty);
	/** Draws flattened transient nodes into the active overlay/raster surface. */
	void RenderOverlay(const RECT& contentDirty);

	int GetOrder(const Control* control) const noexcept;

private:
	static constexpr size_t NoSegment = static_cast<size_t>(-1);

	struct Node
	{
		// The retained scene is allowed to be invalidated by a control while an
		// earlier node is preparing or rendering.  Keep only a lifetime-checked
		// identity here: a raw Control* can otherwise outlive a visual mutation
		// until the next frame synchronizes the topology.
		ControlWeakReference Element;
		int Order = 0;
		size_t SegmentIndex = NoSegment;
		size_t SubtreeEnd = 0;
		bool NativeComposition = false;
		bool Overlay = false;
		bool ContentDirty = true;
		bool GeometryDirty = true;
		bool CompositionDirty = true;
		bool HasGeometry = false;
		bool HasPresented = false;
		PresentationRevisionSnapshot AppliedRevisions{};
		D2D1_RECT_F RenderedBounds{};
		Microsoft::WRL::ComPtr<ID2D1CommandList> DrawingCommands;
		uint64_t CommandGeneration = 0;
	};

	struct Segment
	{
		int Order = 0;
		D2DGraphics* Context = nullptr;
	};

	std::vector<Control*> _rasterRoots;
	std::vector<Node> _nodes;
	std::vector<Segment> _segments;
	std::unordered_map<Control*, size_t> _nodeIndex;
	std::vector<std::pair<size_t, size_t>> _pendingGeometryRanges;
	bool _allGeometryDirty = false;
	std::vector<Control*> _transientRoots;
	bool _requiresComposition = false;
	bool _structureDirty = true;
	bool _rendering = false;
	uint64_t _revision = 0;
	uint64_t _contentRevision = 1;
	uint64_t _geometryRevision = 1;
	uint64_t _compositionRevision = 1;
	uint64_t _frameSequence = 0;
	uint64_t _resourceGeneration = 0;
	size_t _pendingCommandCacheInvalidations = 0;
	uint64_t _preparedRevision = 0;
	bool _hasPreparedRevision = false;
	PresentationFrameStatistics _frameStatistics;

	void Rebuild(std::span<Control* const> roots);
	bool RefreshNodeState(Node& node);
	void ApplyPendingGeometryInvalidations();
	bool RefreshNodeGeometry(Node& node);
	bool CompleteNode(
		Node& node,
		const PresentationRevisionSnapshot& submitted);
	bool PrepareNodeForRendering(Node& node, Control*& control);
	void BeginFrameStatistics() noexcept;
};
