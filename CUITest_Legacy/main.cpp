#include "DemoWindow_Legacy.h"
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main()
{
	Application::EnsureDpiAwareness();
	DemoWindow_Legacy fm;
	fm.Show();
	while (1)
	{
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	return 0;
}