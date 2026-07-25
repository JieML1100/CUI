# CUI XAML 声明组件架构

本文描述当前 `ComponentDefinition` 契约。控件类型由 XAML 定义；C++ 不注册组件类型，只能按声明的
`RuntimeTypeId` 为实例挂接 behavior、事件、消息或渲染。

## 1. 身份模型

组件身份是完整 QName：

```text
RuntimeTypeId = namespace URI + local name
```

`test:FeatureCard` 与另一个命名空间下的 `FeatureCard` 是不同类型，即使二者都使用同一个 native Canvas behavior
host。`UIClass` 只用于选择 native host 和框架 class behavior，不是组件身份，也不能作为 Style 的最终匹配依据。

以下路径必须保存同一 QName：

- XAML Parser 与 canonical serializer
- `DesignDocument` / clipboard / Designer preview
- `XamlRuntimeSchema` / Materializer / RuntimeDocument 热重载
- Style `TargetType` 与 class command binding
- 静态 CodeGen 生成的 `DeclarativeTypeDescriptor`

## 2. 声明示例

```xml
<Window xmlns="urn:cui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        xmlns:local="urn:sample">
  <Window.Resources>
    <ComponentDefinition x:Key="local:FeatureCard"
                         BaseType="Canvas"
                         DisplayName="Feature card">
      <ComponentDefinition.Properties>
        <ComponentProperty Name="Caption"
                           Type="String"
                           Default="Feature"
                           Category="Content" />
        <ComponentProperty Name="State"
                           Type="String"
                           Default="Created"
                           ReadOnly="true"
                           Category="Runtime" />
        <ComponentProperty Name="IsActive"
                           Type="Bool"
                           Default="false"
                           Category="State" />
        <ComponentProperty Name="AccentColor"
                           Type="Color"
                           Default="#FF2F6FE4"
                           AffectsRender="true" />
        <ComponentProperty Name="ContentPadding"
                           Type="Thickness"
                           Default="12"
                           AffectsMeasure="true" />
      </ComponentDefinition.Properties>

      <ComponentDefinition.ContentProperties>
        <ComponentContentProperty Name="Content"
                                  Cardinality="Single"
                                  Default="true" />
        <ComponentContentProperty Name="Actions"
                                  Cardinality="Multiple" />
      </ComponentDefinition.ContentProperties>

      <ComponentDefinition.Events>
        <ComponentEvent Name="Invoked"
                        RoutingStrategy="Bubble"
                        Default="true" />
        <ComponentEvent Name="Pulse"
                        RoutingStrategy="Direct" />
      </ComponentDefinition.Events>

      <ComponentDefinition.Template>
        <StackPanel x:Name="PART_Root"
                    Padding="{TemplateBinding ContentPadding}"
                    Background="#FFF7F9FC"
                    BorderBrush="#FFD3DCE8"
                    BorderThickness="1">
          <TextBlock x:Name="PART_Caption"
                     Text="{TemplateBinding Caption}"
                     Foreground="{TemplateBinding AccentColor}" />
          <TextBlock x:Name="PART_State"
                     Text="{TemplateBinding State}" />
          <StackPanel x:Name="PART_Content"
                      ComponentSlot.Presents="Content" />
          <Button x:Name="PART_Invoke"
                  Content="_Invoke"
                  Click="{RaiseEvent Invoked}" />
          <StackPanel x:Name="PART_Actions"
                      ComponentSlot.Presents="Actions" />
        </StackPanel>
      </ComponentDefinition.Template>
    </ComponentDefinition>
  </Window.Resources>

  <local:FeatureCard x:Name="card"
                     Caption="Runtime"
                     Invoked="OnCardInvoked">
    <TextBlock Text="Default content" />
    <Button ComponentSlot.Target="Actions" Content="Refresh" />
  </local:FeatureCard>
</Window>
```

## 3. 属性

- `ComponentProperty` 生成声明类型描述符中的依赖属性定义，不生成 C++ 字段。
- `Type`、默认值、只读性、类别和 `AffectsMeasure/AffectsArrange/AffectsRender/Inherits` 等标志在解析阶段校验。
- 组件成员不得覆盖 native host 已有属性、attached property 或事件。
- 只读属性只能由组件 behavior/框架通过 read-only key 更新；XAML Setter、Binding 回写和普通作者写入必须拒绝。
- `TemplateBinding` 读取宿主的有效值，不复制 Local 字段。
- Brush、Geometry、Transform 等结构化值在文档模型中只有一个规范对象表示。

## 4. 事件与 C++ behavior

`ComponentEvent` 只声明事件身份、路由策略、展示信息和默认事件。XAML 中保存处理函数名；运行时 registry 把该名称
挂到强类型 C++ handler。C++ 不能添加文档未声明的公共事件。

