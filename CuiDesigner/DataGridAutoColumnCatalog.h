#pragma once

#include "DesignerModel/DesignDocument.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DesignerModel
{
enum class DataGridAutoColumnRuleAction : unsigned char
{
	Transform,
	Suppress,
};

/**
 * One build-time transformation of the default column inferred from a static
 * DataType property. Empty GridName applies to every DataGrid using the type;
 * an exact named rule takes precedence over the type-wide rule.
 */
struct DataGridAutoColumnRule final
{
	std::wstring DataType;
	std::wstring Property;
	std::wstring GridName;
	DataGridAutoColumnRuleAction Action =
		DataGridAutoColumnRuleAction::Transform;
	std::optional<DesignDataGridColumnKind> Kind;
	std::optional<std::wstring> Header;
	std::optional<DesignDataGridLength> Width;
	std::optional<bool> IsReadOnly;
	std::optional<bool> IsThreeState;
	std::optional<bool> CanUserSort;
	std::optional<bool> CanUserResize;
	std::optional<bool> CanUserReorder;
	std::optional<DesignDataGridColumnVisibility> Visibility;
	std::optional<std::wstring> SortMemberPath;

	bool operator==(const DataGridAutoColumnRule&) const = default;
};

/**
 * Strict XML-backed DataGrid auto-column catalog consumed only by CuiCodeGen.
 * It transforms the immutable design model before native C++ is emitted and
 * is never linked into, parsed by, or consulted from Production.
 */
class DataGridAutoColumnCatalog final
{
public:
	static constexpr unsigned int CurrentManifestVersion = 1;

	static bool FromXml(
		std::string_view xml,
		DataGridAutoColumnCatalog& output,
		std::wstring* outError = nullptr);
	static bool LoadFile(
		const std::wstring& path,
		DataGridAutoColumnCatalog& output,
		std::wstring* outError = nullptr);

	bool Empty() const noexcept { return _rules.empty(); }
	std::size_t Size() const noexcept { return _rules.size(); }
	const std::vector<DataGridAutoColumnRule>& Rules() const noexcept
	{
		return _rules;
	}
	const DataGridAutoColumnRule* Find(
		std::wstring_view dataType,
		std::wstring_view property,
		std::wstring_view gridName = {}) const noexcept;

private:
	std::vector<DataGridAutoColumnRule> _rules;
};
}
