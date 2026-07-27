#include "DependencyObject.h"
#include "EventInfrastructure.h"

namespace
{
	int StoredPropertySourceIndex(
		DependencyPropertyValueSource source) noexcept
	{
		const int value = static_cast<int>(source);
		return value >= static_cast<int>(
			DependencyPropertyValueSource::Inherited)
			&& value <= static_cast<int>(
				DependencyPropertyValueSource::Animation)
			? value - static_cast<int>(
				DependencyPropertyValueSource::Inherited)
			: -1;
	}
}

DependencyObject::~DependencyObject()
{
	InvalidateLifetimeToken();
	_isDestroying = true;
	_bindingSourceMetadataConnections.clear();
	_dataBindings.reset();
	_propertyValues.clear();
}

void DependencyObject::ApplyPropertyMetadataChange(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& oldValue,
	const BindingValue& newValue)
{
	DependencyObject* const self = this;
	const auto lifetime = WeakLifetimeToken();
	const auto isAlive = [&lifetime]
	{
		const auto token = lifetime.lock();
		return token && *token;
	};

	++_propertyChangeVersion;
	metadata.NotifyChanged(*this, oldValue, newValue);
	if (!isAlive()) return;

	DependencyPropertyChangedEventArgs args{
		metadata.Name(), oldValue, newValue, &metadata.Property() };
	cui::framework::EventAccess::Raise(
		self->OnPropertyValueChanged, self, args);
	if (!isAlive()) return;
	self->_bindingSourcePropertyChanged.Notify(metadata.Name());
}

EventConnection DependencyObject::SubscribeDefaultPropertyChange(
	const std::wstring& propertyName,
	DependencyPropertyChangeHandler handler,
	DataSourceUpdateMode updateMode)
{
	(void)updateMode;
	return OnPropertyValueChanged.Subscribe(
		[propertyName, handler = std::move(handler)](
			DependencyObject*,
			const DependencyPropertyChangedEventArgs& args)
		{
			if (args.PropertyName == propertyName)
				handler();
		});
}

const DependencyProperty* DependencyObject::FindDependencyProperty(
	const std::wstring& propertyName)
{
	EnsureBindingPropertiesRegistered();
	return DependencyPropertyRegistry::FindProperty(*this, propertyName);
}

const DependencyPropertyMetadata* DependencyObject::FindPropertyMetadata(
	const std::wstring& propertyName)
{
	EnsureBindingPropertiesRegistered();
	return DependencyPropertyRegistry::Find(*this, propertyName);
}

const DependencyPropertyMetadata* DependencyObject::GetPropertyMetadata(
	const DependencyProperty& property)
{
	EnsureBindingPropertiesRegistered();
	return DependencyPropertyRegistry::GetMetadata(*this, property);
}

bool DependencyObject::TryGetPropertyValue(
	const std::wstring& propertyName,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && metadata->TryGet(*this, out);
}

bool DependencyObject::TryGetPropertyValue(
	const DependencyProperty& property,
	BindingValue& out)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && metadata->TryGet(*this, out);
}

bool DependencyObject::TryGetPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	if (source == DependencyPropertyValueSource::Default)
		return metadata->TryGetDefaultValue(out);
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end()
		|| !entry->second.Slots[(size_t)index].ProposedValue.has_value())
		return false;
	out = *entry->second.Slots[(size_t)index].ProposedValue;
	return true;
}

bool DependencyObject::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return TrySetPropertyValue(
		propertyName, value, DependencyPropertyValueSource::Local);
}

bool DependencyObject::TrySetPropertyValue(
	const DependencyProperty& property,
	const BindingValue& value)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata
		&& TrySetPropertyValueOwned(
			metadata->Name(), value,
			DependencyPropertyValueSource::Local, nullptr);
}

bool DependencyObject::TrySetPropertyValue(
	const DependencyPropertyKey& key,
	const BindingValue& value)
{
	return TrySetReadOnlyPropertyValue(key, value);
}