模板中的 `{RaiseEvent Invoked}` 使用同一 routed-event 管线：稳定 `OriginalSource`、冻结 route、Preview/Bubble
顺序和共享 `Handled` 语义。组件 behavior 可在事件前后执行本机逻辑，但不得直接枚举另一套回调列表。

class handler/command binding 若针对组件类型，必须以完整 `RuntimeTypeId` 精确匹配；只有框架内建 behavior 才允许
按 `UIClass` 继承闭包匹配。

## 5. 内容槽

- 每个 `ComponentContentProperty` 明确 `Single` 或 `Multiple` cardinality。
- 至多一个内容属性可为默认槽。
- 模板中每个声明内容属性必须恰好有一个 `ComponentSlot.Presents` presenter。
- 作者子项通过 `ComponentSlot.Target` 选择非默认槽；未指定时进入默认槽。
- 单值槽多根、未知槽、重复 presenter 或缺失 presenter 都在提交前拒绝。
- presenter 是框架基础设施视觉，不得被当作作者子节点持久化。

内容实例拥有明确的 VisualParent、LogicalParent、InheritanceParent、RoutedParent 和 TemplatedParent；不能用一个
通用 Parent 猜测资源继承、路由或模板身份。

## 6. 模板和名称作用域

- 每次模板实例化都创建独立 namescope。
- `x:Reference`、ElementName、InputBinding/CommandTarget 只能解析当前作用域内名称。
- 组件实例不能引用另一个模板实例中的同名部件，也不能越过模板边界引用外层文档名称。
- `PART_*` 是 XAML 声明的部件名称。C++ behavior 可以在 attach 后按名称取得部件并保存弱引用，但不能创建或
  注册这些部件。
- Template、Style、VisualState、Trigger、Storyboard 和 `{RaiseEvent}` 共用资源与类型校验管线。

## 7. Style 与资源

```xml
<Style TargetType="local:FeatureCard">
  <Setter Property="AccentColor" Value="#FF36A269" />
  <Style.Triggers>
    <Trigger Property="IsActive" Value="true">
      <Setter Property="Background" Value="#2036A269" />
    </Trigger>
  </Style.Triggers>
</Style>
```

Style selector 保存 `local:FeatureCard` 的 QName，同时可保存 native base type 作为快速排除条件；最终匹配必须验证
声明身份。一个 Canvas host 上的不同组件不能互相命中隐式 Style、class command binding 或模板。

资源查找遵循当前元素、继承上下文、文档/Application/Theme 的词法与层级规则。StaticResource 在提交前解析；
DynamicResource 保留表达式并在资源 revision 改变时重新求值。

## 8. 动态 Runtime 与静态 CodeGen

动态路径：

```text
XAML → Parser → DesignDocument → XamlObjectMaterializer
     → native behavior host + DeclarativeTypeDescriptor
     → properties/resources/templates/events/commands
```

静态路径只是同一文档的辅助 lowering：

- 先默认构造 native host 并清理构造器产生的伪 Local 值。
- 为 Window 和每个控件附加相同 `DeclarativeTypeDescriptor`。
- 先创建完整 namescope，再连接 authored references、bindings、events 和 commands。
- Style selector 同时输出完整 QName；不能只输出 `UIClass`。
- 生成输出语义变化时提升 CodeGen contract，使旧 stamp 强制失效。

静态生成文件不得成为第二作者源，也不得手工编辑 `.g.*`。

## 9. 热重载

RuntimeDocument 以稳定 DesignId 和 QName 判断原位更新、子树重组或完整替换。一次 reload 必须原子提交：

- 新文档完整解析、Schema/资源/模板/事件验证成功后才触碰活动树。
- QName 改变、namescope 失效或宿主拒绝时回滚原 Content、DataContext、事件连接和引用。
- 原位复用不能保留已从 XAML 删除的 Local 值、事件或 behavior 连接。
- C++ behavior attach/detach 具有明确生命周期，不能让旧实例回调命中新文档。

## 10. 禁止回归

- `d:CppType`、`d:Header`、`d:BaseType`、`d:Constructor` 等 C++ 自定义类型元数据。
- 以 native 类名或 `UIClass` 代替 XAML QName。
- C++ 运行时注册属性/事件来补齐组件声明。
- 组件属性与 native 字段双写，或静态 CodeGen 生成另一套默认值。
- 跨 namescope 的 ElementName/x:Reference/CommandTarget。
- 旧 QName 别名、旧 snapshot 兼容读取或失败后回走 Legacy materializer。

真实综合示例与回归入口是 `CUITest/DemoWindow.cui.xaml` 中的 `test:FeatureCard`。
