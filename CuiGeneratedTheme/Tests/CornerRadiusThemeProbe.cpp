#include "CornerRadiusVisualStateTheme.g.h"

#include <memory>
#include <string>

class ControlStyleSheet;

std::shared_ptr<const ControlStyleSheet>
CuiCornerRadiusVisualStateAotProbe(std::wstring* outError)
{
	return CuiGeneratedCornerRadiusTheme::DefaultStyleSheet(outError);
}
