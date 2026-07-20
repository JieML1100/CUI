# CUI XAML 声明组件架构

## 决策

CUI 的产品级设计器不再把“加载任意外部 C++ 控件类型”作为核心能力。新的主路径是：

- XAML 定义组件类型身份、属性、事件、样式和视觉模板；
- CUI 运行时只实例化框架内置基类，不依赖业务 DLL 才能理解文档；
- C++ 通过稳定的 Behavior/Surface 接口挂接高性能绘制、输入和业务消息，不向 XAML 注入类型系统；
- 动态 XAML 是 UI 的规范运行时表示，完整 C++ UI 构造代码生成不再是框架正确性的前提。

规范 XAML 不再接受 `d:CppType`、`d:Header`、`d:BaseType`、`d:Constructor` 或 `d:CustomEvents`。
旧控件清单、预览插件和运行时工厂不属于新的组件类型系统；产品命令行、运行时公开 API 和样例均已移除这些入口。

## 已落地的组件闭环

`ComponentDefinition` 当前支持：

- 使用 XML 命名空间和限定名声明文档组件类型；
- 选择一个 CUI 内置 `BaseType`；
- 声明 Bool、Int、Int64、Float、Double、String、Enum、Color、Thickness、Size、Length、Brush、Geometry、Transform 属性及默认值；
- 声明属性栏名称、分类、顺序、编辑器、数值范围、只读状态、继承/默认 Binding 模式和布局/绘制失效标志；
- 使用一个独立视觉模板，模板节点具有定义内局部身份；
- 声明单值/多值视觉内容属性，并由模板内显式 Presenter 承载实例子树；
- 使用 `{TemplateBinding Property}` 把组件实例属性实时投影到模板节点；
- 声明带稳定 payload schema 的组件事件，并用 `{RaiseEvent EventName}` 从模板事件转发；
- 组件实例使用普通 XAML 属性、Binding 和稳定 `DesignId`，模板内部不污染页面 ID/名称空间；
- 动态属性进入与原生属性相同的 Default/Inherited/Theme/Style/Binding/Local 优先级系统；
- 组件 QName 可作为 `Style TargetType`，Setter/Trigger 可设置组件属性且不会命中同 BaseType 的普通控件；
- 设计器属性发现、动态材质化、规范 XAML、v23 XML 快照、剪贴板和热重载共享同一契约；
- 组件属性不能覆盖 `BaseType` 已有属性。

示例：

```xml
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:local="urn:sample:controls"
      x:Name="MainForm">
  <Form.Resources>
    <ComponentDefinition x:Key="local:StatusSurface" BaseType="Panel">
      <ComponentDefinition.Properties>
        <ComponentProperty Name="Severity" Type="Int" Default="0"
                           Category="Status" Editor="Number"
                           Minimum="0" Maximum="10" AffectsRender="true" />
        <ComponentProperty Name="Caption" Type="String" Default="Ready"
                           Category="Status" BindsTwoWayByDefault="true"
                           DefaultUpdateSourceTrigger="LostFocus" />
		<ComponentProperty Name="Status" Type="String" Default="Idle"
		                   Category="Status" ReadOnly="true" />
        <ComponentProperty Name="AccentLevel" Type="Int" Default="1"
                           Inherits="true" BindsTwoWayByDefault="true"
                           AffectsParentMeasure="true" />
      </ComponentDefinition.Properties>
      <ComponentDefinition.ContentProperties>
        <ComponentContentProperty Name="Content" Cardinality="Single"
                                  Default="true" />
        <ComponentContentProperty Name="Actions" Cardinality="Multiple" />
      </ComponentDefinition.ContentProperties>
      <ComponentDefinition.Events>
        <ComponentEvent Name="Invoked" Category="Action" Default="true"
                        RoutingStrategy="Bubble" />
      </ComponentDefinition.Events>
      <ComponentDefinition.Template>
        <StackPanel Padding="8" OnMouseClick="{RaiseEvent Invoked}">
          <Label Text="{TemplateBinding Caption}" />
		  <Label Text="{TemplateBinding Status}" />
          <StackPanel ComponentSlot.Presents="Content" />
          <StackPanel Orientation="Horizontal"
                      ComponentSlot.Presents="Actions" />
        </StackPanel>
      </ComponentDefinition.Template>
    </ComponentDefinition>
  </Form.Resources>

  <local:StatusSurface x:Name="status" Severity="2" Caption="Warning"
                       Invoked="HandleStatusInvoked">
    <Label Text="Default content" />
    <local:StatusSurface.Actions>
      <Button Text="Accept" />
      <Button Text="Cancel" />
    </local:StatusSurface.Actions>
  </local:StatusSurface>
</Form>
```

## 视觉模板语义

`ComponentDefinition.Template` 保存一棵独立于页面实例节点的模板树。组件实例只有一个公开设计节点和一个
稳定 ID；模板内部节点使用定义内局部身份，不能污染页面名称/ID 空间。模板展开由材质化器完成，设计器默认
把模板内部视为封装内容。后续加入“编辑模板”模式后才允许直接选择和修改内部节点。

`{TemplateBinding Property}` 是从组件实例属性到模板子节点属性的单向轻量连接。它不借用 DataContext，
也不序列化为普通 Binding。当前连接会随实例属性变化实时更新。需要 TwoWay 模板交互时，应显式使用组件事件或受控的相对源绑定，不能通过
隐式父级遍历猜测来源。

第一版模板约束：一个视觉根；根和后代只允许内置类型或已解析的声明组件；禁止模板递归环；定义加载时完成
属性、资源和拓扑验证；任意实例化失败保持旧文档不变。

### ControlTemplate：类型契约与外观分离

`ComponentDefinition` 继续负责声明新的 XAML 类型、公开属性、事件、内容槽和默认视觉结构；新增的
`ControlTemplate` 则开始把“已有控件类型的外观”从 C++ 控件行为中拆出来。第一批支持
`ContentControl`、`Button`、`GroupBox`、`Expander`、`ItemsControl`、`ListBox`、`ListBoxItem` 和 `ComboBoxItem`，既可声明有键资源并由
`Template="{StaticResource Key}"` 显式选择，也可省略 `x:Key`，按 `TargetType` 在当前控件、逻辑祖先和
文档资源中选择最近的隐式模板。显式模板允许 `ContentControl` 作为上述派生内容控件的通用目标；隐式类型键
保持精确匹配，避免一个基础模板无意替换所有派生控件外观。

模板生成的视觉根属于控件基础设施，不占用 authored `Content` 槽。`Button`、`GroupBox` 和 `Expander`
检测到模板根后跳过原生 chrome，只保留各自输入、状态和内容行为；这使 XAML 控制绘制结构，C++ 继续拥有稳定
消息状态机。模板中的 `{TemplateBinding Property}` 从宿主属性元数据建立实时单向连接，普通旧属性和动态声明
属性使用同一 `IBindingSource` 通知边界。模板还复用声明组件现有的 VisualState、StateTrigger、Setter、
Storyboard、EventTrigger 和命名部件系统，状态目标在模板 namescope 中解析。

`ControlTemplate` 已进入全局、合并字典和控件局部资源、规范 XAML、v29 XML 快照、设计器预览、事务热重载与
剪贴板闭包。复制控件时，外部命中的模板会提升为片段根局部资源，避免粘贴操作把目标文档的同类控件全部换肤。
模板必须有且只有一个有效视觉根，`TargetType` 必须与宿主兼容，递归模板链会在材质化提交前拒绝。模板资源变化
属于视觉拓扑变化，不能误走原位属性热更新。

`ControlTemplate.TargetType` 现同时接受上述内置类型与声明组件 QName。组件模板按 QName 精确匹配，不会因为共享
同一 `BaseType` 而串用；直接 `Template`、`Style.Template`、词法隐式模板和
`ComponentDefinition.Template` 按显式、样式、隐式、类型默认顺序解析。属性栏只列出与当前实际类型兼容的有键模板，
修改后通过文档事务重建并支持 Undo/Redo。Trigger/VisualState 动态换模板暂不开放，后续若开放必须进入独立结构事务，
不能降格为普通运行时 Setter。

模板内容现通过 `ContentPresenter.ContentSource` 显式占位。`ContentSource="Content"` 自动把宿主的
Content/ContentTemplate/DisplayMemberPath 映射到 Presenter；`ContentSource="Header"` 对
Header/HeaderTemplate/HeaderDisplayMemberPath 做同样映射，且只允许用于 HeaderedContentControl 目标。
每个来源在同一模板中最多出现一次，不能与显式别名属性冲突。视觉内容的物理父级是生成的 Presenter，设计器逻辑
父级仍是模板宿主；这样既保持 WPF 式视觉所有权，也不把模板内部节点暴露为 authored 文档节点。

列表模板通过 `ItemsPresenter` 标记唯一的 ItemsHost 插槽。它只允许出现在 `ItemsControl` / `ListBox` 的模板视觉树中，
不能包含 authored 子项；`ItemsPanelTemplate` 仍决定实际 ItemsHost 类型，模板展开时宿主把同一个 ItemsHost 原子转移到
Presenter。Presenter 最近的内层 `ScrollView` 取得滚动偏移、BringIntoView 和虚拟化视口职责；没有 Presenter 的模板仍
保留数据、容器生成与选择状态，但 ItemsHost 不进入视觉树。这条边界让 XAML 完整拥有列表外观，C++ 只保留选择、输入、
容器生成和虚拟化行为，也避免把模板基础设施伪装成 authored 设计节点。

选择器生成容器共用内部 `ItemContainerControl` 契约，并以 WPF 名称 `ListBoxItem` / `ComboBoxItem` 暴露给
Style、ControlTemplate 和运行时类型系统。它们是派生自 `ContentControl` 的非 authored 基础设施节点，记录视觉通过
`ContentPresenter ContentSource="Content"` 接入；`ItemContainerStyle` 可设置普通容器属性和 `Template`，无显式模板时
按宿主所在词法作用域选择对应 TargetType 的隐式 ControlTemplate。运行时保存可重复的模板材质化工厂，每个生成或
回收容器取得独立模板树，不能复用只适合单实例的物化结果。两类容器都公开只读、可观察的 `IsSelected`、
`IsMouseOver` 和 `IsKeyboardFocusWithin`；StateTrigger 读取这些状态时走条件元数据校验，不要求属性可写，Setter 仍
严格禁止写入。复制宿主时会把实际命中的 ItemContainerStyle、显式/隐式容器模板及其子树依赖一起提升到片段根。
`ListBoxItem` 继续只允许作为生成容器；ComboBox 直接内容中的 `<ComboBoxItem Content="..."/>` 是静态项声明语法，
不是可选择的 authored 控件节点。旧 `SelectorItem` 仅保留为 C++/读取兼容名。

### 属性行为元数据

`ComponentProperty` 的行为不再只表示“值类型”。`Inherits="true"` 使同一组件 QName、同一属性名的值沿逻辑
父链传播，中间可以穿过不声明该属性的 Presenter/Panel；本地值、Binding、Style 和 Theme 仍按属性值优先级
覆盖继承值。继承身份由组件限定名和属性名稳定组成，不会让不同命名空间中的同名组件互相串值，属性栏会把
当前有效来源显示为 `Inherited`。

