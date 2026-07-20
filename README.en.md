# CUI - Modern Windows GUI Framework

[English](README.en.md) | [简体中文](README.md) | [Full Documentation](ReadMeFull.en.md)

[完整文档(中文)](ReadMeFull.md)

CUI is a modern native Windows GUI framework based on **Direct2D** and **DirectComposition** (C++20). It also comes with a **visual designer** (drag & drop, XML/XAML save/load, and automatic C++ code generation).

This repository mainly contains:
- `CUI/`: runtime GUI framework and controls
- `CuiDesigner/`: visual UI designer
- `CUITest/`: complete dynamic UI control gallery driven by external `DemoWindow.cui.xaml`
- `D2DGraphics/`: low-level graphics wrapper
- `Utils/`: general utilities still used by the designer and related projects

## Features

- **High-performance rendering**: Direct2D hardware acceleration + DirectComposition compositor
- **Controls**: 46+ commonly used UI controls
- **Layouts**: multiple layout containers (Stack/Grid/Dock/Wrap/Relative, etc.)
- **Events & input**: mouse/keyboard/focus/drag-drop events, with IME support
- **Generic data binding**: metadata-driven target properties with OneWay, TwoWay, OneWayToSource, OneTime, nested paths, and converters
- **SVG support**: built-in nanosvg (included)
- **Media playback**: built-in MediaPlayer control
- **WebView2 integration**: embed modern web content via Microsoft WebView2
- **Designer workflow**: property editing, live preview, XML/XAML design files, and C++ code generation

## Data binding

Runtime binding does not depend on hard-coded control types or target-property switches. Controls declare read, write, and change-notification capabilities through property metadata, and `BindingCollection` validates the selected mode:

```cpp
ObservableObject viewModel;
viewModel.SetValue(L"Name", std::wstring(L"CUI"));
textBox->DataBindings.Add(
    L"Text", viewModel, L"Name", BindingMode::TwoWay);
```

The same metadata now forms the common control-property contract.
`ControlPropertyOptions` declares a default value, coercion, an exact comparer, a
changed callback, and `AffectsMeasure` / `AffectsArrange` / `AffectsRender` flags.
Use `TracksLocalValue` when a public setter must represent a Local value from its first
assignment.
When a custom control setter uses the protected `SetPropertyField(...)`, direct C++
assignment, `TrySetPropertyValue(...)`, and Binding writes share normalization,
invalidation, and `OnPropertyValueChanged` notifications. `ResetPropertyValue(...)`
and `IsPropertyValueDefault(...)` let Designer and code generation avoid hard-coded
defaults.

Effective values use `Animation > VisualState > Local > Binding > Style > Theme > Inherited > Default` precedence.
`TrySetPropertyValue(name, value, source)` writes a layer, while
`ClearPropertyValue(...)` or `ClearPropertyValues(source)` removes it. Hidden layers
retain their latest value and become effective automatically when higher layers are
cleared. A Binding exclusively owns and releases its Binding layer; ordinary property
APIs cannot overwrite or clear an active Binding's layer, and duplicate bindings for
one target property are rejected even when constructed directly. Interactive controls
should update through `SetCurrentPropertyField(...)` so a TwoWay Binding is preserved
instead of being replaced by a Local value.

```cpp
button->TrySetPropertyValue(
    L"BackColor", BindingValue(themeColor),
    ControlPropertyValueSource::Theme);
button->ClearPropertyValue(
    L"BackColor", ControlPropertyValueSource::Theme);
```

`ControlStyleSheet` builds control-level themes and styles on top of those value
sources. Rules match runtime type, StyleId, multiple StyleClasses, and states such as
Hovered, Focused, Pressed, Disabled, and Checked. Declarations cascade by ID,
class/state, type specificity, and source order. Resource lookup is case-insensitive;
changing a rule or resource hot-reloads every attached control. Attaching a sheet to a
root applies it recursively, and children added later inherit it.

```cpp
auto theme = std::make_shared<ControlStyleSheet>();
theme->SetResource(L"Accent", BindingValue(accentColor));

ControlStyleSelector hoveredButton;
hoveredButton.Type = UIClass::UI_Button;
hoveredButton.RequiredStates = ControlStyleState::Hovered;
theme->AddRule(hoveredButton, {
    ControlStyleSetter::Resource(L"BackColor", L"Accent")
});

form->SetThemeStyleSheet(theme); // recursively applies the Theme layer
```

Common state colors, borders, corner radii, and spacing on `Button`, `TextBox`, and
`ComboBox` now use the same property metadata and can be supplied by Theme, Style, or
Binding. The Designer property panel also edits `StyleId` and comma-separated
`StyleClasses`; both round-trip through the XML document and are emitted into generated
C++ code.

When no control is selected, the form property panel also provides an Edit Document
Style Sheet command. Its structured editor manages typed resources, type/ID/class/state
selectors, and property setters, then applies valid changes immediately to the design
canvas. Missing resources, contradictory states, and values that cannot be converted by
the target property metadata are rejected before saving. The sheet round-trips through
XML, and generated C++ recreates the `ControlStyleSheet` and attaches it with
`SetStyleSheet(...)`.

The Setter property list now comes directly from the selected control type's runtime
property metadata. It infers Boolean, numeric, enum, color, thickness, size, and length
kinds together with a representative value. Even when that type is not yet present on
the canvas, a lightweight probe validates property existence, writability, conversion,
and coercion so errors are reported before a future control starts matching the rule.

The ordinary control property panel now comes directly from a catalog view that includes
every browsable scalar, including Legacy properties. Common properties that still use legacy
XML fields—including text, bounds, colors, margin/padding, and alignment—no longer have
separate display branches and route
edits through the same runtime metadata, so coercion, change callbacks, and
Local/Style/Binding precedence are no longer bypassed by direct field writes. A unified
access layer synchronizes the optional typed `props.metadata` bag from the declared
`Persistence` policy: Metadata/Automatic values are stored canonically, while Legacy
and Transient duplicates are removed. Reset clears the Local value and exposes the
next Style, Binding, Theme, or default value. Existing XML fields remain compatible
while load, undo/redo, and generated C++ share the canonical property name and kind.

`ControlPropertyOptions::Design` can additionally declare browsability, display name,
category and ordering, a preferred editor, strongly typed choices, numeric bounds, and
persistence policy. The ordinary property panel groups these descriptors and selects a
Boolean, choice, color, thickness, size, length, numeric, or text editor automatically.
`Legacy` and `Transient` properties are kept out of the generic metadata bag while
remaining valid Binding and style-setter targets.

`X`, `Y`, `Enabled`, and `Dock` are presentation names for the canonical `Left`, `Top`,
`Enable`, and `DockPosition` properties; Grid placement and Dock appear only under the
matching parent container. Former type-specific scalar rows for rings, DateTimePicker,
PictureBox, and TreeView are metadata-backed as well. ComboBox items, GridView columns,
tab pages, toolbar buttons, tree nodes, Grid definitions, menu items, and status-bar parts
retain structural dialogs, but their entries now come from the extensible
`DesignerCustomEditorCatalog` rather than a control-type `if/else` chain. All eight
dialogs run inside one strict document transaction captured before opening. Confirmed,
valid changes enter history as one command; cancel and no-op changes do not. Exceptions,
nested edits, invalid post-state, or history insertion failure restore the prior document
and complete selection. Results distinguish `Begun`, `Committed`, `Unchanged`,
`RolledBack`, `Canceled`, `Aborted`, `Rejected`, and `Failed`; cancellation also detects
and restores a dialog that unexpectedly leaked mutations instead of merely discarding
the before snapshot.

Properties owned by the Designer wrapper rather than runtime metadata—Name, Locked, Anchor,
StyleId, StyleClasses, font overrides, and the MediaPlayer source path—now share a
typed `DesignerControlPropertyCatalog`. PropertyGrid captures, applies, and resets
them through its Binder, so unique naming, inherited fonts, anchor-bound preservation,
and design-only data no longer live in separate text, Boolean, or float fallbacks.
Unknown properties and kind mismatches are rejected instead of writing raw fields.

`DesignerPropertyRowCatalog` then projects the form catalog, wrapper-owned catalog,
and runtime metadata into one row type carrying source, current typed value, category
and order, editor, choices, numeric hints, reset capability, and Binding/Validation/
Style/Theme diagnostics. Control rows are
deduplicated by canonical name and globally sorted before rendering, so each category
appears once. The Designer projects that row stream directly into CUI's native
`PropertyGridView`. Boolean, enum, color, and slider rows use native editors (including
`ColorPickerPopup`), while mixed values, reset affordances, action rows, and grouped
slider sessions are reusable `PropertyGridView` capabilities instead of Designer-built
rows of TextBox/CheckBox/ComboBox/Button controls. Diagnostics identify binding paths,
modes, converters, preview state, source validation, winning style rule IDs/specificity,
and higher-precedence values that mask a style candidate.

Canvas multi-selection is passed to the same Binder as a complete selection set. The
property panel intersects rows whose kinds, editors, and constraints are compatible,
marks mixed values and mixed effective sources explicitly, and excludes identity fields
such as `Name` from batch editing. A new value is preflighted against every target before
one batch is applied and recorded as a single undo command with the complete selection.
Rows owned by an active Binding on any target are read-only for both apply and reset.
Mixed diagnostics are flagged instead of presenting the primary target's details as if
they applied to the complete selection.

Property apply and reset now converge on the transactional `DesignerPropertyEdit`
service. It validates every target and captures Local/wrapper values plus tracked
metadata before mutation; a rejected or throwing setter restores all touched targets in
reverse order and returns a target-qualified error. PropertyGrid reserves a fixed,
accessible error-status area and clears it after a successful edit or selection change.
Ordinary scalar apply/reset and grouped sliders now commit per-property deltas, while
DataContext Schema, document styles, bindings, and structural editors share the
result-bearing `DesignerCanvas` transaction model. ComboBox Items (together with the
Local/Binding values, binding configuration, and tracked metadata of `SelectedIndex`),
TreeView nodes, GridView columns, GridPanel row/column definitions, and StatusBar parts
use a typed, single-control `ControlStructureCommand`; recursive Menu Items also retain
text, command IDs, shortcuts, enabled/separator state, and hierarchy ownership. Editors
that transfer Designer-owned child controls, such as TabControl and ToolBar, retain the
full-document fallback. PropertyGrid no longer
duplicates before/after document and selection capture, command construction, or failure
recovery. Grouped sliders restore their pre-drag property state when either preview or
commit fails.

`PropertyGrid::ApplyPropertyValue(...)`, `ResetPropertyValue(...)`, and read-only
row/error inspection expose that same production interaction path to automation without
bypassing the Binder or command stack. `Designer.exe --self-test` constructs the real
`DesignerCanvas` and `PropertyGrid` without showing a window and verifies mixed values,
multi-target edits, rejected-input feedback, reset, complete selection, document-
rebuilding undo/redo, transaction states, leaked-cancel mutation recovery, rejected and
throwing operation rollback, and restoration of Local fallback values across design-time
binding attach/detach as a runtime smoke gate beyond model-only unit tests.

The Designer toolbox is grouped into seven stable control families and supports
multi-token filtering by localized name, C++ type name, and category. Every control has
a code-native vector silhouette; long secondary type names stay on one ellipsized line
in narrow sidebars instead of wrapping across neighboring rows.

A toolbox item can still be clicked and then placed on the Canvas, or dragged directly onto
the Form or a nested container. After the system drag threshold is crossed, the Canvas
highlights the resolved destination and shows a translucent ghost at the control's default
size. Ordinary containers, the active TabPage, and either SplitContainer region use the same
resolution rules as the final placement. Mouse release commits one undoable add command;
`Escape`, capture loss, or window deactivation clears the preview and cancels without leaving
a pressed item or changing the document.

The toolbar and Canvas now support Copy, Cut, and Paste (`Ctrl+C` / `Ctrl+X` /
`Ctrl+V`). A multi-selection captures only its top-level roots and their complete
subtrees, so selecting both a parent and child does not duplicate content. Canonical,
readable CUI XAML is placed on the Windows Unicode text clipboard and can cross editor
or Designer-process boundaries. Ordinary `Ctrl+V` allocates fresh stable IDs, creates
case-insensitively unique names, repairs ordinary-parent and TabPage references, and cascades
successive results by 12 DIPs. `Ctrl+Shift+V` pastes in place, preserving the fragment's local
X/Y values without consuming the ordinary-paste cascade sequence. Canvas **Paste Here** aligns
the top-left of a multi-root fragment's bounding box with a pointed Canvas/Panel, TabPage, or
SplitContainer First/Second region while preserving relative root layout. Layout-managed targets
use their own semantics: Stack/Wrap/ToolBar insert at the pointed item boundary while preserving
root order, Grid assigns the pointed cell, Dock chooses an edge/fill position without displacing
the last-child fill item, and RelativePanel converts the point to Margin. Ordinary paste uses the
current control's container; selecting another Panel/layout container or TabControl targets that
container or active page. Repeated pastes keep the previous destination instead of nesting each
new container inside the preceding copy. Absolute-layout targets preserve hand-authored
`Canvas.Left` / `Canvas.Top` metadata; managed targets remove coordinates that their layout would
ignore. One paste or cut produces one Undo entry; invalid content, destinations, insertion indices,
or overflowing coordinates leave the document and history unchanged.

When an event value still denotes the control's conventional default handler, such as
`Button1_OnMouseClick`, copy and duplicate rename it with the new control to
`Button2_OnMouseClick`. Explicit references to that default handler elsewhere in the copied subtree
follow the same mapping. User-supplied custom or external handler names remain unchanged, so a copy
does not accidentally bind back to its source while intentional application-level sharing is preserved.

A fragment containing bindings also carries the referenced DataContext paths, every dotted parent,
and their type/read/write/notification capabilities; unrelated schema entries are omitted. Paste adds
only missing paths and keeps the destination declaration authoritative for an existing path. If the
destination previously used the permissive empty-schema mode, its existing binding paths are first
declared as `Unknown`, preventing the first imported explicit path from invalidating existing controls.
Schema merging and control insertion share one Designer command, so Undo/Redo restores both together.

A styled fragment similarly carries only rules whose static selectors match copied nodes and the
resources referenced by those rules. An identical destination style environment is reused, so
same-document duplicate does not keep appending styles. If resources, classes, style IDs, global rules,
or typed rules conflict, Designer assigns each pasted node a private style ID/qualifier classes and
remaps imported resources while preserving source states, cascade precedence, and source order. Existing
destination controls keep their own appearance, and one Undo/Redo command restores both styles and nodes.

The toolbar and all three Canvas paste commands now track Windows clipboard changes live. They are
disabled when there is no non-empty text, the clipboard contains only bitmap/file data, or a strict
document transaction is active, and become enabled after another application publishes text without
requiring Designer to restart or regain focus. A bounded delayed recheck handles clipboard owners that
are still releasing data when the Windows notification arrives. External non-empty text is still parsed
and transactionally validated when paste runs, so malformed CUI XAML reports a diagnostic without
changing the document.

`Ctrl+D` duplicates without replacing the system clipboard and preserves each source root's
ordinary parent, TabPage, or SplitContainer region. Absolute layout and RelativePanel offset by
12 DIPs through Location and Margin respectively; Stack/Wrap/Dock/ToolBar insert each copy next
to its source, while Grid preserves the source cell. This prevents managed layouts from ignoring
X/Y and either overlapping the copy or sending it to the end. The
toolbar's Arrange popup provides left/center/right and top/middle/bottom alignment,
horizontal/vertical equal distribution, matching width/height/size, and four layer-order
commands. Alignment and sizing use the primary selection as their reference; distribution
keeps the two outer controls fixed. Geometry commands require one non-layout-managed parent,
leaving Grid, Stack, Dock, Wrap, Relative, and ToolBar positioning to their layout rules.
Layer commands preserve both sibling order and explicit `ZIndex`; use `Ctrl+]` / `Ctrl+[` to
move one layer and add `Shift` to send directly to front/back. Each duplicate or arrangement
is one Undo entry and retains the complete selection.