bool DependencyObject::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	DependencyPropertyValueSource source)
{
	return TrySetPropertyValueOwned(propertyName, value, source, nullptr);
}

bool DependencyObject::TrySetPropertyValueOwned(
	const std::wstring& propertyName,
	const BindingValue& value,
	DependencyPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0
		|| (!metadata->CanWrite()
			&& !(allowReadOnly && metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;

	if (owner) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;
	return TrySetEffectiveValueEntry(
		*metadata, std::move(converted), source,
		source == DependencyPropertyValueSource::Animation
			? DependencyPropertyExpressionKind::Animation
			: DependencyPropertyExpressionKind::None,
		nullptr, {}, allowReadOnly);
}

bool DependencyObject::TrySetEffectiveValueEntry(
	const DependencyPropertyMetadata& metadata,
	std::optional<BindingValue> proposedValue,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind,
	const Binding* owner,
	std::wstring resourceKey,
	bool allowReadOnly)
{
	VerifyAccess();
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (!metadata.CanWrite()
			&& !(allowReadOnly && metadata.IsReadOnly()
				&& metadata.CanWriteInternally()))) return false;
	if (proposedValue && !metadata.IsValidValue(*proposedValue))
		return false;

	const bool bindingExpression =
		expressionKind == DependencyPropertyExpressionKind::Binding
		|| expressionKind == DependencyPropertyExpressionKind::TemplateBinding;
	const bool validExpression =
		(expressionKind == DependencyPropertyExpressionKind::None
			&& proposedValue.has_value() && !owner && resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::Binding
			&& source == DependencyPropertyValueSource::Local
			&& owner && resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::TemplateBinding
			&& source == DependencyPropertyValueSource::Template
			&& owner && resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::DynamicResource
			&& source != DependencyPropertyValueSource::Animation
			&& !owner && !resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::Animation
			&& source == DependencyPropertyValueSource::Animation
			&& proposedValue.has_value() && !owner && resourceKey.empty());
	if (!validExpression) return false;

	// Slot-backed properties keep their previously applied effective value in
	// the property engine so callbacks can observe a transactional old/new
	// pair without consulting a CLR backing field.
	if (metadata.UsesEffectiveValueStorage())
	{
		BindingValue ignored;
		if (!TryGetEffectivePropertyValue(metadata, ignored)) return false;
	}

	auto [entryIt, inserted] =
		_propertyValues.try_emplace(&metadata.Property());
	auto& entry = entryIt->second;
	if (inserted)
	{
		// A Local value/expression replaces the previous local state. Its
		// fallback is metadata default, never a hidden copy resurrected later.
		entry.HasBaseValue = source == DependencyPropertyValueSource::Local
			&& metadata.TryGetDefaultValue(entry.BaseValue);
		if (!entry.HasBaseValue)
			entry.HasBaseValue = metadata.TryGet(*this, entry.BaseValue);
		if (!entry.HasBaseValue)
			entry.HasBaseValue = metadata.TryGetDefaultValue(entry.BaseValue);
	}

	const size_t sourceIndex = static_cast<size_t>(index);
	auto& slot = entry.Slots[sourceIndex];
	const bool previousWasBinding =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	if (bindingExpression && previousWasBinding
		&& slot.BindingOwner && slot.BindingOwner != owner)
	{
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	BindingValue oldEffective;
	DependencyPropertyValueSource oldSource = DependencyPropertyValueSource::Default;
	const bool hadOldEffective = metadata.UsesEffectiveValueStorage()
		&& entry.HasEffectiveValue
		? (oldEffective = entry.EffectiveValue,
			oldSource = entry.EffectiveSource, true)
		: TryEvaluateEffectivePropertyValue(
			metadata, entry, oldEffective, oldSource);
	const auto previousSlot = slot;
	const Binding* retiredBinding = previousWasBinding
		&& (slot.BindingOwner != owner || slot.Expression != expressionKind)
		? slot.BindingOwner : nullptr;

	slot.ProposedValue = std::move(proposedValue);
	slot.Expression = expressionKind;
	slot.BindingOwner = bindingExpression ? owner : nullptr;
	slot.ResourceKey = expressionKind == DependencyPropertyExpressionKind::DynamicResource
		? std::move(resourceKey) : std::wstring{};

	BindingValue newEffective;
	DependencyPropertyValueSource newSource = DependencyPropertyValueSource::Default;
	if (!TryEvaluateEffectivePropertyValue(
		metadata, entry, newEffective, newSource))
	{
		slot = previousSlot;
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	BindingValue currentEffective;
	const bool backingStorageMatches = metadata.UsesEffectiveValueStorage()
		? entry.HasEffectiveValue
			&& metadata.ValuesEqual(entry.EffectiveValue, newEffective)
		: metadata.TryGet(*this, currentEffective)
			&& metadata.ValuesEqual(currentEffective, newEffective);
	const bool effectiveUnchanged = hadOldEffective
		&& oldSource == newSource
		&& metadata.ValuesEqual(oldEffective, newEffective)
		&& backingStorageMatches;
	if (!effectiveUnchanged
		&& !ApplyEffectivePropertyValue(
			metadata, newEffective, newSource, allowReadOnly))
	{
		slot = previousSlot;
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	if (retiredBinding)
		RetireBindingExpression(metadata.Name(), retiredBinding);
	return true;
}

bool DependencyObject::CanAcquireBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (expressionKind == DependencyPropertyExpressionKind::Binding
			&& source != DependencyPropertyValueSource::Local)
		|| (expressionKind == DependencyPropertyExpressionKind::TemplateBinding
			&& source != DependencyPropertyValueSource::Template))
		return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end()) return true;
	const auto& slot = entry->second.Slots[(size_t)index];
	const bool isBindingExpression =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	return !isBindingExpression || !slot.BindingOwner
		|| slot.BindingOwner == owner;
}

bool DependencyObject::TryAttachBindingPropertyExpression(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!CanAcquireBindingPropertyValue(
		propertyName, owner, source, expressionKind)) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && TrySetEffectiveValueEntry(
		*metadata, std::nullopt, source, expressionKind, owner, {}, false);
}

