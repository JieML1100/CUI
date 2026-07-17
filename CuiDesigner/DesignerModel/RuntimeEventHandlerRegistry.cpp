#include "RuntimeEventHandlerRegistry.h"

#include <algorithm>
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
		UIClass ControlType = UIClass::UI_Base;
		std::wstring CustomControlKey;
		DesignerEventDescriptor Event;
		ControlBinder Bind;
	};

	struct FormRoute
	{
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
};

RuntimeEventHandlerRegistry::RuntimeEventHandlerRegistry()
	: _state(std::make_shared<State>())
{
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
	return _state->Handlers.erase(handlerName) != 0;
}

void RuntimeEventHandlerRegistry::Clear() noexcept
{
	_state->Handlers.clear();
}

size_t RuntimeEventHandlerRegistry::HandlerCount() const noexcept
{
	return _state->Handlers.size();
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
		entry.ControlRoutes.push_back(State::ControlRoute{
			controlType, std::move(customControlKey),
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
		entry.FormRoutes.push_back(State::FormRoute{
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