The toolbar now exposes dynamically disabled Undo and Redo buttons. When available, their
accessible descriptions and the Canvas menu identify the next command. Right-click first
updates the primary hit selection, preserves a multi-selection when one of its members is
hit, and switches to the Form context on empty space. The popup centralizes Undo/Redo,
Cut/Copy/Paste, Duplicate, Delete, every arrangement command, Select All in Current
Container, and XAML editing. The keyboard menu key or `Shift+F10` opens the same surface;
`Ctrl+N`, `Ctrl+O`, and `Ctrl+S` invoke New, Open, and Save.

The built-in XAML editor is intentionally a thin shell. It provides multiline text input,
300 ms debounced validation, synchronization of valid documents, diagnostic navigation,
restore-last-valid, and transactional OK/Cancel. XML syntax failures and semantic failures
such as invalid properties, unknown controls, or duplicate names report 1-based line/column
coordinates and UTF-16 offsets. Only **Locate Error** or `F8` moves the caret; new input and
successful previews clear stale diagnostics. OK commits the whole editor session as one
Designer Undo entry, while Cancel restores the document and selection from before the dialog.
Completion, syntax coloring, find/replace, formatting, tag matching, and multi-checkpoint
history are intentionally left to the future Visual Studio/COM host.
A persistent strip below the Canvas exposes zoom out, the current percentage, zoom in,
and Fit; the Canvas View menu exposes the same commands. `Ctrl+wheel` zooms around the
design point under the pointer, `Ctrl++` / `Ctrl+-` step the zoom, `Ctrl+0` fits, and
`Ctrl+1` restores 100%. Drag with the middle mouse button or `Space+left mouse` to pan a
large surface. Zoom is clamped to 25%–400%, while selection handles and guides compensate
for the scale so they remain visible. Zoom and pan are view-only state: they do not enter
XML/XAML, mark the document dirty, or consume Undo/Redo history. Fit mode automatically
recalculates after the Designer viewport or designed Form changes size.

The bottom `Grid N` button and Canvas `View > Grid and Snapping` menu expose the same
session settings: show the grid, snap to the grid, snap to alignment guides, and choose a
5/10/20-DIP interval. Checked menu states reflect the active values, and popup placement is
clamped in logical coordinates for the current DPI and client area. These settings are
Designer view state only; changing them does not modify the design document, mark it dirty,
or consume Undo/Redo history.

The bottom **Tab Order** button and Canvas `View > Tab Order Mode` command provide a
WinForms-style keyboard-navigation editor. While active, every `IsTabStop=true`, keyboard-
focusable control receives a zoom-compensated blue `TabIndex` badge. Clicking controls assigns
indices from zero, the most recent assignment is green, and `Escape` exits. A hidden control
remains editable through its design outline, while controls excluded from tab navigation do not
intercept the gesture. `Auto Order by Layout` assigns consecutive indices from top to bottom and
left to right using absolute Canvas positions. Each manual assignment and a complete automatic
pass is one metadata-backed, undoable command. `TabIndex` persists to XML/XAML and generated
code; the mode switch and badges remain Designer-only view state.

Selected controls can be protected through the `Locked` property, the Canvas or Arrange
menu, or `Ctrl+L`; invoking it again unlocks the selection. Locked controls remain
selectable, property-editable, copyable, cuttable, and deletable, but lose their resize
handles and show an orange lock badge. Pointer moves, splitter drags, keyboard nudges,
arrangement, and outline reparenting reject the complete selection if any member is locked,
so a mixed selection never moves only partially. A batch lock/unlock is one undoable command.
It persists only as design metadata (`d:Locked`, or `locked` in Designer XML), survives
copy/paste and live-XAML round trips, and is excluded from runtime properties and generated code.

The left sidebar can switch between Toolbox and Document Outline. The outline tracks the
Form, ordinary containers, TabPages, and every descendant by stable ID, displays
`Name (Type)`, and marks controls whose own `Visible` value is false or whose design-time
`Locked` value is true. Covered, tiny, or
runtime-hidden controls therefore remain directly selectable and can be restored from the
property panel. Selecting a descendant on an inactive TabPage first activates its complete
tab-ancestor chain. Canvas primary selection expands and scrolls the matching outline path;
selecting the root returns to Form properties. Add/Delete/Paste, rename, Undo/Redo, and a
committed live-XAML session rebuild the tree while preserving expansion and scroll state by
stable ID. When the outline owns focus, arrow keys continue to navigate the tree, while
`Ctrl+C` / `Ctrl+X` / `Ctrl+V` / `Ctrl+D` / `Ctrl+L` / `Ctrl+Z` / `Ctrl+Y` / `Ctrl+A` and `Delete`
act directly on the design selection without returning focus to the Canvas.

The outline also supports direct drag-and-drop. The upper edge, center, and lower edge of a
node mean insert before, reparent inside, and insert after; dropping on empty space moves the
control to the Form root. A drag can reorder siblings or move across ordinary containers,
auto-scrolls near the viewport edges, preserves the control's screen position, and commits as
one undoable structure command. Cycles and invalid TabPage, Menu, StatusBar, or composite
container targets are rejected.

Designer command `Execute()`, `Undo()`, and `Redo()` now return the same result object
end to end. Failed or throwing restores retain their error and `DocumentRestored` state
and keep the command on its original undo/redo stack; empty history is an explicit
`Unchanged` result rather than the same `false` used for failure. Canvas Add/Delete now uses
`ControlSubtreeCommand`: the runtime tree owns attached controls exclusively, while the command
owns absent roots through `unique_ptr` and retains only normalized subtree nodes, reconstructible
parent locators, sibling order, ToolBar size overrides, and complete selection. It no longer stores
the whole document in history. Structural and placement/tree deltas require a successful
pre-capture before mutation, reject mismatched endpoints without losing the stack entry, and
restore the prior state and selection if capture or command insertion fails. Keyboard nudges, mouse
move/resize, and SplitContainer splitter previews use result-bearing Canvas delta-preview
transactions. The splitter reuses a single-target `ControlPropertyCommand`. Mouse-up commits
once; Escape, system cancellation, focus loss,
or capture loss restores the pre-preview document without destroying redo. Canvas retains
and publishes the last result for the Designer status area. Add/Delete/Undo/Redo publish
a separate discrete-command completion event with the history label, so empty deletion,
out-of-bounds add, empty history, and actual restore failure remain distinguishable and
toolbar/keyboard entry points no longer report unconditional success. A splitter metadata
failure aborts and rolls back instead of falling through to a raw setter.
The six structure deltas verify stable ID, name, control type, and the expected
collection state before changing anything. Undo/redo preserves the control instance;
an external-state conflict leaves the history entry retryable. Their memory usage grows
only with the edited ComboBox/Menu items, columns, nodes, tracks, or parts, not with
unrelated controls, styles, bindings, or resources in the document.

The Designer document lifecycle now uses the same result and restoration semantics.
`CommandManager` assigns a non-reusable document-state ID to every commit, so the save
point is independent of undo-stack depth: undoing a save and creating a new branch stays
dirty, while undo/redo back to the exact saved state becomes clean. New and Open clear
history and establish a fresh save point only after the complete target document applies;
parse or apply failures restore the previous document, complete selection, history state,
and dirty flag. Save writes and flushes a sibling temporary file before atomically replacing
the XML, so write or replacement failure preserves both the old file and the dirty save
point. The window title marks unsaved work with `*`; New, Open, and Close first settle
pending property edits, roll back an active Canvas preview, and offer Save/Discard/Cancel.
The current filename changes only after Open or Save actually succeeds.

A dirty document is also written to an automatic recovery snapshot 750 ms after the
last committed command. Snapshots live under
`%LOCALAPPDATA%\CUI\Designer\Recovery` and use the same flushed temporary-file plus
atomic-replace path as a normal save without moving the real save point. Each Designer
process owns a session file keyed by PID and process creation time. Startup skips sessions
whose owner is still running and offers only genuinely orphaned snapshots for recovery.
A recovered document has no fabricated Undo history but remains dirty until explicitly
saved. Successful Save, New, Open, or clean shutdown removes only the current session's
snapshot. Corrupt, truncated, oversized, or unsupported recovery envelopes are renamed
into quarantine without replacing the current document or blocking other recovery files.

Undo history is now bounded by both the existing 128-entry Undo-side count and a default 64 MiB
estimated-memory budget spanning the Undo and Redo sides. Trimming removes the farthest
history first but always retains at least one nearest actionable command, even when one
large snapshot exceeds the budget by itself. Ordinary control properties—including
multi-selection, Reset, Name, grouped sliders, and continuous SplitterDistance previews—store
per-target property deltas.
Keyboard nudges, pointer move/resize, Reparent, and Stack/Wrap reordering store a
placement/tree delta containing Location, Margin, explicit dimensions, alignment, Anchor,
Grid/Dock fields, a parent locator, and sibling index. These high-frequency edits no longer
retain two full documents or rebuild control instances during ordinary Undo/Redo. Legacy
properties restore a serialization-equivalent base value, while Metadata properties
preserve their exact Local and tracked states. Simple Add/Delete subtree entries remain below
32 KiB and small nested subtrees below 64 KiB, with their runtime ownership included in the
estimate. All eight modal structural editors now use local deltas: six store typed value
collections, while TabControl pages and ToolBar buttons transfer live subtree ownership,
Designer wrappers, stable IDs, selection, and attachment metadata without rebuilding instances.
Single Form/control event edits and document-wide handler renames use stable-ID event
deltas. Remaining Form-property and Binding edits retain the full-document transaction
fallback; gestures in an unidentifiable custom parent also fall back safely. Event deltas
verify every expected mapping, build replacement maps off-document, and commit them with
non-throwing swaps, so a stale command cannot overwrite newer handlers or rebuild controls.

Changes to the same property on the same selection, and consecutive keyboard nudges, merge
the original before state with the newest after state when commits are at most one second
apart. Merging never crosses an exact save point, an existing Redo branch, a selection
change, a different operation label, or a discontinuous current state. Targets are resolved
again by name and type after another snapshot command rebuilds controls. Pointer gestures
such as splitter dragging explicitly opt out of time-window merging. Canvas exposes the
budget, estimated usage, and Undo/Redo counts, and hosts can tune the budget for their
document scale.

The property panel now has separate Properties and Events views (`Ctrl+1` / `Ctrl+2`)
plus an immediate filter box. The Properties view owns properties, Binding, and structural
editors; the Events view owns named events and document-wide handler management. Each view
retains its own filter, collapsed categories, and scroll position, so an edit or selection
refresh does not expand every group or return to the top. Whitespace-separated tokens use
AND matching across names, categories, current values, editor kinds, choices, source
names, and diagnostic details. Rows show their effective `[Default]`, `[Theme]`,
`[Style]`, `[Binding]`, or `[Local]` source plus binding/error/mixed-diagnostic badges.
Accessible descriptions and inline summaries refresh after validation or style changes,
making precedence issues visible. Event rows are editable C++ member-function
names rather than Boolean switches: empty unbinds, legacy `1/true` values resolve to a
conventional default, and F4/the drop-down lists handlers with the same parameter
signature from both the document and the user `.h/.cpp`. A source candidate must be a unique
real member definition of the current `x:Class`; constructors, wrong signatures, duplicate
definitions, and comment/string/raw-string lookalikes are omitted. Events are grouped as action, value, mouse, keyboard, focus, drag/drop,
layout, lifecycle, data, navigation, media, or diagnostics, and the catalog declares one
default event per control type. With multiple controls selected, the Events view shows only
events whose names and exact parameter signatures are compatible on every target. Different
handlers appear as a mixed value; typing, choosing a suggestion, activating, or resetting the
row applies one atomic command to the complete selection. An unsupported event or cross-signature
name conflict rejects the whole batch. Mixed scalar editors start empty instead of inserting the
`<multiple values>` presentation placeholder into the real handler name. Whenever the current
filter leaves at least one visible event, the Events view exposes a **Generate/Locate Handler**
action whose `F12 · EventName` value identifies its target. It prefers the last selected visible
event, then the catalog default, then the first visible event. Double-clicking an event row,
pressing `F12`, invoking that action, or double-clicking a control on the canvas shares one
activation path: it reuses an existing handler or writes the conventional default through the
normal undoable transaction. Double-clicking the Form client surface activates its one-shot
`OnShown` event. After one explicit code export, another activation safely regenerates `.g.*`,
appends a missing user stub to `.cpp`, and opens the actual `.h` or `.cpp` definition. Source lookup ignores comments,
ordinary/raw strings, and declarations. The Designer detects VS Code or Visual Studio and
requests the exact definition line without sending paths through a shell. Hosts may override
the executable with `CUI_CODE_EDITOR` and provide a `CUI_CODE_EDITOR_ARGS` template containing
`{file}`, `{line}`, and `{column}`; a failed editor launch falls back to the system file
association. The status area reports exact navigation or fallback. A successful export persists the C++ class
identity as `x:Class`, separately from `Form.Name`, and the extensionless path relative to
the design file as `d:CodeBehind`; save/reopen therefore retains the association. Before
the first export it only asks the user to establish a target and never guesses an overwrite
path. `x:Class` accepts `Acme.Views.MainWindow` or `Acme::Views::MainWindow` and canonicalizes
to C++ `::`; generated headers declare the leaf type inside that namespace, independently of
the output file stem. Invalid class segments/handlers and cross-signature reuse are rejected.
Once associated, Regenerate also reports exact freshness: `*` means the design and code
differ, `!` means one or more files are missing, and “generation blocked” means an existing
user-file identity or handler signature prevents a safe update. No suffix means the five-file
plan is byte-for-byte current. Document commits mark the target stale immediately and debounce
an exact recheck; Undo can return immediately to a known generated state, and reactivating the
app detects external file drift.

Code export separates regenerated and user-owned files. `FormName.g.h/.g.cpp` contains
the generated base class, protected typed control references, virtual event hooks, and
RAII-owned `Subscribe(std::bind_front(...))` connections. `FormName.h/.cpp` is created
once; later exports only append missing handler stubs to the user source. A handler may also
be defined inline as `void Handler(...) {}` in the exact user class body. In that case
`FormName.handlers.g.inc` omits the conflicting in-class declaration. A currently bound handler
defined in the user `.cpp` is declared with `override`, making the generated virtual contract
compiler-visible. After unbinding, that retained declaration becomes an ordinary member so the
existing user definition still compiles; rebinding restores `override`. A shared C++ token index jointly recognizes inline and
out-of-class definitions in `.h/.cpp` while ignoring comments,
ordinary/raw strings, and prefix collisions such as `Handle` versus `HandleSave`; fake text
therefore cannot suppress a required stub. It also compares parameter types for an existing
same-name definition and requires a non-static, non-cv/ref `void` member that can really
override the generated virtual hook. Parameter names and whitespace may change, but return
type, `static`/`const`/ref qualifiers, or parameter-type drift is rejected before any target
is replaced and is excluded from candidates and body migration. Inline `noexcept` and the
equivalent trailing `auto ... -> void` remain supported. Preprocessor directives and continued macro bodies never
contribute scope tokens; inactive branches selected by definite `#if 0` / `#if 1` conditions
are ignored, while unknown macro conditions are retained conservatively. Masking preserves
the original offsets and line numbers so diagnostics, navigation, and body migration share
the same positions. Fully qualified definitions, traditional nested namespace
blocks, and C++17 `namespace Acme::Views` blocks resolve to the same `x:Class`; a similarly
named class in a neighboring namespace does not match. Before writing, the same token surface verifies
that an existing user header contains exactly one class body in the precise `x:Class` namespace,
derives the current generated base, and that the user header and source jointly contain exactly
one usable default constructor, preventing a manually changed `x:Class` from mixing class
generations. The constructor may be out of class, inline, or `= default`; `= delete` and duplicate
cross-file definitions block before any write. If the user source is missing while the header owns
the constructor, source recreation does not emit a duplicate body. Export macros, `final`,
access specifiers, and multiple direct bases remain valid; a same-leaf class in a definitely
inactive branch or neighboring namespace no longer proves identity.
Export refuses same-name files with missing markers or mismatched class identity. Every target in one export is staged and flushed beside
its destination before batch commit. If a target is locked or replacement fails, previously
committed existing files are restored from backups in reverse order and newly created targets
are removed, preventing mixed generations across `.g.h`, `.g.cpp`, `.handlers.g.inc`, and the
user source. A plan also captures the existence and exact bytes of all five targets before it
reads user code, then rechecks them before staging, before each mutation, and through the backup.
If an IDE or another process changes or creates any target after planning, the entire commit is
rejected instead of overwriting that edit. Interactive export additionally uses `GenerateAndCommit` across the file and
document transactions: if generation succeeds but the code-behind association cannot commit,
all five paths recover their exact pre-export existence and bytes through another rollback-safe,
conditional batch. An external edit made during the association callback is preserved and reported
as an incomplete rollback rather than being overwritten by the old snapshot. Explicit handler-body
migration and its Undo/Redo path use the same conditional commit and rollback semantics.
write/delete batch.
Only an explicit export creates or changes the code-behind association. After an output is
selected, the Designer shows the current `x:Class`, target base, and resulting `d:CodeBehind`,
and accepts a qualified C++ class name. It preserves the existing identity by default; a class
migration occurs only when the user explicitly edits that field. Migration never rewrites old
user bodies, and the five-file identity guard rejects a target that still belongs to the old
class. An unsaved design first records the identity, then computes the portable relative path
when the design file is first saved. The complete class, extensionless output, and relative
association are validated before generation starts. The association participates in normal
document transactions and Undo/Redo; absolute machine-specific paths are never persisted.
A Form with no child controls is also exportable, so Form-only events such as `OnShown` and
`OnClose` still receive generated user handlers. Once associated, the toolbar's Regenerate
action reuses the current target without reopening the file/class dialogs; open, recovery, and
code-behind Undo/Redo keep its enabled state synchronized.

