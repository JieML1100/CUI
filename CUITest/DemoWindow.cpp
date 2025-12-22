#include "DemoWindow.h"
#include "imgs.h"
#include "../CUI/nanosvg.h"
ID2D1Bitmap* ToBitmapFromSvg(D2DGraphics1* g, const char* data) {
	if (!g || !data) return NULL;
	int len = strlen(data) + 1;
	char* svg_text = new char[len];
	memcpy(svg_text, data, len);
	NSVGimage* image = nsvgParse(svg_text, "px", 96.0f);
	delete[] svg_text;
	if (!image) return NULL;
	float percen = 1.0f;
	if (image->width > 4096 || image->height > 4096) {
		float maxv = image->width > image->height ? image->width : image->height;
		percen = 4096.0f / maxv;
	}
	auto renderSource = BitmapSource::CreateEmpty(image->width * percen, image->height * percen);
	auto subg = new D2DGraphics1(renderSource.get());
	NSVGshape* shape;
	NSVGpath* path;
	subg->BeginRender();
	subg->Clear(D2D1::ColorF(0, 0, 0, 0));
	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		auto geo = Factory::CreateGeomtry();
		if (geo) {
			ID2D1GeometrySink* skin = NULL;
			geo->Open(&skin);
			if (skin) {
				for (path = shape->paths; path != NULL; path = path->next) {
					for (int i = 0; i < path->npts - 1; i += 3) {
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
		auto _get_svg_brush = [](NSVGpaint paint, float opacity, D2DGraphics1* g) ->ID2D1Brush* {
			const auto ic2fc = [](int colorInt, float opacity)->D2D1_COLOR_F {
				return D2D1_COLOR_F{ (float)GetRValue(colorInt) / 255.0f ,(float)GetGValue(colorInt) / 255.0f ,(float)GetBValue(colorInt) / 255.0f ,opacity };
				};
			switch (paint.type) {
			case NSVG_PAINT_NONE: {
				return NULL;
			}break;
			case NSVG_PAINT_COLOR: {
				return g->CreateSolidColorBrush(ic2fc(paint.color, opacity));
			}break;
			case NSVG_PAINT_LINEAR_GRADIENT: {
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++) {
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateLinearGradientBrush(cols.data(), cols.size());
			}break;
			case NSVG_PAINT_RADIAL_GRADIENT: {
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++) {
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateRadialGradientBrush(cols.data(), cols.size(), { paint.gradient->fx,paint.gradient->fy });
			} break;
			}
			return NULL;
			};
		ID2D1Brush* brush = _get_svg_brush(shape->fill, shape->opacity, subg);
		if (brush) {
			subg->FillGeometry(geo, brush);
			brush->Release();
		}
		brush = _get_svg_brush(shape->stroke, shape->opacity, subg);
		if (brush) {
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
void DemoWindow::label1_OnMouseWheel(class Control* sender, MouseEventArgs e)
{
	this->label1->Text = StringHelper::Format(L"MouseWheel Delta=[%d]", e.Delta);
	this->label1->PostRender();
}

void DemoWindow::button1_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	sender->Text = StringHelper::Format(L"独立Tag计数[%d]", sender->Tag++);
	sender->PostRender();
}

void DemoWindow::menu_OnCommand(class Control* sender, int id)
{
	(void)sender;
	switch (id)
	{
	case 101:
		this->label1->Text = L"Menu: 文件 -> 打开";
		this->label1->PostRender();
		break;
	case 102:
		PostMessage(this->Handle, WM_CLOSE, 0, 0);
		break;
	case 201:
		this->label1->Text = L"Menu: 帮助 -> 关于";
		this->label1->PostRender();
		break;
	}
}

void DemoWindow::slider1_OnValueChanged(class Control* sender, float oldValue, float newValue)
{
	(void)sender;
	(void)oldValue;
	this->label1->Text = StringHelper::Format(L"Slider Value=%.0f", newValue);
	this->label1->PostRender();
}

void DemoWindow::radiobox1_OnChecked(class Control* sender)
{
	this->radiobox2->Checked = false;
	this->radiobox2->PostRender();
}

void DemoWindow::radiobox2_OnChecked(class Control* sender)
{
	this->radiobox1->Checked = false;
	this->radiobox1->PostRender();
}

void DemoWindow::bt2_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	PictureBox* picturebox1 = this->picturebox1;
	OpenFileDialog ofd;

	ofd.Filter = MakeDialogFilterStrring("图片文件", "*.jpg;*.png;*.bmp;*.svg;*.webp");
	ofd.SupportMultiDottedExtensions = true;
	ofd.Title = "选择一个图片文件";
	if (ofd.ShowDialog(this->Handle) == DialogResult::OK)
	{
		if (picturebox1->Image)
		{
			picturebox1->Image->Release();
		}
		if (this->Image && this->Image != picturebox1->Image)
		{
			this->Image->Release();
		}
		FileInfo file(ofd.SelectedPaths[0]);
		if (file.Extension() == ".svg" || file.Extension() == ".SVG")
		{
			auto bytes = File::ReadAllBytes(ofd.SelectedPaths[0]);
			picturebox1->Image = this->Image = ToBitmapFromSvg(this->Render, (char*)bytes.data());
			picturebox1->PostRender();
		}
		else
		{
			auto bytes = File::ReadAllBytes(ofd.SelectedPaths[0]);
			auto img = BitmapSource::FromBuffer(bytes.data(), bytes.size());
			picturebox1->Image = this->Image = this->Render->CreateBitmap(img->GetWicBitmap());
			picturebox1->PostRender();
		}
		this->Invalidate();
	}
}

void DemoWindow::sw1_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	Switch* sw = (Switch*)sender;
	this->gridview1->Enable = sw->Checked;
	this->Invalidate();
}

void DemoWindow::sw2_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	Switch* sw = (Switch*)sender;
	this->gridview1->Visible = sw->Checked;
	this->Invalidate();
}

void DemoWindow::iconButton_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
}

void DemoWindow::picturebox1_OnDropFile(class Control* sender, List<std::wstring> files)
{
	if (sender->Image)
	{
		sender->Image->Release();
		sender->Image = NULL;
	}
	FileInfo file(Convert::wstring_to_string(files[0]));
	if (file.Extension() == ".svg" || file.Extension() == ".SVG")
	{
		sender->Image = ToBitmapFromSvg(this->Render, (char*)File::ReadAllBytes(Convert::wstring_to_string(files[0]).c_str()).data());
		sender->PostRender();
	}
	else
	{
		auto img = BitmapSource::FromFile(files[0]);
		sender->Image = sender->ParentForm->Render->CreateBitmap(img->GetWicBitmap());
		sender->PostRender();
	}
}
DemoWindow::DemoWindow() : Form(L"", { 0,0 }, { 1280,640 })
{
	bmps[0] = ToBitmapFromSvg(this->Render, _0_ico);
	bmps[1] = ToBitmapFromSvg(this->Render, _1_ico);
	bmps[2] = ToBitmapFromSvg(this->Render, _2_ico);
	bmps[3] = ToBitmapFromSvg(this->Render, _3_ico);
	bmps[4] = ToBitmapFromSvg(this->Render, _4_ico);
	bmps[5] = ToBitmapFromSvg(this->Render, _5_ico);
	bmps[6] = ToBitmapFromSvg(this->Render, _6_ico);
	bmps[7] = ToBitmapFromSvg(this->Render, _7_ico);
	bmps[8] = ToBitmapFromSvg(this->Render, _8_ico);
	bmps[9] = ToBitmapFromSvg(this->Render, _9_ico);
	icos[0] = ToBitmapFromSvg(this->Render, icon0);
	icos[1] = ToBitmapFromSvg(this->Render, icon1);
	icos[2] = ToBitmapFromSvg(this->Render, icon2);
	icos[3] = ToBitmapFromSvg(this->Render, icon3);
	icos[4] = ToBitmapFromSvg(this->Render, icon4);

	menu1 = this->AddControl(new Menu(0, 0, this->Size.cx, 28));
	menu1->BarBackColor = D2D1_COLOR_F{ 1,1,1,0.08f };
	menu1->DropBackColor = D2D1_COLOR_F{ 0.12f,0.12f,0.12f,0.92f };
	menu1->OnMenuCommand += std::bind_front(&DemoWindow::menu_OnCommand, this);
	{
		auto file = menu1->AddItem(L"文件");
		file->AddSubItem(L"打开", 101);
		auto recent = file->AddSubItem(L"最近打开");
		recent->AddSubItem(L"project_a.cui", 111);
		recent->AddSubItem(L"project_b.cui", 112);
		recent->AddSeparator();
		recent->AddSubItem(L"清空列表", 113);
		file->AddSeparator();
		file->AddSubItem(L"退出", 102);

		auto settings = menu1->AddItem(L"设置");
		auto theme = settings->AddSubItem(L"主题");
		theme->AddSubItem(L"浅色", 301);
		theme->AddSubItem(L"深色", 302);
		settings->AddSeparator();
		settings->AddSubItem(L"重置", 303);

		auto help = menu1->AddItem(L"帮助");
		help->AddSubItem(L"关于", 201);
	}

	toolbar1 = this->AddControl(new ToolBar(0, this->Size.cy - this->HeadHeight - 24, this->Size.cx, 24));
	auto tbNew = toolbar1->AddToolButton(L"New", 90);
	auto tbSave = toolbar1->AddToolButton(L"Save", 90);
	auto tbRun = toolbar1->AddToolButton(L"Run", 90);

	tbNew->OnMouseClick += [&](class Control* s, MouseEventArgs e) { (void)s; (void)e; this->label1->Text = L"ToolBar: New"; this->label1->PostRender(); };
	tbSave->OnMouseClick += [&](class Control* s, MouseEventArgs e) { (void)s; (void)e; this->label1->Text = L"ToolBar: Save"; this->label1->PostRender(); };
	tbRun->OnMouseClick += [&](class Control* s, MouseEventArgs e) { (void)s; (void)e; this->label1->Text = L"ToolBar: Run"; this->label1->PostRender(); };

	slider1 = this->AddControl(new Slider(10, menu1->Bottom + 8, 320, 32));
	slider1->Min = 0;
	slider1->Max = 10000;
	slider1->Value = 50;
	slider1->OnValueChanged += std::bind_front(&DemoWindow::slider1_OnValueChanged, this);

	label1 = this->AddControl(new Label(L"Label", 10, (int)slider1->Bottom + 6));
	label1->OnMouseWheel += std::bind_front(&DemoWindow::label1_OnMouseWheel, this);

	clabel1 = this->AddControl(new CustomLabel1(L"Custom Label", 400, label1->Top));
	button1 = this->AddControl(new Button(L"BUTTON1", 10, this->LastChild()->Bottom + 5, 120, 24));
	button1->OnMouseClick += std::bind_front(&DemoWindow::button1_OnMouseClick, this);
	textbox0 = this->AddControl(new TextBox(L"TextBox", 10, this->LastChild()->Bottom + 5, 120, 20));
	textbox1 = this->AddControl(new CustomTextBox1(L"Custom TextBox", 10, this->LastChild()->Bottom + 5, 120, 20));
	textbox3 = this->AddControl(new RoundTextBox(L"RoundTextBox", 10, this->LastChild()->Bottom + 5, 120, 20));
	pwdbox1 = this->AddControl(new PasswordBox(L"pwd", 10, this->LastChild()->Bottom + 5, 120, 20));
	combobox1 = this->AddControl(new ComboBox(L"item1", 10, this->LastChild()->Bottom + 5, 120, 24));
	combobox1->ExpandCount = 8;
	for (int i = 0; i < 100; i++) {
		combobox1->values.Add(StringHelper::Format(L"item%d", i));
	}
	checkbox1 = this->AddControl(new CheckBox(L"CheckBox", combobox1->Right + 5, button1->Top));
	radiobox1 = this->AddControl(new RadioBox(L"RadioBox1", combobox1->Right + 5, this->LastChild()->Bottom + 5));
	radiobox1->Checked = true;
	radiobox2 = this->AddControl(new RadioBox(L"RadioBox2", combobox1->Right + 5, this->LastChild()->Bottom + 5));
	radiobox1->OnChecked += std::bind_front(&DemoWindow::radiobox1_OnChecked, this);
	radiobox2->OnChecked += std::bind_front(&DemoWindow::radiobox2_OnChecked, this);

	textbox2 = this->AddControl(new RichTextBox(L"RichTextBox", 260, button1->Top, 800, 160));
	textbox2->BackColor = D2D1_COLOR_F{ 1,1,1,0.25f };
	textbox2->FocusedColor = D2D1_COLOR_F{ 1,1,1,0.5f };

	textbox2->AllowMultiLine = true;
	textbox2->ScrollToEnd();
	tabControl1 = this->AddControl(new TabControl(10, combobox1->Bottom + 5, 1200, 300));
	tabControl1->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.0f };
	tabControl1->AddPage(L"Page 1")->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.3f };
	tabControl1->AddPage(L"Grid View")->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.3f };
	tabControl1->AddPage(L"Icon Buttons")->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.3f };
	tabControl1->AddPage(L"Layout Demo")->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.3f };
	tabControl1->AddPage(L"WebBrowser")->BackColor = D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.3f };
	tabControl1->get(0)->AddControl(new Label(L"基本容器", 10, 10));

	bt2 = tabControl1->get(0)->AddControl(new Button(L"打开图片", 120, 10, 120, 24));
	bt2->OnMouseClick += std::bind_front(&DemoWindow::bt2_OnMouseClick, this);
	panel1 = tabControl1->get(0)->AddControl(new Panel(10, 40, 400, 200));

	TreeView* tree = tabControl1->get(0)->AddControl(new TreeView(420, 10, 360, 230));
	tree->Font = new Font(L"宋体", 24);
	tree->BackColor = D2D1_COLOR_F{ 1,1,1,0.25f };
	for (int i = 0; i < 3; i++) {
		auto sub = new TreeNode(StringHelper::Format(L"item%d", i), bmps[1]);
		sub->Expand = true;
		tree->Root->Children.push_back(sub);
		sub->Tag = i;
		for (int j = 0; j < 3; j++) {
			auto ssub = new TreeNode(StringHelper::Format(L"item%d-%d", i, j), bmps[2]);
			sub->Children.push_back(ssub);
			ssub->Tag = i * j;
			for (int n = 0; n < 10; n++) {
				auto sssub = new TreeNode(StringHelper::Format(L"item%d-%d-%d", i, j, n), bmps[3]);
				ssub->Children.push_back(sssub);
				sssub->Tag = i * j * n;
			}
		}
	}

	panel1->AddControl(new Label(L"图片框", 10, 10));
	picturebox1 = panel1->AddControl(new PictureBox(120, 10, 260, 120));
	picturebox1->Image = this->Image;
	picturebox1->SizeMode = ImageSizeMode::StretchIamge;
	picturebox1->OnDropFile += std::bind_front(&DemoWindow::picturebox1_OnDropFile, this);
	panel1->AddControl(new Label(L"Progress Bar", 10, picturebox1->Bottom + 5));
	progressbar1 = panel1->AddControl(new ProgressBar(120, picturebox1->Bottom + 5, 260, 24));
	gridview1 = tabControl1->get(1)->AddControl(new GridView(10, 10, 1000, 200));
	gridview1->HeadFont = new Font(L"Arial", 16);
	gridview1->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	gridview1->Font = new Font(L"Arial", 16);

		GridViewColumn textColumn = GridViewColumn(L"Text", 100, ColumnType::Text, false);
	textColumn.SetSortFunc([](const CellValue& lhs, const CellValue& rhs) -> int {
		wchar_t* end1 = nullptr;
		wchar_t* end2 = nullptr;
		long long a = wcstoll(lhs.Text.c_str(), &end1, 10);
		long long b = wcstoll(rhs.Text.c_str(), &end2, 10);
		if (end1 == lhs.Text.c_str()) a = 0;
		if (end2 == rhs.Text.c_str()) b = 0;
		if (a == b) return 0;
		return (a < b) ? -1 : 1;
	});
	gridview1->Columns.Add(GridViewColumn(L"Image", 80, ColumnType::Image));
	gridview1->Columns.Add(GridViewColumn(L"Check", 80, ColumnType::Check));
	gridview1->Columns.Add(textColumn);
	gridview1->Columns.Add(GridViewColumn(L"Check", 80, ColumnType::Check));
	gridview1->Columns.Add(GridViewColumn(L"Edit", 200, ColumnType::Text, true));
	for (int i = 0; i < 10000; i++)
	{
		GridViewRow row;
		row.Cells = { bmps[i % 10] ,i % 2 == 0,std::to_wstring(Random::Next()) ,i % 3 == 0 ,std::to_wstring(Random::Next()) };
		gridview1->Rows.Add(row);
	}

	sw1 = tabControl1->get(1)->AddControl(new Switch(gridview1->Right + 5, 10));
	sw1->Checked = gridview1->Visible;
	sw1->OnMouseClick += std::bind_front(&DemoWindow::sw1_OnMouseClick, this);

	sw2 = tabControl1->get(1)->AddControl(new Switch(gridview1->Right + 5, 42));
	sw2->Checked = gridview1->Visible;
	sw2->OnMouseClick += std::bind_front(&DemoWindow::sw2_OnMouseClick, this);
	for (int i = 0; i < 5; i++)
	{
		Button* ingButton = tabControl1->get(2)->AddControl(new Button(L"", 10 + (44 * i), 10, 40, 40));
		ingButton->Image = icos[i];
		ingButton->SizeMode = ImageSizeMode::CenterImage;
		ingButton->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
		ingButton->Boder = 2.0f;
		ingButton->OnMouseClick += std::bind_front(&DemoWindow::iconButton_OnMouseClick, this);
	}

	// ========== 布局系统演示 ==========
	{
		auto layoutPage = tabControl1->get(3);
		layoutPage->AddControl(new Label(L"布局系统演示", 10, 10));

		// StackPanel 示例
		auto stackLabel = layoutPage->AddControl(new Label(L"StackPanel:", 10, 40));
		auto stack = layoutPage->AddControl(new StackPanel(10, 60, 280, 200));
		stack->SetOrientation(Orientation::Vertical);
		stack->SetSpacing(5);
		stack->BackColor = D2D1_COLOR_F{ 0.2f, 0.2f, 0.2f, 0.5f };
		
		auto stackBtn1 = new Button(L"按钮 1 (200px)", 0, 0, 200, 25);
		auto stackBtn2 = new Button(L"按钮 2 (180px)", 0, 0, 180, 25);
		auto stackBtn3 = new Button(L"按钮 3 (220px)", 0, 0, 220, 25);
		stack->AddControl(stackBtn1);
		stack->AddControl(stackBtn2);
		stack->AddControl(stackBtn3);

		// GridPanel 示例
		auto gridLabel = layoutPage->AddControl(new Label(L"GridPanel:", 300, 40));
		auto grid = layoutPage->AddControl(new GridPanel(300, 60, 280, 200));
		grid->BackColor = D2D1_COLOR_F{ 0.2f, 0.2f, 0.2f, 0.5f };
		
		grid->AddRow(GridLength::Auto());
		grid->AddRow(GridLength::Star(1.0f));
		grid->AddRow(GridLength::Pixels(30));
		grid->AddColumn(GridLength::Star(1.0f));
		grid->AddColumn(GridLength::Star(1.0f));
		
		auto gridTitle = new Label(L"标题", 0, 0);
		gridTitle->GridRow = 0;
		gridTitle->GridColumn = 0;
		gridTitle->GridColumnSpan = 2;
		gridTitle->HAlign = HorizontalAlignment::Center;
		
		auto gridContent1 = new Button(L"内容1", 0, 0, 100, 80);
		gridContent1->GridRow = 1;
		gridContent1->GridColumn = 0;
		gridContent1->Margin = Thickness(5);
		
		auto gridContent2 = new Button(L"内容2", 0, 0, 100, 80);
		gridContent2->GridRow = 1;
		gridContent2->GridColumn = 1;
		gridContent2->Margin = Thickness(5);
		
		auto gridFooter = new Label(L"底部", 0, 0);
		gridFooter->GridRow = 2;
		gridFooter->GridColumn = 0;
		gridFooter->GridColumnSpan = 2;
		gridFooter->HAlign = HorizontalAlignment::Center;
		
		grid->AddControl(gridTitle);
		grid->AddControl(gridContent1);
		grid->AddControl(gridContent2);
		grid->AddControl(gridFooter);

		// DockPanel 示例
		auto dockLabel = layoutPage->AddControl(new Label(L"DockPanel:", 590, 40));
		auto dock = layoutPage->AddControl(new DockPanel(590, 60, 280, 200));
		dock->BackColor = D2D1_COLOR_F{ 0.2f, 0.2f, 0.2f, 0.5f };
		dock->SetLastChildFill(true);
		
		auto dockTop = new Label(L"Top", 0, 0);
		dockTop->BackColor = D2D1_COLOR_F{ 0.3f, 0.3f, 0.5f, 0.7f };
		dockTop->Size = SIZE{280, 30};
		dockTop->DockPosition = Dock::Top;
		
		auto dockBottom = new Label(L"Bottom", 0, 0);
		dockBottom->BackColor = D2D1_COLOR_F{ 0.5f, 0.3f, 0.3f, 0.7f };
		dockBottom->Size = SIZE{280, 30};
		dockBottom->DockPosition = Dock::Bottom;
		
		auto dockLeft = new Label(L"Left", 0, 0);
		dockLeft->BackColor = D2D1_COLOR_F{ 0.3f, 0.5f, 0.3f, 0.7f };
		dockLeft->Size = SIZE{60, 140};
		dockLeft->DockPosition = Dock::Left;
		
		auto dockFill = new Label(L"Fill", 0, 0);
		dockFill->BackColor = D2D1_COLOR_F{ 0.4f, 0.4f, 0.4f, 0.7f };
		dockFill->DockPosition = Dock::Fill;
		
		dock->AddControl(dockTop);
		dock->AddControl(dockBottom);
		dock->AddControl(dockLeft);
		dock->AddControl(dockFill);

		// WrapPanel 示例
		auto wrapLabel = layoutPage->AddControl(new Label(L"WrapPanel:", 880, 40));
		auto wrap = layoutPage->AddControl(new WrapPanel(880, 60, 300, 200));
		wrap->SetOrientation(Orientation::Horizontal);
		wrap->BackColor = D2D1_COLOR_F{ 0.2f, 0.2f, 0.2f, 0.5f };
		
		for (int i = 1; i <= 8; i++) {
			auto wrapBtn = new Button(
				StringHelper::Format(L"Btn%d", i), 
				0, 0, 
				60,
				25
			);
			wrap->AddControl(wrapBtn);
		}
	}

	// WebBrowser（WebView2 原生嵌入渲染）演示
	{
		auto page = tabControl1->get(4);
		page->AddControl(new Label(L"WebBrowser", 10, 10));
		web1 = page->AddControl(new WebBrowser(10, 36, 1180, 210));

		web1->Navigate(L"https://www.bing.com");
	}

	this->BackColor = Colors::grey31;
	this->SizeMode = ImageSizeMode::StretchIamge;

	this->OnSizeChanged += [&](class Form* sender) {
		(void)sender;
		if (this->menu1) this->menu1->Width = this->Size.cx;
		if (this->toolbar1) {
			this->toolbar1->Width = this->Size.cx;
			this->toolbar1->Top = this->Size.cy - this->HeadHeight - 24;
		}
		};
}

