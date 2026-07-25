#pragma once

#include "Event.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <span>
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

/** A structurally observable collection with a private contiguous store. */
template<typename T>
class ObservableCollection : private std::vector<T>
{
public:
	using Base = std::vector<T>;
	using typename Base::const_iterator;
	using typename Base::const_reverse_iterator;
	using typename Base::size_type;
	using typename Base::value_type;
	using Base::capacity;
	using Base::empty;
	using Base::get_allocator;
	using Base::max_size;
	using Base::reserve;
	using Base::shrink_to_fit;
	using Base::size;
	using OwnerChangedHandler =
		std::function<void(const CollectionChangedEventArgs&)>;
	using OwnerChangingHandler = std::function<void()>;
	using CollectionChangedEvent = Event<void(
		ObservableCollection<T>*, const CollectionChangedEventArgs&)>;
	using ReadOnlyView = std::span<const T>;

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

	[[nodiscard]] const T& at(size_type index) const { return Base::at(index); }
	[[nodiscard]] const T& operator[](size_type index) const noexcept
	{
		return Base::operator[](index);
	}
	[[nodiscard]] const T& front() const noexcept { return Base::front(); }
	[[nodiscard]] const T& back() const noexcept { return Base::back(); }
	[[nodiscard]] const T* data() const noexcept { return Base::data(); }
	[[nodiscard]] const_iterator begin() const noexcept { return Base::cbegin(); }
	[[nodiscard]] const_iterator end() const noexcept { return Base::cend(); }
	[[nodiscard]] const_iterator cbegin() const noexcept { return Base::cbegin(); }
	[[nodiscard]] const_iterator cend() const noexcept { return Base::cend(); }
	[[nodiscard]] const_reverse_iterator rbegin() const noexcept
	{
		return Base::crbegin();
	}
	[[nodiscard]] const_reverse_iterator rend() const noexcept
	{
		return Base::crend();
	}
	[[nodiscard]] const_reverse_iterator crbegin() const noexcept
	{
		return Base::crbegin();
	}
	[[nodiscard]] const_reverse_iterator crend() const noexcept
	{
		return Base::crend();
	}

	/** Exposes a non-owning read-only view without leaking the vector base. */
	[[nodiscard]] ReadOnlyView View() const noexcept
	{
		return ReadOnlyView(Base::data(), Base::size());
	}

	ObservableCollection() = default;
	ObservableCollection(std::initializer_list<T> values) : Base(values) {}
	ObservableCollection(const Base& values) : Base(values) {}
	ObservableCollection(Base&& values) noexcept : Base(std::move(values)) {}
	ObservableCollection(const ObservableCollection& other)
		: Base(static_cast<const Base&>(other)) {}
	ObservableCollection(ObservableCollection&& other) noexcept
		: Base(std::move(static_cast<Base&>(other))) {}

	friend bool operator==(
		const ObservableCollection& left,
		const ObservableCollection& right)
	{
		return static_cast<const Base&>(left)
			== static_cast<const Base&>(right);
	}
	friend bool operator==(
		const ObservableCollection& left, const Base& right)
	{
		return static_cast<const Base&>(left) == right;
	}
	friend bool operator==(
		const Base& left, const ObservableCollection& right)
	{
		return left == static_cast<const Base&>(right);
	}

	ObservableCollection& operator=(const ObservableCollection& other)
	{
		if (this == &other) return *this;
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::operator=(static_cast<const Base&>(other));
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(ObservableCollection&& other)
	{
		if (this == &other) return *this;
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::operator=(std::move(static_cast<Base&>(other)));
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(const Base& values)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::operator=(values);
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(Base&& values)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::operator=(std::move(values));
		PublishReset(oldSize);
		return *this;
	}
	ObservableCollection& operator=(std::initializer_list<T> values)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::operator=(values);
		PublishReset(oldSize);
		return *this;
	}

	void SetOwnerChangedHandler(OwnerChangedHandler handler)
	{
		_ownerChanged = std::move(handler);
	}
	/** Validates a mutation before the contiguous store is modified. */
	void SetOwnerChangingHandler(OwnerChangingHandler handler)
	{
		_ownerChanging = std::move(handler);
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
		Changed.InvokeCore(this, change);
	}
	bool IsUpdating() const noexcept { return _updateDepth != 0; }
	[[nodiscard]] UpdateScope DeferNotifications() noexcept
	{
		return UpdateScope(*this);
	}

