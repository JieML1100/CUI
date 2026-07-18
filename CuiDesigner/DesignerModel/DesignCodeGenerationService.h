#pragma once

#include "DesignDocument.h"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace DesignerModel
{
/** Bump together with CuiCodeGen.targets when generated output semantics change. */
inline constexpr unsigned int DesignCodeGenerationContractVersion = 8;

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

/** Validated intent shown by the interactive export workflow before writing. */
struct DesignCodeExportPlan
{
	DesignCodeBehindModel Association;
	bool CreatesAssociation = false;
	bool MigratesClass = false;
	bool ChangesRelativeOutput = false;
};

enum class DesignCodeFreshnessState
{
	Unassociated,
	Current,
	Stale,
	Missing,
	Blocked,
};

/** Exact, read-only comparison between a document and its five-file plan. */
struct DesignCodeFreshnessResult
{
	DesignCodeFreshnessState State =
		DesignCodeFreshnessState::Unassociated;
	DesignCodeGenerationResult Target;
	std::vector<std::wstring> MissingFiles;
	std::vector<std::wstring> ChangedFiles;
	std::wstring Diagnostic;
};

enum class DesignEventHandlerCodeState : unsigned char
{
	SourceMissing,
	DefinitionMissing,
	Current,
	SignatureMismatch,
	DuplicateDefinition,
};

struct DesignEventHandlerCodeEntry
{
	std::wstring HandlerName;
	std::wstring ParameterList;
	DesignEventHandlerCodeState State =
		DesignEventHandlerCodeState::DefinitionMissing;
	size_t DefinitionCount = 0;
	/** Preferred definition target for navigation (header inline or source). */
	std::wstring DefinitionFilePath;
	size_t DefinitionLine = 0;
	std::wstring Diagnostic;
};

/** Read-only, per-handler projection of the current user .h/.cpp pair. */
struct DesignEventHandlerCodeInspection
{
	bool Associated = false;
	/** UI hosts may mark an old snapshot pending while a debounced scan runs. */
	bool Pending = false;
	DesignCodeGenerationResult Target;
	std::map<std::wstring, DesignEventHandlerCodeEntry> Handlers;
	/** Existing header/source members grouped by the catalog signature they match. */
	std::map<std::string, std::vector<std::wstring>> CompatibleUserHandlers;
	std::wstring Diagnostic;
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
	using CommitCallback = std::function<bool(
		const DesignCodeGenerationResult& result,
		std::wstring& error)>;

	/** Builds the portable x:Class/d:CodeBehind pair before any code is written. */
	static bool BuildCodeBehindAssociation(
		const std::wstring& className,
		const std::wstring& outputBasePath,
		const std::wstring& designFilePath,
		DesignCodeBehindModel& association,
		std::wstring* outError = nullptr);

	static bool BuildCodeExportPlan(
		const DesignCodeBehindModel& existingAssociation,
		const std::wstring& requestedClassName,
		const std::wstring& outputBasePath,
		const std::wstring& designFilePath,
		DesignCodeExportPlan& plan,
		std::wstring* outError = nullptr);

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

	/**
	 * Generates the five-file set and then commits an external association.
	 * A callback failure restores the exact pre-generation file set.
	 */
	static bool GenerateAndCommit(
		const DesignDocument& document,
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options,
		const CommitCallback& commit,
		DesignCodeGenerationResult* outResult = nullptr,
		std::wstring* outError = nullptr);

	/** Computes freshness without creating directories or modifying any file. */
	static bool InspectFreshness(
		const DesignDocument& document,
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options,
		DesignCodeFreshnessResult& freshness,
		std::wstring* outError = nullptr);

	/** Inspects each referenced handler using the generator's exact token rules. */
	static bool InspectEventHandlers(
		const DesignDocument& document,
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options,
		DesignEventHandlerCodeInspection& inspection,
		std::wstring* outError = nullptr);

	static bool GenerateFile(
		const std::wstring& designFilePath,
		const DesignCodeGenerationOptions& options = {},
		DesignCodeGenerationResult* outResult = nullptr,
		std::wstring* outError = nullptr);
};
}
