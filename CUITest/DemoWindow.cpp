#include "DemoWindow.h"
#include "imgs.h"
#include <memory>
#include "../CUI/GUI/WebBrowser.h"

namespace {

	D2D1_COLOR_F Color(float r, float g, float b, float a = 1.0f)
	{
		return D2D1_COLOR_F{ r, g, b, a };
	}

	struct DemoThemePalette
	{
		FormThemeFrame Window;
		D2D1_COLOR_F Surface = Color(0.15f, 0.17f, 0.21f, 1.0f);
		D2D1_COLOR_F SurfaceAlt = Color(0.19f, 0.22f, 0.27f, 1.0f);
		D2D1_COLOR_F SurfacePanel = Color(0.20f, 0.23f, 0.28f, 0.92f);
		D2D1_COLOR_F SurfacePanelSoft = Color(0.23f, 0.26f, 0.31f, 0.96f);
		D2D1_COLOR_F InputBack = Color(0.17f, 0.18f, 0.20f, 0.98f);
		D2D1_COLOR_F InputHover = Color(0.24f, 0.25f, 0.28f, 1.0f);
		D2D1_COLOR_F Border = Color(0.46f, 0.50f, 0.58f, 0.95f);
		D2D1_COLOR_F BorderStrong = Color(0.76f, 0.80f, 0.87f, 0.65f);
		D2D1_COLOR_F Text = Color(0.95f, 0.96f, 0.98f, 1.0f);
		D2D1_COLOR_F TextMuted = Color(0.76f, 0.79f, 0.84f, 1.0f);
		D2D1_COLOR_F Accent = Color(0.28f, 0.63f, 0.98f, 0.98f);
		D2D1_COLOR_F AccentSoft = Color(0.34f, 0.66f, 0.98f, 0.30f);
		D2D1_COLOR_F AccentText = Color(0.98f, 0.99f, 1.0f, 1.0f);
		D2D1_COLOR_F Selection = Color(0.27f, 0.57f, 0.96f, 0.72f);
		D2D1_COLOR_F InputSelection = Color(0.10f, 0.52f, 0.98f, 0.96f);
		D2D1_COLOR_F ScrollTrack = Color(0.31f, 0.34f, 0.41f, 0.72f);
		D2D1_COLOR_F ScrollThumb = Color(0.63f, 0.69f, 0.80f, 0.92f);
	};

	const std::wstring& DemoThemeKeyLight()
	{
		static const std::wstring value = L"light";
		return value;
	}

	const std::wstring& DemoThemeKeyDark()
	{
		static const std::wstring value = L"dark";
		return value;
	}

	const std::wstring& DemoThemeLabelLight()
	{
		static const std::wstring value = L"亮色";
		return value;
	}

	const std::wstring& DemoThemeLabelDark()
	{
		static const std::wstring value = L"暗色";
		return value;
	}

	const DemoThemePalette& GetDemoThemePalette(const std::wstring& themeName)
	{
		static const DemoThemePalette light = []()
			{
				DemoThemePalette theme;
				theme.Window.WindowBackColor = Color(0.95f, 0.96f, 0.98f, 1.0f);
				theme.Window.WindowForeColor = Color(0.10f, 0.12f, 0.15f, 1.0f);
				theme.Window.WindowBorderLightColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
				theme.Window.WindowBorderDarkColor = Color(0.60f, 0.65f, 0.74f, 0.95f);
				theme.Window.TitleBarBackColor = Color(0.98f, 0.99f, 1.0f, 0.92f);
				theme.Window.CaptionHoverColor = Color(0.18f, 0.42f, 0.88f, 0.12f);
				theme.Window.CaptionPressedColor = Color(0.18f, 0.42f, 0.88f, 0.20f);
				theme.Window.CloseHoverColor = Color(0.88f, 0.26f, 0.26f, 0.24f);
				theme.Window.ClosePressedColor = Color(0.88f, 0.26f, 0.26f, 0.38f);
				theme.Surface = Color(0.95f, 0.96f, 0.98f, 1.0f);
				theme.SurfaceAlt = Color(0.91f, 0.93f, 0.96f, 1.0f);
				theme.SurfacePanel = Color(1.0f, 1.0f, 1.0f, 0.96f);
				theme.SurfacePanelSoft = Color(0.97f, 0.98f, 1.0f, 0.98f);
				theme.InputBack = Color(1.0f, 1.0f, 1.0f, 0.98f);
				theme.InputHover = Color(0.95f, 0.95f, 0.96f, 1.0f);
				theme.Border = Color(0.76f, 0.80f, 0.87f, 1.0f);
				theme.BorderStrong = Color(0.52f, 0.58f, 0.68f, 0.95f);
				theme.Text = Color(0.10f, 0.12f, 0.15f, 1.0f);
				theme.TextMuted = Color(0.33f, 0.38f, 0.46f, 1.0f);
				theme.Accent = Color(0.13f, 0.46f, 0.93f, 0.96f);
				theme.AccentSoft = Color(0.13f, 0.46f, 0.93f, 0.16f);
				theme.AccentText = Color(1.0f, 1.0f, 1.0f, 1.0f);
				theme.Selection = Color(0.19f, 0.50f, 0.95f, 0.26f);
				theme.InputSelection = Color(0.12f, 0.48f, 0.94f, 0.88f);
				theme.ScrollTrack = Color(0.81f, 0.84f, 0.89f, 0.95f);
				theme.ScrollThumb = Color(0.46f, 0.54f, 0.66f, 0.92f);
				return theme;
			}();

		static const DemoThemePalette dark = []()
			{
				DemoThemePalette theme;
				theme.Window.WindowBackColor = Color(0.15f, 0.17f, 0.21f, 1.0f);
				theme.Window.WindowForeColor = Color(0.95f, 0.96f, 0.98f, 1.0f);
				theme.Window.WindowBorderLightColor = Color(0.80f, 0.84f, 0.90f, 0.28f);
				theme.Window.WindowBorderDarkColor = Color(0.05f, 0.06f, 0.09f, 0.95f);
				theme.Window.TitleBarBackColor = Color(0.10f, 0.12f, 0.15f, 0.94f);
				theme.Window.CaptionHoverColor = Color(1.0f, 1.0f, 1.0f, 0.14f);
				theme.Window.CaptionPressedColor = Color(1.0f, 1.0f, 1.0f, 0.24f);
				theme.Window.CloseHoverColor = Color(0.90f, 0.24f, 0.24f, 0.42f);
				theme.Window.ClosePressedColor = Color(0.90f, 0.24f, 0.24f, 0.58f);
				theme.InputBack = Color(0.16f, 0.17f, 0.19f, 0.98f);
				theme.InputHover = Color(0.22f, 0.23f, 0.26f, 1.0f);
				return theme;
			}();

		return themeName == DemoThemeKeyLight() ? light : dark;
	}

	void ApplyTextInputTheme(TextBox* textBox, const DemoThemePalette& theme)
	{
		if (!textBox) return;
		textBox->BackColor = theme.InputBack;
		textBox->ForeColor = theme.Text;
		textBox->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
		textBox->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.055f);
		textBox->FocusedColor = theme.Accent;
		textBox->SelectedBackColor = theme.InputSelection;
		textBox->SelectedForeColor = theme.AccentText;
		textBox->ScrollBackColor = theme.ScrollTrack;
		textBox->ScrollForeColor = theme.ScrollThumb;
		textBox->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
		textBox->CornerRadius = 6.0f;
		textBox->FocusBorder = 1.6f;
	}

	void ApplyPasswordTheme(PasswordBox* textBox, const DemoThemePalette& theme)
	{
		if (!textBox) return;
		textBox->BackColor = theme.InputBack;
		textBox->ForeColor = theme.Text;
		textBox->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
		textBox->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.055f);
		textBox->FocusedColor = theme.Accent;
		textBox->SelectedBackColor = theme.InputSelection;
		textBox->SelectedForeColor = theme.AccentText;
		textBox->ScrollBackColor = theme.ScrollTrack;
		textBox->ScrollForeColor = theme.ScrollThumb;
		textBox->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
		textBox->CornerRadius = 6.0f;
		textBox->FocusBorder = 1.6f;
	}

	void ApplyRichTextTheme(RichTextBox* textBox, const DemoThemePalette& theme)
	{
		if (!textBox) return;
		textBox->BackColor = theme.InputBack;
		textBox->ForeColor = theme.Text;
		textBox->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
		textBox->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.055f);
		textBox->FocusedColor = theme.Accent;
		textBox->SelectedBackColor = theme.InputSelection;
		textBox->SelectedForeColor = theme.AccentText;
		textBox->ScrollBackColor = theme.ScrollTrack;
		textBox->ScrollForeColor = theme.ScrollThumb;
		textBox->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
		textBox->CornerRadius = 7.0f;
		textBox->FocusBorder = 1.6f;
	}

	std::wstring ToJsStringLiteral(const std::wstring& s)
	{
		std::wstring out;
		out.reserve(s.size() + 8);
		out.push_back(L'"');
		for (wchar_t c : s)
		{
			switch (c)
			{
			case L'\\': out += L"\\\\"; break;
			case L'"': out += L"\\\""; break;
			case L'\r': out += L"\\r"; break;
			case L'\n': out += L"\\n"; break;
			case L'\t': out += L"\\t"; break;
			default:
				if (c >= 0 && c < 0x20)
				{
					wchar_t buf[8];
					swprintf_s(buf, L"\\u%04x", (unsigned)c);
					out += buf;
				}
				else
				{
					out.push_back(c);
				}
				break;
			}
		}
		out.push_back(L'"');
		return out;
	}

	std::wstring FileNameFromPath(const std::wstring& path)
	{
		size_t pos = path.find_last_of(L"\\/");
		return (pos != std::wstring::npos) ? path.substr(pos + 1) : path;
	}

}

void DemoWindow::Theme_OnSelectionChanged(class Control* sender)
{
	(void)sender;
	if (!_themeSelector)
	{
		return;
	}

	Theme_Apply(_themeSelector->Text == DemoThemeLabelLight() ? DemoThemeKeyLight() : DemoThemeKeyDark());
}

void DemoWindow::Theme_Apply(const std::wstring& themeName)
{
	this->ApplyThemeFrame(GetDemoThemePalette(themeName).Window, themeName);
}

