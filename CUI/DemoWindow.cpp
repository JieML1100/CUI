#include "DemoWindow.h"
#include "imgs.h"
#include "nanosvg.h"
ID2D1Bitmap* ToBitmapFromSvg(D2DGraphics* g, const char* data) {
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
	auto subg = new D2DGraphics(renderSource.get());
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
		auto _get_svg_brush = [](NSVGpaint paint, float opacity, D2DGraphics* g) ->ID2D1Brush* {
			const auto ic2fc = [](int colorInt, float opacity)->D2D1_COLOR_F {
				return D2D1_COLOR_F{ (float)GetRValue(colorInt) / 255.0f ,(float)GetGValue(colorInt) / 255.0f ,(float)GetBValue(colorInt) / 255.0f ,opacity };
				};
			switch (paint.type) {
			case NSVG_PAINT_NONE: {
				return NULL;
			}
								break;
			case NSVG_PAINT_COLOR: {
				return g->CreateSolidColorBrush(ic2fc(paint.color, opacity));
			}
								 break;
			case NSVG_PAINT_LINEAR_GRADIENT: {
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++) {
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateLinearGradientBrush(cols.data(), cols.size());
			}
										   break;
			case NSVG_PAINT_RADIAL_GRADIENT: {
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++) {
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateRadialGradientBrush(cols.data(), cols.size(), { paint.gradient->fx,paint.gradient->fy });
			}
										   break;
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
void label1_OnMouseWheel(class Control* sender, MouseEventArgs e)
{
	sender->Text = StringHelper::Format(L"MouseWheel Delta=[%d]", e.Delta);
	sender->PostRender();
}
void button1_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	sender->Text = StringHelper::Format(L"独立Tag计数[%d]", sender->Tag++);
	sender->PostRender();
}
void radiobox1_OnChecked(class Control* sender)
{
	((DemoWindow*)sender->ParentForm)->radiobox2->Checked = false;
	((DemoWindow*)sender->ParentForm)->radiobox2->PostRender();
}
void radiobox2_OnChecked(class Control* sender)
{
	((DemoWindow*)sender->ParentForm)->radiobox1->Checked = false;
	((DemoWindow*)sender->ParentForm)->radiobox1->PostRender();
}
void bt2_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	DemoWindow* form = ((DemoWindow*)sender->ParentForm);
	PictureBox* picturebox1 = form->picturebox1;
	OpenFileDialog ofd;

	ofd.Filter = MakeDialogFilterStrring("图片文件", "*.jpg;*.png;*.bmp;*.svg;*.webp");
	ofd.SupportMultiDottedExtensions = true;
	ofd.Title = "选择一个图片文件";
	if (ofd.ShowDialog(form->Handle) == DialogResult::OK)
	{
		if (picturebox1->Image)
		{
			picturebox1->Image->Release();
		}
		if (form->Image && form->Image != picturebox1->Image)
		{
			form->Image->Release();
		}
		FileInfo file(ofd.SelectedPaths[0]);
		if (file.Extension() == ".svg" || file.Extension() == ".SVG")
		{
			auto bytes = File::ReadAllBytes(ofd.SelectedPaths[0]);
			picturebox1->Image = form->Image = ToBitmapFromSvg(form->Render,(char*)bytes.data());
			picturebox1->PostRender();
		}
		else
		{
			auto bytes = File::ReadAllBytes(ofd.SelectedPaths[0]);
			auto img = BitmapSource::FromBuffer(bytes.data(), bytes.size());
			picturebox1->Image = form->Image = form->Render->CreateBitmap(img->GetWicBitmap());
			picturebox1->PostRender();
		}
		form->Invalidate();
	}
}
void sw1_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	Switch* sw = (Switch*)sender;
	((DemoWindow*)sender->ParentForm)->gridview1->Enable = sw->Checked;
}
void sw2_OnMouseClick(class Control* sender, MouseEventArgs e)
{
	Switch* sw = (Switch*)sender;
	((DemoWindow*)sender->ParentForm)->gridview1->Visible = sw->Checked;
}
void iconButton_OnMouseClick(class Control* sender, MouseEventArgs e)
{
}
DemoWindow::DemoWindow() : Form(L"", { 0,0 }, { 1280,600 })
{
	bmps[0] = ToBitmapFromSvg(this->Render,_0_ico);
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

	label1 = this->AddControl(new Label(L"Label", 10, 10));
	label1->OnMouseWheel += label1_OnMouseWheel;
	clabel1 = this->AddControl(new CustomLabel1(L"Custom Label", 400, 10));
	button1 = this->AddControl(new Button(L"BUTTON1", 10, this->LastChild()->Bottom + 5, 120, 24));
	button1->OnMouseClick += button1_OnMouseClick;
	textbox0 = this->AddControl(new TextBox(L"TextBox", 10, this->LastChild()->Bottom + 5, 120, 20));
	textbox1 = this->AddControl(new CustomTextBox1(L"Custom TextBox", 10, this->LastChild()->Bottom + 5, 120, 20));
	textbox3 = this->AddControl(new RoundTextBox(L"RoundTextBox", 10, this->LastChild()->Bottom + 5, 120, 20));
	pwdbox1 = this->AddControl(new PasswordBox(L"pwd", 10, this->LastChild()->Bottom + 5, 120, 20));
	combobox1 = this->AddControl(new ComboBox(L"item1", 10, this->LastChild()->Bottom + 5, 120, 24));
	combobox1->ExpandCount = 8;
	for (int i = 0; i < 100; i++)
	{
		combobox1->values.Add(StringHelper::Format(L"item%d", i));
	}
	checkbox1 = this->AddControl(new CheckBox(L"CheckBox", combobox1->Right + 5, button1->Top));
	radiobox1 = this->AddControl(new RadioBox(L"RadioBox1", combobox1->Right + 5, this->LastChild()->Bottom + 5));
	radiobox1->Checked = true;
	radiobox2 = this->AddControl(new RadioBox(L"RadioBox2", combobox1->Right + 5, this->LastChild()->Bottom + 5));
	radiobox1->OnChecked += radiobox1_OnChecked;
	radiobox2->OnChecked += radiobox2_OnChecked;

	textbox2 = this->AddControl(new RichTextBox(L"RichTextBox", 260, button1->Top, 800, 160));
	textbox2->BackColor = D2D1_COLOR_F{ 1,1,1,0.25f };
	textbox2->FocusedColor = D2D1_COLOR_F{ 1,1,1,0.5f };

	textbox2->AllowMultiLine = true;
	textbox2->ScrollToEnd();
	tabControl1 = this->AddControl(new TabControl(10, combobox1->Bottom + 5, 1200, 300));
	tabControl1->BackColor = D2D1_COLOR_F{ 1,1,1,0.0 };
	tabControl1->AddPage(L"Page 1")->BackColor = D2D1_COLOR_F{ 1,1,1,0.3 };
	tabControl1->AddPage(L"Grid View")->BackColor = D2D1_COLOR_F{ 1,1,1,0.3 };
	tabControl1->AddPage(L"Icon Buttons")->BackColor = D2D1_COLOR_F{ 1,1,1,0.3 };
	tabControl1->get(0)->AddControl(new Label(L"基本容器", 10, 10));

	bt2 = tabControl1->get(0)->AddControl(new Button(L"打开图片", 120, 10, 120, 24));
	bt2->OnMouseClick += bt2_OnMouseClick;
	panel1 = tabControl1->get(0)->AddControl(new Panel(10, 40, 400, 200));

	TreeView* tree = tabControl1->get(0)->AddControl(new TreeView(420, 10, 360, 230));
	tree->Font = new Font(L"宋体", 24);
	tree->BackColor = D2D1_COLOR_F{ 1,1,1,0.25f };
	for (int i = 0; i < 3; i++)
	{
		auto sub = new TreeNode(StringHelper::Format(L"item%d", i), bmps[1]);
		sub->Expand = true;
		tree->Root->Children.push_back(sub);
		sub->Tag = i;
		for (int j = 0; j < 3; j++)
		{
			auto ssub = new TreeNode(StringHelper::Format(L"item%d-%d", i, j), bmps[2]);
			sub->Children.push_back(ssub);
			ssub->Tag = i * j;
			for (int n = 0; n < 10; n++)
			{
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
	picturebox1->OnDropFile += [&](class Control* sender, List<std::wstring> files)
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
		};
	panel1->AddControl(new Label(L"Progress Bar", 10, picturebox1->Bottom + 5));
	progressbar1 = panel1->AddControl(new ProgressBar(120, picturebox1->Bottom + 5, 260, 24));
	gridview1 = tabControl1->get(1)->AddControl(new GridView(10, 10, 1000, 200));
	gridview1->HeadFont = new Font(L"Arial", 16);
	gridview1->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	gridview1->Font = new Font(L"Arial", 16);
	gridview1->Columns.Add(GridViewColumn(L"Image", 80, ColumnType::Image));
	gridview1->Columns.Add(GridViewColumn(L"Check", 80, ColumnType::Check));
	gridview1->Columns.Add(GridViewColumn(L"Text", 100, ColumnType::Text, false));
	gridview1->Columns.Add(GridViewColumn(L"Check", 80, ColumnType::Check));
	gridview1->Columns.Add(GridViewColumn(L"Edit", 200, ColumnType::Text, true));
	for (int i = 0; i < 100; i++)
	{
		GridViewRow row;
		row.Cells = { bmps[i % 10] ,i % 2 == 0,std::to_wstring(Random::Next()) ,i % 3 == 0 ,std::to_wstring(Random::Next()) };
		gridview1->Rows.Add(row);
	}
	sw1 = tabControl1->get(1)->AddControl(new Switch(gridview1->Right + 5, 10));
	sw1->Checked = gridview1->Visible;
	sw1->OnMouseClick += sw1_OnMouseClick;

	sw2 = tabControl1->get(1)->AddControl(new Switch(gridview1->Right + 5, 42));
	sw2->Checked = gridview1->Visible;
	sw2->OnMouseClick += sw2_OnMouseClick;
	for (int i = 0; i < 5; i++)
	{
		Button* ingButton = tabControl1->get(2)->AddControl(new Button(L"", 10 + (44 * i), 10, 40, 40));
		ingButton->Image = icos[i];
		ingButton->SizeMode = ImageSizeMode::CenterImage;
		ingButton->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
		ingButton->Boder = 2.0f;
		ingButton->OnMouseClick += iconButton_OnMouseClick;
	}

	this->BackColor = Colors::grey31;
	this->SizeMode = ImageSizeMode::StretchIamge;
}