#include "ToolBox.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Form.h"

#include <algorithm>
#include <cwctype>
#include <sstream>

namespace
{
	std::wstring ToolCategory(UIClass type)
	{
		switch (type)
		{
		case UIClass::UI_TextBox:
		case UIClass::UI_PasswordBox:
		case UIClass::UI_RichTextBox:
		case UIClass::UI_DateTimePicker:
		case UIClass::UI_NumericUpDown:
		case UIClass::UI_ComboBox:
		case UIClass::UI_Slider:
		case UIClass::UI_FilterBar:
			return L"输入";
		case UIClass::UI_Panel:
		case UIClass::UI_GroupBox:
		case UIClass::UI_Expander:
		case UIClass::UI_ScrollView:
		case UIClass::UI_StackPanel:
		case UIClass::UI_GridPanel:
		case UIClass::UI_DockPanel:
		case UIClass::UI_WrapPanel:
		case UIClass::UI_RelativePanel:
		case UIClass::UI_SplitContainer:
			return L"布局";
		case UIClass::UI_ListView:
		case UIClass::UI_ListBox:
		case UIClass::UI_GridView:
		case UIClass::UI_PropertyGrid:
		case UIClass::UI_ChartView:
		case UIClass::UI_ReportView:
		case UIClass::UI_KpiCard:
		case UIClass::UI_TreeView:
			return L"数据与列表";
		case UIClass::UI_ProgressBar:
		case UIClass::UI_LoadingRing:
		case UIClass::UI_ProgressRing:
		case UIClass::UI_ToastHost:
			return L"状态与反馈";
		case UIClass::UI_TabControl:
		case UIClass::UI_ToolBar:
		case UIClass::UI_Menu:
		case UIClass::UI_StatusBar:
			return L"导航与外壳";
		case UIClass::UI_WebBrowser:
		case UIClass::UI_MediaPlayer:
			return L"媒体与 Web";
		default:
			return L"基础控件";
		}
	}

