#include <Control.h>
#include <CompiledBindingRecord.h>
#include <CuiBuildFeatures.h>
#include <DependencyPropertyInfrastructure.h>
#include <Style.h>
#include <TemplateInfrastructure.h>
#include <XamlInfrastructure.h>

#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(CUI_ENABLE_DYNAMIC_XAML == 0,
	"AOT consumers must compile against the production CUI ABI");
static_assert(CUI_ENABLE_DESIGN_METADATA == 0,
	"AOT consumers must not expose dependency-property design metadata");

template<typename T>
concept HasDynamicXamlDescriptor = requires(const T& value)
{
	value.GetDeclarativeTypeDescriptor();
};

template<typename T>
concept HasDynamicComponentQNameSurface = requires(const T& value)
{
	value.GetDeclarativeTypeId();
	value.GetDeclarativeTypeNamespace();
	value.GetDeclarativeTypeName();
};

template<typename T>
concept HasDynamicComponentBehavior = requires(const T& value)
{
	value.GetDeclarativeComponentBehavior();
	value.HasDeclarativeComponentBehavior();
};

template<typename T>
concept HasDeclarativeEventName = requires(T& value)
{
	value.Name;
};

template<typename T>
concept HasDeclarativeEventOwnerType = requires(T& value)
{
	value.OwnerType;
};

template<typename T>
concept HasNamedDeclarativeEventLookup = requires(
	const T& value, const std::wstring& name)
{
	value.FindDeclarativeEvent(name);
};

template<typename T>
concept HasStringDeclarativeEventRaise = requires(
	T& value, std::wstring name)
{
	{ value.RaiseDeclarativeEvent(std::move(name), BindingValue{}) }
		-> std::same_as<bool>;
};

template<typename T>
concept HasTypedDeclarativeEventRaise = requires(
	T& value, const DeclarativeEventDefinition& definition)
{
	{ value.RaiseDeclarativeEvent(definition, BindingValue{}) }
		-> std::same_as<bool>;
};

template<typename T>
concept HasCompiledInteractionInstall = requires(
	Control& target,
	const CompiledInteractionProgramView& program,
	std::span<const BindingValue> values,
	std::span<Control* const> targets,
	std::wstring* error)
{
	{ T::InstallCompiledInteractions(target, program, values, targets, error) }
		-> std::same_as<bool>;
};

template<typename T>
concept HasEmbeddedCompiledInteractionValues = requires(T& value)
{
	value.Values;
};

template<typename T>
concept HasCompiledRecordIndexSource = requires(T& value, size_t index)
{
	{ value.MakeCompiledPropertySource(index) }
		-> std::same_as<CompiledSourceHandle>;
};

