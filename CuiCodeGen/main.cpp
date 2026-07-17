#include "../CuiDesigner/DesignerModel/DesignCodeGenerationService.h"

#include <Windows.h>

#include <string>
#include <string_view>

namespace
{
	constexpr int ExitSuccess = 0;
	constexpr int ExitGenerationFailure = 1;
	constexpr int ExitUsageError = 2;

	void WriteText(HANDLE stream, std::wstring_view value)
	{
		if (!stream || stream == INVALID_HANDLE_VALUE || value.empty()) return;
		DWORD mode = 0;
		if (::GetConsoleMode(stream, &mode))
		{
			DWORD written = 0;
			(void)::WriteConsoleW(stream, value.data(),
				static_cast<DWORD>(value.size()), &written, nullptr);
			return;
		}
		const int required = ::WideCharToMultiByte(
			CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		if (required <= 0) return;
		std::string utf8(static_cast<size_t>(required), '\0');
		(void)::WideCharToMultiByte(
			CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
			utf8.data(), required, nullptr, nullptr);
		DWORD written = 0;
		(void)::WriteFile(stream, utf8.data(),
			static_cast<DWORD>(utf8.size()), &written, nullptr);
	}

	void WriteLine(HANDLE stream, std::wstring value)
	{
		value += L"\r\n";
		WriteText(stream, value);
	}

	void PrintUsage(HANDLE stream)
	{
		WriteText(stream,
			L"CuiCodeGen - CUI headless design-file code generator\r\n"
			L"\r\n"
			L"Usage:\r\n"
			L"  CuiCodeGen generate <design-file> [--output <base>] [--class <name>] [--quiet]\r\n"
			L"  CuiCodeGen --help\r\n"
			L"  CuiCodeGen --version\r\n"
			L"\r\n"
			L"Without overrides, x:Class and d:CodeBehind are read from the design file.\r\n"
			L"The output value is an extensionless base path; relative overrides use the current directory.\r\n");
	}

	bool ReadOptionValue(
		int& index,
		int argc,
		wchar_t** argv,
		std::wstring_view option,
		std::wstring& output,
		std::wstring& error)
	{
		const std::wstring_view current(argv[index]);
		const std::wstring prefix = std::wstring(option) + L"=";
		if (current.starts_with(prefix))
		{
			output.assign(current.substr(prefix.size()));
		}
		else
		{
			if (index + 1 >= argc)
			{
				error = L"选项缺少值：" + std::wstring(option);
				return false;
			}
			output = argv[++index];
		}
		if (output.empty())
		{
			error = L"选项值不能为空：" + std::wstring(option);
			return false;
		}
		return true;
	}
}

int wmain(int argc, wchar_t** argv)
{
	const HANDLE standardOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
	const HANDLE standardError = ::GetStdHandle(STD_ERROR_HANDLE);
	if (argc == 2 && (std::wstring_view(argv[1]) == L"--help"
		|| std::wstring_view(argv[1]) == L"-h"))
	{
		PrintUsage(standardOutput);
		return ExitSuccess;
	}
	if (argc == 2 && std::wstring_view(argv[1]) == L"--version")
	{
		WriteLine(standardOutput, L"CuiCodeGen 1");
		return ExitSuccess;
	}
	if (argc < 3 || std::wstring_view(argv[1]) != L"generate")
	{
		PrintUsage(standardError);
		return ExitUsageError;
	}

	const std::wstring designFile = argv[2];
	DesignerModel::DesignCodeGenerationOptions options;
	bool quiet = false;
	std::wstring parseError;
	for (int index = 3; index < argc; ++index)
	{
		const std::wstring_view argument(argv[index]);
		if (argument == L"--quiet")
		{
			quiet = true;
			continue;
		}
		if (argument == L"--output" || argument.starts_with(L"--output="))
		{
			if (!options.OutputBasePath.empty())
			{
				parseError = L"--output 只能指定一次。";
				break;
			}
			if (!ReadOptionValue(index, argc, argv, L"--output",
				options.OutputBasePath, parseError)) break;
			continue;
		}
		if (argument == L"--class" || argument.starts_with(L"--class="))
		{
			if (!options.ClassName.empty())
			{
				parseError = L"--class 只能指定一次。";
				break;
			}
			if (!ReadOptionValue(index, argc, argv, L"--class",
				options.ClassName, parseError)) break;
			continue;
		}
		parseError = L"未知参数：" + std::wstring(argument);
		break;
	}
	if (!parseError.empty())
	{
		WriteLine(standardError, L"CuiCodeGen: " + parseError);
		return ExitUsageError;
	}

	DesignerModel::DesignCodeGenerationResult result;
	std::wstring error;
	if (!DesignerModel::DesignCodeGenerationService::GenerateFile(
		designFile, options, &result, &error))
	{
		WriteLine(standardError, L"CuiCodeGen: "
			+ (error.empty() ? L"代码生成失败。" : error));
		return ExitGenerationFailure;
	}
	if (!quiet)
	{
		WriteLine(standardOutput, L"Generated " + result.ClassName);
		for (const auto& path : result.OutputFiles())
			WriteLine(standardOutput, L"  " + path);
	}
	return ExitSuccess;
}
