#include "Designer.h"
#include <Windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <map>

namespace
{
	static std::string MakeDesignFilter()
	{
		std::string s;
		s.append("JSON Files (*.json)");
		s.push_back('\0');
		s.append("*.json");
		s.push_back('\0');
		s.append("CUI Designer Files (*.cui.json)");
		s.push_back('\0');
		s.append("*.cui.json");
		s.push_back('\0');
		s.push_back('\0');
		return s;
	}


	static void ShowModalMessage(Form* ownerForm, const std::wstring& caption, const std::wstring& text)
	{
		::MessageBoxW(ownerForm->Handle, text.c_str(), caption.c_str(), MB_OK | MB_SETFOREGROUND);
	}
}

Designer::Designer() : Form(L"CUI 窗口设计器", { 0,0 }, { 1400, 840 })
{
	this->BackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
}

Designer::~Designer()
{
}

void Designer::InitAndShow()
{
	InitializeComponents();
	this->Show();
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
	
	_btnExport = new Button(L"导出代码", btnX, btnY, btnWidth + 20, btnHeight);
	_btnExport->Round = 0.5f;
	_btnExport->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnExportClick();
	};
	this->AddControl(_btnExport);
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
	int formHeight = this->Size.cy;
	_toolBox = new ToolBox(10, toolbarHeight + 10, toolBoxWidth, formHeight - toolbarHeight - 40);
	_toolBox->OnControlSelected += [this](UIClass type) {
		OnToolBoxControlSelected(type);
	};
	this->AddControl(_toolBox);
	
	// 属性面板（右侧）
	int propertyGridWidth = 250;
	int formWidth = this->Size.cx;
	_propertyGrid = new PropertyGrid(formWidth - propertyGridWidth - 10, toolbarHeight + 10, 
		propertyGridWidth, formHeight - toolbarHeight - 40);
	this->AddControl(_propertyGrid);
	
	// 设计画布（中间）
	int canvasX = toolBoxWidth + 20;
	int canvasWidth = formWidth - toolBoxWidth - propertyGridWidth - 40;
	_canvas = new DesignerCanvas(canvasX, toolbarHeight + 10, canvasWidth, formHeight - toolbarHeight - 40);
	_canvas->OnControlSelected += [this](std::shared_ptr<DesignerControl> control) {
		OnCanvasControlSelected(control);
	};
	this->AddControl(_canvas);

	// 让 PropertyGrid 能在“编辑页/按钮”时同步更新 DesignerCanvas 的设计器模型
	_propertyGrid->SetDesignerCanvas(_canvas);

	// 窗口大小变化时：自动调整内部控件布局
	auto doLayout = [this, toolbarHeight, toolBoxWidth, propertyGridWidth]() {
		int w = this->Size.cx;
		int h = this->Size.cy;
		int usableH = h - toolbarHeight - 40;
		if (usableH < 50) usableH = 50;
		if (_toolBox)
		{
			_toolBox->Location = { 10, toolbarHeight + 10 };
			_toolBox->Size = { toolBoxWidth, usableH };
		}
		if (_propertyGrid)
		{
			_propertyGrid->Location = { w - propertyGridWidth - 10, toolbarHeight + 10 };
			_propertyGrid->Size = { propertyGridWidth, usableH };
			// 重新加载以适配宽度变化
			_propertyGrid->LoadControl(_canvas ? _canvas->GetSelectedControl() : nullptr);
		}
		if (_canvas)
		{
			int canvasX = toolBoxWidth + 20;
			int canvasW = w - toolBoxWidth - propertyGridWidth - 40;
			if (canvasW < 100) canvasW = 100;
			_canvas->Location = { canvasX, toolbarHeight + 10 };
			_canvas->Size = { canvasW, usableH };
			_canvas->PostRender();
		}
	};

	this->OnSizeChanged += [doLayout](Form*) { doLayout(); };
	doLayout();
}

void Designer::OnToolBoxControlSelected(UIClass type)
{
	_canvas->SetControlToAdd(type);
	_lblInfo->Text = L"请在画布上点击以添加控件";
}

void Designer::OnCanvasControlSelected(std::shared_ptr<DesignerControl> control)
{
	_propertyGrid->LoadControl(control);
	
	if (control)
	{
		_lblInfo->Text = L"已选中: " + control->Name;
	}
	else
	{
		_lblInfo->Text = L"就绪";
	}
}

