#pragma once

/**
 * @file DesignerCanvas.h
 * @brief DesignerCanvas：设计器画布与控件选中/拖拽交互实现。
 */
#include "../CUI/include/Panel.h"
#include "../CUI/include/ContentControl.h"
#include "../CUI/include/Window.h"
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

namespace DesignerModel { struct XamlDocumentDiagnostic; }

struct DesignerEventDescriptor;
class IDesignerCommand;
class DesignerCommandCoordinator;
class SelectionService;
class ControlPlacementCommand;
class ControlSubtreeCommand;
class EventHandlerCommand;
struct DesignerCanvasPlacementInteraction;
struct DesignerCanvasPropertyInteraction;
struct DesignerEventHandlerCodeMigration;

enum class DesignerSelectionArrangeAction : uint8_t
{
	AlignLeft,
	AlignHorizontalCenters,
	AlignRight,
	AlignTop,
	AlignVerticalCenters,
	AlignBottom,
	MakeSameWidth,
	MakeSameHeight,
	MakeSameSize,
	DistributeHorizontally,
	DistributeVertically,
	BringForward,
	SendBackward,
	BringToFront,
	SendToBack,
};

enum class DesignerHierarchyDropPosition : uint8_t
{
	Before,
	Inside,
	After
};

namespace DesignerModel
{
	struct DesignDocument;
	class DesignDocumentEventIndex;
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

struct DesignerCanvasContextMenuEventArgs
{
	POINT CanvasPosition{ 0, 0 };
	bool HasSelection = false;
};

struct DesignerCanvasViewChangedEventArgs
{
	float Zoom = 1.0f;
	D2D1_POINT_2F Offset{ 0.0f, 0.0f };
	bool FitToViewport = false;
};

struct DesignerCanvasTabOrderStateEventArgs
{
	bool Active = false;
	int NextIndex = 0;
	size_t CandidateCount = 0;
};

class DesignerCanvas : public Panel
{
friend class DesignerCommandCoordinator;
friend class ControlPlacementCommand;
friend class ControlSubtreeCommand;
friend class EventHandlerCommand;

private:
	Panel* _designSurface = nullptr;
	ContentControl* _clientSurface = nullptr;
	// The design chrome hosts exactly one authored Window.Content element.
	// New documents start with a real XAML Canvas behavior host; this is never
	// a second, designer-only root collection.
	Control* _defaultContentRoot = nullptr;
	DesignerModel::DesignNode _designedWindowNode =
		DesignerModel::DesignDocument{}.Window;
	bool _applyingDesignedWindowNode = false;
	std::wstring _designedWindowName = L"MainWindow";
	std::wstring _designedWindowTitle = L"Window";
	cui::core::Size _designedWindowSize{ 800.0f, 600.0f };
	D2D1_COLOR_F _designedWindowBackgroundColor = Colors::WhiteSmoke;
	D2D1_COLOR_F _designedWindowForegroundColor = Colors::Black;
	std::wstring _designedWindowFontFamily;
	float _designedWindowFontSize = 18.0f;
	// Designer-only chrome projection. Runtime controls inherit the Window
	// typography through the dependency-property tree instead of sharing it.
	std::unique_ptr<::Font> _designedWindowChromeFont;
	bool _designedWindowShowInTaskbar = true;
	bool _designedWindowTopmost = false;
	bool _designedWindowEnable = true;
	std::wstring _lastPastedClipboardText;
	std::vector<std::wstring> _lastPastedRootNames;
	unsigned int _clipboardPasteSequence = 0;
	DesignerDataContextSchema _dataContextSchema;
	DesignerModel::DesignCodeBehindModel _codeBehind;
	DesignerStyleSheet _documentStyleSheet;
	std::vector<DesignerModel::DesignComponentDefinition> _componentDefinitions;
	std::vector<DesignerModel::DesignControlTemplate> _controlTemplates;
	std::vector<DesignerModel::DesignDataTypeDefinition> _dataTypes;
	std::vector<DesignerModel::DesignDataTemplate> _dataTemplates;
	std::vector<DesignerModel::DesignItemsPanelTemplate> _itemsPanelTemplates;
	std::vector<DesignerModel::DesignGroupStyle> _groupStyles;
	std::vector<DesignerModel::DesignDataList> _dataLists;
	std::vector<DesignerModel::DesignCollectionViewSource> _collectionViews;
	std::wstring _documentResourceBasePath;
	std::shared_ptr<ResourceLoadContext> _documentResources;
	std::shared_ptr<ControlStyleSheet> _previewStyleSheet;
	std::shared_ptr<IBindingSource> _designDataContext;
	::WindowStyle _designedWindowStyle = ::WindowStyle::SingleBorderWindow;
	::ResizeMode _designedWindowResizeMode = ::ResizeMode::CanResize;
	POINT _designSurfaceOrigin = { 20, 20 };

