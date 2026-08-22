#include "Style.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"
#include "../include/XamlRuntimeSchema.h"

#include <algorithm>
#include <cwctype>
#include <utility>

#if !CUI_ENABLE_DYNAMIC_XAML
#error StyleMutableBackend.Design.cpp requires the Design runtime flavor.
#endif

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
		std::wstring_view left,
		std::wstring_view right) noexcept
	{
		return left == right;
	}

	bool ContainsStyleName(
		const std::vector<std::wstring>& values,
		std::wstring_view value) noexcept
	{
		return std::any_of(values.begin(), values.end(),
			[&](const auto& current) { return current == value; });
	}

	bool ContainsStyleProperty(
		const std::vector<DependencyPropertyReference>& values,
		const DependencyPropertyReference& value) noexcept
	{
		return std::any_of(values.begin(), values.end(),
			[&](const auto& current) { return current.Matches(value); });
	}

	template<typename TRange>
	auto CompiledRange(
		const TRange& values,
		const CompiledStyleRange& range) noexcept
	{
		const auto end = values.end();
		const size_t begin = range.Offset;
		if (begin > values.size()) return std::pair{ end, end };
		const size_t count = std::min<size_t>(
			range.Count, values.size() - begin);
		const auto first = values.begin() + begin;
		return std::pair{ first, first + count };
	}

	const std::wstring_view* CompiledString(
		const CompiledStyleProgramView& program,
		uint32_t index) noexcept
	{
		return index < program.Strings.size()
			? &program.Strings[index] : nullptr;
	}

	const std::wstring_view* DynamicDataPath(
		const CompiledStyleProgramView& program,
		uint32_t reference) noexcept
	{
		return IsCompiledStyleDynamicDataPathReference(reference)
			? CompiledString(
				program, CompiledStyleDataPathIndex(reference))
			: nullptr;
	}

	bool TryReadDataPath(
		const IBindingSource& source,
		const std::vector<std::wstring>& path,
		BindingValue& value)
	{
		if (path.empty()) return false;
		const IBindingSource* current = &source;
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

	std::vector<ControlStyleSetter> NormalizeStyleSetters(
		std::vector<ControlStyleSetter> setters)
	{
		std::vector<ControlStyleSetter> normalized;
		normalized.reserve(setters.size());
		for (auto& setter : setters)
		{
			if (setter.Property.Empty()) continue;
			auto existing = std::find_if(
				normalized.begin(), normalized.end(),
				[&](const auto& current)
				{ return current.Property.Matches(setter.Property); });
			if (existing == normalized.end())
				normalized.push_back(std::move(setter));
			else
				*existing = std::move(setter);
		}
		return normalized;
	}

	const DependencyPropertyMetadata* FindStylePropertyMetadata(
		Control& target,
		const DependencyPropertyReference& property)
	{
		if (const auto* identity = property.Identity())
			return target.GetPropertyMetadata(*identity);
		return cui::style::design::FindNamedPropertyMetadata(
			target, property.Name());
	}
}

namespace cui::style::design
{
	bool TryParseDataPathSegments(
		std::wstring_view value,
		std::vector<std::wstring>& segments)
	{
		segments.clear();
		size_t start = 0;
		while (start <= value.size())
		{
			const auto separator = value.find(L'.', start);
			const auto end = separator == std::wstring_view::npos
				? value.size() : separator;
			std::wstring segment(value.substr(start, end - start));
			segment.erase(segment.begin(), std::find_if(
				segment.begin(), segment.end(),
				[](wchar_t ch) { return std::iswspace(ch) == 0; }));
			segment.erase(std::find_if(
				segment.rbegin(), segment.rend(),
				[](wchar_t ch) { return std::iswspace(ch) == 0; }).base(),
				segment.end());
			if (segment.empty())
			{
				segments.clear();
				return false;
			}
			segments.push_back(std::move(segment));
			if (separator == std::wstring_view::npos) break;
			start = separator + 1;
		}
		return !segments.empty();
	}

	bool ValidateDynamicDataPathReference(
		const CompiledStyleProgramView& program,
		uint32_t reference,
		bool requireObserve)
	{
		(void)requireObserve;
		const auto* path = DynamicDataPath(program, reference);
		if (!path) return false;
		std::vector<std::wstring> parsed;
		return TryParseDataPathSegments(*path, parsed);
	}

	const DependencyPropertyMetadata* FindNamedPropertyMetadata(
		Control& target,
		const std::wstring& propertyName)
	{
		return target.FindPropertyMetadata(propertyName);
	}

