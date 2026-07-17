# CUI 使用指南

## 1. 先建立正确心智模型

`CUI` 不是 `.NET WinForms`，但它的 API 风格和 WinForms 很接近。

- 语言与运行时：`C++20 + Win32 + Direct2D + DirectComposition`
- 平台：仅 `Windows`
- 主窗口类型：`Form`
- 通用控件基类：`Control`
- 容器控件：`Panel`
- 子控件挂载方式：静态 C++ 使用 `AddControl(...)`，动态 UI 使用 CUI XAML + `RuntimeDocumentSession`
- 事件订阅方式：静态 C++ 使用 `OnXxx += lambda`，动态 XAML 使用命名事件注册表
- 窗口循环：`Show()` 后手动调用 `Form::DoEvent()`

必须明确：

- 不要假设存在 `namespace CUI`
- 不要假设存在 `Application::Initialize()` 或 `Application::Run()`
- 不要把它写成 WinForms、WPF、Qt 或 MFC 的语义
- 不要先入为主地假设宿主项目一定是“链接 lib”或“一定是直接嵌源码”

如果文档、历史代码和头文件冲突，以这几类内容为准：

1. `CUITest/main.cpp`
2. `CUITest/DemoWindow.h`
3. `CUITest/DemoWindow.cui.xaml`
4. `CUITest/DemoWindow.cpp`
5. `CUI/include/*.h`

## 2. 和 WinForms 的对应关系

| WinForms 心智模型 | CUI 中的对应物 |
| --- | --- |
| `Form` | `Form` |
| `Control` | `Control` |
| `Panel` | `Panel` |
| `Controls.Add(...)` | `AddControl(...)` |
| `FlowLayoutPanel` 风格布局 | `StackPanel` / `WrapPanel` |
| `TableLayoutPanel` 风格布局 | `GridPanel` |
| `Dock = Top/Left/Fill` | `DockPanel` + `DockPosition` |
| 事件 `Click += ...` | `OnMouseClick += ...` |
| 窗口关闭事件 | `OnClosing` / `OnFormClosing` / `OnFormClosed` |
| `Application.Run(form)` | `form.Show()` + 循环 `Form::DoEvent()` |
| 自定义控件覆写消息/绘制 | 继承控件，必要时覆写 `Update()` / `ProcessMessage()` |

## 3. Agent 先看哪些文件

如果你接手的是一个使用 CUI 的项目，优先阅读：

1. `CUITest/main.cpp`
2. `CUITest/DemoWindow.h`
3. `CUITest/DemoWindow.cui.xaml`
4. `CUITest/DemoWindow.cpp`
5. `CUI/include/Form.h`
6. `CUI/include/Control.h`
7. `CuiRuntime/include/CuiRuntime.h`
8. `CUI/include/Style.h`
9. `CUI/include/Panel.h`
10. `CUI/include/Layout/Layout.h`
11. `CUI/GUI/DefaultProcessMessageConvention.md`

这几处基本覆盖了：入口、消息循环、控件树、布局、事件、主题、自定义控件扩展规则。

## 4. 应用生命周期

一个 CUI 程序的真实入口模型是：

1. 在创建窗口前调用 `Application::EnsureDpiAwareness()`
2. 构造 `Form` 子类
3. 在构造函数里用 `AddControl(...)` 搭建静态控件树，或用 `RuntimeDocumentSession::MountFile(...)`
   把外部 CUI XAML 材质化到窗体
4. 调用 `Show()`
5. 循环调用 `Form::DoEvent()`
6. 当 `Application::Forms.size() == 0` 时退出

最小骨架如下。注意：头文件路径要按宿主工程的组织方式调整，下面只展示 API 形态，不固化集成方式。

```cpp
// include 路径按宿主项目组织方式调整
#include "Form.h"

class MainWindow : public Form
{
public:
    MainWindow() : Form(L"My App", { 100, 100 }, { 900, 600 })
    {
        auto title = AddControl(new Label(L"Hello CUI", 20, 20));
        title->ForeColor = Colors::Black;

        auto button = AddControl(new Button(L"Click", 20, 60, 120, 32));
        button->OnMouseClick += [](Control* sender, MouseEventArgs e)
        {
            (void)e;
            MessageBoxW(sender->ParentForm->Handle, L"Clicked", L"CUI", MB_OK);
        };
    }
};

int main()
{
    Application::EnsureDpiAwareness();

    MainWindow fm;
    fm.Show();

    while (true)
    {
        Form::DoEvent();
        if (Application::Forms.size() == 0)
            break;
    }
    return 0;
}
```

要点：

- `Form` 本身不是放进 `Application::Run()` 的对象，而是显示后进入手动事件循环
- 示例工程使用 `main()`，也可以按宿主工程的入口约定封装，但消息泵语义不变
- `Form` 构造时会兜底处理 DPI；推荐仍显式先调用 `EnsureDpiAwareness()`

### 4.1 线程模型与跨线程回调

CUI 控件具有**线程亲和性**：第一个创建 `Form` 的线程被登记为 UI 线程，控件属性、
布局、失效与事件都应在该线程上访问。框架现在提供了显式的封送设施（`CUI/include/Core/Threading.h`）：

- `cui::InitializeUIThread()`：`Form` 构造时自动调用，登记 UI 线程并建立封送 dispatcher。
- `cui::IsUIThread()` / `cui::GetUIThreadId()`：判断当前线程。
- `cui::AssertUIThread(reason)`：Debug 构建下对跨线程 UI 访问触发断言。
- `cui::PostToUIThread(fn)`：把工作线程的回调**异步**封送到 UI 线程执行（经消息泵驱动）。
- `cui::InvokeOnUIThread(fn)`：已在 UI 线程则**同步立即执行**，否则走 `PostToUIThread`。

封送的回调由 `Form::DoEvent()` / `WaitEvent()` / 模态消息循环在每轮自动排空。

**关键规则**：在工作线程（如 `MediaPlayer` 播放线程、自建的 `std::thread`、线程池）里，
**不要直接读写控件属性或调用控件方法**。应当把这类操作包进 `cui::PostToUIThread(...)`：

```cpp
std::thread([this, label] {
    auto text = ComputeSomething();
    cui::PostToUIThread([this, label, text] {
        label->Text = text;              // 安全：在 UI 线程上执行
        label->InvalidateVisual();
    });
}).detach();
```

框架内部已遵循这一规则：`Control::InvalidateVisualRect` 在非 UI 线程被调用时会自动封送回
UI 线程（并用生命周期令牌防止控件先销毁导致的悬空访问）；`MediaPlayer` 的
`OnStateChanged` / `OnPositionChanged` / `OnMediaOpened` / `OnMediaEnded` / `OnMediaError`
事件已统一封送到 UI 线程 invoke，因此这些事件的处理器可以安全地操作其他控件。

## 5. 控件树怎么搭

### 5.1 顶层控件

给窗口挂控件，用 `Form::AddControl(...)`：

```cpp
auto button = this->AddControl(new Button(L"OK", 20, 20, 100, 30));
```

### 5.2 容器内子控件

给容器挂子控件，用容器自己的 `AddControl(...)`：

```cpp
auto panel = this->AddControl(new Panel(20, 70, 400, 200));
panel->AddControl(new Label(L"In panel", 10, 10));
panel->AddControl(new TextBox(L"text", 10, 40, 160, 26));
```

### 5.3 所有权与生命周期

- CUI 的典型写法是 `Add<T>(...)`，或把 `unique_ptr` 交给 `AddOwned(...)` / `InsertOwned(...)`
- `Control::Children` 是可观察的拥有型集合；通过具体类型直接 insert/erase/Move/Swap 也会同步结构
- 控件树建立后父容器持有子控件，`Parent`、`ParentForm`、继承样式和可访问性会在公开集合通知前补齐
- 分离使用 `DetachControl` / `DetachControlAt` 并接收 `unique_ptr`；销毁使用 Delete/Clear，直接 erase 只分离
- `Form` 对 `Menu`、`ToolBar`、`StatusBar` 还有额外的主引用管理

对于资源：

- `Font` 通过属性设置时，默认按“控件接管所有权”处理
- 图片改为 `BitmapSource` 语义，控件内部会按需建立 D2D 位图缓存
- 若你不想让控件接管 `Font`，使用 `SetFontEx(..., false)`
- 若你要自定义布局引擎，`Panel::SetLayoutEngine(...)` 会接管传入指针并负责释放

## 6. 布局系统怎么理解

CUI 的布局不是单一模型，而是“两套并存”：

### 6.1 默认容器布局

普通 `Panel` 在没有设置布局引擎时，主要依赖这些属性：

- `Location`
- `Size`
- `Margin`
- `Padding`
- `HAlign`
- `VAlign`
- `AnchorStyles`

这更接近 WinForms 的“绝对位置 + 锚定/对齐”思路。

### 6.2 布局引擎容器

当 `Panel` 设置了 `LayoutEngine`，则走标准的 `Measure -> Arrange` 两阶段布局。

仓库内已经提供了这些高频容器：

| 容器 | 适合场景 | 关键 API |
| --- | --- | --- |
| `StackPanel` | 纵向/横向线性堆叠 | `SetOrientation()` `SetSpacing()` `Set*ContentAlignment()` |
| `GridPanel` | 表单、二维网格 | `AddRow()` `AddColumn()` `GridRow/GridColumn` |
| `DockPanel` | 顶部栏、侧栏、填充区 | 子控件设 `DockPosition` |
| `WrapPanel` | 自动换行/换列 | `SetOrientation()` `SetItemWidth()` |
| `RelativePanel` | 约束式相对布局 | `SetConstraints()` |

此外还有两个非常常见的复合容器：

- `ScrollView`：内容超出时提供滚动视口
- `SplitContainer`：双面板 + 可拖动分隔条

### 6.3 布局时 Agent 应怎么选

- 简单页面、像素级摆放：先用 `Panel`
- 垂直表单、工具列：优先 `StackPanel`
- 行列明确：优先 `GridPanel`
- 典型“上中下”“左中右”：优先 `DockPanel`
- 卡片流、图标流：优先 `WrapPanel`
- 控件之间需要“在谁左边/下边/与谁对齐”：用 `RelativePanel`

### 6.4 布局属性

布局引擎会读取 `Control` 上的这些属性：

- `Margin`
- `Padding`
- `GridRow`
- `GridColumn`
- `GridRowSpan`
- `GridColumnSpan`
- `DockPosition`
- `MinSize`
- `MaxSize`
- `LayoutWidth` / `LayoutHeight`：`Length::Auto()` 或精确的浮点 DIP `Length::Fixed(...)`

如果改动这些属性后界面没有马上变化，通常需要等待容器下一帧布局，或显式触发 `InvalidateLayout()` / `PerformLayout()`。

新布局实现应调用 `Measure(cui::core::Constraints)` 并保留 `cui::core::Size`
结果中的小数；`Measure(SIZE)` 是旧代码兼容入口。自定义控件需要内容测量时，
优先覆写浮点版 `MeasureCore(const cui::core::Constraints&)`。

