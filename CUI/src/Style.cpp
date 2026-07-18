#include "Style.h"
#include <algorithm>
#include <bit>
#include <cwchar>
#include <cwctype>

namespace
{
	bool StyleNameEquals(
		const std::wstring& left,
		const std::wstring& right) noexcept
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	bool ContainsStyleName(
		const std::vector<std::wstring>& values,
		const std::wstring& value) noexcept
	{
		return std::any_of(values.begin(), values.end(),
			[&value](const auto& current)
			{
				return StyleNameEquals(current, value);
			});
	}

	uint32_t StyleStateCount(ControlStyleState value) noexcept
	{
		return static_cast<uint32_t>(
			std::popcount(static_cast<uint32_t>(value)));
	}

	bool TryParseDataPath(
		const std::wstring& value,
		std::vector<std::wstring>& segments)
	{
		segments.clear();
		size_t start = 0;
		while (start <= value.size())
		{
			const auto separator = value.find(L'.', start);
			const auto end = separator == std::wstring::npos
				? value.size() : separator;
			auto segment = value.substr(start, end - start);
			segment.erase(segment.begin(), std::find_if(segment.begin(), segment.end(),
				[](wchar_t ch) { return std::iswspace(ch) == 0; }));
			segment.erase(std::find_if(segment.rbegin(), segment.rend(),
				[](wchar_t ch) { return std::iswspace(ch) == 0; }).base(), segment.end());
			if (segment.empty())
			{
				segments.clear();
				return false;
			}
			segments.push_back(std::move(segment));
			if (separator == std::wstring::npos) break;
			start = separator + 1;
		}
		return !segments.empty();
	}

	bool TryReadDataPath(
		IBindingSource& source,
		const std::vector<std::wstring>& path,
		BindingValue& value)
	{
		if (path.empty()) return false;
		IBindingSource* current = &source;
		std::vector<std::shared_ptr<IBindingSource>> owners;
		for (size_t index = 0; index + 1 < path.size(); ++index)
		{
			BindingValue intermediate;
			BindingSourceReference reference;
			if (!current->TryGetValue(path[index], intermediate)
				|| !intermediate.TryGet(reference) || !reference)
				return false;
			owners.push_back(reference.Shared());
			current = reference.Get();
		}
		return current->TryGetValue(path.back(), value);
	}
}

struct ControlStyleSheet::DataContextState
{
	struct ObservedPath
	{
		std::wstring Path;
		std::vector<std::wstring> Segments;
		std::vector<EventConnection> Connections;
		std::vector<std::shared_ptr<IBindingSource>> Owners;
	};

	IBindingSource* Source = nullptr;
	std::weak_ptr<const void> Lifetime;
	std::vector<ObservedPath> Paths;
};

ControlStyleSheet::ControlStyleSheet() = default;
ControlStyleSheet::~ControlStyleSheet() = default;

bool ControlStyleSelector::Matches(Control& target) const
{
	if (Type.has_value()
		&& *Type != UIClass::UI_Base
		&& target.Type() != *Type)
		return false;
	if (!Id.empty() && !StyleNameEquals(Id, target.GetStyleId()))
		return false;
	for (const auto& styleClass : Classes)
	{
		if (!target.HasStyleClass(styleClass)) return false;
	}
	const auto state = target.GetEffectiveStyleState();
	if (!HasControlStyleState(state, RequiredStates)) return false;
	if ((state & ExcludedStates) != ControlStyleState::None) return false;
	return true;
}

uint32_t ControlStyleSelector::Specificity() const noexcept
{
	const uint32_t id = Id.empty() ? 0u : 1u;
	const uint32_t qualifiers = static_cast<uint32_t>(Classes.size())
		+ StyleStateCount(RequiredStates)
		+ StyleStateCount(ExcludedStates)
		+ static_cast<uint32_t>(DataConditions.size());
	const uint32_t exactType = Type.has_value() && *Type != UIClass::UI_Base
		? 1u
		: 0u;
	return id * 1'000'000u + qualifiers * 1'000u + exactType;
}

size_t ControlStyleSheet::AddRule(
	ControlStyleSelector selector,
	std::vector<ControlStyleSetter> setters)
{
	std::vector<ControlStyleSetter> normalized;
	for (auto& setter : setters)
	{
		if (setter.PropertyName.empty()) continue;
		auto existing = std::find_if(normalized.begin(), normalized.end(),
			[&setter](const auto& current)
			{
				return StyleNameEquals(
					current.PropertyName, setter.PropertyName);
			});
		if (existing == normalized.end())
			normalized.push_back(std::move(setter));
		else
			*existing = std::move(setter);
	}
	if (normalized.empty()) return 0;

	const size_t id = _nextRuleId++;
	_rules.push_back(ControlStyleRule{
		id, std::move(selector), std::move(normalized) });
	RebuildDataContextSubscriptions();
	NotifyChanged();
	return id;
}

