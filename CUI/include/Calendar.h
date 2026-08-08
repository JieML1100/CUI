#pragma once

#include "CalendarView.h"

/**
 * WPF Calendar QName and public C++ type.
 *
 * CalendarView remains as a source/XAML compatibility surface. Calendar is
 * the canonical WPF-facing type and deliberately reuses the same retained
 * date-grid behavior, keyboard model and accessibility virtualization.
 */
class Calendar : public CalendarView
{
public:
	Calendar() = default;
	UIClass Type() override { return UIClass::UI_Calendar; }
};
