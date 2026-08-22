#include "DesignDocument.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/GroupStyle.h"
#include "../../CUI/include/RichTextDocument.h"
#include <Convert.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace DesignerModel
{
DesignDocument::DesignDocument()
{
	Window.Name = L"MainWindow";
	Window.Type = UIClass::UI_Window;
	Window.XamlType = { L"urn:cui", L"Window" };
}

void XamlDocumentDiagnostic::Apply(const XamlSourceSpan& span) noexcept
{
	if (!span.Valid()) return;
	Line = span.Line;
	Column = span.Column;
	EndLine = span.EndLine;
	EndColumn = span.EndColumn;
	Utf16Offset = span.Utf16Offset;
	Utf16Length = span.Utf16Length;
}

void DesignNodeSourceInfo::RecordMember(
	std::wstring name, XamlSourceSpan span)
{
	if (name.empty() || !span.Valid()) return;
	Members.insert_or_assign(std::move(name), std::move(span));
}

const XamlSourceSpan* DesignNodeSourceInfo::FindMember(
	const std::wstring& name) const noexcept
{
	const auto found = Members.find(name);
	return found == Members.end() ? nullptr : &found->second;
}

void XamlDocumentSourceMap::RecordSymbol(
	std::wstring symbol, XamlSourceSpan span)
{
	if (symbol.empty() || !span.Valid()) return;
	Symbols.try_emplace(std::move(symbol), std::move(span));
}

const XamlSourceSpan* XamlDocumentSourceMap::FindSymbol(
	const std::wstring& symbol) const noexcept
{
	const auto found = Symbols.find(symbol);
	return found == Symbols.end() ? nullptr : &found->second;
}

const XamlSourceSpan* XamlDocumentSourceMap::FindMentionedSymbol(
	const std::wstring& message,
	std::wstring* matchedSymbol) const noexcept
{
	const XamlSourceSpan* best = nullptr;
	std::wstring bestName;
	auto lower = [](std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	};
	const auto normalizedMessage = lower(message);
	for (const auto& [symbol, span] : Symbols)
	{
		if (symbol.size() <= bestName.size()
			|| normalizedMessage.find(lower(symbol)) == std::wstring::npos) continue;
		best = &span;
		bestName = symbol;
	}
	if (matchedSymbol) *matchedSymbol = std::move(bestName);
	return best;
}

bool DesignCodeBehindModel::TryNormalizeRelativeBasePath(
	const std::wstring& value,
	std::wstring& normalized,
	std::wstring* outError)
{
	normalized.clear();
	if (outError) outError->clear();
	if (value.empty()) return true;
	try
	{
		const std::filesystem::path input(value);
		if (input.empty() || input.is_absolute()
			|| input.has_root_name() || input.has_root_directory())
		{
			if (outError) *outError = L"代码导出关联必须是相对于设计文件的路径。";
			return false;
		}
		const auto path = input.lexically_normal();
		const auto fileName = path.filename().wstring();
		if (fileName.empty() || fileName == L"." || fileName == L"..")
		{
			if (outError) *outError = L"代码导出关联必须包含文件基名。";
			return false;
		}
		if (path.has_extension())
		{
			if (outError) *outError = L"代码导出关联应为不带 .h/.cpp 扩展名的基路径。";
			return false;
		}
		normalized = path.generic_wstring();
		return true;
	}
	catch (...)
	{
		if (outError) *outError = L"代码导出关联路径无效。";
		return false;
	}
}

bool DesignCodeBehindModel::TryNormalizeClassName(
	const std::wstring& value,
	std::wstring& normalized,
	std::wstring* outError)
{
	normalized.clear();
	if (outError) outError->clear();
	if (value.empty()) return true;
	std::vector<std::wstring> segments;
	size_t begin = 0;
	for (size_t position = 0; position <= value.size();)
	{
		const bool end = position == value.size();
		const bool dotted = !end && value[position] == L'.';
		const bool qualified = !end && value[position] == L':'
			&& position + 1 < value.size() && value[position + 1] == L':';
		if (!end && !dotted && !qualified)
		{
			if (value[position] == L':')
			{
				if (outError) *outError = L"x:Class 中的命名空间分隔符必须是 :: 或 .。";
				return false;
			}
			++position;
			continue;
		}
		auto segment = value.substr(begin, position - begin);
		if (segment.empty())
		{
			if (outError) *outError = L"x:Class 不能包含空命名空间段。";
			return false;
		}
		std::wstring validation;
		if (!DesignerEventCatalog::ValidateHandlerName(segment, &validation))
		{
			if (outError) *outError = L"x:Class 段 “" + segment
				+ L"” 无效：" + validation;
			return false;
		}
		segments.push_back(std::move(segment));
		if (end) break;
		position += qualified ? 2 : 1;
		begin = position;
	}
	for (const auto& segment : segments)
	{
		if (!normalized.empty()) normalized += L"::";
		normalized += segment;
	}
	return true;
}

bool DesignCodeBehindModel::Validate(std::wstring* outError) const
{
	if (outError) outError->clear();
	if (ClassName.empty())
	{
		if (RelativeBasePath.empty()) return true;
		if (outError) *outError = L"代码导出关联缺少 x:Class 类名。";
		return false;
	}
	std::wstring normalizedClass;
	if (!TryNormalizeClassName(ClassName, normalizedClass, outError)) return false;
	std::wstring normalized;
	return TryNormalizeRelativeBasePath(
		RelativeBasePath, normalized, outError);
}

bool DesignCodeBehindModel::operator==(
	const DesignCodeBehindModel& other) const
{
	return ClassName == other.ClassName
		&& RelativeBasePath == other.RelativeBasePath;
}

DesignObjectResourceDictionary::DesignObjectResourceDictionary() = default;
DesignObjectResourceDictionary::~DesignObjectResourceDictionary() = default;
DesignObjectResourceDictionary::DesignObjectResourceDictionary(
	const DesignObjectResourceDictionary&) = default;
DesignObjectResourceDictionary::DesignObjectResourceDictionary(
	DesignObjectResourceDictionary&&) noexcept = default;
DesignObjectResourceDictionary& DesignObjectResourceDictionary::operator=(
	const DesignObjectResourceDictionary&) = default;
DesignObjectResourceDictionary& DesignObjectResourceDictionary::operator=(
	DesignObjectResourceDictionary&&) noexcept = default;

bool DesignObjectResourceDictionary::Empty() const noexcept
{
	return Storyboards.empty() && Components.empty()
		&& ControlTemplates.empty() && DataTemplates.empty()
		&& ItemsPanelTemplates.empty() && GroupStyles.empty();
}

bool DesignObjectResourceDictionary::operator==(
	const DesignObjectResourceDictionary& other) const
{
	return Storyboards == other.Storyboards
		&& Components == other.Components
		&& ControlTemplates == other.ControlTemplates
		&& DataTemplates == other.DataTemplates
		&& ItemsPanelTemplates == other.ItemsPanelTemplates
		&& GroupStyles == other.GroupStyles;
}

bool DesignRelativePanelConstraints::Empty() const noexcept
{
	return !CenterHorizontal && !CenterVertical
		&& !AlignLeftWithPanel && !AlignTopWithPanel
		&& !AlignRightWithPanel && !AlignBottomWithPanel
		&& !Above && !Below && !LeftOf && !RightOf
		&& !AlignLeftWith && !AlignRightWith
		&& !AlignTopWith && !AlignBottomWith;
}

bool DesignTextFormatting::Empty() const noexcept
{
	return !Foreground && !Background
		&& !FontFamily && !Language && !FontSize
		&& !FontWeight && !FontStretch && !FontStyle
		&& !Underline && !Strikethrough;
}

bool DesignNodeStructure::Empty() const noexcept
{
	return CommandTarget.empty()
		&& ItemsSourceResource.empty() && ItemTemplate.empty()
		&& ContentTemplate.empty() && HeaderTemplate.empty()
		&& ControlTemplate.empty() && RowValidationErrorTemplate.empty()
		&& DataGridCellStyle.empty()
		&& DataGridColumnHeaderStyle.empty()
		&& DataGridRowStyle.empty()
		&& DataGridRowHeaderStyle.empty()
		&& DataGridRowHeaderTemplate.empty()
		&& DataGridRowDetailsTemplate.empty()
		&& GroupStyle.empty()
		&& ItemsPanel.empty() && ItemContainerStyle.empty()
		&& ChildRole == DesignNodeChildRole::Default
		&& (!RelativePanel || RelativePanel->Empty())
		&& !GridRows && !GridColumns
		&& !DataGridColumns
		&& !ChartSeries && !Document;
}

namespace
{
	std::string StructuralUtf8(const std::wstring& value)
	{
		return Convert::UnicodeToUtf8(value);
	}

	std::wstring StructuralWide(const DesignValue& value)
	{
		return value.is_string()
			? Convert::Utf8ToUnicode(value.get<std::string>()) : std::wstring{};
	}

	bool StructuralError(std::wstring* outError, const std::wstring& message)
	{
		if (outError) *outError = message;
		return false;
	}

	DesignValue EncodeColor(const DesignColor& color)
	{
		return DesignValue{
			{ "r", color.R }, { "g", color.G },
			{ "b", color.B }, { "a", color.A } };
	}

	bool DecodeColor(
		const DesignValue& value,
		DesignColor& output,
		std::wstring* outError)
	{
		if (!value.is_object())
			return StructuralError(outError, L"结构颜色必须是对象。");
		for (const auto* key : { "r", "g", "b", "a" })
			if (value.contains(key) && !value[key].is_number())
				return StructuralError(outError, L"结构颜色通道必须是数值。");
		output.R = value.value("r", 0.0);
		output.G = value.value("g", 0.0);
		output.B = value.value("b", 0.0);
		output.A = value.value("a", 1.0);
		if (!std::isfinite(output.R) || !std::isfinite(output.G)
			|| !std::isfinite(output.B) || !std::isfinite(output.A))
			return StructuralError(outError, L"结构颜色包含非有限数值。");
		return true;
	}

	DesignValue EncodeGridLength(const DesignGridLength& length)
	{
		const char* unit = "Auto";
		switch (length.Unit)
		{
		case DesignGridLengthUnit::Pixel: unit = "Pixel"; break;
		case DesignGridLengthUnit::Star: unit = "Star"; break;
		default: break;
		}
		return DesignValue{ { "value", length.Value }, { "unit", unit } };
	}

	bool DecodeGridLength(
		const DesignValue& value,
		DesignGridLength& output,
		std::wstring* outError)
	{
		if (!value.is_object() || !value.contains("value")
			|| !value["value"].is_number() || !value.contains("unit")
			|| !value["unit"].is_string())
			return StructuralError(outError, L"GridLength 结构无效。");
		output.Value = value["value"].get<double>();
		const auto unit = value["unit"].get<std::string>();
		if (unit == "Auto") output.Unit = DesignGridLengthUnit::Auto;
		else if (unit == "Pixel") output.Unit = DesignGridLengthUnit::Pixel;
		else if (unit == "Star") output.Unit = DesignGridLengthUnit::Star;
		else return StructuralError(outError, L"GridLength Unit 无效。");
		if (!std::isfinite(output.Value) || output.Value < 0.0)
			return StructuralError(outError, L"GridLength Value 无效。");
		return true;
	}

	DesignValue EncodeGridTracks(
		const std::vector<DesignGridTrack>& tracks,
		const char* lengthKey)
	{
		auto result = DesignValue::array();
		for (const auto& track : tracks)
		{
			DesignValue value{ { lengthKey, EncodeGridLength(track.Length) } };
			if (track.Minimum != 0.0) value["min"] = track.Minimum;
			if (track.Maximum != (std::numeric_limits<float>::max)())
				value["max"] = track.Maximum;
			result.push_back(std::move(value));
		}
		return result;
	}

	bool DecodeGridTracks(
		const DesignValue& value,
		const char* lengthKey,
		std::vector<DesignGridTrack>& output,
		std::wstring* outError)
	{
		if (!value.is_array())
			return StructuralError(outError, L"Grid 定义必须是数组。");
		output.clear();
		for (const auto& item : value.ArrayItems())
		{
			if (!item.is_object() || !item.contains(lengthKey))
				return StructuralError(outError, L"Grid 定义缺少长度。");
			DesignGridTrack track;
			if (!DecodeGridLength(item[lengthKey], track.Length, outError)) return false;
			if ((item.contains("min") && !item["min"].is_number())
				|| (item.contains("max") && !item["max"].is_number()))
				return StructuralError(outError, L"Grid Min/Max 必须是数值。");
			track.Minimum = item.value("min", 0.0);
			track.Maximum = item.value(
				"max", static_cast<double>((std::numeric_limits<float>::max)()));
			if (!std::isfinite(track.Minimum) || !std::isfinite(track.Maximum)
				|| track.Minimum < 0.0 || track.Maximum < track.Minimum)
				return StructuralError(outError, L"Grid Min/Max 范围无效。");
			output.push_back(std::move(track));
		}
		return true;
	}

	template<typename T, typename Encode>
	DesignValue EncodeArray(const std::vector<T>& values, Encode&& encode)
	{
		auto result = DesignValue::array();
		for (const auto& value : values) result.push_back(encode(value));
		return result;
	}

	template<typename T, typename Decode>
	bool DecodeArray(
		const DesignValue& value,
		std::vector<T>& output,
		Decode&& decode,
		std::wstring* outError)
	{
		if (!value.is_array())
			return StructuralError(outError, L"结构集合必须是数组。");
		output.clear();
		output.reserve(value.size());
		for (const auto& item : value.ArrayItems())
		{
			T decoded;
			if (!decode(item, decoded, outError)) return false;
			output.push_back(std::move(decoded));
		}
		return true;
	}

	DesignValue EncodeDataGridLength(const DesignDataGridLength& length)
	{
		const char* unit = "Auto";
		switch (length.Unit)
		{
		case DesignDataGridLengthUnit::SizeToHeader:
			unit = "SizeToHeader";
			break;
		case DesignDataGridLengthUnit::SizeToCells:
			unit = "SizeToCells";
			break;
		case DesignDataGridLengthUnit::Pixel: unit = "Pixel"; break;
		case DesignDataGridLengthUnit::Star: unit = "Star"; break;
		default: break;
		}
		return DesignValue{ { "value", length.Value }, { "unit", unit } };
	}

	bool DecodeDataGridLength(
		const DesignValue& value,
		DesignDataGridLength& output,
		std::wstring* outError)
	{
		if (!value.is_object() || value.size() != 2
			|| !value.contains("value") || !value["value"].is_number()
			|| !value.contains("unit") || !value["unit"].is_string())
			return StructuralError(outError, L"DataGridLength 结构无效。");
		output.Value = value["value"].get<double>();
		const auto unit = value["unit"].get<std::string>();
		if (unit == "Auto") output.Unit = DesignDataGridLengthUnit::Auto;
		else if (unit == "SizeToHeader")
			output.Unit = DesignDataGridLengthUnit::SizeToHeader;
		else if (unit == "SizeToCells")
			output.Unit = DesignDataGridLengthUnit::SizeToCells;
		else if (unit == "Pixel")
			output.Unit = DesignDataGridLengthUnit::Pixel;
		else if (unit == "Star")
			output.Unit = DesignDataGridLengthUnit::Star;
		else return StructuralError(outError, L"DataGridLength Unit 无效。");
		if (!std::isfinite(output.Value) || output.Value < 0.0)
			return StructuralError(outError, L"DataGridLength Value 无效。");
		if (output.Unit != DesignDataGridLengthUnit::Pixel
			&& output.Unit != DesignDataGridLengthUnit::Star)
		{
			if (output.Value != 1.0)
				return StructuralError(outError,
					L"描述性 DataGridLength 的 Value 必须为 1。");
			output.Value = 1.0;
		}
		return true;
	}

	bool ValidateDataGridColumnBinding(
		const DesignerDataBinding& binding,
		std::wstring* outError)
	{
		std::wstring bindingError;
		if (DesignerBindingUtils::ValidateDataGridColumnBindingSource(
			binding, nullptr, &bindingError)) return true;
		return StructuralError(outError,
			L"DataGrid 列 Binding 无效：" + bindingError);
	}

	DesignValue EncodeDataGridColumn(const DesignDataGridColumn& column)
	{
		const char* kind = "Text";
		if (column.Kind == DesignDataGridColumnKind::CheckBox)
			kind = "CheckBox";
		else if (column.Kind == DesignDataGridColumnKind::ComboBox)
			kind = "ComboBox";
		else if (column.Kind == DesignDataGridColumnKind::Hyperlink)
			kind = "Hyperlink";
		else if (column.Kind == DesignDataGridColumnKind::Template)
			kind = "Template";
		DesignValue result{
			{ "kind", kind },
			{ "width", EncodeDataGridLength(column.Width) },
			{ "hasWidth", column.HasWidth },
			{ "hasMinWidth", column.HasMinWidth },
			{ "hasMaxWidth", column.HasMaxWidth }
		};
		if (!column.Header.empty())
			result["header"] = StructuralUtf8(column.Header);
		if (!column.HeaderStyle.empty())
			result["headerStyle"] = StructuralUtf8(column.HeaderStyle);
		if (!column.HeaderTemplate.empty())
			result["headerTemplate"] = StructuralUtf8(column.HeaderTemplate);
		if (!column.CellStyle.empty())
			result["cellStyle"] = StructuralUtf8(column.CellStyle);
		if (column.Binding)
			result["binding"] = DesignerBindingUtils::WriteBindingDefinition(
				*column.Binding);
		const bool bound = column.Kind == DesignDataGridColumnKind::Text
			|| column.Kind == DesignDataGridColumnKind::CheckBox
			|| column.Kind == DesignDataGridColumnKind::ComboBox
			|| column.Kind == DesignDataGridColumnKind::Hyperlink;
		if (bound)
		{
			if (!column.ElementStyle.empty())
				result["elementStyle"] = StructuralUtf8(column.ElementStyle);
			if (!column.EditingElementStyle.empty())
				result["editingElementStyle"] = StructuralUtf8(
					column.EditingElementStyle);
		}
		else if (!column.ElementStyle.empty()
			|| !column.EditingElementStyle.empty())
			throw std::invalid_argument(
				"ElementStyle fields require a bound DataGrid column");
		if (column.HasMinWidth) result["minWidth"] = column.MinWidth;
		if (column.HasMaxWidth && std::isfinite(column.MaxWidth))
			result["maxWidth"] = column.MaxWidth;
		if (column.IsReadOnly) result["isReadOnly"] = true;
		if (column.Kind == DesignDataGridColumnKind::CheckBox)
		{
			if (column.IsThreeState) result["isThreeState"] = true;
		}
		else if (column.IsThreeState)
			throw std::invalid_argument(
				"IsThreeState is only valid for DataGridCheckBoxColumn");
		if (column.Kind == DesignDataGridColumnKind::ComboBox)
		{
			if (DesignerBindingUtils::Trim(column.ItemsSourceResource).empty())
				throw std::invalid_argument(
					"DataGridComboBoxColumn requires ItemsSourceResource");
			result["itemsSourceResource"] = StructuralUtf8(
				column.ItemsSourceResource);
			if (!column.DisplayMemberPath.empty())
				result["displayMemberPath"] = StructuralUtf8(
					column.DisplayMemberPath);
			if (!column.SelectedValuePath.empty())
				result["selectedValuePath"] = StructuralUtf8(
					column.SelectedValuePath);
			if (column.SelectionBinding
				== DesignDataGridComboBoxSelectionBinding::SelectedValue)
				result["selectionBinding"] = "SelectedValue";
		}
		else if (!column.ItemsSourceResource.empty()
			|| !column.DisplayMemberPath.empty()
			|| !column.SelectedValuePath.empty()
			|| column.SelectionBinding
				!= DesignDataGridComboBoxSelectionBinding::SelectedItem)
			throw std::invalid_argument(
				"ComboBox fields are only valid for DataGridComboBoxColumn");
		if (column.Kind == DesignDataGridColumnKind::Hyperlink)
		{
			if (column.ContentBinding)
				result["contentBinding"] =
					DesignerBindingUtils::WriteBindingDefinition(
						*column.ContentBinding);
			if (!column.TargetName.empty())
				result["targetName"] = StructuralUtf8(column.TargetName);
		}
		else if (column.ContentBinding || !column.TargetName.empty())
			throw std::invalid_argument(
				"Hyperlink fields are only valid for DataGridHyperlinkColumn");
		if (!column.CanUserSort) result["canUserSort"] = false;
		if (!column.CanUserResize) result["canUserResize"] = false;
		if (!column.CanUserReorder) result["canUserReorder"] = false;
		if (column.Visibility != DesignDataGridColumnVisibility::Visible)
			result["visibility"] = column.Visibility
				== DesignDataGridColumnVisibility::Hidden
				? "Hidden" : "Collapsed";
		if (!column.SortMemberPath.empty())
			result["sortMemberPath"] = StructuralUtf8(column.SortMemberPath);
		if (!column.CellTemplate.empty())
			result["cellTemplate"] = StructuralUtf8(column.CellTemplate);
		if (!column.CellEditingTemplate.empty())
			result["cellEditingTemplate"] = StructuralUtf8(
				column.CellEditingTemplate);
		return result;
	}

	bool DecodeDataGridColumn(
		const DesignValue& value,
		DesignDataGridColumn& output,
		std::wstring* outError)
	{
		if (!value.is_object())
			return StructuralError(outError, L"DataGrid 列必须是对象。");
		for (const auto& [key, ignored] : value.ObjectItems())
		{
			(void)ignored;
			if (key != "kind" && key != "header"
				&& key != "headerStyle" && key != "headerTemplate"
				&& key != "cellStyle" && key != "binding"
				&& key != "elementStyle" && key != "editingElementStyle"
				&& key != "contentBinding" && key != "targetName"
				&& key != "width" && key != "minWidth" && key != "maxWidth"
				&& key != "hasWidth" && key != "hasMinWidth"
				&& key != "hasMaxWidth"
				&& key != "isReadOnly" && key != "isThreeState"
				&& key != "itemsSourceResource"
				&& key != "displayMemberPath"
				&& key != "selectedValuePath"
				&& key != "selectionBinding"
				&& key != "canUserSort" && key != "canUserResize"
				&& key != "canUserReorder"
				&& key != "visibility"
				&& key != "sortMemberPath" && key != "cellTemplate"
				&& key != "cellEditingTemplate")
				return StructuralError(outError,
					L"DataGrid 列包含未知字段："
					+ Convert::Utf8ToUnicode(key));
		}
		if (!value.contains("kind") || !value["kind"].is_string()
			|| !value.contains("width"))
			return StructuralError(outError,
				L"DataGrid 列缺少 Kind 或 Width。");
		const auto kind = value["kind"].get<std::string>();
		if (kind == "Text") output.Kind = DesignDataGridColumnKind::Text;
		else if (kind == "CheckBox")
			output.Kind = DesignDataGridColumnKind::CheckBox;
		else if (kind == "ComboBox")
			output.Kind = DesignDataGridColumnKind::ComboBox;
		else if (kind == "Hyperlink")
			output.Kind = DesignDataGridColumnKind::Hyperlink;
		else if (kind == "Template")
			output.Kind = DesignDataGridColumnKind::Template;
		else return StructuralError(outError, L"DataGrid 列 Kind 无效。");
		if (!DecodeDataGridLength(value["width"], output.Width, outError))
			return false;

		auto readString = [&](const char* key, std::wstring& result) -> bool
		{
			if (!value.contains(key)) return true;
			if (!value[key].is_string())
				return StructuralError(outError,
					L"DataGrid 列字符串字段类型无效："
					+ Convert::Utf8ToUnicode(key));
			result = StructuralWide(value[key]);
			return true;
		};
		if (!readString("header", output.Header)
			|| !readString("headerStyle", output.HeaderStyle)
			|| !readString("headerTemplate", output.HeaderTemplate)
			|| !readString("cellStyle", output.CellStyle)
			|| !readString("elementStyle", output.ElementStyle)
			|| !readString("editingElementStyle", output.EditingElementStyle)
			|| !readString("sortMemberPath", output.SortMemberPath)
			|| !readString("targetName", output.TargetName)
			|| !readString("itemsSourceResource", output.ItemsSourceResource)
			|| !readString("displayMemberPath", output.DisplayMemberPath)
			|| !readString("selectedValuePath", output.SelectedValuePath)
			|| !readString("cellTemplate", output.CellTemplate)
			|| !readString("cellEditingTemplate", output.CellEditingTemplate))
			return false;

		if ((value.contains("minWidth") && !value["minWidth"].is_number())
			|| (value.contains("maxWidth") && !value["maxWidth"].is_number())
			|| (value.contains("hasWidth")
				&& !value["hasWidth"].is_boolean())
			|| (value.contains("hasMinWidth")
				&& !value["hasMinWidth"].is_boolean())
			|| (value.contains("hasMaxWidth")
				&& !value["hasMaxWidth"].is_boolean())
			|| (value.contains("isReadOnly")
				&& !value["isReadOnly"].is_boolean())
			|| (value.contains("isThreeState")
				&& !value["isThreeState"].is_boolean())
			|| (value.contains("canUserSort")
				&& !value["canUserSort"].is_boolean())
			|| (value.contains("canUserResize")
				&& !value["canUserResize"].is_boolean())
			|| (value.contains("canUserReorder")
				&& !value["canUserReorder"].is_boolean()))
			return StructuralError(outError,
				L"DataGrid 列数值或布尔字段类型无效。");
		output.MinWidth = value.value("minWidth", 20.0);
		output.MaxWidth = value.value("maxWidth",
			(std::numeric_limits<double>::infinity)());
		// Documents predating M7 always emitted all three setters, including
		// implicit defaults. Missing flags therefore preserve that old meaning.
		output.HasWidth = value.value("hasWidth", true);
		output.HasMinWidth = value.value("hasMinWidth", true);
		output.HasMaxWidth = value.value("hasMaxWidth", true);
		if (output.HasMinWidth && !value.contains("minWidth"))
			output.MinWidth = 20.0;
		if (output.HasMaxWidth && !value.contains("maxWidth"))
			output.MaxWidth = (std::numeric_limits<double>::infinity)();
		output.IsReadOnly = value.value("isReadOnly", false);
		output.IsThreeState = value.value("isThreeState", false);
		output.SelectionBinding =
			DesignDataGridComboBoxSelectionBinding::SelectedItem;
		if (value.contains("selectionBinding"))
		{
			if (!value["selectionBinding"].is_string())
				return StructuralError(outError,
					L"DataGridComboBoxColumn SelectionBinding 类型无效。");
			const auto selection = value["selectionBinding"].get<std::string>();
			if (selection == "SelectedItem")
				output.SelectionBinding =
					DesignDataGridComboBoxSelectionBinding::SelectedItem;
			else if (selection == "SelectedValue")
				output.SelectionBinding =
					DesignDataGridComboBoxSelectionBinding::SelectedValue;
			else return StructuralError(outError,
				L"DataGridComboBoxColumn SelectionBinding 无效。");
		}
		output.CanUserSort = value.value("canUserSort", true);
		output.CanUserResize = value.value("canUserResize", true);
		output.CanUserReorder = value.value("canUserReorder", true);
		output.Visibility = DesignDataGridColumnVisibility::Visible;
		if (value.contains("visibility"))
		{
			if (!value["visibility"].is_string())
				return StructuralError(outError,
					L"DataGrid 列 Visibility 类型无效。");
			const auto visibility = value["visibility"].get<std::string>();
			if (visibility == "Visible")
				output.Visibility = DesignDataGridColumnVisibility::Visible;
			else if (visibility == "Hidden")
				output.Visibility = DesignDataGridColumnVisibility::Hidden;
			else if (visibility == "Collapsed")
				output.Visibility = DesignDataGridColumnVisibility::Collapsed;
			else return StructuralError(outError,
				L"DataGrid 列 Visibility 无效。");
		}
		if (!std::isfinite(output.MinWidth) || std::isnan(output.MaxWidth)
			|| output.MinWidth < 0.0 || output.MaxWidth < output.MinWidth)
			return StructuralError(outError,
				L"DataGrid 列 MinWidth/MaxWidth 范围无效。");
		if (value.contains("maxWidth") && !std::isfinite(output.MaxWidth))
			return StructuralError(outError,
				L"显式 DataGrid 列 MaxWidth 必须是有限数值。");
		if (!output.SortMemberPath.empty()
			&& !DesignerBindingUtils::IsValidSourcePath(output.SortMemberPath))
			return StructuralError(outError,
				L"DataGrid 列 SortMemberPath 无效。");

		auto readBinding = [&](const char* key,
			std::optional<DesignerDataBinding>& result) -> bool
		{
			if (!value.contains(key)) return true;
			const auto& bindingValue = value[key];
			const std::wstring fieldName = Convert::Utf8ToUnicode(key);
			if (!bindingValue.is_object())
				return StructuralError(outError,
					L"DataGrid 列 " + fieldName + L" 必须是对象。");
			for (const auto& [propertyKey, ignored] : bindingValue.ObjectItems())
			{
				(void)ignored;
				if (propertyKey != "source" && propertyKey != "kind"
					&& propertyKey != "bindings" && propertyKey != "mode"
					&& propertyKey != "updateMode"
					&& propertyKey != "converter"
					&& propertyKey != "elementName"
					&& propertyKey != "relativeSource"
					&& propertyKey != "ancestorType"
					&& propertyKey != "ancestorTypeNamespace"
					&& propertyKey != "ancestorLevel"
					&& propertyKey != "stringFormat"
					&& propertyKey != "fallbackValue"
					&& propertyKey != "fallbackValueKind"
					&& propertyKey != "targetNullValue"
					&& propertyKey != "targetNullValueKind"
					&& propertyKey != "converterParameter"
					&& propertyKey != "converterParameterKind")
					return StructuralError(outError,
						L"DataGrid 列 " + fieldName + L" 包含未知字段："
						+ Convert::Utf8ToUnicode(propertyKey));
			}
			const bool multiDefinition = bindingValue.value(
				"kind", std::string{}) == "MultiBinding"
				|| bindingValue.contains("bindings");
			if ((!multiDefinition && (!bindingValue.contains("source")
					|| !bindingValue["source"].is_string()))
				|| (bindingValue.contains("kind")
					&& !bindingValue["kind"].is_string())
				|| (bindingValue.contains("bindings")
					&& !bindingValue["bindings"].is_array())
				|| (bindingValue.contains("mode")
					&& !bindingValue["mode"].is_number_integer())
				|| (bindingValue.contains("updateMode")
					&& !bindingValue["updateMode"].is_number_integer())
				|| (bindingValue.contains("converter")
					&& !bindingValue["converter"].is_string())
				|| (bindingValue.contains("elementName")
					&& !bindingValue["elementName"].is_string())
				|| (bindingValue.contains("relativeSource")
					&& !bindingValue["relativeSource"].is_string())
				|| (bindingValue.contains("ancestorType")
					&& !bindingValue["ancestorType"].is_string())
				|| (bindingValue.contains("ancestorTypeNamespace")
					&& !bindingValue["ancestorTypeNamespace"].is_string())
				|| (bindingValue.contains("ancestorLevel")
					&& !bindingValue["ancestorLevel"].is_number_integer())
				|| (bindingValue.contains("stringFormat")
					&& !bindingValue["stringFormat"].is_string()))
				return StructuralError(outError,
					L"DataGrid 列 " + fieldName + L" 字段类型无效。");
			for (const auto& [literal, literalKind] : {
				std::pair{ "fallbackValue", "fallbackValueKind" },
				std::pair{ "targetNullValue", "targetNullValueKind" },
				std::pair{ "converterParameter", "converterParameterKind" } })
				if (bindingValue.contains(literalKind)
					&& !bindingValue.contains(literal))
					return StructuralError(outError,
						L"DataGrid 列 " + fieldName
						+ L" 字面量类型缺少对应值。");
			DesignerDataBinding binding;
			std::wstring bindingError;
			if (!DesignerBindingUtils::TryReadBindingDefinition(
				bindingValue, binding, &bindingError))
				return StructuralError(outError,
					L"DataGrid 列 " + fieldName + L" 无效：" + bindingError);
			if (!ValidateDataGridColumnBinding(binding, outError)) return false;
			result = std::move(binding);
			return true;
		};
		if (!readBinding("binding", output.Binding)
			|| !readBinding("contentBinding", output.ContentBinding)) return false;

		const bool bound = output.Kind == DesignDataGridColumnKind::Text
			|| output.Kind == DesignDataGridColumnKind::CheckBox
			|| output.Kind == DesignDataGridColumnKind::ComboBox
			|| output.Kind == DesignDataGridColumnKind::Hyperlink;
		if (output.Kind != DesignDataGridColumnKind::CheckBox
			&& value.contains("isThreeState"))
			return StructuralError(outError,
				L"IsThreeState 仅适用于 DataGridCheckBoxColumn。");
		if (bound && !output.Binding)
			return StructuralError(outError,
				L"DataGrid 绑定列必须声明 Binding。");
		if (!bound && output.Binding)
			return StructuralError(outError,
				L"DataGridTemplateColumn 不支持 Binding。");
		if (!bound && (value.contains("elementStyle")
			|| value.contains("editingElementStyle")))
			return StructuralError(outError,
				L"ElementStyle/EditingElementStyle 仅适用于绑定列。");
		output.ElementStyle = DesignerBindingUtils::Trim(output.ElementStyle);
		output.EditingElementStyle = DesignerBindingUtils::Trim(
			output.EditingElementStyle);
		output.HeaderStyle = DesignerBindingUtils::Trim(output.HeaderStyle);
		output.HeaderTemplate = DesignerBindingUtils::Trim(output.HeaderTemplate);
		output.CellStyle = DesignerBindingUtils::Trim(output.CellStyle);
		if ((value.contains("elementStyle") && output.ElementStyle.empty())
			|| (value.contains("editingElementStyle")
				&& output.EditingElementStyle.empty())
			|| (value.contains("headerStyle") && output.HeaderStyle.empty())
			|| (value.contains("headerTemplate")
				&& output.HeaderTemplate.empty())
			|| (value.contains("cellStyle") && output.CellStyle.empty()))
			return StructuralError(outError,
				L"DataGrid 列 Style/DataTemplate 资源键不能为空。");
		if (bound && (value.contains("cellTemplate")
			|| value.contains("cellEditingTemplate")))
			return StructuralError(outError,
				L"绑定列不支持 CellTemplate/CellEditingTemplate。");
		const bool hyperlink = output.Kind
			== DesignDataGridColumnKind::Hyperlink;
		if (!hyperlink && (value.contains("contentBinding")
			|| value.contains("targetName")))
			return StructuralError(outError,
				L"ContentBinding/TargetName 仅适用于 DataGridHyperlinkColumn。");
		output.TargetName = DesignerBindingUtils::Trim(output.TargetName);
		const bool comboBox = output.Kind
			== DesignDataGridColumnKind::ComboBox;
		if (comboBox)
		{
			output.ItemsSourceResource = DesignerBindingUtils::Trim(
				output.ItemsSourceResource);
			output.DisplayMemberPath = DesignerBindingUtils::Trim(
				output.DisplayMemberPath);
			output.SelectedValuePath = DesignerBindingUtils::Trim(
				output.SelectedValuePath);
			if (output.ItemsSourceResource.empty())
				return StructuralError(outError,
					L"DataGridComboBoxColumn 必须声明 ItemsSource。");
			if ((!output.DisplayMemberPath.empty()
					&& !DesignerBindingUtils::IsValidSourcePath(
						output.DisplayMemberPath))
				|| (!output.SelectedValuePath.empty()
					&& !DesignerBindingUtils::IsValidSourcePath(
						output.SelectedValuePath)))
				return StructuralError(outError,
					L"DataGridComboBoxColumn 成员路径无效。");
		}
		else if (value.contains("itemsSourceResource")
			|| value.contains("displayMemberPath")
			|| value.contains("selectedValuePath")
			|| value.contains("selectionBinding"))
			return StructuralError(outError,
				L"ComboBox 字段仅适用于 DataGridComboBoxColumn。");
		if (!bound && ((value.contains("cellTemplate")
				&& DesignerBindingUtils::Trim(output.CellTemplate).empty())
			|| (value.contains("cellEditingTemplate")
				&& DesignerBindingUtils::Trim(
					output.CellEditingTemplate).empty())))
			return StructuralError(outError,
				L"DataGridTemplateColumn 模板资源键不能为空。");
		return true;
	}

	void EncodeTextFormatting(
		DesignValue& output,
		const DesignTextFormatting& formatting)
	{
		if (formatting.Foreground)
			output["foreground"] = EncodeColor(*formatting.Foreground);
		if (formatting.Background)
			output["background"] = EncodeColor(*formatting.Background);
	if (formatting.FontFamily)
		output["fontFamily"] = StructuralUtf8(*formatting.FontFamily);
	if (formatting.Language)
		output["language"] = StructuralUtf8(*formatting.Language);
		if (formatting.FontSize) output["fontSize"] = *formatting.FontSize;
		if (formatting.FontWeight)
			output["fontWeight"] = StructuralUtf8(*formatting.FontWeight);
		if (formatting.FontStretch)
			output["fontStretch"] = StructuralUtf8(*formatting.FontStretch);
		if (formatting.FontStyle)
			output["fontStyle"] = StructuralUtf8(*formatting.FontStyle);
		if (formatting.Underline) output["underline"] = *formatting.Underline;
		if (formatting.Strikethrough)
			output["strikethrough"] = *formatting.Strikethrough;
	}

	bool IsCanonicalFontWeight(const std::wstring& value)
	{
		return value == L"Thin" || value == L"ExtraLight"
			|| value == L"UltraLight" || value == L"Light"
			|| value == L"SemiLight" || value == L"Normal"
			|| value == L"Regular" || value == L"Medium"
			|| value == L"DemiBold" || value == L"SemiBold"
			|| value == L"Bold" || value == L"ExtraBold"
			|| value == L"UltraBold" || value == L"Black"
			|| value == L"Heavy" || value == L"ExtraBlack"
			|| value == L"UltraBlack";
	}

	bool IsCanonicalFontStyle(const std::wstring& value)
	{
		return value == L"Normal" || value == L"Oblique"
			|| value == L"Italic";
	}

	bool IsCanonicalFontStretch(const std::wstring& value)
	{
		return value == L"UltraCondensed" || value == L"ExtraCondensed"
			|| value == L"Condensed" || value == L"SemiCondensed"
			|| value == L"Normal" || value == L"Medium"
			|| value == L"SemiExpanded" || value == L"Expanded"
			|| value == L"ExtraExpanded" || value == L"UltraExpanded";
	}

	bool IsCanonicalTextAlignment(const std::wstring& value)
	{
		return value == L"Left" || value == L"Right"
			|| value == L"Center" || value == L"Justify";
	}

	bool IsCanonicalFlowDirection(const std::wstring& value)
	{
		return value == L"LeftToRight" || value == L"RightToLeft";
	}

	bool DecodeTextFormattingField(
		const std::string& key,
		const DesignValue& value,
		DesignTextFormatting& formatting,
		bool& handled,
		std::wstring* outError)
	{
		handled = true;
		if (key == "foreground" || key == "background")
		{
			DesignColor color;
			if (!DecodeColor(value, color, outError)) return false;
			if (key == "foreground") formatting.Foreground = color;
			else formatting.Background = color;
			return true;
		}
		if (key == "fontFamily")
		{
			if (!value.is_string())
				return StructuralError(outError, L"FontFamily 必须是字符串。");
			auto family = StructuralWide(value);
			if (family.empty() || std::all_of(family.begin(), family.end(),
				[](wchar_t ch) { return std::iswspace(ch) != 0; }))
				return StructuralError(outError, L"FontFamily 不能为空。");
			formatting.FontFamily = std::move(family);
			return true;
		}
		if (key == "language")
		{
			if (!value.is_string())
				return StructuralError(outError, L"Language 必须是字符串。");
			auto language = StructuralWide(value);
			if (!IsCanonicalRichTextLanguageTag(language))
				return StructuralError(outError, L"Language 值无效。");
			formatting.Language = std::move(language);
			return true;
		}
		if (key == "fontSize")
		{
			if (!value.is_number())
				return StructuralError(outError, L"FontSize 必须是数值。");
			const auto size = value.get<double>();
			if (!std::isfinite(size)
				|| size < (1.0 / 300.0) || size > 160000.0)
				return StructuralError(outError,
					L"FontSize 必须位于 1/300 到 160000 之间。");
			formatting.FontSize = size;
			return true;
		}
		if (key == "fontWeight" || key == "fontStretch"
			|| key == "fontStyle")
		{
			if (!value.is_string())
				return StructuralError(outError, L"字体枚举值必须是字符串。");
			auto text = StructuralWide(value);
			if (key == "fontWeight")
			{
				if (!IsCanonicalFontWeight(text))
					return StructuralError(outError, L"FontWeight 值无效。");
				formatting.FontWeight = std::move(text);
			}
			else if (key == "fontStretch")
			{
				if (!IsCanonicalFontStretch(text))
					return StructuralError(outError, L"FontStretch 值无效。");
				formatting.FontStretch = std::move(text);
			}
			else
			{
				if (!IsCanonicalFontStyle(text))
					return StructuralError(outError, L"FontStyle 值无效。");
				formatting.FontStyle = std::move(text);
			}
			return true;
		}
		if (key == "underline" || key == "strikethrough")
		{
			if (!value.is_boolean())
				return StructuralError(outError,
					key == "underline" ? L"Underline 必须是布尔值。"
						: L"Strikethrough 必须是布尔值。");
			if (key == "underline") formatting.Underline = value.get<bool>();
			else formatting.Strikethrough = value.get<bool>();
			return true;
		}
		handled = false;
		return true;
	}

	const char* InlineKindText(DesignInlineKind kind) noexcept
	{
		switch (kind)
		{
		case DesignInlineKind::Run: return "Run";
		case DesignInlineKind::Span: return "Span";
		case DesignInlineKind::Bold: return "Bold";
		case DesignInlineKind::Italic: return "Italic";
		case DesignInlineKind::Underline: return "Underline";
		case DesignInlineKind::LineBreak: return "LineBreak";
		}
		return "";
	}

	std::optional<DesignInlineKind> ParseInlineKind(
		const std::string& value) noexcept
	{
		if (value == "Run") return DesignInlineKind::Run;
		if (value == "Span") return DesignInlineKind::Span;
		if (value == "Bold") return DesignInlineKind::Bold;
		if (value == "Italic") return DesignInlineKind::Italic;
		if (value == "Underline") return DesignInlineKind::Underline;
		if (value == "LineBreak") return DesignInlineKind::LineBreak;
		return std::nullopt;
	}

	DesignValue EncodeInline(const DesignInline& inlineValue)
	{
		DesignValue output{
			{ "kind", InlineKindText(inlineValue.Kind) } };
		EncodeTextFormatting(output, inlineValue);
		if (inlineValue.Kind == DesignInlineKind::Run)
			output["text"] = StructuralUtf8(inlineValue.Text);
		else if (inlineValue.Kind == DesignInlineKind::LineBreak)
		{
			// LineBreak is a terminal structural Inline with no content member.
		}
		else
			output["inlines"] = EncodeArray(
				inlineValue.Inlines, EncodeInline);
		return output;
	}

	DesignValue EncodeParagraph(const DesignParagraph& paragraph)
	{
		DesignValue output = DesignValue::object();
		EncodeTextFormatting(output, paragraph);
		if (paragraph.TextAlignment)
			output["textAlignment"] = StructuralUtf8(
				*paragraph.TextAlignment);
		if (paragraph.FlowDirection)
			output["flowDirection"] = StructuralUtf8(
				*paragraph.FlowDirection);
		output["inlines"] = EncodeArray(
			paragraph.Inlines, EncodeInline);
		return output;
	}

	DesignValue EncodeFlowDocument(const DesignFlowDocument& document)
	{
		DesignValue output = DesignValue::object();
		EncodeTextFormatting(output, document);
		if (document.TextAlignment)
			output["textAlignment"] = StructuralUtf8(
				*document.TextAlignment);
		if (document.FlowDirection)
			output["flowDirection"] = StructuralUtf8(
				*document.FlowDirection);
		output["paragraphs"] = EncodeArray(
			document.Paragraphs, EncodeParagraph);
		return output;
	}

	bool DecodeInline(
		const DesignValue& value,
		DesignInline& inlineValue,
		std::wstring* outError)
	{
		if (!value.is_object())
			return StructuralError(outError, L"Inline 结构必须是对象。");
		bool foundKind = false;
		bool foundText = false;
		bool foundInlines = false;
		for (const auto& [key, item] : value.ObjectItems())
		{
			if (key == "kind")
			{
				if (!item.is_string())
					return StructuralError(outError,
						L"Inline.Kind 必须是字符串。");
				const auto kind = ParseInlineKind(item.get<std::string>());
				if (!kind)
					return StructuralError(outError,
						L"Inline.Kind 值无效。");
				inlineValue.Kind = *kind;
				foundKind = true;
				continue;
			}
			if (key == "text")
			{
				if (!item.is_string())
					return StructuralError(outError, L"Run.Text 必须是字符串。");
				inlineValue.Text = StructuralWide(item);
				foundText = true;
				continue;
			}
			if (key == "inlines")
			{
				if (!DecodeArray(item, inlineValue.Inlines,
					DecodeInline, outError)) return false;
				foundInlines = true;
				continue;
			}
			bool handled = false;
			if (!DecodeTextFormattingField(
				key, item, inlineValue, handled, outError)) return false;
			if (!handled) return StructuralError(outError,
				L"Inline 包含未知结构字段："
					+ Convert::Utf8ToUnicode(key));
		}
		// Version-43 Run snapshots did not carry an explicit kind.
		if (!foundKind && foundText && !foundInlines)
			inlineValue.Kind = DesignInlineKind::Run;
		else if (!foundKind)
			return StructuralError(outError, L"Inline 结构缺少 Kind。");
		if (inlineValue.Kind == DesignInlineKind::Run)
		{
			if (!foundText || foundInlines)
				return StructuralError(outError,
					L"Run 必须且只能包含 Text。");
		}
		else if (inlineValue.Kind == DesignInlineKind::LineBreak)
		{
			if (foundText || foundInlines)
				return StructuralError(outError,
					L"LineBreak 不能包含 Text 或 Inlines。");
		}
		else if (!foundInlines || foundText)
		{
			return StructuralError(outError,
				L"Span/Bold/Italic/Underline 必须且只能包含 Inlines。");
		}
		return true;
	}

	bool DecodeParagraph(
		const DesignValue& value,
		DesignParagraph& paragraph,
		std::wstring* outError)
	{
		if (!value.is_object())
			return StructuralError(outError, L"Paragraph 结构必须是对象。");
		bool foundInlines = false;
		for (const auto& [key, item] : value.ObjectItems())
		{
			if (key == "textAlignment")
			{
				if (!item.is_string())
					return StructuralError(outError,
						L"Paragraph.TextAlignment 必须是字符串。");
				auto alignment = StructuralWide(item);
				if (!IsCanonicalTextAlignment(alignment))
					return StructuralError(outError,
						L"Paragraph.TextAlignment 值无效。");
				paragraph.TextAlignment = std::move(alignment);
				continue;
			}
			if (key == "flowDirection")
			{
				if (!item.is_string())
					return StructuralError(outError,
						L"Paragraph.FlowDirection 必须是字符串。");
				auto direction = StructuralWide(item);
				if (!IsCanonicalFlowDirection(direction))
					return StructuralError(outError,
						L"Paragraph.FlowDirection 值无效。");
				paragraph.FlowDirection = std::move(direction);
				continue;
			}
			if (key == "inlines" || key == "runs")
			{
				if (foundInlines)
					return StructuralError(outError,
						L"Paragraph.Inlines 不能重复。");
				if (!DecodeArray(item, paragraph.Inlines,
					DecodeInline, outError))
					return false;
				foundInlines = true;
				continue;
			}
			bool handled = false;
			if (!DecodeTextFormattingField(
				key, item, paragraph, handled, outError)) return false;
			if (!handled) return StructuralError(outError,
				L"Paragraph 包含未知结构字段："
				+ Convert::Utf8ToUnicode(key));
		}
		if (!foundInlines)
			return StructuralError(outError,
				L"Paragraph 结构缺少 Inlines。");
		return true;
	}

	bool DecodeFlowDocument(
		const DesignValue& value,
		DesignFlowDocument& document,
		std::wstring* outError)
	{
		if (!value.is_object())
			return StructuralError(outError, L"FlowDocument 结构必须是对象。");
		bool foundParagraphs = false;
		for (const auto& [key, item] : value.ObjectItems())
		{
			if (key == "textAlignment")
			{
				if (!item.is_string())
					return StructuralError(outError,
						L"FlowDocument.TextAlignment 必须是字符串。");
				auto alignment = StructuralWide(item);
				if (!IsCanonicalTextAlignment(alignment))
					return StructuralError(outError,
						L"FlowDocument.TextAlignment 值无效。");
				document.TextAlignment = std::move(alignment);
				continue;
			}
			if (key == "flowDirection")
			{
				if (!item.is_string())
					return StructuralError(outError,
						L"FlowDocument.FlowDirection 必须是字符串。");
				auto direction = StructuralWide(item);
				if (!IsCanonicalFlowDirection(direction))
					return StructuralError(outError,
						L"FlowDocument.FlowDirection 值无效。");
				document.FlowDirection = std::move(direction);
				continue;
			}
			if (key == "paragraphs")
			{
				if (!DecodeArray(
					item, document.Paragraphs, DecodeParagraph, outError)) return false;
				foundParagraphs = true;
				continue;
			}
			bool handled = false;
			if (!DecodeTextFormattingField(
				key, item, document, handled, outError)) return false;
			if (!handled) return StructuralError(outError,
				L"FlowDocument 包含未知结构字段："
				+ Convert::Utf8ToUnicode(key));
		}
		if (!foundParagraphs)
			return StructuralError(outError,
				L"FlowDocument 结构缺少 Paragraphs。");
		return true;
	}

}

DesignValue EncodeDesignNodeStructure(
	UIClass type,
	const DesignNodeStructure& structure)
{
	DesignValue result = DesignValue::object();
	auto stringField = [&](const char* key, const std::wstring& value)
	{
		if (!value.empty()) result[key] = StructuralUtf8(value);
	};
	stringField("commandTarget", structure.CommandTarget);
	stringField("itemsSourceResource", structure.ItemsSourceResource);
	stringField("itemTemplate", structure.ItemTemplate);
	stringField("contentTemplate", structure.ContentTemplate);
	stringField("headerTemplate", structure.HeaderTemplate);
	stringField("controlTemplate", structure.ControlTemplate);
	stringField("rowValidationErrorTemplate",
		structure.RowValidationErrorTemplate);
	stringField("dataGridCellStyle", structure.DataGridCellStyle);
	stringField("dataGridColumnHeaderStyle",
		structure.DataGridColumnHeaderStyle);
	stringField("dataGridRowStyle", structure.DataGridRowStyle);
	stringField("dataGridRowHeaderStyle", structure.DataGridRowHeaderStyle);
	stringField("dataGridRowHeaderTemplate",
		structure.DataGridRowHeaderTemplate);
	stringField("dataGridRowDetailsTemplate",
		structure.DataGridRowDetailsTemplate);
	stringField("groupStyle", structure.GroupStyle);
	stringField("itemsPanel", structure.ItemsPanel);
	stringField("itemContainerStyle", structure.ItemContainerStyle);
	if (structure.ChildRole == DesignNodeChildRole::Header)
		result["headeredRegion"] = "header";
	if (structure.RelativePanel && !structure.RelativePanel->Empty())
	{
		DesignValue constraints = DesignValue::object();
		auto boolean = [&](const char* key, const std::optional<bool>& value)
		{
			if (value) constraints[key] = *value;
		};
		auto reference = [&](const char* key,
			const std::optional<std::wstring>& value)
		{
			if (value) constraints[key] = StructuralUtf8(*value);
		};
		boolean("centerHorizontal", structure.RelativePanel->CenterHorizontal);
		boolean("centerVertical", structure.RelativePanel->CenterVertical);
		boolean("alignLeftWithPanel", structure.RelativePanel->AlignLeftWithPanel);
		boolean("alignTopWithPanel", structure.RelativePanel->AlignTopWithPanel);
		boolean("alignRightWithPanel", structure.RelativePanel->AlignRightWithPanel);
		boolean("alignBottomWithPanel", structure.RelativePanel->AlignBottomWithPanel);
		reference("above", structure.RelativePanel->Above);
		reference("below", structure.RelativePanel->Below);
		reference("leftOf", structure.RelativePanel->LeftOf);
		reference("rightOf", structure.RelativePanel->RightOf);
		reference("alignLeftWith", structure.RelativePanel->AlignLeftWith);
		reference("alignRightWith", structure.RelativePanel->AlignRightWith);
		reference("alignTopWith", structure.RelativePanel->AlignTopWith);
		reference("alignBottomWith", structure.RelativePanel->AlignBottomWith);
		result["relativePanelConstraints"] = std::move(constraints);
	}

	if (structure.GridRows)
		result["rows"] = EncodeGridTracks(*structure.GridRows, "height");
	if (structure.GridColumns)
		result["columns"] = EncodeGridTracks(*structure.GridColumns, "width");
	if (structure.DataGridColumns)
		result["dataGridColumns"] = EncodeArray(*structure.DataGridColumns,
			EncodeDataGridColumn);
	if (structure.ChartSeries)
		result["series"] = EncodeArray(*structure.ChartSeries,
			[](const DesignChartSeries& series)
			{
				DesignValue value{ { "name", StructuralUtf8(series.Name) },
					{ "visible", series.Visible } };
				if (series.Color) value["color"] = EncodeColor(*series.Color);
				value["points"] = EncodeArray(series.Points,
					[](const DesignChartPoint& point)
					{
						DesignValue result{ { "label", StructuralUtf8(point.Label) },
							{ "value", point.Value }, { "tag", point.Tag } };
						if (point.Color)
						{
							result["color"] = EncodeColor(*point.Color);
							result["useCustomColor"] = true;
						}
						return result;
					});
				return value;
			});
	if (structure.Document)
		result["document"] = EncodeFlowDocument(*structure.Document);
	(void)type;
	return result;
}

bool DecodeDesignNodeStructure(
	UIClass type,
	const DesignValue& value,
	DesignNodeStructure& structure,
	std::wstring* outError)
{
	if (!value.is_object())
		return StructuralError(outError, L"控件结构必须是对象。");
	DesignNodeStructure decoded;
	auto readString = [&](const std::string& key, std::wstring& output)
	{
		if (!value[key].is_string())
			return StructuralError(outError,
				L"控件结构字段必须是字符串："
				+ Convert::Utf8ToUnicode(key));
		output = StructuralWide(value[key]);
		return true;
	};
	for (const auto& [key, item] : value.ObjectItems())
	{
		if (key == "commandTarget")
		{
			if (type != UIClass::UI_Button && type != UIClass::UI_MenuItem)
				return StructuralError(outError,
					L"CommandTarget 仅适用于 Button 或 MenuItem。");
			if (!readString(key, decoded.CommandTarget)) return false;
			std::wstring targetError;
			if (decoded.CommandTarget.empty()
				|| !DesignerEventCatalog::ValidateHandlerName(
					decoded.CommandTarget, &targetError))
				return StructuralError(outError,
					L"CommandTarget 必须是直接 x:Name：" + targetError);
			continue;
		}
		if (key == "itemsSourceResource")
		{
			if (!readString(key, decoded.ItemsSourceResource)) return false;
			continue;
		}
		if (key == "itemTemplate")
		{
			if (!readString(key, decoded.ItemTemplate)) return false;
			continue;
		}
		if (key == "contentTemplate")
		{
			if (!readString(key, decoded.ContentTemplate)) return false;
			continue;
		}
		if (key == "headerTemplate")
		{
			if (!readString(key, decoded.HeaderTemplate)) return false;
			continue;
		}
		if (key == "controlTemplate")
		{
			if (!readString(key, decoded.ControlTemplate)) return false;
			continue;
		}
		if (key == "rowValidationErrorTemplate")
		{
			if (type != UIClass::UI_DataGrid)
				return StructuralError(outError,
					L"RowValidationErrorTemplate 仅适用于 DataGrid。");
			if (!readString(key, decoded.RowValidationErrorTemplate))
				return false;
			continue;
		}
		if (key == "dataGridCellStyle"
			|| key == "dataGridColumnHeaderStyle"
			|| key == "dataGridRowStyle"
			|| key == "dataGridRowHeaderStyle"
			|| key == "dataGridRowHeaderTemplate"
			|| key == "dataGridRowDetailsTemplate")
		{
			if (type != UIClass::UI_DataGrid)
				return StructuralError(outError,
					L"DataGrid 样式/模板字段仅适用于 DataGrid。");
			std::wstring* destination = nullptr;
			if (key == "dataGridCellStyle")
				destination = &decoded.DataGridCellStyle;
			else if (key == "dataGridColumnHeaderStyle")
				destination = &decoded.DataGridColumnHeaderStyle;
			else if (key == "dataGridRowStyle")
				destination = &decoded.DataGridRowStyle;
			else if (key == "dataGridRowHeaderStyle")
				destination = &decoded.DataGridRowHeaderStyle;
			else if (key == "dataGridRowHeaderTemplate")
				destination = &decoded.DataGridRowHeaderTemplate;
			else destination = &decoded.DataGridRowDetailsTemplate;
			if (!readString(key, *destination)) return false;
			continue;
		}
		if (key == "groupStyle")
		{
			if (!readString(key, decoded.GroupStyle)) return false;
			continue;
		}
		if (key == "itemsPanel")
		{
			if (!readString(key, decoded.ItemsPanel)) return false;
			continue;
		}
		if (key == "itemContainerStyle")
		{
			if (!readString(key, decoded.ItemContainerStyle)) return false;
			continue;
		}
		if (key == "headeredRegion")
		{
			if (!item.is_string() || item.get<std::string>() != "header")
				return StructuralError(outError, L"HeaderedContent 区域无效。");
			decoded.ChildRole = DesignNodeChildRole::Header;
			continue;
		}
		if (key == "relativePanelConstraints")
		{
			if (!item.is_object())
				return StructuralError(outError, L"RelativePanel 约束必须是对象。");
			DesignRelativePanelConstraints constraints;
			for (const auto& [constraintKey, constraintValue] : item.ObjectItems())
			{
				auto boolean = [&](std::optional<bool>& output)
				{
					if (!constraintValue.is_boolean()) return false;
					output = constraintValue.get<bool>();
					return true;
				};
				auto reference = [&](std::optional<std::wstring>& output)
				{
					if (!constraintValue.is_string()) return false;
					output = StructuralWide(constraintValue);
					return !output->empty();
				};
				bool valid = false;
				if (constraintKey == "centerHorizontal")
					valid = boolean(constraints.CenterHorizontal);
				else if (constraintKey == "centerVertical")
					valid = boolean(constraints.CenterVertical);
				else if (constraintKey == "alignLeftWithPanel")
					valid = boolean(constraints.AlignLeftWithPanel);
				else if (constraintKey == "alignTopWithPanel")
					valid = boolean(constraints.AlignTopWithPanel);
				else if (constraintKey == "alignRightWithPanel")
					valid = boolean(constraints.AlignRightWithPanel);
				else if (constraintKey == "alignBottomWithPanel")
					valid = boolean(constraints.AlignBottomWithPanel);
				else if (constraintKey == "above") valid = reference(constraints.Above);
				else if (constraintKey == "below") valid = reference(constraints.Below);
				else if (constraintKey == "leftOf") valid = reference(constraints.LeftOf);
				else if (constraintKey == "rightOf") valid = reference(constraints.RightOf);
				else if (constraintKey == "alignLeftWith")
					valid = reference(constraints.AlignLeftWith);
				else if (constraintKey == "alignRightWith")
					valid = reference(constraints.AlignRightWith);
				else if (constraintKey == "alignTopWith")
					valid = reference(constraints.AlignTopWith);
				else if (constraintKey == "alignBottomWith")
					valid = reference(constraints.AlignBottomWith);
				else return StructuralError(outError,
					L"未知 RelativePanel 约束："
					+ Convert::Utf8ToUnicode(constraintKey));
				if (!valid) return StructuralError(outError,
					L"RelativePanel 约束值类型无效："
					+ Convert::Utf8ToUnicode(constraintKey));
			}
			decoded.RelativePanel = std::move(constraints);
			continue;
		}
		if (key == "rows" && type == UIClass::UI_Grid)
		{
			std::vector<DesignGridTrack> tracks;
			if (!DecodeGridTracks(item, "height", tracks, outError)) return false;
			decoded.GridRows = std::move(tracks);
			continue;
		}
		if (key == "columns" && type == UIClass::UI_Grid)
		{
			std::vector<DesignGridTrack> tracks;
			if (!DecodeGridTracks(item, "width", tracks, outError)) return false;
			decoded.GridColumns = std::move(tracks);
			continue;
		}
		if (key == "dataGridColumns" && type == UIClass::UI_DataGrid)
		{
			std::vector<DesignDataGridColumn> columns;
			if (!DecodeArray(item, columns,
				DecodeDataGridColumn, outError)) return false;
			decoded.DataGridColumns = std::move(columns);
			continue;
		}
		if (key == "series" && type == UIClass::UI_ChartView)
		{
			std::vector<DesignChartSeries> series;
			if (!DecodeArray(item, series,
				[](const DesignValue& source, DesignChartSeries& result,
					std::wstring* error)
				{
					if (!source.is_object())
						return StructuralError(error, L"ChartSeries 结构无效。");
					result.Name = Convert::Utf8ToUnicode(
						source.value("name", std::string{}));
					result.Visible = source.value("visible", true);
					if (source.contains("color"))
					{
						DesignColor color;
						if (!DecodeColor(source["color"], color, error)) return false;
						result.Color = color;
					}
					if (!source.contains("points")) return true;
					return DecodeArray(source["points"], result.Points,
						[](const DesignValue& pointValue, DesignChartPoint& point,
							std::wstring* pointError)
						{
							if (!pointValue.is_object())
								return StructuralError(pointError, L"ChartPoint 结构无效。");
							point.Label = Convert::Utf8ToUnicode(
								pointValue.value("label", std::string{}));
							point.Value = pointValue.value("value", 0.0);
							point.Tag = pointValue.value("tag", 0ULL);
							if (pointValue.contains("color")
								&& pointValue.value("useCustomColor", true))
							{
								DesignColor color;
								if (!DecodeColor(pointValue["color"], color,
									pointError)) return false;
								point.Color = color;
							}
							return true;
						}, error);
				}, outError)) return false;
			decoded.ChartSeries = std::move(series);
			continue;
		}
		if (key == "document" && type == UIClass::UI_RichTextBox)
		{
			DesignFlowDocument document;
			if (!DecodeFlowDocument(item, document, outError)) return false;
			decoded.Document = std::move(document);
			continue;
		}
		return StructuralError(outError,
			L"未知或不适用于当前控件的结构字段："
			+ Convert::Utf8ToUnicode(key));
	}
	structure = std::move(decoded);
	return true;
}

bool DesignPropertyNameLess::operator()(
	const std::wstring& left,
	const std::wstring& right) const noexcept
{
	return left < right;
}

bool DesignNodeProperties::Empty() const noexcept
{
	return StyleResourceKey.empty() && Values.empty();
}

const DesignPropertyAssignment* DesignNodeProperties::Find(
	const std::wstring& name) const noexcept
{
	const auto found = Values.find(name);
	return found == Values.end() ? nullptr : &found->second;
}

DesignPropertyAssignment* DesignNodeProperties::Find(
	const std::wstring& name) noexcept
{
	const auto found = Values.find(name);
	return found == Values.end() ? nullptr : &found->second;
}

void DesignNodeProperties::Set(
	std::wstring name,
	DesignPropertyAssignment assignment)
{
	if (name.empty())
		throw std::invalid_argument("Design property name cannot be empty");
	const auto found = Values.find(name);
	if (found != Values.end())
	{
		if (found->first == name)
		{
			found->second = std::move(assignment);
			return;
		}
		Values.erase(found);
	}
	Values.emplace(std::move(name), std::move(assignment));
}

bool DesignNodeProperties::Remove(const std::wstring& name) noexcept
{
	const auto found = Values.find(name);
	if (found == Values.end()) return false;
	Values.erase(found);
	return true;
}

DesignValue EncodeDesignNodeProperties(const DesignNodeProperties& properties)
{
	DesignValue result = DesignValue::object();
	if (!properties.StyleResourceKey.empty())
		result["styleResourceKey"] = StructuralUtf8(
			properties.StyleResourceKey);
	if (!properties.Values.empty())
	{
		DesignValue values = DesignValue::object();
		for (const auto& [name, assignment] : properties.Values)
		{
			DesignValue stored{
				{ "kind", static_cast<int>(assignment.Value.Kind) },
				{ "text", StructuralUtf8(assignment.Value.Text) }
			};
			if (!assignment.Value.ObjectValue.is_null())
				stored["object"] = assignment.Value.ObjectValue;
			if (!assignment.ResourceKey.empty())
				stored["resourceKey"] = StructuralUtf8(
					assignment.ResourceKey);
			if (!assignment.DynamicResourceKey.empty())
				stored["dynamicResourceKey"] = StructuralUtf8(
					assignment.DynamicResourceKey);
			values[StructuralUtf8(name)] = std::move(stored);
		}
		result["values"] = std::move(values);
	}
	return result;
}

bool DecodeDesignNodeProperties(
	const DesignValue& value,
	DesignNodeProperties& properties,
	std::wstring* outError)
{
	if (!value.is_object())
		return StructuralError(outError, L"节点 Properties 必须是对象。");
	for (const auto& [key, ignored] : value.ObjectItems())
	{
		(void)ignored;
		if (key != "styleResourceKey" && key != "values")
			return StructuralError(outError,
				L"节点 Properties 包含未知字段：" + Convert::Utf8ToUnicode(key));
	}

	DesignNodeProperties decoded;
	if (value.contains("styleResourceKey"))
	{
		if (!value["styleResourceKey"].is_string())
			return StructuralError(outError, L"StyleResourceKey 必须是字符串。");
		decoded.StyleResourceKey = StructuralWide(value["styleResourceKey"]);
		if (decoded.StyleResourceKey.empty())
			return StructuralError(outError, L"StyleResourceKey 不能为空。");
	}
	if (value.contains("values"))
	{
		const auto& values = value["values"];
		if (!values.is_object())
			return StructuralError(outError, L"Properties.Values 必须是对象。");
		for (const auto& [rawName, stored] : values.ObjectItems())
		{
			if (rawName.empty() || !stored.is_object())
				return StructuralError(outError, L"属性赋值结构无效。");
			for (const auto& [key, ignored] : stored.ObjectItems())
			{
				(void)ignored;
				if (key != "kind" && key != "text" && key != "object"
					&& key != "resourceKey" && key != "dynamicResourceKey")
					return StructuralError(outError,
						L"属性赋值包含未知字段：" + Convert::Utf8ToUnicode(key));
			}
			if (!stored.contains("kind")
				|| !stored["kind"].is_number_integer()
				|| !stored.contains("text") || !stored["text"].is_string())
				return StructuralError(outError, L"属性赋值缺少 Kind 或 Text。");
			const auto kind = stored["kind"].get<int>();
			if (kind < static_cast<int>(DesignerStyleValueKind::Bool)
				|| kind > static_cast<int>(
					DesignerStyleValueKind::NullableBool))
				return StructuralError(outError, L"属性赋值 Kind 无效。");
			if ((stored.contains("resourceKey")
					&& !stored["resourceKey"].is_string())
				|| (stored.contains("dynamicResourceKey")
					&& !stored["dynamicResourceKey"].is_string())
				|| (stored.contains("resourceKey")
					&& stored.contains("dynamicResourceKey")))
				return StructuralError(outError, L"属性资源表达式无效。");

			DesignPropertyAssignment assignment;
			assignment.Value.Kind = static_cast<DesignerStyleValueKind>(kind);
			assignment.Value.Text = StructuralWide(stored["text"]);
			if (stored.contains("object"))
				assignment.Value.ObjectValue = stored["object"];
			if (stored.contains("resourceKey"))
				assignment.ResourceKey = StructuralWide(stored["resourceKey"]);
			if (stored.contains("dynamicResourceKey"))
				assignment.DynamicResourceKey = StructuralWide(
					stored["dynamicResourceKey"]);
			if ((stored.contains("resourceKey") && assignment.ResourceKey.empty())
				|| (stored.contains("dynamicResourceKey")
					&& assignment.DynamicResourceKey.empty()))
				return StructuralError(outError, L"属性资源键不能为空。");
			const auto name = Convert::Utf8ToUnicode(rawName);
			if (decoded.Find(name))
				return StructuralError(outError, L"属性名不能仅大小写不同。");
			decoded.Set(name, std::move(assignment));
		}
	}
	properties = std::move(decoded);
	return true;
}

DesignValue EncodeDesignNodeBindings(const DesignBindingMap& bindings)
{
	DesignValue result = DesignValue::object();
	if (bindings.empty()) return result;
	DesignValue values = DesignValue::object();
	for (const auto& [target, binding] : bindings)
		values[StructuralUtf8(target)] =
			DesignerBindingUtils::WriteBindingDefinition(binding);
	result["values"] = std::move(values);
	return result;
}

bool DecodeDesignNodeBindings(
	const DesignValue& value,
	DesignBindingMap& bindings,
	std::wstring* outError)
{
	if (!value.is_object())
		return StructuralError(outError, L"节点 Bindings 必须是对象。");
	for (const auto& [key, ignored] : value.ObjectItems())
	{
		(void)ignored;
		if (key != "values")
			return StructuralError(outError,
				L"节点 Bindings 包含未知字段：" + Convert::Utf8ToUnicode(key));
	}
	DesignBindingMap decoded;
	if (value.contains("values"))
	{
		const auto& values = value["values"];
		if (!values.is_object())
			return StructuralError(outError, L"Bindings.Values 必须是对象。");
		for (const auto& [rawTarget, stored] : values.ObjectItems())
		{
			const auto target = Convert::Utf8ToUnicode(rawTarget);
			if (target.empty() || decoded.contains(target))
				return StructuralError(outError,
					L"Binding 目标名为空或仅大小写不同。");
			DesignerDataBinding binding;
			std::wstring bindingError;
			if (!DesignerBindingUtils::TryReadBindingDefinition(
				stored, binding, &bindingError))
				return StructuralError(outError,
					L"Binding " + target + L" 无效：" + bindingError);
			if (DesignerBindingUtils::WriteBindingDefinition(binding) != stored)
				return StructuralError(outError,
					L"Binding " + target + L" 不是规范快照表达式。");
			decoded.emplace(target, std::move(binding));
		}
	}
	bindings = std::move(decoded);
	return true;
}

DesignValue EncodeDesignNodeEvents(const DesignEventHandlerMap& events)
{
	DesignValue result = DesignValue::object();
	if (events.empty()) return result;
	DesignValue handlers = DesignValue::object();
	for (const auto& [eventName, handler] : events)
		handlers[StructuralUtf8(eventName)] = StructuralUtf8(handler);
	result["handlers"] = std::move(handlers);
	return result;
}

bool DecodeDesignNodeEvents(
	const DesignValue& value,
	DesignEventHandlerMap& events,
	std::wstring* outError)
{
	if (!value.is_object())
		return StructuralError(outError, L"节点 Events 必须是对象。");
	for (const auto& [key, ignored] : value.ObjectItems())
	{
		(void)ignored;
		if (key != "handlers")
			return StructuralError(outError,
				L"节点 Events 包含未知字段：" + Convert::Utf8ToUnicode(key));
	}
	DesignEventHandlerMap decoded;
	if (value.contains("handlers"))
	{
		const auto& handlers = value["handlers"];
		if (!handlers.is_object())
			return StructuralError(outError, L"Events.Handlers 必须是对象。");
		for (const auto& [rawEvent, stored] : handlers.ObjectItems())
		{
			const auto eventName = Convert::Utf8ToUnicode(rawEvent);
			if (eventName.empty() || decoded.contains(eventName)
				|| !stored.is_string())
				return StructuralError(outError,
					L"事件名为空、重复或处理函数不是字符串。");
			const auto handler = StructuralWide(stored);
			if (handler.empty())
				return StructuralError(outError, L"事件处理函数不能为空。");
			decoded.emplace(eventName, handler);
		}
	}
	events = std::move(decoded);
	return true;
}

DesignValue EncodeDesignCommandBindings(
	const std::vector<DesignCommandBinding>& bindings)
{
	DesignValue result = DesignValue::array();
	for (const auto& binding : bindings)
	{
		DesignValue value{
			{ "command", StructuralUtf8(binding.Command) } };
		if (!binding.PreviewCanExecute.empty())
			value["previewCanExecute"] = StructuralUtf8(binding.PreviewCanExecute);
		if (!binding.CanExecute.empty())
			value["canExecute"] = StructuralUtf8(binding.CanExecute);
		if (!binding.PreviewExecuted.empty())
			value["previewExecuted"] = StructuralUtf8(binding.PreviewExecuted);
		if (!binding.Executed.empty())
			value["executed"] = StructuralUtf8(binding.Executed);
		result.push_back(std::move(value));
	}
	return result;
}

bool DecodeDesignCommandBindings(
	const DesignValue& value,
	std::vector<DesignCommandBinding>& bindings,
	std::wstring* outError)
{
	if (!value.is_array())
		return StructuralError(outError, L"CommandBindings 必须是数组。");
	std::vector<DesignCommandBinding> decoded;
	for (const auto& stored : value.ArrayItems())
	{
		if (!stored.is_object())
			return StructuralError(outError, L"CommandBinding 必须是对象。");
		for (const auto& [key, field] : stored.ObjectItems())
		{
			(void)field;
			if (key != "command" && key != "previewCanExecute"
				&& key != "canExecute" && key != "previewExecuted"
				&& key != "executed")
				return StructuralError(outError,
					L"CommandBinding 包含未知字段："
					+ Convert::Utf8ToUnicode(key));
		}
		DesignCommandBinding binding;
		binding.Command = Convert::Utf8ToUnicode(
			stored.value("command", std::string{}));
		binding.PreviewCanExecute = Convert::Utf8ToUnicode(
			stored.value("previewCanExecute", std::string{}));
		binding.CanExecute = Convert::Utf8ToUnicode(
			stored.value("canExecute", std::string{}));
		binding.PreviewExecuted = Convert::Utf8ToUnicode(
			stored.value("previewExecuted", std::string{}));
		binding.Executed = Convert::Utf8ToUnicode(
			stored.value("executed", std::string{}));
		if (binding.Command.empty())
			return StructuralError(outError, L"CommandBinding.Command 不能为空。");
		if (binding.PreviewCanExecute.empty() && binding.CanExecute.empty()
			&& binding.PreviewExecuted.empty() && binding.Executed.empty())
			return StructuralError(outError, L"CommandBinding 至少需要一个处理器。");
		decoded.push_back(std::move(binding));
	}
	bindings = std::move(decoded);
	return true;
}

DesignValue EncodeDesignInputBindings(
	const std::vector<DesignInputBinding>& bindings)
{
	DesignValue result = DesignValue::array();
	for (const auto& binding : bindings)
	{
		DesignValue value{
			{ "kind", binding.Kind == DesignInputBindingKind::Mouse
				? "mouse" : "key" },
			{ "command", StructuralUtf8(binding.Command) },
			{ "gesture", StructuralUtf8(binding.Gesture) } };
		if (!binding.CommandParameter.empty())
			value["commandParameter"] = StructuralUtf8(binding.CommandParameter);
		if (!binding.CommandTarget.empty())
			value["commandTarget"] = StructuralUtf8(binding.CommandTarget);
		result.push_back(std::move(value));
	}
	return result;
}

bool DecodeDesignInputBindings(
	const DesignValue& value,
	std::vector<DesignInputBinding>& bindings,
	std::wstring* outError)
{
	if (!value.is_array())
		return StructuralError(outError, L"InputBindings 必须是数组。");
	std::vector<DesignInputBinding> decoded;
	std::unordered_set<std::wstring> gestureIdentities;
	for (const auto& stored : value.ArrayItems())
	{
		if (!stored.is_object())
			return StructuralError(outError, L"InputBinding 必须是对象。");
		for (const auto& [key, field] : stored.ObjectItems())
		{
			(void)field;
			if (key != "kind" && key != "command" && key != "gesture"
				&& key != "commandParameter" && key != "commandTarget")
				return StructuralError(outError,
					L"InputBinding 包含未知字段："
					+ Convert::Utf8ToUnicode(key));
		}
		DesignInputBinding binding;
		const auto kind = stored.value("kind", std::string{});
		if (kind == "key") binding.Kind = DesignInputBindingKind::Key;
		else if (kind == "mouse") binding.Kind = DesignInputBindingKind::Mouse;
		else return StructuralError(outError,
			L"InputBinding.kind 必须是 key 或 mouse。");
		binding.Command = Convert::Utf8ToUnicode(
			stored.value("command", std::string{}));
		binding.Gesture = Convert::Utf8ToUnicode(
			stored.value("gesture", std::string{}));
		binding.CommandParameter = Convert::Utf8ToUnicode(
			stored.value("commandParameter", std::string{}));
		binding.CommandTarget = Convert::Utf8ToUnicode(
			stored.value("commandTarget", std::string{}));
		if (!binding.CommandTarget.empty())
		{
			std::wstring targetError;
			if (!DesignerEventCatalog::ValidateHandlerName(
				binding.CommandTarget, &targetError))
				return StructuralError(outError,
					L"InputBinding.CommandTarget 必须是直接 x:Name："
					+ targetError);
		}
		std::wstring gestureError;
		if (binding.Command.empty())
			return StructuralError(outError,
				L"InputBinding.Command 不能为空。");
		if (binding.Kind == DesignInputBindingKind::Key)
		{
			KeyGesture gesture;
			if (!TryParseKeyGesture(binding.Gesture, gesture, &gestureError))
				return StructuralError(outError, gestureError);
			binding.Gesture = FormatKeyGesture(gesture);
		}
		else
		{
			MouseGesture gesture;
			if (!TryParseMouseGesture(binding.Gesture, gesture, &gestureError))
				return StructuralError(outError, gestureError);
			binding.Gesture = FormatMouseGesture(gesture);
		}
		const auto gestureIdentity =
			(binding.Kind == DesignInputBindingKind::Mouse ? L"mouse:" : L"key:")
			+ binding.Gesture;
		if (!gestureIdentities.insert(gestureIdentity).second)
			return StructuralError(outError,
				L"InputBindings 包含重复手势：" + binding.Gesture);
		decoded.push_back(std::move(binding));
	}
	bindings = std::move(decoded);
	return true;
}

bool DesignNode::operator==(const DesignNode& other) const
{
	return Id == other.Id
		&& ParentId == other.ParentId
		&& ParentRef == other.ParentRef
		&& Name == other.Name
		&& NameIsGenerated == other.NameIsGenerated
		&& Type == other.Type
		&& XamlType == other.XamlType
		&& ComponentType == other.ComponentType
		&& ComponentContentProperty == other.ComponentContentProperty
		&& PresentedComponentContent == other.PresentedComponentContent
		&& TemplateContentSource == other.TemplateContentSource
		&& Order == other.Order
		&& Locked == other.Locked
		&& Properties == other.Properties
		&& Structure == other.Structure
		&& TemplateState == other.TemplateState
		&& Events == other.Events
		&& Bindings == other.Bindings
		&& CommandBindings == other.CommandBindings
		&& InputBindings == other.InputBindings
		&& LocalResources == other.LocalResources
		&& LocalObjectResources == other.LocalObjectResources
		&& TemplateBindings == other.TemplateBindings
		&& TemplateEventBindings == other.TemplateEventBindings;
}

bool DesignDataTemplate::HasSameResourceIdentity(
	const DesignDataTemplate& other) const noexcept
{
	if (IsImplicit() != other.IsImplicit()) return false;
	const auto& left = IsImplicit() ? DataType : Key;
	const auto& right = other.IsImplicit() ? other.DataType : other.Key;
	return left == right;
}

std::wstring DesignDataTemplate::DisplayName() const
{
	return IsImplicit() ? L"{DataType " + DataType + L"}" : Key;
}

bool DesignControlTemplate::HasSameResourceIdentity(
	const DesignControlTemplate& other) const noexcept
{
	if (IsImplicit() != other.IsImplicit()) return false;
	if (IsImplicit())
	{
		if (TargetComponentType.Empty() != other.TargetComponentType.Empty())
			return false;
		return TargetComponentType.Empty()
			? TargetType == other.TargetType
			: TargetComponentType.RegistryKey()
				== other.TargetComponentType.RegistryKey();
	}
	return Key == other.Key;
}

std::wstring DesignControlTemplate::DisplayName() const
{
	return IsImplicit()
		? L"{TargetType " + (TargetComponentType.Empty()
			? DesignerStyleSheetUtils::UIClassName(TargetType)
			: TargetComponentType.XamlPrefix + L":"
				+ TargetComponentType.XamlName) + L"}"
		: Key;
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const DesignerComponentType& type) const
{
	return FindComponent(type.XamlNamespace, type.XamlName);
}

DesignObjectResourceDictionary DesignDocument::VisibleObjectResources(
	const DesignNode& origin) const
{
	return VisibleObjectResources(Nodes, origin);
}

DesignObjectResourceDictionary DesignDocument::VisibleObjectResources(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin) const
{
	DesignObjectResourceDictionary result;
	auto append = [&](const DesignObjectResourceDictionary& source)
	{
		for (const auto& storyboard : source.Storyboards)
		{
			result.Storyboards.erase(std::remove_if(
				result.Storyboards.begin(), result.Storyboards.end(),
				[&](const auto& current)
				{ return current.Key == storyboard.Key; }),
				result.Storyboards.end());
			result.Storyboards.push_back(storyboard);
		}
		for (const auto& component : source.Components)
		{
			result.Components.erase(std::remove_if(
				result.Components.begin(), result.Components.end(),
				[&](const auto& current)
				{
					return current.Type.XamlNamespace
							== component.Type.XamlNamespace
						&& current.Type.XamlName == component.Type.XamlName;
				}), result.Components.end());
			result.Components.push_back(component);
		}
		for (const auto& dataTemplate : source.DataTemplates)
		{
			result.DataTemplates.erase(std::remove_if(
				result.DataTemplates.begin(), result.DataTemplates.end(),
				[&](const auto& current)
				{ return current.HasSameResourceIdentity(dataTemplate); }),
				result.DataTemplates.end());
			result.DataTemplates.push_back(dataTemplate);
		}
		for (const auto& controlTemplate : source.ControlTemplates)
		{
			result.ControlTemplates.erase(std::remove_if(
				result.ControlTemplates.begin(), result.ControlTemplates.end(),
				[&](const auto& current)
				{ return current.HasSameResourceIdentity(controlTemplate); }),
				result.ControlTemplates.end());
			result.ControlTemplates.push_back(controlTemplate);
		}
		for (const auto& itemsPanel : source.ItemsPanelTemplates)
		{
			result.ItemsPanelTemplates.erase(std::remove_if(
				result.ItemsPanelTemplates.begin(),
				result.ItemsPanelTemplates.end(), [&](const auto& current)
				{ return current.Key == itemsPanel.Key; }),
				result.ItemsPanelTemplates.end());
			result.ItemsPanelTemplates.push_back(itemsPanel);
		}
		for (const auto& groupStyle : source.GroupStyles)
		{
			result.GroupStyles.erase(std::remove_if(
				result.GroupStyles.begin(), result.GroupStyles.end(),
				[&](const auto& current)
				{ return current.Key == groupStyle.Key; }),
				result.GroupStyles.end());
			result.GroupStyles.push_back(groupStyle);
		}
	};
	DesignObjectResourceDictionary documentResources;
	documentResources.Storyboards = Storyboards;
	documentResources.Components = Components;
	documentResources.ControlTemplates = ControlTemplates;
	documentResources.DataTemplates = DataTemplates;
	documentResources.ItemsPanelTemplates = ItemsPanelTemplates;
	documentResources.GroupStyles = GroupStyles;
	append(documentResources);

	std::unordered_map<int, const DesignNode*> byId;
	std::unordered_map<std::wstring, const DesignNode*> byName;
	for (const auto& node : scopeNodes)
	{
		byId.emplace(node.Id, &node);
		byName.emplace(node.Name, &node);
	}
	std::vector<const DesignNode*> route;
	std::unordered_set<int> visited;
	for (const DesignNode* node = &origin;
		node && visited.insert(node->Id).second;)
	{
		route.push_back(node);
		if (node->ParentId > 0)
		{
			const auto parent = byId.find(node->ParentId);
			node = parent == byId.end() ? nullptr : parent->second;
			continue;
		}
		if (node->ParentRef.empty()) break;
		const auto parent = byName.find(node->ParentRef);
		node = parent == byName.end() ? nullptr : parent->second;
	}
	for (auto node = route.rbegin(); node != route.rend(); ++node)
		append((*node)->LocalObjectResources);
	return result;
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const DesignNode& origin,
	const DesignerComponentType& type) const
{
	return FindComponent(Nodes, origin, type);
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin,
	const DesignerComponentType& type) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope;)
	{
		const auto local = std::find_if(
			scope->LocalObjectResources.Components.rbegin(),
			scope->LocalObjectResources.Components.rend(), [&](const auto& component)
			{
				return component.Type.XamlNamespace == type.XamlNamespace
					&& component.Type.XamlName == type.XamlName;
			});
		if (local != scope->LocalObjectResources.Components.rend()) return &*local;
		scope = parentOf(*scope);
	}
	return FindComponent(type);
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const std::wstring& xamlNamespace,
	const std::wstring& xamlName) const
{
	const auto found = std::find_if(Components.begin(), Components.end(),
		[&](const DesignComponentDefinition& component)
		{
			return component.Type.XamlNamespace == xamlNamespace
				&& component.Type.XamlName == xamlName;
		});
	return found == Components.end() ? nullptr : &*found;
}

