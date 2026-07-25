#pragma once

#include "Binding.h"
#include "DispatcherObject.h"
#include "Event.h"
#include "XamlSchema.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Binding;
class BindingCollection;
class DependencyPropertyMetadata;
class DependencyPropertyRegistry;
class DeclarativeTypeDescriptor;

struct DependencyPropertyChangedEventArgs
{
	std::wstring PropertyName;
	BindingValue OldValue;
	BindingValue NewValue;
};

using DependencyPropertyChangedEvent = Event<void(
	class DependencyObject*, const DependencyPropertyChangedEventArgs&)>;

/** Owns dependency-property values, expressions and change observation. */
class DependencyObject : public DispatcherObject, public IBindingSource
{
protected:
	struct EffectiveValueSlot
	{
		std::optional<BindingValue> ProposedValue;
		DependencyPropertyExpressionKind Expression =
			DependencyPropertyExpressionKind::None;
		const Binding* BindingOwner = nullptr;
		std::wstring ResourceKey;

		bool IsOccupied() const noexcept
		{
			return ProposedValue.has_value()
				|| Expression != DependencyPropertyExpressionKind::None;
		}

		void Reset()
		{
			ProposedValue.reset();
			Expression = DependencyPropertyExpressionKind::None;
			BindingOwner = nullptr;
			ResourceKey.clear();
		}
	};

	struct EffectiveValueEntry
	{
		BindingValue BaseValue;
		bool HasBaseValue = false;
		std::array<EffectiveValueSlot, 7> Slots;

		bool HasSources() const noexcept
		{
			for (const auto& slot : Slots)
				if (slot.IsOccupied()) return true;
			return false;
		}
	};

	std::unordered_map<const DependencyPropertyMetadata*, EffectiveValueEntry>
		_propertyValues;
	std::shared_ptr<const DeclarativeTypeDescriptor> _declarativeTypeDescriptor;
	std::vector<BindingValue> _declarativePropertyValues;
	const DependencyPropertyMetadata* _applyingPropertyMetadata = nullptr;
	DependencyPropertyValueSource _applyingPropertySource =
		DependencyPropertyValueSource::Default;
	bool _refreshingInheritedProperties = false;
	unsigned long long _propertyChangeVersion = 0;
	bool _isDestroying = false;
	PropertyChangedEvent _bindingSourcePropertyChanged;
	std::vector<EventConnection> _bindingSourceMetadataConnections;
	bool _bindingSourceMetadataConnectionsInitialized = false;
	// Destroy expressions before the value slots and their sources disappear.
	std::unique_ptr<BindingCollection> _dataBindings;

	virtual void ApplyPropertyMetadataChange(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& oldValue,
		const BindingValue& newValue) = 0;
	virtual void OnBindingValidationChanged(
		const std::wstring& targetProperty) = 0;
	virtual const DependencyPropertyMetadata* FindDeclarativePropertyMetadata(
		const std::wstring& propertyName) const = 0;
	virtual std::vector<const DependencyPropertyMetadata*>
		GetDeclarativePropertyMetadata() const = 0;
	/**
	 * Filters native metadata through the object's public framework type.
	 *
	 * The C++ behavior-host hierarchy is an implementation detail. Structural
	 * WPF elements may reuse native storage/algorithms without inheriting that
	 * host's author-facing dependency properties or XAML type identity.
	 */
	virtual bool SupportsNativeProperty(
		const DependencyPropertyMetadata& metadata) const
	{
		(void)metadata;
		return true;
	}
	virtual bool CanAcquireBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) = 0;
	virtual bool TryAttachBindingPropertyExpression(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) = 0;
	virtual bool TrySetBindingPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) = 0;
	virtual bool ClearBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) = 0;
	virtual bool IsBindingExpressionOwner(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) const = 0;
	virtual bool TryGetPropertyValue(
		const std::wstring& propertyName,
		BindingValue& out) = 0;
	virtual bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value) = 0;
	virtual bool TrySetCurrentPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value) = 0;

	friend class Binding;
	friend class BindingCollection;
	friend class DependencyPropertyMetadata;
	friend class DependencyPropertyRegistry;

public:
	DependencyObject() = default;
	~DependencyObject() override = default;

	DependencyPropertyChangedEvent OnPropertyValueChanged;
	bool TryGetValue(
		const std::wstring& propertyName,
		BindingValue& out) const override;
	bool TrySetValue(
		const std::wstring& propertyName,
		const BindingValue& value) override;
	bool TryGetPropertyMetadata(
		const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const override;
	std::vector<BindingSourcePropertyMetadata> GetProperties() const override;
	PropertyChangedEvent& PropertyChanged() override;
	unsigned long long PropertyChangeVersion() const noexcept
	{
		return _propertyChangeVersion;
	}
	bool IsDestroying() const noexcept { return _isDestroying; }

	virtual void EnsureBindingPropertiesRegistered() = 0;
};
