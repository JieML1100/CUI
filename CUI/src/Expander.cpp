#include "Expander.h"

#include "ToggleButton.h"

#include <stdexcept>
#include <typeindex>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Expander, TValue> ExpanderPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags)
	{
		DependencyPropertyOptions<Expander, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	const DependencyPropertyMetadataRegistration&
		ExpanderIsTabStopMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::IsTabStopProperty();
			DependencyPropertyOptions<Expander, bool> options;
			options.DefaultValue = false;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index ownerTypes[] = {
				std::type_index(typeid(Control))
			};
			const auto* base =
				DependencyPropertyRegistry::FindRegistered(
					ownerTypes, L"IsTabStop");
			if (!base)
				throw std::logic_error(
					"Control.IsTabStop must be registered before Expander");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				Expander, HeaderedContentControl, bool>(
					property, std::move(options));
		}();
		return relation;
	}

}

UIClass Expander::Type()
{
	return UIClass::UI_Expander;
}

const DependencyProperty& Expander::IsExpandedProperty()
{
	static const auto registration = []
	{
		auto options = ExpanderPropertyOptions(
			false
			CUI_DESIGN_METADATA_ARGUMENTS(
				10, DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender
				| DependencyPropertyFlags::BindsTwoWayByDefault);
		options.Changed = [](
			Expander& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyExpandedStateChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<Expander, bool>(
			DependencyPropertyRegistrationLiteral(L"IsExpanded"),
			[](Expander& target) { return target.IsExpanded; },
			[](Expander& target, const bool& value)
			{ target.IsExpanded = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Expander::ExpandDirectionProperty()
{
	static const auto registration = []
	{
		auto options = ExpanderPropertyOptions(
			::ExpandDirection::Down
			CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender
				| DependencyPropertyFlags::BindsTwoWayByDefault);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Down", BindingValue(::ExpandDirection::Down) },
			{ L"Up", BindingValue(::ExpandDirection::Up) },
			{ L"Left", BindingValue(::ExpandDirection::Left) },
			{ L"Right", BindingValue(::ExpandDirection::Right) },
		};
		)
		options.Changed = [](
			Expander& target,
			const ::ExpandDirection&, const ::ExpandDirection&)
		{
			target.RequestLayout();
			target.InvalidateVisual();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			Expander, ::ExpandDirection>(
				DependencyPropertyRegistrationLiteral(L"ExpandDirection"),
				[](Expander& target) { return target.ExpandDirection; },
				[](Expander& target, const ::ExpandDirection& value)
				{ target.ExpandDirection = value; }, {}, std::move(options));
	}();
	return *registration;
}

void Expander::RegisterDependencyProperties()
{
	HeaderedContentControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsExpandedProperty();
	(void)ExpandDirectionProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)ExpanderIsTabStopMetadataRelation();
	)
}

const DependencyPropertyMetadata*
Expander::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::IsTabStopProperty())
		return &ExpanderIsTabStopMetadataRelation().Metadata();
	return HeaderedContentControl::
		ResolveExactDependencyPropertyMetadata(property);
}

Expander::Expander()
	: HeaderedContentControl()
{
	RegisterDependencyProperties();
}

GET_CPP(Expander, bool, IsExpanded)
{
	return _isExpanded;
}

SET_CPP(Expander, bool, IsExpanded)
{
	(void)SetPropertyField(IsExpandedProperty(), _isExpanded, value);
}

GET_CPP(Expander, ::ExpandDirection, ExpandDirection)
{
	return _expandDirection;
}

SET_CPP(Expander, ::ExpandDirection, ExpandDirection)
{
	(void)SetPropertyField(
		ExpandDirectionProperty(), _expandDirection, value);
}

void Expander::OnControlTemplatePresentationChanged()
{
	const ControlWeakReference lifetime(this);
	ClearTemplatePartEventConnections();
	_headerSite = nullptr;
	HeaderedContentControl::OnControlTemplatePresentationChanged();

	auto* source = dynamic_cast<Expander*>(lifetime.Get());
	if (!source) return;
	source->_headerSite = dynamic_cast<ToggleButton*>(
		source->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"HeaderSite")));
	if (source->_headerSite)
	{
		source->RetainTemplatePartEventConnection(
			source->_headerSite->Click.Subscribe(
				[lifetime](Control* sender, RoutedEventArgs&)
				{
					auto* expander =
						dynamic_cast<Expander*>(lifetime.Get());
					auto* header = expander
						? expander->_headerSite : nullptr;
					if (!expander || sender != header) return;
					expander->SetCurrentExpanded(
						header->IsChecked == true);
				}));
	}
	source->SynchronizeHeaderSite();
}

void Expander::SynchronizeHeaderSite()
{
	if (!_headerSite) return;
	(void)_headerSite->TrySetCurrentPropertyValue(
		ToggleButton::IsCheckedProperty(),
		BindingValue(NullableBool(_isExpanded)));
}

void Expander::ApplyExpandedStateChange(
	bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	const ControlWeakReference lifetime(this);
	SynchronizeHeaderSite();
	auto* source = dynamic_cast<Expander*>(lifetime.Get());
	if (!source) return;
	source->RequestLayout();
	source->InvalidateVisual();
	source->NotifyAccessibilityStateChanged();

	source = dynamic_cast<Expander*>(lifetime.Get());
	if (!source) return;
	RoutedEventArgs args;
	if (newValue) source->Expanded(source, args);
	else source->Collapsed(source, args);
}

void Expander::SetCurrentExpanded(bool value)
{
	(void)SetCurrentPropertyField(
		IsExpandedProperty(), _isExpanded, value);
}

void Expander::SetExpanded(bool value)
{
	IsExpanded = value;
}

void Expander::Toggle()
{
	SetCurrentExpanded(!_isExpanded);
}

bool Expander::OnAccessKey(bool isMultiple)
{
	if (!IsEffectivelyEnabled() || !IsVisible) return false;
	if (_headerSite)
	{
		const ControlWeakReference lifetime(this);
		const ControlWeakReference headerLifetime(_headerSite);
		(void)_headerSite->Focus();
		auto* source = dynamic_cast<Expander*>(lifetime.Get());
		auto* header = dynamic_cast<ToggleButton*>(
			headerLifetime.Get());
		return source && header && source->_headerSite == header
			&& header->InvokeAccessKey(isMultiple);
	}
	Toggle();
	return true;
}
