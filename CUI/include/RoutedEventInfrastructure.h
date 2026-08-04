#pragma once

#include "UIElement.h"

namespace cui::framework
{
	/** Route-engine-only access to instance handler tables. */
	struct RoutedEventAccess final
	{
		RoutedEventAccess() = delete;

		template<typename TArgs, RoutedEventId TEventId>
		static RoutedHandlerInvocationCount InvokeHandlers(
			RoutedEvent<TArgs, TEventId>& event,
			Control* sender,
			TArgs& args)
		{
			return event.InvokeHandlers(sender, args);
		}

		static RoutedHandlerInvocationCount InvokeSemanticHandlers(
			UIElement& target,
			Control* sender,
			RoutedEventArgs& args)
		{
			return target.InvokeSemanticRoutedEventHandlers(sender, args);
		}

		static RoutedHandlerInvocationCount InvokeGenericHandlers(
			UIElement& target,
			Control* sender,
			RoutedEventArgs& args)
		{
			return RoutedEventHandlerStore::Invoke(
				target,
				args.EventId,
				RoutedHandlerStorageKind::Generic,
				sender,
				args);
		}

		template<typename F>
		static EventConnection SubscribeClick(UIElement& target, F&& handler)
		{
			return target.Click.Subscribe(std::forward<F>(handler));
		}

		static void RaiseClick(UIElement& target, Control* sender)
		{
			RoutedEventArgs args;
			target.Click.Invoke(sender, args);
		}
	};
}