Binding 省略 `Mode` 时保存为 `BindingMode::Default`。普通目标属性将其解析为 `OneWay`；声明了
`BindsTwoWayByDefault="true"` 的组件属性解析为 `TwoWay`，设计器校验、运行时安装、MultiBinding 子项继承和
规范 XAML 使用同一规则。显式 `Mode=OneWay/TwoWay/...` 始终覆盖默认值。

Binding 省略 `UpdateSourceTrigger` 时保存为 `DataSourceUpdateMode::Default`，安装时再由目标属性元数据解析。
`ComponentProperty.DefaultUpdateSourceTrigger` 必须声明具体的 `PropertyChanged`、`LostFocus` 或 `Explicit`；
`Default` 不是合法的元数据默认值。内置 `TextBox.Text` 使用 `TwoWay + LostFocus`，显式 Binding 配置仍可覆盖。
解析器兼容旧 `UpdateMode` 名称，但规范 XAML 只输出 WPF `UpdateSourceTrigger` 拼写并省略 `Default`。

布局标志除 `AffectsMeasure`、`AffectsArrange`、`AffectsRender` 外，还支持 `AffectsParentMeasure` 与
`AffectsParentArrange`，用于让子项尺寸/定位属性使逻辑父容器失效。以上标志均进入规范 XAML、v14 快照、
设计器元数据发现和动态运行时，非法布尔文本在提交前事务性拒绝。

`ReadOnly="true"` 声明由组件 Behavior/运行时状态维护的公开只读属性。元数据保留 getter 与变更通知，因此它可
作为普通 Binding、ElementName Binding 和 `TemplateBinding` 的源，也可参与继承；但它不能成为实例字面值、
Style Setter、Binding/MultiBinding 的目标，普通 `TrySetPropertyValue`/`ClearPropertyValue` 也不能绕过此边界。
属性栏保留只读行用于观察当前有效值和来源，样式/Binding 目标目录则过滤该属性。动态组件没有可在编译期持有的
DependencyPropertyKey，因此 C++ Behavior 通过显式的 `TrySetReadOnlyPropertyValue` 与
`ClearReadOnlyPropertyValue` 获得 key-equivalent 写能力；该写入占用 Local 来源层，清除后重新显露继承值或默认值。
`ReadOnly` 与 `BindsTwoWayByDefault`、非 `PropertyChanged` 的 `DefaultUpdateSourceTrigger` 互斥。

### 视觉内容属性

`ComponentDefinition.ContentProperties` 定义的是视觉子树入口，不是可塞入任意值的对象袋。`Cardinality="Single"`
最多接收一个视觉根，`Multiple` 接收有序的多个视觉根；一个定义至多有一个 `Default="true"` 内容属性。实例的
直接视觉子节点进入默认属性，非默认属性使用 `<local:Type.Property>` 显式包装。

每个内容属性必须在模板中恰好拥有一个 `ComponentSlot.Presents="Property"`。Presenter 只能标记 Panel、
StackPanel、WrapPanel、DockPanel、GridPanel 或 RelativePanel；未知槽、重复 Presenter、单值槽多根以及无默认槽的
直接内容都会在文档提交前拒绝。公开子节点保留自己的稳定 `DesignId`，设计器逻辑父级仍是组件实例；运行时才把
它挂到生成的 Presenter。布局撤销/重做、剪贴板、规范 XAML、v14 快照和热重载都保存这层双重父级语义。

这批能力只解决“控件组成控件”的视觉集合。数据集合由下面独立的强类型模板层负责，不能复用视觉内容属性冒充
`std::any` 集合。

### 强类型数据集合与 DataTemplate

`ItemsControl` 是通用数据集合宿主。它的 `ItemsSource` 只接受带稳定 `ItemType` 的 `IBindingList`，
`ItemTemplate` 只接受文档资源中的 `DataTemplate`；两者的类型必须精确一致。记录结构由 XAML `DataType`
声明，模板内的普通 `{Binding Path}` 以单条记录而不是窗体 DataContext 为源：

```xml
<Form.Resources>
  <DataType x:Key="Person">
    <DataType.Properties>
      <Property Path="Name" Kind="String" />
      <Property Path="Role" Kind="String" />
    </DataType.Properties>
  </DataType>
  <DataList x:Key="PeopleSeed" ItemType="Person">
    <DataRecord Name="Alice" Role="Admin" />
    <DataRecord Name="Bob" Role="Guest" />
  </DataList>
  <DataTemplate x:Key="PersonRow" DataType="Person">
    <StackPanel Orientation="Horizontal" Spacing="8">
      <Label Text="{Binding Name}" />
      <Label Text="{Binding Role}" />
    </StackPanel>
  </DataTemplate>
  <Style TargetType="ListBoxItem" x:Key="PersonContainer">
    <Setter Property="Padding" Value="8,4" />
    <Style.Triggers>
      <Trigger Property="IsSelected" Value="true">
        <Setter Property="BackColor" Value="#282F6FE4" />
      </Trigger>
    </Style.Triggers>
  </Style>
  <ItemsPanelTemplate x:Key="PersonItemsPanel">
    <VirtualizingStackPanel ItemHeight="32" Spacing="2" CacheLength="1" />
  </ItemsPanelTemplate>
</Form.Resources>
<Form.DataContextSchema>
  <Property Path="People" Kind="Object" ObjectType="BindingList"
            ItemType="Person" CanWrite="false" />
</Form.DataContextSchema>
<ItemsControl ItemsSource="{Binding People}"
              ItemTemplate="{StaticResource PersonRow}" />
<ItemsControl ItemsSource="{StaticResource PeopleSeed}"
              ItemTemplate="{StaticResource PersonRow}" />
<ListBox ItemsSource="{StaticResource PeopleSeed}"
         ItemTemplate="{StaticResource PersonRow}"
         ItemContainerStyle="{StaticResource PersonContainer}"
         ItemsPanel="{StaticResource PersonItemsPanel}"
         SelectedValuePath="Name" />
```

`ObservableBindingList` 发布结构变化；记录使用 `IBindingSource`，字段替换会保留选择/可访问身份并更新对应模板实例。
`DataList` 是正式运行时资源而不是 `d:` 预览包：它会被转换为同一 `ObservableBindingList` 契约，因此设计器和
运行时看到同一批数据。属性栏的 `ItemsSourceResource` / `ItemTemplate` 选择器按 `ItemType` 过滤，修改会以整文档
事务重新材质化。模板和记录契约支持合并字典、来源追踪、规范 XAML、v14 XML、设计器恢复、剪贴板依赖收集和热重载。
窗体属性页还提供统一数据资源编辑器，可维护本地类型、字段、列表、记录和模板身份。资源/字段重命名不是字符串替换：
它通过文档模型重写 DataContext ItemType、DataList ItemType、DataRecord 路径、DataTemplate Binding 及控件资源引用，
再进行整文档验证与事务提交；删除前检查依赖。合并字典条目保留来源边界并在主文档编辑器中只读。
跨文件声明不依赖字典书写顺序。模板变化会事务式重建拥有它的视觉树；失败时保留旧文档。

`ComboBox`、`ListView`、`ListBox` 的数据投影采用同一规则：`DisplayMemberPath` 生成显示文本，
`SelectedValuePath` 从当前记录提取强类型 `SelectedValue`。空路径表示记录自身；标量比较保持原类型，记录比较
使用稳定共享身份，不通过字符串退化。`SelectedValue` 可 TwoWay Binding，交互选择、所选字段变化和数据重载
会同步 Binding 的当前值源；外部值变化则反向定位记录。静态 DataList 与 DataContext BindingList 都必须按
`ItemType` 的 `DataType` 验证这些路径。`ListBox` 建立在共享 `Selector` 上，`SelectedItem` 保存记录身份，
生成的 `ListBoxItem` 承载任意 DataTemplate，并公开只读交互状态供 Trigger/VisualState 使用；它不是可直接书写的
文档节点。`ItemContainerStyle` 只接受面向 `ListBoxItem` 的命名 Style，属性栏、规范 XAML、剪贴板依赖隔离和
事务热重载共享这一资源引用。`ItemsControl`/`ListBox` 使用 ScrollView 视口和资源化 `ItemsPanelTemplate`；
普通 `StackPanel`/`WrapPanel` 完整实例化，`VirtualizingStackPanel` 依赖固定 `ItemHeight` 计算精确范围，仅创建
可见区与 `CacheLength` 缓存区，面板 `Spacing` 取代控件级间距。滚轮、方向键、Home/End、PageUp/PageDown 与
选中项 BringIntoView 对未实例化记录同样有效。集合重排时选择随记录而不是
旧索引移动。无选择状态统一为 `-1`，不得自动选择首项。

`ItemContainerGenerator` 是集合索引、已实现容器和回收容器的唯一映射层。Add/Remove/Move/Swap/Replace
直接重映射未受影响的实例，更新容器 `ItemIndex` 与虚拟 host 索引；只有 Reset 或不完整的第三方通知才
使用全量候选重建。虚拟面板以首个可见记录为滚动锚点，视口上方的结构变化会同步修正 offset。无 DataTemplate
时，显示路径字段变化通过合成 Replace 只刷新一个容器。

`CollectionViewSource` 位于 `IBindingList` 与控件之间，负责筛选、稳定多键排序和 CurrentItem 导航。
它的 Source 可以是 DataList、另一 CollectionViewSource 或 DataContext BindingList；视图链共享记录身份，
投影差异只产生 Add/Remove/Move。`GroupDescriptions` 建立多级连续分组，`GroupStyle` 以
`CollectionViewGroup` HeaderTemplate 包装组边界并保持底层 ListBoxItem 身份；`AggregateDescriptions`
把命名聚合公开为 `Aggregates.*`。固定项高虚拟面板使用项/组头段偏移索引，`HeaderHeight` 保证组头 extent 精确。
设计器模型、v14 XML、规范 XAML、合并字典、属性候选、剪贴板依赖和热重载
必须共同保留该资源，不允许在 ListBox 上增加一次性的 Sort/Filter 开关。

首批有意不允许 DataTemplate 内代码后置事件，也不提供隐式反射、任意对象集合或 C++ 模板工厂。交互应通过
绑定、声明组件事件或宿主 Behavior 表达。`ComboBox`、`ListView`、`ListBox` 已共享同一 `IBindingList` 数据源、
显示路径和选择值契约；需要无选择语义的任意视觉项时使用 `ItemsControl + DataTemplate`，需要选择语义时使用
`ListBox + DataTemplate`。

### 声明事件

组件事件属于 XAML 类型契约，而不是 C++ Event 字段名。当前事件声明支持 None、Bool、Int、Int64、Float、
Double、String payload；控件实例会安装同名动态事件元数据并在 Raise 时校验类型。设计器为实例保存处理函数名，
运行时通过文档事件路由表连接。

模板内部使用 `{RaiseEvent EventName}` 显式转发受支持的内置事件。C++ 业务层只按“组件类型 + 处理函数名”注册，
文档再用实例稳定 ID/名称和事件名解析连接；处理函数签名稳定为
`void(Control*, DeclarativeEventArgs&)`，payload 位于 `args.Value`。
业务代码不需要知道模板内节点。
默认处理函数命名仍由设计器建议，但函数名是可编辑引用，不能退化为是否生成的布尔值。

`ComponentEvent.RoutingStrategy` 支持 `Direct`（默认）、`Bubble` 和 `Tunnel`。事件身份是声明组件的命名空间 URI、
类型名和事件名三元组；同名但不同 QName 的事件不能互相触发。Bubble 按 Source→根顺序，Tunnel 按根→Source 顺序
遍历 Raise 时快照的 Control 父链。组件模板通过 `{RaiseEvent ...}` 发布的是组件宿主的公开事件，因此
`OriginalSource` 是组件实例，而不是模板局部节点。

