#include "DesignerPropertyCatalog.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerStyleSheetUtils.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include "../CUI/include/DependencyPropertyInfrastructure.h"
#include "../D2DGraphics/include/BitmapSource.h"
#include <Convert.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <typeindex>

namespace DesignerPropertyCatalog
{
namespace
{
	bool IsCompatibleStyleValueKind(
		DesignerStyleValueKind expected,
		DesignerStyleValueKind actual) noexcept
	{
		return expected == actual
			|| (expected == DesignerStyleValueKind::Brush
				&& actual == DesignerStyleValueKind::Color)
			|| (expected == DesignerStyleValueKind::NullableBool
				&& actual == DesignerStyleValueKind::Bool);
	}

	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	std::wstring NumberText(double value, int precision = 7)
	{
		std::wostringstream stream;
		stream << std::setprecision(precision) << value;
		return stream.str();
	}

	unsigned int ColorByte(float value)
	{
		const auto normalized = (std::max)(0.0f, (std::min)(1.0f, value));
		return static_cast<unsigned int>(std::lround(normalized * 255.0f));
	}

	std::wstring FormatValue(
		const BindingValue& value,
		DesignerStyleValueKind kind)
	{
		switch (kind)
		{
		case DesignerStyleValueKind::Bool:
		{
			bool typed = false;
			return value.TryGet(typed) && typed ? L"true" : L"false";
		}
		case DesignerStyleValueKind::NullableBool:
		{
			NullableBool typed;
			if (!value.TryGet(typed) || !typed.HasValue())
				return L"{x:Null}";
			return typed.GetValueOrDefault() ? L"true" : L"false";
		}
		case DesignerStyleValueKind::Int:
		{
			int typed = 0;
			return value.TryGet(typed) ? std::to_wstring(typed) : L"0";
		}
		case DesignerStyleValueKind::Int64:
		{
			long long typed = 0;
			return value.TryGet(typed) ? std::to_wstring(typed) : L"0";
		}
		case DesignerStyleValueKind::Float:
		{
			float typed = 0.0f;
			return value.TryGet(typed) ? NumberText(typed) : L"0";
		}
		case DesignerStyleValueKind::Double:
		{
			double typed = 0.0;
			return value.TryGet(typed) ? NumberText(typed, 15) : L"0";
		}
		case DesignerStyleValueKind::String:
		{
			std::wstring typed;
			return value.TryGet(typed) ? typed : L"";
		}
		case DesignerStyleValueKind::Color:
		{
			D2D1_COLOR_F typed{};
			if (!value.TryGet(typed)) return L"#FF0078D4";
			wchar_t text[10]{};
			swprintf_s(text, L"#%02X%02X%02X%02X",
				ColorByte(typed.a), ColorByte(typed.r),
				ColorByte(typed.g), ColorByte(typed.b));
			return text;
		}
		case DesignerStyleValueKind::Thickness:
		{
			Thickness typed;
			if (!value.TryGet(typed)) return L"0";
			if (typed.Left == typed.Top && typed.Left == typed.Right
				&& typed.Left == typed.Bottom)
				return NumberText(typed.Left);
			if (typed.Left == typed.Right && typed.Top == typed.Bottom)
				return NumberText(typed.Left) + L", " + NumberText(typed.Top);
			return NumberText(typed.Left) + L", " + NumberText(typed.Top)
				+ L", " + NumberText(typed.Right) + L", " + NumberText(typed.Bottom);
		}
		case DesignerStyleValueKind::CornerRadius:
		{
			::CornerRadius typed;
			if (!value.TryGet(typed)) return L"0";
			if (typed.TopLeft == typed.TopRight
				&& typed.TopLeft == typed.BottomRight
				&& typed.TopLeft == typed.BottomLeft)
				return NumberText(typed.TopLeft);
			return NumberText(typed.TopLeft) + L", "
				+ NumberText(typed.TopRight) + L", "
				+ NumberText(typed.BottomRight) + L", "
				+ NumberText(typed.BottomLeft);
		}
		case DesignerStyleValueKind::Point:
		{
			cui::core::Point typed{};
			return value.TryGet(typed)
				? NumberText(typed.x,
					std::numeric_limits<float>::max_digits10) + L", "
					+ NumberText(typed.y,
						std::numeric_limits<float>::max_digits10)
				: L"0, 0";
		}
		case DesignerStyleValueKind::Vector:
		{
			cui::core::Vector typed{};
			return value.TryGet(typed)
				? NumberText(typed.x,
					std::numeric_limits<float>::max_digits10) + L", "
					+ NumberText(typed.y,
						std::numeric_limits<float>::max_digits10)
				: L"0, 0";
		}
		case DesignerStyleValueKind::Rect:
		{
			cui::core::Rect typed{};
			return value.TryGet(typed)
				? NumberText(typed.x,
					std::numeric_limits<float>::max_digits10) + L", "
					+ NumberText(typed.y,
						std::numeric_limits<float>::max_digits10) + L", "
					+ NumberText(typed.width,
						std::numeric_limits<float>::max_digits10) + L", "
					+ NumberText(typed.height,
						std::numeric_limits<float>::max_digits10)
				: L"0, 0, 0, 0";
		}
		case DesignerStyleValueKind::Size:
		{
			cui::core::Size typed{};
			return value.TryGet(typed)
				? NumberText(typed.width,
					std::numeric_limits<float>::max_digits10) + L", "
					+ NumberText(typed.height,
						std::numeric_limits<float>::max_digits10)
				: L"0, 0";
		}
		case DesignerStyleValueKind::Matrix:
		{
			D2D1_MATRIX_3X2_F typed{};
			if (!value.TryGet(typed)) return L"1, 0, 0, 1, 0, 0";
			return NumberText(typed._11, std::numeric_limits<float>::max_digits10)
				+ L", " + NumberText(typed._12, std::numeric_limits<float>::max_digits10)
				+ L", " + NumberText(typed._21, std::numeric_limits<float>::max_digits10)
				+ L", " + NumberText(typed._22, std::numeric_limits<float>::max_digits10)
				+ L", " + NumberText(typed._31, std::numeric_limits<float>::max_digits10)
				+ L", " + NumberText(typed._32, std::numeric_limits<float>::max_digits10);
		}
		case DesignerStyleValueKind::Length:
		{
			cui::layout::Length typed;
			if (!value.TryGet(typed) || typed.IsAuto()) return L"Auto";
			return NumberText(typed.value);
		}
		case DesignerStyleValueKind::ImageSource:
		{
			std::shared_ptr<BitmapSource> typed;
			return value.TryGet(typed) && typed ? typed->GetSourceUri() : L"";
		}
		case DesignerStyleValueKind::Brush:
			return L"{Brush}";
		case DesignerStyleValueKind::Geometry:
			return L"{Geometry}";
		case DesignerStyleValueKind::Transform:
			return L"{Transform}";
		}
		return L"";
	}