### 6.5 Grid 轨道尺寸

`GridPanel` 行列支持 `GridLength::Pixels(dip)`、`Percent(percent)`、`Auto()` 和
`Star(weight)`。`Percent(50)` 表示有界可用空间的 50%，不是 0.5；对应轴无界时
按 Auto 内容尺寸处理。Auto 会计算跨行/跨列子控件的尺寸缺口，Star 会在
`MinWidth/MaxWidth` 或 `MinHeight/MaxHeight` 约束下重新分配剩余空间。

### 6.6 Stack / Wrap / Dock 细节

- `StackPanel::SetHorizontalContentAlignment()` / `SetVerticalContentAlignment()` 控制整个堆叠内容带在容器中的位置；子控件自己的 `HAlign/VAlign` 继续控制其在内容带内部的位置。主轴上的 `Stretch` 等价于从起点排列，交叉轴上的 `Stretch` 会让内容带占满容器。
- `WrapPanel::ItemWidth/ItemHeight` 是子项内容尺寸；设为正数时会作为子控件测量约束，因此固定宽度下的换行文本能得到正确高度。非正数或非有限值统一恢复为 Auto（`0`）。
- `DockPanel::LastChildFill` 指最后一个可见子控件。Dock 测量会扣除 Margin，并把已经由 Left/Right/Top/Bottom 消耗的两轴空间计入期望尺寸。

这些容器属性均已注册 Binding 元数据，可直接用于 OneWay Binding；因为它们没有公开变更事件，自动 TwoWay Binding 会以 `TargetNotObservable` 拒绝，而不是静默失效。

## 7. 事件模型

事件系统基于 `Event<>`，常见写法是：

```cpp
button->OnMouseClick += [this](Control* sender, MouseEventArgs e)
{
    (void)sender;
    (void)e;
    // handle click
};
```

需要在对象销毁或功能停用时解绑的处理器，应保存 `Subscribe()` 返回的
`EventConnection`：

```cpp
EventConnection connection = button->OnMouseClick.Subscribe(
    [this](Control*, MouseEventArgs e) { HandleClick(e); });
// connection 析构或 connection.Disconnect() 时自动解绑
```

`+=` 注册的是持久处理器，适合处理器与事件发布者同寿命的场景。Binding 的
属性元数据订阅必须使用并返回 `EventConnection`，不要用无法自动解绑的 `+=`。

退订规则（重要）：

- `operator-=` **只支持函数指针**。对 lambda / `std::function` / 函数对象使用 `-=`
  会在**编译期报错**（过去是静默无效的陷阱）。需要退订时必须改用 `Subscribe()`
  拿到的 `EventConnection`。
- 当处理器捕获了"持有该事件的对象"的 `shared_ptr` 时会形成循环引用。此时用
  `SubscribeWeak(...)` 以弱引用订阅，目标销毁后处理器自动不再触发：

```cpp
// 防止 控件->事件->处理器->shared_ptr<控件> 循环引用
button->OnMouseClick.SubscribeWeak(shared_from_this(), &MyPanel::HandleClick);
```

常见控件事件：

- `OnMouseClick`
- `OnMouseDown`
- `OnMouseUp`
- `OnMouseMove`
- `OnMouseWheel`
- `OnChecked`
- `OnSelectionChanged`
- `OnValueChanged`
- `OnTextChanged`
- `OnDropFile`
- `OnDropText`
- `OnGotFocus`
- `OnLostFocus`
- `OnPropertyValueChanged`
- `OnValidationStateChanged`

常见窗口事件：

- `OnClosing`
- `OnFormClosing`
- `OnFormClosed`
- `OnShown`（每个 `Form` 实例首次显示时只触发一次）
- `OnSizeChanged`
- `OnMoved`
- `OnThemeChanged`
- `OnCommand`

其中：

- `OnClosing` 的签名里带 `bool&`，可用于取消关闭
- 菜单、上下文菜单等命令型入口常走 `OnCommand` 或 `OnMenuCommand`
- `sender` 一般就是触发事件的控件本身
- `OnDropFile` 的文件集合类型是 `std::vector<std::wstring>`；事件目录、生成声明和运行时 Event
  必须保持完全一致，不能生成框架中不存在的集合别名
- `OnValidationStateChanged` 的签名只有 `const BindingValidationChangedEventArgs&`，没有 `sender`

### 7.1 Binding 诊断

`DataBindings.Add(...)` 对配置进行即时校验；失败时返回 `nullptr`，通过
`DataBindings.LastError()` 或 `LastErrorMessage()` 查看原因。绑定创建成功后，
可用 `Binding::LastError()` 查看源属性读取、目标写入或数据源已销毁等运行时错误。
自定义可绑定属性通过 `BindingPropertyRegistry::Register<Owner, Value>()` 提供
getter、setter 和可选的 RAII 变更订阅，不需要修改 Binding 核心代码。

注册表同时承载控件属性行为。第五个参数 `ControlPropertyOptions<Owner, Value>` 可指定：

- `DefaultValue`：供 `ResetPropertyValue`、`IsPropertyValueDefault`、Designer 和代码生成使用；
- `Coerce`：返回规范化后的有效值，返回 `std::nullopt` 可拒绝写入；
- `Equals`：为没有 `operator==` 的结构提供精确比较；
- `Changed`：仅在有效值真正改变后调用；
- `Flags`：声明 `AffectsMeasure`、`AffectsArrange`、`AffectsRender`；可主题化公开包装器还应使用
  `TracksLocalValue`，让第一次直接 setter 调用也明确写入 Local 层。
- `Design`：可选的工具层契约，声明 `Browsable` / `BrowsableWhen`、`DisplayName`、`Category`、
  `CategoryOrder` / `Order`、`Editor`、强类型 `Choices`、数值 `Minimum` / `Maximum` / `Step` 和
  `Persistence`。默认值保持旧注册兼容；新属性应主动选择 `Metadata`、`Legacy` 或 `Transient`。

新属性的直接 setter 应调用 `SetPropertyField(propertyName, storage, value)`，这样直接赋值、
Binding 和 `TrySetPropertyValue` 才会共享 Coerce、失效和 `OnPropertyValueChanged`。不要在
Coerce 中修改其他状态。旧的字段型属性可渐进迁移，但新增属性不得再自行复制一套钳制和
脏状态逻辑。

有效值优先级固定为 `Local > Binding > Style > Theme > Default`。使用带
`ControlPropertyValueSource` 的 `TrySetPropertyValue(...)` 写入指定层；
`ClearPropertyValue(...)` 会回退到下一层，`ClearPropertyValues(source)` 用于整体卸载主题或
样式。进入来源体系前的现有字段值会保存为兼容基线，所有来源清除后恢复。隐藏层更新不得
触发有效值事件，但其最新值必须保留。

带 `TracksLocalValue` 的属性包装器公开 setter 表示显式 Local 值。控件内部因鼠标、键盘等交互改变可绑定状态时，
应调用 `SetCurrentPropertyField(...)`；当当前来源是 Binding 时它会更新 Binding 层并保留
TwoWay 连接，之后的源更新仍能生效。Binding 清理时必须先断开目标通知，再释放 Binding 层；
活动 Binding 独占自己的层，普通 `TrySetPropertyValue` / `ClearPropertyValue` 不得覆盖或清除；
同一控件的同一目标属性只能有一个 Binding，无论经 BindingCollection 添加还是直接构造，
重复绑定都返回 `BindingError::DuplicateTargetProperty`。
需要局部替换或卸载时使用 `BindingCollection::Find(targetProperty)` / `Remove(targetProperty)`；
名称比较与 `Add` 一样不区分大小写，`Remove` 只释放该目标拥有的 Binding 层和验证订阅，不得用
`Clear()` 误删其他目标的绑定。

`IBindingSource` 可选实现 `TryGetPropertyMetadata(...)` / `GetProperties()` 来公开源属性
名称、`BindingValueKind`、具体 C++ 类型和 Read/Write/Observe 能力。`ObservableObject`
会在首次 `SetValue(...)` 时自动建立这份元数据；需要只读或不通知属性时使用
`DefineProperty(...)`，派生 ViewModel 可通过受保护的 `SetCurrentValue(...)` 更新只读值。
运行时 Binding 会在元数据存在时校验整条点分路径，并分别报告
`SourceNotReadable`、`SourceNotWritable`、`SourceNotObservable`。

源端验证与值通知是独立能力。自定义 `IBindingSource` 可选实现
`GetValidationIssues(propertyName)`，并从 `ValidationChanged()` 返回
`BindingValidationChangedEvent`；不支持实时通知时返回默认的 `nullptr`，Binding 仍会读取
创建时快照。`BindingValidationIssue` 包含 `Message`、`Severity`（Info/Warning/Error）和
可选稳定 `Code`。空属性名表示对象级问题，其通知会让所有相关绑定刷新。

`ObservableObject` 已实现验证存储与事件。派生 ViewModel 使用受保护的
`SetValidationIssues(...)`、`SetValidationError(...)`、`ClearValidationIssues(...)` 和
`ClearAllValidationIssues()` 更新状态；空消息会清除单字段错误，空白问题和重复问题会被
规范化掉。删除属性时其字段验证也会删除。复制 ViewModel 会复制当前问题，但不会复制
事件订阅。

`Binding::ValidationIssues()` 返回整条点分路径上对象级、中间属性和叶属性问题，并通过
`Binding::ValidationChanged()` 发布变化。中间对象替换时验证订阅与值订阅一起重建；即使
是 `OneTime` / `OneWayToSource`，验证路径也会跟随新对象。控件可使用
`DataBindings.GetValidationResults()` 获取带 Target/Source 上下文的汇总结果，或使用
`HasValidationErrors()` 查询状态。`Control` 会自动按最高严重级别绘制主题化边框，并在
鼠标悬停时由 `Form` 的顶层渲染阶段显示校验摘要；不要在具体控件中重复实现这两层表现。
边框、提示开关和尺寸由 `ShowValidationBorder`、`ShowValidationToolTip`、
`ValidationBorderThickness`、`ValidationCornerRadius`、`ValidationToolTipMaxWidth` 控制，
颜色统一来自 `FormThemeFrame`。状态变化可订阅 `OnValidationStateChanged`。
`AccessibleDescription` 是静态说明，`GetEffectiveAccessibleDescription()` 会追加当前校验
摘要，原生 UIA Provider 会通过 FullDescription 与属性变化事件公开该有效说明。Provider 必须继续
通过稳定 runtime ID 解析当前控件，Form 销毁或控件失效后返回 `UIA_E_ELEMENTNOTAVAILABLE`，不得持有
可越过控件生命周期的裸引用。MSAA 客户区对象仍作为兼容路径保留。