void DemoWindow::Theme_ApplyCurrent()
{
	const auto& theme = GetDemoThemePalette(this->GetThemeName());

	auto applyControlTheme = [&](auto&& self, Control* control) -> void
		{
			if (!control)
			{
				return;
			}

			control->ForeColor = theme.Text;
			control->BolderColor = theme.Border;

			switch (control->Type())
			{
			case UIClass::UI_Label:
				control->BackColor = Color(0, 0, 0, 0);
				control->ForeColor = theme.TextMuted;
				break;
			case UIClass::UI_Button:
			{
				auto* button = (Button*)control;
				const bool inToolbar = button->Parent && button->Parent->Type() == UIClass::UI_ToolBar;
				button->ForeColor = theme.Text;
				button->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.11f);
				button->CheckedColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
				button->HighlightColor = Color(1.0f, 1.0f, 1.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.10f : 0.035f);
				button->ShadowColor = Color(0.0f, 0.0f, 0.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.04f : 0.08f);
				button->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				button->Raised = false;
				if (inToolbar)
				{
					button->BackColor = Color(0, 0, 0, 0);
					button->BolderColor = Color(0, 0, 0, 0);
					button->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
					button->CheckedColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
					button->HighlightColor = Color(0, 0, 0, 0);
					button->ShadowColor = Color(0, 0, 0, 0);
					button->Boder = 0.0f;
				}
				else
				{
					button->BackColor = theme.SurfaceAlt;
					button->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.52f);
					button->Boder = 1.0f;
				}
				break;
			}
			case UIClass::UI_CheckBox:
			{
				auto* check = (CheckBox*)control;
				check->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				check->BoxBackColor = Color(theme.InputBack.r, theme.InputBack.g, theme.InputBack.b, 0.68f);
				check->BoxBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.72f);
				check->CheckedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.92f);
				check->CheckMarkColor = theme.AccentText;
				check->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				check->BoxCornerRadius = 4.0f;
				break;
			}
			case UIClass::UI_RadioBox:
			{
				auto* radio = (RadioBox*)control;
				radio->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				radio->CircleBackColor = Color(theme.InputBack.r, theme.InputBack.g, theme.InputBack.b, 0.68f);
				radio->CircleBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.72f);
				radio->SelectedColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.92f);
				radio->DotColor = theme.AccentText;
				radio->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_Switch:
			{
				auto* sw = (Switch*)control;
				sw->UnderMouseColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				sw->TrackOffColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.34f : 0.28f);
				sw->TrackOnColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.92f);
				sw->TrackBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.42f);
				sw->ThumbColor = Color(0.98f, 0.99f, 1.0f, 1.0f);
				sw->ThumbShadowColor = Color(0.0f, 0.0f, 0.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.18f : 0.30f);
				sw->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				sw->TrackPadding = 3.0f;
				break;
			}
			case UIClass::UI_TextBox:
				ApplyTextInputTheme((TextBox*)control, theme);
				break;
			case UIClass::UI_PasswordBox:
				ApplyPasswordTheme((PasswordBox*)control, theme);
				break;
			case UIClass::UI_RichTextBox:
				ApplyRichTextTheme((RichTextBox*)control, theme);
				break;
			case UIClass::UI_ComboBox:
			{
				auto* combo = (ComboBox*)control;
				const bool inToolbar = combo->Parent && combo->Parent->Type() == UIClass::UI_ToolBar;
				combo->BackColor = inToolbar ? theme.SurfaceAlt : theme.InputBack;
				combo->ForeColor = theme.Text;
				combo->BolderColor = inToolbar ? Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.55f) : theme.Border;
				combo->AccentColor = theme.Accent;
				combo->HeaderHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.06f);
				combo->DropBackColor = theme.SurfacePanel;
				combo->DropBorderColor = theme.Border;
				combo->SelectedItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				combo->UnderMouseBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				combo->SelectedItemForeColor = theme.Text;
				combo->UnderMouseForeColor = theme.Text;
				combo->ScrollBackColor = theme.ScrollTrack;
				combo->ScrollForeColor = theme.ScrollThumb;
				combo->ButtonBackColor = inToolbar ? Color(0, 0, 0, 0) : theme.Accent;
				if (inToolbar)
				{
					combo->CornerRadius = 6.0f;
					combo->DropCornerRadius = 8.0f;
					combo->DropGap = 5.0f;
				}
				break;
			}
			case UIClass::UI_DateTimePicker:
			{
				auto* picker = (DateTimePicker*)control;
				picker->BackColor = theme.InputBack;
				picker->ForeColor = theme.Text;
				picker->BolderColor = theme.Border;
				picker->PanelBackColor = theme.InputBack;
				picker->DropBackColor = theme.SurfacePanel;
				picker->DropBorderColor = theme.Border;
				picker->AccentColor = theme.Accent;
				picker->HeaderHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.06f);
				picker->HoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				picker->SelectedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				picker->SelectedForeColor = theme.Text;
				picker->SecondaryTextColor = theme.TextMuted;
				picker->FocusBorderColor = theme.Accent;
				break;
			}
			case UIClass::UI_DateRangePicker:
			{
				auto* picker = (DateRangePicker*)control;
				picker->PanelBackColor = theme.InputBack;
				picker->ForeColor = theme.Text;
				picker->BolderColor = theme.Border;
				picker->DropBackColor = theme.SurfacePanel;
				picker->DropBorderColor = theme.Border;
				picker->AccentColor = theme.Accent;
				picker->HeaderHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.06f);
				picker->HoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				picker->SelectedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				picker->RangeBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.09f);
				picker->SelectedForeColor = theme.Text;
				picker->SecondaryTextColor = theme.TextMuted;
				picker->FocusBorderColor = theme.Accent;
				picker->ButtonBackColor = theme.Accent;
				picker->ButtonTextColor = theme.AccentText;
				break;
			}
			case UIClass::UI_ColorPicker:
			{
				auto* picker = (ColorPicker*)control;
				picker->PanelBackColor = theme.InputBack;
				picker->PanelHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.08f);
				picker->ButtonBackColor = theme.SurfaceAlt;
				picker->ForeColor = theme.Text;
				picker->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
				picker->AccentColor = theme.Accent;
				picker->FocusBorderColor = theme.Accent;
				picker->MutedTextColor = theme.TextMuted;
				picker->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_NumericUpDown:
			{
				auto* number = (NumericUpDown*)control;
				number->PanelBackColor = theme.InputBack;
				number->ForeColor = theme.Text;
				number->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
				number->ButtonBackColor = theme.SurfaceAlt;
				number->ButtonHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.12f);
				number->ButtonPressedColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.22f);
				number->AccentColor = theme.Accent;
				number->FocusBorderColor = theme.Accent;
				number->MutedTextColor = theme.TextMuted;
				number->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				number->CornerRadius = 6.0f;
				number->ButtonWidth = 28.0f;
				break;
			}
			case UIClass::UI_CalendarView:
			{
				auto* calendar = (CalendarView*)control;
				calendar->BackColor = theme.SurfacePanelSoft;
				calendar->ForeColor = theme.Text;
				calendar->BolderColor = theme.Border;
				calendar->SurfaceColor = theme.SurfacePanelSoft;
				calendar->HeaderBackColor = theme.SurfaceAlt;
				calendar->MutedTextColor = theme.TextMuted;
				calendar->TrailingTextColor = Color(theme.TextMuted.r, theme.TextMuted.g, theme.TextMuted.b, 0.62f);
				calendar->HoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				calendar->SelectedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				calendar->RangeBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.09f);
				calendar->SelectedForeColor = theme.Text;
				calendar->AccentColor = theme.Accent;
				break;
			}
			case UIClass::UI_NavigationView:
			case UIClass::UI_SideBar:
			{
				auto* nav = (NavigationView*)control;
				nav->BackColor = theme.SurfacePanelSoft;
				nav->ForeColor = theme.Text;
				nav->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.58f);
				nav->SurfaceColor = theme.SurfacePanelSoft;
				nav->HeaderBackColor = theme.SurfaceAlt;
				nav->MutedTextColor = theme.TextMuted;
				nav->SelectedItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				nav->SelectedItemForeColor = theme.Text;
				nav->UnderMouseItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				nav->AccentColor = theme.Accent;
				nav->IconPlaceholderColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
				nav->BadgeBackColor = theme.Accent;
				nav->BadgeForeColor = theme.AccentText;
				nav->SeparatorColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.45f);
				nav->ScrollBackColor = theme.ScrollTrack;
				nav->ScrollForeColor = theme.ScrollThumb;
				break;
			}
			case UIClass::UI_BreadcrumbBar:
			{
				auto* breadcrumb = (BreadcrumbBar*)control;
				breadcrumb->BackColor = theme.SurfacePanelSoft;
				breadcrumb->ForeColor = theme.Text;
				breadcrumb->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.50f);
				breadcrumb->SurfaceColor = theme.SurfacePanelSoft;
				breadcrumb->HoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				breadcrumb->SelectedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				breadcrumb->MutedTextColor = theme.TextMuted;
				breadcrumb->AccentColor = theme.Accent;
				break;
			}
			case UIClass::UI_Menu:
			{
				auto* menu = (Menu*)control;
				menu->BackColor = Color(0, 0, 0, 0);
				menu->ForeColor = theme.Text;
				menu->BarBackColor = Color(theme.SurfacePanel.r, theme.SurfacePanel.g, theme.SurfacePanel.b, 0.72f);
				menu->BarBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.34f);
				menu->BarItemHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.08f);
				menu->BarItemActiveColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.14f);
				menu->DropBackColor = theme.SurfacePanelSoft;
				menu->DropBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
				menu->DropHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.12f);
				menu->DropTextColor = theme.Text;
				menu->DropSeparatorColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.45f);
				menu->BarItemCornerRadius = 6.0f;
				menu->DropCornerRadius = 8.0f;
				menu->DropItemCornerRadius = 6.0f;
				menu->DropItemHorizontalInset = 6.0f;
				menu->PopupAnimationDurationMs = 95;
				break;
			}
			case UIClass::UI_MenuItem:
			{
				auto* item = (MenuItem*)control;
				item->HoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.08f);
				item->ActiveBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.14f);
				item->CornerRadius = 6.0f;
				break;
			}
			case UIClass::UI_ContextMenu:
			{
				auto* popup = (ContextMenu*)control;
				popup->PopupBackColor = theme.SurfacePanelSoft;
				popup->PopupBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
				popup->PopupHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.12f);
				popup->PopupTextColor = theme.Text;
				popup->PopupSeparatorColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.45f);
				popup->PopupCornerRadius = 8.0f;
				popup->ItemCornerRadius = 6.0f;
				popup->ItemHorizontalInset = 6.0f;
				popup->PopupAnimationDurationMs = 95;
				break;
			}
			case UIClass::UI_ToolTip:
			{
				auto* tip = (ToolTip*)control;
				tip->PopupBackColor = theme.SurfacePanelSoft;
				tip->PopupBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
				tip->PopupTextColor = theme.Text;
				tip->PopupShadowColor = Color(0.0f, 0.0f, 0.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.14f : 0.22f);
				tip->CornerRadius = 8.0f;
				tip->PopupAnimationDurationMs = 90;
				break;
			}
			case UIClass::UI_ToastHost:
			{
				auto* toast = (ToastHost*)control;
				toast->ToastBackColor = theme.SurfacePanelSoft;
				toast->ToastBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.62f);
				toast->TitleColor = theme.Text;
				toast->MessageColor = theme.TextMuted;
				toast->CloseHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.12f);
				toast->InfoColor = theme.Accent;
				toast->SuccessColor = Color(0.10f, 0.68f, 0.48f, 1.0f);
				toast->WarningColor = Color(0.95f, 0.62f, 0.18f, 1.0f);
				toast->ErrorColor = Color(0.90f, 0.20f, 0.24f, 1.0f);
				break;
			}
			case UIClass::UI_ToolBar:
			{
				auto* toolbar = (ToolBar*)control;
				toolbar->BackColor = theme.SurfacePanelSoft;
				toolbar->ForeColor = theme.Text;
				toolbar->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.42f);
				toolbar->SeparatorColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.52f);
				toolbar->BottomLineColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.34f);
				toolbar->CornerRadius = 8.0f;
				break;
			}
			case UIClass::UI_StatusBar:
			{
				auto* status = (StatusBar*)control;
				status->BackColor = theme.SurfacePanelSoft;
				status->ForeColor = theme.TextMuted;
				status->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.34f);
				status->TopLineColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.38f);
				status->SeparatorColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.34f);
				status->PartBackColor = Color(0, 0, 0, 0);
				status->PartBorderColor = Color(0, 0, 0, 0);
				status->CornerRadius = 0.0f;
				status->ShowBorder = false;
				status->UsePartPills = false;
				break;
			}
			case UIClass::UI_Panel:
			case UIClass::UI_StackPanel:
			case UIClass::UI_GridPanel:
			case UIClass::UI_DockPanel:
			case UIClass::UI_WrapPanel:
			case UIClass::UI_RelativePanel:
			case UIClass::UI_TabPage:
			{
				auto* panel = (Panel*)control;
				control->BackColor = theme.SurfacePanel;
				control->ForeColor = theme.Text;
				control->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.58f : 0.44f);
				panel->Boder = control->Type() == UIClass::UI_TabPage ? 0.0f : 1.0f;
				panel->CornerRadius = 8.0f;
				panel->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_GroupBox:
			{
				auto* group = (GroupBox*)control;
				group->BackColor = theme.SurfacePanel;
				group->ForeColor = theme.Text;
				group->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.58f : 0.44f);
				group->CaptionBackColor = theme.SurfacePanelSoft;
				group->CaptionBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.42f : 0.34f);
				group->CaptionCornerRadius = 7.0f;
				group->CornerRadius = 9.0f;
				group->Boder = 1.0f;
				group->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_Expander:
			{
				auto* expander = (Expander*)control;
				expander->BackColor = Color(0, 0, 0, 0);
				expander->ForeColor = theme.Text;
				expander->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.58f : 0.44f);
				expander->SurfaceColor = theme.SurfacePanel;
				expander->HeaderBackColor = theme.SurfacePanelSoft;
				expander->HeaderHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				expander->ContentBackColor = Color(theme.SurfacePanelSoft.r, theme.SurfacePanelSoft.g, theme.SurfacePanelSoft.b, 0.35f);
				expander->AccentColor = theme.Accent;
				expander->MutedTextColor = theme.TextMuted;
				expander->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				expander->CornerRadius = 8.0f;
				expander->Border = 1.0f;
				break;
			}
			case UIClass::UI_SplitContainer:
			{
				auto* split = (SplitContainer*)control;
				split->BackColor = theme.SurfacePanel;
				split->ForeColor = theme.Text;
				split->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.58f : 0.44f);
				split->SplitterColor = Color(theme.BorderStrong.r, theme.BorderStrong.g, theme.BorderStrong.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.30f : 0.24f);
				split->SplitterHotColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.22f);
				split->SplitterPressedColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.78f);
				split->SplitterCornerRadius = 3.0f;
				split->SplitterVisualInset = 8.0f;
				split->CornerRadius = 8.0f;
				split->Boder = 1.0f;
				split->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_ScrollView:
			{
				auto* scroll = (ScrollView*)control;
				scroll->BackColor = theme.SurfacePanel;
				scroll->BolderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.58f : 0.44f);
				scroll->ScrollBackColor = theme.ScrollTrack;
				scroll->ScrollForeColor = theme.ScrollThumb;
				scroll->CornerRadius = 8.0f;
				scroll->Boder = 1.0f;
				scroll->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_TreeView:
			{
				auto* tree = (TreeView*)control;
				tree->BackColor = theme.SurfacePanelSoft;
				tree->ForeColor = theme.Text;
				tree->BolderColor = theme.Border;
				tree->ScrollBackColor = theme.ScrollTrack;
				tree->ScrollForeColor = theme.ScrollThumb;
				tree->AccentColor = theme.Accent;
				tree->SelectedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				tree->UnderMouseItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.09f);
				tree->SelectedForeColor = theme.Text;
				break;
			}
			case UIClass::UI_ListView:
			case UIClass::UI_ListBox:
			{
				auto* list = (ListView*)control;
				list->BackColor = theme.SurfacePanelSoft;
				list->ForeColor = theme.Text;
				list->BolderColor = theme.Border;
				list->HeaderBackColor = theme.SurfaceAlt;
				list->HeaderForeColor = theme.Text;
				list->GridLineColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.42f);
				list->SelectedItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
				list->SelectedItemForeColor = theme.Text;
				list->UnderMouseItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				list->MutedTextColor = theme.TextMuted;
				list->AccentColor = theme.Accent;
				list->ScrollBackColor = theme.ScrollTrack;
				list->ScrollForeColor = theme.ScrollThumb;
				list->CheckBackColor = theme.InputBack;
				list->CheckBorderColor = theme.BorderStrong;
				break;
			}
			case UIClass::UI_PropertyGrid:
			{
				auto* pg = (PropertyGridView*)control;
				pg->BackColor = theme.SurfacePanelSoft;
				pg->ForeColor = theme.Text;
				pg->BolderColor = theme.Border;
				pg->HeaderBackColor = theme.SurfaceAlt;
				pg->HeaderForeColor = theme.Text;
				pg->CategoryBackColor = theme.SurfaceAlt;
				pg->CategoryForeColor = theme.Text;
				pg->GridLineColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.42f);
				pg->SelectedItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
				pg->UnderMouseItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				pg->ReadOnlyForeColor = theme.TextMuted;
				pg->AccentColor = theme.Accent;
				pg->EditBackColor = theme.InputBack;
				pg->EditForeColor = theme.Text;
				pg->CheckBackColor = theme.InputBack;
				pg->CheckBorderColor = theme.BorderStrong;
				pg->ScrollBackColor = theme.ScrollTrack;
				pg->ScrollForeColor = theme.ScrollThumb;
				break;
			}
			case UIClass::UI_GridView:
			{
				auto* grid = (GridView*)control;
				grid->BackColor = Color(0, 0, 0, 0);
				grid->ForeColor = theme.Text;
				grid->BolderColor = theme.Border;
				grid->HeadBackColor = theme.SurfaceAlt;
				grid->HeadForeColor = theme.Text;
				grid->HeadHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.08f);
				grid->GridLineColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.55f);
				grid->AccentColor = theme.Accent;
				grid->ButtonBackColor = theme.SurfaceAlt;
				grid->ButtonCheckedColor = theme.SurfacePanelSoft;
				grid->ButtonHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.10f);
				grid->ButtonPressedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
				grid->ButtonBorderDarkColor = theme.BorderStrong;
				grid->ButtonBorderLightColor = theme.Border;
				grid->SelectedItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.16f);
				grid->SelectedItemForeColor = theme.Text;
				grid->UnderMouseItemBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.09f);
				grid->UnderMouseItemForeColor = theme.Text;
				grid->ScrollBackColor = theme.ScrollTrack;
				grid->ScrollForeColor = theme.ScrollThumb;
				grid->EditBackColor = theme.InputBack;
				grid->EditForeColor = theme.Text;
				grid->EditSelectedBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.30f);
				grid->EditSelectedForeColor = theme.Text;
				grid->NewRowBackColor = theme.SurfaceAlt;
				grid->NewRowForeColor = theme.TextMuted;
				grid->NewRowIndicatorColor = theme.Accent;
				break;
			}
			case UIClass::UI_PagedGridView:
			{
				auto* paged = (PagedGridView*)control;
				paged->BackColor = Color(0, 0, 0, 0);
				paged->ForeColor = theme.Text;
				paged->PagerBackColor = theme.SurfacePanelSoft;
				paged->PagerBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.55f);
				paged->PagerButtonBackColor = theme.SurfaceAlt;
				paged->PagerButtonHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.11f);
				paged->PagerButtonCheckedColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.18f);
				paged->PagerTextColor = theme.TextMuted;
				paged->AccentColor = theme.Accent;
				paged->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				break;
			}
			case UIClass::UI_ChartView:
			{
				auto* chart = (ChartView*)control;
				chart->BackColor = theme.SurfacePanel;
				chart->ForeColor = theme.Text;
				chart->BolderColor = theme.Border;
				chart->PlotBackColor = theme.AccentSoft;
				chart->GridLineColor = theme.Border;
				chart->AxisColor = theme.BorderStrong;
				chart->AccentColor = theme.Accent;
				chart->LegendTextColor = theme.TextMuted;
				chart->TooltipBackColor = theme.SurfacePanelSoft;
				chart->TooltipBorderColor = theme.BorderStrong;
				chart->TooltipTextColor = theme.Text;
				chart->SelectedColor = Color(1.0f, 1.0f, 1.0f, 0.42f);
				chart->HoverColor = theme.AccentSoft;
				chart->ScrollBackColor = theme.Border;
				chart->ScrollForeColor = theme.BorderStrong;
				break;
			}
			case UIClass::UI_ReportView:
			{
				auto* report = (ReportView*)control;
				report->BackColor = theme.SurfacePanel;
				report->ForeColor = theme.Text;
				report->BolderColor = theme.Border;
				report->HeaderBackColor = theme.SurfaceAlt;
				report->HeaderForeColor = theme.Text;
				report->RowBackColor = Color(1.0f, 1.0f, 1.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.58f : 0.035f);
				report->AlternateRowBackColor = theme.SurfacePanelSoft;
				report->GroupBackColor = theme.AccentSoft;
				report->SummaryBackColor = Color(0.95f, 0.72f, 0.22f, 0.20f);
				report->SelectedRowBackColor = theme.Selection;
				report->UnderMouseRowBackColor = theme.AccentSoft;
				report->GridLineColor = theme.Border;
				report->MutedTextColor = theme.TextMuted;
				report->ScrollBackColor = theme.ScrollTrack;
				report->ScrollForeColor = theme.ScrollThumb;
				break;
			}
			case UIClass::UI_KpiCard:
			{
				auto* card = (KpiCard*)control;
				card->BackColor = Color(0, 0, 0, 0);
				card->SurfaceColor = theme.SurfacePanel;
				card->ForeColor = theme.Text;
				card->BolderColor = theme.Border;
				card->AccentColor = theme.Accent;
				card->ActiveBackColor = theme.AccentSoft;
				card->HoverColor = theme.SurfacePanelSoft;
				card->MutedTextColor = theme.TextMuted;
				card->SparklineFillColor = theme.AccentSoft;
				break;
			}
			case UIClass::UI_FilterBar:
			{
				auto* filter = (FilterBar*)control;
				filter->BackColor = Color(0, 0, 0, 0);
				filter->SurfaceColor = theme.SurfacePanel;
				filter->ForeColor = theme.Text;
				filter->BolderColor = theme.Border;
				filter->InputBackColor = theme.InputBack;
				filter->ChipBackColor = theme.SurfacePanelSoft;
				filter->ChipSelectedBackColor = theme.AccentSoft;
				filter->HoverColor = theme.SurfaceAlt;
				filter->AccentColor = theme.Accent;
				filter->MutedTextColor = theme.TextMuted;
				filter->ButtonBackColor = theme.Accent;
				filter->ButtonTextColor = theme.AccentText;
				break;
			}
			case UIClass::UI_Slider:
			{
				auto* slider = (Slider*)control;
				slider->TrackBackColor = theme.ScrollTrack;
				slider->TrackForeColor = theme.Accent;
				slider->TrackHoverColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.12f);
				slider->TrackBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.24f);
				slider->ThumbColor = theme.SurfacePanelSoft;
				slider->ThumbHoverColor = Color(0.98f, 0.99f, 1.0f, 1.0f);
				slider->ThumbBorderColor = theme.BorderStrong;
				slider->ThumbShadowColor = Color(0.0f, 0.0f, 0.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.18f : 0.30f);
				slider->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				slider->TrackHeight = 5.0f;
				slider->ThumbRadius = 8.0f;
				break;
			}
			case UIClass::UI_TabControl:
			{
				auto* tabs = (TabControl*)control;
				tabs->BackColor = Color(0, 0, 0, 0);
				tabs->ForeColor = theme.Text;
				tabs->BolderColor = theme.Border;
				tabs->TitleBackColor = Color(0, 0, 0, 0);
				tabs->SelectedTitleBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.13f);
				tabs->TitleHoverBackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, 0.08f);
				tabs->AccentColor = theme.Accent;
				tabs->TitleMutedForeColor = theme.TextMuted;
				break;
			}
			case UIClass::UI_LinkLabel:
			{
				auto* link = (LinkLabel*)control;
				link->ForeColor = theme.Accent;
				link->HoverColor = theme.Accent;
				link->VisitedColor = theme.TextMuted;
				link->UnderlineColor = theme.Accent;
				break;
			}
			case UIClass::UI_PictureBox:
			{
				auto* picture = (PictureBox*)control;
				picture->BackColor = theme.SurfacePanelSoft;
				picture->BolderColor = theme.Border;
				picture->CornerRadius = 8.0f;
				break;
			}
			case UIClass::UI_ProgressBar:
			{
				auto* progress = (ProgressBar*)control;
				control->BackColor = theme.ScrollTrack;
				control->ForeColor = theme.Accent;
				control->BolderColor = theme.Border;
				progress->TrackBorderColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.28f);
				progress->FillHighlightColor = Color(1.0f, 1.0f, 1.0f, this->GetThemeName() == DemoThemeKeyLight() ? 0.18f : 0.10f);
				progress->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				progress->CornerRadius = -1.0f;
				progress->InnerPadding = 2.0f;
				break;
			}
			case UIClass::UI_LoadingRing:
				control->ForeColor = theme.Accent;
				control->BackColor = Color(0, 0, 0, 0);
				break;
			case UIClass::UI_ProgressRing:
			{
				auto* ring = (ProgressRing*)control;
				ring->ForeColor = theme.Accent;
				ring->BackColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.13f : 0.18f);
				ring->ProgressGlowColor = Color(theme.Accent.r, theme.Accent.g, theme.Accent.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.10f : 0.18f);
				ring->CenterTextColor = theme.Text;
				ring->CenterBackColor = Color(theme.SurfacePanel.r, theme.SurfacePanel.g, theme.SurfacePanel.b, this->GetThemeName() == DemoThemeKeyLight() ? 0.56f : 0.18f);
				ring->DisabledOverlayColor = Color(theme.Surface.r, theme.Surface.g, theme.Surface.b, 0.48f);
				ring->RingThickness = -1.0f;
				ring->ShowCaps = true;
				break;
			}
			case UIClass::UI_WebBrowser:
			case UIClass::UI_MediaPlayer:
				control->BackColor = theme.SurfacePanelSoft;
				control->BolderColor = theme.Border;
				break;
			default:
				break;
			}

			for (int i = 0; i < control->Count; ++i)
			{
				self(self, control->operator[](i));
			}
		};

	for (size_t i = 0; i < this->Controls.size(); ++i)
	{
		applyControlTheme(applyControlTheme, this->Controls[i]);
	}

	if (_topStatus)
	{
		_topStatus->ForeColor = theme.TextMuted;
	}
	if (_themeLabel)
	{
		_themeLabel->ForeColor = theme.TextMuted;
	}
	if (_themeSelector)
	{
		_themeSelector->Text = this->GetThemeName() == DemoThemeKeyLight() ? DemoThemeLabelLight() : DemoThemeLabelDark();
	}
	if (_statusbar)
	{
		_statusbar->BackColor = theme.SurfacePanelSoft;
		_statusbar->ForeColor = theme.TextMuted;
		_statusbar->TopLineColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.38f);
		_statusbar->SeparatorColor = Color(theme.Border.r, theme.Border.g, theme.Border.b, 0.34f);
		_statusbar->PartBackColor = Color(0, 0, 0, 0);
		_statusbar->PartBorderColor = Color(0, 0, 0, 0);
		_statusbar->CornerRadius = 0.0f;
		_statusbar->UsePartPills = false;
	}
	if (_web)
	{
		std::wstring mode = this->GetThemeName() == DemoThemeKeyLight() ? L"light" : L"dark";
		std::wstring script = L"window.applyTheme && window.applyTheme(" + ToJsStringLiteral(mode) + L");";
		_web->ExecuteScriptAsync(script);
	}

	this->Invalidate(true);
}

