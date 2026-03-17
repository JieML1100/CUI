#include "DemoWindow.h"
//#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main()
{
	Application::EnsureDpiAwareness();
	DemoWindow fm;
	fm.OnClosing += [](Form* sender, bool& canceled)
		{
			if (MessageBoxW(sender->Handle, L"真的要退出吗？", L"提示", MB_ICONQUESTION | MB_YESNO) == IDNO)
				canceled = true;
		};
	fm.Show();
	while (1)
	{
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	return 0;
}