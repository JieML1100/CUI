# 静态框架主题

`CuiGeneratedTheme` 把 [`Themes/Generic.xaml`](../Themes/Generic.xaml) 编译成 Production 可以直接链接的
静态库。XAML 解析只发生在构建阶段；生成的 provider 和 `ThemeProgram` 只依赖 CUI 公共头文件。

生成文件位于 `$(IntDir)CuiGenerated`，不应提交到源码目录。输入主题、代码生成器或 targets 契约变化时，
MSBuild 会使对应 stamp 失效并重新生成。

主题编译有两种模式：

- `Full` 保留完整的共享主题，供 `CuiGeneratedTheme` 使用。
- `Closure` 从根 XAML、显式类型和资源计算传递闭包，供静态应用缩小最终链接集合。

`CuiStaticGeneratedSample` 默认使用 Closure；设置 `CuiFrameworkThemeMode=Full` 可以验证完整主题路径。
两种产物都不能链接 `CuiRuntime`、DesignerModel、XmlLite，也不能携带原始 XAML 字节。

CornerRadius/VisualState 的定向编译 probe 可这样构建：

先按 [根 README](../README.md#构建环境) 中的片段初始化 `$msbuild`，再运行：

```powershell
& $msbuild .\CuiGeneratedTheme\CuiGeneratedTheme.vcxproj /t:Rebuild /m:1 /nr:false `
  /p:Configuration=Debug /p:Platform=x64 `
  /p:CuiGeneratedThemeDirectedTests=true
```
