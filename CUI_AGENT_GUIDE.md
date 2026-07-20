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

有效值优先级固定为 `Local > Binding > Style > Theme > Inherited > Default`。使用带
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
Binding API 的省略值必须保持为 `DataSourceUpdateMode::Default`，并在安装时通过目标
`BindingPropertyMetadata::DefaultUpdateMode()` 解析为具体策略；控件订阅器不得收到 `Default`。
XAML 规范输出使用 `UpdateSourceTrigger=PropertyChanged/LostFocus/Explicit`，旧 `UpdateMode` 仅作兼容输入。
声明组件用 `DefaultUpdateSourceTrigger` 发布具体默认值，禁止把 `Default` 写成属性元数据默认策略。
`Explicit` 必须具备可调用语义：普通与 MultiBinding 均公开 `UpdateSource()`/`UpdateTarget()`，通用调用方优先使用
`BindingCollection` 的按目标属性入口。MultiBinding 手动提交必须继续刷新内部 Explicit 子 Binding，并把
ConvertBack/源写入失败保留在表达式和集合的 `LastError` 中；失败不得修改未到达的源值，且修正后必须可重试。

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
  Selected；
- `PropertyConditions`：按目标控件的可读、可观察属性元数据读取并类型化比较，所有条件必须同时匹配；
- `DataConditions`：按 DataContext 点分路径读取值，所有条件必须同时匹配。

级联特异性顺序是 `Id > Class/状态/属性条件/数据条件 > 精确类型 > 通配规则`，特异性相同时后加入的规则胜出。
Hover、Focus、Pressed 和 Checked 会从控件事件自动刷新；Disabled 也会从有效 Enable 状态
解析。显式状态使用 `SetStyleState(...)`。样式表资源通过 `SetResource(...)` 和
`ControlStyleSetter::Resource(...)` 引用，键名不区分大小写。规则或资源修改会通知所有附着
控件热刷新；缺失资源会出现在 `Resolve(...)` 的问题列表中，并使显式刷新返回 false。
不存在、不可写或无法按元数据转换/Coerce 的属性也会形成解析问题；无效声明不参与级联，
因此较低特异性的有效声明仍可作为回退。

DataTrigger 必须针对每个目标控件的有效（本地或继承）DataContext 求值。目标控件以稳定的
`DataContextSource()` 为根分别订阅点分路径每一级的 `PropertyChanged`；叶值变化、中间对象替换、
继承来源切换和清除 DataContext 都只触发相关目标重新解析。共享样式表不得把某个 DataTemplate 项的
DataContext 保存成全局可变状态；`ControlStyleSheet::SetDataContext(...)` 仅保留为目标没有有效
DataContext 时的兼容回退。比较前把期望值转换到实际值类型，转换或读取失败只表示条件不匹配，不能让整份样式失效。

Style `Trigger` / `MultiTrigger` / `DataTrigger` / `MultiDataTrigger` 的 `EnterActions`、`ExitActions` 使用既有完整
Storyboard 模型，但定义共享、运行状态绝不能共享。只有类型/Id/Class 静态选择器先匹配目标时，才为该目标编译
动作作用域；状态、目标属性和数据条件共同决定作用域的 Active 边沿。初始 true 和 false→true 执行 Enter，true→false 执行 Exit，
无边沿刷新不得重启时钟。每个目标、样式表来源和值源、RuleId 共同隔离命名时钟；规则消失、样式表替换或控件
销毁必须停止并释放作用域。Style 动画只能以匹配控件自身为目标，禁止 `TargetName`；Begin 名称在同一规则的
Enter/Exit 合集内唯一，控制动作也只在该合集解析。Style 时钟与组件 EventTrigger/VisualState 可共享动画泵和
`Animation` 值源，但索引、定义生命周期和失败回滚必须隔离。规范 XAML、v14、撤销内存估算、剪贴板资源闭包、
热重载和设计器预览必须保存同一语义。辅助静态生成不支持时必须明确失败，禁止生成缺少动作的近似代码。

`Button`、`TextBox`、`ComboBox`、`Panel`、`GroupBox`、`Expander`、`ScrollView` 的常用状态色、边框、圆角和间距已迁移到
属性元数据，并使用 `TracksLocalValue` 保持公开 setter 的 Local 语义。仍未迁移的旧外观字段继续直接设置；新增
可主题化属性必须先注册元数据并使用 `SetPropertyField(...)`，不要再为主题系统增加控件类型
switch。

Designer 选中控件后可直接编辑 `StyleId` 与 `StyleClasses`；后者使用逗号分隔，空白会被清理，
重复项由运行时去重。两项都必须随设计文档保存/加载，并由代码生成器输出
`SetStyleId(...)` / `AddStyleClass(...)`。

未选中控件时，窗体属性面板的“编辑文档样式表”用于维护 `DesignerStyleSheet`。资源值必须
显式声明 Bool、Int、Int64、Float、Double、String、Enum、Color、Thickness、Size、Length、Brush、Geometry 或 Transform 类型；
规则可组合类型、ID、多个 Class、必需/排除状态、`BasedOn` 及 Literal/Resource Setter。只有
`TargetType` 而没有 Id/Class/状态的规则是隐式样式。`BasedOn` 可引用命名 Id，或用 `{x:Type T}`
引用隐式样式；继承展开必须统一处理 TargetType 继承、基 Setter 在前/派生同名 Setter 覆盖、循环检测，
并让动态运行时、静态生成、Designer 预览得到相同结果。合并字典与本地声明重名时按源顺序由后者覆盖。
`Style.Triggers` 将 `IsMouseOver`、`IsKeyboardFocused`、`IsPressed`、`IsEnabled`、`IsChecked`、
`IsSelected` 规范为状态条件；其他 `Property` 必须解析到目标类型中可读、可观察且具有受支持值类型的属性元数据，
并用该元数据完成值转换与比较，禁止硬编码新的控件属性分支。`MultiTrigger` 必须在
`MultiTrigger.Conditions` 中声明至少两个 Condition，允许混合状态与普通属性，全部条件以 AND 语义合并；重复属性、
条件内部矛盾及与规则 Required/Excluded 的冲突必须在提交前拒绝。Trigger Setter 与普通 Setter 使用相同资源
解析和属性元数据验证，BasedOn 继承 Trigger，派生声明按源顺序在后。`DataTrigger` 首批只接受
`Binding="{Binding Path}"` 和字面 `Value`，路径必须走通用 Binding 路径规范化/校验；不得接受 Converter、
Mode/UpdateMode 或 StaticResource Value。`MultiDataTrigger` 必须在 `MultiDataTrigger.Conditions` 中声明
至少两个同类 Condition，所有数据条件按 AND 匹配；空列表、单条件和重复路径必须在提交前拒绝，不能降低为
多个彼此独立的 DataTrigger。动态运行时的
`BindDataContext`、设计时 DataContext、DataTemplate 项上下文与静态生成的 `BindData` 必须进入控件的统一
DataContext 继承链；禁止通过反复调用共享样式表的 `SetDataContext` 传递项上下文。
编辑器必须在提交前
调用统一的规范化与校验逻辑，成功后把运行时 `ControlStyleSheet` 递归附着到预览根控件；应用
失败时保留旧样式表。XML v14 和动态运行时必须保存并重建同一份语义，不能各自维护另一套解析规则。

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

