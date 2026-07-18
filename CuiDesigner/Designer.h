#pragma once

/**
 * @file Designer.h
 * @brief Designer：CUI 可视化设计器主窗口。
 */
#include "../CUI/include/Form.h"
#include "DesignerCanvas.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "ToolBox.h"
#include "PropertyGrid.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/ContextMenu.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TreeView.h"
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
class Designer : public Form
{
friend bool RunDesignerSelfTest(std::wstring& report);

private:
	ToolBox* _toolBox = nullptr;
	Button* _btnToolboxView = nullptr;
	Button* _btnOutlineView = nullptr;
	TreeView* _outlineTree = nullptr;
	std::unordered_map<int, TreeNode*> _outlineNodesByStableId;
	TreeNode* _outlineFormNode = nullptr;
	bool _showDocumentOutline = false;
	bool _syncingDocumentOutline = false;
	bool _documentOutlineRebuildPending = false;
	wchar_t _suppressedOutlineShortcutCharacter = L'\0';
	bool _designerControlKeyDown = false;
	bool _designerShiftKeyDown = false;
	bool _designerAltKeyDown = false;
	HWND _clipboardListenerWindow = nullptr;
	unsigned int _clipboardRefreshRetriesRemaining = 0;
	bool _outlinePointerDown = false;
	bool _outlineDragging = false;
	POINT _outlineDragStart{ 0, 0 };
	int _outlineDragSourceStableId = 0;
	std::optional<int> _outlineDropTargetStableId;
	TreeViewDropPosition _outlineDropPosition = TreeViewDropPosition::None;
	bool _toolBoxPointerDown = false;
	bool _toolBoxDragging = false;
	POINT _toolBoxDragStart{ 0, 0 };
	POINT _toolBoxDropCanvasPoint{ 0, 0 };
	bool _toolBoxDropAccepted = false;
	std::optional<DesignerControlDescriptor> _toolBoxDragDescriptor;
	POINT _canvasContextPastePoint{ 0, 0 };
	bool _hasCanvasContextPastePoint = false;
	DesignerCanvas* _canvas = nullptr;
	PropertyGrid* _propertyGrid = nullptr;
	
	// 顶部工具栏
	Button* _btnNew = nullptr;
	Button* _btnOpen = nullptr;
	Button* _btnSave = nullptr;
	Button* _btnReload = nullptr;
	Button* _btnExport = nullptr;
	Button* _btnRegenerate = nullptr;
	Button* _btnUndo = nullptr;
	Button* _btnRedo = nullptr;
	Button* _btnCopy = nullptr;
	Button* _btnCut = nullptr;
	Button* _btnPaste = nullptr;
	Button* _btnXaml = nullptr;
	Button* _btnArrange = nullptr;
	Button* _btnDelete = nullptr;
	Button* _btnZoomOut = nullptr;
	Button* _btnZoomIn = nullptr;
	Button* _btnFitView = nullptr;
	Button* _btnGridSettings = nullptr;
	Button* _btnTabOrder = nullptr;
	Label* _lblZoom = nullptr;
	ContextMenu* _arrangeMenu = nullptr;
	ContextMenu* _canvasMenu = nullptr;
	ContextMenu* _gridMenu = nullptr;
	Label* _lblInfo = nullptr;
	std::shared_ptr<IBindingSource> _designDataContext;
	std::vector<DesignerControlDescriptor> _controlDescriptors;
	
