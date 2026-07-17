# CUI 构建与回归验证清单

## 构建矩阵

使用 Visual Studio 2022 的 MSBuild 验证解决方案：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Release /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Debug /p:Platform=x86 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Release /p:Platform=x86 /v:minimal
```

说明：解决方案平台使用 `x86`，项目内部映射到 `Win32`。

## 无窗口布局内核测试

`CUICoreTests` 不创建 HWND，也不依赖 Direct2D 设备，可快速验证 DIP 几何、
Measure/Arrange 脏状态以及兼容 Canvas/Anchor 规则：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUICoreTests\CUICoreTests.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
.\CUICoreTests\x64\Debug\CUICoreTests.exe
```

该项目已加入 `CUI.sln`，正常构建解决方案时会一并编译。

## 无窗口代码生成与 MSBuild 增量集成

`CuiCodeGen` 必须能在不创建 Designer/窗口的情况下读取 XML/XAML，并与交互导出经过同一个
`DesignCodeGenerationService`：

```powershell
msbuild CuiCodeGen\CuiCodeGen.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
.\CuiCodeGen\x64\Debug\CuiCodeGen.exe --help
.\CuiCodeGen\x64\Debug\CuiCodeGen.exe generate `
  .\CuiStaticGeneratedSample\NamespacedWindow.cui.xaml `
  --output "$env:TEMP\CuiCodeGenCheck\NamespacedWindow" --quiet
```

`--help` / `--version` 返回 0，生成失败返回 1，未知参数或缺值返回 2；成功生成必须原子得到
`.h/.cpp/.g.h/.g.cpp/.handlers.g.inc` 五个文件。核心测试会让服务分别读取 XAML 和 v5 XML，验证
`x:Class` 规范化、相对 `d:CodeBehind`、失败时输出模型不变、非法扩展名拒绝，并把 XAML 结果与静态样例
五文件逐一比较。交互重复导出须默认复用文档已有的限定 `x:Class`，显式修改类名时给出迁移警告，且完整关联
预检必须早于代码写入。门禁还需覆盖无子控件但含 Form 事件的代码生成。

`CuiCodeGenCore.lib` 是代码生成实现的唯一链接单元。四配置均须构建该静态库，且下面的清单搜索只能返回
`CuiCodeGenCore.vcxproj` 中两行；Designer、CLI 与测试工程只能出现项目引用：

```powershell
rg '<ClCompile Include=".*(CodeGenerator|DesignCodeGenerationService)\.cpp"' -g '*.vcxproj'
```

`CuiStaticGeneratedSample` 导入 `build/CuiCodeGen.targets`。把设计文件或 targets 文件任一时间戳更新后首次
Build 必须出现
`CUI code generation`；若解析后的规范输出未变化，五个代码文件时间戳必须保持且样例 C++ 不应重新编译，
只有 `$(IntDir)\CuiCodeGen` 下的 stamp 更新。不再修改输入立即再次 Build 时必须跳过生成器，stamp 与代码文件
时间戳均保持不变。手工漂移 `.g.h/.g.cpp/.handlers.g.inc` 任一个文件后普通 Build 必须启动生成器并恢复规范
字节；只改变用户 `.h/.cpp` 时也必须重新预检但保留用户扩展。删除任一代码文件后准备目标应使 stamp 失效。还须从干净状态构建一次完整解决方案，确认
项目依赖先生成对应配置/平台的 `CuiCodeGen.exe`。改变生成输出语义时必须提升契约版本，验证新的 versioned
stamp 路径使旧输出失效；单纯重新链接相同契约的 exe 不得让后续 Build 重复运行生成器。

## Designer 无窗口交互冒烟

`CUICoreTests` 只链接 Designer 的模型/目录层。属性面板或命令栈修改后，还必须运行链接完整生产实现的
Designer 自检；它不创建 HWND，但会构造真实 `DesignerCanvas` 和 `PropertyGrid`，覆盖混合多选、批量
Apply、错误状态、Reset、完整选择恢复以及 Undo/Redo：

```powershell
$p = Start-Process .\x64\Debug\Designer.exe `
    -ArgumentList '--self-test' -Wait -PassThru -NoNewWindow
if ($p.ExitCode -ne 0) { throw "Designer self-test failed: $($p.ExitCode)" }
```

该入口必须返回进程码 0；无效属性输入不得创建撤销记录，成功 Reset 必须清除先前错误状态。

## CUITest 动态 XAML 门禁

`CUITest` 的八个页面由 `CUITest/DemoWindow.cui.xaml` 动态构造；项目构建必须把该文件复制到
`CUITest.exe` 同目录。除正常启动检查外，每种配置都应执行：

```powershell
$validate = Start-Process .\CUITest\x64\Debug\CUITest.exe `
    -ArgumentList '--validate-xaml' -Wait -PassThru -WindowStyle Hidden
if ($validate.ExitCode -ne 0) { throw "CUITest XAML validation failed: $($validate.ExitCode)" }

$smoke = Start-Process .\CUITest\x64\Debug\CUITest.exe `
    -ArgumentList '--smoke-xaml' -Wait -PassThru -WindowStyle Hidden
if ($smoke.ExitCode -ne 0) { throw "CUITest XAML smoke failed: $($smoke.ExitCode)" }
```

`--validate-xaml` 覆盖解析、事件契约和自定义控件工厂；`--smoke-xaml` 还会创建真实 `Form`、
材质化全部内置/自定义控件、解析命名事件并初始化菜单、表格、图表、Web 和媒体数据，然后安全销毁。
失败会在 XAML 旁生成 `DemoWindow.cui.xaml.error.txt`，成功必须删除旧诊断并返回 0。

## WebView2 可选构建与公共 ABI

`WebBrowser.h` 的公开类布局不依赖 WebView2 SDK 或 `CUI_ENABLE_WEBVIEW2`。除默认启用构建外，
至少验证一次显式禁用分支：

```powershell
msbuild CUI\CUI.vcxproj /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:CUIEnableWebView2=false /v:minimal
```

禁用时 `WebBrowser::GetInitializationState()` 应为 `Unsupported`，`TryInitialize()` 与浏览操作
返回 `false`，分阶段诊断返回 `E_NOTIMPL`；对象大小、公开方法和 Designer 可见属性保持不变。

## 浮点测量与 Auto 尺寸

新布局代码应使用浮点约束入口；`SIZE` 重载只作为旧自定义控件/布局引擎的
兼容桥保留：

```cpp
auto desired = control->Measure(cui::core::Constraints{
    cui::core::Size{ availableWidth, availableHeight }
});
```

`Width` / `Height` 仍是整数兼容投影。需要小数 DIP 或公开的内容自适应语义时，
使用不会被测量结果改写的声明属性：

```cpp
label->LayoutWidth = cui::layout::Length::Auto();
label->LayoutHeight = cui::layout::Length::Auto();
panel->LayoutWidth = cui::layout::Length::Fixed(320.5f);
```

新自定义控件优先覆写
`cui::core::Size MeasureCore(const cui::core::Constraints&)`；旧的
`SIZE MeasureCore(SIZE)` 仍会由默认桥调用。

`GridPanel` 的轨道定义支持四种公开语义：

- `GridLength::Pixels(dip)`：固定 DIP。
- `GridLength::Percent(50.0f)`：有界轴上占可用空间的 50%；无界测量时按内容尺寸处理。
- `GridLength::Auto()`：按内容测量，包含 `GridRowSpan` / `GridColumnSpan` 的内容缺口。
- `GridLength::Star(weight)`：按权重分配剩余空间，并迭代满足各轨道的 `Min` / `Max`。

Auto 行会使用已经求出的跨列宽度测量子控件，因此换行内容的高度不再按无界宽度误算。
设计器行列编辑器使用 `50%` 文本表示 Percent；保存/加载和 C++ 代码生成都会保留该单位。

## 内置容器布局语义

- Stack 的 ContentAlignment 对齐整个内容带，子控件的 `HAlign/VAlign` 再在内容带内生效。默认两轴 `Stretch` 与历史布局兼容。
- Wrap 的固定 `ItemWidth/ItemHeight` 会参与子控件 Measure，而不只是覆盖 Arrange 尺寸；Auto 子项和 Dock 子项都会先从可用约束中扣除 Margin。
- Dock 的 `LastChildFill` 使用最后一个可见子项；混合 Left/Right 与 Top/Bottom 时，Measure 会保留先前已消费的另一轴尺寸。
- Stack、Wrap、Dock 的上述公开配置已通过属性元数据接入 Binding；Stack、Wrap、Dock 与 Split 的
  布局配置，以及 Split 的分隔条颜色、圆角和视觉内缩，均通过 Design 描述接入通用 Designer 编辑、
  Coerce、持久化和代码生成路径。Split 的交互拖动同样更新规范 `SplitterDistance` metadata。

## 基于属性元数据的 Binding

`Binding` 不再按属性名判断控件类型。控件类型在
`EnsureBindingPropertiesRegistered()` 中声明自己的读、写和变更订阅元数据，
然后所有 `OneWay` / `TwoWay` 路径都通过同一套元数据执行。`BindingValue`
除数值、布尔和字符串转换外，也可携带任意可复制 C++ 值类型。

自定义控件可用 `BindingPropertyRegistry::Register<Owner, Value>()` 注册属性；
注册后仍使用原有调用方式：

```cpp
target->DataBindings.Add(
    L"Payload", viewModel, L"Payload", BindingMode::TwoWay);
