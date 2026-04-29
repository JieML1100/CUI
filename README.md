# CUI - 现代化 Windows GUI 框架

[简体中文](README.md) | [English](README.en.md) | [完整文档](ReadMeFull.md)

[完整文档(英文)](ReadMeFull.en.md)

一个基于 **Direct2D** 和 **DirectComposition** 的 Windows 原生 GUI 框架（C++20），并提供配套的 **可视化设计器**（拖放设计 + XML 保存/加载 + 自动生成 C++ 代码）。

本仓库主要包含：
- `CUI/`：运行时 GUI 框架与控件库
- `CuiDesigner/`：可视化 UI 设计器
- `CUITest/`：示例与测试程序
- `D2DGraphics/`：底层图形封装
- `Utils/`：设计器等项目仍在使用的通用工具库

## 特点

- **高性能渲染**：Direct2D 硬件加速 + DirectComposition 合成
- **控件与布局**：提供33+常用控件
- **控件与布局**：提供多种布局容器（如 Stack/Grid/Dock/Wrap/Relative 等）
- **事件与输入**：完善的鼠标/键盘/焦点/拖放事件，支持 IME 中文输入
- **资源支持**：内置 SVG 渲染（nanosvg 已包含）
- **多媒体功能 集成**：媒体播放器（MediaPlayer）
- **WebView2 集成**：可嵌入现代 Web 内容（基于 Microsoft WebView2）
- **设计器工作流**：拖放编辑属性、实时预览、XML 设计文件保存/加载、自动生成 C++ 代码

## 界面截图

### 设计器

可视化设计器支持拖放布局、属性编辑和代码生成。

![CUI Designer](imgs/Designer.png)

### Demo 窗口与菜单

示例程序包含主窗口菜单、独立上下文菜单，以及 TabControl 的多个演示页面。

| 主窗口菜单 | 上下文菜单 |
| --- | --- |
| ![Window Menu](imgs/Menu.png) | ![Context Menu](imgs/ContexMenu.png) |

### TabControl 页面截图

以下截图对应 Demo 中选中 TabControl 不同页面时的显示效果：

| Tab 1 | Tab 2 |
| --- | --- |
| ![Tab 1](imgs/Tab1.png) | ![Tab 2](imgs/Tab2.png) |

| Tab 3 | Tab 4 |
| --- | --- |
| ![Tab 3](imgs/Tab3.png) | ![Tab 4](imgs/Tab4.png) |

| Tab 5 | WebBrowser |
| --- | --- |
| ![Tab 5](imgs/Tab5.png) | ![WebBrowser](imgs/WebBrowser.png) |

### 多媒体页面

MediaPlayer 页面演示了框架内置媒体播放控件。

![MediaPlayer](imgs/MediaPlayer.png)

## 注意事项

- **仅支持 Windows**：依赖 Windows 图形栈（Direct2D/DirectWrite/DirectComposition）。
- **Windows版本限制**：`CUI` 支持 Windows 7+。通过预处理器宏 `CUI_ENABLE_WEBVIEW2` 控制是否启用 DirectComposition + WebView2 功能（需要 Windows 8+）；不定义该宏时仅使用 Direct2D HWND 渲染，兼容 Windows 7。
- **项目依赖关系**：
  - `CUI` 依赖 `D2DGraphics`
  - `CUITest` 已内置原先来自 `Utils` 的轻量测试辅助逻辑，不再依赖 `Utils`
  - `CuiDesigner` 当前依赖 `CUI` 和 `Utils`
- **第三方依赖**：WebView2；仓库中的图形/工具源码已直接包含，无需额外引入 `CppUtils/Graphics`
- **设计器输出**：设计器会保存 XML 设计文件并生成 C++ 代码；建议将生成代码纳入版本控制、设计文件作为 UI 源文件长期维护。

## 交流社区
- **QQ群**：522222570

许可证：AFL 3.0，见 `LICENSE`。
