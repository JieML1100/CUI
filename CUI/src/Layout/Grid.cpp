#include "Layout/Grid.h"
#include "Window.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

// GridLayoutEngine 实现

namespace
{
	constexpr float TrackEpsilon = 0.0001f;

	struct ContentRequest final
	{
		int Start = 0;
		int Span = 1;
		float Required = 0.0f;
	};

	float NormalizeMinimum(float value)
	{
		return std::isfinite(value) && value > 0.0f ? value : 0.0f;
	}

	float NormalizeMaximum(float value, float minimum)
	{
		return std::isfinite(value) && value >= minimum ? value : FLT_MAX;
	}

	float NormalizeTrackValue(float value)
	{
		return std::isfinite(value) && value > 0.0f ? value : 0.0f;
	}

	bool IsContentSized(const GridLength& length, bool bounded)
	{
		return length.IsAuto()
			|| (!bounded && length.IsStar());
	}

	int ClampTrackStart(int start, int count)
	{
		if (count <= 0) return 0;
		return (std::clamp)(start, 0, count - 1);
	}

	int ClampTrackSpan(int start, int span, int count)
	{
		return (std::clamp)(span, 1, (std::max)(1, count - start));
	}

	float SumSpan(const std::vector<float>& sizes, int start, int span)
	{
		float result = 0.0f;
		const int end = (std::min)(start + span, static_cast<int>(sizes.size()));
		for (int index = start; index < end; ++index)
			result += sizes[static_cast<size_t>(index)];
		return result;
	}

	template<typename TDefinition, typename TLength, typename TMaximum>
	void GrowContentTracks(
		std::vector<float>& sizes,
		const std::vector<TDefinition>& definitions,
		const ContentRequest& request,
		bool bounded,
		TLength getLength,
		TMaximum getMaximum)
	{
		float deficit = request.Required - SumSpan(sizes, request.Start, request.Span);
		if (!(deficit > TrackEpsilon)) return;

		std::vector<int> active;
		const int end = (std::min)(request.Start + request.Span, static_cast<int>(definitions.size()));
		for (int index = request.Start; index < end; ++index)
		{
			if (IsContentSized(getLength(definitions[static_cast<size_t>(index)]), bounded)
				&& getMaximum(definitions[static_cast<size_t>(index)])
					- sizes[static_cast<size_t>(index)] > TrackEpsilon)
				active.push_back(index);
		}

		while (deficit > TrackEpsilon && !active.empty())
		{
			const float share = deficit / static_cast<float>(active.size());
			float distributed = 0.0f;
			std::vector<int> next;
			for (int index : active)
			{
				const size_t track = static_cast<size_t>(index);
				const float capacity = (std::max)(0.0f, getMaximum(definitions[track]) - sizes[track]);
				const float amount = (std::min)(share, capacity);
				sizes[track] += amount;
				distributed += amount;
				if (capacity - amount > TrackEpsilon)
					next.push_back(index);
			}

			if (!(distributed > TrackEpsilon)) break;
			deficit -= distributed;
			active = std::move(next);
		}
	}

