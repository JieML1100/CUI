#include "Binding.h"
#include "Control.h"

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <mutex>
#include <stdexcept>
#include <unordered_set>

#if !CUI_ENABLE_DYNAMIC_XAML
#error DependencyPropertyRegistry.Design.cpp belongs only to the Design runtime.
#endif

/**
 * Process-lifetime metadata index owned by one native DependencyProperty.
 *
 * Registration is rare and publishes a new immutable layer snapshot. Typed
 * property access reads that snapshot without taking the global registry
 * mutex or scanning unrelated properties. Effective merged metadata is also
 * published as an immutable snapshot after its first resolution.
 */
class DependencyPropertyMetadataCache final
{
public:
	using LayerCollection =
		std::vector<const DependencyPropertyMetadata*>;

	explicit DependencyPropertyMetadataCache(
		const DependencyPropertyMetadata& defaultMetadata)
	{
		auto layers = std::make_shared<LayerCollection>();
		layers->push_back(&defaultMetadata);
		PublishLayers(std::move(layers));
	}

	std::shared_ptr<const LayerCollection> Layers() const noexcept
	{
		return _layers.load(std::memory_order_acquire);
	}

	void AddLayer(const DependencyPropertyMetadata& metadata)
	{
		std::scoped_lock lock(_mutex);
		const auto current = _layers.load(std::memory_order_relaxed);
		auto next = current
			? std::make_shared<LayerCollection>(*current)
			: std::make_shared<LayerCollection>();
		next->push_back(&metadata);
		PublishLayers(std::move(next));
	}

	const DependencyPropertyMetadata* Resolve(
		std::span<const DependencyPropertyMetadata* const> layers)
	{
		if (layers.empty()) return nullptr;
		const auto registered = Layers();
		if (!registered || registered->empty()) return nullptr;
		const auto* defaultMetadata = registered->front();
		if (layers.size() == 1 && layers.front() == defaultMetadata)
			return defaultMetadata;

		const auto published =
			_resolved.load(std::memory_order_acquire);
		if (const auto* cached = FindResolved(published, layers))
			return cached;

		std::scoped_lock lock(_mutex);
		for (const auto& cached : _resolvedStorage)
		{
			if (cached && LayersEqual(cached->Layers, layers))
				return cached->Metadata.get();
		}

		auto effective =
			std::make_unique<DependencyPropertyMetadata>(*defaultMetadata);
		for (const auto* layer : layers)
		{
			if (!layer || layer == defaultMetadata) continue;
			auto derived =
				std::make_unique<DependencyPropertyMetadata>(*layer);
			derived->MergeBaseMetadata(*effective);
			effective = std::move(derived);
		}

		auto cached = std::make_unique<ResolvedMetadata>();
		cached->Layers.assign(layers.begin(), layers.end());
		cached->Metadata = std::move(effective);
		const auto* cachedPointer = cached.get();
		const auto* result = cachedPointer->Metadata.get();
		_resolvedStorage.push_back(std::move(cached));

		const auto current =
			_resolved.load(std::memory_order_relaxed);
		auto next = current
			? std::make_shared<ResolvedCollection>(*current)
			: std::make_shared<ResolvedCollection>();
		next->push_back(cachedPointer);
		std::shared_ptr<const ResolvedCollection> immutable =
			std::move(next);
		_resolved.store(
			std::move(immutable), std::memory_order_release);
		return result;
	}

private:
	struct ResolvedMetadata final
	{
		LayerCollection Layers;
		std::unique_ptr<DependencyPropertyMetadata> Metadata;
	};
	using ResolvedCollection =
		std::vector<const ResolvedMetadata*>;

	mutable std::mutex _mutex;
	std::atomic<std::shared_ptr<const LayerCollection>> _layers;
	std::atomic<std::shared_ptr<const ResolvedCollection>> _resolved;
	std::vector<std::unique_ptr<ResolvedMetadata>> _resolvedStorage;

	void PublishLayers(std::shared_ptr<LayerCollection> layers)
	{
		std::shared_ptr<const LayerCollection> immutable =
			std::move(layers);
		_layers.store(
			std::move(immutable), std::memory_order_release);
	}