	D2D1_COLOR_F CategoryAccent(const std::wstring& category)
	{
		if (category == L"输入") return D2D1::ColorF(0.38f, 0.36f, 0.88f, 1.0f);
		if (category == L"布局") return D2D1::ColorF(0.09f, 0.56f, 0.53f, 1.0f);
		if (category == L"数据与列表") return D2D1::ColorF(0.12f, 0.48f, 0.82f, 1.0f);
		if (category == L"状态与反馈") return D2D1::ColorF(0.90f, 0.47f, 0.12f, 1.0f);
		if (category == L"导航与外壳") return D2D1::ColorF(0.66f, 0.30f, 0.72f, 1.0f);
		if (category == L"媒体与 Web") return D2D1::ColorF(0.82f, 0.25f, 0.38f, 1.0f);
		return D2D1::ColorF(0.18f, 0.45f, 0.90f, 1.0f);
	}

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a = alpha;
		return color;
	}

	void DrawMiniLines(
		D2DGraphics* d2d,
		float x,
		float y,
		float width,
		int count,
		D2D1_COLOR_F color)
	{
		for (int index = 0; index < count; ++index)
		{
			const float lineY = y + index * 4.0f;
			d2d->DrawLine(x, lineY, x + width - index * 1.5f,
				lineY, color, 1.35f);
		}
	}

	void DrawControlGlyph(
		D2DGraphics* d2d,
		UIClass type,
		const D2D1_RECT_F& r,
		D2D1_COLOR_F color,
		Font* font)
	{
		const float cx = (r.left + r.right) * 0.5f;
		const float cy = (r.top + r.bottom) * 0.5f;
		auto frame = D2D1::RectF(r.left + 4.0f, r.top + 5.0f,
			r.right - 4.0f, r.bottom - 5.0f);
		auto monogram = [=](const std::wstring& text)
		{
			float textWidth = 8.0f;
			float textHeight = 12.0f;
			if (font)
			{
				const auto size = font->GetTextSize(text);
				textWidth = size.width;
				textHeight = size.height;
			}
			d2d->DrawString(text, cx - textWidth * 0.5f,
				cy - textHeight * 0.5f, color, font);
		};

		switch (type)
		{
		case UIClass::UI_Label:
			monogram(L"A");
			break;
		case UIClass::UI_LinkLabel:
			d2d->DrawEllipse(cx - 4.0f, cy, 5.0f, 3.5f, color, 1.4f);
			d2d->DrawEllipse(cx + 4.0f, cy, 5.0f, 3.5f, color, 1.4f);
			d2d->DrawLine(cx - 2.0f, cy, cx + 2.0f, cy, color, 1.4f);
			break;
		case UIClass::UI_Button:
			d2d->DrawRoundRect(frame, color, 1.5f, 4.0f);
			d2d->FillEllipse(cx, cy, 1.7f, 1.7f, color);
			break;
		case UIClass::UI_TextBox:
			d2d->DrawRoundRect(frame, color, 1.35f, 3.0f);
			d2d->DrawLine(cx - 5.0f, cy, cx + 3.0f, cy, color, 1.35f);
			d2d->DrawLine(cx + 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f, color, 1.35f);
			break;
		case UIClass::UI_PasswordBox:
			d2d->DrawRoundRect(frame, color, 1.35f, 3.0f);
			for (int i = -1; i <= 1; ++i)
				d2d->FillEllipse(cx + i * 6.0f, cy, 1.6f, 1.6f, color);
			break;
		case UIClass::UI_RichTextBox:
			d2d->DrawRect(frame, color, 1.25f);
			DrawMiniLines(d2d, frame.left + 4.0f, frame.top + 5.0f,
				frame.right - frame.left - 8.0f, 4, color);
			break;
		case UIClass::UI_DateTimePicker:
			d2d->DrawRoundRect(frame, color, 1.3f, 2.0f);
			d2d->DrawLine(frame.left, frame.top + 6.0f,
				frame.right, frame.top + 6.0f, color, 1.3f);
			d2d->FillRect(cx - 5.0f, cy, 3.0f, 3.0f, color);
			d2d->FillRect(cx + 2.0f, cy, 3.0f, 3.0f, color);
			break;
		case UIClass::UI_NumericUpDown:
			monogram(L"#");
			d2d->DrawLine(frame.right - 4.0f, cy,
				frame.right, cy - 4.0f, color, 1.2f);
			d2d->DrawLine(frame.right - 4.0f, cy,
				frame.right, cy + 4.0f, color, 1.2f);
			break;
		case UIClass::UI_Panel:
			d2d->DrawRect(frame, color, 1.4f);
			break;
		case UIClass::UI_GroupBox:
			d2d->DrawRoundRect(frame, color, 1.3f, 2.0f);
			d2d->FillRect(frame.left + 4.0f, frame.top - 1.0f, 8.0f, 3.0f,
				WithAlpha(color, 0.24f));
			break;
		case UIClass::UI_Expander:
			d2d->DrawRect(frame, color, 1.25f);
			d2d->DrawLine(frame.left + 4.0f, frame.top + 5.0f,
				frame.left + 8.0f, frame.top + 9.0f, color, 1.4f);
			d2d->DrawLine(frame.left + 8.0f, frame.top + 9.0f,
				frame.left + 12.0f, frame.top + 5.0f, color, 1.4f);
			break;
		case UIClass::UI_ScrollView:
			d2d->DrawRect(frame, color, 1.25f);
			d2d->FillRoundRect(frame.right - 3.0f, frame.top + 3.0f,
				2.0f, 8.0f, color, 1.0f);
			break;
		case UIClass::UI_StackPanel:
			for (int i = 0; i < 3; ++i)
				d2d->DrawRoundRect(frame.left + 2.0f, frame.top + 1.0f + i * 6.0f,
					frame.right - frame.left - 4.0f, 4.0f, color, 1.1f, 1.5f);
			break;
		case UIClass::UI_GridPanel:
			d2d->DrawRect(frame, color, 1.25f);
			d2d->DrawLine(cx, frame.top, cx, frame.bottom, color, 1.1f);
			d2d->DrawLine(frame.left, cy, frame.right, cy, color, 1.1f);
			break;
		case UIClass::UI_DockPanel:
			d2d->DrawRect(frame, color, 1.25f);
			d2d->FillRect(frame.left + 2.0f, frame.top + 2.0f,
				4.0f, frame.bottom - frame.top - 4.0f, WithAlpha(color, 0.75f));
			d2d->FillRect(frame.left + 8.0f, frame.top + 2.0f,
				frame.right - frame.left - 10.0f, 4.0f, WithAlpha(color, 0.45f));
			break;
		case UIClass::UI_WrapPanel:
			for (int row = 0; row < 2; ++row)
				for (int col = 0; col < 3; ++col)
					d2d->DrawRoundRect(frame.left + col * 7.0f,
						frame.top + row * 8.0f, 5.0f, 5.0f, color, 1.0f, 1.0f);
			break;
		case UIClass::UI_RelativePanel:
			d2d->FillEllipse(frame.left + 4.0f, frame.top + 4.0f, 2.0f, 2.0f, color);
			d2d->FillEllipse(frame.right - 4.0f, frame.bottom - 4.0f, 2.0f, 2.0f, color);
			d2d->DrawLine(frame.left + 6.0f, frame.top + 6.0f,
				frame.right - 6.0f, frame.bottom - 6.0f, color, 1.25f);
			break;
		case UIClass::UI_SplitContainer:
			d2d->DrawRect(frame, color, 1.25f);
			d2d->DrawLine(cx, frame.top, cx, frame.bottom, color, 2.0f);
			d2d->FillEllipse(cx, cy, 1.5f, 1.5f, color);
			break;
		case UIClass::UI_CheckBox:
			d2d->DrawRoundRect(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, color, 1.4f, 2.0f);
			d2d->DrawLine(cx - 4.0f, cy, cx - 1.0f, cy + 3.0f, color, 1.6f);
			d2d->DrawLine(cx - 1.0f, cy + 3.0f, cx + 5.0f, cy - 4.0f, color, 1.6f);
			break;
		case UIClass::UI_RadioBox:
			d2d->DrawEllipse(cx, cy, 7.0f, 7.0f, color, 1.4f);
			d2d->FillEllipse(cx, cy, 3.0f, 3.0f, color);
			break;
		case UIClass::UI_ComboBox:
			d2d->DrawRoundRect(frame, color, 1.25f, 2.0f);
			d2d->DrawLine(frame.right - 7.0f, cy - 2.0f,
				frame.right - 4.0f, cy + 2.0f, color, 1.3f);
			d2d->DrawLine(frame.right - 4.0f, cy + 2.0f,
				frame.right - 1.0f, cy - 2.0f, color, 1.3f);
			break;
		case UIClass::UI_ListView:
		case UIClass::UI_ListBox:
			for (int i = 0; i < 3; ++i)
			{
				d2d->FillEllipse(frame.left + 2.0f, frame.top + 3.0f + i * 6.0f,
					1.3f, 1.3f, color);
				d2d->DrawLine(frame.left + 6.0f, frame.top + 3.0f + i * 6.0f,
					frame.right, frame.top + 3.0f + i * 6.0f, color, 1.2f);
			}
			if (type == UIClass::UI_ListView)
				d2d->DrawLine(frame.right - 5.0f, frame.top,
					frame.right - 5.0f, frame.bottom, color, 1.0f);
			break;
		case UIClass::UI_GridView:
		case UIClass::UI_PropertyGrid:
			d2d->DrawRect(frame, color, 1.2f);
			d2d->DrawLine(frame.left, frame.top + 5.0f, frame.right, frame.top + 5.0f, color, 1.0f);
			d2d->DrawLine(cx, frame.top, cx, frame.bottom, color, 1.0f);
			if (type == UIClass::UI_GridView)
				d2d->DrawLine(frame.left, cy + 3.0f, frame.right, cy + 3.0f, color, 1.0f);
			else
				d2d->FillRect(frame.left + 1.0f, frame.top + 6.0f,
					frame.right - frame.left - 2.0f, 3.0f, WithAlpha(color, 0.35f));
			break;
		case UIClass::UI_ChartView:
			d2d->DrawLine(frame.left, frame.bottom, frame.right, frame.bottom, color, 1.1f);
			d2d->DrawLine(frame.left, frame.bottom, frame.left, frame.top, color, 1.1f);
			d2d->DrawLine(frame.left + 3.0f, frame.bottom - 4.0f,
				cx - 1.0f, cy + 2.0f, color, 1.5f);
			d2d->DrawLine(cx - 1.0f, cy + 2.0f,
				frame.right - 2.0f, frame.top + 3.0f, color, 1.5f);
			break;
		case UIClass::UI_ReportView:
			d2d->DrawRect(frame, color, 1.2f);
			DrawMiniLines(d2d, frame.left + 4.0f, frame.top + 5.0f,
				frame.right - frame.left - 8.0f, 3, color);
			d2d->FillRect(frame.left + 4.0f, frame.bottom - 5.0f, 4.0f, 3.0f, color);
			d2d->FillRect(frame.left + 10.0f, frame.bottom - 8.0f, 4.0f, 6.0f, color);
			break;
		case UIClass::UI_KpiCard:
			d2d->DrawRoundRect(frame, color, 1.2f, 3.0f);
			d2d->FillRect(frame.left + 4.0f, frame.bottom - 5.0f, 3.0f, 3.0f, color);
			d2d->FillRect(frame.left + 9.0f, frame.bottom - 9.0f, 3.0f, 7.0f, color);
			d2d->FillRect(frame.left + 14.0f, frame.bottom - 13.0f, 3.0f, 11.0f, color);
			break;
		case UIClass::UI_FilterBar:
			d2d->DrawLine(frame.left, frame.top, cx - 2.0f, cy, color, 1.5f);
			d2d->DrawLine(frame.right, frame.top, cx + 2.0f, cy, color, 1.5f);
			d2d->DrawLine(cx - 2.0f, cy, cx - 2.0f, frame.bottom, color, 1.5f);
			d2d->DrawLine(cx + 2.0f, cy, cx - 2.0f, frame.bottom, color, 1.5f);
			break;
		case UIClass::UI_TreeView:
			d2d->DrawLine(frame.left + 4.0f, frame.top + 3.0f,
				frame.left + 4.0f, frame.bottom - 3.0f, color, 1.1f);
			for (int i = 0; i < 3; ++i)
			{
				const float y = frame.top + 3.0f + i * 7.0f;
				d2d->DrawLine(frame.left + 4.0f, y, frame.left + 10.0f, y, color, 1.1f);
				d2d->FillEllipse(frame.left + 12.0f, y, 2.0f, 2.0f, color);
			}
			break;
		case UIClass::UI_ProgressBar:
			d2d->DrawRoundRect(cx - 9.0f, cy - 3.0f, 18.0f, 6.0f, color, 1.1f, 3.0f);
			d2d->FillRoundRect(cx - 8.0f, cy - 2.0f, 10.0f, 4.0f, color, 2.0f);
			break;
		case UIClass::UI_LoadingRing:
			d2d->DrawArc(D2D1::Point2F(cx, cy), 7.0f, 15.0f, 255.0f, color, 2.0f);
			break;
		case UIClass::UI_ProgressRing:
			d2d->DrawArc(D2D1::Point2F(cx, cy), 7.0f, 0.0f, 285.0f, color, 2.0f);
			d2d->FillEllipse(cx, cy, 1.5f, 1.5f, color);
			break;
		case UIClass::UI_Slider:
			d2d->DrawLine(frame.left, cy, frame.right, cy, color, 2.0f);
			d2d->FillEllipse(cx + 3.0f, cy, 4.0f, 4.0f, color);
			break;
		case UIClass::UI_PictureBox:
			d2d->DrawRect(frame, color, 1.2f);
			d2d->FillEllipse(frame.right - 5.0f, frame.top + 5.0f, 2.0f, 2.0f, color);
			d2d->DrawLine(frame.left + 2.0f, frame.bottom - 2.0f,
				cx - 1.0f, cy, color, 1.2f);
			d2d->DrawLine(cx - 1.0f, cy,
				frame.right - 2.0f, frame.bottom - 2.0f, color, 1.2f);
			break;
		case UIClass::UI_Switch:
			d2d->DrawRoundRect(cx - 9.0f, cy - 5.0f, 18.0f, 10.0f, color, 1.3f, 5.0f);
			d2d->FillEllipse(cx + 4.0f, cy, 3.0f, 3.0f, color);
			break;
		case UIClass::UI_TabControl:
			d2d->DrawRect(frame, color, 1.2f);
			d2d->DrawRoundRect(frame.left + 2.0f, frame.top - 3.0f,
				8.0f, 6.0f, color, 1.1f, 2.0f);
			d2d->DrawRoundRect(frame.left + 11.0f, frame.top - 3.0f,
				8.0f, 6.0f, WithAlpha(color, 0.65f), 1.0f, 2.0f);
			break;
		case UIClass::UI_ToolBar:
		case UIClass::UI_Menu:
		case UIClass::UI_StatusBar:
			d2d->DrawRoundRect(frame, color, 1.2f, 2.0f);
			for (int i = 0; i < 3; ++i)
			{
				if (type == UIClass::UI_Menu)
					d2d->DrawLine(frame.left + 3.0f + i * 7.0f, cy,
						frame.left + 7.0f + i * 7.0f, cy, color, 1.2f);
				else
					d2d->FillRoundRect(frame.left + 3.0f + i * 7.0f,
						cy - 2.0f, 4.0f, 4.0f, color, 1.0f);
			}
			if (type == UIClass::UI_StatusBar)
				d2d->DrawLine(frame.left, frame.top + 4.0f,
					frame.right, frame.top + 4.0f, color, 1.0f);
			break;
		case UIClass::UI_ToastHost:
			d2d->DrawRoundRect(frame, color, 1.2f, 4.0f);
			d2d->FillEllipse(frame.left + 5.0f, cy, 2.0f, 2.0f, color);
			DrawMiniLines(d2d, frame.left + 9.0f, cy - 3.0f,
				frame.right - frame.left - 12.0f, 2, color);
			break;
		case UIClass::UI_WebBrowser:
			d2d->DrawEllipse(cx, cy, 8.0f, 8.0f, color, 1.2f);
			d2d->DrawEllipse(cx, cy, 3.5f, 8.0f, color, 1.0f);
			d2d->DrawLine(cx - 8.0f, cy, cx + 8.0f, cy, color, 1.0f);
			break;
		case UIClass::UI_MediaPlayer:
			d2d->DrawRoundRect(frame, color, 1.2f, 3.0f);
			d2d->DrawLine(cx - 3.0f, cy - 5.0f, cx + 5.0f, cy, color, 1.5f);
			d2d->DrawLine(cx + 5.0f, cy, cx - 3.0f, cy + 5.0f, color, 1.5f);
			d2d->DrawLine(cx - 3.0f, cy + 5.0f, cx - 3.0f, cy - 5.0f, color, 1.5f);
			break;
		default:
			monogram(L"C");
			break;
		}
	}

	std::wstring Lower(std::wstring value)
	{
		for (auto& ch : value) ch = static_cast<wchar_t>(towlower(ch));
		return value;
	}

	std::wstring FitSingleLine(
		Font* font,
		std::wstring value,
		float maxWidth)
	{
		if (!font || value.empty() || maxWidth <= 0.0f) return value;
		if (font->GetTextSize(value).width <= maxWidth) return value;
		const std::wstring ellipsis = L"…";
		while (!value.empty())
		{
			value.pop_back();
			if (font->GetTextSize(value + ellipsis).width <= maxWidth)
				return value + ellipsis;
		}
		return ellipsis;
	}

	bool MatchesFilter(ToolBoxItem& item, const std::wstring& filter)
	{
		if (filter.empty()) return true;
		const auto searchable = Lower(
			item.Text + L" " + item.TypeName + L" " + item.Category);
		std::wistringstream tokens(Lower(filter));
		std::wstring token;
		while (tokens >> token)
			if (searchable.find(token) == std::wstring::npos) return false;
		return true;
	}
}

