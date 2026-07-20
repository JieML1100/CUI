#include "TreeView.h"
#include "Form.h"
#include "Core/Threading.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
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

namespace
{
	template<typename TValue>
	ControlPropertyOptions<TreeViewItem, TValue> TreeItemStateOptions(
		TValue defaultValue, int order, bool readOnly = true)
	{
		ControlPropertyOptions<TreeViewItem, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = ControlPropertyFlags::AffectsRender;
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = order;
		options.Design.Browsable = false;
		options.Design.Persistence = ControlPropertyPersistence::Transient;
		options.IsReadOnly = readOnly;
		return options;
	}
}

TreeViewItem::TreeViewItem()
	: HeaderedContentControl(0, 0, 0, 0)
{
	EnsureBindingPropertiesRegistered();
	HAlign = HorizontalAlignment::Stretch;
	VAlign = VerticalAlignment::Top;
	(void)TrySetPropertyValue(L"BorderThickness", BindingValue(0.0f),
		ControlPropertyValueSource::Theme);
}

void TreeViewItem::EnsureBindingPropertiesRegistered()
{
	HeaderedContentControl::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		BindingPropertyRegistry::Register<TreeViewItem, bool>(L"IsExpanded",
			[](TreeViewItem& target) { return target.GetIsExpanded(); },
			[](TreeViewItem& target, const bool& value)
			{ target.SetIsExpanded(value); },
			[](TreeViewItem& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._expandedChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(false, 10, false));
		BindingPropertyRegistry::Register<TreeViewItem, bool>(L"HasItems",
			[](TreeViewItem& target) { return target.GetHasItems(); }, {},
			[](TreeViewItem& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._hasItemsChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(false, 20));
		BindingPropertyRegistry::Register<TreeViewItem, int>(L"Level",
			[](TreeViewItem& target) { return target.GetLevel(); }, {},
			[](TreeViewItem& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._levelChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(0, 30));
		BindingPropertyRegistry::Register<TreeViewItem, bool>(L"IsSelected",
			[](TreeViewItem& target) { return target.GetIsSelected(); }, {},
			[](TreeViewItem& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(false, 40));
		BindingPropertyRegistry::Register<TreeViewItem, bool>(L"IsMouseOver",
			[](TreeViewItem& target) { return target.GetIsMouseOver(); }, {},
			[](TreeViewItem& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._mouseOverChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(false, 50));
		BindingPropertyRegistry::Register<TreeViewItem, bool>(
			L"IsKeyboardFocusWithin",
			[](TreeViewItem& target)
			{ return target.GetIsKeyboardFocusWithin(); }, {},
			[](TreeViewItem& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._keyboardFocusWithinChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(false, 60));
		return true;
	}();
	(void)registered;
}

bool TreeViewItem::Initialize(
	TreeView& owner, TreeNode& node, int level, std::wstring* outError)
{
	// A virtualized container may arrive from the recycle pool. Clear the old
	// data presentation before validating the next item, while retaining its
	// ControlTemplate chrome and allocated control object.
	SetHeader(BindingValue{});
	SetHeaderTemplate({});
	SetHeaderDisplayMemberPath({});
	SetHeaderTypeName({});
	(void)SetDataContext({});
	_owner = &owner;
	_node = &node;
	if (node._dataItem)
	{
		if (!SetDataContext(node._dataItem))
		{
			if (outError) *outError =
				L"TreeViewItem 无法继承对应数据项的 DataContext。";
			_owner = nullptr;
			_node = nullptr;
			return false;
		}
		SetHeaderTypeName(node._headerTemplate
			? node._headerTemplate.Get()->DataTypeName()
			: owner.GetItemsSource()
				? owner.GetItemsSource().Get()->ItemTypeName() : std::wstring{});
		SetHeaderDisplayMemberPath(owner.GetDisplayMemberPath());
		SetHeaderTemplate(node._headerTemplate);
		SetHeader(BindingValue(node._dataItem));
	}
	else SetHeader(BindingValue(node.Text));
	if (!LastHeaderError().empty())
	{
		if (outError) *outError = LastHeaderError();
		_owner = nullptr;
		_node = nullptr;
		return false;
	}
	SyncNodeState(level, owner.SelectedNode == &node,
		owner.HoveredNode == &node, false);
	if (outError) outError->clear();
	return true;
}

void TreeViewItem::ClearForRecycle()
{
	_owner = nullptr;
	_node = nullptr;
	SyncNodeState(0, false, false, false);
	SyncExpanded(false);
	SetHeader(BindingValue{});
	SetHeaderTemplate({});
	SetHeaderDisplayMemberPath({});
	SetHeaderTypeName({});
	(void)SetDataContext({});
	Parent = nullptr;
	Control::SetChildrenParentForm(this, nullptr);
}

void TreeViewItem::SetIsExpanded(bool value)
{
	if (_node)
	{
		_node->SetExpanded(value, AreSystemAnimationsEnabled());
		value = _node->Expand;
	}
	SyncExpanded(value);
}

void TreeViewItem::SyncExpanded(bool value)
{
	if (_expanded == value) return;
	(void)SetPropertyField(L"IsExpanded", _expanded, value);
	_expandedChanged(this);
	if (value) Expanded(this);
	else Collapsed(this);
}

void TreeViewItem::SyncHasItems(bool value)
{
	if (_hasItems == value) return;
	(void)SetPropertyField(L"HasItems", _hasItems, value);
	_hasItemsChanged(this);
}

void TreeViewItem::SyncNodeState(
	int level, bool selected, bool mouseOver, bool keyboardFocusWithin)
{
	if (_level != level)
	{
		(void)SetPropertyField(L"Level", _level, level);
		_levelChanged(this);
	}
	SyncHasItems(_node && _node->HasItems());
	SyncExpanded(_node && _node->Expand);
	if (_selected != selected)
	{
		(void)SetPropertyField(L"IsSelected", _selected, selected);
		SetStyleState(ControlStyleState::Selected, selected);
		_selectedChanged(this);
		if (selected) Selected(this);
		else Unselected(this);
	}
	if (_mouseOver != mouseOver)
	{
		(void)SetPropertyField(L"IsMouseOver", _mouseOver, mouseOver);
		SetStyleState(ControlStyleState::Hovered, mouseOver);
		_mouseOverChanged(this);
	}
	if (_keyboardFocusWithin != keyboardFocusWithin)
	{
		(void)SetPropertyField(L"IsKeyboardFocusWithin",
			_keyboardFocusWithin, keyboardFocusWithin);
		SetStyleState(ControlStyleState::Focused, keyboardFocusWithin);
		_keyboardFocusWithinChanged(this);
	}

	if (_owner)
	{
		const float chevronSize = (std::max)(6.0f, _owner->ChevronSize);
		const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
		const float imageSlot = _node && _node->Image
			? (std::max)(12.0f, (std::min)(18.0f,
				_owner->ItemHeight - 8.0f)) + _owner->TextLeftSpacing : 0.0f;
		(void)TrySetPropertyValue(L"Padding", BindingValue(Thickness(
			chevronSlot + _owner->TextLeftSpacing + imageSlot,
			_owner->ItemVerticalPadding,
			_owner->ItemHorizontalPadding,
			_owner->ItemVerticalPadding)), ControlPropertyValueSource::Theme);
		const auto back = selected ? _owner->SelectedBackColor
			: mouseOver ? _owner->UnderMouseItemBackColor
			: D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
		(void)TrySetPropertyValue(L"BackColor", BindingValue(back),
			ControlPropertyValueSource::Theme);
		(void)TrySetPropertyValue(L"ForeColor", BindingValue(
			selected ? _owner->SelectedForeColor : _owner->ForeColor),
			ControlPropertyValueSource::Theme);
	}
}

void TreeViewItem::Update()
{
	if (_node && !_node->_dataItem)
	{
		std::wstring header;
		if (!GetHeader().TryGetString(header) || header != _node->Text)
			SetHeader(BindingValue(_node->Text));
	}
	HeaderedContentControl::Update();
	if (!ParentForm || !_owner || !_node || GetControlTemplateRoot()) return;
	auto* render = ParentForm->Render;
	if (!render) return;
	const auto size = GetActualSizeDip();
	const float chevronSize = (std::max)(6.0f, _owner->ChevronSize);
	const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
	BeginRender();
	if (_hasItems)
		DrawChevron(render, chevronSlot * 0.5f, size.height * 0.5f,
			chevronSize, _node->CurrentExpandProgress(),
			_selected ? _owner->SelectedForeColor : _owner->ForeColor);
	if (_selected)
	{
		const float accentW = (std::max)(2.0f, _owner->SelectedAccentWidth);
		render->FillRoundRect(0.0f, 5.0f, accentW,
			(std::max)(6.0f, size.height - 10.0f),
			_owner->AccentColor, accentW * 0.5f);
	}
	if (auto* bitmap = _node->GetImageBitmap(render))
	{
		const float imageSize = (std::max)(12.0f,
			(std::min)(18.0f, size.height - 8.0f));
		render->DrawBitmap(bitmap, chevronSlot + _owner->TextLeftSpacing,
			(size.height - imageSize) * 0.5f, imageSize, imageSize);
	}
	EndRender();
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

static void renderNodeRow(TreeView* tree, D2DGraphics* d2d,
	float width, float itemHeight, float renderTop,
	int level, TreeNode* node)
{
	if (!tree || !d2d || !node) return;
	const float renderBottom = renderTop + itemHeight;
	const float fontHeight = tree->Font ? tree->Font->FontHeight : 16.0f;
	const float rowInsetX = (std::max)(0.0f, tree->ItemHorizontalPadding);
	const float rowInsetY = (std::max)(0.0f, tree->ItemVerticalPadding);
	const float chevronSize = (std::max)(6.0f, tree->ChevronSize);
	const float chevronSlot = (std::max)(16.0f, chevronSize + 6.0f);
	const float textSpacing = (std::max)(2.0f, tree->TextLeftSpacing);
	const float baseLeft = rowInsetX
		+ (level * (std::max)(8.0f, tree->IndentWidth));
	auto foreColor = node == tree->SelectedNode
		? tree->SelectedForeColor : tree->ForeColor;
	const float pillRight = (std::max)(
		rowInsetX + 1.0f, width - rowInsetX - 10.0f);
	const D2D1_RECT_F itemRect = D2D1::RectF(
		rowInsetX, renderTop + rowInsetY,
		pillRight, renderBottom - rowInsetY);
	auto* generatedContainer = tree->GetGeneratedItem(node);
	if (generatedContainer)
	{
		generatedContainer->Parent = tree;
		Control::SetChildrenParentForm(
			generatedContainer, tree->ParentForm);
		generatedContainer->ApplyLayout(cui::core::Rect{
			baseLeft, renderTop + rowInsetY,
			(std::max)(1.0f, pillRight - baseLeft),
			(std::max)(1.0f, itemHeight - rowInsetY * 2.0f) });
		generatedContainer->Update();
	}
	else if (node == tree->SelectedNode)
	{
		d2d->FillRoundRect(
			itemRect, tree->SelectedBackColor, tree->ItemCornerRadius);
		const float accentW = (std::max)(2.0f, tree->SelectedAccentWidth);
		const float accentTop = itemRect.top + 5.0f;
		const float accentH = (std::max)(
			6.0f, itemRect.bottom - itemRect.top - 10.0f);
		d2d->FillRoundRect(itemRect.left, accentTop,
			accentW, accentH, tree->AccentColor, accentW * 0.5f);
	}
	else if (node == tree->HoveredNode)
	{
		d2d->FillRoundRect(itemRect,
			tree->UnderMouseItemBackColor, tree->ItemCornerRadius);
	}
	if (!generatedContainer && node->HasItems())
	{
		DrawChevron(d2d, baseLeft + chevronSlot * 0.5f,
			renderTop + itemHeight * 0.5f, chevronSize,
			node->CurrentExpandProgress(), foreColor);
	}
	if (!generatedContainer)
	{
		float contentLeft = baseLeft + chevronSlot + textSpacing;
		if (auto* bitmap = node->GetImageBitmap(d2d))
		{
			const float imageSize = (std::max)(12.0f,
				(std::min)(18.0f, itemHeight - 8.0f));
			d2d->DrawBitmap(bitmap, contentLeft,
				renderTop + (itemHeight - imageSize) * 0.5f,
				imageSize, imageSize);
			contentLeft += imageSize + textSpacing;
		}
		d2d->DrawString(node->Text, contentLeft,
			renderTop + (std::max)(0.0f,
				(itemHeight - fontHeight) * 0.5f), foreColor, tree->Font);
	}
	if (node == tree->DropTargetNode
		&& tree->DropPosition == TreeViewDropPosition::Inside)
	{
		auto fill = tree->DropIndicatorColor;
		fill.a = (std::min)(fill.a, 0.16f);
		d2d->FillRoundRect(itemRect, fill, tree->ItemCornerRadius);
		d2d->DrawRoundRect(itemRect,
			tree->DropIndicatorColor, 1.5f, tree->ItemCornerRadius);
	}
	if (node == tree->DropTargetNode
		&& (tree->DropPosition == TreeViewDropPosition::Before
			|| tree->DropPosition == TreeViewDropPosition::After))
	{
		const float indicatorY = tree->DropPosition
			== TreeViewDropPosition::Before
			? renderTop + 1.0f : renderBottom - 1.0f;
		d2d->DrawLine(rowInsetX + 1.0f, indicatorY,
			pillRight, indicatorY, tree->DropIndicatorColor, 2.0f);
		d2d->FillRoundRect(rowInsetX, indicatorY - 2.5f,
			5.0f, 5.0f, tree->DropIndicatorColor, 2.5f);
	}
}

static void renderNodes(TreeView* tree, D2DGraphics* d2d, float w, float h, float itemHeight, float scrollOffsetY, float& cursorY, int sunLevel, std::vector<TreeNode*>& children)
{
	if (!tree || !d2d) return;
	for (auto* c : children)
	{
		if (!c) continue;
		const float renderTop = cursorY - scrollOffsetY;
		const float renderBottom = renderTop + itemHeight;
		if (renderBottom >= 0.0f && renderTop < h)
			renderNodeRow(tree, d2d, w, itemHeight, renderTop, sunLevel, c);

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

static TreeNode* findNode(TreeView* tree, float posX, float posY,
	float h, float itemHeight, float scrollOffsetY, float& cursorY,
	int sunLevel, std::vector<TreeNode*>& children, bool& isHitEx,
	float* relativeRowY = nullptr)
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
				if (relativeRowY)
					*relativeRowY = itemHeight > 0.0f
						? (std::clamp)((posY - currTop) / itemHeight, 0.0f, 1.0f)
						: 0.5f;
				float exLeft = rowInsetX + (sunLevel * (std::max)(8.0f, tree->IndentWidth));
				if (posX >= (exLeft - 3.0f) && posX <= (exLeft + chevronSlot + 3.0f) && c->HasItems())
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
					auto result = findNode(tree, posX, posY, h, itemHeight,
						scrollOffsetY, childCursor, sunLevel + 1,
						c->Children, isHitEx, relativeRowY);
					if (result)
						return result;
				}
				cursorY += childVisibleHeight;
			}
		}
	}

	return nullptr;
}

TreeNode* TreeView::HitTestNodeCore(
	float localX, float localY, float* relativeRowY, bool* hitExpander)
{
	if (relativeRowY) *relativeRowY = 0.5f;
	if (hitExpander) *hitExpander = false;
	if (!Root || localX < 0.0f || localY < 0.0f
		|| localX > static_cast<float>(Width)
		|| localY > static_cast<float>(Height))
		return nullptr;
	const auto size = GetActualSizeDip();
	const float itemHeight = EffectiveItemHeight(this);
	if (!_hasExpansionAnimation && itemHeight > 0.0f)
	{
		EnsureAccessibilityVisibleIndex();
		const int row = static_cast<int>(std::floor(localY / itemHeight));
		const int index = ScrollIndex + row;
		if (index < 0
			|| index >= static_cast<int>(_accessibilityVisibleNodes.size()))
			return nullptr;
		const auto& [node, level] =
			_accessibilityVisibleNodes[static_cast<size_t>(index)];
		if (relativeRowY)
			*relativeRowY = (std::clamp)(
				(localY - row * itemHeight) / itemHeight, 0.0f, 1.0f);
		if (hitExpander && node && node->HasItems())
		{
			const float rowInsetX = (std::max)(
				0.0f, ItemHorizontalPadding);
			const float chevronSize = (std::max)(6.0f, ChevronSize);
			const float chevronSlot = (std::max)(
				16.0f, chevronSize + 6.0f);
			const float expanderLeft = rowInsetX
				+ ((std::max)(0, level - 1)
					* (std::max)(8.0f, IndentWidth));
			*hitExpander = localX >= expanderLeft - 3.0f
				&& localX <= expanderLeft + chevronSlot + 3.0f;
		}
		return node;
	}
	float cursorY = 0.0f;
	bool animatedHitExpander = false;
	auto* result = findNode(
		this, localX, localY, size.height, itemHeight,
		static_cast<float>(ScrollIndex) * itemHeight,
		cursorY, 0, Root->Children, animatedHitExpander, relativeRowY);
	if (hitExpander) *hitExpander = animatedHitExpander;
	return result;
}

TreeNode* TreeView::HitTestNode(
	float localX, float localY, float* relativeRowY)
{
	bool hitExpander = false;
	return HitTestNodeCore(
		localX, localY, relativeRowY, &hitExpander);
}

void TreeView::RenderStableVisibleNodes(
	D2DGraphics* render, float width, float height, float itemHeight)
{
	if (!render || itemHeight <= 0.0f) return;
	EnsureAccessibilityVisibleIndex();
	const size_t first = ScrollIndex > 0
		? static_cast<size_t>(ScrollIndex) : 0;
	for (size_t index = first;
		index < _accessibilityVisibleNodes.size(); ++index)
	{
		const float renderTop = static_cast<float>(index - first) * itemHeight;
		if (renderTop >= height) break;
		const auto& [node, level] = _accessibilityVisibleNodes[index];
		renderNodeRow(this, render, width, itemHeight,
			renderTop, (std::max)(0, level - 1), node);
	}
}

void TreeView::SetDropTarget(
	TreeNode* node, TreeViewDropPosition position)
{
	if (!node) position = TreeViewDropPosition::None;
	if (DropTargetNode == node && DropPosition == position) return;
	DropTargetNode = node;
	DropPosition = position;
	InvalidateVisual();
}

void TreeView::ClearDropTarget()
{
	SetDropTarget(nullptr, TreeViewDropPosition::None);
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

static bool ContainsTreeNode(
	const std::vector<TreeNode*>& nodes, const TreeNode* candidate);

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

void TreeNode::DisconnectDataObservers()
{
	_childItemsChangedConnection.Disconnect();
	_childItemsObservation.Connections.clear();
	_childItemsObservation.Owners.clear();
	_childItemsObservation.ListOwners.clear();
	for (auto* child : Children)
		if (child) child->DisconnectDataObservers();
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
	if (expanded && HasItems() && !_childrenMaterialized && _ownerTree)
		(void)_ownerTree->EnsureDataChildrenMaterialized(*this);
	const bool wantExpand = expanded && HasItems() && !Children.empty();
	const float current = CurrentExpandProgress();
	const bool semanticChanged = this->Expand != wantExpand;
	this->Expand = wantExpand;
	if (_container) _container->SyncExpanded(wantExpand);
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
		if (_ownerTree) _ownerTree->_hasExpansionAnimation = true;
	}
	if (_ownerTree && semanticChanged)
		_ownerTree->OnNodeExpansionChanged(*this);
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
	_childItemsChangedConnection.Disconnect();
	_childItemsObservation.Connections.clear();
	_childItemsObservation.Owners.clear();
	_childItemsObservation.ListOwners.clear();
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

void TreeView::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto registerColor = [](const wchar_t* name, D2D1_COLOR_F defaultValue,
			int order, auto getter, auto setter)
		{
			ControlPropertyOptions<TreeView, D2D1_COLOR_F> options;
			options.DefaultValue = defaultValue;
			options.Flags = ControlPropertyFlags::AffectsRender;
			options.Equals = [](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
			{
				return left.r == right.r && left.g == right.g
					&& left.b == right.b && left.a == right.a;
			};
			options.Design.Category = L"Appearance";
			options.Design.CategoryOrder = 200;
			options.Design.Order = order;
			options.Design.Editor = ControlPropertyEditorKind::Color;
			options.Design.Persistence = ControlPropertyPersistence::Legacy;
			BindingPropertyRegistry::Register<TreeView, D2D1_COLOR_F>(
				name, std::move(getter), std::move(setter), {}, std::move(options));
		};
		registerColor(L"SelectedBackColor",
			cui::theme::palette::AccentSelected, 40,
			[](TreeView& target) { return target.SelectedBackColor; },
			[](TreeView& target, const D2D1_COLOR_F& value)
			{
				target.SelectedBackColor = value;
				target.InvalidateVisual();
			});
		registerColor(L"UnderMouseItemBackColor",
			cui::theme::palette::AccentSoft, 50,
			[](TreeView& target) { return target.UnderMouseItemBackColor; },
			[](TreeView& target, const D2D1_COLOR_F& value)
			{
				target.UnderMouseItemBackColor = value;
				target.InvalidateVisual();
			});
		registerColor(L"SelectedForeColor", cui::theme::palette::TextPrimary, 60,
			[](TreeView& target) { return target.SelectedForeColor; },
			[](TreeView& target, const D2D1_COLOR_F& value)
			{
				target.SelectedForeColor = value;
				target.InvalidateVisual();
			});
		ControlPropertyOptions<TreeView, std::wstring> containerStyleOptions;
		containerStyleOptions.DefaultValue = std::wstring{};
		containerStyleOptions.Flags = ControlPropertyFlags::AffectsRender;
		containerStyleOptions.Design.Category = L"Data";
		containerStyleOptions.Design.CategoryOrder = 80;
		containerStyleOptions.Design.Order = 70;
		containerStyleOptions.Design.Browsable = false;
		containerStyleOptions.Design.Persistence =
			ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<TreeView, std::wstring>(
			L"ItemContainerStyle",
			[](TreeView& target) { return target.GetItemContainerStyle(); },
			[](TreeView& target, const std::wstring& value)
			{ target.SetItemContainerStyle(value); }, {},
			std::move(containerStyleOptions));

		ControlPropertyOptions<TreeView, BindingListReference> sourceOptions;
		sourceOptions.DefaultValue = BindingListReference{};
		sourceOptions.Flags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsRender;
		sourceOptions.Design.Category = L"Data";
		sourceOptions.Design.CategoryOrder = 80;
		sourceOptions.Design.Order = 10;
		sourceOptions.Design.Browsable = false;
		sourceOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<TreeView, BindingListReference>(
			L"ItemsSource",
			[](TreeView& target) { return target.GetItemsSource(); },
			[](TreeView& target, const BindingListReference& value)
			{ target.SetItemsSource(value); }, {}, std::move(sourceOptions));

		ControlPropertyOptions<TreeView, ItemTemplateReference> templateOptions;
		templateOptions.DefaultValue = ItemTemplateReference{};
		templateOptions.Flags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsRender;
		templateOptions.Design.Category = L"Data";
		templateOptions.Design.CategoryOrder = 80;
		templateOptions.Design.Order = 20;
		templateOptions.Design.Browsable = false;
		templateOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<TreeView, ItemTemplateReference>(
			L"ItemTemplate",
			[](TreeView& target) { return target.GetItemTemplate(); },
			[](TreeView& target, const ItemTemplateReference& value)
			{ target.SetItemTemplate(value); }, {}, std::move(templateOptions));

		ControlPropertyOptions<TreeView, std::wstring> pathOptions;
		pathOptions.DefaultValue = std::wstring{};
		pathOptions.Flags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsRender;
		pathOptions.Design.Category = L"Data";
		pathOptions.Design.CategoryOrder = 80;
		pathOptions.Design.Order = 30;
		pathOptions.Design.Editor = ControlPropertyEditorKind::Text;
		pathOptions.Design.Persistence = ControlPropertyPersistence::Metadata;
		BindingPropertyRegistry::Register<TreeView, std::wstring>(
			L"DisplayMemberPath",
			[](TreeView& target) { return target.GetDisplayMemberPath(); },
			[](TreeView& target, const std::wstring& value)
			{ target.SetDisplayMemberPath(value); }, {}, std::move(pathOptions));

		ControlPropertyOptions<TreeView, std::wstring> valuePathOptions;
		valuePathOptions.DefaultValue = std::wstring{};
		valuePathOptions.Design.Category = L"Data";
		valuePathOptions.Design.CategoryOrder = 80;
		valuePathOptions.Design.Order = 40;
		valuePathOptions.Design.Editor = ControlPropertyEditorKind::Text;
		valuePathOptions.Design.Persistence =
			ControlPropertyPersistence::Metadata;
		BindingPropertyRegistry::Register<TreeView, std::wstring>(
			L"SelectedValuePath",
			[](TreeView& target) { return target.GetSelectedValuePath(); },
			[](TreeView& target, const std::wstring& value)
			{ target.SetSelectedValuePath(value); }, {},
			std::move(valuePathOptions));

		auto selectionProjectionOptions = [](int order)
		{
			ControlPropertyOptions<TreeView, BindingValue> options;
			options.DefaultValue = BindingValue{};
			options.Design.Category = L"Data";
			options.Design.CategoryOrder = 80;
			options.Design.Order = order;
			options.Design.Browsable = false;
			options.Design.Persistence = ControlPropertyPersistence::Transient;
			options.IsReadOnly = true;
			return options;
		};
		BindingPropertyRegistry::Register<TreeView, BindingValue>(
			L"SelectedItem",
			[](TreeView& target) { return target.GetSelectedItem(); }, {},
			[](TreeView& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedItemChanged.Subscribe(
					[handler = std::move(handler)](TreeView*) { handler(); });
			}, selectionProjectionOptions(50));
		BindingPropertyRegistry::Register<TreeView, BindingValue>(
			L"SelectedValue",
			[](TreeView& target) { return target.GetSelectedValue(); }, {},
			[](TreeView& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedValueChanged.Subscribe(
					[handler = std::move(handler)](TreeView*) { handler(); });
			}, selectionProjectionOptions(60));
		return true;
	}();
	(void)registered;
}

bool TreeView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	const float itemHeight = EffectiveItemHeight(this);
	const int renderItemCount = itemHeight > 0.0f ? (std::max)(1, (int)((float)this->Height / itemHeight)) : 1;
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
		const int renderCount = (std::max)(1, (int)((float)this->Height / itemHeight));
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
	EnsureBindingPropertiesRegistered();
	OnGotFocus += [this](Control*) { UpdateGeneratedItemStates(); };
	OnLostFocus += [this](Control*) { UpdateGeneratedItemStates(); };
}
TreeView::~TreeView()
{
	_itemsSourceChangedConnection.Disconnect();
	_retiredDataRoots->clear();
	ClearGeneratedItemContainers();
	if (this->Root) this->Root->AttachOwner(nullptr);
	delete this->Root;
}

ItemTemplateReference TreeView::ResolveDataItemTemplate(
	const BindingListReference& source, int level,
	std::wstring* outError) const
{
	if (outError) outError->clear();
	if (!source) return {};
	const auto& itemType = source.Get()->ItemTypeName();
	ItemTemplateReference result;
	if (_itemTemplate && (level == 0 || itemType.empty()
		|| _wcsicmp(_itemTemplate.Get()->DataTypeName().c_str(),
			itemType.c_str()) == 0)) result = _itemTemplate;
	if (!result && _implicitItemTemplateResolver)
		result = _implicitItemTemplateResolver(itemType);
	if (result && !itemType.empty()
		&& !result.Get()->DataTypeName().empty()
		&& _wcsicmp(result.Get()->DataTypeName().c_str(),
			itemType.c_str()) != 0)
	{
		if (outError) *outError =
			L"TreeView ItemTemplate DataType 与 ItemsSource ItemType 不一致："
			+ itemType;
		return {};
	}
	return result;
}

bool TreeView::BuildDataNode(
	const BindingListReference& source, size_t index,
	TreeNode& parent, int level,
	const std::unordered_map<IBindingSource*, bool>* expandedByItem,
	std::unique_ptr<TreeNode>& result, std::wstring* outError)
{
	result.reset();
	if (outError) outError->clear();
	BindingSourceReference item;
	if (!source || !source.Get()->TryGetItem(index, item) || !item)
	{
		if (outError) *outError = L"TreeView ItemsSource 无法读取索引 "
			+ std::to_wstring(index) + L"。";
		return false;
	}
	for (auto* ancestor = &parent; ancestor; ancestor = ancestor->_parentNode)
	{
		if (ancestor->_dataItem.Get() == item.Get())
		{
			if (outError) *outError =
				L"HierarchicalDataTemplate ItemsSource 形成数据项循环。";
			return false;
		}
	}

	std::wstring error;
	auto itemTemplate = ResolveDataItemTemplate(source, level, &error);
	if (!error.empty())
	{
		if (outError) *outError = std::move(error);
		return false;
	}
	auto node = std::make_unique<TreeNode>(GetBindingRecordText(
		item, _displayMemberPath, { L"Name", L"Header", L"Text" }));
	// Build detached subtrees transactionally, but keep the logical ancestry
	// available so a not-yet-committed grandchild can still detect a cycle.
	node->_parentNode = &parent;
	node->_dataItem = item;
	node->_headerTemplate = itemTemplate;

	if (itemTemplate && itemTemplate.Get()->IsHierarchical())
	{
		BindingListReference children;
		if (!itemTemplate.Get()->TryGetChildItemsSource(item, children, &error))
		{
			if (outError) *outError = error.empty()
				? L"HierarchicalDataTemplate 无法读取子 ItemsSource。"
				: std::move(error);
			return false;
		}
		node->_childItemsSource = children;
		node->_childrenMaterialized = false;
		auto* raw = node.get();
		node->_childItemsObservation = itemTemplate.Get()->
			ObserveChildItemsSource(item,
				[this, raw] { OnDataChildSourceChanged(raw); });
		if (children)
			node->_childItemsChangedConnection = children.Get()->SubscribeChanged(
				[this, raw, source = children](
					const CollectionChangedEventArgs& change)
				{ OnDataItemsChanged(raw, source, change); });

		bool restoreExpanded = false;
		if (expandedByItem)
		{
			const auto previous = expandedByItem->find(item.Get());
			restoreExpanded = previous != expandedByItem->end()
				&& previous->second;
		}
		if (restoreExpanded && children && children.Get()->Count() != 0)
		{
			if (!BuildDataChildren(*node, children, level + 1,
				expandedByItem, outError)) return false;
			node->Expand = !node->Children.empty();
			node->ExpandProgress = node->Expand ? 1.0f : 0.0f;
		}
	}
	result = std::move(node);
	return true;
}

bool TreeView::BuildDataChildren(
	TreeNode& parent, const BindingListReference& source, int level,
	const std::unordered_map<IBindingSource*, bool>* expandedByItem,
	std::wstring* outError)
{
	if (outError) outError->clear();
	TreeNode::ChildCollection::Base children;
	std::vector<std::unique_ptr<TreeNode>> owned;
	if (source)
	{
		children.reserve(source.Get()->Count());
		owned.reserve(source.Get()->Count());
		for (size_t index = 0; index < source.Get()->Count(); ++index)
		{
			std::unique_ptr<TreeNode> node;
			if (!BuildDataNode(source, index, parent, level,
				expandedByItem, node, outError)) return false;
			children.push_back(node.get());
			owned.emplace_back(std::move(node));
		}
	}

	const bool attached = parent._ownerTree == this;
	const int previousScroll = ScrollIndex;
	if (attached) _lastTemplateError.clear();
	parent._childrenMaterialized = true;
	parent.Children = std::move(children);
	if (attached && _useGeneratedItemContainers && !_lastTemplateError.empty())
	{
		const auto error = _lastTemplateError;
		parent._childrenMaterialized = false;
		parent.Children = TreeNode::ChildCollection::Base{};
		ScrollIndex = previousScroll;
		_lastTemplateError = error;
		if (outError) *outError = error;
		return false;
	}
	for (auto& node : owned) (void)node.release();
	return true;
}

bool TreeView::ApplyDataItemsChange(
	TreeNode& parent, const BindingListReference& source,
	const CollectionChangedEventArgs& change)
{
	const size_t oldCount = parent.Children.size();
	const size_t newCount = source ? source.Get()->Count() : 0;
	int level = 0;
	for (auto* cursor = &parent; cursor && cursor != Root;
		cursor = cursor->_parentNode) ++level;

	auto precise = [&]() noexcept
	{
		if (change.Action == CollectionChangeAction::Reset
			|| change.OldSize != oldCount || change.NewSize != newCount)
			return false;
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			return change.NewIndex <= oldCount && change.OldCount == 0
				&& change.NewCount > 0
				&& oldCount + change.NewCount == newCount;
		case CollectionChangeAction::Remove:
			return change.OldIndex < oldCount && change.OldCount > 0
				&& change.OldIndex + change.OldCount <= oldCount
				&& change.NewCount == 0
				&& oldCount - change.OldCount == newCount;
		case CollectionChangeAction::Replace:
			return change.OldIndex == change.NewIndex
				&& change.OldCount == change.NewCount
				&& change.OldCount > 0
				&& change.OldIndex + change.OldCount <= oldCount
				&& oldCount == newCount;
		case CollectionChangeAction::Move:
		case CollectionChangeAction::Swap:
			return change.OldCount == 1 && change.NewCount == 1
				&& oldCount == newCount && change.OldIndex < oldCount
				&& change.NewIndex < newCount;
		default: return false;
		}
	}();

	TreeNode::ChildCollection::Base oldOrder(
		parent.Children.begin(), parent.Children.end());
	TreeNode::ChildCollection::Base nextOrder = oldOrder;
	std::vector<std::unique_ptr<TreeNode>> added;
	std::vector<std::tuple<TreeNode*, std::wstring, ItemTemplateReference>>
		reusedUpdates;
	std::wstring error;
	auto buildRange = [&](size_t first, size_t count,
		TreeNode::ChildCollection::Base& output) -> bool
	{
		for (size_t offset = 0; offset < count; ++offset)
		{
			std::unique_ptr<TreeNode> node;
			if (!BuildDataNode(source, first + offset, parent, level,
				nullptr, node, &error)) return false;
			output.push_back(node.get());
			added.emplace_back(std::move(node));
		}
		return true;
	};

	if (precise)
	{
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
		{
			TreeNode::ChildCollection::Base inserted;
			if (!buildRange(change.NewIndex, change.NewCount, inserted)) break;
			nextOrder.insert(nextOrder.begin()
				+ static_cast<ptrdiff_t>(change.NewIndex),
				inserted.begin(), inserted.end());
			break;
		}
		case CollectionChangeAction::Remove:
			nextOrder.erase(nextOrder.begin()
				+ static_cast<ptrdiff_t>(change.OldIndex),
				nextOrder.begin() + static_cast<ptrdiff_t>(
					change.OldIndex + change.OldCount));
			break;
		case CollectionChangeAction::Replace:
		{
			TreeNode::ChildCollection::Base inserted;
			if (!buildRange(change.NewIndex, change.NewCount, inserted)) break;
			std::copy(inserted.begin(), inserted.end(), nextOrder.begin()
				+ static_cast<ptrdiff_t>(change.NewIndex));
			break;
		}
		case CollectionChangeAction::Move:
			if (change.OldIndex < change.NewIndex)
				std::rotate(nextOrder.begin()
					+ static_cast<ptrdiff_t>(change.OldIndex),
					nextOrder.begin()
					+ static_cast<ptrdiff_t>(change.OldIndex + 1),
					nextOrder.begin()
					+ static_cast<ptrdiff_t>(change.NewIndex + 1));
			else if (change.NewIndex < change.OldIndex)
				std::rotate(nextOrder.begin()
					+ static_cast<ptrdiff_t>(change.NewIndex),
					nextOrder.begin()
					+ static_cast<ptrdiff_t>(change.OldIndex),
					nextOrder.begin()
					+ static_cast<ptrdiff_t>(change.OldIndex + 1));
			break;
		case CollectionChangeAction::Swap:
			std::swap(nextOrder[change.OldIndex], nextOrder[change.NewIndex]);
			break;
		default: break;
		}
		if (!error.empty())
		{
			_lastTemplateError = std::move(error);
			return false;
		}
	}
	else
	{
		nextOrder.clear();
		nextOrder.reserve(newCount);
		std::unordered_set<TreeNode*> reused;
		std::wstring templateError;
		auto levelTemplate = ResolveDataItemTemplate(
			source, level, &templateError);
		if (!templateError.empty())
		{
			_lastTemplateError = std::move(templateError);
			return false;
		}
		for (size_t index = 0; index < newCount; ++index)
		{
			BindingSourceReference item;
			if (!source.Get()->TryGetItem(index, item) || !item)
			{
				_lastTemplateError = L"TreeView ItemsSource 无法读取索引 "
					+ std::to_wstring(index) + L"。";
				return false;
			}
			TreeNode* existing = nullptr;
			for (auto* candidate : oldOrder)
				if (candidate && !reused.contains(candidate)
					&& candidate->_dataItem.Shared() == item.Shared())
				{
					existing = candidate;
					break;
				}
			if (existing)
			{
				reused.insert(existing);
				nextOrder.push_back(existing);
				reusedUpdates.emplace_back(existing,
					GetBindingRecordText(item, _displayMemberPath,
						{ L"Name", L"Header", L"Text" }), levelTemplate);
			}
			else
			{
				std::unique_ptr<TreeNode> node;
				if (!BuildDataNode(source, index, parent, level,
					nullptr, node, &error))
				{
					_lastTemplateError = error.empty()
						? L"TreeView 无法生成层次数据项。"
						: std::move(error);
					return false;
				}
				nextOrder.push_back(node.get());
				added.emplace_back(std::move(node));
			}
		}
	}

	if (nextOrder.size() != newCount)
	{
		_lastTemplateError = L"TreeView 集合通知与 ItemsSource 数量不一致。";
		return false;
	}
	EnsureAccessibilityVisibleIndex();
	_pendingScrollAnchor = ScrollIndex >= 0
		&& static_cast<size_t>(ScrollIndex) < _accessibilityVisibleNodes.size()
		? _accessibilityVisibleNodes[static_cast<size_t>(ScrollIndex)].first
		: nullptr;
	const int previousScroll = ScrollIndex;
	TreeNode* previousSelected = SelectedNode;
	TreeNode* previousHovered = HoveredNode;
	TreeNode* previousDropTarget = DropTargetNode;
	std::unordered_set<TreeNode*> retained(nextOrder.begin(), nextOrder.end());
	std::vector<std::unique_ptr<TreeNode>> removed;
	for (auto* node : oldOrder)
		if (node && !retained.contains(node)) removed.emplace_back(node);

	_lastTemplateError.clear();
	parent.Children = std::move(nextOrder);
	if (_useGeneratedItemContainers && !_lastTemplateError.empty())
	{
		const auto containerError = _lastTemplateError;
		parent.Children = std::move(oldOrder);
		for (auto& node : removed) (void)node.release();
		SelectedNode = previousSelected;
		HoveredNode = previousHovered;
		DropTargetNode = previousDropTarget;
		ScrollIndex = previousScroll;
		_pendingScrollAnchor = nullptr;
		InvalidateAccessibilityIndex(true);
		UpdateGeneratedItemStates();
		_lastTemplateError = containerError;
		return false;
	}
	for (auto& [node, text, itemTemplate] : reusedUpdates)
	{
		node->Text = std::move(text);
		node->_headerTemplate = std::move(itemTemplate);
	}
	for (auto& node : added) (void)node.release();
	for (auto& node : removed)
	{
		if (node) node->DisconnectDataObservers();
		_retiredDataRoots->emplace_back(std::move(node));
	}
	if (!removed.empty() && cui::HasUIThreadDispatcher())
	{
		auto retired = _retiredDataRoots;
		(void)cui::PostToUIThread([retired] { retired->clear(); });
	}
	_lastTemplateError.clear();
	return true;
}