	static bool LayersEqual(
		const LayerCollection& left,
		std::span<const DependencyPropertyMetadata* const> right)
	{
		return left.size() == right.size()
			&& std::equal(left.begin(), left.end(), right.begin());
	}

	static const DependencyPropertyMetadata* FindResolved(
		const std::shared_ptr<const ResolvedCollection>& entries,
		std::span<const DependencyPropertyMetadata* const> layers)
	{
		if (!entries) return nullptr;
		for (const auto* entry : *entries)
		{
			if (entry && LayersEqual(entry->Layers, layers))
				return entry->Metadata.get();
		}
		return nullptr;
	}
};

namespace
{
	bool IsRegistryPropertyNameEqual(
		const std::wstring& left,
		const std::wstring& right)
	{
		return left == right;
	}

	bool IsRegistryPropertyNameLess(
		const std::wstring& left,
		const std::wstring& right)
	{
		const auto common = (std::min)(left.size(), right.size());
		for (std::size_t index = 0; index < common; ++index)
		{
			const auto leftCharacter = std::towlower(left[index]);
			const auto rightCharacter = std::towlower(right[index]);
			if (leftCharacter != rightCharacter)
				return leftCharacter < rightCharacter;
		}
		return left.size() < right.size();
	}

	std::vector<std::unique_ptr<DependencyProperty>>&
		RegisteredDependencyProperties()
	{
		static std::vector<std::unique_ptr<DependencyProperty>> properties;
		return properties;
	}

	std::vector<std::unique_ptr<DependencyPropertyMetadata>>&
		RegisteredBindingProperties()
	{
		static std::vector<std::unique_ptr<DependencyPropertyMetadata>> properties;
		return properties;
	}

	template<typename T>
	void ReserveRegistryAppend(
		std::vector<T>& values,
		std::size_t count = 1)
	{
		const auto required = values.size() + count;
		if (required <= values.capacity()) return;
		const auto grown = values.capacity()
			? values.capacity() * 2 : static_cast<std::size_t>(64);
		values.reserve((std::max)(required, grown));
	}

	/**
	 * Process-lifetime flat token index for native dependency-property metadata.
	 *
	 * Multiple identities may legitimately share a canonical property name
	 * (and therefore a token) when unrelated owner hierarchies register the
	 * same member. Their layers stay in registration order so lookup can apply
	 * the same most-derived/latest-owner selection as the name registry without
	 * scanning unrelated properties. Different names sharing one 64-bit token
	 * are rejected at registration.
	 */
	struct BindingPropertyTokenLayer final
	{
		std::uint64_t Token = 0;
		const DependencyPropertyMetadata* Metadata = nullptr;
	};

	std::vector<BindingPropertyTokenLayer>&
		RegisteredBindingPropertyTokenLayers()
	{
		static std::vector<BindingPropertyTokenLayer> layers;
		return layers;
	}

	std::uint64_t PrepareBindingPropertyTokenLayer(
		const std::wstring& name)
	{
		const auto token = MakeBindingSourcePropertyToken(name);
		auto& layers = RegisteredBindingPropertyTokenLayers();
		const auto found = std::lower_bound(
			layers.begin(), layers.end(), token.Value,
			[](const BindingPropertyTokenLayer& layer, std::uint64_t value)
			{
				return layer.Token < value;
			});
		if (found != layers.end() && found->Token == token.Value
			&& found->Metadata && found->Metadata->Name() != name)
			throw std::invalid_argument(
				"Dependency property binding token collision");

		const auto required = layers.size() + 1;
		if (required > layers.capacity())
		{
			const auto grown = layers.capacity()
				? layers.capacity() * 2 : static_cast<std::size_t>(64);
			layers.reserve((std::max)(required, grown));
		}
		return token.Value;
	}

	void InsertBindingPropertyTokenLayer(
		std::uint64_t token,
		const DependencyPropertyMetadata& metadata)
	{
		auto& layers = RegisteredBindingPropertyTokenLayers();
		const auto insertion = std::upper_bound(
			layers.begin(), layers.end(), token,
			[](std::uint64_t value, const BindingPropertyTokenLayer& layer)
			{
				return value < layer.Token;
			});
		layers.insert(
			insertion, BindingPropertyTokenLayer{ token, &metadata });
	}

