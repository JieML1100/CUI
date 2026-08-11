#pragma once

#include <cstdint>

/** Base reading direction for a flow-document paragraph. */
enum class FlowDirection : std::uint8_t
{
	LeftToRight,
	RightToLeft
};