bool DesignDocument::HasResourceBackedVisualStates() const noexcept
{
	auto componentUsesResources = [](const auto& component)
	{
		auto animationUsesResource = [](const auto& animation)
		{
			return (animation.HasTo && animation.ToUsesResource)
				|| (animation.HasFrom && animation.FromUsesResource)
				|| (animation.HasBy && animation.ByUsesResource)
				|| std::any_of(animation.KeyFrames.begin(),
					animation.KeyFrames.end(), [](const auto& keyFrame)
					{ return keyFrame.UsesResource; });
		};
		return std::any_of(component.VisualStateGroups.begin(),
				component.VisualStateGroups.end(), [&](const auto& group)
				{
					return std::any_of(group.Transitions.begin(),
						group.Transitions.end(), [&](const auto& transition)
						{
							return std::any_of(transition.Animations.begin(),
								transition.Animations.end(), animationUsesResource);
						}) || std::any_of(group.States.begin(), group.States.end(),
						[&](const auto& state)
						{
							return std::any_of(state.Setters.begin(), state.Setters.end(),
								[](const auto& setter) { return setter.UsesResource; })
								|| std::any_of(state.Animations.begin(), state.Animations.end(),
									animationUsesResource);
						});
				}) || std::any_of(component.EventTriggers.begin(),
				component.EventTriggers.end(), [&](const auto& trigger)
				{
					return std::any_of(trigger.Actions.begin(),
						trigger.Actions.end(), [&](const auto& action)
						{
							return std::any_of(action.Animations.begin(),
								action.Animations.end(), animationUsesResource);
						});
				});
	};
	if (std::any_of(Components.begin(), Components.end(),
		componentUsesResources)) return true;
	if (std::any_of(ControlTemplates.begin(), ControlTemplates.end(),
		componentUsesResources)) return true;
	auto nodesUseResources = [&](const auto& self,
		const std::vector<DesignNode>& nodes) -> bool
	{
		for (const auto& node : nodes)
		{
			for (const auto& component
				: node.LocalObjectResources.Components)
				if (componentUsesResources(component)
					|| self(self, component.Template)) return true;
			for (const auto& dataTemplate
				: node.LocalObjectResources.DataTemplates)
				if (self(self, dataTemplate.Template)) return true;
			for (const auto& controlTemplate
				: node.LocalObjectResources.ControlTemplates)
				if (componentUsesResources(controlTemplate)
					|| self(self, controlTemplate.Template)) return true;
		}
		return false;
	};
	if (nodesUseResources(nodesUseResources, Nodes)) return true;
	for (const auto& component : Components)
		if (nodesUseResources(nodesUseResources, component.Template)) return true;
	for (const auto& dataTemplate : DataTemplates)
		if (nodesUseResources(nodesUseResources, dataTemplate.Template)) return true;
	for (const auto& controlTemplate : ControlTemplates)
		if (nodesUseResources(nodesUseResources, controlTemplate.Template)) return true;
	return false;
}