bool TreeView::EnsureDataChildrenMaterialized(TreeNode& node)
{
	if (node._childrenMaterialized) return true;
	if (!node._childItemsSource)
	{
		node._childrenMaterialized = true;
		if (node._container) node._container->SyncHasItems(false);
		return true;
	}
	std::wstring error;
	if (!BuildDataChildren(node, node._childItemsSource,
		[&]
		{
			int result = 1;
			for (auto* parent = node._parentNode;
				parent && parent != Root; parent = parent->_parentNode) ++result;
			return result;
		}(), nullptr, &error))
	{
		_lastTemplateError = error.empty()
			? L"TreeView 无法延迟创建子项。" : std::move(error);
		return false;
	}
	_lastTemplateError.clear();
	return true;
}

void TreeView::OnDataItemsChanged(
	TreeNode* parent, BindingListReference source,
	const CollectionChangedEventArgs& change)
{
	if (_rebuildingDataItems || !Root || !parent || !source) return;
	if (parent == Root)
	{
		if (_itemsSource != source) return;
	}
	else
	{
		if (!ContainsTreeNode(Root->Children, parent)
			|| parent->_childItemsSource != source) return;
		if (!parent->_childrenMaterialized)
		{
			if (source.Get()->Count() == 0) parent->SetExpanded(false, false);
			if (parent->_container)
				parent->_container->SyncHasItems(parent->HasItems());
			_lastTemplateError.clear();
			NotifyAccessibilityVirtualChanged(parent->AccessibilityId,
				AccessibilityChange::Structure);
			InvalidateVisual();
			return;
		}
	}
	(void)ApplyDataItemsChange(*parent, source, change);
}

