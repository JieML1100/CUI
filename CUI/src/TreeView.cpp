#include "TreeView.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

static float EffectiveItemHeight(TreeView* tree)
{
	const float fontHeight = (tree && tree->Font) ? tree->Font->FontHeight : 16.0f;
	const float configuredHeight = tree ? tree->ItemHeight : 28.0f;
	return (std::max)(configuredHeight, fontHeight + 8.0f);
}

static D2D1_POINT_2F RotateAround(const D2D1_POINT_2F& point, float cx, float cy, float angle)
{
	const float dx = point.x - cx;
	const float dy = point.y - cy;
	const float s = std::sin(angle);
	const float c = std::cos(angle);
	return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
}

static void DrawChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
{
	if (!d2d) return;
	progress = (std::clamp)(progress, 0.0f, 1.0f);
	const float angle = progress * 1.57079632679f;
	const float halfW = size * 0.28f;
	const float halfH = size * 0.46f;
	D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
	D2D1_POINT_2F p2 = D2D1::Point2F(cx + halfW, cy);
	D2D1_POINT_2F p3 = D2D1::Point2F(cx - halfW, cy + halfH);
	p1 = RotateAround(p1, cx, cy, angle);
	p2 = RotateAround(p2, cx, cy, angle);
	p3 = RotateAround(p3, cx, cy, angle);
	d2d->DrawLine(p1, p2, color, 1.8f);
	d2d->DrawLine(p2, p3, color, 1.8f);
}

static float measureNodes(std::vector<TreeNode*>& children)
{
	float count = 0.0f;
	for (auto* child : children)
	{
		if (child) count += child->AnimatedVisibleCount();
	}
	return count;
}

static void renderNodes(TreeView* tree, D2DGraphics* d2d, float w, float h, float itemHeight, float scrollOffsetY, float& cursorY, int sunLevel, std::vector<TreeNode*>& children)
{
	if (!tree || !d2d) return;
	const float fontHeight = tree->Font ? tree->Font->FontHeight : 16.0f;
	const float rowInsetX = (std::max)(0.0f, tree->ItemHorizontalPadding);
	const float rowInsetY = (std::max)(0.0f, tree->ItemVerticalPadding);
	const float chevronSize = (std::max)(6.0f, tree->ChevronSize);
	const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
	const float textSpacing = (std::max)(2.0f, tree->TextLeftSpacing);
	for (auto* c : children)
	{
		if (!c) continue;
		const float renderTop = cursorY - scrollOffsetY;
		const float renderBottom = renderTop + itemHeight;
		const float baseLeft = rowInsetX + (sunLevel * (std::max)(8.0f, tree->IndentWidth));

		if (renderBottom >= 0.0f && renderTop < h)
		{
			auto foreColor = (c == tree->SelectedNode) ? tree->SelectedForeColor : tree->ForeColor;
			const float pillRight = (std::max)(rowInsetX + 1.0f, w - rowInsetX - 10.0f);
			const D2D1_RECT_F itemRect = D2D1::RectF(rowInsetX, renderTop + rowInsetY, pillRight, renderBottom - rowInsetY);
			if (c == tree->SelectedNode)
			{
				d2d->FillRoundRect(itemRect, tree->SelectedBackColor, tree->ItemCornerRadius);
				const float accentW = (std::max)(2.0f, tree->SelectedAccentWidth);
				const float accentTop = itemRect.top + 5.0f;
				const float accentH = (std::max)(6.0f, (itemRect.bottom - itemRect.top) - 10.0f);
				d2d->FillRoundRect(itemRect.left, accentTop, accentW, accentH, tree->AccentColor, accentW * 0.5f);
			}
			else if (c == tree->HoveredNode)
			{
				d2d->FillRoundRect(itemRect, tree->UnderMouseItemBackColor, tree->ItemCornerRadius);
			}
			if (c->Children.size() > 0)
			{
				const float chevronCx = baseLeft + chevronSlot * 0.5f;
				const float chevronCy = renderTop + itemHeight * 0.5f;
				DrawChevron(d2d, chevronCx, chevronCy, chevronSize, c->CurrentExpandProgress(), foreColor);
			}

			float contentLeft = baseLeft + chevronSlot + textSpacing;
			if (auto* bmp = c->GetImageBitmap(d2d))
			{
				const float imageSize = (std::max)(12.0f, (std::min)(18.0f, itemHeight - 8.0f));
				const float imageTop = renderTop + (itemHeight - imageSize) * 0.5f;
				d2d->DrawBitmap(bmp, contentLeft, imageTop, imageSize, imageSize);
				contentLeft += imageSize + textSpacing;
			}
			const float textTop = renderTop + (std::max)(0.0f, (itemHeight - fontHeight) * 0.5f);
			d2d->DrawString(c->Text, contentLeft, textTop, foreColor, tree->Font);
		}

		cursorY += itemHeight;

		if (c->Children.size() > 0)
		{
			const float progress = c->CurrentExpandProgress();
			if (progress > 0.001f)
			{
				const float childFullHeight = measureNodes(c->Children) * itemHeight;
				const float childVisibleHeight = childFullHeight * progress;
				if (childVisibleHeight > 0.001f)
				{
					const float clipTop = cursorY - scrollOffsetY;
					const float clipBottom = clipTop + childVisibleHeight;
					if (clipBottom > 0.0f && clipTop < h)
					{
						const float clipY = (std::max)(0.0f, clipTop);
						const float clipH = (std::min)(h, clipBottom) - clipY;
						if (clipH > 0.0f)
						{
							float childCursor = cursorY;
							d2d->PushDrawRect(0.0f, clipY, w, clipH);
							renderNodes(tree, d2d, w, h, itemHeight, scrollOffsetY, childCursor, sunLevel + 1, c->Children);
							d2d->PopDrawRect();
						}
					}
					cursorY += childVisibleHeight;
				}
			}
		}
	}
}

