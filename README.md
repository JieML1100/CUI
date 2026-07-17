# CUI - 现代化 Windows GUI 框架

[简体中文](README.md) | [English](README.en.md) | [完整文档](ReadMeFull.md)

[完整文档(英文)](ReadMeFull.en.md)

一个基于 **Direct2D** 和 **DirectComposition** 的 Windows 原生 GUI 框架（C++20），并提供配套的 **可视化设计器**（拖放设计 + XML/XAML 保存与加载 + 自动生成 C++ 代码）。

本仓库主要包含：
- `CUI/`：运行时 GUI 框架与控件库
- `CuiDesigner/`：可视化 UI 设计器
- `CUITest/`：示例与测试程序
- `D2DGraphics/`：底层图形封装
- `Utils/`：设计器等项目仍在使用的通用工具库

## 特点

- **高性能渲染**：Direct2D 硬件加速 + DirectComposition 合成
- **控件与布局**：提供46+常用控件
- **控件与布局**：提供多种布局容器（如 Stack/Grid/Dock/Wrap/Relative 等）
- **事件与输入**：完善的鼠标/键盘/焦点/拖放事件，支持 IME 中文输入
- **通用数据绑定**：基于控件属性元数据，支持 OneWay、TwoWay、OneWayToSource、OneTime、嵌套属性路径和转换器
- **资源支持**：内置 SVG 渲染（nanosvg 已包含）
- **多媒体功能 集成**：媒体播放器（MediaPlayer）
- **WebView2 集成**：可嵌入现代 Web 内容（基于 Microsoft WebView2）
- **设计器工作流**：拖放编辑属性、实时预览、XML/XAML 设计文件保存/加载、自动生成 C++ 代码

## 数据绑定

运行时绑定不依赖硬编码的控件类型或目标属性。控件通过属性元数据声明读、写和变更通知能力，`BindingCollection` 根据绑定模式自动校验：

```cpp
ObservableObject viewModel;
viewModel.SetValue(L"Name", std::wstring(L"CUI"));
textBox->DataBindings.Add(
    L"Text", viewModel, L"Name", BindingMode::TwoWay);
```

这份元数据现在也是控件属性系统的统一契约。`ControlPropertyOptions` 可声明默认值、
Coerce、精确比较器、Changed 回调以及 `AffectsMeasure` / `AffectsArrange` /
`AffectsRender`；需要让公开 setter 从第一次赋值起就表示 Local 值时，可再声明
`TracksLocalValue`。自定义控件的 setter 使用受保护的 `SetPropertyField(...)` 后，直接 C++
赋值、`TrySetPropertyValue(...)` 和 Binding 写入会共享相同的规范化、失效与
`OnPropertyValueChanged` 通知；`ResetPropertyValue(...)` 和
`IsPropertyValueDefault(...)` 则让 Designer 和代码生成器不再硬编码默认值。

属性值按 `Local > Binding > Style > Theme > Default` 取最高优先级。各层通过
`TrySetPropertyValue(name, value, source)` 写入，通过 `ClearPropertyValue(...)` 或
`ClearPropertyValues(source)` 移除；隐藏层仍保留最新值，重新成为最高层时会自动恢复。
Binding 会独占并在清除时释放自己的层；活动 Binding 的层不能由普通属性 API 覆盖或清除，
同一目标属性不允许重复绑定（包括直接构造的 Binding）。交互控件更新当前值时应使用
`SetCurrentPropertyField(...)`，这样 TwoWay Binding 不会被意外替换成 Local 值。

```cpp
button->TrySetPropertyValue(
    L"BackColor", BindingValue(themeColor),
    ControlPropertyValueSource::Theme);
button->ClearPropertyValue(
    L"BackColor", ControlPropertyValueSource::Theme);
```

`ControlStyleSheet` 在这套来源模型之上提供控件级主题和样式。规则可按运行时类型、StyleId、
多个 StyleClass 以及 Hovered/Focused/Pressed/Disabled/Checked 等状态匹配；同一属性按
ID、Class/状态、类型的特异性和规则顺序级联。资源引用与键名匹配不区分大小写，修改规则或
资源后，已附着控件会自动刷新；附着到根控件时会递归应用，之后加入的子控件也会继承。

```cpp
auto theme = std::make_shared<ControlStyleSheet>();
theme->SetResource(L"Accent", BindingValue(accentColor));

ControlStyleSelector hoveredButton;
hoveredButton.Type = UIClass::UI_Button;
hoveredButton.RequiredStates = ControlStyleState::Hovered;
theme->AddRule(hoveredButton, {
    ControlStyleSetter::Resource(L"BackColor", L"Accent")
});

form->SetThemeStyleSheet(theme); // 递归应用 Theme 层
```

`Button`、`TextBox`、`ComboBox` 的常用状态色、边框、圆角和间距已经接入同一套属性元数据，
可直接由 Theme/Style/Binding 设置。Designer 属性面板也可编辑 `StyleId` 和逗号分隔的
`StyleClasses`；两者会随 XML 设计文件往返，并写入生成的 C++ 代码。

未选中控件时，窗体属性面板还提供“编辑文档样式表”入口。结构化编辑器可维护强类型资源、
类型/ID/Class/状态选择器和属性 Setter，修改后立即应用到设计画布；无效资源引用、冲突状态或
不能按属性元数据转换的值会在保存前被拒绝。文档样式表随 XML 往返，代码生成器会输出等价的
`ControlStyleSheet` 初始化与 `SetStyleSheet(...)` 调用。

Setter 属性列表也直接来自所选控件类型的运行时属性元数据，并自动推断 Bool、数值、枚举、
Color、Thickness、Size 或 Length 类型及示例值。即使画布尚未放置该类型，Designer 也会创建
轻量探针检查属性存在性、可写性、类型转换与 Coerce，因此错误不会延迟到以后添加控件时才暴露。

普通控件属性面板现在直接由包含 Legacy 属性的目录视图生成全部可浏览标量。Text、位置、尺寸、
颜色、Margin/Padding、对齐等常用属性不再另有一份显示分支，编辑时也经过相同的运行时元数据，因此
Coerce、变更回调以及 Local/Style/Binding 优先级不再被设计器直接字段赋值绕过。统一访问层按
`Persistence` 自动维护可选的 `props.metadata` 强类型属性包：Metadata/Automatic 写入规范值，
Legacy/Transient 则主动移除重复项；重置会清除 Local 值并显露下一个 Style、Binding、Theme 或
默认值。旧 XML 的既有字段保持兼容，加载、撤销/重做和 C++ 生成继续使用规范名称和值类型。

`ControlPropertyOptions::Design` 可进一步声明属性是否可浏览、显示名、分类与排序、首选编辑器、
强类型选项、数值范围和持久化策略。普通属性面板按这些描述分组并自动选择 Boolean、Choice、
Color、Thickness、Size、Length 或数值/文本编辑器；`Legacy` 与 `Transient` 属性不会误写入通用
metadata 包，但仍可作为 Binding 或样式 Setter 的目标。

`X` / `Y` / `Enabled` / `Dock` 只是规范属性 `Left` / `Top` / `Enable` / `DockPosition` 的显示名；
Grid 行列和 Dock 只在对应父容器中出现。进度环、日期选择器、PictureBox、TreeView 等原先由控件类型
分支显示的标量也已注册为属性元数据。ComboBox Items、GridView Columns、Tab Pages、ToolBar Buttons、
Tree Nodes、Grid Definitions、Menu Items 与 StatusBar Parts 仍使用结构化对话框，但入口统一来自可扩展的
`DesignerCustomEditorCatalog`，属性面板不再维护控件类型 `if/else` 链。八类对话框在打开前统一建立文档事务；
只有用户确定且后置文档有效时才以一条命令提交，取消和无变化不入栈，异常、嵌套编辑、捕获或入栈失败会
恢复编辑前文档及完整选择。事务返回 `Begun`、`Committed`、`Unchanged`、`RolledBack`、`Canceled`、
`Aborted`、`Rejected` 或 `Failed`，调用方可区分无变化、业务拒绝和基础设施失败；取消时若检测到对话框
泄漏了修改，会自动恢复而不是直接丢弃前置快照。

不属于运行时元数据、但属于控件设计包装器的 Name、Anchor、StyleId、StyleClasses、字体覆盖和
MediaPlayer 媒体路径，也统一由 `DesignerControlPropertyCatalog` 描述。PropertyGrid 通过 Binder
捕获、应用和恢复这些值，唯一命名、字体继承、Anchor 保持边界和设计期附加数据不再散落在文本、
布尔或浮点编辑分支中；未知属性或类型错误会直接拒绝，不再回退为裸字段赋值。