DemoWindow::~DemoWindow()
{
	if (_notify)
	{
		_notify->HideNotifyIcon();
		delete _notify;
		_notify = nullptr;
	}
	if (_taskbar)
	{
		delete _taskbar;
		_taskbar = nullptr;
	}
}

void DemoWindow::Ui_UpdateStatus(const std::wstring& text)
{
	if (_topStatus)
	{
		_topStatus->Text = text;
		_topStatus->PostRender();
	}
	if (_statusbar)
	{
		_statusbar->SetPartText(0, text);
		_statusbar->PostRender();
	}
}

void DemoWindow::Ui_UpdateProgress(float value01)
{
	if (value01 < 0.0f) value01 = 0.0f;
	if (value01 > 1.0f) value01 = 1.0f;
	if (_progress)
	{
		_progress->PercentageValue = value01;
		_progress->PostRender();
	}
	if (_progressRing)
	{
		_progressRing->PercentageValue = value01;
		_progressRing->PostRender();
	}
	if (_taskbar)
	{
		_taskbar->SetValue((ULONGLONG)(value01 * 1000.0f), 1000);
	}
}

void DemoWindow::Menu_OnCommand(class Control* sender, int id)
{
	(void)sender;
	switch (id)
	{
	case 101:
		Ui_UpdateStatus(L"Menu: 文件 -> 打开");
		break;
	case 102:
		this->Close();
		break;
	case 201:
		Ui_UpdateStatus(L"Menu: 帮助 -> 关于");
		break;
	}
}

