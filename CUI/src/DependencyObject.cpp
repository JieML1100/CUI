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

	DependencyObject* CompiledDependencySource(
		const CompiledSourceHandle& source) noexcept
	{
		return static_cast<DependencyObject*>(source.Object);
	}

	const DependencyProperty* CompiledDependencyProperty(
		const CompiledSourceHandle& source) noexcept
	{
		return static_cast<const DependencyProperty*>(source.Context);
	}

	CompiledBindingPathCapabilities DependencySourceCapabilities(
		const CompiledSourceHandle& source)
	{
		auto* object = CompiledDependencySource(source);
		const auto* property = CompiledDependencyProperty(source);
		if (!object || !property) return CompiledBindingPathCapabilities::None;
		const auto* metadata = object->GetPropertyMetadata(*property);
		if (!metadata) return CompiledBindingPathCapabilities::None;
		auto result = CompiledBindingPathCapabilities::None;
		if (metadata->CanRead())
			result = result | CompiledBindingPathCapabilities::Read;
		if (metadata->CanWrite())
			result = result | CompiledBindingPathCapabilities::Write;
		if (metadata->CanObserve())
			result = result | CompiledBindingPathCapabilities::Observe;
		return result;
	}

	BindingValueKind DependencySourceValueKind(
		const CompiledSourceHandle& source)
	{
		auto* object = CompiledDependencySource(source);
		const auto* property = CompiledDependencyProperty(source);
		if (!object || !property) return BindingValueKind::Empty;
		const auto* metadata = object->GetPropertyMetadata(*property);
		return metadata ? metadata->ValueKind() : BindingValueKind::Empty;
	}

	std::weak_ptr<const void> DependencySourceLifetime(
		const CompiledSourceHandle& source)
	{
		auto* object = CompiledDependencySource(source);
		return object ? object->BindingLifetime()
			: std::weak_ptr<const void>{};
	}

	bool ReadDependencySource(
		const CompiledSourceHandle& source,
		BindingValue& out)
	{
		auto* object = CompiledDependencySource(source);
		const auto* property = CompiledDependencyProperty(source);
		return object && property
			&& object->TryGetPropertyValue(*property, out);
	}

	bool WriteDependencySource(
		const CompiledSourceHandle& source,
		const BindingValue& value)
	{
		auto* object = CompiledDependencySource(source);
		const auto* property = CompiledDependencyProperty(source);
		return object && property
			&& object->TrySetPropertyValue(*property, value);
	}

	EventConnection SubscribeDependencySource(
		const CompiledSourceHandle& source,
		DependencyPropertyChangeHandler handler)
	{
		auto* object = CompiledDependencySource(source);
		const auto* property = CompiledDependencyProperty(source);
		if (!object || !property || !handler) return {};
		const auto* metadata = object->GetPropertyMetadata(*property);
		return metadata
			? metadata->Subscribe(*object, std::move(handler),
				DataSourceUpdateMode::OnPropertyChanged)
			: EventConnection{};
	}

	const CompiledSourceOps DependencySourceOps{
		&DependencySourceCapabilities,
		&DependencySourceValueKind,
		&DependencySourceLifetime,
		&ReadDependencySource,
		&WriteDependencySource,
		&SubscribeDependencySource,
		nullptr,
		nullptr
	};
}

CompiledSourceHandle cui::binding::MakeCompiledDependencyPropertySource(
	DependencyObject& source,
	const DependencyProperty& property) noexcept
{
	return { &source, &property, &DependencySourceOps };
}

CompiledSourceHandle cui::binding::ResolveCompiledDependencyPropertySource(
	IBindingSource& source,
	const DependencyProperty& property) noexcept
{
	auto* dependencySource = dynamic_cast<DependencyObject*>(&source);
	return dependencySource
		? MakeCompiledDependencyPropertySource(*dependencySource, property)
		: CompiledSourceHandle{};
}

DependencyObject::~DependencyObject()
{
	InvalidateLifetimeToken();
	_isDestroying = true;
#if CUI_ENABLE_DYNAMIC_XAML
	_bindingSourceMetadataConnections.clear();
#endif
	_dataBindings.reset();
	_propertyValues.clear();
}

