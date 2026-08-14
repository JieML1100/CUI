#include "XamlSchema.h"
#include "Control.h"

#include <algorithm>
#include <cwctype>
#include <typeindex>
#include <unordered_set>

#if !CUI_ENABLE_DYNAMIC_XAML
#error XamlSchema.Design.cpp requires the Design runtime flavor.
#endif

namespace cui::details
{
	/** Design-runtime-only access to the standalone dependency-property factory. */
	class DependencyPropertyStandaloneAccess final
	{
	public:
		template<typename... TArgs>
		static std::unique_ptr<DependencyPropertyMetadata> CreateMetadata(
			TArgs&&... args)
		{
			return std::unique_ptr<DependencyPropertyMetadata>(
				new DependencyPropertyMetadata(
					std::forward<TArgs>(args)...));
		}

		static std::unique_ptr<DependencyProperty> CreateProperty(
			DependencyPropertyMetadata& metadata)
		{
			return DependencyPropertyRegistry::CreateStandalone(metadata);
		}
	};
}

namespace
{
	std::wstring MemberKey(const std::wstring& value)
	{
		return value;
	}

	bool IsXamlIdentifier(const std::wstring& value)
	{
		if (value.empty()
			|| !(std::iswalpha(value.front()) || value.front() == L'_'))
			return false;
		return std::all_of(value.begin() + 1, value.end(), [](wchar_t ch)
		{
			return std::iswalnum(ch) || ch == L'_';
		});
	}

	std::type_index DeclarativePropertyValueType(
		BindingValueKind kind,
		const BindingValue& defaultValue)
	{
		switch (kind)
		{
		case BindingValueKind::Bool: return std::type_index(typeid(bool));
		case BindingValueKind::NullableBool:
			return std::type_index(typeid(NullableBool));
		case BindingValueKind::Int: return std::type_index(typeid(int));
		case BindingValueKind::Int64: return std::type_index(typeid(long long));
		case BindingValueKind::Float: return std::type_index(typeid(float));
		case BindingValueKind::Double: return std::type_index(typeid(double));
		case BindingValueKind::String: return std::type_index(typeid(std::wstring));
		case BindingValueKind::Object:
			return std::type_index(defaultValue.Type());
		default: return std::type_index(typeid(void));
		}
	}

	bool DeclarativePropertyValuesEqual(
		const BindingValue& left,
		const BindingValue& right)
	{
		if (left.Kind() != BindingValueKind::Object
			|| right.Kind() != BindingValueKind::Object)
			return BindingValuesEqual(left, right);
		if (std::type_index(left.Type()) != std::type_index(right.Type()))
			return false;

		if (left.Type() == typeid(D2D1_COLOR_F))
		{
			D2D1_COLOR_F a{}, b{};
			return left.TryGet(a) && right.TryGet(b)
				&& a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
		}
		if (left.Type() == typeid(Thickness))
		{
			Thickness a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::core::Size))
		{
			cui::core::Size a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(D2D1_MATRIX_3X2_F))
		{
			D2D1_MATRIX_3X2_F a{}, b{};
			return left.TryGet(a) && right.TryGet(b)
				&& a._11 == b._11 && a._12 == b._12
				&& a._21 == b._21 && a._22 == b._22
				&& a._31 == b._31 && a._32 == b._32;
		}
		if (left.Type() == typeid(cui::core::Point))
		{
			cui::core::Point a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::core::Vector))
		{
			cui::core::Vector a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::core::Rect))
		{
			cui::core::Rect a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::layout::Length))
		{
			cui::layout::Length a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::drawing::Transform))
		{
			cui::drawing::Transform a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::drawing::Geometry))
		{
			cui::drawing::Geometry a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::drawing::Brush))
		{
			cui::drawing::Brush a, b;
			if (!left.TryGet(a) || !right.TryGet(b)) return false;
			return a.Kind == b.Kind
				&& a.MappingMode == b.MappingMode
				&& a.Color.r == b.Color.r && a.Color.g == b.Color.g
				&& a.Color.b == b.Color.b && a.Color.a == b.Color.a
				&& a.Opacity == b.Opacity
				&& a.StartPoint.x == b.StartPoint.x
				&& a.StartPoint.y == b.StartPoint.y
				&& a.EndPoint.x == b.EndPoint.x
				&& a.EndPoint.y == b.EndPoint.y
				&& a.Center.x == b.Center.x && a.Center.y == b.Center.y
				&& a.GradientOrigin.x == b.GradientOrigin.x
				&& a.GradientOrigin.y == b.GradientOrigin.y
				&& a.RadiusX == b.RadiusX && a.RadiusY == b.RadiusY
				&& a.GradientStops == b.GradientStops
				&& a.Transform == b.Transform
				&& a.RelativeTransform == b.RelativeTransform
				&& a.ImageSource == b.ImageSource
				&& a.Stretch == b.Stretch
				&& a.AlignmentX == b.AlignmentX
				&& a.AlignmentY == b.AlignmentY;
		}
		return false;
	}
}

