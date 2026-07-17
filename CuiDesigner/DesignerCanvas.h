#pragma once

/**
 * @file DesignerCanvas.h
 * @brief DesignerCanvas：设计器画布与控件选中/拖拽交互实现。
 */
#include "../CUI/include/Panel.h"
#include "DesignerTypes.h"
#include "DesignerStyleSheet.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerCore/DesignerDocumentTransaction.h"
#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <optional>

struct CodeGenInput;
class IDesignerCommand;
class DesignerCommandCoordinator;
class SelectionService;
class SplitContainer;
class ControlPlacementCommand;
class ControlSubtreeCommand;
class ControlOwnedCollectionCommand;
struct DesignerCanvasPlacementInteraction;
struct DesignerCanvasPropertyInteraction;

namespace DesignerModel
{
	struct DesignDocument;
	class DesignDocumentEventIndex;
	struct DesignFormModel;
}

struct DesignerCanvasInteractionTransactionEventArgs
{
	std::wstring Operation;
	std::wstring Message;
	DesignerDocumentTransactionResult Result;
};

struct DesignerCanvasCommandEventArgs
{
	std::wstring Operation;
	std::wstring Label;
	std::wstring Message;
	DesignerDocumentTransactionResult Result;
};

struct DesignerCanvasDocumentStateEventArgs
{
	uint64_t CurrentStateId = 0;
	uint64_t SavedStateId = 0;
	bool IsDirty = false;
};

class DesignerCanvas : public Panel
{
friend class DesignerCommandCoordinator;
friend class ControlPlacementCommand;
friend class ControlSubtreeCommand;
friend class ControlOwnedCollectionCommand;

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
	DesignerDataContextSchema _dataContextSchema;
	DesignerModel::DesignCodeBehindModel _codeBehind;
	DesignerStyleSheet _documentStyleSheet;
	std::shared_ptr<ControlStyleSheet> _previewStyleSheet;
	std::shared_ptr<IBindingSource> _designDataContext;
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
	std::unique_ptr<SelectionService> _selectionService;

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

	// SplitContainer 分隔条拖拽状态
	bool _isSplitterDragging = false;
	POINT _splitterDragStartPoint = { 0, 0 };
	int _splitterDragStartDistance = 0;
	SplitContainer* _splitterDragTarget = nullptr;
	
	// 待添加的完整控件描述；自定义控件不能退化为单一 UIClass。
	std::optional<DesignerControlDescriptor> _controlToAdd;
	// 进程内预览工厂不进入文档，但必须跨 Open/Undo/Redo 材质化保持。
	std::unordered_map<std::wstring, DesignerControlDescriptor>
		_customControlDescriptors;
	std::unordered_map<int, int> _controlTypeCounters;
	// 单调递增且随文档持久化；删除控件不会回收 ID。
	int _nextStableControlId = 1;
	std::unique_ptr<DesignerCommandCoordinator> _commandCoordinator;
	std::unique_ptr<DesignerCanvasPlacementInteraction>
		_activePlacementInteraction;
	std::unique_ptr<DesignerCanvasPropertyInteraction>
		_activePropertyInteraction;
	std::wstring _activeInteractionTransaction;
	std::wstring _lastInteractionTransaction;
	DesignerDocumentTransactionResult _lastInteractionTransactionResult =
		DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	bool _hasInteractionTransactionResult = false;
	std::wstring _lastCommandOperation;
	std::wstring _lastCommandLabel;
	DesignerDocumentTransactionResult _lastCommandResult =
		DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	bool _hasCommandResult = false;

