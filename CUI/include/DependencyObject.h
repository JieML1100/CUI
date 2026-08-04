#ifndef CUI_DEPENDENCY_OBJECT_H_INCLUDED
#define CUI_DEPENDENCY_OBJECT_H_INCLUDED
#pragma once

#include "Binding.h"
#include "CuiBuildFeatures.h"
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
	BindingValue OldValue;
	BindingValue NewValue;
	const DependencyProperty* Property = nullptr;

	DependencyPropertyChangedEventArgs() = default;
	DependencyPropertyChangedEventArgs(
		const DependencyProperty& property,
		const BindingValue& oldValue,
		const BindingValue& newValue)
		: OldValue(oldValue), NewValue(newValue), Property(&property)
	{
	}

	/** Allocation-free diagnostic projection of the stable property identity. */
	[[nodiscard]] const std::wstring& Name() const noexcept
	{
		if (Property) return Property->Name();
		static const std::wstring empty;
		return empty;
	}
};

using DependencyPropertyChangedEvent = Event<void(
	class DependencyObject*, const DependencyPropertyChangedEventArgs&)>;

/** Owns dependency-property values, expressions and change observation. */
class DependencyObject : public DispatcherObject, public IBindingSource
{
public:
	/**
	 * One already-committed dependency-property value change whose public
	 * notifications are intentionally delayed. Framework transactions use this
	 * to make all related getters final before the first callback is raised.
	 */
	class DeferredPropertyChange final
	{
	public:
		bool HasValue() const noexcept { return Metadata != nullptr; }
		const DependencyProperty* Property() const noexcept
		{
			return Metadata ? &Metadata->Property() : nullptr;
		}
		const BindingValue& OldValue() const noexcept { return Previous; }
		const BindingValue& NewValue() const noexcept { return Current; }

