#pragma once

#include "DesignDocument.h"

#include <cstddef>
#include <memory>
#include <string>

namespace DesignerModel
{
struct XamlDocumentParseOptions
{
	/** Optional directory used to resolve relative image/resource URIs. */
	std::wstring ResourceBasePath;
	/** Optional per-load context; defaults to an Application resolver snapshot. */
	std::shared_ptr<ResourceLoadContext> Resources;
	/**
	 * Source spans (DesignNode::Source, XamlDocumentSourceMap) only serve
	 * editor-facing features: designer selection sync, squiggles, and code
	 * generation diagnostics. Runtime hosts that merely materialize a document
	 * can clear this to skip the per-element tag index and the per-attribute
	 * span maps, which dominate document memory on large windows. Failure
	 * diagnostics stay accurate: the index is still built whenever a caller
	 * passes outDiagnostic.
	 */
	bool CaptureSourceSpans = true;
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
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool FromXaml(
		const std::string& xaml,
		DesignDocument& output,
		const XamlDocumentParseOptions& options,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);

	/**
	 * Parses a standalone ResourceDictionary into the same canonical document
	 * resource model used by Window.Resources. No synthetic Window markup is
	 * introduced, so framework themes and application dictionaries share one
	 * frontend and one validation path.
	 */
	static bool FromResourceDictionary(
		const std::string& xaml,
		DesignDocument& output,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool FromResourceDictionary(
		const std::string& xaml,
		DesignDocument& output,
		const XamlDocumentParseOptions& options,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);

	/** Reads a UTF-8 XAML file and applies the same transactional semantics. */
	static bool LoadFromFile(
		const std::wstring& filePath,
		DesignDocument& output,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool LoadFromFile(
		const std::wstring& filePath,
		DesignDocument& output,
		const XamlDocumentParseOptions& options,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);

	/** Reads a UTF-8 standalone ResourceDictionary transactionally. */
	static bool LoadResourceDictionary(
		const std::wstring& filePath,
		DesignDocument& output,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool LoadResourceDictionary(
		const std::wstring& filePath,
		DesignDocument& output,
		const XamlDocumentParseOptions& options,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
};
}
