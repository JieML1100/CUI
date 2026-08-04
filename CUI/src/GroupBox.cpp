#include "GroupBox.h"
#include "Window.h"

#include <stdexcept>
#include <typeindex>
#include <utility>

namespace
{
	const DependencyPropertyMetadataRegistration&
		GroupBoxFocusableMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::FocusableProperty();
			DependencyPropertyOptions<GroupBox, bool> options;
			options.DefaultValue = false;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index ownerTypes[] = {
				std::type_index(typeid(Control))
			};
			const auto* base =
				DependencyPropertyRegistry::FindRegistered(
					ownerTypes, L"Focusable");
			if (!base)
				throw std::logic_error(
					"Control focus metadata must be registered before GroupBox");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				GroupBox, HeaderedContentControl, bool>(
					property, std::move(options));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		GroupBoxIsTabStopMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::IsTabStopProperty();
			DependencyPropertyOptions<GroupBox, bool> options;
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
					"Control focus metadata must be registered before GroupBox");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				GroupBox, HeaderedContentControl, bool>(
					property, std::move(options));
		}();
		return relation;
	}
}

UIClass GroupBox::Type()
{
	return UIClass::UI_GroupBox;
}

void GroupBox::RegisterDependencyProperties()
{
	HeaderedContentControl::RegisterDependencyProperties();
	CUI_DESIGN_METADATA_ONLY(
	(void)GroupBoxFocusableMetadataRelation();
	(void)GroupBoxIsTabStopMetadataRelation();
	)
}

const DependencyPropertyMetadata*
GroupBox::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::FocusableProperty())
		return &GroupBoxFocusableMetadataRelation().Metadata();
	if (&property == &Control::IsTabStopProperty())
		return &GroupBoxIsTabStopMetadataRelation().Metadata();
	return HeaderedContentControl::
		ResolveExactDependencyPropertyMetadata(property);
}

GroupBox::GroupBox()
	: HeaderedContentControl()
{
	RegisterDependencyProperties();
}

bool GroupBox::OnAccessKey(bool isMultiple)
{
	(void)isMultiple;
	auto* window = GetPresentationWindow();
	if (!window) return false;
	for (auto* candidate : window->GetTabOrder())
	{
		for (auto* current = candidate; current;
			current = current->GetVisualParent())
		{
			if (current != this) continue;
			return candidate && candidate->Focus();
		}
	}
	return false;
}