```

元数据中的变更订阅必须返回 `EventConnection`，通常使用事件的
`Subscribe(...)`；连接由 `Binding` 持有并在清除/析构时自动解绑。普通 `+=`
仍适合与发布者同寿命的永久处理器。

`BindingPropertyMetadata` 也承载通用控件属性行为。相关回归必须证明：

- 默认值可发现、可通过 `ResetPropertyValue(...)` 恢复，并能由
  `IsPropertyValueDefault(...)` 判断；
- 直接 setter、`TrySetPropertyValue(...)` 和 Binding 写入得到相同的 Coerce 结果；
- 相同有效值不重复触发 Changed 或 `OnPropertyValueChanged`；
- `AffectsMeasure` / `AffectsArrange` / `AffectsRender` 正确推进布局脏状态和重绘；
- `TracksLocalValue` 使公开 setter 在尚无其他来源时也创建 Local 层，清除 Local 后可显露
  已缓存的 Style/Theme 值；
- 自定义比较器可支持 `SIZE`、颜色等没有通用 `operator==` 的值类型；
- 未知属性、错误类型和没有默认值的 Reset 明确返回失败且不修改控件。

属性来源相关回归还必须覆盖：

- `Local > Binding > Style > Theme > Default` 的确定性优先级；
- 隐藏来源更新只更新缓存，不触发有效值变化，清除高层后显示隐藏层最新值；
- `ResetPropertyValue` 清除 Local 并回退，而 `ClearPropertyValues` 可整体卸载 Theme/Style；
- Binding 创建时占用 Binding 层，清空/析构时恢复 Style、Theme 或进入体系前的基线值；
- `SetCurrentPropertyField` 在 TwoWay Binding 下回写源且不遮蔽后续源更新；
- 普通属性 API 不能覆盖或清除活动 Binding 所拥有的层；
- BindingCollection 和直接构造场景中的重复目标 Binding 都以 `DuplicateTargetProperty` 拒绝，
  后构造 Binding 的析构也不得误清除先前所有者的值。

控件样式表相关回归必须覆盖：

- `UI_Base` 通配、精确 `UIClass`、StyleId、多个 StyleClass、必需/排除状态匹配；
- `Id > Class/状态 > 精确类型 > 通配`，同特异性后规则胜出的确定性级联；
- Hovered、Focused、Pressed、Checked、Disabled 状态改变后自动重新解析；
- 资源键大小写不敏感；缺失资源、未知/只读属性和无效值可诊断且不阻断较低规则回退；
- 资源和规则修改可热刷新；
- Theme 与 Style 使用各自来源层，隐藏 Theme 更新在 Style/Local 清除后恢复；
- 根控件递归附着、后加入子控件继承、规则删除和样式表卸载均正确清理并回退。
- `Button`、`TextBox`、`ComboBox` 的状态色、边框、圆角和间距通过同一元数据路径 Coerce，
  公开赋值、样式覆盖和重置行为一致。
- Designer 文档样式表的强类型资源、选择器、状态与 Literal/Resource Setter 可规范化并校验；
  XML v5 往返、实时预览和生成的 C++ 保持同一语义，预览应用失败后恢复先前样式。
- XML v5 的控件 `id`/`parentId` 与文档 `nextId` 必须往返稳定，重命名、重排、撤销/重做和
  重新生成代码不得改变既有 ID，删除后新增不得复用旧 ID；重复、悬空、成环或高水位回退应拒绝。
  版本 1–3 文档应在加载时补齐身份，版本 1–4 均应升级。生成控件的 `DesignId` 与运行时递归查找结果必须一致。
  v5 可选 code-behind 必须把合法 C++ 类名与 `Form.Name` 分离，只持久化相对设计文件且无扩展名的基路径；
  XML/XAML 往返、Canvas 完整快照和 Undo/Redo 均须保真，绝对路径、缺失类名和非法标识符必须拒绝。
  `x:Class` 的 `Acme.Views.MainWindow` 与 `Acme::Views::MainWindow` 都须规范为后者，空段/关键字必须拒绝；
  生成头在 `namespace Acme::Views` 内声明叶类，生成源使用限定定义，并按导出文件基名 include `.g.h`。
- `DesignDocumentGraph` 必须对乱序节点给出确定的根/子级顺序，并统一拒绝重复身份、缺失普通父级 ID
  和父级循环；`DesignDocumentControlPool` 必须写入同一 `DesignId`，未挂载项在失败时自动释放，
  `Take` 后只由目标控件树持有。Canvas 不得另建控件类型 switch 或用裸指针承担待挂载所有权。
- `RuntimeDocumentLoader` 从模型、规范 XML、XAML 风格字符串或文件加载时必须使用完整 Designer 材质化语义，
  并以候选对象提交；
  解析、属性、复合容器、样式、Binding 或事件解析失败时不得修改已有输出。运行时文档必须覆盖稳定 ID/名称
  查询、根控件所有权转移、DataContext 实时更新与 Local 后备恢复、命名控件事件的 RAII 连接，以及与静态
  代码生成相同的 `CodeGenInput` 投影。生成样式必须挂到根控件树，不得调用不存在的
  `Form::SetStyleSheet`。`XamlDocumentParser` 必须把嵌套树、浮点/Auto 尺寸、Grid 定义、TabPage、Split
  区域、Binding、命名事件、强类型资源与 Style Setter 投影到同一 `DesignDocument`；未知语法和失败替换
  不得污染已有模型或运行时树。`XamlDocumentSerializer` 必须把同一文档规范写回，并通过通用
  `d:DesignProps` / `d:DesignExtra` 保留没有直接语法的结构值；浮点颜色不得量化为 8 位通道。
  外部自定义标签必须把命名空间、C++ 类型/头文件、内置 `d:BaseType` 和构造约定在规范 XAML 与 v5 XML 中
  完整往返。生产 `RuntimeDocumentLoader` 未注册工厂时应保持旧输出并失败；注册表应创建真实派生实例，
  Designer/CLI 的显式代理模式应继续生成真实类型 include、强类型成员/访问器和构造代码。注册真实控件后，
  专有属性应以普通 XAML attribute 接受并经过属性元数据 Coerce；规范 writer 应回退为 `d:DesignProps` /
  `d:DesignBindings`，无业务 DLL 的解析与代码生成仍须保真，Reload 省略注册表时应继续原位更新真实实例。
  `Designer.exe --self-test` 覆盖上述动态 XML/XAML 冒烟路径。
- Designer 外部控件清单必须通过 `Designer.exe --validate-controls <manifest>` 无窗口校验；有效清单加入
  ToolBox 后应保留分类、默认尺寸、容器语义、XAML identity、规范 C++ 类型和构造约定。重复名称、重复 XAML
  identity、冲突前缀、不安全 include、未知 schema/属性/子内容、DTD 和超过 4 MiB 的输入都必须整份拒绝且不
  修改已有描述符。无 `PreviewFactory` 时 Canvas 使用声明基类代理；同进程附加工厂后必须走同一描述符
  Add/Undo/Redo/文档捕获链路并核对工厂实例的 `Type()`。
- `DesignDocumentEventIndex` 必须解析窗体/控件事件的约定名与显式名，按精确 `Event::function_type` 聚合同名引用，并事务性拒绝
  未知事件、非法标识符和跨签名复用。文档级重命名应覆盖全部共享引用与旧 Auto 名，支持同签名合并、
  Undo/Redo、XML/XAML 往返和新名称的 `std::bind_front` 代码生成；冲突失败不得修改文档。动态注册表门禁应覆盖
  Event 成员的 callable 约束、同签名错误成员拒绝、普通/自定义/继承事件包装器、类型相同但参数名不同的复用、
  同名异签名拒绝、未知函数诊断，以及共享 resolver 在 Reload 前追加路由后生效。事件参数类型文本必须从真实
  Event 成员自动生成，目录只能提供参数名，避免成员类型与手写声明漂移。`RegisterBatch` 门禁必须覆盖中途
  重复与异常后的完整 route 恢复、既有 resolver 继续可用，以及成功批次一次可见。
- EventCatalog 必须为每个可设计控件类型提供唯一默认事件和稳定分类，Form 以仅触发一次的 `OnShown` 为默认事件。
  事件行、画布控件及 Form 客户区双击必须汇合到同一激活入口，复用已有函数或经同一可撤销属性事务创建默认
  函数名，并发布处理函数激活事件；Designer
  只在已有显式代码导出目标时自动重生成/追加用户桩并打开实际定义所在的 `.h` 或 `.cpp`；显式导出须事务性持久化 `x:Class` 与
  `d:CodeBehind`，保存并重开后继续解析相对目标，未导出或无关联文档不得猜测代码路径。
  门禁应覆盖已有函数零修改激活、控件/Form 画布双击、默认名单次 Undo 记录、分类展示、`OnShown` 单次触发、
  回调函数名和签名冲突不旁路；删除事件后生成订阅必须消失，但用户声明与函数体继续保留。
- 重复代码导出判断用户处理函数定义时必须使用 token/函数体边界而非子串；门禁至少覆盖 `Handle` 与
  `HandleSave` 前缀碰撞，以及注释、普通字符串和 raw string 内的 `Class::Handler` 伪文本，四种情况都不得
  阻止缺失用户桩生成，真实定义仍只能保留一份。已有同名定义的空白/参数名调整必须正常复用，参数类型漂移
  必须在写入前拒绝并保持五文件逐字节不变。以不同 `x:Class` 指向已有用户文件必须在写入前拒绝，并证明
  `.g.*`、声明 include 与用户 `.h/.cpp` 全部逐字节不变。
- 事件定义检查必须联合用户 `.h/.cpp`：除类外定义外，还要识别精确用户类体内的 `void Handler(...) {}`。
  头/源合计只允许一个兼容定义；类内定义存在时 `.handlers.g.inc` 必须省略冲突声明，导航、候选发现和显式
  函数体迁移必须指向实际文件。兼容定义还必须是非静态、非 cv/ref `void` 成员；错误返回类型、`static`、
  `const`/ref、删除定义与类外不匹配的 `noexcept` 应成为签名错误且保持五文件不变。门禁须覆盖头内实现、
  头源重复拒绝、缺失源仍为已实现、错误成员形状过滤以及头内迁移 Undo/Redo。
- 用户头类身份必须与事件/构造函数复用同一 `CppUserCodeIndex`：只接受精确 `x:Class` namespace 中唯一、直接
  继承预期 `Generated` 基类的活跃类体。门禁应允许导出宏、`final`、访问说明、多基类和全限定基类，并证明
  `#if 0` 假类、相邻 namespace 同名类、错误基类及重复类体会在五文件写入前阻塞且保持全部文件逐字节不变。
- 默认构造函数检查必须联合用户头和源文件：类体内联、委托构造、`= default` 与源文件外部定义使用同一
  token/预处理/namespace 索引，合计只允许一个可用默认构造函数。门禁须证明 `= delete` 与跨文件重复定义
  阻塞且不改文件；用户源缺失而头中已有构造定义时应只重建 marker/include 与事件桩，不得生成重复构造体。
- 同一用户代码索引必须屏蔽预处理指令和续行宏中的作用域符号，并按确定的字面量 `#if 0` / `#if 1`、嵌套
  条件、`#elif` 与 `#else` 排除失活函数体；注释、普通/原始字符串中的伪指令不得改变分支状态。门禁还须证明
  生成诊断、兼容候选、源码跳转和显式函数体重命名使用同一位置保持索引，重命名只修改活跃定义的名称 token。
- 代码导出的目标集合必须使用预写入 + flush + 有备份的批次提交。门禁应锁定中间的
  `.handlers.g.inc` 使 rename 真实失败，并证明此前已提交的既有文件逐字节恢复、此前不存在的目标重新消失、
  尚未提交的用户文件不变，且目录中不残留 `.~cui-batch-*` 临时或备份文件。
  外部 code-behind 提交失败还必须恢复生成前文件集：既有文件逐字节还原、原先不存在的目标删除，结果对象
  保持空；原子删除后遇到后续锁定目标时也必须恢复被删除文件且不残留 `.~cui-*` 工件。
- 五文件计划必须保存读取前的存在性与精确内容，并以原子批次前置条件防止 stale plan 覆盖外部编辑。门禁须
  覆盖已有用户源被修改、原本缺失目标被外部新建、后置目标冲突时前置目标零写入，以及关联回调期间外部修改
  导致条件回滚拒绝；最后一种情况必须保留外部内容并明确报告恢复不完整。显式事件函数体迁移也必须按读取
  内容条件写入，Undo/Redo 回滚仅可覆盖仍等于本次迁移/生成结果的文件。
- `CuiStaticGeneratedSample` 必须作为四配置解决方案项目编译并运行，覆盖限定 `x:Class`、外部自定义控件、生成/用户类分离、
  强类型 `GetXxx()` 命名控件访问器、`ControlIds` 稳定身份、动态 `ClassReferences<RuntimeDocument>` 的
  `GetXxx()`/`ReferenceXxx()`、`ClassEventSink` 自动注册控件/Form/自定义事件、失败批次回滚、跨 Reload
  重新解析/重新连接和 `std::bind_front` 事件连接。编译门禁必须至少覆盖
  鼠标、`std::vector<std::wstring>` 文件拖放、属性变化和无 sender 的校验变化签名。核心测试必须将其五个代码文件
  与同输入的新生成结果规范化比较，并覆盖名称规范化后的全局去重以及同叶类不同 namespace 的身份冲突整批拒绝。
  样例项目必须通过 `CuiDesign` + `build/CuiCodeGen.targets` 在编译前增量生成，而非依赖人工更新；输入未变化时
  不得启动生成器，用户 `.h/.cpp` 变化时必须重新检查生成计划，语义输出未变化时不得改写生成文件或用户文件。
- `RuntimeDocumentLoader::Reload*` 对通用标量/元数据属性、Binding/DataContext Schema、文档样式、控件事件和
  窗体显示属性变化应返回 `InPlace`，保持稳定 ID 对应的指针、DataContext 和宿主所有权。门禁必须覆盖属性、
  Binding、样式与事件的复合失败回滚。拓扑/`Extra` 变化应在根仍由文档持有时返回 `Recomposed`，并覆盖普通
  子级重排、增删、Grid/Tab/Split 嵌套子树保留、Binding/事件附件重建及失败后的拥有权逆序回滚；没有可复用
  子树、字体所有权、未知属性袋或活动 Binding 冲突才返回 `Replaced`。通过 `TransferRootControlsTo(Form)`
  转移后仍应支持 `Recomposed` / `Replaced`，保持宿主自有根和文档根锚点；门禁必须覆盖多文档根与宿主根交错时
  的精确失败回滚、候选提交拒绝、复用实例、类型化引用、Binding 和旧事件连接；窗体显示属性与窗体事件解析器
  必须跨 `InPlace` / `Recomposed` / `Replaced` 自动延续，失败时恢复旧显示状态、借用字体和旧连接。旧 `ReleaseRootControls()`
  无适配器路径必须拒绝拓扑重组/整树替换且保持旧树。省略的运行时附件应继承当前文档。