	DesignerModel::DesignValue ColorToValue(const D2D1_COLOR_F& color)
	{
		return DesignerModel::DesignValue{
			{ "r", color.r }, { "g", color.g },
			{ "b", color.b }, { "a", color.a } };
	}

	DesignerModel::DesignValue TransformToValue(
		const cui::drawing::Transform& transform)
	{
		DesignerModel::DesignValue value = DesignerModel::DesignValue::array();
		for (const auto& operation : transform.Operations)
		{
			DesignerModel::DesignValue item = DesignerModel::DesignValue::object();
			switch (operation.Kind)
			{
			case cui::drawing::TransformKind::Matrix:
				item = {
					{ "type", "matrix" },
					{ "m11", operation.Matrix._11 },
					{ "m12", operation.Matrix._12 },
					{ "m21", operation.Matrix._21 },
					{ "m22", operation.Matrix._22 },
					{ "dx", operation.Matrix._31 },
					{ "dy", operation.Matrix._32 }
				};
				break;
			case cui::drawing::TransformKind::Translate:
				item = {
					{ "type", "translate" },
					{ "x", operation.X }, { "y", operation.Y }
				};
				break;
			case cui::drawing::TransformKind::Scale:
				item = {
					{ "type", "scale" },
					{ "scaleX", operation.ScaleX },
					{ "scaleY", operation.ScaleY },
					{ "centerX", operation.CenterX },
					{ "centerY", operation.CenterY }
				};
				break;
			case cui::drawing::TransformKind::Rotate:
				item = {
					{ "type", "rotate" },
					{ "angle", operation.Angle },
					{ "centerX", operation.CenterX },
					{ "centerY", operation.CenterY }
				};
				break;
			case cui::drawing::TransformKind::Skew:
				item = {
					{ "type", "skew" },
					{ "angleX", operation.AngleX },
					{ "angleY", operation.AngleY },
					{ "centerX", operation.CenterX },
					{ "centerY", operation.CenterY }
				};
				break;
			}
			value.push_back(std::move(item));
		}
		return value;
	}

