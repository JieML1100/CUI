#pragma once

/**
 * @file DesignerCanvas.h
 * @brief DesignerCanvas：设计器画布与控件选中/拖拽交互实现。
 */
#include "../CUI/GUI/Panel.h"
#include "DesignerTypes.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>

class DesignerCanvas : public Panel
{
private:
	Panel* _designSurface = nullptr;
	Panel* _clientSurface = nullptr;
	std::wstring _designedFormName = L"MainForm";
	std::wstring _designedFormText = L"Form";
	SIZE _designedFormSize = { 800, 600 };
	POINT _designedFormLocation = { 100, 100 };
	D2D1_COLOR_F _designedFormBackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F _designedFormForeColor = Colors::Black;
	// 窗体默认字体：空表示使用框架默认字体名（GetDefaultFontObject）
	std::wstring _designedFormFontName;
	float _designedFormFontSize = 18.0f;
	// 共享字体对象：用于让“默认字体”的控件跟随窗体字体（控件 SetFontEx(..., false)）。
	::Font* _designedFormSharedFont = nullptr;
	// 已被替换但暂未释放的共享字体（避免 UAF：某些复合控件/缓存可能短时间仍引用旧指针）
	std::vector<::Font*> _retiredDesignedFormSharedFonts;
	bool _designedFormShowInTaskBar = true;
	bool _designedFormTopMost = false;
	bool _designedFormEnable = true;
	bool _designedFormVisible = true;
	std::map<std::wstring, std::wstring> _designedFormEventHandlers;
	bool _designedFormVisibleHead = true;
	int _designedFormHeadHeight = 24;
	bool _designedFormMinBox = true;
	bool _designedFormMaxBox = true;
	bool _designedFormCloseBox = true;
	bool _designedFormCenterTitle = true;
	bool _designedFormAllowResize = true;
	POINT _designSurfaceOrigin = { 20, 20 };

	// 网格/吸附/参考线
	int _gridSize = 10;
	bool _snapToGrid = true;
	bool _snapToGuides = true;
	int _snapThreshold = 5; // 像素阈值：接近则吸附
	std::vector<int> _vGuides; // canvas 坐标
	std::vector<int> _hGuides; // canvas 坐标

	std::vector<std::shared_ptr<DesignerControl>> _designerControls;
	std::vector<std::shared_ptr<DesignerControl>> _selectedControls;
	std::shared_ptr<DesignerControl> _selectedControl; // primary selection

	// 框选状态（rubber band）
	bool _isBoxSelecting = false;
	POINT _boxSelectStart = { 0,0 };
	RECT _boxSelectRect = { 0,0,0,0 };
	bool _boxSelectAddToSelection = false; // Shift+框选：在现有选择上追加
	
	// 拖拽状态
	bool _isDragging = false;
	bool _dragHasMoved = false;
	bool _dragLiftedToRoot = false;
	int _dragStartThreshold = 2; // 像素阈值：超过则视为真正拖拽（避免单击触发换容器/重排）
	POINT _dragStartPoint = {0, 0};
	RECT _dragStartRectInCanvas = { 0,0,0,0 };
	DWORD _lastPropSyncTick = 0;

	struct DragStartItem
	{
		Control* ControlInstance = nullptr;
		Control* Parent = nullptr;
		RECT StartRectInCanvas{ 0,0,0,0 };
		POINT StartLocation{ 0,0 };
		Thickness StartMargin{};
		bool UsesRelativeMargin = false;
	};
	std::vector<DragStartItem> _dragStartItems;
	
	// 调整大小状态
	bool _isResizing = false;
	DesignerControl::ResizeHandle _resizeHandle = DesignerControl::ResizeHandle::None;
	RECT _resizeStartRect = {0, 0, 0, 0};
	
	// 待添加的控件类型
	UIClass _controlToAdd = UIClass::UI_Base;
	std::unordered_map<int, int> _controlTypeCounters;

	std::wstring GenerateDefaultControlName(UIClass type, const std::wstring& typeName);
	void UpdateDefaultNameCounterFromName(UIClass type, const std::wstring& name);
	
