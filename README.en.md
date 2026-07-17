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

Properties owned by the Designer wrapper rather than runtime metadata—Name, Anchor,
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
default event per control type. Double-clicking either an event row or a control on the
canvas reuses an existing handler or writes the conventional default through the normal
undoable transaction; double-clicking the Form client surface activates its one-shot
`OnShown` event. After one explicit code export, activation also safely regenerates `.g.*`,
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
class/state selectors, and WPF-like `x:Key` plus `Style="{StaticResource ...}"`.
Runtime property metadata remains authoritative, so a newly exposed generic property
does not need a dedicated XAML setter.

```cpp
const std::string_view xaml = R"(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="MainForm" Text="CUI XAML" Width="480" Height="240">
  <Form.Resources>
    <Color x:Key="Accent">#FF0078D4</Color>
    <Style x:Key="PrimaryButton" TargetType="Button" Class="primary">
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

External controls use a prefixed element plus portable design/code-generation
metadata. `d:BaseType` selects the built-in CUI base used for Designer preview,
property/event metadata, layout, and headless generation. `d:CppType`, `d:Header`,
and `d:Constructor` select the static C++ output. Constructor conventions are
`Default`, `Bounds(x, y, width, height)`, and
`TextBounds(text, x, y, width, height)`. Canonical XAML and v5 XML preserve the
complete descriptor:

```xml
<Form xmlns="urn:cui" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer" xmlns:controls="urn:acme:controls">
  <controls:StatusBadge x:Name="statusBadge"
      d:CppType="Acme.Controls.StatusBadge"
      d:Header="Controls/StatusBadge.h"
      d:BaseType="Button" d:Constructor="Bounds"
      Text="Ready" Width="120" Height="30" />
</Form>
```

A dynamic host must register the real factory explicitly. The returned control
must inherit and retain the declared `d:BaseType` from `Type()`. A missing
registration fails transactionally instead of silently replacing the control.
The Designer and `CuiCodeGen` opt into a built-in-base proxy, so they can preview
generic metadata and emit the typed member, include, and constructor without
loading an application DLL. Custom-only XAML properties still require metadata
from the real control. Dynamic parsing validates and coerces them through the
registered factory. Canonical output keeps tool-unknown values and Bindings in
typed `d:DesignProps` / `d:DesignBindings` bags; the CLI proxy preserves them and
emits deferred setters/bindings. Unknown attributes are never guessed. Reload
inherits the current document's registry when it is omitted, so custom properties
can still update in place.

The Designer ToolBox can register the same portable descriptors from a UTF-8
control manifest without loading application DLLs. Loading is transactional and
strictly validates the schema, instantiable built-in base, XAML identity/prefix
conflicts, canonical C++ type, and safe relative include. See
`CuiStaticGeneratedSample/CuiDesigner.controls.xml` for a complete example:

```xml
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="StatusBadge" displayName="Status badge" category="Samples"
    baseType="Button" xamlPrefix="sample" xamlName="StatusBadge"
    xamlNamespace="urn:cui:samples" cppType="Acme.Controls.StatusBadge"
	header="Controls/StatusBadge.h" constructor="Bounds"
	width="150" height="30" container="false">
    <property name="Severity" displayName="Severity" category="Appearance"
      kind="Int64" default="1" editor="Choice"
      minimum="0" maximum="2" bindable="true" twoWay="false">
      <choice displayName="Normal" value="1" />
      <choice displayName="Warning" value="2" />
    </property>
    <event name="OnSeverityInvoked" displayName="Severity invoked"
      field="OnSeverityInvoked" category="Action"
      signature="SenderInt" order="5" default="true" />
  </control>
</cuiControlCatalog>
```

Launch with `Designer.exe --controls <manifest>` to add the entries to the
ToolBox, or use `Designer.exe --validate-controls <manifest>` in CI for a
validation-only `0/2` exit code. A `property` schema drives typed PropertyGrid
rows, choices/ranges, Reset, Undo/Redo, persistence, and deferred Binding. The
real custom control should still register runtime metadata under the same name.
`twoWay=true` explicitly promises a getter and change notification; the default
portable contract exposes only OneWay/OneTime.

An `event` schema adds the custom event to the PropertyGrid event page and drives
default-event activation, named handlers, Undo/Redo, XAML/XML round-trip, and
static `std::bind_front` generation. `signature` is a fixed safe preset rather
than arbitrary C++ text: `None`, `Sender`, `SenderBool`, `SenderInt`,
`SenderFloat`, `SenderDouble`, `SenderString`, `SenderIntInt`, `SenderIntBool`,
`SenderDoubleDouble`, or `SenderStringString`; sender is always `Control*`.
The contract is persisted in `d:CustomEvents`, so headless generation does not
depend on a locally installed manifest. A dynamic host additionally registers
the real Event member through `RuntimeEventHandlerRegistry::RegisterCustomControl(...)`,
which verifies `Event::function_type`. If an installed manifest changes the
name, field, or signature of an event already in use, the Designer rejects the
load and preserves the current canvas. See `CuiRuntimeSample/main.cpp` for the
complete dynamic example.

Without enhanced preview the canvas uses a `baseType` proxy while persisted
XAML and generated C++ retain the real type. An in-process host can use
`DesignerControlCatalog::AttachPreviewFactory(...)`. A separate trusted DLL is
loaded only through explicit `--preview-plugin <dll>` configuration; CI can run
`--validate-preview-plugin <dll> <xaml-namespace> <xaml-name>`. The host owns the
proxy and the plugin returns bounded value-only drawing primitives, never a
`Control*`. Design files and manifests cannot supply a DLL path. See the
[value-only C ABI](CuiDesigner/CUSTOM_CONTROL_PLUGIN_ABI.md).

```cpp
auto controls = std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
controls->Register(L"urn:acme:controls", L"StatusBadge",
    [](const DesignerModel::DesignNode&) {
        return std::make_unique<Acme::Controls::StatusBadge>();
    }, &error);

