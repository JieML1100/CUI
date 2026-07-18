#pragma once

#include <d2d1.h>
#include <vector>

namespace cui::drawing
{
enum class TransformKind
{
	Matrix,
	Translate,
	Scale,
	Rotate,
	Skew
};

/** One operation in a TransformGroup, evaluated in declaration order. */
struct TransformOperation
{
	TransformKind Kind = TransformKind::Matrix;
	D2D1_MATRIX_3X2_F Matrix = D2D1::Matrix3x2F::Identity();
	float X = 0.0f;
	float Y = 0.0f;
	float ScaleX = 1.0f;
	float ScaleY = 1.0f;
	float Angle = 0.0f;
	float AngleX = 0.0f;
	float AngleY = 0.0f;
	float CenterX = 0.0f;
	float CenterY = 0.0f;

	bool operator==(const TransformOperation& other) const noexcept;
	D2D1_MATRIX_3X2_F ToMatrix() const noexcept;
};

/**
 * Device-independent render transform used by controls and XAML.
 * Origin is relative to the control bounds, matching RenderTransformOrigin.
 */
struct Transform
{
	std::vector<TransformOperation> Operations;

	bool Empty() const noexcept { return Operations.empty(); }
	bool operator==(const Transform&) const noexcept = default;
	D2D1_MATRIX_3X2_F ToMatrix(
		D2D1_SIZE_F bounds,
		D2D1_POINT_2F origin = D2D1::Point2F()) const noexcept;
};
}
