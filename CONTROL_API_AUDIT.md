# CUI Control API Audit

This note tracks the "production-useful" control APIs that should exist beyond the minimum demo surface.

## Completed in Current Pass

- ProgressBar
  - Stable `MaxValue`, `Value`, and `PercentageValue` clamping.
  - Added `OnValueChanged`, `SetRange`, `Increment`, and `Reset`.
- Slider
  - Added `SetRange`, `Increment`, `Decrement`, and `Reset`.
  - `Min`/`Max` setters now keep the range coherent.
- ComboBox
  - Added `ItemCount`, `GetSelectedItem`, `SetSelectedIndex`, `FindItem`, `AddItem`, `InsertItem`, `RemoveItemAt`, and `ClearItems`.
  - Item mutation keeps selection, text, scroll, popup state, and `OnSelectionChanged` synchronized.
- TextBox
  - Added selection APIs: `SelectionLength`, `HasSelection`, `Select`, `SelectAll`, `ClearSelection`.
  - Added editing APIs: `Clear`, `InsertText`, `Copy`, `Cut`, `Paste`, public `Undo`, and public `Redo`.
- RichTextBox
  - Added the same selection/editing API surface as `TextBox`.
  - Public editing APIs keep virtualized buffer, selection range, scroll, and render invalidation synchronized.
- PasswordBox
  - Added selection APIs: `SelectionLength`, `HasSelection`, `Select`, `SelectAll`, `ClearSelection`.
  - Added safe editing APIs: `Clear`, `InsertText`, and `Paste`.
  - Added configurable `PasswordChar` and `RevealPassword`.
  - Copy/cut are intentionally not exposed by default to avoid pushing secrets into the clipboard.
- CheckBox
  - Added `SetChecked`, `Toggle`, and keyboard activation with Space/Enter.
- RadioBox
  - Added `SetChecked` and keyboard activation with Space/Enter.
- Switch
  - Added `SetChecked`, `Toggle`, keyboard activation with Space/Enter, and programmatic animation/event synchronization.
- TabControl
  - Added page management APIs: `InsertPage`, `RemovePageAt`, `RemovePage`, `ClearPages`, and `FindPage`.
  - Added selection helpers: `SelectedPage` and `SelectPage`.
- TreeView
  - Added node management APIs on `TreeNode`: `AddNode`, `RemoveNode`, `RemoveNodeAt`, `ClearNodes`, and `FindChild`.
  - Added tree-level helpers: `AddNode`, `RemoveNode`, `ClearNodes`, `FindNode`, `SelectNode`, `SetNodeExpanded`, `ExpandAll`, and `CollapseAll`.
- GridView
  - Added `FullRowSelect`, enabled by default, so selected rows render with a full-row selection effect while preserving active-cell editing.
  - Added row/column/cell helpers: `RowCount`, `ColumnCount`, `AddColumn`, `RemoveColumnAt`, `AddRow`, `RemoveRowAt`, `GetCell`, and `SetCellValue`.
  - Added programmatic selection helpers: `SelectCell` and `SelectRow`.
- ScrollView
  - Added scroll metrics and range accessors: `GetScrollLayout`, `MaxScrollX`, and `MaxScrollY`.
  - Added programmatic scrolling helpers: `ScrollToStart`, `ScrollToEnd`, `ScrollToTop`, `ScrollToBottom`, `ScrollToLeft`, `ScrollToRight`, and `ScrollIntoView`.
- ProgressRing
  - Matched `ProgressBar` value semantics with `MaxValue`, `Value`, `OnValueChanged`, `SetRange`, `Increment`, and `Reset`.
- PictureBox
  - Added `LoadFromFile`, `ClearImage`, and `SizeToImage`.
- ToolBar
  - Added `RemoveToolButton`, `RemoveToolButtonAt`, and `ClearToolButtons`.
- SplitContainer
  - Added `SetOrientation`, `SetSplitterPercent`, `GetSplitterPercent`, `CollapseFirstPanel`, and `CollapseSecondPanel`.
- LoadingRing
  - Added `Start`, `Stop`, and `Restart`.
- ToolTip
  - Added `Target`, `IsOpen`, and `SetText`.
- ContextMenu
  - Added `FindItemById`, `FindItemByText`, `RemoveItem`, and `RemoveItemById`.
- DateTimePicker
  - Added `SetNow`, `SetToday`, `SetDate`, `SetTime`, and `GetDisplayText`.
- Menu/MenuItem
  - Added top-level and recursive find/remove helpers plus `ClearItems`/`ClearSubItems`.
- StatusBar
  - Added `InsertPart`, `RemovePartAt`, and `FindPart`.
- Taskbar
  - Added progress state helpers: `SetState`, `Clear`, `SetIndeterminate`, `SetPaused`, `SetError`, and `SetNormal`.
- Panel
  - Added `ClearControls`, `ContainsControl`, and `IndexOf`.
- NotifyIcon
  - Added `IsVisible`, `MenuItemCount`, and `RemoveMenuItem`; Show/Hide now track visibility.

## Next High-Value Passes

- GridView
  - Add batch update guard and clearer public edit commit/cancel APIs.
- MediaPlayer/WebBrowser
  - Continue expanding higher-level media/browser APIs as real usage demands; both already have larger specialized surfaces than basic controls.
- NotifyIcon/Taskbar
  - Consider Unicode-first overloads in a future pass.

## Verification Checklist

- Public setters clamp invalid input instead of leaving controls in inconsistent states.
- Programmatic changes fire changed events only when the observable value actually changes.
- Programmatic changes call `PostRender()` when the visual result changes.
- Text editing APIs preserve undo/redo behavior where the user would expect it.
- Container/list mutation updates selected indices and scroll offsets after removing or inserting items.
