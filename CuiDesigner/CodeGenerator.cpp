#include "CodeGenerator.h"
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cctype>

// 生成时需要访问具体控件类型的公开字段/方法
#include "../CUI/GUI/ComboBox.h"
#include "../CUI/GUI/GridView.h"
#include "../CUI/GUI/TabControl.h"
#include "../CUI/GUI/ToolBar.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/ProgressBar.h"
#include "../CUI/GUI/Slider.h"
#include "../CUI/GUI/PictureBox.h"
#include "../CUI/GUI/TreeView.h"

#include "../CUI/GUI/Layout/GridPanel.h"
#include "../CUI/GUI/Layout/StackPanel.h"
#include "../CUI/GUI/Layout/DockPanel.h"
#include "../CUI/GUI/Layout/WrapPanel.h"
#include "../CUI/GUI/Layout/RelativePanel.h"

static bool IsLayoutContainerType(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_GridPanel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
		return true;
	default:
		return false;
	}
}

static bool IsContainerType(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_Panel:
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
}

CodeGenerator::CodeGenerator(std::wstring className, const std::vector<std::shared_ptr<DesignerControl>>& controls,
	std::wstring formText, SIZE formSize, POINT formLocation)
	: _className(className), _controls(controls), _formText(formText), _formSize(formSize), _formLocation(formLocation)
{
	if (_formSize.cx <= 0) _formSize.cx = 800;
	if (_formSize.cy <= 0) _formSize.cy = 600;
	// 防御：避免生成一个极端位置导致窗体不可见
	if (_formLocation.x < -10000) _formLocation.x = -10000;
	if (_formLocation.y < -10000) _formLocation.y = -10000;
	if (_formLocation.x > 10000) _formLocation.x = 10000;
	if (_formLocation.y > 10000) _formLocation.y = 10000;
	BuildVarNameMap();
}

static bool IsCppKeyword(const std::string& s)
{
	static const std::unordered_set<std::string> k = {
		"alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
		"auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
		"class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue",
		"co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast",
		"else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline",
		"int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or",
		"or_eq","private","protected","public","register","reinterpret_cast","requires","return","short",
		"signed","sizeof","static","static_assert","static_cast","struct","switch","synchronized","template",
		"this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using",
		"virtual","void","volatile","wchar_t","while","xor","xor_eq"
	};
	return k.find(s) != k.end();
}

std::string CodeGenerator::SanitizeCppIdentifier(const std::string& raw)
{
	std::string out;
	out.reserve(raw.size() + 2);

	for (unsigned char ch : raw)
	{
		if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
			out.push_back((char)ch);
		else
			out.push_back('_');
	}

	// 不能以数字开头
	if (!out.empty() && (out[0] >= '0' && out[0] <= '9'))
		out.insert(out.begin(), '_');

	// 不能空
	if (out.empty()) out = "control";

	// 避免关键字
	if (IsCppKeyword(out)) out += "_";

	return out;
}

void CodeGenerator::BuildVarNameMap()
{
	_varNameOf.clear();
	_varNameOf.reserve(_controls.size());

	std::unordered_map<std::string, int> used;
	used.reserve(_controls.size());

	for (const auto& dc : _controls)
	{
		if (!dc) continue;
		std::string base = SanitizeCppIdentifier(WStringToString(dc->Name));
		// 保守：成员变量建议以小写开头，避免与类型名混淆（仅在安全情况下调整）
		if (!base.empty() && base[0] >= 'A' && base[0] <= 'Z')
			base[0] = (char)(base[0] - 'A' + 'a');

		int& cnt = used[base];
		cnt++;
		std::string finalName = base;
		if (cnt > 1)
			finalName = base + std::to_string(cnt);

		// 二次防御：仍可能撞上关键字（例如 base="this" 调整后）
		if (IsCppKeyword(finalName)) finalName += "_";

		_varNameOf[dc.get()] = finalName;
	}
}

std::string CodeGenerator::GetVarName(const std::shared_ptr<DesignerControl>& dc) const
{
	if (!dc) return "";
	auto it = _varNameOf.find(dc.get());
	if (it != _varNameOf.end()) return it->second;
	return SanitizeCppIdentifier(WStringToString(dc->Name));
}

std::string CodeGenerator::WStringToString(const std::wstring& wstr) const
{
	if (wstr.empty()) return std::string();
	int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, NULL, NULL);
	return result;
}

std::wstring CodeGenerator::StringToWString(const std::string& str) const
{
	if (str.empty()) return std::wstring();
	int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
	return result;
}

