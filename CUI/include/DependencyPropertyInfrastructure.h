#pragma once

#include "Control.h"

#include <utility>

namespace cui::framework
{
	/** Framework-only writer for non-Local dependency-property precedence slots. */
	struct DependencyPropertyAccess final
	{
		DependencyPropertyAccess() = delete;

		#if CUI_ENABLE_DYNAMIC_XAML
		static bool SetValue(
			Control& target,
			const std::wstring& propertyName,
			const BindingValue& value,
			DependencyPropertyValueSource source)
		{
			return target.TrySetPropertyValue(propertyName, value, source);
		}
		#endif

		static bool SetValue(
			Control& target,
			const DependencyProperty& property,
			const BindingValue& value,
			DependencyPropertyValueSource source)
		{
			return target.TrySetPropertyValue(property, value, source);
		}

		#if CUI_ENABLE_DYNAMIC_XAML
		static bool ClearValue(
			Control& target,
			const std::wstring& propertyName,
			DependencyPropertyValueSource source)
		{
			return target.ClearPropertyValue(propertyName, source);
		}
		#endif

		static bool ClearValue(
			Control& target,
			const DependencyProperty& property,
			DependencyPropertyValueSource source)
		{
			return target.ClearPropertyValue(property, source);
		}

		static size_t ClearValues(
			Control& target, DependencyPropertyValueSource source)
		{
			return target.ClearPropertyValues(source);
		}

		#if CUI_ENABLE_DYNAMIC_XAML
		static bool SetBaseValue(
			Control& target,
			const std::wstring& propertyName,
			const BindingValue& value)
		{
			return target.TrySetPropertyBaseValue(propertyName, value);
		}
		#endif

		/** Writes a framework-owned read-only dependency property by key. */
		static bool SetReadOnlyValue(
			Control& target,
			const DependencyPropertyKey& key,
			const BindingValue& value)
		{
			return target.TrySetReadOnlyPropertyValue(key, value);
		}

		#if CUI_ENABLE_DYNAMIC_XAML
		static bool SetDynamicResource(
			Control& target,
			const std::wstring& propertyName,
			std::wstring resourceKey,
			DependencyPropertyValueSource source)
		{
			return target.SetDynamicResource(
				propertyName, std::move(resourceKey), source);
		}
		#endif

		static bool SetDynamicResource(
			Control& target,
			const DependencyProperty& property,
			std::wstring resourceKey,
			DependencyPropertyValueSource source)
		{
			return target.SetDynamicResource(
				property, std::move(resourceKey), source);
		}

		#if CUI_ENABLE_DYNAMIC_XAML
		static bool ClearDynamicResource(
			Control& target,
			const std::wstring& propertyName,
			DependencyPropertyValueSource source)
		{
			return target.ClearDynamicResource(propertyName, source);
		}
		#endif

		static bool ClearDynamicResource(
			Control& target,
			const DependencyProperty& property,
			DependencyPropertyValueSource source)
		{
			return target.ClearDynamicResource(property, source);
		}
	};
}
