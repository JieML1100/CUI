#pragma once

#include "Control.h"

/**
 * Semantic base for controls whose current value is constrained by a numeric
 * interval. WPF owns Minimum, Maximum, Value, SmallChange, LargeChange and
 * ValueChanged here; derived controls only override metadata and presentation.
 */
class RangeBase : public Control
{
public:
	using UIElement::ValueChanged;
	RangeBase() = default;
	UIClass Type() override { return UIClass::UI_RangeBase; }
	static const DependencyProperty& MinimumProperty();
	static const DependencyProperty& MaximumProperty();
	static const DependencyProperty& ValueProperty();
	static const DependencyProperty& SmallChangeProperty();
	static const DependencyProperty& LargeChangeProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	PROPERTY(double, Minimum);
	GET(double, Minimum);
	SET(double, Minimum);

	PROPERTY(double, Maximum);
	GET(double, Maximum);
	SET(double, Maximum);

	PROPERTY(double, Value);
	GET(double, Value);
	SET(double, Value);

	PROPERTY(double, SmallChange);
	GET(double, SmallChange);
	SET(double, SmallChange);

	PROPERTY(double, LargeChange);
	GET(double, LargeChange);
	SET(double, LargeChange);

	void SetRange(double minimum, double maximum);

protected:
	explicit RangeBase(double defaultMaximum)
		: _maximum(defaultMaximum) {}
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::Slider, L"RangeBase");
	}
	double MinimumCore() const noexcept { return _minimum; }
	double MaximumCore() const noexcept { return _maximum; }
	double ValueCore() const noexcept { return _value; }
	virtual double CoerceRangeValue(double value) const;
	virtual void OnMinimumChanged(double oldValue, double newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	virtual void OnMaximumChanged(double oldValue, double newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	virtual void OnRangeValueChanged(double oldValue, double newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	void SetCurrentRangeValue(double value);
	void ReevaluateRangeValue();

private:
	double _minimum = 0.0;
	double _maximum = 1.0;
	double _value = 0.0;
	double _smallChange = 0.1;
	double _largeChange = 1.0;
};
