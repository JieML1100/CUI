#include "DemoWindow.h"

#include "CustomControls.h"
#include "imgs.h"

#include <Button.h>
#include <ChartView.h>
#include <CheckBox.h>
#include <ComboBox.h>
#include <ContextMenu.h>
#include <Expander.h>
#include <FilterBar.h>
#include <GridView.h>
#include <KpiCard.h>
#include <ListView.h>
#include <MediaPlayer.h>
#include <Menu.h>
#include <MessageDialog.h>
#include <NavigationView.h>
#include <NumericUpDown.h>
#include <PagedGridView.h>
#include <PictureBox.h>
#include <ProgressBar.h>
#include <ProgressRing.h>
#include <PropertyGrid.h>
#include <RadioBox.h>
#include <Layout/RelativePanel.h>
#include <ReportView.h>
#include <Slider.h>
#include <StatusBar.h>
#include <Switch.h>
#include <TabControl.h>
#include <Taskbar.h>
#include <Toast.h>
#include <ToolBar.h>
#include <TreeView.h>
#include <WebBrowser.h>

#include <Utils.h>
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace
{
	std::wstring ExecutableDirectory()
	{
		std::wstring path(32768, L'\0');
		const auto length = GetModuleFileNameW(
			nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (!length || length >= path.size()) return std::filesystem::current_path().wstring();
		path.resize(length);
		return std::filesystem::path(path).parent_path().wstring();
	}

	std::shared_ptr<DesignerModel::RuntimeCustomControlRegistry>
	CreateCustomControlRegistry(std::wstring* outError)
	{
		auto registry =
			std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
		if (!registry->Register(
			L"urn:cui:test", L"GradientInput",
			[](const DesignerModel::DesignNode&)
			{
				return std::make_unique<CustomTextBox1>(L"", 0, 0, 360, 42);
			}, outError)) return {};
		if (!registry->Register(
			L"urn:cui:test", L"GradientLabel",
			[](const DesignerModel::DesignNode&)
			{
				return std::make_unique<CustomLabel1>(L"", 0, 0);
			}, outError)) return {};
		return registry;
	}

	std::wstring FileNameFromPath(const std::wstring& path)
	{
		return std::filesystem::path(path).filename().wstring();
	}

	std::wstring ToJsStringLiteral(const std::wstring& value)
	{
		std::wstring result = L"\"";
		for (const auto ch : value)
		{
			switch (ch)
			{
			case L'\\': result += L"\\\\"; break;
			case L'\"': result += L"\\\""; break;
			case L'\r': result += L"\\r"; break;
			case L'\n': result += L"\\n"; break;
			default: result.push_back(ch); break;
			}
		}
		result += L"\"";
		return result;
	}

	[[noreturn]] void ThrowRuntimeError(const std::wstring& message)
	{
		throw std::runtime_error(Convert::WStringToString(message));
	}
}

std::wstring DemoWindow::XamlFilePath()
{
	return (std::filesystem::path(ExecutableDirectory())
		/ L"DemoWindow.cui.xaml").wstring();
}

bool DemoWindow::ValidateXaml(std::wstring* outError)
{
	auto registry = CreateCustomControlRegistry(outError);
	if (!registry) return false;
	DesignerModel::XamlDocumentParseOptions options;
	options.CustomControlFactory = [registry](const DesignerModel::DesignNode& node)
	{
		return registry->Create(node);
	};
	DesignerModel::DesignDocument document;
	return DesignerModel::XamlDocumentParser::LoadFromFile(
		XamlFilePath(), document, options, outError);
}

template<typename T>
T* DemoWindow::RequireControl(const wchar_t* name)
{
	auto* control = dynamic_cast<T*>(
		_xamlSession.Document().FindControlByName(name));
	if (!control) ThrowRuntimeError(L"XAML 缺少控件或类型不匹配：" + std::wstring(name));
	return control;
}

DemoWindow::DemoWindow()
	: Form(L"CUI XAML Component Gallery", { 0, 0 }, { 1400, 800 })
{
	std::wstring error;
	_customControls = CreateCustomControlRegistry(&error);
	if (!_customControls) ThrowRuntimeError(error);
	RegisterXamlHandlers();
	MountXaml();
	ResolveControls();
	LoadImages();
	InitializeChrome();
	InitializeBasicPage();
	InitializeContainerPage();
	InitializeDataPage();
	InitializeAnalyticsPage();
	InitializeLayoutPage();
	InitializeSystemPage();
	InitializeWebPage();
	InitializeMediaPage();
	(void)SetDefaultButton(_basicButton);
	UpdateProgress(0.25f);
}

DemoWindow::~DemoWindow()
{
	if (_notify) _notify->HideNotifyIcon();
}

void DemoWindow::MountXaml()
{
	DesignerModel::RuntimeDocumentSessionMountOptions options;
	options.CustomControls = _customControls;
	// CUITest deliberately uses an external file, but keeps watching disabled:
	// the sample compares construction modes without introducing editor timing.
	options.WatchFile = false;
	std::wstring error;
	if (!_xamlSession.MountFile(XamlFilePath(), *this, options, &error))
		ThrowRuntimeError(L"加载 DemoWindow.cui.xaml 失败：" + error);
}

void DemoWindow::RegisterXamlHandlers()
{
	auto& registry = _xamlSession.EventHandlers();
	std::wstring error;
	auto require = [&](bool result)
	{
		if (!result) ThrowRuntimeError(L"注册 XAML 事件失败：" + error);
	};

	require(registry.RegisterForm(L"HandleShown", L"OnShown", &Form::OnShown,
		[this](Form* sender) { HandleShown(sender); }, &error));
	require(registry.RegisterForm(L"HandleClosing", L"OnClose", &Form::OnClosing,
		[this](Form* sender, bool& canceled) { HandleClosing(sender, canceled); }, &error));
	require(registry.RegisterControl(L"HandleMenuCommand", UIClass::UI_Menu,
		L"OnMenuCommand", &Menu::OnMenuCommand,
		[this](Control* sender, int id) { HandleMenuCommand(sender, id); }, &error));
	require(registry.RegisterControl(L"HandleGlobalProgress", UIClass::UI_Slider,
		L"OnValueChanged", &Slider::OnValueChanged,
		[this](Control* sender, float oldValue, float newValue)
		{ HandleGlobalProgress(sender, oldValue, newValue); }, &error));
	require(registry.RegisterControl(L"HandleMouseWheel", UIClass::UI_Label,
		L"OnMouseWheel", &Control::OnMouseWheel,
		[this](Control* sender, MouseEventArgs e) { HandleMouseWheel(sender, e); }, &error));

	auto registerClick = [&](const wchar_t* name, UIClass type,
		auto callback)
	{
		require(registry.RegisterControl(name, type, L"OnMouseClick",
			&Control::OnMouseClick, std::move(callback), &error));
	};
	registerClick(L"HandleBasicClick", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleBasicClick(sender, e); });
	registerClick(L"HandleDocsLink", UIClass::UI_LinkLabel,
		[this](Control* sender, MouseEventArgs e) { HandleDocsLink(sender, e); });
	registerClick(L"HandleOpenImage", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleOpenImage(sender, e); });
	registerClick(L"HandleChartKind", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleChartKind(sender, e); });
	registerClick(L"HandleFarButton", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleFarButton(sender, e); });
	registerClick(L"HandleSystemAction", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleSystemAction(sender, e); });
	registerClick(L"HandleInvokeWeb", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleInvokeWeb(sender, e); });
	registerClick(L"HandleMediaCommand", UIClass::UI_Button,
		[this](Control* sender, MouseEventArgs e) { HandleMediaCommand(sender, e); });

	auto registerChecked = [&](const wchar_t* name, UIClass type, auto callback)
	{
		require(registry.RegisterControl(name, type, L"OnChecked",
			&Control::OnChecked, std::move(callback), &error));
	};
	registerChecked(L"HandleEnableInput", UIClass::UI_CheckBox,
		[this](Control* sender) { HandleEnableInput(sender); });
	registerChecked(L"HandleRadio", UIClass::UI_RadioBox,
		[this](Control* sender) { HandleRadio(sender); });
	registerChecked(L"HandlePictureVisibility", UIClass::UI_Switch,
		[this](Control* sender) { HandlePictureVisibility(sender); });
	registerChecked(L"HandleGridEnabled", UIClass::UI_Switch,
		[this](Control* sender) { HandleGridEnabled(sender); });
	registerChecked(L"HandleGridVisible", UIClass::UI_Switch,
		[this](Control* sender) { HandleGridVisible(sender); });
	registerChecked(L"HandleMediaLoop", UIClass::UI_CheckBox,
		[this](Control* sender) { HandleMediaLoop(sender); });

	require(registry.RegisterControl(L"HandleComboSelection", UIClass::UI_ComboBox,
		L"OnSelectionChanged", &ComboBox::OnSelectionChanged,
		[this](Control* sender) { HandleComboSelection(static_cast<ComboBox*>(sender)); }, &error));
	require(registry.RegisterControl(L"HandleNumericValue", UIClass::UI_NumericUpDown,
		L"OnValueChanged", &NumericUpDown::OnValueChanged,
		[this](NumericUpDown* sender, double oldValue, double newValue)
		{ HandleNumericValue(sender, oldValue, newValue); }, &error));
	require(registry.RegisterControl(L"HandleExpander", UIClass::UI_Expander,
		L"OnExpandedChanged", &Expander::OnExpandedChanged,
		[this](Expander* sender, bool expanded) { HandleExpander(sender, expanded); }, &error));
	require(registry.RegisterControl(L"HandleDropImage", UIClass::UI_PictureBox,
		L"OnDropFile", &Control::OnDropFile,
		[this](Control* sender, std::vector<std::wstring> files)
		{ HandleDropImage(sender, std::move(files)); }, &error));

	auto registerList = [&](UIClass type)
	{
		require(registry.RegisterControl(L"HandleListItem", type,
			L"OnItemClick", &ListView::OnItemClick,
			[this](ListView* sender, int index) { HandleListItem(sender, index); }, &error));
	};
	registerList(UIClass::UI_ListBox);
	registerList(UIClass::UI_ListView);
	require(registry.RegisterControl(L"HandlePropertyValue", UIClass::UI_PropertyGrid,
		L"OnValueChanged", &PropertyGridView::OnValueChanged,
		[this](PropertyGridView* sender, int index,
			std::wstring oldValue, std::wstring newValue)
		{ HandlePropertyValue(sender, index, std::move(oldValue), std::move(newValue)); }, &error));
	require(registry.RegisterControl(L"HandleFilterApply", UIClass::UI_FilterBar,
		L"OnApply", &FilterBar::OnApply,
		[this](FilterBar* sender) { HandleFilterApply(sender); }, &error));
	require(registry.RegisterControl(L"HandleFilterReset", UIClass::UI_FilterBar,
		L"OnReset", &FilterBar::OnReset,
		[this](FilterBar* sender) { HandleFilterReset(sender); }, &error));
	require(registry.RegisterControl(L"HandleKpiClick", UIClass::UI_KpiCard,
		L"OnCardClick", &KpiCard::OnCardClick,
		[this](KpiCard* sender) { HandleKpiClick(sender); }, &error));
	require(registry.RegisterControl(L"HandleChartPoint", UIClass::UI_ChartView,
		L"OnPointClick", &ChartView::OnPointClick,
		[this](ChartView* sender, int series, int point)
		{ HandleChartPoint(sender, series, point); }, &error));
	require(registry.RegisterControl(L"HandleReportRow", UIClass::UI_ReportView,
		L"OnRowClick", &ReportView::OnRowClick,
		[this](ReportView* sender, int row) { HandleReportRow(sender, row); }, &error));
	require(registry.RegisterControl(L"HandleSystemSurfaceMouseUp", UIClass::UI_Panel,
		L"OnMouseUp", &Control::OnMouseUp,
		[this](Control* sender, MouseEventArgs e)
		{ HandleSystemSurfaceMouseUp(sender, e); }, &error));
	require(registry.RegisterControl(L"HandleToastClick", UIClass::UI_ToastHost,
		L"OnToastClick", &ToastHost::OnToastClick,
		[this](ToastHost* sender, int index) { HandleToastClick(sender, index); }, &error));

	auto registerSlider = [&](const wchar_t* name, auto callback)
	{
		require(registry.RegisterControl(name, UIClass::UI_Slider,
			L"OnValueChanged", &Slider::OnValueChanged,
			std::move(callback), &error));
	};
	registerSlider(L"HandleMediaVolume",
		[this](Control* sender, float oldValue, float newValue)
		{ HandleMediaVolume(sender, oldValue, newValue); });
	registerSlider(L"HandleMediaSpeed",
		[this](Control* sender, float oldValue, float newValue)
		{ HandleMediaSpeed(sender, oldValue, newValue); });
	registerSlider(L"HandleMediaSeek",
		[this](Control* sender, float oldValue, float newValue)
		{ HandleMediaSeek(sender, oldValue, newValue); });

	require(registry.RegisterControl(L"HandleMediaOpened", UIClass::UI_MediaPlayer,
		L"OnMediaOpened", &MediaPlayer::OnMediaOpened,
		[this](Control* sender) { HandleMediaOpened(static_cast<MediaPlayer*>(sender)); }, &error));
	require(registry.RegisterControl(L"HandleMediaEnded", UIClass::UI_MediaPlayer,
		L"OnMediaEnded", &MediaPlayer::OnMediaEnded,
		[this](Control* sender) { HandleMediaEnded(static_cast<MediaPlayer*>(sender)); }, &error));
	require(registry.RegisterControl(L"HandleMediaFailed", UIClass::UI_MediaPlayer,
		L"OnMediaFailed", &MediaPlayer::OnMediaFailed,
		[this](Control* sender) { HandleMediaFailed(static_cast<MediaPlayer*>(sender)); }, &error));
	require(registry.RegisterControl(L"HandleMediaPosition", UIClass::UI_MediaPlayer,
		L"OnPositionChanged", &MediaPlayer::OnPositionChanged,
		[this](Control* sender, double position)
		{ HandleMediaPosition(static_cast<MediaPlayer*>(sender), position); }, &error));
}

