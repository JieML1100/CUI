#include "CollectionViewSource.h"
#include "EventInfrastructure.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace
{
	std::atomic_size_t NextCollectionOccurrenceIdentity{ 1 };
	constexpr size_t PassThroughOccurrenceCacheCapacity = 128;

	/**
	 * Immutable projection boundary for a pass-through CollectionViewSource.
	 *
	 * The underlying version may itself expose source-specific Groups or
	 * CurrentItem state. A no-shaping CollectionViewSource does not adopt that
	 * source-owned group/currency state, so returning the source object directly
	 * would change the public projection after rollback. This wrapper deliberately
	 * forwards only the item/order/occurrence surface that belongs to the view.
	 */
	class PassThroughStableSnapshot final
		: public IBindingList,
		  public IBindingListOccurrenceIdentity,
		  public IBindingListOccurrenceLookup,
		  public IBindingListStableSnapshot
	{
	public:
		explicit PassThroughStableSnapshot(BindingListReference source) noexcept
			: _source(std::move(source)) {}

		size_t Count() const noexcept override
		{
			return _source ? _source.Get()->Count() : 0;
		}
		bool TryGetItem(
			size_t index, BindingSourceReference& out) const override
		{
			return _source && _source.Get()->TryGetItem(index, out);
		}
		EventConnection SubscribeChanged(ChangedHandler) override { return {}; }
		DataTypeToken GetItemTypeToken() const noexcept override
		{
			return _source
				? _source.Get()->GetItemTypeToken() : DataTypeToken{};
		}
		bool TryGetItemOccurrenceIdentity(
			size_t index, size_t& result) const noexcept override
		{
			result = 0;
			const auto* identities = _source
				? dynamic_cast<const IBindingListOccurrenceIdentity*>(
					_source.Get()) : nullptr;
			return identities
				&& identities->TryGetItemOccurrenceIdentity(index, result)
				&& result != 0;
		}
		bool TryGetItemIndexByOccurrenceIdentity(
			size_t identity, size_t& index) const noexcept override
		{
			index = 0;
			if (!_source || identity == 0) return false;
			const auto* identities = dynamic_cast<
				const IBindingListOccurrenceIdentity*>(_source.Get());
			if (!identities) return false;
			if (const auto* lookup = dynamic_cast<
				const IBindingListOccurrenceLookup*>(_source.Get()))
			{
				size_t candidateIndex = 0;
				size_t candidateIdentity = 0;
				if (lookup->TryGetItemIndexByOccurrenceIdentity(
						identity, candidateIndex)
					&& candidateIndex < _source.Get()->Count()
					&& identities->TryGetItemOccurrenceIdentity(
						candidateIndex, candidateIdentity)
					&& candidateIdentity == identity)
				{
					index = candidateIndex;
					return true;
				}
			}
			const size_t count = _source.Get()->Count();
			for (size_t candidateIndex = 0;
				candidateIndex < count; ++candidateIndex)
			{
				size_t candidateIdentity = 0;
				if (!identities->TryGetItemOccurrenceIdentity(
						candidateIndex, candidateIdentity)
					|| candidateIdentity != identity) continue;
				index = candidateIndex;
				return true;
			}
			return false;
		}
		bool IsItemIndexByOccurrenceIdentityLookupBounded()
			const noexcept override
		{
			const auto* lookup = _source
				? dynamic_cast<const IBindingListOccurrenceLookup*>(_source.Get())
				: nullptr;
			return lookup
				&& lookup->IsItemIndexByOccurrenceIdentityLookupBounded();
		}
#if CUI_ENABLE_DYNAMIC_XAML
		const std::wstring& ItemTypeName() const noexcept override
		{
			static const std::wstring empty;
			return _source ? _source.Get()->ItemTypeName() : empty;
		}
#endif

	private:
		const BindingListReference _source;
	};

	size_t AllocateCollectionOccurrenceIdentityRange(size_t count)
	{
		if (count == 0)
			throw std::invalid_argument(
				"CollectionViewSource occurrence identity range is empty");
		size_t candidate = NextCollectionOccurrenceIdentity.load(
			std::memory_order_relaxed);
		for (;;)
		{
			if (candidate == 0
				|| count > (std::numeric_limits<size_t>::max)() - candidate)
				throw std::overflow_error(
					"CollectionViewSource occurrence identity exhausted");
			if (NextCollectionOccurrenceIdentity.compare_exchange_weak(
				candidate, candidate + count,
				std::memory_order_relaxed, std::memory_order_relaxed))
				return candidate;
		}
	}

	size_t AllocateCollectionOccurrenceIdentity()
	{
		return AllocateCollectionOccurrenceIdentityRange(1);
	}

	std::wstring Fold(std::wstring value, bool ignoreCase)
	{
		if (ignoreCase)
			std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	int CompareStringViews(
		std::wstring_view left,
		std::wstring_view right,
		bool ignoreCase) noexcept
	{
		const size_t common = (std::min)(left.size(), right.size());
		for (size_t index = 0; index < common; ++index)
		{
			const wchar_t l = ignoreCase ? towlower(left[index]) : left[index];
			const wchar_t r = ignoreCase ? towlower(right[index]) : right[index];
			if (l < r) return -1;
			if (l > r) return 1;
		}
		return left.size() < right.size()
			? -1 : left.size() > right.size() ? 1 : 0;
	}

	bool StringRegionEquals(
		std::wstring_view text,
		size_t offset,
		std::wstring_view pattern,
		bool ignoreCase) noexcept
	{
		if (offset > text.size()
			|| pattern.size() > text.size() - offset) return false;
		for (size_t index = 0; index < pattern.size(); ++index)
		{
			const wchar_t candidate = text[offset + index];
			const wchar_t expected = pattern[index];
			if (candidate == expected) continue;
			if (!ignoreCase
				|| towlower(candidate) != towlower(expected)) return false;
		}
		return true;
	}

	bool MatchesStringFilter(
		std::wstring_view text,
		std::wstring_view pattern,
		CollectionFilterOperator filterOperator,
		bool ignoreCase) noexcept
	{
		switch (filterOperator)
		{
		case CollectionFilterOperator::Contains:
			if (!ignoreCase)
				return text.find(pattern) != std::wstring_view::npos;
			if (pattern.empty()) return true;
			if (pattern.size() > text.size()) return false;
			for (size_t offset = 0;
				offset <= text.size() - pattern.size(); ++offset)
			{
				if (StringRegionEquals(
					text, offset, pattern, true)) return true;
			}
			return false;
		case CollectionFilterOperator::StartsWith:
			return StringRegionEquals(text, 0, pattern, ignoreCase);
		case CollectionFilterOperator::EndsWith:
			return pattern.size() <= text.size()
				&& StringRegionEquals(text, text.size() - pattern.size(),
					pattern, ignoreCase);
		default:
			return false;
		}
	}

	bool MatchesStringFilter(
		const BindingValue& candidate,
		const BindingValue& pattern,
		CollectionFilterOperator filterOperator,
		bool ignoreCase)
	{
		std::wstring candidateStorage;
		std::wstring patternStorage;
		std::wstring_view candidateView;
		std::wstring_view patternView;
		if (!candidate.TryGetStringView(candidateView))
		{
			if (!candidate.TryGetString(candidateStorage)) return false;
			candidateView = candidateStorage;
		}
		if (!pattern.TryGetStringView(patternView))
		{
			if (!pattern.TryGetString(patternStorage)) return false;
			patternView = patternStorage;
		}
		return MatchesStringFilter(
			candidateView, patternView, filterOperator, ignoreCase);
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
		std::wstring_view leftString;
		std::wstring_view rightString;
		if (left.TryGetStringView(leftString)
			&& right.TryGetStringView(rightString))
			return CompareStringViews(leftString, rightString, ignoreCase);
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

/**
 * Order-statistic rope for occurrence identities synthesized by a no-shaping
 * pass-through view over a legacy source.
 *
 * A node describes a contiguous token interval, while its position is implicit
 * in the treap.  The ordered token table owns every node, so an allocation
 * failure during a post-commit source notification can discard even a partially
 * split forest without leaking.  Sequence queries descend by SubtreeCount;
 * reverse queries find the token interval and derive its rank through Parent.
 */
struct CollectionViewSource::PassThroughGeneratedOccurrenceIndex final
{
	struct Node final
	{
		size_t TokenStart = 0;
		size_t Count = 0;
		size_t SubtreeCount = 0;
		uint64_t Priority = 0;
		Node* Left = nullptr;
		Node* Right = nullptr;
		Node* Parent = nullptr;
	};

	using ReverseMap = std::map<size_t, std::unique_ptr<Node>>;

	struct PreparedNodes final
	{
		explicit PreparedNodes(size_t count)
		{
			Handles.reserve(count);
			ReverseMap donor;
			for (size_t index = 0; index < count; ++index)
			{
				auto [iterator, inserted] = donor.emplace(
					index, std::make_unique<Node>());
				if (!inserted)
					throw std::logic_error(
						"duplicate generated occurrence preparation key");
				Handles.push_back(donor.extract(iterator));
			}
		}

		std::vector<ReverseMap::node_type> Handles;
		size_t Next = 0;
	};

	PassThroughGeneratedOccurrenceIndex() = default;
	PassThroughGeneratedOccurrenceIndex(
		const PassThroughGeneratedOccurrenceIndex&) = delete;
	PassThroughGeneratedOccurrenceIndex& operator=(
		const PassThroughGeneratedOccurrenceIndex&) = delete;

	[[nodiscard]] std::shared_ptr<PassThroughGeneratedOccurrenceIndex>
		Clone() const
	{
		auto result =
			std::make_shared<PassThroughGeneratedOccurrenceIndex>();
		result->_priorityState = _priorityState;
		result->Root = result->CloneTree(Root, nullptr);
		return result;
	}

	[[nodiscard]] size_t Size() const noexcept
	{
		return NodeSize(Root);
	}

	void Initialize(size_t count, size_t tokenStart)
	{
		if (Root || !ByToken.empty())
			throw std::logic_error(
				"generated occurrence index initialized twice");
		if (count == 0) return;
		PreparedNodes prepared(1);
		Root = RegisterRun(prepared, tokenStart, count);
	}

	[[nodiscard]] bool TryGetIdentity(
		size_t index, size_t& identity) const noexcept
	{
		identity = 0;
		if (index >= Size()) return false;
		const Node* node = Root;
		while (node)
		{
			const size_t leftCount = NodeSize(node->Left);
			if (index < leftCount)
			{
				node = node->Left;
				continue;
			}
			const size_t offset = index - leftCount;
			if (offset < node->Count)
			{
				if (offset > (std::numeric_limits<size_t>::max)()
					- node->TokenStart) return false;
				identity = node->TokenStart + offset;
				return identity != 0;
			}
			index = offset - node->Count;
			node = node->Right;
		}
		return false;
	}

	[[nodiscard]] bool TryGetIndex(
		size_t identity, size_t& index) const noexcept
	{
		index = 0;
		if (identity == 0 || ByToken.empty()) return false;
		auto found = ByToken.upper_bound(identity);
		if (found == ByToken.begin()) return false;
		--found;
		const Node* node = found->second.get();
		if (!node || identity < node->TokenStart) return false;
		const size_t offset = identity - node->TokenStart;
		if (offset >= node->Count) return false;
		size_t rank = NodeSize(node->Left);
		for (const Node* child = node, *parent = node->Parent;
			parent; child = parent, parent = parent->Parent)
		{
			if (child != parent->Right) continue;
			const size_t prefix = NodeSize(parent->Left) + parent->Count;
			if (prefix > (std::numeric_limits<size_t>::max)() - rank)
				return false;
			rank += prefix;
		}
		if (offset > (std::numeric_limits<size_t>::max)() - rank)
			return false;
		const size_t candidate = rank + offset;
		if (candidate >= Size()) return false;
		index = candidate;
		return true;
	}

	void Apply(const CollectionChangedEventArgs& change)
	{
		auto validRange = [](size_t index, size_t count, size_t size) noexcept
		{
			return index != CollectionChangedEventArgs::Npos
				&& index <= size && count <= size - index;
		};
		if (Size() != change.OldSize)
			throw std::logic_error("stale generated occurrence rope");

		size_t preparedCount = 0;
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			if (change.OldCount != 0 || change.NewCount == 0
				|| change.OldSize > (std::numeric_limits<size_t>::max)()
					- change.NewCount
				|| change.NewSize != change.OldSize + change.NewCount
				|| change.NewIndex == CollectionChangedEventArgs::Npos
				|| change.NewIndex > change.OldSize)
				throw std::logic_error("invalid generated occurrence Add");
			preparedCount = 2; // insertion boundary plus the new token interval
			break;
		case CollectionChangeAction::Remove:
			if (change.NewCount != 0 || change.OldCount == 0
				|| !validRange(change.OldIndex, change.OldCount, change.OldSize)
				|| change.NewSize != change.OldSize - change.OldCount)
				throw std::logic_error("invalid generated occurrence Remove");
			preparedCount = 2;
			break;
		case CollectionChangeAction::Move:
			if (change.OldSize != change.NewSize || change.OldCount == 0
				|| change.OldCount != change.NewCount
				|| !validRange(change.OldIndex, change.OldCount, change.OldSize)
				|| change.NewIndex == CollectionChangedEventArgs::Npos
				|| change.NewIndex > change.NewSize - change.NewCount)
				throw std::logic_error("invalid generated occurrence Move");
			preparedCount = 3;
			break;
		case CollectionChangeAction::Swap:
			if (change.OldSize != change.NewSize
				|| change.OldCount != 1 || change.NewCount != 1
				|| change.OldIndex >= change.OldSize
				|| change.NewIndex >= change.NewSize)
				throw std::logic_error("invalid generated occurrence Swap");
			preparedCount = 4;
			break;
		case CollectionChangeAction::Replace:
			if (change.OldSize != change.NewSize || change.OldCount == 0
				|| change.OldCount != change.NewCount
				|| change.OldIndex != change.NewIndex
				|| !validRange(change.OldIndex, change.OldCount, change.OldSize))
				throw std::logic_error("invalid generated occurrence Replace");
			return; // physical slot identities survive Replace
		default:
			throw std::logic_error("unsupported generated occurrence change");
		}

		// All allocations happen before the first split. Once mutation starts,
		// split/join/map-node transfer are allocation-free and cannot expose a
		// half-committed valid index.
		const size_t addedTokenStart =
			change.Action == CollectionChangeAction::Add
			? AllocateCollectionOccurrenceIdentityRange(change.NewCount) : 0;
		PreparedNodes prepared(preparedCount);
		Node* added = nullptr;
		if (change.Action == CollectionChangeAction::Add)
			added = RegisterRun(
				prepared, addedTokenStart, change.NewCount);

		switch (change.Action)
		{
		case CollectionChangeAction::Add:
		{
			auto [before, after] = Split(
				Root, change.NewIndex, prepared);
			Root = Join(Join(before, added), after);
			break;
		}
		case CollectionChangeAction::Remove:
		{
			auto [before, tail] = Split(
				Root, change.OldIndex, prepared);
			auto [removed, after] = Split(
				tail, change.OldCount, prepared);
			EraseTree(removed);
			Root = Join(before, after);
			break;
		}
		case CollectionChangeAction::Move:
		{
			auto [before, tail] = Split(
				Root, change.OldIndex, prepared);
			auto [moved, after] = Split(
				tail, change.OldCount, prepared);
			Node* remaining = Join(before, after);
			auto [destinationBefore, destinationAfter] = Split(
				remaining, change.NewIndex, prepared);
			Root = Join(Join(destinationBefore, moved), destinationAfter);
			break;
		}
		case CollectionChangeAction::Swap:
		{
			if (change.OldIndex == change.NewIndex) break;
			const size_t lower = (std::min)(
				change.OldIndex, change.NewIndex);
			const size_t upper = (std::max)(
				change.OldIndex, change.NewIndex);
			auto [prefix, lowerTail] = Split(Root, lower, prepared);
			auto [lowerItem, middleTail] = Split(lowerTail, 1, prepared);
			auto [middle, upperTail] = Split(
				middleTail, upper - lower - 1, prepared);
			auto [upperItem, suffix] = Split(upperTail, 1, prepared);
			Root = Join(Join(Join(Join(
				prefix, upperItem), middle), lowerItem), suffix);
			break;
		}
		default:
			break;
		}
		if (Root) Root->Parent = nullptr;
		if (Size() != change.NewSize)
			throw std::logic_error("invalid generated occurrence rope result");
	}

private:
	static size_t NodeSize(const Node* node) noexcept
	{
		return node ? node->SubtreeCount : 0;
	}

	static void Update(Node* node) noexcept
	{
		if (!node) return;
		node->SubtreeCount = NodeSize(node->Left)
			+ node->Count + NodeSize(node->Right);
		if (node->Left) node->Left->Parent = node;
		if (node->Right) node->Right->Parent = node;
	}

	uint64_t NextPriority() noexcept
	{
		// SplitMix64 gives deterministic, well-distributed priorities even though
		// occurrence token domains themselves are monotonically allocated.
		_priorityState += 0x9e3779b97f4a7c15ULL;
		uint64_t value = _priorityState;
		value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
		value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
		return value ^ (value >> 31);
	}

	Node* RegisterRun(
		PreparedNodes& prepared, size_t tokenStart, size_t count)
	{
		if (tokenStart == 0 || count == 0
			|| tokenStart > (std::numeric_limits<size_t>::max)() - count
			|| prepared.Next >= prepared.Handles.size())
			throw std::logic_error("invalid generated occurrence run");
		auto handle = std::move(prepared.Handles[prepared.Next++]);
		handle.key() = tokenStart;
		Node* node = handle.mapped().get();
		*node = Node{};
		node->TokenStart = tokenStart;
		node->Count = count;
		node->SubtreeCount = count;
		node->Priority = NextPriority();
		auto inserted = ByToken.insert(std::move(handle));
		if (!inserted.inserted)
			throw std::logic_error(
				"overlapping generated occurrence token boundary");
		return node;
	}

	static Node* Merge(Node* left, Node* right) noexcept
	{
		if (!left) return right;
		if (!right) return left;
		if (left->Priority <= right->Priority)
		{
			left->Right = Merge(left->Right, right);
			Update(left);
			return left;
		}
		right->Left = Merge(left, right->Left);
		Update(right);
		return right;
	}

	static Node* ExtractRightmost(Node*& root) noexcept
	{
		if (!root) return nullptr;
		if (root->Right)
		{
			Node* result = ExtractRightmost(root->Right);
			Update(root);
			root->Parent = nullptr;
			return result;
		}
		Node* result = root;
		root = result->Left;
		if (root) root->Parent = nullptr;
		result->Left = nullptr;
		result->Right = nullptr;
		result->Parent = nullptr;
		Update(result);
		return result;
	}

	static Node* ExtractLeftmost(Node*& root) noexcept
	{
		if (!root) return nullptr;
		if (root->Left)
		{
			Node* result = ExtractLeftmost(root->Left);
			Update(root);
			root->Parent = nullptr;
			return result;
		}
		Node* result = root;
		root = result->Right;
		if (root) root->Parent = nullptr;
		result->Left = nullptr;
		result->Right = nullptr;
		result->Parent = nullptr;
		Update(result);
		return result;
	}

	Node* Join(Node* left, Node* right) noexcept
	{
		if (!left)
		{
			if (right) right->Parent = nullptr;
			return right;
		}
		if (!right)
		{
			left->Parent = nullptr;
			return left;
		}
		Node* leftLast = left;
		while (leftLast->Right) leftLast = leftLast->Right;
		Node* rightFirst = right;
		while (rightFirst->Left) rightFirst = rightFirst->Left;
		const bool contiguous = leftLast->TokenStart
			<= (std::numeric_limits<size_t>::max)() - leftLast->Count
			&& leftLast->TokenStart + leftLast->Count
				== rightFirst->TokenStart;
		if (!contiguous)
		{
			left->Parent = nullptr;
			right->Parent = nullptr;
			Node* result = Merge(left, right);
			if (result) result->Parent = nullptr;
			return result;
		}

		Node* merged = ExtractRightmost(left);
		Node* retired = ExtractLeftmost(right);
		merged->Count += retired->Count;
		Update(merged);
		const size_t retiredTokenStart = retired->TokenStart;
		ByToken.erase(retiredTokenStart);
		Node* result = Merge(Merge(left, merged), right);
		if (result) result->Parent = nullptr;
		return result;
	}

	std::pair<Node*, Node*> Split(
		Node* root, size_t index, PreparedNodes& prepared)
	{
		if (!root)
		{
			if (index != 0)
				throw std::logic_error(
					"generated occurrence split outside rope");
			return {};
		}
		const size_t leftCount = NodeSize(root->Left);
		if (index < leftCount)
		{
			auto [before, after] = Split(root->Left, index, prepared);
			root->Left = after;
			Update(root);
			root->Parent = nullptr;
			if (before) before->Parent = nullptr;
			return { before, root };
		}
		if (index > leftCount + root->Count)
		{
			auto [before, after] = Split(
				root->Right, index - leftCount - root->Count, prepared);
			root->Right = before;
			Update(root);
			root->Parent = nullptr;
			if (after) after->Parent = nullptr;
			return { root, after };
		}
		if (index == leftCount)
		{
			Node* before = root->Left;
			root->Left = nullptr;
			Update(root);
			root->Parent = nullptr;
			if (before) before->Parent = nullptr;
			return { before, root };
		}
		if (index == leftCount + root->Count)
		{
			Node* after = root->Right;
			root->Right = nullptr;
			Update(root);
			root->Parent = nullptr;
			if (after) after->Parent = nullptr;
			return { root, after };
		}

		const size_t firstCount = index - leftCount;
		Node* second = RegisterRun(prepared,
			root->TokenStart + firstCount, root->Count - firstCount);
		Node* before = root->Left;
		Node* after = root->Right;
		root->Left = nullptr;
		root->Right = nullptr;
		root->Parent = nullptr;
		root->Count = firstCount;
		Update(root);
		before = Merge(before, root);
		after = Merge(second, after);
		if (before) before->Parent = nullptr;
		if (after) after->Parent = nullptr;
		return { before, after };
	}

	void EraseTree(Node* root) noexcept
	{
		if (!root) return;
		Node* left = root->Left;
		Node* right = root->Right;
		const size_t tokenStart = root->TokenStart;
		EraseTree(left);
		EraseTree(right);
		ByToken.erase(tokenStart);
	}

	Node* CloneTree(const Node* source, Node* parent)
	{
		if (!source) return nullptr;
		auto node = std::make_unique<Node>();
		node->TokenStart = source->TokenStart;
		node->Count = source->Count;
		node->SubtreeCount = source->SubtreeCount;
		node->Priority = source->Priority;
		node->Parent = parent;
		Node* raw = node.get();
		auto [iterator, inserted] = ByToken.emplace(
			source->TokenStart, std::move(node));
		if (!inserted)
			throw std::logic_error(
				"duplicate generated occurrence token while cloning");
		raw->Left = CloneTree(source->Left, raw);
		raw->Right = CloneTree(source->Right, raw);
		return raw;
	}

	Node* Root = nullptr;
	ReverseMap ByToken;
	uint64_t _priorityState = 0x243f6a8885a308d3ULL;
};

/** Immutable pass-through snapshot with a generated occurrence-token version. */
class CollectionViewSource::PassThroughGeneratedStableSnapshot final
	: public IBindingList,
	  public IBindingListOccurrenceIdentity,
	  public IBindingListOccurrenceLookup,
	  public IBindingListStableSnapshot
{
public:
	PassThroughGeneratedStableSnapshot(
		BindingListReference source,
		std::shared_ptr<const PassThroughGeneratedOccurrenceIndex> occurrences)
		noexcept
		: _source(std::move(source)),
		  _occurrences(std::move(occurrences)) {}

	size_t Count() const noexcept override
	{
		return _source ? _source.Get()->Count() : 0;
	}
	bool TryGetItem(
		size_t index, BindingSourceReference& out) const override
	{
		return _source && _source.Get()->TryGetItem(index, out);
	}
	EventConnection SubscribeChanged(ChangedHandler) override { return {}; }
	DataTypeToken GetItemTypeToken() const noexcept override
	{
		return _source
			? _source.Get()->GetItemTypeToken() : DataTypeToken{};
	}
	bool TryGetItemOccurrenceIdentity(
		size_t index, size_t& result) const noexcept override
	{
		result = 0;
		return _occurrences
			&& _occurrences->Size() == Count()
			&& _occurrences->TryGetIdentity(index, result);
	}
	bool TryGetItemIndexByOccurrenceIdentity(
		size_t identity, size_t& index) const noexcept override
	{
		index = 0;
		return _occurrences
			&& _occurrences->Size() == Count()
			&& _occurrences->TryGetIndex(identity, index);
	}
	bool IsItemIndexByOccurrenceIdentityLookupBounded()
		const noexcept override { return true; }
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ItemTypeName() const noexcept override
	{
		static const std::wstring empty;
		return _source ? _source.Get()->ItemTypeName() : empty;
	}
#endif

private:
	const BindingListReference _source;
	const std::shared_ptr<const PassThroughGeneratedOccurrenceIndex>
		_occurrences;
};

CollectionViewSource::CollectionViewSource()
	: _sourceCallbackLifetime(std::make_shared<SourceCallbackLifetime>())
{
	_sourceCallbackLifetime->Owner = this;
}

CollectionViewSource::~CollectionViewSource()
{
	// Event takes a handler snapshot before dispatch. Disconnecting cannot remove
	// a callback already present in that snapshot, so revoke the callback's weak
	// lifetime state before releasing the connection and the rest of this object.
	if (_sourceCallbackLifetime)
		_sourceCallbackLifetime->Owner = nullptr;
	_sourceCallbackLifetime.reset();
	_sourceChanged.Disconnect();
}

bool CollectionViewSource::TryGetItem(
	size_t index,
	BindingSourceReference& out) const
{
	if (_sourcePassThrough)
		return _source && _source.Get()->TryGetItem(index, out);
	if (index >= _items.size()) return false;
	out = _items[index].Item;
	return static_cast<bool>(out);
}

bool CollectionViewSource::TryGetItemOccurrenceIdentity(
	size_t viewIndex, size_t& result) const noexcept
{
	result = 0;
	if (_sourcePassThrough)
	{
		if (_passThroughUsesGeneratedOccurrences)
			return TryGetPassThroughGeneratedOccurrenceIdentity(
				viewIndex, result);
		const auto* identities = _source
			? dynamic_cast<const IBindingListOccurrenceIdentity*>(_source.Get())
			: nullptr;
		if (!identities
			|| !identities->TryGetItemOccurrenceIdentity(viewIndex, result)
			|| result == 0) return false;
		CachePassThroughOccurrencePosition(result, viewIndex);
		return true;
	}
	if (viewIndex >= _items.size() || _items[viewIndex].Token == 0)
		return false;
	result = _items[viewIndex].Token;
	return true;
}

bool CollectionViewSource::TryGetItemIndexByOccurrenceIdentity(
	size_t identity, size_t& viewIndex) const noexcept
{
	viewIndex = 0;
	if (identity == 0) return false;
	if (_sourcePassThrough)
	{
		if (_passThroughUsesGeneratedOccurrences)
			return TryGetPassThroughGeneratedOccurrenceIndex(
				identity, viewIndex);
		const auto* identities = _source
			? dynamic_cast<const IBindingListOccurrenceIdentity*>(_source.Get())
			: nullptr;
		if (!identities) return false;
		const size_t count = _source.Get()->Count();
		if (const auto cached = _passThroughOccurrenceIndex.find(identity);
			cached != _passThroughOccurrenceIndex.end())
		{
			size_t candidate = 0;
			if (cached->second.Index < count
				&& identities->TryGetItemOccurrenceIdentity(
					cached->second.Index, candidate)
				&& candidate == identity)
			{
				viewIndex = cached->second.Index;
				CachePassThroughOccurrencePosition(identity, viewIndex);
				return true;
			}
			_passThroughOccurrenceRecency.erase(cached->second.Recency);
			_passThroughOccurrenceIndex.erase(cached);
		}
		const auto* lookup = _source
			? dynamic_cast<const IBindingListOccurrenceLookup*>(_source.Get())
			: nullptr;
		if (lookup)
		{
			size_t candidate = 0;
			if (lookup->TryGetItemIndexByOccurrenceIdentity(
					identity, candidate)
				&& candidate < count)
			{
				size_t confirmed = 0;
				if (identities->TryGetItemOccurrenceIdentity(candidate, confirmed)
					&& confirmed == identity)
				{
					viewIndex = candidate;
					CachePassThroughOccurrencePosition(identity, candidate);
					return true;
				}
			}
		}
		// The view advertises reverse lookup even when a legacy pass-through
		// source only exposes forward occurrence identities. Preserve exact
		// duplicate-item semantics with one historical scan, but retain only the
		// requested token so subsequent current/selection/UIA resolution is O(1)
		// without constructing a million-entry inverse table.
		for (size_t index = 0; index < count; ++index)
		{
			size_t candidate = 0;
			if (identities->TryGetItemOccurrenceIdentity(index, candidate)
				&& candidate == identity)
			{
				viewIndex = index;
				CachePassThroughOccurrencePosition(identity, index);
				return true;
			}
		}
		return false;
	}
	const auto found = _occurrenceIndex.find(identity);
	if (found == _occurrenceIndex.end() || found->second >= _items.size()
		|| _items[found->second].Token != identity) return false;
	viewIndex = found->second;
	return true;
}

bool CollectionViewSource::IsItemIndexByOccurrenceIdentityLookupBounded()
	const noexcept
{
	if (!_sourcePassThrough) return true;
	if (_passThroughUsesGeneratedOccurrences)
		return _passThroughGeneratedOccurrencesValid;
	const auto* lookup = _source
		? dynamic_cast<const IBindingListOccurrenceLookup*>(_source.Get())
		: nullptr;
	return lookup
		&& lookup->IsItemIndexByOccurrenceIdentityLookupBounded();
}

void CollectionViewSource::CachePassThroughOccurrencePosition(
	size_t identity, size_t index) const noexcept
{
	if (identity == 0) return;
	try
	{
		if (auto found = _passThroughOccurrenceIndex.find(identity);
			found != _passThroughOccurrenceIndex.end())
		{
			found->second.Index = index;
			_passThroughOccurrenceRecency.splice(
				_passThroughOccurrenceRecency.end(),
				_passThroughOccurrenceRecency, found->second.Recency);
			return;
		}
		_passThroughOccurrenceRecency.push_back(identity);
		const auto recency = std::prev(_passThroughOccurrenceRecency.end());
		try
		{
			_passThroughOccurrenceIndex.emplace(identity,
				PassThroughOccurrencePosition{ index, recency });
		}
		catch (...)
		{
			_passThroughOccurrenceRecency.pop_back();
			throw;
		}
		if (_passThroughOccurrenceIndex.size()
			> PassThroughOccurrenceCacheCapacity)
		{
			const size_t oldest = _passThroughOccurrenceRecency.front();
			_passThroughOccurrenceRecency.pop_front();
			_passThroughOccurrenceIndex.erase(oldest);
		}
	}
	catch (...)
	{
		// This cache is an optional acceleration for a noexcept identity API.
		// Allocation pressure must fall back to delegated lookup/linear scan,
		// never terminate an otherwise valid collection query.
	}
}

bool CollectionViewSource::InitializePassThroughGeneratedOccurrences(
	size_t count) noexcept
{
	try
	{
		auto candidate =
			std::make_shared<PassThroughGeneratedOccurrenceIndex>();
		if (count != 0)
		{
			const size_t tokenStart =
				AllocateCollectionOccurrenceIdentityRange(count);
			candidate->Initialize(count, tokenStart);
		}
		_passThroughGeneratedOccurrences.swap(candidate);
		_passThroughGeneratedOccurrencesValid = true;
		return true;
	}
	catch (...)
	{
		// The source has already committed by the time this can run from Changed.
		// Keep the live pass-through view usable and explicitly withdraw generated
		// identities instead of exposing a stale token table or blocking Changed.
		InvalidatePassThroughGeneratedOccurrences();
		return false;
	}
}

void CollectionViewSource::InvalidatePassThroughGeneratedOccurrences() noexcept
{
	_passThroughGeneratedOccurrences.reset();
	_passThroughGeneratedOccurrencesValid = false;
	// The source mutation is already committed, so its original notification
	// must still be forwarded.  Remember that a later explicit Refresh has to
	// publish the newly allocated token domain even when Count did not change.
	_passThroughGeneratedOccurrenceResetPending = true;
}

bool CollectionViewSource::TryGetPassThroughGeneratedOccurrenceIdentity(
	size_t index, size_t& identity) const noexcept
{
	identity = 0;
	if (!_passThroughGeneratedOccurrencesValid
		|| !_passThroughGeneratedOccurrences || !_source
		|| _passThroughGeneratedOccurrences->Size()
			!= _source.Get()->Count()) return false;
	return _passThroughGeneratedOccurrences->TryGetIdentity(index, identity);
}

bool CollectionViewSource::TryGetPassThroughGeneratedOccurrenceIndex(
	size_t identity, size_t& index) const noexcept
{
	index = 0;
	if (!_passThroughGeneratedOccurrencesValid
		|| !_passThroughGeneratedOccurrences || !_source
		|| _passThroughGeneratedOccurrences->Size()
			!= _source.Get()->Count()) return false;
	return _passThroughGeneratedOccurrences->TryGetIndex(identity, index);
}

void CollectionViewSource::ApplyPassThroughGeneratedOccurrenceChange(
	const CollectionChangedEventArgs& change) noexcept
{
	if (!_source || _source.Get()->Count() != change.NewSize)
	{
		InvalidatePassThroughGeneratedOccurrences();
		return;
	}
	if (change.Action == CollectionChangeAction::Reset)
	{
		if (InitializePassThroughGeneratedOccurrences(change.NewSize))
			_passThroughGeneratedOccurrenceResetPending = false;
		return;
	}
	if (!_passThroughGeneratedOccurrencesValid
		|| !_passThroughGeneratedOccurrences) return;

	try
	{
		// A stable snapshot retains this complete index version. Clone only on
		// the first subsequent live write; snapshot capture itself copies one
		// shared pointer and is therefore O(1), while the retained tree/map and
		// all of their parent links stay immutable.
		if (change.Action != CollectionChangeAction::Replace
			&& _passThroughGeneratedOccurrences.use_count() != 1)
			_passThroughGeneratedOccurrences =
				_passThroughGeneratedOccurrences->Clone();
		_passThroughGeneratedOccurrences->Apply(change);
	}
	catch (...)
	{
		InvalidatePassThroughGeneratedOccurrences();
	}
}

void CollectionViewSource::ApplyPassThroughOccurrenceChange(
	const CollectionChangedEventArgs& change) noexcept
{
	if (_passThroughUsesGeneratedOccurrences)
	{
		ApplyPassThroughGeneratedOccurrenceChange(change);
		return;
	}
	if (_passThroughOccurrenceIndex.empty()) return;
	auto invalidate = [this]() noexcept
	{
		_passThroughOccurrenceIndex.clear();
		_passThroughOccurrenceRecency.clear();
	};
	if (!_source || _source.Get()->Count() != change.NewSize)
	{
		invalidate();
		return;
	}
	for (const auto& [identity, position] : _passThroughOccurrenceIndex)
	{
		(void)identity;
		if (position.Index >= change.OldSize)
		{
			invalidate();
			return;
		}
	}
	auto validRange = [](size_t index, size_t count, size_t size) noexcept
	{
		return index != CollectionChangedEventArgs::Npos
			&& index <= size && count <= size - index;
	};

	switch (change.Action)
	{
	case CollectionChangeAction::Add:
		if (change.OldCount != 0 || change.NewCount == 0
			|| change.NewSize < change.NewCount
			|| change.OldSize != change.NewSize - change.NewCount
			|| change.NewIndex == CollectionChangedEventArgs::Npos
			|| change.NewIndex > change.OldSize)
		{
			invalidate();
			return;
		}
		for (auto& [identity, position] : _passThroughOccurrenceIndex)
		{
			(void)identity;
			if (position.Index >= change.NewIndex)
				position.Index += change.NewCount;
		}
		break;

	case CollectionChangeAction::Remove:
		if (change.NewCount != 0 || change.OldCount == 0
			|| !validRange(change.OldIndex, change.OldCount, change.OldSize)
			|| change.OldSize - change.OldCount != change.NewSize)
		{
			invalidate();
			return;
		}
		for (auto iterator = _passThroughOccurrenceIndex.begin();
			iterator != _passThroughOccurrenceIndex.end();)
		{
			auto& index = iterator->second.Index;
			if (index >= change.OldIndex
				&& index < change.OldIndex + change.OldCount)
			{
				_passThroughOccurrenceRecency.erase(iterator->second.Recency);
				iterator = _passThroughOccurrenceIndex.erase(iterator);
				continue;
			}
			if (index >= change.OldIndex + change.OldCount)
				index -= change.OldCount;
			++iterator;
		}
		break;

	case CollectionChangeAction::Move:
		if (change.OldSize != change.NewSize || change.OldCount == 0
			|| change.OldCount != change.NewCount
			|| !validRange(change.OldIndex, change.OldCount, change.OldSize)
			|| change.NewIndex == CollectionChangedEventArgs::Npos
			|| change.NewIndex > change.NewSize - change.NewCount)
		{
			invalidate();
			return;
		}
		for (auto& [identity, position] : _passThroughOccurrenceIndex)
		{
			(void)identity;
			auto& index = position.Index;
			if (index >= change.OldIndex
				&& index < change.OldIndex + change.OldCount)
			{
				index = change.NewIndex + (index - change.OldIndex);
				continue;
			}
			size_t afterRemoval = index;
			if (index >= change.OldIndex + change.OldCount)
				afterRemoval -= change.OldCount;
			index = afterRemoval >= change.NewIndex
				? afterRemoval + change.NewCount : afterRemoval;
		}
		break;

	case CollectionChangeAction::Swap:
		if (change.OldSize != change.NewSize
			|| change.OldCount != 1 || change.NewCount != 1
			|| change.OldIndex >= change.OldSize
			|| change.NewIndex >= change.NewSize)
		{
			invalidate();
			return;
		}
		for (auto& [identity, position] : _passThroughOccurrenceIndex)
		{
			(void)identity;
			auto& index = position.Index;
			if (index == change.OldIndex) index = change.NewIndex;
			else if (index == change.NewIndex) index = change.OldIndex;
		}
		break;

	case CollectionChangeAction::Replace:
		if (change.OldSize != change.NewSize || change.OldCount == 0
			|| change.OldCount != change.NewCount
			|| change.OldIndex != change.NewIndex
			|| !validRange(change.OldIndex, change.OldCount, change.OldSize))
		{
			invalidate();
			return;
		}
		// Occurrence identity belongs to the physical slot and survives Replace.
		break;

	case CollectionChangeAction::Reset:
	default:
		invalidate();
		return;
	}

	const auto* identities = dynamic_cast<
		const IBindingListOccurrenceIdentity*>(_source.Get());
	if (!identities)
	{
		invalidate();
		return;
	}
	for (auto iterator = _passThroughOccurrenceIndex.begin();
		iterator != _passThroughOccurrenceIndex.end();)
	{
		size_t candidate = 0;
		if (iterator->second.Index >= change.NewSize
			|| !identities->TryGetItemOccurrenceIdentity(
				iterator->second.Index, candidate)
			|| candidate != iterator->first)
		{
			_passThroughOccurrenceRecency.erase(iterator->second.Recency);
			iterator = _passThroughOccurrenceIndex.erase(iterator);
			continue;
		}
		++iterator;
	}
}

bool CollectionViewSource::TryGetStableSnapshot(
	BindingListReference& result) const
{
	result = {};
	if (!_sourcePassThrough || !_source || !_groups.empty()) return false;
	const bool generatedOccurrences = _passThroughUsesGeneratedOccurrences;
	std::shared_ptr<const PassThroughGeneratedOccurrenceIndex>
		generatedVersion;
	if (generatedOccurrences)
	{
		if (!_passThroughGeneratedOccurrencesValid
			|| !_passThroughGeneratedOccurrences
			|| _passThroughGeneratedOccurrences->Size() != Count()) return false;
		// Retaining the version before the source snapshot callback serves two
		// purposes: it is the O(1) capture, and it forces a reentrant live mutation
		// to COW. Pointer equality below then proves that the immutable item version
		// and token version were observed in the same source revision.
		generatedVersion = _passThroughGeneratedOccurrences;
	}
	else if (dynamic_cast<const IBindingListOccurrenceIdentity*>(
		_source.Get()) == nullptr) return false;
	BindingListReference stable;
	if (dynamic_cast<const IBindingListStableSnapshot*>(_source.Get()))
		stable = _source;
	else if (const auto* provider = dynamic_cast<
		const IBindingListSnapshotProvider*>(_source.Get()))
	{
		if (!provider->TryGetStableSnapshot(stable)) return false;
	}
	else return false;
	if (!stable
		|| dynamic_cast<const IBindingListStableSnapshot*>(stable.Get()) == nullptr
		|| stable.Get()->Count() != Count()
		|| stable.Get()->GetItemTypeToken() != GetItemTypeToken()) return false;
	if (generatedOccurrences)
	{
		if (!_sourcePassThrough || !_passThroughUsesGeneratedOccurrences
			|| !_passThroughGeneratedOccurrencesValid
			|| _passThroughGeneratedOccurrences != generatedVersion
			|| generatedVersion->Size() != stable.Get()->Count()) return false;
		// Both the underlying immutable item version and the complete generated
		// token index are retained by shared ownership. No row/run traversal occurs
		// on this capture path.
		result = BindingListReference(std::make_shared<
			PassThroughGeneratedStableSnapshot>(
				std::move(stable), std::move(generatedVersion)));
		return true;
	}
	if (dynamic_cast<const IBindingListOccurrenceIdentity*>(stable.Get())
		== nullptr) return false;
	result = BindingListReference(std::make_shared<PassThroughStableSnapshot>(
		std::move(stable)));
	return true;
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

bool CollectionViewSource::CanUseSourcePassThrough() const noexcept
{
	return _source
		&& !_filterPredicate
		&& _filterDescriptions.empty()
		&& _sortDescriptions.empty()
		&& _groupDescriptions.empty()
		&& _aggregateDescriptions.empty();
}

void CollectionViewSource::ClearItemObservations() noexcept
{
	_itemObservations.clear();
	_observedCompiledPaths.clear();
#if CUI_ENABLE_DYNAMIC_XAML
	_observedAuthoredPaths.clear();
#endif
	_observedSource = nullptr;
	_observedSourceStructureRevision = 0;
	_itemObservationsDirty = true;
}

void CollectionViewSource::SetResolvedSource(BindingListReference value)
{
	if (_source == value) return;
	const bool wasPassThrough = _sourcePassThrough;
	const bool replacingExistingSource = static_cast<bool>(_source);
	_publishedCount = Count();
	_sourceChanged.Disconnect();
	_pendingSourceChanges.clear();
	_source = std::move(value);
	_occurrenceIndex.clear();
	_passThroughOccurrenceIndex.clear();
	_passThroughOccurrenceRecency.clear();
	_passThroughGeneratedOccurrences.reset();
	_passThroughUsesGeneratedOccurrences = false;
	_passThroughGeneratedOccurrencesValid = false;
	_passThroughGeneratedOccurrenceResetPending = false;
	if (++_sourceConnectionRevision == 0) ++_sourceConnectionRevision;
	if (++_sourceStructureRevision == 0) ++_sourceStructureRevision;
	_itemObservationsDirty = true;
	const bool willPassThrough = CanUseSourcePassThrough();
	if (willPassThrough)
		_sourceSlots.clear();
	else
		RebuildSourceSlots(_source ? _source.Get()->Count() : 0);
	if (wasPassThrough || willPassThrough || replacingExistingSource)
		_resetProjectionOnNextRefresh = true;
	IBindingList* const subscribedSource = _source.Get();
	const size_t subscribedRevision = _sourceConnectionRevision;
	if (_source)
	{
		const std::weak_ptr<CollectionViewSource> weakSelf = weak_from_this();
		const std::weak_ptr<SourceCallbackLifetime> weakLifetime =
			_sourceCallbackLifetime;
		EventConnection candidate = _source.Get()->SubscribeChanged(
			[weakSelf, weakLifetime, subscribedSource, subscribedRevision](
				const CollectionChangedEventArgs& change)
			{
				// Shared-owned views are pinned across the full notification. The
				// lifetime state preserves legacy stack-owned use without capturing a
				// raw this in a source Event handler snapshot.
				if (const auto self = weakSelf.lock())
				{
					if (!self->IsCurrentSourceConnection(
						subscribedSource, subscribedRevision)) return;
					self->QueueSourceChanged(
						change, subscribedSource, subscribedRevision);
					return;
				}
				const auto lifetime = weakLifetime.lock();
				auto* const owner = lifetime ? lifetime->Owner : nullptr;
				if (!owner || !owner->IsCurrentSourceConnection(
					subscribedSource, subscribedRevision)) return;
				owner->QueueSourceChanged(
					change, subscribedSource, subscribedRevision);
			});
		// SubscribeChanged is application-extensible. It may synchronously call
		// SetSource and install a newer connection before returning. Never let the
		// stale outer subscription overwrite that newer connection.
		if (!IsCurrentSourceConnection(
			subscribedSource, subscribedRevision)) return;
		_sourceChanged = std::move(candidate);
	}
	if (!IsCurrentSourceConnection(subscribedSource, subscribedRevision)) return;
	Refresh();
}

CollectionViewSource::SourceSlotIdentity
CollectionViewSource::CreateSourceSlotIdentity()
{
	SourceSlotIdentity result;
	result.Token = AllocateCollectionOccurrenceIdentity();
	result.Revision = CreateSourceSlotRevision();
	return result;
}

size_t CollectionViewSource::CreateSourceSlotRevision()
{
	if (_nextSourceSlotRevision == 0
		|| _nextSourceSlotRevision
			== (std::numeric_limits<size_t>::max)())
		throw std::overflow_error(
			"CollectionViewSource occurrence revision exhausted");
	return _nextSourceSlotRevision++;
}

void CollectionViewSource::RebuildSourceSlots(size_t count)
{
	const BindingListReference sourceReference = _source;
	const size_t sourceRevision = _sourceStructureRevision;
	const auto sourceIsCurrent = [&]() noexcept
	{
		return _source == sourceReference
			&& _sourceStructureRevision == sourceRevision;
	};
	std::vector<SourceSlotIdentity> rebuilt;
	rebuilt.reserve(count);
	const auto* sourceIdentities = sourceReference
		? dynamic_cast<const IBindingListOccurrenceIdentity*>(
			sourceReference.Get())
		: nullptr;
	if (sourceIdentities)
	{
		for (size_t index = 0; index < count; ++index)
		{
			size_t token = 0;
			const bool found = sourceIdentities->TryGetItemOccurrenceIdentity(
				index, token);
			if (!sourceIsCurrent()) return;
			if (!found
				|| token == 0)
			{
				rebuilt.clear();
				break;
			}
			rebuilt.push_back(SourceSlotIdentity{
				token, CreateSourceSlotRevision() });
		}
		if (rebuilt.size() == count)
		{
			if (!sourceIsCurrent()) return;
			_sourceSlots = std::move(rebuilt);
			return;
		}
	}
	rebuilt.clear();
	rebuilt.reserve(count);
	for (size_t index = 0; index < count; ++index)
		rebuilt.push_back(CreateSourceSlotIdentity());
	if (!sourceIsCurrent()) return;
	_sourceSlots = std::move(rebuilt);
}

bool CollectionViewSource::IsCurrentSourceConnection(
	const IBindingList* source,
	size_t connectionRevision) const noexcept
{
	return _source.Get() == source
		&& _sourceConnectionRevision == connectionRevision;
}

void CollectionViewSource::QueueSourceChanged(
	const CollectionChangedEventArgs& change,
	IBindingList* source,
	size_t connectionRevision)
{
	if (!IsCurrentSourceConnection(source, connectionRevision)) return;
	_pendingSourceChanges.push_back(PendingSourceChange{
		change, source, connectionRevision });
	if (_drainingSourceChanges) return;
	_drainingSourceChanges = true;
	try
	{
		while (!_pendingSourceChanges.empty())
		{
			const PendingSourceChange pending =
				std::move(_pendingSourceChanges.front());
			_pendingSourceChanges.pop_front();
			if (!IsCurrentSourceConnection(
				pending.Source, pending.ConnectionRevision)) continue;
			HandleSourceChanged(
				pending.Change, pending.Source, pending.ConnectionRevision);
		}
	}
	catch (...)
	{
		_drainingSourceChanges = false;
		throw;
	}
	_drainingSourceChanges = false;
}

void CollectionViewSource::HandleSourceChanged(
	const CollectionChangedEventArgs& change,
	IBindingList* source,
	size_t connectionRevision)
{
	if (!IsCurrentSourceConnection(source, connectionRevision)) return;
	// A Changed subscriber may detach the last external owner while this view is
	// still publishing its projection. Keep a shared-owned view alive until the
	// complete source-notification stack unwinds; a stack-owned view naturally
	// yields an empty weak_from_this() and already has lexical lifetime.
	const auto selfLifetime = weak_from_this().lock();
	(void)selfLifetime;
	if (_sourcePassThrough && CanUseSourcePassThrough())
	{
		ApplyPassThroughOccurrenceChange(change);
		if (!IsCurrentSourceConnection(source, connectionRevision)) return;
		_publishedCount = change.NewSize;
		cui::framework::EventAccess::Raise(_changed, this, change);
		if (!IsCurrentSourceConnection(source, connectionRevision)) return;
		RestoreCurrentItem();
		return;
	}
	if (change.Action != CollectionChangeAction::Move
		&& change.Action != CollectionChangeAction::Swap)
	{
		if (++_sourceStructureRevision == 0) ++_sourceStructureRevision;
		_itemObservationsDirty = true;
	}
	ApplySourceChange(change);
	if (!IsCurrentSourceConnection(source, connectionRevision)) return;
	// A source Reset already declares that fine-grained source deltas are not
	// available.  Views such as DataGrid that request an atomic complex refresh
	// must preserve that boundary instead of expanding a million-row reset into
	// one projection notification per occurrence.
	RefreshProjection(_useResetNotificationForComplexRefresh
		&& change.Action == CollectionChangeAction::Reset);
}

void CollectionViewSource::ApplySourceChange(
	const CollectionChangedEventArgs& change)
{
	const size_t actualSize = _source ? _source.Get()->Count() : 0;
	auto rebuild = [this, actualSize]
	{
		RebuildSourceSlots(actualSize);
	};
	if (_sourceSlots.size() != change.OldSize
		|| actualSize != change.NewSize)
	{
		rebuild();
		return;
	}
	// Reset retires the complete slot table.  Avoid first copying that table into
	// the transactional candidate only to discard it immediately below.
	if (change.Action == CollectionChangeAction::Reset)
	{
		rebuild();
		return;
	}

	auto candidate = _sourceSlots;
	auto validRange = [](size_t index, size_t count, size_t size) noexcept
	{
		return index <= size && count <= size - index;
	};
	auto makeSlots = [this](size_t start, size_t count,
		std::vector<SourceSlotIdentity>& result)
	{
		result.clear();
		result.reserve(count);
		const auto* sourceIdentities = _source
			? dynamic_cast<const IBindingListOccurrenceIdentity*>(_source.Get())
			: nullptr;
		for (size_t index = 0; index < count; ++index)
		{
			if (!sourceIdentities)
			{
				result.push_back(CreateSourceSlotIdentity());
				continue;
			}
			size_t token = 0;
			if (!sourceIdentities->TryGetItemOccurrenceIdentity(
				start + index, token) || token == 0) return false;
			result.push_back(SourceSlotIdentity{
				token, CreateSourceSlotRevision() });
		}
		return true;
	};

	switch (change.Action)
	{
	case CollectionChangeAction::Add:
		if (change.OldCount != 0 || change.NewCount == 0
			|| change.NewIndex == CollectionChangedEventArgs::Npos
			|| change.NewIndex > candidate.size()
			|| actualSize < candidate.size()
			|| change.NewCount != actualSize - candidate.size())
		{
			rebuild();
			return;
		}
		{
			std::vector<SourceSlotIdentity> additions;
			if (!makeSlots(change.NewIndex, change.NewCount, additions))
			{
				rebuild();
				return;
			}
			candidate.insert(
				candidate.begin() + change.NewIndex,
				std::make_move_iterator(additions.begin()),
				std::make_move_iterator(additions.end()));
		}
		break;

	case CollectionChangeAction::Remove:
		if (change.NewCount != 0 || change.OldCount == 0
			|| change.OldIndex == CollectionChangedEventArgs::Npos
			|| !validRange(
				change.OldIndex, change.OldCount, candidate.size())
			|| candidate.size() - change.OldCount != actualSize)
		{
			rebuild();
			return;
		}
		candidate.erase(
			candidate.begin() + change.OldIndex,
			candidate.begin() + change.OldIndex + change.OldCount);
		break;

	case CollectionChangeAction::Move:
		if (change.OldCount == 0
			|| change.OldCount != change.NewCount
			|| change.OldIndex == CollectionChangedEventArgs::Npos
			|| change.NewIndex == CollectionChangedEventArgs::Npos
			|| actualSize != candidate.size()
			|| !validRange(
				change.OldIndex, change.OldCount, candidate.size())
			|| change.NewIndex > candidate.size() - change.OldCount)
		{
			rebuild();
			return;
		}
		{
			std::vector<SourceSlotIdentity> moved(
				candidate.begin() + change.OldIndex,
				candidate.begin() + change.OldIndex + change.OldCount);
			candidate.erase(
				candidate.begin() + change.OldIndex,
				candidate.begin() + change.OldIndex + change.OldCount);
			candidate.insert(
				candidate.begin() + change.NewIndex,
				std::make_move_iterator(moved.begin()),
				std::make_move_iterator(moved.end()));
		}
		break;

	case CollectionChangeAction::Swap:
		if (change.OldCount != 1 || change.NewCount != 1
			|| change.OldIndex >= candidate.size()
			|| change.NewIndex >= candidate.size()
			|| actualSize != candidate.size())
		{
			rebuild();
			return;
		}
		std::swap(candidate[change.OldIndex], candidate[change.NewIndex]);
		break;

	case CollectionChangeAction::Replace:
		if (change.OldIndex == CollectionChangedEventArgs::Npos
			|| change.NewIndex == CollectionChangedEventArgs::Npos
			|| change.OldIndex != change.NewIndex
			|| change.OldCount == 0
			|| change.OldCount != change.NewCount
			|| !validRange(
				change.OldIndex, change.OldCount, candidate.size())
			|| candidate.size() != actualSize)
		{
			rebuild();
			return;
		}
		{
			std::vector<SourceSlotIdentity> replaced(
				candidate.begin() + change.OldIndex,
				candidate.begin() + change.OldIndex + change.OldCount);
			candidate.erase(
				candidate.begin() + change.OldIndex,
				candidate.begin() + change.OldIndex + change.OldCount);
			if (change.NewIndex > candidate.size())
			{
				rebuild();
				return;
			}
			std::vector<SourceSlotIdentity> replacements;
			replacements.reserve(change.NewCount);
			for (size_t index = 0; index < change.NewCount; ++index)
			{
				auto identity = replaced[index];
				identity.Revision = CreateSourceSlotRevision();
				replacements.push_back(identity);
			}
			candidate.insert(
				candidate.begin() + change.NewIndex,
				std::make_move_iterator(replacements.begin()),
				std::make_move_iterator(replacements.end()));
		}
		break;

	default:
		rebuild();
		return;
	}

	if (candidate.size() != actualSize)
	{
		rebuild();
		return;
	}
	_sourceSlots = std::move(candidate);
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
	_itemObservationsDirty = true;
	if (_useResetNotificationForComplexRefresh)
		_resetProjectionOnNextRefresh = true;
	Refresh();
}

void CollectionViewSource::SetLiveShapingRequest(bool& target, bool value)
{
	if (target == value) return;
	target = value;
	_itemObservationsDirty = true;
	// A property may have changed while live shaping was disabled. Rebuild the
	// projection before publishing the new live state so enabling the request
	// cannot leave stale sort/filter/group results until the next notification.
	Refresh();
}

void CollectionViewSource::SetIsLiveSortingRequested(bool value)
{
	SetLiveShapingRequest(_isLiveSortingRequested, value);
}

void CollectionViewSource::SetIsLiveFilteringRequested(bool value)
{
	SetLiveShapingRequest(_isLiveFilteringRequested, value);
}

void CollectionViewSource::SetIsLiveGroupingRequested(bool value)
{
	SetLiveShapingRequest(_isLiveGroupingRequested, value);
}

void CollectionViewSource::SetFilterDescriptions(
	std::vector<CollectionFilterDescription> value)
{
	_filterDescriptions = std::move(value);
	_itemObservationsDirty = true;
	Refresh();
}

void CollectionViewSource::SetGroupDescriptions(
	std::vector<CollectionGroupDescription> value)
{
	if (_groupDescriptions == value) return;
	_groupDescriptions = std::move(value);
	_itemObservationsDirty = true;
	Refresh();
}

void CollectionViewSource::SetAggregateDescriptions(
	std::vector<CollectionAggregateDescription> value)
{
	if (_aggregateDescriptions == value) return;
	_aggregateDescriptions = std::move(value);
	_itemObservationsDirty = true;
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
	const BindingSourceReference& item,
	const FilterPredicate& predicate,
	std::span<const CollectionFilterDescription> descriptions) const
{
	if (!item || (predicate && !predicate(item))) return false;
	for (const auto& filter : descriptions)
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
		if (filter.Operator == CollectionFilterOperator::Contains
			|| filter.Operator == CollectionFilterOperator::StartsWith
			|| filter.Operator == CollectionFilterOperator::EndsWith)
		{
			if (!MatchesStringFilter(
				candidate, filter.Value, filter.Operator,
				filter.IgnoreCase)) return false;
			continue;
		}
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
		default:
			break;
		}
	}
	return true;
}

void CollectionViewSource::Refresh()
{
	RefreshProjection(_useResetNotificationForComplexRefresh);
}

void CollectionViewSource::RefreshProjection(bool publishComplexReset)
{
	if (publishComplexReset)
		_resetProjectionOnNextRefresh = true;
	// Refresh can publish through sorting/filtering, item observation, or a
	// source notification. Any subscriber may detach the last external owner;
	// pin shared-owned views until the complete publish loop unwinds.
	const auto selfLifetime = weak_from_this().lock();
	(void)selfLifetime;
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
	if (CanUseSourcePassThrough())
	{
		const BindingListReference sourceReference = _source;
		const bool modeChanged = !_sourcePassThrough;
		const size_t oldCount = _publishedCount;
		const bool groupsChanged = !_groups.empty();
		_sourcePassThrough = true;
		_sourceSlots.clear();
		_items.clear();
		if (modeChanged)
		{
			_occurrenceIndex.clear();
			_passThroughOccurrenceIndex.clear();
			_passThroughOccurrenceRecency.clear();
		}
		_groups.clear();
		ClearItemObservations();
		const size_t newCount = sourceReference.Get()->Count();
		const bool generateOccurrences = dynamic_cast<const
			IBindingListOccurrenceIdentity*>(sourceReference.Get()) == nullptr;
		const bool generatedOccurrenceResetPending =
			_passThroughGeneratedOccurrenceResetPending;
		if (generateOccurrences)
		{
			const size_t representedCount = _passThroughGeneratedOccurrences
				? _passThroughGeneratedOccurrences->Size() : 0;
			if (!_passThroughUsesGeneratedOccurrences
				|| !_passThroughGeneratedOccurrencesValid
				|| representedCount != newCount)
				InitializePassThroughGeneratedOccurrences(newCount);
		}
		else
		{
			_passThroughGeneratedOccurrences.reset();
			_passThroughGeneratedOccurrencesValid = false;
			_passThroughGeneratedOccurrenceResetPending = false;
		}
		_passThroughUsesGeneratedOccurrences = generateOccurrences;
		if (_refreshPending) return;
		const bool resetRequested = std::exchange(
			_resetProjectionOnNextRefresh, false);
		const bool publishReset = modeChanged
			|| resetRequested
			|| generatedOccurrenceResetPending
			|| oldCount != newCount;
		_publishedCount = newCount;
		if (publishReset)
		{
			const CollectionChangedEventArgs change{
				CollectionChangeAction::Reset,
				CollectionChangedEventArgs::Npos,
				CollectionChangedEventArgs::Npos,
				oldCount, newCount, oldCount, newCount };
			cui::framework::EventAccess::Raise(_changed, this, change);
		}
		if (publishReset && _passThroughGeneratedOccurrencesValid)
			_passThroughGeneratedOccurrenceResetPending = false;
		RestoreCurrentItem();
		if (groupsChanged)
			cui::framework::EventAccess::Raise(_groupsChanged, this);
		return;
	}
	// Every shaping callback below is application-extensible. Snapshot the
	// transaction inputs so a predicate/property getter can request a nested
	// Refresh without invalidating the vector or std::function currently being
	// evaluated. The outer loop discards the stale target before publication.
	const BindingListReference sourceReference = _source;
	const auto filterPredicate = _filterPredicate;
	const auto filterDescriptions = _filterDescriptions;
	const auto groupDescriptions = _groupDescriptions;
	const auto sortDescriptions = _sortDescriptions;
	const auto aggregateDescriptions = _aggregateDescriptions;
	if (_refreshPending) return;
	bool hasSourceOrderBaseline = false;
	if (_sourcePassThrough)
	{
		_sourcePassThrough = false;
		_items.clear();
		_occurrenceIndex.clear();
		_passThroughOccurrenceIndex.clear();
		_passThroughOccurrenceRecency.clear();
		_passThroughGeneratedOccurrences.reset();
		_passThroughUsesGeneratedOccurrences = false;
		_passThroughGeneratedOccurrencesValid = false;
		_passThroughGeneratedOccurrenceResetPending = false;
		_sourceSlots.clear();
		ClearItemObservations();
		if (!_resetProjectionOnNextRefresh && sourceReference)
		{
			auto* const source = sourceReference.Get();
			const size_t sourceCount = source->Count();
			if (_refreshPending) return;
			RebuildSourceSlots(sourceCount);
			_items.reserve(sourceCount);
			for (size_t index = 0; index < sourceCount; ++index)
			{
				const auto identity = _sourceSlots[index];
				BindingSourceReference item;
				const bool read = source->TryGetItem(index, item);
				if (_refreshPending) return;
				if (!read || !item)
				{
					_items.clear();
					_resetProjectionOnNextRefresh = true;
					break;
				}
				_items.push_back(ProjectionItem{
					std::move(item), identity.Token, identity.Revision });
			}
			hasSourceOrderBaseline = _items.size() == sourceCount;
			if (hasSourceOrderBaseline) RebuildOccurrenceIndex();
		}
	}
	std::vector<ProjectionItem> target;
	if (sourceReference)
	{
		auto* const source = sourceReference.Get();
		const size_t sourceCount = source->Count();
		if (_refreshPending) return;
		// A well-formed source notification keeps the identity table in lockstep.
		// Still repair a count mismatch here so an explicit Refresh() after a
		// malformed or missed notification cannot publish identity-less items.
		if (_sourceSlots.size() != sourceCount)
			RebuildSourceSlots(sourceCount);
		target.reserve(sourceCount);
		if (hasSourceOrderBaseline)
		{
			for (const auto& item : _items)
			{
				const bool included = PassesFilters(
					item.Item, filterPredicate, filterDescriptions);
				if (_refreshPending) return;
				if (included) target.push_back(item);
			}
		}
		else
		{
			for (size_t index = 0; index < sourceCount; ++index)
			{
				const auto identity = _sourceSlots[index];
				BindingSourceReference item;
				const bool read = source->TryGetItem(index, item);
				const bool included = read && PassesFilters(
					item, filterPredicate, filterDescriptions);
				if (_refreshPending) return;
				if (included)
					target.push_back(ProjectionItem{
						std::move(item), identity.Token, identity.Revision });
			}
		}
	}
	std::vector<BindingValue> groupKeys;
	if (!groupDescriptions.empty() || !sortDescriptions.empty())
	{
		// A binding path can be arbitrarily expensive (and user-extensible).
		// Reading both operands inside stable_sort's comparator amplified one
		// million rows into tens of millions of path evaluations. Decorate every
		// projected occurrence once, then compare only the captured values.
		// Snapshot the descriptions as well: a property getter may re-enter
		// Refresh() and replace the live vectors while this pass is in progress.
		const size_t groupKeyCount = groupDescriptions.size();
		const size_t sortKeyCount = sortDescriptions.size();

		struct DecoratedSortKey final
		{
			BindingValue Value;
			bool Found = false;
		};
		if (groupKeyCount != 0 && target.size()
			> (std::numeric_limits<size_t>::max)() / groupKeyCount)
			throw std::length_error("CollectionViewSource group key table overflow");
		if (sortKeyCount != 0 && target.size()
			> (std::numeric_limits<size_t>::max)() / sortKeyCount)
			throw std::length_error("CollectionViewSource sort key table overflow");
		// Group keys outlive sorting because BuildGroups consumes the exact values
		// that established group order. Keep them separate from the disposable
		// non-group sort decoration from the beginning: the former implementation
		// first retained one wide table and then allocated a second N x group-key
		// table, making both large payload sets overlap at million-row scale.
		groupKeys.resize(target.size() * groupKeyCount);
		std::vector<unsigned char> groupKeysFound(groupKeys.size(), 0);
		std::vector<DecoratedSortKey> sortKeys(
			target.size() * sortKeyCount);
		for (size_t itemIndex = 0; itemIndex < target.size(); ++itemIndex)
		{
			auto& item = target[itemIndex].Item;
			const size_t groupBase = itemIndex * groupKeyCount;
			for (size_t keyIndex = 0; keyIndex < groupKeyCount; ++keyIndex)
			{
				auto& key = groupKeys[groupBase + keyIndex];
				groupKeysFound[groupBase + keyIndex] = static_cast<unsigned char>(
					item && TryGetDescriptionValue(
						*item.Get(), groupDescriptions[keyIndex], key));
			}
			for (size_t sortIndex = 0;
				sortIndex < sortKeyCount; ++sortIndex)
			{
				auto& key = sortKeys[itemIndex * sortKeyCount + sortIndex];
				key.Found = item && TryGetDescriptionValue(
					*item.Get(), sortDescriptions[sortIndex], key.Value);
			}
		}
		// A source/property callback requested another refresh while keys were
		// captured. Discard this stale transaction; Refresh()'s outer loop will
		// rebuild it against the new source/descriptions without publishing a
		// projection sorted by a mixture of old and new state.
		if (_refreshPending) return;

		std::vector<size_t> order(target.size());
		std::iota(order.begin(), order.end(), size_t{ 0 });
		std::stable_sort(order.begin(), order.end(),
			[&](size_t leftIndex, size_t rightIndex)
			{
				auto compareGroup = [&](size_t keyIndex, bool ignoreCase)
				{
					const size_t leftKey = leftIndex * groupKeyCount + keyIndex;
					const size_t rightKey = rightIndex * groupKeyCount + keyIndex;
					const bool leftFound = groupKeysFound[leftKey] != 0;
					const bool rightFound = groupKeysFound[rightKey] != 0;
					if (!leftFound || !rightFound)
						return leftFound == rightFound
							? 0 : leftFound ? 1 : -1;
					return CompareValues(
						groupKeys[leftKey], groupKeys[rightKey], ignoreCase);
				};
				auto compareSort = [&](size_t keyIndex, bool ignoreCase)
				{
					const auto& left = sortKeys[
						leftIndex * sortKeyCount + keyIndex];
					const auto& right = sortKeys[
						rightIndex * sortKeyCount + keyIndex];
					if (!left.Found || !right.Found)
						return left.Found == right.Found
							? 0 : left.Found ? 1 : -1;
					return CompareValues(left.Value, right.Value, ignoreCase);
				};
				for (size_t keyIndex = 0;
					keyIndex < groupKeyCount; ++keyIndex)
				{
					const auto& group = groupDescriptions[keyIndex];
					const int comparison = compareGroup(
						keyIndex, group.IgnoreCase);
					if (comparison == 0) continue;
					return group.Direction == CollectionSortDirection::Ascending
						? comparison < 0 : comparison > 0;
				}
				for (size_t sortIndex = 0; sortIndex < sortKeyCount; ++sortIndex)
				{
					const auto& sort = sortDescriptions[sortIndex];
					const int comparison = compareSort(
						sortIndex, sort.IgnoreCase);
					if (comparison == 0) continue;
					return sort.Direction == CollectionSortDirection::Ascending
						? comparison < 0 : comparison > 0;
				}
				return false;
			});
		// The non-group decoration is no longer needed once the index order is
		// known. Release it before permuting the projection/group rows in place.
		std::vector<DecoratedSortKey>().swap(sortKeys);
		if (_refreshPending) return;

		// order[newIndex] is the corresponding old index. Apply its cycles in
		// place to both target and the group-key rows. This avoids a third N-sized
		// ProjectionItem buffer and a second N x group-key payload table.
		std::vector<BindingValue> displacedGroupKeys(groupKeyCount);
		std::vector<unsigned char> displacedGroupFound(groupKeyCount);
		for (size_t first = 0; first < order.size(); ++first)
		{
			if (order[first] == first) continue;
			auto displaced = std::move(target[first]);
			for (size_t key = 0; key < groupKeyCount; ++key)
			{
				displacedGroupKeys[key] = std::move(
					groupKeys[first * groupKeyCount + key]);
				displacedGroupFound[key] =
					groupKeysFound[first * groupKeyCount + key];
			}
			size_t current = first;
			while (order[current] != first)
			{
				const size_t next = order[current];
				target[current] = std::move(target[next]);
				for (size_t key = 0; key < groupKeyCount; ++key)
				{
					groupKeys[current * groupKeyCount + key] = std::move(
						groupKeys[next * groupKeyCount + key]);
					groupKeysFound[current * groupKeyCount + key] =
						groupKeysFound[next * groupKeyCount + key];
				}
				order[current] = current;
				current = next;
			}
			target[current] = std::move(displaced);
			for (size_t key = 0; key < groupKeyCount; ++key)
			{
				groupKeys[current * groupKeyCount + key] = std::move(
					displacedGroupKeys[key]);
				groupKeysFound[current * groupKeyCount + key] =
					displacedGroupFound[key];
			}
			order[current] = current;
		}
		// Found/missing was needed only by sorting. BuildGroups intentionally
		// groups both a missing path and an explicit empty BindingValue together,
		// matching the previous cache semantics.
		std::vector<unsigned char>().swap(groupKeysFound);
	}
	auto groups = BuildGroups(
		target, groupDescriptions, aggregateDescriptions, groupKeys);
	if (_refreshPending) return;
	const bool groupsChanged = !SameGroups(_groups, groups);
	_groups = std::move(groups);
	if (_refreshPending) return;
	const bool publishReset = std::exchange(
		_resetProjectionOnNextRefresh, false);
	PublishProjection(std::move(target), publishReset);
	RebuildItemObservations();
	RestoreCurrentItem();
	if (groupsChanged)
		cui::framework::EventAccess::Raise(_groupsChanged, this);
}

std::vector<BindingListGroup> CollectionViewSource::BuildGroups(
	const std::vector<ProjectionItem>& items,
	std::span<const CollectionGroupDescription> descriptions,
	std::span<const CollectionAggregateDescription> aggregates,
	std::span<const BindingValue> groupKeys) const
{
	std::vector<BindingListGroup> result;
	if (descriptions.empty() || items.empty()) return result;
	if (items.size() > (std::numeric_limits<size_t>::max)()
			/ descriptions.size()
		|| groupKeys.size() != items.size() * descriptions.size())
		throw std::logic_error(
			"CollectionViewSource group key cache is inconsistent");
	const auto keyAt = [&](size_t item, size_t level)
		-> const BindingValue&
	{
		return groupKeys[item * descriptions.size() + level];
	};
	struct AggregatePathPlan final
	{
		size_t ReaderIndex = 0;
		bool NeedsNumeric = false;
		bool NeedsMin = false;
		bool NeedsMax = false;
	};
	const auto sameAggregatePath = [](
		const CollectionAggregateDescription& left,
		const CollectionAggregateDescription& right) noexcept
	{
		if (!left.CompiledPath.Empty() || !right.CompiledPath.Empty())
			return !left.CompiledPath.Empty() && !right.CompiledPath.Empty()
				&& SameCompiledCollectionPath(
					left.CompiledPath, right.CompiledPath);
#if CUI_ENABLE_DYNAMIC_XAML
		return left.PropertyName == right.PropertyName;
#else
		return true;
#endif
	};
	std::vector<AggregatePathPlan> aggregatePaths;
	std::vector<size_t> aggregatePathIndices(
		aggregates.size(), CollectionChangedEventArgs::Npos);
	for (size_t aggregateIndex = 0;
		aggregateIndex < aggregates.size(); ++aggregateIndex)
	{
		const auto& aggregate = aggregates[aggregateIndex];
		if (aggregate.Function == CollectionAggregateFunction::Count
			|| !HasDescriptionPath(aggregate)) continue;
		auto path = std::find_if(
			aggregatePaths.begin(), aggregatePaths.end(),
			[&](const auto& candidate)
			{
				return sameAggregatePath(
					aggregates[candidate.ReaderIndex], aggregate);
			});
		if (path == aggregatePaths.end())
		{
			aggregatePaths.push_back({ aggregateIndex });
			path = aggregatePaths.end() - 1;
		}
		aggregatePathIndices[aggregateIndex] = static_cast<size_t>(
			std::distance(aggregatePaths.begin(), path));
		path->NeedsNumeric = path->NeedsNumeric
			|| aggregate.Function == CollectionAggregateFunction::Sum
			|| aggregate.Function == CollectionAggregateFunction::Average;
		path->NeedsMin = path->NeedsMin
			|| aggregate.Function == CollectionAggregateFunction::Min;
		path->NeedsMax = path->NeedsMax
			|| aggregate.Function == CollectionAggregateFunction::Max;
	}
	bool staleGeneration = false;
	auto buildAggregates = [&](size_t begin, size_t end)
	{
		struct AggregatePathState final
		{
			double Sum = 0.0;
			size_t NumericCount = 0;
			BindingValue Min;
			BindingValue Max;
		};
		std::vector<AggregatePathState> states(aggregatePaths.size());
		for (size_t pathIndex = 0;
			pathIndex < aggregatePaths.size(); ++pathIndex)
		{
			const auto& path = aggregatePaths[pathIndex];
			auto& state = states[pathIndex];
			const auto& reader = aggregates[path.ReaderIndex];
			for (size_t index = begin; index < end; ++index)
			{
				BindingValue candidate;
				const bool found = items[index].Item
					&& TryGetDescriptionValue(
						*items[index].Item.Get(), reader, candidate);
				// Property paths are application-extensible. A getter can replace
				// the aggregate plan and request a nested Refresh; stop the old
				// generation after that exact call instead of scanning the rest of
				// a million-row range with stale descriptions.
				if (_refreshPending)
				{
					staleGeneration = true;
					return std::map<std::wstring, BindingValue>{};
				}
				if (!found || candidate.Empty()) continue;
				if (path.NeedsNumeric)
				{
					double numeric = 0.0;
					if (candidate.TryGetDouble(numeric))
					{
						state.Sum += numeric;
						++state.NumericCount;
					}
				}
				if (path.NeedsMin && (state.Min.Empty()
					|| CompareValues(candidate, state.Min, false) < 0))
					state.Min = candidate;
				if (path.NeedsMax && (state.Max.Empty()
					|| CompareValues(candidate, state.Max, false) > 0))
					state.Max = candidate;
			}
		}
		std::map<std::wstring, BindingValue> values;
		for (size_t aggregateIndex = 0;
			aggregateIndex < aggregates.size(); ++aggregateIndex)
		{
			const auto& aggregate = aggregates[aggregateIndex];
			if (aggregate.Function == CollectionAggregateFunction::Count)
			{
				values.emplace(aggregate.Name,
					BindingValue(static_cast<long long>(end - begin)));
				continue;
			}
			const size_t pathIndex = aggregatePathIndices[aggregateIndex];
			const AggregatePathState emptyState;
			const auto& state = pathIndex == CollectionChangedEventArgs::Npos
				? emptyState : states[pathIndex];
			if (aggregate.Function == CollectionAggregateFunction::Sum)
				values.emplace(aggregate.Name, BindingValue(state.Sum));
			else if (aggregate.Function == CollectionAggregateFunction::Average)
				values.emplace(aggregate.Name, BindingValue(
					state.NumericCount
						? state.Sum / static_cast<double>(state.NumericCount)
						: 0.0));
			else if (aggregate.Function == CollectionAggregateFunction::Min)
				values.emplace(aggregate.Name, state.Min);
			else values.emplace(aggregate.Name, state.Max);
		}
		return values;
	};
	std::function<void(size_t, size_t, size_t)> appendLevel;
	appendLevel = [&](size_t level, size_t begin, size_t end)
	{
		if (staleGeneration || _refreshPending
			|| level >= descriptions.size() || begin >= end) return;
		const auto& description = descriptions[level];
		size_t groupBegin = begin;
		while (groupBegin < end)
		{
			const auto& key = keyAt(groupBegin, level);
			size_t groupEnd = groupBegin + 1;
			for (; groupEnd < end; ++groupEnd)
			{
				const auto& candidate = keyAt(groupEnd, level);
				if (CompareValues(key, candidate, description.IgnoreCase) != 0)
					break;
			}
			BindingListGroup group{
				key, CollectionGroupPropertyName(description), level,
				groupBegin, groupEnd - groupBegin };
			group.Aggregates = buildAggregates(groupBegin, groupEnd);
			if (staleGeneration || _refreshPending) return;
			result.push_back(std::move(group));
			appendLevel(level + 1, groupBegin, groupEnd);
			if (staleGeneration || _refreshPending) return;
			groupBegin = groupEnd;
		}
	};
	appendLevel(0, 0, items.size());
	return result;
}

void CollectionViewSource::RebuildOccurrenceIndex()
{
	_occurrenceIndex.clear();
	_occurrenceIndex.reserve(_items.size());
	for (size_t index = 0; index < _items.size(); ++index)
		if (_items[index].Token != 0)
			_occurrenceIndex[_items[index].Token] = index;
}

void CollectionViewSource::RefreshOccurrenceIndex(size_t first, size_t last)
{
	first = (std::min)(first, _items.size());
	last = (std::min)(last, _items.size());
	for (size_t index = first; index < last; ++index)
		if (_items[index].Token != 0)
			_occurrenceIndex[_items[index].Token] = index;
}

void CollectionViewSource::PublishProjection(
	std::vector<ProjectionItem> target,
	bool publishReset)
{
	const auto sameProjectionItem = [](const ProjectionItem& left,
		const ProjectionItem& right) noexcept
	{
		return left.Token == right.Token
			&& left.Revision == right.Revision
			&& SameItem(left.Item, right.Item);
	};
	if (publishReset)
	{
		const bool changed = _publishedCount != target.size()
			|| _items.size() != target.size()
			|| !std::equal(
				_items.begin(), _items.end(), target.begin(), target.end(),
				sameProjectionItem);
		if (!changed) return;
		const size_t oldSize = _publishedCount;
		_items = std::move(target);
		_publishedCount = _items.size();
		// A DataGrid sort Reset is normally a pure occurrence permutation. Reuse
		// the existing hash nodes in that common million-row path instead of
		// destroying and allocating one node per row again. Marking each visited
		// node also rejects a malformed duplicate-token projection before commit.
		bool reusedOccurrenceIndex =
			_occurrenceIndex.size() == _items.size();
		if (reusedOccurrenceIndex)
		{
			for (const auto& item : _items)
			{
				const auto found = _occurrenceIndex.find(item.Token);
				if (item.Token == 0 || found == _occurrenceIndex.end()
					|| found->second == CollectionChangedEventArgs::Npos)
				{
					reusedOccurrenceIndex = false;
					break;
				}
				found->second = CollectionChangedEventArgs::Npos;
			}
		}
		if (reusedOccurrenceIndex)
		{
			for (size_t index = 0; index < _items.size(); ++index)
				_occurrenceIndex.find(_items[index].Token)->second = index;
		}
		else RebuildOccurrenceIndex();
		CollectionChangedEventArgs change{
			CollectionChangeAction::Reset,
			CollectionChangedEventArgs::Npos,
			CollectionChangedEventArgs::Npos,
			oldSize, _items.size(), oldSize, _items.size() };
		cui::framework::EventAccess::Raise(_changed, this, change);
		return;
	}
	if (_occurrenceIndex.size() != _items.size())
		RebuildOccurrenceIndex();
	const auto suffixDiffers = [&](size_t first)
	{
		if (_items.size() != target.size()) return true;
		first = (std::min)(first, _items.size());
		return !std::equal(
				_items.begin() + first, _items.end(), target.begin() + first,
				sameProjectionItem);
	};
	std::unordered_set<size_t> targetTokens;
	targetTokens.reserve(target.size());
	for (const auto& item : target)
		if (item.Token != 0) targetTokens.insert(item.Token);

	for (size_t index = _items.size(); index-- > 0;)
	{
		if (targetTokens.contains(_items[index].Token)) continue;
		const size_t oldSize = _items.size();
		_occurrenceIndex.erase(_items[index].Token);
		_items.erase(_items.begin() + index);
		_publishedCount = _items.size();
		RefreshOccurrenceIndex(index, _items.size());
		CollectionChangedEventArgs change{
			CollectionChangeAction::Remove,
			index, CollectionChangedEventArgs::Npos,
			1, 0, oldSize, _items.size() };
		change.HasMoreChanges = suffixDiffers(0);
		cui::framework::EventAccess::Raise(_changed, this, change);
	}
	for (size_t index = 0; index < target.size(); ++index)
	{
		if (index < _items.size()
			&& _items[index].Token == target[index].Token)
		{
			if (sameProjectionItem(_items[index], target[index])) continue;
			const size_t oldToken = _items[index].Token;
			_items[index] = target[index];
			_publishedCount = _items.size();
			if (oldToken != _items[index].Token)
				_occurrenceIndex.erase(oldToken);
			_occurrenceIndex[_items[index].Token] = index;
			CollectionChangedEventArgs change{
				CollectionChangeAction::Replace,
				index, index, 1, 1,
				_items.size(), _items.size() };
			change.HasMoreChanges = suffixDiffers(index + 1);
			cui::framework::EventAccess::Raise(_changed, this, change);
			continue;
		}
		const auto found = std::find_if(
			_items.begin() + (std::min)(index, _items.size()), _items.end(),
			[&](const auto& item)
			{ return item.Token == target[index].Token; });
		if (found != _items.end())
		{
			const size_t oldIndex = static_cast<size_t>(
				std::distance(_items.begin(), found));
			if (!sameProjectionItem(*found, target[index]))
			{
				*found = target[index];
				_publishedCount = _items.size();
				CollectionChangedEventArgs replacement{
					CollectionChangeAction::Replace,
					oldIndex, oldIndex, 1, 1,
					_items.size(), _items.size() };
				replacement.HasMoreChanges = true;
				cui::framework::EventAccess::Raise(
					_changed, this, replacement);
			}
			auto item = std::move(*found);
			_items.erase(found);
			_items.insert(_items.begin() + index, std::move(item));
			_publishedCount = _items.size();
			RefreshOccurrenceIndex(
				(std::min)(oldIndex, index), (std::max)(oldIndex, index) + 1);
			CollectionChangedEventArgs change{
				CollectionChangeAction::Move,
				oldIndex, index, 1, 1,
				_items.size(), _items.size() };
			// The alignment loop has already made [0, index] identical to
			// target. Compare only the unresolved suffix so the batching hint
			// does not add another quadratic identity scan to large sorts.
			change.HasMoreChanges = suffixDiffers(index + 1);
			cui::framework::EventAccess::Raise(_changed, this, change);
		}
		else
		{
			const size_t oldSize = _items.size();
			_items.insert(_items.begin() + index, target[index]);
			_publishedCount = _items.size();
			RefreshOccurrenceIndex(index, _items.size());
			CollectionChangedEventArgs change{
				CollectionChangeAction::Add,
				CollectionChangedEventArgs::Npos, index,
				0, 1, oldSize, _items.size() };
			change.HasMoreChanges = suffixDiffers(index + 1);
			cui::framework::EventAccess::Raise(_changed, this, change);
		}
	}
	while (_items.size() > target.size())
	{
		const size_t index = _items.size() - 1;
		const size_t oldSize = _items.size();
		_occurrenceIndex.erase(_items.back().Token);
		_items.pop_back();
		_publishedCount = _items.size();
		CollectionChangedEventArgs change{
			CollectionChangeAction::Remove,
			index, CollectionChangedEventArgs::Npos,
			1, 0, oldSize, _items.size() };
		change.HasMoreChanges = _items.size() > target.size();
		cui::framework::EventAccess::Raise(_changed, this, change);
	}
	_publishedCount = _items.size();
}

void CollectionViewSource::RebuildItemObservations()
{
	if (!_itemObservationsDirty) return;
	std::vector<CompiledBindingPathView> compiledPaths;
	compiledPaths.reserve(
		(_isLiveFilteringRequested ? _filterDescriptions.size() : 0)
		+ (_isLiveSortingRequested ? _sortDescriptions.size() : 0)
		+ (_isLiveGroupingRequested
			? _groupDescriptions.size() + _aggregateDescriptions.size() : 0));
	auto appendCompiledPath = [&](CompiledBindingPathView path)
	{
		if (path.Empty()) return;
		if (std::none_of(
			compiledPaths.begin(), compiledPaths.end(),
			[&](CompiledBindingPathView current)
			{ return SameCompiledCollectionPath(current, path); }))
			compiledPaths.push_back(path);
	};
	if (_isLiveFilteringRequested)
		for (const auto& filter : _filterDescriptions)
			appendCompiledPath(filter.CompiledPath);
	if (_isLiveSortingRequested)
		for (const auto& sort : _sortDescriptions)
			appendCompiledPath(sort.CompiledPath);
	if (_isLiveGroupingRequested)
	{
		for (const auto& group : _groupDescriptions)
			appendCompiledPath(group.CompiledPath);
		for (const auto& aggregate : _aggregateDescriptions)
			appendCompiledPath(aggregate.CompiledPath);
	}
	const auto sameCompiledPaths = [&]
	{
		return compiledPaths.size() == _observedCompiledPaths.size()
			&& std::equal(
				compiledPaths.begin(), compiledPaths.end(),
				_observedCompiledPaths.begin(), _observedCompiledPaths.end(),
				[](CompiledBindingPathView left, CompiledBindingPathView right)
				{ return SameCompiledCollectionPath(left, right); });
	};
#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<std::wstring> authoredPaths;
	authoredPaths.reserve(
		(_isLiveFilteringRequested ? _filterDescriptions.size() : 0)
		+ (_isLiveSortingRequested ? _sortDescriptions.size() : 0)
		+ (_isLiveGroupingRequested
			? _groupDescriptions.size() + _aggregateDescriptions.size() : 0));
	auto appendAuthoredPath = [&](const auto& description)
	{
		if (description.CompiledPath.Empty()
			&& !description.PropertyName.empty())
			authoredPaths.push_back(description.PropertyName);
	};
	if (_isLiveFilteringRequested)
		for (const auto& filter : _filterDescriptions)
			appendAuthoredPath(filter);
	if (_isLiveSortingRequested)
		for (const auto& sort : _sortDescriptions)
			appendAuthoredPath(sort);
	if (_isLiveGroupingRequested)
	{
		for (const auto& group : _groupDescriptions)
			appendAuthoredPath(group);
		for (const auto& aggregate : _aggregateDescriptions)
			appendAuthoredPath(aggregate);
	}
	std::sort(authoredPaths.begin(), authoredPaths.end());
	authoredPaths.erase(
		std::unique(authoredPaths.begin(), authoredPaths.end()),
		authoredPaths.end());
#endif
	if (_observedSource == (_source ? _source.Get() : nullptr)
		&& _observedSourceStructureRevision == _sourceStructureRevision
		&& sameCompiledPaths()
#if CUI_ENABLE_DYNAMIC_XAML
		&& authoredPaths == _observedAuthoredPaths
#endif
		)
	{
		_itemObservationsDirty = false;
		return;
	}
	_itemObservations.clear();
	if (!_source)
	{
		_observedSource = nullptr;
		_observedSourceStructureRevision = _sourceStructureRevision;
		_observedCompiledPaths = std::move(compiledPaths);
#if CUI_ENABLE_DYNAMIC_XAML
		_observedAuthoredPaths = std::move(authoredPaths);
#endif
		_itemObservationsDirty = false;
		return;
	}
	auto* const source = _source.Get();
	if (!compiledPaths.empty())
	{
		const size_t sourceCount = source->Count();
		std::unordered_set<const IBindingSource*> observedItems;
		for (size_t index = 0; index < sourceCount; ++index)
		{
			BindingSourceReference item;
			if (!source->TryGetItem(index, item) || !item) continue;
			// Live shaping is object-based: one property notification refreshes
			// every occurrence projected from that object. Repeated references must
			// therefore share one path observation instead of retaining one owner
			// and one EventConnection set per physical list slot.
			if (!observedItems.insert(item.Get()).second) continue;
			_itemObservations.push_back(ObserveBindingPaths(
				item,
				std::span<const CompiledBindingPathView>{ compiledPaths },
				[this] { Refresh(); }));
		}
	}
#if CUI_ENABLE_DYNAMIC_XAML
	AppendAuthoredItemObservations();
#endif
	_observedSource = source;
	_observedSourceStructureRevision = _sourceStructureRevision;
	_observedCompiledPaths = std::move(compiledPaths);
#if CUI_ENABLE_DYNAMIC_XAML
	_observedAuthoredPaths = std::move(authoredPaths);
#endif
	_itemObservationsDirty = false;
}

void CollectionViewSource::RestoreCurrentItem()
{
	const auto previousItem = _currentItem;
	const size_t previousIdentity = _currentItemOccurrenceIdentity;
	const int previousPosition = _currentPosition;
	const size_t count = Count();
	int next = -1;
	if (_currentItemOccurrenceIdentity != 0)
	{
		size_t index = 0;
		if (TryGetItemIndexByOccurrenceIdentity(
			_currentItemOccurrenceIdentity, index))
			next = static_cast<int>(index);
		else
		{
			for (index = 0; index < count; ++index)
			{
				size_t candidate = 0;
				if (!TryGetItemOccurrenceIdentity(index, candidate)
					|| candidate != _currentItemOccurrenceIdentity) continue;
				next = static_cast<int>(index);
				break;
			}
		}
	}
	// Preserve the historical object-identity fallback for callers that set
	// currency before occurrence identities existed, while preferring the
	// exact source occurrence whenever one has been recorded.
	if (next < 0 && _currentItem)
	{
		for (size_t index = 0; index < count; ++index)
		{
			BindingSourceReference item;
			if (!TryGetItem(index, item) || !SameItem(item, _currentItem))
				continue;
			next = static_cast<int>(index);
			break;
		}
	}
	if (next < 0 && count != 0)
		next = (std::clamp)(previousPosition, 0,
			static_cast<int>(count) - 1);
	_currentPosition = next;
	if (next >= 0)
	{
		BindingSourceReference item;
		size_t identity = 0;
		if (TryGetItem(static_cast<size_t>(next), item))
		{
			_currentItem = std::move(item);
			(void)TryGetItemOccurrenceIdentity(
				static_cast<size_t>(next), identity);
			_currentItemOccurrenceIdentity = identity;
		}
		else
		{
			_currentPosition = -1;
			_currentItem = {};
			_currentItemOccurrenceIdentity = 0;
		}
	}
	else
	{
		_currentItem = {};
		_currentItemOccurrenceIdentity = 0;
	}
	if (previousPosition != _currentPosition
		|| previousIdentity != _currentItemOccurrenceIdentity
		|| !SameItem(previousItem, _currentItem))
		cui::framework::EventAccess::Raise(CurrentChanged, this);
}

bool CollectionViewSource::MoveCurrentToPosition(int position)
{
	if (position < -1 || position >= static_cast<int>(Count()))
		return false;
	BindingSourceReference nextItem;
	size_t nextIdentity = 0;
	if (position >= 0)
	{
		if (!TryGetItem(static_cast<size_t>(position), nextItem)) return false;
		(void)TryGetItemOccurrenceIdentity(
			static_cast<size_t>(position), nextIdentity);
	}
	const auto previous = _currentItem;
	const size_t previousIdentity = _currentItemOccurrenceIdentity;
	const int previousPosition = _currentPosition;
	_currentPosition = position;
	if (position >= 0)
	{
		_currentItem = std::move(nextItem);
		_currentItemOccurrenceIdentity = nextIdentity;
	}
	else
	{
		_currentItem = {};
		_currentItemOccurrenceIdentity = 0;
	}
	if (previousPosition != _currentPosition
		|| previousIdentity != _currentItemOccurrenceIdentity
		|| !SameItem(previous, _currentItem))
		cui::framework::EventAccess::Raise(CurrentChanged, this);
	return true;
}