const DesignDataTypeDefinition* DesignDocument::FindDataType(
	const std::wstring& name) const
{
	const auto found = std::find_if(DataTypes.begin(), DataTypes.end(),
		[&](const DesignDataTypeDefinition& type)
		{
			return type.Name == name;
		});
	return found == DataTypes.end() ? nullptr : &*found;
}

const DesignDataTemplate* DesignDocument::FindDataTemplate(
	const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	const auto found = std::find_if(DataTemplates.begin(), DataTemplates.end(),
		[&](const DesignDataTemplate& item)
		{
			return item.Key == key;
		});
	return found == DataTemplates.end() ? nullptr : &*found;
}

const DesignStoryboardResource* DesignDocument::FindStoryboard(
	const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	const auto found = std::find_if(Storyboards.rbegin(), Storyboards.rend(),
		[&](const auto& item) { return item.Key == key; });
	return found == Storyboards.rend() ? nullptr : &*found;
}

const DesignDataTemplate* DesignDocument::FindDataTemplate(
	const DesignNode& origin,
	const std::wstring& key) const
{
	return FindDataTemplate(Nodes, origin, key);
}

const DesignDataTemplate* DesignDocument::FindDataTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin,
	const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope;)
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.DataTemplates.rbegin(),
			scope->LocalObjectResources.DataTemplates.rend(),
			[&](const auto& item)
			{ return item.Key == key; });
		if (found != scope->LocalObjectResources.DataTemplates.rend()) return &*found;
		scope = parentOf(*scope);
	}
	return FindDataTemplate(key);
}

