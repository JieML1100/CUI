#pragma once

#include "DesignerTypes.h"
#include <string>
#include <vector>

namespace DesignerDataContextSchemaUtils
{
	std::wstring NormalizePath(const std::wstring& path);
	bool IsValidPath(const std::wstring& path);
	const wchar_t* ValueKindName(BindingValueKind kind) noexcept;
	bool TryParseValueKind(const std::wstring& text, BindingValueKind& kind);

	const DesignerDataContextProperty* Find(
		const DesignerDataContextSchema& schema,
		const std::wstring& path);
	std::vector<std::wstring> GetPaths(const DesignerDataContextSchema& schema);
	void Canonicalize(DesignerDataContextSchema& schema);
	bool Validate(const DesignerDataContextSchema& schema, std::wstring* outError = nullptr);
	std::wstring Describe(const DesignerDataContextProperty& property);
	/** Recursively discovers dotted paths from runtime IBindingSource metadata. */
	bool BuildFromBindingSource(
		const IBindingSource& source,
		DesignerDataContextSchema& schema,
		std::wstring* outError = nullptr,
		size_t maxDepth = 16);
}