`DesignerPropertyRowCatalog` 再把窗体目录、包装器专用目录和运行时元数据投影为同一种属性行：统一携带
来源、当前强类型值、分类/顺序、编辑器、Choice、数值范围、Reset 能力，以及 Binding/Validation/Style/
Theme 诊断。控件属性会在投影阶段按名称
去重并全局排序，因此 Common/Layout/Appearance 等分类只生成一次。设计器把这条行流直接投影到 CUI 原生
`PropertyGridView`：Boolean、Enum、Color 和 Slider 使用原生编辑器，颜色选择复用 `ColorPickerPopup`；
混合值、Reset、结构化 Action 行和滑块编辑事务作为通用能力扩展在 `PropertyGridView` 内，不再由设计器逐行
拼装 TextBox/CheckBox/ComboBox/Button。诊断会给出绑定路径、模式、Converter、预览连接状态、源端
校验问题，以及提供候选值的样式规则 ID/特异性和被更高优先级值遮蔽的原因。

画布多选会把完整选择集合交给同一 Binder。属性面板只显示所有所选控件中 kind、编辑器与约束兼容的
公共属性，并明确标记“多个值”与“混合来源”；`Name` 这类对象身份属性不会参与批量编辑。输入或选择新值
会先对全部目标做类型/绑定所有权预检，再一次应用，并把整批修改连同完整选择集写成一条撤销命令。
至少一个目标仍由 Binding 拥有的属性显示为只读，Apply 与 Reset 都不能偷偷写入 Local；不同目标的诊断
不一致时会单独标记，而不会把主选控件的详情冒充整组状态。

属性写入与 Reset 进一步统一到 `DesignerPropertyEdit` 事务服务：服务先验证全部目标，再捕获每个目标的
Local/包装器值与 metadata 跟踪快照；任何 setter 拒绝或抛出异常都会逆序恢复已经触碰的目标，并返回包含
控件名的错误。属性面板顶部保留固定、可访问的错误状态区，成功编辑或切换选择后清除。普通标量
Apply/Reset 与分组滑块直接提交逐属性差量；DataContext Schema、文档样式、Binding 和结构编辑器复用
`DesignerCanvas` 的结果型事务。ComboBox Items（连同 `SelectedIndex` 的 Local/Binding 值来源、Binding
配置和 metadata 跟踪）、TreeView 节点、GridView 列、GridPanel 行列定义及 StatusBar 分段使用
`ControlStructureCommand` 的单控件强类型差量；递归 Menu Items 也保留文本、命令 ID、快捷键、启用状态、
分隔符和层级所有权。TabControl 页面与 ToolBar 按钮使用 `ControlOwnedCollectionCommand`，在缺席状态持有直接子树，
并原样恢复页内/按钮包装器、稳定 ID、选择、尺寸覆盖及 Tab `SelectedIndex` 的 Local/Binding 状态，不再保留完整文档。
PropertyGrid 不再自行复制前后文档/选择捕获、命令构造或失败恢复；
滑块连续编辑在预览失败或提交失败时同样恢复拖动前状态。

`PropertyGrid::ApplyPropertyValue(...)`、`ResetPropertyValue(...)` 和只读的行/错误查询接口把同一条
生产交互路径开放给自动化，而不会绕过 Binder 或命令栈。`Designer.exe --self-test` 可在不创建窗口的
情况下构造真实 `DesignerCanvas` 与 `PropertyGrid`，验证混合值、多选批量修改、拒绝错误、Reset、
完整选择、Undo/Redo 的文档重建、事务状态、取消时泄漏修改恢复、业务拒绝/异常回滚，以及设计期 Binding
连接/断开时 Local 后备值的恢复，作为模型单元测试之外的 Designer 运行时冒烟门禁。

设计器工具箱按“基础控件、输入、布局、数据与列表、状态与反馈、导航与外壳、媒体与 Web”分组，支持中文
名称、C++ 类型名和分类的多关键词筛选。每种控件使用代码原生的矢量轮廓图标，窄侧栏中的长类型名会单行
省略，避免原先所有控件共用一个 SVG 和简单按钮造成的辨识困难。

设计器命令的 `Execute()`、`Undo()` 和 `Redo()` 现在贯穿返回同一结果对象；恢复失败或抛出异常时会保留
错误与 `DocumentRestored` 状态，命令也保留在原撤销/重做栈中。空历史明确返回 `Unchanged`，不再和失败共用
一个 `false`。画布 Add/Delete 使用 `ControlSubtreeCommand`：挂载时由运行时树唯一拥有，缺席时由命令以
`unique_ptr` 拥有分离根，并保存规范化子树节点、父级定位器、同级顺序、ToolBar 尺寸覆盖和完整选择，不再把整份文档存入历史。
增删与移动/缩放的 placement/tree 差量都采用“先成功捕获、再允许修改”的规则；起点冲突会保留原历史项，后置捕获或命令入栈失败会恢复此前状态和选择。键盘微调、鼠标
拖拽、resize 和 SplitContainer 分隔条都使用结果型 Canvas 差量预览事务；分隔条复用单目标
`ControlPropertyCommand`，鼠标抬起只提交一次，Escape、系统取消、
窗口失焦或捕获丢失会恢复预览前状态且不破坏 redo。Canvas 保留最后结果并发布完成事件，Designer 顶部状态区
显示提交、取消或失败原因。Add/Delete/Undo/Redo 另行发布带历史标签的离散命令完成事件；空删除、越界添加、
空历史和真实恢复失败均可区分，工具栏与键盘入口不再无条件报告成功。分隔条预览若无法写入属性元数据会
中止并回滚，不再退回裸 setter。
六类结构差量同样先核对 stable ID、名称、控件类型和 expected 集合状态，再原位恢复目标集合；Undo/Redo
不会替换控件实例，外部修改导致冲突时历史项保留并可在状态修复后重试。命令内存只随目标列/节点/轨道/分段
或 ComboBox/Menu 项增长，不再随文档中的无关控件、样式、Binding 或资源增长。

设计器文档生命周期也使用同一套结果与恢复语义。`CommandManager` 为每次提交分配不可复用的文档状态 ID，
保存点不依赖撤销栈深度，因此“保存后撤销再创建新分支”仍会正确显示 Dirty；撤销/重做到保存点会恢复干净
状态。新建和打开在成功应用完整目标文档后才清空历史并建立新保存点，解析或应用失败会恢复原文档、完整选择
和原 Dirty 状态。保存先在目标目录写入并刷盘临时文件，再原子替换现有设计文件；写入或替换失败不会破坏旧文件，
也不会误清除 Dirty。窗口标题以 `*` 标记未保存修改；新建、打开和关闭前会先结束属性编辑、回滚未提交的画布
预览，并提供保存/放弃/取消提示，当前文件名只在打开或保存真正成功后更新。

Dirty 文档还会在最后一次已提交命令后 750ms 写入自动恢复快照。快照位于
`%LOCALAPPDATA%\CUI\Designer\Recovery`，使用与正式保存相同的临时文件刷盘和原子替换，但不会移动正式
保存点。每个 Designer 进程按 PID 与进程创建时间使用独立会话文件；启动时会跳过仍在运行的其他实例，只提示
恢复真正遗留的最新快照。恢复后的文档没有伪造的 Undo 历史，但保持 Dirty，用户仍需显式保存；成功保存、
新建、打开或干净关闭只清理当前会话快照。损坏、截断、超大或版本不支持的恢复文件会改名隔离，不会覆盖当前
文档，也不会阻止检查其他恢复项。

撤销历史现在同时受 Undo 侧 128 条数量上限和默认 64MiB 估算内存预算约束，预算覆盖 Undo 与 Redo 两侧；超限时优先
淘汰最远历史，但始终保留至少一个最近可操作命令，即使单条大型快照本身超过预算。普通控件属性（包括多选、
Reset、Name、分组滑块和 SplitterDistance 连续预览）使用逐目标属性差量；键盘微调、鼠标移动、缩放、Reparent
与 Stack/Wrap 重排使用 placement/tree 差量，保存 Location/Margin/显式尺寸/对齐/Anchor、Grid/Dock 字段、父级定位器和同级索引。
这些高频编辑不再保存两份完整文档，也不会在普通 Undo/Redo 时重建控件实例。Legacy 属性差量会恢复为序列化
等价的基值，Metadata 属性精确保留 Local 与跟踪状态；八类模态结构编辑均使用局部差量，窗体、事件和 Binding 仍由完整文档事务兜底，
无法标识父级的自定义容器手势也会安全回退完整事务。

相同选择上的同一属性修改以及连续键盘微调，在每次提交间隔不超过 1 秒时会把旧 before 与最新 after 合并为
一条命令。合并不会跨越精确保存点、已有 Redo 分支、选择变化、不同操作标签或不连续的当前状态；目标在其他
快照命令重建控件后会按名称和类型重新解析。Splitter 等鼠标手势显式关闭时间窗合并，每次手势保持独立命令。
Canvas 提供历史预算、当前估算用量和 Undo/Redo 数量查询，也允许
宿主按文档规模调整预算。

