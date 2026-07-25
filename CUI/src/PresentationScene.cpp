#include "PresentationScene.h"

#include "Control.h"
#include "PresentationRenderHost.h"
#include "Graphics.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace
{
	void AdvanceRevision(uint64_t& value) noexcept
	{
		++value;
		if (value == 0) ++value;
	}

	bool RectIntersects(const RECT& left, const RECT& right) noexcept
	{
		return left.left < right.right && left.right > right.left
			&& left.top < right.bottom && left.bottom > right.top;
	}

	RECT ToRect(D2D1_RECT_F value, int inflate = 0) noexcept
	{
		return RECT{
			static_cast<LONG>(std::floor(value.left)) - inflate,
			static_cast<LONG>(std::floor(value.top)) - inflate,
			static_cast<LONG>(std::ceil(value.right)) + inflate,
			static_cast<LONG>(std::ceil(value.bottom)) + inflate };
	}

	bool GetClientClip(
		Control* control,
		const RECT& contentDirty,
		int titleBarOffset,
		RECT& result)
	{
		if (!control) return false;
		result = contentDirty;
		result.top += titleBarOffset;
		result.bottom += titleBarOffset;

		for (Control* current = control->GetVisualParent(); current;
			current = current->GetVisualParent())
		{
			if (!current->ClipsChildren()) continue;
			auto clip = current->GetVisualChildrenClipRect();
			const auto absolute = current->GetAbsoluteLocationDip();
			clip = current->TransformAbsoluteRectToRenderSpace(D2D1_RECT_F{
				clip.left + absolute.x,
				clip.top + absolute.y,
				clip.right + absolute.x,
				clip.bottom + absolute.y });
			RECT clipRect{
				static_cast<LONG>(std::floor(clip.left)),
				static_cast<LONG>(std::floor(clip.top)) + titleBarOffset,
				static_cast<LONG>(std::ceil(clip.right)),
				static_cast<LONG>(std::ceil(clip.bottom)) + titleBarOffset };
			RECT intersection{};
			if (!::IntersectRect(&intersection, &result, &clipRect))
				return false;
			result = intersection;
		}
		return result.right > result.left && result.bottom > result.top;
	}

	std::vector<Control*> SortedVisibleChildren(Control* control)
	{
		auto children = control ? control->GetVisualChildrenInZOrder()
			: std::vector<Control*>{};
		children.erase(
			std::remove_if(children.begin(), children.end(), [](Control* child)
			{
				return !child || !child->IsVisible
					|| !child->ParticipatesInPresentationScene();
			}),
			children.end());
		return children;
	}
}

void PresentationScene::InvalidateNode(
	Control* control,
	PresentationInvalidationKind kind) noexcept
{
	if (!control || kind == PresentationInvalidationKind::None) return;
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Content))
		AdvanceRevision(_contentRevision);
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Geometry))
		AdvanceRevision(_geometryRevision);
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Composition))
		AdvanceRevision(_compositionRevision);

	if (_structureDirty) return;
	const auto found = _nodeIndex.find(control);
	if (found == _nodeIndex.end() || found->second >= _nodes.size()) return;
	auto& node = _nodes[found->second];
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Content))
		node.ContentDirty = true;
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Composition))
		node.CompositionDirty = true;
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Geometry))
	{
		const size_t end = (std::min)(node.SubtreeEnd, _nodes.size());
		try
		{
			_pendingGeometryRanges.emplace_back(found->second, end);
		}
		catch (...)
		{
			// Allocation failure may cost one full geometry pass, never correctness.
			_allGeometryDirty = true;
			_pendingGeometryRanges.clear();
		}
	}
}

bool PresentationScene::Synchronize(
	std::span<Control* const> roots,
	std::span<Control* const> transientRoots)
{
	if (_transientRoots.size() != transientRoots.size()
		|| !std::equal(
			_transientRoots.begin(), _transientRoots.end(),
			transientRoots.begin(), transientRoots.end()))
	{
		_transientRoots.assign(transientRoots.begin(), transientRoots.end());
		_structureDirty = true;
	}
	if (!_structureDirty || _rendering) return false;
	Rebuild(roots);
	_structureDirty = false;
	AdvanceRevision(_revision);
	return true;
}

