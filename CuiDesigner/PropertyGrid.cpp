#include "PropertyGrid.h"
#include "../CUI/include/Form.h"
#include "ComboBoxItemsEditorDialog.h"
#include "GridViewColumnsEditorDialog.h"
#include "TabControlPagesEditorDialog.h"
#include "ToolBarButtonsEditorDialog.h"
#include "TreeViewNodesEditorDialog.h"
#include "GridPanelDefinitionsEditorDialog.h"
#include "BindingEditorDialog.h"
#include "DataContextSchemaEditorDialog.h"
#include "DesignerPropertyCatalog.h"
#include "StyleSheetEditorDialog.h"
#include "MenuItemsEditorDialog.h"
#include "StatusBarPartsEditorDialog.h"
#include "DesignerCanvas.h"
#include "DesignerCore/Commands/UpdatePropertyCommand.h"
#include "../CUI/include/LinkLabel.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/LoadingRing.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ProgressRing.h"
#include "../CUI/include/PictureBox.h"
#include "../CUI/include/DateTimePicker.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/ScrollView.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/PropertyGrid.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/MediaPlayer.h"
#include "../CUI/include/SplitContainer.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../CUI/include/Layout/WrapPanel.h"
#include "../CUI/include/Layout/DockPanel.h"
#include <commdlg.h>
#include <windowsx.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <set>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
	static bool PropertyNamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	static std::wstring DesignerCategoryCaption(const std::wstring& category)
	{
		if (PropertyNamesEqual(category, L"Common")) return L"常用";
		if (PropertyNamesEqual(category, L"Layout")) return L"布局";
		if (PropertyNamesEqual(category, L"Appearance")) return L"外观";
		if (PropertyNamesEqual(category, L"Behavior")) return L"行为";
		if (PropertyNamesEqual(category, L"Validation")) return L"校验";
		if (PropertyNamesEqual(category, L"Accessibility")) return L"可访问性";
		if (PropertyNamesEqual(category, L"Data")) return L"数据";
		if (PropertyNamesEqual(category, L"Misc")) return L"其他";
		return category;
	}

	static void SetTrackedMetadataProperty(
		DesignerControl& control,
		std::wstring canonicalName,
		DesignerStyleValue value)
	{
		for (auto it = control.MetadataProperties.begin();
			it != control.MetadataProperties.end(); ++it)
		{
			if (!PropertyNamesEqual(it->first, canonicalName)) continue;
			control.MetadataProperties.erase(it);
			break;
		}
		control.MetadataProperties[std::move(canonicalName)] = std::move(value);
	}

	static std::wstring TrimWs(const std::wstring& s);

	static const std::wstring kFontDefaultOption = L"<Default>";

	static std::wstring FloatToText(float v)
	{
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(2) << v;
		return oss.str();
	}

	static std::wstring DoubleToText(double v)
	{
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(4) << v;
		auto s = oss.str();
		while (!s.empty() && s.find(L'.') != std::wstring::npos && s.back() == L'0')
			s.pop_back();
		if (!s.empty() && s.back() == L'.')
			s.pop_back();
		return s.empty() ? L"0" : s;
	}

	static bool TryParseFloatWs(const std::wstring& s, float& out)
	{
		try
		{
			out = std::stof(s);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	static bool EnumOptionMatchesValue(const std::wstring& option, const std::wstring& value)
	{
		auto left = TrimWs(option);
		auto right = TrimWs(value);
		if (left == right)
			return true;

		float leftNumber = 0.0f;
		float rightNumber = 0.0f;
		if (TryParseFloatWs(left, leftNumber) && TryParseFloatWs(right, rightNumber))
			return std::fabs(leftNumber - rightNumber) < 1e-4f;

		return false;
	}

	static std::vector<std::wstring> GetFontNameOptions()
	{
		std::vector<std::wstring> out;
		out.push_back(kFontDefaultOption);

		try
		{
			auto fonts = ::Font::GetSystemFonts();
			std::set<std::wstring> uniq;
			for (auto& f : fonts)
			{
				auto t = TrimWs(f);
				if (!t.empty()) uniq.insert(t);
			}
			for (auto& n : uniq) out.push_back(n);
		}
		catch (...) {}
		return out;
	}

	static std::vector<std::wstring> GetFontSizeOptions()
	{
		static const int sizes[] = { 8,9,10,11,12,14,16,18,20,22,24,26,28,32,36,48,72 };
		std::vector<std::wstring> out;
		out.reserve(_countof(sizes));
		for (int s : sizes) out.push_back(std::to_wstring(s));
		return out;
	}

	static COLORREF ColorFToCOLORREF(const D2D1_COLOR_F& c)
	{
		int r = (int)std::lround(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
		int g = (int)std::lround(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
		int b = (int)std::lround(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
		return RGB(r, g, b);
	}

	static D2D1_COLOR_F COLORREFToColorF(COLORREF cr, float a01)
	{
		float r = GetRValue(cr) / 255.0f;
		float g = GetGValue(cr) / 255.0f;
		float b = GetBValue(cr) / 255.0f;
		return D2D1::ColorF(r, g, b, std::clamp(a01, 0.0f, 1.0f));
	}

	static bool PickColorWithDialog(HWND owner, const D2D1_COLOR_F& initial, D2D1_COLOR_F& out)
	{
		CHOOSECOLORW cc{};
		static COLORREF custom[16]{};
		cc.lStructSize = sizeof(cc);
		cc.hwndOwner = owner;
		cc.rgbResult = ColorFToCOLORREF(initial);
		cc.lpCustColors = custom;
		cc.Flags = CC_FULLOPEN | CC_RGBINIT;
		if (!ChooseColorW(&cc))
			return false;
		out = COLORREFToColorF(cc.rgbResult, initial.a);
		return true;
	}

	static std::wstring TrimWs(const std::wstring& s)
	{
		size_t b = 0;
		while (b < s.size() && iswspace(s[b])) b++;
		size_t e = s.size();
		while (e > b && iswspace(s[e - 1])) e--;
		return s.substr(b, e - b);
	}

	static std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep)
	{
		std::vector<std::wstring> out;
		std::wstring cur;
		for (wchar_t c : s)
		{
			if (c == sep)
			{
				out.push_back(TrimWs(cur));
				cur.clear();
			}
			else cur.push_back(c);
		}
		out.push_back(TrimWs(cur));
		return out;
	}

	static std::wstring JoinStyleClasses(const Control& control)
	{
		std::wstring result;
		for (const auto& styleClass : control.GetStyleClasses())
		{
			if (!result.empty())
				result += L", ";
			result += styleClass;
		}
		return result;
	}

	static std::wstring ColorToText(const D2D1_COLOR_F& c)
	{
		auto toByte = [](float v) -> int {
			return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
		};

		std::wostringstream oss;
		oss << L"#"
			<< std::uppercase << std::hex << std::setfill(L'0')
			<< std::setw(2) << toByte(c.r)
			<< std::setw(2) << toByte(c.g)
			<< std::setw(2) << toByte(c.b);

		int alphaByte = toByte(c.a);
		if (alphaByte != 255)
			oss << std::setw(2) << alphaByte;

		return oss.str();
	}

	static bool TryParseHexNibble(wchar_t c, int& out)
	{
		if (c >= L'0' && c <= L'9') { out = c - L'0'; return true; }
		if (c >= L'a' && c <= L'f') { out = 10 + (c - L'a'); return true; }
		if (c >= L'A' && c <= L'F') { out = 10 + (c - L'A'); return true; }
		return false;
	}

	static bool TryParseHexByte(const std::wstring& s, size_t offset, unsigned char& out)
	{
		int hi = 0, lo = 0;
		if (offset + 1 >= s.size()) return false;
		if (!TryParseHexNibble(s[offset], hi)) return false;
		if (!TryParseHexNibble(s[offset + 1], lo)) return false;
		out = (unsigned char)((hi << 4) | lo);
		return true;
	}

	static bool TryParseColor(const std::wstring& s, D2D1_COLOR_F& out)
	{
		auto t = TrimWs(s);
		if (t.empty()) return false;
		if (t[0] == L'#')
		{
			std::wstring hex = t.substr(1);
			if (hex.size() != 6 && hex.size() != 8) return false;

			unsigned char p0 = 0, p1 = 0, p2 = 0, p3 = 255;
			if (!TryParseHexByte(hex, 0, p0)) return false;
			if (!TryParseHexByte(hex, 2, p1)) return false;
			if (!TryParseHexByte(hex, 4, p2)) return false;

			unsigned char r = p0, g = p1, b = p2, a = 255;
			if (hex.size() == 8)
			{
				if (!TryParseHexByte(hex, 6, p3)) return false;

				const bool argbAlphaIsExtreme = (p0 == 0x00 || p0 == 0xFF);
				const bool rgbaAlphaIsExtreme = (p3 == 0x00 || p3 == 0xFF);
				const bool preferArgb = (!argbAlphaIsExtreme && rgbaAlphaIsExtreme);

				if (preferArgb)
				{
					a = p0;
					r = p1;
					g = p2;
					b = p3;
				}
				else
				{
					r = p0;
					g = p1;
					b = p2;
					a = p3;
				}
			}

			out = D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
			return true;
		}
		// 0xAARRGGBB
		if (t.size() == 10 && t[0] == L'0' && (t[1] == L'x' || t[1] == L'X'))
		{
			std::wstring hex = t.substr(2);
			unsigned char a = 255, r = 0, g = 0, b = 0;
			if (!TryParseHexByte(hex, 0, a)) return false;
			if (!TryParseHexByte(hex, 2, r)) return false;
			if (!TryParseHexByte(hex, 4, g)) return false;
			if (!TryParseHexByte(hex, 6, b)) return false;
			out = D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
			return true;
		}
		// r,g,b or r,g,b,a (float 0~1 or int 0~255)
		auto parts = Split(t, L',');
		if (parts.size() < 3) return false;
		float v[4] = { 0,0,0,1 };
		for (size_t i = 0; i < parts.size() && i < 4; i++)
		{
			try { v[i] = std::stof(parts[i]); }
			catch (...) { return false; }
		}
		bool anyOver1 = (v[0] > 1.0f || v[1] > 1.0f || v[2] > 1.0f || v[3] > 1.0f);
		if (anyOver1)
		{
			out = D2D1::ColorF(v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f);
		}
		else
		{
			out = D2D1::ColorF(v[0], v[1], v[2], v[3]);
		}
		return true;
	}

	static std::wstring ThicknessToText(const Thickness& t)
	{
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(1) << t.Left << L"," << t.Top << L"," << t.Right << L"," << t.Bottom;
		return oss.str();
	}

	static bool TryParseThickness(const std::wstring& s, Thickness& out)
	{
		auto parts = Split(s, L',');
		if (parts.size() != 4) return false;
		try
		{
			out.Left = std::stof(parts[0]);
			out.Top = std::stof(parts[1]);
			out.Right = std::stof(parts[2]);
			out.Bottom = std::stof(parts[3]);
			return true;
		}
		catch (...) { return false; }
	}

	static bool TryParseHAlign(const std::wstring& s, ::HorizontalAlignment& out)
	{
		auto t = TrimWs(s);
		if (t == L"Left") { out = HorizontalAlignment::Left; return true; }
		if (t == L"Center") { out = HorizontalAlignment::Center; return true; }
		if (t == L"Right") { out = HorizontalAlignment::Right; return true; }
		if (t == L"Stretch") { out = HorizontalAlignment::Stretch; return true; }
		return false;
	}

	static bool TryParseVAlign(const std::wstring& s, ::VerticalAlignment& out)
	{
		auto t = TrimWs(s);
		if (t == L"Top") { out = VerticalAlignment::Top; return true; }
		if (t == L"Center") { out = VerticalAlignment::Center; return true; }
		if (t == L"Bottom") { out = VerticalAlignment::Bottom; return true; }
		if (t == L"Stretch") { out = VerticalAlignment::Stretch; return true; }
		return false;
	}

	static std::wstring HAlignToText(::HorizontalAlignment a)
	{
		switch (a)
		{
		case HorizontalAlignment::Left: return L"Left";
		case HorizontalAlignment::Center: return L"Center";
		case HorizontalAlignment::Right: return L"Right";
		case HorizontalAlignment::Stretch: return L"Stretch";
		default: return L"Left";
		}
	}

	static std::wstring VAlignToText(::VerticalAlignment a)
	{
		switch (a)
		{
		case VerticalAlignment::Top: return L"Top";
		case VerticalAlignment::Center: return L"Center";
		case VerticalAlignment::Bottom: return L"Bottom";
		case VerticalAlignment::Stretch: return L"Stretch";
		default: return L"Top";
		}
	}

	static bool TryParseDock(const std::wstring& s, ::Dock& out)
	{
		auto t = TrimWs(s);
		if (t == L"Fill") { out = Dock::Fill; return true; }
		if (t == L"Left") { out = Dock::Left; return true; }
		if (t == L"Top") { out = Dock::Top; return true; }
		if (t == L"Right") { out = Dock::Right; return true; }
		if (t == L"Bottom") { out = Dock::Bottom; return true; }
		return false;
	}

	static std::wstring DockToText(::Dock d)
	{
		switch (d)
		{
		case Dock::Fill: return L"Fill";
		case Dock::Left: return L"Left";
		case Dock::Top: return L"Top";
		case Dock::Right: return L"Right";
		case Dock::Bottom: return L"Bottom";
		default: return L"Fill";
		}
	}

	static bool TryParseImageSizeMode(const std::wstring& s, ::ImageSizeMode& out)
	{
		auto t = TrimWs(s);
		if (t == L"Normal") { out = ImageSizeMode::Normal; return true; }
		if (t == L"CenterImage") { out = ImageSizeMode::CenterImage; return true; }
		if (t == L"Stretch") { out = ImageSizeMode::StretchImage; return true; }
		if (t == L"Zoom") { out = ImageSizeMode::Zoom; return true; }
		// 兼容旧拼写
		if (t == L"StretchImage") { out = ImageSizeMode::StretchImage; return true; }
		return false;
	}

	static std::wstring ImageSizeModeToText(::ImageSizeMode m)
	{
		switch (m)
		{
		case ImageSizeMode::Normal: return L"Normal";
		case ImageSizeMode::CenterImage: return L"CenterImage";
		case ImageSizeMode::StretchImage: return L"Stretch";
		case ImageSizeMode::Zoom: return L"Zoom";
		default: return L"Zoom";
		}
	}

	static const std::set<std::wstring>& KnownEventPropertyNames()
	{
		static const std::set<std::wstring> k = {
			L"OnMouseWheel",
			L"OnMouseMove",
			L"OnMouseDown",
			L"OnMouseUp",
			L"OnMouseClick",
			L"OnMouseDoubleClick",
			L"OnMouseEnter",
			L"OnMouseLeave",
			L"OnKeyDown",
			L"OnKeyUp",
			L"OnCharInput",
			L"OnGotFocus",
			L"OnLostFocus",
			L"OnDropFile",
			L"OnDropText",
			L"OnPaint",
			L"OnClose",
			L"OnMoved",
			L"OnSizeChanged",
			L"OnTextChanged",
			L"OnFormClosing",
			L"OnFormClosed",
			L"OnCommand",
			L"OnChecked",
			L"OnSelectionChanged",
			L"OnSelectedChanged",
			L"OnScrollChanged",
			L"ScrollChanged",
			L"SelectionChanged",
			L"OnGridViewCheckStateChanged",
			L"OnGridViewLinkedTextClick",
			L"OnItemClick",
			L"OnItemDoubleClick",
			L"OnItemCheckChanged",
			L"OnToastClick",
			L"OnToastDismissed",
			L"OnUserAddingRow",
			L"OnUserAddedRow",
			L"OnValueChanged",
			L"OnMenuCommand",
		};
		return k;
	}

	static bool IsEventPropertyName(const std::wstring& name)
	{
		return KnownEventPropertyNames().find(name) != KnownEventPropertyNames().end();
	}

	static std::vector<std::wstring> GetEventPropertiesFor(UIClass type)
	{
		std::vector<std::wstring> out;

		out.push_back(L"OnMouseWheel");
		out.push_back(L"OnMouseMove");
		out.push_back(L"OnMouseDown");
		out.push_back(L"OnMouseUp");
		out.push_back(L"OnMouseClick");
		out.push_back(L"OnMouseDoubleClick");
		out.push_back(L"OnMouseEnter");
		out.push_back(L"OnMouseLeave");
		out.push_back(L"OnKeyDown");
		out.push_back(L"OnKeyUp");
		out.push_back(L"OnCharInput");
		out.push_back(L"OnGotFocus");
		out.push_back(L"OnLostFocus");
		out.push_back(L"OnDropText");
		out.push_back(L"OnDropFile");
		out.push_back(L"OnPaint");
		out.push_back(L"OnClose");
		out.push_back(L"OnMoved");
		out.push_back(L"OnSizeChanged");
		out.push_back(L"OnSelectedChanged");
		out.push_back(L"OnScrollChanged");

		switch (type)
		{
		case UIClass::UI_TextBox:
		case UIClass::UI_RichTextBox:
		case UIClass::UI_PasswordBox:
			out.push_back(L"OnTextChanged");
			break;
		case UIClass::UI_CheckBox:
		case UIClass::UI_RadioBox:
		case UIClass::UI_Switch:
			out.push_back(L"OnChecked");
			break;
		case UIClass::UI_ComboBox:
			out.push_back(L"OnSelectionChanged");
			break;
		case UIClass::UI_DateTimePicker:
			out.push_back(L"OnSelectionChanged");
			break;
		case UIClass::UI_GridView:
			out.push_back(L"ScrollChanged");
			out.push_back(L"SelectionChanged");
			out.push_back(L"OnGridViewCheckStateChanged");
			out.push_back(L"OnGridViewLinkedTextClick");
			out.push_back(L"OnUserAddingRow");
			out.push_back(L"OnUserAddedRow");
			break;
		case UIClass::UI_TreeView:
			out.push_back(L"ScrollChanged");
			out.push_back(L"SelectionChanged");
			break;
		case UIClass::UI_ListView:
		case UIClass::UI_ListBox:
			out.push_back(L"ScrollChanged");
			out.push_back(L"SelectionChanged");
			out.push_back(L"OnItemClick");
			out.push_back(L"OnItemDoubleClick");
			out.push_back(L"OnItemCheckChanged");
			break;
		case UIClass::UI_PropertyGrid:
			out.push_back(L"ScrollChanged");
			out.push_back(L"SelectionChanged");
			out.push_back(L"OnItemClick");
			out.push_back(L"OnValueChanged");
			break;
		case UIClass::UI_ToastHost:
			out.push_back(L"OnToastClick");
			out.push_back(L"OnToastDismissed");
			break;
		case UIClass::UI_Slider:
			out.push_back(L"OnValueChanged");
			break;
		case UIClass::UI_NumericUpDown:
			out.push_back(L"OnValueChanged");
			break;
		case UIClass::UI_Expander:
			out.push_back(L"OnExpandedChanged");
			break;
		case UIClass::UI_Menu:
			out.push_back(L"OnMenuCommand");
			break;
		default:
			break;
		}
		return out;
	}

	static std::vector<std::wstring> GetFormEventProperties()
	{
		return {
			L"OnMouseWheel",
			L"OnMouseMove",
			L"OnMouseDown",
			L"OnMouseUp",
			L"OnMouseClick",
			L"OnMouseDoubleClick",
			L"OnMouseEnter",
			L"OnMouseLeave",
			L"OnKeyDown",
			L"OnKeyUp",
			L"OnCharInput",
			L"OnGotFocus",
			L"OnLostFocus",
			L"OnDropText",
			L"OnDropFile",
			L"OnPaint",
			L"OnClose",
			L"OnMoved",
			L"OnSizeChanged",
			L"OnTextChanged",
			L"OnFormClosing",
			L"OnFormClosed",
			L"OnCommand",
		};
	}
}

void PropertyGrid::CreateEventBoolPropertyItem(std::wstring eventName, bool enabled, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	auto checkBox = new CheckBox(eventName, 10, yOffset);
	checkBox->Size = { width - 20, 20 };
	checkBox->Checked = enabled;
	checkBox->ParentForm = this->ParentForm;
	checkBox->OnMouseClick += [this, eventName](Control* sender, MouseEventArgs) {
		auto box = (CheckBox*)sender;
		UpdatePropertyFromBool(eventName, box->Checked);
	};
	container->AddControl(checkBox);
	RegisterScrollable(checkBox);
	_items.push_back(new PropertyItem(eventName, nullptr, checkBox));
	yOffset += 25;
}

PropertyGrid::PropertyGrid(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
	this->BorderThickness = 1.0f;

	// 标题
	_titleLabel = new Label(L"属性", 10, 10);
	_titleLabel->Size = { width - 20, 25 };
	_titleLabel->Font = new ::Font(L"Microsoft YaHei", 16.0f);
	this->AddControl(_titleLabel);

	_scrollView = new ScrollView(0, _contentTop, width, std::max(0, height - _contentTop));
	_scrollView->BackColor = this->BackColor;
	_scrollView->BorderThickness = 0.0f;
	_scrollView->MouseWheelStep = 25;
	this->AddControl(_scrollView);

	_contentHost = new Panel(0, 0, width, std::max(0, height - _contentTop));
	_contentHost->BackColor = D2D1::ColorF(0, 0, 0, 0);
	_contentHost->BorderThickness = 0.0f;
	UpdateContentHostLayout();
	_scrollView->AddControl(_contentHost);
}

PropertyGrid::~PropertyGrid()
{
}

void PropertyGrid::RegisterScrollable(Control* c)
{
	(void)c;
}

Panel* PropertyGrid::GetContentContainer()
{
	return _contentHost ? _contentHost : this;
}

int PropertyGrid::GetContentTopLocal()
{
	return _contentHost ? 0 : _contentTop;
}

int PropertyGrid::GetContentWidthLocal()
{
	if (!_contentHost) return this->Width;
	return _contentHost->Width;
}

int PropertyGrid::GetViewportHeightLocal()
{
	if (_scrollView) return _scrollView->Height;
	return this->Height - _contentTop;
}

void PropertyGrid::UpdateContentHostLayout()
{
	if (_titleLabel)
	{
		_titleLabel->Size = { std::max(0, this->Width - 20), 25 };
	}
	if (_scrollView)
	{
		_scrollView->Left = 0;
		_scrollView->Top = _contentTop;
		_scrollView->Width = this->Width;
		_scrollView->Height = std::max(0, this->Height - _contentTop);
	}
	if (!_contentHost) return;
	int w = _scrollView ? std::max(0, _scrollView->Width - 12) : this->Width;
	int h = _scrollView ? std::max(0, _scrollView->Height) : std::max(0, this->Height - _contentTop);
	_contentHost->Left = 0;
	_contentHost->Top = 0;
	_contentHost->Width = w;
	_contentHost->Height = std::max(h, _contentHeight);
}

void PropertyGrid::ClampScroll()
{
}

void PropertyGrid::UpdateScrollLayout()
{
	UpdateContentHostLayout();
	if (!_contentHost) return;
	int maxBottom = 0;
	for (int i = 0; i < _contentHost->Count; ++i)
	{
		auto* child = _contentHost->operator[](i);
		if (!child || !child->Visible) continue;
		auto sz = child->ActualSize();
		maxBottom = (std::max)(maxBottom, static_cast<int>(child->Top) + static_cast<int>(sz.cy));
	}
	_contentHeight = maxBottom + _contentBottomPadding;
	_contentHost->Height = (std::max)(GetViewportHeightLocal(), _contentHeight);
}

bool PropertyGrid::TryGetScrollBarLocalRect(D2D1_RECT_F& outTrack, D2D1_RECT_F& outThumb)
{
	(void)outTrack;
	(void)outThumb;
	return true;
}

void PropertyGrid::Update()
{
	UpdateContentHostLayout();
	UpdateScrollLayout();
	Panel::Update();
}

bool PropertyGrid::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (message == WM_KEYDOWN)
	{
		auto* canvas = _binding.GetCanvas();
		if (canvas && (GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if (wParam == 'Z' && !shiftDown)
			{
				if (canvas->UndoCommand()) return true;
			}
			else if (wParam == 'Y' || (wParam == 'Z' && shiftDown))
			{
				if (canvas->RedoCommand()) return true;
			}
		}
	}
	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}

void PropertyGrid::CreatePropertyItem(std::wstring propertyName, std::wstring value, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	// 属性名标签
	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	// 确保ParentForm已设置
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	// 值文本框
	auto valueTextBox = new TextBox(L"", (width - 30) / 2 + 15, yOffset, (width - 30) / 2, 20);
	valueTextBox->Text = value;

	valueTextBox->OnLostFocus += [this, propertyName](Control* sender) {
		auto* textBox = dynamic_cast<TextBox*>(sender);
		if (!textBox) return;
		UpdatePropertyFromTextBox(propertyName, textBox->Text);
	};

	container->AddControl(valueTextBox);
	// 确保ParentForm已设置（关键！）
	valueTextBox->ParentForm = this->ParentForm;
	RegisterScrollable(valueTextBox);

	auto item = new PropertyItem(propertyName, nameLabel, valueTextBox);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateColorPropertyItem(std::wstring propertyName, const D2D1_COLOR_F& value, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	int valueX = (width - 30) / 2 + 15;
	int valueW = (width - 30) / 2;

	// 容器：带背景色的按钮 + 文本
	auto panel = new Panel(valueX, yOffset, valueW, 20);
	panel->BackColor = D2D1::ColorF(0, 0);
	panel->BorderThickness = 0.0f;
	panel->ParentForm = this->ParentForm;

	const int btnW = 28;
	const int gap = 6;
	int textW = valueW - btnW - gap;
	if (textW < 40) textW = 40;

	auto btn = new Button(L"...", 0, -1, btnW, 22);
	btn->ParentForm = this->ParentForm;
	btn->BorderThickness = 1.0f;
	btn->BorderColor = Colors::DimGrey;
	auto refreshButtonColor = [btn](const D2D1_COLOR_F& c) {
		btn->BackColor = c;
		float luminance = c.r * 0.299f + c.g * 0.587f + c.b * 0.114f;
		btn->ForeColor = (luminance < 0.5f || c.a < 0.5f) ? Colors::White : Colors::Black;
		btn->InvalidateVisual();
	};
	refreshButtonColor(value);

	auto textBox = new TextBox(L"", btnW + gap, 0, textW, 20);
	textBox->Text = ColorToText(value);
	textBox->ParentForm = this->ParentForm;
	textBox->OnLostFocus += [this, propertyName, textBox, refreshButtonColor](Control*) {
		UpdatePropertyFromTextBox(propertyName, textBox->Text);
		D2D1_COLOR_F c{};
		if (TryParseColor(textBox->Text, c))
		{
			refreshButtonColor(c);
		}
	};

	btn->OnMouseClick += [this, propertyName, textBox, refreshButtonColor](Control*, MouseEventArgs) {
		if (!this->ParentForm) return;
		D2D1_COLOR_F cur{};
		if (!TryParseColor(textBox->Text, cur)) cur = D2D1::ColorF(0, 0, 0, 1);
		D2D1_COLOR_F picked{};
		if (PickColorWithDialog(this->ParentForm->Handle, cur, picked))
		{
			refreshButtonColor(picked);
			textBox->Text = ColorToText(picked);
			UpdatePropertyFromTextBox(propertyName, textBox->Text);
		}
	};

	panel->AddControl(textBox);
	panel->AddControl(btn);
	container->AddControl(panel);
	RegisterScrollable(panel);

	_items.push_back(new PropertyItem(propertyName, nameLabel, (Control*)panel));

	yOffset += 25;
}

void PropertyGrid::CreateThicknessPropertyItem(std::wstring propertyName, const Thickness& value, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	int valueX = (width - 30) / 2 + 15;
	int valueW = (width - 30) / 2;

	// 多行布局：两行（L/T 与 R/B），提升每个输入框宽度，便于输入
	const int rowH = 20;
	const int gapX = 6;
	const int gapY = 4;
	const int panelH = rowH * 2 + gapY;

	auto panel = new Panel(valueX, yOffset, valueW, panelH);
	panel->BackColor = D2D1::ColorF(0, 0);
	panel->BorderThickness = 0.0f;
	panel->ParentForm = this->ParentForm;

	int boxW = (valueW - gapX) / 2;
	if (boxW < 40) boxW = 40;

	auto makeBox = [&](int x, int y, float v) {
		auto t = new TextBox(L"", x, y, boxW, rowH);
		t->ParentForm = this->ParentForm;
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(2) << v;
		t->Text = oss.str();
		return t;
	};

	auto tbL = makeBox(0, 0, value.Left);
	auto tbT = makeBox(boxW + gapX, 0, value.Top);
	auto tbR = makeBox(0, rowH + gapY, value.Right);
	auto tbB = makeBox(boxW + gapX, rowH + gapY, value.Bottom);

	auto apply = [this, propertyName, tbL, tbT, tbR, tbB](Control*, std::wstring, std::wstring) {
		Thickness t{};
		try { t.Left = std::stof(tbL->Text); } catch (...) { return; }
		try { t.Top = std::stof(tbT->Text); } catch (...) { return; }
		try { t.Right = std::stof(tbR->Text); } catch (...) { return; }
		try { t.Bottom = std::stof(tbB->Text); } catch (...) { return; }
		UpdatePropertyFromTextBox(propertyName, ThicknessToText(t));
	};

	tbL->OnLostFocus += [apply](Control* sender) { apply(sender, L"", L""); };
	tbT->OnLostFocus += [apply](Control* sender) { apply(sender, L"", L""); };
	tbR->OnLostFocus += [apply](Control* sender) { apply(sender, L"", L""); };
	tbB->OnLostFocus += [apply](Control* sender) { apply(sender, L"", L""); };

	panel->AddControl(tbL);
	panel->AddControl(tbT);
	panel->AddControl(tbR);
	panel->AddControl(tbB);
	container->AddControl(panel);
	RegisterScrollable(panel);

	_items.push_back(new PropertyItem(propertyName, nameLabel, (Control*)panel));

	yOffset += panelH + 5;
}

void PropertyGrid::CreateBoolPropertyItem(std::wstring propertyName, bool value, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	// 属性名标签
	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	// 值复选框（不显示额外文字）
	auto valueCheckBox = new CheckBox(L"", (width - 30) / 2 + 15, yOffset);
	valueCheckBox->Checked = value;
	valueCheckBox->ParentForm = this->ParentForm;

	valueCheckBox->OnMouseClick += [this, propertyName](Control* sender, MouseEventArgs) {
		auto checkBox = (CheckBox*)sender;
		UpdatePropertyFromBool(propertyName, checkBox->Checked);
		};

	container->AddControl(valueCheckBox);
	RegisterScrollable(valueCheckBox);

	auto item = new PropertyItem(propertyName, nameLabel, valueCheckBox);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateMetadataPropertyItems(
	const std::shared_ptr<DesignerControl>& control,
	int& yOffset)
{
	if (!control || !control->ControlInstance) return;
	auto* target = control->ControlInstance;
	auto properties = DesignerPropertyCatalog::GetBrowsableProperties(*target);

	auto represented = [this](const std::wstring& name)
	{
		for (const auto* item : _items)
		{
			if (item && PropertyNamesEqual(item->PropertyName, name)) return true;
		}
		return false;
	};

	properties.erase(std::remove_if(properties.begin(), properties.end(),
		[&](const DesignerPropertyDescriptor& property)
		{
			return represented(property.Name);
		}), properties.end());
	if (properties.empty()) return;

	std::wstring currentCategory;
	for (const auto& property : properties)
	{
		if (!PropertyNamesEqual(currentCategory, property.Category))
		{
			currentCategory = property.Category;
			auto* container = GetContentContainer();
			const int width = GetContentWidthLocal();
			auto* heading = new Label(
				L"元数据属性 · " + DesignerCategoryCaption(property.Category),
				10, yOffset + 4);
			heading->Size = { width - 20, 20 };
			heading->Font = new ::Font(L"Microsoft YaHei", 12.0f);
			heading->ForeColor = Colors::DimGrey;
			container->AddControl(heading);
			heading->ParentForm = this->ParentForm;
			_extraControls.push_back(heading);
			RegisterScrollable(heading);
			yOffset += 28;
		}

		BindingValue current;
		(void)property.Metadata->TryGet(*target, current);
		if (property.Editor == ControlPropertyEditorKind::Choice
			&& !property.Choices.empty())
		{
			CreateChoicePropertyItem(property, yOffset);
			continue;
		}

		switch (property.Editor)
		{
		case ControlPropertyEditorKind::Boolean:
		{
			bool value = false;
			(void)current.TryGet(value);
			CreateBoolPropertyItem(property.Name, value, yOffset);
			break;
		}
		case ControlPropertyEditorKind::Color:
		{
			D2D1_COLOR_F value{};
			if (current.TryGet(value)) CreateColorPropertyItem(property.Name, value, yOffset);
			else CreatePropertyItem(property.Name, property.SampleValue, yOffset);
			break;
		}
		case ControlPropertyEditorKind::Thickness:
		{
			Thickness value;
			if (current.TryGet(value)) CreateThicknessPropertyItem(property.Name, value, yOffset);
			else CreatePropertyItem(property.Name, property.SampleValue, yOffset);
			break;
		}
		default:
			if (property.Editor == ControlPropertyEditorKind::Number
				&& property.Minimum && property.Maximum
				&& *property.Minimum < *property.Maximum
				&& (property.ValueKind == DesignerStyleValueKind::Float
					|| property.ValueKind == DesignerStyleValueKind::Double))
			{
				double numericValue = 0.0;
				if (property.ValueKind == DesignerStyleValueKind::Float)
				{
					float value = 0.0f;
					(void)current.TryGet(value);
					numericValue = value;
				}
				else
				{
					(void)current.TryGet(numericValue);
				}
				const double step = property.Step.value_or(
					(*property.Maximum - *property.Minimum) / 100.0);
				CreateFloatSliderPropertyItem(
					property.Name,
					static_cast<float>(numericValue),
					static_cast<float>(*property.Minimum),
					static_cast<float>(*property.Maximum),
					static_cast<float>((std::max)(step, 0.000001)),
					yOffset);
			}
			else
			{
				CreatePropertyItem(property.Name, property.SampleValue, yOffset);
			}
			break;
		}
		if (!_items.empty() && _items.back() && _items.back()->NameLabel)
			_items.back()->NameLabel->Text = property.DisplayName;
	}
}

void PropertyGrid::CreateAnchorPropertyItem(std::wstring propertyName, uint8_t anchorStyles, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	int valueX = (width - 30) / 2 + 15;
	int valueW = (width - 30) / 2;

	// 方向布局：像 WinForms 一样按上/左/右/下摆放
	const int cbSize = 20;
	const int gap = 4;
	const int panelH = cbSize * 3 + gap * 2;

	// 使用一个容器承载 4 个方向开关
	auto panel = new Panel(valueX, yOffset, valueW, panelH);
	panel->BackColor = D2D1::ColorF(0, 0);
	panel->BorderThickness = 0.0f;
	panel->ParentForm = this->ParentForm;

	const int topY = 0;
	const int midY = cbSize + gap;
	const int bottomY = (cbSize + gap) * 2;
	int centerX = (valueW - cbSize) / 2;
	if (centerX < 0) centerX = 0;
	const int leftX = 0;
	int rightX = valueW - cbSize;
	if (rightX < 0) rightX = 0;

	auto cbT = new CheckBox(L"", centerX, topY);
	auto cbL = new CheckBox(L"", leftX, midY);
	auto cbR = new CheckBox(L"", rightX, midY);
	auto cbB = new CheckBox(L"", centerX, bottomY);
	cbL->Size = { cbSize, cbSize };
	cbT->Size = { cbSize, cbSize };
	cbR->Size = { cbSize, cbSize };
	cbB->Size = { cbSize, cbSize };
	cbL->ParentForm = this->ParentForm;
	cbT->ParentForm = this->ParentForm;
	cbR->ParentForm = this->ParentForm;
	cbB->ParentForm = this->ParentForm;

	cbL->Checked = (anchorStyles & AnchorStyles::Left) != 0;
	cbT->Checked = (anchorStyles & AnchorStyles::Top) != 0;
	cbR->Checked = (anchorStyles & AnchorStyles::Right) != 0;
	cbB->Checked = (anchorStyles & AnchorStyles::Bottom) != 0;

	auto apply = [this, cbL, cbT, cbR, cbB](Control*, MouseEventArgs) {
		UpdateAnchorFromChecks(cbL->Checked, cbT->Checked, cbR->Checked, cbB->Checked);
	};
	cbL->OnMouseClick += apply;
	cbT->OnMouseClick += apply;
	cbR->OnMouseClick += apply;
	cbB->OnMouseClick += apply;

	panel->AddControl(cbL);
	panel->AddControl(cbT);
	panel->AddControl(cbR);
	panel->AddControl(cbB);

	container->AddControl(panel);
	RegisterScrollable(panel);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)panel);
	_items.push_back(item);

	yOffset += panelH + 5;
}

void PropertyGrid::CreateEnumPropertyItem(std::wstring propertyName, const std::wstring& value,
	const std::vector<std::wstring>& options, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	auto valueCombo = new ComboBox(L"", (width - 30) / 2 + 15, yOffset, (width - 30) / 2, 20);
	valueCombo->ParentForm = this->ParentForm;
	valueCombo->Items.clear();
	for (auto& o : options) valueCombo->Items.push_back(o);

	int selectedOptionIndex = -1;
	for (size_t i = 0; i < valueCombo->Items.size(); ++i)
	{
		if (EnumOptionMatchesValue(valueCombo->Items[i], value))
		{
			selectedOptionIndex = static_cast<int>(i);
			break;
		}
	}
	valueCombo->SelectedIndex = selectedOptionIndex >= 0 ? selectedOptionIndex : 0;
	if (!valueCombo->Items.empty()
		&& selectedOptionIndex >= 0
		&& static_cast<size_t>(selectedOptionIndex) < valueCombo->Items.size())
	{
		valueCombo->Text = valueCombo->Items[static_cast<size_t>(selectedOptionIndex)];
	}
	else
		valueCombo->Text = value;

	valueCombo->OnSelectionChanged += [this, propertyName](Control* sender) {
		auto comboBox = (ComboBox*)sender;
		UpdatePropertyFromTextBox(propertyName, comboBox->Text);
		};

	container->AddControl(valueCombo);
	RegisterScrollable(valueCombo);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)valueCombo);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateChoicePropertyItem(
	const DesignerPropertyDescriptor& property,
	int& yOffset)
{
	auto* container = GetContentContainer();
	const int width = GetContentWidthLocal();
	auto* nameLabel = new Label(property.DisplayName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	nameLabel->ParentForm = this->ParentForm;
	container->AddControl(nameLabel);
	RegisterScrollable(nameLabel);

	auto* combo = new ComboBox(
		L"", (width - 30) / 2 + 15, yOffset, (width - 30) / 2, 20);
	combo->ParentForm = this->ParentForm;
	for (const auto& choice : property.Choices)
		combo->Items.push_back(choice.DisplayName);
	int selectedIndex = -1;
	for (size_t index = 0; index < property.Choices.size(); ++index)
	{
		if (EnumOptionMatchesValue(
			property.Choices[index].ValueText, property.SampleValue))
		{
			selectedIndex = static_cast<int>(index);
			break;
		}
	}
	combo->SelectedIndex = selectedIndex;
	combo->Text = selectedIndex >= 0
		? property.Choices[static_cast<size_t>(selectedIndex)].DisplayName
		: property.SampleValue;
	const auto propertyName = property.Name;
	const auto choices = property.Choices;
	combo->OnSelectionChanged += [this, propertyName, choices](Control* sender)
	{
		auto* selected = dynamic_cast<ComboBox*>(sender);
		if (!selected) return;
		const int index = selected->SelectedIndex;
		if (index >= 0 && static_cast<size_t>(index) < choices.size())
			UpdatePropertyFromTextBox(
				propertyName, choices[static_cast<size_t>(index)].ValueText);
	};
	container->AddControl(combo);
	RegisterScrollable(combo);
	_items.push_back(new PropertyItem(property.Name, nameLabel, (Control*)combo));
	yOffset += 25;
}

void PropertyGrid::CreateFloatSliderPropertyItem(std::wstring propertyName, float value,
	float minValue, float maxValue, float step, int& yOffset)
{
	auto* container = GetContentContainer();
	int width = GetContentWidthLocal();

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	container->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;
	RegisterScrollable(nameLabel);

	auto slider = new Slider((width - 30) / 2 + 15, yOffset - 4, (width - 30) / 2, 28);
	slider->ParentForm = this->ParentForm;
	slider->Min = minValue;
	slider->Max = maxValue;
	slider->Step = step;
	slider->SnapToStep = false;
	slider->Value = value;

	slider->OnValueChanged += [this, propertyName](Control*, float, float newValue) {
		if (ShouldGroupFloatSliderProperty(propertyName))
		{
			if (_pendingFloatSliderCommand.Active || BeginGroupedFloatSliderEdit(propertyName))
			{
				UpdateFloatPropertyPreview(propertyName, newValue);
				return;
			}
		}
		UpdatePropertyFromFloat(propertyName, newValue);
		};
	slider->OnMouseUp += [this, propertyName](Control*, MouseEventArgs) {
		if (_pendingFloatSliderCommand.Active && _pendingFloatSliderCommand.PropertyName == propertyName)
		{
			CommitGroupedFloatSliderEdit();
		}
		};

	container->AddControl(slider);
	RegisterScrollable(slider);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)slider);
	_items.push_back(item);

	yOffset += 32;
}

bool PropertyGrid::ShouldGroupFloatSliderProperty(const std::wstring& propertyName) const
{
	if (propertyName != L"PercentageValue")
	{
		return false;
	}

	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance)
	{
		return false;
	}

	return currentControl->Type == UIClass::UI_ProgressBar ||
		currentControl->Type == UIClass::UI_ProgressRing;
}