属性面板顶部提供“属性 / 事件”双视图（`Ctrl+1` / `Ctrl+2`）和即时筛选框。两个视图不再混排：
属性页只承载属性、Binding 与结构化编辑入口，事件页只承载命名事件和文档级处理函数管理；两页分别保存
筛选词、分类折叠状态和滚动位置，编辑或切换选择不会把用户带回顶部或重新展开全部组。空格分隔的多个
关键词采用 AND 匹配，可搜索属性名、分类、当前值、
编辑器类型、Choice 选项、中英文值来源和诊断详情。属性行会显示当前有效来源 `[默认]`、`[主题]`、
`[样式]`、`[绑定]` 或 `[本地]`，并附带绑定配置、警告/错误和诊断不一致标记；可访问说明与行下摘要会在
Binding 校验或样式表变化后刷新，便于判断 Binding/Style 优先级为何遮蔽修改。
事件行不再是 Boolean 开关，而是可编辑的 C++
成员函数名：空值解绑，旧文档中的 `1/true` 会显示为约定默认名，F4/下拉按钮会列出当前文档内参数
签名兼容的处理函数。事件按操作、值变化、鼠标、键盘、焦点、拖放、布局、生命周期、数据、导航、媒体和诊断
分类展示，每种控件由事件目录声明一个默认事件。双击事件行或直接双击画布中的控件都会复用现有函数，或通过
正常可撤销事务写入约定默认名；双击窗体客户区对应首次显示事件 `OnShown`。完成过一次代码导出后，双击还会
安全重生成 `.g.*`、向用户 `.cpp` 追加缺失桩并打开该源文件。源码定位会跳过注释、字符串、原始字符串和
仅有声明的伪匹配；默认探测 VS Code / Visual Studio 并精确跳到定义行，启动参数不经过 shell。
也可用 `CUI_CODE_EDITOR` 指定编辑器，并用包含 `{file}`、`{line}`、`{column}` 的
`CUI_CODE_EDITOR_ARGS` 定义参数模板；编辑器启动失败时才回退系统文件关联。状态栏会明确显示是否完成
精确定位或发生回退。导出成功后，
设计文档用独立于 `Form.Name` 的 `x:Class` 保存 C++ 类身份，并用 `d:CodeBehind` 保存相对设计文件、无扩展名的
代码基路径；保存并重开文档后仍能继续这一体验。首次未导出时只提示建立导出目标，不会从窗体名猜路径覆盖
文件。`x:Class` 支持 `Acme.Views.MainWindow` 或 `Acme::Views::MainWindow`，统一规范为 C++ `::`；生成头会在
对应命名空间声明叶类，文件基名不必等于限定类名。函数名和类名各段在写入前校验为合法 C++ 标识符，同名
不同签名会被拒绝。

代码导出采用生成基类与用户类分离的持久化契约。`FormName.g.h/.g.cpp` 完全由 Designer 管理并可重复
覆盖，包含强类型 `protected` 控件引用、虚事件钩子和由 `EventConnection` 持有的
`Subscribe(std::bind_front(...))`；`FormName.h/.cpp` 仅首次创建，后续新增事件只向用户源文件追加一个
空处理函数体。`FormName.handlers.g.inc` 保留已出现过的声明，因此删除事件不会使已有用户函数定义失去
声明。生成器用轻量 C++ token 扫描确认用户 `.cpp` 中真实的 `Class::Handler(...) {}` 定义，会跳过注释、
普通/原始字符串并区分 `Handle` 与 `HandleSave`，避免伪文本阻止缺失桩生成。用户构造函数体是初始化代码的
稳定扩展点；导出前还会确认已有用户头中的类确实继承当前生成基类、用户源含有同类构造函数，防止手改
`x:Class` 后混用两代类身份。遇到没有 Designer 标记或类身份不匹配的同名文件时导出会拒绝覆盖。一次导出的全部目标会先写入并 flush 各自
目录内的临时文件，随后按批次提交；任一目标被锁定或替换失败时，已提交的既有文件从备份逆序恢复，原先不
存在的目标被移除，避免 `.g.h/.g.cpp/.handlers.g.inc/.cpp` 落在不同代际。
显式导出是创建或改变 code-behind 关联的唯一入口；若设计文档尚未保存，先记录类身份，首次保存时再根据实际
设计文件目录计算可移植相对路径。该关联进入正常文档事务和 Undo/Redo，绝对路径不会写入 XML/XAML。

设计器窗口和构建工具现在共同调用无 HWND 依赖的
`DesignCodeGenerationService`，因此交互导出、CI 与本机构建不会形成两套生成规则。
`CuiCodeGenCore/CuiCodeGenCore.vcxproj` 是 `CodeGenerator.cpp` 与该服务实现的唯一编译所有者，输出
`CuiCodeGenCore.lib`；Designer、`CuiCodeGen.exe` 和 `CUICoreTests` 只链接这一个库，不再各自编译同一实现。
`CuiCodeGen.exe` 可直接消费 `.xml` / `.xaml`；默认从设计文件读取 `x:Class` 和
`d:CodeBehind`，也可显式覆盖类名和无扩展名输出基路径：

```powershell
.\CuiCodeGen\x64\Debug\CuiCodeGen.exe generate `
    .\CuiStaticGeneratedSample\NamespacedWindow.cui.xaml
.\CuiCodeGen\x64\Debug\CuiCodeGen.exe generate .\MainWindow.cui.xaml `
    --output .\Generated\MainWindow --class Acme.Views.MainWindow --quiet
```

成功、生成失败和命令行用法错误的退出码分别为 `0`、`1`、`2`。命令仍使用同一五文件事务提交和用户代码
保护规则。`build/CuiCodeGen.targets` 提供编译前增量集成；项目引用 `CuiCodeGen.vcxproj`、设置
`CuiCodeGenExe`，声明一个或多个 `CuiDesign` 后，在 `Microsoft.Cpp.targets` 之后导入即可：

```xml
<ItemGroup>
  <CuiDesign Include="MainWindow.cui.xaml">
    <OutputBase>$(ProjectDir)Generated\MainWindow</OutputBase>
    <!-- ClassName 通常省略，以 x:Class 为准 -->
  </CuiDesign>
</ItemGroup>
<Import Project="..\build\CuiCodeGen.targets" />
```

目标用 `$(IntDir)\CuiCodeGen` 下的 stamp 记录设计文件的新鲜度，并在 stamp 有效前检查五个代码文件都存在；
输入未变化时不会启动生成器。即使输入时间戳变化，只要规范生成结果逐字节相同，批次提交也会保留代码文件
及其时间戳，从而避免无意义的 C++ 重编译。`CuiStaticGeneratedSample` 已使用这条构建链，而非依赖手工预生成步骤。

后续运行时表示采用“静态生成优先、动态加载复用同一模型”的混合路线，而不是维护两套属性和容器规则。
`DesignDocumentGraph` 已成为文档 ID、父级解析和子级顺序的唯一拓扑层；
`DesignDocumentControlPool` 通过可注入工厂创建控件，在挂载前保持 `unique_ptr` 所有权，失败即自动回滚，
成功后再逐项转移给运行时控件树。公开的 `RuntimeDocumentLoader` 现在可从 `DesignDocument`、规范 XML、
XAML 风格字符串或文件事务性构建完整控件树；失败不会替换调用方已有的 `RuntimeDocument`。运行时文档在
`ReleaseRootControls()` / `TransferRootControlsTo()` 前持有所有根控件，可按稳定 ID 或设计期名称索引控件，并能连接 DataContext、
恢复被 Binding 暂存的 Local 后备值，以及通过应用提供的名称解析器持有控件/窗体事件的 RAII 连接。
`ApplyFormProperties(...)` 会把窗体模型应用到宿主拥有的 `Form`，并记住该目标；`BindFormEvents(...)`
同样保留窗体和名称解析器，使后续原位、重组及完整替换都能自动刷新显示属性并重建窗体事件连接。代码生成输入也已经改为从同一个
`RuntimeDocument` 投影；文档样式在静态代码中挂到每棵根控件树，不再错误调用不存在的
`Form::SetStyleSheet`。

`DesignDocumentEventIndex` 会把窗体和所有控件的事件引用解析为“函数名 + 精确 C++ Event 函数类型”，统一拒绝
未知事件、非法标识符和跨签名重名。属性栏继续提供每个事件的可编辑函数名与同签名候选，并新增文档级
“重命名处理函数”入口：一次更新所有同名引用、支持合并到同签名函数，并通过完整文档事务参与 Undo/Redo。
XML、XAML、动态加载与静态代码生成都会消费同一契约；静态输出仍使用
`Subscribe(std::bind_front(&GeneratedClass::Handler, this))`。重命名不会猜测性改写用户 C++ 函数体，下一次
生成会保留旧用户代码并为新名称创建缺失的安全桩。

