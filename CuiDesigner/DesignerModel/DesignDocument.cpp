#include "DesignDocument.h"
#include "../DesignerEventCatalog.h"
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace DesignerModel
{
bool DesignFormModel::operator==(const DesignFormModel& other) const
{
	return Name == other.Name
		&& Text == other.Text
		&& FontName == other.FontName
		&& FontSize == other.FontSize
		&& Size.cx == other.Size.cx
		&& Size.cy == other.Size.cy
		&& Location.x == other.Location.x
		&& Location.y == other.Location.y
		&& BackColor.r == other.BackColor.r
		&& BackColor.g == other.BackColor.g
		&& BackColor.b == other.BackColor.b
		&& BackColor.a == other.BackColor.a
		&& ForeColor.r == other.ForeColor.r
		&& ForeColor.g == other.ForeColor.g
		&& ForeColor.b == other.ForeColor.b
		&& ForeColor.a == other.ForeColor.a
		&& ShowInTaskBar == other.ShowInTaskBar
		&& TopMost == other.TopMost
		&& Enable == other.Enable
		&& Visible == other.Visible
		&& VisibleHead == other.VisibleHead
		&& HeadHeight == other.HeadHeight
		&& MinBox == other.MinBox
		&& MaxBox == other.MaxBox
		&& CloseBox == other.CloseBox
		&& CenterTitle == other.CenterTitle
		&& AllowResize == other.AllowResize
		&& EventHandlers == other.EventHandlers;
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

bool DesignNode::operator==(const DesignNode& other) const
{
	return Id == other.Id
		&& ParentId == other.ParentId
		&& ParentRef == other.ParentRef
		&& Name == other.Name
		&& Type == other.Type
		&& CustomType == other.CustomType
		&& CustomEvents == other.CustomEvents
		&& Order == other.Order
		&& Props == other.Props
		&& Extra == other.Extra
		&& Events == other.Events
		&& Bindings == other.Bindings;
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

void DesignDocument::Clear()
{
	*this = DesignDocument();
}

bool DesignDocument::operator==(const DesignDocument& other) const
{
	return Schema == other.Schema
		&& SchemaVersion == other.SchemaVersion
		&& NextStableId == other.NextStableId
		&& Form == other.Form
		&& CodeBehind == other.CodeBehind
		&& DataContextSchema == other.DataContextSchema
		&& StyleSheet == other.StyleSheet
		&& Nodes == other.Nodes;
}
}
