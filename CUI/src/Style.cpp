#include "Style.h"
#include "Application.h"
#include "BindingList.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"
#include <algorithm>
#include <cmath>

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
			[&value](const auto& current)
			{
				return StyleNameEquals(current, value);
			});
	}

	bool ContainsStyleProperty(
		const std::vector<DependencyPropertyReference>& values,
		const DependencyPropertyReference& value) noexcept
	{
		return std::any_of(values.begin(), values.end(),
			[&value](const auto& current)
			{
				return current.Matches(value);
			});
	}

	template<typename TRange>
	auto
	CompiledRange(
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

	const CompiledBindingPathView* CompiledDataPath(
		const CompiledStyleProgramView& program,
		uint32_t reference) noexcept
	{
		if (reference == CompiledStyleInvalidIndex
			|| IsCompiledStyleDynamicDataPathReference(reference)) return nullptr;
		const auto index = CompiledStyleDataPathIndex(reference);
		return index < program.DataPaths.size()
			? &program.DataPaths[index] : nullptr;
	}

	bool ValidCompiledDataPath(
		CompiledBindingPathView path,
		bool requireObserve) noexcept
	{
		if (path.Version != CompiledBindingPathVersion
			|| path.Steps.empty() || path.Steps.data() == nullptr) return false;
		for (const auto& step : path.Steps)
		{
			if (static_cast<uint8_t>(step.Kind)
					> static_cast<uint8_t>(CompiledBindingPathStepKind::ListIndex)
				|| static_cast<uint8_t>(step.ValueKind)
					> static_cast<uint8_t>(BindingValueKind::NullableBool)
				|| !HasCompiledBindingPathCapability(
					step.Capabilities, CompiledBindingPathCapabilities::Read))
				return false;
			if (step.Kind == CompiledBindingPathStepKind::Property)
			{
				if (!step.Property
					|| (requireObserve
						&& !HasCompiledBindingPathCapability(
							step.Capabilities,
							CompiledBindingPathCapabilities::Observe)))
					return false;
			}
			else if (step.Property || step.ValueKind != BindingValueKind::Object)
			{
				return false;
			}
		}
		return true;
	}

	bool ValidCompiledDataPathReference(
		const CompiledStyleProgramView& program,
		uint32_t reference,
		bool requireObserve) noexcept
	{
		if (const auto* path = CompiledDataPath(program, reference))
			return ValidCompiledDataPath(*path, requireObserve);
#if CUI_ENABLE_DYNAMIC_XAML
		return cui::style::design::ValidateDynamicDataPathReference(
			program, reference, requireObserve);
#else
		return false;
#endif
	}

	template<typename TValue>
	bool TryReadCompiledTypedValue(
		const CompiledStyleValuePoolView& pool,
		uint32_t index,
		BindingValue& value)
	{
		if (!pool.Data || index >= pool.Count) return false;
		value = BindingValue(static_cast<const TValue*>(pool.Data)[index]);
		return true;
	}

	template<typename TProgram>
	bool TryReadCompiledValue(
		const TProgram& program,
		uint32_t reference,
		BindingValue& value)
	{
		if (!IsCompiledStyleStaticValueReference(reference))
		{
			if (reference >= program.Values.size()) return false;
			value = program.Values[reference];
			return true;
		}

		const uint32_t poolIndex =
			CompiledStyleStaticValuePoolIndex(reference);
		if (poolIndex >= program.ValuePools.size()) return false;
		const auto& pool = program.ValuePools[poolIndex];
		const uint32_t elementIndex =
			CompiledStyleStaticValueElementIndex(reference);
		switch (pool.Kind)
		{
		case CompiledStyleValuePoolKind::Bool:
			return TryReadCompiledTypedValue<bool>(pool, elementIndex, value);
		case CompiledStyleValuePoolKind::NullableBool:
			return TryReadCompiledTypedValue<NullableBool>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Int:
			return TryReadCompiledTypedValue<int>(pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Int64:
			return TryReadCompiledTypedValue<long long>(pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Float:
			return TryReadCompiledTypedValue<float>(pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Double:
			return TryReadCompiledTypedValue<double>(pool, elementIndex, value);
		case CompiledStyleValuePoolKind::String:
		{
			if (!pool.Data || elementIndex >= pool.Count) return false;
			const auto text =
				static_cast<const std::wstring_view*>(pool.Data)[elementIndex];
			value = BindingValue(std::wstring(text));
			return true;
		}
		case CompiledStyleValuePoolKind::Color:
			return TryReadCompiledTypedValue<D2D1_COLOR_F>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Thickness:
			return TryReadCompiledTypedValue<Thickness>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::CornerRadius:
			return TryReadCompiledTypedValue<CornerRadius>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Point:
			return TryReadCompiledTypedValue<cui::core::Point>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Vector:
			return TryReadCompiledTypedValue<cui::core::Vector>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Rect:
			return TryReadCompiledTypedValue<cui::core::Rect>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Size:
			return TryReadCompiledTypedValue<cui::core::Size>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Matrix:
			return TryReadCompiledTypedValue<D2D1_MATRIX_3X2_F>(
				pool, elementIndex, value);
		case CompiledStyleValuePoolKind::Length:
			return TryReadCompiledTypedValue<cui::layout::Length>(
				pool, elementIndex, value);
		}
		return false;
	}

	const DependencyPropertyMetadata* FindStylePropertyMetadata(
		Control& target,
		const DependencyPropertyReference& property)
	{
		if (const auto* identity = property.Identity())
			return target.GetPropertyMetadata(*identity);
#if CUI_ENABLE_DYNAMIC_XAML
		return cui::style::design::FindNamedPropertyMetadata(
			target, property.Name());
#else
		return nullptr;
#endif
	}

	bool ValidCompiledStyleRange(
		CompiledStyleRange range, size_t size) noexcept
	{
		return range.Offset <= size
			&& range.Count <= size - range.Offset;
	}

	bool ValidCompiledInteractionRange(
		CompiledInteractionRange range, size_t size) noexcept
	{
		return range.Offset <= size
			&& range.Count <= size - range.Offset;
	}

	template<typename TValue>
	bool ValidCompiledSpan(std::span<const TValue> values) noexcept
	{
		return values.empty() || values.data() != nullptr;
	}

	bool ValidCompiledStyleValueReference(
		const CompiledStyleProgramView& program,
		std::span<const BindingValue> values,
		uint32_t reference) noexcept
	{
		if (reference == CompiledStyleInvalidIndex) return false;
		if (!IsCompiledStyleStaticValueReference(reference))
			return reference < values.size();
		const auto poolIndex = CompiledStyleStaticValuePoolIndex(reference);
		if (poolIndex >= program.ValuePools.size()) return false;
		const auto& pool = program.ValuePools[poolIndex];
		return static_cast<uint8_t>(pool.Kind)
			<= static_cast<uint8_t>(CompiledStyleValuePoolKind::Length)
			&& pool.Data != nullptr
			&& CompiledStyleStaticValueElementIndex(reference) < pool.Count;
	}

	bool ValidCompiledAnimationKind(DeclarativeAnimationKind value) noexcept
	{
		return static_cast<uint8_t>(value)
			<= static_cast<uint8_t>(DeclarativeAnimationKind::Object);
	}

	bool ValidCompiledKeyFrameKind(DeclarativeKeyFrameKind value) noexcept
	{
		return static_cast<uint8_t>(value)
			<= static_cast<uint8_t>(DeclarativeKeyFrameKind::Spline);
	}

	bool ValidCompiledEasingKind(DeclarativeEasingKind value) noexcept
	{
		return static_cast<uint8_t>(value)
			<= static_cast<uint8_t>(DeclarativeEasingKind::Sine);
	}

	bool ValidCompiledEasingMode(DeclarativeEasingMode value) noexcept
	{
		return static_cast<uint8_t>(value)
			<= static_cast<uint8_t>(DeclarativeEasingMode::EaseInOut);
	}

	bool ValidCompiledActionKind(
		DeclarativeStoryboardActionKind value) noexcept
	{
		return static_cast<uint8_t>(value)
			<= static_cast<uint8_t>(DeclarativeStoryboardActionKind::Stop);
	}

	bool ValidCompiledObjectPath(
		const CompiledStyleProgramView& program,
		const CompiledStoryboardObjectPathOp& path) noexcept
	{
		constexpr auto knownFlags =
			static_cast<uint8_t>(
				CompiledStoryboardObjectPathFlags::RelativeTransform)
			| static_cast<uint8_t>(
				CompiledStoryboardObjectPathFlags::HasPathSegment);
		return static_cast<uint8_t>(path.Kind)
				<= static_cast<uint8_t>(
					CompiledStoryboardObjectPathKind::BrushTransform)
			&& static_cast<uint8_t>(path.Member)
				<= static_cast<uint8_t>(
					CompiledStoryboardObjectPathMember::BrushGradientStopOffset)
			&& (static_cast<uint8_t>(path.Flags) & ~knownFlags) == 0
			&& path.Reserved == 0
			&& path.Identity != 0
			&& ValidCompiledInteractionRange(
				path.ChildIndices, program.ObjectPathChildIndices.size());
	}

	bool ValidateCompiledStyleProgram(
		const CompiledStyleProgramView& program,
		std::span<const BindingValue> values,
		size_t actionCount,
		bool usesFlatActions)
	{
		if (program.Version != CompiledStyleProgramViewVersion) return false;
		const bool spansValid = ValidCompiledSpan(program.Strings)
			&& ValidCompiledSpan(program.ValuePools)
			&& ValidCompiledSpan(program.Resources)
			&& ValidCompiledSpan(program.ResourceLookup)
			&& ValidCompiledSpan(program.PropertyConditions)
			&& ValidCompiledSpan(program.DataConditions)
			&& ValidCompiledSpan(program.Setters)
			&& ValidCompiledSpan(program.PropertyOperands)
			&& ValidCompiledSpan(program.ObjectPathChildIndices)
			&& ValidCompiledSpan(program.ObjectPaths)
			&& ValidCompiledSpan(program.KeyFrames)
			&& ValidCompiledSpan(program.Animations)
			&& ValidCompiledSpan(program.Storyboards)
			&& ValidCompiledSpan(program.Actions)
			&& ValidCompiledSpan(program.Rules)
			&& ValidCompiledSpan(program.RuleIndexes)
			&& ValidCompiledSpan(program.PropertyWatchers)
			&& ValidCompiledSpan(program.DataPathWatchers)
			&& ValidCompiledSpan(program.Groups)
			&& ValidCompiledSpan(program.GlobalPropertyWatchers)
			&& ValidCompiledSpan(program.GlobalDataPathWatchers)
			&& ValidCompiledSpan(program.DataPaths);
		if (!spansValid) return false;

		for (const auto& pool : program.ValuePools)
			if (static_cast<uint8_t>(pool.Kind)
					> static_cast<uint8_t>(CompiledStyleValuePoolKind::Length)
				|| (pool.Count != 0 && pool.Data == nullptr)) return false;

		if (program.ResourceLookup.size() != program.Resources.size())
			return false;
		const std::wstring_view* previousResourceKey = nullptr;
		for (const auto resourceIndex : program.ResourceLookup)
		{
			if (resourceIndex >= program.Resources.size()) return false;
			const auto& resource = program.Resources[resourceIndex];
			const auto* key = CompiledString(program, resource.KeyStringIndex);
			if (!key || key->empty()
				|| !ValidCompiledStyleValueReference(
					program, values, resource.ValueIndex)
				|| (previousResourceKey && !(*previousResourceKey < *key)))
				return false;
			previousResourceKey = key;
		}

		for (const auto& condition : program.PropertyConditions)
			if (!condition.Property.Identity()
				|| !ValidCompiledStyleValueReference(
					program, values, condition.ValueIndex)) return false;
		for (const auto path : program.DataPaths)
			if (!ValidCompiledDataPath(path, false)) return false;
		for (const auto& condition : program.DataConditions)
		{
			if (!ValidCompiledDataPathReference(
					program, condition.PathReference, false)
				|| !ValidCompiledStyleValueReference(
					program, values, condition.ValueIndex)) return false;
		}
		for (const auto& setter : program.Setters)
		{
			if (!setter.Property.Identity()
				|| static_cast<uint8_t>(setter.Value.Kind)
					> static_cast<uint8_t>(
						CompiledStyleOperandKind::DynamicResource)) return false;
			if (setter.Value.Kind == CompiledStyleOperandKind::Literal)
			{
				if (!ValidCompiledStyleValueReference(
					program, values, setter.Value.Index)) return false;
			}
			else
			{
				const auto* key = CompiledString(program, setter.Value.Index);
				if (!key || key->empty()) return false;
			}
		}

		if (!usesFlatActions
			&& (!program.PropertyOperands.empty()
				|| !program.ObjectPathChildIndices.empty()
				|| !program.ObjectPaths.empty()
				|| !program.KeyFrames.empty()
				|| !program.Animations.empty()
				|| !program.Storyboards.empty()
				|| !program.Actions.empty())) return false;
		if (usesFlatActions)
		{
			if (actionCount != program.Actions.size()) return false;
			for (const auto& operand : program.PropertyOperands)
				if (operand.TargetSlot != 0 || !operand.Property.Identity())
					return false;
			for (const auto& path : program.ObjectPaths)
				if (!ValidCompiledObjectPath(program, path)) return false;
			for (const auto& keyFrame : program.KeyFrames)
			{
				if (!ValidCompiledKeyFrameKind(keyFrame.Kind)
					|| !ValidCompiledEasingKind(keyFrame.Easing)
					|| !ValidCompiledEasingMode(keyFrame.EasingMode)
					|| keyFrame.ValueIndex >= values.size()) return false;
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Spline
					&& (!std::isfinite(keyFrame.KeySplineX1)
						|| !std::isfinite(keyFrame.KeySplineY1)
						|| !std::isfinite(keyFrame.KeySplineX2)
						|| !std::isfinite(keyFrame.KeySplineY2)
						|| keyFrame.KeySplineX1 < 0.0f
						|| keyFrame.KeySplineX1 > 1.0f
						|| keyFrame.KeySplineY1 < 0.0f
						|| keyFrame.KeySplineY1 > 1.0f
						|| keyFrame.KeySplineX2 < 0.0f
						|| keyFrame.KeySplineX2 > 1.0f
						|| keyFrame.KeySplineY2 < 0.0f
						|| keyFrame.KeySplineY2 > 1.0f)) return false;
			}
			std::vector<uint32_t> keyFrameOwners(
				program.KeyFrames.size(), CompiledStyleInvalidIndex);
			for (size_t animationIndex = 0;
				animationIndex < program.Animations.size(); ++animationIndex)
			{
				const auto& animation = program.Animations[animationIndex];
				auto validOptionalValue = [&](uint32_t index)
					{
						return index == CompiledInteractionInvalidIndex
							|| index < values.size();
					};
				if (!ValidCompiledAnimationKind(animation.Kind)
					|| !ValidCompiledEasingKind(animation.Easing)
					|| !ValidCompiledEasingMode(animation.EasingMode)
					|| animation.OperandIndex >= program.PropertyOperands.size()
					|| (animation.ObjectPathIndex
						!= CompiledInteractionInvalidIndex
						&& animation.ObjectPathIndex >= program.ObjectPaths.size())
					|| !validOptionalValue(animation.FromValueIndex)
					|| !validOptionalValue(animation.ToValueIndex)
					|| !validOptionalValue(animation.ByValueIndex)
					|| !ValidCompiledInteractionRange(
						animation.KeyFrames, program.KeyFrames.size())
					|| static_cast<uint8_t>(animation.RepeatBehavior)
						> static_cast<uint8_t>(
							DeclarativeRepeatBehaviorKind::Forever)
					|| static_cast<uint8_t>(animation.FillBehavior)
						> static_cast<uint8_t>(
							DeclarativeTimelineFillBehavior::Stop)
					|| !std::isfinite(animation.RepeatCount)
					|| !std::isfinite(animation.SpeedRatio)
					|| !std::isfinite(animation.AccelerationRatio)
					|| !std::isfinite(animation.DecelerationRatio)
					|| animation.SpeedRatio <= 0.0
					|| animation.AccelerationRatio < 0.0
					|| animation.AccelerationRatio > 1.0
					|| animation.DecelerationRatio < 0.0
					|| animation.DecelerationRatio > 1.0
					|| animation.AccelerationRatio
						+ animation.DecelerationRatio > 1.0) return false;
				if (animation.RepeatBehavior
						== DeclarativeRepeatBehaviorKind::Count
					&& animation.RepeatCount <= 0.0) return false;
				if (animation.RepeatBehavior
						== DeclarativeRepeatBehaviorKind::Duration
					&& animation.RepeatDurationMilliseconds == 0) return false;
				const bool hasKeyFrames = animation.KeyFrames.Count != 0;
				for (uint32_t offset = 0;
					offset < animation.KeyFrames.Count; ++offset)
				{
					auto& owner = keyFrameOwners[
						animation.KeyFrames.Offset + offset];
					if (owner != CompiledStyleInvalidIndex) return false;
					owner = static_cast<uint32_t>(animationIndex);
				}
				if (hasKeyFrames
					&& (animation.FromValueIndex != CompiledInteractionInvalidIndex
						|| animation.ToValueIndex != CompiledInteractionInvalidIndex
						|| animation.ByValueIndex
							!= CompiledInteractionInvalidIndex)) return false;
				if (animation.Kind == DeclarativeAnimationKind::Object)
				{
					if (!hasKeyFrames || animation.IsAdditive
						|| animation.IsCumulative
						|| animation.Easing != DeclarativeEasingKind::Linear)
						return false;
					for (uint32_t offset = 0;
						offset < animation.KeyFrames.Count; ++offset)
						if (program.KeyFrames[
							animation.KeyFrames.Offset + offset].Kind
							!= DeclarativeKeyFrameKind::Discrete) return false;
				}
			}
			if (std::any_of(keyFrameOwners.begin(), keyFrameOwners.end(),
				[](uint32_t owner)
				{ return owner == CompiledStyleInvalidIndex; })) return false;
			std::vector<uint32_t> animationOwners(
				program.Animations.size(), CompiledStyleInvalidIndex);
			for (size_t storyboardIndex = 0;
				storyboardIndex < program.Storyboards.size(); ++storyboardIndex)
			{
				const auto& storyboard = program.Storyboards[storyboardIndex];
				if (!ValidCompiledInteractionRange(
						storyboard.Animations, program.Animations.size())
					|| storyboard.Animations.Count == 0) return false;
				for (uint32_t leftOffset = 0;
					leftOffset < storyboard.Animations.Count; ++leftOffset)
				{
					auto& animationOwner = animationOwners[
						storyboard.Animations.Offset + leftOffset];
					if (animationOwner != CompiledStyleInvalidIndex) return false;
					animationOwner = static_cast<uint32_t>(storyboardIndex);
					const auto& left = program.Animations[
						storyboard.Animations.Offset + leftOffset];
					const auto& leftOperand =
						program.PropertyOperands[left.OperandIndex];
					const uint64_t leftPath = left.ObjectPathIndex
						== CompiledInteractionInvalidIndex ? 0
						: program.ObjectPaths[left.ObjectPathIndex].Identity;
					for (uint32_t rightOffset = 0;
						rightOffset < leftOffset; ++rightOffset)
					{
						const auto& right = program.Animations[
							storyboard.Animations.Offset + rightOffset];
						const auto& rightOperand =
							program.PropertyOperands[right.OperandIndex];
						if (leftOperand.Property.Identity()
							!= rightOperand.Property.Identity()) continue;
						const uint64_t rightPath = right.ObjectPathIndex
							== CompiledInteractionInvalidIndex ? 0
							: program.ObjectPaths[right.ObjectPathIndex].Identity;
						if (leftPath == 0 || rightPath == 0
							|| leftPath == rightPath) return false;
					}
				}
			}
			if (std::any_of(animationOwners.begin(), animationOwners.end(),
				[](uint32_t owner)
				{ return owner == CompiledStyleInvalidIndex; })) return false;
			for (const auto& action : program.Actions)
				if (!ValidCompiledActionKind(action.Kind)
					|| action.StoryboardIndex >= program.Storyboards.size())
					return false;
		}

		std::vector<uint32_t> actionOwners(
			actionCount, CompiledStyleInvalidIndex);
		std::vector<uint32_t> storyboardOwners;
		std::vector<uint32_t> storyboardBeginCounts;
		if (usesFlatActions)
		{
			storyboardOwners.assign(
				program.Storyboards.size(), CompiledStyleInvalidIndex);
			storyboardBeginCounts.assign(program.Storyboards.size(), 0);
		}
		for (size_t ruleIndex = 0;
			ruleIndex < program.Rules.size(); ++ruleIndex)
		{
			const auto& rule = program.Rules[ruleIndex];
			if (rule.RuleId == 0
				|| !ValidCompiledStyleRange(
					rule.PropertyConditions, program.PropertyConditions.size())
				|| !ValidCompiledStyleRange(
					rule.DataConditions, program.DataConditions.size())
				|| !ValidCompiledStyleRange(rule.Setters, program.Setters.size())
				|| !ValidCompiledStyleRange(rule.EnterActions, actionCount)
				|| !ValidCompiledStyleRange(rule.ExitActions, actionCount))
				return false;
			for (size_t previous = 0; previous < ruleIndex; ++previous)
				if (program.Rules[previous].RuleId == rule.RuleId) return false;
			for (const auto range : { rule.EnterActions, rule.ExitActions })
				{
					for (uint32_t offset = 0; offset < range.Count; ++offset)
					{
						const size_t actionIndex = range.Offset + offset;
						auto& actionOwner = actionOwners[actionIndex];
						if (actionOwner != CompiledStyleInvalidIndex) return false;
						actionOwner = static_cast<uint32_t>(ruleIndex);
						if (!usesFlatActions) continue;
						const auto& action = program.Actions[actionIndex];
						auto& storyboardOwner =
							storyboardOwners[action.StoryboardIndex];
						if (storyboardOwner == CompiledStyleInvalidIndex)
							storyboardOwner = static_cast<uint32_t>(ruleIndex);
						else if (storyboardOwner != ruleIndex) return false;
						if (action.Kind
							== DeclarativeStoryboardActionKind::Begin)
							++storyboardBeginCounts[action.StoryboardIndex];
					}
				}
		}
		if (std::any_of(actionOwners.begin(), actionOwners.end(),
			[](uint32_t owner)
			{ return owner == CompiledStyleInvalidIndex; })) return false;
		if (usesFlatActions)
		{
			for (size_t index = 0; index < storyboardOwners.size(); ++index)
				if (storyboardOwners[index] == CompiledStyleInvalidIndex
					|| storyboardBeginCounts[index] != 1) return false;
		}

		std::vector<uint32_t> ruleOwners(
			program.Rules.size(), CompiledStyleInvalidIndex);
		for (size_t index = 0; index < program.RuleIndexes.size(); ++index)
		{
			const auto ruleIndex = program.RuleIndexes[index];
			if (ruleIndex >= program.Rules.size()
				|| ruleOwners[ruleIndex] != CompiledStyleInvalidIndex)
				return false;
			ruleOwners[ruleIndex] = static_cast<uint32_t>(index);
		}
		if (std::any_of(ruleOwners.begin(), ruleOwners.end(),
			[](uint32_t owner)
			{ return owner == CompiledStyleInvalidIndex; })) return false;
		for (const auto& property : program.PropertyWatchers)
			if (!property.Identity()) return false;
		for (const auto pathIndex : program.DataPathWatchers)
			if (!ValidCompiledDataPathReference(
				program, pathIndex, true)) return false;
		std::vector<uint32_t> ruleIndexSlotOwners(
			program.RuleIndexes.size(), CompiledStyleInvalidIndex);
		for (size_t groupIndex = 0;
			groupIndex < program.Groups.size(); ++groupIndex)
		{
			const auto& group = program.Groups[groupIndex];
			const auto typeValue = static_cast<int>(group.Type);
			if (typeValue < static_cast<int>(UIClass::UI_Base)
				|| typeValue > static_cast<int>(UIClass::UI_CUSTOM)
				|| (group.StyleResourceKey != CompiledStyleInvalidIndex
					&& group.StyleResourceKey >= program.Strings.size())
				|| !ValidCompiledStyleRange(
					group.RuleIndexes, program.RuleIndexes.size())
				|| !ValidCompiledStyleRange(
					group.PropertyWatchers, program.PropertyWatchers.size())
				|| !ValidCompiledStyleRange(
					group.DataPathWatchers, program.DataPathWatchers.size()))
				return false;
			for (uint32_t offset = 0; offset < group.RuleIndexes.Count; ++offset)
			{
				auto& owner = ruleIndexSlotOwners[
					group.RuleIndexes.Offset + offset];
				if (owner != CompiledStyleInvalidIndex) return false;
				owner = static_cast<uint32_t>(groupIndex);
			}
		}
		if (std::any_of(ruleIndexSlotOwners.begin(), ruleIndexSlotOwners.end(),
			[](uint32_t owner)
			{ return owner == CompiledStyleInvalidIndex; })) return false;
		for (const auto& property : program.GlobalPropertyWatchers)
			if (!property.Identity()) return false;
		for (const auto pathIndex : program.GlobalDataPathWatchers)
			if (!ValidCompiledDataPathReference(
				program, pathIndex, true)) return false;
		return true;
	}
}

ControlStyleSheet::ControlStyleSheet() = default;
ControlStyleSheet::~ControlStyleSheet() = default;

std::shared_ptr<const ControlStyleSheet>
ControlStyleSheet::CreateCompiled(
	CompiledStyleProgramView program,
	std::vector<BindingValue> values)
{
	return CreateCompiledCore(
		program, std::move(values), program.Actions.size(), true);
}

std::shared_ptr<ControlStyleSheet>
ControlStyleSheet::CreateCompiledCore(
	CompiledStyleProgramView program,
	std::vector<BindingValue> values,
	size_t actionCount,
	bool usesFlatActions)
{
	if (!ValidateCompiledStyleProgram(
		program, values, actionCount, usesFlatActions)) return {};
	auto sheet = std::shared_ptr<ControlStyleSheet>(new ControlStyleSheet());
	sheet->_compiledProgram = std::make_unique<CompiledStyleInstance>();
	static_cast<CompiledStyleProgramView&>(*sheet->_compiledProgram) = program;
	sheet->_compiledProgram->Values = std::move(values);
	return sheet;
}

bool ControlStyleSheet::TryGetCompiledResource(
	const std::wstring& key,
	BindingValue& value) const
{
	if (!_compiledProgram) return false;
	const auto& program = *_compiledProgram;
	size_t first = 0;
	size_t last = program.ResourceLookup.size();
	while (first < last)
	{
		const size_t middle = first + (last - first) / 2;
		const uint32_t resourceIndex = program.ResourceLookup[middle];
		if (resourceIndex >= program.Resources.size()) return false;
		const auto& resource = program.Resources[resourceIndex];
		const auto* resourceKey = CompiledString(
			program, resource.KeyStringIndex);
		if (!resourceKey) return false;
		if (*resourceKey < key) first = middle + 1;
		else last = middle;
	}
	if (first >= program.ResourceLookup.size()) return false;
	const uint32_t resourceIndex = program.ResourceLookup[first];
	if (resourceIndex >= program.Resources.size()) return false;
	const auto& resource = program.Resources[resourceIndex];
	const auto* resourceKey = CompiledString(
		program, resource.KeyStringIndex);
	BindingValue resourceValue;
	if (!resourceKey
		|| !TryReadCompiledValue(program, resource.ValueIndex, resourceValue)
		|| !StyleNameEquals(*resourceKey, key)) return false;
	value = std::move(resourceValue);
	return true;
}

#if !CUI_ENABLE_DYNAMIC_XAML
size_t ControlStyleSheet::RuleCount() const noexcept
{
	return _compiledProgram ? _compiledProgram->Rules.size() : 0;
}

bool ControlStyleSheet::TryGetResource(
	const std::wstring& key,
	BindingValue& value) const
{
	return TryGetCompiledResource(key, value);
}
#endif

#if !CUI_ENABLE_DYNAMIC_XAML
ControlStyleResolution ControlStyleSheet::Resolve(
	Control& target,
	bool themeStyle) const
{
	return ResolveCompiled(target, themeStyle);
}
#endif

bool ControlStyleSheet::CompiledHasPropertyConditionsFor(
	Control& target,
	bool themeStyle) const
{
	if (!_compiledProgram) return false;
	for (const auto& group : _compiledProgram->Groups)
	{
		if (!CompiledGroupMatches(group, target, themeStyle)) continue;
		const auto [begin, end] = CompiledRange(
			_compiledProgram->PropertyWatchers,
			group.PropertyWatchers);
		if (begin != end) return true;
	}
	return false;
}

#if !CUI_ENABLE_DYNAMIC_XAML
bool ControlStyleSheet::HasPropertyConditionsFor(
	Control& target,
	bool themeStyle) const
{
	return CompiledHasPropertyConditionsFor(target, themeStyle);
}
#endif

bool ControlStyleSheet::CompiledUsesPropertyCondition(
	Control& target,
	const DependencyPropertyChangedEventArgs& args,
	bool themeStyle) const
{
	if (!_compiledProgram) return false;
	for (const auto& group : _compiledProgram->Groups)
	{
		if (!CompiledGroupMatches(group, target, themeStyle)) continue;
		const auto [begin, end] = CompiledRange(
			_compiledProgram->PropertyWatchers,
			group.PropertyWatchers);
		if (std::any_of(begin, end, [&](const auto& property)
			{
				return args.Property
					&& property.Identity() == args.Property;
			})) return true;
	}
	return false;
}

#if !CUI_ENABLE_DYNAMIC_XAML
bool ControlStyleSheet::UsesPropertyCondition(
	Control& target,
	const DependencyPropertyChangedEventArgs& args,
	bool themeStyle) const
{
	return CompiledUsesPropertyCondition(target, args, themeStyle);
}
#endif

bool ControlStyleSheet::CompiledHasDataConditionsFor(
	Control& target,
	bool themeStyle) const
{
	if (!_compiledProgram) return false;
	for (const auto& group : _compiledProgram->Groups)
	{
		if (!CompiledGroupMatches(group, target, themeStyle)) continue;
		const auto [begin, end] = CompiledRange(
			_compiledProgram->DataPathWatchers,
			group.DataPathWatchers);
		if (begin != end) return true;
	}
	return false;
}

#if !CUI_ENABLE_DYNAMIC_XAML
bool ControlStyleSheet::HasDataConditionsFor(
	Control& target,
	bool themeStyle) const
{
	return CompiledHasDataConditionsFor(target, themeStyle);
}
#endif

std::vector<CompiledBindingPathView>
ControlStyleSheet::CompiledDataConditionPaths() const
{
	std::vector<CompiledBindingPathView> result;
	if (!_compiledProgram) return result;
	result.reserve(_compiledProgram->GlobalDataPathWatchers.size());
	for (const auto reference : _compiledProgram->GlobalDataPathWatchers)
	{
		const auto* path = CompiledDataPath(*_compiledProgram, reference);
		if (!path) continue;
		const auto duplicate = std::any_of(
			result.begin(), result.end(), [&](const auto existing)
			{
				return existing.Version == path->Version
					&& existing.Steps.size() == path->Steps.size()
					&& std::equal(existing.Steps.begin(), existing.Steps.end(),
						path->Steps.begin());
			});
		if (!duplicate) result.push_back(*path);
	}
	return result;
}

void Control::SetStyleResourceKey(
	std::wstring value,
	bool capturedFromTheme,
	bool automatic)
{
	if (StyleNameEquals(_styleResourceKey, value)
		&& _styleResourceKeyCapturedFromTheme == capturedFromTheme
		&& _styleResourceKeyIsAutomatic == automatic) return;
	_styleResourceKey = std::move(value);
	_styleResourceKeyCapturedFromTheme = capturedFromTheme;
	_styleResourceKeyIsAutomatic = automatic;
	RefreshStyleValues(false);
}

bool Control::HasVisibleStyleRules() const noexcept
{
	if (_themeStyleSheet && _themeStyleSheet->HasRules()) return true;
	if (_styleSheet && _styleSheet->HasRules()) return true;
	if (const auto* application = Application::Current())
	{
		const auto resources = application->GetResourcesSnapshot();
		if (resources && resources->HasRules()) return true;
	}
	for (const Control* scope = this; scope;
		scope = scope->GetInheritanceParent())
		if (scope->_resourceDictionary
			&& scope->_resourceDictionary->HasRules())
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
#if CUI_ENABLE_DYNAMIC_XAML
	_themeStyleConnection.Disconnect();
#endif
	_themeStyleSheet = std::move(value);
#if CUI_ENABLE_DYNAMIC_XAML
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
#endif
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
#if CUI_ENABLE_DYNAMIC_XAML
	_styleSheetConnection.Disconnect();
#endif
	_styleSheet = std::move(value);
#if CUI_ENABLE_DYNAMIC_XAML
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
#endif
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

bool ControlStyleSheet::CompiledGroupMatches(
	const CompiledStyleGroupOp& group,
	Control& target,
	bool themeStyle) const
{
	if (!_compiledProgram) return false;
	const auto& program = *_compiledProgram;
	if (group.HasType
		&& group.Type != UIClass::UI_Base
		&& target.Type() != group.Type) return false;
	if (group.ComponentType
		&& target.GetDeclarativeTypeToken() != group.ComponentType) return false;

	const auto* styleResourceKey = CompiledString(
		program, group.StyleResourceKey);
	const auto& targetResourceKey =
		cui::framework::StyleAccess::ResourceKey(target);
	if (themeStyle || targetResourceKey.empty())
		return (!styleResourceKey || styleResourceKey->empty())
			&& (group.HasType
				|| static_cast<bool>(group.ComponentType));
	return styleResourceKey
		&& StyleNameEquals(*styleResourceKey, targetResourceKey);
}

ControlStyleResolution ControlStyleSheet::ResolveCompiled(
	Control& target,
	bool themeStyle) const
{
	ControlStyleResolution result;
	if (!_compiledProgram) return result;
	const auto& program = *_compiledProgram;
	struct Winner
	{
		ResolvedControlStyleSetter Setter;
		uint32_t SourceOrder = 0;
	};
	std::vector<Winner> winners;

	for (const auto& group : program.Groups)
	{
		if (!CompiledGroupMatches(group, target, themeStyle)) continue;
		const auto [ruleIndexBegin, ruleIndexEnd] =
			CompiledRange(program.RuleIndexes, group.RuleIndexes);
		for (auto ruleIndex = ruleIndexBegin;
			ruleIndex != ruleIndexEnd; ++ruleIndex)
		{
			if (*ruleIndex >= program.Rules.size()) continue;
			const auto& rule = program.Rules[*ruleIndex];
			result.HasStyle = true;

			bool matched = true;
			const auto [propertyBegin, propertyEnd] = CompiledRange(
				program.PropertyConditions, rule.PropertyConditions);
			for (auto condition = propertyBegin;
				condition != propertyEnd; ++condition)
			{
				const auto* metadata = FindStylePropertyMetadata(
					target, condition->Property);
				BindingValue actual;
				BindingValue expectedSource;
				BindingValue expected;
				if (!metadata
					|| !TryReadCompiledValue(
						program, condition->ValueIndex, expectedSource)
					|| !metadata->CanRead()
					|| !metadata->TryGet(target, actual)
					|| !metadata->TryConvert(expectedSource, expected)
					|| !metadata->ValuesEqual(actual, expected))
				{
					matched = false;
					break;
				}
			}

			if (matched)
			{
				const auto [dataBegin, dataEnd] = CompiledRange(
					program.DataConditions, rule.DataConditions);
				if (dataBegin != dataEnd)
				{
					auto* source = target.GetDataContext().Get();
					if (!source) matched = false;
					for (auto condition = dataBegin;
						matched && condition != dataEnd; ++condition)
					{
						BindingValue actual;
						BindingValue expectedSource;
						BindingValue expected;
						bool pathRead = false;
						if (const auto* path = CompiledDataPath(
								program, condition->PathReference))
							pathRead = TryGetBindingPathValue(
								*source, *path, actual);
#if CUI_ENABLE_DYNAMIC_XAML
						else
							pathRead = cui::style::design::TryReadDynamicDataPath(
								program, condition->PathReference, *source, actual);
#endif
						if (!pathRead
							|| !TryReadCompiledValue(
								program, condition->ValueIndex, expectedSource)
							|| actual.Empty()
							|| !TryConvertBindingValue(
								expectedSource, actual.Kind(), expected)
							|| !BindingValuesEqual(actual, expected))
							matched = false;
					}
				}
			}

			if (rule.EnterActions.Count != 0 || rule.ExitActions.Count != 0)
			{
				ResolvedControlStyleTrigger trigger;
				trigger.RuleId = rule.RuleId;
				trigger.IsActive = matched;
#if CUI_ENABLE_DYNAMIC_XAML
				if (!TryPopulateDesignTriggerActions(rule, trigger))
#endif
				{
					trigger.CompiledProgram = &program;
					trigger.CompiledValues = program.Values;
					trigger.CompiledEnterActions = rule.EnterActions;
					trigger.CompiledExitActions = rule.ExitActions;
				}
				result.Triggers.push_back(std::move(trigger));
			}
			if (!matched) continue;

			const bool conditional = rule.PropertyConditions.Count != 0
				|| rule.DataConditions.Count != 0;
			const auto [setterBegin, setterEnd] = CompiledRange(
				program.Setters, rule.Setters);
			for (auto setter = setterBegin; setter != setterEnd; ++setter)
			{
				BindingValue value;
				std::wstring resourceKey;
				bool foundResource = true;
				const bool dynamicResource = setter->Value.Kind
					== CompiledStyleOperandKind::DynamicResource;
				if (setter->Value.Kind == CompiledStyleOperandKind::Literal)
				{
					(void)TryReadCompiledValue(
						program, setter->Value.Index, value);
				}
				else
				{
					if (const auto* key = CompiledString(
						program, setter->Value.Index))
						resourceKey.assign(key->data(), key->size());
					foundResource = dynamicResource
						? target.TryFindResource(resourceKey, value)
						: TryGetResource(resourceKey, value);
					if (!foundResource && !dynamicResource)
					{
						result.Issues.push_back({
							ControlStyleResolutionIssueCode::MissingResource,
							rule.RuleId,
							setter->Property.Name(),
							resourceKey });
						continue;
					}
				}

				const auto* metadata = FindStylePropertyMetadata(
					target, setter->Property);
				if (!metadata)
				{
					result.Issues.push_back({
						ControlStyleResolutionIssueCode::PropertyNotFound,
						rule.RuleId, setter->Property.Name(), {} });
					continue;
				}
				if (!metadata->CanWrite())
				{
					result.Issues.push_back({
						ControlStyleResolutionIssueCode::PropertyNotWritable,
						rule.RuleId, setter->Property.Name(), {} });
					continue;
				}
				BindingValue converted;
				if ((!dynamicResource || foundResource)
					&& !metadata->TryConvert(value, converted))
				{
					result.Issues.push_back({
						ControlStyleResolutionIssueCode::InvalidValue,
						rule.RuleId, setter->Property.Name(), {} });
					continue;
				}

				const DependencyPropertyReference property(
					metadata->Property());
				auto winner = std::find_if(
					winners.begin(), winners.end(),
					[&property](const auto& current)
					{
						return current.Setter.Property.Matches(property);
					});
				ResolvedControlStyleSetter candidate;
				candidate.Property = property;
				if (!dynamicResource || foundResource)
					candidate.Value = std::move(converted);
				candidate.ResourceKey = std::move(resourceKey);
				candidate.IsDynamicResource = dynamicResource;
				candidate.RuleId = rule.RuleId;
				candidate.IsConditional = conditional;
				if (winner == winners.end())
					winners.push_back({
						std::move(candidate), rule.SourceOrder });
				else if (conditional > winner->Setter.IsConditional
					|| (conditional == winner->Setter.IsConditional
						&& rule.SourceOrder >= winner->SourceOrder))
				{
					winner->Setter = std::move(candidate);
					winner->SourceOrder = rule.SourceOrder;
				}
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

bool Control::SetStyleEnvironment(
	std::shared_ptr<const ControlStyleSheet> theme,
	std::shared_ptr<const ControlStyleSheet> styles,
	bool recursive)
{
	const auto previousTheme = _themeStyleSheet;
	const auto previousStyles = _styleSheet;
#if CUI_ENABLE_DYNAMIC_XAML
	auto connect = [this]()
	{
		_themeStyleConnection.Disconnect();
		_styleSheetConnection.Disconnect();
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
	};
#endif
	_themeStyleSheet = std::move(theme);
	_styleSheet = std::move(styles);

#if CUI_ENABLE_DYNAMIC_XAML
	connect();
#endif

	RebuildStylePropertyConditionSubscription();
	RebuildStyleDataContextSubscriptions();
	bool result = RefreshDynamicResourceValues(false);
	if (!RefreshStyleValues(false)) result = false;
	if (recursive)
	{
		for (auto* child : _inheritanceChildren)
		{
			if (child && !child->SetStyleEnvironment(
				_themeStyleSheet, _styleSheet, true))
				result = false;
		}
	}
	if (result) return true;

	// Environment installation is one transaction: a resource conversion or
	// descendant refresh failure must not leave a half-new Theme/Style pair.
	_themeStyleSheet = previousTheme;
	_styleSheet = previousStyles;
#if CUI_ENABLE_DYNAMIC_XAML
	connect();
#endif
	RebuildStylePropertyConditionSubscription();
	RebuildStyleDataContextSubscriptions();
	(void)RefreshDynamicResourceValues(false);
	(void)RefreshStyleValues(false);
	if (recursive)
		for (auto* child : _inheritanceChildren)
			if (child)
				(void)child->SetStyleEnvironment(
					_themeStyleSheet, _styleSheet, true);
	return false;
}

bool Control::SetResourceDictionary(
	std::shared_ptr<const ControlStyleSheet> value)
{
	const auto previous = _resourceDictionary;
#if CUI_ENABLE_DYNAMIC_XAML
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
#endif
	_resourceDictionary = std::move(value);
#if CUI_ENABLE_DYNAMIC_XAML
	connect();
#endif
	RebuildStyleSubscriptions(true);
	bool result = RefreshDynamicResourceValues(true);
	if (!RefreshStyleValues(true)) result = false;
	if (result) return true;
	_resourceDictionary = previous;
#if CUI_ENABLE_DYNAMIC_XAML
	connect();
#endif
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
	const DependencyProperty& property,
	std::wstring resourceKey)
{
	return SetDynamicResource(
		property, std::move(resourceKey),
		DependencyPropertyValueSource::Local);
}

bool Control::SetDynamicResource(
	const DependencyProperty& property,
	std::wstring resourceKey,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
	return metadata && TrySetDynamicResourceExpressionOwned(
		*metadata, std::move(resourceKey), source);
}

bool Control::TrySetDynamicResourceExpressionOwned(
	const DependencyPropertyMetadata& metadata,
	std::wstring resourceKey,
	DependencyPropertyValueSource source)
{
	if (!metadata.CanWrite() || resourceKey.empty()
		|| EffectiveSlotIndex(source) < 0
		|| source == DependencyPropertyValueSource::Animation) return false;

	std::optional<BindingValue> proposed;
	BindingValue value;
	if (TryResolveDynamicResource(resourceKey, value))
	{
		BindingValue converted;
		if (!metadata.TryConvert(value, converted)) return false;
		proposed = std::move(converted);
	}
	return TrySetEffectiveValueEntry(
		metadata, std::move(proposed), source,
		DependencyPropertyExpressionKind::DynamicResource,
		nullptr, std::move(resourceKey), false);
}

bool Control::ClearDynamicResource(
	const DependencyProperty& property)
{
	return ClearDynamicResource(
		property, DependencyPropertyValueSource::Local);
}

bool Control::ClearDynamicResource(
	const DependencyProperty& property,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
	if (!metadata) return false;
	const int index = EffectiveSlotIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&metadata->Property());
	if (entry == _propertyValues.end()
		|| entry->second.Slots[(size_t)index].Expression
			!= DependencyPropertyExpressionKind::DynamicResource) return false;
	return ClearPropertyValueOwned(
		*metadata, source, nullptr);
}

bool Control::TryGetDynamicResourceKey(
	const DependencyProperty& property,
	std::wstring& resourceKey,
	DependencyPropertyValueSource source)
{
	const auto* metadata = GetPropertyMetadata(property);
	if (!metadata) return false;
	const int index = EffectiveSlotIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(&property);
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
		const DependencyProperty* Property = nullptr;
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
			expressions.push_back({ metadata, slot.ResourceKey,
				static_cast<DependencyPropertyValueSource>(
					index + static_cast<size_t>(
						DependencyPropertyValueSource::Inherited)) });
		}
	}
	for (auto& expression : expressions)
		if (!expression.Property || !SetDynamicResource(
			*expression.Property, std::move(expression.ResourceKey),
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
	_stylePropertyConditionConnection = OnPropertyValueChanged.Subscribe(
		[this, authorSheets](
			DependencyObject*, const DependencyPropertyChangedEventArgs& args)
		{
			const bool usedByTheme = _themeStyleSheet
				&& _themeStyleSheet->UsesPropertyCondition(
					*this, args, true);
			const bool usedByStyle = std::any_of(
				authorSheets.begin(), authorSheets.end(), [&](const auto& sheet)
				{
					return sheet
						&& sheet->UsesPropertyCondition(
							*this, args, false);
				});
			if (usedByTheme || usedByStyle) RefreshStyleValues(false);
		});
}

void Control::RebuildStyleDataContextSubscriptions()
{
	_styleDataContextConnections.clear();
	_styleDataContextOwners.clear();
	if (!GetDataContext()) return;

	std::vector<CompiledBindingPathView> compiledPaths;
	auto appendCompiledPaths = [&](
		const std::shared_ptr<const ControlStyleSheet>& sheet)
	{
		if (!sheet) return;
		for (const auto path : sheet->CompiledDataConditionPaths())
		{
			const auto duplicate = std::any_of(
				compiledPaths.begin(), compiledPaths.end(), [&](const auto existing)
				{
					return existing.Version == path.Version
						&& existing.Steps.size() == path.Steps.size()
						&& std::equal(
							existing.Steps.begin(), existing.Steps.end(),
							path.Steps.begin());
				});
			if (!duplicate) compiledPaths.push_back(path);
		}
	};
	appendCompiledPaths(_themeStyleSheet);
	for (const auto& sheet : VisibleAuthorStyleSheets())
		appendCompiledPaths(sheet);

#if CUI_ENABLE_DYNAMIC_XAML
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
	if (paths.empty() && compiledPaths.empty()) return;

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
		if (!cui::style::design::TryParseDataPathSegments(
			pathText, segments)) continue;
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
#else
	if (compiledPaths.empty()) return;
#endif

	for (const auto path : compiledPaths)
	{
		auto observation = ObserveBindingPaths(
			GetDataContext(), { path }, [this]
			{
				RebuildStyleDataContextSubscriptions();
				RefreshStyleValues(false);
			});
		for (auto& owner : observation.Owners)
			_styleDataContextOwners.push_back(std::move(owner));
		for (auto& owner : observation.ListOwners)
			_styleDataContextOwners.push_back(std::move(owner));
		for (auto& connection : observation.Connections)
			_styleDataContextConnections.push_back(std::move(connection));
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
	// WPF's ToolBar.PrepareContainerForItemOverride installs the type-specific
	// theme resource (ToolBar.ButtonStyleKey, and peers) as the container's
	// Style.  CUI represents that reference with StyleResourceKey.  The keyed
	// rule must therefore participate at Style precedence even when its
	// ResourceDictionary is the framework Theme; the implicit Theme style is
	// still evaluated separately below at Theme precedence.
	if (!_styleResourceKey.empty()
		&& _themeStyleSheet && _themeStyleSheet->HasRules())
		result.push_back(_themeStyleSheet);
	// WPF StaticResource captures the Style object in the dictionary that
	// defines a Theme template. Do not let an Application/document/local
	// dictionary with the same string key replace that captured Style.
	if (_styleResourceKeyCapturedFromTheme) return result;
	if (const auto* application = Application::Current())
	{
		const auto resources = application->GetResourcesSnapshot();
		if (resources && resources->HasRules())
			result.push_back(resources);
	}
	if (_styleSheet) result.push_back(_styleSheet);
	std::vector<std::shared_ptr<const ControlStyleSheet>> lexical;
	for (const Control* scope = this; scope;
		scope = scope->GetInheritanceParent())
		if (scope->_resourceDictionary
			&& scope->_resourceDictionary->HasRules())
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
	std::vector<DependencyPropertyReference>& appliedProperties)
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
	std::vector<DependencyPropertyReference> nextProperties;
	nextProperties.reserve(resolution.Setters.size());

	for (const auto& setter : resolution.Setters)
	{
		const bool wasApplied = ContainsStyleProperty(
			appliedProperties, setter.Property);
		bool applied = false;
		if (const auto* property = setter.Property.Identity())
		{
			applied = setter.IsDynamicResource
				? SetDynamicResource(*property, setter.ResourceKey, source)
				: TrySetPropertyValue(*property, setter.Value, source);
		}
		else
		{
#if CUI_ENABLE_DYNAMIC_XAML
			applied = setter.IsDynamicResource
				? TrySetDynamicResourceExpressionOwned(
					setter.Property.Name(), setter.ResourceKey, source)
				: TrySetPropertyValue(
					setter.Property.Name(), setter.Value, source);
#else
			applied = false;
#endif
		}
		if (applied)
		{
			nextProperties.push_back(setter.Property);
		}
		else
		{
			result = false;
			if (wasApplied) nextProperties.push_back(setter.Property);
		}
	}

	for (const auto& property : appliedProperties)
	{
		if (ContainsStyleProperty(nextProperties, property)) continue;
		const auto* identity = property.Identity();
		bool cleared = false;
		bool retained = false;
		if (identity)
		{
			cleared = ClearPropertyValue(*identity, source);
			retained = HasPropertyValue(*identity, source);
		}
#if CUI_ENABLE_DYNAMIC_XAML
		else
		{
			cleared = ClearPropertyValue(property.Name(), source);
			retained = HasPropertyValue(property.Name(), source);
		}
#endif
		if (!cleared && retained)
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
	if (!PruneStyleTriggerActions(source, activeSheets))
		result = false;
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