复制/剪切/粘贴必须以 `DesignDocumentClipboard` 和规范 CUI XAML 为边界，禁止复制运行时控件裸指针或直接
克隆挂载对象。捕获多选时先消除已选祖先下的重复根，再包含每个根的完整普通/TabPage 子树；粘贴必须为所有
节点分配高水位之后的新 stable ID，生成大小写不敏感的唯一名称，并同步重写 ParentId、ParentRef 和
`TabName#pageN` 引用。片段根的目标必须显式建模并覆盖所有根：普通父级使用现有 stable ID，TabPage 使用已验证
的合成引用，SplitContainer 必须指定 First/Second；重复命令应从源文档恢复每个根自己的父级和区域，不能退化
为统一根粘贴。任何解析、验证、重映射或应用失败都不得修改源/目标文档。一次剪切或粘贴只能形成一个命令，
选择恢复到新根；系统文本剪贴板中的完整文档或控件片段均应走同一模型路径。
事件值重映射必须通过 `DesignerEventCatalog::ResolveHandlerName()` 与 `MakeDefaultHandlerName()` 判断语义：仅当
原值逻辑上解析为原控件的约定默认函数时，才随新控件名重写。子树内对该默认函数的显式共享引用必须使用同一
映射更新；legacy `1/true/yes/on` 所有者仍保留 Auto 语义。自定义/外部函数名和有歧义的旧默认名必须原样保留，
禁止仅按 `旧控件名_事件名` 字符串替换而误伤用户有意共享的处理函数。
剪贴板片段还必须是 Binding 自包含的：Capture 只投影已复制节点实际引用的源路径、全部点分父路径及源 Schema
中的类型/读写/通知能力，未声明路径用 `Unknown + R/W/O` 保持空 Schema 的宽松语义。Paste 只把目标缺少的路径
合并进候选文档，同名路径以目标 Schema 为权威并交由后续 Binding 材质化验证兼容性。目标 Schema 原本为空时，
必须先为目标既有 Binding 推导 `Unknown` 路径，再导入片段声明，避免从“无 Schema”切到“部分显式 Schema”后
破坏旧绑定。Schema 合并必须与控件树插入共用同一文档事务和 Undo/Redo，格式错误或合并校验失败保持输出不变。
样式依赖同样必须自包含且最小化：Capture 先展开 `BasedOn`，再仅保留静态 Type/Id/Class 选择器能匹配
已复制节点的有效规则，并按源顺序携带这些规则实际引用的资源。Paste 只有在目标相关规则序列及资源值完全一致时才可复用；否则必须为每个粘贴
节点生成不与目标冲突的私有 Id/限定 Class，重命名资源并重写 Setter 引用，同时保持动态状态、源特异性顺序和
同特异性源顺序。隔离规则不得改变目标既有节点的 Id/Class 或外观，也不得让重复粘贴复用前一次副本的私有名。
样式合并、节点属性重写和树插入必须先在候选文档中整体校验，再以同一命令提交或完整失败。
Designer 的工具栏与画布菜单必须共享同一个只读粘贴可用性查询，并监听 Windows 剪贴板格式更新；仅公布
`CF_UNICODETEXT` 但文本为空（例如某些位图源）的剪贴板不得误判为可用。通知到达时若源进程仍短暂占用
剪贴板，应做有上限的延迟重读，禁止无限轮询；严格文档事务期间所有粘贴入口必须同时禁用。非空外部文本的
语法和语义验证仍应推迟到实际粘贴，从而保留明确诊断和零修改失败语义。

粘贴放置语义必须保持三种模式互不混淆：普通 `Ctrl+V` 按 12 DIP 级联，`Ctrl+Shift+V` 保留片段根的局部位置且
不得消耗级联序号，画布“粘贴到此处”则把所有片段根的包围框左上角对齐到上下文点击点。点击点只能从视口坐标
经 `ViewToCanvasPoint()` 转换一次，再按命中容器的画布绝对位置换算为局部坐标；目标推断必须覆盖普通 Panel、
TabPage 合成引用和 Split First/Second，并保持多根之间的相对位置。偏移既要更新精确 `Props.location`，也要同步
手写 XAML 进入 `props.metadata` 的 `Left` / `Top`，防止材质化时旧元数据覆盖新坐标；任一整数溢出必须事务性
拒绝整次粘贴。命中布局托管容器时不得继续假设 X/Y 有效：Stack/Wrap/ToolBar 必须把点击点解释为同级插入边界，
Grid 必须写入命中 Row/Column，Dock 必须解析边缘/Fill 且不能让新停靠项意外成为 LastChildFill，RelativePanel
必须把平移后的根坐标转换成 Margin。模型层的可选插入序号以粘贴前同级集合为基准，同序号的多个片段根保持
片段顺序；越界或同一目标混用追加/显式插入必须事务性拒绝。布局托管目标还应清理会覆盖或误导的
`Canvas.Left` / `Canvas.Top` 元数据。三种模式都应分配唯一名称/ID、选择新根，并且一次操作只创建一条 Undo 记录。

