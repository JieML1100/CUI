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
    void label1_OnMouseWheel(class Control* sender, MouseEventArgs e);
    void button1_OnMouseClick(class Control* sender, MouseEventArgs e);
    void menu_OnCommand(class Control* sender, int id);
    void slider1_OnValueChanged(class Control* sender, float oldValue, float newValue);
    void radiobox1_OnChecked(class Control* sender);
    void radiobox2_OnChecked(class Control* sender);
    void bt2_OnMouseClick(class Control* sender, MouseEventArgs e);
    void sw1_OnMouseClick(class Control* sender, MouseEventArgs e);
    void sw2_OnMouseClick(class Control* sender, MouseEventArgs e);
    void iconButton_OnMouseClick(class Control* sender, MouseEventArgs e);
    void picturebox1_OnDropFile(class Control* sender, List<std::wstring> files);
    ID2D1Bitmap* bmps[10]{};
    ID2D1Bitmap* icos[8];
    Label* label1;
    CustomLabel1* clabel1;
    Button* button1;
    TextBox* textbox0;
    CustomTextBox1* textbox1;
    RoundTextBox* textbox3;
    PasswordBox* pwdbox1;
    RichTextBox* textbox2;
    ComboBox* combobox1;
    CheckBox* checkbox1;
    RadioBox* radiobox1;
    RadioBox* radiobox2;
    TabControl* tabControl1;
    Panel* panel1;
    PictureBox* picturebox1;
    ProgressBar* progressbar1;
    GridView* gridview1;
    Button* bt2;
    Switch* sw1;
    Switch* sw2;

    WebBrowser* web1;
    MediaPlayer* mediaPlayer;

    Menu* menu1;
    ToolBar* toolbar1;
    StatusBar* statusbar1;
    Slider* slider1;
};

NotifyIcon* TestNotifyIcon(HWND handle);