void DemoWindow::ResolveControls()
{
	_menu = RequireControl<Menu>(L"mainMenu");
	_toolBar = RequireControl<ToolBar>(L"mainToolBar");
	_statusBar = RequireControl<StatusBar>(L"mainStatusBar");
	_globalProgress = RequireControl<Slider>(L"globalProgress");
	_statusText = RequireControl<Label>(L"statusText");
	_tabs = RequireControl<TabControl>(L"mainTabs");
	_basicButton = RequireControl<Button>(L"basicButton");
	_radioA = RequireControl<RadioBox>(L"radioA");
	_radioB = RequireControl<RadioBox>(L"radioB");
	_picture = RequireControl<PictureBox>(L"demoPicture");
	_progress = RequireControl<ProgressBar>(L"demoProgress");
	_progressRing = RequireControl<ProgressRing>(L"progressRing");
	_pagedGrid = RequireControl<PagedGridView>(L"pagedGrid");
	_propertyGrid = RequireControl<PropertyGridView>(L"propertyGrid");
	_filter = RequireControl<FilterBar>(L"analyticsFilter");
	_kpiRevenue = RequireControl<KpiCard>(L"kpiRevenue");
	_kpiDeals = RequireControl<KpiCard>(L"kpiDeals");
	_kpiMargin = RequireControl<KpiCard>(L"kpiMargin");
	_chart = RequireControl<ChartView>(L"salesChart");
	_report = RequireControl<ReportView>(L"salesReport");
	_toast = RequireControl<ToastHost>(L"toastHost");
	_web = RequireControl<WebBrowser>(L"webBrowser");
	_media = RequireControl<MediaPlayer>(L"mediaPlayer");
	_mediaProgress = RequireControl<Slider>(L"mediaProgress");
	_mediaTime = RequireControl<Label>(L"mediaTime");
	_mediaSpeedText = RequireControl<Label>(L"mediaSpeedText");
}

