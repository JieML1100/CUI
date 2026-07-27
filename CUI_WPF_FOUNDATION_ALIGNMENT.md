# CUI WPF 底层对齐状态

更新日期：2026-07-27

本文只描述底层对象模型、依赖属性、树关系和渲染边界。它不是“CUI 已完整实现 WPF”的声明。
语义基准优先取自仓库内的 `.NET 11 Preview 6` 源码：

- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/WindowsBase/System/Windows/DependencyObject.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/WindowsBase/System/Windows/DependencyProperty.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/WindowsBase/System/Windows/DependencyPropertyKey.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/WindowsBase/System/Windows/EffectiveValueEntry.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/MS/Internal/FrameworkObject.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/FrameworkElement.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationCore/System/Windows/UIElement.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/Control.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/Panel.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/Border.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/TextBlock.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/TextBox.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/ComboBox.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/RichTextBox.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/PasswordBox.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Documents/TextElement.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/Canvas.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/Grid.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Controls/DockPanel.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Application.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/Window.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/ResourceDictionary.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/StartupEventArgs.cs`
- `DotNetSource/src/wpf/src/Microsoft.DotNet.Wpf/src/PresentationFramework/System/Windows/ExitEventArgs.cs`

## 第八批结论

本批把 `Application` 从静态消息循环/窗口工具集收敛为 WPF 形状的实例生命周期所有者，
并把应用资源接入既有资源与 Style 求值链；HWND、DPI 和 URI 解析等进程级能力继续留在
明确的平台互操作边界。

1. **Application 实例、Current 与唯一性**
   - `Application` 现在继承 `DispatcherObject`，构造时建立进程内唯一实例与
     `Application::Current()`；完成 shutdown 后 `Current` 变为 null，但与 WPF
     “每 AppDomain 最多创建一次”对应，当前 C++ 进程内不会重新签发第二个 Application。
   - 原静态 `Application::Run()` 已改为只能调用一次的实例 `Run()` / `Run(Window&)`；
     CUITest 与 Designer 两个产品入口均由显式 Application 实例拥有消息循环。
   - 静态成员只保留 HWND 互操作快照、DPI/系统视觉偏好和资源 URI resolver。
     平台窗口表与应用的 `_windows` 集合已分离，非应用 Dispatcher 的窗口不会混入
     `Application.Windows` 语义。

2. **Windows、MainWindow 与 ShutdownMode**
   - 同 Dispatcher 的 Window 在建立平台窗口时登记到当前 Application；
     `GetWindows()` 返回只读快照，第一扇应用窗口成为默认 `MainWindow`，也可显式改写。
   - 默认 `ShutdownMode` 为 `OnLastWindowClose`，并补齐
     `OnMainWindowClose`、`OnExplicitShutdown` 及无效枚举拒绝。
   - 普通 `Window.Close` 仍可由 `Closing.Cancel` 阻止；显式 Application shutdown
     会关闭应用窗口并忽略 Closing 取消或处理器异常，和 WPF 不可取消的应用关停边界一致。
   - 模态窗口的 native owner/禁用协调继续使用独立平台窗口快照，避免为互操作需要重新
     扩大 Application 的公开窗口集合。

3. **应用 ResourceDictionary 生命周期进入资源链**
   - `Application.Resources` 采用惰性创建与可替换所有权；替换字典时断开旧 changed
     subscription，旧字典后续变化不会再使应用树失效。
   - 资源查找顺序现在是元素/继承词法资源、文档资源、Application、Theme；应用隐式
     Style 的优先级低于文档/局部 Style、高于 Theme，不会新增一类 DP 值源。
   - 应用资源增加、修改或整体替换会遍历应用窗口，重建 Style 条件订阅并重新求值
     DynamicResource 与隐式 Style；新登记窗口也会立即投影当前应用资源。
   - `ControlStyleSheet` 仍是 CUI 当前对普通资源和 Style 的 lowered runtime
     `ResourceDictionary` 投影。本批没有把它包装成已经完整对齐的公开
     `ResourceDictionary/MergedDictionaries` 对象模型。

4. **Startup、Exit 与 Dispatcher 关停**
   - `Startup` 在 `Run` 后且 `Run(Window&)` 默认显示窗口前只触发一次，并携带除可执行文件
     之外的命令行参数；`OnStartup`/`OnExit` 均保留可覆写事件入口。
   - `ExitEventArgs.ApplicationExitCode` 可在 Exit 中改写最终 `Run` 返回码；
     即使 Exit 处理器抛出，关停清理仍会完成，随后再传播异常。
   - Application shutdown 会永久终止隐藏 UI dispatcher、丢弃未执行回调并使后续
     `PostToUIThread` 失败；“应用退出后静默重建 dispatcher”的旧可能性已封闭。

5. **本批明确未覆盖的上层面**
   - 尚无 `Application.xaml` 生成入口、`StartupUri`、navigation lifetime、
     Activated/Deactivated/SessionEnding 等上层事件。
   - 当前只有单 UI dispatcher 的 Application window collection；WPF 的
     `NonAppWindows` 跨 Dispatcher 资源失效路径还没有等价公开对象模型。
   - 以上项目继续保留为后续工作，不影响本批已经对齐的实例所有权、资源优先级和
     shutdown 基础边界。

## 第七批结论

本批处理 `Control.Template` 与各文本控件的 `Text`：移除内部 `Control`
behavior host 上共享的 `_template/_text` 作者值镜像，让声明值、行为同步状态与渲染缓存
重新落到各自边界。

1. **Template 改用规范有效值槽**
   - `Control.Template` 现在由 DependencyObject effective-value entry 唯一持有；
     wrapper、Style、Binding、DynamicResource、VisualState 与 `ClearValue` 读取同一份有效值。
   - `_template` 字段和 getter/setter metadata 回灌已移除；Template 的
     Style → Local → `ClearValue` 回退会通过同一 changed callback 原子撤销旧视觉树。
   - `_templateNameScope`、`_controlTemplateRoot`、模板事件连接、视觉状态运行时、
     apply/retry 标志和错误诊断继续保留。它们描述已实例化模板的运行状态，不是作者值。
   - WPF `Control` 为性能维护 `_templateCache`；CUI 的 `ControlTemplateReference` 是轻量值，
     当前直接读取 effective-value slot，不再维护一份可能漂移的等价缓存，公开语义一致。

2. **Text 按 WPF owner 拆成独立值槽**
   - TextBlock（当前原生实现类名 `Label`）、`TextBox`、`ComboBox` 的 `Text` 各自注册
     独立 DependencyProperty 身份；内部 `Control` 只保留复用 wrapper，不再拥有共享文本字段。
   - 新增 slot-backed + owner-specific subscriber 注册路径，使值仍只存在于 DP 槽，
     同时保留 `TextBox.Text` 的 LostFocus source update 等行为 metadata。
   - `ComboBox.Text` 补齐 `BindsTwoWayByDefault`；`TextBox.Text` 继续保持
     TwoWay + LostFocus 默认更新契约。
   - TextBlock 的格式化文本对象、RichTextBox 的编辑 buffer/布局对象，以及 TextBox 的
     选择、caret、scroll 状态均保留为派生运行时缓存；Text changed callback 只负责使这些
     投影失效或同步。

3. **框架内部文本变更不再覆盖表达式**
   - TextBox 用户编辑、ComboBox 选择同步、现有 RichTextBox 编辑兼容路径改走
     `TrySetCurrentPropertyValue`，对应 WPF `SetCurrentValue`/internal current-value 语义。
   - 这些内部行为会更新当前目标值并正常触发 TwoWay source update，但不会把 Local
     Binding 或 DynamicResource 替换成字面量。
   - `Text` 的 accessibility Name/Value 通知收口到有效值发布末端，因此 Local、Style、
     Binding、资源和 `ClearValue` 的变化具有同一通知语义。

4. **明确非 Text 状态与尚存差异**
   - `PasswordBox.Password` 不再借用 `Control::_text`，密码内容回收到
     `PasswordBox::_password` 私有编辑状态，避免进入普通 Text DP/自动化名称路径。
   - WPF `RichTextBox` 公开 `Document`，并没有 `Text` DP；CUI 现有
     `RichTextBox.Text` 仅完成独立 slot-backed 兼容存储，仍是待 FlowDocument/文本容器
     对象模型落地后移除的公开差异，本文不把它标记为 WPF 已对齐。
   - WPF ComboBox 的 editable TextBox、按文本反查 selection、Journal 等更高层行为仍未
     由本批实现；本批只收敛底层属性身份、值源和表达式保持语义。

## 第六批结论

本批处理 `Background`、`Foreground`、`BorderBrush`：把过去注册在内部
`Control` behavior host 上并投影到结构元素的三项 Brush，改成 WPF 对应的共享
DependencyProperty 身份，同时把作者值与系统色渲染兜底彻底分开。

1. **三项 Brush 改用规范有效值槽**
   - `Background`、`Foreground`、`BorderBrush` 的 wrapper、Brush 工厂和 renderer
     现在都读取 DependencyObject effective-value entry。
   - `_backgroundBrush/_foregroundBrush/_borderBrush` 三个 optional 镜像字段以及六个
     Apply/Clear 同步钩子已移除；Style、Local、Binding、DynamicResource、VisualState、
     Animation 和 `ClearValue` 不再把有效值复制到第二份作者状态。
   - `GetBackgroundBrush/GetForegroundBrush/GetLocalBorderBrush` 改为返回当前有效 Brush
     的 optional 值快照，避免向外暴露已经删除的字段引用。

2. **恢复 WPF 的 Brush owner 和属性身份**
   - `Panel` 注册 `Background`；`Control` 与 `Border` 通过 `AddOwner` 复用同一枚
     DependencyProperty。
   - `Border` 注册 `BorderBrush`；`Control` 通过 `AddOwner` 复用该身份。
   - 新增非视觉 `TextElement` property-owner shell：它只承载当前文档对象模型尚未实现时
     所需的 `Foreground/Background` 身份，不伪装成可构造 XAML 元素。
   - `Control.Foreground` 和 TextBlock（当前原生实现名为 `Label`）的 `Foreground`
     共享 `TextElement.Foreground`；TextBlock 的 `Background` 则使用
     `TextElement.Background`，与 `Panel.Background` 保持同名不同身份。

3. **owner metadata 默认值和公开边界**
   - `TextElement/TextBlock.Foreground` 默认是 WPF 的黑色并带 `Inherits`；
     `Control.Foreground` 的 owner metadata 仍以 `NoBrush` 表示动态系统文本色。
     这是当前 CUI 高对比度/系统主题桥接，系统色只由 `RendererForegroundColor` 在
     无 authored Brush 时选择，不会写回 DP。
   - Background/BorderBrush 的 `NoBrush` 同样只表示不绘制；原始
     `Renderer*Color` 保留为尚未模板化的 native renderer fallback，而不是依赖属性值源。
   - Panel、Border、TextBlock 现在依靠自己的 owner metadata 出现在实例与无实例
     XAML Schema 中；旧的 `Control.Background/Foreground/BorderBrush` 类型特判已删除。
     Panel 仍不公开 Foreground/BorderBrush，Border 不公开 Foreground，TextBlock 不公开
     BorderBrush。

## 第五批结论

本批继续迁移 owner 边界清楚、但过去仍依赖 `Control`/`FrameworkElement` 字段镜像的
布局与 chrome 属性，并把实例查找和无实例 XAML Schema 的 owner 分支选择统一起来：

1. **5 组属性改用规范有效值槽**
   - `Padding`、`HorizontalContentAlignment/VerticalContentAlignment`、
     `BorderThickness`、`ClipToBounds` 的 CLR-shaped wrapper 现在只读写
     DependencyObject effective-value entry。
   - `_padding`、`_borderThickness`、`_clipToBounds` 和两项 ContentAlignment backing
     field 已移除；`GetSpecifiedLayout()`、Label 文本布局和 Border 布局/绘制都读取当前有效值。
   - Style、Local、TemplateBinding、VisualState、Animation 与 `ClearValue` 不再需要 setter
     把结果同步到第二份字段；静态 `SetPropertyField` 审计从 122 降到 117 个调用点。

2. **Padding 与 BorderThickness 恢复 WPF owner 身份**
   - `BorderThickness` 现在由 `Border` 注册，`Control` 通过 `AddOwner` 复用同一枚
     `DependencyProperty`；Button、TextBox、ComboBox、GroupBox 等默认边框继续通过派生
     metadata override 提供，不再由构造函数预写字段。
   - `Control.Padding` 与 `Border.Padding` 现在是两枚同名但不同身份的属性：
     Control 分支与本地 WPF 一样不设置 ValidateValue，Border 分支拒绝负数和非有限 Thickness。
   - CUI 的原生 Control/Label 实现仍直接消费 Padding，因此 Control metadata 在 WPF 的
     `AffectsParentMeasure` 之外暂时保留自身 Measure/Render 失效；这是当前无模板实现叶的
     明确桥接，不是第二个值源。
   - `HorizontalContentAlignment/VerticalContentAlignment` 归 `Control` 所有，并增加与
     FrameworkElement alignment 相同的枚举 ValidateValue；`ClipToBounds` 的属性身份归
     `UIElement`，默认值为 false，变化继续触发 Arrange 和裁剪损伤更新。

3. **公开 Schema 与实例使用同一 owner 分支**
   - XAML Schema 的无实例属性枚举现在先按公开 `UIClass` 过滤原生 owner metadata，再合并
     override/AddOwner 层；不会因为 `Border/Panel` 的内部 C++ `Control` 继承而先合并错误分支。
   - Schema owner 闭包已纳入 `DependencyObject/Visual/UIElement/FrameworkElement`，
     因此 `ClipToBounds` 等基类属性不再依赖伪装成 `Control` owner 才能被发现。
   - Panel、ContentPresenter、ItemsPresenter、WebBrowser 等结构/平台宿主仍不能设置隐藏的
     Padding 或 BorderThickness；继承来的 C++ wrapper 对这些宿主保持无副作用的默认投影。

## 第四批结论

本批把 FrameworkElement 布局声明和三组容器 attached property 接到第三批建立的
slot-backed 路径，关闭布局引擎仍从 CLR backing field 读取作者值的问题：

1. **18 个布局属性改用规范有效值槽**
   - `Width/Height/MinWidth/MinHeight/MaxWidth/MaxHeight`、`Margin`、
     `HorizontalAlignment/VerticalAlignment`。
   - `Canvas.Left/Top/Right/Bottom`、`Grid.Row/Column/RowSpan/ColumnSpan`、
     `DockPanel.Dock`。
   - CLR-shaped wrapper 只调用 `GetDependencyPropertyValue` /
     `SetDependencyPropertyValue`；相应的 Canvas、Margin、Alignment、Grid、Dock 字段以及
     兼作 Width/Min/Max 存储的 `_layoutStyle` 已移除。

2. **布局读取的是 effective declaration，而不是字段镜像**
   - `GetSpecifiedLayout()` 现在按需返回只读 `LayoutStyle` 快照；Width/Height/Min/Max、
     Margin 和 Alignment 都从当前 DP 有效值投影。
   - Style、Local、Binding、VisualState、Animation 或 `ClearValue` 改变有效值后，Measure /
     Arrange 看到的是同一个结果，不需要 CLR setter 再同步第二份状态。
   - `LayoutState` 继续只保存 DesiredSize、ArrangeRect、约束和 dirty flags；布局计算结果不写回
     作者声明。
   - `Padding` 不是 FrameworkElement 属性，且结构型 Panel 不公开该属性；本批没有把它混入
     FrameworkElement 迁移，快照暂时读取其现有 Control backing，留到下一组按 owner 处理。

3. **attached property 的验证和父布局失效与 WPF 对齐**
   - Canvas offset 变化使父容器 Arrange 失效；Grid cell 与 Dock 变化使父容器 Measure 失效，
     不再只把子元素自身标为需要 Measure。
   - Grid 行列索引必须非负、跨度必须大于零；无效 proposed value 由 ValidateValue 拒绝，
     不再先写入值槽再 coercion 成 0/1。
   - HorizontalAlignment、VerticalAlignment 同样增加枚举 ValidateValue，不能把未定义枚举值
     保存为作者值。
   - Designer 对无效 `Grid.Row` 的测试已改为验证拒绝、错误信息和 Default source 保持不变。

## 第三批结论

本批开始关闭原生依赖属性的“双份可写状态”，先建立可复用的规范存储路径，再迁移
FrameworkElement/UIElement/Control 层不拥有平台资源的基础属性：

1. **新增真正的 slot-backed DependencyProperty**
   - `Register/RegisterReadOnly/AddOwner` 现在提供只接收 metadata 的重载；这类属性不再要求
     C++ getter/setter，也不需要 CLR backing field。
   - metadata 的 `CanRead/CanWrite` 由属性槽能力决定；未显式提供默认值时，使用属性值类型的
     默认值，不能默认构造的类型必须显式提供 default。
   - C++ CLR-shaped wrapper 通过类型化 `GetDependencyPropertyValue` /
     `SetDependencyPropertyValue` 直接调用 DependencyObject 的 effective-value 管线。

2. **EffectiveValueEntry 保存已求值结果，而不是向字段镜像**
   - Local、Style、Template、Theme、Inherited、VisualState、Animation 仍保存 proposed value；
     coercion 后的 effective value 和 source 由同一个 property-engine entry 缓存。
   - 缓存只用于事务 old/new、coercion 重算和读取，不是第二个可由 CLR setter 写入的状态。
   - `SetCurrentValue` 在 Default source 上产生的 base value、`ClearValue` 回退、Reset、
     Binding expression 替换和重新 coercion 均不再依赖隐藏字段。
   - 属性首次按名称或身份访问时会确保其原生 registrar 已运行，CLR wrapper 不再依赖调用方
     预先触发静态注册。

3. **首组 13 个基础属性已移除 backing field**
   - `Tag`、`Cursor`、`Focusable`、`IsTabStop`、`TabIndex`、`AllowDrop`。
   - `FocusManager.IsFocusScope`、
     `KeyboardNavigation.TabNavigation/DirectionalNavigation`。
   - `AutomationProperties.Name/FullDescription/HelpText/AutomationId`。
   - Cursor、AllowDrop 的继承值直接来自 DP 槽；焦点资格和无障碍通知已从 CLR setter 移到
     metadata changed callback，因此 Binding、Style 和继承更新会获得相同副作用。

## 第二批结论

本批关闭了上一批记录的 DependencyProperty 身份与注册边界问题：

1. **DependencyProperty 是稳定的进程级身份**
   - 每个注册属性拥有不可复制的 `DependencyProperty` 对象与稳定 `GlobalIndex`。
   - effective-value 槽、VisualState setter 和 Style setter 都以属性身份为键，不再以
     owner type 加字符串充当存储身份。
   - 同一 owner/name 的重复注册会失败，不再把第二次注册静默折叠为第一次的属性。
   - `DependencyPropertyChangedEventArgs` 会携带同一个属性身份；名称只保留给 XAML、
     PropertyPath 和诊断入口。

2. **`AddOwner` 与每类型 metadata override 共用同一属性**
   - `AddOwner` 不再复制出第二个属性；注册 owner、派生 override 和新增 owner 都通过
     `GetMetadata(target)` 取得各自 metadata。
   - metadata 按实际目标匹配到的继承分支从基类向派生类合并，并缓存为进程期稳定对象。
     无关 `AddOwner` 分支即使先注册，也不会污染另一个派生分支。
   - property-wide `ValidateValue` 留在 DependencyProperty 身份上；default、coercion、
     changed callback、flags 和 getter/setter/subscriber 属于每类型 metadata。
   - Button、TextBox、ComboBox、GroupBox 等派生控件的 `BorderThickness` 默认值已改为
     override `Control.BorderThickness` metadata，不再注册同名的第二个属性。

3. **原生只读属性必须持有 `DependencyPropertyKey`**
   - `RegisterReadOnly` 返回带私有授权令牌的 Key；普通 `SetValue/ClearValue` 不能写入，
     Key 与属性不匹配时同样拒绝。
   - 重复调用 `RegisterReadOnly` 不会重新签发 Key。
   - 原先按名称绕过只读保护的 C++ 路径已封闭；名称入口只保留给挂接到当前宿主的
     declarative component behavior。
   - `Control`、`ButtonBase`、`ContextMenu`、`MenuItem`、`ScrollViewer`、
     `TreeViewItem/TreeView` 的原生只读属性均已迁移。

4. **属性系统不再物理寄居于 `Control`**
   - 注册模板与 property field helper 已从 `Control.h` 移到
     `CUI/include/DependencyProperty.h`。
   - effective-value 实现已从 `Control.cpp` 移到 `CUI/src/DependencyObject.cpp`。
   - `Control` 继续只负责 FrameworkElement/Control 层的布局、渲染、继承和可访问性副作用。

## 第一批已完成的底座

第一批关闭了三个会继续放大上层差异的 P0 问题：

1. **依赖属性宿主归位到 `DependencyObject`**
   - 有效值槽、值源优先级、Binding/TemplateBinding/DynamicResource/Animation 表达式身份、
     `SetCurrentValue`、`ClearValue`、只读键写入和重新强制不再由 `Control` 独占。
   - 非视觉、非 `Control` 的 `DependencyObject` 派生类型现在可以直接承载依赖属性。
   - `Control` 只扩展布局失效、渲染失效、继承传播和无障碍通知等框架元素副作用。

2. **验证与强制成为两个阶段**
   - `DependencyPropertyOptions::Validate` 是与实例无关的值契约，对注册默认值、进入值槽的
     proposed value 和 coercion result 生效。
   - `Coerce` 仍是实例相关回调。
   - `DependencyObject::CoerceValue` 只重算 effective value，不替换 base/proposed value，
     也不改变 Local、Style、Template、Inherited 等值源身份。

3. **FrameworkParent 恢复 WPF 的树语义**
   - 继承父级按 `logical parent -> containing visual parent` 解析。
   - `TemplatedParent` 是独立模板关系，不再冒充继承父级。
   - `ControlTemplate` 生成的每一层视觉节点都不会进入作者逻辑树；只有投影的
     `Content` 归属其内容宿主。
   - 模板尚未挂接时所需的 DataContext 源由 Materializer 的 staging 关系显式解析，
     不泄漏为运行时 FrameworkParent 规则。

## 当前不变量

后续改动必须保持以下约束：

- `DispatcherObject -> DependencyObject -> Visual -> UIElement -> FrameworkElement -> Control`
  是能力边界，不允许再次把依赖属性引擎上移到 `Control`。
- `DependencyProperty` 身份在 owner、`AddOwner` 和 metadata override 之间保持不变；
  ValidateValue 与只读授权不得下沉回某个 owner metadata。
- XAML Schema 是公开类型、属性、事件和模板契约；`UIClass` 与 C++ behavior host 只是内部实现。
- 有效值优先级保持
  `Animation > Local > VisualState > Template > Style > Theme > Inherited > Default`。
- 表达式占用值槽；不能以第二份字段绕过优先级、清值和绑定生命周期。
- 逻辑树负责作者内容、资源和 DataContext 所有权；视觉树负责布局、命中和呈现；
  `TemplatedParent` 负责模板实例身份。
- 默认外观应继续迁移到 Theme 中的 `Style/ControlTemplate/VisualState`；
  native renderer 只能作为尚未模板化的实现叶或临时后备。

## 与 WPF 仍有差距

### P0：下一批底层工作

1. **继续迁移原生属性的规范存储**
   - slot-backed 注册、首组基础属性、FrameworkElement 布局组、chrome/layout owner
     组、三项 Brush 以及 Template/Text 已经完成；当前静态审计仍有 115 个
     `SetPropertyField` 调用点，分布在 34 个 C++ 实现文件。
   - 第四批移除 18 个调用点，第五批再移除 5 个；第六批 Brush 的旧同步钩子没有使用
     `SetPropertyField`，所以计数不变，但已额外移除 3 个并行字段和 6 个同步函数；
     第七批再移除 Template/Text 的 2 个回灌点及共享作者值字段。
     下一组继续按 owner 审计剩余作者 DP 与布局/平台缓存。
   - 必须逐类区分“作者可写 DP”和“布局/平台计算的只读投影”；后者可以保留原生缓存，
     但不能伪装成第二个作者可写值源。WebBrowser/MediaPlayer 等平台叶应最后迁移。

2. **继承上下文**
   - FrameworkElement 的逻辑/视觉继承链已有明确规则，但尚无 WPF 的通用
     `InheritanceContext`、mentor 和 `Freezable` 传播模型。
   - Popup、非视觉资源对象和冻结资源不能继续依赖模板父级或控件特例模拟。

3. **Application 上层启动面与通用资源对象**
   - Application 的实例、Current、Windows/MainWindow、Resources、Startup/Exit、
     ShutdownMode 与 dispatcher shutdown 基础生命周期已经完成。
   - 仍需建立公开通用 `ResourceDictionary`/`MergedDictionaries` 对象模型，以及
     `Application.xaml`、`StartupUri` 和相应代码生成/启动集成。
   - Activated/Deactivated/SessionEnding、navigation lifetime 与跨 Dispatcher
     NonAppWindows 资源失效属于后续扩展面，不能由进程 HWND 表冒充。

### P1：对象模型与呈现

1. **内部 C++ 类型层级**
   - `Panel`、`Decorator`、`ContentPresenter`、`ItemsPresenter` 等结构元素仍复用
     `Control` behavior host；Schema 已屏蔽错误的公开属性，并能在合并前选择正确 owner
     分支，但内部继承仍过宽。
   - 下一步应把可复用布局/视觉/内容能力从 `Control` 拆成组合式服务，再让结构类型落到
     WPF 对应的基类边界。

2. **Visual/Composition 属性**
   - 需要补齐 `Visual` 层的 Opacity、Effect、变换/裁剪组合、缓存与命中规则，并让这些属性
     走 DependencyObject effective-value 管线。

3. **Style/Template 细分优先级**
   - 当前大类优先级已经确定，但 WPF 内部的 implicit style、style trigger、
     template trigger、default style 等子层仍需拆分和逐项验证。

### P2：默认行为与默认风格

审计快照中仍有 33 个 C++ `OnRender()` override，以及 160 处 `Renderer*Color` 命中。
其中部分是合法的图形/媒体/平台实现叶；可模板化控件的状态外观则应继续迁往
`Generic.xaml`。不要为了减少数字而把平台实现塞进 XAML，也不要新增第二套公开外观 API。

## 验证记录

第八批最终验证：

- `Debug|x64` 全解决方案构建通过。
- 新增 Application 生命周期测试，覆盖唯一实例/Current、Dispatcher 亲和性、
  Windows 快照、默认 MainWindow、三种 ShutdownMode 及无效枚举拒绝。
- 同一测试覆盖 Application 资源的惰性创建、查找与替换，元素/文档/Application/Theme
  优先级，DynamicResource 和隐式 Style 热更新，以及旧字典替换后的订阅断开。
- Startup/Exit 顺序、命令行参数入口、Exit code 改写、普通 Closing 可取消、
  Application shutdown 不可取消、Current 清空和 dispatcher 永久关停均有定向断言。
- 静态检查确认旧静态 `Application::Run`、Application 全局窗口集合为 0；
  进程全局表只保留平台 HWND 注册。`SetPropertyField` 保持 115 个调用点/34 个实现文件，
  `git diff --check` 通过。
- `CUICoreTests.exe`：340/340 通过。
- `Designer.exe --self-test` 通过。
- `CUITest.exe --validate-xaml` 通过。
- `CUITest.exe --smoke-xaml` 通过。
- `CUITest.exe --render-smoke` 通过。
- `CuiCodeGen.exe --version` 通过，版本 36。

第七批最终验证：

- `Debug|x64` 全解决方案构建通过。
- 新增 Template/Text 规范值槽测试，覆盖 `Control.Template` 的属性身份、slot-backed
  metadata、Style/Local/`ClearValue` 回退与模板树撤销/重建。
- 同一测试覆盖 TextBlock/TextBox/ComboBox/RichTextBox 的独立 `Text` 身份、owner
  metadata、默认值和 Style/Local/`ClearValue`；TextBox 实际编辑、ComboBox selection
  同步与 RichTextBox 兼容编辑均验证不会替换 Binding 表达式。
- 无实例 XAML Schema 测试新增四个 Text owner 的 slot-backed/owner 断言，并确认
  `Control`/Canvas 不公开 Text、`Control.Template` 使用同一规范身份。
- 静态检查确认 `Control::_template/_text`、Template/Text 的 `SetPropertyField` 回灌均为
  0；`SetPropertyField` 降为 115 个调用点/34 个实现文件，`git diff --check` 通过。
- `CUICoreTests.exe`：339/339 通过。
- `Designer.exe --self-test` 通过。
- `CUITest.exe --validate-xaml` 通过。
- `CUITest.exe --smoke-xaml` 通过。
- `CUITest.exe --render-smoke` 通过。
- `CuiCodeGen.exe --version` 通过，版本 36。

第六批最终验证：

- `Debug|x64` 全解决方案构建通过。
- 新增 Brush 规范 owner/值槽测试，覆盖 Panel/Control/Border/TextElement/TextBlock 的
  属性身份和 owner metadata、同名不同身份的两枚 Background、slot-backed 存储、
  Style/Local/`ClearValue` 回退、颜色到 Brush 的转换、跨 Panel 中间节点的 Foreground
  继承，以及 authored Brush 与 renderer 系统色 fallback 的隔离。
- 无实例 XAML Schema 测试新增 Panel、Border、TextBlock owner 分支断言；三个 Brush
  object-path 动画回归继续通过。
- 静态检查确认三个 optional backing field、六个 Apply/Clear 同步钩子、三项
  `Control` 直接注册及旧 Schema 类型特判均为 0；`SetPropertyField` 仍为
  117 个调用点/35 个实现文件，`git diff --check` 通过。
- `CUICoreTests.exe`：338/338 通过。
- `Designer.exe --self-test` 通过。
- `CUITest.exe --validate-xaml` 通过。
- `CUITest.exe --smoke-xaml` 通过。
- `CUITest.exe --render-smoke` 通过。
- `CuiCodeGen.exe --version` 通过，版本 36。

第五批最终验证：

- `Debug|x64` 全解决方案构建通过。
- 新增 Control chrome 规范 owner/值槽测试，覆盖 5 组属性的 slot-backed metadata、
  Style/Local/`ClearValue` 回退、ContentAlignment 验证、Control/Border Padding 身份与验证差异、
  BorderThickness 的 Border-owner/Control-AddOwner 身份、派生默认 metadata，以及
  ClipToBounds 的 UIElement owner 和 Arrange 失效。
- 结构元素 chrome 隔离、无实例 XAML Schema、通用 Style 值源、GroupBox metadata、
  TextBlock Padding 布局等关联定向测试继续通过。
- 静态检查确认五个 backing field 组和派生构造函数默认写入均已清除；
  `SetPropertyField` 为 117 个调用点/35 个实现文件，`git diff --check` 通过。
- `CUICoreTests.exe`：337/337 通过。
- `Designer.exe --self-test` 通过。
- `CUITest.exe --validate-xaml` 通过。
- `CUITest.exe --smoke-xaml` 通过。
- `CUITest.exe --render-smoke` 通过。
- `CuiCodeGen.exe --version` 通过，版本 36。

改动前基线曾出现一次 `GeometryGroup children expose recursive WPF object paths` 的动画时序失败；
本批完整回归未复现。它没有被本批宣称为已定位或已修复。

## 下一批建议顺序

1. 实现 InheritanceContext/Freezable，再处理 Popup 和非视觉资源继承。
2. 继续按 owner 收敛剩余 DependencyProperty 作者值与运行时缓存。
3. 拆分结构元素的内部 `Control` 继承。
4. 建立公开通用 ResourceDictionary/MergedDictionaries，再接入
   `Application.xaml`/`StartupUri` 上层启动面。
5. 继续细分 Style/Template 优先级并清理可模板化控件的默认 native renderer。
6. 建立 FlowDocument/文本容器对象模型，以 `Document` 替换
   `RichTextBox.Text` 兼容扩展。
