#include "Designer.h"
#include "DesignerControlCatalog.h"
#include "ProgrammaticControlFactory.h"
#include "CodeBehindExportDialog.h"
#include "XamlEditorDialog.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "DesignerModel/DesignDocumentFileFormat.h"
#include "DesignerModel/DesignRecoveryStore.h"
#include "SourceCodeNavigator.h"
#include "../CUI/include/Core/Threading.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/NativeVisualStateInfrastructure.h"
#include "../CUI/include/XamlInfrastructure.h"
#include "../CUI/include/WindowInfrastructure.h"
#include "../CUI/include/CuiGeneratedFrameworkTheme.h"
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
	inline constexpr int OutlineWindowIdentity = -1;

	bool TryGetOutlineIdentity(TreeViewItem* item, int& value)
	{
		return item && item->Tag.TryGetInt(value);
	}

	bool IsControlWithin(Control* control, Control* ancestor)
	{
		for (auto* current = control; current; current = current->GetVisualParent())
			if (current == ancestor) return true;
		return false;
	}

	bool IsOutlineShortcutKey(Key key, bool controlDown)
	{
		if (!controlDown) return key == Key::Delete;
		switch (key)
		{
		case Key::C:
		case Key::X:
		case Key::V:
		case Key::D:
		case Key::L:
		case Key::Z:
		case Key::Y:
		case Key::A:
			return true;
		default:
			return false;
		}
	}

	wchar_t OutlineShortcutControlCharacter(Key key)
	{
		switch (key)
		{
		case Key::A: return L'\x01';
		case Key::C: return L'\x03';
		case Key::D: return L'\x04';
		case Key::L: return L'\x0c';
		case Key::V: return L'\x16';
		case Key::X: return L'\x18';
		case Key::Y: return L'\x19';
		case Key::Z: return L'\x1a';
		default: return L'\0';
		}
	}

	inline constexpr const wchar_t* CanvasUndo = L"Designer.Canvas.Undo";
	inline constexpr const wchar_t* CanvasRedo = L"Designer.Canvas.Redo";
	inline constexpr const wchar_t* CanvasCut = L"Designer.Canvas.Cut";
	inline constexpr const wchar_t* CanvasCopy = L"Designer.Canvas.Copy";
	inline constexpr const wchar_t* CanvasPaste = L"Designer.Canvas.Paste";
	inline constexpr const wchar_t* CanvasPasteInPlace =
		L"Designer.Canvas.PasteInPlace";
	inline constexpr const wchar_t* CanvasPasteHere =
		L"Designer.Canvas.PasteHere";
	inline constexpr const wchar_t* CanvasDuplicate =
		L"Designer.Canvas.Duplicate";
	inline constexpr const wchar_t* CanvasDelete = L"Designer.Canvas.Delete";
	inline constexpr const wchar_t* CanvasToggleLock =
		L"Designer.Canvas.ToggleLock";
	inline constexpr const wchar_t* CanvasSelectAll =
		L"Designer.Canvas.SelectAll";
	inline constexpr const wchar_t* CanvasEditXaml =
		L"Designer.Canvas.EditXaml";
	inline constexpr const wchar_t* CanvasViewFit = L"Designer.View.Fit";
	inline constexpr const wchar_t* CanvasViewActualSize =
		L"Designer.View.ActualSize";
	inline constexpr const wchar_t* CanvasViewZoomIn =
		L"Designer.View.ZoomIn";
	inline constexpr const wchar_t* CanvasViewZoomOut =
		L"Designer.View.ZoomOut";
	inline constexpr const wchar_t* CanvasToggleGrid =
		L"Designer.Grid.Toggle";
	inline constexpr const wchar_t* CanvasToggleSnapGrid =
		L"Designer.Grid.ToggleSnap";
	inline constexpr const wchar_t* CanvasToggleSnapGuides =
		L"Designer.Grid.ToggleGuides";
	inline constexpr const wchar_t* CanvasGridSize5 = L"Designer.Grid.Size5";
	inline constexpr const wchar_t* CanvasGridSize10 = L"Designer.Grid.Size10";
	inline constexpr const wchar_t* CanvasGridSize20 = L"Designer.Grid.Size20";
	inline constexpr const wchar_t* CanvasToggleTabOrder =
		L"Designer.TabOrder.Toggle";
	inline constexpr const wchar_t* CanvasAutoTabOrder =
		L"Designer.TabOrder.Auto";
	inline constexpr const wchar_t* ArrangeDuplicate =
		L"Designer.Arrange.Duplicate";
	inline constexpr const wchar_t* ArrangeAlignLeft =
		L"Designer.Arrange.AlignLeft";
	inline constexpr const wchar_t* ArrangeAlignHorizontalCenters =
		L"Designer.Arrange.AlignHorizontalCenters";
	inline constexpr const wchar_t* ArrangeAlignRight =
		L"Designer.Arrange.AlignRight";
	inline constexpr const wchar_t* ArrangeAlignTop =
		L"Designer.Arrange.AlignTop";
	inline constexpr const wchar_t* ArrangeAlignVerticalCenters =
		L"Designer.Arrange.AlignVerticalCenters";
	inline constexpr const wchar_t* ArrangeAlignBottom =
		L"Designer.Arrange.AlignBottom";
	inline constexpr const wchar_t* ArrangeDistributeHorizontally =
		L"Designer.Arrange.DistributeHorizontally";
	inline constexpr const wchar_t* ArrangeDistributeVertically =
		L"Designer.Arrange.DistributeVertically";
	inline constexpr const wchar_t* ArrangeMakeSameWidth =
		L"Designer.Arrange.MakeSameWidth";
	inline constexpr const wchar_t* ArrangeMakeSameHeight =
		L"Designer.Arrange.MakeSameHeight";
	inline constexpr const wchar_t* ArrangeMakeSameSize =
		L"Designer.Arrange.MakeSameSize";
	inline constexpr const wchar_t* ArrangeBringForward =
		L"Designer.Arrange.BringForward";
	inline constexpr const wchar_t* ArrangeSendBackward =
		L"Designer.Arrange.SendBackward";
	inline constexpr const wchar_t* ArrangeBringToFront =
		L"Designer.Arrange.BringToFront";
	inline constexpr const wchar_t* ArrangeSendToBack =
		L"Designer.Arrange.SendToBack";

	MenuItem* FindDesignerCommand(
		ContextMenu* menu,
		std::wstring_view commandName)
	{
		return menu
			? menu->FindItemByCommand(std::wstring(commandName)) : nullptr;
	}

	void CollectDesignerCommandNames(
		MenuItem& item,
		std::set<std::wstring>& names)
	{
		const auto command = item.GetCommand();
		if (!command.empty()) names.insert(command);
		for (auto* entry : item.GetMenuItemsView())
			if (auto* child = dynamic_cast<MenuItem*>(entry))
				CollectDesignerCommandNames(*child, names);
	}

	template<typename THandler>
	void BindDesignerMenuCommands(
		Control& routeOwner,
		std::initializer_list<ContextMenu*> menus,
		THandler handler)
	{
		std::vector<ContextMenu*> menuList(menus);
		std::set<std::wstring> commandNames;
		for (auto* menu : menuList)
		{
			if (!menu) continue;
			for (int index = 0; index < menu->ItemCount(); ++index)
				if (auto* item = menu->GetItem(index))
					CollectDesignerCommandNames(*item, commandNames);
		}
		for (const auto& commandName : commandNames)
		{
			CommandBinding binding;
			binding.Command = RoutedCommand(commandName);
			binding.CanExecute = [menuList](
				Control*, CanExecuteRoutedEventArgs& args)
			{
				bool found = false;
				bool enabled = false;
				for (const auto* menu : menuList)
				{
					const auto* item = menu
						? menu->FindItemByCommand(args.Command.Name()) : nullptr;
					if (!item) continue;
					found = true;
					enabled = enabled || item->IsLocallyEnabled();
				}
				if (found) args.CanExecute = enabled;
			};
			binding.Executed = [handler](
				Control*, ExecutedRoutedEventArgs& args) mutable
			{
				handler(std::wstring_view(args.Command.Name()));
				args.Executed = true;
			};
			cui::framework::XamlAccess::RetainEventConnection(
				routeOwner,
				routeOwner.AddCommandBinding(std::move(binding)));
		}
	}

	static std::string MakeDesignFilter()
	{
		std::string s;
		s.append("CUI XAML Files (*.cui.xaml;*.xaml)");
		s.push_back('\0');
		s.append("*.cui.xaml;*.xaml");
		s.push_back('\0');
		s.append("Legacy CUI Designer Files (*.cui.xml;*.xml)");
		s.push_back('\0');
		s.append("*.cui.xml;*.xml");
		s.push_back('\0');
		s.push_back('\0');
		return s;
	}


	static void ShowModalMessage(Window* ownerWindow, const std::wstring& caption, const std::wstring& text)
	{
		::MessageBoxW(ownerWindow->Handle, text.c_str(), caption.c_str(), MB_OK | MB_SETFOREGROUND);
	}

	static cui::core::Size GetLogicalDesignerContentSize(Window* window)
	{
		return window ? window->GetContentViewportSizeDip()
			: cui::core::Size{};
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
		if (operation == L"BoxSelection") return L"框选控件";
		if (operation == L"AdoptVisualChild") return L"添加控件";
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

Designer::Designer()
	: Window(),
	_controlDescriptors(DesignerControlCatalog::BuiltInDescriptors())
{
	this->Title = L"CUI 窗口设计器";
	this->Width = 1400.0f;
	this->Height = 840.0f;
	this->Background = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	SetVisualContent(std::move(contentOwner));
}

Designer::~Designer()
{
	StopClipboardMonitoring();
	// Window tears down its owned controls as the native window closes, before the
	// stack-allocated Designer itself is destroyed.  Do not publish shutdown
	// state through raw child-control pointers after that ownership teardown.
	_propertyGrid = nullptr;
	_lblInfo = nullptr;
	ResetCodeFreshnessTracking();
	DiscardSessionRecoverySnapshot();
}

void Designer::ApplyFrameworkTheme()
{
	auto* contentRoot = GetVisualContent();
	if (!contentRoot) return;

	std::wstring themeError;
	if (!CuiGeneratedFrameworkTheme::Apply(
		*contentRoot, true, &themeError))
	{
		throw std::runtime_error(Convert::UnicodeToUtf8(
			themeError.empty()
				? L"Designer 无法加载 Generic.xaml 主题。"
				: themeError));
	}
}

void Designer::InitAndShow()
{
	InitializeComponents();
	ApplyFrameworkTheme();
	InitializeRecoverySession();
	this->Show();
	StartClipboardMonitoring();
	RefreshCommandAvailability();
	TryRestoreRecoveryOnStartup();
}

void Designer::InitializeComponents()
{
	auto* contentRoot = static_cast<Panel*>(GetVisualContent());
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };
	// 顶部工具栏
	int toolbarHeight = 50;
	int btnWidth = 80;
	int btnHeight = 30;
	int btnY = 10;
	int btnX = 10;
	
	_btnNew = cui::designer::NewControl<Button>(L"新建", btnX, btnY, btnWidth, btnHeight);
	_btnNew->Click += [this](Control* sender, RoutedEventArgs& e) {
		OnNewClick();
	};
	addContent(_btnNew);
	btnX += btnWidth + 10;

	_btnOpen = cui::designer::NewControl<Button>(L"打开", btnX, btnY, btnWidth, btnHeight);
	_btnOpen->Click += [this](Control*, RoutedEventArgs&) {
		OnOpenClick();
	};
	addContent(_btnOpen);
	btnX += btnWidth + 10;

	_btnSave = cui::designer::NewControl<Button>(L"保存", btnX, btnY, btnWidth, btnHeight);
	_btnSave->Click += [this](Control*, RoutedEventArgs&) {
		OnSaveClick();
	};
	addContent(_btnSave);
	btnX += btnWidth + 10;

	_btnReload = cui::designer::NewControl<Button>(L"重新加载", btnX, btnY, btnWidth + 10, btnHeight);
	_btnReload->IsEnabled = false;
	_btnReload->Click += [this](Control*, RoutedEventArgs&) {
		OnReloadClick();
	};
	addContent(_btnReload);
	btnX += btnWidth + 20;
	
	_btnExport = cui::designer::NewControl<Button>(L"导出代码", btnX, btnY, btnWidth + 20, btnHeight);
	_btnExport->Click += [this](Control* sender, RoutedEventArgs& e) {
		OnExportClick();
	};
	addContent(_btnExport);
	btnX += btnWidth + 30;

	_btnRegenerate = cui::designer::NewControl<Button>(L"重新生成", btnX, btnY, btnWidth + 20, btnHeight);
	_btnRegenerate->IsEnabled = false;
	_btnRegenerate->Click += [this](Control*, RoutedEventArgs&) {
		OnRegenerateCodeClick();
	};
	addContent(_btnRegenerate);
	btnX += btnWidth + 15;

	const int historyButtonWidth = 58;
	_btnUndo = cui::designer::NewControl<Button>(L"撤销", btnX, btnY,
		historyButtonWidth, btnHeight);
	_btnUndo->IsEnabled = false;
	_btnUndo->AutomationName = L"撤销";
	_btnUndo->AutomationFullDescription = L"没有可撤销的操作。快捷键 Ctrl+Z。";
	_btnUndo->Click += [this](Control*, RoutedEventArgs&) {
		OnUndoClick();
	};
	addContent(_btnUndo);
	btnX += historyButtonWidth + 6;

	_btnRedo = cui::designer::NewControl<Button>(L"重做", btnX, btnY,
		historyButtonWidth, btnHeight);
	_btnRedo->IsEnabled = false;
	_btnRedo->AutomationName = L"重做";
	_btnRedo->AutomationFullDescription = L"没有可重做的操作。快捷键 Ctrl+Y。";
	_btnRedo->Click += [this](Control*, RoutedEventArgs&) {
		OnRedoClick();
	};
	addContent(_btnRedo);
	btnX += historyButtonWidth + 10;

	const int editButtonWidth = 64;
	_btnCopy = cui::designer::NewControl<Button>(L"复制", btnX, btnY, editButtonWidth, btnHeight);
	_btnCopy->IsEnabled = false;
	_btnCopy->Click += [this](Control*, RoutedEventArgs&) {
		OnCopyClick();
	};
	addContent(_btnCopy);
	btnX += editButtonWidth + 6;

	_btnCut = cui::designer::NewControl<Button>(L"剪切", btnX, btnY, editButtonWidth, btnHeight);
	_btnCut->IsEnabled = false;
	_btnCut->Click += [this](Control*, RoutedEventArgs&) {
		OnCutClick();
	};
	addContent(_btnCut);
	btnX += editButtonWidth + 6;

	_btnPaste = cui::designer::NewControl<Button>(L"粘贴", btnX, btnY, editButtonWidth, btnHeight);
	_btnPaste->Click += [this](Control*, RoutedEventArgs&) {
		OnPasteClick();
	};
	addContent(_btnPaste);
	btnX += editButtonWidth + 6;

	_btnXaml = cui::designer::NewControl<Button>(L"XAML", btnX, btnY, editButtonWidth, btnHeight);
	_btnXaml->Click += [this](Control*, RoutedEventArgs&) {
		OnXamlClick();
	};
	addContent(_btnXaml);
	btnX += editButtonWidth + 6;

	_btnArrange = cui::designer::NewControl<Button>(L"排列", btnX, btnY, editButtonWidth, btnHeight);
	_btnArrange->IsEnabled = false;
	_btnArrange->Click += [this](Control*, RoutedEventArgs&) {
		OnArrangeClick();
	};
	addContent(_btnArrange);
	btnX += editButtonWidth + 15;
	
	_btnDelete = cui::designer::NewControl<Button>(L"删除", btnX, btnY, btnWidth, btnHeight);
	_btnDelete->Background = Colors::IndianRed;
	_btnDelete->Click += [this](Control* sender, RoutedEventArgs& e) {
		OnDeleteClick();
	};
	addContent(_btnDelete);
	btnX += btnWidth + 30;
	
	_lblInfo = cui::designer::NewControl<Label>(L"就绪", btnX, btnY + 5);
	_lblInfo->Width = 190.0f;
	_lblInfo->Height = 25.0f;
	cui::designer::ApplyProgrammaticTypography(
		*_lblInfo, L"Microsoft YaHei", 14.0);
	addContent(_lblInfo);

	_arrangeMenu = new ContextMenu();
	auto* duplicate = cui::designer::AddMenuItem(*_arrangeMenu,
		L"重复", ArrangeDuplicate);
	duplicate->InputGestureText = L"Ctrl+D";
	auto* lockItem = cui::designer::AddMenuItem(*_arrangeMenu,
		L"锁定控件", CanvasToggleLock);
	lockItem->InputGestureText = L"Ctrl+L";
	_arrangeMenu->AddSeparator();
	auto* align = cui::designer::AddMenuItem(*_arrangeMenu, L"对齐");
	cui::designer::AddMenuItem(*align, L"左对齐", ArrangeAlignLeft);
	cui::designer::AddMenuItem(
		*align, L"水平中心", ArrangeAlignHorizontalCenters);
	cui::designer::AddMenuItem(*align, L"右对齐", ArrangeAlignRight);
	align->AddSeparator();
	cui::designer::AddMenuItem(*align, L"顶端对齐", ArrangeAlignTop);
	cui::designer::AddMenuItem(
		*align, L"垂直中心", ArrangeAlignVerticalCenters);
	cui::designer::AddMenuItem(*align, L"底端对齐", ArrangeAlignBottom);
	auto* distribute = cui::designer::AddMenuItem(*_arrangeMenu, L"分布");
	cui::designer::AddMenuItem(
		*distribute, L"水平分布", ArrangeDistributeHorizontally);
	cui::designer::AddMenuItem(
		*distribute, L"垂直分布", ArrangeDistributeVertically);
	auto* sameSize = cui::designer::AddMenuItem(*_arrangeMenu, L"相同尺寸");
	cui::designer::AddMenuItem(*sameSize, L"宽度", ArrangeMakeSameWidth);
	cui::designer::AddMenuItem(*sameSize, L"高度", ArrangeMakeSameHeight);
	cui::designer::AddMenuItem(
		*sameSize, L"宽度和高度", ArrangeMakeSameSize);
	auto* layer = cui::designer::AddMenuItem(*_arrangeMenu, L"层级");
	auto* forward = cui::designer::AddMenuItem(*layer,
		L"上移一层", ArrangeBringForward);
	forward->InputGestureText = L"Ctrl+]";
	auto* backward = cui::designer::AddMenuItem(*layer,
		L"下移一层", ArrangeSendBackward);
	backward->InputGestureText = L"Ctrl+[";
	auto* front = cui::designer::AddMenuItem(*layer,
		L"置于顶层", ArrangeBringToFront);
	front->InputGestureText = L"Ctrl+Shift+]";
	auto* back = cui::designer::AddMenuItem(*layer,
		L"置于底层", ArrangeSendToBack);
	back->InputGestureText = L"Ctrl+Shift+[";
	addContent(_arrangeMenu);

	_canvasMenu = new ContextMenu();
	auto* undoItem = cui::designer::AddMenuItem(*_canvasMenu,
		L"撤销", CanvasUndo);
	undoItem->InputGestureText = L"Ctrl+Z";
	auto* redoItem = cui::designer::AddMenuItem(*_canvasMenu,
		L"重做", CanvasRedo);
	redoItem->InputGestureText = L"Ctrl+Y";
	_canvasMenu->AddSeparator();
	auto* cutItem = cui::designer::AddMenuItem(
		*_canvasMenu, L"剪切", CanvasCut);
	cutItem->InputGestureText = L"Ctrl+X";
	auto* copyItem = cui::designer::AddMenuItem(
		*_canvasMenu, L"复制", CanvasCopy);
	copyItem->InputGestureText = L"Ctrl+C";
	auto* pasteItem = cui::designer::AddMenuItem(
		*_canvasMenu, L"粘贴", CanvasPaste);
	pasteItem->InputGestureText = L"Ctrl+V";
	auto* pasteInPlaceItem = cui::designer::AddMenuItem(*_canvasMenu,
		L"原位粘贴", CanvasPasteInPlace);
	pasteInPlaceItem->InputGestureText = L"Ctrl+Shift+V";
	cui::designer::AddMenuItem(*_canvasMenu, L"粘贴到此处", CanvasPasteHere);
	auto* duplicateItem = cui::designer::AddMenuItem(*_canvasMenu,
		L"重复", CanvasDuplicate);
	duplicateItem->InputGestureText = L"Ctrl+D";
	cui::designer::AddMenuItem(*_canvasMenu, L"删除", CanvasDelete)
		->InputGestureText = L"Delete";
	auto* contextLockItem = cui::designer::AddMenuItem(*_canvasMenu,
		L"锁定控件", CanvasToggleLock);
	contextLockItem->InputGestureText = L"Ctrl+L";
	_canvasMenu->AddSeparator();
	auto* arrangeItem = cui::designer::AddMenuItem(*_canvasMenu, L"排列");
	auto* contextAlign = cui::designer::AddMenuItem(*arrangeItem, L"对齐");
	cui::designer::AddMenuItem(*contextAlign,
		L"左对齐", ArrangeAlignLeft);
	cui::designer::AddMenuItem(*contextAlign,
		L"水平中心", ArrangeAlignHorizontalCenters);
	cui::designer::AddMenuItem(*contextAlign,
		L"右对齐", ArrangeAlignRight);
	contextAlign->AddSeparator();
	cui::designer::AddMenuItem(*contextAlign,
		L"顶端对齐", ArrangeAlignTop);
	cui::designer::AddMenuItem(*contextAlign,
		L"垂直中心", ArrangeAlignVerticalCenters);
	cui::designer::AddMenuItem(*contextAlign,
		L"底端对齐", ArrangeAlignBottom);
	auto* contextDistribute = cui::designer::AddMenuItem(*arrangeItem, L"分布");
	cui::designer::AddMenuItem(*contextDistribute,
		L"水平分布", ArrangeDistributeHorizontally);
	cui::designer::AddMenuItem(*contextDistribute,
		L"垂直分布", ArrangeDistributeVertically);
	auto* contextSize = cui::designer::AddMenuItem(*arrangeItem, L"相同尺寸");
	cui::designer::AddMenuItem(*contextSize,
		L"宽度", ArrangeMakeSameWidth);
	cui::designer::AddMenuItem(*contextSize,
		L"高度", ArrangeMakeSameHeight);
	cui::designer::AddMenuItem(*contextSize,
		L"宽度和高度", ArrangeMakeSameSize);
	auto* contextLayer = cui::designer::AddMenuItem(*arrangeItem, L"层级");
	cui::designer::AddMenuItem(*contextLayer,
		L"上移一层", ArrangeBringForward)
		->InputGestureText = L"Ctrl+]";
	cui::designer::AddMenuItem(*contextLayer,
		L"下移一层", ArrangeSendBackward)
		->InputGestureText = L"Ctrl+[";
	cui::designer::AddMenuItem(*contextLayer,
		L"置于顶层", ArrangeBringToFront)
		->InputGestureText = L"Ctrl+Shift+]";
	cui::designer::AddMenuItem(*contextLayer,
		L"置于底层", ArrangeSendToBack)
		->InputGestureText = L"Ctrl+Shift+[";
	cui::designer::AddMenuItem(*_canvasMenu,
		L"全选当前容器", CanvasSelectAll)
		->InputGestureText = L"Ctrl+A";
	_canvasMenu->AddSeparator();
	cui::designer::AddMenuItem(*_canvasMenu,
		L"编辑 XAML", CanvasEditXaml);
	_canvasMenu->AddSeparator();
	auto* viewItem = cui::designer::AddMenuItem(*_canvasMenu, L"视图");
	cui::designer::AddMenuItem(*viewItem,
		L"适合窗口", CanvasViewFit)
		->InputGestureText = L"Ctrl+0";
	cui::designer::AddMenuItem(*viewItem,
		L"实际大小 (100%)", CanvasViewActualSize)
		->InputGestureText = L"Ctrl+1";
	viewItem->AddSeparator();
	cui::designer::AddMenuItem(*viewItem,
		L"放大", CanvasViewZoomIn)
		->InputGestureText = L"Ctrl++";
	cui::designer::AddMenuItem(*viewItem,
		L"缩小", CanvasViewZoomOut)
		->InputGestureText = L"Ctrl+-";
	viewItem->AddSeparator();
	auto* gridViewItem = cui::designer::AddMenuItem(*viewItem, L"网格与吸附");
	cui::designer::AddMenuItem(*gridViewItem,
		L"显示网格", CanvasToggleGrid);
	cui::designer::AddMenuItem(*gridViewItem,
		L"吸附到网格", CanvasToggleSnapGrid);
	cui::designer::AddMenuItem(*gridViewItem,
		L"吸附到参考线", CanvasToggleSnapGuides);
	gridViewItem->AddSeparator();
	cui::designer::AddMenuItem(*gridViewItem,
		L"网格间距 5 DIP", CanvasGridSize5);
	cui::designer::AddMenuItem(*gridViewItem,
		L"网格间距 10 DIP", CanvasGridSize10);
	cui::designer::AddMenuItem(*gridViewItem,
		L"网格间距 20 DIP", CanvasGridSize20);
	viewItem->AddSeparator();
	cui::designer::AddMenuItem(*viewItem,
		L"Tab 顺序模式", CanvasToggleTabOrder);
	cui::designer::AddMenuItem(*viewItem,
		L"按布局自动排序", CanvasAutoTabOrder);
	addContent(_canvasMenu);
	
	// 左侧工具箱 / 文档层级切换
	int toolBoxWidth = 150;
	const auto contentSize = GetLogicalDesignerContentSize(this);
	int formHeight = static_cast<int>(std::lround(contentSize.height));
	const int sidebarTabsHeight = 30;
	_btnToolboxView = cui::designer::NewControl<Button>(
		L"工具箱", 10, toolbarHeight + 10, 72, 26);
	_btnOutlineView = cui::designer::NewControl<Button>(
		L"层级", 86, toolbarHeight + 10, 74, 26);
	for (auto* button : { _btnToolboxView, _btnOutlineView })
	{
		button->BorderThickness = 1.0f;
		button->Background = D2D1::ColorF(0.88f, 0.90f, 0.94f, 1.0f);
		addContent(button);
	}
	_btnToolboxView->AutomationName = L"显示设计器工具箱";
	_btnOutlineView->AutomationName = L"显示文档层级";
	_btnToolboxView->Click += [this](Control*, RoutedEventArgs&) {
		SetSidebarView(false);
	};
	_btnOutlineView->Click += [this](Control*, RoutedEventArgs&) {
		SetSidebarView(true);
	};

	_toolBox = new ToolBox(
		10, toolbarHeight + 10 + sidebarTabsHeight, toolBoxWidth,
		formHeight - toolbarHeight - 40 - sidebarTabsHeight);
	_toolBox->OnControlSelected += [this](const DesignerControlDescriptor& descriptor) {
		OnToolBoxControlSelected(descriptor);
	};
	_toolBox->OnControlDragReady +=
		[this](const DesignerControlDescriptor& descriptor, POINT formPoint)
		{
			BeginToolBoxDrag(descriptor, formPoint);
		};
	addContent(_toolBox);

	_outlineScroll = cui::designer::NewControl<ScrollViewer>(
		10, toolbarHeight + 10 + sidebarTabsHeight, toolBoxWidth,
		formHeight - toolbarHeight - 40 - sidebarTabsHeight);
	_outlineScroll->VerticalScrollBarVisibility = ScrollBarVisibility::Visible;
	_outlineTree = cui::designer::NewControl<TreeView>(0, 0, toolBoxWidth, 0);
	_outlineTree->Width = cui::layout::Length::Auto();
	_outlineTree->Height = cui::layout::Length::Auto();
	_outlineTree->HorizontalAlignment = HorizontalAlignment::Stretch;
	_outlineTree->Focusable = true;
	_outlineTree->AutomationName = L"文档层级";
	_outlineTree->AutomationFullDescription =
		L"显示窗体与控件父子关系；可选择被遮挡或不可见控件，拖拽可重排或更换父容器。";
	_outlineTree->SelectedItemChanged += [this](
		Control*, RoutedPropertyChangedEventArgs<BindingValue>&) {
		OnDocumentOutlineSelectionChanged();
	};
	_outlineTree->OnMouseDown += [this](Control*, MouseEventArgs& args) {
		BeginDocumentOutlineDrag(args);
	};
	_outlineTree->OnKeyDown += [this](Control*, KeyEventArgs& args) {
		if (args.HasModifier(ModifierKeys::Alt)) return;
		(void)QueueOutlineShortcut(
			args.Key, args.HasModifier(ModifierKeys::Control), args.HasModifier(ModifierKeys::Shift));
	};
	_outlineScroll->SetVisualContent(
		std::unique_ptr<Control>(_outlineTree));
	addContent(_outlineScroll);
	SetSidebarView(false);
	
	// 属性面板（右侧）
	int propertyGridWidth = 250;
	int formWidth = static_cast<int>(std::lround(contentSize.width));
	_propertyGrid = new PropertyGrid(formWidth - propertyGridWidth - 15, toolbarHeight + 10, 
		propertyGridWidth, formHeight - toolbarHeight - 40);
	_propertyGrid->OnEventHandlerActivated +=
		[this](PropertyGrid*, const std::wstring& handlerName)
		{
			OnEventHandlerActivated(handlerName);
		};
	addContent(_propertyGrid);
	
	// 设计画布（中间）
	int canvasX = toolBoxWidth + 20;
	int canvasWidth = formWidth - toolBoxWidth - propertyGridWidth - 40;
	_canvas = new DesignerCanvas(canvasX, toolbarHeight + 10, canvasWidth, formHeight - toolbarHeight - 40);
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
	_canvas->AutomationFullDescription =
		L"设计画布。Ctrl+滚轮缩放；按住中键或空格拖动可平移；Ctrl+0 适合窗口；Ctrl+1 恢复 100%。";
	addContent(_canvas);

	const int zoomStripY = formHeight - 26;
	const int zoomStripRight = canvasX + canvasWidth;
	_btnTabOrder = cui::designer::NewControl<Button>(
		L"Tab 顺序", zoomStripRight - 352, zoomStripY, 84, 22);
	_btnTabOrder->AutomationName = L"Tab 顺序模式";
	_btnTabOrder->AutomationFullDescription =
		L"显示可接收键盘焦点控件的 TabIndex；进入后依次单击控件编号。";
	_btnTabOrder->Click += [this](Control*, RoutedEventArgs&) {
		ToggleTabOrderMode();
	};
	addContent(_btnTabOrder);

	_btnGridSettings = cui::designer::NewControl<Button>(
		L"网格 10", zoomStripRight - 262, zoomStripY, 76, 22);
	_btnGridSettings->AutomationName = L"网格与吸附设置";
	_btnGridSettings->OnMouseDown += [this](Control*, MouseEventArgs& args) {
		if (args.ChangedButton != MouseButton::Left) return;
		if (!_gridMenu || !_btnGridSettings) return;
		RefreshGridSettingsPresentation();
		const float buttonHeight = _btnGridSettings->ActualHeight > 0.0f
			? _btnGridSettings->ActualHeight
			: (_btnGridSettings->Height.IsFixed()
				? _btnGridSettings->Height.value : 0.0f);
		_gridMenu->ShowAt(
			_btnGridSettings, 0,
			static_cast<int>(std::lround(buttonHeight)), true);
	};
	addContent(_btnGridSettings);

	_gridMenu = new ContextMenu();
	cui::designer::AddMenuItem(*_gridMenu,
		L"显示网格", CanvasToggleGrid);
	cui::designer::AddMenuItem(*_gridMenu,
		L"吸附到网格", CanvasToggleSnapGrid);
	cui::designer::AddMenuItem(*_gridMenu,
		L"吸附到参考线", CanvasToggleSnapGuides);
	_gridMenu->AddSeparator();
	cui::designer::AddMenuItem(*_gridMenu,
		L"网格间距 5 DIP", CanvasGridSize5);
	cui::designer::AddMenuItem(*_gridMenu,
		L"网格间距 10 DIP", CanvasGridSize10);
	cui::designer::AddMenuItem(*_gridMenu,
		L"网格间距 20 DIP", CanvasGridSize20);
	addContent(_gridMenu);
	BindDesignerMenuCommands(
		*this, { _arrangeMenu, _canvasMenu, _gridMenu },
		[this](std::wstring_view commandName) {
			OnCanvasMenuCommand(commandName);
		});
	RefreshGridSettingsPresentation();
	RefreshTabOrderPresentation();

	_btnFitView = cui::designer::NewControl<Button>(
		L"适配", zoomStripRight - 52, zoomStripY, 52, 22);
	_btnFitView->AutomationFullDescription = L"使设计窗体适合当前画布。快捷键 Ctrl+0。";
	_btnFitView->Click += [this](Control*, RoutedEventArgs&) {
		if (_canvas) _canvas->FitDesignSurfaceToViewport();
	};
	addContent(_btnFitView);

	_btnZoomIn = cui::designer::NewControl<Button>(
		L"+", zoomStripRight - 86, zoomStripY, 28, 22);
	_btnZoomIn->AutomationName = L"放大设计画布";
	_btnZoomIn->AutomationFullDescription = L"放大设计画布。快捷键 Ctrl+加号。";
	_btnZoomIn->Click += [this](Control*, RoutedEventArgs&) {
		if (_canvas) _canvas->ZoomIn();
	};
	addContent(_btnZoomIn);

	_lblZoom = cui::designer::NewControl<Label>(
		L"100%", zoomStripRight - 146, zoomStripY + 2);
	_lblZoom->Width = 54.0f;
	_lblZoom->Height = 20.0f;
	_lblZoom->AutomationName = L"设计画布缩放比例";
	addContent(_lblZoom);

	_btnZoomOut = cui::designer::NewControl<Button>(
		L"-", zoomStripRight - 180, zoomStripY, 28, 22);
	_btnZoomOut->AutomationName = L"缩小设计画布";
	_btnZoomOut->AutomationFullDescription = L"缩小设计画布。快捷键 Ctrl+减号。";
	_btnZoomOut->Click += [this](Control*, RoutedEventArgs&) {
		if (_canvas) _canvas->ZoomOut();
	};
	addContent(_btnZoomOut);

	RebuildDocumentOutline();
	_canvas->FitDesignSurfaceToViewport();
	UpdateDocumentPresentation();

	this->OnClosing += [this](Window*, CancelEventArgs& args) {
		if (_closeApproved)
		{
			DiscardSessionRecoverySnapshot();
			return;
		}
		if (!ConfirmCanReplaceOrCloseDocument())
		{
			args.Cancel = true;
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
		auto arrange = [](Control* control, float x, float y,
			float width, float height)
		{
			if (!control) return;
			width = (std::max)(0.0f, width);
			height = (std::max)(0.0f, height);
			Canvas::SetLeft(*(control), x);
			Canvas::SetTop(*(control), y);
			Canvas::SetRight(*(control), cui::layout::UnsetCanvasOffset);
			Canvas::SetBottom(*(control), cui::layout::UnsetCanvasOffset);
			control->Width = width;
			control->Height = height;
			control->Arrange({ x, y, width, height });
		};
		auto move = [&arrange](Control* control, float x, float y)
		{
			if (!control) return;
			float width = control->ActualWidth;
			float height = control->ActualHeight;
			if (width <= 0.0f && control->Width.IsFixed())
				width = control->Width.value;
			if (height <= 0.0f && control->Height.IsFixed())
				height = control->Height.value;
			arrange(control, x, y, width, height);
		};
		const auto contentSize = GetLogicalDesignerContentSize(this);
		int w = static_cast<int>(std::lround(contentSize.width));
		int h = static_cast<int>(std::lround(contentSize.height));
		int usableH = h - toolbarHeight - 40;
		if (usableH < 50) usableH = 50;
		if (_toolBox)
			arrange(_toolBox, 10.0f,
				static_cast<float>(toolbarHeight + 10 + sidebarTabsHeight),
				static_cast<float>(toolBoxWidth),
				static_cast<float>((std::max)(50, usableH - sidebarTabsHeight)));
		if (_btnToolboxView)
			move(_btnToolboxView, 10.0f,
				static_cast<float>(toolbarHeight + 10));
		if (_btnOutlineView)
			move(_btnOutlineView, 86.0f,
				static_cast<float>(toolbarHeight + 10));
		if (_outlineScroll)
			arrange(_outlineScroll, 10.0f,
				static_cast<float>(toolbarHeight + 10 + sidebarTabsHeight),
				static_cast<float>(toolBoxWidth),
				static_cast<float>((std::max)(50, usableH - sidebarTabsHeight)));
		if (_propertyGrid)
		{
			arrange(_propertyGrid,
				static_cast<float>(w - propertyGridWidth - 15),
				static_cast<float>(toolbarHeight + 10),
				static_cast<float>(propertyGridWidth),
				static_cast<float>(usableH));
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
			arrange(_canvas, static_cast<float>(canvasX),
				static_cast<float>(toolbarHeight + 10),
				static_cast<float>(canvasW), static_cast<float>(usableH));
			if (refitCanvas) _canvas->FitDesignSurfaceToViewport();
			else _canvas->InvalidateVisual();

			const int zoomY = h - 26;
			const int zoomRight = canvasX + canvasW;
			if (_btnFitView)
				move(_btnFitView, static_cast<float>(zoomRight - 52), static_cast<float>(zoomY));
			if (_btnZoomIn)
				move(_btnZoomIn, static_cast<float>(zoomRight - 86), static_cast<float>(zoomY));
			if (_lblZoom)
				move(_lblZoom, static_cast<float>(zoomRight - 146), static_cast<float>(zoomY + 2));
			if (_btnZoomOut)
				move(_btnZoomOut, static_cast<float>(zoomRight - 180), static_cast<float>(zoomY));
			if (_btnGridSettings)
				move(_btnGridSettings, static_cast<float>(zoomRight - 262), static_cast<float>(zoomY));
			if (_btnTabOrder)
				move(_btnTabOrder, static_cast<float>(zoomRight - 352), static_cast<float>(zoomY));
		}
	};

	this->SizeChanged += [doLayout](Control*, SizeChangedEventArgs&)
	{ doLayout(); };
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
	if (_toolBox) _toolBox->Visibility = showDocumentOutline
		? Visibility::Collapsed : Visibility::Visible;
	if (_outlineScroll) _outlineScroll->Visibility = showDocumentOutline
		? Visibility::Visible : Visibility::Collapsed;
	if (_btnToolboxView)
	{
		cui::framework::NativeVisualStateAccess::Set(
			*_btnToolboxView, ControlStyleState::Checked, !showDocumentOutline);
		_btnToolboxView->AutomationFullDescription = !showDocumentOutline
			? L"当前正在显示工具箱。" : L"切换到工具箱。";
		_btnToolboxView->InvalidateVisual();
	}
	if (_btnOutlineView)
	{
		cui::framework::NativeVisualStateAccess::Set(
			*_btnOutlineView, ControlStyleState::Checked, showDocumentOutline);
		_btnOutlineView->AutomationFullDescription = showDocumentOutline
			? L"当前正在显示文档层级。" : L"切换到文档层级。";
		_btnOutlineView->InvalidateVisual();
	}
	if (showDocumentOutline)
	{
		// The view-toggle click is still walking the Window/child input stack.
		// Defer destructive TreeViewItem replacement through the same coalesced path
		// used by structural document changes; RebuildDocumentOutline performs
		// selection synchronization once the new tree is stable.
		ScheduleDocumentOutlineRebuild();
	}
	if (_canvas) this->SetKeyboardFocus(_canvas, true);
	if (_toolBox) _toolBox->InvalidateVisual();
	if (_outlineTree) _outlineTree->InvalidateVisual();
}

void Designer::RebuildDocumentOutline()
{
	if (!_outlineTree || !_canvas) return;
	if (_outlinePointerDown) CancelDocumentOutlineDrag();
	else _outlineTree->ClearDropTarget();
	std::set<int> expandedIds;
	for (const auto& [stableId, item] : _outlineNodesByStableId)
		if (item && item->GetIsExpanded()) expandedIds.insert(stableId);
	const bool hadOutline = _outlineWindowNode != nullptr;
	const bool formExpanded = !hadOutline
		|| _outlineWindowNode->GetIsExpanded();
	const double oldScrollOffset = _outlineScroll
		? _outlineScroll->VerticalOffset : 0.0;

	_syncingDocumentOutline = true;
	(void)_outlineTree->SelectItem(nullptr, false);
	_outlineTree->ClearItemControls();
	_outlineNodesByStableId.clear();
	_outlineWindowNode = nullptr;
	auto windowItem = std::make_unique<TreeViewItem>();
	windowItem->SetHeader(BindingValue(
		_canvas->GetDesignedWindowName() + L"  (Window)"));
	_outlineWindowNode = windowItem.get();
	_outlineWindowNode->Tag = BindingValue(OutlineWindowIdentity);

	struct OutlineRecord
	{
		std::shared_ptr<DesignerControl> Control;
		std::unique_ptr<TreeViewItem> OwnedItem;
		TreeViewItem* Item = nullptr;
		TreeViewItem* ParentItem = nullptr;
		int ChildOrder = 0;
	};
	std::vector<OutlineRecord> records;
	records.reserve(_canvas->GetAllControls().size());
	std::unordered_map<Control*, TreeViewItem*> itemsByRuntimeControl;
	for (const auto& control : _canvas->GetAllControls())
	{
		if (!control || !control->ControlInstance) continue;
		std::wstring typeName;
		if (!control->ComponentType.Empty())
			typeName = control->ComponentType.XamlName;
		else
		{
			const auto descriptor = std::find_if(
				_controlDescriptors.begin(), _controlDescriptors.end(),
				[&](const DesignerControlDescriptor& candidate)
				{
					return candidate.Type == control->Type;
				});
			if (descriptor != _controlDescriptors.end())
				typeName = descriptor->Name;
		}
		if (typeName.empty())
			typeName = control->Type == UIClass::UI_TabItem
				? L"TabItem" : L"Control";

		std::wstring flags;
		if (control->ControlInstance->Visibility != Visibility::Visible)
			flags += control->ControlInstance->Visibility == Visibility::Hidden
				? L"[Hidden]" : L"[Collapsed]";
		if (control->IsLocked) flags += L"[锁定]";
		std::wstring text = (flags.empty() ? L"" : flags + L" ")
			+ control->Name + L"  (" + typeName + L")";
		auto item = std::make_unique<TreeViewItem>();
		item->SetHeader(BindingValue(std::move(text)));
		item->Tag = BindingValue(control->StableId);
		auto* itemPointer = item.get();
		_outlineNodesByStableId[control->StableId] = itemPointer;
		itemsByRuntimeControl[control->ControlInstance] = itemPointer;
		records.push_back(OutlineRecord{
			control, std::move(item), itemPointer });
	}

	for (auto& record : records)
	{
		auto* parentControl = record.Control->DesignerParent;
		if (!parentControl && record.Control->ControlInstance)
			parentControl = record.Control->ControlInstance->GetVisualParent();
		auto* runtimeParent = record.Control->ControlInstance
			? record.Control->ControlInstance->GetVisualParent() : nullptr;
		TreeViewItem* parentItem = nullptr;
		while (parentControl && !parentItem)
		{
			const auto found = itemsByRuntimeControl.find(parentControl);
			if (found != itemsByRuntimeControl.end()) parentItem = found->second;
			else parentControl = parentControl->GetVisualParent();
		}
		record.ParentItem = !parentItem || parentItem == record.Item
			? _outlineWindowNode : parentItem;
		record.ChildOrder = runtimeParent
			? runtimeParent->IndexOfVisualChild(record.Control->ControlInstance)
			: 0;
		if (record.ChildOrder < 0) record.ChildOrder = 0;
	}
	std::unordered_map<TreeViewItem*, std::vector<OutlineRecord*>> childrenByParent;
	for (auto& record : records)
		childrenByParent[record.ParentItem].push_back(&record);
	for (auto& [parentItem, children] : childrenByParent)
	{
		std::stable_sort(children.begin(), children.end(),
			[](const OutlineRecord* left, const OutlineRecord* right)
			{
				if (left->ChildOrder != right->ChildOrder)
					return left->ChildOrder < right->ChildOrder;
				return left->Control->StableId < right->Control->StableId;
			});
		for (auto* child : children)
		{
			if (!child || !child->OwnedItem) continue;
			auto owned = std::move(child->OwnedItem);
			if (!parentItem
				|| !parentItem->AddItemControl(std::move(owned)))
				(void)_outlineWindowNode->AddItemControl(std::move(owned));
		}
	}
	for (auto& record : records)
		if (record.OwnedItem)
			(void)_outlineWindowNode->AddItemControl(
				std::move(record.OwnedItem));

	_outlineWindowNode->SetIsExpanded(formExpanded);
	for (const auto& record : records)
	{
		if (record.Item && record.Item->AuthoredItemCount() != 0)
			record.Item->SetIsExpanded(
				!hadOutline || expandedIds.contains(record.Control->StableId));
	}
	(void)_outlineTree->AddItemControl(std::move(windowItem));
	if (_outlineScroll)
		_outlineScroll->ScrollToVerticalOffset((std::max)(0.0, oldScrollOffset));
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
			if (Application::FindWindow(expectedHandle) != expectedDesigner)
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
	TreeViewItem* selectedItem = _outlineWindowNode;
	if (const auto selected = _canvas->GetSelectedControl())
	{
		const auto found = _outlineNodesByStableId.find(selected->StableId);
		if (found != _outlineNodesByStableId.end()) selectedItem = found->second;
	}
	(void)_outlineTree->SelectItem(selectedItem, false);

	std::function<bool(TreeViewItem*)> expandPath =
		[&](TreeViewItem* current) -> bool
		{
			if (!current) return false;
			if (current == selectedItem) return true;
			for (size_t index = 0;
				index < current->AuthoredItemCount(); ++index)
			{
				auto* child = dynamic_cast<TreeViewItem*>(
					current->GetAuthoredItem(index));
				if (!expandPath(child)) continue;
				current->SetIsExpanded(true);
				return true;
			}
			return false;
		};
	(void)expandPath(_outlineWindowNode);

	_outlineTree->UpdateLayout();
	if (_outlineScroll && selectedItem)
		(void)_outlineScroll->BringDescendantIntoView(selectedItem);
	_outlineTree->InvalidateVisual();
}

void Designer::OnDocumentOutlineSelectionChanged()
{
	if (_syncingDocumentOutline || !_outlineTree || !_canvas
		|| !_outlineTree->GetSelectedContainer()) return;
	_syncingDocumentOutline = true;
	int stableId = 0;
	if (!TryGetOutlineIdentity(
		_outlineTree->GetSelectedContainer(), stableId))
	{
		_syncingDocumentOutline = false;
		return;
	}
	if (stableId == OutlineWindowIdentity)
	{
		_canvas->RestoreSelectionByNames({}, {}, true);
	}
	else
	{
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
	this->SetKeyboardFocus(_showDocumentOutline
		? static_cast<Control*>(_outlineTree)
		: static_cast<Control*>(_canvas), true);
}

void Designer::BeginDocumentOutlineDrag(const MouseEventArgs& args)
{
	if (args.ChangedButton != MouseButton::Left || !_outlineTree || !_canvas
		|| !_showDocumentOutline)
		return;
	auto* item = _outlineTree->HitTestItem(
		static_cast<float>(args.X), static_cast<float>(args.Y));
	int stableId = 0;
	if (!TryGetOutlineIdentity(item, stableId)
		|| stableId == OutlineWindowIdentity) return;
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
	(void)_outlineTree->CaptureMouse();
}

void Designer::UpdateDocumentOutlineDrag(int localX, int localY)
{
	if (!_outlinePointerDown || !_outlineTree || !_canvas) return;
	const auto treeLocation = _outlineTree->GetActualLocationDip();
	const int treeX = static_cast<int>(std::lround(
		static_cast<float>(localX) - treeLocation.x));
	const int treeY = static_cast<int>(std::lround(
		static_cast<float>(localY) - treeLocation.y));
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

	if (_outlineScroll && treeY < _outlineScroll->VerticalOffset + 10)
		_outlineScroll->ScrollToVerticalOffset(_outlineScroll->VerticalOffset - 24.0);
	else if (_outlineScroll
		&& treeY > _outlineScroll->VerticalOffset
			+ _outlineScroll->ActualHeight - 10.0f)
		_outlineScroll->ScrollToVerticalOffset(_outlineScroll->VerticalOffset + 24.0);

	float rowPosition = 0.5f;
	auto* targetItem = _outlineTree->HitTestItem(
		static_cast<float>(treeX), static_cast<float>(treeY), &rowPosition);
	if (!targetItem)
	{
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::None;
		_outlineTree->ClearDropTarget();
		return;
	}
	int targetStableId = 0;
	if (!TryGetOutlineIdentity(targetItem, targetStableId))
	{
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::None;
		_outlineTree->ClearDropTarget();
		return;
	}
	if (targetStableId == OutlineWindowIdentity)
	{
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::Inside;
		_outlineTree->SetDropTarget(targetItem, _outlineDropPosition);
		return;
	}

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
		ancestor = ancestor->GetVisualParent())
	{
		if (ancestor != source->ControlInstance) continue;
		_outlineDropTargetStableId.reset();
		_outlineDropPosition = TreeViewDropPosition::None;
		_outlineTree->ClearDropTarget();
		return;
	}

	auto canContain = [](UIClass type)
	{
		if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type))
			return true;
		switch (type)
		{
		case UIClass::UI_Canvas:
		case UIClass::UI_GroupBox:
		case UIClass::UI_Expander:
		case UIClass::UI_ScrollViewer:
		case UIClass::UI_StackPanel:
		case UIClass::UI_Grid:
		case UIClass::UI_DockPanel:
		case UIClass::UI_WrapPanel:
		case UIClass::UI_RelativePanel:
		case UIClass::UI_TabControl:
		case UIClass::UI_TabItem:
			return true;
		default:
			return false;
		}
	};
	TreeViewDropPosition dropPosition = TreeViewDropPosition::Inside;
	if (target->Type == UIClass::UI_TabItem)
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
	_outlineTree->SetDropTarget(targetItem, dropPosition);
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
	this->SetKeyboardFocus(_showDocumentOutline
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
	if (releaseCapture && _outlineTree && _outlineTree->IsMouseCaptured())
		(void)_outlineTree->ReleaseMouseCapture();
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
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
			_lblInfo->AutomationFullDescription = _lblInfo->Text;
			_lblInfo->InvalidateVisual();
		}
		return;
	}
	this->SetKeyboardFocus(_canvas, true);
	(void)_canvas->AdoptVisualChildToCanvas(*descriptor, canvasPoint);
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
	if (releaseCapture)
		(void)this->ReleaseMouseCapture();
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
	if (_btnCopy) _btnCopy->IsEnabled = control != nullptr;
	if (_btnCut) _btnCut->IsEnabled = control != nullptr;
	if (_btnArrange) _btnArrange->IsEnabled = control != nullptr;
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
	for (const std::wstring_view commandName : {
		CanvasCut, CanvasCopy, CanvasDuplicate, CanvasDelete,
		CanvasToggleLock })
	{
		if (auto* item = FindDesignerCommand(_canvasMenu, commandName))
			item->IsEnabled = hasSelection;
	}
	if (auto* arrange = _canvasMenu->FindItemByText(L"排列", false))
		arrange->IsEnabled = hasSelection;
	if (auto* selectAll = FindDesignerCommand(_canvasMenu, CanvasSelectAll))
		selectAll->IsEnabled = !_canvas->GetAllControls().empty();
	const bool transactionActive = _canvas->HasActiveDocumentTransaction();
	if (auto* xaml = FindDesignerCommand(_canvasMenu, CanvasEditXaml))
		xaml->IsEnabled = !transactionActive;
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
		_lblZoom->AutomationFullDescription = args.FitToViewport
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
		_lblInfo->AutomationFullDescription = L"Tab 顺序模式：下一项 "
			+ std::to_wstring(args.NextIndex) + L"；可编排 "
			+ std::to_wstring(args.CandidateCount)
			+ L" 个控件，Escape 退出。";
	}
	else
	{
		_lblInfo->Text = L"已退出 Tab 顺序模式。";
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
		cui::framework::NativeVisualStateAccess::Set(
			*_btnTabOrder, ControlStyleState::Checked, active);
		_btnTabOrder->SetContent(BindingValue(active
			? L"Tab " + std::to_wstring(nextIndex)
			: L"Tab 顺序"));
		_btnTabOrder->AutomationFullDescription = active
			? L"Tab 顺序模式已开启；下一项为 "
				+ std::to_wstring(nextIndex)
				+ L"。单击控件编号，Escape 退出。"
			: L"显示可接收键盘焦点控件的 TabIndex；进入后依次单击控件编号。";
		_btnTabOrder->InvalidateVisual();
	}
	if (_canvasMenu)
	{
		if (auto* item = FindDesignerCommand(_canvasMenu, CanvasToggleTabOrder))
			item->IsChecked = active;
		if (auto* item = FindDesignerCommand(_canvasMenu, CanvasAutoTabOrder))
			item->IsEnabled = candidateCount > 0
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
		if (auto* item = FindDesignerCommand(menu, CanvasToggleGrid))
			item->IsChecked = _canvas->IsGridVisible();
		if (auto* item = FindDesignerCommand(menu, CanvasToggleSnapGrid))
			item->IsChecked = _canvas->IsSnapToGridEnabled();
		if (auto* item = FindDesignerCommand(menu, CanvasToggleSnapGuides))
			item->IsChecked = _canvas->IsSnapToGuidesEnabled();
		for (const auto [commandName, size] : {
			std::pair{ std::wstring_view(CanvasGridSize5), 5 },
			std::pair{ std::wstring_view(CanvasGridSize10), 10 },
			std::pair{ std::wstring_view(CanvasGridSize20), 20 } })
		{
			if (auto* item = FindDesignerCommand(menu, commandName))
				item->IsChecked = _canvas->GetGridSize() == size;
		}
		menu->InvalidateVisual();
	};
	refreshMenu(_gridMenu);
	refreshMenu(_canvasMenu);
	if (_btnGridSettings)
	{
		_btnGridSettings->SetContent(BindingValue(
			L"网格 " + std::to_wstring(_canvas->GetGridSize())));
		cui::framework::NativeVisualStateAccess::Set(
			*_btnGridSettings, ControlStyleState::Checked,
			_canvas->IsGridVisible());
		_btnGridSettings->AutomationFullDescription =
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
		_btnUndo->IsEnabled = canUndo;
		_btnUndo->AutomationFullDescription = canUndo
			? L"撤销“" + undoLabel + L"”。快捷键 Ctrl+Z。"
			: L"没有可撤销的操作。快捷键 Ctrl+Z。";
		_btnUndo->InvalidateVisual();
	}
	if (_btnRedo)
	{
		_btnRedo->IsEnabled = canRedo;
		_btnRedo->AutomationFullDescription = canRedo
			? L"重做“" + redoLabel + L"”。快捷键 Ctrl+Y。"
			: L"没有可重做的操作。快捷键 Ctrl+Y。";
		_btnRedo->InvalidateVisual();
	}
	if (_btnPaste)
	{
		_btnPaste->IsEnabled = canPaste;
		_btnPaste->AutomationName = L"粘贴";
		_btnPaste->AutomationFullDescription = transactionActive
			? L"画布事务进行中，暂时不能粘贴。快捷键 Ctrl+V。"
			: clipboardHasText
				? L"从剪贴板粘贴 CUI XAML；外部文本会在粘贴时验证。快捷键 Ctrl+V。"
				: L"剪贴板中没有可粘贴的文本。快捷键 Ctrl+V。";
		_btnPaste->InvalidateVisual();
	}
	if (_canvasMenu)
	{
		if (auto* undo = FindDesignerCommand(_canvasMenu, CanvasUndo))
		{
			undo->IsEnabled = canUndo;
			undo->SetHeader(BindingValue(
				canUndo ? L"撤销 " + undoLabel : L"撤销"));
		}
		if (auto* redo = FindDesignerCommand(_canvasMenu, CanvasRedo))
		{
			redo->IsEnabled = canRedo;
			redo->SetHeader(BindingValue(
				canRedo ? L"重做 " + redoLabel : L"重做"));
		}
		for (const std::wstring_view commandName : {
			CanvasPaste, CanvasPasteInPlace, CanvasPasteHere })
		{
			if (auto* paste = FindDesignerCommand(
				_canvasMenu, commandName))
				paste->IsEnabled = canPaste
					&& (commandName != CanvasPasteHere
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
		if (auto* item = FindDesignerCommand(menu, CanvasToggleLock))
		{
			item->IsEnabled = hasSelection;
			item->IsChecked = allLocked;
			item->SetHeader(BindingValue(
				allLocked ? L"解除锁定" : L"锁定控件"));
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
	_lblInfo->AutomationFullDescription = text;
	_lblInfo->InvalidateVisual();
}

bool Designer::OnPreviewInputReport(const InputReport& input)
{
	const Key key = input.Key;
	const bool keyDown = input.Kind == InputReportKind::KeyDown;
	const bool leftButtonUp = input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left;
	if (_toolBoxPointerDown)
	{
		if (input.Kind == InputReportKind::PointerMove)
		{
			UpdateToolBoxDrag(input.X, input.Y);
			if (_toolBoxDragging) return true;
		}
		if (leftButtonUp)
		{
			if (_toolBoxDragging)
			{
				EndToolBoxDrag(input.X, input.Y);
				return true;
			}
			// Let Window deliver a normal mouse-up/click to the toolbox item.
			// Its click handler preserves the existing click-then-place workflow.
			CancelToolBoxDrag(false);
		}
		if (keyDown && key == Key::Escape)
		{
			const bool wasDragging = _toolBoxDragging;
			CancelToolBoxDrag();
			if (wasDragging && _lblInfo)
			{
				_lblInfo->Text = L"已取消工具箱拖放。";
				_lblInfo->AutomationFullDescription = _lblInfo->Text;
				_lblInfo->InvalidateVisual();
			}
			return true;
		}
	}
	if (_outlinePointerDown)
	{
		if (input.Kind == InputReportKind::PointerMove)
		{
			UpdateDocumentOutlineDrag(input.X, input.Y);
			return true;
		}
		if (leftButtonUp)
		{
			UpdateDocumentOutlineDrag(input.X, input.Y);
			EndDocumentOutlineDrag();
			return true;
		}
		if (keyDown && key == Key::Escape)
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
		&& (IsControlWithin(this->GetKeyboardFocusedElement(), _outlineTree)
			|| IsControlWithin(this->GetKeyboardFocusedElement(), _canvas));
	if (keyDown
		&& outlineShortcutTarget
		&& !input.HasModifier(ModifierKeys::Alt)
		&& QueueOutlineShortcut(key, input.HasModifier(ModifierKeys::Control), input.HasModifier(ModifierKeys::Shift)))
	{
		if (input.HasModifier(ModifierKeys::Control))
			cui::framework::WindowAccess::TextComposition(
				*this).SuppressNextCharacter(
				OutlineShortcutControlCharacter(key));
		return true;
	}
	if (keyDown && input.HasModifier(ModifierKeys::Control) && !input.HasModifier(ModifierKeys::Alt))
	{
		if (key == Key::S)
		{
			OnSaveClick();
			return true;
		}
		if (key == Key::N)
		{
			OnNewClick();
			return true;
		}
		if (key == Key::O)
		{
			OnOpenClick();
			return true;
		}
	}
	const bool interactionCanceled = input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost
		|| input.Kind == InputReportKind::FocusLost;
	if (interactionCanceled && _outlinePointerDown)
		CancelDocumentOutlineDrag(false);
	if (interactionCanceled && _toolBoxPointerDown)
		CancelToolBoxDrag(false);
	if (interactionCanceled && _canvas)
	{
		(void)_canvas->CancelActivePointerInteraction(
			input.Kind == InputReportKind::CaptureLost
				? L"画布失去鼠标捕获，修改已回滚。"
				: L"窗口交互被中断，画布修改已回滚。");
	}
	return false;
}

std::optional<LRESULT> Designer::OnPlatformMessage(
	UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CLIPBOARDUPDATE)
	{
		_clipboardRefreshRetriesRemaining = ClipboardRefreshRetryCount;
		RefreshCommandAvailability();
		if (Handle)
			(void)::SetTimer(Handle, ClipboardRefreshTimerId,
				ClipboardRefreshDelayMilliseconds, nullptr);
		return LRESULT{ 0 };
	}
	if (message == WM_DESTROY)
		StopClipboardMonitoring();
	if (message == WM_CLOSE && !_closeApproved)
	{
		if (!ConfirmCanReplaceOrCloseDocument()) return LRESULT{ 0 };
		_closeApproved = true;
	}
	if (message == WM_TIMER && wParam == RecoveryTimerId)
	{
		(void)::KillTimer(this->Handle, RecoveryTimerId);
		if (!_recoverySnapshotPending) return LRESULT{ 0 };
		std::wstring recoveryError;
		if (!FlushRecoverySnapshot(&recoveryError)
			&& _lblInfo && !recoveryError.empty())
		{
			_lblInfo->Text = L"自动恢复保存失败：" + recoveryError;
			_lblInfo->AutomationFullDescription = _lblInfo->Text;
			_lblInfo->InvalidateVisual();
			if (_canvas && _canvas->IsDocumentDirty())
			{
				_recoverySnapshotPending = true;
				(void)::SetTimer(this->Handle, RecoveryTimerId,
					RecoveryRetryMilliseconds, nullptr);
			}
		}
		return LRESULT{ 0 };
	}
	if (message == WM_TIMER && wParam == CodeFreshnessTimerId)
	{
		(void)::KillTimer(this->Handle, CodeFreshnessTimerId);
		if (!_codeFreshnessInspectionPending) return LRESULT{ 0 };
		_codeFreshnessInspectionPending = false;
		RefreshCodeFreshnessFromFiles();
		UpdateDocumentPresentation();
		return LRESULT{ 0 };
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
		return LRESULT{ 0 };
	}
	if (message == WM_ACTIVATEAPP && wParam == TRUE)
	{
		RefreshCodeFreshnessFromFiles();
		UpdateDocumentPresentation();
	}
	const bool applicationDeactivated = message == WM_ACTIVATEAPP
		&& wParam == FALSE;
	if (applicationDeactivated && _outlinePointerDown)
		CancelDocumentOutlineDrag(false);
	if (applicationDeactivated && _toolBoxPointerDown)
		CancelToolBoxDrag(false);
	if (applicationDeactivated && _canvas)
	{
		(void)_canvas->CancelActivePointerInteraction(
			L"应用失去激活状态，画布修改已回滚。");
	}
	(void)lParam;
	return std::nullopt;
}

bool Designer::QueueOutlineShortcut(
	Key key,
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
	Key key,
	bool controlDown,
	bool shiftDown)
{
	if (!_canvas) return false;
	if (!controlDown)
	{
		if (key != Key::Delete) return false;
		OnDeleteClick();
		return true;
	}

	switch (key)
	{
	case Key::C:
		OnCopyClick();
		return true;
	case Key::X:
		OnCutClick();
		return true;
	case Key::V:
		if (shiftDown)
			(void)_canvas->PasteControlsFromClipboardInPlace();
		else OnPasteClick();
		return true;
	case Key::D:
		(void)_canvas->DuplicateSelectedControls();
		return true;
	case Key::L:
		ToggleSelectedControlsLocked();
		return true;
	case Key::Z:
		if (shiftDown) OnRedoClick();
		else OnUndoClick();
		return true;
	case Key::Y:
		OnRedoClick();
		return true;
	case Key::A:
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
		if (r != DialogResult::OK)
			return false;
		path = Convert::StringToWString(sfd.SelectedPath);
		if (path.empty()) return false;
		if (!DesignerModel::HasDesignDocumentExtension(path))
			path += L".cui.xaml";
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
	this->Title = std::move(title);
	if (_btnReload) _btnReload->IsEnabled = !_currentFileName.empty();
	if (_btnRegenerate)
	{
		const bool available = !_lastExportBasePath.empty()
			&& _canvas && !_canvas->GetCodeBehind().ClassName.empty();
		_btnRegenerate->IsEnabled = available;
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
		_btnRegenerate->SetContent(BindingValue(std::move(caption)));
		_btnRegenerate->AutomationName = L"重新生成代码";
		_btnRegenerate->AutomationFullDescription = std::move(description);
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
			_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
	DesignerModel::DesignDocument exportDocument;
	std::wstring exportSnapshotError;
	if (!_canvas || !_canvas->BuildDesignDocument(
		exportDocument, &exportSnapshotError))
	{
		if (_lblInfo) _lblInfo->Text = L"导出失败：无法构建 XAML 文档。";
		ShowModalMessage(this, L"错误", exportSnapshotError.empty()
			? L"无法从当前 XAML 文档构建代码生成输入。"
			: exportSnapshotError);
		return;
	}
	const auto exportCount = static_cast<int>(exportDocument.Nodes.size());
	const auto buttonCount = static_cast<int>(std::count_if(
		exportDocument.Nodes.begin(), exportDocument.Nodes.end(),
		[](const auto& node) { return node.Type == UIClass::UI_Button; }));
	const auto gridCount = static_cast<int>(std::count_if(
		exportDocument.Nodes.begin(), exportDocument.Nodes.end(),
		[](const auto& node) { return node.Type == UIClass::UI_Grid; }));
	
	SaveFileDialog saveFileDialog;
	saveFileDialog.Filter = std::string("C++ Files (*.h;*.cpp)\0*.h;*.cpp\0\0\0",35);
	::DialogResult dialogResult = saveFileDialog.ShowDialog(this->Handle);

	if (dialogResult == ::DialogResult::OK)
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
		exportDialog.Owner = this;
		(void)exportDialog.ShowDialog();
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
				+ L", Grid:" + std::to_wstring(gridCount)
				+ L", Button:" + std::to_wstring(buttonCount) + L")";
			ShowModalMessage(this, L"导出成功", (L"代码已成功导出到:\n"
				+ headerPath + L"\n" + cppPath + L"\n"
				+ generatedHeaderPath + L"\n" + generatedCppPath + L"\n"
				+ handlerIncludePath
				+ L"\n\n.h/.cpp 仅首次创建，.g.* 可安全重新生成。"
				+ L"\n\n导出统计：控件=" + std::to_wstring(exportCount)
				+ L"，Grid=" + std::to_wstring(gridCount)
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
			_lblInfo->AutomationFullDescription = _lblInfo->Text;
			_lblInfo->InvalidateVisual();
		}
		return;
	}

	std::wstring error;
	if (!GenerateCodeFiles(_lastExportBasePath, &error))
	{
		_lblInfo->Text = L"代码重新生成失败："
			+ (error.empty() ? L"未知错误。" : error);
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
		_lblInfo->InvalidateVisual();
		return;
	}

	UpdateDocumentPresentation();
	_lblInfo->Text = L"代码已重新生成：" + _lastExportBasePath;
	_lblInfo->AutomationFullDescription = _lblInfo->Text;
	_lblInfo->InvalidateVisual();
}

void Designer::OnEventHandlerActivated(const std::wstring& handlerName)
{
	if (handlerName.empty() || !_lblInfo) return;
	if (_lastExportBasePath.empty())
	{
		_lblInfo->Text = L"处理函数已就绪: " + handlerName
			+ L"。首次“导出代码”后，再次激活会更新并打开用户源文件。";
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
		_lblInfo->AutomationFullDescription = _lblInfo->Text;
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
	_lblInfo->AutomationFullDescription = _lblInfo->Text;
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

	XamlEditorDialog dialog(_canvas, std::move(xaml));
	dialog.Owner = this;
	(void)dialog.ShowDialog();
	auto result = !_canvas->HasActiveDocumentTransaction()
		? DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged)
		: dialog.Applied
			? _canvas->CommitDocumentEditTransaction()
			: _canvas->RollbackDocumentEditTransaction();
	const std::wstring completionMessage = dialog.Applied
		? L"XAML 编辑已提交。"
		: L"XAML 编辑已取消并恢复画布。";
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
	if (!_arrangeMenu || !_btnArrange || !_btnArrange->IsEnabled) return;
	RefreshLockPresentation();
	const float buttonHeight = _btnArrange->ActualHeight > 0.0f
		? _btnArrange->ActualHeight
		: (_btnArrange->Height.IsFixed() ? _btnArrange->Height.value : 0.0f);
	_arrangeMenu->ShowAt(_btnArrange, 0,
		static_cast<int>(std::lround(buttonHeight)) + 4);
}

void Designer::OnArrangeCommand(std::wstring_view commandName)
{
	if (!_canvas) return;
	if (commandName == ArrangeDuplicate)
	{
		(void)_canvas->DuplicateSelectedControls();
		return;
	}
	if (commandName == CanvasToggleLock)
	{
		ToggleSelectedControlsLocked();
		return;
	}
	std::optional<DesignerSelectionArrangeAction> action;
	if (commandName == ArrangeAlignLeft)
		action = DesignerSelectionArrangeAction::AlignLeft;
	else if (commandName == ArrangeAlignHorizontalCenters)
		action = DesignerSelectionArrangeAction::AlignHorizontalCenters;
	else if (commandName == ArrangeAlignRight)
		action = DesignerSelectionArrangeAction::AlignRight;
	else if (commandName == ArrangeAlignTop)
		action = DesignerSelectionArrangeAction::AlignTop;
	else if (commandName == ArrangeAlignVerticalCenters)
		action = DesignerSelectionArrangeAction::AlignVerticalCenters;
	else if (commandName == ArrangeAlignBottom)
		action = DesignerSelectionArrangeAction::AlignBottom;
	else if (commandName == ArrangeDistributeHorizontally)
		action = DesignerSelectionArrangeAction::DistributeHorizontally;
	else if (commandName == ArrangeDistributeVertically)
		action = DesignerSelectionArrangeAction::DistributeVertically;
	else if (commandName == ArrangeMakeSameWidth)
		action = DesignerSelectionArrangeAction::MakeSameWidth;
	else if (commandName == ArrangeMakeSameHeight)
		action = DesignerSelectionArrangeAction::MakeSameHeight;
	else if (commandName == ArrangeMakeSameSize)
		action = DesignerSelectionArrangeAction::MakeSameSize;
	else if (commandName == ArrangeBringForward)
		action = DesignerSelectionArrangeAction::BringForward;
	else if (commandName == ArrangeSendBackward)
		action = DesignerSelectionArrangeAction::SendBackward;
	else if (commandName == ArrangeBringToFront)
		action = DesignerSelectionArrangeAction::BringToFront;
	else if (commandName == ArrangeSendToBack)
		action = DesignerSelectionArrangeAction::SendToBack;
	if (action) (void)_canvas->ArrangeSelection(*action);
}

void Designer::OnCanvasMenuCommand(std::wstring_view commandName)
{
	if (!_canvas) return;
	if (commandName == CanvasUndo)
	{
		OnUndoClick();
		return;
	}
	if (commandName == CanvasRedo)
	{
		OnRedoClick();
		return;
	}
	if (commandName == CanvasCut)
	{
		OnCutClick();
		return;
	}
	if (commandName == CanvasCopy)
	{
		OnCopyClick();
		return;
	}
	if (commandName == CanvasPaste)
	{
		OnPasteClick();
		return;
	}
	if (commandName == CanvasPasteInPlace)
	{
		(void)_canvas->PasteControlsFromClipboardInPlace();
		return;
	}
	if (commandName == CanvasPasteHere)
	{
		if (_hasCanvasContextPastePoint)
			(void)_canvas->PasteControlsFromClipboardAt(
				_canvasContextPastePoint);
		return;
	}
	if (commandName == CanvasDuplicate)
	{
		(void)_canvas->DuplicateSelectedControls();
		return;
	}
	if (commandName == CanvasDelete)
	{
		OnDeleteClick();
		return;
	}
	if (commandName == CanvasToggleLock)
	{
		ToggleSelectedControlsLocked();
		return;
	}
	if (commandName == CanvasSelectAll)
	{
		(void)_canvas->SelectAllInCurrentContainer(true);
		return;
	}
	if (commandName == CanvasEditXaml)
	{
		OnXamlClick();
		return;
	}
	if (commandName == CanvasViewFit)
	{
		_canvas->FitDesignSurfaceToViewport();
		return;
	}
	if (commandName == CanvasViewActualSize)
	{
		_canvas->ResetView();
		return;
	}
	if (commandName == CanvasViewZoomIn)
	{
		_canvas->ZoomIn();
		return;
	}
	if (commandName == CanvasViewZoomOut)
	{
		_canvas->ZoomOut();
		return;
	}
	if (commandName == CanvasToggleTabOrder)
	{
		ToggleTabOrderMode();
		return;
	}
	if (commandName == CanvasAutoTabOrder)
	{
		(void)_canvas->AutoArrangeTabOrder();
		RefreshTabOrderPresentation();
		return;
	}
	if (commandName == CanvasToggleGrid)
	{
		_canvas->SetGridVisible(!_canvas->IsGridVisible());
		RefreshGridSettingsPresentation();
	}
	else if (commandName == CanvasToggleSnapGrid)
	{
		_canvas->SetSnapToGridEnabled(
			!_canvas->IsSnapToGridEnabled());
		RefreshGridSettingsPresentation();
	}
	else if (commandName == CanvasToggleSnapGuides)
	{
		_canvas->SetSnapToGuidesEnabled(
			!_canvas->IsSnapToGuidesEnabled());
		RefreshGridSettingsPresentation();
	}
	else if (commandName == CanvasGridSize5)
	{
		_canvas->SetGridSize(5);
		RefreshGridSettingsPresentation();
	}
	else if (commandName == CanvasGridSize10)
	{
		_canvas->SetGridSize(10);
		RefreshGridSettingsPresentation();
	}
	else if (commandName == CanvasGridSize20)
	{
		_canvas->SetGridSize(20);
		RefreshGridSettingsPresentation();
	}
	else
	{
		OnArrangeCommand(commandName);
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