static TreeNode* findNode(TreeView* tree, float posX, float posY, float h, float itemHeight, float scrollOffsetY, float& cursorY, int sunLevel, std::vector<TreeNode*>& children, bool& isHitEx)
{
	if (!tree) return nullptr;
	const float rowInsetX = (std::max)(0.0f, tree->ItemHorizontalPadding);
	const float chevronSize = (std::max)(6.0f, tree->ChevronSize);
	const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
	for (auto* c : children)
	{
		if (!c) continue;
		const float currTop = cursorY - scrollOffsetY;
		const float currBottom = currTop + itemHeight;
		if (currBottom >= 0.0f && currTop < h)
		{
			if (posY >= currTop && posY <= currBottom)
			{
				float exLeft = rowInsetX + (sunLevel * (std::max)(8.0f, tree->IndentWidth));
				if (posX >= (exLeft - 3.0f) && posX <= (exLeft + chevronSlot + 3.0f) && c->Children.size() > 0)
					isHitEx = true;
				else
					isHitEx = false;
				return c;
			}
		}
		cursorY += itemHeight;
		if (c->Children.size() > 0)
		{
			const float progress = c->CurrentExpandProgress();
			if (progress > 0.001f)
			{
				const float childVisibleHeight = measureNodes(c->Children) * itemHeight * progress;
				const float childTop = cursorY - scrollOffsetY;
				const float childBottom = childTop + childVisibleHeight;
				if (posY >= childTop && posY <= childBottom)
				{
					float childCursor = cursorY;
					auto result = findNode(tree, posX, posY, h, itemHeight, scrollOffsetY, childCursor, sunLevel + 1, c->Children, isHitEx);
					if (result)
						return result;
				}
				cursorY += childVisibleHeight;
			}
		}
	}

	return nullptr;
}

static void CollectVisibleTreeNodes(
	const std::vector<TreeNode*>& nodes, int level,
	std::vector<std::pair<TreeNode*, int>>& result)
{
	for (auto* node : nodes)
	{
		if (!node) continue;
		result.emplace_back(node, level);
		if (node->Expand)
			CollectVisibleTreeNodes(node->Children, level + 1, result);
	}
}

static size_t CountVisibleTreeNodes(
	const std::vector<TreeNode*>& nodes) noexcept
{
	size_t count = 0;
	for (auto* node : nodes)
	{
		if (!node) continue;
		++count;
		if (node->Expand)
			count += CountVisibleTreeNodes(node->Children);
	}
	return count;
}

TreeNode::TreeNode(std::wstring text, std::shared_ptr<BitmapSource> image)
{
	this->AccessibilityId = AllocateAccessibilityVirtualId();
	this->Text = text;
	this->Image = std::move(image);
	this->Expand = false;
	this->ExpandProgress = 0.0f;
	this->Children.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnChildrenChanged(change); });
}

static bool TreeSubtreeContains(
	const TreeNode* root, const TreeNode* candidate)
{
	if (!root || !candidate) return false;
	if (root == candidate) return true;
	for (auto* child : root->Children)
	{
		if (TreeSubtreeContains(child, candidate)) return true;
	}
	return false;
}

bool TreeNode::CanAdopt(const TreeNode* child) const
{
	if (!child || child == this || TreeSubtreeContains(child, this))
		return false;
	if (child->_parentNode && child->_parentNode != this)
		return false;
	return std::find(Children.begin(), Children.end(), child) == Children.end();
}

void TreeNode::AttachOwner(TreeView* owner)
{
	_ownerTree = owner;
	for (auto* child : Children)
	{
		if (!child) continue;
		child->_parentNode = this;
		child->AttachOwner(owner);
	}
	_observedChildren.assign(Children.begin(), Children.end());
}

void TreeNode::OnChildrenChanged(const CollectionChangedEventArgs& change)
{
	for (auto* child : _observedChildren)
	{
		if (!child || std::find(Children.begin(), Children.end(), child)
			!= Children.end()) continue;
		if (child->_parentNode == this) child->_parentNode = nullptr;
		child->AttachOwner(nullptr);
	}
	for (auto* child : Children)
	{
		if (!child) continue;
		child->_parentNode = this;
		child->AttachOwner(_ownerTree);
	}
	_observedChildren.assign(Children.begin(), Children.end());
	if (_ownerTree) _ownerTree->OnNodeChildrenChanged(this, change);
}