void PresentationScene::SynchronizeResourceGeneration(
	uint64_t generation) noexcept
{
	if (generation == 0 || generation == _resourceGeneration) return;
	_resourceGeneration = generation;
	for (auto& node : _nodes)
	{
		if (node.DrawingCommands)
		{
			node.DrawingCommands.Reset();
			node.CommandGeneration = 0;
			++_pendingCommandCacheInvalidations;
		}
		node.HasPresented = false;
	}
}

void PresentationScene::Rebuild(std::span<Control* const> roots)
{
	_rasterRoots.assign(roots.begin(), roots.end());
	std::stable_sort(
		_rasterRoots.begin(), _rasterRoots.end(),
		[](Control* left, Control* right)
		{
			if (!left || !right) return left != nullptr;
			return left->ZIndex < right->ZIndex;
		});
	_rasterRoots.erase(
		std::remove_if(
			_rasterRoots.begin(), _rasterRoots.end(),
			[this](Control* control)
			{
				if (!control || !control->IsVisible) return true;
				return std::find(
					_transientRoots.begin(), _transientRoots.end(), control)
					!= _transientRoots.end();
			}),
		_rasterRoots.end());

	_pendingCommandCacheInvalidations += static_cast<size_t>(std::count_if(
		_nodes.begin(), _nodes.end(), [](const Node& node)
		{ return node.DrawingCommands != nullptr; }));
	_nodes.clear();
	_segments.clear();
	_nodeIndex.clear();
	_pendingGeometryRanges.clear();
	_allGeometryDirty = false;
	_requiresComposition = false;
	bool segmentOpen = false;
	size_t currentSegment = NoSegment;
	int order = 0;
	auto isTransientRoot = [this](Control* control)
	{
		return std::find(
			_transientRoots.begin(), _transientRoots.end(), control)
			!= _transientRoots.end();
	};
	std::function<void(Control*, bool)> visit =
		[&](Control* control, bool overlay)
	{
		if (!control || !control->IsVisible) return;
		if (!overlay && isTransientRoot(control)) return;
		if (_nodeIndex.contains(control)) return;
		const bool native = control->GetPresentationSurfaceKind()
			== PresentationSurfaceKind::NativeComposition;
		if (overlay)
		{
			segmentOpen = false;
			currentSegment = NoSegment;
			if (native) _requiresComposition = true;
		}
		else if (native)
		{
			segmentOpen = false;
			currentSegment = NoSegment;
			_requiresComposition = true;
		}
		else if (!segmentOpen)
		{
			currentSegment = _segments.size();
			_segments.push_back(Segment{ order, nullptr });
			segmentOpen = true;
		}
		const size_t nodeIndex = _nodes.size();
		Node node;
		node.Element = control;
		node.Order = order++;
		node.SegmentIndex = currentSegment;
		node.NativeComposition = native;
		node.Overlay = overlay;
		node.AppliedRevisions = {};
		_nodes.push_back(std::move(node));
		_nodeIndex.emplace(control, nodeIndex);
		for (auto* child : SortedVisibleChildren(control))
			visit(child, overlay);
		_nodes[nodeIndex].SubtreeEnd = _nodes.size();
	};
	for (auto* root : _rasterRoots) visit(root, false);
	segmentOpen = false;
	currentSegment = NoSegment;
	for (auto* root : _transientRoots) visit(root, true);
}

void PresentationScene::BeginFrameStatistics() noexcept
{
	_frameStatistics = {};
	AdvanceRevision(_frameSequence);
	_frameStatistics.Frame = _frameSequence;
	_frameStatistics.CommandCacheInvalidatedNodes =
		_pendingCommandCacheInvalidations;
	_pendingCommandCacheInvalidations = 0;
}