	template<typename TDefinition, typename TLength, typename TMinimum, typename TMaximum>
	void ResolveStarTracks(
		std::vector<float>& sizes,
		const std::vector<TDefinition>& definitions,
		float available,
		float occupied,
		TLength getLength,
		TMinimum getMinimum,
		TMaximum getMaximum)
	{
		const double target = static_cast<double>((std::max)(0.0f, available - occupied));
		std::vector<size_t> stars;
		double minimumTotal = 0.0;
		double maximumTotal = 0.0;
		for (size_t index = 0; index < definitions.size(); ++index)
		{
			if (!getLength(definitions[index]).IsStar()) continue;
			stars.push_back(index);
			minimumTotal += getMinimum(definitions[index]);
			maximumTotal += getMaximum(definitions[index]);
		}
		if (stars.empty()) return;

		auto assignMinimums = [&]()
		{
			for (size_t index : stars)
				sizes[index] = getMinimum(definitions[index]);
		};
		if (target <= minimumTotal)
		{
			assignMinimums();
			return;
		}
		if (target >= maximumTotal)
		{
			for (size_t index : stars)
				sizes[index] = getMaximum(definitions[index]);
			return;
		}

		auto allocatedAt = [&](double unit)
		{
			double total = 0.0;
			for (size_t index : stars)
			{
				const double weighted = unit
					* static_cast<double>(NormalizeTrackValue(getLength(definitions[index]).Value));
				total += (std::clamp)(
					weighted,
					static_cast<double>(getMinimum(definitions[index])),
					static_cast<double>(getMaximum(definitions[index])));
			}
			return total;
		};

		double low = 0.0;
		double high = 1.0;
		while (allocatedAt(high) < target && high < 1.0e38)
			high *= 2.0;
		for (int iteration = 0; iteration < 64; ++iteration)
		{
			const double middle = (low + high) * 0.5;
			if (allocatedAt(middle) < target)
				low = middle;
			else
				high = middle;
		}

		for (size_t index : stars)
		{
			const double weighted = high
				* static_cast<double>(NormalizeTrackValue(getLength(definitions[index]).Value));
			sizes[index] = static_cast<float>((std::clamp)(
				weighted,
				static_cast<double>(getMinimum(definitions[index])),
				static_cast<double>(getMaximum(definitions[index]))));
		}
	}

	void SortContentRequests(std::vector<ContentRequest>& requests)
	{
		std::stable_sort(requests.begin(), requests.end(),
			[](const ContentRequest& left, const ContentRequest& right)
			{
				return left.Span < right.Span;
			});
	}

	template<typename TDefinition, typename TLength>
	float ChildMeasureMaximum(
		const std::vector<TDefinition>& definitions,
		const std::vector<float>& allocated,
		int start,
		int span,
		bool bounded,
		const Thickness& margin,
		bool horizontal,
		TLength getLength)
	{
		const int end = (std::min)(
			start + span, static_cast<int>(definitions.size()));
		for (int index = start; index < end; ++index)
		{
			const auto& length = getLength(definitions[static_cast<size_t>(index)]);
			// Auto receives an unbounded Measure offer in WPF. Star has the same
			// content-sized behavior when the Grid itself is unbounded on this axis.
			if (length.IsAuto() || (!bounded && length.IsStar()))
				return cui::core::Infinity;
		}
		const float outer = SumSpan(allocated, start, span);
		const float margins = horizontal
			? margin.Left + margin.Right : margin.Top + margin.Bottom;
		return (std::max)(0.0f, outer - margins);
	}

	template<typename TDefinition, typename TLength, typename TMinimum,
		typename TMaximum>
	std::vector<float> CreateDesiredTrackSizes(
		const std::vector<TDefinition>& definitions,
		TLength getLength,
		TMinimum getMinimum,
		TMaximum getMaximum)
	{
		std::vector<float> result(definitions.size(), 0.0f);
		for (size_t index = 0; index < definitions.size(); ++index)
		{
			const auto& definition = definitions[index];
			const float minimum = NormalizeMinimum(getMinimum(definition));
			const float maximum = NormalizeMaximum(
				getMaximum(definition), minimum);
			if (getLength(definition).IsPixel())
			{
				result[index] = (std::clamp)(
					NormalizeTrackValue(getLength(definition).Value),
					minimum, maximum);
			}
			else
			{
				// A bounded Star track is allocated remaining layout space for child
				// measurement and Arrange, but that allocation is not Grid.DesiredSize.
				// Its desired contribution begins at Min just like WPF's measure cache.
				result[index] = minimum;
			}
		}
		return result;
	}

