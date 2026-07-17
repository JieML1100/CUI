#include "Designer.h"
#include "CodeBehindExportDialog.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "DesignerModel/DesignDocumentFileFormat.h"
#include "DesignerModel/DesignRecoveryStore.h"
#include "SourceCodeNavigator.h"
#include <Utils.h>
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>

namespace
{
	static std::string MakeDesignFilter()
	{
		std::string s;
		s.append("CUI Designer Files (*.cui.xml;*.cui.xaml)");
		s.push_back('\0');
		s.append("*.cui.xml;*.cui.xaml");
		s.push_back('\0');
		s.append("CUI XAML Files (*.cui.xaml;*.xaml)");
		s.push_back('\0');
		s.append("*.cui.xaml;*.xaml");
		s.push_back('\0');
		s.append("XML Files (*.cui.xml;*.xml)");
		s.push_back('\0');
		s.append("*.cui.xml;*.xml");
		s.push_back('\0');
		s.push_back('\0');
		return s;
	}


	static void ShowModalMessage(Form* ownerForm, const std::wstring& caption, const std::wstring& text)
	{
		::MessageBoxW(ownerForm->Handle, text.c_str(), caption.c_str(), MB_OK | MB_SETFOREGROUND);
	}

	static SIZE GetLogicalDesignerContentSize(Form* form)
	{
		if (!form)
			return SIZE{ 0, 0 };

		float dpiScale = form->GetDpiScale();
		if (dpiScale <= 0.0f)
			dpiScale = 1.0f;

		if (form->Handle && ::IsWindow(form->Handle))
		{
			RECT rc{};
			::GetClientRect(form->Handle, &rc);
			int contentWidth = (int)((float)(rc.right - rc.left) / dpiScale);
			int topPhysical = (form->VisibleHead ? form->HeadHeight : 0);
			int contentHeight = (int)((float)((rc.bottom - rc.top) - topPhysical) / dpiScale);
			if (contentWidth < 0) contentWidth = 0;
			if (contentHeight < 0) contentHeight = 0;
			return SIZE{ contentWidth, contentHeight };
		}

		SIZE size = form->Size;
		int topLogical = (int)((float)(form->VisibleHead ? form->HeadHeight : 0) / dpiScale);
		size.cx = (int)((float)size.cx / dpiScale);
		size.cy = (int)((float)size.cy / dpiScale) - topLogical;
		if (size.cx < 0) size.cx = 0;
		if (size.cy < 0) size.cy = 0;
		return size;
	}

	static std::wstring DescribeCanvasOperation(
		const std::wstring& operation)
	{
		const std::wstring updatePropertyPrefix = L"UpdateProperty:";
		if (operation.rfind(updatePropertyPrefix, 0) == 0)
			return L"修改属性 " + operation.substr(
				updatePropertyPrefix.size());
		const std::wstring editStructurePrefix = L"EditStructure:";
		if (operation.rfind(editStructurePrefix, 0) == 0)
			return L"编辑结构 " + operation.substr(
				editStructurePrefix.size());
		if (operation == L"MoveSelection") return L"移动控件";
		if (operation == L"NudgeSelection") return L"微调控件";
		if (operation == L"ResizeSelection") return L"调整控件大小";
		if (operation == L"UpdateProperty:SplitterDistance")
			return L"调整分隔条";
		if (operation == L"BoxSelection") return L"框选控件";
		if (operation == L"AddControl") return L"添加控件";
		if (operation == L"DeleteSelection") return L"删除控件";
		if (operation == L"Undo") return L"撤销";
		if (operation == L"Redo") return L"重做";
		if (operation == L"ExecuteCommand") return L"执行命令";
		if (operation == L"NewDocument") return L"新建文档";
		if (operation == L"OpenDocument") return L"打开文档";
		if (operation == L"SaveDocument") return L"保存文档";
		if (operation == L"RestoreRecovery") return L"恢复自动保存文档";
		return operation.empty() ? L"画布操作" : operation;
	}

	static std::wstring DisplayDocumentName(
		const std::wstring& path)
	{
		if (path.empty()) return L"未命名";
		const auto slash = path.find_last_of(L"\\/");
		return slash == std::wstring::npos
			? path : path.substr(slash + 1);
	}

}

Designer::Designer(std::vector<DesignerControlDescriptor> controlDescriptors)
	: Form(L"CUI 窗口设计器", { 0,0 }, { 1400, 840 }),
	_controlDescriptors(std::move(controlDescriptors))
{
	this->BackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
}

Designer::~Designer()
{
	ResetCodeFreshnessTracking();
	DiscardSessionRecoverySnapshot();
}

void Designer::InitAndShow()
{
	InitializeComponents();
	InitializeRecoverySession();
	this->Show();
	TryRestoreRecoveryOnStartup();
}