祖先节点使用 `local:StatusSurface.Invoked="Handler"` 形式安装附加处理器。规范 XAML 输出当前组件前缀，文档模型则
保存无前缀的稳定 QName；因此资源合并/剪贴板发生前缀重映射时事件身份不变。仅由附加事件引用的组件定义也必须进入
剪贴板依赖闭包。附加处理器改名、原位热重载和失败回滚与普通实例事件共享 `DesignDocumentEventIndex` 和运行时连接事务。

`DeclarativeEventArgs` 包含 `OwnerNamespace`、`OwnerTypeName`、`OriginalSource`、`Source`、`CurrentTarget`、
`RoutingStrategy`、`Value` 和可写 `Handled`；处理器的 sender 始终是 `CurrentTarget`。路由不会因 Handled 提前截断，
而是由普通注册跳过后续回调；`RuntimeComponentEventRegistrationOptions.HandledEventsToo` 可显式继续接收，这与 WPF
`AddHandler(..., handledEventsToo)` 的行为一致。Behavior 可传入 args 调用 `RaiseDeclarativeEvent(args)` 并在返回后读取
最终 Handled 状态。

当前转发覆盖无 payload 的鼠标单击/双击/进入/离开、焦点、绘制、关闭、移动、尺寸、选择和滚动事件，
以及 `OnChecked -> Bool`、`OnTextChanged`/`OnDropText -> String`。后续事件不通过隐式参数猜测扩展，必须增加
明确且可验证的映射。

C++ 也可由 Behavior 调用 `RaiseDeclarativeEvent(...)` 产生组件事件；未声明事件或 payload 类型不匹配会失败。

### 组件样式

`TargetType="local:StatusSurface"` 保存的是命名空间 URI + 类型名，不是 `Panel` 的别名。运行时选择器仍保留
BaseType 约束用于快速排除，但最终必须精确匹配组件身份。因此同一个 `Panel` BaseType 可以承载多个声明组件，
各自的默认样式互不污染。样式 Setter 与 Trigger 在解析、Designer 编辑和材质化前使用安装了组件动态属性契约的
probe 校验；规范 XAML 和 v14 XML 都保留组件 QName。资源内的组件定义作为 schema 先于本地 Style 解析，
不要求作者为了 TargetType 手工调整两者的书写顺序。

### 组件视觉状态

组件模板根可声明 `VisualStateManager.VisualStateGroups`。状态是组件公开属性/事件契约到模板外观的声明映射，
不是由 C++ Behavior 私下维护的第二套状态机：

```xml
<VisualStateManager.VisualStateGroups>
  <VisualStateGroup x:Name="CommonStates">
    <VisualStateGroup.Transitions>
      <VisualTransition From="Normal" To="Active"
                        GeneratedDuration="0:0:0.200">
        <VisualTransition.GeneratedEasingFunction>
          <CubicEase EasingMode="EaseInOut" />
        </VisualTransition.GeneratedEasingFunction>
        <VisualTransition.Storyboard>
          <Storyboard>
            <DoubleAnimation Storyboard.TargetName="chrome"
                             Storyboard.TargetProperty="ValidationCornerRadius"
                             To="12" Duration="0:0:0.150" />
          </Storyboard>
        </VisualTransition.Storyboard>
      </VisualTransition>
      <VisualTransition GeneratedDuration="0:0:0.100" />
    </VisualStateGroup.Transitions>
    <VisualState x:Name="Normal" />
    <VisualState x:Name="Active">
      <VisualState.StateTriggers>
        <StateTrigger Property="IsActive" Value="true" />
      </VisualState.StateTriggers>
      <VisualState.Setters>
        <Setter TargetName="chrome" Property="BackColor"
                Value="{StaticResource ActiveColor}" />
      </VisualState.Setters>
      <VisualState.Storyboard>
        <Storyboard>
          <DoubleAnimation Storyboard.TargetName="chrome"
                           Storyboard.TargetProperty="ValidationCornerRadius"
                           From="0" To="12" Duration="0:0:0.200">
            <DoubleAnimation.EasingFunction>
              <CubicEase EasingMode="EaseInOut" />
            </DoubleAnimation.EasingFunction>
          </DoubleAnimation>
          <ColorAnimation Storyboard.TargetName="caption"
                          Storyboard.TargetProperty="ForeColor"
                          To="{StaticResource ActiveTextColor}"
                          BeginTime="0:0:0.050" Duration="0:0:0.200" />
        </Storyboard>
      </VisualState.Storyboard>
    </VisualState>
    <VisualState x:Name="Invoked">
      <VisualState.StateTriggers>
        <EventTrigger Event="Invoked" />
      </VisualState.StateTriggers>
      <VisualState.Setters>
        <Setter TargetName="caption" Property="Text" Value="Invoked" />
      </VisualState.Setters>
    </VisualState>
  </VisualStateGroup>
</VisualStateManager.VisualStateGroups>
```

每组恰好有一个无触发器的回退状态。一个状态内的多个 `StateTrigger` 使用 AND；同组条件状态按声明顺序取第一个
匹配项，否则回到回退状态。`EventTrigger` 只允许组件自身已声明事件，事件进入的状态保持到显式切换或相关条件
属性变化。首批禁止一个状态混合属性触发器和事件触发器，避免隐含的优先级规则。

Setter 为空 `TargetName` 时作用于组件宿主，否则名称必须在当前模板 namescope 中解析；属性必须存在、可写且值按
目标元数据强类型转换。不同组不得写入同一“目标 + 属性”，从定义阶段消除并行状态组合的覆盖歧义。活动 Setter
与状态 Storyboard 使用同一个高于 Local 的 `VisualState` 值源；离开状态只清除该层，原有
Local/Binding/Style/Inherited/Default 值自动恢复，不能再增加并列的隐式覆盖通道。

Storyboard 基础批次包含有限的 `DoubleAnimation`、`ColorAnimation`、`ThicknessAnimation`、`PointAnimation`、`VectorAnimation`、
`RectAnimation`、`SizeAnimation` 和离散 `ObjectAnimationUsingKeyFrames`。值类型动画分别要求数值、颜色、Thickness、Point、Vector、Rect 或 Size
元数据；Object 允许任意可写元数据但只做离散切换。`Storyboard.TargetName` 为空时指向组件宿主，否则必须解析到模板局部 namescope。
`Storyboard.TargetProperty` 通常是单一属性名；首个对象适配器还允许具名模板控件使用
`(Control.RenderTransform).(TransformGroup.Children)[n].(TransformType.Property)` 定位模板中已经声明的
Translate/Scale/Rotate/Skew 数值成员。路径解析模型只负责稳定语法，对象适配器负责按实际操作索引、类型与末端成员
解析；这避免把 C++ 反射或 Transform 的内部存储形态泄漏进通用路径层。同状态的多个不同变换末端先基于同一根值
合成，再以单个 `VisualState` 来源提交完整 `RenderTransform`；相同末端、整属性/子路径混写以及跨组拆分根所有权
均拒绝。第二个对象适配器允许 `RectAnimation` 使用
`(Control.Clip).(RectangleGeometry.Rect)`；它要求具名模板部件已经显式声明 RectangleGeometry，每帧仅替换 Rect，
保留圆角、几何变换和其他根数据，并将 `UIElement.Clip` 规范化为 `Control.Clip`。
第三个对象适配器允许 `ColorAnimation` / `DoubleAnimation` 分别使用
`(Control.Foreground).(GradientBrush.GradientStops)[n].(GradientStop.Color|Offset)`；目标必须显式声明线性/径向画刷和
有效 Stop 索引。同根多个 Color/Offset 动画按一份完整 Brush 逐帧合成，保留类型、坐标、Opacity 与未命中的 Stop；绝对
Offset 端点限制在 0..1，By 允许有符号有限增量，帧写回按属性语义约束。线性/径向具体所有者规范化为 `GradientBrush`。
第四个对象适配器允许 `DoubleAnimation` 使用
`(Control.Foreground).(Brush.Transform|RelativeTransform).(TransformGroup.Children)[n].(TransformType.Property)`；
目标必须显式声明对应 Brush Transform 与操作，具体画刷所有者规范化为 `Brush`。Transform、RelativeTransform 与
GradientStop 末端基于同一 Foreground 根值合成，属性元数据相等比较覆盖两套 Transform，避免纯变换帧被短路。
第五个对象适配器允许 `DoubleAnimation` 使用
`(Control.Clip).(Geometry.Transform).(TransformGroup.Children)[n].(TransformType.Property)`；目标必须显式声明
Rectangle/Ellipse/Path/GeometryGroup、Geometry.Transform 与匹配操作，具体所有者规范化为 `Geometry`。它与
RectangleGeometry.Rect 动画基于同一 Clip 根值逐帧合成，保留形状、圆角、FillRule 与未命中的 Transform 操作。
运行时使用单一 `ObjectPathAccessor` 变体承载这些适配器，Designer 使用统一的对象路径分类、规范化、根属性与解析结果。
后续对象图末端必须注册到该边界，不能在构建、采样、写回、停止和所有权检查中增加平行路径字段。

普通动画要求有限 `Duration`，`From`、`To`、`By`、`BeginTime` 可选。端点按 WPF 的 Automatic/From/To/By/
FromTo/FromBy 六种类型解析：From-only 到基础目标，To-only 从当前来源开始，By-only 从当前来源累加，FromBy 的终点
为 From+By，FromTo 直接使用 To；To 与 By 同时存在时 To 优先且 By 仍保留，三者均省略时当前来源回到基础目标。
显式 Transition 将进入过渡时的当前值同时作为缺省来源和缺省目标。By 是增量，只按目标类型转换，不在相加前套用
绝对值 Coerce；插值帧写入属性时仍经过统一元数据/Coerce。Color 按 RGBA 通道相加，Thickness 按
Left/Top/Right/Bottom 分量相加，Point/Vector 按 x/y 分量相加，Rect 按 x/y/width/height 分量相加，Size 按 Width/Height 分量相加，Double 同样覆盖 Transform 子路径。
支持 Linear 以及 Quadratic/Cubic/Sine 的 EaseIn/EaseOut/EaseInOut，完成后 HoldEnd；系统禁用动画时直接写入终值。
同状态 Setter/动画目标重复会拒绝。

普通 Double/Color/Thickness/Point/Vector/Rect/Size 与其关键帧时间线允许 `IsAdditive` / `IsCumulative`。输出顺序固定为“本 repetition 局部值 + 已完成 repetition
累计量 + foundation”。Automatic、From-only、To-only 的 foundation 为零；By-only 的局部端点为零到 By 且 foundation
始终是缺省来源；FromTo/FromBy 仅在 IsAdditive 时以缺省来源为 foundation。普通动画累计 `(To-From) * iterationIndex`，
关键帧动画累计 `lastKeyFrameValue * iterationIndex`；IsAdditive 关键帧第一段从零开始。AutoReverse 的正向+反向整体才
推进一次 iterationIndex。生成 Transition 必须先求绝对进入值，再清除 additive/cumulative，防止二次叠加；显式
Transition 保留作者标志。

