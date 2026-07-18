#pragma once

#include "DesignDocument.h"

#include <functional>
#include <cstddef>
#include <memory>
#include <string>

namespace DesignerModel
{
struct XamlDocumentParseOptions
{
	/** Optional real-control probe for custom elements and their properties. */
	std::function<std::unique_ptr<Control>(const DesignNode&)> CustomControlFactory;
	/** Optional directory used to resolve relative image/resource URIs. */
	std::wstring ResourceBasePath;
	/** Optional per-load context; defaults to an Application resolver snapshot. */
	std::shared_ptr<ResourceLoadContext> Resources;
};

/** Structured syntax or semantic source diagnostic produced by the XAML frontend. */
struct XamlDocumentDiagnostic
{
	static constexpr std::size_t UnknownOffset = static_cast<std::size_t>(-1);

	std::wstring Message;
	/** 1-based source line and Unicode-column coordinates. */
	std::size_t Line = 0;
	std::size_t Column = 0;
	/** Zero-based UTF-16 offset for direct navigation in the Windows editor. */
	std::size_t Utf16Offset = UnknownOffset;

	bool HasLocation() const noexcept { return Line != 0 && Column != 0; }
	bool HasSourceOffset() const noexcept { return Utf16Offset != UnknownOffset; }
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
};
}