void Designer::InitializeComponents()
{
	// 顶部工具栏
	int toolbarHeight = 50;
	int btnWidth = 80;
	int btnHeight = 30;
	int btnY = 10;
	int btnX = 10;
	
	_btnNew = new Button(L"新建", btnX, btnY, btnWidth, btnHeight);
	_btnNew->Round = 0.5f;
	_btnNew->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnNewClick();
	};
	this->AddControl(_btnNew);
	btnX += btnWidth + 10;

	_btnOpen = new Button(L"打开", btnX, btnY, btnWidth, btnHeight);
	_btnOpen->Round = 0.5f;
	_btnOpen->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnOpenClick();
	};
	this->AddControl(_btnOpen);
	btnX += btnWidth + 10;

	_btnSave = new Button(L"保存", btnX, btnY, btnWidth, btnHeight);
	_btnSave->Round = 0.5f;
	_btnSave->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnSaveClick();
	};
	this->AddControl(_btnSave);
	btnX += btnWidth + 10;

	_btnReload = new Button(L"重新加载", btnX, btnY, btnWidth + 10, btnHeight);
	_btnReload->Round = 0.5f;
	_btnReload->Enable = false;
	_btnReload->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnReloadClick();
	};
	this->AddControl(_btnReload);
	btnX += btnWidth + 20;
	
	_btnExport = new Button(L"导出代码", btnX, btnY, btnWidth + 20, btnHeight);
	_btnExport->Round = 0.5f;
	_btnExport->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnExportClick();
	};
	this->AddControl(_btnExport);
	btnX += btnWidth + 30;

	_btnRegenerate = new Button(L"重新生成", btnX, btnY, btnWidth + 20, btnHeight);
	_btnRegenerate->Round = 0.5f;
	_btnRegenerate->Enable = false;
	_btnRegenerate->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnRegenerateCodeClick();
	};
	this->AddControl(_btnRegenerate);
	btnX += btnWidth + 30;
	
	_btnDelete = new Button(L"删除", btnX, btnY, btnWidth, btnHeight);
	_btnDelete->Round = 0.5f;
	_btnDelete->BackColor = Colors::IndianRed;
	_btnDelete->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnDeleteClick();
	};
	this->AddControl(_btnDelete);
	btnX += btnWidth + 30;
	
	_lblInfo = new Label(L"就绪", btnX, btnY + 5);
	_lblInfo->Size = {400, 25};
	_lblInfo->Font = new ::Font(L"Microsoft YaHei", 14.0f);
	this->AddControl(_lblInfo);
	
	// 工具箱（左侧）
	int toolBoxWidth = 150;
	SIZE contentSize = GetLogicalDesignerContentSize(this);
	int formHeight = contentSize.cy;
	_toolBox = new ToolBox(
		10, toolbarHeight + 10, toolBoxWidth,
		formHeight - toolbarHeight - 40, _controlDescriptors);
	_toolBox->OnControlSelected += [this](const DesignerControlDescriptor& descriptor) {
		OnToolBoxControlSelected(descriptor);
	};
	this->AddControl(_toolBox);
	
	// 属性面板（右侧）
	int propertyGridWidth = 250;
	int formWidth = contentSize.cx;
	_propertyGrid = new PropertyGrid(formWidth - propertyGridWidth - 15, toolbarHeight + 10, 
		propertyGridWidth, formHeight - toolbarHeight - 40);
	_propertyGrid->OnEventHandlerActivated +=
		[this](PropertyGrid*, const std::wstring& handlerName)
		{
			OnEventHandlerActivated(handlerName);
		};
	this->AddControl(_propertyGrid);
	
	// 设计画布（中间）
	int canvasX = toolBoxWidth + 20;
	int canvasWidth = formWidth - toolBoxWidth - propertyGridWidth - 40;
	_canvas = new DesignerCanvas(canvasX, toolbarHeight + 10, canvasWidth, formHeight - toolbarHeight - 40);
	_canvas->SetControlDescriptors(_controlDescriptors);
	_canvas->SetDesignDataContext(_designDataContext);
	_canvas->OnControlSelected += [this](std::shared_ptr<DesignerControl> control) {
		OnCanvasControlSelected(control);
	};
	_canvas->OnDefaultEventRequested +=
		[this](std::shared_ptr<DesignerControl>)
		{
			if (_propertyGrid)
				(void)_propertyGrid->ActivateDefaultEventHandler();
		};
	_canvas->OnInteractionTransactionCompleted +=
		[this](const DesignerCanvasInteractionTransactionEventArgs& args) {
			OnCanvasInteractionTransactionCompleted(args);
		};
	_canvas->OnCommandCompleted +=
		[this](const DesignerCanvasCommandEventArgs& args) {
			OnCanvasCommandCompleted(args);
		};
	_canvas->OnDocumentStateChanged +=
		[this](const DesignerCanvasDocumentStateEventArgs& args) {
			OnCanvasDocumentStateChanged(args);
		};
	this->AddControl(_canvas);
	UpdateDocumentPresentation();

	this->OnClosing += [this](Form*, bool& canceled) {
		if (_closeApproved)
		{
			DiscardSessionRecoverySnapshot();
			return;
		}
		if (!ConfirmCanReplaceOrCloseDocument())
		{
			canceled = true;
			return;
		}
		_closeApproved = true;
		DiscardSessionRecoverySnapshot();
	};

	// 让 PropertyGrid 能在“编辑页/按钮”时同步更新 DesignerCanvas 的设计器模型
	_propertyGrid->SetDesignerCanvas(_canvas);

	// 窗口大小变化时：自动调整内部控件布局
	auto doLayout = [this, toolbarHeight, toolBoxWidth, propertyGridWidth]() {
		SIZE contentSize = GetLogicalDesignerContentSize(this);
		int w = contentSize.cx;
		int h = contentSize.cy;
		int usableH = h - toolbarHeight - 40;
		if (usableH < 50) usableH = 50;
		if (_toolBox)
		{
			_toolBox->Location = { 10, toolbarHeight + 10 };
			_toolBox->Size = { toolBoxWidth, usableH };
		}
		if (_propertyGrid)
		{
			_propertyGrid->Location = { w - propertyGridWidth - 15, toolbarHeight + 10 };
			_propertyGrid->Size = { propertyGridWidth, usableH };
			// 重新加载以适配宽度变化
			if (_canvas)
				_propertyGrid->LoadControls(
					_canvas->GetSelectedControls(), _canvas->GetSelectedControl());
			else
				_propertyGrid->LoadControl(nullptr);
		}
		if (_canvas)
		{
			int canvasX = toolBoxWidth + 20;
			int canvasW = w - toolBoxWidth - propertyGridWidth - 40;
			if (canvasW < 100) canvasW = 100;
			_canvas->Location = { canvasX, toolbarHeight + 10 };
			_canvas->Size = { canvasW, usableH };
			_canvas->InvalidateVisual();
		}
	};

	this->OnSizeChanged += [doLayout](Form*) { doLayout(); };
	doLayout();
}

void Designer::SetDesignDataContext(std::shared_ptr<IBindingSource> source)
{
	_designDataContext = std::move(source);
	if (_canvas) _canvas->SetDesignDataContext(_designDataContext);
}

void Designer::OnToolBoxControlSelected(
	const DesignerControlDescriptor& descriptor)
{
	_canvas->SetControlToAdd(descriptor);
	_lblInfo->Text = L"请在画布上点击以添加控件";
}

void Designer::OnCanvasControlSelected(std::shared_ptr<DesignerControl> control)
{
	if (_propertyGrid)
	{
		_propertyGrid->CommitPendingEdits();
	}
	if (_canvas)
		_propertyGrid->LoadControls(_canvas->GetSelectedControls(), control);
	else
		_propertyGrid->LoadControl(control);
	
	if (control)
	{
		const auto selectedCount = _canvas
			? _canvas->GetSelectedControls().size() : size_t{ 1 };
		_lblInfo->Text = selectedCount > 1
			? L"已选中 " + std::to_wstring(selectedCount)
				+ L" 个控件（主选: " + control->Name + L"）"
			: L"已选中: " + control->Name;
	}
	else
	{
		_lblInfo->Text = L"就绪";
	}
}

void Designer::OnCanvasInteractionTransactionCompleted(
	const DesignerCanvasInteractionTransactionEventArgs& args)
{
	UpdateCanvasOperationStatus(
		args.Operation, {}, args.Message, args.Result);
}

void Designer::OnCanvasCommandCompleted(
	const DesignerCanvasCommandEventArgs& args)
{
	UpdateCanvasOperationStatus(
		args.Operation, args.Label, args.Message, args.Result);
}

void Designer::OnCanvasDocumentStateChanged(
	const DesignerCanvasDocumentStateEventArgs& args)
{
	RestoreCodeBehindAssociation();
	UpdateCodeFreshnessForDocumentState();
	UpdateDocumentPresentation();
	if (args.IsDirty) ScheduleRecoverySnapshot();
	else DiscardSessionRecoverySnapshot();
}