The Designer window and build tooling now call the same HWND-free
`DesignCodeGenerationService`, so interactive export, CI, and local builds cannot drift into
separate generation rules. `CodeGenerator::BuildFilePlan` first constructs the exact five-file
result, then the normal path atomically commits it. `InspectFreshness` reuses that plan strictly
read-only: it creates no directories and preserves timestamps. Arbitrary valid user additions
in `.h/.cpp` remain part of the plan, while missing event stubs, managed/declaration drift, or
missing targets are detected precisely. `CuiCodeGenCore/CuiCodeGenCore.vcxproj` is the sole compilation
owner of `CodeGenerator.cpp`, shared `CppUserCodeIndex.cpp`, and the service implementation and emits `CuiCodeGenCore.lib`;
the Designer, `CuiCodeGen.exe`, and `CUICoreTests` only link that library instead of compiling
parallel copies. `CuiCodeGen.exe` accepts `.xml` and `.xaml`; by default it reads
`x:Class` and `d:CodeBehind`, while explicit class and extensionless output-base overrides are
available:

```powershell
.\CuiCodeGen\x64\Debug\CuiCodeGen.exe generate `
    .\CuiStaticGeneratedSample\NamespacedWindow.cui.xaml
.\CuiCodeGen\x64\Debug\CuiCodeGen.exe generate .\MainWindow.cui.xaml `
    --output .\Generated\MainWindow --class Acme.Views.MainWindow --quiet
```

Exit codes `0`, `1`, and `2` mean success, generation failure, and command-line usage error.
The command retains the same atomic five-file commit and user-code protection. For incremental
pre-compile integration, reference `CuiCodeGen.vcxproj`, set `CuiCodeGenExe`, declare one or
more `CuiDesign` items, and import `build/CuiCodeGen.targets` after
`Microsoft.Cpp.targets`:

```xml
<ItemGroup>
  <CuiDesign Include="MainWindow.cui.xaml">
    <OutputBase>$(ProjectDir)Generated\MainWindow</OutputBase>
    <!-- ClassName is normally omitted so x:Class stays authoritative. -->
  </CuiDesign>
</ItemGroup>
<Import Project="..\build\CuiCodeGen.targets" />
```

The target records freshness for the design file, imported targets rules, and all five code
files with a contract-versioned stamp under `$(IntDir)\CuiCodeGen`. User `.h/.cpp` extensions
remain intact, while an external edit to `.g.h`, `.g.cpp`, or `.handlers.g.inc` makes an ordinary
Build restore canonical generated content. All five files must exist before the stamp is accepted,
and unchanged inputs do not launch the generator.
The current generation contract is 7. A generator
output-semantic change bumps the contract version so the old stamp path cannot be accepted,
while an ordinary executable relink does not cause needless generation. Even when an input timestamp changes, byte-identical canonical output keeps
the code files and their timestamps intact, avoiding a needless C++ rebuild.
`CuiStaticGeneratedSample` uses this build path instead of relying on a manual pre-generation
step.

The runtime representation follows a hybrid roadmap: static generation remains the
default deployment path, while dynamic loading reuses the same document model instead
of maintaining a second property/container implementation. `DesignDocumentGraph` is now
the single topology layer for IDs, parent resolution, and child order.
`DesignDocumentControlPool` instantiates controls through an injected factory, retains
`unique_ptr` ownership before attachment, rolls back automatically on failure, and
transfers ownership only when materialization succeeds. The public
`RuntimeDocumentLoader` now transactionally builds a complete control tree from a
`DesignDocument`, canonical XML, or a XAML-style string/file; failure leaves the caller's existing
`RuntimeDocument` unchanged. The runtime document owns every root until
`ReleaseRootControls()` or `TransferRootControlsTo()`, supports lookup by stable ID or design-time name, attaches a
DataContext while restoring suspended Local fallbacks, and owns RAII control/form event
connections supplied by an application name resolver. `ApplyFormProperties(...)`
projects the form model onto an application-owned `Form` and retains that target;
`BindFormEvents(...)` likewise retains the Form and resolver so in-place, recomposed,
and replaced reloads can refresh presentation and rebuild Form connections. Static code-generation input
now comes from that same `RuntimeDocument`, and generated document styles are attached
to every root tree instead of calling the nonexistent `Form::SetStyleSheet`.

The same `.g.h` also emits a `ClassReferences<TDocument>` dynamic reference view for every
named control with a stable `DesignId`. It is a zero-owning template, so a static-only consumer
does not acquire a `CuiRuntime` dependency merely by including the header. A dynamic host passes
its `RuntimeDocument` or `session.Document()` and then uses the same typed `GetXxx()` shape as
the static class, or retains a `ReferenceXxx()` handle that resolves the stable ID on every
access and therefore follows InPlace, Recomposed, and Replaced reloads. The view stores the
weak lifetime view returned by `document.Reference()`, not a raw document pointer: it follows
document moves, and after destruction its boolean conversion is false while `TryDocument()`
and `GetXxx()` return null:

```cpp
Acme::Views::MainWindowReferences<DesignerModel::RuntimeDocument>
    ui{session.Document()};
auto namespaceButton = ui.ReferenceNamespaceButton();
if (namespaceButton) namespaceButton->Text = L"Save";
```

A raw pointer returned by `GetXxx()` represents the current instance only and should not be
retained across a reload that may replace topology; use `ReferenceXxx()` for long-lived access.
`Document()` remains as the compatibility reference accessor and requires a live view; use
`TryDocument()` when lifetime is uncertain.
Static construction, dynamic XAML, and hot reload now share names, types, and stable identities
without hand-written ID lookup or casts.

When the document has named events, the generated header also emits a `ClassEventSink`. Every
unique handler becomes a pure virtual function, while
`RegisterDynamicEventHandlers(registry)` generates and registers all ordinary-control, Form,
and restricted custom-event routes in one call, binding member callbacks to the sink instance
with `std::bind_front`. A dynamic controller only derives from the sink and implements the
functions (overrides may remain private); missing handlers or signature drift fail at C++ compile
time. `RuntimeEventHandlerRegistry::RegisterScopedBatch` snapshots the complete route set and
returns a move-only lease, so a duplicate, signature conflict, or exception restores the exact
pre-call registry instead of leaving a partial resolver. The sink owns that lease: registering
against another registry, explicitly calling `UnregisterDynamicEventHandlers()`, or destroying
the sink removes only its generated routes. A loaded RuntimeDocument still owns its existing
EventConnections, so generated callbacks also carry a weak lifetime gate; after lease release,
those old subscriptions safely become no-ops instead of calling a destroyed controller. The
generated static Form inherits the same sink, preserving one virtual-handler contract for both
deployment paths. Event sinks are non-copyable/non-movable and retain the registry's UI-thread
lifetime rule.

```cpp
class MainWindowController final : public Acme::Views::MainWindowEventSink {
private:
    void HandleSave(Control*, MouseEventArgs) override { /* ... */ }
    // The compiler requires the remaining handlers currently referenced by XAML.
};

MainWindowController controller;
DesignerModel::RuntimeEventHandlerRegistry handlers;
if (!controller.RegisterDynamicEventHandlers(handlers, &error)) {
    // The whole batch failed and the registry still has its previous state.
}
options.ControlEventResolver = handlers.ControlResolver();
auto formResolver = handlers.FormResolver();
// controller.UnregisterDynamicEventHandlers(); // optional; destruction also releases it
```

`DesignDocumentEventIndex` resolves every form/control event reference into a handler
name plus its exact C++ Event function type. It centrally rejects unknown events, invalid
identifiers, and cross-signature name reuse. Event rows remain editable and offer
same-signature handlers; the document-wide Rename Handler action updates every shared
reference as one compact `EventHandlerCommand`. It checks Form/stable-control identity and
all expected mappings before committing replacement maps, so Undo/Redo preserves live
control instances. XML, XAML, dynamic loading, and static generation therefore use the same
contract. Static output still emits
`Subscribe(std::bind_front(&GeneratedClass::Handler, this))`. By default, renaming deliberately
does not rewrite arbitrary user C++ bodies; regeneration preserves the old user code and
creates a missing safe stub for the new name. When the old handler has exactly one compatible
definition in the user `.cpp` and the target has no same-signature body, the dialog offers an
explicit “migrate user body and regenerate” option. It replaces only the member-name token,
preserves the body/comments/literals byte-for-byte, and commits the five code files together
with the event-map command. Undo/Redo performs the inverse migration and regeneration; an
external source conflict or generation failure leaves history retryable and restores the
pre-operation document and file snapshot.

Once code-behind is associated, every event row also shows `[checking]`, `[implemented]`,
`[pending generation]`, `[source missing]`, `[signature error]`, or `[duplicate definition]`.
The scan uses the same token and parameter-type index as generation, ignoring comments,
ordinary/raw strings, whitespace, and parameter-name changes. Document commits, completed
generation, and app reactivation refresh the badges without losing event-group expansion or
scroll position. Double-clicking a current implementation navigates directly; a missing body is
generated first; a signature error or duplicate definition opens the existing bad body instead
of stopping at the expected generation failure. When overloads share a name, navigation selects
the definition compatible with the event's exact parameter types.

Dynamic hosts no longer need a handler-name `if/switch` for every load.
`RuntimeEventHandlerRegistry` registers a handler name, Designer event descriptor, real
CUI `Event` member, and callable as one route. Catalog entries now derive the field name,
function identity, and generated C++ parameter types from the real member; parameter names
are only readable code-generation labels. Registration also checks exact member identity,
so `OnMouseMove` cannot masquerade as same-shaped `OnMouseClick`. Ordinary `Event<>`, the
validation notification wrapper, and inherited Form/Control events share this contract.
The registry rejects invalid names, cross-type reuse, and duplicate routes.
`ControlResolver()` and `FormResolver()` capture shared registration
state, so handlers added for a later hot reload are immediately visible to resolvers
already retained by a RuntimeDocument. Static generation still emits direct
`std::bind_front` subscriptions and does not acquire runtime string dispatch.

For the common “file + Form + named events + save-driven reload” host, prefer
`RuntimeDocumentSession`. It gathers the document, shared event registry, and
threadless watcher into one non-movable UI-thread session without hiding transaction
boundaries or creating a worker thread. Initial `MountFile()` becomes visible only after
parsing, materialization, Binding, control/Form events, presentation, and root commit all
succeed. The host still calls `Poll()` and handles explicit `Reloaded` / `Failed` results.
The Form and objects captured by callbacks must outlive the session.

```cpp
Form form; // Declare first: the Form must outlive the session.
DesignerModel::RuntimeDocumentSession session{
    std::chrono::milliseconds{150}};
session.EventHandlers().RegisterControl(
    L"HandleSave", UIClass::UI_Base, L"OnMouseClick",
    &Control::OnMouseClick,
    std::bind_front(&MainWindow::HandleSave, this), &error);
session.EventHandlers().RegisterForm(
    L"HandleCommand", L"OnCommand", &Form::OnCommand,
    std::bind_front(&MainWindow::HandleCommand, this), &error);

DesignerModel::RuntimeDocumentSessionMountOptions mount;
mount.DataContext = viewModel;
if (!session.MountFile(L"MainForm.cui.xaml", form, mount, &error)) {
    // Form and session.Document() retain their pre-mount state; register and retry.
}

// Called from a timer on that same UI thread.
const auto result = session.Poll();
if (result.State == DesignerModel::RuntimeDocumentWatchState::Failed)
    ShowReloadError(result.Error); // The previous UI remains active.
```

The `RuntimeDocumentLoader`, standalone registry, and watcher below are the equivalent
lower-level composition points for in-memory text, pre-attach inspection, custom root
hosts, or application-managed multi-document lifecycles.

Full property application, composite attachment, layout refresh, and style assembly now
converge in the neutral `DesignDocumentMaterializer`. Both `DesignerCanvas` and
`RuntimeDocumentLoader` consume its detached control forest, so dynamic loading no longer
constructs a hidden Designer or depends on Designer-owned fonts and client-surface
lifetimes. Static generation remains the default deployment mode, while dynamic loading
is usable by tools, previews, and controlled hosts; future property support has one
materialization path to maintain.

```cpp
DesignerModel::RuntimeDocument document;
DesignerModel::RuntimeDocumentLoadOptions options;
options.DataContext = viewModel;
DesignerModel::RuntimeEventHandlerRegistry handlers;
if (!handlers.RegisterControl(
        L"HandleSave", UIClass::UI_Base, L"OnMouseClick",
        &Control::OnMouseClick,
        std::bind_front(&MainWindow::HandleSave, this), &error)) {
    // Invalid name, signature conflict, duplicate route, or unknown event.
}
options.ControlEventResolver = handlers.ControlResolver();
if (!handlers.RegisterForm(
		L"HandleCommand", L"OnCommand", &Form::OnCommand,
		std::bind_front(&MainWindow::HandleCommand, this), &error)) {
	// Form events use the same name/signature rules.
}
if (!DesignerModel::RuntimeDocumentLoader::LoadFileIntoForm(
		L"MainForm.cui.xml", form, document, options,
		handlers.FormResolver(), &error)) {
	// Parse, materialization, Binding, event, presentation, or root commit failed;
	// both form and document retain their previous state.
}
```

`Load*IntoForm(...)` is the recommended first-load path for a dynamic window. It
commits Form presentation, Form-event connections, and the root forest only after the
candidate document is fully ready. A host that needs to inspect or adjust the detached
tree can call `Load*()` followed by `document.AttachToForm(...)`; the second step still
rolls back as a unit. Once roots have been handed off by `AttachToForm`,
`TransferRootControlsTo`, or the legacy manual-release path, direct `Load*()` is rejected
without side effects. Subsequent changes must use `Reload*()` so the retained host
adapter participates in commit and recovery.

