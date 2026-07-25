#pragma once

#include "Control.h"

/**
 * Semantic base for controls whose current value is constrained by a numeric
 * interval. Minimum, Maximum, Value and ValueChanged have one owner and one
 * coercion path; derived controls only refine value coercion and presentation.
 */
class RangeBase : public Control
{
public:
	using UIElement::ValueChanged;
	RangeBase() = default;
	UIClass Type() override { return UIClass::UI_RangeBase; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	PROPERTY(double, Minimum);
	GET(double, Minimum);
	SET(double, Minimum);

	PROPERTY(double, Maximum);
	GET(double, Maximum);
	SET(double, Maximum);

	PROPERTY(double, Value);
	GET(double, Value);
	SET(double, Value);

	void SetRange(double minimum, double maximum);

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::Slider, L"RangeBase");
	}
	double MinimumCore() const noexcept { return _minimum; }
	double MaximumCore() const noexcept { return _maximum; }
	double ValueCore() const noexcept { return _value; }
	virtual double CoerceRangeValue(double value) const;
	virtual void OnRangeValueChanged(double oldValue, double newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	void SetCurrentRangeValue(double value);
	void ReevaluateRangeValue();

private:
	double _minimum = 0.0;
	double _maximum = 100.0;
	double _value = 0.0;
};