	void DrawSelectionHandles(std::shared_ptr<DesignerControl> dc);
	void DrawGrid();
	RECT GetDesignSurfaceRectInCanvas() const;
	RECT GetClientSurfaceRectInCanvas() const;
	bool IsPointInDesignSurface(POINT ptCanvas) const;
	RECT ClampRectToBounds(RECT r, const RECT& bounds, bool keepSize) const;
	bool TryHandleTabHeaderClick(POINT ptCanvas);
	int DesignedClientTop() const { return (_designedFormVisibleHead && _designedFormHeadHeight > 0) ? _designedFormHeadHeight : 0; }
	void UpdateClientSurfaceLayout();
	
	std::shared_ptr<DesignerControl> HitTestControl(POINT pt);
	CursorKind GetResizeCursor(DesignerControl::ResizeHandle handle);
	void ClearSelection();
	bool IsSelected(const std::shared_ptr<DesignerControl>& dc) const;
	void SetPrimarySelection(const std::shared_ptr<DesignerControl>& dc, bool fireEvent);
	void AddToSelection(const std::shared_ptr<DesignerControl>& dc, bool setPrimary, bool fireEvent);
	void ToggleSelection(const std::shared_ptr<DesignerControl>& dc, bool fireEvent);
	RECT GetSelectionBoundsInCanvas() const;
	void BeginDragFromCurrentSelection(POINT mousePos);
	bool IsLayoutContainer(Control* c) const;
	void LiftSelectedToRootForDrag();
	void ApplyMoveDeltaToSelection(int dx, int dy);
	void NotifySelectionChangedThrottled();
	void ClearAlignmentGuides();
	void AddVGuide(int xCanvas);
	void AddHGuide(int yCanvas);
	RECT ApplyMoveSnap(RECT desiredRectInCanvas, Control* referenceParent);
	RECT ApplyResizeSnap(RECT desiredRectInCanvas, Control* referenceParent, DesignerControl::ResizeHandle handle);
	void ApplyRectToControl(Control* c, const RECT& rectInCanvas);
	static Thickness GetPaddingOfContainer(Control* container);
	void RebuildDesignedFormSharedFont();
	
public:
	DesignerCanvas(int x, int y, int width, int height);
	virtual ~DesignerCanvas();
	bool HitTestChildren() const override { return false; }

	// 当外部（属性面板）修改 Name 后，同步默认命名计数器（按类型）。
	void SyncDefaultNameCounter(UIClass type, const std::wstring& name) { UpdateDefaultNameCounterFromName(type, name); }

