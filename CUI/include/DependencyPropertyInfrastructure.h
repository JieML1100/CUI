#pragma once

#include "Control.h"

#include <utility>

namespace cui::framework
{
	/** Framework-only writer for non-Local dependency-property precedence slots. */
	struct DependencyPropertyAccess final
	{
		DependencyPropertyAccess() = delete;

		static bool SetValue(
			Control& target,
			const std::wstring& propertyName,
			const BindingValue& value,
			DependencyPropertyValueSource source)
		{
			return target.TrySetPropertyValue(propertyName, value, source);
		}

		static bool ClearValue(
			Control& target,
			const std::wstring& propertyName,
			DependencyPropertyValueSource source)
		{
			return target.ClearPropertyValue(propertyName, source);
		}

		static size_t ClearValues(
			Control& target, DependencyPropertyValueSource source)
		{
			return target.ClearPropertyValues(source);
		}

		static bool SetBaseValue(
			Control& target,
			const std::wstring& propertyName,
			const BindingValue& value)
		{
			return target.TrySetPropertyBaseValue(propertyName, value);
		}

		static bool SetDynamicResource(
			Control& target,
			const std::wstring& propertyName,
			std::wstring resourceKey,
			DependencyPropertyValueSource source)
		{
			return target.SetDynamicResource(
				propertyName, std::move(resourceKey), source);
		}

		static bool ClearDynamicResource(
			Control& target,
			const std::wstring& propertyName,
			DependencyPropertyValueSource source)
		{
			return target.ClearDynamicResource(propertyName, source);
		}
	};
}
