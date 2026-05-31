#include "DemoWindow.h"
#include "../Utils/Utils/HttpHelper.h"
#include "../Utils/Utils/json.h"
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

std::string kavaca_post(const std::string& api,const std::string& body) {
	return HttpHelper::Post(std::format("http://kavacaapi.chinacarwrap.com{}", api),
		body,
		std::format(R"(Culture: en-us
Code: 91000
Version: 1.0.1.10
signature: 015153e3b0cec4af914bee2d9010169e
tenant: be5f5d32-58de-42ad-ba39-809aa31e1b8f
CheckCode: e298687e41ed01a98a7781b533c42dc4
ClientOS: Microsoft+Windows+10+%E4%B8%93%E4%B8%9A%E7%89%88+10.0.19045
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJodHRwOi8vc2NoZW1hcy54bWxzb2FwLm9yZy93cy8yMDA1LzA1L2lkZW50aXR5L2NsYWltcy9uYW1laWRlbnRpZmllciI6ImYxNDgxNjI1LTQ3MDYtNDE5Zi1iYmMzLWRiMWU5ZDAyZDU1YSIsImh0dHA6Ly9zY2hlbWFzLnhtbHNvYXAub3JnL3dzLzIwMDUvMDUvaWRlbnRpdHkvY2xhaW1zL25hbWUiOiJMdVRhbmciLCJodHRwOi8vc2NoZW1hcy5taWNyb3NvZnQuY29tL3dzLzIwMDgvMDYvaWRlbnRpdHkvY2xhaW1zL3JvbGUiOiIiLCJncm91cCI6IiIsIm93bmVyIjoiYzI5NjE3OTctZDZmNi00NTZmLTlmMWItMzQwOTBjZDEzMzFiIiwicGFyZW50IjoiIiwibWVyY2hhbnQiOiIwMDAwMDAwMC0wMDAwLTAwMDAtMDAwMC0wMDAwMDAwMDAwMDAiLCJ0ZW5hbnQiOiJiZTVmNWQzMi01OGRlLTQyYWQtYmEzOS04MDlhYTMxZTFiOGYiLCJzb3VyY2UiOiJERUZBVUxUIiwianRpIjoiMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAiLCJzdWIiOiJMdVRhbmciLCJpc3MiOiIxMDAwMDAiLCJhdWQiOiIxMDAwMDAifQ.g5Wq2sZZ0uN1A8PSF3ahtUd70JV7cVkqTpU3rX5hnTg
Accept: text/html, application/xhtml+xml, */*
Content-Type: application/json
User-Agent: Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)
Host: kavacaapi.chinacarwrap.com
Content-Length: {}
Expect: 1000-continue)", body.size()));
}


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