TreeNode* TreeNode::AddChild(TreeNode* child)
{
	if (!CanAdopt(child)) return nullptr;
	Children.push_back(child);
	return child;
}

TreeNode* TreeNode::AddChild(std::unique_ptr<TreeNode> child)
{
	if (!child || !CanAdopt(child.get())) return nullptr;
	auto* result = child.get();
	Children.push_back(result);
	child.release();
	return result;
}

std::unique_ptr<TreeNode> TreeNode::DetachChildAt(size_t index)
{
	if (index >= Children.size()) return {};
	auto* child = Children[index];
	Children.erase(Children.begin() + static_cast<ptrdiff_t>(index));
	return std::unique_ptr<TreeNode>(child);
}

bool TreeNode::RemoveChild(TreeNode* child)
{
	const auto found = std::find(Children.begin(), Children.end(), child);
	if (found == Children.end()) return false;
	return RemoveChildAt(static_cast<size_t>(found - Children.begin()));
}

bool TreeNode::RemoveChildAt(size_t index)
{
	auto child = DetachChildAt(index);
	return child != nullptr;
}

void TreeNode::ClearChildren()
{
	if (Children.empty()) return;
	std::vector<TreeNode*> removed(Children.begin(), Children.end());
	Children.clear();
	std::unordered_set<TreeNode*> deleted;
	for (auto* child : removed)
	{
		if (child && deleted.insert(child).second) delete child;
	}
}

float TreeNode::CurrentExpandProgress()
{
	if (this->Children.size() <= 0)
	{
		this->Animating = false;
		this->ExpandProgress = 0.0f;
		return 0.0f;
	}
	if (!this->Animating)
	{
		this->ExpandProgress = this->Expand ? 1.0f : 0.0f;
		return this->ExpandProgress;
	}
	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= this->AnimStartTick ? (now - this->AnimStartTick) : 0;
	float t = this->AnimDurationMs > 0 ? (float)elapsed / (float)this->AnimDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		this->ExpandProgress = this->AnimTargetProgress;
		this->Animating = false;
		return this->ExpandProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	this->ExpandProgress = this->AnimStartProgress + (this->AnimTargetProgress - this->AnimStartProgress) * t;
	return this->ExpandProgress;
}

void TreeNode::SetExpanded(bool expanded, bool animate)
{
	const bool wantExpand = expanded && this->Children.size() > 0;
	const float current = CurrentExpandProgress();
	const bool semanticChanged = this->Expand != wantExpand;
	this->Expand = wantExpand;
	if (semanticChanged && _ownerTree)
		_ownerTree->InvalidateAccessibilityIndex(false);
	this->AnimStartProgress = current;
	this->AnimTargetProgress = wantExpand ? 1.0f : 0.0f;
	if (!animate || this->AnimDurationMs == 0
		|| std::fabs(this->AnimTargetProgress - this->AnimStartProgress) < 0.001f)
	{
		this->ExpandProgress = this->AnimTargetProgress;
		this->Animating = false;
	}
	else
	{
		this->AnimStartTick = ::GetTickCount64();
		this->Animating = true;
	}
}

bool TreeNode::IsAnimationRunning()
{
	CurrentExpandProgress();
	if (this->Animating) return true;
	for (auto* child : this->Children)
	{
		if (child && child->IsAnimationRunning()) return true;
	}
	return false;
}

float TreeNode::AnimatedVisibleCount()
{
	float count = 1.0f;
	if (this->Children.size() > 0)
	{
		float childCount = 0.0f;
		for (auto* child : this->Children)
		{
			if (child) childCount += child->AnimatedVisibleCount();
		}
		count += CurrentExpandProgress() * childCount;
	}
	return count;
}

ID2D1Bitmap* TreeNode::GetImageBitmap(D2DGraphics* render)
{
	if (!render || !Image)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (ImageCache && ImageCacheTarget == target && ImageCacheSource == Image.get())
		return ImageCache.Get();
	ImageCache.Reset();
	ImageCacheTarget = target;
	ImageCacheSource = Image.get();
	auto* bmp = render->CreateBitmap(Image);
	if (!bmp)
		return nullptr;
	ImageCache.Attach(bmp);
	return ImageCache.Get();
}
TreeNode::~TreeNode()
{
	Children.SetOwnerChangedHandler({});
	_ownerTree = nullptr;
	std::unordered_set<TreeNode*> deleted;
	for (auto* child : Children)
	{
		if (!child || !deleted.insert(child).second) continue;
		if (child->_parentNode == this) child->_parentNode = nullptr;
		child->AttachOwner(nullptr);
		delete child;
	}
	static_cast<TreeNode::ChildCollection::Base&>(Children).clear();
	_observedChildren.clear();
}
int TreeNode::UnfoldedCount()
{
	int count = 1;
	for (auto& c : this->Children)
	{
		if (c->Expand)
			count += c->UnfoldedCount();
	}
	return count;
}
UIClass TreeView::Type() { return UIClass::UI_TreeView; }