bool DependencyObject::TrySetBindingPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !IsBindingExpressionOwner(
		propertyName, owner, source, expressionKind)) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;
	return TrySetEffectiveValueEntry(
		*metadata, std::move(converted), source,
		expressionKind, owner, {}, false);
}

bool DependencyObject::TrySetCurrentPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;

	BindingValue converted;
	BindingValue coerced;
	if (!metadata->TryConvert(value, converted)
		|| !metadata->TryCoerce(*this, converted, coerced)) return false;
	BindingValue current;
	if (metadata->TryGet(*this, current)
		&& metadata->ValuesEqual(current, coerced)) return true;

	DependencyPropertyValueSource source =
		DependencyPropertyValueSource::Default;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry != _propertyValues.end())
	{
		BindingValue ignored;
		(void)TryResolveEffectivePropertyValue(
			*metadata, entry->second, ignored, source);
		const int sourceIndex = StoredPropertySourceIndex(source);
		const auto* slot = sourceIndex < 0 ? nullptr
			: &entry->second.Slots[static_cast<size_t>(sourceIndex)];
		if (slot
			&& (slot->Expression == DependencyPropertyExpressionKind::Binding
				|| slot->Expression
					== DependencyPropertyExpressionKind::DynamicResource
				|| slot->Expression
					== DependencyPropertyExpressionKind::TemplateBinding))
		{
			return TrySetEffectiveValueEntry(
				*metadata, std::move(converted),
				source, slot->Expression, slot->BindingOwner,
				slot->ResourceKey, false);
		}
	}

	// WPF SetCurrentValue changes the value carried by the existing source;
	// it never promotes a Default/Style/Template/Inherited value to Local.
	// A later update from that source therefore remains authoritative.
	return source == DependencyPropertyValueSource::Default
		? TrySetPropertyBaseValue(propertyName, converted)
		: TrySetPropertyValue(propertyName, converted, source);
}