	private:
		friend class DependencyObject;
		const DependencyPropertyMetadata* Metadata = nullptr;
		BindingValue Previous;
		BindingValue Current;
	};

protected:
	using DeclarativePropertyMetadata = DependencyPropertyMetadata;
	using DeclarativePropertyMetadataPointer =
		const DeclarativePropertyMetadata*;
	using DeclarativePropertyMetadataCollection =
		std::vector<DeclarativePropertyMetadataPointer>;
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
	const DependencyPropertyMetadata* _applyingPropertyMetadata = nullptr;
	DependencyPropertyValueSource _applyingPropertySource =
		DependencyPropertyValueSource::Default;
	bool _refreshingInheritedProperties = false;
	DeferredPropertyChange* _stagingPropertyChange = nullptr;
	unsigned long long _propertyChangeVersion = 0;
	bool _isDestroying = false;
	PropertyChangedEvent _bindingSourcePropertyChanged;
#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<EventConnection> _bindingSourceMetadataConnections;
	bool _bindingSourceMetadataConnectionsInitialized = false;
#endif
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
	bool DeferPropertyMetadataChange(
		const DeclarativePropertyMetadata& metadata,
		const BindingValue& oldValue,
		const BindingValue& newValue);
	virtual void OnBindingValidationChanged(
		const std::wstring& targetProperty)
	{
		(void)targetProperty;
	}
	virtual DeclarativePropertyMetadataPointer FindObjectPropertyMetadataByName(
		const std::wstring& propertyName) const
	{
		(void)propertyName;
		return nullptr;
	}
	virtual DeclarativePropertyMetadataCollection
		GetObjectPropertyMetadata() const
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
	/**
	 * Sparse exact-identity metadata hook used by accessor-owned AddOwner and
	 * override relations. Derived types may touch only their exceptional
	 * relation accessors before delegating; the default never enumerates a type's
	 * property surface.
	 */
	virtual const DependencyPropertyMetadata*
		ResolveExactDependencyPropertyMetadata(
			const DependencyProperty& property) const;
	/** Default metadata observation; UIElement overrides validation commits. */
	virtual EventConnection SubscribeDefaultPropertyChange(
		const DependencyProperty& property,
		DependencyPropertyChangeHandler handler,
		DataSourceUpdateMode updateMode);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool CanAcquireBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#endif
	bool CanAcquireBindingPropertyValue(
		const DependencyPropertyMetadata& metadata,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool TryAttachBindingPropertyExpression(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#endif
	bool TryAttachBindingPropertyExpression(
		const DependencyPropertyMetadata& metadata,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool TrySetBindingPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#endif
	bool TrySetBindingPropertyValue(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool ClearBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#endif
	bool ClearBindingPropertyValue(
		const DependencyPropertyMetadata& metadata,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool IsBindingExpressionOwner(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) const;
	#endif
	bool IsBindingExpressionOwner(
		const DependencyPropertyMetadata& metadata,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) const;

	bool TrySetEffectiveValueEntry(
		const DependencyPropertyMetadata& metadata,
		std::optional<BindingValue> proposedValue,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind,
		const Binding* owner,
		std::wstring resourceKey);

private:
	friend class Control;

	// Only DependencyObject and explicitly trusted friends may opt into
	// read-only mutation after validating the corresponding capability.
	bool TrySetEffectiveValueEntry(
		const DependencyPropertyMetadata& metadata,
		std::optional<BindingValue> proposedValue,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind,
		const Binding* owner,
		std::wstring resourceKey,
		bool allowReadOnly);
	bool ApplyEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		bool allowReadOnly);

	// Key/read-only authorization is resolved by trusted DependencyObject
	// entry points before these raw metadata operations are reached.
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TrySetPropertyValueOwned(
		const std::wstring& propertyName,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	#endif
	bool TrySetPropertyValueOwned(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	#if CUI_ENABLE_DYNAMIC_XAML
	bool ClearPropertyValueOwned(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	#endif

	/** Metadata has already been resolved for this exact target and identity. */
	bool ClearPropertyValueOwned(
		const DependencyPropertyMetadata& metadata,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);

protected:
	void RetireBindingExpression(
		const DependencyProperty& property,
		const Binding* owner);
	bool ApplyEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value,
		DependencyPropertyValueSource source);
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
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		DependencyPropertyValueSource source);
	#endif
	bool TrySetPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value,
		DependencyPropertyValueSource source);
	#if CUI_ENABLE_DYNAMIC_XAML
	bool ClearPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);
	#endif
	bool ClearPropertyValue(
		const DependencyProperty& property,
		DependencyPropertyValueSource source);
	size_t ClearPropertyValues(DependencyPropertyValueSource source);
	/** Updates metadata base storage without creating a precedence source. */
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TrySetPropertyBaseValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	#endif
	bool TrySetPropertyBaseValueCore(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& convertedValue);
	bool TrySetCurrentPropertyValueCore(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value);
	bool CoerceValueCore(const DependencyPropertyMetadata& metadata);
	/** Framework/attached-behavior equivalent of SetValue(DependencyPropertyKey). */
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TrySetReadOnlyPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool ClearReadOnlyPropertyValue(const std::wstring& propertyName);
	#endif
	bool TrySetReadOnlyPropertyValue(
		const DependencyPropertyKey& key,
		const BindingValue& value);
	bool ClearReadOnlyPropertyValue(const DependencyPropertyKey& key);

	#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TValue>
	bool SetPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);
	#endif
	template<typename TValue>
	bool SetPropertyField(
		const DependencyProperty& property,
		TValue& storage,
		TValue value);
	#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TValue>
	bool SetReadOnlyPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);
	#endif
	template<typename TValue>
	bool SetReadOnlyPropertyField(
		const DependencyPropertyKey& key,
		TValue& storage,
		TValue value);
	template<typename TValue>
	bool StageReadOnlyPropertyField(
		const DependencyPropertyKey& key,
		TValue& storage,
		TValue value,
		DeferredPropertyChange& change);
	void PublishDeferredPropertyChange(
		const DeferredPropertyChange& change);
	#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TValue>
	bool SetCurrentPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);
	#endif
	template<typename TValue>
	bool SetCurrentPropertyField(
		const DependencyProperty& property,
		TValue& storage,
		TValue value);
	/** Typed CLR-wrapper read from the canonical effective-value store. */
	#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TValue>
	TValue GetDependencyPropertyValue(
		const std::wstring& propertyName) const;
	#endif
	template<typename TValue>
	TValue GetDependencyPropertyValue(
		const DependencyProperty& property) const;
	/** Typed CLR-wrapper SetValue path; establishes a Local contribution. */
	#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TValue>
	bool SetDependencyPropertyValue(
		const std::wstring& propertyName,
		TValue value);
	#endif
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
	#if CUI_ENABLE_DYNAMIC_XAML
	const DependencyProperty* FindDependencyProperty(
		const std::wstring& propertyName);
	const DependencyPropertyMetadata* FindPropertyMetadata(
		const std::wstring& propertyName);
	#endif
	const DependencyPropertyMetadata* GetPropertyMetadata(
		const DependencyProperty& property);
	const DependencyPropertyMetadata* GetPropertyMetadata(
		BindingSourcePropertyToken property);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool TryGetPropertyValue(
		const std::wstring& propertyName,
		BindingValue& out);
	#endif
	bool TryGetPropertyValue(
		const DependencyProperty& property,
		BindingValue& out);
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TryGetPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		BindingValue& out);
	#endif
	bool TryGetPropertyValue(
		const DependencyProperty& property,
		DependencyPropertyValueSource source,
		BindingValue& out);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	#endif
	bool TrySetPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value);
	/** WPF SetValue(DependencyPropertyKey) authorization path. */
	bool TrySetPropertyValue(
		const DependencyPropertyKey& key,
		const BindingValue& value);
	#if CUI_ENABLE_DYNAMIC_XAML
	virtual bool TrySetCurrentPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	#endif
	bool TrySetCurrentPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value);
	/** Re-runs coercion while preserving the property's base value and source. */
	#if CUI_ENABLE_DYNAMIC_XAML
	bool CoerceValue(const std::wstring& propertyName);
	#endif
	bool CoerceValue(const DependencyProperty& property);
	/** WPF ClearValue semantics: removes only the Local contribution. */
	#if CUI_ENABLE_DYNAMIC_XAML
	bool ClearPropertyValue(const std::wstring& propertyName);
	#endif
	bool ClearPropertyValue(const DependencyProperty& property);
	/** WPF ClearValue(DependencyPropertyKey) authorization path. */
	bool ClearPropertyValue(const DependencyPropertyKey& key);
	/** Clears all Local values and expressions from this object. */
	size_t ClearPropertyValues();
	#if CUI_ENABLE_DYNAMIC_XAML
	bool HasPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);
	#endif
	bool HasPropertyValue(
		const DependencyProperty& property,
		DependencyPropertyValueSource source);
	#if CUI_ENABLE_DYNAMIC_XAML
	DependencyPropertyValueSource GetPropertyValueSource(
		const std::wstring& propertyName);
	DependencyPropertyExpressionKind GetPropertyExpressionKind(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);
	bool ResetPropertyValue(const std::wstring& propertyName);
	bool IsPropertyValueDefault(const std::wstring& propertyName);
	#endif
	DependencyPropertyValueSource GetPropertyValueSource(
		const DependencyProperty& property);
	DependencyPropertyExpressionKind GetPropertyExpressionKind(
		const DependencyProperty& property,
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);
	bool ResetPropertyValue(const DependencyProperty& property);
	bool IsPropertyValueDefault(const DependencyProperty& property);
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TryGetValue(
		const std::wstring& propertyName,
		BindingValue& out) const override;
	#else
	/** Compile-time bridge for native implementation literals; no name lookup or
	 * name storage is present in the Production binding ABI. */
	template<std::size_t N>
	bool TryGetValue(
		const wchar_t (&propertyName)[N],
		BindingValue& out) const
	{
		static_assert(N > 1);
		return TryGetValue(MakeBindingSourcePropertyToken(
			std::wstring_view(propertyName, N - 1)), out);
	}
	#endif
	bool TryGetValue(
		BindingSourcePropertyToken property,
		BindingValue& out) const override;
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TrySetValue(
		const std::wstring& propertyName,
		const BindingValue& value) override;
	#endif
	bool TrySetValue(
		BindingSourcePropertyToken property,
		const BindingValue& value) override;
	#if CUI_ENABLE_DYNAMIC_XAML
	bool TryGetPropertyMetadata(
		const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const override;
	#endif
	bool TryGetPropertyMetadata(
		BindingSourcePropertyToken property,
		BindingSourcePropertyMetadata& out) const override;
	#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<BindingSourcePropertyMetadata> GetProperties() const override;
	#endif
	PropertyChangedEvent& PropertyChanged() override;
	unsigned long long PropertyChangeVersion() const noexcept
	{
		return _propertyChangeVersion;
	}
	bool IsDestroying() const noexcept { return _isDestroying; }

#if CUI_ENABLE_DYNAMIC_XAML
	virtual void EnsureBindingPropertiesRegistered() {}
#endif
};

#endif // CUI_DEPENDENCY_OBJECT_H_INCLUDED
