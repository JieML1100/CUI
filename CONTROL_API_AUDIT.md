# CUI WPF 语义审计

本文档记录当前公共架构边界，不再保存已删除 WinForms/早期 CUI API 的功能清单。历史实现不能作为兼容需求；
若旧代码与这里的单一路径冲突，应删除旧代码而不是增加别名、fallback 或双写。

## 固定约定

- 控件类型、属性、事件、命令、模板部件及名称作用域均由 XAML Schema 定义。
- C++ 只承载 native behavior host、事件/命令处理、平台消息、输入、渲染 realization 和自动化 peer。
- C++ 不向 XAML 注册控件类型，也不根据 C++ RTTI/枚举猜测声明类型能力。
- 动态 Materializer、Designer 预览和静态 CodeGen 必须消费同一 XAML QName、属性元数据和事件目录。
- 框架默认外观只定义在 `Themes/Generic.xaml`；C++ 只能嵌入、缓存、物化和执行该 XAML。
- 不保留与新方向无关的 Legacy 兼容入口。

## 本轮确认并清除的旧语义

### 外观与渲染

- 已删除公共 `BackColor`、`ForeColor`、`BorderColor`；公共外观只使用 `Background`、`Foreground`、
  `BorderBrush` 等 Brush 依赖属性。
- 已删除 `FocusedColor`、`FocusBorderColor` 和 Control 尾部直绘焦点/验证装饰器。焦点、验证、hover、pressed
  等状态由 Style、Template/Adorner 和 VisualState 表达。
- `BrushKind::None` 是真正的 unset/no-paint 值；透明色不是“没有 Brush”。颜色字面量只作为 XAML 简写，
  进入模型后只保存结构化 Brush，避免标量文本与对象双表示。
- 公开 setter 走 Local value source；高对比度和默认外观只进入 Theme/fallback，不覆写作者 Local 值。
- Panel 在 `Background=None` 时不绘制 fallback fill；ContentControl/ItemsControl 不安装透明画刷伪 Theme 值。
- TextBlock behavior 不硬画 disabled overlay；状态外观属于 Style/Template/VisualState。

### 自动化与可访问性

- XAML 只公开 `AutomationProperties.Name`、`FullDescription`、`HelpText`、`AutomationId` 等 attached property。
- 自动化能力只由 `AutomationPeer` 声明；已删除 `AccessibleRole`、角色推断、按 `UIClass` 猜 Pattern 和
  `IAccessibilityVirtualizedControl` 并行 provider。
- 虚拟子项使用同一 `AutomationPattern` 集合，包括 ScrollItem、VirtualizedItem、GridItem 和 TableItem。
- Window/UIA provider 只查询 peer。ComboBox 的 UIA Scroll 指标读取其真实 `ScrollViewer`，不再复制滚动模型。

### 焦点与助记键

- 焦点状态只来自每 Window 唯一 `FocusManager` 的只读 `IsFocused`、`IsKeyboardFocused`、
  `IsKeyboardFocusWithin` 投影；selection 不得冒充 focus。
- `Focusable` 决定可否取得键盘焦点，`IsTabStop` 只决定 Tab 候选资格。
- 助记键只解析 WPF `AccessText` 标记 `_`，`__` 表示字面 `_`。不存在通用可写 `AccessKey` 依赖属性。

### 类型与静态生成

- `RuntimeTypeId(namespace URI + local name)` 是声明类型的唯一身份；`UIClass` 只是内部 native behavior host。
- Parser 对控件和全部 `Style.TargetType` 保存 QName；动态 Materializer 与静态 CodeGen 都附加同一
  `DeclarativeTypeDescriptor`。
- 静态 Style selector 同时输出 QName，不能只输出 `UIClass`。当前 CodeGen 契约为 v32，保证旧生成 stamp 失效。
- 静态 generated base 构造函数不展开 XAML；用户 code-behind 构造函数体调用 `InitializeComponent()`，确保
  C++ 虚事件、命令和模板状态在完整派生实例上连接。旧 base-constructor lowering 不保留。
- 旧 native 名称不再作为 XAML QName 接受，也不通过 serializer/loader 自动升级。
- `Panel` 是抽象 XAML 类型；`ComboBoxItem : ListBoxItem`、`ToolBar : HeaderedItemsControl`、
  `Popup : FrameworkElement` 且只拥有 `Child`，能力由真实层级而不是 Designer/serializer 白名单决定。
