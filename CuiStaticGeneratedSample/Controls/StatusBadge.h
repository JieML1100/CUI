#pragma once

#include <Button.h>

namespace Acme::Controls
{
class StatusBadge final : public Button
{
public:
	Event<void(Control*, int)> OnSeverityInvoked;

	StatusBadge(int x, int y, int width, int height)
		: Button(L"", x, y, width, height)
	{
		Round = 10.0f;
	}
};
}