const DesignDataTemplate* DesignDocument::FindImplicitDataTemplate(
	const std::wstring& dataType) const
{
	if (dataType.empty()) return nullptr;
	const auto found = std::find_if(DataTemplates.rbegin(), DataTemplates.rend(),
		[&](const DesignDataTemplate& item)
		{
			return item.IsImplicit() && item.DataType == dataType;
		});
	return found == DataTemplates.rend() ? nullptr : &*found;
}

const DesignDataTemplate* DesignDocument::FindImplicitDataTemplate(
	const DesignNode& origin, const std::wstring& dataType) const
{
	return FindImplicitDataTemplate(Nodes, origin, dataType);
}

const DesignDataTemplate* DesignDocument::FindImplicitDataTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& dataType) const
{
	if (dataType.empty()) return nullptr;
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.DataTemplates.rbegin(),
			scope->LocalObjectResources.DataTemplates.rend(),
			[&](const auto& item)
			{
				return item.IsImplicit() && item.DataType == dataType;
			});
		if (found != scope->LocalObjectResources.DataTemplates.rend()) return &*found;
	}
	return FindImplicitDataTemplate(dataType);
}

const DesignControlTemplate* DesignDocument::FindControlTemplate(
	const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	const auto found = std::find_if(
		ControlTemplates.rbegin(), ControlTemplates.rend(),
		[&](const DesignControlTemplate& item)
		{ return !item.IsImplicit() && item.Key == key; });
	return found == ControlTemplates.rend() ? nullptr : &*found;
}

