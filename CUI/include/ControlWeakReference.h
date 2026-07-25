#pragma once

#include <cstddef>
#include <memory>

class Control;

/**
 * Non-owning identity for a Control.
 *
 * The raw address is kept only for identity. Get() publishes it while the
 * captured DispatcherObject lifetime is still alive; an expired explicit
 * reference remains distinguishable from an unspecified reference so command
 * target resolution never falls back to focus after its authored target dies.
 */
class ControlWeakReference final
{
public:
	ControlWeakReference() noexcept = default;
	ControlWeakReference(std::nullptr_t) noexcept {}
	ControlWeakReference(Control* target) noexcept;

	ControlWeakReference& operator=(Control* target) noexcept;
	ControlWeakReference& operator=(std::nullptr_t) noexcept
	{
		Reset();
		return *this;
	}

	Control* Get() const noexcept;
	bool HasValue() const noexcept { return _target != nullptr; }
	explicit operator bool() const noexcept { return Get() != nullptr; }
	void Reset() noexcept
	{
		_target = nullptr;
		_lifetime.reset();
	}

	friend bool operator==(
		const ControlWeakReference& left,
		const ControlWeakReference& right) noexcept
	{
		if (left._target != right._target) return false;
		if (!left._target) return true;
		return !left._lifetime.owner_before(right._lifetime)
			&& !right._lifetime.owner_before(left._lifetime);
	}
	friend bool operator!=(
		const ControlWeakReference& left,
		const ControlWeakReference& right) noexcept
	{
		return !(left == right);
	}

	friend bool operator==(
		const ControlWeakReference& left, const Control* right) noexcept
	{
		return left.Get() == right;
	}
	friend bool operator==(
		const Control* left, const ControlWeakReference& right) noexcept
	{
		return right == left;
	}
	friend bool operator!=(
		const ControlWeakReference& left, const Control* right) noexcept
	{
		return !(left == right);
	}
	friend bool operator!=(
		const Control* left, const ControlWeakReference& right) noexcept
	{
		return !(right == left);
	}

private:
	Control* _target = nullptr;
	std::weak_ptr<const bool> _lifetime;
};
