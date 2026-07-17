# Undo 命令入口评估

这份文档记录当前设计器里真正会改变“设计状态”的入口，以及它们后续应该收敛成哪些命令。

## 0. 当前命令安全不变量

- `IDesignerCommand`、`CommandManager`、协调器和 Canvas 的 Execute/Undo/Redo 必须贯穿返回
  `DesignerDocumentTransactionResult`；空历史为 `Unchanged`，失败或异常时保留错误、`DocumentRestored` 和原栈条目。
- 增删快照必须在修改前成功构建设计文档；前置捕获失败时不得执行修改。
- 后置文档捕获或命令入栈失败时，协调器恢复前置文档、完整选择集和主选控件。
- 键盘移动、拖拽、缩放和分隔条预览只有在结果型交互事务成功 Begin 后才允许首次修改；MouseUp 单次提交，Escape/失焦/捕获丢失回滚。
- 八类模态结构编辑器都只在对话框内建立编辑模型；六类值集合提交 `ControlStructureCommand`，TabControl/ToolBar 提交 `ControlOwnedCollectionCommand`。取消/无变化不入栈，异常或提交失败保持或恢复原状态。
- 文档事务以 `DesignerDocumentTransactionResult` 返回明确状态和恢复结果；PropertyGrid 不得复制完整文档快照，普通控件属性只能构造统一的逐属性差量命令。
- Add/Delete/Undo/Redo 必须通过 `OnCommandCompleted` 发布 operation、历史 label、消息和完整结果；空删除、拒绝添加和失败恢复不得无条件显示成功。
- 严格文档事务处于 Begin 与 Commit/Cancel/Rollback 之间时，独立 Execute/Undo/Redo 必须为 `Rejected`，不得把旧历史穿插进预览文档。
- Dirty 由不可复用的文档状态 ID 与保存点比较得出，不得按栈深度推断；保存后 Undo/Redo 可离开/回到保存点，Undo 后的新分支不得复用旧 redo 分支 ID。
- New/Open 只有在完整文档成功应用后才重置为干净历史；失败须恢复原文档、选择和历史状态。Save 只有在同目录临时文件刷盘并原子替换成功后才移动保存点。
- 自动恢复快照不能调用正式 Save 或移动保存点；恢复内容通过 `RestoreRecoveredDocument` 建立空历史但 Dirty 的新基线。恢复失败或活动事务拒绝时保留当前文档和历史。
- 连续 `UpdateProperty:<name>` 与 `NudgeSelection` 分别由属性差量和放置差量在同选择、同中间状态和 1 秒窗口内合并；保存点、redo 分支、标签、选择或 expected 起点变化必须切断/拒绝。合并更新 after 状态和状态 ID，不改变最初 before。Undo+Redo 历史同时受 64MiB 估算预算约束，淘汰最远项但保留至少一个最近命令。
- `Designer.exe --self-test` 使用真实画布、属性面板和命令栈验证属性、多选、Reset、结构集合事务状态、取消泄漏恢复、拒绝/异常回滚、Add/Delete 与 Undo/Redo 的结果/标签/历史保持，以及保存点分支、失败打开恢复、锁文件保存失败和 New/Open 历史重置；不得用测试专用 setter 代替。

## 1. 当前高价值入口

### 1.1 画布增删

- AddControlToCanvas
  位置：DesignerCanvas.cpp
  当前事务：`ExecuteDocumentEditTransaction("AddControl")`
  说明：先验证控件类型与落点，再捕获前置文档；创建失败回滚，成功发布 `OnCommandCompleted`

- DeleteSelectedControl / DeleteControlRecursive
  位置：DesignerCanvas.cpp
  当前事务：`ExecuteDocumentEditTransaction("DeleteSelection")`
  说明：空选择返回 `Unchanged`；有效删除以完整子树、父容器、顺序和选择集快照进入一条历史记录

### 1.2 拖拽与键盘移动