const DesignControlTemplate* DesignDocument::FindControlTemplate(
	const DesignNode& origin, const std::wstring& key) const
{
	return FindControlTemplate(Nodes, origin, key);
}

const DesignControlTemplate* DesignDocument::FindControlTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ControlTemplates.rbegin(),
			scope->LocalObjectResources.ControlTemplates.rend(),
			[&](const auto& item)
			{ return !item.IsImplicit() && item.Key == key; });
		if (found != scope->LocalObjectResources.ControlTemplates.rend())
			return &*found;
	}
	return FindControlTemplate(key);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	UIClass targetType) const
{
	const auto found = std::find_if(
		ControlTemplates.rbegin(), ControlTemplates.rend(),
		[&](const DesignControlTemplate& item)
		{ return item.IsImplicit() && item.TargetComponentType.Empty()
			&& item.TargetType == targetType; });
	return found == ControlTemplates.rend() ? nullptr : &*found;
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const DesignerComponentType& targetType) const
{
	if (targetType.Empty()) return nullptr;
	const auto key = targetType.RegistryKey();
	const auto found = std::find_if(
		ControlTemplates.rbegin(), ControlTemplates.rend(),
		[&](const DesignControlTemplate& item)
		{ return item.IsImplicit() && !item.TargetComponentType.Empty()
			&& item.TargetComponentType.RegistryKey() == key; });
	return found == ControlTemplates.rend() ? nullptr : &*found;
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const DesignNode& origin, UIClass targetType) const
{
	return FindImplicitControlTemplate(Nodes, origin, targetType);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const DesignNode& origin,
	const DesignerComponentType& targetType) const
{
	return FindImplicitControlTemplate(Nodes, origin, targetType);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, UIClass targetType) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ControlTemplates.rbegin(),
			scope->LocalObjectResources.ControlTemplates.rend(),
			[&](const auto& item)
			{ return item.IsImplicit() && item.TargetComponentType.Empty()
				&& item.TargetType == targetType; });
		if (found != scope->LocalObjectResources.ControlTemplates.rend())
			return &*found;
	}
	return FindImplicitControlTemplate(targetType);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin,
	const DesignerComponentType& targetType) const
{
	if (targetType.Empty()) return nullptr;
	const auto key = targetType.RegistryKey();
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			if (found != scopeNodes.end()) return &*found;
		}
		if (!node.ParentRef.empty())
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate)
				{ return candidate.Name == node.ParentRef; });
			if (found != scopeNodes.end()) return &*found;
		}
		return nullptr;
	};
	std::unordered_set<int> visited;
	for (const DesignNode* scope = &origin;
		scope && visited.insert(scope->Id).second; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ControlTemplates.rbegin(),
			scope->LocalObjectResources.ControlTemplates.rend(),
			[&](const auto& item)
			{ return item.IsImplicit() && !item.TargetComponentType.Empty()
				&& item.TargetComponentType.RegistryKey() == key; });
		if (found != scope->LocalObjectResources.ControlTemplates.rend())
			return &*found;
	}
	return FindImplicitControlTemplate(targetType);
}

