#include "Geometry.h"

#include <Factory.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>
#include <wrl/client.h>

namespace cui::drawing
{
namespace
{
	using Microsoft::WRL::ComPtr;
	constexpr float Pi = std::numbers::pi_v<float>;
	constexpr float BoundaryEpsilon = 0.0001f;

	bool PointEquals(D2D1_POINT_2F left, D2D1_POINT_2F right) noexcept
	{
		return left.x == right.x && left.y == right.y;
	}

	bool SizeEquals(D2D1_SIZE_F left, D2D1_SIZE_F right) noexcept
	{
		return left.width == right.width && left.height == right.height;
	}

	bool RectEquals(D2D1_RECT_F left, D2D1_RECT_F right) noexcept
	{
		return left.left == right.left && left.top == right.top
			&& left.right == right.right && left.bottom == right.bottom;
	}

	float FiniteOr(float value, float fallback = 0.0f) noexcept
	{
		return std::isfinite(value) ? value : fallback;
	}

	float NonNegativeFinite(float value) noexcept
	{
		return (std::max)(0.0f, FiniteOr(value));
	}

	D2D1_POINT_2F FinitePoint(D2D1_POINT_2F value) noexcept
	{
		return D2D1::Point2F(FiniteOr(value.x), FiniteOr(value.y));
	}

	D2D1_RECT_F NormalizedRect(D2D1_RECT_F value) noexcept
	{
		if (!std::isfinite(value.left) || !std::isfinite(value.top)
			|| !std::isfinite(value.right) || !std::isfinite(value.bottom))
			return D2D1::RectF();
		if (value.right < value.left) std::swap(value.right, value.left);
		if (value.bottom < value.top) std::swap(value.bottom, value.top);
		return value;
	}

	bool ContainsRoundedRectangle(
		D2D1_RECT_F rect,
		float radiusX,
		float radiusY,
		D2D1_POINT_2F point) noexcept
	{
		rect = NormalizedRect(rect);
		if (point.x < rect.left || point.x > rect.right
			|| point.y < rect.top || point.y > rect.bottom) return false;
		const float width = rect.right - rect.left;
		const float height = rect.bottom - rect.top;
		if (width <= 0.0f || height <= 0.0f) return false;
		radiusX = (std::min)(NonNegativeFinite(radiusX), width * 0.5f);
		radiusY = (std::min)(NonNegativeFinite(radiusY), height * 0.5f);
		if (radiusX <= 0.0f || radiusY <= 0.0f) return true;
		const float nearestX = point.x < rect.left + radiusX
			? rect.left + radiusX
			: point.x > rect.right - radiusX ? rect.right - radiusX : point.x;
		const float nearestY = point.y < rect.top + radiusY
			? rect.top + radiusY
			: point.y > rect.bottom - radiusY ? rect.bottom - radiusY : point.y;
		if (nearestX == point.x || nearestY == point.y) return true;
		const float dx = (point.x - nearestX) / radiusX;
		const float dy = (point.y - nearestY) / radiusY;
		return dx * dx + dy * dy <= 1.0f;
	}

	D2D1_POINT_2F CubicPoint(
		D2D1_POINT_2F start,
		D2D1_POINT_2F control1,
		D2D1_POINT_2F control2,
		D2D1_POINT_2F end,
		float t) noexcept
	{
		const float oneMinus = 1.0f - t;
		const float a = oneMinus * oneMinus * oneMinus;
		const float b = 3.0f * oneMinus * oneMinus * t;
		const float c = 3.0f * oneMinus * t * t;
		const float d = t * t * t;
		return D2D1::Point2F(
			a * start.x + b * control1.x + c * control2.x + d * end.x,
			a * start.y + b * control1.y + c * control2.y + d * end.y);
	}

	D2D1_POINT_2F QuadraticPoint(
		D2D1_POINT_2F start,
		D2D1_POINT_2F control,
		D2D1_POINT_2F end,
		float t) noexcept
	{
		const float oneMinus = 1.0f - t;
		return D2D1::Point2F(
			oneMinus * oneMinus * start.x
				+ 2.0f * oneMinus * t * control.x + t * t * end.x,
			oneMinus * oneMinus * start.y
				+ 2.0f * oneMinus * t * control.y + t * t * end.y);
	}

	float VectorAngle(float ux, float uy, float vx, float vy) noexcept
	{
		return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
	}

