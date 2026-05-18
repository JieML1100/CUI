#pragma once
#include "Control.h"

/**
 * @file TreeView.h
 * @brief TreeView：树形控件（节点展开/收起、选择、滚动）。
 */

/**
 * @brief 树节点。
 *
 * 所有权：Children 由该节点拥有；~TreeNode 会释放所有子节点。
 */
class TreeNode
{
public:
	ULONG64 Tag = NULL;
	std::shared_ptr<BitmapSource> Image;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> ImageCache;
	ID2D1RenderTarget* ImageCacheTarget = nullptr;
	const BitmapSource* ImageCacheSource = nullptr;
	std::wstring Text = L"";
	std::vector<TreeNode*> Children;
	bool Expand = false;
	float ExpandProgress = 0.0f;
	float AnimStartProgress = 0.0f;
	float AnimTargetProgress = 0.0f;
	ULONGLONG AnimStartTick = 0;
	UINT AnimDurationMs = 200;
	bool Animating = false;
	TreeNode(std::wstring text, std::shared_ptr<BitmapSource> image = nullptr);
	ID2D1Bitmap* GetImageBitmap(D2DGraphics* render);
	float CurrentExpandProgress();
	void SetExpanded(bool expanded);
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
class TreeView : public Control
{
private:
	bool isDraggingScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	float _contentRenderItems = 0.0f;
	void UpdateScrollDrag(float posY);
	void DrawScroll();
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 根节点（所有权由 TreeView 管理，见实现）。 */
	TreeNode* Root = NULL;
	/** @brief 当前选中节点。 */
	TreeNode* SelectedNode = NULL;
	/** @brief 当前悬停节点。 */
	TreeNode* HoveredNode = NULL;
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
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_COLOR_F AccentColor = { 0.3882f, 0.4000f, 0.9451f, 1.0f };
	D2D1_COLOR_F SelectedBackColor = { 0.3882f, 0.4000f, 0.9451f, 0.14f };
	D2D1_COLOR_F UnderMouseItemBackColor = { 0.3882f, 0.4000f, 0.9451f, 0.08f };
	D2D1_COLOR_F SelectedForeColor = Colors::Black;
	ScrollChangedEvent ScrollChanged;
	SelectionChangedEvent SelectionChanged;
	TreeView(int x, int y, int width = 120, int height = 24);
	~TreeView();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