const DesignItemsPanelTemplate* DesignDocument::FindItemsPanelTemplate(
	const std::wstring& key) const
{
	const auto found = std::find_if(
		ItemsPanelTemplates.begin(), ItemsPanelTemplates.end(),
		[&](const auto& item)
		{
			return item.Key == key;
		});
	return found == ItemsPanelTemplates.end() ? nullptr : &*found;
}

const DesignItemsPanelTemplate* DesignDocument::FindItemsPanelTemplate(
	const DesignNode& origin, const std::wstring& key) const
{
	return FindItemsPanelTemplate(Nodes, origin, key);
}

const DesignItemsPanelTemplate* DesignDocument::FindItemsPanelTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ItemsPanelTemplates.rbegin(),
			scope->LocalObjectResources.ItemsPanelTemplates.rend(),
			[&](const auto& item)
			{ return item.Key == key; });
		if (found != scope->LocalObjectResources.ItemsPanelTemplates.rend())
			return &*found;
	}
	return FindItemsPanelTemplate(key);
}

const DesignGroupStyle* DesignDocument::FindGroupStyle(
	const std::wstring& key) const
{
	const auto found = std::find_if(GroupStyles.begin(), GroupStyles.end(),
		[&](const DesignGroupStyle& item)
		{
			return item.Key == key;
		});
	return found == GroupStyles.end() ? nullptr : &*found;
}

