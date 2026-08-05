#include "DemoWindow.h"
#include "MediaPerformanceRunner.h"

#include <Utils.h>

#include <Windows.h>

#include <exception>
#include <filesystem>
#include <string_view>

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

namespace
{
	std::filesystem::path DiagnosticPath()
	{
		std::wstring executable(32768, L'\0');
		const auto length = GetModuleFileNameW(
			nullptr, executable.data(), static_cast<DWORD>(executable.size()));
		if (!length || length >= executable.size())
			return std::filesystem::current_path() / L"CUITest.aot.error.txt";
		executable.resize(length);
		return std::filesystem::path(executable).parent_path()
			/ L"CUITest.aot.error.txt";
	}

	void WriteDiagnostic(const std::wstring& error)
	{
		const auto diagnosticPath = DiagnosticPath();
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
			const auto message = L"CUITest AOT failed: " + error + L"\r\n";
			DWORD written = 0;
			(void)WriteConsoleW(output, message.data(),
				static_cast<DWORD>(message.size()), &written, nullptr);
			CloseHandle(output);
		}
	}

	void ClearDiagnostic()
	{
		(void)DeleteFileW(DiagnosticPath().c_str());
	}
}

int main(int argc, char** argv)
{
	const auto mediaPerformance = ParseMediaPerformanceCommandLine();
	if (mediaPerformance.State != MediaPerformanceParseState::NotRequested)
	{
		if (mediaPerformance.State == MediaPerformanceParseState::Invalid)
		{
			WriteDiagnostic(L"Invalid CUITest media performance arguments: "
				+ mediaPerformance.Error);
			return 64;
		}
		try
		{
			Application::EnsureDpiAwareness();
			std::wstring error;
			const int result = RunMediaPerformance(
				mediaPerformance.Options, &error);
			if (result != 0)
			{
				WriteDiagnostic(error.empty()
					? L"CUITest media performance run failed." : error);
				return result;
			}
			ClearDiagnostic();
			return 0;
		}
		catch (const std::exception& error)
		{
			WriteDiagnostic(L"CUITest media performance run failed: "
				+ Convert::StringToWString(error.what()));
			return 5;
		}
	}

	if (argc == 2 && std::string_view(argv[1]) == "--construct-xaml")
	{
		try
		{
			Application::EnsureDpiAwareness();
			DemoWindow window(DemoWindow::InitializationMode::DeclarativeOnly);
			ClearDiagnostic();
			return 0;
		}
		catch (const std::exception& error)
		{
			WriteDiagnostic(Convert::StringToWString(error.what()));
			return 2;
		}
	}
	if (argc == 2 && std::string_view(argv[1]) == "--validate-xaml")
	{
		try
		{
			std::wstring error;
			Application::EnsureDpiAwareness();
			DemoWindow window(DemoWindow::InitializationMode::DeclarativeOnly);
			if (!window.VerifyDeclarativeFeatures(&error)
				|| !window.VerifyTextCompositionFeatures(&error))
			{
				WriteDiagnostic(error);
				return 2;
			}
			ClearDiagnostic();
			return 0;
		}
		catch (const std::exception& error)
		{
			WriteDiagnostic(Convert::StringToWString(error.what()));
			return 2;
		}
	}
	if (argc == 2 && std::string_view(argv[1]) == "--smoke-xaml")
	{
		try
		{
			Application::EnsureDpiAwareness();
			// Exercise the complete XAML tree plus ordinary runtime data, while
			// keeping taskbar/tray/toast platform side effects disabled.
			DemoWindow window(DemoWindow::InitializationMode::RuntimeData);
			std::wstring error;
			if (!window.VerifyDeclarativeFeatures(&error)
				|| !window.VerifyTextCompositionFeatures(&error)
				|| !window.VerifyRuntimeDataFeatures(&error))
			{
				WriteDiagnostic(error);
				return 3;
			}
			ClearDiagnostic();
			return 0;
		}
		catch (const std::exception& error)
		{
			WriteDiagnostic(Convert::StringToWString(error.what()));
			return 3;
		}
	}
	if (argc == 2 && std::string_view(argv[1]) == "--render-smoke")
	{
		try
		{
			Application::EnsureDpiAwareness();
			{
				DemoWindow window(DemoWindow::InitializationMode::DeclarativeOnly);
				std::wstring error;
				if (!window.VerifyPresentationFeatures(&error))
				{
					WriteDiagnostic(error);
					return 4;
				}
			}
			ClearDiagnostic();
			return 0;
		}
		catch (const std::exception& error)
		{
			WriteDiagnostic(Convert::StringToWString(error.what()));
			return 4;
		}
	}
	if (argc != 1)
	{
		WriteDiagnostic(
			L"Unsupported CUITest argument. Production accepts only "
			L"--construct-xaml, --validate-xaml, --smoke-xaml, "
			L"--render-smoke, or --media <path> [--rate 0.1..4.0] "
			L"[--duration seconds] [--video-path auto|cpu|gpu-required] "
			L"[--require-audio] "
			L"[--expect-width pixels] [--expect-height pixels] [--expect-fps value] "
			L"[--inject-presentation-device-loss-at seconds | "
			L"--inject-shared-device-rotation-at seconds] "
			L"[--perf-json path].");
		return 64;
	}

	try
	{
		Application::EnsureDpiAwareness();
		Application application;
		DemoWindow window;
		return application.Run(window);
	}
	catch (const std::exception& error)
	{
		MessageBoxW(nullptr,
			Convert::StringToWString(error.what()).c_str(),
			L"CUITest XAML startup failed", MB_OK | MB_ICONERROR);
		return 1;
	}
}