bool DependencyObject::TrySetCurrentPropertyValue(
	const DependencyProperty& property,
	const BindingValue& value)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata
		&& TrySetCurrentPropertyValue(metadata->Name(), value);
}

bool DependencyObject::TrySetReadOnlyPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	// Name-based framework access exists only for a declarative component's
	// behavior host. Native read-only properties require the unforgeable key
	// returned by RegisterReadOnly.
	if (!metadata || !metadata->IsReadOnly()
		|| FindDeclarativePropertyMetadata(metadata->Name()) != metadata)
		return false;
	return TrySetPropertyValueOwned(
		metadata->Name(), value, DependencyPropertyValueSource::Local, nullptr, true);
}

bool DependencyObject::TrySetReadOnlyPropertyValue(
	const DependencyPropertyKey& key,
	const BindingValue& value)
{
	const auto& property = key.Property();
	if (!property.Authorizes(key)) return false;
	const auto* metadata = GetPropertyMetadata(property);
	return metadata
		&& TrySetPropertyValueOwned(
			metadata->Name(), value,
			DependencyPropertyValueSource::Local, nullptr, true);
}

bool DependencyObject::ClearReadOnlyPropertyValue(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->IsReadOnly()
		|| FindDeclarativePropertyMetadata(metadata->Name()) != metadata)
		return false;
	return ClearPropertyValueOwned(
		metadata->Name(), DependencyPropertyValueSource::Local, nullptr, true);
}

bool DependencyObject::ClearReadOnlyPropertyValue(
	const DependencyPropertyKey& key)
{
	const auto& property = key.Property();
	if (!property.Authorizes(key)) return false;
	const auto* metadata = GetPropertyMetadata(property);
	return metadata
		&& ClearPropertyValueOwned(
			metadata->Name(), DependencyPropertyValueSource::Local,
			nullptr, true);
}

bool DependencyObject::CoerceValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata
		|| (!metadata->CanWrite()
			&& !(metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;
	const bool allowReadOnly = metadata->IsReadOnly();

	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry != _propertyValues.end())
	{
		BindingValue effective;
		DependencyPropertyValueSource source = DependencyPropertyValueSource::Default;
		if (!TryEvaluateEffectivePropertyValue(
			*metadata, entry->second, effective, source)) return false;
		BindingValue current;
		if (metadata->TryGet(*this, current)
			&& metadata->ValuesEqual(current, effective)) return true;
		return ApplyEffectivePropertyValue(
			*metadata, effective, source, allowReadOnly);
	}

	BindingValue proposed;
	if (!metadata->TryGetDefaultValue(proposed)
		&& !metadata->TryGet(*this, proposed)) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(proposed, converted)
		|| !metadata->TryCoerce(*this, converted, effective)) return false;

	BindingValue current;
	if (metadata->TryGet(*this, current)
		&& metadata->ValuesEqual(current, effective)) return true;
	return ApplyEffectivePropertyValue(
		*metadata, effective, DependencyPropertyValueSource::Default,
		allowReadOnly);
}

bool DependencyObject::CoerceValue(const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && CoerceValue(metadata->Name());
}

bool DependencyObject::ClearPropertyValue(
	const std::wstring& propertyName)
{
	return ClearPropertyValue(
		propertyName, DependencyPropertyValueSource::Local);
}

bool DependencyObject::ClearPropertyValue(
	const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && ClearPropertyValue(metadata->Name());
}

bool DependencyObject::ClearPropertyValue(
	const DependencyPropertyKey& key)
{
	return ClearReadOnlyPropertyValue(key);
}