std::string CodeGenerator::GetControlTypeName(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_Label: return "Label";
	case UIClass::UI_Button: return "Button";
	case UIClass::UI_TextBox: return "TextBox";
	case UIClass::UI_RichTextBox: return "RichTextBox";
	case UIClass::UI_PasswordBox: return "PasswordBox";
	case UIClass::UI_Panel: return "Panel";
	case UIClass::UI_StackPanel: return "StackPanel";
	case UIClass::UI_GridPanel: return "GridPanel";
	case UIClass::UI_DockPanel: return "DockPanel";
	case UIClass::UI_WrapPanel: return "WrapPanel";
	case UIClass::UI_RelativePanel: return "RelativePanel";
	case UIClass::UI_CheckBox: return "CheckBox";
	case UIClass::UI_RadioBox: return "RadioBox";
	case UIClass::UI_ComboBox: return "ComboBox";
	case UIClass::UI_GridView: return "GridView";
	case UIClass::UI_TreeView: return "TreeView";
	case UIClass::UI_ProgressBar: return "ProgressBar";
	case UIClass::UI_Slider: return "Slider";
	case UIClass::UI_PictureBox: return "PictureBox";
	case UIClass::UI_Switch: return "Switch";
	case UIClass::UI_TabControl: return "TabControl";
	case UIClass::UI_TabPage: return "TabPage";
	case UIClass::UI_ToolBar: return "ToolBar";
	case UIClass::UI_WebBrowser: return "WebBrowser";
	default: return "Control";
	}
}

std::string CodeGenerator::GetIncludeForType(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_TabControl:
	case UIClass::UI_TabPage:
		return "GUI/TabControl.h";
	case UIClass::UI_ToolBar:
		return "GUI/ToolBar.h";
	case UIClass::UI_StackPanel:
		return "GUI/Layout/StackPanel.h";
	case UIClass::UI_GridPanel:
		return "GUI/Layout/GridPanel.h";
	case UIClass::UI_DockPanel:
		return "GUI/Layout/DockPanel.h";
	case UIClass::UI_WrapPanel:
		return "GUI/Layout/WrapPanel.h";
	case UIClass::UI_RelativePanel:
		return "GUI/Layout/RelativePanel.h";
	default:
		return "GUI/" + GetControlTypeName(type) + ".h";
	}
}

std::string CodeGenerator::EscapeWStringLiteral(const std::wstring& s)
{
	std::wstring out;
	out.reserve(s.size());
	for (wchar_t c : s)
	{
		switch (c)
		{
		case L'\\': out += L"\\\\"; break;
		case L'\"': out += L"\\\""; break;
		case L'\r': out += L"\\r"; break;
		case L'\n': out += L"\\n"; break;
		case L'\t': out += L"\\t"; break;
		default: out.push_back(c); break;
		}
	}
	return WStringToString(out);
}

std::string CodeGenerator::FloatLiteral(float v)
{
	// 生成合法 C++ float 字面量：保证有小数点，再加 f 后缀。
	// 例如：0 -> 0.f，1 -> 1.f，0.25 -> 0.25f
	const float eps = 1e-6f;
	float rounded = std::round(v);
	if (std::fabs(v - rounded) <= eps)
	{
		std::ostringstream oss;
		oss << (int)rounded << ".f";
		return oss.str();
	}

	std::ostringstream oss;
	oss.setf(std::ios::fixed);
	oss.precision(6);
	oss << v;
	std::string s = oss.str();
	// trim trailing zeros
	while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0')
		s.pop_back();
	if (!s.empty() && s.back() == '.')
		s.push_back('0');
	return s + "f";
}

std::string CodeGenerator::ColorToString(D2D1_COLOR_F color)
{
	std::ostringstream oss;
	oss << "D2D1::ColorF(" 
		<< FloatLiteral(color.r) << ", "
		<< FloatLiteral(color.g) << ", "
		<< FloatLiteral(color.b) << ", "
		<< FloatLiteral(color.a) << ")";
	return oss.str();
}

std::string CodeGenerator::ThicknessToString(const Thickness& t)
{
	std::ostringstream oss;
	oss << "Thickness(" << FloatLiteral(t.Left) << ", " << FloatLiteral(t.Top) << ", "
		<< FloatLiteral(t.Right) << ", " << FloatLiteral(t.Bottom) << ")";
	return oss.str();
}

