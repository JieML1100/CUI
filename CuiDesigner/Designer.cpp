#include "Designer.h"
#include <Windows.h>
#include <commdlg.h>

namespace
{
	class SimpleModalMessageForm : public Form
	{
	public:
		SimpleModalMessageForm(const std::wstring& caption, const std::wstring& text)
			: Form(caption, { 0, 0 }, { 520, 240 })
		{
			this->BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);

			auto label = new Label(text, 20, 20);
			label->Size = { 480, 140 };
			label->Font = new ::Font(L"Microsoft YaHei", 14.0f);
			this->AddControl(label);

			auto ok = new Button(L"确定", 210, 175, 100, 35);
			ok->Round = 4.0f;
			ok->OnMouseClick += [this](Control*, MouseEventArgs) {
				this->Close();
			};
			this->AddControl(ok);
		}
	};

	static void ShowModalMessage(Form* ownerForm, const std::wstring& caption, const std::wstring& text)
	{
		SimpleModalMessageForm dlg(caption, text);
		dlg.ShowDialog(ownerForm ? ownerForm->Handle : NULL);
	}
}

Designer::Designer() : Form(L"CUI 窗口设计器", { 0,0 }, { 1400, 840 })
{
	// 设置窗体属性
	this->BackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
	
	// 注意：不在构造函数中调用InitializeComponents
	// 需要在Show之前调用，确保所有控件都正确初始化
}

Designer::~Designer()
{
}

void Designer::InitAndShow()
{
	// 先初始化所有控件
	InitializeComponents();
	// 确保所有控件都添加完成后再显示窗口
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
	_btnNew->Round = 4.0f;
	_btnNew->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnNewClick();
	};
	this->AddControl(_btnNew);
	btnX += btnWidth + 10;
	
	_btnExport = new Button(L"导出代码", btnX, btnY, btnWidth + 20, btnHeight);
	_btnExport->Round = 4.0f;
	_btnExport->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
		OnExportClick();
	};
	this->AddControl(_btnExport);
	btnX += btnWidth + 30;
	
	_btnDelete = new Button(L"删除", btnX, btnY, btnWidth, btnHeight);
	_btnDelete->Round = 4.0f;
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
	_lblInfo->Text = L"画布已清空";
}

void Designer::OnExportClick()
{
	auto controls = _canvas->GetAllControls();
	if (controls.empty())
	{
		ShowModalMessage(this, L"提示", L"画布上没有控件，无法导出！");
		return;
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
		std::wstring headerPath = Convert::string_to_wstring(saveFileDialog.SelectedPath);
		std::wstring cppPath = headerPath;
		
		// 替换扩展名
		size_t pos = cppPath.find_last_of(L'.');
		if (pos != std::wstring::npos)
		{
			cppPath = cppPath.substr(0, pos) + L".cpp";
		}
		
		// 从文件名提取类名
		std::wstring fileName = headerPath;
		size_t lastSlash = fileName.find_last_of(L"\\/");
		if (lastSlash != std::wstring::npos)
		{
			fileName = fileName.substr(lastSlash + 1);
		}
		pos = fileName.find_last_of(L'.');
		if (pos != std::wstring::npos)
		{
			fileName = fileName.substr(0, pos);
		}
		
		// 生成代码
		CodeGenerator generator(fileName, controls);
		
		if (generator.GenerateFiles(headerPath, cppPath))
		{
			_lblInfo->Text = L"代码导出成功: " + fileName;
			ShowModalMessage(this, L"导出成功", (L"代码已成功导出到:\n" + headerPath + L"\n" + cppPath));
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
