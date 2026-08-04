#pragma once

#include "Binding.h"
#include "ObservableCollection.h"
#include "RuntimeTypeMetadata.h"

#include <algorithm>
#include <functional>
#include <cwctype>
#include <initializer_list>
#include <memory>
#include <map>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * Read-only, observable list of binding records.
 *
 * Items are explicit BindingSourceReference values rather than std::any. This
 * keeps item-property discovery and DataTemplate binding on the same metadata
 * contract as an ordinary DataContext.
 */
class IBindingList
{
public:
	using ChangedHandler = std::function<void(const CollectionChangedEventArgs&)>;

	virtual ~IBindingList() = default;
	virtual size_t Count() const noexcept = 0;
	virtual bool TryGetItem(size_t index, BindingSourceReference& out) const = 0;
	virtual EventConnection SubscribeChanged(ChangedHandler handler) = 0;
#if CUI_ENABLE_DYNAMIC_XAML
	virtual DataTypeToken GetItemTypeToken() const noexcept
	{
		return MakeDataTypeToken(ItemTypeName());
	}
	virtual const std::wstring& ItemTypeName() const noexcept = 0;
#else
	virtual DataTypeToken GetItemTypeToken() const noexcept = 0;
#endif
};

/** One flattened level in a hierarchical grouped-list projection. */
struct BindingListGroup final
{
	BindingValue Key;
	std::wstring PropertyName;
	size_t Level = 0;
	size_t StartIndex = 0;
	size_t ItemCount = 0;
	std::map<std::wstring, BindingValue> Aggregates;
};

/** Optional grouping contract exposed by collection views. */
class IBindingListGroupView
{
public:
	using GroupsChangedHandler = std::function<void()>;
	virtual ~IBindingListGroupView() = default;
	virtual const std::vector<BindingListGroup>& Groups() const noexcept = 0;
	virtual EventConnection SubscribeGroupsChanged(
		GroupsChangedHandler handler) = 0;
};

/** Optional current-item contract exposed by collection views. */
class IBindingListCurrentView
{
public:
	using CurrentChangedHandler = std::function<void()>;

	virtual ~IBindingListCurrentView() = default;
	virtual int CurrentPosition() const noexcept = 0;
	virtual BindingSourceReference CurrentItem() const noexcept = 0;
	virtual bool MoveCurrentToPosition(int position) = 0;
	virtual EventConnection SubscribeCurrentChanged(
		CurrentChangedHandler handler) = 0;
};

/** Strong BindingValue payload used by ItemsSource properties. */
class BindingListReference final
{
public:
	BindingListReference() = default;
	explicit BindingListReference(std::shared_ptr<IBindingList> source)
		: _source(std::move(source)) {}

	template<typename T>
		requires std::is_base_of_v<IBindingList, T>
	explicit BindingListReference(std::shared_ptr<T> source)
		: _source(std::move(source)) {}

	IBindingList* Get() const noexcept { return _source.get(); }
	const std::shared_ptr<IBindingList>& Shared() const noexcept { return _source; }
	explicit operator bool() const noexcept { return static_cast<bool>(_source); }

	bool operator==(const BindingListReference& other) const noexcept
	{
		return _source == other._source;
	}

private:
	std::shared_ptr<IBindingList> _source;
};

/** Default application-side observable record list. */
class ObservableBindingList final : public IBindingList
{
public:
	using ItemCollection = ObservableCollection<BindingSourceReference>;

#if CUI_ENABLE_DYNAMIC_XAML
	explicit ObservableBindingList(std::wstring itemTypeName = {})
		: _itemTypeToken(MakeDataTypeToken(itemTypeName)),
		  _itemTypeName(std::move(itemTypeName)) {}
#else
	explicit ObservableBindingList(DataTypeToken itemTypeToken = {}) noexcept
		: _itemTypeToken(itemTypeToken) {}
#endif