bool PropertyGrid::TryCapturePropertyCommandState(DesignerModel::DesignDocument& document,
	std::vector<std::wstring>& selectionNames,
	std::wstring& selectionName) const
{
	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		return false;
	}

	std::wstring error;
	if (!canvas->BuildDesignDocument(document, &error))
	{
		return false;
	}

	selectionName.clear();
	selectionNames.clear();
	if (auto boundControl = _binding.GetBoundControl())
	{
		selectionName = boundControl->Name;
		if (!selectionName.empty())
		{
			selectionNames.push_back(selectionName);
		}
	}

	return true;
}

bool PropertyGrid::BeginGroupedFloatSliderEdit(const std::wstring& propertyName)
{
	if (_pendingFloatSliderCommand.Active)
	{
		return _pendingFloatSliderCommand.PropertyName == propertyName;
	}

	PendingFloatSliderCommand pending;
	if (!TryCapturePropertyCommandState(
		pending.BeforeDocument,
		pending.BeforeSelectionNames,
		pending.BeforeSelectionName))
	{
		return false;
	}

	pending.Active = true;
	pending.PropertyName = propertyName;
	_pendingFloatSliderCommand = std::move(pending);
	return true;
}

void PropertyGrid::CommitGroupedFloatSliderEdit()
{
	if (!_pendingFloatSliderCommand.Active)
	{
		return;
	}

	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		CancelGroupedFloatSliderEdit();
		return;
	}

	DesignerModel::DesignDocument afterDocument;
	std::vector<std::wstring> afterSelectionNames;
	std::wstring afterSelectionName;
	if (!TryCapturePropertyCommandState(afterDocument, afterSelectionNames, afterSelectionName))
	{
		CancelGroupedFloatSliderEdit();
		return;
	}

	auto command = std::make_unique<UpdatePropertyCommand>(
		canvas,
		std::move(_pendingFloatSliderCommand.BeforeDocument),
		std::move(afterDocument),
		std::move(_pendingFloatSliderCommand.BeforeSelectionNames),
		std::move(afterSelectionNames),
		std::move(_pendingFloatSliderCommand.BeforeSelectionName),
		std::move(afterSelectionName),
		L"UpdateProperty:" + _pendingFloatSliderCommand.PropertyName,
		true);
	canvas->ExecuteCommand(std::move(command));
	CancelGroupedFloatSliderEdit();
}

