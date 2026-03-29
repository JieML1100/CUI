# CUI - 现代化 Windows GUI 框架完整文档

## 目录

1. [项目概述](#项目概述)
2. [核心特性](#核心特性)
3. [项目架构](#项目架构)
4. [快速开始](#快速开始)
5. [控件指南](#控件指南)
6. [布局系统](#布局系统)
7. [事件与输入](#事件与输入)
8. [高级功能](#高级功能)
9. [可视化设计器](#可视化设计器)
10. [示例代码](#示例代码)
11. [常见问题](#常见问题)
12. [依赖与要求](#依赖与要求)
13. [社区与支持](#社区与支持)

---

## 项目概述

CUI 是一个基于 **Direct2D** 和 **DirectComposition** 的现代化 Windows 原生 GUI 框架，使用 C++20 编写。它为 Windows 桌面应用程序开发提供了一套完整的高性能解决方案，同时配备了功能强大的可视化 UI 设计器。

### 核心设计理念

- **高性能渲染**: 利用 Direct2D 硬件加速和 DirectComposition 合成引擎
- **现代化 API**: 采用 C++20 特性，提供简洁易用的面向对象接口
- **所见即所得**: 配套的可视化设计器支持拖拽式 UI 开发
- **原生体验**: 完全基于 Windows 图形栈，提供原生应用程序体验

### 应用场景

CUI 适用于开发各类 Windows 桌面应用程序：

- 专业工具软件
- 多媒体应用
- 数据可视化工具
- 工业控制系统
- 嵌入式设备界面
- Web 混合应用

---

## 核心特性

### 高性能渲染引擎

CUI 采用 Direct2D 作为底层渲染引擎，充分利用 GPU 硬件加速：

```cpp
// Direct2D 硬件加速渲染
auto render = new D2DGraphics1(hwnd);
render->BeginRender();
render->Clear(D2D1::ColorF(0, 0, 0, 0));
// 执行渲染操作
render->EndRender();
```

**渲染特性：**
- 硬件加速的 2D 图形绘制
- DirectComposition 窗口合成
- 平滑的动画支持
- 高 DPI 感知渲染
- 抗锯齿文字渲染

### 丰富的控件库

CUI 提供了 33+ 常用 UI 控件，覆盖各种开发需求：

**基础控件:**
- `Button` - 按钮控件
- `Label` - 文本标签
- `LinkLabel` - 超链接标签
- `TextBox` - 单行文本输入
- `RichTextBox` - 富文本编辑
- `PasswordBox` - 密码输入框
- `ComboBox` - 下拉选择框
- `CheckBox` - 复选框
- `RadioBox` - 单选框
- `Switch` - 开关控件
- `Slider` - 滑动条
- `ProgressBar` - 进度条

**容器控件:**
- `Panel` - 通用面板
- `TabControl` / `TabPage` - 选项卡
- `GroupBox` - 分组框（可扩展）

**数据展示:**
- `TreeView` - 树形视图
- `GridView` - 数据网格
- `ListView` - 列表视图（可扩展）

**媒体控件:**
- `PictureBox` - 图片显示
- `MediaPlayer` - 媒体播放器
- `WebBrowser` - WebView2 浏览器

**系统集成:**
- `Menu` / `MenuItem` - 菜单系统
- `ToolBar` - 工具栏
- `StatusBar` - 状态栏
- `NotifyIcon` - 托盘图标
- `Taskbar` - 任务栏集成

### 灵活的布局系统

CUI 提供了五种强大的布局容器，满足各种界面需求：

| 布局类型 | 特点 | 适用场景 |
|---------|------|---------|
| `StackPanel` | 线性排列子控件 | 工具栏、表单 |
| `GridPanel` | 网格行列布局 | 复杂表格界面 |
| `DockPanel` | 停靠式布局 | 窗口停靠、IDE 界面 |
| `WrapPanel` | 自动换行布局 | 工具箱、图标网格 |
| `RelativePanel` | 相对约束布局 | 精确位置控制 |

### 完善的事件系统

CUI 提供了全面的事件处理机制：

```cpp
// 按钮点击事件
button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // 处理点击
};

// 文本变更事件
textBox->OnTextChanged += [](Control* sender, const std::wstring& oldText, 
                              const std::wstring& newText) {
    // 处理文本变更
};

// 键盘事件
control->OnKeyDown += [](Control* sender, KeyEventArgs e) {
    if (e.Key == VK_RETURN) {
        // 回车键处理
    }
};
```

**支持的事件类型：**
- 鼠标事件（点击、移动、滚动、拖放）
- 键盘事件（按键、字符输入）
- 焦点事件（获得/失去焦点）
- 控件事件（选中、变更、滚动）
- 窗口事件（大小改变、移动、关闭）

### 国际化输入支持

CUI 原生支持 IME（Input Method Editor）中文、日文、韩文等复杂文字输入：

```cpp
// 启用中文输入
textBox->ImeMode = ImeMode::On;
```

### SVG 矢量图形

内置 nanosvg 库，支持 SVG 矢量图渲染：

```cpp
// 从 SVG 数据加载位图
ID2D1Bitmap* svgBitmap = ToBitmapFromSvg(graphics, svgContent);
pictureBox->SetImageEx(svgBitmap, false);
```

### WebView2 集成

通过 WebView2 实现 Web 内容嵌入和 JS/C++ 互操作：

```cpp
// 创建 WebBrowser 控件
WebBrowser* web = new WebBrowser(10, 40, 800, 500);

// 注册 JS 调用 C++ 处理器
web->RegisterJsInvokeHandler(L"native.echo", [](const std::wstring& payload) {
    return L"echo: " + payload;
});

// C++ 调用 JS
web->ExecuteScriptAsync(L"window.setFromNative('Hello from C++');");

// 设置 HTML 内容
web->SetHtml(L"<html><body><h1>Hello World</h1></body></html>");
```

### 媒体播放功能

内置 MediaPlayer 控件支持多种媒体格式：

```cpp
// 创建媒体播放器
MediaPlayer* player = new MediaPlayer(10, 40, 800, 500);

// 加载并播放视频
player->Load(L"C:\\video\\demo.mp4");
player->Play();

// 控制播放
player->Pause();
player->Stop();
player->Volume = 0.8f;
player->PlaybackRate = 1.5f;
```

---

## 项目架构

### 解决方案结构

```
CUI.sln
├── CUI/                    # 主框架项目（Windows 8+）
│   ├── CUI/CUI.vcxproj    # 核心项目配置
│   ├── GUI/               # 框架源代码
│   │   ├── Controls/      # 控件实现
│   │   ├── Layout/        # 布局系统
│   │   └── ...
│   └── nanosvg.cpp/h      # SVG 渲染库
├── CUI_Legacy/            # 遗留版本（Windows 7）
├── CuiDesigner/           # 可视化设计器
├── CUITest/               # 示例与测试程序
├── CUITest_Legacy/        # 遗留版本示例
├── D2DGraphics/           # 底层图形封装
└── Utils/                 # 设计器等项目使用的通用工具库
```

### 核心类层次

```
Control (基类)
├── Form                    # 顶层窗口
├── Button                  # 按钮
├── Label                   # 标签
├── TextBox                 # 文本框
├── ComboBox                # 下拉框
├── CheckBox/RadioBox       # 选择控件
├── Slider                  # 滑动条
├── ProgressBar             # 进度条
├── PictureBox              # 图片框
├── Panel                   # 面板
├── TabControl              # 选项卡
├── TreeView                # 树形视图
├── GridView                # 数据网格
├── Menu/ToolBar/StatusBar  # 菜单栏
├── WebBrowser              # Web 浏览器
├── MediaPlayer             # 媒体播放
└── NotifyIcon              # 托盘图标
```

### 布局容器层次

```
Control
├── StackPanel              # 堆叠布局
├── GridPanel               # 网格布局
├── DockPanel               # 停靠布局
├── WrapPanel               # 换行布局
└── RelativePanel           # 相对布局
```

---

## 快速开始

### 环境要求

- **操作系统**: Windows 8 / Windows 10 / Windows 11
- **开发工具**: Visual Studio 2022 或更高版本
- **C++ 标准**: C++20
- **依赖项**: WebView2 NuGet 包

### 编译项目

1. 使用 Visual Studio 打开 `CUI.sln`
2. 选择 Release|x64 配置
3. 编译解决方案

### 创建第一个应用

```cpp
#include "CUI/GUI/Form.h"

using namespace CUI;

class MainWindow : public Form
{
public:
    MainWindow() : Form(L"我的第一个 CUI 应用", { 100, 100 }, { 800, 600 })
    {
        // 设置窗口背景色
        BackColor = Colors::DarkGray;
        
        // 添加标签
        auto label = AddControl(new Label(L"欢迎使用 CUI!", 50, 50));
        label->Font = new Font(L"Microsoft YaHei", 24.0f);
        label->ForeColor = Colors::White;
        
        // 添加按钮
        auto button = AddControl(new Button(L"点击我", 50, 100, 120, 40));
        button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
            MessageBoxW(sender->GetForm()->Handle, 
                       L"你点击了按钮!", L"CUI", MB_OK);
        };
        
        // 添加图片框
        auto picture = AddControl(new PictureBox(50, 160, 200, 150));
        picture->SizeMode = ImageSizeMode::StretchIamge;
        
        // 状态栏
        auto statusBar = AddControl(new StatusBar(0, 0, 800, 26));
        statusBar->AddPart(L"就绪", -1);
        statusBar->AddPart(L"CUI Demo", 100);
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    Application::Initialize(hInstance);
    
    auto window = new MainWindow();
    window->Show();
    
    return Application::Run();
}
```

### 运行效果

上述代码将创建一个包含以下元素的窗口：
- 标题栏带有最小化、最大化、关闭按钮
- 居中的"欢迎使用 CUI!"标签
- 点击会弹出消息框的按钮
- 可拖放图片的图片显示区域
- 显示"就绪"和"CUI Demo"的状态栏

---

## 控件指南

### 基础控件

#### Label 标签

用于显示静态文本：

```cpp
// 创建标签
auto label = AddControl(new Label(L"这是一个标签", 20, 20));

// 设置样式
label->Font = new Font(L"Microsoft YaHei", 16.0f);
label->ForeColor = D2D1::ColorF(Colors::LightBlue);
```

#### Button 按钮

标准按钮控件：

```cpp
auto button = AddControl(new Button(L"确定", 20, 50, 100, 30));

button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // 处理点击事件
    sender->Text = L"已点击";
    sender->PostRender();
};
```

带图标的按钮：

```cpp
auto iconButton = AddControl(new Button(L"", 20, 90, 40, 40));
iconButton->Image = myBitmap;
iconButton->SizeMode = ImageSizeMode::CenterImage;
iconButton->BackColor = D2D1::ColorF(0, 0, 0, 0);
```

#### TextBox 文本框

单行文本输入：

```cpp
auto textBox = AddControl(new TextBox(L"默认文本", 20, 120, 200, 26));

textBox->OnTextChanged += [](Control* sender, 
                             const std::wstring& oldText,
                             const std::wstring& newText) {
    // 文本变更处理
};
```

#### PasswordBox 密码框

带掩码的密码输入：

```cpp
auto password = AddControl(new PasswordBox(L"", 20, 150, 200, 26));
```

#### RichTextBox 富文本框

支持多行文本和拖放：

```cpp
auto richText = AddControl(new RichTextBox(L"多行文本区域\n", 20, 180, 400, 150));
richText->AllowMultiLine = true;
richText->ScrollToEnd();

richText->OnDropText += [](Control* sender, std::wstring text) {
    auto rtb = (RichTextBox*)sender;
    rtb->AppendText(text);
    rtb->ScrollToEnd();
};
```

#### ComboBox 下拉框

```cpp
auto combo = AddControl(new ComboBox(L"请选择", 20, 340, 180, 28));
combo->ExpandCount = 8;

// 添加选项
for (int i = 0; i < 20; i++) {
    combo->Items.Add(StringHelper::Format(L"选项 %d", i));
}

combo->OnSelectionChanged += [](Control* sender) {
    auto cb = (ComboBox*)sender;
    // 处理选中变更
};
```

#### CheckBox 复选框

```cpp
auto check = AddControl(new CheckBox(L"同意条款", 20, 380));
check->Checked = true;

check->OnChecked += [](Control* sender) {
    auto cb = (CheckBox*)sender;
    if (cb->Checked) {
        // 已选中
    }
};
```

#### RadioBox 单选框

```cpp
auto radio1 = AddControl(new RadioBox(L"选项 A", 20, 410));
auto radio2 = AddControl(new RadioBox(L"选项 B", 100, 410));

radio1->Checked = true;

radio1->OnChecked += radio2->OnChecked += [](Control* sender) {
    // 确保单选行为
    if (sender->Checked) {
        // 处理选中
    }
};
```

#### Switch 开关

```cpp
auto switchControl = AddControl(new Switch(20, 440));
switchControl->Checked = true;

switchControl->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    auto sw = (Switch*)sender;
    // 切换状态处理
};
```

#### Slider 滑动条

```cpp
auto slider = AddControl(new Slider(20, 480, 200, 30));
slider->Min = 0;
slider->Max = 100;
slider->Value = 50;

slider->OnValueChanged += [](Control* sender, float oldValue, float newValue) {
    // 滑块值变更处理
};
```

#### ProgressBar 进度条

```cpp
auto progress = AddControl(new ProgressBar(20, 520, 200, 24));
progress->PercentageValue = 0.5f;  // 50%
```

### 容器控件

#### Panel 面板

```cpp
auto panel = AddControl(new Panel(300, 20, 400, 300));
panel->BackColor = D2D1::ColorF(1, 1, 1, 0.1f);
panel->BolderColor = D2D1::ColorF(1, 1, 1, 0.2f);

// 在面板中添加子控件
panel->AddControl(new Label(L"面板内标签", 10, 10));
```

#### TabControl 选项卡

```cpp
auto tabControl = AddControl(new TabControl(20, 20, 600, 400));

// 添加选项卡页面
auto page1 = tabControl->AddPage(L"第一页");
auto page2 = tabControl->AddPage(L"第二页");

// 在页面中添加内容
page1->AddControl(new Label(L"第一页内容", 10, 10));
page2->AddControl(new Label(L"第二页内容", 10, 10));
```

### 数据展示控件

#### TreeView 树形视图

```cpp
auto tree = AddControl(new TreeView(20, 20, 250, 400));

// 添加根节点
auto root = new TreeNode(L"根节点", nullptr);
tree->Root->Children.push_back(root);

// 添加子节点
for (int i = 0; i < 5; i++) {
    auto child = new TreeNode(StringHelper::Format(L"子节点 %d", i), nullptr);
    root->Children.push_back(child);
}
```

#### GridView 数据网格

```cpp
auto grid = AddControl(new GridView(290, 20, 500, 400));

// 添加列
grid->Columns.Add(GridViewColumn(L"名称", 150, ColumnType::Text));
grid->Columns.Add(GridViewColumn(L"类型", 100, ColumnType::Text));
grid->Columns.Add(GridViewColumn(L"操作", 80, ColumnType::Button));

// 添加行数据
for (int i = 0; i < 20; i++) {
    GridViewRow row;
    row.Cells = { L"项目 " + std::to_wstring(i), L"类型 A", L"" };
    grid->Rows.Add(row);
}
```

### 媒体控件

#### PictureBox 图片框

```cpp
auto picture = AddControl(new PictureBox(20, 20, 300, 200));
picture->SizeMode = ImageSizeMode::StretchIamge;

// 加载图片
auto img = BitmapSource::FromFile(L"C:\\image.png");
picture->SetImageEx(graphics->CreateBitmap(img->GetWicBitmap()), false);

// 拖放支持
picture->OnDropFile += [](Control* sender, List<std::wstring> files) {
    // 处理拖放的文件
};
```

#### MediaPlayer 媒体播放器

```cpp
auto player = AddControl(new MediaPlayer(20, 240, 400, 300));
player->AutoPlay = true;
player->Loop = false;

// 加载媒体
player->Load(L"C:\\video.mp4");

// 事件处理
player->OnMediaOpened += [](Control* sender) {
    // 媒体打开完成
};

player->OnMediaEnded += [](Control* sender) {
    // 媒体播放结束
};

player->OnPositionChanged += [](Control* sender, double position) {
    // 播放进度更新
};
```

### 系统控件

#### Menu 菜单

```cpp
auto menu = AddControl(new Menu(0, 0, 800, 28));
menu->BarBackColor = D2D1::ColorF(1, 1, 1, 0.08f);

auto fileMenu = menu->AddItem(L"文件");
fileMenu->AddSubItem(L"打开", 1001);
fileMenu->AddSubItem(L"保存", 1002);
fileMenu->AddSeparator();
fileMenu->AddSubItem(L"退出", 1003);

auto helpMenu = menu->AddItem(L"帮助");
helpMenu->AddSubItem(L"关于", 2001);

menu->OnMenuCommand += [](Control* sender, int id) {
    switch (id) {
        case 1001: /* 打开处理 */ break;
        case 1003: /* 退出处理 */ break;
    }
};
```

#### ToolBar 工具栏

```cpp
auto toolbar = AddControl(new ToolBar(0, 28, 800, 32));

auto btnNew = toolbar->AddToolButton(L"新建", 24);
auto btnOpen = toolbar->AddToolButton(L"打开", 24);
auto btnSave = toolbar->AddToolButton(L"保存", 24);

btnNew->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // 新建处理
};
```

#### StatusBar 状态栏

```cpp
auto statusBar = AddControl(new StatusBar(0, 550, 800, 26));
statusBar->AddPart(L"就绪", -1);
statusBar->AddPart(L"用户: admin", 120);
statusBar->AddPart(L"编码: UTF-8", 100);

// 更新状态栏文本
statusBar->SetPartText(0, L"处理中...");
```

#### NotifyIcon 托盘图标

```cpp
auto notify = new NotifyIcon();
notify->InitNotifyIcon(hwnd, 1);
notify->SetIcon(LoadIcon(NULL, IDI_APPLICATION));
notify->SetToolTip(L"我的应用");

notify->ClearMenu();
notify->AddMenuItem(L"显示窗口", 1);
notify->AddMenuSeparator();
notify->AddMenuItem(L"退出", 3);

notify->OnNotifyIconMenuClick += [](NotifyIcon* sender, int menuId) {
    switch (menuId) {
        case 1: ShowWindow(sender->hWnd, SW_SHOWNORMAL); break;
        case 3: PostMessage(sender->hWnd, WM_CLOSE, 0, 0); break;
    }
};

notify->ShowNotifyIcon();
```

#### Taskbar 任务栏

```cpp
auto taskbar = new Taskbar(hwnd);

// 设置任务栏进度
taskbar->SetValue(current, total);

// 设置任务栏缩略图按钮
taskbar->AddThumbnailButton(/* 按钮配置 */);
```

### WebBrowser 浏览器控件

```cpp
auto web = AddControl(new WebBrowser(20, 20, 760, 500));

// 注册 JS 调用 C++ 处理器
web->RegisterJsInvokeHandler(L"native.log", [](const std::wstring& msg) {
    // 从 JS 接收日志
    return L"logged";
});

// C++ 调用 JS
web->ExecuteScriptAsync(L"console.log('Hello from C++');");

// 设置 HTML
web->SetHtml(L"<html><body><h1>Embedded Content</h1></body></html>");

// 导航到 URL
web->Navigate(L"https://www.example.com");
```

---

## 布局系统

### StackPanel 堆叠布局

子控件按顺序排列，支持水平和垂直方向：

```cpp
auto stack = AddControl(new StackPanel(20, 20, 200, 300));
stack->SetOrientation(Orientation::Vertical);
stack->SetSpacing(8);

stack->AddControl(new Button(L"按钮 1", 0, 0, 180, 30));
stack->AddControl(new Button(L"按钮 2", 0, 0, 180, 30));
stack->AddControl(new Button(L"按钮 3", 0, 0, 180, 30));
```

### GridPanel 网格布局

支持行列定义的灵活网格布局：

```cpp
auto grid = AddControl(new GridPanel(240, 20, 300, 300));

// 定义行
grid->AddRow(GridLength::Auto());          // 自动高度
grid->AddRow(GridLength::Star(1.0f));       // 剩余空间
grid->AddRow(GridLength::Pixels(50));       // 固定高度 50px

// 定义列
grid->AddColumn(GridLength::Star(1.0f));    // 50% 宽度
grid->AddColumn(GridLength::Star(1.0f));    // 50% 宽度

// 添加控件并指定位置
auto title = new Label(L"标题", 0, 0);
title->GridRow = 0;
title->GridColumn = 0;
title->GridColumnSpan = 2;
grid->AddControl(title);

auto leftPanel = new Button(L"左侧", 0, 0, 80, 80);
leftPanel->GridRow = 1;
leftPanel->GridColumn = 0;
grid->AddControl(leftPanel);

auto rightPanel = new Button(L"右侧", 0, 0, 80, 80);
rightPanel->GridRow = 1;
rightPanel->GridColumn = 1;
grid->AddControl(rightPanel);
```

### DockPanel 停靠布局

子控件停靠在容器边缘：

```cpp
auto dock = AddControl(new DockPanel(560, 20, 300, 300));
dock->SetLastChildFill(true);

// 顶部停靠
auto top = new Label(L"顶部标题栏", 0, 0);
top->Size = SIZE{ 300, 40 };
top->DockPosition = Dock::Top;
dock->AddControl(top);

// 左侧停靠
auto left = new Label(L"左侧边栏", 0, 0);
left->Size = SIZE{ 60, 200 };
left->DockPosition = Dock::Left;
dock->AddControl(left);

// 底部停靠
auto bottom = new Label(L"底部状态", 0, 0);
bottom->Size = SIZE{ 300, 30 };
bottom->DockPosition =Dock::Bottom;
dock->AddControl(bottom);

// 填充剩余空间
auto fill = new Label(L"主内容区", 0, 0);
fill->DockPosition = Dock::Fill;
dock->AddControl(fill);
```

### WrapPanel 换行布局

子控件自动换行排列：

```cpp
auto wrap = AddControl(new WrapPanel(880, 20, 300, 300));
wrap->SetOrientation(Orientation::Horizontal);
wrap->SetSpacing(8);

for (int i = 1; i <= 15; i++) {
    wrap->AddControl(new Button(StringHelper::Format(L"按钮 %d", i), 
                                0, 0, 80, 30));
}
```

### RelativePanel 相对布局

通过相对约束定位子控件：

```cpp
auto relative = AddControl(new RelativePanel(20, 340, 400, 300));

// 居中按钮
auto centerBtn = new Button(L"居中", 0, 0, 100, 40);
RelativeConstraints constraints;
constraints.CenterHorizontal = true;
constraints.CenterVertical = true;
relative->AddControl(centerBtn);
relative->SetConstraints(centerBtn, constraints);

// 左上角标签
auto topLeft = new Label(L"左上", 0, 0);
RelativeConstraints tlConstraints;
tlConstraints.AlignLeftWithPanel = true;
tlConstraints.AlignTopWithPanel = true;
relative->AddControl(topLeft);
relative->SetConstraints(topLeft, tlConstraints);

// 右下角标签
auto bottomRight = new Label(L"右下", 0, 0);
RelativeConstraints brConstraints;
brConstraints.AlignRightWithPanel = true;
brConstraints.AlignBottomWithPanel = true;
relative->AddControl(bottomRight);
relative->SetConstraints(bottomRight, brConstraints);
```

### 布局组合使用

```cpp
// 创建主面板
auto mainPanel = AddControl(new Panel(20, 20, 760, 500));

// 使用 GridPanel 划分左右两部分
auto gridPanel = mainPanel->AddControl(new GridPanel(10, 10, 740, 480));
gridPanel->AddColumn(GridLength::Star(1.0f));
gridPanel->AddColumn(GridLength::Pixels(250));

// 左侧 StackPanel
auto leftStack = new StackPanel(0, 0, 300, 400);
leftStack->GridRow = 0;
leftStack->GridColumn = 0;
leftStack->SetOrientation(Orientation::Vertical);
leftStack->SetSpacing(10);
gridPanel->AddControl(leftStack);

// 右侧 WrapPanel
auto rightWrap = new WrapPanel(0, 0, 230, 400);
rightWrap->GridRow = 0;
rightWrap->GridColumn = 1;
rightWrap->SetOrientation(Orientation::Horizontal);
rightWrap->SetSpacing(8);
gridPanel->AddControl(rightWrap);
```

---

## 事件与输入

### 鼠标事件

```cpp
// 鼠标点击
control->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // e.Button - 鼠标按钮
    // e.X, e.Y - 鼠标位置
    // e.Clicks - 点击次数
};

// 鼠标双击
control->OnMouseDoubleClick += [](Control* sender, MouseEventArgs e) {
    // 双击处理
};

// 鼠标移动
control->OnMouseMove += [](Control* sender, MouseEventArgs e) {
    // 鼠标移动处理
};

// 鼠标滚轮
control->OnMouseWheel += [](Control* sender, MouseEventArgs e) {
    // e.Delta - 滚轮增量
};

// 鼠标进入/离开
control->OnMouseEnter += [](Control* sender, MouseEventArgs e) {
    // 鼠标进入
};

control->OnMouseLeave += [](Control* sender, MouseEventArgs e) {
    // 鼠标离开
};
```

### 键盘事件

```cpp
// 键按下
control->OnKeyDown += [](Control* sender, KeyEventArgs e) {
    // e.Key - 虚拟键码
    // e.Modifiers - 修饰键状态
};

// 键释放
control->OnKeyUp += [](Control* sender, KeyEventArgs e) {
    // 键释放处理
};

// 字符输入
control->OnCharInput += [](Control* sender, wchar_t ch) {
    // 字符输入处理
};
```

### 焦点事件

```cpp
control->OnGotFocus += [](Control* sender) {
    // 获得焦点
};

control->OnLostFocus += [](Control* sender) {
    // 失去焦点
};
```

### 拖放事件

```cpp
// 接受拖放
control->AllowDrop = true;

// 文件拖放
control->OnDropFile += [](Control* sender, List<std::wstring> files) {
    for (const auto& file : files) {
        // 处理拖放的文件
    }
};

// 文本拖放
control->OnDropText += [](Control* sender, std::wstring text) {
    // 处理拖放的文本
};
```

### 窗口事件

```cpp
// 窗口大小改变
form->OnSizeChanged += [](Form* sender) {
    // 调整布局
};

// 窗口移动
form->OnMoved += [](Form* sender) {
    // 窗口移动处理
};

// 窗口关闭
form->OnClosing += [](Form* sender) {
    // 询问是否关闭
    // 设置 sender->Cancel = true 可取消关闭
};

form->OnClosed += [](Form* sender) {
    // 窗口已关闭，清理资源
};
```

### 组合事件处理

```cpp
// 使用 Lambda 表达式捕获上下文
class MyWindow : public Form
{
private:
    Label* _statusLabel;
    int _clickCount = 0;

public:
    MyWindow() : Form(L"事件演示", { 100, 100 }, { 600, 400 })
    {
        auto button = AddControl(new Button(L"点击计数", 50, 50, 120, 40));
        
        _statusLabel = AddControl(new Label(L"点击次数: 0", 50, 100));
        
        // 捕获 this 指针
        button->OnMouseClick += [this](Control* sender, MouseEventArgs e) {
            (void)sender;
            (void)e;
            _clickCount++;
            _statusLabel->Text = StringHelper::Format(L"点击次数: %d", _clickCount);
            _statusLabel->PostRender();
        };
    }
};
```

---

## 高级功能

### 自定义控件

继承现有控件创建自定义版本：

```cpp
// CustomControls.h
#pragma once
#include "CUI/GUI/Button.h"
#include "CUI/GUI/TextBox.h"

class CustomLabel1 : public Label
{
public:
    CustomLabel1(const std::wstring& text, int x, int y) : Label(text, x, y) {}
    
    void OnPaint(Graphics* g) override
    {
        // 渐变绘制
        auto gradient = g->CreateLinearGradientBrush(
            D2D1::Point2F(0, 0),
            D2D1::Point2F(Width, 0),
            { {0, D2D1::ColorF(Colors::Cyan)}, {1, D2D1::ColorF(Colors::Blue)} }
        );
        
        auto rect = D2D1::RectF(0, 0, (float)Width, (float)Height);
        g->FillRectangle(rect, gradient);
        
        // 绘制文字
        DrawText(g, Text.c_str(), rect, GetTextFormat(), ForeColor);
    }
};

class CustomTextBox1 : public TextBox
{
public:
    CustomTextBox1(const std::wstring& text, int x, int y, int w, int h) 
        : TextBox(text, x, y, w, h) {}
    
    void OnPaint(Graphics* g) override
    {
        // 自定义边框绘制
        auto rect = D2D1::RectF(0, 0, (float)Width, (float)Height);
        g->DrawRoundedRectangle(rect, 8.0f, D2D1::ColorF(Colors::Gold));
        
        // 内部填充
        g->FillRectangle(
            D2D1::RoundedRect(D2D1::RectF(1, 1, Width - 1, Height - 1), 7, 7),
            BackColor
        );
        
        // 绘制文字
        DrawText(g, Text.c_str(), 
                 D2D1::RectF(5, 5, Width - 5, Height - 5),
                 GetTextFormat(), ForeColor);
    }
};
```

### DirectComposition 集成

```cpp
// 使用 DCompLayeredHost 进行层叠渲染
auto dcompHost = new DCompLayeredHost(hwnd);
dcompHost->Initialize();

// 创建合成视觉效果
IDCompositionVisual* visual = nullptr;
dcompDevice->CreateVisual(&visual);
dcompSurface = // 获取 Direct3D 表面
visual->SetContent(dcompSurface);
dcompTarget->AddVisual(visual);
```

### 资源管理

```cpp
// 字体资源管理
Font* font = new Font(L"Microsoft YaHei", 16.0f);
control->Font = font;  // 控件接管字体所有权

// 图片资源管理
ID2D1Bitmap* bitmap = graphics->CreateBitmap(image);
pictureBox->Image = bitmap;  // 控件接管图片所有权

// 自定义资源释放
void CleanupResources()
{
    for (auto& b : _bitmaps) {
        if (b) {
            b->Release();
            b = nullptr;
        }
    }
}
```

### 国际化

```cpp
// 启用 IME 输入
textBox->ImeMode = ImeMode::On;

// 设置 IME 模式
textBox->ImeMode = ImeMode::AlphaNumeric;

// 获取 IME 状态
auto imeStatus = textBox->GetImeStatus();
```

---

## 可视化设计器

### 启动设计器

```
CuiDesigner.exe
```

### 设计器功能

1. **拖放式界面设计**
   - 从工具箱拖拽控件到画布
   - 实时预览界面效果
   - 调整控件位置和大小

2. **属性编辑**
   - 在属性网格中修改控件属性
   - 支持属性分组和搜索
   - 实时应用属性变更

3. **布局容器支持**
   - StackPanel、GridPanel、DockPanel
   - WrapPanel、RelativePanel
   - 行列定义编辑器

4. **设计文件管理**
   - JSON 格式保存设计文件
   - 自动生成 C++ 代码
   - 支持设计文件版本控制

### 使用设计器创建界面

1. **创建新项目**
   - 启动 CuiDesigner.exe
   - 选择"新建项目"

2. **添加控件**
   - 从工具箱选择控件类型
   - 拖放到设计画布
   - 调整位置和大小

3. **设置属性**
   - 选中控件
   - 在属性网格中编辑属性
   - 实时预览效果

4. **使用布局容器**
   - 从工具箱添加布局容器
   - 设置容器属性
   - 将子控件添加到容器

5. **保存和生成代码**
   - 保存 JSON 设计文件
   - 生成 C++ 代码
   - 将代码集成到项目

### 生成代码示例

设计器生成的 C++ 代码结构：

```cpp
// Form1.h - 自动生成的界面定义
#pragma once
#include "Form.h"

class Form1 : public Form
{
public:
    Form1() : Form(L"Form1", { 100, 100 }, { 800, 600 })
    {
        InitializeComponents();
    }

private:
    void InitializeComponents()
    {
        // 标签
        auto label1 = AddControl(new Label(L"欢迎使用 CUI", 50, 50));
        label1->Font = new Font(L"Microsoft YaHei", 24.0f);
        
        // 按钮
        auto button1 = AddControl(new Button(L"确定", 50, 100, 100, 36));
        button1->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            // 事件处理
        };
        
        // 文本框
        auto textBox1 = AddControl(new TextBox(L"", 50, 150, 200, 28));
        
        // 布局容器
        auto stackPanel = AddControl(new StackPanel(300, 50, 200, 300));
        stackPanel->SetOrientation(Orientation::Vertical);
        stackPanel->AddControl(new Button(L"按钮1", 0, 0, 180, 30));
        stackPanel->AddControl(new Button(L"按钮2", 0, 0, 180, 30));
    }
};
```

---

## 示例代码

### 完整示例：多媒体播放器

```cpp
#include "Form.h"
#include "MediaPlayer.h"
#include "Button.h"
#include "Slider.h"
#include "Label.h"
#include "OpenFileDialog.h"

using namespace CUI;

class MediaPlayerWindow : public Form
{
private:
    MediaPlayer* _player;
    Label* _timeLabel;
    Slider* _progressSlider;
    Slider* _volumeSlider;
    bool _progressUpdating = false;

public:
    MediaPlayerWindow() : Form(L"媒体播放器", { 100, 100 }, { 900, 600 })
    {
        // 创建媒体播放器
        _player = AddControl(new MediaPlayer(10, 40, 880, 450));
        _player->Margin = Thickness(10, 40, 10, 120);
        _player->AutoPlay = false;
        _player->Loop = false;

        // 媒体事件
        _player->OnMediaOpened += [this](Control* sender) {
            auto player = (MediaPlayer*)sender;
            int total = (int)player->Duration;
            _timeLabel->Text = StringHelper::Format(L"00:00 / %02d:%02d", 
                                                     total / 60, total % 60);
            _progressSlider->Max = 1000;
            _progressSlider->Value = 0;
        };

        _player->OnPositionChanged += [this](Control* sender, double position) {
            if (_progressUpdating) return;
            auto player = (MediaPlayer*)sender;
            if (player->Duration > 0) {
                int cur = (int)position;
                int total = (int)player->Duration;
                _timeLabel->Text = StringHelper::Format(
                    L"%02d:%02d / %02d:%02d", 
                    cur / 60, cur % 60, total / 60, total % 60
                );
                _progressUpdating = true;
                _progressSlider->Value = (float)(position / player->Duration * 1000.0);
                _progressUpdating = false;
            }
        };

        _player->OnMediaEnded += [this](Control* sender) {
            (void)sender;
            _timeLabel->Text = L"播放结束";
        };

        // 控制面板
        auto controlPanel = AddControl(new Panel(10, 500, 880, 80));
        controlPanel->BackColor = D2D1::ColorF(1, 1, 1, 0.05f);

        // 打开按钮
        auto btnOpen = controlPanel->AddControl(new Button(L"打开", 10, 20, 80, 36));
        btnOpen->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            OpenFileDialog ofd;
            ofd.Filter = MakeDialogFilterString("媒体文件", 
                                                "*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.mp3");
            ofd.Title = "选择媒体文件";
            if (ofd.ShowDialog(this->Handle) == DialogResult::OK && 
                !ofd.SelectedPaths.empty()) {
                _player->Load(ofd.SelectedPaths[0]);
                _player->Play();
            }
        };

        // 播放控制按钮
        auto btnPlay = controlPanel->AddControl(new Button(L"播放", 100, 20, 70, 36));
        btnPlay->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            _player->Play();
        };

        auto btnPause = controlPanel->AddControl(new Button(L"暂停", 180, 20, 70, 36));
        btnPause->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            _player->Pause();
        };

        auto btnStop = controlPanel->AddControl(new Button(L"停止", 260, 20, 70, 36));
        btnStop->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            _player->Stop();
        };

        // 音量控制
        controlPanel->AddControl(new Label(L"音量", 340, 26));
        _volumeSlider = controlPanel->AddControl(new Slider(390, 22, 100, 30));
        _volumeSlider->Min = 0;
        _volumeSlider->Max = 100;
        _volumeSlider->Value = 80;
        _volumeSlider->OnValueChanged += [this](Control* s, float old, float ne) {
            (void)s;
            (void)old;
            _player->Volume = ne / 100.0;
        };
        _player->Volume = 0.8;

        // 进度条
        _progressSlider = controlPanel->AddControl(new Slider(500, 22, 300, 30));
        _progressSlider->OnValueChanged += [this](Control* s, float old, float ne) {
            (void)s;
            (void)old;
            if (_progressUpdating) return;
            if (_player->Duration > 0) {
                _player->Position = (ne / 1000.0) * _player->Duration;
            }
        };

        // 时间标签
        _timeLabel = controlPanel->AddControl(new Label(L"00:00 / 00:00", 810, 26));
        _timeLabel->ForeColor = Colors::LightGray;
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    Application::Initialize(hInstance);
    
    auto window = new MediaPlayerWindow();
    window->Show();
    
    return Application::Run();
}
```

### Web 集成示例

```cpp
#include "Form.h"
#include "WebBrowser.h"

using namespace CUI;

class Web集成Window : public Form
{
private:
    WebBrowser* _web;

public:
    Web集成Window() : Form(L"Web 集成演示", { 100, 100 }, { 1000, 700 })
    {
        _web = AddControl(new WebBrowser(10, 10, 980, 600));
        _web->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                             AnchorStyles::Right | AnchorStyles::Bottom;

        // 注册 JS 调用 C++ 处理器
        _web->RegisterJsInvokeHandler(L"native.getTime", 
            [](const std::wstring& payload) {
                (void)payload;
                SYSTEMTIME st;
                GetLocalTime(&st);
                return StringHelper::Format(
                    L"%04d-%02d-%02d %02d:%02d:%02d",
                    st.wYear, st.wMonth, st.wDay, 
                    st.wHour, st.wMinute, st.wSecond
                );
            });

        _web->RegisterJsInvokeHandler(L"native.calculate", 
            [](const std::wstring& payload) {
                // 简单计算示例
                return L"Result: " + payload;
            });

        // 生成 HTML
        std::wstring html =
            L"<!DOCTYPE html>"
            L"<html><head><meta charset='utf-8'/>"
            L"<style>"
            L"body{font-family:Segoe UI,Arial; padding:20px; background:#1e1e1e; color:#fff;}"
            L"button{padding:10px 20px; margin:5px; background:#0078d4; color:#fff; border:none; border-radius:4px; cursor:pointer;}"
            L"button:hover{background:#1084d8;}"
            L".output{padding:15px; background:#2d2d2d; border-radius:4px; margin-top:15px;}"
            L"input{padding:10px; border-radius:4px; border:1px solid #444; background:#333; color:#fff;}"
            L"</style></head><body>"
            L"<h2>CUI WebBrowser 互操作演示</h2>"
            L"<div>"
            L"<button id='btnTime'>获取服务器时间</button>"
            L"<button id='btnCalc'>计算 2+2</button>"
            L"</div>"
            L"<div class='output'>JS 调用 C++ 结果: <span id='jsResult'>(无)</span></div>"
            L"<div class='output'>C++ 调用 JS: <span id='nativeResult'>(等待响应)</span></div>"
            L"<script>"
            L"// JS 调用 C++"
            L"document.getElementById('btnTime').onclick = async function() {"
            L"  try {"
            L"    const r = await window.CUI.invoke('native.getTime', '');"
            L"    document.getElementById('jsResult').textContent = r;"
            L"  } catch(e) {"
            L"    document.getElementById('jsResult').textContent = 'Error: ' + e;"
            L"  }"
            L"};"
            L""
            L"document.getElementById('btnCalc').onclick = async function() {"
            L"  try {"
            L"    const r = await window.CUI.invoke('native.calculate', '2+2');"
            L"    document.getElementById('jsResult').textContent = r;"
            L"  } catch(e) {"
            L"    document.getElementById('jsResult').textContent = 'Error: ' + e;"
            L"  }"
            L"};"
            L""
            L"// C++ 调用 JS 的回调函数"
            L"window.setFromNative = function(text) {"
            L"  document.getElementById('nativeResult').textContent = text;"
            L"  return 'ok';"
            L"};"
            L"</script></body></html>";

        _web->SetHtml(html);

        // 添加演示 C++ 调用 JS 的按钮
        auto btnCallJs = AddControl(new Button(L"C++ 调用 JS", 10, 630, 150, 36));
        btnCallJs->OnMouseClick += [this](Control* s, MouseEventArgs e) {
            (void)s;
            (void)e;
            SYSTEMTIME st;
            GetLocalTime(&st);
            std::wstring msg = StringHelper::Format(
                L"来自 C++ 的问候 - %02d:%02d:%02d",
                st.wHour, st.wMinute, st.wSecond
            );
            _web->ExecuteScriptAsync(
                L"window.setFromNative(\"" + msg + L"\");"
            );
        };
    }
};
```

### 完整示例：数据管理系统

```cpp
#include "Form.h"
#include "GridView.h"
#include "TreeView.h"
#include "Button.h"
#include "TextBox.h"
#include "Panel.h"

using namespace CUI;

class DataManagerWindow : public Form
{
private:
    GridView* _dataGrid;
    TreeView* _categoryTree;
    TextBox* _searchBox;
    std::vector<GridViewRow> _allData;

public:
    DataManagerWindow() : Form(L"数据管理系统", { 100, 100 }, { 1200, 800 })
    {
        BackColor = Colors::grey31;

        // 顶部搜索栏
        auto searchPanel = AddControl(new Panel(10, 10, 1180, 50));
        searchPanel->BackColor = D2D1::ColorF(1, 1, 1, 0.05f);

        searchPanel->AddControl(new Label(L"搜索:", 15, 15));
        _searchBox = searchPanel->AddControl(new TextBox(L"", 60, 14, 300, 28));
        _searchBox->OnTextChanged += [this](Control* s, 
                                            const std::wstring& oldText,
                                            const std::wstring& newText) {
            (void)s;
            (void)oldText;
            FilterData(newText);
        };

        auto btnAdd = searchPanel->AddControl(new Button(L"添加", 380, 12, 80, 32));
        auto btnEdit = searchPanel->AddControl(new Button(L"编辑", 470, 12, 80, 32));
        auto btnDelete = searchPanel->AddControl(new Button(L"删除", 560, 12, 80, 32));
        auto btnRefresh = searchPanel->AddControl(new Button(L"刷新", 650, 12, 80, 32));

        // 左侧分类树
        _categoryTree = AddControl(new TreeView(10, 70, 250, 650));
        _categoryTree->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                                       AnchorStyles::Bottom;
        _categoryTree->BackColor = D2D1::ColorF(1, 1, 1, 0.05f);

        // 初始化分类
        auto root = new TreeNode(L"所有类别", nullptr);
        _categoryTree->Root->Children.push_back(root);

        auto category1 = new TreeNode(L"电子产品", nullptr);
        category1->Expand = true;
        root->Children.push_back(category1);
        for (int i = 1; i <= 3; i++) {
            category1->Children.push_back(
                new TreeNode(StringHelper::Format(L"子类别 %d", i), nullptr)
            );
        }

        root->Children.push_back(new TreeNode(L"办公用品", nullptr));
        root->Children.push_back(new TreeNode(L"家居用品", nullptr));
        root->Children.push_back(new TreeNode(L"食品饮料", nullptr));

        // 右侧数据网格
        _dataGrid = AddControl(new GridView(280, 70, 910, 650));
        _dataGrid->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                                   AnchorStyles::Right | AnchorStyles::Bottom;
        _dataGrid->BackColor = D2D1::ColorF(0, 0, 0, 0);
        _dataGrid->AllowUserToAddRows = false;

        // 设置列
        _dataGrid->Columns.Add(GridViewColumn(L"ID", 60, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"名称", 200, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"类别", 120, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"价格", 100, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"库存", 80, ColumnType::Text));
        _dataGrid->Columns.Add(GridViewColumn(L"状态", 100, ColumnType::ComboBox));
        
        // 设置列下拉选项
        auto statusCol = _dataGrid->Columns[5];
        statusCol.ComboBoxItems = { L"在售", L"缺货", L"停售" };

        // 添加示例数据
        GenerateSampleData();
        RefreshGrid();

        // 底部状态栏
        auto statusBar = AddControl(new StatusBar(0, 740, 1200, 30));
        statusBar->AddPart(L"就绪", -1);
        statusBar->AddPart(L"共 " + std::to_wstring(_allData.size()) + " 条记录", 200);
    }

private:
    void GenerateSampleData()
    {
        const wchar_t* categories[] = { L"电子产品", L"办公用品", L"家居用品", L"食品饮料" };
        const wchar_t* names[][4] = {
            { L"智能手机", L"笔记本电脑", L"平板电脑", L"智能手表" },
            { L"打印机", L"扫描仪", L"碎纸机", L"投影仪" },
            { L"沙发", L"餐桌", L"书柜", L"床" },
            { L"咖啡", L"茶叶", L"零食", L"饮料" }
        };

        for (int cat = 0; cat < 4; cat++) {
            for (int i = 0; i < 8; i++) {
                GridViewRow row;
                row.Cells = {
                    std::to_wstring(cat * 8 + i + 1),
                    names[cat][i % 4],
                    categories[cat],
                    std::to_wstring(100 + (cat * 100) + (i * 10)),
                    std::to_wstring(10 + (i * 5)),
                    i % 2 == 0 ? L"在售" : (i % 3 == 0 ? L"缺货" : L"停售")
                };
                _allData.push_back(row);
            }
        }
    }

    void RefreshGrid()
    {
        _dataGrid->Rows.Clear();
        for (const auto& row : _allData) {
            _dataGrid->Rows.Add(row);
        }
    }

    void FilterData(const std::wstring& keyword)
    {
        _dataGrid->Rows.Clear();
        for (const auto& row : _allData) {
            bool match = false;
            for (const auto& cell : row.Cells) {
                if (StringHelper::Contains(cell, keyword)) {
                    match = true;
                    break;
                }
            }
            if (match || keyword.empty()) {
                _dataGrid->Rows.Add(row);
            }
        }
    }
};
```

---

## 常见问题

### Q1: CUI 支持哪些 Windows 版本？

**A**: CUI 主版本支持 Windows 8 及以上版本。如果您需要支持 Windows 7，请使用 `CUI_Legacy` 项目（不包括 WebBrowser 功能）。

### Q2: 如何处理高 DPI 显示？

**A**: CUI 默认支持高 DPI 渲染。窗口会根据系统 DPI 设置自动缩放。对于更精细的控制，可以使用 `SetDpiAwareness()` 函数。

### Q3: 如何调试渲染问题？

**A**: 
1. 启用调试输出：`graphics->SetDebugMode(true)`
2. 检查 Direct2D 工厂状态
3. 验证资源创建是否成功
4. 使用 Visual Studio 的 Graphics Diagnostics 工具

### Q4: WebBrowser 控件如何处理导航错误？

**A**: 使用 `NavigationCompleted` 和 `NavigationStarting` 事件：

```cpp
_web->OnNavigationStarting += [](Control* sender, NavigationStartingArgs& args) {
    // 可以取消导航
    // args.Cancel = true;
};

_web->OnNavigationCompleted += [](Control* sender, NavigationCompletedArgs args) {
    if (!args.IsSuccess) {
        // 处理导航错误
        // args.WebErrorStatus 包含错误类型
    }
};
```

### Q5: 如何实现自定义绘制？

**A**: 继承控件并重写 `OnPaint` 方法：

```cpp
class CustomControl : public Control
{
public:
    void OnPaint(Graphics* g) override
    {
        // 自定义绘制逻辑
        auto rect = D2D1::RectF(0, 0, (float)Width, (float)Height);
        g->FillRectangle(rect, BackColor);
        // ... 更多绘制
    }
};
```

### Q6: 如何在不同控件间共享数据？

**A**: 有几种方式：
1. 通过父窗口持有共享数据指针
2. 使用观察者模式发布订阅
3. 通过静态变量共享（谨慎使用）
4. 使用外部数据模型类

### Q7: CUI 与 MFC/WTL 相比有什么优势？

**A**:
- 硬件加速渲染，性能更高
- 现代 C++ API，更易用
- 完整的可视化设计器
- 内置 WebView2 集成
- 更好的高 DPI 支持
- 更丰富的控件库

### Q8: 如何贡献代码？

**A**:
1. Fork 项目仓库
2. 创建功能分支
3. 提交更改
4. 创建 Pull Request
5. 加入 QQ 群 522222570 讨论

---

## 依赖与要求

### 系统要求

| 组件 | 要求 |
|-----|------|
| 操作系统 | Windows 8 / 10 / 11 |
| 处理器 | x64 架构 |
| 内存 | 建议 4GB 以上 |
| 显卡 | 支持 Direct2D |

### 开发环境

| 组件 | 版本要求 |
|-----|---------|
| Visual Studio | 2022 或更高版本 |
| Windows SDK | 10.0.17763.0 或更高 |
| C++ 标准 | C++20 |
| WebView2 SDK | 1.0.x 或更高 |

### NuGet 依赖

```
Microsoft.Web.WebView2
```

### 项目依赖关系

- `CUI` / `CUI_Legacy` 依赖仓库内的 `D2DGraphics/`
- `CUITest` / `CUITest_Legacy` 已将原先依赖 `Utils` 的测试辅助能力内聚到各自项目中，不再依赖 `Utils`
- `CuiDesigner` 当前依赖 `CUI_Legacy` 与 `Utils`
- `Utils` 现主要作为设计器及相关工程的工具库存在，不再是 `CUI` / `CUI_Legacy` / `CUITest` / `CUITest_Legacy` 的前置依赖

### 第三方组件

| 组件 | 用途 | 许可证 |
|-----|------|-------|
| nanosvg | SVG 渲染 | MIT |
| WebView2 | Web 内容嵌入 | 微软 EULA |

---

## 社区与支持

### 交流社区

- **QQ 群**: 522222570
- **GitHub Issues**: 报告 Bug 和功能请求

### 许可证

本项目采用 **AFL 3.0** (Academic Free License v3.0) 许可证开源。

### 致谢

感谢所有贡献者和社区成员的支持！

---

## 附录

### A. 控件属性速查

| 控件 | 核心属性 | 核心事件 |
|-----|---------|---------|
| Button | Text, Image, SizeMode | OnMouseClick |
| Label | Text, Font, ForeColor | - |
| TextBox | Text, MaxLength, ReadOnly | OnTextChanged, OnCharInput |
| ComboBox | Text, Items, SelectedIndex | OnSelectionChanged |
| CheckBox | Checked, Text | OnChecked |
| Slider | Value, Min, Max | OnValueChanged |
| GridView | Columns, Rows, Selection | OnSelectionChanged |
| TreeView | Root, SelectedNode | OnSelectionChanged |
| WebBrowser | Url, Html | OnNavigationCompleted |
| MediaPlayer | Volume, Position, Duration | OnMediaOpened, OnPositionChanged |

### B. 布局属性速查

| 属性 | 说明 | 适用控件 |
|-----|------|---------|
| Margin | 外边距 | 所有控件 |
| Padding | 内边距 | 容器控件 |
| DockPosition | 停靠位置 | DockPanel |
| GridRow/GridColumn | 网格位置 | GridPanel |
| AnchorStyles | 锚定样式 | 所有控件 |
| HAlign/VAlign | 对齐方式 | 布局容器 |
| RelativeConstraints | 相对约束 | RelativePanel |

### C. 颜色常量

```cpp
namespace Colors {
    // 基础颜色
    ColorF White;
    ColorF Black;
    ColorF Red;
    ColorF Green;
    ColorF Blue;
    
    // 系统颜色
    ColorF LightGray;
    ColorF DarkGray;
    ColorF Gray;
    ColorF Silver;
    
    // 扩展颜色
    ColorF Transparent;
    ColorF Accent;
    ColorF Error;
    ColorF Warning;
    ColorF Success;
}
```