控件若内部绘制逻辑子项而不创建 `Control`，应实现 `IAccessibilityVirtualizedControl`，返回值语义的
`AccessibilityVirtualNode`。ID 必须跟随逻辑项而不是行号/显示索引；复制导致的重复 ID 要在查询时修复，
删除后必须让旧 Provider 解析失败。结构顺序通过 `GetAccessibilityVirtualChildren` 表达，动作只修改对应
数据模型。`ScrollIntoView`/`Realize` 不得暗中选择项目，`AddToSelection` 必须幂等且遵守容器的多选能力，
UIA Fragment 焦点也不得与选择状态混为一谈。集合 API 的增删、交换、展开和排序要发送结构通知；
Invoke、Toggle、ExpandCollapse、Selection 与 Value 使用各自的变化类型，避免错报 Pattern 属性。
虚拟容器可通过 `GetAccessibilityScrollInfo`、`ScrollAccessibility` 和
`SetAccessibilityScrollPercent` 公开 Scroll Pattern。不可滚动轴必须返回百分比 -1、视口 100，百分比输入只
接受 -1 或 0..100；不支持的方向要失败，滚动动作不得改变选择。范围、偏移、视口尺寸或可滚动性变化时发送
`AccessibilityChange::Scroll`。表格型虚拟控件必须同时实现 Grid 与 Table：容器返回行列数和稳定表头，单元格
返回 Row/Column、父行以及对应表头；仅在视觉上真实存在行头时才公开行头关系。
大集合还应覆盖 `GetAccessibilityVirtualChildCount`、`TryGetAccessibilityVirtualChildAt`、
`TryGetAccessibilityVirtualSibling` 与 `TryHitTestAccessibilityVirtualNode`，避免 Provider 回退到整组 ID 复制和
递归节点解析。索引必须由结构通知失效，并在公开观察者运行前恢复；只为实际查询过的二维单元格创建身份，
结构变化时按逻辑行/列 ID 保留仍有效的物化身份。不要用易波动的时间阈值代替复杂度回归：测试应以旧枚举
入口调用次数、边界索引查询、稳定 ID 和删除后解析失败作为确定性证据，再用大数据规模暴露意外的平方级路径。
ListView Details 与 GridView 可用 `MaterializedAccessibilityCellCount()` 验证根/行导航没有意外物化整张表。
ListView 的逐帧绘制必须通过 `GetVisibleItemRange()` 只枚举视口候选项；Icon 命中测试也必须直接映射到
单个行列候选，禁止重新引入对完整 Items 的扫描。
ListView 大批量结构修改优先用 `DeferUpdates()`；尾部追加的
`LastAccessibilityIndexUpdateWork()` 与 `LastSelectionUpdateWork()` 应保持常数级，Move 只允许与移动区间、
前插/删除只允许与受影响后缀相关。`ListViewItem::Selected` 是兼容字段，直接修改后必须调用
`Items.NotifyReset()`；交互 API 和结构集合会自行维护逻辑 ID 选择缓存。
需要采集耗时时设置 `CUI_TEST_TIMINGS=1`；该输出只作为同机同配置趋势基线，硬延迟预算只能放在固定硬件和
固定电源/调试器条件的专用性能任务中，不能让普通开发机速度影响正确性测试。
公开结构集合优先使用 `ObservableCollection<T>`：通过具体集合的 push/insert/erase/Move/SwapIndices/Sort 修改，
批量更新使用 `DeferNotifications()`；若显式转成 `std::vector` 或用迭代器算法原地重排，完成后必须调用
`NotifyReset()`。Control::Children 会在批处理中逐次保持内部 Parent/Form 状态，并在结束时只发布一次 Reset。
拥有型裸指针兼容面不得直接用 vector API 表达销毁语义：TreeNode 优先使用
AddChild/DetachChildAt/RemoveChild/ClearChildren，TabControl 使用 InsertPage/DetachPage/RemovePage/ClearPages，
Menu/MenuItem 使用对应的 Insert/Detach/Remove/Clear API。直接 erase 只表示分离，释放责任转交给调用方。
所有订阅都由 `EventConnection` 自动释放。`DataSourceUpdateMode::OnValidation`
仍只表示目标控件在失焦时回写，不等同于源验证事件。

基础数值转换会检查范围；TwoWay 回写也会保持源属性既有的具体类型。转换失败时
目标或源值保持不变，并分别报告 `TargetConversionFailed` 或
`SourceConversionFailed`；后续合法更新成功后错误会自动清除。

源属性支持点分路径。中间对象必须通过拥有所有权的 `BindingSourceReference`
存入数据源，例如：

```cpp
viewModel.SetValue(L"Profile", BindingSourceReference(profile));
textBox->DataBindings.Add(L"Text", viewModel, L"Profile.DisplayName");
```

路径会监听每一级对象；中间对象替换、暂时缺失后重新出现时都会自动重建订阅。
格式化或单位换算可把 `DelegateBindingValueConverter` 的 `shared_ptr` 作为
`DataBindings.Add(...)` 最后一个参数传入，Binding 会持有转换器生命周期。
需要让设计文件和生成代码引用转换器时，通过
`BindingValueConverterRegistry::Register(...)` 注册名称、源/目标 `BindingValueKind`、
`CanConvertBack` 和工厂，并在调用生成窗体的 `BindData(...)` 前完成注册。注册表内置
`BooleanNegation`、`StringIsNotEmpty`、`StringTrim`；名称查找不区分大小写，
设计器会据目标属性类型筛选并校验能否反向转换。

设计器窗体属性中的 `DataContext Schema` 是可选的设计期契约。每个条目声明点分
源路径、`BindingValueKind` 以及 Read/Write/Observe 能力。Schema 非空时，Binding
编辑器会列出已知路径并严格检查路径存在性、绑定方向、源通知能力和 Converter
的 `SourceKind`；Schema 为空时继续允许自由路径，以兼容旧设计文件。当前版本 3 XML
同时保存 Schema 与文档样式表；版本 1、2 文件仍可加载并在下次保存时升级。
宿主通过 `Designer::SetDesignDataContext(sharedSource)` 连接真实 ViewModel 后，Schema
编辑器的“从运行时源导入”会递归读取 `IBindingSource` 元数据并合并已知路径；
导入有深度上限并检测循环引用，不会在自引用对象图中无限递归。Binding 编辑器还会
读取所选源路径当前的验证问题用于设计期预览；验证问题是运行时瞬时状态，不进入 Schema、
XML 或生成代码。校验呈现配置和 `AccessibleDescription` 则是控件属性，会随设计文档往返，
并由代码生成器输出非默认值。

## 8. 常用控件分类

### 8.1 基础输入与显示

- `Label`
- `LinkLabel`
- `Button`
- `TextBox`
- `RichTextBox`
- `PasswordBox`
- `ComboBox`
- `CheckBox`
- `RadioBox`
- `Switch`
- `Slider`
- `DateTimePicker`

### 8.2 容器与组织

- `Panel`
- `GroupBox`
- `TabControl`
- `TabPage`
- `ScrollView`
- `SplitContainer`

### 8.3 数据与树形

- `TreeView`
- `ListView`
- `ListBox`
- `PropertyGridView`
- `GridView`

`GridView` 常用能力：

- **行级多选**：设 `grid->MultiSelect = true` 后，Ctrl+点击切换单行、Shift+点击从锚点扩展范围；
  程序侧用 `GetSelectedRows()` / `SetRowSelected(row, bool)` / `SelectRowRange(a, b)` / `SelectAllRows()`。
  单选 API（`SelectedRowIndex`/`SelectRow`）在多选下仍表示焦点/锚点行，完全兼容。
- **运行时列管理**：`SetColumnVisible(col, bool)` 隐藏/显示列（保留数据与原始索引），
  `MoveColumn(from, to)` 重排列（各行单元格同步移动，选中/排序列自动重映射）。
- **批量加载**：`SetRows(...)` 原子替换全部行，内部一次批量更新只触发一次重排/重绘，
  远比逐行 `AddRow` 高效；配合 `DeferUpdates()` 可进一步合并多次结构变更。

### 8.4 媒体与内容嵌入

- `PictureBox`
- `WebBrowser`
- `MediaPlayer`

### 8.5 系统集成

- `Menu`
- `ContextMenu`
- `ToolBar`
- `StatusBar`
- `ToastHost`
- `MessageDialog`
- `NotifyIcon`
- `Taskbar`
- `ToolTip`

如果 Agent 不知道某个控件如何声明或布局，先在 `CUITest/DemoWindow.cui.xaml` 搜控件名；
若要看集合数据、图表、Web/媒体或系统服务初始化，再查 `CUITest/DemoWindow.cpp`。

## 9. 主题、颜色与外观

`Form` 窗框级主题入口仍是：

- `ApplyThemeFrame(...)`
- `GetThemeName()`

控件级主题和样式使用 `ControlStyleSheet`。它只写入已经注册元数据的属性，并分别通过
`SetThemeStyleSheet(...)` 和 `SetStyleSheet(...)` 落到 Theme、Style 来源层，因此 Local 和
Binding 仍按既定优先级覆盖它们。把样式表附着到根控件会递归传播；之后通过 `AddControl` /
`AddOwned` 加入的子树也会继承。传入 `nullptr` 会只清除该样式表曾应用的属性并回退到下一层。

`ControlStyleSelector` 支持：

- `Type`：`UI_Base` 表示所有控件，其他 `UIClass` 为精确类型；
- `Id`：匹配 `Control::SetStyleId(...)`；
- `Classes`：列出的 Class 必须全部存在，可用 `AddStyleClass` / `RemoveStyleClass` 修改；
- `RequiredStates` / `ExcludedStates`：组合 Hovered、Focused、Pressed、Disabled、Checked、
  Selected。

级联特异性顺序是 `Id > Class/状态 > 精确类型 > 通配规则`，特异性相同时后加入的规则胜出。
Hover、Focus、Pressed 和 Checked 会从控件事件自动刷新；Disabled 也会从有效 Enable 状态
解析。显式状态使用 `SetStyleState(...)`。样式表资源通过 `SetResource(...)` 和
`ControlStyleSetter::Resource(...)` 引用，键名不区分大小写。规则或资源修改会通知所有附着
控件热刷新；缺失资源会出现在 `Resolve(...)` 的问题列表中，并使显式刷新返回 false。
不存在、不可写或无法按元数据转换/Coerce 的属性也会形成解析问题；无效声明不参与级联，
因此较低特异性的有效声明仍可作为回退。

`Button`、`TextBox`、`ComboBox`、`Panel`、`GroupBox`、`Expander`、`ScrollView` 的常用状态色、边框、圆角和间距已迁移到
属性元数据，并使用 `TracksLocalValue` 保持公开 setter 的 Local 语义。仍未迁移的旧外观字段继续直接设置；新增
可主题化属性必须先注册元数据并使用 `SetPropertyField(...)`，不要再为主题系统增加控件类型
switch。

Designer 选中控件后可直接编辑 `StyleId` 与 `StyleClasses`；后者使用逗号分隔，空白会被清理，
重复项由运行时去重。两项都必须随设计文档保存/加载，并由代码生成器输出
`SetStyleId(...)` / `AddStyleClass(...)`。