void PropertyGrid::CancelGroupedFloatSliderEdit()
{
	_pendingFloatSliderCommand = PendingFloatSliderCommand{};
}

void PropertyGrid::ApplyFloatPropertyValue(Control* targetControl, const std::wstring& propertyName, float value)
{
	if (!targetControl)
	{
		return;
	}

	if (propertyName == L"PercentageValue")
	{
		float v = std::clamp(value, 0.0f, 1.0f);
		if (targetControl->Type() == UIClass::UI_ProgressBar)
		{
			auto* progressBar = (ProgressBar*)targetControl;
			progressBar->PercentageValue = v;
		}
		else if (targetControl->Type() == UIClass::UI_ProgressRing)
		{
			auto* progressRing = (ProgressRing*)targetControl;
			progressRing->PercentageValue = v;
		}
	}
}

void PropertyGrid::UpdateFloatPropertyPreview(const std::wstring& propertyName, float value)
{
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance)
	{
		return;
	}

	auto* targetControl = currentControl->ControlInstance;
	try
	{
		ApplyFloatPropertyValue(targetControl, propertyName, value);
	}
	catch (...)
	{
		return;
	}

	_binding.NotifyControlChanged(targetControl);
}

void PropertyGrid::UpdatePropertyFromTextBox(std::wstring propertyName, std::wstring value)
{
	ExecutePropertyCommand(propertyName, [this, propertyName, value]() {
	// 未选中控件时：编辑“被设计窗体”属性
	if (_binding.IsFormBinding())
	{
		auto* canvas = _binding.GetCanvas();
		if (!canvas) return;
		try
		{
			if (propertyName == L"Name")
			{
				_binding.ApplyFormTextProperty(propertyName, value);
			}
			else if (propertyName == L"Text")
			{
				_binding.ApplyFormTextProperty(propertyName, value);
			}
			else if (propertyName == L"FontName")
			{
				auto v = TrimWs(value);
				if (v == kFontDefaultOption) v.clear();
				canvas->SetDesignedFormFontName(v);
			}
			else if (propertyName == L"FontSize")
			{
				float fs = 0.0f;
				if (TryParseFloatWs(TrimWs(value), fs))
					canvas->SetDesignedFormFontSize(fs);
			}
			else if (propertyName == L"BackColor")
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) canvas->SetDesignedFormBackColor(c);
			}
			else if (propertyName == L"ForeColor")
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) canvas->SetDesignedFormForeColor(c);
			}
			else if (propertyName == L"HeadHeight")
			{
				_binding.ApplyFormTextProperty(propertyName, value);
			}
			else if (propertyName == L"X")
			{
				auto p = canvas->GetDesignedFormLocation();
				p.x = std::stoi(value);
				canvas->SetDesignedFormLocation(p);
			}
			else if (propertyName == L"Y")
			{
				auto p = canvas->GetDesignedFormLocation();
				p.y = std::stoi(value);
				canvas->SetDesignedFormLocation(p);
			}
		else if (propertyName == L"Width")
		{
			auto formSize = canvas->GetDesignedFormSize();
			formSize.cx = std::stoi(value);
			canvas->SetDesignedFormSize(formSize);
		}
		else if (propertyName == L"Height")
		{
			auto formSize = canvas->GetDesignedFormSize();
			formSize.cy = std::stoi(value);
			canvas->SetDesignedFormSize(formSize);
		}
		}
		catch (...) {}
		return;
	}
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance) return;

	// 事件属性：仅更新设计期映射，不改运行时控件状态
	if (IsEventPropertyName(propertyName))
	{
		auto v = TrimWs(value);
		if (v.empty())
			currentControl->EventHandlers.erase(propertyName);
		else
			currentControl->EventHandlers[propertyName] = std::move(v);
		return;
	}

	auto targetControl = currentControl->ControlInstance;
	auto* canvas = _binding.GetCanvas();

	try
	{
		if (propertyName == L"Name")
		{
			if (canvas)
			{
				currentControl->Name = _binding.MakeUniqueControlName(currentControl, value);
				_binding.SyncDefaultNameCounter(currentControl->Type, currentControl->Name);
			}
			else
				currentControl->Name = value;
		}
		else if (propertyName == L"StyleId")
		{
			targetControl->SetStyleId(TrimWs(value));
		}
		else if (propertyName == L"StyleClasses")
		{
			targetControl->ClearStyleClasses();
			for (auto& styleClass : Split(value, L','))
			{
				if (!styleClass.empty())
					targetControl->AddStyleClass(std::move(styleClass));
			}
		}
		else if (propertyName == L"Text")
		{
			targetControl->Text = value;
		}
		else if (propertyName == L"FontName")
		{
			auto v = TrimWs(value);
			if (v == kFontDefaultOption || v.empty())
			{
				if (_binding.GetDesignedFormSharedFont())
					targetControl->SetFontEx(_binding.GetDesignedFormSharedFont(), false);
				else
					targetControl->SetFontEx(nullptr, false);
			}
			else
			{
				float curSize = targetControl->Font ? targetControl->Font->FontSize : GetDefaultFontObject()->FontSize;
				targetControl->Font = new ::Font(v, curSize);
			}
		}
		else if (propertyName == L"FontSize")
		{
			float fs = 0.0f;
			if (!TryParseFloatWs(TrimWs(value), fs))
				throw std::exception();
			if (fs < 1.0f) fs = 1.0f;
			if (fs > 200.0f) fs = 200.0f;
			std::wstring curName = targetControl->Font ? targetControl->Font->FontName : GetDefaultFontObject()->FontName;
			targetControl->Font = new ::Font(curName, fs);
		}
		else if (propertyName == L"X")
		{
			auto loc = targetControl->Location;
			loc.x = std::stoi(value);
			targetControl->Location = loc;
		}
		else if (propertyName == L"Y")
		{
			auto loc = targetControl->Location;
			loc.y = std::stoi(value);
			targetControl->Location = loc;
		}
		else if (propertyName == L"Width")
		{
			auto size = targetControl->Size;
			size.cx = std::stoi(value);
			targetControl->Size = size;
		}
		else if (propertyName == L"Height")
		{
			auto size = targetControl->Size;
			size.cy = std::stoi(value);
			targetControl->Size = size;
		}
		else if (propertyName == L"Enabled")
		{
			targetControl->Enable = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"Visible")
		{
			targetControl->Visible = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"BackColor")
		{
			D2D1_COLOR_F c;
			if (TryParseColor(value, c)) targetControl->BackColor = c;
		}
		else if (propertyName == L"ForeColor")
		{
			D2D1_COLOR_F c;
			if (TryParseColor(value, c)) targetControl->ForeColor = c;
		}
		else if (propertyName == L"BorderColor")
		{
			D2D1_COLOR_F c;
			if (TryParseColor(value, c)) targetControl->BorderColor = c;
		}
		else if (propertyName == L"ShowValidationBorder")
		{
			targetControl->ShowValidationBorder = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"ShowValidationToolTip")
		{
			targetControl->ShowValidationToolTip = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"ValidationBorderThickness")
		{
			float parsed = 0.0f;
			if (TryParseFloatWs(TrimWs(value), parsed)) targetControl->ValidationBorderThickness = parsed;
		}
		else if (propertyName == L"ValidationCornerRadius")
		{
			float parsed = 0.0f;
			if (TryParseFloatWs(TrimWs(value), parsed)) targetControl->ValidationCornerRadius = parsed;
		}
		else if (propertyName == L"ValidationToolTipMaxWidth")
		{
			float parsed = 0.0f;
			if (TryParseFloatWs(TrimWs(value), parsed)) targetControl->ValidationToolTipMaxWidth = parsed;
		}
		else if (propertyName == L"AccessibleDescription")
		{
			targetControl->AccessibleDescription = value;
		}
		else if (propertyName == L"Margin")
		{
			Thickness t;
			if (TryParseThickness(value, t)) targetControl->Margin = t;
		}
		else if (propertyName == L"Padding")
		{
			Thickness t;
			if (TryParseThickness(value, t)) targetControl->Padding = t;
		}
		else if (propertyName == L"HAlign")
		{
			::HorizontalAlignment a;
			if (TryParseHAlign(value, a)) targetControl->HAlign = a;
		}
		else if (propertyName == L"VAlign")
		{
			::VerticalAlignment a;
			if (TryParseVAlign(value, a)) targetControl->VAlign = a;
		}
		else if (propertyName == L"Dock")
		{
			::Dock d;
			if (TryParseDock(value, d)) targetControl->DockPosition = d;
		}
				else if (propertyName == L"ZIndex")
				{
					targetControl->ZIndex = std::stoi(value);
				}
		else if (propertyName == L"GridRow")
		{
			targetControl->GridRow = std::stoi(value);
		}
		else if (propertyName == L"GridColumn")
		{
			targetControl->GridColumn = std::stoi(value);
		}
		else if (propertyName == L"GridRowSpan")
		{
			targetControl->GridRowSpan = std::stoi(value);
		}
		else if (propertyName == L"GridColumnSpan")
		{
			targetControl->GridColumnSpan = std::stoi(value);
		}
		else if (propertyName == L"Mode")
		{
			if (targetControl->Type() == UIClass::UI_DateTimePicker)
			{
				auto* dateTimePicker = (DateTimePicker*)targetControl;
				auto v = TrimWs(value);
				if (v == L"DateOnly") dateTimePicker->Mode = DateTimePickerMode::DateOnly;
				else if (v == L"TimeOnly") dateTimePicker->Mode = DateTimePickerMode::TimeOnly;
				else dateTimePicker->Mode = DateTimePickerMode::DateTime;
			}
		}
		else if (propertyName == L"SizeMode")
		{
			if (targetControl->Type() == UIClass::UI_PictureBox)
			{
				::ImageSizeMode m;
				if (TryParseImageSizeMode(value, m)) targetControl->SizeMode = m;
			}
		}
		else if (propertyName == L"MediaFile")
		{
			if (currentControl->Type == UIClass::UI_MediaPlayer)
			{
				// 设计期字段：仅保存路径，不在设计器里自动加载/播放
				currentControl->DesignStrings[L"mediaFile"] = TrimWs(value);
			}
		}
		else if (propertyName == L"SelectedBackColor")
		{
			if (targetControl->Type() == UIClass::UI_TreeView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((TreeView*)targetControl)->SelectedBackColor = c;
			}
		}
		else if (propertyName == L"UnderMouseItemBackColor")
		{
			if (targetControl->Type() == UIClass::UI_TreeView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((TreeView*)targetControl)->UnderMouseItemBackColor = c;
			}
		}
		else if (propertyName == L"SelectedForeColor")
		{
			if (targetControl->Type() == UIClass::UI_TreeView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((TreeView*)targetControl)->SelectedForeColor = c;
			}
		}
		else
		{
			const auto properties = DesignerPropertyCatalog::GetStyleProperties(*targetControl);
			const auto* property = DesignerPropertyCatalog::Find(properties, propertyName);
			if (property)
			{
				std::wstring canonicalName;
				DesignerStyleValue effective;
				std::wstring metadataError;
				if (DesignerPropertyCatalog::ApplyValue(
					*targetControl,
					property->Name,
					DesignerStyleValue{ property->ValueKind, value },
					&canonicalName,
					&effective,
					&metadataError))
				{
					SetTrackedMetadataProperty(
						*currentControl, std::move(canonicalName), std::move(effective));
				}
			}
		}
	}
	catch (...)
	{
	}

	_binding.NotifyControlChanged(targetControl);
	});
}