	std::wstring GenerateDefaultControlName(UIClass type, const std::wstring& typeName);
	void UpdateDefaultNameCounterFromName(UIClass type, const std::wstring& name);
	int AllocateStableControlId();
	
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
	std::shared_ptr<DesignerControl> HitTestSplitContainerSplitter(POINT pt) const;
	CursorKind GetResizeCursor(DesignerControl::ResizeHandle handle);
	CursorKind GetSplitContainerSplitterCursor(SplitContainer* split) const;
	RECT GetSplitContainerSplitterRectInCanvas(SplitContainer* split) const;
	int ClampSplitContainerDistance(SplitContainer* split, int value) const;
	bool UpdateSplitContainerPreview(
		SplitContainer* split,
		int splitterDistance,
		std::wstring* outError = nullptr);
	bool HasActiveDeltaInteraction() const noexcept;
	bool HasVisibleDesignerAncestors(Control* control) const noexcept;
	bool BeginCanvasInteractionTransaction(const std::wstring& operation);
	bool BeginPlacementInteraction(const std::wstring& operation);
	bool BeginControlPropertyInteraction(
		const std::wstring& operation,
		const std::shared_ptr<DesignerControl>& target,
		const std::wstring& propertyName);
	DesignerDocumentTransactionResult CommitCanvasInteractionTransaction();
	DesignerDocumentTransactionResult CommitPlacementInteraction();
	DesignerDocumentTransactionResult CommitControlPropertyInteraction();
	DesignerDocumentTransactionResult RollbackCanvasInteractionTransaction(
		std::wstring message = {});
	DesignerDocumentTransactionResult RollbackPlacementInteraction(
		std::wstring message = {});
	DesignerDocumentTransactionResult RollbackControlPropertyInteraction(
		std::wstring message = {});
	DesignerDocumentTransactionResult AbortCanvasInteractionTransaction(
		std::wstring error);
	DesignerDocumentTransactionResult AbortPlacementInteraction(
		std::wstring error);
	DesignerDocumentTransactionResult AbortControlPropertyInteraction(
		std::wstring error);
	void PublishCanvasInteractionTransactionResult(
		const std::wstring& operation,
		const DesignerDocumentTransactionResult& result,
		std::wstring message = {});
	void PublishCanvasCommandResult(
		const std::wstring& operation,
		const std::wstring& label,
		const DesignerDocumentTransactionResult& result,
		std::wstring message = {});
	void NotifyDocumentStateChanged();
	DesignerDocumentTransactionResult ExecuteCommandCore(
		std::unique_ptr<IDesignerCommand> command);
	std::wstring CurrentPointerInteractionOperation() const;
	void ResetPointerInteractionState();
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
	std::vector<std::wstring> CaptureSelectionNames() const;
	void NotifySelectionChangedThrottled();
	void ClearAlignmentGuides();
	void AddVGuide(int xCanvas);
	void AddHGuide(int yCanvas);
	RECT ApplyMoveSnap(RECT desiredRectInCanvas, Control* referenceParent);
	RECT ApplyResizeSnap(RECT desiredRectInCanvas, Control* referenceParent, DesignerControl::ResizeHandle handle);
	void ApplyRectToControl(Control* c, const RECT& rectInCanvas);
	static Thickness GetPaddingOfContainer(Control* container);
	void RebuildDesignedFormSharedFont();
	void DetachDesignBindingPreview(DesignerControl& control);
	
public:
	DesignerCanvas(int x, int y, int width, int height);
	virtual ~DesignerCanvas();
	bool HitTestChildren() const override { return false; }
	DesignerModel::DesignFormModel CaptureDesignedFormModel() const;
	void ApplyDesignedFormModel(const DesignerModel::DesignFormModel& form);

	// 当外部（属性面板）修改 Name 后，同步默认命名计数器（按类型）。
	void SyncDefaultNameCounter(UIClass type, const std::wstring& name) { UpdateDefaultNameCounterFromName(type, name); }

