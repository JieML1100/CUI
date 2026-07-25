# CUI 向 WPF 架构推进的总纲

状态：已决策，后续实现以本文为准。

## 1. 产品定位

CUI 的目标不是复刻 WinForms API，也不是把 Win32、WinUI、CSS 和 WPF 的概念拼装在一起；目标是建立一套面向 C++ 的、高性能的 WPF 式桌面 UI 框架。

“与 WPF 对齐”首先指模型和语义一致，而不是逐字复制 .NET API：依赖属性、逻辑树与可视树、路由事件、模板、样式、绑定、资源、布局、保留式渲染和线程亲和性必须形成同一套闭环。C++ 可以采用更明确的所有权、更紧凑的数据布局和编译期优化，但不能因此破坏这些上层语义。

## 2. 不可变更的约定

1. XAML 是公开声明模型的唯一来源。控件类型、属性、事件以及相关元数据由 XAML Schema 定义。
2. C++ 不注册应用控件类型到 XAML，不用宏、静态初始化器或手写工厂模拟 .NET 反射。
3. C++ 只实现框架内建行为，以及为 XAML 类型覆盖或挂接消息、事件、命令、布局和渲染行为。
4. 新架构只有一条主路径。与新方向无关的 Legacy 实现、别名、双轨状态和兼容分支直接删除，不为“暂时不炸”长期保留适配层。
5. 迁移按完整模块批量推进。一个大模块基本闭合后，再统一编译、修补和测试；不以每个小改动都保持可运行作为约束。
6. Designer 与 Runtime 消费同一份不可变 Schema 和文档语义；Designer 可以附加设计期信息，但不得拥有另一套运行时规则。

## 3. 当前架构诊断

当前实现已经具备依赖属性雏形、绑定、样式、模板、资源、ItemsControl、路由事件和 Direct2D 渲染等重要能力，但底层仍存在几组互相冲突的模型：

- `Control` 同时承担属性宿主、树节点、布局节点、输入节点、样式宿主、模板宿主和绘制对象，接近传统控件工具箱的“大对象”模型。
- `Form`、`Control` 和原生窗口/消息之间存在并列身份，而 WPF 所需的是 `Window` 作为元素、`HwndSource` 作为宿主的单向关系。
- 动态声明属性和事件逐实例安装，类型身份以两个字符串附着在实例上；这不是类型系统，也使 Runtime、Designer、Binding 和事件系统重复解释同一份定义。
- `Parent` 同时隐含逻辑父级、可视父级、模板父级和事件路由父级，模板、资源查找、继承、命中测试和事件路由因而无法拥有准确语义。
- 布局和绘制仍偏向“控件递归立即绘制”，缺少稳定的保留式场景节点、统一失效传播以及布局/渲染调度边界。
- 一部分命名和 API 仍带有 WinForms、CSS 或历史实现的主导语义，造成相似功能存在多入口、多套优先级或多种生命周期。

结论：现有代码可以保留大量算法和平台能力，但对象模型、Schema、树、属性值系统、窗口宿主和渲染调度必须先后收敛。仅重命名控件不会解决问题。

## 4. 目标分层

核心对象层固定为以下职责链：

```text
DispatcherObject
  └─ DependencyObject
      └─ Visual
          └─ UIElement
              └─ FrameworkElement
                  └─ Control
                      └─ ContentControl / ItemsControl / ...
```

- `DispatcherObject`：线程亲和、调度器访问和跨线程保护。
- `DependencyObject`：依赖属性槽、表达式、继承、强制值与变更通知。
- `Visual`：可视父子关系、变换、裁剪、不透明度、绘制节点和脏区。
- `UIElement`：测量/排列、命中测试、输入、焦点和路由事件。
- `FrameworkElement`：逻辑树、资源、样式、数据上下文、名称作用域和模板关系。
- `Control`：控件模板、视觉状态和控件级行为，不再承担窗口宿主或通用 Schema 注册职责。

这条链首先是职责边界；迁移期间可以分阶段拆文件和存储，但新功能不得继续堆入旧的全能 `Control`。

## 5. XAML Schema 与类型身份

建立唯一的、共享的、不可变的声明 Schema：

```text
XAML 文档 / 组件定义
        ↓
RuntimeTypeId = { namespace-uri, local-name }
        ↓
DeclarativeTypeDescriptor
  ├─ property descriptors
  ├─ routed-event descriptors
  ├─ base/content/name-scope metadata
  └─ optional design metadata
        ↓
Runtime 与 Designer 共同消费
```

规则：

- QName 前缀只属于文档语法，类型身份由 namespace URI 与 local name 构成。
- 属性和事件描述符属于类型，不属于控件实例；实例只保存紧凑的属性值槽和行为状态。
- 一个 XAML 类型在一次文档会话中只有一个规范描述符；Binding、Style、Template、Event、Serializer 和 Designer 必须引用它，而不是再次按字符串推断。
- 框架内建 C++ 类型可以提供内建描述符；应用组件类型只能来自 XAML Schema。
- C++ 行为解析以 `RuntimeTypeId`/描述符为入口，不能反向成为声明类型注册系统。

## 6. 树模型

必须拆分并明确三种关系：

- 可视树：渲染、变换、裁剪、命中测试和可视失效传播。
- 逻辑树：资源查找、数据上下文继承、内容关系和主要生命周期。
- 模板关系：`TemplatedParent`、模板名称作用域和模板生成内容。

事件路由由事件元数据选择逻辑/可视组合策略，不再无条件沿单一 `Parent` 链。依赖属性继承和资源查找也必须使用各自定义的继承上下文，而不是共享一个含糊父指针。

## 7. 属性、表达式和事件

属性系统最终只保留一个有效值管线：

```text
Default < Inherited < Theme < Style < Template < VisualState < Local < Animation → Coerce
```

- Binding、DynamicResource、TemplateBinding 和 Animation 都是来源槽内的表达式身份，不是额外的值来源或旁路存储。
- 各来源先保存未经 Coerce 的 proposed value；选出最高优先级来源后才执行 Coerce，隐藏来源不被高层的强制结果污染。
- `SetCurrentValue`、`ClearValue`、值来源查询和失效标志必须在同一管线中实现。
- 路由事件描述符属于 Schema；实例仅保存处理器表。
- 原生消息先转换为输入事件，再经 `InputManager`/焦点/捕获生成路由事件；业务控件不直接扩散 Win32 消息语义。
- 命令是输入手势、路由事件和可执行状态之上的独立层，不用普通回调代替。

## 8. 窗口与原生互操作

目标关系为：

```text
Window (FrameworkElement)
  └─ HwndSource / PlatformWindowHost
      └─ HWND + 消息接入 + DPI/IME/无障碍桥接
```

删除 `Form` 与元素树并列的第二套 UI 身份。需要原生子窗口的场景通过专门的 `NativeSurface`/`HwndHost` 边界实现；普通控件不拥有 HWND，也不暴露 WinForms 式窗口生命周期。

## 9. 布局与保留式渲染

保留 DIP、浮点布局和 Direct2D/DirectWrite 后端，但把执行模型收敛为：

```text
属性/树/输入变更
   ↓
统一失效标志
   ↓
LayoutManager（Measure → Arrange）
   ↓
Visual/RenderNode 更新与脏区合并
   ↓
Composition/RenderScheduler 提交
   ↓
Direct2D/平台后端
```

控件输出绘制内容或渲染节点，不自行组织整棵树的立即递归绘制。布局、渲染和输入调度共享同一个 Dispatcher 节奏；动画优先在合成属性上运行，避免无必要地触发布局。

## 10. Runtime 与 Designer 边界

- Runtime 负责 Schema 物化、对象树、表达式、布局、输入和渲染。
- Designer 负责文档编辑、选择器、设计期属性、错误恢复和可视化辅助。
- Parser/Serializer 操作规范文档模型；Materializer 调用 Runtime 公共物化入口。
- Designer 不再通过探针控件、私有运行时规则或重复属性目录猜测最终行为。
- 设计期专用数据放入独立命名空间/附加元数据，运行时可以明确忽略。

## 11. 保留、替换与删除

继续保留并演进：

- Direct2D/DirectWrite 图形后端和设备资源管理。
- DIP/浮点布局、文本测量、几何、画刷和图像基础设施。
- 已有 Binding、CollectionView、ItemsControl/Generator、资源和模板算法中符合目标语义的部分。
- Native surface、IME、DPI、可访问性等必要的平台桥接能力。

批量替换或删除：

- 逐实例 `DefineDynamicProperty` / `DefineDynamicEvent` 和字符串型声明身份。
- Runtime 与 Designer 各自构建属性/事件规则的双轨。
- 把逻辑树、可视树和模板树压成一个 `Parent/Children` 的实现。
- `Form` 主导的窗口模型和普通控件 HWND 化倾向。
- 绕开有效值管线的样式、绑定、触发器或资源旁路。
- 为旧命名、旧默认值、旧选择器或旧生命周期保留的兼容别名与隐式回退。
- 仅为旧测试继续通过而存在、且不符合本文模型的 API。

## 12. 实施顺序

### P0：先固定语义地基

1. 共享 `RuntimeTypeId`、`DeclarativeTypeDescriptor`、属性/事件描述符；删除逐实例声明成员安装。
2. 统一依赖属性值、表达式和优先级存储。
3. 拆分可视树、逻辑树和模板关系，并让继承、资源和事件路由迁移到正确关系。
4. 建立 `DispatcherObject → DependencyObject → Visual → UIElement → FrameworkElement → Control` 的真实职责边界。
5. 让 Runtime 与 Designer 共同使用 Schema 和同一 Materializer 入口。

### P1：窗口、输入与渲染闭环

1. 以 `Window + PlatformWindowHost/HwndSource` 替换 `Form` 双轨。
2. 建立统一 Dispatcher、LayoutManager、InputManager 和 RenderScheduler。
3. 引入稳定的保留式 `RenderNode`/场景树和脏区提交。
4. 完成焦点、捕获、命令、IME、DPI 和可访问性边界。

### P2：WPF 上层能力

1. 完整模板名称作用域、视觉状态、触发器与动画表达式。
2. 完整资源作用域、主题、样式密封与共享策略。
3. 控件族 API 命名和行为统一；最后清理 WinForms/CSS 历史名称。
4. CUITest 按特性域展示完整语义，而不是保留 Legacy API 展品。

## 13. 模块完成门槛

每个大模块采用同一节奏：架构边界确定 → 批量实现 → 删除旧路径 → 全量构建 → 集中修补 → 核心语义与 CUITest 演示验收。

一个模块只有同时满足以下条件才算完成：

- 只有一个规范数据源和执行路径。
- Runtime、Designer、序列化和测试没有复制相同规则。
- Legacy API/状态/回退已删除，或有明确且短期的下一批删除入口。
- 新模型能表达 WPF 对应语义，并保留 C++ 所需的性能和所有权优势。
- 完成统一构建与针对该模块的语义测试；CUITest 提供可观察、可交互的完整演示。

## 14. 第一批实施结果：共享 XAML Schema

P0-1 已于 2026-07-21 按本文的硬切规则落地：

- 新增 `RuntimeTypeId`、`DeclarativeTypeDescriptor` 和 `XamlSchemaContext`。类型身份只由 namespace URI + local name 组成，QName 前缀不进入运行时。
- 属性、路由事件和内容属性描述符归属于不可变类型 Schema；`Control` 实例只持有共享描述符和紧凑属性值槽。
- 删除逐实例 `DefineDynamicProperty` / `DefineDynamicEvent`、字符串类型身份及对应容器，没有旧 API 别名或回退分支。
- Materializer 在一次文档会话中规范化每个类型描述符，嵌套 DataTemplate / ControlTemplate 共享同一 Schema Context。同一 `RuntimeTypeId` 的契约不一致会直接拒绝，不允许以资源作用域为名义覆盖类型契约。
- 内建 C++ 控件继续使用原生元数据，不伪造 XAML QName；声明类型成员不得覆盖基类原生属性。
- Binding、Behavior、Runtime 事件请求、路由事件和生成代码统一消费 `RuntimeTypeId` / Schema；生成 API 已从 `DynamicEventHandlers` 硬切为 `DeclarativeEventHandlers`。
- CUITest 会对 `FeatureCard` 的共享 Schema 进行实际内省，验证 5 个属性、3 个事件、2 个内容槽、只读元数据、默认内容和 Bubble 路由，而不只是验证 XAML 能解析。
- `Debug|x64` 全解决方案构建、核心回归 268/268、Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` 全部通过。

## 15. 第二批实施结果：统一有效值与表达式管线

P0-2 已于 2026-07-21 按无 Legacy 回退的规则落地：

- 每个属性元数据只对应一个 `EffectiveValueEntry`；Inherited、Theme、Style、Template、VisualState、Local、Animation 七个来源槽共同保存 proposed value 与表达式身份，默认值来自同一元数据。
- 删除独立 Binding 来源层。Binding 与 DynamicResource 使用其声明位置的来源槽，TemplateBinding 使用 Template 槽，活动 Storyboard 时钟使用 Animation 槽；值来源和表达式种类可以分别内省。
- 普通 Local 字面写入会替换该槽中的 Binding/DynamicResource 表达式；`ClearValue` 只显露当前低优先级值或元数据默认值，不复活被替换前的旧 Local。`SetCurrentValue` 则保留活动表达式，使控件交互和 TwoWay 回写不破坏 Binding。
- 初次 Binding 转换失败时表达式仍保持安装，有效值回到低层或默认值；不再以“绑定失败”为理由恢复旧 Local。被替换的独立 Binding 会完整断开回调，不能重新夺取目标槽。
- Template 物化出的字面属性和 TemplateBinding 统一进入 Template 来源；Style/Trigger 的 DynamicResource 保留在原 Style 来源，资源暂缺时表达式仍然存在并显露低层值。
- VisualState Setter 固定使用 VisualState 来源，VisualTransition、EventTrigger 和 Storyboard 时钟统一使用 Animation 来源；停止或完成时钟只移除 Animation 槽，准确恢复 VisualState 或更低来源。
- proposed value 在来源选定后统一 Coerce；有效值提交还会核对真实 backing storage，从而修复旧直接字段写入造成的槽值/字段分叉，而不增加同步旁路。
- 删除 DynamicResource 专用表达式表、应用中标志、Binding Local 暂存/恢复以及动画 BaseValue 恢复分支。Slider、Numeric、Split、Tab、Selector、Scroll 等交互路径批量改用 `SetCurrentValue` 语义。
- CUITest 的声明组件与 WPF 语义实验区会实际断言 Template 字面值、TemplateBinding、VisualState、Local、Animation、Binding、Style DynamicResource 的来源/表达式身份，以及 Local > VisualState > Template、Animation > Local 的覆盖与恢复过程。
- `Debug|x64` 全解决方案构建、核心回归 268/268、Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` 全部通过。

## 16. 第三批实施结果：可视树、逻辑树与模板关系拆分

P0-3 已于 2026-07-21 按无第四种旧父级语义的规则落地：

