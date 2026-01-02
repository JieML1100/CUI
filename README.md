# CUI - 现代化 Windows GUI 框架

[简体中文](README.md) | [English](README.en.md)

一个基于 **Direct2D** 和 **DirectComposition** 的 Windows 原生 GUI 框架（C++20），并提供配套的 **可视化设计器**（拖放设计 + JSON 保存/加载 + 自动生成 C++ 代码）。

本仓库主要包含：
- `CUI/`：运行时 GUI 框架与控件库
- `CuiDesigner/`：可视化 UI 设计器
- `CUITest/`：示例与测试程序

## 特点

- **高性能渲染**：Direct2D 硬件加速 + DirectComposition 合成
- **控件与布局**：提供25+常用控件
- **控件与布局**：提供多种布局容器（如 Stack/Grid/Dock/Wrap/Relative 等）
- **事件与输入**：完善的鼠标/键盘/焦点/拖放事件，支持 IME 中文输入
- **资源支持**：内置 SVG 渲染（nanosvg 已包含）
- **多媒体功能 集成**：媒体播放器（MediaPlayer）
- **WebView2 集成**：可嵌入现代 Web 内容（基于 Microsoft WebView2）
- **设计器工作流**：拖放编辑属性、实时预览、JSON 设计文件保存/加载、自动生成 C++ 代码

## 注意事项

- **仅支持 Windows**：依赖 Windows 图形栈（Direct2D/DirectWrite/DirectComposition）。
- **Windows版本限制**：CUI仅支持Windows8+,CUI_Legacy支持Windows7(不含WebBrowser)
- **第三方依赖**：WebView2,CppUtils/Graphics  CppUtils/Graphics开源在https://github.com/JieML1100/CppUtils
- **设计器输出**：设计器会保存 JSON 设计文件并生成 C++ 代码；建议将生成代码纳入版本控制、设计文件作为 UI 源文件长期维护。

## 交流社区
- **QQ群**：522222570

许可证：AFL 3.0，见 `LICENSE`。