std::shared_ptr<const DeclarativeTypeDescriptor>
DeclarativeTypeDescriptor::Create(
	RuntimeTypeId type,
	std::vector<DeclarativePropertyDefinition> properties,
	std::vector<DeclarativeEventDefinition> events,
	std::vector<DeclarativeContentPropertyDefinition> contentProperties,
	std::wstring* outError)
{
	if (!type.Valid() || !IsXamlIdentifier(type.LocalName))
	{
		if (outError) *outError =
			L"声明类型必须包含命名空间 URI 和有效的 XAML 类型名。";
		return {};
	}
	auto descriptor = std::shared_ptr<DeclarativeTypeDescriptor>(
		new DeclarativeTypeDescriptor(std::move(type)));
	if (!descriptor->Build(
		std::move(properties), std::move(events),
		std::move(contentProperties), outError)) return {};
	if (outError) outError->clear();
	return descriptor;
}

bool DeclarativeTypeDescriptor::Build(
	std::vector<DeclarativePropertyDefinition> properties,
	std::vector<DeclarativeEventDefinition> events,
	std::vector<DeclarativeContentPropertyDefinition> contentProperties,
	std::wstring* outError)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	std::unordered_set<std::wstring> memberNames;
	memberNames.reserve(properties.size() + events.size() + contentProperties.size());
	_properties.reserve(properties.size());
	_propertyMetadata.reserve(properties.size());
	_propertyIndex.reserve(properties.size());
	_propertyTokenIndex.reserve(properties.size());

	for (auto& definition : properties)
	{
		if (!IsXamlIdentifier(definition.Name))
			return fail(L"声明属性名称必须是有效的 XAML 标识符。");
		const auto key = MemberKey(definition.Name);
		if (!memberNames.insert(key).second)
			return fail(L"声明类型成员名称重复：" + definition.Name);
		if (definition.ValueKind == BindingValueKind::Empty)
			return fail(L"声明属性必须提供非空值类型：" + definition.Name);
		if (definition.ValueKind == BindingValueKind::Object
			&& (definition.DefaultValue.Kind() != BindingValueKind::Object
				|| definition.DefaultValue.Type() == typeid(void)))
			return fail(L"对象声明属性必须提供具体类型的默认值："
				+ definition.Name);
		if (HasDependencyPropertyFlag(
			definition.Flags, DependencyPropertyFlags::Inherits)
			&& definition.InheritanceKey.empty())
			return fail(L"可继承声明属性必须提供稳定的 InheritanceKey："
				+ definition.Name);
		if (definition.DefaultUpdateMode == DataSourceUpdateMode::Default)
			return fail(L"声明属性的默认更新触发器必须是具体值："
				+ definition.Name);
		if (definition.IsReadOnly && HasDependencyPropertyFlag(
			definition.Flags, DependencyPropertyFlags::BindsTwoWayByDefault))
			return fail(L"只读声明属性不能指定 BindsTwoWayByDefault："
				+ definition.Name);
		if (definition.IsReadOnly && definition.DefaultUpdateMode
			!= DataSourceUpdateMode::OnPropertyChanged)
			return fail(L"只读声明属性不能指定源更新触发器："
				+ definition.Name);

		BindingValue normalizedDefault;
		if (definition.ValueKind == BindingValueKind::Object)
			normalizedDefault = definition.DefaultValue;
		else if (!TryConvertBindingValue(
			definition.DefaultValue, definition.ValueKind, normalizedDefault))
			return fail(L"声明属性默认值无法转换为声明类型："
				+ definition.Name);

		std::vector<BindingValue> normalizedAllowedValues;
		normalizedAllowedValues.reserve(definition.AllowedValues.size());
		for (const auto& candidate : definition.AllowedValues)
		{
			BindingValue normalized;
			const bool converted = definition.ValueKind == BindingValueKind::Object
				? TryConvertBindingValue(candidate, normalizedDefault, normalized)
				: TryConvertBindingValue(
					candidate, definition.ValueKind, normalized);
			if (!converted)
				return fail(L"声明属性候选值无法转换为声明类型："
					+ definition.Name);
			if (std::any_of(
				normalizedAllowedValues.begin(), normalizedAllowedValues.end(),
				[&](const auto& existing)
				{
					return DeclarativePropertyValuesEqual(existing, normalized);
				}))
				return fail(L"声明属性候选值重复：" + definition.Name);
			normalizedAllowedValues.push_back(std::move(normalized));
		}
		if (!normalizedAllowedValues.empty()
			&& std::none_of(
				normalizedAllowedValues.begin(), normalizedAllowedValues.end(),
				[&](const auto& candidate)
				{
					return DeclarativePropertyValuesEqual(
						candidate, normalizedDefault);
				}))
			return fail(L"声明属性默认值不在允许的候选集合中："
				+ definition.Name);
		if (definition.Design.Persistence
			== DependencyPropertyPersistence::Automatic)
			definition.Design.Persistence = DependencyPropertyPersistence::Metadata;

		const auto slot = _properties.size();
		const auto canonicalName = definition.Name;
		const auto propertyToken =
			MakeBindingSourcePropertyToken(canonicalName);
		const auto [tokenEntry, tokenInserted] =
			_propertyTokenIndex.emplace(propertyToken.Value, slot);
		if (!tokenInserted)
		{
			const auto existingSlot = tokenEntry->second;
			const auto* existing = existingSlot < _properties.size()
				? _properties[existingSlot].Metadata.get() : nullptr;
			if (!existing || existing->Name() != canonicalName)
				return fail(L"声明属性 Binding token 发生名称冲突："
					+ canonicalName);
			return fail(L"声明属性重复：" + canonicalName);
		}
		auto schemaAllowedValues = normalizedAllowedValues;
		auto metadata =
			cui::details::DependencyPropertyStandaloneAccess::CreateMetadata(
				canonicalName,
				definition.ValueKind,
				DeclarativePropertyValueType(
					definition.ValueKind, normalizedDefault),
				std::type_index(typeid(Control)),
				[owner = this](const DependencyObject& object)
				{
					const auto* target = dynamic_cast<const Control*>(&object);
					return target
						&& target->GetDeclarativeTypeDescriptor().get() == owner;
				},
				[kind = definition.ValueKind, objectDefault = normalizedDefault](
					const BindingValue& value, BindingValue& converted)
				{
					if (kind == BindingValueKind::Object)
						return TryConvertBindingValue(
							value, objectDefault, converted);
					return TryConvertBindingValue(value, kind, converted);
				},
				[allowedValues = std::move(normalizedAllowedValues)](
					const BindingValue& value)
				{
					if (allowedValues.empty()) return true;
					return std::any_of(
						allowedValues.begin(), allowedValues.end(),
						[&](const auto& candidate)
						{
							return DeclarativePropertyValuesEqual(
								candidate, value);
						});
				},
				nullptr,
				[](const BindingValue& left, const BindingValue& right)
				{
					return DeclarativePropertyValuesEqual(left, right);
				},
				[owner = this, slot](DependencyObject& object, BindingValue& value)
				{
					auto* target = dynamic_cast<Control*>(&object);
					return target && target->TryGetDeclarativePropertyBacking(
						*owner, slot, value);
				},
				[owner = this, slot](
					DependencyObject& object, const BindingValue& value)
				{
					auto* target = dynamic_cast<Control*>(&object);
					return target && target->TrySetDeclarativePropertyBacking(
						*owner, slot, value);
				},
				[canonicalName](
					DependencyObject& object,
					DependencyPropertyMetadata::ChangeHandler handler,
					DataSourceUpdateMode updateMode)
				{
					auto* target = dynamic_cast<Control*>(&object);
					if (!target) return EventConnection{};
					if (updateMode == DataSourceUpdateMode::OnValidation)
						return target->OnLostFocus.Subscribe(
							[handler = std::move(handler)](Control*)
							{
								handler();
							});
					return target->OnPropertyValueChanged.Subscribe(
						[canonicalName, handler = std::move(handler)](
							DependencyObject*, const DependencyPropertyChangedEventArgs& args)
						{
							if (args.Name() == canonicalName)
								handler();
						});
				},
				nullptr,
				normalizedDefault,
				true,
				false,
				definition.Flags,
				definition.IsReadOnly,
				definition.DefaultUpdateMode,
				std::move(definition.InheritanceKey),
				std::move(definition.Design));

		PropertyEntry entry;
		entry.DefaultValue = std::move(normalizedDefault);
		entry.AllowedValues = std::move(schemaAllowedValues);
		entry.Property = cui::details::DependencyPropertyStandaloneAccess::
			CreateProperty(*metadata);
		if (!entry.Property)
			return fail(L"声明属性无法创建稳定的 DependencyProperty 身份："
				+ canonicalName);
		entry.Metadata = std::move(metadata);
		_propertyIndex.emplace(key, slot);
		_propertyMetadata.push_back(entry.Metadata.get());
		_properties.push_back(std::move(entry));
		_hasInheritedProperties = _hasInheritedProperties
			|| HasDependencyPropertyFlag(
				definition.Flags, DependencyPropertyFlags::Inherits);
	}

	_events.reserve(events.size());
	_eventIndex.reserve(events.size());
	for (auto& definition : events)
	{
		if (!IsXamlIdentifier(definition.Name))
			return fail(L"声明事件名称必须是有效的 XAML 标识符。");
		if (definition.PayloadKind == BindingValueKind::Object)
			return fail(L"声明事件不支持无具体类型的 Object payload："
				+ definition.Name);
		const auto key = MemberKey(definition.Name);
		if (!memberNames.insert(key).second)
			return fail(L"声明类型成员名称重复：" + definition.Name);
		_eventIndex.emplace(key, _events.size());
		_events.push_back(std::move(definition));
	}

	_contentProperties.reserve(contentProperties.size());
	_contentPropertyIndex.reserve(contentProperties.size());
	for (auto& definition : contentProperties)
	{
		if (!IsXamlIdentifier(definition.Name))
			return fail(L"声明内容属性名称必须是有效的 XAML 标识符。");
		const auto key = MemberKey(definition.Name);
		if (!memberNames.insert(key).second)
			return fail(L"声明类型成员名称重复：" + definition.Name);
		if (definition.IsDefault
			&& _defaultContentProperty != static_cast<std::size_t>(-1))
			return fail(L"声明类型只能拥有一个默认内容属性。");
		if (definition.DisplayName.empty())
			definition.DisplayName = definition.Name;
		const auto index = _contentProperties.size();
		if (definition.IsDefault) _defaultContentProperty = index;
		_contentPropertyIndex.emplace(key, index);
		_contentProperties.push_back(std::move(definition));
	}
	return true;
}