	std::size_t& NextDependencyPropertyGlobalIndex()
	{
		static std::size_t value = 0;
		return value;
	}

	std::mutex& BindingPropertyMutex()
	{
		static std::mutex mutex;
		return mutex;
	}
}

std::unique_ptr<DependencyProperty>
DependencyPropertyRegistry::CreateStandalone(
	DependencyPropertyMetadata& metadata)
{
	if (metadata._name.empty()) return {};
	std::scoped_lock lock(BindingPropertyMutex());
	auto authorization = metadata._isReadOnly
		? std::make_shared<const unsigned char>(0)
		: std::shared_ptr<const unsigned char>{};
	auto property = std::unique_ptr<DependencyProperty>(
		new DependencyProperty(
			metadata._name,
			metadata._valueKind,
			metadata._valueType,
			metadata._ownerType,
			NextDependencyPropertyGlobalIndex()++,
			metadata._validator,
			std::move(authorization)));
	metadata.AttachProperty(*property);
	property->_standaloneMetadata = &metadata;
	if (metadata._hasDefaultValue
		&& !property->IsValidValue(metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");
	return property;
}

const DependencyProperty* DependencyPropertyRegistry::Register(
	DependencyPropertyMetadata metadata)
{
	if (metadata._name.empty())
		throw std::invalid_argument(
			"Dependency property name cannot be empty");
	std::scoped_lock lock(BindingPropertyMutex());
	for (const auto& existing : RegisteredBindingProperties())
	{
		if (existing->OwnerType() == metadata.OwnerType()
			&& IsRegistryPropertyNameEqual(
				existing->Name(), metadata.Name()))
			throw std::invalid_argument(
				"Dependency property is already registered for this owner");
	}

	auto authorization = metadata._isReadOnly
		? std::make_shared<const unsigned char>(0)
		: std::shared_ptr<const unsigned char>{};
	auto property = std::unique_ptr<DependencyProperty>(
		new DependencyProperty(
			metadata._name,
			metadata._valueKind,
			metadata._valueType,
			metadata._ownerType,
			NextDependencyPropertyGlobalIndex()++,
			metadata._validator,
			std::move(authorization)));
	metadata.AttachProperty(*property);
	if (metadata._hasDefaultValue
		&& !property->IsValidValue(metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");

	auto storedMetadata =
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata));
	property->_metadataCache =
		std::make_shared<DependencyPropertyMetadataCache>(*storedMetadata);
	const auto bindingToken = PrepareBindingPropertyTokenLayer(
		storedMetadata->Name());
	ReserveRegistryAppend(RegisteredDependencyProperties());
	ReserveRegistryAppend(RegisteredBindingProperties());
	const auto* result = property.get();
	const auto* resultMetadata = storedMetadata.get();
	RegisteredDependencyProperties().push_back(std::move(property));
	RegisteredBindingProperties().push_back(std::move(storedMetadata));
	InsertBindingPropertyTokenLayer(bindingToken, *resultMetadata);
	return result;
}

DependencyPropertyKey DependencyPropertyRegistry::RegisterReadOnly(
	DependencyPropertyMetadata metadata)
{
	metadata._isReadOnly = true;
	if (metadata._name.empty())
		throw std::invalid_argument(
			"Dependency property name cannot be empty");
	std::scoped_lock lock(BindingPropertyMutex());
	for (const auto& existing : RegisteredBindingProperties())
	{
		if (existing->OwnerType() != metadata.OwnerType()
			|| !IsRegistryPropertyNameEqual(
				existing->Name(), metadata.Name()))
			continue;
		throw std::invalid_argument(
			"Dependency property is already registered for this owner");
	}

	auto authorization = std::make_shared<const unsigned char>(0);
	auto property = std::unique_ptr<DependencyProperty>(
		new DependencyProperty(
			metadata._name,
			metadata._valueKind,
			metadata._valueType,
			metadata._ownerType,
			NextDependencyPropertyGlobalIndex()++,
			metadata._validator,
			authorization));
	metadata.AttachProperty(*property);
	if (metadata._hasDefaultValue
		&& !property->IsValidValue(metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");

	auto storedMetadata =
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata));
	property->_metadataCache =
		std::make_shared<DependencyPropertyMetadataCache>(*storedMetadata);
	const auto bindingToken = PrepareBindingPropertyTokenLayer(
		storedMetadata->Name());
	ReserveRegistryAppend(RegisteredDependencyProperties());
	ReserveRegistryAppend(RegisteredBindingProperties());
	const auto* result = property.get();
	const auto* resultMetadata = storedMetadata.get();
	RegisteredDependencyProperties().push_back(std::move(property));
	RegisteredBindingProperties().push_back(std::move(storedMetadata));
	InsertBindingPropertyTokenLayer(bindingToken, *resultMetadata);
	return DependencyPropertyKey(*result, std::move(authorization));
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::AddOwner(
	const DependencyProperty& property,
	DependencyPropertyMetadata metadata,
	const DependencyPropertyKey* key)
{
	std::scoped_lock lock(BindingPropertyMutex());
	if (property.ReadOnly()
		? (!key || !property.Authorizes(*key))
		: key != nullptr)
		return nullptr;
	if (metadata._valueKind != property.ValueKind()
		|| metadata._valueType != property.ValueType())
		return nullptr;

	bool propertyRegistered = false;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() != &property) continue;
		propertyRegistered = true;
		if (candidate->OwnerType() == metadata.OwnerType())
			return nullptr;
	}
	if (!propertyRegistered) return nullptr;

	metadata.AttachProperty(property);
	if (metadata._hasDefaultValue
		&& !property.IsValidValue(metadata._defaultValue))
		return nullptr;
	auto stored =
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata));
	const auto bindingToken = PrepareBindingPropertyTokenLayer(stored->Name());
	ReserveRegistryAppend(RegisteredBindingProperties());
	const auto* result = stored.get();
	RegisteredBindingProperties().push_back(std::move(stored));
	InsertBindingPropertyTokenLayer(bindingToken, *result);
	property._metadataCache->AddLayer(*result);
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::OverrideMetadata(
	const DependencyProperty& property,
	DependencyPropertyMetadata metadata,
	const DependencyPropertyKey* key)
{
	std::scoped_lock lock(BindingPropertyMutex());
	if (property.ReadOnly()
		? (!key || !property.Authorizes(*key))
		: key != nullptr)
		return nullptr;
	if (metadata._valueKind != property.ValueKind()
		|| metadata._valueType != property.ValueType())
		return nullptr;

	bool propertyRegistered = false;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() != &property) continue;
		propertyRegistered = true;
		if (candidate->OwnerType() == metadata.OwnerType())
			return nullptr;
	}
	if (!propertyRegistered) return nullptr;

	metadata.AttachProperty(property);
	if (metadata._hasDefaultValue
		&& !property.IsValidValue(metadata._defaultValue))
		return nullptr;
	auto stored =
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata));
	const auto bindingToken = PrepareBindingPropertyTokenLayer(stored->Name());
	ReserveRegistryAppend(RegisteredBindingProperties());
	const auto* result = stored.get();
	RegisteredBindingProperties().push_back(std::move(stored));
	InsertBindingPropertyTokenLayer(bindingToken, *result);
	property._metadataCache->AddLayer(*result);
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::ResolveMetadata(
	const DependencyProperty& property,
	std::span<const DependencyPropertyMetadata* const> layers)
{
	return property._metadataCache
		? property._metadataCache->Resolve(layers)
		: nullptr;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::Find(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	target.EnsureBindingPropertiesRegistered();
	if (const auto* objectProperty =
		target.FindObjectPropertyMetadataByName(propertyName))
		return objectProperty;
	return FindNativeCore(target, propertyName);
}

const DependencyProperty* DependencyPropertyRegistry::FindProperty(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	const auto* metadata = Find(target, propertyName);
	return metadata ? &metadata->Property() : nullptr;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::GetMetadata(
	DependencyObject& target,
	const DependencyProperty& property)
{
	if (!property._metadataCache)
	{
		// Standalone identities belong to the object-local declarative schema,
		// not the native registry. Resolve them against the descriptor currently
		// attached to this Design object; native UIClass filtering must not reject
		// component-owned properties merely because their behavior host is Control.
		const auto* declarative =
			target.FindObjectPropertyMetadataByName(property.Name());
		return declarative && &declarative->Property() == &property
			? declarative : nullptr;
	}
	target.EnsureBindingPropertiesRegistered();

	const auto registeredLayers = property._metadataCache->Layers();
	if (!registeredLayers) return nullptr;
	if (registeredLayers->size() == 1)
	{
		const auto* metadata = registeredLayers->front();
		return metadata
			&& metadata->Matches(target)
			&& target.SupportsNativeProperty(*metadata)
			? metadata
			: nullptr;
	}
	std::vector<const DependencyPropertyMetadata*> layers;
	layers.reserve(registeredLayers->size());
	for (const auto* candidate : *registeredLayers)
	{
		if (candidate
			&& candidate->Matches(target)
			&& target.SupportsNativeProperty(*candidate))
			layers.push_back(candidate);
	}
	return ResolveMetadata(property, layers);
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::FindNative(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	target.EnsureBindingPropertiesRegistered();
	return FindNativeCore(target, propertyName);
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::Find(
	DependencyObject& target,
	BindingSourcePropertyToken property)
{
	if (!property) return nullptr;
	target.EnsureBindingPropertiesRegistered();
	const DependencyPropertyMetadata* objectProperty = nullptr;
	// Standalone declarative properties belong to a document-owned descriptor,
	// not the process registry. Design resolves them through the descriptor's
	// own token hash table.
	if (const auto* control = dynamic_cast<const Control*>(&target))
	{
		const auto& descriptor = control->GetDeclarativeTypeDescriptor();
		if (descriptor) objectProperty = descriptor->FindProperty(property);
	}

	const DependencyProperty* nativeIdentity = nullptr;
	{
		std::scoped_lock lock(BindingPropertyMutex());
		const auto& tokenLayers = RegisteredBindingPropertyTokenLayers();
		const auto first = std::lower_bound(
			tokenLayers.begin(), tokenLayers.end(), property.Value,
			[](const BindingPropertyTokenLayer& layer, std::uint64_t value)
			{
				return layer.Token < value;
			});
		if (first == tokenLayers.end() || first->Token != property.Value)
			return objectProperty;
		const auto last = std::upper_bound(
			first, tokenLayers.end(), property.Value,
			[](std::uint64_t value, const BindingPropertyTokenLayer& layer)
			{
				return value < layer.Token;
			});

		// Declarative descriptors own their own token index. Refuse a collision
		// with a native name, while retaining the established rule that an
		// object-specific declaration shadows a same-named native property.
		if (objectProperty)
			return objectProperty->Name() == first->Metadata->Name()
				? objectProperty : nullptr;

		for (auto it = last; it != first;)
		{
			const auto* candidate = (--it)->Metadata;
			if (!candidate || !candidate->Matches(target)
				|| !target.SupportsNativeProperty(*candidate))
				continue;
			nativeIdentity = &candidate->Property();
			break;
		}
	}
	if (!nativeIdentity) return nullptr;
	return GetMetadata(target, *nativeIdentity);
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::FindNativeCore(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	std::scoped_lock lock(BindingPropertyMutex());
	auto& properties = RegisteredBindingProperties();
	const DependencyProperty* identity = nullptr;
	for (auto it = properties.rbegin(); it != properties.rend(); ++it)
	{
		if (IsRegistryPropertyNameEqual((*it)->Name(), propertyName)
			&& (*it)->Matches(target)
			&& target.SupportsNativeProperty(**it))
		{
			identity = &(*it)->Property();
			break;
		}
	}
	if (!identity) return nullptr;
	std::vector<const DependencyPropertyMetadata*> layers;
	for (const auto& candidate : properties)
	{
		if (&candidate->Property() == identity
			&& candidate->Matches(target)
			&& target.SupportsNativeProperty(*candidate))
			layers.push_back(candidate.get());
	}
	return ResolveMetadata(*identity, layers);
}

std::vector<const DependencyPropertyMetadata*>
DependencyPropertyRegistry::GetProperties(DependencyObject& target)
{
	target.EnsureBindingPropertiesRegistered();
	std::scoped_lock lock(BindingPropertyMutex());
	std::vector<const DependencyPropertyMetadata*> result =
		target.GetObjectPropertyMetadata();
	auto& properties = RegisteredBindingProperties();
	std::unordered_set<std::wstring> effectiveNames;
	effectiveNames.reserve(result.size() + properties.size());
	for (const auto* property : result)
		if (property) effectiveNames.insert(property->Name());
	for (auto it = properties.rbegin(); it != properties.rend(); ++it)
	{
		const auto* candidate = it->get();
		if (!candidate->Matches(target)
			|| !target.SupportsNativeProperty(*candidate))
			continue;
		if (!effectiveNames.insert(candidate->Name()).second)
			continue;
		std::vector<const DependencyPropertyMetadata*> layers;
		for (const auto& layer : properties)
		{
			if (&layer->Property() == &candidate->Property()
				&& layer->Matches(target)
				&& target.SupportsNativeProperty(*layer))
				layers.push_back(layer.get());
		}
		if (const auto* metadata =
			ResolveMetadata(candidate->Property(), layers))
			result.push_back(metadata);
	}
	std::sort(result.begin(), result.end(),
		[](const DependencyPropertyMetadata* left,
			const DependencyPropertyMetadata* right)
		{
			return IsRegistryPropertyNameLess(
				left->Name(), right->Name());
		});
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::FindRegistered(
	std::span<const std::type_index> ownerTypes,
	const std::wstring& propertyName)
{
	std::scoped_lock lock(BindingPropertyMutex());
	const DependencyProperty* identity = nullptr;
	for (auto it = RegisteredBindingProperties().rbegin();
		it != RegisteredBindingProperties().rend(); ++it)
	{
		const auto* candidate = it->get();
		if (!IsRegistryPropertyNameEqual(candidate->Name(), propertyName))
			continue;
		if (std::find(ownerTypes.begin(), ownerTypes.end(), candidate->OwnerType())
			!= ownerTypes.end())
		{
			identity = &candidate->Property();
			break;
		}
	}
	if (!identity) return nullptr;
	std::vector<const DependencyPropertyMetadata*> layers;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() == identity
			&& std::find(
				ownerTypes.begin(), ownerTypes.end(), candidate->OwnerType())
				!= ownerTypes.end())
			layers.push_back(candidate.get());
	}
	return ResolveMetadata(*identity, layers);
}

std::vector<const DependencyPropertyMetadata*>
DependencyPropertyRegistry::GetRegisteredProperties(
	std::span<const std::type_index> ownerTypes,
	std::function<bool(const DependencyPropertyMetadata&)> include)
{
	std::scoped_lock lock(BindingPropertyMutex());
	std::vector<const DependencyPropertyMetadata*> result;
	std::unordered_set<std::wstring> effectiveNames;
	for (auto it = RegisteredBindingProperties().rbegin();
		it != RegisteredBindingProperties().rend(); ++it)
	{
		const auto* candidate = it->get();
		if (std::find(ownerTypes.begin(), ownerTypes.end(), candidate->OwnerType())
			== ownerTypes.end()) continue;
		if (include && !include(*candidate)) continue;
		if (!effectiveNames.insert(candidate->Name()).second)
			continue;
		std::vector<const DependencyPropertyMetadata*> layers;
		for (const auto& layer : RegisteredBindingProperties())
		{
			if (&layer->Property() == &candidate->Property()
				&& std::find(
					ownerTypes.begin(), ownerTypes.end(), layer->OwnerType())
					!= ownerTypes.end()
				&& (!include || include(*layer)))
				layers.push_back(layer.get());
		}
		if (const auto* metadata =
			ResolveMetadata(candidate->Property(), layers))
			result.push_back(metadata);
	}
	std::sort(result.begin(), result.end(),
		[](const DependencyPropertyMetadata* left,
			const DependencyPropertyMetadata* right)
		{
			return IsRegistryPropertyNameLess(
				left->Name(), right->Name());
		});
	return result;
}
