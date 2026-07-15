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
  XML v3 往返、实时预览和生成的 C++ 保持同一语义，预览应用失败后恢复先前样式。
- 样式 Setter 属性目录来自目标控件的运行时元数据；属性下拉、自动值类型/示例值、资源兼容性、
  未出现在画布上的目标类型探针，以及无类型规则仅允许公共属性都必须有回归覆盖。
- 普通 Designer 属性面板中未被旧字段表示的可写元数据属性可直接编辑；应用必须经过类型转换与
  Coerce，规范值通过可选 `props.metadata` 往返，并由代码生成器输出 `TrySetPropertyValue(...)`。
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
源类型校验，并随当前版本 3 XML 往返。版本 3 同时保存文档样式表；序列化器仍接受
版本 1、2 文档并升级到当前版本。
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
   `Visible`、事件开关，以及 Button 分类后的 `Round`、`Width (Auto)` 元数据属性；再修改
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
6. 保存 XML，重新加载，确认层级、样式表、样式标识、`props.metadata` 属性、事件映射和选中控件无异常。
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
   ListView Details 与 GridView 都应在仅查询容器/行列数时保持 `MaterializedAccessibilityCellCount()==0`，首次 GetItem 后只增长到 1。滚动 12k 项 ListView 后，`GetVisibleItemRange()` 返回的 `[start,end)` 候选数仍应只覆盖视口；Icon 间隙命中返回 -1，滚动后坐标直接映射到新行。可在 PowerShell 中设置 `$env:CUI_TEST_TIMINGS='1'` 后运行 Release x64 `CUICoreTests.exe` 获取逐测试耗时。本机本轮大型虚拟集合参考值约 29–44 ms，仅用于后续同机趋势比较，不作为跨机器门禁。
10. 密码框只公开显式 `AccessibleName`，不得把密码内容放入 Name、Value 或 Description；关闭窗口后，MSAA 对象应返回 `CO_E_OBJNOTCONNECTED`，UIA Provider 应返回 `UIA_E_ELEMENTNOTAVAILABLE`，均不得访问已释放控件。
11. 打开 Windows 高对比度，确认窗体、公共控件表面、文本和焦点框切换到系统色；再切换关闭动画、始终显示键盘提示和 150%/225% 文字大小，确认过渡立即完成、键盘焦点可见且字体重新布局。退出设置后应自动恢复，无需重启窗口。

## 已知后续清理项

- 当前 `Debug|x64`、`Release|x64`、`Debug|x86`、`Release|x86` 四套默认启用模式的 `CUICoreTests` 均为 123/123。`CUIEnableWebView2=false` 的 x64 Release 已完整 `Rebuild` 并通过 123/123；随后默认启用模式的 x64 Release 全解决方案也已完整重建，`CUI`、`CUITest`、`CUICoreTests` 与 `CuiDesigner` 均成功，并再次通过 123/123。
- Release 配置若沿用历史 LTCG/IPDB/IOBJ 中间产物，增量链接可能报告 `LNK1103`；对对应配置执行一次 `/t:Rebuild` 可重新生成一致的调试信息，本轮 x64/x86 Release 均已通过完整重建。
- `TextBox` / `PasswordBox` 的输入、选择、拖放和光标绘制转换 warning 已收敛；`ComboBox` 的索引、滚动和下拉绘制转换 warning 已收敛；`GridView` 的行列索引、组合框单元格、编辑路径和主要绘制转换 warning 已收敛；`TabControl` 的页签绘制和拖放循环转换 warning 已收敛；`RichTextBox` 的滚动、绘制和拖放循环转换 warning 已收敛。
- `CUITest` Demo 和自定义示例控件的常见转换 warning 已收敛。
- `Button`、`Label`、`LinkLabel`、`CheckBox`、`RadioBox`、`ProgressBar`、`ProgressRing`、`PictureBox`、`GroupBox`、`LoadingRing`、`Slider`、`RoundTextBox`、`Switch`、`ToolBar`、`SplitContainer`、`StatusBar` 等小控件绘制转换 warning 已收敛。
- `Panel`、`ScrollView`、`TreeView` 的容器循环和主要绘制转换 warning 已收敛。
- 设计器模态编辑器的列表索引类 warning 已收敛。
- `Utils` 中第三方 `sqlite3.c` 的 C4028/C4113 签名兼容 warning 已通过 `Utils.vcxproj` 文件级 `DisableSpecificWarnings` 隔离，未修改 vendored 源码。
