#include "Transform.h"

#include <algorithm>
#include <cmath>

namespace cui::drawing
{
namespace
{
	float FiniteOr(float value, float fallback) noexcept
	{
		return std::isfinite(value) ? value : fallback;
	}

	bool MatrixEquals(
		const D2D1_MATRIX_3X2_F& left,
		const D2D1_MATRIX_3X2_F& right) noexcept
	{
		return left._11 == right._11 && left._12 == right._12
			&& left._21 == right._21 && left._22 == right._22
			&& left._31 == right._31 && left._32 == right._32;
	}

	D2D1::Matrix3x2F AsMatrix(const D2D1_MATRIX_3X2_F& value) noexcept
	{
		return D2D1::Matrix3x2F(
			value._11, value._12, value._21,
			value._22, value._31, value._32);
	}
}

bool TransformOperation::operator==(
	const TransformOperation& other) const noexcept
{
	return Kind == other.Kind && MatrixEquals(Matrix, other.Matrix)
		&& X == other.X && Y == other.Y
		&& ScaleX == other.ScaleX && ScaleY == other.ScaleY
		&& Angle == other.Angle
		&& AngleX == other.AngleX && AngleY == other.AngleY
		&& CenterX == other.CenterX && CenterY == other.CenterY;
}

D2D1_MATRIX_3X2_F TransformOperation::ToMatrix() const noexcept
{
	const auto center = D2D1::Point2F(
		FiniteOr(CenterX, 0.0f), FiniteOr(CenterY, 0.0f));
	switch (Kind)
	{
	case TransformKind::Translate:
		return D2D1::Matrix3x2F::Translation(
			FiniteOr(X, 0.0f), FiniteOr(Y, 0.0f));
	case TransformKind::Scale:
		return D2D1::Matrix3x2F::Scale(
			FiniteOr(ScaleX, 1.0f), FiniteOr(ScaleY, 1.0f), center);
	case TransformKind::Rotate:
		return D2D1::Matrix3x2F::Rotation(
			FiniteOr(Angle, 0.0f), center);
	case TransformKind::Skew:
		return D2D1::Matrix3x2F::Skew(
			FiniteOr(AngleX, 0.0f), FiniteOr(AngleY, 0.0f), center);
	case TransformKind::Matrix:
	default:
		return D2D1::Matrix3x2F(
			FiniteOr(Matrix._11, 1.0f), FiniteOr(Matrix._12, 0.0f),
			FiniteOr(Matrix._21, 0.0f), FiniteOr(Matrix._22, 1.0f),
			FiniteOr(Matrix._31, 0.0f), FiniteOr(Matrix._32, 0.0f));
	}
}

D2D1_MATRIX_3X2_F Transform::ToMatrix(
	D2D1_SIZE_F bounds,
	D2D1_POINT_2F origin) const noexcept
{
	auto result = D2D1::Matrix3x2F::Identity();
	for (const auto& operation : Operations)
		result = result * AsMatrix(operation.ToMatrix());

	const float originX = FiniteOr(origin.x, 0.0f)
		* (std::max)(0.0f, FiniteOr(bounds.width, 0.0f));
	const float originY = FiniteOr(origin.y, 0.0f)
		* (std::max)(0.0f, FiniteOr(bounds.height, 0.0f));
	if (originX == 0.0f && originY == 0.0f) return result;
	return D2D1::Matrix3x2F::Translation(-originX, -originY)
		* result
		* D2D1::Matrix3x2F::Translation(originX, originY);
}
}