bool TreeView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	const float itemHeight = EffectiveItemHeight(this);
	const int renderItemCount = itemHeight > 0.0f ? std::max(1, (int)((float)this->Height / itemHeight)) : 1;
	int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
	if (maxScroll < 0) maxScroll = 0;
	if (maxScroll <= 0) return false;
	return delta > 0
		? this->ScrollIndex > 0
		: this->ScrollIndex < maxScroll;
}

CursorKind TreeView::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->Enable) return CursorKind::Arrow;

	const float itemHeight = EffectiveItemHeight(this);
	if (itemHeight > 0.0f)
	{
		const int renderCount = std::max(1, (int)((float)this->Height / itemHeight));
		const bool hasVScroll = (_contentRenderItems > (float)renderCount + 0.001f);
		if (hasVScroll && localX >= (this->Width - 8))
			return CursorKind::SizeNS;
	}
	return CursorKind::Arrow;
}

TreeView::TreeView(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->Root = new TreeNode(L"");
	this->Root->AttachOwner(this);
	this->SelectedNode = nullptr;
}
TreeView::~TreeView()
{
	if (this->Root) this->Root->AttachOwner(nullptr);
	delete this->Root;
}

static bool ContainsTreeNode(
	const std::vector<TreeNode*>& nodes, const TreeNode* candidate)
{
	if (!candidate) return false;
	for (auto* node : nodes)
	{
		if (!node) continue;
		if (node == candidate || ContainsTreeNode(node->Children, candidate))
			return true;
	}
	return false;
}

void TreeView::InvalidateAccessibilityIndex(bool structure) noexcept
{
	if (structure) _accessibilityIndexDirty = true;
	_accessibilityVisibleDirty = true;
	_accessibilityVisibleCount = Root
		? CountVisibleTreeNodes(Root->Children) : 0;
}

void TreeView::EnsureAccessibilityIndex()
{
	if (!_accessibilityIndexDirty) return;
	_accessibilityNodeIndex.clear();
	_accessibilityChildrenByParentId.clear();
	if (!Root)
	{
		_accessibilityIndexDirty = false;
		_accessibilityVisibleDirty = true;
		return;
	}
	std::unordered_set<uint32_t> used;
	auto build = [&](const std::vector<TreeNode*>& nodes,
		TreeNode* parent, int level, auto&& self) -> void
	{
		const uint32_t parentId = parent ? parent->AccessibilityId : 0;
		auto& indexedChildren = _accessibilityChildrenByParentId[parentId];
		indexedChildren.reserve(nodes.size());
		for (auto* node : nodes)
		{
			if (!node) continue;
			uint32_t id = node->AccessibilityId;
			while (id == 0 || !used.insert(id).second)
				id = AllocateAccessibilityVirtualId();
			node->AccessibilityId = id;
			const size_t siblingIndex = indexedChildren.size();
			indexedChildren.push_back(node);
			_accessibilityNodeIndex.emplace(id,
				AccessibilityNodeIndex{ node, parent, siblingIndex, level });
			self(node->Children, node, level + 1, self);
		}
	};
	build(Root->Children, nullptr, 1, build);
	_accessibilityIndexDirty = false;
	_accessibilityVisibleDirty = true;
}

void TreeView::EnsureAccessibilityVisibleIndex()
{
	EnsureAccessibilityIndex();
	if (!_accessibilityVisibleDirty) return;
	_accessibilityVisibleNodes.clear();
	_accessibilityVisibleIndex.clear();
	if (Root)
		CollectVisibleTreeNodes(Root->Children, 1, _accessibilityVisibleNodes);
	_accessibilityVisibleIndex.reserve(_accessibilityVisibleNodes.size());
	for (size_t index = 0; index < _accessibilityVisibleNodes.size(); ++index)
		_accessibilityVisibleIndex.emplace(
			_accessibilityVisibleNodes[index].first, index);
	_accessibilityVisibleCount = _accessibilityVisibleNodes.size();
	_accessibilityVisibleDirty = false;
}

void TreeView::OnNodeChildrenChanged(
	TreeNode* parent, const CollectionChangedEventArgs& change)
{
	(void)change;
	if (!Root) return;
	InvalidateAccessibilityIndex(true);
	if (parent && parent->Children.empty())
		parent->SetExpanded(false, false);

	EnsureAccessibilityIndex();
	if (SelectedNode && !ContainsTreeNode(Root->Children, SelectedNode))
	{
		const uint32_t removedId = SelectedNode->AccessibilityId;
		SelectedNode = nullptr;
		SelectionChanged(this);
		if (removedId != 0)
			NotifyAccessibilityVirtualChanged(
				removedId, AccessibilityChange::Selection);
	}
	if (HoveredNode && !ContainsTreeNode(Root->Children, HoveredNode))
		HoveredNode = nullptr;

	EnsureAccessibilityVisibleIndex();
	_contentRenderItems = static_cast<float>(_accessibilityVisibleNodes.size());
	MaxRenderItems = static_cast<int>(_accessibilityVisibleNodes.size());
	const float itemHeight = EffectiveItemHeight(this);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(Height) / itemHeight)))
		: 1;
	const int maximum = (std::max)(0, MaxRenderItems - page);
	const int nextScroll = (std::clamp)(ScrollIndex, 0, maximum);
	if (nextScroll != ScrollIndex)
	{
		ScrollIndex = nextScroll;
		ScrollChanged(this);
	}
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
}