std::string CodeGenerator::HorizontalAlignmentToString(::HorizontalAlignment a)
{
	switch (a)
	{
	case HorizontalAlignment::Left: return "HorizontalAlignment::Left";
	case HorizontalAlignment::Center: return "HorizontalAlignment::Center";
	case HorizontalAlignment::Right: return "HorizontalAlignment::Right";
	case HorizontalAlignment::Stretch: return "HorizontalAlignment::Stretch";
	default: return "HorizontalAlignment::Left";
	}
}

std::string CodeGenerator::VerticalAlignmentToString(::VerticalAlignment a)
{
	switch (a)
	{
	case VerticalAlignment::Top: return "VerticalAlignment::Top";
	case VerticalAlignment::Center: return "VerticalAlignment::Center";
	case VerticalAlignment::Bottom: return "VerticalAlignment::Bottom";
	case VerticalAlignment::Stretch: return "VerticalAlignment::Stretch";
	default: return "VerticalAlignment::Top";
	}
}

std::string CodeGenerator::DockToString(::Dock d)
{
	switch (d)
	{
	case Dock::Left: return "Dock::Left";
	case Dock::Top: return "Dock::Top";
	case Dock::Right: return "Dock::Right";
	case Dock::Bottom: return "Dock::Bottom";
	case Dock::Fill: return "Dock::Fill";
	default: return "Dock::Fill";
	}
}

std::string CodeGenerator::OrientationToString(::Orientation o)
{
	switch (o)
	{
	case Orientation::Horizontal: return "Orientation::Horizontal";
	case Orientation::Vertical: return "Orientation::Vertical";
	default: return "Orientation::Vertical";
	}
}

std::string CodeGenerator::SizeUnitToString(SizeUnit u)
{
	switch (u)
	{
	case SizeUnit::Pixel: return "SizeUnit::Pixel";
	case SizeUnit::Percent: return "SizeUnit::Percent";
	case SizeUnit::Auto: return "SizeUnit::Auto";
	case SizeUnit::Star: return "SizeUnit::Star";
	default: return "SizeUnit::Pixel";
	}
}

std::string CodeGenerator::GridLengthToCtorString(const GridLength& gl)
{
	// 优先用静态工厂，生成更可读
	if (gl.Unit == SizeUnit::Auto)
		return "GridLength::Auto()";
	if (gl.Unit == SizeUnit::Star)
	{
		std::ostringstream oss;
		oss << "GridLength::Star(" << FloatLiteral(gl.Value) << ")";
		return oss.str();
	}
	// Pixel / 其他
	{
		std::ostringstream oss;
		oss << "GridLength::Pixels(" << FloatLiteral(gl.Value) << ")";
		return oss.str();
	}
}

std::string CodeGenerator::GenerateControlInstantiation(const std::shared_ptr<DesignerControl>& dc, int indent)
{
	if (!dc || !dc->ControlInstance) return "";
	if (dc->Type == UIClass::UI_TabPage) return ""; // TabPage 通过 TabControl::AddPage 创建

	auto* ctrl = dc->ControlInstance;
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(dc);
	std::string typeName = GetControlTypeName(dc->Type);

	code << indentStr << "// " << name << "\n";
	code << indentStr << name << " = new " << typeName << "(";

	switch (dc->Type)
	{
	case UIClass::UI_Label:
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
		code << "L\"" << EscapeWStringLiteral(ctrl->Text) << "\", "
			<< ctrl->Location.x << ", " << ctrl->Location.y;
		break;
	case UIClass::UI_Button:
		code << "L\"" << EscapeWStringLiteral(ctrl->Text) << "\", "
			<< ctrl->Location.x << ", " << ctrl->Location.y << ", "
			<< ctrl->Size.cx << ", " << ctrl->Size.cy;
		break;
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_PasswordBox:
	case UIClass::UI_ComboBox:
		code << "L\"" << EscapeWStringLiteral(ctrl->Text) << "\", "
			<< ctrl->Location.x << ", " << ctrl->Location.y << ", "
			<< ctrl->Size.cx << ", " << ctrl->Size.cy;
		break;
	case UIClass::UI_Panel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_ProgressBar:
	case UIClass::UI_Slider:
	case UIClass::UI_PictureBox:
	case UIClass::UI_Switch:
	case UIClass::UI_GridView:
	case UIClass::UI_TreeView:
	case UIClass::UI_TabControl:
	case UIClass::UI_ToolBar:
	case UIClass::UI_WebBrowser:
		code << ctrl->Location.x << ", " << ctrl->Location.y << ", "
			<< ctrl->Size.cx << ", " << ctrl->Size.cy;
		break;
	default:
		code << ctrl->Location.x << ", " << ctrl->Location.y << ", "
			<< ctrl->Size.cx << ", " << ctrl->Size.cy;
		break;
	}

	code << ");\n";

	return code.str();
}

