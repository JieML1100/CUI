#pragma once

#include "Transform.h"

#include <d2d1.h>
#include <optional>
#include <vector>

struct ID2D1Geometry;

namespace cui::drawing
{
enum class GeometryKind
{
	Rectangle,
	Ellipse,
	Path,
	Group
};

enum class GeometryFillRule
{
	EvenOdd,
	Nonzero
};

enum class PathSegmentKind
{
	Line,
	Bezier,
	QuadraticBezier,
	Arc
};

enum class SweepDirection
{
	Counterclockwise,
	Clockwise
};

struct PathSegment
{
	PathSegmentKind Kind = PathSegmentKind::Line;
	D2D1_POINT_2F Point = D2D1::Point2F();
	D2D1_POINT_2F Point1 = D2D1::Point2F();
	D2D1_POINT_2F Point2 = D2D1::Point2F();
	D2D1_POINT_2F Point3 = D2D1::Point2F();
	D2D1_SIZE_F Size = D2D1::SizeF();
	float RotationAngle = 0.0f;
	bool IsLargeArc = false;
	SweepDirection Sweep = SweepDirection::Counterclockwise;

	bool operator==(const PathSegment& other) const noexcept;
};

struct PathFigure
{
	D2D1_POINT_2F StartPoint = D2D1::Point2F();
	bool IsClosed = false;
	bool IsFilled = true;
	std::vector<PathSegment> Segments;

	bool operator==(const PathFigure& other) const noexcept;
};

/**
 * Device-independent geometry used by Control::Clip and public XAML.
 *
 * Rectangle geometry uses Rect and optional RadiusX/RadiusY. Ellipse geometry
 * uses Center and RadiusX/RadiusY. Path geometry owns Figures. Group geometry
 * recursively owns Children. LocalTransform maps Geometry.Transform.
 */
struct Geometry
{
	GeometryKind Kind = GeometryKind::Rectangle;
	GeometryFillRule FillRule = GeometryFillRule::EvenOdd;
	D2D1_RECT_F Rect = D2D1::RectF();
	D2D1_POINT_2F Center = D2D1::Point2F();
	float RadiusX = 0.0f;
	float RadiusY = 0.0f;
	std::vector<PathFigure> Figures;
	std::vector<Geometry> Children;
	std::optional<cui::drawing::Transform> LocalTransform;

	bool operator==(const Geometry& other) const noexcept;
	/** Returns true when the local point is inside the filled geometry. */
	bool ContainsPoint(D2D1_POINT_2F point) const noexcept;
	/**
	 * Creates a caller-owned Direct2D geometry. When transform is supplied, the
	 * returned geometry is expressed in the transformed coordinate space.
	 */
	ID2D1Geometry* CreateD2DGeometry(
		const D2D1_MATRIX_3X2_F* transform = nullptr) const;
};
}