	size_t Count() const noexcept override { return Items.size(); }
	bool TryGetItem(size_t index, BindingSourceReference& out) const override
	{
		if (index >= Items.size() || !Items[index]) return false;
		out = Items[index];
		return true;
	}
	EventConnection SubscribeChanged(ChangedHandler handler) override
	{
		if (!handler) return {};
		return Items.Changed.Subscribe(
			[handler = std::move(handler)](
				ItemCollection*, const CollectionChangedEventArgs& change)
			{
				handler(change);
			});
	}
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ItemTypeName() const noexcept override
	{
		return _itemTypeName;
	}
#endif
	DataTypeToken GetItemTypeToken() const noexcept override
	{
		return _itemTypeToken;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	void SetItemTypeName(std::wstring value)
	{
		_itemTypeToken = MakeDataTypeToken(value);
		_itemTypeName = std::move(value);
	}
#endif
	void SetItemTypeToken(DataTypeToken value) noexcept
	{
		_itemTypeToken = value;
	}

	ItemCollection Items;

private:
	DataTypeToken _itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _itemTypeName;
#endif
};

/**
 * Immutable list emitted by the AOT compiler for static DataList resources.
 * It owns only record references plus their stable type token and has no
 * observable-collection machinery. Production carries no diagnostic type
 * name at all.
 */
class CompiledBindingList final : public IBindingList
{
public:
	explicit CompiledBindingList(
		std::vector<BindingSourceReference> items = {},
		DataTypeToken itemTypeToken = {}
#if CUI_ENABLE_DYNAMIC_XAML
		, std::wstring diagnosticItemTypeName = {}
#endif
		) noexcept
		: _items(std::move(items)),
		  _itemTypeToken(itemTypeToken)
#if CUI_ENABLE_DYNAMIC_XAML
		, _diagnosticItemTypeName(std::move(diagnosticItemTypeName))
#endif
	{
	}

	size_t Count() const noexcept override { return _items.size(); }
	bool TryGetItem(size_t index, BindingSourceReference& out) const override
	{
		if (index >= _items.size() || !_items[index]) return false;
		out = _items[index];
		return true;
	}
	EventConnection SubscribeChanged(ChangedHandler) override { return {}; }
	DataTypeToken GetItemTypeToken() const noexcept override
	{
		return _itemTypeToken;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ItemTypeName() const noexcept override
	{
		return _diagnosticItemTypeName;
	}
#endif

private:
	const std::vector<BindingSourceReference> _items;
	const DataTypeToken _itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring _diagnosticItemTypeName;
#endif
};

/** Owns all subscriptions needed to observe member/indexer record paths. */
struct BindingPathObservation final
{
	std::vector<std::shared_ptr<IBindingSource>> Owners;
	std::vector<std::shared_ptr<IBindingList>> ListOwners;
	std::vector<EventConnection> Connections;
};

#if CUI_ENABLE_DYNAMIC_XAML
inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::span<const std::wstring> paths,
	std::function<void()> changed)
{
	BindingPathObservation result;
	if (!source || !changed) return result;
	result.Owners.push_back(source.Shared());
	struct PathWatch final
	{
		IBindingSource* Source = nullptr;
		std::unordered_set<std::wstring> Properties;
		bool AllProperties = false;
	};
	std::vector<PathWatch> watches;
	std::vector<IBindingList*> listWatches;
	auto watch = [&](IBindingSource& current, std::wstring property)
	{
		auto found = std::find_if(watches.begin(), watches.end(),
			[&](const auto& candidate) { return candidate.Source == &current; });
		if (found == watches.end())
		{
			watches.push_back(PathWatch{ &current });
			found = std::prev(watches.end());
		}
		if (property.empty()) found->AllProperties = true;
		else found->Properties.insert(std::move(property));
	};
	for (const auto& rawPath : paths)
	{
		if (rawPath.empty())
		{
			watch(*source.Get(), {});
			continue;
		}
		std::vector<BindingPathStep> steps;
		if (!TryParseBindingPropertyPath(rawPath, steps)) continue;
		IBindingSource* currentSource = source.Get();
		IBindingList* currentList = nullptr;
		for (size_t index = 0; index < steps.size(); ++index)
		{
			const auto& step = steps[index];
			if (currentSource) watch(*currentSource, step.Value);
			else if (currentList
				&& std::find(listWatches.begin(), listWatches.end(), currentList)
					== listWatches.end())
				listWatches.push_back(currentList);
			if (index + 1 == steps.size()) break;

			BindingValue value;
			if (currentSource)
			{
				if (!currentSource->TryGetValue(step.Value, value)) break;
			}
			else
			{
				if (!currentList || step.Kind != BindingPathStepKind::Indexer) break;
				size_t itemIndex = 0;
				if (step.Value.empty()
					|| !std::all_of(step.Value.begin(), step.Value.end(),
						[](wchar_t ch) { return std::iswdigit(ch) != 0; })) break;
				try
				{
					itemIndex = static_cast<size_t>(std::stoull(step.Value));
				}
				catch (...)
				{
					break;
				}
				BindingSourceReference item;
				if (!currentList->TryGetItem(itemIndex, item) || !item) break;
				value = BindingValue(item);
			}

			BindingSourceReference nested;
			if (value.TryGet(nested) && nested)
			{
				result.Owners.push_back(nested.Shared());
				currentSource = nested.Get();
				currentList = nullptr;
				continue;
			}
			BindingListReference nestedList;
			if (value.TryGet(nestedList) && nestedList)
			{
				result.ListOwners.push_back(nestedList.Shared());
				currentSource = nullptr;
				currentList = nestedList.Get();
				continue;
			}
			break;
		}
	}
	for (auto& pathWatch : watches)
	{
		const bool allProperties = pathWatch.AllProperties;
		auto properties = std::move(pathWatch.Properties);
		result.Connections.push_back(pathWatch.Source->PropertyChanged().Subscribe(
			[changed, allProperties, properties = std::move(properties)](
				const PropertyChangedEventArgs& args)
			{
				if (allProperties || args.PropertyName.empty()
					|| properties.contains(args.PropertyName)) changed();
			}));
	}
	for (auto* list : listWatches)
		result.Connections.push_back(list->SubscribeChanged(
			[changed](const CollectionChangedEventArgs&) { changed(); }));
	return result;
}

inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::initializer_list<std::wstring> paths,
	std::function<void()> changed)
{
	return ObserveBindingPaths(
		source,
		std::span<const std::wstring>{ paths.begin(), paths.size() },
		std::move(changed));
}
#endif

