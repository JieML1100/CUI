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
	ID2D1Bitmap* Image = NULL;
	std::wstring Text = L"";
	List<TreeNode*> Children;
	bool Expand = false; 
	TreeNode(std::wstring text, ID2D1Bitmap* image = NULL);
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
	void UpdateScrollDrag(float posY);
	void DrawScroll();
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	/** @brief 根节点（所有权由 TreeView 管理，见实现）。 */
	TreeNode* Root = NULL;
	/** @brief 当前选中节点。 */
	TreeNode* SelectedNode = NULL;
	/** @brief 当前悬停节点。 */
	TreeNode* HoveredNode = NULL;
	int MaxRenderItems = 0;
	int ScrollIndex = 0;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F UnderMouseItemBackColor = { 0.5961f , 0.9608f , 1.f , 0.5f };
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	ScrollChangedEvent ScrollChanged;
	SelectionChangedEvent SelectionChanged;
	TreeView(int x, int y, int width = 120, int height = 24);
	~TreeView();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};