void Designer::UpdateCanvasOperationStatus(
	const std::wstring& operationName,
	const std::wstring& label,
	const std::wstring& message,
	const DesignerDocumentTransactionResult& result)
{
	if (!_lblInfo) return;
	const auto operation = DescribeCanvasOperation(operationName);
	const auto command = label.empty()
		? std::wstring{} : DescribeCanvasOperation(label);
	std::wstring text;
	if (!result)
	{
		text = L"操作失败（" + operation + L"）：" + result.Error;
	}
	else
	{
		switch (result.State)
		{
		case DesignerDocumentTransactionState::Committed:
			if (operationName == L"Undo")
				text = command.empty() ? L"已撤销。" : L"已撤销：" + command;
			else if (operationName == L"Redo")
				text = command.empty() ? L"已重做。" : L"已重做：" + command;
			else
				text = L"已提交：" + operation;
			break;
		case DesignerDocumentTransactionState::RolledBack:
		case DesignerDocumentTransactionState::Canceled:
			text = message.empty()
				? L"已取消：" + operation
				: message;
			break;
		case DesignerDocumentTransactionState::Unchanged:
			if (!message.empty()) text = message;
			else if (operationName == L"Undo") text = L"没有可撤销的操作。";
			else if (operationName == L"Redo") text = L"没有可重做的操作。";
			else text = L"未发生变化：" + operation;
			break;
		default:
			text = operation;
			break;
		}
	}
	_lblInfo->Text = text;
	_lblInfo->AccessibleDescription = text;
	_lblInfo->InvalidateVisual();
}

bool Designer::ProcessMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	int localX,
	int localY)
{
	if (message == WM_CLOSE && !_closeApproved)
	{
		if (!ConfirmCanReplaceOrCloseDocument()) return true;
		_closeApproved = true;
	}
	if (message == WM_TIMER && wParam == RecoveryTimerId)
	{
		(void)::KillTimer(this->Handle, RecoveryTimerId);
		if (!_recoverySnapshotPending) return true;
		std::wstring recoveryError;
		if (!FlushRecoverySnapshot(&recoveryError)
			&& _lblInfo && !recoveryError.empty())
		{
			_lblInfo->Text = L"自动恢复保存失败：" + recoveryError;
			_lblInfo->AccessibleDescription = _lblInfo->Text;
			_lblInfo->InvalidateVisual();
			if (_canvas && _canvas->IsDocumentDirty())
			{
				_recoverySnapshotPending = true;
				(void)::SetTimer(this->Handle, RecoveryTimerId,
					RecoveryRetryMilliseconds, nullptr);
			}
		}
		return true;
	}
	if (message == WM_TIMER && wParam == CodeFreshnessTimerId)
	{
		(void)::KillTimer(this->Handle, CodeFreshnessTimerId);
		if (!_codeFreshnessInspectionPending) return true;
		_codeFreshnessInspectionPending = false;
		RefreshCodeFreshnessFromFiles();
		UpdateDocumentPresentation();
		return true;
	}
	if (message == WM_ACTIVATEAPP && wParam == TRUE)
	{
		RefreshCodeFreshnessFromFiles();
		UpdateDocumentPresentation();
	}
	const bool captureLost = message == WM_CAPTURECHANGED
		&& reinterpret_cast<HWND>(lParam) != this->Handle;
	const bool interactionCanceled = message == WM_CANCELMODE
		|| message == WM_KILLFOCUS
		|| (message == WM_ACTIVATEAPP && wParam == FALSE)
		|| captureLost;
	if (interactionCanceled && _canvas)
	{
		(void)_canvas->CancelActivePointerInteraction(
			captureLost
				? L"画布失去鼠标捕获，修改已回滚。"
				: L"窗口交互被中断，画布修改已回滚。");
	}
	return Form::ProcessMessage(message, wParam, lParam, localX, localY);
}

void Designer::OnNewClick()
{
	if (!_canvas || !ConfirmCanReplaceOrCloseDocument()) return;
	auto result = _canvas->CreateNewDocument();
	if (!result)
	{
		ShowModalMessage(this, L"新建失败",
			result.Error.empty() ? L"无法创建新文档。" : result.Error);
		return;
	}
	_propertyGrid->Clear();
	_currentFileName.clear();
	_lastExportBasePath.clear();
	_sessionExportBasePaths.clear();
	ResetCodeFreshnessTracking();
	UpdateDocumentPresentation();
	_lblInfo->Text = L"已新建空白文档";
}

void Designer::OnOpenClick()
{
	OpenFileDialog ofd;
	ofd.Filter = MakeDesignFilter();
	ofd.Multiselect = false;
	ofd.Title = "Open Designer File";
	auto r = ofd.ShowDialog(this->Handle);

	// 兜底恢复交互
	if (this->Handle && ::IsWindow(this->Handle))
	{
		::EnableWindow(this->Handle, TRUE);
		::ReleaseCapture();
		::SetForegroundWindow(this->Handle);
		::SetActiveWindow(this->Handle);
		::SetFocus(this->Handle);
	}

	if (r != DialogResult::OK || ofd.SelectedPaths.empty())
		return;

	std::wstring path = Convert::StringToWString(ofd.SelectedPaths[0]);
	if (!ConfirmCanReplaceOrCloseDocument()) return;
	std::wstring err;
	auto result = _canvas->LoadDesignFile(path, &err);
	if (result)
	{
		_sessionExportBasePaths.clear();
		ResetCodeFreshnessTracking();
		_currentFileName = path;
		RestoreCodeBehindAssociation();
		RefreshCodeFreshnessFromFiles();
		_propertyGrid->LoadControl(nullptr);
		UpdateDocumentPresentation();
		_lblInfo->Text = L"已打开: " + path;
	}
	else
	{
		ShowModalMessage(this, L"打开失败", err.empty() ? L"无法加载设计文件。" : err);
	}
}

void Designer::OnSaveClick()
{
	(void)SaveDocumentInteractive();
}

void Designer::OnReloadClick()
{
	if (!_canvas) return;
	if (_currentFileName.empty())
	{
		_lblInfo->Text = L"当前文档尚未关联设计文件";
		return;
	}
	if (!ConfirmCanReplaceOrCloseDocument()) return;

	const auto path = _currentFileName;
	std::wstring error;
	auto result = _canvas->LoadDesignFile(path, &error);
	if (!result)
	{
		ShowModalMessage(this, L"重新加载失败",
			error.empty() ? L"无法重新加载设计文件；当前文档已保留。" : error);
		return;
	}
	RestoreCodeBehindAssociation();
	RefreshCodeFreshnessFromFiles();
	_propertyGrid->LoadControl(nullptr);
	UpdateDocumentPresentation();
	_lblInfo->Text = L"已重新加载: " + path;
}

