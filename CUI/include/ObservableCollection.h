#pragma once

#include "Event.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

enum class CollectionChangeAction : uint8_t
{
	Add,
	Remove,
	Replace,
	Move,
	Swap,
	Reset
};

struct CollectionChangedEventArgs
{
	static constexpr size_t Npos = static_cast<size_t>(-1);

	CollectionChangeAction Action = CollectionChangeAction::Reset;
	size_t OldIndex = Npos;
	size_t NewIndex = Npos;
	size_t OldCount = 0;
	size_t NewCount = 0;
	size_t OldSize = 0;
	size_t NewSize = 0;
};

/**
 * A vector-compatible collection that publishes structural mutations.
 *
 * The public inheritance intentionally preserves the existing CUI source API:
 * an ObservableCollection can still be passed to code that reads a std::vector.
 * Mutations made through this concrete type are observed. Code that explicitly
 * casts it to std::vector and mutates the base, or reorders through raw iterators,
 * must call NotifyReset() after the operation.
 */
template<typename T>
class ObservableCollection : public std::vector<T>
{
public:
	using Base = std::vector<T>;
	using typename Base::const_iterator;
	using typename Base::iterator;
	using OwnerChangedHandler =
		std::function<void(const CollectionChangedEventArgs&)>;
	using CollectionChangedEvent = Event<void(
		ObservableCollection<T>*, const CollectionChangedEventArgs&)>;

	class UpdateScope final
	{
	public:
		UpdateScope() = default;
		explicit UpdateScope(ObservableCollection& owner) noexcept
			: _owner(&owner)
		{
			_owner->BeginUpdate();
		}
		~UpdateScope() { Commit(); }

		UpdateScope(const UpdateScope&) = delete;
		UpdateScope& operator=(const UpdateScope&) = delete;
		UpdateScope(UpdateScope&& other) noexcept : _owner(other._owner)
		{
			other._owner = nullptr;
		}
		UpdateScope& operator=(UpdateScope&& other) noexcept
		{
			if (this == &other) return *this;
			Commit();
			_owner = other._owner;
			other._owner = nullptr;
			return *this;
		}
		void Commit() noexcept
		{
			if (!_owner) return;
			auto* owner = _owner;
			_owner = nullptr;
			owner->EndUpdate();
		}

	private:
		ObservableCollection* _owner = nullptr;
	};

	CollectionChangedEvent Changed;

	ObservableCollection() = default;
	ObservableCollection(std::initializer_list<T> values) : Base(values) {}
	ObservableCollection(const Base& values) : Base(values) {}
	ObservableCollection(Base&& values) noexcept : Base(std::move(values)) {}
	ObservableCollection(const ObservableCollection& other)
		: Base(static_cast<const Base&>(other)) {}
	ObservableCollection(ObservableCollection&& other) noexcept
		: Base(std::move(static_cast<Base&>(other))) {}