	std::wstring GetDesignedFormName() const { return _designedFormName; }
	void SetDesignedFormName(const std::wstring& n) { _designedFormName = n; }
	std::wstring GetDesignedFormText() const { return _designedFormText; }
	void SetDesignedFormText(const std::wstring& t) { _designedFormText = t; this->InvalidateVisual(); }
	D2D1_COLOR_F GetDesignedFormBackColor() const { return _designedFormBackColor; }
	void SetDesignedFormBackColor(D2D1_COLOR_F c) { _designedFormBackColor = c; if (_clientSurface) _clientSurface->BackColor = c; this->InvalidateVisual(); }
	D2D1_COLOR_F GetDesignedFormForeColor() const { return _designedFormForeColor; }
	void SetDesignedFormForeColor(D2D1_COLOR_F c) { _designedFormForeColor = c; this->InvalidateVisual(); }
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
	const DesignerDataContextSchema& GetDataContextSchema() const { return _dataContextSchema; }
	bool SetDataContextSchema(DesignerDataContextSchema schema, std::wstring* outError = nullptr);
	const DesignerModel::DesignCodeBehindModel& GetCodeBehind() const noexcept
	{
		return _codeBehind;
	}
	bool SetCodeBehind(
		DesignerModel::DesignCodeBehindModel codeBehind,
		std::wstring* outError = nullptr);
	const DesignerStyleSheet& GetDocumentStyleSheet() const { return _documentStyleSheet; }
	bool SetDocumentStyleSheet(DesignerStyleSheet styleSheet, std::wstring* outError = nullptr);
	void SetDesignDataContext(std::shared_ptr<IBindingSource> source);
	bool RefreshDesignBindings(
		DesignerControl& control,
		std::wstring* outError = nullptr);
	bool RefreshAllDesignBindings(std::wstring* outError = nullptr);
	const std::shared_ptr<IBindingSource>& GetDesignDataContext() const
	{
		return _designDataContext;
	}
	void SetDesignedFormEventHandler(
		const std::wstring& eventName, const std::wstring& handlerName)
	{
		if (handlerName.empty()) _designedFormEventHandlers.erase(eventName);
		else _designedFormEventHandlers[eventName] = handlerName;
	}
	SIZE GetDesignedFormSize() const { return _designedFormSize; }
	void SetDesignedFormSize(SIZE s);
	POINT GetDesignedFormLocation() const { return _designedFormLocation; }
	void SetDesignedFormLocation(POINT p) { _designedFormLocation = p; }
	bool GetDesignedFormVisibleHead() const { return _designedFormVisibleHead; }
	void SetDesignedFormVisibleHead(bool v) { _designedFormVisibleHead = v; UpdateClientSurfaceLayout(); this->InvalidateVisual(); }
	int GetDesignedFormHeadHeight() const { return _designedFormHeadHeight; }
	void SetDesignedFormHeadHeight(int h) { _designedFormHeadHeight = h; if (_designedFormHeadHeight < 0) _designedFormHeadHeight = 0; UpdateClientSurfaceLayout(); this->InvalidateVisual(); }
	bool GetDesignedFormMinBox() const { return _designedFormMinBox; }
	void SetDesignedFormMinBox(bool v) { _designedFormMinBox = v; this->InvalidateVisual(); }
	bool GetDesignedFormMaxBox() const { return _designedFormMaxBox; }
	void SetDesignedFormMaxBox(bool v) { _designedFormMaxBox = v; this->InvalidateVisual(); }
	bool GetDesignedFormCloseBox() const { return _designedFormCloseBox; }
	void SetDesignedFormCloseBox(bool v) { _designedFormCloseBox = v; this->InvalidateVisual(); }
	bool GetDesignedFormCenterTitle() const { return _designedFormCenterTitle; }
	void SetDesignedFormCenterTitle(bool v) { _designedFormCenterTitle = v; this->InvalidateVisual(); }
	bool GetDesignedFormAllowResize() const { return _designedFormAllowResize; }
	void SetDesignedFormAllowResize(bool v) { _designedFormAllowResize = v; this->InvalidateVisual(); }
	void ClampControlToDesignSurface(Control* c);
	// 设计器专用：切换 Anchor 时保持控件当前视觉矩形不变，并同步换算 Margin
	void ApplyAnchorStylesKeepingBounds(Control* c, uint8_t newAnchorStyles);