- 删除 `Control::Parent`、`Control::Children`、`ChildCollection`、`OnParentChanged` 和 `SetChildrenParentForm`，不提供兼容别名。`VisualChildren` 是唯一拥有型控件子集合，`GetLogicalChildrenView()` 只投影非拥有的内容关系。
- `GetVisualParent()`、`GetLogicalParent()` 和 `GetTemplatedParent()` 分别保存物理呈现、作者内容和模板实例关系；`GetInheritanceParent()` 明确采用 Logical → Templated，`GetRoutedParent()` 明确采用 Visual → Logical → Templated，并都有各自的关系变更事件。
- 普通作者子节点同时进入可视树和逻辑树。ControlTemplate 根在视觉上挂到宿主、逻辑父级为空、TemplatedParent 指向宿主；模板内部节点沿模板可视/逻辑结构组织，但共享同一 TemplatedParent。投影内容在视觉上位于 ContentPresenter 下、逻辑上仍属于内容宿主，并且不是模板生成节点。
- ItemsPresenter 生成的 ItemsHost 在视觉上属于 Presenter、逻辑上属于 ItemsControl；生成容器在视觉上属于 ItemsHost、逻辑上属于 ItemsControl。虚拟 ComboBox/TreeView 容器也显式建立同一关系，不再伪造单一父链。
- Measure/Arrange、绘制、命中测试、DComp、UIA 和窗口交互清理只遍历可视树；DataContext、Inherited 属性、资源和样式沿继承上下文；路由事件与 FindAncestor 沿显式路由父级。模板部件查询只接受真实 TemplatedParent，并由宿主维护独立模板 NameScope。
- Materializer 先建立全部模板实例的 TemplatedParent，再安装 TemplateBinding、相对源、事件和 NameScope；Designer 的选择/命中使用可视关系，文档层级和生成顺序使用逻辑关系。热重载拓扑快照同时保存并恢复三种关系，不能用视觉重挂覆盖逻辑所有权。
- CUITest 的 FeatureCard、Button ControlTemplate 和 ListBox/ItemsPresenter 实验会直接断言三种父级、继承父级、路由父级、模板 NameScope、投影内容和生成容器关系，并在界面上显示关键树关系，而不只验证最终像素。
- 集中验收还补齐了 `VisualTransition.GeneratedDuration` 对可插值 VisualState Setter 的生成时钟：进入和退出都使用 Animation 来源，退出目标直接读取低于 VisualState 的有效值快照而不临时改写控件；非可插值 Setter 在过渡完成时切换。
- `Debug|x64` 全解决方案构建成功，核心回归 269/269，Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` 均返回 0，`git diff --check` 通过。

## 17. 第四批实施结果：真实元素职责层级

P0-4 已于 2026-07-21 按不保留空壳基类和旧类型别名的规则落地：

- 建立真实继承链 `DispatcherObject → DependencyObject → Visual → UIElement → FrameworkElement → Control`。每层都拥有自己的状态和行为，不是由 `Control` 继续保存状态、基类只做转发。
- `DispatcherObject` 持有对象创建线程、`CheckAccess/VerifyAccess`、显式异步投递和销毁前失效的生命周期令牌。属性写入、树关系修改、视觉失效等入口会拒绝跨线程直接访问；MediaPlayer 只在平台异步回调边界显式投递回对象 Dispatcher。
- `DependencyObject` 成为 Binding 与依赖属性的真实根：有效值表、表达式槽、声明属性槽、变更版本、通用属性事件和 `BindingCollection` 全部从 `Control` 下沉。`DependencyPropertyRegistry`、`Binding`、`MultiBinding` 和元数据回调统一接受 `DependencyObject`，删除 `BindingProperty*`、`ControlProperty*` 身份和别名。
- `Visual` 持有可视父子、脏区、裁剪、变换和合成状态；`UIElement` 持有布局通道、输入、焦点、命中测试及通用输入事件；`FrameworkElement` 持有逻辑/模板/继承关系、DataContext、资源、样式和布局声明；`Control` 只保留模板、视觉状态、控件行为和控件外观。
- Designer 事件目录从真实 C++ Event 成员推导声明层，公共鼠标/键盘/焦点事件的 Owner 为 `UIElement`，属性变更事件的 Owner 为 `DependencyObject`。生成路由是否使用 `UI_Base` 由基类事件目录的真实契约决定，不再用 `Control` 类名猜测。
- 事件持久化硬切为显式 C++ 处理函数标识符。解析、设计文档、物化、序列化、剪贴板和代码生成均删除 `1/true/yes/on/Auto → 约定函数名` 的布尔兼容链；Designer 双击仍可作为编辑操作生成约定名称，但文件格式不再保存隐式开关。
- 删除全局 `AssertUIThread` / `InvokeOnUIThread` 双入口；删除整数 `MeasureCore(SIZE)` / `Measure(SIZE)` 扩展点。元素测量只保留 float-DIP `Constraints → Size` 主路径，Win32 整数只在平台投影边界出现。
- CUITest 新增完全由 XAML 声明的元素层级实验区；C++ 仅挂接探针事件。界面会展示六层继承链，并实际验证依赖属性 Owner/来源/版本/事件、跨线程拒绝和显式 Dispatcher 投递。
- `Debug|x64` 全解决方案构建成功，核心回归 270/270；Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` 均返回 0。

## 18. 第五批实施结果：布局单路径与 Canvas 语义硬切

布局硬切已于 2026-07-21 完成，没有保留旧引擎、Anchor 或整数桥接：

- `LayoutEngine` 只剩 `Measure(LayoutContext&, const Constraints&) -> Size` 和 `Arrange(LayoutContext&, Rect)` 两个纯虚入口。删除 `Control* + SIZE/D2D1_RECT_F` 重载、默认桥接和 `LegacyContainer()`；Grid、Dock、Stack、Wrap、Relative、ItemsControl 虚拟化布局、Panel、ScrollView 与 Form 全部使用同一浮点 DIP 契约。
- Form 不再构造伪 `Control` 根适配器。窗口根布局以显式非拥有子项 span、宿主 Form 和 `IsWindowRoot` 构造 `LayoutContext`；`Owner()` 在窗口根可以为空，布局引擎不得依赖虚构控件身份。
- 新增唯一 `CanvasLayout` 算法，删除 `LegacyCanvasLayout`、`LegacyCanvasAdapter` 与 `compat`。普通 Panel、Form 根和未安装其他引擎的 ScrollView 均执行同一 Canvas 测量/排列：子项无界测量、不贡献容器 DesiredSize、不参与 Stretch/Alignment，Left 优先于 Right，Top 优先于 Bottom。
- `Canvas.Left`、`Canvas.Top`、`Canvas.Right`、`Canvas.Bottom` 成为规范 float-DIP 依赖属性，默认值为未设置的 NaN，具有父级 Arrange 失效语义。XAML Parser、Serializer、Materializer、Runtime 热重载、Designer 属性目录、拖放/复制和代码生成只使用这四个名称，不再注册或解释 `Left`/`Top` XAML 属性别名。
- 删除 WinForms `AnchorStyles`、`AnchorPickerPopup`、PropertyGrid Anchor 编辑器、设计器 Anchor 包装属性、placement/snapshot/runtime 字段、XAML 解析/序列化和代码生成分支。设计器设置显式位置时直接写 Canvas 左上坐标并清除 Right/Bottom，不模拟锚定伸缩。
- 剪贴板偏移保留 Canvas 小数坐标，并对非数字、非有限值和 float 溢出事务性失败；不再用整数解析悄悄截断附加属性。
- CUITest 布局页新增完全由 XAML 声明的 Canvas 语义探针，实际验证父级 Right/Top、子级 Left/Top 与 Right/Bottom、四边同时存在时的优先级、Margin 和 fractional DIP；C++ 仅做运行时断言。
- `Debug|x64` 全解决方案构建成功，核心回归 269/269，Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` 均返回 0，`git diff --check` 通过。

## 19. 第六批入口（已完成）

下一批收口 P0-5 Materializer 与规范文档边界：

1. 把当前位于 `DesignerModel`、同时被 Runtime/Parser/Serializer 调用的物化器迁到中立 Runtime 边界；Designer 只通过同一入口传入预览工厂和设计期选项，不再直接创建探针控件解释运行时契约。
2. 删除旧 `Extra` 字段向 metadata 晋升、旧事件/属性快照升级等只为历史文档存在的读取分支，以规范 XAML + 不可变 Schema 作为唯一输入。
3. 明确内建 Panel 类型与 XAML `Canvas` 类型的 Schema/行为映射，使 Canvas 名称本身也由 XAML 类型系统表达；C++ 仅提供共享布局 behavior，不能反向注册应用类型。
4. 让 Runtime 与 Designer 的物化错误都引用同一 Schema 成员和源位置，删除 Designer 探针工厂、控件类型猜测及重复属性目录的剩余入口。

## 20. 第六批实施结果：Materializer、XAML 类型身份与文档边界

P0-5 的对象构建和文档输入主链已于 2026-07-21 完成硬切：

- 原 `DesignerModel::DesignDocumentMaterializer` 已删除；唯一对象构建入口迁到 Runtime 中立边界
  `CuiRuntime::XamlObjectMaterializer`。生产控件创建和内建 XAML 类型映射集中在
  `CuiRuntime::XamlRuntimeSchema`，Designer 只能向同一入口提供设计安全工厂和预览选项。
- `DesignNode`、`DesignerControl` 和需要精确类型选择的 Style 都保存权威 `RuntimeTypeId`。解析、规范写回、
  物化、样式选择和 FindAncestor 不再从 `UIClass` 反猜元素名；共享一个 `UI_Panel` 行为宿主的 `<Panel>` 与
  `<Canvas>` 仍是不同 XAML 类型，Canvas attached properties 由 Schema 声明。
- C++ 没有获得 XAML 类型注册入口。内建表只描述框架元素到原生 behavior host 的映射；应用组件仍全部由
  XAML `ComponentDefinition` 定义属性、事件、内容槽和模板，C++ 只按 QName 挂接 Behavior。
- `Extra` 不再是标量或公开对象属性的兼容袋。普通属性以及 Foreground、Clip、RenderTransform、
  RenderTransformOrigin 在该批先归一到 Schema metadata，随后已由第九批迁入 `DesignNodeProperties`；
  结构集合进入强类型 `DesignNodeStructure`，`DesignNode::Extra` 已彻底删除。Materializer 和内部快照会
  明确拒绝历史 `Extra` / `Props` 状态。
- Runtime 公开文本/文件输入只保留规范 XAML：`LoadXaml*` / `ReloadXaml*`。旧 XML 文本、文件和
  `*IntoForm` 入口已删除。Designer XML 仅作为内部快照，读取器只接受当前版本；该批曾提升到 v31，
  当前契约已由第十四批替换为 v34，所有更早版本都直接拒绝，不再保留升级链。
- CUITest 的 Canvas 实验使用真实 `<Canvas>`，同时验证 QName 身份、Panel behavior host、四个 attached
  properties、布局优先级和 Style 精确匹配；核心回归另覆盖错误 QName/native 映射及 Legacy Extra 拒绝。
- `Debug|x64` 全解决方案构建 0 警告/0 错误，核心回归 271/271；Designer `--self-test`、CUITest
  `--validate-xaml` / `--smoke-xaml`、动态/静态样例及 `CuiCodeGen 10` 全部返回 0。

## 21. 第七批实施结果：无实例 Schema 验证

P0-5 的 Schema 驱动验证已于 2026-07-21 完成，验证阶段不再为了查询属性而创建临时控件：

- 所有原生依赖属性类型都提供静态 `RegisterDependencyProperties()`；构造函数只通过虚函数桥保证真实对象的
  注册时序。`DependencyPropertyRegistry` 可按 owner 类型闭包直接查询一个属性或完整属性集，不要求
  `DependencyObject` 实例。
- `CuiRuntime::XamlTypePropertySchema` 是 Parser、Serializer、Materializer、Style/Trigger、Template、
  Storyboard 与 Designer 共用的属性视图。它合并原生依赖属性元数据和 XAML
  `DeclarativeTypeDescriptor`，并在构造组件契约时拒绝声明属性/事件/内容成员覆盖原生属性。
- 删除 Parser 的组件 probe、ControlTemplate probe、Style 规则工厂和 Designer 临时控件验证路径。
  `CreateNativeControl` 现在只存在于真正的 Runtime/Designer 物化边界；验证、规范化和往返序列化只消费
  Schema 元数据。
- Schema 阶段执行类型转换、只读性和合法值验证；依赖真实对象状态的 `Coerce` 延后到有效值写入真实目标时
  执行，避免验证临时对象得到错误结果。Brush、Geometry、Transform 等结构化对象同时保留规范对象值和作者
  文本，XAML/XML 往返不再制造占位字面量。
- 声明组件的默认资源按组件定义节点的词法 Style 作用域解析；Materializer 使用和 Parser 相同的可见资源
  规则，局部资源缺失或成员冲突直接失败，不增加全局回退。
- CUITest 在挂载窗口前直接内省 Button 与声明式 `FeatureCard` 的合并 Schema，并在界面显示
  `Schema-first · Button <N> DP · FeatureCard 5 properties`；原有 5 属性、3 事件、2 内容槽以及
  Binding、Template、VisualState 等可执行语义演示继续保留。
- `Debug|x64` 全解决方案构建通过，核心回归 272/272；Designer `--self-test`、CUITest
  `--validate-xaml` / `--smoke-xaml`、动态/静态样例全部返回 0。`CuiCodeGen 10` 连续生成 5/5 文件且
  SHA-256 一致。

## 22. 第八批实施结果：强类型结构文档与 `Extra` 删除

文档结构状态已于 2026-07-21 完成硬切，不再把 XAML 对象树压回通用动态对象：

- `DesignNode` 删除 `Extra`，新增强类型 `DesignNodeStructure`。Grid 行列、TabItem、ComboBox/ListView/
  GridView/PropertyGrid/TreeView/StatusBar/Menu/Navigation/Breadcrumb/Filter/Chart/Report 集合、媒体源、
  Content/Header 文本槽、模板及 Items 资源引用、Split 子槽和 RelativePanel constraints 都有确定的 C++ 类型。
- 模板展开产生的 Component/ControlTemplate owner、part、chain、generated/root 标记进入独立
  `DesignNodeTemplateState`。它是物化期状态，不再和作者声明结构混放，也不写入持久化快照。
- Parser、规范 XAML Serializer、Runtime Materializer、Designer Canvas、资源编辑器、剪贴板、属性面板、
  拓扑重载与撤销内存预算全部改为消费强类型结构。动态 `DesignValue` 只允许作为解析/序列化适配边界的
  短生命周期投影，文档、命令和运行时源模型不保存该投影。
- 热重载复用判断直接比较 `Structure` / `TemplateState`。原样例中仅为触发分支而写入任意键的 topology
  probe 已删除，改用真实 GridDefinition、StackPanel 子节点、控件属性和 ItemsPanelTemplate 资源变化验证
  InPlace、Recomposed、Replaced、宿主拒绝及事务回滚。
- 该批把 Designer 内部快照升至 current-only v31 并引入规范 `<structure>` 字段；当前快照已由第九批
  随后由第十批提升为 v33、由第十四批提升为 v34。任意旧版本以及 `<extra>` / 模板 JSON `extra`
  都直接拒绝，不提供迁移、默认值补齐或静默丢弃。
- 集中验收完成 `CuiRuntime`、`CuiDesigner`、`CuiRuntimeSample`、`CUICoreTests` 和 `CUITest` 的
  `Debug|x64` 构建；核心回归 272/272，Designer `--self-test`、RuntimeSample、CUITest
  `--validate-xaml` / `--smoke-xaml` 全部返回 0。

## 23. 第九批入口（已完成）

1. 审计并拆除残余通用 `Props`：作者属性应归一为 Schema 成员赋值/依赖属性表达式，布局和公共属性不得再有
   小写专用键、`props.metadata` 与字段并存的双轨；纯设计期状态进入明确的设计期模型。
2. 把 `Bindings` 从按目标属性名索引的动态对象收口成强类型 Binding/MultiBinding 表达式 IR，并让 Parser、
   Serializer、Materializer、Designer 和热重载共同消费；无法由公开 XAML 表达的键直接删除。
3. 统一 Parser → normalized document → Materializer 的源跨度和 Schema 成员诊断，使 Runtime、Designer、
   热重载和代码生成报告同一 QName/属性/事件及行列，不再由各层拼装不同错误。
4. 完成文档表达式地基后进入 P1-1：以 `Window + PlatformWindowHost/HwndSource` 拆除 `Form` 同时承担
   窗口、根元素、消息泵和渲染宿主的职责，并同步清理 WinForms 风格生命周期和命名。

## 24. 第九批实施结果：Schema 作者属性 IR 与 `Props` 删除

作者属性文档已于 2026-07-21 完成硬切；`DesignNode` 不再保存可混入任意设计状态的动态属性袋：

- 删除 `DesignNode::Props`，新增 `DesignPropertyAssignment` 与 `DesignNodeProperties`。每个赋值明确保存
  `DesignerStyleValue`、`StaticResource` 键或 `DynamicResource` 键；Style 资源键和 Class 集合分别进入
  `StyleResourceKey` / `StyleClasses`，不能再伪装成小写普通属性。该批曾以大小写不敏感顺序同时承担查找；
  第三十批已将身份查找/唯一性收紧为精确比较，稳定展示/序列化排序不再决定相等性。
- 属性名仍由 XAML Schema 决定，而不是由 C++ 枚举封闭。这使 XAML 声明组件可以拥有文档中此前未知的成员；
  但每个成员值都必须具有确定 kind、规范文本或结构对象，并且不能携带与属性赋值无关的任意 JSON 状态。
- Parser 直接生成规范 Schema 成员名和强类型赋值；Serializer 直接消费同一 IR。`Width`/`Height`、Canvas
  attached properties、Foreground、Clip、RenderTransform、RenderTransformOrigin 等不再同时存在公开名称、
  小写专用键和 `metadata` 子袋三条路径。
- `CuiRuntime::XamlObjectMaterializer` 删除整段小写键解释与控件专用回退，按 Schema 的 Design 顺序统一写入
  typed assignment；静态/动态资源表达式和结构对象也走同一成员应用入口。未知成员、类型错误或资源表达式
  冲突在候选对象提交前失败。
- Designer Canvas 捕获、属性编辑跟踪、布局放置、资源重命名、剪贴板偏移/闭包、撤销内存估算和代码生成
  全部使用 `DesignNodeProperties`。热重载直接比较 typed assignment，并只对真实依赖属性执行原位更新；不再
  扫描“受支持 Legacy key”白名单来猜测变化能否复用实例。
- 该批内部快照升至 v32，唯一节点字段为 `<properties>`；模板快照使用 `properties`。旧 `<props>`、
  JSON `props`、v31 及更早版本直接拒绝，不迁移、不补默认值，也不为旧测试保留读取分支。
- 当时仍由原生字体对象承载的 `FontFamily` / `FontSize` 已改用 typed assignment 保存，但 Materializer 中保留
  一个明确的字体应用边界；该债务已由第十一批以可继承 Typography 依赖属性消除，历史旁路没有保留。
- `Debug|x64` 全解决方案构建成功；核心回归 272/272，Designer `--self-test`、CUITest
  `--validate-xaml` / `--smoke-xaml`、动态/静态样例和 `CuiCodeGen 10 --version` 全部返回 0；全局 C++
  搜索只剩快照读取器对 removed `props` 的显式拒绝。

## 25. 第十批实施结果：强类型 Binding/Event IR 与 v33 快照

- `DesignNode::Bindings` 已由动态对象替换为 typed `DesignBindingMap`；每个目标成员直接保存
  `DesignerDataBinding`，普通 Binding 与 MultiBinding 共用一个确定模型，子 Binding、Mode、UpdateSourceTrigger、
  Converter/Parameter、StringFormat、Fallback/TargetNull、ElementName 和三类 RelativeSource 不再由各层读取字段名。
  该批最初沿用的大小写不敏感比较已在第三十批删除，binding target/path 身份现在精确匹配。
- `DesignNode::Events` 已替换为 `DesignEventHandlerMap`，值只能是显式宽字符串处理函数名。事件身份仍由 XAML
  Schema/组件契约决定，C++ 只验证签名、连接处理函数或 Behavior；旧的布尔/任意 JSON 值形态没有兼容入口。
- XAML Parser 与 Serializer、Runtime Materializer、Designer 捕获、DataContext/资源类型推导、模板 namescope
  重写、剪贴板依赖闭包与身份重映射、事件索引/重命名、热重载和样例全部直接消费 typed IR。动态
  `VisitLeafBindingDefinitions(DesignValue)` 重载已删除，仅保留 const/mutable `DesignerDataBinding` 遍历。
- 内部快照升至 current-only v33。节点及组件模板统一使用 `bindings.values` 和 `events.handlers` wrapper；
  Binding 读回后必须重新编码为完全相同的规范表达式。旧直挂对象、未知字段、非规范 Binding 和非字符串事件值
  均直接拒绝，v32 及更早快照仍按版本门禁拒绝，不提供升级链。
- 核心回归新增 typed adapter 门禁，覆盖普通 Binding、MultiBinding、事件往返以及上述拒绝路径。`Debug|x64`
  全解决方案构建成功，核心回归 273/273；Designer `--self-test`、CUITest `--validate-xaml` /
  `--smoke-xaml`、CuiRuntimeSample、CuiStaticGeneratedSample 与 `CuiCodeGen 10 --version` 全部返回 0。

## 26. 第十一批实施结果：统一源码跨度、Typography 与平台窗口宿主

- normalized document 新增瞬态 `XamlSourceSpan` / `DesignNodeSourceInfo` / `XamlDocumentSourceMap`。Parser 在
  规范化 QName 和成员名的同时保存元素、完整属性及资源/Style/Template/组件符号位置；偏移与长度使用 UTF-16，
  行列遵守编辑器的 Unicode 标量规则。源码位置不进入 current-only v34 快照，也不参与语义相等比较。
- `XamlDocumentDiagnostic` 成为 Parse、Materialization、Runtime、Designer Preview 与 CodeGeneration 共用的
  单一诊断载体，统一报告 Stage、QName、Member、起止行列和 UTF-16 范围。Materializer 失败不再只返回一段文本；
  RuntimeDocument、DesignerCanvas 和生成输入构建器会沿同一 span 回到作者 XAML。
- `FontFamily` 与 `FontSize` 已从 Parser、Materializer、Designer 包装器和代码生成特判中移除，成为 `Control`
  上真实的可继承依赖属性，完整参与 Default/Inherited/Theme/Style/Template/VisualState/Local/Animation 优先级、
  Binding、StaticResource/DynamicResource、Coerce、Measure 与 Render 失效。为对齐 WPF，公开 `FontSize` 使用
  `double`；仅在最终原生 `Font` 投影边界显式降为 `float`。
- 新增 `PlatformWindowHost`，只负责 Win32 类注册、HWND 生命周期和原生消息转发；`GWLP_USERDATA` 只存在于该
  平台边界。`Application::Run()` 接管进程 Dispatcher 消息泵；公开的同步排空消息入口已在后续审计中删除，
  `Form::DoEvent`、`WaitEvent`、静态窗口过程以及 `Form` 类型/源码均不存在；元素侧唯一窗口身份为 `Window`。
- CUITest 的 WPF 语义页加入可见 Typography 继承、局部覆盖和 DynamicResource 热刷新演示，并从 C++ 验证
  有效值来源及原生字体投影。`Debug|x64` 全解决方案构建成功；核心回归 275/275，Designer `--self-test`、
  CUITest `--validate-xaml` / `--smoke-xaml`、CuiRuntimeSample、CuiStaticGeneratedSample 与
  `CuiCodeGen 10 --version` 全部返回 0。

## 27. 第十二批实施结果：真实 `Window` 与单一 `Content`

P1-1 对象层和文档根语义已于 2026-07-22 完成硬切，不保留 Form 别名、多根文档或宿主兼容层：

- `Window` 成为真实元素并继承 `ContentControl`，具有 `UI_Window` 类型身份和唯一拥有型
  `Content`。旧 `Form.h/.cpp` 及项目入口已删除；原生 HWND、窗口类注册、消息转发只存在于
  `PlatformWindowHost`，不再与元素身份并列。
- XAML 作者文档只允许一个 `<Window>` 及最多一个内容子树。Parser 在规范化阶段保留
  Window 模型而不把它伪装成普通元素节点；Materializer 输出已由 root forest 改为
  `XamlObjectTree::ContentRoot`，第二个顶层节点会直接拒绝。
- `RuntimeDocument` 只保存一个 `unique_ptr<Control> _ownedContentRoot` 和一个非拥有
  `Control* _contentRoot`。公开契约只剩 `ContentRoot()`、`OwnsContentRoot()`、`ReleaseContentRoot()`
  与 `TransferContentRootTo()`；`RootControls` 复数 API 已删除。
- 外部宿主桥收口为 `RuntimeDocumentContentHost`，以单个 `unique_ptr` 执行
  `DetachContent` 及 Initial/Replacement/Rollback 事务。Window 内建适配器、完整替换、稳定 ID
  子树重组、提交拒绝和失败回滚全部沿同一个 Content 槽运作，不再维护根向量与非连续
  根槽位算法。
- 静态生成器对 Window 只生成 `SetVisualContent`；Runtime 动态挂载使用同一 Content 契约。
  代码生成、动态加载、热重载与 Designer Preview 因此不再对顶层树形状作不同解释。
- `RuntimeDocument` 的显式运行时数据源不再作为 Content 根上的临时旁路继承值长期存在。离屏构建只暂存
  环境值；原子挂载、重组和替换会把它事务性投影到真实 `Window.DataContext`，失败时恢复原 Window
  状态。若 Window 的 DataContext 已由 Binding/DynamicResource 表达式占用则明确拒绝覆盖；没有初始
  数据源的普通 Binding 仍安装到稳定 `DataContextSource`，待 Window 继承源出现后自动求值。
- `DataContext` 已成为带 `Inherits` 标志的真实依赖属性。删除 FrameworkElement 的
  `_inheritedDataContext` 旁路字段、专用父子递归和 Default 来源伪装；Local/Inherited/Default 来源、
  Binding 表达式、父级切换和后代通知统一通过有效值引擎。CUITest 会直接断言 Window 为 Local 来源、
  Content 后代为 Inherited 来源，并覆盖 TargetNullValue、列表/键索引和动态源恢复。
- XAML 原生控件工厂创建后会清除构造函数坐标产生的伪 `Canvas.Left/Top` Local 槽。坐标默认值不是作者
  XAML 值，因此不再压过显式 `Canvas.Right/Bottom`；Canvas 四边定位优先级只由附加属性有效值决定。
- Designer 默认文档由真实 Window 包含一个作者 `Canvas` Content。内部用 `ContentControl` 托管客户区，
  避免 Canvas 对子元素“不伸展”的正规语义把 Content 压成零尺寸。Window 整体命中域与客户区
  控件放置域已分开；添加、粘贴、拖动、层级、预览和全选只作用于唯一 Content 子树。
- 相关 `CuiRuntime`、`CUICoreTests`、`CuiDesigner`、`CUITest` 和 `CuiRuntimeSample` 项目均已零警告编译；
  275 项核心回归、Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` 与 RuntimeSample
  集中验证全部返回 0。