bool DeclarativeTypeDescriptor::IsEquivalentTo(
	const DeclarativeTypeDescriptor& other) const
{
	if (_type != other._type
		|| _properties.size() != other._properties.size()
		|| _events.size() != other._events.size()
		|| _contentProperties.size() != other._contentProperties.size())
		return false;
	for (std::size_t index = 0; index < _properties.size(); ++index)
	{
		const auto& leftEntry = _properties[index];
		const auto* left = leftEntry.Metadata.get();
		if (!left) return false;
		const auto found = other._propertyIndex.find(MemberKey(left->Name()));
		if (found == other._propertyIndex.end()) return false;
		const auto& rightEntry = other._properties[found->second];
		const auto* right = rightEntry.Metadata.get();
		if (!right || left->Name() != right->Name()
			|| left->ValueKind() != right->ValueKind()
			|| left->ValueType() != right->ValueType()
			|| left->Flags() != right->Flags()
			|| left->IsReadOnly() != right->IsReadOnly()
			|| left->DefaultUpdateMode() != right->DefaultUpdateMode()
			|| left->InheritanceKey() != right->InheritanceKey()
			|| !DeclarativePropertyValuesEqual(
				leftEntry.DefaultValue, rightEntry.DefaultValue)
			|| leftEntry.AllowedValues.size()
				!= rightEntry.AllowedValues.size())
			return false;
		for (std::size_t choice = 0;
			choice < leftEntry.AllowedValues.size(); ++choice)
			if (!DeclarativePropertyValuesEqual(
				leftEntry.AllowedValues[choice],
				rightEntry.AllowedValues[choice])) return false;

		const auto& leftDesign = left->Design();
		const auto& rightDesign = right->Design();
		if (leftDesign.Browsable != rightDesign.Browsable
			|| leftDesign.DisplayName != rightDesign.DisplayName
			|| leftDesign.Category != rightDesign.Category
			|| leftDesign.CategoryOrder != rightDesign.CategoryOrder
			|| leftDesign.Order != rightDesign.Order
			|| leftDesign.Editor != rightDesign.Editor
			|| leftDesign.Minimum != rightDesign.Minimum
			|| leftDesign.Maximum != rightDesign.Maximum
			|| leftDesign.Step != rightDesign.Step
			|| leftDesign.Persistence != rightDesign.Persistence
			|| static_cast<bool>(leftDesign.BrowsableWhen)
				!= static_cast<bool>(rightDesign.BrowsableWhen)
			|| leftDesign.Choices.size() != rightDesign.Choices.size())
			return false;
		for (std::size_t choice = 0;
			choice < leftDesign.Choices.size(); ++choice)
			if (leftDesign.Choices[choice].DisplayName
					!= rightDesign.Choices[choice].DisplayName
				|| !DeclarativePropertyValuesEqual(
					leftDesign.Choices[choice].Value,
					rightDesign.Choices[choice].Value)) return false;
	}
	for (const auto& left : _events)
	{
		const auto* right = other.FindEvent(left.Name);
		if (!right || left.Name != right->Name
			|| left.PayloadKind != right->PayloadKind
			|| left.RoutingStrategy != right->RoutingStrategy)
			return false;
	}
	for (const auto& left : _contentProperties)
	{
		const auto* right = other.FindContentProperty(left.Name);
		if (!right || left.Name != right->Name
			|| left.DisplayName != right->DisplayName
			|| left.Cardinality != right->Cardinality
			|| left.IsDefault != right->IsDefault)
			return false;
	}
	return true;
}

