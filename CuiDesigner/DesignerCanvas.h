#pragma once
#include "../CUI/GUI/Panel.h"
#include "DesignerTypes.h"
#include <vector>
#include <memory>

class DesignerCanvas : public Panel
{
private:
	std::vector<std::shared_ptr<DesignerControl>> _designerControls;
	std::shared_ptr<DesignerControl> _selectedControl;
	
	// 拖拽状态
	bool _isDragging = false;
	POINT _dragStartPoint = {0, 0};
	POINT _controlStartLocation = {0, 0};
	
	// 调整大小状态
	bool _isResizing = false;
	DesignerControl::ResizeHandle _resizeHandle = DesignerControl::ResizeHandle::None;
	RECT _resizeStartRect = {0, 0, 0, 0};
	
	// 待添加的控件类型
	UIClass _controlToAdd = UIClass::UI_Base;
	int _controlCounter = 0;
	
	void DrawSelectionHandles(std::shared_ptr<DesignerControl> dc);
	void DrawGrid();
	
	std::shared_ptr<DesignerControl> HitTestControl(POINT pt);
	CursorKind GetResizeCursor(DesignerControl::ResizeHandle handle);
	
public:
	DesignerCanvas(int x, int y, int width, int height);
	virtual ~DesignerCanvas();
	bool HitTestChildren() const override { return false; }
	
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	
	// 控件管理
	void AddControlToCanvas(UIClass type, POINT canvasPos);
	void DeleteSelectedControl();
	void ClearCanvas();
	void RemoveDesignerControlsInSubtree(Control* root);
	std::shared_ptr<DesignerControl> GetSelectedControl() { return _selectedControl; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetAllControls() const { return _designerControls; }
	
	// 准备添加控件（鼠标模式）
	void SetControlToAdd(UIClass type) { _controlToAdd = type; }
	
	Event<void(std::shared_ptr<DesignerControl>)> OnControlSelected;

private:
	RECT GetControlRectInCanvas(Control* c);
	std::vector<RECT> GetHandleRectsFromRect(const RECT& r, int handleSize);
	DesignerControl::ResizeHandle HitTestHandleFromRect(const RECT& r, POINT pt, int handleSize);
	Control* FindBestContainerAtPoint(POINT ptCanvas, Control* ignore);
	bool IsContainerControl(Control* c);
	bool IsDescendantOf(Control* ancestor, Control* node);
	Control* NormalizeContainerForDrop(Control* container);
	POINT CanvasToContainerPoint(POINT ptCanvas, Control* container);
	void TryReparentSelectedAfterDrag();
	void DeleteControlRecursive(Control* c);
};