	DesignerModel::DesignValue BrushToValue(const cui::drawing::Brush& brush)
	{
		DesignerModel::DesignValue value = DesignerModel::DesignValue::object();
		value["type"] = brush.Kind == cui::drawing::BrushKind::None ? "none"
			: brush.Kind == cui::drawing::BrushKind::Solid ? "solid"
			: brush.Kind == cui::drawing::BrushKind::LinearGradient ? "linear"
			: brush.Kind == cui::drawing::BrushKind::RadialGradient ? "radial"
			: "image";
		if (brush.Kind == cui::drawing::BrushKind::None) return value;
		value["mapping"] = brush.MappingMode
			== cui::drawing::BrushMappingMode::Absolute ? "absolute" : "relative";
		value["opacity"] = brush.Opacity;
		if (brush.Transform)
			value["transform"] = TransformToValue(*brush.Transform);
		if (brush.RelativeTransform)
			value["relativeTransform"] = TransformToValue(*brush.RelativeTransform);
		if (brush.Kind == cui::drawing::BrushKind::Solid)
			value["color"] = ColorToValue(brush.Color);
		else if (brush.Kind == cui::drawing::BrushKind::Image)
		{
			value["source"] = brush.ImageSource
				? Convert::UnicodeToUtf8(brush.ImageSource->GetSourceUri()) : std::string{};
			value["stretch"] = brush.Stretch == ::Stretch::None
				? "none" : brush.Stretch == ::Stretch::Uniform
					? "uniform" : brush.Stretch == ::Stretch::UniformToFill
						? "uniformToFill" : "fill";
			value["alignmentX"] = brush.AlignmentX == cui::drawing::ImageBrushAlignmentX::Left
				? "left" : brush.AlignmentX == cui::drawing::ImageBrushAlignmentX::Right
					? "right" : "center";
			value["alignmentY"] = brush.AlignmentY == cui::drawing::ImageBrushAlignmentY::Top
				? "top" : brush.AlignmentY == cui::drawing::ImageBrushAlignmentY::Bottom
					? "bottom" : "center";
		}
		else
		{
			if (brush.Kind == cui::drawing::BrushKind::LinearGradient)
			{
				value["startX"] = brush.StartPoint.x;
				value["startY"] = brush.StartPoint.y;
				value["endX"] = brush.EndPoint.x;
				value["endY"] = brush.EndPoint.y;
			}
			else
			{
				value["centerX"] = brush.Center.x;
				value["centerY"] = brush.Center.y;
				value["originX"] = brush.GradientOrigin.x;
				value["originY"] = brush.GradientOrigin.y;
				value["radiusX"] = brush.RadiusX;
				value["radiusY"] = brush.RadiusY;
			}
			DesignerModel::DesignValue stops = DesignerModel::DesignValue::array();
			for (const auto& stop : brush.GradientStops)
				stops.push_back(DesignerModel::DesignValue{
					{ "offset", stop.Offset }, { "color", ColorToValue(stop.Color) } });
			value["stops"] = std::move(stops);
		}
		return value;
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	void EraseTrackedValues(
		TrackedPropertyValues& values,
		const std::wstring& name)
	{
		for (auto current = values.begin(); current != values.end();)
		{
			if (EqualsName(current->first, name)) current = values.erase(current);
			else ++current;
		}
	}

	void SynchronizeTrackedValue(
		TrackedPropertyValues& values,
		const DependencyPropertyMetadata& metadata,
		const std::wstring& canonicalName,
		const DesignerStyleValue* effectiveValue)
	{
		EraseTrackedValues(values, canonicalName);
		if (!UsesMetadataPersistence(metadata) || !effectiveValue)
			return;
		values[canonicalName] = *effectiveValue;
	}

	DependencyPropertyEditorKind ResolveEditor(
		DependencyPropertyEditorKind requested,
		DesignerStyleValueKind kind,
		bool hasChoices)
	{
		if (requested != DependencyPropertyEditorKind::Auto) return requested;
		if (hasChoices) return DependencyPropertyEditorKind::Choice;
		switch (kind)
		{
		case DesignerStyleValueKind::Bool:
		case DesignerStyleValueKind::NullableBool:
			return DependencyPropertyEditorKind::Boolean;
		case DesignerStyleValueKind::Int:
		case DesignerStyleValueKind::Int64:
		case DesignerStyleValueKind::Float:
		case DesignerStyleValueKind::Double:
			return DependencyPropertyEditorKind::Number;
		case DesignerStyleValueKind::Color: return DependencyPropertyEditorKind::Color;
		case DesignerStyleValueKind::Thickness: return DependencyPropertyEditorKind::Thickness;
		case DesignerStyleValueKind::CornerRadius: return DependencyPropertyEditorKind::Text;
		case DesignerStyleValueKind::Point: return DependencyPropertyEditorKind::Text;
		case DesignerStyleValueKind::Vector: return DependencyPropertyEditorKind::Text;
		case DesignerStyleValueKind::Rect: return DependencyPropertyEditorKind::Text;
		case DesignerStyleValueKind::Size: return DependencyPropertyEditorKind::Size;
		case DesignerStyleValueKind::Matrix: return DependencyPropertyEditorKind::Text;
		case DesignerStyleValueKind::Length: return DependencyPropertyEditorKind::Length;
		case DesignerStyleValueKind::String:
		case DesignerStyleValueKind::ImageSource:
		case DesignerStyleValueKind::Brush:
		case DesignerStyleValueKind::Geometry:
		case DesignerStyleValueKind::Transform:
		default:
			return DependencyPropertyEditorKind::Text;
		}
	}

	void AppendIntrinsicChoices(
		DesignerStyleValueKind kind,
		std::vector<DesignerPropertyDescriptor::Choice>& choices)
	{
		if (kind != DesignerStyleValueKind::NullableBool
			|| !choices.empty()) return;
		choices = {
			{ L"False", L"false" },
			{ L"True", L"true" },
			{ L"Indeterminate", L"{x:Null}" }
		};
	}

	bool TryCreateDescriptor(
		Control& target,
		const DependencyPropertyMetadata& metadata,
		DesignerPropertyDescriptor& out)
	{
		if (!metadata.CanRead()) return false;
		DesignerStyleValueKind kind;
		if (!TryGetStyleValueKind(metadata, kind)) return false;

		BindingValue sample;
		if (!metadata.TryGet(target, sample))
			(void)metadata.TryGetDefaultValue(sample);
		const auto& design = metadata.Design();
		out.Name = metadata.Name();
		out.DisplayName = design.DisplayName.empty()
			? metadata.Name() : design.DisplayName;
		out.Category = design.Category.empty() ? L"Misc" : design.Category;
		out.CategoryOrder = design.CategoryOrder;
		out.Order = design.Order;
		out.ValueKind = kind;
		out.SampleValue = FormatValue(sample, kind);
		out.Minimum = design.Minimum;
		out.Maximum = design.Maximum;
		out.Step = design.Step;
		out.Persistence = design.Persistence;
		out.Metadata = &metadata;
		for (const auto& choice : design.Choices)
		{
			BindingValue converted;
			BindingValue effective;
			if (!metadata.TryConvert(choice.Value, converted)
				|| !metadata.TryCoerce(target, converted, effective)) continue;
			out.Choices.push_back({
				choice.DisplayName,
				FormatValue(effective, kind) });
		}
		AppendIntrinsicChoices(kind, out.Choices);
		out.Editor = ResolveEditor(design.Editor, kind, !out.Choices.empty());
		return true;
	}

	bool TryCreateDescriptor(
		const DependencyPropertyMetadata& metadata,
		DesignerPropertyDescriptor& out)
	{
		if (!metadata.CanRead()) return false;
		DesignerStyleValueKind kind;
		if (!TryGetStyleValueKind(metadata, kind)) return false;

		BindingValue sample;
		(void)metadata.TryGetDefaultValue(sample);
		const auto& design = metadata.Design();
		out.Name = metadata.Name();
		out.DisplayName = design.DisplayName.empty()
			? metadata.Name() : design.DisplayName;
		out.Category = design.Category.empty() ? L"Misc" : design.Category;
		out.CategoryOrder = design.CategoryOrder;
		out.Order = design.Order;
		out.ValueKind = kind;
		out.SampleValue = FormatValue(sample, kind);
		out.Minimum = design.Minimum;
		out.Maximum = design.Maximum;
		out.Step = design.Step;
		out.Persistence = design.Persistence;
		out.Metadata = &metadata;
		for (const auto& choice : design.Choices)
		{
			BindingValue converted;
			if (!metadata.TryConvert(choice.Value, converted)) continue;
			out.Choices.push_back({
				choice.DisplayName,
				FormatValue(converted, kind) });
		}
		AppendIntrinsicChoices(kind, out.Choices);
		out.Editor = ResolveEditor(design.Editor, kind, !out.Choices.empty());
		return true;
	}

	void SortBrowsableProperties(std::vector<DesignerPropertyDescriptor>& properties)
	{
		std::sort(properties.begin(), properties.end(), [](const auto& left, const auto& right)
		{
			if (left.CategoryOrder != right.CategoryOrder)
				return left.CategoryOrder < right.CategoryOrder;
			const auto leftCategory = Lower(left.Category);
			const auto rightCategory = Lower(right.Category);
			if (leftCategory != rightCategory) return leftCategory < rightCategory;
			if (left.Order != right.Order) return left.Order < right.Order;
			const auto leftDisplay = Lower(left.DisplayName);
			const auto rightDisplay = Lower(right.DisplayName);
			if (leftDisplay != rightDisplay) return leftDisplay < rightDisplay;
			return Lower(left.Name) < Lower(right.Name);
		});
	}
}

bool TryGetStyleValueKind(
	const DependencyPropertyMetadata& metadata,
	DesignerStyleValueKind& out)
{
	switch (metadata.ValueKind())
	{
	case BindingValueKind::Bool: out = DesignerStyleValueKind::Bool; return true;
	case BindingValueKind::NullableBool:
		out = DesignerStyleValueKind::NullableBool;
		return true;
	case BindingValueKind::Int: out = DesignerStyleValueKind::Int; return true;
	case BindingValueKind::Int64: out = DesignerStyleValueKind::Int64; return true;
	case BindingValueKind::Float: out = DesignerStyleValueKind::Float; return true;
	case BindingValueKind::Double: out = DesignerStyleValueKind::Double; return true;
	case BindingValueKind::String: out = DesignerStyleValueKind::String; return true;
	case BindingValueKind::Object:
		break;
	case BindingValueKind::Empty:
	default:
		return false;
	}

	const auto& type = metadata.ValueType();
	if (type == std::type_index(typeid(D2D1_COLOR_F)))
		out = DesignerStyleValueKind::Color;
	else if (type == std::type_index(typeid(Thickness)))
		out = DesignerStyleValueKind::Thickness;
	else if (type == std::type_index(typeid(::CornerRadius)))
		out = DesignerStyleValueKind::CornerRadius;
	else if (type == std::type_index(typeid(cui::core::Point)))
		out = DesignerStyleValueKind::Point;
	else if (type == std::type_index(typeid(cui::core::Vector)))
		out = DesignerStyleValueKind::Vector;
	else if (type == std::type_index(typeid(cui::core::Rect)))
		out = DesignerStyleValueKind::Rect;
	else if (type == std::type_index(typeid(cui::core::Size)))
		out = DesignerStyleValueKind::Size;
	else if (type == std::type_index(typeid(D2D1_MATRIX_3X2_F)))
		out = DesignerStyleValueKind::Matrix;
	else if (type == std::type_index(typeid(cui::layout::Length)))
		out = DesignerStyleValueKind::Length;
	else if (type == std::type_index(typeid(std::shared_ptr<BitmapSource>)))
		out = DesignerStyleValueKind::ImageSource;
	else if (type == std::type_index(typeid(cui::drawing::Brush)))
		out = DesignerStyleValueKind::Brush;
	else if (type == std::type_index(typeid(cui::drawing::Geometry)))
		out = DesignerStyleValueKind::Geometry;
	else if (type == std::type_index(typeid(cui::drawing::Transform)))
		out = DesignerStyleValueKind::Transform;
	else if (type == std::type_index(typeid(BindingValue)))
		// ContentControl.Content is object-typed at runtime, but an authored
		// attribute literal is still canonical scalar text. Structured visual or
		// data content travels through the content/template model, not this slot.
		out = DesignerStyleValueKind::String;
	else
		return false;
	return true;
}

std::vector<DesignerPropertyDescriptor> GetStyleProperties(Control& target)
{
	std::vector<DesignerPropertyDescriptor> result;
	for (const auto* metadata : DependencyPropertyRegistry::GetProperties(target))
	{
		if (!metadata || !metadata->CanWrite()) continue;
		DesignerPropertyDescriptor property;
		if (TryCreateDescriptor(target, *metadata, property))
			result.push_back(std::move(property));
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

std::vector<DesignerPropertyDescriptor> GetStyleProperties(
	std::span<const DependencyPropertyMetadata* const> properties)
{
	std::vector<DesignerPropertyDescriptor> result;
	for (const auto* metadata : properties)
	{
		if (!metadata || !metadata->CanWrite()) continue;
		DesignerPropertyDescriptor property;
		if (TryCreateDescriptor(*metadata, property))
			result.push_back(std::move(property));
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

bool TryGetStyleProperty(
	Control& target,
	const std::wstring& propertyName,
	DesignerPropertyDescriptor& out)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	return metadata && metadata->CanWrite()
		&& TryCreateDescriptor(target, *metadata, out);
}

bool TryGetStyleProperty(
	std::span<const DependencyPropertyMetadata* const> properties,
	const std::wstring& propertyName,
	DesignerPropertyDescriptor& out)
{
	const auto found = std::find_if(properties.begin(), properties.end(),
		[&](const DependencyPropertyMetadata* metadata)
		{
			return metadata && EqualsName(metadata->Name(), propertyName);
		});
	return found != properties.end() && (*found)->CanWrite()
		&& TryCreateDescriptor(**found, out);
}

std::vector<DesignerPropertyDescriptor> GetConditionProperties(Control& target)
{
	std::vector<DesignerPropertyDescriptor> result;
	for (const auto* metadata : DependencyPropertyRegistry::GetProperties(target))
	{
		if (!metadata || !metadata->CanRead() || !metadata->CanObserve()) continue;
		DesignerPropertyDescriptor property;
		if (TryCreateDescriptor(target, *metadata, property))
			result.push_back(std::move(property));
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

std::vector<DesignerPropertyDescriptor> GetConditionProperties(
	std::span<const DependencyPropertyMetadata* const> properties)
{
	std::vector<DesignerPropertyDescriptor> result;
	for (const auto* metadata : properties)
	{
		if (!metadata || !metadata->CanRead() || !metadata->CanObserve()) continue;
		DesignerPropertyDescriptor property;
		if (TryCreateDescriptor(*metadata, property))
			result.push_back(std::move(property));
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

std::vector<DesignerPropertyDescriptor> GetBrowsableProperties(Control& target)
{
	auto result = GetStyleProperties(target);
	result.erase(std::remove_if(result.begin(), result.end(), [&](const auto& property)
	{
		if (!property.Metadata || !property.Metadata->IsDesignerBrowsable(target)) return true;
		return property.Persistence == DependencyPropertyPersistence::Native
			|| property.Persistence == DependencyPropertyPersistence::Transient;
	}), result.end());
	SortBrowsableProperties(result);
	return result;
}

std::vector<DesignerPropertyDescriptor> GetPropertyGridProperties(Control& target)
{
	std::vector<DesignerPropertyDescriptor> result;
	for (const auto* metadata : DependencyPropertyRegistry::GetProperties(target))
	{
		if (!metadata) continue;
		DesignerPropertyDescriptor property;
		if (TryCreateDescriptor(target, *metadata, property))
			result.push_back(std::move(property));
	}
	result.erase(std::remove_if(result.begin(), result.end(), [&](const auto& property)
	{
		return !property.Metadata
			|| !property.Metadata->IsDesignerBrowsable(target)
			|| property.Persistence == DependencyPropertyPersistence::Transient;
	}), result.end());
	SortBrowsableProperties(result);
	return result;
}

const DesignerPropertyDescriptor* Find(
	const std::vector<DesignerPropertyDescriptor>& properties,
	const std::wstring& name)
{
	const auto found = std::find_if(properties.begin(), properties.end(),
		[&](const DesignerPropertyDescriptor& property)
		{
			return EqualsName(property.Name, name);
		});
	return found == properties.end() ? nullptr : &*found;
}

bool ValidateStyleValue(
	Control& target,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata)
		return Fail(L"目标类型没有可样式化属性：" + propertyName, outError);
	if (!metadata->CanWrite())
		return Fail(L"目标属性不可写：" + propertyName, outError);
	DesignerStyleValueKind expected;
	if (!TryGetStyleValueKind(*metadata, expected))
		return Fail(L"Designer 尚不支持属性类型：" + propertyName, outError);
	if (!IsCompatibleStyleValueKind(expected, value.Kind))
		return Fail(L"属性 " + propertyName + L" 需要 "
			+ DesignerStyleSheetUtils::ValueKindName(expected) + L" 值。", outError);

	BindingValue parsed;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, parsed, outError, resourceBasePath, resources)) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(parsed, converted)
		|| !metadata->TryCoerce(target, converted, effective))
		return Fail(L"属性值无法通过元数据转换或 Coerce：" + propertyName, outError);
	if (outError) outError->clear();
	return true;
}

bool NormalizeStyleValue(
	const DependencyPropertyMetadata& metadata,
	const DesignerStyleValue& value,
	DesignerStyleValue& outCanonical,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	if (!metadata.CanWrite())
		return Fail(L"目标属性不可写：" + metadata.Name(), outError);
	DesignerStyleValueKind expected;
	if (!TryGetStyleValueKind(metadata, expected))
		return Fail(L"Designer 尚不支持属性类型：" + metadata.Name(), outError);
	if (!IsCompatibleStyleValueKind(expected, value.Kind))
		return Fail(L"属性 " + metadata.Name() + L" 需要 "
			+ DesignerStyleSheetUtils::ValueKindName(expected) + L" 值。", outError);
	BindingValue parsed;
	BindingValue converted;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, parsed, outError, resourceBasePath, resources)
		|| !metadata.TryConvert(parsed, converted))
		return Fail(L"属性值无法通过 Schema 转换：" + metadata.Name(), outError);
	outCanonical = DesignerStyleValue{
		expected, FormatValue(converted, expected) };
	if (expected == DesignerStyleValueKind::Brush)
	{
		cui::drawing::Brush brush;
		if (!converted.TryGet(brush))
			return Fail(L"属性值无法转换为画刷：" + metadata.Name(), outError);
		// Brush has one canonical authoring representation. Scalar color text is
		// accepted as XAML shorthand, then lowered to the structured brush object;
		// retaining both creates divergent values after canonical XAML round-trip.
		outCanonical.Text.clear();
		outCanonical.ObjectValue = BrushToValue(brush);
	}
	else if (!value.ObjectValue.is_null())
	{
		// Structured XAML values have no scalar text representation. Preserve the
		// authored empty/text payload so XAML and the internal snapshot round-trip
		// the same contract instead of inventing a "{Transform}"-style sentinel.
		outCanonical.Text = value.Text;
		outCanonical.ObjectValue = value.ObjectValue;
	}
	if (outError) outError->clear();
	return true;
}

bool ValidateConditionValue(
	Control& target,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata)
		return Fail(L"目标类型没有触发器属性：" + propertyName, outError);
	if (!metadata->CanRead() || !metadata->CanObserve())
		return Fail(L"触发器属性必须可读且可观察：" + propertyName, outError);
	DesignerStyleValueKind expected;
	if (!TryGetStyleValueKind(*metadata, expected))
		return Fail(L"Designer 尚不支持触发器属性类型："
			+ propertyName, outError);
	if (!IsCompatibleStyleValueKind(expected, value.Kind))
		return Fail(L"触发器属性 " + propertyName + L" 需要 "
			+ DesignerStyleSheetUtils::ValueKindName(expected) + L" 值。", outError);

	BindingValue parsed;
	BindingValue converted;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, parsed, outError, resourceBasePath, resources)
		|| !metadata->TryConvert(parsed, converted))
		return Fail(L"触发器值无法通过元数据转换：" + propertyName, outError);
	if (outError) outError->clear();
	return true;
}