- `Load*IntoForm()` 必须把解析/材质化、Binding、控件事件、Form 显示、Form 事件和根挂载作为一个候选提交；
  `AttachToForm()` 必须覆盖缺失 Form 函数、根宿主拒绝后的显示/借用字体/事件/根所有权完整回滚和同实例重试。
  已附加输出上的直接 `Load*()` 必须无副作用拒绝并保留旧事件/根；未保存 Form resolver 时，Reload 新增 Form
  事件必须失败并恢复精确根槽位，不得静默接受。
- `FindControlByDesignId` / `FindControlByName` 应由文档索引提供常数时间查询；`RuntimeControlRef<T>` 应在
  `Replaced` 后解析新实例、`Recomposed` 后保持复用实例，并在对应节点删除后返回空，不得持有控件所有权。
  文档销毁后引用必须安全过期；移动构造应迁移已有引用，Loader/Reload 的目标移动赋值应保留目标引用并让
  赋值源引用失效，任何路径都不得留下可解引用的悬空 `RuntimeDocument*`。
- 生成的 `ClassReferences<TDocument>` 必须保存 `document.Reference()` 返回的弱文档视图，不得保存裸
  `TDocument*`；应提供布尔有效性和 `TryDocument()`，并验证视图的 `GetXxx()` / `ReferenceXxx()` 跨 Reload、
  文档移动后继续解析，文档销毁后统一返回空。改变该输出时必须同步提升服务、CLI 与 targets 的生成契约版本。
- `RuntimeDocumentFileWatcher` 必须由宿主线程轮询且不创建后台线程；门禁应覆盖文件 ID 可识别的原子替换、
  防抖窗口、稳定失败签名抑制、下一有效签名自动恢复、格式感知重载，以及失败期间旧实例/事件连接保持。
- `RuntimeDocumentSession` 必须把文件、Form、共享事件注册表和 watcher 组合为不可移动的显式 UI 线程生命周期；
  门禁应覆盖未注册处理函数导致的首次挂载完整回滚、同 session 补注册后重试、可延迟启用监视、跨线程 Poll
  拒绝，以及热重载未知函数失败后晚注册路由 + `RequestRetry()` 原位恢复。session 不得创建后台线程或隐藏
  watcher 的 ReloadMode/错误。
- `DesignDocumentMaterializer` 必须位于独立编译单元，默认创建生产控件；Designer 只能通过显式工厂注入
  `FakeWebBrowser` 等预览控件。`CuiRuntime.lib` 必须能脱离 Designer 可执行文件链接，四套默认配置的
  `CuiRuntimeSample.exe` 与 `CuiStaticGeneratedSample.exe` 均须构建并运行成功；前者输出 XAML/XML 往返、嵌套布局、索引、属性/Binding/样式/事件
  签名安全命名事件、原子首次 Form 挂载/直接 Load 防护、原位重载、复合失败回滚、拓扑子树重组/回滚、适配宿主后的结构替换/精确槽位恢复、Form 属性/事件延续、手动所有权边界及 UI 线程 session 通过信息；
  Designer 项目必须引用这同一静态库，不得再次编译 Runtime/事件注册表/材质化/序列化源文件。
- 样式 Setter 属性目录来自目标控件的运行时元数据；属性下拉、自动值类型/示例值、资源兼容性、
  未出现在画布上的目标类型探针，以及无类型规则仅允许公共属性都必须有回归覆盖。
- 普通 Designer 属性面板中未被旧字段表示的可写元数据属性可直接编辑；应用必须经过类型转换与
  Coerce，规范值通过可选 `props.metadata` 往返，并由代码生成器输出 `TrySetPropertyValue(...)`。
- 普通面板里仍由旧字段持久化的 Text、位置、尺寸、颜色、Margin/Padding 与对齐属性也必须优先走
  元数据 Local 层，不能直接写字段绕过 Coerce、Changed 或 Binding/Style 优先级。统一访问层必须按
  Persistence 自动跟踪 Metadata/Automatic、排除 Legacy/Transient；Reset 后应移除 metadata 条目并
  正确显露 Style/Binding/Theme/默认值。大小写不同的同名条目不得重复。
- 普通控件属性面板必须使用 `GetPropertyGridProperties(...)` 生成包括 Legacy 在内的全部可浏览标量；
  `Enabled`/`X`/`Y`/`Dock` 只作为规范属性的显示名，不得产生别名重复项。Grid 行列与 Dock 必须根据
  父容器条件出现，Transient 状态仍不得显示。标量滑块的实时预览与提交必须走同一元数据 Coerce/跟踪路径。
- ComboBox Items、GridView Columns、Tab Pages、ToolBar Buttons、Tree Nodes、Grid Definitions、Menu Items
  与 StatusBar Parts 的按钮必须来自 `DesignerCustomEditorCatalog`；新增结构编辑器不得在属性面板恢复
  `UIClass` 显示分支，注册覆盖应保持目标类型/ID 唯一并按 Order/ID 稳定排序。八类编辑器均须在打开前建立
  严格文档事务：确定后单命令提交，取消/无变化不入栈，嵌套 Begin 被拒绝，异常、无效后置文档或入栈失败
  恢复前置文档及完整选择。结果必须区分 Begun/Committed/Unchanged/RolledBack/Canceled/Aborted/Rejected/
  Failed；Cancel 发现意外修改时必须恢复而非丢弃快照。Designer 自检需覆盖真实结构集合的提交、Undo/Redo、
  显式回滚、无变化提交、取消泄漏恢复，以及返回 false/抛出异常的执行回调不入栈。
- Name、Anchor、StyleId、StyleClasses、FontName、FontSize 与 MediaPlayer MediaFile 必须来自
  `DesignerControlPropertyCatalog`，并经 Binder 注入唯一命名、默认名称计数、共享字体和 Anchor 边界上下文。
  目录须覆盖适用条件、大小写查找、类型校验、规范值回读和 Reset；PropertyGrid 不得为这些属性保留
  文本/布尔/浮点裸字段回退，元数据滑块缺失或拒绝时也不得静默写入控件字段。
- `DesignerPropertyRowCatalog` 必须将窗体属性，或包装器属性与运行时元数据的并集，投影为同一种
  `DesignerPropertyRow`。投影必须按规范名去重，跨来源全局排序并保持分类连续，同时携带当前 kind/value、
  编辑器、Choice、数值提示、Reset 能力和 Binding/Validation/Style/Theme 诊断。诊断需包含绑定路径、模式、
  Converter、预览/运行时错误、验证严重级别，以及样式规则 ID/特异性/遮蔽来源。设计器只能把这一行流映射到
  CUI 原生 `PropertyGridView` 的 Item；Boolean/Enum/Color/Slider、混合值、Reset 与 Action 行必须复用原生能力，
  不得为不同来源维护平行渲染循环或重新逐行拼装编辑控件。
- 运行时属性行必须记录 `GetPropertyValueSource(...)` 返回的 Default/Theme/Style/Binding/Local 有效来源；
  PropertyGrid 应显示并在成功编辑后刷新来源。顶部筛选框必须保持固定且跨选择保留查询，空格分隔关键词
  使用大小写不敏感 AND 匹配，覆盖行名称/分类/当前值/编辑器/Choice/中英文来源/诊断摘要与详情，以及事件、Binding 入口
  和结构编辑器。空查询不得改变顺序或数量，无匹配查询必须返回空行而不破坏完整目录。
- 多选属性面板必须对每个目标生成统一行投影并求兼容交集；对象身份 `Name` 不进入交集，不同值和不同
  有效来源必须分别标记为 mixed。批量输入须先验证全部目标，任一 Binding-owned 目标使该行只读；成功
  修改或 Reset 必须作用于全部所选控件，并以完整选择集保存为一条撤销命令；Binding-owned 行的 Reset 也必须
  拒绝。诊断不一致时须标记 mixed 且不得展示主选详情。多选时不得显示仅面向主选的
  事件、Binding 或结构化编辑器入口。
- 标量 Apply/Reset 必须通过 `DesignerPropertyEdit`：无效文本应返回带目标名的错误且不修改值；批量 setter
  在后续目标拒绝或抛出异常时必须恢复先前目标及 metadata 跟踪。PropertyGrid 顶部错误状态区需固定、可见
  且有 AccessibleDescription，成功或选择切换后清除。无法建立设计文档快照、命令无法入栈或滑块分组提交
  失败时，操作不得绕过撤销栈，已应用预览必须回滚并恢复完整选择。普通属性、DataContext Schema、文档
  样式、Binding、分组滑块和结构编辑器必须复用 `DesignerCanvas` 结果型文档事务；PropertyGrid 中不得再出现
  手工前后 `DesignDocument`/选择捕获或 `UpdatePropertyCommand` 拼装。
- PropertyGrid 的程序化 Apply/Reset 必须复用交互提交路径，并提供只读行模型与错误状态查询；不得为测试
  另建绕过 Binder/命令栈的 setter。`Designer.exe --self-test` 必须在不创建窗口的情况下验证混合多选、
  批量修改、失败不入栈、Reset、完整选择、Undo/Redo 文档重建，以及设计期 Binding 连接/断开时 Local 值恢复。
- ToolBox 必须按七个稳定控件族分组并支持名称、类型名和分类的关键词筛选；各控件使用代码原生矢量轮廓，
  不得退回所有条目共用一个 SVG。窄侧栏中的主/次文本必须保持单行且不覆盖相邻条目。
- Canvas 键盘微调、拖拽、resize 与 SplitContainer 分隔条必须复用结果型交互事务：首次预览修改前 Begin，
  MouseUp 单次提交；Escape、CancelMode、窗口失焦/停用和 CaptureChanged 回滚。鼠标移动/缩放/Reparent/容器
  重排必须提交 `ControlPlacementCommand`；SplitterDistance 必须提交禁止跨手势合并的单目标
  `ControlPropertyCommand`。这些内置手势不得保存完整文档；无法标识的自定义父级可安全回退。完成结果必须
  可查询并通过事件提供给 Designer 状态区；取消不得新增 undo 或清空既有 redo。Splitter 元数据 setter 失败
  不得退回裸写。Designer 自检须使用真实鼠标/键盘消息覆盖提交+Undo、拖拽 CancelMode、resize Escape、
  Reparent/同级顺序 Undo/Redo 和 splitter 捕获丢失。
- 设计期 DataContext 存在时，持久化 Binding 配置必须实例化为真实运行时 Binding。写目标模式连接前须暂存并
  清除 Local，断开或失败时恢复；OneWayToSource 保留 Local。生成的 `BindData(...)` 也须使用相同失败回滚，
  `BindingCollection::Find/Remove` 应以大小写不敏感目标名局部查询/卸载且保持其他绑定与验证订阅。
- `IDesignerCommand`、`CommandManager` 与 Canvas 的 Execute/Undo/Redo 必须贯穿返回
  `DesignerDocumentTransactionResult`；空历史为 `Unchanged`，失败/异常不得丢失原栈条目，且错误与
  `DocumentRestored` 必须保留。Add/Delete/Undo/Redo 完成后须发布带历史标签的 `OnCommandCompleted`，Designer
  状态区不得无条件报告成功。严格文档事务期间的独立 Execute/Undo/Redo 必须为 `Rejected`，且事务与原历史
  均保持可继续提交/取消。Add/Delete 必须使用所有权安全的 `ControlSubtreeCommand`：缺席根由 `unique_ptr` 拥有，并保存规范化节点、
  Root/普通/TabPage/Split 父级定位器、同级顺序、ToolBar 尺寸覆盖和完整选择，不得存整文档快照或裸指针所有权。画布增删、键盘移动、拖拽、缩放和分隔条只有在交互前的子树、placement/tree
  或属性差量捕获成功后才能修改，后置捕获或命令提交失败须恢复此前状态及完整选择。Designer 自检还需
  覆盖 Add/Delete 的结果事件、同实例 Undo/Redo、嵌套子树、多根同级顺序、Tab/Split/ToolBar 父级、整文档重建后恢复、冲突保留历史及内存上界，以及失败 Undo 的恢复标记
  和历史保持，以及事务进行中 Undo/Redo 的拒绝与后续历史可用性。