void DemoWindow::Basic_OnMouseWheel(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	Ui_UpdateStatus(StringHelper::Format(L"MouseWheel Delta=[%d]", e.Delta));
}

void DemoWindow::Basic_OnButtonClick(class Control* sender, MouseEventArgs e)
{
	(void)e;
	sender->Text = StringHelper::Format(L"点击计数[%d]", sender->Tag++);
	sender->PostRender();
	Ui_UpdateStatus(L"Button: OnMouseClick");
}

void DemoWindow::Basic_OnRadioChecked(class Control* sender)
{
	if (!_rb1 || !_rb2) return;
	if (sender == _rb1 && _rb1->Checked)
	{
		_rb2->Checked = false;
		_rb2->PostRender();
		Ui_UpdateStatus(L"Radio: 选中 A");
	}
	else if (sender == _rb2 && _rb2->Checked)
	{
		_rb1->Checked = false;
		_rb1->PostRender();
		Ui_UpdateStatus(L"Radio: 选中 B");
	}
}

void DemoWindow::Basic_OnIconButtonClick(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
	MessageBoxW(this->Handle, L"Icon Button Clicked", L"CUI", MB_OK);
}

void DemoWindow::Picture_OnOpenImage(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
	if (!_picture) return;

	OpenFileDialog ofd;
	ofd.Filter = MakeDialogFilterStrring("图片文件", "*.jpg;*.jpeg;*.png;*.bmp;*.svg;*.webp");
	ofd.SupportMultiDottedExtensions = true;
	ofd.Title = "选择一个图片文件";
	if (ofd.ShowDialog(this->Handle) != DialogResult::OK || ofd.SelectedPaths.empty())
		return;

	_picture->Image = nullptr;

	FileInfo file(ofd.SelectedPaths[0]);
	if (file.Extension() == ".svg" || file.Extension() == ".SVG")
	{
		auto svg = File::ReadAllText(file.FullName());
		_picture->SetImageEx(D2DGraphics::ToBitmapFromSvg(svg.c_str()));
	}
	else if (StringHelper::Contains(".jpg.jpeg.png.bmp.webp", StringHelper::ToLower(file.Extension())))
	{
		auto img = BitmapSource::FromFile(Convert::string_to_wstring(ofd.SelectedPaths[0]));
		_picture->SetImageEx(std::move(img));
	}

	Ui_UpdateStatus(L"PictureBox: 已加载图片");
	this->Invalidate();
}

void DemoWindow::Picture_OnDropFile(class Control* sender, std::vector<std::wstring> files)
{
	(void)sender;
	if (!_picture || files.empty()) return;

	_picture->Image = nullptr;

	FileInfo file(Convert::wstring_to_string(files[0]));
	if (file.Extension() == ".svg" || file.Extension() == ".SVG")
	{
		auto svg = File::ReadAllText(file.FullName());
		_picture->SetImageEx(D2DGraphics::ToBitmapFromSvg(svg.c_str()));
	}
	else if (StringHelper::Contains(".png.jpg.jpeg.bmp.webp", StringHelper::ToLower(file.Extension())))
	{
		auto img = BitmapSource::FromFile(files[0]);
		_picture->SetImageEx(std::move(img));
	}
	Ui_UpdateStatus(L"PictureBox: 拖拽载入");
	this->Invalidate();
}

void DemoWindow::Data_OnToggleEnable(class Control* sender, MouseEventArgs e)
{
	(void)e;
	if (!_grid && !_pagedGrid) return;
	auto sw = (Switch*)sender;
	if (_pagedGrid) _pagedGrid->Enable = sw->Checked;
	else _grid->Enable = sw->Checked;
	Ui_UpdateStatus(sw->Checked ? L"PagedGridView: Enable" : L"PagedGridView: Disable");
	this->Invalidate();
}

void DemoWindow::Data_OnToggleVisible(class Control* sender, MouseEventArgs e)
{
	(void)e;
	if (!_grid && !_pagedGrid) return;
	auto sw = (Switch*)sender;
	if (_pagedGrid) _pagedGrid->Visible = sw->Checked;
	else _grid->Visible = sw->Checked;
	Ui_UpdateStatus(sw->Checked ? L"PagedGridView: Visible" : L"PagedGridView: Hidden");
	this->Invalidate();
}

void DemoWindow::System_OnNotifyToggle(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
	if (!_notify) return;
	_notifyVisible = !_notifyVisible;
	if (_notifyVisible)
	{
		_notify->ShowNotifyIcon();
		Ui_UpdateStatus(L"NotifyIcon: Show");
	}
	else
	{
		_notify->HideNotifyIcon();
		Ui_UpdateStatus(L"NotifyIcon: Hide");
	}
}

void DemoWindow::System_OnBalloonTip(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
	if (!_notify) return;
	_notify->ShowBalloonTip("CUI", "NotifyIcon Balloon Tip", 3000, NIIF_INFO);
	Ui_UpdateStatus(L"NotifyIcon: BalloonTip");
}

void DemoWindow::System_OnContextMenuCommand(class Control* sender, int id)
{
	(void)sender;
	switch (id)
	{
	case 1001:
		Ui_UpdateStatus(L"ContextMenu: 新建项目");
		break;
	case 1002:
		Ui_UpdateStatus(L"ContextMenu: 刷新视图");
		break;
	case 1003:
		Ui_UpdateStatus(L"ContextMenu: 更多 -> 复制信息");
		break;
	case 1004:
		Ui_UpdateStatus(L"ContextMenu: 更多 -> 关于此页");
		break;
	}
}

void DemoWindow::BuildMenuToolStatus()
{
	_menu = this->AddControl(new Menu(0, 0, this->Size.cx, 28));
	_menu->BarBackColor = D2D1_COLOR_F{ 1,1,1,0.08f };
	_menu->DropBackColor = D2D1_COLOR_F{ 0.12f,0.12f,0.12f,0.92f };
	_menu->OnMenuCommand += [this](class Control* sender, int id) { this->Menu_OnCommand(sender, id); };
	{
		auto file = _menu->AddItem(L"文件");
		file->AddSubItem(L"打开", 101);
		file->AddSeparator();
		file->AddSubItem(L"退出", 102);

		auto help = _menu->AddItem(L"帮助");
		help->AddSubItem(L"关于", 201);
	}

	_toolbar = this->AddControl(new ToolBar(0, 0, this->Size.cx, 40));
	auto tbBasic = _toolbar->AddTextButton(L"基础", 64);
	auto tbData = _toolbar->AddTextButton(L"数据", 64);
	auto tbSystem = _toolbar->AddTextButton(L"系统", 64);

	tbBasic->OnMouseClick += [this](class Control* s, MouseEventArgs e) { (void)s; (void)e; if (_tabs) _tabs->SelectedIndex = 0; Ui_UpdateStatus(L"ToolBar/TextButton: 基础"); };
	tbData->OnMouseClick += [this](class Control* s, MouseEventArgs e) { (void)s; (void)e; if (_tabs) _tabs->SelectedIndex = 2; Ui_UpdateStatus(L"ToolBar/TextButton: 数据"); };
	tbSystem->OnMouseClick += [this](class Control* s, MouseEventArgs e) { (void)s; (void)e; if (_tabs) _tabs->SelectedIndex = 5; Ui_UpdateStatus(L"ToolBar/TextButton: 系统"); };

	_toolbar->AddSeparator();

	for (int i = 0; i < 3; ++i)
	{
		auto iconBtn = _toolbar->AddIconButton(_icons[i], 30);
		iconBtn->Tag = i;
		iconBtn->OnMouseClick += [this](class Control* sender, MouseEventArgs e)
			{
				(void)e;
				Ui_UpdateStatus(StringHelper::Format(L"ToolBar/IconButton: icon=%d", (int)sender->Tag + 1));
			};
	}

	_toolbar->AddSeparator();
	auto label = new Label(L"页面", 0, 0);
	label->ForeColor = Colors::LightGray;
	_toolbar->AddToolItem(label);

	auto pageCombo = _toolbar->AddToolComboBox(L"基础控件", 130);
	pageCombo->ExpandCount = 8;
	pageCombo->Items.push_back(L"基础控件");
	pageCombo->Items.push_back(L"容器与图像");
	pageCombo->Items.push_back(L"数据控件");
	pageCombo->Items.push_back(L"数据可视化");
	pageCombo->Items.push_back(L"布局容器");
	pageCombo->Items.push_back(L"系统集成");
	pageCombo->Items.push_back(L"WebBrowser");
	pageCombo->Items.push_back(L"MediaPlayer");
	pageCombo->OnSelectionChanged += [this, pageCombo](class Control* sender)
		{
			(void)sender;
			if (_tabs && pageCombo->SelectedIndex >= 0 && pageCombo->SelectedIndex < _tabs->Count)
			{
				_tabs->SelectedIndex = pageCombo->SelectedIndex;
				_tabs->PostRender();
			}
			Ui_UpdateStatus(L"ToolBar/ComboBox: " + pageCombo->Text);
		};

	_toolbar->AddSpacer(4);
	auto compactCheck = _toolbar->AddToolCheckBox(L"紧凑");
	compactCheck->OnChecked += [this](class Control* sender)
		{
			auto* check = (CheckBox*)sender;
			if (_toolbar)
			{
				_toolbar->Gap = check->Checked ? 3 : 6;
				_toolbar->Padding = check->Checked ? 4 : 6;
				_toolbar->LayoutItems();
				_toolbar->PostRender();
			}
			Ui_UpdateStatus(check->Checked ? L"ToolBar/CheckBox: 紧凑布局" : L"ToolBar/CheckBox: 标准布局");
		};

	_statusbar = this->AddControl(new StatusBar(0, 0, this->Size.cx, 26));
	_statusbar->AddPart(L"Ready", -1);
	_statusbar->AddPart(L"CUI", 120);
}

void DemoWindow::BuildTabs()
{
	int top = (_menu ? _menu->Height : 0) + (_toolbar ? _toolbar->Height : 0);

	_topSlider = this->AddControl(new Slider(10, top + 6, 320, 30));
	_topSlider->Min = 0;
	_topSlider->Max = 1000;
	_topSlider->Value = 250;
	_topSlider->OnValueChanged += [this](class Control* sender, float oldValue, float newValue)
		{
			(void)sender;
			(void)oldValue;
			Ui_UpdateProgress(newValue / 1000.0f);
			Ui_UpdateStatus(StringHelper::Format(L"Slider Value=%.0f", newValue));
		};

	_topStatus = this->AddControl(new Label(L"CUI 全组件演示（CUITest）", 350, top + 10));
	_topStatus->ForeColor = Colors::LightGray;
	_topStatus->OnMouseWheel += [this](class Control* sender, MouseEventArgs e) { this->Basic_OnMouseWheel(sender, e); };

	_themeLabel = this->AddControl(new Label(L"主题", 0, top + 10));
	_themeLabel->Width = 32;
	_themeLabel->Margin = Thickness(0, (float)(top + 10), 154, 0);
	_themeLabel->AnchorStyles = AnchorStyles::Top | AnchorStyles::Right;

	_themeSelector = this->AddControl(new ComboBox(DemoThemeLabelDark(), 0, top + 6, 110, 28));
	_themeSelector->Items.push_back(DemoThemeLabelLight());
	_themeSelector->Items.push_back(DemoThemeLabelDark());
	_themeSelector->Margin = Thickness(0, (float)(top + 6), 10, 0);
	_themeSelector->AnchorStyles = AnchorStyles::Top | AnchorStyles::Right;
	_themeSelector->OnSelectionChanged += [this](class Control* sender)
		{
			this->Theme_OnSelectionChanged(sender);
		};

	const int tabsTop = static_cast<int>(_topSlider->Bottom + 8);
	_tabs = this->AddControl(new TabControl(10, tabsTop, this->Size.cx - 20, this->Size.cy - tabsTop - 10));
	_tabs->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.0f };
	_tabs->Margin = Thickness(10, static_cast<float>(tabsTop), 10, 10);
	_tabs->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	_tabs->AnimationMode = TabControlAnimationMode::SlideHorizontal;
	_tabs->TitlePosition = TabControlTitlePosition::Top;
	auto pBasic = _tabs->AddPage(L"基础控件");
	auto pContainers = _tabs->AddPage(L"容器与图像");
	auto pData = _tabs->AddPage(L"数据控件");
	auto pAnalytics = _tabs->AddPage(L"数据可视化");
	auto pLayout = _tabs->AddPage(L"布局容器");
	auto pSystem = _tabs->AddPage(L"系统集成");
	auto pWeb = _tabs->AddPage(L"WebBrowser");
	auto pMedia = _tabs->AddPage(L"MediaPlayer");
	for (int i = 0; i < 32; ++i)
		_tabs->AddPage(L"占位页");
	BuildTab_Basic(pBasic);
	BuildTab_Containers(pContainers);
	BuildTab_Data(pData);
	BuildTab_Analytics(pAnalytics);
	BuildTab_Layout(pLayout);
	BuildTab_System(pSystem);
	BuildTab_Web(pWeb);
	BuildTab_Media(pMedia);
}

