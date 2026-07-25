#include "Designer.h"
#include "DesignerSelfTest.h"

#include <Shellapi.h>
#include <string>
#include <vector>

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#pragma comment(lib, "Shell32.lib")

namespace
{
	void WriteReport(const std::wstring& report)
	{
		if (report.empty()) return;
		const int size = ::WideCharToMultiByte(
			CP_UTF8, 0, report.c_str(), static_cast<int>(report.size()),
			nullptr, 0, nullptr, nullptr);
		if (size <= 0) return;
		std::vector<char> utf8(static_cast<size_t>(size) + 2);
		(void)::WideCharToMultiByte(
			CP_UTF8, 0, report.c_str(), static_cast<int>(report.size()),
			utf8.data(), size, nullptr, nullptr);
		utf8[static_cast<size_t>(size)] = '\r';
		utf8[static_cast<size_t>(size) + 1] = '\n';
		const auto output = ::GetStdHandle(STD_OUTPUT_HANDLE);
		if (!output || output == INVALID_HANDLE_VALUE) return;
		DWORD written = 0;
		(void)::WriteFile(output, utf8.data(),
			static_cast<DWORD>(utf8.size()), &written, nullptr);
	}

	struct ProgramOptions
	{
		bool SelfTest = false;
		bool ShowHelp = false;
	};

	bool ParseProgramOptions(ProgramOptions& options, std::wstring& error)
	{
		int count = 0;
		auto** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &count);
		if (!arguments)
		{
			error = L"无法读取命令行参数。";
			return false;
		}
		for (int index = 1; index < count; ++index)
		{
			const std::wstring argument = arguments[index];
			if (argument == L"--self-test") options.SelfTest = true;
			else if (argument == L"--help" || argument == L"-h")
				options.ShowHelp = true;
			else
			{
				error = L"未知参数：" + argument;
				::LocalFree(arguments);
				return false;
			}
		}
		::LocalFree(arguments);
		return true;
	}
}

int main()
{
	Application::EnsureDpiAwareness();
	ProgramOptions options;
	std::wstring error;
	if (!ParseProgramOptions(options, error))
	{
		WriteReport(error);
		return 2;
	}
	if (options.ShowHelp)
	{
		WriteReport(L"Designer [--self-test]");
		return 0;
	}
	if (options.SelfTest)
	{
		std::wstring report;
		const bool passed = RunDesignerSelfTest(report);
		WriteReport(report);
		return passed ? 0 : 1;
	}

	// The product Designer consumes only framework/XAML-defined types. Native
	// application code is represented by NativeSurface placeholders and is
	// never loaded into this process.
	Designer designer;
	designer.InitAndShow();
	(void)Application::Run();
	return 0;
}
