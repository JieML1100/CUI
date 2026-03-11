# ProcessMessage 默认行为约定

这份约定同时适用于主线 CUI 和 Legacy。

## 1. 总原则

- 新控件优先复用 Control 的默认消息分发与 hook。
- 只有当控件存在明显的自定义路由、输入状态机或复合交互时，才保留自定义 ProcessMessage。
- 如果一个控件只是“接收命中 -> 更新少量状态 -> 触发事件 -> 请求重绘”，不要再重写整段 ProcessMessage。

## 2. 适合走默认 hook 的控件

满足以下大部分条件时，应该优先走默认 hook：

- 命中区域就是控件自身，不需要在多个内部区域之间做消息分流。
- 鼠标主交互是悬停、按下、抬起、单击、双击中的一部分。
- 只需要在按下或抬起时修改少量状态，比如 Checked、Visited、按压态。
- 不需要自己管理鼠标捕获、拖拽状态机或复杂的连续输入过程。
- 不需要把消息继续转发给子控件或弹出层。
- 不需要自己定义坐标系换算、视口偏移或滚动补偿。

当前已经迁入默认 hook 的典型控件：

- Button
- LinkLabel
- CheckBox
- RadioBox
- Switch

## 3. 适合继续保留自定义 ProcessMessage 的控件

出现以下任一特征时，应继续保留自定义 ProcessMessage：

- 需要把消息分发给子控件、页签内容区、前景弹层或其他内部子区域。
- 需要维护独立的输入状态机，比如文本选择、拖拽、缩放、范围选择、滚动、播放控制。
- 需要处理输入法、光标、选区、键盘导航或以键盘为主的交互。
- 需要处理滚动容器、视口偏移、命中换算、虚拟区域或多段坐标空间。
- 需要和 Form、原生窗口、系统组件或外部宿主做特殊协作。
- 点击语义不是基类默认模型，或者不同消息之间存在严格时序依赖。

当前应保留自定义 ProcessMessage 的典型类型：

- TextBox、RichTextBox、PasswordBox
- ComboBox、DateTimePicker
- Panel、ScrollView、TabControl、TabPage
- GridView、TreeView
- MediaPlayer、WebBrowser
- Form 及系统集成类控件

## 4. hook 的使用边界

- BeforeDefaultMouseDown
  适合放“按下瞬间”的轻量状态更新，不适合写复杂路由。

- BeforeDefaultMouseUp
  适合放“抬起时提交状态”的逻辑，比如勾选切换、开关切换。

- BeforeDefaultClick
  适合放“确认是一次 click 后”才应该发生的副作用，比如 LinkLabel 的 Visited。

- BeforeDefaultMouseDoubleClick
  只处理双击附带的局部差异，不承担完整双击流程。

- DefaultSelectOnLeftButtonDown / DefaultSelectOnLeftButtonDoubleClick
  只用于声明是否沿用基类默认选中行为，不要在这里塞业务逻辑。

- DefaultRaiseClickOnLeftButtonUp / DefaultRaiseMouseDoubleClick
  只用于声明是否由基类统一抛事件。

- DefaultPostRenderOnMouseDown / DefaultPostRenderOnMouseUp / DefaultPostRenderOnMouseDoubleClick
  只用于声明这些默认输入节点后是否需要立即刷新。

## 5. 决策顺序

新增或重构控件时，按下面顺序判断：

1. 先默认使用基类 ProcessMessage。
2. 仅通过 hook 补足状态变更、事件触发和重绘策略。
3. 只有在 hook 无法表达消息路由或输入状态机时，才下沉为自定义 ProcessMessage。

## 6. 一条硬性约束

- 如果控件重写了 ProcessMessage，就应该是因为它真的需要接管“消息路由”或“交互状态机”；如果只是为了改一个 Checked、Visited 或点击后的重绘策略，应回到默认 hook。