void DemoWindow::BuildTab_Basic(TabPage* page)
{
	page->AddControl(new Label(L"Button / Label / LinkLabel / TextBox / ComboBox / NumericUpDown / ColorPicker / CheckBox / RadioBox / Calendar / RichTextBox", 10, 10));
	page->AddControl(new CustomLabel1(L"CustomLabel1（渐变绘制）", 10, 38));

	_basicButton = page->AddControl(new Button(L"点击计数[0]", 10, 70, 160, 28));
	_basicButton->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Basic_OnButtonClick(sender, e); };

	_basicEnableCheck = page->AddControl(new CheckBox(L"启用输入框", 180, 74));
	_basicEnableCheck->Checked = true;

	_basicLink = page->AddControl(new LinkLabel(L"查看文档", 320, 74));
	_basicLink->OnMouseClick += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			if (_basicLink)
			{
				_basicLink->Visited = true;
				_basicLink->PostRender();
			}
			Ui_UpdateStatus(L"LinkLabel: OnMouseClick");
		};

	auto tb1 = page->AddControl(new TextBox(L"TextBox", 10, 110, 200, 26));
	auto tb2 = page->AddControl(new CustomTextBox1(L"CustomTextBox1", 10, 145, 200, 26));
	auto tb3 = page->AddControl(new RoundTextBox(L"RoundTextBox", 10, 180, 200, 26));
	auto pwd = page->AddControl(new PasswordBox(L"pwd", 10, 215, 200, 26));

	_basicEnableCheck->OnChecked += [tb1, tb2, tb3, pwd](class Control* sender)
		{
			bool en = ((CheckBox*)sender)->Checked;
			tb1->Enable = en;
			tb2->Enable = en;
			tb3->Enable = en;
			pwd->Enable = en;
			tb1->PostRender();
			tb2->PostRender();
			tb3->PostRender();
			pwd->PostRender();
		};

	auto combo = page->AddControl(new ComboBox(L"item0", 240, 110, 180, 28));
	combo->ExpandCount = 8;
	for (int i = 0; i < 30; i++) combo->Items.push_back(StringHelper::Format(L"item%d", i));
	combo->OnSelectionChanged += [this, combo](class Control* sender)
		{
			(void)sender;
			Ui_UpdateStatus(StringHelper::Format(L"ComboBox: %ws", combo->Text.c_str()));
		};

	page->AddControl(new Label(L"DateTimePicker", 450, 110));
	auto dtBoth = page->AddControl(new DateTimePicker(L"", 450, 140, 200, 28));
	dtBoth->AllowModeSwitch = true;
	dtBoth->OnSelectionChanged += [this, dtBoth](class Control* sender)
		{
			(void)sender;
			Ui_UpdateStatus(StringHelper::Format(L"DateTimePicker: %s", dtBoth->Text.c_str()));
		};
	auto dtDate = page->AddControl(new DateTimePicker(L"", 450, 175, 200, 28));
	dtDate->Mode = DateTimePickerMode::DateOnly;
	dtDate->AllowModeSwitch = false;
	auto dtTime = page->AddControl(new DateTimePicker(L"", 450, 210, 200, 28));
	dtTime->Mode = DateTimePickerMode::TimeOnly;
	dtTime->AllowModeSwitch = false;

	page->AddControl(new Label(L"NumericUpDown", 240, 220));
	auto numeric = page->AddControl(new NumericUpDown(360, 214, 80, 28));
	numeric->Min = 0;
	numeric->Max = 100;
	numeric->Step = 5;
	numeric->Value = 25;
	numeric->OnValueChanged += [this](class NumericUpDown* sender, double oldValue, double newValue)
		{
			(void)sender;
			(void)oldValue;
			Ui_UpdateStatus(StringHelper::Format(L"NumericUpDown: %.0f", newValue));
		};

	page->AddControl(new Label(L"DateRangePicker", 740, 150));
	auto rangePicker = page->AddControl(new DateRangePicker(L"选择日期范围", 740, 178, 210, 28));
	rangePicker->OnRangeChanged += [this](class DateRangePicker* sender)
		{
			auto range = sender->GetRange();
			if (range.HasStart && range.HasEnd)
				Ui_UpdateStatus(L"DateRangePicker: " + sender->Text);
			else if (range.HasStart)
				Ui_UpdateStatus(L"DateRangePicker: start = " + sender->Text);
			else
				Ui_UpdateStatus(L"DateRangePicker: cleared");
		};

	page->AddControl(new Label(L"ColorPicker", 740, 230));
	auto colorPicker = page->AddControl(new ColorPicker(740, 258, 210, 28));
	colorPicker->SelectedColor = D2D1_COLOR_F{ 0.28f, 0.63f, 0.98f, 0.98f };
	colorPicker->OnColorChanged += [this](class ColorPicker* sender, D2D1_COLOR_F oldColor, D2D1_COLOR_F newColor, std::wstring value)
		{
			(void)sender;
			(void)oldColor;
			(void)newColor;
			Ui_UpdateStatus(L"ColorPicker: " + value);
		};

	page->AddControl(new Label(L"CalendarView", 980, 70));
	auto calendar = page->AddControl(new CalendarView(980, 98, 320, 250));
	calendar->SelectionMode = CalendarSelectionMode::Range;
	calendar->OnSelectionChanged += [this](class CalendarView* sender)
		{
			auto range = sender->GetRange();
			if (range.HasStart && range.HasEnd)
				Ui_UpdateStatus(StringHelper::Format(L"CalendarView: %04d-%02d-%02d ~ %04d-%02d-%02d",
					range.Start.wYear, range.Start.wMonth, range.Start.wDay,
					range.End.wYear, range.End.wMonth, range.End.wDay));
			else if (range.HasStart)
				Ui_UpdateStatus(StringHelper::Format(L"CalendarView: start %04d-%02d-%02d",
					range.Start.wYear, range.Start.wMonth, range.Start.wDay));
		};

	_rb1 = page->AddControl(new RadioBox(L"选项 A", 240, 150));
	_rb2 = page->AddControl(new RadioBox(L"选项 B", 240, 180));
	_rb1->Checked = true;
	_rb1->OnChecked += [this](class Control* sender) { this->Basic_OnRadioChecked(sender); };
	_rb2->OnChecked += [this](class Control* sender) { this->Basic_OnRadioChecked(sender); };

	auto rich = page->AddControl(new RichTextBox(L"RichTextBox: 支持拖拽文本到此处\r\n", 10, 260, 700, 220));
	rich->AllowMultiLine = true;
	rich->ScrollToEnd();
	rich->OnDropText += [](class Control* sender, std::wstring text)
		{
			RichTextBox* rtb = (RichTextBox*)sender;
			rtb->AppendText(text);
			rtb->ScrollToEnd();
			rtb->PostRender();
		};

	page->AddControl(new Label(L"Icon Buttons:", 740, 70));
	for (int i = 0; i < 5; i++)
	{
		Button* iconBtn = page->AddControl(new Button(L"", 740 + (44 * i), 95, 40, 40));
		iconBtn->Image = _icons[i];
		iconBtn->SizeMode = ImageSizeMode::CenterImage;
		iconBtn->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
		iconBtn->Boder = 2.0f;
		iconBtn->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Basic_OnIconButtonClick(sender, e); };
	}
}

void DemoWindow::BuildTab_Containers(TabPage* page)
{
	page->AddControl(new Label(L"Panel / PictureBox / ProgressBar / LoadingRing / ProgressRing / Switch / Navigation（拖拽文件到图片框）", 10, 10));

	auto openBtn = page->AddControl(new Button(L"打开图片", 10, 40, 120, 28));
	openBtn->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Picture_OnOpenImage(sender, e); };

	auto panel = page->AddControl(new Panel(10, 78, 520, 320));
	panel->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	panel->BolderColor = D2D1_COLOR_F{ 1,1,1,0.12f };

	panel->AddControl(new Label(L"PictureBox", 10, 10));
	_picture = panel->AddControl(new PictureBox(110, 10, 390, 210));
	_picture->SizeMode = ImageSizeMode::StretchIamge;
	_picture->OnDropFile += [this](class Control* sender, std::vector<std::wstring> files) { this->Picture_OnDropFile(sender, files); };

	panel->AddControl(new Label(L"ProgressBar", 10, 235));
	_progress = panel->AddControl(new ProgressBar(110, 230, 390, 24));
	_progress->PercentageValue = 0.25f;

	const int panelRight = static_cast<int>(panel->Right);
	const int panelTop = static_cast<int>(panel->Top);
	page->AddControl(new Label(L"LoadingRing", panelRight + 20, panelTop + 100));
	_loadingRing = page->AddControl(new LoadingRing(panelRight + 44, panelTop + 130, 56, 56));

	page->AddControl(new Label(L"ProgressRing", panelRight + 118, panelTop + 100));
	_progressRing = page->AddControl(new ProgressRing(panelRight + 136, panelTop + 124, 92, 92));
	_progressRing->PercentageValue = 0.25f;

	auto swEnable = page->AddControl(new Switch(panelRight + 20, panelTop + 10));
	page->AddControl(new Label(L"Enable Panel", static_cast<int>(swEnable->Right + 8), static_cast<int>(swEnable->Top)));
	swEnable->Checked = true;
	swEnable->OnMouseClick += [panel, this](class Control* sender, MouseEventArgs e)
		{
			(void)e;
			panel->Enable = ((Switch*)sender)->Checked;
			Ui_UpdateStatus(panel->Enable ? L"Panel: Enable" : L"Panel: Disable");
			this->Invalidate();
		};

	auto swVisible = page->AddControl(new Switch(panelRight + 20, panelTop + 50));
	page->AddControl(new Label(L"Visible PictureBox", static_cast<int>(swVisible->Right + 8), static_cast<int>(swVisible->Top)));
	swVisible->Checked = true;
	swVisible->OnMouseClick += [this](class Control* sender, MouseEventArgs e)
		{
			(void)e;
			if (!_picture) return;
			_picture->Visible = ((Switch*)sender)->Checked;
			Ui_UpdateStatus(_picture->Visible ? L"PictureBox: Visible" : L"PictureBox: Hidden");
			this->Invalidate();
		};

	auto split = page->AddControl(new SplitContainer(900, 78, 420, 260));
	split->SplitterDistance = 150;
	split->SplitterWidth = 8;
	split->BackColor = D2D1_COLOR_F{ 1,1,1,0.03f };
	split->BolderColor = D2D1_COLOR_F{ 1,1,1,0.14f };
	split->FirstPanel()->BackColor = D2D1_COLOR_F{ 1,1,1,0.05f };
	split->SecondPanel()->BackColor = D2D1_COLOR_F{ 1,1,1,0.05f };
	split->FirstPanel()->AddControl(new Label(L"左侧面板", 12, 12));
	auto sideBar = split->FirstPanel()->AddControl(new SideBar(10, 38, 128, 188));
	sideBar->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	sideBar->Margin = Thickness(10, 38, 12, 36);
	sideBar->AddHeader(L"工作区");
	sideBar->AddItem(L"概览", L"overview", _icons[0]);
	sideBar->Items.back().BadgeText = L"3";
	sideBar->AddItem(L"资源", L"assets", _icons[1]);
	sideBar->AddSeparator();
	sideBar->AddItem(L"设置", L"settings", _icons[2]);
	sideBar->SelectItem(1);
	sideBar->OnItemClick += [this](class NavigationView* sender, int index)
		{
			if (index >= 0 && index < (int)sender->Items.size())
				Ui_UpdateStatus(L"SideBar: " + sender->Items[index].Text);
		};
	split->SecondPanel()->AddControl(new Label(L"右侧内容区", 12, 12));
	auto breadcrumb = split->SecondPanel()->AddControl(new BreadcrumbBar(12, 38, 240, 30));
	breadcrumb->AddItem(L"应用");
	breadcrumb->AddItem(L"资源");
	breadcrumb->AddItem(L"详情");
	breadcrumb->SelectItem(2);
	breadcrumb->OnItemClick += [this](class BreadcrumbBar* sender, int index)
		{
			if (index >= 0 && index < (int)sender->Items.size())
				Ui_UpdateStatus(L"BreadcrumbBar: " + sender->Items[index].Text);
		};
	auto splitMemo = split->SecondPanel()->AddControl(new RichTextBox(L"拖拽中间分隔条，调整左右区域宽度。\r\n\r\nSplitContainer 很适合导航 + 内容、属性面板 + 画布等场景。", 12, 78, 230, 132));
	splitMemo->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	splitMemo->Margin = Thickness(12, 78, 20, 50);
	splitMemo->AllowMultiLine = true;

	auto group = page->AddControl(new GroupBox(L"GroupBox", 900, 342, 420, 150));
	group->BackColor = D2D1_COLOR_F{ 1,1,1,0.04f };
	group->BolderColor = D2D1_COLOR_F{ 1,1,1,0.20f };
	group->AddControl(new Label(L"把相关输入项包成一个逻辑区块。", 16, 20));
	auto groupName = group->AddControl(new TextBox(L"名称", 16, 52, 180, 26));
	auto groupEnabled = group->AddControl(new CheckBox(L"启用高级选项", 16, 88));
	groupEnabled->Checked = true;
	groupEnabled->OnChecked += [groupName](class Control* sender)
		{
			groupName->Enable = ((CheckBox*)sender)->Checked;
			groupName->PostRender();
		};

	auto expander = page->AddControl(new Expander(L"Expander", 540, 342, 330, 150));
	expander->Padding = Thickness(12);
	expander->AddControl(new Label(L"标题栏点击展开 / 折叠，内容区带裁剪动画。", 0, 0));
	auto expNumber = expander->AddControl(new NumericUpDown(0, 36, 130, 28));
	expNumber->Min = 1;
	expNumber->Max = 20;
	expNumber->Value = 6;
	expNumber->OnValueChanged += [this](class NumericUpDown*, double, double value)
		{
			Ui_UpdateStatus(StringHelper::Format(L"Expander Numeric: %.0f", value));
		};
	auto expCheck = expander->AddControl(new CheckBox(L"启用折叠区设置", 150, 40));
	expCheck->Checked = true;
	expander->OnExpandedChanged += [this](class Expander*, bool expanded)
		{
			Ui_UpdateStatus(expanded ? L"Expander: Expanded" : L"Expander: Collapsed");
		};

	page->AddControl(new Label(L"提示：顶部 Slider 同时驱动 ProgressBar、ProgressRing 与 Taskbar 进度。", 10, 510));
}

