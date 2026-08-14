#include "Control.h"
#include "Window.h"

#include <algorithm>

#if !CUI_ENABLE_DYNAMIC_XAML
#error Control.Design.cpp requires the Design runtime flavor.
#endif

bool IDeclarativeComponentBehavior::SetReadOnlyProperty(
	Control& host,
	const std::wstring& propertyName,
	const BindingValue& value)
{
	if (host.GetDeclarativeComponentBehavior() != this) return false;
	return host.TrySetReadOnlyPropertyValue(propertyName, value);
}

bool IDeclarativeComponentBehavior::ClearReadOnlyProperty(
	Control& host,
	const std::wstring& propertyName)
{
	if (host.GetDeclarativeComponentBehavior() != this) return false;
	return host.ClearReadOnlyPropertyValue(propertyName);
}

bool Control::SetDeclarativeComponentBehavior(
	std::unique_ptr<DeclarativeComponentBehavior> behavior,
	const DeclarativeComponentBehaviorContext& context,
	std::wstring* outError)
{
	if (&context.Host != this)
	{
		if (outError) *outError = L"组件 Behavior 上下文与宿主不匹配。";
		return false;
	}
	ClearDeclarativeComponentBehavior();
	if (!behavior)
	{
		if (outError) outError->clear();
		return true;
	}

	_declarativeComponentBehavior = std::move(behavior);
	bool attached = false;
	try
	{
		attached = _declarativeComponentBehavior->Attach(
			*this, context, outError);
	}
	catch (...)
	{
		if (outError)
			*outError = L"组件 Behavior Attach 抛出异常。";
	}
	if (!attached)
	{
		auto failed = std::move(_declarativeComponentBehavior);
		try { failed->Detach(*this); }
		catch (...) {}
		if (outError && outError->empty())
			*outError = L"组件 Behavior 拒绝附加。";
		return false;
	}
	try
	{
		_declarativeComponentBehavior->DpiChanged(
			*this, GetPresentationWindow()
				? GetPresentationWindow()->GetDpiScale() : 1.0f);
	}
	catch (...)
	{
	}
	InvalidateVisual();
	if (outError) outError->clear();
	return true;
}

void Control::ClearDeclarativeComponentBehavior() noexcept
{
	if (!_declarativeComponentBehavior) return;
	auto behavior = std::move(_declarativeComponentBehavior);
	try { behavior->Detach(*this); }
	catch (...) {}
	if (!_isDestroying) InvalidateVisual();
}

bool Control::SetDeclarativeTypeDescriptor(
	std::shared_ptr<const DeclarativeType> descriptor,
	std::wstring* outError)
{
	auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
	if (!descriptor)
		return fail(L"声明类型描述符不能为空。");
	const auto& currentDescriptor = GetDeclarativeTypeDescriptor();
	if (currentDescriptor)
	{
		if (currentDescriptor == descriptor)
		{
			if (outError) outError->clear();
			return true;
		}
		return fail(L"控件实例已经绑定到另一个声明类型；类型身份不可变。");
	}

	auto rejectNativeCollision = [&](const std::wstring& name)
		{
			if (!DependencyPropertyRegistry::FindNative(*this, name)) return false;
			if (outError) *outError = L"声明类型成员不能覆盖控件已有属性："
				+ name;
			return true;
		};
	for (const auto* property : descriptor->Properties())
		if (property && rejectNativeCollision(property->Name())) return false;
	for (const auto& event : descriptor->Events())
		if (rejectNativeCollision(event.Name)) return false;
	for (const auto& content : descriptor->ContentProperties())
		if (rejectNativeCollision(content.Name)) return false;

	std::vector<BindingValue> values;
	values.reserve(descriptor->PropertyCount());
	for (std::size_t slot = 0; slot < descriptor->PropertyCount(); ++slot)
	{
		BindingValue value;
		if (!descriptor->TryGetPropertyDefault(slot, value))
			return fail(L"声明类型描述符包含无效的属性值槽。");
		values.push_back(std::move(value));
	}
	const bool hasInheritedProperties = descriptor->HasInheritedProperties();
	InstallDynamicXamlPropertyState(
		std::move(descriptor), std::move(values));
	RefreshStyleValues(false);
	if (hasInheritedProperties)
		RefreshInheritedPropertiesRecursive();
	if (outError) outError->clear();
	return true;
}