动态宿主不再需要为每次加载手写处理函数名 `if/switch`。`RuntimeEventHandlerRegistry` 把函数名、Designer
事件描述、真实 CUI `Event` 成员和可调用对象注册为一条路由。事件目录现在直接由真实成员生成字段名、函数类型
和 C++ 参数类型文本，参数名仅作为生成代码的可读标签；注册时还会比较精确成员身份，因此不能把
`OnMouseMove` 冒充成同签名的 `OnMouseClick`。普通 `Event<>`、校验通知事件以及 Form 继承的 Control 事件使用
同一契约。注册表统一拒绝非法名称、同名异类型和重复路由。`ControlResolver()` / `FormResolver()` 捕获共享注册状态，宿主可在
后续热重载出现新函数名前追加注册，现有 RuntimeDocument 保存的 resolver 会立即看到新路由。静态生成路径
仍直接输出 `std::bind_front`，不会为此引入运行时字符串分派。

常规的“文件 + Form + 命名事件 + 保存热重载”宿主优先使用 `RuntimeDocumentSession`。它把文档、共享事件注册表
和无线程 watcher 收口到一个不可移动的 UI 线程会话中，但不会隐藏事务边界或创建后台线程：首次 `MountFile()`
只有在解析、材质化、Binding、控件/Form 事件、显示属性和根提交全部成功后才生效，后续仍由宿主定时调用
`Poll()` 并处理明确的 `Reloaded` / `Failed` 结果。`Form` 和处理函数捕获的业务对象必须比 session 活得更久。

```cpp
Form form; // 先声明：Form 必须比 session 活得更久
DesignerModel::RuntimeDocumentSession session{
    std::chrono::milliseconds{150}};
session.EventHandlers().RegisterControl(
    L"HandleSave", UIClass::UI_Base, L"OnMouseClick",
    &Control::OnMouseClick,
    std::bind_front(&MainWindow::HandleSave, this), &error);
session.EventHandlers().RegisterForm(
    L"HandleCommand", L"OnCommand", &Form::OnCommand,
    std::bind_front(&MainWindow::HandleCommand, this), &error);

DesignerModel::RuntimeDocumentSessionMountOptions mount;
mount.DataContext = viewModel;
if (!session.MountFile(L"MainForm.cui.xaml", form, mount, &error)) {
    // form 与 session.Document() 都保持挂载前状态，可补注册后重试
}

// 在同一个 UI 线程的定时器中调用。
const auto result = session.Poll();
if (result.State == DesignerModel::RuntimeDocumentWatchState::Failed)
    ShowReloadError(result.Error); // 原界面仍保持活动
```

下面的 `RuntimeDocumentLoader`、独立注册表和 watcher 是等价的低层组合入口，适合内存字符串、挂载前检查、
自定义根宿主或由应用自行管理多个文档的场景。

全属性应用、复合容器挂载、布局刷新与样式装配已经收口到中立的
`DesignDocumentMaterializer`。`DesignerCanvas` 与 `RuntimeDocumentLoader` 都消费它输出的脱离控件森林，
动态加载不再创建隐藏设计器，也不再依赖设计器字体或客户区生命周期。静态生成仍是默认发布方式，动态路径则
可用于工具、预览和受控宿主；后续属性扩展只需维护这一条材质化语义。

```cpp
DesignerModel::RuntimeDocument document;
DesignerModel::RuntimeDocumentLoadOptions options;
options.DataContext = viewModel;
DesignerModel::RuntimeEventHandlerRegistry handlers;
if (!handlers.RegisterControl(
        L"HandleSave", UIClass::UI_Base, L"OnMouseClick",
        &Control::OnMouseClick,
        std::bind_front(&MainWindow::HandleSave, this), &error)) {
    // 非法名称、签名冲突、重复路由或无效事件
}
options.ControlEventResolver = handlers.ControlResolver();
if (!handlers.RegisterForm(
		L"HandleCommand", L"OnCommand", &Form::OnCommand,
		std::bind_front(&MainWindow::HandleCommand, this), &error)) {
	// Form 事件也经过同一名称/签名规则
}
if (!DesignerModel::RuntimeDocumentLoader::LoadFileIntoForm(
		L"MainForm.cui.xml", form, document, options,
		handlers.FormResolver(), &error)) {
	// 解析、材质化、Binding、事件、显示属性或根提交失败；
	// form 与 document 都保持调用前状态
}
```

`Load*IntoForm(...)` 是动态窗口的推荐首次加载入口：候选文档完全就绪后，才把 Form 显示属性、Form 事件连接
和根森林作为一次事务提交。若宿主需要在挂载前检查或调整材质化结果，可先调用 `Load*()`，再调用
`document.AttachToForm(...)`，两步中的第二步仍可完整回滚。文档一旦通过 `AttachToForm`、
`TransferRootControlsTo` 或旧手动释放路径交出根，直接 `Load*()` 会无副作用拒绝；后续更新必须使用
`Reload*()`，让保留的宿主适配器参与提交与恢复。

`XamlDocumentParser` 是同一 `DesignDocument` 之上的可读前端，不是另一套控件运行时。它支持 `Form`/`Window`
根、嵌套控件、`x:Name`、可选 `DesignId`、Grid 行列定义、TabPage、SplitContainer 两个面板、附加布局属性、
直接文本内容、属性元数据枚举值，以及 `Width/Height="Auto"` 和浮点固定长度。`{Binding ...}` 会转为现有
通用 Binding 配置；未显式声明的点分源路径会补入 Unknown 类型的 DataContext Schema。事件属性可使用
`Click="HandleSave"` 或 `Click="Auto"`，最终仍由静态生成的 `std::bind_front` 或动态宿主的名称解析器连接。
资源/样式支持强类型资源、`Setter`、Class/状态选择器和 WPF 风格的 `x:Key` +
`Style="{StaticResource ...}"`。属性名称与值最终都由运行时属性元数据校验，因此新增通用元数据属性无需再给
XAML 编写专用 setter。

```cpp
const std::string_view xaml = R"(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="MainForm" Text="CUI XAML" Width="480" Height="240">
  <Form.Resources>
    <Color x:Key="Accent">#FF0078D4</Color>
    <Style x:Key="PrimaryButton" TargetType="Button" Class="primary">
      <Setter Property="BackColor" Value="{StaticResource Accent}" />
      <Setter Property="Round" Value="8" />
    </Style>
  </Form.Resources>
  <StackPanel x:Name="root" Width="Auto" Height="Auto"
              Orientation="Vertical" Spacing="8">
    <Button x:Name="saveButton" Classes="primary"
            Style="{StaticResource PrimaryButton}"
            Text="{Binding User.Caption, Mode=OneWay}"
            Click="HandleSave" />
  </StackPanel>
</Form>)";

if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
        std::string(xaml), document, options, &error)) {
    // 解析或材质化失败；document 仍保留之前成功加载的树
}
```

外部控件使用带前缀的元素，并声明可移植的设计/生成信息。`d:BaseType` 是 CUI 内置基类，负责设计器预览、
属性/事件元数据、布局和无窗口代码生成；`d:CppType`、`d:Header` 与 `d:Constructor` 决定静态 C++ 输出。
构造约定可选 `Default`、`Bounds(x, y, width, height)` 或
`TextBounds(text, x, y, width, height)`。XAML 与 v5 XML 都会完整往返这些字段：

```xml
<Form xmlns="urn:cui" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer" xmlns:controls="urn:acme:controls">
  <controls:StatusBadge x:Name="statusBadge"
      d:CppType="Acme.Controls.StatusBadge"
      d:Header="Controls/StatusBadge.h"
      d:BaseType="Button" d:Constructor="Bounds"
      Text="Ready" Width="120" Height="30" />
</Form>
```

动态宿主必须显式注册真实工厂；工厂返回的控件应继承并保持所声明 `d:BaseType` 的 `Type()`。未注册时生产加载
会事务性失败，不会把外部控件悄悄替换成普通控件。设计器和 `CuiCodeGen` 使用显式基类代理，因此无需加载业务
DLL 也能预览通用属性并生成强类型成员、头文件引用和构造代码。自定义专有属性仍应通过真实控件的属性元数据
暴露：动态加载在解析 XAML 时就用注册工厂校验/Coerce；规范写回会把工具端无法探测的专有值和 Binding 保存在
强类型 `d:DesignProps` / `d:DesignBindings` 中，后续 CLI 代理会保留并生成延迟设置。没有元数据的任意 XAML
属性不会被猜测。Reload 省略注册表时会继承当前文档的注册表，因此专有属性仍可原位更新。

