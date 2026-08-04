#include "CollectionViewSource.h"
#include "EventInfrastructure.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <functional>
#include <iterator>
#include <unordered_set>

namespace
{
	std::wstring Fold(std::wstring value, bool ignoreCase)
	{
		if (ignoreCase)
			std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	int CompareValues(
		const BindingValue& left,
		const BindingValue& right,
		bool ignoreCase)
	{
		if (left.Empty() || right.Empty())
			return left.Empty() == right.Empty() ? 0 : left.Empty() ? -1 : 1;
		const auto numeric = [](BindingValueKind kind)
		{
			return kind == BindingValueKind::Bool
				|| kind == BindingValueKind::NullableBool
				|| kind == BindingValueKind::Int
				|| kind == BindingValueKind::Int64
				|| kind == BindingValueKind::Float
				|| kind == BindingValueKind::Double;
		};
		if (numeric(left.Kind()) && numeric(right.Kind()))
		{
			double l = 0.0;
			double r = 0.0;
			if (left.TryGetDouble(l) && right.TryGetDouble(r))
				return l < r ? -1 : l > r ? 1 : 0;
		}
		std::wstring l;
		std::wstring r;
		if (left.TryGetString(l) && right.TryGetString(r))
		{
			l = Fold(std::move(l), ignoreCase);
			r = Fold(std::move(r), ignoreCase);
			return l < r ? -1 : l > r ? 1 : 0;
		}
		if (left.Kind() != right.Kind())
			return static_cast<int>(left.Kind())
				< static_cast<int>(right.Kind()) ? -1 : 1;
		// Object values have no intrinsic ordering. Keep their source order.
		return 0;
	}

	bool SameItem(
		const BindingSourceReference& left,
		const BindingSourceReference& right) noexcept
	{
		return left.Shared() == right.Shared();
	}

	template<typename TDescription>
	bool TryGetDescriptionValue(
		IBindingSource& source,
		const TDescription& description,
		BindingValue& out)
	{
		if (!description.CompiledPath.Empty())
			return TryGetBindingPathValue(
				source, description.CompiledPath, out);
#if CUI_ENABLE_DYNAMIC_XAML
		return cui::design::TryReadAuthoredCollectionDescription(
			source, description, out);
#else
		return false;
#endif
	}

	template<typename TDescription>
	bool HasDescriptionPath(const TDescription& description) noexcept
	{
		if (!description.CompiledPath.Empty()) return true;
#if CUI_ENABLE_DYNAMIC_XAML
		return cui::design::HasAuthoredCollectionDescriptionPath(description);
#else
		return false;
#endif
	}

	std::wstring CollectionGroupPropertyName(
		const CollectionGroupDescription& description)
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return cui::design::AuthoredCollectionGroupPropertyName(description);
#else
		(void)description;
		return {};
#endif
	}

	bool SameGroups(
		const std::vector<BindingListGroup>& left,
		const std::vector<BindingListGroup>& right)
	{
		if (left.size() != right.size()) return false;
		for (size_t index = 0; index < left.size(); ++index)
		{
			const auto& l = left[index];
			const auto& r = right[index];
			if (l.PropertyName != r.PropertyName || l.Level != r.Level
				|| l.StartIndex != r.StartIndex || l.ItemCount != r.ItemCount
				|| !BindingValuesEqual(l.Key, r.Key)
				|| l.Aggregates.size() != r.Aggregates.size()) return false;
			for (const auto& [name, value] : l.Aggregates)
			{
				const auto found = r.Aggregates.find(name);
				if (found == r.Aggregates.end()
					|| !BindingValuesEqual(value, found->second)) return false;
			}
		}
		return true;
	}
}

bool CollectionViewSource::TryGetItem(
	size_t index,
	BindingSourceReference& out) const
{
	if (index >= _items.size()) return false;
	out = _items[index];
	return static_cast<bool>(out);
}

EventConnection CollectionViewSource::SubscribeChanged(ChangedHandler handler)
{
	if (!handler) return {};
	return _changed.Subscribe(
		[handler = std::move(handler)](
			CollectionViewSource*, const CollectionChangedEventArgs& change)
		{ handler(change); });
}

EventConnection CollectionViewSource::SubscribeCurrentChanged(
	CurrentChangedHandler handler)
{
	if (!handler) return {};
	return CurrentChanged.Subscribe(
		[handler = std::move(handler)](CollectionViewSource*) { handler(); });
}

