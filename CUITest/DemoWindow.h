#pragma once

/**
 * @file DemoWindow.h
 * @brief CUITest 演示窗口声明（用于示例/测试）。
 */
#include <iostream>
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Layout/Layout.h"
#include "CustomControls.h"
class DemoWindow : public Form
{
public:
    DemoWindow();
    ~DemoWindow();

private:
    void Menu_OnCommand(class Control* sender, int id);
    void Ui_UpdateStatus(const std::wstring& text);
    void Ui_UpdateProgress(float value01);

    void Basic_OnButtonClick(class Control* sender, MouseEventArgs e);
    void Basic_OnMouseWheel(class Control* sender, MouseEventArgs e);
    void Basic_OnRadioChecked(class Control* sender);
    void Basic_OnIconButtonClick(class Control* sender, MouseEventArgs e);

    void Picture_OnOpenImage(class Control* sender, MouseEventArgs e);
    void Picture_OnDropFile(class Control* sender, List<std::wstring> files);

    void Data_OnToggleEnable(class Control* sender, MouseEventArgs e);
    void Data_OnToggleVisible(class Control* sender, MouseEventArgs e);

    void System_OnNotifyToggle(class Control* sender, MouseEventArgs e);
    void System_OnBalloonTip(class Control* sender, MouseEventArgs e);

    void BuildMenuToolStatus();
    void BuildTabs();
    void BuildTab_Basic(TabPage* page);
    void BuildTab_Containers(TabPage* page);
    void BuildTab_Data(TabPage* page);
    void BuildTab_Layout(TabPage* page);
    void BuildTab_System(TabPage* page);
    void BuildTab_Web(TabPage* page);
    void BuildTab_Media(TabPage* page);

private:
    ID2D1Bitmap* _bmps[10]{};
    ID2D1Bitmap* _icons[5]{};

    Menu* _menu = nullptr;
    ToolBar* _toolbar = nullptr;
    StatusBar* _statusbar = nullptr;

    Slider* _topSlider = nullptr;
    Label* _topStatus = nullptr;
    TabControl* _tabs = nullptr;

    // Basic tab
    Button* _basicButton = nullptr;
    CheckBox* _basicEnableCheck = nullptr;
    LinkLabel* _basicLink = nullptr;
    RadioBox* _rb1 = nullptr;
    RadioBox* _rb2 = nullptr;


    // Containers tab
    PictureBox* _picture = nullptr;
    ProgressBar* _progress = nullptr;

    // Data tab
    GridView* _grid = nullptr;
    Switch* _gridEnableSwitch = nullptr;
    Switch* _gridVisibleSwitch = nullptr;

    // Web/Media
    WebBrowser* _web = nullptr;
    MediaPlayer* _media = nullptr;

    // System integration
    Taskbar* _taskbar = nullptr;
    NotifyIcon* _notify = nullptr;
    bool _notifyVisible = false;
};
