# CUI - Modern Windows GUI Framework

[English](README.en.md) | [简体中文](README.md) | [Full Documentation](ReadMeFull.en.md)

[完整文档(中文)](ReadMeFull.md)

CUI is a modern native Windows GUI framework based on **Direct2D** and **DirectComposition** (C++20). It also comes with a **visual designer** (drag & drop, XML save/load, and automatic C++ code generation).

This repository mainly contains:
- `CUI/`: runtime GUI framework and controls
- `CuiDesigner/`: visual UI designer
- `CUITest/`: samples and test program
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
- **Designer workflow**: property editing, live preview, XML design files, and C++ code generation

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

Effective values use `Local > Binding > Style > Theme > Default` precedence.
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

The same catalog now fills the ordinary property panel with metadata properties that
are not already represented by legacy fields. Edits use runtime conversion and
coercion, then persist the canonical value in the optional typed `props.metadata` bag.
Existing XML fields remain compatible while load, undo/redo, and generated C++ share
the canonical property name and value kind.

`ControlPropertyOptions::Design` can additionally declare browsability, display name,
category and ordering, a preferred editor, strongly typed choices, numeric bounds, and
persistence policy. The ordinary property panel groups these descriptors and selects a
Boolean, choice, color, thickness, size, length, numeric, or text editor automatically.
`Legacy` and `Transient` properties are kept out of the generic metadata bag while
remaining valid Binding and style-setter targets.

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

`ListView` / `ListBox` view and selection modes, header/check-box options, geometry, wheel step,
and all specialized colors now share the same metadata contract. `SelectedIndex`, focus/hover
indices, and `ScrollYOffset` are observable, TwoWay-bindable transient interaction state. Single,
Ctrl-multiple, range selection, and scrolling update the current value without replacing an active
Binding. Columns and Items remain structurally persisted and are observable, so direct structural
mutation synchronizes selection, focus, scrolling, stable UIA IDs, structure events, and rendering.
`SetItems()` restores multiple selected
flags in one operation, and generated code applies configuration metadata before the collection.
Legacy List scalars are promoted only when matching metadata is absent. `FullRowSelect` and
`HideSelectionWhenLostFocus` now affect rendering, while derived ListBox metadata keeps its hidden
column-header default false.

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
ListView/ListBox items, ComboBox items, TreeNode objects, and GridView headers, rows, and cells are
also exposed as stable virtual fragments with the corresponding Selection, Toggle, ExpandCollapse,
Grid/Table, Value, Invoke, VirtualizedItem, and ScrollItem patterns. Retained virtual providers fail
safely after their logical item is removed.
ListView/ListBox, ComboBox, TreeView, and GridView containers also expose native Scroll Pattern
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

The Designer property panel provides an Edit Data Bindings command. Its structured editor lists target properties from the selected control's metadata and filters binding modes and update modes using each property's read, write, and change-notification capabilities. Source paths support dotted values such as `Profile.Name`. The editor can select the built-in `BooleanNegation`, `StringIsNotEmpty`, and `StringTrim` converters or persist an application-defined converter ID. When a host supplies a design-time data source, the editor also previews active runtime validation issues for the selected path; this transient state is not persisted. Validation-presentation options and `AccessibleDescription` are editable as regular properties and persist into the design document and generated code. Bindings are stored in the XML design document, and generated forms with bindings expose `BindData(IBindingSource& dataContext)` so the application supplies the data context explicitly.

When no control is selected, the form property panel provides an Edit DataContext Schema command. The schema declares dotted source paths together with their value kinds and read, write, and change-notification capabilities. Once defined, the binding editor offers discoverable source-path choices and validates source capabilities plus both sides of converter metadata. An embedded Designer host can call `Designer::SetDesignDataContext(...)` and recursively import metadata from the real view model; cyclic object graphs are truncated safely. Documents without a schema retain free-form source paths. The current design document version 3 persists both the schema and document style sheet; version 1 and 2 files remain readable and are upgraded on the next save.

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
- **Designer output**: the designer saves XML and generates C++ code; it’s recommended to version-control generated code and keep the XML design files as the long-term UI source.

## Community

- QQ group: 522222570

License: AFL 3.0 (see `LICENSE`).
