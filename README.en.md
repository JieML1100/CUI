# CUI - Modern Windows GUI Framework

[English](README.en.md) | [简体中文](README.md) | [Full Documentation](ReadMeFull.en.md)

[完整文档(中文)](ReadMeFull.md)

CUI is a modern native Windows GUI framework based on **Direct2D** and **DirectComposition** (C++20). It also comes with a **visual designer** (drag & drop, JSON save/load, and automatic C++ code generation).

This repository mainly contains:
- `CUI/`: runtime GUI framework and controls
- `CuiDesigner/`: visual UI designer
- `CUITest/`: samples and test program

## Features

- **High-performance rendering**: Direct2D hardware acceleration + DirectComposition compositor
- **Controls**: 25+ commonly used UI controls
- **Layouts**: multiple layout containers (Stack/Grid/Dock/Wrap/Relative, etc.)
- **Events & input**: mouse/keyboard/focus/drag-drop events, with IME support
- **SVG support**: built-in nanosvg (included)
- **Media playback**: built-in MediaPlayer control
- **WebView2 integration**: embed modern web content via Microsoft WebView2
- **Designer workflow**: property editing, live preview, JSON design files, and C++ code generation

## Notes

- **Windows only**: relies on Direct2D/DirectWrite/DirectComposition.
- **Windows version**: `CUI` supports Windows 8+; `CUI_Legacy` supports Windows 7 (without WebBrowser).
- **Dependencies**:
  - WebView2
  - `CppUtils/Graphics` (open-sourced at https://github.com/JieML1100/CppUtils)
- **Designer output**: the designer saves JSON and generates C++ code; it’s recommended to version-control generated code and keep the JSON design files as the long-term UI source.

## Community

- QQ group: 522222570

License: AFL 3.0 (see `LICENSE`).
