# CUI Agent 开发规则

本文是当前开发入口。详细架构决议见 `CUI_WPF_ARCHITECTURE_DIRECTION.md`，当前清理边界见
`CONTROL_API_AUDIT.md`。旧提交、旧 README 或删除文件中的 API 不能作为兼容要求。

## 1. 目标

CUI 要实现高性能的 C++ WPF：XAML 是作者源，C++ 是行为、平台和渲染实现。任何新设计都先判断它属于声明层、
运行时语义层还是平台 realization，禁止把三层揉成一个控件类里的字段和消息分支。

## 2. 不可破坏的约定

1. 控件类型、依赖属性、attached property、事件、命令、模板部件和 namescope 由 XAML Schema 定义。
2. C++ 不向 XAML 注册控件类型；不得以 RTTI、构造器或 `UIClass` switch 发明声明能力。
3. `RuntimeTypeId(namespace URI + local name)` 是声明身份；`UIClass` 只选择 native behavior host。
4. 应用 UI 由 XAML 建立。不得恢复 `AddControl(new ...)` 作为公开作者工作流。
5. 动态 Materializer、Designer preview 和静态 CodeGen 必须消费同一 Schema/文档模型。
6. 不为旧 API “不炸”而增加别名、兼容读取、双字段、双事件或失败后回走 Legacy 的分支。
7. C++ 可挂接 routed event/command handler、平台消息、NativeSurface behavior 和 renderer realization；
   不能在 behavior 中重新声明控件的公共属性或事件。

## 3. 先读这些文件

1. `CUI_WPF_ARCHITECTURE_DIRECTION.md`
2. `CONTROL_API_AUDIT.md`
3. `CUITest/DemoWindow.cui.xaml`
4. `CUITest/DemoWindow.h` 与 `CUITest/DemoWindow.cpp`
5. `CuiRuntimeSample/main.cpp`
6. `CuiRuntime/XamlRuntimeSchema.cpp`
7. `CuiRuntime/Runtime/XamlObjectMaterializer.cpp`
8. `CUI/include/DependencyObject.h`、`UIElement.h`、`FrameworkElement.h`、`Control.h`
9. `CUI/include/Window.h`、`InputManager.h`、`FocusManager.h`、`AutomationPeer.h`
10. `CUI/include/PresentationScene.h` 与 `PresentationRenderHost.h`

## 4. 层次边界

```text
XAML Schema / authoring
    ↓
Parser + canonical document + Designer + CodeGen
    ↓
Runtime materialization / value sources / resources / templates
    ↓
Visual tree + layout + input + routed events + commands + automation peers
    ↓
Presentation scene + Direct2D/DirectComposition + Win32 projection
```

- `DispatcherObject`：线程归属。
- `DependencyObject`：属性 identity、有效值、表达式和值来源。
- `Visual`：视觉父子关系、变换、裁剪和场景节点。
- `UIElement`：输入、命中、路由事件、命令和焦点资格。
- `FrameworkElement`：Measure/Arrange、资源、Style、DataContext、模板关系。
- `Control`：控件公共基类和 template/automation behavior 入口。
- `Window`：顶层元素与平台窗口投影，不得重新拥有第二套焦点、输入、布局或自动化模型。

## 5. 类型和 Schema

- 新的公共 QName 必须先进入 XAML Schema，并在 Parser、canonical serializer、Designer、Materializer 和
  CodeGen 中形成同一身份链。
- native C++ 类名可以与 QName 不同。一个 behavior host 可承载多个声明组件；匹配组件必须看完整
  `RuntimeTypeId`，不能只看 `UIClass`。
- 旧 native 名称不作为 XAML 别名接受。改变静态输出语义时同步提升
  `DesignCodeGenerationContractVersion` 与 `build/CuiCodeGen.targets`。

## 6. 属性和值来源

有效值顺序固定为：

```text
Animation > Local > VisualState > Template > Style > Theme > Inherited > Default
```

- 普通公开 setter 必须进入 Local 槽；交互更新使用 current-value 语义以保留 Binding。
- 不允许公开字段与依赖属性双写，不允许高对比度或默认主题覆写作者 Local 值。
- 公共外观使用 Brush：`Background`、`Foreground`、`BorderBrush`。`BrushKind::None` 是 no-paint，
  透明色不是 unset。