NotifyIcon* TestNotifyIcon(HWND handle)
{
	NotifyIcon* notifyIcon = new NotifyIcon();
	notifyIcon->InitNotifyIcon(handle, 1);
	//notifyIcon->SetIcon(LoadIcon(NULL, IDI_ICON1));
	notifyIcon->SetToolTip(Convert::Utf8ToAnsi("应用程序").c_str());
	notifyIcon->ShowNotifyIcon();

	notifyIcon->AddMenuItem(NotifyIconMenuItem(Convert::Utf8ToAnsi("打开主窗口").c_str(), 1));


	NotifyIconMenuItem settingsMenu(Convert::Utf8ToAnsi("设置").c_str(), 2);
	settingsMenu.AddSubItem(NotifyIconMenuItem(Convert::Utf8ToAnsi("音频设置").c_str(), 21));
	settingsMenu.AddSubItem(NotifyIconMenuItem(Convert::Utf8ToAnsi("显示设置").c_str(), 22));
	settingsMenu.AddSubItem(NotifyIconMenuItem::CreateSeparator());
	settingsMenu.AddSubItem(NotifyIconMenuItem(Convert::Utf8ToAnsi("高级设置").c_str(), 23));

	notifyIcon->AddMenuItem(settingsMenu);

	notifyIcon->AddMenuSeparator();
	notifyIcon->AddMenuItem(NotifyIconMenuItem(Convert::Utf8ToAnsi("退出").c_str(), 3));


	notifyIcon->OnNotifyIconMenuClick += [&](NotifyIcon* sender, int menuId) {
		switch (menuId) {
		case 1:
			ShowWindow(sender->hWnd, SW_SHOWNORMAL);
			break;
		case 21:
			break;
		case 22:
			break;
		case 23:
			break;
		case 3:
			PostMessage(sender->hWnd, WM_CLOSE, 0, 0);
			break;
		}
		};

	return notifyIcon;
}
/*
sample:
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main() {
	auto form = DemoWindow();
	form.Show();
	NotifyIcon* notifyIcon = TestNotifyIcon(form.Handle);
	int index = 0;
	while (1) {
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	notifyIcon->HideNotifyIcon();
	return 0;
}
*/