#include "DemoWindow1.h"
#include <functional>

DemoWindow1::DemoWindow1()
	: Form(L"Form", POINT{ 100, 100 }, SIZE{ 860, 600 })
{

	// 窗体属性（标题栏/按钮/缩放）
	this->VisibleHead = true;
	this->HeadHeight = 24;
	this->MinBox = true;
	this->MaxBox = true;
	this->CloseBox = true;
	this->CenterTitle = true;
	this->AllowResize = true;

	// 窗体属性（通用）
	this->BackColor = D2D1::ColorF(0.753f, 0.753f, 0.753f, 1.f);
	this->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	this->ShowInTaskBar = true;
	this->TopMost = false;
	this->Enable = true;
	this->Visible = true;

	// Font
	auto* __formFont = new ::Font(L"Arial", 16.f);
	this->SetFontEx(__formFont, true);

	// 创建控件
	// tabControl1
	tabControl1 = new TabControl(10, 28, 840, 538);
	tabControl1->SetFontEx(__formFont, false);
	tabControl1->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	tabControl1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	tabControl1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	tabControl1->Margin = Thickness(10.f, 28.f, 10.f, 10.f);
	tabControl1->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	tabControl1->SelectIndex = 0;

	// label1
	label1 = new Label(L"标签", 10, 6);
	label1->Size = {36, 23};
	label1->SetFontEx(__formFont, false);
	label1->BackColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);
	label1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	label1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// button1
	button1 = new Button(L"按钮", 10, 36, 200, 30);
	button1->SetFontEx(__formFont, false);
	button1->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button1->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// textBox1
	textBox1 = new TextBox(L"textbox", 10, 76, 200, 25);
	textBox1->SetFontEx(__formFont, false);
	textBox1->BackColor = D2D1::ColorF(0.8275f, 0.8275f, 0.8275f, 1.f);
	textBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	textBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// passwordBox1
	passwordBox1 = new PasswordBox(L"qwe", 10, 116, 200, 25);
	passwordBox1->SetFontEx(__formFont, false);
	passwordBox1->BackColor = D2D1::ColorF(0.75f, 0.75f, 0.75f, 0.75f);
	passwordBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	passwordBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// richTextBox1
	richTextBox1 = new RichTextBox(L"RichTextBox", 10, 156, 300, 160);
	richTextBox1->SetFontEx(__formFont, false);
	richTextBox1->BackColor = D2D1::ColorF(0.8275f, 0.8275f, 0.8275f, 1.f);
	richTextBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	richTextBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// checkBox1
	checkBox1 = new CheckBox(L"复选框", 10, 326);
	checkBox1->Size = {110, 23};
	checkBox1->SetFontEx(__formFont, false);
	checkBox1->BackColor = D2D1::ColorF(0.75f, 0.75f, 0.75f, 0.75f);
	checkBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	checkBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// radioBox1
	radioBox1 = new RadioBox(L"单选框", 10, 356);
	radioBox1->Size = {79, 23};
	radioBox1->SetFontEx(__formFont, false);
	radioBox1->BackColor = D2D1::ColorF(0.75f, 0.75f, 0.75f, 0.75f);
	radioBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	radioBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// comboBox1
	comboBox1 = new ComboBox(L"1", 10, 389, 150, 25);
	comboBox1->SetFontEx(__formFont, false);
	comboBox1->BackColor = D2D1::ColorF(0.75f, 0.75f, 0.75f, 0.75f);
	comboBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	comboBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	comboBox1->values.Clear();
	comboBox1->values.Add(L"1");
	comboBox1->values.Add(L"2");
	comboBox1->values.Add(L"3");
	comboBox1->values.Add(L"4");
	comboBox1->SelectedIndex = 0;

	// progressBar1
	progressBar1 = new ProgressBar(10, 426, 300, 20);
	progressBar1->SetFontEx(__formFont, false);
	progressBar1->BackColor = D2D1::ColorF(0.5451f, 0.5451f, 0.4784f, 1.f);
	progressBar1->ForeColor = D2D1::ColorF(0.4863f, 0.9882f, 0.f, 1.f);
	progressBar1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// slider1
	slider1 = new Slider(170, 386, 140, 30);
	slider1->SetFontEx(__formFont, false);
	slider1->BackColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);
	slider1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	slider1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);

	// switch1
	switch1 = new Switch(130, 326, 60, 30);
	switch1->SetFontEx(__formFont, false);
	switch1->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 0.f);
	switch1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	switch1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// gridView15
	gridView15 = new GridView(320, 38, 510, 278);
	gridView15->SetFontEx(__formFont, false);
	gridView15->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	gridView15->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	gridView15->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	gridView15->Margin = Thickness(0.f, 0.f, 10.f, 198.f);
	gridView15->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;
	gridView15->Columns.Clear();
	gridView15->Columns.Add(GridViewColumn(L"Column1", 120.f, ColumnType::Text, true));
	gridView15->Columns.Add(GridViewColumn(L"Column2", 120.f, ColumnType::Check, true));
	gridView15->Columns.Add(GridViewColumn(L"Column3", 120.f, ColumnType::Text, true));
	gridView15->Columns.Add(GridViewColumn(L"Column4", 120.f, ColumnType::Text, true));

	// treeView16
	treeView16 = new TreeView(320, 326, 160, 150);
	treeView16->SetFontEx(__formFont, false);
	treeView16->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	treeView16->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	treeView16->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	treeView16->Margin = Thickness(0.f, 0.f, 360.f, 38.f);
	treeView16->AnchorStyles = AnchorStyles::Left | AnchorStyles::Bottom;
	// TreeView nodes
	for (auto n : treeView16->Root->Children) delete n;
	treeView16->Root->Children.Clear();
	auto* treeView16_node1 = new TreeNode(L"L1");
	treeView16_node1->Expand = true;
	treeView16->Root->Children.push_back(treeView16_node1);
	auto* treeView16_node2 = new TreeNode(L"L2");
	treeView16_node1->Children.push_back(treeView16_node2);
	auto* treeView16_node3 = new TreeNode(L"L2");
	treeView16_node1->Children.push_back(treeView16_node3);
	auto* treeView16_node4 = new TreeNode(L"L1");
	treeView16_node4->Expand = true;
	treeView16->Root->Children.push_back(treeView16_node4);
	auto* treeView16_node5 = new TreeNode(L"L2");
	treeView16_node5->Expand = true;
	treeView16_node4->Children.push_back(treeView16_node5);
	auto* treeView16_node6 = new TreeNode(L"L3");
	treeView16_node5->Children.push_back(treeView16_node6);
	auto* treeView16_node7 = new TreeNode(L"L3");
	treeView16_node5->Children.push_back(treeView16_node7);
	auto* treeView16_node8 = new TreeNode(L"L1");
	treeView16_node8->Expand = true;
	treeView16->Root->Children.push_back(treeView16_node8);
	auto* treeView16_node9 = new TreeNode(L"L2");
	treeView16_node8->Children.push_back(treeView16_node9);
	auto* treeView16_node10 = new TreeNode(L"L2");
	treeView16_node10->Expand = true;
	treeView16_node8->Children.push_back(treeView16_node10);
	auto* treeView16_node11 = new TreeNode(L"L3");
	treeView16_node10->Children.push_back(treeView16_node11);
	auto* treeView16_node12 = new TreeNode(L"L3");
	treeView16_node10->Children.push_back(treeView16_node12);

	// pictureBox1
	pictureBox1 = new PictureBox(490, 326, 340, 150);
	pictureBox1->SetFontEx(__formFont, false);
	pictureBox1->BackColor = D2D1::ColorF(0.75f, 0.75f, 0.75f, 0.75f);
	pictureBox1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	pictureBox1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	pictureBox1->Margin = Thickness(0.f, 0.f, 10.f, 38.f);
	pictureBox1->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right | AnchorStyles::Bottom;

	// stackPanel18
	stackPanel18 = new StackPanel(10, 18, 200, 200);
	stackPanel18->SetFontEx(__formFont, false);
	stackPanel18->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	stackPanel18->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	stackPanel18->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	stackPanel18->SetOrientation(Orientation::Vertical);
	stackPanel18->SetSpacing(0.f);

	// button22
	button22 = new Button(L"按钮", 0, 0, 120, 30);
	button22->SetFontEx(__formFont, false);
	button22->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button22->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button22->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button19
	button19 = new Button(L"按钮", 0, 30, 120, 30);
	button19->SetFontEx(__formFont, false);
	button19->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button19->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button19->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button20
	button20 = new Button(L"按钮", 0, 60, 120, 30);
	button20->SetFontEx(__formFont, false);
	button20->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button20->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button20->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// label21
	label21 = new Label(L"标签", 0, 90);
	label21->Size = {36, 18};
	label21->SetFontEx(__formFont, false);
	label21->BackColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);
	label21->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	label21->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// label23
	label23 = new Label(L"标签", 0, 108);
	label23->Size = {120, 20};
	label23->SetFontEx(__formFont, false);
	label23->BackColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);
	label23->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	label23->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// gridPanel24
	gridPanel24 = new GridPanel(430, 18, 200, 200);
	gridPanel24->SetFontEx(__formFont, false);
	gridPanel24->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	gridPanel24->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	gridPanel24->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	gridPanel24->ClearRows();
	gridPanel24->ClearColumns();
	gridPanel24->AddRow(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddRow(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddRow(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddRow(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddColumn(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddColumn(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddColumn(GridLength::Star(1.f), 0.f, FLT_MAX);
	gridPanel24->AddColumn(GridLength::Star(1.f), 0.f, FLT_MAX);

	// button25
	button25 = new Button(L"按钮", 0, 0, 50, 50);
	button25->SetFontEx(__formFont, false);
	button25->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button25->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button25->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button26
	button26 = new Button(L"按钮", 50, 0, 50, 50);
	button26->SetFontEx(__formFont, false);
	button26->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button26->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button26->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button27
	button27 = new Button(L"按钮", 100, 0, 50, 50);
	button27->SetFontEx(__formFont, false);
	button27->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button27->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button27->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button28
	button28 = new Button(L"按钮", 150, 0, 50, 50);
	button28->SetFontEx(__formFont, false);
	button28->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button28->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button28->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button29
	button29 = new Button(L"按钮", 0, 50, 50, 50);
	button29->SetFontEx(__formFont, false);
	button29->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button29->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button29->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button30
	button30 = new Button(L"按钮", 50, 50, 50, 50);
	button30->SetFontEx(__formFont, false);
	button30->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button30->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button30->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button31
	button31 = new Button(L"按钮", 100, 50, 50, 50);
	button31->SetFontEx(__formFont, false);
	button31->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button31->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button31->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button32
	button32 = new Button(L"按钮", 150, 50, 50, 50);
	button32->SetFontEx(__formFont, false);
	button32->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button32->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button32->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button33
	button33 = new Button(L"按钮", 0, 100, 50, 50);
	button33->SetFontEx(__formFont, false);
	button33->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button33->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button33->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button34
	button34 = new Button(L"按钮", 0, 150, 50, 50);
	button34->SetFontEx(__formFont, false);
	button34->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button34->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button34->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button35
	button35 = new Button(L"按钮", 50, 100, 50, 50);
	button35->SetFontEx(__formFont, false);
	button35->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button35->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button35->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button36
	button36 = new Button(L"按钮", 50, 150, 50, 50);
	button36->SetFontEx(__formFont, false);
	button36->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button36->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button36->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button37
	button37 = new Button(L"按钮", 100, 100, 50, 50);
	button37->SetFontEx(__formFont, false);
	button37->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button37->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button37->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button38
	button38 = new Button(L"按钮", 100, 150, 50, 50);
	button38->SetFontEx(__formFont, false);
	button38->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button38->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button38->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button39
	button39 = new Button(L"按钮", 150, 100, 50, 50);
	button39->SetFontEx(__formFont, false);
	button39->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button39->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button39->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button52
	button52 = new Button(L"按钮", 150, 150, 50, 50);
	button52->SetFontEx(__formFont, false);
	button52->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button52->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button52->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// dockPanel41
	dockPanel41 = new DockPanel(220, 18, 200, 200);
	dockPanel41->SetFontEx(__formFont, false);
	dockPanel41->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	dockPanel41->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	dockPanel41->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	dockPanel41->Padding = Thickness(50.f, 50.f, 50.f, 50.f);
	dockPanel41->SetLastChildFill(true);

	// button53
	button53 = new Button(L"按钮", 50, 50, 100, 100);
	button53->SetFontEx(__formFont, false);
	button53->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button53->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button53->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// wrapPanel43
	wrapPanel43 = new WrapPanel(220, 224, 200, 200);
	wrapPanel43->SetFontEx(__formFont, false);
	wrapPanel43->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	wrapPanel43->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	wrapPanel43->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	wrapPanel43->SetOrientation(Orientation::Horizontal);
	wrapPanel43->SetItemWidth(0.f);
	wrapPanel43->SetItemHeight(0.f);

	// button44
	button44 = new Button(L"按钮", 0, 0, 130, 30);
	button44->SetFontEx(__formFont, false);
	button44->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button44->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button44->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button46
	button46 = new Button(L"按钮", 0, 30, 130, 30);
	button46->SetFontEx(__formFont, false);
	button46->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button46->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button46->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button45
	button45 = new Button(L"按钮", 0, 60, 130, 30);
	button45->SetFontEx(__formFont, false);
	button45->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button45->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button45->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// button47
	button47 = new Button(L"按钮", 0, 90, 130, 30);
	button47->SetFontEx(__formFont, false);
	button47->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button47->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button47->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);

	// relativePanel48
	relativePanel48 = new RelativePanel(10, 224, 200, 200);
	relativePanel48->SetFontEx(__formFont, false);
	relativePanel48->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	relativePanel48->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	relativePanel48->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// button50
	button50 = new Button(L"按钮", 30, 64, 120, 30);
	button50->SetFontEx(__formFont, false);
	button50->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button50->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button50->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);
	button50->Margin = Thickness(30.f, 64.f, 0.f, 0.f);

	// button49
	button49 = new Button(L"按钮", 30, 34, 120, 30);
	button49->SetFontEx(__formFont, false);
	button49->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button49->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button49->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);
	button49->Margin = Thickness(30.f, 34.f, 0.f, 0.f);

	// panel1
	panel1 = new Panel(430, 224, 200, 200);
	panel1->SetFontEx(__formFont, false);
	panel1->BackColor = D2D1::ColorF(0.9098f, 0.9098f, 0.9098f, 1.f);
	panel1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	panel1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);

	// button54
	button54 = new Button(L"按钮", 0, 0, 100, 100);
	button54->SetFontEx(__formFont, false);
	button54->BackColor = D2D1::ColorF(0.8039f, 0.7882f, 0.7882f, 1.f);
	button54->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	button54->BolderColor = D2D1::ColorF(0.5451f, 0.5373f, 0.5373f, 1.f);
	button54->Margin = Thickness(0.f, 0.f, 50.f, 0.f);

	// webBrowser52
	webBrowser52 = new WebBrowser(10, 10, 820, 494);
	webBrowser52->SetFontEx(__formFont, false);
	webBrowser52->BackColor = D2D1::ColorF(1.f, 1.f, 1.f, 1.f);
	webBrowser52->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	webBrowser52->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	webBrowser52->Margin = Thickness(10.f, 10.f, 10.f, 10.f);
	webBrowser52->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | AnchorStyles::Right | AnchorStyles::Bottom;

	// menu1
	menu1 = new Menu(0, 0, 860, 28);
	menu1->SetFontEx(__formFont, false);
	menu1->BackColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);
	menu1->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	menu1->BolderColor = D2D1::ColorF(0.f, 0.f, 0.f, 0.f);
	// Menu items
	while (menu1->Count > 0)
	{
		auto* it = (MenuItem*)menu1->operator[](menu1->Count - 1);
		menu1->RemoveControl(it);
		delete it;
	}
	auto* menu1_item1 = menu1->AddItem(L"文件");
	menu1_item1->Id = 1;
	auto* menu1_item2 = menu1_item1->AddSubItem(L"打开", 11);
	menu1_item2->Id = 11;
	auto* menu1_item3 = menu1_item1->AddSubItem(L"最近", 12);
	menu1_item3->Id = 12;
	menu1_item1->AddSeparator();
	auto* menu1_item4 = menu1_item1->AddSubItem(L"退出", 13);
	menu1_item4->Id = 13;
	auto* menu1_item5 = menu1->AddItem(L"设置");
	menu1_item5->Id = 2;
	auto* menu1_item6 = menu1_item5->AddSubItem(L"主题", 21);
	menu1_item6->Id = 21;
	auto* menu1_item7 = menu1_item6->AddSubItem(L"深色", 211);
	menu1_item7->Id = 211;
	auto* menu1_item8 = menu1_item6->AddSubItem(L"浅色", 212);
	menu1_item8->Id = 212;
	menu1_item5->AddSeparator();
	auto* menu1_item9 = menu1_item5->AddSubItem(L"重置", 22);
	menu1_item9->Id = 22;
	auto* menu1_item10 = menu1->AddItem(L"帮助");
	menu1_item10->Id = 3;
	auto* menu1_item11 = menu1_item10->AddSubItem(L"关于", 31);
	menu1_item11->Id = 31;

	// 绑定事件
	tabControl1->OnSelectedChanged += std::bind_front(&DemoWindow1::tabControl1_OnSelectedChanged, this);
	button1->OnMouseClick += std::bind_front(&DemoWindow1::button1_OnMouseClick, this);
	textBox1->OnTextChanged += std::bind_front(&DemoWindow1::textBox1_OnTextChanged, this);
	slider1->OnValueChanged += std::bind_front(&DemoWindow1::slider1_OnValueChanged, this);
	switch1->OnChecked += std::bind_front(&DemoWindow1::switch1_OnChecked, this);
	menu1->OnMenuCommand += std::bind_front(&DemoWindow1::menu1_OnMenuCommand, this);

	// 组装控件层级（包含布局容器）
	this->AddControl(tabControl1);
	tabPage1 = tabControl1->AddPage(L"基本控件");
	tabPage1->AddControl(label1);

	tabPage1->AddControl(button1);

	tabPage1->AddControl(textBox1);

	tabPage1->AddControl(passwordBox1);

	tabPage1->AddControl(richTextBox1);

	tabPage1->AddControl(checkBox1);

	tabPage1->AddControl(radioBox1);

	tabPage1->AddControl(comboBox1);

	tabPage1->AddControl(progressBar1);

	tabPage1->AddControl(slider1);

	tabPage1->AddControl(switch1);

	tabPage1->AddControl(gridView15);

	tabPage1->AddControl(treeView16);

	tabPage1->AddControl(pictureBox1);

	tabPage2 = tabControl1->AddPage(L"布局控件");
	tabPage2->AddControl(stackPanel18);
	stackPanel18->AddControl(button22);
	button22->HAlign = HorizontalAlignment::Left;
	button22->VAlign = VerticalAlignment::Top;

	stackPanel18->AddControl(button19);
	button19->HAlign = HorizontalAlignment::Left;
	button19->VAlign = VerticalAlignment::Top;

	stackPanel18->AddControl(button20);
	button20->HAlign = HorizontalAlignment::Left;
	button20->VAlign = VerticalAlignment::Top;

	stackPanel18->AddControl(label21);
	label21->HAlign = HorizontalAlignment::Left;
	label21->VAlign = VerticalAlignment::Top;

	stackPanel18->AddControl(label23);
	label23->HAlign = HorizontalAlignment::Left;
	label23->VAlign = VerticalAlignment::Top;

	stackPanel18->PerformLayout();

	tabPage2->AddControl(gridPanel24);
	gridPanel24->AddControl(button25);
	button25->GridRow = 0;
	button25->GridColumn = 0;
	button25->GridRowSpan = 1;
	button25->GridColumnSpan = 1;
	button25->HAlign = HorizontalAlignment::Stretch;
	button25->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button26);
	button26->GridRow = 0;
	button26->GridColumn = 1;
	button26->GridRowSpan = 1;
	button26->GridColumnSpan = 1;
	button26->HAlign = HorizontalAlignment::Stretch;
	button26->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button27);
	button27->GridRow = 0;
	button27->GridColumn = 2;
	button27->GridRowSpan = 1;
	button27->GridColumnSpan = 1;
	button27->HAlign = HorizontalAlignment::Stretch;
	button27->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button28);
	button28->GridRow = 0;
	button28->GridColumn = 3;
	button28->GridRowSpan = 1;
	button28->GridColumnSpan = 1;
	button28->HAlign = HorizontalAlignment::Stretch;
	button28->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button29);
	button29->GridRow = 1;
	button29->GridColumn = 0;
	button29->GridRowSpan = 1;
	button29->GridColumnSpan = 1;
	button29->HAlign = HorizontalAlignment::Stretch;
	button29->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button30);
	button30->GridRow = 1;
	button30->GridColumn = 1;
	button30->GridRowSpan = 1;
	button30->GridColumnSpan = 1;
	button30->HAlign = HorizontalAlignment::Stretch;
	button30->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button31);
	button31->GridRow = 1;
	button31->GridColumn = 2;
	button31->GridRowSpan = 1;
	button31->GridColumnSpan = 1;
	button31->HAlign = HorizontalAlignment::Stretch;
	button31->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button32);
	button32->GridRow = 1;
	button32->GridColumn = 3;
	button32->GridRowSpan = 1;
	button32->GridColumnSpan = 1;
	button32->HAlign = HorizontalAlignment::Stretch;
	button32->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button33);
	button33->GridRow = 2;
	button33->GridColumn = 0;
	button33->GridRowSpan = 1;
	button33->GridColumnSpan = 1;
	button33->HAlign = HorizontalAlignment::Stretch;
	button33->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button34);
	button34->GridRow = 3;
	button34->GridColumn = 0;
	button34->GridRowSpan = 1;
	button34->GridColumnSpan = 1;
	button34->HAlign = HorizontalAlignment::Stretch;
	button34->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button35);
	button35->GridRow = 2;
	button35->GridColumn = 1;
	button35->GridRowSpan = 1;
	button35->GridColumnSpan = 1;
	button35->HAlign = HorizontalAlignment::Stretch;
	button35->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button36);
	button36->GridRow = 3;
	button36->GridColumn = 1;
	button36->GridRowSpan = 1;
	button36->GridColumnSpan = 1;
	button36->HAlign = HorizontalAlignment::Stretch;
	button36->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button37);
	button37->GridRow = 2;
	button37->GridColumn = 2;
	button37->GridRowSpan = 1;
	button37->GridColumnSpan = 1;
	button37->HAlign = HorizontalAlignment::Stretch;
	button37->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button38);
	button38->GridRow = 3;
	button38->GridColumn = 2;
	button38->GridRowSpan = 1;
	button38->GridColumnSpan = 1;
	button38->HAlign = HorizontalAlignment::Stretch;
	button38->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button39);
	button39->GridRow = 2;
	button39->GridColumn = 3;
	button39->GridRowSpan = 1;
	button39->GridColumnSpan = 1;
	button39->HAlign = HorizontalAlignment::Stretch;
	button39->VAlign = VerticalAlignment::Stretch;

	gridPanel24->AddControl(button52);
	button52->GridRow = 3;
	button52->GridColumn = 3;
	button52->GridRowSpan = 1;
	button52->GridColumnSpan = 1;
	button52->HAlign = HorizontalAlignment::Stretch;
	button52->VAlign = VerticalAlignment::Stretch;

	gridPanel24->PerformLayout();

	tabPage2->AddControl(dockPanel41);
	dockPanel41->AddControl(button53);
	button53->DockPosition = Dock::Fill;

	dockPanel41->PerformLayout();

	tabPage2->AddControl(wrapPanel43);
	wrapPanel43->AddControl(button44);
	button44->HAlign = HorizontalAlignment::Left;
	button44->VAlign = VerticalAlignment::Top;

	wrapPanel43->AddControl(button46);
	button46->HAlign = HorizontalAlignment::Left;
	button46->VAlign = VerticalAlignment::Top;

	wrapPanel43->AddControl(button45);
	button45->HAlign = HorizontalAlignment::Left;
	button45->VAlign = VerticalAlignment::Top;

	wrapPanel43->AddControl(button47);
	button47->HAlign = HorizontalAlignment::Left;
	button47->VAlign = VerticalAlignment::Top;

	wrapPanel43->PerformLayout();

	tabPage2->AddControl(relativePanel48);
	relativePanel48->AddControl(button50);
	button50->Margin = Thickness(30.f, 64.f, 0.f, 0.f);

	relativePanel48->AddControl(button49);
	button49->Margin = Thickness(30.f, 34.f, 0.f, 0.f);

	relativePanel48->PerformLayout();

	tabPage2->AddControl(panel1);
	panel1->AddControl(button54);

	panel1->PerformLayout();

	tabPage3 = tabControl1->AddPage(L"WebBrowser");
	tabPage3->AddControl(webBrowser52);


	this->AddControl(menu1);

}

