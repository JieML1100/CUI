#pragma once

/**
 * @file Designer.h
 * @brief Designer：CUI 可视化设计器主窗口。
 */
#include "../CUI/include/Form.h"
#include "DesignerCanvas.h"
#include "ToolBox.h"
#include "PropertyGrid.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/Label.h"
#include <map>
#include <vector>
class Designer : public Form
{
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
	void OnEventHandlerActivated(const std::wstring& handlerName);
	void OnDeleteClick();
	bool GenerateCodeFiles(
		const std::wstring& basePath,
		std::wstring* outError = nullptr,
		const std::wstring& className = {});
	bool AssociateCodeBehind(
		const std::wstring& className,
		const std::wstring& basePath,
		const std::wstring& designFilePath,
		std::wstring* outError = nullptr);
	void RestoreCodeBehindAssociation();
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
	std::wstring _recoveryDirectory;
	std::wstring _sessionRecoveryPath;
	uint64_t _recoveryProcessStartTime = 0;
	bool _recoverySnapshotPending = false;
	bool _closeApproved = false;
	static constexpr UINT_PTR RecoveryTimerId = 0xC0D2;
	static constexpr UINT RecoveryDelayMilliseconds = 750;
	static constexpr UINT RecoveryRetryMilliseconds = 5000;
	
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