- `Panel`、`Decorator`、`ContentPresenter`、`ItemsPresenter`、TextBlock behavior host 和 `WebBrowser`
  均停在 `FrameworkElement` 语义边界；`Border : Decorator`，`ItemsControl : Control`。私有 C++ behavior
  继承不能扩大 projected XAML 能力。

### 身份与值转换

- QName、成员、依赖属性、事件、资源 key、`x:Name`、binding/DataContext 路径、VisualState/Storyboard、
  TargetName/PropertyPath、Style 条件和 Designer 扩展 ID 均精确匹配。
- 错误大小写不被 parser、runtime、Designer 或 CodeGen canonicalize 成正确身份。
- 声明组件的 String `AllowedValues` 是作者数据，精确比较；bool 只接受不区分大小写的 `true/false`，
  不接受 `1/0/yes/no/on/off` 或空串。
- 仅文件系统路径、展示搜索/排序、enum/value token 以及显式 `CollectionViewSource.IgnoreCase` 数据比较允许
  大小写折叠。

### 树、宿主与基础状态

- Visual children 只有只读 view；所有权变化和重排必须经过 `Control` 的受控事务 API。
- presentation Window 只有只读查询，不能由任意调用方改写；平台宿主只由 Window 内部拥有。
- `DesignId` 只有只读 runtime 查询和 infrastructure-only 写桥，不是依赖属性或应用状态。
- `Tag` 是任意 `BindingValue` 的对象依赖属性；Cursor 是可继承依赖属性，最终解析不会被控件的默认命中策略覆盖。
- 文本编辑缓存均为私有状态；选择和 caret 只能通过规范编辑 API 改变。
- 公共 CLR-shaped 依赖属性 setter 等价于 `SetValue` 并产生 Local source；behavior 内部仅在明确需要保留
  expression 时使用 `SetCurrentValue`。
- projected 类型找不到有效依赖属性元数据时，`SetPropertyField`/`SetCurrentPropertyField` 必须拒绝写入，
  不得落入共享 C++ backing。通用 `Text` 也不能借 behavior-host 基类泄漏给非文本类型。

### 原生 realization 边界

- 公共 typography 只有 `FontFamily` / `FontSize`；不存在可持有或替换的 `Control.Font` 对象。
- `GetRenderFont()` 和 `GetDrawingContext()` 仅供 C++ native behavior 在 measure/render 阶段使用。
- Window 不公开 `Render` 或 `PlatformWindowHost`。扩展绘制通过 `NativeSurfaceRenderContext`，不能任意取得并长期保存
  当前 D2D frame context。
- `PresentationRenderHost` 是设备/surface/frame transaction 的唯一 owner，`PresentationScene` 是 retained 结构权威。
- `PresentationRenderHost` 可跨帧持有设备和 surface 资源，但 active `DrawingContext` 只在已打开的 frame surface
  内存在；关闭 surface、commit/abort 和 device recovery 后必须为空。`WM_PAINT` 通过 host attached 状态启动
  transaction，不能要求帧外 context 预先存在。

### 已删除的容器/内容旁路

- StackPanel 不公开 `Spacing` 或容器级 content alignment；间距和交叉轴对齐由子元素 Margin/Alignment 表达。
- GroupStyle 只保留 HeaderTemplate 等声明语义，不保存 HeaderIndent/HeaderSpacing/HeaderHeight 固定像素布局。
- 不存在 ContentText/HeaderText 字符串捷径或 ContextMenu 字符串 item bag；内容与项目只走对象树、模板和 Items 管线。
- `Control` 不公开 `CanvasLeft`、`GridRow` 或 `DockPosition` 等扁平属性；附加值只由
  `Canvas::Get/Set*`、`Grid::Get/Set*`、`DockPanel::Get/SetDock` 投影。

### Style/Resource 降级边界

- `Control` 不公开 Style key、Theme/Document StyleSheet 或 ResourceDictionary backing 的 getter/setter。
- Runtime、Designer 与 CodeGen 只能通过 infrastructure-only `StyleAccess` 安装 XAML 降级结果。
- `ControlStyleSheet` 是 XAML Style/Resources 的物化 IR，不是 CSS 式第二作者系统；应用代码只使用
  ResourceDictionary、Style、Setter、Trigger、Template 与 Dynamic/StaticResource 语义。
- `Themes/Generic.xaml` 是框架 Theme 的唯一声明源；共享 `XamlDocumentCompiler` 为 Runtime、Designer 和
  CodeGen 展开同一模板树，Theme 与作者 Style 保持不同值源。
- `Button` 的默认 Style、ControlTemplate、TemplateBinding 和 CommonStates 已迁入 Generic.xaml。
  Local Template 和作者 Style.Template 可覆盖 Theme；C++ 不提供第二套默认模板注册。
