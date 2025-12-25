#pragma once
#include "Control.h"
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
	int UnfoldedCount();
};
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
	TreeNode* Root = NULL;
	TreeNode* SelectedNode = NULL;
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