auto Control::FindObjectPropertyMetadataByName(
	const std::wstring& propertyName) const
	-> DeclarativePropertyMetadataPointer
{
	const auto& descriptor = GetDeclarativeTypeDescriptor();
	if (descriptor) return descriptor->FindProperty(propertyName);
	const auto* metadata = FindCompiledComponentPropertyCore(
		MakeComponentPropertyToken(propertyName));
	// The compiler rejects collisions for generated classes. Keep the authored
	// name comparison only in the Design adapter so hand-written subclasses
	// cannot accidentally alias a colliding token.
	return metadata && metadata->Name() == propertyName ? metadata : nullptr;
}

auto Control::GetObjectPropertyMetadata() const
	-> DeclarativePropertyMetadataCollection
{
	const auto& descriptor = GetDeclarativeTypeDescriptor();
	if (descriptor)
	{
		const auto properties = descriptor->Properties();
		return std::vector<const DependencyPropertyMetadata*>(
			properties.begin(), properties.end());
	}
	// Compiled component properties are normal registered dependency
	// properties. DependencyPropertyRegistry appends them after this optional
	// object-local Design collection, so no generated name/vector table is
	// required.
	return {};
}

bool Control::TryGetDeclarativePropertyBacking(
	const DeclarativeType& owner,
	std::size_t slot,
	BindingValue& out) const
{
	return TryReadDynamicXamlPropertySlot(owner, slot, out);
}

bool Control::TrySetDeclarativePropertyBacking(
	const DeclarativeType& owner,
	std::size_t slot,
	const BindingValue& value)
{
	return TryWriteDynamicXamlPropertySlot(owner, slot, value);
}

Control* Control::FindDeclarativeTemplatePart(
	const std::wstring& localName) noexcept
{
	return const_cast<Control*>(static_cast<const Control*>(this)
		->FindDeclarativeTemplatePart(localName));
}

const Control* Control::FindDeclarativeTemplatePart(
	const std::wstring& localName) const noexcept
{
	if (localName.empty()) return nullptr;
	const auto token = MakeTemplatePartToken(localName);
	const auto named = std::find_if(
		_templateNameScopeNames.begin(), _templateNameScopeNames.end(),
		[token](const auto& item) { return item.first == token; });
	// AOT registration deliberately carries no string into either build. Its
	// token was collision-checked by the compiler, so an absent sidecar remains
	// a valid compatibility lookup; dynamic registrations always have a name.
	if (named != _templateNameScopeNames.end()
		&& named->second != localName) return nullptr;
	return FindDeclarativeTemplatePart(token);
}

Control* Control::FindDeclarativeContentPresenter(
	const std::wstring& propertyName) noexcept
{
	return const_cast<Control*>(static_cast<const Control*>(this)
		->FindDeclarativeContentPresenter(propertyName));
}

const Control* Control::FindDeclarativeContentPresenter(
	const std::wstring& propertyName) const noexcept
{
	const auto found = std::find_if(
		_declarativeContentPresenters.begin(),
		_declarativeContentPresenters.end(),
		[&](const auto& item) { return item.first == propertyName; });
	return found == _declarativeContentPresenters.end() ? nullptr : found->second;
}

bool Control::RegisterDeclarativeTemplatePart(
	std::wstring localName,
	Control* instance)
{
	if (localName.empty() || !instance
		|| instance->GetTemplatedParent() != this) return false;
	const auto token = MakeTemplatePartToken(localName);
	// The name sidecar distinguishes an authored duplicate from a true hash
	// collision in Design/Dynamic XAML. Both are rejected before publishing
	// the compact token/pointer entry used by the runtime path.
	if (std::any_of(
		_templateNameScopeNames.begin(), _templateNameScopeNames.end(),
		[&](const auto& item)
		{ return item.first == token || item.second == localName; })) return false;
	_templateNameScopeNames.emplace_back(token, std::move(localName));
	try
	{
		if (!RegisterDeclarativeTemplatePart(token, instance))
		{
			_templateNameScopeNames.pop_back();
			return false;
		}
	}
	catch (...)
	{
		_templateNameScopeNames.pop_back();
		throw;
	}
	return true;
}