关键帧批次包含 `DoubleAnimationUsingKeyFrames` / `ColorAnimationUsingKeyFrames` /
`ThicknessAnimationUsingKeyFrames` / `PointAnimationUsingKeyFrames` / `VectorAnimationUsingKeyFrames` / `RectAnimationUsingKeyFrames` / `SizeAnimationUsingKeyFrames`。
Double/Color/Thickness/Point/Vector/Rect/Size 均提供 Discrete/Linear/Easing/Spline；每帧必须有显式有限 `KeyTime` 和强类型 `Value`，端点允许 `StaticResource`；Easing 帧复用上述
缓动函数，Spline 帧要求四个 0..1 控制点的 `KeySpline`。省略 `Duration` 时使用最大 `KeyTime`，同时间帧按声明顺序
稳定排序，第一段从进入状态时捕获的当前有效值插值。关键帧可使用同一 RenderTransform 路径适配器和逐帧合成协议。

Object 关键帧批次仅允许 `ObjectAnimationUsingKeyFrames` + `DiscreteObjectKeyFrame`。标量、资源和
Brush/Geometry/Transform 属性元素都先转换为统一值模型，再经目标属性元数据转换与 Coerce。
Object 没有 From/To/By、Easing、Additive/Cumulative；生成 Transition 只为 Double/Color/Thickness/Point/Vector/Rect/Size 插值，Object 在过渡中
回到基础值，显式 Transition Storyboard 则正常执行离散切换。

Timeline 活动期由 `Duration`、`AutoReverse` 和 `RepeatBehavior` 共同决定。RepeatBehavior 支持正的整数/分数 Count、
有限正 TimeSpan 和 Forever；Count 的一个 repetition 在 AutoReverse 时包含向前与向后两个 simple iteration，按时长
重复则在指定总时长处截断当前方向/进度。BeginTime 仅延迟第一次启动。FillBehavior 默认 HoldEnd，使用活动期边界
的精确样本；Stop 移除动画贡献并恢复下层值源。Transform 子路径 Stop 必须只恢复自己的末端并保留同根仍活动或
HoldEnd 的其他末端。Transition Storyboard 使用其完整活动期参与过渡总时长；Forever Transition 保持 pending，直到
新的切换请求中断，`useTransitions=false` 对同一个 pending 目标也必须立即完成切换。
SpeedRatio 是有限正数，只缩放 simple iteration 的本地时钟；BeginTime 保持父时钟偏移。Count 型有效活动期除以
SpeedRatio，而 Duration 型 RepeatBehavior 本身就是父时钟总时长，不再二次缩放。AccelerationRatio 与
DecelerationRatio 各自位于 0..1、总和不超过 1，使用 WPF 归一化峰值速率对方向确定后的 simple progress 做分段
二次/线性/二次映射，再交给动画 Easing 或关键帧采样；因此加减速不改变自然活动期，SpeedRatio 是全程平均速度。
实时状态切换必须先事务性提交初始帧，再把实际提交时刻写入所有新时间线的 StartTick，禁止让状态构造耗时提前消耗
Duration。显式 Transition 完成后安装目标状态必须传递当前采样时刻，而不是重新读取墙钟，以维持确定性手动时钟语义。

`VisualStateGroup.Transitions` 为状态切换提供独立的 `VisualTransition` 集合。`From`/`To` 为空分别表示任意来源/目标，
选择优先级严格为 From+To、To、From、默认，重复选择器与不存在的状态名均拒绝。`GeneratedDuration` 和可选的
`VisualTransition.GeneratedEasingFunction` 从新状态 Double/Color/Thickness/Point/Vector/Rect/Size 动画的目标值生成插值；离开旧状态且新状态没有接管
该目标时，生成动画会回到进入视觉状态层之前的基础值。显式 `VisualTransition.Storyboard` 使用相同的 Timeline、
PropertyPath、资源和变换合成契约，并按目标抑制对应生成动画。过渡总时长取 GeneratedDuration 与显式时间线结束时间
的最大值，结束后才安装目标状态 Setter/Storyboard；切换请求期间当前状态查询立即返回目标状态，后续切换从当前有效
帧重新捕获。`useTransitions=false`、初始状态求值和系统禁用动画均直接进入目标状态。

未注册的任意对象图路径和 Uniform/Paced `KeyTime` 尚未进入契约，解析器必须报错
而不是接受后忽略。

视觉状态随组件定义进入规范 XAML、v14 XML、设计器预览/恢复、剪贴板依赖闭包和运行时物化。Setter 以及动画
From/To/By 的 `StaticResource` 保留作者资源键；资源内容变化时热重载必须走完整候选树，因为运行时状态管理器保存的是已解析值，
不能只替换 StyleSheet 后继续复用旧实例。其他属性变化可原位更新，定义错误与切换失败均保持旧状态/旧运行树。
Behavior 只通过 `GetCurrentVisualState`、`GoToVisualState` 和 `OnVisualStateChanged` 参与，不得绕过 XAML 注入私有状态定义。

### 组件事件动作与可控 Storyboard

组件模板根可通过 `<RootType.Triggers>` 声明 WPF 风格的事件动作。这里的
`EventTrigger RoutedEvent="..."` 与进入 VisualState 的 `EventTrigger Event="..."` 是两种明确语义：
前者执行有序 TriggerAction，后者仍只负责状态选择。

```xml
<StackPanel.Triggers>
  <EventTrigger RoutedEvent="Started">
    <BeginStoryboard x:Name="Pulse">
      <Storyboard>
        <DoubleAnimation Storyboard.TargetName="chrome"
                         Storyboard.TargetProperty="ValidationCornerRadius"
                         To="12" Duration="0:0:0.25" />
      </Storyboard>
    </BeginStoryboard>
  </EventTrigger>
  <EventTrigger RoutedEvent="Paused">
    <PauseStoryboard BeginStoryboardName="Pulse" />
  </EventTrigger>
  <EventTrigger RoutedEvent="Resumed">
    <ResumeStoryboard BeginStoryboardName="Pulse" />
  </EventTrigger>
  <EventTrigger RoutedEvent="Stopped">
    <StopStoryboard BeginStoryboardName="Pulse" />
  </EventTrigger>
</StackPanel.Triggers>
```

`RoutedEvent` 必须是组件已声明事件；动作按 XAML 顺序执行。`BeginStoryboard` 必须包含且只包含
一个 Storyboard，可用唯一 `x:Name` 使其受后续 Pause/Resume/Stop 控制；控制动作的
`BeginStoryboardName` 必须在同一模板根 Triggers 中解析。事件 Storyboard 复用已有全部动画类型、
Timeline 时钟和对象路径适配器，不建立第二套窄化动画模型。

事件时钟输出进入独立的 `Animation` 值源，优先级高于 `VisualState`。因此 Pause 保持当前帧，
Resume 从暂停位置继续，Stop 或 `FillBehavior="Stop"` 只释放动画层；如果被遮挡的视觉状态在
动画期间已变化，停止后显示的是新状态值，而不是 Begin 时捕获的过期快照。

### 声明组件 Behavior

`ComponentDefinition` 本身已经拥有类型、公开属性/事件、内容槽、模板和样式。应用若只需补充业务状态、消息处理或
最终绘制，可通过 `DeclarativeComponentBehaviorRegistry` 按组件的精确“命名空间 URI + 类型名”注册
`IDeclarativeComponentBehavior`。这不是第二套控件注册机制：工厂接收已经物化的组件上下文，只能返回行为对象，
不能创建、替换宿主或改变 XAML 的类型 schema。

行为契约如下：

- `Attach` 在完整模板/内容树、动态属性/事件、模板 Binding、样式和布局属性安装后调用；嵌套组件先于外层组件附加；
- `FindDeclarativeTemplatePart(localName)` 提供当前模板实例的局部 `x:Name` 查询，
  `FindDeclarativeContentPresenter(propertyName)` 提供声明内容槽的生成 Presenter；
- 行为可订阅普通 Control 事件、用 key-equivalent API 更新只读组件状态、用 `RaiseDeclarativeEvent` 发布声明事件；
- `HandleMessage` 在宿主正常 `ProcessMessage` 前运行，返回 true 即消费；坐标始终是宿主局部 DIP；
- `RenderOverlay` 在宿主普通绘制扩展之后、焦点/校验装饰之前运行，并受宿主局部变换和裁剪约束；
- `DpiChanged` 与 `DeviceResourcesInvalidated` 显式通知非布局资源变化；Behavior 不接管 Measure/Arrange；
- 宿主独占拥有一个行为。只要热重载复用宿主，行为与其内部状态一并保留；宿主替换或析构时先 `Detach`，再销毁
  模板子树。显式提供新的行为注册表会强制候选树完整替换，工厂/Attach 失败则旧运行树保持不变。

工厂参数 `DeclarativeComponentBehaviorContext` 只在工厂与紧随其后的 `Attach` 调用期间有效；行为若需长期使用，
只能复制 QName/稳定 ID/实例名或保存由宿主拥有的 Control 指针。没有注册项不是错误，组件继续以纯 XAML 运行。
设计器从不加载应用行为，因此设计时展示与属性编辑仍完全由 XAML 契约决定。

声明组件 Behavior 适合“XAML 决定外观和布局，C++ 补充业务”的场景。3D 场景、矢量编辑画布等由应用持续绘制、
测量或深度接管输入的区域，应使用下面的 `NativeSurface`，而不是逐步把声明组件 Behavior 扩张成自定义控件。

### Native Surface 与 Behavior

高需求 C++ 场景使用内置 `NativeSurface` 作为 XAML 可布局、可裁剪、可变换、可命中和可访问的
宿主。C++ 只注册 Behavior，不注册控件类型：

- `Attach/Detach`：明确 UI 线程和生命周期；
- `Render(RenderContext&)`：只在宿主给定的局部坐标、裁剪和资源范围内绘制；
- `Pointer/Keyboard/Focus`：接收规范化输入，可选择是否消费；
- `Message`：面向业务的强类型/稳定 ID 消息，不暴露设计器内部对象；
- `Measure`：可选内容期望尺寸，默认由 XAML Width/Height/布局决定；
- 设备丢失、DPI、可见性和宿主替换均有显式通知。

Behavior 的注册键是应用配置，不写入组件类型系统。`NativeSurfaceBehaviorRegistry` 在应用启动时按
`BehaviorKey` 注册工厂；运行时未解析到非空键会拒绝材质化，设计器则显式启用占位模式。行为获得局部 DIP
输入、受宿主裁剪的绘制上下文、可选测量，以及 DPI/设备丢失通知。这样产品设计器无需加载用户 C++，运行时
仍保留全部自由度。

## Binding 来源与名称域

普通 `{Binding Path}` 以目标控件当前的有效 `DataContext` 为默认来源。`DataContext` 是可继承属性：本地值或
Binding 高于父级继承值，清除本地值后立即恢复继承；`DataContext="{Binding Profile}"` 使用父级有效上下文
求值，子孙的 `{Binding Name}` 随后相对于 `Profile` 求值。稳定的来源代理会在中间对象或整段 DataContext
替换时重连现有 Binding，而不要求重新创建控件。

Binding 支持目标类型化的 `FallbackValue` 与 `TargetNullValue`。源对象不可用、路径中间段未解析、读取失败、
Converter 失败或目标转换失败时使用 `FallbackValue`；源明确返回 Empty 时优先使用 `TargetNullValue`。
缺省值仍经过目标属性元数据转换与 Coerce，不以字符串旁路属性系统。来源随后恢复、再次丢失或在 Empty/正常值间
切换时，同一个 Binding 会实时恢复真实值或重新应用缺省值。带逗号、花括号等标记字符的字符串使用引号参数，
规范 XAML 会稳定转义并往返；设计器编辑器用独立启用开关区分“未设置”和“已设置为空字符串”。