void TreeView::OnDataChildSourceChanged(TreeNode* node)
{
	if (_rebuildingDataItems || !Root || !node
		|| !ContainsTreeNode(Root->Children, node)
		|| !node->_dataItem || !node->_headerTemplate
		|| !node->_headerTemplate.Get()->IsHierarchical()) return;
	BindingListReference next;
	std::wstring error;
	if (!node->_headerTemplate.Get()->TryGetChildItemsSource(
		node->_dataItem, next, &error))
	{
		_lastTemplateError = error.empty()
			? L"HierarchicalDataTemplate 无法读取子 ItemsSource。"
			: std::move(error);
		return;
	}
	if (next == node->_childItemsSource)
	{
		_lastTemplateError.clear();
		return;
	}

	auto previousSource = node->_childItemsSource;
	auto previousObservation = std::move(node->_childItemsObservation);
	auto previousConnection = std::move(node->_childItemsChangedConnection);
	node->_childItemsSource = next;
	node->_childItemsObservation = node->_headerTemplate.Get()->
		ObserveChildItemsSource(node->_dataItem,
			[this, node] { OnDataChildSourceChanged(node); });
	if (next)
		node->_childItemsChangedConnection = next.Get()->SubscribeChanged(
			[this, node, source = next](const CollectionChangedEventArgs& change)
			{ OnDataItemsChanged(node, source, change); });

	if (node->_childrenMaterialized)
	{
		const CollectionChangedEventArgs reset{
			CollectionChangeAction::Reset,
			CollectionChangedEventArgs::Npos,
			CollectionChangedEventArgs::Npos,
			node->Children.size(), next ? next.Get()->Count() : 0,
			node->Children.size(), next ? next.Get()->Count() : 0 };
		if (!ApplyDataItemsChange(*node, next, reset))
		{
			const auto applyError = _lastTemplateError;
			node->_childItemsChangedConnection.Disconnect();
			node->_childItemsObservation.Connections.clear();
			node->_childItemsObservation.Owners.clear();
			node->_childItemsObservation.ListOwners.clear();
			node->_childItemsSource = previousSource;
			node->_childItemsObservation = std::move(previousObservation);
			node->_childItemsChangedConnection = std::move(previousConnection);
			_lastTemplateError = applyError;
			return;
		}
	}
	else
	{
		if (!next || next.Get()->Count() == 0) node->SetExpanded(false, false);
		if (node->_container) node->_container->SyncHasItems(node->HasItems());
		_lastTemplateError.clear();
		NotifyAccessibilityVirtualChanged(node->AccessibilityId,
			AccessibilityChange::Structure);
		InvalidateVisual();
	}
}