## 28. 第十三批实施结果：Style/DataContext 单路径

共享样式在本批彻底退回为无上下文的声明定义，数据环境只存在于元素树：

- 删除 `ControlStyleSheet::SetDataContext`、`DataContext`、`DataContextState` 和样式表级路径订阅。
  `DataTrigger` / `MultiDataTrigger` 只读取每个目标的有效 `DataContext`；无 Context 即不匹配，不再有
  全局回退或一个 DataTemplate 项污染另一个项的可能。
- 数据条件的动态订阅仍由目标控件持有，并以稳定 `DataContextSource()` 观察叶值、中间对象和继承源替换。
  规则/资源变化只广播声明 revision，样式表自身不持有任何业务数据或业务生命周期。
- Runtime 显式源只进入真实 `Window.DataContext`；Designer 设计源只进入内部 `ContentControl`
  预览宿主并由作者 Content 继承。两条路径均不再把源写入文档样式表或局部资源字典。
- 静态生成接口硬切为 `BindData(BindingSourceReference dataContext)`。生成 Window 持有该引用并设置真实
  DataContext；普通 Binding 连接目标代理，目标为 DataContext 的 Binding 连接父级代理。旧裸
  `IBindingSource&` 接口及 StyleSheet 注入代码不保留。
- Designer 文档捕获删除“非线性容器都当 Canvas”的旧回退。只有真正 `Panel/Canvas` 作者父级下的
  非 Default Canvas.Left/Top 才进入规范 XAML；ContentPresenter、Header/Content 槽和模板投影不会再写出
  `NaN` 坐标。
- `Debug|x64` 全解决方案 0 警告/0 错误；275/275 核心回归、Designer `--self-test`、CUITest
  `--validate-xaml` / `--smoke-xaml`、动态/静态样例及 `CuiCodeGen --version` 全部返回 0。

## 29. 第十四批入口（已完成）

1. 删除 normalized document 中 `DesignWindowModel` 作为 Window 属性特殊袋的剩余双轨；让 Window
   与普通元素一样以 XAML Schema 成员、依赖属性有效值及 typed assignment 作为唯一权威状态。
2. 拆分 `ContentControl::ConfigureContentVisual` 中对子元素 Alignment 的强制改写。Stretch 应来自
   `FrameworkElement` 默认元数据，作者显式 Alignment 不应在挂载 Content 时被宿主覆盖。
3. 把 Direct2D/DirectComposition surface、dirty region、设备恢复和场景提交从 Window 语义对象拆到
   compositor/render host；Window 只协调 PresentationSource 生命周期、输入和内容，不实现整棵树绘制。
4. 将 Designer 手写对话框逐步迁为 XAML 声明的 Window/Content，并把 Designer 文档捕获从固定属性
   列表收口到 Schema 驱动的稀疏 local-value 序列化，降低撤销快照内存和重复封装。

## 30. 第十四批实施结果：Window 普通节点与 Content 对齐

Window 的作者状态、序列化、运行时投影和静态生成已于 2026-07-22 完成单路径硬切：

- 删除 `DesignWindowModel` 与专用 Window 属性目录。`DesignDocument::Window` 现在就是
  `UI_Window` / `{urn:cui}Window` 的普通 `DesignNode`；属性、Binding、事件、StyleId/Class 与其他元素
  使用相同 typed IR，`x:Name` 仅保留为名称指令。Designer 绘制所需标量只是从节点投影出的瞬态缓存。
- `Window` 注册自己的运行时依赖属性元数据，包括 Text、位置/尺寸、颜色、字体、可见性、标题栏和窗口行为。
  `XamlRuntimeSchema` 按最派生 owner 优先暴露 Window 元数据，属性面板、Parser、Runtime 热重载和代码生成
  共用该集合；Window 同时自然继承 FrameworkElement/Control 的可用成员。
- `CodeGenInput` 删除二十余个 `Window*` 平行字段，只携带一个 Window 节点；旧长参数 CodeGenerator 构造函数
  已删除。生成器从 Schema 节点投影原生构造参数；随后根 Window 样式/绑定也进入同一路径，生成标识改为 v13，静态样例已重新生成。
- 文档 StyleSheet 现在挂在真实 Window，而不是只挂 Content 根；隐式 `TargetType="Window"`、StyleId/Class
  与继承资源因而使用正常的 Default < Style < Local/Binding 优先级。Runtime 和 v13 静态生成只提交 XAML
  实际书写的 Window Local 值，缺失成员会清除上一版本的 Local 并回落到 Style/Default，不再把 Schema 默认值
  批量投影成伪 Local。
- Window 根 Binding 安装到 `Window::DataBindings`，DataContext 与 ElementName namescope 同时覆盖根 Window
  和 Content 子树。挂载与 in-place reload 会把根属性、样式、DataContext、Window/控件 Binding 作为同一事务
  提交；任一解析或附件失败均恢复旧有效值、样式表和连接。
- 内部 Designer 快照提升为 current-only v34，根改为明确的 `<window type="Window" ...>`，包含规范
  properties/events/bindings。旧 `<form>`、旧版本以及缺失 Window 节点直接拒绝；同时修正了 UI_Window 曾
  因字符串映射遗漏而落成 Base 的问题。
- 控件与 Window 的事件表统一使用 typed `DesignEventHandlerMap`。事件索引、属性面板、撤销命令、
  Runtime 连接与 CodeGen 不再在两种 map 比较器之间复制或转换；第三十批进一步将事件身份收紧为精确比较。
- FrameworkElement/Control 的 HorizontalAlignment、VerticalAlignment 默认值统一为 Stretch；
  `ContentControl` 不再在挂载 Content、ControlTemplate 或 ContentPresenter 替换时重写子元素 Alignment，
  作者 Local 值和低优先级有效值不会被宿主污染。
- `Debug|x64` 全解决方案 0 警告/0 错误；核心回归 275/275，Designer `--self-test`、CUITest
  `--validate-xaml` / 隐藏 `--smoke-xaml`、CuiRuntimeSample 与 CuiStaticGeneratedSample 全部返回 0。

## 31. 第十五批实施结果：Presentation render host 与损伤提交

Presentation 设备所有权和真实局部帧闭环已于 2026-07-22 完成第一阶段硬切：

- 新增 `PresentationRenderHost`，独占一个 native presentation source 的 `HwndGraphics`、
  `DCompLayeredHost`、main/overlay/scene-layer swap chain、当前 `DrawingContext`、DPI/尺寸投影、
  device-loss 恢复和首帧历史。`Window::Render` 只读投影当前 context；Window 不再创建、删除或保存这些资源。
- presentation damage queue 成为框架权威损伤状态，Win32 update region 只负责唤醒/合并。隐藏或遮挡 HWND 的
  内部 `WM_PAINT` 即使得到空 `PAINTSTRUCT::rcPaint`，也能消费 host 中的损伤并提交真实帧；失败会把损伤交还，
  渲染中产生的新损伤留到下一帧，不会被本帧结束错误清除。
- 首帧、Resize、DPI、设备恢复、composition surface 新建及 DComp scene 分段拓扑变化统一提升为完整帧；普通
  后续失效只把物理客户区损伤向外取整为逻辑 DIP。scene layer 必须在 primary `BeginDraw` 前完成规划，帧打开后
  禁止新增 surface。