void DemoWindow::LoadImages()
{
	const char* images[] = { _0_ico, _1_ico, _2_ico, _3_ico, _4_ico,
		_5_ico, _6_ico, _7_ico, _8_ico, _9_ico };
	for (size_t i = 0; i < std::size(images); ++i)
		_images[i] = D2DGraphics::ToBitmapFromSvg(images[i]);
	const char* icons[] = { icon0, icon1, icon2, icon3, icon4 };
	for (size_t i = 0; i < std::size(icons); ++i)
		_icons[i] = D2DGraphics::ToBitmapFromSvg(icons[i]);
}

void DemoWindow::InitializeChrome()
{
	auto file = _menu->AddItem(L"文件");
	file->AddSubItem(L"打开", 101);
	file->AddSeparator();
	file->AddSubItem(L"退出", 102);
	auto help = _menu->AddItem(L"帮助");
	help->AddSubItem(L"关于 XAML 模式", 201);

	auto addPageButton = [&](const wchar_t* text, int page)
	{
		auto* button = _toolBar->AddTextButton(text, 88);
		button->OnMouseClick += [this, page, text](Control*, MouseEventArgs)
		{
			_tabs->SelectPage(page);
			UpdateStatus(std::wstring(L"ToolBar: ") + text);
		};
	};
	addPageButton(L"基础", 0);
	addPageButton(L"数据", 2);
	addPageButton(L"可视化", 3);
	addPageButton(L"系统", 5);
	_toolBar->AddSeparator();
	for (int i = 0; i < 3; ++i)
	{
		auto* button = _toolBar->AddIconButton(_icons[i], 30);
		button->Tag = i;
		button->OnMouseClick += [this](Control* sender, MouseEventArgs)
		{
			UpdateStatus(StringHelper::Format(
				L"ToolBar icon %d", static_cast<int>(sender->Tag) + 1));
		};
	}
	_statusBar->AddPart(L"XAML ready", -1);
	_statusBar->AddPart(L"DemoWindow.cui.xaml", 250);
}