Designer 的 ToolBox 可通过 UTF-8 控件清单注册同一份可移植描述符，而不加载业务 DLL。清单会严格校验
schema、可实例化的内置基类、XAML 前缀/类型冲突、规范 C++ 类型和安全相对 include；任一项失败时整份清单
不生效。`CuiStaticGeneratedSample/CuiDesigner.controls.xml` 是可直接使用的示例：

```xml
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="StatusBadge" displayName="状态徽标" category="示例控件"
    baseType="Button" xamlPrefix="sample" xamlName="StatusBadge"
    xamlNamespace="urn:cui:samples" cppType="Acme.Controls.StatusBadge"
	header="Controls/StatusBadge.h" constructor="Bounds"
	width="150" height="30" container="false">
    <property name="Severity" displayName="严重程度" category="外观"
      kind="Int64" default="1" editor="Choice"
      minimum="0" maximum="2" bindable="true" twoWay="false">
      <choice displayName="正常" value="1" />
      <choice displayName="警告" value="2" />
    </property>
    <event name="OnSeverityInvoked" displayName="严重程度已触发"
      field="OnSeverityInvoked" category="Action"
      signature="SenderInt" order="5" default="true" />
  </control>
</cuiControlCatalog>
```

用 `Designer.exe --controls <清单>` 启动即可把条目加入 ToolBox；CI 可用
`Designer.exe --validate-controls <清单>` 只校验并返回 `0/2`。`property` schema 会生成强类型属性行、
Choice/范围校验、Reset、Undo/Redo 和延迟 Binding；真实自定义控件仍应注册同名运行时属性元数据。
`twoWay=true` 是对 getter 和变更通知能力的显式承诺；默认只开放 OneWay/OneTime。

`event` schema 会把自定义控件事件加入属性栏的事件页，并支持默认事件双击、命名处理函数、Undo/Redo、
XAML/XML 往返和静态 `std::bind_front` 生成。`signature` 不是任意 C++ 文本，只能使用固定安全预设：
`None`、`Sender`、`SenderBool`、`SenderInt`、`SenderFloat`、`SenderDouble`、`SenderString`、
`SenderIntInt`、`SenderIntBool`、`SenderDoubleDouble`、`SenderStringString`；其中 sender 始终是
`Control*`。契约会持久化到 `d:CustomEvents`，因此无窗口生成不依赖本机已安装清单。动态宿主还需通过
`RuntimeEventHandlerRegistry::RegisterCustomControl(...)` 注册真实 Event 成员；注册时会核对
`Event::function_type`。已绑定事件的 `name`、`field` 或 `signature` 若与当前清单不一致，Designer 会拒绝
加载并保留当前画布，避免静默生成错误连接。完整动态示例见 `CuiRuntimeSample/main.cpp`。

无增强预览时画布使用 `baseType` 代理，保存后的 XAML/代码生成仍引用真实类型。同进程宿主可用
`DesignerControlCatalog::AttachPreviewFactory(...)`；独立 DLL 则用显式受信任的
`--preview-plugin <dll>` 加载，并可用
`--validate-preview-plugin <dll> <xaml-namespace> <xaml-name>` 做无窗口 ABI/session/render 门禁。
插件不返回 `Control*`：宿主依然持有基类代理，插件只返回有上限的值类型绘制原语，且设计文件/清单不能指定 DLL 路径。
完整边界见 [纯 C ABI 约定](CuiDesigner/CUSTOM_CONTROL_PLUGIN_ABI.md)。

```cpp
auto controls = std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
controls->Register(L"urn:acme:controls", L"StatusBadge",
    [](const DesignerModel::DesignNode&) {
        return std::make_unique<Acme::Controls::StatusBadge>();
    }, &error);

DesignerModel::RuntimeDocumentLoadOptions options;
options.CustomControls = controls;
DesignerModel::RuntimeDocumentLoader::LoadXaml(xaml, document, options, &error);
```

动态宿主可以使用 `Reload(...)` / `ReloadXaml(...)` / `ReloadFile(...)` 安全重载。通用标量/元数据属性、
Binding 与 DataContext Schema、文档样式、控件事件和窗体显示属性变化会返回
`RuntimeDocumentReloadMode::InPlace`：先完整材质化候选树做校验，再按稳定 `DesignId` 复用全部控件实例，
事务性提交属性值源、绑定、样式和事件连接；任一步失败都会恢复旧状态。省略的 DataContext 和事件解析器继承
现有运行时附件。拓扑或容器 `Extra` 改变时，加载器会先构建候选树，再把内容与内部层级完全未变的最大
`DesignId` 子树移植到新位置，并返回 `RuntimeDocumentReloadMode::Recomposed`；因此增删、同父重排及父容器
重建不必让无关控件丢失实例状态。重组期间的 Binding、事件或样式失败会把原拥有关系和运行时附件一起回滚。
没有可复用子树、字体所有权、未知属性袋，或被活动 Binding 占用的持久化属性会保守地返回 `Replaced`。
使用 `TransferRootControlsTo(form)` 时，文档会保留 Form 根宿主适配器：重载先按原槽位原子分离旧根，候选
通过后在相同锚点提交；材质化、Binding、事件、样式或宿主提交失败都会把旧根精确放回，宿主自有顶层控件
不受影响。自定义宿主可实现 `RuntimeDocumentRootHost` 的 Detach/Replacement/Rollback 契约。旧的
`ReleaseRootControls()` 仍用于完全手动的所有权管理；因为没有适配器，这条路径在需要重组或替换时会明确拒绝，
不会猜测宿主结构。

```cpp
DesignerModel::RuntimeDocumentReloadMode mode;
if (!DesignerModel::RuntimeDocumentLoader::ReloadXaml(
        updatedXaml, document, {}, &mode, &error)) {
    // 失败时原控件实例、事件连接和 DataContext 保持不变
}

// 按稳定 ID 的 O(1) 类型化引用；Reload 替换实例后再次 Get() 会解析新实例。
auto saveButton = document.ReferenceByDesignId<Button>(42);
if (auto* button = saveButton.Get()) button->Text = L"Save";
```

`FindControlByDesignId` / `FindControlByName` 由文档内索引提供 O(1) 查询。`RuntimeControlRef<T>` 不拥有控件，
每次访问都按稳定 ID 重新解析，所以能跨 `InPlace`、`Recomposed` 和 `Replaced` 跟随当前实例；引用期间应保持
同一个 `RuntimeDocument` 对象地址，不要移动或销毁该文档对象。

动态附件均为非拥有引用：调用过 `ApplyFormProperties(form)`、`BindFormEvents(form, ...)` 或
`TransferRootControlsTo(form)` 后，`Form` 必须比 `RuntimeDocument` 活得更久（通常先声明 `Form`，再声明
`RuntimeDocument`）。Reload 会把候选窗体显示状态、窗体事件连接和根森林作为一次事务提交；解析器或宿主拒绝
候选时，旧标题/尺寸/颜色/可见性/字体语义、旧事件连接和旧根槽位都会保留。

低层宿主需要自行组合文件监视时，使用无后台线程的 `RuntimeDocumentFileWatcher`。宿主从 UI 定时器调用 `Poll()`；监视器
以文件 ID、写入时间和大小识别直接写入及原子替换，等签名稳定超过防抖窗口后才调用格式感知的 `ReloadFile`。
失败签名不会在每个 tick 重复执行；文件再次变化会自动恢复，也可显式 `RequestRetry()`：

```cpp
DesignerModel::RuntimeDocumentFileWatcher watcher{std::chrono::milliseconds{150}};
if (!watcher.Start(L"MainWindow.cui.xaml", &error)) return;

// 在创建/操作控件的同一 UI 线程定时调用。
const auto result = watcher.Poll(document);
if (result.State == DesignerModel::RuntimeDocumentWatchState::Failed) {
    ShowReloadError(result.Error); // 原文档仍保持活动
}
```

监视器不创建线程、不投递窗口消息，也不拥有 `RuntimeDocument`；宿主仍控制轮询频率、线程归属、错误呈现和
是否接受 `Recomposed` / `Replaced`。因此事件解析器和所有控件变更不会意外跨线程。

这是一种面向 CUI 模型的 XAML 方言，而非完整 WPF XAML 对象系统；不支持的元素、属性或标记扩展会在提交
前给出错误。`XamlDocumentSerializer` 提供与解析器对称的规范写入；普通属性保持可读写法，无法直接表达的
结构化兼容数据放在 `d:DesignProps` / `d:DesignExtra` 中，因此 XAML 往返不会丢失 Designer 模型字段。
设计器现在可直接打开、保存 `.cui.xaml` / `.xaml`，普通保存保持当前源格式；`.cui.xml` / `.xml` 使用
版本 5 XML。v5 新增可选 code-behind 类身份与相对基路径；版本 1–4 仍可读取并在下次保存时升级。两种格式都
采用原子替换、同一材质化/代码生成链路，并可用工具栏“重新加载”安全刷新；有 Dirty
修改时先进入保存/放弃/取消流程，加载失败则保留当前画布。`LoadXamlFile(...)` 提供等价运行时文件入口。