const DependencyPropertyMetadata* DeclarativeTypeDescriptor::FindProperty(
	const std::wstring& propertyName) const noexcept
{
	const auto found = _propertyIndex.find(MemberKey(propertyName));
	return found == _propertyIndex.end()
		? nullptr : _properties[found->second].Metadata.get();
}

const DependencyPropertyMetadata* DeclarativeTypeDescriptor::FindProperty(
	BindingSourcePropertyToken property) const noexcept
{
	if (!property) return nullptr;
	const auto found = _propertyTokenIndex.find(property.Value);
	return found == _propertyTokenIndex.end()
		? nullptr : _properties[found->second].Metadata.get();
}

bool DeclarativeTypeDescriptor::TryGetPropertyDefault(
	std::size_t slot,
	BindingValue& value) const
{
	if (slot >= _properties.size()) return false;
	value = _properties[slot].DefaultValue;
	return true;
}

const DeclarativeEventDefinition* DeclarativeTypeDescriptor::FindEvent(
	const std::wstring& eventName) const noexcept
{
	const auto found = _eventIndex.find(MemberKey(eventName));
	return found == _eventIndex.end() ? nullptr : &_events[found->second];
}

const DeclarativeContentPropertyDefinition*
DeclarativeTypeDescriptor::FindContentProperty(
	const std::wstring& propertyName) const noexcept
{
	const auto found = _contentPropertyIndex.find(MemberKey(propertyName));
	return found == _contentPropertyIndex.end()
		? nullptr : &_contentProperties[found->second];
}