- BeginDragFromCurrentSelection + ApplyMoveDeltaToSelection
  位置：DesignerCanvas.cpp
  当前命令：`BeginPlacementInteraction("MoveSelection")` 捕获起点，MouseUp 提交一条
  `ControlPlacementCommand`
  差量内容：Location、Margin、显式尺寸、对齐、Anchor、Grid/Dock 字段、父级定位器、同级索引和选择

- TryReparentSelectedAfterDrag
  位置：DesignerCanvas.cpp
  当前命令：并入同一条 `ControlPlacementCommand`
  说明：支持 Root、普通控件、TabPage、Split panel1/panel2 的重建后解析，以及 Stack/Wrap 同级重排；
  无法标识的自定义父级安全回退完整文档事务

- WM_KEYDOWN 中的方向键微调
  位置：DesignerCanvas.cpp
  当前命令：`ControlPlacementCommand`，复用完整 placement/tree 状态但父级与顺序保持不变
  说明：同选择、文档连续且相邻提交不超过 1 秒的方向键微调会合并为一条 `NudgeSelection` 命令；每次按键仍发布一次 `MoveSelection` 结果事件，保存点和 redo 分支会切断合并。

### 1.3 缩放

- WM_LBUTTONDOWN / WM_MOUSEMOVE / WM_LBUTTONUP 中的 resize 流程
  位置：DesignerCanvas.cpp
  当前命令：`BeginPlacementInteraction("ResizeSelection")`，MouseUp 单次提交
  差量内容：起止矩形映射后的完整 placement/tree 状态；Undo/Redo 不重建控件实例

- SplitContainer splitter 预览
  位置：DesignerCanvas.cpp
  当前命令：`BeginControlPropertyInteraction("UpdateProperty:SplitterDistance")` 捕获单目标属性状态，MouseUp 提交
  禁止跨手势合并的 `ControlPropertyCommand`
  说明：只允许通过属性元数据写入；setter 失败中止并回滚，不得裸调 `SetSplitterDistance`；Undo/Redo 不重建实例

### 1.4 属性编辑

- UpdatePropertyFromTextBox
  位置：PropertyGrid.cpp
  当前命令：普通控件属性使用 `ControlPropertyCommand`；Form/控件事件使用 `EventHandlerCommand`；其余窗体属性和不支持差量的入口回退完整文档事务
  说明：Text、尺寸、位置、枚举、颜色、复合属性字符串和事件处理函数名都经过这里；事件名校验与同名签名冲突在提交前完成，Legacy 恢复基值，Metadata 恢复 Local/跟踪条目

- UpdatePropertyFromBool
  位置：PropertyGrid.cpp
  当前命令：普通布尔控件属性使用同一属性差量
  说明：事件已迁移到可编辑函数名，不再经过 Boolean 路径；不能把事件映射误当成运行时元数据属性

- UpdatePropertyFromFloat
  位置：PropertyGrid.cpp
  当前命令：普通浮点使用一次性属性差量；ProgressBar/ProgressRing 百分比捕获一次 before 并在 MouseUp 提交一个差量
  说明：预览或提交失败恢复拖动前属性状态与完整选择，不建立整份文档快照

- UpdateAnchorFromChecks
  位置：PropertyGrid.cpp
  当前命令：`ControlPropertyCommand` 保存 Anchor 的包装器属性状态
  说明：Anchor setter 仍通过画布保持视觉边界，Undo/Redo 走同一目录 setter 并验证精确终点

- DataContextSchema / StyleSheet / DataBindings
  位置：PropertyGrid.cpp
  当前事务：`ExecutePropertyCommand` 转发到同一结果型文档事务
  说明：业务 setter 或预览刷新返回 false 时事务回滚，不再先修改后忽略失败

### 1.5 模态编辑器