	template<typename TDefinition, typename TLength, typename TMaximum>
	void GrowDesiredTracks(
		std::vector<float>& desired,
		const std::vector<float>& allocated,
		const std::vector<TDefinition>& definitions,
		const ContentRequest& request,
		bool bounded,
		TLength getLength,
		TMaximum getMaximum)
	{
		float deficit = request.Required
			- SumSpan(desired, request.Start, request.Span);
		if (!(deficit > TrackEpsilon)) return;

		std::vector<int> active;
		const int end = (std::min)(request.Start + request.Span,
			static_cast<int>(definitions.size()));
		for (int index = request.Start; index < end; ++index)
		{
			const size_t track = static_cast<size_t>(index);
			const auto& length = getLength(definitions[track]);
			if (length.IsPixel()) continue;
			float capacity = getMaximum(definitions[track]);
			if (bounded && length.IsStar() && track < allocated.size())
				capacity = (std::min)(capacity,
					(std::max)(desired[track], allocated[track]));
			if (capacity - desired[track] > TrackEpsilon)
				active.push_back(index);
		}

		while (deficit > TrackEpsilon && !active.empty())
		{
			const float share = deficit / static_cast<float>(active.size());
			float distributed = 0.0f;
			std::vector<int> next;
			for (int index : active)
			{
				const size_t track = static_cast<size_t>(index);
				const auto& length = getLength(definitions[track]);
				float capacity = getMaximum(definitions[track]);
				if (bounded && length.IsStar() && track < allocated.size())
					capacity = (std::min)(capacity,
						(std::max)(desired[track], allocated[track]));
				const float amount = (std::min)(
					share, (std::max)(0.0f, capacity - desired[track]));
				desired[track] += amount;
				distributed += amount;
				if (capacity - desired[track] > TrackEpsilon)
					next.push_back(index);
			}
			if (!(distributed > TrackEpsilon)) break;
			deficit -= distributed;
			active = std::move(next);
		}
	}
}

void GridLayoutEngine::CalculateColumnWidths(LayoutContext& context, float availableWidth)
{
	if (_columnDefinitions.empty())
	{
		_columnDefinitions.push_back(ColumnDefinition(GridLength::Star(1.0f)));
	}
	
	const size_t columnCount = _columnDefinitions.size();
	_columnWidths.assign(columnCount, 0.0f);
	_columnPositions.resize(columnCount + 1, 0.0f);
	const bool widthIsBounded = std::isfinite(availableWidth);
	if (!widthIsBounded) availableWidth = 0.0f;

	for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
	{
		auto& columnDefinition = _columnDefinitions[columnIndex];
		columnDefinition.MinWidth = NormalizeMinimum(columnDefinition.MinWidth);
		columnDefinition.MaxWidth = NormalizeMaximum(columnDefinition.MaxWidth, columnDefinition.MinWidth);

		if (columnDefinition.Width.IsPixel())
		{
			_columnWidths[columnIndex] = (std::clamp)(
				NormalizeTrackValue(columnDefinition.Width.Value),
				columnDefinition.MinWidth,
				columnDefinition.MaxWidth);
		}
		else if (IsContentSized(columnDefinition.Width, widthIsBounded))
			_columnWidths[columnIndex] = columnDefinition.MinWidth;
	}

	std::vector<ContentRequest> requests;
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
		const int start = ClampTrackStart(
			Grid::GetColumn(*child), static_cast<int>(columnCount));
		const int span = ClampTrackSpan(
			start, Grid::GetColumnSpan(*child), static_cast<int>(columnCount));
		bool hasContentTrack = false;
		for (int index = start; index < start + span; ++index)
			hasContentTrack = hasContentTrack
				|| IsContentSized(_columnDefinitions[static_cast<size_t>(index)].Width, widthIsBounded);
		if (!hasContentTrack) continue;

		const auto childSize = child->Measure(cui::core::Constraints::Unbounded());
		const Thickness margin = child->Margin;
		requests.push_back(ContentRequest{
			start,
			span,
			(std::max)(0.0f, childSize.width + margin.Left + margin.Right) });
	}
	SortContentRequests(requests);
	std::vector<ContentRequest> starSpanningRequests;
	for (const auto& request : requests)
	{
		bool spansBoundedStar = false;
		for (int index = request.Start; index < request.Start + request.Span; ++index)
			spansBoundedStar = spansBoundedStar
				|| (widthIsBounded && _columnDefinitions[static_cast<size_t>(index)].Width.IsStar());
		if (spansBoundedStar)
		{
			starSpanningRequests.push_back(request);
			continue;
		}
		GrowContentTracks(_columnWidths, _columnDefinitions, request, widthIsBounded,
			[](const ColumnDefinition& definition) -> const GridLength& { return definition.Width; },
			[](const ColumnDefinition& definition) { return definition.MaxWidth; });
	}

	if (widthIsBounded)
	{
		float occupied = 0.0f;
		for (size_t index = 0; index < columnCount; ++index)
			if (!_columnDefinitions[index].Width.IsStar())
				occupied += _columnWidths[index];
		ResolveStarTracks(_columnWidths, _columnDefinitions, availableWidth, occupied,
			[](const ColumnDefinition& definition) -> const GridLength& { return definition.Width; },
			[](const ColumnDefinition& definition) { return definition.MinWidth; },
			[](const ColumnDefinition& definition) { return definition.MaxWidth; });
	}
	for (const auto& request : starSpanningRequests)
	{
		GrowContentTracks(_columnWidths, _columnDefinitions, request, widthIsBounded,
			[](const ColumnDefinition& definition) -> const GridLength& { return definition.Width; },
			[](const ColumnDefinition& definition) { return definition.MaxWidth; });
	}

	// 计算列起始位置
	_columnPositions[0] = 0.0f;
	for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
	{
		_columnPositions[columnIndex + 1] = _columnPositions[columnIndex] + _columnWidths[columnIndex];
	}
}

