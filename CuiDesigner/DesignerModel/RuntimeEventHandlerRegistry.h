#pragma once

#include "RuntimeDocument.h"
#include "../../CUI/include/Form.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
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
private:
	struct State;

public:
	using RegistrationBatch = std::function<bool(
		RuntimeEventHandlerRegistry& registry,
		std::wstring& error)>;

	/**
	 * Move-only ownership of every route committed by one scoped batch.
	 * Reset/destruction removes only those routes. EventConnections already
	 * created from them remain owned by their RuntimeDocument.
	 */
	class RegistrationLease final
	{
	public:
		RegistrationLease() = default;
		~RegistrationLease() { Reset(); }

		RegistrationLease(const RegistrationLease&) = delete;
		RegistrationLease& operator=(const RegistrationLease&) = delete;

		RegistrationLease(RegistrationLease&& other) noexcept;
		RegistrationLease& operator=(RegistrationLease&& other) noexcept;

		void Reset() noexcept;
		[[nodiscard]] bool Active() const noexcept;
		explicit operator bool() const noexcept { return Active(); }

	private:
		friend class RuntimeEventHandlerRegistry;

		RegistrationLease(
			std::weak_ptr<State> state,
			std::uint64_t firstToken,
			std::uint64_t endToken) noexcept;

		std::weak_ptr<State> _state;
		std::uint64_t _firstToken = 0;
		std::uint64_t _endToken = 0;
	};

	RuntimeEventHandlerRegistry();

	/**
	 * Applies a group of route registrations atomically. A false result or
	 * exception restores the exact pre-batch route set while existing resolver
	 * objects keep observing the same shared state. The callback may register
	 * routes only; it must not remove routes or reset another active lease.
	 */
	bool RegisterBatch(
		const RegistrationBatch& registration,
		std::wstring* outError = nullptr);

	/**
	 * Atomically registers a group and returns exclusive route ownership.
	 * An empty lease means the whole batch failed and was rolled back.
	 */
	[[nodiscard]] RegistrationLease RegisterScopedBatch(
		const RegistrationBatch& registration,
		std::wstring* outError = nullptr);

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

	/**
	 * Restricted bridge used by generated event sinks. Portable strings are
	 * converted back into the same fixed custom-event contract before the real
	 * C++ Event member and callable are type-checked by RegisterCustomControl.
	 */
	template<typename Owner, typename RuntimeEvent, typename Handler>
	bool RegisterGeneratedCustomControl(
		std::wstring handlerName,
		std::wstring xamlNamespace,
		std::wstring xamlName,
		std::wstring eventName,
		std::string eventField,
		std::wstring signatureName,
		RuntimeEvent Owner::* eventMember,
		Handler&& handler,
		std::wstring* outError = nullptr)
	{
		DesignerCustomEventSignature signature{};
		if (!DesignerEventCatalog::TryParseCustomSignature(
			signatureName, signature))
		{
			SetError(outError,
				L"生成的自定义事件签名预设无效：" + signatureName);
			return false;
		}
		DesignerCustomControlType customType;
		customType.XamlNamespace = std::move(xamlNamespace);
		customType.XamlName = std::move(xamlName);
		DesignerCustomEventDescriptor customEvent;
		customEvent.Name = std::move(eventName);
		customEvent.EventField = std::move(eventField);
		customEvent.Signature = signature;
		return RegisterCustomControl(
			std::move(handlerName), std::move(customType),
			std::move(customEvent), eventMember,
			std::forward<Handler>(handler), outError);
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

	bool ApplyBatch(
		const RegistrationBatch& registration,
		std::uint64_t* firstToken,
		std::uint64_t* endToken,
		std::wstring* outError);

	static void RemoveRoutes(
		State& state,
		std::uint64_t firstToken,
		std::uint64_t endToken) noexcept;
	static bool ContainsRoutes(
		const State& state,
		std::uint64_t firstToken,
		std::uint64_t endToken) noexcept;

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
