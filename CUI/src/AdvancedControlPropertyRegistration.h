#pragma once

#include "Control.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cui::advanced_properties
{
	template<typename TOwner, typename TValue>
	DependencyPropertyOptions<TOwner, TValue> Options(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			DependencyPropertyEditorKind editor))
	{
		DependencyPropertyOptions<TOwner, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	template<typename TOwner, typename TValue, typename TName>
	DependencyPropertyRegistration RegisterFieldStatic(
		TName name,
		TValue TOwner::* field,
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			DependencyPropertyEditorKind editor = DependencyPropertyEditorKind::Auto))
	{
		auto options = Options<TOwner, TValue>(
			std::move(defaultValue)
			CUI_DESIGN_METADATA_ARGUMENTS(
				category, categoryOrder, order, editor));
		return DependencyPropertyRegistry::RegisterStatic<TOwner, TValue>(
			std::move(name),
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const TValue& value) { target.*field = value; },
			{}, std::move(options));
	}

	template<typename TOwner, typename TName>
	DependencyPropertyRegistration RegisterMetricStatic(
		TName name,
		float TOwner::* field,
		float defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order),
		float minimum = 0.0f,
		std::optional<float> maximum = std::nullopt)
	{
		auto options = Options<TOwner, float>(defaultValue
			CUI_DESIGN_METADATA_ARGUMENTS(
				category, categoryOrder, order,
				DependencyPropertyEditorKind::Number));
		options.Validate = [](const float& value)
		{
			return std::isfinite(value);
		};
		options.Coerce = [minimum, maximum](
			TOwner&, const float& value) -> std::optional<float>
		{
			return maximum
				? (std::clamp)(value, minimum, *maximum)
				: (std::max)(value, minimum);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = minimum;
		if (maximum) options.Design.Maximum = *maximum;
		options.Design.Step = 0.5;
		)
		return DependencyPropertyRegistry::RegisterStatic<TOwner, float>(
			std::move(name),
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const float& value) { target.*field = value; },
			{}, std::move(options));
	}

	template<typename TOwner, typename TName>
	DependencyPropertyRegistration RegisterIntMetricStatic(
		TName name,
		int TOwner::* field,
		int defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order),
		int minimum = 0,
		std::optional<int> maximum = std::nullopt)
	{
		auto options = Options<TOwner, int>(defaultValue
			CUI_DESIGN_METADATA_ARGUMENTS(
				category, categoryOrder, order,
				DependencyPropertyEditorKind::Number));
		options.Coerce = [minimum, maximum](
			TOwner&, const int& value) -> std::optional<int>
		{
			return maximum
				? (std::clamp)(value, minimum, *maximum)
				: (std::max)(value, minimum);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = minimum;
		if (maximum) options.Design.Maximum = *maximum;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<TOwner, int>(
			std::move(name),
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const int& value) { target.*field = value; },
			{}, std::move(options));
	}

	template<typename TOwner, typename TEnum, typename TName>
	DependencyPropertyRegistration RegisterEnumFieldStatic(
		TName name,
		TEnum TOwner::* field,
		TEnum defaultValue,
		std::initializer_list<TEnum> allowedValues
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			std::initializer_list<
				std::pair<const wchar_t*, TEnum>> choices))
	{
		auto options = Options<TOwner, int>(static_cast<int>(defaultValue)
			CUI_DESIGN_METADATA_ARGUMENTS(
				category, categoryOrder, order,
				DependencyPropertyEditorKind::Choice));
		std::vector<int> allowedValueStorage;
		CUI_DESIGN_METADATA_ONLY(
		for (const auto& [displayName, value] : choices)
		{
			options.Design.Choices.push_back(
				{ displayName, BindingValue(static_cast<int>(value)) });
		}
		)
		for (const auto value : allowedValues)
			allowedValueStorage.push_back(static_cast<int>(value));
		options.Validate = [allowedValues = std::move(allowedValueStorage)](
			const int& proposed)
		{
			for (const auto value : allowedValues)
				if (value == proposed) return true;
			return false;
		};
		return DependencyPropertyRegistry::RegisterStatic<TOwner, int>(
			std::move(name),
			[field](TOwner& target) { return static_cast<int>(target.*field); },
			[field](TOwner& target, const int& value)
			{
				target.*field = static_cast<TEnum>(value);
			}, {}, std::move(options));
	}

	template<typename TOwner, typename TName>
	DependencyPropertyRegistration RegisterColorStatic(
		TName name,
		D2D1_COLOR_F TOwner::* field,
		D2D1_COLOR_F defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = Options<TOwner, D2D1_COLOR_F>(defaultValue
			CUI_DESIGN_METADATA_ARGUMENTS(
				L"Appearance", 200, order,
				DependencyPropertyEditorKind::Color));
		options.Equals = [](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
		{
			return std::fabs(left.r - right.r) < 1e-6f
				&& std::fabs(left.g - right.g) < 1e-6f
				&& std::fabs(left.b - right.b) < 1e-6f
				&& std::fabs(left.a - right.a) < 1e-6f;
		};
		return DependencyPropertyRegistry::RegisterStatic<TOwner, D2D1_COLOR_F>(
			std::move(name),
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const D2D1_COLOR_F& value)
			{
				target.*field = value;
			}, {}, std::move(options));
	}
}