	void AppendArcPoints(
		std::vector<D2D1_POINT_2F>& points,
		D2D1_POINT_2F start,
		const PathSegment& segment)
	{
		const auto end = FinitePoint(segment.Point);
		float radiusX = NonNegativeFinite(segment.Size.width);
		float radiusY = NonNegativeFinite(segment.Size.height);
		if ((PointEquals(start, end)) || radiusX <= 0.0f || radiusY <= 0.0f)
		{
			if (!PointEquals(start, end)) points.push_back(end);
			return;
		}
		const float phi = FiniteOr(segment.RotationAngle) * Pi / 180.0f;
		const float cosine = std::cos(phi);
		const float sine = std::sin(phi);
		const float halfX = (start.x - end.x) * 0.5f;
		const float halfY = (start.y - end.y) * 0.5f;
		const float transformedX = cosine * halfX + sine * halfY;
		const float transformedY = -sine * halfX + cosine * halfY;
		const float scale = transformedX * transformedX / (radiusX * radiusX)
			+ transformedY * transformedY / (radiusY * radiusY);
		if (scale > 1.0f)
		{
			const float factor = std::sqrt(scale);
			radiusX *= factor;
			radiusY *= factor;
		}
		const float rxSquared = radiusX * radiusX;
		const float rySquared = radiusY * radiusY;
		const float xSquared = transformedX * transformedX;
		const float ySquared = transformedY * transformedY;
		const float denominator = rxSquared * ySquared + rySquared * xSquared;
		float coefficient = 0.0f;
		if (denominator > 0.0f)
		{
			const float numerator = (std::max)(0.0f,
				rxSquared * rySquared - rxSquared * ySquared - rySquared * xSquared);
			coefficient = std::sqrt(numerator / denominator);
			const bool clockwise = segment.Sweep == SweepDirection::Clockwise;
			if (segment.IsLargeArc == clockwise) coefficient = -coefficient;
		}
		const float centerPrimeX = coefficient * radiusX * transformedY / radiusY;
		const float centerPrimeY = -coefficient * radiusY * transformedX / radiusX;
		const float centerX = cosine * centerPrimeX - sine * centerPrimeY
			+ (start.x + end.x) * 0.5f;
		const float centerY = sine * centerPrimeX + cosine * centerPrimeY
			+ (start.y + end.y) * 0.5f;
		const float ux = (transformedX - centerPrimeX) / radiusX;
		const float uy = (transformedY - centerPrimeY) / radiusY;
		const float vx = (-transformedX - centerPrimeX) / radiusX;
		const float vy = (-transformedY - centerPrimeY) / radiusY;
		const float startAngle = VectorAngle(1.0f, 0.0f, ux, uy);
		float sweepAngle = VectorAngle(ux, uy, vx, vy);
		const bool clockwise = segment.Sweep == SweepDirection::Clockwise;
		if (!clockwise && sweepAngle > 0.0f) sweepAngle -= 2.0f * Pi;
		if (clockwise && sweepAngle < 0.0f) sweepAngle += 2.0f * Pi;
		const int steps = (std::clamp)(
			static_cast<int>(std::ceil(std::fabs(sweepAngle) / (Pi / 16.0f))), 1, 64);
		for (int index = 1; index <= steps; ++index)
		{
			const float angle = startAngle
				+ sweepAngle * static_cast<float>(index) / static_cast<float>(steps);
			const float x = radiusX * std::cos(angle);
			const float y = radiusY * std::sin(angle);
			points.push_back(D2D1::Point2F(
				cosine * x - sine * y + centerX,
				sine * x + cosine * y + centerY));
		}
		points.back() = end;
	}

	std::vector<D2D1_POINT_2F> FlattenFigure(const PathFigure& figure)
	{
		std::vector<D2D1_POINT_2F> points;
		points.push_back(FinitePoint(figure.StartPoint));
		for (const auto& segment : figure.Segments)
		{
			const auto start = points.back();
			if (segment.Kind == PathSegmentKind::Line)
				points.push_back(FinitePoint(segment.Point));
			else if (segment.Kind == PathSegmentKind::Bezier)
			{
				const auto control1 = FinitePoint(segment.Point1);
				const auto control2 = FinitePoint(segment.Point2);
				const auto end = FinitePoint(segment.Point3);
				for (int index = 1; index <= 24; ++index)
					points.push_back(CubicPoint(start, control1, control2, end,
						static_cast<float>(index) / 24.0f));
			}
			else if (segment.Kind == PathSegmentKind::QuadraticBezier)
			{
				const auto control = FinitePoint(segment.Point1);
				const auto end = FinitePoint(segment.Point2);
				for (int index = 1; index <= 20; ++index)
					points.push_back(QuadraticPoint(start, control, end,
						static_cast<float>(index) / 20.0f));
			}
			else AppendArcPoints(points, start, segment);
		}
		return points;
	}