- Designer Dirty 必须按不可复用的文档状态 ID 与保存点计算：保存后 Undo 为 dirty、Redo 回同一保存点为 clean，
  Undo 后新建分支不得误命中旧保存点。Save 只有在原子文件替换成功后才能移动保存点；目标文件被独占锁定或
  替换失败时，旧 XML/XAML、当前文档、Dirty 和状态 ID 均须保持，且不得遗留临时文件。New/Open 成功后应恢复默认或
  目标文档并建立不可 Undo 的干净基线；解析/应用失败须恢复原文档、完整选择和 Dirty。活动严格事务中的
  Save/New/Open 必须为 `Rejected`。按 `.xaml` / `.xml` 扩展名执行的 Save As/Open 必须保持模型语义和
  干净保存点；显式 Reload 必须先处理 Dirty，失败时保留当前画布。`Designer.exe --self-test` 必须覆盖这些生命周期场景。
- Dirty 命令提交后应防抖写入会话级恢复文件，但不得移动正式保存点。恢复文件须以 PID+进程创建时间隔离；
  枚举时跳过仍存活的 owner，损坏/截断/超大 envelope 应隔离且输出参数保持不变。独占锁导致原子替换失败时
  旧恢复文件必须可读，且不遗留 `.~cui-*.tmp`。恢复应用成功后文档须为无 Undo/Redo 的 Dirty 基线；活动严格
  事务中恢复应为 `Rejected`。Save/New/Open/干净关闭只清理当前会话，接管遗留文件时须先写成当前会话快照，
  再删除旧文件。核心测试和 `Designer.exe --self-test` 必须覆盖存储往返、进程身份和 Canvas 恢复语义。
- 命令历史默认内存预算为 64MiB，并与 Undo 侧 128 条数量限制同时生效；用量必须覆盖 Undo+Redo，失败恢复、栈间移动、
  redo 清空和文档重置不得泄漏或重复计数。压力测试至少连续提交 1000 条估算命令，验证用量/条数有界、最远
  保存点淘汰后仍保持 Dirty、最近命令可 Undo。单条命令超过预算时仍保留该项。连续同选择属性编辑和键盘微调
  应合并为一个 before→final after 命令，Undo/Redo 恢复两端；精确保存点、redo 分支、不同标签/选择和超时必须
  阻止合并。普通控件属性与 SplitterDistance 预览必须使用 `ControlPropertyCommand`，其中每次 splitter 手势必须
  保持独立；键盘微调、鼠标 Move/Resize/Reparent 与容器重排必须
  使用 `ControlPlacementCommand`。160 控件文档中的单目标属性命令以及普通鼠标手势命令估算应小于 32KiB；简单 Add/Delete 子树命令应小于 32KiB，小型嵌套子树应小于 64KiB。普通
  Undo/Redo 不重建目标实例。Reparent 必须恢复 Root/普通/Tab/Split 父级、Grid/Dock 字段与同级索引。完整快照
  重建控件后，差量命令仍须按 Name+UIClass 解析目标和父级。历史外修改造成 expected 位置或父级不一致时，Undo
  应失败并保持栈项/用量，修复起点后可重试。Canvas 查询 API、Legacy 基值恢复和保存点边界须由核心测试与
  Designer 自检覆盖。
- 窗体属性必须直接投影 `DesignFormModel`，目录中的 21 个属性在默认模型上均可捕获且名称唯一；
  Width/Height 至少为 50，HeadHeight 至少为 0，FontSize 限制在 1..200，空 Name 规范为 MainForm。
  普通属性面板应按目录分类生成编辑器，并为有默认值的窗体/控件属性显示逐项恢复按钮；恢复必须
  进入撤销栈且延迟到事件返回后重建面板。默认字体族配合显式 FontSize 必须通过 XML 往返。
- `ControlPropertyOptions::Design` 的 Browsable/目标条件、分类与两级排序、显示名、自动编辑器、
  强类型 Choice、数值提示和 Metadata/Legacy/Transient 策略必须由目录回归覆盖；普通属性面板的
  过滤不能改变 Binding 或样式 Setter 的可用属性集合。
- Stack/Wrap/Dock/Split 容器属性必须从 Design 元数据生成编辑器并通过
  `props.metadata`/`TrySetPropertyValue` 往返；Split 的范围 Coerce、TwoWay 通知和拖动后的距离跟踪
  必须有覆盖。旧 `Extra` 仅在同名 metadata 缺失时读取并升级，不能覆盖新值或在新文档/代码中
  重复输出。
- Slider/NumericUpDown 的 Min/Max/Step/吸附/Value 联动、Numeric 格式与输入行为、两者的专用外观
  必须走同一元数据路径；范围重算不得改变 Value 的有效来源，交互 Value 必须保持 TwoWay Binding。
  Designer 加载与代码生成必须按 Design 顺序处理依赖属性，旧 Extra 只能在 metadata 缺失时升级。
- GroupBox 的 Caption 几何/颜色与 Expander 的 Header 几何、展开状态、动画时长和专用外观必须全部
  来自元数据目录；负数/非有限尺寸需统一 Coerce。Expander 交互切换必须保留 TwoWay Binding，
  AnimationDurationMs 应先于 IsExpanded 恢复和生成。新文档/代码不得继续输出对应旧 Extra/直接赋值。
- ScrollView 的 ContentSize、滚动条可见性/粗细、滚轮步长、边框和滚动条颜色必须来自元数据目录；
  ContentSize 使用强类型 Size，尺寸/粗细需统一 Coerce。ScrollXOffset/ScrollYOffset 必须保持可观察但
  为 Transient，不得进入普通属性面板、`props.metadata` 或生成代码；旧 Extra 配置仅作只读升级。
- Panel 的 BorderThickness、CornerRadius、DisabledOverlayColor 必须由所有 Panel 派生容器共享同一 backing；
  ToolBar/PagedGridView/Expander 的派生默认值需由同名派生元数据表达，Reset 后分别恢复 8/8/7。通过 Panel
  基类引用、Style 和 TwoWay Binding 修改后必须得到同一值；ToolBar/StatusBar 禁用绘制不得硬编码遮罩颜色。
- ToolBar/StatusBar 必须同时暴露继承的 Padding(Thickness) 与专用 HorizontalPadding(int)，不得恢复同名
  int Padding。Gap、ItemHeight、TopMost、分段圆角/颜色/显示开关均需通过元数据 Coerce、Binding、
  `props.metadata` 和 `TrySetPropertyValue` 往返；ToolBar 自动高度项应跟随 ItemHeight。旧 Extra 标量只能
  只读升级，StatusBar parts 仍可使用结构化专用路径。
- TabControl 的 SelectedIndex、标题布局、动画、滚动配置和颜色必须来自元数据目录；TitleWidth、
  TitleHeight 与 TitleScrollOffset 使用浮点 DIP。交互选择/滚动必须保留 TwoWay Binding，等值校正不得
  创建 Local 值遮蔽后续 Binding。TitleScrollOffset 为可观察 Transient，页面集合仍走结构化路径；旧
  selectedIndex/titleHeight/titleWidth/titlePosition/animationMode Extra 只能在 metadata 缺失时升级。
- ComboBox 的 SelectedIndex、ExpandCount、动画时长、下拉几何和专用颜色必须来自元数据目录；
  SelectItem/SetExpanded/ScrollBy 及鼠标、键盘交互必须保留 TwoWay Binding。Expand/ExpandScroll 为
  可观察 Transient，Items 仍走结构化路径并在晚于 Binding 到达时重算依赖范围；旧 expandCount/
  selectedIndex Extra 只能在 metadata 缺失时升级。新代码不得输出 SelectedIndex/ExpandCount 裸赋值或
  无效的 Items.Clear()，应通过 std::vector<std::wstring> 调用 Items setter。Items 的直接 insert/remove/move/swap
  必须发布精确集合通知，且选择与虚拟 UIA ID 要跟随逻辑项；DeferNotifications 只发布一次 Reset。
- ListView/ListBox 的视图/选择配置、布局尺寸、滚轮步长和专用颜色必须来自元数据目录；SelectedIndex、
  HoveredIndex、FocusedIndex、ScrollYOffset 必须是可观察 Transient。单选、Ctrl/范围多选与滚动交互必须
  保留 TwoWay Binding，Items 中的多选标志需由 SetItems 一次恢复。Columns/Items 仍走结构化路径，生成代码
  必须先应用 metadata 再 SetItems，不得输出 ListView 专用标量或 SelectedIndex 裸赋值。旧 Extra 标量只能
  在同名 metadata 缺失时升级；ListBox 的 ShowColumnHeaders 派生默认值必须为 false。
  大量 Items/Columns 修改使用 `DeferUpdates()` 时，内部稳定 ID 与选择必须逐次同步，最外层结束时公开集合
  各只发一次 Reset，并只执行一次滚动/UIA/重绘收口；尾部追加的两项工作量诊断必须保持常数级。直接修改
  `ListViewItem::Selected` 后必须用 `Items.NotifyReset()` 重新校正缓存。
- ListView/ListBox 的 Items/Columns、GridView 的 Rows/Columns 与 TreeNode::Children 必须保持 vector 读取兼容，
  同时对直接结构修改发布通知。Grid 列移动必须同步移动所有行的 Cell，批量结束时列对齐先于公开行通知；
  Tree 子节点分离必须清理无效选择/悬停，安全所有权操作优先使用 AddChild/DetachChildAt/RemoveChild/ClearChildren。
- PagedGridView 的 Rows/Columns 直接变更必须同步当前页与全部离屏页，批量列 Reset 后公开观察者只能看到已
  对齐的 Cell；PropertyGrid Items 重排必须保持逻辑选择、活动编辑器与 Binding 身份。TabControl 插入/分离页
  必须按页对象保持选择并更新 TwoWay SelectedIndex；MenuItem::SubItems 批量通知只发一次 Reset，菜单树变化
  必须清除旧 hover/open 路径。拥有型集合的销毁操作必须通过 Detach/Remove/Clear API 明确所有权。
- Control::Children 的直接 insert/erase/Replace/Move/Swap 与 DeferNotifications 必须在公开通知前同步
  Parent、ParentForm、继承样式、Form 捕获/焦点引用、布局和可访问性。空、重复、跨父级、成环及专用容器
  类型错误必须回滚；批处理中内部所有权逐次有效但公开只发一次 Reset。Form 布局临时根必须使用非拥有 span，
  不得把真实 Form 控件挂到临时 Control。销毁优先使用 DeleteControl/DeleteControlAt/ClearControls。

基础 `Control` 的 Text、Visible、尺寸、布局、颜色和校验呈现属性已声明默认值与失效范围；
Grid 行列及 Span 已迁移到 `SetPropertyField`，负索引钳制为 0，Span 至少为 1。

`DataBindings.Add(...)` 会在目标属性不存在、方向所需的 getter/setter 缺失，
或 TwoWay 目标不可观察时返回 `nullptr`。可通过
`DataBindings.LastError()` / `LastErrorMessage()` 获取配置错误；成功创建后的
源/目标读写错误则通过 `Binding::LastError()` 查询。数据源先于目标销毁时会
报告 `BindingError::SourceUnavailable`，不会再解引用悬空源指针。

转换遵循目标元数据和源属性已有类型，并进行整数/浮点范围检查。失败不会改写
原值，分别报告 `TargetConversionFailed` / `SourceConversionFailed`；下一次
合法更新可恢复绑定并清除错误。非标准格式可以传入由 Binding 共享持有的
`IBindingValueConverter`，或直接使用函数式 `DelegateBindingValueConverter`。

源端支持点分属性路径。中间节点使用 `BindingSourceReference(sharedSource)`
显式保存；Binding 会逐级订阅 `PropertyChanged`，中间节点替换时断开旧订阅并
连接新节点。路径暂不可解析时报告 `SourcePathUnresolved`，但保留已解析层级的
订阅，因此节点稍后出现即可自动恢复。空路径段会在创建时以
`InvalidSourcePropertyPath` 拒绝。

