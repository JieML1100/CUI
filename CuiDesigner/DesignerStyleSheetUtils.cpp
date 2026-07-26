#include "DesignerStyleSheetUtils.h"
#include "DesignerBindingUtils.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerPropertyCatalog.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include <Application.h>
#include <Convert.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <unordered_set>
#include "../D2DGraphics/include/BitmapSource.h"

namespace DesignerStyleSheetUtils
{
std::vector<RuntimeStyleResource> BuildItemsPanelStyleResources(
	const std::vector<DesignerModel::DesignItemsPanelTemplate>& templates)
{
	std::vector<RuntimeStyleResource> result;
	result.reserve(templates.size());
	for (const auto& definition : templates)
		result.emplace_back(
			definition.Key,
			BindingValue(ItemsPanelTemplateReference(
				std::make_shared<const ItemsPanelTemplate>(
					definition.Value))));
	return result;
}

std::vector<RuntimeStyleResource> BuildItemsPanelStyleResources(
	const DesignerModel::DesignDocument* document)
{
	return document
		? BuildItemsPanelStyleResources(document->ItemsPanelTemplates)
		: std::vector<RuntimeStyleResource>{};
}

namespace
{
	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	bool IsStructuralResourceSetter(const std::wstring& property)
	{
		return EqualsName(property, L"Template")
			|| EqualsName(property, L"ItemsPanel");
	}

	std::vector<std::wstring> Split(const std::wstring& value, wchar_t delimiter)
	{
		std::vector<std::wstring> result;
		std::wstring current;
		for (wchar_t ch : value)
		{
			if (ch == delimiter)
			{
				result.push_back(Trim(current));
				current.clear();
			}
			else
			{
				current.push_back(ch);
			}
		}
		result.push_back(Trim(current));
		return result;
	}

	bool TryParseLongLong(const std::wstring& text, long long& out)
	{
		const auto trimmed = Trim(text);
		if (trimmed.empty()) return false;
		wchar_t* end = nullptr;
		errno = 0;
		const auto value = std::wcstoll(trimmed.c_str(), &end, 10);
		if (errno == ERANGE || !end || *end != L'\0') return false;
		out = value;
		return true;
	}

	bool TryParseDouble(const std::wstring& text, double& out)
	{
		const auto trimmed = Trim(text);
		if (trimmed.empty()) return false;
		wchar_t* end = nullptr;
		errno = 0;
		const auto value = std::wcstod(trimmed.c_str(), &end);
		if (errno == ERANGE || !end || *end != L'\0' || !std::isfinite(value))
			return false;
		out = value;
		return true;
	}

	bool TryParseFloat(const std::wstring& text, float& out)
	{
		double value = 0.0;
		if (!TryParseDouble(text, value)
			|| value < -static_cast<double>((std::numeric_limits<float>::max)())
			|| value > static_cast<double>((std::numeric_limits<float>::max)()))
			return false;
		out = static_cast<float>(value);
		return true;
	}

	bool TryParseHexByte(const std::wstring& value, size_t offset, unsigned int& out)
	{
		if (offset + 2 > value.size()) return false;
		wchar_t buffer[3]{ value[offset], value[offset + 1], L'\0' };
		wchar_t* end = nullptr;
		const auto parsed = std::wcstoul(buffer, &end, 16);
		if (!end || *end != L'\0' || parsed > 255) return false;
		out = static_cast<unsigned int>(parsed);
		return true;
	}

