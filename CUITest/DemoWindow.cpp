#include "DemoWindow.h"

#include "imgs.h"
#include "../CUI/nanosvg.h"

#include <memory>

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
		textBox->BolderColor = theme.Border;
		textBox->UnderMouseColor = theme.InputHover;
		textBox->FocusedColor = theme.Accent;
		textBox->SelectedBackColor = theme.InputSelection;
		textBox->SelectedForeColor = theme.AccentText;
		textBox->ScrollBackColor = theme.ScrollTrack;
		textBox->ScrollForeColor = theme.ScrollThumb;
	}

	void ApplyPasswordTheme(PasswordBox* textBox, const DemoThemePalette& theme)
	{
		if (!textBox) return;
		textBox->BackColor = theme.InputBack;
		textBox->ForeColor = theme.Text;
		textBox->BolderColor = theme.Border;
		textBox->UnderMouseColor = theme.InputHover;
		textBox->FocusedColor = theme.Accent;
		textBox->SelectedBackColor = theme.InputSelection;
		textBox->SelectedForeColor = theme.AccentText;
		textBox->ScrollBackColor = theme.ScrollTrack;
		textBox->ScrollForeColor = theme.ScrollThumb;
	}

	void ApplyRichTextTheme(RichTextBox* textBox, const DemoThemePalette& theme)
	{
		if (!textBox) return;
		textBox->BackColor = theme.InputBack;
		textBox->ForeColor = theme.Text;
		textBox->BolderColor = theme.Border;
		textBox->UnderMouseColor = theme.InputHover;
		textBox->FocusedColor = theme.Accent;
		textBox->SelectedBackColor = theme.InputSelection;
		textBox->SelectedForeColor = theme.AccentText;
		textBox->ScrollBackColor = theme.ScrollTrack;
		textBox->ScrollForeColor = theme.ScrollThumb;
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

	std::shared_ptr<BitmapSource> ToBitmapFromSvg(const char* data)
	{
		if (!data) return {};
		int len = (int)strlen(data) + 1;
		char* svg_text = new char[len];
		memcpy(svg_text, data, len);
		NSVGimage* image = nsvgParse(svg_text, "px", 96.0f);
		delete[] svg_text;
		if (!image) return {};
		float percen = 1.0f;
		if (image->width > 4096 || image->height > 4096)
		{
			float maxv = image->width > image->height ? image->width : image->height;
			percen = 4096.0f / maxv;
		}
		auto renderSource = BitmapSource::CreateEmpty(image->width * percen, image->height * percen);
		auto subg = new D2DGraphics(renderSource.get());
		NSVGshape* shape;
		NSVGpath* path;
		subg->BeginRender();
		subg->Clear(D2D1::ColorF(0, 0, 0, 0));
		for (shape = image->shapes; shape != NULL; shape = shape->next)
		{
			auto geo = Factory::CreateGeomtry();
			if (geo)
			{
				ID2D1GeometrySink* skin = NULL;
				geo->Open(&skin);
				if (skin)
				{
					for (path = shape->paths; path != NULL; path = path->next)
					{
						for (int i = 0; i < path->npts - 1; i += 3)
						{
							float* p = &path->pts[i * 2];
							if (i == 0)
								skin->BeginFigure({ p[0] * percen, p[1] * percen }, D2D1_FIGURE_BEGIN_FILLED);
							skin->AddBezier({ {p[2] * percen, p[3] * percen},{p[4] * percen, p[5] * percen},{p[6] * percen, p[7] * percen} });
						}
						skin->EndFigure(path->closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
					}
				}
				skin->Close();
			}

			auto getSvgBrush = [](NSVGpaint paint, float opacity, D2DGraphics* g) -> ID2D1Brush*
				{
					const auto ic2fc = [](int colorInt, float opacity) -> D2D1_COLOR_F
						{
							return D2D1_COLOR_F{ (float)GetRValue(colorInt) / 255.0f ,(float)GetGValue(colorInt) / 255.0f ,(float)GetBValue(colorInt) / 255.0f ,opacity };
						};
					switch (paint.type)
					{
					case NSVG_PAINT_NONE:
						return NULL;
					case NSVG_PAINT_COLOR:
						return g->CreateSolidColorBrush(ic2fc(paint.color, opacity));
					case NSVG_PAINT_LINEAR_GRADIENT:
					{
						std::vector<D2D1_GRADIENT_STOP> cols;
						for (int i = 0; i < paint.gradient->nstops; i++)
						{
							auto stop = paint.gradient->stops[i];
							cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
						}
						return g->CreateLinearGradientBrush(cols.data(), cols.size());
					}
					case NSVG_PAINT_RADIAL_GRADIENT:
					{
						std::vector<D2D1_GRADIENT_STOP> cols;
						for (int i = 0; i < paint.gradient->nstops; i++)
						{
							auto stop = paint.gradient->stops[i];
							cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
						}
						return g->CreateRadialGradientBrush(cols.data(), cols.size(), { paint.gradient->fx,paint.gradient->fy });
					}
					}
					return NULL;
				};

			ID2D1Brush* brush = getSvgBrush(shape->fill, shape->opacity, subg);
			if (brush)
			{
				subg->FillGeometry(geo, brush);
				brush->Release();
			}
			brush = getSvgBrush(shape->stroke, shape->opacity, subg);
			if (brush)
			{
				subg->DrawGeometry(geo, brush, shape->strokeWidth);
				brush->Release();
			}
			geo->Release();
		}
		nsvgDelete(image);
		subg->EndRender();
		delete subg;

		return renderSource;
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
				button->BackColor = theme.SurfaceAlt;
				button->ForeColor = theme.Text;
				button->BolderColor = theme.Border;
				button->UnderMouseColor = theme.AccentSoft;
				button->CheckedColor = theme.Accent;
				break;
			}
			case UIClass::UI_CheckBox:
				((CheckBox*)control)->UnderMouseColor = theme.AccentSoft;
				break;
			case UIClass::UI_RadioBox:
				((RadioBox*)control)->UnderMouseColor = theme.AccentSoft;
				break;
			case UIClass::UI_Switch:
				((Switch*)control)->UnderMouseColor = theme.AccentSoft;
				break;
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
				combo->BackColor = theme.InputBack;
				combo->ForeColor = theme.Text;
				combo->BolderColor = theme.Border;
				combo->UnderMouseBackColor = theme.Accent;
				combo->UnderMouseForeColor = theme.AccentText;
				combo->ScrollBackColor = theme.ScrollTrack;
				combo->ScrollForeColor = theme.ScrollThumb;
				combo->ButtonBackColor = theme.Accent;
				break;
			}
			case UIClass::UI_DateTimePicker:
			{
				auto* picker = (DateTimePicker*)control;
				picker->BackColor = theme.InputBack;
				picker->ForeColor = theme.Text;
				picker->BolderColor = theme.Border;
				picker->PanelBackColor = theme.InputBack;
				picker->DropBackColor = theme.SurfacePanelSoft;
				picker->DropBorderColor = theme.Border;
				picker->HoverColor = theme.AccentSoft;
				picker->AccentColor = theme.Accent;
				picker->SecondaryTextColor = theme.TextMuted;
				picker->FocusBorderColor = theme.Accent;
				break;
			}
			case UIClass::UI_Menu:
			{
				auto* menu = (Menu*)control;
				menu->BackColor = Color(0, 0, 0, 0);
				menu->ForeColor = theme.Text;
				menu->BarBackColor = theme.SurfacePanel;
				menu->BarBorderColor = theme.Border;
				menu->DropBackColor = theme.SurfacePanelSoft;
				menu->DropBorderColor = theme.Border;
				menu->DropHoverColor = theme.AccentSoft;
				menu->DropTextColor = theme.Text;
				menu->DropSeparatorColor = theme.Border;
				break;
			}
			case UIClass::UI_MenuItem:
				((MenuItem*)control)->HoverBackColor = theme.AccentSoft;
				break;
			case UIClass::UI_ContextMenu:
			{
				auto* popup = (ContextMenu*)control;
				popup->PopupBackColor = theme.SurfacePanelSoft;
				popup->PopupBorderColor = theme.Border;
				popup->PopupHoverColor = theme.AccentSoft;
				popup->PopupTextColor = theme.Text;
				popup->PopupSeparatorColor = theme.Border;
				break;
			}
			case UIClass::UI_ToolTip:
			{
				auto* tip = (ToolTip*)control;
				tip->PopupBackColor = theme.SurfacePanelSoft;
				tip->PopupBorderColor = theme.Border;
				tip->PopupTextColor = theme.Text;
				break;
			}
			case UIClass::UI_Panel:
			case UIClass::UI_GroupBox:
			case UIClass::UI_StatusBar:
			case UIClass::UI_StackPanel:
			case UIClass::UI_GridPanel:
			case UIClass::UI_DockPanel:
			case UIClass::UI_WrapPanel:
			case UIClass::UI_RelativePanel:
			case UIClass::UI_SplitContainer:
			case UIClass::UI_TabPage:
				control->BackColor = theme.SurfacePanel;
				control->ForeColor = theme.Text;
				control->BolderColor = theme.Border;
				break;
			case UIClass::UI_ScrollView:
			{
				auto* scroll = (ScrollView*)control;
				scroll->BackColor = theme.SurfacePanel;
				scroll->BolderColor = theme.Border;
				scroll->ScrollBackColor = theme.ScrollTrack;
				scroll->ScrollForeColor = theme.ScrollThumb;
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
				tree->SelectedBackColor = theme.Selection;
				tree->UnderMouseItemBackColor = theme.AccentSoft;
				tree->SelectedForeColor = theme.AccentText;
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
				grid->ButtonBackColor = theme.SurfaceAlt;
				grid->ButtonCheckedColor = theme.SurfacePanelSoft;
				grid->ButtonHoverBackColor = theme.AccentSoft;
				grid->ButtonPressedBackColor = theme.Accent;
				grid->ButtonBorderDarkColor = theme.BorderStrong;
				grid->ButtonBorderLightColor = theme.Border;
				grid->SelectedItemBackColor = theme.Selection;
				grid->SelectedItemForeColor = theme.AccentText;
				grid->UnderMouseItemBackColor = theme.AccentSoft;
				grid->UnderMouseItemForeColor = theme.Text;
				grid->ScrollBackColor = theme.ScrollTrack;
				grid->ScrollForeColor = theme.ScrollThumb;
				grid->EditBackColor = theme.InputBack;
				grid->EditForeColor = theme.Text;
				grid->EditSelectedBackColor = theme.Selection;
				grid->EditSelectedForeColor = theme.AccentText;
				grid->NewRowBackColor = theme.SurfaceAlt;
				grid->NewRowForeColor = theme.TextMuted;
				grid->NewRowIndicatorColor = theme.Accent;
				break;
			}
			case UIClass::UI_Slider:
			{
				auto* slider = (Slider*)control;
				slider->TrackBackColor = theme.ScrollTrack;
				slider->TrackForeColor = theme.Accent;
				slider->ThumbColor = theme.SurfacePanelSoft;
				slider->ThumbBorderColor = theme.BorderStrong;
				break;
			}
			case UIClass::UI_TabControl:
			{
				auto* tabs = (TabControl*)control;
				tabs->BackColor = Color(0, 0, 0, 0);
				tabs->ForeColor = theme.Text;
				tabs->BolderColor = theme.Border;
				tabs->TitleBackColor = theme.SurfaceAlt;
				tabs->SelectedTitleBackColor = theme.AccentSoft;
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
				control->BackColor = theme.SurfacePanelSoft;
				control->BolderColor = theme.Border;
				break;
			case UIClass::UI_ProgressBar:
				control->BackColor = theme.ScrollTrack;
				control->ForeColor = theme.Accent;
				control->BolderColor = theme.Border;
				break;
			case UIClass::UI_LoadingRing:
			case UIClass::UI_ProgressRing:
				control->ForeColor = theme.Accent;
				control->BackColor = Color(0, 0, 0, 0);
				break;
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

	for (int i = 0; i < this->Controls.Count; ++i)
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
		_picture->SetImageEx(ToBitmapFromSvg(svg.c_str()));
	}
	else if (StringHelper::Contains(".jpg.jpeg.png.bmp.webp", StringHelper::ToLower(file.Extension())))
	{
		auto img = BitmapSource::FromFile(Convert::string_to_wstring(ofd.SelectedPaths[0]));
		_picture->SetImageEx(std::move(img));
	}

	Ui_UpdateStatus(L"PictureBox: 已加载图片");
	this->Invalidate();
}