- DComp layer 的透明清理也受本帧 client dirty clip 约束，脏区外 retained pixels 保持不变；可视顺序、换父、
  Tab 可见页、native/D2D 分段或可见性改变会推进派生 scene revision，并强制完整重建，不能沿用旧 layer 内容。
- `Control::EndRender()` 在成功提交自身绘制后清除上一轮局部失效的合并记录，防止陈旧大矩形把以后的小损伤逐步
  扩成整窗口。`NativeSurface::InvalidateRegion(local DIP)` 为 C++ behavior 提供规范局部失效入口。
- CUITest 新增完全由 XAML 定义的“Presentation/渲染”页和 `PresentationProbe` behavior；隐藏
  `--render-smoke` 会建立真实 DComp device，验证首帧为 full frame、队列完全排空、下一次局部 pulse 保持
  region-only 且确实重绘 scene。临时窗口属性探针和离屏显隐技巧均未保留。

## 32. 第十六批实施结果：Retained PresentationScene 与 Window 去遍历

`Visual → retained presentation node → compositor` 的结构阶段已于 2026-07-22 完成硬切：

- 新增 `PresentationScene`，保存由当前可见 Visual 树派生出的非拥有扁平节点、稳定 preorder、raster roots、
  native-composition 标记及 D2D segment 边界。它不是作者文档、布局树或元素身份，不序列化、不持有控件；
  bounds/transform 不进入结构身份，ancestor clip 与 damage 保持提交时输入。第十七批随后在不重建拓扑的前提下
  为 bounds 加入由 geometry revision 驱动的节点缓存。
- `Window` 中原有 `ComputeDCompSceneTopology`、递归 `RenderDCompControlTree`、Tab/WebBrowser 类型分支、
  segment begin/end 和逐帧 topology hash 已删除。Window 只在布局后同步场景、按需建立 composition、让 scene
  规划 surfaces，并提交 raster/composition 帧；它不再知道每个 D2D 控件段怎样遍历和裁剪。
- retained 快照只在真正的 presentation 结构变化时重建：`VisualChildren` mutation、`Visible`、`ZIndex` 和
  `ForegroundControl` 都使 scene 失效。普通局部 damage、布局尺寸、transform/clip 几何和动画帧不会扫描整棵
  树或推进 revision；结构 revision 改变后，segment 数量即使相同也会使旧帧历史失效，防止像素归属串层。
- `ZIndex` 从无法观察的公开整数改为有 setter 的 Visual 属性；直接 C++ 赋值和 XAML 依赖属性写入现在走同一
  scene-order 失效入口。`ForegroundControl` 同样成为可观察的 Window 投影，不再依靠每帧比较弥补裸字段写入。
- 控件差异不再由 Window 按 `UIClass` 判断。`GetPresentationSurfaceKind()` 让 WebBrowser 声明自己是 native
  composition 节点；`PreparePresentation()` 让 TabControl 在子节点提交前推进可见页动画布局。原
  `IsDCompSceneRenderActive` / `DCompSceneOrderOverride` Win32 后端命名已删除，替换为 presentation 语义 API。
