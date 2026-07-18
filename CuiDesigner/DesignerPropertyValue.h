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
	Size,
	Length,
	ImageSource,
	Brush,
	Geometry,
	Transform
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
