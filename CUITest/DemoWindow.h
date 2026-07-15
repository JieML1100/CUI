#pragma once

/**
 * @file DemoWindow.h
 * @brief CUITest 演示窗口声明（用于示例/测试）。
 */
#include <iostream>
#include "../CUI/include/Form.h"
#include "../CUI/include/Layout/Layout.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/MessageDialog.h"
#include "../CUI/include/PropertyGrid.h"
#include "../CUI/include/ReportView.h"
#include "../CUI/include/Toast.h"
#include "../CUI/include/KpiCard.h"
#include "../CUI/include/FilterBar.h"
#include "CustomControls.h"
#include <Utils.h>

class WebBrowser;
class DemoWindow : public Form
{
public:
    DemoWindow();
    ~DemoWindow();

private:
    void Theme_OnSelectionChanged(class Control* sender);
    void Theme_Apply(const std::wstring& themeName);
    void Theme_ApplyCurrent();
    void Menu_OnCommand(class Control* sender, int id);
    void Ui_UpdateStatus(const std::wstring& text);
    void Ui_UpdateProgress(float value01);

    void Basic_OnButtonClick(class Control* sender, MouseEventArgs e);
    void Basic_OnMouseWheel(class Control* sender, MouseEventArgs e);
    void Basic_OnRadioChecked(class Control* sender);
    void Basic_OnIconButtonClick(class Control* sender, MouseEventArgs e);

    void Picture_OnOpenImage(class Control* sender, MouseEventArgs e);
    void Picture_OnDropFile(class Control* sender, std::vector<std::wstring> files);

    void Data_OnToggleEnable(class Control* sender, MouseEventArgs e);
    void Data_OnToggleVisible(class Control* sender, MouseEventArgs e);

    void System_OnNotifyToggle(class Control* sender, MouseEventArgs e);
    void System_OnBalloonTip(class Control* sender, MouseEventArgs e);
    void System_OnContextMenuCommand(class Control* sender, int id);

    void BuildMenuToolStatus();
    void BuildTabs();
    void BuildTab_Basic(TabPage* page);
    void BuildTab_Containers(TabPage* page);
    void BuildTab_Data(TabPage* page);
    void BuildTab_Analytics(TabPage* page);
    void BuildTab_Layout(TabPage* page);
    void BuildTab_System(TabPage* page);
    void BuildTab_Web(TabPage* page);
    void BuildTab_Media(TabPage* page);

private:
    std::shared_ptr<BitmapSource> _bmps[10]{};
    std::shared_ptr<BitmapSource> _icons[5]{};

    Menu* _menu = nullptr;
    ToolBar* _toolbar = nullptr;
    StatusBar* _statusbar = nullptr;

    Slider* _topSlider = nullptr;
    Label* _topStatus = nullptr;
    Label* _themeLabel = nullptr;
    ComboBox* _themeSelector = nullptr;
    TabControl* _tabs = nullptr;

    // Basic tab
    Button* _basicButton = nullptr;
    ToolTip* _basicToolTip = nullptr;
    CheckBox* _basicEnableCheck = nullptr;
    LinkLabel* _basicLink = nullptr;
    RadioBox* _rb1 = nullptr;
    RadioBox* _rb2 = nullptr;


    // Containers tab
    PictureBox* _picture = nullptr;
    ProgressBar* _progress = nullptr;
    LoadingRing* _loadingRing = nullptr;
    ProgressRing* _progressRing = nullptr;

    // Data tab
    GridView* _grid = nullptr;
    PagedGridView* _pagedGrid = nullptr;
    Switch* _gridEnableSwitch = nullptr;
    Switch* _gridVisibleSwitch = nullptr;

    // Analytics tab
    FilterBar* _analyticsFilter = nullptr;
    KpiCard* _kpiRevenue = nullptr;
    KpiCard* _kpiDeals = nullptr;
    KpiCard* _kpiMargin = nullptr;
    ChartView* _salesChart = nullptr;
    ReportView* _salesReport = nullptr;

    // Web/Media
    WebBrowser* _web = nullptr;
    MediaPlayer* _media = nullptr;

    // System integration
    Taskbar* _taskbar = nullptr;
    NotifyIcon* _notify = nullptr;
    ContextMenu* _systemContextMenu = nullptr;
    ToastHost* _toastHost = nullptr;
    bool _notifyVisible = false;
};