void DemoWindow::BuildTab_Data(TabPage* page)
{
	page->AddControl(new Label(L"TreeView / ListView / ListBox / PagedGridView(GridView) / PropertyGrid / Switch", 10, 10));

	TreeView* tree = page->AddControl(new TreeView(10, 40, 175, 180));
	tree->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top;
	tree->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	tree->Margin = Thickness(10, 40, 0, 10);
	for (int i = 0; i < 4; i++)
	{
		auto sub = new TreeNode(StringHelper::Format(L"node%d", i), _bmps[i % 10]);
		sub->Expand = true;
		tree->Root->Children.push_back(sub);
		for (int j = 0; j < 6; j++)
		{
			auto ssub = new TreeNode(StringHelper::Format(L"node%d-%d", i, j), _bmps[(i + j) % 10]);
			sub->Children.push_back(ssub);
		}
	}

	auto listBox = page->AddControl(new ListBox(195, 40, 175, 180));
	listBox->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top;
	listBox->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	listBox->AlternatingRows = true;
	listBox->AddItem(ListViewItem(L"全部任务"));
	listBox->AddItem(ListViewItem(L"今天"));
	listBox->AddItem(ListViewItem(L"进行中"));
	listBox->AddItem(ListViewItem(L"已完成"));
	listBox->SelectItem(0);
	listBox->OnItemClick += [this](class ListView* sender, int index)
		{
			if (index >= 0 && index < (int)sender->Items.size())
				Ui_UpdateStatus(L"ListBox: " + sender->Items[index].Text);
		};

	auto list = page->AddControl(new ListView(10, 235, 360, 225));
	list->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Bottom;
	list->Margin = Thickness(10, 235, 0, 10);
	list->ViewMode = ListViewViewMode::Details;
	list->SelectionMode = ListViewSelectionMode::Multiple;
	list->ShowCheckBoxes = true;
	list->AlternatingRows = true;
	list->AddColumn(ListViewColumn(L"Name", 150));
	list->AddColumn(ListViewColumn(L"State", 110));
	for (int i = 0; i < 40; i++)
	{
		ListViewItem item(StringHelper::Format(L"List item %02d", i + 1), i % 3 == 0 ? L"Ready" : L"Queued");
		item.Image = _bmps[i % 10];
		item.Checked = (i % 5 == 0);
		item.SubItems.push_back(item.SubText);
		list->AddItem(item);
	}
	list->OnItemClick += [this](class ListView* sender, int index)
		{
			if (index >= 0 && index < (int)sender->Items.size())
				Ui_UpdateStatus(L"ListView: " + sender->Items[index].Text);
		};

	_pagedGrid = page->AddControl(new PagedGridView(390, 70, 650, 390));
	_pagedGrid->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Bottom;
	_pagedGrid->Margin = Thickness(390, 70, 0, 10);
	_pagedGrid->PageSize = 45;
	_pagedGrid->OnPageChanged += [this](class PagedGridView* sender, int oldPage, int newPage)
		{
			(void)sender;
			(void)oldPage;
			Ui_UpdateStatus(StringHelper::Format(L"PagedGridView: page %d", newPage + 1));
		};
	_grid = _pagedGrid->Grid;
	_grid->AllowUserToAddRows = false;
	_grid->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	_grid->HeadFont = new Font(L"Arial", 16);
	_grid->Font = new Font(L"Arial", 16);

	_pagedGrid->AddColumn(GridViewColumn(L"Image", 80, ColumnType::Image));
	GridViewColumn comColumn = GridViewColumn(L"ComboBox", 120, ColumnType::ComboBox);
	comColumn.ComboBoxItems = { L"Item 1", L"Item 2", L"Item 3" };
	_pagedGrid->AddColumn(comColumn);
	_pagedGrid->AddColumn(GridViewColumn(L"Check", 80, ColumnType::Check));
	GridViewColumn textColumn = GridViewColumn(L"Text", 160, ColumnType::Text, true);
	_pagedGrid->AddColumn(textColumn);
	_pagedGrid->AddColumn(GridViewColumn(L"Linked", 120, ColumnType::LinkedText));
	GridViewColumn buttonColumn = GridViewColumn(L"Button", 80, ColumnType::Button);
	buttonColumn.ButtonText = L"OK";
	_pagedGrid->AddColumn(buttonColumn);
	_grid->OnGridViewLinkedTextClick += [this](class GridView* sender, int c, int r, std::wstring text)
		{
			(void)sender;
			Ui_UpdateStatus(StringHelper::Format(L"GridView: linked text [%d,%d] %s", c, r, text.c_str()));
		};
	for (int i = 0; i < 500; i++)
	{
		GridViewRow row;
		row.Cells = { _bmps[i % 10], L"Item 1", i % 2 == 0, std::to_wstring(Random::Next()), L"Open", L"" };
		_pagedGrid->AddRow(row);
	}
	_pagedGrid->RefreshPage();

	_gridEnableSwitch = page->AddControl(new Switch(390, 40));
	_gridEnableSwitch->Checked = true;
	_gridEnableSwitch->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Data_OnToggleEnable(sender, e); };
	page->AddControl(new Label(L"Enable", 460, 43));

	_gridVisibleSwitch = page->AddControl(new Switch(520, 40));
	_gridVisibleSwitch->Checked = true;
	_gridVisibleSwitch->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Data_OnToggleVisible(sender, e); };
	page->AddControl(new Label(L"Visible", 590, 43));

	auto props = page->AddControl(new PropertyGridView(1055, 70, 315, 390));
	props->AnchorStyles = AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	props->Margin = Thickness(0, 70, 10, 10);
	props->AddProperty(L"Appearance", L"Title", L"Grid settings", PropertyGridValueType::Text);
	props->AddProperty(L"Appearance", L"Enabled", L"True", PropertyGridValueType::Bool);
	PropertyGridItem density(L"Behavior", L"Density", L"Comfortable", PropertyGridValueType::Enum);
	density.Options = { L"Compact", L"Comfortable", L"Roomy" };
	props->AddItem(density);
	props->AddProperty(L"Behavior", L"PageSize", L"50", PropertyGridValueType::Number);
	props->AddProperty(L"Theme", L"Accent", L"#2F7DF0", PropertyGridValueType::Color);
	props->OnValueChanged += [this](class PropertyGridView* sender, int index, std::wstring oldValue, std::wstring newValue)
		{
			(void)oldValue;
			if (index >= 0 && index < (int)sender->Items.size())
				Ui_UpdateStatus(L"PropertyGrid: " + sender->Items[index].Name + L" = " + newValue);
		};
}

void DemoWindow::BuildTab_Analytics(TabPage* page)
{
	page->AddControl(new Label(L"KpiCard / FilterBar / ChartView / ReportView（指标、筛选、图表、报表组合）", 10, 10));

	_analyticsFilter = page->AddControl(new FilterBar(10, 38, 1360, 48));
	_analyticsFilter->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right;
	_analyticsFilter->Margin = Thickness(10, 38, 10, 0);
	_analyticsFilter->Placeholder = L"搜索客户、区域或阶段";
	_analyticsFilter->AddItem(FilterBarItem(L"已成交", L"done", true));
	_analyticsFilter->AddItem(FilterBarItem(L"合同中", L"contract"));
	_analyticsFilter->AddItem(FilterBarItem(L"跟进中", L"follow"));
	_analyticsFilter->AddItem(FilterBarItem(L"高毛利", L"margin"));
	_analyticsFilter->OnQueryChanged += [this](class FilterBar* sender, const std::wstring& query)
		{
			(void)sender;
			Ui_UpdateStatus(query.empty() ? L"FilterBar: query cleared" : L"FilterBar: " + query);
		};
	_analyticsFilter->OnFilterChanged += [this](class FilterBar* sender, int index, bool selected)
		{
			Ui_UpdateStatus(StringHelper::Format(L"FilterBar: %s %s",
				sender->Items[index].Text.c_str(),
				selected ? L"selected" : L"cleared"));
		};
	_analyticsFilter->OnApply += [this](class FilterBar* sender)
		{
			auto values = sender->GetSelectedValues();
			Ui_UpdateStatus(StringHelper::Format(L"FilterBar: apply %d filters", (int)values.size()));
		};
	_analyticsFilter->OnReset += [this](class FilterBar*)
		{
			Ui_UpdateStatus(L"FilterBar: reset");
		};

	_kpiRevenue = page->AddControl(new KpiCard(10, 96, 200, 96));
	_kpiRevenue->Title = L"成交额";
	_kpiRevenue->Value = L"1,870.5";
	_kpiRevenue->Unit = L"万";
	_kpiRevenue->TrendText = L"+18.4%";
	_kpiRevenue->TrendDirection = KpiTrendDirection::Up;
	_kpiRevenue->Caption = L"较上期";
	_kpiRevenue->SetSparkline({ 118,134,126,156,178,172,191,218 });

	_kpiDeals = page->AddControl(new KpiCard(222, 96, 200, 96));
	_kpiDeals->Title = L"成交客户";
	_kpiDeals->Value = L"128";
	_kpiDeals->TrendText = L"+9";
	_kpiDeals->TrendDirection = KpiTrendDirection::Up;
	_kpiDeals->Caption = L"本月新增";
	_kpiDeals->SetSparkline({ 56,64,72,79,88,96,113,128 });

	_kpiMargin = page->AddControl(new KpiCard(434, 96, 200, 96));
	_kpiMargin->Title = L"平均毛利率";
	_kpiMargin->Value = L"29.8";
	_kpiMargin->Unit = L"%";
	_kpiMargin->TrendText = L"-1.2%";
	_kpiMargin->TrendDirection = KpiTrendDirection::Down;
	_kpiMargin->Caption = L"需关注";
	_kpiMargin->SetSparkline({ 34,32,31,30,31,29,28,29.8 });

	auto bindKpi = [this](KpiCard* card)
		{
			card->OnCardClick += [this](class KpiCard* sender)
				{
					Ui_UpdateStatus(L"KpiCard: " + sender->Title);
				};
		};
	bindKpi(_kpiRevenue);
	bindKpi(_kpiDeals);
	bindKpi(_kpiMargin);

	auto barButton = page->AddControl(new Button(L"柱状图", 10, 202, 82, 28));
	auto pieButton = page->AddControl(new Button(L"饼形图", 100, 202, 82, 28));
	auto lineButton = page->AddControl(new Button(L"曲线图", 190, 202, 82, 28));

	_salesChart = page->AddControl(new ChartView(10, 238, 620, 232));
	_salesChart->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Bottom;
	_salesChart->Margin = Thickness(10, 238, 0, 10);
	_salesChart->Title = L"成交趋势";
	_salesChart->Subtitle = L"点击数据点查看明细；滚轮缩放，水平滚动条定位，双击复位";
	_salesChart->ValuePrecision = 1;
	_salesChart->ShowValueLabels = false;
	_salesChart->ShowLegend = true;

	std::vector<std::wstring> months = { L"1月", L"2月", L"3月", L"4月", L"5月", L"6月", L"7月", L"8月" };
	ChartSeries retail(L"零售", Color(0.17f, 0.49f, 0.96f, 0.95f));
	ChartSeries enterprise(L"企业", Color(0.10f, 0.68f, 0.55f, 0.95f));
	ChartSeries channel(L"渠道", Color(0.94f, 0.53f, 0.18f, 0.95f));
	double retailValues[] = { 118.0, 134.5, 126.2, 156.8, 178.4, 172.0, 191.3, 218.5 };
	double enterpriseValues[] = { 92.4, 108.0, 131.8, 139.0, 151.2, 169.5, 182.8, 197.0 };
	double channelValues[] = { 66.0, 72.5, 84.0, 90.4, 96.0, 104.3, 112.0, 128.6 };
	for (int i = 0; i < (int)months.size(); ++i)
	{
		retail.Points.push_back(ChartPoint(months[i], retailValues[i]));
		enterprise.Points.push_back(ChartPoint(months[i], enterpriseValues[i]));
		channel.Points.push_back(ChartPoint(months[i], channelValues[i]));
	}
	_salesChart->AddSeries(retail);
	_salesChart->AddSeries(enterprise);
	_salesChart->AddSeries(channel);
	_salesChart->OnPointClick += [this](class ChartView* sender, int seriesIndex, int pointIndex)
		{
			Ui_UpdateStatus(StringHelper::Format(L"ChartView: %s / %s = %.1f",
				sender->Series[seriesIndex].Name.c_str(),
				sender->Series[seriesIndex].Points[pointIndex].Label.c_str(),
				sender->Series[seriesIndex].Points[pointIndex].Value));
		};

	barButton->OnMouseClick += [this](class Control*, MouseEventArgs)
		{
			_salesChart->ChartKind = ChartViewKind::Bar;
			_salesChart->ResetView();
			Ui_UpdateStatus(L"ChartView: Bar");
		};
	pieButton->OnMouseClick += [this](class Control*, MouseEventArgs)
		{
			_salesChart->ChartKind = ChartViewKind::Pie;
			_salesChart->ResetView();
			Ui_UpdateStatus(L"ChartView: Pie");
		};
	lineButton->OnMouseClick += [this](class Control*, MouseEventArgs)
		{
			_salesChart->ChartKind = ChartViewKind::Line;
			_salesChart->ResetView();
			Ui_UpdateStatus(L"ChartView: Line");
		};

	_salesReport = page->AddControl(new ReportView(650, 96, 720, 374));
	_salesReport->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	_salesReport->Margin = Thickness(650, 96, 10, 10);
	_salesReport->Title = L"成交报表";
	_salesReport->Subtitle = L"点击表头排序，点击分组折叠/展开";
	_salesReport->FooterText = L"ReportView: sortable / grouped / scrollable";
	_salesReport->AddColumn(ReportColumn(L"客户", 150));
	_salesReport->AddColumn(ReportColumn(L"区域", 92));
	_salesReport->AddColumn(ReportColumn(L"阶段", 90));
	_salesReport->AddColumn(ReportColumn(L"成交额", 96, ReportCellAlign::Right));
	_salesReport->AddColumn(ReportColumn(L"毛利率", 82, ReportCellAlign::Right));
	_salesReport->AddGroup(L"华东区域");
	_salesReport->AddRow(ReportRow({ L"上海云舟", L"华东", L"已成交", L"312.4", L"31%" }));
	_salesReport->AddRow(ReportRow({ L"杭州数擎", L"华东", L"合同中", L"228.6", L"28%" }));
	_salesReport->AddRow(ReportRow({ L"苏州智造", L"华东", L"已成交", L"186.0", L"34%" }));
	_salesReport->AddSummary(L"华东小计", { L"华东小计", L"", L"", L"727.0", L"31%" });
	_salesReport->AddGroup(L"华南区域");
	_salesReport->AddRow(ReportRow({ L"深圳星河", L"华南", L"已成交", L"276.8", L"29%" }));
	_salesReport->AddRow(ReportRow({ L"广州远航", L"华南", L"跟进中", L"162.5", L"25%" }));
	_salesReport->AddRow(ReportRow({ L"厦门蓝海", L"华南", L"已成交", L"198.2", L"33%" }));
	_salesReport->AddSummary(L"华南小计", { L"华南小计", L"", L"", L"637.5", L"29%" });
	_salesReport->AddGroup(L"北方区域");
	_salesReport->AddRow(ReportRow({ L"北京乾元", L"北方", L"合同中", L"245.0", L"30%" }));
	_salesReport->AddRow(ReportRow({ L"天津港信", L"北方", L"已成交", L"141.6", L"27%" }));
	_salesReport->AddRow(ReportRow({ L"青岛新程", L"北方", L"跟进中", L"119.4", L"24%" }));
	_salesReport->AddSummary(L"北方小计", { L"北方小计", L"", L"", L"506.0", L"28%" });
	_salesReport->OnRowClick += [this](class ReportView* sender, int rowIndex)
		{
			if (rowIndex >= 0 && rowIndex < (int)sender->Rows.size() && !sender->Rows[rowIndex].Cells.empty())
				Ui_UpdateStatus(L"ReportView: " + sender->Rows[rowIndex].Cells[0]);
		};
	_salesReport->OnGroupToggled += [this](class ReportView* sender, int rowIndex, bool expanded)
		{
			(void)sender;
			Ui_UpdateStatus(StringHelper::Format(L"ReportView: group %d %s", rowIndex, expanded ? L"expanded" : L"collapsed"));
		};
}

