# CUI 使用指南

## 1. 先建立正确心智模型

`CUI` 不是 `.NET WinForms`，但它的 API 风格和 WinForms 很接近。

- 语言与运行时：`C++20 + Win32 + Direct2D + DirectComposition`
- 平台：仅 `Windows`
- 主窗口类型：`Form`
- 通用控件基类：`Control`
- 容器控件：`Panel`
- 子控件挂载方式：`AddControl(...)`
- 事件订阅方式：`OnXxx += lambda`
- 窗口循环：`Show()` 后手动调用 `Form::DoEvent()`

必须明确：

- 不要假设存在 `namespace CUI`
- 不要假设存在 `Application::Initialize()` 或 `Application::Run()`
- 不要把它写成 WinForms、WPF、Qt 或 MFC 的语义
- 不要先入为主地假设宿主项目一定是“链接 lib”或“一定是直接嵌源码”

如果文档、历史代码和头文件冲突，以这几类内容为准：

1. `CUITest/main.cpp`
2. `CUITest/DemoWindow.h`
3. `CUITest/DemoWindow.cpp`
4. `CUI/GUI/*.h`

## 2. 和 WinForms 的对应关系

| WinForms 心智模型 | CUI 中的对应物 |
| --- | --- |
| `Form` | `Form` |
| `Control` | `Control` |
| `Panel` | `Panel` |
| `Controls.Add(...)` | `AddControl(...)` |
| `FlowLayoutPanel` 风格布局 | `StackPanel` / `WrapPanel` |
| `TableLayoutPanel` 风格布局 | `GridPanel` |
| `Dock = Top/Left/Fill` | `DockPanel` + `DockPosition` |
| 事件 `Click += ...` | `OnMouseClick += ...` |
| 窗口关闭事件 | `OnClosing` / `OnFormClosing` / `OnFormClosed` |
| `Application.Run(form)` | `form.Show()` + 循环 `Form::DoEvent()` |
| 自定义控件覆写消息/绘制 | 继承控件，必要时覆写 `Update()` / `ProcessMessage()` |

## 3. Agent 先看哪些文件

如果你接手的是一个使用 CUI 的项目，优先阅读：

1. `CUITest/main.cpp`
2. `CUITest/DemoWindow.h`
3. `CUITest/DemoWindow.cpp`
4. `CUI/GUI/Form.h`
5. `CUI/GUI/Control.h`
6. `CUI/GUI/Panel.h`
7. `CUI/GUI/Layout/Layout.h`
8. `CUI/GUI/DefaultProcessMessageConvention.md`

这几处基本覆盖了：入口、消息循环、控件树、布局、事件、主题、自定义控件扩展规则。

## 4. 应用生命周期

一个 CUI 程序的真实入口模型是：

1. 在创建窗口前调用 `Application::EnsureDpiAwareness()`
2. 构造 `Form` 子类
3. 在构造函数里搭建控件树
4. 调用 `Show()`
5. 循环调用 `Form::DoEvent()`
6. 当 `Application::Forms.size() == 0` 时退出

最小骨架如下。注意：头文件路径要按宿主工程的组织方式调整，下面只展示 API 形态，不固化集成方式。

```cpp
// include 路径按宿主项目组织方式调整
#include "Form.h"

class MainWindow : public Form
{
public:
    MainWindow() : Form(L"My App", { 100, 100 }, { 900, 600 })
    {
        auto title = AddControl(new Label(L"Hello CUI", 20, 20));
        title->ForeColor = Colors::Black;

        auto button = AddControl(new Button(L"Click", 20, 60, 120, 32));
        button->OnMouseClick += [](Control* sender, MouseEventArgs e)
        {
            (void)e;
            MessageBoxW(sender->ParentForm->Handle, L"Clicked", L"CUI", MB_OK);
        };
    }
};

int main()
{
    Application::EnsureDpiAwareness();

    MainWindow fm;
    fm.Show();

    while (true)
    {
        Form::DoEvent();
        if (Application::Forms.size() == 0)
            break;
    }
    return 0;
}
```

要点：

- `Form` 本身不是放进 `Application::Run()` 的对象，而是显示后进入手动事件循环
- 示例工程使用 `main()`，也可以按宿主工程的入口约定封装，但消息泵语义不变
- `Form` 构造时会兜底处理 DPI；推荐仍显式先调用 `EnsureDpiAwareness()`

## 5. 控件树怎么搭

### 5.1 顶层控件

给窗口挂控件，用 `Form::AddControl(...)`：

```cpp
auto button = this->AddControl(new Button(L"OK", 20, 20, 100, 30));
```

### 5.2 容器内子控件

给容器挂子控件，用容器自己的 `AddControl(...)`：

```cpp
auto panel = this->AddControl(new Panel(20, 70, 400, 200));
panel->AddControl(new Label(L"In panel", 10, 10));
panel->AddControl(new TextBox(L"text", 10, 40, 160, 26));
```