bool ValidateConditionValue(
	const DependencyPropertyMetadata& metadata,
	const DesignerStyleValue& value,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	if (!metadata.CanRead() || !metadata.CanObserve())
		return Fail(L"触发器属性必须可读且可观察："
			+ metadata.Name(), outError);
	DesignerStyleValueKind expected;
	if (!TryGetStyleValueKind(metadata, expected))
		return Fail(L"Designer 尚不支持触发器属性类型："
			+ metadata.Name(), outError);
	if (!IsCompatibleStyleValueKind(expected, value.Kind))
		return Fail(L"触发器属性 " + metadata.Name() + L" 需要 "
			+ DesignerStyleSheetUtils::ValueKindName(expected) + L" 值。", outError);
	BindingValue parsed;
	BindingValue converted;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, parsed, outError, resourceBasePath, resources)
		|| !metadata.TryConvert(parsed, converted))
		return Fail(L"触发器值无法通过 Schema 转换："
			+ metadata.Name(), outError);
	if (outError) outError->clear();
	return true;
}

bool CaptureValue(
	Control& target,
	const std::wstring& propertyName,
	std::wstring* outCanonicalName,
	DesignerStyleValue& out,
	std::wstring* outError)
{
	DesignerPropertyDescriptor property;
	if (!TryGetStyleProperty(target, propertyName, property))
		return Fail(L"目标类型没有可持久化的元数据属性：" + propertyName, outError);
	out = DesignerStyleValue{ property.ValueKind, property.SampleValue };
	if (property.ValueKind == DesignerStyleValueKind::Brush)
	{
		BindingValue runtimeValue;
		cui::drawing::Brush brush;
		if (property.Metadata
			&& property.Metadata->TryGet(target, runtimeValue)
			&& runtimeValue.TryGet(brush))
			out.ObjectValue = BrushToValue(brush);
	}
	if (outCanonicalName) *outCanonicalName = property.Name;
	if (outError) outError->clear();
	return true;
}