	ObservableCollection& operator=(const ObservableCollection& other)
	{
		if (this == &other) return *this;
		const size_t oldSize = Base::size();
		Base::operator=(static_cast<const Base&>(other));
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(ObservableCollection&& other)
	{
		if (this == &other) return *this;
		const size_t oldSize = Base::size();
		Base::operator=(std::move(static_cast<Base&>(other)));
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(const Base& values)
	{
		const size_t oldSize = Base::size();
		Base::operator=(values);
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(Base&& values)
	{
		const size_t oldSize = Base::size();
		Base::operator=(std::move(values));
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(std::initializer_list<T> values)
	{
		const size_t oldSize = Base::size();
		Base::operator=(values);
		PublishReset(oldSize);
		return *this;
	}

	void SetOwnerChangedHandler(OwnerChangedHandler handler)
	{
		_ownerChanged = std::move(handler);
	}
	/** Keep the owner internally coherent for every mutation in a public batch. */
	void SetOwnerSynchronizationDuringUpdates(bool value) noexcept
	{
		_synchronizeOwnerDuringUpdates = value;
	}

	void BeginUpdate(bool keepOwnerSynchronized = false) noexcept
	{
		if (_updateDepth++ == 0)
		{
			_batchOldSize = Base::size();
			_batchChanged = false;
			_keepOwnerSynchronized = keepOwnerSynchronized
				|| _synchronizeOwnerDuringUpdates;
		}
		else if (keepOwnerSynchronized)
			_keepOwnerSynchronized = true;
	}
	void EndUpdate() noexcept
	{
		if (_updateDepth == 0) return;
		if (--_updateDepth != 0) return;
		if (!_batchChanged)
		{
			_keepOwnerSynchronized = false;
			return;
		}
		_batchChanged = false;
		const CollectionChangedEventArgs change{
			CollectionChangeAction::Reset,
			CollectionChangedEventArgs::Npos,
			CollectionChangedEventArgs::Npos,
			_batchOldSize,
			Base::size(),
			_batchOldSize,
			Base::size()
		};
		if (!_keepOwnerSynchronized && _ownerChanged)
			_ownerChanged(change);
		_keepOwnerSynchronized = false;
		Changed(this, change);
	}
	bool IsUpdating() const noexcept { return _updateDepth != 0; }
	[[nodiscard]] UpdateScope DeferNotifications() noexcept
	{
		return UpdateScope(*this);
	}

	void push_back(const T& value)
	{
		const size_t oldSize = Base::size();
		Base::push_back(value);
		PublishAdd(oldSize, 1, oldSize);
	}
	void push_back(T&& value)
	{
		const size_t oldSize = Base::size();
		Base::push_back(std::move(value));
		PublishAdd(oldSize, 1, oldSize);
	}
	template<typename... Args>
	T& emplace_back(Args&&... args)
	{
		const size_t oldSize = Base::size();
		Base::emplace_back(std::forward<Args>(args)...);
		PublishAdd(oldSize, 1, oldSize);
		return Base::back();
	}
	void pop_back()
	{
		if (Base::empty()) return;
		const size_t oldSize = Base::size();
		Base::pop_back();
		PublishRemove(oldSize - 1, 1, oldSize);
	}

	iterator insert(const_iterator position, const T& value)
	{
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, value);
		PublishAdd(index, 1, oldSize);
		return result;
	}
	iterator insert(const_iterator position, T&& value)
	{
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, std::move(value));
		PublishAdd(index, 1, oldSize);
		return result;
	}
	iterator insert(const_iterator position, size_t count, const T& value)
	{
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, count, value);
		if (count != 0) PublishAdd(index, count, oldSize);
		return result;
	}
	template<typename InputIt,
		std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
	iterator insert(const_iterator position, InputIt first, InputIt last)
	{
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, first, last);
		const size_t count = Base::size() - oldSize;
		if (count != 0) PublishAdd(index, count, oldSize);
		return result;
	}
	iterator insert(const_iterator position, std::initializer_list<T> values)
	{
		return insert(position, values.begin(), values.end());
	}
	template<typename... Args>
	iterator emplace(const_iterator position, Args&&... args)
	{
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::emplace(position, std::forward<Args>(args)...);
		PublishAdd(index, 1, oldSize);
		return result;
	}

	iterator erase(const_iterator position)
	{
		if (position == Base::cend()) return Base::end();
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::erase(position);
		PublishRemove(index, 1, oldSize);
		return result;
	}
	iterator erase(const_iterator first, const_iterator last)
	{
		const size_t index = IndexOf(first);
		const size_t count = static_cast<size_t>(std::distance(first, last));
		const size_t oldSize = Base::size();
		auto result = Base::erase(first, last);
		if (count != 0) PublishRemove(index, count, oldSize);
		return result;
	}
	void clear()
	{
		if (Base::empty()) return;
		const size_t oldSize = Base::size();
		Base::clear();
		PublishRemove(0, oldSize, oldSize);
	}

	void resize(size_t count)
	{
		const size_t oldSize = Base::size();
		if (count == oldSize) return;
		Base::resize(count);
		if (count > oldSize) PublishAdd(oldSize, count - oldSize, oldSize);
		else PublishRemove(count, oldSize - count, oldSize);
	}
	void resize(size_t count, const T& value)
	{
		const size_t oldSize = Base::size();
		if (count == oldSize) return;
		Base::resize(count, value);
		if (count > oldSize) PublishAdd(oldSize, count - oldSize, oldSize);
		else PublishRemove(count, oldSize - count, oldSize);
	}
	template<typename InputIt,
		std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
	void assign(InputIt first, InputIt last)
	{
		const size_t oldSize = Base::size();
		Base::assign(first, last);
		PublishReset(oldSize);
	}
	void assign(size_t count, const T& value)
	{
		const size_t oldSize = Base::size();
		Base::assign(count, value);
		PublishReset(oldSize);
	}
	void assign(std::initializer_list<T> values)
	{
		assign(values.begin(), values.end());
	}