std::string CodeGenerator::GenerateControlCommonProperties(const std::shared_ptr<DesignerControl>& dc, int indent)
{
	if (!dc || !dc->ControlInstance) return "";
	if (dc->Type == UIClass::UI_TabPage) return "";

	auto* ctrl = dc->ControlInstance;
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(dc);

	// 尺寸：Label/CheckBox/RadioBox 构造函数无 size
	if (dc->Type == UIClass::UI_Label || dc->Type == UIClass::UI_CheckBox || dc->Type == UIClass::UI_RadioBox)
	{
		code << indentStr << name << "->Size = {" << ctrl->Size.cx << ", " << ctrl->Size.cy << "};\n";
	}

	// 对于不在构造函数中写入 Text 的控件：补齐 Text
	if (dc->Type != UIClass::UI_Label && dc->Type != UIClass::UI_Button &&
		dc->Type != UIClass::UI_CheckBox && dc->Type != UIClass::UI_RadioBox &&
		dc->Type != UIClass::UI_TextBox && dc->Type != UIClass::UI_RichTextBox &&
		dc->Type != UIClass::UI_PasswordBox && dc->Type != UIClass::UI_ComboBox)
	{
		if (!ctrl->Text.empty())
			code << indentStr << name << "->Text = L\"" << EscapeWStringLiteral(ctrl->Text) << "\";\n";
	}

	if (!ctrl->Enable)
		code << indentStr << name << "->Enable = false;\n";
	if (!ctrl->Visible)
		code << indentStr << name << "->Visible = false;\n";

	// 颜色
	code << indentStr << name << "->BackColor = " << ColorToString(ctrl->BackColor) << ";\n";
	code << indentStr << name << "->ForeColor = " << ColorToString(ctrl->ForeColor) << ";\n";
	code << indentStr << name << "->BolderColor = " << ColorToString(ctrl->BolderColor) << ";\n";

	// 布局通用属性
	auto m = ctrl->Margin;
	if (m.Left != 0.0f || m.Top != 0.0f || m.Right != 0.0f || m.Bottom != 0.0f)
		code << indentStr << name << "->Margin = " << ThicknessToString(m) << ";\n";
	auto p = ctrl->Padding;
	if (p.Left != 0.0f || p.Top != 0.0f || p.Right != 0.0f || p.Bottom != 0.0f)
		code << indentStr << name << "->Padding = " << ThicknessToString(p) << ";\n";

	// Min/MaxSize（只在用户改过时输出；不精确比较，保守输出非默认）
	if (ctrl->MinSize.cx != 0 || ctrl->MinSize.cy != 0)
		code << indentStr << name << "->MinSize = {" << ctrl->MinSize.cx << ", " << ctrl->MinSize.cy << "};\n";
	if (ctrl->MaxSize.cx != INT_MAX || ctrl->MaxSize.cy != INT_MAX)
		code << indentStr << name << "->MaxSize = {" << ctrl->MaxSize.cx << ", " << ctrl->MaxSize.cy << "};\n";

	// ComboBox items
	if (dc->Type == UIClass::UI_ComboBox)
	{
		auto* cb = (ComboBox*)ctrl;
		if (cb->values.Count > 0)
		{
			code << indentStr << name << "->values.Clear();\n";
			for (int i = 0; i < cb->values.Count; i++)
			{
				code << indentStr << name << "->values.Add(L\"" << EscapeWStringLiteral(cb->values[i]) << "\");\n";
			}
			code << indentStr << name << "->SelectedIndex = " << cb->SelectedIndex << ";\n";
		}
	}

	// ProgressBar
	if (dc->Type == UIClass::UI_ProgressBar)
	{
		auto* pb = (ProgressBar*)ctrl;
		// 默认值 0.5f
		if (std::fabs(pb->PercentageValue - 0.5f) > 1e-6f)
			code << indentStr << name << "->PercentageValue = " << FloatLiteral(pb->PercentageValue) << ";\n";
	}

	// Slider
	if (dc->Type == UIClass::UI_Slider)
	{
		auto* s = (Slider*)ctrl;
		if (std::fabs(s->Min - 0.0f) > 1e-6f) code << indentStr << name << "->Min = " << FloatLiteral(s->Min) << ";\n";
		if (std::fabs(s->Max - 100.0f) > 1e-6f) code << indentStr << name << "->Max = " << FloatLiteral(s->Max) << ";\n";
		if (std::fabs(s->Value - 0.0f) > 1e-6f) code << indentStr << name << "->Value = " << FloatLiteral(s->Value) << ";\n";
		if (std::fabs(s->Step - 1.0f) > 1e-6f) code << indentStr << name << "->Step = " << FloatLiteral(s->Step) << ";\n";
		if (s->SnapToStep) code << indentStr << name << "->SnapToStep = true;\n";
	}

	// PictureBox (SizeMode is on base Control)
	if (dc->Type == UIClass::UI_PictureBox)
	{
		if (ctrl->SizeMode != ImageSizeMode::Zoom)
		{
			switch (ctrl->SizeMode)
			{
			case ImageSizeMode::Normal: code << indentStr << name << "->SizeMode = ImageSizeMode::Normal;\n"; break;
			case ImageSizeMode::CenterImage: code << indentStr << name << "->SizeMode = ImageSizeMode::CenterImage;\n"; break;
			case ImageSizeMode::StretchIamge: code << indentStr << name << "->SizeMode = ImageSizeMode::StretchIamge;\n"; break;
			case ImageSizeMode::Zoom: code << indentStr << name << "->SizeMode = ImageSizeMode::Zoom;\n"; break;
			default: break;
			}
		}
	}

	// TreeView colors
	if (dc->Type == UIClass::UI_TreeView)
	{
		auto* tv = (TreeView*)ctrl;
		// 这些默认值在 TreeView.h 里有初始化，这里做“保守输出”：只要与默认不同就生成
		const D2D1_COLOR_F defSelBack = D2D1_COLOR_F{ 0.f , 0.f , 1.f , 0.5f };
		const D2D1_COLOR_F defUnderBack = D2D1_COLOR_F{ 0.5961f , 0.9608f , 1.f , 0.5f };
		const D2D1_COLOR_F defSelFore = Colors::White;
		auto neq = [](const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) {
			return std::fabs(a.r - b.r) > 1e-6f || std::fabs(a.g - b.g) > 1e-6f || std::fabs(a.b - b.b) > 1e-6f || std::fabs(a.a - b.a) > 1e-6f;
		};
		if (neq(tv->SelectedBackColor, defSelBack))
			code << indentStr << name << "->SelectedBackColor = " << ColorToString(tv->SelectedBackColor) << ";\n";
		if (neq(tv->UnderMouseItemBackColor, defUnderBack))
			code << indentStr << name << "->UnderMouseItemBackColor = " << ColorToString(tv->UnderMouseItemBackColor) << ";\n";
		if (neq(tv->SelectedForeColor, defSelFore))
			code << indentStr << name << "->SelectedForeColor = " << ColorToString(tv->SelectedForeColor) << ";\n";
	}

	// GridView columns
	if (dc->Type == UIClass::UI_GridView)
	{
		auto* gv = (GridView*)ctrl;
		if (gv->Columns.Count > 0)
		{
			code << indentStr << name << "->Columns.Clear();\n";
			for (int i = 0; i < gv->Columns.Count; i++)
			{
				auto& col = gv->Columns[i];
				std::string typeStr = "ColumnType::Text";
				switch (col.Type)
				{
				case ColumnType::Text: typeStr = "ColumnType::Text"; break;
				case ColumnType::Image: typeStr = "ColumnType::Image"; break;
				case ColumnType::Check: typeStr = "ColumnType::Check"; break;
				default: typeStr = "ColumnType::Text"; break;
				}
				code << indentStr << name << "->Columns.Add(GridViewColumn(L\"" << EscapeWStringLiteral(col.Name)
					<< "\", " << FloatLiteral(col.Width) << ", " << typeStr << ", " << (col.CanEdit ? "true" : "false") << "));\n";
			}
		}
	}

	// TabControl 选择页（页本身在层级阶段创建）
	if (dc->Type == UIClass::UI_TabControl)
	{
		auto* tc = (TabControl*)ctrl;
		code << indentStr << name << "->SelectIndex = " << tc->SelectIndex << ";\n";
	}

	// ToolBar 基本参数
	if (dc->Type == UIClass::UI_ToolBar)
	{
		auto* tb = (ToolBar*)ctrl;
		code << indentStr << name << "->Padding = " << tb->Padding << ";\n";
		code << indentStr << name << "->Gap = " << tb->Gap << ";\n";
		code << indentStr << name << "->ItemHeight = " << tb->ItemHeight << ";\n";
	}

	return code.str();
}