实时 XAML 编辑器只承担源码输入、300ms 防抖验证/预览、错误定位和确定/取消，不得内建补全、
语法着色、查找替换、格式化、标签配对或多检查点历史；这些能力由未来的 Visual Studio/COM 宿主提供。
整个模态会话必须由一条严格文档事务包围：每次预览先在隔离模型中解析和验证，失败只更新诊断并保持
最后一次有效画布；应用候选失败必须恢复本次应用前文档和完整选择。确定提交最终端点，取消/关闭恢复
打开编辑器前的文档，逐次键入不得进入 Designer Undo 历史。最后有效源码只在完整预览成功后更新；
“恢复有效版本”通过 RichTextBox 的原子全文替换写成一个文本 Undo 单元。
语法与语义错误都必须通过 `XamlDocumentDiagnostic` 提供用户消息、1-based 行/列和 UTF-16 偏移。
XmlLite 的 UTF-8 字节列不得直接作为 RichTextBox 索引；语义错误通过只读源码标签扫描器映射到当前属性名，
其次映射到元素名。防抖验证不得自动移动光标；只有显式“定位错误”或 `F8` 才选中并滚动到诊断位置，
任何新输入或成功预览都必须清除旧诊断。对话框控件使用 Right/Bottom Anchor 时必须设置对应 Margin 锚距。
重复控件必须复用文档剪贴板模型但不得改写系统剪贴板。绝对布局副本通过 Location 偏移；RelativePanel 必须
把相同偏移写入 Margin，Stack/Wrap/Dock/ToolBar 必须按源 Order 后一位设置显式插入序号，多选非连续根时每个
副本仍应紧邻自己的源根，Grid 则保留源单元格。进入任何布局托管父级都要把 Location 归零并清理可能覆盖结果的
Canvas 定位元数据。对齐、分布、同尺寸和层级命令必须先捕获
`ControlPlacementCommand` 起点，成功修改后再捕获终点并以 `skipInitialExecute` 提交一条差量；捕获或入栈失败
必须恢复起点与完整选择。几何编排只接受同一非布局托管父级，对齐/尺寸以主选控件为基准，分布固定两端。
层级命令只能移动所选同级，必须把 sibling index 与 `ZIndex` 一同纳入快照，不能通过重写所有兄弟的 ZIndex
来“规范化”层级，否则会破坏用户显式值。内部复合控件子项不得被当成设计控件参与层级计算。

Designer 的工具栏、画布快捷键和右键菜单不得各自实现命令逻辑。Canvas 负责命中后更新选择并发布上下文菜单
请求；宿主只负责根据选择、事务和 Undo/Redo 栈刷新菜单项，再调用公开的 Copy/Cut/Paste/Duplicate/Arrange/
Undo/Redo API。右键命中已选成员必须保留多选并只改变主选，空白处清空控件选择；键盘菜单键与 `Shift+F10`
必须发布同一种请求。Undo/Redo 可用状态和显示标签必须来自命令栈，不能用 Dirty 或按钮点击历史推断。

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
是稳定快捷键，表头、筛选框和模式按钮的可访问名称必须随视图更新。多选事件页只能投影所有目标同名且精确签名相同的事件，编辑、重置与激活
必须原子作用到全部目标，不得暗中只改主选对象。只要当前筛选有可见事件，必须保留“生成/定位处理函数”显式操作行；其目标按最后选中的可见事件、
目录默认事件、首个可见事件的顺序解析。选中 Action 行不得覆盖最后事件选择，筛选或切换控件后不得使用已不可见/不支持的旧事件。`F12` 是该操作的稳定快捷键。

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
XAML → `DesignDocument` → 规范 XAML/v14 XML 往返、动态加载和失败回滚。静态代码生成已降为可选辅助能力，
不得反向限制动态 XAML 的类型系统。
`XamlDocumentSerializer` 只允许输出公开属性、Binding 标记扩展和公开属性元素。ComboBox/ListView
的旧式静态项使用直接内容；`ListBox` 只接受强类型 `ItemsSource + DataTemplate`，不得恢复 `ListBoxItem`、
`.Items` 或 `.Columns` 兼容分支。多集合控件使用 `.Columns` / `.Rows` 等显式属性元素。GridView/PagedGridView 共用
强类型 Column/Row/Cell 节点，媒体源使用 `Source`。新增结构类型必须同时补齐 Parser、规范 Serializer、
Materializer 和 Designer `BuildDesignDocument` 回存，不能只让动态加载识别。可选 CodeGenerator 只处理它明确
支持的静态子集；遇到 `ComponentDefinition`、`DataType`、`DataTemplate` 等动态类型系统内容必须明确拒绝，
不得迫使 XAML 降级。
不存在公开语法的残余 `Props` / `Bindings` / `Extra` 必须明确失败，禁止恢复 `d:` 通用值袋或静默丢字段。
普通颜色使用可读 `#AARRGGBB`，浮点几何和布局值不得整数化。
`Control.Foreground` 的 Solid/Linear/Radial/Image Brush 是公开结构对象；ImageBrush 的资源 URI、
Stretch、Alignment 与 Opacity 必须在动态加载和 Designer 回存中一致。静态辅助生成若声明支持该文档，也必须
保持相同结果。自定义控件若只改变画刷，应复用基类 Brush-aware 绘制，
不得复制整段 `Update()`。`Control.RenderTransform` 同样是公开结构对象，Matrix/Translate/Scale/Rotate/Skew
按声明顺序组合，`RenderTransformOrigin` 使用相对控件边界的坐标；控件自身变换与容器提供的后代视图变换
必须分层组合，输入坐标必须通过最终矩阵求逆，脏区、DComp、可访问性边界和 Designer 回存必须
使用同一结果。`Control.Clip` 的 Rectangle/Ellipse/Path/GeometryGroup 是控件局部 DIP 中的公开结构对象；
PathFigure 支持 Line/Bezier/QuadraticBezier/Arc，所有 Geometry 均可拥有独立 `Geometry.Transform`；
父控件 Clip 必须约束独立绘制的后代，渲染、普通/虚拟命中、可访问性和 Designer 回存必须使用
同一几何语义。动画仍只有在这些路径全部对齐后才能声明为支持，禁止只接受标签却静默忽略。
`ResourceDictionary.MergedDictionaries` 的 `Source` 必须统一经过 Application 配置的 `ResourceResolver`；
禁止在 Parser、Materializer 或画刷层重新拼磁盘路径。当前内置 `FileResourceSource` 以声明字典的目录解析相对
URI，也支持应用搜索目录；后合并项覆盖前项，本地项覆盖合并项。自定义 `IResourceSource` 必须返回字节、稳定
Identity、逻辑 BaseUri 和可选 WatchPath，为后续产品包资源保留扩展点。模型必须保留外部来源和本次解析上下文，
使规范 XAML 回存不展开外部字典；循环引用和缺失资源必须事务性拒绝，原生 XML、剪贴板依赖裁剪、材质化和
静态辅助生成在支持的文档子集内也不得丢失或误用资源基目录。文件监视必须覆盖主 XAML、递归字典和图片依赖，
并支持删除后恢复。
`{DynamicResource Key}` 是属性值表达式，不得在 Parser 中降级为解析时字面量。首批合法位置为可写控件属性和
Style/Trigger Setter；`Style`、`BasedOn`、`ItemsSource`、模板选择等结构引用继续只接受 `StaticResource`。
运行时表达式占用 Local 值槽，按“当前控件局部字典 → 逻辑父链局部字典 → 文档样式表 → Application/主题样式表”查找，资源表替换、同表资源修改、换父级或
继承到新子控件时必须重新求值；键暂时缺失时保留表达式并显露较低值源，普通 Local 写入或 ClearValue 会移除表达式。
规范 XAML、v16、设计器捕获/恢复、资源重命名、剪贴板闭包、事务热重载和辅助代码生成必须保留动态/静态身份。

