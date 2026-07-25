# CUI 构建与验证门禁

本文只记录当前有效门禁，不保存已删除 API/旧 Schema 的历史验证步骤。架构批次历史见
`CUI_WPF_ARCHITECTURE_DIRECTION.md`。

## 环境

- Visual Studio 2022 / MSVC v143
- Windows SDK
- 默认验证配置：`Debug|x64`
- 工作目录：仓库根目录

## 全解决方案构建

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  CUI.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 `
  /p:LinkIncremental=false /verbosity:minimal
```

构建必须包含 CUI、CuiRuntime、CuiDesigner、CUITest、CuiCodeGen、CuiRuntimeSample、
CuiStaticGeneratedSample 和 CUICoreTests。CodeGen 输出语义变化后，版本化 stamp 必须触发 `.g.*` 重生。

## 运行门禁

```powershell
.\x64\Debug\Designer.exe --self-test
.\x64\Debug\CUITest.exe --validate-xaml
.\x64\Debug\CUITest.exe --smoke-xaml
.\x64\Debug\CUITest.exe --render-smoke
.\x64\Debug\CuiRuntimeSample.exe
.\x64\Debug\CuiStaticGeneratedSample.exe
.\x64\Debug\CuiCodeGen.exe --version
.\x64\Debug\CUICoreTests.exe
```

完整核心回归包含 Debug 下的大规模虚拟化数据集，超时应至少给 10 分钟。若只执行定向测试，使用
`CUI_TEST_FILTER`，结束后必须移除环境变量。

## 当前 WPF 语义检查

### XAML 与类型

- 旧 `<Form>`、`<Label>`、`<Panel>`、`<GridPanel>`、`<ScrollView>`、`<PictureBox>`、
  `<RadioBox>` 等 QName 被拒绝。
- Window、控件、Style TargetType 在 Parser → canonical XAML → Parser 往返后保留同一 QName。
- 动态 Materializer 和静态 CodeGen 均附加相同 `DeclarativeTypeDescriptor`。
- 静态 selector 输出 namespace/local-name；`UIClass` 只作为 behavior host。
- 类型、成员、资源 key、binding/DataContext path、事件、VisualState/Storyboard 与 Designer 扩展 ID
  使用精确大小写身份。

### 默认 Theme 与模板

- `Themes/Generic.xaml` 可作为独立 ResourceDictionary 解析，并在构建时嵌入；C++ 不注册其类型、属性或事件。
- 动态 Materializer、Designer 预览和静态 CodeGen 均通过 `XamlDocumentCompiler` 消费同一展开结果。
- Theme Style 与作者 Style 不合并；Local Template、作者 Style.Template、Theme Style.Template 保持规范优先级。
- 默认 Button 具有 `PART_Chrome`、`PART_ContentPresenter` 和 CommonStates；Normal、PointerOver、Pressed、
  Disabled 及返回 Normal 的状态变化通过 smoke/render smoke。
- 静态模板内部节点不进入公开成员、ControlId 或动态 reference；其属性来源为 Template，框架默认值来源为 Theme。
- generated base 构造函数为空，XAML 仅由用户 code-behind 构造函数体中的 `InitializeComponent()` 展开。

### 属性与 Brush

- 不存在 `BackColor`、`ForeColor`、`BorderColor`、`FocusedColor`、`FocusBorderColor` 或通用
  `AccessKey` 元数据。
- Brush 颜色简写解析后只有结构化 ObjectValue，规范往返不产生标量/对象双表示。
- 普通 setter 进入 Local value source；Theme/high-contrast 不覆写 Local。
- `BrushKind::None` 不绘制，renderer fallback 与作者 Brush 分层。
- 声明字符串候选值精确匹配；bool 只接受 `true/false`，拒绝旧数字、yes/no、on/off 和空串。
- Canvas/Grid/DockPanel attached property 只经真实 owner API 投影，`Control` 不公开扁平属性。
- Style/Resource 降级存储只经 `StyleAccess`，不是公共 Control API。
- 公开依赖属性 wrapper 写入产生 Local source；projected 类型没有有效 metadata 时不得写入共享 C++ backing。
- `BorderThickness` 是单一 `Thickness` 有效值；Border 不保存或注册第二份同名状态。
- Panel 的 `Background=None` 真正不绘制；构造器不使用透明 Theme Brush 伪装 unset。
- Style Setter 先建立 base value，Trigger/Storyboard 再解析对象路径并建立 animation clocks。

### 结构层级与布局

- Panel、Decorator、ContentPresenter、ItemsPresenter、Popup、TextBlock behavior host 与 WebBrowser 不获得
  Control chrome；Border 只通过 Decorator 的 single child 与自身 Border/Padding 实现布局。
- ContentPresenter/ItemsPresenter 不隐式消费 Padding，Popup 不拥有 Background/Padding。
- ControlTemplate 根取得完整 final slot；Padding 只由模板树通过 TemplateBinding 消费一次。
- structural element 上不属于 projected 类型的 Padding/Border/Text 写入被拒绝，不形成隐藏状态。
- `AffectsParentArrange` 会保持 Measure 缓存、只使父 Panel 的 child-arrangement policy 失效；
  Canvas attached offset 改变后实际位置更新。