	bool PointOnSegment(
		D2D1_POINT_2F point,
		D2D1_POINT_2F start,
		D2D1_POINT_2F end) noexcept
	{
		const float dx = end.x - start.x;
		const float dy = end.y - start.y;
		const float cross = (point.x - start.x) * dy - (point.y - start.y) * dx;
		const float tolerance = BoundaryEpsilon * (1.0f + std::fabs(dx) + std::fabs(dy));
		if (std::fabs(cross) > tolerance) return false;
		return point.x >= (std::min)(start.x, end.x) - BoundaryEpsilon
			&& point.x <= (std::max)(start.x, end.x) + BoundaryEpsilon
			&& point.y >= (std::min)(start.y, end.y) - BoundaryEpsilon
			&& point.y <= (std::max)(start.y, end.y) + BoundaryEpsilon;
	}

	bool ContainsPathPoint(const Geometry& geometry, D2D1_POINT_2F point) noexcept
	{
		bool parity = false;
		int winding = 0;
		for (const auto& figure : geometry.Figures)
		{
			if (!figure.IsFilled) continue;
			const auto points = FlattenFigure(figure);
			if (points.size() < 2) continue;
			for (size_t index = 0; index < points.size(); ++index)
			{
				const auto start = points[index];
				const auto end = points[(index + 1) % points.size()];
				if (PointOnSegment(point, start, end)) return true;
				const bool crosses = (start.y <= point.y && end.y > point.y)
					|| (start.y > point.y && end.y <= point.y);
				if (!crosses) continue;
				const float intersectionX = start.x
					+ (point.y - start.y) * (end.x - start.x) / (end.y - start.y);
				if (intersectionX <= point.x) continue;
				parity = !parity;
				winding += end.y > start.y ? 1 : -1;
			}
		}
		return geometry.FillRule == GeometryFillRule::Nonzero
			? winding != 0 : parity;
	}

	ComPtr<ID2D1Geometry> ApplyTransform(
		ComPtr<ID2D1Geometry> source,
		const D2D1_MATRIX_3X2_F& transform)
	{
		if (!source) return {};
		auto* factory = Factory::D2DFactory();
		if (!factory) return {};
		ComPtr<ID2D1TransformedGeometry> transformed;
		if (FAILED(factory->CreateTransformedGeometry(
			source.Get(), &transform, &transformed))) return {};
		ComPtr<ID2D1Geometry> result;
		transformed.As(&result);
		return result;
	}