void DemoWindow::InitializeBasicPage()
{
	auto* combo = RequireControl<ComboBox>(L"basicCombo");
	combo->Items = { L"动态 XAML", L"静态生成 C++", L"手写 C++ 控件树" };
	combo->SelectedIndex = 0;
	combo->Text = combo->Items[0];
}

void DemoWindow::InitializeContainerPage()
{
	_picture->SetImageEx(_images[5]);
	auto* sideBar = RequireControl<SideBar>(L"sideBar");
	sideBar->AddHeader(L"工作区");
	sideBar->AddItem(L"概览", L"overview", _icons[0]);
	sideBar->Items.back().BadgeText = L"3";
	sideBar->AddItem(L"资源", L"assets", _icons[1]);
	sideBar->AddSeparator();
	sideBar->AddItem(L"设置", L"settings", _icons[2]);
	sideBar->SelectItem(1);
	sideBar->OnItemClick += [this](NavigationView* sender, int index)
	{
		if (index >= 0 && index < static_cast<int>(sender->Items.size()))
			UpdateStatus(L"SideBar: " + sender->Items[index].Text);
	};
	auto* breadcrumb = RequireControl<BreadcrumbBar>(L"breadcrumb");
	breadcrumb->AddItem(L"应用");
	breadcrumb->AddItem(L"资源");
	breadcrumb->AddItem(L"详情");
	breadcrumb->SelectItem(2);
}

void DemoWindow::InitializeDataPage()
{
	auto* tree = RequireControl<TreeView>(L"demoTree");
	for (int i = 0; i < 4; ++i)
	{
		auto* parent = new TreeNode(StringHelper::Format(L"node%d", i), _images[i]);
		parent->Expand = true;
		tree->Root->Children.push_back(parent);
		for (int j = 0; j < 6; ++j)
			parent->Children.push_back(new TreeNode(
				StringHelper::Format(L"node%d-%d", i, j), _images[(i + j) % 10]));
	}

	auto* listBox = RequireControl<ListBox>(L"demoListBox");
	for (const auto* text : { L"全部任务", L"今天", L"进行中", L"已完成" })
		listBox->AddItem(ListViewItem(text));
	listBox->SelectItem(0);

	auto* list = RequireControl<ListView>(L"demoList");
	list->AddColumn(ListViewColumn(L"Name", 170));
	list->AddColumn(ListViewColumn(L"State", 120));
	for (int i = 0; i < 40; ++i)
	{
		ListViewItem item(StringHelper::Format(L"List item %02d", i + 1),
			i % 3 == 0 ? L"Ready" : L"Queued");
		item.Image = _images[i % 10];
		item.Checked = i % 5 == 0;
		item.SubItems.push_back(item.SubText);
		list->AddItem(item);
	}

	_pagedGrid->AddColumn(GridViewColumn(L"Image", 70, ColumnType::Image));
	GridViewColumn combo(L"State", 110, ColumnType::ComboBox);
	combo.ComboBoxItems = { L"Ready", L"Running", L"Done" };
	_pagedGrid->AddColumn(combo);
	_pagedGrid->AddColumn(GridViewColumn(L"Check", 70, ColumnType::Check));
	_pagedGrid->AddColumn(GridViewColumn(L"Value", 150, ColumnType::Text, true));
	_pagedGrid->AddColumn(GridViewColumn(L"Link", 100, ColumnType::LinkedText));
	for (int i = 0; i < 500; ++i)
	{
		GridViewRow row;
		row.Cells = { _images[i % 10], L"Ready", i % 2 == 0,
			std::to_wstring(Random::Next()), L"Open" };
		_pagedGrid->AddRow(row);
	}
	_pagedGrid->RefreshPage();
	_pagedGrid->OnPageChanged += [this](PagedGridView*, int, int page)
	{
		UpdateStatus(StringHelper::Format(L"PagedGridView: page %d", page + 1));
	};

	_propertyGrid->AddProperty(
		L"Appearance", L"Title", L"XAML grid settings", PropertyGridValueType::Text);
	_propertyGrid->AddProperty(
		L"Appearance", L"Enabled", L"True", PropertyGridValueType::Bool);
	PropertyGridItem density(
		L"Behavior", L"Density", L"Comfortable", PropertyGridValueType::Enum);
	density.Options = { L"Compact", L"Comfortable", L"Roomy" };
	_propertyGrid->AddItem(density);
	_propertyGrid->AddProperty(
		L"Theme", L"Accent", L"#2F7DF0", PropertyGridValueType::Color);
}