Binding PropertyPath 使用统一的成员/索引器步骤模型。`People[0].Name` 从 `IBindingList` 读取记录，订阅列表结构、
当前索引记录以及叶属性三层变化；增删、Replace、越界后恢复和中间集合替换都会重建后续订阅而不重建控件。
`Settings[key]` 与 `Settings['accent.color']` 在 `IBindingSource` 上按原样键访问，后者允许键名包含点和空白；引号通过
重复自身转义。键索引叶支持 TwoWay，列表索引本身保持只读，但 `People[0].Name` 可通过记录属性元数据 TwoWay 回写。
解析、运行时读写、校验聚合、集合视图路径观察、设计器校验/活动校验、规范 XAML、v14 XML、剪贴板依赖和静态辅助
生成共用同一语法。DataContext Schema 仍只声明稳定的成员契约；索引器不会被伪装成 Schema 字段，而是先校验其
BindingList/BindingSource 容器，索引后的具体值由运行时元数据解析。

Binding 的转换/显示层支持 `ConverterParameter` 与 `StringFormat`。参数作为独立的强类型 `BindingValue` 通过
`BindingValueConverterContext` 同时传入 `Convert`/`ConvertBack`；旧的无上下文 Converter 重载仍保持源码兼容。
`StringFormat` 只用于 String 目标，在 Converter 之后、目标属性元数据转换之前执行，不参与反向解析。首批使用
单值复合格式：`{0}`、重复占位、左右对齐、`{{`/`}}` 转义、`C/D/E/F/G/N/P/X` 精度以及
`0/#/./,/%` 数字模式，结果固定使用不变区域规则；XAML 中前导 `{}` 可避免格式文本被当作标记扩展。
格式语法会在解析/设计阶段校验，运行时值与格式不兼容则进入 `FallbackValue`。参数和格式贯通页面、DataTemplate、
组件模板、设计器预览/编辑、规范 XAML、v14 XML、热重载、剪贴板和静态辅助生成；代码生成契约由此提升到 v10。

`MultiBinding` 使用 WPF 式属性元素表达多个输入；每个子 `<Binding>` 都是真实的普通 Binding，因此复用相同的
PropertyPath、索引器、ElementName/RelativeSource、来源寿命、缺省值、Converter 和校验订阅。顶层可用带索引的
`StringFormat="{}{0} / {1}"` 直接组合显示，也可注册 `IMultiBindingValueConverter`；TwoWay/OneWayToSource 必须由
多值 Converter 的 `ConvertBack` 返回与子项等长的值列表。子项未显式声明 Mode/UpdateSourceTrigger 时继承顶层，
显式值则覆盖。动态页面、DataTemplate、组件模板、设计器预览、规范 XAML、v14 XML、热重载和剪贴板共享同一递归
定义与安装入口。结构化 Binding 对话框当前只读保留 MultiBinding，编辑在简化 XAML 编辑器完成；静态 C++ 辅助
生成明确拒绝 MultiBinding，避免复制一套动态来源与多值协调语义。区域性格式仍未实现，格式化固定使用不变区域规则。

`{Binding Path, ElementName=name}` 以当前局部 namescope 中具有该 `x:Name` 的控件为来源；
`{Binding Path, RelativeSource={RelativeSource Self}}` 以目标控件自身为来源；组件模板内部还支持
`RelativeSource TemplatedParent` 指向组件实例。`RelativeSource FindAncestor` 按逻辑父链查找指定内置控件基类
或声明组件类型，并支持从 1 开始的 `AncestorLevel`；父链变化时稳定来源代理会重新查找并让既有 Binding 自动切源。
页面、每个 `DataTemplate` 实例和每个组件模板实例分别拥有独立
名称域，引用不能越过边界。组件模板展开时，局部名称会改写为实例隔离的运行时名称，但规范 XAML 始终保留作者
书写的局部 `ElementName`。DataTemplate 的视觉根持有记录级本地 DataContext，接入 ItemsHost 后不会被宿主覆盖。

`Control` 本身实现 `IBindingSource`。属性元数据的旧式变更订阅（如 `OnTextChanged`、`OnChecked`）会统一桥接到
`PropertyChanged`，因此用户交互、属性系统写入、TemplateBinding 与普通 C++ setter 都可驱动 ElementName
绑定，且一次变化只发布一次。设计器 Binding 编辑器允许在 DataContext、Self、FindAncestor 与当前页面命名控件
之间选择来源，并编辑 `AncestorType`/`AncestorLevel`；
重命名控件会事务式重写引用，删除或仅复制被引用目标会被依赖检查拒绝，整棵复制则同步重映射名称。

当前仍不支持点号形式的名称查找、跨 namescope 引用或任意对象源；MultiBinding 子项也不能嵌套 MultiBinding。
CollectionViewSource、DataTrigger 等仍明确使用 DataContext，不借用 ElementName/RelativeSource 产生隐式语义。
静态 C++ 代码生成是迁移辅助路径；`TemplatedParent` 与动态父链 `FindAncestor` 明确要求使用动态 XAML。控件级 DataContext 的静态输出仍是
后续兼容项，不能复制一套与动态运行时不同的上下文继承语义。

## 属性与资源的后续扩展

对象/集合属性不能简单塞进 `std::any`。Color、Thickness、Size、Length 已作为具有具体运行时类型的值契约加入；
Enum 使用显式 `ComponentProperty.Choices` 定义封闭字符串值域，赋值会校验并规范到声明大小写，不依赖任意 C++ enum。
Brush、Geometry、Transform 默认值使用 `ComponentProperty.Default` 的结构化对象树，已进入 TemplateBinding、样式、Binding 和快照通道。
任意已支持类型也可用 `Default="{StaticResource Key}"` 引用同类型资源；资源值先于本地组件 schema 与 Style 发现，
因此不要求作者调整书写顺序。视觉控件集合由 ContentProperties/Presenter 闭环，强类型数据集合由
`DataType + DataList/IBindingList + DataTemplate + ItemsControl` 闭环。每种后续类型必须同时具备解析、
规范输出、属性编辑、默认值比较、Binding 能力和资源依赖收集。

组件定义属于资源系统。文件/目录只是当前资源来源；合并字典中的组件保留来源，不在主文档中展开。未来产品包
资源源仍通过 `IResourceSource` 提供字节、稳定 Identity、BaseUri、依赖和监视信息，组件解析层不得直接访问磁盘。

## 实施顺序

1. 已完成：组件身份、标量属性、实例、设计器/运行时/持久化。
2. 已完成：`ComponentDefinition.Template`、模板局部身份、模板展开和 `TemplateBinding`。
3. 已完成第一批：组件声明事件、设计器事件页、运行时名称路由和模板 `RaiseEvent`。
4. 已完成第一批：组件 QName TargetType、默认样式、Setter/Trigger 对组件属性的校验与运行时隔离。
5. 已完成视觉内容属性第一批：Single/Multiple、默认内容、命名属性元素、模板 Presenter、设计器布局/剪贴板/热重载。
6. 已完成值类型、结构对象与默认资源引用第一批：Enum、Color、Thickness、Size、Length、Brush、Geometry、Transform。
7. 已完成强类型数据集合第二批：`IBindingList`、记录 `DataType`、文件 `DataList/DataRecord`、`DataTemplate`、
   通用 `ItemsControl`、类型过滤属性选择器、规范 XAML/v14 XML、剪贴板和事务热重载。
8. 已完成集合视图第一批：`CollectionViewSource`、强类型筛选、稳定多键排序、CurrentItem、静态/动态 Source、
   视图链、增量投影通知以及设计器资源全链路。
9. 已完成集合分组第一批：多级 `GroupDescriptions`、实时分组快照、`GroupStyle`/HeaderTemplate、容器身份保持、
   属性候选、剪贴板隔离、规范 XAML 与 v14 XML；命名聚合与固定项高分组虚拟化使用专用段索引。
10. 已完成第一批：`NativeSurface`、可替换 Behavior、注册表、设计器占位和宿主生命周期通知。
11. 已完成公开路径与模型清理：删除运行时 C++ 控件工厂/自定义事件注册、设计器清单与插件命令行入口，旧 XML
   自定义控件元数据只会被明确拒绝。代码生成只保留为可选迁移/辅助工具，遇到声明组件文档会要求使用动态 XAML。
12. 已完成 Binding 来源第一批：默认 DataContext 与局部 `ElementName`、控件属性通知桥接、页面/DataTemplate/
   组件模板 namescope、设计器来源选择、规范 XAML/v14 XML、热重载、重命名和剪贴板依赖闭环。
13. 已完成 Binding 来源第二批：可继承控件级 DataContext、父上下文求值、稳定来源替换、`RelativeSource Self`、
   组件模板 `TemplatedParent`、作用域 Schema/设计器预览、DataTemplate 记录上下文和原位热重载。
14. 已完成 Binding 来源第三批：`RelativeSource FindAncestor`、内置类型基类匹配、声明组件精确类型匹配、
   `AncestorLevel`、父链变化动态重解析、设计器编辑/预览、规范 XAML/v14 XML 与动态运行时闭环。
15. 已完成 Binding 缺省值第一批：目标类型化 `FallbackValue`/`TargetNullValue`、失败与 Empty 分流、来源恢复、
    引号参数、设计器编辑/预览、规范 XAML/v14 XML、DataTemplate/组件模板与静态辅助生成。
16. 已完成 Binding PropertyPath 索引器第一批：公共成员/索引步骤模型、`IBindingList` 数字索引、`IBindingSource`
    键索引、集合/记录动态重订阅、TwoWay 键与记录叶写回、Schema 容器校验、设计器路径编辑/预览及全格式往返。
17. 已完成 Binding 转换/显示第一批：上下文 Converter、`ConverterParameter`、String 目标 `StringFormat`、
    不变区域单值复合格式、TwoWay ConvertBack、失败回退、设计器编辑与全持久化/运行时/静态生成闭环。
18. 已完成 Binding 多源表达第一批：WPF 属性元素 `MultiBinding`、任意索引复合格式、`IMultiBindingValueConverter`/
    ConvertBack、子 Binding 完整语义复用、页面/DataTemplate/组件模板/设计器预览、规范 XAML 与 v14 XML 闭环；
    静态 C++ 辅助生成保持明确边界。
19. 已完成声明属性行为元数据第一批：`Inherited` 属性值层、组件限定名隔离的 `Inherits`、
    `BindingMode::Default`/`BindsTwoWayByDefault`、`AffectsParentMeasure`/`AffectsParentArrange`，以及设计器、
    规范 XAML、v14 XML、动态运行时和错误回滚闭环。
20. 已完成声明属性行为元数据第二批：`DataSourceUpdateMode::Default`、目标属性
    `DefaultUpdateMode`、组件 `DefaultUpdateSourceTrigger`、`TextBox.Text` 的 `TwoWay + LostFocus` 契约，
    以及 Binding/MultiBinding、设计器编辑、规范 WPF 拼写、v14 兼容和动态运行时闭环。
