#include "Style.h"
#include "Application.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"
#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace
{
	int EffectiveSlotIndex(DependencyPropertyValueSource source) noexcept
	{
		const int value = static_cast<int>(source);
		return value >= static_cast<int>(DependencyPropertyValueSource::Inherited)
			&& value <= static_cast<int>(DependencyPropertyValueSource::Animation)
			? value - static_cast<int>(DependencyPropertyValueSource::Inherited)
			: -1;
	}

	bool StyleNameEquals(
		const std::wstring& left,
		const std::wstring& right) noexcept
	{
		return left == right;
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

ControlStyleSheet::ControlStyleSheet() = default;
ControlStyleSheet::~ControlStyleSheet() = default;

bool ControlStyleSelector::MatchesTargetType(Control& target) const
{
	if (Type.has_value()
		&& *Type != UIClass::UI_Base
		&& target.Type() != *Type)
		return false;
	if ((!DeclarativeTypeNamespace.empty() || !DeclarativeTypeName.empty())
		&& (!StyleNameEquals(
			DeclarativeTypeNamespace, target.GetDeclarativeTypeNamespace())
			|| !StyleNameEquals(
				DeclarativeTypeName, target.GetDeclarativeTypeName())))
		return false;
	return true;
}

bool ControlStyleSelector::MatchesConditions(Control& target) const
{
	for (const auto& condition : PropertyConditions)
	{
		const auto* metadata = target.FindPropertyMetadata(
			condition.PropertyName);
		BindingValue actual;
		BindingValue expected;
		if (!metadata || !metadata->CanRead()
			|| !metadata->TryGet(target, actual)
			|| !metadata->TryConvert(condition.Value, expected)
			|| !metadata->ValuesEqual(actual, expected)) return false;
	}
	return true;
}

bool ControlStyleSelector::IsConditional() const noexcept
{
	return !PropertyConditions.empty()
		|| !DataConditions.empty();
}

size_t ControlStyleSheet::AddRule(
	ControlStyleSelector selector,
	std::vector<ControlStyleSetter> setters,
	std::vector<DeclarativeEventTriggerActionDefinition> enterActions,
	std::vector<DeclarativeEventTriggerActionDefinition> exitActions)
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
	if (normalized.empty() && enterActions.empty() && exitActions.empty()) return 0;

	const size_t id = _nextRuleId++;
	_rules.push_back(ControlStyleRule{
		id, std::move(selector), std::move(normalized),
		std::move(enterActions), std::move(exitActions) });
	NotifyChanged();
	return id;
}

bool ControlStyleSheet::RemoveRule(size_t ruleId)
{
	const auto position = std::find_if(_rules.begin(), _rules.end(),
		[ruleId](const auto& rule) { return rule.Id == ruleId; });
	if (position == _rules.end()) return false;
	_rules.erase(position);
	NotifyChanged();
	return true;
}

void ControlStyleSheet::ClearRules()
{
	if (_rules.empty()) return;
	_rules.clear();
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

ControlStyleResolution ControlStyleSheet::Resolve(
	Control& target,
	bool themeStyle) const
{
	ControlStyleResolution result;
	struct Winner
	{
		ResolvedControlStyleSetter Setter;
		size_t RuleOrder = 0;
	};
	std::vector<Winner> winners;

	const auto& requestedKey =
		cui::framework::StyleAccess::ResourceKey(target);
	auto matchesStyleIdentity = [&](const ControlStyleSelector& selector)
	{
		if (!selector.MatchesTargetType(target)) return false;
		if (themeStyle || requestedKey.empty())
		{
			if (!selector.StyleResourceKey.empty()) return false;
			return selector.Type.has_value()
				|| !selector.DeclarativeTypeName.empty();
		}
		return StyleNameEquals(selector.StyleResourceKey, requestedKey);
	};

	for (const auto& rule : _rules)
		if (matchesStyleIdentity(rule.Selector))
		{
			result.HasStyle = true;
			break;
		}
	if (!result.HasStyle) return result;

	for (size_t ruleOrder = 0; ruleOrder < _rules.size(); ++ruleOrder)
	{
		const auto& rule = _rules[ruleOrder];
		const bool identityMatched = matchesStyleIdentity(rule.Selector);
		const bool conditionMatched = identityMatched
			&& rule.Selector.MatchesConditions(target);
		const bool matched = conditionMatched
			&& MatchesDataConditions(rule.Selector, target);
		if (identityMatched
			&& (!rule.EnterActions.empty() || !rule.ExitActions.empty()))
			result.Triggers.push_back({ rule.Id, matched,
				rule.EnterActions, rule.ExitActions });
		if (!matched) continue;
		const bool conditional = rule.Selector.IsConditional();
		for (const auto& setter : rule.Setters)
		{
			BindingValue value = setter.Value.Literal;
			const bool foundResource = !setter.Value.IsResource()
				|| (setter.Value.IsDynamicResource
					? target.TryFindResource(setter.Value.ResourceKey, value)
					: TryGetResource(setter.Value.ResourceKey, value));
			if (setter.Value.IsResource() && !foundResource)
			{
				if (!setter.Value.IsDynamicResource)
				{
					result.Issues.push_back(ControlStyleResolutionIssue{
						ControlStyleResolutionIssueCode::MissingResource,
						rule.Id,
						setter.PropertyName,
						setter.Value.ResourceKey });
					continue;
				}
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
			if ((!setter.Value.IsDynamicResource || foundResource)
				&& !metadata->TryConvert(value, converted))
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
			ResolvedControlStyleSetter candidate;
			candidate.PropertyName = setter.PropertyName;
			if (!setter.Value.IsDynamicResource || foundResource)
				candidate.Value = std::move(converted);
			candidate.ResourceKey = setter.Value.ResourceKey;
			candidate.IsDynamicResource = setter.Value.IsDynamicResource;
			candidate.RuleId = rule.Id;
			candidate.IsConditional = conditional;
			if (winner == winners.end())
			{
				winners.push_back(Winner{ std::move(candidate), ruleOrder });
			}
			else if (conditional > winner->Setter.IsConditional
				|| (conditional == winner->Setter.IsConditional
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
			return left.Setter.PropertyName < right.Setter.PropertyName;
		});
	result.Setters.reserve(winners.size());
	for (auto& winner : winners)
		result.Setters.push_back(std::move(winner.Setter));
	return result;
}

bool ControlStyleSheet::UsesPropertyCondition(
	const std::wstring& propertyName) const
{
	if (propertyName.empty()) return false;
	for (const auto& rule : _rules)
		for (const auto& condition : rule.Selector.PropertyConditions)
			if (StyleNameEquals(condition.PropertyName, propertyName))
				return true;
	return false;
}

std::vector<std::wstring> ControlStyleSheet::DataConditionPaths() const
{
	std::vector<std::wstring> result;
	for (const auto& rule : _rules)
		for (const auto& condition : rule.Selector.DataConditions)
			if (!condition.SourceProperty.empty()
				&& !ContainsStyleName(result, condition.SourceProperty))
				result.push_back(condition.SourceProperty);
	return result;
}

EventConnection ControlStyleSheet::SubscribeChanged(
	std::function<void()> handler) const
{
	return _changed.Subscribe(std::move(handler));
}

bool ControlStyleSheet::MatchesDataConditions(
	const ControlStyleSelector& selector,
	Control& target) const
{
	if (selector.DataConditions.empty()) return true;
	auto* source = target.GetDataContext().Get();
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

void ControlStyleSheet::NotifyChanged() const
{
	++_revision;
	cui::framework::EventAccess::Raise(_changed);
}

void Control::SetStyleResourceKey(std::wstring value)
{
	if (StyleNameEquals(_styleResourceKey, value)) return;
	_styleResourceKey = std::move(value);
	RefreshStyleValues(false);
}

bool Control::HasVisibleStyleRules() const noexcept
{
	if (_themeStyleSheet && !_themeStyleSheet->Rules().empty()) return true;
	if (_styleSheet && !_styleSheet->Rules().empty()) return true;
	if (const auto* application = Application::Current())
	{
		const auto resources = application->GetResourcesSnapshot();
		if (resources && !resources->Rules().empty()) return true;
	}
	for (const Control* scope = this; scope;
		scope = scope->GetInheritanceParent())
		if (scope->_resourceDictionary
			&& !scope->_resourceDictionary->Rules().empty())
			return true;
	return false;
}

ControlStyleState Control::GetEffectiveStyleState() const noexcept
{
	auto result = _styleState;
	if (!IsEffectivelyEnabled()) result |= ControlStyleState::Disabled;
	return result;
}

void Control::SetStyleState(ControlStyleState state, bool enabled)
{
	const auto next = enabled ? (_styleState | state) : (_styleState & ~state);
	if (next == _styleState) return;
	const bool wasPressed = HasControlStyleState(
		_styleState, ControlStyleState::Pressed);
	const bool isPressed = HasControlStyleState(
		next, ControlStyleState::Pressed);
	_styleState = next;
	if (wasPressed != isPressed)
		OnPressedVisualStateChanged(isPressed);
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
			[this]()
			{
				RebuildStyleDataContextSubscriptions();
				RefreshDynamicResourceValues(false);
				RefreshStyleValues(false);
			});
	}
	RebuildStylePropertyConditionSubscription();
	RebuildStyleDataContextSubscriptions();
	bool result = RefreshDynamicResourceValues(false);
	if (!RefreshStyleValues(false)) result = false;
	if (recursive)
	{
		for (auto* child : _inheritanceChildren)
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
			[this]()
			{
				RebuildStyleDataContextSubscriptions();
				RefreshDynamicResourceValues(false);
				RefreshStyleValues(false);
			});
	}
	RebuildStylePropertyConditionSubscription();
	RebuildStyleDataContextSubscriptions();
	bool result = RefreshDynamicResourceValues(false);
	if (!RefreshStyleValues(false)) result = false;
	if (recursive)
	{
		for (auto* child : _inheritanceChildren)
		{
			if (child && !child->SetStyleSheet(_styleSheet, true))
				result = false;
		}
	}
	return result;
}

bool Control::SetResourceDictionary(
	std::shared_ptr<const ControlStyleSheet> value)
{
	const auto previous = _resourceDictionary;
	auto connect = [this]()
	{
		_resourceDictionaryConnection.Disconnect();
		if (!_resourceDictionary) return;
		_resourceDictionaryConnection = _resourceDictionary->SubscribeChanged(
			[this]()
			{
				// A local entry can feed this control or any descendant. Rules and
				// their trigger subscriptions are lexical for the same parent route.
				RebuildStyleSubscriptions(true);
				(void)RefreshDynamicResourceValues(true);
				(void)RefreshStyleValues(true);
			});
	};
	_resourceDictionaryConnection.Disconnect();
	_resourceDictionary = std::move(value);
	connect();
	RebuildStyleSubscriptions(true);
	bool result = RefreshDynamicResourceValues(true);
	if (!RefreshStyleValues(true)) result = false;
	if (result) return true;
	_resourceDictionary = previous;
	connect();
	RebuildStyleSubscriptions(true);
	(void)RefreshDynamicResourceValues(true);
	(void)RefreshStyleValues(true);
	return false;
}

bool Control::TryFindResource(
	const std::wstring& resourceKey,
	BindingValue& value) const
{
	if (resourceKey.empty()) return false;
	for (const Control* scope = this; scope;
		scope = scope->GetInheritanceParent())
		if (scope->_resourceDictionary
			&& scope->_resourceDictionary->TryGetResource(resourceKey, value))
			return true;
	if (_styleSheet && _styleSheet->TryGetResource(resourceKey, value))
		return true;
	if (const auto* application = Application::Current();
		application && application->TryFindResource(resourceKey, value))
		return true;
	return _themeStyleSheet
			&& _themeStyleSheet->TryGetResource(resourceKey, value);
}

bool Control::TryResolveDynamicResource(
	const std::wstring& resourceKey,
	BindingValue& value) const
{
	return TryFindResource(resourceKey, value);
}

bool Control::SetDynamicResource(
	const std::wstring& propertyName,
	std::wstring resourceKey)
{
	return SetDynamicResource(
		propertyName, std::move(resourceKey),
		DependencyPropertyValueSource::Local);
}

bool Control::SetDynamicResource(
	const std::wstring& propertyName,
	std::wstring resourceKey,
	DependencyPropertyValueSource source)
{
	return TrySetDynamicResourceExpressionOwned(
		propertyName, std::move(resourceKey), source);
}

bool Control::TrySetDynamicResourceExpressionOwned(
	const std::wstring& propertyName,
	std::wstring resourceKey,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite() || resourceKey.empty()
		|| EffectiveSlotIndex(source) < 0
		|| source == DependencyPropertyValueSource::Animation) return false;

	std::optional<BindingValue> proposed;
	BindingValue value;
	if (TryResolveDynamicResource(resourceKey, value))
	{
		BindingValue converted;
		if (!metadata->TryConvert(value, converted)) return false;
		proposed = std::move(converted);
	}
	return TrySetEffectiveValueEntry(
		*metadata, std::move(proposed), source,
		DependencyPropertyExpressionKind::DynamicResource,
		nullptr, std::move(resourceKey), false);
}

bool Control::ClearDynamicResource(
	const std::wstring& propertyName)
{
	return ClearDynamicResource(
		propertyName, DependencyPropertyValueSource::Local);
}

bool Control::ClearDynamicResource(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	const int index = EffectiveSlotIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end()
		|| entry->second.Slots[(size_t)index].Expression
			!= DependencyPropertyExpressionKind::DynamicResource) return false;
	return ClearPropertyValueOwned(
		metadata->Name(), source, nullptr);
}

bool Control::TryGetDynamicResourceKey(
	const std::wstring& propertyName,
	std::wstring& resourceKey,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	const int index = EffectiveSlotIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end()) return false;
	const auto& slot = entry->second.Slots[(size_t)index];
	if (slot.Expression != DependencyPropertyExpressionKind::DynamicResource)
		return false;
	resourceKey = slot.ResourceKey;
	return true;
}

