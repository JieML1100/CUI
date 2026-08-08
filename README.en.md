# CUI

[English](README.en.md) | [简体中文](README.md)

CUI is a native Windows UI framework that implements WPF semantics in C++20 on top of Direct2D and
DirectComposition. Its goal is a declarative property, resource, template, routed-event, input, layout,
rendering, and automation system—not a WinForms-style control collection.

## Non-negotiable contract

- XAML schema defines control types, properties, events, commands, template parts, and namescopes.
- C++ does not register control types into XAML. It implements native behavior hosts, handlers, platform
  messages, input, and rendering.
- `RuntimeTypeId(namespace URI + local name)` is the sole declarative type identity. `UIClass` is an internal
  behavior-host discriminator only.
- Dynamic Runtime, Designer preview, and static CodeGen consume the same XAML document and schema.
- Unrelated legacy APIs are removed rather than retained through aliases, dual writes, or compatibility fallbacks.

## Architecture

```text
XAML / Schema
    ├─ QNames, dependency and attached properties, events, commands
    ├─ ResourceDictionary, Style, Template, VisualState
    └─ Binding, DataTemplate, ItemsPanelTemplate, namescopes
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

Effective property precedence is:

```text
Animation > Local > VisualState > Template > Style > Theme > Inherited > Default
```

Binding, DynamicResource, TemplateBinding, and Animation are expression identities inside value slots; they
do not bypass the property system through duplicate fields.

## Minimal XAML

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

`_S` is the WPF AccessText marker and `__` renders a literal underscore. Appearance uses Brush properties;
there are no `BackColor`, `ForeColor`, `BorderColor`, or general writable `AccessKey` properties.

The C++ host creates an `Application`, loads XAML, registers the `OnSave` behavior, supplies DataContext,
and enters `application.Run(window)`. Application code does not build a second authored tree with
`AddControl(new ...)`.
See [`CuiRuntimeSample/main.cpp`](CuiRuntimeSample/main.cpp) for dynamic mounting and event registration,
[`CuiStaticGeneratedSample`](CuiStaticGeneratedSample) for static lowering, and [`CUITest`](CUITest) for the
feature gallery.

## Repository layout

- `CUI/`: element layers, layout, input, commands, automation, rendering, and native behavior hosts.
- `CuiRuntime/`: XAML schema, materialization, RuntimeDocument, hot reload, and event registration.
- `CuiDesigner/`: canonical XAML editing, preview, property/event design, and static CodeGen.
- `CUITest/`: declarative feature demonstrations and validation gates.
- `CUICoreTests/`: semantic and regression tests.
- `D2DGraphics/`: Direct2D/DirectComposition graphics layer.

## Build and verify

Build `Debug|x64` with Visual Studio 2022, MSVC v143, and a Windows SDK:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  CUI.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 `
  /p:LinkIncremental=false /verbosity:minimal
```

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