DesignerModel::RuntimeDocumentLoadOptions options;
options.CustomControls = controls;
DesignerModel::RuntimeDocumentLoader::LoadXaml(xaml, document, options, &error);
```

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
`XamlDocumentSerializer` is the parser's canonical counterpart. It keeps ordinary
properties readable and stores structured compatibility data that has no direct syntax
in `d:DesignProps` / `d:DesignExtra`, so a save/reload cycle preserves the complete
Designer model. The Designer opens and saves `.cui.xaml` / `.xaml` directly and keeps
the current source format on Save; `.cui.xml` / `.xml` use v5 XML. Version 5 adds optional
code-behind class identity and a relative base path; versions 1–4 remain readable and are
upgraded on the next save. Both use atomic replacement and the same materialization/code-generation path. The explicit
Reload command runs the existing save/discard/cancel flow for dirty documents and keeps
the current canvas if loading fails. `LoadXamlFile(...)` remains the runtime file entry.

`CuiRuntime/CuiRuntime.vcxproj` packages this dynamic path as a standalone static
library; applications do not link the Designer executable. `CuiRuntimeSample` is a
buildable minimal host covering XAML/XML round-trip, registered custom controls, nested Grid/Tab/Split content,
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
Custom controls are registered through `RuntimeCustomControlRegistry`, while named
events are routed to member functions through `RuntimeEventHandlerRegistry`. This makes
the application a direct comparison between hand-built C++ and dynamic XAML rather
than a parser-only sample. The build copies the XAML beside the executable and exposes
two non-interactive gates:

```powershell
.\CUITest\x64\Debug\CUITest.exe --validate-xaml
.\CUITest\x64\Debug\CUITest.exe --smoke-xaml
```

The first validates parsing, event contracts, and custom-control factories. The second
also materializes the complete form and initializes its runtime data. Both return zero
on success.

`CuiStaticGeneratedSample` adds the Designer's namespaced `x:Class` and external custom-control output to the solution as
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

The Designer property panel provides an Edit Data Bindings command. Its structured editor lists target properties from the selected control's metadata and filters binding modes and update modes using each property's read, write, and change-notification capabilities. Source paths support dotted values such as `Profile.Name`. The editor can select the built-in `BooleanNegation`, `StringIsNotEmpty`, and `StringTrim` converters or persist an application-defined converter ID. When a host supplies a design-time data source, the Designer materializes persisted configurations as real runtime bindings. It snapshots and clears masking Local values before attach, then restores them when the context is removed, the configuration changes, or attach fails. Rows expose attach errors and active source validation; this transient state is not persisted. Validation-presentation options and `AccessibleDescription` are editable as regular properties and persist into the design document and generated code. Bindings are stored in the XML design document, and generated forms with bindings expose `BindData(IBindingSource& dataContext)`. Generated attach code applies the same Local snapshot/clear/failure-restore rule so initialization cannot permanently mask a binding.

When no control is selected, the form property panel provides an Edit DataContext Schema command. The schema declares dotted source paths together with their value kinds and read, write, and change-notification capabilities. Once defined, the binding editor offers discoverable source-path choices and validates source capabilities plus both sides of converter metadata. An embedded Designer host can call `Designer::SetDesignDataContext(...)` and recursively import metadata from the real view model; cyclic object graphs are truncated safely. Documents without a schema retain free-form source paths.

The current design document format is version 5. Every control persists an `id` that survives renames and reordering, an optional `parentId` for ordinary control parents, and the document persists a `nextId` high-water mark so deleted IDs are not reused. Optional code-behind metadata stores a validated C++ class identity and an extensionless path relative to the design file, never an absolute workstation path. Name references remain for readability and compatibility cases such as TabPage parents. Version 1–4 documents remain readable, receive missing state in memory, and are upgraded on the next save. Runtime controls expose `DesignId` and `FindControlByDesignId(...)`; generated code assigns the same IDs, giving dynamic XML loading and static generated UI a shared lookup contract.

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