- ComboBoxItemsEditorDialog
- GridViewColumnsEditorDialog
- TabControlPagesEditorDialog
- ToolBarButtonsEditorDialog
- TreeViewNodesEditorDialog
- GridPanelDefinitionsEditorDialog
- MenuItemsEditorDialog
- StatusBarPartsEditorDialog

当前命令：ComboBox/GridView/TreeView/GridPanel/Menu/StatusBar 使用单控件强类型 `ControlStructureCommand`；TabControl 页面和 ToolBar 按钮使用转移直接子树所有权及 DesignerControl 包装器的 `ControlOwnedCollectionCommand`（标签均为 `EditStructure:<kind>`）。

说明：对话框只返回带原实例指针的编辑模型，不直接修改目标树。拥有型命令在活跃状态由运行时父树唯一拥有子控件，在缺席状态由命令以 `unique_ptr` 持有；Undo/Redo 同时恢复包装器扁平位置、稳定 ID、选择、ToolBar 尺寸覆盖和 TabControl `SelectedIndex` 的 Local/Binding/metadata 状态。取消或关闭窗口不产生记录。

## 2. 推荐的提交时机

### 2.1 不要在鼠标移动过程中不断压栈

- 拖拽与缩放应该只在 WM_LBUTTONUP 时提交命令
- WM_MOUSEMOVE 只更新临时运行时反馈

### 2.2 文本编辑在“确认”时提交

- TextBox 的失焦或回车提交，应该只生成一条命令
- 同一个属性的连续字符输入不应一字一条命令

### 2.3 集合编辑在对话框关闭时提交

- 模态编辑器关闭且用户确认后，再生成一条 BatchCommand

## 3. 下一步最小落地建议

### 3.1 后续按收益替换专用命令

普通控件属性、SplitterDistance 连续预览、键盘微调、鼠标 Move/Resize/Reparent/容器重排以及 Add/Delete 已经使用安全差量。
Add/Delete 的 `ControlSubtreeCommand` 在缺席期间拥有分离根，保存规范化子树、可重建父级定位器、同级顺序、ToolBar 覆盖和选择，并对 expected 起点做精确验证。
ComboBox Items、TreeView 节点、递归 Menu Items、GridView 列、GridPanel 行列定义和 StatusBar 分段已经使用 `ControlStructureCommand`：
历史只保存单控件强类型集合、目标身份与选择，原位 Undo/Redo 不重建控件。单事件编辑与批量处理函数重命名也已
使用稳定 ID + expected 映射的 `EventHandlerCommand`；Schema/样式/Binding 仍保留统一完整文档事务兜底。
显式选择源码迁移时，同一命令额外保存紧凑的类/路径/签名/名称元数据；Execute/Undo/Redo 重新预检用户源码、
迁移唯一兼容定义并重新生成五文件，失败时同时恢复事件映射和文件快照，历史项保持可重试。
TabControl/ToolBar 已使用 `ControlOwnedCollectionCommand`，不会把拥有型子树错误地压成仅含文本的值集合，也不再保留完整文档。

Canvas 手势完成结果由 `OnInteractionTransactionCompleted` 发布，并保存在最后结果查询接口中；捕获丢失、
CancelMode、Escape、窗口失焦/停用都必须经 `CancelActivePointerInteraction(...)`。新增手势不得恢复旧的
Boolean snapshot wrapper，也不得在 MouseUp 后无条件清空快照而忽略提交结果。

离散命令结果由 `OnCommandCompleted` 发布，并保存在最后命令结果查询接口中。Undo/Redo 发布被操作的历史
label；空历史仍发布 `Unchanged`，恢复失败发布原始错误和真实 `DocumentRestored`。新增离散入口不得只判断
`bool`、丢弃结果或在事件返回后覆盖 Designer 状态文本。

### 3.2 继续减少剩余完整快照

- Schema、样式、Binding 与其余窗体级属性编辑可继续按收益拆成文档局部差量。

任何替换仍须先实现 expected 起点验证、重建后解析、部分失败回滚和历史内存估算，再移除完整事务兜底。