bool TreeView::RebuildDataItems()
{
	if (_rebuildingDataItems) return true;
	struct RebuildGuard final
	{
		bool& Value;
		explicit RebuildGuard(bool& value) : Value(value) { Value = true; }
		~RebuildGuard() { Value = false; }
	} guard(_rebuildingDataItems);

	std::unordered_map<IBindingSource*, bool> expandedByItem;
	std::shared_ptr<IBindingSource> selectedItem;
	auto capture = [&](TreeNode* node, const auto& self) -> void
	{
		if (!node) return;
		if (node->_dataItem)
		{
			expandedByItem[node->_dataItem.Get()] = node->Expand;
			if (SelectedNode == node) selectedItem = node->_dataItem.Shared();
		}
		for (auto* child : node->Children) self(child, self);
	};
	if (Root)
		for (auto* node : Root->Children) capture(node, capture);

	auto replacement = std::make_unique<TreeNode>(L"");
	std::wstring buildError;
	replacement->_childrenMaterialized = true;
	if (_itemsSource && !BuildDataChildren(*replacement, _itemsSource, 0,
		&expandedByItem, &buildError))
	{
		_lastTemplateError = buildError.empty()
			? L"TreeView 无法生成层次数据项。" : std::move(buildError);
		return false;
	}

	auto* previousRoot = Root;
	auto* previousSelected = SelectedNode;
	const uint32_t previousSelectedId = previousSelected
		? previousSelected->AccessibilityId : 0;
	auto* previousHovered = HoveredNode;
	auto* previousDropTarget = DropTargetNode;
	const int previousScroll = ScrollIndex;
	SelectedNode = nullptr;
	HoveredNode = nullptr;
	DropTargetNode = nullptr;
	Root = replacement.release();
	Root->AttachOwner(this);
	if (selectedItem)
	{
		auto restoreSelection = [&](TreeNode* node, const auto& self) -> TreeNode*
		{
			if (!node) return nullptr;
			if (node->_dataItem.Shared() == selectedItem) return node;
			for (auto* child : node->Children)
				if (auto* found = self(child, self)) return found;
			return nullptr;
		};
		for (auto* node : Root->Children)
			if ((SelectedNode = restoreSelection(node, restoreSelection))) break;
	}
	InvalidateAccessibilityIndex(true);
	EnsureAccessibilityVisibleIndex();
	const float itemHeight = EffectiveItemHeight(this);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(Height) / itemHeight))) : 1;
	const int maximum = (std::max)(0,
		static_cast<int>(_accessibilityVisibleNodes.size()) - page);
	ScrollIndex = (std::clamp)(ScrollIndex, 0, maximum);
	if (_useGeneratedItemContainers && !RebuildGeneratedItemContainers())
	{
		const auto error = _lastTemplateError;
		Root->AttachOwner(nullptr);
		delete Root;
		Root = previousRoot;
		SelectedNode = previousSelected;
		HoveredNode = previousHovered;
		DropTargetNode = previousDropTarget;
		ScrollIndex = previousScroll;
		if (Root) Root->AttachOwner(this);
		InvalidateAccessibilityIndex(true);
		_lastTemplateError = error;
		return false;
	}
	if (previousRoot)
	{
		previousRoot->AttachOwner(nullptr);
		previousRoot->DisconnectDataObservers();
		_retiredDataRoots->emplace_back(previousRoot);
		if (cui::HasUIThreadDispatcher())
		{
			auto retired = _retiredDataRoots;
			(void)cui::PostToUIThread(
				[retired] { retired->clear(); });
		}
	}
	const bool selectionLost = previousSelected && !SelectedNode;
	if (selectionLost)
	{
		UpdateGeneratedItemStates();
		SelectionChanged(this);
		SelectedItemChanged(this);
		NotifySelectionProjectionChanged(true);
		if (previousSelectedId != 0)
			NotifyAccessibilityVirtualChanged(
				previousSelectedId,
				AccessibilityChange::Selection);
	}
	else
	{
		// Rebuilt nodes may have new addresses while preserving the same data
		// record. Rewire SelectedValue observation without reporting a user
		// selection change.
		RefreshSelectedItemObservation();
	}
	_lastTemplateError.clear();
	if (ScrollIndex != previousScroll)
	{
		ScrollChanged(this);
		NotifyAccessibilityScrollChanged();
	}
	InvalidateVisual();
	return true;
}