未选中控件时，窗体属性面板的“编辑文档样式表”用于维护 `DesignerStyleSheet`。资源值必须
显式声明 Bool、Int、Int64、Float、Double、String、Color、Thickness、Size 或 Length 类型；
规则可组合类型、ID、多个 Class、必需/排除状态及 Literal/Resource Setter。编辑器必须在提交前
调用统一的规范化与校验逻辑，成功后把运行时 `ControlStyleSheet` 递归附着到预览根控件；应用
失败时保留旧样式表。XML v3 和代码生成器必须保存并重建同一份语义，不能各自维护另一套解析规则。

Setter 属性选择使用 `DesignerPropertyCatalog` 对 `BindingPropertyRegistry::GetProperties(...)`
的投影，不维护第二份属性名清单。目录只列出可写且能由 Designer 持久化的类型，并从当前值或
默认值生成示例文本；常用枚举按 Int 持久化并继续走元数据转换。规则保存与画布应用还必须通过
目标 `UIClass` 的代表实例检查属性存在性、值类型、转换和 Coerce；无类型规则只允许基础
`Control` 的公共属性，避免通配规则在某些控件上产生延迟错误。

普通控件属性面板使用同一运行时投影生成全部可浏览标量，包括仍由旧 `Props` / `Extra` 字段持久化的
Legacy 属性。所有能解析到
元数据的编辑（包括仍由 Legacy 字段持久化的常用属性）必须通过
`DesignerPropertyCatalog::ApplyAndTrackValue(...)` 设置 Local 层并回读 Coerce 后的规范值，禁止在
属性面板中用直接字段赋值绕过 Binding/Style 优先级、Changed 回调或属性标志。包装器的跟踪规则只
由 Design `Persistence` 决定：Metadata/Automatic 写入 `MetadataProperties`，Legacy/Transient 删除
同名重复项；恢复默认值使用 `ResetAndUntrackValue(...)`，直接交互修改则用 `TrackCurrentValue(...)`。
加载、撤销/重做和代码生成均
使用规范属性名与精确 kind，旧字段继续保留以兼容已有 XML，不能把两套字段同时用于同一属性。
普通面板使用 `GetPropertyGridProperties(...)`，按 Design 分类和顺序生成编辑器，只排除不可浏览项与
`Transient`；只需要新式通用持久化属性的调用方使用 `GetBrowsableProperties(...)`，它继续排除
`Legacy` 与 `Transient`。样式 Setter 使用 `GetStyleProperties(...)`，仍能看到全部可写且可转换属性，不能把
“不在普通面板显示”误解为“不可 Binding/不可样式化”。Choice 显示文本与真正的强类型值必须分开，
保存和 Setter 始终使用规范值而不是本地化显示名。

结构化集合不应伪装成标量元数据。ComboBox Items、GridView Columns、Tab Pages、ToolBar Buttons、
Tree Nodes、Grid Definitions、Menu Items 与 StatusBar Parts 的入口注册在
`DesignerCustomEditorCatalog`，属性面板只按目录创建按钮，再按 `DesignerCustomEditorKind` 分发对话框。
新增结构编辑器应注册描述，不得恢复按 `UIClass` 扩张的显示 `if/else` 链。

所有文档级编辑应优先使用 `DesignerCanvas::ExecuteDocumentEditTransaction(...)`；需要跨模态窗口或连续预览时，
使用 `BeginDocumentEditTransaction(...)` 配对 `CommitDocumentEditTransaction()`、
`CancelDocumentEditTransaction()` 或 `RollbackDocumentEditTransaction()`。返回的
`DesignerDocumentTransactionResult` 会区分 `Begun`、`Committed`、`Unchanged`、`RolledBack`、`Canceled`、
`Aborted`、`Rejected` 与 `Failed`，调用方必须显示其错误而不是压成没有原因的 `bool`。同一时间只允许一个
严格事务，嵌套 Begin 必须为 `Rejected`。提交会验证后置文档并以单条命令保存前后完整选择；无变化提交不
进入历史，捕获/验证/入栈失败必须恢复前置文档。Cancel 会先验证当前文档，若模态窗口意外泄漏修改则自动
回滚。不得在 PropertyGrid 重新捕获前后 `DesignDocument`、手工构造快照命令，或在 `ShowDialog()` 返回后
只做 `InvalidateVisual()` 而遗留不可撤销集合修改。

Canvas 的键盘微调、鼠标拖拽、resize 与 SplitContainer 分隔条预览必须使用结果型交互入口。鼠标移动/缩放
通过 `BeginPlacementInteraction` 捕获 placement/tree 起点并在 MouseUp 提交 `ControlPlacementCommand`；分隔条通过
`BeginControlPropertyInteraction` 捕获 `SplitterDistance` 并提交禁止跨手势合并的 `ControlPropertyCommand`。只有
不支持差量的自定义父级回退 `BeginCanvasInteractionTransaction` 完整快照。不得直接访问
`DesignerCommandCoordinator` 的内部快照。首次运行时修改前必须先成功 Begin，鼠标抬起只提交一次；Escape、
`WM_CANCELMODE`、窗口失焦、应用停用和 `WM_CAPTURECHANGED` 必须调用
`CancelActivePointerInteraction(...)` 恢复预览前状态。完成结果要写入 Canvas
最后结果并通过 `OnInteractionTransactionCompleted` 发布，Designer 状态区需显示错误或取消原因。预览 setter
失败必须中止事务，禁止退回裸字段/setter 绕过属性元数据。

离散的 Add/Delete/Undo/Redo 必须返回 `DesignerDocumentTransactionResult`，并通过 Canvas 的
`OnCommandCompleted` 发布 operation、历史 label、消息和完整结果；Designer 状态区消费该事件。空历史或空删除是
`Unchanged`，业务前置条件不满足是 `Rejected`，应用/恢复失败是 `Failed`，不得重新压成 `bool` 或无条件显示成功。
`IDesignerCommand` 与 `CommandManager` 的 Execute/Undo/Redo 同样返回结果对象；失败和异常必须把命令保留在原栈，
`DocumentRestored` 必须来自实际快照恢复结果而不是调用方猜测。严格文档事务处于 Begin 与
Commit/Cancel/Rollback 之间时，独立 Execute/Undo/Redo 必须返回 `Rejected`，不得让旧历史穿插进预览状态。

Add/Delete 必须使用 `ControlSubtreeCommand`，不得回退到整文档快照或裸指针所有权。子树挂载时只能由运行时父子树拥有；
缺席时命令必须以 `unique_ptr` 拥有每个顶层分离根，并保存规范化持久化节点、Root/普通/TabPage/Split 父级定位器、同级索引、
ToolBar 工具项尺寸覆盖和完整选择。Undo/Redo 先验证 expected 端点；名称冲突、子树外部修改或父级失效时保留原栈项，修复后可重试。
整文档快照在子树缺席期间重建了其他控件后，恢复必须按 Name+UIClass 重新解析父级，不得持有已销毁父级裸指针。历史内存估算必须同时预留规范化快照和缺席期间的运行时子树。

文档 Dirty 必须由 `CommandManager` 的文档状态 ID 与保存点判断，不能用撤销栈长度、是否存在 undo 或文件名
推断。每个成功提交分配不可复用的状态 ID；Undo 切到条目的 before ID，Redo 切到 after ID。保存只在 XML
真正落盘后调用 `MarkDocumentSaved()`；Undo 后建立的新分支不得复用已丢弃 redo 分支的 ID。New/Open 成功后
调用 `ResetDocumentHistoryAsSaved()` 建立全新干净基线，失败时必须保留原文档、完整选择、历史状态和 Dirty。

New/Open/Save 必须返回 `DesignerDocumentTransactionResult` 并拒绝严格事务中的预览状态。Open 应先完成解析，
再以完整文档替换事务应用；目标应用失败时恢复原文档和选择。Save 必须使用同目录临时文件、完成写入和刷盘后
原子替换目标，失败时清理临时文件且不得移动保存点。Designer 发起 New/Open/Save/Close 前应取消活动指针预览
并提交 PropertyGrid 挂起编辑；存在属性编辑错误时不得保存。未保存提示必须支持 Save/Discard/Cancel，窗口标题
和当前文件名只反映已成功的生命周期操作。

自动恢复不得调用 `SaveDesignFile()`，因为恢复快照不能移动正式保存点。Designer 应在
`OnDocumentStateChanged` 的 Dirty 状态下防抖构建完整 `DesignDocument`，通过 `DesignRecoveryStore` 写入
当前进程会话文件；回到保存点、New/Open 成功或干净关闭时只删除自己的会话文件。会话身份同时包含 PID 与
进程创建时间，启动枚举必须跳过身份仍匹配的存活进程，不能让两个 Designer 实例互删恢复数据。恢复成功后使用
`RestoreRecoveredDocument()` 建立无 Undo/Redo、但 CurrentStateId 与 SavedStateId 不同的 Dirty 基线；正式
保存成功后才可变 clean。恢复 envelope 必须有版本、长度边界和最大尺寸校验，损坏文件改名隔离；任何加载或
应用失败都不得修改当前文档。原遗留快照只有在当前会话的新快照已原子落盘后才能删除。

`CommandManager` 的历史限制同时包含命令条数和 Undo+Redo 总估算内存。`IDesignerCommand` 应为大型命令实现
`GetEstimatedMemoryUsage()`；预算淘汰优先移除最远的 undo/redo，但即使单条命令超预算也必须保留一个最近可
操作项。失败 Undo/Redo 在原栈恢复时不得重复增加或扣减用量；清空 redo、重置文档和历史淘汰必须同步更新计数。
需要合并的命令通过 `TryMergeWith()` 把旧 before 与新 after 合成一条，调用方不得先丢旧命令再尝试合并。
`ControlPropertyCommand` 只合并 1 秒内、同 Canvas、同选择且中间属性状态完全相等并允许合并的 `UpdateProperty:<name>`；
Splitter 与其他鼠标预览必须关闭合并。`ControlPlacementCommand` 只对 `NudgeSelection` 使用同样规则；每个鼠标手势保持一条独立命令。保存点恰好位于当前状态、存在 redo 分支、选择/标签
变化或当前起点被历史外修改时必须切断或拒绝。合并仍分配新的文档状态 ID，并重新计算内存估算，不能让 Dirty
或恢复文件通知漏掉最新提交。其余窗体属性和仍走完整事务的属性可继续由 `DocumentSnapshotCommand` 兜底合并；
单个事件映射与文档级处理函数重命名必须使用 `EventHandlerCommand`，不得退回完整文档历史。
处理函数重命名默认只改文档引用；只有用户在对话框显式选择且头/源联合索引证明旧名称恰有一个兼容定义、目标没有
同签名定义时，才可迁移用户函数体。迁移命令只能替换成员定义名 token，不得重写函数体；命令历史只保存路径、
类名、签名、实际定义文件和旧/新名称，不得保存整份用户源码。实际定义文件只能是关联基路径的 `.h` 或 `.cpp`。
Execute/Undo/Redo 每次都要重新读取并预检当前源码，先捕获
`.h/.cpp/.g.h/.g.cpp/.handlers.g.inc` 快照，再把文档映射、源码 token 与重新生成作为一个可回滚操作；外部编辑、
写入或生成失败必须恢复文档和五文件并把原命令留在原历史栈供重试。合并到已有文档函数时禁止迁移函数体。

