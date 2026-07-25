#include "DependencyObject.h"

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