bool CaptureDefaultValue(
	const DependencyPropertyMetadata& metadata,
	DesignerStyleValue& out,
	std::wstring* outError)
{
	DesignerStyleValueKind kind;
	if (!TryGetStyleValueKind(metadata, kind))
		return Fail(L"Designer 尚不支持属性类型："
			+ metadata.Name(), outError);
	BindingValue value;
	if (!metadata.TryGetDefaultValue(value))
		return Fail(L"属性没有可用的 Schema 默认值："
			+ metadata.Name(), outError);
	out = DesignerStyleValue{
		kind, FormatValue(value, kind) };
	if (kind == DesignerStyleValueKind::Brush)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) out.ObjectValue = BrushToValue(brush);
	}
	if (outError) outError->clear();
	return true;
}

bool ApplyValue(
	Control& target,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outCanonicalName,
	DesignerStyleValue* outEffective,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources,
	DependencyPropertyValueSource source)
{
	if (!ValidateStyleValue(
		target, propertyName, value, outError, resourceBasePath, resources)) return false;
	BindingValue parsed;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, parsed, outError, resourceBasePath, resources)) return false;
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata || !cui::framework::DependencyPropertyAccess::SetValue(
		target, metadata->Name(), parsed, source))
		return Fail(L"无法设置元数据属性：" + propertyName, outError);

	DesignerStyleValue effective;
	std::wstring canonicalName;
	if (!CaptureValue(target, metadata->Name(), &canonicalName, effective, outError))
		return false;
	if (outCanonicalName) *outCanonicalName = std::move(canonicalName);
	if (outEffective) *outEffective = std::move(effective);
	if (outError) outError->clear();
	return true;
}