std::string CodeGenerator::GenerateContainerProperties(const std::shared_ptr<DesignerControl>& dc, int indent)
{
	if (!dc || !dc->ControlInstance) return "";
	if (dc->Type == UIClass::UI_TabPage) return "";

	auto* ctrl = dc->ControlInstance;
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(dc);

	if (dc->Type == UIClass::UI_GridPanel)
	{
		auto* gp = (GridPanel*)ctrl;
		const auto& rows = gp->GetRows();
		const auto& cols = gp->GetColumns();
		if (!rows.empty() || !cols.empty())
		{
			code << indentStr << name << "->ClearRows();\n";
			code << indentStr << name << "->ClearColumns();\n";
			for (auto& r : rows)
			{
				code << indentStr << name << "->AddRow(" << GridLengthToCtorString(r.Height)
					<< ", " << FloatLiteral(r.MinHeight) << ", " << FloatLiteral(r.MaxHeight) << ");\n";
			}
			for (auto& c : cols)
			{
				code << indentStr << name << "->AddColumn(" << GridLengthToCtorString(c.Width)
					<< ", " << FloatLiteral(c.MinWidth) << ", " << FloatLiteral(c.MaxWidth) << ");\n";
			}
		}
	}
	else if (dc->Type == UIClass::UI_StackPanel)
	{
		auto* sp = (StackPanel*)ctrl;
		code << indentStr << name << "->SetOrientation(" << OrientationToString(sp->GetOrientation()) << ");\n";
		code << indentStr << name << "->SetSpacing(" << FloatLiteral(sp->GetSpacing()) << ");\n";
	}
	else if (dc->Type == UIClass::UI_WrapPanel)
	{
		auto* wp = (WrapPanel*)ctrl;
		code << indentStr << name << "->SetOrientation(" << OrientationToString(wp->GetOrientation()) << ");\n";
		code << indentStr << name << "->SetItemWidth(" << FloatLiteral(wp->GetItemWidth()) << ");\n";
		code << indentStr << name << "->SetItemHeight(" << FloatLiteral(wp->GetItemHeight()) << ");\n";
	}
	else if (dc->Type == UIClass::UI_DockPanel)
	{
		auto* dp = (DockPanel*)ctrl;
		code << indentStr << name << "->SetLastChildFill(" << (dp->GetLastChildFill() ? "true" : "false") << ");\n";
	}

	return code.str();
}