bool DependencyObject::ClearPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	return ClearPropertyValueOwned(propertyName, source, nullptr);
}

bool DependencyObject::ClearPropertyValueOwned(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0
		|| (!metadata->CanWrite()
			&& !(allowReadOnly && metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;
	auto entryIt = _propertyValues.find(&metadata->Property());
	if (entryIt == _propertyValues.end()) return false;
	auto& entry = entryIt->second;
	const size_t sourceIndex = (size_t)index;
	auto& slot = entry.Slots[sourceIndex];
	if (!slot.IsOccupied()) return false;
	const bool bindingExpression =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	if (owner && (!bindingExpression || slot.BindingOwner != owner)) return false;

	BindingValue oldEffective;
	DependencyPropertyValueSource oldSource = DependencyPropertyValueSource::Default;
	const bool hadOldEffective = metadata->UsesEffectiveValueStorage()
		&& entry.HasEffectiveValue
		? (oldEffective = entry.EffectiveValue,
			oldSource = entry.EffectiveSource, true)
		: TryEvaluateEffectivePropertyValue(
			*metadata, entry, oldEffective, oldSource);
	const auto previous = slot;
	const Binding* retiredBinding = bindingExpression && !owner
		? slot.BindingOwner : nullptr;
	slot.Reset();

	BindingValue newEffective;
	DependencyPropertyValueSource newSource = DependencyPropertyValueSource::Default;
	const bool hasNewEffective = TryEvaluateEffectivePropertyValue(
		*metadata, entry, newEffective, newSource);
	BindingValue currentEffective;
	const bool backingStorageMatches = hasNewEffective
		&& (metadata->UsesEffectiveValueStorage()
			? entry.HasEffectiveValue
				&& metadata->ValuesEqual(
					entry.EffectiveValue, newEffective)
			: metadata->TryGet(*this, currentEffective)
				&& metadata->ValuesEqual(
					currentEffective, newEffective));
	const bool effectiveUnchanged = hadOldEffective && hasNewEffective
		&& oldSource == newSource
		&& metadata->ValuesEqual(oldEffective, newEffective)
		&& backingStorageMatches;
	const bool applied = effectiveUnchanged || !hasNewEffective
		|| ApplyEffectivePropertyValue(
			*metadata, newEffective, newSource, allowReadOnly);
	if (!applied)
	{
		slot = previous;
		return false;
	}
	// A slot-backed entry also owns a modified Default/base value and its
	// evaluated coercion cache. Keep it after the last precedence source clears.
	if (!entry.HasSources() && !metadata->UsesEffectiveValueStorage())
		_propertyValues.erase(entryIt);
	if (retiredBinding)
		RetireBindingExpression(metadata->Name(), retiredBinding);
	return true;

}

bool DependencyObject::ClearBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner) return false;
	if (!IsBindingExpressionOwner(
		propertyName, owner, source, expressionKind)) return false;
	return ClearPropertyValueOwned(propertyName, source, owner);
}

bool DependencyObject::IsBindingExpressionOwner(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind) const
{
	if (!owner) return false;
	auto* mutableThis = const_cast<DependencyObject*>(this);
	const auto* metadata = mutableThis->FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0) return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end()) return false;
	const auto& slot = entry->second.Slots[(size_t)index];
	return slot.Expression == expressionKind && slot.BindingOwner == owner;
}

void DependencyObject::RetireBindingExpression(
	const std::wstring& propertyName,
	const Binding* owner)
{
	if (!owner) return;
	if (_dataBindings && _dataBindings->Find(propertyName) == owner)
	{
		(void)_dataBindings->Remove(propertyName);
		return;
	}
	const_cast<Binding*>(owner)->DetachReplacedTargetExpression();
}

