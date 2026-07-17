#include "DemoWindow.h"

#include <Utils.h>

#include <Windows.h>

#include <exception>
#include <string_view>

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

namespace
{
	void WriteDiagnostic(const std::wstring& error)
	{
		const auto diagnosticPath = DemoWindow::XamlFilePath() + L".error.txt";
		const auto diagnostic = Convert::WStringToString(error);
		if (const auto file = CreateFileW(diagnosticPath.c_str(), GENERIC_WRITE,
			0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			file && file != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			(void)WriteFile(file, diagnostic.data(),
				static_cast<DWORD>(diagnostic.size()), &written, nullptr);
			CloseHandle(file);
		}
		(void)AttachConsole(ATTACH_PARENT_PROCESS);
		const auto output = CreateFileW(L"CONOUT$", GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		if (output && output != INVALID_HANDLE_VALUE)
		{
			const auto message = L"CUITest XAML failed: " + error + L"\r\n";
			DWORD written = 0;
			(void)WriteConsoleW(output, message.data(),
				static_cast<DWORD>(message.size()), &written, nullptr);
			CloseHandle(output);
		}
	}

	void ClearDiagnostic()
	{
		const auto path = DemoWindow::XamlFilePath() + L".error.txt";
		(void)DeleteFileW(path.c_str());
	}
}

int main(int argc, char** argv)
{
	if (argc == 2 && std::string_view(argv[1]) == "--validate-xaml")
	{
		std::wstring error;
		if (DemoWindow::ValidateXaml(&error))
		{
			ClearDiagnostic();
			return 0;
		}
		WriteDiagnostic(error);
		return 2;
	}
	if (argc == 2 && std::string_view(argv[1]) == "--smoke-xaml")
	{
		try
		{
			Application::EnsureDpiAwareness();
			DemoWindow window;
			ClearDiagnostic();
			return 0;
		}
		catch (const std::exception& error)
		{
			WriteDiagnostic(Convert::StringToWString(error.what()));
			return 3;
		}
	}

	try
	{
		Application::EnsureDpiAwareness();
		DemoWindow window;
		window.Show();
		while (!Application::Forms.empty()) Form::DoEvent();
		return 0;
	}
	catch (const std::exception& error)
	{
		MessageBoxW(nullptr,
			Convert::StringToWString(error.what()).c_str(),
			L"CUITest XAML startup failed", MB_OK | MB_ICONERROR);
		return 1;
	}
}
