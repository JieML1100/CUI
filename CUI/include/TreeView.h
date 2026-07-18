#pragma once
#include "Control.h"
#include "ObservableCollection.h"
#include <unordered_map>

class TreeView;

enum class TreeViewDropPosition : uint8_t
{
	None,
	Before,
	Inside,
	After
};

/**
 * @file TreeView.h
 * @brief TreeView：树形控件（节点展开/收起、选择、滚动）。
 */

/**
 * @brief 树节点。
 *
 * 所有权：Children 中仍存在的节点由该节点拥有；~TreeNode 会释放它们。
 * 优先使用 AddChild/DetachChildAt/RemoveChild/ClearChildren 明确转移所有权。
 * 直接 erase 的节点会从树中分离，但其释放责任转交给调用方。
 */
class TreeNode
{
private:
	TreeView* _ownerTree = nullptr;
	TreeNode* _parentNode = nullptr;
	std::vector<TreeNode*> _observedChildren;
	void AttachOwner(TreeView* owner);
	void OnChildrenChanged(const CollectionChangedEventArgs& change);
	bool CanAdopt(const TreeNode* child) const;
	friend class TreeView;
public:
	using ChildCollection = ObservableCollection<TreeNode*>;
	mutable uint32_t AccessibilityId = 0;
	ULONG64 Tag = 0;
	std::shared_ptr<BitmapSource> Image;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> ImageCache;
	ID2D1RenderTarget* ImageCacheTarget = nullptr;
	const BitmapSource* ImageCacheSource = nullptr;
	std::wstring Text = L"";
	ChildCollection Children;
	bool Expand = false;
	float ExpandProgress = 0.0f;
	float AnimStartProgress = 0.0f;
	float AnimTargetProgress = 0.0f;
	ULONGLONG AnimStartTick = 0;
	UINT AnimDurationMs = 200;
	bool Animating = false;
	TreeNode(std::wstring text, std::shared_ptr<BitmapSource> image = nullptr);
	/** @brief 添加子节点并接管所有权；无效节点或环形关系会被拒绝。 */
	TreeNode* AddChild(TreeNode* child);
	TreeNode* AddChild(std::unique_ptr<TreeNode> child);
	/** @brief 分离子节点并把所有权交给调用方。 */
	std::unique_ptr<TreeNode> DetachChildAt(size_t index);
	/** @brief 删除指定子节点。 */
	bool RemoveChild(TreeNode* child);
	bool RemoveChildAt(size_t index);
	/** @brief 删除全部子节点。 */
	void ClearChildren();
	ID2D1Bitmap* GetImageBitmap(D2DGraphics* render);
	float CurrentExpandProgress();
	void SetExpanded(bool expanded, bool animate = true);
	bool IsAnimationRunning();
	float AnimatedVisibleCount();
	~TreeNode();
	/** @brief 展开状态下可渲染的总节点数量（含子树）。 */
	int UnfoldedCount();
};

/**
 * @brief TreeView 控件。
 *
 * - Root 为根节点（通常不显示或作为顶层容器节点）
 * - SelectedNode/HoveredNode 表示当前选择/悬停节点
 * - ScrollIndex 为滚动到的起始可见节点索引（按展开后的线性序列）
 */
class TreeView : public Control, public IAccessibilityVirtualizedControl
{
private:
	struct AccessibilityNodeIndex
	{
		TreeNode* Node = nullptr;
		TreeNode* Parent = nullptr;
		size_t SiblingIndex = 0;
		int Level = 0;
	};
	bool isDraggingScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	float _contentRenderItems = 0.0f;
	std::unordered_map<uint32_t, AccessibilityNodeIndex> _accessibilityNodeIndex;
	std::unordered_map<uint32_t, std::vector<TreeNode*>>
		_accessibilityChildrenByParentId;
	std::vector<std::pair<TreeNode*, int>> _accessibilityVisibleNodes;
	std::unordered_map<TreeNode*, size_t> _accessibilityVisibleIndex;
	size_t _accessibilityVisibleCount = 0;
	bool _accessibilityIndexDirty = true;
	bool _accessibilityVisibleDirty = true;
	void UpdateScrollDrag(float posY);
	void DrawScroll();
	void InvalidateAccessibilityIndex(bool structure) noexcept;
	void EnsureAccessibilityIndex();
	void EnsureAccessibilityVisibleIndex();
	void OnNodeChildrenChanged(
		TreeNode* parent, const CollectionChangedEventArgs& change);
	friend class TreeNode;
public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 根节点（所有权由 TreeView 管理，见实现）。 */
	TreeNode* Root = nullptr;
	/** @brief 当前选中节点。 */
	TreeNode* SelectedNode = nullptr;
	/** @brief 当前悬停节点。 */
	TreeNode* HoveredNode = nullptr;
	/** @brief 可选的拖放目标；仅用于瞬态呈现，不改变节点集合。 */
	TreeNode* DropTargetNode = nullptr;
	TreeViewDropPosition DropPosition = TreeViewDropPosition::None;
	int MaxRenderItems = 0;
	int ScrollIndex = 0;
	float ItemHeight = 28.0f;
	float IndentWidth = 20.0f;
	float ItemHorizontalPadding = 8.0f;
	float ItemVerticalPadding = 3.0f;
	float ItemCornerRadius = 6.0f;
	float SelectedAccentWidth = 3.0f;
	float ChevronSize = 10.0f;
	float TextLeftSpacing = 7.0f;
	D2D1_COLOR_F ScrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F ScrollForeColor = cui::theme::palette::ScrollThumb;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F SelectedBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F UnderMouseItemBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F SelectedForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F DropIndicatorColor = cui::theme::palette::Accent;
	ScrollChangedEvent ScrollChanged;
	SelectionChangedEvent SelectionChanged;
	TreeView(int x, int y, int width = 120, int height = 24);
	~TreeView();
	void GetAccessibilityVirtualChildren(
		uint32_t parentId, std::vector<uint32_t>& result) override;
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
	bool SetAccessibilityVirtualNodeExpanded(uint32_t id, bool expanded) override;
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id) override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	/** Returns the visible row under a local point and optionally its 0..1 row position. */
	TreeNode* HitTestNode(float localX, float localY, float* relativeRowY = nullptr);
	void SetDropTarget(TreeNode* node, TreeViewDropPosition position);
	void ClearDropTarget();

protected:
	void OnComputedLayoutSizeChanged() override;
};