size_t DependencyObject::ClearPropertyValues(
	DependencyPropertyValueSource source)
{
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return 0;
	std::vector<std::wstring> properties;
	properties.reserve(_propertyValues.size());
	for (const auto& [metadata, entry] : _propertyValues)
	{
		if (metadata && entry.Slots[(size_t)index].IsOccupied())
			properties.push_back(metadata->Name());
	}
	size_t cleared = 0;
	for (const auto& property : properties)
	{
		if (ClearPropertyValue(property, source)) ++cleared;
	}
	return cleared;
}

size_t DependencyObject::ClearPropertyValues()
{
	return ClearPropertyValues(DependencyPropertyValueSource::Local);
}

bool DependencyObject::HasPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	if (source == DependencyPropertyValueSource::Default)
	{
		BindingValue ignored;
		return metadata->TryGetDefaultValue(ignored);
	}
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	return entry != _propertyValues.end()
		&& entry->second.Slots[(size_t)index].IsOccupied();
}

DependencyPropertyValueSource DependencyObject::GetPropertyValueSource(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return DependencyPropertyValueSource::Default;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end())
		return DependencyPropertyValueSource::Default;
	BindingValue value;
	DependencyPropertyValueSource source = DependencyPropertyValueSource::Default;
	TryResolveEffectivePropertyValue(*metadata, entry->second, value, source);
	return source;
}

DependencyPropertyExpressionKind DependencyObject::GetPropertyExpressionKind(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0) return DependencyPropertyExpressionKind::None;
	const auto entry = _propertyValues.find(&metadata->Property());
	return entry == _propertyValues.end()
		? DependencyPropertyExpressionKind::None
		: entry->second.Slots[(size_t)index].Expression;
}

bool DependencyObject::ResetPropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	if (ClearPropertyValue(propertyName, DependencyPropertyValueSource::Local))
		return true;
	auto entry = _propertyValues.find(&metadata->Property());
	if (entry != _propertyValues.end() && entry->second.HasSources())
		return false;
	if (metadata->UsesEffectiveValueStorage())
	{
		if (entry == _propertyValues.end())
		{
			BindingValue ignored;
			return TryGetEffectivePropertyValue(*metadata, ignored);
		}

		auto& effectiveEntry = entry->second;
		const auto previousBase = effectiveEntry.BaseValue;
		const bool previouslyHadBase = effectiveEntry.HasBaseValue;
		effectiveEntry.BaseValue = {};
		effectiveEntry.HasBaseValue = false;

		BindingValue defaultValue;
		DependencyPropertyValueSource defaultSource =
			DependencyPropertyValueSource::Default;
		if (!TryEvaluateEffectivePropertyValue(
			*metadata, effectiveEntry, defaultValue, defaultSource)
			|| !ApplyEffectivePropertyValue(
				*metadata, defaultValue, defaultSource))
		{
			effectiveEntry.BaseValue = previousBase;
			effectiveEntry.HasBaseValue = previouslyHadBase;
			return false;
		}
		return true;
	}
	BindingValue defaultValue;
	BindingValue effective;
	return metadata->TryGetDefaultValue(defaultValue)
		&& metadata->TryCoerce(*this, defaultValue, effective)
		&& ApplyEffectivePropertyValue(
			*metadata, effective, DependencyPropertyValueSource::Default);
}