- CUITest 的 XAML Presentation 页现在实时显示 scene revision、retained node 数和 drawing segment 数；局部
  Pulse 验证 revision/节点/分段完全稳定，XAML 蓝色节点的 `ZIndex` 改变验证只重排不增减节点，`Visible=false`
  验证节点脱离与恢复，二者均验证完整帧提升。`Debug|x64` 全解决方案 0 警告/0 错误；核心回归 275/275，
  Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` / `--render-smoke`、动态/静态样例及
  `CuiCodeGen --version` 全部返回 0。

## 33. 第十七批实施结果：四路 presentation revision 与节点更新分类

结构快照之上已经建立独立的内容、几何和仅合成失效协议；结构不再是 scene 唯一能理解的“脏”状态：

- 每个 `Visual` 保存单调递增的 `Content / Geometry / Composition` 本地 revision，`PresentationScene` 同时维护
  三条窗口级诊断 revision。`InvalidateVisual/InvalidateVisualRect` 只声明绘制内容变化；Arrange、Clip、
  RenderTransform/Origin 走几何失效；`InvalidateComposition()` 为 native surface / behavior 提供不改绘制内容的
  retained-resource 重新提交入口。原来含糊的 `InvalidatePresentationScene` 已硬切为只表达拓扑的
  `InvalidatePresentationStructure`。
- retained node 现在保存三路已提交 revision、内容/几何/合成 pending 标记、是否提交过、渲染边界缓存和扁平
  子树范围。scene 以 `Control* -> node index` 定位局部失效；祖先布局/变换/Clip 的几何变化会使对应扁平子树失效，
  不重建节点、不重新分段、不推进结构 revision。一个布局 pass 产生的重叠子树区间会在下一帧合并后只标记一次，
  Window 同期把大量旧/新损伤合并成一次 host/Win32 提交，避免深树 Arrange 退化成重复 scene 扫描和消息风暴。
- 几何变化在修改前后分别排队旧边界与新边界损伤，修正了布局移动可能只刷新新位置、在旧位置遗留像素的问题。
  节点 rendered bounds 只在 geometry revision 变化时重算；祖先 `ClipsChildren()` 的粗裁剪仍在提交时读取实时值，
  以支持 Expander 等不改树结构的动画裁剪。
- native-composition 节点在首次提交后，只有 Content/Geometry/Composition 任一路变化才再次调用 `Update()`；普通
  drawing node 仍遵守 segment 正确性：清除一块共享 surface 后，所有与 damage 相交的节点都必须重放，即使它们
  自身 revision 未变。当前明确把这种工作记为 `DamageReplayNodes`，没有用错误的“未变即跳过”优化制造透明洞。
- `PresentationFrameStatistics` 公开上一帧的三路 dirty node、几何重算、立即绘制、damage replay、native commit
  和 cull 数量。CUITest 的 XAML Presentation 页加入内容局部 Pulse、几何移动、仅合成、完整 replay、结构切换
  五个实验，并实时显示 scene/三路 revision 与帧分类；隐藏 render smoke 对五条路径逐一做自动断言。

`Debug|x64` 全解决方案构建 0 警告/0 错误；核心回归增至 276/276。Designer `--self-test`、CUITest
`--validate-xaml` / `--smoke-xaml` / `--render-smoke`、CuiRuntimeSample、CuiStaticGeneratedSample 与
`CuiCodeGen --version`（13）全部返回 0，`git diff --check` 通过。

## 34. 第十八批实施结果：帧事务、设备代际与 retained command list

立即模式 drawing segment 已经建立可安全复用的设备域命令缓存，surface 生命周期也从若干松散的
`BeginRender/EndRender` 调用硬切为一个显式帧事务：

- `PresentationRenderHost::FrameTransaction` 统一拥有 primary、scene layer 和 overlay surface 的打开、关闭及
  最终提交。scene 不再直接操作 surface 的 Begin/End；primary 关闭、overlay 关闭和 DComp `Commit` 全部成功后
  才算 committed frame。任一 `EndDraw`、`Present`、DComp commit、命令录制或控件绘制异常都会使事务整体 abort，
  清理仍打开的 context、废弃首帧历史并请求一次完整设备恢复，不允许提交半帧。
- `HwndGraphics` 与 `CompositionSwapChainGraphics` 已删除内部悄悄重建 target/device 的旧路径；它们只报告
  `EndDraw/Present/Resize` 结果和 device-lost 状态。真正恢复唯一发生在 `PresentationRenderHost`，恢复后推进单调
  `ResourceGeneration`。Resize、DPI、composition 切换、scene surface 新建/删除同样推进 generation；Window 在
  帧开始前一次同步 generation，统一清空窗口 bitmap、retained command 和 behavior/控件设备资源。
- `InjectPresentationDeviceLossForTesting()` 是明确的故障注入缝，不模拟另一套恢复逻辑：下一帧直接进入与真实
  device-loss 相同的 host 重建、generation、资源通知、full-frame 与命令重录路径。事务统计公开 sequence、
  committed/aborted frame 和 recovery 数量，故障恢复因此可以由 CUITest 稳定断言而不依赖实际驱动重启。
- `D2DGraphics::GetColorBrush/GetBackColorBrush` 不再复用两个会不断 `SetColor` 的共享 brush；同一 device context
  现在按颜色位模式返回稳定且不再修改的 solid brush。渐变、图像和显式 Brush 在单次录制中创建，command list
  持有其资源引用；List/Grid/Navigation/Tree/Tab/RichText 及 Control 自身的 bitmap/brush cache 全部接入
  `NotifyDeviceResourcesInvalidated()`，设备对象地址即使被复用也不会误命中旧资源。
- 每个 DComp drawing segment 拥有与 presentation context 同一 `ID2D1Device` resource domain 的独立 recorder。
  retained node 保存 `ID2D1CommandList + CommandGeneration`：首次提交、Content、Geometry 或 generation 变化时才
  回调 `Control::Update()` 重录；Composition-only 和普通 damage 帧直接 replay。segment 的 dirty clip 和实时
  ancestor clip 仍参与最终提交，因此缓存没有改变局部损伤和叠加正确性。raster fallback 继续走立即递归路径。
- `PresentationFrameStatistics` 新增 transaction/generation、command record/replay/cache-hit/cache-invalidated 数量。
  CUITest XAML Presentation 页增加“注入设备丢失”实验，实时显示事务和缓存统计；原有重叠的 retained tile、native
  surface、内容/几何/仅合成/full replay/结构切换共同证明只有内容与几何触发重录。隐藏 `--render-smoke` 同时验证
  generation 提升、behavior 通知、旧命令失效、完整重录和成功事务提交。

核心回归新增不可变 solid brush 身份以及同设备域 command-list 录制/回放测试，增至 277/277；真实 CUITest
`--validate-xaml` 与 `--render-smoke` 已通过。全量门禁结果记录在 `BUILD_VERIFICATION.md`。

## 35. 第十九批实施结果：统一 InputManager 与 RoutedEvent

Win32 输入、内建控件事件和声明组件路由事件已经收束到同一条 WPF 式路由管线：

- 每个 `Window` 拥有一个 `InputManager`。一条原生鼠标或键盘报告只建立一个 staging scope，并保存稳定 sequence、
  OriginalSource、根坐标、设备类型和已触发阶段；同一报告统一按 Preview tunnel → C++ behavior/message → Bubble
  执行。控件旧代码在 behavior 内再次调用 `OnMouseDown` / `OnKeyDown` 时会进入当前 staging，重复的公开 raise 会被
  抑制；scope 完成时则保证 bubble 阶段至少发生一次。Window 不再维护另一套同名输入 Event。
- `RoutedEventMetadata` 集中保存内建事件身份、routing strategy、preview/bubble 配对、设备和阶段。
  `BuildRoutedEventRoute()` 只从 `RoutedParent` 取得一次循环安全快照；tunnel 反转同一快照，bubble 正向使用。
  `OriginalSource` / `Source` 在整个报告中稳定，`CurrentTarget` 随节点变化，鼠标坐标按当前目标重新投影。
- `RoutedEventArgs`、`MouseEventArgs` 和 `KeyEventArgs` 是整条路由共享的可变状态，已禁止复制；所有处理函数统一接收
  `Control*, Args&`。`Handled` 会跨 Preview/Bubble 保持，默认跳过后续普通 handler；实例 handler 和 framework
  class handler 都可显式选择 `handledEventsToo`。订阅返回 `EventConnection`，连接销毁后精确退订，事件 owner
  销毁后连接不会反向访问悬空对象。
- `UIElement` 统一拥有 mouse/key/focus routed events 及 Preview 对，`RoutedEvent<TArgs>` 仅保留熟悉的
  `OnXxx.Subscribe(...)` / `OnXxx += ...` 门面，实际 invocation 一律进入 `InputManager`。框架 class handler 只允许
 绑定内建 `UIClass` 或 `UI_Base`；它不会成为 C++ 注册 XAML 控件类型的旁路。
- Designer 事件目录、Runtime 注册表、动态 Materializer、静态 CodeGen 和现有 C++ handler 已整体迁移到引用参数。
  XAML 声明组件仍由 XAML 定义事件契约，但其 tunnel/bubble 路由复用同一个 `BuildRoutedEventRoute()`，不再各自
  推导一棵父链。删除 `Utils/include/Event.h` 与 `Utils/src/Event.cpp` 中未使用的 WinForms 式 Event/Keys/
  MouseButtons 副本，不提供 Legacy 兼容层。
- CUITest 的 WPF 页新增三层 XAML 路由实验，展示 Preview 顺序、source 处 `Handled`、外层
  `handledEventsToo`、class/instance handler 次数、route depth 和重复 raise 抑制；核心回归同时覆盖 direct、
  tunnel、bubble、坐标转换、连接生命周期与非复制参数。
- 扩大的演示树稳定触发了 ControlTemplate 展开中的旧悬空引用：向 `DesignDocument::Nodes` 追加模板节点导致 vector
  扩容后仍读取 `owner`。Materializer 现已在追加前捕获稳定值，不再跨 `push_back` 保存元素引用；真实
  `--render-smoke` 已覆盖该路径。

`Debug|x64` 全解决方案构建为 0 警告/0 错误；核心回归增至 278/278。Designer `--self-test`、CUITest
`--validate-xaml` / `--smoke-xaml` / `--render-smoke`、CuiRuntimeSample、CuiStaticGeneratedSample 与
`CuiCodeGen --version` 均返回 0。

## 36. 第二十批实施结果：类型闭包、输入状态与 TextInput

输入管线已经从“事件能够路由”推进到“输入状态也只有一份”：

- `GetUIClassBase()` 成为框架内建类型继承关系的唯一真源，`IsUIClassAssignableFrom()` 与
  `GetUIClassInheritanceDistance()` 供事件、Designer 和后续 Schema 检查共同使用。class handler 不再只匹配精确
  `UIClass`，而是对目标类型的完整基类闭包生效，并稳定按最派生类型到基类执行。同一个继承表不允许再散落到
  Designer 或控件工厂；这只是框架 behavior 的类型关系，仍不允许 C++ 注册 XAML 控件类型。
- 鼠标捕获唯一归 `InputManager` 保存。`Control::CaptureMouse()` / `ReleaseMouseCapture()` 和 Window 门面统一负责
  Win32 capture、`GotMouseCapture` / `LostMouseCapture` 路由、统计、原生捕获丢失以及控件树脱离清理。框架控件、
  Designer 层级拖放和画布平移的直接 `SetCapture` / `ReleaseCapture` 已删除；只有 `InputManager` 本身和 Window
  原生标题栏这一平台交互边界允许直接调用 Win32 capture。
- 旧的公开可写 `Window::Selected` 已直接删除，不保留兼容别名。Window 只保存私有键盘焦点元素，并通过
  `GetKeyboardFocusedElement()` / `SetKeyboardFocus()` 与 `Control::Focus()` 访问；所有切换都由同一路径触发
  `LostFocus` / `GotFocus`、失效和无障碍通知。控件不能再裸写 Window 焦点字段。
- `WM_CHAR` 的已提交文本进入 `PreviewTextInput → C++ behavior → TextInput` staging，整条 route 共享不可复制的
  `TextCompositionEventArgs` 与 `Handled`。旧 `CharInputEvent` / `OnCharInput` 已删除，Designer 事件目录、动态
  Runtime 和静态 CodeGen 直接消费新的 routed event。Tab/Return 等已被键盘策略消费的字符不会重复生成文本报告。
  当前只完成已提交文本；IME composition start/update/end 与候选窗口/文本服务仍是下一批工作，不能把 WM_CHAR
  接入等同于完整 IME。
- CUITest 的 WPF 语义页现在可见演示 Preview/TextInput 顺序、Got/Lost focus、Got/Lost mouse capture、class
  handler 基类闭包和输入统计；启动自检还会直接构造 capture/focus/text route，避免演示仅停留在标签文字。

`Debug|x64` 全解决方案构建通过，核心回归增至 279/279。Designer `--self-test`、CUITest
`--validate-xaml` / `--smoke-xaml` / `--render-smoke`、CuiRuntimeSample、CuiStaticGeneratedSample 与
`CuiCodeGen --version` 均返回 0。

## 37. 第二十一批实施结果：RoutedCommand、CommandBinding 与键鼠 InputBinding

命令已经从菜单 ID、窗口回调和快捷键文本的混合模型硬切为 XAML 作者态 identity 加统一路由执行：

- `RoutedCommand` 只保存 XAML 声明的稳定命令名，不注册 C++ 命令类型。`UIElement` 统一拥有
  `PreviewCanExecute / CanExecute / PreviewExecuted / Executed` 四个 routed event；查询与执行复用现有
  `RoutedParent` 路由、共享参数、`Handled` 和 Preview tunnel → Bubble 语义，`CommandBinding` 只把 C++
  behavior 挂到 XAML 已声明的命令和处理函数名。
- `KeyGesture` / `MouseGesture` 支持规范化修饰键以及按键、单击、双击和中键/滚轮动作；任意 UIElement 可声明
  `KeyBinding` / `MouseBinding`。Window 在输入 Preview 完成后、控件 behavior 之前执行匹配，命令成功即把同一输入
  报告标记为 handled，不会再进入另一条快捷键或默认消息路径。控件局部 InputBinding 沿焦点/命中源的 bubble route
  查找，窗口级 Binding 自然成为最后一级。
- `DesignCommandBinding` 与带明确 `Key/Mouse` kind 的 `DesignInputBinding` 已进入规范 XAML、current-only v35
  内部快照、XML 往返、剪贴板/文档比较、动态 Materializer、Window 原子挂载/热重载和静态 CodeGen。输入绑定先
  完整校验再一次替换，挂载失败恢复原 Window 集合；生成契约提升为 v14，使旧 stamp 必然失效。
- Button、Menu、ContextMenu 统一只保存 `Command + CommandParameter`；菜单显示提示使用
  `InputGestureText`，真正触发只来自同名 InputBinding。旧 `CommandId`、`Shortcut`、`OnMenuCommand`、
  `Window::OnCommand` 以及按整数查找/删除菜单项的 API 已直接删除，不留兼容层；NotifyIcon 当时尚留在平台边界的
  数字菜单投影已在第二十五批彻底收口为瞬时 native id → RoutedCommand，不再公开应用级整数 identity。
- `RoutedCommandManager::RequerySuggested` 提供可观察的显式 requery 通知。CUITest 的 Window 级菜单、
  ContextMenu、Button、Ctrl+O/F5/F1、Ctrl+MiddleClick 以及局部 Ctrl+Shift+P/Alt+RightClick 都由 XAML 声明，
  启动自检直接验证 CanExecute/Executed Preview/Bubble、参数、局部作用域和 requery，而不是只展示文字。
- retained 首帧门禁同时暴露了 ImageBrush 在 command-list recorder 上以 `DXGI_FORMAT_UNKNOWN` 创建兼容目标的
  Direct2D 契约错误；现在显式使用 BGRA8 premultiplied 同设备域中间目标。Presentation smoke 也按节点真实性断言
  `NativeSurface` 的 command-record 分类，不再把 drawing node 错记成 native-composition commit。

## 38. 第二十二批实施结果：FocusManager、逻辑焦点与键盘导航

焦点已经从 Window 中的一个当前指针提升为独立的 WPF 式状态机，并把此前混在一起的两类焦点事件彻底拆开：

- 每个 `Window` 独占一个 `FocusManager`。它保存唯一 keyboard focus、每个 focus scope 的 logical focused element、
  窗口失活时的 suspended target 与 transient popup restore target。`Window` 只是平台投影和公开门面，不再自己维护
  第二份焦点指针、Tab 列表或恢复状态。
- `FocusManager.IsFocusScope`、`KeyboardNavigation.TabNavigation` 与
  `KeyboardNavigation.DirectionalNavigation` 是普通 XAML attached property；`Continue / Once / Cycle / None /
  Contained / Local` 进入 Schema、规范 XAML、Designer、Runtime 和静态生成，不允许 C++ 注册焦点作用域类型。
  嵌套 scope 分别保留 logical focus；离开内部 scope 不会清除其记忆，重新进入时恢复原元素。
- Tab/Shift+Tab 使用 `TabIndex + stable tree order`；Left/Right/Up/Down 使用布局后几何中心、主轴距离和正交距离选择
  邻居。`Cycle` 在边界回绕，`Contained` 阻止越界，`None` 排除整个子树，`Once` 只暴露一个首选后代。方向键先交给
  当前控件 behavior，未处理时才进入通用导航；Window 中原来的可访问角色/ScrollView 类型分派已经删除。
- keyboard focus 切换是一个可取消事务：先按 WPF 顺序路由 `PreviewLostKeyboardFocus`，再路由
  `PreviewGotKeyboardFocus`；任一预览把共享 `KeyboardFocusChangedEventArgs.Handled` 设为 true 即不提交状态。
  提交成功后才发布 `LostKeyboardFocus / GotKeyboardFocus`。树脱离和原生窗口失活属于不可拒绝的平台事实，仍会
  完成 keyboard-focus lost 路由并清除当前键盘目标。
- `GotFocus / LostFocus` 现在只表示 logical focus 映射变化。窗口失活只触发 keyboard-focus lost，保留每个 scope
  的 logical focus，也不会错误触发 LostFocus 或 LostFocus binding 回写；窗口重新激活后从 suspended/logical target
  恢复 keyboard focus。关闭 transient popup 同样恢复打开前目标，脱离树则清除所有悬空 keyboard/logical/popup 引用。
- Designer 事件目录、Runtime 命名处理函数、组件模板事件转发和静态 CodeGen 已公开四个 keyboard-focus routed
  event 及强类型参数；生成契约提升为 v16。CUITest 的 XAML WPF 页声明嵌套 scope、Cycle/Contained、几何方向导航、
  logical/keyboard 事件顺序、预览取消、窗口失活恢复和 popup 恢复，并由启动自检主动执行全部路径。

`Debug|x64` 全解决方案构建 0 警告/0 错误，核心回归 281/281；Designer `--self-test`、CUITest
`--validate-xaml` / `--smoke-xaml` / `--render-smoke`、CuiRuntimeSample、CuiStaticGeneratedSample 与
`CuiCodeGen --version`（16）全部返回 0，`git diff --check` 通过。

## 39. 第二十三批实施结果：Focusable、只读焦点状态与候选资格分层

焦点资格和焦点状态已经从控件类型枚举、Tab 设置及容器自维护字段中彻底拆开：

- `Focusable` 是普通可写依赖属性，`IsTabStop` 只控制 Tab 候选资格。`CanReceiveKeyboardFocus()` 只检查
  `Focusable`、可见/启用状态及视觉祖先；`CanParticipateInTabNavigation()` 才额外检查 `IsTabStop`。因此程序化焦点、
  方向导航与 AccessKey 可以到达 `Focusable=true, IsTabStop=false` 的元素，Tab 顺序不会到达它。
- 内建 XAML 类型描述符保存 `FocusableByDefault`，Materializer 以 Theme 来源投影到真实 behavior host；静态生成代码
  同样写出物化后的值，生成契约提升为 v17。交互类型是否默认可聚焦由 XAML Schema 决定，C++ 不再通过
  `Control::IsKeyboardFocusable()` 或 `UIClass` switch 注册/猜测控件能力；少数 Designer/弹出层内部焦点宿主显式声明
  自身行为资格。
- `IsFocused`、`IsKeyboardFocused`、`IsKeyboardFocusWithin` 成为 `Control` 上统一的只读、瞬态、可观察依赖属性。
  FocusManager 在一次已提交事务内分别投影 logical-focus map、唯一 keyboard target 和 RoutedParent 祖先链；普通作者
  写入、Setter 和 Binding 回写均被拒绝。`Focusable=false` 会以不可取消的 EligibilityChanged 原因清除当前键盘焦点，
  但不会伪造或删除所属 scope 保存的 logical focus。
- Style、Trigger、VisualState 与渲染状态直接消费三种投影：logical focus、exact keyboard focus 与 focus-within 使用
  独立状态位，不再由 `GotFocus`/`LostFocus` 订阅反推。UIA 的 Focused 来自 exact keyboard focus，Focusable 来自
  有效资格；Selection 则只读取真实 `IsSelected`，不再把焦点冒充选择。
- `ItemContainerControl`、Selector、ComboBox 与 TreeView 中的容器私有 `IsKeyboardFocusWithin` 字段、事件和同步调用已
  删除。ListBoxItem、ComboBoxItem、TreeViewItem 模板只观察基类的真实 FocusManager 投影；选择变化不会再制造伪焦点。
- CUITest 的 WPF 页新增 A/B/C/Blocked 焦点实验，直接展示 `IsTabStop=false` 的方向可达与 Tab 排除、
  `Focusable=false` 拒绝、嵌套 logical/keyboard/within 差异以及由只读状态驱动的橙/蓝/绿色 Style。启动自检同时验证
  UIA 快照、预览取消、失活恢复和 popup 恢复；核心回归覆盖 Schema 默认值、只读写保护、Designer 往返与 v17 CodeGen。

`Debug|x64` 全解决方案构建 0 警告/0 错误，核心回归 281/281；Designer `--self-test`、CUITest
`--validate-xaml` / `--smoke-xaml` / `--render-smoke`、CuiRuntimeSample、CuiStaticGeneratedSample 与
`CuiCodeGen --version`（17）全部返回 0，`git diff --check` 通过。

## 40. 第二十四批架构决议：TextCompositionManager 与文本服务单路径

文本输入不再是每个编辑控件各自解释 `WM_CHAR` / `WM_IME_*` 的消息分支，而是 Window 级事务服务产生的
WPF 式 staged input。该批的唯一主路径固定如下：

- `InputManager` 只负责输入报告的路由与 staging：每次接收一份 Start、Update 或 Commit 报告，并让其不可复制的
  `TextCompositionEventArgs` 沿 Preview tunnel → C++ behavior → Bubble 共享，保持 `OriginalSource`、`Source`、
  `CompositionId` 和 `Handled` 稳定。它不读取 IMM，不保存 composition 会话，也不决定候选窗口位置。
- 每个 `Window` 独占一个 `TextCompositionManager`，负责 composition 的 Started → Updated → Completed/Canceled
  状态机、稳定事务 ID、起始 source、预编辑文本、提交文本、system/control text、caret、attribute、clause、
  surrogate pair、`WM_UNICHAR` 以及 IME result echo 抑制。事务状态只在这一处存在，Window 仅负责平台消息入口和
  生命周期转发，控件不得另存一份“是否正在组合”状态。
- 对外严格公开 WPF 对应的六个 routed event：`PreviewTextInputStart` / `TextInputStart`、
  `PreviewTextInputUpdate` / `TextInputUpdate`、`PreviewTextInput` / `TextInput`。Start 与 Update 只报告 composition，
  最后一对表示已提交文本；预览处理可阻止控件默认写入，但不能阻止事务完成清理。Cancel 是 manager 的生命周期
  结果与诊断状态，不增加公开的 `PreviewTextInputCancel` / `TextInputCancel`，避免发明 WPF 不存在的公共事件面。
- Win32/IMM 访问只允许存在于 `TextCompositionManager` 及 Window 的最薄平台投影。`WM_IME_STARTCOMPOSITION`、
  `WM_IME_COMPOSITION`、`WM_IME_ENDCOMPOSITION`、`WM_IME_NOTIFY`、`WM_INPUTLANGCHANGE`、`WM_CHAR` 和
  `WM_UNICHAR` 在此归一化；`GCS_RESULTSTR` 与 `GCS_COMPSTR` 同报时必须分别提交/更新，result 只提交一次，随后
  的 `WM_IME_CHAR` / `WM_CHAR` 回声不得重复编辑。`UNICODE_NOCHAR` 必须确认支持，UTF-16 高低代理项只产生一个
  Unicode 文本报告，失配代理项以明确的 invalid-Unicode 规则处理。
- 正常焦点事务在两个 keyboard-focus preview 均接受之后、真正提交焦点指针之前通知文本服务完成当前 composition；
  若没有可提交结果则以 `FocusChanged` 结束内部事务。窗口失活、source 脱离、资格失效和强制焦点清理属于平台事实，
  直接 cancel 且不能被 routed preview 拒绝。旧 source 在事务开始时捕获，不能因焦点中途变化把 result 写入新控件。
- 被 manager 取消的原生 IME 会话进入 `Tombstoned` 代际：迟到的 result 仅用于登记 echo 抑制，
  update/END 仅记录诊断，都不能重启 composition、取消新的 Programmatic 事务或按新焦点重解析 source。
  只有真实 `WM_IME_STARTCOMPOSITION` 或 `WM_INPUTLANGCHANGE` 开启新原生代际；普通 `WM_CHAR`、公开
  `CompleteComposition` 和 Programmatic composition 不能隐式解封旧会话。
- `Control` / `ComponentBehavior` 只公开两个 text-client hook：`ApplyTextInput(args)` 接收已经规范化的提交结果，
  `TryGetTextInputCaretRect(rect)` 返回 top-level DIP 候选锚点。TextBox、RichTextBox、PasswordBox、NumericUpDown、
  PropertyGridView、GridView、FilterBar、DateTimePicker 与 NativeSurface 均只消费该契约；NativeSurface 的输入 payload
  是 `std::wstring Text`，不能退回单个 `wchar_t Character`。Backspace、Delete、导航和命令仍是 key behavior，不能
  伪装为 TextInput。
- 候选窗口几何只由 manager 查询当前 text client 后投影到 HWND；控件不得调用 IMM，也不得按 `UIClass` 在 Window
  中分派 caret 算法。除 manager 的单一链接边界外，控件源文件中的 `ImmGetContext`、`ImmGetCompositionStringW`、
  `ImmSetCompositionWindow`、`WM_IME_*` 分支和 `Imm32.lib` pragma 全部直接删除，不保留兼容 helper、转发别名或
  “新管线失败再走旧消息”的回退。
- IMM placement 是 manager 向平台提交的输出，不是新的输入报告。`ImmSetCompositionWindow` /
  `ImmSetCandidateWindow` 可以同步产生 `IMN_SETCOMPOSITIONWINDOW` / `IMN_SETCANDIDATEPOS`；这两类
  placement feedback 只记录诊断，绝不反向调用 `UpdateCaretPosition`。manager 同时以进入门保护整个
  caret 查询与 IMM 提交调用域，使第三方 IME 的同步通知无法形成 native message 自反馈递归。
- 类型和事件身份仍完全由 XAML Schema 定义。Designer 事件目录、动态 Runtime、模板 `{RaiseEvent}` 转发与静态
  CodeGen 只消费上述六个声明事件；C++ 新增 manager 和 behavior hook 不构成控件类型注册，也不能建立第二份事件
  白名单语义。若解析、设计器和 materializer 仍各自维护事件规则，后续必须继续收口到共享 Schema，而不是用兼容
  分支掩盖差异。

CUITest 必须以 XAML 声明一个可见的 TextComposition/IME 实验区，而不是只在 C++ 构造参数后写“已支持”：外层元素
同时监听六个 routed event，TextBox、RichTextBox 与 PasswordBox 作为真实 source，并提供 Start、Update、Commit、
Cancel、UTF-16 surrogate、`WM_UNICHAR`、焦点切换、Preview handled 和 Reset 的确定性探针。界面实时显示 manager
snapshot、统计和事件顺序；PasswordBox 只显示长度/阶段，不得把明文写进 trace。

本模块完成后再统一执行修补与门禁，验收至少证明：Start/Update 不修改控件文本；Commit 只写入一次；六事件顺序、
source、事务 ID、caret/attribute/clause 正确；handled preview 阻止默认编辑但状态完成；focus/detach/deactivate cancel
不遗留会话；surrogate 与 `WM_UNICHAR` 产生完整 Unicode 文本；PasswordBox trace 无明文；全仓除 manager/Window 平台
边界外不存在 `WM_CHAR` / `WM_IME_*`、IMM 查询或逐控件候选窗口实现。整体实现完成前不以零散小测替代该批门禁。

本批集中收口后，`Debug|x64` 全解决方案构建为 0 警告/0 错误，核心回归增至
290/290；Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` / `--render-smoke`、
CuiRuntimeSample、CuiStaticGeneratedSample 与 `CuiCodeGen --version`（18）全部返回 0。CUITest 的 native
文本注入在声明式 TextComposition Tab 实际可见且具备 keyboard focus 时执行，不绕过
WPF 式 `Visible` / `Focusable` 资格。

## 41. 下一批入口

1. 让命令源的 `IsEnabled` 与 requery 建立统一有效值/自动更新协议，并加入 routed command class binding；不能让
   Button、MenuItem 各自订阅全局事件或用类型判断实现同一功能。
