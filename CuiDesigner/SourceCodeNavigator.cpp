#include "SourceCodeNavigator.h"
#include "DesignerModel/CppUserCodeIndex.h"

#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace
{
	std::string WideToUtf8(const std::wstring& value)
	{
		if (value.empty()) return {};
		const int required = ::WideCharToMultiByte(
			CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		if (required <= 0) return {};
		std::string result(static_cast<size_t>(required), '\0');
		::WideCharToMultiByte(
			CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
			result.data(), required, nullptr, nullptr);
		return result;
	}

	std::wstring ReadEnvironment(const wchar_t* name)
	{
		const DWORD required = ::GetEnvironmentVariableW(name, nullptr, 0);
		if (required == 0) return {};
		std::wstring value(required, L'\0');
		const DWORD written = ::GetEnvironmentVariableW(
			name, value.data(), static_cast<DWORD>(value.size()));
		if (written == 0 || written >= value.size()) return {};
		value.resize(written);
		return value;
	}

	std::wstring ExpandEnvironment(std::wstring value)
	{
		if (value.empty()) return value;
		const DWORD required = ::ExpandEnvironmentStringsW(
			value.c_str(), nullptr, 0);
		if (required == 0) return value;
		std::wstring expanded(required, L'\0');
		const DWORD written = ::ExpandEnvironmentStringsW(
			value.c_str(), expanded.data(), required);
		if (written == 0 || written > required) return value;
		expanded.resize(written - 1);
		return expanded;
	}

	std::wstring SearchExecutable(const std::wstring& name)
	{
		if (name.empty()) return {};
		std::error_code error;
		const std::filesystem::path candidate(name);
		if ((candidate.is_absolute() || candidate.has_parent_path())
			&& std::filesystem::is_regular_file(candidate, error))
			return std::filesystem::absolute(candidate, error).wstring();

		std::vector<wchar_t> buffer(32768);
		const DWORD length = ::SearchPathW(
			nullptr, name.c_str(), nullptr,
			static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
		if (length > 0 && length < buffer.size())
			return std::wstring(buffer.data(), length);
		return {};
	}

	std::wstring ExistingFile(std::initializer_list<std::wstring> candidates)
	{
		for (const auto& candidate : candidates)
		{
			if (candidate.empty()) continue;
			std::error_code error;
			if (std::filesystem::is_regular_file(candidate, error))
				return candidate;
		}
		return {};
	}

	std::wstring FindVisualStudioCode()
	{
		if (auto path = SearchExecutable(L"Code.exe"); !path.empty())
			return path;
		const auto local = ReadEnvironment(L"LOCALAPPDATA");
		const auto programFiles = ReadEnvironment(L"ProgramFiles");
		const auto programFilesX86 = ReadEnvironment(L"ProgramFiles(x86)");
		return ExistingFile({
			local.empty() ? L"" : local + L"\\Programs\\Microsoft VS Code\\Code.exe",
			programFiles.empty() ? L"" : programFiles + L"\\Microsoft VS Code\\Code.exe",
			programFilesX86.empty() ? L"" : programFilesX86 + L"\\Microsoft VS Code\\Code.exe"
		});
	}

	std::wstring FindVisualStudio()
	{
		if (auto path = SearchExecutable(L"devenv.exe"); !path.empty())
			return path;
		const auto programFiles = ReadEnvironment(L"ProgramFiles");
		const auto programFilesX86 = ReadEnvironment(L"ProgramFiles(x86)");
		std::vector<std::wstring> roots;
		if (!programFiles.empty()) roots.push_back(programFiles);
		if (!programFilesX86.empty()) roots.push_back(programFilesX86);
		static constexpr const wchar_t* versions[] = { L"2022", L"2019" };
		static constexpr const wchar_t* editions[] = {
			L"Community", L"Professional", L"Enterprise" };
		for (const auto& root : roots)
			for (const auto* version : versions)
				for (const auto* edition : editions)
				{
					const auto candidate = root
						+ L"\\Microsoft Visual Studio\\" + version + L"\\"
						+ edition + L"\\Common7\\IDE\\devenv.exe";
					std::error_code error;
					if (std::filesystem::is_regular_file(candidate, error))
						return candidate;
				}
		return {};
	}

	std::wstring LowerFileName(const std::wstring& executable)
	{
		auto result = std::filesystem::path(executable).filename().wstring();
		std::transform(result.begin(), result.end(), result.begin(), towlower);
		return result;
	}

	void ReplaceAll(
		std::wstring& value,
		std::wstring_view token,
		std::wstring_view replacement)
	{
		for (size_t position = 0;
			(position = value.find(token, position)) != std::wstring::npos;
			position += replacement.size())
			value.replace(position, token.size(), replacement);
	}

	std::wstring FormatWindowsError(DWORD error)
	{
		wchar_t* text = nullptr;
		const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
		const DWORD length = ::FormatMessageW(
			flags, nullptr, error, 0,
			reinterpret_cast<wchar_t*>(&text), 0, nullptr);
		std::wstring result = length && text
			? std::wstring(text, length)
			: L"Windows 错误 " + std::to_wstring(error);
		if (text) ::LocalFree(text);
		while (!result.empty()
			&& (result.back() == L'\r' || result.back() == L'\n'))
			result.pop_back();
		return result;
	}

	bool LaunchProcess(
		const SourceCodeNavigationPlan& plan,
		std::wstring& error)
	{
		if (plan.Executable.empty())
		{
			error = L"源码编辑器可执行文件为空。";
			return false;
		}
		std::wstring commandLine =
			SourceCodeNavigator::QuoteArgument(plan.Executable);
		if (!plan.Arguments.empty()) commandLine += L" " + plan.Arguments;
		std::vector<wchar_t> writable(
			commandLine.begin(), commandLine.end());
		writable.push_back(L'\0');
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		const BOOL started = ::CreateProcessW(
			plan.Executable.c_str(), writable.data(),
			nullptr, nullptr, FALSE, 0, nullptr,
			plan.WorkingDirectory.empty()
				? nullptr : plan.WorkingDirectory.c_str(),
			&startup, &process);
		if (!started)
		{
			error = L"无法启动源码编辑器："
				+ FormatWindowsError(::GetLastError());
			return false;
		}
		::CloseHandle(process.hThread);
		::CloseHandle(process.hProcess);
		error.clear();
		return true;
	}

	bool OpenWithShell(
		HWND owner,
		const std::wstring& sourcePath,
		std::wstring& error)
	{
		const auto directory =
			std::filesystem::path(sourcePath).parent_path().wstring();
		const auto result = reinterpret_cast<INT_PTR>(::ShellExecuteW(
			owner, L"open", sourcePath.c_str(), nullptr,
			directory.empty() ? nullptr : directory.c_str(), SW_SHOWNORMAL));
		if (result > 32)
		{
			error.clear();
			return true;
		}
		error = L"系统文件关联无法打开用户源文件（ShellExecute="
			+ std::to_wstring(result) + L"）。";
		return false;
	}
}

size_t SourceCodeNavigator::FindMemberDefinitionLineInText(
	std::string_view source,
	std::string_view handlerName,
	std::string_view qualifiedClassName,
	std::string_view generatedParameterList)
{
	if (source.empty() || handlerName.empty()) return 0;
	DesignerModel::CppUserCodeIndex index;
	if (!DesignerModel::CppUserCodeIndex::Build(
		source, qualifiedClassName, index, nullptr)) return 0;
	const auto inspection = index.InspectHandler(
		handlerName, generatedParameterList);
	if (!generatedParameterList.empty()
		&& inspection.FirstCompatibleDefinitionLine > 0)
		return inspection.FirstCompatibleDefinitionLine;
	return inspection.FirstDefinitionLine;
}

size_t SourceCodeNavigator::FindMemberDefinitionLine(
	const std::wstring& sourcePath,
	const std::wstring& handlerName,
	const std::wstring& qualifiedClassName,
	const std::string& generatedParameterList)
{
	std::ifstream stream(
		std::filesystem::path(sourcePath), std::ios::binary);
	if (!stream) return 0;
	const std::string source{
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>() };
	return FindMemberDefinitionLineInText(
		source, WideToUtf8(handlerName), WideToUtf8(qualifiedClassName),
		generatedParameterList);
}

std::wstring SourceCodeNavigator::QuoteArgument(const std::wstring& value)
{
	std::wstring result = L"\"";
	size_t backslashes = 0;
	for (const wchar_t ch : value)
	{
		if (ch == L'\\')
		{
			++backslashes;
			continue;
		}
		if (ch == L'\"')
		{
			result.append(backslashes * 2 + 1, L'\\');
			result.push_back(L'\"');
			backslashes = 0;
			continue;
		}
		result.append(backslashes, L'\\');
		backslashes = 0;
		result.push_back(ch);
	}
	result.append(backslashes * 2, L'\\');
	result.push_back(L'\"');
	return result;
}

SourceCodeNavigationPlan SourceCodeNavigator::BuildPlan(
	SourceCodeEditorKind editor,
	std::wstring executable,
	const std::wstring& sourcePath,
	size_t line,
	const std::wstring& customArguments)
{
	SourceCodeNavigationPlan plan;
	plan.Editor = editor;
	plan.Executable = std::move(executable);
	plan.WorkingDirectory =
		std::filesystem::path(sourcePath).parent_path().wstring();
	const size_t targetLine = (std::max)(size_t{ 1 }, line);
	switch (editor)
	{
	case SourceCodeEditorKind::VisualStudioCode:
		if (line > 0)
		{
			plan.Arguments = L"--goto " + QuoteArgument(
				sourcePath + L":" + std::to_wstring(targetLine) + L":1");
			plan.RequestsExactLine = true;
		}
		else plan.Arguments = QuoteArgument(sourcePath);
		break;
	case SourceCodeEditorKind::VisualStudio:
		plan.Arguments = L"/Edit " + QuoteArgument(sourcePath);
		if (line > 0)
		{
			plan.Arguments += L" /Command "
				+ QuoteArgument(L"Edit.Goto " + std::to_wstring(targetLine));
			plan.RequestsExactLine = true;
		}
		break;
	case SourceCodeEditorKind::Custom:
		if (!customArguments.empty())
		{
			plan.Arguments = customArguments;
			const bool hasFile = plan.Arguments.find(L"{file}")
				!= std::wstring::npos;
			const bool hasLine = plan.Arguments.find(L"{line}")
				!= std::wstring::npos;
			ReplaceAll(plan.Arguments, L"{file}", QuoteArgument(sourcePath));
			ReplaceAll(plan.Arguments, L"{line}", std::to_wstring(targetLine));
			ReplaceAll(plan.Arguments, L"{column}", L"1");
			if (!hasFile) plan.Arguments += L" " + QuoteArgument(sourcePath);
			plan.RequestsExactLine = line > 0 && hasLine;
		}
		else
		{
			const auto fileName = LowerFileName(plan.Executable);
			if (fileName == L"code.exe" || fileName == L"code-insiders.exe")
				return BuildPlan(SourceCodeEditorKind::VisualStudioCode,
					std::move(plan.Executable), sourcePath, line);
			if (fileName == L"devenv.exe")
				return BuildPlan(SourceCodeEditorKind::VisualStudio,
					std::move(plan.Executable), sourcePath, line);
			plan.Arguments = QuoteArgument(sourcePath);
		}
		break;
	case SourceCodeEditorKind::ShellAssociation:
	default:
		plan.Executable.clear();
		plan.Arguments.clear();
		break;
	}
	return plan;
}

SourceCodeNavigationPlan SourceCodeNavigator::ResolvePlan(
	const std::wstring& sourcePath,
	size_t line)
{
	auto configured = ExpandEnvironment(ReadEnvironment(L"CUI_CODE_EDITOR"));
	if (!configured.empty())
	{
		if (const auto resolved = SearchExecutable(configured); !resolved.empty())
			configured = resolved;
		return BuildPlan(
			SourceCodeEditorKind::Custom, std::move(configured), sourcePath, line,
			ReadEnvironment(L"CUI_CODE_EDITOR_ARGS"));
	}
	if (auto code = FindVisualStudioCode(); !code.empty())
		return BuildPlan(SourceCodeEditorKind::VisualStudioCode,
			std::move(code), sourcePath, line);
	if (auto visualStudio = FindVisualStudio(); !visualStudio.empty())
		return BuildPlan(SourceCodeEditorKind::VisualStudio,
			std::move(visualStudio), sourcePath, line);
	return BuildPlan(SourceCodeEditorKind::ShellAssociation,
		{}, sourcePath, line);
}

bool SourceCodeNavigator::Open(
	HWND owner,
	const std::wstring& sourcePath,
	size_t line,
	SourceCodeNavigationResult* outResult,
	std::wstring* outError)
{
	if (outResult) *outResult = SourceCodeNavigationResult{};
	if (outError) outError->clear();
	std::error_code fileError;
	if (!std::filesystem::is_regular_file(sourcePath, fileError))
	{
		if (outError) *outError = L"用户源文件不存在：" + sourcePath;
		return false;
	}

	auto plan = ResolvePlan(sourcePath, line);
	std::wstring error;
	if (plan.Editor != SourceCodeEditorKind::ShellAssociation
		&& LaunchProcess(plan, error))
	{
		if (outResult) outResult->Plan = std::move(plan);
		return true;
	}

	const bool usedFallback =
		plan.Editor != SourceCodeEditorKind::ShellAssociation;
	if (!OpenWithShell(owner, sourcePath, error))
	{
		if (outError) *outError = std::move(error);
		return false;
	}
	if (outResult)
	{
		outResult->Plan = BuildPlan(
			SourceCodeEditorKind::ShellAssociation, {}, sourcePath, line);
		outResult->UsedShellFallback = usedFallback;
	}
	return true;
}
