#ifndef CUI_DEPENDENCY_OBJECT_H_INCLUDED
#define CUI_DEPENDENCY_OBJECT_H_INCLUDED
#pragma once

#include "Binding.h"
#include "DispatcherObject.h"
#include "Event.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct DependencyPropertyChangedEventArgs
{
	std::wstring PropertyName;
	BindingValue OldValue;
	BindingValue NewValue;
	const DependencyProperty* Property = nullptr;
};

using DependencyPropertyChangedEvent = Event<void(
	class DependencyObject*, const DependencyPropertyChangedEventArgs&)>;

/** Owns dependency-property values, expressions and change observation. */
class DependencyObject : public DispatcherObject, public IBindingSource
{
protected:
	using DeclarativePropertyMetadata = DependencyPropertyMetadata;
	using DeclarativePropertyMetadataPointer =
		const DeclarativePropertyMetadata*;
	using DeclarativePropertyMetadataCollection =
		std::vector<DeclarativePropertyMetadataPointer>;
	using DeclarativeType = DeclarativeTypeDescriptor;
	using DataBindingCollection = BindingCollection;

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
		// Cached evaluated value (including coercion). This belongs to the
		// property engine; it is not a second CLR/backing-field store.
		BindingValue EffectiveValue;
		bool HasEffectiveValue = false;
		DependencyPropertyValueSource EffectiveSource =
			DependencyPropertyValueSource::Default;
		std::array<EffectiveValueSlot, 7> Slots;

		bool HasSources() const noexcept
		{
			for (const auto& slot : Slots)
				if (slot.IsOccupied()) return true;
			return false;
		}
	};

	std::unordered_map<const DependencyProperty*, EffectiveValueEntry>
		_propertyValues;
	std::shared_ptr<const DeclarativeType> _declarativeTypeDescriptor;
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
	std::unique_ptr<DataBindingCollection> _dataBindings;

	/**
	 * Applies framework-specific consequences after the effective value has
	 * changed. DependencyObject publishes the metadata callback and generic
	 * property notifications; FrameworkElement/Control extend this for layout,
	 * rendering and inherited-property propagation.
	 */
	virtual void ApplyPropertyMetadataChange(
		const DeclarativePropertyMetadata& metadata,
		const BindingValue& oldValue,
		const BindingValue& newValue);
	virtual void OnBindingValidationChanged(
		const std::wstring& targetProperty)
	{
		(void)targetProperty;
	}
	virtual DeclarativePropertyMetadataPointer FindDeclarativePropertyMetadata(
		const std::wstring& propertyName) const
	{
		(void)propertyName;
		return nullptr;
	}
	virtual DeclarativePropertyMetadataCollection
		GetDeclarativePropertyMetadata() const
	{
		return {};
	}
	/**
	 * Filters native metadata through the object's public framework type.
	 *
	 * The C++ behavior-host hierarchy is an implementation detail. Structural
	 * WPF elements may reuse native storage/algorithms without inheriting that
	 * host's author-facing dependency properties or XAML type identity.
	 */
	virtual bool SupportsNativeProperty(
		const DeclarativePropertyMetadata& metadata) const
	{
		(void)metadata;
		return true;
	}
	/** Default metadata observation; UIElement overrides validation commits. */
	virtual EventConnection SubscribeDefaultPropertyChange(
		const std::wstring& propertyName,
		DependencyPropertyChangeHandler handler,
		DataSourceUpdateMode updateMode);
	virtual bool CanAcquireBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	virtual bool TryAttachBindingPropertyExpression(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	virtual bool TrySetBindingPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	virtual bool ClearBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	virtual bool IsBindingExpressionOwner(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) const;

	bool TrySetEffectiveValueEntry(
		const DependencyPropertyMetadata& metadata,
		std::optional<BindingValue> proposedValue,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind,
		const Binding* owner,
		std::wstring resourceKey,
		bool allowReadOnly);
	bool TrySetPropertyValueOwned(
		const std::wstring& propertyName,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	bool ClearPropertyValueOwned(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	void RetireBindingExpression(
		const std::wstring& propertyName,
		const Binding* owner);
	bool ApplyEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		bool allowReadOnly = false);
	bool TryResolveEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const EffectiveValueEntry& entry,
		BindingValue& value,
		DependencyPropertyValueSource& source) const;
	bool TryEvaluateEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const EffectiveValueEntry& entry,
		BindingValue& value,
		DependencyPropertyValueSource& source) const;
	bool TryGetEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		BindingValue& value);
	bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		DependencyPropertyValueSource source);
	bool ClearPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);
	size_t ClearPropertyValues(DependencyPropertyValueSource source);
	/** Updates metadata base storage without creating a precedence source. */
	bool TrySetPropertyBaseValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	/** Framework/attached-behavior equivalent of SetValue(DependencyPropertyKey). */
	bool TrySetReadOnlyPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool ClearReadOnlyPropertyValue(const std::wstring& propertyName);
	bool TrySetReadOnlyPropertyValue(
		const DependencyPropertyKey& key,
		const BindingValue& value);
	bool ClearReadOnlyPropertyValue(const DependencyPropertyKey& key);

	template<typename TValue>
	bool SetPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);
	template<typename TValue>
	bool SetReadOnlyPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);
	template<typename TValue>
	bool SetReadOnlyPropertyField(
		const DependencyPropertyKey& key,
		TValue& storage,
		TValue value);
	template<typename TValue>
	bool SetCurrentPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);
	/** Typed CLR-wrapper read from the canonical effective-value store. */
	template<typename TValue>
	TValue GetDependencyPropertyValue(
		const std::wstring& propertyName) const;
	template<typename TValue>
	TValue GetDependencyPropertyValue(
		const DependencyProperty& property) const;
	/** Typed CLR-wrapper SetValue path; establishes a Local contribution. */
	template<typename TValue>
	bool SetDependencyPropertyValue(
		const std::wstring& propertyName,
		TValue value);
	template<typename TValue>
	bool SetDependencyPropertyValue(
		const DependencyProperty& property,
		TValue value);

	friend class Binding;
	friend class BindingCollection;
	friend class DependencyPropertyMetadata;
	friend class DependencyPropertyRegistry;