控件级 `<Owner.Resources>` 是词法作用域，必须保存在拥有它的 `DesignNode`，不得在 Parser 或 Designer 捕获时
扁平化到 `Form.Resources`。运行时沿逻辑 `Parent` 查找，移动子树后必须重新求值。v15 引入强类型值资源和文件型
MergedDictionaries；v16 进一步允许局部 `Style`。局部规则必须与文档及祖先规则共同解析 `BasedOn`，运行时按
特异性合并并让近端规则在同分时胜出，Data/Property Trigger 的订阅和动作时钟也必须按作用域重建。文档样式变化若
影响已展开的局部 `BasedOn`，必须重建局部运行时字典，不能只替换根样式表。DataTemplate/ComponentDefinition 等尚未
实现词法对象索引的结构资源仍须明确拒绝，不能悄悄注册成全局资源。

窗体属性页的结构化数据资源编辑器是 `DataType`、`DataList`、`DataRecord` 与 `DataTemplate` 标识的统一入口。
它必须先在候选 `DesignDocument` 上完成类型验证和规范化，再作为一条文档命令提交。类型、字段、列表键或模板键
重命名必须原子重写 Schema、记录路径、模板 Binding 和控件资源引用；有依赖的删除必须拒绝，合并字典资源只读。
`DataTemplate` 视觉树继续由统一画布/XAML 编辑，不建立第二套控件编辑器。