2. 给 command/resource cache 加入窗口级预算、节点淘汰与命中率统计，并把 gradient stop、bitmap realization、
   text layout/format 的设备无关 identity 与设备域 realization 明确分层，避免“无限缓存换性能”。
3. 清理仍暴露给 XAML 的旧容器/WinForms 命名和文档兼容入口；WPF 公共 QName、native behavior host 与内部实现名必须
   单向映射，不能继续同时接受旧名称。
4. 继续构建 composition property graph，并增加离屏像素 golden、capture/focus/IME/command 组合矩阵、事务中途
   失败注入和长时间 resize/DPI/device-reset 压测，再据统计决定 raster fallback 是否引入 display-list cache。

## 42. 第二十五批架构决议：命令源有效值、Window requery 域与 class command binding

命令不能只做到“能从 XAML 执行”，还必须像 WPF 一样成为输入、状态和路由共用的一条管线。本批把此前分散在
Button、Menu、ContextMenu、NotifyIcon 和 Runtime 命名事件中的状态与执行入口继续收口，固定以下边界：

- 每个 `Window` 独占一个 `RoutedCommandManager` 和一代一代递增的 requery 域。`RequerySuggested` 不再是
  进程全局广播；同一窗口内的多次失效合并成一个 UI-thread 回调，窗口销毁会让排队回调和 observer 自动失效，
  不允许跨窗口刷新所有命令源，也不允许以静态裸指针换取“方便”。
- 所有 command source 统一按 `显式 CommandTarget → 当前 Window keyboard focus → source` 解析目标。
  Button、MenuItem、InputBinding 和托盘菜单不得各自重写焦点查找。一次执行先冻结 routed route，再让
  CanExecute 与 Executed 复用同一个 transaction id、route id 和 route snapshot；CanExecute 回调中即使移动控件树，
  Executed 也不能跳到另一棵树。
- 冻结 route 不允许退化为一组裸 `Control*`。`ControlWeakReference` 只保存对象 identity 与生命周期 token，
  `Control` 在析构清理任何事件、父子树或 command state 之前先使 token 失效；命令的 class handler、instance
  handler 和 observer 回调每跨过一次用户代码都重新解析并验证 route。显式 `CommandTarget` 已过期时必须失败，
  不能悄悄回退到 keyboard focus/source；返回给调用方的 target 同样在回调后重验并在失效时清空。
- `CommandBinding` 的所有权由 `EventConnection` 唯一表达。`UIElement::AddCommandBinding`、XAML Runtime 热重载和
  静态 CodeGen 都必须保存该连接；清理连接就是撤销 binding，不再保留直接订阅四个 routed event 的第二实现。
  只有 Executed handler 的 binding 按 WPF 默认语义可执行；没有任何匹配 binding 的 InputBinding 不能因
  `ContinueRouting` 默认值而吞掉原始输入。
- class command binding 分成两种明确用途：XAML 组件使用完整 `RuntimeTypeId(namespace + local name)` 精确匹配，
  同一 native Panel behavior host 上的两个组件绝不能互相命中；框架内建行为才允许使用 `UIClass` 继承闭包，且按
  最派生到基类执行。这里注册的是 C++ behavior，不是从 C++ 向 XAML 注册控件类型，控件身份仍只来自 XAML Schema。
- `IsEnabled` 现在是有效值，而不是 command source 在每次 requery 时覆写的本地布尔值。有效结果由本地 XAML/
  Style/Binding 值、routed ancestor 和可选的 command CanExecute predicate 共同决定；predicate 变化通过同一依赖属性、
  Style、焦点资格、输入命中和无障碍状态发布。移除 Command 或 observer 只移除 predicate，永远不能把作者写下的
  `IsEnabled=false` 改回 true。
- 旧 `UIElement::bool Enable` 已删除；`Control` 私有保存作者本地值，公开 getter 返回有效值，持久化、Designer 和
  CodeGen 需要作者态时只能读取 `IsLocallyEnabled()`。visual/logical/templated parent 变化在提交前冻结受影响子树，
  提交后逐节点发布真正发生变化的有效值；任何通知删除或重挂节点都通过弱引用重新验证。Window 还把同一有效值
  投影到 HWND enable state，不再维护另一套原生可用状态。
- Button 先发布 Click，再执行 Command；程序化 `Invoke()` 与鼠标走相同顺序，命令被拒绝时不能伪报成功。
  MenuItem 自身是 command source，子菜单建立真实 logical/routed parent；Menu 和 ContextMenu 只负责布局与弹出，
  不再代表叶子项执行命令。禁用项不能 hover、展开或执行，ContextMenu 的 `PlacementTarget` 是弹出菜单未显式指定
  `CommandTarget` 时的目标。
- 默认/取消按钮不再由 Window 保存易悬空的 `_defaultButton/_cancelButton` 或 C++ 注册 API。`Button.IsDefault` 与
  `Button.IsCancel` 是 XAML 依赖属性；未处理的 Enter/Escape 每次从当前树解析唯一、可见且有效的候选，Space 只激活
  当前有效 Button，不能再对任意 focused Control 调 `Invoke()`。
- NotifyIcon 是平台服务而不是另一套应用命令系统。公开菜单项只保存 Unicode text、RoutedCommand、parameter、
  target、local enabled 和子项；Win32 数字 ID 只在一次 `TrackPopupMenu` 内瞬时分配，选中后立即映射回统一命令事务。
  旧 `ID`、按 ID 查找/启用/改名以及 `OnNotifyIconMenuClick(int)` 全部删除，不保留兼容转发。
- 每个 NotifyIcon 实例独占内部 native identity，初始化只接受有效 Window HWND，并在 owner 关闭时解除注册；
  Explorer 的 `TaskbarCreated` 会按 desired-visible 状态恢复图标，失效的 `NIM_MODIFY` 会回落到一次新的 `NIM_ADD`。
  popup 操作用独立共享状态守住 modal reentrancy，菜单 CanExecute/Executed 回调即使删除 NotifyIcon、关闭 Window 或
  使目标过期也不会继续解引用 service/Control；这些生命周期细节仍不向 XAML 暴露 HWND、callback message 或 icon id。
- 动态 Runtime 先解析并分组 XAML CommandBinding，再原子取得并保存连接；静态 CodeGen 生成同一所有权结构，生成契约
  提升到 v20。`InputBinding.CommandTarget` 仅接受当前 namescope 内的直接 `x:Name` / `{x:Reference ...}`，
  current-only 内部快照提升到 v36；重命名、剪贴板、组件/ControlTemplate 局部名称重写、动态 Runtime 和静态
  CodeGen 使用同一引用语义。CUITest 同时可见演示 QName class binding、native fallback、自动 requery、有效
  IsEnabled、Button、Menu/ContextMenu、显式 CommandTarget、模板本地 InputBinding、默认/取消按钮和托盘
  RoutedCommand，不允许用测试代码直接调用 manager 冒充控件行为。

本批集中实现与硬化完成后，current-only Designer snapshot 为 v36，静态生成契约为 v20。`Debug|x64` 全解决方案
构建 0 警告/0 错误，核心回归 299/299；Designer `--self-test`、CUITest `--validate-xaml` / `--smoke-xaml` /
`--render-smoke`、CuiRuntimeSample、CuiStaticGeneratedSample 与 `CuiCodeGen --version`（20）全部返回 0。
`git diff --check` 返回 0，仅报告工作树既有的 LF→CRLF 提示。

## 43. 后续入口

1. 若为 command route/class-binding lookup 引入缓存，key 必须包含 Window generation、`RuntimeTypeId`/`UIClass`
   闭包以及树/命令 revision，并提供窗口级预算、淘汰和命中率统计；缓存不能强持有 Control，也不能把 requery
   退回进程全局表。资源缓存继续明确 device-independent identity 与 device-domain realization 的分层。
2. 清理仍暴露给 XAML 的旧容器/WinForms 命名和文档兼容入口；WPF QName、native behavior host 与内部实现名保持
   单向映射。
3. 扩展 composition property graph，并加入离屏像素 golden、capture/focus/IME/command 组合矩阵、失败注入及
   resize/DPI/device-reset 长时间压力门禁；矩阵还要覆盖 handler 内删除/换父 source 或 target、显式 target 过期、
   多 Window requery 隔离、祖先 IsEnabled 与三种 parent 切换、popup 回调删除 NotifyIcon、Explorer 重建和 owner 关闭。

## 44. 第二十六批实施结果：authored CommandTarget、单一 IsEnabled 与名称作用域闭环

本批把上一批只完成于 InputBinding 的名称引用扩展为完整的 WPF `ICommandSource.CommandTarget`
作者链，并将依赖属性身份和对象生命周期进一步收口：

- `Button.CommandTarget`、直接 `MenuItem.CommandTarget` 及 `Menu.Items` / `ContextMenu.Items` 中的递归
  `MenuItem.CommandTarget` 均是强类型 authored 引用。Parser 只接受当前 namescope 内的直接
  `x:Name` 或 `{x:Reference ...}`；规范 XAML、current-only 快照、Designer、剪贴板重命名、动态
  Materializer/RuntimeDocument、热重载回滚及静态 CodeGen 消费同一份名称图。Designer 保存
  作者名称和递归菜单稳定 path，不从 native 裸指针反推；重命名会原子改写全部引用，
  删除仍被引用的目标在分离子树前直接拒绝。
- Window 名称可作为合法的显式目标，但名称绝不跨作用域。Window 文档、声明组件模板、
  `ControlTemplate`、`DataTemplate` 和局部对象资源各自建立独立 namescope；模板每次实例化都把局部
  source/target 重映射到该实例，不能引用另一模板实例或外层文档的同名控件。静态生成先完成
  全部对象实例化，再连接 authored target，因而前向 `x:Reference` 不会被构造顺序破坏。
- native `CommandTarget` 依赖属性的 Object 值是 `ControlWeakReference`，不是 `Control*`。Local/
  Style 等有效值槽不得因“成员已经是 weak”而再存一份悬空裸指针；显式 Local 引用过期后
  仍保留“作者明确指定过目标”的值来源身份，执行必须失败，绝不回退到 keyboard focus、
  source 或 `ContextMenu.PlacementTarget`。只有从未指定显式目标时，才允许使用规定的默认目标策略。
- `IsEnabled` 现在是 `Control` 类型闭包中唯一的依赖属性 identity；旧 `Enable` / `Enabled`
  隐式 identity 及 Window 重复注册已删除。作者本地值、祖先约束和 command predicate 在这一 identity
  上合成并发布，Window 只将同一有效值投影到 HWND；XAML/Designer/Style/Binding/CodeGen 不得再
  通过旧别名建立第二个属性状态。
- current-only Designer Schema 因完整 authored `CommandTarget` 图提升到 **v37**，静态生成契约
  提升到 **v21**。新版不读取字符串目标 ID、旧属性别名或跨 namescope 引用，也不为了
  “不炸”保留 Legacy 降级分支。
- 这些改动不改变项目的根约定：控件类型、依赖属性、事件和名称作用域契约都由 XAML
  Schema 定义；C++ 只实现/挂接 command behavior、消息、输入、渲染和平台投影，不向 XAML 注册控件
  类型，也不私下增加第二份属性/事件白名单。

## 45. 第二十七批实施结果：旧公共语义清零与单一 WPF 身份

在继续扩展模板、VisualState 与渲染能力前，本批重新从渲染、属性、自动化和静态生成四条路径反向审计，
并直接删除会形成第二套公共模型的实现。结论不是“旧 API 换个名字”，而是把作者态、运行时身份和平台投影
重新压回一条链路：

- `Control` 不再在模板/内容渲染结束后直接绘制验证框、焦点框或验证提示。旧 `FocusedColor`、
  `FocusBorderColor` 及 `ShowValidation*` 一类控件自带装饰器入口已删除；焦点、验证和交互外观必须由
  依赖属性、Style、ControlTemplate/Adorner 层及 VisualState 表达，renderer 不能在最后一遍偷偷覆盖 XAML。
- 公共外观只保留 WPF 语义的 `Background`、`Foreground`、`BorderBrush` 等 `Brush` 属性。
  `BackColor`、`ForeColor`、`BorderColor` 已硬删除；`BrushKind::None` 明确表示“作者未提供画刷/不绘制”，
  不再伪装成透明黑色。原生 fallback 所需颜色是受保护的 renderer 输入，不能重新注册成 XAML 属性。
- Brush 作者值只有一个规范表示：颜色字符串只是 XAML 输入简写，解析后立即归一为结构化 Brush 对象；
  Serializer、Designer、Runtime 与 CodeGen 不得同时保留一份标量文本和一份对象值。普通 C++ setter 也必须
  进入依赖属性 Local 槽并遵守 Animation/Local/VisualState/Template/Style/Theme/Inherited/Default 优先级，
  不能为“直接赋值方便”旁路有效值系统。高对比度只改变 Theme/fallback，不覆写作者 Local 值。
- 自动化只有 `AutomationProperties` + `AutomationPeer` 一条能力模型。旧 `AccessibleRole`、
  `GetEffectiveAccessibleRole()`、按 `UIClass` 猜测 Pattern，以及并行的
  `IAccessibilityVirtualizedControl`/`AccessibilityVirtualPattern` provider 已删除。真实控件和虚拟子项都由
  peer 声明支持的 `AutomationPattern`；Window/UIA 只消费 peer，不再对具体控件做 capability switch。
  ComboBox 的虚拟化滚动指标直接读取其唯一 `ScrollViewer`，不再维护第二份 item-count/固定高度滚动公式。
- 助记键采用 WPF `AccessText` 语义：`_X` 声明 access key，`__` 显示一个字面下划线。通用可写
  `AccessKey` 依赖属性及 WinForms 风格 `&`/`&&` 解析已删除；C++ 中仍保留的 `ProcessAccessKey`、
  `GetEffectiveAccessKey()` 只是输入行为和 UIA 投影名称，不是第二个作者属性面。
- XAML QName 是动态和静态路径的唯一声明类型身份。Parser 对所有 `Style.TargetType` 都保存完整
  namespace/local-name；Materializer 和静态 CodeGen 都为 Window、控件实例及 Style selector 附加/输出同一
  `RuntimeTypeId`。`UIClass` 只保留为 native behavior host 判别器，不能再被当作 XAML 类型名；旧
  `Panel`/`Label`/`PictureBox` 等 QName 不被兼容接受。生成契约提升为 v23，旧 stamp 必须失效并重生。

审计后仍存在的以下实现不是 Legacy 兼容层，暂时不删除：

- 无模板控件的 native fallback renderer，以及其受保护的 `Renderer*Color`/系统颜色输入。它只在没有可用
  Template/Brush 时保证 native host 可渲染，并始终让位于依赖属性、Style、Template 和 VisualState。
- 内部 `UIClass` behavior host 映射及框架 class-handler 继承闭包。它们服务 C++ 行为复用，不向 XAML 注册类型，
  声明身份始终由 `RuntimeTypeId` 决定。
- Runtime/CodeGen 为自动生成 presenter 写入的 Theme 来源默认值。它们是 XAML 默认主题尚未覆盖完整前的暂时
  fallback，不得进入 Local 槽，也不得成为控件构造器硬编码作者值。

下一阶段从这里继续：先把默认主题迁移为 XAML Style/ControlTemplate/VisualState，再补齐所有 renderer 对非 Solid
Brush 的 realization、AutomationPeer Pattern/虚拟 peer 深度和 Adorner 层。只有默认模板已经覆盖某个 native
fallback 的全部状态后，才删除对应 fallback 分支；不得反过来保留两套长期并行外观模型。

## 46. 第二十八批实施结果：树、宿主、光标与渲染入口的旧语义硬切

在只剩若干 WinForms 风格名称之前，本批继续审计了“名称已经像 WPF、行为仍走旧通道”的半迁移状态。审计确认并
删除了以下第二模型；这些改动均不提供旧接口转发：

- `Cursor` 不再是公开原始字段，而是 `Control` 上可继承的依赖属性，完整参与有效值来源、Schema、Style、Binding、
  Designer 和 XAML 物化。控件的区域命中只实现 Theme/Default 下的 native behavior 建议；Local、Style、Template、
  VisualState、Animation 和 Inherited 的有效值由统一的 `ResolvePointerCursor` 优先解析，因而 TextBox、ComboBox、
  Calendar 等控件不能再绕过作者 Cursor。Designer 拖放/缩放光标是独立 view state，不写回文档属性。
