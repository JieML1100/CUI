#include "RuntimeEventHandlerRegistry.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace DesignerModel
{
namespace
{
	bool SameEvent(
		const DesignerEventDescriptor& left,
		const DesignerEventDescriptor& right) noexcept
	{
		return left.Name == right.Name
			&& left.EventField == right.EventField
			&& left.SameSignature(right);
	}
}

struct RuntimeEventHandlerRegistry::State
{
	struct ControlRoute
	{
		std::uint64_t Token = 0;
		UIClass ControlType = UIClass::UI_Base;
		std::wstring CustomControlKey;
		DesignerEventDescriptor Event;
		ControlBinder Bind;
	};

	struct FormRoute
	{
		std::uint64_t Token = 0;
		DesignerEventDescriptor Event;
		FormBinder Bind;
	};

	struct HandlerEntry
	{
		std::type_index Signature{ typeid(void) };
		std::vector<ControlRoute> ControlRoutes;
		std::vector<FormRoute> FormRoutes;
	};

	std::unordered_map<std::wstring, HandlerEntry> Handlers;
	std::uint64_t NextRouteToken = 1;
	bool BatchActive = false;
};

RuntimeEventHandlerRegistry::RegistrationLease::RegistrationLease(
	std::weak_ptr<State> state,
	std::uint64_t firstToken,
	std::uint64_t endToken) noexcept
	: _state(std::move(state)),
	_firstToken(firstToken),
	_endToken(endToken)
{
}

RuntimeEventHandlerRegistry::RegistrationLease::RegistrationLease(
	RegistrationLease&& other) noexcept
	: _state(std::move(other._state)),
	_firstToken(other._firstToken),
	_endToken(other._endToken)
{
	other._firstToken = 0;
	other._endToken = 0;
}

RuntimeEventHandlerRegistry::RegistrationLease&
RuntimeEventHandlerRegistry::RegistrationLease::operator=(
	RegistrationLease&& other) noexcept
{
	if (this == &other) return *this;
	Reset();
	_state = std::move(other._state);
	_firstToken = other._firstToken;
	_endToken = other._endToken;
	other._firstToken = 0;
	other._endToken = 0;
	return *this;
}

void RuntimeEventHandlerRegistry::RegistrationLease::Reset() noexcept
{
	if (_firstToken != 0 && _firstToken < _endToken)
	{
		if (auto state = _state.lock())
			RuntimeEventHandlerRegistry::RemoveRoutes(
				*state, _firstToken, _endToken);
	}
	_state.reset();
	_firstToken = 0;
	_endToken = 0;
}

bool RuntimeEventHandlerRegistry::RegistrationLease::Active() const noexcept
{
	if (_firstToken == 0 || _firstToken >= _endToken) return false;
	const auto state = _state.lock();
	return state && RuntimeEventHandlerRegistry::ContainsRoutes(
		*state, _firstToken, _endToken);
}

RuntimeEventHandlerRegistry::RuntimeEventHandlerRegistry()
	: _state(std::make_shared<State>())
{
}

bool RuntimeEventHandlerRegistry::RegisterBatch(
	const RegistrationBatch& registration,
	std::wstring* outError)
{
	return ApplyBatch(registration, nullptr, nullptr, outError);
}

RuntimeEventHandlerRegistry::RegistrationLease
RuntimeEventHandlerRegistry::RegisterScopedBatch(
	const RegistrationBatch& registration,
	std::wstring* outError)
{
	std::uint64_t firstToken = 0;
	std::uint64_t endToken = 0;
	if (!ApplyBatch(
		registration, &firstToken, &endToken, outError))
		return {};
	if (firstToken == endToken)
	{
		SetError(outError,
			L"运行时事件注册租约必须至少包含一条路由。");
		return {};
	}
	return RegistrationLease(_state, firstToken, endToken);
}

bool RuntimeEventHandlerRegistry::ApplyBatch(
	const RegistrationBatch& registration,
	std::uint64_t* firstToken,
	std::uint64_t* endToken,
	std::wstring* outError)
{
	if (!registration)
	{
		SetError(outError, L"运行时事件注册批次不能为空。");
		return false;
	}
	if (_state->BatchActive)
	{
		SetError(outError, L"运行时事件注册批次不能嵌套。");
		return false;
	}
	std::unordered_map<std::wstring, State::HandlerEntry> before;
	const auto beforeToken = _state->NextRouteToken;
	try
	{
		before = _state->Handlers;
	}
	catch (...)
	{
		SetError(outError, L"无法为运行时事件注册批次创建回滚快照。");
		return false;
	}

	_state->BatchActive = true;
	std::wstring error;
	bool committed = false;
	try
	{
		committed = registration(*this, error);
		if (committed && firstToken) *firstToken = beforeToken;
		if (committed && endToken) *endToken = _state->NextRouteToken;
		if (error.empty()) error = L"运行时事件注册批次被拒绝。";
	}
	catch (...)
	{
		error = L"运行时事件注册批次抛出异常。";
	}
	_state->BatchActive = false;
	if (committed)
	{
		if (outError) outError->clear();
		return true;
	}
	_state->Handlers.swap(before);
	_state->NextRouteToken = beforeToken;
	SetError(outError, std::move(error));
	return false;
}

void RuntimeEventHandlerRegistry::SetError(
	std::wstring* output,
	std::wstring value)
{
	if (output) *output = std::move(value);
}

RuntimeControlEventResolver RuntimeEventHandlerRegistry::ControlResolver() const
{
	auto state = _state;
	return [state = std::move(state)](
		const RuntimeControlEventRequest& request,
		EventConnection& connection,
		std::wstring& error)
	{
		return ResolveControl(*state, request, connection, error);
	};
}

RuntimeFormEventResolver RuntimeEventHandlerRegistry::FormResolver() const
{
	auto state = _state;
	return [state = std::move(state)](
		const RuntimeFormEventRequest& request,
		EventConnection& connection,
		std::wstring& error)
	{
		return ResolveForm(*state, request, connection, error);
	};
}

bool RuntimeEventHandlerRegistry::Remove(
	const std::wstring& handlerName) noexcept
{
	if (_state->BatchActive) return false;
	return _state->Handlers.erase(handlerName) != 0;
}

void RuntimeEventHandlerRegistry::Clear() noexcept
{
	if (_state->BatchActive) return;
	_state->Handlers.clear();
}

size_t RuntimeEventHandlerRegistry::HandlerCount() const noexcept
{
	return _state->Handlers.size();
}

void RuntimeEventHandlerRegistry::RemoveRoutes(
	State& state,
	std::uint64_t firstToken,
	std::uint64_t endToken) noexcept
{
	for (auto handler = state.Handlers.begin();
		handler != state.Handlers.end();)
	{
		auto& entry = handler->second;
		entry.ControlRoutes.erase(std::remove_if(
			entry.ControlRoutes.begin(), entry.ControlRoutes.end(),
			[=](const State::ControlRoute& route)
			{
				return route.Token >= firstToken && route.Token < endToken;
			}), entry.ControlRoutes.end());
		entry.FormRoutes.erase(std::remove_if(
			entry.FormRoutes.begin(), entry.FormRoutes.end(),
			[=](const State::FormRoute& route)
			{
				return route.Token >= firstToken && route.Token < endToken;
			}), entry.FormRoutes.end());
		if (entry.ControlRoutes.empty() && entry.FormRoutes.empty())
			handler = state.Handlers.erase(handler);
		else
			++handler;
	}
}

bool RuntimeEventHandlerRegistry::ContainsRoutes(
	const State& state,
	std::uint64_t firstToken,
	std::uint64_t endToken) noexcept
{
	for (const auto& [name, entry] : state.Handlers)
	{
		(void)name;
		if (std::any_of(entry.ControlRoutes.begin(),
			entry.ControlRoutes.end(), [=](const State::ControlRoute& route)
			{
				return route.Token >= firstToken && route.Token < endToken;
			}))
			return true;
		if (std::any_of(entry.FormRoutes.begin(),
			entry.FormRoutes.end(), [=](const State::FormRoute& route)
			{
				return route.Token >= firstToken && route.Token < endToken;
			}))
			return true;
	}
	return false;
}

bool RuntimeEventHandlerRegistry::AddControlRoute(
	std::wstring handlerName,
	UIClass controlType,
	std::wstring customControlKey,
	DesignerEventDescriptor descriptor,
	ControlBinder binder,
	std::wstring* outError)
{
	try
	{
		auto [position, inserted] =
			_state->Handlers.try_emplace(handlerName);
		auto& entry = position->second;
		if (inserted) entry.Signature = descriptor.Signature;
		else if (entry.Signature != descriptor.Signature)
		{
			SetError(outError, L"处理函数名已由另一种事件签名占用："
				+ handlerName);
			return false;
		}
		const auto duplicate = std::any_of(
			entry.ControlRoutes.begin(), entry.ControlRoutes.end(),
			[&](const State::ControlRoute& route)
			{
				return route.ControlType == controlType
					&& route.CustomControlKey == customControlKey
					&& SameEvent(route.Event, descriptor);
			});
		if (duplicate)
		{
			SetError(outError, L"运行时控件事件路由已注册："
				+ handlerName + L" / " + descriptor.Name);
			return false;
		}
		if (_state->NextRouteToken ==
			std::numeric_limits<std::uint64_t>::max())
		{
			if (inserted) _state->Handlers.erase(position);
			SetError(outError, L"运行时事件路由令牌已耗尽。");
			return false;
		}
		const auto token = _state->NextRouteToken++;
		entry.ControlRoutes.push_back(State::ControlRoute{
			token, controlType, std::move(customControlKey),
			std::move(descriptor), std::move(binder) });
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		auto found = _state->Handlers.find(handlerName);
		if (found != _state->Handlers.end()
			&& found->second.ControlRoutes.empty()
			&& found->second.FormRoutes.empty())
			_state->Handlers.erase(found);
		SetError(outError, L"保存运行时控件事件路由时资源分配失败。");
		return false;
	}
}

bool RuntimeEventHandlerRegistry::AddFormRoute(
	std::wstring handlerName,
	DesignerEventDescriptor descriptor,
	FormBinder binder,
	std::wstring* outError)
{
	try
	{
		auto [position, inserted] =
			_state->Handlers.try_emplace(handlerName);
		auto& entry = position->second;
		if (inserted) entry.Signature = descriptor.Signature;
		else if (entry.Signature != descriptor.Signature)
		{
			SetError(outError, L"处理函数名已由另一种事件签名占用："
				+ handlerName);
			return false;
		}
		const auto duplicate = std::any_of(
			entry.FormRoutes.begin(), entry.FormRoutes.end(),
			[&](const State::FormRoute& route)
			{
				return SameEvent(route.Event, descriptor);
			});
		if (duplicate)
		{
			SetError(outError, L"运行时窗体事件路由已注册："
				+ handlerName + L" / " + descriptor.Name);
			return false;
		}
		if (_state->NextRouteToken ==
			std::numeric_limits<std::uint64_t>::max())
		{
			if (inserted) _state->Handlers.erase(position);
			SetError(outError, L"运行时事件路由令牌已耗尽。");
			return false;
		}
		const auto token = _state->NextRouteToken++;
		entry.FormRoutes.push_back(State::FormRoute{
			token, std::move(descriptor), std::move(binder) });
		if (outError) outError->clear();
		return true;
	}
	catch (...)
	{
		auto found = _state->Handlers.find(handlerName);
		if (found != _state->Handlers.end()
			&& found->second.ControlRoutes.empty()
			&& found->second.FormRoutes.empty())
			_state->Handlers.erase(found);
		SetError(outError, L"保存运行时窗体事件路由时资源分配失败。");
		return false;
	}
}

bool RuntimeEventHandlerRegistry::ResolveControl(
	State& state,
	const RuntimeControlEventRequest& request,
	EventConnection& connection,
	std::wstring& error)
{
	const auto found = state.Handlers.find(request.HandlerName);
	if (found == state.Handlers.end())
	{
		error = L"未注册运行时处理函数：" + request.HandlerName;
		return false;
	}
	const auto& entry = found->second;
	if (entry.Signature != request.Event.Signature)
	{
		error = L"运行时处理函数签名与控件事件不兼容："
			+ request.HandlerName;
		return false;
	}

	const State::ControlRoute* wildcard = nullptr;
	const State::ControlRoute* selected = nullptr;
	const auto customKey = request.CustomType.Empty()
		? std::wstring{} : request.CustomType.RegistryKey();
	for (const auto& route : entry.ControlRoutes)
	{
		if (!SameEvent(route.Event, request.Event)) continue;
		if (!route.CustomControlKey.empty())
		{
			if (route.CustomControlKey == customKey)
			{
				selected = &route;
				break;
			}
			continue;
		}
		if (route.ControlType == request.ControlType)
		{
			selected = &route;
			break;
		}
		if (route.ControlType == UIClass::UI_Base && !wildcard)
			wildcard = &route;
	}
	if (!selected) selected = wildcard;
	if (!selected)
	{
		error = L"处理函数没有匹配当前控件类型与事件的路由："
			+ request.HandlerName + L" / " + request.Event.Name;
		return false;
	}
	try
	{
		return selected->Bind(request, connection, error);
	}
	catch (...)
	{
		error = L"运行时控件事件处理函数订阅时抛出异常。";
		return false;
	}
}

bool RuntimeEventHandlerRegistry::ResolveForm(
	State& state,
	const RuntimeFormEventRequest& request,
	EventConnection& connection,
	std::wstring& error)
{
	const auto found = state.Handlers.find(request.HandlerName);
	if (found == state.Handlers.end())
	{
		error = L"未注册运行时处理函数：" + request.HandlerName;
		return false;
	}
	const auto& entry = found->second;
	if (entry.Signature != request.Event.Signature)
	{
		error = L"运行时处理函数签名与窗体事件不兼容："
			+ request.HandlerName;
		return false;
	}
	const auto route = std::find_if(
		entry.FormRoutes.begin(), entry.FormRoutes.end(),
		[&](const State::FormRoute& value)
		{
			return SameEvent(value.Event, request.Event);
		});
	if (route == entry.FormRoutes.end())
	{
		error = L"处理函数没有匹配当前窗体事件的路由："
			+ request.HandlerName + L" / " + request.Event.Name;
		return false;
	}
	try
	{
		return route->Bind(request, connection, error);
	}
	catch (...)
	{
		error = L"运行时窗体事件处理函数订阅时抛出异常。";
		return false;
	}
}
}