bool Control::RefreshDynamicResourceValues(bool recursive)
{
	if (_refreshingDynamicResources) return true;
	_refreshingDynamicResources = true;
	bool result = true;
	struct Expression
	{
		std::wstring PropertyName;
		std::wstring ResourceKey;
		DependencyPropertyValueSource Source = DependencyPropertyValueSource::Local;
	};
	std::vector<Expression> expressions;
	for (const auto& [metadata, entry] : _propertyValues)
	{
		if (!metadata) continue;
		for (size_t index = 0; index < entry.Slots.size(); ++index)
		{
			const auto& slot = entry.Slots[index];
			if (slot.Expression != DependencyPropertyExpressionKind::DynamicResource)
				continue;
			expressions.push_back({ metadata->Name(), slot.ResourceKey,
				static_cast<DependencyPropertyValueSource>(
					index + static_cast<size_t>(
						DependencyPropertyValueSource::Inherited)) });
		}
	}
	for (auto& expression : expressions)
		if (!TrySetDynamicResourceExpressionOwned(
			expression.PropertyName, std::move(expression.ResourceKey),
			expression.Source)) result = false;
	_refreshingDynamicResources = false;
	if (recursive)
	{
		for (auto* child : _inheritanceChildren)
			if (child && !child->RefreshDynamicResourceValues(true)) result = false;
	}
	return result;
}

