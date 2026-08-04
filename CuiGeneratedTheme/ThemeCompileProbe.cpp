#include "CuiFrameworkTheme.g.h"

#include "Control.h"

#include <memory>
#include <string>
#include <type_traits>

static_assert(!std::is_constructible_v<CuiGeneratedFrameworkTheme>);

std::shared_ptr<const ControlStyleSheet>
CuiGeneratedThemeCompileProbe(std::wstring* outError)
{
	return CuiGeneratedFrameworkTheme::DefaultStyleSheet(outError);
}