void DemoWindow::BuildTab_Layout(TabPage* page)
{
	page->AddControl(new Label(L"StackPanel / GridPanel / DockPanel / WrapPanel / RelativePanel / ScrollView", 10, 10));

	auto stack = page->AddControl(new StackPanel(10, 40, 260, 220));
	stack->SetOrientation(Orientation::Vertical);
	stack->SetSpacing(6);
	stack->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	stack->AddControl(new Button(L"Stack A", 0, 0, 180, 26));
	stack->AddControl(new Button(L"Stack B", 0, 0, 200, 26));
	stack->AddControl(new Button(L"Stack C", 0, 0, 160, 26));

	auto grid = page->AddControl(new GridPanel(290, 40, 320, 220));
	grid->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	grid->AddRow(GridLength::Auto());
	grid->AddRow(GridLength::Star(1.0f));
	grid->AddRow(GridLength::Pixels(30));
	grid->AddColumn(GridLength::Star(1.0f));
	grid->AddColumn(GridLength::Star(1.0f));
	{
		auto title = new Label(L"GridPanel Title", 0, 0);
		title->GridRow = 0;
		title->GridColumn = 0;
		title->GridColumnSpan = 2;
		title->HAlign = HorizontalAlignment::Center;
		grid->AddControl(title);
		auto c1 = new Button(L"(0,1)", 0, 0, 80, 80);
		c1->GridRow = 1;
		c1->GridColumn = 0;
		c1->Margin = Thickness(6);
		grid->AddControl(c1);
		auto c2 = new Button(L"(1,1)", 0, 0, 80, 80);
		c2->GridRow = 1;
		c2->GridColumn = 1;
		c2->Margin = Thickness(6);
		grid->AddControl(c2);
		auto footer = new Label(L"Footer", 0, 0);
		footer->GridRow = 2;
		footer->GridColumn = 0;
		footer->GridColumnSpan = 2;
		footer->HAlign = HorizontalAlignment::Center;
		grid->AddControl(footer);
	}

	auto dock = page->AddControl(new DockPanel(630, 40, 320, 220));
	dock->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	dock->SetLastChildFill(true);
	{
		auto top = new Label(L"Top", 0, 0);
		top->DockPosition = Dock::Top;
		top->HAlign = HorizontalAlignment::Center;
		dock->AddControl(top);
		auto left = new Label(L"Left", 0, 0);
		left->DockPosition = Dock::Left;
		left->VAlign = VerticalAlignment::Center;
		dock->AddControl(left);
		auto bottom = new Label(L"Bottom", 0, 0);
		bottom->DockPosition = Dock::Bottom;
		bottom->HAlign = HorizontalAlignment::Center;
		dock->AddControl(bottom);
		auto fill = new Label(L"Fill", 0, 0);
		fill->DockPosition = Dock::Fill;
		dock->AddControl(fill);
	}

	auto wrap = page->AddControl(new WrapPanel(970, 40, 360, 220));
	wrap->SetOrientation(Orientation::Horizontal);
	wrap->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	for (int i = 1; i <= 10; i++)
	{
		wrap->AddControl(new Button(StringHelper::Format(L"Btn%d", i), 0, 0, 70, 26));
	}

	auto rp = page->AddControl(new RelativePanel(10, 280, 500, 250));
	rp->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	page->AddControl(new Label(L"RelativePanel（相对约束）", 10, 260));

	auto b = rp->AddControl(new Button(L"居中", 0, 0, 100, 26));

	RelativeConstraints cd;
	cd.CenterHorizontal = true;
	cd.CenterVertical = true;
	rp->SetConstraints(b, cd);

	page->AddControl(new Label(L"ScrollView（滚轮、拖动滚动条，内容超出时自动显示横纵滚动条）", 530, 260));
	auto scroll = page->AddControl(new ScrollView(530, 280, 800, 250));
	scroll->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	scroll->BolderColor = D2D1_COLOR_F{ 1,1,1,0.14f };
	scroll->Padding = Thickness(12);

	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			const int left = 16 + (col * 250);
			const int top = 16 + (row * 92);
			auto card = scroll->AddControl(new Panel(left, top, 220, 76));
			card->BackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.05f };
			card->BolderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.10f };
			card->AddControl(new Label(StringHelper::Format(L"Card %d", row * 4 + col + 1), 10, 10));
			auto btn = card->AddControl(new Button(L"点击", 10, 36, 70, 26));
			btn->Tag = row * 4 + col + 1;
			btn->OnMouseClick += [this](class Control* sender, MouseEventArgs e)
				{
					(void)e;
					Ui_UpdateStatus(StringHelper::Format(L"ScrollView Button: card=%d", (int)sender->Tag));
				};
			card->AddControl(new Label(StringHelper::Format(L"位置 %d,%d", col + 1, row + 1), 96, 40));
		}
	}

	scroll->AddControl(new Label(L"这一行专门把内容宽度拉长，便于测试横向滚动。", 16, 400));
	scroll->AddControl(new Button(L"Far Button", 860, 392, 120, 30));
	scroll->AddControl(new Label(L"最右侧区域", 1010, 398));
}

void DemoWindow::BuildTab_System(TabPage* page)
{
	page->AddControl(new Label(L"NotifyIcon / Taskbar / ContextMenu / MessageDialog / ToastHost", 10, 10));
	page->AddControl(new Label(L"Taskbar：顶部 Slider 会同步设置任务栏进度条（ITaskbarList3）", 10, 40));

	auto btnToggle = page->AddControl(new Button(L"显示/隐藏托盘图标", 10, 80, 180, 30));
	btnToggle->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->System_OnNotifyToggle(sender, e); };

	auto btnBalloon = page->AddControl(new Button(L"气泡提示", 200, 80, 120, 30));
	btnBalloon->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->System_OnBalloonTip(sender, e); };

	auto btnDialog = page->AddControl(new Button(L"CUI 对话框", 330, 80, 120, 30));
	btnDialog->OnMouseClick += [this](class Control*, MouseEventArgs)
		{
			auto result = MessageDialog::Show(L"CUI MessageDialog",
				L"这是一个 CUI 风格的模态对话框，按钮返回 MessageDialogResult。",
				MessageDialogButtons::OKCancel,
				MessageDialogIcon::Question,
				this->Handle);
			Ui_UpdateStatus(result == MessageDialogResult::OK ? L"MessageDialog: OK" : L"MessageDialog: Cancel/Close");
		};

	auto btnToast = page->AddControl(new Button(L"显示 Toast", 460, 80, 120, 30));
	btnToast->OnMouseClick += [this](class Control*, MouseEventArgs)
		{
			if (_toastHost)
				_toastHost->ShowToast(L"CUI Toast", L"这是由 ToastHost 管理的运行时通知。", ToastKind::Success);
		};

	page->AddControl(new Label(L"提示：右键托盘图标可弹出菜单；在此页空白区域单击右键可弹出自定义 ContextMenu。", 10, 125));
	page->AddControl(new Label(L"ContextMenu 不绑定具体控件，由业务代码主动调用 ShowAt。ToastHost 可作为页面级通知层。", 10, 150));

	_toastHost = page->AddControl(new ToastHost(900, 40, 430, 260));
	_toastHost->AnchorStyles = AnchorStyles::Top | AnchorStyles::Right;
	_toastHost->Margin = Thickness(0, 40, 40, 0);
	_toastHost->ShowToast(L"ToastHost", L"点击“显示 Toast”添加更多通知。", ToastKind::Info, 5200);
	_toastHost->OnToastClick += [this](class ToastHost*, int index)
		{
			Ui_UpdateStatus(StringHelper::Format(L"ToastHost: click #%d", index));
		};

	page->OnMouseUp += [this, page](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			if (e.Buttons != MouseButtons::Right) return;
			if (!_systemContextMenu) return;
			_systemContextMenu->ShowAt(page, e.X, e.Y);
			Ui_UpdateStatus(L"ContextMenu: 已弹出，点击其他区域会自动关闭");
		};
}