void GridLayoutEngine::CalculateRowHeights(LayoutContext& context, float availableHeight)
{
	if (_rowDefinitions.empty())
	{
		_rowDefinitions.push_back(RowDefinition(GridLength::Star(1.0f)));
	}
	
	const size_t rowCount = _rowDefinitions.size();
	_rowHeights.assign(rowCount, 0.0f);
	_rowPositions.resize(rowCount + 1, 0.0f);
	const bool heightIsBounded = std::isfinite(availableHeight);
	if (!heightIsBounded) availableHeight = 0.0f;

	for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		auto& rowDefinition = _rowDefinitions[rowIndex];
		rowDefinition.MinHeight = NormalizeMinimum(rowDefinition.MinHeight);
		rowDefinition.MaxHeight = NormalizeMaximum(rowDefinition.MaxHeight, rowDefinition.MinHeight);

		if (rowDefinition.Height.IsPixel())
		{
			_rowHeights[rowIndex] = (std::clamp)(
				NormalizeTrackValue(rowDefinition.Height.Value),
				rowDefinition.MinHeight,
				rowDefinition.MaxHeight);
		}
		else if (IsContentSized(rowDefinition.Height, heightIsBounded))
			_rowHeights[rowIndex] = rowDefinition.MinHeight;
	}

	std::vector<ContentRequest> requests;
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
		const int start = ClampTrackStart(
			Grid::GetRow(*child), static_cast<int>(rowCount));
		const int span = ClampTrackSpan(
			start, Grid::GetRowSpan(*child), static_cast<int>(rowCount));
		bool hasContentTrack = false;
		for (int index = start; index < start + span; ++index)
			hasContentTrack = hasContentTrack
				|| IsContentSized(_rowDefinitions[static_cast<size_t>(index)].Height, heightIsBounded);
		if (!hasContentTrack) continue;

		const int columnCount = static_cast<int>(_columnWidths.size());
		const int column = ClampTrackStart(
			Grid::GetColumn(*child), columnCount);
		const int columnSpan = ClampTrackSpan(
			column, Grid::GetColumnSpan(*child), columnCount);
		const Thickness margin = child->Margin;
		const float childAvailableWidth = (std::max)(
			0.0f, SumSpan(_columnWidths, column, columnSpan) - margin.Left - margin.Right);
		const auto childSize = child->Measure(cui::core::Constraints{
			cui::core::Size{ childAvailableWidth, cui::core::Infinity } });
		requests.push_back(ContentRequest{
			start,
			span,
			(std::max)(0.0f, childSize.height + margin.Top + margin.Bottom) });
	}
	SortContentRequests(requests);
	std::vector<ContentRequest> starSpanningRequests;
	for (const auto& request : requests)
	{
		bool spansBoundedStar = false;
		for (int index = request.Start; index < request.Start + request.Span; ++index)
			spansBoundedStar = spansBoundedStar
				|| (heightIsBounded && _rowDefinitions[static_cast<size_t>(index)].Height.IsStar());
		if (spansBoundedStar)
		{
			starSpanningRequests.push_back(request);
			continue;
		}
		GrowContentTracks(_rowHeights, _rowDefinitions, request, heightIsBounded,
			[](const RowDefinition& definition) -> const GridLength& { return definition.Height; },
			[](const RowDefinition& definition) { return definition.MaxHeight; });
	}

	if (heightIsBounded)
	{
		float occupied = 0.0f;
		for (size_t index = 0; index < rowCount; ++index)
			if (!_rowDefinitions[index].Height.IsStar())
				occupied += _rowHeights[index];
		ResolveStarTracks(_rowHeights, _rowDefinitions, availableHeight, occupied,
			[](const RowDefinition& definition) -> const GridLength& { return definition.Height; },
			[](const RowDefinition& definition) { return definition.MinHeight; },
			[](const RowDefinition& definition) { return definition.MaxHeight; });
	}
	for (const auto& request : starSpanningRequests)
	{
		GrowContentTracks(_rowHeights, _rowDefinitions, request, heightIsBounded,
			[](const RowDefinition& definition) -> const GridLength& { return definition.Height; },
			[](const RowDefinition& definition) { return definition.MaxHeight; });
	}

	// 计算行起始位置
	_rowPositions[0] = 0.0f;
	for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		_rowPositions[rowIndex + 1] = _rowPositions[rowIndex] + _rowHeights[rowIndex];
	}
}