DataTypeToken CollectionViewSource::GetItemTypeToken() const noexcept
{
	return _source ? _source.Get()->GetItemTypeToken() : DataTypeToken{};
}

void CollectionViewSource::SetSource(BindingListReference value)
{
#if CUI_ENABLE_DYNAMIC_XAML
	ClearAuthoredSourceBindingPath();
#endif
	_sourceCompiledBindingPath = {};
	_dataContext = {};
	_sourceBindingObservation = {};
	SetResolvedSource(std::move(value));
}

void CollectionViewSource::SetResolvedSource(BindingListReference value)
{
	if (_source == value) return;
	_sourceChanged.Disconnect();
	_source = std::move(value);
	if (_source)
		_sourceChanged = _source.Get()->SubscribeChanged(
			[this](const CollectionChangedEventArgs&) { Refresh(); });
	Refresh();
}

void CollectionViewSource::SetCompiledSourceBindingPath(
	CompiledBindingPathView value)
{
	if (SameCompiledCollectionPath(_sourceCompiledBindingPath, value)
#if CUI_ENABLE_DYNAMIC_XAML
		&& !HasAuthoredSourceBindingPath()
#endif
		) return;
	_sourceCompiledBindingPath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	ClearAuthoredSourceBindingPath();
#endif
	ResolveBoundSource();
}

void CollectionViewSource::BindDataContext(BindingSourceReference value)
{
	const bool hasPath = !_sourceCompiledBindingPath.Empty()
#if CUI_ENABLE_DYNAMIC_XAML
		|| HasAuthoredSourceBindingPath()
#endif
		;
	if (!hasPath) return;
	if (_dataContext.Shared() == value.Shared()) return;
	_dataContext = std::move(value);
	ResolveBoundSource();
}

void CollectionViewSource::ResolveBoundSource()
{
	_sourceBindingObservation = {};
	BindingListReference resolved;
	if (_dataContext && !_sourceCompiledBindingPath.Empty())
	{
		BindingValue value;
		(void)(TryGetBindingPathValue(
			*_dataContext.Get(), _sourceCompiledBindingPath, value)
			&& value.TryGet(resolved));
		_sourceBindingObservation = ObserveBindingPaths(
			_dataContext, { _sourceCompiledBindingPath },
			[this] { ResolveBoundSource(); });
	}
#if CUI_ENABLE_DYNAMIC_XAML
	else if (_dataContext && HasAuthoredSourceBindingPath())
		ResolveAuthoredSourceBinding(resolved);
#endif
	SetResolvedSource(std::move(resolved));
}

void CollectionViewSource::SetSortDescriptions(
	std::vector<CollectionSortDescription> value)
{
	if (_sortDescriptions == value) return;
	_sortDescriptions = std::move(value);
	Refresh();
}

void CollectionViewSource::SetFilterDescriptions(
	std::vector<CollectionFilterDescription> value)
{
	_filterDescriptions = std::move(value);
	Refresh();
}

void CollectionViewSource::SetGroupDescriptions(
	std::vector<CollectionGroupDescription> value)
{
	if (_groupDescriptions == value) return;
	_groupDescriptions = std::move(value);
	Refresh();
}

void CollectionViewSource::SetAggregateDescriptions(
	std::vector<CollectionAggregateDescription> value)
{
	if (_aggregateDescriptions == value) return;
	_aggregateDescriptions = std::move(value);
	Refresh();
}

EventConnection CollectionViewSource::SubscribeGroupsChanged(
	GroupsChangedHandler handler)
{
	if (!handler) return {};
	return _groupsChanged.Subscribe(
		[handler = std::move(handler)](CollectionViewSource*) { handler(); });
}

void CollectionViewSource::SetFilterPredicate(FilterPredicate value)
{
	_filterPredicate = std::move(value);
	Refresh();
}