void DemoWindow::BuildTab_Web(TabPage* page)
{
	page->AddControl(new Label(L"WebBrowser（WebView2 Composition）", 320, 10));
	_web = page->AddControl(new WebBrowser(10, 40, 1200, 420));
	_web->Margin = Thickness(10, 40, 10, 10);
	_web->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;

	// JS -> C++: window.CUI.invoke(name, payload)
	_web->RegisterJsInvokeHandler(L"native.echo", [](const std::wstring& payload) {
		return L"echo: " + payload;
		});
	_web->RegisterJsInvokeHandler(L"native.time", [](const std::wstring& payload) {
		(void)payload;
		SYSTEMTIME st{};
		GetLocalTime(&st);
		return StringHelper::Format(L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		});

	std::wstring html =
		L"<!doctype html>"
		L"<html><head><meta charset='utf-8'/>"
		L"<style>"
		L":root{color-scheme:light dark;}"
		L"body{font-family:Segoe UI,Arial; padding:14px; background:#f5f7fb; color:#17202a; transition:background .18s ease,color .18s ease;}"
		L"body.dark{background:#1d2128; color:#edf1f6;}"
		L"button{padding:8px 12px; margin-right:8px; border:1px solid #b8c3d4; background:#ffffff; color:inherit; border-radius:8px;}"
		L"body.dark button{background:#2a3038; border-color:#5a6473;}"
		L".box{margin-top:10px; padding:10px; background:#e9eef5; border-radius:8px;}"
		L"body.dark .box{background:#2a3038;}"
		L"code{background:#dde5ef; padding:2px 4px; border-radius:4px;}"
		L"body.dark code{background:#39424d;}"
		L"</style></head><body>"
		L"<h3>CUI WebBrowser 互操作示例</h3>"
		L"<div>JS 调用 C++：<code>await window.CUI.invoke('native.time','')</code></div>"
		L"<div style='margin-top:8px;'>"
		L"  <button id='btnTime'>获取时间（JS->C++）</button>"
		L"  <button id='btnEcho'>回声（JS->C++）</button>"
		L"</div>"
		L"<div class='box'>输出：<span id='out'>(none)</span></div>"
		L"<div class='box'>C++ 调用 JS：<span id='fromNative'>(none)</span></div>"
		L"<script>"
		L"window.setFromNative=function(text){ document.getElementById('fromNative').textContent=String(text); return 'ok'; };"
		L"document.getElementById('btnTime').onclick=async function(){"
		L"  try{ const r = await window.CUI.invoke('native.time',''); document.getElementById('out').textContent=r; }"
		L"  catch(e){ document.getElementById('out').textContent='ERR: '+e; }"
		L"};"
		L"document.getElementById('btnEcho').onclick=async function(){"
		L"  try{ const r = await window.CUI.invoke('native.echo','hello from js'); document.getElementById('out').textContent=r; }"
		L"  catch(e){ document.getElementById('out').textContent='ERR: '+e; }"
		L"};"
		L"window.applyTheme=function(mode){ document.body.className = (mode==='dark') ? 'dark' : ''; return 'ok'; };"
		L"</script></body></html>";

	_web->SetHtml(html);

	// 原生按钮：演示 C++ -> JS（ExecuteScriptAsync）
	auto btn = page->AddControl(new Button(L"C++ 调用 JS（写入 fromNative）", 10, 10, 260, 24));
	btn->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top;
	btn->OnMouseClick += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			if (!_web) return;
			SYSTEMTIME st{};
			GetLocalTime(&st);
			std::wstring text = StringHelper::Format(L"from C++ at %02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
			std::wstring script = L"window.setFromNative(" + ToJsStringLiteral(text) + L");";
			_web->ExecuteScriptAsync(script);
		};
}

void DemoWindow::BuildTab_Media(TabPage* page)
{
	Label* titleLabel = page->AddControl(new Label(L"MediaPlayer（打开后立即播放；含进度条/拖动跳转/时间显示）", 10, 10));
	titleLabel->ForeColor = Colors::LightGray;

	_media = page->AddControl(new MediaPlayer(10, 40, 1200, 380));
	_media->Margin = Thickness(10, 40, 10, 140);
	_media->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	_media->AutoPlay = true;
	_media->Loop = false;
	MediaPlayer* mp = _media;

	Panel* controlPanel = page->AddControl(new Panel(10, 430, 1200, 110));
	controlPanel->Margin = Thickness(10, 0, 10, 10);
	controlPanel->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right | AnchorStyles::Bottom;
	controlPanel->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	controlPanel->BolderColor = D2D1_COLOR_F{ 1,1,1,0.12f };

	auto progressUpdating = std::make_shared<bool>(false);

	Button* btnOpen = controlPanel->AddControl(new Button(L"打开", 10, 10, 80, 30));
	btnOpen->OnMouseClick += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			if (!_media) return;
			OpenFileDialog ofd;
			ofd.Filter = MakeDialogFilterStrring("媒体文件", "*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.mp3;*.wav;*.flac;*.m4a;*.wma;*.aac");
			ofd.SupportMultiDottedExtensions = true;
			ofd.Title = "选择媒体文件";
			if (ofd.ShowDialog(this->Handle) == DialogResult::OK && !ofd.SelectedPaths.empty())
			{
				std::wstring file = Convert::string_to_wstring(ofd.SelectedPaths[0]);
				_media->Load(file);
				_media->Play();
				Ui_UpdateStatus(L"MediaPlayer: 已打开并播放 " + FileNameFromPath(file));
			}
		};

	Button* btnPlay = controlPanel->AddControl(new Button(L"播放", 100, 10, 70, 30));
	btnPlay->OnMouseClick += [mp](class Control* sender, MouseEventArgs e) { (void)sender; (void)e; mp->Play(); };
	Button* btnPause = controlPanel->AddControl(new Button(L"暂停", 180, 10, 70, 30));
	btnPause->OnMouseClick += [mp](class Control* sender, MouseEventArgs e) { (void)sender; (void)e; mp->Pause(); };
	Button* btnStop = controlPanel->AddControl(new Button(L"停止", 260, 10, 70, 30));
	btnStop->OnMouseClick += [mp](class Control* sender, MouseEventArgs e) { (void)sender; (void)e; mp->Stop(); };

	controlPanel->AddControl(new Label(L"音量", 340, 16));
	Slider* volume = controlPanel->AddControl(new Slider(390, 12, 140, 30));
	volume->Min = 0;
	volume->Max = 100;
	volume->Value = 80;
	volume->OnValueChanged += [mp](class Control* sender, float oldValue, float newValue)
		{
			(void)sender;
			(void)oldValue;
			mp->Volume = newValue / 100.0;
		};
	mp->Volume = 0.8;

	controlPanel->AddControl(new Label(L"速度", 540, 16));
	Slider* speed = controlPanel->AddControl(new Slider(590, 12, 140, 30));
	speed->Min = 10;
	speed->Max = 400;
	speed->Value = 100;
	Label* speedLabel = controlPanel->AddControl(new Label(L"1.00x", 735, 16));
	speedLabel->ForeColor = Colors::LightGray;
	speed->OnValueChanged += [mp, speedLabel](class Control* sender, float oldValue, float newValue)
		{
			(void)sender;
			(void)oldValue;
			mp->PlaybackRate = newValue / 100.0f;
			speedLabel->Text = StringHelper::Format(L"%.2fx", newValue / 100.0f);
			speedLabel->PostRender();
		};

	CheckBox* loop = controlPanel->AddControl(new CheckBox(L"循环", 790, 16));
	loop->OnChecked += [mp](class Control* sender) { mp->Loop = ((CheckBox*)sender)->Checked; };

	Label* progressLabel = controlPanel->AddControl(new Label(L"进度", 10, 84));
	progressLabel->ForeColor = Colors::LightGray;

	Slider* progressSlider = controlPanel->AddControl(new Slider(60, 58, 900, 30));
	progressSlider->Min = 0;
	progressSlider->Max = 1000;
	progressSlider->Value = 0;
	progressSlider->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right | AnchorStyles::Bottom;

	Label* timeLabel = controlPanel->AddControl(new Label(L"00:00 / 00:00", 970, 40));
	timeLabel->ForeColor = Colors::LightGray;
	timeLabel->AnchorStyles = AnchorStyles::Right | AnchorStyles::Top;
	timeLabel->Width = 200;

	progressSlider->OnValueChanged += [mp, progressUpdating](class Control* sender, float oldValue, float newValue)
		{
			(void)sender;
			(void)oldValue;
			if (*progressUpdating) return;
			if (mp->Duration > 0)
			{
				mp->Position = (newValue / 1000.0) * mp->Duration;
			}
		};

	_media->OnMediaOpened += [this, titleLabel, timeLabel, progressSlider, progressUpdating](class Control* sender)
		{
			MediaPlayer* player = (MediaPlayer*)sender;
			std::wstring fileName = FileNameFromPath(player->MediaFile);
			titleLabel->Text = StringHelper::Format(L"MediaPlayer - %s", fileName.c_str());
			titleLabel->PostRender();
			*progressUpdating = true;
			progressSlider->Value = 0;
			*progressUpdating = false;
			int total = (int)player->Duration;
			timeLabel->Text = StringHelper::Format(L"00:00 / %02d:%02d", total / 60, total % 60);
			timeLabel->PostRender();
			Ui_UpdateStatus(L"MediaPlayer: MediaOpened");
		};

	_media->OnMediaEnded += [timeLabel, this](class Control* sender)
		{
			(void)sender;
			timeLabel->Text = L"播放结束";
			timeLabel->PostRender();
			Ui_UpdateStatus(L"MediaPlayer: Ended");
		};

	_media->OnMediaFailed += [timeLabel, titleLabel, this](class Control* sender)
		{
			(void)sender;
			titleLabel->Text = L"MediaPlayer - 加载失败";
			titleLabel->PostRender();
			timeLabel->Text = L"加载失败";
			timeLabel->PostRender();
			Ui_UpdateStatus(L"MediaPlayer: Failed");
		};

	_media->OnPositionChanged += [timeLabel, progressSlider, progressUpdating](class Control* sender, double position)
		{
			MediaPlayer* player = (MediaPlayer*)sender;
			int cur = (int)position;
			int total = (int)player->Duration;
			if (total < 0) total = 0;
			timeLabel->Text = StringHelper::Format(L"%02d:%02d / %02d:%02d", cur / 60, cur % 60, total / 60, total % 60);
			timeLabel->PostRender();
			if (player->Duration > 0)
			{
				*progressUpdating = true;
				progressSlider->Value = (float)(position / player->Duration * 1000.0);
				*progressUpdating = false;
			}
		};
}

DemoWindow::DemoWindow() : Form(L"CUI Test Demo", { 0,0 }, { 1400,800 })
{
	_bmps[0] = D2DGraphics::ToBitmapFromSvg(_0_ico);
	_bmps[1] = D2DGraphics::ToBitmapFromSvg(_1_ico);
	_bmps[2] = D2DGraphics::ToBitmapFromSvg(_2_ico);
	_bmps[3] = D2DGraphics::ToBitmapFromSvg(_3_ico);
	_bmps[4] = D2DGraphics::ToBitmapFromSvg(_4_ico);
	_bmps[5] = D2DGraphics::ToBitmapFromSvg(_5_ico);
	_bmps[6] = D2DGraphics::ToBitmapFromSvg(_6_ico);
	_bmps[7] = D2DGraphics::ToBitmapFromSvg(_7_ico);
	_bmps[8] = D2DGraphics::ToBitmapFromSvg(_8_ico);
	_bmps[9] = D2DGraphics::ToBitmapFromSvg(_9_ico);
	_icons[0] = D2DGraphics::ToBitmapFromSvg(icon0);
	_icons[1] = D2DGraphics::ToBitmapFromSvg(icon1);
	_icons[2] = D2DGraphics::ToBitmapFromSvg(icon2);
	_icons[3] = D2DGraphics::ToBitmapFromSvg(icon3);
	_icons[4] = D2DGraphics::ToBitmapFromSvg(icon4);

	_taskbar = new Taskbar(this->Handle);
	_notify = new NotifyIcon();
	_notify->InitNotifyIcon(this->Handle, 1);
	_notify->SetIcon(LoadIcon(NULL, IDI_APPLICATION));
	_notify->SetToolTip("CUI Demo");
	_notify->ClearMenu();
	_notify->AddMenuItem(NotifyIconMenuItem("Show Window", 1));
	_notify->AddMenuSeparator();
	_notify->AddMenuItem(NotifyIconMenuItem("Exit", 3));
	_notify->OnNotifyIconMenuClick += [&](NotifyIcon* sender, int menuId)
		{
			switch (menuId)
			{
			case 1:
				ShowWindow(sender->hWnd, SW_SHOWNORMAL);
				break;
			case 3:
				PostMessage(sender->hWnd, WM_CLOSE, 0, 0);
				break;
			}
		};
	_notify->ShowNotifyIcon();
	_notifyVisible = true;

	this->OnThemeChanged += [this](class Form* sender, std::wstring oldTheme, std::wstring newTheme)
		{
			(void)sender;
			(void)oldTheme;
			(void)newTheme;
			this->Theme_ApplyCurrent();
		};

	BuildMenuToolStatus();
	BuildTabs();

	_basicToolTip = this->AddControl(new ToolTip());
	if (_basicToolTip && _basicButton)
	{
		_basicToolTip->Bind(_basicButton, L"这是绑定到 Button 的悬停提示示例");
	}

	_systemContextMenu = this->AddControl(new ContextMenu());
	if (_systemContextMenu)
	{
		_systemContextMenu->AddItem(L"新建项目", 1001);
		_systemContextMenu->AddItem(L"刷新视图", 1002);
		_systemContextMenu->AddSeparator();
		auto more = _systemContextMenu->AddItem(L"更多", 0);
		more->AddSubItem(L"复制信息", 1003);
		more->AddSubItem(L"关于此页", 1004);
		_systemContextMenu->OnMenuCommand += [this](class Control* sender, int id) { this->System_OnContextMenuCommand(sender, id); };
	}

	this->SizeMode = ImageSizeMode::StretchIamge;
	Theme_Apply(DemoThemeKeyDark());

}
