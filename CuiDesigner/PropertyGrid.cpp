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
#include "../CUI/GUI/ComboBox.h"
#include "../CUI/GUI/Slider.h"
#include "../CUI/GUI/ProgressBar.h"
#include "../CUI/GUI/PictureBox.h"
#include "../CUI/GUI/TreeView.h"
#include "../CUI/GUI/TabControl.h"
#include "../CUI/GUI/ToolBar.h"
#include "../CUI/GUI/Layout/StackPanel.h"
#include "../CUI/GUI/Layout/WrapPanel.h"
#include <commdlg.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
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
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(3) << c.r << L"," << c.g << L"," << c.b << L"," << c.a;
		return oss.str();
	}

	static bool TryParseHexNibble(wchar_t c, int& out)
	{
		if (c >= L'0' && c <= L'9') { out = c - L'0'; return true; }
		if (c >= L'a' && c <= L'f') { out = 10 + (c - L'a'); return true; }
		if (c >= L'A' && c <= L'F') { out = 10 + (c - L'A'); return true; }
		return false;
	}

	static bool TryParseColor(const std::wstring& s, D2D1_COLOR_F& out)
	{
		auto t = TrimWs(s);
		if (t.empty()) return false;
		// #RRGGBB or #AARRGGBB
		if (t[0] == L'#')
		{
			std::wstring hex = t.substr(1);
			if (hex.size() != 6 && hex.size() != 8) return false;
			auto byteAt = [&](size_t i, unsigned char& b) -> bool {
				int hi = 0, lo = 0;
				if (!TryParseHexNibble(hex[i], hi)) return false;
				if (!TryParseHexNibble(hex[i + 1], lo)) return false;
				b = (unsigned char)((hi << 4) | lo);
				return true;
				};
			unsigned char a = 255, r = 0, g = 0, b = 0;
			size_t off = 0;
			if (hex.size() == 8)
			{
				if (!byteAt(0, a)) return false;
				off = 2;
			}
			if (!byteAt(off + 0, r)) return false;
			if (!byteAt(off + 2, g)) return false;
			if (!byteAt(off + 4, b)) return false;
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
}

PropertyGrid::~PropertyGrid()
{
}

void PropertyGrid::CreatePropertyItem(std::wstring propertyName, std::wstring value, int& yOffset)
{
	int width = this->Width;

	// 属性名标签
	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	// 确保ParentForm已设置
	nameLabel->ParentForm = this->ParentForm;

	// 值文本框
	auto valueTextBox = new TextBox(L"", (width - 30) / 2 + 15, yOffset, (width - 30) / 2, 20);
	valueTextBox->Text = value;

	// 文本改变事件
	valueTextBox->OnTextChanged += [this, propertyName](Control* sender, std::wstring oldText, std::wstring newText) {
		UpdatePropertyFromTextBox(propertyName, newText);
		};

	this->AddControl(valueTextBox);
	// 确保ParentForm已设置（关键！）
	valueTextBox->ParentForm = this->ParentForm;

	auto item = new PropertyItem(propertyName, nameLabel, valueTextBox);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateColorPropertyItem(std::wstring propertyName, const D2D1_COLOR_F& value, int& yOffset)
{
	int width = this->Width;

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	int valueX = (width - 30) / 2 + 15;
	int valueW = (width - 30) / 2;

	// 容器：颜色预览 + 文本 + 选择按钮
	auto panel = new Panel(valueX, yOffset, valueW, 20);
	panel->BackColor = D2D1::ColorF(0, 0);
	panel->Boder = 0.0f;
	panel->ParentForm = this->ParentForm;

	const int previewW = 18;
	const int btnW = 26;
	const int gap = 6;
	int textW = valueW - previewW - btnW - gap * 2;
	if (textW < 40) textW = 40;

	auto preview = new Panel(0, 1, previewW, 18);
	preview->BackColor = value;
	preview->Boder = 1.0f;
	preview->BolderColor = Colors::DimGrey;
	preview->ParentForm = this->ParentForm;

	auto tb = new TextBox(L"", previewW + gap, 0, textW, 20);
	tb->Text = ColorToText(value);
	tb->ParentForm = this->ParentForm;
	tb->OnTextChanged += [this, propertyName, preview](Control*, std::wstring, std::wstring newText) {
		UpdatePropertyFromTextBox(propertyName, newText);
		D2D1_COLOR_F c{};
		if (TryParseColor(newText, c))
		{
			preview->BackColor = c;
			preview->PostRender();
		}
	};

	auto btn = new Button(L"...", previewW + gap + textW + gap, -1, btnW, 22);
	btn->ParentForm = this->ParentForm;
	btn->OnMouseClick += [this, propertyName, tb, preview](Control*, MouseEventArgs) {
		if (!this->ParentForm) return;
		D2D1_COLOR_F cur{};
		if (!TryParseColor(tb->Text, cur)) cur = D2D1::ColorF(0, 0, 0, 1);
		D2D1_COLOR_F picked{};
		if (PickColorWithDialog(this->ParentForm->Handle, cur, picked))
		{
			preview->BackColor = picked;
			tb->Text = ColorToText(picked);
			UpdatePropertyFromTextBox(propertyName, tb->Text);
		}
	};

	panel->AddControl(preview);
	panel->AddControl(tb);
	panel->AddControl(btn);
	this->AddControl(panel);

	_items.push_back(new PropertyItem(propertyName, nameLabel, (Control*)panel));

	yOffset += 25;
}

void PropertyGrid::CreateThicknessPropertyItem(std::wstring propertyName, const Thickness& value, int& yOffset)
{
	int width = this->Width;

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	int valueX = (width - 30) / 2 + 15;
	int valueW = (width - 30) / 2;

	auto panel = new Panel(valueX, yOffset, valueW, 20);
	panel->BackColor = D2D1::ColorF(0, 0);
	panel->Boder = 0.0f;
	panel->ParentForm = this->ParentForm;

	int gap = 4;
	int boxW = (valueW - gap * 3) / 4;
	if (boxW < 26) boxW = 26;

	auto makeBox = [&](int x, float v) {
		auto t = new TextBox(L"", x, 0, boxW, 20);
		t->ParentForm = this->ParentForm;
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(2) << v;
		t->Text = oss.str();
		return t;
	};

	auto tbL = makeBox(0, value.Left);
	auto tbT = makeBox(boxW + gap, value.Top);
	auto tbR = makeBox((boxW + gap) * 2, value.Right);
	auto tbB = makeBox((boxW + gap) * 3, value.Bottom);

	auto apply = [this, propertyName, tbL, tbT, tbR, tbB](Control*, std::wstring, std::wstring) {
		Thickness t{};
		try { t.Left = std::stof(tbL->Text); } catch (...) { return; }
		try { t.Top = std::stof(tbT->Text); } catch (...) { return; }
		try { t.Right = std::stof(tbR->Text); } catch (...) { return; }
		try { t.Bottom = std::stof(tbB->Text); } catch (...) { return; }
		UpdatePropertyFromTextBox(propertyName, ThicknessToText(t));
	};

	tbL->OnTextChanged += apply;
	tbT->OnTextChanged += apply;
	tbR->OnTextChanged += apply;
	tbB->OnTextChanged += apply;

	panel->AddControl(tbL);
	panel->AddControl(tbT);
	panel->AddControl(tbR);
	panel->AddControl(tbB);
	this->AddControl(panel);

	_items.push_back(new PropertyItem(propertyName, nameLabel, (Control*)panel));

	yOffset += 25;
}

void PropertyGrid::CreateBoolPropertyItem(std::wstring propertyName, bool value, int& yOffset)
{
	int width = this->Width;

	// 属性名标签
	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	// 值复选框（不显示额外文字）
	auto valueCheckBox = new CheckBox(L"", (width - 30) / 2 + 15, yOffset);
	valueCheckBox->Checked = value;
	valueCheckBox->ParentForm = this->ParentForm;

	valueCheckBox->OnMouseClick += [this, propertyName](Control* sender, MouseEventArgs) {
		auto cb = (CheckBox*)sender;
		UpdatePropertyFromBool(propertyName, cb->Checked);
		};

	this->AddControl(valueCheckBox);

	auto item = new PropertyItem(propertyName, nameLabel, valueCheckBox);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateAnchorPropertyItem(std::wstring propertyName, uint8_t anchorStyles, int& yOffset)
{
	int width = this->Width;

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	int valueX = (width - 30) / 2 + 15;
	int valueW = (width - 30) / 2;

	// 使用一个容器承载 4 个方向开关
	auto panel = new Panel(valueX, yOffset, valueW, 20);
	panel->BackColor = D2D1::ColorF(0, 0);
	panel->Boder = 0.0f;
	panel->ParentForm = this->ParentForm;

	// 紧凑布局：L/T/R/B
	int gap = 6;
	int cbW = (valueW - gap * 3) / 4;
	if (cbW < 24) cbW = 24;

	auto cbL = new CheckBox(L"", 0, 0);
	auto cbT = new CheckBox(L"", cbW + gap, 0);
	auto cbR = new CheckBox(L"", (cbW + gap) * 2, 0);
	auto cbB = new CheckBox(L"", (cbW + gap) * 3, 0);
	cbL->Size = { cbW, 20 };
	cbT->Size = { cbW, 20 };
	cbR->Size = { cbW, 20 };
	cbB->Size = { cbW, 20 };
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

	this->AddControl(panel);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)panel);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateEnumPropertyItem(std::wstring propertyName, const std::wstring& value,
	const std::vector<std::wstring>& options, int& yOffset)
{
	int width = this->Width;

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	auto valueCombo = new ComboBox(L"", (width - 30) / 2 + 15, yOffset, (width - 30) / 2, 20);
	valueCombo->ParentForm = this->ParentForm;
	valueCombo->values.Clear();
	for (auto& o : options) valueCombo->values.Add(o);

	int idx = 0;
	for (int i = 0; i < valueCombo->values.Count; i++)
	{
		if (valueCombo->values[i] == value) { idx = i; break; }
	}
	valueCombo->SelectedIndex = idx;
	if (valueCombo->values.Count > 0 && idx >= 0 && idx < valueCombo->values.Count)
		valueCombo->Text = valueCombo->values[idx];
	else
		valueCombo->Text = value;

	valueCombo->OnSelectionChanged += [this, propertyName](Control* sender) {
		auto cb = (ComboBox*)sender;
		UpdatePropertyFromTextBox(propertyName, cb->Text);
		};

	this->AddControl(valueCombo);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)valueCombo);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::CreateFloatSliderPropertyItem(std::wstring propertyName, float value,
	float minValue, float maxValue, float step, int& yOffset)
{
	int width = this->Width;

	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = { (width - 30) / 2, 20 };
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	auto slider = new Slider((width - 30) / 2 + 15, yOffset - 4, (width - 30) / 2, 28);
	slider->ParentForm = this->ParentForm;
	slider->Min = minValue;
	slider->Max = maxValue;
	slider->Step = step;
	slider->SnapToStep = false;
	slider->Value = value;

	slider->OnValueChanged += [this, propertyName](Control*, float, float newValue) {
		UpdatePropertyFromFloat(propertyName, newValue);
		};

	this->AddControl(slider);

	auto item = new PropertyItem(propertyName, nameLabel, (Control*)slider);
	_items.push_back(item);

	yOffset += 32;
}

void PropertyGrid::UpdatePropertyFromTextBox(std::wstring propertyName, std::wstring value)
{
	// 未选中控件时：编辑“被设计窗体”属性
	if (!_currentControl)
	{
		if (!_canvas) return;
		try
		{
			if (propertyName == L"Text")
			{
				_canvas->SetDesignedFormText(value);
			}
			else if (propertyName == L"HeadHeight")
			{
				_canvas->SetDesignedFormHeadHeight(std::stoi(value));
			}
			else if (propertyName == L"X")
			{
				auto p = _canvas->GetDesignedFormLocation();
				p.x = std::stoi(value);
				_canvas->SetDesignedFormLocation(p);
			}
			else if (propertyName == L"Y")
			{
				auto p = _canvas->GetDesignedFormLocation();
				p.y = std::stoi(value);
				_canvas->SetDesignedFormLocation(p);
			}
			else if (propertyName == L"Width")
			{
				auto s = _canvas->GetDesignedFormSize();
				s.cx = std::stoi(value);
				_canvas->SetDesignedFormSize(s);
			}
			else if (propertyName == L"Height")
			{
				auto s = _canvas->GetDesignedFormSize();
				s.cy = std::stoi(value);
				_canvas->SetDesignedFormSize(s);
			}
		}
		catch (...) {}
		return;
	}
	if (!_currentControl->ControlInstance) return;

	auto ctrl = _currentControl->ControlInstance;

	try
	{
		if (propertyName == L"Name")
		{
			if (_canvas)
				_currentControl->Name = _canvas->MakeUniqueControlName(_currentControl, value);
			else
				_currentControl->Name = value;
		}
		else if (propertyName == L"Text")
		{
			ctrl->Text = value;
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
		else if (propertyName == L"SelectIndex")
		{
			if (ctrl->Type() == UIClass::UI_TabControl)
			{
				auto* tc = (TabControl*)ctrl;
				tc->SelectIndex = std::stoi(value);
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
		else if (propertyName == L"SizeMode")
		{
			if (ctrl->Type() == UIClass::UI_PictureBox)
			{
				::ImageSizeMode m;
				if (TryParseImageSizeMode(value, m)) ctrl->SizeMode = m;
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
		else if (propertyName == L"Spacing")
		{
			if (ctrl->Type() == UIClass::UI_StackPanel)
				((StackPanel*)ctrl)->SetSpacing(std::stof(value));
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
				((ToolBar*)ctrl)->Gap = std::stof(value);
		}
	}
	catch (...)
	{
		// 忽略转换错误
	}

	if (auto* p = dynamic_cast<Panel*>(ctrl->Parent))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
	if (_canvas) _canvas->ClampControlToDesignSurface(ctrl);
	ctrl->PostRender();
}

void PropertyGrid::UpdatePropertyFromFloat(std::wstring propertyName, float value)
{
	if (!_currentControl || !_currentControl->ControlInstance) return;
	auto ctrl = _currentControl->ControlInstance;

	try
	{
		if (propertyName == L"PercentageValue")
		{
			if (ctrl->Type() == UIClass::UI_ProgressBar)
			{
				auto* pb = (ProgressBar*)ctrl;
				float v = std::clamp(value, 0.0f, 1.0f);
				pb->PercentageValue = v;
			}
		}
	}
	catch (...) {}

	if (auto* p = dynamic_cast<Panel*>(ctrl->Parent))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
	if (_canvas) _canvas->ClampControlToDesignSurface(ctrl);
	ctrl->PostRender();
}

void PropertyGrid::UpdateAnchorFromChecks(bool left, bool top, bool right, bool bottom)
{
	if (!_currentControl || !_currentControl->ControlInstance) return;
	auto* ctrl = _currentControl->ControlInstance;

	uint8_t a = AnchorStyles::None;
	if (left) a |= AnchorStyles::Left;
	if (top) a |= AnchorStyles::Top;
	if (right) a |= AnchorStyles::Right;
	if (bottom) a |= AnchorStyles::Bottom;
	if (_canvas)
	{
		_canvas->ApplyAnchorStylesKeepingBounds(ctrl, a);
	}
	else
	{
		ctrl->AnchorStyles = a;
	}

	if (auto* p = dynamic_cast<Panel*>(ctrl->Parent))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
	if (_canvas) _canvas->ClampControlToDesignSurface(ctrl);
	ctrl->PostRender();
}

void PropertyGrid::UpdatePropertyFromBool(std::wstring propertyName, bool value)
{
	// 未选中控件时：编辑“被设计窗体”属性
	if (!_currentControl)
	{
		if (!_canvas) return;
		if (propertyName == L"VisibleHead") _canvas->SetDesignedFormVisibleHead(value);
		else if (propertyName == L"MinBox") _canvas->SetDesignedFormMinBox(value);
		else if (propertyName == L"MaxBox") _canvas->SetDesignedFormMaxBox(value);
		else if (propertyName == L"CloseBox") _canvas->SetDesignedFormCloseBox(value);
		else if (propertyName == L"CenterTitle") _canvas->SetDesignedFormCenterTitle(value);
		else if (propertyName == L"AllowResize") _canvas->SetDesignedFormAllowResize(value);
		return;
	}
	if (!_currentControl->ControlInstance) return;
	auto ctrl = _currentControl->ControlInstance;

	if (propertyName == L"Enabled")
	{
		ctrl->Enable = value;
	}
	else if (propertyName == L"Visible")
	{
		ctrl->Visible = value;
	}
	ctrl->PostRender();
}

void PropertyGrid::LoadControl(std::shared_ptr<DesignerControl> control)
{
	Clear();
	_currentControl = control;

	if (!control || !control->ControlInstance)
	{
		// 未选中控件时：展示被设计窗体属性
		if (_canvas)
		{
			_titleLabel->Text = L"属性 - 窗体";
			int yOffset = 45;
			CreatePropertyItem(L"Text", _canvas->GetDesignedFormText(), yOffset);
			CreateBoolPropertyItem(L"VisibleHead", _canvas->GetDesignedFormVisibleHead(), yOffset);
			CreatePropertyItem(L"HeadHeight", std::to_wstring(_canvas->GetDesignedFormHeadHeight()), yOffset);
			CreateBoolPropertyItem(L"MinBox", _canvas->GetDesignedFormMinBox(), yOffset);
			CreateBoolPropertyItem(L"MaxBox", _canvas->GetDesignedFormMaxBox(), yOffset);
			CreateBoolPropertyItem(L"CloseBox", _canvas->GetDesignedFormCloseBox(), yOffset);
			CreateBoolPropertyItem(L"CenterTitle", _canvas->GetDesignedFormCenterTitle(), yOffset);
			CreateBoolPropertyItem(L"AllowResize", _canvas->GetDesignedFormAllowResize(), yOffset);
			auto p = _canvas->GetDesignedFormLocation();
			CreatePropertyItem(L"X", std::to_wstring(p.x), yOffset);
			CreatePropertyItem(L"Y", std::to_wstring(p.y), yOffset);
			auto s = _canvas->GetDesignedFormSize();
			CreatePropertyItem(L"Width", std::to_wstring(s.cx), yOffset);
			CreatePropertyItem(L"Height", std::to_wstring(s.cy), yOffset);
			Control::SetChildrenParentForm(this, this->ParentForm);
			return;
		}
		_titleLabel->Text = L"属性";
		return;
	}

	_titleLabel->Text = L"属性 - " + control->Name;

	auto ctrl = control->ControlInstance;
	int yOffset = 45;

	// 基本属性
	CreatePropertyItem(L"Name", control->Name, yOffset);
	CreatePropertyItem(L"Text", ctrl->Text, yOffset);

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
		CreatePropertyItem(L"SelectIndex", std::to_wstring(tc->SelectIndex), yOffset);
		CreatePropertyItem(L"TitleHeight", std::to_wstring(tc->TitleHeight), yOffset);
		CreatePropertyItem(L"TitleWidth", std::to_wstring(tc->TitleWidth), yOffset);
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
	if (control->Type == UIClass::UI_PictureBox)
	{
		CreateEnumPropertyItem(L"SizeMode", ImageSizeModeToText(ctrl->SizeMode), { L"Normal", L"CenterImage", L"Stretch", L"Zoom" }, yOffset);
	}
	if (control->Type == UIClass::UI_TreeView)
	{
		auto* tv = (TreeView*)ctrl;
		CreatePropertyItem(L"SelectedBackColor", ColorToText(tv->SelectedBackColor), yOffset);
		CreatePropertyItem(L"UnderMouseItemBackColor", ColorToText(tv->UnderMouseItemBackColor), yOffset);
		CreatePropertyItem(L"SelectedForeColor", ColorToText(tv->SelectedForeColor), yOffset);
	}
	if (control->Type == UIClass::UI_ToolBar)
	{
		auto* tb = (ToolBar*)ctrl;
		CreatePropertyItem(L"Gap", std::to_wstring(tb->Gap), yOffset);
	}

	// 高级编辑入口（模态窗口）
	if (control->Type == UIClass::UI_ComboBox)
	{
		auto editBtn = new Button(L"编辑下拉项...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto cb = dynamic_cast<ComboBox*>(_currentControl->ControlInstance);
			if (!cb) return;
			ComboBoxItemsEditorDialog dlg(cb);
			dlg.ShowDialog(this->ParentForm->Handle);
			cb->PostRender();
			};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_GridView)
	{
		auto editBtn = new Button(L"编辑列...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto gv = dynamic_cast<GridView*>(_currentControl->ControlInstance);
			if (!gv) return;
			GridViewColumnsEditorDialog dlg(gv);
			dlg.ShowDialog(this->ParentForm->Handle);
			gv->PostRender();
			};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_TabControl)
	{
		auto editBtn = new Button(L"编辑页...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto tc = dynamic_cast<TabControl*>(_currentControl->ControlInstance);
			if (!tc) return;
			TabControlPagesEditorDialog dlg(tc);
			// 如果删除页，需要同步移除该页下的 DesignerControl 以避免悬挂
			dlg.OnBeforeDeletePage = [this](Control* page) {
				if (_canvas && page) _canvas->RemoveDesignerControlsInSubtree(page);
				};
			dlg.ShowDialog(this->ParentForm->Handle);
			tc->PostRender();
			};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_ToolBar)
	{
		auto editBtn = new Button(L"编辑按钮...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto tb = dynamic_cast<ToolBar*>(_currentControl->ControlInstance);
			if (!tb) return;
			ToolBarButtonsEditorDialog dlg(tb);
			// 如果删除按钮控件，需要同步移除 DesignerControl
			dlg.OnBeforeDeleteButton = [this](Control* btn) {
				if (_canvas && btn) _canvas->RemoveDesignerControlsInSubtree(btn);
				};
			dlg.ShowDialog(this->ParentForm->Handle);
			tb->PostRender();
			};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_TreeView)
	{
		auto editBtn = new Button(L"编辑节点...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto tv = dynamic_cast<TreeView*>(_currentControl->ControlInstance);
			if (!tv) return;
			TreeViewNodesEditorDialog dlg(tv);
			dlg.ShowDialog(this->ParentForm->Handle);
			tv->PostRender();
			};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_GridPanel)
	{
		auto editBtn = new Button(L"编辑行/列...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto gp = dynamic_cast<GridPanel*>(_currentControl->ControlInstance);
			if (!gp) return;
			GridPanelDefinitionsEditorDialog dlg(gp);
			dlg.ShowDialog(this->ParentForm->Handle);
			gp->PostRender();
			};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_Menu)
	{
		auto editBtn = new Button(L"编辑菜单项...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto m = dynamic_cast<Menu*>(_currentControl->ControlInstance);
			if (!m) return;
			MenuItemsEditorDialog dlg(m);
			dlg.ShowDialog(this->ParentForm->Handle);
			m->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_StatusBar)
	{
		auto editBtn = new Button(L"编辑分段...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto sb = dynamic_cast<StatusBar*>(_currentControl->ControlInstance);
			if (!sb) return;
			StatusBarPartsEditorDialog dlg(sb);
			dlg.ShowDialog(this->ParentForm->Handle);
			sb->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}

	// 确保所有新创建的子控件的ParentForm都被正确设置
	Control::SetChildrenParentForm(this, this->ParentForm);
}

void PropertyGrid::Clear()
{
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
			for (int i = 0; i < cur->Children.Count; i++)
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
			if (this->ParentForm->Selected == item->NameLabel ||
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
		this->RemoveControl(item->NameLabel);
		delete item->NameLabel;
		if (item->ValueControl)
		{
			this->RemoveControl(item->ValueControl);
			delete item->ValueControl;
		}
		delete item;
	}
	_items.clear();

	for (auto* c : _extraControls)
	{
		if (!c) continue;
		this->RemoveControl(c);
		delete c;
	}
	_extraControls.clear();
}
