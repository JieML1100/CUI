#pragma once

#include "DesignDocument.h"

#include <string>

namespace DesignerModel
{
/** Canonical writer for CUI's readable XAML dialect. */
class XamlDocumentSerializer final
{
public:
	/** Serializes without mutating the source document. Throws on invalid topology. */
	static std::string ToXaml(const DesignDocument& document);

	/** Uses sibling-temporary, flush, and atomic replacement semantics. */
	static bool SaveToFile(
		const DesignDocument& document,
		const std::wstring& filePath,
		std::wstring* outError = nullptr);
};
}