void DemoWindow::InitializeAnalyticsPage()
{
	_filter->Placeholder = L"搜索客户、区域或阶段";
	_filter->AddItem(FilterBarItem(L"已成交", L"done", true));
	_filter->AddItem(FilterBarItem(L"合同中", L"contract"));
	_filter->AddItem(FilterBarItem(L"跟进中", L"follow"));
	_filter->AddItem(FilterBarItem(L"高毛利", L"margin"));
	_filter->OnQueryChanged += [this](FilterBar*, const std::wstring& query)
	{
		UpdateStatus(query.empty() ? L"FilterBar: query cleared" : L"FilterBar: " + query);
	};
	_kpiRevenue->Title = L"成交额";
	_kpiRevenue->Value = L"1,870.5";
	_kpiRevenue->Unit = L"万";
	_kpiRevenue->TrendText = L"+18.4%";
	_kpiRevenue->Caption = L"较上期";
	_kpiRevenue->TrendDirection = KpiTrendDirection::Up;
	_kpiRevenue->SetSparkline({ 118, 134, 126, 156, 178, 172, 191, 218 });
	_kpiDeals->Title = L"成交客户";
	_kpiDeals->Value = L"128";
	_kpiDeals->TrendText = L"+9";
	_kpiDeals->Caption = L"本月新增";
	_kpiDeals->TrendDirection = KpiTrendDirection::Up;
	_kpiDeals->SetSparkline({ 56, 64, 72, 79, 88, 96, 113, 128 });
	_kpiMargin->Title = L"平均毛利率";
	_kpiMargin->Value = L"29.8";
	_kpiMargin->Unit = L"%";
	_kpiMargin->TrendText = L"-1.2%";
	_kpiMargin->Caption = L"需关注";
	_kpiMargin->TrendDirection = KpiTrendDirection::Down;
	_kpiMargin->SetSparkline({ 34, 32, 31, 30, 31, 29, 28, 29.8 });

	_chart->Title = L"成交趋势";
	_chart->Subtitle = L"点击数据点查看明细";
	std::vector<std::wstring> months = {
		L"1月", L"2月", L"3月", L"4月", L"5月", L"6月", L"7月", L"8月" };
	ChartSeries retail(L"零售", D2D1::ColorF(0.17f, 0.49f, 0.96f, 0.95f));
	ChartSeries enterprise(L"企业", D2D1::ColorF(0.10f, 0.68f, 0.55f, 0.95f));
	ChartSeries channel(L"渠道", D2D1::ColorF(0.94f, 0.53f, 0.18f, 0.95f));
	const double retailValues[] = { 118, 134.5, 126.2, 156.8, 178.4, 172, 191.3, 218.5 };
	const double enterpriseValues[] = { 92.4, 108, 131.8, 139, 151.2, 169.5, 182.8, 197 };
	const double channelValues[] = { 66, 72.5, 84, 90.4, 96, 104.3, 112, 128.6 };
	for (size_t i = 0; i < months.size(); ++i)
	{
		retail.Points.emplace_back(months[i], retailValues[i]);
		enterprise.Points.emplace_back(months[i], enterpriseValues[i]);
		channel.Points.emplace_back(months[i], channelValues[i]);
	}
	_chart->AddSeries(retail);
	_chart->AddSeries(enterprise);
	_chart->AddSeries(channel);

	_report->Title = L"成交报表";
	_report->Subtitle = L"表头排序与分组折叠";
	_report->FooterText = L"ReportView · runtime data";
	_report->AddColumn(ReportColumn(L"客户", 150));
	_report->AddColumn(ReportColumn(L"区域", 90));
	_report->AddColumn(ReportColumn(L"阶段", 90));
	_report->AddColumn(ReportColumn(L"成交额", 100, ReportCellAlign::Right));
	_report->AddColumn(ReportColumn(L"毛利率", 84, ReportCellAlign::Right));
	_report->AddGroup(L"华东区域");
	_report->AddRow(ReportRow({ L"上海云舟", L"华东", L"已成交", L"312.4", L"31%" }));
	_report->AddRow(ReportRow({ L"杭州数擎", L"华东", L"合同中", L"228.6", L"28%" }));
	_report->AddSummary(L"华东小计", { L"华东小计", L"", L"", L"541.0", L"30%" });
	_report->AddGroup(L"华南区域");
	_report->AddRow(ReportRow({ L"深圳星河", L"华南", L"已成交", L"276.8", L"29%" }));
	_report->AddRow(ReportRow({ L"广州远航", L"华南", L"跟进中", L"162.5", L"25%" }));
	_report->AddSummary(L"华南小计", { L"华南小计", L"", L"", L"439.3", L"27%" });
}

void DemoWindow::InitializeLayoutPage()
{
	auto* panel = RequireControl<RelativePanel>(L"demoRelative");
	auto* button = RequireControl<Button>(L"relativeCenter");
	RelativeConstraints constraints;
	constraints.CenterHorizontal = true;
	constraints.CenterVertical = true;
	panel->SetConstraints(button, constraints);
}