bool CollectionViewSource::PassesFilters(
	const BindingSourceReference& item) const
{
	if (!item || (_filterPredicate && !_filterPredicate(item))) return false;
	for (const auto& filter : _filterDescriptions)
	{
		BindingValue candidate;
		const bool found = HasDescriptionPath(filter)
			&& TryGetDescriptionValue(*item.Get(), filter, candidate);
		if (filter.Operator == CollectionFilterOperator::IsEmpty)
		{
			if (found && !candidate.Empty()) return false;
			continue;
		}
		if (filter.Operator == CollectionFilterOperator::IsNotEmpty)
		{
			if (!found || candidate.Empty()) return false;
			continue;
		}
		if (!found) return false;
		const int comparison = CompareValues(
			candidate, filter.Value, filter.IgnoreCase);
		switch (filter.Operator)
		{
		case CollectionFilterOperator::Equals:
			if (comparison != 0) return false;
			break;
		case CollectionFilterOperator::NotEquals:
			if (comparison == 0) return false;
			break;
		case CollectionFilterOperator::LessThan:
			if (comparison >= 0) return false;
			break;
		case CollectionFilterOperator::LessThanOrEqual:
			if (comparison > 0) return false;
			break;
		case CollectionFilterOperator::GreaterThan:
			if (comparison <= 0) return false;
			break;
		case CollectionFilterOperator::GreaterThanOrEqual:
			if (comparison < 0) return false;
			break;
		case CollectionFilterOperator::Contains:
		case CollectionFilterOperator::StartsWith:
		case CollectionFilterOperator::EndsWith:
		{
			std::wstring text;
			std::wstring pattern;
			if (!candidate.TryGetString(text)
				|| !filter.Value.TryGetString(pattern)) return false;
			text = Fold(std::move(text), filter.IgnoreCase);
			pattern = Fold(std::move(pattern), filter.IgnoreCase);
			if (filter.Operator == CollectionFilterOperator::Contains
				&& text.find(pattern) == std::wstring::npos) return false;
			if (filter.Operator == CollectionFilterOperator::StartsWith
				&& !text.starts_with(pattern)) return false;
			if (filter.Operator == CollectionFilterOperator::EndsWith
				&& !text.ends_with(pattern)) return false;
			break;
		}
		default:
			break;
		}
	}
	return true;
}

void CollectionViewSource::Refresh()
{
	if (_refreshing)
	{
		_refreshPending = true;
		return;
	}
	_refreshing = true;
	try
	{
		do
		{
			_refreshPending = false;
			RebuildProjection();
		} while (_refreshPending);
	}
	catch (...)
	{
		// Projection and currency notifications are user-extensible. Never let
		// one throwing handler permanently leave Refresh() in its re-entrancy
		// guard; a later explicit or source-driven refresh must be able to
		// converge the partially published projection.
		_refreshing = false;
		_refreshPending = false;
		throw;
	}
	_refreshing = false;
}

void CollectionViewSource::RebuildProjection()
{
	std::vector<BindingSourceReference> target;
	if (_source)
	{
		target.reserve(_source.Get()->Count());
		for (size_t index = 0; index < _source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			if (_source.Get()->TryGetItem(index, item)
				&& PassesFilters(item)) target.push_back(std::move(item));
		}
	}
	if (!_groupDescriptions.empty() || !_sortDescriptions.empty())
		std::stable_sort(target.begin(), target.end(),
			[this](const auto& left, const auto& right)
			{
				for (const auto& group : _groupDescriptions)
				{
					BindingValue l;
					BindingValue r;
					const bool hasLeft = left
						&& TryGetDescriptionValue(*left.Get(), group, l);
					const bool hasRight = right
						&& TryGetDescriptionValue(*right.Get(), group, r);
					const int comparison = !hasLeft || !hasRight
						? hasLeft == hasRight ? 0 : hasLeft ? 1 : -1
						: CompareValues(l, r, group.IgnoreCase);
					if (comparison == 0) continue;
					return group.Direction == CollectionSortDirection::Ascending
						? comparison < 0 : comparison > 0;
				}
				for (const auto& sort : _sortDescriptions)
				{
					BindingValue l;
					BindingValue r;
					const bool hasLeft = left
						&& TryGetDescriptionValue(*left.Get(), sort, l);
					const bool hasRight = right
						&& TryGetDescriptionValue(*right.Get(), sort, r);
					const int comparison = !hasLeft || !hasRight
						? hasLeft == hasRight ? 0 : hasLeft ? 1 : -1
						: CompareValues(l, r, sort.IgnoreCase);
					if (comparison == 0) continue;
					return sort.Direction == CollectionSortDirection::Ascending
						? comparison < 0 : comparison > 0;
				}
				return false;
			});
	const auto groups = BuildGroups(target);
	const bool groupsChanged = !SameGroups(_groups, groups);
	_groups = groups;
	PublishProjection(std::move(target));
	RebuildItemObservations();
	RestoreCurrentItem();
	if (groupsChanged)
		cui::framework::EventAccess::Raise(_groupsChanged, this);
}