template<typename T>
concept HasPublicCompiledRecordEntrySource = requires(
	T& value, const CompiledBindingRecordProperty& property)
{
	{ value.MakeCompiledPropertySource(property) }
		-> std::same_as<CompiledSourceHandle>;
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyRegister = requires
{
	TRegistry::template Register<DependencyObject, int>(
		std::wstring{}, DependencyPropertyOptions<DependencyObject, int>{});
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyRegisterReadOnly = requires
{
	TRegistry::template RegisterReadOnly<DependencyObject, int>(
		std::wstring{}, DependencyPropertyOptions<DependencyObject, int>{});
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyAddOwner = requires(
	const DependencyProperty& property)
{
	TRegistry::template AddOwner<DependencyObject, int>(
		property, DependencyPropertyOptions<DependencyObject, int>{});
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyOverride = requires(
	const DependencyProperty& property)
{
	TRegistry::template OverrideMetadata<
		DependencyObject, DispatcherObject, int>(
			property, DependencyPropertyOptions<DependencyObject, int>{});
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyNameFind = requires(
	DependencyObject& target, const std::wstring& name)
{
	TRegistry::Find(target, name);
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyFindNative = requires(
	DependencyObject& target, const std::wstring& name)
{
	TRegistry::FindNative(target, name);
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyFindRegistered = requires(
	std::span<const std::type_index> owners, const std::wstring& name)
{
	TRegistry::FindRegistered(owners, name);
};

template<typename TRegistry>
concept HasLegacyDependencyPropertyGetRegisteredProperties = requires(
	std::span<const std::type_index> owners)
{
	TRegistry::GetRegisteredProperties(owners);
};

template<typename T>
concept HasLegacyInteractionInstall = requires
{
	&T::DefineInteractions;
};

template<typename T>
concept HasStringVisualStateNavigation = requires(
	T& value, std::wstring group, std::wstring state, std::wstring* error)
{
	{ value.GoToVisualState(group, state, false, error) }
		-> std::same_as<bool>;
	{ value.GetCurrentVisualState(group) } -> std::same_as<std::wstring>;
};

template<typename T>
concept HasTypedVisualStateNavigation = requires(
	T& value, VisualStateGroupToken group, VisualStateToken state,
	std::wstring* error)
{
	{ value.GoToVisualState(group, state, false, error) }
		-> std::same_as<bool>;
	{ value.GetCurrentVisualState(group) } -> std::same_as<VisualStateToken>;
};

template<typename T>
concept CompiledInteractionRecord =
	std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template<typename T>
concept HasOwnedStyleActionGraph = requires(T& value)
{
	value.EnterActions.emplace_back();
	value.ExitActions.emplace_back();
};

template<typename T>
concept HasStringTemplatePartLookup = requires(
	T& value, const std::wstring& name)
{
	value.FindDeclarativeTemplatePart(name);
};

template<typename T>
concept HasStringTemplatePartRegistration = requires(
	Control& owner, std::wstring name, Control* instance)
{
	{ T::RegisterTemplatePart(owner, std::move(name), instance) }
		-> std::same_as<bool>;
};

template<typename T>
concept HasStringComponentContentPresenterLookup = requires(
	T& value, const std::wstring& propertyName)
{
	{ value.FindDeclarativeContentPresenter(propertyName) }
		-> std::same_as<Control*>;
};

template<typename T>
concept HasStringComponentContentPresenterRegistration = requires(
	Control& owner, std::wstring propertyName, Control* instance)
{
	{ T::RegisterComponentContentPresenter(
		owner, std::move(propertyName), instance) }
		-> std::same_as<bool>;
};

template<typename T>
concept HasStringXamlContentPresenterRegistration = requires(
	Control& owner, std::wstring propertyName, Control* instance)
{
	{ T::RegisterContentPresenter(
		owner, std::move(propertyName), instance) }
		-> std::same_as<bool>;
};

struct CompiledComponentPropertyAccessProbe : Control
{
	using Control::FindCompiledComponentPropertyCore;
};

template<typename T>
concept HasStringCompiledComponentPropertyLookup = requires(
	const T& value, const std::wstring& name)
{
	{ value.FindCompiledComponentPropertyCore(name) }
		-> std::same_as<const DependencyPropertyMetadata*>;
};

template<typename T>
concept HasTokenCompiledComponentPropertyLookup = requires(
	const T& value, ComponentPropertyToken property)
{
	{ value.FindCompiledComponentPropertyCore(property) }
		-> std::same_as<const DependencyPropertyMetadata*>;
};

#define CUI_DECLARE_NAMED_PROPERTY_CONCEPT(conceptName, expression) \
	template<typename T> concept conceptName = requires(T& value, BindingValue& out) \
	{ expression; }

CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedFindDependencyProperty,
	value.FindDependencyProperty(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedFindPropertyMetadata,
	value.FindPropertyMetadata(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedTryGetPropertyValue,
	value.TryGetPropertyValue(std::wstring{}, out));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedTryGetPropertyValueAtSource,
	value.TryGetPropertyValue(std::wstring{},
		DependencyPropertyValueSource::Local, out));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedTrySetPropertyValue,
	value.TrySetPropertyValue(std::wstring{}, BindingValue{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedTrySetCurrentPropertyValue,
	value.TrySetCurrentPropertyValue(std::wstring{}, BindingValue{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedCoerceValue,
	value.CoerceValue(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedClearPropertyValue,
	value.ClearPropertyValue(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedHasPropertyValue,
	value.HasPropertyValue(std::wstring{},
		DependencyPropertyValueSource::Local));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedGetPropertyValueSource,
	value.GetPropertyValueSource(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedGetPropertyExpressionKind,
	value.GetPropertyExpressionKind(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedResetPropertyValue,
	value.ResetPropertyValue(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedIsPropertyValueDefault,
	value.IsPropertyValueDefault(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedSetDynamicResource,
	value.SetDynamicResource(std::wstring{}, std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedClearDynamicResource,
	value.ClearDynamicResource(std::wstring{}));
CUI_DECLARE_NAMED_PROPERTY_CONCEPT(HasNamedTryGetDynamicResourceKey,
	value.TryGetDynamicResourceKey(std::wstring{}, std::declval<std::wstring&>()));

#undef CUI_DECLARE_NAMED_PROPERTY_CONCEPT

#define CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(conceptName, expression) \
	template<typename T> concept conceptName = requires( \
		T& value, IBindingSource* source) \
	{ expression; }

CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionAdd,
	value.Add(std::wstring{}, source, std::wstring{}));
CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionAddMulti,
	value.AddMulti(std::wstring{}, std::vector<MultiBindingSource>{}));
CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionFind,
	value.Find(std::wstring{}));
CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionFindMulti,
	value.FindMulti(std::wstring{}));
CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionRemove,
	value.Remove(std::wstring{}));
CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionUpdateTarget,
	value.UpdateTarget(std::wstring{}));
CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT(HasNamedBindingCollectionUpdateSource,
	value.UpdateSource(std::wstring{}));

#undef CUI_DECLARE_NAMED_BINDING_COLLECTION_CONCEPT

#define CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT(conceptName, expression) \
	template<typename T> concept conceptName = requires( \
		Control& target, const BindingValue& bindingValue) \
	{ expression; }

CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT(HasNamedPropertyAccessSetValue,
	T::SetValue(target, std::wstring{}, bindingValue,
		DependencyPropertyValueSource::Style));
CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT(HasNamedPropertyAccessClearValue,
	T::ClearValue(target, std::wstring{},
		DependencyPropertyValueSource::Style));
CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT(HasNamedPropertyAccessSetBaseValue,
	T::SetBaseValue(target, std::wstring{}, bindingValue));
CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT(HasNamedPropertyAccessSetDynamicResource,
	T::SetDynamicResource(target, std::wstring{}, std::wstring{},
		DependencyPropertyValueSource::Style));
CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT(HasNamedPropertyAccessClearDynamicResource,
	T::ClearDynamicResource(target, std::wstring{},
		DependencyPropertyValueSource::Style));

#undef CUI_DECLARE_NAMED_PROPERTY_ACCESS_CONCEPT

template<typename T>
concept HasRuntimePropertyIdentitySurface = requires(
	T& value,
	const DependencyProperty& property,
	BindingValue& out)
{
	{ value.GetPropertyMetadata(property) } ->
		std::same_as<const DependencyPropertyMetadata*>;
	{ value.TryGetPropertyValue(property, out) } -> std::same_as<bool>;
	{ value.TrySetPropertyValue(property, BindingValue{}) } -> std::same_as<bool>;
	{ value.TrySetCurrentPropertyValue(property, BindingValue{}) } ->
		std::same_as<bool>;
	{ value.GetPropertyValueSource(property) } ->
		std::same_as<DependencyPropertyValueSource>;
	{ value.GetPropertyExpressionKind(property) } ->
		std::same_as<DependencyPropertyExpressionKind>;
	{ value.ResetPropertyValue(property) } -> std::same_as<bool>;
};

template<typename T>
concept HasCompatibilityBindingSourceNameSurface = requires(
	T& value,
	const std::wstring& path,
	BindingValue& out,
	BindingSourcePropertyMetadata& metadata)
{
	{ value.TryGetValue(path, out) } -> std::same_as<bool>;
	{ value.TrySetValue(path, out) } -> std::same_as<bool>;
	{ value.TryGetPropertyMetadata(path, metadata) } -> std::same_as<bool>;
};

template<typename T>
concept HasBindingSourceDiscoverySurface = requires(const T& value)
{
	{ value.GetProperties() }
		-> std::same_as<std::vector<BindingSourcePropertyMetadata>>;
};

template<typename T>
concept HasBindingSourceNameValidationSurface = requires(
	const T& value, const std::wstring& propertyName)
{
	{ value.GetValidationIssues(propertyName) }
		-> std::same_as<std::vector<BindingValidationIssue>>;
};

template<typename T>
concept HasBindingSourceTokenSurface = requires(
	T& value,
	const T& source,
	BindingSourcePropertyToken property,
	BindingValue& out,
	BindingSourcePropertyMetadata& metadata)
{
	{ source.TryGetValue(property, out) } -> std::same_as<bool>;
	{ value.TrySetValue(property, out) } -> std::same_as<bool>;
	{ source.TryGetPropertyMetadata(property, metadata) } -> std::same_as<bool>;
	{ source.GetValidationIssues(property) }
		-> std::same_as<std::vector<BindingValidationIssue>>;
};

template<typename T>
concept HasCompiledBindingSourceAdapterCollectionSurface = requires(
	T& bindings,
	const DependencyProperty& targetProperty,
	IBindingSource* source,
	BindingSourceReference ownedSource,
	CompiledBindingPathView sourcePath,
	std::vector<MultiBindingSource> multiSources)
{
	{ bindings.Add(targetProperty, source, sourcePath) }
		-> std::same_as<Binding*>;
	{ bindings.Add(targetProperty, std::move(ownedSource), sourcePath) }
		-> std::same_as<Binding*>;
	{ bindings.AddMulti(targetProperty, std::move(multiSources)) }
		-> std::same_as<MultiBinding*>;
};

template<typename T>
concept HasDirectCompiledBindingCollectionSurface = requires(
	T& bindings,
	const DependencyProperty& targetProperty,
	CompiledSourceHandle source)
{
	{ bindings.Add(targetProperty, source) } -> std::same_as<Binding*>;
};

template<typename T>
concept HasOwnedPropertyName = requires(T& value)
{
	value.PropertyName;
};

template<typename T>
concept HasOwnedName = requires(T& value)
{
	value.Name;
};

template<typename T>
concept HasPropertyIdentityNameProjection = requires(const T& value)
{
	{ value.Name() } -> std::same_as<const std::wstring&>;
};

template<typename T>
concept HasDesignMetadataOption = requires(T& value)
{
	value.Design;
};

template<typename T>
concept HasDesignMetadataSurface = requires(const T& value)
{
	value.Design();
};

template<typename T>
concept HasDesignerBrowsableSurface = requires(
	const T& value, DependencyObject& target)
{
	value.IsDesignerBrowsable(target);
};

#define CUI_DECLARE_STYLE_BUILDER_CONCEPT(conceptName, memberName) \
	template<typename T> concept conceptName = requires { &T::memberName; }

CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleAddRule, AddRule);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleRemoveRule, RemoveRule);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleClearRules, ClearRules);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleRulesView, Rules);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleSetResource, SetResource);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleRemoveResource, RemoveResource);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleClearResources, ClearResources);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleRevision, Revision);
CUI_DECLARE_STYLE_BUILDER_CONCEPT(HasStyleSubscribeChanged, SubscribeChanged);

#undef CUI_DECLARE_STYLE_BUILDER_CONCEPT

static_assert(!HasDynamicXamlDescriptor<Control>);
static_assert(!HasDynamicComponentQNameSurface<Control>);
static_assert(!HasDynamicComponentBehavior<Control>);
static_assert(!HasNamedFindDependencyProperty<Control>);
static_assert(!HasNamedFindPropertyMetadata<Control>);
static_assert(!HasNamedTryGetPropertyValue<Control>);
static_assert(!HasNamedTryGetPropertyValueAtSource<Control>);
static_assert(!HasNamedTrySetPropertyValue<Control>);
static_assert(!HasNamedTrySetCurrentPropertyValue<Control>);
static_assert(!HasNamedCoerceValue<Control>);
static_assert(!HasNamedClearPropertyValue<Control>);
static_assert(!HasNamedHasPropertyValue<Control>);
static_assert(!HasNamedGetPropertyValueSource<Control>);
static_assert(!HasNamedGetPropertyExpressionKind<Control>);
static_assert(!HasNamedResetPropertyValue<Control>);
static_assert(!HasNamedIsPropertyValueDefault<Control>);
static_assert(!HasNamedSetDynamicResource<Control>);
static_assert(!HasNamedClearDynamicResource<Control>);
static_assert(!HasNamedTryGetDynamicResourceKey<Control>);
static_assert(!HasNamedBindingCollectionAdd<BindingCollection>);
static_assert(!HasNamedBindingCollectionAddMulti<BindingCollection>);
static_assert(!HasNamedBindingCollectionFind<BindingCollection>);
static_assert(!HasNamedBindingCollectionFindMulti<BindingCollection>);
static_assert(!HasNamedBindingCollectionRemove<BindingCollection>);
static_assert(!HasNamedBindingCollectionUpdateTarget<BindingCollection>);
static_assert(!HasNamedBindingCollectionUpdateSource<BindingCollection>);
static_assert(!HasNamedPropertyAccessSetValue<
	cui::framework::DependencyPropertyAccess>);
static_assert(!HasNamedPropertyAccessClearValue<
	cui::framework::DependencyPropertyAccess>);
static_assert(!HasNamedPropertyAccessSetBaseValue<
	cui::framework::DependencyPropertyAccess>);
static_assert(!HasNamedPropertyAccessSetDynamicResource<
	cui::framework::DependencyPropertyAccess>);
static_assert(!HasNamedPropertyAccessClearDynamicResource<
	cui::framework::DependencyPropertyAccess>);
static_assert(!std::is_constructible_v<Binding,
	DependencyObject*, std::wstring, IBindingSource*, std::wstring>);
static_assert(!std::is_constructible_v<MultiBinding,
	DependencyObject*, std::wstring, std::vector<MultiBindingSource>>);
static_assert(!std::is_constructible_v<DependencyPropertyReference,
	std::wstring>);
static_assert(!std::is_constructible_v<DependencyPropertyReference,
	const wchar_t*>);
static_assert(!HasDeclarativeEventName<DeclarativeEventDefinition>);
static_assert(!HasDeclarativeEventName<DeclarativeEventArgs>);
static_assert(!HasDeclarativeEventOwnerType<DeclarativeEventArgs>);
static_assert(!HasNamedDeclarativeEventLookup<Control>);
static_assert(!HasStringDeclarativeEventRaise<Control>);
static_assert(HasTypedDeclarativeEventRaise<Control>);
static_assert(!HasStringTemplatePartLookup<Control>);
static_assert(!HasStringTemplatePartRegistration<
	cui::framework::TemplateAccess>);
static_assert(!HasStringTemplatePartRegistration<
	cui::framework::XamlAccess>);
static_assert(!HasStringComponentContentPresenterLookup<Control>);
static_assert(!HasStringComponentContentPresenterRegistration<
	cui::framework::TemplateAccess>);
static_assert(!HasStringXamlContentPresenterRegistration<
	cui::framework::XamlAccess>);
static_assert(!HasStringCompiledComponentPropertyLookup<
	CompiledComponentPropertyAccessProbe>);
static_assert(HasTokenCompiledComponentPropertyLookup<
	CompiledComponentPropertyAccessProbe>);
static_assert(sizeof(ComponentTypeToken) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<ComponentTypeToken>);
static_assert(std::is_standard_layout_v<ComponentTypeToken>);
static_assert(MakeComponentTypeToken({}, {}).Value == 0);
static_assert(MakeComponentTypeToken(
	L"urn:cui:runtime-boundary", L"Component").Value != 0);
static_assert(MakeComponentTypeToken(
	L"urn:cui:runtime-boundary", L"Component")
	!= MakeComponentTypeToken(
		L"urn:cui:runtime-boundary:other", L"Component"));
static_assert(MakeComponentTypeToken(
	L"urn:cui:runtime-boundary", L"Component")
	!= MakeComponentTypeToken(
		L"urn:cui:runtime-boundary", L"OtherComponent"));
static_assert(std::same_as<decltype(
	std::declval<const Control&>().GetDeclarativeTypeToken()),
	ComponentTypeToken>);

static_assert(HasRuntimePropertyIdentitySurface<Control>);
static_assert(HasBindingSourceTokenSurface<IBindingSource>,
	"Production binding sources must expose name-free property tokens");
static_assert(!HasCompatibilityBindingSourceNameSurface<IBindingSource>,
	"Production IBindingSource must not expose name-based virtual dispatch");
static_assert(!HasCompatibilityBindingSourceNameSurface<DependencyObject>,
	"Production DependencyObject must not restore name-based binding access");
static_assert(!HasBindingSourceDiscoverySurface<IBindingSource>,
	"Production IBindingSource must not expose dynamic property discovery");
static_assert(!HasBindingSourceDiscoverySurface<DependencyObject>,
	"Production DependencyObject must not expose dynamic property discovery");
static_assert(!HasBindingSourceNameValidationSurface<IBindingSource>,
	"Production IBindingSource validation must be token-addressed");
static_assert(sizeof(BindingSourcePropertyToken) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<BindingSourcePropertyToken>);
static_assert(MakeBindingSourcePropertyToken(L"runtime-boundary-source").Value
	!= 0);
static_assert(!HasOwnedPropertyName<PropertyChangedEventArgs>);
static_assert(!HasOwnedPropertyName<BindingValidationChangedEventArgs>);
static_assert(sizeof(PropertyChangedEventArgs)
	== sizeof(BindingSourcePropertyToken));
static_assert(sizeof(BindingValidationChangedEventArgs)
	== sizeof(BindingSourcePropertyToken));
static_assert(!HasOwnedName<BindingSourcePropertyMetadata>);
static_assert(std::is_trivially_copyable_v<BindingSourcePropertyMetadata>);
#if _ITERATOR_DEBUG_LEVEL == 0
// MSVC's iterator-debug storage changes the layout of the STL containers in
// DependencyObject. Keep the byte-for-byte ABI gate on production STL builds;
// the surface checks above still apply to every configuration.
static_assert(sizeof(void*) != 8 || sizeof(DependencyObject) == 200,
	"Production DependencyObject must not retain Design subscriber storage");
#endif
static_assert(sizeof(void*) != 8 || sizeof(DependencyProperty) == 128,
	"Production DependencyProperty must not retain authored-name, global-index "
	"or Design metadata-cache sidecars");
static_assert(sizeof(void*) != 8 || sizeof(DependencyPropertyMetadata) == 696,
	"Production DependencyPropertyMetadata must not retain authored-name or "
	"inheritance-key sidecars");
static_assert(!HasLegacyDependencyPropertyRegister<DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyRegisterReadOnly<
	DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyAddOwner<DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyOverride<DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyNameFind<DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyFindNative<DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyFindRegistered<
	DependencyPropertyRegistry>);
static_assert(!HasLegacyDependencyPropertyGetRegisteredProperties<
	DependencyPropertyRegistry>);
static_assert(sizeof(BindingSourcePropertyMetadata)
	<= sizeof(std::uint64_t) * 3,
	"Production source metadata must remain name-free and compact");

// The binary boundary script requires this exact marker from the compiled AOT
// probe. The paired static_asserts above make the encoded layout impossible to
// publish when either Production object regains a sidecar.
extern "C" std::uint64_t
CuiA8e4DependencyPropertySidecarLayout_128_696() noexcept
{
	return (static_cast<std::uint64_t>(sizeof(DependencyProperty)) << 32)
		| sizeof(DependencyPropertyMetadata);
}

// The primary generated Binding ABI is a compact endpoint handle plus one
// immutable operation table.  Every operation is a raw function pointer so a
// generated binding does not allocate a per-instance callable or enter the
// generic IBindingSource virtual/token dispatch lane.
static_assert(std::is_trivially_copyable_v<CompiledSourceOps>);
static_assert(std::is_standard_layout_v<CompiledSourceOps>);
static_assert(std::same_as<CompiledSourceOps::CapabilitiesCallback,
	CompiledBindingPathCapabilities(*)(
		const CompiledSourceHandle&)>);
static_assert(std::same_as<CompiledSourceOps::ValueKindCallback,
	BindingValueKind(*)(const CompiledSourceHandle&)>);
static_assert(std::same_as<CompiledSourceOps::LifetimeCallback,
	std::weak_ptr<const void>(*)(const CompiledSourceHandle&)>);
static_assert(std::same_as<CompiledSourceOps::ReadCallback,
	bool(*)(const CompiledSourceHandle&, BindingValue&)>);
static_assert(std::same_as<CompiledSourceOps::WriteCallback,
	bool(*)(const CompiledSourceHandle&, const BindingValue&)>);
static_assert(std::same_as<CompiledSourceOps::SubscribeCallback,
	EventConnection(*)(const CompiledSourceHandle&,
		DependencyPropertyChangeHandler)>);
static_assert(std::same_as<CompiledSourceOps::ValidationCallback,
	std::vector<BindingValidationIssue>(*)(const CompiledSourceHandle&)>);
static_assert(std::same_as<CompiledSourceOps::SubscribeValidationCallback,
	EventConnection(*)(const CompiledSourceHandle&,
		DependencyPropertyChangeHandler)>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.Capabilities),
	CompiledSourceOps::CapabilitiesCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.ValueKind),
	CompiledSourceOps::ValueKindCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.Lifetime),
	CompiledSourceOps::LifetimeCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.Read),
	CompiledSourceOps::ReadCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.Write),
	CompiledSourceOps::WriteCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.Subscribe),
	CompiledSourceOps::SubscribeCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.Validation),
	CompiledSourceOps::ValidationCallback>);
