#pragma once
/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Utils/Utils.h>

/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
// (using external CppUtils)

/**
 * @file Application.h
 * @brief CUI 应用级静态工具与全局状态。
 *
 * 该类不负责窗口渲染本身，而是提供：
 * - 窗口(Form)注册表
 * - 设计器模式开关
 * - 路径/应用名获取
 * - 用户数据目录与注册表键
 */
class Application
{
public:
	/**
	 * @brief 当前进程内创建过的所有 Form。
	 *
	 * Key 为窗口句柄 HWND；Value 为对应的 Form 指针。
	 * 注意：指针所有权不属于该容器（仅用于查找/分发）。
	 */
	static Dictionary<HWND, class Form*> Forms;
	/**
	 * @brief 设计器模式：用于在设计时禁用一些会产生副作用的真实组件（例如 WebView2）。
	 */
	static bool DesignMode;
	/**
	 * @brief 设置设计器模式。
	 * @param value true 表示启用设计器模式。
	 */
	static void SetDesignMode(bool value);
	/**
	 * @brief 获取是否处于设计器模式。
	 */
	static bool IsDesignMode();
	/**
	 * @brief 返回当前可执行文件完整路径。
	 */
	static std::string ExecutablePath();
	/**
	 * @brief 返回启动目录（通常为可执行文件所在目录）。
	 */
	static std::string StartupPath();
	/**
	 * @brief 返回应用名（通常从可执行文件推导）。
	 */
	static std::string ApplicationName();
	/**
	 * @brief 返回本地（Local）用户数据目录。
	 */
	static std::string LocalUserAppDataPath();
	/**
	 * @brief 返回漫游（Roaming）用户数据目录。
	 */
	static std::string UserAppDataPath();
	/**
	 * @brief 返回用于保存用户数据的注册表键。
	 */
	static RegistryKey UserAppDataRegistry();

};