void PropertyGrid::UpdatePropertyFromFloat(std::wstring propertyName, float value)
{
	ExecutePropertyCommand(propertyName, [this, propertyName, value]() {
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance) return;
	auto targetControl = currentControl->ControlInstance;

	try
	{
		const auto properties = DesignerPropertyCatalog::GetStyleProperties(*targetControl);
		const auto* property = DesignerPropertyCatalog::Find(properties, propertyName);
		if (property && (property->ValueKind == DesignerStyleValueKind::Float
			|| property->ValueKind == DesignerStyleValueKind::Double))
		{
			std::wstring canonicalName;
			DesignerStyleValue effective;
			if (DesignerPropertyCatalog::ApplyValue(
				*targetControl,
				property->Name,
				DesignerStyleValue{ property->ValueKind, FloatToText(value) },
				&canonicalName,
				&effective))
			{
				SetTrackedMetadataProperty(
					*currentControl, std::move(canonicalName), std::move(effective));
			}
		}
		else
		{
			ApplyFloatPropertyValue(targetControl, propertyName, value);
		}
	}
	catch (...) {}

	_binding.NotifyControlChanged(targetControl);
	});
}

void PropertyGrid::UpdateAnchorFromChecks(bool left, bool top, bool right, bool bottom)
{
	ExecutePropertyCommand(L"Anchor", [this, left, top, right, bottom]() {
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance) return;
	auto* targetControl = currentControl->ControlInstance;

	uint8_t a = AnchorStyles::None;
	if (left) a |= AnchorStyles::Left;
	if (top) a |= AnchorStyles::Top;
	if (right) a |= AnchorStyles::Right;
	if (bottom) a |= AnchorStyles::Bottom;
	_binding.ApplyAnchorStylesKeepingBounds(targetControl, a);

	_binding.NotifyControlChanged(targetControl);
	});
}