`XamlDocumentParser` is a readable frontend over that same `DesignDocument`, not a
second control runtime. It supports a `Form`/`Window` root, nested controls, `x:Name`,
optional `DesignId`, Grid definitions, TabPage content, both SplitContainer regions,
attached layout properties, direct text, metadata-backed enum values, and floating or
`Auto` control width/height. `{Binding ...}` becomes the existing generic binding model;
undeclared dotted source paths are added to the DataContext schema with an unknown value
kind. Event attributes accept either a handler such as `Click="HandleSave"` or
`Click="Auto"`, and are ultimately connected by generated `std::bind_front` code or a
dynamic host's name resolver. Resources and styles support typed values, setters,
class/state selectors, implicit styles that only declare `TargetType`, and WPF-like
`x:Key`, `Style="{StaticResource ...}"`, and `Style.BasedOn`. `BasedOn` can reference
either a named style or an implicit `{x:Type Button}` key; base setters are applied first,
derived setters replace them by property name, and missing or cyclic references are rejected.
`Style.Triggers` keeps `IsMouseOver`, `IsKeyboardFocused`, `IsPressed`, `IsEnabled`,
`IsChecked`, and `IsSelected` as state aliases, while `Trigger.Property` may also name any
readable, observable target metadata property that the Designer can represent. Its `Value`
is converted and compared using that property's actual type. `MultiTrigger` can mix state
aliases and ordinary metadata properties with AND semantics. Trigger setters use the same
resource, property-metadata, coercion, and cascade validation and are inherited through
`BasedOn`; duplicate, unobservable, or type-incompatible conditions are rejected while
loading. A `DataTrigger` compares a value from each target
control's effective local or inherited DataContext through `Binding="{Binding Path}"` with a
literal `Value`. `MultiDataTrigger.Conditions` accepts two or more equivalent `Condition`
entries and matches them with AND semantics. Each target observes its own dotted paths and
reconnects when an intermediate object is replaced, so DataTemplate items sharing one Style
cannot overwrite one another's condition context. Data
conditions currently support only Path plus a literal value; Converter, Mode/UpdateMode, and
StaticResource values are not accepted.
`Trigger`, `MultiTrigger`, `DataTrigger`, and `MultiDataTrigger` can all declare WPF-style
`EnterActions` and `ExitActions`. `BeginStoryboard`, `PauseStoryboard`, `ResumeStoryboard`, and
`StopStoryboard` execute on initial activation and false-to-true or true-to-false edges.
Every matching control owns independent named clocks; an ordinary style refresh does not
restart them, while removing the rule or sheet stops the owned clocks and reveals the current
lower value source. A Style has no template namescope, so the matched control is the animation
target and `Storyboard.TargetName` must be omitted. This is a dynamic-XAML capability; the
auxiliary static C++ generator rejects Styles containing TriggerActions instead of silently
dropping them.
Runtime property metadata remains authoritative, so a newly exposed generic property does
not need a dedicated XAML setter.

```cpp
const std::string_view xaml = R"(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="MainForm" Text="CUI XAML" Width="480" Height="240">
  <Form.Resources>
    <Color x:Key="Accent">#FF0078D4</Color>
    <Style TargetType="Button">
      <Setter Property="Raised" Value="false" />
      <Style.Triggers>
        <Trigger Property="IsMouseOver" Value="true">
          <Setter Property="BorderThickness" Value="2.5" />
        </Trigger>
        <MultiTrigger>
          <MultiTrigger.Conditions>
            <Condition Property="IsMouseOver" Value="true" />
            <Condition Property="Text" Value="Ready" />
          </MultiTrigger.Conditions>
          <Setter Property="Round" Value="12" />
        </MultiTrigger>
        <DataTrigger Binding="{Binding User.Status}" Value="Ready">
          <Setter Property="Visible" Value="true" />
          <DataTrigger.EnterActions>
            <BeginStoryboard x:Name="ReadyPulse">
              <Storyboard>
                <DoubleAnimation Storyboard.TargetProperty="Round"
                                 To="12" Duration="0:0:0.15" />
              </Storyboard>
            </BeginStoryboard>
          </DataTrigger.EnterActions>
          <DataTrigger.ExitActions>
            <StopStoryboard BeginStoryboardName="ReadyPulse" />
          </DataTrigger.ExitActions>
        </DataTrigger>
        <MultiDataTrigger>
          <MultiDataTrigger.Conditions>
            <Condition Binding="{Binding User.Status}" Value="Ready" />
            <Condition Binding="{Binding User.IsAdmin}" Value="true" />
          </MultiDataTrigger.Conditions>
          <Setter Property="Raised" Value="true" />
        </MultiDataTrigger>
      </Style.Triggers>
    </Style>
    <Style x:Key="PrimaryButton" TargetType="Button" Class="primary"
           BasedOn="{StaticResource {x:Type Button}}">
      <Setter Property="BackColor" Value="{StaticResource Accent}" />
      <Setter Property="Round" Value="8" />
    </Style>
  </Form.Resources>
  <StackPanel x:Name="root" Width="Auto" Height="Auto"
              Orientation="Vertical" Spacing="8">
    <Button x:Name="saveButton" Classes="primary"
            Style="{StaticResource PrimaryButton}"
            Text="{Binding User.Caption, Mode=OneWay}"
            Click="HandleSave" />
  </StackPanel>
</Form>)";

if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
        std::string(xaml), document, options, &error)) {
    // Parse/materialization failed; document still owns its previous tree.
}
```

Legacy static collection data can be written directly as `ComboBoxItem` and `ListViewItem` content
without an extra `*.Items` wrapper. `ListBox` now requires `ItemsSource + DataTemplate` and no longer
accepts directly authored `<ListBoxItem>` children. Controls with multiple collections keep
explicit property elements. `GridView` and `PagedGridView` share `GridViewColumn`, `GridViewRow`,
and `GridViewCell`; cells support `Value`, `IsChecked`, `Tag`, and `SelectedIndex`. The same data
round-trips through dynamic loading, Designer saves, and static C++ generation:

```xml
<ComboBox x:Name="mode">
  <ComboBoxItem Content="Debug" />
  <ComboBoxItem Content="Release" />
</ComboBox>

<PagedGridView x:Name="jobs">
  <PagedGridView.Columns>
    <GridViewColumn Header="Name" Width="180" />
    <GridViewColumn Header="Ready" Type="Check" />
  </PagedGridView.Columns>
  <PagedGridView.Rows>
    <GridViewRow>
      <GridViewCell Value="Compile" />
      <GridViewCell IsChecked="true" />
    </GridViewRow>
  </PagedGridView.Rows>
</PagedGridView>
```

`Control.Foreground` accepts device-independent `SolidColorBrush`, `LinearGradientBrush`,
`RadialGradientBrush`, and `ImageBrush` objects. The built-in file source supports `ImageSource`
(including SVG), `Stretch="None|Fill|Uniform|UniformToFill"`, horizontal/vertical alignment,
and `Opacity`. `Label`, `TextBox`, and their derived controls currently use this
brush for text rendering, so a custom type no longer needs to copy an entire `Update()` just to
draw gradient text. See `CUITest/CustomControls` and `DemoWindow.cui.xaml` for the complete sample:

```xml
<Label Text="Declarative paint">
  <Control.Foreground>
    <LinearGradientBrush StartPoint="0,0" EndPoint="1,0">
      <LinearGradientBrush.RelativeTransform>
        <RotateTransform Angle="15" CenterX="0.5" CenterY="0.5" />
      </LinearGradientBrush.RelativeTransform>
      <LinearGradientBrush.Transform>
        <TranslateTransform X="4" Y="0" />
      </LinearGradientBrush.Transform>
      <GradientStop Color="#E30940" Offset="0" />
      <GradientStop Color="#1373E8" Offset="1" />
    </LinearGradientBrush>
  </Control.Foreground>
</Label>
```

All four brush kinds support WPF-style `Brush.RelativeTransform` and `Brush.Transform`
property elements. Canonical round-trips retain the concrete `SolidColorBrush`,
`LinearGradientBrush`, `RadialGradientBrush`, or `ImageBrush` owner. Rendering applies brush
content, RelativeTransform in normalized brush space, projection into the painted bounds, and
then Transform in DIP space. Thus `CenterX="0.5" CenterY="0.5"` naturally denotes the brush
center for RelativeTransform, while Transform is suitable for a final DIP offset.

Resources can remain in the main document or be split into WPF-like merged dictionaries.
Later merged dictionaries override earlier ones, and local entries in the current dictionary
override all merged entries. Relative image URIs in an external dictionary resolve from that
dictionary's directory. Designer round-trips preserve `Source` instead of expanding imported
entries into the main XAML:

```xml
<Form.Resources>
  <ResourceDictionary>
    <ResourceDictionary.MergedDictionaries>
      <ResourceDictionary Source="Themes/Dark.xaml" />
    </ResourceDictionary.MergedDictionaries>
    <Color x:Key="Accent">#FF2F6FE4</Color>
  </ResourceDictionary>
</Form.Resources>
```

Value resources may also live directly in any control's `Resources`. Lookup walks the
current control, logical ancestors, Form/document resources, and finally Application/theme
resources. The nearest matching key wins, and reparenting a subtree reevaluates against its
new logical route instead of copying the previous parent's dictionary. Local dictionaries also
support file-backed `MergedDictionaries` and survive canonical XAML, v18 snapshots,
component/DataTemplate visual nodes, and runtime recomposition.
In addition to value resources such as colors, numbers, strings, Thickness, Brush,
ImageSource, Geometry, and Transform, control-local `Style` now has lexical semantics.
Implicit/named styles, `BasedOn`, setters/triggers, and dynamic resources can inherit outer
declarations, while the nearer dictionary wins equal-specificity conflicts. `DataTemplate`,
`ComponentDefinition`, `ItemsPanelTemplate`, and `GroupStyle` may now live in any control resource scope and shadow definitions along
the current-control, logical-ancestor, then Form route. Templates may contain further local
templates or components. The optional C++ generator rejects these dynamic XAML objects explicitly
instead of flattening or silently dropping them.

```xml
<StackPanel>
  <StackPanel.Resources>
    <Color x:Key="AccentText">#FFE84B3C</Color>
    <Style TargetType="Label" BasedOn="{StaticResource BaseLabel}">
      <Setter Property="ForeColor" Value="{DynamicResource AccentText}" />
    </Style>
    <DataTemplate x:Key="RowView" DataType="Row">
      <Label Text="{Binding Name}" ForeColor="{StaticResource AccentText}" />
    </DataTemplate>
  </StackPanel.Resources>
  <Label ForeColor="{DynamicResource AccentText}" />
  <StackPanel>
    <StackPanel.Resources>
      <Color x:Key="AccentText">#FF36A269</Color>
    </StackPanel.Resources>
    <Label ForeColor="{StaticResource AccentText}" />
  </StackPanel>
</StackPanel>
```

When a Designer copy operation extracts a subtree that depends on ancestor-local object resources,
the clipboard promotes the selected template, component, panel template, group style, group-header
template, and visible value resources onto the fragment root. The pasted subtree therefore keeps
shadowing behavior and also works when no global fallback definition exists. A
`GroupStyle.HeaderTemplate` resolves at the GroupStyle declaration scope, so a nearer use-site
template with the same key does not retroactively change the declared group style.

Writable control properties and Style/Trigger setters also accept
`{DynamicResource Key}`. The parser preserves it as a Local-value expression rather
than replacing it with a literal. Lookup first walks control-local dictionaries along
the logical parent route, then checks document and Application/theme resources; edits,
sheet replacement, and reparenting reevaluate the expression. A temporarily
missing key is valid and exposes the lower-precedence value until the resource appears.
A regular Local assignment or ClearValue removes the expression. Structural references
such as `Style`, `BasedOn`, `ItemsSource`, and `ItemTemplate` remain StaticResource-only.
Canonical XAML, v18, Designer resource renames, clipboard, transactional hot reload, and the
optional C++ generator preserve the Static/Dynamic identity.

```xml
<Color x:Key="AccentText">#FF2F6FE4</Color>
<Style TargetType="Label">
  <Setter Property="ForeColor" Value="{DynamicResource AccentText}" />
</Style>
<Label ForeColor="{DynamicResource AccentText}" Text="Live resource" />
```

Resource lookup is no longer hard-coded as path concatenation in the XAML parser. Configure
search directories through `Application` before creating windows or loading documents. Without
configuration, document-relative, executable-directory, and current-directory lookup are enabled:

```cpp
const std::filesystem::path startup = Application::StartupPath();
Application::ConfigureResourceDirectories({
    (startup / L"Assets").wstring(),
    startup.wstring()
});
```

For product packaging, implement `IResourceSource`, add it to a `ResourceResolver`, and install it
with `Application::SetResourceResolver()`. A source returns bytes, a stable identity, a logical base
URI, and an optional `WatchPath`, so packaged resources do not need to masquerade as files. The only
built-in implementation today is `FileResourceSource`. Dynamic documents collect their main XAML,
recursive merged dictionaries, and image dependencies; changed, deleted, and restored file-backed
dependencies participate in the same debounced transactional hot-reload flow.

`Control.Clip` accepts `RectangleGeometry`, `EllipseGeometry`, `PathGeometry`, and nested
`GeometryGroup` values in control-local DIPs. A path is composed explicitly from `PathFigure`
and `LineSegment`, `BezierSegment`, `QuadraticBezierSegment`, or `ArcSegment` objects. Paths
and groups support `EvenOdd` (the default) and `Nonzero` fill rules. Every geometry can also
use `Geometry.Transform` with Matrix, Translate, Scale, Rotate, Skew, or TransformGroup.
A Clip is an
additional constraint beyond the normal layout-bounds clip. It is inherited by descendant
rendering, ordinary and virtual accessibility hit testing, and Designer hit testing, but it
does not change Measure/Arrange or the layout rectangle:

```xml
<Panel Width="320" Height="180">
  <Control.Clip>
    <PathGeometry FillRule="Nonzero">
      <Geometry.Transform>
        <TranslateTransform X="4" Y="6" />
      </Geometry.Transform>
      <PathFigure StartPoint="18,0" IsClosed="true">
        <LineSegment Point="282,0" />
        <ArcSegment Point="300,18" Size="18,18" SweepDirection="Clockwise" />
        <LineSegment Point="300,144" />
        <ArcSegment Point="282,162" Size="18,18" SweepDirection="Clockwise" />
        <LineSegment Point="18,162" />
        <ArcSegment Point="0,144" Size="18,18" SweepDirection="Clockwise" />
        <LineSegment Point="0,18" />
        <ArcSegment Point="18,0" Size="18,18" SweepDirection="Clockwise" />
      </PathFigure>
    </PathGeometry>
  </Control.Clip>
</Panel>
```

`Control.RenderTransform` supports `MatrixTransform`, `TranslateTransform`, `ScaleTransform`,
`RotateTransform`, `SkewTransform`, and declaration-ordered `TransformGroup` composition.
`RenderTransformOrigin="x,y"` is relative to the control bounds. A render transform does not
participate in Measure/Arrange, but it consistently affects control and descendant rendering,
pointer hit testing, dirty bounds, accessibility bounds, Designer selection bounds, and static C++ output:

```xml
<Button Text="Transformed" RenderTransformOrigin="0.5,0.5">
  <Control.RenderTransform>
    <TransformGroup>
      <RotateTransform Angle="-4" />
      <ScaleTransform ScaleX="1.05" ScaleY="1.05" />
    </TransformGroup>
  </Control.RenderTransform>
</Button>
```

Custom visual structure is declared with XAML `ComponentDefinition` resources.
The definition owns the public property/event contract and template, so the
Designer can edit and preview it without loading application binaries. C++ is
used only for application behavior and event handlers.

