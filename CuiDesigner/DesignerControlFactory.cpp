#include "DesignerControlFactory.h"
#include "FakeWebBrowser.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/DateTimePicker.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/FilterBar.h"
#include "../CUI/include/GridView.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/KpiCard.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/LinkLabel.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/LoadingRing.h"
#include "../CUI/include/MediaPlayer.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/Panel.h"
#include "../CUI/include/PasswordBox.h"
#include "../CUI/include/PictureBox.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ProgressRing.h"
#include "../CUI/include/PropertyGrid.h"
#include "../CUI/include/RadioBox.h"
#include "../CUI/include/ReportView.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ScrollView.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/SplitContainer.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/Switch.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/Toast.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/Layout/DockPanel.h"
#include "../CUI/include/Layout/GridPanel.h"
#include "../CUI/include/Layout/RelativePanel.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../CUI/include/Layout/WrapPanel.h"

namespace DesignerControlFactory
{
std::unique_ptr<Control> Create(UIClass type, int x, int y)
{
	switch (type)
	{
	case UIClass::UI_Base: return std::make_unique<Control>();
	case UIClass::UI_Label: return std::make_unique<Label>(L"标签", x, y);
	case UIClass::UI_LinkLabel: return std::make_unique<LinkLabel>(L"链接标签", x, y);
	case UIClass::UI_Button: return std::make_unique<Button>(L"按钮", x, y, 120, 30);
	case UIClass::UI_TextBox: return std::make_unique<TextBox>(L"", x, y, 200, 25);
	case UIClass::UI_RichTextBox: return std::make_unique<RichTextBox>(L"", x, y, 300, 160);
	case UIClass::UI_PasswordBox: return std::make_unique<PasswordBox>(L"", x, y, 200, 25);
	case UIClass::UI_DateTimePicker: return std::make_unique<DateTimePicker>(L"", x, y, 200, 28);
	case UIClass::UI_NumericUpDown: return std::make_unique<NumericUpDown>(x, y, 140, 30);
	case UIClass::UI_Panel: return std::make_unique<Panel>(x, y, 200, 200);
	case UIClass::UI_GroupBox: return std::make_unique<GroupBox>(L"GroupBox", x, y, 240, 180);
	case UIClass::UI_Expander: return std::make_unique<Expander>(L"Expander", x, y, 260, 160);
	case UIClass::UI_ScrollView: return std::make_unique<ScrollView>(x, y, 240, 200);
	case UIClass::UI_StackPanel: return std::make_unique<StackPanel>(x, y, 200, 200);
	case UIClass::UI_GridPanel: return std::make_unique<GridPanel>(x, y, 200, 200);
	case UIClass::UI_DockPanel: return std::make_unique<DockPanel>(x, y, 200, 200);
	case UIClass::UI_WrapPanel: return std::make_unique<WrapPanel>(x, y, 200, 200);
	case UIClass::UI_RelativePanel: return std::make_unique<RelativePanel>(x, y, 200, 200);
	case UIClass::UI_SplitContainer: return std::make_unique<SplitContainer>(x, y, 360, 220);
	case UIClass::UI_CheckBox: return std::make_unique<CheckBox>(L"复选框", x, y);
	case UIClass::UI_RadioBox: return std::make_unique<RadioBox>(L"单选框", x, y);
	case UIClass::UI_ComboBox: return std::make_unique<ComboBox>(L"", x, y, 150, 25);
	case UIClass::UI_ListView: return std::make_unique<ListView>(x, y, 320, 220);
	case UIClass::UI_ListBox: return std::make_unique<ListBox>(x, y, 220, 180);
	case UIClass::UI_GridView: return std::make_unique<GridView>(x, y, 360, 200);
	case UIClass::UI_PropertyGrid: return std::make_unique<PropertyGridView>(x, y, 300, 320);
	case UIClass::UI_ChartView: return std::make_unique<ChartView>(x, y, 420, 260);
	case UIClass::UI_ReportView: return std::make_unique<ReportView>(x, y, 480, 300);
	case UIClass::UI_KpiCard: return std::make_unique<KpiCard>(x, y, 220, 132);
	case UIClass::UI_FilterBar: return std::make_unique<FilterBar>(x, y, 640, 48);
	case UIClass::UI_TreeView: return std::make_unique<TreeView>(x, y, 220, 220);
	case UIClass::UI_ProgressBar: return std::make_unique<ProgressBar>(x, y, 200, 20);
	case UIClass::UI_LoadingRing: return std::make_unique<LoadingRing>(x, y, 48, 48);
	case UIClass::UI_ProgressRing: return std::make_unique<ProgressRing>(x, y, 72, 72);
	case UIClass::UI_Slider: return std::make_unique<Slider>(x, y, 200, 30);
	case UIClass::UI_PictureBox: return std::make_unique<PictureBox>(x, y, 150, 150);
	case UIClass::UI_Switch: return std::make_unique<Switch>(x, y, 60, 30);
	case UIClass::UI_TabControl: return std::make_unique<TabControl>(x, y, 360, 240);
	case UIClass::UI_ToolBar: return std::make_unique<ToolBar>(x, y, 360, 34);
	case UIClass::UI_Menu: return std::make_unique<Menu>(x, y, 600, 28);
	case UIClass::UI_StatusBar: return std::make_unique<StatusBar>(x, y, 600, 26);
	case UIClass::UI_ToastHost: return std::make_unique<ToastHost>(x, y, 340, 260);
	case UIClass::UI_WebBrowser: return std::make_unique<FakeWebBrowser>(x, y, 500, 360);
	case UIClass::UI_MediaPlayer: return std::make_unique<MediaPlayer>(x, y, 640, 360);
	default: return nullptr;
	}
}
}