void PropertyGrid::UpdatePropertyFromBool(std::wstring propertyName, bool value)
{
	ExecutePropertyCommand(propertyName, [this, propertyName, value]() {
	// 未选中控件时：编辑“被设计窗体”属性
	if (_binding.IsFormBinding())
	{
		auto* canvas = _binding.GetCanvas();
		if (!canvas) return;
		// 事件：写入窗体事件映射（用于保存/导出）
		if (IsEventPropertyName(propertyName))
		{
			canvas->SetDesignedFormEventEnabled(propertyName, value);
			return;
		}
		_binding.ApplyFormBoolProperty(propertyName, value);
		return;
	}
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance) return;
	auto targetControl = currentControl->ControlInstance;

	// 事件：仅更新设计期映射
	if (IsEventPropertyName(propertyName))
	{
		if (value)
		{
			currentControl->EventHandlers[propertyName] = L"1";
		}
		else
		{
			currentControl->EventHandlers.erase(propertyName);
		}
		return;
	}
	else if (propertyName == L"AllowDateSelection")
	{
		if (targetControl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)targetControl)->AllowDateSelection = value;
	}
	else if (propertyName == L"AllowTimeSelection")
	{
		if (targetControl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)targetControl)->AllowTimeSelection = value;
	}
	else if (propertyName == L"AllowModeSwitch")
	{
		if (targetControl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)targetControl)->AllowModeSwitch = value;
	}
	else if (propertyName == L"Expand")
	{
		if (targetControl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)targetControl)->SetExpanded(value);
	}
	else if (propertyName == L"Visited")
	{
		if (targetControl->Type() == UIClass::UI_LinkLabel)
			((LinkLabel*)targetControl)->Visited = value;
	}
	else if (propertyName == L"Enabled")
	{
		targetControl->Enable = value;
	}
	else if (propertyName == L"Visible")
	{
		targetControl->Visible = value;
	}
	else
	{
		const auto properties = DesignerPropertyCatalog::GetStyleProperties(*targetControl);
		const auto* property = DesignerPropertyCatalog::Find(properties, propertyName);
		if (property && property->ValueKind == DesignerStyleValueKind::Bool)
		{
			std::wstring canonicalName;
			DesignerStyleValue effective;
			if (DesignerPropertyCatalog::ApplyValue(
				*targetControl,
				property->Name,
				DesignerStyleValue{ DesignerStyleValueKind::Bool, value ? L"true" : L"false" },
				&canonicalName,
				&effective))
			{
				SetTrackedMetadataProperty(
					*currentControl, std::move(canonicalName), std::move(effective));
			}
		}
	}


	_binding.NotifyControlChanged(targetControl);
	});
}

