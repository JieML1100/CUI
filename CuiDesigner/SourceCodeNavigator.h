#pragma once

#include <Windows.h>

#include <cstddef>
#include <string>
#include <string_view>

enum class SourceCodeEditorKind : unsigned char
{
	ShellAssociation,
	VisualStudioCode,
	VisualStudio,
	Custom
};

struct SourceCodeNavigationPlan
{
	SourceCodeEditorKind Editor = SourceCodeEditorKind::ShellAssociation;
	std::wstring Executable;
	std::wstring Arguments;
	std::wstring WorkingDirectory;
	bool RequestsExactLine = false;
};

struct SourceCodeNavigationResult
{
	SourceCodeNavigationPlan Plan;
	bool UsedShellFallback = false;
};

/**
 * Resolves and launches a source editor without routing paths through a shell.
 *
 * CUI_CODE_EDITOR may name an executable. CUI_CODE_EDITOR_ARGS optionally
 * supplies an argument template containing {file}, {line}, and {column};
 * placeholders are expanded as correctly quoted Windows command-line values.
 */
class SourceCodeNavigator final
{
public:
	/** Finds an out-of-class member definition while ignoring comments/literals. */
	static size_t FindMemberDefinitionLineInText(
		std::string_view source,
		std::string_view handlerName,
		std::string_view qualifiedClassName = {});
	static size_t FindMemberDefinitionLine(
		const std::wstring& sourcePath,
		const std::wstring& handlerName,
		const std::wstring& qualifiedClassName = {});
	static std::wstring QuoteArgument(const std::wstring& value);
	static SourceCodeNavigationPlan BuildPlan(
		SourceCodeEditorKind editor,
		std::wstring executable,
		const std::wstring& sourcePath,
		size_t line,
		const std::wstring& customArguments = {});
	static SourceCodeNavigationPlan ResolvePlan(
		const std::wstring& sourcePath,
		size_t line);
	static bool Open(
		HWND owner,
		const std::wstring& sourcePath,
		size_t line,
		SourceCodeNavigationResult* outResult = nullptr,
		std::wstring* outError = nullptr);
};
