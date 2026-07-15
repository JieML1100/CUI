#pragma once

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
	Length
};

/** A strongly typed value whose text remains editable and XML-friendly. */
struct DesignerStyleValue
{
	DesignerStyleValueKind Kind = DesignerStyleValueKind::String;
	std::wstring Text;

	bool operator==(const DesignerStyleValue&) const = default;
};

