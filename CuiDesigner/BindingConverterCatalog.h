#pragma once

#include "../CUI/include/Binding.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace DesignerModel
{
/** Converter ABI selected by a build-time AOT converter manifest entry. */
enum class BindingConverterCatalogKind : std::uint8_t
{
	Single,
	Multi,
};

/**
 * One immutable, name-to-C++ conversion contract consumed by CuiCodeGen.
 *
 * FactorySymbol is a qualified free-function name without parentheses. The
 * generated translation unit calls it directly and assigns its result to the
 * appropriate shared_ptr interface, allowing the C++ compiler to verify the
 * declared ABI without a runtime name registry.
 */
struct BindingConverterCatalogEntry final
{
	std::wstring Id;
	BindingConverterCatalogKind Kind = BindingConverterCatalogKind::Single;
	std::string Include;
	std::string FactorySymbol;
	BindingValueKind SourceKind = BindingValueKind::Empty;
	BindingValueKind TargetKind = BindingValueKind::Empty;
	std::size_t MinimumInputCount = 0;
	bool CanConvertBack = false;

	std::string FactoryCallExpression() const;
	bool operator==(const BindingConverterCatalogEntry&) const = default;
};

/**
 * Strict XML-backed catalog for application-owned AOT binding converters.
 * The catalog is a build-time input only; it is never linked into Production.
 */
class BindingConverterCatalog final
{
public:
	static constexpr unsigned int CurrentManifestVersion = 1;

	/** Parses a complete UTF-8 manifest without modifying output on failure. */
	static bool FromXml(
		std::string_view xml,
		BindingConverterCatalog& output,
		std::wstring* outError = nullptr);
	/** Loads and parses one manifest file without modifying output on failure. */
	static bool LoadFile(
		const std::wstring& path,
		BindingConverterCatalog& output,
		std::wstring* outError = nullptr);

	bool Empty() const noexcept { return _entries.empty(); }
	std::size_t Size() const noexcept { return _entries.size(); }
	const std::vector<BindingConverterCatalogEntry>& Entries() const noexcept
	{
		return _entries;
	}
	/** Case-insensitive lookup using the same authored-ID semantics as Design. */
	const BindingConverterCatalogEntry* Find(std::wstring_view id) const noexcept;
	const BindingConverterCatalogEntry* Find(
		std::wstring_view id,
		BindingConverterCatalogKind kind) const noexcept;
	/** Stable first-use order with duplicate include directives removed. */
	std::vector<std::string> Includes() const;

private:
	std::vector<BindingConverterCatalogEntry> _entries;
};
}