void ToolBoxItem::Update()
{
	if (!this->IsVisual || !this->ParentForm || !this->ParentForm->Render) return;
	const bool isUnderMouse = this->ParentForm->UnderMouse == this;
	const bool isSelected = this->ParentForm->Selected == this;
	auto* d2d = this->ParentForm->Render;
	const auto size = this->ActualSize();
	const float width = static_cast<float>(size.cx);
	const float height = static_cast<float>(size.cy);
	const auto accent = CategoryAccent(this->Category);
	this->BeginRender();
	{
		const float radius = 6.0f;
		d2d->FillRoundRect(1.0f, 1.0f, width - 2.0f, height - 2.0f,
			isSelected ? WithAlpha(accent, 0.16f)
			: (isUnderMouse ? WithAlpha(accent, 0.09f) : Colors::White),
			radius);
		d2d->DrawRoundRect(1.0f, 1.0f, width - 2.0f, height - 2.0f,
			isSelected ? WithAlpha(accent, 0.78f)
			: (isUnderMouse ? WithAlpha(accent, 0.45f)
				: D2D1::ColorF(0.42f, 0.47f, 0.56f, 0.18f)),
			1.0f, radius);

		const auto iconRect = D2D1::RectF(7.0f, 4.0f, 39.0f, height - 4.0f);
		d2d->FillRoundRect(iconRect, WithAlpha(accent, 0.12f), 6.0f);
		DrawControlGlyph(d2d, this->ControlType, iconRect, accent, this->Font);

		const float textLeft = 48.0f;
		const float textWidth = std::max(1.0f, width - textLeft - 8.0f);
		const auto primaryColor = this->Enable
			? D2D1::ColorF(0.12f, 0.14f, 0.18f, 1.0f)
			: D2D1::ColorF(0.45f, 0.48f, 0.53f, 1.0f);
		d2d->DrawString(FitSingleLine(this->Font, this->Text, textWidth), textLeft, 2.0f,
			textWidth, height * 0.55f,
			primaryColor, this->Font);
		d2d->DrawString(FitSingleLine(this->Font, this->TypeName, textWidth), textLeft, height * 0.46f,
			textWidth, height * 0.5f,
			D2D1::ColorF(0.38f, 0.42f, 0.49f, 0.88f), this->Font);
	}
	if (!this->Enable)
		d2d->FillRoundRect(1.0f, 1.0f, width - 2.0f, height - 2.0f,
			D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.48f), 6.0f);
	this->EndRender();
}