/**
 * Observes immutable AOT binding paths without parsing or retaining member
 * names.  An empty path keeps the legacy "whole record" meaning; token-less
 * notifications are likewise treated as an all-properties invalidation.
 */
inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::span<const CompiledBindingPathView> paths,
	std::function<void()> changed)
{
	BindingPathObservation result;
	if (!source || !changed) return result;
	result.Owners.push_back(source.Shared());
	struct PathWatch final
	{
		IBindingSource* Source = nullptr;
		std::unordered_set<std::uint64_t> Properties;
		bool AllProperties = false;
	};
	std::vector<PathWatch> watches;
	std::vector<IBindingList*> listWatches;
	auto watch = [&](IBindingSource& current,
		BindingSourcePropertyToken property)
	{
		auto found = std::find_if(watches.begin(), watches.end(),
			[&](const auto& candidate) { return candidate.Source == &current; });
		if (found == watches.end())
		{
			watches.push_back(PathWatch{ &current });
			found = std::prev(watches.end());
		}
		if (!property) found->AllProperties = true;
		else found->Properties.insert(property.Value);
	};
	auto watchList = [&](IBindingList& list)
	{
		if (std::find(listWatches.begin(), listWatches.end(), &list)
			== listWatches.end())
			listWatches.push_back(&list);
	};

	for (const auto path : paths)
	{
		if (path.Version != CompiledBindingPathVersion) continue;
		if (path.Empty())
		{
			watch(*source.Get(), {});
			continue;
		}
		IBindingSource* currentSource = source.Get();
		IBindingList* currentList = nullptr;
		for (size_t index = 0; index < path.Steps.size(); ++index)
		{
			const auto& step = path.Steps[index];
			if (currentSource)
			{
				if (step.Kind != CompiledBindingPathStepKind::Property
					|| !step.Property) break;
				if (HasCompiledBindingPathCapability(step.Capabilities,
					CompiledBindingPathCapabilities::Observe))
					watch(*currentSource, step.Property);
			}
			else if (currentList)
			{
				if (step.Kind != CompiledBindingPathStepKind::ListIndex) break;
				watchList(*currentList);
			}
			else
			{
				break;
			}
			if (index + 1 == path.Steps.size()) break;

			BindingValue value;
			if (currentSource)
			{
				if (!HasCompiledBindingPathCapability(step.Capabilities,
						CompiledBindingPathCapabilities::Read)
					|| !currentSource->TryGetValue(step.Property, value)) break;
			}
			else
			{
				BindingSourceReference item;
				if (!currentList->TryGetItem(step.ListIndex, item) || !item) break;
				value = BindingValue(std::move(item));
			}

			BindingSourceReference nested;
			if (value.TryGet(nested) && nested)
			{
				result.Owners.push_back(nested.Shared());
				currentSource = nested.Get();
				currentList = nullptr;
				continue;
			}
			BindingListReference nestedList;
			if (value.TryGet(nestedList) && nestedList)
			{
				result.ListOwners.push_back(nestedList.Shared());
				currentSource = nullptr;
				currentList = nestedList.Get();
				continue;
			}
			break;
		}
	}

	for (auto& pathWatch : watches)
	{
		const bool allProperties = pathWatch.AllProperties;
		auto properties = std::move(pathWatch.Properties);
		result.Connections.push_back(pathWatch.Source->PropertyChanged().Subscribe(
			[changed, allProperties, properties = std::move(properties)](
				const PropertyChangedEventArgs& args)
			{
				if (allProperties || !args.PropertyToken
					|| properties.contains(args.PropertyToken.Value)) changed();
			}));
	}
	for (auto* list : listWatches)
		result.Connections.push_back(list->SubscribeChanged(
			[changed](const CollectionChangedEventArgs&) { changed(); }));
	return result;
}

inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::initializer_list<CompiledBindingPathView> paths,
	std::function<void()> changed)
{
	return ObserveBindingPaths(
		source,
		std::span<const CompiledBindingPathView>{ paths.begin(), paths.size() },
		std::move(changed));
}

#if CUI_ENABLE_DYNAMIC_XAML
inline std::wstring GetBindingRecordText(
	const BindingSourceReference& item,
	const std::wstring& path)
{
	if (!item || path.empty()) return {};
	BindingValue value;
	return TryGetBindingPathValue(*item.Get(), path, value)
		? value.ToString() : std::wstring{};
}
#endif

inline std::wstring GetBindingRecordText(
	const BindingSourceReference& item,
	CompiledBindingPathView path)
{
	if (!item || path.Empty()) return {};
	BindingValue value;
	return TryGetBindingPathValue(*item.Get(), path, value)
		? value.ToString() : std::wstring{};
}

/** Reads one ItemsSource value. An empty path denotes the item record itself. */
#if CUI_ENABLE_DYNAMIC_XAML
inline bool TryGetBindingListItemValue(
	const BindingListReference& source,
	size_t index,
	const std::wstring& path,
	BindingValue& out)
{
	if (!source) return false;
	BindingSourceReference item;
	if (!source.Get()->TryGetItem(index, item) || !item) return false;
	if (path.empty())
	{
		out = BindingValue(item);
		return true;
	}
	return TryGetBindingPathValue(*item.Get(), path, out);
}
#endif

inline bool TryGetBindingListItemValue(
	const BindingListReference& source,
	size_t index,
	CompiledBindingPathView path,
	BindingValue& out)
{
	if (!source) return false;
	BindingSourceReference item;
	if (!source.Get()->TryGetItem(index, item) || !item) return false;
	if (path.Empty())
	{
		out = BindingValue(item);
		return true;
	}
	return TryGetBindingPathValue(*item.Get(), path, out);
}

/** SelectedValue equality preserves scalar types and record identity. */
inline bool BindingItemValuesEqual(
	const BindingValue& left,
	const BindingValue& right)
{
	BindingSourceReference leftItem;
	BindingSourceReference rightItem;
	if (left.TryGet(leftItem) && right.TryGet(rightItem))
		return leftItem.Shared() == rightItem.Shared();
	return BindingValuesEqual(left, right);
}

#if CUI_ENABLE_DYNAMIC_XAML
inline int FindBindingListItemByValue(
	const BindingListReference& source,
	const std::wstring& path,
	const BindingValue& value)
{
	if (!source) return -1;
	for (size_t index = 0; index < source.Get()->Count(); ++index)
	{
		BindingValue candidate;
		if (TryGetBindingListItemValue(source, index, path, candidate)
			&& BindingItemValuesEqual(candidate, value))
			return static_cast<int>(index);
	}
	return -1;
}
#endif

inline int FindBindingListItemByValue(
	const BindingListReference& source,
	CompiledBindingPathView path,
	const BindingValue& value)
{
	if (!source) return -1;
	for (size_t index = 0; index < source.Get()->Count(); ++index)
	{
		BindingValue candidate;
		if (TryGetBindingListItemValue(source, index, path, candidate)
			&& BindingItemValuesEqual(candidate, value))
			return static_cast<int>(index);
	}
	return -1;
}
