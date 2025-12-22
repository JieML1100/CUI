#include "Designer.h"
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main()
{
	Designer designer;
	// 初始化完成后再显示，确保所有控件的ParentForm都已设置
	designer.InitAndShow();

	while (1)
	{
		Form::DoEvent();
		if (Application::Forms.size() == 0)
			break;
	}
	return 0;
}