`CuiRuntime/CuiRuntime.vcxproj` 把这条动态路径作为独立静态库提供，不需要链接 Designer 可执行文件。
`CuiRuntimeSample` 是可直接构建运行的最小宿主，覆盖 XAML、XML 往返、注册式自定义控件、嵌套 Grid/Tab/Split、稳定索引、
DataContext、属性/Binding/样式/事件原位事务、替换边界、失败回滚、根控件所有权转移、防抖文件监视和
`RuntimeDocumentSession` 的 UI 线程生命周期。应用只需包含
`CuiRuntime/include/CuiRuntime.h`；Designer 本身也通过项目引用
链接同一份 `CuiRuntime.lib`，不再重复编译另一套 Runtime 实现：

```powershell
msbuild CuiRuntimeSample\CuiRuntimeSample.vcxproj /m /p:Configuration=Debug /p:Platform=x64
.\CuiRuntimeSample\x64\Debug\CuiRuntimeSample.exe
```

`CuiStaticGeneratedSample` 则把 Designer 的命名空间限定 `x:Class` 和外部自定义控件输出作为真实 `.g.h/.g.cpp` 与用户
`.h/.cpp` 加入解决方案编译并运行。生成基类为每个 `x:Name` 提供 const/non-const 强类型访问器，例如
`GetNamespaceButton()`，并在 `ControlIds` 中公开同一控件的稳定 `DesignId`；业务代码无需遍历
`Form::Controls` 或执行 `dynamic_cast`。名称规范化后的 C++ 成员会进行全局去重，指针默认初始化为空。
`CUICoreTests` 会把样例的五个代码文件与当前生成器输出按规范换行逐一比较，防止“样例仍能编译、生成器却已漂移”。

```powershell
msbuild CuiStaticGeneratedSample\CuiStaticGeneratedSample.vcxproj /m /p:Configuration=Debug /p:Platform=x64
.\CuiStaticGeneratedSample\x64\Debug\CuiStaticGeneratedSample.exe
```

默认材质化工厂创建生产控件（包括真实 `WebBrowser`）；只有 `DesignerCanvas` 显式注入轻量预览工厂，
因此设计器仍不会初始化 WebView，而动态宿主不会误拿到 `FakeWebBrowser`。

被设计窗体不再维护独立的 `DesignedFormSnapshot`、文本属性分支和布尔属性分支；持久化使用的
`DesignFormModel` 同时也是属性面板、撤销/重做、XML 与代码生成输入的唯一状态模型。窗体目录为
21 个属性声明类型、分类、顺序、范围和默认值，尺寸、标题高度、字体大小等统一 Coerce。属性面板
为窗体属性和所有具有默认值的控件元数据属性显示逐项“↺”按钮；恢复操作进入撤销栈，并清除
控件 Local 层以显露 Style/Binding/Theme/默认值。默认字体族与显式字号可以正确保存并重新加载。

`StackPanel` 的 Orientation、Spacing 与内容对齐，`WrapPanel` 的 Orientation、ItemWidth、
ItemHeight，`DockPanel` 的 LastChildFill，以及 `SplitContainer` 的方向、分隔条尺寸/位置、面板
最小尺寸、固定状态和分隔条外观，均已完全迁移到这条通用路径。拖动分隔条得到的新位置也会回写
同一份元数据。新文档只写 `props.metadata`，属性面板和生成的 C++ 不再维护容器专用分支；旧文档
中的同名 `Extra` 字段仍可读取，并在没有新 metadata 值时自动升级。若两种格式同时存在，以强类型
metadata 为准。

`Slider` 与 `NumericUpDown` 的范围、步长、吸附、输入行为和控件专用外观也使用同一契约。
`Min` 变化会重新 Coerce `Max` 与 `Value`，交互更新 `Value` 时保留现有 Binding，范围导致的值变化
仍会发布统一通知。Designer 按元数据顺序恢复和生成这些依赖属性，避免按属性名排序时先应用
`Value` 而得到不同结果；旧 Extra 继续只读升级。

`GroupBox` 的标题间距、圆角和颜色，以及 `Expander` 的标题几何、展开状态、动画时长和专用外观
也已完全迁移。负数或非有限几何值由运行时元数据统一 Coerce；Expander 的鼠标、键盘与 `Toggle()`
交互使用当前值更新，因此不会用 Local 值覆盖现有 TwoWay Binding。新文档和生成代码只使用
`props.metadata` / `TrySetPropertyValue(...)`，旧 `Extra` 字段仅在缺少同名 metadata 时升级。

`ScrollView` 的内容尺寸、滚动条可见性/粗细、滚轮步长、边框和滚动条颜色也已接入同一契约。
`ContentSize` 以强类型 Size 编辑和持久化，尺寸与粗细由元数据统一钳制为非负值。滚动偏移仍是可观察、
可 Binding 的瞬时运行状态，因此不会写入 `props.metadata` 或生成代码；旧文档中的配置字段会升级为
metadata，旧偏移只在加载时兼容读取。

`Panel` 的边框粗细、圆角与禁用遮罩也已成为所有容器共享的元数据属性。`ToolBar`、`StatusBar`、
`PagedGridView`、`Expander`、`ScrollView` 不再声明同名裸字段，而是共享 Panel 的唯一 backing；需要不同
圆角默认值的派生类型只覆盖自己的元数据默认值。因此通过基类引用、派生类型、Theme/Style/Binding 或
Designer 修改属性时，绘制与属性来源始终看到同一状态。

`ToolBar` 与 `StatusBar` 的专用布局、行为和外观也已完全迁移。原来遮蔽 `Control::Padding(Thickness)` 的
整数 `Padding` 已改名为语义明确的 `HorizontalPadding`；两者现在可同时编辑和生成，不再发生类型歧义。
ToolBar 的自动高度项会跟随 `ItemHeight` 更新，StatusBar 的 `TopMost`、分段间距/圆角、颜色和显示开关
均支持 Theme/Style/Binding。旧 XML 的 `padding`、`gap`、`itemHeight`、`topMost` 只读升级到 metadata，
StatusBar 的 parts 集合仍使用结构化专用持久化。

`Control::Children` 现在是兼容 vector 读取的拥有型 `ObservableCollection`。直接 insert/erase、Replace、
Move、Swap 或批处理都会先同步 Parent/ParentForm、继承样式、Form 交互引用、布局与可访问性，再通知公开
观察者；空指针、重复项、跨父级挂接和成环结构会回滚并拒绝。新代码可使用 `InsertOwned()`、
`DetachControlAt()`、`DeleteControlAt()` 与 `ClearControls()` 明确表达所有权；直接 erase/clear 只分离对象。

`TabControl` 的选中索引、标题位置、动画模式/时长、标题几何、滚动行为和全部专用颜色也已接入
同一套元数据。`TitleWidth`、`TitleHeight` 与标题滚动量现在使用浮点 DIP；鼠标、键盘、拖动和
`SelectPage()` 通过 current-value 更新保留活动 Binding。`TitleScrollOffset` 可观察、可 TwoWay Binding，
但属于 `Transient` 运行状态，不进入普通属性面板或生成代码。页面集合仍使用结构化持久化；旧 XML 的
`selectedIndex`、标题尺寸/位置和动画模式只在缺少同名 metadata 时升级。新增 `InsertPage`、
`DetachPageAt`、`RemovePage` 与 `ClearPages` 等所有权安全入口；插入和重排会让选中状态按页对象身份移动，
并同步 TwoWay `SelectedIndex`、过渡动画和原生子窗口。`Pages` 现直接投影可观察的 Children 集合。

`Menu` 顶层项与 `ContextMenu` 现在提供对称的插入、分离、删除和清空 API；`MenuItem::SubItems` 改为
兼容 vector 读取的 `ObservableCollection`。直接移动/交换或批量修改会发布结构通知，安全 API 使用
`unique_ptr` 明确转移所有权；菜单树变化时会关闭已失效的悬停/展开路径，避免旧索引命中错误项。

