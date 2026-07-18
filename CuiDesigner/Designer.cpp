#include "Designer.h"
#include "CodeBehindExportDialog.h"
#include "XamlEditorDialog.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "DesignerModel/DesignDocumentFileFormat.h"
#include "DesignerModel/DesignRecoveryStore.h"
#include "SourceCodeNavigator.h"
#include "../CUI/include/Core/Threading.h"
#include <Utils.h>
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>

namespace
{
	bool IsControlWithin(Control* control, Control* ancestor)
	{
		for (auto* current = control; current; current = current->Parent)
			if (current == ancestor) return true;
		return false;
	}

	bool IsOutlineShortcutKey(WPARAM key, bool controlDown)
	{
		if (!controlDown) return key == VK_DELETE;
		switch (key)
		{
		case 'C':
		case 'X':
		case 'V':
		case 'D':
		case 'L':
		case 'Z':
		case 'Y':
		case 'A':
			return true;
		default:
			return false;
		}
	}

	wchar_t OutlineShortcutControlCharacter(WPARAM key)
	{
		switch (key)
		{
		case 'A': return L'\x01';
		case 'C': return L'\x03';
		case 'D': return L'\x04';
		case 'L': return L'\x0c';
		case 'V': return L'\x16';
		case 'X': return L'\x18';
		case 'Y': return L'\x19';
		case 'Z': return L'\x1a';
		default: return L'\0';
		}
	}

	WPARAM OutlineShortcutKeyFromControlCharacter(wchar_t character)
	{
		switch (character)
		{
		case L'\x01': return 'A';
		case L'\x03': return 'C';
		case L'\x04': return 'D';
		case L'\x0c': return 'L';
		case L'\x16': return 'V';
		case L'\x18': return 'X';
		case L'\x19': return 'Y';
		case L'\x1a': return 'Z';
		default: return 0;
		}
	}

	enum ArrangeMenuCommandId
	{
		CanvasUndo = 30901,
		CanvasRedo,
		CanvasCut,
		CanvasCopy,
		CanvasPaste,
		CanvasPasteInPlace,
		CanvasPasteHere,
		CanvasDuplicate,
		CanvasDelete,
		CanvasToggleLock,
		CanvasSelectAll,
		CanvasEditXaml,
		CanvasViewFit,
		CanvasViewActualSize,
		CanvasViewZoomIn,
		CanvasViewZoomOut,
		CanvasToggleGrid,
		CanvasToggleSnapGrid,
		CanvasToggleSnapGuides,
		CanvasGridSize5,
		CanvasGridSize10,
		CanvasGridSize20,
		CanvasToggleTabOrder,
		CanvasAutoTabOrder,
		ArrangeDuplicate = 31001,
		ArrangeAlignLeft,
		ArrangeAlignHorizontalCenters,
		ArrangeAlignRight,
		ArrangeAlignTop,
		ArrangeAlignVerticalCenters,
		ArrangeAlignBottom,
		ArrangeDistributeHorizontally,
		ArrangeDistributeVertically,
		ArrangeMakeSameWidth,
		ArrangeMakeSameHeight,
		ArrangeMakeSameSize,
		ArrangeBringForward,
		ArrangeSendBackward,
		ArrangeBringToFront,
		ArrangeSendToBack,
	};

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
		if (operation == L"SetTabOrder") return L"设置 Tab 顺序";
		if (operation == L"AutoTabOrder") return L"自动编排 Tab 顺序";
		if (operation == L"SetLocked") return L"锁定控件";
		if (operation == L"UnlockControls") return L"解除锁定";
		if (operation == L"CopySelection") return L"复制控件";
		if (operation == L"CutSelection") return L"剪切控件";
		if (operation == L"PasteSelection") return L"粘贴控件";
		if (operation == L"DuplicateSelection") return L"重复控件";
		if (operation == L"EditXaml") return L"实时编辑 XAML";
		if (operation == L"AlignLeft") return L"左对齐";
		if (operation == L"AlignHorizontalCenters") return L"水平居中对齐";
		if (operation == L"AlignRight") return L"右对齐";
		if (operation == L"AlignTop") return L"顶端对齐";
		if (operation == L"AlignVerticalCenters") return L"垂直居中对齐";
		if (operation == L"AlignBottom") return L"底端对齐";
		if (operation == L"DistributeHorizontally") return L"水平分布";
		if (operation == L"DistributeVertically") return L"垂直分布";
		if (operation == L"MakeSameWidth") return L"相同宽度";
		if (operation == L"MakeSameHeight") return L"相同高度";
		if (operation == L"MakeSameSize") return L"相同尺寸";
		if (operation == L"BringForward") return L"上移一层";
		if (operation == L"SendBackward") return L"下移一层";
		if (operation == L"BringToFront") return L"置于顶层";
		if (operation == L"SendToBack") return L"置于底层";
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
	StopClipboardMonitoring();
	// Form tears down its owned controls as the native window closes, before the
	// stack-allocated Designer itself is destroyed.  Do not publish shutdown
	// state through raw child-control pointers after that ownership teardown.
	_propertyGrid = nullptr;
	_lblInfo = nullptr;
	ResetCodeFreshnessTracking();
	DiscardSessionRecoverySnapshot();
}

