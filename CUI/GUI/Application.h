#pragma once
/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Utils/Utils.h>

/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
// (using external CppUtils)
class Application
{
public:
	static Dictionary<HWND, class Form*> Forms;
	// 设计器模式：用于在设计时禁用一些会产生副作用的真实组件（例如 WebView2）。
	static bool DesignMode;
	static void SetDesignMode(bool value);
	static bool IsDesignMode();
	static std::string ExecutablePath();
	static std::string StartupPath();
	static std::string ApplicationName();
	static std::string LocalUserAppDataPath();
	static std::string UserAppDataPath();
	static RegistryKey UserAppDataRegistry();

};