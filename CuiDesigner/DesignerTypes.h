#pragma once

/**
 * @file DesignerTypes.h
 * @brief 设计器数据模型与辅助类型定义（控件树、属性、布局等）。
 */
#include "../CUI/GUI/Control.h"
#include <string>
#include <map>
#include <unordered_map>

// 设计器中控件的元数据
struct ControlMetadata
{
	UIClass Type;
	std::wstring Name;
	std::wstring DisplayName;
	SIZE DefaultSize;
	bool IsContainer;
};

// 设计器中的控件包装类
class DesignerControl
{
public:
	Control* ControlInstance;
	// 设计器层面的父容器：nullptr 表示直接属于窗体（画布根级）。
	// 注意：不要与 ControlInstance->Parent 混淆；后者在设计器运行时可能指向 DesignerCanvas。
	Control* DesignerParent = nullptr;
	std::wstring Name;
	UIClass Type;
	bool IsSelected;

	// 事件绑定：key 为事件成员名（如 OnMouseClick/OnTextChanged），value 为类成员函数名
	// 仅用于设计期保存/加载与导出代码生成；运行时不自动绑定。
	std::map<std::wstring, std::wstring> EventHandlers;

	// 设计期附加数据（不一定映射到运行时属性）。
	// 例如：MediaPlayer 的媒体源路径等。
	std::unordered_map<std::wstring, std::wstring> DesignStrings;
	
	// 用于调整大小的句柄位置
	enum class ResizeHandle
	{
		None,
		TopLeft,
		Top,
		TopRight,
		Right,
		BottomRight,
		Bottom,
		BottomLeft,
		Left
	};
	
	DesignerControl(Control* control, std::wstring name, UIClass type, Control* designerParent = nullptr)
		: ControlInstance(control), DesignerParent(designerParent), Name(name), Type(type), IsSelected(false)
	{
	}
	
	ResizeHandle HitTestHandle(POINT pt, int handleSize = 6);
	std::vector<RECT> GetHandleRects(int handleSize = 6);
};

// 可用控件类型列表
class ControlRegistry
{
public:
	static std::vector<ControlMetadata> GetAvailableControls()
	{
		return {
			{ UIClass::UI_Label, L"Label", L"标签", {100, 20}, false },
			{ UIClass::UI_Button, L"Button", L"按钮", {120, 30}, false },
			{ UIClass::UI_TextBox, L"TextBox", L"文本框", {200, 25}, false },
			{ UIClass::UI_PasswordBox, L"PasswordBox", L"密码框", {200, 25}, false },
			{ UIClass::UI_RichTextBox, L"RichTextBox", L"富文本框", {300, 160}, false },
			{ UIClass::UI_Panel, L"Panel", L"面板", {200, 200}, true },
			{ UIClass::UI_StackPanel, L"StackPanel", L"堆叠面板", {200, 200}, true },
			{ UIClass::UI_GridPanel, L"GridPanel", L"网格面板", {200, 200}, true },
			{ UIClass::UI_DockPanel, L"DockPanel", L"停靠面板", {200, 200}, true },
			{ UIClass::UI_WrapPanel, L"WrapPanel", L"换行面板", {200, 200}, true },
			{ UIClass::UI_RelativePanel, L"RelativePanel", L"相对面板", {200, 200}, true },
			{ UIClass::UI_CheckBox, L"CheckBox", L"复选框", {100, 20}, false },
			{ UIClass::UI_RadioBox, L"RadioBox", L"单选框", {100, 20}, false },
			{ UIClass::UI_ComboBox, L"ComboBox", L"下拉框", {150, 25}, false },
			{ UIClass::UI_GridView, L"GridView", L"表格", {360, 200}, false },
			{ UIClass::UI_TreeView, L"TreeView", L"树", {220, 220}, false },
			{ UIClass::UI_ProgressBar, L"ProgressBar", L"进度条", {200, 20}, false },
			{ UIClass::UI_Slider, L"Slider", L"滑块", {200, 30}, false },
			{ UIClass::UI_PictureBox, L"PictureBox", L"图片框", {150, 150}, false },
			{ UIClass::UI_Switch, L"Switch", L"开关", {60, 30}, false },
			{ UIClass::UI_TabControl, L"TabControl", L"选项卡", {360, 240}, true },
			{ UIClass::UI_ToolBar, L"ToolBar", L"工具栏", {360, 34}, false },
			{ UIClass::UI_Menu, L"Menu", L"菜单", {600, 28}, false },
			{ UIClass::UI_StatusBar, L"StatusBar", L"状态栏", {600, 26}, false },
			{ UIClass::UI_WebBrowser, L"WebBrowser", L"浏览器", {500, 360}, false },
			{ UIClass::UI_MediaPlayer, L"MediaPlayer", L"媒体播放器", {640, 360}, false },
		};
	}
};