void Control::RebuildStylePropertyConditionSubscription()
{
	_stylePropertyConditionConnection.Disconnect();
	const auto authorSheets = VisibleAuthorStyleSheets();
	if (!_themeStyleSheet && authorSheets.empty()) return;
	_stylePropertyConditionConnection = PropertyChanged().Subscribe(
		[this, authorSheets](const PropertyChangedEventArgs& args)
		{
			const bool usedByTheme = _themeStyleSheet
				&& _themeStyleSheet->UsesPropertyCondition(args.PropertyName);
			const bool usedByStyle = std::any_of(
				authorSheets.begin(), authorSheets.end(), [&](const auto& sheet)
				{
					return sheet
						&& sheet->UsesPropertyCondition(args.PropertyName);
				});
			if (usedByTheme || usedByStyle) RefreshStyleValues(false);
		});
}

void Control::RebuildStyleDataContextSubscriptions()
{
	_styleDataContextConnections.clear();
	_styleDataContextOwners.clear();
	if (!GetDataContext()) return;

	std::vector<std::wstring> paths;
	auto appendPaths = [&](const std::shared_ptr<const ControlStyleSheet>& sheet)
	{
		if (!sheet) return;
		for (auto& path : sheet->DataConditionPaths())
			if (!ContainsStyleName(paths, path))
				paths.push_back(std::move(path));
	};
	appendPaths(_themeStyleSheet);
	for (const auto& sheet : VisibleAuthorStyleSheets()) appendPaths(sheet);
	if (paths.empty()) return;

	auto refresh = [this](const PropertyChangedEventArgs& args,
		const std::wstring& expectedProperty)
	{
		if (!args.PropertyName.empty()
			&& !StyleNameEquals(args.PropertyName, expectedProperty)) return;
		RebuildStyleDataContextSubscriptions();
		RefreshStyleValues(false);
	};
	for (const auto& pathText : paths)
	{
		std::vector<std::wstring> segments;
		if (!TryParseDataPath(pathText, segments)) continue;
		IBindingSource* current = &DataContextSource();
		for (size_t index = 0; index < segments.size(); ++index)
		{
			const auto expectedProperty = segments[index];
			auto connection = current->PropertyChanged().Subscribe(
				[this, refresh, expectedProperty](
					const PropertyChangedEventArgs& args)
				{ refresh(args, expectedProperty); });
			if (connection.Connected())
				_styleDataContextConnections.push_back(std::move(connection));
			if (index + 1 == segments.size()) break;
			BindingValue intermediate;
			BindingSourceReference reference;
			if (!current->TryGetValue(expectedProperty, intermediate)
				|| !intermediate.TryGet(reference) || !reference) break;
			_styleDataContextOwners.push_back(reference.Shared());
			current = reference.Get();
		}
	}
}