void TreeView::SetItemsSource(BindingListReference value)
{
	if (_itemsSource == value) return;
	const auto previous = _itemsSource;
	const bool previousUse = _useGeneratedItemContainers;
	_itemsSourceChangedConnection.Disconnect();
	_itemsSource = std::move(value);
	if (_itemsSource) _useGeneratedItemContainers = true;
	if (RebuildDataItems())
	{
		if (_itemsSource)
			_itemsSourceChangedConnection = _itemsSource.Get()->SubscribeChanged(
				[this, source = _itemsSource](
					const CollectionChangedEventArgs& change)
				{ OnDataItemsChanged(Root, source, change); });
		return;
	}
	const auto error = _lastTemplateError;
	_itemsSource = previous;
	_useGeneratedItemContainers = previousUse;
	if (_itemsSource)
		_itemsSourceChangedConnection = _itemsSource.Get()->SubscribeChanged(
			[this, source = _itemsSource](
				const CollectionChangedEventArgs& change)
			{ OnDataItemsChanged(Root, source, change); });
	_lastTemplateError = error;
}

void TreeView::SetItemTemplate(ItemTemplateReference value)
{
	if (_itemTemplate == value) return;
	const auto previous = _itemTemplate;
	_itemTemplate = std::move(value);
	if (!_itemsSource || RebuildDataItems()) return;
	const auto error = _lastTemplateError;
	_itemTemplate = previous;
	_lastTemplateError = error;
}