public:
	DependencyObject() = default;
	~DependencyObject() override;

	DependencyPropertyChangedEvent OnPropertyValueChanged;
	const DependencyProperty* FindDependencyProperty(
		const std::wstring& propertyName);
	const DependencyPropertyMetadata* FindPropertyMetadata(
		const std::wstring& propertyName);
	const DependencyPropertyMetadata* GetPropertyMetadata(
		const DependencyProperty& property);
	virtual bool TryGetPropertyValue(
		const std::wstring& propertyName,
		BindingValue& out);
	bool TryGetPropertyValue(
		const DependencyProperty& property,
		BindingValue& out);
	bool TryGetPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		BindingValue& out);
	virtual bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool TrySetPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value);
	/** WPF SetValue(DependencyPropertyKey) authorization path. */
	bool TrySetPropertyValue(
		const DependencyPropertyKey& key,
		const BindingValue& value);
	virtual bool TrySetCurrentPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool TrySetCurrentPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value);
	/** Re-runs coercion while preserving the property's base value and source. */
	bool CoerceValue(const std::wstring& propertyName);
	bool CoerceValue(const DependencyProperty& property);
	/** WPF ClearValue semantics: removes only the Local contribution. */
	bool ClearPropertyValue(const std::wstring& propertyName);
	bool ClearPropertyValue(const DependencyProperty& property);
	/** WPF ClearValue(DependencyPropertyKey) authorization path. */
	bool ClearPropertyValue(const DependencyPropertyKey& key);
	/** Clears all Local values and expressions from this object. */
	size_t ClearPropertyValues();
	bool HasPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);
	DependencyPropertyValueSource GetPropertyValueSource(
		const std::wstring& propertyName);
	DependencyPropertyExpressionKind GetPropertyExpressionKind(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);
	bool ResetPropertyValue(const std::wstring& propertyName);
	bool IsPropertyValueDefault(const std::wstring& propertyName);
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

	virtual void EnsureBindingPropertiesRegistered() {}
};

#endif // CUI_DEPENDENCY_OBJECT_H_INCLUDED