static_assert(std::same_as<decltype(CompiledSourceOps{}.SubscribeValidation),
	CompiledSourceOps::SubscribeValidationCallback>);
static_assert(CompiledSourceOps{}.Validation == nullptr);
static_assert(CompiledSourceOps{}.SubscribeValidation == nullptr);

static_assert(std::is_trivially_copyable_v<CompiledSourceHandle>);
static_assert(std::is_standard_layout_v<CompiledSourceHandle>);
static_assert(sizeof(CompiledSourceHandle) == sizeof(void*) * 3,
	"direct Binding source handles must remain exactly three pointers");
static_assert(alignof(CompiledSourceHandle) == alignof(void*));
static_assert(std::same_as<decltype(CompiledSourceHandle{}.Object), void*>);
static_assert(std::same_as<decltype(CompiledSourceHandle{}.Context),
	const void*>);
static_assert(std::same_as<decltype(CompiledSourceHandle{}.Ops),
	const CompiledSourceOps*>);
static_assert(!std::is_convertible_v<IBindingSource*, CompiledSourceHandle>,
	"the legacy adapter must not implicitly become a direct source handle");
static_assert(std::same_as<decltype(
	cui::binding::MakeCompiledDependencyPropertySource(
		std::declval<DependencyObject&>(),
		std::declval<const DependencyProperty&>())),
	CompiledSourceHandle>);
