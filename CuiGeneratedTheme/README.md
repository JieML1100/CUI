# Static framework theme

`CuiGeneratedTheme` is the runtime half of the `Themes/Generic.xaml`
compilation boundary.

- `CuiCodeGen compile-theme` parses and validates XAML only during the build.
- The generated provider and template factories compile into this static
  library and include only CUI headers.
- `CuiRuntime`, `DesignerModel`, `XmlLite`, and the original XAML bytes are not
  linked into this library.

The build-owned files live under `$(IntDir)CuiGenerated` and are regenerated
incrementally when `Generic.xaml`, the generator, or its target contract
changes.