bool UsesMetadataPersistence(const DependencyPropertyMetadata& metadata) noexcept
{
	const auto persistence = metadata.Design().Persistence;
	return persistence == DependencyPropertyPersistence::Automatic
		|| persistence == DependencyPropertyPersistence::Metadata;
}

bool TrackCurrentValue(
	Control& target,
	TrackedPropertyValues& trackedValues,
	const std::wstring& propertyName,
	std::wstring* outCanonicalName,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata)
		return Fail(L"目标类型没有可持久化的元数据属性：" + propertyName, outError);
	DesignerStyleValue effective;
	std::wstring canonicalName;
	if (!CaptureValue(
		target, metadata->Name(), &canonicalName, effective, outError)) return false;
	SynchronizeTrackedValue(
		trackedValues, *metadata, canonicalName, &effective);
	if (outCanonicalName) *outCanonicalName = canonicalName;
	if (outEffective) *outEffective = effective;
	if (outError) outError->clear();
	return true;
}

bool ApplyAndTrackValue(
	Control& target,
	TrackedPropertyValues& trackedValues,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outCanonicalName,
	DesignerStyleValue* outEffective,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources,
	DependencyPropertyValueSource source)
{
	std::wstring canonicalName;
	DesignerStyleValue effective;
	if (!ApplyValue(
		target, propertyName, value,
		&canonicalName, &effective, outError, resourceBasePath, resources,
		source)) return false;
	const auto* metadata = target.FindPropertyMetadata(canonicalName);
	if (!metadata)
		return Fail(L"属性应用后无法解析规范元数据：" + canonicalName, outError);
	SynchronizeTrackedValue(
		trackedValues, *metadata, canonicalName, &effective);
	if (outCanonicalName) *outCanonicalName = canonicalName;
	if (outEffective) *outEffective = effective;
	if (outError) outError->clear();
	return true;
}