不转移 Designer 子控件所有权的模态集合必须优先使用 `ControlStructureCommand`，当前包括 ComboBox Items、
TreeView 节点、递归 Menu Items、GridView 列、GridPanel 行列定义和 StatusBar 分段。ComboBox 快照必须把 Items 与
`SelectedIndex` 的有效值、Local/Binding 存储值、设计器 Binding 配置及 metadata 跟踪作为同一原子状态；
列表替换触发的 Coerce 不得丢失可恢复的 Binding 索引。其他快照只能保存目标集合的强类型持久化字段与目标
stable ID/Name/UIClass，禁止夹带完整 `DesignDocument`。Undo/Redo 必须先捕获当前集合并与 expected 状态逐项
比较，冲突时保留历史；应用失败必须尝试恢复刚捕获的当前集合。恢复应保持目标 `Control*` 实例和完整选择。
TabControl 页面和 ToolBar 按钮必须使用 `ControlOwnedCollectionCommand`：对话框只输出编辑模型；命令在子树缺席时以
`unique_ptr` 持有直接根，并保留该根下全部 DesignerControl 包装器及扁平位置。TabPage 本身不要求包装，但页内包装器
必须随页共同分离/恢复；ToolBar 新建或旧版未包装 Button 必须分配稳定 ID、默认名称和根包装器。命令还须精确恢复
ToolBar 尺寸覆盖、完整选择，以及 TabControl `SelectedIndex` 的有效值、Local/Binding、Binding 配置和 metadata 跟踪。
expected 冲突不得消费历史，部分失败必须回滚；禁止退回完整文档或仅保存文本的伪差量。

属性差量必须区分持久化语义：Legacy 保存规范有效值并通过 `TrySetPropertyBaseValue()` 恢复为序列化等价基值；
Metadata 保存 Local `BindingValue`、规范文本和单个跟踪条目，不能复制整个 MetadataProperties 或完整文档。
放置差量须保存 Location/Margin/显式尺寸/对齐/Anchor、Grid/Dock 附加字段、可重建的父级定位器和同级索引；
Root、普通控件、TabPage 与 Split 两个运行时区域必须可解析，无法标识的自定义父级才允许回退完整事务。
差量 Undo/Redo 必须先验证 expected 起点，目标与父级实例失效时按 Name+UIClass（TabPage 再加页索引）重新解析；
验证、父子集合观察者或 setter 失败须恢复当前目标及已应用目标，并让 `CommandManager` 保留原栈项。

包装器专用属性（当前为 Name、Anchor、StyleId、StyleClasses、FontName、FontSize，以及
MediaPlayer 的 MediaFile）必须注册在 `DesignerControlPropertyCatalog`。PropertyGrid 只能通过
`PropertyGridBinder` 的 Capture/Apply/Reset 接口读写；唯一命名、默认名称计数、共享字体继承和
Anchor 保持边界由 `DesignerControlPropertyContext` 注入。不得在文本、布尔、浮点或控件类型分支中
恢复裸字段回退。新增包装器属性必须同时声明 kind、分类、顺序、编辑器、适用条件和 Reset 合约。

属性面板的展示输入必须来自 `DesignerPropertyRowCatalog`。该投影负责把窗体属性，或包装器属性与
运行时元数据的并集，转换为统一 `DesignerPropertyRow`，并完成规范名去重及跨来源的分类/顺序排序。
PropertyGrid 不得再次分别遍历三类 Descriptor，也不得自行推断 Reset 能力或为某一来源复制渲染分支。
设计器只负责把统一行映射为 CUI 原生 `PropertyGridView::Items`；Boolean、Choice、Color、Slider、混合值、
Reset 和 Action 行优先扩展并复用原生能力，不能重新逐行创建 TextBox/CheckBox/ComboBox/Button。新增属性
来源应先扩展行投影，再复用统一映射。

统一行还必须承载 Binding、Validation、Style 与 Theme 诊断。Binding 诊断应包含持久化路径/模式/
UpdateMode/Converter、设计期预览状态及运行时错误；验证问题保留严重级别与代码；Style/Theme 诊断应
指出命中规则 ID、特异性和是否被 Local/Binding 等更高来源遮蔽。筛选与 AccessibleDescription 必须覆盖
这些文本，`OnValidationStateChanged` 或样式表 Changed 后应重建完整行快照。不得在 PropertyGrid 再维护
一套独立诊断数据源。

多选属性面板必须把完整选择集合绑定到 `PropertyGridBinder`，再由
`DesignerPropertyRowCatalog::GetCommonControlRows(...)` 求公共行交集。只有名称相同且 source、kind、
editor、Choice、数值约束兼容的行可进入交集；`Name` 等身份属性不得批量编辑。不同当前值/有效来源分别用
`HasMixedValue` / `HasMixedValueSource` 表示，不能用主选值冒充整组值。批量写入前必须验证所有目标，任一
目标由 Binding 拥有时该行只读；一次用户修改只由统一文档事务创建一条 `DocumentSnapshotCommand`，命令
前后都保存完整选择名称集合和主选名称。诊断集合不同时用 `HasMixedDiagnostics` 标记并隐藏主选详情。多选时事件、
Binding 与结构化集合编辑器仍只允许单目标操作，不得隐式作用于主选。

控件标量写入和 Reset 必须经过 `DesignerPropertyEdit`，不得在 PropertyGrid 的 Text/Bool/Float/Anchor
分支重新实现目录查找和逐目标循环。属性层须先验证全部目标，随后捕获 Local 值、包装器值和
`MetadataProperties`；任一 setter 返回 false 或抛出异常时，已触碰目标必须逆序恢复，失败结果须包含目标
控件名。PropertyGrid 必须在固定顶部区域呈现失败信息并填写可访问描述，成功编辑或选择变化后清除。
外层必须再经结果型文档事务保存完整文档和选择；`BuildDesignDocument` 无法建立命令前/后快照时不得直接
应用或遗留修改。业务拒绝、异常、命令入栈失败、Binding/样式刷新失败和分组滑块预览/提交失败都必须由同一
事务入口恢复修改前文档及完整选择集。

需要程序化驱动属性面板时，使用 `PropertyGrid::ApplyPropertyValue(...)` / `ResetPropertyValue(...)`；
不得直接调用 Binder setter 来冒充交互测试，因为那会跳过设计文档快照和命令栈。修改 PropertyGrid、
DesignerCanvas、选择恢复或 Undo/Redo 后，除 `CUICoreTests` 外还必须运行对应配置的
`Designer.exe --self-test`。该模式不得创建 HWND，且应继续使用真实生产对象而非复制的测试控制器；Canvas
手势修改还须用真实 `WM_LBUTTONDOWN/MOVE/UP`、Escape、CancelMode 或 CaptureChanged 消息验证提交、回滚、
最后结果事件以及取消不污染 undo/redo。

PropertyGrid 必须保持“属性 / 事件”两种互斥视图，属性行和事件行不得重新混排。两种视图分别保存筛选词、
折叠分类和滚动位置；`SetItems`、属性提交、选择刷新和视图切换都不得清空当前视图状态。`Ctrl+1` / `Ctrl+2`
是稳定快捷键，表头、筛选框和模式按钮的可访问名称必须随视图更新。多选事件页只显示明确说明，不得暗中把
事件编辑作用到主选对象。

事件激活后的源码跳转必须使用 `SourceCodeNavigator`：定义识别要忽略注释、普通/原始字符串和仅有声明，
编辑器路径及文件参数通过 `CreateProcessW` 的安全引用传递，不得拼接到 shell。VS Code / Visual Studio 的
精确行参数、`CUI_CODE_EDITOR` / `CUI_CODE_EDITOR_ARGS` 自定义模板和系统文件关联回退都须由不启动外部程序的
plan 自测覆盖。

ToolBox 使用七个稳定控件族、名称/类型/分类关键词筛选和代码原生矢量轮廓。新增控件必须选择合适分类并
提供可辨识的图形；不得让所有条目回退为同一个 SVG。窄栏文本须单行省略，筛选后的分类标题与空结果状态
必须同步更新。

设计期 `DesignerDataBinding` 配置在宿主设置 `Designer::SetDesignDataContext(...)` 后必须实例化为真实
`Control::DataBindings`。对 OneWay/TwoWay/OneTime，连接前先保存并清除目标 Local 层，使 Binding 能按
既定优先级生效；配置移除、DataContext 断开或 Add 失败时恢复该 Local 值。OneWayToSource 不清除目标
Local。预览状态只保存在 `DesignerControl::BindingPreviewStates`，不得进入 XML。生成的 `BindData(...)`
也必须对写目标模式执行同样的 Local 保存、清除和 Add 失败恢复，避免构造期初始化值遮蔽 Binding。

`IDesignerCommand::Undo()` 必须真实返回恢复是否成功；`CommandManager` 只有在成功 Undo 后才能把命令
移入 redo 栈，失败或异常时须保持原栈和顺序。`DesignerCommandCoordinator` 的增删/交互快照以及 Canvas 的
placement/tree、属性预览在前置捕获失败时不得执行修改，后置捕获或命令入栈失败时必须恢复前置状态和完整选择。
拖拽、缩放、分隔条和键盘移动在差量捕获与完整事务回退都无法开始时应停止本次预览，而不是产生不可撤销状态。

`DesignerPropertyRow::EffectiveValueSource` 必须来自运行时 `GetPropertyValueSource(...)`，不得根据
是否存在持久化值猜测。属性筛选使用 `DesignerPropertyRowCatalog::FilterRows(...)`，采用大小写不敏感、
空格分词的 AND 语义，并覆盖名称、分类、值、编辑器、Choice、中英文来源及诊断摘要/详情；事件与结构入口用同一
`MatchesFilterText(...)` 规则。PropertyGrid 的来源标记在成功编辑后必须刷新，避免把新 Local 值继续
显示成 Default/Style/Binding。筛选框属于固定顶部区域，不得随滚动内容销毁或在切换选择时丢失查询。

窗体属性只能以 `DesignerModel::DesignFormModel` 为状态模型；不得再增加平行 Snapshot 或按 Text/Bool
拆分的字符串分派。窗体属性面板必须从 `DesignerFormPropertyCatalog` 生成，所有修改与恢复默认值
分别使用 `ApplyValue(...)` / `ResetValue(...)`，然后通过 `ApplyDesignedFormModel(...)` 一次应用。
`BuildDesignDocument` 与代码生成输入必须先 `CaptureDesignedFormModel()`，避免保存、预览和生成各自
复制字段清单。默认字体名为空表示继承字体族，不代表丢弃显式 FontSize。

属性面板中有默认值的窗体/控件属性必须提供可见的逐项恢复入口。控件恢复使用
`DesignerPropertyCatalog::ResetAndUntrackValue(...)`，窗体恢复使用 Binder 的 `ResetFormProperty(...)`；
两者都必须通过 `ExecutePropertyCommand(...)` 进入撤销栈。事件回调中不得立即删除作为 sender 的
编辑器控件，应请求下一次 Update 重建面板。