cui::core::Size GridLayoutEngine::Measure(LayoutContext& context, const cui::core::Constraints& available)
{
	const auto maximum = available.Normalized().maximum;
	CalculateColumnWidths(context, maximum.width);
	CalculateRowHeights(context, maximum.height);

	const bool widthIsBounded = std::isfinite(maximum.width);
	const bool heightIsBounded = std::isfinite(maximum.height);
	auto desiredColumns = CreateDesiredTrackSizes(
		_columnDefinitions,
		[](const ColumnDefinition& definition) -> const GridLength&
		{ return definition.Width; },
		[](const ColumnDefinition& definition) { return definition.MinWidth; },
		[](const ColumnDefinition& definition) { return definition.MaxWidth; });
	auto desiredRows = CreateDesiredTrackSizes(
		_rowDefinitions,
		[](const RowDefinition& definition) -> const GridLength&
		{ return definition.Height; },
		[](const RowDefinition& definition) { return definition.MinHeight; },
		[](const RowDefinition& definition) { return definition.MaxHeight; });

	std::vector<ContentRequest> columnRequests;
	std::vector<ContentRequest> rowRequests;
	columnRequests.reserve(static_cast<size_t>(context.ChildCount()));
	rowRequests.reserve(static_cast<size_t>(context.ChildCount()));
	for (int childIndex = 0; childIndex < context.ChildCount(); ++childIndex)
	{
		auto* child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
		const int column = ClampTrackStart(
			Grid::GetColumn(*child), static_cast<int>(_columnDefinitions.size()));
		const int columnSpan = ClampTrackSpan(
			column, Grid::GetColumnSpan(*child),
			static_cast<int>(_columnDefinitions.size()));
		const int row = ClampTrackStart(
			Grid::GetRow(*child), static_cast<int>(_rowDefinitions.size()));
		const int rowSpan = ClampTrackSpan(
			row, Grid::GetRowSpan(*child),
			static_cast<int>(_rowDefinitions.size()));
		const Thickness margin = child->Margin;
		const float childMaximumWidth = ChildMeasureMaximum(
			_columnDefinitions, _columnWidths, column, columnSpan,
			widthIsBounded, margin, true,
			[](const ColumnDefinition& definition) -> const GridLength&
			{ return definition.Width; });
		const float childMaximumHeight = ChildMeasureMaximum(
			_rowDefinitions, _rowHeights, row, rowSpan,
			heightIsBounded, margin, false,
			[](const RowDefinition& definition) -> const GridLength&
			{ return definition.Height; });
		const auto childSize = child->Measure(cui::core::Constraints{
			cui::core::Size{ childMaximumWidth, childMaximumHeight } });
		columnRequests.push_back({ column, columnSpan,
			(std::max)(0.0f,
				childSize.width + margin.Left + margin.Right) });
		rowRequests.push_back({ row, rowSpan,
			(std::max)(0.0f,
				childSize.height + margin.Top + margin.Bottom) });
	}
	SortContentRequests(columnRequests);
	SortContentRequests(rowRequests);
	for (const auto& request : columnRequests)
		GrowDesiredTracks(
			desiredColumns, _columnWidths, _columnDefinitions,
			request, widthIsBounded,
			[](const ColumnDefinition& definition) -> const GridLength&
			{ return definition.Width; },
			[](const ColumnDefinition& definition) { return definition.MaxWidth; });
	for (const auto& request : rowRequests)
		GrowDesiredTracks(
			desiredRows, _rowHeights, _rowDefinitions,
			request, heightIsBounded,
			[](const RowDefinition& definition) -> const GridLength&
			{ return definition.Height; },
			[](const RowDefinition& definition) { return definition.MaxHeight; });

	float totalWidth = 0.0f;
	for (float columnWidth : desiredColumns) totalWidth += columnWidth;
	float totalHeight = 0.0f;
	for (float rowHeight : desiredRows) totalHeight += rowHeight;
	
	_needsLayout = false;
	return { totalWidth, totalHeight };
}