void TreeView::SetDisplayMemberPath(std::wstring value)
{
	if (_displayMemberPath == value) return;
	const auto previous = _displayMemberPath;
	_displayMemberPath = std::move(value);
	if (!_itemsSource || RebuildDataItems()) return;
	const auto error = _lastTemplateError;
	_displayMemberPath = previous;
	_lastTemplateError = error;
}

BindingValue TreeView::GetSelectedItem() const
{
	if (!SelectedNode) return {};
	if (SelectedNode->_dataItem)
		return BindingValue(SelectedNode->_dataItem);
	return BindingValue(SelectedNode);
}

BindingValue TreeView::GetSelectedValue() const
{
	if (!SelectedNode) return {};
	if (_selectedValuePath.empty()) return GetSelectedItem();
	BindingValue result;
	return SelectedNode->_dataItem
		&& TryGetBindingPathValue(
			*SelectedNode->_dataItem.Get(), _selectedValuePath, result)
		? result : BindingValue{};
}

void TreeView::SetSelectedValuePath(std::wstring value)
{
	if (_selectedValuePath == value) return;
	_selectedValuePath = std::move(value);
	RefreshSelectedItemObservation();
	_selectedValueChanged(this);
}

void TreeView::RefreshSelectedItemObservation()
{
	_selectedItemObservation = {};
	if (!SelectedNode || !SelectedNode->_dataItem
		|| _selectedValuePath.empty()) return;
	_selectedItemObservation = ObserveBindingPaths(
		SelectedNode->_dataItem, { _selectedValuePath },
		[this]
		{
			// A parent member in a nested path may now reference another object.
			// Rebuild the path watch before publishing the new projection.
			RefreshSelectedItemObservation();
			_selectedValueChanged(this);
		});
}

void TreeView::NotifySelectionProjectionChanged(bool itemChanged)
{
	RefreshSelectedItemObservation();
	if (itemChanged) _selectedItemChanged(this);
	_selectedValueChanged(this);
}

bool TreeView::ApplySelection(TreeNode* node, bool bringIntoView)
{
	if (node && (!Root || !ContainsTreeNode(Root->Children, node)))
		return false;
	if (SelectedNode == node)
	{
		if (node && bringIntoView)
			(void)BringNodeIntoView(*node, true);
		return false;
	}

	TreeNode* previous = SelectedNode;
	SelectedNode = node;
	if (node && bringIntoView)
		(void)BringNodeIntoView(*node, true);
	UpdateGeneratedItemStates();
	SelectionChanged(this);
	SelectedItemChanged(this);
	NotifySelectionProjectionChanged(true);
	if (previous && previous->AccessibilityId != 0)
		NotifyAccessibilityVirtualChanged(
			previous->AccessibilityId, AccessibilityChange::Selection);
	if (node && node->AccessibilityId != 0)
		NotifyAccessibilityVirtualChanged(
			node->AccessibilityId, AccessibilityChange::Selection);
	InvalidateVisual();
	return true;
}

bool TreeView::SelectNode(TreeNode* node, bool bringIntoView)
{
	return ApplySelection(node, bringIntoView);
}

void TreeView::SetImplicitItemTemplateResolver(
	ImplicitItemTemplateResolver value)
{
	auto previous = std::move(_implicitItemTemplateResolver);
	_implicitItemTemplateResolver = std::move(value);
	if (!_itemsSource || RebuildDataItems()) return;
	const auto error = _lastTemplateError;
	_implicitItemTemplateResolver = std::move(previous);
	_lastTemplateError = error;
}

void TreeView::SetItemContainerStyle(std::wstring value)
{
	if (_itemContainerStyle == value) return;
	const auto previous = _itemContainerStyle;
	const bool previousUse = _useGeneratedItemContainers;
	_itemContainerStyle = std::move(value);
	if (!_itemContainerStyle.empty()) _useGeneratedItemContainers = true;
	if (RebuildGeneratedItemContainers()) return;
	const auto error = _lastTemplateError;
	_itemContainerStyle = previous;
	_useGeneratedItemContainers = previousUse;
	(void)RebuildGeneratedItemContainers();
	_lastTemplateError = error;
}

void TreeView::SetItemContainerTemplate(ControlTemplateReference value)
{
	if (_itemContainerTemplate == value) return;
	const auto previous = _itemContainerTemplate;
	const bool previousUse = _useGeneratedItemContainers;
	_itemContainerTemplate = std::move(value);
	if (_itemContainerTemplate) _useGeneratedItemContainers = true;
	if (RebuildGeneratedItemContainers()) return;
	const auto error = _lastTemplateError;
	_itemContainerTemplate = previous;
	_useGeneratedItemContainers = previousUse;
	(void)RebuildGeneratedItemContainers();
	_lastTemplateError = error;
}

void TreeView::SetUseGeneratedItemContainers(bool value)
{
	if (_useGeneratedItemContainers == value) return;
	const bool previous = _useGeneratedItemContainers;
	_useGeneratedItemContainers = value;
	if (RebuildGeneratedItemContainers()) return;
	const auto error = _lastTemplateError;
	_useGeneratedItemContainers = previous;
	(void)RebuildGeneratedItemContainers();
	_lastTemplateError = error;
}

void TreeView::ClearGeneratedItemContainers()
{
	for (auto& [node, level] : _realizedGeneratedNodes)
	{
		(void)level;
		if (!node || !node->_container) continue;
		node->_container->Parent = nullptr;
		node->_container.reset();
	}
	_realizedGeneratedNodes.clear();
	_recycledItemContainers.clear();
}

bool TreeView::RebuildGeneratedItemContainers()
{
	return RefreshGeneratedItemContainers(true);
}

bool TreeView::RefreshGeneratedItemContainers(bool recreate)
{
	if (!_useGeneratedItemContainers)
	{
		ClearGeneratedItemContainers();
		_lastTemplateError.clear();
		InvalidateVisual();
		return true;
	}
	if (!Root)
	{
		_lastTemplateError = L"TreeView 缺少根节点。";
		return false;
	}
	if (_itemContainerTemplate
		&& _itemContainerTemplate.Get()->TargetType()
			!= UIClass::UI_TreeViewItem)
	{
		_lastTemplateError =
			L"ItemContainerTemplate TargetType 必须是 TreeViewItem。";
		return false;
	}

	if (recreate) _recycledItemContainers.clear();
	std::vector<std::pair<TreeNode*, int>> realizable;
	if (_hasExpansionAnimation)
	{
		bool anyAnimation = false;
		auto collect = [&](TreeNode* node, int level, const auto& self) -> void
		{
			if (!node) return;
			realizable.emplace_back(node, level);
			const float progress = node->CurrentExpandProgress();
			anyAnimation = anyAnimation || node->Animating;
			if (node->Expand || node->Animating || progress > 0.001f)
				for (auto* child : node->Children)
					self(child, level + 1, self);
		};
		for (auto* node : Root->Children) collect(node, 0, collect);
		_hasExpansionAnimation = anyAnimation;
	}
	else
	{
		EnsureAccessibilityVisibleIndex();
	}
	const size_t rowCount = _hasExpansionAnimation
		? realizable.size() : _accessibilityVisibleNodes.size();
	auto rowAt = [&](size_t index) -> std::pair<TreeNode*, int>
	{
		if (_hasExpansionAnimation) return realizable[index];
		const auto& [node, level] = _accessibilityVisibleNodes[index];
		return { node, (std::max)(0, level - 1) };
	};

	const float itemHeight = EffectiveItemHeight(this);
	const float viewportHeight = GetActualSizeDip().height > 0.0f
		? GetActualSizeDip().height : static_cast<float>(Height);
	const size_t page = itemHeight > 0.0f
		? static_cast<size_t>((std::max)(1.0f,
			std::ceil(viewportHeight / itemHeight))) : 1;
	const size_t scroll = ScrollIndex > 0
		? static_cast<size_t>(ScrollIndex) : 0;
	const size_t first = scroll > 0 ? scroll - 1 : 0;
	const size_t last = (std::min)(rowCount, scroll + page + 1);
	std::unordered_set<TreeNode*> desired;
	for (size_t index = first; index < last; ++index)
		desired.insert(rowAt(index).first);

	std::vector<std::pair<TreeNode*, std::unique_ptr<TreeViewItem>>> replacement;
	auto build = [&](TreeNode* node, int level) -> bool
	{
		if (!node || (!recreate && node->_container)) return true;
		std::unique_ptr<TreeViewItem> container;
		if (!recreate && !_recycledItemContainers.empty())
		{
			container = std::move(_recycledItemContainers.back());
			_recycledItemContainers.pop_back();
		}
		else if (_itemContainerTemplate)
		{
			std::wstring error;
			auto built = _itemContainerTemplate.Get()->Build(&error);
			auto* typed = dynamic_cast<TreeViewItem*>(built.get());
			if (!typed)
			{
				_lastTemplateError = error.empty()
					? L"ItemContainerTemplate 未生成 TreeViewItem。"
					: std::move(error);
				return false;
			}
			container.reset(static_cast<TreeViewItem*>(built.release()));
		}
		else container = std::make_unique<TreeViewItem>();
		container->SetStyleId(_itemContainerStyle);
		std::wstring error;
		if (!container->Initialize(*this, *node, level, &error))
		{
			_lastTemplateError = error.empty()
				? L"TreeViewItem Header 初始化失败。" : std::move(error);
			return false;
		}
		container->Parent = this;
		Control::SetChildrenParentForm(container.get(), ParentForm);
		(void)container->RefreshStyleValues(true);
		replacement.emplace_back(node, std::move(container));
		return true;
	};
	for (size_t index = first; index < last; ++index)
	{
		const auto [node, level] = rowAt(index);
		if (!build(node, level)) return false;
	}

	for (auto& [node, level] : _realizedGeneratedNodes)
	{
		(void)level;
		if (!node || !node->_container
			|| (!recreate && desired.contains(node))) continue;
		if (recreate)
		{
			node->_container->Parent = nullptr;
			node->_container.reset();
		}
		else
		{
			auto recycled = std::move(node->_container);
			recycled->ClearForRecycle();
			_recycledItemContainers.emplace_back(std::move(recycled));
		}
	}
	for (auto& [node, container] : replacement)
		node->_container = std::move(container);
	_realizedGeneratedNodes.clear();
	for (size_t index = first; index < last; ++index)
	{
		const auto [node, level] = rowAt(index);
		if (node && node->_container)
			_realizedGeneratedNodes.emplace_back(node, level);
	}
	const size_t recycleLimit = (std::max)(
		static_cast<size_t>(8), page * 2 + 2);
	if (_recycledItemContainers.size() > recycleLimit)
		_recycledItemContainers.erase(_recycledItemContainers.begin(),
			_recycledItemContainers.begin() + static_cast<ptrdiff_t>(
				_recycledItemContainers.size() - recycleLimit));
	_lastTemplateError.clear();
	UpdateGeneratedItemStates();
	InvalidateVisual();
	return true;
}