```xml
<Form xmlns="urn:cui" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
       xmlns:local="urn:sample:components">
  <Form.Resources>
    <ComponentDefinition x:Key="local:StatusBadge" BaseType="Panel">
      <ComponentDefinition.Properties>
        <ComponentProperty Name="Caption" Type="String" Default="Ready"
          BindsTwoWayByDefault="true"
          DefaultUpdateSourceTrigger="LostFocus" />
		<ComponentProperty Name="Status" Type="String" Default="Idle"
		  ReadOnly="true" />
        <ComponentProperty Name="IsActive" Type="Bool" Default="false" />
        <ComponentProperty Name="AccentColor" Type="Color" Default="#FF0078D4" />
        <ComponentProperty Name="ContentPadding" Type="Thickness" Default="8" />
        <ComponentProperty Name="AccentLevel" Type="Int" Default="1"
          Inherits="true" BindsTwoWayByDefault="true"
          AffectsParentMeasure="true" />
        <ComponentProperty Name="DisplayMode" Type="Enum" Default="Detailed">
          <ComponentProperty.Choices>
            <ComponentChoice Value="Compact" DisplayName="Compact view" />
            <ComponentChoice Value="Detailed" DisplayName="Detailed view" />
          </ComponentProperty.Choices>
        </ComponentProperty>
      </ComponentDefinition.Properties>
      <ComponentDefinition.ContentProperties>
        <ComponentContentProperty Name="Content" Cardinality="Single" Default="true" />
      </ComponentDefinition.ContentProperties>
      <ComponentDefinition.Template>
        <StackPanel x:Name="PART_Root"
                    Padding="{TemplateBinding ContentPadding}"
                    ForeColor="{TemplateBinding AccentColor}">
          <VisualStateManager.VisualStateGroups>
            <VisualStateGroup x:Name="CommonStates">
              <VisualState x:Name="Normal" />
              <VisualState x:Name="Active">
                <VisualState.StateTriggers>
                  <StateTrigger Property="IsActive" Value="true" />
                </VisualState.StateTriggers>
                <VisualState.Setters>
                  <Setter TargetName="PART_Root" Property="BackColor"
                          Value="#2036A269" />
                </VisualState.Setters>
              </VisualState>
            </VisualStateGroup>
          </VisualStateManager.VisualStateGroups>
          <Label Text="{TemplateBinding Caption}" />
		  <Label x:Name="PART_Status" Text="{TemplateBinding Status}" />
          <StackPanel x:Name="PART_Content"
                      ComponentSlot.Presents="Content" />
        </StackPanel>
      </ComponentDefinition.Template>
    </ComponentDefinition>
    <Style TargetType="local:StatusBadge">
      <Setter Property="AccentColor" Value="#FFE95420" />
    </Style>
  </Form.Resources>
  <local:StatusBadge x:Name="statusBadge" Caption="Warning">
    <Label Text="Projected content" />
  </local:StatusBadge>
</Form>
```

Component properties currently cover Bool, numeric, String, and closed declarative Enum values plus Color,
Thickness, Point, Vector, Rect, Size, and Length value types. Structured Brush, Geometry, and Transform defaults use
`ComponentProperty.Default` and the same object-element syntax as control properties;
component defaults may also use `Default="{StaticResource Key}"`; local value resources are discovered before
component schemas and styles, regardless of source order. Component QNames are valid style targets;
the runtime selector checks both the component identity and built-in base type, so a
component style cannot leak to an ordinary control with the same base class.

`ComponentProperty` also supports `Inherits`, `BindsTwoWayByDefault`,
`DefaultUpdateSourceTrigger`, `AffectsParentMeasure`, and `AffectsParentArrange`.
Inherited values occupy their own `Inherited` precedence layer. An omitted Binding
`Mode` is stored as `Default` and is resolved from the target metadata: normally
OneWay, or TwoWay for a property carrying `BindsTwoWayByDefault`. An omitted
`UpdateSourceTrigger` is resolved in the same way; a component property can choose
`PropertyChanged`, `LostFocus`, or `Explicit`. Canonical XAML and v14 snapshots
preserve all behavior metadata.

`ReadOnly="true"` declares state maintained by component behavior and consumed by
templates or external bindings. Such a property remains readable, observable,
inheritable, and valid as a `Binding`/`TemplateBinding` source, but instance literals,
Style setters, Binding/MultiBinding targets, and ordinary property writes are rejected.
The PropertyGrid shows a disabled row and the Binding editor omits the property from
target choices. C++ behavior updates dynamic read-only state through
`TrySetReadOnlyPropertyValue(...)` / `ClearReadOnlyPropertyValue(...)`; these APIs are
the runtime equivalent of a property key and do not reopen public XAML writes.
Read-only properties cannot opt into `BindsTwoWayByDefault` or a non-PropertyChanged
default update trigger.

Component events default to `RoutingStrategy="Direct"` and may opt into
`Bubble` or `Tunnel`. An ancestor handles a descendant component event with a
WPF-style attached event attribute such as
`local:StatusBadge.Invoked="HandleDescendantInvoked"`. The event identity is the
owner QName plus event name, not just the local name.

Handlers use `void(Control*, DeclarativeEventArgs&)`. `sender` is the current
route target, while the arguments expose `OriginalSource`, `Source`,
`CurrentTarget`, owner QName, routing strategy, payload, and writable `Handled`.
Normal registrations skip an event after it is handled;
`RuntimeComponentEventRegistrationOptions::HandledEventsToo` opts back in.
The `RaiseDeclarativeEvent(args)` overload returns the final handled state to a
component behavior. Canonical XAML, v14 snapshots, clipboard dependency closure,
event-handler rename, and transactional hot reload preserve attached routes.

The component template root may declare `VisualStateManager.VisualStateGroups`.
Each group has exactly one triggerless fallback state. Multiple `StateTrigger`
conditions in one state use AND semantics, and the first matching conditional state
wins in declaration order. An `EventTrigger` can reference only an event declared by
that component; its state remains active until `GoToVisualState(...)` is called or a
relevant condition property is reevaluated. An empty `Setter.TargetName` targets the
component host, while a non-empty name must resolve inside the template namescope.
Different groups cannot own the same target property.

Active setters occupy a dedicated `VisualState` value source above Local. Leaving
a state clears only that source and reveals the previous Local, Binding, Style,
Inherited, or Default value. `VisualState.Storyboard` supports finite
`DoubleAnimation` over Int/Int64/Float/Double metadata, `ColorAnimation` over color
metadata, `ThicknessAnimation` over Margin, Padding, or a declared Thickness property,
`PointAnimation` over `RenderTransformOrigin` or a declared Point property,
`VectorAnimation` over a declared Vector property,
`RectAnimation` over a declared Rect property or a named template part's rectangle
clip through `(Control.Clip).(RectangleGeometry.Rect)`, `SizeAnimation` over floating-DIP Size metadata, and
`MatrixAnimation` over Matrix metadata or `MatrixTransform.Matrix` object paths through WPF-style
`Storyboard.TargetName` / `Storyboard.TargetProperty`.
A finite `Duration` is required; `From`, `To`, `By`, and `BeginTime` are optional.
Endpoint combinations follow WPF: From to base, current to To, current to current+By,
From to From+By, and From to To. To wins when To and By are both authored, while By is
still preserved; omitting all three animates the current effective value back to base.
Color By values add per channel, Thickness By values add per edge, Point and Vector By values add
per x/y component, Rect By values add per x/y/width/height component, Size By values add per dimension,
Matrix By values add per six components, and Double By values also work on RenderTransform
subpaths. The delta is type-converted without absolute-value coercion; each final frame
still passes through target metadata coercion. Quadratic, cubic, and sine easing support
EaseIn, EaseOut, and EaseInOut. Completed animations hold their final value in the same
state layer, while disabling system animations applies the final value immediately.

Regular Double/Color/Thickness/Point/Vector/Rect/Size/Matrix timelines and their key-frame variants support `IsAdditive` and
`IsCumulative`. WPF endpoint
classification determines the additive foundation: By-only always uses the current
value; FromTo and FromBy add the current value only when IsAdditive is true; Automatic,
From-only, and To-only never add it twice. Each completed repetition accumulates
`To-From` for a regular animation and the final key-frame value for a key-frame
animation. With AutoReverse, the complete forward/backward cycle is one repetition.
Generated transitions use their resolved absolute destination and clear both flags,
while explicit transition storyboards retain the authored behavior.

`VisualStateGroup.Transitions` now provides WPF-style state transitions. A
`VisualTransition` can select `From` and/or `To`; resolution is deterministic:
exact From+To, To-only, From-only, then the default transition. Duplicate selectors
are rejected. `GeneratedDuration` and
`VisualTransition.GeneratedEasingFunction` synthesize interpolation for Double/Color/Thickness/Point/Vector/Rect/Size/Matrix
state animations and can smoothly return an old state to its base value.
`VisualTransition.Storyboard` overrides selected targets: an explicit target suppresses
the corresponding generated animation while all other targets remain generated.
During a transition `GetCurrentVisualState(...)` immediately reports the destination;
the destination setters/storyboard are committed when the transition completes. An
interruption continues from the current effective frame. Passing `false` to the new
`GoToVisualState(..., useTransitions, ...)` overload, or disabling system animations,
bypasses the transition and applies the destination directly.

`DoubleAnimation` also supports the first object-subproperty adapter. A named template
part can target a declared transform operation with a path such as
`(Control.RenderTransform).(TransformGroup.Children)[0].(ScaleTransform.ScaleX)`.
Translate, scale, rotate, and skew numeric members are validated against the actual
operation type and index. Distinct leaves in one state are composed into one complete
`RenderTransform` value per frame, so they do not overwrite one another and the whole
base transform is restored when the state is left.

The first Geometry subproperty adapter lets `RectAnimation` target
`(Control.Clip).(RectangleGeometry.Rect)` on a named template part; an authored
`UIElement` owner is canonicalized to `Control`. Each frame replaces only Rect and
preserves RadiusX, RadiusY, Geometry.Transform, and the rest of the root Geometry.
Targets without an explicitly declared RectangleGeometry are rejected.
Other public Geometry leaves on the same Clip root are addressable too: `DoubleAnimation`
targets `RectangleGeometry.RadiusX/RadiusY` and `EllipseGeometry.RadiusX/RadiusY`, while
`PointAnimation` targets `EllipseGeometry.Center`. The concrete Geometry owner must match
the template object; absolute radii are nonnegative while By may remain signed.
`DoubleAnimation` can also target a declared translate, scale, rotate, or skew member on a
Rectangle, Ellipse, Path, or GeometryGroup through
`(Control.Clip).(Geometry.Transform).(TransformGroup.Children)[n].(TransformType.Property)`.
Concrete Geometry owners and `UIElement.Clip` authored aliases canonicalize to `Geometry` and
`Control.Clip`. Geometry-transform, Rect, Center, and radius animations compose against one Clip
root, preserving figures, radii, fill rules, and simultaneous member updates.
The `PathGeometry` object graph is addressable through indexed WPF-style paths as well:
`(Control.Clip).(PathGeometry.Figures)[n].(PathFigure.StartPoint|IsClosed|IsFilled)`, optionally
continuing through `(PathFigure.Segments)[m]` to point members on Line, Bezier,
QuadraticBezier, or Arc segments. Arc also exposes `Size`, `RotationAngle`, `IsLargeArc`, and
`SweepDirection`. Point, Size, and Double timelines animate continuous leaves; discrete Object
key frames switch booleans and sweep direction. Figure/segment indices and concrete segment
owners must match the actual Clip, absolute Arc sizes are nonnegative, and sweep direction accepts
only `Clockwise` or `Counterclockwise`. These leaves compose with FillRule, Geometry.Transform,
and other Geometry members against the same Clip root.
`GeometryGroup` additionally permits any number of `(GeometryGroup.Children)[n]` hops before
targeting the supported public members, PathFigure/PathSegment graph, or `Geometry.Transform`
of a nested Rectangle, Ellipse, Path, or GeometryGroup. Every hop validates the actual Group and
child index, and the final concrete owner must match the selected child. PathGeometry and
GeometryGroup `FillRule` can also switch discretely between `EvenOdd` and `Nonzero`. All nested
leaves still compose against one complete Clip value, preserving parent transforms, sibling
geometries, and untouched child data.
The Brush subproperty adapter lets `ColorAnimation` and `DoubleAnimation` target
`(Control.Foreground).(GradientBrush.GradientStops)[n].(GradientStop.Color)` and
`(Control.Foreground).(GradientBrush.GradientStops)[n].(GradientStop.Offset)`.
Linear/radial owner aliases canonicalize to `GradientBrush`. The target must explicitly
declare a linear or radial brush and a valid stop index; same-root Color and Offset updates
are composed while brush coordinates, opacity, and untouched stops are preserved. Authored
Offset From/To/key-frame values stay in 0..1, By may be signed, and frame writes use the
property's 0..1 coercion.
Public brush members use the same Foreground object-path root. `ColorAnimation` targets
`(Control.Foreground).(SolidColorBrush.Color)`; `DoubleAnimation` targets
`(Control.Foreground).(Brush.Opacity)` on every brush kind and `RadiusX/RadiusY` on radial
gradients; `PointAnimation` targets linear `StartPoint/EndPoint` and radial
`Center/GradientOrigin`. Concrete brush and `UIElement.Foreground` authored aliases are
canonicalized. Absolute opacity stays in 0..1, radii stay nonnegative, and By may remain
signed. These leaves compose with GradientStop and Transform updates against one complete
Brush root, so simultaneous color, coordinate, radius, and opacity animations do not overwrite
one another.
`DoubleAnimation` can also target a declared translate, scale, rotate, or skew member on any
brush kind through
`(Control.Foreground).(Brush.Transform|RelativeTransform).(TransformGroup.Children)[n].(TransformType.Property)`.
Concrete brush owners and `UIElement.Foreground` authored aliases canonicalize to `Brush` and
`Control.Foreground`. Brush-transform and GradientStop animations compose against the same
Foreground root each frame, preserving coordinates, opacity, untouched stops, and the other
transform.
`MatrixAnimation` uses the same three Transform object-path families, but its leaf must be
`(MatrixTransform.Matrix)`. The six finite components are interpolated and combined as one
strongly typed Matrix. Regular/key-frame timelines, From/To/By, easing,
Additive/Cumulative, StaticResource, generated transitions, and explicit transitions all
share this contract across RenderTransform, recursive Geometry.Transform, and Brush
Transform/RelativeTransform; sibling DoubleAnimation leaves still compose per frame.
The runtime stores all indirect paths in one `ObjectPathAccessor` variant, while the
Designer uses one classification, canonicalization, root-property, and resolution API.
Future Geometry and Brush adapters therefore extend one boundary instead of adding
parallel fields and lifecycle branches.

Key-frame timelines support `DoubleAnimationUsingKeyFrames`,
`ColorAnimationUsingKeyFrames`, `ThicknessAnimationUsingKeyFrames`,
`PointAnimationUsingKeyFrames`, `VectorAnimationUsingKeyFrames`, `RectAnimationUsingKeyFrames`,
`SizeAnimationUsingKeyFrames`, and `MatrixAnimationUsingKeyFrames`. All eight value types provide discrete, linear, easing, and spline frames, including WPF's
`EasingThicknessKeyFrame`, `EasingPointKeyFrame`, `EasingVectorKeyFrame`, `EasingRectKeyFrame`, `EasingSizeKeyFrame`, and `EasingMatrixKeyFrame`.
Every frame has an explicit finite `KeyTime` and a strongly typed `Value`,
which may use `StaticResource`. Easing frames use the same quadratic, cubic, and sine
functions and easing modes; spline frames use a WPF-style four-coordinate `KeySpline`
whose control points are in 0..1. An omitted `Duration` resolves to the final `KeyTime`,
equal times retain declaration order, and the first segment starts from the effective
value captured on state entry, or from zero before adding that value when IsAdditive is
true. Key-frame animations may also target the supported
RenderTransform, recursive GeometryGroup.Children, public Geometry-member, PathFigure/PathSegment, Geometry.Transform, public Brush-member, and Brush Transform subpaths and participate in per-frame
composition.

`ObjectAnimationUsingKeyFrames`, like WPF, accepts only `DiscreteObjectKeyFrame` and can
switch any writable property represented by the metadata catalog. Scalar `Value`
supports Visibility, bool, enum, string, Thickness, and the other catalog value types,
including `StaticResource`; Brush, Geometry, and Transform values may be authored inside
`DiscreteObjectKeyFrame.Value`. Object timelines share BeginTime, Duration,
RepeatBehavior, AutoReverse, FillBehavior, SpeedRatio, and acceleration/deceleration
behavior, but do not expose From/To/By, easing, `IsAdditive`, or `IsCumulative`. Explicit
transition storyboards execute object switches. Generated transitions do not invent
object interpolation: they expose the property base value during the transition and
start the destination state's object timeline when the transition completes.

