#include "DemoWindow_Legacy.h"

#include "imgs.h"
#include "../CUI/nanosvg.h"

#include <memory>

namespace {

	ID2D1Bitmap* ToBitmapFromSvg(D2DGraphics* g, const char* data)
	{
		if (!g || !data) return NULL;
		int len = (int)strlen(data) + 1;
		char* svg_text = new char[len];
		memcpy(svg_text, data, len);
		NSVGimage* image = nsvgParse(svg_text, "px", 96.0f);
		delete[] svg_text;
		if (!image) return NULL;
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

		auto result = g->CreateBitmap(renderSource);
		renderSource->GetWicBitmap()->Release();
		delete subg;

		return result;
	}

	std::wstring FileNameFromPath(const std::wstring& path)
	{
		size_t pos = path.find_last_of(L"\\/");
		return (pos != std::wstring::npos) ? path.substr(pos + 1) : path;
	}

}

DemoWindow_Legacy::~DemoWindow_Legacy()
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
	for (auto& b : _bmps)
	{
		if (b)
		{
			b->Release();
			b = nullptr;
		}
	}
	for (auto& i : _icons)
	{
		if (i)
		{
			i->Release();
			i = nullptr;
		}
	}
}

void DemoWindow_Legacy::Ui_UpdateStatus(const std::wstring& text)
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

void DemoWindow_Legacy::Ui_UpdateProgress(float value01)
{
	if (value01 < 0.0f) value01 = 0.0f;
	if (value01 > 1.0f) value01 = 1.0f;
	if (_progress)
	{
		_progress->PercentageValue = value01;
		_progress->PostRender();
	}
	if (_taskbar)
	{
		_taskbar->SetValue((ULONGLONG)(value01 * 1000.0f), 1000);
	}
}

void DemoWindow_Legacy::Menu_OnCommand(class Control* sender, int id)
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

void DemoWindow_Legacy::Basic_OnMouseWheel(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	Ui_UpdateStatus(StringHelper::Format(L"MouseWheel Delta=[%d]", e.Delta));
}

void DemoWindow_Legacy::Basic_OnButtonClick(class Control* sender, MouseEventArgs e)
{
	(void)e;
	sender->Text = StringHelper::Format(L"点击计数[%d]", sender->Tag++);
	sender->PostRender();
	Ui_UpdateStatus(L"Button: OnMouseClick");
}

void DemoWindow_Legacy::Basic_OnRadioChecked(class Control* sender)
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

void DemoWindow_Legacy::Basic_OnIconButtonClick(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
	MessageBoxW(this->Handle, L"Icon Button Clicked", L"CUI", MB_OK);
}

void DemoWindow_Legacy::Picture_OnOpenImage(class Control* sender, MouseEventArgs e)
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

	if (_picture->Image)
	{
		_picture->Image->Release();
		_picture->Image = nullptr;
	}

	FileInfo file(ofd.SelectedPaths[0]);
	if (file.Extension() == ".svg" || file.Extension() == ".SVG")
	{
		auto svg = File::ReadAllText(file.FullName());
		_picture->SetImageEx(ToBitmapFromSvg(this->Render, svg.c_str()), false);
	}
	else if (StringHelper::Contains(".jpg.jpeg.png.bmp.webp", StringHelper::ToLower(file.Extension())))
	{
		auto img = BitmapSource::FromFile(Convert::string_to_wstring(ofd.SelectedPaths[0]));
		_picture->SetImageEx(this->Render->CreateBitmap(img->GetWicBitmap()), false);
		img.reset();
	}

	Ui_UpdateStatus(L"PictureBox: 已加载图片");
	this->Invalidate();
}