bool GridLayoutEngine::TryGetCellAtPoint(Control* container, float localX, float localY, int& outRow, int& outCol)
{
	outRow = 0;
	outCol = 0;
	if (!container) return false;

	// WPF Panel has no Padding: cell coordinates and arranged children share
	// the panel's local origin.
	const auto containerSize = container->GetActualSizeDip();
	float contentWidth = containerSize.width;
	float contentHeight = containerSize.height;
	if (contentWidth < 0.0f) contentWidth = 0.0f;
	if (contentHeight < 0.0f) contentHeight = 0.0f;
	LayoutContext context(container);
	CalculateColumnWidths(context, contentWidth);
	CalculateRowHeights(context, contentHeight);

	if (_columnPositions.size() < 2 || _rowPositions.size() < 2) return false;

	// Clamp 到有效区域
	if (localX < 0.0f) localX = 0.0f;
	if (localY < 0.0f) localY = 0.0f;
	float maxLocalX = _columnPositions.back();
	float maxLocalY = _rowPositions.back();
	if (localX > maxLocalX) localX = maxLocalX;
	if (localY > maxLocalY) localY = maxLocalY;

	// 找列
	outCol = (int)_columnWidths.size() - 1;
	for (size_t columnIndex = 0; columnIndex + 1 < _columnPositions.size(); columnIndex++)
	{
		if (localX >= _columnPositions[columnIndex]
			&& localX < _columnPositions[columnIndex + 1])
		{
			outCol = (int)columnIndex;
			break;
		}
	}

	// 找行
	outRow = (int)_rowHeights.size() - 1;
	for (size_t rowIndex = 0; rowIndex + 1 < _rowPositions.size(); rowIndex++)
	{
		if (localY >= _rowPositions[rowIndex]
			&& localY < _rowPositions[rowIndex + 1])
		{
			outRow = (int)rowIndex;
			break;
		}
	}

	if (outCol < 0) outCol = 0;
	if (outRow < 0) outRow = 0;
	return true;
}