const DependencyPropertyMetadata*
DependencyObject::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
#if CUI_ENABLE_DYNAMIC_XAML
	(void)property;
	return nullptr;
#else
	const auto matches = [this, &property](
		const DependencyPropertyMetadataRegistration& relation)
	{
		const auto& metadata = relation.Metadata();
		return &metadata.Property() == &property
			&& metadata.Matches(*this)
			&& SupportsNativeProperty(metadata);
	};

	// First select a matching descendant whenever the explicit immediate-base
	// chain proves it is more specific. A second pass rejects incomparable
	// branches, so publication/first-touch order cannot affect the result.
	const auto* relations = property._staticMetadataRelations.load(
		std::memory_order_acquire);
	const DependencyPropertyMetadataRegistration* best = nullptr;
	for (auto* candidate = relations;
		candidate; candidate = candidate->_next)
	{
		if (!matches(*candidate)) continue;
		if (!best || candidate->IsBasedOn(*best)) best = candidate;
	}
	if (!best) return nullptr;
	for (auto* candidate = relations;
		candidate; candidate = candidate->_next)
	{
		if (candidate == best || !matches(*candidate)) continue;
		if (!best->IsBasedOn(*candidate)) return nullptr;
	}
	return &best->Metadata();
#endif
}

void DependencyObject::PublishDeferredPropertyChange(
	const DeferredPropertyChange& change)
{
	VerifyAccess();
	if (!change.Metadata) return;
	ApplyPropertyMetadataChange(
		*change.Metadata, change.Previous, change.Current);
}

void DependencyObject::ApplyPropertyMetadataChange(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& oldValue,
	const BindingValue& newValue)
{
	if (DeferPropertyMetadataChange(metadata, oldValue, newValue)) return;
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
		metadata.Property(), oldValue, newValue };
	cui::framework::EventAccess::Raise(
		self->OnPropertyValueChanged, self, args);
	if (!isAlive()) return;
#if CUI_ENABLE_DYNAMIC_XAML
	self->_bindingSourcePropertyChanged.Notify(metadata.Name());
#else
	self->_bindingSourcePropertyChanged.Notify(
		metadata.Property().BindingSourceToken());
#endif
}

EventConnection DependencyObject::SubscribeDefaultPropertyChange(
	const DependencyProperty& property,
	DependencyPropertyChangeHandler handler,
	DataSourceUpdateMode updateMode)
{
	(void)updateMode;
	return OnPropertyValueChanged.Subscribe(
		[expected = &property, handler = std::move(handler)](
			DependencyObject*,
			const DependencyPropertyChangedEventArgs& args)
		{
			if (args.Property == expected)
				handler();
		});
}

bool DependencyObject::DeferPropertyMetadataChange(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& oldValue,
	const BindingValue& newValue)
{
	if (!_stagingPropertyChange) return false;
	_stagingPropertyChange->Metadata = &metadata;
	_stagingPropertyChange->Previous = oldValue;
	_stagingPropertyChange->Current = newValue;
	return true;
}

const DependencyPropertyMetadata* DependencyObject::GetPropertyMetadata(
	const DependencyProperty& property)
{
	return DependencyPropertyRegistry::GetMetadata(*this, property);
}

const DependencyPropertyMetadata* DependencyObject::GetPropertyMetadata(
	BindingSourcePropertyToken property)
{
#if CUI_ENABLE_DYNAMIC_XAML
	return DependencyPropertyRegistry::Find(*this, property);
#else
	(void)property;
	return nullptr;
#endif
}

bool DependencyObject::TryGetPropertyValue(
	const DependencyProperty& property,
	BindingValue& out)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && metadata->TryGet(*this, out);
}