bool Designer::SaveDocumentInteractive()
{
	if (!_canvas) return false;
	PrepareDocumentLifecycle();
	if (_propertyGrid && _propertyGrid->HasPropertyEditError())
	{
		ShowModalMessage(this, L"保存失败",
			L"属性“" + _propertyGrid->GetPropertyEditErrorProperty()
			+ L"”尚未提交："
			+ _propertyGrid->GetPropertyEditErrorMessage());
		return false;
	}
	std::wstring path = _currentFileName;
	if (path.empty())
	{
		SaveFileDialog sfd;
		sfd.Filter = MakeDesignFilter();
		sfd.Title = "Save Designer File";
		auto r = sfd.ShowDialog(this->Handle);
		if (this->Handle && ::IsWindow(this->Handle))
		{
			::EnableWindow(this->Handle, TRUE);
			::ReleaseCapture();
			::SetForegroundWindow(this->Handle);
			::SetActiveWindow(this->Handle);
			::SetFocus(this->Handle);
		}
		if (r != DialogResult::OK)
			return false;
		path = Convert::StringToWString(sfd.SelectedPath);
		if (path.empty()) return false;
		if (!DesignerModel::HasDesignDocumentExtension(path))
			path += L".cui.xml";
	}

	std::wstring err;
	if (!_lastExportBasePath.empty()
		&& !_canvas->GetCodeBehind().ClassName.empty()
		&& !AssociateCodeBehind(
			_canvas->GetCodeBehind().ClassName,
			_lastExportBasePath, path, &err))
	{
		ShowModalMessage(this, L"保存失败",
			err.empty() ? L"无法更新 code-behind 关联。" : err);
		return false;
	}
	auto result = _canvas->SaveDesignFile(path, &err);
	if (result)
	{
		_currentFileName = path;
		RestoreCodeBehindAssociation();
		RefreshCodeFreshnessFromFiles();
		UpdateDocumentPresentation();
		_lblInfo->Text = L"已保存: " + path;
		return true;
	}
	ShowModalMessage(this, L"保存失败",
		err.empty() ? L"无法保存设计文件。" : err);
	return false;
}

bool Designer::ConfirmCanReplaceOrCloseDocument()
{
	PrepareDocumentLifecycle();
	if (!_canvas || !_canvas->IsDocumentDirty()) return true;
	const auto name = DisplayDocumentName(_currentFileName);
	const int choice = ::MessageBoxW(
		this->Handle,
		(L"“" + name + L"”有未保存的修改。是否先保存？").c_str(),
		L"CUI 窗口设计器",
		MB_YESNOCANCEL | MB_ICONWARNING | MB_SETFOREGROUND);
	if (choice == IDYES) return SaveDocumentInteractive();
	return choice == IDNO;
}

void Designer::PrepareDocumentLifecycle()
{
	if (_canvas)
		(void)_canvas->CancelActivePointerInteraction(
			L"文档操作中断了画布预览，修改已回滚。");
	if (_propertyGrid) _propertyGrid->CommitPendingEdits();
}

void Designer::UpdateDocumentPresentation()
{
	const bool dirty = _canvas && _canvas->IsDocumentDirty();
	std::wstring title = L"CUI 窗口设计器 - "
		+ DisplayDocumentName(_currentFileName);
	if (dirty) title += L" *";
	this->Text = title;
	if (_btnReload) _btnReload->Enable = !_currentFileName.empty();
	if (_btnRegenerate)
	{
		const bool available = !_lastExportBasePath.empty()
			&& _canvas && !_canvas->GetCodeBehind().ClassName.empty();
		_btnRegenerate->Enable = available;
		std::wstring caption = L"重新生成";
		std::wstring description = available
			? L"重新生成当前文档的 code-behind 文件。"
			: L"当前文档尚未建立 code-behind 目标。";
		if (available)
		{
			switch (_codeFreshness.State)
			{
			case DesignerModel::DesignCodeFreshnessState::Current:
				description = L"生成代码与当前设计完全一致。";
				break;
			case DesignerModel::DesignCodeFreshnessState::Stale:
				caption += L" *";
				description = L"设计内容已变化，需要重新生成代码。";
				break;
			case DesignerModel::DesignCodeFreshnessState::Missing:
				caption += L" !";
				description = L"代码文件不完整，需要重新生成；缺少 "
					+ std::to_wstring(_codeFreshness.MissingFiles.size())
					+ L" 个文件。";
				break;
			case DesignerModel::DesignCodeFreshnessState::Blocked:
				caption = L"生成受阻 !";
				description = _codeFreshness.Diagnostic.empty()
					? L"当前用户代码或生成目标阻止了安全重新生成。"
					: _codeFreshness.Diagnostic;
				break;
			default:
				caption += L" ?";
				description = L"尚未检查生成代码状态。";
				break;
			}
		}
		_btnRegenerate->Text = std::move(caption);
		_btnRegenerate->AccessibleName = L"重新生成代码";
		_btnRegenerate->AccessibleDescription = std::move(description);
		_btnRegenerate->InvalidateVisual();
	}
	if (this->Handle && ::IsWindow(this->Handle))
		::SetWindowTextW(this->Handle, title.c_str());
}

void Designer::InitializeRecoverySession()
{
	std::wstring error;
	if (!DesignerModel::DesignRecoveryStore::GetDefaultDirectory(
		_recoveryDirectory, &error))
	{
		_recoveryDirectory.clear();
		_sessionRecoveryPath.clear();
		if (_lblInfo && !error.empty())
		{
			_lblInfo->Text = L"自动恢复不可用：" + error;
			_lblInfo->AccessibleDescription = _lblInfo->Text;
		}
		return;
	}
	_recoveryProcessStartTime =
		DesignerModel::DesignRecoveryStore::GetCurrentProcessStartTime();
	_sessionRecoveryPath =
		DesignerModel::DesignRecoveryStore::MakeSessionFilePath(
			_recoveryDirectory, ::GetCurrentProcessId(),
			_recoveryProcessStartTime);
}