	void OnToolBoxControlSelected(const DesignerControlDescriptor& descriptor);
	void OnCanvasControlSelected(std::shared_ptr<DesignerControl> control);
	void OnCanvasInteractionTransactionCompleted(
		const DesignerCanvasInteractionTransactionEventArgs& args);
	void OnCanvasCommandCompleted(
		const DesignerCanvasCommandEventArgs& args);
	void OnCanvasDocumentStateChanged(
		const DesignerCanvasDocumentStateEventArgs& args);
	void OnCanvasContextMenuRequested(
		const DesignerCanvasContextMenuEventArgs& args);
	void OnCanvasViewChanged(
		const DesignerCanvasViewChangedEventArgs& args);
	void OnCanvasTabOrderStateChanged(
		const DesignerCanvasTabOrderStateEventArgs& args);
	void SetSidebarView(bool showDocumentOutline);
	void RebuildDocumentOutline();
	void ScheduleDocumentOutlineRebuild();
	void SyncDocumentOutlineSelection();
	void OnDocumentOutlineSelectionChanged();
	void BeginDocumentOutlineDrag(const MouseEventArgs& args);
	void UpdateDocumentOutlineDrag(int localX, int localY);
	void EndDocumentOutlineDrag();
	void CancelDocumentOutlineDrag(bool releaseCapture = true);
	void BeginToolBoxDrag(
		const DesignerControlDescriptor& descriptor,
		POINT formPoint);
	void UpdateToolBoxDrag(int localX, int localY);
	void EndToolBoxDrag(int localX, int localY);
	void CancelToolBoxDrag(bool releaseCapture = true);
	void UpdateCanvasOperationStatus(
		const std::wstring& operation,
		const std::wstring& label,
		const std::wstring& message,
		const DesignerDocumentTransactionResult& result);
	void OnNewClick();
	void OnOpenClick();
	void OnSaveClick();
	void OnReloadClick();
	void OnExportClick();
	void OnRegenerateCodeClick();
	void OnUndoClick();
	void OnRedoClick();
	void OnCopyClick();
	void OnCutClick();
	void OnPasteClick();
	void OnXamlClick();
	void OnArrangeClick();
	void OnArrangeCommand(int commandId);
	void OnCanvasMenuCommand(int commandId);
	bool QueueOutlineShortcut(WPARAM key, bool controlDown, bool shiftDown);
	bool ExecuteOutlineShortcut(WPARAM key, bool controlDown, bool shiftDown);
	void RefreshGridSettingsPresentation();
	void RefreshTabOrderPresentation();
	void RefreshLockPresentation();
	void ToggleTabOrderMode();
	void ToggleSelectedControlsLocked();
	void RefreshCommandAvailability();
	void StartClipboardMonitoring();
	void StopClipboardMonitoring();
	void OnEventHandlerActivated(const std::wstring& handlerName);
	void OnDeleteClick();
	bool GenerateCodeFiles(
		const std::wstring& basePath,
		std::wstring* outError = nullptr,
		const std::wstring& className = {});
	bool GenerateAndAssociateCodeFiles(
		const std::wstring& basePath,
		const std::wstring& className,
		std::wstring* outError = nullptr);
	bool AssociateCodeBehind(
		const std::wstring& className,
		const std::wstring& basePath,
		const std::wstring& designFilePath,
		std::wstring* outError = nullptr);
	void RestoreCodeBehindAssociation();
	void PublishEventHandlerCodeInspection(
		DesignerModel::DesignEventHandlerCodeInspection inspection);
	void RefreshEventHandlerCodeInspection(
		const DesignerModel::DesignDocument& document,
		const DesignerModel::DesignCodeGenerationOptions& options);
	std::wstring CurrentCodeFreshnessTargetKey() const;
	void ResetCodeFreshnessTracking();
	void ScheduleCodeFreshnessInspection();
	void RefreshCodeFreshnessFromFiles();
	void UpdateCodeFreshnessForDocumentState();
	void RecordGeneratedCodeState(
		const DesignerModel::DesignCodeGenerationResult& result);
	bool SaveDocumentInteractive();
	bool ConfirmCanReplaceOrCloseDocument();
	void PrepareDocumentLifecycle();
	void UpdateDocumentPresentation();
	void InitializeRecoverySession();
	void TryRestoreRecoveryOnStartup();
	void ScheduleRecoverySnapshot();
	bool FlushRecoverySnapshot(std::wstring* outError = nullptr);
	void DiscardSessionRecoverySnapshot();
	
	std::wstring _currentFileName;
	/** Last explicit code-export target for event double-click regeneration. */
	std::wstring _lastExportBasePath;
	/** Session-only absolute targets let unsaved code-behind undo/redo follow class identity. */
	std::map<std::wstring, std::wstring> _sessionExportBasePaths;
	DesignerModel::DesignCodeFreshnessResult _codeFreshness;
	DesignerModel::DesignEventHandlerCodeInspection _eventCodeInspection;
	std::wstring _codeFreshnessTargetKey;
	std::map<std::wstring, std::set<uint64_t>> _currentCodeStateIds;
	bool _codeFreshnessInspectionPending = false;
	std::wstring _recoveryDirectory;
	std::wstring _sessionRecoveryPath;
	uint64_t _recoveryProcessStartTime = 0;
	bool _recoverySnapshotPending = false;
	bool _closeApproved = false;
	static constexpr UINT_PTR RecoveryTimerId = 0xC0D2;
	static constexpr UINT_PTR CodeFreshnessTimerId = 0xC0D3;
	static constexpr UINT_PTR ClipboardRefreshTimerId = 0xC0D4;
	static constexpr UINT RecoveryDelayMilliseconds = 750;
	static constexpr UINT RecoveryRetryMilliseconds = 5000;
	static constexpr UINT CodeFreshnessDelayMilliseconds = 250;
	static constexpr UINT ClipboardRefreshDelayMilliseconds = 60;
	static constexpr unsigned int ClipboardRefreshRetryCount = 4;
	
public:
	explicit Designer(
		std::vector<DesignerControlDescriptor> controlDescriptors = {});
	virtual ~Designer();
	
	void InitializeComponents();
	void InitAndShow(); // 初始化并显示窗口
	void SetDesignDataContext(std::shared_ptr<IBindingSource> source);
	bool ProcessMessage(
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		int localX,
		int localY) override;
};