bool DependencyObject::TryGetPropertyValue(
	const DependencyProperty& property,
	DependencyPropertyValueSource source,
	BindingValue& out)
{
	const auto* metadata = GetPropertyMetadata(property);
	if (!metadata) return false;
	if (source == DependencyPropertyValueSource::Default)
		return metadata->TryGetDefaultValue(out);
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&property);
	if (entry == _propertyValues.end()
		|| !entry->second.Slots[(size_t)index].ProposedValue.has_value())
		return false;
	out = *entry->second.Slots[(size_t)index].ProposedValue;
	return true;
}

bool DependencyObject::TrySetPropertyValue(
	const DependencyProperty& property,
	const BindingValue& value)
{
	return TrySetPropertyValue(
		property, value, DependencyPropertyValueSource::Local);
}

bool DependencyObject::TrySetPropertyValue(
	const DependencyProperty& property,
	const BindingValue& value,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && TrySetPropertyValueOwned(
		*metadata, value, source, nullptr);
}

bool DependencyObject::TrySetPropertyValue(
	const DependencyPropertyKey& key,
	const BindingValue& value)
{
	return TrySetReadOnlyPropertyValue(key, value);
}

bool DependencyObject::TrySetPropertyValueOwned(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& value,
	DependencyPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (!metadata.CanWrite()
			&& !(allowReadOnly && metadata.IsReadOnly()
				&& metadata.CanWriteInternally()))) return false;

	if (owner) return false;
	BindingValue converted;
	if (!metadata.TryConvert(value, converted)) return false;
	return TrySetEffectiveValueEntry(
		metadata, std::move(converted), source,
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
	std::wstring resourceKey)
{
	return TrySetEffectiveValueEntry(
		metadata, std::move(proposedValue), source, expressionKind,
		owner, std::move(resourceKey), false);
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
		RetireBindingExpression(metadata.Property(), retiredBinding);
	return true;
}

bool DependencyObject::CanAcquireBindingPropertyValue(
	const DependencyPropertyMetadata& metadata,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner || !metadata.CanWrite()) return false;
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (expressionKind == DependencyPropertyExpressionKind::Binding
			&& source != DependencyPropertyValueSource::Local)
		|| (expressionKind == DependencyPropertyExpressionKind::TemplateBinding
			&& source != DependencyPropertyValueSource::Template))
		return false;
	const auto entry = _propertyValues.find(&metadata.Property());
	if (entry == _propertyValues.end()) return true;
	const auto& slot = entry->second.Slots[(size_t)index];
	const bool isBindingExpression =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	return !isBindingExpression || !slot.BindingOwner
		|| slot.BindingOwner == owner;
}

bool DependencyObject::TryAttachBindingPropertyExpression(
	const DependencyPropertyMetadata& metadata,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!CanAcquireBindingPropertyValue(
		metadata, owner, source, expressionKind)) return false;
	return TrySetEffectiveValueEntry(
		metadata, std::nullopt, source, expressionKind, owner, {}, false);
}

bool DependencyObject::TrySetBindingPropertyValue(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& value,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner || !IsBindingExpressionOwner(
		metadata, owner, source, expressionKind)) return false;
	BindingValue converted;
	if (!metadata.TryConvert(value, converted)) return false;
	return TrySetEffectiveValueEntry(
		metadata, std::move(converted), source,
		expressionKind, owner, {}, false);
}

bool DependencyObject::TrySetCurrentPropertyValueCore(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& value)
{
	if (!metadata.CanWrite()) return false;

	BindingValue converted;
	BindingValue coerced;
	if (!metadata.TryConvert(value, converted)
		|| !metadata.TryCoerce(*this, converted, coerced)) return false;
	BindingValue current;
	if (metadata.TryGet(*this, current)
		&& metadata.ValuesEqual(current, coerced)) return true;

	DependencyPropertyValueSource source =
		DependencyPropertyValueSource::Default;
	const auto entry = _propertyValues.find(&metadata.Property());
	if (entry != _propertyValues.end())
	{
		BindingValue ignored;
		(void)TryResolveEffectivePropertyValue(
			metadata, entry->second, ignored, source);
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
				metadata, std::move(converted),
				source, slot->Expression, slot->BindingOwner,
				slot->ResourceKey, false);
		}
	}

	// WPF SetCurrentValue changes the value carried by the existing source;
	// it never promotes a Default/Style/Template/Inherited value to Local.
	// A later update from that source therefore remains authoritative.
	return source == DependencyPropertyValueSource::Default
		? TrySetPropertyBaseValueCore(metadata, converted)
		: TrySetEffectiveValueEntry(
			metadata, std::move(converted), source,
			source == DependencyPropertyValueSource::Animation
				? DependencyPropertyExpressionKind::Animation
				: DependencyPropertyExpressionKind::None,
			nullptr, {}, false);
}

