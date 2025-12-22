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
	static std::string ExecutablePath();
	static std::string StartupPath();
	static std::string ApplicationName();
	static std::string LocalUserAppDataPath();
	static std::string UserAppDataPath();
	static RegistryKey UserAppDataRegistry();

};