21. 已完成显式更新表达式闭环：Binding/MultiBinding 对称公开 `UpdateSource()`/`UpdateTarget()`，
    `BindingCollection` 可按目标属性统一调度；MultiBinding 手动提交会执行 ConvertBack、提交 Explicit 子项并传播
    Converter/源写入错误，OneTime 多源表达也可显式重新拉取全部来源。
22. 已完成声明只读属性第一批：`ComponentProperty ReadOnly`、key-equivalent Behavior 写入、继承/默认值恢复、
    Binding/TemplateBinding 源、目标/Style/公开写入拒绝、属性栏只读展示，以及规范 XAML、v14 XML、动态运行时和
    事务错误回滚闭环。
23. 已完成声明组件 Behavior 第一批：按精确 QName 的可选注册表、Attach/Detach 宿主所有权、模板部件与内容
    Presenter 查询、只读状态/声明事件入口、消息预处理、最终 overlay、DPI/设备通知，以及原位复用、拓扑重组、
    显式替换和 Attach 失败事务回滚。
24. 已完成声明组件路由事件第一批：Direct/Bubble/Tunnel、所有者 QName 身份、WPF 式附加事件属性、可变
    `DeclarativeEventArgs`/Handled/HandledEventsToo、模板公开 Source、规范 XAML/v14、处理函数索引与改名、剪贴板
    依赖闭包，以及事件连接原位热重载和失败回滚。
25. 已完成声明组件视觉状态第一批：`VisualStateGroup`/`VisualState`、回退状态、AND 条件 `StateTrigger`、组件
    `EventTrigger`、模板部件强类型 Setter、独立 `VisualState` 值源、Behavior 查询/切换/通知，以及规范 XAML/v14、设计器预览、
    资源依赖、剪贴板、原位/完整事务热重载和失败回滚。
26. 已完成 Storyboard/Timeline 第一批：有限 `DoubleAnimation`/`ColorAnimation`、可选 From/BeginTime、当前值捕获、
    Linear/Quadratic/Cubic/Sine 缓动、HoldEnd、系统减弱动态效果、统一 VisualState 值源、动画资源依赖/重映射/完整
    候选热重载，以及确定性手动时钟测试入口；其余时间线语法继续按批次扩展。
27. 已完成 Storyboard PropertyPath/Transform 第一批：通用 WPF 式成员/索引语法与 RenderTransform 对象适配器分层，
    支持 Translate/Scale/Rotate/Skew 数值成员；同根多末端逐帧合成、整根状态退出恢复、资源端点、规范 XAML/v14、
    设计器校验与热重载共用同一契约。
28. 已完成 Storyboard 关键帧第一批：Double/Color UsingKeyFrames 及 Discrete/Linear/Easing/Spline 四类帧，支持资源端点、
    省略 Duration 推导、同时间稳定顺序、进入状态基础值捕获、KeySpline 和 RenderTransform 子路径合成；规范 XAML、v14、
    设计器资源校验/剪贴板重映射、事务热重载及确定性时钟测试使用同一模型。
29. 已完成 VisualTransition 第一批：From/To/default 确定性匹配、GeneratedDuration/GeneratedEasing、显式 Storyboard
    目标覆盖、Double/Color 生成过渡、离开状态回基础值、当前帧中断与 useTransitions/系统动画旁路；规范 XAML、v14、
    设计器资源校验、剪贴板重映射、事务热重载和失败不污染输出文档共用同一契约。
30. 已完成 Timeline 活动期第一批：RepeatBehavior Count/Duration/Forever、分数重复、AutoReverse、FillBehavior
    HoldEnd/Stop、关键帧反向采样、Transform 子路径独立释放、Forever 状态/Transition 中断及系统动画旁路；状态与
    Transition 共享 XAML、v14、设计器校验、热重载和确定性时钟模型。
31. 已完成 Timeline 速度第一批：SpeedRatio、AccelerationRatio 与 DecelerationRatio 使用 WPF 本地时钟和归一化
    峰值速率；BeginTime 不缩放、Count/Duration RepeatBehavior 保持不同父时钟语义，普通/关键帧/Transition 共用
    XAML、v14、设计器校验、热重载、事务回滚和确定性非线性采样。
32. 已完成动画端点组合第一批：普通 Double/Color 动画支持可选 From/To/By 和 Automatic/From/To/By/FromTo/FromBy
    解析，To 优先、By 数值/颜色/Transform 增量、状态基础目标与 Transition 当前目标使用同一运行时端点解析器；
    规范 XAML、v14、设计器未使用组件校验、资源闭包/冲突重映射、生成/显式过渡、事务热重载和非法输入回滚均已覆盖。
33. 已完成 Additive/Cumulative 第一批：普通与 Double/Color 关键帧动画使用 WPF foundation/accumulation 矩阵，
    Count/Duration/Forever、分数重复与 AutoReverse 共用精确 iteration 索引；数值、颜色和 RenderTransform 子路径共享
    局部值+累计量+foundation 合成，生成 Transition 转为绝对目标并清除标志，显式 Transition 保留作者语义。规范
    XAML、v14、设计器校验、热重载、无效布尔值回滚与父/本地时钟边界均已覆盖。
34. 已完成 Object 关键帧第一批：`ObjectAnimationUsingKeyFrames` / `DiscreteObjectKeyFrame`
    支持 Visibility/bool/枚举/string/Thickness 等标量、StaticResource 以及内联 Brush/Geometry/Transform；
    时间线活动期、显式 Transition、生成 Transition 基础值语义、规范 XAML、v14、设计器验证、
    资源剪贴板重映射、热重载和非法输入回滚共用同一元数据契约。
35. 已完成 Thickness 动画第一批：`ThicknessAnimation` 与 `ThicknessAnimationUsingKeyFrames` 覆盖
    Automatic/From/To/By、Easing、Additive/Cumulative、Discrete/Linear/Easing/Spline 帧和四边分量插值；Margin/Padding、
    声明组件 Thickness 属性、StaticResource、生成/显式 Transition、规范 XAML、v14、剪贴板、热重载及事务回滚共享同一管线。
36. 已完成浮点 Size 与动画第一批：声明式 `Size` 从 Win32 整数 `SIZE` 收口到 `cui::core::Size`，MinSize/MaxSize 与
    ScrollView.ContentSize 直接持有浮点 DIP，旧 `SIZE` API 仅作兼容投影；`SizeAnimation` / `SizeAnimationUsingKeyFrames`
    覆盖 Automatic/From/To/By、普通/关键帧 Easing、Additive/Cumulative、四类关键帧、生成/显式 Transition、资源、
    规范 XAML、v14、热重载及事务失败回滚。同时纠正并覆盖 WPF 合法的 `EasingThicknessKeyFrame`。
37. 已完成 Point 与动画第一批：`RenderTransformOrigin` 从设计器 Extra 特判迁移为 `cui::core::Point` 元数据，
    声明组件可直接定义 Point 属性并参与 TemplateBinding、样式、资源和绑定；`PointAnimation` /
    `PointAnimationUsingKeyFrames` 覆盖 Automatic/From/To/By、普通/关键帧 Easing、Additive/Cumulative、
    四类关键帧、生成/显式 Transition、规范 XAML、v14、资源热重载和事务失败回滚。旧 v14 Extra 仍可读取，
    下一次规范 XAML 输出会收口到公开 `RenderTransformOrigin` 属性。
38. 已完成 Rect 与 Geometry 子属性动画第一批：声明组件可直接定义 Rect 属性，`RectAnimation` /
    `RectAnimationUsingKeyFrames` 覆盖 Automatic/From/To/By、普通/关键帧 Easing、Additive/Cumulative、四类关键帧、
    StaticResource、生成/显式 Transition、规范 XAML、v14 与热重载；具名模板部件可用
    `(Control.Clip).(RectangleGeometry.Rect)` 只动画矩形裁剪范围并保留同根圆角/变换。路径别名、根所有权、无 Clip、
    非 RectangleGeometry、非法有限值与事务失败回滚均进入统一验证。
39. 已完成 Vector 与对象路径适配边界第一批：新增 `cui::core::Vector`，Point/Vector 运算不再混用两个 Point；
    声明组件 Vector 属性、资源、样式、代码生成、`VectorAnimation` / `VectorAnimationUsingKeyFrames`、
    `EasingVectorKeyFrame`、Automatic/From/To/By、Additive/Cumulative、生成/显式 Transition、规范 XAML、v14、
    热重载和事务失败回滚进入统一管线。运行时 Transform/Geometry 路径收口为单一 `ObjectPathAccessor` 变体，
    Designer 的分类、规范路径、根属性和解析入口也已统一，为下一批 Brush/GradientStop 子属性动画预留注册边界。
40. 已完成 GradientStop Color/Offset 对象路径动画：线性与径向 Foreground Brush 可通过规范化集合索引路径定位 Stop，
    ColorAnimation/DoubleAnimation 的普通时间线、四类关键帧、StaticResource、显式 Transition、FillBehavior、规范 XAML、
    v14 和热重载共用现有时间线管线；同根多成员逐帧合成并保留其余 Brush 数据。无显式 Brush、非渐变 Brush、越界索引、
    类型不匹配和非法 Offset 会在解析/设计器/快照/运行时边界事务拒绝。
41. 已完成 WPF 风格 Brush Transform：Solid/LinearGradient/RadialGradient/Image Brush 支持 `Transform` 与
    `RelativeTransform` 属性元素、规范 XAML、v14、Materializer、静态代码生成和 D2D 渲染。RelativeTransform 在归一化
    画刷空间应用，随后映射到目标边界，再应用 DIP 空间 Transform。统一对象路径适配器支持普通/关键帧、StaticResource、
    显式 Transition、热重载及与 GradientStop 同根逐帧合成；所有者/索引/操作类型不匹配和缺失 Transform 均事务拒绝。
42. 已完成 Geometry.Transform 对象路径动画：Rectangle/Ellipse/Path/GeometryGroup 的 Translate/Scale/Rotate/Skew
    数值成员进入统一 DoubleAnimation/UsingKeyFrames 管线，支持具体所有者与 `UIElement.Clip` 规范化、StaticResource、
    显式 Transition、规范 XAML、v14 和热重载。Geometry Transform 与 RectangleGeometry.Rect 在同一 Clip 根值上逐帧
    合成；缺失 Geometry/Transform、所有者或操作索引/类型不匹配、非法快照均在设计器和运行时边界事务拒绝。
43. 已完成 Brush 公开成员对象路径动画：SolidColorBrush.Color、通用 Brush.Opacity、LinearGradientBrush 的
    StartPoint/EndPoint、RadialGradientBrush 的 Center/GradientOrigin/RadiusX/RadiusY 分别进入 Color/Double/Point
    普通与关键帧时间线。具体 Brush 与 UIElement 作者别名规范化、StaticResource、显式 Transition、规范 XAML、v14、
    热重载及失败原子性共用统一适配器；这些成员与 GradientStop、Transform 在同一 Foreground 根值上逐帧合成。
    Opacity 绝对端点限制为 0..1、半径绝对端点必须非负，所有者/动画类型/缺失 Brush/非法快照均事务拒绝。
44. 已完成 Rectangle/Ellipse 公开 Geometry 成员对象路径动画：RectangleGeometry.RadiusX/RadiusY 与
    EllipseGeometry.Center/RadiusX/RadiusY 分别进入 Double/Point 普通与关键帧时间线，并覆盖 StaticResource、生成/显式
    Transition、规范 XAML、v14、热重载和失败原子性。UIElement 根所有者规范化、具体 Geometry 所有者校验、半径非负绝对端点、
    动画类型和缺失 Clip 均由统一适配器验证；Rect、Center、半径和 Geometry.Transform 在同一 Clip 根值上逐帧合成。