	// 设计文件（用于保存/加载设计进度）
	bool BuildDesignDocument(DesignerModel::DesignDocument& document, std::wstring* outError = nullptr) const;
	bool ApplyDesignDocument(const DesignerModel::DesignDocument& document, std::wstring* outError = nullptr);
	bool BuildEventHandlerIndex(
		DesignerModel::DesignDocumentEventIndex& index,
		std::wstring* outError = nullptr) const;
	bool RenameEventHandler(
		const std::wstring& oldName,
		const std::wstring& newName,
		size_t* outRenamedReferenceCount = nullptr,
		std::wstring* outError = nullptr);
	DesignerDocumentTransactionResult CreateNewDocument();
	DesignerDocumentTransactionResult SaveDesignFile(
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
	DesignerDocumentTransactionResult LoadDesignFile(
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
	DesignerDocumentTransactionResult RestoreRecoveredDocument(
		const DesignerModel::DesignDocument& document);
	
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	
	// 控件管理
	DesignerDocumentTransactionResult AddControlToCanvas(
		UIClass type, POINT canvasPos);
	DesignerDocumentTransactionResult AddControlToCanvas(
		const DesignerControlDescriptor& descriptor, POINT canvasPos);
	void AddControlToCanvasCore(UIClass type, POINT canvasPos);
	void AddControlToCanvasCore(
		const DesignerControlDescriptor& descriptor, POINT canvasPos);
	DesignerDocumentTransactionResult DeleteSelectedControl();
	void DeleteSelectedControlCore();
	void RemoveDesignerControlsInSubtree(Control* root);
	// 设计期 Name：用于保存/加载 parent 引用，必须非空且在当前文档内唯一
	std::wstring MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const;
	DesignerDocumentTransactionResult ExecuteCommand(
		std::unique_ptr<IDesignerCommand> command);
	/** Adds an already-applied delta command without publishing a second UI action. */
	DesignerDocumentTransactionResult CommitAlreadyAppliedCommand(
		std::unique_ptr<IDesignerCommand> command);
	DesignerDocumentTransactionResult UndoCommand();
	DesignerDocumentTransactionResult RedoCommand();
	bool IsDocumentDirty() const noexcept;
	uint64_t GetCurrentDocumentStateId() const noexcept;
	uint64_t GetSavedDocumentStateId() const noexcept;
	void SetCommandHistoryMemoryLimit(size_t byteLimit);
	size_t GetCommandHistoryMemoryLimit() const noexcept;
	size_t GetCommandHistoryMemoryUsage() const noexcept;
	size_t GetUndoCommandCount() const noexcept;
	size_t GetRedoCommandCount() const noexcept;
	DesignerDocumentTransactionResult MarkDocumentSaved();
	DesignerDocumentTransactionResult ResetDocumentHistoryAsSaved();
	DesignerDocumentTransactionResult ResetDocumentHistoryAsUnsaved();
	bool HasActiveDocumentTransaction() const noexcept;
	/** Strict, result-bearing transaction for document-level edits. */
	DesignerDocumentTransactionResult ExecuteDocumentEditTransaction(
		const std::wstring& label,
		const std::function<bool(std::wstring& error)>& applyChange);
	DesignerDocumentTransactionResult BeginDocumentEditTransaction(
		const std::wstring& label);
	DesignerDocumentTransactionResult CommitDocumentEditTransaction();
	DesignerDocumentTransactionResult RollbackDocumentEditTransaction();
	DesignerDocumentTransactionResult CancelDocumentEditTransaction();
	DesignerDocumentTransactionResult CancelActivePointerInteraction(
		const std::wstring& reason = L"画布交互已取消。");
	DesignerDocumentTransactionResult NudgeSelectionBy(int dx, int dy);
	bool HasInteractionTransactionResult() const noexcept
	{
		return _hasInteractionTransactionResult;
	}
	const std::wstring& GetLastInteractionTransaction() const noexcept
	{
		return _lastInteractionTransaction;
	}
	const DesignerDocumentTransactionResult&
		GetLastInteractionTransactionResult() const noexcept
	{
		return _lastInteractionTransactionResult;
	}
	void ClearInteractionTransactionResult();
	bool HasCommandResult() const noexcept { return _hasCommandResult; }
	const std::wstring& GetLastCommandOperation() const noexcept
	{
		return _lastCommandOperation;
	}
	const std::wstring& GetLastCommandLabel() const noexcept
	{
		return _lastCommandLabel;
	}
	const DesignerDocumentTransactionResult& GetLastCommandResult() const noexcept
	{
		return _lastCommandResult;
	}
	void ClearCommandResult();
	void RestorePrimarySelectionByName(const std::wstring& name, bool fireEvent = true);
	void RestoreSelectionByNames(const std::vector<std::wstring>& selectionNames, const std::wstring& primaryName, bool fireEvent = true);
	std::shared_ptr<DesignerControl> GetSelectedControl() { return _selectedControl; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetSelectedControls() const { return _selectedControls; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetAllControls() const { return _designerControls; }
	CodeGenInput BuildCodeGenInput() const;
	std::vector<std::shared_ptr<DesignerControl>> GetAllControlsForExport() const;
	
	// 准备添加控件（鼠标模式）
	void SetControlDescriptors(
		const std::vector<DesignerControlDescriptor>& descriptors);
	void RegisterControlDescriptor(
		const DesignerControlDescriptor& descriptor);
	void SetControlToAdd(UIClass type);
	void SetControlToAdd(const DesignerControlDescriptor& descriptor);
	
	Event<void(std::shared_ptr<DesignerControl>)> OnControlSelected;
	/** WinForms-style request raised when a control or the form is double-clicked. */
	Event<void(std::shared_ptr<DesignerControl>)> OnDefaultEventRequested;
	Event<void(const DesignerCanvasInteractionTransactionEventArgs&)>
		OnInteractionTransactionCompleted;
	Event<void(const DesignerCanvasCommandEventArgs&)> OnCommandCompleted;
	Event<void(const DesignerCanvasDocumentStateEventArgs&)>
		OnDocumentStateChanged;

private:
	void ClearCanvasCore();
	DesignerDocumentTransactionResult ReplaceDesignDocument(
		const DesignerModel::DesignDocument& document,
		const std::wstring& operation,
		bool markAsSaved = true);
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
