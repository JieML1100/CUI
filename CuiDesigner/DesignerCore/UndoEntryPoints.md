# Undo 命令入口评估

这份文档记录当前设计器里真正会改变“设计状态”的入口，以及它们后续应该收敛成哪些命令。

## 1. 当前高价值入口

### 1.1 画布增删

- AddControlToCanvas
  位置：DesignerCanvas.cpp
  目标命令：AddNodeCommand
  需要快照：新增节点的类型、父容器、插入顺序、初始属性、初始命名

- DeleteSelectedControl / DeleteControlRecursive
  位置：DesignerCanvas.cpp
  目标命令：DeleteSelectionCommand
  需要快照：被删节点完整子树、原父容器、原顺序、原选择集

### 1.2 拖拽与键盘移动

- BeginDragFromCurrentSelection + ApplyMoveDeltaToSelection
  位置：DesignerCanvas.cpp
  目标命令：MoveSelectionCommand
  需要快照：_dragStartItems 里的起始矩形、Location、Margin、Parent

- TryReparentSelectedAfterDrag
  位置：DesignerCanvas.cpp
  目标命令：ReparentSelectionCommand，或并入 MoveSelectionCommand 的提交阶段
  需要快照：旧父容器、新父容器、重排前后顺序、布局容器特有属性

- WM_KEYDOWN 中的方向键微调
  位置：DesignerCanvas.cpp
  目标命令：MoveSelectionCommand
  说明：当前直接调用 BeginDragFromCurrentSelection + ApplyMoveDeltaToSelection。后续应改成“一次按键一次命令”，或在连续按键时做合并。

### 1.3 缩放

- WM_LBUTTONDOWN / WM_MOUSEMOVE / WM_LBUTTONUP 中的 resize 流程
  位置：DesignerCanvas.cpp
  目标命令：ResizeSelectionCommand
  需要快照：_resizeStartRect、最终矩形、受布局约束时的 Margin/Anchor 变化

### 1.4 属性编辑

- UpdatePropertyFromTextBox
  位置：PropertyGrid.cpp
  目标命令：UpdatePropertyCommand
  说明：Text、尺寸、位置、枚举、颜色、复合属性字符串都经过这里

- UpdatePropertyFromBool
  位置：PropertyGrid.cpp
  目标命令：UpdatePropertyCommand
  说明：布尔属性与事件开关都经过这里

- UpdatePropertyFromFloat
  位置：PropertyGrid.cpp
  目标命令：UpdatePropertyCommand
  说明：ProgressBar 百分比、MediaPlayer 音量等浮点输入经过这里

- UpdateAnchorFromChecks
  位置：PropertyGrid.cpp
  目标命令：UpdatePropertyCommand 或专门的 UpdateAnchorCommand
  说明：这里不仅改 Anchor，还会通过画布回写布局位置，是单独命令的候选入口

### 1.5 模态编辑器

- ComboBoxItemsEditorDialog
- GridViewColumnsEditorDialog
- TabControlPagesEditorDialog
- ToolBarButtonsEditorDialog
- TreeViewNodesEditorDialog
- GridPanelDefinitionsEditorDialog
- MenuItemsEditorDialog
- StatusBarPartsEditorDialog

目标命令：BatchCommand 或各自的 SpecializedCommand

说明：这类入口一次性改动一组集合数据，天然适合“提交前抓旧值、提交后抓新值”，然后作为单条撤销记录压栈。

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

### 3.1 先接最便宜的命令入口

优先顺序：

1. UpdatePropertyCommand
2. AddNodeCommand / DeleteSelectionCommand
3. MoveSelectionCommand
4. ResizeSelectionCommand
5. ReparentSelectionCommand

原因：PropertyGrid 的提交点最集中，最容易先接入命令管理器。

### 3.2 再把拖拽和缩放的“起始快照”外提

- _dragStartItems 已经接近命令快照
- _resizeStartRect 已经接近命令快照

后续可以把这两份状态直接变成命令构造参数。