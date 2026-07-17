# Designer 自定义控件预览插件 ABI

## 结论

外部设计文件只声明 `DesignerCustomControlType`，不能触发 DLL 加载。同进程宿主可注入
`DesignerControlCatalog::AttachPreviewFactory(...)`；独立 DLL 预览由显式受信任的
`--preview-plugin <path>` 或宿主 API 加载，采用 `DesignerPreviewPluginAbi.h` 的宿主代理协议，
插件不得返回 `Control*`。

原因不是只有 `new/delete`：CUI 控件树以 `std::unique_ptr<Control>` 和直接 `delete` 持有对象，项目又使用
静态 CRT；更重要的是 Binding/属性元数据注册表随静态 CUI 库复制到每个模块。让插件静态链接另一份 CUI 后
返回派生对象，会造成宿主与插件看到不同的注册表、默认对象和全局状态。即便特定 MSVC 版本的虚析构恰好在
分配模块释放内存，这个模型也不是稳定 ABI。

## V1 边界

- ABI 是纯 C，入口只有 `CuiDesignerGetPreviewPluginV1`。
- 每个可扩展结构都带 `StructSize`，函数表带 `AbiVersion`；宿主只读取双方共同的前缀。
- XAML 命名空间与名称选择 session。插件创建并销毁自己的 opaque session，宿主不释放插件内存。
- 宿主拥有真正进入 Canvas/布局/输入/属性系统的基类代理控件。
- 插件只接收尺寸、DPI、状态和强类型属性值，并返回矩形、圆角矩形、椭圆、线和 UTF-8 文本等值类型绘制原语。
- 区域原语的 `X/Y/Width/Height` 是局部 DIP 边界；线原语从 `(X,Y)` 绘制到
  `(X+Width,Y+Height)`；ARGB 的 alpha 为 0 表示不绘制对应 fill/stroke。
- frame/字符串内存归插件所有，只在当前 session 的下一次调用前有效；宿主必须同步检查上限并复制需要保留的值。
- 不跨边界传递 STL、异常、C++ RTTI、`Control*`、`Font*`、COM/D2D 指针、分配器或回调闭包。
- 所有 session 调用保持在创建它的 Designer UI 线程。`DestroySession` 完成后不得再回调宿主；全部 session
  销毁后调用一次 `Shutdown`，随后才允许 `FreeLibrary`。

## 安全与加载策略

插件路径不能来自 `.cui.xaml`、`.cui.xml` 或控件清单，否则打开设计文件就会执行任意代码。loader 只
接受显式 `--preview-plugin <path>`、受信任项目配置或宿主 API，并在加载前规范化绝对路径。清单的
namespace/name 只用于已经受信任插件中的 session 路由。

宿主应拒绝：ABI 主版本不匹配、结构过小、必需函数为空、超过 4096 个原语、单帧 UTF-8 总量超过 1 MiB、
非有限几何值、越界枚举和插件抛出到 C 边界。插件失败只禁用该控件的增强预览并保留基类代理与文档；不得
破坏当前 Canvas 或阻止保存/代码生成。

## 已实现与后续

V1 已实现 RAII loader/session、函数表与帧上限校验、宿主绘制 decorator、
清单强类型属性到 `SetValue` 的同步、样例 DLL 和无窗口端到端门禁。
后续是进程隔离模式：不受信任插件应放到独立预览进程，崩溃时回退基类代理。
只有 CUI 自身改为单一共享运行时并发布兼容承诺后，才重新评估直接加载真实 `Control` 派生类。
