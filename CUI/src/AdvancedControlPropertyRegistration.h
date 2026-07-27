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
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DependencyPropertyEditorKind editor)
	{
		DependencyPropertyOptions<TOwner, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	template<typename TOwner>
	auto Subscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			TOwner& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName)
						handler();
				});
		};
	}

	template<typename TOwner, typename TValue>
	void RegisterField(
		const wchar_t* name,
		TValue TOwner::* field,
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DependencyPropertyEditorKind editor = DependencyPropertyEditorKind::Auto)
	{
		auto options = Options<TOwner, TValue>(
			std::move(defaultValue), category, categoryOrder, order, editor);
		DependencyPropertyRegistry::Register<TOwner, TValue>(name,
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const TValue& value) { target.*field = value; },
			Subscriber<TOwner>(name), std::move(options));
	}

	template<typename TOwner>
	void RegisterMetric(
		const wchar_t* name,
		float TOwner::* field,
		float defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		float minimum = 0.0f,
		std::optional<float> maximum = std::nullopt)
	{
		auto options = Options<TOwner, float>(defaultValue, category,
			categoryOrder, order, DependencyPropertyEditorKind::Number);
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
		options.Design.Minimum = minimum;
		if (maximum) options.Design.Maximum = *maximum;
		options.Design.Step = 0.5;
		DependencyPropertyRegistry::Register<TOwner, float>(name,
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const float& value) { target.*field = value; },
			Subscriber<TOwner>(name), std::move(options));
	}

	template<typename TOwner>
	void RegisterIntMetric(
		const wchar_t* name,
		int TOwner::* field,
		int defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		int minimum = 0,
		std::optional<int> maximum = std::nullopt)
	{
		auto options = Options<TOwner, int>(defaultValue, category,
			categoryOrder, order, DependencyPropertyEditorKind::Number);
		options.Coerce = [minimum, maximum](
			TOwner&, const int& value) -> std::optional<int>
		{
			return maximum
				? (std::clamp)(value, minimum, *maximum)
				: (std::max)(value, minimum);
		};
		options.Design.Minimum = minimum;
		if (maximum) options.Design.Maximum = *maximum;
		options.Design.Step = 1.0;
		DependencyPropertyRegistry::Register<TOwner, int>(name,
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const int& value) { target.*field = value; },
			Subscriber<TOwner>(name), std::move(options));
	}

	template<typename TOwner, typename TEnum>
	void RegisterEnumField(
		const wchar_t* name,
		TEnum TOwner::* field,
		TEnum defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		std::initializer_list<std::pair<const wchar_t*, TEnum>> choices)
	{
		auto options = Options<TOwner, int>(static_cast<int>(defaultValue),
			category, categoryOrder, order, DependencyPropertyEditorKind::Choice);
		std::vector<int> allowedValues;
		for (const auto& [displayName, value] : choices)
		{
			options.Design.Choices.push_back(
				{ displayName, BindingValue(static_cast<int>(value)) });
			allowedValues.push_back(static_cast<int>(value));
		}
		options.Validate = [allowedValues = std::move(allowedValues)](
			const int& proposed)
		{
			for (const auto value : allowedValues)
				if (value == proposed) return true;
			return false;
		};
		DependencyPropertyRegistry::Register<TOwner, int>(name,
			[field](TOwner& target) { return static_cast<int>(target.*field); },
			[field](TOwner& target, const int& value)
			{
				target.*field = static_cast<TEnum>(value);
			}, Subscriber<TOwner>(name), std::move(options));
	}

	template<typename TOwner>
	void RegisterColor(
		const wchar_t* name,
		D2D1_COLOR_F TOwner::* field,
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = Options<TOwner, D2D1_COLOR_F>(defaultValue,
			L"Appearance", 200, order, DependencyPropertyEditorKind::Color);
		options.Equals = [](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
		{
			return std::fabs(left.r - right.r) < 1e-6f
				&& std::fabs(left.g - right.g) < 1e-6f
				&& std::fabs(left.b - right.b) < 1e-6f
				&& std::fabs(left.a - right.a) < 1e-6f;
		};
		DependencyPropertyRegistry::Register<TOwner, D2D1_COLOR_F>(name,
			[field](TOwner& target) { return target.*field; },
			[field](TOwner& target, const D2D1_COLOR_F& value)
			{
				target.*field = value;
			}, Subscriber<TOwner>(name), std::move(options));
	}
}