void DemoWindow::InitializeSystemPage()
{
	_taskbar = std::make_unique<Taskbar>(Handle);
	_notify = std::make_unique<NotifyIcon>();
	(void)_notify->TryInitialize(Handle, 1);
	_notify->SetIcon(LoadIcon(nullptr, IDI_APPLICATION));
	_notify->SetToolTip(L"CUI XAML Demo");
	_notify->AddMenuItem(NotifyIconMenuItem(L"显示窗口", 1));
	_notify->AddMenuSeparator();
	_notify->AddMenuItem(NotifyIconMenuItem(L"退出", 3));
	_notify->OnNotifyIconMenuClick += [](NotifyIcon* sender, int id)
	{
		if (id == 1) ShowWindow(sender->hWnd, SW_SHOWNORMAL);
		else if (id == 3) PostMessage(sender->hWnd, WM_CLOSE, 0, 0);
	};
	(void)_notify->TryShow();

	_systemContextMenu = Add<ContextMenu>();
	_systemContextMenu->AddItem(L"新建项目", 1001);
	_systemContextMenu->AddItem(L"刷新视图", 1002);
	_systemContextMenu->AddSeparator();
	auto more = _systemContextMenu->AddItem(L"更多", 0);
	more->AddSubItem(L"复制信息", 1003);
	more->AddSubItem(L"关于此页", 1004);
	_systemContextMenu->OnMenuCommand += [this](Control* sender, int id)
	{ HandleSystemContextMenu(sender, id); };
	_toast->ShowToast(L"CUI XAML", L"ToastHost 控件由 XAML 创建。", ToastKind::Info, 5200);
}

