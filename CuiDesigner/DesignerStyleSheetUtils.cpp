#include "DesignerStyleSheetUtils.h"
#include "DesignerBindingUtils.h"
#include "DesignerPropertyCatalog.h"
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
namespace
{
	constexpr uint32_t KnownStateMask =
		static_cast<uint32_t>(ControlStyleState::Hovered)
		| static_cast<uint32_t>(ControlStyleState::Focused)
		| static_cast<uint32_t>(ControlStyleState::Pressed)
		| static_cast<uint32_t>(ControlStyleState::Disabled)
		| static_cast<uint32_t>(ControlStyleState::Checked)
		| static_cast<uint32_t>(ControlStyleState::Selected);

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
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

	bool TryParseStateName(const std::wstring& value, ControlStyleState& out)
	{
		if (EqualsName(value, L"Hovered")) out = ControlStyleState::Hovered;
		else if (EqualsName(value, L"Focused")) out = ControlStyleState::Focused;
		else if (EqualsName(value, L"Pressed")) out = ControlStyleState::Pressed;
		else if (EqualsName(value, L"Disabled")) out = ControlStyleState::Disabled;
		else if (EqualsName(value, L"Checked")) out = ControlStyleState::Checked;
		else if (EqualsName(value, L"Selected")) out = ControlStyleState::Selected;
		else return false;
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

	bool TryConvertBrush(
		const DesignerModel::DesignValue& value,
		cui::drawing::Brush& output,
		std::wstring* outError,
		const std::wstring& resourceBasePath,
		const std::shared_ptr<ResourceLoadContext>& resources)
	{
		if (!value.is_object()) return Fail(L"画刷必须是对象。", outError);
		const auto type = value.value("type", std::string{});
		if (type == "solid") output.Kind = cui::drawing::BrushKind::Solid;
		else if (type == "linear") output.Kind = cui::drawing::BrushKind::LinearGradient;
		else if (type == "radial") output.Kind = cui::drawing::BrushKind::RadialGradient;
		else if (type == "image") output.Kind = cui::drawing::BrushKind::Image;
		else return Fail(L"画刷类型无效。", outError);

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
	case DesignerStyleValueKind::Size: return L"Size";
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
		DesignerStyleValueKind::Size, DesignerStyleValueKind::Length,
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
		L"Color", L"Thickness", L"Size", L"Length", L"ImageSource", L"Brush",
		L"Geometry", L"Transform" };
}

std::wstring UIClassName(UIClass type)
{
#define CUI_STYLE_WIDEN_INNER(value) L##value
#define CUI_STYLE_WIDEN(value) CUI_STYLE_WIDEN_INNER(value)
#define CUI_STYLE_UI_NAME(name) case UIClass::UI_##name: return CUI_STYLE_WIDEN(#name)
	switch (type)
	{
	CUI_STYLE_UI_NAME(Base); CUI_STYLE_UI_NAME(Label); CUI_STYLE_UI_NAME(LinkLabel);
	CUI_STYLE_UI_NAME(Button); CUI_STYLE_UI_NAME(PictureBox); CUI_STYLE_UI_NAME(TextBox);
	CUI_STYLE_UI_NAME(RichTextBox); CUI_STYLE_UI_NAME(PasswordBox); CUI_STYLE_UI_NAME(ComboBox);
	CUI_STYLE_UI_NAME(ListView); CUI_STYLE_UI_NAME(ListBox); CUI_STYLE_UI_NAME(GridView);
	CUI_STYLE_UI_NAME(PropertyGrid); CUI_STYLE_UI_NAME(CheckBox); CUI_STYLE_UI_NAME(RadioBox);
	CUI_STYLE_UI_NAME(ProgressBar); CUI_STYLE_UI_NAME(LoadingRing); CUI_STYLE_UI_NAME(ProgressRing);
	CUI_STYLE_UI_NAME(TreeView); CUI_STYLE_UI_NAME(Panel); CUI_STYLE_UI_NAME(GroupBox);
	CUI_STYLE_UI_NAME(ScrollView); CUI_STYLE_UI_NAME(TabPage); CUI_STYLE_UI_NAME(TabControl);
	CUI_STYLE_UI_NAME(Switch); CUI_STYLE_UI_NAME(Menu); CUI_STYLE_UI_NAME(MenuItem);
	CUI_STYLE_UI_NAME(ToolBar); CUI_STYLE_UI_NAME(StatusBar); CUI_STYLE_UI_NAME(Slider);
	CUI_STYLE_UI_NAME(WebBrowser); CUI_STYLE_UI_NAME(MediaPlayer); CUI_STYLE_UI_NAME(StackPanel);
	CUI_STYLE_UI_NAME(GridPanel); CUI_STYLE_UI_NAME(DockPanel); CUI_STYLE_UI_NAME(WrapPanel);
	CUI_STYLE_UI_NAME(RelativePanel); CUI_STYLE_UI_NAME(SplitContainer); CUI_STYLE_UI_NAME(DateTimePicker);
	CUI_STYLE_UI_NAME(ToolTip); CUI_STYLE_UI_NAME(ContextMenu); CUI_STYLE_UI_NAME(ToastHost);
	CUI_STYLE_UI_NAME(ChartView); CUI_STYLE_UI_NAME(ReportView); CUI_STYLE_UI_NAME(KpiCard);
	CUI_STYLE_UI_NAME(FilterBar); CUI_STYLE_UI_NAME(NavigationView); CUI_STYLE_UI_NAME(SideBar);
	CUI_STYLE_UI_NAME(BreadcrumbBar); CUI_STYLE_UI_NAME(CalendarView); CUI_STYLE_UI_NAME(DateRangePicker);
	CUI_STYLE_UI_NAME(ColorPicker); CUI_STYLE_UI_NAME(PagedGridView); CUI_STYLE_UI_NAME(NumericUpDown);
	CUI_STYLE_UI_NAME(Expander); CUI_STYLE_UI_NAME(CUSTOM);
	}
#undef CUI_STYLE_UI_NAME
#undef CUI_STYLE_WIDEN
#undef CUI_STYLE_WIDEN_INNER
	return L"Base";
}

bool TryParseUIClass(const std::wstring& value, UIClass& out)
{
	const auto name = Trim(value);
	for (int numeric = static_cast<int>(UIClass::UI_Base);
		numeric <= static_cast<int>(UIClass::UI_CUSTOM); ++numeric)
	{
		const auto candidate = static_cast<UIClass>(numeric);
		if (EqualsName(name, UIClassName(candidate)))
		{
			out = candidate;
			return true;
		}
	}
	return false;
}

std::vector<std::wstring> UIClassNames(bool includeAny)
{
	std::vector<std::wstring> result;
	if (includeAny) result.push_back(L"Any");
	for (int numeric = static_cast<int>(UIClass::UI_Base);
		numeric <= static_cast<int>(UIClass::UI_CUSTOM); ++numeric)
		result.push_back(UIClassName(static_cast<UIClass>(numeric)));
	return result;
}

std::wstring FormatStates(ControlStyleState states)
{
	std::wstring result;
	for (const auto& [state, name] : {
		std::pair{ ControlStyleState::Hovered, L"Hovered" },
		std::pair{ ControlStyleState::Focused, L"Focused" },
		std::pair{ ControlStyleState::Pressed, L"Pressed" },
		std::pair{ ControlStyleState::Disabled, L"Disabled" },
		std::pair{ ControlStyleState::Checked, L"Checked" },
		std::pair{ ControlStyleState::Selected, L"Selected" } })
	{
		if ((states & state) == ControlStyleState::None) continue;
		if (!result.empty()) result += L", ";
		result += name;
	}
	return result;
}

bool TryParseStates(const std::wstring& value, ControlStyleState& out)
{
	out = ControlStyleState::None;
	std::wstring normalized = value;
	std::replace(normalized.begin(), normalized.end(), L'|', L',');
	for (const auto& part : Split(normalized, L','))
	{
		if (part.empty()) continue;
		ControlStyleState state = ControlStyleState::None;
		if (!TryParseStateName(part, state)) return false;
		out |= state;
	}
	return true;
}

std::vector<std::wstring> SplitClasses(const std::wstring& value)
{
	std::vector<std::wstring> result;
	for (const auto& item : Split(value, L','))
	{
		if (!item.empty() && !ContainsName(result, item)) result.push_back(item);
	}
	return result;
}

std::wstring JoinClasses(const std::vector<std::wstring>& classes)
{
	std::wstring result;
	for (const auto& value : classes)
	{
		if (!result.empty()) result += L", ";
		result += value;
	}
	return result;
}

std::wstring CanonicalTriggerProperty(const std::wstring& property)
{
	const auto value = Lower(Trim(property));
	if (value == L"ismouseover" || value == L"hovered") return L"IsMouseOver";
	if (value == L"iskeyboardfocused" || value == L"isfocused"
		|| value == L"focused") return L"IsKeyboardFocused";
	if (value == L"ispressed" || value == L"pressed") return L"IsPressed";
	if (value == L"isenabled" || value == L"enable"
		|| value == L"enabled") return L"IsEnabled";
	if (value == L"ischecked" || value == L"checked") return L"IsChecked";
	if (value == L"isselected" || value == L"selected") return L"IsSelected";
	return {};
}

bool TryGetTriggerStates(
	const std::wstring& property,
	bool value,
	ControlStyleState& required,
	ControlStyleState& excluded)
{
	required = ControlStyleState::None;
	excluded = ControlStyleState::None;
	const auto canonical = CanonicalTriggerProperty(property);
	ControlStyleState state = ControlStyleState::None;
	bool inverted = false;
	if (canonical == L"IsMouseOver") state = ControlStyleState::Hovered;
	else if (canonical == L"IsKeyboardFocused") state = ControlStyleState::Focused;
	else if (canonical == L"IsPressed") state = ControlStyleState::Pressed;
	else if (canonical == L"IsEnabled")
	{
		state = ControlStyleState::Disabled;
		inverted = true;
	}
	else if (canonical == L"IsChecked") state = ControlStyleState::Checked;
	else if (canonical == L"IsSelected") state = ControlStyleState::Selected;
	else return false;
	const bool requireState = inverted ? !value : value;
	if (requireState) required = state;
	else excluded = state;
	return true;
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
	case DesignerStyleValueKind::Size:
	{
		auto parts = Split(value.Text, L',');
		long long width = 0, height = 0;
		if (parts.size() != 2 || !TryParseLongLong(parts[0], width)
			|| !TryParseLongLong(parts[1], height)
			|| width < (std::numeric_limits<LONG>::min)()
			|| width > (std::numeric_limits<LONG>::max)()
			|| height < (std::numeric_limits<LONG>::min)()
			|| height > (std::numeric_limits<LONG>::max)()) return invalid();
		out = BindingValue(SIZE{ static_cast<LONG>(width), static_cast<LONG>(height) });
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
		if (!TryConvertBrush(
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
		rule.Id = Trim(rule.Id);
		rule.BasedOn = Trim(rule.BasedOn);
		rule.SourceDictionary = Trim(rule.SourceDictionary);
		auto classes = rule.Classes;
		rule.Classes.clear();
		for (auto& value : classes)
		{
			value = Trim(value);
			if (!value.empty() && !ContainsName(rule.Classes, value))
				rule.Classes.push_back(std::move(value));
		}
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
		for (auto& setter : rule.Setters) canonicalizeSetter(setter);
		for (auto& condition : rule.DataConditions)
			canonicalizeDataCondition(condition);
		for (auto& trigger : rule.Triggers)
		{
			for (auto& condition : trigger.Conditions)
			{
				const auto property = CanonicalTriggerProperty(condition.Property);
				condition.Property = property.empty()
					? Trim(condition.Property) : property;
			}
			for (auto& condition : trigger.DataConditions)
				canonicalizeDataCondition(condition);
			for (auto& setter : trigger.Setters) canonicalizeSetter(setter);
		}
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
		return rule.Classes.empty()
			&& rule.RequiredStates == ControlStyleState::None
			&& rule.ExcludedStates == ControlStyleState::None
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
			}
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
			ControlStyleState required = ControlStyleState::None;
			ControlStyleState excluded = ControlStyleState::None;
			if (!trigger.DataConditions.empty() && !trigger.Conditions.empty())
				return Fail(L"数据 Trigger 不能同时包含状态 Condition。", outError);
			if (trigger.DataConditions.empty() && trigger.Conditions.empty())
				return Fail(L"Style Trigger 至少需要一个 Condition。", outError);
			for (const auto& condition : trigger.Conditions)
			{
				ControlStyleState conditionRequired = ControlStyleState::None;
				ControlStyleState conditionExcluded = ControlStyleState::None;
				if (!TryGetTriggerStates(condition.Property, condition.Value,
					conditionRequired, conditionExcluded))
					return Fail(L"Style Trigger 不支持条件属性："
						+ condition.Property, outError);
				required |= conditionRequired;
				excluded |= conditionExcluded;
			}
			auto lowered = rule;
			lowered.BasedOn.clear();
			lowered.Triggers.clear();
			lowered.Setters = trigger.Setters;
			lowered.DataConditions.insert(lowered.DataConditions.end(),
				trigger.DataConditions.begin(), trigger.DataConditions.end());
			lowered.RequiredStates |= required;
			lowered.ExcludedStates |= excluded;
			if ((lowered.RequiredStates & lowered.ExcludedStates)
				!= ControlStyleState::None)
				return Fail(L"Style Trigger 条件彼此冲突或与样式状态选择器冲突。",
					outError);
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
		if (!validOrigin(rule.SourceDictionary))
			return Fail(L"样式规则包含未知的来源字典："
				+ rule.SourceDictionary, outError);
		const auto required = static_cast<uint32_t>(rule.RequiredStates);
		const auto excluded = static_cast<uint32_t>(rule.ExcludedStates);
		if ((required & ~KnownStateMask) != 0 || (excluded & ~KnownStateMask) != 0)
			return Fail(L"样式规则包含未知状态。", outError);
		if ((required & excluded) != 0)
			return Fail(L"同一状态不能同时出现在规则的必需和排除状态中。", outError);
		if (rule.Setters.empty() && rule.Triggers.empty()
			&& Trim(rule.BasedOn).empty())
			return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 没有 Setter 或 Trigger。", outError);

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
				if (setter.UsesResource)
				{
					const auto key = Trim(setter.ResourceKey);
					if (key.empty() || !ContainsName(resourceKeys, key))
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
		if (!validateSetters(rule.Setters,
			L"样式规则 " + std::to_wstring(ruleIndex + 1))) return false;
		if (!validateDataConditions(rule.DataConditions,
			L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 DataTrigger")) return false;
		if (!rule.DataConditions.empty() && !rule.Triggers.empty())
			return Fail(L"已降低的 DataTrigger 规则不能再包含嵌套 Trigger。",
				outError);
		for (size_t triggerIndex = 0;
			triggerIndex < rule.Triggers.size(); ++triggerIndex)
		{
			const auto& trigger = rule.Triggers[triggerIndex];
			ControlStyleState triggerRequired = ControlStyleState::None;
			ControlStyleState triggerExcluded = ControlStyleState::None;
			if (!trigger.DataConditions.empty() && !trigger.Conditions.empty())
				return Fail(L"数据 Trigger 不能同时包含状态 Condition。", outError);
			if (trigger.DataConditions.empty() && trigger.Conditions.empty())
				return Fail(L"Style Trigger 至少需要一个 Condition。", outError);
			std::vector<std::wstring> conditionProperties;
			for (const auto& condition : trigger.Conditions)
			{
				const auto property = CanonicalTriggerProperty(condition.Property);
				if (property.empty())
					return Fail(L"Style Trigger 不支持条件属性："
						+ Trim(condition.Property), outError);
				if (ContainsName(conditionProperties, property))
					return Fail(L"Style MultiTrigger 的 Condition 属性重复："
						+ property, outError);
				conditionProperties.push_back(property);
				ControlStyleState conditionRequired = ControlStyleState::None;
				ControlStyleState conditionExcluded = ControlStyleState::None;
				if (!TryGetTriggerStates(property, condition.Value,
					conditionRequired, conditionExcluded))
					return Fail(L"Style Trigger 不支持条件属性："
						+ property, outError);
				triggerRequired |= conditionRequired;
				triggerExcluded |= conditionExcluded;
			}
			if (trigger.Setters.empty())
				return Fail(L"Style Trigger 没有 Setter。", outError);
			if (!validateDataConditions(trigger.DataConditions,
				trigger.DataConditions.size() > 1
					? L"Style.MultiDataTrigger" : L"Style.DataTrigger")) return false;
			if (((rule.RequiredStates | triggerRequired)
				& (rule.ExcludedStates | triggerExcluded))
				!= ControlStyleState::None)
				return Fail(L"Style Trigger 条件彼此冲突或与样式状态选择器冲突。",
					outError);
			if (!validateSetters(trigger.Setters,
				L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 Trigger " + std::to_wstring(triggerIndex + 1))) return false;
		}
	}
	DesignerStyleSheet resolved;
	if (!ResolveInheritance(styleSheet, resolved, outError)) return false;
	for (size_t ruleIndex = 0; ruleIndex < resolved.Rules.size(); ++ruleIndex)
		if (resolved.Rules[ruleIndex].Setters.empty()
			&& resolved.Rules[ruleIndex].Triggers.empty())
			return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
				+ L" 的 BasedOn 链没有提供 Setter 或 Trigger。", outError);
	DesignerStyleSheet lowered;
	if (!ExpandRuntimeRules(styleSheet, lowered, outError)) return false;
	return true;
}

bool ValidateAgainstPropertyMetadata(
	const DesignerStyleSheet& styleSheet,
	const ControlFactory& controlFactory,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
{
	if (!Validate(styleSheet, outError, resourceBasePath, resources)) return false;
	if (!controlFactory)
	{
		if (outError) outError->clear();
		return true;
	}

	DesignerStyleSheet resolved;
	if (!ExpandRuntimeRules(styleSheet, resolved, outError)) return false;
	for (size_t ruleIndex = 0; ruleIndex < resolved.Rules.size(); ++ruleIndex)
	{
		const auto& rule = resolved.Rules[ruleIndex];
		auto target = controlFactory(rule.HasType ? rule.Type : UIClass::UI_Base);
		// Unknown/custom runtime types may not have a Designer probe. Keep their
		// structurally valid declarations for forward compatibility.
		if (!target) continue;
		const auto properties = DesignerPropertyCatalog::GetStyleProperties(*target);

		for (const auto& setter : rule.Setters)
		{
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
					return Fail(L"样式 Setter 引用了不存在的资源："
						+ setter.ResourceKey, outError);
				value = &resource->Value;
			}

			std::wstring validationError;
			if (!DesignerPropertyCatalog::ValidateStyleValue(
				*target, setter.PropertyName, *value, &validationError,
				resourceBasePath, resources))
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L"：" + validationError, outError);
		}
	}
	if (outError) outError->clear();
	return true;
}

bool BuildRuntimeStyleSheet(
	const DesignerStyleSheet& source,
	std::shared_ptr<ControlStyleSheet>& out,
	std::wstring* outError,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources)
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
	for (const auto& rule : styleSheet.Rules)
	{
		ControlStyleSelector selector;
		if (rule.HasType) selector.Type = rule.Type;
		selector.Id = rule.Id;
		selector.Classes = rule.Classes;
		selector.RequiredStates = rule.RequiredStates;
		selector.ExcludedStates = rule.ExcludedStates;
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
			if (setter.UsesResource)
				setters.push_back(ControlStyleSetter::Resource(
					setter.PropertyName, setter.ResourceKey));
			else
			{
				BindingValue value;
				if (!TryConvertValue(
					setter.Literal, value, outError, resourceBasePath, resources)) return false;
				setters.emplace_back(setter.PropertyName, std::move(value));
			}
		}
		runtime->AddRule(std::move(selector), std::move(setters));
	}
	out = std::move(runtime);
	return true;
}
}