- Brush 颜色文本只是 XAML 简写；规范模型只保存结构化 Brush 对象。
- Binding、DynamicResource、TemplateBinding 和 Animation 是槽内表达式，不是额外优先级或旁路字段。

## 7. 布局和树关系

- 全部几何使用浮点 DIP，按 `Measure → Arrange` 运行。
- VisualParent、LogicalParent、InheritanceParent、RoutedParent、TemplatedParent 各自表达单一关系，
  不得恢复一个 `Parent` 猜所有关系。
- Canvas attached properties、Grid definitions、Dock/Stack/Wrap/Relative 等语义由 XAML/Schema 定义；
  C++ layout engine 只执行。
- 树变更必须事务性维护父关系、namescope、继承、焦点/capture、自动化、布局和场景失效。

## 8. 输入、焦点和文本

- 一个原生报告只进入一次 `InputManager` staging，并沿 Preview tunnel → C++ behavior → Bubble 使用同一 args。
- 捕获只通过 InputManager/Window 平台投影；控件不得直接维护第二个 Win32 capture 状态。
- 每个 Window 只有一个 `FocusManager`。`Focusable`、`IsTabStop`、logical focus 和 keyboard focus 不得混用。
- 文本输入只走 `TextCompositionManager`。逐控件 `WM_CHAR`/`WM_IME_*`、IME 查询和候选窗定位属于禁止回归。
- 助记键采用 AccessText `_`/`__`；不存在公开可写 `AccessKey` DP。

## 9. 事件和命令

- 公开输入事件是 routed event；处理阶段共享 `Handled` 和稳定 route snapshot。
- XAML 保存 handler 名，C++ registry 只把该名称挂到强类型 behavior，不注册事件类型。
- 命令只走 `RoutedCommand`、`CommandBinding` 和 `InputBinding`；不能恢复整数 CommandId、窗口 switch 或
  菜单/托盘独立命令系统。
- `CommandTarget` 是 authored namescope 引用并以弱引用实现；显式目标失效必须失败，不能偷回退到焦点元素。

## 10. 渲染

- XAML/DP/Style/Template/VisualState 决定视觉语义，renderer 只 realization。
- 控件不得在内容绘制结束后偷偷直绘 focus/validation/hover 外观。此类非内容视觉进入 Adorner/Template 层。
- `PresentationScene` 保存 retained 节点和 dirty region；`PresentationRenderHost` 负责设备、帧和提交。
- native fallback renderer 仅服务尚无默认模板的 behavior host，必须让位于 authored Brush/Template。
- 新增 Brush 类型或 transform 时，要同时审计 D2D realization、缓存 identity、设备丢失和 retained scene 记录。

## 11. 自动化

- XAML 只公开 `AutomationProperties`。
- 控件能力只由 `AutomationPeer`/虚拟 peer 的 `AutomationPattern` 声明。
- Window/UIA provider 不得按具体控件类型或 `UIClass` 猜 Pattern。
- focus、selection、value、scroll 是不同状态；虚拟容器必须读取真实 selection/viewport 模型，不能复制公式。
- Password 等敏感值不得进入自动化 name/value/trace。

## 12. 工作方式

- 先完成一个大模块的结构闭环，再集中编译、修补和回归；不要每改一行就跑完整测试。
- 搜索旧语义时覆盖源代码、XAML、生成器、Designer、样例和文档。
- 删除 Legacy 时同时删除项目项、include、parser 分支、serializer 升级、Designer catalog、CodeGen 和测试夹具。
- 生成文件由 CodeGen 重生，不手改 `.g.*`。
- 保留用户工作树中的无关修改，不使用 destructive git 操作。

## 13. 完整门禁

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  CUI.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 `
  /p:LinkIncremental=false /verbosity:minimal

.\x64\Debug\Designer.exe --self-test
.\x64\Debug\CUITest.exe --validate-xaml
.\x64\Debug\CUITest.exe --smoke-xaml
.\x64\Debug\CUITest.exe --render-smoke
.\x64\Debug\CuiRuntimeSample.exe
.\x64\Debug\CuiStaticGeneratedSample.exe
.\x64\Debug\CuiCodeGen.exe --version
.\x64\Debug\CUICoreTests.exe
```

若完整核心回归在 Debug 下包含大规模虚拟化压力用例，应给足超时；性能测试慢不等于允许跳过失败。