	bool TryParseColor(const std::wstring& text, D2D1_COLOR_F& out)
	{
		const auto value = Trim(text);
		if (value.size() == 7 || value.size() == 9)
		{
			if (value.front() != L'#') return false;
			unsigned int a = 255, r = 0, g = 0, b = 0;
			const size_t rgbOffset = value.size() == 9 ? 3 : 1;
			if (value.size() == 9 && !TryParseHexByte(value, 1, a)) return false;
			if (!TryParseHexByte(value, rgbOffset, r)
				|| !TryParseHexByte(value, rgbOffset + 2, g)
				|| !TryParseHexByte(value, rgbOffset + 4, b)) return false;
			out = D2D1_COLOR_F{
				static_cast<float>(r) / 255.0f,
				static_cast<float>(g) / 255.0f,
				static_cast<float>(b) / 255.0f,
				static_cast<float>(a) / 255.0f };
			return true;
		}

		auto parts = Split(value, L',');
		if (parts.size() != 3 && parts.size() != 4) return false;
		float channels[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		for (size_t index = 0; index < parts.size(); ++index)
		{
			if (!TryParseFloat(parts[index], channels[index])
				|| channels[index] < 0.0f || channels[index] > 1.0f) return false;
		}
		out = D2D1_COLOR_F{ channels[0], channels[1], channels[2], channels[3] };
		return true;
	}

	bool TryParseFloatList(
		const std::wstring& text,
		std::initializer_list<size_t> allowedCounts,
		std::vector<float>& out)
	{
		auto parts = Split(text, L',');
		if (std::find(allowedCounts.begin(), allowedCounts.end(), parts.size())
			== allowedCounts.end()) return false;
		out.clear();
		out.reserve(parts.size());
		for (const auto& part : parts)
		{
			float value = 0.0f;
			if (!TryParseFloat(part, value)) return false;
			out.push_back(value);
		}
		return true;
	}

	bool ContainsName(const std::vector<std::wstring>& values, const std::wstring& value)
	{
		return std::any_of(values.begin(), values.end(),
			[&](const std::wstring& item) { return EqualsName(item, value); });
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	D2D1_COLOR_F ColorFromValue(
		const DesignerModel::DesignValue& value,
		const D2D1_COLOR_F& fallback)
	{
		if (!value.is_object()) return fallback;
		return D2D1_COLOR_F{
			static_cast<float>(value.value("r", static_cast<double>(fallback.r))),
			static_cast<float>(value.value("g", static_cast<double>(fallback.g))),
			static_cast<float>(value.value("b", static_cast<double>(fallback.b))),
			static_cast<float>(value.value("a", static_cast<double>(fallback.a))) };
	}

	std::shared_ptr<ResourceLoadContext> EffectiveResources(
		const std::shared_ptr<ResourceLoadContext>& resources)
	{
		return resources ? resources
			: std::make_shared<ResourceLoadContext>(
				Application::GetResourceResolver());
	}

	bool TryConvertTransform(
		const DesignerModel::DesignValue& value,
		cui::drawing::Transform& output,
		std::wstring* outError);

	bool TryConvertBrush(
		const DesignerModel::DesignValue& value,
		cui::drawing::Brush& output,
		std::wstring* outError,
		const std::wstring& resourceBasePath,
		const std::shared_ptr<ResourceLoadContext>& resources)
	{
		if (!value.is_object()) return Fail(L"画刷必须是对象。", outError);
		output = cui::drawing::Brush{};
		const auto type = value.value("type", std::string{});
		if (type == "none") output.Kind = cui::drawing::BrushKind::None;
		else if (type == "solid") output.Kind = cui::drawing::BrushKind::Solid;
		else if (type == "linear") output.Kind = cui::drawing::BrushKind::LinearGradient;
		else if (type == "radial") output.Kind = cui::drawing::BrushKind::RadialGradient;
		else if (type == "image") output.Kind = cui::drawing::BrushKind::Image;
		else return Fail(L"画刷类型无效。", outError);
		if (output.Kind == cui::drawing::BrushKind::None) return true;

		output.Opacity = static_cast<float>(value.value("opacity", 1.0));
		if (!std::isfinite(output.Opacity)
			|| output.Opacity < 0.0f || output.Opacity > 1.0f)
			return Fail(L"画刷 Opacity 必须位于 0 到 1。", outError);
		const auto mapping = value.value("mapping", std::string("relative"));
		if (mapping == "absolute")
			output.MappingMode = cui::drawing::BrushMappingMode::Absolute;
		else if (mapping == "relative")
			output.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox;
		else return Fail(L"画刷 MappingMode 无效。", outError);
		output.Color = ColorFromValue(
			value.contains("color") ? value["color"] : DesignerModel::DesignValue(),
			output.Color);
		output.StartPoint = D2D1::Point2F(
			static_cast<float>(value.value("startX", 0.0)),
			static_cast<float>(value.value("startY", 0.0)));
		output.EndPoint = D2D1::Point2F(
			static_cast<float>(value.value("endX", 1.0)),
			static_cast<float>(value.value("endY", 1.0)));
		output.Center = D2D1::Point2F(
			static_cast<float>(value.value("centerX", 0.5)),
			static_cast<float>(value.value("centerY", 0.5)));
		output.GradientOrigin = D2D1::Point2F(
			static_cast<float>(value.value("originX", 0.5)),
			static_cast<float>(value.value("originY", 0.5)));
		output.RadiusX = static_cast<float>(value.value("radiusX", 0.5));
		output.RadiusY = static_cast<float>(value.value("radiusY", 0.5));
		auto convertOptionalTransform = [&](const char* key,
			std::optional<cui::drawing::Transform>& destination,
			const wchar_t* label)
		{
			if (!value.contains(key)) return true;
			cui::drawing::Transform transform;
			std::wstring transformError;
			if (!TryConvertTransform(value[key], transform, &transformError))
				return Fail(std::wstring(label) + L"：" + transformError, outError);
			destination = std::move(transform);
			return true;
		};
		if (!convertOptionalTransform(
				"transform", output.Transform, L"Brush.Transform")
			|| !convertOptionalTransform("relativeTransform",
				output.RelativeTransform, L"Brush.RelativeTransform")) return false;
		if (output.Kind == cui::drawing::BrushKind::Image)
		{
			const auto source = Trim(Convert::Utf8ToUnicode(
				value.value("source", std::string{})));
			if (source.empty()) return Fail(L"ImageBrush 缺少 ImageSource。", outError);
			ResolvedResource resolved;
			if (!EffectiveResources(resources)->Resolve(
				source, resourceBasePath, resolved, outError)) return false;
			output.ImageSource = BitmapSource::FromBuffer(
				resolved.Bytes.data(), resolved.Bytes.size(), source);
			if (!output.ImageSource)
				return Fail(L"无法解码 ImageBrush：" + resolved.Identity, outError);
			const auto stretch = value.value("stretch", std::string("fill"));
			if (stretch == "none") output.Stretch = cui::drawing::ImageBrushStretch::None;
			else if (stretch == "fill") output.Stretch = cui::drawing::ImageBrushStretch::Fill;
			else if (stretch == "uniform") output.Stretch = cui::drawing::ImageBrushStretch::Uniform;
			else if (stretch == "uniformToFill")
				output.Stretch = cui::drawing::ImageBrushStretch::UniformToFill;
			else return Fail(L"ImageBrush Stretch 无效。", outError);
			const auto alignmentX = value.value("alignmentX", std::string("center"));
			if (alignmentX == "left") output.AlignmentX = cui::drawing::ImageBrushAlignmentX::Left;
			else if (alignmentX == "center") output.AlignmentX = cui::drawing::ImageBrushAlignmentX::Center;
			else if (alignmentX == "right") output.AlignmentX = cui::drawing::ImageBrushAlignmentX::Right;
			else return Fail(L"ImageBrush AlignmentX 无效。", outError);
			const auto alignmentY = value.value("alignmentY", std::string("center"));
			if (alignmentY == "top") output.AlignmentY = cui::drawing::ImageBrushAlignmentY::Top;
			else if (alignmentY == "center") output.AlignmentY = cui::drawing::ImageBrushAlignmentY::Center;
			else if (alignmentY == "bottom") output.AlignmentY = cui::drawing::ImageBrushAlignmentY::Bottom;
			else return Fail(L"ImageBrush AlignmentY 无效。", outError);
			return true;
		}
		if (output.Kind == cui::drawing::BrushKind::Solid) return true;
		if (!value.contains("stops") || !value["stops"].is_array())
			return Fail(L"渐变画刷缺少 GradientStops。", outError);
		output.GradientStops.clear();
		for (const auto& item : value["stops"])
		{
			if (!item.is_object() || !item.contains("color"))
				return Fail(L"GradientStop 格式无效。", outError);
			const auto offset = static_cast<float>(item.value("offset", 0.0));
			if (!std::isfinite(offset) || offset < 0.0f || offset > 1.0f)
				return Fail(L"GradientStop Offset 必须位于 0 到 1。", outError);
			output.GradientStops.push_back({ offset,
				ColorFromValue(item["color"], D2D1::ColorF(D2D1::ColorF::Black)) });
		}
		if (output.GradientStops.size() < 2)
			return Fail(L"渐变画刷至少需要两个 GradientStop。", outError);
		return true;
	}

	bool TryConvertTransform(
		const DesignerModel::DesignValue& value,
		cui::drawing::Transform& output,
		std::wstring* outError)
	{
		if (!value.is_array() || value.empty())
			return Fail(L"Transform 必须是非空数组。", outError);
		output.Operations.clear();
		auto finite = [](std::initializer_list<float> values)
		{
			return std::all_of(values.begin(), values.end(), [](float item)
				{ return std::isfinite(item); });
		};
		for (const auto& item : value.ArrayItems())
		{
			if (!item.is_object())
				return Fail(L"Transform 操作格式无效。", outError);
			cui::drawing::TransformOperation operation;
			const auto type = item.value("type", std::string{});
			if (type == "matrix")
			{
				operation.Kind = cui::drawing::TransformKind::Matrix;
				operation.Matrix = D2D1::Matrix3x2F(
					static_cast<float>(item.value("m11", 1.0)),
					static_cast<float>(item.value("m12", 0.0)),
					static_cast<float>(item.value("m21", 0.0)),
					static_cast<float>(item.value("m22", 1.0)),
					static_cast<float>(item.value("dx", 0.0)),
					static_cast<float>(item.value("dy", 0.0)));
				if (!finite({ operation.Matrix._11, operation.Matrix._12,
					operation.Matrix._21, operation.Matrix._22,
					operation.Matrix._31, operation.Matrix._32 }))
					return Fail(L"MatrixTransform 数值无效。", outError);
			}
			else if (type == "translate")
			{
				operation.Kind = cui::drawing::TransformKind::Translate;
				operation.X = static_cast<float>(item.value("x", 0.0));
				operation.Y = static_cast<float>(item.value("y", 0.0));
			}
			else if (type == "scale")
			{
				operation.Kind = cui::drawing::TransformKind::Scale;
				operation.ScaleX = static_cast<float>(item.value("scaleX", 1.0));
				operation.ScaleY = static_cast<float>(item.value("scaleY", 1.0));
			}
			else if (type == "rotate")
			{
				operation.Kind = cui::drawing::TransformKind::Rotate;
				operation.Angle = static_cast<float>(item.value("angle", 0.0));
			}
			else if (type == "skew")
			{
				operation.Kind = cui::drawing::TransformKind::Skew;
				operation.AngleX = static_cast<float>(item.value("angleX", 0.0));
				operation.AngleY = static_cast<float>(item.value("angleY", 0.0));
			}
			else return Fail(L"Transform 操作类型无效。", outError);
			operation.CenterX = static_cast<float>(item.value("centerX", 0.0));
			operation.CenterY = static_cast<float>(item.value("centerY", 0.0));
			if (!finite({ operation.X, operation.Y, operation.ScaleX,
				operation.ScaleY, operation.Angle, operation.AngleX,
				operation.AngleY, operation.CenterX, operation.CenterY }))
				return Fail(L"Transform 数值无效。", outError);
			output.Operations.push_back(operation);
		}
		return true;
	}

	bool TryConvertGeometry(
		const DesignerModel::DesignValue& value,
		cui::drawing::Geometry& output,
		std::wstring* outError)
	{
		if (!value.is_object()) return Fail(L"Geometry 必须是对象。", outError);
		output = cui::drawing::Geometry{};
		auto finite = [](std::initializer_list<float> values)
		{
			return std::all_of(values.begin(), values.end(), [](float item)
				{ return std::isfinite(item); });
		};
		auto finish = [&]() -> bool
		{
			if (!value.contains("transform")) return true;
			cui::drawing::Transform transform;
			std::wstring transformError;
			if (!TryConvertTransform(value["transform"], transform, &transformError))
				return Fail(L"Geometry.Transform：" + transformError, outError);
			output.LocalTransform = std::move(transform);
			return true;
		};
		const auto type = value.value("type", std::string{});
		if (type == "rectangle")
		{
			const float x = static_cast<float>(value.value("x", 0.0));
			const float y = static_cast<float>(value.value("y", 0.0));
			const float width = static_cast<float>(value.value("width", 0.0));
			const float height = static_cast<float>(value.value("height", 0.0));
			const float radiusX = static_cast<float>(value.value("radiusX", 0.0));
			const float radiusY = static_cast<float>(value.value("radiusY", 0.0));
			if (!finite({ x, y, width, height, radiusX, radiusY })
				|| width < 0.0f || height < 0.0f
				|| radiusX < 0.0f || radiusY < 0.0f)
				return Fail(L"RectangleGeometry 数值无效。", outError);
			output.Kind = cui::drawing::GeometryKind::Rectangle;
			output.Rect = D2D1::RectF(x, y, x + width, y + height);
			output.RadiusX = radiusX;
			output.RadiusY = radiusY;
			return finish();
		}
		if (type == "ellipse")
		{
			const float centerX = static_cast<float>(value.value("centerX", 0.0));
			const float centerY = static_cast<float>(value.value("centerY", 0.0));
			const float radiusX = static_cast<float>(value.value("radiusX", 0.0));
			const float radiusY = static_cast<float>(value.value("radiusY", 0.0));
			if (!finite({ centerX, centerY, radiusX, radiusY })
				|| radiusX < 0.0f || radiusY < 0.0f)
				return Fail(L"EllipseGeometry 数值无效。", outError);
			output.Kind = cui::drawing::GeometryKind::Ellipse;
			output.Center = D2D1::Point2F(centerX, centerY);
			output.RadiusX = radiusX;
			output.RadiusY = radiusY;
			return finish();
		}
		if (type == "path")
		{
			output.Kind = cui::drawing::GeometryKind::Path;
			const auto fillRule = value.value("fillRule", std::string("evenodd"));
			if (fillRule == "nonzero")
				output.FillRule = cui::drawing::GeometryFillRule::Nonzero;
			else if (fillRule != "evenodd")
				return Fail(L"PathGeometry FillRule 无效。", outError);
			if (!value.contains("figures") || !value["figures"].is_array())
				return Fail(L"PathGeometry 缺少 Figures。", outError);
			for (const auto& figureValue : value["figures"].ArrayItems())
			{
				if (!figureValue.is_object() || !figureValue.contains("segments")
					|| !figureValue["segments"].is_array())
					return Fail(L"PathFigure 格式无效。", outError);
				cui::drawing::PathFigure figure;
				figure.StartPoint = D2D1::Point2F(
					static_cast<float>(figureValue.value("startX", 0.0)),
					static_cast<float>(figureValue.value("startY", 0.0)));
				figure.IsClosed = figureValue.value("closed", false);
				figure.IsFilled = figureValue.value("filled", true);
				if (!finite({ figure.StartPoint.x, figure.StartPoint.y }))
					return Fail(L"PathFigure.StartPoint 数值无效。", outError);
				for (const auto& segmentValue : figureValue["segments"].ArrayItems())
				{
					if (!segmentValue.is_object())
						return Fail(L"PathSegment 格式无效。", outError);
					cui::drawing::PathSegment segment;
					const auto segmentType = segmentValue.value("type", std::string{});
					if (segmentType == "line")
						segment.Kind = cui::drawing::PathSegmentKind::Line;
					else if (segmentType == "bezier")
						segment.Kind = cui::drawing::PathSegmentKind::Bezier;
					else if (segmentType == "quadratic")
						segment.Kind = cui::drawing::PathSegmentKind::QuadraticBezier;
					else if (segmentType == "arc")
						segment.Kind = cui::drawing::PathSegmentKind::Arc;
					else return Fail(L"PathSegment 类型无效。", outError);
					segment.Point = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x", 0.0)),
						static_cast<float>(segmentValue.value("y", 0.0)));
					segment.Point1 = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x1", 0.0)),
						static_cast<float>(segmentValue.value("y1", 0.0)));
					segment.Point2 = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x2", 0.0)),
						static_cast<float>(segmentValue.value("y2", 0.0)));
					segment.Point3 = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x3", 0.0)),
						static_cast<float>(segmentValue.value("y3", 0.0)));
					segment.Size = D2D1::SizeF(
						static_cast<float>(segmentValue.value("width", 0.0)),
						static_cast<float>(segmentValue.value("height", 0.0)));
					segment.RotationAngle = static_cast<float>(
						segmentValue.value("rotation", 0.0));
					segment.IsLargeArc = segmentValue.value("large", false);
					const auto sweep = segmentValue.value(
						"sweep", std::string("counterclockwise"));
					if (sweep == "clockwise")
						segment.Sweep = cui::drawing::SweepDirection::Clockwise;
					else if (sweep != "counterclockwise")
						return Fail(L"ArcSegment SweepDirection 无效。", outError);
					if (!finite({ segment.Point.x, segment.Point.y,
						segment.Point1.x, segment.Point1.y,
						segment.Point2.x, segment.Point2.y,
						segment.Point3.x, segment.Point3.y,
						segment.Size.width, segment.Size.height,
						segment.RotationAngle })
						|| segment.Size.width < 0.0f || segment.Size.height < 0.0f)
						return Fail(L"PathSegment 数值无效。", outError);
					figure.Segments.push_back(segment);
				}
				output.Figures.push_back(std::move(figure));
			}
			return finish();
		}
		if (type != "group") return Fail(L"Geometry 类型无效。", outError);
		output.Kind = cui::drawing::GeometryKind::Group;
		const auto fillRule = value.value("fillRule", std::string("evenodd"));
		if (fillRule == "nonzero")
			output.FillRule = cui::drawing::GeometryFillRule::Nonzero;
		else if (fillRule != "evenodd")
			return Fail(L"GeometryGroup FillRule 无效。", outError);
		if (!value.contains("children") || !value["children"].is_array())
			return Fail(L"GeometryGroup 缺少 Children。", outError);
		for (const auto& childValue : value["children"].ArrayItems())
		{
			cui::drawing::Geometry child;
			if (!TryConvertGeometry(childValue, child, outError)) return false;
			output.Children.push_back(std::move(child));
		}
		return finish();
	}

}

