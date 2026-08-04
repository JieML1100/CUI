#include "RangeBase.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<RangeBase, TValue> RangeOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		DependencyPropertyOptions<RangeBase, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Range";
		options.Design.CategoryOrder = 100;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Step = 0.1;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}
}

const DependencyProperty& RangeBase::MinimumProperty()
{
	static const auto registration = []
	{
		auto options = RangeOptions(0.0 CUI_DESIGN_METADATA_ARGUMENTS(10));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		options.Changed = [](
			RangeBase& target, const double& oldValue, const double& newValue)
		{
			(void)target.CoerceValue(RangeBase::MaximumProperty());
			target.ReevaluateRangeValue();
			target.OnMinimumChanged(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<RangeBase, double>(
			DependencyPropertyRegistrationLiteral(L"Minimum"),
			[](RangeBase& target) { return target.Minimum; },
			[](RangeBase& target, const double& value)
			{ target.Minimum = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& RangeBase::MaximumProperty()
{
	static const auto registration = []
	{
		auto options = RangeOptions(1.0 CUI_DESIGN_METADATA_ARGUMENTS(20));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		options.Coerce = [](
			RangeBase& target,
			const double& proposed) -> std::optional<double>
		{
			return (std::max)(target.Minimum, proposed);
		};
		options.Changed = [](
			RangeBase& target, const double& oldValue, const double& newValue)
		{
			target.ReevaluateRangeValue();
			target.OnMaximumChanged(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<RangeBase, double>(
			DependencyPropertyRegistrationLiteral(L"Maximum"),
			[](RangeBase& target) { return target.Maximum; },
			[](RangeBase& target, const double& value)
			{ target.Maximum = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& RangeBase::ValueProperty()
{
	static const auto registration = []
	{
		auto options = RangeOptions(0.0 CUI_DESIGN_METADATA_ARGUMENTS(30));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		options.Coerce = [](
			RangeBase& target,
			const double& proposed) -> std::optional<double>
		{
			return target.CoerceRangeValue(proposed);
		};
		options.Equals = [](const double& left, const double& right)
		{
			return std::fabs(left - right) <= 0.0000001;
		};
		options.Changed = [](
			RangeBase& target, const double& oldValue, const double& newValue)
		{
			target.OnRangeValueChanged(oldValue, newValue);
			RoutedPropertyChangedEventArgs<double> args(oldValue, newValue);
			target.ValueChanged(&target, args);
		};
		return DependencyPropertyRegistry::RegisterStatic<RangeBase, double>(
			DependencyPropertyRegistrationLiteral(L"Value"),
			[](RangeBase& target) { return target.Value; },
			[](RangeBase& target, const double& value)
			{ target.Value = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& RangeBase::SmallChangeProperty()
{
	static const auto registration = []
	{
		auto options = RangeOptions(0.1 CUI_DESIGN_METADATA_ARGUMENTS(40));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed) && proposed >= 0.0;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<RangeBase, double>(
			DependencyPropertyRegistrationLiteral(L"SmallChange"),
			[](RangeBase& target) { return target.SmallChange; },
			[](RangeBase& target, const double& value)
			{ target.SmallChange = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& RangeBase::LargeChangeProperty()
{
	static const auto registration = []
	{
		auto options = RangeOptions(1.0 CUI_DESIGN_METADATA_ARGUMENTS(50));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed) && proposed >= 0.0;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<RangeBase, double>(
			DependencyPropertyRegistrationLiteral(L"LargeChange"),
			[](RangeBase& target) { return target.LargeChange; },
			[](RangeBase& target, const double& value)
			{ target.LargeChange = value; }, {}, std::move(options));
	}();
	return *registration;
}

void RangeBase::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)MinimumProperty();
	(void)MaximumProperty();
	(void)ValueProperty();
	(void)SmallChangeProperty();
	(void)LargeChangeProperty();
#endif
}

GET_CPP(RangeBase, double, Minimum)
{
	return _minimum;
}

SET_CPP(RangeBase, double, Minimum)
{
	(void)SetPropertyField(MinimumProperty(), _minimum, value);
}

GET_CPP(RangeBase, double, Maximum)
{
	return _maximum;
}

SET_CPP(RangeBase, double, Maximum)
{
	(void)SetPropertyField(MaximumProperty(), _maximum, value);
}

GET_CPP(RangeBase, double, Value)
{
	return _value;
}

SET_CPP(RangeBase, double, Value)
{
	(void)SetPropertyField(ValueProperty(), _value, value);
}

GET_CPP(RangeBase, double, SmallChange)
{
	return _smallChange;
}

SET_CPP(RangeBase, double, SmallChange)
{
	(void)SetPropertyField(SmallChangeProperty(), _smallChange, value);
}

GET_CPP(RangeBase, double, LargeChange)
{
	return _largeChange;
}

SET_CPP(RangeBase, double, LargeChange)
{
	(void)SetPropertyField(LargeChangeProperty(), _largeChange, value);
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
	(void)SetCurrentPropertyField(ValueProperty(), _value, value);
}

void RangeBase::ReevaluateRangeValue()
{
	(void)CoerceValue(ValueProperty());
}