`ComboBox` 的 `SelectedIndex`、`ExpandCount`、动画时长、下拉几何和全部专用颜色也已完成迁移。
鼠标、键盘、`SelectItem()`、`SetExpanded()` 与 `ScrollBy()` 使用 current-value 更新，可在交互后继续
保持 TwoWay Binding；`Expand` 与 `ExpandScroll` 可观察、可绑定，但作为 `Transient` 运行状态不进入
设计文件或生成代码。Items 仍使用结构化持久化，并已改为兼容 `std::vector` 的 `ObservableCollection`；
直接 insert/remove/move/swap 会发布精确变更并让选择和虚拟项 ID 跟随逻辑项，批量作用域合并为一次 Reset。
集合晚于 Binding/metadata 到达时仍会重新校正选择与滚动范围。旧 XML 的 `expandCount` / `selectedIndex` 只在缺少同名 metadata 时升级；生成代码
通过合法的 `std::vector<std::wstring>` 一次性设置 Items，不再输出控件专用裸字段。

`ListView` / `ListBox` 的视图、选择模式、表头/复选框、尺寸、滚轮步长和全部专用颜色现在也使用
同一元数据契约。`SelectedIndex`、焦点/悬停索引与 `ScrollYOffset` 是可观察、可 TwoWay Binding 的
`Transient` 交互状态；单选、Ctrl 多选、范围选择和滚动使用 current-value 更新，不会覆盖活动 Binding。
Columns/Items 继续结构化持久化，同时公开可观察集合；直接结构修改会同步选择、焦点、滚动、稳定 UIA ID
与结构通知。`SetItems()` 可一次恢复多选标志；生成代码先应用配置 metadata，再设置
集合。旧 XML 的 List 标量只在缺少同名 metadata 时升级。`FullRowSelect` 和
`HideSelectionWhenLostFocus` 也已落实到实际绘制语义，ListBox 的隐藏表头通过派生元数据保持 false 默认值。
大量增删可使用可嵌套的 `BeginUpdate()` / `EndUpdate()` 或 `DeferUpdates()`：内部稳定 ID、选择和行列位置
逐次增量同步，Items/Columns 的公开观察者在最外层结束时各只收到一次 Reset，滚动校正、UIA 通知和重绘
也只收口一次。尾部追加只触碰新增身份和选择项；可用 `LastAccessibilityIndexUpdateWork()` 与
`LastSelectionUpdateWork()` 做确定性复杂度回归。若直接修改公开的 `ListViewItem::Selected` 字段，需调用
`Items.NotifyReset()` 让选择缓存重新以 Items 为真值校正。

`GridView` 的表头/行高（`0` 表示 Auto）、单元格几何、滚动条尺寸、行为开关和全部专用颜色也已接入
属性元数据。选择、悬停、排序与横纵滚动是可观察、可 TwoWay Binding 的 `Transient` 状态，鼠标、键盘
和公开选择 API 通过 current-value 更新，不覆盖活动 Binding。`FullRowSelect` 默认开启并参与实际绘制。
Rows/Columns 也是可观察集合；直接增删、移动、交换或排序会按稳定 ID 保持所选行列，并让每行 Cell 与
逻辑列一起移动。大量列/行更新可用可嵌套的 `DeferUpdates()` 合并集合通知、滚动校正与重绘；文本编辑提供 `BeginEdit()`、
`SetEditingText()`、`CommitEdit()` 与 `CancelEdit()`，且可在未挂接 Form 时安全使用。Designer 先生成
metadata，再以批量作用域恢复列，并完整保留 ButtonText 与 ComboBoxItems。

`PagedGridView` 的页大小、分页条几何、行为开关和专用颜色也已迁移到属性元数据；`PageIndex` 是
可观察、可 TwoWay Binding 的 `Transient` 交互状态，分页按钮与 PageUp/PageDown 使用 current-value
更新，不会覆盖活动 Binding。`SetRows()` / `SetColumns()` 提供原子集合替换，可嵌套的
`BeginUpdate()` / `EndUpdate()` 与 `DeferUpdates()` 会把多次源数据修改合并为一次当前页刷新。Rows 与
Columns 公开可观察集合；直接增删、移动、交换或批量 Reset 列时，会按稳定列 ID 同步所有页（包括离屏页）
的 Cell，并让公开通知观察到已完成对齐的数据。

`PropertyGridView` 的布局、编辑行为和全部专用颜色现在共享同一元数据契约；选择、悬停和滚动偏移
保持为可绑定但不持久化的运行状态。`SetItems()` 原子关闭旧编辑器并替换结构集合，公开的
`SelectItem()`、`ClearSelection()`、`BeginEdit()`、`SetEditingText()`、`CommitEdit()` 和
`CancelEdit()` 可在无 Form 场景安全使用。Designer 只为 Items 保留结构化通道（包括 Options 与 Tag），
标量统一写入 `props.metadata`；旧 `extra` 标量仍可读取，并且不会覆盖同名新 metadata。Items 现为
可观察集合；直接插入、删除、移动、交换、排序或批量 Reset 会按稳定身份保持选择、活动编辑器、Binding、
类别折叠状态与滚动范围，删除正在编辑的项则安全结束会话。

`MediaPlayer` 的 `AutoPlay`、`Loop`、`Volume`、`PlaybackRate`、硬件解码/NV12 偏好和
`RenderMode` 也已接入统一属性元数据，支持 Theme、Style、Binding、Designer 属性面板与代码生成。
带 Min/Max 的浮点元数据会自动使用范围滑块；旧设计文件中的媒体标量会迁移到 `props.metadata`，
媒体路径仍单独保存，生成代码保证先应用配置再 `Load()`。运行时新增 `TryPlay()`、`TryPause()`、
`TryStop()`、`TrySeek()`、`TogglePlayback()`、`SeekBy()`、`SetProgress()` 和 `Close()`；
`OnStateChanged` 与携带 HRESULT 的 `OnMediaError` 可用于可靠地驱动 UI 和诊断失败。媒体会话与视频帧
异步回调在析构/关闭时同步解绑定，播放位置也使用原子状态跨解码线程发布。

`WebBrowser` 的公开类布局不再依赖 `CUI_ENABLE_WEBVIEW2`：WebView2、COM 和 DirectComposition
类型都隐藏在 PImpl 中，应用、Designer 与测试可使用同一 ABI。`InitialUrl`、`ZoomFactor`、默认上下文
菜单、状态栏和缩放控件开关已接入统一属性元数据，可参与 Theme、Style、Binding、Designer 持久化与
代码生成。运行时可使用 `TryInitialize()` 和分阶段 HRESULT 查询诊断初始化，使用 `TryNavigate()`、
`TrySetHtml()`、`TryReload()`、`TryStop()`、`TryGoBack()`、`TryGoForward()` 获得明确结果；初始化前的
URL/HTML 请求共用最后写入获胜的待处理槽。异步环境、控制器、事件及脚本回调都受生命周期令牌保护。

`NotifyIcon` 的托盘、提示、气泡和递归菜单现已全链路使用 Unicode；窄字符串兼容入口优先按
UTF-8 解码。显示/隐藏、提示与菜单修改均提供 `Try*` 和 HRESULT 诊断，右键菜单自动弹出，支持多个
图标按窗口/消息/ID 分发，并在 Explorer 重启后恢复。菜单只保存值语义数据，临时 HMENU 不再随对象
浅复制。`Taskbar` 则改为每实例 RAII 持有 `ITaskbarList3`，提供可诊断的进度值、Normal、Paused、
Error、Indeterminate 与 Clear 操作，不再存在共享 COM 指针的重复释放风险。

键盘焦点现在使用统一的 `IsTabStop` / `TabIndex` 契约；`Form` 支持循环 Tab/Shift+Tab、访问键、
默认按钮和取消按钮，`Button`、`LinkLabel`、`CheckBox`、`RadioBox` 与 `Switch` 共享可编程
`Invoke()` 语义。可访问名称、说明、帮助、AutomationId、角色、快捷键和焦点外观均为属性元数据，
可进入 Binding、Style、Designer 和代码生成。Form 通过 `WM_GETOBJECT` 暴露生命周期安全的
原生 UI Automation Fragment 树，并为核心控件提供 Invoke、Toggle、Value、RangeValue、
ExpandCollapse、SelectionItem 和 Selection Pattern；兼容的 `IAccessible` 客户区对象及 WinEvent
仍然保留。密码内容不会作为名称或值公开，窗口销毁后已持有的 Provider 会安全失效。
ListView/ListBox 项、ComboBox 项、TreeNode 以及 GridView 列头、行和单元格也作为稳定的虚拟
Fragment 暴露，支持 Selection、Toggle、ExpandCollapse、Grid/Table、Value、Invoke、
VirtualizedItem 与 ScrollItem 等对应 Pattern；逻辑项删除后，已持有的虚拟 Provider 会安全失效。
ListView/ListBox、ComboBox、TreeView 与 GridView 容器同时公开原生 Scroll Pattern，并以当前
视口和可滚动范围报告百分比；不支持滚动的轴按 UIA 约定报告 NoScroll。ListView Details 模式还会
公开稳定的列头、行和单元格层级，以及可按行列寻址的 Grid/Table 与 TableItem 表头关系。
原生 Provider 的首尾、兄弟导航和命中测试现在使用按索引/按 ID 快路径，不再为一次导航复制完整子集合
或递归扫描整棵虚拟树。内置虚拟控件会在结构变化时重建稳定索引；ListView Details 与 GridView 的
单元格 ID 均按访问懒创建，行列删除时仅清理已经物化且失效的身份，因此大数据表格不会预先分配
“行数 × 列数”的 UIA 反向索引。两者可通过 `MaterializedAccessibilityCellCount()` 检查当前物化规模。
ListView 的绘制和图标模式命中测试同时使用 `[start, end)` 可见索引范围；`GetVisibleItemRange()` 可供
延迟图像加载等调用复用，因此逐帧绘制成本只随可见项数增长，而不再扫描完整 Items。
这些控件的虚拟集合现由 `ObservableCollection` 驱动，直接结构修改不再等到下一次 Provider 查询才修正身份。
TreeNode 提供 `AddChild`、`DetachChildAt`、`RemoveChild` 与 `ClearChildren` 来明确表达嵌套节点所有权。