bool ControlStyleSheet::RemoveRule(size_t ruleId)
{
	const auto position = std::find_if(_rules.begin(), _rules.end(),
		[ruleId](const auto& rule) { return rule.Id == ruleId; });
	if (position == _rules.end()) return false;
	_rules.erase(position);
	RebuildDataContextSubscriptions();
	NotifyChanged();
	return true;
}

void ControlStyleSheet::ClearRules()
{
	if (_rules.empty()) return;
	_rules.clear();
	RebuildDataContextSubscriptions();
	NotifyChanged();
}

bool ControlStyleSheet::SetResource(std::wstring key, BindingValue value)
{
	if (key.empty()) return false;
	auto existing = std::find_if(_resources.begin(), _resources.end(),
		[&key](const auto& entry) { return StyleNameEquals(entry.Key, key); });
	if (existing == _resources.end())
		_resources.push_back(ResourceEntry{ std::move(key), std::move(value) });
	else
		existing->Value = std::move(value);
	NotifyChanged();
	return true;
}

bool ControlStyleSheet::RemoveResource(const std::wstring& key)
{
	const auto position = std::find_if(_resources.begin(), _resources.end(),
		[&key](const auto& entry) { return StyleNameEquals(entry.Key, key); });
	if (position == _resources.end()) return false;
	_resources.erase(position);
	NotifyChanged();
	return true;
}

void ControlStyleSheet::ClearResources()
{
	if (_resources.empty()) return;
	_resources.clear();
	NotifyChanged();
}

bool ControlStyleSheet::TryGetResource(
	const std::wstring& key,
	BindingValue& value) const
{
	const auto existing = std::find_if(_resources.begin(), _resources.end(),
		[&key](const auto& entry) { return StyleNameEquals(entry.Key, key); });
	if (existing == _resources.end()) return false;
	value = existing->Value;
	return true;
}

ControlStyleResolution ControlStyleSheet::Resolve(Control& target) const
{
	ControlStyleResolution result;
	struct Winner
	{
		ResolvedControlStyleSetter Setter;
		size_t RuleOrder = 0;
	};
	std::vector<Winner> winners;

	for (size_t ruleOrder = 0; ruleOrder < _rules.size(); ++ruleOrder)
	{
		const auto& rule = _rules[ruleOrder];
		if (!rule.Selector.Matches(target)
			|| !MatchesDataConditions(rule.Selector)) continue;
		const uint32_t specificity = rule.Selector.Specificity();
		for (const auto& setter : rule.Setters)
		{
			BindingValue value = setter.Value.Literal;
			if (setter.Value.IsResource()
				&& !TryGetResource(setter.Value.ResourceKey, value))
			{
				result.Issues.push_back(ControlStyleResolutionIssue{
					ControlStyleResolutionIssueCode::MissingResource,
					rule.Id,
					setter.PropertyName,
					setter.Value.ResourceKey });
				continue;
			}
			const auto* metadata = target.FindPropertyMetadata(
				setter.PropertyName);
			if (!metadata)
			{
				result.Issues.push_back(ControlStyleResolutionIssue{
					ControlStyleResolutionIssueCode::PropertyNotFound,
					rule.Id,
					setter.PropertyName,
					{} });
				continue;
			}
			if (!metadata->CanWrite())
			{
				result.Issues.push_back(ControlStyleResolutionIssue{
					ControlStyleResolutionIssueCode::PropertyNotWritable,
					rule.Id,
					setter.PropertyName,
					{} });
				continue;
			}
			BindingValue converted;
			BindingValue effective;
			if (!metadata->TryConvert(value, converted)
				|| !metadata->TryCoerce(target, converted, effective))
			{
				result.Issues.push_back(ControlStyleResolutionIssue{
					ControlStyleResolutionIssueCode::InvalidValue,
					rule.Id,
					setter.PropertyName,
					{} });
				continue;
			}

			auto winner = std::find_if(winners.begin(), winners.end(),
				[&setter](const auto& current)
				{
					return StyleNameEquals(
						current.Setter.PropertyName, setter.PropertyName);
				});
			ResolvedControlStyleSetter candidate{
				setter.PropertyName, std::move(effective), rule.Id, specificity };
			if (winner == winners.end())
			{
				winners.push_back(Winner{ std::move(candidate), ruleOrder });
			}
			else if (specificity > winner->Setter.Specificity
				|| (specificity == winner->Setter.Specificity
					&& ruleOrder >= winner->RuleOrder))
			{
				winner->Setter = std::move(candidate);
				winner->RuleOrder = ruleOrder;
			}
		}
	}

	std::sort(winners.begin(), winners.end(),
		[](const auto& left, const auto& right)
		{
			return _wcsicmp(
				left.Setter.PropertyName.c_str(),
				right.Setter.PropertyName.c_str()) < 0;
		});
	result.Setters.reserve(winners.size());
	for (auto& winner : winners)
		result.Setters.push_back(std::move(winner.Setter));
	return result;
}

