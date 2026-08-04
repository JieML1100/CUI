#pragma once

#include "ContentControl.h"
#include "ItemsControl.h"

#include <memory>
#include <utility>

namespace cui::framework
{
	/**
	 * Narrow bridge used by the XAML/template engine and framework diagnostics.
	 *
	 * These operations describe presentation infrastructure, not application
	 * control state. Keeping them here prevents runtime materialization,
	 * designer topology code, and tests from turning ItemsHost or generated
	 * presenters into a second public object model.
	 */
	struct TemplateAccess final
	{
		TemplateAccess() = delete;

		static Control* GetTemplateRoot(const Control& owner) noexcept
		{
			return owner.GetControlTemplateRoot();
		}
		static Control* SetTemplateRoot(
			Control& owner, std::unique_ptr<Control> value)
		{
			return owner.SetControlTemplateRoot(std::move(value));
		}
		static std::unique_ptr<Control> DetachTemplateRoot(Control& owner)
		{
			return owner.DetachVisualChildTemplateRoot();
		}
		static void CompleteTemplateApplication(Control& owner)
		{
			owner.CompleteControlTemplateApplication();
		}
		static void AbortTemplateApplication(Control& owner) noexcept
		{
			owner.AbortControlTemplateApplication();
		}
		static void SetPresentationSuppressed(
			Control& target, bool value)
		{
			target.SetPresentationSuppressed(value);
		}
		static void SetParticipatesInPresentationScene(
			Control& target, bool value)
		{
			target.SetParticipatesInPresentationScene(value);
		}
		static bool SetTemplate(
			Control& target,
			ControlTemplateReference value,
			DependencyPropertyValueSource source)
		{
			return target.TrySetPropertyValue(
				Control::TemplateProperty(),
				BindingValue(std::move(value)), source);
		}
		static bool RegisterTemplatePart(
			Control& owner, TemplatePartToken token, Control* instance)
		{
			return owner.RegisterDeclarativeTemplatePart(
				token, instance);
		}
#if CUI_ENABLE_DYNAMIC_XAML
		static bool RegisterTemplatePart(
			Control& owner, std::wstring localName, Control* instance)
		{
			return owner.RegisterDeclarativeTemplatePart(
				std::move(localName), instance);
		}
#endif
#if CUI_ENABLE_DYNAMIC_XAML
		static bool RegisterComponentContentPresenter(
			Control& owner, std::wstring propertyName, Control* instance)
		{
			return owner.RegisterDeclarativeContentPresenter(
				std::move(propertyName), instance);
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
#endif
		static bool InstallCompiledInteractions(
			Control& target,
			const CompiledInteractionProgramView& program,
			std::span<const BindingValue> values,
			std::span<Control* const> targets,
			std::wstring* outError = nullptr)
		{
			return target.InstallCompiledInteractions(
				program, values, targets, outError);
		}
		static void RetainTemplateEventConnection(
			Control& target, EventConnection connection)
		{
			target.RetainTemplateEventConnection(std::move(connection));
		}

		static Panel* GetItemsHost(const ItemsControl& owner) noexcept
		{
			return owner.GetItemsHost();
		}
		static ItemsPresenter* GetItemsPresenter(
			const ItemsControl& owner) noexcept
		{
			return owner.GetTemplateItemsPresenter();
		}
		static Control* GetTemplateRoot(const ItemsControl& owner) noexcept
		{
			return owner.GetControlTemplateRoot();
		}
		static bool RegisterItemsPresenter(
			ItemsControl& owner, ItemsPresenter* presenter)
		{
			return owner.RegisterTemplateItemsPresenter(presenter);
		}
		static Control* SetTemplateRoot(
			ItemsControl& owner, std::unique_ptr<Control> value)
		{
			return owner.SetControlTemplateRoot(std::move(value));
		}
		static std::unique_ptr<Control> DetachTemplateRoot(
			ItemsControl& owner)
		{
			return owner.DetachVisualChildTemplateRoot();
		}
		static Panel* GetItemsHost(const ItemsPresenter& presenter) noexcept
		{
			return presenter.GetItemsHost();
		}

		static ContentPresenter* GetGeneratedPresenter(
			const ContentControl& owner) noexcept
		{
			return owner.GetGeneratedPresenter();
		}
		static ContentPresenter* GetContentPresenter(
			const ContentControl& owner) noexcept
		{
			return owner.GetTemplateContentPresenter();
		}
		static Control* GetGeneratedContent(
			const ContentControl& owner) noexcept
		{
			return owner.GetGeneratedContent();
		}
		static Control* GetTemplateRoot(const ContentControl& owner) noexcept
		{
			return owner.GetControlTemplateRoot();
		}
		static bool RegisterContentPresenter(
			ContentControl& owner, ContentPresenter* presenter)
		{
			return owner.RegisterTemplateContentPresenter(presenter);
		}
		static Control* SetTemplateRoot(
			ContentControl& owner, std::unique_ptr<Control> value)
		{
			return owner.SetControlTemplateRoot(std::move(value));
		}
		static std::unique_ptr<Control> DetachTemplateRoot(
			ContentControl& owner)
		{
			return owner.DetachVisualChildTemplateRoot();
		}
	};
}