void TreeView::UpdateGeneratedItemStates()
{
	const bool ownerFocused = ParentForm && ParentForm->Selected == this;
	for (auto& [node, level] : _realizedGeneratedNodes)
	{
		if (!node || !node->_container) continue;
		node->_container->Parent = this;
		Control::SetChildrenParentForm(node->_container.get(), ParentForm);
		node->_container->SyncNodeState(level,
			SelectedNode == node, HoveredNode == node,
			ownerFocused && SelectedNode == node);
	}
}

size_t TreeView::GeneratedItemCount() const noexcept
{
	size_t count = 0;
	for (const auto& [node, level] : _realizedGeneratedNodes)
	{
		(void)level;
		if (node && node->_container) ++count;
	}
	return count;
}

TreeViewItem* TreeView::GetGeneratedItem(const TreeNode* node) const noexcept
{
	return node ? node->_container.get() : nullptr;
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
	if (!Root) _accessibilityVisibleCount = 0;
}

void TreeView::EnsureAccessibilityIndex()
{
	if (!_accessibilityIndexDirty) return;
	_accessibilityNodeIndex.clear();
	_accessibilityChildrenByParentId.clear();
	if (!Root)
	{
		_accessibilityIndexDirty = false;
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
}

void TreeView::EnsureAccessibilityVisibleIndex()
{
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

bool TreeView::PatchAccessibilityVisibleChildren(TreeNode* parent)
{
	if (_accessibilityVisibleDirty || !Root || !parent) return false;
	if (parent == Root)
	{
		_accessibilityVisibleNodes.clear();
		CollectVisibleTreeNodes(
			Root->Children, 1, _accessibilityVisibleNodes);
	}
	else
	{
		const auto parentAt = _accessibilityVisibleIndex.find(parent);
		if (parentAt == _accessibilityVisibleIndex.end() || !parent->Expand)
		{
			_accessibilityVisibleCount = _accessibilityVisibleNodes.size();
			return true;
		}
		const size_t parentIndex = parentAt->second;
		const int parentLevel = _accessibilityVisibleNodes[parentIndex].second;
		size_t end = parentIndex + 1;
		while (end < _accessibilityVisibleNodes.size()
			&& _accessibilityVisibleNodes[end].second > parentLevel) ++end;
		std::vector<std::pair<TreeNode*, int>> replacement;
		CollectVisibleTreeNodes(parent->Children,
			parentLevel + 1, replacement);
		_accessibilityVisibleNodes.erase(
			_accessibilityVisibleNodes.begin()
				+ static_cast<ptrdiff_t>(parentIndex + 1),
			_accessibilityVisibleNodes.begin()
				+ static_cast<ptrdiff_t>(end));
		_accessibilityVisibleNodes.insert(
			_accessibilityVisibleNodes.begin()
				+ static_cast<ptrdiff_t>(parentIndex + 1),
			replacement.begin(), replacement.end());
	}
	_accessibilityVisibleIndex.clear();
	_accessibilityVisibleIndex.reserve(_accessibilityVisibleNodes.size());
	for (size_t index = 0; index < _accessibilityVisibleNodes.size(); ++index)
		_accessibilityVisibleIndex.emplace(
			_accessibilityVisibleNodes[index].first, index);
	_accessibilityVisibleCount = _accessibilityVisibleNodes.size();
	_accessibilityVisibleDirty = false;
	return true;
}

void TreeView::OnNodeChildrenChanged(
	TreeNode* parent, const CollectionChangedEventArgs& change)
{
	(void)change;
	if (!Root) return;
	_accessibilityIndexDirty = true;
	if (!PatchAccessibilityVisibleChildren(parent))
		_accessibilityVisibleDirty = true;
	if (parent && parent->Children.empty())
		parent->SetExpanded(false, false);

	if (SelectedNode && !ContainsTreeNode(Root->Children, SelectedNode))
		(void)ApplySelection(nullptr, false);
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
	int nextScroll = ScrollIndex;
	if (_pendingScrollAnchor)
	{
		const auto anchor = _accessibilityVisibleIndex.find(_pendingScrollAnchor);
		if (anchor != _accessibilityVisibleIndex.end())
			nextScroll = static_cast<int>(anchor->second);
		_pendingScrollAnchor = nullptr;
	}
	nextScroll = (std::clamp)(nextScroll, 0, maximum);
	if (nextScroll != ScrollIndex)
	{
		ScrollIndex = nextScroll;
		ScrollChanged(this);
	}
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	if (_useGeneratedItemContainers)
		(void)RefreshGeneratedItemContainers(false);
	else UpdateGeneratedItemStates();
	InvalidateVisual();
}

void TreeView::OnNodeExpansionChanged(TreeNode& node)
{
	if (!Root || !ContainsTreeNode(Root->Children, &node)) return;
	InvalidateAccessibilityIndex(false);
	EnsureAccessibilityVisibleIndex();
	_contentRenderItems = static_cast<float>(_accessibilityVisibleNodes.size());
	MaxRenderItems = static_cast<int>(_accessibilityVisibleNodes.size());
	const float itemHeight = EffectiveItemHeight(this);
	const int page = itemHeight > 0.0f
		? (std::max)(1, static_cast<int>(std::floor(
			static_cast<float>(Height) / itemHeight))) : 1;
	const int maximum = (std::max)(0, MaxRenderItems - page);
	const int nextScroll = (std::clamp)(ScrollIndex, 0, maximum);
	if (nextScroll != ScrollIndex)
	{
		ScrollIndex = nextScroll;
		ScrollChanged(this);
	}
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	if (_useGeneratedItemContainers)
		(void)RefreshGeneratedItemContainers(false);
	else UpdateGeneratedItemStates();
	InvalidateVisual();
}

void TreeView::GetAccessibilityVirtualChildren(
	uint32_t parentId, std::vector<uint32_t>& result)
{
	result.clear();
	if (!Root) return;
	EnsureAccessibilityIndex();
	if (parentId != 0)
	{
		const auto parent = _accessibilityNodeIndex.find(parentId);
		if (parent != _accessibilityNodeIndex.end()
			&& !parent->second.Node->ChildrenMaterialized()
			&& parent->second.Node->HasItems())
		{
			(void)EnsureDataChildrenMaterialized(*parent->second.Node);
			EnsureAccessibilityIndex();
		}
	}
	const auto children = _accessibilityChildrenByParentId.find(parentId);
	if (children == _accessibilityChildrenByParentId.end()) return;
	result.reserve(children->second.size());
	for (auto* child : children->second)
		result.push_back(child->AccessibilityId);
}

size_t TreeView::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	EnsureAccessibilityIndex();
	if (parentId != 0)
	{
		const auto parent = _accessibilityNodeIndex.find(parentId);
		if (parent != _accessibilityNodeIndex.end()
			&& !parent->second.Node->ChildrenMaterialized()
			&& parent->second.Node->HasItems())
		{
			(void)EnsureDataChildrenMaterialized(*parent->second.Node);
			EnsureAccessibilityIndex();
		}
	}
	const auto children = _accessibilityChildrenByParentId.find(parentId);
	return children == _accessibilityChildrenByParentId.end()
		? 0 : children->second.size();
}

bool TreeView::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	EnsureAccessibilityIndex();
	if (parentId != 0)
	{
		const auto parent = _accessibilityNodeIndex.find(parentId);
		if (parent != _accessibilityNodeIndex.end()
			&& !parent->second.Node->ChildrenMaterialized()
			&& parent->second.Node->HasItems())
		{
			(void)EnsureDataChildrenMaterialized(*parent->second.Node);
			EnsureAccessibilityIndex();
		}
	}
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
	EnsureAccessibilityIndex();
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
	EnsureAccessibilityIndex();
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
	if (node->HasItems())
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
	if (action == AccessibilitySelectionAction::Remove
		&& SelectedNode != indexed->second.Node) return true;
	TreeNode* next = action == AccessibilitySelectionAction::Remove
		? nullptr : indexed->second.Node;
	(void)ApplySelection(next,
		action != AccessibilitySelectionAction::Remove);
	return true;
}

