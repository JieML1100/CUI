#include "Application.h"
Dictionary<HWND, class Form*>  Application::Forms = Dictionary<HWND, class Form*>();

bool Application::DesignMode = false;

void Application::SetDesignMode(bool value)
{
	DesignMode = value;
}

bool Application::IsDesignMode()
{
	return DesignMode;
}

std::string Application::ExecutablePath()
{
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	return std::string(path);
}
std::string Application::StartupPath()
{
	std::string path = ExecutablePath();
	return path.substr(0, path.find_last_of("\\"));
}
std::string Application::ApplicationName()
{
	std::string path = ExecutablePath();
	std::string exe = path.substr(path.find_last_of("\\") + 1);
	return exe.substr(0, exe.find_last_of("."));
}
std::string Application::LocalUserAppDataPath()
{
	char path[MAX_PATH];
	SHGetSpecialFolderPathA(NULL, path, CSIDL_LOCAL_APPDATA, FALSE);
	return std::string(path);
}
std::string Application::UserAppDataPath()
{
	char path[MAX_PATH];
	SHGetSpecialFolderPathA(NULL, path, CSIDL_APPDATA, FALSE);
	return std::string(path);
}
RegistryKey Application::UserAppDataRegistry()
{
	return RegistryKey(HKEY_CURRENT_USER, StringHelper::Format("Software\\%s", ApplicationName().c_str()));
}