	bool TryReadDynamicDataPath(
		const CompiledStyleProgramView& program,
		uint32_t reference,
		const IBindingSource& source,
		BindingValue& value)
	{
		const auto* pathText = DynamicDataPath(program, reference);
		std::vector<std::wstring> path;
		return pathText
			&& TryParseDataPathSegments(*pathText, path)
			&& TryReadDataPath(source, path, value);
	}
}

ControlStyleValue ControlStyleValue::Resource(std::wstring key)
{
	ControlStyleValue result;
	result.ResourceKey = std::move(key);
	return result;
}

ControlStyleValue ControlStyleValue::DynamicResource(std::wstring key)
{
	ControlStyleValue result;
	result.ResourceKey = std::move(key);
	result.IsDynamicResource = true;
	return result;
}

ControlStyleSetter::ControlStyleSetter(
	std::wstring propertyName,
	BindingValue value)
	: Property(DependencyPropertyReference(std::move(propertyName))),
	  Value(std::move(value))
{
}

ControlStyleSetter::ControlStyleSetter(
	std::wstring propertyName,
	ControlStyleValue value)
	: Property(DependencyPropertyReference(std::move(propertyName))),
	  Value(std::move(value))
{
}

ControlStyleSetter ControlStyleSetter::Resource(
	std::wstring propertyName,
	std::wstring resourceKey)
{
	return ControlStyleSetter(
		std::move(propertyName),
		ControlStyleValue::Resource(std::move(resourceKey)));
}

ControlStyleSetter ControlStyleSetter::DynamicResource(
	std::wstring propertyName,
	std::wstring resourceKey)
{
	return ControlStyleSetter(
		std::move(propertyName),
		ControlStyleValue::DynamicResource(std::move(resourceKey)));
}

ControlStylePropertyCondition::ControlStylePropertyCondition(
	std::wstring propertyName,
	BindingValue value)
	: Property(DependencyPropertyReference(std::move(propertyName))),
	  Value(std::move(value))
{
}

std::shared_ptr<const ControlStyleSheet>
ControlStyleSheet::CreateCompiled(CompiledStyleProgram program)
{
	auto tagDynamicPath = [](uint32_t& reference)
	{
		if (reference >= CompiledStyleDynamicDataPathFlag) return false;
		reference |= CompiledStyleDynamicDataPathFlag;
		return true;
	};
	for (auto& condition : program.DataConditions)
		if (!tagDynamicPath(condition.PathReference)) return {};
	for (auto& reference : program.DataPathWatchers)
		if (!tagDynamicPath(reference)) return {};
	for (auto& reference : program.GlobalDataPathWatchers)
		if (!tagDynamicPath(reference)) return {};

	auto owned = std::make_unique<CompiledStyleProgram>(std::move(program));
	std::vector<std::wstring_view> stringViews;
	stringViews.reserve(owned->Strings.size());
	for (const auto& value : owned->Strings)
		stringViews.emplace_back(value);

	CompiledStyleProgramView view{
		CompiledStyleProgramViewVersion,
		stringViews,
		{},
		owned->Resources,
		owned->ResourceLookup,
		owned->PropertyConditions,
		owned->DataConditions,
		owned->Setters,
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		owned->Rules,
		owned->RuleIndexes,
		owned->PropertyWatchers,
		owned->DataPathWatchers,
		owned->Groups,
		owned->GlobalPropertyWatchers,
		owned->GlobalDataPathWatchers,
		{} };
	const auto actionCount = owned->Actions.size();
	auto sheet = CreateCompiledCore(
		view, std::move(owned->Values), actionCount, false);
	if (!sheet) return {};
	sheet->_compiledProgram->DesignActions = std::move(owned->Actions);
	sheet->_compiledProgram->UsesDesignActions = true;
	sheet->_compiledProgram->OwnedProgram = std::move(owned);
	sheet->_compiledProgram->OwnedStringViews = std::move(stringViews);

	// Rebind all spans after ownership transfer; moved-from local vectors must
	// never remain part of the immutable adapter view.
	auto& compiled = *sheet->_compiledProgram;
	const auto& source = *compiled.OwnedProgram;
	compiled.Strings = compiled.OwnedStringViews;
	compiled.ValuePools = {};
	compiled.Resources = source.Resources;
	compiled.ResourceLookup = source.ResourceLookup;
	compiled.PropertyConditions = source.PropertyConditions;
	compiled.DataConditions = source.DataConditions;
	compiled.Setters = source.Setters;
	compiled.PropertyOperands = {};
	compiled.ObjectPathChildIndices = {};
	compiled.ObjectPaths = {};
	compiled.KeyFrames = {};
	compiled.PathSegments = {};
	compiled.Animations = {};
	compiled.TimelineGroups = {};
	compiled.Storyboards = {};
	compiled.Actions = {};
	compiled.Rules = source.Rules;
	compiled.RuleIndexes = source.RuleIndexes;
	compiled.PropertyWatchers = source.PropertyWatchers;
	compiled.DataPathWatchers = source.DataPathWatchers;
	compiled.Groups = source.Groups;
	compiled.GlobalPropertyWatchers = source.GlobalPropertyWatchers;
	compiled.GlobalDataPathWatchers = source.GlobalDataPathWatchers;
	compiled.DataPaths = {};
	return sheet;
}

