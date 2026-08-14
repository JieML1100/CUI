#include "DesignerStructureEdit.h"

#include "../CUI/include/Layout/Grid.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace
{
	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(value)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	bool ValidTrack(const DesignerGridTrackSnapshot& track) noexcept
	{
		const auto unit = static_cast<SizeUnit>(track.Unit);
		return (unit == SizeUnit::Pixel
				|| unit == SizeUnit::Auto
				|| unit == SizeUnit::Star)
			&& std::isfinite(track.Value)
			&& std::isfinite(track.Minimum)
			&& std::isfinite(track.Maximum)
			&& track.Minimum >= 0.0f
			&& track.Maximum >= track.Minimum;
	}

}

size_t DesignerStructureSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	size_t result = sizeof(*this) + StringMemory(TargetName)
		+ GridRows.capacity() * sizeof(DesignerGridTrackSnapshot)
		+ GridColumns.capacity() * sizeof(DesignerGridTrackSnapshot);
	return result;
}

bool DesignerStructureEdit::SupportsDelta(
	DesignerCustomEditorKind kind) noexcept
{
	return kind == DesignerCustomEditorKind::GridDefinitions;
}

bool DesignerStructureEdit::Capture(
	DesignerControl& control,
	DesignerCustomEditorKind kind,
	DesignerStructureSnapshot& output,
	std::wstring* outError)
{
	if (!SupportsDelta(kind))
		return Fail(L"该结构编辑器需要完整文档事务。", outError);
	if (!control.ControlInstance || control.StableId < 1
		|| control.Name.empty())
		return Fail(L"结构差量的目标控件身份无效。", outError);

	try
	{
		DesignerStructureSnapshot candidate;
		candidate.StableId = control.StableId;
		candidate.TargetName = control.Name;
		candidate.TargetType = control.Type;
		candidate.Kind = kind;
		switch (kind)
		{
		case DesignerCustomEditorKind::GridDefinitions:
		{
			auto* grid = dynamic_cast<Grid*>(control.ControlInstance);
			if (!grid || control.Type != UIClass::UI_Grid)
				return Fail(L"Grid 定义差量与目标控件类型不匹配。", outError);
			candidate.GridRows.reserve(grid->GetRows().size());
			for (const auto& row : grid->GetRows())
				candidate.GridRows.push_back({ row.Height.Value,
					static_cast<int>(row.Height.Unit), row.MinHeight, row.MaxHeight });
			candidate.GridColumns.reserve(grid->GetColumns().size());
			for (const auto& column : grid->GetColumns())
				candidate.GridColumns.push_back({ column.Width.Value,
					static_cast<int>(column.Width.Unit), column.MinWidth, column.MaxWidth });
			break;
		}
		default:
			return Fail(L"该结构编辑器需要完整文档事务。", outError);
		}
		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		return Fail(L"捕获结构差量时资源分配失败。", outError);
	}
}

bool DesignerStructureEdit::Restore(
	DesignerControl& control,
	const DesignerStructureSnapshot& snapshot,
	std::wstring* outError)
{
	if (!control.ControlInstance || control.StableId != snapshot.StableId
		|| control.Name != snapshot.TargetName
		|| control.Type != snapshot.TargetType)
		return Fail(L"结构差量的目标控件身份已经变化。", outError);
	try
	{
		switch (snapshot.Kind)
		{
		case DesignerCustomEditorKind::GridDefinitions:
		{
			auto* grid = dynamic_cast<Grid*>(control.ControlInstance);
			if (!grid || control.Type != UIClass::UI_Grid)
				return Fail(L"Grid 定义差量与目标控件类型不匹配。", outError);
			if (!std::all_of(snapshot.GridRows.begin(), snapshot.GridRows.end(), ValidTrack)
				|| !std::all_of(snapshot.GridColumns.begin(), snapshot.GridColumns.end(), ValidTrack))
				return Fail(L"Grid 定义快照包含无效值。", outError);
			grid->ClearRows();
			grid->ClearColumns();
			for (const auto& row : snapshot.GridRows)
				grid->AddRow(GridLength(row.Value, static_cast<SizeUnit>(row.Unit)),
					row.Minimum, row.Maximum);
			for (const auto& column : snapshot.GridColumns)
				grid->AddColumn(GridLength(column.Value, static_cast<SizeUnit>(column.Unit)),
					column.Minimum, column.Maximum);
			break;
		}
		default:
			return Fail(L"该结构编辑器需要完整文档事务。", outError);
		}
		control.ControlInstance->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		return Fail(L"恢复结构差量时资源分配失败。", outError);
	}
}