EventConnection ControlStyleSheet::SubscribeChanged(
	std::function<void()> handler) const
{
	return _changed.Subscribe(std::move(handler));
}

void ControlStyleSheet::SetDataContext(IBindingSource* source) const
{
	if (source && _dataContextState
		&& _dataContextState->Source == source
		&& !_dataContextState->Lifetime.expired()) return;
	if (!source)
	{
		if (!_dataContextState) return;
		_dataContextState.reset();
		NotifyChanged();
		return;
	}
	if (!_dataContextState)
		_dataContextState = std::make_unique<DataContextState>();
	_dataContextState->Source = source;
	_dataContextState->Lifetime = source->BindingLifetime();
	RebuildDataContextSubscriptions();
	NotifyChanged();
}

IBindingSource* ControlStyleSheet::DataContext() const noexcept
{
	if (!_dataContextState || _dataContextState->Lifetime.expired()) return nullptr;
	return _dataContextState->Source;
}

bool ControlStyleSheet::MatchesDataConditions(
	const ControlStyleSelector& selector) const
{
	if (selector.DataConditions.empty()) return true;
	auto* source = DataContext();
	if (!source) return false;
	for (const auto& condition : selector.DataConditions)
	{
		std::vector<std::wstring> path;
		BindingValue actual;
		if (!TryParseDataPath(condition.SourceProperty, path)
			|| !TryReadDataPath(*source, path, actual)
			|| actual.Empty()) return false;
		BindingValue expected;
		if (!TryConvertBindingValue(condition.Value, actual.Kind(), expected)
			|| !BindingValuesEqual(actual, expected)) return false;
	}
	return true;
}

void ControlStyleSheet::RebuildDataContextSubscriptions() const
{
	if (!_dataContextState) return;
	_dataContextState->Paths.clear();
	auto* source = DataContext();
	if (!source) return;
	std::vector<std::wstring> uniquePaths;
	for (const auto& rule : _rules)
		for (const auto& condition : rule.Selector.DataConditions)
			if (!condition.SourceProperty.empty()
				&& !ContainsStyleName(uniquePaths, condition.SourceProperty))
				uniquePaths.push_back(condition.SourceProperty);

	for (const auto& pathText : uniquePaths)
	{
		DataContextState::ObservedPath observed;
		observed.Path = pathText;
		if (!TryParseDataPath(pathText, observed.Segments)) continue;
		IBindingSource* current = source;
		for (size_t index = 0; index < observed.Segments.size(); ++index)
		{
			const auto expectedProperty = observed.Segments[index];
			auto connection = current->PropertyChanged().Subscribe(
				[this, expectedProperty](const PropertyChangedEventArgs& args)
				{
					if (!args.PropertyName.empty()
						&& !StyleNameEquals(args.PropertyName, expectedProperty)) return;
					RebuildDataContextSubscriptions();
					NotifyChanged();
				});
			if (connection.Connected())
				observed.Connections.push_back(std::move(connection));
			if (index + 1 == observed.Segments.size()) break;
			BindingValue intermediate;
			BindingSourceReference reference;
			if (!current->TryGetValue(expectedProperty, intermediate)
				|| !intermediate.TryGet(reference) || !reference) break;
			observed.Owners.push_back(reference.Shared());
			current = reference.Get();
		}
		_dataContextState->Paths.push_back(std::move(observed));
	}
}

void ControlStyleSheet::NotifyChanged() const
{
	++_revision;
	_changed();
}

void Control::SetStyleId(std::wstring value)
{
	if (StyleNameEquals(_styleId, value)) return;
	_styleId = std::move(value);
	RefreshStyleValues(false);
}

bool Control::HasStyleClass(const std::wstring& value) const
{
	return !value.empty() && ContainsStyleName(_styleClasses, value);
}