45. 已完成 PathGeometry 深层索引对象路径动画：PathFigure 的 StartPoint/IsClosed/IsFilled，Line/Bezier/
    QuadraticBezier/Arc 的点成员，以及 Arc 的 Size/RotationAngle/IsLargeArc/SweepDirection 进入统一 Point/Size/Double/
    Object 时间线。Figure/Segment 索引、具体 Segment 所有者、值类型、Arc Size 非负约束和 SweepDirection 枚举均在设计器与
    运行时边界验证；StaticResource、普通/关键帧、生成/显式 Transition、规范 XAML、v14、热重载和失败原子性共用现有
    适配器。这些路径与 FillRule、Geometry.Transform 和未动画 Figure/Segment 数据在同一 Clip 根值上逐帧合成。
46. 已完成 GeometryGroup.Children 递归对象路径：任意层 `(GeometryGroup.Children)[n]` 后可继续定位 Rectangle/Ellipse
    公开成员、PathFigure/PathSegment 深层成员和子 Geometry.Transform；访问器保存完整子索引链，设计器 JSON 与运行时
    Geometry 以同一导航语义验证实际 Group、索引和最终具体所有者。PathGeometry/GeometryGroup.FillRule 同时进入离散
    Object 关键帧，值限制为 EvenOdd/Nonzero。规范 XAML、v14、StaticResource、普通/关键帧、生成/显式 Transition、
    热重载和失败原子性均覆盖；父级变换、兄弟 Geometry 与未命中子数据在同一完整 Clip 根值上保持不变。
47. 已完成 Matrix 强类型值与动画：`<Matrix x:Key>`、声明属性、属性元数据、静态辅助生成和 v14 使用
    `D2D1_MATRIX_3X2_F` 统一表示；`MatrixAnimation` / `MatrixAnimationUsingKeyFrames` 支持 From/To/By、
    Easing、Additive/Cumulative、Discrete/Linear/Easing/Spline、生成/显式 Transition 与资源热重载。
    RenderTransform、任意层 Geometry.Transform、Brush Transform/RelativeTransform 的
    `MatrixTransform.Matrix` 通过既有对象图适配边界定位，并可与同根 Double/Geometry/Brush 末端逐帧合成；
    非有限分量、动画类型、所有者、索引和实际操作不匹配均在 XAML、v14 与运行时边界事务拒绝。
48. 已完成组件事件 Storyboard 第一批：模板根 `<RootType.Triggers>`、`EventTrigger.RoutedEvent`、
    `BeginStoryboard`、`PauseStoryboard`、`ResumeStoryboard` 和 `StopStoryboard` 支持命名时钟控制；
    全部现有动画类型/对象路径、规范 XAML、v14、资源依赖、剪贴板、设计器预览与热重载复用同一模型。
    事件时钟使用高于 `VisualState` 的独立 `Animation` 值层，停止后会显露当前状态/本地值，不回填过期快照。
49. 已完成 Style DataTrigger 上下文收口：单/多数据条件针对每个目标控件的有效 DataContext 独立求值，
    点分路径观察挂在目标稳定的 `DataContextSource` 上，并在继承来源或中间对象替换后重连。DataTemplate 中共享
    Style 的多个实例不再通过样式表全局 DataContext 互相覆盖；样式表级 SetDataContext 仅作为无目标上下文时的兼容回退。
50. 已完成 Style DataTrigger 动作第一批：`DataTrigger` / `MultiDataTrigger` 的 `EnterActions` 与
    `ExitActions` 可执行 Begin/Pause/Resume/StopStoryboard，并复用全部既有动画类型、对象路径与 Timeline。
    共享 Style 只保存定义；静态选择器匹配后，每个目标按数据条件边沿建立独立命名时钟，普通刷新不重启，规则或
    样式表移除会停止并释放。Style 没有模板 namescope，Storyboard 必须省略 TargetName。规范 XAML、v14、
    设计器预览/撤销、热重载、剪贴板资源重命名和组件交互共存已闭环；辅助静态 C++ 生成器明确拒绝而不静默降级。
51. 已完成 Style 属性 Trigger 元数据化：六个交互状态继续作为兼容别名，其他 `Trigger.Property` /
    `MultiTrigger Condition.Property` 直接解析目标控件的可读、可观察属性元数据，值按真实类型转换比较；普通属性条件
    与状态条件可在 MultiTrigger 中混合，并自动订阅目标属性变化。`Trigger` / `MultiTrigger` 同时接入
    `EnterActions` / `ExitActions` 的每目标命名时钟，静态类型/Id/Class 只决定动作作用域，状态、属性和数据条件统一
    决定活动边沿，从而保证任一动态条件退出时都执行 Exit。规范 XAML、v14、热重载、剪贴板特异性、撤销内存估算、
    动态运行时和辅助静态生成边界已贯通。
52. 已完成 DynamicResource 属性表达式第一批：可写控件属性及 Style/Trigger Setter 支持
	`{DynamicResource Key}`，直接属性表达式占用 Local 值槽并按当前控件、逻辑父链、文档样式表、
	主题/Application 样式表逐级查找；
	资源更新、样式表替换、控件树继承及缺失后恢复会重新求值，普通 Local 写入/ClearValue 会移除表达式。
	Static/Dynamic 身份已进入规范 XAML、当前 v15 快照、设计器捕获/恢复与资源重命名、剪贴板、原位热重载和辅助 C++ 生成；
    Style/BasedOn/ItemsSource/模板选择等结构引用继续保持 StaticResource 边界。
53. 已完成控件级值资源词法作用域第一批：任意内置控件或声明组件实例可通过 `<Owner.Resources>` 持有自己的
    ResourceDictionary，支持现有标量、Thickness、Brush、ImageSource、Geometry、Transform 与文件型
    MergedDictionaries。运行时按“自身 → 逻辑父级 → 文档 → Application/主题”查找，近端同名键遮蔽远端；
    字典内容变化和子树换父级都会递归刷新直接 DynamicResource 及 Style 中的 DynamicResource Setter。
    局部字典保持在 `DesignNode.LocalResources`，已进入规范 XAML、XML v15、组件/DataTemplate 模板节点、
    Designer 捕获、事务重组和辅助 C++ 生成。Style/DataTemplate/ComponentDefinition 等结构型局部资源暂时
    明确拒绝，下一批在对象资源词法索引完成后开放，避免伪局部的全局注册语义。
54. 已完成控件级 `Style` 词法作用域第一批：局部字典可声明隐式/命名 Style、BasedOn、Setter、Trigger、
    DataTrigger 与动作；可见上下文按“文档 → 祖先 → 当前”组合，选择器仍按特异性级联，同分时近端规则覆盖。
    StaticResource 在局部运行时表中捕获为私有别名，DynamicResource 保留键并在实际目标父链求值。规则进入规范 XAML、
    XML v16、普通与组件/DataTemplate 模板节点、Designer 样式编辑/资源重命名、剪贴板闭包、事务热重载和辅助代码生成；
    文档基样式变化会重建依赖它的局部表，模板内部 BasedOn 也可前向看到同一 Form.Resources 中的样式声明。
    DataTemplate/ComponentDefinition 等结构对象的局部注册仍等待统一对象资源索引，继续显式拒绝。
55. 已完成统一局部对象资源索引第一批：`DataTemplate` 与 `ComponentDefinition` 可进入任意
    `<Owner.Resources>`，按当前控件、逻辑祖先、文档资源建立词法遮蔽；组件实例、ItemsControl 模板、组件模板和
    DataTemplate 内的嵌套对象资源共用同一查找规则。定义及其声明作用域值资源随规范 XAML、XML v17、Designer
    包装模型、事务重组和热重载保持；属性栏的 ItemTemplate 选择按当前控件作用域投影。复制离开声明宿主的子树时，
    剪贴板会把实际命中的局部模板/组件及其可见值资源提升到片段根，因而同名遮蔽和仅局部定义均可独立粘贴。动态对象继续不进入辅助
    C++ UI 展开器，遇到局部对象资源会明确拒绝。文件型 MergedDictionaries 仍通过 Application 资源来源层加载，
    导入定义保留 Source 而不回写展开内容。
56. 已完成统一局部对象资源索引第二批：`ItemsPanelTemplate` 与 `GroupStyle` 可进入任意
    `<Owner.Resources>`，ItemsControl/ListBox 按当前控件、逻辑祖先、文档资源解析并遮蔽同名定义；属性栏的
    ItemsPanel/GroupStyle 候选也投影同一可见集合。`GroupStyle.HeaderTemplate` 固定在 GroupStyle 声明作用域
    解析，避免使用处更近的同名 DataTemplate 改写既有样式含义。规范 XAML、XML v18、普通/组件/DataTemplate
    视觉节点、Designer 捕获、事务重组、热重载和运行时物化均保存这两类局部资源。剪贴板会把脱离宿主后实际命中的
    面板模板、组样式、组头模板和值资源作为闭包提升到片段根；同名遮蔽和无全局兜底的局部列表资源可独立粘贴运行。
    辅助 C++ UI 展开器继续对局部对象资源明确拒绝，不把动态 XAML 对象悄然扁平化。
57. 已完成隐式 `DataTemplate` 类型键第一批：省略 `x:Key` 的模板以 `DataType` 作为独立资源身份，
    不占用字符串资源键空间；ItemsControl/ListBox 在未显式设置 ItemTemplate 时，从强类型 DataList、
    CollectionViewSource 或 ItemsSource Binding Schema 推断项类型，并按“当前控件 → 逻辑祖先 → 文档”选择
    最近的同类型模板。显式 ItemTemplate 始终优先。GroupStyle 未声明 HeaderTemplate 时同样在样式声明作用域
    自动查找 `DataType="CollectionViewGroup"` 的隐式模板，避免使用处改写样式语义。属性栏以“自动”表示空
    ItemTemplate 且只列出可显式引用的有键模板；规范 XAML 不输出空 x:Key，XML v19、热重载、模板嵌套与
    剪贴板闭包保留类型键及局部遮蔽。粘贴到已有不同契约的同类型隐式模板时事务拒绝，不静默替换视觉契约。
58. 已完成强类型单对象呈现第一批：`BindingSource` Schema 属性可用 `DataType` 关联已有记录契约；
    `ContentPresenter.Content` 接收单个 `BindingSourceReference`，显式 `ContentTemplate` 优先，否则按内容类型沿
    当前控件、逻辑祖先、文档资源选择隐式 `DataTemplate`。模板视觉根以内容对象为 DataContext，内容替换和
    字段通知保持绑定活性；无模板时使用 `DisplayMemberPath` 文本后备。控件拒绝直接视觉子节点，避免 authored
    tree 与 generated tree 双重所有权。属性栏按 Content Schema 过滤模板候选，规范 XAML、XML v20、局部资源
    遮蔽、嵌套模板依赖、剪贴板提升、Designer 预览和事务热重载均复用同一物化链。
59. 已完成 WPF 风格默认内容宿主第一批：新增 `ContentControl`，默认属性可直接包含一个 authored 视觉子节点，
    也可声明标量/Binding `Content` 并由内部 `ContentPresenter` 呈现，两种所有权模式严格互斥。
    `ContentPresenter.Content` 同步泛化为 BindingValue；无模板标量生成文本后备，强类型 BindingSource 继续使用
    显式或词法隐式 DataTemplate。XAML 解析、XML v21、设计器捕获、自检、属性栏类型过滤、模板依赖扫描、
    剪贴板提升与事务热重载共用相同验证和物化链；直接内容冲突在进入运行时所有权树前即被拒绝。