	bool Replace(size_t index, const T& value)
	{
		if (index >= Base::size()) return false;
		Base::operator[](index) = value;
		PublishReplace(index);
		return true;
	}
	bool Replace(size_t index, T&& value)
	{
		if (index >= Base::size()) return false;
		Base::operator[](index) = std::move(value);
		PublishReplace(index);
		return true;
	}
	bool Move(size_t oldIndex, size_t newIndex)
	{
		if (oldIndex >= Base::size() || newIndex >= Base::size()) return false;
		if (oldIndex == newIndex) return true;
		if (oldIndex < newIndex)
			std::rotate(Base::begin() + oldIndex,
				Base::begin() + oldIndex + 1,
				Base::begin() + newIndex + 1);
		else
			std::rotate(Base::begin() + newIndex,
				Base::begin() + oldIndex,
				Base::begin() + oldIndex + 1);
		Publish(CollectionChangedEventArgs{
			CollectionChangeAction::Move,
			oldIndex,
			newIndex,
			1,
			1,
			Base::size(),
			Base::size()
		});
		return true;
	}
	bool SwapIndices(size_t first, size_t second)
	{
		if (first >= Base::size() || second >= Base::size()) return false;
		if (first == second) return true;
		std::swap(Base::operator[](first), Base::operator[](second));
		Publish(CollectionChangedEventArgs{
			CollectionChangeAction::Swap,
			first,
			second,
			1,
			1,
			Base::size(),
			Base::size()
		});
		return true;
	}

	template<typename Compare>
	void Sort(Compare compare)
	{
		std::stable_sort(Base::begin(), Base::end(), std::move(compare));
		PublishReset(Base::size());
	}

	/** Publish after a mutation performed through iterators or a base-vector API. */
	void NotifyReset()
	{
		PublishReset(Base::size());
	}

private:
	size_t IndexOf(const_iterator position) const
	{
		return static_cast<size_t>(std::distance(Base::cbegin(), position));
	}
	void PublishAdd(size_t index, size_t count, size_t oldSize)
	{
		Publish(CollectionChangedEventArgs{
			CollectionChangeAction::Add,
			CollectionChangedEventArgs::Npos,
			index,
			0,
			count,
			oldSize,
			Base::size()
		});
	}
	void PublishRemove(size_t index, size_t count, size_t oldSize)
	{
		Publish(CollectionChangedEventArgs{
			CollectionChangeAction::Remove,
			index,
			CollectionChangedEventArgs::Npos,
			count,
			0,
			oldSize,
			Base::size()
		});
	}
	void PublishReplace(size_t index)
	{
		Publish(CollectionChangedEventArgs{
			CollectionChangeAction::Replace,
			index,
			index,
			1,
			1,
			Base::size(),
			Base::size()
		});
	}
	void PublishReset(size_t oldSize)
	{
		Publish(CollectionChangedEventArgs{
			CollectionChangeAction::Reset,
			CollectionChangedEventArgs::Npos,
			CollectionChangedEventArgs::Npos,
			oldSize,
			Base::size(),
			oldSize,
			Base::size()
		});
	}
	void Publish(const CollectionChangedEventArgs& args)
	{
		if (_updateDepth != 0)
		{
			if (_keepOwnerSynchronized && _ownerChanged)
				_ownerChanged(args);
			_batchChanged = true;
			return;
		}
		PublishNow(args);
	}
	void PublishNow(const CollectionChangedEventArgs& args)
	{
		if (_ownerChanged) _ownerChanged(args);
		Changed(this, args);
	}

	OwnerChangedHandler _ownerChanged;
	unsigned int _updateDepth = 0;
	bool _batchChanged = false;
	bool _keepOwnerSynchronized = false;
	bool _synchronizeOwnerDuringUpdates = false;
	size_t _batchOldSize = 0;
};
