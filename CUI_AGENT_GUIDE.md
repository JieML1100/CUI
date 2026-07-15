# CUI 使用指南

## 1. 先建立正确心智模型

`CUI` 不是 `.NET WinForms`，但它的 API 风格和 WinForms 很接近。

- 语言与运行时：`C++20 + Win32 + Direct2D + DirectComposition`
- 平台：仅 `Windows`
- 主窗口类型：`Form`
- 通用控件基类：`Control`
- 容器控件：`Panel`
- 子控件挂载方式：`AddControl(...)`
- 事件订阅方式：`OnXxx += lambda`
- 窗口循环：`Show()` 后手动调用 `Form::DoEvent()`

必须明确：

- 不要假设存在 `namespace CUI`
- 不要假设存在 `Application::Initialize()` 或 `Application::Run()`
- 不要把它写成 WinForms、WPF、Qt 或 MFC 的语义
- 不要先入为主地假设宿主项目一定是“链接 lib”或“一定是直接嵌源码”

如果文档、历史代码和头文件冲突，以这几类内容为准：

1. `CUITest/main.cpp`
2. `CUITest/DemoWindow.h`
3. `CUITest/DemoWindow.cpp`
4. `CUI/include/*.h`

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
3. `CUITest/DemoWindow.cpp`
4. `CUI/include/Form.h`
5. `CUI/include/Control.h`
6. `CUI/include/Style.h`
7. `CUI/include/Panel.h`
8. `CUI/include/Layout/Layout.h`
9. `CUI/GUI/DefaultProcessMessageConvention.md`

这几处基本覆盖了：入口、消息循环、控件树、布局、事件、主题、自定义控件扩展规则。

## 4. 应用生命周期

一个 CUI 程序的真实入口模型是：

1. 在创建窗口前调用 `Application::EnsureDpiAwareness()`
2. 构造 `Form` 子类
3. 在构造函数里搭建控件树
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

常见窗口事件：

- `OnClosing`
- `OnFormClosing`
- `OnFormClosed`
- `OnSizeChanged`
- `OnMoved`
- `OnThemeChanged`
- `OnCommand`

其中：

- `OnClosing` 的签名里带 `bool&`，可用于取消关闭
- 菜单、上下文菜单等命令型入口常走 `OnCommand` 或 `OnMenuCommand`
- `sender` 一般就是触发事件的控件本身

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

如果 Agent 不知道某个控件如何使用，先在 `示例/DemoWindow.cpp` 搜这个控件名，通常能直接找到真实用法。

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

普通控件属性面板也使用同一目录补齐尚未由旧 `Props` / `Extra` 字段表示的可写属性。编辑必须
通过 `DesignerPropertyCatalog::ApplyValue(...)` 设置 Local 层并回读 Coerce 后的规范值；包装器只
跟踪这些扩展属性，保存时写入可选的 `props.metadata` 强类型对象。加载、撤销/重做和代码生成均
使用规范属性名与精确 kind，旧字段继续保留以兼容已有 XML，不能把两套字段同时用于同一属性。
普通面板使用 `GetBrowsableProperties(...)`，按 Design 分类和顺序生成编辑器，并排除 `Legacy` 与
`Transient`；样式 Setter 使用 `GetStyleProperties(...)`，仍能看到全部可写且可转换属性，不能把
“不在普通面板显示”误解为“不可 Binding/不可样式化”。Choice 显示文本与真正的强类型值必须分开，
保存和 Setter 始终使用规范值而不是本地化显示名。

从旧 `Extra` 迁移属性时，只允许“旧读、新写”：先应用 `props.metadata`，仅当同一规范属性尚未
存在时读取旧字段，并立即把 Coerce 后的有效值记录到 `MetadataProperties`。新保存与代码生成必须
停止输出旧字段。`StackPanel` 的 Orientation/Spacing/内容对齐、`WrapPanel` 的
Orientation/ItemWidth/ItemHeight、`DockPanel` 的 LastChildFill、`SplitContainer` 的布局和分隔条外观，
以及 `GroupBox` / `Expander` / `ScrollView` 的专用属性是这条规则的基准实现。拖拽、缩放等交互若直接改变已迁移属性，也必须通过
`DesignerPropertyCatalog::ApplyValue(...)` 回写包装器的规范 metadata，不能只修改运行时字段。

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

1. 先找该项目里已有的 `Form` 子类和 `AddControl(...)` 写法。
2. 先确认该项目用的是绝对布局、布局容器，还是两者混用。
3. 事件优先沿用该项目已有的 lambda / 成员函数绑定风格。
4. 控件主题优先复用项目已有的 `ControlStyleSheet`，窗口框架继续使用 `ApplyThemeFrame(...)`；
   只有未注册元数据的旧字段才直接设置 `BackColor` / `ForeColor` 等值。
5. 需要新页面时，优先参考 `示例/DemoWindow.cpp` 里最接近的那一段控件组合。
6. 需要自定义控件时，先判断能否只覆写默认 hook，而不是直接重写 `ProcessMessage()`。

## 14. 最短结论

把 CUI 当成“原生 C++ 的、API 风格接近 WinForms 的 Windows GUI 框架”来用，通常不会错。

最重要的四件事只有这些：

1. 用 `Form` 做顶层窗口，用 `Control` / `Panel` 组织控件树
2. 用 `AddControl(...)` 搭 UI
3. 用 `OnXxx += ...` 绑事件
4. 用 `Show()` + `Form::DoEvent()` 跑消息循环

如果你需要进一步确认具体控件的真实用法，直接看 `示例/DemoWindow.cpp`，它就是当前仓库最接近“官方参考实现”的示例。