bool DependencyObject::TrySetCurrentPropertyValue(
	const DependencyProperty& property,
	const BindingValue& value)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && TrySetCurrentPropertyValueCore(*metadata, value);
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
			*metadata, value,
			DependencyPropertyValueSource::Local, nullptr, true);
}

bool DependencyObject::ClearReadOnlyPropertyValue(
	const DependencyPropertyKey& key)
{
	const auto& property = key.Property();
	if (!property.Authorizes(key)) return false;
	const auto* metadata = GetPropertyMetadata(property);
	return metadata
		&& ClearPropertyValueOwned(
			*metadata, DependencyPropertyValueSource::Local,
			nullptr, true);
}

bool DependencyObject::CoerceValueCore(
	const DependencyPropertyMetadata& metadata)
{
	if (!metadata.CanWrite()
		&& !(metadata.IsReadOnly()
			&& metadata.CanWriteInternally())) return false;
	const bool allowReadOnly = metadata.IsReadOnly();

	const auto entry = _propertyValues.find(&metadata.Property());
	if (entry != _propertyValues.end())
	{
		BindingValue effective;
		DependencyPropertyValueSource source = DependencyPropertyValueSource::Default;
		if (!TryEvaluateEffectivePropertyValue(
			metadata, entry->second, effective, source)) return false;
		BindingValue current;
		if (metadata.TryGet(*this, current)
			&& metadata.ValuesEqual(current, effective)) return true;
		return ApplyEffectivePropertyValue(
			metadata, effective, source, allowReadOnly);
	}

	BindingValue proposed;
	if (!metadata.TryGetDefaultValue(proposed)
		&& !metadata.TryGet(*this, proposed)) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata.TryConvert(proposed, converted)
		|| !metadata.TryCoerce(*this, converted, effective)) return false;

	BindingValue current;
	if (metadata.TryGet(*this, current)
		&& metadata.ValuesEqual(current, effective)) return true;
	return ApplyEffectivePropertyValue(
		metadata, effective, DependencyPropertyValueSource::Default,
		allowReadOnly);
}

bool DependencyObject::CoerceValue(const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && CoerceValueCore(*metadata);
}

bool DependencyObject::ClearPropertyValue(
	const DependencyProperty& property)
{
	return ClearPropertyValue(
		property, DependencyPropertyValueSource::Local);
}

bool DependencyObject::ClearPropertyValue(
	const DependencyPropertyKey& key)
{
	return ClearReadOnlyPropertyValue(key);
}

bool DependencyObject::ClearPropertyValue(
	const DependencyProperty& property,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && ClearPropertyValueOwned(
		*metadata, source, nullptr);
}

