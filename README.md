# CUI

[简体中文](README.md) | [English](README.en.md)

CUI 是以 Direct2D、DirectComposition 和 C++20 实现的 Windows 原生 WPF 语义 UI 框架。目标不是复刻
WinForms 控件集合，而是建立 C++ 阵营的声明式属性、资源、模板、路由事件、输入、布局、渲染与自动化体系。

## 核心约定

- 控件类型、属性、事件、命令、模板部件和名称作用域全部由 XAML Schema 定义。
- C++ 不向 XAML 注册控件类型；C++ 只实现 native behavior host、事件/命令处理、平台消息、输入和渲染。
- Production 使用 collision-checked type/property token 与静态 C++ endpoint；QName / `RuntimeTypeId`
  只用于 Designer、CodeGen 和 Design compatibility 诊断，`UIClass` 只是内部 behavior host 判别器。
- Designer 预览、诊断和静态 CodeGen 消费同一 XAML 文档及 Schema；Production 只消费生成的 C++，
  不加载 authored XAML，也不按名称查找属性或 converter。
- 不兼容与新方向无关的 Legacy API；旧名称或旧语义应删除，不能用别名、双写或 fallback 继续传播。

## 当前架构

```text
Authored XAML / Schema
    ├─ 类型、依赖属性、attached property、事件、命令
    ├─ ResourceDictionary、Style、Template、VisualState
    └─ Binding、DataTemplate、ItemsPanelTemplate、名称作用域
             │
             ▼
Designer / Parser / Canonical Serializer / AOT CodeGen
       │                                  │
       │ Design compatibility             │ generated .g.h / .g.cpp
       ▼                                  ▼
Runtime Materializer                 Production CUI (pure C++)
       │                                  │
       └──────────────┬───────────────────┘
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

Production 构建先把 XAML 编译为 `.g.h/.g.cpp`。C++ 宿主负责创建 `Application`、连接生成的
`OnSave` handler、提供 typed DataContext，然后进入 `application.Run(window)`；进程不读取 authored
XAML，也不再用 `AddControl(new ...)` 建立另一棵作者树。Production 静态链路见
[`CuiStaticGeneratedSample`](CuiStaticGeneratedSample) 和
[`CuiAotCompileGate`](CuiAotCompileGate)；动态挂载样例
[`CuiRuntimeSample/main.cpp`](CuiRuntimeSample/main.cpp) 属于 Design compatibility，综合特性展厅见
[`CUITest`](CUITest)。

## 项目结构

- `CUI/`：元素基类、布局、输入、命令、自动化、渲染和 native behavior hosts。
- `CuiRuntime/`：Design compatibility 的 XAML Schema、Materializer、RuntimeDocument、热重载和事件注册；
  不属于 Production AOT 项目图。
- `CuiDesigner/`：规范 XAML 编辑、预览、属性/事件设计与静态 CodeGen。
- `CUITest/`：声明式综合特性演示和验证门禁。
- `CUICoreTests/`：核心语义与回归测试。
- `D2DGraphics/`：Direct2D/DirectComposition 底层图形封装。

## 构建

使用 Visual Studio 2022、MSVC v143 和 Windows SDK。日常可构建 `CUI.sln` 的 `Debug|x64`：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  CUI.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 `
  /p:LinkIncremental=false /verbosity:minimal
```

当前 checkout 的 solution 级 Debug 构建会在未迁移的 Design compatibility 样例
`CuiRuntimeSample` 中遇到已知的 `Event<void(Window*)>::args_type` 编译错误；Production AOT 主链使用
下面的 Release gate，不能把该 sample 失败误记为 AOT 回归：

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild .\CUI\CUI.vcxproj /t:Build /m:1 `
  /p:Configuration=Release /p:Platform=x64 /p:CuiRuntimeFlavor=Production
& $msbuild .\CuiAotCompileGate\CuiAotCompileGate.vcxproj /t:Build /m:1 `
  /p:Configuration=Release /p:Platform=x64
.\CuiStaticGeneratedSample\x64\Release\CuiStaticGeneratedSample.exe
```

主要验证入口：

```powershell
.\x64\Debug\Designer.exe --self-test
.\x64\Debug\CUITest.exe --validate-xaml
.\x64\Debug\CUITest.exe --smoke-xaml
.\x64\Debug\CUITest.exe --render-smoke
.\x64\Debug\CuiCodeGen.exe --version
.\CUICoreTests\x64\Debug\CUICoreTests.exe
```