All supported regular and key-frame animations now share a WPF-style Timeline active
period. `RepeatBehavior` accepts a positive Count (including fractional values such as
`0.5x`), a finite positive TimeSpan, or `Forever`. With `AutoReverse="true"`, one
repetition is a forward iteration followed by a backward iteration, so Count applies to
the complete round trip. `BeginTime` is applied once before the first iteration, not on
every repeat. A live state change transactionally commits its initial animation frame before
starting the timeline clock, so parsing, resource preparation, and first-frame work cannot
consume animation time. Installing the destination state after an explicit transition keeps
that transition's deterministic sample tick, making manual-clock and live execution agree.
`FillBehavior` defaults to `HoldEnd`, retaining the final sample produced
by the active-period boundary; `Stop` releases the animation and exposes the lower value
source. When transform subpaths run together, Stop restores only its own member and
does not overwrite active or HoldEnd siblings. A Forever timeline remains active until
the state is left or a `GoToVisualState(..., false, ...)` request interrupts it.
`SpeedRatio` accepts a finite positive value and scales the timeline-local clock, not
`BeginTime`; Count-based active periods vary inversely with speed, while a TimeSpan
RepeatBehavior remains a fixed duration in parent-clock time. `AccelerationRatio` and
`DecelerationRatio` are each in 0..1 and their sum may not exceed 1. They use WPF's
normalized peak-rate mapping within each simple iteration before animation easing and
key-frame sampling, so SpeedRatio remains the average rate over the natural duration.

Unregistered arbitrary object-graph paths and Uniform/Paced `KeyTime`
are still rejected explicitly.
A state cannot mix a whole-property setter/animation with transform subpaths, and
different groups cannot split ownership of the same transform root. Canonical XAML, v14 snapshots,
Designer preview, clipboard dependencies, and transactional reload preserve both
state definitions and resource-backed From/To/By animation endpoints. Such resource changes
force a full candidate rebuild. Behaviors can call
`GetCurrentVisualState(...)` / `GoToVisualState(...)` and subscribe to
`OnVisualStateChanged`, but must not create a runtime-only parallel state contract.

A component template root may also declare `<RootType.Triggers>`.
`EventTrigger RoutedEvent="..."` references a declared component event and executes
`BeginStoryboard`, `PauseStoryboard`, `ResumeStoryboard`, or `StopStoryboard` actions
in declaration order. `BeginStoryboard x:Name` gives the clock a stable name; the
control actions reference it through `BeginStoryboardName`. Event storyboards reuse
all supported regular/key-frame timelines and object subpaths. Their output occupies
a dedicated `Animation` value source above `VisualState`, so Stop exposes the current
state or local value instead of restoring a stale value captured at Begin time.
Canonical XAML, v14 snapshots, resource/clipboard dependencies, Designer preview,
and transactional reload all preserve these triggers and named clocks.

Applications may optionally attach business state, message handling, or a final
render overlay to a declarative component by registering an
`IDeclarativeComponentBehavior` for its exact QName. The factory receives an already
materialized XAML host and never creates or replaces a control. `Attach` runs after the
template, content presenters, styles, and layout properties are installed. A behavior
can resolve local template names through `FindDeclarativeTemplatePart`, resolve a
declared content slot through `FindDeclarativeContentPresenter`, update read-only state,
preprocess host messages, and receive DPI/device-resource notifications. The host owns
the behavior and calls `Detach` before template children are destroyed.

`DeclarativeComponentBehaviorRegistry` is passed through
`RuntimeDocumentLoadOptions::DeclarativeComponentBehaviors`. Normal in-place and
recomposition reloads retain behavior with a reused host. Supplying a registry
explicitly requests replacement; a factory or `Attach` failure leaves the previous
runtime tree intact. A component remains fully functional pure XAML when no behavior
is registered. Use `NativeSurface` instead when application code owns measurement,
continuous high-performance rendering, or complex input interaction.

Declarative components expose visual children through
`ComponentDefinition.ContentProperties`. Direct children use the default content
property; named properties use `<local:Type.Property>`. A template container marked
with `ComponentSlot.Presents` receives those public children at runtime. Single versus
multiple cardinality, presenter uniqueness, Designer undo/redo, clipboard persistence,
snapshot round-trip, and structural hot reload all share the same contract.

Typed item data uses `DataType` for the record contract, `DataTemplate` for the item
visual tree, and `IBindingList` for application-owned observable collections. XAML can
also declare first-class `DataList` / `DataRecord` resources and reference them through
`ItemsSource="{StaticResource People}"`; this is runtime data, not a designer-only `d:`
sample bag. `ItemsControl`, `ComboBox`, `ListView`, and `ListBox` share the contract. `ListBox` now
uses the `ItemsControl -> Selector -> ListBox` hierarchy and directly hosts DataTemplate visuals; the
Designer filters `ItemsSourceResource` / `ItemTemplate` choices by `ItemType`.
When `x:Key` is omitted, a `DataTemplate` uses `DataType` as an implicit type key.
An ItemsControl/ListBox with no explicit `ItemTemplate` infers the strongly typed item
contract and selects the nearest matching template through control, ancestor, then document
resources; an explicit template always wins. The property panel presents the empty value as
Automatic and lists only keyed templates that can be referenced by StaticResource.
Canonical XAML, v21 XML, merged dictionaries, clipboard dependency isolation, preview,
and transactional reload preserve the same resources.

`ComboBox`, `ListView`, and `ListBox` also share `DisplayMemberPath` and
`SelectedValuePath`. `SelectedValue` preserves the scalar or record-identity type and
supports true TwoWay binding to an application key. Paths are validated against the
collection `DataType`; `ListBox.SelectedItem` preserves record identity across collection reordering.
`ItemContainerGenerator` consumes precise Add/Remove/Move/Swap/Replace notifications and preserves
unaffected container, binding, and selection identity; only Reset falls back to a candidate-tree rebuild.
Default display-member changes replace only the affected container. `ComboBox` now uses
`SelectedIndex=-1` for the unselected state instead of
implicitly selecting the first item.

Strongly typed single objects use `ObjectType="BindingSource" DataType="Person"`.
`ContentPresenter.Content` and `ContentControl.Content` accept either scalar values or that
object reference; `ContentTemplate` may explicitly
reference a keyed `DataTemplate`, or the nearest implicit template is selected by DataType
through control, ancestor, and document resources. Template Bindings use the content object
as their DataContext, and replacing Content atomically rebuilds the visual root. A
ContentPresenter rejects authored visual children. ContentControl is the default content host:
it owns either one authored visual child or data Content rendered through its internal presenter,
never both. Button now uses the same single-content contract: `Text` remains a compatibility entry
point, while canonical XAML can use literal or bound `Content`, `ContentTemplate`, or one complex
authored visual root. The button remains the sole interaction surface and its content subtree is
presentational. GroupBox and Expander now derive from `HeaderedContentControl`: Header and Content
are independent slots, and each accepts a literal, Binding, DataTemplate, or one authored visual
root. Complex headers use `<GroupBox.Header>...</GroupBox.Header>` or the corresponding Expander
property element. `Text` remains a legacy header fallback; new XAML should use `Header`. Without a
template, scalar content becomes text and object content falls back to `DisplayMemberPath` or
`HeaderDisplayMemberPath`. The Designer filters explicit template choices by the Content schema type
and offers no incompatible templates for a known scalar binding. Lexical shadowing, clipboard
promotion, canonical XAML, v21 XML, and
transactional reload share the same resolution path.

Controls can now separate appearance from C++ behavior with `ControlTemplate`.
Built-in `ContentControl`, `Button`, `GroupBox`, `Expander`, `ItemsControl`, `ListBox`, `ListBoxItem`, `ComboBoxItem`, and `TreeViewItem` types as well as declarative
component QNames can be used as `TargetType`. Keyed templates are selected by
`Template="{StaticResource Key}"`, while keyless templates use the actual XAML type and lexical
resource lookup. Components sharing one native `BaseType` do not match each other's templates.
The generated template root does not consume authored Content. A `ContentPresenter` inside the
template can claim a WPF-style `ContentSource="Content"` or `ContentSource="Header"` slot, deriving
the corresponding Content/ContentTemplate/DisplayMemberPath aliases. Authored visuals are physically
owned by that presenter while retaining the templated control as their logical Designer parent; data
content continues through the presenter's DataTemplate or text fallback. A templated control skips its native chrome but retains input, Checked/Expanded state,
and content behavior. `TemplateBinding` live-observes host metadata, and template trees can use
VisualState, StateTrigger, Setter, Storyboard, EventTrigger, and named parts. Direct `Template`,
`Style.Template`, lexical implicit templates, and `ComponentDefinition.Template` form the explicit,
style, implicit, and type-default precedence levels. Trigger-driven structural template replacement
remains deliberately unsupported. The resource participates in merged/local dictionaries, canonical
XAML, v29 XML, Designer selection and preview, clipboard-local promotion, and structural hot reload;
incompatible targets, duplicate/invalid ContentSource slots, missing resources, and recursive template
chains fail before commit.

List templates use `ItemsPresenter` to mark the visual slot for generated items. It is valid only inside
an `ItemsControl` or `ListBox` `ControlTemplate`, at most once per template, and cannot have authored
children. The actual ItemsHost is still created by `ItemsPanelTemplate` and is transferred to the presenter
when the template is instantiated. When the presenter is inside an inner `ScrollView`, that view becomes
the scrolling and virtualization owner. Omitting the presenter keeps item generation alive but leaves the
ItemsHost detached from the visual tree. XAML can therefore replace the complete list appearance while C++
retains selection, keyboard navigation, container generation, and virtualization behavior.

Generated ListBox visuals are hosted by non-authored `ListBoxItem` containers (the C++ compatibility
name remains `SelectorItem`). Each container derives from `ContentControl` and presents the record's
DataTemplate through `ContentPresenter ContentSource="Content"`. Read-only `IsSelected`, `IsMouseOver`,
and `IsKeyboardFocusWithin` states can drive normal Triggers or template VisualStates.
`ItemContainerStyle="{StaticResource ...}"` can set container properties and `Template`; a lexical implicit
`ControlTemplate TargetType="ListBoxItem"` is also supported. A repeatable template factory creates an
independent visual tree for every realized or recycled item. `ListBoxItem` is valid as a Style or
ControlTemplate target, not as a directly authored document node. `ItemsPanelTemplate` selects a StackPanel, WrapPanel, or
VirtualizingStackPanel ItemsHost. The virtualizing host realizes only the visible/cache range and requires
a fixed `ItemHeight` so extent, hit testing, and BringIntoView remain exact; spacing belongs to the panel.
Insert/remove/move operations above a virtual viewport remap its record anchor instead of jumping content.
Wheel and keyboard navigation automatically bring the selected item into view. Container-style
references participate in Designer choices, canonical XAML, clipboard isolation, and transactional reload.

`ComboBox` now uses the same item-container contract. `ItemTemplate` owns data content, while
`ItemContainerStyle` and an explicit or lexical implicit `ControlTemplate TargetType="ComboBoxItem"`
own popup-row appearance. Generated `ComboBoxItem` instances expose the same read-only `IsSelected`,
`IsMouseOver`, and `IsKeyboardFocusWithin` states. A direct `<ComboBoxItem Content="Debug"/>` remains a
compact static-item declaration rather than an authored Designer node. XAML-created ComboBoxes enable real
containers by default; the legacy C++ plain-text path enables them only after an item template, container
style, or container template is configured, preserving the lightweight large-data path. The popup retains
its existing animation, scrolling, hit testing, and selection logic. The selection box still uses projected
text; a later batch can add `SelectionBoxItemTemplate` and popup-item virtualization.

`TreeView` supports both static `TreeView.Items` and typed data hierarchies. Static
`<TreeViewItem Header="...">` entries and data items create real `HeaderedContentControl` containers with
writable `IsExpanded` plus read-only `HasItems`, `Level`, `IsSelected`, `IsMouseOver`, and
`IsKeyboardFocusWithin` states. `ItemContainerStyle` and explicit or lexical implicit
`ControlTemplate TargetType="TreeViewItem"` own appearance, while
`ContentPresenter ContentSource="Header"` presents the header slot. In data mode,
`HierarchicalDataTemplate.ItemsSource` reads the next typed `BindingList` from the current item and template
selection continues by each level's ItemType:

```xml
<HierarchicalDataTemplate DataType="Folder" ItemsSource="{Binding Children}">
  <Label Text="{Binding Name}" />
</HierarchicalDataTemplate>
<DataTemplate DataType="File">
  <Label Text="{Binding Name}" />
</DataTemplate>
<TreeView ItemsSource="{Binding Roots}" SelectedValuePath="Name"
          SelectedItemChanged="OnSelectedItemChanged" />
```

Root-list changes, changes in materialized child lists, and `Children` replacement consume precise
Add/Remove/Replace/Move/Swap notifications while preserving unaffected nodes, selection, and realized
containers by data-object identity; Reset reconciles by the same identity. A collapsed branch observes only
its child source and `HasItems`, materializing its next node level on first expansion or accessibility child
enumeration. Real `TreeViewItem` containers cover only the viewport plus one prefetched row on each side,
and list mutations anchor scrolling to the first visible node rather than its old numeric index. Cycles or
template failures retain the last committed subtree, while complete template/source replacement remains
transactional. `TreeView.Items` and `ItemsSource` are mutually exclusive. `TreeNode` remains the C++
compatibility model, and an unconfigured legacy C++ TreeView keeps its lightweight drawing path. In stable
state, container selection, rendering, and hit testing share one cached flattened visible-node projection:
hits are direct row lookups, while rendering and state refresh touch viewport rows only. Expansion animation
temporarily uses the recursive clipped path and returns to the fast path when complete. A child-list change
replaces only that parent's visible projection segment, and the complete UIA hierarchy index is rebuilt lazily
on an actual accessibility query. A `TreeViewItem` leaving the viewport clears its Header/DataContext into a
bounded recycle pool, so later rows reuse its template chrome rather than constructing another container.
TreeView itself now exposes read-only `SelectedItem` and `SelectedValue`. In data mode, SelectedItem preserves
the `BindingSource` record identity, while `SelectedValuePath` projects a typed path and live-observes its value;
an empty path returns the selected item itself. Static compatibility trees project their corresponding `TreeNode`.
The new default event is the WPF-style `SelectedItemChanged`; legacy `SelectionChanged` is still raised in lockstep
for existing C++ handlers. Pointer input, programmatic `SelectNode`, UIA, and keyboard input share one selection
path. Up/Down/Home/End/PageUp/PageDown navigate the visible projection and scroll it into view, while Right expands
or enters the first child and Left collapses or selects the parent. These read-only projections add no snapshot
field, so XML remains at v29.

`CollectionViewSource` is a reusable declarative projection over a `DataList`, another view, or a
DataContext `IBindingList` supplied through `Source="{Binding Path}"`. AND-combined typed filters and
stable multi-key sorting retain record identity and publish precise Add/Remove/Move changes, preserving
selection, CurrentItem, and unaffected containers. Source chains, paths, operators, and literals are
validated against the projected `DataType` before materialization.
Ordered `GroupDescriptions` build hierarchical contiguous groups. Group headers use the
built-in, reserved `DataType="CollectionViewGroup"`. `GroupStyle.HeaderTemplate` may explicitly
reference a keyed template; when omitted, the declaration scope selects an implicit template
of that type. Its context exposes `Key` / `Name`, `PropertyName`,
`Level`, `StartIndex`, `ItemCount`, `IsBottomLevel`, `FirstItem`, a typed `Items` snapshot, and `Aggregates`;
`FirstItem.*` paths are validated against the source list `DataType`. Headers wrap group-boundary items
without replacing the underlying `ListBoxItem`, preserving selection and container identity. Dynamic
sources and recycled containers also normalize their group wrapper instead of nesting it. Named
`AggregateDescriptions` provide `Count`, `Sum`, `Average`, `Min`, and `Max` as `Aggregates.Name`; their
paths are typed and live-observed. Grouped `VirtualizingStackPanel` uses a segment-offset index covering
items and multi-level headers, so extent, visible ranges, anchor remapping, and BringIntoView remain exact.
`GroupStyle.HeaderHeight` is the enforced header extent in virtual mode.

```xml
<DataTemplate DataType="CollectionViewGroup">
  <StackPanel Orientation="Horizontal" Spacing="6">
    <Label Text="{Binding Key}" />
    <Label Text="{Binding ItemCount}" />
    <Label Text="{Binding FirstItem.Name}" />
    <Label Text="{Binding Aggregates.TotalScore}" />
  </StackPanel>
</DataTemplate>
```