void DemoWindow::Picture_OnDropFile(class Control* sender, List<std::wstring> files)
{
	(void)sender;
	if (!_picture || files.empty()) return;

	_picture->Image = nullptr;

	FileInfo file(Convert::wstring_to_string(files[0]));
	if (file.Extension() == ".svg" || file.Extension() == ".SVG")
	{
		auto svg = File::ReadAllText(file.FullName());
		_picture->SetImageEx(ToBitmapFromSvg(svg.c_str()));
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
	if (!_grid) return;
	auto sw = (Switch*)sender;
	_grid->Enable = sw->Checked;
	Ui_UpdateStatus(sw->Checked ? L"GridView: Enable" : L"GridView: Disable");
	this->Invalidate();
}

void DemoWindow::Data_OnToggleVisible(class Control* sender, MouseEventArgs e)
{
	(void)e;
	if (!_grid) return;
	auto sw = (Switch*)sender;
	_grid->Visible = sw->Checked;
	Ui_UpdateStatus(sw->Checked ? L"GridView: Visible" : L"GridView: Hidden");
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

	_toolbar = this->AddControl(new ToolBar(0, 0, this->Size.cx, 32));
	auto tb1 = _toolbar->AddToolButton(L"Basic", 80);
	auto tb2 = _toolbar->AddToolButton(L"Data", 80);
	auto tb3 = _toolbar->AddToolButton(L"System", 80);

	tb1->OnMouseClick += [this](class Control* s, MouseEventArgs e) { (void)s; (void)e; if (_tabs) _tabs->SelectedIndex = 0; Ui_UpdateStatus(L"ToolBar: Basic"); };
	tb2->OnMouseClick += [this](class Control* s, MouseEventArgs e) { (void)s; (void)e; if (_tabs) _tabs->SelectedIndex = 2; Ui_UpdateStatus(L"ToolBar: Data"); };
	tb3->OnMouseClick += [this](class Control* s, MouseEventArgs e) { (void)s; (void)e; if (_tabs) _tabs->SelectedIndex = 4; Ui_UpdateStatus(L"ToolBar: System"); };

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
	_themeSelector->Items.Add(DemoThemeLabelLight());
	_themeSelector->Items.Add(DemoThemeLabelDark());
	_themeSelector->Margin = Thickness(0, (float)(top + 6), 10, 0);
	_themeSelector->AnchorStyles = AnchorStyles::Top | AnchorStyles::Right;
	_themeSelector->OnSelectionChanged += [this](class Control* sender)
		{
			this->Theme_OnSelectionChanged(sender);
		};

	_tabs = this->AddControl(new TabControl(10, _topSlider->Bottom + 8, this->Size.cx - 20, this->Size.cy - (_topSlider->Bottom + 8) - 10));
	_tabs->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.0f };
	_tabs->Margin = Thickness(10, (float)(_topSlider->Bottom + 8), 10, 40);
	_tabs->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	_tabs->AnimationMode = TabControlAnimationMode::SlideHorizontal;
	auto pBasic = _tabs->AddPage(L"基础控件");
	auto pContainers = _tabs->AddPage(L"容器与图像");
	auto pData = _tabs->AddPage(L"数据控件");
	auto pLayout = _tabs->AddPage(L"布局容器");
	auto pSystem = _tabs->AddPage(L"系统集成");
	auto pWeb = _tabs->AddPage(L"WebBrowser");
	auto pMedia = _tabs->AddPage(L"MediaPlayer");

	BuildTab_Basic(pBasic);
	BuildTab_Containers(pContainers);
	BuildTab_Data(pData);
	BuildTab_Layout(pLayout);
	BuildTab_System(pSystem);
	BuildTab_Web(pWeb);
	BuildTab_Media(pMedia);
}