从旧 `Extra` 迁移属性时，只允许“旧读、新写”：先应用 `props.metadata`，仅当同一规范属性尚未
存在时读取旧字段，并立即把 Coerce 后的有效值记录到 `MetadataProperties`。新保存与代码生成必须
停止输出旧字段。`StackPanel` 的 Orientation/Spacing/内容对齐、`WrapPanel` 的
Orientation/ItemWidth/ItemHeight、`DockPanel` 的 LastChildFill、`SplitContainer` 的布局和分隔条外观，
以及 `GroupBox` / `Expander` / `ScrollView` 的专用属性是这条规则的基准实现。拖拽、缩放等交互若直接改变已迁移属性，也必须通过
`DesignerPropertyCatalog::TrackCurrentValue(...)` 回写包装器的规范 metadata，不能只修改运行时字段。

`ScrollView::ScrollXOffset` / `ScrollYOffset` 是可观察、可 Binding 的运行时状态，但 Design 持久化策略为
`Transient`：普通属性面板、`props.metadata` 与代码生成必须排除它们，滚动交互继续通过 `OnScrollChanged`
通知。旧 XML 偏移只允许加载兼容，不得在新保存中重新写回。

`Panel::BorderThickness`、`CornerRadius`、`DisabledOverlayColor` 使用 Panel 的唯一 backing；ScrollView、
ToolBar、StatusBar、PagedGridView、Expander 不得重新声明同名字段。派生类型需要不同圆角默认值时，构造阶段
使用 `InitializePanelCornerRadiusDefault(...)` 初始化 backing，并在自己的元数据注册中调用
`RegisterPanelCornerRadiusMetadata<T>(...)` 声明对应默认值；两者必须一致。这样基类 setter、派生绘制、
Reset、样式来源与 Designer 目录不会分裂。自定义 `Update()` 也必须读取这些继承属性，禁止重新硬编码禁用遮罩。

派生控件不得用不同类型重新声明基类属性名。ToolBar/StatusBar 旧的 `int Padding` 已删除，规范属性是
`HorizontalPadding(int)`，而继承的 `Padding(Thickness)` 继续表示四边布局内边距。旧 C++ 调用方必须把
原来的 `bar->Padding = n` 改成 `bar->HorizontalPadding = n`；Designer 旧 Extra 的 `padding` 只升级到
`HorizontalPadding`，不能覆盖 `props.padding` 中的 Thickness。ToolBar/StatusBar 的公开标量和颜色均由
元数据目录负责，只有 ToolBar 子项编辑器与 StatusBar parts 这类结构化集合保留专用路径。

存在依赖关系的 metadata 属性必须用 Design `CategoryOrder` / `Order` 明确应用顺序，Designer 加载和
代码生成都按该顺序处理，不能依赖 JSON/XML 对象的键名顺序。`Slider` / `NumericUpDown` 的
Min → Max → Step/吸附 → Value 是基准实现。范围或其他依赖项变化后，控件使用受保护的
`ReevaluatePropertyValue(...)` 在当前值来源层重新执行 Coerce；交互值更新则使用
`SetCurrentPropertyField(...)`，以免破坏现有 Binding。
`Expander` 的依赖顺序为 AnimationDurationMs → IsExpanded；交互展开/折叠同样使用当前值更新，
而公开 setter 和 `SetExpanded(...)` 仍表达显式 Local 值。

### Designer 文档、静态生成与动态 XAML

Designer、静态代码生成和动态加载必须共同经过 `DesignDocument` → `DesignDocumentMaterializer`；禁止为
XAML 另写一套控件构建 switch、属性 setter 或容器挂载逻辑。生产工厂由
`DesignDocumentMaterializer::CreateRuntimeControl(...)` 提供，只有 Designer 预览可显式注入
`DesignerControlFactory`。`RuntimeDocumentLoader` 的所有入口都先构建候选对象，任何解析、Binding、样式、
事件或挂载错误都不得修改调用方原有文档。

`XamlDocumentParser` 只是可读语法前端：控件属性类型、规范名称、枚举 Choice、Coerce 与持久化类型继续取自
运行时属性元数据。`Width` / `Height` 映射到支持浮点与 `Auto` 的 `LayoutWidth` / `LayoutHeight`；
`x:Name` 用于名称索引和静态引用，可选 `DesignId` 保持跨持久化稳定身份。`{Binding ...}`、命名事件、强类型
资源和 Style Setter 必须投影到现有模型，不能在 XAML 层直接安装运行时连接。新增语法应同时覆盖
XAML → `DesignDocument` → 规范 XAML/v5 XML 往返、静态代码生成输入、动态加载和失败回滚。
`XamlDocumentSerializer` 必须保留无法用普通属性表示的 `Props` / `Extra`，当前约定使用
`d:DesignProps` / `d:DesignExtra` 的强类型通用值袋；自定义控件专有 Binding 使用 `d:DesignBindings`，不得为
追求表面可读性而静默量化颜色或丢弃结构字段。
外部控件必须作为 `DesignNode::CustomType` 的一等模型贯穿 XAML、v5 XML、Canvas 快照、材质化和代码生成，
不能只把未知标签塞入 `Extra`。描述符至少包含带前缀 XAML 名/命名空间、规范 C++ 限定类型、安全相对头文件和
`Default`/`Bounds`/`TextBounds` 构造约定；`DesignNode::Type` 保存内置元数据基类。生产动态加载只接受
`RuntimeCustomControlRegistry` 中显式注册且 `Type()` 与基类一致的实例，缺失/错误工厂必须事务性失败。
Designer 与无窗口生成器可显式启用基类代理，但普通运行时不得默认降级；静态生成仍须使用描述符中的真实 C++
类型和头文件。前缀命名空间冲突、保留 `x`/`d` 前缀、不完整描述符及不安全 include 路径应在提交前拒绝。
Designer ToolBox 的扩展入口统一为 `DesignerControlDescriptor`；内置项和外部项不得再走两套仅传 `UIClass` 的
添加链路。外部清单使用 `cui.designer.controls/1`，整份文件必须事务性合并并拒绝名称、XAML identity 和前缀
映射冲突；解析应限制大小、禁止 DTD、拒绝未知属性/子内容。`property` 子元素是受限的可移植 schema：
必须校验 kind/default/editor/choice/范围、基类重名和 Binding 能力，不得把未校验文本直接写入生成代码。
`event` 子元素同样是受限契约：名称和真实 Event 字段必须是安全标识符，签名只能来自框架定义的固定预设，
不得接受清单提供的任意 C++ 类型文本。自定义事件契约必须随 `DesignNode` 持久化到 XAML/XML，使无窗口生成
不依赖当前机器的清单；Designer 重新加载已安装清单时，已使用事件的名称、字段或签名不兼容必须事务性拒绝，
不能静默覆盖文档契约。动态宿主必须用 `RegisterCustomControl` 将持久化函数名、XAML 类型身份、契约与真实
Event 成员配对，并按 `Event::function_type` 精确核对后才能订阅。
清单不得包含 DLL 路径。同进程预览可按 XAML key 附加 `PreviewFactory`；独立插件只能由受信任宿主显式传入
`--preview-plugin`。不得跨静态 CRT 边界传递并由宿主 `delete Control*`。
跨模块预览遵循 `CuiDesigner/DesignerPreviewPluginAbi.h` 与
`CuiDesigner/CUSTOM_CONTROL_PLUGIN_ABI.md`：宿主拥有基类代理，插件拥有 opaque session 并只返回有上限的
值类型绘制原语。`DesignerPreviewPluginModule`/session 负责 UI 线程亲和、上限校验、同步拷贝和 RAII 卸载；
`DesignerPreviewBridge` 把原语绘制在宿主代理的局部裁剪中，并将强类型自定义属性同步为 ABI `SetValue`。
插件路径必须来自显式受信任配置，不能来自设计文档或控件清单。
动态 XAML 解析收到注册表时必须用真实控件探测专有属性元数据并执行类型转换/Coerce；规范 writer 没有业务 DLL
时不得把专有属性误写成无法再解析的普通 attribute，而应保留到强类型袋。工具代理允许延迟保存这些元数据与
Binding 并由生成器输出运行时设置，生产加载仍必须对真实实例重新校验。Reload 解析 XAML 时应继承已有注册表。
Designer 依据 `.xaml` / `.xml` 扩展名选择源格式，Save 保持当前格式且只在原子替换成功后移动保存点。
显式重新加载必须先解决 Dirty，并通过候选文档替换保证失败时当前画布不变；自动文件监视不能绕过此事务。