With the Form selected, the property panel exposes a structured Data Resources editor
for local `DataType` fields, `DataList` / `DataRecord`, and `DataTemplate` identities.
Renames transactionally rewrite schema item types, record paths, template bindings, and
StaticResource references; referenced resources cannot be deleted. Merged-dictionary
resources are read-only and must be edited in their source file. New templates receive a
minimal bound Label root; richer visual trees continue through the canonical XAML/canvas path.
Implicit DataTemplates have no string key and are currently edited directly in XAML.

High-performance native areas (3D views, vector editors, media/compute surfaces)
use the built-in `NativeSurface`. XAML contains only a stable `BehaviorKey`; the
application supplies an `INativeSurfaceBehavior` through
`NativeSurfaceBehaviorRegistry`. Runtime loading is strict, while tooling may
render a placeholder without loading user code:

```xml
<NativeSurface x:Name="scene" BehaviorKey="Scene3D"
               PlaceholderText="3D scene" Width="640" Height="360" />
```

The former external-control manifest, preview-plugin ABI, `d:CppType` metadata,
and runtime custom-control factory APIs were removed. Canonical XAML is the
product contract; generated C++ UI construction is no longer a framework
dependency.

Dynamic hosts can safely call `Reload(...)`, `ReloadXaml(...)`, or `ReloadFile(...)`.
Common scalar/metadata properties, Binding and DataContext schema, document styles,
control events, and form presentation return `RuntimeDocumentReloadMode::InPlace`.
The loader first materializes a complete candidate for validation, then retains every
control instance by stable `DesignId` and transactionally commits property sources,
bindings, styles, and event connections; a failure restores the previous state. Omitted
DataContext and resolver options inherit the current runtime attachments. For topology
or container `Extra` changes, the loader builds a candidate tree and transplants maximal
`DesignId` subtrees whose payload and internal topology are unchanged. It returns
`RuntimeDocumentReloadMode::Recomposed`, so add/remove/reorder operations and parent
replacement retain unrelated control instances. Binding, event, or style failure rolls
back both ownership and runtime attachments. With no reusable subtree, font ownership,
unknown property bags, and a persisted property occupied by an active Binding still
conservatively require `Replaced`. `TransferRootControlsTo(form)` retains a transactional
Form-root adapter: reload detaches the old forest from its recorded slots, commits the
candidate at the same anchor, and restores the exact old slots if materialization,
Binding, events, styles, or host commit fails. Host-owned roots outside the document are
left intact. Custom hosts can implement the `RuntimeDocumentRootHost`
Detach/Replacement/Rollback contract. The legacy `ReleaseRootControls()` remains the
fully manual path; without an adapter, required recomposition or replacement fails
explicitly instead of guessing the host structure.

```cpp
DesignerModel::RuntimeDocumentReloadMode mode;
if (!DesignerModel::RuntimeDocumentLoader::ReloadXaml(
        updatedXaml, document, {}, &mode, &error)) {
    // Existing instances, connections, and DataContext remain active.
}

// O(1), typed stable-ID reference; Get() resolves a replacement after reload.
auto saveButton = document.ReferenceByDesignId<Button>(42);
if (auto* button = saveButton.Get()) button->Text = L"Save";
```

Runtime attachments are non-owning. After `ApplyFormProperties(form)`,
`BindFormEvents(form, ...)`, or `TransferRootControlsTo(form)`, the `Form` must outlive
the `RuntimeDocument` (normally declare the Form first). Reload commits candidate Form
presentation, Form-event connections, and the root forest as one transaction. Resolver
or host rejection preserves the old presentation/font semantics, connections, and root slots.

`FindControlByDesignId` and `FindControlByName` use document-owned O(1) indexes.
`RuntimeControlRef<T>` owns neither the control nor the document. It resolves its stable
ID through a weak document-lifetime state on every access, so it follows `InPlace`,
`Recomposed`, and `Replaced` reloads; after document destruction `Get()` safely returns
null instead of touching a dangling address. Move construction transfers that state to the
new document. Loading, reloading, or move-assigning into an existing destination preserves
references issued by that destination, while references issued by the assignment source expire.
`RuntimeDocument::Reference()` exposes the same state as a storable `RuntimeDocumentRef`;
its typed find/reference operations also return null after the document expires.

For a lower-level host that composes monitoring itself, use the threadless
`RuntimeDocumentFileWatcher`. The host calls
`Poll()` from a UI timer. File identity, write time, and size detect direct writes and
atomic replacement; a format-aware `ReloadFile` runs only after the signature remains
stable for the debounce interval. A failed stable signature is not executed on every
tick; a new file signature recovers automatically, or the host can call `RequestRetry()`:

```cpp
DesignerModel::RuntimeDocumentFileWatcher watcher{std::chrono::milliseconds{150}};
if (!watcher.Start(L"MainWindow.cui.xaml", &error)) return;

// Poll on the same UI thread that creates and operates the controls.
const auto result = watcher.Poll(document);
if (result.State == DesignerModel::RuntimeDocumentWatchState::Failed) {
    ShowReloadError(result.Error); // The previous document remains active.
}
```

The watcher creates no thread, posts no window message, and does not own the
`RuntimeDocument`; the host retains control of scheduling, thread affinity, diagnostics,
and whether a `Recomposed` or `Replaced` result is acceptable.

This is a CUI-oriented XAML dialect rather than the full WPF XAML object system.
Unsupported elements, properties, or markup extensions fail before commit.
`XamlDocumentSerializer` is the parser's canonical counterpart. Scalar properties use
ordinary attributes, Bindings use `{Binding ...}`, single Items collections prefer direct
content, multi-collection controls use public property elements such as `ListView.Columns`
and `GridView.Rows`; brushes, clips, and transforms use the `Control.Foreground`,
`Control.Clip`, and `Control.RenderTransform` object elements. Residual model
data without public syntax fails explicitly instead of creating opaque `d:` bags.
The Designer opens and saves `.cui.xaml` / `.xaml` directly and keeps
the current source format on Save; `.cui.xml` / `.xml` use v29 XML. Version 29 adds
`HierarchicalDataTemplate.ItemsSource`, data-backed TreeView hierarchy, child-list observation, and recursive
template closure. Version 28 adds WPF-style static `TreeViewItem` hierarchy containers, the Header slot, expansion/level/selection states, and complete
Designer/clipboard/reload closure. Version 27 adds the shared item-container contract and WPF-style
`ComboBoxItem` item templates, container styles/templates, and complete
Designer/clipboard/reload closure. Version 26 adds WPF-style `ListBoxItem` container templates, read-only
interaction states, and lexical/clipboard/reload closure.
Version 25 adds `ItemsControl` / `ListBox` templates and the WPF-style `ItemsPresenter` ItemsHost slot. Version 24 adds WPF-style
`ContentPresenter.ContentSource` Content/Header slots inside ControlTemplate. Version 23 adds declarative
component-QName `ControlTemplate.TargetType`, `Style.Template`, and the unified template precedence.
Version 22 adds ControlTemplate resources, implicit type keys, TemplateBinding/VisualState appearance instances,
and local-resource snapshots. Version 21 adds the
ContentControl default-content host and scalar/object Content semantics. Version 20 adds named DataType
contracts for BindingSource properties and the ContentPresenter/ContentTemplate single-object
template path. Version 19 adds DataType-keyed
implicit DataTemplates, automatic item/group-header selection, and clipboard closure. Version 18 adds lexical
control-local `ItemsPanelTemplate` / `GroupStyle` resources, declaration-scoped group headers,
and their local-object snapshots. Version 17 adds lexical control-local `DataTemplate` /
`ComponentDefinition` resources and nested template-object snapshots.
Version 16 adds lexical control-local styles and preserves local style rules inside template nodes; version 15 adds
control-local value-resource dictionaries. Version 14 adds group aggregates and
virtual header metrics; version 13 adds grouping and
`GroupStyle`; v12 adds `CollectionViewSource`; v11 adds `ItemsPanelTemplate`; v10 adds `DataList` / `DataRecord`; v9 adds typed `DataType` and `DataTemplate` resources. Earlier
versions remain readable and are upgraded on the next save. Both use atomic replacement
and the same materialization/code-generation path. The explicit
Reload command runs the existing save/discard/cancel flow for dirty documents and keeps
the current canvas if loading fails. `LoadXamlFile(...)` remains the runtime file entry.

The toolbar's XAML action opens an intentionally simple source editor for the complete document.
Input is parsed after a 300 ms debounce and valid text previews immediately on the main Canvas;
syntax or model errors retain the last valid preview and expose precise diagnostic navigation.
Restore Valid Version discards the current draft, `Ctrl+Enter` accepts, and `Escape` or closing
the dialog cancels and restores the pre-editor Canvas. The dialog does not implement completion,
syntax coloring, find/replace, formatting, tag matching, or multiple checkpoints; a future
Visual Studio/COM host can layer those services over the same parser, diagnostics, and transaction API.

`CuiRuntime/CuiRuntime.vcxproj` packages this dynamic path as a standalone static
library; applications do not link the Designer executable. `CuiRuntimeSample` is a
buildable minimal host covering XAML/XML round-trip, NativeSurface behavior registration, nested Grid/Tab/Split content,
stable lookup, property/Binding/style/event in-place transactions, replacement
boundaries, topology subtree recomposition and rollback, root ownership transfer, and
debounced file watching plus the `RuntimeDocumentSession` UI-thread lifecycle.
Applications include only
`CuiRuntime/include/CuiRuntime.h`; the Designer itself now references the same
`CuiRuntime.lib` instead of compiling a second copy of the runtime implementation:

```powershell
msbuild CuiRuntimeSample\CuiRuntimeSample.vcxproj /m /p:Configuration=Debug /p:Platform=x64
.\CuiRuntimeSample\x64\Debug\CuiRuntimeSample.exe
```

`CUITest` has migrated all eight pages that were previously constructed manually in
`DemoWindow.cpp` to the external `DemoWindow.cui.xaml`. XAML owns the control tree,
layout, resources, styles, and named events. The reduced C++ host retains collection
data, chart series, HTML/media content, system services, and business handlers.
XAML components own reusable visual types, while named events are routed to member
functions through `RuntimeEventHandlerRegistry`. Native-only regions attach application
behaviors to the built-in `NativeSurface` instead of registering a new XAML control type. This makes
the application a direct comparison between hand-built C++ and dynamic XAML rather
than a parser-only sample. The build copies the XAML beside the executable and exposes
two non-interactive gates:

```powershell
.\x64\Debug\CUITest.exe --validate-xaml
.\x64\Debug\CUITest.exe --smoke-xaml
```

The first validates parsing, component/event contracts, and NativeSurface behavior keys. The second
also materializes the complete form and generated data-template visuals without starting platform
services. Both return zero
on success.

`CuiStaticGeneratedSample` adds the Designer's namespaced `x:Class` and built-in control output to the solution as
real `.g.h/.g.cpp` and user `.h/.cpp` translation units, then runs it. The generated base exposes
const and non-const typed accessors for every `x:Name` (for example `GetNamespaceButton()`) and
publishes the matching stable IDs through `ControlIds`, so application code does not scan
`Form::Controls` or use `dynamic_cast`. Normalized C++ member names are globally uniqued and
their pointers are null-initialized. `CUICoreTests` also compares all five checked-in code files
with fresh generator output (normalizing line endings), so a compiling fixture cannot silently
drift away from the generator.

```powershell
msbuild CuiStaticGeneratedSample\CuiStaticGeneratedSample.vcxproj /m /p:Configuration=Debug /p:Platform=x64
.\CuiStaticGeneratedSample\x64\Debug\CuiStaticGeneratedSample.exe
```

The default materialization factory creates production controls, including a real
`WebBrowser`. Only `DesignerCanvas` explicitly injects the lightweight preview factory,
so the Designer still avoids WebView initialization while dynamic hosts never receive a
`FakeWebBrowser`.

The designed form no longer has a duplicate `DesignedFormSnapshot` plus separate text
and Boolean update switches. Its persisted `DesignFormModel` is now the single state
model used by the property panel, undo/redo, XML, and code-generation input. A typed
catalog describes all 21 form properties, including category, order, numeric bounds,
and defaults, and centralizes coercion for size, title height, and font size. The
property panel exposes a per-property “↺” action for form values and control metadata
with defaults. Reset participates in undo and, for controls, clears the Local layer to
reveal the next Style, Binding, Theme, or default value. An explicit font size now also
round-trips when the form continues to use the default font family.

`StackPanel` orientation, spacing, and content alignment; `WrapPanel` orientation, item width,
and item height; `DockPanel` last-child fill; and `SplitContainer` orientation, splitter
geometry, panel minimums, fixed state, and splitter appearance now use this generic path end
to end. Interactive splitter dragging records the new distance through the same metadata.
New documents write only `props.metadata`, and neither the property panel nor generated C++
keeps container-specific branches. Legacy `Extra` fields remain readable and are promoted
when no typed metadata value exists; typed metadata wins when both formats are present.

`Slider` and `NumericUpDown` range, step, snapping, input behavior, and control-specific
appearance now share the same contract. Changing `Min` re-coerces `Max` and `Value`, while
interactive `Value` updates preserve an active Binding and still publish one consistent
notification when range coercion changes the value. The Designer restores and generates
dependent properties in metadata order instead of name order; legacy `Extra` remains a
read-only upgrade path.

`GroupBox` caption spacing, radius, and colors, together with `Expander` header geometry,
expanded state, animation duration, and control-specific appearance, now use the same path
end to end. Runtime metadata coerces negative or non-finite geometry consistently. Mouse,
keyboard, and `Toggle()` expansion update the current value so an active TwoWay Binding is
not replaced by a Local value. New documents and generated code use only `props.metadata` /
`TrySetPropertyValue(...)`; legacy `Extra` fields are promoted only when typed metadata is absent.

`ScrollView` content size, scrollbar visibility and thickness, wheel step, border, and scrollbar
colors now use the same contract. `ContentSize` is edited and persisted as a strongly typed Size,
while metadata coerces sizes and thicknesses to nonnegative values. Scroll offsets remain observable,
bindable transient runtime state, so they are not written to `props.metadata` or generated code.
Legacy configuration fields are promoted to metadata, while old offsets are read only for load compatibility.

`Panel` border thickness, corner radius, and disabled overlay are now shared metadata properties for
all containers. `ToolBar`, `StatusBar`, `PagedGridView`, `Expander`, and `ScrollView` no longer declare
same-named raw fields; they use Panel's single backing store. A derived type that needs a different
corner-radius default overrides only its metadata default, so base references, derived code,
Theme/Style/Binding, Designer, and rendering always observe the same state.

`ToolBar` and `StatusBar` specialized layout, behavior, and appearance settings now use metadata end
to end. Their former integer `Padding`, which hid `Control::Padding(Thickness)`, is now the explicit
`HorizontalPadding`; both properties can be edited and generated without a type collision. Automatic
ToolBar items follow `ItemHeight`, while StatusBar `TopMost`, part spacing/radius, colors, and display
switches support Theme, Style, and Binding. Legacy `padding`, `gap`, `itemHeight`, and `topMost` XML
fields are promoted to metadata on load; the StatusBar parts collection remains structurally persisted.

`Control::Children` is now a vector-readable, observable owning collection. Direct insert/erase,
Replace, Move, Swap, and batched mutations synchronize Parent/ParentForm, inherited styles, Form
interaction references, layout, and accessibility before public observers run. Null, duplicate,
cross-parent, and cyclic additions are rejected and rolled back. `InsertOwned()`, `DetachControlAt()`,
`DeleteControlAt()`, and `ClearControls()` express ownership explicitly; direct erase/clear only detach.

`TabControl` selected index, title position, animation mode and duration, title geometry, scrolling
behavior, and all control-specific colors now use the same metadata contract. `TitleWidth`,
`TitleHeight`, and title scrolling are floating-point DIPs. Mouse, keyboard, drag, and `SelectPage()`
update the current value so active bindings remain intact. `TitleScrollOffset` is observable and
TwoWay-bindable but remains transient runtime state, so it is absent from the ordinary property panel
and generated code. Pages remain structurally persisted; legacy selected-index, title geometry,
position, and animation fields are promoted only when matching typed metadata is absent. Ownership-safe
`InsertPage`, `DetachPageAt`, `RemovePage`, and `ClearPages` APIs preserve selection by page identity and
synchronize TwoWay `SelectedIndex`, transitions, and native child windows. `Pages` now directly
projects the observable Children collection.