### 5.3 所有权与生命周期

- CUI 的典型写法是 `new 控件` 后立刻交给 `AddControl(...)`
- 控件树建立后，父容器会持有子控件关系
- `Parent` 和 `ParentForm` 会在挂接时自动补齐
- `Form` 对 `Menu`、`ToolBar`、`StatusBar` 还有额外的主引用管理

对于资源：

- `Font` 通过属性设置时，默认按“控件接管所有权”处理
- 图片改为 `BitmapSource` 语义，控件内部会按需建立 D2D 位图缓存
- 若你不想让控件接管 `Font`，使用 `SetFontEx(..., false)`
- 若你要自定义布局引擎，`Panel::SetLayoutEngine(...)` 会接管传入指针并负责释放

## 6. 布局系统怎么理解

CUI 的布局不是单一模型，而是“两套并存”：

### 6.1 默认容器布局

普通 `Panel` 在没有设置布局引擎时，主要依赖这些属性：

- `Location`
- `Size`
- `Margin`
- `Padding`
- `HAlign`
- `VAlign`
- `AnchorStyles`

这更接近 WinForms 的“绝对位置 + 锚定/对齐”思路。

### 6.2 布局引擎容器

当 `Panel` 设置了 `LayoutEngine`，则走标准的 `Measure -> Arrange` 两阶段布局。

仓库内已经提供了这些高频容器：

| 容器 | 适合场景 | 关键 API |
| --- | --- | --- |
| `StackPanel` | 纵向/横向线性堆叠 | `SetOrientation()` `SetSpacing()` |
| `GridPanel` | 表单、二维网格 | `AddRow()` `AddColumn()` `GridRow/GridColumn` |
| `DockPanel` | 顶部栏、侧栏、填充区 | 子控件设 `DockPosition` |
| `WrapPanel` | 自动换行/换列 | `SetOrientation()` `SetItemWidth()` |
| `RelativePanel` | 约束式相对布局 | `SetConstraints()` |

此外还有两个非常常见的复合容器：

- `ScrollView`：内容超出时提供滚动视口
- `SplitContainer`：双面板 + 可拖动分隔条

### 6.3 布局时 Agent 应怎么选

- 简单页面、像素级摆放：先用 `Panel`
- 垂直表单、工具列：优先 `StackPanel`
- 行列明确：优先 `GridPanel`
- 典型“上中下”“左中右”：优先 `DockPanel`
- 卡片流、图标流：优先 `WrapPanel`
- 控件之间需要“在谁左边/下边/与谁对齐”：用 `RelativePanel`

### 6.4 布局属性

布局引擎会读取 `Control` 上的这些属性：

- `Margin`
- `Padding`
- `GridRow`
- `GridColumn`
- `GridRowSpan`
- `GridColumnSpan`
- `DockPosition`
- `MinSize`
- `MaxSize`

如果改动这些属性后界面没有马上变化，通常需要等待容器下一帧布局，或显式触发 `InvalidateLayout()` / `PerformLayout()`。

## 7. 事件模型

事件系统基于 `CppUtils::Event<>`，常见写法是：

```cpp
button->OnMouseClick += [this](Control* sender, MouseEventArgs e)
{
    (void)sender;
    (void)e;
    // handle click
};
```

常见控件事件：

- `OnMouseClick`
- `OnMouseDown`
- `OnMouseUp`
- `OnMouseMove`
- `OnMouseWheel`
- `OnChecked`
- `OnSelectionChanged`
- `OnValueChanged`
- `OnTextChanged`
- `OnDropFile`
- `OnDropText`
- `OnGotFocus`
- `OnLostFocus`

常见窗口事件：

- `OnClosing`
- `OnFormClosing`
- `OnFormClosed`
- `OnSizeChanged`
- `OnMoved`
- `OnThemeChanged`
- `OnCommand`

其中：

- `OnClosing` 的签名里带 `bool&`，可用于取消关闭
- 菜单、上下文菜单等命令型入口常走 `OnCommand` 或 `OnMenuCommand`
- `sender` 一般就是触发事件的控件本身

## 8. 常用控件分类

### 8.1 基础输入与显示

- `Label`
- `LinkLabel`
- `Button`
- `TextBox`
- `RichTextBox`
- `PasswordBox`
- `ComboBox`
- `CheckBox`
- `RadioBox`
- `Switch`
- `Slider`
- `DateTimePicker`

### 8.2 容器与组织

- `Panel`
- `GroupBox`
- `TabControl`
- `TabPage`
- `ScrollView`
- `SplitContainer`

### 8.3 数据与树形

- `TreeView`
- `GridView`

### 8.4 媒体与内容嵌入

- `PictureBox`
- `WebBrowser`
- `MediaPlayer`

### 8.5 系统集成

