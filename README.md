# CUI

[简体中文](README.md) | [English](README.en.md)

CUI 是以 Direct2D、DirectComposition 和 C++20 实现的 Windows 原生 WPF 语义 UI 框架。目标不是复刻
WinForms 控件集合，而是建立 C++ 阵营的声明式属性、资源、模板、路由事件、输入、布局、渲染与自动化体系。

## 核心约定

- 控件类型、属性、事件、命令、模板部件和名称作用域全部由 XAML Schema 定义。
- C++ 不向 XAML 注册控件类型；C++ 只实现 native behavior host、事件/命令处理、平台消息、输入和渲染。
- `RuntimeTypeId(namespace URI + local name)` 是声明类型的唯一身份；`UIClass` 只是内部 behavior host 判别器。
- 动态 Runtime、Designer 预览和静态 CodeGen 消费同一 XAML 文档及 Schema。
- 不兼容与新方向无关的 Legacy API；旧名称或旧语义应删除，不能用别名、双写或 fallback 继续传播。

## 当前架构

```text
XAML / Schema
    ├─ 类型 QName、依赖属性、attached property、事件、命令
    ├─ ResourceDictionary、Style、Template、VisualState
    └─ Binding、DataTemplate、ItemsPanelTemplate、名称作用域
             │
             ▼
Designer / Parser / Canonical Serializer / CodeGen
             │
             ▼
Runtime Materializer
             │
             ▼
DispatcherObject → DependencyObject → Visual → UIElement
                 → FrameworkElement → Control
             │
             ├─ Measure / Arrange
             ├─ InputManager / FocusManager / TextCompositionManager
             ├─ RoutedEvent / RoutedCommand
             ├─ AutomationProperties / AutomationPeer
             └─ PresentationScene / Direct2D / DirectComposition
```

属性有效值优先级为：

```text
Animation > Local > VisualState > Template > Style > Theme > Inherited > Default
```

Binding、DynamicResource、TemplateBinding 和 Animation 是值槽中的表达式身份，不是旁路属性系统的第二份字段。

## 最小 XAML

```xml
<Window xmlns="urn:cui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        x:Class="Sample.MainWindow"
        Title="CUI" Width="640" Height="360">
  <Window.Resources>
    <SolidColorBrush x:Key="Accent">#FF2F6FE4</SolidColorBrush>
    <Style TargetType="Button">
      <Setter Property="Background" Value="{StaticResource Accent}" />
    </Style>
  </Window.Resources>

  <StackPanel Margin="20">
    <TextBlock Text="C++ WPF" FontSize="24" />
    <TextBox x:Name="nameEditor" Text="{Binding Name, Mode=TwoWay}" />
    <Button x:Name="saveButton"
            Content="_Save"
            Click="OnSave"
            AutomationProperties.Name="Save" />
  </StackPanel>
</Window>
```

`_S` 是 WPF AccessText 助记键标记，`__` 表示字面下划线。外观使用 Brush 属性；不存在
`BackColor`、`ForeColor`、`BorderColor` 或通用可写 `AccessKey` 属性。

C++ 宿主负责创建 `Application`、加载 XAML、注册 `OnSave` behavior、提供 DataContext，然后进入
`application.Run(window)`；应用代码不再
用 `AddControl(new ...)` 建立另一棵作者树。完整动态挂载和事件注册见
[`CuiRuntimeSample/main.cpp`](CuiRuntimeSample/main.cpp)，静态 lowering 见
[`CuiStaticGeneratedSample`](CuiStaticGeneratedSample)，综合特性展厅见 [`CUITest`](CUITest)。

## 项目结构

- `CUI/`：元素基类、布局、输入、命令、自动化、渲染和 native behavior hosts。
- `CuiRuntime/`：XAML Schema、Materializer、RuntimeDocument、热重载和事件注册。
- `CuiDesigner/`：规范 XAML 编辑、预览、属性/事件设计与静态 CodeGen。
- `CUITest/`：声明式综合特性演示和验证门禁。
- `CUICoreTests/`：核心语义与回归测试。
- `D2DGraphics/`：Direct2D/DirectComposition 底层图形封装。

## 构建

使用 Visual Studio 2022、MSVC v143 和 Windows SDK，构建 `CUI.sln` 的 `Debug|x64`：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  CUI.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 `
  /p:LinkIncremental=false /verbosity:minimal
```

完整验证入口：

```powershell
.\x64\Debug\Designer.exe --self-test
.\x64\Debug\CUITest.exe --validate-xaml
.\x64\Debug\CUITest.exe --smoke-xaml
.\x64\Debug\CUITest.exe --render-smoke
.\x64\Debug\CuiRuntimeSample.exe
.\x64\Debug\CuiStaticGeneratedSample.exe
.\x64\Debug\CuiCodeGen.exe --version
.\x64\Debug\CUICoreTests.exe
```

## 架构文档

- [WPF 底层对齐状态、审计与验证记录](CUI_WPF_FOUNDATION_ALIGNMENT.md)

下一阶段先继续收敛剩余 DependencyProperty 身份/元数据、
InheritanceContext/Freezable 和结构元素基类；Application 实例生命周期已经进入底座，
`Application.xaml`/`StartupUri` 与通用 ResourceDictionary/MergedDictionaries 作为后续上层面接入。
随后再推进默认主题、Adorner、完整 Brush realization 与更深的 AutomationPeer 能力。
native fallback renderer 只是在默认模板覆盖完成前的内部后备，不是第二套公共外观模型。
