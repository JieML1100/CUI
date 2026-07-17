#pragma once

#include "DesignDocument.h"

#include <functional>
#include <memory>
#include <string>

namespace DesignerModel
{
struct XamlDocumentParseOptions
{
	/** Optional real-control probe for custom elements and their properties. */
	std::function<std::unique_ptr<Control>(const DesignNode&)> CustomControlFactory;
};

/**
 * Parses CUI's compact, XAML-style authoring format into the canonical
 * DesignDocument model. It is deliberately a frontend only: materialization,
 * binding, styles, events, code generation, and persistence continue to use
 * the same neutral document pipeline.
 */
class XamlDocumentParser final
{
public:
	/** Transactional: output is unchanged when parsing or validation fails. */
	static bool FromXaml(
		const std::string& xaml,
		DesignDocument& output,
		std::wstring* outError = nullptr);
	static bool FromXaml(
		const std::string& xaml,
		DesignDocument& output,
		const XamlDocumentParseOptions& options,
		std::wstring* outError = nullptr);

	/** Reads a UTF-8 XAML file and applies the same transactional semantics. */
	static bool LoadFromFile(
		const std::wstring& filePath,
		DesignDocument& output,
		std::wstring* outError = nullptr);
	static bool LoadFromFile(
		const std::wstring& filePath,
		DesignDocument& output,
		const XamlDocumentParseOptions& options,
		std::wstring* outError = nullptr);
};
}