static_assert(noexcept(cui::binding::MakeCompiledDependencyPropertySource(
	std::declval<DependencyObject&>(),
	std::declval<const DependencyProperty&>())));
static_assert(std::same_as<decltype(
	cui::binding::ResolveCompiledDependencyPropertySource(
		std::declval<IBindingSource&>(),
		std::declval<const DependencyProperty&>())),
	CompiledSourceHandle>);
static_assert(noexcept(cui::binding::ResolveCompiledDependencyPropertySource(
	std::declval<IBindingSource&>(),
	std::declval<const DependencyProperty&>())));
static_assert(HasCompiledRecordIndexSource<CompiledBindingRecord>,
	"generated records must publish known properties by stable table index");
static_assert(!HasPublicCompiledRecordEntrySource<CompiledBindingRecord>,
	"a descriptor from another record shape must not form a public endpoint");
static_assert(std::same_as<decltype(
	std::declval<CompiledBindingRecord&>().MakeCompiledPropertySource(
		size_t{})), CompiledSourceHandle>);
static_assert(noexcept(
	std::declval<CompiledBindingRecord&>().MakeCompiledPropertySource(
		size_t{})));
static_assert(std::is_constructible_v<Binding,
	DependencyObject*, const DependencyProperty&, CompiledSourceHandle>);
