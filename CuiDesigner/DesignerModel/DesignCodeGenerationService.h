#pragma once

#include "DesignDocument.h"

#include <string>
#include <vector>

namespace DesignerModel
{
struct DesignCodeGenerationOptions
{
	/** Optional extensionless output base. Relative values use the process CWD. */
	std::wstring OutputBasePath;
	/** Optional qualified C++ class override; otherwise x:Class is required. */
	std::wstring ClassName;
};

struct DesignCodeGenerationResult
{
	std::wstring DesignFilePath;
	std::wstring OutputBasePath;
	std::wstring ClassName;
	std::wstring UserHeaderPath;
	std::wstring UserSourcePath;
	std::wstring GeneratedHeaderPath;
	std::wstring GeneratedSourcePath;
	std::wstring HandlerDeclarationsPath;

	std::vector<std::wstring> OutputFiles() const;
};

/**
 * Headless orchestration shared by the Designer and CuiCodeGen.exe.
 *
 * Parsing, code-behind path resolution, document materialization, and the
 * generator's atomic multi-file commit all run without creating a Form/HWND.
 */
class DesignCodeGenerationService final
{
public:
	static bool LoadDocument(
		const std::wstring& designFilePath,
		DesignDocument& document,
		std::wstring* outError = nullptr);

	static bool Generate(
		const DesignDocument& document,
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options,
		DesignCodeGenerationResult* outResult = nullptr,
		std::wstring* outError = nullptr);

	static bool GenerateFile(
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options = {},
		DesignCodeGenerationResult* outResult = nullptr,
		std::wstring* outError = nullptr);
};
}
