#pragma once

#include "Control.h"

#include <memory>
#include <utility>

namespace cui::framework
{
	/** Mutation surface reserved for XAML materialization and template expansion. */
	struct XamlAccess final
	{
		XamlAccess() = delete;

		static bool SetTypeDescriptor(
			Control& target,
			std::shared_ptr<const DeclarativeTypeDescriptor> descriptor,
			std::wstring* outError = nullptr)
		{
			return target.SetDeclarativeTypeDescriptor(
				std::move(descriptor), outError);
		}

		static bool RegisterTemplatePart(
			Control& owner, std::wstring localName, Control* instance)
		{
			return owner.RegisterDeclarativeTemplatePart(
				std::move(localName), instance);
		}

		static bool RegisterContentPresenter(
			Control& owner, std::wstring propertyName, Control* instance)
		{
			return owner.RegisterDeclarativeContentPresenter(
				std::move(propertyName), instance);
		}

		static void ClearTemplateScope(Control& owner)
		{
			owner.ClearDeclarativeTemplateScope();
		}

		static bool SetComponentBehavior(
			Control& target,
			std::unique_ptr<IDeclarativeComponentBehavior> behavior,
			const DeclarativeComponentBehaviorContext& context,
			std::wstring* outError = nullptr)
		{
			return target.SetDeclarativeComponentBehavior(
				std::move(behavior), context, outError);
		}

		static void ClearComponentBehavior(Control& target) noexcept
		{
			target.ClearDeclarativeComponentBehavior();
		}

		static void SetInheritedDataContext(
			Control& target, BindingSourceReference value)
		{
			target.SetInheritedDataContext(std::move(value));
		}

		static void SetLogicalParent(Control& target, Control* parent)
		{
			target.SetLogicalParent(parent);
		}

		static void SetTemplatedParent(Control& target, Control* parent)
		{
			target.SetTemplatedParent(parent);
		}

		static bool DefineVisualStateGroups(
			Control& target,
			std::vector<DeclarativeVisualStateGroupDefinition> groups,
			std::wstring* outError = nullptr)
		{
			return target.DefineVisualStateGroups(
				std::move(groups), outError);
		}

		static bool DefineInteractions(
			Control& target,
			std::vector<DeclarativeVisualStateGroupDefinition> groups,
			std::vector<DeclarativeEventTriggerDefinition> eventTriggers,
			std::wstring* outError = nullptr)
		{
			return target.DefineDeclarativeInteractions(
				std::move(groups), std::move(eventTriggers), outError);
		}

		static void RetainEventConnection(
			Control& target, EventConnection connection)
		{
			target.RetainEventConnection(std::move(connection));
		}

		static void RetainTemplateEventConnection(
			Control& target, EventConnection connection)
		{
			target.RetainTemplateEventConnection(std::move(connection));
		}

		static bool SetTemplate(
			Control& target,
			const ControlTemplateReference& value,
			DependencyPropertyValueSource source)
		{
			return target.TrySetPropertyValue(
				L"Template", BindingValue(value), source);
		}

		static void ClearRetainedEventConnections(Control& target) noexcept
		{
			target.ClearRetainedEventConnections();
		}
	};
}
