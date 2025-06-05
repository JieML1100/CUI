#include "AudioView.h"
int main()
{
    //http://sym101.com:5000/

	HttpHelper::Get("http://sym101.com:5000/");
	AudioView* av = new AudioView();
	av->Show();
    NotifyIcon notifyIcon;
    notifyIcon.InitNotifyIcon(av->Handle, 1);
    notifyIcon.SetIcon(LoadIcon(NULL, IDI_APPLICATION));
    notifyIcon.SetToolTip("应用程序");
    notifyIcon.ShowNotifyIcon();

    notifyIcon.AddMenuItem(NotifyIconMenuItem("打开主窗口", 1));

    
    NotifyIconMenuItem settingsMenu("设置", 2);
    settingsMenu.HasSubMenu = true;
    settingsMenu.SubMenu = CreatePopupMenu();

    
    settingsMenu.AddSubItem(NotifyIconMenuItem("音频设置", 21));
    settingsMenu.AddSubItem(NotifyIconMenuItem("显示设置", 22));
    settingsMenu.AddSubItem(NotifyIconMenuItem::CreateSeparator());
    settingsMenu.AddSubItem(NotifyIconMenuItem("高级设置", 23));

    
    notifyIcon.AddMenuItem(settingsMenu);

    
    notifyIcon.AddMenuSeparator();
    notifyIcon.AddMenuItem(NotifyIconMenuItem("退出", 3));

    
    notifyIcon.OnNotifyIconMenuClick += [](NotifyIcon* sender, int menuId) {
            switch (menuId) {
            case 1: 
                ShowWindow(sender->hWnd, SW_SHOW);
                break;
            case 21: 
                break;
            case 22: 
                break;
            case 23: 
                break;
            case 3: 
                PostMessage(sender->hWnd, WM_CLOSE, 0, 0);
                break;
            }
        };
	int index = 0;
	while (1)
	{
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	return 0;
}
