#pragma once

#include <d2d1.h>

/**
 * Semantic colors used by CUI's built-in light theme.
 *
 * Controls should consume these roles instead of inventing isolated grays or
 * accent blues. Applications remain free to override every color through
 * local properties, styles, or XAML resources.
 */
namespace cui::theme::palette
{
	inline constexpr D2D1_COLOR_F Transparent{ 0.0f, 0.0f, 0.0f, 0.0f };

	inline constexpr D2D1_COLOR_F Window{ 0.957f, 0.969f, 0.984f, 1.0f };
	inline constexpr D2D1_COLOR_F Surface{ 1.0f, 1.0f, 1.0f, 1.0f };
	inline constexpr D2D1_COLOR_F SurfaceSubtle{ 0.969f, 0.976f, 0.988f, 1.0f };
	inline constexpr D2D1_COLOR_F SurfaceMuted{ 0.933f, 0.949f, 0.973f, 1.0f };

	inline constexpr D2D1_COLOR_F Border{ 0.827f, 0.863f, 0.910f, 1.0f };
	inline constexpr D2D1_COLOR_F BorderStrong{ 0.682f, 0.733f, 0.804f, 1.0f };

	inline constexpr D2D1_COLOR_F TextPrimary{ 0.090f, 0.125f, 0.200f, 1.0f };
	inline constexpr D2D1_COLOR_F TextSecondary{ 0.310f, 0.376f, 0.471f, 1.0f };
	inline constexpr D2D1_COLOR_F TextMuted{ 0.455f, 0.514f, 0.600f, 1.0f };

	inline constexpr D2D1_COLOR_F Accent{ 0.184f, 0.435f, 0.894f, 1.0f };
	inline constexpr D2D1_COLOR_F AccentHover{ 0.141f, 0.373f, 0.780f, 1.0f };
	inline constexpr D2D1_COLOR_F AccentSoft{ 0.184f, 0.435f, 0.894f, 0.10f };
	inline constexpr D2D1_COLOR_F AccentSelected{ 0.184f, 0.435f, 0.894f, 0.18f };
	inline constexpr D2D1_COLOR_F AccentPressed{ 0.184f, 0.435f, 0.894f, 0.26f };
	inline constexpr D2D1_COLOR_F SelectionBack{ 0.184f, 0.435f, 0.894f, 0.30f };
	inline constexpr D2D1_COLOR_F OnAccent{ 1.0f, 1.0f, 1.0f, 1.0f };

	inline constexpr D2D1_COLOR_F Positive{ 0.082f, 0.588f, 0.416f, 1.0f };
	inline constexpr D2D1_COLOR_F Negative{ 0.820f, 0.263f, 0.263f, 1.0f };
	inline constexpr D2D1_COLOR_F Warning{ 0.820f, 0.541f, 0.071f, 1.0f };

	inline constexpr D2D1_COLOR_F ScrollTrack{ 0.859f, 0.890f, 0.933f, 1.0f };
	inline constexpr D2D1_COLOR_F ScrollThumb{ 0.553f, 0.627f, 0.722f, 1.0f };
	inline constexpr D2D1_COLOR_F DisabledOverlay{ 0.957f, 0.969f, 0.984f, 0.56f };
	inline constexpr D2D1_COLOR_F Shadow{ 0.055f, 0.086f, 0.145f, 0.10f };
	inline constexpr D2D1_COLOR_F TooltipSurface{ 0.090f, 0.125f, 0.200f, 0.96f };
}