	// 仅影响设计器视图，不进入 XAML/文档历史。逻辑画布坐标始终保持 1 DIP。
	float _viewZoom = 1.0f;
	D2D1_POINT_2F _viewOffset{ 0.0f, 0.0f };
	bool _fitToViewport = false;
	cui::core::Size _lastFitViewportSize{};
	bool _isPanning = false;
	bool _panStartedWithLeftButton = false;
	// View-only pointer feedback; never enters the document dependency-property state.
	CursorKind _interactionCursor = CursorKind::Arrow;
	POINT _panStartViewPoint{ 0, 0 };
	D2D1_POINT_2F _panStartViewOffset{ 0.0f, 0.0f };

	// 网格/吸附/参考线
	int _gridSize = 10;
	bool _showGrid = true;
	bool _snapToGrid = true;
	bool _snapToGuides = true;
	int _snapThreshold = 5; // 像素阈值：接近则吸附
	std::vector<int> _vGuides; // canvas 坐标
	std::vector<int> _hGuides; // canvas 坐标

	// Tab 顺序模式只影响设计视图；实际 TabIndex 修改仍走属性差量命令。
	bool _tabOrderMode = false;
	int _nextTabOrderIndex = 0;
	int _lastTabOrderStableId = 0;
	std::unique_ptr<::Font> _tabOrderBadgeFont;
	float _tabOrderBadgeFontZoom = 0.0f;
	bool _spacePanModifierDown = false;

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
		Thickness StartMargin{};
		bool UsesRelativeMargin = false;
	};
	std::vector<DragStartItem> _dragStartItems;
	
	// 调整大小状态
	bool _isResizing = false;
	DesignerControl::ResizeHandle _resizeHandle = DesignerControl::ResizeHandle::None;
	RECT _resizeStartRect = {0, 0, 0, 0};

	// 待添加的完整控件描述；自定义控件不能退化为单一 UIClass。
	std::optional<DesignerControlDescriptor> _controlToAdd;
	// Toolbox drag preview is view-only state.  The actual drop still enters
	// AdoptVisualChildToCanvas as one undoable subtree command.
	bool _controlDropPreviewVisible = false;
	RECT _controlDropPreviewRect{ 0, 0, 0, 0 };
	RECT _controlDropTargetRect{ 0, 0, 0, 0 };
	std::wstring _controlDropTargetDescription;
	std::optional<DesignerControlDescriptor> _controlDropPreviewDescriptor;
	// 进程内预览工厂不进入文档，但必须跨 Open/Undo/Redo 材质化保持。
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
	void CreateDefaultContentRoot();
	std::shared_ptr<DesignerControl> GetDocumentContentRootRecord() const;
	Control* GetDocumentContentRoot() const;
	
	void DrawSelectionHandles(std::shared_ptr<DesignerControl> dc);
	void DrawGrid();
	D2D1_MATRIX_3X2_F GetViewRenderTransform() const;
	void ClampViewOffset();
	void RecalculateFitView(bool notify);
	void NotifyViewChanged();
	void NotifyTabOrderStateChanged();
	void BeginViewPan(POINT viewPoint, bool leftButton);
	void EndViewPan();
	int GetSelectionHandleSizeInCanvas() const;
	RECT GetDesignSurfaceRectInCanvas() const;
	RECT GetClientSurfaceRectInCanvas() const;
	RECT GetViewportRectInCanvas() const;
	bool IsPointInDesignedWindow(POINT ptCanvas) const;
	bool IsPointInDesignSurface(POINT ptCanvas) const;
	RECT ClampRectToBounds(RECT r, const RECT& bounds, bool keepSize) const;
	bool TryHandleTabHeaderClick(POINT ptCanvas);
	bool DesignedWindowHasChrome() const noexcept
	{
		return _designedWindowStyle != ::WindowStyle::None;
	}
	bool DesignedWindowHasMinimizeBox() const noexcept
	{
		return DesignedWindowHasChrome()
			&& _designedWindowStyle != ::WindowStyle::ToolWindow
			&& _designedWindowResizeMode != ::ResizeMode::NoResize;
	}
	bool DesignedWindowHasMaximizeBox() const noexcept
	{
		return DesignedWindowHasChrome()
			&& _designedWindowStyle != ::WindowStyle::ToolWindow
			&& (_designedWindowResizeMode == ::ResizeMode::CanResize
				|| _designedWindowResizeMode == ::ResizeMode::CanResizeWithGrip);
	}
	int DesignedClientTop() const
	{
		return DesignedWindowHasChrome() ? 24 : 0;
	}
	void UpdateClientSurfaceLayout();
	void UpdateContentPreviewLayout();
	
