#pragma once

#include "RuntimeDocument.h"
#include "../../CUI/include/Form.h"

#include <memory>
#include <type_traits>
#include <utility>

namespace DesignerModel
{
/**
 * Declarative, signature-checked name router for dynamic document events.
 *
 * RegisterControl/RegisterForm pair a persisted handler name with the exact
 * CUI Event member and a callable. Resolver objects retain shared registry
 * state, so they remain valid after this facade is copied or destroyed and
 * observe handlers registered later on the same state. Registration and
 * resolution are intended to run on the owning UI thread.
 */
class RuntimeEventHandlerRegistry final
{
public:
	RuntimeEventHandlerRegistry();

	/**
	 * Registers one control event route. UI_Base is a wildcard for common
	 * Control events and requires an Event member declared by Control itself.
	 */
	template<typename Owner, typename RuntimeEvent, typename Handler>
	bool RegisterControl(
		std::wstring handlerName,
		UIClass controlType,
		std::wstring eventName,
		RuntimeEvent Owner::* eventMember,
		Handler&& handler,
		std::wstring* outError = nullptr)
	{
		static_assert(std::is_base_of_v<Control, Owner>,
			"Runtime control event owners must derive from Control");
		if (controlType == UIClass::UI_Base
			&& !std::is_same_v<Owner, Control>)
		{
			SetError(outError,
				L"UI_Base 通配注册必须使用 Control 声明的事件成员。");
			return false;
		}

		try
		{
			std::wstring validationError;
			if (!DesignerEventCatalog::ValidateHandlerName(
				handlerName, &validationError))
			{
				SetError(outError, std::move(validationError));
				return false;
			}
			auto descriptor = DesignerEventCatalog::FindControlEvent(
				controlType, eventName);
			if (!descriptor)
			{
				SetError(outError, L"控件类型不公开事件：" + eventName);
				return false;
			}
			if (!descriptor->MatchesEventMember(eventMember))
			{
				SetError(outError,
					L"注册的 C++ Event 成员与事件目录不一致：" + eventName);
				return false;
			}
			typename RuntimeEvent::std_function_type callback(
				std::forward<Handler>(handler));
			if (!callback)
			{
				SetError(outError, L"运行时控件事件处理函数为空。");
				return false;
			}
			return AddControlRoute(
				std::move(handlerName),
				controlType,
				{},
				std::move(*descriptor),
				[eventMember, callback = std::move(callback)](
					const RuntimeControlEventRequest& request,
					EventConnection& connection,
					std::wstring& error) mutable
				{
					auto* target = dynamic_cast<Owner*>(&request.Target);
					if (!target)
					{
						error = L"注册的事件成员与目标控件 C++ 类型不匹配。";
						return false;
					}
					connection = (target->*eventMember).Subscribe(callback);
					if (connection.Connected()) return true;
					error = L"CUI Event 拒绝了空的控件事件订阅。";
					return false;
				},
				outError);
		}
		catch (...)
		{
			SetError(outError, L"注册运行时控件事件时资源分配失败。");
			return false;
		}
	}

	/**
	 * Registers an event declared by a portable custom-control contract.
	 * The manifest cannot provide C++ types: this template proves that the
	 * selected Event member has the exact function type of the fixed preset.
	 */
	template<typename Owner, typename RuntimeEvent, typename Handler>
	bool RegisterCustomControl(
		std::wstring handlerName,
		DesignerCustomControlType customType,
		DesignerCustomEventDescriptor customEvent,
		RuntimeEvent Owner::* eventMember,
		Handler&& handler,
		std::wstring* outError = nullptr)
	{
		static_assert(std::is_base_of_v<Control, Owner>,
			"Runtime custom event owners must derive from Control");
		try
		{
			std::wstring validationError;
			if (customType.Empty())
			{
				SetError(outError, L"自定义事件注册缺少控件类型身份。");
				return false;
			}
			if (!DesignerEventCatalog::ValidateHandlerName(
				handlerName, &validationError))
			{
				SetError(outError, std::move(validationError));
				return false;
			}
			auto descriptor = DesignerEventCatalog::FromCustomEvent(customEvent);
			if (!descriptor)
			{
				SetError(outError, L"自定义事件契约无效：" + customEvent.Name);
				return false;
			}
			using Function = typename RuntimeEvent::function_type;
			if (descriptor->Signature != std::type_index(typeid(Function)))
			{
				SetError(outError,
					L"实际 C++ Event 成员与自定义事件签名预设不一致："
					+ customEvent.Name);
				return false;
			}
			typename RuntimeEvent::std_function_type callback(
				std::forward<Handler>(handler));
			if (!callback)
			{
				SetError(outError, L"运行时自定义事件处理函数为空。");
				return false;
			}
			return AddControlRoute(
				std::move(handlerName),
				UIClass::UI_Base,
				customType.RegistryKey(),
				std::move(*descriptor),
				[eventMember, callback = std::move(callback)](
					const RuntimeControlEventRequest& request,
					EventConnection& connection,
					std::wstring& error) mutable
				{
					auto* target = dynamic_cast<Owner*>(&request.Target);
					if (!target)
					{
						error = L"自定义事件成员与目标控件 C++ 类型不匹配。";
						return false;
					}
					connection = (target->*eventMember).Subscribe(callback);
					if (connection.Connected()) return true;
					error = L"CUI Event 拒绝了空的自定义事件订阅。";
					return false;
				},
				outError);
		}
		catch (...)
		{
			SetError(outError, L"注册运行时自定义事件时资源分配失败。");
			return false;
		}
	}