bool ResetAndUntrackValue(
	Control& target,
	TrackedPropertyValues& trackedValues,
	const std::wstring& propertyName,
	std::wstring* outCanonicalName,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata)
		return Fail(L"目标类型没有可重置的元数据属性：" + propertyName, outError);
	if (!metadata->CanWrite())
		return Fail(L"目标属性不可写：" + propertyName, outError);
	DesignerStyleValueKind ignored;
	if (!TryGetStyleValueKind(*metadata, ignored))
		return Fail(L"Designer 尚不支持属性类型：" + propertyName, outError);

	const bool hadLocalValue = target.HasPropertyValue(
		metadata->Name(), DependencyPropertyValueSource::Local);
	if (!target.ResetPropertyValue(metadata->Name()) && hadLocalValue)
		return Fail(L"无法重置元数据属性：" + metadata->Name(), outError);
	// With no Local value, an inherited/theme/style/template expression already represents the
	// reset state even though Control::ResetPropertyValue has nothing to clear.
	if (!hadLocalValue
		&& target.GetPropertyValueSource(metadata->Name())
			== DependencyPropertyValueSource::Default
		&& !metadata->HasDefaultValue())
		return Fail(L"属性没有可恢复的默认值：" + metadata->Name(), outError);

	EraseTrackedValues(trackedValues, metadata->Name());
	DesignerStyleValue effective;
	std::wstring canonicalName;
	if (!CaptureValue(
		target, metadata->Name(), &canonicalName, effective, outError)) return false;
	if (outCanonicalName) *outCanonicalName = canonicalName;
	if (outEffective) *outEffective = effective;
	if (outError) outError->clear();
	return true;
}

