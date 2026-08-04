#pragma once

#include "DesignerModel/DesignValue.h"
#include <string>

/** Serializable literal kinds shared by styles and ordinary metadata properties. */
enum class DesignerStyleValueKind
{
	Bool,
	Int,
	Int64,
	Float,
	Double,
	String,
	Color,
	Thickness,
	Point,
	Vector,
	Rect,
	Size,
	Matrix,
	Length,
	ImageSource,
	Brush,
	Geometry,
	Transform,
	/** Appended to preserve the serialized numeric values of existing kinds. */
	CornerRadius,
	/** WPF Nullable<Boolean>, including the canonical {x:Null} literal. */
	NullableBool
};

/** A strongly typed value whose text remains editable and XML-friendly. */
struct DesignerStyleValue
{
	DesignerStyleValueKind Kind = DesignerStyleValueKind::String;
	std::wstring Text;
	/** Structured payload for object-valued literals such as Brush or Geometry. */
	DesignerModel::DesignValue ObjectValue;

	bool operator==(const DesignerStyleValue&) const = default;
};