bool ControlStyleSelector::MatchesTargetType(Control& target) const
{
	if (Type.has_value()
		&& *Type != UIClass::UI_Base
		&& target.Type() != *Type)
		return false;
	if (!DeclarativeTypeNamespace.empty() || !DeclarativeTypeName.empty())
	{
		const bool exact = StyleNameEquals(
			DeclarativeTypeNamespace, target.GetDeclarativeTypeNamespace())
			&& StyleNameEquals(
				DeclarativeTypeName, target.GetDeclarativeTypeName());
		if (!exact)
		{
			// Native-generated built-in children (for example DataGrid cells and
			// column editors) have no authored Dynamic-XAML sidecar. Production
			// already matches their compiled theme rules by native UIClass; keep
			// the mutable Design backend equivalent without letting a custom
			// ComponentDefinition style leak onto its native behavior host.
			if (!target.GetDeclarativeTypeName().empty()) return false;
			const auto* builtIn = CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
				DeclarativeTypeNamespace, DeclarativeTypeName);
			if (!builtIn || !builtIn->IsDefaultForNativeType
				|| builtIn->NativeType != target.Type()) return false;
		}
	}
	return true;
}

bool ControlStyleSelector::MatchesConditions(Control& target) const
{
	for (const auto& condition : PropertyConditions)
	{
		const auto* metadata = FindStylePropertyMetadata(
			target, condition.Property);
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
	return !PropertyConditions.empty() || !DataConditions.empty();
}

const std::vector<ControlStyleRule>&
ControlStyleSheet::Rules() const noexcept
{
	return _rules;
}

uint64_t ControlStyleSheet::Revision() const noexcept
{
	return _revision;
}

size_t ControlStyleSheet::AddRule(
	ControlStyleSelector selector,
	std::vector<ControlStyleSetter> setters,
	std::vector<DeclarativeEventTriggerActionDefinition> enterActions,
	std::vector<DeclarativeEventTriggerActionDefinition> exitActions)
{
	if (_compiledProgram) return 0;
	auto normalized = NormalizeStyleSetters(std::move(setters));
	if (normalized.empty() && enterActions.empty() && exitActions.empty())
		return 0;

	const size_t id = _nextRuleId++;
	_rules.push_back(ControlStyleRule{
		id, std::move(selector), std::move(normalized),
		std::move(enterActions), std::move(exitActions) });
	NotifyChanged();
	return id;
}

bool ControlStyleSheet::RemoveRule(size_t ruleId)
{
	if (_compiledProgram) return false;
	const auto position = std::find_if(
		_rules.begin(), _rules.end(),
		[ruleId](const auto& rule) { return rule.Id == ruleId; });
	if (position == _rules.end()) return false;
	_rules.erase(position);
	NotifyChanged();
	return true;
}

void ControlStyleSheet::ClearRules()
{
	if (_compiledProgram || _rules.empty()) return;
	_rules.clear();
	NotifyChanged();
}

size_t ControlStyleSheet::RuleCount() const noexcept
{
	return _compiledProgram ? _compiledProgram->Rules.size() : _rules.size();
}

bool ControlStyleSheet::SetResource(std::wstring key, BindingValue value)
{
	if (_compiledProgram || key.empty()) return false;
	auto existing = std::find_if(
		_resources.begin(), _resources.end(),
		[&](const auto& entry) { return StyleNameEquals(entry.Key, key); });
	if (existing == _resources.end())
		_resources.push_back(ResourceEntry{ std::move(key), std::move(value) });
	else
		existing->Value = std::move(value);
	NotifyChanged();
	return true;
}

bool ControlStyleSheet::RemoveResource(const std::wstring& key)
{
	if (_compiledProgram) return false;
	const auto position = std::find_if(
		_resources.begin(), _resources.end(),
		[&](const auto& entry) { return StyleNameEquals(entry.Key, key); });
	if (position == _resources.end()) return false;
	_resources.erase(position);
	NotifyChanged();
	return true;
}

void ControlStyleSheet::ClearResources()
{
	if (_compiledProgram || _resources.empty()) return;
	_resources.clear();
	NotifyChanged();
}

bool ControlStyleSheet::TryGetResource(
	const std::wstring& key,
	BindingValue& value) const
{
	if (_compiledProgram) return TryGetCompiledResource(key, value);
	if (!_resourceIndexBuilt)
	{
		_resourceIndex.clear();
		_resourceIndex.reserve(_resources.size());
		for (size_t index = 0; index < _resources.size(); ++index)
			_resourceIndex[_resources[index].Key] = index;
		_resourceIndexBuilt = true;
	}
	const auto existing = _resourceIndex.find(key);
	if (existing == _resourceIndex.end()
		|| existing->second >= _resources.size()) return false;
	value = _resources[existing->second].Value;
	return true;
}

ControlStyleResolution ControlStyleSheet::Resolve(
	Control& target,
	bool themeStyle) const
{
	if (_compiledProgram) return ResolveCompiled(target, themeStyle);

	ControlStyleResolution result;
	struct Winner
	{
		ResolvedControlStyleSetter Setter;
		size_t RuleOrder = 0;
	};
	std::vector<Winner> winners;

	const auto& identity = CandidateRuleIdentity(target, themeStyle);
	result.HasStyle = !identity.RuleIndexes.empty();
	if (!result.HasStyle) return result;

	for (const size_t ruleOrder : identity.RuleIndexes)
	{
		if (ruleOrder >= _rules.size()) continue;
		const auto& rule = _rules[ruleOrder];
		const bool matched = rule.Selector.MatchesConditions(target)
			&& MatchesDataConditions(rule.Selector, target);
		if (!rule.EnterActions.empty() || !rule.ExitActions.empty())
		{
			ResolvedControlStyleTrigger trigger;
			trigger.RuleId = rule.Id;
			trigger.IsActive = matched;
			trigger.EnterActions = rule.EnterActions;
			trigger.ExitActions = rule.ExitActions;
			result.Triggers.push_back(std::move(trigger));
		}
		if (!matched) continue;
		const bool conditional = rule.Selector.IsConditional();
		for (const auto& setter : rule.Setters)
		{
			BindingValue value = setter.Value.Literal;
			const bool foundResource = !setter.Value.IsResource()
				|| (setter.Value.IsDynamicResource
					? target.TryFindResource(setter.Value.ResourceKey, value)
					: TryGetResource(setter.Value.ResourceKey, value));
			if (setter.Value.IsResource() && !foundResource
				&& !setter.Value.IsDynamicResource)
			{
				result.Issues.push_back({
					ControlStyleResolutionIssueCode::MissingResource,
					rule.Id, setter.Property.Name(), setter.Value.ResourceKey });
				continue;
			}
			const auto* metadata = FindStylePropertyMetadata(
				target, setter.Property);
			if (!metadata)
			{
				result.Issues.push_back({
					ControlStyleResolutionIssueCode::PropertyNotFound,
					rule.Id, setter.Property.Name(), {} });
				continue;
			}
			if (!metadata->CanWrite())
			{
				result.Issues.push_back({
					ControlStyleResolutionIssueCode::PropertyNotWritable,
					rule.Id, setter.Property.Name(), {} });
				continue;
			}
			BindingValue converted;
			if ((!setter.Value.IsDynamicResource || foundResource)
				&& !metadata->TryConvert(value, converted))
			{
				result.Issues.push_back({
					ControlStyleResolutionIssueCode::InvalidValue,
					rule.Id, setter.Property.Name(), {} });
				continue;
			}

			const DependencyPropertyReference property(metadata->Property());
			auto winner = std::find_if(
				winners.begin(), winners.end(),
				[&](const auto& current)
				{ return current.Setter.Property.Matches(property); });
			ResolvedControlStyleSetter candidate;
			candidate.Property = property;
			if (!setter.Value.IsDynamicResource || foundResource)
				candidate.Value = std::move(converted);
			candidate.ResourceKey = setter.Value.ResourceKey;
			candidate.IsDynamicResource = setter.Value.IsDynamicResource;
			candidate.RuleId = rule.Id;
			candidate.IsConditional = conditional;
			if (winner == winners.end())
				winners.push_back({ std::move(candidate), ruleOrder });
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
			const auto& leftName = left.Setter.Property.Name();
			const auto& rightName = right.Setter.Property.Name();
			if (leftName != rightName) return leftName < rightName;
			const auto* leftIdentity = left.Setter.Property.Identity();
			const auto* rightIdentity = right.Setter.Property.Identity();
			if (leftIdentity && rightIdentity)
				return leftIdentity->GlobalIndex()
					< rightIdentity->GlobalIndex();
			return leftIdentity != nullptr && rightIdentity == nullptr;
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
	if (_compiledProgram)
		return std::any_of(
			_compiledProgram->GlobalPropertyWatchers.begin(),
			_compiledProgram->GlobalPropertyWatchers.end(),
			[&](const auto& property)
			{ return property.Name() == propertyName; });
	EnsureConditionCaches();
	return std::any_of(
		_propertyConditions.begin(), _propertyConditions.end(),
		[&](const auto& property)
		{ return property.Name() == propertyName; });
}

bool ControlStyleSheet::HasPropertyConditionsFor(
	Control& target,
	bool themeStyle) const
{
	return _compiledProgram
		? CompiledHasPropertyConditionsFor(target, themeStyle)
		: !CandidateRuleIdentity(
			target, themeStyle).PropertyConditions.empty();
}

bool ControlStyleSheet::UsesPropertyCondition(
	Control& target,
	const std::wstring& propertyName,
	bool themeStyle) const
{
	if (propertyName.empty()) return false;
	if (_compiledProgram)
	{
		for (const auto& group : _compiledProgram->Groups)
		{
			if (!CompiledGroupMatches(group, target, themeStyle)) continue;
			const auto [begin, end] = CompiledRange(
				_compiledProgram->PropertyWatchers,
				group.PropertyWatchers);
			if (std::any_of(begin, end, [&](const auto& property)
				{ return property.Name() == propertyName; })) return true;
		}
		return false;
	}
	const auto& properties = CandidateRuleIdentity(
		target, themeStyle).PropertyConditions;
	return std::any_of(properties.begin(), properties.end(),
		[&](const auto& property)
		{ return property.Name() == propertyName; });
}

bool ControlStyleSheet::UsesPropertyCondition(
	Control& target,
	const DependencyPropertyChangedEventArgs& args,
	bool themeStyle) const
{
	if (_compiledProgram)
		return CompiledUsesPropertyCondition(target, args, themeStyle);
	const auto& properties = CandidateRuleIdentity(
		target, themeStyle).PropertyConditions;
	return std::any_of(properties.begin(), properties.end(),
		[&](const auto& property)
		{ return property.Matches(args.Property, args.Name()); });
}

bool ControlStyleSheet::HasDataConditionsFor(
	Control& target,
	bool themeStyle) const
{
	return _compiledProgram
		? CompiledHasDataConditionsFor(target, themeStyle)
		: !CandidateRuleIdentity(
			target, themeStyle).DataConditionPaths.empty();
}

std::vector<std::wstring>
ControlStyleSheet::DataConditionPathsFor(
	Control& target,
	bool themeStyle) const
{
	if (!_compiledProgram)
		return CandidateRuleIdentity(
			target, themeStyle).DataConditionPaths;
	std::vector<std::wstring> result;
	for (const auto& group : _compiledProgram->Groups)
	{
		if (!CompiledGroupMatches(group, target, themeStyle)) continue;
		const auto [begin, end] = CompiledRange(
			_compiledProgram->DataPathWatchers,
			group.DataPathWatchers);
		for (auto reference = begin; reference != end; ++reference)
			if (const auto* path = DynamicDataPath(
					*_compiledProgram, *reference);
				path && !ContainsStyleName(result, *path))
				result.emplace_back(*path);
	}
	return result;
}

const std::vector<std::wstring>&
ControlStyleSheet::DataConditionPaths() const
{
	EnsureConditionCaches();
	return _dataConditionPaths;
}

const ControlStyleSheet::RuleIdentityCacheEntry&
ControlStyleSheet::CandidateRuleIdentity(
	Control& target,
	bool themeStyle) const
{
	RuleIdentityCacheKey key;
	key.NativeType = target.Type();
	key.DeclarativeTypeNamespace = target.GetDeclarativeTypeNamespace();
	key.DeclarativeTypeName = target.GetDeclarativeTypeName();
	key.StyleResourceKey = cui::framework::StyleAccess::ResourceKey(target);
	key.ThemeStyle = themeStyle;

	if (const auto found = _ruleIdentityCache.find(key);
		found != _ruleIdentityCache.end())
		return found->second;

	RuleIdentityCacheEntry entry;
	for (size_t index = 0; index < _rules.size(); ++index)
	{
		const auto& selector = _rules[index].Selector;
		if (!selector.MatchesTargetType(target)) continue;
		const bool identityMatched =
			(themeStyle || key.StyleResourceKey.empty())
				? selector.StyleResourceKey.empty()
					&& (selector.Type.has_value()
						|| !selector.DeclarativeTypeName.empty())
				: StyleNameEquals(
					selector.StyleResourceKey, key.StyleResourceKey);
		if (!identityMatched) continue;
		entry.RuleIndexes.push_back(index);
		for (const auto& condition : selector.PropertyConditions)
			if (!condition.Property.Empty()
				&& !ContainsStyleProperty(
					entry.PropertyConditions, condition.Property))
				entry.PropertyConditions.push_back(condition.Property);
		for (const auto& condition : selector.DataConditions)
			if (!condition.SourceProperty.empty()
				&& !ContainsStyleName(
					entry.DataConditionPaths, condition.SourceProperty))
				entry.DataConditionPaths.push_back(condition.SourceProperty);
	}
	return _ruleIdentityCache.emplace(
		std::move(key), std::move(entry)).first->second;
}

void ControlStyleSheet::EnsureConditionCaches() const
{
	if (_conditionCachesBuilt) return;
	_propertyConditions.clear();
	_dataConditionPaths.clear();
	if (_compiledProgram)
	{
		for (const auto reference : _compiledProgram->GlobalDataPathWatchers)
			if (const auto* path = DynamicDataPath(
					*_compiledProgram, reference);
				path && !ContainsStyleName(_dataConditionPaths, *path))
				_dataConditionPaths.emplace_back(*path);
		_conditionCachesBuilt = true;
		return;
	}
	for (const auto& rule : _rules)
	{
		for (const auto& condition : rule.Selector.PropertyConditions)
			if (!condition.Property.Empty()
				&& !ContainsStyleProperty(
					_propertyConditions, condition.Property))
				_propertyConditions.push_back(condition.Property);
		for (const auto& condition : rule.Selector.DataConditions)
			if (!condition.SourceProperty.empty()
				&& !ContainsStyleName(
					_dataConditionPaths, condition.SourceProperty))
				_dataConditionPaths.push_back(condition.SourceProperty);
	}
	_conditionCachesBuilt = true;
}

EventConnection ControlStyleSheet::SubscribeChanged(
	std::function<void()> handler) const
{
	if (_compiledProgram) return {};
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
		if (!cui::style::design::TryParseDataPathSegments(
				condition.SourceProperty, path)
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
	if (_compiledProgram) return;
	_ruleIdentityCache.clear();
	_conditionCachesBuilt = false;
	_propertyConditions.clear();
	_dataConditionPaths.clear();
	_resourceIndexBuilt = false;
	_resourceIndex.clear();
	++_revision;
	cui::framework::EventAccess::Raise(_changed);
}

bool ControlStyleSheet::TryPopulateDesignTriggerActions(
	const CompiledStyleRuleOp& rule,
	ResolvedControlStyleTrigger& trigger) const
{
	if (!_compiledProgram || !_compiledProgram->UsesDesignActions)
		return false;
	const auto [enterBegin, enterEnd] = CompiledRange(
		_compiledProgram->DesignActions, rule.EnterActions);
	const auto [exitBegin, exitEnd] = CompiledRange(
		_compiledProgram->DesignActions, rule.ExitActions);
	trigger.EnterActions.assign(enterBegin, enterEnd);
	trigger.ExitActions.assign(exitBegin, exitEnd);
	return true;
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
	return metadata && TrySetDynamicResourceExpressionOwned(
		*metadata, std::move(resourceKey), source);
}

bool Control::ClearDynamicResource(const std::wstring& propertyName)
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
		|| entry->second.Slots[static_cast<size_t>(index)].Expression
			!= DependencyPropertyExpressionKind::DynamicResource) return false;
	return ClearPropertyValueOwned(*metadata, source, nullptr);
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
	const auto& slot = entry->second.Slots[static_cast<size_t>(index)];
	if (slot.Expression != DependencyPropertyExpressionKind::DynamicResource)
		return false;
	resourceKey = slot.ResourceKey;
	return true;
}