选择控件的数据语义必须以记录为真源：`DisplayMemberPath` 只负责显示，`SelectedValuePath` 决定强类型
`SelectedValue`；不得从显示文本反推业务值，也不得把数值/布尔主键字符串化。`ComboBox`、`ListView`、
`ListBox` 必须支持同一 TwoWay Binding、记录字段观察和集合重载规则。`ListBox` 必须沿
`ItemsControl → Selector → ListBox` 复用 ItemTemplate、SelectedItem/SelectedValue 与 SelectionChanged；不得
重新继承 ListView。生成项必须由非文档节点 `SelectorItem` 承载，以只读 `IsSelected` 和标准样式状态支持
`ItemContainerStyle` Setter/Trigger；不得把容器恢复成 XAML 子节点。ItemsHost 必须位于 ScrollView 视口内，
滚轮、键盘导航和 BringIntoView 使用同一浮点布局几何。项容器样式引用必须覆盖 Parser、Serializer、属性栏、
Materializer、剪贴板依赖和热重载。空选择为 `SelectedIndex=-1`，禁止为兼容旧行为自动选中第 0 项。静态 DataList 或带 ItemType 的 DataContext 集合都必须在文档提交前验证这些路径。
`ItemContainerGenerator` 是 ItemsSource 索引到已实现/回收容器的唯一映射层。精确 Add/Remove/Move/Swap/Replace
必须保留未受影响的实例并同步 `SelectorItem.ItemIndex`、host 排列和选择身份；不得重新引入“任何集合变化都
RebuildGeneratedItems”的路径。Reset 或字段不完整的第三方通知才允许候选式全量重建。固定项高虚拟面板必须
以记录为滚动锚点修正视口上方的结构变化，不能只夹取旧像素 offset。无模板 DisplayMemberPath 的字段变化只
替换对应容器，不能摧毁整个列表。
`CollectionViewSource` 是列表筛选、排序和 CurrentItem 的唯一声明式视图层。Source 允许 DataList、视图链或
DataContext BindingList；FilterDescriptions 必须按 DataType 强类型转换并以 AND 组合，SortDescriptions 必须
稳定地按声明顺序比较。投影刷新必须发布精确 Add/Remove/Move，禁止用 Reset 掩盖可计算差异。该资源必须进入
规范 XAML、v14 XML、合并字典来源、属性候选、剪贴板、RuntimeDocument DataContext 重绑定和热重载。
分组只能通过有序 `GroupDescriptions` 与键控 `GroupStyle` 表达；HeaderTemplate 必须使用保留的
`CollectionViewGroup`，头部包装不得改变 SelectorItem 身份。命名 `AggregateDescriptions` 必须按源 DataType
校验并实时刷新 `Aggregates.*`。固定项高 VirtualizingStackPanel 必须使用项/组头段偏移索引，滚动锚点、extent、
可见区与 BringIntoView 都必须计入 `HeaderHeight`，不得退回 `index * pitch`。
产品级扩展的主路径是 `ComponentDefinition`，不是外部 C++ 类型反射。`DesignNode::ComponentType` 保存命名空间
身份，`DesignComponentDefinition::BaseType` 保存内置布局/渲染宿主；声明属性必须安装为实例级
`BindingPropertyMetadata`，与原生属性共享值来源、Binding、样式和 Designer 发现。组件实例不需要
`d:CppType`、头文件、构造约定或运行时控件工厂。完整分层、模板/事件/Native Behavior 约束见
`CUI_XAML_COMPONENT_ARCHITECTURE.md`。
组件属性的 `BindsTwoWayByDefault` 与 `DefaultUpdateSourceTrigger` 必须分别进入 Mode 和更新触发器元数据；
两者是独立维度，不能因目标默认 OneWay 就丢失更新策略，也不能在 Binding 显式覆盖后继续强制组件默认值。
组件属性的 `ReadOnly` 是公开写入边界，不是“没有 setter”或属性栏隐藏标记。只读属性必须继续可读、可观察、可继承，
并可作为 Binding/TemplateBinding 源；实例字面值、Style Setter、Binding/MultiBinding 目标、普通 set/clear API 必须
统一拒绝。属性栏显示禁用行，样式和绑定目标候选过滤。动态组件行为仅通过
`TrySetReadOnlyPropertyValue`/`ClearReadOnlyPropertyValue` 的 key-equivalent 能力写入 Local 层，清除必须恢复
Inherited/Default；禁止重新引入可从 XAML 或通用元数据 setter 调用的旁路。`ReadOnly` 不能和
`BindsTwoWayByDefault` 或非 PropertyChanged 的默认更新触发器组合。
声明组件的可选 C++ 扩展只能通过 `DeclarativeComponentBehaviorRegistry` 按精确 QName 附加
`IDeclarativeComponentBehavior`；工厂不得创建/替换 Control，也不得发布第二套属性或事件 schema。Attach 必须发生在
完整模板、内容 Presenter、样式和模板 Binding 安装之后；局部 `x:Name` 只能通过宿主的 template-part 查询访问，内容槽
通过声明属性名查询。宿主独占行为寿命并在模板子树销毁前 Detach；原位/重组复用不得对废弃候选树执行应用 Attach，
显式注册表替换必须走完整候选与失败回滚。Behavior 只可补充事件订阅、只读状态、声明事件、宿主消息预处理、最终
overlay 和 DPI/设备通知，不能接管 Measure/Arrange。需要应用主导测量/渲染/复杂输入时使用 `NativeSurface`。
组件属性当前允许标量、封闭字符串 Enum、Color、Thickness、Size、Length 以及结构化 Brush、Geometry、Transform；对象值必须把具体运行时类型写入动态属性元数据，
并按该类型严格转换，禁止退化为可接受任意内容的 Object。组件 QName 可作为 Style TargetType；运行时选择器必须
同时核对命名空间/类型名和 BaseType，组件 Setter/Trigger 的属性校验必须使用安装了组件契约的 probe。
组件视觉子树必须声明为 `ContentProperties`，模板用 `ComponentSlot.Presents` 显式承载。公开子节点的
`DesignerParent` 是组件实例，运行时 `Parent` 是生成的 Presenter；布局快照、层级拖放、剪贴板、序列化与热重载
不得把两者混同。Single 槽在事务提交前检查占用，普通容器上的槽名必须清除。视觉内容槽不能冒充 Items 数据 schema。
组件事件来自 `DesignComponentDefinition::Events`，不是 C++ Event 字段。实例事件值始终是处理函数名；
模板只通过显式 `{RaiseEvent Name}` 转发，运行时组件实例必须安装并校验声明事件及 payload 类型。
应用使用 `RuntimeEventHandlerRegistry::RegisterComponent(...)` 注册稳定的
`void(Control*, DeclarativeEventArgs&)` 处理器；不得从 XAML 接受任意 C++ 参数列表。
组件事件 `RoutingStrategy` 只能是 Direct/Bubble/Tunnel，身份必须包含所有者组件 QName，禁止只按事件局部名路由。
附加处理器使用 `prefix:Type.Event="Handler"`，模型持久化必须去前缀并保存稳定 QName；规范输出再按当前资源前缀生成。
sender 必须是 CurrentTarget，OriginalSource/Source 是 Raise 的组件宿主；Handled 不截断路由快照，但普通注册必须跳过，
只有显式 `HandledEventsToo` 可继续接收。附加事件必须进入事件索引/改名、剪贴板组件依赖、规范 XAML/v14、Designer
捕获、原位热重载和失败回滚；不得只在运行时添加一条旁路。
组件视觉状态只能在模板根的 `VisualStateManager.VisualStateGroups` 中声明。每组必须恰好一个无触发器回退状态；
同状态多个 StateTrigger 是 AND，同组条件状态按声明顺序优先，首批禁止混合 StateTrigger/EventTrigger。
EventTrigger 必须引用组件自身声明事件；Setter.TargetName 必须为空（组件宿主）或解析到模板局部 namescope，值必须
按目标属性元数据强类型校验。不同组不得拥有相同“目标 + 属性”。活动 Setter 与状态 Storyboard 必须使用独立且高于
Local 的同一个 `VisualState` 值源，离开状态只清除此层并恢复下层来源。
	Storyboard 基础批次允许有限 Double/Color/Thickness/Point/Vector/Rect/Size/Matrix 动画、必填 Duration、可选 From/To/By/BeginTime 和
	Quadratic/Cubic/Sine 缓动。端点必须按 WPF Automatic/From/To/By/FromTo/FromBy 解析：状态缺省来源是进入时当前值、
	缺省目标是下层基础值，Transition 两者均取过渡开始当前值；To 与 By 共存时 To 优先但不得丢弃 By 作者表达。By 只做
	类型转换再与来源相加，禁止提前套用绝对值 Coerce，最终动画帧仍走目标元数据 Coerce。完成后 HoldEnd，系统禁用动画时
	直达终值。TargetProperty