bool DependencyObject::TrySetPropertyBaseValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;
	if (metadata->UsesEffectiveValueStorage())
	{
		BindingValue ignored;
		if (!TryGetEffectivePropertyValue(*metadata, ignored)) return false;
	}

	auto entryIt = _propertyValues.find(&metadata->Property());
	if (entryIt == _propertyValues.end())
	{
		BindingValue effective;
		if (!metadata->TryCoerce(*this, converted, effective)) return false;
		return ApplyEffectivePropertyValue(
			*metadata, effective, DependencyPropertyValueSource::Default);
	}

	auto& entry = entryIt->second;
	const auto previousBase = entry.BaseValue;
	const bool previouslyHadBase = entry.HasBaseValue;
	BindingValue previousEffective;
	DependencyPropertyValueSource previousSource =
		DependencyPropertyValueSource::Default;
	const bool hadPreviousEffective = metadata->UsesEffectiveValueStorage()
		&& entry.HasEffectiveValue
		? (previousEffective = entry.EffectiveValue,
			previousSource = entry.EffectiveSource, true)
		: TryEvaluateEffectivePropertyValue(
			*metadata, entry, previousEffective, previousSource);
	entry.BaseValue = converted;
	entry.HasBaseValue = true;

	BindingValue nextEffective;
	DependencyPropertyValueSource nextSource = DependencyPropertyValueSource::Default;
	if (!TryEvaluateEffectivePropertyValue(
		*metadata, entry, nextEffective, nextSource))
	{
		entry.BaseValue = previousBase;
		entry.HasBaseValue = previouslyHadBase;
		return false;
	}
	if (nextSource != DependencyPropertyValueSource::Default
		|| (hadPreviousEffective
			&& previousSource == nextSource
			&& metadata->ValuesEqual(previousEffective, nextEffective)))
		return true;
	if (ApplyEffectivePropertyValue(*metadata, nextEffective, nextSource))
		return true;
	entry.BaseValue = previousBase;
	entry.HasBaseValue = previouslyHadBase;
	return false;
}

bool DependencyObject::IsPropertyValueDefault(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanRead()) return false;
	BindingValue currentValue;
	BindingValue defaultValue;
	return metadata->TryGet(*this, currentValue)
		&& metadata->TryGetDefaultValue(defaultValue)
		&& metadata->ValuesEqual(currentValue, defaultValue);
}

bool DependencyObject::TryResolveEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	const EffectiveValueEntry& entry,
	BindingValue& value,
	DependencyPropertyValueSource& source) const
{
	for (int index = (int)entry.Slots.size() - 1; index >= 0; --index)
	{
		const auto& slot = entry.Slots[(size_t)index];
		if (!slot.ProposedValue.has_value()) continue;
		value = *slot.ProposedValue;
		source = static_cast<DependencyPropertyValueSource>(
			index + static_cast<int>(DependencyPropertyValueSource::Inherited));
		return true;
	}
	if (entry.HasBaseValue)
	{
		value = entry.BaseValue;
		source = DependencyPropertyValueSource::Default;
		return true;
	}
	source = DependencyPropertyValueSource::Default;
	return metadata.TryGetDefaultValue(value);
}

bool DependencyObject::TryEvaluateEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	const EffectiveValueEntry& entry,
	BindingValue& value,
	DependencyPropertyValueSource& source) const
{
	BindingValue proposed;
	if (!TryResolveEffectivePropertyValue(
		metadata, entry, proposed, source)) return false;
	return metadata.TryCoerce(
		*const_cast<DependencyObject*>(this), proposed, value);
}

bool DependencyObject::TryGetEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	BindingValue& value)
{
	if (!metadata.UsesEffectiveValueStorage()
		|| !metadata.Matches(*this)) return false;
	auto [entryIt, inserted] =
		_propertyValues.try_emplace(&metadata.Property());
	auto& entry = entryIt->second;
	if (!entry.HasEffectiveValue)
	{
		BindingValue effective;
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Default;
		if (!TryEvaluateEffectivePropertyValue(
			metadata, entry, effective, source))
		{
			if (inserted) _propertyValues.erase(entryIt);
			return false;
		}
		entry.EffectiveValue = std::move(effective);
		entry.HasEffectiveValue = true;
		entry.EffectiveSource = source;
	}
	value = entry.EffectiveValue;
	return true;
}

