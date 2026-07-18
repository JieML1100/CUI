#pragma once

#include <d2d1.h>
#include <memory>
#include <vector>

class D2DGraphics;
class BitmapSource;
struct ID2D1Brush;

namespace cui::drawing
{
enum class BrushKind
{
	Solid,
	LinearGradient,
	RadialGradient,
	Image
};

enum class BrushMappingMode
{
	Absolute,
	RelativeToBoundingBox
};

enum class ImageBrushStretch
{
	None,
	Fill,
	Uniform,
	UniformToFill
};

enum class ImageBrushAlignmentX
{
	Left,
	Center,
	Right
};

enum class ImageBrushAlignmentY
{
	Top,
	Center,
	Bottom
};

struct GradientStop
{
	float Offset = 0.0f;
	D2D1_COLOR_F Color{ 0.0f, 0.0f, 0.0f, 1.0f };

	bool operator==(const GradientStop& other) const noexcept
	{
		return Offset == other.Offset
			&& Color.r == other.Color.r
			&& Color.g == other.Color.g
			&& Color.b == other.Color.b
			&& Color.a == other.Color.a;
	}
};

/**
 * Device-independent brush description used by controls and XAML.
 * CreateBrush returns an owned COM reference; callers must Release it.
 */
struct Brush
{
	BrushKind Kind = BrushKind::Solid;
	BrushMappingMode MappingMode = BrushMappingMode::RelativeToBoundingBox;
	D2D1_COLOR_F Color{ 0.0f, 0.0f, 0.0f, 1.0f };
	float Opacity = 1.0f;
	D2D1_POINT_2F StartPoint{ 0.0f, 0.0f };
	D2D1_POINT_2F EndPoint{ 1.0f, 1.0f };
	D2D1_POINT_2F Center{ 0.5f, 0.5f };
	D2D1_POINT_2F GradientOrigin{ 0.5f, 0.5f };
	float RadiusX = 0.5f;
	float RadiusY = 0.5f;
	std::vector<GradientStop> GradientStops;
	std::shared_ptr<BitmapSource> ImageSource;
	ImageBrushStretch Stretch = ImageBrushStretch::Fill;
	ImageBrushAlignmentX AlignmentX = ImageBrushAlignmentX::Center;
	ImageBrushAlignmentY AlignmentY = ImageBrushAlignmentY::Center;

	ID2D1Brush* CreateBrush(
		D2DGraphics& graphics,
		D2D1_SIZE_F bounds) const;
};
}