std::wstring Trim(const std::wstring& value)
{
	size_t begin = 0;
	while (begin < value.size() && std::iswspace(value[begin])) ++begin;
	size_t end = value.size();
	while (end > begin && std::iswspace(value[end - 1])) --end;
	return value.substr(begin, end - begin);
}

std::wstring ValueKindName(DesignerStyleValueKind kind)
{
	switch (kind)
	{
	case DesignerStyleValueKind::Bool: return L"Bool";
	case DesignerStyleValueKind::Int: return L"Int";
	case DesignerStyleValueKind::Int64: return L"Int64";
	case DesignerStyleValueKind::Float: return L"Float";
	case DesignerStyleValueKind::Double: return L"Double";
	case DesignerStyleValueKind::String: return L"String";
	case DesignerStyleValueKind::Color: return L"Color";
	case DesignerStyleValueKind::Thickness: return L"Thickness";
	case DesignerStyleValueKind::Point: return L"Point";
	case DesignerStyleValueKind::Vector: return L"Vector";
	case DesignerStyleValueKind::Rect: return L"Rect";
	case DesignerStyleValueKind::Size: return L"Size";
	case DesignerStyleValueKind::Matrix: return L"Matrix";
	case DesignerStyleValueKind::Length: return L"Length";
	case DesignerStyleValueKind::ImageSource: return L"ImageSource";
	case DesignerStyleValueKind::Brush: return L"Brush";
	case DesignerStyleValueKind::Geometry: return L"Geometry";
	case DesignerStyleValueKind::Transform: return L"Transform";
	}
	return L"String";
}

bool TryParseValueKind(const std::wstring& value, DesignerStyleValueKind& out)
{
	for (auto kind : { DesignerStyleValueKind::Bool, DesignerStyleValueKind::Int,
		DesignerStyleValueKind::Int64, DesignerStyleValueKind::Float,
		DesignerStyleValueKind::Double, DesignerStyleValueKind::String,
		DesignerStyleValueKind::Color, DesignerStyleValueKind::Thickness,
		DesignerStyleValueKind::Point, DesignerStyleValueKind::Vector,
		DesignerStyleValueKind::Rect,
		DesignerStyleValueKind::Size,
		DesignerStyleValueKind::Matrix,
		DesignerStyleValueKind::Length,
		DesignerStyleValueKind::ImageSource, DesignerStyleValueKind::Brush,
		DesignerStyleValueKind::Geometry, DesignerStyleValueKind::Transform })
	{
		if (EqualsName(Trim(value), ValueKindName(kind)))
		{
			out = kind;
			return true;
		}
	}
	return false;
}

std::vector<std::wstring> ValueKindNames()
{
	return { L"Bool", L"Int", L"Int64", L"Float", L"Double", L"String",
		L"Color", L"Thickness", L"Point", L"Vector", L"Rect", L"Size", L"Matrix",
		L"Length", L"ImageSource", L"Brush",
		L"Geometry", L"Transform" };
}

std::wstring UIClassName(UIClass type)
{
	if (type == UIClass::UI_Base) return L"Any";
	if (type == UIClass::UI_CUSTOM) return L"CUSTOM";
	const auto* descriptor =
		CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(type);
	return descriptor ? descriptor->TypeId.LocalName : std::wstring{};
}

bool TryParseUIClass(const std::wstring& value, UIClass& out)
{
	const auto name = Trim(value);
	if (EqualsName(name, L"Any"))
	{
		out = UIClass::UI_Base;
		return true;
	}
	if (EqualsName(name, L"CUSTOM"))
	{
		out = UIClass::UI_CUSTOM;
		return true;
	}
	const auto* descriptor = CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
		CuiRuntime::XamlRuntimeSchema::CuiNamespace, name);
	if (!descriptor) return false;
	out = descriptor->NativeType;
	return true;
}

std::vector<std::wstring> UIClassNames(bool includeAny)
{
	std::vector<std::wstring> result;
	if (includeAny) result.push_back(L"Any");
	for (int numeric = static_cast<int>(UIClass::UI_FrameworkElement);
		numeric <= static_cast<int>(UIClass::UI_CUSTOM); ++numeric)
	{
		auto name = UIClassName(static_cast<UIClass>(numeric));
		if (!name.empty()) result.push_back(std::move(name));
	}
	return result;
}