事件引用必须先通过 `DesignDocumentEventIndex` 解析。处理函数身份由大小写敏感的 C++ 标识符和精确
`Event::function_type` 共同约束；`ParameterList` 只负责生成/展示可读声明，不能再作为类型身份。同名同类型可以
复用，即使两个事件使用不同参数名；同名异类型必须在 XML/XAML 提交、Designer 编辑、动态加载和
代码生成前拒绝。文档级重命名必须更新所有解析到旧名称的引用（包括旧 `Auto`/布尔约定名），并作为一次完整
文档事务支持 Undo/Redo。不得自动文本替换任意用户 C++ 函数体。
事件差量必须以 Form 身份或控件稳定 ID + 类型 + 名称 + 事件名定位，提交前一次性核对所有 expected 映射；
目标事件表应先在文档外构造，再以无异常交换提交。过期起点、重复目标、契约变化或最终索引冲突必须保持全部
原映射和历史不变。单事件编辑、默认事件激活及跨控件批量重命名均不得重建控件实例，典型命令应 `<32 KiB`。
动态宿主优先用 `RuntimeEventHandlerRegistry` 声明函数名到真实 `Event` 成员/callable 的路由，不应在每个
Load/Reload 调用点复制名称 `if/switch`。EventCatalog 项必须从真实成员指针推导字段、函数类型和参数类型文本，
只单独提供参数名；手写一整段类型字符串属于错误实现。注册除 callable 类型外还必须核对精确成员指针，拒绝
同签名的错误成员、同名异类型和重复路由；自定义事件包装器应公开 `function_type` / `std_function_type` 与
`Subscribe()`，Form 继承的 Control 事件也必须可注册。
resolver 应持有共享注册状态，使后续新增函数名无需替换已保存的 resolver。事件注册与解析均在 UI 线程执行。
手工批量路由可通过 `RuntimeEventHandlerRegistry::RegisterBatch` 一次提交；生成路由必须使用
`RegisterScopedBatch` 并由生成 Sink 持有其移动租约。两者都要在调用前复制完整 Handler/route/令牌状态，任一
注册返回 false 或抛异常后以 noexcept swap 恢复；已有 resolver 继续观察同一共享 State，不得用替换 shared_ptr
伪造回滚。租约只能删除本批令牌范围内的路由，保留同名处理函数的既有其他路由；批次禁止嵌套，回调只能追加
路由，不得清空注册表或释放另一租约。批次执行期间不得跨线程解析事件。自定义生成路由只能把
namespace/type/event/field/固定签名预设
重新构造成 `DesignerCustomEventDescriptor`，随后仍由真实 Event 成员类型校验，禁止把任意签名文本带入生成头。
EventCatalog 必须为每个可设计控件类型声明且仅声明一个默认事件，并提供稳定的类型化分类；Form 默认事件为
只触发一次的 `OnShown`。事件属性双击与画布控件/Form 客户区双击必须汇合到同一激活入口，并通过与文本提交
相同的校验/撤销事务：已有函数直接激活，空事件写入可预测默认名，不得直接改 `EventHandlers` 绕过命令栈。
处理函数激活应作为显式事件交给宿主；独立 Designer 只能由用户的明确导出创建
或改变 code-behind 关联。类身份必须独立于 `Form.Name` 持久化为合法的 `x:Class`，代码位置只能保存为相对
设计文件且无扩展名的 `d:CodeBehind`；打开/重新加载/恢复同一文档可解析并复用该关联，新建或无关联旧文档仍
必须清空会话目标，禁止从设计文件名猜测并覆盖代码路径。关联修改走完整文档事务并支持 Undo/Redo；删除或
重命名事件仍保留既有用户函数体。已有 `x:Class` 的文档再次显式导出时必须默认保持该类身份，单纯选择输出
文件只更新 `d:CodeBehind`；只有独立的类名确认字段可显式请求迁移，并必须说明旧用户函数体不会自动改写。
当前绑定且由用户源文件定义的处理函数声明必须带 `override`；事件解绑后仍需保留普通成员声明，使已有函数体
继续编译，重新绑定时恢复 `override`。用户类体内已有兼容定义时生成声明必须省略，不能制造类内重复声明。
类名、无扩展名输出和可移植相对路径必须在生成器创建目录或替换任何代码文件前完成预检。导出不得要求至少
存在一个子控件；只有 Form 事件的空窗体也是合法静态代码输入。已有显式目标应提供无需再次选择路径/类名的
快速重新生成入口，其启用状态必须随 Open/Recovery/code-behind Undo/Redo 的关联解析同步，不能复用陈旧会话路径。
`x:Class` 应接受 XAML 风格 `.` 与 C++ `::` 分隔并规范为 `::`；每一段都按非保留 C++ 标识符校验。生成器必须
把限定名称拆成 namespace + 叶类，在 namespace 内声明 Generated/用户类，在 `.cpp` 中使用限定定义；输出
文件基名属于 `d:CodeBehind`，不得再次从限定类名拼接 include 路径。
交互导出与无窗口构建必须共同调用 `DesignCodeGenerationService`；该服务不得创建 `Form`、HWND 或依赖
Designer 会话状态。`CuiCodeGen generate` 只负责编排参数、加载 `.xml/.xaml` 并调用同一服务，不得复制解析、
材质化或五文件提交逻辑；无覆盖参数时必须以文档的 `x:Class` / `d:CodeBehind` 为唯一依据。CLI 的成功、生成
失败、用法错误退出码固定为 `0/1/2`，标准输出重定向时保持 UTF-8。`build/CuiCodeGen.targets` 以设计文件、
targets 文件本身及目标五文件作为输入、以 `$(IntDir)` 下带生成契约版本的 stamp 表示增量新鲜度，并在
`ClCompile` 前运行；接受 stamp 前必须确认五个代码文件
仍存在。用户 `.h/.cpp` 不得成为 Clean 可删除的普通构建产物；三个生成文件被外部修改后普通 Build 必须恢复
规范内容。生成内容逐字节相同时，原子批次必须跳过该目标
文件以保留时间戳；stamp 仍需更新，防止相同语义的设计文件每次 Build 都重复启动 CLI。CLI 项目引用必须保证
干净解决方案构建先产出对应平台/配置的生成器。任何改变生成输出语义的修改必须同步提升
`CuiCodeGenContractVersion` 和 CLI 主版本；不得把链接器每次可能刷新的 exe 时间戳直接作为增量输入。
生成器必须先用无副作用的 `BuildFilePlan` 得到精确五文件内容，再进入原子提交；交互新鲜度检查只能复用同一
计划逐字节比较，禁止另写一套近似 hash/时间戳规则。检查不得创建目录、修改文件或更新时间戳；合法用户扩展
必须视为当前计划的一部分，而缺失用户事件桩、`.g.*`/声明文件漂移、缺文件和用户类身份/签名阻塞必须区分为
Stale、Missing 或 Blocked。Designer 文档提交后应立即显示过期并防抖精确复核，Undo/Redo 可按已验证状态 ID
即时恢复，窗口重新激活时必须复查外部文件漂移；状态缓存应有界。
`CuiCodeGenCore.vcxproj` 必须是 `CodeGenerator.cpp`、`CppUserCodeIndex.cpp` 与 `DesignCodeGenerationService.cpp` 的唯一编译所有者；
Designer、CLI 与测试只能通过 `CuiCodeGenCore.lib` 复用实现，禁止重新把源文件加入各自 `ClCompile`。公开头路径
保持稳定，核心库通过项目引用明确依赖 `CuiRuntime`，四配置解决方案构建日志中两份实现都只能编译一次。
每个设计控件还必须在 Generated 基类中得到空初始化的强类型成员、const/non-const `GetXxx()` 访问器和
`ControlIds` 稳定 ID。访问器让业务代码直接使用 `x:Name` 的编译期类型，不得退回遍历 `Form::Controls`；
`ControlIds` 与动态 `RuntimeDocument::ReferenceByDesignId<T>()` 使用同一身份。名称转为 C++ 标识符后必须在
整个类作用域全局去重，不能让显式数字后缀与自动后缀生成重复成员。
同一生成头还必须提供零所有权的 `ClassReferences<TDocument>` 模板：仅为正稳定 ID 的控件生成强类型
`GetXxx()` 当前实例解析和 `ReferenceXxx()` 持久引用，后者必须通过 `ReferenceByDesignId<T>()` 跟随原位、重组
和完整替换。模板定义不得直接包含或链接 CuiRuntime，使静态消费者只有在实际以 `RuntimeDocument` 实例化时才
增加运行时依赖。视图必须保存 `document.Reference()` 返回的弱 `RuntimeDocumentRef` 而非裸文档指针，公开
`operator bool` / `TryDocument()`；文档移动时视图继续跟随，销毁后 `GetXxx()` 与 `ReferenceXxx()` 均返回空。
真实编译样例必须加载 XAML、实例化该视图并验证 Reload、移动与销毁边界。
有事件的生成头还必须声明 `ClassEventSink`：唯一处理函数按目录推导的参数类型成为纯虚接口，生成的静态 Form
继承并实现同一接口；动态控制器可独立继承并使用公开 `RegisterDynamicEventHandlers`。普通控件路由必须从真实
Event descriptor 的 declaring-owner 类型和字段生成，Control 公共事件只注册一次 UI_Base wildcard；Form 路由
使用同一目录成员，自定义路由只使用固定 signature preset。注册方法必须调用 `RegisterScopedBatch`，同名路由
合并按 handler/type/event/custom identity 去重，不能因两个控件共享同一函数而生成必然失败的重复注册。
Sink 必须不可复制/移动，并把租约和每次注册独立的生命周期令牌作为一个状态自动持有；替换注册、显式解绑或
析构时应先使弱令牌失效再移除路由。由于 RuntimeDocument 已创建的 EventConnection 不归注册表所有，生成的
`std::bind_front` 外必须有弱令牌 guard，使旧订阅在控制器失效后成为 no-op，禁止留下绑定到悬空 `this` 的回调。
代码持久化判断用户处理函数是否存在时不得使用 `Class::Handler` 子串搜索；至少要按 C++ token 边界识别
类外 `Class :: Handler (...) {}` 及精确用户类体内的 `void Handler(...) {}`，跳过行/块注释、字符/普通字符串
和 raw string，并区分函数名前缀。扫描只决定
是否追加缺失桩，不得重写或删除既有用户函数体。真实同名定义还必须按参数类型与当前事件声明匹配；空白和
参数名不同仍应识别。兼容定义必须是能覆盖生成虚函数的非静态、非 cv/ref `void` 成员；错误返回类型、
`static`、`const`/`volatile`、ref 限定、删除定义或参数类型不同都必须在首次目标替换前拒绝，不能因只命中
函数名而静默调用 Generated 空钩子，也不得进入候选或显式函数体迁移。类内 `noexcept` 和尾置 `-> void` 合法。
生成校验、逐事件 Designer 诊断与源码定位必须共用 `CppUserCodeIndex`；禁止维护三套逐渐漂移的注释/string/raw
string 跳过与参数 token 规则。索引必须同时返回定义总数、兼容定义数、首个定义行和首个兼容定义行：事件行据此
联合用户 `.h/.cpp` 的结果显示未关联/检查中/已实现/待生成/源文件缺失/签名错误/重复定义，筛选也应命中状态与诊断。已实现且整体生成
状态 Current 时双击应直接导航；缺失定义先生成；签名错误或多个相同签名定义必须绕过必然失败的生成并直接
定位现有定义。同名重载导航优先精确兼容行，只有没有兼容定义时才回退首个同名定义供修复。文档状态防抖、
窗口重新激活和生成完成均需刷新检查，并通过正常 PropertyGrid reload 保留分组/滚动状态。
类内兼容定义存在时 `.handlers.g.inc` 必须省略会与定义冲突的同类声明；解绑后其余声明仍需保留。
事件下拉除文档索引外还必须使用同一 `CppUserCodeIndex` 联合枚举用户 `.h/.cpp` 中尚未绑定的兼容成员；候选只接受
当前类下恰有一个精确签名定义的合法处理函数名，必须排除构造函数、`operator`、错误签名、重复兼容定义以及
注释或字面量伪代码。若候选名已被文档中的另一事件签名占用，不得显示为可选项；默认名和当前值仍排在最前。
索引解析类身份时必须把全限定定义、逐层 `namespace` 与 C++17 嵌套 namespace 块规范到同一限定名称；只有
当前 namespace 路径与剩余类限定段拼接后精确等于 `x:Class` 才能命中。匿名 namespace、函数/类体内伪匹配
及相邻命名空间中的同名类不得被接受；该规则必须同时覆盖生成复用、事件诊断、候选发现和源码跳转。
共享索引还必须以位置保持的掩码排除所有预处理指令及续行宏，解析确定的字面量 `#if 0` / `#if 1` 嵌套、
`#elif` 与 `#else` 失活分支；未知宏环境必须保守保留所有可能分支，不得猜测编译配置。掩码不得改变字节偏移、
CR/LF 或行号，使源码跳转和函数名 token 迁移仍能安全作用于原始用户文件。
用户头类身份也必须复用 `CppUserCodeIndex` 的同一预处理与 namespace 作用域，不得保留 CodeGenerator 私有
tokenizer。只有精确 `x:Class` 作用域中恰有一个类体且直接基类包含同 namespace 的 `LeafGenerated` 才通过；
导出宏、`final`、访问说明、多基类和全限定基类应允许，失活分支、相邻 namespace、错误基类及重复类体必须
在五文件写入前阻塞。
已有用户文件还必须在首次目标替换前验证类身份：用户头需声明当前类并继承当前 `Generated` 基类；用户头与
源文件合计必须恰有一个可用默认构造函数。类体内联、`= default` 和源文件外部定义均合法，`= delete`、重复
定义或已有源文件却无构造定义必须阻塞；源文件缺失但头中已有定义时，重建源不得追加第二个构造体。只出现
Designer marker 但类属于旧 `x:Class` 时必须拒绝整批导出，不得留下新 `.g.*` + 旧用户类。
解决方案中的 `CuiStaticGeneratedSample` 必须真实编译并运行一套命名空间限定生成代码；核心测试还须把其
`.g.h/.g.cpp/.handlers.g.inc/.h/.cpp` 与临时生成结果对比，样例手工漂移和生成器不可编译都要成为门禁失败。
一次代码生成涉及的所有文件必须先完成同目录临时写入与 flush，再开始批次提交。中间目标锁定、rename 或写入
失败时，要逆序恢复所有已替换的旧文件并删除事务中新建的目标；回滚失败必须连同备份路径明确报告，不能用
“单文件都原子”掩盖跨文件的新旧混搭。用户文件标记、事件签名及所有内容预检必须发生在首次目标替换之前。
生成计划必须在读取用户代码前捕获五目标的存在性和精确内容，并把此前置条件传到原子批次；预写入、逐目标
提交及备份后任一复核不匹配都必须整批中止，禁止覆盖 IDE/外部进程在计划后写入的内容。事务回滚也必须要求
目标仍等于本事务提交结果；若回调期间出现外部修改，应保留修改、报告恢复冲突，不得用旧快照强制覆盖。
交互导出还必须把五文件提交与外部 code-behind 文档事务协调：关联回调失败或抛异常时，按生成前快照原子恢复
既有内容并删除本次新建目标；快照读取失败必须在生成前中止，恢复失败必须明确报告而不能谎称整体回滚成功。
常规文件型 Form 宿主优先使用不可移动的 `RuntimeDocumentSession` 组合文档、注册表和 watcher；session 不得
创建隐藏线程或吞掉 `RuntimeDocumentWatchResult`，首次挂载必须保持 `Load*IntoForm` 的原子性。Form 与回调捕获
对象必须比 session 活得更久；成功挂载后记录所属 UI 线程，跨线程 Poll 必须在接触控件前明确拒绝。内存文档、
自定义根宿主和多文档调度继续使用低层组合接口。