`IBindingSource` 的可选源属性元数据会参与创建期校验；`ObservableObject` 自动为
`SetValue(...)` 的属性维护名称、稳定类型及默认能力，也支持 `DefineProperty(...)`
声明只读/静默属性。相关回归应覆盖 `SourceNotReadable`、`SourceNotWritable`、
`SourceNotObservable` 以及嵌套中间源能力。

`IBindingSource` 的可选验证接口与属性值通知彼此独立：
`GetValidationIssues(...)` 提供快照，`ValidationChanged()` 提供可选实时通知。
`ObservableObject` 会规范化空白/重复问题，并按字段或对象级维护 Info、Warning、Error。
相关回归必须覆盖：

- 字段问题的设置、清除、去重和稳定错误码；
- 对象级、中间属性与叶属性问题沿点分路径汇总；
- 中间对象替换后旧验证订阅释放、新订阅建立；
- `OneTime` 只冻结值更新，不遗留旧对象的验证订阅；
- `BindingCollection` 返回带 Target/Source 上下文的汇总结果；
- `BindingCollection` 的变化同步到 `Control::OnValidationStateChanged`，清空绑定后移除表现状态；
- 控件按 Error > Warning > Info 选择主题边框颜色，提示摘要去重并支持最大条数；
- 校验边框、提示、尺寸和 `AccessibleDescription` 已注册为通用目标属性元数据；
- 数据源先销毁、复制 ViewModel 或清空绑定后不暴露陈旧问题且不泄漏订阅。

连接设计时数据源后，Binding 编辑器应预览当前源路径的活动验证问题；该状态不能写入
DataContext Schema、XML 或生成代码。`DataSourceUpdateMode::OnValidation` 仍表示文本类
目标失焦时回写，需要与源端验证通知分别验证。

控件的校验呈现配置与运行时校验结果分开持久化：前者应在 Designer 属性面板可编辑、在
设计文档中往返并生成非默认 C++ 赋值；后者必须保持瞬时。主题切换需同时覆盖
`FormThemeFrame` 的 Info/Warning/Error 边框色和提示浮层前景/背景色。

设计器可以在窗体属性中定义文档级 `DataContext Schema`。Schema 声明点分路径、
值类型和 Read/Write/Observe 能力；非空时会参与 Binding 模式与命名 Converter
源类型校验，并随当前版本 5 XML 往返。版本 5 同时保存文档样式表、稳定控件身份与可选 code-behind；
序列化器仍接受版本 1–4 文档并升级到当前版本。
连接运行时 `IBindingSource` 后，设计器还可递归导入源元数据；验证需包含嵌套属性、
只读能力、循环引用截断和无元数据源的明确失败。

## 批量界面构建与布局事务

`Form`、`Panel` 以及所有 `Control` 都支持可嵌套的 WinForms 风格布局暂停。
批量添加控件或连续设置多个布局属性时，推荐使用异常安全的 RAII scope：

```cpp
auto layoutScope = cui::layout::DeferLayout(*panel);
panel->Add<Label>(L"Name", 12, 12);
panel->Add<TextBox>(L"", 100, 8, 220, 28);
panel->Padding = Thickness(12);
layoutScope.Commit();        // 可选；不调用时离开作用域自动恢复
```

也可以继续使用熟悉的 WinForms 风格显式调用：

```cpp
panel->SuspendLayout();
panel->AddControl(new Label(L"Name", 12, 12));
panel->AddControl(new TextBox(L"", 100, 8, 220, 28));
panel->Padding = Thickness(12);
panel->ResumeLayout();       // 合并脏状态，并立即执行一次 Panel 布局
```

暂停支持嵌套，只有最外层 `ResumeLayout()` 才会传播合并后的布局和重绘请求。
使用 `ResumeLayout(false)` 可只恢复调度，把实际布局留到下一次窗口绘制前完成。

`Add<T>(args...)` 会通过 `unique_ptr` 完成异常安全的构造与挂载；现有
`AddControl(new T(...))` 继续兼容。需要在挂载前配置对象时，可使用
`AddOwned(std::unique_ptr<T>)` 显式转移所有权；指定顺序时使用 `InsertOwned(index, ...)`。

移除接口与添加接口保持所有权对称：`DetachControl(child)` / `DetachControlAt(index)` 从容器分离控件并
返回 `unique_ptr`，适合换父容器；`DeleteControl(child)` 表示移除并销毁。
旧的 `RemoveControl(child)` 仍只解除挂载，保留用于兼容现有调用；它不会
销毁对象，分离后的生命周期仍由调用方负责。

```cpp
auto button = oldPanel->DetachControl(button1);
newPanel->AddOwned(std::move(button));

panel->DeleteControl(obsoleteLabel);
```

## 设计器主链路

每次修改设计器、XML 序列化、代码生成或控件布局后，至少手工走一遍：

1. 启动 `Designer.exe`。
2. 拖放几个基础控件和容器控件，例如 `Button`、`TextBox`、`GridPanel`、`SplitContainer`、`TabControl`。
3. 修改属性，包括 `Name`、`StyleId`、逗号分隔的 `StyleClasses`、`Text`、位置、尺寸、颜色、
   `Visible`，并用 `Ctrl+1` / `Ctrl+2` 切换属性与事件视图，确认两页筛选、折叠组和滚动位置各自保留；
   在事件页填写、解绑并双击处理函数，再验证 Button 分类后的 `Round`、`Width (Auto)` 元数据属性；再修改
   StackPanel 的 Orientation/Spacing/内容对齐、WrapPanel 的 ItemWidth/ItemHeight、DockPanel 的
   LastChildFill，以及 SplitContainer 的方向、距离、宽度、面板最小尺寸、固定状态与分隔条外观，
   确认都位于元数据分类，且旧 Width 与 Auto 规格语义分别保留；拖动分隔条后再保存并重新加载，
   确认新距离被保留。再修改 Slider/NumericUpDown 的负数范围、Step、SnapToStep、Value、Numeric
   小数位/滚轮行为和专用外观，确认保存加载后范围与吸附结果不受属性名称排序影响。继续修改
   GroupBox 的 Caption 间距/圆角/颜色，以及 Expander 的 HeaderHeight、动画时长、展开状态和外观；
   验证负数尺寸被钳制、绑定状态可交互切换，并在保存加载后保持一致。最后修改 ScrollView 的
   ContentSize、滚动条可见性/粗细、滚轮步长与颜色，确认使用强类型元数据；滚动后保存，确认偏移不被持久化。
   再分别修改 Panel、ToolBar、StatusBar、Expander 的圆角和禁用遮罩，确认普通属性面板只出现
   一份属性，保存/重载与生成代码均走 `props.metadata` / `TrySetPropertyValue(...)`，且各派生默认圆角保持不变。
   对 ToolBar/StatusBar 分别设置四边 `Padding` 与整数 `HorizontalPadding`，确认两项同时存在且互不覆盖；
   再修改 Gap、ItemHeight、TopMost、分段外观和显示开关，确认代码中没有恢复直接标量赋值。
   对 ListView/ListBox 修改 ViewMode、SelectionMode、行/磁贴/图标尺寸、FullRowSelect、失焦隐藏选择和颜色，
   验证 Ctrl/范围多选保存重载后仍完整，运行时交互不覆盖 TwoWay Binding，生成代码先写 metadata 再 SetItems。
4. 取消控件选择，在窗体属性中编辑文档样式表：添加强类型资源、Button Class/状态规则及
   Literal/Resource Setter，确认画布立即更新；无效资源引用或冲突状态不能提交。
5. 验证 `Ctrl+Z`、`Ctrl+Y`、`Ctrl+Shift+Z` 能撤销/重做属性、样式表、增删、拖拽、缩放。
6. 分别保存 `.cui.xml` 与 `.cui.xaml`，重新加载，确认层级、样式表、样式标识、metadata 属性、事件映射
   和选中控件无异常；修改文件后使用工具栏“重新加载”，验证 Dirty 提示及错误文件回滚。
7. 生成 C++ 代码，确认包含对应的 `ControlStyleSheet`、`SetStyleId` / `AddStyleClass`、
   `TrySetPropertyValue` 和 `SetStyleSheet` 调用，并编译 Demo 或宿主工程。

## Form 图标回归

1. 不设置 `Form::Icon` 启动窗口，确认标题栏、Alt-Tab 和任务栏使用当前 exe 的程序图标。
2. 显式设置 `Form::Icon` 后再显示窗口，确认自定义图标覆盖默认程序图标。

## Demo WebBrowser 回归

1. 启动 `CUITest.exe`。
2. 切到 `WebBrowser` 页签，等待 WebView2 内容初始化完成。
3. 切到其他页签，例如 `数据控件`。
4. 最大化、还原窗口，确认隐藏页中的 WebBrowser 内容不会漏绘到当前页。
5. 在 WebBrowser 内容中的按钮、文本、链接等区域移动鼠标，确认指针图标跟随 WebView2 内容变化，不会被 CUI 恢复为默认箭头。
6. 在 Designer 放置 WebBrowser，修改 InitialUrl、ZoomFactor、默认上下文菜单、状态栏和缩放控件开关；保存重载后确认值进入 `props.metadata`，生成代码使用 `TrySetPropertyValue(...)`，画布不创建真实 WebView。
7. 在运行时先后于初始化前调用 `TryNavigate`、`TrySetHtml`、`TryNavigate`，确认仅最后一个请求在就绪后执行；初始化失败时确认状态和环境/控制器/WebView HRESULT 可区分失败阶段。

## NotifyIcon / Taskbar 系统集成回归

1. 启动 `CUITest.exe`，确认中文托盘 tooltip 和中文右键菜单显示正确，右键松开后菜单自动弹出。
2. 反复显示/隐藏托盘图标，确认 `IsVisible()` 与实际状态一致；重复调用不产生重复图标，失败可从 `GetLastError()` 取得 HRESULT。
3. 修改嵌套菜单项文本和启用状态，删除后重新打开菜单，确认递归数据与原生菜单同步且重复命令 ID 被拒绝。
4. 同一窗口注册两个不同 ID 的 NotifyIcon，确认鼠标事件和菜单命令不会串到另一个实例；销毁当前 `Instance` 后旧兼容别名回退到仍显示的图标。
5. 保持托盘图标显示并重启 Explorer，确认 `TaskbarCreated` 后图标自动恢复。
6. 创建两个 `Taskbar` 实例并交错销毁，确认无空指针或重复释放；分别验证 Normal、Paused、Error、Indeterminate 和 Clear，非法 Total/句柄返回 false 并保留 HRESULT。

## 鼠标捕获回归

1. 在任意支持拖拽/选择的控件或自定义画布上按住鼠标左键开始拖动。
2. 保持按下状态，把鼠标移出窗口边界继续移动。
3. 在窗口外松开鼠标，再移回窗口，确认控件不会停留在正在拖拽/框选状态。
4. 对滚动条、分割条、文本选择等已有拖拽控件做一次快速回归，确认窗口外松开也能结束操作。
5. 在自绘标题栏最小化、最大化、关闭按钮及其周边移动鼠标，确认不会出现窗口缩放光标，也不能从该区域触发边缘/角落缩放。
6. 在 Button 的 `OnMouseClick` 中同步打开 `OpenFileDialog`，选择文件后确认只提交一次 Click 且对话框不会再次弹出；
   单独向仍有键盘焦点的 Button/CheckBox 发送无配对 `WM_LBUTTONUP`，不得触发 Click 或翻转 Checked。

## 滚轮穿透回归

1. 在 `ScrollView` 内放置 `GridView`、`TreeView`、`RichTextBox` 或另一个 `ScrollView`。
2. 鼠标位于内部控件上方滚动，确认只有内部控件存在有效滚动条且当前方向还能滚动时，滚轮才由内部控件消费。
3. 内部控件滚到顶部后继续向上滚，或滚到底部后继续向下滚，确认滚轮穿透给父级 `ScrollView`。
4. 鼠标位于不支持滚动或当前无有效滚动条的控件上方时，确认父级 `ScrollView` 正常滚动。