void Control::RebuildStyleSubscriptions(bool recursive)
{
	RebuildStylePropertyConditionSubscription();
	RebuildStyleDataContextSubscriptions();
	if (!recursive) return;
	for (auto* child : _inheritanceChildren)
		if (child) child->RebuildStyleSubscriptions(true);
}

std::vector<std::shared_ptr<const ControlStyleSheet>>
Control::VisibleAuthorStyleSheets() const
{
	std::vector<std::shared_ptr<const ControlStyleSheet>> result;
	if (const auto* application = Application::Current())
	{
		const auto resources = application->GetResourcesSnapshot();
		if (resources && !resources->Rules().empty())
			result.push_back(resources);
	}
	if (_styleSheet) result.push_back(_styleSheet);
	std::vector<std::shared_ptr<const ControlStyleSheet>> lexical;
	for (const Control* scope = this; scope;
		scope = scope->GetInheritanceParent())
		if (scope->_resourceDictionary
			&& !scope->_resourceDictionary->Rules().empty())
			lexical.push_back(scope->_resourceDictionary);
	for (auto item = lexical.rbegin(); item != lexical.rend(); ++item)
		if (std::none_of(result.begin(), result.end(), [&](const auto& existing)
			{ return existing.get() == item->get(); }))
			result.push_back(*item);
	return result;
}