void PresentationScene::RefreshNodeState(Node& node)
{
	if (!node.Element) return;
	const auto revisions = node.Element->GetPresentationRevisions();
	if (revisions.Content != node.AppliedRevisions.Content)
		node.ContentDirty = true;
	if (revisions.Geometry != node.AppliedRevisions.Geometry)
		node.GeometryDirty = true;
	if (revisions.Composition != node.AppliedRevisions.Composition)
		node.CompositionDirty = true;
}

void PresentationScene::ApplyPendingGeometryInvalidations()
{
	if (_allGeometryDirty)
	{
		for (auto& node : _nodes) node.GeometryDirty = true;
		_allGeometryDirty = false;
		_pendingGeometryRanges.clear();
		return;
	}
	if (_pendingGeometryRanges.empty()) return;
	std::sort(_pendingGeometryRanges.begin(), _pendingGeometryRanges.end());
	size_t rangeStart = _pendingGeometryRanges.front().first;
	size_t rangeEnd = _pendingGeometryRanges.front().second;
	auto applyRange = [this](size_t start, size_t end)
	{
		end = (std::min)(end, _nodes.size());
		for (size_t index = (std::min)(start, end); index < end; ++index)
			_nodes[index].GeometryDirty = true;
	};
	for (size_t index = 1; index < _pendingGeometryRanges.size(); ++index)
	{
		const auto [nextStart, nextEnd] = _pendingGeometryRanges[index];
		if (nextStart <= rangeEnd)
		{
			rangeEnd = (std::max)(rangeEnd, nextEnd);
			continue;
		}
		applyRange(rangeStart, rangeEnd);
		rangeStart = nextStart;
		rangeEnd = nextEnd;
	}
	applyRange(rangeStart, rangeEnd);
	_pendingGeometryRanges.clear();
}

void PresentationScene::RefreshNodeGeometry(Node& node)
{
	if (!node.Element || (!node.GeometryDirty && node.HasGeometry)) return;
	const auto geometryRevision =
		node.Element->GetPresentationRevisions().Geometry;
	node.RenderedBounds = node.Element->GetRenderedAbsoluteRectDip();
	node.HasGeometry = true;
	node.AppliedRevisions.Geometry = geometryRevision;
	node.GeometryDirty =
		node.Element->GetPresentationRevisions().Geometry != geometryRevision;
	++_frameStatistics.GeometryRecomputedNodes;
}

void PresentationScene::CompleteNode(
	Node& node,
	const PresentationRevisionSnapshot& submitted)
{
	if (!node.Element) return;
	node.AppliedRevisions = submitted;
	const auto current = node.Element->GetPresentationRevisions();
	node.ContentDirty = current.Content != submitted.Content;
	node.GeometryDirty = current.Geometry != submitted.Geometry;
	node.CompositionDirty = current.Composition != submitted.Composition;
	node.HasPresented = true;
}

bool PresentationScene::PrepareComposition(PresentationRenderHost& host)
{
	if (!host.UsesComposition()) return false;
	for (size_t index = 0; index < _segments.size(); ++index)
	{
		auto& segment = _segments[index];
		segment.Context = host.AcquireSceneLayer(
			index, PresentationSceneContentLayer, segment.Order);
		if (!segment.Context)
		{
			for (auto& item : _segments) item.Context = nullptr;
			return false;
		}
	}
	host.TrimSceneLayers(_segments.size());
	if (!_hasPreparedRevision || _preparedRevision != _revision)
	{
		// Segment ownership can change while its count stays constant. No pixels
		// from the previous topology may leak into the new retained snapshot.
		host.InvalidateFrameHistory();
		_preparedRevision = _revision;
		_hasPreparedRevision = true;
	}
	return true;
}

