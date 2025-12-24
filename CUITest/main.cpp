#include "DemoWindow.h"
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main()
{
	DemoWindow fm;
	fm.Show();

	while (1)
	{
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	return 0;
}