const DesignGroupStyle* DesignDocument::FindGroupStyle(
	const DesignNode& origin, const std::wstring& key) const
{
	return FindGroupStyle(Nodes, origin, key);
}

const DesignGroupStyle* DesignDocument::FindGroupStyle(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.GroupStyles.rbegin(),
			scope->LocalObjectResources.GroupStyles.rend(),
			[&](const auto& item)
			{ return item.Key == key; });
		if (found != scope->LocalObjectResources.GroupStyles.rend()) return &*found;
	}
	return FindGroupStyle(key);
}

const DesignDataTemplate* DesignDocument::FindGroupStyleHeaderTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& groupStyleKey) const
{
	if (const auto* scope = FindLocalGroupStyleOwner(
		scopeNodes, origin, groupStyleKey))
	{
		const auto style = std::find_if(
			scope->LocalObjectResources.GroupStyles.rbegin(),
			scope->LocalObjectResources.GroupStyles.rend(), [&](const auto& item)
			{ return item.Key == groupStyleKey; });
		if (style == scope->LocalObjectResources.GroupStyles.rend()) return nullptr;
		return style->HeaderTemplate.empty()
			? FindImplicitDataTemplate(scopeNodes, *scope,
				std::wstring(CollectionViewGroupDataTypeName))
			: FindDataTemplate(scopeNodes, *scope, style->HeaderTemplate);
	}
	const auto* style = FindGroupStyle(groupStyleKey);
	if (!style) return nullptr;
	return style->HeaderTemplate.empty()
		? FindImplicitDataTemplate(std::wstring(CollectionViewGroupDataTypeName))
		: FindDataTemplate(style->HeaderTemplate);
}

