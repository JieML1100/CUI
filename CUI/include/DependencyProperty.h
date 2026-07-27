#ifndef CUI_DEPENDENCY_PROPERTY_H_INCLUDED
#define CUI_DEPENDENCY_PROPERTY_H_INCLUDED
#pragma once

#include "DependencyObject.h"

#include <algorithm>
#include <any>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace cui::property_system_detail
{
	template<typename TValue>
	constexpr BindingValueKind ValueKind() noexcept
	{
		using Value = std::remove_cvref_t<TValue>;
		if constexpr (std::is_same_v<Value, BindingValue>)
			return BindingValueKind::Object;
		else if constexpr (std::is_same_v<Value, bool>)
			return BindingValueKind::Bool;
		else if constexpr (std::is_same_v<Value, int>)
			return BindingValueKind::Int;
		else if constexpr (std::is_same_v<Value, long long>)
			return BindingValueKind::Int64;
		else if constexpr (std::is_same_v<Value, float>)
			return BindingValueKind::Float;
		else if constexpr (std::is_same_v<Value, double>)
			return BindingValueKind::Double;
		else if constexpr (std::is_same_v<Value, std::wstring>)
			return BindingValueKind::String;
		else if constexpr (std::is_enum_v<Value>)
		{
			using Underlying = std::underlying_type_t<Value>;
			constexpr bool fitsInt = sizeof(Underlying) < sizeof(int)
				|| (sizeof(Underlying) == sizeof(int)
					&& std::is_signed_v<Underlying>);
			return fitsInt ? BindingValueKind::Int : BindingValueKind::Int64;
		}
		else
			return BindingValueKind::Object;
	}

	/**
	 * Native enum storage is an implementation detail. The dependency-property
	 * interchange contract exposes every enum as a canonical signed number plus
	 * its metadata Choices, so XAML tooling never needs a C++ enum whitelist.
	 */
	template<typename TValue>
	BindingValue Pack(TValue&& value)
	{
		using Value = std::remove_cvref_t<TValue>;
		if constexpr (std::is_enum_v<Value>)
		{
			if constexpr (ValueKind<Value>() == BindingValueKind::Int)
				return BindingValue(static_cast<int>(value));
			else
				return BindingValue(static_cast<long long>(value));
		}
		else
		{
			return BindingValue(std::forward<TValue>(value));
		}
	}
}

