#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace DesignerModel
{
inline constexpr unsigned int FrameworkThemeCodeGenerationContractVersion = 23;

struct FrameworkThemeCodeGenerationOptions
{
	/** Extensionless output base for the public .g.h/.g.cpp pair. */
	std::wstring OutputBasePath;
	/** Qualified provider type; defaults to CuiGeneratedFrameworkTheme. */
	std::wstring ClassName = L"CuiGeneratedFrameworkTheme";
	/**
	 * Application XAML roots used to compute the production theme closure.
	 * Empty preserves the complete ResourceDictionary for tooling and shared
	 * fallback builds; a non-empty list emits only transitively reachable
	 * styles, templates, and resources.
	 */
	std::vector<std::wstring> RootDocuments;
	/** Controls created outside XAML, expressed as built-in XAML type names. */
	std::vector<std::wstring> PreservedTypes;
	/** Theme resources reached only from handwritten/native factory code. */
	std::vector<std::wstring> PreservedResources;
};

struct FrameworkThemeCodeGenerationResult
{
	std::wstring ResourceDictionaryPath;
	std::wstring OutputBasePath;
	std::wstring ClassName;
	std::wstring GeneratedHeaderPath;
	std::wstring GeneratedSourcePath;
	std::wstring ProgramHeaderPath;
	std::wstring ProgramSourcePath;
	std::vector<std::wstring> RootDocuments;
	bool ApplicationClosure = false;
	std::size_t SourceStyleRuleCount = 0;
	std::size_t RetainedStyleRuleCount = 0;
	std::size_t SourceControlTemplateCount = 0;
	std::size_t RetainedControlTemplateCount = 0;
	std::size_t SourceResourceCount = 0;
	std::size_t RetainedResourceCount = 0;

	std::vector<std::wstring> OutputFiles() const;
	bool IsApplicationClosure() const noexcept
	{
		return ApplicationClosure;
	}
};

/**
 * Build-time compiler for the framework ResourceDictionary.
 *
 * The generated provider and immutable ThemeProgram only consume CUI runtime
 * types. XAML parsing, DesignerModel, XmlLite, and CuiRuntime stay on the
 * producer side of this boundary.
 */
class FrameworkThemeCodeGenerator final
{
public:
	static bool GenerateFile(
		const std::wstring& resourceDictionaryPath,
		const FrameworkThemeCodeGenerationOptions& options,
		FrameworkThemeCodeGenerationResult* outResult = nullptr,
		std::wstring* outError = nullptr);
};
}