除直接属性外，已注册对象适配器首先处理具名模板部件上的
`(Control.RenderTransform).(TransformGroup.Children)[n].(TransformType.Property)`，且只能定位模板已声明的
Translate/Scale/Rotate/Skew 数值成员，或由 MatrixAnimation 定位 `MatrixTransform.Matrix` 强类型末端。通用 PropertyPath 层只解析语法，对象类型校验必须留在适配器；同状态多末端
必须合成一个完整 Transform 值；以及 RectAnimation 使用的
`(Control.Clip).(RectangleGeometry.Rect)`，目标必须显式持有 RectangleGeometry，每帧只替换 Rect 并保留同根圆角/变换。
第三个适配器处理 GradientStop：ColorAnimation 使用
`(Control.Foreground).(GradientBrush.GradientStops)[n].(GradientStop.Color)`，DoubleAnimation 使用对应的 `Offset`；
目标必须显式持有线性/径向 Brush 和有效 Stop 索引，同根多个 Stop/成员必须基于一份完整 Brush 合成后写回，禁止丢失其他 Stop、
坐标或 Opacity。Offset 的绝对端点限制为 0..1，By 可为有符号有限增量，最终写回按属性语义 Coerce。
第四个适配器处理 Brush Transform：DoubleAnimation 或 MatrixAnimation 使用
`(Control.Foreground).(Brush.Transform|RelativeTransform).(TransformGroup.Children)[n].(TransformType.Property)`；
目标必须显式持有对应类型 Brush、Transform 和操作，具体 Brush 所有者规范化为 `Brush`。Transform 与 GradientStop
末端必须共享 Foreground 根值合成，属性元数据的 Brush 相等比较也必须包含两套 Transform，否则纯变换帧会被误判为未改变。
第五个适配器处理 Geometry Transform：DoubleAnimation 或 MatrixAnimation 使用
`(Control.Clip).(Geometry.Transform).(TransformGroup.Children)[n].(TransformType.Property)`；目标必须显式持有
Rectangle/Ellipse/Path/GeometryGroup、LocalTransform 和匹配操作，具体 Geometry 所有者规范化为 `Geometry`。它与
RectangleGeometry.Rect 动画必须在一份 Clip 根值上合成并保留形状、圆角、填充规则和未命中操作。
第六类适配器处理 Brush 公开成员：ColorAnimation 定位 `SolidColorBrush.Color`，DoubleAnimation 定位通用
`Brush.Opacity` 和 `RadialGradientBrush.RadiusX/RadiusY`，PointAnimation 定位 LinearGradientBrush 的
`StartPoint/EndPoint` 与 RadialGradientBrush 的 `Center/GradientOrigin`。Opacity 绝对端点限制为 0..1、半径绝对端点
必须非负，By 可为有符号有限增量；这些末端必须与 Stop、Transform 在同一 Foreground 根值上合成。
第七类适配器处理 Geometry 公开成员：RectAnimation 定位 `RectangleGeometry.Rect`，DoubleAnimation 定位
RectangleGeometry/EllipseGeometry 的 `RadiusX/RadiusY`，PointAnimation 定位 `EllipseGeometry.Center`。具体 Geometry
所有者必须与实际 Clip 类型一致，半径绝对端点必须非负而 By 可为有符号有限增量；这些末端必须与 Geometry.Transform
在同一 Clip 根值上合成并保留未命中的形状数据。
第八类适配器处理 PathGeometry 的索引对象图：先经
`(Control.Clip).(PathGeometry.Figures)[n]` 定位 PathFigure 的 StartPoint/IsClosed/IsFilled，再可经
`(PathFigure.Segments)[m]` 定位 Line/Bezier/QuadraticBezier/Arc 的点成员及 Arc 的 Size/RotationAngle/
IsLargeArc/SweepDirection。必须同时校验 Figure/Segment 索引、实际 Segment 类型、末端所有者和动画值类型；Point/Size/
Double 使用连续动画，bool 与 SweepDirection 只使用离散 Object 关键帧，Arc Size 绝对值非负且 SweepDirection 仅接受
Clockwise/Counterclockwise。这些末端必须与 FillRule、Geometry.Transform 和其他 Geometry 成员共享 Clip 根值合成。
第九类扩展不是新的平行适配器，而是 Geometry 适配器的递归导航层：`(GeometryGroup.Children)[n]` 可重复任意次，
随后复用 Rectangle/Ellipse 公开成员、PathFigure/PathSegment 或 Geometry.Transform 叶子。每一跳必须验证当前对象确为
GeometryGroup 且索引有效，最终具体所有者仍须匹配；PathGeometry/GeometryGroup.FillRule 只接受离散 Object 关键帧和
EvenOdd/Nonzero。访问器保存完整子索引链，读写必须从完整 Clip 导航后原位合成，禁止把嵌套子 Geometry 提升为独立根值。
所有适配器都禁止相同末端、整根/子路径混写和跨组拆分同一根，`UIElement` 所有者必须规范化为 `Control`。运行时对象路径只能通过
单一 `ObjectPathAccessor` 适配器变体进入生命周期，Designer 只能通过统一分类/规范化/根属性/解析入口访问；新增 Brush、Geometry
或其他对象图末端时必须扩展该边界，禁止恢复每种路径一组平行字段与分支。
UsingKeyFrames 支持 Double/Color/Thickness/Point/Vector/Rect/Size/Matrix 的 Discrete/Linear/Easing/Spline 帧，包括 WPF 的 EasingThicknessKeyFrame、
EasingPointKeyFrame、EasingVectorKeyFrame、EasingRectKeyFrame、EasingSizeKeyFrame 与 EasingMatrixKeyFrame。
每帧必须有有限 KeyTime 和强类型 Value，允许 StaticResource，Spline 控制点必须
位于 0..1。省略 Duration 取最后 KeyTime，同时间帧保持声明顺序，首段从状态进入时捕获的有效值开始，且必须复用同一
RenderTransform 适配器与合成路径。RepeatBehavior 必须区分正 Count（允许分数）、正 Duration 与 Forever；AutoReverse
时一个 Count repetition 包含向前和向后，BeginTime 不能在每次 repetition 重放。FillBehavior.Stop 必须释放动画值源，
Transform 子路径只能恢复自己的成员；Forever 状态和 Transition 必须可由离开状态或 useTransitions=false 确定性中断。
SpeedRatio 必须是有限正数且不能缩放 BeginTime；Count 活动期按速度反比变化，Duration 型 RepeatBehavior 保持父时钟
固定总时长。AccelerationRatio/DecelerationRatio 必须各在 0..1 且总和不超过 1，按 WPF 归一化峰值速率在方向映射后、
	动画 Easing/关键帧采样前改变 simple progress。未知对象路径、未知时间线属性、Uniform/Paced KeyTime、
	仍必须明确拒绝。普通 Double/Color/Thickness/Point/Vector/Rect/Size/Matrix 与其关键帧动画允许 IsAdditive/IsCumulative；输出合成顺序必须是局部值、累计量、foundation。
	实时状态切换必须在初始帧事务提交成功后统一写入新时间线 StartTick，准备阶段不得消耗 Duration；显式 Transition
	完成后安装目标状态必须沿用当前采样 tick，不能破坏确定性手动时钟。
	Automatic/From/To 不得因 IsAdditive 重复加当前值；By-only 必须用 zero->By 并始终以缺省来源为 foundation；只有
	FromTo/FromBy 在 IsAdditive 时额外使用缺省来源。普通动画按 `(To-From)*(iteration-1)` 累计，关键帧按最后帧值
	累计；AutoReverse 的完整往返才推进 iteration。生成 Transition 必须清除两标志，显式 Transition 必须保留。
	ObjectAnimationUsingKeyFrames 只允许 DiscreteObjectKeyFrame，每帧可使用元数据可转换的标量、
	StaticResource 或 Brush/Geometry/Transform 属性元素；不得接受 From/To/By、Easing、IsAdditive 或
	IsCumulative。对象帧与数值帧共用 Timeline 活动期，但必须在离散边界切换整个 BindingValue。