ToolBox::ToolBox(
	int x, int y, int width, int height,
	std::vector<DesignerControlDescriptor> descriptors)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1::ColorF(0.95f, 0.96f, 0.98f, 1.0f);
	this->BorderThickness = 1.0f;

	_titleLabel = new Label(L"工具箱", 10, 8);
	_titleLabel->Size = { width - 20, 25 };
	_titleLabel->Font = new ::Font(L"Microsoft YaHei", 16.0f);
	this->AddControl(_titleLabel);

	_filterLabel = new Label(L"搜索", 10, 42);
	_filterLabel->Size = { 40, 22 };
	_filterLabel->Font = new ::Font(L"Microsoft YaHei", 11.0f);
	_filterLabel->AccessibleName = L"工具箱搜索标签";
	this->AddControl(_filterLabel);

	_filterBox = new TextBox(L"", 52, 39, std::max(0, width - 62), 25);
	_filterBox->AccessibleName = L"搜索工具箱控件";
	_filterBox->AccessibleDescription = L"按中文名称、控件类型或分类筛选。";
	_filterBox->OnTextChanged += [this](Control*, std::wstring, std::wstring value)
	{
		_filterText = std::move(value);
		ApplyFilterLayout();
		InvalidateVisual();
	};
	this->AddControl(_filterBox);

	_scrollView = new ScrollView(
		0, _contentTop, width, std::max(0, height - _contentTop));
	_scrollView->BackColor = D2D1::ColorF(0, 0, 0, 0);
	_scrollView->BorderThickness = 0.0f;
	_scrollView->MouseWheelStep = 42;
	this->AddControl(_scrollView);

	_itemsHost = new Panel(0, 0, width, std::max(0, height - _contentTop));
	_itemsHost->BackColor = D2D1::ColorF(0, 0, 0, 0);
	_itemsHost->BorderThickness = 0.0f;
	_scrollView->AddControl(_itemsHost);

	if (descriptors.empty())
	{
		for (const auto& metadata : ControlRegistry::GetAvailableControls())
			descriptors.push_back(DesignerControlDescriptor::BuiltIn(
				metadata, ToolCategory(metadata.Type)));
	}
	else
	{
		for (auto& descriptor : descriptors)
			if (descriptor.Category.empty())
				descriptor.Category = ToolCategory(descriptor.Type);
	}

	std::vector<std::wstring> categories = {
		L"基础控件", L"输入", L"布局", L"数据与列表",
		L"状态与反馈", L"导航与外壳", L"媒体与 Web"
	};
	for (const auto& descriptor : descriptors)
	{
		if (!descriptor.IsValid()) continue;
		if (std::find(categories.begin(), categories.end(),
			descriptor.Category) == categories.end())
			categories.push_back(descriptor.Category);
	}
	for (const auto& category : categories)
	{
		auto* heading = new Label(category, 12, 0);
		heading->Size = { width - 28, 24 };
		heading->Font = new ::Font(L"Microsoft YaHei", 11.0f);
		heading->ForeColor = CategoryAccent(category);
		heading->AccessibleName = category + L" 分类";
		_itemsHost->AddControl(heading);
		_categoryHeadings.push_back({ category, heading, 0 });
	}

	for (auto& descriptor : descriptors)
	{
		if (!descriptor.IsValid()) continue;
		auto* item = new ToolBoxItem(
			std::move(descriptor), 8, 0, width - 24, 40);
		item->Font = new ::Font(L"Microsoft YaHei", 10.5f);
		item->BackColor = Colors::White;
		item->BorderThickness = 1.0f;
		item->AccessibleName = item->Descriptor.DisplayName;
		item->AccessibleDescription = item->Descriptor.Name
			+ L"；分类：" + item->Descriptor.Category;
		item->OnMouseClick += [this, item](Control*, MouseEventArgs)
		{
			OnControlSelected(item->Descriptor);
		};
		_itemsHost->AddControl(item);
		_items.push_back(item);
	}

	_emptyLabel = new Label(L"没有匹配的控件", 12, 8);
	_emptyLabel->Size = { width - 28, 24 };
	_emptyLabel->ForeColor = Colors::DimGrey;
	_emptyLabel->Visible = false;
	_emptyLabel->AccessibleName = L"没有匹配的工具箱控件";
	_itemsHost->AddControl(_emptyLabel);

	_titleLabel->Text = L"工具箱 · " + std::to_wstring(_items.size());
	ApplyFilterLayout();
	UpdateScrollLayout();
}