- Style base Setter 必须先进入有效值系统，再编译和同步依赖这些对象的 Trigger/Storyboard clocks；
  不能靠 native 构造器预制透明 Brush 来让对象路径“碰巧可解析”。

### 结构布局与 chrome 所有权

- `Control.BorderThickness` 是四边 `Thickness`，Runtime、Designer、CodeGen、布局和渲染共用同一类型。
  `Border` 不保存第二份 thickness backing，也不重复注册同名 metadata。
- ContentPresenter/ItemsPresenter 不自动消费 Padding；Popup 不拥有 Background/Padding；WebBrowser 外观由
  XAML Border/Template 组合表达。结构类型不能读取隐藏 Control chrome。
- 模板根始终安排在 Control 的完整 final slot；Padding 由模板树中的 Border/Presenter 通过 TemplateBinding
  消费一次，behavior host 不预先 deflate。
- `Background=None` 表示无绘制；透明 Brush 只有作者或 Theme XAML 显式声明时才存在。
- `AffectsParentArrange` 必须使父 Panel 的 child-arrangement policy 失效；Canvas attached offset 变化需要真实
  重排子元素，同时保持 Measure 缓存有效，并只推进 retained geometry revision。

### 生命周期、拖放与输入身份

- Window 事件面只保留 WPF `Closing`、`Closed`、`LocationChanged`、`ContentRendered`；不存在 `Shown`、
  `ThemeChanged`、FormClosing/FormClosed 或二次 system-key 发布。
- 拖放只有 OLE + routed `DragEventArgs`；不存在 `WM_DROPFILES`、`DropFile`、`DropText` 或平行文件列表载荷。
- `SizeChanged` 携带 PreviousSize/NewSize，位置变化单独发布；`IsVisibleChanged` 只观察有效 Visibility 转换。
- `Key` 数值不等于 Win32 virtual key。`VK_*` 只在 Window 平台边界映射；公开 system key 只由
  `Key == Key::System` 和 `SystemKey` 表示，不存在第三个 `IsSystemKey` 布尔状态。
- `Key`、`ModifierKeys`、`MouseButton`、`MouseButtonState` 和 `MouseButtonStates` 是正交身份；不接受
  Keys/MouseButtons 混合掩码、KeyData、PressedButtons 或 SuppressKeyPress。
- CodeGen 输出 Key/MouseAction/ModifierKeys 的符号表达式，不把 enum 整数写入生成 ABI。

### 事件所有者与发布权限

- 普通 `Event` 消费者只能订阅；publisher 的 Raise/Clear 只在 `EventInfrastructure.h`。
- parent-change 通知只由 Tree infrastructure 发布；route engine 的 handler-table 访问只在
  `RoutedEventInfrastructure.h`。
- `Click` 的内部 route slot 是 protected；公开 facade 只存在于 `ButtonBase`、`MenuItem`。
  Designer/Runtime/CodeGen 不再使用 `UIElement::Click` 作为公开事件 owner。
- 其他 owner-specific routed event 同样由实际控件用 `using` 暴露；UIElement 的 protected slot 只是
  `AddHandler`/route 实现，不是所有元素都拥有这些 CLR 事件的声明。

### Window 与 Application 基础设施

- transient presentation、tab order 构建、native color/cursor 投影、输入/focus/text-composition 统计、
  dirty rect、scene/frame/device-recovery 诊断均由 `WindowInfrastructure.h` 窄桥访问，不属于公共 Window 语义。
- `Application::PumpPendingMessages` 已删除；不存在 DoEvents 式同步消息泵旁路。DPI 缩放 helper 是 Window 私有平台实现。
- 未使用的 `Application::ExecutablePath`、`StartupPath`、`ApplicationName`、`LocalUserAppDataPath`、
  `UserAppDataPath` 和 `Window::IsShowingAsDialog()` 已删除；Application 不再兼任 WinForms 式路径工具箱，
  Window 也不公开内部 modal-loop 状态。
- `Application::Run` 仍是当前唯一进程 Dispatcher loop；`GetWindows` 是只读 snapshot，mutable HWND registry 不公开。

## 仍保留但不属于 Legacy 的内部实现

- 无模板时的 native fallback renderer，以及受保护的 `Renderer*Color`/系统色输入。
- `UIClass` 的 behavior-host 映射和框架 class-handler 继承闭包。
- 尚未迁入 `Generic.xaml` 的控件仍可能由 Runtime/CodeGen 以 Theme source 安装 presenter 默认值；
  已完成迁移的 Button 不再依赖这条默认外观路径。