std::vector<BindingListGroup> CollectionViewSource::BuildGroups(
	const std::vector<BindingSourceReference>& items) const
{
	std::vector<BindingListGroup> result;
	if (_groupDescriptions.empty() || items.empty()) return result;
	auto buildAggregates = [&](size_t begin, size_t end)
	{
		std::map<std::wstring, BindingValue> values;
		for (const auto& aggregate : _aggregateDescriptions)
		{
			if (aggregate.Function == CollectionAggregateFunction::Count)
			{
				values.emplace(aggregate.Name,
					BindingValue(static_cast<long long>(end - begin)));
				continue;
			}
			BindingValue extremum;
			double sum = 0.0;
			size_t numericCount = 0;
			for (size_t index = begin; index < end; ++index)
			{
				BindingValue candidate;
				if (!items[index] || !TryGetDescriptionValue(
					*items[index].Get(), aggregate, candidate)
					|| candidate.Empty()) continue;
				if (aggregate.Function == CollectionAggregateFunction::Sum
					|| aggregate.Function == CollectionAggregateFunction::Average)
				{
					double numeric = 0.0;
					if (candidate.TryGetDouble(numeric))
					{
						sum += numeric;
						++numericCount;
					}
					continue;
				}
				if (extremum.Empty()
					|| (aggregate.Function == CollectionAggregateFunction::Min
						&& CompareValues(candidate, extremum, false) < 0)
					|| (aggregate.Function == CollectionAggregateFunction::Max
						&& CompareValues(candidate, extremum, false) > 0))
					extremum = candidate;
			}
			if (aggregate.Function == CollectionAggregateFunction::Sum)
				values.emplace(aggregate.Name, BindingValue(sum));
			else if (aggregate.Function == CollectionAggregateFunction::Average)
				values.emplace(aggregate.Name, BindingValue(
					numericCount ? sum / static_cast<double>(numericCount) : 0.0));
			else values.emplace(aggregate.Name, std::move(extremum));
		}
		return values;
	};
	std::function<void(size_t, size_t, size_t)> appendLevel;
	appendLevel = [&](size_t level, size_t begin, size_t end)
	{
		if (level >= _groupDescriptions.size() || begin >= end) return;
		const auto& description = _groupDescriptions[level];
		size_t groupBegin = begin;
		while (groupBegin < end)
		{
			BindingValue key;
			if (items[groupBegin])
				(void)TryGetDescriptionValue(
					*items[groupBegin].Get(), description, key);
			size_t groupEnd = groupBegin + 1;
			for (; groupEnd < end; ++groupEnd)
			{
				BindingValue candidate;
				if (items[groupEnd])
					(void)TryGetDescriptionValue(
						*items[groupEnd].Get(), description, candidate);
				if (CompareValues(key, candidate, description.IgnoreCase) != 0)
					break;
			}
			BindingListGroup group{
				key, CollectionGroupPropertyName(description), level,
				groupBegin, groupEnd - groupBegin };
			group.Aggregates = buildAggregates(groupBegin, groupEnd);
			result.push_back(std::move(group));
			appendLevel(level + 1, groupBegin, groupEnd);
			groupBegin = groupEnd;
		}
	};
	appendLevel(0, 0, items.size());
	return result;
}