template<typename TOwner, typename TValue>
DependencyPropertyMetadata DependencyPropertyRegistry::CreateMetadata(
	std::wstring name,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options,
	bool usesEffectiveValueStorage,
	bool includeValidator)
{
	static_assert(std::is_base_of_v<DependencyObject, TOwner>,
		"Bindable property owners must derive from DependencyObject.");

	constexpr BindingValueKind valueKind =
		cui::property_system_detail::ValueKind<TValue>();

	auto matcher = [](const DependencyObject& target)
	{
		return dynamic_cast<const TOwner*>(&target) != nullptr;
	};

	auto customValueConverter = std::move(options.Convert);
	DependencyPropertyMetadata::ValueConverter valueConverter = [
		customValueConverter = std::move(customValueConverter)](
		const BindingValue& value,
		BindingValue& out)
	{
		using Value = std::remove_cv_t<TValue>;
		if (customValueConverter)
		{
			auto converted = customValueConverter(value);
			if (!converted.has_value()) return false;
			out = cui::property_system_detail::Pack(
				std::move(*converted));
			return true;
		}
		if constexpr (std::is_same_v<Value, BindingValue>)
		{
			out = value;
			return true;
		}
		else if constexpr (std::is_default_constructible_v<Value>)
		{
			Value converted{};
			if (!value.TryGet(converted)) return false;
			out = cui::property_system_detail::Pack(std::move(converted));
			return true;
		}
		else
		{
			if (value.Kind() != BindingValueKind::Object) return false;
			const auto* exact = std::any_cast<Value>(
				&std::get<std::any>(value.Raw()));
			if (!exact) return false;
			out = BindingValue(*exact);
			return true;
		}
	};

	DependencyPropertyMetadata::Validator untypedValidator;
	if (includeValidator && options.Validate)
	{
		untypedValidator = [validate = std::move(options.Validate)](
			const BindingValue& value)
		{
			using Value = std::remove_cv_t<TValue>;
			if constexpr (std::is_same_v<Value, BindingValue>)
			{
				return validate(value);
			}
			else if constexpr (std::is_default_constructible_v<Value>)
			{
				Value converted{};
				return value.TryGet(converted) && validate(converted);
			}
			else
			{
				if (value.Kind() != BindingValueKind::Object) return false;
				const auto* exact = std::any_cast<Value>(
					&std::get<std::any>(value.Raw()));
				return exact && validate(*exact);
			}
		};
	}

	DependencyPropertyMetadata::Getter untypedGetter;
	if (getter)
	{
		untypedGetter = [getter = std::move(getter)](DependencyObject& target, BindingValue& out)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
				out = getter(*owner);
			else
				out = cui::property_system_detail::Pack(getter(*owner));
			return true;
		};
	}

	DependencyPropertyMetadata::Setter untypedSetter;
	if (setter)
	{
		untypedSetter = [setter = std::move(setter)](DependencyObject& target, const BindingValue& value)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			{
				setter(*owner, value);
				return true;
			}
			else if constexpr (std::is_default_constructible_v<TValue>)
			{
				TValue converted{};
				if (!value.TryGet(converted)) return false;
				setter(*owner, converted);
				return true;
			}
			else
			{
				if (value.Kind() != BindingValueKind::Object) return false;
				const auto* exact = std::any_cast<TValue>(
					&std::get<std::any>(value.Raw()));
				if (!exact) return false;
				setter(*owner, *exact);
				return true;
			}
		};
	}

	DependencyPropertyMetadata::Subscriber untypedSubscriber;
	if (subscriber)
	{
		untypedSubscriber = [subscriber = std::move(subscriber)](
			DependencyObject& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode updateMode)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			return owner
				? subscriber(*owner, std::move(handler), updateMode)
				: EventConnection{};
		};
	}
	else
	{
		// Dependency properties are observable by definition.  Native owners no
		// longer need to repeat a per-property forwarding subscriber merely to
		// participate in Binding, Trigger, or designer observation.
		auto observedName = name;
		untypedSubscriber = [observedName = std::move(observedName)](
			DependencyObject& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode updateMode)
		{
			return target.SubscribeDefaultPropertyChange(
				observedName, std::move(handler), updateMode);
		};
	}

	DependencyPropertyMetadata::Coercer untypedCoercer;
	if (options.Coerce)
	{
		untypedCoercer = [coerce = std::move(options.Coerce)](
			DependencyObject& target,
			const BindingValue& value,
			BindingValue& out)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			std::optional<TValue> proposed;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
				proposed = value;
			else if constexpr (std::is_default_constructible_v<TValue>)
			{
				TValue converted{};
				if (value.TryGet(converted)) proposed = std::move(converted);
			}
			else if (value.Kind() == BindingValueKind::Object)
			{
				if (const auto* exact = std::any_cast<TValue>(
					&std::get<std::any>(value.Raw())))
					proposed = *exact;
			}
			if (!proposed.has_value()) return false;
			auto coerced = coerce(*owner, *proposed);
			if (!coerced.has_value()) return false;
			out = cui::property_system_detail::Pack(std::move(*coerced));
			return true;
		};
	}

	DependencyPropertyMetadata::Comparer typedComparer =
		[equals = std::move(options.Equals)](
		const BindingValue& left,
		const BindingValue& right) -> bool
	{
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
		{
			if (equals) return equals(left, right);
			return BindingValuesEqual(left, right);
		}
		else if constexpr (std::is_default_constructible_v<TValue>)
		{
			TValue leftValue{};
			TValue rightValue{};
			if (!left.TryGet(leftValue) || !right.TryGet(rightValue)) return false;
			if (equals) return equals(leftValue, rightValue);
			if constexpr (requires(const TValue& a, const TValue& b)
				{ { a == b } -> std::convertible_to<bool>; })
			{
				return leftValue == rightValue;
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (left.Kind() != BindingValueKind::Object
				|| right.Kind() != BindingValueKind::Object)
				return false;
			const auto* leftValue = std::any_cast<TValue>(
				&std::get<std::any>(left.Raw()));
			const auto* rightValue = std::any_cast<TValue>(
				&std::get<std::any>(right.Raw()));
			if (!leftValue || !rightValue) return false;
			if (equals) return equals(*leftValue, *rightValue);
			if constexpr (requires(const TValue& a, const TValue& b)
				{ { a == b } -> std::convertible_to<bool>; })
				return *leftValue == *rightValue;
			return false;
		}
	};

	DependencyPropertyMetadata::Changed untypedChanged;
	if (options.Changed)
	{
		untypedChanged = [changed = std::move(options.Changed)](
			DependencyObject& target,
			const BindingValue& oldValue,
			const BindingValue& newValue)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
				changed(*owner, oldValue, newValue);
			else if constexpr (std::is_default_constructible_v<TValue>)
			{
				TValue typedOld{};
				TValue typedNew{};
				if (oldValue.TryGet(typedOld) && newValue.TryGet(typedNew))
					changed(*owner, typedOld, typedNew);
			}
			else if (oldValue.Kind() == BindingValueKind::Object
				&& newValue.Kind() == BindingValueKind::Object)
			{
				const auto* typedOld = std::any_cast<TValue>(
					&std::get<std::any>(oldValue.Raw()));
				const auto* typedNew = std::any_cast<TValue>(
					&std::get<std::any>(newValue.Raw()));
				if (typedOld && typedNew) changed(*owner, *typedOld, *typedNew);
			}
		};
	}

	BindingValue defaultValue;
	bool hasDefaultValue = options.DefaultValue.has_value();
	if (hasDefaultValue)
	{
		defaultValue = cui::property_system_detail::Pack(
			std::move(*options.DefaultValue));
	}
	else if (usesEffectiveValueStorage)
	{
		if constexpr (std::is_default_constructible_v<TValue>)
		{
			defaultValue =
				cui::property_system_detail::Pack(TValue{});
			hasDefaultValue = true;
		}
		else
		{
			throw std::invalid_argument(
				"Slot-backed dependency properties require a default value");
		}
	}
	if (hasDefaultValue
		&& untypedValidator && !untypedValidator(defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");

	return DependencyPropertyMetadata(
		std::move(name),
		valueKind,
		std::type_index(typeid(TValue)),
		std::type_index(typeid(TOwner)),
		std::move(matcher),
		std::move(valueConverter),
		std::move(untypedValidator),
		std::move(untypedCoercer),
		std::move(typedComparer),
		std::move(untypedGetter),
		std::move(untypedSetter),
		std::move(untypedSubscriber),
		std::move(untypedChanged),
		std::move(defaultValue),
		hasDefaultValue,
		usesEffectiveValueStorage,
		options.Flags,
		options.IsReadOnly,
		options.DefaultUpdateMode,
		{},
		std::move(options.Design));
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::Register(
	std::wstring name,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	if (options.IsReadOnly)
		throw std::invalid_argument(
			"Use RegisterReadOnly to register a read-only dependency property");
	return Register(CreateMetadata<TOwner, TValue>(
		std::move(name), {}, {}, {}, std::move(options), true, true));
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::Register(
	std::wstring name,
	std::function<EventConnection(
		TOwner&,
		DependencyPropertyMetadata::ChangeHandler,
		DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	if (options.IsReadOnly)
		throw std::invalid_argument(
			"Use RegisterReadOnly to register a read-only dependency property");
	return Register(CreateMetadata<TOwner, TValue>(
		std::move(name), {}, {}, std::move(subscriber),
		std::move(options), true, true));
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::Register(
	std::wstring name,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(
		TOwner&,
		DependencyPropertyMetadata::ChangeHandler,
		DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	if (options.IsReadOnly)
		throw std::invalid_argument(
			"Use RegisterReadOnly to register a read-only dependency property");
	return Register(CreateMetadata<TOwner, TValue>(
		std::move(name), std::move(getter), std::move(setter),
		std::move(subscriber), std::move(options), false, true));
}

template<typename TOwner, typename TValue>
DependencyPropertyKey DependencyPropertyRegistry::RegisterReadOnly(
	std::wstring name,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	options.IsReadOnly = true;
	return RegisterReadOnly(CreateMetadata<TOwner, TValue>(
		std::move(name), {}, {}, {}, std::move(options), true, true));
}

template<typename TOwner, typename TValue>
DependencyPropertyKey DependencyPropertyRegistry::RegisterReadOnly(
	std::wstring name,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(
		TOwner&,
		DependencyPropertyMetadata::ChangeHandler,
		DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	options.IsReadOnly = true;
	return RegisterReadOnly(CreateMetadata<TOwner, TValue>(
		std::move(name), std::move(getter), std::move(setter),
		std::move(subscriber), std::move(options), false, true));
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::AddOwner(
	const DependencyProperty& property,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	if (options.Validate)
		throw std::invalid_argument(
			"ValidateValue belongs to the DependencyProperty identity");
	options.IsReadOnly = property.ReadOnly();
	auto metadata = CreateMetadata<TOwner, TValue>(
		property.Name(), {}, {}, {}, std::move(options), true, false);
	return AddOwner(property, std::move(metadata), nullptr)
		? &property : nullptr;
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::AddOwner(
	const DependencyProperty& property,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(
		TOwner&,
		DependencyPropertyMetadata::ChangeHandler,
		DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	if (options.Validate)
		throw std::invalid_argument(
			"ValidateValue belongs to the DependencyProperty identity");
	options.IsReadOnly = property.ReadOnly();
	auto metadata = CreateMetadata<TOwner, TValue>(
		property.Name(), std::move(getter), std::move(setter),
		std::move(subscriber), std::move(options), false, false);
	return AddOwner(property, std::move(metadata), nullptr)
		? &property : nullptr;
}

template<typename TOwner, typename TValue>
const DependencyPropertyMetadata* DependencyPropertyRegistry::OverrideMetadata(
	const DependencyProperty& property,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	if (options.Validate)
		throw std::invalid_argument(
			"ValidateValue belongs to the DependencyProperty identity");
	options.IsReadOnly = property.ReadOnly();
	auto metadata = CreateMetadata<TOwner, TValue>(
		property.Name(), {}, {}, {}, std::move(options), false, false);
	return OverrideMetadata(property, std::move(metadata), nullptr);
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::AddOwner(
	const DependencyPropertyKey& key,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	const auto& property = key.Property();
	if (options.Validate)
		throw std::invalid_argument(
			"ValidateValue belongs to the DependencyProperty identity");
	options.IsReadOnly = true;
	auto metadata = CreateMetadata<TOwner, TValue>(
		property.Name(), {}, {}, {}, std::move(options), true, false);
	return AddOwner(property, std::move(metadata), &key)
		? &property : nullptr;
}

template<typename TOwner, typename TValue>
const DependencyProperty* DependencyPropertyRegistry::AddOwner(
	const DependencyPropertyKey& key,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(
		TOwner&,
		DependencyPropertyMetadata::ChangeHandler,
		DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	const auto& property = key.Property();
	if (options.Validate)
		throw std::invalid_argument(
			"ValidateValue belongs to the DependencyProperty identity");
	options.IsReadOnly = true;
	auto metadata = CreateMetadata<TOwner, TValue>(
		property.Name(), std::move(getter), std::move(setter),
		std::move(subscriber), std::move(options), false, false);
	return AddOwner(property, std::move(metadata), &key)
		? &property : nullptr;
}

template<typename TOwner, typename TValue>
const DependencyPropertyMetadata* DependencyPropertyRegistry::OverrideMetadata(
	const DependencyPropertyKey& key,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	const auto& property = key.Property();
	if (options.Validate)
		throw std::invalid_argument(
			"ValidateValue belongs to the DependencyProperty identity");
	options.IsReadOnly = true;
	auto metadata = CreateMetadata<TOwner, TValue>(
		property.Name(), {}, {}, {}, std::move(options), false, false);
	return OverrideMetadata(property, std::move(metadata), &key);
}

template<typename TValue>
bool DependencyObject::SetPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	auto* metadata = DependencyPropertyRegistry::Find(*this, propertyName);
	// A missing effective metadata entry means that the projected XAML/WPF
	// type does not own this property.  The private C++ behavior-host
	// inheritance graph must never turn that into an untracked side channel.
	// Native implementation state which is intentionally not a dependency
	// property is written explicitly and must not use this helper.
	if (!metadata) return false;
	BindingValue proposed =
		cui::property_system_detail::Pack(std::move(value));
	if (_applyingPropertyMetadata == metadata)
	{
		TValue typed = storage;
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			typed = proposed;
		else if (!proposed.TryGet(typed)) return false;

		BindingValue oldValue = cui::property_system_detail::Pack(storage);
		BindingValue newValue = cui::property_system_detail::Pack(typed);
		if (metadata->ValuesEqual(oldValue, newValue)) return true;
		storage = std::move(typed);
		ApplyPropertyMetadataChange(*metadata, oldValue, newValue);
		return true;
	}
	// A public CLR-style wrapper is exactly SetValue in WPF terms.  Internal
	// behavior that must preserve an expression uses SetCurrentPropertyField;
	// metadata application is handled by the guarded branch above.
	return TrySetPropertyValue(
		propertyName, proposed, DependencyPropertyValueSource::Local);
}

template<typename TValue>
bool DependencyObject::SetCurrentPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	(void)storage;
	auto* metadata = DependencyPropertyRegistry::Find(*this, propertyName);
	// SetCurrentValue has the same ownership boundary as SetValue: it may
	// preserve an existing expression, but it cannot manufacture a property
	// which the projected type does not expose.
	if (!metadata) return false;
	return TrySetCurrentPropertyValue(
		propertyName, BindingValue(std::move(value)));
}

template<typename TValue>
bool DependencyObject::SetReadOnlyPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	auto* metadata = DependencyPropertyRegistry::Find(*this, propertyName);
	if (!metadata || !metadata->IsReadOnly()) return false;
	BindingValue proposed =
		cui::property_system_detail::Pack(std::move(value));
	if (_applyingPropertyMetadata == metadata)
	{
		TValue typed = storage;
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			typed = proposed;
		else if (!proposed.TryGet(typed)) return false;

		BindingValue oldValue = cui::property_system_detail::Pack(storage);
		BindingValue newValue = cui::property_system_detail::Pack(typed);
		if (metadata->ValuesEqual(oldValue, newValue)) return true;
		storage = std::move(typed);
		ApplyPropertyMetadataChange(*metadata, oldValue, newValue);
		return true;
	}
	return TrySetReadOnlyPropertyValue(propertyName, proposed);
}

template<typename TValue>
bool DependencyObject::SetReadOnlyPropertyField(
	const DependencyPropertyKey& key,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	auto* metadata =
		DependencyPropertyRegistry::GetMetadata(*this, key.Property());
	if (!metadata || !metadata->IsReadOnly()) return false;
	BindingValue proposed =
		cui::property_system_detail::Pack(std::move(value));
	if (_applyingPropertyMetadata == metadata)
	{
		TValue typed = storage;
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			typed = proposed;
		else if (!proposed.TryGet(typed)) return false;

		BindingValue oldValue = cui::property_system_detail::Pack(storage);
		BindingValue newValue = cui::property_system_detail::Pack(typed);
		if (metadata->ValuesEqual(oldValue, newValue)) return true;
		storage = std::move(typed);
		ApplyPropertyMetadataChange(*metadata, oldValue, newValue);
		return true;
	}
	return TrySetReadOnlyPropertyValue(key, proposed);
}

template<typename TValue>
TValue DependencyObject::GetDependencyPropertyValue(
	const std::wstring& propertyName) const
{
	BindingValue value;
	if (!const_cast<DependencyObject*>(this)->TryGetPropertyValue(
		propertyName, value))
		throw std::logic_error(
			"Dependency property is not readable on this object");
	if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
	{
		return value;
	}
	else
	{
		static_assert(std::is_default_constructible_v<TValue>,
			"Typed dependency-property wrappers require a default-constructible value");
		TValue typed{};
		if (!value.TryGet(typed))
			throw std::logic_error(
				"Dependency property value does not match its CLR wrapper type");
		return typed;
	}
}

template<typename TValue>
TValue DependencyObject::GetDependencyPropertyValue(
	const DependencyProperty& property) const
{
	BindingValue value;
	if (!const_cast<DependencyObject*>(this)->TryGetPropertyValue(
		property, value))
		throw std::logic_error(
			"Dependency property is not readable on this object");
	if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
	{
		return value;
	}
	else
	{
		static_assert(std::is_default_constructible_v<TValue>,
			"Typed dependency-property wrappers require a default-constructible value");
		TValue typed{};
		if (!value.TryGet(typed))
			throw std::logic_error(
				"Dependency property value does not match its CLR wrapper type");
		return typed;
	}
}

template<typename TValue>
bool DependencyObject::SetDependencyPropertyValue(
	const std::wstring& propertyName,
	TValue value)
{
	return TrySetPropertyValue(
		propertyName,
		cui::property_system_detail::Pack(std::move(value)));
}

template<typename TValue>
bool DependencyObject::SetDependencyPropertyValue(
	const DependencyProperty& property,
	TValue value)
{
	return TrySetPropertyValue(
		property,
		cui::property_system_detail::Pack(std::move(value)));
}

#endif // CUI_DEPENDENCY_PROPERTY_H_INCLUDED