void TreeView::GetAccessibilityVirtualChildren(
	uint32_t parentId, std::vector<uint32_t>& result)
{
	result.clear();
	if (!Root) return;
	EnsureAccessibilityIndex();
	const auto children = _accessibilityChildrenByParentId.find(parentId);
	if (children == _accessibilityChildrenByParentId.end()) return;
	result.reserve(children->second.size());
	for (auto* child : children->second)
		result.push_back(child->AccessibilityId);
}

size_t TreeView::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	EnsureAccessibilityIndex();
	const auto children = _accessibilityChildrenByParentId.find(parentId);
	return children == _accessibilityChildrenByParentId.end()
		? 0 : children->second.size();
}

bool TreeView::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	EnsureAccessibilityIndex();
	const auto children = _accessibilityChildrenByParentId.find(parentId);
	if (children == _accessibilityChildrenByParentId.end()
		|| index >= children->second.size()) return false;
	result = children->second[index]->AccessibilityId;
	return result != 0;
}

bool TreeView::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	result = 0;
	EnsureAccessibilityIndex();
	const auto node = _accessibilityNodeIndex.find(id);
	if (node == _accessibilityNodeIndex.end()) return false;
	const uint32_t actualParentId = node->second.Parent
		? node->second.Parent->AccessibilityId : 0;
	if (actualParentId != parentId) return false;
	const size_t index = node->second.SiblingIndex;
	if (!next && index == 0) return false;
	const size_t sibling = next ? index + 1 : index - 1;
	return TryGetAccessibilityVirtualChildAt(parentId, sibling, result);
}

bool TreeView::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	const auto size = GetActualSizeDip();
	if (localX < 0.0f || localY < 0.0f
		|| localX >= size.width || localY >= size.height) return false;
	const float itemHeight = EffectiveItemHeight(this);
	if (itemHeight <= 0.0f) return false;
	EnsureAccessibilityVisibleIndex();
	const int index = ScrollIndex
		+ static_cast<int>(std::floor(localY / itemHeight));
	if (index < 0
		|| index >= static_cast<int>(_accessibilityVisibleNodes.size())) return false;
	result = _accessibilityVisibleNodes[static_cast<size_t>(index)]
		.first->AccessibilityId;
	return result != 0;
}

bool TreeView::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	if (!Root || id == 0) return false;
	EnsureAccessibilityVisibleIndex();
	const auto indexed = _accessibilityNodeIndex.find(id);
	if (indexed == _accessibilityNodeIndex.end()) return false;
	auto* node = indexed->second.Node;
	const auto position = _accessibilityVisibleIndex.find(node);
	const bool realized = position != _accessibilityVisibleIndex.end();
	const float itemHeight = EffectiveItemHeight(this);
	const float top = realized
		? static_cast<float>(static_cast<int>(position->second) - ScrollIndex)
			* itemHeight
		: 0.0f;
	result = {};
	result.Id = id;
	result.ParentId = indexed->second.Parent
		? indexed->second.Parent->AccessibilityId : 0;
	result.Role = AccessibleRole::TreeItem;
	result.Patterns = AccessibilityVirtualPattern::SelectionItem
		| AccessibilityVirtualPattern::ScrollItem
		| AccessibilityVirtualPattern::VirtualizedItem;
	if (!node->Children.empty())
		result.Patterns |= AccessibilityVirtualPattern::ExpandCollapse;
	result.Name = node->Text;
	const auto ownerId = GetAccessibilitySnapshot().AutomationId;
	result.AutomationId = ownerId.empty()
		? L"node-" + std::to_wstring(id)
		: ownerId + L".node-" + std::to_wstring(id);
	result.BoundsDip = realized
		? D2D1::RectF(0.0f, top, static_cast<float>(Width), top + itemHeight)
		: D2D1::RectF(0, 0, 0, 0);
	result.Enabled = Enable;
	result.Visible = Visible && realized && top < static_cast<float>(Height)
		&& top + itemHeight > 0.0f;
	result.Selected = SelectedNode == node;
	result.Expanded = node->Expand;
	result.Row = realized ? static_cast<int>(position->second) : -1;
	result.Column = 0;
	result.Level = indexed->second.Level;
	return true;
}

AccessibilityVirtualContainerInfo
TreeView::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AccessibilityVirtualPattern::Selection
		| AccessibilityVirtualPattern::Scroll;
	result.CanSelectMultiple = false;
	result.IsSelectionRequired = false;
	return result;
}

void TreeView::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
	if (!Root || !SelectedNode) return;
	EnsureAccessibilityIndex();
	if (_accessibilityNodeIndex.contains(SelectedNode->AccessibilityId))
		result.push_back(SelectedNode->AccessibilityId);
}