void Designer::TryRestoreRecoveryOnStartup()
{
	if (!_canvas || _recoveryDirectory.empty()) return;
	std::vector<DesignerModel::DesignRecoveryFile> files;
	std::wstring enumerateError;
	if (!DesignerModel::DesignRecoveryStore::EnumerateRecoveryFiles(
		_recoveryDirectory, files, &enumerateError))
	{
		ShowModalMessage(this, L"自动恢复不可用", enumerateError);
		return;
	}

	bool reportedCorruptFile = false;
	for (const auto& file : files)
	{
		if (file.Path == _sessionRecoveryPath) continue;
		DesignerModel::DesignRecoverySnapshot snapshot;
		std::wstring loadError;
		if (!DesignerModel::DesignRecoveryStore::LoadFromFile(
			file.Path, snapshot, &loadError))
		{
			std::wstring quarantinePath;
			std::wstring quarantineError;
			const bool quarantined =
				DesignerModel::DesignRecoveryStore::QuarantineFile(
					file.Path, &quarantinePath, &quarantineError);
			if (!reportedCorruptFile)
			{
				reportedCorruptFile = true;
				std::wstring message = L"发现无法读取的自动恢复文件：\n"
					+ loadError;
				if (quarantined)
					message += L"\n文件已隔离为：\n" + quarantinePath;
				else if (!quarantineError.empty())
					message += L"\n隔离失败：" + quarantineError;
				ShowModalMessage(this, L"自动恢复文件损坏", message);
			}
			continue;
		}
		if (DesignerModel::DesignRecoveryStore::IsOwnerProcessRunning(snapshot))
			continue;

		const auto documentName = snapshot.OriginalFilePath.empty()
			? std::wstring(L"未命名文档")
			: snapshot.OriginalFilePath;
		const int choice = ::MessageBoxW(
			this->Handle,
			(L"发现上次异常退出留下的自动恢复内容：\n\n"
				+ documentName
				+ L"\n\n是否恢复？选择“否”会删除该恢复文件，选择“取消”会保留到下次启动。")
				.c_str(),
			L"恢复未保存的设计",
			MB_YESNOCANCEL | MB_ICONWARNING | MB_SETFOREGROUND);
		if (choice == IDCANCEL) return;
		if (choice == IDNO)
		{
			std::wstring deleteError;
			if (!DesignerModel::DesignRecoveryStore::DeleteFile(
				file.Path, &deleteError) && !deleteError.empty())
				ShowModalMessage(this, L"删除恢复文件失败", deleteError);
			return;
		}

		auto result = _canvas->RestoreRecoveredDocument(snapshot.Document);
		if (!result)
		{
			std::wstring quarantinePath;
			std::wstring quarantineError;
			(void)DesignerModel::DesignRecoveryStore::QuarantineFile(
				file.Path, &quarantinePath, &quarantineError);
			ShowModalMessage(this, L"恢复失败",
				result.Error.empty() ? L"恢复文档无法应用。" : result.Error);
			return;
		}

		_currentFileName = snapshot.OriginalFilePath;
		_sessionExportBasePaths.clear();
		ResetCodeFreshnessTracking();
		RestoreCodeBehindAssociation();
		RefreshCodeFreshnessFromFiles();
		_propertyGrid->LoadControl(nullptr);
		UpdateDocumentPresentation();
		std::wstring snapshotError;
		if (FlushRecoverySnapshot(&snapshotError))
		{
			std::wstring deleteError;
			(void)DesignerModel::DesignRecoveryStore::DeleteFile(
				file.Path, &deleteError);
		}
		else if (!snapshotError.empty())
		{
			ShowModalMessage(this, L"自动恢复保存失败",
				L"文档已恢复，但新的恢复快照无法写入。原恢复文件将保留。\n"
				+ snapshotError);
		}
		_lblInfo->Text = L"已恢复未保存的设计: " + documentName;
		_lblInfo->AccessibleDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
		return;
	}
}

void Designer::ScheduleRecoverySnapshot()
{
	if (!_canvas || !_canvas->IsDocumentDirty()
		|| _sessionRecoveryPath.empty())
		return;
	_recoverySnapshotPending = true;
	if (!this->Handle || !::IsWindow(this->Handle)
		|| ::SetTimer(this->Handle, RecoveryTimerId,
			RecoveryDelayMilliseconds, nullptr) == 0)
	{
		std::wstring ignored;
		(void)FlushRecoverySnapshot(&ignored);
	}
}

bool Designer::FlushRecoverySnapshot(std::wstring* outError)
{
	if (outError) outError->clear();
	if (this->Handle && ::IsWindow(this->Handle))
		(void)::KillTimer(this->Handle, RecoveryTimerId);
	_recoverySnapshotPending = false;
	if (!_canvas || _sessionRecoveryPath.empty()) return true;
	if (!_canvas->IsDocumentDirty())
	{
		DiscardSessionRecoverySnapshot();
		return true;
	}
	if (_canvas->HasActiveDocumentTransaction())
	{
		ScheduleRecoverySnapshot();
		return true;
	}

	DesignerModel::DesignDocument document;
	std::wstring buildError;
	if (!_canvas->BuildDesignDocument(document, &buildError))
	{
		if (outError) *outError = buildError.empty()
			? L"无法构建恢复文档。" : buildError;
		return false;
	}
	DesignerModel::DesignRecoverySnapshot snapshot;
	snapshot.OwnerProcessId = ::GetCurrentProcessId();
	snapshot.OwnerProcessStartTime = _recoveryProcessStartTime;
	snapshot.OriginalFilePath = _currentFileName;
	snapshot.Document = std::move(document);
	return DesignerModel::DesignRecoveryStore::SaveToFile(
		snapshot, _sessionRecoveryPath, outError);
}

void Designer::DiscardSessionRecoverySnapshot()
{
	if (this->Handle && ::IsWindow(this->Handle))
		(void)::KillTimer(this->Handle, RecoveryTimerId);
	_recoverySnapshotPending = false;
	if (_sessionRecoveryPath.empty()) return;
	std::wstring error;
	if (!DesignerModel::DesignRecoveryStore::DeleteFile(
		_sessionRecoveryPath, &error) && _lblInfo && !error.empty())
	{
		_lblInfo->Text = L"自动恢复清理失败：" + error;
		_lblInfo->AccessibleDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
	}
}

bool Designer::GenerateCodeFiles(
	const std::wstring& basePath,
	std::wstring* outError,
	const std::wstring& className)
{
	if (outError) outError->clear();
	if (!_canvas || basePath.empty())
	{
		if (outError) *outError = L"代码导出目标不可用。";
		return false;
	}
	try
	{
		DesignerModel::DesignDocument document;
		std::wstring error;
		if (!_canvas->BuildDesignDocument(document, &error))
		{
			if (outError) *outError = error.empty()
				? L"无法构建设计文档。" : std::move(error);
			return false;
		}
		DesignerModel::DesignCodeGenerationOptions options;
		options.OutputBasePath = basePath;
		options.ClassName = className;
		DesignerModel::DesignCodeGenerationResult result;
		if (!DesignerModel::DesignCodeGenerationService::Generate(
			document, _currentFileName, options, &result, &error))
		{
			if (outError) *outError = error.empty()
				? L"导出失败，请检查文件路径。" : std::move(error);
			return false;
		}
		_lastExportBasePath = result.OutputBasePath;
		_sessionExportBasePaths[result.ClassName] = result.OutputBasePath;
		RecordGeneratedCodeState(result);
		return true;
	}
	catch (...)
	{
		if (outError) *outError = L"准备代码导出时发生未知错误。";
		return false;
	}
}

bool Designer::GenerateAndAssociateCodeFiles(
	const std::wstring& basePath,
	const std::wstring& className,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!_canvas || basePath.empty() || className.empty())
	{
		if (outError) *outError = L"代码导出事务参数无效。";
		return false;
	}
	try
	{
		DesignerModel::DesignDocument document;
		std::wstring error;
		if (!_canvas->BuildDesignDocument(document, &error))
		{
			if (outError) *outError = error.empty()
				? L"无法构建设计文档。" : std::move(error);
			return false;
		}

		DesignerModel::DesignCodeGenerationOptions options;
		options.OutputBasePath = basePath;
		options.ClassName = className;
		DesignerModel::DesignCodeGenerationResult result;
		if (!DesignerModel::DesignCodeGenerationService::GenerateAndCommit(
			document, _currentFileName, options,
			[this, className, basePath](
				const DesignerModel::DesignCodeGenerationResult&,
				std::wstring& commitError)
			{
				return AssociateCodeBehind(
					className, basePath, _currentFileName, &commitError);
			},
			&result, &error))
		{
			if (outError) *outError = error.empty()
				? L"代码导出事务失败。" : std::move(error);
			return false;
		}

		_lastExportBasePath = result.OutputBasePath;
		_sessionExportBasePaths[result.ClassName] = result.OutputBasePath;
		RecordGeneratedCodeState(result);
		return true;
	}
	catch (...)
	{
		if (outError) *outError = L"准备代码导出事务时发生未知错误。";
		return false;
	}
}