	void push_back(const T& value)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::push_back(value);
		PublishAdd(oldSize, 1, oldSize);
	}
	void push_back(T&& value)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::push_back(std::move(value));
		PublishAdd(oldSize, 1, oldSize);
	}
	template<typename... Args>
	const T& emplace_back(Args&&... args)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::emplace_back(std::forward<Args>(args)...);
		PublishAdd(oldSize, 1, oldSize);
		return Base::back();
	}
	void pop_back()
	{
		if (Base::empty()) return;
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::pop_back();
		PublishRemove(oldSize - 1, 1, oldSize);
	}

	const_iterator insert(const_iterator position, const T& value)
	{
		VerifyMutationAllowed();
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, value);
		PublishAdd(index, 1, oldSize);
		return result;
	}
	const_iterator insert(const_iterator position, T&& value)
	{
		VerifyMutationAllowed();
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, std::move(value));
		PublishAdd(index, 1, oldSize);
		return result;
	}
	const_iterator insert(const_iterator position, size_t count, const T& value)
	{
		if (count != 0) VerifyMutationAllowed();
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, count, value);
		if (count != 0) PublishAdd(index, count, oldSize);
		return result;
	}
	template<typename InputIt,
		std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
	const_iterator insert(const_iterator position, InputIt first, InputIt last)
	{
		const size_t index = IndexOf(position);
		if (first != last) VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		auto result = Base::insert(position, first, last);
		const size_t count = Base::size() - oldSize;
		if (count != 0) PublishAdd(index, count, oldSize);
		return result;
	}
	const_iterator insert(const_iterator position, std::initializer_list<T> values)
	{
		return insert(position, values.begin(), values.end());
	}
	template<typename... Args>
	const_iterator emplace(const_iterator position, Args&&... args)
	{
		VerifyMutationAllowed();
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::emplace(position, std::forward<Args>(args)...);
		PublishAdd(index, 1, oldSize);
		return result;
	}

	const_iterator erase(const_iterator position)
	{
		if (position == Base::cend()) return Base::cend();
		VerifyMutationAllowed();
		const size_t index = IndexOf(position);
		const size_t oldSize = Base::size();
		auto result = Base::erase(position);
		PublishRemove(index, 1, oldSize);
		return result;
	}
	const_iterator erase(const_iterator first, const_iterator last)
	{
		const size_t index = IndexOf(first);
		const size_t count = static_cast<size_t>(std::distance(first, last));
		if (count != 0) VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		auto result = Base::erase(first, last);
		if (count != 0) PublishRemove(index, count, oldSize);
		return result;
	}
	void clear()
	{
		if (Base::empty()) return;
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::clear();
		PublishRemove(0, oldSize, oldSize);
	}

	void resize(size_t count)
	{
		const size_t oldSize = Base::size();
		if (count == oldSize) return;
		VerifyMutationAllowed();
		Base::resize(count);
		if (count > oldSize) PublishAdd(oldSize, count - oldSize, oldSize);
		else PublishRemove(count, oldSize - count, oldSize);
	}
	void resize(size_t count, const T& value)
	{
		const size_t oldSize = Base::size();
		if (count == oldSize) return;
		VerifyMutationAllowed();
		Base::resize(count, value);
		if (count > oldSize) PublishAdd(oldSize, count - oldSize, oldSize);
		else PublishRemove(count, oldSize - count, oldSize);
	}
	template<typename InputIt,
		std::enable_if_t<!std::is_integral_v<InputIt>, int> = 0>
	void assign(InputIt first, InputIt last)
	{
		VerifyMutationAllowed();
		const size_t oldSize = Base::size();
		Base::assign(first, last);
		PublishReset(oldSize);
	}
	void assign(size_t count, const T& value)
	{
		VerifyMutationAllowed();
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
		VerifyMutationAllowed();
		Base::operator[](index) = value;
		PublishReplace(index);
		return true;
	}
	bool Replace(size_t index, T&& value)
	{
		if (index >= Base::size()) return false;
		VerifyMutationAllowed();
		Base::operator[](index) = std::move(value);
		PublishReplace(index);
		return true;
	}
	bool Move(size_t oldIndex, size_t newIndex)
	{
		if (oldIndex >= Base::size() || newIndex >= Base::size()) return false;
		if (oldIndex == newIndex) return true;
		VerifyMutationAllowed();
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
		VerifyMutationAllowed();
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
		if (Base::size() > 1) VerifyMutationAllowed();
		std::stable_sort(Base::begin(), Base::end(), std::move(compare));
		PublishReset(Base::size());
	}

	/**
	 * Explicitly edits one item and publishes a Replace notification.
	 *
	 * Mutable iterators and writable indexing are intentionally not exposed:
	 * callers that change item data must make that write visible at the
	 * collection boundary.  Owners that implement a separate item-property
	 * notification contract use OwnedObservableCollection instead.
	 */
	template<typename Editor>
	bool Modify(size_type index, Editor&& editor)
	{
		if (index >= Base::size()) return false;
		VerifyMutationAllowed();
		std::invoke(
			std::forward<Editor>(editor), Base::operator[](index));
		PublishReplace(index);
		return true;
	}

protected:
	/** Restores an owner-validated mutation that was rejected mid-notification. */
	void RestoreOwnerSnapshot(ReadOnlyView values)
	{
		Base::assign(values.begin(), values.end());
	}
	/**
	 * Lets a collection owner update an item's own state without pretending the
	 * collection structure changed.  Only an OwnedObservableCollection can
	 * surface this operation, and only to its declared owner type.
	 */
	T& MutableItemForOwner(size_type index)
	{
		return Base::at(index);
	}

private:
	void VerifyMutationAllowed()
	{
		if (_ownerChanging) _ownerChanging();
	}
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
		Changed.InvokeCore(this, args);
	}

	OwnerChangedHandler _ownerChanged;
	OwnerChangingHandler _ownerChanging;
	unsigned int _updateDepth = 0;
	bool _batchChanged = false;
	bool _keepOwnerSynchronized = false;
	bool _synchronizeOwnerDuringUpdates = false;
	size_t _batchOldSize = 0;
};

/**
 * ObservableCollection with a private item-state channel for its owning type.
 * Structural changes remain observable; item property changes are coordinated
 * by the owner and therefore do not generate fake collection Replace events.
 */
template<typename T, typename TOwner>
class OwnedObservableCollection : public ObservableCollection<T>
{
public:
	using ObservableCollection<T>::ObservableCollection;
	using ObservableCollection<T>::operator=;

private:
	friend TOwner;
	T& MutableAt(typename ObservableCollection<T>::size_type index)
	{
		return this->MutableItemForOwner(index);
	}
};