static_assert(HasDirectCompiledBindingCollectionSurface<BindingCollection>);
static_assert(std::is_constructible_v<MultiBindingSource,
	CompiledSourceHandle>);

// CompiledBindingPath v2 embeds exact native endpoint resolvers while retaining
// tokens only for external sources whose concrete C++ contract is unknown.
static_assert(CompiledBindingPathVersion == 2);
static_assert(std::same_as<CompiledBindingPathEndpointResolver,
	CompiledSourceHandle(*)(IBindingSource&) noexcept>);
static_assert(std::is_trivially_copyable_v<CompiledBindingPathStep>);
static_assert(std::is_standard_layout_v<CompiledBindingPathStep>);
static_assert(sizeof(CompiledBindingPathStep) <= sizeof(std::uint64_t) * 4,
	"compiled binding steps must remain compact immutable records");
static_assert(std::same_as<
	decltype(CompiledBindingPathStep{}.EndpointResolver),
	CompiledBindingPathEndpointResolver>);
static_assert(CompiledBindingPathStep{}.EndpointResolver == nullptr);
static_assert(std::is_trivially_copyable_v<CompiledBindingPathView>);
static_assert(sizeof(CompiledBindingPathView)
	<= sizeof(std::span<const CompiledBindingPathStep>)
		+ sizeof(std::uint64_t),
	"compiled binding paths must remain a compact non-owning view");