bool DependencyObject::ClearPropertyValueOwned(
	const DependencyPropertyMetadata& metadata,
	DependencyPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (!metadata.CanWrite()
			&& !(allowReadOnly && metadata.IsReadOnly()
				&& metadata.CanWriteInternally()))) return false;
	auto entryIt = _propertyValues.find(&metadata.Property());
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
	const bool hadOldEffective = metadata.UsesEffectiveValueStorage()
		&& entry.HasEffectiveValue
		? (oldEffective = entry.EffectiveValue,
			oldSource = entry.EffectiveSource, true)
		: TryEvaluateEffectivePropertyValue(
			metadata, entry, oldEffective, oldSource);
	const auto previous = slot;
	const Binding* retiredBinding = bindingExpression && !owner
		? slot.BindingOwner : nullptr;
	slot.Reset();

	BindingValue newEffective;
	DependencyPropertyValueSource newSource = DependencyPropertyValueSource::Default;
	const bool hasNewEffective = TryEvaluateEffectivePropertyValue(
		metadata, entry, newEffective, newSource);
	BindingValue currentEffective;
	const bool backingStorageMatches = hasNewEffective
		&& (metadata.UsesEffectiveValueStorage()
			? entry.HasEffectiveValue
				&& metadata.ValuesEqual(
					entry.EffectiveValue, newEffective)
			: metadata.TryGet(*this, currentEffective)
				&& metadata.ValuesEqual(
					currentEffective, newEffective));
	const bool effectiveUnchanged = hadOldEffective && hasNewEffective
		&& oldSource == newSource
		&& metadata.ValuesEqual(oldEffective, newEffective)
		&& backingStorageMatches;
	const bool applied = effectiveUnchanged || !hasNewEffective
		|| ApplyEffectivePropertyValue(
			metadata, newEffective, newSource, allowReadOnly);
	if (!applied)
	{
		slot = previous;
		return false;
	}
	// A slot-backed entry also owns a modified Default/base value and its
	// evaluated coercion cache. Keep it after the last precedence source clears.
	if (!entry.HasSources() && !metadata.UsesEffectiveValueStorage())
		_propertyValues.erase(entryIt);
	if (retiredBinding)
		RetireBindingExpression(metadata.Property(), retiredBinding);
	return true;

}

bool DependencyObject::ClearBindingPropertyValue(
	const DependencyPropertyMetadata& metadata,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner || !IsBindingExpressionOwner(
		metadata, owner, source, expressionKind)) return false;
	return ClearPropertyValueOwned(metadata, source, owner);
}

bool DependencyObject::IsBindingExpressionOwner(
	const DependencyPropertyMetadata& metadata,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind) const
{
	if (!owner) return false;
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&metadata.Property());
	if (entry == _propertyValues.end()) return false;
	const auto& slot = entry->second.Slots[(size_t)index];
	return slot.Expression == expressionKind && slot.BindingOwner == owner;
}

void DependencyObject::RetireBindingExpression(
	const DependencyProperty& property,
	const Binding* owner)
{
	if (!owner) return;
	if (_dataBindings && _dataBindings->Find(property) == owner)
	{
		(void)_dataBindings->Remove(property);
		return;
	}
	const_cast<Binding*>(owner)->DetachReplacedTargetExpression();
}

size_t DependencyObject::ClearPropertyValues(
	DependencyPropertyValueSource source)
{
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return 0;
	std::vector<const DependencyProperty*> properties;
	properties.reserve(_propertyValues.size());
	for (const auto& [property, entry] : _propertyValues)
	{
		if (property && entry.Slots[(size_t)index].IsOccupied())
			properties.push_back(property);
	}
	size_t cleared = 0;
	for (const auto* property : properties)
	{
		if (property && ClearPropertyValue(*property, source)) ++cleared;
	}
	return cleared;
}

size_t DependencyObject::ClearPropertyValues()
{
	return ClearPropertyValues(DependencyPropertyValueSource::Local);
}

bool DependencyObject::HasPropertyValue(
	const DependencyProperty& property,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
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
	const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
	if (!metadata) return DependencyPropertyValueSource::Default;
	const auto entry = _propertyValues.find(&property);
	if (entry == _propertyValues.end())
		return DependencyPropertyValueSource::Default;
	BindingValue value;
	DependencyPropertyValueSource source = DependencyPropertyValueSource::Default;
	TryResolveEffectivePropertyValue(*metadata, entry->second, value, source);
	return source;
}

DependencyPropertyExpressionKind DependencyObject::GetPropertyExpressionKind(
	const DependencyProperty& property,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0) return DependencyPropertyExpressionKind::None;
	const auto entry = _propertyValues.find(&property);
	return entry == _propertyValues.end()
		? DependencyPropertyExpressionKind::None
		: entry->second.Slots[(size_t)index].Expression;
}

