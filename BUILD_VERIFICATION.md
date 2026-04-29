# CUI 构建与回归验证清单

## 构建矩阵

使用 Visual Studio 2022 的 MSBuild 验证解决方案：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Release /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Debug /p:Platform=x86 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' CUI.sln /m /t:Build /p:Configuration=Release /p:Platform=x86 /v:minimal
```

说明：解决方案平台使用 `x86`，项目内部映射到 `Win32`。

## 设计器主链路

每次修改设计器、XML 序列化、代码生成或控件布局后，至少手工走一遍：

1. 启动 `Designer.exe`。
2. 拖放几个基础控件和容器控件，例如 `Button`、`TextBox`、`GridPanel`、`SplitContainer`、`TabControl`。
3. 修改属性，包括 `Name`、`Text`、位置、尺寸、颜色、`Visible`、事件开关。
4. 验证 `Ctrl+Z`、`Ctrl+Y`、`Ctrl+Shift+Z` 能撤销/重做属性修改、增删、拖拽、缩放。
5. 保存 XML，重新加载，确认层级、属性、事件映射和选中控件无异常。
6. 生成 C++ 代码并编译 Demo 或宿主工程。

## Form 图标回归

1. 不设置 `Form::Icon` 启动窗口，确认标题栏、Alt-Tab 和任务栏使用当前 exe 的程序图标。
2. 显式设置 `Form::Icon` 后再显示窗口，确认自定义图标覆盖默认程序图标。

## Demo WebBrowser 回归

1. 启动 `CUITest.exe`。
2. 切到 `WebBrowser` 页签，等待 WebView2 内容初始化完成。
3. 切到其他页签，例如 `数据控件`。
4. 最大化、还原窗口，确认隐藏页中的 WebBrowser 内容不会漏绘到当前页。
5. 在 WebBrowser 内容中的按钮、文本、链接等区域移动鼠标，确认指针图标跟随 WebView2 内容变化，不会被 CUI 恢复为默认箭头。

## 鼠标捕获回归

1. 在任意支持拖拽/选择的控件或自定义画布上按住鼠标左键开始拖动。
2. 保持按下状态，把鼠标移出窗口边界继续移动。
3. 在窗口外松开鼠标，再移回窗口，确认控件不会停留在正在拖拽/框选状态。
4. 对滚动条、分割条、文本选择等已有拖拽控件做一次快速回归，确认窗口外松开也能结束操作。
5. 在自绘标题栏最小化、最大化、关闭按钮及其周边移动鼠标，确认不会出现窗口缩放光标，也不能从该区域触发边缘/角落缩放。

## 滚轮穿透回归

1. 在 `ScrollView` 内放置 `GridView`、`TreeView`、`RichTextBox` 或另一个 `ScrollView`。
2. 鼠标位于内部控件上方滚动，确认只有内部控件存在有效滚动条且当前方向还能滚动时，滚轮才由内部控件消费。
3. 内部控件滚到顶部后继续向上滚，或滚到底部后继续向下滚，确认滚轮穿透给父级 `ScrollView`。
4. 鼠标位于不支持滚动或当前无有效滚动条的控件上方时，确认父级 `ScrollView` 正常滚动。

## 已知后续清理项

- 当前 `Debug|x64`、`Release|x64`、`Debug|x86`、`Release|x86` 构建已通过，项目内常规 C4244/C4267/C4018 warning 已收敛到无警告输出。
- `TextBox` / `PasswordBox` 的输入、选择、拖放和光标绘制转换 warning 已收敛；`ComboBox` 的索引、滚动和下拉绘制转换 warning 已收敛；`GridView` 的行列索引、组合框单元格、编辑路径和主要绘制转换 warning 已收敛；`TabControl` 的页签绘制和拖放循环转换 warning 已收敛；`RichTextBox` 的滚动、绘制和拖放循环转换 warning 已收敛。
- `CUITest` Demo 和自定义示例控件的常见转换 warning 已收敛。
- `Button`、`Label`、`LinkLabel`、`CheckBox`、`RadioBox`、`ProgressBar`、`ProgressRing`、`PictureBox`、`GroupBox`、`LoadingRing`、`Slider`、`RoundTextBox`、`Switch`、`ToolBar`、`SplitContainer`、`StatusBar` 等小控件绘制转换 warning 已收敛。
- `Panel`、`ScrollView`、`TreeView` 的容器循环和主要绘制转换 warning 已收敛。
- 设计器模态编辑器的列表索引类 warning 已收敛。
- `Utils` 中第三方 `sqlite3.c` 的 C4028/C4113 签名兼容 warning 已通过 `Utils.vcxproj` 文件级 `DisableSpecificWarnings` 隔离，未修改 vendored 源码。