void PropertyGrid::ExecutePropertyCommand(const std::wstring& propertyName, const std::function<void()>& applyChange)
{
	if (!applyChange)
	{
		return;
	}

	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		applyChange();
		return;
	}

	DesignerModel::DesignDocument beforeDocument;
	std::wstring error;
	if (!canvas->BuildDesignDocument(beforeDocument, &error))
	{
		applyChange();
		return;
	}

	std::wstring beforeSelectionName;
	if (auto boundControl = _binding.GetBoundControl())
	{
		beforeSelectionName = boundControl->Name;
	}
	std::vector<std::wstring> beforeSelectionNames;
	if (!beforeSelectionName.empty())
	{
		beforeSelectionNames.push_back(beforeSelectionName);
	}

	applyChange();

	DesignerModel::DesignDocument afterDocument;
	if (!canvas->BuildDesignDocument(afterDocument, &error))
	{
		return;
	}

	std::wstring afterSelectionName;
	if (auto boundControl = _binding.GetBoundControl())
	{
		afterSelectionName = boundControl->Name;
	}
	std::vector<std::wstring> afterSelectionNames;
	if (!afterSelectionName.empty())
	{
		afterSelectionNames.push_back(afterSelectionName);
	}

	if (beforeDocument == afterDocument && beforeSelectionName == afterSelectionName && beforeSelectionNames == afterSelectionNames)
	{
		return;
	}

	auto command = std::make_unique<UpdatePropertyCommand>(
		canvas,
		std::move(beforeDocument),
		std::move(afterDocument),
		std::move(beforeSelectionNames),
		std::move(afterSelectionNames),
		std::move(beforeSelectionName),
		std::move(afterSelectionName),
		L"UpdateProperty:" + propertyName,
		true);
	canvas->ExecuteCommand(std::move(command));
}

void PropertyGrid::CommitPendingEdits()
{
	CommitGroupedFloatSliderEdit();

	if (!this->ParentForm || !this->ParentForm->Selected)
	{
		return;
	}

	auto isDescendantOf = [](Control* root, Control* node) -> bool {
		if (!root || !node) return false;
		if (root == node) return true;
		std::vector<Control*> stack;
		stack.reserve(64);
		stack.push_back(root);
		while (!stack.empty())
		{
			Control* current = stack.back();
			stack.pop_back();
			if (!current) continue;
			for (size_t i = 0; i < current->Children.size(); ++i)
			{
				auto* child = current->Children[i];
				if (!child) continue;
				if (child == node) return true;
				stack.push_back(child);
			}
		}
		return false;
	};

	auto* selected = this->ParentForm->Selected;
	bool belongsToPropertyGrid = false;
	for (auto item : _items)
	{
		if (!item) continue;
		if ((item->NameLabel && selected == item->NameLabel) ||
			(item->ValueControl && (selected == item->ValueControl || isDescendantOf(item->ValueControl, selected))) ||
			(item->ValueTextBox && selected == item->ValueTextBox) ||
			(item->ValueCheckBox && selected == item->ValueCheckBox))
		{
			belongsToPropertyGrid = true;
			break;
		}
	}

	if (!belongsToPropertyGrid)
	{
		for (auto* control : _extraControls)
		{
			if (control && (selected == control || isDescendantOf(control, selected)))
			{
				belongsToPropertyGrid = true;
				break;
			}
		}
	}

	if (!belongsToPropertyGrid)
	{
		return;
	}

	selected->OnLostFocus(selected);
	selected->InvalidateVisual();
	this->ParentForm->Selected = nullptr;
}