60. 已完成内置交互控件接入默认内容的第一批：`Button` 改为派生 `ContentControl`，支持文本/Binding/
    DataTemplate Content 或单个 authored 视觉根，规范 XAML、设计器投放、属性候选、资源依赖、剪贴板、热重载和
    辅助静态生成共用既有内容链。旧 `Text` 构造与属性继续作为兼容入口；Button 明确保留单一点击状态机，视觉内容
    不接管鼠标消息；其逻辑内容槽成为后续 HeaderedContentControl 的共用基础。
61. 已完成 `HeaderedContentControl` 双槽位基础设施：框架内部 Presenter/标题视觉与 authored Content 在同一物理
    子树中拥有明确身份，公开子集合不能破坏内部所有权；Header 和 Content 分别支持字面量、Binding、显式/词法隐式
    DataTemplate 或单视觉根。`GroupBox`、`Expander` 已迁移为 Header + 单 Content，布局、绘制、命中、折叠裁剪和旧
    `Text` 后备均在该契约上运行。规范 XAML 支持 Header/Content 属性及属性元素，设计器属性栏、资源重命名、词法模板
    扫描、剪贴板依赖提升、热重载与辅助静态生成同步理解两个槽位；Demo 的多子项 GroupBox 已用单一 Panel 内容根表达。
62. 已完成 `ControlTemplate` 第一批：`ContentControl`、`Button`、`GroupBox`、`Expander` 支持有键显式模板与按
    TargetType 精确匹配的词法隐式模板；模板根是基础设施视觉，不占 authored Content，应用后跳过原生 chrome 但保留
    输入、状态和内容行为。TemplateBinding 统一依赖 Control 的属性元数据观察边界，模板复用 VisualState/StateTrigger/
    Setter/Storyboard/EventTrigger 与命名部件。全局、合并及控件局部资源、规范 XAML、XML v22、Designer 预览、
    结构热重载、递归/TargetType 校验和剪贴板局部提升已闭环；声明组件 QName 与统一模板优先级由第二批继续完成。
63. 已完成 `ControlTemplate` 类型化第二批：TargetType 扩展到声明组件 QName，直接 Template、Style.Template、
    词法隐式模板和 ComponentDefinition 默认模板共享统一优先级；设计器属性栏、画布捕获/恢复、资源重命名重物化、
    XML v23、剪贴板和热重载按实际 XAML 类型身份闭环。条件 Setter 仍拒绝 Template，避免运行时标量样式层破坏视觉拓扑。
64. 已完成 `ControlTemplate` 内容插槽第一批：模板内 `ContentPresenter.ContentSource` 支持 Content/Header，自动派生
    ContentTemplate 与 DisplayMemberPath 别名；authored 视觉内容转移到 Presenter 的物理子树，同时保留模板宿主作为
    Designer 逻辑父级。数据内容、属性实时更新、规范 XAML、XML v24/v23 兼容读取、剪贴板、XAML 即时预览、选择保持、
    Undo/Redo 和结构热重载共用同一物化链；重复来源、错误 TargetType 与显式别名冲突在提交前拒绝。
65. 已完成 `ItemsControl` 模板宿主第一批：`ItemsControl` / `ListBox` 可应用 ControlTemplate，`ItemsPresenter` 取得由
    `ItemsPanelTemplate` 创建的唯一 ItemsHost，最近的模板内 ScrollView 成为滚动与虚拟化宿主；省略 Presenter 时保留
    数据和容器但不渲染 ItemsHost。选择、键盘导航和容器生成仍由 C++ 行为层负责，模板根、Presenter 与 ItemsHost 均为
    非 authored 基础设施。规范 XAML、XML v25/v24 兼容读取、剪贴板、即时预览、选择保持、Undo/Redo 和结构热重载已
    共用同一物化链；模板外使用、错误 TargetType、重复 Presenter 和 authored 子项均在提交前拒绝。
66. 已完成 `ListBoxItem` 容器模板第一批：生成容器迁移为 `ContentControl`，记录 DataTemplate 通过 Content 槽呈现；
    `ItemContainerStyle.Template` 与词法隐式 `ControlTemplate TargetType="ListBoxItem"` 共享统一优先级和可重复运行时
    工厂。只读 `IsSelected`、`IsMouseOver`、`IsKeyboardFocusWithin` 可驱动 Trigger/VisualState，条件读取与 Setter
    写入校验已分离。规范 XAML、XML v26/v25 兼容读取、Designer 即时预览、选择保持、Undo/Redo、剪贴板显式/隐式
    模板闭包和结构热重载已贯通；直接 authored ListBoxItem、错误 ContentSource 与不兼容 Style.Template 在提交前拒绝。
67. 已完成通用项容器与 `ComboBoxItem` 第一批：`ListBoxItem` 和 `ComboBoxItem` 共用内容初始化、DataTemplate、
    DisplayMemberPath 及只读交互状态契约；ComboBox 支持 `ItemTemplate`、`ItemContainerStyle`、显式/词法隐式
    `ControlTemplate TargetType="ComboBoxItem"`，弹出项继续复用既有滚动、动画和命中行为。XAML 宿主默认启用真实
    容器，旧 C++ ComboBox 仅在配置模板/样式后启用，以免大数据纯文本下产生不必要的控件树。规范 XAML、XML v27/v26
    兼容读取、Designer 预览/选择保持/Undo/Redo、剪贴板依赖提升和 RuntimeDocument 结构热重载已闭环；错误 TargetType、
    不兼容 Style.Template 与把 ComboBoxItem 当普通文档节点使用会在提交前拒绝。
68. 已完成静态 `TreeViewItem` 分层容器第一批：`TreeView.Items` 的节点继续保留轻量 `TreeNode` 数据身份，同时按需
    生成真实 `HeaderedContentControl` 容器；Header 使用正式内容插槽，`IsExpanded` 可写并同步回节点，`HasItems`、
    `Level`、`IsSelected`、`IsMouseOver`、`IsKeyboardFocusWithin` 为只读状态，可驱动 Trigger/VisualState。
    `ItemContainerStyle`、显式/词法隐式 `ControlTemplate TargetType="TreeViewItem"`、规范 XAML、XML v28/v27
    兼容读取、Designer 预览/选择保持/Undo/Redo、剪贴板模板闭包和 RuntimeDocument 结构热重载已贯通；普通 authored
    TreeViewItem、错误 TargetType 与不兼容 Style.Template 会在提交前拒绝。未配置模板/样式的旧 C++ TreeView 仍走
    轻量绘制兼容路径；下一批接入数据项、HierarchicalDataTemplate.ItemsSource 与层次集合变更观察。
69. 已完成数据驱动 `TreeView` 与 `HierarchicalDataTemplate` 第一批：`TreeView.ItemsSource` 接受强类型
    `BindingList` 资源或 Binding，显式/词法隐式 `ItemTemplate` 负责标题视觉；`HierarchicalDataTemplate.ItemsSource`
    以当前数据项为源解析子 `BindingList`，并按每层 ItemType 继续选择隐式模板。生成的 `TreeViewItem` 以数据项作为
    Header 与 DataContext，保留 Header 插槽、容器模板和全部交互状态。根/子列表变化及子列表属性替换会重建层次，
    展开与选择按数据身份恢复；循环数据、错误类型或模板生成失败不会替换已提交树。发生在属性通知内部的重建会先断开
    旧层次观察，并把旧标题绑定延迟到通知返回后释放，避免事件快照命中悬空目标。规范 XAML、XML v29/v28 兼容读取、
    Designer 预览/选择保持/Undo/Redo、局部隐式模板、剪贴板递归模板闭包及 RuntimeDocument 热重载均已贯通；
    `TreeView.Items` 与 `ItemsSource` 同时声明、非 BindingList 子路径、写入模式和数据环会在提交边界事务拒绝。
70. 已完成数据 `TreeView` 增量与虚拟化第二批：根列表及已物化子列表直接消费 Add/Remove/Replace/Move/Swap，
    Reset 按 BindingSource 对象身份协调复用；未受影响 TreeNode、选择、悬停和已实现 TreeViewItem 不再因相邻记录变化
    被替换。折叠层次只保持强类型子源、路径观察与 HasItems，首次展开或 UIA 子项枚举时事务创建下一层，后代继续延迟；
    子源替换和数据环失败保留已提交子树。生成容器按展开后的线性节点序列限制在视口及前后各一行，并在滚动、动画、
    尺寸和集合变化时增量进入/退出；插入或移动按首行节点身份重定位 ScrollIndex，避免数值索引漂移造成视觉跳动。
    静态 TreeViewItem 仍立即物化，未启用生成容器的旧 C++ 轻量绘制路径不变；本批不增加 XAML 字段，继续使用 XML v29。
71. 已完成 `TreeView` 可见投影与容器回收第三批：展开稳定时以缓存的 `(TreeNode, Level)` 扁平序列统一驱动容器窗口、
    O(1) 行命中和仅视口行绘制，不再由容器刷新、状态同步和渲染各自递归整棵已物化树；动画期间保留按展开进度嵌套裁剪，
    结束后自动回归扁平快路径。子集合变化按父节点在旧投影中的层级边界，仅重建该父节点的可见后代片段；结构 UIA
    索引与可见投影解耦，前者只在辅助功能查询时延迟建立。滚出窗口的 TreeViewItem 清空 Header、HeaderTemplate、
    DataContext 与瞬态交互状态后进入按页面大小限制的回收池，再实现时保留 ControlTemplate chrome 并重新绑定目标节点；
    Style/Template 整体替换仍清空池并事务重建。本批是纯运行时优化，不增加 XAML/XML 字段，快照保持 v29。
72. 已完成 `TreeView` WPF 选择与导航第四批：公开只读 `SelectedItem` / `SelectedValue` 与可写
    `SelectedValuePath` 元数据；数据节点投影 BindingSource 身份，路径值保持类型并观察嵌套字段变化，静态兼容节点投影
    TreeNode。`SelectedItemChanged` 成为设计器默认事件，旧 `SelectionChanged` 保留同步通知。鼠标、程序化
    `SelectNode`、UIA 和键盘统一经过一个选择提交点，集中同步生成容器状态、只读属性观察、双事件与辅助功能通知；
    数据树事务重建恢复同一记录时只重连观察，不伪造用户选择变化。`Up/Down/Home/End/PageUp/PageDown` 按缓存的可见
    投影移动，`Right` 展开或进入首个子项，`Left` 折叠或返回父项；所有移动共用层次 BringIntoView，必要时展开祖先并
    调整 ScrollIndex。解析器同步接受 WPF 默认内容式的直接嵌套 `TreeViewItem`，但拒绝与显式
    `TreeViewItem.Items` 混用；Designer 回捕仅把无 ItemsSource 的节点序列化为静态 Items，避免把数据生成节点写成第二
    项来源。选择属性由运行时元数据表达，不新增文档字段，XML 继续保持 v29。

每一步都必须覆盖 XAML 解析与诊断、规范 XAML、当前版本快照、设计器捕获/恢复、动态运行时、热重载、剪贴板和
事务失败回滚；不能只在设计器或只在运行时增加旁路。