## 键盘与无障碍回归

1. 在 Demo 的基础页连续按 Tab/Shift+Tab，确认顺序遵循 `TabIndex`、首尾循环，隐藏/禁用控件会被跳过，焦点框始终跟随当前控件。
2. 在基础页验证 Alt+C、Alt+E、Alt+D、Alt+A、Alt+B；确认按钮、复选框、链接和单选框与鼠标主动作一致，文本中的单个 `&` 不被绘制，`&&` 显示为一个 `&`。
3. 焦点位于普通输入框时按 Enter，确认当前页可见的默认按钮触发；按 Escape 确认可见取消按钮触发。展开的 ComboBox、日期/颜色弹层等仍应优先消费 Enter/Escape。
4. RichTextBox 的 `AllowTabInput=false` 时 Tab 切换焦点；设置为 true 时 Tab 插入文本且不离开控件。
5. 使用 Inspect/Narrator 查询窗口客户区，确认原生 UIA 树保留 Panel 等真实层级，并公开 Name、AutomationId、ControlType、State、KeyboardShortcut 和位置；焦点、勾选、文本、校验和显隐变化应被重新播报。
6. 用 Inspect 验证 Button/Link 的 Invoke、CheckBox/Switch 的 Toggle、TextBox/RichTextBox 的 Value、Slider/NumericUpDown/Progress 的 RangeValue、ComboBox/Expander 的 ExpandCollapse，以及 RadioBox/Tab 的 SelectionItem/Selection Pattern。
7. 用 Inspect 展开 ListView/ListBox、ComboBox、TreeView 和 GridView：确认逻辑项是独立 Fragment，四类容器按实际范围公开 Scroll Pattern；不可滚动轴的百分比为 NoScroll、视口为 100，大小/偏移变化会刷新属性，Scroll/SetScrollPercent 不得改变选择。折叠或离屏项可 Realize/ScrollIntoView，交换/排序后 runtime ID 保持，删除项后旧元素变为不可用。
8. 将 ListView 切换到 Details：确认容器同时公开 Grid/Table，列头、行和单元格形成稳定层级，GetItem(row, column) 可寻址，GridItem/TableItem 的行列与列头关系正确；列移动后单元格身份跟随逻辑行列，删除行/列后旧 Provider 变为不可用。GridView 继续公开 Grid/Table，多选列表的 AddToSelection 不得清空已有项，Grid ScrollIntoView 不得改变选择。
9. 对大型虚拟集合执行首尾子项、相邻兄弟和命中查询，确认 Provider 使用索引入口且不调用旧的整组 `GetAccessibilityVirtualChildren` 回退；ListView Details 只在 GetItem/导航实际访问单元格时物化其 ID。批量重排后逻辑项与单元格 ID 保持，删除后旧节点立即失效。
   ListView Details 与 GridView 都应在仅查询容器/行列数时保持 `MaterializedAccessibilityCellCount()==0`，首次 GetItem 后只增长到 1。滚动 12k 项 ListView 后，`GetVisibleItemRange()` 返回的 `[start,end)` 候选数仍应只覆盖视口；Icon 间隙命中返回 -1，滚动后坐标直接映射到新行。用 ListView `DeferUpdates()` 追加 4k 项时，每次身份/选择工作量保持 1 以内，作用域内无公开集合事件，结束后 Items/Columns 各一次 Reset；移动、交换、前插和删除后选择及已物化单元格身份跟随逻辑项，删除后立即失效。可在 PowerShell 中设置 `$env:CUI_TEST_TIMINGS='1'` 后运行 Release x64 `CUICoreTests.exe` 获取逐测试耗时。本机本轮大型虚拟集合参考值约 29–44 ms，仅用于后续同机趋势比较，不作为跨机器门禁。
10. 密码框只公开显式 `AccessibleName`，不得把密码内容放入 Name、Value 或 Description；关闭窗口后，MSAA 对象应返回 `CO_E_OBJNOTCONNECTED`，UIA Provider 应返回 `UIA_E_ELEMENTNOTAVAILABLE`，均不得访问已释放控件。
11. 打开 Windows 高对比度，确认窗体、公共控件表面、文本和焦点框切换到系统色；再切换关闭动画、始终显示键盘提示和 150%/225% 文字大小，确认过渡立即完成、键盘焦点可见且字体重新布局。退出设置后应自动恢复，无需重启窗口。

## 已知后续清理项

- `CUITest` 已从约 2400 行手工控件构造迁移为外部 `DemoWindow.cui.xaml` + 轻量 C++ 宿主，保留八个页面、
  自定义控件、命名事件和运行时数据；`NavigationView`、`SideBar`、`BreadcrumbBar`、`CalendarView`、
  `DateRangePicker`、`ColorPicker`、`PagedGridView` 已补入统一生产材质化工厂。`Debug/Release × x64/x86`
  四套 `CUITest --validate-xaml` / `--smoke-xaml` 均返回 0，四套 `CUICoreTests` 均为 159/159，四套
  `Designer.exe --self-test` 均返回 0。两个 Release 核心增量链接遇到陈旧 LTCG `LNK1103` 后均以完整
  Rebuild 通过；Release x86 Designer 的依赖 PDB 竞争以 `/m:1` 串行 Rebuild 后通过。
- 当前 `Debug|x64`、`Release|x64`、`Debug|x86`、`Release|x86` 四套默认启用模式的 `CUICoreTests` 均为 147/147，四套对应 `Designer.exe --self-test` 也全部通过；四套 `CuiRuntime.lib` / `CuiRuntimeSample.exe` 和 `CuiStaticGeneratedSample.exe` 均完成独立链接并运行成功。Designer 门禁已覆盖原生 PropertyGrid 的 Bool/Color/Slider/Anchor/EditableEnum 映射、事件默认名/自定义名/签名冲突校验、事件双击零修改复用与可撤销默认名激活、文档级处理函数索引与同签名安全重命名，以及动态 XML 的稳定索引/生产控件工厂/样式/Binding/事件/失败回滚/所有权转移。XAML 门禁覆盖嵌套树、浮点/Auto 尺寸、Binding、强类型资源、Class/`x:Key` 样式、命名事件、规范 XAML/XML 往返、处理函数重命名与事务回滚，以及非法/大小写重复 `x:Name` 的事务性拒绝；运行时门禁还覆盖由真实 Event 成员生成的类型目录、同签名错误成员拒绝、普通/自定义/继承事件包装器、参数名不同但函数类型相同的处理函数复用、同名异类型/未知函数诊断与共享 resolver 后注册，`Load*IntoForm` 原子首次挂载、缺失 Form 函数/根宿主拒绝回滚、同文档重试、附加输出直接 Load 防护和未来 Form 事件缺失 resolver 回滚，按稳定 `DesignId` 保留实例的 XML/XAML 通用属性与元数据增删、Binding 路径/模式和 DataContext Schema 同步、样式资源、事件与窗体显示属性原位重载，属性/Binding/样式/事件复合失败回滚，Grid/Tab/Split 嵌套结构参数变化、子级重排与增删时的最大未变子树重组，Binding/事件附件重建和失败后的拥有权回滚，活动 Binding 冲突及无可复用子树时的完整替换边界，Form 适配宿主后的原位/重组/替换、显示属性与事件解析器自动延续、借用字体和旧连接失败回滚、精确根槽位与候选提交失败回滚、手动转移路径的安全拒绝，稳定 ID/名称常数时间索引与跨 InPlace/Recomposed/Replaced 的类型化引用，以及原子保存识别、防抖、失败签名抑制和下一有效签名恢复的无线程文件监视；`RuntimeDocumentSession` 门禁覆盖首次缺失处理函数完整回滚、补注册后同实例重试、延迟启动 watcher、跨线程 Poll 防护，以及热重载未知函数失败后晚注册 + RequestRetry 原位恢复。设计器生命周期门禁覆盖 `.cui.xaml` 的原子 Save As/Open、完整模型保真、干净保存点、错误文件拒绝及当前画布保持。其余门禁覆盖命名空间限定 `x:Class` 的 `.`/`::` 规范化、按命名空间生成 C++ 类、导出文件名与类名解耦、生成与用户文件分离后的重复导出保真及错误命名空间防覆盖；静态样例进一步以真实编译验证 `GetXxx()` 强类型命名控件访问、`ControlIds` 稳定身份、名称规范化后的全局去重，以及鼠标/文件拖放/属性变化/校验变化四类 `std::bind_front` 事件连线。门禁也覆盖 Anchor 四边组合解析与数值代码生成、滑块单命令提交、属性值重载后的分类折叠/滚动位置保持、ToolBox 七分类/筛选，以及鼠标 Move/Resize、Reparent、同父重排和 Root/普通/Tab/Split 父级的 placement/tree 差量，还覆盖 SplitterDistance 单属性差量的提交、捕获丢失取消、活动事务隔离、精确 Undo/Redo、实例保持、历史冲突重试、手势不合并和 `<32 KiB` 预算。本轮四套默认输出均完成构建与门禁；`CUIEnableWebView2=false` 的 x64 Release 保留既有 138/138 与 Designer 自检记录。
- 本轮事件体验增量在同一四配置矩阵上重新完成 `Rebuild`：事件目录对每个 `UIClass` 恰有一个默认项并提供稳定分类，属性栏显示默认项说明；画布控件双击与 Form 客户区双击分别创建控件默认处理函数和 `MainForm_OnShown`，都复用可撤销属性事务与处理函数激活回调。`Form::OnShown` 在 `Visible=true`、`Show()`、`ShowDialog()` 混合调用下每实例只触发一次。解绑后 `.g.cpp` 中对应 `std::bind_front` 订阅消失，而 `.handlers.g.inc` 与用户函数体继续保留。
- 本轮属性/事件工作流增量再次在 `Debug|x64`、`Release|x64`、`Debug|x86`、`Release|x86` 完成完整 `Rebuild`；四套 `CUICoreTests` 均为 147/147，四套 `Designer.exe --self-test`、`CuiRuntimeSample.exe` 与 `CuiStaticGeneratedSample.exe` 均通过。Designer 门禁新增验证属性/事件视图互斥、动态表头、两套筛选/分类折叠/滚动状态独立保持，以及编辑后回到属性页仍可继续原生 Bool 写入。源码导航门禁覆盖 VS Code、Visual Studio 和自定义编辑器的安全参数计划、带空格及尾反斜杠路径引用，并验证定义定位跳过注释、普通/原始字符串、声明和其他类的同名函数；测试不实际启动外部编辑器。
- 本轮无窗口代码生成增量在同一四配置矩阵完成完整 `Rebuild`：`CuiCodeGen.exe` 四套均成功构建并返回版本信息，设计文件时间戳更新后 `CuiGenerateCode` 均在编译前实际执行。核心门禁新增覆盖 XAML/XML 加载、`x:Class` 与 `d:CodeBehind` 解析、五文件静态样例一致性和非法输出拒绝；CLI 定向门禁确认成功/生成失败/用法错误退出码为 `0/1/2`。连续第二次 Build 证明增量目标跳过、生成文件时间戳及用户文件哈希不变。
- 本轮代码生成链接收口新增 `CuiCodeGenCore.lib`，在 `Debug/Release × x64/x86` 均构建成功；Designer、CLI、核心测试及两套样例四配置运行门禁全部返回 0。项目清单搜索确认 `CodeGenerator.cpp` 与 `DesignCodeGenerationService.cpp` 只由核心库编译一次，三个消费者不再维护重复目标文件。
- 本轮自定义控件扩展在 `Debug/Release × x64/x86` 四套解决方案完成完整构建（Release x64 遇到陈旧增量 LTCG `LNK1103` 后以完整 `Rebuild` 通过）；四套 `CUICoreTests` 均为 151/151，四套 `Designer.exe --self-test`、`CuiRuntimeSample.exe`、`CuiStaticGeneratedSample.exe` 与 `CuiCodeGen.exe --version` 全部返回 0。新增门禁覆盖自定义标签的规范 XAML/v5 XML 往返、保留内置元数据基类、缺失注册的事务性拒绝、线程安全注册表创建真实派生实例、工具代理、专有属性真实元数据探测/规范强类型袋/延迟代码生成，以及真实 include/限定 C++ 类型/构造约定/强类型访问器生成。动态样例实际注册并加载 `StatusBadge`、验证专有 `Severity` 属性及继承注册表的原位 Reload；静态样例通过 `CuiGenerateCode` 生成并编译同一外部控件。
- 本轮 Designer 控件目录在统一 `DesignerControlDescriptor` 之上新增 `cui.designer.controls/1` UTF-8 清单、
  ToolBox 动态分类/注册、`--controls` 启动入口和 `--validate-controls` 无窗口门禁。清单解析覆盖 DTD/4 MiB
  限制、未知内容、规范 C++ 类型、安全 include、重复名称/XAML identity/冲突前缀及事务性不修改；同进程
  `PreviewFactory` 通过 Canvas 注册表跨 Open/Undo/Redo 保持真实实例，错误 `Type()` 被拒绝，无工厂时才显式
  使用基类代理。`Debug/Release × x64/x86` 四套解决方案均构建成功（两个 Release 完整 Rebuild），四套
  `CUICoreTests` 均为 151/151；四套 Designer 自测、动态/静态样例、CodeGen 版本和示例清单校验六项均返回
  0。Designer/CoreTests 四配置同时加入 `/FS`，与已有 `/MP` 配合消除并行 PDB 写入竞争。