static_assert(std::same_as<
	decltype(CompiledBindingPathView{}.Steps),
	std::span<const CompiledBindingPathStep>>);
static_assert(CompiledBindingPathView{}.Version == CompiledBindingPathVersion);
static_assert(HasCompiledBindingPathCapability(
	CompiledBindingPathCapabilities::Read
		| CompiledBindingPathCapabilities::Observe,
	CompiledBindingPathCapabilities::Observe));

static_assert(std::is_constructible_v<Binding,
	DependencyObject*, const DependencyProperty&, IBindingSource*,
	CompiledBindingPathView>);
static_assert(std::is_constructible_v<Binding,
	DependencyObject*, const DependencyProperty&, BindingSourceReference,
	CompiledBindingPathView>);
static_assert(
	HasCompiledBindingSourceAdapterCollectionSurface<BindingCollection>);
static_assert(std::is_constructible_v<MultiBindingSource,
	IBindingSource*, CompiledBindingPathView>);
static_assert(std::is_constructible_v<MultiBindingSource,
	BindingSourceReference, CompiledBindingPathView>);
static_assert(!HasDesignMetadataOption<
	DependencyPropertyOptions<Control, bool>>);
static_assert(!HasDesignMetadataSurface<DependencyPropertyMetadata>);
static_assert(!HasDesignerBrowsableSurface<DependencyPropertyMetadata>);
static_assert(!HasOwnedPropertyName<DependencyPropertyChangedEventArgs>);
static_assert(HasPropertyIdentityNameProjection<
	DependencyPropertyChangedEventArgs>);