### 输入、焦点和命令

- 输入只经过一次 InputManager staging，Preview/behavior/Bubble 共享 args 与 `Handled`。
- FocusManager 唯一拥有 keyboard/logical focus；selection 不冒充 focus。
- TextCompositionManager 唯一处理 `WM_CHAR`/IME，提交文本只写入一次。
- TextBox、RichTextBox、PasswordBox 与 NumericUpDown 声明自身拥有四个方向键；Window 不得先把方向键解释为
  焦点导航。未被编辑 behavior 消费的 Space 必须保留给 TextCompositionManager，不能因“控件收到 KeyDown”
  就抑制对应字符。
- 鼠标捕获期间由 capture owner 决定游标；复合编辑控件的子按钮按下后，游标语义不得随捕获坐标滑回编辑区而变成
  I-Beam。
- AccessText 使用 `_`/`__`。
- RoutedCommand、CommandBinding、InputBinding 和 authored CommandTarget 共用 Window command domain。

### 自动化

- XAML 使用 `AutomationProperties`，不存在 `AccessibleRole`。
- Window/UIA 只查询 `AutomationPeer`，不按具体控件类型或 `UIClass` 猜 Pattern。
- 真实与虚拟元素共用 `AutomationPattern`；ComboBox scroll metrics 来自真实 ScrollViewer。
- Password 值不进入自动化输出。

### 渲染与 Presentation

- retained scene、dirty region、native composition 分类和设备重建门禁通过。
- active DrawingContext 只存在于打开的 frame surface 内；帧外为空，WM_PAINT 依据 attached host 启动 transaction。
- flip/sequential 交换链的局部帧必须使用 Present1 dirty rect；新建、恢复或 Resize 后的每个 surface
  必须先完整清屏并提交一个不带 dirty rect 的首帧。
- WebBrowser 首次启用 DirectComposition 后，切回普通页面仍必须继续提交帧；任何 surface 的
  EndDraw/Present 失败都要记录 transaction、surface role 与 HRESULT。
- Canvas 几何移动只推进 geometry revision，并覆盖 dirty/recomputed/command-cache 分类。
- Expander 折叠时内容退出 measure/render 后还必须清空旧 arrange slot；未显式固定 Expander 高度时，展开尺寸由
  内容决定、折叠尺寸只由 Header 决定。原生 fallback chrome 在折叠状态不得继续绘制整个展开区域。
- 控件不在内容渲染尾部直绘 focus/validation adorners。
- 非 Solid Brush 的实现覆盖范围由 render smoke/像素门禁持续扩展；未覆盖控件只允许内部 native fallback，
  不能注册第二套公共颜色属性。

## 最新验证结果

本节在每个大批次完成后更新；不得用旧批次通过记录替代当前工作树验证。

- 日期：2026-07-25
- 配置：`Release|x64`（本轮真实 GUI 故障复现配置）。
- 完整构建：解决方案单进程 Rebuild 成功，CUI、CuiRuntime、CuiDesigner、CUITest、CUICoreTests、
  CuiRuntimeSample、CuiCodeGen 与静态生成样例全部重新编译/链接。并行增量链接曾因被外层超时打断而产生
  `LNK1103` 缓存损坏，清理中间产物后的完整 Rebuild 源码零错误。
- CUITest validate/smoke/render smoke：全部返回 0。
- CUICoreTests：326/326。
- 本批专项覆盖：
  - Expander 折叠会把内容旧 arrange slot 归零，fallback 只保留 Header chrome；CUITest 将固定展开高度放到
    Content 上，使 Expander 自身恢复 WPF 风格 Auto 折叠；
  - NumericUpDown 不再用 Theme `Cursor=IBeam` 截断区域化游标解析；保持 `Cursor=Auto` 后，
    `ResolvePointerCursor` 在按钮区以及 spin button 捕获期间返回 Hand，释放回文字区后恢复 I-Beam；
    PointerDown 改变 capture owner 后 Window 立即重算游标；
  - TextBox、RichTextBox、PasswordBox、NumericUpDown 接管四个方向键；Space KeyDown 不再被误报为已处理，
    后续 TextComposition 提交能够插入空格；
  - flip 双缓冲局部 retained redraw 使用 Present1 dirty rect，不再交替显示不同页签/焦点/动画历史；
  - 新建/Resize/恢复后的 composition scene surface 先提交完整首帧，不再以
    `DXGI_ERROR_INVALID_CALL` 中止整棵合成树；
  - 普通页 → WebBrowser 页 → 普通页序列持续刷新，committed frame 前进且 aborted frame 不增加；
  - ComboBox placement target 在一次 pointer transaction 中唯一拥有开合决定；
  - ListBox/ListView、TreeView 和 Tab header fallback 均受各自 viewport/strip 裁剪。
- 可见窗口复核：Release CUITest 中实际点击进入 WebBrowser，WebView2 HTML 正常合成；随后点击“布局容器”
  立即得到完整新页面，关闭确认对话框也能正常弹出并退出。
- 本轮相关文件 `git diff --check` 通过（仅行尾转换提示）。
