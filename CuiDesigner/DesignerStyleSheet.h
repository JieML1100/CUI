#pragma once

#include "DesignerTypes.h"
#include "DesignerPropertyValue.h"
#include <string>
#include <vector>

struct DesignerStyleResource
{
	std::wstring Key;
	DesignerStyleValue Value;

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

struct DesignerStyleRule
{
	bool HasType = false;
	UIClass Type = UIClass::UI_Base;
	std::wstring Id;
	std::vector<std::wstring> Classes;
	ControlStyleState RequiredStates = ControlStyleState::None;
	ControlStyleState ExcludedStates = ControlStyleState::None;
	std::vector<DesignerStyleSetter> Setters;

	bool operator==(const DesignerStyleRule&) const = default;
};

struct DesignerStyleSheet
{
	std::vector<DesignerStyleResource> Resources;
	std::vector<DesignerStyleRule> Rules;

	bool Empty() const noexcept { return Resources.empty() && Rules.empty(); }
	bool operator==(const DesignerStyleSheet&) const = default;
};
