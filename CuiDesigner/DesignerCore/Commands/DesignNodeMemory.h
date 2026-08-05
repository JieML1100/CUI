#pragma once

#include "../../DesignerModel/DesignDocument.h"

#include <cstddef>
#include <string>
#include <vector>

namespace DesignerCommandMemory
{
	inline size_t StringHeap(const std::wstring& value) noexcept
	{
		return value.capacity() * sizeof(std::wstring::value_type);
	}

	template<typename T>
	inline size_t VectorHeap(const std::vector<T>& value) noexcept
	{
		return value.capacity() * sizeof(T);
	}

	inline size_t StringVectorHeap(
		const std::vector<std::wstring>& values) noexcept
	{
		size_t result = VectorHeap(values);
		for (const auto& value : values) result += StringHeap(value);
		return result;
	}

	inline size_t StructureHeap(
		const DesignerModel::DesignNodeStructure& value) noexcept
	{
		size_t result = StringHeap(value.CommandTarget)
			+ StringHeap(value.ItemsSourceResource)
			+ StringHeap(value.ItemTemplate)
			+ StringHeap(value.ContentTemplate)
			+ StringHeap(value.HeaderTemplate)
			+ StringHeap(value.ControlTemplate)
			+ StringHeap(value.GroupStyle)
			+ StringHeap(value.ItemsPanel)
			+ StringHeap(value.ItemContainerStyle);
		if (value.RelativePanel)
		{
			const auto& constraints = *value.RelativePanel;
			auto add = [&](const auto& item)
			{
				if (item) result += StringHeap(*item);
			};
			add(constraints.Above);
			add(constraints.Below);
			add(constraints.LeftOf);
			add(constraints.RightOf);
			add(constraints.AlignLeftWith);
			add(constraints.AlignRightWith);
			add(constraints.AlignTopWith);
			add(constraints.AlignBottomWith);
		}
		if (value.GridRows) result += VectorHeap(*value.GridRows);
		if (value.GridColumns) result += VectorHeap(*value.GridColumns);
		if (value.ChartSeries)
		{
			result += VectorHeap(*value.ChartSeries);
			for (const auto& series : *value.ChartSeries)
			{
				result += StringHeap(series.Name) + VectorHeap(series.Points);
				for (const auto& point : series.Points)
					result += StringHeap(point.Label);
			}
		}
		return result;
	}

	inline size_t TemplateStateHeap(
		const DesignerModel::DesignNodeTemplateState& value) noexcept
	{
		return StringHeap(value.Owner)
			+ StringHeap(value.ContentOwner)
			+ StringHeap(value.PartName)
			+ StringHeap(value.AppliedControlTemplate)
			+ StringHeap(value.AppliedControlTemplateResource)
			+ StringHeap(value.ControlTemplateChain);
	}
}