void Designer::OnNewClick()
{
	_canvas->ClearCanvas();
	_propertyGrid->Clear();
	_currentFileName.clear();
	_lblInfo->Text = L"画布已清空";
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

	std::wstring path = Convert::string_to_wstring(ofd.SelectedPaths[0]);
	std::wstring err;
	if (_canvas->LoadDesignFile(path, &err))
	{
		_currentFileName = path;
		_propertyGrid->LoadControl(nullptr);
		_lblInfo->Text = L"已打开: " + path;
	}
	else
	{
		ShowModalMessage(this, L"打开失败", err.empty() ? L"无法加载设计文件。" : err);
	}
}

void Designer::OnSaveClick()
{
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
			return;
		path = Convert::string_to_wstring(sfd.SelectedPath);
		if (path.empty()) return;
	}

	std::wstring err;
	if (_canvas->SaveDesignFile(path, &err))
	{
		_currentFileName = path;
		_lblInfo->Text = L"已保存: " + path;
	}
	else
	{
		ShowModalMessage(this, L"保存失败", err.empty() ? L"无法保存设计文件。" : err);
	}
}

void Designer::OnExportClick()
{
	auto controls = _canvas->GetAllControlsForExport();
	if (controls.empty())
	{
		ShowModalMessage(this, L"提示", L"画布上没有控件，无法导出！");
		return;
	}

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
		std::wstring selectedPath = Convert::string_to_wstring(saveFileDialog.SelectedPath);
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
		
		// 生成代码（包含窗体标题/尺寸）
		CodeGenerator generator(fileName, controls,
			_canvas ? _canvas->GetDesignedFormText() : L"",
			_canvas ? _canvas->GetDesignedFormSize() : SIZE{ 800, 600 },
			_canvas ? _canvas->GetDesignedFormLocation() : POINT{ 100, 100 },
			_canvas ? _canvas->GetDesignedFormName() : L"MainForm",
			_canvas ? _canvas->GetDesignedFormBackColor() : Colors::WhiteSmoke,
			_canvas ? _canvas->GetDesignedFormForeColor() : Colors::Black,
			_canvas ? _canvas->GetDesignedFormShowInTaskBar() : true,
			_canvas ? _canvas->GetDesignedFormTopMost() : false,
			_canvas ? _canvas->GetDesignedFormEnable() : true,
			_canvas ? _canvas->GetDesignedFormVisible() : true,
			_canvas ? _canvas->GetDesignedFormEventHandlers() : std::map<std::wstring, std::wstring>{},
			_canvas ? _canvas->GetDesignedFormVisibleHead() : true,
			_canvas ? _canvas->GetDesignedFormHeadHeight() : 24,
			_canvas ? _canvas->GetDesignedFormMinBox() : true,
			_canvas ? _canvas->GetDesignedFormMaxBox() : true,
			_canvas ? _canvas->GetDesignedFormCloseBox() : true,
			_canvas ? _canvas->GetDesignedFormCenterTitle() : true,
			_canvas ? _canvas->GetDesignedFormAllowResize() : true,
			_canvas ? _canvas->GetDesignedFormFontName() : L"",
			_canvas ? _canvas->GetDesignedFormFontSize() : GetDefaultFontObject()->FontSize);
		
		if (generator.GenerateFiles(headerPath, cppPath))
		{
			_lblInfo->Text = L"代码导出成功: " + fileName + L" (控件:" + std::to_wstring(exportCount)
				+ L", GridPanel:" + std::to_wstring(gridPanelCount)
				+ L", Button:" + std::to_wstring(buttonCount) + L")";
			ShowModalMessage(this, L"导出成功", (L"代码已成功导出到:\n" + headerPath + L"\n" + cppPath
				+ L"\n\n导出统计：控件=" + std::to_wstring(exportCount)
				+ L"，GridPanel=" + std::to_wstring(gridPanelCount)
				+ L"，Button=" + std::to_wstring(buttonCount)));
		}
		else
		{
			_lblInfo->Text = L"导出失败";
			ShowModalMessage(this, L"错误", L"导出失败，请检查文件路径！");
		}
	}
}

void Designer::OnDeleteClick()
{
	_canvas->DeleteSelectedControl();
	_lblInfo->Text = L"控件已删除";
}