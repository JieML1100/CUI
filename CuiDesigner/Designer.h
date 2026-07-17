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
#include "../CUI/include/Label.h"
#include <map>
#include <set>
#include <vector>
class Designer : public Form
{
friend bool RunDesignerSelfTest(std::wstring& report);

private:
	ToolBox* _toolBox = nullptr;
	DesignerCanvas* _canvas = nullptr;
	PropertyGrid* _propertyGrid = nullptr;
	
	// 顶部工具栏
	Button* _btnNew = nullptr;
	Button* _btnOpen = nullptr;
	Button* _btnSave = nullptr;
	Button* _btnReload = nullptr;
	Button* _btnExport = nullptr;
	Button* _btnRegenerate = nullptr;
	Button* _btnDelete = nullptr;
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
	static constexpr UINT RecoveryDelayMilliseconds = 750;
	static constexpr UINT RecoveryRetryMilliseconds = 5000;
	static constexpr UINT CodeFreshnessDelayMilliseconds = 250;
	
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