bool Designer::AssociateCodeBehind(
	const std::wstring& className,
	const std::wstring& basePath,
	const std::wstring& designFilePath,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!_canvas)
	{
		if (outError) *outError = L"设计画布不可用。";
		return false;
	}

	DesignerModel::DesignCodeBehindModel association;
	if (!DesignerModel::DesignCodeGenerationService::BuildCodeBehindAssociation(
		className, basePath, designFilePath, association, outError))
		return false;

	if (_canvas->GetCodeBehind() == association) return true;
	auto result = _canvas->ExecuteDocumentEditTransaction(
		L"关联 code-behind",
		[this, association](std::wstring& error)
		{
			return _canvas->SetCodeBehind(association, &error);
		});
	if (!result)
	{
		if (outError) *outError = result.Error.empty()
			? L"无法把 code-behind 关联写入设计文档。"
			: result.Error;
		return false;
	}
	return true;
}

void Designer::RestoreCodeBehindAssociation()
{
	_lastExportBasePath.clear();
	if (!_canvas) return;
	const auto& association = _canvas->GetCodeBehind();
	if (association.ClassName.empty()) return;
	if (association.RelativeBasePath.empty() || _currentFileName.empty())
	{
		const auto session = _sessionExportBasePaths.find(
			association.ClassName);
		if (session != _sessionExportBasePaths.end())
			_lastExportBasePath = session->second;
		return;
	}
	try
	{
		_lastExportBasePath = (
			std::filesystem::absolute(std::filesystem::path(_currentFileName))
				.parent_path()
			/ std::filesystem::path(association.RelativeBasePath))
			.lexically_normal().wstring();
	}
	catch (...)
	{
		_lastExportBasePath.clear();
	}
}

void Designer::PublishEventHandlerCodeInspection(
	DesignerModel::DesignEventHandlerCodeInspection inspection)
{
	_eventCodeInspection = std::move(inspection);
	if (_propertyGrid)
		_propertyGrid->SetEventHandlerCodeInspection(_eventCodeInspection);
}

void Designer::RefreshEventHandlerCodeInspection(
	const DesignerModel::DesignDocument& document,
	const DesignerModel::DesignCodeGenerationOptions& options)
{
	DesignerModel::DesignEventHandlerCodeInspection inspection;
	std::wstring error;
	if (!DesignerModel::DesignCodeGenerationService::InspectEventHandlers(
		document, _currentFileName, options, inspection, &error))
	{
		inspection = {};
		inspection.Associated = !options.ClassName.empty()
			&& !options.OutputBasePath.empty();
		inspection.Target.ClassName = options.ClassName;
		inspection.Target.OutputBasePath = options.OutputBasePath;
		if (!options.OutputBasePath.empty())
		{
			inspection.Target.UserHeaderPath =
				options.OutputBasePath + L".h";
			inspection.Target.UserSourcePath =
				options.OutputBasePath + L".cpp";
		}
		inspection.Diagnostic = error.empty()
			? L"事件处理函数代码检查失败。" : std::move(error);
	}
	PublishEventHandlerCodeInspection(std::move(inspection));
}

std::wstring Designer::CurrentCodeFreshnessTargetKey() const
{
	if (!_canvas || _lastExportBasePath.empty()) return {};
	const auto& association = _canvas->GetCodeBehind();
	if (association.ClassName.empty()) return {};
	std::wstring path = _lastExportBasePath;
	std::replace(path.begin(), path.end(), L'/', L'\\');
	std::transform(path.begin(), path.end(), path.begin(), [](wchar_t value)
	{
		return static_cast<wchar_t>(std::towlower(value));
	});
	return association.ClassName + L"\n" + path;
}

void Designer::ResetCodeFreshnessTracking()
{
	if (this->Handle && ::IsWindow(this->Handle))
		(void)::KillTimer(this->Handle, CodeFreshnessTimerId);
	_codeFreshnessInspectionPending = false;
	_codeFreshness = {};
	_codeFreshnessTargetKey.clear();
	_currentCodeStateIds.clear();
	PublishEventHandlerCodeInspection({});
}

void Designer::ScheduleCodeFreshnessInspection()
{
	const auto targetKey = CurrentCodeFreshnessTargetKey();
	if (targetKey.empty())
	{
		if (this->Handle && ::IsWindow(this->Handle))
			(void)::KillTimer(this->Handle, CodeFreshnessTimerId);
		_codeFreshnessInspectionPending = false;
		_codeFreshness = {};
		_codeFreshnessTargetKey.clear();
		PublishEventHandlerCodeInspection({});
		return;
	}
	_codeFreshnessInspectionPending = true;
	if (!this->Handle || !::IsWindow(this->Handle)
		|| ::SetTimer(this->Handle, CodeFreshnessTimerId,
			CodeFreshnessDelayMilliseconds, nullptr) == 0)
	{
		_codeFreshnessInspectionPending = false;
		RefreshCodeFreshnessFromFiles();
	}
}

void Designer::RefreshCodeFreshnessFromFiles()
{
	if (this->Handle && ::IsWindow(this->Handle))
		(void)::KillTimer(this->Handle, CodeFreshnessTimerId);
	_codeFreshnessInspectionPending = false;
	const auto targetKey = CurrentCodeFreshnessTargetKey();
	if (!_canvas || targetKey.empty())
	{
		_codeFreshness = {};
		_codeFreshnessTargetKey.clear();
		PublishEventHandlerCodeInspection({});
		return;
	}

	DesignerModel::DesignDocument document;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(document, &error))
	{
		_codeFreshness = {};
		_codeFreshness.State =
			DesignerModel::DesignCodeFreshnessState::Blocked;
		_codeFreshness.Diagnostic = error.empty()
			? L"无法构建用于新鲜度检查的设计文档。" : std::move(error);
		_codeFreshnessTargetKey = targetKey;
		DesignerModel::DesignEventHandlerCodeInspection inspection;
		inspection.Associated = true;
		inspection.Pending = false;
		inspection.Target.ClassName = _canvas->GetCodeBehind().ClassName;
		inspection.Target.OutputBasePath = _lastExportBasePath;
		inspection.Target.UserHeaderPath = _lastExportBasePath + L".h";
		inspection.Target.UserSourcePath = _lastExportBasePath + L".cpp";
		inspection.Diagnostic = _codeFreshness.Diagnostic;
		PublishEventHandlerCodeInspection(std::move(inspection));
		return;
	}

	DesignerModel::DesignCodeGenerationOptions options;
	options.ClassName = _canvas->GetCodeBehind().ClassName;
	options.OutputBasePath = _lastExportBasePath;
	DesignerModel::DesignCodeFreshnessResult freshness;
	if (!DesignerModel::DesignCodeGenerationService::InspectFreshness(
		document, _currentFileName, options, freshness, &error))
	{
		freshness = {};
		freshness.State = DesignerModel::DesignCodeFreshnessState::Blocked;
		freshness.Diagnostic = error.empty()
			? L"代码生成新鲜度检查失败。" : std::move(error);
	}
	_codeFreshness = std::move(freshness);
	_codeFreshnessTargetKey = targetKey;
	RefreshEventHandlerCodeInspection(document, options);
	if (_codeFreshness.State
		== DesignerModel::DesignCodeFreshnessState::Current)
	{
		auto& states = _currentCodeStateIds[targetKey];
		states.insert(_canvas->GetCurrentDocumentStateId());
		while (states.size() > 256) states.erase(states.begin());
	}
}