bool PresentationScene::RenderComposition(
	PresentationRenderHost& host,
	PresentationRenderHost::FrameTransaction& transaction,
	const RECT& contentDirty,
	int titleBarOffset,
	float logicalWidth,
	float logicalHeight)
{
	(void)logicalWidth;
	(void)logicalHeight;
	if (!host.UsesComposition()
		|| !host.IsTransactionActive(transaction)
		|| contentDirty.right <= contentDirty.left
		|| contentDirty.bottom <= contentDirty.top) return false;
	BeginFrameStatistics();
	_frameStatistics.Transaction = transaction.Sequence;
	_frameStatistics.ResourceGeneration = transaction.ResourceGeneration;
	ApplyPendingGeometryInvalidations();

	struct RenderingScope
	{
		bool& Value;
		explicit RenderingScope(bool& value) : Value(value) { Value = true; }
		~RenderingScope() { Value = false; }
	} rendering(_rendering);

	RECT clientDirty = contentDirty;
	clientDirty.top += titleBarOffset;
	clientDirty.bottom += titleBarOffset;
	size_t activeSegment = NoSegment;
	D2DGraphics* segmentContext = nullptr;
	PresentationRenderHost::SurfaceFrame segmentFrame;
	bool frameHealthy = true;

	auto endSegment = [&]
	{
		if (!segmentContext) return;
		if (!host.CloseSurface(transaction, segmentFrame))
			frameHealthy = false;
		segmentContext = nullptr;
		activeSegment = NoSegment;
	};
	auto beginSegment = [&](size_t index)
	{
		if (!frameHealthy) return false;
		if (index == activeSegment) return true;
		endSegment();
		if (!frameHealthy || index >= _segments.size()
			|| !_segments[index].Context) return false;
		activeSegment = index;
		segmentContext = _segments[index].Context;
		if (!host.OpenSceneSurface(
			transaction, segmentContext, clientDirty, segmentFrame))
		{
			segmentContext = nullptr;
			activeSegment = NoSegment;
			frameHealthy = false;
			return false;
		}
		return true;
	};

	for (auto& node : _nodes)
	{
		if (!frameHealthy || transaction.Failed) break;
		if (node.Overlay) continue;
		auto* control = node.Element;
		if (!control) continue;
		if (node.NativeComposition)
		{
			endSegment();
			if (!frameHealthy) break;
			control->PreparePresentation();
			RefreshNodeState(node);
			const bool contentDirtyNode = node.ContentDirty;
			const bool geometryDirtyNode = node.GeometryDirty;
			const bool compositionDirtyNode = node.CompositionDirty;
			if (contentDirtyNode) ++_frameStatistics.ContentDirtyNodes;
			if (geometryDirtyNode) ++_frameStatistics.GeometryDirtyNodes;
			if (compositionDirtyNode)
				++_frameStatistics.CompositionDirtyNodes;
			RefreshNodeGeometry(node);
			if (!control->IsVisible)
			{
				++_frameStatistics.CulledNodes;
				continue;
			}
			if (!node.HasPresented || contentDirtyNode
				|| geometryDirtyNode || compositionDirtyNode)
			{
				const auto submitted = control->GetPresentationRevisions();
				control->SetPresentationOrderOverride(node.Order);
				control->OnRender();
				control->ClearPresentationOrderOverride();
				++_frameStatistics.NativeCommitNodes;
				CompleteNode(node, submitted);
			}
			continue;
		}

		if (!beginSegment(node.SegmentIndex)) break;
		control->PreparePresentation();
		RefreshNodeState(node);
		const bool contentDirtyNode = node.ContentDirty;
		const bool geometryDirtyNode = node.GeometryDirty;
		const bool compositionDirtyNode = node.CompositionDirty;
		if (contentDirtyNode) ++_frameStatistics.ContentDirtyNodes;
		if (geometryDirtyNode) ++_frameStatistics.GeometryDirtyNodes;
		if (compositionDirtyNode)
			++_frameStatistics.CompositionDirtyNodes;
		RefreshNodeGeometry(node);
		if (!control->IsVisible)
		{
			++_frameStatistics.CulledNodes;
			continue;
		}
		RECT controlRect = ToRect(node.RenderedBounds, 2);
		controlRect.top += titleBarOffset;
		controlRect.bottom += titleBarOffset;
		RECT clientClip{};
		if (!GetClientClip(control, contentDirty, titleBarOffset, clientClip)
			|| !RectIntersects(clientClip, controlRect))
		{
			++_frameStatistics.CulledNodes;
			continue;
		}

		const bool needsRecording = !node.DrawingCommands
			|| node.CommandGeneration != transaction.ResourceGeneration
			|| contentDirtyNode || geometryDirtyNode;
		const auto submitted = control->GetPresentationRevisions();
		if (needsRecording)
		{
			Microsoft::WRL::ComPtr<ID2D1CommandList> commands;
			if (!host.RecordDrawingCommands(
				transaction, segmentContext,
				[control] { control->OnRender(); },
				commands.ReleaseAndGetAddressOf()))
			{
				frameHealthy = false;
				break;
			}
			node.DrawingCommands = std::move(commands);
			node.CommandGeneration = transaction.ResourceGeneration;
			++_frameStatistics.CommandRecordedNodes;
		}
		else
		{
			++_frameStatistics.CommandCacheHitNodes;
		}

		segmentContext->PushDrawRect(
			static_cast<float>(clientClip.left),
			static_cast<float>(clientClip.top),
			static_cast<float>(clientClip.right - clientClip.left),
			static_cast<float>(clientClip.bottom - clientClip.top));
		const bool replayed = host.ReplayDrawingCommands(
			transaction, segmentContext, node.DrawingCommands.Get());
		segmentContext->PopDrawRect();
		if (!replayed)
		{
			frameHealthy = false;
			break;
		}
		++_frameStatistics.CommandReplayedNodes;
		if (!contentDirtyNode && !geometryDirtyNode
			&& !compositionDirtyNode)
			++_frameStatistics.DamageReplayNodes;
		CompleteNode(node, submitted);
	}
	endSegment();
	return frameHealthy && !transaction.Failed;
}