`Menu` top-level items and `ContextMenu` now expose symmetric insert, detach, remove, and clear APIs.
`MenuItem::SubItems` is a vector-readable `ObservableCollection`: moves, swaps, and batched updates
publish structural changes, while the safe APIs use `unique_ptr` for explicit ownership transfer.
Changing a menu tree closes stale hover/open paths before their indices can target a different item.

`ComboBox` selected index, visible-item count, animation duration, dropdown geometry, and all
control-specific colors now use metadata as well. Mouse, keyboard, `SelectItem()`,
`SetExpanded()`, and `ScrollBy()` update the current value, preserving active TwoWay bindings.
`Expand` and `ExpandScroll` remain observable and bindable but are transient runtime state, so
they are omitted from design files and generated code. Items keep their structural persistence and
now use a vector-compatible `ObservableCollection`: direct insert/remove/move/swap operations publish
precise changes, keep selection and virtual IDs attached to logical items, and batch to one Reset.
Selection and scrolling are still re-coerced when the collection arrives after Binding or metadata.
Legacy `expandCount` / `selectedIndex` fields are promoted only when typed metadata is absent;
generated C++ assigns Items through a valid `std::vector<std::wstring>` and emits no ComboBox-only
raw scalar assignments.

`ListView` view and selection modes, header/check-box options, geometry, wheel step,
and all specialized colors use its metadata contract. `SelectedIndex`, focus/hover
indices, and `ScrollYOffset` are observable, TwoWay-bindable transient interaction state. Single,
Ctrl-multiple, range selection, and scrolling update the current value without replacing an active
Binding. Columns and Items remain structurally persisted and are observable, so direct structural
mutation synchronizes selection, focus, scrolling, stable UIA IDs, structure events, and rendering.
`SetItems()` restores multiple selected
flags in one operation, and generated code applies configuration metadata before the collection.
Legacy List scalars are promoted only when matching metadata is absent. `FullRowSelect` and
`HideSelectionWhenLostFocus` now affect rendering. `ListBox` is no longer a derived virtual-row shell;
it wraps template visuals in `ListBoxItem` and uses `OnSelectionChanged` as its default event.
Large edits can use nested `BeginUpdate()` / `EndUpdate()` or `DeferUpdates()`. Stable identities,
selection, and positions advance incrementally, while public Items/Columns observers each receive one
Reset and scroll correction, UIA notification, and redraw are finalized once. Tail appends touch only
the new identity/selection entries; `LastAccessibilityIndexUpdateWork()` and
`LastSelectionUpdateWork()` provide deterministic complexity diagnostics. After directly changing the
public `ListViewItem::Selected` field, call `Items.NotifyReset()` to reconcile the selection cache from
Items as the source of truth.

`GridView::Rows` and `Columns` are observable collections as well. Direct add/remove/move/swap/sort
operations preserve selected row and column identity by stable ID and move every row's cells with the
logical column. Nested `DeferUpdates()` batches collection notifications, scroll correction, and
rendering; column alignment is complete before public row notifications are delivered.

`PagedGridView::Rows` and its projected `Columns` are observable too. Direct column add/remove/move,
swap, or batched reset realigns cells on every page, including off-screen rows, by stable column ID;
public notifications run only after the current Grid and the master source agree. `PropertyGridView::Items`
is observable as well, preserving logical selection, active editor, Binding, category state, and scrolling
through direct insert/remove/move/swap/sort and batched reset.

`WebBrowser` now has one public PImpl ABI independent of `CUI_ENABLE_WEBVIEW2`; WebView2, COM,
DirectComposition, and event-token types stay out of public headers. `InitialUrl`, `ZoomFactor`,
default context menus, the status bar, and zoom controls use the shared metadata path for Theme,
Style, Binding, Designer persistence, and generated C++. `TryInitialize()` plus per-stage HRESULT
accessors expose initialization failures, while `TryNavigate()`, `TrySetHtml()`, reload, stop, and
history operations return explicit results. URL and HTML requests made before readiness share one
last-write-wins pending slot, and all asynchronous environment, controller, event, and script
callbacks are protected by a lifetime token.

`NotifyIcon` now uses Unicode end to end for tray data, tooltips, balloons, and recursive menus;
legacy narrow overloads decode UTF-8 first. Show/hide, notification, and menu mutations expose Try
results plus HRESULT diagnostics. Right-click menus open automatically, multiple icons are dispatched
by window/message/ID, visible icons recover after Explorer restarts, and temporary HMENU handles are
never shallow-copied as ownership. `Taskbar` now owns one RAII `ITaskbarList3` per instance and exposes
diagnosable value, Normal, Paused, Error, Indeterminate, and Clear operations without shared-COM
double-release risks.

Keyboard focus now follows one `IsTabStop` / `TabIndex` contract. `Form` provides wrapping
Tab/Shift+Tab traversal, access keys, and default/cancel buttons, while `Button`, `LinkLabel`,
`CheckBox`, `RadioBox`, and `Switch` share a programmable `Invoke()` action. Accessible name,
description, help text, AutomationId, role, shortcut, and focus visuals are property metadata and
therefore participate in Binding, Style, Designer persistence, and code generation. Each Form
answers `WM_GETOBJECT` with a lifetime-safe native UI Automation fragment tree and exposes Invoke,
Toggle, Value, RangeValue, ExpandCollapse, SelectionItem, and Selection patterns for core controls.
The compatible `IAccessible` client object and WinEvents remain available. Password content is never
exposed as a name or value, and retained providers fail safely after their window is destroyed.
ListView items, ComboBox items, TreeNode objects, and GridView headers, rows, and cells are
also exposed as stable virtual fragments with the corresponding Selection, Toggle, ExpandCollapse,
Grid/Table, Value, Invoke, VirtualizedItem, and ScrollItem patterns. Retained virtual providers fail
safely after their logical item is removed.
ListView, ComboBox, TreeView, and GridView containers also expose native Scroll Pattern
metrics and actions derived from their current viewport and scroll range; unsupported axes report
NoScroll. ListView Details mode additionally exposes stable column-header, row, and cell fragments,
row/column Grid addressing, and TableItem header relationships.
Native first/last-child, sibling, and hit-test navigation now uses indexed ID-based fast paths rather
than copying a complete child collection or recursively scanning the virtual tree for each request.
Built-in virtual controls rebuild stable indexes on structural mutation; ListView Details and GridView
cell IDs are created on demand and only materialized invalid identities are pruned, avoiding a
rows-by-columns UIA reverse-index allocation for large tables. Both expose
`MaterializedAccessibilityCellCount()` for deterministic cache-size diagnostics.
ListView drawing and icon-mode hit testing now use a shared `[start, end)` visible index range.
`GetVisibleItemRange()` also exposes that range for work such as deferred image loading, so per-frame
drawing scales with visible items instead of scanning the complete Items collection.
These virtual collections are now driven by `ObservableCollection`, so direct structural mutation no
longer waits for the next provider query to reconcile identity. TreeNode also exposes `AddChild`,
`DetachChildAt`, `RemoveChild`, and `ClearChildren` for explicit nested-node ownership.

`Form` also responds automatically to Windows high-contrast, client-animation, text-scale, and
keyboard-focus-cue settings. Common surfaces, foregrounds, and focus colors use high-contrast system
colors; common control animations complete immediately when motion is disabled; inherited and
explicit fonts follow the text scale. `Application::QuerySystemVisualPreferences()` returns a
snapshot, while `Form::ApplySystemVisualPreferences(...)` supports deterministic test injection.

`ObservableObject::SetValue` automatically records each source property's name, stable value type, and default read/write/notification capabilities. Explicit declarations support read-only or silent properties, and runtime binding rejects incompatible modes using that metadata:

```cpp
auto viewModel = std::make_shared<ObservableObject>();
viewModel->DefineProperty(
    L"Status", std::wstring(L"Ready"),
    true,   // CanRead
    false,  // CanWrite
    true);  // CanObserve
```

`ObservableObject` also supports field-level and object-level validation state.
Derived view models publish info, warnings, and errors through the protected
`SetValidationIssues` / `SetValidationError` methods. A binding observes every
level of its dotted path, and the target control aggregates results through
`DataBindings`:

```cpp
class ViewModel final : public ObservableObject
{
public:
    void SetName(std::wstring value)
    {
        SetValue(L"Name", value);
        SetValidationError(L"Name",
            value.empty() ? L"Name is required." : L"",
            L"required");
    }
};

auto results = textBox->DataBindings.GetValidationResults();
bool hasErrors = textBox->DataBindings.HasValidationErrors();
```

Controls present these results consistently: a theme-colored border reflects the
highest active severity, and hover shows a summary of up to three issues. Configure
this with `ShowValidationBorder`, `ShowValidationToolTip`,
`ValidationBorderThickness`, `ValidationCornerRadius`, and
`ValidationToolTipMaxWidth`. `FormThemeFrame` supplies Info/Warning/Error and popup
colors. `AccessibleDescription` stores the control's own description, while
`GetEffectiveAccessibleDescription()` combines it with active validation text for a
host accessibility adapter.

Validation notifications use RAII connections returned by
`BindingValidationChangedEvent::Subscribe(...)`. Replacing an intermediate
object disconnects the old validation source and attaches the new one; a
destroyed source does not leave stale validation results visible.
`DataSourceUpdateMode::OnValidation` still means “write on focus loss” for text
controls and is independent from source-side validation state.

An omitted update policy is now stored as `DataSourceUpdateMode::Default` and resolved
from `BindingPropertyMetadata::DefaultUpdateMode()` when the Binding is installed.
Built-in `TextBox.Text` defaults to `TwoWay + LostFocus`; use
`UpdateSourceTrigger=PropertyChanged` for per-change writes or `Explicit` for manual
source updates. Legacy `UpdateMode=OnPropertyChanged/OnValidation/Never` input remains
accepted, while canonical XAML emits the WPF spellings and omits `Default`.
Both `Binding` and `MultiBinding` expose `UpdateSource()` / `UpdateTarget()`; callers
can also use `control->DataBindings.UpdateSource(L"Text")` without branching on the
expression kind. A manual MultiBinding commit runs the top-level `ConvertBack` and
then commits its internal `Explicit` child bindings. Conversion or source-write
failures return `false`, and the same expression can be retried after correcting the value.

The Designer property panel provides an Edit Data Bindings command. Its structured editor lists target properties from the selected control's metadata and filters binding modes and update modes using each property's read, write, and change-notification capabilities. Source paths support `Profile.Name`, `People[0].Name`, and `Settings[key]`. The editor can select the built-in `BooleanNegation`, `StringIsNotEmpty`, and `StringTrim` converters or persist an application-defined converter ID, and it edits `ConverterParameter`, String-target `StringFormat`, `FallbackValue`, and `TargetNullValue` independently. When a host supplies a design-time data source, the Designer materializes persisted configurations as real runtime bindings. It snapshots and clears masking Local values before attach, then restores them when the context is removed, the configuration changes, or attach fails. Rows expose attach errors and active source validation; this transient state is not persisted. Validation-presentation options and `AccessibleDescription` are editable as regular properties and persist into the design document and generated code. Bindings are stored in the XML design document, and generated forms with bindings expose `BindData(IBindingSource& dataContext)`. Generated attach code applies the same Local snapshot/clear/failure-restore rule so initialization cannot permanently mask a binding.

`StringFormat` runs after conversion and supports `{0}`, alignment, escaped braces,
and common invariant numeric formats. For example:
`Text="{Binding Amount, Converter=Application.Scale, ConverterParameter='100', StringFormat='{}{0:N2}', FallbackValue='--'}"`.
Formatting is target-display-only; TwoWay source updates remain the converter's
`ConvertBack` responsibility.

Multiple sources use WPF-style `MultiBinding` property elements. Every child
Binding retains the complete PropertyPath, ElementName/RelativeSource,
converter, fallback, and dynamic re-subscription behavior. The top level can
use an indexed `StringFormat="{}{0} / {1}"` directly or a registered
`IMultiBindingValueConverter`; writable modes split the target value through
the multi-value converter's `ConvertBack`. Dynamic XAML, DataTemplate,
component templates, and Designer preview share this model. It is currently
authored in the simplified XAML editor and is intentionally outside static C++
auxiliary generation.

When no control is selected, the form property panel provides an Edit DataContext Schema command. The schema declares dotted source paths together with their value kinds and read, write, and change-notification capabilities. Once defined, the binding editor offers discoverable source-path choices and validates source capabilities plus both sides of converter metadata. An embedded Designer host can call `Designer::SetDesignDataContext(...)` and recursively import metadata from the real view model; cyclic object graphs are truncated safely. Documents without a schema retain free-form source paths.

The current design document format is version 8. Every control persists an `id` that survives renames and reordering, an optional `parentId` for ordinary control parents, and the document persists a `nextId` high-water mark so deleted IDs are not reused. Version 8 adds declarative enum choices, structured component defaults, and component default-resource references; versions 7 and 6 introduced component templates and component contracts. Optional code-behind metadata stores a validated C++ class identity and an extensionless path relative to the design file, never an absolute workstation path. Older documents remain readable, receive missing state in memory, and are upgraded on the next save. Runtime controls expose `DesignId` and `FindControlByDesignId(...)`, giving dynamic XAML loading a stable lookup contract.

Register custom converters before calling a generated form's `BindData`. The metadata lets both the runtime and design tools reason about the target value kind and reverse-conversion support:

```cpp
BindingValueConverterRegistry::Register(
    { L"Application.Trim", BindingValueKind::String,
      BindingValueKind::String, true },
    []
    {
        return std::make_shared<MyTrimConverter>();
    });
```

## Screenshots

### Designer

The visual designer supports drag-and-drop layout editing, property inspection, and C++ code generation.

![CUI Designer](imgs/Designer.png)

### Demo Window and Menus

The sample application includes a main window menu, a standalone context menu, and multiple TabControl demo pages.

| Window Menu | Context Menu |
| --- | --- |
| ![Window Menu](imgs/Menu.png) | ![Context Menu](imgs/ContexMenu.png) |

### TabControl Pages

The following screenshots correspond to different pages selected in the TabControl of the demo window:

| Tab 1 | Tab 2 |
| --- | --- |
| ![Tab 1](imgs/Tab1.png) | ![Tab 2](imgs/Tab2.png) |

| Tab 3 | Tab 4 |
| --- | --- |
| ![Tab 3](imgs/Tab3.png) | ![Tab 4](imgs/Tab4.png) |

| Tab 5 | Tab6 |
| --- | --- |
| ![Tab 5](imgs/Tab5.png) | ![Tab 6](imgs/Tab6.png) |

| WebBrowser |
| --- | --- |
| ![WebBrowser](imgs/WebBrowser.png) |

### Media Page

The MediaPlayer page demonstrates the built-in media playback control.

![MediaPlayer](imgs/MediaPlayer.png)

## Notes

- **Windows only**: relies on Direct2D/DirectWrite/DirectComposition.
- **Windows version**: `CUI` supports Windows 7+. Use the preprocessor macro `CUI_ENABLE_WEBVIEW2` to enable DirectComposition + WebView2 (requires Windows 8+); without it, only Direct2D HWND rendering is used, maintaining Windows 7 compatibility.
- **Project dependencies**:
  - `CUI` depends on `D2DGraphics`
  - `CUITest` now carries the small helper code it previously consumed from `Utils`, so it no longer depends on `Utils`
  - `CuiDesigner` currently depends on `CUI` and `Utils`
- **Third-party dependencies**: WebView2; the graphics and utility source used by this repo is already included locally
- **Designer output**: the designer saves XML or CUI XAML by extension and generates C++ code; keep `.cui.xml` / `.cui.xaml` under version control as the long-term UI source.

## Community

- QQ group: 522222570

License: AFL 3.0 (see `LICENSE`).