std::string CodeGenerator::GenerateHeader()
{
	std::ostringstream header;
	std::string className = WStringToString(_className);
	
	// 收集需要的头文件
	std::set<std::string> includes;
	includes.insert("GUI/Form.h");
	includes.insert("GUI/Control.h");
	
	for (const auto& dc : _controls)
	{
		includes.insert(GetIncludeForType(dc->Type));
	}
	
	// 生成头文件
	header << "#pragma once\n";
	for (const auto& inc : includes)
	{
		header << "#include \"" << inc << "\"\n";
	}
	header << "\n";
	
	header << "class " << className << " : public Form\n";
	header << "{\n";
	header << "private:\n";
	
	// 声明控件成员
	for (const auto& dc : _controls)
	{
		std::string name = GetVarName(dc);
		std::string typeName = GetControlTypeName(dc->Type);
		header << "\t" << typeName << "* " << name << ";\n";
	}
	
	header << "\n";
	header << "public:\n";
	header << "\t" << className << "();\n";
	header << "\tvirtual ~" << className << "();\n";
	header << "};\n";
	
	return header.str();
}

std::string CodeGenerator::GenerateCpp()
{
	std::ostringstream cpp;
	std::string className = WStringToString(_className);
	
	// 包含头文件
	cpp << "#include \"" << className << ".h\"\n\n";
	
	// 构造函数（注意：本框架的窗体初始化应走 Form 基类构造函数参数，
	// 在构造函数体内直接写 this->Text/Size/Location 可能不会生效。）
	cpp << className << "::" << className << "()\n";
	std::wstring text = _formText.empty() ? _className : _formText;
	cpp << "\t: Form(L\"" << EscapeWStringLiteral(text) << "\", POINT{ "
		<< _formLocation.x << ", " << _formLocation.y << " }, SIZE{ "
		<< _formSize.cx << ", " << _formSize.cy << " })\n";
	cpp << "{\n\n";
	
	cpp << "\t// 创建控件\n";

	// 1) 先实例化所有可设计控件（不做 AddControl）
	for (const auto& dc : _controls)
	{
		cpp << GenerateControlInstantiation(dc, 1);
		cpp << GenerateControlCommonProperties(dc, 1);
		cpp << GenerateContainerProperties(dc, 1);
		if (dc && dc->ControlInstance && dc->Type != UIClass::UI_TabPage)
			cpp << "\n";
	}

	// 2) 按 DesignerParent 组装层级（容器内应为 container->AddControl）
	cpp << "\t// 组装控件层级（包含布局容器）\n";

	// 建立 Control* -> 变量名 映射
	std::unordered_map<Control*, std::string> varOf;
	varOf.reserve(_controls.size());
	for (const auto& dc : _controls)
	{
		if (!dc || !dc->ControlInstance) continue;
		varOf[dc->ControlInstance] = GetVarName(dc);
	}

	// parent -> children(designer) 映射
	std::unordered_map<Control*, std::vector<std::shared_ptr<DesignerControl>>> childrenOf;
	childrenOf.reserve(_controls.size());
	for (const auto& dc : _controls)
	{
		if (!dc || !dc->ControlInstance) continue;
		childrenOf[dc->DesignerParent].push_back(dc);
	}

	auto sortChildrenByRuntimeOrder = [&](Control* parent, std::vector<std::shared_ptr<DesignerControl>>& list)
	{
		if (!parent || list.size() <= 1) return;
		// 用 parent->Children 的顺序来稳定排序（用于 Stack/Warp 等需要顺序的容器）
		std::unordered_map<Control*, int> idx;
		idx.reserve((size_t)parent->Count);
		for (int i = 0; i < parent->Count; i++)
		{
			idx[parent->operator[](i)] = i;
		}
		std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b)
			{
				int ia = INT_MAX;
				int ib = INT_MAX;
				auto ita = idx.find(a->ControlInstance);
				if (ita != idx.end()) ia = ita->second;
				auto itb = idx.find(b->ControlInstance);
				if (itb != idx.end()) ib = itb->second;
				return ia < ib;
			});
	};

	// TabPage 可能没有 DesignerControl：需要在生成时为其分配变量名
	std::unordered_map<Control*, std::string> tabPageVarOf;
	int tabPageAutoId = 0;

	std::function<void(Control* parentCtrl, const std::string& parentExpr, int indent)> emitChildren;
	std::function<void(const std::shared_ptr<DesignerControl>& dc, const std::string& parentExpr, int indent)> emitControl;

	emitChildren = [&](Control* parentCtrl, const std::string& parentExpr, int indent)
	{
		auto it = childrenOf.find(parentCtrl);
		if (it == childrenOf.end()) return;
		auto list = it->second;
		sortChildrenByRuntimeOrder(parentCtrl, list);
		for (auto& childDc : list)
			emitControl(childDc, parentExpr, indent);
	};

	auto emitLayoutPropsForParent = [&](Control* parentCtrl, const std::string& childVar, Control* childCtrl, int indent)
	{
		if (!parentCtrl || !childCtrl) return;
		std::string indentStr(indent, '\t');
		UIClass pt = parentCtrl->Type();
		if (pt == UIClass::UI_GridPanel)
		{
			cpp << indentStr << childVar << "->GridRow = " << childCtrl->GridRow << ";\n";
			cpp << indentStr << childVar << "->GridColumn = " << childCtrl->GridColumn << ";\n";
			cpp << indentStr << childVar << "->GridRowSpan = " << childCtrl->GridRowSpan << ";\n";
			cpp << indentStr << childVar << "->GridColumnSpan = " << childCtrl->GridColumnSpan << ";\n";
			cpp << indentStr << childVar << "->HAlign = " << HorizontalAlignmentToString(childCtrl->HAlign) << ";\n";
			cpp << indentStr << childVar << "->VAlign = " << VerticalAlignmentToString(childCtrl->VAlign) << ";\n";
		}
		else if (pt == UIClass::UI_DockPanel)
		{
			cpp << indentStr << childVar << "->DockPosition = " << DockToString(childCtrl->DockPosition) << ";\n";
		}
		else if (pt == UIClass::UI_RelativePanel)
		{
			// 设计器中 RelativePanel 主要用 Margin.Left/Top 表示定位
			auto m = childCtrl->Margin;
			if (m.Left != 0.0f || m.Top != 0.0f || m.Right != 0.0f || m.Bottom != 0.0f)
				cpp << indentStr << childVar << "->Margin = " << ThicknessToString(m) << ";\n";
		}
		else if (pt == UIClass::UI_StackPanel || pt == UIClass::UI_WrapPanel)
		{
			// 可选：输出对齐（设计器中经常用 Stretch）
			cpp << indentStr << childVar << "->HAlign = " << HorizontalAlignmentToString(childCtrl->HAlign) << ";\n";
			cpp << indentStr << childVar << "->VAlign = " << VerticalAlignmentToString(childCtrl->VAlign) << ";\n";
		}
	};

	emitControl = [&](const std::shared_ptr<DesignerControl>& dc, const std::string& parentExpr, int indent)
	{
		if (!dc || !dc->ControlInstance) return;
		// TabPage 由 TabControl::AddPage 创建，不走通用 AddControl 流程
		if (dc->Type == UIClass::UI_TabPage) return;
		auto* c = dc->ControlInstance;
		std::string childVar = GetVarName(dc);
		std::string indentStr(indent, '\t');

		// 添加到父容器
		UIClass parentType = UIClass::UI_CUSTOM;
		if (dc->DesignerParent) parentType = dc->DesignerParent->Type();
		if (parentType == UIClass::UI_ToolBar)
		{
			cpp << indentStr << parentExpr << "->AddToolButton((Button*)" << childVar << ");\n";
		}
		else
		{
			cpp << indentStr << parentExpr << "->AddControl(" << childVar << ");\n";
		}

		emitLayoutPropsForParent(dc->DesignerParent, childVar, c, indent);

		// 如果是容器，递归添加子控件
		if (dc->Type == UIClass::UI_TabControl)
		{
			// TabControl：先创建页，再向页内添加子控件
			auto* tc = (TabControl*)c;
			for (int i = 0; i < tc->Count; i++)
			{
				auto* page = tc->operator[](i);
				if (!page) continue;
				std::string pageVar;
				// 如果页本身有 DesignerControl 包装，则用成员变量接收
				auto itVar = varOf.find(page);
				if (itVar != varOf.end())
				{
					pageVar = itVar->second;
					cpp << indentStr << pageVar << " = " << childVar << "->AddPage(L\"" << EscapeWStringLiteral(page->Text) << "\");\n";
				}
				else
				{
					pageVar = childVar + "_page" + std::to_string(i + 1);
					// 避免命名冲突
					pageVar += "_" + std::to_string(++tabPageAutoId);
					cpp << indentStr << "auto* " << pageVar << " = " << childVar << "->AddPage(L\"" << EscapeWStringLiteral(page->Text) << "\");\n";
				}
				tabPageVarOf[page] = pageVar;
				// 页是容器：继续往里加
				emitChildren(page, pageVar, indent + 1);
			}
		}
		else if (dc->Type == UIClass::UI_ToolBar)
		{
			// ToolBar：其子控件必须用 AddToolButton
			emitChildren(c, childVar, indent + 1);
			cpp << indentStr << childVar << "->LayoutItems();\n";
		}
		else if (IsContainerType(dc->Type))
		{
			emitChildren(c, childVar, indent + 1);
			// 对 Panel 类容器，生成后立即触发布局一次
			if (dc->Type == UIClass::UI_Panel || IsLayoutContainerType(dc->Type) || dc->Type == UIClass::UI_TabPage)
				cpp << indentStr << childVar << "->PerformLayout();\n";
		}

		cpp << "\n";
	};

	// 根级控件：DesignerParent == nullptr
	auto rootsIt = childrenOf.find(nullptr);
	if (rootsIt != childrenOf.end())
	{
		auto roots = rootsIt->second;
		sortChildrenByRuntimeOrder(nullptr, roots);
		for (auto& rootDc : roots)
			emitControl(rootDc, "this", 1);
	}
	
	cpp << "}\n\n";
	
	// 析构函数
	cpp << className << "::~" << className << "()\n";
	cpp << "{\n";
	cpp << "}\n";
	
	return cpp.str();
}

bool CodeGenerator::GenerateFiles(std::wstring headerPath, std::wstring cppPath)
{
	try
	{
		// 生成头文件
		std::ofstream headerFile(headerPath);
		if (!headerFile.is_open()) return false;
		headerFile << GenerateHeader();
		headerFile.close();
		
		// 生成cpp文件
		std::ofstream cppFile(cppPath);
		if (!cppFile.is_open()) return false;
		cppFile << GenerateCpp();
		cppFile.close();
		
		return true;
	}
	catch (...)
	{
		return false;
	}
}
