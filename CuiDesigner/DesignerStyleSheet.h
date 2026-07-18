#pragma once

#include "DesignerTypes.h"
#include "DesignerPropertyValue.h"
#include <string>
#include <vector>

struct DesignerStyleResource
{
	std::wstring Key;
	DesignerStyleValue Value;
	/** Non-empty when materialized from an external merged dictionary. */
	std::wstring SourceDictionary;

	bool operator==(const DesignerStyleResource&) const = default;
};

struct DesignerStyleSetter
{
	std::wstring PropertyName;
	bool UsesResource = false;
	std::wstring ResourceKey;
	DesignerStyleValue Literal;

	bool operator==(const DesignerStyleSetter&) const = default;
};

struct DesignerStyleCondition
{
	/** Canonical state property such as IsMouseOver or IsEnabled. */
	std::wstring Property;
	bool Value = true;

	bool operator==(const DesignerStyleCondition&) const = default;
};

struct DesignerStyleDataCondition
{
	std::wstring SourceProperty;
	DesignerStyleValue Value;

	bool operator==(const DesignerStyleDataCondition&) const = default;
};

struct DesignerStyleTrigger
{
	/** One condition is serialized as Trigger; multiple conditions as MultiTrigger. */
	std::vector<DesignerStyleCondition> Conditions;
	/** One condition is DataTrigger; multiple conditions are MultiDataTrigger. */
	std::vector<DesignerStyleDataCondition> DataConditions;
	std::vector<DesignerStyleSetter> Setters;

	bool operator==(const DesignerStyleTrigger&) const = default;
};

struct DesignerStyleRule
{
	bool HasType = false;
	UIClass Type = UIClass::UI_Base;
	std::wstring Id;
	/** Named style key, or an {x:Type ...} key, inherited before local setters. */
	std::wstring BasedOn;
	std::vector<std::wstring> Classes;
	ControlStyleState RequiredStates = ControlStyleState::None;
	ControlStyleState ExcludedStates = ControlStyleState::None;
	std::vector<DesignerStyleSetter> Setters;
	/** Effective DataTrigger predicates after runtime/clipboard expansion. */
	std::vector<DesignerStyleDataCondition> DataConditions;
	std::vector<DesignerStyleTrigger> Triggers;
	/** Non-empty when materialized from an external merged dictionary. */
	std::wstring SourceDictionary;

	bool operator==(const DesignerStyleRule&) const = default;
};

struct DesignerStyleSheet
{
	/** Authoring URIs declared through ResourceDictionary.MergedDictionaries. */
	std::vector<std::wstring> MergedDictionaries;
	std::vector<DesignerStyleResource> Resources;
	std::vector<DesignerStyleRule> Rules;

	bool Empty() const noexcept
	{
		return MergedDictionaries.empty() && Resources.empty() && Rules.empty();
	}
	bool operator==(const DesignerStyleSheet&) const = default;
};
