#pragma once

#include <cstdint>

/** WPF paragraph/text alignment shared by text controls and flow blocks. */
enum class TextAlignment : std::uint8_t
{
	Left,
	Right,
	Center,
	Justify
};