ToolBox::~ToolBox()
{
}

void ToolBox::ApplyFilterLayout()
{
	int yOffset = 2;
	size_t visibleCount = 0;
	for (auto* item : _items)
		if (item) item->Visible = false;
	for (auto& heading : _categoryHeadings)
	{
		bool hasVisibleItems = false;
		for (auto* item : _items)
		{
			if (item && item->Category == heading.Name
				&& MatchesFilter(*item, _filterText))
			{
				hasVisibleItems = true;
				break;
			}
		}
		heading.LabelPtr->Visible = hasVisibleItems;
		if (!hasVisibleItems) continue;
		heading.BaseY = yOffset;
		heading.LabelPtr->Top = yOffset;
		yOffset += 25;
		for (auto* item : _items)
		{
			if (!item || item->Category != heading.Name) continue;
			item->Visible = MatchesFilter(*item, _filterText);
			if (!item->Visible) continue;
			item->BaseY = yOffset;
			item->Top = yOffset;
			yOffset += 44;
			++visibleCount;
		}
		yOffset += 5;
	}

	if (_emptyLabel)
	{
		_emptyLabel->Visible = visibleCount == 0;
		_emptyLabel->Top = 8;
	}
	_contentHeight = visibleCount == 0
		? 44 : yOffset + _contentBottomPadding;
	UpdateScrollLayout();
}

