#include "DemoWindow.h"
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main()
{
	Application::EnsureDpiAwareness();
	DemoWindow fm;
	fm.OnClosing += [](Form* sender, bool& canceled)
		{
			auto result = MessageDialog::Show(L"确认",
				L"是否关闭窗口!",
				MessageDialogButtons::YesNo,
				MessageDialogIcon::Question,
				sender->Handle);

			canceled = result != MessageDialogResult::Yes;
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