std::vector<DesignerPropertyDescriptor> GetNodeProperties(UIClass nativeType)
{
	const auto properties = CuiRuntime::XamlRuntimeSchema::NativeProperties(nativeType);
	return GetStyleProperties(properties);
}

bool CaptureNodeValue(
	const DesignerModel::DesignNode& node,
	const std::wstring& propertyName,
	DesignerStyleValue& out,
	std::wstring* outCanonicalName,
	std::wstring* outError)
{
	const auto properties =
		CuiRuntime::XamlRuntimeSchema::NativeProperties(node.Type);
	DesignerPropertyDescriptor descriptor;
	if (!TryGetStyleProperty(properties, propertyName, descriptor)
		|| !descriptor.Metadata)
		return Fail(L"XAML 节点类型没有可持久化属性：" + propertyName,
			outError);
	if (const auto* assignment = node.Properties.Find(descriptor.Name))
		out = assignment->Value;
	else if (!CaptureDefaultValue(*descriptor.Metadata, out, outError))
		return false;
	out.Kind = descriptor.ValueKind;
	if (outCanonicalName) *outCanonicalName = descriptor.Name;
	if (outError) outError->clear();
	return true;
}

bool ReadNodeValue(
	const DesignerModel::DesignNode& node,
	const std::wstring& propertyName,
	BindingValue& out,
	std::wstring* outCanonicalName,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	DesignerStyleValue value;
	std::wstring canonicalName;
	if (!CaptureNodeValue(
		node, propertyName, value, &canonicalName, outError)) return false;
	const auto* metadata = CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
		node.Type, canonicalName);
	if (!metadata)
		return Fail(L"XAML 节点属性缺少运行时元数据：" + canonicalName,
			outError);
	BindingValue converted;
	std::wstring conversionError;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, converted, &conversionError, resourceBasePath, resources)
		|| !metadata->TryConvert(converted, out))
		return Fail(L"XAML 节点属性无法转换到运行时类型：" + canonicalName
			+ (conversionError.empty() ? std::wstring{}
				: L"：" + conversionError), outError);
	if (outCanonicalName) *outCanonicalName = canonicalName;
	if (outError) outError->clear();
	return true;
}

bool ApplyNodeValue(
	DesignerModel::DesignNode& node,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outCanonicalName,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	const auto properties =
		CuiRuntime::XamlRuntimeSchema::NativeProperties(node.Type);
	DesignerPropertyDescriptor descriptor;
	if (!TryGetStyleProperty(properties, propertyName, descriptor)
		|| !descriptor.Metadata)
		return Fail(L"XAML 节点类型没有可写属性：" + propertyName,
			outError);
	DesignerStyleValue effective;
	if (!NormalizeStyleValue(
		*descriptor.Metadata, value, effective, outError,
		resourceBasePath, resources)) return false;
	node.Properties.Set(descriptor.Name, { effective, {}, {} });
	node.Bindings.erase(descriptor.Name);
	if (outCanonicalName) *outCanonicalName = descriptor.Name;
	if (outEffective) *outEffective = effective;
	if (outError) outError->clear();
	return true;
}

bool ResetNodeValue(
	DesignerModel::DesignNode& node,
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outCanonicalName,
	std::wstring* outError)
{
	const auto properties =
		CuiRuntime::XamlRuntimeSchema::NativeProperties(node.Type);
	DesignerPropertyDescriptor descriptor;
	if (!TryGetStyleProperty(properties, propertyName, descriptor)
		|| !descriptor.Metadata)
		return Fail(L"XAML 节点类型没有可重置属性：" + propertyName,
			outError);
	node.Properties.Remove(descriptor.Name);
	node.Bindings.erase(descriptor.Name);
	DesignerStyleValue effective;
	if (!CaptureDefaultValue(*descriptor.Metadata, effective, outError))
		return false;
	if (outCanonicalName) *outCanonicalName = descriptor.Name;
	if (outEffective) *outEffective = effective;
	if (outError) outError->clear();
	return true;
}
}