VisualStateGroup.Transitions 的选择顺序必须固定为 From+To、To、From、默认；重复选择器和未知状态必须在定义阶段拒绝。
GeneratedDuration/GeneratedEasing 只能为已支持的 Double/Color/Thickness/Point/Vector/Rect/Size/Matrix 状态目标生成过渡，显式 Transition Storyboard 按同一
目标抑制生成动画但不得绕过既有 PropertyPath、类型、根所有权和资源校验。过渡完成前不得提前安装目标状态
Setter/Storyboard；当前状态查询应立即返回目标，中断必须从当前有效帧重新捕获。初始求值、useTransitions=false 与
系统禁用动画必须直达目标。Transition 资源也必须进入规范 XAML、v14、剪贴板、设计器校验与完整候选热重载。
	GeneratedDuration/GeneratedEasing 不得为 Object 伪造插值；过渡期间应释放旧 Object 动画并显示基础值，
	结束后再安装目标状态 Object 时间线。显式 Transition Storyboard 中的 Object 时间线必须正常执行。
视觉状态必须与组件定义一同进入规范 XAML、v14、设计器预览/恢复、剪贴板资源依赖和事务热重载；资源型 Setter
	及动画 From/To/By 变化必须完整候选重建，不能让复用宿主持有旧解析值。Behavior 只能调用公开查询/GoTo/Changed 接口，不能注册只在
运行时存在的状态定义。
组件模板根的 `<RootType.Triggers>` 只允许已声明组件事件的 `EventTrigger.RoutedEvent`。
TriggerAction 按声明顺序执行；`BeginStoryboard` 必须包含唯一 Storyboard，可选唯一 `x:Name` 是
Pause/Resume/Stop 的稳定引用。重名 Begin、未解析 `BeginStoryboardName`、未知动作/事件、
重复动画末端都必须在定义阶段事务拒绝。事件 Storyboard 必须复用完整 Timeline/对象路径管线，
不得只支持一组窄化动画。其输出使用高于 `VisualState` 的独立 `Animation` 值源；Stop 只清除该层，
必须显露当前而非 Begin 时的状态/本地值。模型必须同时进入规范 XAML、v14、剪贴板资源闭包、设计器预览和热重载。

规范 XAML 不得接受旧 `DesignNode::CustomType` 对应的 `d:CppType` 或控件清单类型信息。
`RuntimeCustomControlRegistry`、`RegisterCustomControl`、设计器 `--controls`/`--preview-plugin` 入口已经删除，
不得重新引入等价旁路。
模板已经是组件定义的一部分；声明事件和 NativeSurface 必须继续建立在组件契约上。
Designer ToolBox 只展示框架内置类型与 XAML `ComponentDefinition`。高性能业务区域必须声明为
`NativeSurface BehaviorKey="..."`；设计器只显示占位，不得探测或加载应用 DLL。应用在启动阶段构造
`NativeSurfaceBehaviorRegistry`，行为工厂只返回 `INativeSurfaceBehavior`，不能替换宿主控件类型。运行时对
未解析的非空 BehaviorKey 严格失败；只有工具材质化可显式设置 `AllowNativeSurfacePlaceholder`。
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
路由，不得清空注册表或释放另一租约。批次执行期间不得跨线程解析事件。生成路由只允许来自真实内置 Event 成员
或组件 `OnDeclarativeEvent` 固定签名；禁止重新引入可由 XAML/清单指定任意字段或参数签名的自定义事件描述符。
EventCatalog 必须为每个可设计控件类型声明且仅声明一个默认事件，并提供稳定的类型化分类；Form 默认事件为
只触发一次的 `OnShown`。事件属性双击、显式操作行、`F12` 与画布控件/Form 客户区双击必须汇合到同一激活入口，并通过与文本提交
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
状态 Current 时任一激活入口应直接导航；缺失定义先生成；签名错误或多个相同签名定义必须绕过必然失败的生成并直接
定位现有定义。同名重载导航优先精确兼容行，只有没有兼容定义时才回退首个同名定义供修复。文档状态防抖、
窗口重新激活和生成完成均需刷新检查，并通过正常 PropertyGrid reload 保留分组/滚动状态。
类内兼容定义存在时 `.handlers.g.inc` 必须省略会与定义冲突的同类声明；解绑后其余声明仍需保留。
事件下拉除文档索引外还必须使用同一 `CppUserCodeIndex` 联合枚举用户 `.h/.cpp` 中尚未绑定的兼容成员；候选只接受
当前类下恰有一个精确签名定义的合法处理函数名，必须排除构造函数、`operator`、错误签名、重复兼容定义以及
注释或字面量伪代码。若候选名已被文档中的另一事件签名占用，不得显示为可选项；默认名和当前值仍排在最前。
多选事件页只能投影所有目标共同具有且真实 Event 函数类型完全一致的事件；不能只按显示名求交集。一个编辑、
默认名激活或重置必须构造包含全部目标稳定 ID、规范事件名及 expected 前值的单个 `EventHandlerCommand`，先验证
目标仍属于文档和同名处理函数未被其他签名占用，再原子应用；不允许逐控件提交后在中途失败。不同有效处理函数
显示为混合值，进入自由文本或 EditableEnum 编辑时必须从空文本开始，展示用的 `<多个值>` 不得成为编辑内容。
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