bool DependencyObject::ResetPropertyValue(const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
	if (!metadata || !metadata->CanWrite()) return false;
	if (ClearPropertyValue(property, DependencyPropertyValueSource::Local))
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

bool DependencyObject::TrySetPropertyBaseValueCore(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& convertedValue)
{
	if (!metadata.CanWrite() || !metadata.IsValidValue(convertedValue))
		return false;
	if (metadata.UsesEffectiveValueStorage())
	{
		BindingValue ignored;
		if (!TryGetEffectivePropertyValue(metadata, ignored)) return false;
	}

	auto entryIt = _propertyValues.find(&metadata.Property());
	if (entryIt == _propertyValues.end())
	{
		BindingValue effective;
		if (!metadata.TryCoerce(*this, convertedValue, effective)) return false;
		return ApplyEffectivePropertyValue(
			metadata, effective, DependencyPropertyValueSource::Default);
	}

	auto& entry = entryIt->second;
	const auto previousBase = entry.BaseValue;
	const bool previouslyHadBase = entry.HasBaseValue;
	BindingValue previousEffective;
	DependencyPropertyValueSource previousSource =
		DependencyPropertyValueSource::Default;
	const bool hadPreviousEffective = metadata.UsesEffectiveValueStorage()
		&& entry.HasEffectiveValue
		? (previousEffective = entry.EffectiveValue,
			previousSource = entry.EffectiveSource, true)
		: TryEvaluateEffectivePropertyValue(
			metadata, entry, previousEffective, previousSource);
	entry.BaseValue = convertedValue;
	entry.HasBaseValue = true;

	BindingValue nextEffective;
	DependencyPropertyValueSource nextSource = DependencyPropertyValueSource::Default;
	if (!TryEvaluateEffectivePropertyValue(
		metadata, entry, nextEffective, nextSource))
	{
		entry.BaseValue = previousBase;
		entry.HasBaseValue = previouslyHadBase;
		return false;
	}
	if (nextSource != DependencyPropertyValueSource::Default
		|| (hadPreviousEffective
			&& previousSource == nextSource
			&& metadata.ValuesEqual(previousEffective, nextEffective)))
		return true;
	if (ApplyEffectivePropertyValue(metadata, nextEffective, nextSource))
		return true;
	entry.BaseValue = previousBase;
	entry.HasBaseValue = previouslyHadBase;
	return false;
}

bool DependencyObject::IsPropertyValueDefault(
	const DependencyProperty& property)
{
	const auto* metadata = GetPropertyMetadata(property);
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
	DependencyPropertyValueSource source)
{
	return ApplyEffectivePropertyValue(metadata, value, source, false);
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
	BindingSourcePropertyToken property,
	BindingValue& out) const
{
	auto& target = *const_cast<DependencyObject*>(this);
	const auto* metadata = target.GetPropertyMetadata(property);
	return metadata && metadata->TryGet(target, out);
}

bool DependencyObject::TrySetValue(
	BindingSourcePropertyToken property,
	const BindingValue& value)
{
	VerifyAccess();
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && TrySetPropertyValueOwned(
		*metadata, value, DependencyPropertyValueSource::Local, nullptr);
}

bool DependencyObject::TryGetPropertyMetadata(
	BindingSourcePropertyToken property,
	BindingSourcePropertyMetadata& out) const
{
	auto& target = *const_cast<DependencyObject*>(this);
	const auto* metadata = target.GetPropertyMetadata(property);
	if (!metadata) return false;
#if CUI_ENABLE_DYNAMIC_XAML
	// The token route does not need to materialize the design-time member name.
	out.Name.clear();
#endif
	out.ValueKind = metadata->ValueKind();
	out.ValueType = metadata->ValueType();
	out.CanRead = metadata->CanRead();
	out.CanWrite = metadata->CanWrite();
	out.CanObserve = true;
	return true;
}

#if !CUI_ENABLE_DYNAMIC_XAML
PropertyChangedEvent& DependencyObject::PropertyChanged()
{
	return _bindingSourcePropertyChanged;
}
#endif
