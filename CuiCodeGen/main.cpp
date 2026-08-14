#include "../CuiDesigner/DesignerModel/DesignCodeGenerationService.h"
#include "../CuiDesigner/FrameworkThemeCodeGenerator.h"

#include <Windows.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
			L"  CuiCodeGen generate <design-file> [--output <base>] [--class <name>] [--converter-manifest <xml>] [--quiet]\r\n"
			L"  CuiCodeGen compile <xaml-file> --output <base> [--class <name>] [--converter-manifest <xml>] [--quiet]\r\n"
			L"  CuiCodeGen compile-theme <resource-dictionary> --output <base> [--class <name>] [--root <xaml>]... [--preserve-type <type>]... [--preserve-resource <key>]... [--quiet]\r\n"
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

	void AppendSemicolonSeparated(
		const std::wstring& values,
		std::vector<std::wstring>& output)
	{
		size_t start = 0;
		while (start <= values.size())
		{
			const auto separator = values.find(L';', start);
			const auto end = separator == std::wstring::npos
				? values.size() : separator;
			if (end > start)
				output.push_back(values.substr(start, end - start));
			if (separator == std::wstring::npos) break;
			start = separator + 1;
		}
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
		WriteLine(standardOutput, L"CuiCodeGen " + std::to_wstring(
			DesignerModel::DesignCodeGenerationContractVersion));
		return ExitSuccess;
	}
	if (argc < 3)
	{
		PrintUsage(standardError);
		return ExitUsageError;
	}
	const std::wstring_view command(argv[1]);
	const bool generateUserFiles = command == L"generate";
	const bool compileGeneratedOnly = command == L"compile";
	const bool compileTheme = command == L"compile-theme";
	if (!generateUserFiles && !compileGeneratedOnly && !compileTheme)
	{
		PrintUsage(standardError);
		return ExitUsageError;
	}

	const std::wstring designFile = argv[2];
	DesignerModel::DesignCodeGenerationOptions options;
	std::vector<std::wstring> themeRootDocuments;
	std::vector<std::wstring> preservedThemeTypes;
	std::vector<std::wstring> preservedThemeResources;
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
		if (argument == L"--converter-manifest"
			|| argument.starts_with(L"--converter-manifest="))
		{
			if (compileTheme)
			{
				parseError = L"--converter-manifest 不适用于 compile-theme。";
				break;
			}
			if (!options.ConverterManifestPath.empty())
			{
				parseError = L"--converter-manifest 只能指定一次。";
				break;
			}
			if (!ReadOptionValue(index, argc, argv, L"--converter-manifest",
				options.ConverterManifestPath, parseError)) break;
			continue;
		}
		if (argument == L"--root" || argument.starts_with(L"--root="))
		{
			if (!compileTheme)
			{
				parseError = L"--root 仅适用于 compile-theme。";
				break;
			}
			std::wstring root;
			if (!ReadOptionValue(index, argc, argv, L"--root",
				root, parseError)) break;
			themeRootDocuments.push_back(std::move(root));
			continue;
		}
		if (argument == L"--roots" || argument.starts_with(L"--roots="))
		{
			if (!compileTheme)
			{
				parseError = L"--roots 仅适用于 compile-theme。";
				break;
			}
			std::wstring roots;
			if (!ReadOptionValue(index, argc, argv, L"--roots",
				roots, parseError)) break;
			AppendSemicolonSeparated(roots, themeRootDocuments);
			continue;
		}
		if (argument == L"--preserve-type"
			|| argument.starts_with(L"--preserve-type="))
		{
			if (!compileTheme)
			{
				parseError = L"--preserve-type 仅适用于 compile-theme。";
				break;
			}
			std::wstring type;
			if (!ReadOptionValue(index, argc, argv, L"--preserve-type",
				type, parseError)) break;
			preservedThemeTypes.push_back(std::move(type));
			continue;
		}
		if (argument == L"--preserve-types"
			|| argument.starts_with(L"--preserve-types="))
		{
			if (!compileTheme)
			{
				parseError = L"--preserve-types 仅适用于 compile-theme。";
				break;
			}
			std::wstring types;
			if (!ReadOptionValue(index, argc, argv, L"--preserve-types",
				types, parseError)) break;
			AppendSemicolonSeparated(types, preservedThemeTypes);
			continue;
		}
		if (argument == L"--preserve-resource"
			|| argument.starts_with(L"--preserve-resource="))
		{
			if (!compileTheme)
			{
				parseError = L"--preserve-resource 仅适用于 compile-theme。";
				break;
			}
			std::wstring resource;
			if (!ReadOptionValue(index, argc, argv, L"--preserve-resource",
				resource, parseError)) break;
			preservedThemeResources.push_back(std::move(resource));
			continue;
		}
		if (argument == L"--preserve-resources"
			|| argument.starts_with(L"--preserve-resources="))
		{
			if (!compileTheme)
			{
				parseError = L"--preserve-resources 仅适用于 compile-theme。";
				break;
			}
			std::wstring resources;
			if (!ReadOptionValue(index, argc, argv, L"--preserve-resources",
				resources, parseError)) break;
			AppendSemicolonSeparated(resources, preservedThemeResources);
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

	std::wstring error;
	std::wstring generatedClass;
	std::vector<std::wstring> generatedFiles;
	if (generateUserFiles)
	{
		DesignerModel::DesignCodeGenerationResult result;
		if (!DesignerModel::DesignCodeGenerationService::GenerateFile(
			designFile, options, &result, &error))
		{
			WriteLine(standardError, L"CuiCodeGen: "
				+ (error.empty() ? L"代码生成失败。" : error));
			return ExitGenerationFailure;
		}
		generatedClass = result.ClassName;
		generatedFiles = result.OutputFiles();
	}
	else if (compileGeneratedOnly)
	{
		DesignerModel::DesignGeneratedCodeResult result;
		if (!DesignerModel::DesignCodeGenerationService::
			GenerateGeneratedOnlyFile(
				designFile, options, &result, &error))
		{
			WriteLine(standardError, L"CuiCodeGen: "
				+ (error.empty() ? L"静态 C++ 代码生成失败。" : error));
			return ExitGenerationFailure;
		}
		generatedClass = result.ClassName;
		generatedFiles = result.OutputFiles();
	}
	else
	{
		DesignerModel::FrameworkThemeCodeGenerationOptions themeOptions;
		themeOptions.OutputBasePath = options.OutputBasePath;
		themeOptions.RootDocuments = std::move(themeRootDocuments);
		themeOptions.PreservedTypes = std::move(preservedThemeTypes);
		themeOptions.PreservedResources =
			std::move(preservedThemeResources);
		if (!options.ClassName.empty())
			themeOptions.ClassName = options.ClassName;
		DesignerModel::FrameworkThemeCodeGenerationResult result;
		if (!DesignerModel::FrameworkThemeCodeGenerator::GenerateFile(
			designFile, themeOptions, &result, &error))
		{
			WriteLine(standardError, L"CuiCodeGen: "
				+ (error.empty() ? L"静态主题代码生成失败。" : error));
			return ExitGenerationFailure;
		}
		generatedClass = result.ClassName;
		generatedFiles = result.OutputFiles();
		if (!quiet && result.IsApplicationClosure())
		{
			WriteLine(standardOutput,
				L"Theme closure: styles "
				+ std::to_wstring(result.RetainedStyleRuleCount) + L"/"
				+ std::to_wstring(result.SourceStyleRuleCount)
				+ L", templates "
				+ std::to_wstring(result.RetainedControlTemplateCount) + L"/"
				+ std::to_wstring(result.SourceControlTemplateCount)
				+ L", resources "
				+ std::to_wstring(result.RetainedResourceCount) + L"/"
				+ std::to_wstring(result.SourceResourceCount));
		}
	}
	if (!quiet)
	{
		WriteLine(standardOutput, L"Generated " + generatedClass);
		for (const auto& path : generatedFiles)
			WriteLine(standardOutput, L"  " + path);
	}
	return ExitSuccess;
}
