#include "PropertyGrid.h"
#include "../CUI/GUI/Form.h"
#include "ComboBoxItemsEditorDialog.h"
#include "GridViewColumnsEditorDialog.h"
#include "TabControlPagesEditorDialog.h"
#include "ToolBarButtonsEditorDialog.h"
#include "TreeViewNodesEditorDialog.h"
#include "GridPanelDefinitionsEditorDialog.h"
#include "MenuItemsEditorDialog.h"
#include "StatusBarPartsEditorDialog.h"
#include "DesignerCanvas.h"
#include "DesignerCore/Commands/UpdatePropertyCommand.h"
#include "../CUI/GUI/LinkLabel.h"
#include "../CUI/GUI/ComboBox.h"
#include "../CUI/GUI/LoadingRing.h"
#include "../CUI/GUI/Slider.h"
#include "../CUI/GUI/ProgressBar.h"
#include "../CUI/GUI/ProgressRing.h"
#include "../CUI/GUI/PictureBox.h"
#include "../CUI/GUI/DateTimePicker.h"
#include "../CUI/GUI/GroupBox.h"
#include "../CUI/GUI/ScrollView.h"
#include "../CUI/GUI/TreeView.h"
#include "../CUI/GUI/TabControl.h"
#include "../CUI/GUI/ToolBar.h"
#include "../CUI/GUI/StatusBar.h"
#include "../CUI/GUI/MediaPlayer.h"
#include "../CUI/GUI/SplitContainer.h"
#include "../CUI/GUI/Layout/StackPanel.h"
#include "../CUI/GUI/Layout/WrapPanel.h"
#include "../CUI/GUI/Layout/DockPanel.h"
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
	static std::wstring TrimWs(const std::wstring& s);

	static const std::wstring kFontDefaultOption = L"<Default>";

	static std::wstring FloatToText(float v)
	{
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(2) << v;
		return oss.str();
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

		int a = toByte(c.a);
		if (a != 255)
			oss << std::setw(2) << a;

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

	static bool TryParseOrientation(const std::wstring& s, ::Orientation& out)
	{
		auto t = TrimWs(s);
		if (t == L"Horizontal") { out = Orientation::Horizontal; return true; }
		if (t == L"Vertical") { out = Orientation::Vertical; return true; }
		return false;
	}

	static std::wstring OrientationToText(::Orientation o)
	{
		switch (o)
		{
		case Orientation::Horizontal: return L"Horizontal";
		case Orientation::Vertical: return L"Vertical";
		default: return L"Vertical";
		}
	}

	static std::wstring TabAnimationModeToText(TabControlAnimationMode mode)
	{
		switch (mode)
		{
		case TabControlAnimationMode::SlideHorizontal: return L"SlideHorizontal";
		case TabControlAnimationMode::DirectReplace:
		default: return L"DirectReplace";
		}
	}

	static bool TryParseImageSizeMode(const std::wstring& s, ::ImageSizeMode& out)
	{
		auto t = TrimWs(s);
		if (t == L"Normal") { out = ImageSizeMode::Normal; return true; }
		if (t == L"CenterImage") { out = ImageSizeMode::CenterImage; return true; }
		if (t == L"Stretch") { out = ImageSizeMode::StretchIamge; return true; }
		if (t == L"Zoom") { out = ImageSizeMode::Zoom; return true; }
		// 兼容旧拼写
		if (t == L"StretchIamge") { out = ImageSizeMode::StretchIamge; return true; }
		return false;
	}

	static std::wstring ImageSizeModeToText(::ImageSizeMode m)
	{
		switch (m)
		{
		case ImageSizeMode::Normal: return L"Normal";
		case ImageSizeMode::CenterImage: return L"CenterImage";
		case ImageSizeMode::StretchIamge: return L"Stretch";
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
			L"OnMouseLeaved",
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
		out.push_back(L"OnMouseLeaved");
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
			break;
		case UIClass::UI_TreeView:
			out.push_back(L"ScrollChanged");
			out.push_back(L"SelectionChanged");
			break;
		case UIClass::UI_Slider:
			out.push_back(L"OnValueChanged");
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
			L"OnMouseLeaved",
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

	auto cb = new CheckBox(eventName, 10, yOffset);
	cb->Size = { width - 20, 20 };
	cb->Checked = enabled;
	cb->ParentForm = this->ParentForm;
	cb->OnMouseClick += [this, eventName](Control* sender, MouseEventArgs) {
		auto box = (CheckBox*)sender;
		UpdatePropertyFromBool(eventName, box->Checked);
	};
	container->AddControl(cb);
	RegisterScrollable(cb);
	_items.push_back(new PropertyItem(eventName, nullptr, cb));
	yOffset += 25;
}

PropertyGrid::PropertyGrid(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
	this->Boder = 1.0f;

	// 标题
	_titleLabel = new Label(L"属性", 10, 10);
	_titleLabel->Size = { width - 20, 25 };
	_titleLabel->Font = new ::Font(L"Microsoft YaHei", 16.0f);
	this->AddControl(_titleLabel);

	_scrollView = new ScrollView(0, _contentTop, width, std::max(0, height - _contentTop));
	_scrollView->BackColor = this->BackColor;
	_scrollView->Boder = 0.0f;
	_scrollView->MouseWheelStep = 25;
	this->AddControl(_scrollView);

	_contentHost = new Panel(0, 0, width, std::max(0, height - _contentTop));
	_contentHost->BackColor = D2D1::ColorF(0, 0, 0, 0);
	_contentHost->Boder = 0.0f;
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

bool PropertyGrid::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (message == WM_KEYDOWN)
	{
		auto* canvas = _binding.GetCanvas();
		if (canvas && (GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			if (wParam == 'Z')
			{
				if (canvas->UndoCommand()) return true;
			}
			else if (wParam == 'Y')
			{
				if (canvas->RedoCommand()) return true;
			}
		}
	}
	return Panel::ProcessMessage(message, wParam, lParam, xof, yof);
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
	panel->Boder = 0.0f;
	panel->ParentForm = this->ParentForm;

	const int btnW = 28;
	const int gap = 6;
	int textW = valueW - btnW - gap;
	if (textW < 40) textW = 40;

	auto btn = new Button(L"...", 0, -1, btnW, 22);
	btn->ParentForm = this->ParentForm;
	btn->Boder = 1.0f;
	btn->BolderColor = Colors::DimGrey;
	auto refreshButtonColor = [btn](const D2D1_COLOR_F& c) {
		btn->BackColor = c;
		float luminance = c.r * 0.299f + c.g * 0.587f + c.b * 0.114f;
		btn->ForeColor = (luminance < 0.5f || c.a < 0.5f) ? Colors::White : Colors::Black;
		btn->PostRender();
	};
	refreshButtonColor(value);

	auto tb = new TextBox(L"", btnW + gap, 0, textW, 20);
	tb->Text = ColorToText(value);
	tb->ParentForm = this->ParentForm;
	tb->OnLostFocus += [this, propertyName, tb, refreshButtonColor](Control*) {
		UpdatePropertyFromTextBox(propertyName, tb->Text);
		D2D1_COLOR_F c{};
		if (TryParseColor(tb->Text, c))
		{
			refreshButtonColor(c);
		}
	};

	btn->OnMouseClick += [this, propertyName, tb, refreshButtonColor](Control*, MouseEventArgs) {
		if (!this->ParentForm) return;
		D2D1_COLOR_F cur{};
		if (!TryParseColor(tb->Text, cur)) cur = D2D1::ColorF(0, 0, 0, 1);
		D2D1_COLOR_F picked{};
		if (PickColorWithDialog(this->ParentForm->Handle, cur, picked))
		{
			refreshButtonColor(picked);
			tb->Text = ColorToText(picked);
			UpdatePropertyFromTextBox(propertyName, tb->Text);
		}
	};

	panel->AddControl(tb);
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
	panel->Boder = 0.0f;
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
		auto cb = (CheckBox*)sender;
		UpdatePropertyFromBool(propertyName, cb->Checked);
		};

	container->AddControl(valueCheckBox);
	RegisterScrollable(valueCheckBox);

	auto item = new PropertyItem(propertyName, nameLabel, valueCheckBox);
	_items.push_back(item);

	yOffset += 25;
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
	panel->Boder = 0.0f;
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

	int idx = -1;
	for (int i = 0; i < valueCombo->Items.size(); i++)
	{
		if (EnumOptionMatchesValue(valueCombo->Items[i], value)) { idx = i; break; }
	}
	valueCombo->SelectedIndex = idx >= 0 ? idx : 0;
	if (valueCombo->Items.size() > 0 && idx >= 0 && idx < valueCombo->Items.size())
		valueCombo->Text = valueCombo->Items[idx];
	else
		valueCombo->Text = value;

	valueCombo->OnSelectionChanged += [this, propertyName](Control* sender) {
		auto cb = (ComboBox*)sender;
		UpdatePropertyFromTextBox(propertyName, cb->Text);
		};

	container->AddControl(valueCombo);
	RegisterScrollable(valueCombo);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)valueCombo);
	_items.push_back(item);

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

void PropertyGrid::ApplyFloatPropertyValue(Control* ctrl, const std::wstring& propertyName, float value)
{
	if (!ctrl)
	{
		return;
	}

	if (propertyName == L"PercentageValue")
	{
		float v = std::clamp(value, 0.0f, 1.0f);
		if (ctrl->Type() == UIClass::UI_ProgressBar)
		{
			auto* pb = (ProgressBar*)ctrl;
			pb->PercentageValue = v;
		}
		else if (ctrl->Type() == UIClass::UI_ProgressRing)
		{
			auto* pr = (ProgressRing*)ctrl;
			pr->PercentageValue = v;
		}
	}
	else if (propertyName == L"Volume")
	{
		if (ctrl->Type() == UIClass::UI_MediaPlayer)
		{
			float v = std::clamp(value, 0.0f, 1.0f);
			((MediaPlayer*)ctrl)->Volume = (double)v;
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

	auto* ctrl = currentControl->ControlInstance;
	try
	{
		ApplyFloatPropertyValue(ctrl, propertyName, value);
	}
	catch (...)
	{
		return;
	}

	_binding.NotifyControlChanged(ctrl);
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
				auto s = canvas->GetDesignedFormSize();
				s.cx = std::stoi(value);
				canvas->SetDesignedFormSize(s);
			}
			else if (propertyName == L"Height")
			{
				auto s = canvas->GetDesignedFormSize();
				s.cy = std::stoi(value);
				canvas->SetDesignedFormSize(s);
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

	auto ctrl = currentControl->ControlInstance;
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
		else if (propertyName == L"Text")
		{
			ctrl->Text = value;
		}
		else if (propertyName == L"FontName")
		{
			auto v = TrimWs(value);
			if (v == kFontDefaultOption || v.empty())
			{
				if (_binding.GetDesignedFormSharedFont())
					ctrl->SetFontEx(_binding.GetDesignedFormSharedFont(), false);
				else
					ctrl->SetFontEx(nullptr, false);
			}
			else
			{
				float curSize = ctrl->Font ? ctrl->Font->FontSize : GetDefaultFontObject()->FontSize;
				ctrl->Font = new ::Font(v, curSize);
			}
		}
		else if (propertyName == L"FontSize")
		{
			float fs = 0.0f;
			if (!TryParseFloatWs(TrimWs(value), fs))
				throw std::exception();
			if (fs < 1.0f) fs = 1.0f;
			if (fs > 200.0f) fs = 200.0f;
			std::wstring curName = ctrl->Font ? ctrl->Font->FontName : GetDefaultFontObject()->FontName;
			ctrl->Font = new ::Font(curName, fs);
		}
		else if (propertyName == L"X")
		{
			auto loc = ctrl->Location;
			loc.x = std::stoi(value);
			ctrl->Location = loc;
		}
		else if (propertyName == L"Y")
		{
			auto loc = ctrl->Location;
			loc.y = std::stoi(value);
			ctrl->Location = loc;
		}
		else if (propertyName == L"Width")
		{
			auto size = ctrl->Size;
			size.cx = std::stoi(value);
			ctrl->Size = size;
		}
		else if (propertyName == L"Height")
		{
			auto size = ctrl->Size;
			size.cy = std::stoi(value);
			ctrl->Size = size;
		}
		else if (propertyName == L"Enabled")
		{
			ctrl->Enable = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"Visible")
		{
			ctrl->Visible = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"BackColor")
		{
			D2D1_COLOR_F c;
			if (TryParseColor(value, c)) ctrl->BackColor = c;
		}
		else if (propertyName == L"ForeColor")
		{
			D2D1_COLOR_F c;
			if (TryParseColor(value, c)) ctrl->ForeColor = c;
		}
		else if (propertyName == L"BolderColor")
		{
			D2D1_COLOR_F c;
			if (TryParseColor(value, c)) ctrl->BolderColor = c;
		}
		else if (propertyName == L"Margin")
		{
			Thickness t;
			if (TryParseThickness(value, t)) ctrl->Margin = t;
		}
		else if (propertyName == L"Padding")
		{
			Thickness t;
			if (TryParseThickness(value, t)) ctrl->Padding = t;
		}
		else if (propertyName == L"HAlign")
		{
			::HorizontalAlignment a;
			if (TryParseHAlign(value, a)) ctrl->HAlign = a;
		}
		else if (propertyName == L"VAlign")
		{
			::VerticalAlignment a;
			if (TryParseVAlign(value, a)) ctrl->VAlign = a;
		}
		else if (propertyName == L"Dock")
		{
			::Dock d;
			if (TryParseDock(value, d)) ctrl->DockPosition = d;
		}
		else if (propertyName == L"GridRow")
		{
			ctrl->GridRow = std::stoi(value);
		}
		else if (propertyName == L"GridColumn")
		{
			ctrl->GridColumn = std::stoi(value);
		}
		else if (propertyName == L"GridRowSpan")
		{
			ctrl->GridRowSpan = std::stoi(value);
		}
		else if (propertyName == L"GridColumnSpan")
		{
			ctrl->GridColumnSpan = std::stoi(value);
		}
		else if (propertyName == L"SelectedIndex")
		{
			if (ctrl->Type() == UIClass::UI_TabControl)
			{
				auto* tc = (TabControl*)ctrl;
				tc->SelectedIndex = std::stoi(value);
				tc->PostRender();
			}
			else if (ctrl->Type() == UIClass::UI_ComboBox)
			{
				auto* cb = (ComboBox*)ctrl;
				cb->SelectedIndex = std::stoi(value);
				if (cb->Items.size() > 0 && cb->SelectedIndex >= 0 && cb->SelectedIndex < cb->Items.size())
					cb->Text = cb->Items[cb->SelectedIndex];
			}
		}
		else if (propertyName == L"ExpandCount")
		{
			if (ctrl->Type() == UIClass::UI_ComboBox)
			{
				auto* cb = (ComboBox*)ctrl;
				cb->ExpandCount = std::max(1, std::stoi(value));
				cb->PostRender();
			}
		}
		else if (propertyName == L"AnimationMode")
		{
			if (ctrl->Type() == UIClass::UI_TabControl)
			{
				auto* tc = (TabControl*)ctrl;
				auto v = TrimWs(value);
				if (v == L"SlideHorizontal") tc->AnimationMode = TabControlAnimationMode::SlideHorizontal;
				else tc->AnimationMode = TabControlAnimationMode::DirectReplace;
				tc->PostRender();
			}
		}
		else if (propertyName == L"Mode")
		{
			if (ctrl->Type() == UIClass::UI_DateTimePicker)
			{
				auto* dtp = (DateTimePicker*)ctrl;
				auto v = TrimWs(value);
				if (v == L"DateOnly") dtp->Mode = DateTimePickerMode::DateOnly;
				else if (v == L"TimeOnly") dtp->Mode = DateTimePickerMode::TimeOnly;
				else dtp->Mode = DateTimePickerMode::DateTime;
			}
		}
		else if (propertyName == L"TitleHeight")
		{
			if (ctrl->Type() == UIClass::UI_TabControl)
			{
				auto* tc = (TabControl*)ctrl;
				tc->TitleHeight = std::stoi(value);
			}
		}
		else if (propertyName == L"TitleWidth")
		{
			if (ctrl->Type() == UIClass::UI_TabControl)
			{
				auto* tc = (TabControl*)ctrl;
				tc->TitleWidth = std::stoi(value);
			}
		}
		else if (propertyName == L"Orientation")
		{
			::Orientation o;
			if (TryParseOrientation(value, o))
			{
				if (ctrl->Type() == UIClass::UI_StackPanel) ((StackPanel*)ctrl)->SetOrientation(o);
				else if (ctrl->Type() == UIClass::UI_WrapPanel) ((WrapPanel*)ctrl)->SetOrientation(o);
			}
		}
		else if (propertyName == L"SplitOrientation")
		{
			if (ctrl->Type() == UIClass::UI_SplitContainer)
			{
				::Orientation o;
				if (TryParseOrientation(value, o))
					((SplitContainer*)ctrl)->SplitOrientation = o;
			}
		}
		else if (propertyName == L"SizeMode")
		{
			if (ctrl->Type() == UIClass::UI_PictureBox)
			{
				::ImageSizeMode m;
				if (TryParseImageSizeMode(value, m)) ctrl->SizeMode = m;
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
		else if (propertyName == L"RenderMode")
		{
			if (ctrl->Type() == UIClass::UI_MediaPlayer)
			{
				auto* mp = (MediaPlayer*)ctrl;
				auto v = TrimWs(value);
				if (v == L"Fit") mp->RenderMode = MediaPlayer::VideoRenderMode::Fit;
				else if (v == L"Fill") mp->RenderMode = MediaPlayer::VideoRenderMode::Fill;
				else if (v == L"Stretch") mp->RenderMode = MediaPlayer::VideoRenderMode::Stretch;
				else if (v == L"Center") mp->RenderMode = MediaPlayer::VideoRenderMode::Center;
				else if (v == L"UniformToFill") mp->RenderMode = MediaPlayer::VideoRenderMode::UniformToFill;
			}
		}
		else if (propertyName == L"PlaybackRate")
		{
			if (ctrl->Type() == UIClass::UI_MediaPlayer)
			{
				float r = 1.0f;
				if (TryParseFloatWs(TrimWs(value), r))
					((MediaPlayer*)ctrl)->PlaybackRate = r;
			}
		}
		else if (propertyName == L"SelectedBackColor")
		{
			if (ctrl->Type() == UIClass::UI_TreeView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((TreeView*)ctrl)->SelectedBackColor = c;
			}
		}
		else if (propertyName == L"UnderMouseItemBackColor")
		{
			if (ctrl->Type() == UIClass::UI_TreeView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((TreeView*)ctrl)->UnderMouseItemBackColor = c;
			}
		}
		else if (propertyName == L"SelectedForeColor")
		{
			if (ctrl->Type() == UIClass::UI_TreeView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((TreeView*)ctrl)->SelectedForeColor = c;
			}
		}
		else if (propertyName == L"ScrollBackColor")
		{
			if (ctrl->Type() == UIClass::UI_ScrollView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((ScrollView*)ctrl)->ScrollBackColor = c;
			}
		}
		else if (propertyName == L"ScrollForeColor")
		{
			if (ctrl->Type() == UIClass::UI_ScrollView)
			{
				D2D1_COLOR_F c;
				if (TryParseColor(value, c)) ((ScrollView*)ctrl)->ScrollForeColor = c;
			}
		}
		else if (propertyName == L"Spacing")
		{
			if (ctrl->Type() == UIClass::UI_StackPanel)
				((StackPanel*)ctrl)->SetSpacing(std::stof(value));
		}
		else if (propertyName == L"CaptionMarginLeft")
		{
			if (ctrl->Type() == UIClass::UI_GroupBox)
				((GroupBox*)ctrl)->CaptionMarginLeft = std::stof(value);
		}
		else if (propertyName == L"CaptionPaddingX")
		{
			if (ctrl->Type() == UIClass::UI_GroupBox)
				((GroupBox*)ctrl)->CaptionPaddingX = std::stof(value);
		}
		else if (propertyName == L"CaptionPaddingY")
		{
			if (ctrl->Type() == UIClass::UI_GroupBox)
				((GroupBox*)ctrl)->CaptionPaddingY = std::stof(value);
		}
		else if (propertyName == L"ItemWidth")
		{
			if (ctrl->Type() == UIClass::UI_WrapPanel)
				((WrapPanel*)ctrl)->SetItemWidth(std::stof(value));
		}
		else if (propertyName == L"ItemHeight")
		{
			if (ctrl->Type() == UIClass::UI_WrapPanel)
				((WrapPanel*)ctrl)->SetItemHeight(std::stof(value));
		}
		else if (propertyName == L"Gap")
		{
			if (ctrl->Type() == UIClass::UI_ToolBar)
				((ToolBar*)ctrl)->Gap = std::stoi(value);
			else if (ctrl->Type() == UIClass::UI_StatusBar)
				((StatusBar*)ctrl)->Gap = std::stoi(value);
		}
		else if (propertyName == L"Padding")
		{
			if (ctrl->Type() == UIClass::UI_ToolBar)
				((ToolBar*)ctrl)->Padding = std::stoi(value);
			else if (ctrl->Type() == UIClass::UI_StatusBar)
				((StatusBar*)ctrl)->Padding = std::stoi(value);
		}
		else if (propertyName == L"ItemHeight")
		{
			if (ctrl->Type() == UIClass::UI_ToolBar)
				((ToolBar*)ctrl)->ItemHeight = std::stoi(value);
		}
		else if (propertyName == L"Min")
		{
			if (ctrl->Type() == UIClass::UI_Slider)
				((Slider*)ctrl)->Min = std::stof(value);
		}
		else if (propertyName == L"Max")
		{
			if (ctrl->Type() == UIClass::UI_Slider)
				((Slider*)ctrl)->Max = std::stof(value);
		}
		else if (propertyName == L"Value")
		{
			if (ctrl->Type() == UIClass::UI_Slider)
				((Slider*)ctrl)->Value = std::stof(value);
		}
		else if (propertyName == L"Step")
		{
			if (ctrl->Type() == UIClass::UI_Slider)
				((Slider*)ctrl)->Step = std::stof(value);
		}
		else if (propertyName == L"MouseWheelStep")
		{
			if (ctrl->Type() == UIClass::UI_ScrollView)
				((ScrollView*)ctrl)->MouseWheelStep = std::stoi(value);
		}
		else if (propertyName == L"SplitterDistance")
		{
			if (ctrl->Type() == UIClass::UI_SplitContainer)
				((SplitContainer*)ctrl)->SetSplitterDistance(std::stoi(value));
		}
		else if (propertyName == L"SplitterWidth")
		{
			if (ctrl->Type() == UIClass::UI_SplitContainer)
				((SplitContainer*)ctrl)->SplitterWidth = std::stoi(value);
		}
		else if (propertyName == L"Panel1MinSize")
		{
			if (ctrl->Type() == UIClass::UI_SplitContainer)
				((SplitContainer*)ctrl)->Panel1MinSize = std::stoi(value);
		}
		else if (propertyName == L"Panel2MinSize")
		{
			if (ctrl->Type() == UIClass::UI_SplitContainer)
				((SplitContainer*)ctrl)->Panel2MinSize = std::stoi(value);
		}
		else if (propertyName == L"ContentWidth")
		{
			if (ctrl->Type() == UIClass::UI_ScrollView)
			{
				auto* sv = (ScrollView*)ctrl;
				auto cs = sv->ContentSize;
				cs.cx = std::stoi(value);
				sv->ContentSize = cs;
			}
		}
		else if (propertyName == L"ContentHeight")
		{
			if (ctrl->Type() == UIClass::UI_ScrollView)
			{
				auto* sv = (ScrollView*)ctrl;
				auto cs = sv->ContentSize;
				cs.cy = std::stoi(value);
				sv->ContentSize = cs;
			}
		}
	}
	catch (...)
	{
	}

	_binding.NotifyControlChanged(ctrl);
	});
}

void PropertyGrid::UpdatePropertyFromFloat(std::wstring propertyName, float value)
{
	ExecutePropertyCommand(propertyName, [this, propertyName, value]() {
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance) return;
	auto ctrl = currentControl->ControlInstance;

	try
	{
		ApplyFloatPropertyValue(ctrl, propertyName, value);
	}
	catch (...) {}

	_binding.NotifyControlChanged(ctrl);
	});
}

void PropertyGrid::UpdateAnchorFromChecks(bool left, bool top, bool right, bool bottom)
{
	ExecutePropertyCommand(L"Anchor", [this, left, top, right, bottom]() {
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance) return;
	auto* ctrl = currentControl->ControlInstance;

	uint8_t a = AnchorStyles::None;
	if (left) a |= AnchorStyles::Left;
	if (top) a |= AnchorStyles::Top;
	if (right) a |= AnchorStyles::Right;
	if (bottom) a |= AnchorStyles::Bottom;
	_binding.ApplyAnchorStylesKeepingBounds(ctrl, a);

	_binding.NotifyControlChanged(ctrl);
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
	auto ctrl = currentControl->ControlInstance;

	// 事件：仅更新设计期映射
	if (IsEventPropertyName(propertyName))
	{
		if (value)
			currentControl->EventHandlers[propertyName] = L"1";
			if (propertyName == L"Active")
			{
				if (ctrl->Type() == UIClass::UI_LoadingRing)
				{
					((LoadingRing*)ctrl)->Active = value;
				}
			}
			else if (propertyName == L"ShowPercentage")
			{
				if (ctrl->Type() == UIClass::UI_ProgressRing)
				{
					((ProgressRing*)ctrl)->ShowPercentage = value;
				}
			}
			else
			{
				if (propertyName == L"Checked")
				{
					ctrl->Checked = value;
				}
				else if (propertyName == L"Enable")
				{
					ctrl->Enable = value;
				}
				else if (propertyName == L"Visible")
				{
					ctrl->Visible = value;
				}
				else if (propertyName == L"Expand")
				{
					if (ctrl->Type() == UIClass::UI_DateTimePicker)
					{
						((DateTimePicker*)ctrl)->SetExpanded(value);
					}
				}
				else if (propertyName == L"ShowInTaskBar" || propertyName == L"TopMost" || propertyName == L"AllowResize" ||
					propertyName == L"VisibleHead" || propertyName == L"MinBox" || propertyName == L"MaxBox" ||
					propertyName == L"CloseBox" || propertyName == L"CenterTitle")
				{
					_binding.ApplyFormBoolProperty(propertyName, value);
					return;
				}
			}
	}
	else if (propertyName == L"AutoPlay")
	{
		if (ctrl->Type() == UIClass::UI_MediaPlayer)
			((MediaPlayer*)ctrl)->AutoPlay = value;
	}
	else if (propertyName == L"Loop")
	{
		if (ctrl->Type() == UIClass::UI_MediaPlayer)
			((MediaPlayer*)ctrl)->Loop = value;
	}
	else if (propertyName == L"AllowDateSelection")
	{
		if (ctrl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)ctrl)->AllowDateSelection = value;
	}
	else if (propertyName == L"AllowTimeSelection")
	{
		if (ctrl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)ctrl)->AllowTimeSelection = value;
	}
	else if (propertyName == L"AllowModeSwitch")
	{
		if (ctrl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)ctrl)->AllowModeSwitch = value;
	}
	else if (propertyName == L"Expand")
	{
		if (ctrl->Type() == UIClass::UI_DateTimePicker)
			((DateTimePicker*)ctrl)->SetExpanded(value);
	}
	else if (propertyName == L"AlwaysShowVScroll")
	{
		if (ctrl->Type() == UIClass::UI_ScrollView)
			((ScrollView*)ctrl)->AlwaysShowVScroll = value;
	}
	else if (propertyName == L"AlwaysShowHScroll")
	{
		if (ctrl->Type() == UIClass::UI_ScrollView)
			((ScrollView*)ctrl)->AlwaysShowHScroll = value;
	}
	else if (propertyName == L"AutoContentSize")
	{
		if (ctrl->Type() == UIClass::UI_ScrollView)
			((ScrollView*)ctrl)->AutoContentSize = value;
	}
	else if (propertyName == L"Visited")
	{
		if (ctrl->Type() == UIClass::UI_LinkLabel)
			((LinkLabel*)ctrl)->Visited = value;
	}
	else if (propertyName == L"IsSplitterFixed")
	{
		if (ctrl->Type() == UIClass::UI_SplitContainer)
			((SplitContainer*)ctrl)->IsSplitterFixed = value;
	}
	else if (propertyName == L"LastChildFill")
	{
		if (ctrl->Type() == UIClass::UI_DockPanel)
			((DockPanel*)ctrl)->SetLastChildFill(value);
	}
	else if (propertyName == L"Enabled")
	{
		ctrl->Enable = value;
	}
	else if (propertyName == L"Visible")
	{
		ctrl->Visible = value;
	}


	_binding.NotifyControlChanged(ctrl);
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
			for (int i = 0; i < current->Children.size(); i++)
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
	selected->PostRender();
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

	auto ctrl = control->ControlInstance;
	int yOffset = GetContentTopLocal();

	// 基本属性
	CreatePropertyItem(L"Name", control->Name, yOffset);
	CreatePropertyItem(L"Text", ctrl->Text, yOffset);
	if (control->Type == UIClass::UI_LinkLabel)
	{
		auto* link = (LinkLabel*)ctrl;
		CreateBoolPropertyItem(L"Visited", link->Visited, yOffset);
	}
	{
		auto* shared = _binding.GetDesignedFormSharedFont();
		::Font* f = ctrl->Font;
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
	CreatePropertyItem(L"X", std::to_wstring(ctrl->Location.x), yOffset);
	CreatePropertyItem(L"Y", std::to_wstring(ctrl->Location.y), yOffset);
	CreatePropertyItem(L"Width", std::to_wstring(ctrl->Size.cx), yOffset);
	CreatePropertyItem(L"Height", std::to_wstring(ctrl->Size.cy), yOffset);

	// 状态
	CreateBoolPropertyItem(L"Enabled", ctrl->Enable, yOffset);
	CreateBoolPropertyItem(L"Visible", ctrl->Visible, yOffset);

	// 常用外观/布局
	CreateColorPropertyItem(L"BackColor", ctrl->BackColor, yOffset);
	CreateColorPropertyItem(L"ForeColor", ctrl->ForeColor, yOffset);
	CreateColorPropertyItem(L"BolderColor", ctrl->BolderColor, yOffset);
	CreateThicknessPropertyItem(L"Margin", ctrl->Margin, yOffset);
	// ToolBar/StatusBar 的 Padding 是 int（会隐藏 Control::Padding(Thickness)），这里对齐其实际语义
	if (control->Type == UIClass::UI_ToolBar)
		CreatePropertyItem(L"Padding", std::to_wstring(((ToolBar*)ctrl)->Padding), yOffset);
	else if (control->Type == UIClass::UI_StatusBar)
		CreatePropertyItem(L"Padding", std::to_wstring(((StatusBar*)ctrl)->Padding), yOffset);
	else
		CreateThicknessPropertyItem(L"Padding", ctrl->Padding, yOffset);
	CreateAnchorPropertyItem(L"Anchor", ctrl->AnchorStyles, yOffset);
	CreateEnumPropertyItem(L"HAlign", HAlignToText(ctrl->HAlign), { L"Left", L"Center", L"Right", L"Stretch" }, yOffset);
	CreateEnumPropertyItem(L"VAlign", VAlignToText(ctrl->VAlign), { L"Top", L"Center", L"Bottom", L"Stretch" }, yOffset);
	if (ctrl->Parent && ctrl->Parent->Type() == UIClass::UI_DockPanel)
		CreateEnumPropertyItem(L"Dock", DockToText(ctrl->DockPosition), { L"Fill", L"Left", L"Top", L"Right", L"Bottom" }, yOffset);
	if (ctrl->Parent && ctrl->Parent->Type() == UIClass::UI_GridPanel)
	{
		CreatePropertyItem(L"GridRow", std::to_wstring(ctrl->GridRow), yOffset);
		CreatePropertyItem(L"GridColumn", std::to_wstring(ctrl->GridColumn), yOffset);
		CreatePropertyItem(L"GridRowSpan", std::to_wstring(ctrl->GridRowSpan), yOffset);
		CreatePropertyItem(L"GridColumnSpan", std::to_wstring(ctrl->GridColumnSpan), yOffset);
	}
	if (control->Type == UIClass::UI_TabControl)
	{
		auto* tc = (TabControl*)ctrl;
		CreatePropertyItem(L"SelectedIndex", std::to_wstring(tc->SelectedIndex), yOffset);
		CreatePropertyItem(L"TitleHeight", std::to_wstring(tc->TitleHeight), yOffset);
		CreatePropertyItem(L"TitleWidth", std::to_wstring(tc->TitleWidth), yOffset);
		CreateEnumPropertyItem(L"AnimationMode", TabAnimationModeToText(tc->AnimationMode), { L"DirectReplace", L"SlideHorizontal" }, yOffset);
	}
	if (control->Type == UIClass::UI_DockPanel)
	{
		auto* dp = (DockPanel*)ctrl;
		CreateBoolPropertyItem(L"LastChildFill", dp->GetLastChildFill(), yOffset);
	}
	if (control->Type == UIClass::UI_GroupBox)
	{
		auto* gb = (GroupBox*)ctrl;
		CreatePropertyItem(L"CaptionMarginLeft", FloatToText(gb->CaptionMarginLeft), yOffset);
		CreatePropertyItem(L"CaptionPaddingX", FloatToText(gb->CaptionPaddingX), yOffset);
		CreatePropertyItem(L"CaptionPaddingY", FloatToText(gb->CaptionPaddingY), yOffset);
	}
	if (control->Type == UIClass::UI_SplitContainer)
	{
		auto* split = (SplitContainer*)ctrl;
		CreateEnumPropertyItem(L"SplitOrientation", OrientationToText(split->SplitOrientation), { L"Horizontal", L"Vertical" }, yOffset);
		CreatePropertyItem(L"SplitterDistance", std::to_wstring(split->SplitterDistance), yOffset);
		CreatePropertyItem(L"SplitterWidth", std::to_wstring(split->SplitterWidth), yOffset);
		CreatePropertyItem(L"Panel1MinSize", std::to_wstring(split->Panel1MinSize), yOffset);
		CreatePropertyItem(L"Panel2MinSize", std::to_wstring(split->Panel2MinSize), yOffset);
		CreateBoolPropertyItem(L"IsSplitterFixed", split->IsSplitterFixed, yOffset);
	}
	if (control->Type == UIClass::UI_StatusBar)
	{
		auto* sb = (StatusBar*)ctrl;
		CreateBoolPropertyItem(L"TopMost", sb->TopMost, yOffset);
		CreatePropertyItem(L"Gap", std::to_wstring(sb->Gap), yOffset);
	}
	if (control->Type == UIClass::UI_StackPanel)
	{
		auto* sp = (StackPanel*)ctrl;
		CreateEnumPropertyItem(L"Orientation", OrientationToText(sp->GetOrientation()), { L"Horizontal", L"Vertical" }, yOffset);
		CreatePropertyItem(L"Spacing", std::to_wstring(sp->GetSpacing()), yOffset);
	}
	if (control->Type == UIClass::UI_WrapPanel)
	{
		auto* wp = (WrapPanel*)ctrl;
		CreateEnumPropertyItem(L"Orientation", OrientationToText(wp->GetOrientation()), { L"Horizontal", L"Vertical" }, yOffset);
		CreatePropertyItem(L"ItemWidth", std::to_wstring(wp->GetItemWidth()), yOffset);
		CreatePropertyItem(L"ItemHeight", std::to_wstring(wp->GetItemHeight()), yOffset);
	}
	if (control->Type == UIClass::UI_ProgressBar)
	{
		auto* pb = (ProgressBar*)ctrl;
		CreateFloatSliderPropertyItem(L"PercentageValue", pb->PercentageValue, 0.0f, 1.0f, 0.01f, yOffset);
	}
	if (control->Type == UIClass::UI_LoadingRing)
	{
		auto* lr = (LoadingRing*)ctrl;
		CreateBoolPropertyItem(L"Active", lr->Active, yOffset);
	}
	if (control->Type == UIClass::UI_ProgressRing)
	{
		auto* pr = (ProgressRing*)ctrl;
		CreateFloatSliderPropertyItem(L"PercentageValue", pr->PercentageValue, 0.0f, 1.0f, 0.01f, yOffset);
		CreateBoolPropertyItem(L"ShowPercentage", pr->ShowPercentage, yOffset);
	}
	if (control->Type == UIClass::UI_DateTimePicker)
	{
		auto* dtp = (DateTimePicker*)ctrl;
		std::wstring mode = L"DateTime";
		switch (dtp->Mode)
		{
		case DateTimePickerMode::DateOnly: mode = L"DateOnly"; break;
		case DateTimePickerMode::TimeOnly: mode = L"TimeOnly"; break;
		case DateTimePickerMode::DateTime: default: mode = L"DateTime"; break;
		}
		CreateEnumPropertyItem(L"Mode", mode, { L"DateOnly", L"TimeOnly", L"DateTime" }, yOffset);
		CreateBoolPropertyItem(L"AllowDateSelection", dtp->AllowDateSelection, yOffset);
		CreateBoolPropertyItem(L"AllowTimeSelection", dtp->AllowTimeSelection, yOffset);
		CreateBoolPropertyItem(L"AllowModeSwitch", dtp->AllowModeSwitch, yOffset);
		CreateBoolPropertyItem(L"Expand", dtp->Expand, yOffset);
	}
	if (control->Type == UIClass::UI_PictureBox)
	{
		CreateEnumPropertyItem(L"SizeMode", ImageSizeModeToText(ctrl->SizeMode), { L"Normal", L"CenterImage", L"Stretch", L"Zoom" }, yOffset);
	}
	if (control->Type == UIClass::UI_ScrollView)
	{
		auto* sv = (ScrollView*)ctrl;
		CreateColorPropertyItem(L"ScrollBackColor", sv->ScrollBackColor, yOffset);
		CreateColorPropertyItem(L"ScrollForeColor", sv->ScrollForeColor, yOffset);
		CreateBoolPropertyItem(L"AlwaysShowVScroll", sv->AlwaysShowVScroll, yOffset);
		CreateBoolPropertyItem(L"AlwaysShowHScroll", sv->AlwaysShowHScroll, yOffset);
		CreateBoolPropertyItem(L"AutoContentSize", sv->AutoContentSize, yOffset);
		CreatePropertyItem(L"ContentWidth", std::to_wstring(sv->ContentSize.cx), yOffset);
		CreatePropertyItem(L"ContentHeight", std::to_wstring(sv->ContentSize.cy), yOffset);
		CreatePropertyItem(L"MouseWheelStep", std::to_wstring(sv->MouseWheelStep), yOffset);
	}
	if (control->Type == UIClass::UI_TreeView)
	{
		auto* tv = (TreeView*)ctrl;
		CreateColorPropertyItem(L"SelectedBackColor", tv->SelectedBackColor, yOffset);
		CreateColorPropertyItem(L"UnderMouseItemBackColor", tv->UnderMouseItemBackColor, yOffset);
		CreateColorPropertyItem(L"SelectedForeColor", tv->SelectedForeColor, yOffset);
	}
	if (control->Type == UIClass::UI_ToolBar)
	{
		auto* tb = (ToolBar*)ctrl;
		CreatePropertyItem(L"Gap", std::to_wstring(tb->Gap), yOffset);
		CreatePropertyItem(L"ItemHeight", std::to_wstring(tb->ItemHeight), yOffset);
	}
	if (control->Type == UIClass::UI_ComboBox)
	{
		auto* cb = (ComboBox*)ctrl;
		CreatePropertyItem(L"ExpandCount", std::to_wstring(cb->ExpandCount), yOffset);
		CreatePropertyItem(L"SelectedIndex", std::to_wstring(cb->SelectedIndex), yOffset);
	}
	if (control->Type == UIClass::UI_Slider)
	{
		auto* s = (Slider*)ctrl;
		CreatePropertyItem(L"Min", std::to_wstring(s->Min), yOffset);
		CreatePropertyItem(L"Max", std::to_wstring(s->Max), yOffset);
		CreatePropertyItem(L"Value", std::to_wstring(s->Value), yOffset);
		CreatePropertyItem(L"Step", std::to_wstring(s->Step), yOffset);
		CreateBoolPropertyItem(L"SnapToStep", s->SnapToStep, yOffset);
	}
	if (control->Type == UIClass::UI_MediaPlayer)
	{
		auto* mp = (MediaPlayer*)ctrl;
		std::wstring mediaFile;
		auto it = control->DesignStrings.find(L"mediaFile");
		if (it != control->DesignStrings.end()) mediaFile = it->second;
		CreatePropertyItem(L"MediaFile", mediaFile, yOffset);
		CreateBoolPropertyItem(L"AutoPlay", mp->AutoPlay, yOffset);
		CreateBoolPropertyItem(L"Loop", mp->Loop, yOffset);
		CreateFloatSliderPropertyItem(L"Volume", (float)mp->Volume, 0.0f, 1.0f, 0.01f, yOffset);
		CreatePropertyItem(L"PlaybackRate", FloatToText(mp->PlaybackRate), yOffset);
		std::wstring rm = L"Fit";
		switch (mp->RenderMode)
		{
		case MediaPlayer::VideoRenderMode::Fit: rm = L"Fit"; break;
		case MediaPlayer::VideoRenderMode::Fill: rm = L"Fill"; break;
		case MediaPlayer::VideoRenderMode::Stretch: rm = L"Stretch"; break;
		case MediaPlayer::VideoRenderMode::Center: rm = L"Center"; break;
		case MediaPlayer::VideoRenderMode::UniformToFill: rm = L"UniformToFill"; break;
		default: rm = L"Fit"; break;
		}
		CreateEnumPropertyItem(L"RenderMode", rm, { L"Fit", L"Fill", L"Stretch", L"Center", L"UniformToFill" }, yOffset);
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
			auto cb = dynamic_cast<ComboBox*>(currentControl->ControlInstance);
			if (!cb) return;
			ComboBoxItemsEditorDialog dlg(cb);
			dlg.ShowDialog(this->ParentForm->Handle);
			cb->PostRender();
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
			auto gv = dynamic_cast<GridView*>(currentControl->ControlInstance);
			if (!gv) return;
			GridViewColumnsEditorDialog dlg(gv);
			dlg.ShowDialog(this->ParentForm->Handle);
			gv->PostRender();
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
			auto tc = dynamic_cast<TabControl*>(currentControl->ControlInstance);
			if (!tc) return;
			TabControlPagesEditorDialog dlg(tc);
			// 如果删除页，需要同步移除该页下的 DesignerControl 以避免悬挂
			dlg.OnBeforeDeletePage = [this](Control* page) {
				if (page) _binding.RemoveDesignerControlsInSubtree(page);
				};
			dlg.ShowDialog(this->ParentForm->Handle);
			tc->PostRender();
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
		auto editBtn = new Button(L"编辑按钮...", 10, yOffset + 8, width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->ParentForm) return;
			auto tb = dynamic_cast<ToolBar*>(currentControl->ControlInstance);
			if (!tb) return;
			ToolBarButtonsEditorDialog dlg(tb);
			// 如果删除按钮控件，需要同步移除 DesignerControl
			dlg.OnBeforeDeleteButton = [this](Control* btn) {
				if (btn) _binding.RemoveDesignerControlsInSubtree(btn);
				};
			dlg.ShowDialog(this->ParentForm->Handle);
			tb->PostRender();
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
			auto tv = dynamic_cast<TreeView*>(currentControl->ControlInstance);
			if (!tv) return;
			TreeViewNodesEditorDialog dlg(tv);
			dlg.ShowDialog(this->ParentForm->Handle);
			tv->PostRender();
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
			auto gp = dynamic_cast<GridPanel*>(currentControl->ControlInstance);
			if (!gp) return;
			GridPanelDefinitionsEditorDialog dlg(gp);
			dlg.ShowDialog(this->ParentForm->Handle);
			gp->PostRender();
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
			m->PostRender();
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
			auto sb = dynamic_cast<StatusBar*>(currentControl->ControlInstance);
			if (!sb) return;
			StatusBarPartsEditorDialog dlg(sb);
			dlg.ShowDialog(this->ParentForm->Handle);
			sb->PostRender();
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

	auto removeFromParent = [this](Control* c) {
		if (!c) return;
		if (_contentHost && c->Parent == _contentHost)
			_contentHost->RemoveControl(c);
		else
			this->RemoveControl(c);
	};

	auto isDescendantOf = [](Control* root, Control* node) -> bool {
		if (!root || !node) return false;
		if (root == node) return true;
		std::vector<Control*> stack;
		stack.reserve(64);
		stack.push_back(root);
		while (!stack.empty())
		{
			Control* cur = stack.back();
			stack.pop_back();
			if (!cur) continue;
			for (int i = 0; i < cur->Children.size(); i++)
			{
				auto* ch = cur->Children[i];
				if (!ch) continue;
				if (ch == node) return true;
				stack.push_back(ch);
			}
		}
		return false;
	};

	// 在移除控件前，如果Form的Selected是PropertyGrid的子控件，先清除Selected
	// 避免移除后的控件在处理鼠标事件时访问ParentForm
	if (this->ParentForm && this->ParentForm->Selected)
	{
		for (auto item : _items)
		{
			if ((item->NameLabel && this->ParentForm->Selected == item->NameLabel) ||
				(item->ValueControl && (this->ParentForm->Selected == item->ValueControl || isDescendantOf(item->ValueControl, this->ParentForm->Selected))) ||
				(item->ValueTextBox && this->ParentForm->Selected == item->ValueTextBox) ||
				(item->ValueCheckBox && this->ParentForm->Selected == item->ValueCheckBox))
			{
				this->ParentForm->Selected = nullptr;
				break;
			}
		}
		if (this->ParentForm->Selected)
		{
			for (auto* c : _extraControls)
			{
				if (c && (this->ParentForm->Selected == c || isDescendantOf(c, this->ParentForm->Selected)))
				{
					this->ParentForm->Selected = nullptr;
					break;
				}
			}
		}
	}

	// 移除所有属性项（保留标题）
	for (auto item : _items)
	{
		if (item->NameLabel)
		{
			removeFromParent(item->NameLabel);
			delete item->NameLabel;
			item->NameLabel = nullptr;
		}
		if (item->ValueControl)
		{
			removeFromParent(item->ValueControl);
			delete item->ValueControl;
			item->ValueControl = nullptr;
		}
		else
		{
			// 兜底：某些条目可能不走 ValueControl（历史代码/异常场景）
			if (item->ValueTextBox)
			{
				removeFromParent(item->ValueTextBox);
				delete item->ValueTextBox;
				item->ValueTextBox = nullptr;
			}
			if (item->ValueCheckBox)
			{
				removeFromParent(item->ValueCheckBox);
				delete item->ValueCheckBox;
				item->ValueCheckBox = nullptr;
			}
		}
		delete item;
	}
	_items.clear();

	for (auto* c : _extraControls)
	{
		if (!c) continue;
		removeFromParent(c);
		delete c;
	}
	_extraControls.clear();
	_scrollEntries.clear();
	_scrollOffsetY = 0;
	_contentHeight = 0;
	_draggingScrollThumb = false;
}
