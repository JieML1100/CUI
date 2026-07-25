#pragma once

#include <cstdint>
#include <functional>
#include <memory>

/**
 * Thread-affine root of the CUI object model.
 *
 * Every element records the thread that created it.  CheckAccess/VerifyAccess
 * are object-scoped; posting is accepted only when that thread owns the
 * process UI dispatcher.  The lifetime token is shared by framework async
 * work and is invalidated before derived storage is released.
 */
class DispatcherObject
{
public:
	DispatcherObject();
	DispatcherObject(const DispatcherObject&) = delete;
	DispatcherObject& operator=(const DispatcherObject&) = delete;
	virtual ~DispatcherObject();

	bool CheckAccess() const noexcept;
	void VerifyAccess() const;
	std::uint32_t DispatcherThreadId() const noexcept
	{
		return _dispatcherThreadId;
	}
	bool TryPost(std::function<void()> callback) const;
	/** Captures lifetime without extending the object's ownership. */
	std::weak_ptr<const bool> WeakLifetimeToken() const noexcept
	{
		return _lifetimeToken;
	}

protected:
	std::weak_ptr<bool> LifetimeToken() const noexcept
	{
		return _lifetimeToken;
	}
	void InvalidateLifetimeToken() noexcept
	{
		if (_lifetimeToken) *_lifetimeToken = false;
	}
	std::shared_ptr<bool> _lifetimeToken = std::make_shared<bool>(true);

private:
	std::uint32_t _dispatcherThreadId = 0;
};