- 自定义预览插件边界已冻结为 `DesignerPreviewPluginAbi.h` 的 V1 纯 C、值类型协议：不跨模块传递
  `Control*`、STL、异常、D2D/COM 或分配器；宿主持有基类代理，插件持有 opaque session 并返回有数量/文本
	上限的绘制原语。RAII loader/session、宿主绘制 decorator、样例 DLL、
	`--preview-plugin` 和 `--validate-preview-plugin` 已接入；仅显式受信任路径可加载，设计文件/控件清单仍不能触发 DLL。
- `cui.designer.controls/1` 现支持受限 `property` schema，覆盖 kind/default/editor/choice/数值范围、
  Binding 可读写/通知能力、基类重名和最大数量校验。属性栏、Reset、Undo/Redo、强类型持久化、
  插件 `SetValue` 和结构化 Binding 编辑器已共用同一描述。无设计期 DataContext 时，代理缺少同名运行时
  元数据的自定义属性 Binding 也会先按清单完成模式、源路径、Schema 与 Converter 结构校验；切换 Schema
  不再误报目标属性不存在。`Debug/Release × x64/x86` 四套 `CUICoreTests` 均为 153/153，四套 Designer
  自测、预览插件 ABI、清单+插件桥、动态/静态样例及 CodeGen 版本门禁全部返回 0。
- 自定义控件事件现由同一清单追加受限 `event` schema，并以固定签名预设贯穿 PropertyGrid 默认事件激活、
  Undo/Redo、XAML/v5 XML 契约持久化、文档事件索引、动态 `RegisterCustomControl` 精确成员校验和静态
  `std::bind_front` 生成。解析器拒绝未知契约内容与任意 C++ 签名；Designer 遇到已使用事件与当前清单的
  name/field/signature 不兼容时事务性保留原画布。`Debug/Release × x64/x86` 四套 153/153、Designer
  无窗口自测、清单/插件、动态热重载、静态生成样例及 CodeGen 版本门禁均已通过；Release x64 的已知陈旧
  LTCG `LNK1103` 由完整 Rebuild 恢复后通过。
- ComboBox Items、TreeView 节点、递归 Menu Items、GridView 列、GridPanel 行列定义和 StatusBar 分段编辑已从完整文档快照
  收口为 `ControlStructureCommand` 单控件强类型差量。Designer 自测验证小型列/ComboBox 命令 `<32 KiB`、
  Undo/Redo 保持控件实例、expected 起点冲突不消费历史且修复后可重试，以及六类状态精确恢复；ComboBox
  额外覆盖列表缩短后 Local/Binding 索引、Binding 配置和 metadata 跟踪的原子恢复，Menu 覆盖多级层次、
  命令 ID、快捷键、禁用项和顶层/子级分隔符。TabControl Pages 与 ToolBar Buttons 现由
  `ControlOwnedCollectionCommand` 转移直接子树所有权，并保留页内/按钮 DesignerControl 包装器、扁平顺序、稳定 ID、
  完整选择、ToolBar 尺寸覆盖及 Tab `SelectedIndex` 的 Local/Binding/配置/metadata 状态。门禁覆盖实例保持、页内子控件、
  新按钮持久化、旧版未包装按钮修复、expected 冲突保留与修复后重试，以及小命令 `<32 KiB`；八类结构编辑均不再走完整文档事务。
- 本轮 `Debug/Release × x64/x86` 四套解决方案均完成完整重建，并在最终修正后复验：四套 `CUICoreTests` 均为
  153/153，四套 `Designer.exe --self-test`、`CuiRuntimeSample.exe`、`CuiStaticGeneratedSample.exe` 与
  `CuiCodeGen.exe --version` 全部返回 0。
- 代码持久化继续收口：`DesignCodeGenerationService::BuildCodeBehindAssociation` 统一规范类名并在生成前验证
  无扩展名输出和可移植相对路径，失败不留下半成品关联；Designer 重复导出保留已有命名空间 `x:Class`，仅更新
  `d:CodeBehind`。用户事件定义按 token 化参数类型匹配，空白/参数名调整可复用，同名类型漂移在五文件写入前
  拒绝且保持原内容。本轮再次通过 `Debug/Release × x64/x86` 四套解决方案构建、153/153、Designer 自测、
  动态/静态样例与 CodeGen 版本门禁；两个 Release 的陈旧 LTCG `LNK1103` 均以完整 Rebuild 恢复。
- Designer 代码导出新增 `DesignCodeExportPlan` 与类名确认对话框：显示当前类、输出基路径和最终相对关联，
  区分新建/保持/迁移/改变输出四种状态；非法类名禁用提交，显式迁移提示旧用户代码不会被改写。空窗体不再
  被旧的“无控件”检查阻止，Form-only `OnShown` 已覆盖生成基类、`std::bind_front` 和用户桩三层门禁。
  最终源码已通过 `Debug/Release × x64/x86` 四套解决方案构建、153/153、Designer 自测、动态/静态样例和
  CodeGen 版本门禁；两个 Release 使用完整 Rebuild。
- 交互导出通过 `DesignCodeGenerationService::GenerateAndCommit` 协调五文件与 code-behind 文档事务；关联失败或
  异常会使用 `AtomicFileBatchSnapshot` 和原子写入/删除批次恢复生成前状态。工具栏新增“重新生成”，只在当前
  文档可解析到显式目标时启用并复用同一生成服务。核心门禁新增快照存在性/内容恢复、删除后续失败回滚、无临时
  工件和关联失败注入。最终源码已通过 `Debug/Release × x64/x86` 四套解决方案构建、154/154、Designer 自测、
  动态/静态样例与 CodeGen 版本门禁；两个 Release 使用完整 Rebuild。
- Release 配置若沿用历史 LTCG/IPDB/IOBJ 中间产物，增量链接可能报告 `LNK1103`；对对应配置执行一次 `/t:Rebuild` 可重新生成一致的调试信息，本轮 x64/x86 Release 均已通过完整重建。
- Form/控件事件赋值、默认事件激活和文档级处理函数重命名已从完整文档快照收口为 `EventHandlerCommand`。
  命令按稳定 ID 验证全部 expected 映射，在副本上构建目标事件表后以 swap 提交；门禁覆盖 legacy 值恢复、
  Form/多控件 Undo/Redo、实例保持、签名冲突、过期起点拒绝、不消费历史及 `<32 KiB` 预算。最终源码再次通过
  `Debug/Release × x64/x86` 四套解决方案构建、154/154、Designer 自测、动态/静态样例与 CodeGen 版本门禁；
  两个 Release 使用完整 Rebuild。
- 代码生成新增无写入 `BuildFilePlan` 与 `InspectFreshness`：五文件缺失、`.g.*` 手工漂移、新事件缺用户桩和
  用户类身份阻塞分别归类，合法用户扩展保持 Current；检查不创建输出目录且不改变时间戳。Designer 工具栏以
  `*` / `!` /“生成受阻”呈现状态，文档提交防抖复核，Undo 回到已生成状态立即恢复，窗口激活复查外部漂移。
  核心门禁增至 155 项，Designer 自测覆盖真实按钮文字、可访问说明、事件 Undo/Redo、漂移、缺失和阻塞恢复。
  最终源码已在 `Debug/Release × x64/x86` 四套配置完成构建（两个 Release 使用完整 Rebuild）；四套
  `CUICoreTests` 均为 155/155，四套 `Designer.exe --self-test`、`CuiRuntimeSample.exe`、
  `CuiStaticGeneratedSample.exe` 与 `CuiCodeGen.exe --version` 全部返回 0。
- 生成头新增零所有权 `ClassReferences<TDocument>`：每个正稳定 ID 控件同时得到当前实例 `GetXxx()` 和跨
  Reload 重新解析的 `ReferenceXxx()`；模板不直接包含 CuiRuntime，静态消费者不会被迫增加运行时依赖。
  `CuiStaticGeneratedSample` 真实链接 CuiRuntime、加载 XAML，并验证强类型引用跨 InPlace Reload 后继续指向
  当前控件。生成契约提升为 2，versioned stamp 在语义升级时使旧缓存失效；Debug x64 连续 Build 还验证即使
  exe 再链接，第二次生成次数仍为 0，stamp 与 `.g.h` 时间戳不变。最终源码在 `Debug/Release × x64/x86`
  四套配置通过构建、155/155、Designer 自测、动态/混合样例与 `CuiCodeGen 2` 门禁；两个 Release 使用完整
  Rebuild。
- 生成头进一步新增 `ClassEventSink`：事件目录公开真实声明类型，生成器据此输出精确成员指针和
  `std::bind_front`，同时覆盖 Form、通用/专用控件及固定签名的自定义控件事件；静态生成窗体复用同一纯虚
  接口，动态 XAML 控制器可直接实现该接口并调用 `RegisterDynamicEventHandlers`。运行时 `RegisterBatch`
  保持共享解析状态并在重复项、显式失败或异常时原子恢复整批路由；混合样例验证注册失败回滚、首次加载和
  InPlace Reload 后的控件/Form/自定义事件连续性。生成契约提升为 3。最终源码在 `Debug/Release × x64/x86`
  四套配置通过构建，四套 `CUICoreTests` 均为 156/156，Designer 自测、动态/混合样例与 `CuiCodeGen 3`
  门禁全部返回 0；两个 Release 使用完整 Rebuild。
- 动态事件注册新增移动 `RegistrationLease` 与单调路由令牌：`RegisterScopedBatch` 仍在同一共享 State 上原子
  提交，但租约析构只移除本批令牌范围，保留同名处理函数已有的其他路由；失败、异常和嵌套批次均不泄漏状态。
  生成的不可复制/移动 `ClassEventSink` 自动把租约与本次注册的弱生命周期令牌共同持有，切换注册表、显式解绑
  或析构时先使已加载 RuntimeDocument 持有的旧 EventConnection 变为 no-op，再撤销未来 resolver 路由，避免
  `std::bind_front(this)` 悬空。混合样例真实覆盖重复注册保留旧租约、跨注册表替换、旧控件/Form/自定义事件
  不再回调、显式解绑和析构回收。生成契约提升为 4；Debug x64 连续 Build 仍为生成 0 次且 v4 stamp/`.g.h`
  时间戳不变。最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，四套 `CUICoreTests` 均为 156/156，
  Designer 自测、动态/混合样例与 `CuiCodeGen 4` 门禁全部返回 0；两个 Release 使用完整 Rebuild。
- Designer 事件页新增逐处理函数代码诊断：未关联、源文件缺失、待生成、已实现、签名错误与重复定义均在
  属性行直接显示；缺失定义仍走原子生成，已实现项直接打开源码，签名错误和重复定义会绕过必然失败的生成并
  定位现有定义。生成校验、无写入诊断与源码定位统一复用 `CppUserCodeIndex`，其词法扫描会忽略注释、普通/
  raw string，并按精确类名和参数类型选择重载；重复的相同签名定义会在写入前拒绝。最终源码在
  `Debug/Release × x64/x86` 四套配置通过构建，四套 `CUICoreTests` 均为 156/156；四套 Designer 自测、
  动态/混合样例、静态生成样例与 `CuiCodeGen 4` 门禁全部返回 0，两个 Release 使用完整 Rebuild。