- `Menu`
- `ContextMenu`
- `ToolBar`
- `StatusBar`
- `NotifyIcon`
- `Taskbar`
- `ToolTip`

如果 Agent 不知道某个控件如何使用，先在 `示例/DemoWindow.cpp` 搜这个控件名，通常能直接找到真实用法。

## 9. 主题、颜色与外观

`Form` 级主题入口是：

- `ApplyThemeFrame(...)`
- `GetThemeName()`

示例 `DemoWindow.cpp` 展示了一个完整的主题切换做法：

- 定义主题色板
- 用 `ApplyThemeFrame(...)` 更新窗口框架
- 遍历控件树同步应用颜色
- 在 `OnThemeChanged` 或切换事件里刷新控件外观

对 Agent 来说最重要的是：

- CUI 没有强制统一主题系统
- 很多控件外观就是直接改 `BackColor`、`ForeColor`、`BolderColor`
- 项目内若已有主题约定，应优先复用现有模式，而不是另起一套皮肤框架

## 10. 拖放、IME、Web、媒体与系统能力

这几个能力是 CUI 相比简单自绘 UI 更容易被忽略的部分：

- `TextBox` / `RichTextBox` / `PasswordBox` 等已考虑 IME 输入
- `Form` 和控件都支持文件/文本拖放事件
- `WebBrowser` 基于 `WebView2`
- `MediaPlayer` 负责媒体播放
- `NotifyIcon` 与 `Taskbar` 可做系统托盘和任务栏状态集成

使用这些能力前，先确认宿主项目是否真的启用了相应依赖，尤其是 `WebView2`。

## 11. 自定义控件应该怎么做

### 11.1 简单扩展

如果只是：

- 改绘制
- 改少量状态
- 在默认点击流程前后插入逻辑

优先：

1. 继承现有控件
2. 覆写 `Update()`、绘制相关逻辑或默认 hook
3. 复用基类 `ProcessMessage()`

### 11.2 复杂扩展

只有在下面这些场景里，才优先考虑重写 `ProcessMessage()`：

- 需要自己管理消息路由
- 需要维护交互状态机
- 需要坐标空间换算
- 需要复杂键盘/鼠标/选区/滚动逻辑
- 需要和 `Form`、系统组件或外部宿主深度协作

具体判断标准看：

- `GUI/DefaultProcessMessageConvention.md`

一句话原则：

- “只是改点击行为”不要重写整套消息分发
- “真的要接管交互状态机”才重写 `ProcessMessage()`

设计器工作流大致是：

1. 拖放控件
2. 编辑属性
3. 保存设计文件
4. 生成 C++ 代码
5. 运行时仍由 `CUI` 负责真正显示与交互

对 Agent 的建议是：

- 如果宿主项目已经用设计器生成代码，就沿着生成代码继续维护
- 如果宿主项目纯手写 UI，不要强推设计器
- 设计文件和生成代码通常都应纳入版本控制

## 12. 集成方式不要写死

其他项目接入 CUI 时，可能出现这些形态：

- 直接把 `CUI` 与 `Graphics` 源码纳入工程
- 预编译成静态库后引用
- 用已有的 monorepo / vendor 目录嵌入
- 只抽取部分控件和运行时代码

所以 Agent 在修改宿主项目时应该先识别：

1. 头文件根路径是什么
2. `Graphics` 是源码依赖还是二进制依赖
4. 是否存在本地封装层或二次包装

不要直接把当前仓库的相对路径拷到其他项目里。

## 13. Agent 实战规则

当你在其他项目里实现 CUI 界面时，优先遵循下面这组规则：

1. 先找该项目里已有的 `Form` 子类和 `AddControl(...)` 写法。
2. 先确认该项目用的是绝对布局、布局容器，还是两者混用。
3. 事件优先沿用该项目已有的 lambda / 成员函数绑定风格。
4. 如果已有主题系统，继续复用 `BackColor` / `ForeColor` / `ApplyThemeFrame(...)` 的现有模式。
5. 需要新页面时，优先参考 `示例/DemoWindow.cpp` 里最接近的那一段控件组合。
6. 需要自定义控件时，先判断能否只覆写默认 hook，而不是直接重写 `ProcessMessage()`。

## 14. 最短结论

把 CUI 当成“原生 C++ 的、API 风格接近 WinForms 的 Windows GUI 框架”来用，通常不会错。

最重要的四件事只有这些：

1. 用 `Form` 做顶层窗口，用 `Control` / `Panel` 组织控件树
2. 用 `AddControl(...)` 搭 UI
3. 用 `OnXxx += ...` 绑事件
4. 用 `Show()` + `Form::DoEvent()` 跑消息循环

如果你需要进一步确认具体控件的真实用法，直接看 `示例/DemoWindow.cpp`，它就是当前仓库最接近“官方参考实现”的示例。