void Designer::InitAndShow()
{
	InitializeComponents();
	InitializeRecoverySession();
	this->Show();
	StartClipboardMonitoring();
	RefreshCommandAvailability();
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
	btnX += btnWidth + 15;

	const int historyButtonWidth = 58;
	_btnUndo = new Button(L"撤销", btnX, btnY,
		historyButtonWidth, btnHeight);
	_btnUndo->Round = 0.5f;
	_btnUndo->Enable = false;
	_btnUndo->AccessibleName = L"撤销";
	_btnUndo->AccessibleDescription = L"没有可撤销的操作。快捷键 Ctrl+Z。";
	_btnUndo->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnUndoClick();
	};
	this->AddControl(_btnUndo);
	btnX += historyButtonWidth + 6;

	_btnRedo = new Button(L"重做", btnX, btnY,
		historyButtonWidth, btnHeight);
	_btnRedo->Round = 0.5f;
	_btnRedo->Enable = false;
	_btnRedo->AccessibleName = L"重做";
	_btnRedo->AccessibleDescription = L"没有可重做的操作。快捷键 Ctrl+Y。";
	_btnRedo->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnRedoClick();
	};
	this->AddControl(_btnRedo);
	btnX += historyButtonWidth + 10;

	const int editButtonWidth = 64;
	_btnCopy = new Button(L"复制", btnX, btnY, editButtonWidth, btnHeight);
	_btnCopy->Round = 0.5f;
	_btnCopy->Enable = false;
	_btnCopy->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnCopyClick();
	};
	this->AddControl(_btnCopy);
	btnX += editButtonWidth + 6;

	_btnCut = new Button(L"剪切", btnX, btnY, editButtonWidth, btnHeight);
	_btnCut->Round = 0.5f;
	_btnCut->Enable = false;
	_btnCut->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnCutClick();
	};
	this->AddControl(_btnCut);
	btnX += editButtonWidth + 6;

	_btnPaste = new Button(L"粘贴", btnX, btnY, editButtonWidth, btnHeight);
	_btnPaste->Round = 0.5f;
	_btnPaste->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnPasteClick();
	};
	this->AddControl(_btnPaste);
	btnX += editButtonWidth + 6;

	_btnXaml = new Button(L"XAML", btnX, btnY, editButtonWidth, btnHeight);
	_btnXaml->Round = 0.5f;
	_btnXaml->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnXamlClick();
	};
	this->AddControl(_btnXaml);
	btnX += editButtonWidth + 6;

	_btnArrange = new Button(L"排列", btnX, btnY, editButtonWidth, btnHeight);
	_btnArrange->Round = 0.5f;
	_btnArrange->Enable = false;
	_btnArrange->OnMouseClick += [this](Control*, MouseEventArgs) {
		OnArrangeClick();
	};
	this->AddControl(_btnArrange);
	btnX += editButtonWidth + 15;
	
	_btnDelete = new Button(L"删除", btnX, btnY, btnWidth, btnHeight);
	_btnDelete->Round = 0.5f;
	_btnDelete->BackColor = Colors::IndianRed;
	_btnDelete->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnDeleteClick();
	};
	this->AddControl(_btnDelete);
	btnX += btnWidth + 30;
	
	_lblInfo = new Label(L"就绪", btnX, btnY + 5);
	_lblInfo->Size = {190, 25};
	_lblInfo->Font = new ::Font(L"Microsoft YaHei", 14.0f);
	this->AddControl(_lblInfo);

	_arrangeMenu = new ContextMenu();
	auto* duplicate = _arrangeMenu->AddItem(L"重复", ArrangeDuplicate);
	duplicate->Shortcut = L"Ctrl+D";
	auto* lockItem = _arrangeMenu->AddItem(L"锁定控件", CanvasToggleLock);
	lockItem->Shortcut = L"Ctrl+L";
	_arrangeMenu->AddSeparator();
	auto* align = _arrangeMenu->AddItem(L"对齐");
	align->AddSubItem(L"左对齐", ArrangeAlignLeft);
	align->AddSubItem(L"水平中心", ArrangeAlignHorizontalCenters);
	align->AddSubItem(L"右对齐", ArrangeAlignRight);
	align->AddSeparator();
	align->AddSubItem(L"顶端对齐", ArrangeAlignTop);
	align->AddSubItem(L"垂直中心", ArrangeAlignVerticalCenters);
	align->AddSubItem(L"底端对齐", ArrangeAlignBottom);
	auto* distribute = _arrangeMenu->AddItem(L"分布");
	distribute->AddSubItem(L"水平分布", ArrangeDistributeHorizontally);
	distribute->AddSubItem(L"垂直分布", ArrangeDistributeVertically);
	auto* sameSize = _arrangeMenu->AddItem(L"相同尺寸");
	sameSize->AddSubItem(L"宽度", ArrangeMakeSameWidth);
	sameSize->AddSubItem(L"高度", ArrangeMakeSameHeight);
	sameSize->AddSubItem(L"宽度和高度", ArrangeMakeSameSize);
	auto* layer = _arrangeMenu->AddItem(L"层级");
	auto* forward = layer->AddSubItem(L"上移一层", ArrangeBringForward);
	forward->Shortcut = L"Ctrl+]";
	auto* backward = layer->AddSubItem(L"下移一层", ArrangeSendBackward);
	backward->Shortcut = L"Ctrl+[";
	auto* front = layer->AddSubItem(L"置于顶层", ArrangeBringToFront);
	front->Shortcut = L"Ctrl+Shift+]";
	auto* back = layer->AddSubItem(L"置于底层", ArrangeSendToBack);
	back->Shortcut = L"Ctrl+Shift+[";
	_arrangeMenu->OnMenuCommand += [this](Control*, int commandId) {
		OnArrangeCommand(commandId);
	};
	this->AddControl(_arrangeMenu);

	_canvasMenu = new ContextMenu();
	auto* undoItem = _canvasMenu->AddItem(L"撤销", CanvasUndo);
	undoItem->Shortcut = L"Ctrl+Z";
	auto* redoItem = _canvasMenu->AddItem(L"重做", CanvasRedo);
	redoItem->Shortcut = L"Ctrl+Y";
	_canvasMenu->AddSeparator();
	auto* cutItem = _canvasMenu->AddItem(L"剪切", CanvasCut);
	cutItem->Shortcut = L"Ctrl+X";
	auto* copyItem = _canvasMenu->AddItem(L"复制", CanvasCopy);
	copyItem->Shortcut = L"Ctrl+C";
	auto* pasteItem = _canvasMenu->AddItem(L"粘贴", CanvasPaste);
	pasteItem->Shortcut = L"Ctrl+V";
	auto* pasteInPlaceItem = _canvasMenu->AddItem(
		L"原位粘贴", CanvasPasteInPlace);
	pasteInPlaceItem->Shortcut = L"Ctrl+Shift+V";
	_canvasMenu->AddItem(L"粘贴到此处", CanvasPasteHere);
	auto* duplicateItem = _canvasMenu->AddItem(L"重复", CanvasDuplicate);
	duplicateItem->Shortcut = L"Ctrl+D";
	_canvasMenu->AddItem(L"删除", CanvasDelete)->Shortcut = L"Delete";
	auto* contextLockItem = _canvasMenu->AddItem(
		L"锁定控件", CanvasToggleLock);
	contextLockItem->Shortcut = L"Ctrl+L";
	_canvasMenu->AddSeparator();
	auto* arrangeItem = _canvasMenu->AddItem(L"排列");
	auto* contextAlign = arrangeItem->AddSubItem(L"对齐");
	contextAlign->AddSubItem(L"左对齐", ArrangeAlignLeft);
	contextAlign->AddSubItem(L"水平中心", ArrangeAlignHorizontalCenters);
	contextAlign->AddSubItem(L"右对齐", ArrangeAlignRight);
	contextAlign->AddSeparator();
	contextAlign->AddSubItem(L"顶端对齐", ArrangeAlignTop);
	contextAlign->AddSubItem(L"垂直中心", ArrangeAlignVerticalCenters);
	contextAlign->AddSubItem(L"底端对齐", ArrangeAlignBottom);
	auto* contextDistribute = arrangeItem->AddSubItem(L"分布");
	contextDistribute->AddSubItem(
		L"水平分布", ArrangeDistributeHorizontally);
	contextDistribute->AddSubItem(
		L"垂直分布", ArrangeDistributeVertically);
	auto* contextSize = arrangeItem->AddSubItem(L"相同尺寸");
	contextSize->AddSubItem(L"宽度", ArrangeMakeSameWidth);
	contextSize->AddSubItem(L"高度", ArrangeMakeSameHeight);
	contextSize->AddSubItem(L"宽度和高度", ArrangeMakeSameSize);
	auto* contextLayer = arrangeItem->AddSubItem(L"层级");
	contextLayer->AddSubItem(L"上移一层", ArrangeBringForward)
		->Shortcut = L"Ctrl+]";
	contextLayer->AddSubItem(L"下移一层", ArrangeSendBackward)
		->Shortcut = L"Ctrl+[";
	contextLayer->AddSubItem(L"置于顶层", ArrangeBringToFront)
		->Shortcut = L"Ctrl+Shift+]";
	contextLayer->AddSubItem(L"置于底层", ArrangeSendToBack)
		->Shortcut = L"Ctrl+Shift+[";
	_canvasMenu->AddItem(L"全选当前容器", CanvasSelectAll)
		->Shortcut = L"Ctrl+A";
	_canvasMenu->AddSeparator();
	_canvasMenu->AddItem(L"编辑 XAML", CanvasEditXaml);
	_canvasMenu->AddSeparator();
	auto* viewItem = _canvasMenu->AddItem(L"视图");
	viewItem->AddSubItem(L"适合窗口", CanvasViewFit)
		->Shortcut = L"Ctrl+0";
	viewItem->AddSubItem(L"实际大小 (100%)", CanvasViewActualSize)
		->Shortcut = L"Ctrl+1";
	viewItem->AddSeparator();
	viewItem->AddSubItem(L"放大", CanvasViewZoomIn)
		->Shortcut = L"Ctrl++";
	viewItem->AddSubItem(L"缩小", CanvasViewZoomOut)
		->Shortcut = L"Ctrl+-";
	viewItem->AddSeparator();
	auto* gridViewItem = viewItem->AddSubItem(L"网格与吸附");
	gridViewItem->AddSubItem(L"显示网格", CanvasToggleGrid);
	gridViewItem->AddSubItem(L"吸附到网格", CanvasToggleSnapGrid);
	gridViewItem->AddSubItem(L"吸附到参考线", CanvasToggleSnapGuides);
	gridViewItem->AddSeparator();
	gridViewItem->AddSubItem(L"网格间距 5 DIP", CanvasGridSize5);
	gridViewItem->AddSubItem(L"网格间距 10 DIP", CanvasGridSize10);
	gridViewItem->AddSubItem(L"网格间距 20 DIP", CanvasGridSize20);
	viewItem->AddSeparator();
	viewItem->AddSubItem(L"Tab 顺序模式", CanvasToggleTabOrder);
	viewItem->AddSubItem(L"按布局自动排序", CanvasAutoTabOrder);
	_canvasMenu->OnMenuCommand += [this](Control*, int commandId) {
		OnCanvasMenuCommand(commandId);
	};
	this->AddControl(_canvasMenu);
	
	// 左侧工具箱 / 文档层级切换
	int toolBoxWidth = 150;
	SIZE contentSize = GetLogicalDesignerContentSize(this);
	int formHeight = contentSize.cy;
	const int sidebarTabsHeight = 30;
	_btnToolboxView = new Button(
		L"工具箱", 10, toolbarHeight + 10, 72, 26);
	_btnOutlineView = new Button(
		L"层级", 86, toolbarHeight + 10, 74, 26);
	for (auto* button : { _btnToolboxView, _btnOutlineView })
	{
		button->Raised = false;
		button->Round = 4.0f;
		button->BorderThickness = 1.0f;
		button->BackColor = D2D1::ColorF(0.88f, 0.90f, 0.94f, 1.0f);
		button->CheckedColor = D2D1::ColorF(0.20f, 0.46f, 0.90f, 0.30f);
		this->AddControl(button);
	}
	_btnToolboxView->AccessibleName = L"显示设计器工具箱";
	_btnOutlineView->AccessibleName = L"显示文档层级";
	_btnToolboxView->OnMouseClick += [this](Control*, MouseEventArgs) {
		SetSidebarView(false);
	};
	_btnOutlineView->OnMouseClick += [this](Control*, MouseEventArgs) {
		SetSidebarView(true);
	};

	_toolBox = new ToolBox(
		10, toolbarHeight + 10 + sidebarTabsHeight, toolBoxWidth,
		formHeight - toolbarHeight - 40 - sidebarTabsHeight,
		_controlDescriptors);
	_toolBox->OnControlSelected += [this](const DesignerControlDescriptor& descriptor) {
		OnToolBoxControlSelected(descriptor);
	};
	_toolBox->OnControlDragReady +=
		[this](const DesignerControlDescriptor& descriptor, POINT formPoint)
		{
			BeginToolBoxDrag(descriptor, formPoint);
		};
	this->AddControl(_toolBox);

	_outlineTree = new TreeView(
		10, toolbarHeight + 10 + sidebarTabsHeight, toolBoxWidth,
		formHeight - toolbarHeight - 40 - sidebarTabsHeight);
	_outlineTree->AccessibleName = L"文档层级";
	_outlineTree->AccessibleDescription =
		L"显示窗体与控件父子关系；可选择被遮挡或不可见控件，拖拽可重排或更换父容器。";
	_outlineTree->ItemHeight = 25.0f;
	_outlineTree->IndentWidth = 13.0f;
	_outlineTree->ItemHorizontalPadding = 4.0f;
	_outlineTree->TextLeftSpacing = 4.0f;
	_outlineTree->SelectionChanged += [this](Control*) {
		OnDocumentOutlineSelectionChanged();
	};
	_outlineTree->OnMouseDown += [this](Control*, MouseEventArgs args) {
		BeginDocumentOutlineDrag(args);
	};
	_outlineTree->OnKeyDown += [this](Control*, KeyEventArgs args) {
		if ((GetKeyState(VK_MENU) & 0x8000) != 0) return;
		(void)QueueOutlineShortcut(
			static_cast<WPARAM>(args.KeyCode()),
			(GetKeyState(VK_CONTROL) & 0x8000) != 0,
			(GetKeyState(VK_SHIFT) & 0x8000) != 0);
	};
	this->AddControl(_outlineTree);
	SetSidebarView(false);
	
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
	_canvas->OnContextMenuRequested +=
		[this](const DesignerCanvasContextMenuEventArgs& args) {
			OnCanvasContextMenuRequested(args);
		};
	_canvas->OnViewChanged +=
		[this](const DesignerCanvasViewChangedEventArgs& args) {
			OnCanvasViewChanged(args);
		};
	_canvas->OnTabOrderStateChanged +=
		[this](const DesignerCanvasTabOrderStateEventArgs& args) {
			OnCanvasTabOrderStateChanged(args);
		};
	_canvas->AccessibleDescription =
		L"设计画布。Ctrl+滚轮缩放；按住中键或空格拖动可平移；Ctrl+0 适合窗口；Ctrl+1 恢复 100%。";
	this->AddControl(_canvas);

	const int zoomStripY = formHeight - 26;
	const int zoomStripRight = canvasX + canvasWidth;
	_btnTabOrder = new Button(
		L"Tab 顺序", zoomStripRight - 352, zoomStripY, 84, 22);
	_btnTabOrder->Round = 0.35f;
	_btnTabOrder->AccessibleName = L"Tab 顺序模式";
	_btnTabOrder->AccessibleDescription =
		L"显示可接收键盘焦点控件的 TabIndex；进入后依次单击控件编号。";
	_btnTabOrder->OnMouseClick += [this](Control*, MouseEventArgs) {
		ToggleTabOrderMode();
	};
	this->AddControl(_btnTabOrder);

	_btnGridSettings = new Button(
		L"网格 10", zoomStripRight - 262, zoomStripY, 76, 22);
	_btnGridSettings->Round = 0.35f;
	_btnGridSettings->AccessibleName = L"网格与吸附设置";
	_btnGridSettings->OnMouseDown += [this](Control*, MouseEventArgs args) {
		if (args.Buttons != MouseButtons::Left) return;
		if (!_gridMenu || !_btnGridSettings) return;
		RefreshGridSettingsPresentation();
		_gridMenu->ShowAt(
			_btnGridSettings, 0, _btnGridSettings->Height, true);
	};
	this->AddControl(_btnGridSettings);

	_gridMenu = new ContextMenu();
	_gridMenu->AddItem(L"显示网格", CanvasToggleGrid);
	_gridMenu->AddItem(L"吸附到网格", CanvasToggleSnapGrid);
	_gridMenu->AddItem(L"吸附到参考线", CanvasToggleSnapGuides);
	_gridMenu->AddSeparator();
	_gridMenu->AddItem(L"网格间距 5 DIP", CanvasGridSize5);
	_gridMenu->AddItem(L"网格间距 10 DIP", CanvasGridSize10);
	_gridMenu->AddItem(L"网格间距 20 DIP", CanvasGridSize20);
	_gridMenu->OnMenuCommand += [this](Control*, int commandId) {
		OnCanvasMenuCommand(commandId);
	};
	this->AddControl(_gridMenu);
	RefreshGridSettingsPresentation();
	RefreshTabOrderPresentation();

	_btnFitView = new Button(
		L"适配", zoomStripRight - 52, zoomStripY, 52, 22);
	_btnFitView->Round = 0.35f;
	_btnFitView->AccessibleDescription = L"使设计窗体适合当前画布。快捷键 Ctrl+0。";
	_btnFitView->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (_canvas) _canvas->FitDesignSurfaceToViewport();
	};
	this->AddControl(_btnFitView);

	_btnZoomIn = new Button(
		L"+", zoomStripRight - 86, zoomStripY, 28, 22);
	_btnZoomIn->Round = 0.35f;
	_btnZoomIn->AccessibleName = L"放大设计画布";
	_btnZoomIn->AccessibleDescription = L"放大设计画布。快捷键 Ctrl+加号。";
	_btnZoomIn->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (_canvas) _canvas->ZoomIn();
	};
	this->AddControl(_btnZoomIn);

	_lblZoom = new Label(
		L"100%", zoomStripRight - 146, zoomStripY + 2);
	_lblZoom->Size = { 54, 20 };
	_lblZoom->AccessibleName = L"设计画布缩放比例";
	this->AddControl(_lblZoom);

	_btnZoomOut = new Button(
		L"-", zoomStripRight - 180, zoomStripY, 28, 22);
	_btnZoomOut->Round = 0.35f;
	_btnZoomOut->AccessibleName = L"缩小设计画布";
	_btnZoomOut->AccessibleDescription = L"缩小设计画布。快捷键 Ctrl+减号。";
	_btnZoomOut->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (_canvas) _canvas->ZoomOut();
	};
	this->AddControl(_btnZoomOut);

	RebuildDocumentOutline();
	_canvas->FitDesignSurfaceToViewport();
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
	auto doLayout = [this, toolbarHeight, toolBoxWidth, propertyGridWidth,
		sidebarTabsHeight]() {
		SIZE contentSize = GetLogicalDesignerContentSize(this);
		int w = contentSize.cx;
		int h = contentSize.cy;
		int usableH = h - toolbarHeight - 40;
		if (usableH < 50) usableH = 50;
		if (_toolBox)
		{
			_toolBox->Location = {
				10, toolbarHeight + 10 + sidebarTabsHeight };
			_toolBox->Size = {
				toolBoxWidth, (std::max)(50, usableH - sidebarTabsHeight) };
		}
		if (_btnToolboxView)
			_btnToolboxView->Location = { 10, toolbarHeight + 10 };
		if (_btnOutlineView)
			_btnOutlineView->Location = { 86, toolbarHeight + 10 };
		if (_outlineTree)
		{
			_outlineTree->Location = {
				10, toolbarHeight + 10 + sidebarTabsHeight };
			_outlineTree->Size = {
				toolBoxWidth, (std::max)(50, usableH - sidebarTabsHeight) };
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
			const bool refitCanvas = _canvas->IsFitToViewport();
			_canvas->Location = { canvasX, toolbarHeight + 10 };
			_canvas->Size = { canvasW, usableH };
			if (refitCanvas) _canvas->FitDesignSurfaceToViewport();
			else _canvas->InvalidateVisual();

			const int zoomY = h - 26;
			const int zoomRight = canvasX + canvasW;
			if (_btnFitView)
				_btnFitView->Location = { zoomRight - 52, zoomY };
			if (_btnZoomIn)
				_btnZoomIn->Location = { zoomRight - 86, zoomY };
			if (_lblZoom)
				_lblZoom->Location = { zoomRight - 146, zoomY + 2 };
			if (_btnZoomOut)
				_btnZoomOut->Location = { zoomRight - 180, zoomY };
			if (_btnGridSettings)
				_btnGridSettings->Location = { zoomRight - 262, zoomY };
			if (_btnTabOrder)
				_btnTabOrder->Location = { zoomRight - 352, zoomY };
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

void Designer::SetSidebarView(bool showDocumentOutline)
{
	if (!showDocumentOutline) CancelDocumentOutlineDrag();
	if (showDocumentOutline && _toolBoxPointerDown) CancelToolBoxDrag();
	_showDocumentOutline = showDocumentOutline;
	if (_toolBox) _toolBox->Visible = !showDocumentOutline;
	if (_outlineTree) _outlineTree->Visible = showDocumentOutline;
	if (_btnToolboxView)
	{
		_btnToolboxView->Checked = !showDocumentOutline;
		_btnToolboxView->AccessibleDescription = !showDocumentOutline
			? L"当前正在显示工具箱。" : L"切换到工具箱。";
		_btnToolboxView->InvalidateVisual();
	}
	if (_btnOutlineView)
	{
		_btnOutlineView->Checked = showDocumentOutline;
		_btnOutlineView->AccessibleDescription = showDocumentOutline
			? L"当前正在显示文档层级。" : L"切换到文档层级。";
		_btnOutlineView->InvalidateVisual();
	}
	if (showDocumentOutline)
	{
		// The view-toggle click is still walking the Form/child input stack.
		// Defer destructive TreeNode replacement through the same coalesced path
		// used by structural document changes; RebuildDocumentOutline performs
		// selection synchronization once the new tree is stable.
		ScheduleDocumentOutlineRebuild();
	}
	if (_canvas) this->SetSelectedControl(_canvas, true);
	if (_toolBox) _toolBox->InvalidateVisual();
	if (_outlineTree) _outlineTree->InvalidateVisual();
}

void Designer::RebuildDocumentOutline()
{
	if (!_outlineTree || !_outlineTree->Root || !_canvas) return;
	if (_outlinePointerDown) CancelDocumentOutlineDrag();
	else _outlineTree->ClearDropTarget();
	std::set<int> expandedIds;
	for (const auto& [stableId, node] : _outlineNodesByStableId)
		if (node && node->Expand) expandedIds.insert(stableId);
	const bool hadOutline = _outlineFormNode != nullptr;
	const bool formExpanded = !hadOutline || _outlineFormNode->Expand;
	const int oldScrollIndex = _outlineTree->ScrollIndex;

	_syncingDocumentOutline = true;
	_outlineTree->SelectedNode = nullptr;
	_outlineTree->Root->ClearChildren();
	_outlineNodesByStableId.clear();
	_outlineFormNode = new TreeNode(
		_canvas->GetDesignedFormName() + L"  (Form)");
	_outlineFormNode->Tag = (std::numeric_limits<ULONG64>::max)();
	_outlineTree->Root->AddChild(_outlineFormNode);

	struct OutlineRecord
	{
		std::shared_ptr<DesignerControl> Control;
		TreeNode* Node = nullptr;
		TreeNode* ParentNode = nullptr;
		int ChildOrder = 0;
	};
	std::vector<OutlineRecord> records;
	records.reserve(_canvas->GetAllControls().size());
	std::unordered_map<Control*, TreeNode*> nodesByRuntimeControl;
	for (const auto& control : _canvas->GetAllControls())
	{
		if (!control || !control->ControlInstance) continue;
		std::wstring typeName;
		if (!control->CustomType.Empty())
			typeName = control->CustomType.XamlName;
		if (typeName.empty())
		{
			const auto descriptor = std::find_if(
				_controlDescriptors.begin(), _controlDescriptors.end(),
				[&](const DesignerControlDescriptor& candidate)
				{
					return !candidate.IsCustom()
						&& candidate.Type == control->Type;
				});
			if (descriptor != _controlDescriptors.end())
				typeName = descriptor->Name;
		}
		if (typeName.empty())
			typeName = control->Type == UIClass::UI_TabPage
				? L"TabPage" : L"Control";

		std::wstring flags;
		if (!control->ControlInstance->Visible) flags += L"[隐藏]";
		if (control->IsLocked) flags += L"[锁定]";
		std::wstring text = (flags.empty() ? L"" : flags + L" ")
			+ control->Name + L"  (" + typeName + L")";
		auto* node = new TreeNode(std::move(text));
		node->Tag = static_cast<ULONG64>(control->StableId);
		_outlineNodesByStableId[control->StableId] = node;
		nodesByRuntimeControl[control->ControlInstance] = node;
		records.push_back(OutlineRecord{ control, node });
	}

	for (auto& record : records)
	{
		auto* parentControl = record.Control->DesignerParent;
		if (!parentControl && record.Control->ControlInstance)
			parentControl = record.Control->ControlInstance->Parent;
		auto* runtimeParent = record.Control->ControlInstance
			? record.Control->ControlInstance->Parent : nullptr;
		TreeNode* parentNode = nullptr;
		while (parentControl && !parentNode)
		{
			const auto found = nodesByRuntimeControl.find(parentControl);
			if (found != nodesByRuntimeControl.end()) parentNode = found->second;
			else parentControl = parentControl->Parent;
		}
		record.ParentNode = !parentNode || parentNode == record.Node
			? _outlineFormNode : parentNode;
		record.ChildOrder = runtimeParent
			? runtimeParent->IndexOfControl(record.Control->ControlInstance)
			: 0;
		if (record.ChildOrder < 0) record.ChildOrder = 0;
	}
	std::unordered_map<TreeNode*, std::vector<OutlineRecord*>> childrenByParent;
	for (auto& record : records)
		childrenByParent[record.ParentNode].push_back(&record);
	for (auto& [parentNode, children] : childrenByParent)
	{
		std::stable_sort(children.begin(), children.end(),
			[](const OutlineRecord* left, const OutlineRecord* right)
			{
				if (left->ChildOrder != right->ChildOrder)
					return left->ChildOrder < right->ChildOrder;
				return left->Control->StableId < right->Control->StableId;
			});
		for (auto* child : children)
			if (!parentNode || !child || !parentNode->AddChild(child->Node))
				_outlineFormNode->AddChild(child->Node);
	}

	_outlineFormNode->SetExpanded(formExpanded, false);
	for (const auto& record : records)
	{
		if (!record.Node->Children.empty())
			record.Node->SetExpanded(
				!hadOutline || expandedIds.contains(record.Control->StableId),
				false);
	}
	_outlineTree->ScrollIndex = (std::max)(0, oldScrollIndex);
	_syncingDocumentOutline = false;
	SyncDocumentOutlineSelection();
	_outlineTree->InvalidateVisual();
}

void Designer::ScheduleDocumentOutlineRebuild()
{
	if (!_outlineTree || !_canvas || !_showDocumentOutline) return;
	if (_documentOutlineRebuildPending) return;

	const HWND expectedHandle = Handle;
	if (!expectedHandle || !cui::HasUIThreadDispatcher())
	{
		RebuildDocumentOutline();
		return;
	}

	_documentOutlineRebuildPending = true;
	Designer* const expectedDesigner = this;
	if (cui::PostToUIThread(
		[expectedHandle, expectedDesigner]()
		{
			const auto found = Application::Forms.find(expectedHandle);
			if (found == Application::Forms.end()
				|| found->second != expectedDesigner)
				return;
			expectedDesigner->_documentOutlineRebuildPending = false;
			if (expectedDesigner->_showDocumentOutline)
				expectedDesigner->RebuildDocumentOutline();
		}))
		return;

	_documentOutlineRebuildPending = false;
	RebuildDocumentOutline();
}

void Designer::SyncDocumentOutlineSelection()
{
	if (_syncingDocumentOutline || !_outlineTree || !_canvas) return;
	TreeNode* selectedNode = _outlineFormNode;
	if (const auto selected = _canvas->GetSelectedControl())
	{
		const auto found = _outlineNodesByStableId.find(selected->StableId);
		if (found != _outlineNodesByStableId.end()) selectedNode = found->second;
	}
	_outlineTree->SelectedNode = selectedNode;

	std::function<bool(TreeNode*)> expandPath =
		[&](TreeNode* current) -> bool
		{
			if (!current) return false;
			if (current == selectedNode) return true;
			for (auto* child : current->Children)
			{
				if (!expandPath(child)) continue;
				current->SetExpanded(true, false);
				return true;
			}
			return false;
		};
	(void)expandPath(_outlineTree->Root);

	int selectedIndex = -1;
	int visibleIndex = 0;
	std::function<void(const std::vector<TreeNode*>&)> findVisibleIndex =
		[&](const std::vector<TreeNode*>& nodes)
		{
			for (auto* node : nodes)
			{
				if (selectedIndex >= 0 || !node) return;
				if (node == selectedNode)
				{
					selectedIndex = visibleIndex;
					return;
				}
				++visibleIndex;
				if (node->Expand) findVisibleIndex(node->Children);
			}
		};
	findVisibleIndex(_outlineTree->Root->Children);
	if (selectedIndex >= 0)
	{
		const int pageSize = (std::max)(1, static_cast<int>(
			static_cast<float>(_outlineTree->Height)
			/ (std::max)(1.0f, _outlineTree->ItemHeight)));
		if (selectedIndex < _outlineTree->ScrollIndex)
			_outlineTree->ScrollIndex = selectedIndex;
		else if (selectedIndex >= _outlineTree->ScrollIndex + pageSize)
			_outlineTree->ScrollIndex = selectedIndex - pageSize + 1;
	}
	_outlineTree->InvalidateVisual();
}

void Designer::OnDocumentOutlineSelectionChanged()
{
	if (_syncingDocumentOutline || !_outlineTree || !_canvas
		|| !_outlineTree->SelectedNode) return;
	_syncingDocumentOutline = true;
	const auto tag = _outlineTree->SelectedNode->Tag;
	if (tag == (std::numeric_limits<ULONG64>::max)())
	{
		_canvas->RestoreSelectionByNames({}, {}, true);
	}
	else
	{
		const int stableId = static_cast<int>(tag);
		const auto found = std::find_if(
			_canvas->GetAllControls().begin(),
			_canvas->GetAllControls().end(),
			[&](const std::shared_ptr<DesignerControl>& control)
			{
				return control && control->StableId == stableId;
			});
		if (found != _canvas->GetAllControls().end() && *found)
		{
			(void)_canvas->RevealControlInDesigner((*found)->ControlInstance);
			_canvas->RestoreSelectionByNames(
				{ (*found)->Name }, (*found)->Name, true);
		}
	}
	_syncingDocumentOutline = false;
	this->SetSelectedControl(_showDocumentOutline
		? static_cast<Control*>(_outlineTree)
		: static_cast<Control*>(_canvas), true);
}

void Designer::BeginDocumentOutlineDrag(const MouseEventArgs& args)
{
	if (args.Buttons != MouseButtons::Left || !_outlineTree || !_canvas
		|| !_showDocumentOutline || args.X >= _outlineTree->Width - 8)
		return;
	auto* node = _outlineTree->HitTestNode(
		static_cast<float>(args.X), static_cast<float>(args.Y));
	if (!node || node->Tag == (std::numeric_limits<ULONG64>::max)()) return;
	const int stableId = static_cast<int>(node->Tag);
	const auto found = std::find_if(
		_canvas->GetAllControls().begin(), _canvas->GetAllControls().end(),
		[stableId](const std::shared_ptr<DesignerControl>& candidate)
		{
			return candidate && candidate->ControlInstance
				&& candidate->StableId == stableId;
		});
	if (found == _canvas->GetAllControls().end()) return;
	_outlinePointerDown = true;
	_outlineDragging = false;
	_outlineDragStart = { args.X, args.Y };
	_outlineDragSourceStableId = stableId;
	_outlineDropTargetStableId.reset();
	_outlineDropPosition = TreeViewDropPosition::None;
	if (Handle) (void)::SetCapture(Handle);
}

void Designer::UpdateDocumentOutlineDrag(int localX, int localY)
{
	if (!_outlinePointerDown || !_outlineTree || !_canvas) return;
	const int treeX = localX - _outlineTree->Location.x;
	const int treeY = localY - _outlineTree->Location.y;
	if (!_outlineDragging)
	{
		const int dx = treeX - _outlineDragStart.x;
		const int dy = treeY - _outlineDragStart.y;
		if (dx * dx + dy * dy < 16) return;
		_outlineDragging = true;
		if (_lblInfo)
		{
			_lblInfo->Text = L"拖到容器中部可更换父级，拖到行边缘可调整顺序";
			_lblInfo->InvalidateVisual();
		}
	}

	const int pageSize = (std::max)(1, static_cast<int>(
		static_cast<float>(_outlineTree->Height)
		/ (std::max)(1.0f, _outlineTree->ItemHeight)));
	const int maxScroll = (std::max)(0,
		_outlineTree->MaxRenderItems - pageSize);
	if (treeY < 10 && _outlineTree->ScrollIndex > 0)
	{
		--_outlineTree->ScrollIndex;
		_outlineTree->InvalidateVisual();
	}
	else if (treeY > _outlineTree->Height - 10
		&& _outlineTree->ScrollIndex < maxScroll)
	{
		++_outlineTree->ScrollIndex;
		_outlineTree->InvalidateVisual();
	}

	float rowPosition = 0.5f;
	auto* targetNode = _outlineTree->HitTestNode(
		static_cast<float>(treeX), static_cast<float>(treeY), &rowPosition);
	if (!targetNode)
	{
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::None;
		_outlineTree->ClearDropTarget();
		return;
	}
	if (targetNode->Tag == (std::numeric_limits<ULONG64>::max)())
	{
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::Inside;
		_outlineTree->SetDropTarget(targetNode, _outlineDropPosition);
		return;
	}

	const int targetStableId = static_cast<int>(targetNode->Tag);
	if (targetStableId == _outlineDragSourceStableId)
	{
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::None;
		_outlineTree->ClearDropTarget();
		return;
	}
	auto findControl = [&](int stableId) -> std::shared_ptr<DesignerControl>
	{
		const auto found = std::find_if(
			_canvas->GetAllControls().begin(), _canvas->GetAllControls().end(),
			[stableId](const std::shared_ptr<DesignerControl>& candidate)
			{
				return candidate && candidate->ControlInstance
					&& candidate->StableId == stableId;
			});
		return found == _canvas->GetAllControls().end() ? nullptr : *found;
	};
	const auto source = findControl(_outlineDragSourceStableId);
	const auto target = findControl(targetStableId);
	if (!source || !target)
	{
		_outlineTree->ClearDropTarget();
		return;
	}
	for (Control* ancestor = target->ControlInstance; ancestor;
		ancestor = ancestor->Parent)
	{
		if (ancestor != source->ControlInstance) continue;
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::None;
		_outlineTree->ClearDropTarget();
		return;
	}

	auto canContain = [](UIClass type)
	{
		switch (type)
		{
		case UIClass::UI_Panel:
		case UIClass::UI_GroupBox:
		case UIClass::UI_Expander:
		case UIClass::UI_ScrollView:
		case UIClass::UI_StackPanel:
		case UIClass::UI_GridPanel:
		case UIClass::UI_DockPanel:
		case UIClass::UI_WrapPanel:
		case UIClass::UI_RelativePanel:
		case UIClass::UI_TabControl:
		case UIClass::UI_TabPage:
		case UIClass::UI_ToolBar:
			return true;
		default:
			return false;
		}
	};
	TreeViewDropPosition dropPosition = TreeViewDropPosition::Inside;
	if (target->Type == UIClass::UI_TabPage)
		dropPosition = TreeViewDropPosition::Inside;
	else if (rowPosition < 0.25f)
		dropPosition = TreeViewDropPosition::Before;
	else if (rowPosition > 0.75f)
		dropPosition = TreeViewDropPosition::After;
	else if (!canContain(target->Type))
		dropPosition = rowPosition < 0.5f
			? TreeViewDropPosition::Before : TreeViewDropPosition::After;

	_outlineDropTargetStableId = targetStableId;
	_outlineDropPosition = dropPosition;
	_outlineTree->SetDropTarget(targetNode, dropPosition);
}

void Designer::EndDocumentOutlineDrag()
{
	if (!_outlinePointerDown) return;
	const bool shouldMove = _outlineDragging && _canvas
		&& _outlineDropPosition != TreeViewDropPosition::None;
	const int sourceStableId = _outlineDragSourceStableId;
	const auto targetStableId = _outlineDropTargetStableId;
	const auto dropPosition = _outlineDropPosition;
	CancelDocumentOutlineDrag();
	if (!shouldMove) return;
	DesignerHierarchyDropPosition hierarchyPosition =
		DesignerHierarchyDropPosition::Inside;
	if (dropPosition == TreeViewDropPosition::Before)
		hierarchyPosition = DesignerHierarchyDropPosition::Before;
	else if (dropPosition == TreeViewDropPosition::After)
		hierarchyPosition = DesignerHierarchyDropPosition::After;
	(void)_canvas->MoveControlInHierarchy(
		sourceStableId, targetStableId, hierarchyPosition);
	this->SetSelectedControl(_showDocumentOutline
		? static_cast<Control*>(_outlineTree)
		: static_cast<Control*>(_canvas), true);
}

void Designer::CancelDocumentOutlineDrag(bool releaseCapture)
{
	_outlinePointerDown = false;
	_outlineDragging = false;
	_outlineDragSourceStableId = 0;
	_outlineDropTargetStableId.reset();
	_outlineDropPosition = TreeViewDropPosition::None;
	if (_outlineTree) _outlineTree->ClearDropTarget();
	if (releaseCapture && Handle && ::GetCapture() == Handle)
		(void)::ReleaseCapture();
}

void Designer::BeginToolBoxDrag(
	const DesignerControlDescriptor& descriptor,
	POINT formPoint)
{
	if (!descriptor.IsValid() || !_canvas || _showDocumentOutline) return;
	if (_outlinePointerDown) CancelDocumentOutlineDrag();
	CancelToolBoxDrag(false);
	// A new toolbox gesture supersedes a prior click-to-place tool.  A plain
	// click will arm this descriptor again from OnControlSelected on mouse-up.
	_canvas->SetControlToAdd(DesignerControlDescriptor{});
	_toolBoxPointerDown = true;
	_toolBoxDragging = false;
	_toolBoxDragStart = formPoint;
	_toolBoxDropCanvasPoint = { 0, 0 };
	_toolBoxDropAccepted = false;
	_toolBoxDragDescriptor = descriptor;
}

void Designer::UpdateToolBoxDrag(int localX, int localY)
{
	if (!_toolBoxPointerDown || !_toolBoxDragDescriptor || !_canvas) return;
	if (!_toolBoxDragging)
	{
		const int dx = localX - _toolBoxDragStart.x;
		const int dy = localY - _toolBoxDragStart.y;
		const int thresholdX = (std::max)(2, ::GetSystemMetrics(SM_CXDRAG) / 2);
		const int thresholdY = (std::max)(2, ::GetSystemMetrics(SM_CYDRAG) / 2);
		if (std::abs(dx) < thresholdX && std::abs(dy) < thresholdY) return;
		_toolBoxDragging = true;
	}

	const auto canvasOrigin = _canvas->GetAbsoluteLocationDip();
	const POINT viewPoint{
		localX - static_cast<LONG>(std::lround(canvasOrigin.x)),
		localY - static_cast<LONG>(std::lround(canvasOrigin.y)) };
	_toolBoxDropCanvasPoint = _canvas->ViewToCanvasPoint(viewPoint);
	std::wstring target;
	_toolBoxDropAccepted = _canvas->UpdateControlDropPreview(
		*_toolBoxDragDescriptor, _toolBoxDropCanvasPoint, &target);
	if (_lblInfo)
	{
		_lblInfo->Text = _toolBoxDropAccepted
			? L"释放以将 " + _toolBoxDragDescriptor->DisplayName
				+ L" 添加到 " + target + L"。"
			: L"将控件拖到窗体设计区域内。";
		_lblInfo->AccessibleDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
	}
	(void)::SetCursor(::LoadCursorW(nullptr,
		_toolBoxDropAccepted ? IDC_CROSS : IDC_NO));
}

void Designer::EndToolBoxDrag(int localX, int localY)
{
	if (!_toolBoxPointerDown) return;
	UpdateToolBoxDrag(localX, localY);
	const bool accepted = _toolBoxDragging && _toolBoxDropAccepted
		&& _toolBoxDragDescriptor.has_value();
	const auto descriptor = _toolBoxDragDescriptor;
	const auto canvasPoint = _toolBoxDropCanvasPoint;
	CancelToolBoxDrag();
	if (!accepted || !descriptor)
	{
		if (_lblInfo)
		{
			_lblInfo->Text = L"已取消工具箱拖放。";
			_lblInfo->AccessibleDescription = _lblInfo->Text;
			_lblInfo->InvalidateVisual();
		}
		return;
	}
	this->SetSelectedControl(_canvas, true);
	(void)_canvas->AddControlToCanvas(*descriptor, canvasPoint);
}

void Designer::CancelToolBoxDrag(bool releaseCapture)
{
	const bool consumedItemMouseUp = _toolBoxDragging;
	_toolBoxPointerDown = false;
	_toolBoxDragging = false;
	_toolBoxDropAccepted = false;
	_toolBoxDragDescriptor.reset();
	if (consumedItemMouseUp && _toolBox)
		_toolBox->CancelActiveItemPress();
	if (_canvas) _canvas->ClearControlDropPreview();
	if (releaseCapture && Handle && ::GetCapture() == Handle)
		(void)::ReleaseCapture();
}

void Designer::OnToolBoxControlSelected(
	const DesignerControlDescriptor& descriptor)
{
	_canvas->SetControlToAdd(descriptor);
	_lblInfo->Text = L"请在画布上点击以添加控件，或直接从工具箱拖到目标容器。";
}

void Designer::OnCanvasControlSelected(std::shared_ptr<DesignerControl> control)
{
	SyncDocumentOutlineSelection();
	if (_btnCopy) _btnCopy->Enable = control != nullptr;
	if (_btnCut) _btnCut->Enable = control != nullptr;
	if (_btnArrange) _btnArrange->Enable = control != nullptr;
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
	RefreshLockPresentation();
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
	RefreshCommandAvailability();
	UpdateCanvasOperationStatus(
		args.Operation, args.Label, args.Message, args.Result);
}

void Designer::OnCanvasDocumentStateChanged(
	const DesignerCanvasDocumentStateEventArgs& args)
{
	// Document transactions publish this event before every caller has unwound.
	// Rebuilding the visible tree here would delete nodes that may still be on
	// the current input/accessibility stack. Coalesce the rebuild onto the UI
	// dispatcher so toolbar, context-menu, and keyboard structural commands all
	// observe one stable outline refresh after the transaction completes.
	ScheduleDocumentOutlineRebuild();
	RestoreCodeBehindAssociation();
	UpdateCodeFreshnessForDocumentState();
	UpdateDocumentPresentation();
	if (args.IsDirty) ScheduleRecoverySnapshot();
	else DiscardSessionRecoverySnapshot();
}

void Designer::OnCanvasContextMenuRequested(
	const DesignerCanvasContextMenuEventArgs& args)
{
	if (!_canvasMenu || !_canvas) return;
	_canvasContextPastePoint = _canvas->ViewToCanvasPoint(
		args.CanvasPosition);
	_hasCanvasContextPastePoint = true;
	RefreshCommandAvailability();
	const bool hasSelection = args.HasSelection
		&& !_canvas->GetSelectedControls().empty();
	for (const int id : {
		CanvasCut, CanvasCopy, CanvasDuplicate, CanvasDelete,
		CanvasToggleLock })
	{
		if (auto* item = _canvasMenu->FindItemById(id))
			item->Enable = hasSelection;
	}
	if (auto* arrange = _canvasMenu->FindItemByText(L"排列", false))
		arrange->Enable = hasSelection;
	if (auto* selectAll = _canvasMenu->FindItemById(CanvasSelectAll))
		selectAll->Enable = !_canvas->GetAllControls().empty();
	const bool transactionActive = _canvas->HasActiveDocumentTransaction();
	if (auto* xaml = _canvasMenu->FindItemById(CanvasEditXaml))
		xaml->Enable = !transactionActive;
	RefreshGridSettingsPresentation();
	RefreshTabOrderPresentation();
	RefreshLockPresentation();
	_canvasMenu->ShowAt(
		_canvas, args.CanvasPosition.x, args.CanvasPosition.y);
}

void Designer::OnCanvasViewChanged(
	const DesignerCanvasViewChangedEventArgs& args)
{
	const int percent = static_cast<int>(std::lround(args.Zoom * 100.0f));
	if (_lblZoom)
	{
		_lblZoom->Text = std::to_wstring(percent) + L"%";
		_lblZoom->AccessibleDescription = args.FitToViewport
			? L"当前缩放比例，已适合窗口。"
			: L"当前缩放比例。";
	}
	if (_lblInfo)
	{
		_lblInfo->Text = L"缩放: " + std::to_wstring(percent) + L"%"
			+ (args.FitToViewport ? L"（适合窗口）" : L"");
	}
}

void Designer::OnCanvasTabOrderStateChanged(
	const DesignerCanvasTabOrderStateEventArgs& args)
{
	RefreshTabOrderPresentation();
	if (!_lblInfo) return;
	if (args.Active)
	{
		_lblInfo->Text = L"Tab 顺序：下一项 "
			+ std::to_wstring(args.NextIndex) + L"（"
			+ std::to_wstring(args.CandidateCount) + L" 项）";
		_lblInfo->AccessibleDescription = L"Tab 顺序模式：下一项 "
			+ std::to_wstring(args.NextIndex) + L"；可编排 "
			+ std::to_wstring(args.CandidateCount)
			+ L" 个控件，Escape 退出。";
	}
	else
	{
		_lblInfo->Text = L"已退出 Tab 顺序模式。";
		_lblInfo->AccessibleDescription = _lblInfo->Text;
	}
	_lblInfo->InvalidateVisual();
}

void Designer::RefreshTabOrderPresentation()
{
	if (!_canvas) return;
	const bool active = _canvas->IsTabOrderMode();
	const int nextIndex = _canvas->GetNextTabOrderIndex();
	const auto candidateCount = _canvas->GetTabOrderCandidateCount();
	if (_btnTabOrder)
	{
		_btnTabOrder->Checked = active;
		_btnTabOrder->Text = active
			? L"Tab " + std::to_wstring(nextIndex)
			: L"Tab 顺序";
		_btnTabOrder->AccessibleDescription = active
			? L"Tab 顺序模式已开启；下一项为 "
				+ std::to_wstring(nextIndex)
				+ L"。单击控件编号，Escape 退出。"
			: L"显示可接收键盘焦点控件的 TabIndex；进入后依次单击控件编号。";
		_btnTabOrder->InvalidateVisual();
	}
	if (_canvasMenu)
	{
		if (auto* item = _canvasMenu->FindItemById(CanvasToggleTabOrder))
			item->Checked = active;
		if (auto* item = _canvasMenu->FindItemById(CanvasAutoTabOrder))
			item->Enable = candidateCount > 0
				&& !_canvas->HasActiveDocumentTransaction();
		_canvasMenu->InvalidateVisual();
	}
}

void Designer::ToggleTabOrderMode()
{
	if (!_canvas) return;
	const bool active = !_canvas->IsTabOrderMode();
	if (!_canvas->SetTabOrderMode(active, 0) && _lblInfo)
	{
		_lblInfo->Text = L"当前文档事务尚未结束，不能进入 Tab 顺序模式。";
		_lblInfo->InvalidateVisual();
	}
}

void Designer::RefreshGridSettingsPresentation()
{
	if (!_canvas) return;
	auto refreshMenu = [this](ContextMenu* menu)
	{
		if (!menu) return;
		if (auto* item = menu->FindItemById(CanvasToggleGrid))
			item->Checked = _canvas->IsGridVisible();
		if (auto* item = menu->FindItemById(CanvasToggleSnapGrid))
			item->Checked = _canvas->IsSnapToGridEnabled();
		if (auto* item = menu->FindItemById(CanvasToggleSnapGuides))
			item->Checked = _canvas->IsSnapToGuidesEnabled();
		for (const auto [id, size] : {
			std::pair{ CanvasGridSize5, 5 },
			std::pair{ CanvasGridSize10, 10 },
			std::pair{ CanvasGridSize20, 20 } })
		{
			if (auto* item = menu->FindItemById(id))
				item->Checked = _canvas->GetGridSize() == size;
		}
		menu->InvalidateVisual();
	};
	refreshMenu(_gridMenu);
	refreshMenu(_canvasMenu);
	if (_btnGridSettings)
	{
		_btnGridSettings->Text = L"网格 "
			+ std::to_wstring(_canvas->GetGridSize());
		_btnGridSettings->Checked = _canvas->IsGridVisible();
		_btnGridSettings->AccessibleDescription =
			(_canvas->IsGridVisible() ? L"显示网格；" : L"隐藏网格；")
			+ std::wstring(_canvas->IsSnapToGridEnabled()
				? L"启用网格吸附；" : L"禁用网格吸附；")
			+ std::wstring(_canvas->IsSnapToGuidesEnabled()
				? L"启用参考线吸附。" : L"禁用参考线吸附。");
		_btnGridSettings->InvalidateVisual();
	}
}

void Designer::RefreshCommandAvailability()
{
	const bool transactionActive = _canvas
		&& _canvas->HasActiveDocumentTransaction();
	const bool clipboardHasText = _canvas
		&& _canvas->CanPasteControlsFromClipboard();
	const bool canPaste = clipboardHasText && !transactionActive;
	const bool canUndo = _canvas && !transactionActive
		&& _canvas->GetUndoCommandCount() > 0;
	const bool canRedo = _canvas && !transactionActive
		&& _canvas->GetRedoCommandCount() > 0;
	const auto undoLabel = canUndo
		? DescribeCanvasOperation(_canvas->GetUndoCommandLabel())
		: std::wstring{};
	const auto redoLabel = canRedo
		? DescribeCanvasOperation(_canvas->GetRedoCommandLabel())
		: std::wstring{};
	if (_btnUndo)
	{
		_btnUndo->Enable = canUndo;
		_btnUndo->AccessibleDescription = canUndo
			? L"撤销“" + undoLabel + L"”。快捷键 Ctrl+Z。"
			: L"没有可撤销的操作。快捷键 Ctrl+Z。";
		_btnUndo->InvalidateVisual();
	}
	if (_btnRedo)
	{
		_btnRedo->Enable = canRedo;
		_btnRedo->AccessibleDescription = canRedo
			? L"重做“" + redoLabel + L"”。快捷键 Ctrl+Y。"
			: L"没有可重做的操作。快捷键 Ctrl+Y。";
		_btnRedo->InvalidateVisual();
	}
	if (_btnPaste)
	{
		_btnPaste->Enable = canPaste;
		_btnPaste->AccessibleName = L"粘贴";
		_btnPaste->AccessibleDescription = transactionActive
			? L"画布事务进行中，暂时不能粘贴。快捷键 Ctrl+V。"
			: clipboardHasText
				? L"从剪贴板粘贴 CUI XAML；外部文本会在粘贴时验证。快捷键 Ctrl+V。"
				: L"剪贴板中没有可粘贴的文本。快捷键 Ctrl+V。";
		_btnPaste->InvalidateVisual();
	}
	if (_canvasMenu)
	{
		if (auto* undo = _canvasMenu->FindItemById(CanvasUndo))
		{
			undo->Enable = canUndo;
			undo->Text = canUndo ? L"撤销 " + undoLabel : L"撤销";
		}
		if (auto* redo = _canvasMenu->FindItemById(CanvasRedo))
		{
			redo->Enable = canRedo;
			redo->Text = canRedo ? L"重做 " + redoLabel : L"重做";
		}
		for (const int id : {
			CanvasPaste, CanvasPasteInPlace, CanvasPasteHere })
		{
			if (auto* paste = _canvasMenu->FindItemById(id))
				paste->Enable = canPaste
					&& (id != CanvasPasteHere
						|| _hasCanvasContextPastePoint);
		}
		_canvasMenu->InvalidateVisual();
	}
	RefreshLockPresentation();
}

void Designer::StartClipboardMonitoring()
{
	if (_clipboardListenerWindow || !Handle || !::IsWindow(Handle)) return;
	if (::AddClipboardFormatListener(Handle))
		_clipboardListenerWindow = Handle;
}

void Designer::StopClipboardMonitoring()
{
	if (Handle)
		(void)::KillTimer(Handle, ClipboardRefreshTimerId);
	_clipboardRefreshRetriesRemaining = 0;
	if (!_clipboardListenerWindow) return;
	(void)::RemoveClipboardFormatListener(_clipboardListenerWindow);
	_clipboardListenerWindow = nullptr;
}

void Designer::RefreshLockPresentation()
{
	if (!_canvas) return;
	const bool hasSelection = !_canvas->GetSelectedControls().empty();
	const bool allLocked = hasSelection
		&& _canvas->AreAllSelectedControlsLocked();
	auto refresh = [hasSelection, allLocked](ContextMenu* menu)
	{
		if (!menu) return;
		if (auto* item = menu->FindItemById(CanvasToggleLock))
		{
			item->Enable = hasSelection;
			item->Checked = allLocked;
			item->Text = allLocked ? L"解除锁定" : L"锁定控件";
		}
		menu->InvalidateVisual();
	};
	refresh(_arrangeMenu);
	refresh(_canvasMenu);
}

void Designer::ToggleSelectedControlsLocked()
{
	if (!_canvas || _canvas->GetSelectedControls().empty()) return;
	(void)_canvas->SetSelectedControlsLocked(
		!_canvas->AreAllSelectedControlsLocked());
	RefreshLockPresentation();
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
	if (message == WM_CLIPBOARDUPDATE)
	{
		_clipboardRefreshRetriesRemaining = ClipboardRefreshRetryCount;
		RefreshCommandAvailability();
		if (Handle)
			(void)::SetTimer(Handle, ClipboardRefreshTimerId,
				ClipboardRefreshDelayMilliseconds, nullptr);
		return true;
	}
	if (message == WM_DESTROY)
		StopClipboardMonitoring();
	// Form's native window procedure enters the virtual override with 0/0 and
	// recomputes pointer coordinates inside Form::ProcessMessage for normal
	// child routing.  Designer-level captured gestures run before that base call,
	// so derive the same logical content coordinates here as well.
	const bool pointerCoordinateMessage = message == WM_MOUSEMOVE
		|| message == WM_LBUTTONDOWN || message == WM_LBUTTONUP
		|| message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN
		|| message == WM_RBUTTONUP || message == WM_MBUTTONDOWN
		|| message == WM_MBUTTONUP || message == WM_MOUSEWHEEL;
	if (pointerCoordinateMessage && Handle)
	{
		POINT pointer{};
		if (::GetCursorPos(&pointer) && ::ScreenToClient(Handle, &pointer))
		{
			const float dpiScale = (std::max)(0.01f, GetDpiScale());
			localX = static_cast<int>(static_cast<float>(pointer.x) / dpiScale);
			localY = static_cast<int>(
				static_cast<float>(pointer.y - ClientTop()) / dpiScale);
		}
	}
	const bool keyDownMessage = message == WM_KEYDOWN
		|| message == WM_SYSKEYDOWN;
	const bool keyUpMessage = message == WM_KEYUP
		|| message == WM_SYSKEYUP;
	if (keyDownMessage || keyUpMessage)
	{
		const bool down = keyDownMessage;
		switch (wParam)
		{
		case VK_CONTROL:
		case VK_LCONTROL:
		case VK_RCONTROL:
			_designerControlKeyDown = down;
			break;
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			_designerShiftKeyDown = down;
			break;
		case VK_MENU:
		case VK_LMENU:
		case VK_RMENU:
			_designerAltKeyDown = down;
			break;
		default:
			break;
		}
	}
	const bool controlDown = _designerControlKeyDown
		|| (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	const bool shiftDown = _designerShiftKeyDown
		|| (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	const bool altDown = _designerAltKeyDown
		|| (GetKeyState(VK_MENU) & 0x8000) != 0;
	if (_toolBoxPointerDown)
	{
		if (message == WM_MOUSEMOVE)
		{
			UpdateToolBoxDrag(localX, localY);
			if (_toolBoxDragging) return true;
		}
		if (message == WM_LBUTTONUP)
		{
			if (_toolBoxDragging)
			{
				EndToolBoxDrag(localX, localY);
				return true;
			}
			// Let Form deliver a normal mouse-up/click to the toolbox item.
			// Its click handler preserves the existing click-then-place workflow.
			CancelToolBoxDrag(false);
		}
		if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
		{
			const bool wasDragging = _toolBoxDragging;
			CancelToolBoxDrag();
			if (wasDragging && _lblInfo)
			{
				_lblInfo->Text = L"已取消工具箱拖放。";
				_lblInfo->AccessibleDescription = _lblInfo->Text;
				_lblInfo->InvalidateVisual();
			}
			return true;
		}
	}
	if (_outlinePointerDown)
	{
		if (message == WM_MOUSEMOVE)
		{
			UpdateDocumentOutlineDrag(localX, localY);
			return true;
		}
		if (message == WM_LBUTTONUP)
		{
			UpdateDocumentOutlineDrag(localX, localY);
			EndDocumentOutlineDrag();
			return true;
		}
		if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
		{
			CancelDocumentOutlineDrag();
			if (_lblInfo)
			{
				_lblInfo->Text = L"已取消层级拖拽";
				_lblInfo->InvalidateVisual();
			}
			return true;
		}
	}
	const bool outlineShortcutTarget = _showDocumentOutline
		&& (IsControlWithin(this->Selected, _outlineTree)
			|| IsControlWithin(this->Selected, _canvas));
	if (message == WM_CHAR)
	{
		const auto character = static_cast<wchar_t>(wParam);
		if (_suppressedOutlineShortcutCharacter == character)
		{
			_suppressedOutlineShortcutCharacter = L'\0';
			return true;
		}
		_suppressedOutlineShortcutCharacter = L'\0';
		const WPARAM shortcutKey =
			OutlineShortcutKeyFromControlCharacter(character);
		if (outlineShortcutTarget && shortcutKey != 0
			&& !altDown
			&& QueueOutlineShortcut(shortcutKey, true, shiftDown))
		{
			return true;
		}
	}
	if (message == WM_KEYDOWN
		&& outlineShortcutTarget
		&& !altDown
		&& QueueOutlineShortcut(wParam, controlDown, shiftDown))
	{
		if (controlDown)
			_suppressedOutlineShortcutCharacter =
				OutlineShortcutControlCharacter(wParam);
		return true;
	}
	if (message == WM_KEYDOWN
		&& controlDown && !altDown)
	{
		if (wParam == 'S')
		{
			OnSaveClick();
			return true;
		}
		if (wParam == 'N')
		{
			OnNewClick();
			return true;
		}
		if (wParam == 'O')
		{
			OnOpenClick();
			return true;
		}
	}
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
	if (message == WM_TIMER && wParam == ClipboardRefreshTimerId)
	{
		(void)::KillTimer(Handle, ClipboardRefreshTimerId);
		if (_clipboardRefreshRetriesRemaining > 0)
		{
			--_clipboardRefreshRetriesRemaining;
			RefreshCommandAvailability();
			if (_clipboardRefreshRetriesRemaining > 0)
				(void)::SetTimer(Handle, ClipboardRefreshTimerId,
					ClipboardRefreshDelayMilliseconds, nullptr);
		}
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
	if (interactionCanceled)
	{
		_designerControlKeyDown = false;
		_designerShiftKeyDown = false;
		_designerAltKeyDown = false;
		_suppressedOutlineShortcutCharacter = L'\0';
	}
	if (interactionCanceled && _outlinePointerDown)
		CancelDocumentOutlineDrag(false);
	if (interactionCanceled && _toolBoxPointerDown)
		CancelToolBoxDrag(false);
	if (interactionCanceled && _canvas)
	{
		(void)_canvas->CancelActivePointerInteraction(
			captureLost
				? L"画布失去鼠标捕获，修改已回滚。"
				: L"窗口交互被中断，画布修改已回滚。");
	}
	return Form::ProcessMessage(message, wParam, lParam, localX, localY);
}

bool Designer::QueueOutlineShortcut(
	WPARAM key,
	bool controlDown,
	bool shiftDown)
{
	if (!IsOutlineShortcutKey(key, controlDown)) return false;
	// Clipboard access and command routing must stay in the originating key
	// dispatch. Structural document notifications already coalesce the outline
	// rebuild onto the UI dispatcher, so deferring the command itself only makes
	// failures invisible and can lose Ctrl+V while the tree owns keyboard focus.
	return ExecuteOutlineShortcut(key, controlDown, shiftDown);
}

bool Designer::ExecuteOutlineShortcut(
	WPARAM key,
	bool controlDown,
	bool shiftDown)
{
	if (!_canvas) return false;
	if (!controlDown)
	{
		if (key != VK_DELETE) return false;
		OnDeleteClick();
		return true;
	}

	switch (key)
	{
	case 'C':
		OnCopyClick();
		return true;
	case 'X':
		OnCutClick();
		return true;
	case 'V':
		if (shiftDown)
			(void)_canvas->PasteControlsFromClipboardInPlace();
		else OnPasteClick();
		return true;
	case 'D':
		(void)_canvas->DuplicateSelectedControls();
		return true;
	case 'L':
		ToggleSelectedControlsLocked();
		return true;
	case 'Z':
		if (shiftDown) OnRedoClick();
		else OnUndoClick();
		return true;
	case 'Y':
		OnRedoClick();
		return true;
	case 'A':
		(void)_canvas->SelectAllInCurrentContainer(true);
		return true;
	default:
		return false;
	}
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
	RefreshCommandAvailability();
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
			+ L"。首次“导出代码”后，再次激活会更新并打开用户源文件。";
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

void Designer::OnUndoClick()
{
	if (!_canvas) return;
	if (_propertyGrid) _propertyGrid->CommitPendingEdits();
	(void)_canvas->UndoCommand();
}

void Designer::OnRedoClick()
{
	if (!_canvas) return;
	if (_propertyGrid) _propertyGrid->CommitPendingEdits();
	(void)_canvas->RedoCommand();
}

void Designer::OnCopyClick()
{
	(void)_canvas->CopySelectedControls();
}

void Designer::OnCutClick()
{
	(void)_canvas->CutSelectedControls();
}

void Designer::OnPasteClick()
{
	(void)_canvas->PasteControlsFromClipboard();
}

void Designer::OnXamlClick()
{
	if (!_canvas) return;
	if (_canvas->IsTabOrderMode())
		(void)_canvas->SetTabOrderMode(false);
	std::wstring xaml;
	std::wstring error;
	if (!_canvas->BuildXamlDocumentText(xaml, &error))
	{
		ShowModalMessage(this, L"无法打开 XAML 编辑器",
			error.empty() ? L"无法生成当前设计文档的 XAML。" : error);
		return;
	}
	auto begin = _canvas->BeginDocumentEditTransaction(L"EditXaml");
	if (!begin)
	{
		UpdateCanvasOperationStatus(L"EditXaml", L"EditXaml", {}, begin);
		return;
	}

	XamlEditorDialog dialog(
		_canvas, std::move(xaml),
		_eventCodeInspection.CompatibleUserHandlers);
	dialog.ShowDialog(this->Handle);
	auto result = !_canvas->HasActiveDocumentTransaction()
		? DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged)
		: dialog.Applied
			? _canvas->CommitDocumentEditTransaction()
			: _canvas->RollbackDocumentEditTransaction();
	std::wstring completionMessage;
	if (dialog.Applied)
	{
		completionMessage = dialog.GetAppliedCheckpointCount() > 0
			? L"XAML 编辑已提交；此前应用的 "
				+ std::to_wstring(dialog.GetAppliedCheckpointCount())
				+ L" 个检查点保留为独立撤销步骤。"
			: L"XAML 编辑已提交。";
	}
	else if (dialog.GetAppliedCheckpointCount() > 0)
	{
		completionMessage = L"已取消最后一次应用后的 XAML 草稿；此前应用的 "
			+ std::to_wstring(dialog.GetAppliedCheckpointCount())
			+ L" 个检查点已保留。";
	}
	else
	{
		completionMessage = L"XAML 编辑已取消并恢复画布。";
	}
	// An Apply checkpoint commits history and immediately opens the next live
	// transaction.  Closing with Cancel rolls that final transaction back, which
	// intentionally emits no document-state change.  Refresh explicitly so the
	// newly committed checkpoint(s) become undoable in the toolbar right away.
	RefreshCommandAvailability();
	UpdateCanvasOperationStatus(
		L"EditXaml", L"EditXaml",
		std::move(completionMessage),
		result);
	if (!result)
	{
		ShowModalMessage(this, L"XAML 编辑事务失败",
			result.Error.empty() ? L"无法完成 XAML 编辑事务。" : result.Error);
	}
}

void Designer::OnArrangeClick()
{
	if (!_arrangeMenu || !_btnArrange || !_btnArrange->Enable) return;
	RefreshLockPresentation();
	_arrangeMenu->ShowAt(_btnArrange, 0, _btnArrange->Size.cy + 4);
}

void Designer::OnArrangeCommand(int commandId)
{
	if (!_canvas) return;
	if (commandId == ArrangeDuplicate)
	{
		(void)_canvas->DuplicateSelectedControls();
		return;
	}
	if (commandId == CanvasToggleLock)
	{
		ToggleSelectedControlsLocked();
		return;
	}
	std::optional<DesignerSelectionArrangeAction> action;
	switch (commandId)
	{
	case ArrangeAlignLeft: action = DesignerSelectionArrangeAction::AlignLeft; break;
	case ArrangeAlignHorizontalCenters: action = DesignerSelectionArrangeAction::AlignHorizontalCenters; break;
	case ArrangeAlignRight: action = DesignerSelectionArrangeAction::AlignRight; break;
	case ArrangeAlignTop: action = DesignerSelectionArrangeAction::AlignTop; break;
	case ArrangeAlignVerticalCenters: action = DesignerSelectionArrangeAction::AlignVerticalCenters; break;
	case ArrangeAlignBottom: action = DesignerSelectionArrangeAction::AlignBottom; break;
	case ArrangeDistributeHorizontally: action = DesignerSelectionArrangeAction::DistributeHorizontally; break;
	case ArrangeDistributeVertically: action = DesignerSelectionArrangeAction::DistributeVertically; break;
	case ArrangeMakeSameWidth: action = DesignerSelectionArrangeAction::MakeSameWidth; break;
	case ArrangeMakeSameHeight: action = DesignerSelectionArrangeAction::MakeSameHeight; break;
	case ArrangeMakeSameSize: action = DesignerSelectionArrangeAction::MakeSameSize; break;
	case ArrangeBringForward: action = DesignerSelectionArrangeAction::BringForward; break;
	case ArrangeSendBackward: action = DesignerSelectionArrangeAction::SendBackward; break;
	case ArrangeBringToFront: action = DesignerSelectionArrangeAction::BringToFront; break;
	case ArrangeSendToBack: action = DesignerSelectionArrangeAction::SendToBack; break;
	default: break;
	}
	if (action) (void)_canvas->ArrangeSelection(*action);
}

void Designer::OnCanvasMenuCommand(int commandId)
{
	if (!_canvas) return;
	switch (commandId)
	{
	case CanvasUndo:
		OnUndoClick();
		return;
	case CanvasRedo:
		OnRedoClick();
		return;
	case CanvasCut:
		OnCutClick();
		return;
	case CanvasCopy:
		OnCopyClick();
		return;
	case CanvasPaste:
		OnPasteClick();
		return;
	case CanvasPasteInPlace:
		(void)_canvas->PasteControlsFromClipboardInPlace();
		return;
	case CanvasPasteHere:
		if (_hasCanvasContextPastePoint)
			(void)_canvas->PasteControlsFromClipboardAt(
				_canvasContextPastePoint);
		return;
	case CanvasDuplicate:
		(void)_canvas->DuplicateSelectedControls();
		return;
	case CanvasDelete:
		OnDeleteClick();
		return;
	case CanvasToggleLock:
		ToggleSelectedControlsLocked();
		return;
	case CanvasSelectAll:
		(void)_canvas->SelectAllInCurrentContainer(true);
		return;
	case CanvasEditXaml:
		OnXamlClick();
		return;
	case CanvasViewFit:
		_canvas->FitDesignSurfaceToViewport();
		return;
	case CanvasViewActualSize:
		_canvas->ResetView();
		return;
	case CanvasViewZoomIn:
		_canvas->ZoomIn();
		return;
	case CanvasViewZoomOut:
		_canvas->ZoomOut();
		return;
	case CanvasToggleTabOrder:
		ToggleTabOrderMode();
		return;
	case CanvasAutoTabOrder:
		(void)_canvas->AutoArrangeTabOrder();
		RefreshTabOrderPresentation();
		return;
	case CanvasToggleGrid:
		_canvas->SetGridVisible(!_canvas->IsGridVisible());
		RefreshGridSettingsPresentation();
		break;
	case CanvasToggleSnapGrid:
		_canvas->SetSnapToGridEnabled(
			!_canvas->IsSnapToGridEnabled());
		RefreshGridSettingsPresentation();
		break;
	case CanvasToggleSnapGuides:
		_canvas->SetSnapToGuidesEnabled(
			!_canvas->IsSnapToGuidesEnabled());
		RefreshGridSettingsPresentation();
		break;
	case CanvasGridSize5:
	case CanvasGridSize10:
	case CanvasGridSize20:
		_canvas->SetGridSize(commandId == CanvasGridSize5 ? 5
			: commandId == CanvasGridSize10 ? 10 : 20);
		RefreshGridSettingsPresentation();
		break;
	default:
		OnArrangeCommand(commandId);
		return;
	}
	if (_lblInfo)
	{
		_lblInfo->Text = L"网格 "
			+ std::to_wstring(_canvas->GetGridSize()) + L" DIP；"
			+ (_canvas->IsSnapToGridEnabled()
				? L"网格吸附开；" : L"网格吸附关；")
			+ (_canvas->IsSnapToGuidesEnabled()
				? L"参考线吸附开" : L"参考线吸附关");
		_lblInfo->InvalidateVisual();
	}
}