void PropertyGrid::LoadControl(std::shared_ptr<DesignerControl> control)
{
	CommitGroupedFloatSliderEdit();
	Clear();
	_binding.BindControl(control);
	_scrollOffsetY = 0;

	if (!control || !control->ControlInstance)
	{
		// 未选中控件时：展示被设计窗体属性
		auto* canvas = _binding.GetCanvas();
		if (canvas)
		{
			auto form = _binding.CaptureFormSnapshot();
			_titleLabel->Text = L"属性 - 窗体";
			int yOffset = GetContentTopLocal();
			CreatePropertyItem(L"Name", form.Name, yOffset);
			CreatePropertyItem(L"Text", form.Text, yOffset);
			{
				std::wstring fn = form.FontName;
				std::wstring dispName = fn.empty() ? kFontDefaultOption : fn;
				CreateEnumPropertyItem(L"FontName", dispName, GetFontNameOptions(), yOffset);
				CreateEnumPropertyItem(L"FontSize", FloatToText(form.FontSize), GetFontSizeOptions(), yOffset);
			}
			CreateColorPropertyItem(L"BackColor", form.BackColor, yOffset);
			CreateColorPropertyItem(L"ForeColor", form.ForeColor, yOffset);
			CreateBoolPropertyItem(L"ShowInTaskBar", form.ShowInTaskBar, yOffset);
			CreateBoolPropertyItem(L"TopMost", form.TopMost, yOffset);
			CreateBoolPropertyItem(L"Enable", form.Enable, yOffset);
			CreateBoolPropertyItem(L"Visible", form.Visible, yOffset);
			CreateBoolPropertyItem(L"VisibleHead", form.VisibleHead, yOffset);
			CreatePropertyItem(L"HeadHeight", std::to_wstring(form.HeadHeight), yOffset);
			CreateBoolPropertyItem(L"MinBox", form.MinBox, yOffset);
			CreateBoolPropertyItem(L"MaxBox", form.MaxBox, yOffset);
			CreateBoolPropertyItem(L"CloseBox", form.CloseBox, yOffset);
			CreateBoolPropertyItem(L"CenterTitle", form.CenterTitle, yOffset);
			CreateBoolPropertyItem(L"AllowResize", form.AllowResize, yOffset);
			auto p = form.Location;
			CreatePropertyItem(L"X", std::to_wstring(p.x), yOffset);
			CreatePropertyItem(L"Y", std::to_wstring(p.y), yOffset);
			auto s = form.Size;
			CreatePropertyItem(L"Width", std::to_wstring(s.cx), yOffset);
			CreatePropertyItem(L"Height", std::to_wstring(s.cy), yOffset);

			{
				auto* container = GetContentContainer();
				const int width = GetContentWidthLocal();
				auto schemaButtonText = [canvas]() {
					return L"编辑 DataContext Schema ("
						+ std::to_wstring(canvas->GetDataContextSchema().size()) + L")...";
				};
				auto* editSchema = new Button(
					schemaButtonText(), 10, yOffset + 8, width - 20, 28);
				editSchema->OnMouseClick += [this, canvas, editSchema, schemaButtonText](
					Control*, MouseEventArgs) {
					if (!this->ParentForm) return;
					DataContextSchemaEditorDialog dialog(
						canvas->GetDataContextSchema(),
						canvas->GetDesignDataContext().get());
					dialog.ShowDialog(this->ParentForm->Handle);
					if (!dialog.Applied
						|| dialog.ResultSchema == canvas->GetDataContextSchema()) return;

					auto result = std::move(dialog.ResultSchema);
					std::wstring schemaError;
					ExecutePropertyCommand(L"DataContextSchema", [canvas, &schemaError,
						result = std::move(result)]() mutable {
						(void)canvas->SetDataContextSchema(std::move(result), &schemaError);
					});
					if (!schemaError.empty())
					{
						::MessageBoxW(this->ParentForm->Handle, schemaError.c_str(),
							L"DataContext Schema 无效", MB_OK | MB_ICONWARNING);
						return;
					}
					editSchema->Text = schemaButtonText();
					editSchema->InvalidateVisual();
				};
				container->AddControl(editSchema);
				_extraControls.push_back(editSchema);
				RegisterScrollable(editSchema);
				yOffset += 36;
			}

			{
				auto* container = GetContentContainer();
				const int width = GetContentWidthLocal();
				auto styleButtonText = [canvas]() {
					const auto& styleSheet = canvas->GetDocumentStyleSheet();
					return L"编辑文档样式表 ("
						+ std::to_wstring(styleSheet.Resources.size()) + L" 资源, "
						+ std::to_wstring(styleSheet.Rules.size()) + L" 规则)...";
				};
				auto* editStyles = new Button(
					styleButtonText(), 10, yOffset + 8, width - 20, 28);
				editStyles->OnMouseClick += [this, canvas, editStyles, styleButtonText](
					Control*, MouseEventArgs) {
					if (!this->ParentForm) return;
					StyleSheetEditorDialog dialog(canvas->GetDocumentStyleSheet());
					dialog.ShowDialog(this->ParentForm->Handle);
					if (!dialog.Applied
						|| dialog.ResultStyleSheet == canvas->GetDocumentStyleSheet()) return;

					auto result = std::move(dialog.ResultStyleSheet);
					std::wstring styleError;
					ExecutePropertyCommand(L"StyleSheet", [canvas, &styleError,
						result = std::move(result)]() mutable {
						(void)canvas->SetDocumentStyleSheet(std::move(result), &styleError);
					});
					if (!styleError.empty())
					{
						::MessageBoxW(this->ParentForm->Handle, styleError.c_str(),
							L"样式表无效", MB_OK | MB_ICONWARNING);
						return;
					}
					editStyles->Text = styleButtonText();
					editStyles->InvalidateVisual();
				};
				container->AddControl(editStyles);
				_extraControls.push_back(editStyles);
				RegisterScrollable(editStyles);
				yOffset += 36;
			}

			// 窗体事件（设计期映射，仅用于导出代码）
			for (const auto& ev : GetFormEventProperties())
			{
				bool enabled = _binding.IsFormEventEnabled(ev);
				CreateEventBoolPropertyItem(ev, enabled, yOffset);
			}
			Control::SetChildrenParentForm(this, this->ParentForm);
			return;
		}
		_titleLabel->Text = L"属性";
		return;
	}

	_titleLabel->Text = L"属性 - " + control->Name;

	auto targetControl = control->ControlInstance;
	int yOffset = GetContentTopLocal();

	// 基本属性
	CreatePropertyItem(L"Name", control->Name, yOffset);
	CreatePropertyItem(L"StyleId", targetControl->GetStyleId(), yOffset);
	CreatePropertyItem(L"StyleClasses", JoinStyleClasses(*targetControl), yOffset);
	CreatePropertyItem(L"Text", targetControl->Text, yOffset);
	if (control->Type == UIClass::UI_LinkLabel)
	{
		auto* link = (LinkLabel*)targetControl;
		CreateBoolPropertyItem(L"Visited", link->Visited, yOffset);
	}
	{
		auto* shared = _binding.GetDesignedFormSharedFont();
		::Font* f = targetControl->Font;
		bool isDefaultLike = false;
		if (shared)
			isDefaultLike = (f == shared);
		else
			isDefaultLike = (f == GetDefaultFontObject());
		std::wstring dispName = isDefaultLike ? kFontDefaultOption : (f ? f->FontName : kFontDefaultOption);
		CreateEnumPropertyItem(L"FontName", dispName, GetFontNameOptions(), yOffset);
		float fs = f ? f->FontSize : GetDefaultFontObject()->FontSize;
		CreateEnumPropertyItem(L"FontSize", FloatToText(fs), GetFontSizeOptions(), yOffset);
	}

	// 位置和大小
	CreatePropertyItem(L"X", std::to_wstring(targetControl->Location.x), yOffset);
	CreatePropertyItem(L"Y", std::to_wstring(targetControl->Location.y), yOffset);
	CreatePropertyItem(L"Width", std::to_wstring(targetControl->Size.cx), yOffset);
	CreatePropertyItem(L"Height", std::to_wstring(targetControl->Size.cy), yOffset);

	// 状态
	CreateBoolPropertyItem(L"Enabled", targetControl->Enable, yOffset);
	CreateBoolPropertyItem(L"Visible", targetControl->Visible, yOffset);

	// 常用外观/布局
	CreateColorPropertyItem(L"BackColor", targetControl->BackColor, yOffset);
	CreateColorPropertyItem(L"ForeColor", targetControl->ForeColor, yOffset);
	CreateColorPropertyItem(L"BorderColor", targetControl->BorderColor, yOffset);
	CreateBoolPropertyItem(L"ShowValidationBorder", targetControl->ShowValidationBorder, yOffset);
	CreateBoolPropertyItem(L"ShowValidationToolTip", targetControl->ShowValidationToolTip, yOffset);
	CreatePropertyItem(L"ValidationBorderThickness", FloatToText(targetControl->ValidationBorderThickness), yOffset);
	CreatePropertyItem(L"ValidationCornerRadius", FloatToText(targetControl->ValidationCornerRadius), yOffset);
	CreatePropertyItem(L"ValidationToolTipMaxWidth", FloatToText(targetControl->ValidationToolTipMaxWidth), yOffset);
	CreatePropertyItem(L"AccessibleDescription", targetControl->AccessibleDescription, yOffset);
	CreateThicknessPropertyItem(L"Margin", targetControl->Margin, yOffset);
	CreateThicknessPropertyItem(L"Padding", targetControl->Padding, yOffset);
	CreateAnchorPropertyItem(L"Anchor", targetControl->AnchorStyles, yOffset);
	CreateEnumPropertyItem(L"HAlign", HAlignToText(targetControl->HAlign), { L"Left", L"Center", L"Right", L"Stretch" }, yOffset);
	CreateEnumPropertyItem(L"VAlign", VAlignToText(targetControl->VAlign), { L"Top", L"Center", L"Bottom", L"Stretch" }, yOffset);
		CreatePropertyItem(L"ZIndex", std::to_wstring(targetControl->ZIndex), yOffset);
	if (targetControl->Parent && targetControl->Parent->Type() == UIClass::UI_DockPanel)
		CreateEnumPropertyItem(L"Dock", DockToText(targetControl->DockPosition), { L"Fill", L"Left", L"Top", L"Right", L"Bottom" }, yOffset);
	if (targetControl->Parent && targetControl->Parent->Type() == UIClass::UI_GridPanel)
	{
		CreatePropertyItem(L"GridRow", std::to_wstring(targetControl->GridRow), yOffset);
		CreatePropertyItem(L"GridColumn", std::to_wstring(targetControl->GridColumn), yOffset);
		CreatePropertyItem(L"GridRowSpan", std::to_wstring(targetControl->GridRowSpan), yOffset);
		CreatePropertyItem(L"GridColumnSpan", std::to_wstring(targetControl->GridColumnSpan), yOffset);
	}
	if (control->Type == UIClass::UI_ProgressBar)
	{
		auto* progressBar = (ProgressBar*)targetControl;
		CreateFloatSliderPropertyItem(L"PercentageValue", progressBar->PercentageValue, 0.0f, 1.0f, 0.01f, yOffset);
	}
	if (control->Type == UIClass::UI_LoadingRing)
	{
		auto* loadingRing = (LoadingRing*)targetControl;
		CreateBoolPropertyItem(L"Active", loadingRing->Active, yOffset);
	}
	if (control->Type == UIClass::UI_ProgressRing)
	{
		auto* progressRing = (ProgressRing*)targetControl;
		CreateFloatSliderPropertyItem(L"PercentageValue", progressRing->PercentageValue, 0.0f, 1.0f, 0.01f, yOffset);
		CreateBoolPropertyItem(L"ShowPercentage", progressRing->ShowPercentage, yOffset);
	}
	if (control->Type == UIClass::UI_DateTimePicker)
	{
		auto* dateTimePicker = (DateTimePicker*)targetControl;
		std::wstring mode = L"DateTime";
		switch (dateTimePicker->Mode)
		{
		case DateTimePickerMode::DateOnly: mode = L"DateOnly"; break;
		case DateTimePickerMode::TimeOnly: mode = L"TimeOnly"; break;
		case DateTimePickerMode::DateTime: default: mode = L"DateTime"; break;
		}
		CreateEnumPropertyItem(L"Mode", mode, { L"DateOnly", L"TimeOnly", L"DateTime" }, yOffset);
		CreateBoolPropertyItem(L"AllowDateSelection", dateTimePicker->AllowDateSelection, yOffset);
		CreateBoolPropertyItem(L"AllowTimeSelection", dateTimePicker->AllowTimeSelection, yOffset);
		CreateBoolPropertyItem(L"AllowModeSwitch", dateTimePicker->AllowModeSwitch, yOffset);
		CreateBoolPropertyItem(L"Expand", dateTimePicker->Expand, yOffset);
	}
	if (control->Type == UIClass::UI_PictureBox)
	{
		CreateEnumPropertyItem(L"SizeMode", ImageSizeModeToText(targetControl->SizeMode), { L"Normal", L"CenterImage", L"Stretch", L"Zoom" }, yOffset);
	}
	if (control->Type == UIClass::UI_TreeView)
	{
		auto* treeView = (TreeView*)targetControl;
		CreateColorPropertyItem(L"SelectedBackColor", treeView->SelectedBackColor, yOffset);
		CreateColorPropertyItem(L"UnderMouseItemBackColor", treeView->UnderMouseItemBackColor, yOffset);
		CreateColorPropertyItem(L"SelectedForeColor", treeView->SelectedForeColor, yOffset);
	}
	if (control->Type == UIClass::UI_MediaPlayer)
	{
		std::wstring mediaFile;
		auto it = control->DesignStrings.find(L"mediaFile");
		if (it != control->DesignStrings.end()) mediaFile = it->second;
		CreatePropertyItem(L"MediaFile", mediaFile, yOffset);
	}

	CreateMetadataPropertyItems(control, yOffset);

	// 数据绑定使用结构化编辑器；目标属性列表完全来自运行时元数据。
	if (!BindingPropertyRegistry::GetProperties(*targetControl).empty())
	{
		auto* container = GetContentContainer();
		const int width = GetContentWidthLocal();
		auto bindingButtonText = [control]() {
			return L"编辑数据绑定 (" + std::to_wstring(control->DataBindings.size()) + L")...";
		};
		auto* editBindings = new Button(bindingButtonText(), 10, yOffset + 8, width - 20, 28);
		editBindings->OnMouseClick += [this, editBindings, bindingButtonText](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;

			const auto* canvas = _binding.GetCanvas();
			BindingEditorDialog dialog(
				currentControl->ControlInstance,
				currentControl->DataBindings,
				canvas ? canvas->GetDataContextSchema() : DesignerDataContextSchema{},
				canvas ? canvas->GetDesignDataContext().get() : nullptr);
			dialog.ShowDialog(this->ParentForm->Handle);
			if (!dialog.Applied || dialog.ResultBindings == currentControl->DataBindings) return;

			auto result = std::move(dialog.ResultBindings);
			ExecutePropertyCommand(L"DataBindings", [currentControl, result = std::move(result)]() mutable {
				currentControl->DataBindings = std::move(result);
			});
			editBindings->Text = bindingButtonText();
			editBindings->InvalidateVisual();
		};
		container->AddControl(editBindings);
		_extraControls.push_back(editBindings);
		RegisterScrollable(editBindings);
		yOffset += 36;
	}

	// 事件（设计期映射，仅用于导出代码）
	for (const auto& ev : GetEventPropertiesFor(control->Type))
	{
		bool enabled = (control->EventHandlers.find(ev) != control->EventHandlers.end());
		CreateEventBoolPropertyItem(ev, enabled, yOffset);
	}

	// 高级编辑入口（模态窗口）
	if (control->Type == UIClass::UI_ComboBox)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑下拉项...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto comboBox = dynamic_cast<ComboBox*>(currentControl->ControlInstance);
			if (!comboBox) return;
			ComboBoxItemsEditorDialog dlg(comboBox);
			dlg.ShowDialog(this->ParentForm->Handle);
			if (dlg.Applied)
			{
				std::wstring canonicalName;
				DesignerStyleValue selectedValue;
				if (DesignerPropertyCatalog::CaptureValue(
					*comboBox, L"SelectedIndex", &canonicalName, selectedValue))
				{
					SetTrackedMetadataProperty(
						*currentControl, std::move(canonicalName),
						std::move(selectedValue));
				}
			}
			comboBox->InvalidateVisual();
			};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_GridView)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑列...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto gridView = dynamic_cast<GridView*>(currentControl->ControlInstance);
			if (!gridView) return;
			GridViewColumnsEditorDialog dlg(gridView);
			dlg.ShowDialog(this->ParentForm->Handle);
			gridView->InvalidateVisual();
			};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_TabControl)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑页...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto tabControl = dynamic_cast<TabControl*>(currentControl->ControlInstance);
			if (!tabControl) return;
			TabControlPagesEditorDialog dlg(tabControl);
			// 如果删除页，需要同步移除该页下的 DesignerControl 以避免悬挂
			dlg.OnBeforeDeletePage = [this](Control* page) {
				if (page) _binding.RemoveDesignerControlsInSubtree(page);
				};
			dlg.ShowDialog(this->ParentForm->Handle);
			tabControl->InvalidateVisual();
			};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_ToolBar)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑文字按钮...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto toolBar = dynamic_cast<ToolBar*>(currentControl->ControlInstance);
			if (!toolBar) return;
			ToolBarButtonsEditorDialog dlg(toolBar);
			// 如果删除按钮控件，需要同步移除 DesignerControl
			dlg.OnBeforeDeleteButton = [this](Control* btn) {
				if (btn) _binding.RemoveDesignerControlsInSubtree(btn);
				};
			dlg.ShowDialog(this->ParentForm->Handle);
			toolBar->InvalidateVisual();
			};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_TreeView)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑节点...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto treeView = dynamic_cast<TreeView*>(currentControl->ControlInstance);
			if (!treeView) return;
			TreeViewNodesEditorDialog dlg(treeView);
			dlg.ShowDialog(this->ParentForm->Handle);
			treeView->InvalidateVisual();
			};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_GridPanel)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑行/列...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto gridPanel = dynamic_cast<GridPanel*>(currentControl->ControlInstance);
			if (!gridPanel) return;
			GridPanelDefinitionsEditorDialog dlg(gridPanel);
			dlg.ShowDialog(this->ParentForm->Handle);
			gridPanel->InvalidateVisual();
			};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_Menu)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑菜单项...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto m = dynamic_cast<Menu*>(currentControl->ControlInstance);
			if (!m) return;
			MenuItemsEditorDialog dlg(m);
			dlg.ShowDialog(this->ParentForm->Handle);
			m->InvalidateVisual();
		};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_StatusBar)
	{
		auto* container = GetContentContainer();
		int width = GetContentWidthLocal();
		auto editBtn = new Button(L"编辑分段...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto statusBar = dynamic_cast<StatusBar*>(currentControl->ControlInstance);
			if (!statusBar) return;
			StatusBarPartsEditorDialog dlg(statusBar);
			dlg.ShowDialog(this->ParentForm->Handle);
			statusBar->InvalidateVisual();
		};
		container->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		RegisterScrollable(editBtn);
		yOffset += 36;
	}

	// 确保所有新创建的子控件的ParentForm都被正确设置
	Control::SetChildrenParentForm(this, this->ParentForm);
}