void DemoWindow_Legacy::Picture_OnDropFile(class Control* sender, List<std::wstring> files)
{
	(void)sender;
	if (!_picture || files.empty()) return;

	if (_picture->Image)
	{
		_picture->Image->Release();
		_picture->Image = nullptr;
	}

	FileInfo file(Convert::wstring_to_string(files[0]));
	if (file.Extension() == ".svg" || file.Extension() == ".SVG")
	{
		auto svg = File::ReadAllText(file.FullName());
		_picture->SetImageEx(ToBitmapFromSvg(this->Render, svg.c_str()), false);
	}
	else if (StringHelper::Contains(".png.jpg.jpeg.bmp.webp", StringHelper::ToLower(file.Extension())))
	{
		auto img = BitmapSource::FromFile(files[0]);
		_picture->SetImageEx(this->Render->CreateBitmap(img->GetWicBitmap()), false);
		img.reset();
	}
	Ui_UpdateStatus(L"PictureBox: 拖拽载入");
	this->Invalidate();
}

void DemoWindow_Legacy::Data_OnToggleEnable(class Control* sender, MouseEventArgs e)
{
	(void)e;
	if (!_grid) return;
	auto sw = (Switch*)sender;
	_grid->Enable = sw->Checked;
	Ui_UpdateStatus(sw->Checked ? L"GridView: Enable" : L"GridView: Disable");
	this->Invalidate();
}

void DemoWindow_Legacy::Data_OnToggleVisible(class Control* sender, MouseEventArgs e)
{
	(void)e;
	if (!_grid) return;
	auto sw = (Switch*)sender;
	_grid->Visible = sw->Checked;
	Ui_UpdateStatus(sw->Checked ? L"GridView: Visible" : L"GridView: Hidden");
	this->Invalidate();
}

void DemoWindow_Legacy::System_OnNotifyToggle(class Control* sender, MouseEventArgs e)
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

void DemoWindow_Legacy::System_OnBalloonTip(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
	if (!_notify) return;
	_notify->ShowBalloonTip("CUI", "NotifyIcon Balloon Tip", 3000, NIIF_INFO);
	Ui_UpdateStatus(L"NotifyIcon: BalloonTip");
}

void DemoWindow_Legacy::BuildMenuToolStatus()
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
	_toolbar->Top = _menu->Bottom;
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

void DemoWindow_Legacy::BuildTabs()
{
	int top = _toolbar ? _toolbar->Bottom : 0;

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

	_tabs = this->AddControl(new TabControl(10, _topSlider->Bottom + 8, this->Size.cx - 20, this->Size.cy - (_topSlider->Bottom + 8) - 10));
	_tabs->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.0f };
	_tabs->Margin = Thickness(10, 0, 10, 40);
	_tabs->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;

	auto pBasic = _tabs->AddPage(L"基础控件");
	auto pContainers = _tabs->AddPage(L"容器与图像");
	auto pData = _tabs->AddPage(L"数据控件");
	auto pLayout = _tabs->AddPage(L"布局容器");
	auto pSystem = _tabs->AddPage(L"系统集成");
	auto pMedia = _tabs->AddPage(L"MediaPlayer");

	BuildTab_Basic(pBasic);
	BuildTab_Containers(pContainers);
	BuildTab_Data(pData);
	BuildTab_Layout(pLayout);
	BuildTab_System(pSystem);
	BuildTab_Media(pMedia);
}

void DemoWindow_Legacy::BuildTab_Basic(TabPage* page)
{
	page->AddControl(new Label(L"Button / Label / TextBox / ComboBox / CheckBox / RadioBox / RichTextBox", 10, 10));
	page->AddControl(new CustomLabel1(L"CustomLabel1（渐变绘制）", 10, 38));

	_basicButton = page->AddControl(new Button(L"点击计数[0]", 10, 70, 160, 28));
	_basicButton->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->Basic_OnButtonClick(sender, e); };

	_basicEnableCheck = page->AddControl(new CheckBox(L"启用输入框", 180, 74));
	_basicEnableCheck->Checked = true;

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
			Ui_UpdateStatus(StringHelper::Format(L"ComboBox: %s", combo->Text.c_str()));
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

void DemoWindow_Legacy::BuildTab_Containers(TabPage* page)
{
	page->AddControl(new Label(L"Panel / PictureBox / ProgressBar / Switch（拖拽文件到图片框）", 10, 10));

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

	page->AddControl(new Label(L"提示：顶部 Slider 同时驱动 ProgressBar 与 Taskbar 进度。", 10, 420));
}