### AOT 自定义 Binding converter

Production 不注册或按名称查找 converter。自定义 single/multi converter 通过构建期 catalog 声明，
生成的 C++ TU 直接包含实现头并调用 qualified factory：

```xml
<CuiBindingConverters Version="1">
  <Single Id="FormatValue" Include="BindingConverters.h"
          Factory="sample::CreateFormatValue"
          SourceKind="String" TargetKind="String" CanConvertBack="false" />
  <Multi Id="JoinValues" Include="BindingConverters.h"
         Factory="sample::CreateJoinValues"
         MinimumInputCount="2" TargetKind="String" CanConvertBack="false" />
</CuiBindingConverters>
```

命令行生成使用 `--converter-manifest <xml>`；MSBuild 项目在对应 `CuiCompile` 上设置：

```xml
<ConverterManifest>$(ProjectDir)BindingConverters.cui.xml</ConverterManifest>
```

catalog 会严格检查 ID、include、factory、value kind、ConvertBack 能力和输入数；失败直接终止 AOT
生成，不回退到运行时 registry。可运行契约见
[`CuiAotCompileGate/TypedConverterContract.cui.xaml`](CuiAotCompileGate/TypedConverterContract.cui.xaml)；
Window、ComponentDefinition、DataTemplate、ControlTemplate 四域的普通/MultiBinding、direct DP、
direct record 与 FindAncestor adapter 覆盖见
[`CuiAotCompileGate/BindingScopeContract.cui.xaml`](CuiAotCompileGate/BindingScopeContract.cui.xaml) 和
[`CuiAotCompileGate/VerifyBindingScopeBoundary.ps1`](CuiAotCompileGate/VerifyBindingScopeBoundary.ps1)。

生成组件的 dependency property 采用 accessor-owned static storage：Production 生成 numeric token，
不发布到名称 registry；Design 仍保留 XAML Schema 名称视图。可写/只读及 direct Binding 契约见
[`CuiAotCompileGate/DependencyPropertyStorageContract.cui.xaml`](CuiAotCompileGate/DependencyPropertyStorageContract.cui.xaml)。

Design-only converter registry、XamlSchema、动态 DP registry/storage、DependencyObject/Control 名称兼容
实现、content/collection authored strategy、Style mutable backend 以及 VisualState builder/name adapter
现已物理归入 `CuiRuntime`。源码、
三层 archive 与最终 MAP 边界可用
[`build/VerifyCuiA8fPhysicalBoundary.ps1`](build/VerifyCuiA8fPhysicalBoundary.ps1) 检查：Production
`CUI.lib` 和中间 `CUIDesignCore.lib` 不得拥有 Design symbol/object，`CuiRuntime.lib` 必须正向拥有九个
Design object 及各组精确成员定义；Closure/Full Production MAP 不得链接 Design/CuiRuntime/XmlLite 或动态
名称符号。A8f 封板时的完整 Core 是 429/441，12 个失败与前基线精确一致；随后独立的
runtime correctness 收尾已清零这组历史失败，当前完整 Core 为 **442/442**。Production AOT、
静态样例、Designer、MAP 和性能门均已按当前代码复验；性能聚合为 0 regressions，Closure/Full
只拉入 39/72 个 CUI object modules，Design/XML/name-registry trace 为 0。

## 架构文档

- [WPF 底层对齐状态、审计与验证记录](CUI_WPF_FOUNDATION_ALIGNMENT.md)

AOT 生成组件与 framework native DependencyProperty 的按需 static identity/storage、动态
registry/name compatibility、content/collection mixed strategy、Style mutable backend 与 VisualState Design backend
均已完成归位，A8f 迁移至此封板；精确边界和证据见
[A8f 最终收尾计划](CUI_AOT_FINAL_CLOSEOUT_PLAN.md)。A8f 当时保留的 12 项历史 runtime
行为失败已在迁移范围之外的独立 correctness 批次完成，当前仓库级 Core 回归为 **442/442**；
设计时 XAML、AOT 生成与纯 Production 运行时边界仍保持贯通。
InheritanceContext/Freezable、结构元素基类、`Application.xaml`/`StartupUri`、通用
ResourceDictionary/MergedDictionaries、默认主题和更深 AutomationPeer 能力都在迁移封板后作为独立
功能批次推进。native fallback renderer 只是在默认模板覆盖完成前的内部后备，不是第二套公共外观模型。