bool Control::RegisterDeclarativeContentPresenter(
	std::wstring propertyName,
	Control* instance)
{
	if (propertyName.empty() || !instance
		|| instance->GetTemplatedParent() != this
		|| FindDeclarativeContentPresenter(propertyName)) return false;
	_declarativeContentPresenters.emplace_back(
		std::move(propertyName), instance);
	return true;
}

const DeclarativeEventDefinition* Control::FindDeclarativeEvent(
	const std::wstring& eventName) const noexcept
{
	const auto& descriptor = GetDeclarativeTypeDescriptor();
	if (descriptor) return descriptor->FindEvent(eventName);
	return nullptr;
}

bool Control::RaiseDeclarativeEvent(
	std::wstring eventName,
	BindingValue value)
{
	DeclarativeEventArgs args;
	args.Name = std::move(eventName);
	args.Value = std::move(value);
	return RaiseDeclarativeEvent(args);
}

void Control::RegisterDependencyProperties()
{
	(void)BackgroundPropertyMetadataRelation();
	(void)ForegroundPropertyMetadataRelation();
	(void)BorderBrushPropertyMetadataRelation();
	(void)BorderThicknessPropertyMetadataRelation();
	(void)DataContextProperty();
	(void)TemplateProperty();
	(void)VisibilityProperty();
	(void)IsVisibleProperty();
	(void)IsEnabledProperty();
	(void)AllowDropProperty();
	(void)CanvasLeftProperty();
	(void)CanvasTopProperty();
	(void)CanvasRightProperty();
	(void)CanvasBottomProperty();
	(void)WidthProperty();
	(void)HeightProperty();
	(void)ActualWidthProperty();
	(void)ActualHeightProperty();
	(void)MarginProperty();
	(void)PaddingProperty();
	(void)HorizontalAlignmentProperty();
	(void)VerticalAlignmentProperty();
	(void)HorizontalContentAlignmentProperty();
	(void)VerticalContentAlignmentProperty();
	(void)ZIndexProperty();
	(void)GridRowProperty();
	(void)GridColumnProperty();
	(void)GridRowSpanProperty();
	(void)GridColumnSpanProperty();
	(void)DockPositionProperty();
	(void)MinWidthProperty();
	(void)MinHeightProperty();
	(void)MaxWidthProperty();
	(void)MaxHeightProperty();
	(void)FontFamilyProperty();
	(void)LanguageProperty();
	(void)FontSizeProperty();
	(void)ClipProperty();
	(void)ClipToBoundsProperty();
	(void)RenderTransformProperty();
	(void)RenderTransformOriginProperty();
	(void)ValidationHasErrorProperty();
	(void)ValidationErrorsProperty();
	(void)TagProperty();
	(void)CursorProperty();
	(void)FocusableProperty();
	(void)IsTabStopProperty();
	(void)TabIndexProperty();
	(void)IsFocusedProperty();
	(void)IsKeyboardFocusedProperty();
	(void)IsKeyboardFocusVisibleProperty();
	(void)IsKeyboardFocusWithinProperty();
	(void)IsMouseOverProperty();
	(void)IsMouseDirectlyOverProperty();
	(void)IsMouseCapturedProperty();
	(void)IsMouseCaptureWithinProperty();
	(void)IsFocusScopeProperty();
	(void)TabNavigationProperty();
	(void)DirectionalNavigationProperty();
	(void)AutomationNameProperty();
	(void)AutomationFullDescriptionProperty();
	(void)AutomationHelpTextProperty();
	(void)AutomationIdProperty();
}