	template<typename Owner, typename RuntimeEvent, typename Handler>
	bool RegisterForm(
		std::wstring handlerName,
		std::wstring eventName,
		RuntimeEvent Owner::* eventMember,
		Handler&& handler,
		std::wstring* outError = nullptr)
	{
		static_assert(std::is_base_of_v<Owner, ::Form>,
			"Runtime Form event owners must be Form or one of its base classes");
		try
		{
			std::wstring validationError;
			if (!DesignerEventCatalog::ValidateHandlerName(
				handlerName, &validationError))
			{
				SetError(outError, std::move(validationError));
				return false;
			}
			auto descriptor = DesignerEventCatalog::FindFormEvent(eventName);
			if (!descriptor)
			{
				SetError(outError, L"Form 不公开事件：" + eventName);
				return false;
			}
			if (!descriptor->MatchesEventMember(eventMember))
			{
				SetError(outError,
					L"注册的 C++ Event 成员与 Form 事件目录不一致："
					+ eventName);
				return false;
			}
			typename RuntimeEvent::std_function_type callback(
				std::forward<Handler>(handler));
			if (!callback)
			{
				SetError(outError, L"运行时窗体事件处理函数为空。");
				return false;
			}
			return AddFormRoute(
				std::move(handlerName),
				std::move(*descriptor),
				[eventMember, callback = std::move(callback)](
					const RuntimeFormEventRequest& request,
					EventConnection& connection,
					std::wstring& error) mutable
				{
					connection = (request.Target.*eventMember).Subscribe(callback);
					if (connection.Connected()) return true;
					error = L"CUI Event 拒绝了空的窗体事件订阅。";
					return false;
				},
				outError);
		}
		catch (...)
		{
			SetError(outError, L"注册运行时窗体事件时资源分配失败。");
			return false;
		}
	}

	RuntimeControlEventResolver ControlResolver() const;
	RuntimeFormEventResolver FormResolver() const;

	/** Existing EventConnections stay active until their document rebinds. */
	bool Remove(const std::wstring& handlerName) noexcept;
	void Clear() noexcept;
	size_t HandlerCount() const noexcept;

private:
	using ControlBinder = std::function<bool(
		const RuntimeControlEventRequest&,
		EventConnection&,
		std::wstring&)>;
	using FormBinder = std::function<bool(
		const RuntimeFormEventRequest&,
		EventConnection&,
		std::wstring&)>;

	struct State;

	std::shared_ptr<State> _state;

	static void SetError(std::wstring* output, std::wstring value);

	bool AddControlRoute(
		std::wstring handlerName,
		UIClass controlType,
		std::wstring customControlKey,
		DesignerEventDescriptor descriptor,
		ControlBinder binder,
		std::wstring* outError);

	bool AddFormRoute(
		std::wstring handlerName,
		DesignerEventDescriptor descriptor,
		FormBinder binder,
		std::wstring* outError);

	static bool ResolveControl(
		State& state,
		const RuntimeControlEventRequest& request,
		EventConnection& connection,
		std::wstring& error);

	static bool ResolveForm(
		State& state,
		const RuntimeFormEventRequest& request,
		EventConnection& connection,
		std::wstring& error);
};
}