	std::shared_ptr<DesignerControl> HitTestControl(
		POINT pt, bool preferParentContainer = false);
	CursorKind GetResizeCursor(DesignerControl::ResizeHandle handle);
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
	void ApplyRectToControl(
		Control* c, const RECT& rectInCanvas, bool preserveSize = false);
	bool IsTabOrderCandidate(
		const std::shared_ptr<DesignerControl>& control) const;
	std::vector<std::shared_ptr<DesignerControl>>
		CollectTabOrderCandidates() const;
	DesignerDocumentTransactionResult ApplyTabOrderAssignments(
		const std::vector<std::pair<std::shared_ptr<DesignerControl>, int>>&
			assignments,
		const std::wstring& operation,
		std::wstring successMessage);
	void RefreshDesignedWindowTypography();
	void DetachDesignBindingPreview(DesignerControl& control);
	
public:
	DesignerCanvas(int x, int y, int width, int height);
	virtual ~DesignerCanvas();
	CursorKind QueryCursor(int localX, int localY) override
	{
		(void)localX;
		(void)localY;
		return _interactionCursor;
	}
	bool HitTestChildren() const override { return false; }
	bool TryGetDescendantRenderTransform(
		D2D1_MATRIX_3X2_F& transform) const override;
	DesignerModel::DesignNode CaptureDesignedWindowNode() const;
	/** Atomically rewrites every document-namescope object reference on rename. */
	void RewriteInputBindingCommandTargetReferences(
		const std::wstring& previousName,
		const std::wstring& nextName);
	bool ApplyDesignedWindowNode(
		const DesignerModel::DesignNode& window,
		std::wstring* outError = nullptr);
	bool ApplyDesignedWindowProperty(
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);
	bool ResetDesignedWindowProperty(
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	// 当外部（属性面板）修改 Name 后，同步默认命名计数器（按类型）。
	void SyncDefaultNameCounter(UIClass type, const std::wstring& name) { UpdateDefaultNameCounterFromName(type, name); }

	std::wstring GetDesignedWindowName() const { return _designedWindowNode.Name; }
	std::wstring GetDesignedWindowTitle() const { return _designedWindowTitle; }
	D2D1_COLOR_F GetDesignedWindowBackgroundColor() const
	{
		return _designedWindowBackgroundColor;
	}
	D2D1_COLOR_F GetDesignedWindowForegroundColor() const
	{
		return _designedWindowForegroundColor;
	}
	std::wstring GetDesignedWindowFontFamily() const { return _designedWindowFontFamily; }
	float GetDesignedWindowFontSize() const { return _designedWindowFontSize; }
	bool GetDesignedWindowShowInTaskbar() const { return _designedWindowShowInTaskbar; }
	bool GetDesignedWindowTopmost() const { return _designedWindowTopmost; }
	bool GetDesignedWindowEnable() const { return _designedWindowEnable; }
	const DesignerModel::DesignEventHandlerMap& GetDesignedWindowEventHandlers() const
	{
		return _designedWindowNode.Events;
	}
	const DesignerDataContextSchema& GetDataContextSchema() const { return _dataContextSchema; }
	DesignerDataContextSchema GetEffectiveDataContextSchema(
		const DesignerControl& control) const;
	IBindingSource* GetEffectiveDesignDataContextSource(
		DesignerControl& control) const;
	bool SetDataContextSchema(DesignerDataContextSchema schema, std::wstring* outError = nullptr);
	const DesignerModel::DesignCodeBehindModel& GetCodeBehind() const noexcept
	{
		return _codeBehind;
	}
	bool SetCodeBehind(
		DesignerModel::DesignCodeBehindModel codeBehind,
		std::wstring* outError = nullptr);
	const DesignerStyleSheet& GetDocumentStyleSheet() const { return _documentStyleSheet; }
	const std::vector<DesignerModel::DesignComponentDefinition>&
		GetComponentDefinitions() const noexcept { return _componentDefinitions; }
	const std::vector<DesignerModel::DesignControlTemplate>&
		GetControlTemplates() const noexcept { return _controlTemplates; }
	const std::vector<DesignerModel::DesignDataTypeDefinition>&
		GetDataTypes() const noexcept { return _dataTypes; }
	const std::vector<DesignerModel::DesignDataTemplate>&
		GetDataTemplates() const noexcept { return _dataTemplates; }
	const std::vector<DesignerModel::DesignItemsPanelTemplate>&
		GetItemsPanelTemplates() const noexcept { return _itemsPanelTemplates; }
	const std::vector<DesignerModel::DesignGroupStyle>&
		GetGroupStyles() const noexcept { return _groupStyles; }
	const std::vector<DesignerModel::DesignDataList>&
		GetDataLists() const noexcept { return _dataLists; }
	const std::vector<DesignerModel::DesignCollectionViewSource>&
		GetCollectionViews() const noexcept { return _collectionViews; }
	const std::wstring& GetDocumentResourceBasePath() const noexcept
	{
		return _documentResourceBasePath;
	}
	bool SetDocumentStyleSheet(
		DesignerStyleSheet styleSheet,
		std::wstring* outError = nullptr,
		const std::vector<std::pair<std::wstring, std::wstring>>& resourceRenames = {},
		bool allowVisualStateRebuild = true);
	void SetDesignDataContext(std::shared_ptr<IBindingSource> source);
	bool RefreshDesignBindings(
		DesignerControl& control,
		std::wstring* outError = nullptr);
	bool RefreshAllDesignBindings(std::wstring* outError = nullptr);
	const std::shared_ptr<IBindingSource>& GetDesignDataContext() const
	{
		return _designDataContext;
	}
	void SetDesignedWindowEventHandler(
		const std::wstring& eventName, const std::wstring& handlerName)
	{
		if (handlerName.empty()) _designedWindowNode.Events.erase(eventName);
		else _designedWindowNode.Events[eventName] = handlerName;
	}
	cui::core::Size GetDesignedWindowSize() const { return _designedWindowSize; }
	void SetDesignedWindowSize(cui::core::Size value);
	::WindowStyle GetDesignedWindowStyle() const
	{
		return _designedWindowStyle;
	}
	::ResizeMode GetDesignedWindowResizeMode() const
	{
		return _designedWindowResizeMode;
	}

	// 视图导航（非文档状态）：Ctrl+滚轮/加减号缩放，Ctrl+0 适配，Ctrl+1 还原。
	float GetViewZoom() const noexcept { return _viewZoom; }
	D2D1_POINT_2F GetViewOffset() const noexcept { return _viewOffset; }
	bool IsFitToViewport() const noexcept { return _fitToViewport; }
	POINT ViewToCanvasPoint(POINT point) const;
	POINT CanvasToViewPoint(POINT point) const;
	void SetViewZoom(float zoom);
	void SetViewZoom(float zoom, POINT focalPointInView);
	void ZoomIn();
	void ZoomOut();
	void ResetView();
	void FitDesignSurfaceToViewport();
	bool IsGridVisible() const noexcept { return _showGrid; }
	bool IsSnapToGridEnabled() const noexcept { return _snapToGrid; }
	bool IsSnapToGuidesEnabled() const noexcept { return _snapToGuides; }
	int GetGridSize() const noexcept { return _gridSize; }
	void SetGridVisible(bool visible);
	void SetSnapToGridEnabled(bool enabled);
	void SetSnapToGuidesEnabled(bool enabled);
	void SetGridSize(int gridSize);
	bool SetTabOrderMode(bool active, int nextIndex = 0);
	bool IsTabOrderMode() const noexcept { return _tabOrderMode; }
	int GetNextTabOrderIndex() const noexcept { return _nextTabOrderIndex; }
	size_t GetTabOrderCandidateCount() const
	{
		return CollectTabOrderCandidates().size();
	}
	/** Assigns one explicit TabIndex as a reversible metadata property delta. */
	DesignerDocumentTransactionResult AssignTabOrderIndex(
		const std::shared_ptr<DesignerControl>& control,
		int tabIndex);
	/** Assigns every focusable design control in visual reading order as one Undo. */
	DesignerDocumentTransactionResult AutoArrangeTabOrder();
	/** Activates any owning TabItem chain so an outline-selected control is visible. */
	bool RevealControlInDesigner(Control* control);
	void ClampControlToDesignSurface(Control* c);
	// 设计文件（用于保存/加载设计进度）
	bool BuildDesignDocument(DesignerModel::DesignDocument& document, std::wstring* outError = nullptr) const;
	bool ApplyDesignDocument(
		const DesignerModel::DesignDocument& document,
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);
	bool BuildXamlDocumentText(
		std::wstring& xamlText,
		std::wstring* outError = nullptr) const;
	/** Shared same-signature handler candidates for the event property grid. */
	std::vector<std::wstring> GetCompatibleEventHandlerNames(
		const DesignerEventDescriptor& requested,
		const std::wstring& defaultName,
		const std::wstring& currentName,
		const std::map<std::string, std::vector<std::wstring>>&
			compatibleUserHandlers) const;
	/** Applies one live XAML preview inside an already active document transaction. */
	bool PreviewXamlDocumentText(
		const std::wstring& xamlText,
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);
	bool BuildEventHandlerIndex(
		DesignerModel::DesignDocumentEventIndex& index,
		std::wstring* outError = nullptr) const;
	/** Applies one Window/control event mapping through a stable-ID delta command. */
	DesignerDocumentTransactionResult UpdateEventHandler(
		const std::shared_ptr<DesignerControl>& control,
		const std::wstring& eventName,
		const std::wstring& handlerName,
		std::wstring* outError = nullptr);
	/** Applies one common event mapping to multiple controls atomically. */
	DesignerDocumentTransactionResult UpdateEventHandlers(
		const std::vector<std::shared_ptr<DesignerControl>>& controls,
		const std::wstring& eventName,
		const std::wstring& handlerName,
		std::wstring* outError = nullptr);
	/** Renames every compatible reference as one batch event delta command. */
	DesignerDocumentTransactionResult RenameEventHandler(
		const std::wstring& oldName,
		const std::wstring& newName,
		size_t* outRenamedReferenceCount = nullptr,
		std::wstring* outError = nullptr,
		const DesignerEventHandlerCodeMigration* codeMigration = nullptr);
	DesignerDocumentTransactionResult CreateNewDocument();
	DesignerDocumentTransactionResult SaveDesignFile(
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
	DesignerDocumentTransactionResult LoadDesignFile(
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
	DesignerDocumentTransactionResult RestoreRecoveredDocument(
		const DesignerModel::DesignDocument& document);
	
protected:
	void PreparePresentation() override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
public:
	// 控件管理
	DesignerDocumentTransactionResult AdoptVisualChildToCanvas(
		UIClass type, POINT canvasPos);
	DesignerDocumentTransactionResult AdoptVisualChildToCanvas(
		const DesignerControlDescriptor& descriptor, POINT canvasPos);
	/** Updates the view-only toolbox ghost and resolves the actual drop host. */
	bool UpdateControlDropPreview(
		const DesignerControlDescriptor& descriptor,
		POINT canvasPos,
		std::wstring* outTargetDescription = nullptr);
	void ClearControlDropPreview();
	bool HasControlDropPreview() const noexcept
	{
		return _controlDropPreviewVisible;
	}
	RECT GetControlDropPreviewRect() const noexcept
	{
		return _controlDropPreviewRect;
	}
	RECT GetControlDropTargetRect() const noexcept
	{
		return _controlDropTargetRect;
	}
	const std::wstring& GetControlDropTargetDescription() const noexcept
	{
		return _controlDropTargetDescription;
	}
	DesignerDocumentTransactionResult CopySelectedControls();
	DesignerDocumentTransactionResult CutSelectedControls();
	/**
	 * Reports whether the current clipboard source contains text that can be
	 * offered to the CUI XAML paste pipeline. Parsing still happens when paste
	 * runs so externally copied malformed text receives a diagnostic.
	 */
	bool CanPasteControlsFromClipboard() const noexcept;
	/** Pastes with the normal 12-DIP cascading offset. */
	DesignerDocumentTransactionResult PasteControlsFromClipboard();
	/** Pastes at the fragment's original local coordinates. */
	DesignerDocumentTransactionResult PasteControlsFromClipboardInPlace();
	/** Pastes with the fragment root bounds starting at a canvas point. */
	DesignerDocumentTransactionResult PasteControlsFromClipboardAt(
		POINT canvasPosition);
	/** Creates an offset copy without replacing the system clipboard. */
	DesignerDocumentTransactionResult DuplicateSelectedControls();
	/** Transactionally inserts a complete CUI XAML document or control fragment. */
	DesignerDocumentTransactionResult PasteControlsFromXamlText(
		const std::wstring& xamlText);
	DesignerDocumentTransactionResult PasteControlsFromXamlTextInPlace(
		const std::wstring& xamlText);
	DesignerDocumentTransactionResult PasteControlsFromXamlTextAt(
		const std::wstring& xamlText,
		POINT canvasPosition);
	/** Applies one designer multi-selection arrangement command. */
	DesignerDocumentTransactionResult ArrangeSelection(
		DesignerSelectionArrangeAction action);
	bool HasLockedSelectedControls() const noexcept;
	bool AreAllSelectedControlsLocked() const noexcept;
	/** Batch-locks the current selection as one undoable design-only edit. */
	DesignerDocumentTransactionResult SetSelectedControlsLocked(bool locked);
	/** Moves one control from the document outline as one undoable tree delta. */
	DesignerDocumentTransactionResult MoveControlInHierarchy(
		int sourceStableId,
		std::optional<int> targetStableId,
		DesignerHierarchyDropPosition position);
	void AdoptVisualChildToCanvasCore(UIClass type, POINT canvasPos);
	void AdoptVisualChildToCanvasCore(
		const DesignerControlDescriptor& descriptor, POINT canvasPos);
	DesignerDocumentTransactionResult DeleteSelectedControl(
		bool publishResult = true);
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
	std::wstring GetUndoCommandLabel() const;
	std::wstring GetRedoCommandLabel() const;
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
	/** Selects every design control in the primary selection's current parent. */
	bool SelectAllInCurrentContainer(bool fireEvent = true);
	std::shared_ptr<DesignerControl> GetSelectedControl() { return _selectedControl; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetSelectedControls() const { return _selectedControls; }
	const std::vector<std::shared_ptr<DesignerControl>>& GetAllControls() const { return _designerControls; }
	
	// 准备添加控件（鼠标模式）
	void SetControlToAdd(UIClass type);
	void SetControlToAdd(const DesignerControlDescriptor& descriptor);
	
	Event<void(std::shared_ptr<DesignerControl>)> OnControlSelected;
	/** Designer request raised when an element or the Window is double-clicked. */
	Event<void(std::shared_ptr<DesignerControl>)> OnDefaultEventRequested;
	Event<void(const DesignerCanvasInteractionTransactionEventArgs&)>
		OnInteractionTransactionCompleted;
	Event<void(const DesignerCanvasCommandEventArgs&)> OnCommandCompleted;
	Event<void(const DesignerCanvasDocumentStateEventArgs&)>
		OnDocumentStateChanged;
	Event<void(const DesignerCanvasViewChangedEventArgs&)> OnViewChanged;
	Event<void(const DesignerCanvasTabOrderStateEventArgs&)>
		OnTabOrderStateChanged;
	Event<void(const DesignerCanvasContextMenuEventArgs&)>
		OnContextMenuRequested;

private:
	enum class ClipboardPastePlacement
	{
		Cascade,
		InPlace,
		AtCanvasPoint
	};

	DesignerDocumentTransactionResult CopySelectedControlsCore(
		bool publishResult);
	DesignerDocumentTransactionResult PasteControlsFromClipboardCore(
		ClipboardPastePlacement placement,
		std::optional<POINT> canvasPosition);
	DesignerDocumentTransactionResult PasteControlsFromXamlTextCore(
		const std::wstring& xamlText,
		ClipboardPastePlacement placement,
		std::optional<POINT> canvasPosition);
	void ClearCanvasCore();
	Control* FindControlInstanceByName(const std::wstring& name) const noexcept;
	std::optional<DesignerDataContextSchema> ResolveBindingSourceSchema(
		const DesignerControl& control,
		bool inherited,
		const DesignerDataContextSchema& rootSchema) const;
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
	Control* NormalizeContainerForDrop(
		Control* container, UIClass childType);
	POINT CanvasToContainerPoint(POINT ptCanvas, Control* container);
	POINT CanvasToChildLayoutPoint(POINT ptCanvas, Control* container);
	void TryReparentSelectedAfterDrag();
	void DeleteVisualChildRecursive(Control* c);
};