- Visual 树不再公开可变 `VisualChildren` collection。外部只能取得只读 span；插入、移除、清空和重排全部经过
  `AdoptVisualChild` / `DetachVisualChild` / `MoveVisualChild` 等所有权事务，重复挂载直接拒绝。底层
  `ObservableCollection` 仅作为私有通知实现，不再构成第二个可写树 API。
- `ParentWindow` 裸字段改为只读 presentation-source 查询；只有树挂载代码能更新内部宿主。`DesignId` 改为只读
  infrastructure identity，写入仅允许 Parser/Designer/CodeGen 使用窄化的 `DesignIdentityAccess`；它不是运行时
  XAML 依赖属性。`Tag` 则反向补齐为真正的 `BindingValue` 对象依赖属性，不再限制为整数或被测试代码当作旁路字段。
- TextBox、PasswordBox、RichTextBox 和 NumericUpDown 的 selection、caret、文本尺寸和滚动偏移缓存全部私有化；
  外部只使用 `Select`、`CaretIndex`、`SelectionStart/Length` 及只读 offset 语义，不能直接制造互相矛盾的编辑状态。
- Typography 对外只有可继承的 `FontFamily` / `FontSize`。旧的派生类 `Font` 属性投影已删除；DirectWrite `Font*`
  只通过受保护的 `GetRenderFont()` 取得当前帧 realization。Designer 的 tab-order 字体也由 `unique_ptr` 唯一拥有，
  不再手写 `new/delete`。
- `Window.Render` 和公开 `GetPlatformWindowHost()` 已删除。`PresentationRenderHost` 独占设备、surface、当前 frame
  context 和提交事务；native 控件 behavior 只能在受保护的 `GetDrawingContext()` 边界中取得当帧 context，普通
  应用代码不能绕过 retained `PresentationScene` 任意即时绘制。扩展绘制继续使用 `NativeSurfaceRenderContext`，
  HWND 互操作只保留为明确的 Window interop 投影，不把 PlatformWindowHost 暴露为另一种窗口身份。
- StackPanel 的 `Spacing`/内容对齐旁路、GroupStyle 的 HeaderIndent/HeaderSpacing/HeaderHeight 布局常量、公开
  ContentText/HeaderText 字符串捷径、公共 Font 对象所有权和 ContextMenu 的字符串 item bag 均已删除。子级间距、
  对齐、Header/Content、Items、模板和 popup presentation state 只走相应的 WPF 树/属性契约。

CUITest 的 Canvas 探针现由 XAML 声明 `Cursor="Cross"` 和对象 `Tag`，子 TextBlock 验证 Inherited 来源及最终光标
解析；核心回归另验证 TextBox 默认 IBeam 仅在 Cursor 为 Default 时生效，显式 Local Arrow 和祖先 Cross 均能覆盖
控件行为。current-only Designer Schema 为 **v40**，静态生成契约为 **v25**。

本轮后仍存在但不属于 Legacy 双轨的实现：

- `UIClass`、native fallback renderer 和 `Update()` 名称只服务 C++ behavior host；声明类型仍由 QName/
  `RuntimeTypeId` 决定，默认外观仍须继续迁移到 XAML Style/ControlTemplate/VisualState。
- Visual 内部的非拥有 parent 指针、route snapshot 和 renderer 当帧裸指针都有唯一 owner 或弱生命周期边界，且不
  对 XAML/应用公开可写入口；它们是高性能实现细节，不是另一个对象模型。
- XAML 中的 `DesignId` 是 current-only 编辑器/编译器元数据，运行时对象只暴露只读查询；待名称作用域和增量热重载
  能完全用结构 identity 表达后，可再评估是否从作者 XAML 文本中移出，但不能恢复成公共可写属性。
- 当时仍存在的 `PictureBox`、`RadioBox`、`ScrollView`、`GridPanel`、`ShowInTaskBar`、`TopMost` 等 native
  类/API 命名不是并行语义；它们已在后续批次直接重命名或删除，不保留转发别名。

通过本批全量门禁后，下一阶段可以从默认主题/模板、Adorner 和 Brush realization 继续推进；若门禁暴露任何仍能
绕过依赖属性、树事务或 retained presentation 的入口，应先删除该入口，不以兼容层进入下一阶段。

## 47. 第二十九批实施结果：生命周期、输入身份、事件所有者与基础设施边界收口

本批没有把“只剩命名问题”当作结论，而是继续从公开头文件、native message 转换、事件发布权限、生成物 ABI、
Window 诊断入口和 Designer 自身实现反向扫描。确认并硬切了以下仍会形成第二语义面的实现：

- Window 生命周期只保留 `Closing`、`Closed`、`LocationChanged` 和首帧提交后的 `ContentRendered`。关闭取消、
  modal `DialogResult`、Owner 关系和 application window snapshot 走同一 Window 状态机；不存在 `Shown`、
  `ThemeChanged` 或 FormClosing/FormClosed 兼容事件。`WM_SYSCHAR` 只做 AccessText access-key 处理，不再在
  `WM_SYSKEYDOWN` 之后伪造第二次 KeyDown。
- 拖放只走 OLE data object 和共享的 routed `DragEventArgs`，Preview/Bubble 修改同一 `Effects`；旧
  `WM_DROPFILES`、`DropFile`、`DropText` 和文件列表回调没有保留。`SizeChanged` 只发布 old/new `Size` 与
  width/height 变化标记，位置变化不再冒充尺寸变化；`IsVisibleChanged` 来自有效 Visibility 依赖属性转换。
- `Key` 改为真正的 WPF 语义枚举顺序，不再把 Win32 `VK_*` 数值伪装成平台无关 ABI。唯一虚拟键转换位于
  Window 平台输入边界；`InputReport` 保存物理键和可选 system-key 分类，公开 `KeyEventArgs`/`NativeSurface`
  只呈现 `Key == System` + `SystemKey`，重复且可能矛盾的 `IsSystemKey` 已删除。键、ModifierKeys、鼠标变化键、
  单键状态和完整按键快照彼此正交，Windows modifier 与 system key 也按同一规则投影。
- KeyGesture/MouseGesture 静态生成不再写 `static_cast<Key>(116)` 等枚举整数，而是输出 `Key::F5`、
  `MouseAction::RightClick`、`ModifierKeys::Control | ModifierKeys::Shift` 等稳定语义符号。生成契约因此提升到
  **v28**；current-only Designer Schema 为 **v42**，旧 stamp 和旧 snapshot 直接失效，不提供数值 ABI 兼容。
- 普通 `UIElement` 不再公开 WinForms 式通用 `.Click`。每个元素仍有受保护的 routed-handler slot，使祖先能够按
  WPF `AddHandler` 语义参与路由；公开 Click facade 只由 `ButtonBase` 和 `MenuItem` 暴露。
  Designer 目录、Runtime 注册和 CodeGen 同样记录真实事件 owner，不能再生成 `&UIElement::Click`。
- CLR-shaped `Event` 的公开面只允许订阅和 RAII 退订；Raise/Clear 位于单独的 `EventInfrastructure.h`，普通
  `Event.h` 消费者无法伪装 publisher。parent-change observer 也只由 Tree infrastructure 发布。RoutedEvent
  的公开 Raise 仍对应 WPF `UIElement.RaiseEvent`；route engine 的 instance-handler 访问则封闭在
  `RoutedEventInfrastructure.h`，两者不再混为一个任意发布 API。
- Window 的 popup/transient presentation、有效 native 颜色、tab-order 构建、cursor 更新、输入/focus/text
  composition 统计、dirty rect、retained scene revision、frame transaction 计数及 device-loss 注入均已从
  应用可见 Window API 移入 `WindowInfrastructure.h` 的窄桥。`Application::PumpPendingMessages` 因未被框架使用且
  等价于 DoEvents 旁路而直接删除；DPI scale helper 也改为 Window 私有平台实现。进一步反查后，未被任何调用方
  使用的 `Application::ExecutablePath` / `StartupPath` / `ApplicationName` / `LocalUserAppDataPath` /
  `UserAppDataPath` WinForms 式杂项工具，以及未被消费的 `Window::IsShowingAsDialog()` 状态探针也已硬删除；
  文件系统路径不再混进 Application 对象模型，modal 状态只属于 Window 内部状态机。
- Designer 原生属性网格曾额外声明一个没有 owner 的 `ScrollChanged`，发布永远是 no-op；该影子事件和空回调已
  删除。Designer/Runtime/样例中所有普通 Event 发布均使用 infrastructure bridge，避免工具链成为公共权限漏洞。

该批当时的审计结论是：除仍待批量重命名的 native C++ 类/API 外，未发现仍被接受或并行执行的 WinForms/早期
CUI Legacy 路径。后续更深的身份、降级 API 与 attached-property 审计见下一节；以下内容仍是 WPF 能力工作，
不应误判为需要兼容的旧实现：

- `Application` 当前仍是进程 Dispatcher/resource/window snapshot 的静态 C++ service；尚未实现完整 WPF
  `Application.Current` 对象模型，但不存在另一套 Form application loop。
- `UIClass` 仍是 native behavior/class-handler 闭包，声明类型仍唯一来自 XAML QName/`RuntimeTypeId`。
- 无模板控件的 native fallback renderer 仍是临时 Theme realization；它必须继续让位于 XAML
  Style/ControlTemplate/VisualState，不能重新长成公共颜色/绘制模型。
- `Control.BorderThickness` 已使用完整 `Thickness`；其余完整默认模板、Adorner、非 Solid Brush realization、
  Application 对象模型等仍属于下一阶段缺失能力或类型精化。不能以这些缺口为理由恢复旧属性、事件、即时绘制
  或消息泵入口。

下一阶段开始前的硬门禁是：全仓实现中不出现旧输入混合掩码、`IsSystemKey`、旧拖放/生命周期事件、公开
Window 诊断调用、`&UIElement::Click`、普通 Event 直接 Raise/Clear 或同步 DoEvents 入口；动态 Runtime、Designer
和静态生成必须继续消费同一 XAML 类型/属性/事件身份。

## 48. 第三十批实施结果：身份精确化、attached-property 所有权与降级边界收口

本批没有把前一轮“只剩命名”当作最终结论，而是继续从公共头文件、类型层级、Parser/Runtime/Designer/CodeGen
身份比较、VisualState/Storyboard、样式安装和布局附加值反向追踪。进一步确认并删除了以下半迁移语义：

- native 类和文件已直接完成 `PictureBox → Image`、`RadioBox → RadioButton`、`ScrollView → ScrollViewer`、
  `GridPanel → Grid` 的硬重命名；`LinkLabel`、`DataPack` 及 Legacy Canvas adapter/layout 已删除。旧头文件、
  QName、项目项、catalog、parser 分支和生成入口均不保留。
- WPF 类型层级重新成为能力来源：`Panel` 是抽象 XAML 类型且不再携带 Control chrome；`ComboBoxItem`
  继承 `ListBoxItem`；`ToolBar` 继承 `HeaderedItemsControl`；`Popup` 投影为 `FrameworkElement` 并只拥有一个
  `Child`；`Window` 使用 `Title`。Designer 与 serializer 不再用成组 `UIClass` 白名单复制这些能力。
- XAML QName、成员名、依赖属性名、事件名、资源 key、`x:Name`、binding/DataContext 路径、VisualState group/state、
  Storyboard 名称、TargetName/PropertyPath、Style 条件和 Designer 扩展 ID 全部精确匹配。错误大小写不再被
  canonicalize 成正确身份。大小写折叠只允许用于文件系统路径、Designer 的展示搜索/排序、枚举/值 token，
  以及作者显式选择的 `CollectionViewSource.IgnoreCase` 数据比较。
- `Control` 不再公开 `Get/SetStyleResourceKey`、Theme/Document StyleSheet 或 ResourceDictionary 的降级存储 API。
  Parser、Runtime、Designer 和 CodeGen 只能通过窄化的 `StyleAccess` 安装 XAML Style/Resources；这些
  `ControlStyleSheet` 对象是编译/物化 IR，不是第二套应用作者样式系统。静态生成契约提升到 **v30**。
- `CanvasLeft/Top/Right/Bottom`、`GridRow/Column/RowSpan/ColumnSpan` 和 `DockPosition` 不再是 `Control` 的
  公共扁平属性。公开 C++ 面只使用 `Canvas::Get/SetLeft...`、`Grid::Get/SetRow...` 和
  `DockPanel::Get/SetDock`；`Control` 中的 wrapper 只是私有依赖属性 backing。XAML 所有者继续唯一为
  `Canvas.Left`、`Grid.Row`、`DockPanel.Dock`。
- 声明组件的字符串 `AllowedValues` 改为精确值比较，不再把作者字符串当枚举标签折叠；底层 bool 转换只接受
  不区分大小写的 `true/false`，拒绝 `1/0/yes/no/on/off` 和空串。VisualState/Storyboard runtime 也不再对
  property/event/group/state 名做忽略大小写匹配。
- current-only Designer Schema 为 **v43**。生成样例已按 CodeGen **v30** 重生；不存在旧 snapshot、旧 stamp、
  旧 QName、旧 public API 或错误大小写 fallback。

本批审计后仍存在但不属于 Legacy 双轨的内容：

- `UIClass` 只承担 native behavior/class-handler 闭包；声明身份仍唯一来自 QName/`RuntimeTypeId`。
- 无默认模板控件的 native fallback renderer、Theme source 默认值和私有 attached-property backing 是当前
  realization/存储细节，均不构成 XAML 作者 API，且必须让位于有效值、Style、Template 和 VisualState。
- Designer placement/runtime rollback snapshot 中的 `CanvasLeft`、`GridRow` 等字段只是事务状态字段；它们通过
  owner API 读写，不是恢复公开的 `Control.CanvasLeft`。
- Windows 路径比较、文件监听去重、enum/value token 解析和 UI 搜索允许大小写折叠；资源 key、类型、成员、事件、
  名称作用域、绑定路径及状态机身份不得借此放宽。

集中门禁结果：`Debug|x64` 全解决方案构建通过；Designer self-test、CUITest 的 validate/smoke/render smoke、
CuiRuntimeSample、CuiStaticGeneratedSample、CuiCodeGen v30 均返回 0；核心回归 **316/316**。

下一阶段可以正式进入默认 XAML Theme/ControlTemplate/VisualState、Adorner、完整 Brush realization 和
Application 对象模型。后续新增能力必须继续沿 Schema → canonical document → Runtime/Designer/CodeGen →
behavior/renderer 的单向链路实现，不能恢复 public lowering API、Control 扁平附加属性或任何错误大小写兼容。

## 49. 第三十一批实施结果：Generic.xaml 默认主题、共享编译与静态初始化收口

本批正式开始默认外观的 XAML 化，并同时审计了主题在动态 Runtime、Designer 预览和静态生成中的三条消费路径。
目标不是再增加一层 C++ 默认样式，而是让框架与应用作者都只面对同一套 ResourceDictionary、Style、
ControlTemplate、TemplateBinding 和 VisualState 语义。

- `Themes/Generic.xaml` 成为框架默认外观的唯一声明源。构建只把原始 XAML 嵌入二进制，
  `XamlFrameworkTheme` 仅负责解析缓存、Style Theme 槽安装及模板 VisualState 装配；它不注册类型、属性或事件，
  也不在 C++ 中复制一份控件模板。独立 `ResourceDictionary` 根现可由 Parser 直接加载，合并字典与普通 Window
  文档仍使用同一资源/Schema 校验。
- 新增共享 `XamlDocumentCompiler`，在物化前统一完成 canonical document、声明组件和 ControlTemplate 展开。
  动态 Materializer、Designer preview/runtime document 与静态 `DesignCodeGenerationService` 全部消费这份编译结果；
  不再允许设计器看一棵树、运行时展开另一棵树、CodeGen 再自行猜一棵树。
- Theme 与作者 Style 保持两个独立有效值来源，未合并成一张伪 CSS 表。模板选择遵守
  `Local Template > 作者 Style.Template > Theme Style.Template`；作者要替换框架默认模板，必须明确设置
  `Control.Template` 或作者 Style 的 Template Setter。裸的隐式 `ControlTemplate` 资源不越过 Theme Style
  的 Template 值，这一优先级不提供旧行为兼容。
- 首个完整迁移对象为 `Button`。默认 Style、Brush、Thickness、Padding、`CuiButtonTemplate`、
  `PART_Chrome`、`PART_ContentPresenter` 及 `CommonStates` 的 Normal/PointerOver/Pressed/Disabled 全部定义在
  `Generic.xaml`。TemplateBinding 写入 Template 槽，VisualState Setter 写入 VisualState 槽；Local 继续覆盖两者。
  `Button::OnRender` 检测到模板根后不再绘制 native chrome，native fallback 只服务真正无模板的行为宿主。