bool TreeView::SetAccessibilityVirtualNodeExpanded(
	uint32_t id, bool expanded)
{
	if (!Root || !Enable) return false;
	EnsureAccessibilityIndex();
	const auto indexed = _accessibilityNodeIndex.find(id);
	if (indexed == _accessibilityNodeIndex.end()
		|| !indexed->second.Node->HasItems()) return false;
	indexed->second.Node->SetExpanded(expanded, AreSystemAnimationsEnabled());
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::ExpandCollapse);
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
	return true;
}

bool TreeView::BringNodeIntoView(TreeNode& node, bool expandAncestors)
{
	if (!Root || !ContainsTreeNode(Root->Children, &node)) return false;
	if (expandAncestors)
		for (auto* parent = node._parentNode;
			parent && parent != Root; parent = parent->_parentNode)
			parent->SetExpanded(true, false);
	EnsureAccessibilityVisibleIndex();
	const auto position = _accessibilityVisibleIndex.find(&node);
	if (position == _accessibilityVisibleIndex.end()) return false;
	const int target = static_cast<int>(position->second);
	const float itemHeight = EffectiveItemHeight(this);
	const int page = (std::max)(1,
		static_cast<int>(std::floor(static_cast<float>(Height)
			/ (std::max)(itemHeight, 1.0f))));
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
		NotifyAccessibilityScrollChanged();
	}
	if (_useGeneratedItemContainers)
		(void)RefreshGeneratedItemContainers(false);
	InvalidateVisual();
	return true;
}

bool TreeView::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	if (!Root || !Enable) return false;
	EnsureAccessibilityIndex();
	const auto indexed = _accessibilityNodeIndex.find(id);
	return indexed != _accessibilityNodeIndex.end()
		&& BringNodeIntoView(*indexed->second.Node, true);
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
		if (_useGeneratedItemContainers)
			(void)RefreshGeneratedItemContainers(false);
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
		if (_useGeneratedItemContainers)
			(void)RefreshGeneratedItemContainers(false);
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
	if (_useGeneratedItemContainers)
		(void)RefreshGeneratedItemContainers(false);
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
		_hasExpansionAnimation = false;
		return false;
	}
	_hasExpansionAnimation = this->Root && this->Root->IsAnimationRunning();
	return _hasExpansionAnimation;
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
			if (_useGeneratedItemContainers)
				(void)RefreshGeneratedItemContainers(false);
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
		if (_useGeneratedItemContainers)
			(void)RefreshGeneratedItemContainers(false);
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
	// Rebuilds may happen inside a data-source notification. Retain the old
	// controls until the next frame so callbacks already copied by that event
	// cannot observe destroyed binding targets.
	_retiredDataRoots->clear();
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
			if (_hasExpansionAnimation)
				_hasExpansionAnimation = Root && Root->IsAnimationRunning();
			if (_hasExpansionAnimation)
				_contentRenderItems = measureNodes(this->Root->Children);
			else
			{
				EnsureAccessibilityVisibleIndex();
				_contentRenderItems = static_cast<float>(
					_accessibilityVisibleNodes.size());
			}
			this->MaxRenderItems = (int)std::ceil(_contentRenderItems);
			int maxScroll = (int)std::ceil(_contentRenderItems - ((float)this->Height / itemHeight));
			if (maxScroll < 0)maxScroll = 0;
			if (this->ScrollIndex > maxScroll)
			{
				this->ScrollIndex = maxScroll;
				this->ScrollChanged(this);
				this->NotifyAccessibilityScrollChanged();
			}
			if (_useGeneratedItemContainers)
			{
				(void)RefreshGeneratedItemContainers(false);
				UpdateGeneratedItemStates();
			}
			if (_hasExpansionAnimation)
			{
				float cursorY = 0.0f;
				renderNodes(this, d2d, actualWidth, actualHeight,
					itemHeight, static_cast<float>(ScrollIndex) * itemHeight,
					cursorY, 0, Root->Children);
			}
			else RenderStableVisibleNodes(
				d2d, actualWidth, actualHeight, itemHeight);
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
bool TreeView::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return Control::HandlesNavigationKey(key);
	}
}

bool TreeView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	if (message == WM_KEYDOWN && HandlesNavigationKey(wParam))
	{
		EnsureAccessibilityVisibleIndex();
		TreeNode* target = nullptr;
		auto current = SelectedNode
			? _accessibilityVisibleIndex.find(SelectedNode)
			: _accessibilityVisibleIndex.end();
		if (SelectedNode && current == _accessibilityVisibleIndex.end())
		{
			(void)BringNodeIntoView(*SelectedNode, true);
			EnsureAccessibilityVisibleIndex();
			current = _accessibilityVisibleIndex.find(SelectedNode);
		}
		const size_t count = _accessibilityVisibleNodes.size();
		const bool hasCurrent = current != _accessibilityVisibleIndex.end();
		const size_t currentIndex = hasCurrent ? current->second : 0;
		const float itemHeight = EffectiveItemHeight(this);
		const size_t page = static_cast<size_t>((std::max)(1,
			static_cast<int>(std::floor(static_cast<float>(Height)
				/ (std::max)(itemHeight, 1.0f)))));

		switch (wParam)
		{
		case VK_UP:
			if (count) target = _accessibilityVisibleNodes[
				hasCurrent ? (currentIndex == 0 ? 0 : currentIndex - 1)
					: count - 1].first;
			break;
		case VK_DOWN:
			if (count) target = _accessibilityVisibleNodes[
				hasCurrent ? (std::min)(currentIndex + 1, count - 1) : 0].first;
			break;
		case VK_HOME:
			if (count) target = _accessibilityVisibleNodes.front().first;
			break;
		case VK_END:
			if (count) target = _accessibilityVisibleNodes.back().first;
			break;
		case VK_PRIOR:
			if (count) target = _accessibilityVisibleNodes[
				hasCurrent && currentIndex > page ? currentIndex - page : 0].first;
			break;
		case VK_NEXT:
			if (count) target = _accessibilityVisibleNodes[
				hasCurrent
					? (std::min)(currentIndex + page, count - 1) : 0].first;
			break;
		case VK_RIGHT:
			if (!SelectedNode)
			{
				if (count) target = _accessibilityVisibleNodes.front().first;
			}
			else if (SelectedNode->HasItems())
			{
				if (!SelectedNode->Expand)
					SelectedNode->SetExpanded(
						true, AreSystemAnimationsEnabled());
				else
				{
					(void)EnsureDataChildrenMaterialized(*SelectedNode);
					if (!SelectedNode->Children.empty())
						target = SelectedNode->Children.front();
				}
			}
			break;
		case VK_LEFT:
			if (!SelectedNode)
			{
				if (count) target = _accessibilityVisibleNodes.front().first;
			}
			else if (SelectedNode->Expand)
				SelectedNode->SetExpanded(
					false, AreSystemAnimationsEnabled());
			else if (SelectedNode->_parentNode
				&& SelectedNode->_parentNode != Root)
				target = SelectedNode->_parentNode;
			break;
		}
		if (target) (void)ApplySelection(target, true);
		KeyEventArgs eventArgs = KeyEventArgs(static_cast<Keys>(wParam));
		OnKeyDown(this, eventArgs);
		InvalidateVisual();
		return true;
	}
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
		const int renderItemCount = itemHeight > 0.0f ? (std::max)(1, (int)((float)this->Height / itemHeight)) : 1;
		int maxScroll = (int)std::ceil(_contentRenderItems - (float)renderItemCount);
		if (maxScroll < 0) maxScroll = 0;
		if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
		{
			if (this->ScrollIndex < maxScroll)
			{
				this->ScrollIndex += 1;
				this->ScrollChanged(this);
				if (_useGeneratedItemContainers)
					(void)RefreshGeneratedItemContainers(false);
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
				if (_useGeneratedItemContainers)
					(void)RefreshGeneratedItemContainers(false);
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
					UpdateGeneratedItemStates();
					this->InvalidateVisual();
				}
			}
			else
			{
				bool isHit = false;
				auto newHoveredNode = HitTestNodeCore(
					static_cast<float>(localX), static_cast<float>(localY),
					nullptr, &isHit);
				bool needUpdate = this->HoveredNode != newHoveredNode;
				this->HoveredNode = newHoveredNode;
				if (needUpdate)
				{
					UpdateGeneratedItemStates();
					this->InvalidateVisual();
				}
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
						const float moveSpace = (std::max)(0.0f, height - thumbH);
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
				bool isHit = false;
				auto node = HitTestNodeCore(
					static_cast<float>(localX), static_cast<float>(localY),
					nullptr, &isHit);
				if (node)
				{
					if (isHit)
					{
						node->SetExpanded(!node->Expand, AreSystemAnimationsEnabled());
						NotifyAccessibilityStructureChanged();
						NotifyAccessibilityScrollChanged();
					}
					else
						(void)ApplySelection(node, false);
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
		bool isHit = false;
		auto node = HitTestNodeCore(
			static_cast<float>(localX), static_cast<float>(localY),
			nullptr, &isHit);
		if (node)
		{
			if (node->HasItems())
			{
				node->SetExpanded(!node->Expand, AreSystemAnimationsEnabled());
				NotifyAccessibilityStructureChanged();
				NotifyAccessibilityScrollChanged();
			}
			if (!isHit)
				(void)ApplySelection(node, false);
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
