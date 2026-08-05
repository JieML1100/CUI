#pragma once

#include "Layout/LayoutTypes.h"
#include "Transform.h"

#include <d2d1.h>
#include <memory>
#include <optional>
#include <vector>

class D2DGraphics;
class BitmapSource;
struct ID2D1Brush;

namespace cui::drawing
{
using ImageBrushStretch [[deprecated("Use ::Stretch")]] = ::Stretch;

enum class BrushKind
{
	/** WPF null-brush value; paints nothing and reveals renderer/theme fallback. */
	None,
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
	Brush() = default;
	Brush(D2D1_COLOR_F color) noexcept
		: Kind(BrushKind::Solid), Color(color) {}

	BrushKind Kind = BrushKind::None;
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
	/** Applied after the brush output is mapped to the painted area. */
	std::optional<cui::drawing::Transform> Transform;
	/** Applied in normalized 1x1 brush-output coordinates before mapping. */
	std::optional<cui::drawing::Transform> RelativeTransform;
	std::shared_ptr<BitmapSource> ImageSource;
	::Stretch Stretch = ::Stretch::Fill;
	ImageBrushAlignmentX AlignmentX = ImageBrushAlignmentX::Center;
	ImageBrushAlignmentY AlignmentY = ImageBrushAlignmentY::Center;

	bool operator==(const Brush& other) const noexcept
	{
		return Kind == other.Kind
			&& MappingMode == other.MappingMode
			&& Color.r == other.Color.r
			&& Color.g == other.Color.g
			&& Color.b == other.Color.b
			&& Color.a == other.Color.a
			&& Opacity == other.Opacity
			&& StartPoint.x == other.StartPoint.x
			&& StartPoint.y == other.StartPoint.y
			&& EndPoint.x == other.EndPoint.x
			&& EndPoint.y == other.EndPoint.y
			&& Center.x == other.Center.x
			&& Center.y == other.Center.y
			&& GradientOrigin.x == other.GradientOrigin.x
			&& GradientOrigin.y == other.GradientOrigin.y
			&& RadiusX == other.RadiusX
			&& RadiusY == other.RadiusY
			&& GradientStops == other.GradientStops
			&& Transform == other.Transform
			&& RelativeTransform == other.RelativeTransform
			&& ImageSource == other.ImageSource
			&& Stretch == other.Stretch
			&& AlignmentX == other.AlignmentX
			&& AlignmentY == other.AlignmentY;
	}

	ID2D1Brush* CreateBrush(
		D2DGraphics& graphics,
		D2D1_SIZE_F bounds) const;
	D2D1_MATRIX_3X2_F ToTransformMatrix(D2D1_SIZE_F bounds) const noexcept;
};

inline Brush MakeSolidColorBrush(D2D1_COLOR_F color, float opacity = 1.0f)
{
	Brush result;
	result.Kind = BrushKind::Solid;
	result.Color = color;
	result.Opacity = opacity;
	return result;
}

inline Brush NoBrush() noexcept
{
	return {};
}
}
