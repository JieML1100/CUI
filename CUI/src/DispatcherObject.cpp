#include "DispatcherObject.h"

#include "Core/Threading.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <stdexcept>
#include <utility>

DispatcherObject::DispatcherObject()
	: _dispatcherThreadId(static_cast<std::uint32_t>(::GetCurrentThreadId()))
{
	if (cui::GetUIThreadId() == 0)
		cui::InitializeUIThread();
}

DispatcherObject::~DispatcherObject()
{
	InvalidateLifetimeToken();
}

bool DispatcherObject::CheckAccess() const noexcept
{
	return _dispatcherThreadId
		== static_cast<std::uint32_t>(::GetCurrentThreadId());
}

void DispatcherObject::VerifyAccess() const
{
	if (!CheckAccess())
		throw std::logic_error("DispatcherObject accessed from a non-owning thread");
}

bool DispatcherObject::TryPost(std::function<void()> callback) const
{
	if (!callback || cui::GetUIThreadId() != _dispatcherThreadId)
		return false;
	return cui::PostToUIThread(std::move(callback));
}