`RuntimeDocumentLoader::Reload*` 的原位路径覆盖通用标量/元数据属性、Binding/DataContext Schema、文档样式、
控件事件映射和不改变窗体名称/窗体事件的显示属性。提交前必须完整材质化候选树，并为 Local 属性值源、绑定配置、
DataContext、样式表、事件映射和连接保存可恢复状态；任一步失败都要恢复旧状态。拓扑或控件专用 `Extra` 数据
变化应先完整材质化候选树，再按稳定 ID 移植载荷与内部拓扑完全相同的最大旧子树，并以 `Recomposed` 区分于
纯原位更新和 `Replaced`；重排、增删及父容器替换不得销毁无关子树。移植必须保留普通容器、TabPage、Split
内部面板和 ToolBar 尺寸覆盖的拥有权语义，Binding/事件/样式任一步失败都要逆序恢复旧树。没有可复用子树、
字体所有权、未知属性袋以及被活动 Binding 占用的持久化属性仍走完整候选替换。通过
`TransferRootControlsTo()` 转交根时必须保留 `RuntimeDocumentRootHost`：先原子分离旧根并记录宿主槽位，候选
完全就绪后才提交 Replacement；任何失败都以 Rollback 精确恢复旧根及宿主自有控件相对顺序。Form 内置适配器
必须支持多根不连续槽位回滚和候选提交拒绝。通过旧 `ReleaseRootControls()` 手动转交且没有适配器时，不得执行
隐式拓扑重组或整树替换。
`ApplyFormProperties()` 与 `BindFormEvents()` 必须保留各自的非拥有 Form 目标；重组/替换应继承窗体事件解析器，
并把候选显示属性、候选窗体事件连接和候选根提交放在同一事务末端。任一解析器或根宿主拒绝都必须恢复旧显示
状态（包括默认/借用/拥有字体语义）并保持旧事件连接。被记录的 Form 必须比 `RuntimeDocument` 活得更久。
动态窗口首次创建优先使用 `Load*IntoForm()`；解析/材质化、DataContext、控件事件、Form 显示、Form 事件与根
挂载必须全部成功后才替换输出。分阶段宿主可使用 `Load*()` + `AttachToForm()`，但 Attach 只接受尚无独立 Form
附件且仍拥有根的文档。Attach 失败必须恢复显示/字体、断开候选 Form 连接并保留完整根森林，以便同一文档重试。
已有外部 Form 或根宿主附件的输出不得被 `Load*()` 直接覆盖，必须明确拒绝并引导调用 `Reload*()`；没有保存
Form resolver 的宿主遇到后续新增 Form 事件时也必须回滚 Reload，不得静默留下未连接事件。
动态业务代码优先保存 `RuntimeControlRef<T>` 而非跨重载缓存裸控件指针；稳定 ID 与名称查询必须走文档索引，
不能每次遍历根树。引用在每次访问时重新解析当前稳定 ID，必须覆盖整树替换后指向新实例、节点删除后为空，
以及拓扑重组后继续指向保留实例的门禁。引用必须通过弱文档生命周期状态避免悬空：文档销毁后 `Get()` 返回
空，移动构造时引用跟随目标；Loader/Reload 使用的目标移动赋值必须保留目标已有引用，赋值源已有引用应失效。
引用不得延长 `RuntimeDocument` 或控件的生命周期。

`RuntimeDocumentFileWatcher` 必须保持无后台线程、无消息泵依赖和非拥有语义；宿主在控件所属 UI 线程主动
`Poll()`。文件签名至少包含文件身份、最后写入时间和大小，以同时识别直接写入与原子替换。每次签名变化都要
重新开始防抖；稳定失败签名不得在每个 tick 重复解析/重载，后续新签名或显式 `RequestRetry()` 才能再次尝试。
文件暂时不存在或不可读时同样先防抖，失败不得破坏当前 `RuntimeDocument`。
`RuntimeDocumentSession` 在启用初始监视时应先建立候选 watcher，再提交 Form；挂载失败后不得留下源路径、
监视状态、Form 显示/事件或根附件，同一个 session 在补注册处理函数后必须可以重试。稳定签名重载因未知处理
函数失败后，向共享注册表追加路由并 `RequestRetry()` 应在原界面保持活动的前提下完成下一次事务重载。

## 10. 拖放、IME、Web、媒体与系统能力

这几个能力是 CUI 相比简单自绘 UI 更容易被忽略的部分：

- `TextBox` / `RichTextBox` / `PasswordBox` 等已考虑 IME 输入
- `Form` 和控件都支持文件/文本拖放事件
- `WebBrowser` 基于 `WebView2`
- `MediaPlayer` 负责媒体播放
- `NotifyIcon` 与 `Taskbar` 可做系统托盘和任务栏状态集成

使用这些能力前，先确认宿主项目是否真的启用了相应依赖，尤其是 `WebView2`。

## 11. 自定义控件应该怎么做

### 11.1 简单扩展

如果只是：

- 改绘制
- 改少量状态
- 在默认点击流程前后插入逻辑

优先：

1. 继承现有控件
2. 覆写 `Update()`、绘制相关逻辑或默认 hook
3. 复用基类 `ProcessMessage()`

### 11.2 复杂扩展

只有在下面这些场景里，才优先考虑重写 `ProcessMessage()`：

- 需要自己管理消息路由
- 需要维护交互状态机
- 需要坐标空间换算
- 需要复杂键盘/鼠标/选区/滚动逻辑
- 需要和 `Form`、系统组件或外部宿主深度协作

具体判断标准看：

- `GUI/DefaultProcessMessageConvention.md`

一句话原则：

- “只是改点击行为”不要重写整套消息分发
- “真的要接管交互状态机”才重写 `ProcessMessage()`

设计器工作流大致是：

1. 拖放控件
2. 编辑属性
3. 保存设计文件
4. 生成 C++ 代码
5. 运行时仍由 `CUI` 负责真正显示与交互

对 Agent 的建议是：

- 如果宿主项目已经用设计器生成代码，就沿着生成代码继续维护
- 如果宿主项目纯手写 UI，不要强推设计器
- 设计文件和生成代码通常都应纳入版本控制

## 12. 集成方式不要写死

其他项目接入 CUI 时，可能出现这些形态：

- 直接把 `CUI` 与 `Graphics` 源码纳入工程
- 预编译成静态库后引用
- 用已有的 monorepo / vendor 目录嵌入
- 只抽取部分控件和运行时代码

所以 Agent 在修改宿主项目时应该先识别：

1. 头文件根路径是什么
2. `Graphics` 是源码依赖还是二进制依赖
4. 是否存在本地封装层或二次包装

不要直接把当前仓库的相对路径拷到其他项目里。

## 13. Agent 实战规则

当你在其他项目里实现 CUI 界面时，优先遵循下面这组规则：

1. 先判断项目以手写 C++、动态 XAML 还是静态生成代码为 UI 真源，再找已有 `Form` 宿主。
2. 先确认该项目用的是绝对布局、布局容器，还是两者混用。
3. 事件优先沿用该项目已有的 lambda、生成式 `std::bind_front` 或 XAML 命名事件注册风格。
4. 控件主题优先复用项目已有的 `ControlStyleSheet`，窗口框架继续使用 `ApplyThemeFrame(...)`；
   只有未注册元数据的旧字段才直接设置 `BackColor` / `ForeColor` 等值。
5. 需要新页面时，优先参考 `CUITest/DemoWindow.cui.xaml` 中最接近的控件组合；运行时数据参考
   `CUITest/DemoWindow.cpp`。
6. 需要自定义控件时，先判断能否只覆写默认 hook，而不是直接重写 `ProcessMessage()`。

## 14. 最短结论

把 CUI 当成“原生 C++ 的、API 风格接近 WinForms 的 Windows GUI 框架”来用，通常不会错。

最重要的四件事只有这些：

1. 用 `Form` 做顶层窗口，用 `Control` / `Panel` 组织控件树
2. 小型静态界面可用 `AddControl(...)`，可编辑或数据驱动界面可用 CUI XAML
3. 静态事件用 `OnXxx += ...`，XAML 命名事件由运行时注册表解析
4. 用 `Show()` + `Form::DoEvent()` 跑消息循环

如果需要进一步确认完整动态 UI 的真实用法，先看 `CUITest/DemoWindow.cui.xaml`，再看负责运行时数据和业务事件的
`CUITest/DemoWindow.cpp`；两者共同构成当前仓库的完整参考实现。