`Form` 会自动响应 Windows 高对比度、客户端动画、文字缩放和键盘焦点提示设置：公共表面、前景色与
焦点色采用系统高对比度色，常用控件动画在系统关闭动画时立即完成，继承或显式设置的字体会按文字比例
缩放。也可用 `Application::QuerySystemVisualPreferences()` 查询快照，并通过
`Form::ApplySystemVisualPreferences(...)` 注入设置以便测试。

`ObservableObject::SetValue` 会自动记录源属性名称、稳定值类型和默认的读写/通知能力。需要只读或静默属性时可显式声明；运行时 Binding 会据这些元数据提前拒绝不兼容模式：

```cpp
auto viewModel = std::make_shared<ObservableObject>();
viewModel->DefineProperty(
    L"Status", std::wstring(L"Ready"),
    true,   // CanRead
    false,  // CanWrite
    true);  // CanObserve
```

`ObservableObject` 也提供字段级和对象级验证状态。派生 ViewModel 可通过受保护的
`SetValidationIssues` / `SetValidationError` 发布信息、警告和错误；Binding 会监听整条
点分路径，并由目标控件的 `DataBindings` 汇总：

```cpp
class ViewModel final : public ObservableObject
{
public:
    void SetName(std::wstring value)
    {
        SetValue(L"Name", value);
        SetValidationError(L"Name",
            value.empty() ? L"Name is required." : L"",
            L"required");
    }
};

auto results = textBox->DataBindings.GetValidationResults();
bool hasErrors = textBox->DataBindings.HasValidationErrors();
```

控件会把这些结果统一呈现为按最高严重级别着色的主题边框，并在悬停时显示最多三条摘要；
可通过 `ShowValidationBorder`、`ShowValidationToolTip`、`ValidationBorderThickness`、
`ValidationCornerRadius` 和 `ValidationToolTipMaxWidth` 调整。`FormThemeFrame` 提供
Info/Warning/Error 及提示浮层配色。`AccessibleDescription` 保存控件本身的说明，
`GetEffectiveAccessibleDescription()` 会把它与当前校验摘要合并，供宿主的可访问性适配层使用。

验证通知使用 RAII 的 `BindingValidationChangedEvent::Subscribe(...)`。嵌套对象被替换时，
Binding 会断开旧验证源并连接新源；数据源先销毁时不会暴露陈旧验证结果。
`DataSourceUpdateMode::OnValidation` 仍表示文本控件失焦时回写，它与源端验证状态是两个
独立概念。

设计器属性面板提供“编辑数据绑定”入口。结构化编辑器会从所选控件的元数据列出目标属性，并根据属性的读、写和变更通知能力过滤 `BindingMode` 与更新策略；源路径支持 `Profile.Name` 这类点分路径。编辑器可选择内置的 `BooleanNegation`、`StringIsNotEmpty`、`StringTrim` 转换器，也可保存应用自定义的 Converter ID。宿主连接设计时数据源后，设计器会把持久化配置接成真实运行时 Binding，并在连接前暂存会遮蔽 Binding 的 Local 值；移除 DataContext、修改配置或连接失败时恢复该值。属性行会同步显示连接错误和当前源端验证问题，这些瞬时状态不会写入设计文件。校验呈现选项和 `AccessibleDescription` 可在普通属性面板编辑，并随设计文件和生成代码保存。绑定随 XML 设计文件保存，生成的窗体在存在绑定时提供 `BindData(IBindingSource& dataContext)`；生成代码同样在连接前暂存/清除目标 Local 值，并在 Add 失败时恢复，避免初始化值永久遮蔽 Binding。

未选中控件时，窗体属性面板提供“编辑 DataContext Schema”入口。Schema 可声明点分源路径的值类型及可读、可写、变更通知能力；定义后，Binding 编辑器会提供源路径下拉选择，并同时校验源能力、目标能力以及 Converter 的源/目标类型。嵌入设计器的宿主还可调用 `Designer::SetDesignDataContext(...)` 连接真实 ViewModel，再在 Schema 编辑器中递归导入运行时元数据；循环对象图会被安全截断。未定义 Schema 的旧工作流仍允许自由输入。

当前设计文件格式为版本 5。每个控件保存不可因重命名或重排而改变的 `id`、可解析到普通父控件的 `parentId`，以及防止删除后复用编号的文档级 `nextId` 高水位；可选 code-behind 元数据保存经过校验的 C++ 类身份和相对设计文件、无扩展名的代码基路径，不保存工作站绝对路径。名称引用继续保留用于可读性和 TabPage 等兼容场景。版本 1–4 文件仍可读取，加载时会补齐缺失状态并在下次保存时升级。运行时控件公开 `DesignId` 和 `FindControlByDesignId(...)`，生成代码会写入同一 ID，为动态 XML 加载与静态生成代码提供共同索引协议。

自定义 Converter 需要在调用生成窗体的 `BindData` 前注册；元数据让运行时和设计器都能判断目标值类型与反向转换能力：

```cpp
BindingValueConverterRegistry::Register(
    { L"Application.Trim", BindingValueKind::String,
      BindingValueKind::String, true },
    []
    {
        return std::make_shared<MyTrimConverter>();
    });
```

## 界面截图

### 设计器

可视化设计器支持拖放布局、属性编辑和代码生成。

![CUI Designer](imgs/Designer.png)

### Demo 窗口与菜单

示例程序包含主窗口菜单、独立上下文菜单，以及 TabControl 的多个演示页面。

| 主窗口菜单 | 上下文菜单 |
| --- | --- |
| ![Window Menu](imgs/Menu.png) | ![Context Menu](imgs/ContexMenu.png) |

### TabControl 页面截图

以下截图对应 Demo 中选中 TabControl 不同页面时的显示效果：

| Tab 1 | Tab 2 |
| --- | --- |
| ![Tab 1](imgs/Tab1.png) | ![Tab 2](imgs/Tab2.png) |

| Tab 3 | Tab 4 |
| --- | --- |
| ![Tab 3](imgs/Tab3.png) | ![Tab 4](imgs/Tab4.png) |

| Tab 5 | Tab6 |
| --- | --- |
| ![Tab 5](imgs/Tab5.png) | ![Tab 6](imgs/Tab6.png) |

| WebBrowser |
| --- | --- |
| ![WebBrowser](imgs/WebBrowser.png) |

### 多媒体页面

MediaPlayer 页面演示了框架内置媒体播放控件。

![MediaPlayer](imgs/MediaPlayer.png)

## 注意事项

- **仅支持 Windows**：依赖 Windows 图形栈（Direct2D/DirectWrite/DirectComposition）。
- **Windows版本限制**：`CUI` 支持 Windows 7+。通过预处理器宏 `CUI_ENABLE_WEBVIEW2` 控制是否启用 DirectComposition + WebView2 功能（需要 Windows 8+）；不定义该宏时仅使用 Direct2D HWND 渲染，兼容 Windows 7。
- **项目依赖关系**：
  - `CUI` 依赖 `D2DGraphics`
  - `CUITest` 已内置原先来自 `Utils` 的轻量测试辅助逻辑，不再依赖 `Utils`
  - `CuiDesigner` 当前依赖 `CUI` 和 `Utils`
- **第三方依赖**：WebView2；仓库中的图形/工具源码已直接包含，无需额外引入 `CppUtils/Graphics`
- **设计器输出**：设计器会按扩展名保存 XML 或 CUI XAML 设计文件并生成 C++ 代码；建议把 `.cui.xml` / `.cui.xaml` 作为长期 UI 源文件纳入版本控制。

## 交流社区
- **QQ群**：522222570

许可证：AFL 3.0，见 `LICENSE`。