void GridLayoutEngine::Arrange(LayoutContext& context, cui::core::Rect finalRect)
{
	finalRect = finalRect.Normalized();
	const float containerWidth = finalRect.width;
	const float containerHeight = finalRect.height;
	
	// 重新计算（如果容器尺寸与测量时不同）
	CalculateColumnWidths(context, containerWidth);
	CalculateRowHeights(context, containerHeight);
	
	// 排列子控件
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto child = context.ChildAt(childIndex);
		if (!child || child->IsCollapsed()) continue;
		
		const int row = ClampTrackStart(
			Grid::GetRow(*child), static_cast<int>(_rowDefinitions.size()));
		const int column = ClampTrackStart(
			Grid::GetColumn(*child), static_cast<int>(_columnDefinitions.size()));
		const int rowSpan = ClampTrackSpan(
			row, Grid::GetRowSpan(*child),
			static_cast<int>(_rowDefinitions.size()));
		const int columnSpan = ClampTrackSpan(
			column, Grid::GetColumnSpan(*child),
			static_cast<int>(_columnDefinitions.size()));
		
		// 计算单元格区域
		float cellX = finalRect.x + _columnPositions[column];
		float cellY = finalRect.y + _rowPositions[row];
		float cellWidth = 0.0f;
		float cellHeight = 0.0f;
		
		// 计算跨列宽度
		for (int columnIndex = column; columnIndex < column + columnSpan && columnIndex < (int)_columnWidths.size(); columnIndex++)
		{
			cellWidth += _columnWidths[columnIndex];
		}
		
		// 计算跨行高度
		for (int rowIndex = row; rowIndex < row + rowSpan && rowIndex < (int)_rowHeights.size(); rowIndex++)
		{
			cellHeight += _rowHeights[rowIndex];
		}
		
		// 应用边距
		Thickness margin = child->Margin;
		float contentX = cellX + margin.Left;
		float contentY = cellY + margin.Top;
		float contentWidth = cellWidth - margin.Left - margin.Right;
		float contentHeight = cellHeight - margin.Top - margin.Bottom;
		
		if (contentWidth < 0) contentWidth = 0;
		if (contentHeight < 0) contentHeight = 0;
		
		// 应用对齐
		const auto horizontalAlignment =
			cui::layout::ResolveHorizontalArrangeAlignment(*child);
		const auto verticalAlignment =
			cui::layout::ResolveVerticalArrangeAlignment(*child);
		const auto childSize = child->Measure(cui::core::Constraints{ cui::core::Size{
			contentWidth, contentHeight } });
		float finalWidth = childSize.width;
		float finalHeight = childSize.height;
		
		// 水平对齐
		if (horizontalAlignment == HorizontalAlignment::Stretch)
		{
			contentX = cellX + margin.Left;
			finalWidth = contentWidth;
		}
		else
		{
			if (horizontalAlignment == HorizontalAlignment::Center)
			{
				contentX = cellX + margin.Left + (contentWidth - finalWidth) / 2.0f;
			}
			else if (horizontalAlignment == HorizontalAlignment::Right)
			{
				contentX = cellX + cellWidth - margin.Right - finalWidth;
			}
		}
		
		// 垂直对齐
		if (verticalAlignment == VerticalAlignment::Stretch)
		{
			contentY = cellY + margin.Top;
			finalHeight = contentHeight;
		}
		else
		{
			if (verticalAlignment == VerticalAlignment::Center)
			{
				contentY = cellY + margin.Top + (contentHeight - finalHeight) / 2.0f;
			}
			else if (verticalAlignment == VerticalAlignment::Bottom)
			{
				contentY = cellY + cellHeight - margin.Bottom - finalHeight;
			}
		}
		
		child->Arrange(cui::core::Rect{
			contentX, contentY, finalWidth, finalHeight });
	}
	
	_needsLayout = false;
}

// Grid 实现

Grid::Grid()
{
	_gridEngine = new GridLayoutEngine();
	SetLayoutEngine(_gridEngine);
}

Grid::~Grid()
{
	// _gridEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