void PresentationScene::RenderRaster(const RECT& contentDirty)
{
	if (contentDirty.right <= contentDirty.left
		|| contentDirty.bottom <= contentDirty.top) return;
	BeginFrameStatistics();
	_frameStatistics.ResourceGeneration = _resourceGeneration;
	ApplyPendingGeometryInvalidations();
	struct RenderingScope
	{
		bool& Value;
		explicit RenderingScope(bool& value) : Value(value) { Value = true; }
		~RenderingScope() { Value = false; }
	} rendering(_rendering);

	// Raster fallback and composition now share the same retained traversal:
	// every scene node draws only itself.  Parent controls never own recursive
	// painting, so switching render hosts cannot change visual semantics.
	for (auto& node : _nodes)
	{
		if (node.Overlay) continue;
		auto* control = node.Element;
		if (!control || !control->IsVisible) continue;
		control->PreparePresentation();
		RefreshNodeState(node);
		const bool geometryDirtyNode = node.GeometryDirty;
		if (node.ContentDirty) ++_frameStatistics.ContentDirtyNodes;
		if (geometryDirtyNode) ++_frameStatistics.GeometryDirtyNodes;
		if (node.CompositionDirty)
			++_frameStatistics.CompositionDirtyNodes;
		if (!node.ContentDirty && !geometryDirtyNode
			&& !node.CompositionDirty)
			++_frameStatistics.DamageReplayNodes;
		RefreshNodeGeometry(node);
		const RECT controlRect = ToRect(node.RenderedBounds, 2);
		RECT clientClip{};
		if (!GetClientClip(control, contentDirty, 0, clientClip)
			|| !RectIntersects(clientClip, controlRect))
		{
			++_frameStatistics.CulledNodes;
			continue;
		}
		const auto submitted = control->GetPresentationRevisions();
		auto* drawingContext = control->GetDrawingContext();
		if (!drawingContext) continue;
		drawingContext->PushDrawRect(
			static_cast<float>(clientClip.left),
			static_cast<float>(clientClip.top),
			static_cast<float>(clientClip.right - clientClip.left),
			static_cast<float>(clientClip.bottom - clientClip.top));
		control->OnRender();
		drawingContext->PopDrawRect();
		++_frameStatistics.ImmediateDrawNodes;
		CompleteNode(node, submitted);
	}
}