bool TryConvertValue(
	const DesignerStyleValue& value,
	BindingValue& out,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	const auto invalid = [&]()
	{
		return Fail(L"样式值不是有效的 " + ValueKindName(value.Kind)
			+ L"：" + value.Text, outError);
	};

	switch (value.Kind)
	{
	case DesignerStyleValueKind::Bool:
	{
		const auto text = Lower(Trim(value.Text));
		if (text == L"true" || text == L"1") out = BindingValue(true);
		else if (text == L"false" || text == L"0") out = BindingValue(false);
		else return invalid();
		return true;
	}
	case DesignerStyleValueKind::Int:
	{
		long long parsed = 0;
		if (!TryParseLongLong(value.Text, parsed)
			|| parsed < (std::numeric_limits<int>::min)()
			|| parsed > (std::numeric_limits<int>::max)()) return invalid();
		out = BindingValue(static_cast<int>(parsed));
		return true;
	}
	case DesignerStyleValueKind::Int64:
	{
		long long parsed = 0;
		if (!TryParseLongLong(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::Float:
	{
		float parsed = 0.0f;
		if (!TryParseFloat(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::Double:
	{
		double parsed = 0.0;
		if (!TryParseDouble(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::String:
		out = BindingValue(value.Text);
		return true;
	case DesignerStyleValueKind::Color:
	{
		D2D1_COLOR_F parsed{};
		if (!TryParseColor(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::Thickness:
	{
		std::vector<float> parsed;
		if (!TryParseFloatList(value.Text, { 1, 2, 4 }, parsed)) return invalid();
		Thickness thickness;
		if (parsed.size() == 1) thickness = Thickness(parsed[0]);
		else if (parsed.size() == 2) thickness = Thickness(parsed[0], parsed[1]);
		else thickness = Thickness(parsed[0], parsed[1], parsed[2], parsed[3]);
		out = BindingValue(thickness);
		return true;
	}
	case DesignerStyleValueKind::Point:
	{
		auto parts = Split(value.Text, L',');
		float x = 0.0f, y = 0.0f;
		if (parts.size() != 2 || !TryParseFloat(parts[0], x)
			|| !TryParseFloat(parts[1], y)) return invalid();
		out = BindingValue(cui::core::Point{ x, y });
		return true;
	}
	case DesignerStyleValueKind::Vector:
	{
		auto parts = Split(value.Text, L',');
		float x = 0.0f, y = 0.0f;
		if (parts.size() != 2 || !TryParseFloat(parts[0], x)
			|| !TryParseFloat(parts[1], y)) return invalid();
		out = BindingValue(cui::core::Vector{ x, y });
		return true;
	}
	case DesignerStyleValueKind::Rect:
	{
		auto parts = Split(value.Text, L',');
		float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
		if (parts.size() != 4 || !TryParseFloat(parts[0], x)
			|| !TryParseFloat(parts[1], y)
			|| !TryParseFloat(parts[2], width)
			|| !TryParseFloat(parts[3], height)
			|| width < 0.0f || height < 0.0f) return invalid();
		out = BindingValue(cui::core::Rect{ x, y, width, height });
		return true;
	}
	case DesignerStyleValueKind::Size:
	{
		auto parts = Split(value.Text, L',');
		float width = 0.0f, height = 0.0f;
		if (parts.size() != 2 || !TryParseFloat(parts[0], width)
			|| !TryParseFloat(parts[1], height)
			|| width < 0.0f || height < 0.0f) return invalid();
		out = BindingValue(cui::core::Size{ width, height });
		return true;
	}
	case DesignerStyleValueKind::Matrix:
	{
		auto parts = Split(value.Text, L',');
		float m11 = 0.0f, m12 = 0.0f, m21 = 0.0f;
		float m22 = 0.0f, offsetX = 0.0f, offsetY = 0.0f;
		if (parts.size() != 6 || !TryParseFloat(parts[0], m11)
			|| !TryParseFloat(parts[1], m12)
			|| !TryParseFloat(parts[2], m21)
			|| !TryParseFloat(parts[3], m22)
			|| !TryParseFloat(parts[4], offsetX)
			|| !TryParseFloat(parts[5], offsetY)) return invalid();
		const D2D1_MATRIX_3X2_F matrix = D2D1::Matrix3x2F(
			m11, m12, m21, m22, offsetX, offsetY);
		out = BindingValue(matrix);
		return true;
	}
	case DesignerStyleValueKind::Length:
	{
		if (EqualsName(Trim(value.Text), L"Auto"))
		{
			out = BindingValue(cui::layout::Length::Auto());
			return true;
		}
		float parsed = 0.0f;
		if (!TryParseFloat(value.Text, parsed) || parsed < 0.0f) return invalid();
		out = BindingValue(cui::layout::Length::Fixed(parsed));
		return true;
	}
	case DesignerStyleValueKind::ImageSource:
	{
		const auto uri = Trim(value.Text);
		if (uri.empty())
		{
			out = BindingValue(std::shared_ptr<BitmapSource>{});
			return true;
		}
		ResolvedResource resolved;
		if (!EffectiveResources(resources)->Resolve(
			uri, resourceBasePath, resolved, outError)) return false;
		auto bitmap = BitmapSource::FromBuffer(
			resolved.Bytes.data(), resolved.Bytes.size(), uri);
		if (!bitmap)
			return Fail(L"无法解码 ImageSource：" + resolved.Identity, outError);
		out = BindingValue(std::move(bitmap));
		return true;
	}
	case DesignerStyleValueKind::Brush:
	{
		cui::drawing::Brush brush;
		if (value.ObjectValue.is_null())
		{
			D2D1_COLOR_F color{};
			if (!TryParseColor(value.Text, color)) return invalid();
			brush.Kind = cui::drawing::BrushKind::Solid;
			brush.Color = color;
		}
		else if (!TryConvertBrush(
			value.ObjectValue, brush, outError, resourceBasePath, resources)) return false;
		out = BindingValue(std::move(brush));
		return true;
	}
	case DesignerStyleValueKind::Geometry:
	{
		cui::drawing::Geometry geometry;
		if (!TryConvertGeometry(value.ObjectValue, geometry, outError)) return false;
		out = BindingValue(std::move(geometry));
		return true;
	}
	case DesignerStyleValueKind::Transform:
	{
		cui::drawing::Transform transform;
		if (!TryConvertTransform(value.ObjectValue, transform, outError)) return false;
		out = BindingValue(std::move(transform));
		return true;
	}
	}
	return invalid();
}

void Canonicalize(DesignerStyleSheet& styleSheet)
{
	auto dictionaries = styleSheet.MergedDictionaries;
	styleSheet.MergedDictionaries.clear();
	for (auto& dictionary : dictionaries)
	{
		dictionary = Trim(dictionary);
		if (!dictionary.empty()
			&& !ContainsName(styleSheet.MergedDictionaries, dictionary))
			styleSheet.MergedDictionaries.push_back(std::move(dictionary));
	}
	for (auto& resource : styleSheet.Resources)
	{
		resource.Key = Trim(resource.Key);
		resource.SourceDictionary = Trim(resource.SourceDictionary);
		if (resource.Value.Kind != DesignerStyleValueKind::String)
			resource.Value.Text = Trim(resource.Value.Text);
	}
	for (auto& rule : styleSheet.Rules)
	{
		rule.XamlType.NamespaceUri = Trim(rule.XamlType.NamespaceUri);
		rule.XamlType.LocalName = Trim(rule.XamlType.LocalName);
		rule.ComponentType.XamlPrefix = Trim(rule.ComponentType.XamlPrefix);
		rule.ComponentType.XamlName = Trim(rule.ComponentType.XamlName);
		rule.ComponentType.XamlNamespace = Trim(rule.ComponentType.XamlNamespace);
		rule.Id = Trim(rule.Id);
		rule.BasedOn = Trim(rule.BasedOn);
		rule.SourceDictionary = Trim(rule.SourceDictionary);
		auto canonicalizeSetter = [](DesignerStyleSetter& setter)
		{
			setter.PropertyName = Trim(setter.PropertyName);
			setter.ResourceKey = Trim(setter.ResourceKey);
			if (setter.Literal.Kind != DesignerStyleValueKind::String)
				setter.Literal.Text = Trim(setter.Literal.Text);
		};
		auto canonicalizeDataCondition = [](DesignerStyleDataCondition& condition)
		{
			condition.SourceProperty = DesignerBindingUtils::Trim(
				condition.SourceProperty);
			if (condition.Value.Kind != DesignerStyleValueKind::String)
				condition.Value.Text = Trim(condition.Value.Text);
			switch (condition.Value.Kind)
			{
			case DesignerStyleValueKind::Bool:
			case DesignerStyleValueKind::Int:
			case DesignerStyleValueKind::Int64:
			case DesignerStyleValueKind::Float:
			case DesignerStyleValueKind::Double:
			case DesignerStyleValueKind::String:
				condition.Value.Kind = DesignerStyleValueKind::String;
				break;
			default:
				break;
			}
		};
		auto canonicalizePropertyCondition = [](DesignerStylePropertyCondition& condition)
		{
			condition.Property = Trim(condition.Property);
			if (condition.Value.Kind != DesignerStyleValueKind::String)
				condition.Value.Text = Trim(condition.Value.Text);
		};
		auto canonicalizeActions = [&](auto& actions)
		{
			for (auto& action : actions)
			{
				action.StoryboardName = Trim(action.StoryboardName);
				for (auto& animation : action.Animations)
				{
					animation.TargetName = Trim(animation.TargetName);
					animation.PropertyName = Trim(animation.PropertyName);
					animation.FromResourceKey = Trim(
						animation.FromResourceKey);
					animation.ToResourceKey = Trim(animation.ToResourceKey);
					animation.ByResourceKey = Trim(animation.ByResourceKey);
					if (animation.From.Kind != DesignerStyleValueKind::String)
						animation.From.Text = Trim(animation.From.Text);
					if (animation.To.Kind != DesignerStyleValueKind::String)
						animation.To.Text = Trim(animation.To.Text);
					if (animation.By.Kind != DesignerStyleValueKind::String)
						animation.By.Text = Trim(animation.By.Text);
					for (auto& frame : animation.KeyFrames)
					{
						frame.ResourceKey = Trim(frame.ResourceKey);
						if (frame.Value.Kind
							!= DesignerStyleValueKind::String)
							frame.Value.Text = Trim(frame.Value.Text);
					}
				}
			}
		};
		for (auto& setter : rule.Setters) canonicalizeSetter(setter);
		for (auto& condition : rule.PropertyConditions)
			canonicalizePropertyCondition(condition);
		for (auto& condition : rule.DataConditions)
			canonicalizeDataCondition(condition);
		canonicalizeActions(rule.EnterActions);
		canonicalizeActions(rule.ExitActions);
		for (auto& trigger : rule.Triggers)
		{
			for (auto& condition : trigger.PropertyConditions)
				canonicalizePropertyCondition(condition);
			for (auto& condition : trigger.DataConditions)
				canonicalizeDataCondition(condition);
			for (auto& setter : trigger.Setters) canonicalizeSetter(setter);
			canonicalizeActions(trigger.EnterActions);
			canonicalizeActions(trigger.ExitActions);
		}
	}
}

bool HasSameStyleResourceIdentity(
	const DesignerStyleRule& left,
	const DesignerStyleRule& right)
{
	if (!left.Id.empty() || !right.Id.empty())
		return !left.Id.empty() && !right.Id.empty()
			&& EqualsName(left.Id, right.Id);
	if (!left.ComponentType.Empty() || !right.ComponentType.Empty())
		return !left.ComponentType.Empty() && !right.ComponentType.Empty()
			&& left.ComponentType.RegistryKey()
				== right.ComponentType.RegistryKey();
	if (left.XamlType.Valid() || right.XamlType.Valid())
		return left.XamlType.Valid() && right.XamlType.Valid()
			&& left.XamlType == right.XamlType;
	return left.HasType && right.HasType && left.Type == right.Type;
}

void AppendLexicalScope(
	DesignerStyleSheet& target,
	const DesignerStyleSheet& source)
{
	for (const auto& dictionary : source.MergedDictionaries)
		if (std::none_of(target.MergedDictionaries.begin(),
			target.MergedDictionaries.end(), [&](const auto& current)
			{ return _wcsicmp(current.c_str(), dictionary.c_str()) == 0; }))
			target.MergedDictionaries.push_back(dictionary);
	for (const auto& resource : source.Resources)
	{
		target.Resources.erase(std::remove_if(
			target.Resources.begin(), target.Resources.end(),
			[&](const auto& current)
			{ return current.Key == resource.Key; }),
			target.Resources.end());
		target.Resources.push_back(resource);
	}
	for (const auto& rule : source.Rules)
	{
		target.Rules.erase(std::remove_if(
			target.Rules.begin(), target.Rules.end(), [&](const auto& current)
			{ return HasSameStyleResourceIdentity(current, rule); }),
			target.Rules.end());
		target.Rules.push_back(rule);
	}
}

void RemapRuleResourceKeys(
	DesignerStyleRule& rule,
	const std::vector<std::pair<std::wstring, std::wstring>>& renames,
	const std::function<bool(const std::wstring&)>& shouldRemap)
{
	auto remap = [&](std::wstring& key)
	{
		if (key.empty() || (shouldRemap && !shouldRemap(key))) return;
		for (const auto& [source, destination] : renames)
			if (key == source)
			{
				key = destination;
				return;
			}
	};
	auto rewriteAnimation = [&](DesignerVisualStateAnimation& animation)
	{
		if (animation.HasTo && animation.ToUsesResource)
			remap(animation.ToResourceKey);
		if (animation.HasFrom && animation.FromUsesResource)
			remap(animation.FromResourceKey);
		if (animation.HasBy && animation.ByUsesResource)
			remap(animation.ByResourceKey);
		for (auto& frame : animation.KeyFrames)
			if (frame.UsesResource) remap(frame.ResourceKey);
	};
	auto rewriteSetters = [&](std::vector<DesignerStyleSetter>& setters)
	{
		for (auto& setter : setters)
			if (setter.UsesResource
				&& !IsStructuralResourceSetter(setter.PropertyName))
				remap(setter.ResourceKey);
	};
	auto rewriteActions = [&](std::vector<DesignerEventTriggerAction>& actions)
	{
		for (auto& action : actions)
			for (auto& animation : action.Animations)
				rewriteAnimation(animation);
	};
	remap(rule.BasedOn);
	rewriteSetters(rule.Setters);
	rewriteActions(rule.EnterActions);
	rewriteActions(rule.ExitActions);
	for (auto& trigger : rule.Triggers)
	{
		rewriteSetters(trigger.Setters);
		rewriteActions(trigger.EnterActions);
		rewriteActions(trigger.ExitActions);
	}
}

bool ResolveInheritance(
	const DesignerStyleSheet& source,
	DesignerStyleSheet& out,
	std::wstring* outError)
{
	out = source;
	Canonicalize(out);
	if (outError) outError->clear();

	auto parseTypeKey = [](const std::wstring& key, UIClass& type)
	{
		auto text = Trim(key);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		if (!Lower(text).starts_with(L"x:type")) return false;
		text = Trim(text.substr(6));
		const auto separator = text.find(L':');
		if (separator != std::wstring::npos)
			text = Trim(text.substr(separator + 1));
		return TryParseUIClass(text, type);
	};
	auto unqualified = [](const DesignerStyleRule& rule)
	{
		return rule.PropertyConditions.empty()
			&& rule.DataConditions.empty();
	};
	auto findBase = [&](size_t owner, size_t& baseIndex)
	{
		const auto& rule = out.Rules[owner];
		UIClass typeKey = UIClass::UI_Base;
		const bool usesTypeKey = parseTypeKey(rule.BasedOn, typeKey);
		std::vector<size_t> candidates;
		for (size_t index = 0; index < out.Rules.size(); ++index)
		{
			const auto& candidate = out.Rules[index];
			if (usesTypeKey)
			{
				if (candidate.Id.empty() && candidate.HasType
					&& candidate.Type == typeKey && unqualified(candidate))
					candidates.push_back(index);
			}
			else if (!rule.BasedOn.empty()
				&& EqualsName(candidate.Id, rule.BasedOn))
				candidates.push_back(index);
		}
		if (!usesTypeKey && rule.BasedOn.starts_with(L"{"))
			return Fail(L"BasedOn 仅支持命名样式或 {x:Type TypeName}："
				+ rule.BasedOn, outError);
		if (candidates.empty())
			return Fail(L"BasedOn 引用了不存在的样式："
				+ rule.BasedOn, outError);
		if (candidates.size() > 1 && !usesTypeKey)
		{
			std::vector<size_t> declarations;
			for (const auto index : candidates)
				if (unqualified(out.Rules[index])) declarations.push_back(index);
			if (!declarations.empty()) candidates = std::move(declarations);
			else
				return Fail(L"BasedOn 样式键只对应到多个状态/Class 规则，无法确定基样式："
					+ rule.BasedOn, outError);
		}
		// Resource dictionaries follow source order: later/local declarations
		// shadow earlier merged declarations for BasedOn lookup.
		baseIndex = candidates.back();
		return true;
	};

	std::vector<unsigned char> states(out.Rules.size(), 0);
	std::function<bool(size_t)> resolve = [&](size_t index)
	{
		if (states[index] == 2) return true;
		if (states[index] == 1)
			return Fail(L"检测到循环 Style.BasedOn："
				+ (out.Rules[index].Id.empty()
					? out.Rules[index].BasedOn : out.Rules[index].Id), outError);
		states[index] = 1;
		auto derived = out.Rules[index];
		if (!derived.BasedOn.empty())
		{
			size_t baseIndex = 0;
			if (!findBase(index, baseIndex) || !resolve(baseIndex)) return false;
			const auto& base = out.Rules[baseIndex];
			if (!derived.HasType && base.HasType)
			{
				derived.HasType = true;
				derived.Type = base.Type;
				derived.XamlType = base.XamlType;
				derived.ComponentType = base.ComponentType;
			}
			else if (derived.XamlType.Empty() && base.XamlType.Valid()
				&& derived.ComponentType.Empty() && derived.Type == base.Type)
				derived.XamlType = base.XamlType;
			else if (derived.XamlType.Valid() && base.XamlType.Valid()
				&& derived.XamlType != base.XamlType)
				return Fail(L"Style.BasedOn 的内置 XAML TargetType 不兼容。", outError);
			else if (derived.ComponentType.Empty()
				&& !base.ComponentType.Empty()
				&& derived.Type == base.Type)
				derived.ComponentType = base.ComponentType;
			else if (!derived.ComponentType.Empty()
				&& !base.ComponentType.Empty()
				&& derived.ComponentType != base.ComponentType)
				return Fail(L"Style.BasedOn 的组件 TargetType 不兼容。", outError);
			auto setters = base.Setters;
			for (const auto& setter : derived.Setters)
			{
				auto existing = std::find_if(setters.begin(), setters.end(),
					[&](const DesignerStyleSetter& current)
					{
						return EqualsName(current.PropertyName, setter.PropertyName);
					});
				if (existing == setters.end()) setters.push_back(setter);
				else *existing = setter;
			}
			derived.Setters = std::move(setters);
			auto triggers = base.Triggers;
			triggers.insert(triggers.end(),
				derived.Triggers.begin(), derived.Triggers.end());
			derived.Triggers = std::move(triggers);
			derived.BasedOn.clear();
		}
		out.Rules[index] = std::move(derived);
		states[index] = 2;
		return true;
	};
	for (size_t index = 0; index < out.Rules.size(); ++index)
		if (!resolve(index)) return false;
	return true;
}

bool PrepareLocalRuntimeStyleSheet(
	const DesignerStyleSheet& localStyleSheet,
	const DesignerStyleSheet& visibleStyleSheet,
	DesignerStyleSheet& out,
	std::wstring* outError)
{
	DesignerStyleSheet resolved;
	if (!ResolveInheritance(visibleStyleSheet, resolved, outError)) return false;
	if (resolved.Rules.size() < localStyleSheet.Rules.size())
	{
		if (outError) *outError = L"局部 Style 的可见规则上下文不完整。";
		return false;
	}
	out = localStyleSheet;
	out.Rules.assign(
		resolved.Rules.end() - localStyleSheet.Rules.size(),
		resolved.Rules.end());
	size_t aliasIndex = 0;
	std::vector<std::pair<std::wstring, std::wstring>> capturedAliases;
	auto hasLocalResource = [&](const std::wstring& key)
	{
		return std::any_of(out.Resources.begin(), out.Resources.end(),
			[&](const auto& resource)
			{ return resource.Key == key; });
	};
	auto aliasStaticResource = [&](std::wstring& key) -> bool
	{
		if (hasLocalResource(key)) return true;
		const auto captured = std::find_if(
			capturedAliases.begin(), capturedAliases.end(),
			[&](const auto& alias) { return alias.first == key; });
		if (captured != capturedAliases.end())
		{
			key = captured->second;
			return true;
		}
		const auto originalKey = key;
		const auto found = std::find_if(
			visibleStyleSheet.Resources.rbegin(),
			visibleStyleSheet.Resources.rend(),
			[&](const auto& resource)
			{ return resource.Key == key; });
		if (found == visibleStyleSheet.Resources.rend())
		{
			if (outError) *outError = L"局部 Style 引用了不可见资源：" + key;
			return false;
		}
		auto alias = *found;
		do
		{
			alias.Key =
				L"__cui_static_scope_" + std::to_wstring(aliasIndex++);
		}
		while (hasLocalResource(alias.Key));
		alias.SourceDictionary.clear();
		capturedAliases.emplace_back(originalKey, alias.Key);
		key = alias.Key;
		out.Resources.push_back(std::move(alias));
		return true;
	};
	auto rewriteAnimation = [&](auto& animation)
	{
		if (animation.HasFrom && animation.FromUsesResource
			&& !aliasStaticResource(animation.FromResourceKey)) return false;
		if (animation.HasTo && animation.ToUsesResource
			&& !aliasStaticResource(animation.ToResourceKey)) return false;
		if (animation.HasBy && animation.ByUsesResource
			&& !aliasStaticResource(animation.ByResourceKey)) return false;
		for (auto& frame : animation.KeyFrames)
			if (frame.UsesResource
				&& !aliasStaticResource(frame.ResourceKey)) return false;
		return true;
	};
	auto rewriteActions = [&](auto& actions)
	{
		for (auto& action : actions)
			for (auto& animation : action.Animations)
				if (!rewriteAnimation(animation)) return false;
		return true;
	};
	auto rewriteSetters = [&](auto& setters)
	{
		for (auto& setter : setters)
			if (!IsStructuralResourceSetter(setter.PropertyName)
				&& setter.UsesResource && !setter.UsesDynamicResource
				&& !aliasStaticResource(setter.ResourceKey)) return false;
		return true;
	};
	for (auto& rule : out.Rules)
	{
		if (!rewriteSetters(rule.Setters)
			|| !rewriteActions(rule.EnterActions)
			|| !rewriteActions(rule.ExitActions)) return false;
		for (auto& trigger : rule.Triggers)
			if (!rewriteSetters(trigger.Setters)
				|| !rewriteActions(trigger.EnterActions)
				|| !rewriteActions(trigger.ExitActions)) return false;
	}
	return true;
}

bool ExpandRuntimeRules(
	const DesignerStyleSheet& source,
	DesignerStyleSheet& out,
	std::wstring* outError)
{
	DesignerStyleSheet inherited;
	if (!ResolveInheritance(source, inherited, outError)) return false;
	out = inherited;
	out.Rules.clear();
	for (const auto& rule : inherited.Rules)
	{
		if (!rule.Setters.empty())
		{
			auto normal = rule;
			normal.Triggers.clear();
			out.Rules.push_back(std::move(normal));
		}
		for (const auto& trigger : rule.Triggers)
		{
			if (!trigger.DataConditions.empty()
				&& !trigger.PropertyConditions.empty())
				return Fail(L"DataTrigger 不能同时包含属性 Condition。", outError);
			if (trigger.DataConditions.empty()
				&& trigger.PropertyConditions.empty())
				return Fail(L"Style Trigger 至少需要一个 Condition。", outError);
			auto lowered = rule;
			lowered.BasedOn.clear();
			lowered.Triggers.clear();
			lowered.Setters = trigger.Setters;
			lowered.EnterActions = trigger.EnterActions;
			lowered.ExitActions = trigger.ExitActions;
			lowered.DataConditions.insert(lowered.DataConditions.end(),
				trigger.DataConditions.begin(), trigger.DataConditions.end());
			lowered.PropertyConditions.insert(lowered.PropertyConditions.end(),
				trigger.PropertyConditions.begin(),
				trigger.PropertyConditions.end());
			out.Rules.push_back(std::move(lowered));
		}
	}
	return true;
}

bool Validate(
	const DesignerStyleSheet& styleSheet,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	if (outError) outError->clear();
	std::vector<std::wstring> dictionaryUris;
	for (const auto& dictionary : styleSheet.MergedDictionaries)
	{
		const auto uri = Trim(dictionary);
		if (uri.empty()) return Fail(L"合并资源字典 Source 不能为空。", outError);
		if (ContainsName(dictionaryUris, uri))
			return Fail(L"合并资源字典重复：" + uri, outError);
		dictionaryUris.push_back(uri);
	}
	auto validOrigin = [&](const std::wstring& origin)
	{
		return origin.empty() || ContainsName(dictionaryUris, Trim(origin));
	};
	std::vector<std::wstring> resourceKeys;
	for (const auto& resource : styleSheet.Resources)
	{
		if (!validOrigin(resource.SourceDictionary))
			return Fail(L"样式资源包含未知的来源字典："
				+ resource.SourceDictionary, outError);
		const auto key = Trim(resource.Key);
		if (key.empty()) return Fail(L"样式资源键不能为空。", outError);
		if (ContainsName(resourceKeys, key))
			return Fail(L"样式资源键重复：" + key, outError);
		BindingValue value;
		if (!TryConvertValue(
			resource.Value, value, outError, resourceBasePath, resources)) return false;
		resourceKeys.push_back(key);
	}

	for (size_t ruleIndex = 0; ruleIndex < styleSheet.Rules.size(); ++ruleIndex)
	{
		const auto& rule = styleSheet.Rules[ruleIndex];
		for (size_t previous = 0; previous < ruleIndex; ++previous)
			if (HasSameStyleResourceIdentity(
				styleSheet.Rules[previous], rule))
				return Fail(L"Style 资源键重复："
					+ (rule.Id.empty()
						? UIClassName(rule.Type) : rule.Id), outError);
		if (rule.Id.empty() && !rule.HasType
			&& rule.ComponentType.Empty() && !rule.XamlType.Valid())
			return Fail(L"隐式 Style 必须声明 TargetType。", outError);
		if (!rule.PropertyConditions.empty()
			|| !rule.DataConditions.empty()
			|| !rule.EnterActions.empty()
			|| !rule.ExitActions.empty())
			return Fail(L"Style 顶层只接受 Setter、BasedOn 和 Style.Triggers；"
				L"运行时条件不得进入作者模型。", outError);
		if (!validOrigin(rule.SourceDictionary))
			return Fail(L"样式规则包含未知的来源字典："
				+ rule.SourceDictionary, outError);
		if (!rule.ComponentType.Empty()
			&& (!rule.HasType || rule.ComponentType.XamlName.empty()
				|| rule.ComponentType.XamlNamespace.empty()))
			return Fail(L"组件样式必须同时保存有效 QName 和 BaseType。", outError);
		if (!rule.XamlType.Empty()
			&& (!rule.HasType || !rule.XamlType.Valid()
				|| !rule.ComponentType.Empty()))
			return Fail(L"内置样式必须保存唯一有效的 XAML TargetType。", outError);
		if (rule.Setters.empty() && rule.EnterActions.empty()
			&& rule.ExitActions.empty() && rule.Triggers.empty()
			&& Trim(rule.BasedOn).empty())
			return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 没有 Setter 或 Trigger。", outError);
		const bool hasTemplateSetter = std::any_of(
			rule.Setters.begin(), rule.Setters.end(), [](const auto& setter)
			{ return EqualsName(setter.PropertyName, L"Template"); });
		if (hasTemplateSetter
			&& (!rule.PropertyConditions.empty()
				|| !rule.DataConditions.empty()))
			return Fail(L"Template Setter 目前不支持状态或数据条件；"
				L"请使用普通 Style Setter。", outError);

			auto validateSetters = [&](const std::vector<DesignerStyleSetter>& setters,
			const std::wstring& context)
		{
			std::vector<std::wstring> properties;
			for (const auto& setter : setters)
			{
				const auto property = Trim(setter.PropertyName);
				if (property.empty())
					return Fail(context + L" Setter 属性名不能为空。", outError);
				if (ContainsName(properties, property))
					return Fail(context + L" 中的 Setter 属性重复：" + property, outError);
				properties.push_back(property);
				if (IsStructuralResourceSetter(property))
				{
					if (!setter.UsesResource || setter.UsesDynamicResource
						|| Trim(setter.ResourceKey).empty())
						return Fail(context
							+ L" " + property
							+ L" Setter 必须使用 StaticResource。", outError);
					continue;
				}
				if (setter.UsesResource)
				{
					const auto key = Trim(setter.ResourceKey);
					if (key.empty()
						|| (!setter.UsesDynamicResource
							&& !ContainsName(resourceKeys, key)))
						return Fail(L"样式 Setter 引用了不存在的资源：" + key, outError);
				}
				else
				{
					BindingValue value;
					if (!TryConvertValue(setter.Literal, value, outError,
						resourceBasePath, resources)) return false;
				}
			}
			return true;
		};
		auto validateDataConditions = [&](const auto& conditions,
			const std::wstring& context)
		{
			std::vector<std::wstring> paths;
			for (const auto& condition : conditions)
			{
				const auto path = DesignerBindingUtils::Trim(condition.SourceProperty);
				if (!DesignerBindingUtils::IsValidSourcePath(path))
					return Fail(context + L" Binding 路径无效：" + path, outError);
				if (ContainsName(paths, path))
					return Fail(context + L" Binding 路径重复：" + path, outError);
				paths.push_back(path);
				BindingValue value;
				if (!TryConvertValue(condition.Value, value, outError,
					resourceBasePath, resources)) return false;
				if (value.Empty() || value.Kind() == BindingValueKind::Object)
					return Fail(context
						+ L" Value 只支持 Bool、数字或 String 字面值。", outError);
			}
			return true;
		};
		auto validatePropertyConditions = [&](const auto& conditions,
			const std::wstring& context)
		{
			std::vector<std::wstring> properties;
			for (const auto& condition : conditions)
			{
				const auto property = Trim(condition.Property);
				if (property.empty())
					return Fail(context + L" 属性名不能为空。", outError);
				if (ContainsName(properties, property))
					return Fail(context + L" 属性重复：" + property, outError);
				properties.push_back(property);
				BindingValue value;
				if (!TryConvertValue(condition.Value, value, outError,
					resourceBasePath, resources)) return false;
				if (value.Empty())
					return Fail(context + L" 值不能为空：" + property, outError);
			}
			return true;
		};
		auto validateTriggerActions = [&](const DesignerStyleTrigger& trigger,
			const std::wstring& context)
		{
			if (trigger.EnterActions.empty() && trigger.ExitActions.empty())
				return true;
			std::vector<std::wstring> beginNames;
			std::vector<std::wstring> references;
			auto validateActions = [&](const auto& actions)
			{
				for (const auto& action : actions)
				{
					if (action.Kind == DesignerStoryboardActionKind::Begin)
					{
						if (action.Animations.empty())
							return Fail(context
								+ L"：BeginStoryboard 不能为空。", outError);
						if (!action.StoryboardName.empty())
						{
							if (ContainsName(beginNames, action.StoryboardName))
								return Fail(context
									+ L"：BeginStoryboard 名称重复："
									+ action.StoryboardName, outError);
							beginNames.push_back(action.StoryboardName);
						}
						for (const auto& animation : action.Animations)
						{
							if (!animation.TargetName.empty())
								return Fail(context
									+ L"：Style Storyboard 不支持 TargetName。",
									outError);
							if (animation.PropertyName.empty())
								return Fail(context
									+ L"：Storyboard.TargetProperty 不能为空。",
									outError);
							auto validateValue = [&](const DesignerStyleValue& literal,
								bool usesResource, const std::wstring& resourceKey)
							{
								if (usesResource)
									return !resourceKey.empty()
										&& ContainsName(resourceKeys, resourceKey);
								BindingValue value;
								return TryConvertValue(literal, value, outError,
									resourceBasePath, resources);
							};
							if ((animation.HasFrom && !validateValue(animation.From,
								animation.FromUsesResource, animation.FromResourceKey))
								|| (animation.HasTo && !validateValue(animation.To,
									animation.ToUsesResource, animation.ToResourceKey))
								|| (animation.HasBy && !validateValue(animation.By,
									animation.ByUsesResource, animation.ByResourceKey)))
								return Fail(context
									+ L"：Storyboard 端点资源或值无效。", outError);
							for (const auto& frame : animation.KeyFrames)
								if (!validateValue(frame.Value, frame.UsesResource,
									frame.ResourceKey))
									return Fail(context
										+ L"：Storyboard 关键帧资源或值无效。",
										outError);
							if (!std::isfinite(animation.RepeatCount)
								|| !std::isfinite(animation.SpeedRatio)
								|| animation.SpeedRatio <= 0.0
								|| !std::isfinite(animation.AccelerationRatio)
								|| !std::isfinite(animation.DecelerationRatio)
								|| animation.AccelerationRatio < 0.0
								|| animation.DecelerationRatio < 0.0
								|| animation.AccelerationRatio
									+ animation.DecelerationRatio > 1.0)
								return Fail(context
									+ L"：Storyboard 时间线参数无效。", outError);
						}
					}
					else
					{
						if (action.StoryboardName.empty())
							return Fail(context
								+ L"：Storyboard 控制动作缺少 BeginStoryboardName。",
								outError);
						references.push_back(action.StoryboardName);
					}
				}
				return true;
			};
			if (!validateActions(trigger.EnterActions)
				|| !validateActions(trigger.ExitActions)) return false;
			for (const auto& reference : references)
				if (!ContainsName(beginNames, reference))
					return Fail(context
						+ L"：Storyboard 控制动作找不到 BeginStoryboard："
						+ reference, outError);
			return true;
		};
		if (!validateSetters(rule.Setters,
			L"样式规则 " + std::to_wstring(ruleIndex + 1))) return false;
		if (!validatePropertyConditions(rule.PropertyConditions,
			L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 Trigger")) return false;
		if (!validateDataConditions(rule.DataConditions,
			L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 DataTrigger")) return false;
		if (!rule.EnterActions.empty() || !rule.ExitActions.empty())
		{
			DesignerStyleTrigger loweredTrigger;
			loweredTrigger.DataConditions = rule.DataConditions;
			loweredTrigger.PropertyConditions = rule.PropertyConditions;
			loweredTrigger.EnterActions = rule.EnterActions;
			loweredTrigger.ExitActions = rule.ExitActions;
			if (!validateTriggerActions(loweredTrigger,
				L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L" 的 Trigger")) return false;
		}
		if ((!rule.DataConditions.empty() || !rule.PropertyConditions.empty())
			&& !rule.Triggers.empty())
			return Fail(L"已降低的 Trigger 规则不能再包含嵌套 Trigger。",
				outError);
		for (size_t triggerIndex = 0;
			triggerIndex < rule.Triggers.size(); ++triggerIndex)
		{
			const auto& trigger = rule.Triggers[triggerIndex];
			if (!trigger.DataConditions.empty()
				&& !trigger.PropertyConditions.empty())
				return Fail(L"DataTrigger 不能同时包含属性 Condition。", outError);
			if (trigger.DataConditions.empty()
				&& trigger.PropertyConditions.empty())
				return Fail(L"Style Trigger 至少需要一个 Condition。", outError);
			if (!validatePropertyConditions(
				trigger.PropertyConditions,
				trigger.PropertyConditions.size() > 1
					? L"Style.MultiTrigger" : L"Style.Trigger")) return false;
			if (trigger.Setters.empty() && trigger.EnterActions.empty()
				&& trigger.ExitActions.empty())
				return Fail(L"Style Trigger 没有 Setter 或 TriggerAction。", outError);
			const auto structuralSetter = std::find_if(
				trigger.Setters.begin(), trigger.Setters.end(),
				[](const auto& setter)
				{ return IsStructuralResourceSetter(setter.PropertyName); });
			if (structuralSetter != trigger.Setters.end())
				return Fail(structuralSetter->PropertyName
					+ L" Setter 目前不支持 Trigger；"
					L"请使用 Style 的普通 Setter。", outError);
			if (!validateDataConditions(trigger.DataConditions,
				trigger.DataConditions.size() > 1
					? L"Style.MultiDataTrigger" : L"Style.DataTrigger")) return false;
			if (!validateSetters(trigger.Setters,
				L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L" 的 Trigger " + std::to_wstring(triggerIndex + 1))) return false;
			if (!validateTriggerActions(trigger,
				L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L" 的 Trigger " + std::to_wstring(triggerIndex + 1))) return false;
		}
	}
	DesignerStyleSheet resolved;
	if (!ResolveInheritance(styleSheet, resolved, outError)) return false;
	for (size_t ruleIndex = 0; ruleIndex < resolved.Rules.size(); ++ruleIndex)
		if (resolved.Rules[ruleIndex].Setters.empty()
			&& resolved.Rules[ruleIndex].EnterActions.empty()
			&& resolved.Rules[ruleIndex].ExitActions.empty()
			&& resolved.Rules[ruleIndex].Triggers.empty())
			return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 BasedOn 链没有提供 Setter 或 Trigger。", outError);
	DesignerStyleSheet lowered;
	if (!ExpandRuntimeRules(styleSheet, lowered, outError)) return false;
	return true;
}

bool ValidateAgainstRulePropertyMetadata(
	const DesignerStyleSheet& styleSheet,
	const RulePropertySchemaResolver& schemaResolver,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	if (!Validate(styleSheet, outError, resourceBasePath, resources)) return false;
	if (!schemaResolver)
	{
		if (outError) outError->clear();
		return true;
	}

	DesignerStyleSheet resolved;
	if (!ExpandRuntimeRules(styleSheet, resolved, outError)) return false;
	for (size_t ruleIndex = 0; ruleIndex < resolved.Rules.size(); ++ruleIndex)
	{
		const auto& rule = resolved.Rules[ruleIndex];
		CuiRuntime::XamlTypePropertySchema schema;
		std::wstring schemaError;
		if (!schemaResolver(rule, schema, &schemaError))
			return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 TargetType Schema 无法解析：" + schemaError, outError);
		const auto properties =
			DesignerPropertyCatalog::GetStyleProperties(schema.Properties);
		for (const auto& condition : rule.PropertyConditions)
		{
			const auto* metadata = schema.FindProperty(condition.Property);
			if (!metadata)
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L" 的目标类型没有可观察属性："
					+ condition.Property, outError);
			std::wstring validationError;
			if (!DesignerPropertyCatalog::ValidateConditionValue(
				*metadata, condition.Value, &validationError,
				resourceBasePath, resources))
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L"：" + validationError, outError);
		}

		for (const auto& setter : rule.Setters)
		{
			if (EqualsName(setter.PropertyName, L"Template")
				|| EqualsName(setter.PropertyName, L"ItemsPanel"))
			{
				// A declarative ComponentDefinition owns its XAML template
				// contract even when its private native behavior host is a
				// structural type such as Canvas. Built-in types must still
				// inherit the real Control.Template DP metadata.
				if (EqualsName(setter.PropertyName, L"Template")
					&& !rule.ComponentType.Empty())
					continue;
				const auto* metadata = schema.FindProperty(setter.PropertyName);
				if (!metadata || !metadata->CanWrite())
					return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
						+ L" 的目标类型没有可写 "
						+ setter.PropertyName + L" 属性。", outError);
				continue;
			}
			const auto* property = DesignerPropertyCatalog::Find(
				properties, setter.PropertyName);
			if (!property)
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L" 的目标类型没有可样式化属性：" + setter.PropertyName,
					outError);

			const DesignerStyleValue* value = &setter.Literal;
			if (setter.UsesResource)
			{
				const auto resource = std::find_if(
					resolved.Resources.begin(), resolved.Resources.end(),
					[&](const DesignerStyleResource& item)
					{
						return EqualsName(item.Key, setter.ResourceKey);
					});
				if (resource == resolved.Resources.end())
				{
					if (setter.UsesDynamicResource) continue;
					return Fail(L"样式 Setter 引用了不存在的资源："
						+ setter.ResourceKey, outError);
				}
				value = &resource->Value;
			}

			std::wstring validationError;
			DesignerStyleValue canonical;
			if (!property->Metadata || !DesignerPropertyCatalog::NormalizeStyleValue(
				*property->Metadata, *value, canonical, &validationError,
				resourceBasePath, resources))
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L"：" + validationError, outError);
		}
	}
	if (outError) outError->clear();
	return true;
}

bool ValidateAgainstPropertyMetadata(
	const DesignerStyleSheet& styleSheet,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	RulePropertySchemaResolver adapter = [](
		const DesignerStyleRule& rule,
		CuiRuntime::XamlTypePropertySchema& schema,
		std::wstring* error)
	{
		if (!rule.ComponentType.Empty())
		{
			if (error) *error = L"组件样式需要文档级 Schema resolver。";
			return false;
		}
		DesignerModel::DesignDocument emptyDocument;
		return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
			rule.HasType ? rule.Type : UIClass::UI_Base,
			nullptr, emptyDocument, schema, error);
	};
	return ValidateAgainstRulePropertyMetadata(
		styleSheet, adapter, outError, resourceBasePath, resources);
}

bool MaterializeStoryboardActions(
	const std::vector<DesignerEventTriggerAction>& sourceActions,
	const DesignerStyleSheet& styleSheet,
	std::vector<DeclarativeEventTriggerActionDefinition>& out,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources,
	const std::wstring& context)
{
	out.clear();
	out.reserve(sourceActions.size());
	auto materializeAnimation = [&](const DesignerVisualStateAnimation& source,
		DeclarativeVisualStateAnimation& animation)
	{
		animation.Kind = source.Kind == DesignerAnimationKind::Color
			? DeclarativeAnimationKind::Color
			: source.Kind == DesignerAnimationKind::Thickness
				? DeclarativeAnimationKind::Thickness
			: source.Kind == DesignerAnimationKind::Point
				? DeclarativeAnimationKind::Point
			: source.Kind == DesignerAnimationKind::Vector
				? DeclarativeAnimationKind::Vector
			: source.Kind == DesignerAnimationKind::Rect
				? DeclarativeAnimationKind::Rect
			: source.Kind == DesignerAnimationKind::Size
				? DeclarativeAnimationKind::Size
			: source.Kind == DesignerAnimationKind::Matrix
				? DeclarativeAnimationKind::Matrix
			: source.Kind == DesignerAnimationKind::Object
				? DeclarativeAnimationKind::Object
				: DeclarativeAnimationKind::Double;
		animation.TargetName = source.TargetName;
		animation.PropertyName = source.PropertyName;
		auto resolveValue = [&](const DesignerStyleValue& literal,
			bool usesResource, const std::wstring& resourceKey,
			BindingValue& output, const std::wstring& label)
		{
			const DesignerStyleValue* value = &literal;
			if (usesResource)
			{
				const auto found = std::find_if(styleSheet.Resources.begin(),
					styleSheet.Resources.end(), [&](const auto& candidate)
					{ return EqualsName(candidate.Key, resourceKey); });
				if (found == styleSheet.Resources.end())
					return Fail(context + L" 引用了不存在的动画资源："
						+ resourceKey, outError);
				value = &found->Value;
			}
			std::wstring conversionError;
			if (TryConvertValue(*value, output, &conversionError,
				resourceBasePath, resources)) return true;
			return Fail(context + L"." + source.PropertyName + L" " + label
				+ L"：" + conversionError, outError);
		};
		if (source.HasFrom)
		{
			BindingValue value;
			if (!resolveValue(source.From, source.FromUsesResource,
				source.FromResourceKey, value, L"From")) return false;
			animation.From = std::move(value);
		}
		if (source.HasTo)
		{
			BindingValue value;
			if (!resolveValue(source.To, source.ToUsesResource,
				source.ToResourceKey, value, L"To")) return false;
			animation.To = std::move(value);
		}
		if (source.HasBy)
		{
			BindingValue value;
			if (!resolveValue(source.By, source.ByUsesResource,
				source.ByResourceKey, value, L"By")) return false;
			animation.By = std::move(value);
		}
		for (const auto& sourceFrame : source.KeyFrames)
		{
			DeclarativeAnimationKeyFrame frame;
			frame.Kind = sourceFrame.Kind == DesignerKeyFrameKind::Discrete
				? DeclarativeKeyFrameKind::Discrete
				: sourceFrame.Kind == DesignerKeyFrameKind::Easing
					? DeclarativeKeyFrameKind::Easing
				: sourceFrame.Kind == DesignerKeyFrameKind::Spline
					? DeclarativeKeyFrameKind::Spline
					: DeclarativeKeyFrameKind::Linear;
			frame.KeyTimeMilliseconds = sourceFrame.KeyTimeMilliseconds;
			if (!resolveValue(sourceFrame.Value, sourceFrame.UsesResource,
				sourceFrame.ResourceKey, frame.Value, L"KeyFrame")) return false;
			frame.Easing = sourceFrame.Easing == DesignerEasingKind::Quadratic
				? DeclarativeEasingKind::Quadratic
				: sourceFrame.Easing == DesignerEasingKind::Cubic
					? DeclarativeEasingKind::Cubic
				: sourceFrame.Easing == DesignerEasingKind::Sine
					? DeclarativeEasingKind::Sine
					: DeclarativeEasingKind::Linear;
			frame.EasingMode = sourceFrame.EasingMode
				== DesignerEasingMode::EaseIn
				? DeclarativeEasingMode::EaseIn
				: sourceFrame.EasingMode == DesignerEasingMode::EaseInOut
					? DeclarativeEasingMode::EaseInOut
					: DeclarativeEasingMode::EaseOut;
			frame.KeySplineX1 = sourceFrame.KeySplineX1;
			frame.KeySplineY1 = sourceFrame.KeySplineY1;
			frame.KeySplineX2 = sourceFrame.KeySplineX2;
			frame.KeySplineY2 = sourceFrame.KeySplineY2;
			animation.KeyFrames.push_back(std::move(frame));
		}
		animation.IsAdditive = source.IsAdditive;
		animation.IsCumulative = source.IsCumulative;
		animation.BeginTimeMilliseconds = source.BeginTimeMilliseconds;
		animation.DurationMilliseconds = source.DurationMilliseconds;
		animation.RepeatBehavior = source.RepeatBehavior
			== DesignerRepeatBehaviorKind::Duration
			? DeclarativeRepeatBehaviorKind::Duration
			: source.RepeatBehavior == DesignerRepeatBehaviorKind::Forever
				? DeclarativeRepeatBehaviorKind::Forever
				: DeclarativeRepeatBehaviorKind::Count;
		animation.RepeatCount = source.RepeatCount;
		animation.RepeatDurationMilliseconds =
			source.RepeatDurationMilliseconds;
		animation.AutoReverse = source.AutoReverse;
		animation.FillBehavior = source.FillBehavior
			== DesignerTimelineFillBehavior::Stop
			? DeclarativeTimelineFillBehavior::Stop
			: DeclarativeTimelineFillBehavior::HoldEnd;
		animation.SpeedRatio = source.SpeedRatio;
		animation.AccelerationRatio = source.AccelerationRatio;
		animation.DecelerationRatio = source.DecelerationRatio;
		animation.Easing = source.Easing == DesignerEasingKind::Quadratic
			? DeclarativeEasingKind::Quadratic
			: source.Easing == DesignerEasingKind::Cubic
				? DeclarativeEasingKind::Cubic
			: source.Easing == DesignerEasingKind::Sine
				? DeclarativeEasingKind::Sine
				: DeclarativeEasingKind::Linear;
		animation.EasingMode = source.EasingMode == DesignerEasingMode::EaseIn
			? DeclarativeEasingMode::EaseIn
			: source.EasingMode == DesignerEasingMode::EaseInOut
				? DeclarativeEasingMode::EaseInOut
				: DeclarativeEasingMode::EaseOut;
		return true;
	};

	for (const auto& sourceAction : sourceActions)
	{
		DeclarativeEventTriggerActionDefinition action;
		action.Kind = sourceAction.Kind == DesignerStoryboardActionKind::Begin
			? DeclarativeStoryboardActionKind::Begin
			: sourceAction.Kind == DesignerStoryboardActionKind::Pause
				? DeclarativeStoryboardActionKind::Pause
			: sourceAction.Kind == DesignerStoryboardActionKind::Resume
				? DeclarativeStoryboardActionKind::Resume
				: DeclarativeStoryboardActionKind::Stop;
		action.StoryboardName = sourceAction.StoryboardName;
		for (const auto& sourceAnimation : sourceAction.Animations)
		{
			DeclarativeVisualStateAnimation animation;
			if (!materializeAnimation(sourceAnimation, animation)) return false;
			action.Animations.push_back(std::move(animation));
		}
		out.push_back(std::move(action));
	}
	if (outError) outError->clear();
	return true;
}

bool BuildRuntimeStyleSheet(
	const DesignerStyleSheet& source,
	std::shared_ptr<ControlStyleSheet>& out,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources,
	const std::vector<RuntimeStyleResource>& supplementalResources)
{
	auto styleSheet = source;
	Canonicalize(styleSheet);
	if (!Validate(styleSheet, outError, resourceBasePath, resources)) return false;
	DesignerStyleSheet resolved;
	if (!ExpandRuntimeRules(styleSheet, resolved, outError)) return false;
	styleSheet = std::move(resolved);

	auto runtime = std::make_shared<ControlStyleSheet>();
	for (const auto& resource : styleSheet.Resources)
	{
		BindingValue value;
		if (!TryConvertValue(
			resource.Value, value, outError, resourceBasePath, resources)) return false;
		if (!runtime->SetResource(resource.Key, std::move(value)))
			return Fail(L"无法创建样式资源：" + resource.Key, outError);
	}
	for (const auto& [key, value] : supplementalResources)
	{
		if (Trim(key).empty() || value.Empty()
			|| !runtime->SetResource(key, value))
			return Fail(L"无法创建结构型样式资源：" + key, outError);
	}
	auto hasSupplementalResource = [&](const std::wstring& key)
	{
		return std::any_of(
			supplementalResources.begin(), supplementalResources.end(),
			[&](const auto& candidate)
			{ return EqualsName(candidate.first, key); });
	};
	for (const auto& rule : styleSheet.Rules)
	{
		ControlStyleSelector selector;
		if (rule.HasType) selector.Type = rule.Type;
		if (!rule.ComponentType.Empty())
		{
			selector.DeclarativeTypeNamespace = rule.ComponentType.XamlNamespace;
			selector.DeclarativeTypeName = rule.ComponentType.XamlName;
		}
		else if (rule.XamlType.Valid())
		{
			selector.DeclarativeTypeNamespace = rule.XamlType.NamespaceUri;
			selector.DeclarativeTypeName = rule.XamlType.LocalName;
		}
		selector.StyleResourceKey = rule.Id;
		for (const auto& condition : rule.PropertyConditions)
		{
			BindingValue value;
			if (!TryConvertValue(condition.Value, value, outError,
				resourceBasePath, resources)) return false;
			selector.PropertyConditions.push_back({
				condition.Property, std::move(value) });
		}
		for (const auto& condition : rule.DataConditions)
		{
			BindingValue value;
			if (!TryConvertValue(condition.Value, value, outError,
				resourceBasePath, resources)) return false;
			selector.DataConditions.push_back({
				condition.SourceProperty, std::move(value) });
		}
		std::vector<ControlStyleSetter> setters;
		setters.reserve(rule.Setters.size());
		for (const auto& setter : rule.Setters)
		{
			if (EqualsName(setter.PropertyName, L"Template")
				&& (!setter.UsesResource || setter.UsesDynamicResource
					|| !hasSupplementalResource(setter.ResourceKey)))
				return Fail(L"Template Setter 引用了不存在的 "
					L"ControlTemplate：" + setter.ResourceKey, outError);
			if (EqualsName(setter.PropertyName, L"ItemsPanel")
				&& (!setter.UsesResource || setter.UsesDynamicResource
					|| !hasSupplementalResource(setter.ResourceKey)))
				return Fail(L"ItemsPanel Setter 引用了不存在的 "
					L"ItemsPanelTemplate：" + setter.ResourceKey, outError);
			if (setter.UsesResource)
				setters.push_back(setter.UsesDynamicResource
					? ControlStyleSetter::DynamicResource(
						setter.PropertyName, setter.ResourceKey)
					: ControlStyleSetter::Resource(
						setter.PropertyName, setter.ResourceKey));
			else
			{
				BindingValue value;
				if (!TryConvertValue(
					setter.Literal, value, outError, resourceBasePath, resources)) return false;
				setters.emplace_back(setter.PropertyName, std::move(value));
			}
		}
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions;
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions;
		if (!MaterializeStoryboardActions(
			rule.EnterActions, styleSheet, enterActions, outError,
			resourceBasePath, resources, L"Style Trigger.EnterActions")
			|| !MaterializeStoryboardActions(
				rule.ExitActions, styleSheet, exitActions, outError,
				resourceBasePath, resources,
				L"Style Trigger.ExitActions")) return false;
		if (setters.empty() && enterActions.empty() && exitActions.empty())
			continue;
		if (!runtime->AddRule(std::move(selector), std::move(setters),
			std::move(enterActions), std::move(exitActions)))
			return Fail(L"无法创建样式规则。", outError);
	}
	out = std::move(runtime);
	return true;
}
}