void DemoWindow_Legacy::BuildTab_Data(TabPage* page)
{
	page->AddControl(new Label(L"TreeView / GridView / Switch", 10, 10));

	TreeView* tree = page->AddControl(new TreeView(10, 40, 360, 420));
	tree->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Bottom;
	tree->BackColor = D2D1_COLOR_F{ 1,1,1,0.06f };
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

void DemoWindow_Legacy::BuildTab_Layout(TabPage* page)
{
	page->AddControl(new Label(L"StackPanel / GridPanel / DockPanel / WrapPanel / RelativePanel", 10, 10));

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
}

void DemoWindow_Legacy::BuildTab_System(TabPage* page)
{
	page->AddControl(new Label(L"NotifyIcon / Taskbar", 10, 10));
	page->AddControl(new Label(L"Taskbar：顶部 Slider 会同步设置任务栏进度条（ITaskbarList3）", 10, 40));

	auto btnToggle = page->AddControl(new Button(L"显示/隐藏托盘图标", 10, 80, 180, 30));
	btnToggle->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->System_OnNotifyToggle(sender, e); };

	auto btnBalloon = page->AddControl(new Button(L"气泡提示", 200, 80, 120, 30));
	btnBalloon->OnMouseClick += [this](class Control* sender, MouseEventArgs e) { this->System_OnBalloonTip(sender, e); };

	page->AddControl(new Label(L"提示：右键托盘图标可弹出菜单。", 10, 125));
}

void DemoWindow_Legacy::BuildTab_Media(TabPage* page)
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

	Label* progressLabel = controlPanel->AddControl(new Label(L"进度", 10, 62));
	progressLabel->ForeColor = Colors::LightGray;

	Slider* progressSlider = controlPanel->AddControl(new Slider(60, 58, 900, 30));
	progressSlider->Min = 0;
	progressSlider->Max = 1000;
	progressSlider->Value = 0;
	progressSlider->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right | AnchorStyles::Bottom;

	Label* timeLabel = controlPanel->AddControl(new Label(L"00:00 / 00:00", 970, 62));
	timeLabel->ForeColor = Colors::LightGray;
	timeLabel->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
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

DemoWindow_Legacy::DemoWindow_Legacy() : Form(L"CUI Test Demo", { 0,0 }, { 1400,800 })
{
	_bmps[0] = ToBitmapFromSvg(this->Render, _0_ico);
	_bmps[1] = ToBitmapFromSvg(this->Render, _1_ico);
	_bmps[2] = ToBitmapFromSvg(this->Render, _2_ico);
	_bmps[3] = ToBitmapFromSvg(this->Render, _3_ico);
	_bmps[4] = ToBitmapFromSvg(this->Render, _4_ico);
	_bmps[5] = ToBitmapFromSvg(this->Render, _5_ico);
	_bmps[6] = ToBitmapFromSvg(this->Render, _6_ico);
	_bmps[7] = ToBitmapFromSvg(this->Render, _7_ico);
	_bmps[8] = ToBitmapFromSvg(this->Render, _8_ico);
	_bmps[9] = ToBitmapFromSvg(this->Render, _9_ico);
	_icons[0] = ToBitmapFromSvg(this->Render, icon0);
	_icons[1] = ToBitmapFromSvg(this->Render, icon1);
	_icons[2] = ToBitmapFromSvg(this->Render, icon2);
	_icons[3] = ToBitmapFromSvg(this->Render, icon3);
	_icons[4] = ToBitmapFromSvg(this->Render, icon4);

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

	BuildMenuToolStatus();
	BuildTabs();

	this->BackColor = Colors::grey31;
	this->SizeMode = ImageSizeMode::StretchIamge;

	this->OnSizeChanged += [&](class Form* sender)
		{
			(void)sender;
			if (_menu) _menu->Width = this->Size.cx;
			if (_toolbar)
			{
				_toolbar->Width = this->Size.cx;
				_toolbar->Top = _menu ? _menu->Height : 0;
			}
			if (_statusbar)
			{
				_statusbar->Width = this->Size.cx;
				_statusbar->Top = this->Size.cy - this->HeadHeight - _statusbar->Height;
			}
		};
}