void Designer::UpdateCodeFreshnessForDocumentState()
{
	const auto targetKey = CurrentCodeFreshnessTargetKey();
	if (!_canvas || targetKey.empty())
	{
		_codeFreshness = {};
		_codeFreshnessTargetKey.clear();
		PublishEventHandlerCodeInspection({});
		return;
	}

	_codeFreshness = {};
	_codeFreshnessTargetKey = targetKey;
	const auto knownTarget = _currentCodeStateIds.find(targetKey);
	const bool knownCurrent = knownTarget != _currentCodeStateIds.end()
		&& knownTarget->second.find(_canvas->GetCurrentDocumentStateId())
			!= knownTarget->second.end();
	_codeFreshness.State = knownCurrent
		? DesignerModel::DesignCodeFreshnessState::Current
		: DesignerModel::DesignCodeFreshnessState::Stale;
	DesignerModel::DesignEventHandlerCodeInspection pending;
	pending.Associated = true;
	pending.Pending = true;
	pending.Target.ClassName = _canvas->GetCodeBehind().ClassName;
	pending.Target.OutputBasePath = _lastExportBasePath;
	pending.Target.UserHeaderPath = _lastExportBasePath + L".h";
	pending.Target.UserSourcePath = _lastExportBasePath + L".cpp";
	PublishEventHandlerCodeInspection(std::move(pending));
	ScheduleCodeFreshnessInspection();
}

void Designer::RecordGeneratedCodeState(
	const DesignerModel::DesignCodeGenerationResult& result)
{
	if (this->Handle && ::IsWindow(this->Handle))
		(void)::KillTimer(this->Handle, CodeFreshnessTimerId);
	_codeFreshnessInspectionPending = false;
	_codeFreshness = {};
	_codeFreshness.State = DesignerModel::DesignCodeFreshnessState::Current;
	_codeFreshness.Target = result;
	_codeFreshnessTargetKey = CurrentCodeFreshnessTargetKey();
	if (_canvas && !_codeFreshnessTargetKey.empty())
	{
		auto& states = _currentCodeStateIds[_codeFreshnessTargetKey];
		states.insert(_canvas->GetCurrentDocumentStateId());
		while (states.size() > 256) states.erase(states.begin());
	}
	if (_canvas)
	{
		DesignerModel::DesignDocument document;
		std::wstring error;
		if (_canvas->BuildDesignDocument(document, &error))
		{
			DesignerModel::DesignCodeGenerationOptions options;
			options.ClassName = result.ClassName;
			options.OutputBasePath = result.OutputBasePath;
			RefreshEventHandlerCodeInspection(document, options);
		}
		else
		{
			DesignerModel::DesignEventHandlerCodeInspection inspection;
			inspection.Associated = true;
			inspection.Target = result;
			inspection.Diagnostic = error.empty()
				? L"生成后无法重建事件代码检查文档。" : std::move(error);
			PublishEventHandlerCodeInspection(std::move(inspection));
		}
	}
	UpdateDocumentPresentation();
}

void Designer::OnExportClick()
{
	PrepareDocumentLifecycle();
	auto controls = _canvas->GetAllControlsForExport();

	int exportCount = (int)controls.size();
	int buttonCount = 0;
	int gridPanelCount = 0;
	for (const auto& dc : controls)
	{
		if (!dc) continue;
		if (dc->Type == UIClass::UI_Button) buttonCount++;
		if (dc->Type == UIClass::UI_GridPanel) gridPanelCount++;
	}
	
	SaveFileDialog saveFileDialog;
	saveFileDialog.Filter = std::string("C++ Files (*.h;*.cpp)\0*.h;*.cpp\0\0\0",35);
	DialogResult dialogResult = saveFileDialog.ShowDialog(this->Handle);
	
	// 保险措施：某些自定义/封装的对话框实现可能会禁用 owner 窗口后未恢复，
	// 导致主窗体“保存完毕后无法交互”。这里强制恢复交互与焦点。
	if (this->Handle && ::IsWindow(this->Handle))
	{
		::EnableWindow(this->Handle, TRUE);
		::ReleaseCapture();
		::SetForegroundWindow(this->Handle);
		::SetActiveWindow(this->Handle);
		::SetFocus(this->Handle);
	}
	// 有些实现会选择“进程内最顶层窗口”作为 owner 并禁用它，这里也一并兜底恢复。
	{
		HWND topMost = GetTopMostWindowInCurrentProcess();
		if (topMost && ::IsWindow(topMost))
		{
			::EnableWindow(topMost, TRUE);
		}
	}

	if (dialogResult == DialogResult::OK)
	{
		std::wstring selectedPath = Convert::StringToWString(saveFileDialog.SelectedPath);
		if (selectedPath.empty())
			return;

		std::wstring basePath = selectedPath;
		size_t lastSlash = basePath.find_last_of(L"\\/");
		size_t lastDot = basePath.find_last_of(L'.');
		bool hasExt = (lastDot != std::wstring::npos)
			&& (lastDot != basePath.size() - 1)
			&& ((lastSlash == std::wstring::npos) || (lastDot > lastSlash + 1));
		if (hasExt)
		{
			basePath = basePath.substr(0, lastDot);
		}
		else
		{
			if (!basePath.empty() && basePath.back() == L'.')
				basePath.pop_back();
		}

		std::wstring headerPath = basePath + L".h";
		std::wstring cppPath = basePath + L".cpp";

		std::wstring fileName = basePath;
		lastSlash = fileName.find_last_of(L"\\/");
		if (lastSlash != std::wstring::npos)
			fileName = fileName.substr(lastSlash + 1);
		
		std::wstring exportError;
		const auto& existingAssociation = _canvas->GetCodeBehind();
		const std::wstring suggestedClassName = existingAssociation.ClassName.empty()
			? fileName : existingAssociation.ClassName;
		CodeBehindExportDialog exportDialog(
			existingAssociation, suggestedClassName,
			basePath, _currentFileName);
		exportDialog.ShowDialog(this->Handle);
		if (!exportDialog.Applied) return;
		const auto className = exportDialog.ClassName;
		const bool exported = GenerateAndAssociateCodeFiles(
			basePath, className, &exportError);
		if (exported)
		{
			const std::wstring generatedHeaderPath = basePath + L".g.h";
			const std::wstring generatedCppPath = basePath + L".g.cpp";
			const std::wstring handlerIncludePath = basePath + L".handlers.g.inc";
			UpdateDocumentPresentation();
			_lblInfo->Text = L"代码导出成功: " + className + L" (控件:" + std::to_wstring(exportCount)
				+ L", GridPanel:" + std::to_wstring(gridPanelCount)
				+ L", Button:" + std::to_wstring(buttonCount) + L")";
			ShowModalMessage(this, L"导出成功", (L"代码已成功导出到:\n"
				+ headerPath + L"\n" + cppPath + L"\n"
				+ generatedHeaderPath + L"\n" + generatedCppPath + L"\n"
				+ handlerIncludePath
				+ L"\n\n.h/.cpp 仅首次创建，.g.* 可安全重新生成。"
				+ L"\n\n导出统计：控件=" + std::to_wstring(exportCount)
				+ L"，GridPanel=" + std::to_wstring(gridPanelCount)
				+ L"，Button=" + std::to_wstring(buttonCount)));
		}
		else
		{
			_lblInfo->Text = L"导出失败";
			ShowModalMessage(this, L"错误", exportError);
		}
	}
}

