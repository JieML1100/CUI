# CUI - 现代化 Windows GUI 框架

<div align="center">

一个基于 Direct2D 和 DirectComposition 的现代化 Windows 原生 GUI 框架

**现已包含可视化设计器** 🎨

[![License](https://img.shields.io/badge/License-AFL%203.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-brightgreen.svg)](https://www.microsoft.com/windows)
[![C++](https://img.shields.io/badge/C%2B%2B-20-orange.svg)](https://isocpp.org/)
[![Designer](https://img.shields.io/badge/Designer-Included-success.svg)](#-可视化设计器)

</div>

---

## ✨ 重要更新

**🎉 CUI Designer 现已发布！**

全新的可视化 UI 设计器让你能够通过拖放方式设计界面，自动生成 C++ 代码。不再需要手写坐标和属性设置，极大提升开发效率！

- ✅ 拖放式控件布局
- ✅ 可视化属性编辑
- ✅ 实时预览
- ✅ 自动生成 C++ 代码
- ✅ 支持所有 CUI 控件
- ✅ 智能吸附和对齐

[查看设计器文档](#-可视化设计器) | [快速开始](#-快速开始)

---

## 📖 目录

- [简介](#-简介)
- [特性](#-特性)
- [依赖项](#-依赖项)
- [架构](#-架构)
- [控件列表](#-控件列表)
- [可视化设计器](#-可视化设计器) 🆕
- [快速开始](#-快速开始)
- [项目结构](#-项目结构)
- [编译要求](#-编译要求)
- [示例代码](#-示例代码)
- [许可证](#-许可证)

## 🎯 简介

CUI 是一个现代化的 Windows 原生 GUI 框架，采用 Direct2D 进行硬件加速渲染，使用 DirectComposition 实现流畅的窗口合成。该框架提供了丰富的 UI 控件和事件系统，适合开发高性能的桌面应用程序。

**全新设计器**：CUI Designer 是配套的可视化 UI 设计工具，提供拖放式界面设计体验，自动生成 C++ 代码，让开发者像使用 Visual Studio WinForms 设计器一样轻松创建现代化的桌面应用界面。🆕

### 核心优势

- **硬件加速渲染**：基于 Direct2D，充分利用 GPU 加速
- **现代化窗口合成**：使用 DirectComposition 实现流畅动画和透明效果
- **丰富的控件库**：内置 25+ 常用 UI 控件
- **可视化设计器**：所见即所得的拖放式 UI 设计工具，自动生成代码 🆕
- **事件驱动架构**：完善的事件系统，支持各类用户交互
- **WebView2 集成**：支持嵌入现代化 Web 内容
- **自定义渲染**：所有控件支持自定义外观和行为

## ✨ 特性

- ✅ **Direct2D 渲染引擎**：高性能 2D 图形渲染
- ✅ **DirectComposition 合成**：分层窗口合成，支持透明和动画
- ✅ **现代化布局系统**：StackPanel、GridPanel、DockPanel、WrapPanel、RelativePanel 五种布局容器
- ✅ **可视化设计器**：所见即所得的 UI 设计工具，拖放式控件布局，自动生成 C++ 代码 🆕
- ✅ **SVG 支持**：内置 nanosvg，支持 SVG 图像渲染
- ✅ **完整事件系统**：鼠标、键盘、焦点、拖放等事件
- ✅ **IME 输入支持**：完整支持中文输入法
- ✅ **自定义控件**：易于扩展的控件基类
- ✅ **系统托盘图标**：支持托盘图标和上下文菜单
- ✅ **任务栏集成**：支持任务栏进度显示
- ✅ **WebView2 集成**：嵌入 Chromium 内核的现代浏览器
- ✅ **灵活的定位系统**：支持绝对定位、Anchor 锚点、Margin 边距

## 📦 依赖项

CUI 框架依赖于两个核心库：

### 1. CppUtils/Graphics

图形渲染库，提供以下功能：

- **Direct2D 封装**：`Graphics1` 类封装了 Direct2D 渲染上下文
- **工厂类**：`Factory` 提供 Direct2D/DirectWrite 资源创建
- **颜色系统**：`Colors` 提供预定义颜色常量
- **字体管理**：`Font` 类封装字体创建和管理
- **位图处理**：`BitmapSource` 处理图像资源

主要类型：
```cpp
- D2DGraphics1      // Direct2D 渲染上下文
- Factory           // D2D/DWrite 工厂
- Colors            // 颜色常量
- Font              // 字体管理
- BitmapSource      // 位图源
```

### 2. CppUtils/Utils

工具库，提供以下功能：

- **事件系统**：`Event<T>` 模板类，类型安全的事件回调
- **集合类型**：`List<T>`、`Dictionary<K,V>` 容器
- **字符串工具**：`StringHelper` 字符串格式化和操作
- **注册表访问**：`RegistryKey` Windows 注册表操作
- **事件参数**：`MouseEventArgs`、`KeyEventArgs` 等

主要类型：
```cpp
- Event<T>          // 事件模板
- List<T>           // 列表容器
- Dictionary<K,V>   // 字典容器
- StringHelper      // 字符串工具
- RegistryKey       // 注册表访问
- MouseEventArgs    // 鼠标事件参数
- KeyEventArgs      // 键盘事件参数
```

### 3. 外部依赖

- **WebView2**：Microsoft.Web.WebView2 (v1.0.3650.58)
- **Windows SDK**：Windows 10 SDK
- **DirectX**：Direct2D、DirectWrite、DirectComposition
- **nanosvg**：SVG 解析和渲染（已包含）

## 🏗 架构

### 核心类层次结构

```
Application
    └─ Forms (Dictionary<HWND, Form*>)

Form (窗口)
    ├─ Controls (List<Control*>)
    ├─ ForegroundControl (置顶控件)
    ├─ MainMenu (主菜单)
    ├─ Render (D2DGraphics1*)
    ├─ OverlayRender (D2DGraphics1*)
    └─ _dcompHost (DCompLayeredHost*)

Control (控件基类)
    ├─ Parent (Control*)
    ├─ ParentForm (Form*)
    ├─ Children (List<Control*>)
    └─ Events (各类事件)

DCompLayeredHost (合成管理)
    ├─ Base Visual (主渲染层)
    ├─ Web Visual (WebView2 容器层)
    └─ Overlay Visual (覆盖层)
```

### 渲染流程

1. **事件循环**：`Form::DoEvent()` 处理 Windows 消息
2. **消息分发**：`WINMSG_PROCESS` 将消息转发给相应的 Form 和 Control
3. **更新绘制**：`Form::Update()` 调用所有控件的 `Update()` 方法
4. **Direct2D 渲染**：通过 `D2DGraphics1` 渲染到 DXGI SwapChain
5. **合成提交**：`DCompLayeredHost::Commit()` 提交 DirectComposition

## 🎨 控件列表

### 基础控件

| 控件 | 说明 | 特性 |
|------|------|------|
| **Label** | 文本标签 | 自动尺寸计算 |
| **Button** | 按钮 | 支持圆角、悬停效果 |
| **TextBox** | 单行文本框 | IME 支持、选择、滚动 |
| **PasswordBox** | 密码框 | 字符掩码显示 |
| **RichTextBox** | 多行文本框 | 虚拟化、大文本优化 |
| **RoundTextBox** | 圆角文本框 | 继承自 TextBox |

### 选择控件

| 控件 | 说明 | 特性 |
|------|------|------|
| **CheckBox** | 复选框 | 双态选择 |
| **RadioBox** | 单选框 | 互斥选择 |
| **Switch** | 开关 | 现代化切换控件 |
| **ComboBox** | 下拉框 | 下拉列表选择 |
| **Slider** | 滑块 | 数值选择、步进支持 |

### 容器控件

| 控件 | 说明 | 特性 |
|------|------|------|
| **Panel** | 面板 | 容器控件 |
| **TabControl** | 标签页 | 多页切换 |
| **TabPage** | 标签页面 | TabControl 子页面 |

### 布局控件 🆕

| 控件 | 说明 | 特性 |
|------|------|------|
| **StackPanel** | 堆叠面板 | 垂直/水平线性排列，支持间距 |
| **GridPanel** | 网格面板 | 行列布局，支持 Auto/Star/Pixel 尺寸 |
| **DockPanel** | 停靠面板 | Top/Bottom/Left/Right/Fill 停靠 |
| **WrapPanel** | 包裹面板 | 流式布局，自动换行/换列 |
| **RelativePanel** | 相对面板 | 相对定位，支持控件间约束关系 |

**布局属性支持**：
- `Margin` - 外边距（上下左右）
- `Padding` - 内边距
- `HorizontalAlignment` - 水平对齐（Left/Center/Right/Stretch）
- `VerticalAlignment` - 垂直对齐（Top/Center/Bottom/Stretch）
- `AnchorStyles` - 锚点（Top|Bottom|Left|Right 组合）
- `GridRow/GridColumn` - Grid 布局的行列位置
- `GridRowSpan/GridColumnSpan` - Grid 布局的跨行跨列
- `Dock` - Dock 布局的停靠位置

### 数据展示

| 控件 | 说明 | 特性 |
|------|------|------|
| **GridView** | 表格视图 | 排序、编辑、滚动、列宽调整 |
| **TreeView** | 树形视图 | 层级展示、展开/折叠 |
| **PictureBox** | 图片框 | 多种显示模式 |
| **ProgressBar** | 进度条 | 百分比显示 |

### 高级控件

| 控件 | 说明 | 特性 |
|------|------|------|
| **Menu** | 菜单 | 菜单栏、下拉菜单、子菜单 |
| **MenuItem** | 菜单项 | 支持分隔符、快捷键 |
| **ToolBar** | 工具栏 | 按钮容器 |
| **WebBrowser** | Web 浏览器 | WebView2 集成 |
| **NotifyIcon** | 托盘图标 | 系统托盘、上下文菜单 |
| **Taskbar** | 任务栏 | 进度显示 |

## 🎨 可视化设计器

CUI Designer 是专为 CUI 框架打造的可视化 UI 设计工具，提供类似 Visual Studio WinForms 设计器的拖放式界面设计体验，极大提升开发效率。

### 设计器特性

| 特性 | 说明 |
|------|------|
| **拖放式设计** | 从工具箱拖拽控件到画布即可创建 UI |
| **所见即所得** | 实时预览设计效果，与运行时渲染一致 |
| **属性编辑器** | 可视化编辑控件所有属性（位置、大小、颜色、文本等）|
| **智能吸附** | 网格吸附、参考线吸附，精确对齐控件 |
| **多选操作** | 支持框选、Ctrl 多选、批量调整 |
| **嵌套布局** | 支持容器控件嵌套，Panel、StackPanel、GridPanel 等 |
| **代码生成** | 自动生成完整的 C++ 头文件和源文件 |
| **保存/加载** | 设计文件保存为 JSON 格式，可重复编辑 |

### 设计器界面布局

```
┌────────────────────────────────────────────────────────────┐
│  [新建] [打开] [保存] [导出代码] [删除]          信息栏   │
├──────────┬──────────────────────────────┬──────────────────┤
│          │                              │                  │
│ 工具箱   │       设计画布               │   属性编辑器     │
│          │                              │                  │
│ Button   │  ┌────────────────────┐      │  名称: button1   │
│ Label    │  │  Form Title        │      │  类型: Button    │
│ TextBox  │  │ ┌────────────────┐ │      │  ───────────────│
│ CheckBox │  │ │ Button1        │ │      │  位置: 10, 10    │
│ ComboBox │  │ └────────────────┘ │      │  大小: 100, 30   │
│ Panel    │  │ ┌────────────────┐ │      │  文本: Button1   │
│ ...      │  │ │ Label1         │ │      │  背景色: ...     │
│          │  │ └────────────────┘ │      │  前景色: ...     │
│          │  └────────────────────┘      │  字体: ...       │
└──────────┴──────────────────────────────┴──────────────────┘
```

### 使用设计器

#### 1. 创建新设计

```
1. 启动 CuiDesigner.exe
2. 点击"新建"按钮创建空白窗体
3. 从工具箱拖拽控件到画布
4. 在属性编辑器中调整控件属性
5. 保存为 .json 设计文件
```

#### 2. 支持的控件

设计器工具箱包含以下控件：

**基础控件**：Button, Label, TextBox, RichTextBox, PasswordBox, RoundTextBox  
**选择控件**：CheckBox, RadioBox, Switch, ComboBox, Slider  
**容器控件**：Panel, TabControl, TabPage  
**布局控件**：StackPanel, GridPanel, DockPanel, WrapPanel, RelativePanel  
**数据展示**：GridView, TreeView, PictureBox, ProgressBar  
**其他控件**：WebBrowser, Menu, ToolBar

#### 3. 设计画布操作

| 操作 | 说明 |
|------|------|
| **选择** | 单击控件选中，Ctrl+单击多选，拖拽框选 |
| **移动** | 拖拽选中的控件移动位置，自动吸附网格/参考线 |
| **调整大小** | 拖拽选择框的 8 个控制点调整控件尺寸 |
| **删除** | 选中后按 Delete 或点击工具栏删除按钮 |
| **网格吸附** | 默认启用 10px 网格，移动时自动对齐 |
| **参考线** | 移动时自动显示与其他控件的对齐参考线 |

#### 4. 属性编辑器

属性编辑器提供所有可编辑属性：

**通用属性**：
- 名称（用于生成变量名）
- 位置（X, Y）
- 大小（Width, Height）
- 文本内容
- 可见性、启用状态

**外观属性**：
- 背景色、前景色、边框色
- 字体、字号、粗体/斜体
- 圆角半径、边框宽度

**布局属性**：
- Margin（外边距）
- Padding（内边距）
- HorizontalAlignment（水平对齐）
- VerticalAlignment（垂直对齐）
- AnchorStyles（锚点）
- Dock（停靠位置）

**特殊属性**：
- GridRow/GridColumn（Grid 布局）
- GridRowSpan/GridColumnSpan（跨行跨列）
- 容器特有属性（Orientation, Spacing 等）

#### 5. 集合编辑器

对于包含子项的控件，设计器提供专用编辑器：

- **ComboBox Items**：编辑下拉列表项
- **GridView Columns**：定义表格列（名称、宽度、类型）
- **TreeView Nodes**：编辑树节点层次结构
- **TabControl Pages**：添加/删除标签页
- **ToolBar Buttons**：配置工具栏按钮
- **GridPanel Definitions**：定义行列布局

#### 6. 代码生成

点击"导出代码"按钮，设计器会生成两个文件：

**MyForm.h**（头文件）：
```cpp
#pragma once
#include "GUI/Form.h"
#include "GUI/Button.h"
#include "GUI/Label.h"
// ... 其他 include

class MyForm : public Form
{
private:
    Button* button1;
    Label* label1;
    // ... 其他控件指针
    
public:
    MyForm();
    virtual ~MyForm() = default;
    void InitializeComponents();
};
```

**MyForm.cpp**（实现文件）：
```cpp
#include "MyForm.h"

MyForm::MyForm() : Form(L"我的窗体", {100, 100}, {800, 600})
{
    InitializeComponents();
}

void MyForm::InitializeComponents()
{
    // 创建并配置所有控件
    button1 = new Button(L"Button1", 10, 10, 100, 30);
    button1->BackColor = {0.2f, 0.4f, 0.8f, 1.0f};
    // ...
    this->AddControl(button1);
    
    label1 = new Label(L"Label1", 10, 50);
    // ...
    this->AddControl(label1);
}
```

生成的代码可以直接编译使用，或作为起点进一步定制。

#### 7. 设计文件格式

设计文件保存为 JSON 格式，包含：

```json
{
  "FormProperties": {
    "Text": "我的窗体",
    "Size": {"Width": 800, "Height": 600},
    "Location": {"X": 100, "Y": 100},
    "VisibleHead": true,
    "HeadHeight": 24
  },
  "Controls": [
    {
      "Type": "Button",
      "Name": "button1",
      "Location": {"X": 10, "Y": 10},
      "Size": {"Width": 100, "Height": 30},
      "Text": "Click Me",
      "BackColor": {"R": 0.2, "G": 0.4, "B": 0.8, "A": 1.0}
    }
  ]
}
```

### 设计器项目结构

```
CuiDesigner/
├── Designer.h/cpp              # 主设计器窗口
├── DesignerCanvas.h/cpp        # 设计画布（控件拖放、选择、移动）
├── ToolBox.h/cpp               # 工具箱面板
├── PropertyGrid.h/cpp          # 属性编辑器
├── CodeGenerator.h/cpp         # C++ 代码生成器
├── DesignerTypes.h/cpp         # 设计器数据类型
├── ComboBoxItemsEditorDialog.h/cpp     # ComboBox 项编辑器
├── GridViewColumnsEditorDialog.h/cpp   # GridView 列编辑器
├── TreeViewNodesEditorDialog.h/cpp     # TreeView 节点编辑器
├── TabControlPagesEditorDialog.h/cpp   # TabControl 页编辑器
├── ToolBarButtonsEditorDialog.h/cpp    # ToolBar 按钮编辑器
├── GridPanelDefinitionsEditorDialog.h/cpp # GridPanel 布局编辑器
└── main.cpp                    # 入口点
```

### 设计器技术细节

**核心类**：
- `Designer`：主窗口，协调各组件
- `DesignerCanvas`：画布，处理控件交互、渲染选择框
- `DesignerControl`：设计时控件包装，保存属性和状态
- `ToolBox`：工具箱，提供可拖拽的控件图标
- `PropertyGrid`：属性网格，动态生成属性编辑界面
- `CodeGenerator`：遍历控件树生成 C++ 代码

**关键功能实现**：
- **吸附算法**：移动/调整大小时检测与网格/参考线的距离，小于阈值则吸附
- **框选**：橡皮筋矩形与控件矩形相交检测
- **多选拖动**：记录所有选中控件的初始位置，应用统一偏移
- **嵌套容器**：通过父子关系递归处理，容器内控件使用相对坐标
- **属性序列化**：使用 JSON 库（nlohmann/json）保存/加载设计

## 🚀 快速开始

### 方式一：使用设计器（推荐）🆕

```
1. 启动 CuiDesigner.exe
2. 从工具箱拖拽控件设计界面
3. 编辑控件属性
4. 点击"导出代码"生成 C++ 文件
5. 将生成的 .h 和 .cpp 添加到项目中
6. 在 main.cpp 中创建和显示窗体
```

**main.cpp**：
```cpp
#include "MyForm.h"

int main()
{
    MyForm form;
    form.Show();
    
    while (Form::DoEvent())
    {
        if (Application::Forms.Count() == 0)
            break;
    }
    
    return 0;
}
```

### 方式二：手写代码

### 基本窗口

```cpp
#include "GUI/Form.h"

int main()
{
    // 创建窗口
    Form form(L"我的应用", {100, 100}, {800, 600});
    
    // 添加标签
    auto label = new Label(L"Hello, CUI!", 10, 10);
    form.AddControl(label);
    
    // 添加按钮
    auto button = new Button(L"点击我", 10, 50, 100, 30);
    button->OnMouseClick += [&](Control* sender, MouseEventArgs e) {
        MessageBoxW(NULL, L"按钮被点击！", L"提示", MB_OK);
    };
    form.AddControl(button);
    
    // 显示窗口
    form.Show();
    
    // 消息循环
    while (Form::DoEvent())
    {
        if (Application::Forms.Count() == 0)
            break;
    }
    
    return 0;
}
```

### 自定义控件

```cpp
class CustomLabel : public Label
{
public:
    CustomLabel(std::wstring text, int x, int y) 
        : Label(text, x, y) {}
    
    void Update() override
    {
        // 自定义渲染逻辑
        auto g = this->ParentForm->Render;
        auto rect = this->AbsRect;
        
        // 绘制渐变背景
        std::vector<D2D1_GRADIENT_STOP> stops = {
            {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}}
        };
        auto brush = g->CreateLinearGradientBrush(stops.data(), stops.size());
        g->FillRectangle(rect, brush);
        brush->Release();
        
        // 绘制文本
        g->DrawString(this->Text, this->Font, rect, 
                     D2D1_COLOR_F{1, 1, 1, 1});
    }
};
```

### 布局系统 🆕

CUI 提供了完整的布局管理系统，类似 WPF/UWP 的布局方式。

#### StackPanel - 线性堆叠布局

```cpp
#include "GUI/Layout/Layout.h"

// 垂直堆叠
auto stack = new StackPanel(10, 10, 380, 280);
stack->SetOrientation(Orientation::Vertical);
stack->SetSpacing(10);  // 间距

stack->AddControl(new Button(L"按钮 1", 0, 0, 200, 30));
stack->AddControl(new Button(L"按钮 2", 0, 0, 200, 30));
stack->AddControl(new Button(L"按钮 3", 0, 0, 200, 30));
```

#### GridPanel - 网格布局

```cpp
auto grid = new GridPanel(10, 10, 580, 380);

// 定义行和列
grid->AddRow(GridLength::Auto());        // 自动高度
grid->AddRow(GridLength::Star(1.0f));    // 占剩余空间的 1 份
grid->AddRow(GridLength::Pixels(50));    // 固定 50 像素

grid->AddColumn(GridLength::Star(1.0f)); // 占 1 份
grid->AddColumn(GridLength::Star(2.0f)); // 占 2 份

// 设置控件的位置
auto label = new Label(L"标题", 0, 0);
label->GridRow = 0;
label->GridColumn = 0;
label->GridColumnSpan = 2;  // 跨两列

grid->AddControl(label);
```

#### DockPanel - 停靠布局

```cpp
auto dock = new DockPanel(10, 10, 580, 380);
dock->SetLastChildFill(true);  // 最后一个控件填充剩余空间

auto toolbar = new Panel(0, 0, 580, 40);
toolbar->Dock = Dock::Top;

auto sidebar = new Panel(0, 0, 150, 300);
sidebar->Dock = Dock::Left;

auto content = new Panel(0, 0, 300, 300);
content->Dock = Dock::Fill;  // 填充剩余空间

dock->AddControl(toolbar);
dock->AddControl(sidebar);
dock->AddControl(content);
```

#### Anchor 和 Margin - 增强绝对定位

```cpp
// 右下角固定的按钮
auto btnOK = new Button(L"确定", 0, 0, 80, 30);
btnOK->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
btnOK->Margin = Thickness(0, 0, 10, 10);  // 距离右下角 10 像素

// 四边锚定，随窗口缩放
auto textBox = new TextBox(L"", 10, 10, 200, 100);
textBox->AnchorStyles = AnchorStyles::Left | AnchorStyles::Top | 
                        AnchorStyles::Right | AnchorStyles::Bottom;
textBox->Margin = Thickness(10, 10, 10, 50);
```

#### 对齐和边距

所有控件都支持：

```cpp
control->HorizontalAlignment = HorizontalAlignment::Center;  // 水平居中
control->VerticalAlignment = VerticalAlignment::Top;         // 顶部对齐
control->Margin = Thickness(10, 5, 10, 5);  // 外边距：左10, 上5, 右10, 下5
control->Padding = Thickness(5);            // 内边距：所有方向5像素
```

### 事件处理

```cpp
// 鼠标事件
button->OnMouseClick += [](Control* sender, MouseEventArgs e) {
    // 处理点击
};

button->OnMouseEnter += [](Control* sender, MouseEventArgs e) {
    // 鼠标进入
};

// 键盘事件
textbox->OnKeyDown += [](Control* sender, KeyEventArgs e) {
    if (e.Key == VK_RETURN) {
        // 处理回车键
    }
};

// 文本变化事件
textbox->OnTextChanged += [](Control* sender, std::wstring old, std::wstring newText) {
    // 文本改变
};

// 选中状态变化
checkbox->OnChecked += [](Control* sender) {
    bool checked = sender->Checked;
};
```

## 📁 项目结构

```
CUI/
├── CUI/                        # 主项目目录
│   ├── GUI/                    # GUI 框架核心
│   │   ├── Application.h/cpp   # 应用程序类
│   │   ├── Form.h/cpp          # 窗口类
│   │   ├── Control.h/cpp       # 控件基类
│   │   ├── DCompLayeredHost.h/cpp  # DirectComposition 管理
│   │   ├── Button.h/cpp        # 按钮控件
│   │   ├── Label.h/cpp         # 标签控件
│   │   ├── TextBox.h/cpp       # 文本框
│   │   ├── CheckBox.h/cpp      # 复选框
│   │   ├── RadioBox.h/cpp      # 单选框
│   │   ├── ComboBox.h/cpp      # 下拉框
│   │   ├── GridView.h/cpp      # 表格视图
│   │   ├── TreeView.h/cpp      # 树形视图
│   │   ├── Menu.h/cpp          # 菜单
│   │   ├── Panel.h/cpp         # 面板
│   │   ├── TabControl.h/cpp    # 标签页
│   │   ├── ProgressBar.h/cpp   # 进度条
│   │   ├── Slider.h/cpp        # 滑块
│   │   ├── Switch.h/cpp        # 开关
│   │   ├── WebBrowser.h/cpp    # Web 浏览器
│   │   ├── NotifyIcon.h/cpp    # 托盘图标
│   │   ├── Taskbar.h/cpp       # 任务栏
│   │   └── ...                 # 其他控件
│   ├── DemoWindow.h/cpp        # 示例窗口
│   ├── CustomControls.h/cpp    # 自定义控件示例
│   ├── main.cpp                # 程序入口
│   ├── nanosvg.h/cpp           # SVG 解析器
│   ├── CUI.vcxproj             # Visual Studio 项目文件
│   └── CUI.rc                  # 资源文件
├── CuiDesigner/                # 可视化设计器 🆕
│   ├── Designer.h/cpp          # 主设计器窗口
│   ├── DesignerCanvas.h/cpp    # 设计画布
│   ├── ToolBox.h/cpp           # 工具箱面板
│   ├── PropertyGrid.h/cpp      # 属性编辑器
│   ├── CodeGenerator.h/cpp     # 代码生成器
│   ├── DesignerTypes.h/cpp     # 设计器类型
│   ├── *EditorDialog.h/cpp     # 各种集合编辑器
│   ├── main.cpp                # 设计器入口
│   └── CuiDesigner.vcxproj     # 项目文件
├── CUITest/                    # 测试项目
│   ├── DemoWindow.h/cpp        # 演示窗口
│   ├── CustomControls.h/cpp    # 自定义控件
│   └── main.cpp                # 测试入口
├── CppUtils/                   # 依赖库
│   ├── Graphics/               # 图形库
│   └── Utils/                  # 工具库
├── CUI.sln                     # Visual Studio 解决方案
├── LICENSE                     # AFL 3.0 许可证
└── README.md                   # 本文档
```

## 🛠 编译要求

### 必需工具

- **Visual Studio 2022** (或 2019)
- **Windows 10 SDK** (10.0 或更高)
- **C++20** 标准支持
- **Platform Toolset**: v143

### NuGet 包

项目自动管理以下 NuGet 包：
- Microsoft.Web.WebView2 (1.0.3650.58)

### 编译步骤

1. **安装依赖**
   
   确保 CppUtils 库（Graphics 和 Utils）已编译为静态库并配置在包含路径中。
   
   CppUtils 库应包含以下头文件：
   ```
   CppUtils/
   ├── Graphics/
   │   ├── Colors.h
   │   ├── Font.h
   │   ├── Factory.h
   │   └── Graphics1.h
   └── Utils/
       ├── Event.h
       ├── Utils.h
       └── ...
   ```

2. **打开项目**
   
   使用 Visual Studio 打开 `CUI.sln`

3. **选择启动项目**
   
   - **CUI**：主框架库（通常作为静态库）
   - **CuiDesigner**：可视化设计器（可执行程序）🆕
   - **CUITest**：测试和演示项目（可执行程序）

4. **配置平台**
   
   选择编译配置：
   - Debug/Release
   - Win32/x64

5. **编译项目**
   
   按 `Ctrl+Shift+B` 或选择"生成" → "生成解决方案"

6. **运行程序**
   
   - 运行 CuiDesigner：右键设为启动项目，按 `F5`
   - 运行 CUITest：右键设为启动项目，按 `F5`

### 编译配置说明

#### Debug 配置
- 运行时库：`MultiThreadedDebugDLL` (Win32) / `MultiThreadedDebug` (x64)
- 优化：禁用
- 调试信息：完整

#### Release 配置
- 运行时库：`MultiThreaded`
- 优化：速度优先（x64：大小优先）
- 全程序优化：启用
- 链接器优化：启用 COMDAT 折叠和引用优化

### 链接库配置

项目自动链接以下库：
- `Imm32.lib` - IME 输入法支持
- `Dwmapi.lib` - Desktop Window Manager
- `Comctl32.lib` - 通用控件
- `WebView2LoaderStatic.lib` - WebView2 加载器
- `dcomp.lib` - DirectComposition（隐式）

## 📝 示例代码

### 完整示例：记事本应用

```cpp
#include "GUI/Form.h"

class NotepadApp : public Form
{
private:
    Menu* menu;
    RichTextBox* editor;
    
public:
    NotepadApp() : Form(L"记事本", {100, 100}, {800, 600})
    {
        // 创建菜单
        menu = new Menu(0, 0, 800, 28);
        auto fileMenu = menu->AddItem(L"文件");
        fileMenu->AddSubItem(L"新建", 1);
        fileMenu->AddSubItem(L"打开", 2);
        fileMenu->AddSubItem(L"保存", 3);
        fileMenu->AddSeparator();
        fileMenu->AddSubItem(L"退出", 4);
        
        menu->OnMenuCommand += [this](Control* sender, int id) {
            this->HandleMenuCommand(id);
        };
        this->AddControl(menu);
        
        // 创建编辑器
        editor = new RichTextBox(L"", 0, 28, 800, 572);
        editor->AllowMultiLine = true;
        editor->BackColor = Colors::White;
        this->AddControl(editor);
    }
    
    void HandleMenuCommand(int id)
    {
        switch (id)
        {
        case 1: // 新建
            editor->Text = L"";
            break;
        case 2: // 打开
            // 实现文件打开逻辑
            break;
        case 3: // 保存
            // 实现文件保存逻辑
            break;
        case 4: // 退出
            this->Close();
            break;
        }
    }
};

int main()
{
    NotepadApp app;
    app.Show();
    
    while (Form::DoEvent())
    {
        if (Application::Forms.Count() == 0)
            break;
    }
    
    return 0;
}
```

### WebBrowser 控件示例

```cpp
// 创建 WebBrowser
auto browser = new WebBrowser(10, 10, 780, 580);
form.AddControl(browser);

// 导航到 URL
browser->Navigate(L"https://www.bing.com");

// 设置 HTML 内容
browser->SetHtml(L"<html><body><h1>Hello, WebBrowser!</h1></body></html>");

// 执行 JavaScript
browser->ExecuteScriptAsync(L"document.title", 
    [](HRESULT hr, const std::wstring& result) {
        if (SUCCEEDED(hr)) {
            // 处理结果
        }
    });

// 查询 DOM 元素
browser->QuerySelectorAllOuterHtmlAsync(L".item",
    [](HRESULT hr, const std::wstring& jsonArray) {
        // 处理查询结果
    });
```

### GridView 数据绑定

```cpp
// 创建 GridView
auto grid = new GridView(10, 10, 780, 580);
form.AddControl(grid);

// 添加列
grid->Columns.Add(GridViewColumn(L"姓名", 200, ColumnType::Text, false));
grid->Columns.Add(GridViewColumn(L"年龄", 100, ColumnType::Text, true));
grid->Columns.Add(GridViewColumn(L"已选", 80, ColumnType::Check, false));

// 添加行
grid->ReSizeRows(100);
for (int i = 0; i < 100; i++)
{
    grid->Rows[i][0] = StringHelper::Format(L"用户%d", i + 1);
    grid->Rows[i][1] = StringHelper::Format(L"%d", 20 + i % 30);
    grid->Rows[i][2] = (i % 2 == 0);
}

// 处理选择变化事件
grid->SelectionChanged += [grid](Control* sender) {
    if (grid->SelectedRowIndex >= 0) {
        auto& row = grid->SelectedRow();
        // 处理选中的行
    }
};

// 处理复选框状态变化
grid->OnGridViewCheckStateChanged += [](GridView* gv, int col, int row, bool checked) {
    // 处理复选框变化
};

// 排序
grid->SortByColumn(1, true); // 按第2列升序排序
```

### 自定义窗口主题

```cpp
// 自定义窗口外观
form.BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f);
form.ForeColor = D2D1::ColorF(0.1f, 0.1f, 0.1f);
form.HeadHeight = 32;
form.VisibleHead = true;
form.CenterTitle = true;

// 自定义按钮样式
button->BackColor = D2D1::ColorF(0.2f, 0.6f, 1.0f);
button->ForeColor = Colors::White;
button->UnderMouseColor = D2D1::ColorF(0.3f, 0.7f, 1.0f);
button->CheckedColor = D2D1::ColorF(0.1f, 0.5f, 0.9f);
button->Round = 5.0f;
button->Boder = 2.0f;
```

## 🔧 高级特性

### DirectComposition 分层渲染

CUI 使用 DirectComposition 实现窗口分层合成：

```cpp
// Base Layer: 主 UI 渲染层（通过 D2DGraphics1 渲染）
// Web Layer: WebView2 容器层（CompositionController）
// Overlay Layer: 覆盖层（用于半透明浮层、光标等）
```

### 虚拟化支持

RichTextBox 支持大文本虚拟化：

```cpp
auto richTextBox = new RichTextBox(L"", 10, 10, 500, 400);
richTextBox->EnableVirtualization = true;      // 启用虚拟化
richTextBox->VirtualizeThreshold = 20000;      // 超过 2 万字符启用
richTextBox->BlockCharCount = 4096;            // 每块 4096 字符
richTextBox->MaxTextLength = 1000000;          // 最大 100 万字符

// 追加大量文本
for (int i = 0; i < 10000; i++) {
    richTextBox->AppendLine(L"这是第 " + std::to_wstring(i) + L" 行");
}
```

### IME 输入法支持

所有文本输入控件自动支持 IME：

```cpp
// TextBox, PasswordBox, RichTextBox, GridView 编辑模式
// 自动处理：
// - WM_IME_STARTCOMPOSITION
// - WM_IME_COMPOSITION
// - WM_IME_ENDCOMPOSITION
// - WM_IME_SETCONTEXT
```

### 拖放支持

```cpp
// 启用拖放
form.OnDropFile += [](Form* sender, List<std::wstring> files) {
    for (auto& file : files) {
        // 处理拖入的文件
    }
};

// 控件级别的拖放
pictureBox->OnDropFile += [](Control* sender, List<std::wstring> files) {
    if (files.Count() > 0) {
        // 加载图像
        auto bmp = sender->ParentForm->Render->LoadBitmap(files[0].c_str());
        sender->Image = bmp;
    }
};
```

## 🎨 主题和样式

### 预定义颜色

CUI 提供丰富的预定义颜色（通过 CppUtils/Graphics/Colors）：

```cpp
Colors::Black, Colors::White, Colors::Red, Colors::Green, Colors::Blue
Colors::Gray, Colors::LightGray, Colors::DarkGray
Colors::WhiteSmoke, Colors::Snow, Colors::GhostWhite
Colors::SkyBlue, Colors::SteelBlue, Colors::DarkSlateGray
// ... 还有更多
```

### 字体管理

```cpp
// 创建字体
auto myFont = new Font(L"微软雅黑", 16.0f);
myFont->Bold = true;
myFont->Italic = false;

// 应用到控件
label->Font = myFont;

// 全局默认字体
// GetDefaultFontObject() 返回 Arial 18pt
```

## 🐛 调试和性能

### 脏矩形更新

CUI 支持局部刷新以提高性能：

```cpp
// 仅更新控件区域
control->PostRender();

// 立即强制刷新
form.Invalidate(true);

// 指定区域刷新
form.Invalidate(RECT{x, y, x+w, y+h}, true);
```

### 动画和定时器

```cpp
// Form 内置动画定时器用于闪烁光标等
// _animIntervalMs: 动画间隔
// _animTimerId: 定时器 ID

// 控件可以通过 GetAnimatedInvalidRect 提供动画区域
bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override {
    if (_needAnimation) {
        outRect = _caretRectCache;
        return true;
    }
    return false;
}
```

## 📚 API 参考

### Application 类

```cpp
class Application
{
public:
    static Dictionary<HWND, Form*> Forms;        // 所有窗口
    static std::string ExecutablePath();         // 可执行文件路径
    static std::string StartupPath();            // 启动目录
    static std::string ApplicationName();        // 应用名称
    static std::string LocalUserAppDataPath();   // 本地应用数据
    static std::string UserAppDataPath();        // 用户应用数据
    static RegistryKey UserAppDataRegistry();    // 用户注册表
};
```

### Form 类

```cpp
class Form
{
public:
    // 属性
    PROPERTY(POINT, Location);              // 窗口位置
    PROPERTY(SIZE, Size);                   // 窗口大小
    PROPERTY(std::wstring, Text);           // 窗口标题
    PROPERTY(bool, TopMost);                // 置顶
    PROPERTY(bool, Enable);                 // 启用
    PROPERTY(bool, Visible);                // 可见
    
    // 成员
    HWND Handle;                            // 窗口句柄
    List<Control*> Controls;                // 子控件列表
    Control* Selected;                      // 当前选中控件
    Control* UnderMouse;                    // 鼠标下控件
    Menu* MainMenu;                         // 主菜单
    D2DGraphics1* Render;                   // 渲染上下文
    
    // 方法
    void Show();                            // 显示窗口
    void ShowDialog(HWND parent = NULL);    // 模态对话框
    void Close();                           // 关闭窗口
    template<typename T> T AddControl(T c); // 添加控件
    bool RemoveControl(Control* c);         // 移除控件
    
    // 静态方法
    static bool DoEvent();                  // 处理消息（非阻塞）
    static bool WaiteEvent();               // 等待消息（阻塞）
    
    // 事件
    FormMouseWheelEvent OnMouseWheel;
    FormMouseMoveEvent OnMouseMove;
    FormKeyDownEvent OnKeyDown;
    FormKeyUpEvent OnKeyUp;
    FormPaintEvent OnPaint;
    FormClosingEvent OnFormClosing;
    FormClosedEvent OnFormClosed;
    // ... 更多事件
};
```

### Control 类

```cpp
class Control
{
public:
    // 属性
    PROPERTY(POINT, Location);          // 位置
    PROPERTY(SIZE, Size);               // 大小
    PROPERTY(std::wstring, Text);       // 文本
    PROPERTY(D2D1_COLOR_F, BackColor);  // 背景色
    PROPERTY(D2D1_COLOR_F, ForeColor);  // 前景色
    PROPERTY(Font*, Font);              // 字体
    
    // 成员
    Form* ParentForm;                   // 父窗口
    Control* Parent;                    // 父控件
    List<Control*> Children;            // 子控件
    bool Enable;                        // 启用
    bool Visible;                       // 可见
    bool Checked;                       // 选中
    CursorKind Cursor;                  // 光标
    
    // 方法
    template<typename T> T AddControl(T c);  // 添加子控件
    void RemoveControl(Control* c);          // 移除子控件
    virtual void Update();                   // 渲染更新
    virtual void PostRender();               // 请求重绘
    
    // 事件
    MouseClickEvent OnMouseClick;
    MouseMoveEvent OnMouseMove;
    KeyDownEvent OnKeyDown;
    KeyUpEvent OnKeyUp;
    TextChangedEvent OnTextChanged;
    CheckedEvent OnChecked;
    // ... 更多事件
};
```

## 🤝 贡献

欢迎贡献代码、报告问题或提出改进建议！

### 贡献方式

1. Fork 本项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

### 代码规范

- 使用 C++20 标准
- 遵循现有代码风格
- 为新功能添加注释
- 测试新增代码


## 📄 许可证

本项目采用 **Academic Free License (AFL) v3.0** 许可证。

主要权限：
- ✅ 商业使用
- ✅ 修改
- ✅ 分发
- ✅ 私人使用

详见 [LICENSE](LICENSE) 文件。

## 🔗 相关链接

- **CppUtils**: 依赖库（需单独获取）
- **WebView2**: [Microsoft Edge WebView2](https://developer.microsoft.com/microsoft-edge/webview2/)
- **Direct2D**: [Direct2D 文档](https://docs.microsoft.com/windows/win32/direct2d/direct2d-portal)
- **DirectComposition**: [DirectComposition 文档](https://docs.microsoft.com/windows/win32/directcomp/directcomposition-portal)

## 📞 联系方式

如有问题或建议，欢迎通过以下方式联系：

- **Issues**: 在 GitHub 上提交 Issue
- **Pull Requests**: 提交改进代码

## 🙏 致谢

- **nanosvg**: SVG 解析库
- **Microsoft WebView2**: 现代 Web 内容嵌入
- **Direct2D/DirectComposition**: 高性能图形渲染

---

<div align="center">

**CUI** - 让 Windows 桌面应用开发更简单

⭐ 如果这个项目对你有帮助，请给个 Star！⭐

QQ群 = 522222570
</div>