void DemoWindow::BuildTab_Basic(TabPage* page)
{
	page->AddControl(new Label(L"Button / Label / LinkLabel / TextBox / ComboBox / CheckBox / RadioBox / RichTextBox", 10, 10));
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
	for (int i = 0; i < 30; i++) combo->Items.Add(StringHelper::Format(L"item%d", i));
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
	page->AddControl(new Label(L"Panel / PictureBox / ProgressBar / LoadingRing / ProgressRing / Switch（拖拽文件到图片框）", 10, 10));

	auto openBtn = page->AddControl(new Button(L"打开图片", 10, 40, 120, 28));
	openBtn->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Picture_OnOpenImage(sender, e); };

	auto panel = page->AddControl(new Panel(10, 78, 520, 320));
	panel->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
	panel->BolderColor = D2D1_COLOR_F{ 1,1,1,0.12f };

	panel->AddControl(new Label(L"PictureBox", 10, 10));
	_picture = panel->AddControl(new PictureBox(110, 10, 390, 210));
	_picture->SizeMode = ImageSizeMode::StretchIamge;
	_picture->OnDropFile += [this](class Control* sender, List<std::wstring> files) { this->Picture_OnDropFile(sender, files); };

	panel->AddControl(new Label(L"ProgressBar", 10, 235));
	_progress = panel->AddControl(new ProgressBar(110, 230, 390, 24));
	_progress->PercentageValue = 0.25f;

	page->AddControl(new Label(L"LoadingRing", panel->Right + 20, panel->Top + 100));
	_loadingRing = page->AddControl(new LoadingRing(panel->Right + 44, panel->Top + 130, 56, 56));

	page->AddControl(new Label(L"ProgressRing", panel->Right + 118, panel->Top + 100));
	_progressRing = page->AddControl(new ProgressRing(panel->Right + 136, panel->Top + 124, 92, 92));
	_progressRing->PercentageValue = 0.25f;

	auto swEnable = page->AddControl(new Switch(panel->Right + 20, panel->Top + 10));
	page->AddControl(new Label(L"Enable Panel", swEnable->Right + 8, swEnable->Top));
	swEnable->Checked = true;
	swEnable->OnMouseClick += [panel, this](class Control* sender, MouseEventArgs e)
		{
			(void)e;
			panel->Enable = ((Switch*)sender)->Checked;
			Ui_UpdateStatus(panel->Enable ? L"Panel: Enable" : L"Panel: Disable");
			this->Invalidate();
		};

	auto swVisible = page->AddControl(new Switch(panel->Right + 20, panel->Top + 50));
	page->AddControl(new Label(L"Visible PictureBox", swVisible->Right + 8, swVisible->Top));
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
	auto navStack = split->FirstPanel()->AddControl(new StackPanel(12, 40, 120, 180));
	navStack->SetOrientation(Orientation::Vertical);
	navStack->SetSpacing(6);
	navStack->AddControl(new Button(L"概览", 0, 0, 110, 26));
	navStack->AddControl(new Button(L"资源", 0, 0, 110, 26));
	navStack->AddControl(new Button(L"设置", 0, 0, 110, 26));
	split->SecondPanel()->AddControl(new Label(L"右侧内容区", 12, 12));
	auto splitMemo = split->SecondPanel()->AddControl(new RichTextBox(L"拖拽中间分隔条，调整左右区域宽度。\r\n\r\nSplitContainer 很适合导航 + 内容、属性面板 + 画布等场景。", 12, 40, 230, 170));
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

	page->AddControl(new Label(L"提示：顶部 Slider 同时驱动 ProgressBar、ProgressRing 与 Taskbar 进度。", 10, 420));
}