DemoWindow1::~DemoWindow1()
{
}

void DemoWindow1::tabControl1_OnSelectedChanged(Control* sender)
{
	(void)sender;
}

void DemoWindow1::button1_OnMouseClick(Control* sender, MouseEventArgs e)
{
	OpenFileDialog ofd;

	ofd.Filter = MakeDialogFilterStrring("图片文件", "*.jpg;*.png;*.bmp;*.webp");
	ofd.SupportMultiDottedExtensions = true;
	ofd.Title = "选择一个图片文件";
	if (ofd.ShowDialog(this->Handle) == DialogResult::OK)
	{
		if (pictureBox1->Image)
		{
			pictureBox1->Image->Release();
		}
		if (this->Image && this->Image != pictureBox1->Image)
		{
			this->Image->Release();
		}
		FileInfo file(ofd.SelectedPaths[0]);
		auto bytes = File::ReadAllBytes(ofd.SelectedPaths[0]);
		auto img = BitmapSource::FromBuffer(bytes.data(), bytes.size());
		this->Image = this->Render->CreateBitmap(img->GetWicBitmap());
		pictureBox1->SetImageEx(this->Image, false);
		this->Invalidate();
	}
}

void DemoWindow1::textBox1_OnTextChanged(Control* sender, std::wstring oldText, std::wstring newText)
{
	(void)sender; (void)oldText; (void)newText;
}

void DemoWindow1::slider1_OnValueChanged(Control* sender, float oldValue, float newValue)
{
	(void)sender;
}

void DemoWindow1::switch1_OnChecked(Control* sender)
{
	gridView15->Visible = sender->Checked;
}

void DemoWindow1::menu1_OnMenuCommand(Control* sender, int id)
{
	(void)sender;
	if (id == 13) {
		this->Close();
	}
}