void PropertyGrid::Clear()
{
	CommitGroupedFloatSliderEdit();

	auto deleteOwnedControl = [this](Control* c) {
		if (!c) return;
		if (c->Parent && c->Parent->DeleteControl(c))
			return;
		if (this->DeleteControl(c))
			return;
		// 兼容尚未挂载或历史异常状态下的编辑器控件。
		delete c;
	};

	// 移除所有属性项（保留标题）
	for (auto item : _items)
	{
		if (item->NameLabel)
		{
			deleteOwnedControl(item->NameLabel);
			item->NameLabel = nullptr;
		}
		if (item->ValueControl)
		{
			deleteOwnedControl(item->ValueControl);
			item->ValueControl = nullptr;
		}
		else
		{
			// 兜底：某些条目可能不走 ValueControl（历史代码/异常场景）
			if (item->ValueTextBox)
			{
				deleteOwnedControl(item->ValueTextBox);
				item->ValueTextBox = nullptr;
			}
			if (item->ValueCheckBox)
			{
				deleteOwnedControl(item->ValueCheckBox);
				item->ValueCheckBox = nullptr;
			}
		}
		delete item;
	}
	_items.clear();

	for (auto* c : _extraControls)
	{
		if (!c) continue;
		deleteOwnedControl(c);
	}
	_extraControls.clear();
	_scrollEntries.clear();
	_scrollOffsetY = 0;
	_contentHeight = 0;
	_draggingScrollThumb = false;
}