	std::wstring GetDesignedFormName() const { return _designedFormName; }
	void SetDesignedFormName(const std::wstring& n) { _designedFormName = n; }
	std::wstring GetDesignedFormText() const { return _designedFormText; }
	void SetDesignedFormText(const std::wstring& t) { _designedFormText = t; this->PostRender(); }
	D2D1_COLOR_F GetDesignedFormBackColor() const { return _designedFormBackColor; }
	void SetDesignedFormBackColor(D2D1_COLOR_F c) { _designedFormBackColor = c; if (_clientSurface) _clientSurface->BackColor = c; this->PostRender(); }
	D2D1_COLOR_F GetDesignedFormForeColor() const { return _designedFormForeColor; }
	void SetDesignedFormForeColor(D2D1_COLOR_F c) { _designedFormForeColor = c; this->PostRender(); }
	std::wstring GetDesignedFormFontName() const { return _designedFormFontName; }
	float GetDesignedFormFontSize() const { return _designedFormFontSize; }
	::Font* GetDesignedFormSharedFont() const { return _designedFormSharedFont; }
	void SetDesignedFormFontName(const std::wstring& name);
	void SetDesignedFormFontSize(float size);
	bool GetDesignedFormShowInTaskBar() const { return _designedFormShowInTaskBar; }
	void SetDesignedFormShowInTaskBar(bool v) { _designedFormShowInTaskBar = v; }
	bool GetDesignedFormTopMost() const { return _designedFormTopMost; }
	void SetDesignedFormTopMost(bool v) { _designedFormTopMost = v; }
	bool GetDesignedFormEnable() const { return _designedFormEnable; }
	void SetDesignedFormEnable(bool v) { _designedFormEnable = v; }
	bool GetDesignedFormVisible() const { return _designedFormVisible; }
	void SetDesignedFormVisible(bool v) { _designedFormVisible = v; }
	const std::map<std::wstring, std::wstring>& GetDesignedFormEventHandlers() const { return _designedFormEventHandlers; }
	bool GetDesignedFormEventEnabled(const std::wstring& eventName) const { return _designedFormEventHandlers.find(eventName) != _designedFormEventHandlers.end(); }
	void SetDesignedFormEventEnabled(const std::wstring& eventName, bool enabled)
	{
		if (enabled) _designedFormEventHandlers[eventName] = L"1";
		else _designedFormEventHandlers.erase(eventName);
	}
	SIZE GetDesignedFormSize() const { return _designedFormSize; }
	void SetDesignedFormSize(SIZE s);
	POINT GetDesignedFormLocation() const { return _designedFormLocation; }
	void SetDesignedFormLocation(POINT p) { _designedFormLocation = p; }
	bool GetDesignedFormVisibleHead() const { return _designedFormVisibleHead; }
	void SetDesignedFormVisibleHead(bool v) { _designedFormVisibleHead = v; UpdateClientSurfaceLayout(); this->PostRender(); }
	int GetDesignedFormHeadHeight() const { return _designedFormHeadHeight; }
	void SetDesignedFormHeadHeight(int h) { _designedFormHeadHeight = h; if (_designedFormHeadHeight < 0) _designedFormHeadHeight = 0; UpdateClientSurfaceLayout(); this->PostRender(); }
	bool GetDesignedFormMinBox() const { return _designedFormMinBox; }
	void SetDesignedFormMinBox(bool v) { _designedFormMinBox = v; this->PostRender(); }
	bool GetDesignedFormMaxBox() const { return _designedFormMaxBox; }
	void SetDesignedFormMaxBox(bool v) { _designedFormMaxBox = v; this->PostRender(); }
	bool GetDesignedFormCloseBox() const { return _designedFormCloseBox; }
	void SetDesignedFormCloseBox(bool v) { _designedFormCloseBox = v; this->PostRender(); }
	bool GetDesignedFormCenterTitle() const { return _designedFormCenterTitle; }
	void SetDesignedFormCenterTitle(bool v) { _designedFormCenterTitle = v; this->PostRender(); }
	bool GetDesignedFormAllowResize() const { return _designedFormAllowResize; }
	void SetDesignedFormAllowResize(bool v) { _designedFormAllowResize = v; this->PostRender(); }
	void ClampControlToDesignSurface(Control* c);
	// 设计器专用：切换 Anchor 时保持控件当前视觉矩形不变，并同步换算 Margin
	void ApplyAnchorStylesKeepingBounds(Control* c, uint8_t newAnchorStyles);

	// 设计文件（用于保存/加载设计进度）
	bool SaveDesignFile(const std::wstring& filePath, std::wstring* outError = nullptr) const;
	bool LoadDesignFile(const std::wstring& filePath, std::wstring* outError = nullptr);
	
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	
	// 控件管理
	void AddControlToCanvas(UIClass type, POINT canvasPos);
	void DeleteSelectedControl();
	void ClearCanvas();
	void RemoveDesignerControlsInSubtree(Control* root);
	// 设计期 Name：用于保存/加载 parent 引用，必须非空且在当前文档内唯一
	std::wstring MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const;
	std::shared_ptr<DesignerControl> GetSelectedControl() { return _selectedControl; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetSelectedControls() const { return _selectedControls; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetAllControls() const { return _designerControls; }
	std::vector<std::shared_ptr<DesignerControl>> GetAllControlsForExport() const;
	
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
