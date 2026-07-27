#include "RangeBase.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<RangeBase, TValue> RangeOptions(
		TValue defaultValue, int order)
	{
		DependencyPropertyOptions<RangeBase, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Range";
		options.Design.CategoryOrder = 100;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Step = 0.1;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto RangeSubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			RangeBase& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName)
						handler();
				});
		};
	}
}

void RangeBase::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto minimumOptions = RangeOptions(0.0, 10);
		minimumOptions.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		DependencyPropertyRegistry::Register<RangeBase, double>(L"Minimum",
			[](RangeBase& target) { return target.Minimum; },
			[](RangeBase& target, const double& value)
			{ target.Minimum = value; },
			RangeSubscriber(L"Minimum"), std::move(minimumOptions));

		auto maximumOptions = RangeOptions(100.0, 20);
		maximumOptions.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		maximumOptions.Coerce = [](
			RangeBase& target,
			const double& proposed) -> std::optional<double>
		{
			return (std::max)(target.Minimum, proposed);
		};
		DependencyPropertyRegistry::Register<RangeBase, double>(L"Maximum",
			[](RangeBase& target) { return target.Maximum; },
			[](RangeBase& target, const double& value)
			{ target.Maximum = value; },
			RangeSubscriber(L"Maximum"), std::move(maximumOptions));

		auto valueOptions = RangeOptions(0.0, 30);
		valueOptions.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		valueOptions.Coerce = [](
			RangeBase& target,
			const double& proposed) -> std::optional<double>
		{
			return target.CoerceRangeValue(proposed);
		};
		valueOptions.Equals = [](
			const double& left, const double& right)
		{
			return std::fabs(left - right) <= 0.0000001;
		};
		valueOptions.Changed = [](
			RangeBase& target,
			const double& oldValue,
			const double& newValue)
		{
			target.OnRangeValueChanged(oldValue, newValue);
			RoutedPropertyChangedEventArgs<double> args(oldValue, newValue);
			target.ValueChanged(&target, args);
		};
		DependencyPropertyRegistry::Register<RangeBase, double>(L"Value",
			[](RangeBase& target) { return target.Value; },
			[](RangeBase& target, const double& value)
			{ target.Value = value; },
			RangeSubscriber(L"Value"), std::move(valueOptions));
		return true;
	}();
	(void)registered;
}

GET_CPP(RangeBase, double, Minimum)
{
	return _minimum;
}

SET_CPP(RangeBase, double, Minimum)
{
	if (!SetPropertyField(L"Minimum", _minimum, value)) return;
	(void)ReevaluatePropertyValue(L"Maximum");
	ReevaluateRangeValue();
}

GET_CPP(RangeBase, double, Maximum)
{
	return _maximum;
}

SET_CPP(RangeBase, double, Maximum)
{
	if (!SetPropertyField(L"Maximum", _maximum, value)) return;
	ReevaluateRangeValue();
}

GET_CPP(RangeBase, double, Value)
{
	return _value;
}

SET_CPP(RangeBase, double, Value)
{
	(void)SetPropertyField(L"Value", _value, value);
}

void RangeBase::SetRange(double minimum, double maximum)
{
	Minimum = minimum;
	Maximum = (std::max)(minimum, maximum);
}

double RangeBase::CoerceRangeValue(double value) const
{
	if (!std::isfinite(value)) value = _minimum;
	return (std::clamp)(value, _minimum, (std::max)(_minimum, _maximum));
}

void RangeBase::SetCurrentRangeValue(double value)
{
	(void)SetCurrentPropertyField(L"Value", _value, value);
}

void RangeBase::ReevaluateRangeValue()
{
	(void)ReevaluatePropertyValue(L"Value");
}
