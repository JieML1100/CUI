#include "DependencyObject.h"

#if !CUI_ENABLE_DYNAMIC_XAML
#error DependencyObject.Design.cpp requires the Design runtime flavor.
#endif

const DependencyProperty* DependencyObject::FindDependencyProperty(
	const std::wstring& propertyName)
{
	return DependencyPropertyRegistry::FindProperty(*this, propertyName);
}

const DependencyPropertyMetadata* DependencyObject::FindPropertyMetadata(
	const std::wstring& propertyName)
{
	return DependencyPropertyRegistry::Find(*this, propertyName);
}

bool DependencyObject::TryGetPropertyValue(
	const std::wstring& propertyName,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && metadata->TryGet(*this, out);
}

bool DependencyObject::TryGetPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata
		&& TryGetPropertyValue(metadata->Property(), source, out);
}

bool DependencyObject::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return TrySetPropertyValue(
		propertyName, value, DependencyPropertyValueSource::Local);
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
	return metadata && TrySetPropertyValueOwned(
		*metadata, value, source, owner, allowReadOnly);
}

bool DependencyObject::CanAcquireBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && CanAcquireBindingPropertyValue(
		*metadata, owner, source, expressionKind);
}

bool DependencyObject::TryAttachBindingPropertyExpression(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && TryAttachBindingPropertyExpression(
		*metadata, owner, source, expressionKind);
}

bool DependencyObject::TrySetBindingPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && TrySetBindingPropertyValue(
		*metadata, value, owner, source, expressionKind);
}

bool DependencyObject::TrySetCurrentPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && TrySetCurrentPropertyValueCore(*metadata, value);
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
		|| FindObjectPropertyMetadataByName(metadata->Name()) != metadata)
		return false;
	return TrySetPropertyValueOwned(
		*metadata, value, DependencyPropertyValueSource::Local, nullptr, true);
}

bool DependencyObject::ClearReadOnlyPropertyValue(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->IsReadOnly()
		|| FindObjectPropertyMetadataByName(metadata->Name()) != metadata)
		return false;
	return ClearPropertyValueOwned(
		*metadata, DependencyPropertyValueSource::Local, nullptr, true);
}

bool DependencyObject::CoerceValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && CoerceValueCore(*metadata);
}

bool DependencyObject::ClearPropertyValue(
	const std::wstring& propertyName)
{
	return ClearPropertyValue(
		propertyName, DependencyPropertyValueSource::Local);
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
	return metadata && ClearPropertyValueOwned(
		*metadata, source, owner, allowReadOnly);
}

bool DependencyObject::ClearBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && ClearBindingPropertyValue(
		*metadata, owner, source, expressionKind);
}

bool DependencyObject::IsBindingExpressionOwner(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind) const
{
	auto* mutableThis = const_cast<DependencyObject*>(this);
	const auto* metadata = mutableThis->FindPropertyMetadata(propertyName);
	return metadata && IsBindingExpressionOwner(
		*metadata, owner, source, expressionKind);
}

bool DependencyObject::HasPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata
		&& HasPropertyValue(metadata->Property(), source);
}

DependencyPropertyValueSource DependencyObject::GetPropertyValueSource(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata
		? GetPropertyValueSource(metadata->Property())
		: DependencyPropertyValueSource::Default;
}

DependencyPropertyExpressionKind DependencyObject::GetPropertyExpressionKind(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata
		? GetPropertyExpressionKind(metadata->Property(), source)
		: DependencyPropertyExpressionKind::None;
}

bool DependencyObject::ResetPropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && ResetPropertyValue(metadata->Property());
}

bool DependencyObject::IsPropertyValueDefault(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && IsPropertyValueDefault(metadata->Property());
}

bool DependencyObject::TrySetPropertyBaseValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;
	return TrySetPropertyBaseValueCore(*metadata, converted);
}

bool DependencyObject::TryGetValue(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	auto& target = *const_cast<DependencyObject*>(this);
	const auto* metadata = DependencyPropertyRegistry::Find(
		target, propertyName);
	return metadata && metadata->TryGet(target, out);
}

bool DependencyObject::TrySetValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	VerifyAccess();
	// WPF DependencyObject.SetValue establishes a Local contribution and may
	// replace the expression occupying that slot. SetCurrentValue is exposed
	// separately by Control for behavior code that must preserve the source.
	const auto* metadata = DependencyPropertyRegistry::Find(
		*this, propertyName);
	return metadata && TrySetPropertyValueOwned(
		*metadata, value, DependencyPropertyValueSource::Local, nullptr);
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
			if (!metadata || !metadata->CanObserve()
				|| metadata->UsesGenericObservation()) continue;
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