static_assert(std::is_constructible_v<DependencyPropertyChangedEventArgs,
	const DependencyProperty&, const BindingValue&, const BindingValue&>);
static_assert(sizeof(DependencyPropertyChangedEventArgs)
	<= sizeof(BindingValue) * 2 + sizeof(const DependencyProperty*) * 2,
	"property change events must not own a property-name string");
static_assert(sizeof(DependencyPropertyReference) ==
	sizeof(const DependencyProperty*));
static_assert(std::is_trivially_copyable_v<DependencyPropertyReference>);
static_assert(sizeof(MouseWheelEvent) == sizeof(UIElement*),
	"typed routed-event facades must contain only their owner pointer");
static_assert(sizeof(RoutedEvent<RoutedEventArgs>)
	<= sizeof(UIElement*) + sizeof(RoutedEventId) + sizeof(void*) - 1,
	"dynamic routed-event facades must not own handler state");
static_assert(std::is_constructible_v<DependencyPropertyReference,
	const DependencyProperty&>);
static_assert(!std::is_constructible_v<Binding,
	DependencyObject*, const DependencyProperty&, IBindingSource*, std::wstring>);
// Production bindings require an immutable compiled source contract.
static_assert(std::is_constructible_v<MultiBinding,
	DependencyObject*, const DependencyProperty&,
	std::vector<MultiBindingSource>>);
static_assert(!std::is_default_constructible_v<ControlStyleSheet>);
static_assert(!HasStyleAddRule<ControlStyleSheet>);
static_assert(!HasStyleRemoveRule<ControlStyleSheet>);
static_assert(!HasStyleClearRules<ControlStyleSheet>);
static_assert(!HasStyleRulesView<ControlStyleSheet>);
static_assert(!HasStyleSetResource<ControlStyleSheet>);
static_assert(!HasStyleRemoveResource<ControlStyleSheet>);
static_assert(!HasStyleClearResources<ControlStyleSheet>);
static_assert(!HasStyleRevision<ControlStyleSheet>);
static_assert(!HasStyleSubscribeChanged<ControlStyleSheet>);
static_assert(CompiledStyleProgramViewVersion == 5);
static_assert(std::is_same_v<
	decltype(CompiledStyleProgramView{}.DataPaths),
	std::span<const CompiledBindingPathView>>);
static_assert(std::is_trivially_copyable_v<CompiledStyleValuePoolView>);
static_assert(sizeof(CompiledStyleValuePoolView)
	<= sizeof(void*) + sizeof(std::uint32_t) * 2,
	"typed Style pool descriptors must remain a compact read-only view");
static_assert(IsCompiledStyleStaticValueReference(
	MakeCompiledStyleStaticValueReference(3u, 17u)));
static_assert(CompiledStyleStaticValuePoolIndex(
	MakeCompiledStyleStaticValueReference(3u, 17u)) == 3u);
static_assert(CompiledStyleStaticValueElementIndex(
	MakeCompiledStyleStaticValueReference(3u, 17u)) == 17u);
static_assert(sizeof(ControlStyleSheet)
	<= sizeof(std::unique_ptr<void>)
		+ sizeof(std::vector<std::wstring>),
	"production style sheets must not carry the mutable backend state");
static_assert(CompiledInteractionRecord<CompiledStyleRuleOp>);
static_assert(CompiledInteractionRecord<CompiledStyleGroupOp>);
static_assert(CompiledInteractionRecord<CompiledStyleResourceOp>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.PropertyOperands),
	std::span<const CompiledInteractionPropertyOperand>>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.ObjectPathChildIndices),
	std::span<const std::uint32_t>>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.ObjectPaths),
	std::span<const CompiledStoryboardObjectPathOp>>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.KeyFrames),
	std::span<const CompiledInteractionKeyFrameOp>>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.Animations),
	std::span<const CompiledInteractionAnimationOp>>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.Storyboards),
	std::span<const CompiledInteractionStoryboardOp>>);
static_assert(std::same_as<
	decltype(CompiledStyleProgramView{}.Actions),
	std::span<const CompiledInteractionActionOp>>);