void CollectionViewSource::PublishProjection(
	std::vector<BindingSourceReference> target)
{
	for (size_t index = _items.size(); index-- > 0;)
	{
		const bool retained = std::any_of(
			target.begin(), target.end(), [&](const auto& candidate)
			{ return SameItem(_items[index], candidate); });
		if (retained) continue;
		const size_t oldSize = _items.size();
		_items.erase(_items.begin() + index);
		const CollectionChangedEventArgs change{
			CollectionChangeAction::Remove,
			index, CollectionChangedEventArgs::Npos,
			1, 0, oldSize, _items.size() };
		cui::framework::EventAccess::Raise(_changed, this, change);
	}
	for (size_t index = 0; index < target.size(); ++index)
	{
		if (index < _items.size() && SameItem(_items[index], target[index]))
			continue;
		const auto found = std::find_if(
			_items.begin() + (std::min)(index, _items.size()), _items.end(),
			[&](const auto& item) { return SameItem(item, target[index]); });
		if (found != _items.end())
		{
			const size_t oldIndex = static_cast<size_t>(
				std::distance(_items.begin(), found));
			auto item = *found;
			_items.erase(found);
			_items.insert(_items.begin() + index, std::move(item));
			const CollectionChangedEventArgs change{
				CollectionChangeAction::Move,
				oldIndex, index, 1, 1,
				_items.size(), _items.size() };
			cui::framework::EventAccess::Raise(_changed, this, change);
		}
		else
		{
			const size_t oldSize = _items.size();
			_items.insert(_items.begin() + index, target[index]);
			const CollectionChangedEventArgs change{
				CollectionChangeAction::Add,
				CollectionChangedEventArgs::Npos, index,
				0, 1, oldSize, _items.size() };
			cui::framework::EventAccess::Raise(_changed, this, change);
		}
	}
	while (_items.size() > target.size())
	{
		const size_t index = _items.size() - 1;
		const size_t oldSize = _items.size();
		_items.pop_back();
		const CollectionChangedEventArgs change{
			CollectionChangeAction::Remove,
			index, CollectionChangedEventArgs::Npos,
			1, 0, oldSize, _items.size() };
		cui::framework::EventAccess::Raise(_changed, this, change);
	}
}

void CollectionViewSource::RebuildItemObservations()
{
	_itemObservations.clear();
	if (!_source) return;
	std::vector<CompiledBindingPathView> compiledPaths;
	compiledPaths.reserve(_filterDescriptions.size() + _sortDescriptions.size()
		+ _groupDescriptions.size() + _aggregateDescriptions.size());
	auto appendCompiledPath = [&](CompiledBindingPathView path)
	{
		if (path.Empty()) return;
		if (std::none_of(
			compiledPaths.begin(), compiledPaths.end(),
			[&](CompiledBindingPathView current)
			{ return SameCompiledCollectionPath(current, path); }))
			compiledPaths.push_back(path);
	};
	for (const auto& filter : _filterDescriptions)
		appendCompiledPath(filter.CompiledPath);
	for (const auto& sort : _sortDescriptions)
		appendCompiledPath(sort.CompiledPath);
	for (const auto& group : _groupDescriptions)
		appendCompiledPath(group.CompiledPath);
	for (const auto& aggregate : _aggregateDescriptions)
		appendCompiledPath(aggregate.CompiledPath);
	if (!compiledPaths.empty())
	{
		_itemObservations.reserve(_source.Get()->Count());
		for (size_t index = 0; index < _source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			if (!_source.Get()->TryGetItem(index, item) || !item) continue;
			_itemObservations.push_back(ObserveBindingPaths(
				item,
				std::span<const CompiledBindingPathView>{ compiledPaths },
				[this] { Refresh(); }));
		}
	}
#if CUI_ENABLE_DYNAMIC_XAML
	AppendAuthoredItemObservations();
#endif
}

void CollectionViewSource::RestoreCurrentItem()
{
	const auto previousItem = _currentItem;
	const int previousPosition = _currentPosition;
	int next = -1;
	if (_currentItem)
	{
		const auto found = std::find_if(
			_items.begin(), _items.end(), [&](const auto& item)
			{ return SameItem(item, _currentItem); });
		if (found != _items.end())
			next = static_cast<int>(std::distance(_items.begin(), found));
	}
	if (next < 0 && !_items.empty())
		next = (std::clamp)(previousPosition, 0,
			static_cast<int>(_items.size()) - 1);
	_currentPosition = next;
	_currentItem = next >= 0 ? _items[static_cast<size_t>(next)]
		: BindingSourceReference{};
	if (previousPosition != _currentPosition
		|| !SameItem(previousItem, _currentItem))
		cui::framework::EventAccess::Raise(CurrentChanged, this);
}

bool CollectionViewSource::MoveCurrentToPosition(int position)
{
	if (position < -1 || position >= static_cast<int>(_items.size()))
		return false;
	const auto previous = _currentItem;
	const int previousPosition = _currentPosition;
	_currentPosition = position;
	_currentItem = position >= 0
		? _items[static_cast<size_t>(position)] : BindingSourceReference{};
	if (previousPosition != _currentPosition
		|| !SameItem(previous, _currentItem))
		cui::framework::EventAccess::Raise(CurrentChanged, this);
	return true;
}