bool Control::AddStyleClass(std::wstring value)
{
	if (value.empty() || ContainsStyleName(_styleClasses, value)) return false;
	_styleClasses.push_back(std::move(value));
	RefreshStyleValues(false);
	return true;
}

bool Control::RemoveStyleClass(const std::wstring& value)
{
	const auto position = std::find_if(_styleClasses.begin(), _styleClasses.end(),
		[&value](const auto& current) { return StyleNameEquals(current, value); });
	if (position == _styleClasses.end()) return false;
	_styleClasses.erase(position);
	RefreshStyleValues(false);
	return true;
}

void Control::ClearStyleClasses()
{
	if (_styleClasses.empty()) return;
	_styleClasses.clear();
	RefreshStyleValues(false);
}

ControlStyleState Control::GetEffectiveStyleState() const noexcept
{
	auto result = _styleState;
	if (!Enable) result |= ControlStyleState::Disabled;
	if (Checked) result |= ControlStyleState::Checked;
	return result;
}

void Control::SetStyleState(ControlStyleState state, bool enabled)
{
	const auto next = enabled ? (_styleState | state) : (_styleState & ~state);
	if (next == _styleState) return;
	_styleState = next;
	RefreshStyleValues(false);
}

bool Control::SetThemeStyleSheet(
	std::shared_ptr<const ControlStyleSheet> value,
	bool recursive)
{
	_themeStyleConnection.Disconnect();
	_themeStyleSheet = std::move(value);
	if (_themeStyleSheet)
	{
		_themeStyleConnection = _themeStyleSheet->SubscribeChanged(
			[this]() { RefreshStyleValues(false); });
	}
	bool result = RefreshStyleValues(false);
	if (recursive)
	{
		for (auto* child : Children)
		{
			if (child && !child->SetThemeStyleSheet(_themeStyleSheet, true))
				result = false;
		}
	}
	return result;
}

bool Control::SetStyleSheet(
	std::shared_ptr<const ControlStyleSheet> value,
	bool recursive)
{
	_styleSheetConnection.Disconnect();
	_styleSheet = std::move(value);
	if (_styleSheet)
	{
		_styleSheetConnection = _styleSheet->SubscribeChanged(
			[this]() { RefreshStyleValues(false); });
	}
	bool result = RefreshStyleValues(false);
	if (recursive)
	{
		for (auto* child : Children)
		{
			if (child && !child->SetStyleSheet(_styleSheet, true))
				result = false;
		}
	}
	return result;
}

bool Control::RefreshStyleValuesForSource(
	ControlPropertyValueSource source,
	const std::shared_ptr<const ControlStyleSheet>& sheet,
	std::vector<std::wstring>& appliedProperties)
{
	ControlStyleResolution resolution;
	if (sheet) resolution = sheet->Resolve(*this);
	bool result = resolution.Success();
	std::vector<std::wstring> nextProperties;
	nextProperties.reserve(resolution.Setters.size());

	for (const auto& setter : resolution.Setters)
	{
		const bool wasApplied = ContainsStyleName(
			appliedProperties, setter.PropertyName);
		if (TrySetPropertyValue(setter.PropertyName, setter.Value, source))
		{
			nextProperties.push_back(setter.PropertyName);
		}
		else
		{
			result = false;
			if (wasApplied) nextProperties.push_back(setter.PropertyName);
		}
	}

	for (const auto& property : appliedProperties)
	{
		if (ContainsStyleName(nextProperties, property)) continue;
		if (!ClearPropertyValue(property, source)
			&& HasPropertyValue(property, source))
		{
			nextProperties.push_back(property);
			result = false;
		}
	}
	appliedProperties = std::move(nextProperties);
	return result;
}

bool Control::RefreshStyleValues(bool recursive)
{
	if (_refreshingStyleValues)
	{
		_styleRefreshPending = true;
		return true;
	}

	_refreshingStyleValues = true;
	bool result = true;
	int pass = 0;
	do
	{
		_styleRefreshPending = false;
		if (!RefreshStyleValuesForSource(
			ControlPropertyValueSource::Theme,
			_themeStyleSheet,
			_styleSheetProperties[0]))
			result = false;
		if (!RefreshStyleValuesForSource(
			ControlPropertyValueSource::Style,
			_styleSheet,
			_styleSheetProperties[1]))
			result = false;
		++pass;
	} while (_styleRefreshPending && pass < 8);
	if (_styleRefreshPending) result = false;
	_refreshingStyleValues = false;

	if (recursive)
	{
		for (auto* child : Children)
		{
			if (child && !child->RefreshStyleValues(true)) result = false;
		}
	}
	return result;
}