const DesignNode* DesignDocument::FindLocalGroupStyleOwner(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return candidate.Name == node.ParentRef; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
		if (std::any_of(
			scope->LocalObjectResources.GroupStyles.rbegin(),
			scope->LocalObjectResources.GroupStyles.rend(), [&](const auto& item)
			{ return item.Key == key; })) return scope;
	return nullptr;
}

const DesignDataList* DesignDocument::FindDataList(
	const std::wstring& key) const
{
	const auto found = std::find_if(DataLists.begin(), DataLists.end(),
		[&](const DesignDataList& item)
		{
			return item.Key == key;
		});
	return found == DataLists.end() ? nullptr : &*found;
}

const DesignCollectionViewSource* DesignDocument::FindCollectionView(
	const std::wstring& key) const
{
	const auto found = std::find_if(
		CollectionViews.begin(), CollectionViews.end(),
		[&](const DesignCollectionViewSource& item)
		{
			return item.Key == key;
		});
	return found == CollectionViews.end() ? nullptr : &*found;
}

int DesignDocument::AllocateNodeId()
{
	if (NextStableId < 1
		|| NextStableId == (std::numeric_limits<int>::max)())
		throw std::overflow_error("Designer stable node id space exhausted");
	return NextStableId++;
}

void DesignDocument::RecalculateNextStableId()
{
	int maxId = 0;
	for (const auto& node : Nodes)
	{
		maxId = (std::max)(maxId, node.Id);
	}
	if (maxId == (std::numeric_limits<int>::max)())
		throw std::overflow_error("Designer stable node id space exhausted");
	NextStableId = (std::max)(1, maxId + 1);
}

bool DesignDocument::ValidateCommandTargetReferences(
	std::wstring* outError) const
{
	auto validateScope = [&](auto&& self,
		const std::vector<DesignNode>& nodes,
		const std::wstring& owner,
		const std::vector<DesignInputBinding>* ownerBindings,
		const std::wstring& ownerName) -> bool
	{
		std::unordered_set<std::wstring> names;
		for (const auto& node : nodes) names.insert(node.Name);
		if (!ownerName.empty()) names.insert(ownerName);
		auto validateTarget = [&](const std::wstring& target,
			const std::wstring& sourceName,
			const std::wstring& propertyName)
		{
			if (target.empty() || names.contains(target)) return true;
			if (outError) *outError = owner + L" / " + sourceName + L" "
				+ propertyName + L" 引用了当前 namescope 中不存在的 x:Name："
				+ target;
			return false;
		};
		auto validateInputBindings = [&](const std::vector<DesignInputBinding>& bindings,
			const std::wstring& sourceName)
		{
			for (const auto& binding : bindings)
			{
				if (!validateTarget(binding.CommandTarget, sourceName,
					L"InputBinding.CommandTarget")) return false;
			}
			return true;
		};
		if (ownerBindings && !validateInputBindings(*ownerBindings,
			ownerName.empty() ? L"owner" : ownerName)) return false;
		for (const auto& node : nodes)
		{
			if (!validateInputBindings(node.InputBindings, node.Name)) return false;
			if (!node.Structure.CommandTarget.empty()
				&& node.Type != UIClass::UI_Button
				&& node.Type != UIClass::UI_MenuItem)
			{
				if (outError) *outError = owner + L" / " + node.Name
					+ L" CommandTarget 仅适用于 Button 或 MenuItem。";
				return false;
			}
			if (!validateTarget(node.Structure.CommandTarget, node.Name,
				L"CommandTarget")) return false;
		}
		for (const auto& node : nodes)
		{
			const auto resourceOwner = owner + L" / " + node.Name
				+ L".Resources";
			for (const auto& component
				: node.LocalObjectResources.Components)
				if (!self(self, component.Template,
					resourceOwner + L" / 组件 " + component.Type.XamlName,
					nullptr, {})) return false;
			for (const auto& dataTemplate
				: node.LocalObjectResources.DataTemplates)
				if (!self(self, dataTemplate.Template,
					resourceOwner + L" / DataTemplate "
						+ dataTemplate.DisplayName(),
					nullptr, {})) return false;
			for (const auto& controlTemplate
				: node.LocalObjectResources.ControlTemplates)
				if (!self(self, controlTemplate.Template,
					resourceOwner + L" / ControlTemplate "
						+ controlTemplate.DisplayName(),
					nullptr, {})) return false;
		}
		return true;
	};

	if (!validateScope(validateScope, Nodes, L"文档",
		&Window.InputBindings, Window.Name)) return false;
	for (const auto& component : Components)
		if (!validateScope(validateScope, component.Template,
			L"组件 " + component.Type.XamlName, nullptr, {})) return false;
	for (const auto& dataTemplate : DataTemplates)
		if (!validateScope(validateScope, dataTemplate.Template,
			L"DataTemplate " + dataTemplate.DisplayName(), nullptr, {}))
			return false;
	for (const auto& controlTemplate : ControlTemplates)
		if (!validateScope(validateScope, controlTemplate.Template,
			L"ControlTemplate " + controlTemplate.DisplayName(), nullptr, {}))
			return false;
	if (outError) outError->clear();
	return true;
}

bool DesignDocument::ValidateDataGridColumnBindingSources(
	std::wstring* outError) const
{
	auto validateScope = [&](auto&& self,
		const std::vector<DesignNode>& nodes,
		const std::wstring& owner) -> bool
	{
		std::unordered_map<std::wstring, const DesignNode*> named;
		for (const auto& node : nodes)
			if (!node.Name.empty() && !node.NameIsGenerated)
				named.emplace(node.Name, &node);
		auto validateBinding = [&](const DesignNode& grid,
			const DesignerDataBinding& binding,
			const std::wstring& propertyName,
			auto&& validateBindingSelf) -> bool
		{
			if (binding.IsMultiBinding())
			{
				std::wstring bindingError;
				if (!DesignerBindingUtils::ValidateDataGridColumnBindingSource(
					binding, nullptr, &bindingError))
				{
					if (outError) *outError = owner + L" / " + grid.Name
						+ L" " + propertyName + L" 无效：" + bindingError;
					return false;
				}
				for (size_t index = 0;
					index < binding.ChildBindings.size(); ++index)
					if (!validateBindingSelf(grid, binding.ChildBindings[index],
						propertyName + L".Bindings["
							+ std::to_wstring(index) + L"]",
						validateBindingSelf)) return false;
				return true;
			}
			UIClass sourceType = UIClass::UI_Base;
			if (!binding.ElementName.empty())
			{
				const auto source = named.find(binding.ElementName);
				if (source == named.end())
				{
					if (outError) *outError = owner + L" / " + grid.Name
						+ L" " + propertyName
						+ L" 引用了当前 namescope 中不存在的具名内建控件："
						+ binding.ElementName;
					return false;
				}
				sourceType = source->second->Type;
			}
			std::wstring bindingError;
			if (DesignerBindingUtils::ValidateDataGridColumnBindingSource(
				binding, nullptr, &bindingError, sourceType)) return true;
			if (outError) *outError = owner + L" / " + grid.Name
				+ L" " + propertyName + L" 无效：" + bindingError;
			return false;
		};
		for (const auto& node : nodes)
		{
			if (!node.Structure.DataGridColumns) continue;
			for (size_t index = 0;
				index < node.Structure.DataGridColumns->size(); ++index)
			{
				const auto& column = (*node.Structure.DataGridColumns)[index];
				const auto prefix = L"DataGrid.Columns["
					+ std::to_wstring(index) + L"]";
				if (column.Binding && !validateBinding(
					node, *column.Binding, prefix + L".Binding",
					validateBinding)) return false;
				if (column.ContentBinding && !validateBinding(
					node, *column.ContentBinding,
					prefix + L".ContentBinding", validateBinding)) return false;
			}
		}
		for (const auto& node : nodes)
		{
			const auto resourceOwner = owner + L" / " + node.Name
				+ L".Resources";
			for (const auto& component
				: node.LocalObjectResources.Components)
				if (!self(self, component.Template,
					resourceOwner + L" / 组件 " + component.Type.XamlName))
					return false;
			for (const auto& dataTemplate
				: node.LocalObjectResources.DataTemplates)
				if (!self(self, dataTemplate.Template,
					resourceOwner + L" / DataTemplate "
						+ dataTemplate.DisplayName())) return false;
			for (const auto& controlTemplate
				: node.LocalObjectResources.ControlTemplates)
				if (!self(self, controlTemplate.Template,
					resourceOwner + L" / ControlTemplate "
						+ controlTemplate.DisplayName())) return false;
		}
		return true;
	};

	if (!validateScope(validateScope, Nodes, L"文档")) return false;
	for (const auto& component : Components)
		if (!validateScope(validateScope, component.Template,
			L"组件 " + component.Type.XamlName)) return false;
	for (const auto& dataTemplate : DataTemplates)
		if (!validateScope(validateScope, dataTemplate.Template,
			L"DataTemplate " + dataTemplate.DisplayName())) return false;
	for (const auto& controlTemplate : ControlTemplates)
		if (!validateScope(validateScope, controlTemplate.Template,
			L"ControlTemplate " + controlTemplate.DisplayName())) return false;
	if (outError) outError->clear();
	return true;
}

bool DesignDocument::ValidateRichTextStructure(
	std::wstring* outError) const
{
	auto fail = [&](const std::wstring& owner,
		const DesignNode& node,
		const std::wstring& message)
	{
		if (outError)
			*outError = owner + L" / " + node.Name + L"：" + message;
		return false;
	};
	auto validateNode = [&](const DesignNode& node,
		const std::wstring& owner)
	{
		if (!node.Structure.Document) return true;
		if (node.Type != UIClass::UI_RichTextBox)
			return fail(owner, node,
				L"Document 结构仅适用于 RichTextBox。");
		if (node.Properties.Find(L"Text")
			|| node.Bindings.contains(L"Text")
			|| node.TemplateBindings.contains(L"Text"))
		{
			return fail(owner, node,
				L"RichTextBox.Text 不能与 Document 同时使用。");
		}

		DesignNodeStructure richTextOnly;
		richTextOnly.Document = node.Structure.Document;
		DesignNodeStructure decoded;
		std::wstring structuralError;
		if (!DecodeDesignNodeStructure(
			node.Type,
			EncodeDesignNodeStructure(node.Type, richTextOnly),
			decoded,
			&structuralError))
		{
			return fail(owner, node,
				L"Document 结构无效：" + structuralError);
		}
		if (decoded.Document != node.Structure.Document)
			return fail(owner, node,
				L"Document 结构无法无损规范化。");
		return true;
	};
	auto validateScope = [&](auto&& self,
		const std::vector<DesignNode>& nodes,
		const std::wstring& owner) -> bool
	{
		for (const auto& node : nodes)
			if (!validateNode(node, owner)) return false;
		for (const auto& node : nodes)
		{
			const auto resourceOwner = owner + L" / " + node.Name
				+ L".Resources";
			for (const auto& component
				: node.LocalObjectResources.Components)
			{
				if (!self(self, component.Template,
					resourceOwner + L" / 组件 "
						+ component.Type.XamlName)) return false;
			}
			for (const auto& dataTemplate
				: node.LocalObjectResources.DataTemplates)
			{
				if (!self(self, dataTemplate.Template,
					resourceOwner + L" / DataTemplate "
						+ dataTemplate.DisplayName())) return false;
			}
			for (const auto& controlTemplate
				: node.LocalObjectResources.ControlTemplates)
			{
				if (!self(self, controlTemplate.Template,
					resourceOwner + L" / ControlTemplate "
						+ controlTemplate.DisplayName())) return false;
			}
		}
		return true;
	};

	if (!validateNode(Window, L"文档根")) return false;
	if (!validateScope(validateScope, Nodes, L"文档")) return false;
	for (const auto& component : Components)
		if (!validateScope(validateScope, component.Template,
			L"组件 " + component.Type.XamlName)) return false;
	for (const auto& dataTemplate : DataTemplates)
		if (!validateScope(validateScope, dataTemplate.Template,
			L"DataTemplate " + dataTemplate.DisplayName())) return false;
	for (const auto& controlTemplate : ControlTemplates)
		if (!validateScope(validateScope, controlTemplate.Template,
			L"ControlTemplate " + controlTemplate.DisplayName())) return false;
	if (outError) outError->clear();
	return true;
}

void DesignDocument::Clear()
{
	*this = DesignDocument();
}

bool DesignDocument::operator==(const DesignDocument& other) const
{
	return Schema == other.Schema
		&& SchemaVersion == other.SchemaVersion
		&& NextStableId == other.NextStableId
		&& Window == other.Window
		&& CodeBehind == other.CodeBehind
		&& DataContextSchema == other.DataContextSchema
		&& StyleSheet == other.StyleSheet
		&& Storyboards == other.Storyboards
		&& Components == other.Components
		&& ControlTemplates == other.ControlTemplates
		&& DataTypes == other.DataTypes
		&& DataTemplates == other.DataTemplates
		&& ItemsPanelTemplates == other.ItemsPanelTemplates
		&& GroupStyles == other.GroupStyles
		&& DataLists == other.DataLists
		&& CollectionViews == other.CollectionViews
		&& Nodes == other.Nodes;
}
}