设计器主工作流大致是：

1. 拖放控件
2. 编辑属性
3. 保存设计文件
4. 动态加载 XAML
5. 在 C++ 中按稳定 ID/名称挂接业务和原生 Behavior

静态 C++ UI 构造代码生成只是一项可选迁移/辅助功能，不得反向限制 XAML 类型、模板、资源或事件模型。

设计大尺寸窗体时，画布视图可用 `Ctrl+滚轮` 或 `Ctrl++` / `Ctrl+-` 缩放，`Ctrl+0` 适配窗口，
`Ctrl+1` 恢复 100%，中键拖动或 `Space+左键` 平移。该状态只属于 Designer 视图，不会写入设计文件、
触发 Dirty 或进入 Undo/Redo；实现画布交互时必须先把视口坐标经 `ViewToCanvasPoint()` 逆变换到逻辑 DIP，
上下文菜单等屏幕定位则使用 `CanvasToViewPoint()` 转回视口坐标。

工具箱支持“单击后落点”和“直接拖到画布”两条入口，但最终都必须调用同一 `AddControlToCanvas()` 添加路径；
拖动中的目标高亮和默认尺寸幽灵图只属于视图状态，不得提前修改运行时树、设计文档或历史。目标解析必须复用
Canvas 的普通容器、TabPage 与 Split First/Second 规则，成功松开只生成一条 `ControlSubtreeCommand`，取消、
捕获丢失或失活必须同时清理预览和 ToolBoxItem 的按下状态。Designer 在 Form 捕获期间处理工具箱或层级拖动时，
不能直接使用虚调用入口收到的 `lParam` 坐标：原生 Form WndProc 进入 `ProcessMessage()` 时该坐标可能仍为零；
必须从当前屏幕光标按 DPI 和 `ClientTop()` 重算逻辑客户区坐标，再执行画布视口变换。

复杂控件树优先使用左侧“层级”视图选择，不要为了命中被遮挡或 `Visible=false` 控件临时改文档属性。层级节点
按 stable ID 同步，选中非活动 TabPage 后代时会先切换页签祖先；结构命令、重命名和 Undo/Redo 后必须重建
节点，但需要保留展开/滚动状态。层级视图的选择只改变当前 Designer 选择，不应自行创建 Undo 记录。通过
层级树选择控件后必须把 Form 的键盘焦点留在树上，画布只同步设计选区；复制、剪切、粘贴、重复、撤销、
重做、全选和删除要在原始按键分发中直接执行，不能把系统剪贴板访问异步延后。命令引起的层级树重建应在
结构事务和输入分发退出后异步合并一次，避免同步清空 TreeView 节点造成迭代器、无障碍或输入栈重入。
工具箱/层级切换若会销毁并重建节点也必须走 `ScheduleDocumentOutlineRebuild()`，不得在 ToggleButton 的鼠标
分发栈内同步执行破坏性 `RebuildDocumentOutline()`。

层级拖放必须通过 `MoveControlInHierarchy(stableId, targetStableId, position)` 进入同一结构事务；树只负责
Before/Inside/After 命中反馈、捕获和边缘自动滚动，不得直接改运行时父子树。换父前要拒绝循环、非法
TabPage/复合容器和 Menu/StatusBar 根约束，成功后保持屏幕位置，并用一条 `ControlPlacementCommand`
覆盖父级、同级索引和布局属性的 Undo/Redo。

网格显示、网格吸附、参考线吸附和 5/10/20 DIP 步长是 Designer 会话视图状态。底部按钮与右键菜单必须
调用同一组 Canvas getter/setter 并同步勾选状态；这些操作不得写入 XAML/XML、改变 Dirty 或创建历史项。
弹出菜单边界计算必须使用与控件坐标一致的逻辑 DIP，不能把物理像素 ClientSize 直接混入 DPI 缩放后的坐标。

Tab 顺序模式同样是纯视图状态，但编号结果是正式属性修改。候选必须统一为非 TabPage 的 Designer 包装器，
同时满足运行时 `IsTabStop` 与 `IsKeyboardFocusable()`，且没有不可见的设计期祖先；控件自身 `Visible=false`
仍需保留，以便通过设计轮廓恢复和编排。徽章尺寸与字体要补偿画布缩放。手动编号和按绝对画布矩形的
top/left 稳定排序都必须通过 `PropertyGridBinder` 写入 `TabIndex`，再提交 `ControlPropertyCommand`；不得裸改
字段。自动排序的多目标差量只能形成一条命令，任一目标失败时要恢复全部已触及状态，不能留下部分编号。

控件锁定是正式的设计文档元数据，但不是运行时控件属性。唯一真源是 `DesignNode::Locked`，XAML 使用
`d:Locked`、Designer XML 使用 `locked`；材质化只把它恢复到 `DesignerControl::IsLocked`，代码生成和动态
运行时拓扑比较都不得消费它。属性栏、画布/排列菜单和 `Ctrl+L` 必须复用同一批量属性命令，以一条 Undo
修改完整选区。锁定只保护布局与树位置：仍允许选择、普通属性编辑、复制、剪切和删除；鼠标 move/resize、
SplitContainer 分隔条、键盘微调、排列/Z-order 和层级拖放都必须在修改前检查锁定状态。混合选择中存在任意
锁定控件时应拒绝整批几何操作，不能只移动未锁定子集。所有锁定选中项要绘制缩放补偿的锁标记，且不能
暴露 resize 光标或手柄。

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