void DemoWindow::BuildTab_Data(TabPage* page)
{
	page->AddControl(new Label(L"TreeView / GridView / Switch", 10, 10));

	TreeView* tree = page->AddControl(new TreeView(10, 40, 360, 400));
	tree->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Bottom;
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

	_grid = page->AddControl(new GridView(390, 70, 980, 390));
	_grid->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	_grid->Margin = Thickness(390, 70, 10, 10);
	_grid->AllowUserToAddRows = false;
	_grid->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	_grid->HeadFont = new Font(L"Arial", 16);
	_grid->Font = new Font(L"Arial", 16);

	_grid->Columns.Add(GridViewColumn(L"Image", 80, ColumnType::Image));
	GridViewColumn comColumn = GridViewColumn(L"ComboBox", 120, ColumnType::ComboBox);
	comColumn.ComboBoxItems = { L"Item 1", L"Item 2", L"Item 3" };
	_grid->Columns.Add(comColumn);
	_grid->Columns.Add(GridViewColumn(L"Check", 80, ColumnType::Check));
	GridViewColumn textColumn = GridViewColumn(L"Text", 160, ColumnType::Text, true);
	_grid->Columns.Add(textColumn);
	GridViewColumn buttonColumn = GridViewColumn(L"Button", 80, ColumnType::Button);
	buttonColumn.ButtonText = L"OK";
	_grid->Columns.Add(buttonColumn);

	for (int i = 0; i < 48; i++)
	{
		GridViewRow row;
		row.Cells = { _bmps[i % 10], L"Item 1", i % 2 == 0, std::to_wstring(Random::Next()), L"" };
		_grid->Rows.Add(row);
	}

	_gridEnableSwitch = page->AddControl(new Switch(390, 40));
	_gridEnableSwitch->Checked = true;
	_gridEnableSwitch->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Data_OnToggleEnable(sender, e); };
	page->AddControl(new Label(L"Enable", 460, 43));

	_gridVisibleSwitch = page->AddControl(new Switch(520, 40));
	_gridVisibleSwitch->Checked = true;
	_gridVisibleSwitch->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Data_OnToggleVisible(sender, e); };
	page->AddControl(new Label(L"Visible", 590, 43));
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
		top->Size = SIZE{ 320, 28 };
		top->DockPosition = Dock::Top;
		dock->AddControl(top);
		auto left = new Label(L"Left", 0, 0);
		left->Size = SIZE{ 60, 150 };
		left->DockPosition = Dock::Left;
		dock->AddControl(left);
		auto bottom = new Label(L"Bottom", 0, 0);
		bottom->Size = SIZE{ 320, 28 };
		bottom->DockPosition = Dock::Bottom;
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
	page->AddControl(new Label(L"NotifyIcon / Taskbar / ContextMenu", 10, 10));
	page->AddControl(new Label(L"Taskbar：顶部 Slider 会同步设置任务栏进度条（ITaskbarList3）", 10, 40));

	auto btnToggle = page->AddControl(new Button(L"显示/隐藏托盘图标", 10, 80, 180, 30));
	btnToggle->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->System_OnNotifyToggle(sender, e); };

	auto btnBalloon = page->AddControl(new Button(L"气泡提示", 200, 80, 120, 30));
	btnBalloon->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->System_OnBalloonTip(sender, e); };

	page->AddControl(new Label(L"提示：右键托盘图标可弹出菜单；在此页空白区域单击右键可弹出自定义 ContextMenu。", 10, 125));
	page->AddControl(new Label(L"ContextMenu 不绑定具体控件，由业务代码主动调用 ShowAt。", 10, 150));

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
	page->AddControl(new Label(L"WebBrowser（WebView2 Composition）", 10, 10));
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
	speed->Min = 25;
	speed->Max = 200;
	speed->Value = 100;
	speed->OnValueChanged += [mp](class Control* sender, float oldValue, float newValue)
		{
			(void)sender;
			(void)oldValue;
			mp->PlaybackRate = newValue / 100.0f;
		};

	CheckBox* loop = controlPanel->AddControl(new CheckBox(L"循环", 740, 16));
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
	_bmps[0] = ToBitmapFromSvg(_0_ico);
	_bmps[1] = ToBitmapFromSvg(_1_ico);
	_bmps[2] = ToBitmapFromSvg(_2_ico);
	_bmps[3] = ToBitmapFromSvg(_3_ico);
	_bmps[4] = ToBitmapFromSvg(_4_ico);
	_bmps[5] = ToBitmapFromSvg(_5_ico);
	_bmps[6] = ToBitmapFromSvg(_6_ico);
	_bmps[7] = ToBitmapFromSvg(_7_ico);
	_bmps[8] = ToBitmapFromSvg(_8_ico);
	_bmps[9] = ToBitmapFromSvg(_9_ico);
	_icons[0] = ToBitmapFromSvg(icon0);
	_icons[1] = ToBitmapFromSvg(icon1);
	_icons[2] = ToBitmapFromSvg(icon2);
	_icons[3] = ToBitmapFromSvg(icon3);
	_icons[4] = ToBitmapFromSvg(icon4);

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