static_assert(!HasOwnedStyleActionGraph<ResolvedControlStyleTrigger>);
static_assert(std::same_as<
	decltype(ResolvedControlStyleTrigger{}.CompiledProgram),
	const CompiledStyleProgramView*>);
static_assert(std::same_as<
	decltype(ResolvedControlStyleTrigger{}.CompiledValues),
	std::span<const BindingValue>>);
static_assert(std::same_as<
	decltype(ResolvedControlStyleTrigger{}.CompiledEnterActions),
	CompiledStyleRange>);
static_assert(std::same_as<
	decltype(ResolvedControlStyleTrigger{}.CompiledExitActions),
	CompiledStyleRange>);
using ProductionCompiledStyleFactory =
	std::shared_ptr<const ControlStyleSheet> (*)(
		CompiledStyleProgramView, std::vector<BindingValue>);
static_assert(std::same_as<decltype(static_cast<ProductionCompiledStyleFactory>(
	&ControlStyleSheet::CreateCompiled)), ProductionCompiledStyleFactory>);
static_assert(sizeof(TemplatePartToken) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<TemplatePartToken>);
static_assert(MakeTemplatePartToken(std::wstring_view{}).Value == 0);
static_assert(MakeTemplatePartToken(L"runtime-boundary-part").Value != 0);
static_assert(sizeof(ComponentPropertyToken) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<ComponentPropertyToken>);
static_assert(std::is_standard_layout_v<ComponentPropertyToken>);
static_assert(MakeComponentPropertyToken(std::wstring_view{}).Value == 0);
static_assert(MakeComponentPropertyToken(
	L"runtime-boundary-component-property").Value != 0);
static_assert(std::same_as<decltype(
	cui::framework::TemplateAccess::RegisterTemplatePart(
		std::declval<Control&>(), TemplatePartToken{},
		static_cast<Control*>(nullptr))), bool>);
static_assert(std::is_trivially_copyable_v<DeclarativeEventDefinition>);
static_assert(sizeof(DeclarativeEventDefinition)
	<= sizeof(std::uint64_t),
	"compiled event definitions must not own a name string");
static_assert(std::same_as<
	decltype(std::declval<DeclarativeEventArgs&>().Definition),
	const DeclarativeEventDefinition*>);
static_assert(!HasLegacyInteractionInstall<cui::framework::TemplateAccess>);
static_assert(!HasLegacyInteractionInstall<cui::framework::XamlAccess>);
static_assert(!HasStringVisualStateNavigation<Control>);
static_assert(HasTypedVisualStateNavigation<Control>);
static_assert(CompiledInteractionProgramViewVersion == 2);
static_assert(CompiledInteractionInvalidIndex == UINT32_MAX);
static_assert(sizeof(VisualStateGroupToken) == sizeof(std::uint64_t));
static_assert(sizeof(VisualStateToken) == sizeof(std::uint64_t));
static_assert(CompiledInteractionRecord<VisualStateGroupToken>);
static_assert(CompiledInteractionRecord<VisualStateToken>);
static_assert(std::same_as<
	decltype(VisualStateGroupToken{}.Value), std::uint64_t>);
static_assert(std::same_as<
	decltype(VisualStateToken{}.Value), std::uint64_t>);
static_assert(MakeVisualStateGroupToken(L"runtime-boundary-group").Value != 0);
static_assert(MakeVisualStateToken(L"runtime-boundary-state").Value != 0);
static_assert(CompiledInteractionRecord<CompiledInteractionRange>);
static_assert(sizeof(CompiledInteractionRange) == sizeof(std::uint32_t) * 2);
static_assert(std::same_as<
	decltype(CompiledInteractionRange{}.Offset), std::uint32_t>);
static_assert(std::same_as<
	decltype(CompiledInteractionRange{}.Count), std::uint32_t>);
static_assert(CompiledInteractionRecord<CompiledStoryboardObjectPathOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionPropertyOperand>);
static_assert(CompiledInteractionRecord<CompiledInteractionConditionOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionSetterOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionKeyFrameOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionAnimationOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionStateOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionTransitionOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionGroupOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionStoryboardOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionActionOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionEventTriggerOp>);
static_assert(CompiledInteractionRecord<CompiledInteractionProgramView>);
static_assert(std::same_as<
	decltype(CompiledInteractionProgramView{}.Version), std::uint32_t>);
static_assert(std::same_as<
	decltype(CompiledInteractionProgramView{}.TargetCount), std::uint32_t>);
static_assert(!HasEmbeddedCompiledInteractionValues<
	CompiledInteractionProgramView>);
static_assert(std::same_as<
	decltype(CompiledInteractionPropertyOperand{}.TargetSlot), std::uint32_t>);
static_assert(std::same_as<
	decltype(CompiledInteractionTransitionOp{}.FromStateIndex), std::uint32_t>);
static_assert(std::same_as<
	decltype(CompiledInteractionTransitionOp{}.ToStateIndex), std::uint32_t>);
static_assert(std::same_as<
	decltype(CompiledInteractionActionOp{}.StoryboardIndex), std::uint32_t>);
static_assert(HasCompiledInteractionInstall<cui::framework::TemplateAccess>);