bool DependencyObject::ApplyEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& value,
	DependencyPropertyValueSource source,
	bool allowReadOnly)
{
	if (metadata.UsesEffectiveValueStorage())
	{
		if ((!allowReadOnly && metadata.IsReadOnly())
			|| !metadata.Matches(*this)) return false;

		BindingValue oldValue;
		const bool hadOldValue =
			TryGetEffectivePropertyValue(metadata, oldValue);
		auto entryIt = _propertyValues.find(&metadata.Property());
		if (entryIt == _propertyValues.end()) return false;
		auto& entry = entryIt->second;
		entry.EffectiveValue = value;
		entry.HasEffectiveValue = true;
		entry.EffectiveSource = source;
		if (hadOldValue && metadata.ValuesEqual(oldValue, value))
			return true;

		ApplyPropertyMetadataChange(metadata, oldValue, value);
		return true;
	}

	DependencyObject* const self = this;
	const auto lifetime = WeakLifetimeToken();
	const auto isAlive = [&lifetime]
	{
		const auto token = lifetime.lock();
		return token && *token;
	};
	const auto* previousMetadata = _applyingPropertyMetadata;
	const auto previousSource = _applyingPropertySource;
	_applyingPropertyMetadata = &metadata;
	_applyingPropertySource = source;
	bool result = false;
	try
	{
		if (!allowReadOnly && metadata.IsReadOnly()) result = false;
		else result = metadata.TrySetEffective(*this, value);
	}
	catch (...)
	{
		if (isAlive())
		{
			self->_applyingPropertyMetadata = previousMetadata;
			self->_applyingPropertySource = previousSource;
		}
		throw;
	}
	if (isAlive())
	{
		self->_applyingPropertyMetadata = previousMetadata;
		self->_applyingPropertySource = previousSource;
	}
	return result;
}

bool DependencyObject::TryGetValue(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	return const_cast<DependencyObject*>(this)->TryGetPropertyValue(
		propertyName, out);
}

bool DependencyObject::TrySetValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	VerifyAccess();
	// WPF DependencyObject.SetValue establishes a Local contribution and may
	// replace the expression occupying that slot. SetCurrentValue is exposed
	// separately by Control for behavior code that must preserve the source.
	return TrySetPropertyValue(propertyName, value);
}

bool DependencyObject::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	auto& target = *const_cast<DependencyObject*>(this);
	const auto* metadata = DependencyPropertyRegistry::Find(
		target, propertyName);
	if (!metadata) return false;
	out.Name = metadata->Name();
	out.ValueKind = metadata->ValueKind();
	out.ValueType = metadata->ValueType();
	out.CanRead = metadata->CanRead();
	out.CanWrite = metadata->CanWrite();
	out.CanObserve = true;
	return true;
}

std::vector<BindingSourcePropertyMetadata> DependencyObject::GetProperties() const
{
	auto& target = *const_cast<DependencyObject*>(this);
	std::vector<BindingSourcePropertyMetadata> result;
	for (const auto* metadata : DependencyPropertyRegistry::GetProperties(target))
	{
		if (!metadata) continue;
		result.push_back({
			metadata->Name(), metadata->ValueKind(), metadata->ValueType(),
			metadata->CanRead(), metadata->CanWrite(), true });
	}
	return result;
}

PropertyChangedEvent& DependencyObject::PropertyChanged()
{
	if (!_bindingSourceMetadataConnectionsInitialized)
	{
		_bindingSourceMetadataConnectionsInitialized = true;
		for (const auto* metadata :
			DependencyPropertyRegistry::GetProperties(*this))
		{
			if (!metadata || !metadata->CanObserve()) continue;
			auto connection = metadata->Subscribe(
				*this,
				[this, metadata]
				{
					if (_applyingPropertyMetadata == metadata) return;
					_bindingSourcePropertyChanged.Notify(metadata->Name());
				},
				DataSourceUpdateMode::OnPropertyChanged);
			if (connection.Connected())
				_bindingSourceMetadataConnections.push_back(
					std::move(connection));
		}
	}
	return _bindingSourcePropertyChanged;
}