bool TreeView::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	if (!Root || !Enable) return false;
	EnsureAccessibilityIndex();
	const auto indexed = _accessibilityNodeIndex.find(id);
	if (indexed == _accessibilityNodeIndex.end()) return false;
	TreeNode* next = action == AccessibilitySelectionAction::Remove
		? nullptr : indexed->second.Node;
	if (SelectedNode != next)
	{
		SelectedNode = next;
		SelectionChanged(this);
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
		InvalidateVisual();
	}
	return true;
}

bool TreeView::SetAccessibilityVirtualNodeExpanded(
	uint32_t id, bool expanded)
{
	if (!Root || !Enable) return false;
	EnsureAccessibilityIndex();
	const auto indexed = _accessibilityNodeIndex.find(id);
	if (indexed == _accessibilityNodeIndex.end()
		|| indexed->second.Node->Children.empty()) return false;
	indexed->second.Node->SetExpanded(expanded, AreSystemAnimationsEnabled());
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::ExpandCollapse);
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
	return true;
}

bool TreeView::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	if (!Root || !Enable) return false;
	EnsureAccessibilityIndex();
	const auto indexed = _accessibilityNodeIndex.find(id);
	if (indexed == _accessibilityNodeIndex.end()) return false;
	for (auto* parent = indexed->second.Parent; parent; parent = parent->_parentNode)
		parent->SetExpanded(true, false);
	EnsureAccessibilityVisibleIndex();
	const auto position = _accessibilityVisibleIndex.find(indexed->second.Node);
	if (position == _accessibilityVisibleIndex.end()) return false;
	const int target = static_cast<int>(position->second);
	const float itemHeight = EffectiveItemHeight(this);
	const int page = (std::max)(1,
		static_cast<int>(std::floor(static_cast<float>(Height) / itemHeight)));
	int nextScroll = ScrollIndex;
	if (target < nextScroll) nextScroll = target;
	else if (target >= nextScroll + page) nextScroll = target - page + 1;
	const int maximum = (std::max)(0,
		static_cast<int>(_accessibilityVisibleNodes.size()) - page);
	nextScroll = (std::clamp)(nextScroll, 0, maximum);
	if (nextScroll != ScrollIndex)
	{
		ScrollIndex = nextScroll;
		ScrollChanged(this);
	}
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
	return true;
}

bool TreeView::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	if (!Root) return true;
	const float fontHeight = _font ? _font->FontHeight : 16.0f;
	const float itemHeight = (std::max)(ItemHeight, fontHeight + 8.0f);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(_size.cy) / itemHeight))) : 1;
	const size_t visibleCount = _accessibilityVisibleCount;
	const int count = visibleCount > static_cast<size_t>((std::numeric_limits<int>::max)())
		? (std::numeric_limits<int>::max)() : static_cast<int>(visibleCount);
	const int maximum = (std::max)(0, count - page);
	result.VerticallyScrollable = maximum > 0;
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent = (std::clamp)(
			static_cast<double>(ScrollIndex) / maximum * 100.0, 0.0, 100.0);
		result.VerticalViewSize = count > 0
			? (std::clamp)(static_cast<double>(page) / count * 100.0, 0.0, 100.0)
			: 100.0;
	}
	return true;
}

bool TreeView::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	if (horizontal != AccessibilityScrollAmount::NoAmount) return false;
	if (vertical == AccessibilityScrollAmount::NoAmount) return true;
	if (!Root) return false;
	EnsureAccessibilityVisibleIndex();
	const float itemHeight = EffectiveItemHeight(this);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(Height) / itemHeight))) : 1;
	const int maximum = (std::max)(0,
		static_cast<int>(_accessibilityVisibleNodes.size()) - page);
	if (maximum <= 0) return false;
	int delta = 0;
	switch (vertical)
	{
	case AccessibilityScrollAmount::LargeDecrement: delta = -page; break;
	case AccessibilityScrollAmount::SmallDecrement: delta = -1; break;
	case AccessibilityScrollAmount::LargeIncrement: delta = page; break;
	case AccessibilityScrollAmount::SmallIncrement: delta = 1; break;
	case AccessibilityScrollAmount::NoAmount: return true;
	}
	const int next = (std::clamp)(ScrollIndex + delta, 0, maximum);
	if (next != ScrollIndex)
	{
		ScrollIndex = next;
		ScrollChanged(this);
		NotifyAccessibilityScrollChanged();
		InvalidateVisual();
	}
	return true;
}

bool TreeView::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	if (horizontalPercent != AccessibilityScrollNoChange) return false;
	if (verticalPercent == AccessibilityScrollNoChange) return true;
	if (!std::isfinite(verticalPercent)
		|| verticalPercent < 0.0 || verticalPercent > 100.0 || !Root) return false;
	EnsureAccessibilityVisibleIndex();
	const float itemHeight = EffectiveItemHeight(this);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(Height) / itemHeight))) : 1;
	const int maximum = (std::max)(0,
		static_cast<int>(_accessibilityVisibleNodes.size()) - page);
	if (maximum <= 0) return false;
	const int next = (std::clamp)(static_cast<int>(std::lround(
		maximum * verticalPercent / 100.0)), 0, maximum);
	if (next != ScrollIndex)
	{
		ScrollIndex = next;
		ScrollChanged(this);
		NotifyAccessibilityScrollChanged();
		InvalidateVisual();
	}
	return true;
}