- 静态生成现在直接降低共享编译后的模板树：模板内部节点是构造局部对象，不泄漏为公开成员、稳定 ControlId 或动态
  reference；模板根、TemplatedParent、part namescope、ContentPresenter/ItemsPresenter 注册、内容逻辑所有者及
  TemplateBinding 都由生成代码显式恢复。作者节点保持 Local 来源，只有模板生成节点写 Template 来源；
  框架主题 VSM 仍从嵌入的同一 `Generic.xaml` 安装。
- 主题接入暴露并删除了静态生成的旧构造生命周期：过去 XAML 在 `*Generated` 基类构造函数中展开，C++ 虚事件和
  `CanExecute` 此时只能分派到空基类实现。现在 generated base 构造函数只建立 native Window，
  用户 code-behind 构造函数体调用幂等 `InitializeComponent()`；模板、事件、命令和初始 VisualState 因而面对已经
  构造完成的派生实例，与 WPF code-behind 生命周期一致。静态生成契约提升到 **v32**，不保留旧构造方式。
- CUITest 新增可见的默认主题、禁用和状态说明，并在动态加载、交互 smoke 与 render smoke 中验证模板部件、
  visual/logical/templated parent、TemplateBinding 表达式、Theme/Template/VisualState/Local 值源，以及
  PointerOver → Pressed → PointerOver → Normal 和 Disabled 的完整状态变化。动态与静态样例也断言同一默认模板。

current-only Designer Schema 保持 **v43**；本批没有改变作者文档 Schema，只提升静态生成契约到 **v32**。
集中门禁结果：`Debug|x64` 全解决方案构建通过；Designer self-test、CUITest validate/smoke/render smoke、
CuiRuntimeSample、CuiStaticGeneratedSample、CuiCodeGen v32 均返回 0；核心回归 **316/316**。

下一批按相同方式扩展 `Generic.xaml`，优先迁移 CheckBox、RadioButton、ProgressBar、Slider 等基础控件；每个控件
只有在默认模板、全部交互状态、内容/部件投影及动态/静态路径均覆盖后，才删除对应 native fallback。Adorner、
非 Solid Brush realization 和 Application 对象模型继续作为并行的 WPF 能力批次，但不得重新引入第二套外观或
构造生命周期。

## 50. 第三十二批实施结果：结构元素职责、依赖属性所有权与 Style 生命周期收口

本批再次从 projected WPF 类型层级反查 C++ behavior-host 继承、依赖属性 wrapper、布局、默认绘制和 Style
Trigger 生命周期。结论是：C++ 为复用实现而形成的继承图可以继续存在，但它必须完全私有，不能让某个 XAML
类型获得 WPF 层级没有声明的属性、布局行为或默认外观。

- projected 层级继续按 WPF 职责硬切：`Panel`、`Decorator`、`ContentPresenter`、`ItemsPresenter`、
  `Popup`、TextBlock behavior host 和 `WebBrowser` 都停在 `FrameworkElement` 边界；`Border : Decorator`；
  `ItemsControl : Control`。`ProgressRing` 的 native class closure 修正为 `RangeBase`。这些类型即使在 C++
  中复用 `Control` 存储，也不能据此获得 Control chrome。
- 所有公共 CLR-shaped 依赖属性 wrapper 都必须等价于 WPF `SetValue`，直接赋值进入 `Local` source。
  `IsEnabled`、`Visibility`、`Window.Topmost`、`Window.ShowInTaskbar`、`LoadingRing.IsActive` 和
  `ProgressRing.ShowPercentage` 已按此收口。只有明确的 framework behavior 才能使用
  `SetCurrentValue` 语义保留表达式。
- `SetPropertyField`/`SetCurrentPropertyField` 在目标 projected 类型没有有效元数据时直接拒绝写入，不再把
  C++ 基类 backing 当作隐藏公共属性。`Control::SetText` 同样不能让任意 behavior host 获得通用 `Text`；
  文本只属于实际 WPF 文本类型，ContentControl 使用 `Content`。
- `Control.BorderThickness` 从标量统一为四边 `Thickness`，measure/arrange/render、Runtime、Designer、
  CodeGen 与测试都消费同一类型。`Border` 删除了自己的 `_borderThicknessValue` 和重复 metadata，直接使用
  唯一有效 backing；不存在“公共 getter 一份、Border renderer 另一份”的双写状态。
- 结构元素不再消费隐藏 chrome：ContentPresenter/ItemsPresenter 不自动解释 Padding，Popup 不拥有
  Background/Padding，WebBrowser 外观由作者用 Border/Template 包装。Panel 在 `Background=None` 时真正透明，
  TextBlock behavior 只绘制有效 Background 并按自身 Padding 放置文本，不再硬画 disabled 白色遮罩。
- ContentControl/ItemsControl 构造器不再安装“透明 SolidColorBrush”伪 Theme 值。`BrushKind::None` 是真正的
  unset/no-paint；需要透明画刷的模板必须在 XAML 中显式创建透明 Brush。
- `Control::Arrange` 将模板根放入完整 final slot，不再由 behavior host 预先扣除 Padding。Padding 属于
  ControlTemplate 的 authored visual tree，通常由 `TemplateBinding` 传给 Border/Presenter；自动扣除会造成
  模板与宿主各做一次 Padding。
- Style 刷新顺序改为先建立并清理 Style base values，再编译/同步 Trigger Storyboard clocks，最后清理不可见
  trigger scope。对象路径动画因此解析 Style Setter 提供的真实 Brush，例如
  `(Control.Background).(SolidColorBrush.Color)`，不再依赖构造器预先制造透明默认对象。
- `PresentationRenderHost` 的设备资源与当前绘制上下文已分离：primary/scene/overlay 资源可跨帧持有，但
  `DrawingContext` 只在对应 frame surface 打开期间可见，surface 关闭、事务提交/终止和设备恢复后立即清空。
  `WM_PAINT` 以 host attached 状态决定能否启动事务，不能再用帧外 context 反向充当 renderer 存活探针。
- `AffectsParentArrange` 现在真正使父 `Panel` 的子元素排列策略失效。`Canvas.Left/Top/Right/Bottom` 等附加属性
  变化会保留有效 Measure 结果、单独置位 Panel arrange policy 并向 Window 调度下一帧，而不是只失效父元素
  自身未变化的 arrange slot，或退化为全量 Measure。retained scene 因此只推进 geometry revision，不误报
  content/composition 或重建 topology。

本批形成的持续门禁：

- projected 类型没有声明的属性，即使 C++ 基类存在同名 backing，公开赋值、Style、Binding 和 XAML 也必须失败；
- 普通公开 DP wrapper 写入后必须报告 `Local` source；
- structural element 的 measure/arrange/render 不得读取不属于它的 Control Padding/Border/Text 状态；
- `BorderThickness` 只有一个 `Thickness` backing；
- `Background=None` 不产生 fill，也不通过透明 Theme 值伪装为“有画刷”；
- Style Setter 必须先于依赖这些对象的 Trigger/Storyboard 物化，Animation source 仍高于 base value；
- 模板根取得完整布局槽，Padding 只在模板树中消费一次。
- frame transaction 外 `GetDrawingContext()` 必须为空；设备/host 可用性由 `PresentationRenderHost` 自身状态判断；
- attached property 标记 `AffectsParentArrange` 时必须实际重排父 Panel，并在 retained scene 中命中 geometry lane。

当前核心回归扩展为 **317/317**。这批清理没有引入 Legacy 兼容分支；剩余 native fallback renderer、
`UIClass` behavior closure 和 platform state 都继续是私有 realization，不得成为第二套 XAML 类型、属性或外观
系统。

## 51. 第三十三批实施结果：运行期场景、帧调度、文本排版与窗口输入事务收口

本批从 CUITest 的真实运行故障反查底层，而没有给五个表象分别添加兼容补丁。最终确认这些问题集中在
retained scene 参与资格、damage 调度、祖先绘制变换、帧外文本资源和平台 chrome 输入边界五组契约：

- 首屏大面积叠画不是 XAML 布局数据同时失效，而是 Menu/MenuItem/ContextMenu 已由宿主手工投影的 header、
  ItemsHost 和 popup 内容又被普通 visual traversal 收入 retained scene，形成同一视觉的两次所有权。
  `Visibility`、逻辑/视觉树所有权和 `ParticipatesInPresentationScene` 现已分离；手工 presenter 只退出 scene
  flattening，不伪装成 Hidden，也不破坏 DataContext、事件路由、命名作用域或布局。Popup placement 同时从
  `OnRender` 移到 `PreparePresentation`，命令录制期间不再反向修改布局。
- retained damage queue 与 HWND paint scheduling 现在是同一事务的两个部分：queue 决定“画什么”，
  `InvalidateRect`/WM_PAINT 决定“何时画”。局部、整帧、设备恢复和 frame-history 失效都必须在入队时安排下一次
  paint，不能等待无关鼠标消息偶然唤醒。隐藏 HWND 本身不保证拥有 Win32 update region，因此无界面门禁直接检查
  host pending damage；窗口可见时再附加检查 OS update region，避免把测试环境限制误当成渲染语义。
- LoadingRing 等 native animation tick 只调用控件级 `InvalidateVisual(Rect)`：推进该节点的 content revision、
  排队局部 damage，但绝不从 WM_TIMER 或鼠标事务中同步 `UpdateWindow`。后续帧在 DComp 后端重录 command list，
  在 raster fallback 后端执行 immediate draw；两者是同一 retained invalidation 的不同 realization，门禁不再
  错把某个后端统计量当成公共语义。这直接消除了按住“容器与图像”页签时 animation paint 与输入 paint 互相重入
  造成的闪烁。
- ScrollViewer offset 是祖先提供给后代的 render transform，不是子控件自身的布局值。offset 改变后，viewport
  只排队局部 visual damage，同时把 ScrollViewer retained subtree 的 geometry/command transform 标脏；拓扑和
  Measure 结果保持不变。CUITest 现用真实水平/垂直 thumb 的 PointerDown/Move/Up 路径验证 offset、远端按钮绝对
  坐标、geometry revision/recompute 及 raster/DComp 提交，而不是只调用 `ScrollTo*` 后检查一个数字。
- 崩溃转储把 TextComposition 问题精确定位到
  `DWrite HitTestTextRange → RichTextBox::UpdateScroll → ApplyTextInput`。文本布局此前错误依赖 frame-only
  DrawingContext，并把零长度 caret range 当成普通 range 查询。RichTextBox 现直接通过长期 DirectWrite factory
  建立/释放 text layout，帧外输入同样可格式化；range 被校验和截断，零长度使用
  `HitTestTextPosition`，TextBox/RichTextBox 对空 metrics 和缺失 layout 均有确定分支。回归会在没有活动 frame
  transaction 时向 RichTextBox 提交中英文和 supplementary Unicode，并检查 text/caret。
- 另一组退出转储落在
  `RuntimeDocument::RemoveDataBindings → Control::GetDataBindings → RuntimeDocumentSession/DemoWindow 析构`。
  Binding target、BindingCollection callback state、MultiBinding state、RuntimeDocument root/installed target
  均改为受控 weak lifetime；文档 bookkeeping 不再假定模板/materialized Control 晚于 session 析构。
- 基础控件页无法关闭的根因位于自定义标题栏与内容输入路由的交界：标题栏 PointerUp 曾先送给被捕获的内容控件，
  内容把事件标为 Handled 后 caption release 根本没有执行；即使执行，同步进入 Closing/模态确认也会在 InputManager
  staging 和 capture 尚未退出时重入消息循环。Window chrome pointer 现不进入应用内容 preview/binding/bubble
  路由，Close 在 release 事务结束后 `PostMessage(WM_CLOSE)`；Closing latch 对异常和取消均能恢复。专项测试故意
  让内容控件保持 capture、让 Window PreviewMouseUp 标记 Handled，仍要求 Close 先异步排队、随后恰好触发一次
  可取消 Closing。

这些调整没有新增任何 C++ 类型/属性/事件注册入口：声明身份仍全部来自 XAML，C++ 只负责 behavior、平台消息、
资源 realization 与渲染提交。`ParticipatesInPresentationScene`、弱引用、Window/Presentation test bridge 都是
框架内部所有权和验证设施，不是第二套作者模型，也没有为旧渲染或旧控件路径保留兼容分支。

集中门禁结果：`Debug|x64` 全解决方案构建通过；Designer self-test、CUITest validate/smoke/render smoke、
CuiRuntimeSample、CuiStaticGeneratedSample 与 CuiCodeGen v32 全部返回 0；核心回归扩展为 **318/318**。
render smoke 覆盖 LoadingRing content-only tick 与 ScrollViewer 双轴真实手势，核心测试覆盖被内容 capture/Handled
干扰的 caption close。最终检查未产生新的 CUITest crash dump，`git diff --check` 通过。

## 52. 第三十四批实施结果：交换链帧历史、Popup 输入所有权与视口裁剪收口

本批继续从真实交互故障反查共同底层契约。Tab 切换延迟、按住页签时旧页与 LoadingRing 交替出现、
TextBox/RichTextBox 首次按住时旧焦点帧回闪，以及进入 WebBrowser 后永久停止刷新，并不是四套控件状态机错误；
它们都暴露了局部 retained redraw、双缓冲历史与 DirectComposition surface 首帧之间没有形成明确协议。

- 所有 HWND 与 DirectComposition surface 均使用 flip/sequential 交换链。局部重绘后调用普通 `Present` 会把整个
  stale back buffer 声明为新帧，使两个 buffer 各自保留不同页签、焦点或动画历史。现在
  `PresentationRenderHost` 把每个 surface 的完整 DIP damage 交给 `D2DGraphics`，后者换算并裁剪为物理像素，
  仅对 `SEQUENTIAL`/`FLIP_SEQUENTIAL` 且已建立历史的局部帧调用 `IDXGISwapChain1::Present1`；完整帧继续使用
  ordinary Present。局部帧必须完整清理并重放 dirty region，DXGI 再负责把上一连贯帧的未修改区域带到当前
  back buffer。
- 每个交换链独立保存 presentation-history 状态。新建、设备恢复或 `ResizeBuffers` 后没有可复用历史，
  `RequiresFullPresentFrame` 会把该 surface 的首帧提升为完整 client clear/replay，并且首帧不携带 dirty rect。
  这不是失败后的兼容回退，而是 flip model 的初始化不变量。此前 WebBrowser 首次建立 DirectComposition scene
  layer 时直接局部 `Present1`，返回 `DXGI_ERROR_INVALID_CALL (0x887A0001)`；宿主随后把事务视为设备失效，因而
  所有后续页面都不再刷新。现在浏览器切入、切出及后续普通页面提交均保持 frame count 前进且 aborted count 不变。
- `PresentationRenderHost::TransactionStatistics` 保留最近一次失败 surface 的 transaction、role、EndDraw HRESULT
  与 Present HRESULT。测试和故障报告不再只能看到“某帧中止”，而能区分 primary/scene/overlay 及提交阶段；
  该统计是内部诊断面，不构成 XAML 作者 API。
- Popup dismissal 与 placement target 的输入事务现已明确分工：指针按下 placement target 时，Window 不把该
  Popup 当作“外部点击”提前关闭；owner control 在同一次 press/release 事务中唯一决定 toggle。由此
  ComboBox 第二次点击不会再经历 PointerDown 关闭、PointerUp 重开的双翻转。
- ListBox/ListView 与 TreeView 的 native viewport fallback 会裁剪生成容器；TabControl fallback 对整个 header
  strip 以及每个 header 分别裁剪。模板化路径仍应由默认 ScrollViewer/模板 visual 提供同样边界，fallback
  clipping 只是当前 renderer 的 WPF 视口 realization，不新增 `Clip` 之外的作者语义。
- CUITest render smoke 新增真实页面序列：普通 HWND 页 → WebBrowser/DirectComposition 页 → 普通布局页，
  同时验证 composition device、页面有效可见性、committed/aborted frame 统计及离开浏览器后的持续刷新。
  核心测试增加生成项视口裁剪和 Popup placement-target 输入所有权门禁。

本批没有为旧 renderer 保留整帧重绘兼容分支，也没有把 WebBrowser 特判写进 Window paint；首帧提升、
局部 Present1 与失败诊断属于所有 swap-chain surface 的统一协议。`Release|x64` 下 CUITest 构建通过，
validate/smoke/render smoke 均返回 0；核心回归扩展为 **324/324**。