void PresentationScene::RenderOverlay(const RECT& contentDirty)
{
	if (contentDirty.right <= contentDirty.left
		|| contentDirty.bottom <= contentDirty.top) return;
	struct RenderingScope
	{
		bool& Value;
		explicit RenderingScope(bool& value) : Value(value) { Value = true; }
		~RenderingScope() { Value = false; }
	} rendering(_rendering);

	for (auto& node : _nodes)
	{
		if (!node.Overlay) continue;
		auto* control = node.Element;
		if (!control || !control->IsVisible) continue;
		control->PreparePresentation();
		RefreshNodeState(node);
		const bool contentDirtyNode = node.ContentDirty;
		const bool geometryDirtyNode = node.GeometryDirty;
		const bool compositionDirtyNode = node.CompositionDirty;
		if (contentDirtyNode) ++_frameStatistics.ContentDirtyNodes;
		if (geometryDirtyNode) ++_frameStatistics.GeometryDirtyNodes;
		if (compositionDirtyNode)
			++_frameStatistics.CompositionDirtyNodes;
		RefreshNodeGeometry(node);
		const RECT controlRect = ToRect(node.RenderedBounds, 2);
		RECT clientClip{};
		if (!GetClientClip(control, contentDirty, 0, clientClip)
			|| !RectIntersects(clientClip, controlRect))
		{
			++_frameStatistics.CulledNodes;
			continue;
		}

		const auto submitted = control->GetPresentationRevisions();
		if (node.NativeComposition)
		{
			if (!node.HasPresented || contentDirtyNode
				|| geometryDirtyNode || compositionDirtyNode)
			{
				control->SetPresentationOrderOverride(node.Order);
				control->OnRender();
				control->ClearPresentationOrderOverride();
				++_frameStatistics.NativeCommitNodes;
				CompleteNode(node, submitted);
			}
			continue;
		}

		auto* drawingContext = control->GetDrawingContext();
		if (!drawingContext) continue;
		drawingContext->PushDrawRect(
			static_cast<float>(clientClip.left),
			static_cast<float>(clientClip.top),
			static_cast<float>(clientClip.right - clientClip.left),
			static_cast<float>(clientClip.bottom - clientClip.top));
		control->OnRender();
		drawingContext->PopDrawRect();
		++_frameStatistics.ImmediateDrawNodes;
		CompleteNode(node, submitted);
	}
}

bool PresentationScene::TryGetNodeSnapshot(
	const Control* control,
	PresentationNodeSnapshot& out) const noexcept
{
	out = {};
	if (!control) return false;
	const auto found = _nodeIndex.find(const_cast<Control*>(control));
	if (found == _nodeIndex.end() || found->second >= _nodes.size())
		return false;
	const auto& node = _nodes[found->second];
	out.ContentDirty = node.ContentDirty;
	out.GeometryDirty = node.GeometryDirty;
	out.CompositionDirty = node.CompositionDirty;
	out.HasGeometry = node.HasGeometry;
	out.HasPresented = node.HasPresented;
	out.HasDrawingCommands = node.DrawingCommands != nullptr;
	out.NativeComposition = node.NativeComposition;
	out.Overlay = node.Overlay;
	out.RenderedBounds = node.RenderedBounds;
	return true;
}

int PresentationScene::GetOrder(const Control* control) const noexcept
{
	if (!control) return 0;
	const auto found = std::find_if(
		_nodes.begin(), _nodes.end(),
		[control](const Node& node) { return node.Element == control; });
	return found == _nodes.end()
		? static_cast<int>(_nodes.size()) : found->Order;
}