void TreeView::OnComputedLayoutSizeChanged()
{
	if (!Root)
	{
		NotifyAccessibilityScrollChanged();
		return;
	}
	EnsureAccessibilityVisibleIndex();
	const float itemHeight = EffectiveItemHeight(this);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(Height) / itemHeight))) : 1;
	const int maximum = (std::max)(0,
		static_cast<int>(_accessibilityVisibleNodes.size()) - page);
	const int next = (std::clamp)(ScrollIndex, 0, maximum);
	if (next != ScrollIndex)
	{
		ScrollIndex = next;
		ScrollChanged(this);
	}
	NotifyAccessibilityScrollChanged();
}

bool TreeView::IsAnimationRunning()
{
	if (!AreSystemAnimationsEnabled() && this->Root)
	{
		auto finish = [](TreeNode* node, const auto& self) -> void
		{
			if (!node) return;
			node->SetExpanded(node->Expand, false);
			for (auto* child : node->Children) self(child, self);
		};
		finish(this->Root, finish);
		return false;
	}
	return this->Root && this->Root->IsAnimationRunning();
}

bool TreeView::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = this->AbsRect;
	return true;
}

void TreeView::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	if (_contentRenderItems <= 0.0f) return;

	const float itemHeight = EffectiveItemHeight(this);
	if (itemHeight <= 0.0f) return;

	const float height = (float)this->Height;
	int renderItemCount = (int)(height / itemHeight);
	if (renderItemCount <= 0) renderItemCount = 1;

	if ((float)renderItemCount >= _contentRenderItems)
	{
		if (this->ScrollIndex != 0)
		{
			this->ScrollIndex = 0;
			this->ScrollChanged(this);
			this->NotifyAccessibilityScrollChanged();
			InvalidateVisual();
		}
		return;
	}

	int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
	if (maxScroll < 0) maxScroll = 0;

	float scrollBlockHeight = (renderItemCount / _contentRenderItems) * height;
	if (scrollBlockHeight < height * 0.1f) scrollBlockHeight = height * 0.1f;
	if (scrollBlockHeight > height) scrollBlockHeight = height;

	const float scrollHeight = height - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - grab;
	float per = targetTop / scrollHeight;
	if (per < 0.0f) per = 0.0f;
	if (per > 1.0f) per = 1.0f;

	int newScroll = (int)(per * (float)maxScroll);
	if (newScroll < 0) newScroll = 0;
	if (newScroll > maxScroll) newScroll = maxScroll;

	if (this->ScrollIndex != newScroll)
	{
		this->ScrollIndex = newScroll;
		this->ScrollChanged(this);
		this->NotifyAccessibilityScrollChanged();
		InvalidateVisual();
	}
}
void TreeView::DrawScroll() {
	float width = this->Width - 8.0f;
	float height = static_cast<float>(this->Height);
	float itemHeight = EffectiveItemHeight(this);
	if (_contentRenderItems > 0.0f) {
		int renderItemCount = static_cast<int>(height / itemHeight);
		if ((float)renderItemCount < _contentRenderItems) {
			int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
			float scrollBlockHeight = (renderItemCount / _contentRenderItems) * height;
			if (scrollBlockHeight < height * 0.1f) scrollBlockHeight = height * 0.1f;
			if (scrollBlockHeight > height) scrollBlockHeight = height;
			float scrollPer = (float)this->ScrollIndex / (float)maxScroll;
			float scrollBlockTop = scrollPer * (height - scrollBlockHeight);
			this->ParentForm->Render->FillRoundRect(width, 0, 8.0f, height, this->ScrollBackColor, 4.0f);
			this->ParentForm->Render->FillRoundRect(width, scrollBlockTop, 8.0f, scrollBlockHeight, this->ScrollForeColor, 4.0f);
		}
	}
}
void TreeView::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	bool isSelected = this->ParentForm->Selected == this;
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}

		{
			const float itemHeight = EffectiveItemHeight(this);
			_contentRenderItems = measureNodes(this->Root->Children);
			this->MaxRenderItems = (int)std::ceil(_contentRenderItems);
			int maxScroll = (int)std::ceil(_contentRenderItems - ((float)this->Height / itemHeight));
			if (maxScroll < 0)maxScroll = 0;
			if (this->ScrollIndex > maxScroll)
			{
				this->ScrollIndex = maxScroll;
				this->ScrollChanged(this);
				this->NotifyAccessibilityScrollChanged();
			}
			float cursorY = 0.0f;
			renderNodes(this, d2d, actualWidth, actualHeight, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children);
			this->DrawScroll();
			d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BorderColor);

		}
		if (!this->Enable)
		{
			d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
bool TreeView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT i = 0; i < fileCount; i++)
		{
			DragQueryFile(hDropInfo, i, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		const float itemHeight = EffectiveItemHeight(this);
		const int renderItemCount = itemHeight > 0.0f ? std::max(1, (int)((float)this->Height / itemHeight)) : 1;
		int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
		if (maxScroll < 0) maxScroll = 0;
		if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
		{
			if (this->ScrollIndex < maxScroll)
			{
				this->ScrollIndex += 1;
				this->ScrollChanged(this);
				this->NotifyAccessibilityScrollChanged();
				this->InvalidateVisual();
			}
		}
		else
		{
			if (this->ScrollIndex > 0)
			{
				this->ScrollIndex -= 1;
				this->ScrollChanged(this);
				this->NotifyAccessibilityScrollChanged();
				this->InvalidateVisual();
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (isDraggingScroll) {
			UpdateScrollDrag(static_cast<float>(localY));
		}
		else
		{
			if (localX < 0 || localX > this->Width || localY < 0 || localY > this->Height)
			{
				if (this->HoveredNode != nullptr)
				{
					this->HoveredNode = nullptr;
					this->InvalidateVisual();
				}
			}
			else
			{
				const auto size = this->GetActualSizeDip();
				const float itemHeight = EffectiveItemHeight(this);
				float cursorY = 0.0f;
				bool isHit = false;
				auto newHoveredNode = findNode(this, (float)localX, (float)localY, size.height, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children, isHit);
				bool needUpdate = this->HoveredNode != newHoveredNode;
				this->HoveredNode = newHoveredNode;
				if (needUpdate) this->InvalidateVisual();
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			if (this->ParentForm->Selected != this)
			{
				auto lse = this->ParentForm->Selected;
				this->ParentForm->Selected = this;
				if (lse) lse->InvalidateVisual();
			}
			if (localX >= Width - 8 && localX <= Width)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float itemHeight = EffectiveItemHeight(this);
				if (itemHeight > 0.0f && this->MaxRenderItems > 0)
				{
					const float height = (float)this->Height;
					int renderItemCount = (int)(height / itemHeight);
					if (renderItemCount <= 0) renderItemCount = 1;
					if (renderItemCount < this->MaxRenderItems)
					{
						int maxScroll = this->MaxRenderItems - renderItemCount;
						if (maxScroll < 0) maxScroll = 0;
						float thumbH = (renderItemCount / (float)this->MaxRenderItems) * height;
						if (thumbH < height * 0.1f) thumbH = height * 0.1f;
						if (thumbH > height) thumbH = height;
						const float moveSpace = std::max(0.0f, height - thumbH);
						float per = 0.0f;
						if (maxScroll > 0) per = std::clamp((float)this->ScrollIndex / (float)maxScroll, 0.0f, 1.0f);
						const float thumbTop = per * moveSpace;
						const float pointerY = (float)localY;
						const bool hitThumb = (pointerY >= thumbTop && pointerY <= (thumbTop + thumbH));
						_scrollThumbGrabOffsetY = hitThumb ? (pointerY - thumbTop) : (thumbH * 0.5f);
					}
					else
					{
						_scrollThumbGrabOffsetY = 0.0f;
					}
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)localY);
			}
			else
			{
				const auto size = this->GetActualSizeDip();
				const float itemHeight = EffectiveItemHeight(this);
				float cursorY = 0.0f;
				bool isHit = false;
				auto node = findNode(this, (float)localX, (float)localY, size.height, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children, isHit);
				if (node)
				{
					if (isHit)
					{
						node->SetExpanded(!node->Expand, AreSystemAnimationsEnabled());
						NotifyAccessibilityStructureChanged();
						NotifyAccessibilityScrollChanged();
					}
					else
					{
						bool isChanged = this->SelectedNode != node;
						this->SelectedNode = node;
						if (isChanged)this->SelectionChanged(this);
					}
				}
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (WM_LBUTTONUP == message)
		{
			if (isDraggingScroll) {
				isDraggingScroll = false;
			}
			if (this->ParentForm->Selected == this)
			{
				MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
				this->OnMouseClick(this, eventArgs);
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->Selected = this;
		const auto size = this->GetActualSizeDip();
		const float itemHeight = EffectiveItemHeight(this);
		float cursorY = 0.0f;
		bool isHit = false;
		auto node = findNode(this, (float)localX, (float)localY, size.height, itemHeight, (float)this->ScrollIndex * itemHeight, cursorY, 0, this->Root->Children, isHit);
		if (node)
		{
			if (!node->Children.empty())
			{
				node->SetExpanded(!node->Expand, AreSystemAnimationsEnabled());
				NotifyAccessibilityStructureChanged();
				NotifyAccessibilityScrollChanged();
			}
			if (!isHit)
			{
				bool isChanged = this->SelectedNode != node;
				this->SelectedNode = node;
				if (isChanged)this->SelectionChanged(this);
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_KEYDOWN:
	{
		if (this->ParentForm)
		{
			const auto pos = this->GetAbsoluteLocationDip();
			float caretH = (this->Font && this->Font->FontHeight > 0.0f) ? this->Font->FontHeight : 16.0f;
			this->ParentForm->SetImeCompositionWindowFromLogicalRect(
				D2D1_RECT_F{ (float)pos.x, (float)pos.y, (float)pos.x + 1.0f, (float)pos.y + caretH });
		}
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	}
	return true;
}