- 事件 F4/下拉候选从仅复用文档引用扩展为同时发现用户 `.cpp` 中尚未绑定的兼容成员。发现仍复用
  `CppUserCodeIndex` 的精确参数类型规则，只提供当前 `x:Class` 下唯一兼容定义；构造函数、`operator`、错误
  签名、重复兼容定义及注释/string/raw string 伪代码均被排除，已被文档中另一事件签名占用的名称也不会显示。
  核心门禁覆盖源码发现和过滤，Designer 自测验证候选呈现及跨签名冲突隐藏。最终源码在
  `Debug/Release × x64/x86` 四套配置通过构建，四套 `CUICoreTests` 均为 156/156；四套 Designer 自测、
  动态/混合样例、静态生成样例与 `CuiCodeGen 4` 门禁全部返回 0，两个 Release 使用完整 Rebuild。
- `CppUserCodeIndex` 新增 namespace 作用域解析：全限定成员、逐层嵌套 `namespace` 和 C++17
  `namespace Acme::Views` 内的短类名成员均规范到同一 `x:Class`；相邻/匿名 namespace 及函数体伪定义不会
  命中。用户默认构造函数身份检查也复用该索引并允许初始化列表，避免事件诊断已识别而重新生成拒绝的分裂
  语义。核心门禁验证 namespace 内已有事件不会追加重复桩、候选发现和构造函数新鲜度保持 Current；Designer
  自测验证同一写法可精确导航且错误命名空间返回 0。最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，
  四套 `CUICoreTests` 均为 156/156；四套 Designer 自测、动态/混合样例、静态生成样例与 `CuiCodeGen 4`
  门禁全部返回 0，两个 Release 使用完整 Rebuild。
- 文档级处理函数重命名新增显式“同时迁移用户函数体并重新生成代码”选项：仅在旧名称有唯一兼容定义、目标
  没有同签名定义且不是合并操作时启用。`CppUserCodeIndex` 只替换成员定义名称 token，函数体、注释和字面量
  保持原字节；`EventHandlerCommand` 历史只保存路径/类/签名/名称元数据，保持 `<32 KiB`，每次
  Execute/Undo/Redo 都重新预检源码、捕获五文件快照、迁移并重新生成。Designer 自测真实验证函数体保留、
  生成代码同步、Undo/Redo 双向迁移、外部源码冲突不消费历史且修复后可重试，以及用户头身份导致的生成失败
  会同时恢复事件映射和五文件。最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，四套
  `CUICoreTests` 均为 156/156；四套 Designer 自测、动态/混合样例、静态生成样例与 `CuiCodeGen 4` 门禁
  全部返回 0，两个 Release 使用完整 Rebuild。
- `CppUserCodeIndex` 新增位置保持的预处理掩码：所有指令及续行宏都不再向 C++ 作用域扫描贡献 token，确定的
  `#if 0` / `#if 1`、嵌套条件、`#elif` 和 `#else` 失活分支会被排除；未知宏条件保守保留所有可能分支，注释
  与 raw string 中的伪指令不会改变状态。核心门禁验证候选发现、生成诊断和显式函数体迁移只命中活跃定义，
  Designer 自测验证宏花括号和失活同名函数不会改变源码跳转行号。最终源码在 `Debug/Release × x64/x86`
  四套配置通过构建，四套 `CUICoreTests` 均为 156/156；四套 Designer 自测、动态宿主、静态生成样例与
  `CuiCodeGen 4` 门禁全部返回 0，两个 Release 使用完整 Rebuild。
- `RuntimeControlRef<T>` 不再保存可悬空的裸 `RuntimeDocument*`，改为按稳定 ID 通过弱文档生命周期状态解析。
  目标文档的 Load/Reload/移动赋值仍保留既有引用，移动构造会迁移引用状态；文档销毁或赋值源失效后
  `Get()` 安全返回空，且引用不拥有文档或控件。核心门禁新增覆盖销毁、移动构造、目标/源移动赋值语义；静态
  生成样例通过 `ClassReferences::ReferenceXxx()` 验证已附加 Form 的动态文档移动后仍解析当前强类型实例。
  最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，四套 `CUICoreTests` 均为 157/157；四套 Designer
  自测、动态宿主、静态生成样例与 `CuiCodeGen 4` 门禁全部返回 0，两个 Release 使用完整 Rebuild。并行清理
  共享 x64 Release 输出时曾出现一次 `WebView2Loader.dll` 删除竞争警告，随后独立构建 CodeGen 链无警告通过。
- 新增零所有权 `RuntimeDocumentRef`，并让生成的 `ClassReferences<TDocument>` 保存
  `document.Reference()` 的弱视图，不再保存裸 `TDocument*`。生成视图提供布尔有效性与 `TryDocument()`；
  `GetXxx()` / `ReferenceXxx()` 跨 Reload 和文档移动继续解析，文档销毁后统一返回空。核心门禁验证文档视图
  的移动、销毁及目标/源赋值语义；真实静态样例进一步验证已附加 Form 的视图跨移动和临时文档销毁边界。
  生成服务、CLI 与 `CuiCodeGen.targets` 契约同步提升为 5，样例五文件已由新 CLI 重新生成。最终源码在
  `Debug/Release × x64/x86` 四套配置通过构建，四套 `CUICoreTests` 均为 157/157；四套 Designer 自测、动态
  宿主、静态生成样例与 `CuiCodeGen 5` 门禁全部返回 0，两个 Release 使用完整 Rebuild。
- 用户头类身份验证移除 `CodeGenerator.cpp` 私有简化 tokenizer，统一复用 `CppUserCodeIndex` 的预处理掩码和
  namespace 作用域。现在只接受精确 `x:Class` namespace 中唯一、直接继承预期 `Generated` 基类的活跃类体；
  导出宏、`final`、访问说明、多基类及全限定 Generated 基类保持合法。核心门禁验证 `#if 0` 假类与相邻
  namespace 同名真类不能伪造身份，阻塞的 Generate 保持五文件逐字节不变，恢复正确头后新鲜度回到 Current。
  最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，四套 `CUICoreTests` 均为 157/157；四套 Designer
  自测、动态宿主、静态生成样例与 `CuiCodeGen 5` 门禁全部返回 0，两个 Release 使用完整 Rebuild。
- 默认构造函数持久化检查从“用户源必须拥有定义”收口为用户头/源联合索引：类体内联、委托构造与
  `= default` 可直接复用，`= delete`、跨文件重复默认构造及已有源却缺少定义会在首次写入前阻塞。若用户源
  丢失但头中已有构造定义，重建只补 marker/include 和当前事件桩，不会生成重复构造体；委托初始化列表中的
  类名调用也不会被误计为第二个定义。最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，四套
  `CUICoreTests` 均为 157/157；四套 Designer 自测、动态宿主、静态生成样例与 `CuiCodeGen 5` 门禁全部返回 0，
  两个 Release 使用完整 Rebuild。
- 五文件代码计划新增乐观并发前置条件：在读取用户代码前捕获全部目标的存在性与字节，原子批次在预写入、
  逐目标提交和备份后复核，stale plan 遇到 IDE 外部修改或新建文件会整批零写入拒绝。`GenerateAndCommit`
  改为单次计划/提交，关联失败仅在目标仍等于本次生成结果时条件回滚；回调期间的外部修改会保留并报告恢复
  冲突。事件函数体迁移及 Undo/Redo 也改用条件写入和条件快照恢复。核心门禁覆盖已有文件修改、缺失目标新建、
  后置冲突零写入、条件恢复和回调期间外部编辑保留。最终源码在 `Debug/Release × x64/x86` 四套配置通过构建，
  四套 `CUICoreTests` 均为 157/157；四套 Designer 自测、动态宿主、静态生成样例与 `CuiCodeGen 5` 门禁全部
  返回 0，两个 Release 使用完整 Rebuild。
- 事件实现索引从仅识别类外定义扩展为联合用户 `.h/.cpp`：精确用户类体内的 `void Handler(...) {}` 可直接
  复用，`.handlers.g.inc` 自动省略冲突声明；头源重复、错误签名仍在五文件写入前拒绝。逐事件诊断、同签名
  候选、源码导航和显式函数体迁移都会记录并使用实际定义文件，Designer 自测覆盖头内函数体重命名及
  Undo/Redo。索引进一步要求处理函数是可覆盖生成虚函数的非静态、非 cv/ref `void` 成员；错误返回类型、
  `static`、`const`/ref、删除定义及类外 `noexcept` 会成为可导航的签名错误，并从候选和函数体迁移中排除。
  静态样例把 `HandleWindowShown` 作为用户头内 `noexcept` 实现，四配置真实编译并运行，证明生成声明省略和
  `std::bind_front` 覆盖链成立。MSBuild 增量输入同步加入用户 `.h/.cpp`，契约提升为 6；连续第二次 Build 不启动生成器。最终源码
  在 `Debug/Release × x64/x86` 四套解决方案完成 Rebuild，四套 `CUICoreTests` 均为 157/157；四套 Designer
  自测、动态宿主、静态生成样例与 `CuiCodeGen 6` 门禁全部返回 0，两个 Release 使用完整 Rebuild。
- `CuiCodeGen.targets` 的增量输入从设计文件、规则和用户 `.h/.cpp` 扩展为全部五个代码文件。真实门禁向静态
  样例 `.g.cpp` 写入漂移探针后执行普通 Build，生成器恢复规范字节并刷新 v6 stamp；紧接着的无变化 Build
  保持 stamp、哈希和时间戳。用户 `.h` 探针则触发预检但原样保留，三个生成文件均未改写。四配置首次 Build
  都因 targets 变化执行生成，连续第二次 Build 全部跳过；四套 157/157 核心测试、Designer 自测、动态/静态
  样例与 `CuiCodeGen 6` 门禁再次全部返回 0。
- `.handlers.g.inc` 现在为当前绑定且由用户源文件实现的处理函数生成 `override`，事件解绑时把保留声明降级为
  普通成员，重新绑定后恢复覆盖契约；类体内联实现仍省略生成声明。旧声明解析同时接受普通和 `override`
  形式，保证跨代解绑不会丢失用户函数声明。生成契约提升为 7。最终源码在 `Debug/Release × x64/x86` 四套
  解决方案完成 Rebuild，四套 `CUICoreTests` 均为 157/157；四套 Designer 自测、动态宿主、静态生成样例与
  `CuiCodeGen 7` 门禁全部返回 0。紧接着四套普通 Build 均未启动生成器（`codegen=0`）。
- `TextBox` / `PasswordBox` 的输入、选择、拖放和光标绘制转换 warning 已收敛；`ComboBox` 的索引、滚动和下拉绘制转换 warning 已收敛；`GridView` 的行列索引、组合框单元格、编辑路径和主要绘制转换 warning 已收敛；`TabControl` 的页签绘制和拖放循环转换 warning 已收敛；`RichTextBox` 的滚动、绘制和拖放循环转换 warning 已收敛。
- `CUITest` Demo 和自定义示例控件的常见转换 warning 已收敛。
- `Button`、`Label`、`LinkLabel`、`CheckBox`、`RadioBox`、`ProgressBar`、`ProgressRing`、`PictureBox`、`GroupBox`、`LoadingRing`、`Slider`、`RoundTextBox`、`Switch`、`ToolBar`、`SplitContainer`、`StatusBar` 等小控件绘制转换 warning 已收敛。
- `Panel`、`ScrollView`、`TreeView` 的容器循环和主要绘制转换 warning 已收敛。
- 设计器模态编辑器的列表索引类 warning 已收敛。
- `Utils` 中第三方 `sqlite3.c` 的 C4028/C4113 签名兼容 warning 已通过 `Utils.vcxproj` 文件级 `DisableSpecificWarnings` 隔离，未修改 vendored 源码。