void ToolBox::UpdateScrollLayout()
{
	if (_titleLabel)
		_titleLabel->Size = { std::max(0, this->Width - 20), 25 };
	if (_filterLabel)
	{
		_filterLabel->Location = { 10, 42 };
		_filterLabel->Size = { 40, 22 };
	}
	if (_filterBox)
	{
		_filterBox->Location = { 52, 39 };
		_filterBox->Size = { std::max(0, this->Width - 62), 25 };
	}
	if (_scrollView)
	{
		_scrollView->Location = { 0, _contentTop };
		_scrollView->Size = {
			this->Width, std::max(0, this->Height - _contentTop) };
	}

	const int hostWidth = _scrollView
		? std::max(0, _scrollView->Width - 12) : this->Width;
	const int hostHeight = _scrollView
		? std::max(_contentHeight, _scrollView->Height) : _contentHeight;
	if (_itemsHost)
	{
		_itemsHost->Location = { 0, 0 };
		_itemsHost->Size = { hostWidth, hostHeight };
	}
	for (auto& heading : _categoryHeadings)
	{
		if (!heading.LabelPtr) continue;
		heading.LabelPtr->Left = 12;
		heading.LabelPtr->Width = std::max(0, hostWidth - 24);
		if (heading.LabelPtr->Visible) heading.LabelPtr->Top = heading.BaseY;
	}
	for (auto* item : _items)
	{
		if (!item) continue;
		item->Left = 8;
		item->Width = std::max(60, hostWidth - 16);
		if (item->Visible) item->Top = item->BaseY;
	}
	if (_emptyLabel)
		_emptyLabel->Width = std::max(0, hostWidth - 24);
}

void ToolBox::Update()
{
	UpdateScrollLayout();
	Panel::Update();
}

bool ToolBox::ProcessMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	int localX,
	int localY)
{
	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}

void ToolBox::SetFilterText(const std::wstring& value)
{
	_filterText = value;
	if (_filterBox && _filterBox->Text != value)
		_filterBox->Text = value;
	ApplyFilterLayout();
}

size_t ToolBox::GetVisibleItemCount() const noexcept
{
	return static_cast<size_t>(std::count_if(
		_items.begin(), _items.end(),
		[](ToolBoxItem* item) { return item && item->Visible; }));
}

size_t ToolBox::GetVisibleCategoryCount() const noexcept
{
	return static_cast<size_t>(std::count_if(
		_categoryHeadings.begin(), _categoryHeadings.end(),
		[](const CategoryHeading& heading)
		{ return heading.LabelPtr && heading.LabelPtr->Visible; }));
}
