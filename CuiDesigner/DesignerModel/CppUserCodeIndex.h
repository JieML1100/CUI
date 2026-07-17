#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace DesignerModel
{
struct CppUserHandlerDefinitionInspection
{
	size_t DefinitionCount = 0;
	size_t CompatibleDefinitionCount = 0;
	size_t IncompatibleShapeDefinitionCount = 0;
	size_t DeletedCompatibleDefinitionCount = 0;
	size_t FirstDefinitionLine = 0;
	size_t FirstCompatibleDefinitionLine = 0;
};

struct CppUserClassDefinitionInspection
{
	size_t DefinitionCount = 0;
	size_t CompatibleGeneratedBaseCount = 0;
	size_t FirstDefinitionLine = 0;
	size_t FirstCompatibleDefinitionLine = 0;
};

/**
 * Token-based, comment/literal-safe index of member bodies, including
 * out-of-class definitions and inline void handlers in the exact class body.
 * Fully qualified definitions and definitions inside traditional or C++17
 * nested namespace blocks resolve to the same canonical class identity.
 * Preprocessor directive lines never contribute tokens; branches guarded by
 * a definite literal #if 0/#if 1 condition are filtered while source offsets
 * and line numbers remain stable. Unknown build conditions are retained
 * conservatively instead of guessing the caller's macro environment.
 * The same namespace/preprocessor surface indexes the exact user class body
 * and verifies its expected sibling Generated base, including export macros,
 * final classes, access specifiers, and multiple direct bases.
 * Default constructors defined inline (including = default and = delete) and
 * out of class share one inspection result, so code generation can enforce a
 * single usable construction path across the user header and source.
	 * Handler matching deliberately mirrors generated event declarations:
	 * parameter names and whitespace may differ, parameter types may not, and
	 * the definition must be a non-static, non-cv/ref void member that can
	 * actually override the generated virtual hook.
 */
class CppUserCodeIndex final
{
public:
	static bool Build(
		std::string_view source,
		std::string_view qualifiedClassName,
		CppUserCodeIndex& index,
		std::wstring* outError = nullptr);

	[[nodiscard]] CppUserHandlerDefinitionInspection InspectHandler(
		std::string_view handlerName,
		std::string_view generatedParameterList) const;
	[[nodiscard]] CppUserHandlerDefinitionInspection InspectConstructor(
		std::string_view generatedParameterList = {}) const;
	/** Inspects the exact x:Class body and its expected sibling Generated base. */
	[[nodiscard]] CppUserClassDefinitionInspection
		InspectGeneratedClassDefinition() const;

	/**
	 * Returns source-defined member names that have exactly one compatible
	 * definition. Names with duplicate compatible bodies are deliberately
	 * omitted so the Designer never offers an ambiguous event target.
	 */
	[[nodiscard]] std::vector<std::string> FindCompatibleHandlerNames(
		std::string_view generatedParameterList) const;

	/** Rewrites only the token of one unique compatible member definition. */
	bool TryRenameUniqueCompatibleHandler(
		std::string_view source,
		std::string_view oldName,
		std::string_view newName,
		std::string_view generatedParameterList,
		std::string& rewrittenSource,
		std::wstring* outError = nullptr) const;

private:
	using ParameterTokens = std::vector<std::vector<std::string>>;
	struct Definition
	{
		std::string Name;
		ParameterTokens Parameters;
		bool CompatibleHandlerShape = true;
		bool Deleted = false;
		size_t Line = 0;
		size_t Position = 0;
	};
	struct ClassDefinition
	{
		bool DerivesGeneratedBase = false;
		size_t Line = 0;
	};

	std::string _qualifiedClassName;
	std::vector<Definition> _definitions;
	std::vector<Definition> _constructors;
	std::vector<ClassDefinition> _classDefinitions;
};
}