- `Control` 私有 Style/Resource lowering 存储和 Canvas/Grid/DockPanel attached-property backing；外部只能经
  infrastructure bridge 或真实 owner API 访问。
- C++ 内部 `AutomationName`、`ProcessAccessKey` 等存储/行为名称；它们分别实现
  `AutomationProperties.Name` 和 AccessText 输入语义，不是额外 XAML 属性。
- `UIClass`、native fallback renderer、内部 ObservableCollection 通知和非拥有 parent 指针；它们均封闭在
  behavior/所有权实现内，不决定 XAML 类型身份，也不公开第二个可写状态面。
- XAML `DesignId` current-only 编辑元数据及只读 runtime identity；它服务增量热重载/Designer，不允许应用改写。
- Window `Handle` 是明确的 HWND interop 投影；具体 `PlatformWindowHost` 和 DrawingContext 不公开。

以上内部路径都必须让位于 XAML 作者值，且不得注册为第二套公共属性或类型系统。

## 当前持续门禁

- 全仓非文档代码不存在 `BackColor`、`ForeColor`、`BorderColor`、`AccessibleRole`、`FocusedColor`、
  `FocusBorderColor`、`IAccessibilityVirtualizedControl` 或 `AccessibilityVirtualPattern`。
- XAML QName 经 Parser → canonical XAML → Parser、动态 Materializer和静态 CodeGen 往返不丢失。
- Brush shorthand 经往返后只剩结构化对象表示。
- Visual child、presentation host、DesignId、Tag、Cursor 与文本选择均只能走各自的单一规范入口。
- 任意 XAML Cursor 值能覆盖控件默认区域光标，Theme/Default 才调用 native `QueryCursor` 行为。
- 公共头文件不存在 `Window.Render`、`GetPlatformWindowHost()` 或 `Control.Font` 对象入口。
- 普通 UIElement 不公开 Click；生成物不含 `&UIElement::Click` 或 numeric `static_cast<Key>(...)`。
- 非基础设施代码不调用 Window presentation/input/focus/text diagnostics，且不存在 `PumpPendingMessages`。
- 公共面不存在 Application 路径杂项工具或 `Window::IsShowingAsDialog()` 状态探针。
- 输入公共面不含 `IsSystemKey`、KeyData、MouseButtons/PressedButtons 或旧 DropFile/DropText。
- public `Control` 不含 StyleSheet/ResourceDictionary lowering setter 或扁平 Canvas/Grid/Dock 属性。
- 错误大小写的 XAML 类型、成员、资源、binding path、VisualState/Storyboard 身份和声明字符串候选值被拒绝。
- bool legacy token 被拒绝。
- Generic.xaml、作者 Style 与 Local 值分别保持 Theme、Style 与 Local 来源；模板内部值保持 Template 来源。
- unsupported projected property 不产生隐藏 backing 写入；公开 DP wrapper 写入后报告 Local source。
- structural element 不消费未声明的 Control Padding/Border/Text，模板 Padding 不得双重扣除。
- BorderThickness 只有一个 Thickness backing；Border、布局和 renderer 读取同一有效值。
- Style Setter 先于依赖对象路径的 Trigger/Storyboard clock 物化。
- frame transaction 外不暴露 DrawingContext，host/device 的跨帧所有权与当帧绘制权限严格分离。
- Canvas 等 attached layout property 会使父 Panel 真正重排，并命中独立 geometry revision/cache 分类。
- 静态生成物只在 `InitializeComponent()` 中展开 XAML，用户构造函数必须显式调用它。
- Designer、CUITest、Runtime sample、static generated sample、CodeGen version 与完整核心回归全部通过。

## 下一阶段

1. 将 CheckBox、RadioButton、ProgressBar、Slider 等剩余控件的默认 Style/ControlTemplate/VisualState
   批量迁入 `Generic.xaml`，覆盖完成后删除对应 native fallback。
2. 补齐 Background/Foreground/BorderBrush 在所有 renderer 中对渐变、图片、Transform 和 None 的 realization。
3. 建立独立 Adorner 层承载 validation/focus 等非内容视觉。
4. 扩展 AutomationPeer 与虚拟 peer，但不得恢复基于具体控件类型的 Window/UIA 分派。

当前 current-only Designer Schema 为 v43，静态生成契约为 v32，核心回归为 317/317。旧 native 名称、
旧 QName、旧 snapshot、旧生成 stamp、base-constructor lowering、隐藏 chrome backing 和错误大小写 fallback
均不保留。