	ComPtr<ID2D1Geometry> CreateNativeGeometry(const Geometry& value)
	{
		ComPtr<ID2D1Geometry> result;
		auto* factory = Factory::D2DFactory();
		if (!factory) return result;
		if (value.Kind == GeometryKind::Rectangle)
		{
			const auto rect = NormalizedRect(value.Rect);
			const float width = rect.right - rect.left;
			const float height = rect.bottom - rect.top;
			const float radiusX = (std::min)(
				NonNegativeFinite(value.RadiusX), width * 0.5f);
			const float radiusY = (std::min)(
				NonNegativeFinite(value.RadiusY), height * 0.5f);
			if (radiusX > 0.0f && radiusY > 0.0f)
			{
				ComPtr<ID2D1RoundedRectangleGeometry> rounded;
				if (SUCCEEDED(factory->CreateRoundedRectangleGeometry(
					D2D1::RoundedRect(rect, radiusX, radiusY), &rounded)))
					rounded.As(&result);
			}
			else
			{
				ComPtr<ID2D1RectangleGeometry> rectangle;
				if (SUCCEEDED(factory->CreateRectangleGeometry(rect, &rectangle)))
					rectangle.As(&result);
			}
		}
		else if (value.Kind == GeometryKind::Ellipse)
		{
			ComPtr<ID2D1EllipseGeometry> ellipse;
			if (SUCCEEDED(factory->CreateEllipseGeometry(
				D2D1::Ellipse(FinitePoint(value.Center),
					NonNegativeFinite(value.RadiusX),
					NonNegativeFinite(value.RadiusY)), &ellipse)))
				ellipse.As(&result);
		}
		else if (value.Kind == GeometryKind::Path)
		{
			ComPtr<ID2D1PathGeometry> path;
			if (FAILED(factory->CreatePathGeometry(&path))) return {};
			ComPtr<ID2D1GeometrySink> sink;
			if (FAILED(path->Open(&sink))) return {};
			sink->SetFillMode(value.FillRule == GeometryFillRule::Nonzero
				? D2D1_FILL_MODE_WINDING : D2D1_FILL_MODE_ALTERNATE);
			for (const auto& figure : value.Figures)
			{
				sink->BeginFigure(FinitePoint(figure.StartPoint), figure.IsFilled
					? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
				for (const auto& segment : figure.Segments)
				{
					if (segment.Kind == PathSegmentKind::Line)
						sink->AddLine(FinitePoint(segment.Point));
					else if (segment.Kind == PathSegmentKind::Bezier)
						sink->AddBezier(D2D1::BezierSegment(
							FinitePoint(segment.Point1), FinitePoint(segment.Point2),
							FinitePoint(segment.Point3)));
					else if (segment.Kind == PathSegmentKind::QuadraticBezier)
						sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
							FinitePoint(segment.Point1), FinitePoint(segment.Point2)));
					else
						sink->AddArc(D2D1::ArcSegment(
							FinitePoint(segment.Point),
							D2D1::SizeF(NonNegativeFinite(segment.Size.width),
								NonNegativeFinite(segment.Size.height)),
							FiniteOr(segment.RotationAngle),
							segment.Sweep == SweepDirection::Clockwise
								? D2D1_SWEEP_DIRECTION_CLOCKWISE
								: D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
							segment.IsLargeArc ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
				}
				sink->EndFigure(figure.IsClosed
					? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
			}
			if (FAILED(sink->Close())) return {};
			path.As(&result);
		}
		else
		{
			std::vector<ComPtr<ID2D1Geometry>> owned;
			std::vector<ID2D1Geometry*> children;
			owned.reserve(value.Children.size());
			children.reserve(value.Children.size());
			for (const auto& child : value.Children)
			{
				auto native = CreateNativeGeometry(child);
				if (!native) return {};
				children.push_back(native.Get());
				owned.push_back(std::move(native));
			}
			ComPtr<ID2D1GeometryGroup> group;
			if (FAILED(factory->CreateGeometryGroup(
				value.FillRule == GeometryFillRule::Nonzero
					? D2D1_FILL_MODE_WINDING : D2D1_FILL_MODE_ALTERNATE,
				children.data(), static_cast<UINT32>(children.size()), &group)))
				return {};
			group.As(&result);
		}
		if (result && value.LocalTransform && !value.LocalTransform->Empty())
			result = ApplyTransform(std::move(result),
				value.LocalTransform->ToMatrix(D2D1::SizeF()));
		return result;
	}
}

bool PathSegment::operator==(const PathSegment& other) const noexcept
{
	return Kind == other.Kind && PointEquals(Point, other.Point)
		&& PointEquals(Point1, other.Point1) && PointEquals(Point2, other.Point2)
		&& PointEquals(Point3, other.Point3) && SizeEquals(Size, other.Size)
		&& RotationAngle == other.RotationAngle && IsLargeArc == other.IsLargeArc
		&& Sweep == other.Sweep;
}

bool PathFigure::operator==(const PathFigure& other) const noexcept
{
	return PointEquals(StartPoint, other.StartPoint)
		&& IsClosed == other.IsClosed && IsFilled == other.IsFilled
		&& Segments == other.Segments;
}

bool Geometry::operator==(const Geometry& other) const noexcept
{
	return Kind == other.Kind && FillRule == other.FillRule
		&& RectEquals(Rect, other.Rect) && PointEquals(Center, other.Center)
		&& RadiusX == other.RadiusX && RadiusY == other.RadiusY
		&& Figures == other.Figures && Children == other.Children
		&& LocalTransform == other.LocalTransform;
}

bool Geometry::ContainsPoint(D2D1_POINT_2F point) const noexcept
{
	if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
	if (LocalTransform && !LocalTransform->Empty())
	{
		auto inverseValue = LocalTransform->ToMatrix(D2D1::SizeF());
		auto inverse = D2D1::Matrix3x2F(
			inverseValue._11, inverseValue._12, inverseValue._21,
			inverseValue._22, inverseValue._31, inverseValue._32);
		if (!inverse.Invert()) return false;
		point = inverse.TransformPoint(point);
	}
	if (Kind == GeometryKind::Rectangle)
		return ContainsRoundedRectangle(Rect, RadiusX, RadiusY, point);
	if (Kind == GeometryKind::Ellipse)
	{
		const float radiusX = NonNegativeFinite(RadiusX);
		const float radiusY = NonNegativeFinite(RadiusY);
		if (radiusX <= 0.0f || radiusY <= 0.0f) return false;
		const float dx = (point.x - Center.x) / radiusX;
		const float dy = (point.y - Center.y) / radiusY;
		return dx * dx + dy * dy <= 1.0f;
	}
	if (Kind == GeometryKind::Path) return ContainsPathPoint(*this, point);
	if (FillRule == GeometryFillRule::Nonzero)
	{
		for (const auto& child : Children)
			if (child.ContainsPoint(point)) return true;
		return false;
	}
	bool inside = false;
	for (const auto& child : Children)
		if (child.ContainsPoint(point)) inside = !inside;
	return inside;
}

ID2D1Geometry* Geometry::CreateD2DGeometry(
	const D2D1_MATRIX_3X2_F* transform) const
{
	auto native = CreateNativeGeometry(*this);
	if (!native || !transform) return native.Detach();
	return ApplyTransform(std::move(native), *transform).Detach();
}
}