bool Control::RefreshStyleValuesForSource(
	DependencyPropertyValueSource source,
	const std::vector<std::shared_ptr<const ControlStyleSheet>>& sheets,
	std::vector<std::wstring>& appliedProperties)
{
	ControlStyleResolution resolution;
	std::shared_ptr<const ControlStyleSheet> selectedSheet;
	bool result = true;
	const bool themeStyle = source == DependencyPropertyValueSource::Theme;
	for (auto item = sheets.rbegin(); item != sheets.rend(); ++item)
	{
		const auto& sheet = *item;
		if (!sheet) continue;
		auto candidate = sheet->Resolve(*this, themeStyle);
		if (!candidate.HasStyle) continue;
		resolution = std::move(candidate);
		selectedSheet = sheet;
		break;
	}
	if (!resolution.Success()) result = false;
	std::vector<std::shared_ptr<const ControlStyleSheet>> activeSheets;
	if (selectedSheet) activeSheets.push_back(selectedSheet);
	std::vector<std::wstring> nextProperties;
	nextProperties.reserve(resolution.Setters.size());

	for (const auto& setter : resolution.Setters)
	{
		const bool wasApplied = ContainsStyleName(
			appliedProperties, setter.PropertyName);
		const bool applied = setter.IsDynamicResource
			? TrySetDynamicResourceExpressionOwned(
				setter.PropertyName, setter.ResourceKey, source)
			: TrySetPropertyValue(setter.PropertyName, setter.Value, source);
		if (applied)
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

	// WPF establishes the Style's base values before its Trigger clocks are
	// materialized.  Object-path animations such as
	// (Control.Background).(SolidColorBrush.Color) must therefore resolve
	// against the brush supplied by a Setter, not against a fabricated native
	// fallback value.
	if (selectedSheet
		&& !SynchronizeStyleTriggerActions(source, selectedSheet, resolution))
		result = false;
	PruneStyleTriggerActions(source, activeSheets);
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
		std::vector<std::shared_ptr<const ControlStyleSheet>> themeSheets;
		if (_themeStyleSheet) themeSheets.push_back(_themeStyleSheet);
		if (!RefreshStyleValuesForSource(
			DependencyPropertyValueSource::Theme,
			themeSheets,
			_styleSheetProperties[0]))
			result = false;
		if (!RefreshStyleValuesForSource(
			DependencyPropertyValueSource::Style,
			VisibleAuthorStyleSheets(),
			_styleSheetProperties[1]))
			result = false;
		++pass;
	} while (_styleRefreshPending && pass < 8);
	if (_styleRefreshPending) result = false;
	_refreshingStyleValues = false;

	if (recursive)
	{
		for (auto* child : _inheritanceChildren)
		{
			if (child && !child->RefreshStyleValues(true)) result = false;
		}
	}
	return result;
}
