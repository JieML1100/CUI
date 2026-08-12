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

	inline size_t TextFormattingHeap(
		const DesignerModel::DesignTextFormatting& value) noexcept
	{
		size_t result = 0;
		if (value.FontFamily) result += StringHeap(*value.FontFamily);
		if (value.Language) result += StringHeap(*value.Language);
		if (value.FontWeight) result += StringHeap(*value.FontWeight);
		if (value.FontStretch) result += StringHeap(*value.FontStretch);
		if (value.FontStyle) result += StringHeap(*value.FontStyle);
		return result;
	}

	inline size_t BindingHeap(const DesignerDataBinding& value) noexcept
	{
		size_t result = StringHeap(value.SourceProperty)
			+ StringHeap(value.Converter)
			+ StringHeap(value.ElementName)
			+ StringHeap(value.AncestorType)
			+ StringHeap(value.AncestorTypeNamespace)
			+ VectorHeap(value.ChildBindings);
		if (value.FallbackValue) result += StringHeap(value.FallbackValue->Text);
		if (value.TargetNullValue) result += StringHeap(value.TargetNullValue->Text);
		if (value.ConverterParameter)
			result += StringHeap(value.ConverterParameter->Text);
		if (value.StringFormat) result += StringHeap(*value.StringFormat);
		for (const auto& child : value.ChildBindings)
			result += BindingHeap(child);
		return result;
	}

	inline size_t StructureHeap(
		const DesignerModel::DesignNodeStructure& value) noexcept
	{
		auto inlineHeap = [&](auto&& self,
			const DesignerModel::DesignInline& inlineValue) noexcept
			-> size_t
		{
			size_t total = TextFormattingHeap(inlineValue)
				+ StringHeap(inlineValue.Text)
				+ VectorHeap(inlineValue.Inlines);
			for (const auto& child : inlineValue.Inlines)
				total += self(self, child);
			return total;
		};
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
		if (value.DataGridColumns)
		{
			result += VectorHeap(*value.DataGridColumns);
			for (const auto& column : *value.DataGridColumns)
			{
				result += StringHeap(column.Header)
					+ StringHeap(column.SortMemberPath)
					+ StringHeap(column.CellTemplate)
					+ StringHeap(column.CellEditingTemplate);
				if (column.Binding) result += BindingHeap(*column.Binding);
			}
		}
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
		if (value.Document)
		{
			result += TextFormattingHeap(*value.Document)
				+ VectorHeap(value.Document->Paragraphs);
			for (const auto& paragraph : value.Document->Paragraphs)
			{
				result += TextFormattingHeap(paragraph)
					+ VectorHeap(paragraph.Inlines);
				for (const auto& inlineValue : paragraph.Inlines)
					result += inlineHeap(inlineHeap, inlineValue);
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