void Designer::OnRegenerateCodeClick()
{
	PrepareDocumentLifecycle();
	if (!_canvas || _lastExportBasePath.empty()
		|| _canvas->GetCodeBehind().ClassName.empty())
	{
		if (_lblInfo)
		{
			_lblInfo->Text = L"当前文档尚未建立可重新生成的 code-behind 目标。";
			_lblInfo->AccessibleDescription = _lblInfo->Text;
			_lblInfo->InvalidateVisual();
		}
		return;
	}

	std::wstring error;
	if (!GenerateCodeFiles(_lastExportBasePath, &error))
	{
		_lblInfo->Text = L"代码重新生成失败："
			+ (error.empty() ? L"未知错误。" : error);
		_lblInfo->AccessibleDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
		return;
	}

	UpdateDocumentPresentation();
	_lblInfo->Text = L"代码已重新生成：" + _lastExportBasePath;
	_lblInfo->AccessibleDescription = _lblInfo->Text;
	_lblInfo->InvalidateVisual();
}

void Designer::OnEventHandlerActivated(const std::wstring& handlerName)
{
	if (handlerName.empty() || !_lblInfo) return;
	if (_lastExportBasePath.empty())
	{
		_lblInfo->Text = L"处理函数已就绪: " + handlerName
			+ L"。首次“导出代码”后，双击会更新并打开用户源文件。";
		_lblInfo->AccessibleDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
		return;
	}

	const auto inspected = _eventCodeInspection.Handlers.find(handlerName);
	const auto inspectedState = inspected == _eventCodeInspection.Handlers.end()
		? DesignerModel::DesignEventHandlerCodeState::DefinitionMissing
		: inspected->second.State;
	const bool signatureMismatch = !_eventCodeInspection.Pending
		&& inspected != _eventCodeInspection.Handlers.end()
		&& inspectedState
			== DesignerModel::DesignEventHandlerCodeState::SignatureMismatch;
	const bool duplicateDefinition = !_eventCodeInspection.Pending
		&& inspected != _eventCodeInspection.Handlers.end()
		&& inspectedState
			== DesignerModel::DesignEventHandlerCodeState::DuplicateDefinition;
	const bool currentDefinition = !_eventCodeInspection.Pending
		&& inspected != _eventCodeInspection.Handlers.end()
		&& inspectedState == DesignerModel::DesignEventHandlerCodeState::Current;
	const bool generatedCodeCurrent = _codeFreshness.State
		== DesignerModel::DesignCodeFreshnessState::Current
		&& _codeFreshnessTargetKey == CurrentCodeFreshnessTargetKey();
	const bool navigateWithoutGeneration = signatureMismatch
		|| duplicateDefinition || (currentDefinition && generatedCodeCurrent);

	bool generated = false;
	std::wstring error;
	if (!navigateWithoutGeneration
		&& !GenerateCodeFiles(_lastExportBasePath, &error))
	{
		_lblInfo->Text = L"处理函数代码更新失败: "
			+ (error.empty() ? handlerName : error);
		_lblInfo->AccessibleDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
		return;
	}
	generated = !navigateWithoutGeneration;

	const auto definitionPath = inspected != _eventCodeInspection.Handlers.end()
		? inspected->second.DefinitionFilePath : std::wstring{};
	const auto sourcePath = !definitionPath.empty()
		? definitionPath
		: !_eventCodeInspection.Target.UserSourcePath.empty()
			? _eventCodeInspection.Target.UserSourcePath
			: _lastExportBasePath + L".cpp";
	const std::string parameterList = inspected == _eventCodeInspection.Handlers.end()
		? std::string{}
		: std::string(inspected->second.ParameterList.begin(),
			inspected->second.ParameterList.end());
	const auto inspectedLine = inspected != _eventCodeInspection.Handlers.end()
		&& inspected->second.DefinitionFilePath == sourcePath
		? inspected->second.DefinitionLine : 0;
	const auto line = inspectedLine > 0
		? inspectedLine
		: SourceCodeNavigator::FindMemberDefinitionLine(
			sourcePath, handlerName,
			_canvas ? _canvas->GetCodeBehind().ClassName : std::wstring{},
			parameterList);
	SourceCodeNavigationResult navigation;
	std::wstring navigationError;
	if (!SourceCodeNavigator::Open(
		this->Handle, sourcePath, line, &navigation, &navigationError))
	{
		_lblInfo->Text = generated
			? L"处理函数已生成，但无法打开用户代码文件："
			: signatureMismatch || duplicateDefinition
				? L"已发现处理函数代码错误，但无法打开用户代码文件："
				: L"无法打开处理函数用户代码文件：";
		_lblInfo->Text += navigationError.empty()
			? sourcePath : navigationError;
	}
	else
	{
		const bool exact = line > 0 && navigation.Plan.RequestsExactLine;
		if (signatureMismatch)
			_lblInfo->Text = exact
				? L"已定位签名错误的处理函数 " + handlerName
				: L"已打开签名错误的处理函数 " + handlerName;
		else if (duplicateDefinition)
			_lblInfo->Text = exact
				? L"已定位重复定义的处理函数 " + handlerName
				: L"已打开重复定义的处理函数 " + handlerName;
		else if (generated)
			_lblInfo->Text = exact
				? L"已更新并定位处理函数 " + handlerName
				: L"已更新并打开处理函数 " + handlerName;
		else
			_lblInfo->Text = exact
				? L"已定位处理函数 " + handlerName
				: L"已打开处理函数 " + handlerName;
		_lblInfo->Text += L"：" + sourcePath;
		if (signatureMismatch)
			_lblInfo->Text += L"（请修正参数签名后重新生成）";
		else if (duplicateDefinition)
			_lblInfo->Text += L"（请仅保留一个相同签名定义）";
		if (line > 0 && !exact)
			_lblInfo->Text += L"（目标第 " + std::to_wstring(line)
				+ L" 行；当前编辑器未提供精确定位）";
		if (navigation.UsedShellFallback)
			_lblInfo->Text += L"（编辑器启动失败，已回退文件关联）";
	}
	_lblInfo->AccessibleDescription = _lblInfo->Text;
	_lblInfo->InvalidateVisual();
}

void Designer::OnDeleteClick()
{
	(void)_canvas->DeleteSelectedControl();
}