void DemoWindow::InitializeWebPage()
{
	_web->RegisterJsInvokeHandler(L"native.echo", [](const std::wstring& payload)
	{
		return L"echo: " + payload;
	});
	_web->RegisterJsInvokeHandler(L"native.time", [](const std::wstring&)
	{
		SYSTEMTIME time{};
		GetLocalTime(&time);
		return StringHelper::Format(L"%02d:%02d:%02d", time.wHour, time.wMinute, time.wSecond);
	});
	_web->SetHtml(
		LR"html(<!doctype html><html><head><meta charset='utf-8'>
<style>body{font-family:Segoe UI;padding:22px;background:#20252d;color:#eef2f6}
button{padding:9px 14px;border-radius:7px;border:1px solid #5f6b7a;background:#2f7df0;color:white}
.box{margin-top:14px;padding:14px;background:#303641;border-radius:8px}</style></head>
<body><h2>CUI WebBrowser hosted by dynamic XAML</h2>
<p>控件布局来自 <code>DemoWindow.cui.xaml</code>，HTML 和 JS bridge 由 C++ 注入。</p>
<button onclick="window.CUI.invoke('native.time','').then(x=>out.textContent=x)">JS → C++ 获取时间</button>
<div class='box'>输出：<span id='out'>(none)</span></div>
<div class='box'>C++ → JS：<span id='native'>(none)</span></div>
<script>window.setFromNative=x=>(native.textContent=String(x),'ok')</script></body></html>)html");
}

void DemoWindow::InitializeMediaPage()
{
	_media->Volume = 0.8;
}

void DemoWindow::UpdateStatus(const std::wstring& text)
{
	if (_statusText)
	{
		_statusText->Text = text;
		_statusText->InvalidateVisual();
	}
	if (_statusBar)
	{
		_statusBar->SetPartText(0, text);
		_statusBar->InvalidateVisual();
	}
}

void DemoWindow::UpdateProgress(float value01)
{
	value01 = std::clamp(value01, 0.0f, 1.0f);
	_progress->PercentageValue = value01;
	_progressRing->PercentageValue = value01;
	if (_taskbar)
		(void)_taskbar->TrySetValue(
			static_cast<ULONGLONG>(value01 * 1000.0f), 1000);
}

void DemoWindow::LoadPicture(const std::wstring& path)
{
	if (path.empty()) return;
	const auto extension = StringHelper::ToLower(
		Convert::WStringToString(std::filesystem::path(path).extension().wstring()));
	_picture->Image = nullptr;
	if (extension == ".svg")
	{
		const auto svg = File::ReadAllText(Convert::WStringToString(path));
		_picture->SetImageEx(D2DGraphics::ToBitmapFromSvg(svg.c_str()));
	}
	else
		_picture->SetImageEx(BitmapSource::FromFile(path));
	UpdateStatus(L"PictureBox: " + FileNameFromPath(path));
	Invalidate();
}

void DemoWindow::HandleShown(Form*)
{
	UpdateStatus(L"XAML 已挂载：视觉树来自 DemoWindow.cui.xaml");
}

void DemoWindow::HandleClosing(Form* sender, bool& canceled)
{
	const auto result = MessageDialog::Show(
		L"确认", L"是否关闭 CUI XAML 示例？",
		MessageDialogButtons::YesNo, MessageDialogIcon::Question, sender->Handle);
	canceled = result != MessageDialogResult::Yes;
}

void DemoWindow::HandleMenuCommand(Control*, int id)
{
	if (id == 101) UpdateStatus(L"Menu: 文件 -> 打开");
	else if (id == 102) Close();
	else if (id == 201)
		MessageDialog::Show(L"关于", L"这个窗口的控件树由 DemoWindow.cui.xaml 动态构造。",
			MessageDialogButtons::OK, MessageDialogIcon::Info, Handle);
}

void DemoWindow::HandleGlobalProgress(Control*, float, float value)
{
	UpdateProgress(value / 1000.0f);
	UpdateStatus(StringHelper::Format(L"XAML Slider Value=%.0f", value));
}

void DemoWindow::HandleMouseWheel(Control*, MouseEventArgs e)
{
	UpdateStatus(StringHelper::Format(L"MouseWheel Delta=%d", e.Delta));
}

void DemoWindow::HandleBasicClick(Control* sender, MouseEventArgs)
{
	sender->Text = StringHelper::Format(L"点击计数 [%d]", ++sender->Tag);
	sender->InvalidateVisual();
	UpdateStatus(L"Button.Click -> HandleBasicClick");
}

void DemoWindow::HandleEnableInput(Control* sender)
{
	auto* target = RequireControl<Control>(L"nameInput");
	target->Enable = static_cast<CheckBox*>(sender)->Checked;
	target->InvalidateVisual();
}

void DemoWindow::HandleRadio(Control* sender)
{
	if (sender == _radioA && _radioA->Checked) _radioB->Checked = false;
	if (sender == _radioB && _radioB->Checked) _radioA->Checked = false;
	UpdateStatus(sender == _radioA ? L"Radio: A" : L"Radio: B");
}

void DemoWindow::HandleComboSelection(ComboBox* sender)
{
	UpdateStatus(L"ComboBox: " + sender->Text);
}

void DemoWindow::HandleNumericValue(NumericUpDown*, double, double value)
{
	UpdateStatus(StringHelper::Format(L"NumericUpDown: %.0f", value));
}

void DemoWindow::HandleDocsLink(Control*, MouseEventArgs)
{
	UpdateStatus(L"CUI XAML = DesignDocument 前端 + RuntimeDocument 材质化");
}

void DemoWindow::HandleExpander(Expander*, bool expanded)
{
	UpdateStatus(expanded ? L"Expander: Expanded" : L"Expander: Collapsed");
}

void DemoWindow::HandleOpenImage(Control*, MouseEventArgs)
{
	OpenFileDialog dialog;
	dialog.Filter = MakeDialogFilterStrring(
		"图片文件", "*.jpg;*.jpeg;*.png;*.bmp;*.svg;*.webp");
	dialog.SupportMultiDottedExtensions = true;
	dialog.Title = "选择一个图片文件";
	if (dialog.ShowDialog(Handle) == DialogResult::OK && !dialog.SelectedPaths.empty())
		LoadPicture(Convert::StringToWString(dialog.SelectedPaths[0]));
}

void DemoWindow::HandleDropImage(Control*, std::vector<std::wstring> files)
{
	if (!files.empty()) LoadPicture(files.front());
}

void DemoWindow::HandlePictureVisibility(Control* sender)
{
	_picture->Visible = static_cast<Switch*>(sender)->Checked;
	UpdateStatus(_picture->Visible ? L"PictureBox: Visible" : L"PictureBox: Hidden");
}

void DemoWindow::HandleListItem(ListView* sender, int index)
{
	if (index >= 0 && index < static_cast<int>(sender->Items.size()))
		UpdateStatus(L"List: " + sender->Items[index].Text);
}

void DemoWindow::HandleGridEnabled(Control* sender)
{
	_pagedGrid->Enable = static_cast<Switch*>(sender)->Checked;
	UpdateStatus(_pagedGrid->Enable ? L"PagedGrid: Enable" : L"PagedGrid: Disable");
}

void DemoWindow::HandleGridVisible(Control* sender)
{
	_pagedGrid->Visible = static_cast<Switch*>(sender)->Checked;
	UpdateStatus(_pagedGrid->Visible ? L"PagedGrid: Visible" : L"PagedGrid: Hidden");
}

void DemoWindow::HandlePropertyValue(PropertyGridView* sender, int index,
	std::wstring, std::wstring newValue)
{
	if (index >= 0 && index < static_cast<int>(sender->Items.size()))
		UpdateStatus(L"PropertyGrid: " + sender->Items[index].Name + L" = " + newValue);
}

void DemoWindow::HandleFilterApply(FilterBar* sender)
{
	UpdateStatus(StringHelper::Format(
		L"FilterBar: apply %d filters", static_cast<int>(sender->GetSelectedValues().size())));
}

void DemoWindow::HandleFilterReset(FilterBar*)
{
	UpdateStatus(L"FilterBar: reset");
}

void DemoWindow::HandleKpiClick(KpiCard* sender)
{
	UpdateStatus(L"KpiCard: " + sender->Title);
}

void DemoWindow::HandleChartKind(Control* sender, MouseEventArgs)
{
	if (sender == RequireControl<Control>(L"chartBar")) _chart->ChartKind = ChartViewKind::Bar;
	else if (sender == RequireControl<Control>(L"chartPie")) _chart->ChartKind = ChartViewKind::Pie;
	else _chart->ChartKind = ChartViewKind::Line;
	_chart->ResetView();
	UpdateStatus(L"ChartView: kind changed from XAML button");
}

void DemoWindow::HandleChartPoint(ChartView* sender, int series, int point)
{
	if (series < 0 || series >= static_cast<int>(sender->Series.size())) return;
	if (point < 0 || point >= static_cast<int>(sender->Series[series].Points.size())) return;
	UpdateStatus(StringHelper::Format(L"Chart: %s / %s = %.1f",
		sender->Series[series].Name.c_str(),
		sender->Series[series].Points[point].Label.c_str(),
		sender->Series[series].Points[point].Value));
}

void DemoWindow::HandleReportRow(ReportView* sender, int row)
{
	if (row >= 0 && row < static_cast<int>(sender->Rows.size())
		&& !sender->Rows[row].Cells.empty())
		UpdateStatus(L"ReportView: " + sender->Rows[row].Cells.front());
}

void DemoWindow::HandleFarButton(Control*, MouseEventArgs)
{
	UpdateStatus(L"ScrollView: reached far XAML child");
}

void DemoWindow::HandleSystemAction(Control* sender, MouseEventArgs)
{
	if (sender == RequireControl<Control>(L"notifyToggle"))
	{
		if (_notify->IsVisible()) (void)_notify->TryHide();
		else (void)_notify->TryShow();
		UpdateStatus(_notify->IsVisible() ? L"NotifyIcon: Show" : L"NotifyIcon: Hide");
	}
	else if (sender == RequireControl<Control>(L"notifyBalloon"))
	{
		(void)_notify->TryShowBalloonTip(
			L"CUI XAML", L"按钮来自动态 XAML。", 3000, NIIF_INFO);
	}
	else if (sender == RequireControl<Control>(L"showDialog"))
	{
		(void)MessageDialog::Show(L"CUI MessageDialog",
			L"这个按钮及其布局来自 XAML，调用对话框是 C++ 业务行为。",
			MessageDialogButtons::OK, MessageDialogIcon::Info, Handle);
	}
	else
		_toast->ShowToast(L"CUI XAML", L"运行时业务向 XAML ToastHost 写入通知。", ToastKind::Success);
}

void DemoWindow::HandleSystemSurfaceMouseUp(Control* sender, MouseEventArgs e)
{
	if (e.Buttons != MouseButtons::Right || !_systemContextMenu) return;
	_systemContextMenu->ShowAt(sender, e.X, e.Y);
	UpdateStatus(L"ContextMenu: shown from XAML Panel.OnMouseUp");
}

void DemoWindow::HandleToastClick(ToastHost*, int index)
{
	UpdateStatus(StringHelper::Format(L"ToastHost: click #%d", index));
}

void DemoWindow::HandleInvokeWeb(Control*, MouseEventArgs)
{
	SYSTEMTIME time{};
	GetLocalTime(&time);
	const auto text = StringHelper::Format(
		L"from C++ at %02d:%02d:%02d", time.wHour, time.wMinute, time.wSecond);
	_web->ExecuteScriptAsync(L"window.setFromNative(" + ToJsStringLiteral(text) + L");");
}

void DemoWindow::HandleMediaCommand(Control* sender, MouseEventArgs)
{
	if (sender == RequireControl<Control>(L"mediaOpen"))
	{
		OpenFileDialog dialog;
		dialog.Filter = MakeDialogFilterStrring(
			"媒体文件", "*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.mp3;*.wav;*.flac;*.m4a");
		dialog.SupportMultiDottedExtensions = true;
		dialog.Title = "选择媒体文件";
		if (dialog.ShowDialog(Handle) == DialogResult::OK && !dialog.SelectedPaths.empty())
		{
			_media->Load(Convert::StringToWString(dialog.SelectedPaths[0]));
			_media->Play();
		}
	}
	else if (sender == RequireControl<Control>(L"mediaPlay")) _media->Play();
	else if (sender == RequireControl<Control>(L"mediaPause")) _media->Pause();
	else _media->Stop();
}

void DemoWindow::HandleMediaVolume(Control*, float, float value)
{
	if (_media) _media->Volume = value / 100.0;
}

void DemoWindow::HandleMediaSpeed(Control*, float, float value)
{
	if (!_media || !_mediaSpeedText) return;
	_media->PlaybackRate = value / 100.0f;
	_mediaSpeedText->Text = StringHelper::Format(L"%.2fx", value / 100.0f);
	_mediaSpeedText->InvalidateVisual();
}

void DemoWindow::HandleMediaLoop(Control* sender)
{
	if (_media) _media->Loop = static_cast<CheckBox*>(sender)->Checked;
}

void DemoWindow::HandleMediaSeek(Control*, float, float value)
{
	if (_updatingMediaProgress || !_media || _media->Duration <= 0) return;
	_media->Position = value / 1000.0 * _media->Duration;
}

void DemoWindow::HandleMediaOpened(MediaPlayer* sender)
{
	const int total = static_cast<int>(sender->Duration);
	_mediaTime->Text = StringHelper::Format(
		L"00:00 / %02d:%02d", total / 60, total % 60);
	_mediaTime->InvalidateVisual();
	UpdateStatus(L"MediaPlayer: " + FileNameFromPath(sender->MediaFile));
}

void DemoWindow::HandleMediaEnded(MediaPlayer*)
{
	_mediaTime->Text = L"播放结束";
	_mediaTime->InvalidateVisual();
	UpdateStatus(L"MediaPlayer: Ended");
}

void DemoWindow::HandleMediaFailed(MediaPlayer*)
{
	_mediaTime->Text = L"加载失败";
	_mediaTime->InvalidateVisual();
	UpdateStatus(L"MediaPlayer: Failed");
}

void DemoWindow::HandleMediaPosition(MediaPlayer* sender, double position)
{
	const int current = static_cast<int>(position);
	const int total = std::max(0, static_cast<int>(sender->Duration));
	_mediaTime->Text = StringHelper::Format(L"%02d:%02d / %02d:%02d",
		current / 60, current % 60, total / 60, total % 60);
	_mediaTime->InvalidateVisual();
	if (sender->Duration > 0)
	{
		_updatingMediaProgress = true;
		_mediaProgress->Value = static_cast<float>(position / sender->Duration * 1000.0);
		_updatingMediaProgress = false;
	}
}

void DemoWindow::HandleSystemContextMenu(Control*, int id)
{
	UpdateStatus(StringHelper::Format(L"ContextMenu command: %d", id));
}
