# Designer Undo 契约

Designer 的修改入口最终都经过 `CommandManager` 或显式文档事务。本文件只记录命令选择和失败恢复约束；
具体入口以 `DesignerCanvas`、`PropertyGrid` 和各命令类为准。

## 结果与历史

`IDesignerCommand::Execute/Undo` 与 `CommandManager::Undo/Redo` 都返回
`DesignerDocumentTransactionResult`。调用方需要保留 `State`、`Error` 与 `DocumentRestored`，不能把结果压成
一个成功/失败布尔值。

命令执行或恢复失败时：

- 文档、选择和运行时树应回到调用前状态；
- 失败的历史项仍留在原栈中，以便修正外部条件后重试；
- 只有 `Committed` 创建新的文档状态；`Unchanged` 不入栈；
- 活动的预览事务会拒绝另一个 Execute、Undo 或 Redo，不能让旧历史插入手势中间。

Dirty 状态由不可复用的文档 state ID 与保存点比较得出，不使用栈深度。保存后 Undo/Redo 可以离开再回到
保存点；Undo 后建立的新分支不会复用旧 redo 分支的 state ID。

默认历史上限为 128 项和 64 MiB。超出内存预算时从最远的历史开始淘汰，但至少保留最近一项。

## 命令选择

| 修改类型 | 命令 |
|---|---|
| 普通控件属性 | `ControlPropertyCommand` |
| Move、Resize、Reparent、同级重排和键盘微调 | `ControlPlacementCommand` |
| Adopt、删除和拥有型子树转移 | `ControlSubtreeCommand` |
| Grid 行列定义 | `ControlStructureCommand` |
| 事件映射与关联的源码迁移 | `EventHandlerCommand` |
| 文档级修改或尚未支持差量的入口 | `DocumentSnapshotCommand` / 文档事务 |

差量命令必须保存目标稳定身份和 expected 起点。Undo/Redo 前先验证起点；不能把命令应用到已经被另一条路径
修改过的对象上。需要重建控件时，命令还要恢复父级、同级顺序、选择和 Local/表达式值来源。

目前自定义结构编辑器 catalog 只有 GridDefinitions，`DesignerStructureEdit::SupportsDelta` 也只接受这一项。
新增结构编辑器之前，应先实现 capture、expected-state 验证、restore 和部分失败回滚，再登记到 catalog。

## 手势事务

Move、Resize 和属性拖动只有在 Begin 成功后才能进行第一次预览修改。MouseMove 只更新预览；MouseUp 把整次
手势提交为一条命令。

Escape、窗口失焦/停用、捕获丢失和 CancelMode 都通过统一取消入口回滚。不能在 MouseUp 后无条件清空快照，
也不能在提交失败后留下半应用的几何或属性状态。

交互结果通过 `OnInteractionTransactionCompleted` 发布；离散命令结果通过 `OnCommandCompleted` 发布。
事件中的 label、operation 和恢复状态应来自真实结果，包括空历史的 `Unchanged`。

## 合并

连续的同属性修改和键盘微调可以合并，但必须满足同一选择、同一 expected 中间状态，且相邻提交不超过一秒。
保存点、redo 分支、命令 label、选择或 expected 起点变化都会切断合并。合并只更新 after 状态，最初的 before
状态和对应 state ID 保持不变。

## 文件操作

New/Open 只有在新文档完整应用后才重置历史。Save 先把临时文件写入同一目录并完成原子替换，成功后才移动
保存点。自动恢复快照不能调用正式 Save；恢复成功后建立空历史但 Dirty 的新基线。

打开、保存或恢复失败时，原文档、选择、历史和保存点都必须保留。

## 验证

Undo/Redo 修改至少从仓库根目录运行 Designer 自测：

```powershell
.\x64\Debug\Designer.exe --self-test
```

自测使用真实 Canvas、PropertyGrid、命令栈和文件路径。新增入口不应依赖测试专用 setter 绕过正式事务。