const DeclarativeContentPropertyDefinition*
DeclarativeTypeDescriptor::DefaultContentProperty() const noexcept
{
	return _defaultContentProperty == static_cast<std::size_t>(-1)
		? nullptr : &_contentProperties[_defaultContentProperty];
}

std::shared_ptr<const DeclarativeTypeDescriptor> XamlSchemaContext::Find(
	const RuntimeTypeId& type) const
{
	std::scoped_lock lock(_mutex);
	const auto found = _types.find(type);
	return found == _types.end() ? nullptr : found->second;
}

std::shared_ptr<const DeclarativeTypeDescriptor> XamlSchemaContext::GetOrAdd(
	std::shared_ptr<const DeclarativeTypeDescriptor> descriptor,
	std::wstring* outError)
{
	if (!descriptor)
	{
		if (outError) *outError = L"不能把空类型描述符加入 XAML Schema 上下文。";
		return {};
	}
	std::scoped_lock lock(_mutex);
	const auto found = _types.find(descriptor->TypeId());
	if (found != _types.end())
	{
		if (!found->second->IsEquivalentTo(*descriptor))
		{
			if (outError) *outError = L"同一 RuntimeTypeId 出现了不一致的 XAML Schema："
				+ descriptor->TypeId().RegistryKey();
			return {};
		}
		if (outError) outError->clear();
		return found->second;
	}
	_types.emplace(descriptor->TypeId(), descriptor);
	if (outError) outError->clear();
	return descriptor;
}
