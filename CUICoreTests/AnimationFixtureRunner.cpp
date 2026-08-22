#include "AnimationFixtureRunner.h"

#include "TestRunner.h"
#include "../CuiDesigner/DesignerModel/AtomicFile.h"
#include "../CuiDesigner/DesignerModel/DesignDocumentSerializer.h"
#include "../CuiDesigner/DesignerModel/RuntimeDocument.h"
#include "../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../CuiDesigner/DesignerModel/XamlDocumentSerializer.h"

#include <Canvas.h>
#include <Border.h>
#include <Convert.h>
#include <DependencyProperty.h>
#include <PresentationInfrastructure.h>
#include <SHA256.h>
#include <Style.h>
#include <StyleInfrastructure.h>
#include <TemplateInfrastructure.h>
#include <XamlInfrastructure.h>
#include <Xml.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#pragma comment(lib, "Shell32.lib")

namespace
{
	constexpr std::size_t MaximumCorpusBytes = 4u * 1024u * 1024u;
	constexpr std::size_t MaximumTimelineBytes = 1024u * 1024u;
	constexpr unsigned long long FixtureClockOrigin = 1'000'000ull;
	constexpr std::string_view RunnerVersion = "0.38.0";

	struct AnimationOperation final
	{
		unsigned long long AtMilliseconds = 0;
		std::string Kind;
		std::optional<double> Value;
	};

	struct AnimationSampleRequest final
	{
		unsigned long long AtMilliseconds = 0;
		std::string Label;
		std::string Phase = "after-begin";
	};

	struct AnimationTarget final
	{
		std::string Kind;
		std::string Name;
		std::string PropertyPath;
		std::string Probe;
		double BaseValue = 0.0;
		std::optional<int> BaseInt32;
		std::optional<long long> BaseInt64;
		std::optional<float> BaseSingle;
		std::optional<bool> BaseBoolean;
		std::optional<std::string> BaseString;
		std::optional<D2D1_COLOR_F> BaseColor;
		std::string BaseColorText;
		std::optional<cui::core::Vector> BaseVector;
		std::string BaseVectorText;
		std::optional<cui::core::Rect> BaseRect;
		std::string BaseRectText;
		std::optional<cui::core::Size> BaseSize;
		std::string BaseSizeText;
		std::optional<D2D1_MATRIX_3X2_F> BaseMatrix;
		std::string BaseMatrixText;
	};

	enum class BrushLeafProbeValue : uint8_t
	{
		SolidColor,
		Opacity,
		StartPointX,
		EndPointY,
		CenterX,
		GradientOriginY,
		RadiusX,
		RadiusY,
		GradientStopColor,
		GradientStopOffset,
	};

	enum class BrushLeafProbeGraph : uint8_t
	{
		Solid,
		Linear,
		Radial,
	};

	struct BrushLeafProbeSpec final
	{
		std::string_view Probe;
		std::string_view TargetKind;
		std::string_view FixturePropertyPath;
		std::wstring_view CanonicalPropertyPath;
		std::wstring_view RootProperty;
		CompiledStoryboardObjectPathMember Member;
		uint8_t ExpectedBrushKind;
		uint32_t StopIndex;
		BrushLeafProbeValue Value;
		BrushLeafProbeGraph Graph;
		bool BorderTarget;
	};

	constexpr uint8_t AnyBrushKind =
		(std::numeric_limits<uint8_t>::max)();
	constexpr std::array<BrushLeafProbeSpec, 10> BrushLeafProbeCatalog{ {
		{ "brush-solid-color", "color",
			"(Panel.Background).(SolidColorBrush.Color)",
			L"(Control.Background).(SolidColorBrush.Color)", L"Background",
			CompiledStoryboardObjectPathMember::BrushSolidColor,
			static_cast<uint8_t>(cui::drawing::BrushKind::Solid), 0u,
			BrushLeafProbeValue::SolidColor, BrushLeafProbeGraph::Solid, false },
		{ "brush-opacity", "double",
			"(Border.BorderBrush).(Brush.Opacity)",
			L"(Control.BorderBrush).(Brush.Opacity)", L"BorderBrush",
			CompiledStoryboardObjectPathMember::BrushOpacity,
			AnyBrushKind, 0u, BrushLeafProbeValue::Opacity,
			BrushLeafProbeGraph::Solid, true },
		{ "linear-gradient-start-point-x", "point",
			"(Panel.Background).(LinearGradientBrush.StartPoint)",
			L"(Control.Background).(LinearGradientBrush.StartPoint)", L"Background",
			CompiledStoryboardObjectPathMember::BrushStartPoint,
			static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient), 0u,
			BrushLeafProbeValue::StartPointX, BrushLeafProbeGraph::Linear, false },
		{ "linear-gradient-end-point-y", "point",
			"(Border.Background).(LinearGradientBrush.EndPoint)",
			L"(Control.Background).(LinearGradientBrush.EndPoint)", L"Background",
			CompiledStoryboardObjectPathMember::BrushEndPoint,
			static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient), 0u,
			BrushLeafProbeValue::EndPointY, BrushLeafProbeGraph::Linear, true },
		{ "radial-gradient-center-x", "point",
			"(Panel.Background).(RadialGradientBrush.Center)",
			L"(Control.Background).(RadialGradientBrush.Center)", L"Background",
			CompiledStoryboardObjectPathMember::BrushCenter,
			static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u,
			BrushLeafProbeValue::CenterX, BrushLeafProbeGraph::Radial, false },
		{ "radial-gradient-origin-y", "point",
			"(Border.Background).(RadialGradientBrush.GradientOrigin)",
			L"(Control.Background).(RadialGradientBrush.GradientOrigin)", L"Background",
			CompiledStoryboardObjectPathMember::BrushGradientOrigin,
			static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u,
			BrushLeafProbeValue::GradientOriginY, BrushLeafProbeGraph::Radial, true },
		{ "radial-gradient-radius-x", "double",
			"(Panel.Background).(RadialGradientBrush.RadiusX)",
			L"(Control.Background).(RadialGradientBrush.RadiusX)", L"Background",
			CompiledStoryboardObjectPathMember::BrushRadiusX,
			static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u,
			BrushLeafProbeValue::RadiusX, BrushLeafProbeGraph::Radial, false },
		{ "radial-gradient-radius-y", "double",
			"(Border.BorderBrush).(RadialGradientBrush.RadiusY)",
			L"(Control.BorderBrush).(RadialGradientBrush.RadiusY)", L"BorderBrush",
			CompiledStoryboardObjectPathMember::BrushRadiusY,
			static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u,
			BrushLeafProbeValue::RadiusY, BrushLeafProbeGraph::Radial, true },
		{ "gradient-stop-color", "color",
			"(Panel.Background).(GradientBrush.GradientStops)[0].(GradientStop.Color)",
			L"(Control.Background).(GradientBrush.GradientStops)[0].(GradientStop.Color)",
			L"Background", CompiledStoryboardObjectPathMember::BrushGradientStopColor,
			AnyBrushKind, 0u, BrushLeafProbeValue::GradientStopColor,
			BrushLeafProbeGraph::Linear, false },
		{ "gradient-stop-offset", "double",
			"(Border.Background).(RadialGradientBrush.GradientStops)[1].(GradientStop.Offset)",
			L"(Control.Background).(GradientBrush.GradientStops)[1].(GradientStop.Offset)",
			L"Background", CompiledStoryboardObjectPathMember::BrushGradientStopOffset,
			AnyBrushKind, 1u, BrushLeafProbeValue::GradientStopOffset,
			BrushLeafProbeGraph::Radial, true },
	} };

	const BrushLeafProbeSpec* FindBrushLeafProbe(
		std::string_view probe) noexcept
	{
		const auto found = std::find_if(
			BrushLeafProbeCatalog.begin(), BrushLeafProbeCatalog.end(),
			[&](const auto& item) { return item.Probe == probe; });
		return found == BrushLeafProbeCatalog.end() ? nullptr : &*found;
	}

	bool BrushLeafUsesColor(const BrushLeafProbeSpec& spec) noexcept
	{
		return spec.Value == BrushLeafProbeValue::SolidColor
			|| spec.Value == BrushLeafProbeValue::GradientStopColor;
	}

	bool BrushLeafUsesPoint(const BrushLeafProbeSpec& spec) noexcept
	{
		return spec.Value == BrushLeafProbeValue::StartPointX
			|| spec.Value == BrushLeafProbeValue::EndPointY
			|| spec.Value == BrushLeafProbeValue::CenterX
			|| spec.Value == BrushLeafProbeValue::GradientOriginY;
	}

	double ReadBrushLeafProbe(
		const cui::drawing::Brush& brush,
		const BrushLeafProbeSpec& spec,
		std::optional<D2D1_COLOR_F>* color = nullptr)
	{
		if (spec.ExpectedBrushKind != AnyBrushKind
			&& spec.ExpectedBrushKind != static_cast<uint8_t>(brush.Kind))
			throw std::runtime_error(
				"CUI Brush leaf probe has an unexpected Brush kind.");
		switch (spec.Value)
		{
		case BrushLeafProbeValue::SolidColor:
			if (color) *color = brush.Color;
			return static_cast<double>(brush.Color.r);
		case BrushLeafProbeValue::Opacity:
			return static_cast<double>(brush.Opacity);
		case BrushLeafProbeValue::StartPointX:
			return static_cast<double>(brush.StartPoint.x);
		case BrushLeafProbeValue::EndPointY:
			return static_cast<double>(brush.EndPoint.y);
		case BrushLeafProbeValue::CenterX:
			return static_cast<double>(brush.Center.x);
		case BrushLeafProbeValue::GradientOriginY:
			return static_cast<double>(brush.GradientOrigin.y);
		case BrushLeafProbeValue::RadiusX:
			return static_cast<double>(brush.RadiusX);
		case BrushLeafProbeValue::RadiusY:
			return static_cast<double>(brush.RadiusY);
		case BrushLeafProbeValue::GradientStopColor:
			if (spec.StopIndex >= brush.GradientStops.size())
				throw std::runtime_error(
					"CUI GradientStop Color probe index is out of range.");
			if (color) *color = brush.GradientStops[spec.StopIndex].Color;
			return static_cast<double>(
				brush.GradientStops[spec.StopIndex].Color.r);
		case BrushLeafProbeValue::GradientStopOffset:
			if (spec.StopIndex >= brush.GradientStops.size())
				throw std::runtime_error(
					"CUI GradientStop Offset probe index is out of range.");
			return static_cast<double>(
				brush.GradientStops[spec.StopIndex].Offset);
		default:
			throw std::runtime_error("CUI Brush leaf probe is invalid.");
		}
	}

	enum class TransformLeafProbeRoot : uint8_t
	{
		RenderTransform,
		GeometryTransformDirect,
		GeometryTransformChild10,
		BrushTransform,
		BrushRelativeTransform,
	};

	struct TransformLeafProbeSpec final
	{
		std::string_view Probe;
		std::string_view TargetKind;
		std::string_view FixturePropertyPath;
		std::wstring_view CanonicalPropertyPath;
		TransformLeafProbeRoot Root;
		CompiledStoryboardObjectPathMember Member;
		cui::drawing::TransformKind TransformKind;
		uint8_t ExpectedOuterKind;
		uint32_t OperationIndex;
		bool Direct;
		bool BorderTarget;
	};

	constexpr uint8_t AnyObjectKind =
		(std::numeric_limits<uint8_t>::max)();
	constexpr std::array<TransformLeafProbeSpec, 12>
		TransformLeafProbeCatalog{ {
		{ "render-transform-translate-x-direct", "double",
			"(UIElement.RenderTransform).(TranslateTransform.X)",
			L"(Control.RenderTransform).(TranslateTransform.X)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformX,
			cui::drawing::TransformKind::Translate, AnyObjectKind, 0u, true, false },
		{ "render-transform-translate-y-grouped", "double",
			"(Control.RenderTransform).(TransformGroup.Children)[0].(TranslateTransform.Y)",
			L"(Control.RenderTransform).(TransformGroup.Children)[0].(TranslateTransform.Y)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformY,
			cui::drawing::TransformKind::Translate, AnyObjectKind, 0u, false, false },
		{ "render-transform-scale-y-grouped", "double",
			"(UIElement.RenderTransform).(TransformGroup.Children)[1].(ScaleTransform.ScaleY)",
			L"(Control.RenderTransform).(TransformGroup.Children)[1].(ScaleTransform.ScaleY)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformScaleY,
			cui::drawing::TransformKind::Scale, AnyObjectKind, 1u, false, false },
		{ "render-transform-scale-center-x-grouped", "double",
			"(Control.RenderTransform).(TransformGroup.Children)[1].(ScaleTransform.CenterX)",
			L"(Control.RenderTransform).(TransformGroup.Children)[1].(ScaleTransform.CenterX)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformCenterX,
			cui::drawing::TransformKind::Scale, AnyObjectKind, 1u, false, false },
		{ "render-transform-scale-center-y-grouped", "double",
			"(UIElement.RenderTransform).(TransformGroup.Children)[1].(ScaleTransform.CenterY)",
			L"(Control.RenderTransform).(TransformGroup.Children)[1].(ScaleTransform.CenterY)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformCenterY,
			cui::drawing::TransformKind::Scale, AnyObjectKind, 1u, false, false },
		{ "render-transform-rotate-center-x-grouped", "double",
			"(Control.RenderTransform).(TransformGroup.Children)[2].(RotateTransform.CenterX)",
			L"(Control.RenderTransform).(TransformGroup.Children)[2].(RotateTransform.CenterX)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformCenterX,
			cui::drawing::TransformKind::Rotate, AnyObjectKind, 2u, false, false },
		{ "geometry-transform-rotate-center-y-recursive", "double",
			"(UIElement.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(Geometry.Transform).(TransformGroup.Children)[1].(RotateTransform.CenterY)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(Geometry.Transform).(TransformGroup.Children)[1].(RotateTransform.CenterY)",
			TransformLeafProbeRoot::GeometryTransformChild10,
			CompiledStoryboardObjectPathMember::TransformCenterY,
			cui::drawing::TransformKind::Rotate,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle),
			1u, false, false },
		{ "brush-transform-skew-angle-x-grouped", "double",
			"(Panel.Background).(Brush.Transform).(TransformGroup.Children)[0].(SkewTransform.AngleX)",
			L"(Control.Background).(Brush.Transform).(TransformGroup.Children)[0].(SkewTransform.AngleX)",
			TransformLeafProbeRoot::BrushTransform,
			CompiledStoryboardObjectPathMember::TransformAngleX,
			cui::drawing::TransformKind::Skew,
			static_cast<uint8_t>(cui::drawing::BrushKind::Solid), 0u, false, false },
		{ "brush-relative-skew-angle-y-grouped", "double",
			"(Border.Background).(Brush.RelativeTransform).(TransformGroup.Children)[1].(SkewTransform.AngleY)",
			L"(Control.Background).(Brush.RelativeTransform).(TransformGroup.Children)[1].(SkewTransform.AngleY)",
			TransformLeafProbeRoot::BrushRelativeTransform,
			CompiledStoryboardObjectPathMember::TransformAngleY,
			cui::drawing::TransformKind::Skew,
			static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient),
			1u, false, true },
		{ "render-transform-skew-center-x-grouped", "double",
			"(Control.RenderTransform).(TransformGroup.Children)[3].(SkewTransform.CenterX)",
			L"(Control.RenderTransform).(TransformGroup.Children)[3].(SkewTransform.CenterX)",
			TransformLeafProbeRoot::RenderTransform,
			CompiledStoryboardObjectPathMember::TransformCenterX,
			cui::drawing::TransformKind::Skew, AnyObjectKind, 3u, false, false },
		{ "geometry-transform-skew-center-y-direct", "double",
			"(Control.Clip).(Geometry.Transform).(SkewTransform.CenterY)",
			L"(Control.Clip).(Geometry.Transform).(SkewTransform.CenterY)",
			TransformLeafProbeRoot::GeometryTransformDirect,
			CompiledStoryboardObjectPathMember::TransformCenterY,
			cui::drawing::TransformKind::Skew,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle),
			0u, true, false },
		{ "brush-transform-matrix-grouped", "matrix",
			"(Panel.Background).(Brush.Transform).(TransformGroup.Children)[1].(MatrixTransform.Matrix)",
			L"(Control.Background).(Brush.Transform).(TransformGroup.Children)[1].(MatrixTransform.Matrix)",
			TransformLeafProbeRoot::BrushTransform,
			CompiledStoryboardObjectPathMember::TransformMatrix,
			cui::drawing::TransformKind::Matrix,
			static_cast<uint8_t>(cui::drawing::BrushKind::Solid), 1u, false, false },
	} };

	const TransformLeafProbeSpec* FindTransformLeafProbe(
		std::string_view probe) noexcept
	{
		const auto found = std::find_if(
			TransformLeafProbeCatalog.begin(), TransformLeafProbeCatalog.end(),
			[&](const auto& item) { return item.Probe == probe; });
		return found == TransformLeafProbeCatalog.end() ? nullptr : &*found;
	}

	std::vector<uint32_t> TransformLeafChildIndices(
		const TransformLeafProbeSpec& spec)
	{
		return spec.Root == TransformLeafProbeRoot::GeometryTransformChild10
			? std::vector<uint32_t>{ 1u, 0u }
			: std::vector<uint32_t>{};
	}

	double ReadTransformLeafProbe(
		const cui::drawing::Transform& transform,
		const TransformLeafProbeSpec& spec,
		std::optional<D2D1_MATRIX_3X2_F>* matrix = nullptr)
	{
		if (spec.OperationIndex >= transform.Operations.size())
			throw std::runtime_error(
				"CUI Transform leaf operation index is out of range.");
		const auto& operation = transform.Operations[spec.OperationIndex];
		if (operation.Kind != spec.TransformKind)
			throw std::runtime_error(
				"CUI Transform leaf operation kind does not match.");
		switch (spec.Member)
		{
		case CompiledStoryboardObjectPathMember::TransformX:
			return operation.X;
		case CompiledStoryboardObjectPathMember::TransformY:
			return operation.Y;
		case CompiledStoryboardObjectPathMember::TransformScaleX:
			return operation.ScaleX;
		case CompiledStoryboardObjectPathMember::TransformScaleY:
			return operation.ScaleY;
		case CompiledStoryboardObjectPathMember::TransformAngle:
			return operation.Angle;
		case CompiledStoryboardObjectPathMember::TransformAngleX:
			return operation.AngleX;
		case CompiledStoryboardObjectPathMember::TransformAngleY:
			return operation.AngleY;
		case CompiledStoryboardObjectPathMember::TransformCenterX:
			return operation.CenterX;
		case CompiledStoryboardObjectPathMember::TransformCenterY:
			return operation.CenterY;
		case CompiledStoryboardObjectPathMember::TransformMatrix:
			if (matrix) *matrix = operation.Matrix;
			return operation.Matrix._11;
		default:
			throw std::runtime_error("CUI Transform leaf member is invalid.");
		}
	}

	enum class GeometryLeafProbeGraph : uint8_t
	{
		RectangleDirect,
		RectangleChild0,
		EllipseDirect,
		EllipseChild1,
		EllipseChild10,
		PathDirect,
		PathChild1,
		PathChild10,
		GroupDirect,
	};

	enum class GeometryLeafProbeValue : uint8_t
	{
		RectangleRect,
		RectangleRadiusX,
		RectangleRadiusY,
		EllipseCenterX,
		EllipseRadiusX,
		EllipseRadiusY,
		FillRule,
		FigureStartPointX,
		FigureIsClosed,
		FigureIsFilled,
		LinePointY,
		BezierPoint1X,
		BezierPoint2Y,
		BezierPoint3X,
		QuadraticPoint1Y,
		QuadraticPoint2X,
		ArcPointY,
		ArcSize,
		ArcRotationAngle,
		ArcIsLargeArc,
		ArcSweepDirection,
	};

	struct GeometryLeafProbeSpec final
	{
		std::string_view Probe;
		std::string_view TargetKind;
		std::string_view FixturePropertyPath;
		std::wstring_view CanonicalPropertyPath;
		GeometryLeafProbeGraph Graph;
		GeometryLeafProbeValue Value;
		CompiledStoryboardObjectPathKind PathKind;
		CompiledStoryboardObjectPathMember Member;
		uint8_t ExpectedObjectKind;
		uint32_t FigureIndex;
		uint32_t SegmentIndex;
	};

	constexpr std::array<GeometryLeafProbeSpec, 22> GeometryLeafProbeCatalog{ {
		{ "rectangle-geometry-rect", "rect",
			"(UIElement.Clip).(RectangleGeometry.Rect)",
			L"(Control.Clip).(RectangleGeometry.Rect)",
			GeometryLeafProbeGraph::RectangleDirect,
			GeometryLeafProbeValue::RectangleRect,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryRect,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle), 0u, 0u },
		{ "rectangle-geometry-radius-x", "double",
			"(Control.Clip).(RectangleGeometry.RadiusX)",
			L"(Control.Clip).(RectangleGeometry.RadiusX)",
			GeometryLeafProbeGraph::RectangleDirect,
			GeometryLeafProbeValue::RectangleRadiusX,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryRadiusX,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle), 0u, 0u },
		{ "rectangle-geometry-radius-y-child", "double",
			"(UIElement.Clip).(GeometryGroup.Children)[0].(RectangleGeometry.RadiusY)",
			L"(Control.Clip).(GeometryGroup.Children)[0].(RectangleGeometry.RadiusY)",
			GeometryLeafProbeGraph::RectangleChild0,
			GeometryLeafProbeValue::RectangleRadiusY,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryRadiusY,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle), 0u, 0u },
		{ "ellipse-geometry-center-child", "point",
			"(Control.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(EllipseGeometry.Center)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(EllipseGeometry.Center)",
			GeometryLeafProbeGraph::EllipseChild10,
			GeometryLeafProbeValue::EllipseCenterX,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryCenter,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse), 0u, 0u },
		{ "ellipse-geometry-radius-x", "double",
			"(UIElement.Clip).(EllipseGeometry.RadiusX)",
			L"(Control.Clip).(EllipseGeometry.RadiusX)",
			GeometryLeafProbeGraph::EllipseDirect,
			GeometryLeafProbeValue::EllipseRadiusX,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryRadiusX,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse), 0u, 0u },
		{ "ellipse-geometry-radius-y-child", "double",
			"(Control.Clip).(GeometryGroup.Children)[1].(EllipseGeometry.RadiusY)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(EllipseGeometry.RadiusY)",
			GeometryLeafProbeGraph::EllipseChild1,
			GeometryLeafProbeValue::EllipseRadiusY,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryRadiusY,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse), 0u, 0u },
		{ "path-geometry-fill-rule", "string",
			"(UIElement.Clip).(PathGeometry.FillRule)",
			L"(Control.Clip).(PathGeometry.FillRule)",
			GeometryLeafProbeGraph::PathDirect,
			GeometryLeafProbeValue::FillRule,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryFillRule,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Path), 0u, 0u },
		{ "geometry-group-fill-rule", "string",
			"(Control.Clip).(GeometryGroup.FillRule)",
			L"(Control.Clip).(GeometryGroup.FillRule)",
			GeometryLeafProbeGraph::GroupDirect,
			GeometryLeafProbeValue::FillRule,
			CompiledStoryboardObjectPathKind::Geometry,
			CompiledStoryboardObjectPathMember::GeometryFillRule,
			static_cast<uint8_t>(cui::drawing::GeometryKind::Group), 0u, 0u },
		{ "path-figure-start-point-x", "point",
			"(UIElement.Clip).(PathGeometry.Figures)[0].(PathFigure.StartPoint)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.StartPoint)",
			GeometryLeafProbeGraph::PathDirect,
			GeometryLeafProbeValue::FigureStartPointX,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathFigureStartPoint, 0u, 0u, 0u },
		{ "path-figure-is-closed", "boolean",
			"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.IsClosed)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.IsClosed)",
			GeometryLeafProbeGraph::PathDirect,
			GeometryLeafProbeValue::FigureIsClosed,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathFigureIsClosed, 0u, 0u, 0u },
		{ "path-figure-is-filled-child", "boolean",
			"(UIElement.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.IsFilled)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.IsFilled)",
			GeometryLeafProbeGraph::PathChild1,
			GeometryLeafProbeValue::FigureIsFilled,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathFigureIsFilled, 0u, 0u, 0u },
		{ "line-segment-point-y", "point",
			"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[0].(LineSegment.Point)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[0].(LineSegment.Point)",
			GeometryLeafProbeGraph::PathDirect, GeometryLeafProbeValue::LinePointY,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Line), 0u, 0u },
		{ "bezier-segment-point1-x", "point",
			"(UIElement.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[1].(BezierSegment.Point1)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[1].(BezierSegment.Point1)",
			GeometryLeafProbeGraph::PathDirect, GeometryLeafProbeValue::BezierPoint1X,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint1,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier), 0u, 1u },
		{ "bezier-segment-point2-y-child", "point",
			"(Control.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.Segments)[1].(BezierSegment.Point2)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.Segments)[1].(BezierSegment.Point2)",
			GeometryLeafProbeGraph::PathChild1, GeometryLeafProbeValue::BezierPoint2Y,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint2,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier), 0u, 1u },
		{ "bezier-segment-point3-x-recursive", "point",
			"(UIElement.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(PathGeometry.Figures)[0].(PathFigure.Segments)[1].(BezierSegment.Point3)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(PathGeometry.Figures)[0].(PathFigure.Segments)[1].(BezierSegment.Point3)",
			GeometryLeafProbeGraph::PathChild10, GeometryLeafProbeValue::BezierPoint3X,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint3,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier), 0u, 1u },
		{ "quadratic-segment-point1-y", "point",
			"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[2].(QuadraticBezierSegment.Point1)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[2].(QuadraticBezierSegment.Point1)",
			GeometryLeafProbeGraph::PathDirect, GeometryLeafProbeValue::QuadraticPoint1Y,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint1,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::QuadraticBezier), 0u, 2u },
		{ "quadratic-segment-point2-x-child", "point",
			"(UIElement.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.Segments)[2].(QuadraticBezierSegment.Point2)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.Segments)[2].(QuadraticBezierSegment.Point2)",
			GeometryLeafProbeGraph::PathChild1, GeometryLeafProbeValue::QuadraticPoint2X,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint2,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::QuadraticBezier), 0u, 2u },
		{ "arc-segment-point-y", "point",
			"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.Point)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.Point)",
			GeometryLeafProbeGraph::PathDirect, GeometryLeafProbeValue::ArcPointY,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathSegmentPoint,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, 3u },
		{ "arc-segment-size-recursive", "size",
			"(UIElement.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.Size)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(GeometryGroup.Children)[0].(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.Size)",
			GeometryLeafProbeGraph::PathChild10, GeometryLeafProbeValue::ArcSize,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathArcSize,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, 3u },
		{ "arc-segment-rotation-angle", "double",
			"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.RotationAngle)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.RotationAngle)",
			GeometryLeafProbeGraph::PathDirect, GeometryLeafProbeValue::ArcRotationAngle,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathArcRotationAngle,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, 3u },
		{ "arc-segment-is-large-child", "boolean",
			"(UIElement.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.IsLargeArc)",
			L"(Control.Clip).(GeometryGroup.Children)[1].(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.IsLargeArc)",
			GeometryLeafProbeGraph::PathChild1, GeometryLeafProbeValue::ArcIsLargeArc,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathArcIsLargeArc,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, 3u },
		{ "arc-segment-sweep-direction", "string",
			"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.SweepDirection)",
			L"(Control.Clip).(PathGeometry.Figures)[0].(PathFigure.Segments)[3].(ArcSegment.SweepDirection)",
			GeometryLeafProbeGraph::PathDirect, GeometryLeafProbeValue::ArcSweepDirection,
			CompiledStoryboardObjectPathKind::PathGeometry,
			CompiledStoryboardObjectPathMember::PathArcSweepDirection,
			static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, 3u },
	} };

	const GeometryLeafProbeSpec* FindGeometryLeafProbe(
		std::string_view probe) noexcept
	{
		const auto found = std::find_if(
			GeometryLeafProbeCatalog.begin(), GeometryLeafProbeCatalog.end(),
			[&](const auto& item) { return item.Probe == probe; });
		return found == GeometryLeafProbeCatalog.end() ? nullptr : &*found;
	}

	std::vector<uint32_t> GeometryLeafChildIndices(
		const GeometryLeafProbeSpec& spec)
	{
		switch (spec.Graph)
		{
		case GeometryLeafProbeGraph::RectangleChild0:
			return { 0u };
		case GeometryLeafProbeGraph::EllipseChild1:
		case GeometryLeafProbeGraph::PathChild1:
			return { 1u };
		case GeometryLeafProbeGraph::EllipseChild10:
		case GeometryLeafProbeGraph::PathChild10:
			return { 1u, 0u };
		default:
			return {};
		}
	}

	bool GeometryLeafUsesObjectEnum(
		const GeometryLeafProbeSpec& spec) noexcept
	{
		return spec.Value == GeometryLeafProbeValue::FillRule
			|| spec.Value == GeometryLeafProbeValue::ArcSweepDirection;
	}

	double ReadGeometryLeafProbe(
		const cui::drawing::Geometry& root,
		const GeometryLeafProbeSpec& spec,
		std::optional<cui::core::Rect>* rect = nullptr,
		std::optional<cui::core::Size>* size = nullptr,
		std::optional<bool>* boolean = nullptr,
		std::optional<std::string>* text = nullptr)
	{
		const auto childIndices = GeometryLeafChildIndices(spec);
		const auto* geometry = &root;
		for (const auto index : childIndices)
		{
			if (geometry->Kind != cui::drawing::GeometryKind::Group
				|| index >= geometry->Children.size())
				throw std::runtime_error(
					"CUI Geometry leaf child index is out of range.");
			geometry = &geometry->Children[index];
		}
		if (spec.PathKind == CompiledStoryboardObjectPathKind::Geometry)
		{
			if (spec.ExpectedObjectKind != static_cast<uint8_t>(geometry->Kind))
				throw std::runtime_error(
					"CUI Geometry leaf has an unexpected Geometry kind.");
			switch (spec.Value)
			{
			case GeometryLeafProbeValue::RectangleRect:
			{
				const cui::core::Rect value{
					geometry->Rect.left, geometry->Rect.top,
					geometry->Rect.right - geometry->Rect.left,
					geometry->Rect.bottom - geometry->Rect.top };
				if (rect) *rect = value;
				return static_cast<double>(value.x);
			}
			case GeometryLeafProbeValue::RectangleRadiusX:
			case GeometryLeafProbeValue::EllipseRadiusX:
				return static_cast<double>(geometry->RadiusX);
			case GeometryLeafProbeValue::RectangleRadiusY:
			case GeometryLeafProbeValue::EllipseRadiusY:
				return static_cast<double>(geometry->RadiusY);
			case GeometryLeafProbeValue::EllipseCenterX:
				return static_cast<double>(geometry->Center.x);
			case GeometryLeafProbeValue::FillRule:
			{
				const std::string value = geometry->FillRule
					== cui::drawing::GeometryFillRule::EvenOdd
					? "EvenOdd" : "Nonzero";
				if (text) *text = value;
				return static_cast<double>(value.size());
			}
			default:
				throw std::runtime_error("CUI Geometry public leaf is invalid.");
			}
		}
		if (geometry->Kind != cui::drawing::GeometryKind::Path
			|| spec.FigureIndex >= geometry->Figures.size())
			throw std::runtime_error("CUI PathGeometry leaf graph is invalid.");
		const auto& figure = geometry->Figures[spec.FigureIndex];
		switch (spec.Value)
		{
		case GeometryLeafProbeValue::FigureStartPointX:
			return static_cast<double>(figure.StartPoint.x);
		case GeometryLeafProbeValue::FigureIsClosed:
			if (boolean) *boolean = figure.IsClosed;
			return figure.IsClosed ? 1.0 : 0.0;
		case GeometryLeafProbeValue::FigureIsFilled:
			if (boolean) *boolean = figure.IsFilled;
			return figure.IsFilled ? 1.0 : 0.0;
		default:
			break;
		}
		if (spec.SegmentIndex >= figure.Segments.size())
			throw std::runtime_error("CUI PathSegment leaf index is out of range.");
		const auto& segment = figure.Segments[spec.SegmentIndex];
		if (spec.ExpectedObjectKind != static_cast<uint8_t>(segment.Kind))
			throw std::runtime_error(
				"CUI PathSegment leaf has an unexpected segment kind.");
		switch (spec.Value)
		{
		case GeometryLeafProbeValue::LinePointY:
		case GeometryLeafProbeValue::ArcPointY:
			return static_cast<double>(segment.Point.y);
		case GeometryLeafProbeValue::BezierPoint1X:
			return static_cast<double>(segment.Point1.x);
		case GeometryLeafProbeValue::BezierPoint2Y:
			return static_cast<double>(segment.Point2.y);
		case GeometryLeafProbeValue::BezierPoint3X:
			return static_cast<double>(segment.Point3.x);
		case GeometryLeafProbeValue::QuadraticPoint1Y:
			return static_cast<double>(segment.Point1.y);
		case GeometryLeafProbeValue::QuadraticPoint2X:
			return static_cast<double>(segment.Point2.x);
		case GeometryLeafProbeValue::ArcSize:
		{
			const cui::core::Size value{
				segment.Size.width, segment.Size.height };
			if (size) *size = value;
			return static_cast<double>(value.width);
		}
		case GeometryLeafProbeValue::ArcRotationAngle:
			return static_cast<double>(segment.RotationAngle);
		case GeometryLeafProbeValue::ArcIsLargeArc:
			if (boolean) *boolean = segment.IsLargeArc;
			return segment.IsLargeArc ? 1.0 : 0.0;
		case GeometryLeafProbeValue::ArcSweepDirection:
		{
			const std::string value = segment.Sweep
				== cui::drawing::SweepDirection::Clockwise
				? "Clockwise" : "Counterclockwise";
			if (text) *text = value;
			return static_cast<double>(value.size());
		}
		default:
			throw std::runtime_error("CUI PathGeometry leaf is invalid.");
		}
	}

	struct AnimationFixture final
	{
		std::string Id;
		std::string Description;
		double Tolerance = 0.0;
		bool CompareValue = true;
		bool CompareIsAnimated = false;
		bool CompareEvents = false;
		std::string Oracle = "aligned-value";
		std::vector<std::string> Capabilities;
		std::string WpfSupport;
		std::string CuiSupport;
		AnimationTarget Target;
		bool HasStoryboardTiming = false;
		DeclarativeStoryboardTimingDefinition StoryboardTiming;
		std::string TimelineXaml;
		std::optional<std::string> ReplacementTimelineXaml;
		std::optional<std::string> StoryboardResourceKey;
		std::optional<std::string> StoryboardResourceScope;
		std::vector<AnimationOperation> Operations;
		std::vector<AnimationSampleRequest> Samples;
	};

	struct AnimationCorpus final
	{
		int SchemaVersion = 0;
		std::string CorpusSha256;
		std::vector<AnimationFixture> Fixtures;
	};

	struct SampleResult final
	{
		unsigned long long AtMilliseconds = 0;
		std::string Label;
		double Value = 0.0;
		bool IsAnimated = false;
		std::optional<DeclarativeClockObservation> Clock;
		std::vector<std::string> Events;
		/** Internal full-value gates; JSON emits the complete typed payloads. */
		std::optional<Thickness> ThicknessValue;
		std::optional<D2D1_COLOR_F> ColorValue;
		std::optional<cui::core::Vector> VectorValue;
		std::optional<cui::core::Rect> RectValue;
		std::optional<cui::core::Size> SizeValue;
		std::optional<D2D1_MATRIX_3X2_F> MatrixValue;
		std::optional<int> Int32Value;
		std::optional<long long> Int64Value;
		std::optional<float> SingleValue;
		std::optional<bool> BooleanValue;
		std::optional<std::string> StringValue;
	};

	struct FixtureResult final
	{
		std::string Id;
		std::string Status;
		std::optional<std::string> Error;
		std::vector<SampleResult> Samples;
	};

	struct AnimationResultDocument final
	{
		std::string CorpusSha256;
		std::vector<FixtureResult> Fixtures;
	};

	struct CorpusFile final
	{
		std::string RawBytes;
		std::string Xml;
	};

	struct CommandLineOptions final
	{
		std::filesystem::path FixturesPath;
		std::optional<std::filesystem::path> OutputPath;
		std::optional<std::string> ExactFixtureId;
	};

	struct CommandLineParseResult final
	{
		bool Requested = false;
		std::optional<CommandLineOptions> Options;
		std::string Error;
	};

	const DependencyProperty*& AnimationDoubleProbeValuePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DependencyProperty*& AnimationDoubleProbePhasePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DependencyProperty*& AnimationColorProbeValuePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DependencyProperty*& AnimationVectorProbeValuePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DependencyProperty*& AnimationRectProbeValuePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DependencyProperty*& AnimationSizeProbeValuePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DependencyProperty*& AnimationMatrixProbeValuePropertyStorage()
	{
		static const DependencyProperty* property = nullptr;
		return property;
	}

	const DeclarativeEventDefinition& FixturePrimaryEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	const DeclarativeEventDefinition& FixtureReplacementEvent()
	{
		static const DeclarativeEventDefinition value{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		return value;
	}

	class AnimationDoubleProbeControl final : public Control
	{
	public:
		UIClass Type() override { return UIClass::UI_CUSTOM; }

		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<AnimationDoubleProbeControl, double>
					options;
				options.DefaultValue = 0.0;
				AnimationDoubleProbeValuePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationDoubleProbeControl, double>(
							L"Value", std::move(options));
				DependencyPropertyOptions<AnimationDoubleProbeControl, int>
					phaseOptions;
				phaseOptions.DefaultValue = 0;
				AnimationDoubleProbePhasePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationDoubleProbeControl, int>(
							L"Phase", std::move(phaseOptions));
				return AnimationDoubleProbeValuePropertyStorage() != nullptr
					&& AnimationDoubleProbePhasePropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *AnimationDoubleProbeValuePropertyStorage();
		}

		static const DependencyProperty& PhaseProperty()
		{
			RegisterDependencyProperties();
			return *AnimationDoubleProbePhasePropertyStorage();
		}

		double Value() const
		{
			return GetDependencyPropertyValue<double>(ValueProperty());
		}

		bool SetValue(double value)
		{
			return SetDependencyPropertyValue(ValueProperty(), value);
		}

		bool SetPhase(int value)
		{
			return SetDependencyPropertyValue(PhaseProperty(), value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	template<typename TValue>
	class AnimationScalarProbeControl final : public Control
	{
		static const DependencyProperty*& PropertyStorage()
		{
			static const DependencyProperty* property = nullptr;
			return property;
		}

	public:
		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<AnimationScalarProbeControl, TValue> options;
				options.DefaultValue = TValue{};
				PropertyStorage() = DependencyPropertyRegistry::Register<
					AnimationScalarProbeControl, TValue>(L"Value", std::move(options));
				return PropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *PropertyStorage();
		}

		TValue Value() const
		{
			return GetDependencyPropertyValue<TValue>(ValueProperty());
		}

		bool SetValue(TValue value)
		{
			return SetDependencyPropertyValue(ValueProperty(), std::move(value));
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	using AnimationInt32ProbeControl = AnimationScalarProbeControl<int>;
	using AnimationInt64ProbeControl = AnimationScalarProbeControl<long long>;
	using AnimationSingleProbeControl = AnimationScalarProbeControl<float>;
	using AnimationBooleanProbeControl = AnimationScalarProbeControl<bool>;
	using AnimationStringProbeControl = AnimationScalarProbeControl<std::wstring>;

	class AnimationColorProbeControl final : public Control
	{
	public:
		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<
					AnimationColorProbeControl, D2D1_COLOR_F> options;
				options.DefaultValue = D2D1_COLOR_F{};
				options.Equals = [](const D2D1_COLOR_F& left,
					const D2D1_COLOR_F& right)
				{
					return left.r == right.r && left.g == right.g
						&& left.b == right.b && left.a == right.a;
				};
				AnimationColorProbeValuePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationColorProbeControl, D2D1_COLOR_F>(
							L"Value", std::move(options));
				return AnimationColorProbeValuePropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *AnimationColorProbeValuePropertyStorage();
		}

		D2D1_COLOR_F Value() const
		{
			return GetDependencyPropertyValue<D2D1_COLOR_F>(ValueProperty());
		}

		bool SetValue(D2D1_COLOR_F value)
		{
			return SetDependencyPropertyValue(ValueProperty(), value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	class AnimationVectorProbeControl final : public Control
	{
	public:
		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<
					AnimationVectorProbeControl, cui::core::Vector> options;
				options.DefaultValue = cui::core::Vector{};
				options.Equals = [](const cui::core::Vector& left,
					const cui::core::Vector& right)
				{
					return left.x == right.x && left.y == right.y;
				};
				AnimationVectorProbeValuePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationVectorProbeControl, cui::core::Vector>(
							L"Value", std::move(options));
				return AnimationVectorProbeValuePropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *AnimationVectorProbeValuePropertyStorage();
		}

		cui::core::Vector Value() const
		{
			return GetDependencyPropertyValue<cui::core::Vector>(ValueProperty());
		}

		bool SetValue(cui::core::Vector value)
		{
			return SetDependencyPropertyValue(ValueProperty(), value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	class AnimationRectProbeControl final : public Control
	{
	public:
		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<
					AnimationRectProbeControl, cui::core::Rect> options;
				options.DefaultValue = cui::core::Rect{};
				options.Equals = [](const cui::core::Rect& left,
					const cui::core::Rect& right)
				{
					return left.x == right.x && left.y == right.y
						&& left.width == right.width && left.height == right.height;
				};
				AnimationRectProbeValuePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationRectProbeControl, cui::core::Rect>(
							L"Value", std::move(options));
				return AnimationRectProbeValuePropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *AnimationRectProbeValuePropertyStorage();
		}

		cui::core::Rect Value() const
		{
			return GetDependencyPropertyValue<cui::core::Rect>(ValueProperty());
		}

		bool SetValue(cui::core::Rect value)
		{
			return SetDependencyPropertyValue(ValueProperty(), value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	class AnimationSizeProbeControl final : public Control
	{
	public:
		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<
					AnimationSizeProbeControl, cui::core::Size> options;
				options.DefaultValue = cui::core::Size{};
				options.Equals = [](const cui::core::Size& left,
					const cui::core::Size& right)
				{
					return left.width == right.width
						&& left.height == right.height;
				};
				AnimationSizeProbeValuePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationSizeProbeControl, cui::core::Size>(
							L"Value", std::move(options));
				return AnimationSizeProbeValuePropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *AnimationSizeProbeValuePropertyStorage();
		}

		cui::core::Size Value() const
		{
			return GetDependencyPropertyValue<cui::core::Size>(ValueProperty());
		}

		bool SetValue(cui::core::Size value)
		{
			return SetDependencyPropertyValue(ValueProperty(), value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	class AnimationMatrixProbeControl final : public Control
	{
	public:
		static void RegisterDependencyProperties()
		{
			static const bool registered = []
			{
				DependencyPropertyOptions<
					AnimationMatrixProbeControl, D2D1_MATRIX_3X2_F> options;
				options.DefaultValue = D2D1::Matrix3x2F::Identity();
				options.Equals = [](const D2D1_MATRIX_3X2_F& left,
					const D2D1_MATRIX_3X2_F& right)
				{
					return left._11 == right._11 && left._12 == right._12
						&& left._21 == right._21 && left._22 == right._22
						&& left._31 == right._31 && left._32 == right._32;
				};
				AnimationMatrixProbeValuePropertyStorage() =
					DependencyPropertyRegistry::Register<
						AnimationMatrixProbeControl, D2D1_MATRIX_3X2_F>(
							L"Value", std::move(options));
				return AnimationMatrixProbeValuePropertyStorage() != nullptr;
			}();
			(void)registered;
		}

		static const DependencyProperty& ValueProperty()
		{
			RegisterDependencyProperties();
			return *AnimationMatrixProbeValuePropertyStorage();
		}

		D2D1_MATRIX_3X2_F Value() const
		{
			return GetDependencyPropertyValue<D2D1_MATRIX_3X2_F>(ValueProperty());
		}

		bool SetValue(D2D1_MATRIX_3X2_F value)
		{
			return SetDependencyPropertyValue(ValueProperty(), value);
		}

		void EnsureBindingPropertiesRegistered() override
		{
			RegisterDependencyProperties();
		}
	};

	bool IsAsciiWhitespace(unsigned char value) noexcept
	{
		return value == ' ' || value == '\t' || value == '\r'
			|| value == '\n' || value == '\f' || value == '\v';
	}

	std::string TrimAscii(std::string_view value)
	{
		std::size_t first = 0;
		while (first < value.size()
			&& IsAsciiWhitespace(static_cast<unsigned char>(value[first])))
			++first;
		std::size_t last = value.size();
		while (last > first
			&& IsAsciiWhitespace(static_cast<unsigned char>(value[last - 1])))
			--last;
		return std::string(value.substr(first, last - first));
	}

	D2D1_COLOR_F ParseArgbColor(
		std::string_view value, std::string_view field)
	{
		auto nibble = [](char character) -> int
		{
			if (character >= '0' && character <= '9') return character - '0';
			if (character >= 'a' && character <= 'f') return character - 'a' + 10;
			if (character >= 'A' && character <= 'F') return character - 'A' + 10;
			return -1;
		};
		if (value.size() != 9u || value.front() != '#')
			throw std::runtime_error("Invalid #AARRGGBB "
				+ std::string(field) + ": " + std::string(value) + ".");
		std::array<unsigned char, 4> bytes{};
		for (size_t index = 0; index < bytes.size(); ++index)
		{
			const auto high = nibble(value[1u + index * 2u]);
			const auto low = nibble(value[2u + index * 2u]);
			if (high < 0 || low < 0)
				throw std::runtime_error("Invalid #AARRGGBB "
					+ std::string(field) + ": " + std::string(value) + ".");
			bytes[index] = static_cast<unsigned char>((high << 4) | low);
		}
		return D2D1_COLOR_F{
			static_cast<float>(bytes[1]) / 255.0f,
			static_cast<float>(bytes[2]) / 255.0f,
			static_cast<float>(bytes[3]) / 255.0f,
			static_cast<float>(bytes[0]) / 255.0f };
	}

	bool EqualColor(const D2D1_COLOR_F& left, const D2D1_COLOR_F& right) noexcept
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	bool EqualOptionalColor(
		const std::optional<D2D1_COLOR_F>& left,
		const std::optional<D2D1_COLOR_F>& right) noexcept
	{
		return left.has_value() == right.has_value()
			&& (!left || EqualColor(*left, *right));
	}

	bool EqualOptionalVector(
		const std::optional<cui::core::Vector>& left,
		const std::optional<cui::core::Vector>& right) noexcept
	{
		return left.has_value() == right.has_value()
			&& (!left || (left->x == right->x && left->y == right->y));
	}

	bool EqualOptionalRect(
		const std::optional<cui::core::Rect>& left,
		const std::optional<cui::core::Rect>& right) noexcept
	{
		return left.has_value() == right.has_value()
			&& (!left || (left->x == right->x && left->y == right->y
				&& left->width == right->width
				&& left->height == right->height));
	}

	bool EqualOptionalSize(
		const std::optional<cui::core::Size>& left,
		const std::optional<cui::core::Size>& right) noexcept
	{
		return left.has_value() == right.has_value()
			&& (!left || (left->width == right->width
				&& left->height == right->height));
	}

	bool EqualOptionalMatrix(
		const std::optional<D2D1_MATRIX_3X2_F>& left,
		const std::optional<D2D1_MATRIX_3X2_F>& right) noexcept
	{
		return left.has_value() == right.has_value()
			&& (!left || (left->_11 == right->_11 && left->_12 == right->_12
				&& left->_21 == right->_21 && left->_22 == right->_22
				&& left->_31 == right->_31 && left->_32 == right->_32));
	}

	bool IsBlank(std::string_view value) noexcept
	{
		return std::all_of(value.begin(), value.end(), [](char character)
			{
				return IsAsciiWhitespace(
					static_cast<unsigned char>(character));
			});
	}

	std::string WideToUtf8(std::wstring_view value)
	{
		if (value.empty()) return {};
		const int required = ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (required <= 0)
			throw std::runtime_error("Could not encode a UTF-16 value as UTF-8.");
		std::string result(static_cast<std::size_t>(required), '\0');
		if (::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), required,
			nullptr, nullptr) != required)
			throw std::runtime_error("Could not encode a UTF-16 value as UTF-8.");
		return result;
	}

	bool IsValidUtf8(std::string_view value) noexcept
	{
		for (std::size_t index = 0; index < value.size();)
		{
			const auto lead = static_cast<unsigned char>(value[index]);
			if (lead == 0) return false;
			if (lead < 0x80)
			{
				++index;
				continue;
			}
			std::size_t continuationCount = 0;
			unsigned codePoint = 0;
			if (lead >= 0xC2 && lead <= 0xDF)
			{
				continuationCount = 1;
				codePoint = lead & 0x1Fu;
			}
			else if (lead >= 0xE0 && lead <= 0xEF)
			{
				continuationCount = 2;
				codePoint = lead & 0x0Fu;
			}
			else if (lead >= 0xF0 && lead <= 0xF4)
			{
				continuationCount = 3;
				codePoint = lead & 0x07u;
			}
			else return false;
			if (index + continuationCount >= value.size()) return false;
			for (std::size_t offset = 1; offset <= continuationCount; ++offset)
			{
				const auto continuation =
					static_cast<unsigned char>(value[index + offset]);
				if ((continuation & 0xC0u) != 0x80u) return false;
				codePoint = (codePoint << 6) | (continuation & 0x3Fu);
			}
			if ((continuationCount == 2 && codePoint < 0x800u)
				|| (continuationCount == 3 && codePoint < 0x10000u)
				|| codePoint > 0x10FFFFu
				|| (codePoint >= 0xD800u && codePoint <= 0xDFFFu))
				return false;
			index += continuationCount + 1;
		}
		return true;
	}

	std::string Sha256(std::string_view bytes)
	{
		SHA256 hash;
		hash.update(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
		hash.finalize();
		return hash.hexdigest();
	}

	CorpusFile ReadCorpusFile(const std::filesystem::path& path)
	{
		std::error_code error;
		if (!std::filesystem::is_regular_file(path, error) || error)
			throw std::runtime_error("Fixture corpus is not a readable regular file: "
				+ WideToUtf8(path.wstring()));
		const auto fileSize = std::filesystem::file_size(path, error);
		if (error)
			throw std::runtime_error("Could not inspect fixture corpus: "
				+ WideToUtf8(path.wstring()));
		if (fileSize > MaximumCorpusBytes)
			throw std::runtime_error(
				"Fixture corpus exceeds the 4 MiB safety limit.");

		std::ifstream input(path, std::ios::binary);
		if (!input)
			throw std::runtime_error("Could not open fixture corpus: "
				+ WideToUtf8(path.wstring()));
		CorpusFile result;
		result.RawBytes.assign(static_cast<std::size_t>(fileSize), '\0');
		if (!result.RawBytes.empty())
			input.read(result.RawBytes.data(),
				static_cast<std::streamsize>(result.RawBytes.size()));
		if (!input
			|| static_cast<std::size_t>(input.gcount()) != result.RawBytes.size())
			throw std::runtime_error("Could not read the complete fixture corpus.");
		result.Xml = result.RawBytes;
		if (result.Xml.starts_with("\xEF\xBB\xBF")) result.Xml.erase(0, 3);
		if (!IsValidUtf8(result.Xml))
			throw std::runtime_error("Fixture corpus is not valid UTF-8 XML.");
		return result;
	}

	std::vector<std::shared_ptr<System::Xml::XmlElement>> ChildElements(
		const System::Xml::XmlElement& parent)
	{
		std::vector<std::shared_ptr<System::Xml::XmlElement>> elements;
		for (const auto& child : parent.ChildNodes())
		{
			if (child->NodeType() == System::Xml::XmlNodeType::Element)
			{
				auto element = std::dynamic_pointer_cast<System::Xml::XmlElement>(child);
				if (!element) throw std::runtime_error("Malformed XML element node.");
				elements.push_back(std::move(element));
				continue;
			}
			if (child->NodeType() == System::Xml::XmlNodeType::Whitespace
				|| child->NodeType() == System::Xml::XmlNodeType::SignificantWhitespace
				|| child->NodeType() == System::Xml::XmlNodeType::Text)
			{
				if (IsBlank(child->Value())) continue;
			}
			throw std::runtime_error("Element " + parent.Name()
				+ " contains unsupported non-element content.");
		}
		return elements;
	}

	void RequireName(
		const System::Xml::XmlElement& element, std::string_view expected)
	{
		if (!element.NamespaceURI().empty() || element.Name() != expected)
			throw std::runtime_error("Expected element " + std::string(expected)
				+ ", found " + element.Name() + ".");
	}

	void RequireOnlyAttributes(
		const System::Xml::XmlElement& element,
		std::initializer_list<std::string_view> allowed)
	{
		for (const auto& attribute : element.Attributes())
		{
			const bool known = std::any_of(
				allowed.begin(), allowed.end(), [&](std::string_view name)
				{ return attribute->Name() == name; });
			if (!known || !attribute->NamespaceURI().empty())
				throw std::runtime_error("Element " + element.Name()
					+ " has unknown attribute " + attribute->Name() + ".");
		}
	}

	std::string RequireAttribute(
		const System::Xml::XmlElement& element, std::string_view name)
	{
		if (!element.HasAttribute(name))
			throw std::runtime_error("Element " + element.Name()
				+ " requires attribute " + std::string(name) + ".");
		const auto value = element.GetAttribute(name);
		if (IsBlank(value))
			throw std::runtime_error("Element " + element.Name()
				+ " requires a non-empty " + std::string(name) + " attribute.");
		return value;
	}

	const System::Xml::XmlElement& RequireSingle(
		const System::Xml::XmlElement& parent, std::string_view name)
	{
		const auto elements = ChildElements(parent);
		const System::Xml::XmlElement* result = nullptr;
		for (const auto& element : elements)
			if (element->Name() == name)
			{
				if (result)
					throw std::runtime_error("Element " + parent.Name()
						+ " contains duplicate " + std::string(name) + " children.");
				result = element.get();
			}
		if (!result)
			throw std::runtime_error("Element " + parent.Name()
				+ " requires exactly one " + std::string(name) + " child.");
		return *result;
	}

	void RequireNoChildren(const System::Xml::XmlElement& element)
	{
		if (!ChildElements(element).empty())
			throw std::runtime_error("Element " + element.Name()
				+ " must not contain child elements.");
	}

	long long ParseInt64(std::string_view value, std::string_view field)
	{
		long long result = 0;
		const auto parsed = std::from_chars(value.data(),
			value.data() + value.size(), result, 10);
		if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
			throw std::runtime_error("Invalid integer " + std::string(field)
				+ ": " + std::string(value) + ".");
		return result;
	}

	double ParseDouble(std::string_view value, std::string_view field)
	{
		const auto trimmed = TrimAscii(value);
		double result = 0.0;
		const auto parsed = std::from_chars(trimmed.data(),
			trimmed.data() + trimmed.size(), result, std::chars_format::general);
		if (parsed.ec != std::errc{}
			|| parsed.ptr != trimmed.data() + trimmed.size()
			|| !std::isfinite(result))
			throw std::runtime_error("Invalid finite number " + std::string(field)
				+ ": " + std::string(value) + ".");
		return result;
	}

	template<typename TValue>
	TValue ParseSignedInteger(std::string_view value, std::string_view field)
	{
		const auto trimmed = TrimAscii(value);
		TValue result{};
		const auto parsed = std::from_chars(
			trimmed.data(), trimmed.data() + trimmed.size(), result);
		if (parsed.ec != std::errc{}
			|| parsed.ptr != trimmed.data() + trimmed.size())
			throw std::runtime_error("Invalid signed integer " + std::string(field)
				+ ": " + std::string(value) + ".");
		return result;
	}

	float ParseSingle(std::string_view value, std::string_view field)
	{
		const auto parsed = ParseDouble(value, field);
		if (parsed < -(std::numeric_limits<float>::max)()
			|| parsed >(std::numeric_limits<float>::max)())
			throw std::runtime_error("Out-of-range Single " + std::string(field)
				+ ": " + std::string(value) + ".");
		const auto result = static_cast<float>(parsed);
		if (!std::isfinite(result))
			throw std::runtime_error("Invalid Single " + std::string(field) + ".");
		return result;
	}

	bool ParseBoolean(std::string_view value, std::string_view field)
	{
		auto equals = [&](std::string_view expected)
		{
			return value.size() == expected.size()
				&& std::equal(value.begin(), value.end(), expected.begin(),
					[](unsigned char left, unsigned char right)
					{ return static_cast<unsigned char>(std::tolower(left)) == right; });
		};
		if (equals("true")) return true;
		if (equals("false")) return false;
		throw std::runtime_error("Invalid Boolean " + std::string(field)
			+ ": " + std::string(value) + ".");
	}

	cui::core::Vector ParseVector(
		std::string_view value, std::string_view field)
	{
		const auto comma = value.find(',');
		if (comma == std::string_view::npos
			|| value.find(',', comma + 1u) != std::string_view::npos)
			throw std::runtime_error("Invalid x,y " + std::string(field)
				+ ": " + std::string(value) + ".");
		const cui::core::Vector result{
			static_cast<float>(ParseDouble(value.substr(0, comma), field)),
			static_cast<float>(ParseDouble(value.substr(comma + 1u), field)) };
		if (!std::isfinite(result.x) || !std::isfinite(result.y))
			throw std::runtime_error("Out-of-range x,y " + std::string(field)
				+ ": " + std::string(value) + ".");
		return result;
	}

	cui::core::Rect ParseRect(
		std::string_view value, std::string_view field)
	{
		std::array<float, 4> components{};
		size_t start = 0;
		for (size_t index = 0; index < components.size(); ++index)
		{
			const auto comma = value.find(',', start);
			if ((index + 1u < components.size() && comma == std::string_view::npos)
				|| (index + 1u == components.size()
					&& comma != std::string_view::npos))
				throw std::runtime_error("Invalid x,y,width,height "
					+ std::string(field) + ": " + std::string(value) + ".");
			const auto end = comma == std::string_view::npos
				? value.size() : comma;
			components[index] = static_cast<float>(ParseDouble(
				value.substr(start, end - start), field));
			if (!std::isfinite(components[index]))
				throw std::runtime_error("Out-of-range x,y,width,height "
					+ std::string(field) + ": " + std::string(value) + ".");
			start = end + 1u;
		}
		if (components[2] < 0.0f || components[3] < 0.0f)
			throw std::runtime_error("Negative Rect extent in "
				+ std::string(field) + ": " + std::string(value) + ".");
		return cui::core::Rect{
			components[0], components[1], components[2], components[3] };
	}

	cui::core::Size ParseSize(
		std::string_view value, std::string_view field)
	{
		const auto comma = value.find(',');
		if (comma == std::string_view::npos
			|| value.find(',', comma + 1u) != std::string_view::npos)
			throw std::runtime_error("Invalid width,height "
				+ std::string(field) + ": " + std::string(value) + ".");
		const cui::core::Size result{
			static_cast<float>(ParseDouble(value.substr(0, comma), field)),
			static_cast<float>(ParseDouble(value.substr(comma + 1u), field)) };
		if (!std::isfinite(result.width) || !std::isfinite(result.height)
			|| result.width < 0.0f || result.height < 0.0f)
			throw std::runtime_error("Invalid Size extent in "
				+ std::string(field) + ": " + std::string(value) + ".");
		return result;
	}

	D2D1_MATRIX_3X2_F ParseMatrix(
		std::string_view value, std::string_view field)
	{
		std::array<float, 6> components{};
		size_t start = 0;
		for (size_t index = 0; index < components.size(); ++index)
		{
			const auto comma = value.find(',', start);
			if ((index + 1u < components.size() && comma == std::string_view::npos)
				|| (index + 1u == components.size()
					&& comma != std::string_view::npos))
				throw std::runtime_error("Invalid Matrix six-tuple "
					+ std::string(field) + ": " + std::string(value) + ".");
			const auto end = comma == std::string_view::npos
				? value.size() : comma;
			components[index] = static_cast<float>(ParseDouble(
				value.substr(start, end - start), field));
			if (!std::isfinite(components[index]))
				throw std::runtime_error("Out-of-range Matrix "
					+ std::string(field) + ": " + std::string(value) + ".");
			start = end + 1u;
		}
		return D2D1_MATRIX_3X2_F{
			components[0], components[1], components[2],
			components[3], components[4], components[5] };
	}

	unsigned long long ParseMilliseconds(
		const System::Xml::XmlElement& element,
		std::string_view fixtureId)
	{
		const auto value = ParseInt64(
			RequireAttribute(element, "atMilliseconds"), "atMilliseconds");
		if (value < 0)
			throw std::runtime_error("Fixture " + std::string(fixtureId)
				+ " contains a negative timeline offset.");
		return static_cast<unsigned long long>(value);
	}

	bool IsStableId(std::string_view value) noexcept
	{
		if (value.empty()) return false;
		auto isAsciiLetter = [](unsigned char character)
		{
			return (character >= 'A' && character <= 'Z')
				|| (character >= 'a' && character <= 'z');
		};
		const auto first = static_cast<unsigned char>(value.front());
		if (!isAsciiLetter(first) && first != '_') return false;
		return std::all_of(value.begin() + 1, value.end(), [&](char character)
			{
				const auto current = static_cast<unsigned char>(character);
				return isAsciiLetter(current)
					|| (current >= '0' && current <= '9')
					|| current == '_' || current == '.' || current == '-';
			});
	}

	bool ContainsCaseInsensitiveAscii(
		std::string_view value, std::string_view needle)
	{
		if (needle.empty()) return true;
		if (value.size() < needle.size()) return false;
		auto lower = [](unsigned char character)
		{
			return character >= 'A' && character <= 'Z'
				? static_cast<unsigned char>(character - 'A' + 'a') : character;
		};
		for (std::size_t first = 0; first + needle.size() <= value.size(); ++first)
		{
			bool equal = true;
			for (std::size_t offset = 0; offset < needle.size(); ++offset)
				if (lower(static_cast<unsigned char>(value[first + offset]))
					!= lower(static_cast<unsigned char>(needle[offset])))
				{
					equal = false;
					break;
				}
			if (equal) return true;
		}
		return false;
	}

	void RejectMarkupExtensions(const System::Xml::XmlElement& element)
	{
		for (const auto& attribute : element.Attributes())
			if (attribute->Value().find('{') != std::string::npos
				|| attribute->Value().find('}') != std::string::npos)
				throw std::runtime_error(
					"Timeline fragment contains an unsupported markup extension.");
	}

	void ValidateEasingProperty(
		const System::Xml::XmlElement& owner,
		std::string_view propertyName,
		bool required)
	{
		const auto properties = ChildElements(owner);
		if (properties.empty() && !required) return;
		if (properties.size() != 1 || properties.front()->Name() != propertyName)
			throw std::runtime_error("Timeline fragment has an invalid "
				+ std::string(propertyName) + " property element.");
		const auto& property = *properties.front();
		if (!property.NamespaceURI().empty())
			throw std::runtime_error(
				"Timeline easing property uses an unsupported XML namespace.");
		RequireOnlyAttributes(property, {});
		const auto easings = ChildElements(property);
		if (easings.size() != 1)
			throw std::runtime_error(
				"Timeline fragment has an unsupported easing function.");
		const auto& easing = *easings.front();
		if (!easing.NamespaceURI().empty())
			throw std::runtime_error(
				"Timeline easing uses an unsupported XML namespace.");
		const auto& easingName = easing.Name();
		if (easingName == "BackEase")
			RequireOnlyAttributes(easing, { "EasingMode", "Amplitude" });
		else if (easingName == "BounceEase")
			RequireOnlyAttributes(
				easing, { "EasingMode", "Bounces", "Bounciness" });
		else if (easingName == "ElasticEase")
			RequireOnlyAttributes(
				easing, { "EasingMode", "Oscillations", "Springiness" });
		else if (easingName == "ExponentialEase")
			RequireOnlyAttributes(easing, { "EasingMode", "Exponent" });
		else if (easingName == "PowerEase")
			RequireOnlyAttributes(easing, { "EasingMode", "Power" });
		else if (easingName == "CircleEase" || easingName == "CubicEase"
			|| easingName == "QuadraticEase" || easingName == "QuarticEase"
			|| easingName == "QuinticEase" || easingName == "SineEase")
			RequireOnlyAttributes(easing, { "EasingMode" });
		else
			throw std::runtime_error(
				"Timeline fragment has an unsupported easing function.");
		RejectMarkupExtensions(easing);
		if (!ChildElements(easing).empty())
			throw std::runtime_error(
				"Timeline easing function must not contain child elements.");
	}

	void ValidatePathProperty(
		const System::Xml::XmlElement& animation)
	{
		const auto expectedProperty = animation.Name() + ".PathGeometry";
		const auto properties = ChildElements(animation);
		if (properties.size() != 1 || properties.front()->Name() != expectedProperty)
			throw std::runtime_error(
				"UsingPath must contain exactly one PathGeometry property element.");
		const auto requirePlain = [](const System::Xml::XmlElement& element)
		{
			if (!element.NamespaceURI().empty()
				|| element.Name().find(':') != std::string::npos)
				throw std::runtime_error(
					"UsingPath geometry uses an unsupported XML namespace.");
			if (!TrimAscii(element.InnerText()).empty())
				throw std::runtime_error(
					"UsingPath geometry may not contain text content.");
		};
		const auto& property = *properties.front();
		requirePlain(property);
		RequireOnlyAttributes(property, {});
		const auto geometries = ChildElements(property);
		if (geometries.size() != 1 || geometries.front()->Name() != "PathGeometry")
			throw std::runtime_error(
				"UsingPath PathGeometry property must contain one PathGeometry.");
		const auto& geometry = *geometries.front();
		requirePlain(geometry);
		RequireOnlyAttributes(geometry, { "Figures" });
		std::function<void(const System::Xml::XmlElement&)> validateTransform;
		validateTransform = [&](const System::Xml::XmlElement& transform)
		{
			requirePlain(transform);
			RejectMarkupExtensions(transform);
			const auto children = ChildElements(transform);
			if (transform.Name() == "TransformGroup")
			{
				RequireOnlyAttributes(transform, {});
				if (children.empty())
					throw std::runtime_error("TransformGroup may not be empty.");
				for (const auto& child : children) validateTransform(*child);
				return;
			}
			if (!children.empty())
				throw std::runtime_error("Transform leaf may not contain children.");
			if (transform.Name() == "MatrixTransform")
			{
				RequireOnlyAttributes(transform, { "Matrix" });
				(void)ParseMatrix(
					RequireAttribute(transform, "Matrix"), "MatrixTransform.Matrix");
				return;
			}
			const auto validateNumbers =
				[&](std::initializer_list<std::string_view> allowed)
			{
				RequireOnlyAttributes(transform, allowed);
				for (const auto name : allowed)
					if (transform.HasAttribute(name))
						(void)ParseDouble(transform.GetAttribute(name), name);
			};
			if (transform.Name() == "TranslateTransform")
				validateNumbers({ "X", "Y" });
			else if (transform.Name() == "ScaleTransform")
				validateNumbers({ "ScaleX", "ScaleY", "CenterX", "CenterY" });
			else if (transform.Name() == "RotateTransform")
				validateNumbers({ "Angle", "CenterX", "CenterY" });
			else if (transform.Name() == "SkewTransform")
				validateNumbers({ "AngleX", "AngleY", "CenterX", "CenterY" });
			else throw std::runtime_error("UsingPath contains an unsupported transform.");
		};
		const auto geometryChildren = ChildElements(geometry);
		std::vector<std::shared_ptr<System::Xml::XmlElement>> figures;
		std::vector<std::shared_ptr<System::Xml::XmlElement>> transformProperties;
		for (const auto& child : geometryChildren)
		{
			if (child->Name() == "PathFigure") figures.push_back(child);
			else if (child->Name() == "PathGeometry.Transform")
				transformProperties.push_back(child);
			else throw std::runtime_error("UsingPath PathGeometry has an unsupported child.");
		}
		const bool abbreviated = geometry.HasAttribute("Figures");
		if ((!abbreviated && figures.empty())
			|| (abbreviated && (IsBlank(geometry.GetAttribute("Figures"))
				|| !figures.empty())) || transformProperties.size() > 1)
			throw std::runtime_error(
				"UsingPath requires at least one PathFigure.");
		if (!transformProperties.empty())
		{
			const auto& transformProperty = *transformProperties.front();
			requirePlain(transformProperty);
			RequireOnlyAttributes(transformProperty, {});
			const auto transforms = ChildElements(transformProperty);
			if (transforms.size() != 1)
				throw std::runtime_error(
					"PathGeometry.Transform requires exactly one transform.");
			validateTransform(*transforms.front());
		}
		if (abbreviated) return;
		bool anyAccepted = false;
		for (const auto& figurePointer : figures)
		{
		const auto& figure = *figurePointer;
		requirePlain(figure);
		RequireOnlyAttributes(figure, { "StartPoint", "IsClosed" });
		(void)RequireAttribute(figure, "StartPoint");
		(void)ParseBoolean(figure.HasAttribute("IsClosed")
			? figure.GetAttribute("IsClosed") : "false", "PathFigure.IsClosed");
		RejectMarkupExtensions(figure);
		const auto segments = ChildElements(figure);
		auto current = ParseVector(
			RequireAttribute(figure, "StartPoint"), "UsingPath StartPoint");
		bool accepted = false;
		auto length = [](cui::core::Vector left, cui::core::Vector right)
			{ return std::hypot(right.x - left.x, right.y - left.y); };
		auto parsePoints = [&](const std::string& text)
		{
			std::vector<cui::core::Vector> points;
			std::istringstream stream(text);
			std::string token;
			while (stream >> token)
				points.push_back(ParseVector(token, "PolySegment.Points"));
			return points;
		};
		for (const auto& segmentPointer : segments)
		{
			const auto& segment = *segmentPointer;
			requirePlain(segment);
			RejectMarkupExtensions(segment);
			if (!ChildElements(segment).empty())
				throw std::runtime_error(
					"UsingPath segment may not contain child elements.");
			if (segment.Name() == "LineSegment")
			{
				RequireOnlyAttributes(segment, { "Point" });
				const auto end = ParseVector(
					RequireAttribute(segment, "Point"), "LineSegment.Point");
				if (length(current, end) >= 1.0e-6f)
				{
					accepted = true;
					current = end;
				}
			}
			else if (segment.Name() == "BezierSegment")
			{
				RequireOnlyAttributes(segment, { "Point1", "Point2", "Point3" });
				const auto point1 = ParseVector(
					RequireAttribute(segment, "Point1"), "BezierSegment.Point1");
				const auto point2 = ParseVector(
					RequireAttribute(segment, "Point2"), "BezierSegment.Point2");
				const auto point3 = ParseVector(
					RequireAttribute(segment, "Point3"), "BezierSegment.Point3");
				if (3.0f * length(current, point1) >= 1.0e-6f
					|| length(point1, point2) >= 1.0e-6f
					|| length(point1, point3) >= 1.0e-6f)
				{
					accepted = true;
					current = point3;
				}
			}
			else if (segment.Name() == "QuadraticBezierSegment")
			{
				RequireOnlyAttributes(segment, { "Point1", "Point2" });
				const auto point1 = ParseVector(
					RequireAttribute(segment, "Point1"),
					"QuadraticBezierSegment.Point1");
				const auto point2 = ParseVector(
					RequireAttribute(segment, "Point2"),
					"QuadraticBezierSegment.Point2");
				constexpr double oneThird = 0.33333333333333333;
				constexpr double twoThirds = 0.66666666666666666;
				const cui::core::Vector cubic1{
					static_cast<float>(oneThird * current.x + twoThirds * point1.x),
					static_cast<float>(oneThird * current.y + twoThirds * point1.y) };
				const cui::core::Vector cubic2{
					static_cast<float>(twoThirds * point1.x + oneThird * point2.x),
					static_cast<float>(twoThirds * point1.y + oneThird * point2.y) };
				if (3.0f * length(current, cubic1) >= 1.0e-6f
					|| length(cubic1, cubic2) >= 1.0e-6f
					|| length(cubic1, point2) >= 1.0e-6f)
				{
					accepted = true;
					current = point2;
				}
			}
			else if (segment.Name() == "PolyLineSegment"
				|| segment.Name() == "PolyBezierSegment"
				|| segment.Name() == "PolyQuadraticBezierSegment")
			{
				RequireOnlyAttributes(segment, { "Points" });
				const auto points = parsePoints(segment.HasAttribute("Points")
					? segment.GetAttribute("Points") : "");
				const std::size_t group = segment.Name() == "PolyBezierSegment"
					? 3u : (segment.Name() == "PolyQuadraticBezierSegment" ? 2u : 1u);
				const auto count = points.size() - points.size() % group;
				for (std::size_t offset = 0; offset < count; offset += group)
				{
					if (group == 1u)
					{
						if (length(current, points[offset]) >= 1.0e-6f)
						{
							accepted = true;
							current = points[offset];
						}
					}
					else if (group == 3u)
					{
						const auto point1 = points[offset];
						const auto point2 = points[offset + 1u];
						const auto point3 = points[offset + 2u];
						if (3.0f * length(current, point1) >= 1.0e-6f
							|| length(point1, point2) >= 1.0e-6f
							|| length(point1, point3) >= 1.0e-6f)
						{
							accepted = true;
							current = point3;
						}
					}
					else
					{
						const auto point1 = points[offset];
						const auto point2 = points[offset + 1u];
						constexpr double oneThird = 0.33333333333333333;
						constexpr double twoThirds = 0.66666666666666666;
						const cui::core::Vector cubic1{
							static_cast<float>(oneThird * current.x + twoThirds * point1.x),
							static_cast<float>(oneThird * current.y + twoThirds * point1.y) };
						const cui::core::Vector cubic2{
							static_cast<float>(twoThirds * point1.x + oneThird * point2.x),
							static_cast<float>(twoThirds * point1.y + oneThird * point2.y) };
						if (3.0f * length(current, cubic1) >= 1.0e-6f
							|| length(cubic1, cubic2) >= 1.0e-6f
							|| length(cubic1, point2) >= 1.0e-6f)
						{
							accepted = true;
							current = point2;
						}
					}
				}
			}
			else if (segment.Name() == "ArcSegment")
			{
				RequireOnlyAttributes(segment, { "Point", "Size", "RotationAngle",
					"IsLargeArc", "SweepDirection" });
				const auto end = ParseVector(
					RequireAttribute(segment, "Point"), "ArcSegment.Point");
				const auto size = ParseVector(
					RequireAttribute(segment, "Size"), "ArcSegment.Size");
				if (size.x < 0.0f || size.y < 0.0f)
					throw std::runtime_error("ArcSegment.Size must be non-negative.");
				(void)ParseDouble(
					segment.HasAttribute("RotationAngle")
						? segment.GetAttribute("RotationAngle") : "0",
					"ArcSegment.RotationAngle");
				(void)ParseBoolean(
					segment.HasAttribute("IsLargeArc")
						? segment.GetAttribute("IsLargeArc") : "false",
					"ArcSegment.IsLargeArc");
				const auto sweep = segment.HasAttribute("SweepDirection")
					? segment.GetAttribute("SweepDirection") : "Counterclockwise";
				if (sweep != "Clockwise" && sweep != "Counterclockwise")
					throw std::runtime_error("ArcSegment.SweepDirection is invalid.");
				if (length(current, end) >= 1.0e-6f)
				{
					accepted = true;
					current = end;
				}
			}
			else throw std::runtime_error(
				"UsingPath contains an unsupported PathSegment.");
		}
		anyAccepted = anyAccepted || accepted;
		}
		if (!anyAccepted)
			throw std::runtime_error(
				"UsingPath requires a non-degenerate accepted segment.");
	}

	void ValidateTimelineRoot(
		const System::Xml::XmlElement& animation,
		const AnimationTarget& target)
	{
		if (!animation.NamespaceURI().empty()
			|| animation.Name().find(':') != std::string::npos)
			throw std::runtime_error(
				"Timeline fragment uses an unsupported XML namespace.");
		const bool doublePath = animation.Name() == "DoubleAnimationUsingPath";
		const bool pointPath = animation.Name() == "PointAnimationUsingPath";
		const bool matrixPath = animation.Name() == "MatrixAnimationUsingPath";
		if (doublePath)
			RequireOnlyAttributes(animation, {
				"Storyboard.TargetName", "Storyboard.TargetProperty", "Source",
				"BeginTime", "Duration", "FillBehavior", "RepeatBehavior",
				"AutoReverse", "SpeedRatio", "AccelerationRatio",
				"DecelerationRatio", "IsAdditive", "IsCumulative" });
		else if (pointPath)
			RequireOnlyAttributes(animation, {
				"Storyboard.TargetName", "Storyboard.TargetProperty",
				"BeginTime", "Duration", "FillBehavior", "RepeatBehavior",
				"AutoReverse", "SpeedRatio", "AccelerationRatio",
				"DecelerationRatio", "IsAdditive", "IsCumulative" });
		else if (matrixPath)
			RequireOnlyAttributes(animation, {
				"Storyboard.TargetName", "Storyboard.TargetProperty",
				"DoesRotateWithTangent", "IsOffsetCumulative",
				"IsAngleCumulative", "BeginTime", "Duration", "FillBehavior",
				"RepeatBehavior", "AutoReverse", "SpeedRatio",
				"AccelerationRatio", "DecelerationRatio", "IsAdditive" });
		else if (animation.Name() == "ObjectAnimationUsingKeyFrames")
			RequireOnlyAttributes(animation, {
				"Storyboard.TargetName", "Storyboard.TargetProperty",
				"BeginTime", "Duration", "FillBehavior", "RepeatBehavior",
				"AutoReverse", "SpeedRatio", "AccelerationRatio",
				"DecelerationRatio" });
		else RequireOnlyAttributes(animation, {
			"Storyboard.TargetName", "Storyboard.TargetProperty",
			"BeginTime", "Duration", "FillBehavior", "RepeatBehavior",
			"AutoReverse", "SpeedRatio", "AccelerationRatio",
			"DecelerationRatio", "IsAdditive", "IsCumulative",
			"From", "To", "By" });
		RejectMarkupExtensions(animation);
		const auto targetProperty = RequireAttribute(
			animation, "Storyboard.TargetProperty");
		const bool primaryTarget = (target.Probe == "metadata-double"
			|| target.Probe == "metadata-int32"
			|| target.Probe == "metadata-int64"
			|| target.Probe == "metadata-single"
			|| target.Probe == "metadata-boolean"
			|| target.Probe == "metadata-string"
			|| target.Probe == "metadata-color"
			|| target.Probe == "metadata-vector"
			|| target.Probe == "metadata-rect"
			|| target.Probe == "metadata-size"
			|| target.Probe == "metadata-matrix")
			? !animation.HasAttribute("Storyboard.TargetName")
				&& targetProperty == target.PropertyPath
			: RequireAttribute(animation, "Storyboard.TargetName") == target.Name
				&& targetProperty == target.PropertyPath;
		const bool auxiliaryTarget = target.Probe == "canvas-left"
			&& animation.Name() == "DoubleAnimation"
			&& animation.HasAttribute("Storyboard.TargetName")
			&& animation.GetAttribute("Storyboard.TargetName") == target.Name
			&& targetProperty == "(Canvas.Top)"
			&& animation.HasAttribute("From")
			&& animation.HasAttribute("To");
		if (!primaryTarget && !auxiliaryTarget)
			throw std::runtime_error(
				"Timeline fragment target differs from the target contract.");
		const auto children = ChildElements(animation);
		if (doublePath || pointPath || matrixPath)
		{
			const auto expectedKind = doublePath ? "double"
				: pointPath ? "point" : "matrix";
			if (target.Kind != expectedKind)
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidatePathProperty(animation);
			return;
		}
		if (animation.Name() == "DoubleAnimation")
		{
			if (target.Kind != "double")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "DoubleAnimation.EasingFunction", false);
			return;
		}
		const std::pair<std::string_view, std::string_view> scalarAnimations[] = {
			{ "Int32Animation", "int32" },
			{ "Int64Animation", "int64" },
			{ "SingleAnimation", "single" }
		};
		for (const auto& [animationName, targetKind] : scalarAnimations)
		{
			if (animation.Name() != animationName) continue;
			if (target.Kind != targetKind)
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(animation,
				std::string(animationName) + ".EasingFunction", false);
			return;
		}
		if (animation.Name() == "PointAnimation")
		{
			if (target.Kind != "point")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "PointAnimation.EasingFunction", false);
			return;
		}
		if (animation.Name() == "ThicknessAnimation")
		{
			if (target.Kind != "thickness")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "ThicknessAnimation.EasingFunction", false);
			return;
		}
		if (animation.Name() == "ColorAnimation")
		{
			if (target.Kind != "color")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "ColorAnimation.EasingFunction", false);
			return;
		}
		if (animation.Name() == "VectorAnimation")
		{
			if (target.Kind != "vector")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "VectorAnimation.EasingFunction", false);
			return;
		}
		if (animation.Name() == "RectAnimation")
		{
			if (target.Kind != "rect")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "RectAnimation.EasingFunction", false);
			return;
		}
		if (animation.Name() == "SizeAnimation")
		{
			if (target.Kind != "size")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			ValidateEasingProperty(
				animation, "SizeAnimation.EasingFunction", false);
			return;
		}
		if (animation.Name() == "MatrixAnimationUsingKeyFrames")
		{
			if (target.Kind != "matrix")
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			if (animation.HasAttribute("From") || animation.HasAttribute("To")
				|| animation.HasAttribute("By"))
				throw std::runtime_error(
					"MatrixAnimationUsingKeyFrames cannot declare From, To, or By.");
			if (children.empty())
				throw std::runtime_error(
					"MatrixAnimationUsingKeyFrames must contain a key frame.");
			for (const auto& keyFrame : children)
			{
				if (!keyFrame->NamespaceURI().empty()
					|| keyFrame->Name().find(':') != std::string::npos)
					throw std::runtime_error(
						"Matrix key frame uses an unsupported XML namespace.");
				if (keyFrame->Name() != "DiscreteMatrixKeyFrame")
					throw std::runtime_error(
						"Fixture v1 contains an unsupported Matrix key-frame type.");
				RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
				RejectMarkupExtensions(*keyFrame);
				if (!ChildElements(*keyFrame).empty())
					throw std::runtime_error(
						"Matrix key frame must not contain child elements.");
			}
			return;
		}
		const std::pair<std::string_view, std::string_view> typedKeyFrameAnimations[] = {
			{ "PointAnimationUsingKeyFrames", "point" },
			{ "ThicknessAnimationUsingKeyFrames", "thickness" },
			{ "ColorAnimationUsingKeyFrames", "color" },
			{ "VectorAnimationUsingKeyFrames", "vector" },
			{ "RectAnimationUsingKeyFrames", "rect" },
			{ "SizeAnimationUsingKeyFrames", "size" }
		};
		for (const auto& [animationName, targetKind] : typedKeyFrameAnimations)
		{
			if (animation.Name() != animationName) continue;
			if (target.Kind != targetKind)
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			if (animation.HasAttribute("From") || animation.HasAttribute("To")
				|| animation.HasAttribute("By"))
				throw std::runtime_error(std::string(animationName)
					+ " cannot declare From, To, or By.");
			if (children.empty())
				throw std::runtime_error(std::string(animationName)
					+ " must contain a key frame.");
			const auto valueType = animationName.substr(
				0u, animationName.size() - std::string_view("AnimationUsingKeyFrames").size());
			const auto discreteName = "Discrete" + std::string(valueType) + "KeyFrame";
			const auto linearName = "Linear" + std::string(valueType) + "KeyFrame";
			for (const auto& keyFrame : children)
			{
				if (!keyFrame->NamespaceURI().empty()
					|| keyFrame->Name().find(':') != std::string::npos)
					throw std::runtime_error(
						"Typed key frame uses an unsupported XML namespace.");
				if (keyFrame->Name() != discreteName && keyFrame->Name() != linearName)
					throw std::runtime_error(std::string(animationName)
						+ " contains an unsupported key-frame type.");
				RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
				RejectMarkupExtensions(*keyFrame);
				if (!ChildElements(*keyFrame).empty())
					throw std::runtime_error(
						"Typed key frame must not contain child elements.");
			}
			return;
		}
		if (animation.Name() == "ObjectAnimationUsingKeyFrames")
		{
			const auto* geometry = FindGeometryLeafProbe(target.Probe);
			if (target.Kind != "string" || !geometry
				|| !GeometryLeafUsesObjectEnum(*geometry)
				|| animation.HasAttribute("From") || animation.HasAttribute("To")
				|| animation.HasAttribute("By") || children.empty())
				throw std::runtime_error(
					"ObjectAnimationUsingKeyFrames is not valid for this target.");
			for (const auto& keyFrame : children)
			{
				if (keyFrame->Name() != "DiscreteObjectKeyFrame")
					throw std::runtime_error(
						"Object animation contains an unsupported key-frame type.");
				RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
				RejectMarkupExtensions(*keyFrame);
				if (!ChildElements(*keyFrame).empty())
					throw std::runtime_error(
						"DiscreteObjectKeyFrame must not contain child elements.");
				const auto value = RequireAttribute(*keyFrame, "Value");
				const bool valid = geometry->Value == GeometryLeafProbeValue::FillRule
					? value == "EvenOdd" || value == "Nonzero"
					: value == "Clockwise" || value == "Counterclockwise";
				if (!valid)
					throw std::runtime_error(
						"Geometry enum key-frame token is invalid.");
			}
			return;
		}
		const std::pair<std::string_view, std::string_view> aliasKeyFrameAnimations[] = {
			{ "Int32AnimationUsingKeyFrames", "int32" },
			{ "Int64AnimationUsingKeyFrames", "int64" },
			{ "SingleAnimationUsingKeyFrames", "single" },
			{ "BooleanAnimationUsingKeyFrames", "boolean" },
			{ "StringAnimationUsingKeyFrames", "string" }
		};
		for (const auto& [animationName, targetKind] : aliasKeyFrameAnimations)
		{
			if (animation.Name() != animationName) continue;
			if (target.Kind != targetKind)
				throw std::runtime_error(
					"Timeline animation kind differs from the target contract.");
			if (animation.HasAttribute("From") || animation.HasAttribute("To")
				|| animation.HasAttribute("By") || children.empty())
				throw std::runtime_error(std::string(animationName)
					+ " must contain key frames and cannot declare endpoints.");
			const auto valueType = animationName.substr(0u,
				animationName.size() - std::string_view("AnimationUsingKeyFrames").size());
			const bool discreteOnly = targetKind == "boolean" || targetKind == "string";
			if (discreteOnly && (animation.HasAttribute("IsAdditive")
				|| animation.HasAttribute("IsCumulative")))
				throw std::runtime_error(std::string(animationName)
					+ " cannot declare additive or cumulative behavior.");
			for (const auto& keyFrame : children)
			{
				if (!keyFrame->NamespaceURI().empty()
					|| keyFrame->Name().find(':') != std::string::npos)
					throw std::runtime_error(
						"Alias key frame uses an unsupported XML namespace.");
				const auto discrete = "Discrete" + std::string(valueType) + "KeyFrame";
				const auto linear = "Linear" + std::string(valueType) + "KeyFrame";
				const auto spline = "Spline" + std::string(valueType) + "KeyFrame";
				const auto easing = "Easing" + std::string(valueType) + "KeyFrame";
				if (keyFrame->Name() == discrete
					|| (!discreteOnly && keyFrame->Name() == linear))
				{
					RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
					RejectMarkupExtensions(*keyFrame);
					if (!ChildElements(*keyFrame).empty())
						throw std::runtime_error(
							"Alias key frame must not contain child elements.");
				}
				else if (!discreteOnly && keyFrame->Name() == spline)
				{
					RequireOnlyAttributes(*keyFrame,
						{ "KeyTime", "Value", "KeySpline" });
					RejectMarkupExtensions(*keyFrame);
					if (!ChildElements(*keyFrame).empty())
						throw std::runtime_error(
							"Alias spline key frame must not contain child elements.");
				}
				else if (!discreteOnly && keyFrame->Name() == easing)
				{
					RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
					RejectMarkupExtensions(*keyFrame);
					ValidateEasingProperty(*keyFrame, easing + ".EasingFunction", true);
				}
				else throw std::runtime_error(std::string(animationName)
					+ " contains an unsupported key-frame type.");
			}
			return;
		}
		if (animation.Name() != "DoubleAnimationUsingKeyFrames")
			throw std::runtime_error(
				"Timeline root must be a supported Double, Point, Thickness, Color, Vector, Rect, Size, or Matrix animation.");
		if (target.Kind != "double")
			throw std::runtime_error(
				"Timeline animation kind differs from the target contract.");
		if (animation.HasAttribute("From") || animation.HasAttribute("To")
			|| animation.HasAttribute("By"))
			throw std::runtime_error(
				"DoubleAnimationUsingKeyFrames cannot declare From, To, or By.");
		if (children.empty())
			throw std::runtime_error(
				"DoubleAnimationUsingKeyFrames must contain a key frame.");
		for (const auto& keyFrame : children)
		{
			if (!keyFrame->NamespaceURI().empty()
				|| keyFrame->Name().find(':') != std::string::npos)
				throw std::runtime_error(
					"Key frame uses an unsupported XML namespace.");
			if (keyFrame->Name() == "DiscreteDoubleKeyFrame"
				|| keyFrame->Name() == "LinearDoubleKeyFrame")
			{
				RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
				RejectMarkupExtensions(*keyFrame);
				if (!ChildElements(*keyFrame).empty())
					throw std::runtime_error(
						"Double key frame must not contain child elements.");
			}
			else if (keyFrame->Name() == "SplineDoubleKeyFrame")
			{
				RequireOnlyAttributes(*keyFrame,
					{ "KeyTime", "Value", "KeySpline" });
				RejectMarkupExtensions(*keyFrame);
				if (!ChildElements(*keyFrame).empty())
					throw std::runtime_error(
						"SplineDoubleKeyFrame must not contain child elements.");
			}
			else if (keyFrame->Name() == "EasingDoubleKeyFrame")
			{
				RequireOnlyAttributes(*keyFrame, { "KeyTime", "Value" });
				RejectMarkupExtensions(*keyFrame);
				ValidateEasingProperty(
					*keyFrame, "EasingDoubleKeyFrame.EasingFunction", true);
			}
			else
			{
				throw std::runtime_error(
					"Fixture v1 contains an unsupported double key-frame type.");
			}
		}
	}

	void ValidateTimeline(
		std::string_view timeline,
		std::string_view fixtureId,
		const AnimationTarget& target)
	{
		if (timeline.empty() || timeline.size() > MaximumTimelineBytes
			|| ContainsCaseInsensitiveAscii(timeline, "<!doctype")
			|| ContainsCaseInsensitiveAscii(timeline, "x:class"))
			throw std::runtime_error("Fixture " + std::string(fixtureId)
				+ " has an unsafe or unsupported timeline.");
		System::Xml::XmlReaderSettings settings;
		settings.DtdProcessing = System::Xml::DtdProcessing::Prohibit;
		settings.MaxCharactersInDocument = MaximumTimelineBytes;
		settings.IgnoreComments = false;
		settings.IgnoreProcessingInstructions = false;
		const auto wrapped = std::string("<fragment>") + std::string(timeline)
			+ "</fragment>";
		auto document = System::Xml::XmlDocument::Parse(wrapped, settings);
		auto root = document->DocumentElement();
		if (!root) throw std::runtime_error("Timeline fragment has no root element.");
		const auto elements = ChildElements(*root);
		if (elements.empty() || elements.size() > 2u)
			throw std::runtime_error("Fixture " + std::string(fixtureId)
				+ " must contain one or two supported timeline roots.");
		std::vector<const System::Xml::XmlElement*> animations;
		std::function<void(const System::Xml::XmlElement&)> collect;
		collect = [&](const System::Xml::XmlElement& element)
		{
			if (element.Name() != "ParallelTimeline")
			{
				ValidateTimelineRoot(element, target);
				animations.push_back(&element);
				return;
			}
			if (!element.NamespaceURI().empty()
				|| element.Name().find(':') != std::string::npos)
				throw std::runtime_error(
					"ParallelTimeline uses an unsupported XML namespace.");
			RequireOnlyAttributes(element, {
				"BeginTime", "Duration", "FillBehavior", "RepeatBehavior",
				"AutoReverse", "SpeedRatio", "AccelerationRatio",
				"DecelerationRatio" });
			RejectMarkupExtensions(element);
			const auto children = ChildElements(element);
			if (children.empty())
				throw std::runtime_error("ParallelTimeline must contain a child timeline.");
			for (const auto& child : children) collect(*child);
		};
		for (const auto& element : elements) collect(*element);
		if (animations.empty() || animations.size() > 2u)
			throw std::runtime_error("Fixture " + std::string(fixtureId)
				+ " must contain one or two supported animations.");
		size_t primaryTargets = 0;
		for (const auto* element : animations)
		{
			if (!element) throw std::runtime_error("Timeline animation is missing.");
			const auto targetProperty = RequireAttribute(
				*element, "Storyboard.TargetProperty");
			const bool primary = (target.Probe == "metadata-double"
				|| target.Probe == "metadata-int32"
				|| target.Probe == "metadata-int64"
				|| target.Probe == "metadata-single"
				|| target.Probe == "metadata-boolean"
				|| target.Probe == "metadata-string"
				|| target.Probe == "metadata-color"
				|| target.Probe == "metadata-vector"
				|| target.Probe == "metadata-rect"
				|| target.Probe == "metadata-size"
				|| target.Probe == "metadata-matrix")
				? !element->HasAttribute("Storyboard.TargetName")
					&& targetProperty == target.PropertyPath
				: element->HasAttribute("Storyboard.TargetName")
					&& element->GetAttribute("Storyboard.TargetName") == target.Name
					&& targetProperty == target.PropertyPath;
			if (primary) ++primaryTargets;
		}
		if (primaryTargets != 1u)
			throw std::runtime_error("Fixture " + std::string(fixtureId)
				+ " must contain exactly one primary animation target.");
	}

	std::string ParseSupport(
		std::string value, std::string_view fixtureId, std::string_view engine)
	{
		if (value != "supported" && value != "expected-gap" && value != "deferred")
			throw std::runtime_error("Fixture " + std::string(fixtureId)
				+ " has unknown " + std::string(engine)
				+ " support status: " + value + ".");
		return value;
	}

	void EnsureOrdered(
		std::span<const AnimationSampleRequest> samples,
		std::string_view fixtureId)
	{
		for (std::size_t index = 1; index < samples.size(); ++index)
			if (samples[index].AtMilliseconds < samples[index - 1].AtMilliseconds)
				throw std::runtime_error("Fixture " + std::string(fixtureId)
					+ " samples are not ordered by parent time.");
	}

	void EnsureOrdered(
		std::span<const AnimationOperation> operations,
		std::string_view fixtureId)
	{
		for (std::size_t index = 1; index < operations.size(); ++index)
			if (operations[index].AtMilliseconds
				< operations[index - 1].AtMilliseconds)
				throw std::runtime_error("Fixture " + std::string(fixtureId)
					+ " operations are not ordered by parent time.");
	}

	AnimationFixture ParseFixture(const System::Xml::XmlElement& element)
	{
		RequireName(element, "fixture");
		RequireOnlyAttributes(element,
			{ "id", "description", "tolerance", "compare", "oracle",
				"storyboardResourceKey", "storyboardResourceScope" });
		AnimationFixture fixture;
		fixture.Id = RequireAttribute(element, "id");
		if (!IsStableId(fixture.Id))
			throw std::runtime_error("Fixture id is not stable: " + fixture.Id + ".");
		fixture.Description = RequireAttribute(element, "description");
		fixture.Tolerance = ParseDouble(
			RequireAttribute(element, "tolerance"), "tolerance");
		if (fixture.Tolerance <= 0.0)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " tolerance must be positive.");
		const auto compare = RequireAttribute(element, "compare");
		if (compare != "value" && compare != "value,isAnimated"
			&& compare != "events")
			throw std::runtime_error("Fixture " + fixture.Id
				+ " has an unsupported compare contract.");
		fixture.CompareValue = compare != "events";
		fixture.CompareIsAnimated = compare == "value,isAnimated";
		fixture.CompareEvents = compare == "events";
		if (element.HasAttribute("oracle"))
			fixture.Oracle = RequireAttribute(element, "oracle");
		if (element.HasAttribute("storyboardResourceKey"))
			fixture.StoryboardResourceKey = RequireAttribute(
				element, "storyboardResourceKey");
		if (element.HasAttribute("storyboardResourceScope"))
			fixture.StoryboardResourceScope = RequireAttribute(
				element, "storyboardResourceScope");
		if (fixture.StoryboardResourceKey && !fixture.StoryboardResourceScope)
			fixture.StoryboardResourceScope = "template-root";
		if (fixture.Oracle != "aligned-value"
			&& fixture.Oracle != "synchronous-control"
			&& fixture.Oracle != "authored-root-control"
			&& fixture.Oracle != "dispatcher-control"
			&& fixture.Oracle != "authored-root-dispatcher-control"
			&& fixture.Oracle != "dispatcher-events"
			&& fixture.Oracle != "authored-root-dispatcher-events"
			&& fixture.Oracle != "snapshot-replace"
			&& fixture.Oracle != "compose-handoff")
			throw std::runtime_error("Fixture " + fixture.Id
				+ " has an unsupported oracle contract: "
				+ fixture.Oracle + ".");

		const auto children = ChildElements(element);
		const bool hasStoryboardTiming = children.size() > 3u
			&& children[3]->Name() == "storyboardTiming";
		const auto timelineIndex = hasStoryboardTiming ? 4u : 3u;
		const bool hasReplacement = children.size() > timelineIndex + 1u
			&& children[timelineIndex + 1u]->Name() == "replacementTimeline";
		const auto operationsIndex = timelineIndex + 1u + (hasReplacement ? 1u : 0u);
		const bool hasOperations = children.size() > operationsIndex
			&& children[operationsIndex]->Name() == "operations";
		std::vector<std::string_view> expected{
			"capabilities", "support", "target" };
		if (hasStoryboardTiming) expected.push_back("storyboardTiming");
		expected.push_back("timeline");
		if (hasReplacement) expected.push_back("replacementTimeline");
		if (hasOperations) expected.push_back("operations");
		expected.push_back("samples");
		if (children.size() != expected.size())
			throw std::runtime_error("Fixture " + fixture.Id
				+ " has invalid child elements.");
		for (std::size_t index = 0; index < expected.size(); ++index)
			if (children[index]->Name() != expected[index])
				throw std::runtime_error("Fixture " + fixture.Id
					+ " has invalid child order or unknown elements.");

		const auto& capabilities = *children[0];
		RequireOnlyAttributes(capabilities, {});
		std::set<std::string, std::less<>> capabilityIds;
		for (const auto& capability : ChildElements(capabilities))
		{
			RequireName(*capability, "capability");
			RequireOnlyAttributes(*capability, { "id" });
			RequireNoChildren(*capability);
			auto id = RequireAttribute(*capability, "id");
			if (!capabilityIds.insert(id).second)
				throw std::runtime_error("Fixture " + fixture.Id
					+ " contains duplicate capability id " + id + ".");
			fixture.Capabilities.push_back(std::move(id));
		}
		if (fixture.Capabilities.empty())
			throw std::runtime_error("Fixture " + fixture.Id
				+ " must declare a capability.");

		const auto& support = *children[1];
		RequireOnlyAttributes(support, { "wpf", "cui" });
		RequireNoChildren(support);
		fixture.WpfSupport = ParseSupport(
			RequireAttribute(support, "wpf"), fixture.Id, "wpf");
		fixture.CuiSupport = ParseSupport(
			RequireAttribute(support, "cui"), fixture.Id, "cui");

		const auto& target = *children[2];
		RequireOnlyAttributes(target,
			{ "kind", "name", "propertyPath", "probe",
				"baseValue", "baseColor", "baseVector", "baseRect", "baseSize",
				"baseMatrix", "baseInt32", "baseInt64", "baseSingle",
				"baseBoolean", "baseString" });
		RequireNoChildren(target);
		fixture.Target.Kind = RequireAttribute(target, "kind");
		fixture.Target.Name = RequireAttribute(target, "name");
		fixture.Target.PropertyPath = RequireAttribute(target, "propertyPath");
		fixture.Target.Probe = RequireAttribute(target, "probe");
		if (target.HasAttribute("baseValue"))
			fixture.Target.BaseValue = ParseDouble(
				RequireAttribute(target, "baseValue"), "baseValue");
		if (target.HasAttribute("baseColor"))
		{
			fixture.Target.BaseColorText = RequireAttribute(target, "baseColor");
			fixture.Target.BaseColor = ParseArgbColor(
				fixture.Target.BaseColorText, "baseColor");
		}
		if (target.HasAttribute("baseVector"))
		{
			fixture.Target.BaseVectorText = RequireAttribute(target, "baseVector");
			fixture.Target.BaseVector = ParseVector(
				fixture.Target.BaseVectorText, "baseVector");
		}
		if (target.HasAttribute("baseRect"))
		{
			fixture.Target.BaseRectText = RequireAttribute(target, "baseRect");
			fixture.Target.BaseRect = ParseRect(
				fixture.Target.BaseRectText, "baseRect");
		}
		if (target.HasAttribute("baseSize"))
		{
			fixture.Target.BaseSizeText = RequireAttribute(target, "baseSize");
			fixture.Target.BaseSize = ParseSize(
				fixture.Target.BaseSizeText, "baseSize");
		}
		if (target.HasAttribute("baseMatrix"))
		{
			fixture.Target.BaseMatrixText = RequireAttribute(target, "baseMatrix");
			fixture.Target.BaseMatrix = ParseMatrix(
				fixture.Target.BaseMatrixText, "baseMatrix");
		}
		if (target.HasAttribute("baseInt32"))
			fixture.Target.BaseInt32 = ParseSignedInteger<int>(
				RequireAttribute(target, "baseInt32"), "baseInt32");
		if (target.HasAttribute("baseInt64"))
			fixture.Target.BaseInt64 = ParseSignedInteger<long long>(
				RequireAttribute(target, "baseInt64"), "baseInt64");
		if (target.HasAttribute("baseSingle"))
			fixture.Target.BaseSingle = ParseSingle(
				RequireAttribute(target, "baseSingle"), "baseSingle");
		if (target.HasAttribute("baseBoolean"))
			fixture.Target.BaseBoolean = ParseBoolean(
				RequireAttribute(target, "baseBoolean"), "baseBoolean");
		if (target.HasAttribute("baseString"))
			fixture.Target.BaseString = target.GetAttribute("baseString");
		const bool canvasLeft = fixture.Target.Probe == "canvas-left"
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath == "(Canvas.Left)";
		const bool metadataDouble = fixture.Target.Probe == "metadata-double"
			&& fixture.Target.Name == "owner"
			&& fixture.Target.PropertyPath == "Value";
		auto metadataAlias = [&](std::string_view kind)
		{
			return fixture.Target.Kind == kind
				&& fixture.Target.Probe == "metadata-" + std::string(kind)
				&& fixture.Target.Name == "owner"
				&& fixture.Target.PropertyPath == "Value";
		};
		const bool metadataInt32 = metadataAlias("int32");
		const bool metadataInt64 = metadataAlias("int64");
		const bool metadataSingle = metadataAlias("single");
		const bool metadataBoolean = metadataAlias("boolean");
		const bool metadataString = metadataAlias("string");
		const bool renderTransformOriginPoint =
			(fixture.Target.Probe == "render-transform-origin-x"
				|| fixture.Target.Probe == "render-transform-origin-y")
			&& fixture.Target.Kind == "point"
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath == "RenderTransformOrigin";
		const bool borderThicknessLeft =
			fixture.Target.Probe == "border-thickness-left"
			&& fixture.Target.Kind == "thickness"
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath == "BorderThickness";
		const bool metadataColor = fixture.Target.Probe == "metadata-color"
			&& fixture.Target.Kind == "color"
			&& fixture.Target.Name == "owner"
			&& fixture.Target.PropertyPath == "Value";
		const bool metadataVector = fixture.Target.Probe == "metadata-vector"
			&& fixture.Target.Kind == "vector"
			&& fixture.Target.Name == "owner"
			&& fixture.Target.PropertyPath == "Value";
		const bool metadataRect = fixture.Target.Probe == "metadata-rect"
			&& fixture.Target.Kind == "rect"
			&& fixture.Target.Name == "owner"
			&& fixture.Target.PropertyPath == "Value";
		const bool metadataSize = fixture.Target.Probe == "metadata-size"
			&& fixture.Target.Kind == "size"
			&& fixture.Target.Name == "owner"
			&& fixture.Target.PropertyPath == "Value";
		const bool metadataMatrix = fixture.Target.Probe == "metadata-matrix"
			&& fixture.Target.Kind == "matrix"
			&& fixture.Target.Name == "owner"
			&& fixture.Target.PropertyPath == "Value";
		const bool geometryTransformDirectX =
			fixture.Target.Probe == "geometry-transform-direct-x"
			&& fixture.Target.Kind == "double"
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath
				== "(UIElement.Clip).(Geometry.Transform).(TranslateTransform.X)";
		const bool brushTransformDirectAngle =
			fixture.Target.Probe == "brush-transform-direct-angle"
			&& fixture.Target.Kind == "double"
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath
				== "(Panel.Background).(Brush.Transform).(RotateTransform.Angle)";
		const bool brushRelativeTransformDirectScaleX =
			fixture.Target.Probe == "brush-relative-transform-direct-scale-x"
			&& fixture.Target.Kind == "double"
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath
				== "(Border.Background).(Brush.RelativeTransform).(ScaleTransform.ScaleX)";
		const bool directObjectPathDouble = geometryTransformDirectX
			|| brushTransformDirectAngle || brushRelativeTransformDirectScaleX;
		const auto* transformLeaf = FindTransformLeafProbe(fixture.Target.Probe);
		const bool transformLeafTarget = transformLeaf
			&& fixture.Target.Kind == transformLeaf->TargetKind
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath == transformLeaf->FixturePropertyPath;
		const auto* brushLeaf = FindBrushLeafProbe(fixture.Target.Probe);
		const bool brushLeafTarget = brushLeaf
			&& fixture.Target.Kind == brushLeaf->TargetKind
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath == brushLeaf->FixturePropertyPath;
		const auto* geometryLeaf = FindGeometryLeafProbe(fixture.Target.Probe);
		const bool geometryLeafTarget = geometryLeaf
			&& fixture.Target.Kind == geometryLeaf->TargetKind
			&& fixture.Target.Name == "target"
			&& fixture.Target.PropertyPath == geometryLeaf->FixturePropertyPath;
		if ((fixture.Target.Kind != "double"
				|| (!canvasLeft && !metadataDouble && !directObjectPathDouble))
			&& !renderTransformOriginPoint && !borderThicknessLeft
			&& !metadataColor && !metadataVector
			&& !metadataRect && !metadataSize && !metadataMatrix
			&& !metadataInt32 && !metadataInt64 && !metadataSingle
			&& !metadataBoolean && !metadataString
			&& !transformLeafTarget && !brushLeafTarget && !geometryLeafTarget)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " uses a target unsupported by the v1 runner slice.");
		const auto aliasBaseCount = static_cast<unsigned>(
			target.HasAttribute("baseInt32"))
			+ static_cast<unsigned>(target.HasAttribute("baseInt64"))
			+ static_cast<unsigned>(target.HasAttribute("baseSingle"))
			+ static_cast<unsigned>(target.HasAttribute("baseBoolean"))
			+ static_cast<unsigned>(target.HasAttribute("baseString"));
		const bool validColorBase = (metadataColor
				|| (brushLeafTarget && BrushLeafUsesColor(*brushLeaf)))
			&& fixture.Target.BaseColor.has_value()
			&& aliasBaseCount == 0u
			&& !target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseVector")
			&& !target.HasAttribute("baseRect")
			&& !target.HasAttribute("baseSize")
			&& !target.HasAttribute("baseMatrix");
		const bool validVectorBase = metadataVector
			&& fixture.Target.BaseVector.has_value()
			&& aliasBaseCount == 0u
			&& !target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseColor")
			&& !target.HasAttribute("baseRect")
			&& !target.HasAttribute("baseSize")
			&& !target.HasAttribute("baseMatrix");
		const bool validRectBase = (metadataRect
				|| (geometryLeafTarget
					&& geometryLeaf->Value == GeometryLeafProbeValue::RectangleRect))
			&& fixture.Target.BaseRect.has_value()
			&& aliasBaseCount == 0u
			&& !target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseColor")
			&& !target.HasAttribute("baseVector")
			&& !target.HasAttribute("baseSize")
			&& !target.HasAttribute("baseMatrix");
		const bool validSizeBase = (metadataSize
				|| (geometryLeafTarget
					&& geometryLeaf->Value == GeometryLeafProbeValue::ArcSize))
			&& fixture.Target.BaseSize.has_value()
			&& aliasBaseCount == 0u
			&& !target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseColor")
			&& !target.HasAttribute("baseVector")
			&& !target.HasAttribute("baseRect")
			&& !target.HasAttribute("baseMatrix");
		const bool validMatrixBase = (metadataMatrix
				|| (transformLeafTarget && fixture.Target.Kind == "matrix"))
			&& fixture.Target.BaseMatrix.has_value()
			&& aliasBaseCount == 0u
			&& !target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseColor")
			&& !target.HasAttribute("baseVector")
			&& !target.HasAttribute("baseRect")
			&& !target.HasAttribute("baseSize");
		const bool validNumericBase = !metadataColor
			&& !metadataVector && !metadataRect
			&& !metadataSize && !metadataMatrix
			&& !metadataInt32 && !metadataInt64 && !metadataSingle
			&& !metadataBoolean && !metadataString
			&& aliasBaseCount == 0u
			&& target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseColor")
			&& !target.HasAttribute("baseVector")
			&& !target.HasAttribute("baseRect")
			&& !target.HasAttribute("baseSize")
			&& !target.HasAttribute("baseMatrix");
		const bool noLegacyBase = !target.HasAttribute("baseValue")
			&& !target.HasAttribute("baseColor")
			&& !target.HasAttribute("baseVector")
			&& !target.HasAttribute("baseRect")
			&& !target.HasAttribute("baseSize")
			&& !target.HasAttribute("baseMatrix");
		const bool validAliasBase = noLegacyBase && aliasBaseCount == 1u
			&& ((metadataInt32 && fixture.Target.BaseInt32.has_value())
				|| (metadataInt64 && fixture.Target.BaseInt64.has_value())
				|| (metadataSingle && fixture.Target.BaseSingle.has_value())
				|| (metadataBoolean && fixture.Target.BaseBoolean.has_value())
				|| (metadataString && fixture.Target.BaseString.has_value())
				|| (geometryLeafTarget && fixture.Target.Kind == "boolean"
					&& fixture.Target.BaseBoolean.has_value())
				|| (geometryLeafTarget && fixture.Target.Kind == "string"
					&& fixture.Target.BaseString.has_value()));
		if (!validColorBase && !validVectorBase
			&& !validRectBase && !validSizeBase
			&& !validMatrixBase && !validNumericBase && !validAliasBase)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " has an invalid typed base-value contract.");
		if (metadataDouble && fixture.Tolerance > 0.000001)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " metadata-double tolerance must not exceed 1e-6.");

		if (hasStoryboardTiming)
		{
			const auto& timing = *children[3];
			RequireOnlyAttributes(timing, {
				"beginTimeMilliseconds", "durationMilliseconds", "repeatBehavior",
				"repeatCount", "repeatDurationMilliseconds", "autoReverse",
				"fillBehavior", "speedRatio", "accelerationRatio",
				"decelerationRatio" });
			RequireNoChildren(timing);
			fixture.HasStoryboardTiming = true;
			auto milliseconds = [&](std::string_view name,
				unsigned long long fallback)
			{
				if (!timing.HasAttribute(name)) return fallback;
				const auto value = ParseInt64(RequireAttribute(timing, name), name);
				if (value < 0) throw std::runtime_error(
					"Storyboard timing milliseconds must be non-negative.");
				return static_cast<unsigned long long>(value);
			};
			auto number = [&](std::string_view name, double fallback)
			{
				return timing.HasAttribute(name)
					? ParseDouble(RequireAttribute(timing, name), name) : fallback;
			};
			auto boolean = [&](std::string_view name, bool fallback)
			{
				if (!timing.HasAttribute(name)) return fallback;
				const auto value = RequireAttribute(timing, name);
				if (value == "true") return true;
				if (value == "false") return false;
				throw std::runtime_error("Storyboard timing boolean is invalid.");
			};
			auto& output = fixture.StoryboardTiming;
			output.BeginTimeMilliseconds = milliseconds("beginTimeMilliseconds", 0);
			output.DurationAutomatic = !timing.HasAttribute("durationMilliseconds");
			output.DurationMilliseconds = milliseconds("durationMilliseconds", 0);
			const auto repeat = timing.HasAttribute("repeatBehavior")
				? RequireAttribute(timing, "repeatBehavior") : "count";
			output.RepeatBehavior = repeat == "duration"
				? DeclarativeRepeatBehaviorKind::Duration
				: repeat == "forever" ? DeclarativeRepeatBehaviorKind::Forever
				: DeclarativeRepeatBehaviorKind::Count;
			if (repeat != "count" && repeat != "duration" && repeat != "forever")
				throw std::runtime_error("Storyboard repeatBehavior is invalid.");
			output.RepeatCount = number("repeatCount", 1.0);
			output.RepeatDurationMilliseconds = milliseconds(
				"repeatDurationMilliseconds", 0);
			output.AutoReverse = boolean("autoReverse", false);
			const auto fill = timing.HasAttribute("fillBehavior")
				? RequireAttribute(timing, "fillBehavior") : "HoldEnd";
			if (fill != "HoldEnd" && fill != "Stop")
				throw std::runtime_error("Storyboard fillBehavior is invalid.");
			output.FillBehavior = fill == "Stop"
				? DeclarativeTimelineFillBehavior::Stop
				: DeclarativeTimelineFillBehavior::HoldEnd;
			output.SpeedRatio = number("speedRatio", 1.0);
			output.AccelerationRatio = number("accelerationRatio", 0.0);
			output.DecelerationRatio = number("decelerationRatio", 0.0);
			if (!std::isfinite(output.RepeatCount) || output.RepeatCount <= 0.0
				|| (output.RepeatBehavior == DeclarativeRepeatBehaviorKind::Duration
					&& output.RepeatDurationMilliseconds == 0)
				|| !std::isfinite(output.SpeedRatio) || output.SpeedRatio <= 0.0
				|| !std::isfinite(output.AccelerationRatio)
				|| !std::isfinite(output.DecelerationRatio)
				|| output.AccelerationRatio < 0.0 || output.AccelerationRatio > 1.0
				|| output.DecelerationRatio < 0.0 || output.DecelerationRatio > 1.0
				|| output.AccelerationRatio + output.DecelerationRatio > 1.0)
				throw std::runtime_error("Storyboard timing ratios are invalid.");
		}

		auto parseTimeline = [&](const System::Xml::XmlElement& timeline,
			std::string_view kind)
		{
			RequireOnlyAttributes(timeline, {});
			for (const auto& child : timeline.ChildNodes())
				if (child->NodeType() != System::Xml::XmlNodeType::Text
					&& child->NodeType() != System::Xml::XmlNodeType::CDATA
					&& child->NodeType() != System::Xml::XmlNodeType::Whitespace
					&& child->NodeType() != System::Xml::XmlNodeType::SignificantWhitespace)
					throw std::runtime_error("Fixture " + fixture.Id + " "
						+ std::string(kind) + " must contain only character data.");
			auto value = TrimAscii(timeline.InnerText());
			ValidateTimeline(value, fixture.Id, fixture.Target);
			return value;
		};
		fixture.TimelineXaml = parseTimeline(*children[timelineIndex], "timeline");
		if (hasReplacement)
			fixture.ReplacementTimelineXaml = parseTimeline(
				*children[timelineIndex + 1u], "replacement timeline");
		if (fixture.StoryboardResourceKey)
		{
			const bool replacementResource =
				(fixture.Oracle == "snapshot-replace"
					&& fixture.ReplacementTimelineXaml
					&& *fixture.ReplacementTimelineXaml == fixture.TimelineXaml
					&& fixture.Target.Probe == "canvas-left"
					&& (*fixture.StoryboardResourceScope == "template-root"
						|| *fixture.StoryboardResourceScope
							== "document-late-target"));
			const bool styleResource =
				(fixture.Oracle == "aligned-value"
					&& !fixture.ReplacementTimelineXaml
					&& fixture.Target.Probe == "metadata-double"
					&& *fixture.StoryboardResourceScope == "style-document");
			const bool stateResource =
				(fixture.Oracle == "aligned-value"
					&& !fixture.ReplacementTimelineXaml
					&& fixture.Target.Probe == "metadata-double"
					&& (*fixture.StoryboardResourceScope
						== "visual-state-document"
						|| *fixture.StoryboardResourceScope
							== "visual-transition-document"));
			if (!IsStableId(*fixture.StoryboardResourceKey)
				|| (!replacementResource && !styleResource && !stateResource))
				throw std::runtime_error("Fixture " + fixture.Id
					+ " resource Storyboard contract is unsupported.");
		}
		if (!fixture.StoryboardResourceKey && fixture.StoryboardResourceScope)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " storyboardResourceScope requires storyboardResourceKey.");

		if (hasOperations)
		{
			const auto& operations = *children[operationsIndex];
			RequireOnlyAttributes(operations, {});
			for (const auto& operation : ChildElements(operations))
			{
				RequireName(*operation, "operation");
				RequireOnlyAttributes(*operation,
					{ "atMilliseconds", "kind", "value" });
				RequireNoChildren(*operation);
				AnimationOperation parsed;
				parsed.AtMilliseconds = ParseMilliseconds(*operation, fixture.Id);
				parsed.Kind = RequireAttribute(*operation, "kind");
				if (parsed.Kind != "pause" && parsed.Kind != "resume"
					&& parsed.Kind != "stop" && parsed.Kind != "remove"
					&& parsed.Kind != "seek" && parsed.Kind != "seek-aligned"
					&& parsed.Kind != "skip-to-fill"
					&& parsed.Kind != "set-speed-ratio"
					&& parsed.Kind != "begin-replacement"
					&& parsed.Kind != "tick"
					&& parsed.Kind != "begin-tick")
					throw std::runtime_error("Fixture " + fixture.Id
						+ " has unknown operation kind: " + parsed.Kind + ".");
				if (operation->HasAttribute("value"))
					parsed.Value = ParseDouble(
						RequireAttribute(*operation, "value"), "operation value");
				const bool requiresValue = parsed.Kind == "seek"
					|| parsed.Kind == "seek-aligned"
					|| parsed.Kind == "set-speed-ratio";
				if (requiresValue != parsed.Value.has_value())
					throw std::runtime_error("Fixture " + fixture.Id
						+ " operation " + parsed.Kind
						+ " has an invalid value contract.");
				if ((parsed.Kind == "seek" || parsed.Kind == "seek-aligned")
					&& *parsed.Value < 0.0)
					throw std::runtime_error("Fixture " + fixture.Id
						+ " operation " + parsed.Kind
						+ " requires a non-negative offset.");
				if (parsed.Kind == "set-speed-ratio" && *parsed.Value < 0.0)
					throw std::runtime_error("Fixture " + fixture.Id
						+ " set-speed-ratio requires a non-negative ratio.");
				fixture.Operations.push_back(std::move(parsed));
			}
			EnsureOrdered(fixture.Operations, fixture.Id);
		}

		const auto& samples = *children.back();
		RequireOnlyAttributes(samples, {});
		for (const auto& sample : ChildElements(samples))
		{
			RequireName(*sample, "sample");
			RequireOnlyAttributes(*sample,
				{ "atMilliseconds", "label", "phase" });
			RequireNoChildren(*sample);
			AnimationSampleRequest parsed;
			parsed.AtMilliseconds = ParseMilliseconds(*sample, fixture.Id);
			parsed.Label = RequireAttribute(*sample, "label");
			if (sample->HasAttribute("phase"))
				parsed.Phase = RequireAttribute(*sample, "phase");
			if (parsed.Phase != "before-begin" && parsed.Phase != "after-begin")
				throw std::runtime_error("Fixture " + fixture.Id
					+ " has unknown sample phase: " + parsed.Phase + ".");
			fixture.Samples.push_back(std::move(parsed));
		}
		if (fixture.Samples.empty())
			throw std::runtime_error("Fixture " + fixture.Id
				+ " must contain at least one sample.");
		EnsureOrdered(fixture.Samples, fixture.Id);
		if (fixture.Oracle == "aligned-value")
		{
			if (!fixture.Operations.empty() || fixture.ReplacementTimelineXaml)
				throw std::runtime_error("Fixture " + fixture.Id
					+ " aligned-value oracle may not declare replacement input.");
		}
		else if (fixture.Oracle == "snapshot-replace"
			|| fixture.Oracle == "compose-handoff")
		{
			if (!fixture.ReplacementTimelineXaml
				|| fixture.Operations.size() != 1u
				|| fixture.Operations.front().Kind != "begin-replacement"
				|| fixture.Operations.front().Value
				|| std::any_of(fixture.Samples.begin(), fixture.Samples.end(),
					[](const auto& sample)
					{ return sample.Phase != "after-begin"; }))
				throw std::runtime_error("Fixture " + fixture.Id + " "
					+ fixture.Oracle
					+ " requires one begin-replacement operation, "
						"one replacement timeline, and after-begin samples.");
		}
		else if (fixture.Operations.empty())
		{
			throw std::runtime_error("Fixture " + fixture.Id + " "
				+ fixture.Oracle + " oracle requires operations.");
		}
		else if (fixture.Oracle == "synchronous-control"
			|| fixture.Oracle == "authored-root-control")
		{
			if (fixture.Oracle == "authored-root-control"
				&& !fixture.HasStoryboardTiming)
				throw std::runtime_error("Fixture " + fixture.Id
					+ " authored-root-control requires storyboardTiming.");
			if (std::any_of(fixture.Operations.begin(), fixture.Operations.end(),
				[](const auto& operation)
				{
					return operation.Kind != "seek-aligned"
						|| !operation.Value || *operation.Value < 0.0;
				}))
				throw std::runtime_error("Fixture " + fixture.Id
					+ " synchronous-control only accepts non-negative "
						"seek-aligned operations.");
			for (const auto& sample : fixture.Samples)
				if (sample.Phase != "after-begin"
					|| std::none_of(fixture.Operations.begin(),
						fixture.Operations.end(), [&](const auto& operation)
						{ return operation.AtMilliseconds <= sample.AtMilliseconds; }))
					throw std::runtime_error("Fixture " + fixture.Id
						+ " synchronous-control samples must follow at least "
							"one operation.");
		}
		else if (fixture.Oracle == "dispatcher-control"
			|| fixture.Oracle == "authored-root-dispatcher-control")
		{
			if (fixture.Oracle == "authored-root-dispatcher-control"
				&& !fixture.HasStoryboardTiming)
				throw std::runtime_error("Fixture " + fixture.Id
					+ " authored-root-dispatcher-control requires storyboardTiming.");
			if (std::any_of(fixture.Operations.begin(), fixture.Operations.end(),
				[](const auto& operation)
				{
					return operation.Kind != "seek-aligned"
						&& operation.Kind != "seek"
						&& operation.Kind != "pause"
						&& operation.Kind != "resume"
						&& operation.Kind != "stop"
						&& operation.Kind != "remove"
						&& operation.Kind != "set-speed-ratio"
						&& operation.Kind != "skip-to-fill"
						&& operation.Kind != "tick";
				})
				|| std::none_of(fixture.Operations.begin(),
					fixture.Operations.end(), [](const auto& operation)
					{ return operation.Kind == "tick"; }))
				throw std::runtime_error("Fixture " + fixture.Id
					+ " dispatcher-control has an invalid operation contract.");
			for (const auto& sample : fixture.Samples)
				if (sample.Phase != "after-begin"
					|| std::none_of(fixture.Operations.begin(),
						fixture.Operations.end(), [&](const auto& operation)
						{ return operation.AtMilliseconds <= sample.AtMilliseconds; }))
					throw std::runtime_error("Fixture " + fixture.Id
						+ " dispatcher-control samples must follow an operation.");
		}
		else if (fixture.Oracle == "dispatcher-events"
			|| fixture.Oracle == "authored-root-dispatcher-events")
		{
			if (fixture.Oracle == "authored-root-dispatcher-events"
				&& !fixture.HasStoryboardTiming)
				throw std::runtime_error("Fixture " + fixture.Id
					+ " authored-root-dispatcher-events requires storyboardTiming.");
			if (std::any_of(fixture.Operations.begin(), fixture.Operations.end(),
				[](const auto& operation)
				{
					return operation.Kind != "seek"
						&& operation.Kind != "pause"
						&& operation.Kind != "resume"
						&& operation.Kind != "stop"
						&& operation.Kind != "remove"
						&& operation.Kind != "set-speed-ratio"
						&& operation.Kind != "skip-to-fill"
						&& operation.Kind != "tick"
						&& operation.Kind != "begin-tick";
				}))
				throw std::runtime_error("Fixture " + fixture.Id
					+ " dispatcher-events has an invalid operation contract.");
			const auto beginTicks = static_cast<size_t>(std::count_if(
				fixture.Operations.begin(), fixture.Operations.end(),
				[](const auto& operation)
				{ return operation.Kind == "begin-tick"; }));
			const auto ticks = static_cast<size_t>(std::count_if(
				fixture.Operations.begin(), fixture.Operations.end(),
				[](const auto& operation)
				{ return operation.Kind == "tick"; }));
			if ((beginTicks != 0u
					&& (beginTicks != 1u || fixture.Operations.size() != 1u
						|| fixture.Operations.front().Kind != "begin-tick"))
				|| (beginTicks == 0u && (ticks != 1u
					|| fixture.Operations.back().Kind != "tick")))
				throw std::runtime_error("Fixture " + fixture.Id
					+ " dispatcher-events requires one final timing tick.");
			for (const auto& sample : fixture.Samples)
				if (sample.Phase != "after-begin"
					|| std::none_of(fixture.Operations.begin(),
						fixture.Operations.end(), [&](const auto& operation)
						{ return operation.AtMilliseconds <= sample.AtMilliseconds; }))
					throw std::runtime_error("Fixture " + fixture.Id
						+ " dispatcher-events samples must follow an operation.");
		}
		if ((fixture.Oracle == "dispatcher-events"
			|| fixture.Oracle == "authored-root-dispatcher-events")
			!= fixture.CompareEvents)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " dispatcher-events requires compare=events exclusively.");
		if (fixture.Oracle != "snapshot-replace"
			&& fixture.Oracle != "compose-handoff"
			&& fixture.ReplacementTimelineXaml)
			throw std::runtime_error("Fixture " + fixture.Id
				+ " replacement timeline requires snapshot-replace oracle.");
		return fixture;
	}

	AnimationCorpus ParseCorpus(
		std::string_view xml,
		std::optional<std::string> corpusSha256 = std::nullopt)
	{
		System::Xml::XmlReaderSettings settings;
		settings.DtdProcessing = System::Xml::DtdProcessing::Prohibit;
		settings.MaxCharactersInDocument = MaximumCorpusBytes;
		settings.IgnoreComments = true;
		settings.IgnoreProcessingInstructions = true;
		auto document = System::Xml::XmlDocument::Parse(xml, settings);
		auto root = document->DocumentElement();
		if (!root) throw std::runtime_error("Fixture corpus has no document element.");
		RequireName(*root, "animationFixtureCorpus");
		RequireOnlyAttributes(*root, { "schemaVersion" });
		const auto schemaVersion = ParseInt64(
			RequireAttribute(*root, "schemaVersion"), "schemaVersion");
		if (schemaVersion != 1)
			throw std::runtime_error("Unsupported fixture schemaVersion: "
				+ std::to_string(schemaVersion) + ".");

		AnimationCorpus corpus;
		corpus.SchemaVersion = 1;
		corpus.CorpusSha256 = corpusSha256
			? std::move(*corpusSha256) : Sha256(xml);
		std::set<std::string, std::less<>> fixtureIds;
		for (const auto& element : ChildElements(*root))
		{
			auto fixture = ParseFixture(*element);
			if (!fixtureIds.insert(fixture.Id).second)
				throw std::runtime_error("Duplicate fixture id: " + fixture.Id + ".");
			corpus.Fixtures.push_back(std::move(fixture));
		}
		if (corpus.Fixtures.empty())
			throw std::runtime_error(
				"Fixture corpus must contain at least one fixture.");
		return corpus;
	}

	std::string FormatDouble(double value)
	{
		if (!std::isfinite(value))
			throw std::runtime_error("Animation result contains a non-finite number.");
		char buffer[128]{};
		const auto formatted = std::to_chars(
			std::begin(buffer), std::end(buffer), value,
			std::chars_format::general,
			std::numeric_limits<double>::max_digits10);
		if (formatted.ec != std::errc{})
			throw std::runtime_error("Could not format an animation double value.");
		return std::string(buffer, formatted.ptr);
	}

	std::string EscapeXmlAttribute(std::string_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const auto character : value)
		{
			switch (character)
			{
			case '&': result += "&amp;"; break;
			case '<': result += "&lt;"; break;
			case '>': result += "&gt;"; break;
			case '"': result += "&quot;"; break;
			case '\'': result += "&apos;"; break;
			default: result.push_back(character); break;
			}
		}
		return result;
	}

	double StoredSrgbToScRgb(float value)
	{
		const auto channel = static_cast<double>(value);
		return channel <= 0.04045
			? channel / 12.92
			: std::pow((channel + 0.055) / 1.055, 2.4);
	}

	std::string TimeSpanText(unsigned long long milliseconds)
	{
		const auto hours = milliseconds / 3600000ULL;
		const auto minutes = (milliseconds / 60000ULL) % 60ULL;
		const auto seconds = (milliseconds / 1000ULL) % 60ULL;
		const auto fraction = milliseconds % 1000ULL;
		std::ostringstream text;
		text << hours << ':' << minutes << ':' << seconds;
		if (fraction != 0)
			text << '.' << std::setw(3) << std::setfill('0') << fraction;
		return text.str();
	}

	std::string StoryboardTimingAttributes(const AnimationFixture& fixture)
	{
		if (!fixture.HasStoryboardTiming) return {};
		const auto& timing = fixture.StoryboardTiming;
		std::string result;
		if (timing.BeginTimeMilliseconds > 0)
			result += " BeginTime=\"" + TimeSpanText(
				timing.BeginTimeMilliseconds) + "\"";
		if (!timing.DurationAutomatic)
			result += " Duration=\"" + TimeSpanText(
				timing.DurationMilliseconds) + "\"";
		if (timing.FillBehavior == DeclarativeTimelineFillBehavior::Stop)
			result += " FillBehavior=\"Stop\"";
		if (timing.SpeedRatio != 1.0)
			result += " SpeedRatio=\"" + FormatDouble(timing.SpeedRatio) + "\"";
		if (timing.AccelerationRatio != 0.0)
			result += " AccelerationRatio=\""
				+ FormatDouble(timing.AccelerationRatio) + "\"";
		if (timing.DecelerationRatio != 0.0)
			result += " DecelerationRatio=\""
				+ FormatDouble(timing.DecelerationRatio) + "\"";
		if (timing.AutoReverse)
			result += " AutoReverse=\"true\"";
		if (timing.RepeatBehavior == DeclarativeRepeatBehaviorKind::Forever)
			result += " RepeatBehavior=\"Forever\"";
		else if (timing.RepeatBehavior == DeclarativeRepeatBehaviorKind::Duration)
			result += " RepeatBehavior=\"" + TimeSpanText(
				timing.RepeatDurationMilliseconds) + "\"";
		else if (timing.RepeatCount != 1.0)
			result += " RepeatBehavior=\"" + FormatDouble(
				timing.RepeatCount) + "x\"";
		return result;
	}

	std::string BuildBrushLeafObjectGraphXaml(
		const AnimationFixture& fixture,
		const BrushLeafProbeSpec& spec)
	{
		std::string brush;
		switch (spec.Value)
		{
		case BrushLeafProbeValue::SolidColor:
			brush = "<SolidColorBrush Color=\""
				+ fixture.Target.BaseColorText + "\" Opacity=\"0.91\" />";
			break;
		case BrushLeafProbeValue::Opacity:
			brush = "<SolidColorBrush Color=\"#FF2864A0\" Opacity=\""
				+ FormatDouble(fixture.Target.BaseValue) + "\" />";
			break;
		case BrushLeafProbeValue::StartPointX:
			brush = "<LinearGradientBrush StartPoint=\""
				+ FormatDouble(fixture.Target.BaseValue)
				+ ",0.27\" EndPoint=\"0.74,0.86\">"
				"<GradientStop Color=\"#FF103050\" Offset=\"0\" />"
				"<GradientStop Color=\"#FFB0D0F0\" Offset=\"1\" />"
				"</LinearGradientBrush>";
			break;
		case BrushLeafProbeValue::EndPointY:
			brush = "<LinearGradientBrush StartPoint=\"0.13,0.27\" EndPoint=\"0.74,"
				+ FormatDouble(fixture.Target.BaseValue) + "\">"
				"<GradientStop Color=\"#FF103050\" Offset=\"0\" />"
				"<GradientStop Color=\"#FFB0D0F0\" Offset=\"1\" />"
				"</LinearGradientBrush>";
			break;
		case BrushLeafProbeValue::CenterX:
		case BrushLeafProbeValue::GradientOriginY:
		case BrushLeafProbeValue::RadiusX:
		case BrushLeafProbeValue::RadiusY:
		{
			const auto centerX = spec.Value == BrushLeafProbeValue::CenterX
				? fixture.Target.BaseValue : 0.43;
			const auto originY = spec.Value == BrushLeafProbeValue::GradientOriginY
				? fixture.Target.BaseValue : 0.69;
			const auto radiusX = spec.Value == BrushLeafProbeValue::RadiusX
				? fixture.Target.BaseValue : 0.81;
			const auto radiusY = spec.Value == BrushLeafProbeValue::RadiusY
				? fixture.Target.BaseValue : 0.63;
			brush = "<RadialGradientBrush Center=\"" + FormatDouble(centerX)
				+ ",0.57\" GradientOrigin=\"0.31," + FormatDouble(originY)
				+ "\" RadiusX=\"" + FormatDouble(radiusX)
				+ "\" RadiusY=\"" + FormatDouble(radiusY) + "\">"
				"<GradientStop Color=\"#FF182838\" Offset=\"0\" />"
				"<GradientStop Color=\"#FFD8C8B8\" Offset=\"1\" />"
				"</RadialGradientBrush>";
			break;
		}
		case BrushLeafProbeValue::GradientStopColor:
			brush = "<LinearGradientBrush StartPoint=\"0.1,0.2\" EndPoint=\"0.9,0.8\">"
				"<GradientStop Color=\"" + fixture.Target.BaseColorText
				+ "\" Offset=\"0.2\" />"
				"<GradientStop Color=\"#FFC0D0E0\" Offset=\"0.9\" />"
				"</LinearGradientBrush>";
			break;
		case BrushLeafProbeValue::GradientStopOffset:
			brush = "<RadialGradientBrush Center=\"0.45,0.55\" "
				"GradientOrigin=\"0.35,0.65\" RadiusX=\"0.75\" RadiusY=\"0.65\">"
				"<GradientStop Color=\"#FF204060\" Offset=\"0.1\" />"
				"<GradientStop Color=\"#FFB07030\" Offset=\""
				+ FormatDouble(fixture.Target.BaseValue) + "\" />"
				"</RadialGradientBrush>";
			break;
		default:
			throw std::runtime_error("CUI Brush leaf object graph is invalid.");
		}
		return spec.RootProperty == L"BorderBrush"
			? "\n\t\t\t<Border.BorderBrush>" + brush
				+ "</Border.BorderBrush>"
			: "\n\t\t\t<Control.Background>" + brush
				+ "</Control.Background>";
	}

	std::string BuildGeometryLeafObjectGraphXaml(
		const AnimationFixture& fixture,
		const GeometryLeafProbeSpec& spec)
	{
		std::string leaf;
		switch (spec.Graph)
		{
		case GeometryLeafProbeGraph::RectangleDirect:
		case GeometryLeafProbeGraph::RectangleChild0:
		{
			const auto rect = spec.Value == GeometryLeafProbeValue::RectangleRect
				? fixture.Target.BaseRectText : "1,2,30,40";
			const auto radiusX = spec.Value == GeometryLeafProbeValue::RectangleRadiusX
				? fixture.Target.BaseValue : 3.5;
			const auto radiusY = spec.Value == GeometryLeafProbeValue::RectangleRadiusY
				? fixture.Target.BaseValue : 4.5;
			leaf = "<RectangleGeometry Rect=\"" + rect + "\" RadiusX=\""
				+ FormatDouble(radiusX) + "\" RadiusY=\""
				+ FormatDouble(radiusY) + "\" />";
			break;
		}
		case GeometryLeafProbeGraph::EllipseDirect:
		case GeometryLeafProbeGraph::EllipseChild1:
		case GeometryLeafProbeGraph::EllipseChild10:
		{
			const auto centerX = spec.Value == GeometryLeafProbeValue::EllipseCenterX
				? fixture.Target.BaseValue : 20.5;
			const auto radiusX = spec.Value == GeometryLeafProbeValue::EllipseRadiusX
				? fixture.Target.BaseValue : 8.5;
			const auto radiusY = spec.Value == GeometryLeafProbeValue::EllipseRadiusY
				? fixture.Target.BaseValue : 9.5;
			leaf = "<EllipseGeometry Center=\"" + FormatDouble(centerX)
				+ ",30.5\" RadiusX=\"" + FormatDouble(radiusX)
				+ "\" RadiusY=\"" + FormatDouble(radiusY) + "\" />";
			break;
		}
		case GeometryLeafProbeGraph::GroupDirect:
			leaf = "<GeometryGroup FillRule=\"" + *fixture.Target.BaseString
				+ "\"><RectangleGeometry Rect=\"0,0,10,10\" />"
				"<EllipseGeometry Center=\"20,20\" RadiusX=\"5\" RadiusY=\"6\" />"
				"</GeometryGroup>";
			break;
		default:
		{
			const auto startX = spec.Value == GeometryLeafProbeValue::FigureStartPointX
				? fixture.Target.BaseValue : 1.0;
			const auto closed = spec.Value == GeometryLeafProbeValue::FigureIsClosed
				? *fixture.Target.BaseBoolean : false;
			const auto filled = spec.Value == GeometryLeafProbeValue::FigureIsFilled
				? *fixture.Target.BaseBoolean : true;
			const auto lineY = spec.Value == GeometryLeafProbeValue::LinePointY
				? fixture.Target.BaseValue : 12.0;
			const auto bezier1X = spec.Value == GeometryLeafProbeValue::BezierPoint1X
				? fixture.Target.BaseValue : 21.0;
			const auto bezier2Y = spec.Value == GeometryLeafProbeValue::BezierPoint2Y
				? fixture.Target.BaseValue : 24.0;
			const auto bezier3X = spec.Value == GeometryLeafProbeValue::BezierPoint3X
				? fixture.Target.BaseValue : 25.0;
			const auto quadratic1Y = spec.Value == GeometryLeafProbeValue::QuadraticPoint1Y
				? fixture.Target.BaseValue : 31.0;
			const auto quadratic2X = spec.Value == GeometryLeafProbeValue::QuadraticPoint2X
				? fixture.Target.BaseValue : 33.0;
			const auto arcY = spec.Value == GeometryLeafProbeValue::ArcPointY
				? fixture.Target.BaseValue : 41.0;
			const auto arcSize = spec.Value == GeometryLeafProbeValue::ArcSize
				? fixture.Target.BaseSizeText : "11,12";
			const auto rotation = spec.Value == GeometryLeafProbeValue::ArcRotationAngle
				? fixture.Target.BaseValue : 25.0;
			const auto large = spec.Value == GeometryLeafProbeValue::ArcIsLargeArc
				? *fixture.Target.BaseBoolean : false;
			const auto sweep = spec.Value == GeometryLeafProbeValue::ArcSweepDirection
				? *fixture.Target.BaseString : "Counterclockwise";
			const auto fillRule = spec.Value == GeometryLeafProbeValue::FillRule
				? *fixture.Target.BaseString : "Nonzero";
			leaf = "<PathGeometry FillRule=\"" + fillRule + "\"><PathFigure StartPoint=\""
				+ FormatDouble(startX) + ",2\" IsClosed=\""
				+ std::string(closed ? "true" : "false") + "\" IsFilled=\""
				+ std::string(filled ? "true" : "false") + "\">"
				+ "<LineSegment Point=\"10," + FormatDouble(lineY) + "\" />"
				+ "<BezierSegment Point1=\"" + FormatDouble(bezier1X)
				+ ",22\" Point2=\"23," + FormatDouble(bezier2Y)
				+ "\" Point3=\"" + FormatDouble(bezier3X) + ",26\" />"
				+ "<QuadraticBezierSegment Point1=\"30," + FormatDouble(quadratic1Y)
				+ "\" Point2=\"" + FormatDouble(quadratic2X) + ",34\" />"
				+ "<ArcSegment Point=\"40," + FormatDouble(arcY)
				+ "\" Size=\"" + arcSize + "\" RotationAngle=\""
				+ FormatDouble(rotation) + "\" IsLargeArc=\""
				+ std::string(large ? "true" : "false")
				+ "\" SweepDirection=\"" + sweep + "\" />"
				+ "</PathFigure></PathGeometry>";
			break;
		}
		}
		auto wrapAt = [](std::string current,
			std::initializer_list<unsigned> indices)
		{
			std::vector<unsigned> values(indices);
			for (auto item = values.rbegin(); item != values.rend(); ++item)
			{
				std::string children;
				for (unsigned index = 0; index < *item; ++index)
					children += "<RectangleGeometry Rect=\""
						+ std::to_string(index) + "," + std::to_string(index)
						+ ",3,4\" />";
				current = "<GeometryGroup>" + children + current + "</GeometryGroup>";
			}
			return current;
		};
		switch (spec.Graph)
		{
		case GeometryLeafProbeGraph::RectangleChild0:
			leaf = wrapAt(std::move(leaf), { 0u }); break;
		case GeometryLeafProbeGraph::EllipseChild1:
		case GeometryLeafProbeGraph::PathChild1:
			leaf = wrapAt(std::move(leaf), { 1u }); break;
		case GeometryLeafProbeGraph::EllipseChild10:
		case GeometryLeafProbeGraph::PathChild10:
			leaf = wrapAt(std::move(leaf), { 1u, 0u }); break;
		default:
			break;
		}
		return "\n\t\t\t<Control.Clip>" + leaf + "</Control.Clip>";
	}

	std::string BuildTransformLeafObjectGraphXaml(
		const AnimationFixture& fixture,
		const TransformLeafProbeSpec& spec)
	{
		const auto base = FormatDouble(fixture.Target.BaseValue);
		std::string operation;
		switch (spec.TransformKind)
		{
		case cui::drawing::TransformKind::Translate:
			operation = "<TranslateTransform X=\""
				+ std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformX
						? base : "2.25")
				+ "\" Y=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformY
						? base : "-3.25") + "\" />";
			break;
		case cui::drawing::TransformKind::Scale:
			operation = "<ScaleTransform ScaleX=\"1.25\" ScaleY=\""
				+ std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformScaleY
						? base : "0.75")
				+ "\" CenterX=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformCenterX
						? base : "0.5")
				+ "\" CenterY=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformCenterY
						? base : "0.25") + "\" />";
			break;
		case cui::drawing::TransformKind::Rotate:
			operation = "<RotateTransform Angle=\"17.5\" CenterX=\""
				+ std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformCenterX
						? base : "0.5")
				+ "\" CenterY=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformCenterY
						? base : "-0.75") + "\" />";
			break;
		case cui::drawing::TransformKind::Skew:
			operation = "<SkewTransform AngleX=\""
				+ std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformAngleX
						? base : "12.5")
				+ "\" AngleY=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformAngleY
						? base : "-7.25")
				+ "\" CenterX=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformCenterX
						? base : "0.5")
				+ "\" CenterY=\"" + std::string(spec.Member
					== CompiledStoryboardObjectPathMember::TransformCenterY
						? base : "-0.25") + "\" />";
			break;
		case cui::drawing::TransformKind::Matrix:
			operation = "<MatrixTransform Matrix=\""
				+ fixture.Target.BaseMatrixText + "\" />";
			break;
		default:
			throw std::runtime_error(
				"CUI Transform leaf XAML graph has an invalid kind.");
		}

		std::string transform = operation;
		if (!spec.Direct)
		{
			transform = "<TransformGroup>";
			for (uint32_t index = 0; index < spec.OperationIndex; ++index)
				transform += "<TranslateTransform X=\""
					+ std::to_string(100u + index) + "\" Y=\"-"
					+ std::to_string(100u + index) + "\" />";
			transform += operation + "</TransformGroup>";
		}

		switch (spec.Root)
		{
		case TransformLeafProbeRoot::RenderTransform:
			return "\n\t\t\t<Control.RenderTransform>" + transform
				+ "</Control.RenderTransform>";
		case TransformLeafProbeRoot::GeometryTransformDirect:
		case TransformLeafProbeRoot::GeometryTransformChild10:
		{
			std::string geometry = "<RectangleGeometry Rect=\"0,0,100,60\">"
				"<Geometry.Transform>" + transform
				+ "</Geometry.Transform></RectangleGeometry>";
			if (spec.Root == TransformLeafProbeRoot::GeometryTransformChild10)
				geometry = "<GeometryGroup><GeometryGroup.Children>"
					"<RectangleGeometry Rect=\"0,0,3,4\" />"
					"<GeometryGroup><GeometryGroup.Children>" + geometry
					+ "</GeometryGroup.Children></GeometryGroup>"
					"</GeometryGroup.Children></GeometryGroup>";
			return "\n\t\t\t<Control.Clip>" + geometry + "</Control.Clip>";
		}
		case TransformLeafProbeRoot::BrushTransform:
			return "\n\t\t\t<Control.Background><SolidColorBrush Color=\"#FF336699\">"
				"<Brush.Transform>" + transform
				+ "</Brush.Transform></SolidColorBrush></Control.Background>";
		case TransformLeafProbeRoot::BrushRelativeTransform:
			return "\n\t\t\t<Control.Background><LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"1,1\">"
				"<Brush.RelativeTransform>" + transform
				+ "</Brush.RelativeTransform><GradientStop Color=\"#FF102030\" Offset=\"0\" />"
				"<GradientStop Color=\"#FFD0E0F0\" Offset=\"1\" />"
				"</LinearGradientBrush></Control.Background>";
		default:
			throw std::runtime_error(
				"CUI Transform leaf XAML graph has an invalid root.");
		}
	}

	std::string BuildRuntimeXaml(const AnimationFixture& fixture)
	{
		const bool metadataDouble = fixture.Target.Probe == "metadata-double";
		const bool metadataInt32 = fixture.Target.Probe == "metadata-int32";
		const bool metadataInt64 = fixture.Target.Probe == "metadata-int64";
		const bool metadataSingle = fixture.Target.Probe == "metadata-single";
		const bool metadataBoolean = fixture.Target.Probe == "metadata-boolean";
		const bool metadataString = fixture.Target.Probe == "metadata-string";
		const bool metadataColor = fixture.Target.Probe == "metadata-color";
		const bool metadataVector = fixture.Target.Probe == "metadata-vector";
		const bool metadataRect = fixture.Target.Probe == "metadata-rect";
		const bool metadataSize = fixture.Target.Probe == "metadata-size";
		const bool metadataMatrix = fixture.Target.Probe == "metadata-matrix";
		const bool pointProjection = fixture.Target.Probe == "render-transform-origin-x"
			|| fixture.Target.Probe == "render-transform-origin-y";
		const bool thicknessProjection =
			fixture.Target.Probe == "border-thickness-left";
		const bool geometryTransformDirect =
			fixture.Target.Probe == "geometry-transform-direct-x";
		const bool brushTransformDirect =
			fixture.Target.Probe == "brush-transform-direct-angle";
		const bool brushRelativeTransformDirect =
			fixture.Target.Probe == "brush-relative-transform-direct-scale-x";
		const auto* transformLeaf = FindTransformLeafProbe(fixture.Target.Probe);
		const auto* brushLeaf = FindBrushLeafProbe(fixture.Target.Probe);
		const auto* geometryLeaf = FindGeometryLeafProbe(fixture.Target.Probe);
		const bool objectPathBorder = brushRelativeTransformDirect
			|| (transformLeaf && transformLeaf->BorderTarget)
			|| (brushLeaf && brushLeaf->BorderTarget);
		const bool styleResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "style-document";
		const bool visualStateResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-state-document";
		const bool visualTransitionResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-transition-document";
		const bool documentResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "document-late-target";
		const bool documentStoryboardResource =
			documentResource || styleResource || visualStateResource
			|| visualTransitionResource;
		const auto rootTimingAttributes = StoryboardTimingAttributes(fixture);
		std::string result = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  xmlns:local="urn:cui:animation-conformance" x:Name="AnimationFixtureWindow">
  <Window.Resources>)XAML";
		if (documentStoryboardResource)
			result += R"XAML(
    <Storyboard x:Key=")XAML" + *fixture.StoryboardResourceKey + "\""
				+ rootTimingAttributes + ">" + fixture.TimelineXaml
				+ R"XAML(</Storyboard>)XAML";
		result += R"XAML(
    <ComponentDefinition x:Key="local:AnimationFixtureHost" BaseType="Canvas">
      <ComponentDefinition.Properties>
        <ComponentProperty Name="Phase" Type="Int" Default="0" />)XAML";
		if (metadataDouble)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Double\" Default=\""
				+ FormatDouble(fixture.Target.BaseValue) + "\" />";
		else if (metadataInt32)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Int\" Default=\""
				+ std::to_string(*fixture.Target.BaseInt32) + "\" />";
		else if (metadataInt64)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Int64\" Default=\""
				+ std::to_string(*fixture.Target.BaseInt64) + "\" />";
		else if (metadataSingle)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Float\" Default=\""
				+ FormatDouble(*fixture.Target.BaseSingle) + "\" />";
		else if (metadataBoolean)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Bool\" Default=\""
				+ std::string(*fixture.Target.BaseBoolean ? "true" : "false") + "\" />";
		else if (metadataString)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"String\" Default=\""
				+ EscapeXmlAttribute(*fixture.Target.BaseString) + "\" />";
		else if (metadataColor)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Color\" Default=\""
				+ fixture.Target.BaseColorText + "\" />";
		else if (metadataVector)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Vector\" Default=\""
				+ fixture.Target.BaseVectorText + "\" />";
		else if (metadataRect)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Rect\" Default=\""
				+ fixture.Target.BaseRectText + "\" />";
		else if (metadataSize)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Size\" Default=\""
				+ fixture.Target.BaseSizeText + "\" />";
		else if (metadataMatrix)
			result += "\n        <ComponentProperty Name=\"Value\" Type=\"Matrix\" Default=\""
				+ fixture.Target.BaseMatrixText + "\" />";
		result += "\n      </ComponentDefinition.Properties>";
		if (fixture.ReplacementTimelineXaml)
			result += R"XAML(
		<ComponentDefinition.Events>
		  <ComponentEvent Name="BeginPrimary" RoutingStrategy="Direct" />
		  <ComponentEvent Name="BeginReplacement" RoutingStrategy="Direct" />
		</ComponentDefinition.Events>)XAML";
		result += documentResource
			? "\n\t\t<ComponentDefinition.Template>\n\t\t  <Canvas x:Name=\"fixtureRoot\""
			: thicknessProjection || objectPathBorder
			? "\n\t\t<ComponentDefinition.Template>\n\t\t  <Border x:Name=\"target\""
			: "\n\t\t<ComponentDefinition.Template>\n\t\t  <Canvas x:Name=\"target\"";
		if (pointProjection)
		{
			const auto base = FormatDouble(fixture.Target.BaseValue);
			result += " RenderTransformOrigin=\"" + base + "," + base + "\"";
		}
		else if (thicknessProjection)
		{
			const auto base = FormatDouble(fixture.Target.BaseValue);
			result += " BorderThickness=\"" + base + "," + base + ","
				+ base + "," + base + "\"";
		}
		else if (!documentResource
			&& !metadataDouble && !metadataInt32 && !metadataInt64
			&& !metadataSingle && !metadataBoolean && !metadataString && !metadataColor
			&& !metadataVector && !metadataRect
			&& !metadataSize && !metadataMatrix
			&& !geometryTransformDirect && !brushTransformDirect
			&& !brushRelativeTransformDirect && !transformLeaf
			&& !brushLeaf && !geometryLeaf)
			result += " Canvas.Left=\"" + FormatDouble(fixture.Target.BaseValue) + "\"";
		result += ">";
		if (transformLeaf)
			result += BuildTransformLeafObjectGraphXaml(fixture, *transformLeaf);
		else if (geometryTransformDirect)
			result += R"XAML(
			<Control.Clip>
			  <RectangleGeometry Rect="0,0,100,60">
				<Geometry.Transform><TranslateTransform X=")XAML"
				+ FormatDouble(fixture.Target.BaseValue)
				+ R"XAML(" Y="-3.25" /></Geometry.Transform>
			  </RectangleGeometry>
			</Control.Clip>)XAML";
		else if (brushTransformDirect)
			result += R"XAML(
			<Control.Background>
			  <SolidColorBrush Color="#FF336699">
				<Brush.Transform><RotateTransform Angle=")XAML"
				+ FormatDouble(fixture.Target.BaseValue)
				+ R"XAML(" CenterX="0.25" CenterY="0.75" /></Brush.Transform>
			  </SolidColorBrush>
			</Control.Background>)XAML";
		else if (brushRelativeTransformDirect)
			result += R"XAML(
			<Control.Background>
			  <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
				<Brush.RelativeTransform><ScaleTransform ScaleX=")XAML"
				+ FormatDouble(fixture.Target.BaseValue)
				+ R"XAML(" ScaleY="0.75" CenterX="0.5" CenterY="0.25" /></Brush.RelativeTransform>
				<GradientStop Color="#FF102030" Offset="0" />
				<GradientStop Color="#FFD0E0F0" Offset="1" />
			  </LinearGradientBrush>
			</Control.Background>)XAML";
		else if (brushLeaf)
			result += BuildBrushLeafObjectGraphXaml(fixture, *brushLeaf);
		else if (geometryLeaf)
			result += BuildGeometryLeafObjectGraphXaml(fixture, *geometryLeaf);
		if (fixture.StoryboardResourceKey && !documentStoryboardResource)
			result += R"XAML(
			<Canvas.Resources>
			  <Storyboard x:Key=")XAML" + *fixture.StoryboardResourceKey + "\""
				+ rootTimingAttributes + ">" + fixture.TimelineXaml + R"XAML(</Storyboard>
			</Canvas.Resources>)XAML";
		if (fixture.ReplacementTimelineXaml)
		{
			if (fixture.StoryboardResourceKey)
				result += R"XAML(
			<Canvas.Triggers>
			  <EventTrigger RoutedEvent="BeginPrimary">
				<BeginStoryboard x:Name="PrimaryClock" Storyboard="{StaticResource )XAML"
					+ *fixture.StoryboardResourceKey + R"XAML(}" />
			  </EventTrigger>
			  <EventTrigger RoutedEvent="BeginReplacement">
				<BeginStoryboard x:Name="ReplacementClock" Storyboard="{StaticResource )XAML"
					+ *fixture.StoryboardResourceKey + R"XAML(}" />
			  </EventTrigger>
			</Canvas.Triggers>)XAML";
			else result += thicknessProjection ? R"XAML(
			<Border.Triggers>
			  <EventTrigger RoutedEvent="BeginPrimary">
				<BeginStoryboard x:Name="PrimaryClock"><Storyboard)XAML"
				+ rootTimingAttributes + ">"
				+ fixture.TimelineXaml + R"XAML(</Storyboard></BeginStoryboard>
			  </EventTrigger>
			  <EventTrigger RoutedEvent="BeginReplacement">
				<BeginStoryboard x:Name="ReplacementClock")XAML"
				+ std::string(fixture.Oracle == "compose-handoff"
					? " HandoffBehavior=\"Compose\"" : "")
				+ R"XAML(><Storyboard>)XAML"
				+ *fixture.ReplacementTimelineXaml + R"XAML(</Storyboard></BeginStoryboard>
			  </EventTrigger>
			</Border.Triggers>)XAML" : R"XAML(
			<Canvas.Triggers>
			  <EventTrigger RoutedEvent="BeginPrimary">
				<BeginStoryboard x:Name="PrimaryClock"><Storyboard)XAML"
				+ rootTimingAttributes + ">"
				+ fixture.TimelineXaml + R"XAML(</Storyboard></BeginStoryboard>
			  </EventTrigger>
			  <EventTrigger RoutedEvent="BeginReplacement">
				<BeginStoryboard x:Name="ReplacementClock")XAML"
				+ std::string(fixture.Oracle == "compose-handoff"
					? " HandoffBehavior=\"Compose\"" : "")
				+ R"XAML(><Storyboard>)XAML"
				+ *fixture.ReplacementTimelineXaml + R"XAML(</Storyboard></BeginStoryboard>
			  </EventTrigger>
			</Canvas.Triggers>)XAML";
		}
		else if (!styleResource)
		{
			result += R"XAML(
		  <VisualStateManager.VisualStateGroups>
			<VisualStateGroup x:Name="FixtureStates">)XAML";
			if (visualTransitionResource)
				result += R"XAML(
              <VisualStateGroup.Transitions>
                <VisualTransition From="Idle" To="Running"
                  Storyboard="{StaticResource )XAML"
					+ *fixture.StoryboardResourceKey + R"XAML(}" />
              </VisualStateGroup.Transitions>)XAML";
			result += R"XAML(
              <VisualState x:Name="Idle" />
              <VisualState x:Name="Running")XAML";
			if (visualStateResource)
				result += R"XAML( Storyboard="{StaticResource )XAML"
					+ *fixture.StoryboardResourceKey + R"XAML(}")XAML";
			result += R"XAML(>
                <VisualState.StateTriggers>
                  <StateTrigger Property="Phase" Value="1" />
				</VisualState.StateTriggers>)XAML";
			if (!visualStateResource && !visualTransitionResource)
				result += R"XAML(
				<VisualState.Storyboard>
				  <Storyboard)XAML"
					+ rootTimingAttributes + ">"
					+ fixture.TimelineXaml + R"XAML(</Storyboard>
				</VisualState.Storyboard>
				)XAML";
			result += R"XAML(
			  </VisualState>
			</VisualStateGroup>
		  </VisualStateManager.VisualStateGroups>)XAML";
		}
		if (documentResource)
			result += "\n\t\t  <Canvas x:Name=\"target\" Canvas.Left=\""
				+ FormatDouble(fixture.Target.BaseValue) + "\" />";
		result += thicknessProjection || objectPathBorder ? R"XAML(
		</Border>
      </ComponentDefinition.Template>
    </ComponentDefinition>)XAML" : R"XAML(
		</Canvas>
      </ComponentDefinition.Template>
    </ComponentDefinition>)XAML";
		if (styleResource)
			result += R"XAML(
    <Style x:Key="AnimationFixtureStyle"
           TargetType="local:AnimationFixtureHost">
      <Style.Triggers>
        <Trigger Property="Phase" Value="1">
          <Trigger.EnterActions>
            <BeginStoryboard x:Name="StyleClock" Storyboard="{StaticResource )XAML"
				+ *fixture.StoryboardResourceKey + R"XAML(}" />
          </Trigger.EnterActions>
          <Trigger.ExitActions>
            <StopStoryboard BeginStoryboardName="StyleClock" />
          </Trigger.ExitActions>
        </Trigger>
      </Style.Triggers>
    </Style>)XAML";
		result += R"XAML(
  </Window.Resources>
  <local:AnimationFixtureHost x:Name="fixtureHost")XAML";
		if (styleResource)
			result += R"XAML( Style="{StaticResource AnimationFixtureStyle}")XAML";
		result += R"XAML( />
</Window>)XAML";
		return result;
	}

	SampleResult ObserveDesignSample(
		const AnimationFixture& fixture,
		const AnimationSampleRequest& sample,
		Control& host,
		Control& templateTarget,
		std::optional<DeclarativeClockObservation> clock = std::nullopt,
		std::vector<std::string> events = {})
	{
		Control* observedTarget = &templateTarget;
		std::wstring propertyName = L"Canvas.Left";
		double value = static_cast<double>(Canvas::GetLeft(templateTarget));
		std::optional<Thickness> thicknessValue;
		std::optional<D2D1_COLOR_F> colorValue;
		std::optional<cui::core::Vector> vectorValue;
		std::optional<cui::core::Rect> rectValue;
		std::optional<cui::core::Size> sizeValue;
		std::optional<D2D1_MATRIX_3X2_F> matrixValue;
		std::optional<int> int32Value;
		std::optional<long long> int64Value;
		std::optional<float> singleValue;
		std::optional<bool> booleanValue;
		std::optional<std::string> stringValue;
		const auto* transformLeaf = FindTransformLeafProbe(fixture.Target.Probe);
		const auto* brushLeaf = FindBrushLeafProbe(fixture.Target.Probe);
		const auto* geometryLeaf = FindGeometryLeafProbe(fixture.Target.Probe);
		if (fixture.Target.Probe == "metadata-double")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(value))
				throw std::runtime_error(
					"CUI metadata-double probe is not readable as Double.");
		}
		else if (fixture.Target.Probe == "metadata-int32")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			int typed = 0;
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(typed))
				throw std::runtime_error(
					"CUI metadata-int32 probe is not readable as Int32.");
			int32Value = typed;
			value = static_cast<double>(typed);
		}
		else if (fixture.Target.Probe == "metadata-int64")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			long long typed = 0;
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(typed))
				throw std::runtime_error(
					"CUI metadata-int64 probe is not readable as Int64.");
			int64Value = typed;
			value = static_cast<double>(typed);
		}
		else if (fixture.Target.Probe == "metadata-single")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			float typed = 0.0f;
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(typed))
				throw std::runtime_error(
					"CUI metadata-single probe is not readable as Single.");
			singleValue = typed;
			value = static_cast<double>(typed);
		}
		else if (fixture.Target.Probe == "metadata-boolean")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			bool typed = false;
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(typed))
				throw std::runtime_error(
					"CUI metadata-boolean probe is not readable as Boolean.");
			booleanValue = typed;
			value = typed ? 1.0 : 0.0;
		}
		else if (fixture.Target.Probe == "metadata-string")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			std::wstring typed;
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(typed))
				throw std::runtime_error(
					"CUI metadata-string probe is not readable as String.");
			stringValue = Convert::UnicodeToUtf8(typed);
			value = static_cast<double>(typed.size());
		}
		else if (fixture.Target.Probe == "metadata-color")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			D2D1_COLOR_F color{};
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(color))
				throw std::runtime_error(
					"CUI metadata-color probe is not readable as Color.");
			colorValue = color;
			value = static_cast<double>(color.r);
		}
		else if (fixture.Target.Probe == "metadata-vector")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			cui::core::Vector vector{};
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(vector))
				throw std::runtime_error(
					"CUI metadata-vector probe is not readable as Vector.");
			vectorValue = vector;
			value = static_cast<double>(vector.x);
		}
		else if (fixture.Target.Probe == "metadata-rect")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			cui::core::Rect rect{};
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(rect))
				throw std::runtime_error(
					"CUI metadata-rect probe is not readable as Rect.");
			rectValue = rect;
			value = static_cast<double>(rect.x);
		}
		else if (fixture.Target.Probe == "metadata-size")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			cui::core::Size size{};
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(size))
				throw std::runtime_error(
					"CUI metadata-size probe is not readable as Size.");
			sizeValue = size;
			value = static_cast<double>(size.width);
		}
		else if (fixture.Target.Probe == "metadata-matrix")
		{
			observedTarget = &host;
			propertyName = L"Value";
			BindingValue observed;
			D2D1_MATRIX_3X2_F matrix{};
			if (!host.TryGetPropertyValue(propertyName, observed)
				|| !observed.TryGet(matrix))
				throw std::runtime_error(
					"CUI metadata-matrix probe is not readable as Matrix.");
			matrixValue = matrix;
			value = static_cast<double>(matrix._11);
		}
		else if (fixture.Target.Probe == "render-transform-origin-x"
			|| fixture.Target.Probe == "render-transform-origin-y")
		{
			propertyName = L"RenderTransformOrigin";
			const auto point = templateTarget.GetRenderTransformOriginDip();
			value = static_cast<double>(
				fixture.Target.Probe == "render-transform-origin-y"
					? point.y : point.x);
		}
		else if (fixture.Target.Probe == "border-thickness-left")
		{
			auto* border = dynamic_cast<Border*>(&templateTarget);
			if (!border)
				throw std::runtime_error(
					"CUI Thickness probe target is not a Border.");
			propertyName = L"BorderThickness";
			thicknessValue = border->BorderThickness;
			value = static_cast<double>(thicknessValue->Left);
		}
		else if (transformLeaf)
		{
			std::optional<D2D1_MATRIX_3X2_F> observedMatrix;
			switch (transformLeaf->Root)
			{
			case TransformLeafProbeRoot::RenderTransform:
			{
				propertyName = L"RenderTransform";
				const auto& transform = templateTarget.GetRenderTransform();
				if (!transform) throw std::runtime_error(
					"CUI RenderTransform leaf graph is missing.");
				value = ReadTransformLeafProbe(
					*transform, *transformLeaf, &observedMatrix);
				break;
			}
			case TransformLeafProbeRoot::GeometryTransformDirect:
			case TransformLeafProbeRoot::GeometryTransformChild10:
			{
				propertyName = L"Clip";
				const auto& clip = templateTarget.GetClip();
				if (!clip) throw std::runtime_error(
					"CUI Geometry Transform leaf graph is missing.");
				const cui::drawing::Geometry* geometry = &*clip;
				for (const auto index : TransformLeafChildIndices(*transformLeaf))
				{
					if (geometry->Kind != cui::drawing::GeometryKind::Group
						|| index >= geometry->Children.size())
						throw std::runtime_error(
							"CUI Geometry Transform child range is invalid.");
					geometry = &geometry->Children[index];
				}
				if (!geometry->LocalTransform) throw std::runtime_error(
					"CUI Geometry Transform leaf is missing.");
				value = ReadTransformLeafProbe(
					*geometry->LocalTransform, *transformLeaf, &observedMatrix);
				break;
			}
			case TransformLeafProbeRoot::BrushTransform:
			case TransformLeafProbeRoot::BrushRelativeTransform:
			{
				propertyName = L"Background";
				const auto brush = templateTarget.GetComputedBackgroundBrush();
				const auto& transform = transformLeaf->Root
					== TransformLeafProbeRoot::BrushRelativeTransform
					? brush.RelativeTransform : brush.Transform;
				if (!transform) throw std::runtime_error(
					"CUI Brush Transform leaf graph is missing.");
				value = ReadTransformLeafProbe(
					*transform, *transformLeaf, &observedMatrix);
				break;
			}
			default:
				throw std::runtime_error("CUI Transform leaf root is invalid.");
			}
			matrixValue = observedMatrix;
		}
		else if (fixture.Target.Probe == "geometry-transform-direct-x")
		{
			propertyName = L"Clip";
			const auto& clip = templateTarget.GetClip();
			if (!clip || !clip->LocalTransform
				|| clip->LocalTransform->Operations.size() != 1
				|| clip->LocalTransform->Operations[0].Kind
					!= cui::drawing::TransformKind::Translate)
				throw std::runtime_error(
					"CUI direct Geometry.Transform probe has an invalid object graph.");
			value = static_cast<double>(clip->LocalTransform->Operations[0].X);
		}
		else if (fixture.Target.Probe == "brush-transform-direct-angle")
		{
			propertyName = L"Background";
			const auto brush = templateTarget.GetComputedBackgroundBrush();
			if (!brush.Transform || brush.Transform->Operations.size() != 1
				|| brush.Transform->Operations[0].Kind
					!= cui::drawing::TransformKind::Rotate)
				throw std::runtime_error(
					"CUI direct Brush.Transform probe has an invalid object graph.");
			value = static_cast<double>(brush.Transform->Operations[0].Angle);
		}
		else if (fixture.Target.Probe
			== "brush-relative-transform-direct-scale-x")
		{
			propertyName = L"Background";
			const auto brush = templateTarget.GetComputedBackgroundBrush();
			if (!brush.RelativeTransform
				|| brush.RelativeTransform->Operations.size() != 1
				|| brush.RelativeTransform->Operations[0].Kind
					!= cui::drawing::TransformKind::Scale)
				throw std::runtime_error(
					"CUI direct Brush.RelativeTransform probe has an invalid object graph.");
			value = static_cast<double>(
				brush.RelativeTransform->Operations[0].ScaleX);
		}
		else if (brushLeaf)
		{
			propertyName = std::wstring(brushLeaf->RootProperty);
			const auto brush = brushLeaf->RootProperty == L"BorderBrush"
				? templateTarget.GetComputedBorderBrush()
				: templateTarget.GetComputedBackgroundBrush();
			value = ReadBrushLeafProbe(brush, *brushLeaf, &colorValue);
		}
		else if (geometryLeaf)
		{
			propertyName = L"Clip";
			const auto& clip = templateTarget.GetClip();
			if (!clip)
				throw std::runtime_error(
					"CUI Geometry leaf probe has no Clip graph.");
			value = ReadGeometryLeafProbe(*clip, *geometryLeaf,
				&rectValue, &sizeValue, &booleanValue, &stringValue);
		}
		if (!std::isfinite(value))
			throw std::runtime_error(
				"CUI fixture produced a non-finite observed value.");
		return SampleResult{
			sample.AtMilliseconds,
			sample.Label,
			value,
			observedTarget->GetPropertyValueSource(propertyName)
				== DependencyPropertyValueSource::Animation,
			std::move(clock),
			std::move(events),
			std::move(thicknessValue),
			std::move(colorValue),
			std::move(vectorValue),
			std::move(rectValue),
			std::move(sizeValue),
			std::move(matrixValue),
			std::move(int32Value),
			std::move(int64Value),
			std::move(singleValue),
			std::move(booleanValue),
			std::move(stringValue) };
	}

	class ClockOverrideScope final
	{
	public:
		ClockOverrideScope(
			Control& owner, unsigned long long nowMilliseconds) noexcept
			: _owner(&owner),
			  _previous(cui::framework::PresentationAccess::
				  ExchangeVisualStateAnimationClockOverrideForTesting(
					  owner, nowMilliseconds))
		{
		}
		ClockOverrideScope(const ClockOverrideScope&) = delete;
		ClockOverrideScope& operator=(const ClockOverrideScope&) = delete;
		~ClockOverrideScope()
		{
			if (_owner)
				(void)cui::framework::PresentationAccess::
					ExchangeVisualStateAnimationClockOverrideForTesting(
						*_owner, _previous);
		}

		void Set(unsigned long long nowMilliseconds) noexcept
		{
			if (_owner)
				(void)cui::framework::PresentationAccess::
					ExchangeVisualStateAnimationClockOverrideForTesting(
						*_owner, nowMilliseconds);
		}

	private:
		Control* _owner = nullptr;
		std::optional<unsigned long long> _previous;
	};

	unsigned long long RequireOperationOffset(
		const AnimationOperation& operation,
		std::string_view context)
	{
		if (!operation.Value || *operation.Value < 0.0
			|| *operation.Value
				> static_cast<double>((std::numeric_limits<
					unsigned long long>::max)())
			|| std::floor(*operation.Value) != *operation.Value)
			throw std::runtime_error(std::string(context)
				+ " has an invalid whole-millisecond offset.");
		return static_cast<unsigned long long>(*operation.Value);
	}

	void ExecuteDispatcherControlOperation(
		const AnimationOperation& operation,
		Control& host,
		ClockOverrideScope& clockScope,
		std::string_view context)
	{
		const auto operationTick = FixtureClockOrigin
			+ operation.AtMilliseconds;
		clockScope.Set(operationTick);
		bool accepted = true;
		if (operation.Kind == "seek-aligned")
			accepted = cui::framework::PresentationAccess::
				SeekSingleVisualStateAnimationRootAlignedForTesting(
					host, RequireOperationOffset(operation, context));
		else if (operation.Kind == "seek")
			accepted = cui::framework::PresentationAccess::
				SeekSingleVisualStateAnimationRootForTesting(
					host, RequireOperationOffset(operation, context));
		else if (operation.Kind == "pause")
			accepted = cui::framework::PresentationAccess::
				PauseSingleVisualStateAnimationRootForTesting(host);
		else if (operation.Kind == "resume")
			accepted = cui::framework::PresentationAccess::
				ResumeSingleVisualStateAnimationRootForTesting(host);
		else if (operation.Kind == "stop")
			accepted = cui::framework::PresentationAccess::
				StopSingleVisualStateAnimationRootForTesting(host);
		else if (operation.Kind == "remove")
			accepted = cui::framework::PresentationAccess::
				RemoveSingleVisualStateAnimationRootForTesting(host);
		else if (operation.Kind == "set-speed-ratio")
			accepted = cui::framework::PresentationAccess::
				SetSpeedRatioSingleVisualStateAnimationRootForTesting(
					host, *operation.Value);
		else if (operation.Kind == "skip-to-fill")
			accepted = cui::framework::PresentationAccess::
				SkipSingleVisualStateAnimationRootToFillForTesting(host);
		else if (operation.Kind == "tick")
		{
			(void)cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(host, operationTick);
			if (cui::framework::PresentationAccess::
				VisualStateAnimationAdvanceFailedForTesting(host))
				throw std::runtime_error(std::string(context)
					+ " explicit tick failed transactionally.");
			return;
		}
		else
			throw std::runtime_error(std::string(context)
				+ " contains an unsupported dispatcher-control operation.");
		if (!accepted)
			throw std::runtime_error(std::string(context)
				+ " pending control operation was rejected.");
	}

	const char* TimingEventName(DeclarativeClockTimingEventKind kind)
	{
		switch (kind)
		{
		case DeclarativeClockTimingEventKind::CurrentTimeInvalidated:
			return "CurrentTimeInvalidated";
		case DeclarativeClockTimingEventKind::CurrentGlobalSpeedInvalidated:
			return "CurrentGlobalSpeedInvalidated";
		case DeclarativeClockTimingEventKind::CurrentStateInvalidated:
			return "CurrentStateInvalidated";
		case DeclarativeClockTimingEventKind::Completed:
			return "Completed";
		case DeclarativeClockTimingEventKind::RemoveRequested:
			return "RemoveRequested";
		}
		throw std::runtime_error("Unknown CUI timing event kind.");
	}

	class TimingEventCollector final
	{
	public:
		TimingEventCollector(Control& expectedSender,
			std::vector<std::string>& events) noexcept
			: ExpectedSender(&expectedSender), Events(&events) {}

		void Observe(Control* sender,
			const DeclarativeClockTimingEventArgs& args)
		{
			if (sender != ExpectedSender)
				throw std::runtime_error(
					"CUI timing event sender did not match its root owner.");
			if (args.ClockInstanceId == 0)
				throw std::runtime_error(
					"CUI timing event exposed a zero clock instance id.");
			if (args.OwnerKind != DeclarativeClockOwnerKind::VisualStateGroup)
				throw std::runtime_error(
					"CUI fixture timing event exposed the wrong owner kind.");
			if (Identity)
			{
				if (Identity->ClockInstanceId != args.ClockInstanceId
					|| Identity->OwnerKind != args.OwnerKind
					|| Identity->OwnerScopeId != args.OwnerScopeId
					|| Identity->StoryboardDefinitionId
						!= args.StoryboardDefinitionId)
					throw std::runtime_error(
						"CUI timing event identity changed within one sample.");
			}
			else Identity = args;
			Events->emplace_back(TimingEventName(args.Kind));
		}

		void Clear() noexcept
		{
			Events->clear();
			Identity.reset();
		}

	private:
		Control* ExpectedSender = nullptr;
		std::vector<std::string>* Events = nullptr;
		std::optional<DeclarativeClockTimingEventArgs> Identity;
	};

	void ExecuteDispatcherEventOperation(
		const AnimationOperation& operation,
		Control& host,
		ClockOverrideScope& clockScope,
		std::string_view context)
	{
		if (operation.Kind == "begin-tick")
		{
			const auto operationTick = FixtureClockOrigin
				+ operation.AtMilliseconds;
			clockScope.Set(operationTick);
			(void)cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(host, operationTick);
			if (cui::framework::PresentationAccess::
				VisualStateAnimationAdvanceFailedForTesting(host))
				throw std::runtime_error(std::string(context)
					+ " begin timing tick failed transactionally.");
			return;
		}
		ExecuteDispatcherControlOperation(
			operation, host, clockScope, context);
	}

	SampleResult ExecuteSample(
		const AnimationFixture& fixture,
		const AnimationSampleRequest& sample)
	{
		const bool styleResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "style-document";
		const bool visualTransitionResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-transition-document";
		DesignerModel::DesignDocument document;
		std::wstring error;
		const auto xaml = BuildRuntimeXaml(fixture);
		if (!DesignerModel::XamlDocumentParser::FromXaml(xaml, document, &error))
			throw std::runtime_error("CUI fixture XAML parse failed: "
				+ Convert::UnicodeToUtf8(error));
		DesignerModel::RuntimeDocument runtime;
		if (!DesignerModel::RuntimeDocumentLoader::Load(
			document, runtime, {}, &error))
			throw std::runtime_error("CUI fixture materialization failed: "
				+ Convert::UnicodeToUtf8(error));
		auto* host = runtime.FindControlByName(L"fixtureHost");
		auto* target = host
			? host->FindDeclarativeTemplatePart(L"target") : nullptr;
		if (!host || !target)
			throw std::runtime_error(
				"CUI fixture materialization did not produce its host and target.");

		std::optional<DeclarativeClockObservation> clock;
		std::vector<std::string> events;
		TimingEventCollector eventCollector(*host, events);
		EventConnection timingEventConnection;
		if (sample.Phase == "after-begin")
		{
			ClockOverrideScope clockScope(*host, FixtureClockOrigin);
			if (fixture.Oracle == "dispatcher-events"
				|| fixture.Oracle == "authored-root-dispatcher-events")
				timingEventConnection = host->OnStoryboardTimingEvent.Subscribe(
					[&](Control* sender, const DeclarativeClockTimingEventArgs& args)
					{ eventCollector.Observe(sender, args); });
			const bool handoffOracle = fixture.Oracle == "snapshot-replace"
				|| fixture.Oracle == "compose-handoff";
			const bool began = styleResource
				? host->TrySetPropertyValue(L"Phase", BindingValue(1))
				: handoffOracle
					? host->RaiseDeclarativeEvent(L"BeginPrimary")
					: host->GoToVisualState(
						L"FixtureStates", L"Running",
						visualTransitionResource, &error);
			if (!began)
				throw std::runtime_error("CUI fixture animation begin failed: "
					+ Convert::UnicodeToUtf8(error));
			if (handoffOracle)
			{
				const auto replacementAt = fixture.Operations.front().AtMilliseconds;
				if (sample.AtMilliseconds >= replacementAt)
				{
					const auto replacementTick = FixtureClockOrigin + replacementAt;
					clockScope.Set(replacementTick);
					(void)cui::framework::PresentationAccess::
						AdvanceVisualStateAnimations(*host, replacementTick);
					if (!host->RaiseDeclarativeEvent(L"BeginReplacement"))
						throw std::runtime_error(
							"CUI fixture replacement begin failed: "
							+ Convert::UnicodeToUtf8(error));
				}
			}
			else if (fixture.Oracle == "synchronous-control"
				|| fixture.Oracle == "authored-root-control")
			{
				for (const auto& operation : fixture.Operations)
				{
					if (operation.AtMilliseconds > sample.AtMilliseconds) break;
					if (operation.Kind != "seek-aligned" || !operation.Value
						|| *operation.Value < 0.0
						|| *operation.Value
							> static_cast<double>((std::numeric_limits<
								unsigned long long>::max)())
						|| std::floor(*operation.Value) != *operation.Value)
						throw std::runtime_error(
							"CUI synchronous fixture has an invalid aligned seek.");
					clockScope.Set(FixtureClockOrigin + operation.AtMilliseconds);
					if (!cui::framework::PresentationAccess::
						SeekSingleVisualStateAnimationRootAlignedForTesting(
							*host, static_cast<unsigned long long>(*operation.Value)))
						throw std::runtime_error(
							"CUI synchronous aligned seek failed transactionally.");
				}
			}
			else if (fixture.Oracle == "dispatcher-control"
				|| fixture.Oracle == "authored-root-dispatcher-control")
			{
				for (const auto& operation : fixture.Operations)
				{
					if (operation.AtMilliseconds > sample.AtMilliseconds) break;
					ExecuteDispatcherControlOperation(
						operation, *host, clockScope,
						"CUI Design dispatcher-control fixture");
				}
			}
			else if (fixture.Oracle == "dispatcher-events"
				|| fixture.Oracle == "authored-root-dispatcher-events")
			{
				const bool capturesBegin = std::any_of(
					fixture.Operations.begin(), fixture.Operations.end(),
					[](const auto& operation)
					{ return operation.Kind == "begin-tick"; });
				if (!capturesBegin)
				{
					clockScope.Set(FixtureClockOrigin);
					(void)cui::framework::PresentationAccess::
						AdvanceVisualStateAnimations(*host, FixtureClockOrigin);
					if (cui::framework::PresentationAccess::
						VisualStateAnimationAdvanceFailedForTesting(*host))
						throw std::runtime_error(
							"CUI Design event baseline tick failed transactionally.");
					eventCollector.Clear();
				}
				for (const auto& operation : fixture.Operations)
				{
					if (operation.AtMilliseconds > sample.AtMilliseconds) break;
					ExecuteDispatcherEventOperation(
						operation, *host, clockScope,
						"CUI Design dispatcher-events fixture");
				}
			}
			const auto sampleTick = FixtureClockOrigin + sample.AtMilliseconds;
			clockScope.Set(sampleTick);
			if (fixture.Oracle != "dispatcher-control"
				&& fixture.Oracle != "authored-root-dispatcher-control"
				&& fixture.Oracle != "dispatcher-events"
				&& fixture.Oracle != "authored-root-dispatcher-events")
			{
				(void)cui::framework::PresentationAccess::
					AdvanceVisualStateAnimations(*host, sampleTick);
				if (cui::framework::PresentationAccess::
					VisualStateAnimationAdvanceFailedForTesting(*host))
					throw std::runtime_error(
						"CUI fixture animation advance failed transactionally.");
			}
			if (fixture.Oracle == "synchronous-control"
				|| fixture.Oracle == "authored-root-control"
				|| fixture.Oracle == "dispatcher-control"
				|| fixture.Oracle == "authored-root-dispatcher-control"
				|| fixture.Oracle == "dispatcher-events"
				|| fixture.Oracle == "authored-root-dispatcher-events")
			{
				clock = cui::framework::PresentationAccess::
					QuerySingleVisualStateAnimationRootForTesting(*host);
				if (!clock && (fixture.Oracle == "synchronous-control"
					|| fixture.Oracle == "authored-root-control"
					|| fixture.Oracle == "authored-root-dispatcher-control"))
					throw std::runtime_error(
						"CUI synchronous root clock query failed.");
			}
		}

		return ObserveDesignSample(
			fixture, sample, *host, *target,
			std::move(clock), std::move(events));
	}

	DeclarativeEasingKind ToRuntimeEasing(DesignerEasingKind value)
	{
		switch (value)
		{
		case DesignerEasingKind::Linear: return DeclarativeEasingKind::Linear;
		case DesignerEasingKind::Quadratic: return DeclarativeEasingKind::Quadratic;
		case DesignerEasingKind::Cubic: return DeclarativeEasingKind::Cubic;
		case DesignerEasingKind::Sine: return DeclarativeEasingKind::Sine;
		case DesignerEasingKind::Back: return DeclarativeEasingKind::Back;
		case DesignerEasingKind::Bounce: return DeclarativeEasingKind::Bounce;
		case DesignerEasingKind::Circle: return DeclarativeEasingKind::Circle;
		case DesignerEasingKind::Elastic: return DeclarativeEasingKind::Elastic;
		case DesignerEasingKind::Exponential: return DeclarativeEasingKind::Exponential;
		case DesignerEasingKind::Power: return DeclarativeEasingKind::Power;
		case DesignerEasingKind::Quartic: return DeclarativeEasingKind::Quartic;
		case DesignerEasingKind::Quintic: return DeclarativeEasingKind::Quintic;
		}
		throw std::runtime_error("Unsupported AOT fixture easing kind.");
	}

	DeclarativeEasingMode ToRuntimeEasingMode(DesignerEasingMode value)
	{
		switch (value)
		{
		case DesignerEasingMode::EaseIn: return DeclarativeEasingMode::EaseIn;
		case DesignerEasingMode::EaseOut: return DeclarativeEasingMode::EaseOut;
		case DesignerEasingMode::EaseInOut: return DeclarativeEasingMode::EaseInOut;
		}
		throw std::runtime_error("Unsupported AOT fixture easing mode.");
	}

	DeclarativeKeyFrameKind ToRuntimeKeyFrameKind(DesignerKeyFrameKind value)
	{
		switch (value)
		{
		case DesignerKeyFrameKind::Discrete:
			return DeclarativeKeyFrameKind::Discrete;
		case DesignerKeyFrameKind::Linear:
			return DeclarativeKeyFrameKind::Linear;
		case DesignerKeyFrameKind::Easing:
			return DeclarativeKeyFrameKind::Easing;
		case DesignerKeyFrameKind::Spline:
			return DeclarativeKeyFrameKind::Spline;
		}
		throw std::runtime_error("Unsupported AOT fixture key-frame kind.");
	}

	struct AotFixtureProgram final
	{
		std::vector<BindingValue> Values;
		std::vector<CompiledInteractionPropertyOperand> PropertyOperands;
		std::vector<uint32_t> ObjectPathChildIndices;
		std::vector<CompiledStoryboardObjectPathOp> ObjectPaths;
		std::vector<CompiledInteractionKeyFrameOp> KeyFrames;
		std::vector<DeclarativePathAnimationSegment> PathSegments;
		std::vector<CompiledInteractionAnimationOp> Animations;
		std::vector<CompiledInteractionTimelineGroupOp> TimelineGroups;
		std::vector<CompiledInteractionStoryboardOp> Storyboards;
		std::vector<CompiledInteractionActionOp> Actions;
		std::vector<CompiledInteractionEventTriggerOp> EventTriggers;
		std::vector<const DeclarativeEventDefinition*> StateEvents;
		std::vector<CompiledInteractionStateOp> States;
		std::vector<CompiledInteractionTransitionOp> Transitions;
		std::vector<CompiledInteractionGroupOp> Groups;
		std::vector<CompiledStylePropertyConditionOp> StylePropertyConditions;
		std::vector<CompiledStyleRuleOp> StyleRules;
		std::vector<uint32_t> StyleRuleIndexes;
		std::vector<DependencyPropertyReference> StylePropertyWatchers;
		std::vector<CompiledStyleGroupOp> StyleGroups;

		CompiledInteractionProgramView View() const
		{
			CompiledInteractionProgramView result;
			result.Version = CompiledInteractionProgramViewVersion;
			result.TargetCount = 0;
			for (const auto& operand : PropertyOperands)
				result.TargetCount = (std::max)(
					result.TargetCount, operand.TargetSlot + 1u);
			result.PropertyOperands = PropertyOperands;
			result.ObjectPathChildIndices = ObjectPathChildIndices;
			result.ObjectPaths = ObjectPaths;
			result.KeyFrames = KeyFrames;
			result.PathSegments = PathSegments;
			result.Animations = Animations;
			result.TimelineGroups = TimelineGroups;
			result.Storyboards = Storyboards;
			result.Actions = Actions;
			result.EventTriggers = EventTriggers;
			result.StateEvents = StateEvents;
			result.States = States;
			result.Transitions = Transitions;
			result.Groups = Groups;
			return result;
		}

		CompiledStyleProgramView StyleView() const
		{
			CompiledStyleProgramView result;
			result.Version = CompiledStyleProgramViewVersion;
			result.PropertyConditions = StylePropertyConditions;
			result.PropertyOperands = PropertyOperands;
			result.ObjectPathChildIndices = ObjectPathChildIndices;
			result.ObjectPaths = ObjectPaths;
			result.KeyFrames = KeyFrames;
			result.PathSegments = PathSegments;
			result.Animations = Animations;
			result.TimelineGroups = TimelineGroups;
			result.Storyboards = Storyboards;
			result.Actions = Actions;
			result.Rules = StyleRules;
			result.RuleIndexes = StyleRuleIndexes;
			result.PropertyWatchers = StylePropertyWatchers;
			result.Groups = StyleGroups;
			result.GlobalPropertyWatchers = StylePropertyWatchers;
			return result;
		}
	};

	std::vector<const DesignerVisualStateAnimation*> ParsedFixtureAnimations(
		const AnimationFixture& fixture,
		DesignerModel::DesignDocument& document)
	{
		std::wstring error;
		if (!DesignerModel::XamlDocumentParser::FromXaml(
			BuildRuntimeXaml(fixture), document, &error))
			throw std::runtime_error("CUI AOT fixture XAML parse failed: "
				+ Convert::UnicodeToUtf8(error));
		if (fixture.Target.Kind == "int32" || fixture.Target.Kind == "int64"
			|| fixture.Target.Kind == "single" || fixture.Target.Kind == "boolean"
			|| fixture.Target.Kind == "string"
			|| fixture.Target.Probe == "geometry-transform-direct-x"
			|| fixture.Target.Probe == "brush-transform-direct-angle"
			|| fixture.Target.Probe
				== "brush-relative-transform-direct-scale-x"
			|| FindTransformLeafProbe(fixture.Target.Probe) != nullptr
			|| FindBrushLeafProbe(fixture.Target.Probe) != nullptr
			|| FindGeometryLeafProbe(fixture.Target.Probe) != nullptr
			|| fixture.StoryboardResourceKey.has_value()
			|| fixture.TimelineXaml.find("<ParallelTimeline")
				!= std::string::npos)
		{
			const auto canonical = DesignerModel::XamlDocumentSerializer::ToXaml(document);
			DesignerModel::DesignDocument canonicalRoundTrip;
			if (!DesignerModel::XamlDocumentParser::FromXaml(
				canonical, canonicalRoundTrip, &error)
				|| canonicalRoundTrip != document)
				throw std::runtime_error(
					"CUI alias fixture canonical XAML round-trip diverged: "
					+ Convert::UnicodeToUtf8(error));
			const auto snapshot =
				DesignerModel::DesignDocumentSerializer::ToXml(document);
			DesignerModel::DesignDocument snapshotRoundTrip;
			if (!DesignerModel::DesignDocumentSerializer::FromXml(
				snapshot, snapshotRoundTrip, &error)
				|| snapshotRoundTrip != document)
				throw std::runtime_error(
					"CUI alias fixture native snapshot round-trip diverged: "
					+ Convert::UnicodeToUtf8(error));
		}
		if (document.Components.size() != 1)
			throw std::runtime_error(
				"CUI AOT fixture must define exactly one component.");
		if (fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "style-document")
		{
			if (document.StyleSheet.Rules.size() != 1u
				|| document.StyleSheet.Rules.front().Triggers.size() != 1u)
				throw std::runtime_error(
					"CUI AOT Style fixture did not normalize to one Trigger.");
			const auto& trigger =
				document.StyleSheet.Rules.front().Triggers.front();
			if (trigger.EnterActions.size() != 1u
				|| trigger.ExitActions.size() != 1u
				|| trigger.EnterActions.front().Kind
					!= DesignerStoryboardActionKind::Begin
				|| trigger.EnterActions.front().StoryboardResourceKey
					!= Convert::Utf8ToUnicode(*fixture.StoryboardResourceKey)
				|| (trigger.EnterActions.front().Animations.empty()
					&& trigger.EnterActions.front().TimelineGroups.empty()))
				throw std::runtime_error(
					"CUI AOT Style resource action was not normalized.");
			std::vector<const DesignerVisualStateAnimation*> result;
			for (const auto& animation :
				trigger.EnterActions.front().Animations)
				result.push_back(&animation);
			std::function<void(const DesignerTimelineGroup&)> appendGroup;
			appendGroup = [&](const DesignerTimelineGroup& group)
			{
				for (const auto& animation : group.Animations)
					result.push_back(&animation);
				for (const auto& child : group.Children) appendGroup(child);
			};
			for (const auto& group :
				trigger.EnterActions.front().TimelineGroups)
				appendGroup(group);
			return result;
		}
		if (fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-transition-document")
		{
			const auto& groups = document.Components.front().VisualStateGroups;
			if (groups.size() != 1u || groups.front().Transitions.size() != 1u)
				throw std::runtime_error(
					"CUI AOT transition fixture did not normalize one transition.");
			const auto& transition = groups.front().Transitions.front();
			if (transition.StoryboardResourceKey
					!= Convert::Utf8ToUnicode(*fixture.StoryboardResourceKey)
				|| (transition.Animations.empty()
					&& transition.TimelineGroups.empty()))
				throw std::runtime_error(
					"CUI AOT transition resource was not normalized.");
			std::vector<const DesignerVisualStateAnimation*> result;
			for (const auto& animation : transition.Animations)
				result.push_back(&animation);
			std::function<void(const DesignerTimelineGroup&)> appendGroup;
			appendGroup = [&](const DesignerTimelineGroup& group)
			{
				for (const auto& animation : group.Animations)
					result.push_back(&animation);
				for (const auto& child : group.Children) appendGroup(child);
			};
			for (const auto& group : transition.TimelineGroups)
				appendGroup(group);
			return result;
		}
		if (fixture.ReplacementTimelineXaml)
		{
			const auto& triggers = document.Components.front().EventTriggers;
			std::vector<const DesignerVisualStateAnimation*> result;
			for (const auto eventName : { L"BeginPrimary", L"BeginReplacement" })
			{
				const auto trigger = std::find_if(triggers.begin(), triggers.end(),
					[&](const auto& item) { return item.EventName == eventName; });
				if (trigger == triggers.end() || trigger->Actions.size() != 1u
					|| trigger->Actions.front().Kind
						!= DesignerStoryboardActionKind::Begin
					|| trigger->Actions.front().Animations.size() != 1u)
					throw std::runtime_error(
						"CUI AOT replacement EventTrigger was not normalized.");
				result.push_back(&trigger->Actions.front().Animations.front());
			}
			return result;
		}
		const auto& groups = document.Components.front().VisualStateGroups;
		auto findState = [&](std::wstring_view groupName,
			std::wstring_view stateName)
			-> const DesignerVisualState*
		{
			const auto group = std::find_if(groups.begin(), groups.end(),
				[&](const auto& item) { return item.Name == groupName; });
			if (group == groups.end()) return nullptr;
			const auto state = std::find_if(group->States.begin(), group->States.end(),
				[&](const auto& item) { return item.Name == stateName; });
			return state == group->States.end() ? nullptr : &*state;
		};
		std::vector<const DesignerVisualStateAnimation*> result;
		if (const auto* state = findState(L"FixtureStates", L"Running");
			state && (!state->Animations.empty()
				|| !state->TimelineGroups.empty()))
		{
			for (const auto& animation : state->Animations)
				result.push_back(&animation);
			std::function<void(const DesignerTimelineGroup&)> appendGroup;
			appendGroup = [&](const DesignerTimelineGroup& timelineGroup)
			{
				for (const auto& animation : timelineGroup.Animations)
					result.push_back(&animation);
				for (const auto& child : timelineGroup.Children)
					appendGroup(child);
			};
			for (const auto& timelineGroup : state->TimelineGroups)
				appendGroup(timelineGroup);
		}
		else
			throw std::runtime_error(
				"CUI AOT fixture must normalize to one Running animation.");
		return result;
	}

	AotFixtureProgram BuildAotFixtureProgram(
		const AnimationFixture& fixture,
		std::span<const DesignerVisualStateAnimation* const> sources,
		const DesignerModel::DesignDocument& document)
	{
		const auto* transformLeaf = FindTransformLeafProbe(fixture.Target.Probe);
		const auto* brushLeaf = FindBrushLeafProbe(fixture.Target.Probe);
		const auto* geometryLeaf = FindGeometryLeafProbe(fixture.Target.Probe);
		const bool brushLeafPoint = brushLeaf && BrushLeafUsesPoint(*brushLeaf);
		const bool brushLeafColor = brushLeaf && BrushLeafUsesColor(*brushLeaf);
		const bool geometryPoint = geometryLeaf && geometryLeaf->TargetKind == "point";
		const bool geometryRect = geometryLeaf && geometryLeaf->TargetKind == "rect";
		const bool geometrySize = geometryLeaf && geometryLeaf->TargetKind == "size";
		const bool geometryBoolean = geometryLeaf
			&& geometryLeaf->TargetKind == "boolean";
		const bool geometryString = geometryLeaf
			&& geometryLeaf->TargetKind == "string";
		const bool pointProjection = fixture.Target.Probe == "render-transform-origin-x"
			|| fixture.Target.Probe == "render-transform-origin-y"
			|| brushLeafPoint || geometryPoint;
		const bool thicknessProjection =
			fixture.Target.Probe == "border-thickness-left";
		const bool metadataColor = fixture.Target.Probe == "metadata-color";
		const bool colorProjection = metadataColor || brushLeafColor;
		const bool vectorProjection = fixture.Target.Probe == "metadata-vector";
		const bool metadataRect = fixture.Target.Probe == "metadata-rect";
		const bool rectProjection = metadataRect || geometryRect;
		const bool metadataSize = fixture.Target.Probe == "metadata-size";
		const bool sizeProjection = metadataSize || geometrySize;
		const bool metadataMatrix = fixture.Target.Probe == "metadata-matrix";
		const bool transformMatrix = transformLeaf
			&& transformLeaf->TargetKind == "matrix";
		const bool matrixProjection = metadataMatrix || transformMatrix;
		const bool int32Projection = fixture.Target.Probe == "metadata-int32";
		const bool int64Projection = fixture.Target.Probe == "metadata-int64";
		const bool singleProjection = fixture.Target.Probe == "metadata-single";
		const bool metadataBoolean = fixture.Target.Probe == "metadata-boolean";
		const bool booleanProjection = metadataBoolean || geometryBoolean;
		const bool metadataString = fixture.Target.Probe == "metadata-string";
		const bool stringProjection = metadataString || geometryString;
		const bool geometryTransformDirect =
			fixture.Target.Probe == "geometry-transform-direct-x";
		const bool brushTransformDirect =
			fixture.Target.Probe == "brush-transform-direct-angle";
		const bool brushRelativeTransformDirect =
			fixture.Target.Probe == "brush-relative-transform-direct-scale-x";
		const bool objectPathFixture = geometryTransformDirect
			|| brushTransformDirect || brushRelativeTransformDirect
			|| transformLeaf || brushLeaf || geometryLeaf;
		const auto expectedKind = pointProjection
			? DesignerAnimationKind::Point
			: thicknessProjection
				? DesignerAnimationKind::Thickness
				: colorProjection
					? DesignerAnimationKind::Color
					: vectorProjection
						? DesignerAnimationKind::Vector
						: rectProjection
							? DesignerAnimationKind::Rect
							: sizeProjection
								? DesignerAnimationKind::Size
					: matrixProjection
									? DesignerAnimationKind::Matrix
								: int32Projection
									? DesignerAnimationKind::Int32
								: int64Projection
									? DesignerAnimationKind::Int64
								: singleProjection
									? DesignerAnimationKind::Single
				: geometryString
					? DesignerAnimationKind::Object
				: booleanProjection
					? DesignerAnimationKind::Boolean
								: stringProjection
									? DesignerAnimationKind::String
									: DesignerAnimationKind::Double;
		if (sources.empty()
			|| (fixture.ReplacementTimelineXaml
				? sources.size() != 2u : sources.size() > 2u)
			|| std::any_of(sources.begin(), sources.end(), [&](const auto* source)
				{ return !source || source->Kind != expectedKind; }))
			throw std::runtime_error("CUI AOT fixture has an invalid animation kind.");
		const bool metadataDouble = fixture.Target.Probe == "metadata-double";
		const bool metadataTarget = metadataDouble
			|| metadataColor || vectorProjection
			|| metadataRect || metadataSize || metadataMatrix
			|| int32Projection || int64Projection || singleProjection
			|| metadataBoolean || metadataString;
		const std::wstring expectedProperty = transformLeaf
			? std::wstring(transformLeaf->CanonicalPropertyPath)
			: geometryLeaf
			? std::wstring(geometryLeaf->CanonicalPropertyPath)
			: brushLeaf
			? std::wstring(brushLeaf->CanonicalPropertyPath)
			: geometryTransformDirect
			? L"(Control.Clip).(Geometry.Transform).(TranslateTransform.X)"
			: brushTransformDirect
				? L"(Control.Background).(Brush.Transform).(RotateTransform.Angle)"
			: brushRelativeTransformDirect
				? L"(Control.Background).(Brush.RelativeTransform).(ScaleTransform.ScaleX)"
			: metadataTarget
			? L"Value" : pointProjection
				? L"RenderTransformOrigin" : thicknessProjection
					? L"BorderThickness" : L"Canvas.Left";
		for (const auto* source : sources)
		{
			const bool primary = source->TargetName
					== (metadataTarget ? L"" : L"target")
				&& source->PropertyName == expectedProperty;
			const bool auxiliary = !fixture.ReplacementTimelineXaml
				&& fixture.Target.Probe == "canvas-left"
				&& source->TargetName == L"target"
				&& source->PropertyName == L"Canvas.Top"
				&& source->HasFrom && source->HasTo;
			if (!primary && !auxiliary)
				throw std::runtime_error(
					"CUI AOT normalized target differs from the fixture contract: "
					+ Convert::UnicodeToUtf8(source->TargetName) + "."
					+ Convert::UnicodeToUtf8(source->PropertyName) + ".");
		}

		AotFixtureProgram result;
		result.PropertyOperands.resize(sources.size());
		for (std::size_t index = 0; index < sources.size(); ++index)
		{
			auto& operand = result.PropertyOperands[index];
			operand.TargetSlot = metadataTarget ? 0u : 1u;
			operand.Property = DependencyPropertyReference(
				sources[index]->PropertyName == L"Canvas.Top"
					? Control::CanvasTopProperty()
				: metadataDouble
				? AnimationDoubleProbeControl::ValueProperty()
				: int32Projection
					? AnimationInt32ProbeControl::ValueProperty()
				: int64Projection
					? AnimationInt64ProbeControl::ValueProperty()
				: singleProjection
					? AnimationSingleProbeControl::ValueProperty()
				: metadataBoolean
					? AnimationBooleanProbeControl::ValueProperty()
				: metadataString
					? AnimationStringProbeControl::ValueProperty()
				: metadataColor
					? AnimationColorProbeControl::ValueProperty()
				: vectorProjection
					? AnimationVectorProbeControl::ValueProperty()
				: metadataRect
					? AnimationRectProbeControl::ValueProperty()
				: metadataSize
					? AnimationSizeProbeControl::ValueProperty()
				: metadataMatrix
					? AnimationMatrixProbeControl::ValueProperty()
				: transformLeaf
					? (transformLeaf->Root
							== TransformLeafProbeRoot::RenderTransform
						? Control::RenderTransformProperty()
						: transformLeaf->Root
							== TransformLeafProbeRoot::GeometryTransformDirect
							|| transformLeaf->Root
								== TransformLeafProbeRoot::GeometryTransformChild10
							? Control::ClipProperty()
							: Control::BackgroundProperty())
				: geometryLeaf
					? Control::ClipProperty()
				: brushLeaf
					? (brushLeaf->RootProperty == L"BorderBrush"
						? Control::BorderBrushProperty()
						: Control::BackgroundProperty())
				: geometryTransformDirect
					? Control::ClipProperty()
				: brushTransformDirect || brushRelativeTransformDirect
					? Control::BackgroundProperty()
				: pointProjection
					? Control::RenderTransformOriginProperty()
					: thicknessProjection
						? Border::BorderThicknessProperty()
						: Control::CanvasLeftProperty());
		}
		auto addValue = [&](const DesignerStyleValue& value)
		{
			if (result.Values.size() >= CompiledInteractionInvalidIndex)
				throw std::runtime_error("CUI AOT fixture value pool overflow.");
			const auto index = static_cast<uint32_t>(result.Values.size());
			const auto text = Convert::UnicodeToUtf8(value.Text);
			if (colorProjection)
			{
				result.Values.emplace_back(ParseArgbColor(text, "AOT Color value"));
			}
			else if (int32Projection)
				result.Values.emplace_back(
					ParseSignedInteger<int>(text, "AOT Int32 value"));
			else if (int64Projection)
				result.Values.emplace_back(
					ParseSignedInteger<long long>(text, "AOT Int64 value"));
			else if (singleProjection)
				result.Values.emplace_back(ParseSingle(text, "AOT Single value"));
			else if (booleanProjection)
				result.Values.emplace_back(ParseBoolean(text, "AOT Boolean value"));
			else if (stringProjection)
				result.Values.emplace_back(value.Text);
			else if (vectorProjection)
			{
				result.Values.emplace_back(ParseVector(text, "AOT Vector value"));
			}
			else if (rectProjection)
			{
				result.Values.emplace_back(ParseRect(text, "AOT Rect value"));
			}
			else if (sizeProjection)
			{
				result.Values.emplace_back(ParseSize(text, "AOT Size value"));
			}
			else if (matrixProjection)
			{
				result.Values.emplace_back(ParseMatrix(text, "AOT Matrix value"));
			}
			else if (pointProjection)
			{
				const auto comma = text.find(',');
				if (comma == std::string::npos
					|| text.find(',', comma + 1u) != std::string::npos)
					throw std::runtime_error(
						"CUI AOT Point animation value must use x,y format.");
				const auto x = ParseDouble(
					std::string_view(text).substr(0, comma), "AOT Point x");
				const auto y = ParseDouble(
					std::string_view(text).substr(comma + 1u), "AOT Point y");
				result.Values.emplace_back(cui::core::Point{
					static_cast<float>(x), static_cast<float>(y) });
			}
			else if (thicknessProjection)
			{
				std::array<float, 4> components{};
				size_t start = 0;
				for (size_t component = 0; component < components.size(); ++component)
				{
					const auto comma = text.find(',', start);
					if ((component + 1u < components.size()
						&& comma == std::string::npos)
						|| (component + 1u == components.size()
							&& comma != std::string::npos))
						throw std::runtime_error(
							"CUI AOT Thickness value must use left,top,right,bottom format.");
					const auto end = comma == std::string::npos
						? text.size() : comma;
					components[component] = static_cast<float>(ParseDouble(
						std::string_view(text).substr(start, end - start),
						"AOT Thickness component"));
					start = end + 1u;
				}
				result.Values.emplace_back(Thickness{
					components[0], components[1], components[2], components[3] });
			}
			else
			{
				const double parsed = ParseDouble(text, "AOT animation value");
				result.Values.emplace_back(metadataDouble
					? BindingValue(parsed)
					: BindingValue(static_cast<float>(parsed)));
			}
			return index;
		};

		if (objectPathFixture)
		{
			if (sources.size() != 1u)
				throw std::runtime_error(
					"CUI object-path fixture requires one animation.");
			CompiledStoryboardObjectPathOp objectPath;
			if (transformLeaf)
			{
				objectPath.Kind = transformLeaf->Root
					== TransformLeafProbeRoot::RenderTransform
					? CompiledStoryboardObjectPathKind::Transform
					: transformLeaf->Root
						== TransformLeafProbeRoot::GeometryTransformDirect
						|| transformLeaf->Root
							== TransformLeafProbeRoot::GeometryTransformChild10
						? CompiledStoryboardObjectPathKind::GeometryTransform
						: CompiledStoryboardObjectPathKind::BrushTransform;
				objectPath.Member = transformLeaf->Member;
				objectPath.Index0 = transformLeaf->OperationIndex;
				if (objectPath.Kind == CompiledStoryboardObjectPathKind::Transform)
					objectPath.ExpectedObjectKind = static_cast<uint8_t>(
						transformLeaf->TransformKind);
				else
				{
					objectPath.ExpectedObjectKind = transformLeaf->ExpectedOuterKind;
					objectPath.ExpectedAuxiliaryKind = static_cast<uint8_t>(
						transformLeaf->TransformKind);
				}
				if (transformLeaf->Direct)
					objectPath.Flags =
						CompiledStoryboardObjectPathFlags::DirectTransform;
				if (transformLeaf->Root
					== TransformLeafProbeRoot::BrushRelativeTransform)
					objectPath.Flags = objectPath.Flags
						| CompiledStoryboardObjectPathFlags::RelativeTransform;
				const auto children = TransformLeafChildIndices(*transformLeaf);
				objectPath.ChildIndices = {
					static_cast<uint32_t>(result.ObjectPathChildIndices.size()),
					static_cast<uint32_t>(children.size()) };
				result.ObjectPathChildIndices.insert(
					result.ObjectPathChildIndices.end(),
					children.begin(), children.end());
			}
			else if (geometryLeaf)
			{
				objectPath.Kind = geometryLeaf->PathKind;
				objectPath.Member = geometryLeaf->Member;
				objectPath.ExpectedObjectKind = geometryLeaf->ExpectedObjectKind;
				objectPath.Index0 = geometryLeaf->FigureIndex;
				objectPath.Index1 = geometryLeaf->SegmentIndex;
				const auto children = GeometryLeafChildIndices(*geometryLeaf);
				objectPath.ChildIndices = {
					static_cast<uint32_t>(result.ObjectPathChildIndices.size()),
					static_cast<uint32_t>(children.size()) };
				result.ObjectPathChildIndices.insert(
					result.ObjectPathChildIndices.end(),
					children.begin(), children.end());
				if (geometryLeaf->PathKind
					== CompiledStoryboardObjectPathKind::PathGeometry
					&& geometryLeaf->Value != GeometryLeafProbeValue::FigureStartPointX
					&& geometryLeaf->Value != GeometryLeafProbeValue::FigureIsClosed
					&& geometryLeaf->Value != GeometryLeafProbeValue::FigureIsFilled)
					objectPath.Flags =
						CompiledStoryboardObjectPathFlags::HasPathSegment;
			}
			else if (brushLeaf)
			{
				objectPath.Kind = CompiledStoryboardObjectPathKind::Brush;
				objectPath.Member = brushLeaf->Member;
				objectPath.ExpectedObjectKind = brushLeaf->ExpectedBrushKind;
				objectPath.Index0 = brushLeaf->StopIndex;
			}
			else
			{
				objectPath.Kind = geometryTransformDirect
					? CompiledStoryboardObjectPathKind::GeometryTransform
					: CompiledStoryboardObjectPathKind::BrushTransform;
				objectPath.Member = geometryTransformDirect
					? CompiledStoryboardObjectPathMember::TransformX
					: brushTransformDirect
						? CompiledStoryboardObjectPathMember::TransformAngle
						: CompiledStoryboardObjectPathMember::TransformScaleX;
				objectPath.ExpectedObjectKind = geometryTransformDirect
					? static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle)
					: static_cast<uint8_t>(brushTransformDirect
						? cui::drawing::BrushKind::Solid
						: cui::drawing::BrushKind::LinearGradient);
				objectPath.ExpectedAuxiliaryKind = static_cast<uint8_t>(
					geometryTransformDirect
						? cui::drawing::TransformKind::Translate
						: brushTransformDirect
							? cui::drawing::TransformKind::Rotate
							: cui::drawing::TransformKind::Scale);
				objectPath.Flags =
					CompiledStoryboardObjectPathFlags::DirectTransform;
				if (brushRelativeTransformDirect)
					objectPath.Flags = objectPath.Flags
						| CompiledStoryboardObjectPathFlags::RelativeTransform;
			}
			objectPath.Identity = MakeCompiledInteractionNameToken(expectedProperty);
			result.ObjectPaths.push_back(objectPath);
		}

		result.Animations.resize(sources.size());
		for (std::size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex)
		{
			const auto& source = *sources[sourceIndex];
			auto& animation = result.Animations[sourceIndex];
			if (objectPathFixture) animation.ObjectPathIndex = 0u;
			animation.Kind = pointProjection
				? DeclarativeAnimationKind::Point
				: thicknessProjection
					? DeclarativeAnimationKind::Thickness
					: colorProjection
						? DeclarativeAnimationKind::Color
						: vectorProjection
							? DeclarativeAnimationKind::Vector
							: rectProjection
								? DeclarativeAnimationKind::Rect
								: sizeProjection
									? DeclarativeAnimationKind::Size
					: matrixProjection
									? DeclarativeAnimationKind::Matrix
								: int32Projection
									? DeclarativeAnimationKind::Int32
								: int64Projection
									? DeclarativeAnimationKind::Int64
								: singleProjection
									? DeclarativeAnimationKind::Single
								: booleanProjection || stringProjection
									? DeclarativeAnimationKind::Object
										: DeclarativeAnimationKind::Double;
			animation.OperandIndex = static_cast<uint32_t>(sourceIndex);
			if (source.HasFrom) animation.FromValueIndex = addValue(source.From);
			if (source.HasTo) animation.ToValueIndex = addValue(source.To);
			if (source.HasBy) animation.ByValueIndex = addValue(source.By);
			animation.KeyFrames = {
				static_cast<uint32_t>(result.KeyFrames.size()),
				static_cast<uint32_t>(source.KeyFrames.size()) };
			for (const auto& sourceFrame : source.KeyFrames)
			{
				CompiledInteractionKeyFrameOp frame;
				frame.Kind = ToRuntimeKeyFrameKind(sourceFrame.Kind);
				frame.KeyTimeMilliseconds = sourceFrame.KeyTimeMilliseconds;
				frame.KeyTimeSubMillisecondTicks =
					sourceFrame.KeyTimeSubMillisecondTicks;
				frame.ValueIndex = addValue(sourceFrame.Value);
				frame.Easing = ToRuntimeEasing(sourceFrame.Easing);
				frame.EasingMode = ToRuntimeEasingMode(sourceFrame.EasingMode);
				frame.EasingParameters = {
					sourceFrame.EasingParameters.Primary,
					sourceFrame.EasingParameters.Secondary };
				frame.KeySplineX1 = sourceFrame.KeySplineX1;
				frame.KeySplineY1 = sourceFrame.KeySplineY1;
				frame.KeySplineX2 = sourceFrame.KeySplineX2;
				frame.KeySplineY2 = sourceFrame.KeySplineY2;
				result.KeyFrames.push_back(frame);
			}
			animation.IsAdditive = source.IsAdditive;
			animation.IsCumulative = source.IsCumulative;
			animation.Path = source.Path;
			animation.PathSegments = {
				static_cast<uint32_t>(result.PathSegments.size()),
				static_cast<uint32_t>(source.PathSegments.size()) };
			result.PathSegments.insert(result.PathSegments.end(),
				source.PathSegments.begin(), source.PathSegments.end());
			animation.BeginTimeMilliseconds = source.BeginTimeMilliseconds;
			animation.DurationMilliseconds = source.DurationMilliseconds;
			switch (source.RepeatBehavior)
			{
			case DesignerRepeatBehaviorKind::Count:
				animation.RepeatBehavior = DeclarativeRepeatBehaviorKind::Count;
				break;
			case DesignerRepeatBehaviorKind::Duration:
				animation.RepeatBehavior = DeclarativeRepeatBehaviorKind::Duration;
				break;
			case DesignerRepeatBehaviorKind::Forever:
				animation.RepeatBehavior = DeclarativeRepeatBehaviorKind::Forever;
				break;
			}
			animation.RepeatCount = source.RepeatCount;
			animation.RepeatDurationMilliseconds = source.RepeatDurationMilliseconds;
			animation.AutoReverse = source.AutoReverse;
			animation.FillBehavior = source.FillBehavior
				== DesignerTimelineFillBehavior::Stop
				? DeclarativeTimelineFillBehavior::Stop
				: DeclarativeTimelineFillBehavior::HoldEnd;
			animation.SpeedRatio = source.SpeedRatio;
			animation.AccelerationRatio = source.AccelerationRatio;
			animation.DecelerationRatio = source.DecelerationRatio;
			animation.Easing = ToRuntimeEasing(source.Easing);
			animation.EasingMode = ToRuntimeEasingMode(source.EasingMode);
			animation.EasingParameters = {
				source.EasingParameters.Primary,
				source.EasingParameters.Secondary };
		}
		auto convertTiming = [](const DesignerStoryboardTiming& source)
		{
			DeclarativeStoryboardTimingDefinition timing;
			timing.BeginTimeMilliseconds = source.BeginTimeMilliseconds;
			timing.DurationAutomatic = source.DurationAutomatic;
			timing.DurationMilliseconds = source.DurationMilliseconds;
			timing.RepeatBehavior = source.RepeatBehavior
				== DesignerRepeatBehaviorKind::Duration
				? DeclarativeRepeatBehaviorKind::Duration
				: source.RepeatBehavior == DesignerRepeatBehaviorKind::Forever
					? DeclarativeRepeatBehaviorKind::Forever
					: DeclarativeRepeatBehaviorKind::Count;
			timing.RepeatCount = source.RepeatCount;
			timing.RepeatDurationMilliseconds =
				source.RepeatDurationMilliseconds;
			timing.AutoReverse = source.AutoReverse;
			timing.FillBehavior = source.FillBehavior
				== DesignerTimelineFillBehavior::Stop
				? DeclarativeTimelineFillBehavior::Stop
				: DeclarativeTimelineFillBehavior::HoldEnd;
			timing.SpeedRatio = source.SpeedRatio;
			timing.AccelerationRatio = source.AccelerationRatio;
			timing.DecelerationRatio = source.DecelerationRatio;
			return timing;
		};
		std::unordered_map<const DesignerVisualStateAnimation*, uint32_t>
			sourceIndexes;
		for (uint32_t index = 0; index < sources.size(); ++index)
			sourceIndexes.emplace(sources[index], index);
		auto animationRange = [&](const auto& animations)
		{
			CompiledInteractionRange range;
			if (animations.empty()) return range;
			const auto first = sourceIndexes.find(&animations.front());
			if (first == sourceIndexes.end())
				throw std::runtime_error(
					"CUI AOT timeline-group animation is not in the source table.");
			range.Offset = first->second;
			range.Count = static_cast<uint32_t>(animations.size());
			for (uint32_t offset = 0; offset < range.Count; ++offset)
			{
				const auto found = sourceIndexes.find(&animations[offset]);
				if (found == sourceIndexes.end()
					|| found->second != range.Offset + offset)
					throw std::runtime_error(
						"CUI AOT timeline-group animations are not contiguous.");
			}
			return range;
		};
		std::function<CompiledInteractionRange(
			const std::vector<DesignerTimelineGroup>&)> appendTimelineGroups;
		appendTimelineGroups = [&](const std::vector<DesignerTimelineGroup>& groups)
		{
			CompiledInteractionRange range{
				static_cast<uint32_t>(result.TimelineGroups.size()),
				static_cast<uint32_t>(groups.size()) };
			result.TimelineGroups.resize(
				result.TimelineGroups.size() + groups.size());
			for (uint32_t index = 0; index < groups.size(); ++index)
			{
				const auto children = appendTimelineGroups(groups[index].Children);
				result.TimelineGroups[range.Offset + index] = {
					animationRange(groups[index].Animations), children,
					convertTiming(groups[index].Timing) };
			}
			return range;
		};
		if (fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "style-document")
		{
			const auto& action = document.StyleSheet.Rules.front()
				.Triggers.front().EnterActions.front();
			result.Storyboards.resize(1u);
			result.Storyboards.front().Animations =
				animationRange(action.Animations);
			result.Storyboards.front().TimelineGroups =
				appendTimelineGroups(action.TimelineGroups);
			result.Storyboards.front().Timing =
				convertTiming(action.StoryboardTiming);
			result.Actions.push_back({
				DeclarativeStoryboardActionKind::Begin, 0u });
			const auto conditionValueIndex =
				static_cast<uint32_t>(result.Values.size());
			result.Values.emplace_back(1);
			result.StylePropertyConditions.push_back({
				DependencyPropertyReference(
					AnimationDoubleProbeControl::PhaseProperty()),
				conditionValueIndex });
			result.StyleRules.push_back({
				1u, 0u, { 0u, 1u }, {}, {}, { 0u, 1u }, {} });
			result.StyleRuleIndexes.push_back(0u);
			result.StylePropertyWatchers.push_back(
				DependencyPropertyReference(
					AnimationDoubleProbeControl::PhaseProperty()));
			result.StyleGroups.push_back({
				true, UIClass::UI_CUSTOM, {}, CompiledStyleInvalidIndex,
				{ 0u, 1u }, { 0u, 1u }, {} });
			return result;
		}
		if (fixture.ReplacementTimelineXaml)
		{
			result.Storyboards.resize(sources.size());
			result.Actions.resize(sources.size());
			result.EventTriggers.resize(sources.size());
			for (std::size_t index = 0; index < sources.size(); ++index)
			{
				result.Storyboards[index].Animations = {
					static_cast<uint32_t>(index), 1u };
				if (index == 0u)
					result.Storyboards[index].Timing = fixture.StoryboardTiming;
				result.Actions[index] = {
					DeclarativeStoryboardActionKind::Begin,
					static_cast<uint32_t>(index) };
				if (fixture.Oracle == "compose-handoff" && index > 0u)
					result.Actions[index].Handoff =
						DeclarativeHandoffBehavior::Compose;
				result.EventTriggers[index] = {
					index == 0u ? &FixturePrimaryEvent()
						: &FixtureReplacementEvent(),
					RoutedEventId::None,
					{ static_cast<uint32_t>(index), 1u } };
			}
			return result;
		}

		static constexpr auto groupToken =
			MakeVisualStateGroupToken(L"FixtureStates");
		static constexpr auto idleToken = MakeVisualStateToken(L"Idle");
		static constexpr auto runningToken = MakeVisualStateToken(L"Running");
		static const DeclarativeEventDefinition runningEvent{
			BindingValueKind::Empty, DeclarativeEventRoutingStrategy::Direct };
		result.StateEvents.assign(1u, &runningEvent);
		result.States.resize(2u);
		result.Groups.resize(1u);
		auto& idle = result.States[0];
		auto& running = result.States[1];
		auto& group = result.Groups[0];
		const DesignerVisualStateGroup* parsedFixtureGroup = nullptr;
		const DesignerVisualState* parsedRunningState = nullptr;
		if (document.Components.size() == 1u)
			for (const auto& parsedGroup :
				document.Components.front().VisualStateGroups)
				if (parsedGroup.Name == L"FixtureStates")
				{
					parsedFixtureGroup = &parsedGroup;
					for (const auto& parsedState : parsedGroup.States)
						if (parsedState.Name == L"Running")
							parsedRunningState = &parsedState;
				}
		if (!parsedRunningState)
			throw std::runtime_error(
				"CUI AOT Running state structure is missing.");
		if (fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-state-document"
			&& parsedRunningState->StoryboardResourceKey
				!= Convert::Utf8ToUnicode(*fixture.StoryboardResourceKey))
			throw std::runtime_error(
				"CUI AOT state resource key was not retained.");
		idle.Token = idleToken;
		running.Token = runningToken;
		running.Events = { 0u, 1u };
		group.Token = groupToken;
		group.States = { 0u, 2u };
		group.FallbackStateIndex = 0u;
		if (fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-transition-document")
		{
			if (!parsedFixtureGroup
				|| parsedFixtureGroup->Transitions.size() != 1u)
				throw std::runtime_error(
					"CUI AOT parsed transition structure is missing.");
			const auto& source = parsedFixtureGroup->Transitions.front();
			result.Transitions.resize(1u);
			auto& transition = result.Transitions.front();
			transition.FromStateIndex = 0u;
			transition.ToStateIndex = 1u;
			transition.GeneratedDurationMilliseconds =
				source.GeneratedDurationMilliseconds;
			transition.GeneratedEasing = ToRuntimeEasing(
				source.GeneratedEasing);
			transition.GeneratedEasingMode = ToRuntimeEasingMode(
				source.GeneratedEasingMode);
			transition.GeneratedEasingParameters = {
				source.GeneratedEasingParameters.Primary,
				source.GeneratedEasingParameters.Secondary };
			transition.Animations = animationRange(source.Animations);
			transition.TimelineGroups = appendTimelineGroups(
				source.TimelineGroups);
			transition.StoryboardTiming = convertTiming(
				source.StoryboardTiming);
			group.Transitions = { 0u, 1u };
		}
		else
		{
			running.Animations = animationRange(parsedRunningState->Animations);
			running.TimelineGroups = appendTimelineGroups(
				parsedRunningState->TimelineGroups);
			running.StoryboardTiming = convertTiming(
				parsedRunningState->StoryboardTiming);
		}
		return result;
	}

	SampleResult RunAotFixtureProgram(
		const AnimationFixture& fixture,
		const AnimationSampleRequest& sample,
		const AotFixtureProgram& storage,
		Control& host,
		std::span<Control* const> targets,
		Control& observedTarget,
		const DependencyProperty& observedProperty,
		const std::function<double()>& readValue)
	{
		std::wstring error;
		const bool styleResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "style-document";
		const bool visualTransitionResource = fixture.StoryboardResourceScope
			&& *fixture.StoryboardResourceScope == "visual-transition-document";
		if (styleResource)
		{
			auto sheet = ControlStyleSheet::CreateCompiled(
				storage.StyleView(), std::vector<BindingValue>(
					storage.Values.begin(), storage.Values.end()));
			if (!sheet || !cui::framework::StyleAccess::SetDocumentStyles(
				host, std::move(sheet)))
				throw std::runtime_error(
					"CUI AOT Style fixture install failed.");
		}
		else
		{
			const auto program = storage.View();
			if (!cui::framework::TemplateAccess::InstallCompiledInteractions(
				host, program, storage.Values, targets, &error))
				throw std::runtime_error("CUI AOT fixture " + fixture.Id
					+ " install failed: "
					+ Convert::UnicodeToUtf8(error));
		}
		std::optional<DeclarativeClockObservation> clock;
		std::vector<std::string> events;
		TimingEventCollector eventCollector(host, events);
		EventConnection timingEventConnection;
		if (sample.Phase == "after-begin")
		{
			ClockOverrideScope clockScope(host, FixtureClockOrigin);
			if (fixture.Oracle == "dispatcher-events"
				|| fixture.Oracle == "authored-root-dispatcher-events")
				timingEventConnection = host.OnStoryboardTimingEvent.Subscribe(
					[&](Control* sender, const DeclarativeClockTimingEventArgs& args)
					{ eventCollector.Observe(sender, args); });
			const bool handoffOracle = fixture.Oracle == "snapshot-replace"
				|| fixture.Oracle == "compose-handoff";
			const bool began = styleResource
				? host.TrySetPropertyValue(L"Phase", BindingValue(1))
				: handoffOracle
					? host.RaiseDeclarativeEvent(FixturePrimaryEvent())
					: host.GoToVisualState(
						MakeVisualStateGroupToken(L"FixtureStates"),
						MakeVisualStateToken(L"Running"),
						visualTransitionResource, &error);
			if (!began)
				throw std::runtime_error("CUI AOT fixture begin failed: "
					+ Convert::UnicodeToUtf8(error));
			if (handoffOracle)
			{
				const auto replacementAt = fixture.Operations.front().AtMilliseconds;
				if (sample.AtMilliseconds >= replacementAt)
				{
					const auto replacementTick = FixtureClockOrigin + replacementAt;
					clockScope.Set(replacementTick);
					(void)cui::framework::PresentationAccess::
						AdvanceVisualStateAnimations(host, replacementTick);
					if (!host.RaiseDeclarativeEvent(FixtureReplacementEvent()))
						throw std::runtime_error(
							"CUI AOT fixture replacement begin failed: "
							+ Convert::UnicodeToUtf8(error));
				}
			}
			else if (fixture.Oracle == "synchronous-control"
				|| fixture.Oracle == "authored-root-control")
			{
				for (const auto& operation : fixture.Operations)
				{
					if (operation.AtMilliseconds > sample.AtMilliseconds) break;
					if (operation.Kind != "seek-aligned" || !operation.Value
						|| *operation.Value < 0.0
						|| *operation.Value
							> static_cast<double>((std::numeric_limits<
								unsigned long long>::max)())
						|| std::floor(*operation.Value) != *operation.Value)
						throw std::runtime_error(
							"CUI AOT synchronous fixture has an invalid aligned seek.");
					clockScope.Set(FixtureClockOrigin + operation.AtMilliseconds);
					if (!cui::framework::PresentationAccess::
						SeekSingleVisualStateAnimationRootAlignedForTesting(
							host, static_cast<unsigned long long>(*operation.Value)))
						throw std::runtime_error(
							"CUI AOT synchronous aligned seek failed transactionally.");
				}
			}
			else if (fixture.Oracle == "dispatcher-control"
				|| fixture.Oracle == "authored-root-dispatcher-control")
			{
				for (const auto& operation : fixture.Operations)
				{
					if (operation.AtMilliseconds > sample.AtMilliseconds) break;
					ExecuteDispatcherControlOperation(
						operation, host, clockScope,
						"CUI AOT dispatcher-control fixture");
				}
			}
			else if (fixture.Oracle == "dispatcher-events"
				|| fixture.Oracle == "authored-root-dispatcher-events")
			{
				const bool capturesBegin = std::any_of(
					fixture.Operations.begin(), fixture.Operations.end(),
					[](const auto& operation)
					{ return operation.Kind == "begin-tick"; });
				if (!capturesBegin)
				{
					clockScope.Set(FixtureClockOrigin);
					(void)cui::framework::PresentationAccess::
						AdvanceVisualStateAnimations(host, FixtureClockOrigin);
					if (cui::framework::PresentationAccess::
						VisualStateAnimationAdvanceFailedForTesting(host))
						throw std::runtime_error(
							"CUI AOT event baseline tick failed transactionally.");
					eventCollector.Clear();
				}
				for (const auto& operation : fixture.Operations)
				{
					if (operation.AtMilliseconds > sample.AtMilliseconds) break;
					ExecuteDispatcherEventOperation(
						operation, host, clockScope,
						"CUI AOT dispatcher-events fixture");
				}
			}
			const auto sampleTick = FixtureClockOrigin + sample.AtMilliseconds;
			clockScope.Set(sampleTick);
			if (fixture.Oracle != "dispatcher-control"
				&& fixture.Oracle != "authored-root-dispatcher-control"
				&& fixture.Oracle != "dispatcher-events"
				&& fixture.Oracle != "authored-root-dispatcher-events")
			{
				(void)cui::framework::PresentationAccess::
					AdvanceVisualStateAnimations(host, sampleTick);
				if (cui::framework::PresentationAccess::
					VisualStateAnimationAdvanceFailedForTesting(host))
					throw std::runtime_error(
						"CUI AOT fixture animation advance failed transactionally.");
			}
			if (fixture.Oracle == "synchronous-control"
				|| fixture.Oracle == "authored-root-control"
				|| fixture.Oracle == "dispatcher-control"
				|| fixture.Oracle == "authored-root-dispatcher-control"
				|| fixture.Oracle == "dispatcher-events"
				|| fixture.Oracle == "authored-root-dispatcher-events")
			{
				clock = cui::framework::PresentationAccess::
					QuerySingleVisualStateAnimationRootForTesting(host);
				if (!clock && (fixture.Oracle == "synchronous-control"
					|| fixture.Oracle == "authored-root-control"
					|| fixture.Oracle == "authored-root-dispatcher-control"))
					throw std::runtime_error(
						"CUI AOT synchronous root clock query failed.");
			}
		}
		const double value = readValue();
		if (!std::isfinite(value))
			throw std::runtime_error(
				"CUI AOT fixture produced a non-finite observed value.");
		return {
			sample.AtMilliseconds,
			sample.Label,
			value,
			observedTarget.GetPropertyValueSource(observedProperty)
				== DependencyPropertyValueSource::Animation,
			std::move(clock),
			std::move(events) };
	}

	SampleResult ExecuteAotSample(
		const AnimationFixture& fixture,
		const AnimationSampleRequest& sample)
	{
		DesignerModel::DesignDocument document;
		const auto sources = ParsedFixtureAnimations(fixture, document);
		const auto storage = BuildAotFixtureProgram(fixture, sources, document);
		if (fixture.Target.Probe == "metadata-double")
		{
			AnimationDoubleProbeControl host;
			if (!host.SetValue(fixture.Target.BaseValue))
				throw std::runtime_error(
					"CUI AOT metadata-double base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			return RunAotFixtureProgram(
				fixture, sample, storage, host, targets, host,
				AnimationDoubleProbeControl::ValueProperty(),
				[&] { return host.Value(); });
		}
		if (fixture.Target.Probe == "metadata-int32")
		{
			AnimationInt32ProbeControl host;
			if (!fixture.Target.BaseInt32 || !host.SetValue(*fixture.Target.BaseInt32))
				throw std::runtime_error(
					"CUI AOT metadata-int32 base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(fixture, sample, storage, host,
				targets, host, AnimationInt32ProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value()); });
			result.Int32Value = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-int64")
		{
			AnimationInt64ProbeControl host;
			if (!fixture.Target.BaseInt64 || !host.SetValue(*fixture.Target.BaseInt64))
				throw std::runtime_error(
					"CUI AOT metadata-int64 base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(fixture, sample, storage, host,
				targets, host, AnimationInt64ProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value()); });
			result.Int64Value = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-single")
		{
			AnimationSingleProbeControl host;
			if (!fixture.Target.BaseSingle || !host.SetValue(*fixture.Target.BaseSingle))
				throw std::runtime_error(
					"CUI AOT metadata-single base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(fixture, sample, storage, host,
				targets, host, AnimationSingleProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value()); });
			result.SingleValue = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-boolean")
		{
			AnimationBooleanProbeControl host;
			if (!fixture.Target.BaseBoolean || !host.SetValue(*fixture.Target.BaseBoolean))
				throw std::runtime_error(
					"CUI AOT metadata-boolean base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(fixture, sample, storage, host,
				targets, host, AnimationBooleanProbeControl::ValueProperty(),
				[&] { return host.Value() ? 1.0 : 0.0; });
			result.BooleanValue = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-string")
		{
			AnimationStringProbeControl host;
			if (!fixture.Target.BaseString
				|| !host.SetValue(Convert::Utf8ToUnicode(*fixture.Target.BaseString)))
				throw std::runtime_error(
					"CUI AOT metadata-string base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(fixture, sample, storage, host,
				targets, host, AnimationStringProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value().size()); });
			result.StringValue = Convert::UnicodeToUtf8(host.Value());
			return result;
		}
		if (fixture.Target.Probe == "metadata-color")
		{
			AnimationColorProbeControl host;
			if (!fixture.Target.BaseColor || !host.SetValue(*fixture.Target.BaseColor))
				throw std::runtime_error(
					"CUI AOT metadata-color base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, host,
				AnimationColorProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value().r); });
			result.ColorValue = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-vector")
		{
			AnimationVectorProbeControl host;
			if (!fixture.Target.BaseVector || !host.SetValue(*fixture.Target.BaseVector))
				throw std::runtime_error(
					"CUI AOT metadata-vector base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, host,
				AnimationVectorProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value().x); });
			result.VectorValue = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-rect")
		{
			AnimationRectProbeControl host;
			if (!fixture.Target.BaseRect || !host.SetValue(*fixture.Target.BaseRect))
				throw std::runtime_error(
					"CUI AOT metadata-rect base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, host,
				AnimationRectProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value().x); });
			result.RectValue = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-size")
		{
			AnimationSizeProbeControl host;
			if (!fixture.Target.BaseSize || !host.SetValue(*fixture.Target.BaseSize))
				throw std::runtime_error(
					"CUI AOT metadata-size base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, host,
				AnimationSizeProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value().width); });
			result.SizeValue = host.Value();
			return result;
		}
		if (fixture.Target.Probe == "metadata-matrix")
		{
			AnimationMatrixProbeControl host;
			if (!fixture.Target.BaseMatrix || !host.SetValue(*fixture.Target.BaseMatrix))
				throw std::runtime_error(
					"CUI AOT metadata-matrix base value was rejected.");
			std::array<Control*, 1> targets{ &host };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, host,
				AnimationMatrixProbeControl::ValueProperty(),
				[&] { return static_cast<double>(host.Value()._11); });
			result.MatrixValue = host.Value();
			return result;
		}
		if (const auto* transformLeaf =
			FindTransformLeafProbe(fixture.Target.Probe))
		{
			Border target;
			Canvas host;
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Transform leaf target registration failed.");

			cui::drawing::TransformOperation operation;
			operation.Kind = transformLeaf->TransformKind;
			switch (transformLeaf->Member)
			{
			case CompiledStoryboardObjectPathMember::TransformX:
				operation.X = static_cast<float>(fixture.Target.BaseValue);
				operation.Y = -3.25f;
				break;
			case CompiledStoryboardObjectPathMember::TransformY:
				operation.X = 2.25f;
				operation.Y = static_cast<float>(fixture.Target.BaseValue);
				break;
			case CompiledStoryboardObjectPathMember::TransformScaleY:
				operation.ScaleX = 1.25f;
				operation.ScaleY = static_cast<float>(fixture.Target.BaseValue);
				operation.CenterX = 0.5f;
				operation.CenterY = 0.25f;
				break;
			case CompiledStoryboardObjectPathMember::TransformCenterX:
				operation.ScaleX = 1.25f;
				operation.ScaleY = 0.75f;
				operation.Angle = 17.5f;
				operation.AngleX = 12.5f;
				operation.AngleY = -7.25f;
				operation.CenterX = static_cast<float>(fixture.Target.BaseValue);
				operation.CenterY = -0.25f;
				break;
			case CompiledStoryboardObjectPathMember::TransformCenterY:
				operation.ScaleX = 1.25f;
				operation.ScaleY = 0.75f;
				operation.Angle = 17.5f;
				operation.AngleX = 12.5f;
				operation.AngleY = -7.25f;
				operation.CenterX = 0.5f;
				operation.CenterY = static_cast<float>(fixture.Target.BaseValue);
				break;
			case CompiledStoryboardObjectPathMember::TransformAngleX:
				operation.AngleX = static_cast<float>(fixture.Target.BaseValue);
				operation.AngleY = -7.25f;
				operation.CenterX = 0.5f;
				operation.CenterY = -0.25f;
				break;
			case CompiledStoryboardObjectPathMember::TransformAngleY:
				operation.AngleX = 12.5f;
				operation.AngleY = static_cast<float>(fixture.Target.BaseValue);
				operation.CenterX = 0.5f;
				operation.CenterY = -0.25f;
				break;
			case CompiledStoryboardObjectPathMember::TransformMatrix:
				operation.Matrix = *fixture.Target.BaseMatrix;
				break;
			default:
				throw std::runtime_error(
					"CUI AOT Transform leaf member is unsupported.");
			}
			cui::drawing::Transform transform;
			for (uint32_t index = 0; index < transformLeaf->OperationIndex; ++index)
			{
				cui::drawing::TransformOperation dummy;
				dummy.Kind = cui::drawing::TransformKind::Translate;
				dummy.X = static_cast<float>(100u + index);
				dummy.Y = -static_cast<float>(100u + index);
				transform.Operations.push_back(dummy);
			}
			transform.Operations.push_back(operation);

			const DependencyProperty* property = nullptr;
			switch (transformLeaf->Root)
			{
			case TransformLeafProbeRoot::RenderTransform:
				target.SetRenderTransform(transform);
				property = &Control::RenderTransformProperty();
				break;
			case TransformLeafProbeRoot::GeometryTransformDirect:
			case TransformLeafProbeRoot::GeometryTransformChild10:
			{
				cui::drawing::Geometry geometry;
				geometry.Kind = cui::drawing::GeometryKind::Rectangle;
				geometry.Rect = D2D1::RectF(0.0f, 0.0f, 100.0f, 60.0f);
				geometry.LocalTransform = transform;
				if (transformLeaf->Root
					== TransformLeafProbeRoot::GeometryTransformChild10)
				{
					cui::drawing::Geometry inner;
					inner.Kind = cui::drawing::GeometryKind::Group;
					inner.Children.push_back(std::move(geometry));
					cui::drawing::Geometry outer;
					outer.Kind = cui::drawing::GeometryKind::Group;
					cui::drawing::Geometry dummy;
					dummy.Kind = cui::drawing::GeometryKind::Rectangle;
					dummy.Rect = D2D1::RectF(0.0f, 0.0f, 3.0f, 4.0f);
					outer.Children.push_back(std::move(dummy));
					outer.Children.push_back(std::move(inner));
					geometry = std::move(outer);
				}
				target.SetClip(geometry);
				property = &Control::ClipProperty();
				break;
			}
			case TransformLeafProbeRoot::BrushTransform:
			{
				auto brush = cui::drawing::MakeSolidColorBrush(
					D2D1::ColorF(0x336699));
				brush.Transform = transform;
				target.SetBackground(std::move(brush));
				property = &Control::BackgroundProperty();
				break;
			}
			case TransformLeafProbeRoot::BrushRelativeTransform:
			{
				cui::drawing::Brush brush;
				brush.Kind = cui::drawing::BrushKind::LinearGradient;
				brush.GradientStops = {
					{ 0.0f, D2D1::ColorF(0x102030) },
					{ 1.0f, D2D1::ColorF(0xD0E0F0) } };
				brush.RelativeTransform = transform;
				target.SetBackground(std::move(brush));
				property = &Control::BackgroundProperty();
				break;
			}
			default:
				throw std::runtime_error("CUI AOT Transform leaf root is invalid.");
			}

			auto read = [&](std::optional<D2D1_MATRIX_3X2_F>* matrix = nullptr)
			{
				switch (transformLeaf->Root)
				{
				case TransformLeafProbeRoot::RenderTransform:
					return ReadTransformLeafProbe(
						*target.GetRenderTransform(), *transformLeaf, matrix);
				case TransformLeafProbeRoot::GeometryTransformDirect:
				case TransformLeafProbeRoot::GeometryTransformChild10:
				{
					const auto& clip = *target.GetClip();
					const cui::drawing::Geometry* geometry = &clip;
					for (const auto index : TransformLeafChildIndices(*transformLeaf))
						geometry = &geometry->Children[index];
					return ReadTransformLeafProbe(
						*geometry->LocalTransform, *transformLeaf, matrix);
				}
				case TransformLeafProbeRoot::BrushTransform:
				case TransformLeafProbeRoot::BrushRelativeTransform:
				{
					const auto brush = target.GetComputedBackgroundBrush();
					const auto& current = transformLeaf->Root
						== TransformLeafProbeRoot::BrushRelativeTransform
						? brush.RelativeTransform : brush.Transform;
					return ReadTransformLeafProbe(*current, *transformLeaf, matrix);
				}
				default:
					return (std::numeric_limits<double>::quiet_NaN)();
				}
			};
			std::array<Control*, 2> targets{ &host, &target };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target, *property,
				[&] { return read(); });
			if (transformLeaf->TargetKind == "matrix")
			{
				std::optional<D2D1_MATRIX_3X2_F> matrix;
				(void)read(&matrix);
				result.MatrixValue = matrix;
			}
			return result;
		}
		if (const auto* geometryLeaf = FindGeometryLeafProbe(fixture.Target.Probe))
		{
			cui::drawing::Geometry leaf;
			switch (geometryLeaf->Graph)
			{
			case GeometryLeafProbeGraph::RectangleDirect:
			case GeometryLeafProbeGraph::RectangleChild0:
			{
				leaf.Kind = cui::drawing::GeometryKind::Rectangle;
				const auto rect = geometryLeaf->Value
					== GeometryLeafProbeValue::RectangleRect
					? *fixture.Target.BaseRect
					: cui::core::Rect{ 1.0f, 2.0f, 30.0f, 40.0f };
				leaf.Rect = D2D1::RectF(rect.x, rect.y,
					rect.x + rect.width, rect.y + rect.height);
				leaf.RadiusX = geometryLeaf->Value
					== GeometryLeafProbeValue::RectangleRadiusX
					? static_cast<float>(fixture.Target.BaseValue) : 3.5f;
				leaf.RadiusY = geometryLeaf->Value
					== GeometryLeafProbeValue::RectangleRadiusY
					? static_cast<float>(fixture.Target.BaseValue) : 4.5f;
				break;
			}
			case GeometryLeafProbeGraph::EllipseDirect:
			case GeometryLeafProbeGraph::EllipseChild1:
			case GeometryLeafProbeGraph::EllipseChild10:
				leaf.Kind = cui::drawing::GeometryKind::Ellipse;
				leaf.Center = D2D1::Point2F(
					geometryLeaf->Value == GeometryLeafProbeValue::EllipseCenterX
						? static_cast<float>(fixture.Target.BaseValue) : 20.5f,
					30.5f);
				leaf.RadiusX = geometryLeaf->Value
					== GeometryLeafProbeValue::EllipseRadiusX
					? static_cast<float>(fixture.Target.BaseValue) : 8.5f;
				leaf.RadiusY = geometryLeaf->Value
					== GeometryLeafProbeValue::EllipseRadiusY
					? static_cast<float>(fixture.Target.BaseValue) : 9.5f;
				break;
			case GeometryLeafProbeGraph::GroupDirect:
			{
				leaf.Kind = cui::drawing::GeometryKind::Group;
				leaf.FillRule = *fixture.Target.BaseString == "EvenOdd"
					? cui::drawing::GeometryFillRule::EvenOdd
					: cui::drawing::GeometryFillRule::Nonzero;
				cui::drawing::Geometry rectangle;
				rectangle.Kind = cui::drawing::GeometryKind::Rectangle;
				rectangle.Rect = D2D1::RectF(0, 0, 10, 10);
				cui::drawing::Geometry ellipse;
				ellipse.Kind = cui::drawing::GeometryKind::Ellipse;
				ellipse.Center = D2D1::Point2F(20, 20);
				ellipse.RadiusX = 5; ellipse.RadiusY = 6;
				leaf.Children = { rectangle, ellipse };
				break;
			}
			default:
			{
				leaf.Kind = cui::drawing::GeometryKind::Path;
				leaf.FillRule = geometryLeaf->Value == GeometryLeafProbeValue::FillRule
					&& *fixture.Target.BaseString == "EvenOdd"
					? cui::drawing::GeometryFillRule::EvenOdd
					: cui::drawing::GeometryFillRule::Nonzero;
				cui::drawing::PathFigure figure;
				figure.StartPoint = D2D1::Point2F(
					geometryLeaf->Value == GeometryLeafProbeValue::FigureStartPointX
						? static_cast<float>(fixture.Target.BaseValue) : 1.0f, 2.0f);
				figure.IsClosed = geometryLeaf->Value
					== GeometryLeafProbeValue::FigureIsClosed
					? *fixture.Target.BaseBoolean : false;
				figure.IsFilled = geometryLeaf->Value
					== GeometryLeafProbeValue::FigureIsFilled
					? *fixture.Target.BaseBoolean : true;
				cui::drawing::PathSegment line;
				line.Kind = cui::drawing::PathSegmentKind::Line;
				line.Point = D2D1::Point2F(10.0f,
					geometryLeaf->Value == GeometryLeafProbeValue::LinePointY
						? static_cast<float>(fixture.Target.BaseValue) : 12.0f);
				cui::drawing::PathSegment bezier;
				bezier.Kind = cui::drawing::PathSegmentKind::Bezier;
				bezier.Point1 = D2D1::Point2F(
					geometryLeaf->Value == GeometryLeafProbeValue::BezierPoint1X
						? static_cast<float>(fixture.Target.BaseValue) : 21.0f, 22.0f);
				bezier.Point2 = D2D1::Point2F(23.0f,
					geometryLeaf->Value == GeometryLeafProbeValue::BezierPoint2Y
						? static_cast<float>(fixture.Target.BaseValue) : 24.0f);
				bezier.Point3 = D2D1::Point2F(
					geometryLeaf->Value == GeometryLeafProbeValue::BezierPoint3X
						? static_cast<float>(fixture.Target.BaseValue) : 25.0f, 26.0f);
				cui::drawing::PathSegment quadratic;
				quadratic.Kind = cui::drawing::PathSegmentKind::QuadraticBezier;
				quadratic.Point1 = D2D1::Point2F(30.0f,
					geometryLeaf->Value == GeometryLeafProbeValue::QuadraticPoint1Y
						? static_cast<float>(fixture.Target.BaseValue) : 31.0f);
				quadratic.Point2 = D2D1::Point2F(
					geometryLeaf->Value == GeometryLeafProbeValue::QuadraticPoint2X
						? static_cast<float>(fixture.Target.BaseValue) : 33.0f, 34.0f);
				cui::drawing::PathSegment arc;
				arc.Kind = cui::drawing::PathSegmentKind::Arc;
				arc.Point = D2D1::Point2F(40.0f,
					geometryLeaf->Value == GeometryLeafProbeValue::ArcPointY
						? static_cast<float>(fixture.Target.BaseValue) : 41.0f);
				const auto arcSize = geometryLeaf->Value == GeometryLeafProbeValue::ArcSize
					? *fixture.Target.BaseSize : cui::core::Size{ 11.0f, 12.0f };
				arc.Size = D2D1::SizeF(arcSize.width, arcSize.height);
				arc.RotationAngle = geometryLeaf->Value
					== GeometryLeafProbeValue::ArcRotationAngle
					? static_cast<float>(fixture.Target.BaseValue) : 25.0f;
				arc.IsLargeArc = geometryLeaf->Value
					== GeometryLeafProbeValue::ArcIsLargeArc
					? *fixture.Target.BaseBoolean : false;
				arc.Sweep = geometryLeaf->Value
					== GeometryLeafProbeValue::ArcSweepDirection
					&& *fixture.Target.BaseString == "Clockwise"
					? cui::drawing::SweepDirection::Clockwise
					: cui::drawing::SweepDirection::Counterclockwise;
				figure.Segments = { line, bezier, quadratic, arc };
				leaf.Figures.push_back(std::move(figure));
				break;
			}
			}
			auto wrapAt = [](cui::drawing::Geometry current,
				const std::vector<uint32_t>& indices)
			{
				for (auto item = indices.rbegin(); item != indices.rend(); ++item)
				{
					cui::drawing::Geometry group;
					group.Kind = cui::drawing::GeometryKind::Group;
					for (uint32_t index = 0; index < *item; ++index)
					{
						cui::drawing::Geometry dummy;
						dummy.Kind = cui::drawing::GeometryKind::Rectangle;
						dummy.Rect = D2D1::RectF(
							static_cast<float>(index), static_cast<float>(index),
							static_cast<float>(index + 3u),
							static_cast<float>(index + 4u));
						group.Children.push_back(std::move(dummy));
					}
					group.Children.push_back(std::move(current));
					current = std::move(group);
				}
				return current;
			};
			auto root = wrapAt(std::move(leaf),
				GeometryLeafChildIndices(*geometryLeaf));
			Canvas target;
			Canvas host;
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Geometry leaf target registration failed.");
			target.SetClip(root);
			std::array<Control*, 2> targets{ &host, &target };
			auto read = [&]
			{
				const auto& clip = target.GetClip();
				if (!clip) return (std::numeric_limits<double>::quiet_NaN)();
				return ReadGeometryLeafProbe(*clip, *geometryLeaf);
			};
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target,
				Control::ClipProperty(), read);
			const auto& clip = target.GetClip();
			if (!clip)
				throw std::runtime_error("CUI AOT Geometry leaf lost its Clip.");
			(void)ReadGeometryLeafProbe(*clip, *geometryLeaf,
				&result.RectValue, &result.SizeValue,
				&result.BooleanValue, &result.StringValue);
			return result;
		}
		if (const auto* brushLeaf = FindBrushLeafProbe(fixture.Target.Probe))
		{
			Canvas canvasTarget;
			Border borderTarget;
			Canvas host;
			Control& target = brushLeaf->BorderTarget
				? static_cast<Control&>(borderTarget)
				: static_cast<Control&>(canvasTarget);
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Brush leaf target registration failed.");

			cui::drawing::Brush brush;
			brush.Opacity = 1.0f;
			switch (brushLeaf->Graph)
			{
			case BrushLeafProbeGraph::Solid:
				brush.Kind = cui::drawing::BrushKind::Solid;
				brush.Color = BrushLeafUsesColor(*brushLeaf)
					? *fixture.Target.BaseColor
					: D2D1::ColorF(0x2864A0);
				if (brushLeaf->Value == BrushLeafProbeValue::Opacity)
					brush.Opacity = static_cast<float>(fixture.Target.BaseValue);
				break;
			case BrushLeafProbeGraph::Linear:
				brush.Kind = cui::drawing::BrushKind::LinearGradient;
				brush.StartPoint = D2D1::Point2F(
					brushLeaf->Value == BrushLeafProbeValue::StartPointX
						? static_cast<float>(fixture.Target.BaseValue) : 0.13f,
					brushLeaf->Value == BrushLeafProbeValue::StartPointX
						? 0.27f : 0.27f);
				brush.EndPoint = D2D1::Point2F(0.74f,
					brushLeaf->Value == BrushLeafProbeValue::EndPointY
						? static_cast<float>(fixture.Target.BaseValue) : 0.86f);
				brush.GradientStops = {
					{ 0.0f, brushLeaf->Value
						== BrushLeafProbeValue::GradientStopColor
						? *fixture.Target.BaseColor : D2D1::ColorF(0x103050) },
					{ 0.9f, D2D1::ColorF(0xC0D0E0) } };
				break;
			case BrushLeafProbeGraph::Radial:
				brush.Kind = cui::drawing::BrushKind::RadialGradient;
				brush.Center = D2D1::Point2F(
					brushLeaf->Value == BrushLeafProbeValue::CenterX
						? static_cast<float>(fixture.Target.BaseValue) : 0.43f,
					0.57f);
				brush.GradientOrigin = D2D1::Point2F(0.31f,
					brushLeaf->Value == BrushLeafProbeValue::GradientOriginY
						? static_cast<float>(fixture.Target.BaseValue) : 0.69f);
				brush.RadiusX = brushLeaf->Value == BrushLeafProbeValue::RadiusX
					? static_cast<float>(fixture.Target.BaseValue) : 0.81f;
				brush.RadiusY = brushLeaf->Value == BrushLeafProbeValue::RadiusY
					? static_cast<float>(fixture.Target.BaseValue) : 0.63f;
				brush.GradientStops = {
					{ 0.1f, D2D1::ColorF(0x204060) },
					{ brushLeaf->Value == BrushLeafProbeValue::GradientStopOffset
						? static_cast<float>(fixture.Target.BaseValue) : 1.0f,
						D2D1::ColorF(0xB07030) } };
				break;
			default:
				throw std::runtime_error(
					"CUI AOT Brush leaf graph kind is invalid.");
			}
			if (brushLeaf->RootProperty == L"BorderBrush")
				target.SetBorderBrush(std::move(brush));
			else
				target.SetBackground(std::move(brush));
			std::array<Control*, 2> targets{ &host, &target };
			const auto& property = brushLeaf->RootProperty == L"BorderBrush"
				? Control::BorderBrushProperty() : Control::BackgroundProperty();
			auto readBrush = [&]
			{
				return brushLeaf->RootProperty == L"BorderBrush"
					? target.GetComputedBorderBrush()
					: target.GetComputedBackgroundBrush();
			};
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target, property,
				[&] { return ReadBrushLeafProbe(readBrush(), *brushLeaf); });
			if (BrushLeafUsesColor(*brushLeaf))
			{
				std::optional<D2D1_COLOR_F> color;
				(void)ReadBrushLeafProbe(readBrush(), *brushLeaf, &color);
				result.ColorValue = color;
			}
			return result;
		}
		if (fixture.Target.Probe == "geometry-transform-direct-x")
		{
			Canvas target;
			Canvas host;
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Geometry direct template target registration failed.");
			cui::drawing::Geometry geometry;
			geometry.Kind = cui::drawing::GeometryKind::Rectangle;
			geometry.Rect = D2D1::RectF(0.0f, 0.0f, 100.0f, 60.0f);
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation operation;
			operation.Kind = cui::drawing::TransformKind::Translate;
			operation.X = static_cast<float>(fixture.Target.BaseValue);
			operation.Y = -3.25f;
			transform.Operations.push_back(operation);
			geometry.LocalTransform = std::move(transform);
			target.SetClip(geometry);
			std::array<Control*, 2> targets{ &host, &target };
			return RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target,
				Control::ClipProperty(), [&]
				{
					const auto& clip = target.GetClip();
					return clip && clip->LocalTransform
						&& clip->LocalTransform->Operations.size() == 1u
						? static_cast<double>(
							clip->LocalTransform->Operations[0].X)
						: (std::numeric_limits<double>::quiet_NaN)();
				});
		}
		if (fixture.Target.Probe == "brush-transform-direct-angle")
		{
			Canvas target;
			Canvas host;
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Brush direct template target registration failed.");
			auto brush = cui::drawing::MakeSolidColorBrush(
				D2D1::ColorF(0x336699));
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation operation;
			operation.Kind = cui::drawing::TransformKind::Rotate;
			operation.Angle = static_cast<float>(fixture.Target.BaseValue);
			operation.CenterX = 0.25f;
			operation.CenterY = 0.75f;
			transform.Operations.push_back(operation);
			brush.Transform = std::move(transform);
			target.SetBackground(std::move(brush));
			std::array<Control*, 2> targets{ &host, &target };
			return RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target,
				Control::BackgroundProperty(), [&]
				{
					const auto current = target.GetComputedBackgroundBrush();
					return current.Transform
						&& current.Transform->Operations.size() == 1u
						? static_cast<double>(
							current.Transform->Operations[0].Angle)
						: (std::numeric_limits<double>::quiet_NaN)();
				});
		}
		if (fixture.Target.Probe
			== "brush-relative-transform-direct-scale-x")
		{
			Border target;
			Canvas host;
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Brush relative direct target registration failed.");
			cui::drawing::Brush brush;
			brush.Kind = cui::drawing::BrushKind::LinearGradient;
			brush.GradientStops = {
				{ 0.0f, D2D1::ColorF(0x102030) },
				{ 1.0f, D2D1::ColorF(0xD0E0F0) } };
			cui::drawing::Transform transform;
			cui::drawing::TransformOperation operation;
			operation.Kind = cui::drawing::TransformKind::Scale;
			operation.ScaleX = static_cast<float>(fixture.Target.BaseValue);
			operation.ScaleY = 0.75f;
			operation.CenterX = 0.5f;
			operation.CenterY = 0.25f;
			transform.Operations.push_back(operation);
			brush.RelativeTransform = std::move(transform);
			target.SetBackground(std::move(brush));
			std::array<Control*, 2> targets{ &host, &target };
			return RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target,
				Control::BackgroundProperty(), [&]
				{
					const auto current = target.GetComputedBackgroundBrush();
					return current.RelativeTransform
						&& current.RelativeTransform->Operations.size() == 1u
						? static_cast<double>(current.RelativeTransform
							->Operations[0].ScaleX)
						: (std::numeric_limits<double>::quiet_NaN)();
				});
		}
		if (fixture.Target.Probe == "border-thickness-left")
		{
			Border target;
			Canvas host;
			cui::framework::XamlAccess::SetTemplatedParent(target, &host);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(
				host, MakeTemplatePartToken(L"target"), &target))
				throw std::runtime_error(
					"CUI AOT Thickness template target registration failed.");
			target.BorderThickness = Thickness(
				static_cast<float>(fixture.Target.BaseValue));
			std::array<Control*, 2> targets{ &host, &target };
			auto result = RunAotFixtureProgram(
				fixture, sample, storage, host, targets, target,
				Border::BorderThicknessProperty(),
				[&] { return static_cast<double>(target.BorderThickness.Left); });
			result.ThicknessValue = target.BorderThickness;
			return result;
		}

		Canvas target;
		Canvas host;
		cui::framework::XamlAccess::SetTemplatedParent(target, &host);
		if (!cui::framework::TemplateAccess::RegisterTemplatePart(
			host, MakeTemplatePartToken(L"target"), &target))
			throw std::runtime_error(
				"CUI AOT template target registration failed.");
		const bool pointProjection = fixture.Target.Probe == "render-transform-origin-x"
			|| fixture.Target.Probe == "render-transform-origin-y";
		if (pointProjection)
			target.SetRenderTransformOriginDip(cui::core::Point{
				static_cast<float>(fixture.Target.BaseValue),
				static_cast<float>(fixture.Target.BaseValue) });
		else
			Canvas::SetLeft(target, static_cast<float>(fixture.Target.BaseValue));
		std::array<Control*, 2> targets{ &host, &target };
		return RunAotFixtureProgram(
			fixture, sample, storage, host, targets, target,
			pointProjection ? Control::RenderTransformOriginProperty()
				: Control::CanvasLeftProperty(),
			[&]
			{
				return pointProjection
					? static_cast<double>(fixture.Target.Probe
						== "render-transform-origin-y"
							? target.GetRenderTransformOriginDip().y
							: target.GetRenderTransformOriginDip().x)
					: static_cast<double>(Canvas::GetLeft(target));
			});
	}

	std::vector<SampleResult> ExecuteSequentialSamples(
		const AnimationFixture& fixture)
	{
		DesignerModel::DesignDocument document;
		std::wstring error;
		if (!DesignerModel::XamlDocumentParser::FromXaml(
			BuildRuntimeXaml(fixture), document, &error))
			throw std::runtime_error("CUI sequential fixture XAML parse failed: "
				+ Convert::UnicodeToUtf8(error));
		DesignerModel::RuntimeDocument runtime;
		if (!DesignerModel::RuntimeDocumentLoader::Load(
			document, runtime, {}, &error))
			throw std::runtime_error("CUI sequential fixture materialization failed: "
				+ Convert::UnicodeToUtf8(error));
		auto* host = runtime.FindControlByName(L"fixtureHost");
		auto* target = host
			? host->FindDeclarativeTemplatePart(L"target") : nullptr;
		if (!host || !target)
			throw std::runtime_error(
				"CUI sequential fixture did not produce its host and target.");

		ClockOverrideScope clockScope(*host, FixtureClockOrigin);
		if (!host->GoToVisualState(
			L"FixtureStates", L"Running", false, &error))
			throw std::runtime_error("CUI sequential fixture begin failed: "
				+ Convert::UnicodeToUtf8(error));
		std::vector<SampleResult> results;
		results.reserve(fixture.Samples.size());
		for (const auto& sample : fixture.Samples)
		{
			if (sample.Phase != "after-begin")
				throw std::runtime_error(
					"Sequential fixture samples must occur after Begin.");
			const auto sampleTick = FixtureClockOrigin + sample.AtMilliseconds;
			clockScope.Set(sampleTick);
			(void)cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(*host, sampleTick);
			if (cui::framework::PresentationAccess::
				VisualStateAnimationAdvanceFailedForTesting(*host))
				throw std::runtime_error(
					"CUI sequential fixture advance failed transactionally.");
			results.push_back(
				ObserveDesignSample(fixture, sample, *host, *target));
		}
		return results;
	}

	FixtureResult ExecuteFixture(const AnimationFixture& fixture)
	{
		if (fixture.CuiSupport != "supported")
			return FixtureResult{ fixture.Id, fixture.CuiSupport, std::nullopt, {} };
		if (!fixture.Operations.empty()
			&& fixture.Oracle != "snapshot-replace"
			&& fixture.Oracle != "compose-handoff"
			&& fixture.Oracle != "synchronous-control"
			&& fixture.Oracle != "authored-root-control"
			&& fixture.Oracle != "dispatcher-control"
			&& fixture.Oracle != "authored-root-dispatcher-control"
			&& fixture.Oracle != "dispatcher-events"
			&& fixture.Oracle != "authored-root-dispatcher-events")
			return FixtureResult{
				fixture.Id,
				"failed",
				"The deterministic CUI value runner does not execute control operations.",
				{} };
		try
		{
			FixtureResult result{ fixture.Id, "passed", std::nullopt, {} };
			result.Samples.reserve(fixture.Samples.size());
			for (const auto& sample : fixture.Samples)
			{
				const auto design = ExecuteSample(fixture, sample);
				const auto aot = ExecuteAotSample(fixture, sample);
				if (design.AtMilliseconds != aot.AtMilliseconds
					|| design.Label != aot.Label
					|| design.IsAnimated != aot.IsAnimated
					|| design.Clock != aot.Clock
					|| design.Events != aot.Events
					|| design.ThicknessValue != aot.ThicknessValue
					|| !EqualOptionalColor(design.ColorValue, aot.ColorValue)
					|| !EqualOptionalVector(design.VectorValue, aot.VectorValue)
					|| !EqualOptionalRect(design.RectValue, aot.RectValue)
					|| !EqualOptionalSize(design.SizeValue, aot.SizeValue)
					|| !EqualOptionalMatrix(design.MatrixValue, aot.MatrixValue)
					|| design.Int32Value != aot.Int32Value
					|| design.Int64Value != aot.Int64Value
					|| design.SingleValue != aot.SingleValue
					|| design.BooleanValue != aot.BooleanValue
					|| design.StringValue != aot.StringValue
					|| std::abs(design.Value - aot.Value) > 1e-12)
					throw std::runtime_error(
						"CUI Design/AOT fixture observations diverged at sample "
						+ sample.Label + " (Design value="
						+ FormatDouble(design.Value) + ", animated="
						+ (design.IsAnimated ? "true" : "false")
						+ "; AOT value=" + FormatDouble(aot.Value)
						+ ", animated="
						+ (aot.IsAnimated ? "true" : "false") + ").");
				result.Samples.push_back(design);
			}
			return result;
		}
		catch (const std::exception& error)
		{
			return FixtureResult{ fixture.Id, "failed", error.what(), {} };
		}
		catch (...)
		{
			return FixtureResult{
				fixture.Id, "failed", "Unknown CUI fixture execution error.", {} };
		}
	}

	AnimationResultDocument ExecuteCorpus(
		const AnimationCorpus& corpus,
		const std::optional<std::string>& exactFixtureId)
	{
		AnimationResultDocument result{ corpus.CorpusSha256, {} };
		for (const auto& fixture : corpus.Fixtures)
			if (!exactFixtureId || fixture.Id == *exactFixtureId)
				result.Fixtures.push_back(ExecuteFixture(fixture));
		if (exactFixtureId && result.Fixtures.empty())
			throw std::runtime_error("Fixture id not found: "
				+ *exactFixtureId + ".");
		return result;
	}

	std::string JsonEscape(std::string_view value)
	{
		static constexpr char hex[] = "0123456789abcdef";
		std::string result;
		result.reserve(value.size() + 2);
		result.push_back('"');
		for (const auto character : value)
		{
			const auto byte = static_cast<unsigned char>(character);
			switch (byte)
			{
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if (byte < 0x20)
				{
					result += "\\u00";
					result.push_back(hex[(byte >> 4) & 0x0F]);
					result.push_back(hex[byte & 0x0F]);
				}
				else result.push_back(character);
				break;
			}
		}
		result.push_back('"');
		return result;
	}

	std::string ProcessArchitecture()
	{
#if defined(_M_X64)
		return "x64";
#elif defined(_M_IX86)
		return "x86";
#elif defined(_M_ARM64)
		return "arm64";
#else
		return "unknown";
#endif
	}

	std::string SerializeResult(const AnimationResultDocument& document)
	{
		std::string json;
		json += "{\n";
		json += "  \"schemaVersion\": 1,\n";
		json += "  \"runnerVersion\": " + JsonEscape(RunnerVersion) + ",\n";
		json += "  \"corpusSha256\": "
			+ JsonEscape(document.CorpusSha256) + ",\n";
		json += "  \"engine\": {\n";
		json += "    \"name\": \"CUI\",\n";
		json += "    \"targetFramework\": null,\n";
		json += "    \"runtimeVersion\": null,\n";
		json += "    \"presentationFrameworkVersion\": null,\n";
		json += "    \"os\": \"Windows\",\n";
		json += "    \"processArchitecture\": "
			+ JsonEscape(ProcessArchitecture()) + "\n";
		json += "  },\n";
		json += "  \"fixtures\": [";
		if (!document.Fixtures.empty()) json += "\n";
		for (std::size_t fixtureIndex = 0;
			fixtureIndex < document.Fixtures.size(); ++fixtureIndex)
		{
			const auto& fixture = document.Fixtures[fixtureIndex];
			json += "    {\n";
			json += "      \"id\": " + JsonEscape(fixture.Id) + ",\n";
			json += "      \"status\": " + JsonEscape(fixture.Status) + ",\n";
			json += "      \"error\": ";
			json += fixture.Error ? JsonEscape(*fixture.Error) : "null";
			json += ",\n";
			json += "      \"samples\": [";
			if (!fixture.Samples.empty()) json += "\n";
			for (std::size_t sampleIndex = 0;
				sampleIndex < fixture.Samples.size(); ++sampleIndex)
			{
				const auto& sample = fixture.Samples[sampleIndex];
				json += "        {\n";
				json += "          \"atMilliseconds\": "
					+ std::to_string(sample.AtMilliseconds) + ",\n";
				json += "          \"label\": " + JsonEscape(sample.Label) + ",\n";
				json += "          \"value\": {\n";
				if (sample.MatrixValue)
				{
					json += "            \"kind\": \"matrix\",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": {\n";
					json += "              \"m11\": "
						+ FormatDouble(sample.MatrixValue->_11) + ",\n";
					json += "              \"m12\": "
						+ FormatDouble(sample.MatrixValue->_12) + ",\n";
					json += "              \"m21\": "
						+ FormatDouble(sample.MatrixValue->_21) + ",\n";
					json += "              \"m22\": "
						+ FormatDouble(sample.MatrixValue->_22) + ",\n";
					json += "              \"offsetX\": "
						+ FormatDouble(sample.MatrixValue->_31) + ",\n";
					json += "              \"offsetY\": "
						+ FormatDouble(sample.MatrixValue->_32) + "\n";
					json += "            },\n";
				}
				else if (sample.SizeValue)
				{
					json += "            \"kind\": \"size\",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": {\n";
					json += "              \"width\": "
						+ FormatDouble(sample.SizeValue->width) + ",\n";
					json += "              \"height\": "
						+ FormatDouble(sample.SizeValue->height) + "\n";
					json += "            },\n";
					json += "            \"matrixValue\": null,\n";
				}
				else if (sample.RectValue)
				{
					json += "            \"kind\": \"rect\",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": {\n";
					json += "              \"x\": "
						+ FormatDouble(sample.RectValue->x) + ",\n";
					json += "              \"y\": "
						+ FormatDouble(sample.RectValue->y) + ",\n";
					json += "              \"width\": "
						+ FormatDouble(sample.RectValue->width) + ",\n";
					json += "              \"height\": "
						+ FormatDouble(sample.RectValue->height) + "\n";
					json += "            },\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": null,\n";
				}
				else if (sample.VectorValue)
				{
					json += "            \"kind\": \"vector\",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": {\n";
					json += "              \"x\": "
						+ FormatDouble(sample.VectorValue->x) + ",\n";
					json += "              \"y\": "
						+ FormatDouble(sample.VectorValue->y) + "\n";
					json += "            },\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": null,\n";
				}
				else if (sample.ColorValue)
				{
					json += "            \"kind\": \"color\",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": {\n";
					json += "              \"scA\": "
						+ FormatDouble(sample.ColorValue->a) + ",\n";
					json += "              \"scR\": "
						+ FormatDouble(StoredSrgbToScRgb(sample.ColorValue->r)) + ",\n";
					json += "              \"scG\": "
						+ FormatDouble(StoredSrgbToScRgb(sample.ColorValue->g)) + ",\n";
					json += "              \"scB\": "
						+ FormatDouble(StoredSrgbToScRgb(sample.ColorValue->b)) + "\n";
					json += "            },\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": null,\n";
				}
				else if (sample.ThicknessValue)
				{
					json += "            \"kind\": \"thickness\",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": {\n";
					json += "              \"left\": "
						+ FormatDouble(sample.ThicknessValue->Left) + ",\n";
					json += "              \"top\": "
						+ FormatDouble(sample.ThicknessValue->Top) + ",\n";
					json += "              \"right\": "
						+ FormatDouble(sample.ThicknessValue->Right) + ",\n";
					json += "              \"bottom\": "
						+ FormatDouble(sample.ThicknessValue->Bottom) + "\n";
					json += "            },\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": null,\n";
				}
				else if (sample.Int32Value || sample.Int64Value || sample.SingleValue
					|| sample.BooleanValue || sample.StringValue)
				{
					const char* kind = sample.Int32Value ? "int32"
						: sample.Int64Value ? "int64"
						: sample.SingleValue ? "single"
						: sample.BooleanValue ? "boolean" : "string";
					json += "            \"kind\": " + JsonEscape(kind) + ",\n";
					json += "            \"doubleValue\": null,\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": null,\n";
				}
				else
				{
					json += "            \"kind\": \"double\",\n";
					json += "            \"doubleValue\": "
						+ FormatDouble(sample.Value) + ",\n";
					json += "            \"thicknessValue\": null,\n";
					json += "            \"colorValue\": null,\n";
					json += "            \"vectorValue\": null,\n";
					json += "            \"rectValue\": null,\n";
					json += "            \"sizeValue\": null,\n";
					json += "            \"matrixValue\": null,\n";
				}
				json += "            \"int32Value\": ";
				json += sample.Int32Value
					? std::to_string(*sample.Int32Value) : "null";
				json += ",\n            \"int64Value\": ";
				json += sample.Int64Value
					? std::to_string(*sample.Int64Value) : "null";
				json += ",\n            \"singleValue\": ";
				json += sample.SingleValue
					? FormatDouble(*sample.SingleValue) : "null";
				json += ",\n            \"booleanValue\": ";
				json += sample.BooleanValue
					? (*sample.BooleanValue ? "true" : "false") : "null";
				json += ",\n            \"stringValue\": ";
				json += sample.StringValue
					? JsonEscape(*sample.StringValue) : "null";
				json += "\n";
				json += "          },\n";
				json += "          \"rootStoryboardClock\": {\n";
				if (sample.Clock)
				{
					const char* state = sample.Clock->State
						== DeclarativeClockState::Active ? "Active"
						: sample.Clock->State == DeclarativeClockState::Filling
							? "Filling" : "Stopped";
					json += "            \"state\": "
						+ JsonEscape(state) + ",\n";
					json += "            \"currentTimeMilliseconds\": ";
					json += sample.Clock->CurrentTimeMilliseconds
						? std::to_string(*sample.Clock->CurrentTimeMilliseconds)
						: "null";
					json += ",\n            \"progress\": ";
					json += sample.Clock->Progress
						? FormatDouble(*sample.Clock->Progress) : "null";
					json += ",\n            \"globalSpeed\": ";
					json += sample.Clock->GlobalSpeed
						? FormatDouble(*sample.Clock->GlobalSpeed) : "null";
					json += "\n";
				}
				else
				{
					json += "            \"state\": null,\n";
					json += "            \"currentTimeMilliseconds\": null,\n";
					json += "            \"progress\": null,\n";
					json += "            \"globalSpeed\": null\n";
				}
				json += "          },\n";
				json += "          \"isAnimated\": ";
				json += sample.IsAnimated ? "true,\n" : "false,\n";
				json += "          \"events\": [";
				for (size_t eventIndex = 0;
					eventIndex < sample.Events.size(); ++eventIndex)
				{
					if (eventIndex != 0) json += ", ";
					json += JsonEscape(sample.Events[eventIndex]);
				}
				json += "]\n";
				json += "        }";
				if (sampleIndex + 1 != fixture.Samples.size()) json += ',';
				json += "\n";
			}
			if (!fixture.Samples.empty()) json += "      ";
			json += "]\n";
			json += "    }";
			if (fixtureIndex + 1 != document.Fixtures.size()) json += ',';
			json += "\n";
		}
		if (!document.Fixtures.empty()) json += "  ";
		json += "]\n";
		json += "}\n";
		return json;
	}

	CommandLineParseResult ParseCommandLineArguments(
		std::span<const std::wstring_view> arguments)
	{
		CommandLineParseResult result;
		result.Requested = std::any_of(arguments.begin(), arguments.end(),
			[](std::wstring_view argument)
			{ return argument == L"--animation-fixtures"; });
		if (!result.Requested) return result;

		CommandLineOptions options;
		bool fixturesSeen = false;
		bool outputSeen = false;
		bool caseSeen = false;
		for (std::size_t index = 0; index < arguments.size(); ++index)
		{
			const auto argument = arguments[index];
			if (argument != L"--animation-fixtures"
				&& argument != L"--animation-output"
				&& argument != L"--animation-case")
			{
				result.Error = "Unknown animation fixture option: "
					+ WideToUtf8(argument) + ".";
				return result;
			}
			if (index + 1 >= arguments.size() || arguments[index + 1].empty())
			{
				result.Error = "Missing value for animation fixture option: "
					+ WideToUtf8(argument) + ".";
				return result;
			}
			const auto value = arguments[++index];
			if (argument == L"--animation-fixtures")
			{
				if (fixturesSeen)
				{
					result.Error = "--animation-fixtures may be specified only once.";
					return result;
				}
				fixturesSeen = true;
				options.FixturesPath = std::filesystem::path(value);
			}
			else if (argument == L"--animation-output")
			{
				if (outputSeen)
				{
					result.Error = "--animation-output may be specified only once.";
					return result;
				}
				outputSeen = true;
				options.OutputPath = std::filesystem::path(value);
			}
			else
			{
				if (caseSeen)
				{
					result.Error = "--animation-case may be specified only once.";
					return result;
				}
				caseSeen = true;
				options.ExactFixtureId = WideToUtf8(value);
				if (!IsStableId(*options.ExactFixtureId))
				{
					result.Error = "--animation-case requires a stable fixture id.";
					return result;
				}
			}
		}
		if (!fixturesSeen)
		{
			result.Error = "--animation-fixtures is required for animation mode.";
			return result;
		}
		result.Options = std::move(options);
		return result;
	}

	CommandLineParseResult ParseLiveCommandLine()
	{
		int argumentCount = 0;
		wchar_t** arguments = ::CommandLineToArgvW(
			::GetCommandLineW(), &argumentCount);
		if (!arguments || argumentCount < 1)
		{
			if (arguments) ::LocalFree(arguments);
			return {};
		}
		struct ArgumentOwner final
		{
			wchar_t** Value = nullptr;
			~ArgumentOwner() { if (Value) ::LocalFree(Value); }
		} owner{ arguments };
		std::vector<std::wstring_view> views;
		views.reserve(static_cast<std::size_t>(argumentCount - 1));
		for (int index = 1; index < argumentCount; ++index)
			views.emplace_back(arguments[index]);
		return ParseCommandLineArguments(views);
	}

	std::wstring NormalizedPathKey(const std::filesystem::path& path)
	{
		std::error_code error;
		auto normalized = std::filesystem::absolute(path, error).lexically_normal().wstring();
		if (error) normalized = path.lexically_normal().wstring();
		std::transform(normalized.begin(), normalized.end(), normalized.begin(),
			[](wchar_t character)
			{ return static_cast<wchar_t>(std::towlower(character)); });
		return normalized;
	}

	bool IsUnderAnimationWorkplanRoot(const std::filesystem::path& output)
	{
		std::error_code error;
		const auto root = std::filesystem::absolute(
			std::filesystem::path("CUI-Workplans")
				/ "WPF-Animation-Alignment", error).lexically_normal();
		if (error) return false;
		const auto candidate = std::filesystem::absolute(
			output, error).lexically_normal();
		if (error || candidate == root) return false;
		const auto relative = candidate.lexically_relative(root);
		return !relative.empty() && *relative.begin() != ".."
			&& candidate.extension() == ".json";
	}

	void WriteStandardStream(FILE* stream, const std::string& value)
	{
		const int descriptor = ::_fileno(stream);
		if (descriptor >= 0) (void)::_setmode(descriptor, _O_BINARY);
		if (std::fwrite(value.data(), 1, value.size(), stream) != value.size()
			|| std::fflush(stream) != 0)
			throw std::runtime_error("Could not write animation runner output.");
	}

	std::string MinimalCorpus(
		std::string_view support = "supported",
		std::string_view timeline =
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100" FillBehavior="HoldEnd" />)XAML")
	{
		return std::string(R"XML(<?xml version="1.0" encoding="utf-8"?>
<animationFixtureCorpus schemaVersion="1">
  <fixture id="self-test" description="In-memory fixture" tolerance="0.0001" compare="value,isAnimated">
    <capabilities><capability id="ANIM-P0-01" /></capabilities>
    <support wpf="supported" cui=")XML") + std::string(support) + R"XML(" />
    <target kind="double" name="target" propertyPath="(Canvas.Left)" probe="canvas-left" baseValue="4" />
    <timeline><![CDATA[)XML" + std::string(timeline) + R"XML(]]></timeline>
    <samples>
      <sample atMilliseconds="0" label="begin" />
      <sample atMilliseconds="50" label="midpoint" />
      <sample atMilliseconds="100" label="active-end" />
    </samples>
  </fixture>
</animationFixtureCorpus>
)XML";
	}

	std::string MetadataDoubleCorpus()
	{
		return R"XML(<?xml version="1.0" encoding="utf-8"?>
<animationFixtureCorpus schemaVersion="1">
  <fixture id="metadata-double-self-test" description="Metadata-backed Double" tolerance="0.000001" compare="value,isAnimated">
    <capabilities><capability id="ANIM-P0-01" /></capabilities>
    <support wpf="supported" cui="supported" />
    <target kind="double" name="owner" propertyPath="Value" probe="metadata-double" baseValue="0.333333333333333" />
    <timeline><![CDATA[<DoubleAnimation Storyboard.TargetProperty="Value" From="0.123456789012345" To="0.987654321098765" Duration="0:0:0.100" FillBehavior="HoldEnd" />]]></timeline>
    <samples>
      <sample atMilliseconds="0" label="base-before-begin" phase="before-begin" />
      <sample atMilliseconds="0" label="begin" />
      <sample atMilliseconds="50" label="midpoint" />
      <sample atMilliseconds="100" label="active-end" />
    </samples>
  </fixture>
</animationFixtureCorpus>
)XML";
	}

	std::string SnapshotReplaceCorpus()
	{
		return R"XML(<?xml version="1.0" encoding="utf-8"?>
<animationFixtureCorpus schemaVersion="1">
  <fixture id="snapshot-replace-self-test" description="Snapshot replacement" tolerance="0.0001" compare="value,isAnimated" oracle="snapshot-replace">
    <capabilities><capability id="ANIM-P0-04" /></capabilities>
    <support wpf="supported" cui="supported" />
    <target kind="double" name="target" propertyPath="(Canvas.Left)" probe="canvas-left" baseValue="5" />
    <timeline><![CDATA[<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="5" To="105" Duration="0:0:1" FillBehavior="HoldEnd" />]]></timeline>
    <replacementTimeline><![CDATA[<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="200" To="300" BeginTime="0:0:0.200" Duration="0:0:1" FillBehavior="Stop" />]]></replacementTimeline>
    <operations><operation atMilliseconds="400" kind="begin-replacement" /></operations>
    <samples>
      <sample atMilliseconds="400" label="snapshot" />
      <sample atMilliseconds="599" label="before-begin" />
      <sample atMilliseconds="600" label="replacement-begin" />
      <sample atMilliseconds="1100" label="midpoint" />
      <sample atMilliseconds="1600" label="fill-stop-snapshot" />
    </samples>
  </fixture>
</animationFixtureCorpus>
)XML";
	}
}

std::optional<int> TryRunAnimationFixtureCommandLine()
{
	const auto parsed = ParseLiveCommandLine();
	if (!parsed.Requested) return std::nullopt;
	if (!parsed.Options)
	{
		try { WriteStandardStream(stderr, parsed.Error + "\n"); }
		catch (...) {}
		return 2;
	}

	try
	{
		const auto& options = *parsed.Options;
		if (options.OutputPath
			&& !IsUnderAnimationWorkplanRoot(*options.OutputPath))
			throw std::runtime_error(
				"--animation-output must be a JSON file under "
				"CUI-Workplans/WPF-Animation-Alignment.");
		if (options.OutputPath
			&& NormalizedPathKey(*options.OutputPath)
				== NormalizedPathKey(options.FixturesPath))
			throw std::runtime_error(
				"--animation-output must not overwrite the fixture corpus.");
		const auto corpusFile = ReadCorpusFile(options.FixturesPath);
		const auto corpus = ParseCorpus(
			corpusFile.Xml, Sha256(corpusFile.RawBytes));
		const auto result = ExecuteCorpus(corpus, options.ExactFixtureId);
		const auto json = SerializeResult(result);
		if (options.OutputPath)
		{
			std::wstring error;
			if (!DesignerModel::AtomicFile::Write(
				options.OutputPath->wstring(), json, &error))
				throw std::runtime_error("Could not write animation result: "
					+ Convert::UnicodeToUtf8(error));
		}
		else
		{
			WriteStandardStream(stdout, json);
		}
		return std::any_of(result.Fixtures.begin(), result.Fixtures.end(),
			[](const FixtureResult& fixture)
			{ return fixture.Status == "failed"; }) ? 1 : 0;
	}
	catch (const std::exception& error)
	{
		try { WriteStandardStream(stderr, std::string(error.what()) + "\n"); }
		catch (...) {}
		return 2;
	}
	catch (...)
	{
		try { WriteStandardStream(stderr, "Unknown animation runner error.\n"); }
		catch (...) {}
		return 2;
	}
}

void RegisterAnimationFixtureTests(cui::test::Runner& runner)
{
	runner.Add("Animation fixture schema is strict versioned and ordered", []
	{
		const auto corpus = ParseCorpus(MinimalCorpus());
		CUI_EXPECT_EQ(1, corpus.SchemaVersion);
		CUI_EXPECT_EQ(64ULL, corpus.CorpusSha256.size());
		CUI_EXPECT_EQ(1ULL, corpus.Fixtures.size());
		CUI_EXPECT_EQ(std::string("self-test"), corpus.Fixtures.front().Id);
		CUI_EXPECT_EQ(3ULL, corpus.Fixtures.front().Samples.size());
		auto timedXml = MinimalCorpus();
		const auto timelineOffset = timedXml.find("    <timeline>");
		CUI_EXPECT_TRUE(timelineOffset != std::string::npos);
		timedXml.insert(timelineOffset,
			"    <storyboardTiming beginTimeMilliseconds=\"25\" "
			"durationMilliseconds=\"50\" fillBehavior=\"Stop\" "
			"speedRatio=\"2\" accelerationRatio=\"0.2\" "
			"decelerationRatio=\"0.3\" />\n");
		const auto timed = ParseCorpus(timedXml);
		CUI_EXPECT_TRUE(timed.Fixtures.front().HasStoryboardTiming);
		CUI_EXPECT_EQ(25ULL, timed.Fixtures.front().StoryboardTiming
			.BeginTimeMilliseconds);
		CUI_EXPECT_FALSE(timed.Fixtures.front().StoryboardTiming
			.DurationAutomatic);
		CUI_EXPECT_EQ(50ULL, timed.Fixtures.front().StoryboardTiming
			.DurationMilliseconds);
		CUI_EXPECT_EQ(DeclarativeTimelineFillBehavior::Stop,
			timed.Fixtures.front().StoryboardTiming.FillBehavior);
		CUI_EXPECT_NEAR(2.0,
			timed.Fixtures.front().StoryboardTiming.SpeedRatio, 0.000001);

		auto rejects = [](std::string xml)
		{
			bool rejected = false;
			try { (void)ParseCorpus(xml); }
			catch (const std::exception&) { rejected = true; }
			CUI_EXPECT_TRUE(rejected);
		};
		auto unknown = MinimalCorpus();
		const auto rootEnd = unknown.find("schemaVersion=\"1\"");
		unknown.insert(rootEnd + std::string("schemaVersion=\"1\"").size(),
			" unknown=\"true\"");
		rejects(std::move(unknown));

		auto unordered = MinimalCorpus();
		const auto midpoint = unordered.find("atMilliseconds=\"50\"");
		unordered.replace(midpoint, std::string("atMilliseconds=\"50\"").size(),
			"atMilliseconds=\"101\"");
		rejects(std::move(unordered));

		auto unsafe = MinimalCorpus("supported",
			R"(<DoubleAnimation x:Class="Injected" />)");
		rejects(std::move(unsafe));
		auto crossEasingAttribute = MinimalCorpus("expected-gap",
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100"><DoubleAnimation.EasingFunction><CircleEase EasingMode="EaseIn" Amplitude="2" /></DoubleAnimation.EasingFunction></DoubleAnimation>)XAML");
		rejects(std::move(crossEasingAttribute));
		auto unknownEasing = MinimalCorpus("expected-gap",
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100"><DoubleAnimation.EasingFunction><InjectedEase EasingMode="EaseIn" /></DoubleAnimation.EasingFunction></DoubleAnimation>)XAML");
		rejects(std::move(unknownEasing));
		auto typedVector = MinimalCorpus("supported",
			R"XAML(<VectorAnimationUsingKeyFrames Storyboard.TargetProperty="Value" Duration="0:0:0.100"><LinearVectorKeyFrame KeyTime="Paced" Value="0,0" /><LinearVectorKeyFrame KeyTime="Paced" Value="3,4" /></VectorAnimationUsingKeyFrames>)XAML");
		const auto vectorTarget = typedVector.find(
			R"XML(<target kind="double" name="target" propertyPath="(Canvas.Left)" probe="canvas-left" baseValue="4" />)XML");
		CUI_EXPECT_TRUE(vectorTarget != std::string::npos);
		typedVector.replace(vectorTarget,
			std::string(R"XML(<target kind="double" name="target" propertyPath="(Canvas.Left)" probe="canvas-left" baseValue="4" />)XML").size(),
			R"XML(<target kind="vector" name="owner" propertyPath="Value" probe="metadata-vector" baseVector="0,0" />)XML");
		(void)ParseCorpus(typedVector);
		auto mixedTypedKeyFrame = typedVector;
		const auto vectorKeyFrame = mixedTypedKeyFrame.find("LinearVectorKeyFrame");
		CUI_EXPECT_TRUE(vectorKeyFrame != std::string::npos);
		mixedTypedKeyFrame.replace(vectorKeyFrame,
			std::string("LinearVectorKeyFrame").size(), "LinearColorKeyFrame");
		rejects(std::move(mixedTypedKeyFrame));
		auto linePath = MinimalCorpus("supported",
			R"XAML(<DoubleAnimationUsingPath Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" Source="X" Duration="0:0:0.100"><DoubleAnimationUsingPath.PathGeometry><PathGeometry><PathFigure StartPoint="10,20"><LineSegment Point="50,100" /></PathFigure></PathGeometry></DoubleAnimationUsingPath.PathGeometry></DoubleAnimationUsingPath>)XAML");
		(void)ParseCorpus(linePath);
		auto transformedPath = linePath;
		const auto geometryStart = transformedPath.find("<PathGeometry>");
		CUI_EXPECT_TRUE(geometryStart != std::string::npos);
		transformedPath.insert(geometryStart + std::string("<PathGeometry>").size(),
			R"XAML(<PathGeometry.Transform><TransformGroup><ScaleTransform ScaleX="1.25" ScaleY="-0.625" CenterX="17" CenterY="-9" /><TransformGroup><RotateTransform Angle="407.5" CenterX="-4" CenterY="12" /><TranslateTransform X="23.5" Y="-11.25" /></TransformGroup></TransformGroup></PathGeometry.Transform>)XAML");
		(void)ParseCorpus(transformedPath);
		auto closedPath = linePath;
		const auto figureStart = closedPath.find("<PathFigure ");
		CUI_EXPECT_TRUE(figureStart != std::string::npos);
		closedPath.insert(figureStart + std::string("<PathFigure ").size(),
			"IsClosed=\"true\" ");
		(void)ParseCorpus(closedPath);
		auto invalidClosedPath = closedPath;
		const auto closedValue = invalidClosedPath.find("IsClosed=\"true\"");
		CUI_EXPECT_TRUE(closedValue != std::string::npos);
		invalidClosedPath.replace(closedValue,
			std::string("IsClosed=\"true\"").size(), "IsClosed=\"sometimes\"");
		rejects(std::move(invalidClosedPath));
		auto unknownTransformAttribute = transformedPath;
		const auto scaleX = unknownTransformAttribute.find("ScaleX=\"1.25\"");
		CUI_EXPECT_TRUE(scaleX != std::string::npos);
		unknownTransformAttribute.insert(scaleX, "Injected=\"true\" ");
		rejects(std::move(unknownTransformAttribute));
		auto emptyTransformGroup = linePath;
		const auto emptyGeometryStart = emptyTransformGroup.find("<PathGeometry>");
		CUI_EXPECT_TRUE(emptyGeometryStart != std::string::npos);
		emptyTransformGroup.insert(
			emptyGeometryStart + std::string("<PathGeometry>").size(),
			"<PathGeometry.Transform><TransformGroup />"
			"</PathGeometry.Transform>");
		rejects(std::move(emptyTransformGroup));
		auto unsupportedSegmentPath = linePath;
		const auto pathSegment = unsupportedSegmentPath.find(
			R"XAML(<LineSegment Point="50,100" />)XAML");
		CUI_EXPECT_TRUE(pathSegment != std::string::npos);
		unsupportedSegmentPath.replace(pathSegment,
			std::string(R"XAML(<LineSegment Point="50,100" />)XAML").size(),
			R"XAML(<PathSegment />)XAML");
		rejects(std::move(unsupportedSegmentPath));
		auto duplicatePrimary = MinimalCorpus("supported",
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100" /><DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="20" To="30" Duration="0:0:0.200" />)XAML");
		rejects(std::move(duplicatePrimary));
		auto unsafeAuxiliary = MinimalCorpus("supported",
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100" /><DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Top)" To="30" Duration="0:0:0.200" />)XAML");
		rejects(std::move(unsafeAuxiliary));
		auto invalidRootTiming = timedXml;
		const auto rootSpeed = invalidRootTiming.find("speedRatio=\"2\"");
		CUI_EXPECT_TRUE(rootSpeed != std::string::npos);
		invalidRootTiming.replace(rootSpeed,
			std::string("speedRatio=\"2\"").size(), "speedRatio=\"0\"");
		rejects(std::move(invalidRootTiming));
		rejects("<!DOCTYPE animationFixtureCorpus [<!ELEMENT animationFixtureCorpus ANY>]>"
			+ MinimalCorpus());
	});

	runner.Add("Animation fixture runner samples deterministic CUI values", []
	{
		const auto corpus = ParseCorpus(MinimalCorpus());
		const auto result = ExecuteCorpus(corpus, std::nullopt);
		CUI_EXPECT_EQ(1ULL, result.Fixtures.size());
		const auto& fixture = result.Fixtures.front();
		if (fixture.Error)
			throw std::runtime_error("animation fixture self-test failed: "
				+ *fixture.Error);
		CUI_EXPECT_EQ(std::string("passed"), fixture.Status);
		CUI_EXPECT_FALSE(fixture.Error.has_value());
		CUI_EXPECT_EQ(3ULL, fixture.Samples.size());
		CUI_EXPECT_NEAR(0.0, fixture.Samples[0].Value, 0.0001);
		CUI_EXPECT_NEAR(5.0, fixture.Samples[1].Value, 0.0001);
		CUI_EXPECT_NEAR(10.0, fixture.Samples[2].Value, 0.0001);
		CUI_EXPECT_TRUE(fixture.Samples[0].IsAnimated);
		CUI_EXPECT_TRUE(fixture.Samples[2].IsAnimated);

		auto requireNaturalDuration = [](std::string_view timeline)
		{
			const auto naturalResult = ExecuteCorpus(
				ParseCorpus(MinimalCorpus("supported", timeline)), std::nullopt);
			const auto& naturalFixture = naturalResult.Fixtures.front();
			if (naturalFixture.Error)
				throw std::runtime_error(
					"natural-duration fixture self-test failed: "
					+ *naturalFixture.Error);
			CUI_EXPECT_EQ(std::string("passed"), naturalFixture.Status);
			CUI_EXPECT_EQ(3ULL, naturalFixture.Samples.size());
			CUI_EXPECT_NEAR(0.0, naturalFixture.Samples[0].Value, 0.0001);
			CUI_EXPECT_NEAR(0.5, naturalFixture.Samples[1].Value, 0.0001);
			CUI_EXPECT_NEAR(1.0, naturalFixture.Samples[2].Value, 0.0001);
		};
		requireNaturalDuration(
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="Automatic" FillBehavior="HoldEnd" />)XAML");
		requireNaturalDuration(
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" FillBehavior="HoldEnd" />)XAML");

		const auto pathCorpus = ParseCorpus(MinimalCorpus(
			"supported",
			R"XAML(<DoubleAnimationUsingPath Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" Source="X" Duration="0:0:0.100" FillBehavior="HoldEnd"><DoubleAnimationUsingPath.PathGeometry><PathGeometry><PathFigure StartPoint="10,20"><LineSegment Point="50,100" /></PathFigure></PathGeometry></DoubleAnimationUsingPath.PathGeometry></DoubleAnimationUsingPath>)XAML"));
		const auto pathResult = ExecuteCorpus(pathCorpus, std::nullopt);
		if (pathResult.Fixtures.front().Error)
			throw std::runtime_error("line-path fixture self-test failed: "
				+ *pathResult.Fixtures.front().Error);
		CUI_EXPECT_EQ(std::string("passed"),
			pathResult.Fixtures.front().Status);
		CUI_EXPECT_NEAR(10.0,
			pathResult.Fixtures.front().Samples[0].Value, 0.000001);
		CUI_EXPECT_NEAR(30.000001907348633,
			pathResult.Fixtures.front().Samples[1].Value, 0.000001);
		CUI_EXPECT_NEAR(50.000003814697266,
			pathResult.Fixtures.front().Samples[2].Value, 0.000001);
		const auto closedPathCorpus = ParseCorpus(MinimalCorpus(
			"supported",
			R"XAML(<DoubleAnimationUsingPath Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" Source="X" Duration="0:0:0.100" FillBehavior="HoldEnd"><DoubleAnimationUsingPath.PathGeometry><PathGeometry><PathFigure StartPoint="10,20" IsClosed="true"><LineSegment Point="50,100" /></PathFigure></PathGeometry></DoubleAnimationUsingPath.PathGeometry></DoubleAnimationUsingPath>)XAML"));
		const auto closedPathResult = ExecuteCorpus(closedPathCorpus, std::nullopt);
		if (closedPathResult.Fixtures.front().Error)
			throw std::runtime_error("closed-path fixture self-test failed: "
				+ *closedPathResult.Fixtures.front().Error);
		CUI_EXPECT_EQ(std::string("passed"),
			closedPathResult.Fixtures.front().Status);
		CUI_EXPECT_NEAR(10.0,
			closedPathResult.Fixtures.front().Samples[0].Value, 0.000001);
		CUI_EXPECT_NEAR(50.000003814697266,
			closedPathResult.Fixtures.front().Samples[1].Value, 0.000001);
		CUI_EXPECT_NEAR(9.999996185302734,
			closedPathResult.Fixtures.front().Samples[2].Value, 0.000001);
		const auto multipleFigureCorpus = ParseCorpus(MinimalCorpus(
			"supported",
			R"XAML(<DoubleAnimationUsingPath Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" Source="X" Duration="0:0:0.100" FillBehavior="HoldEnd"><DoubleAnimationUsingPath.PathGeometry><PathGeometry><PathFigure StartPoint="0,0"><LineSegment Point="100,0" /></PathFigure><PathFigure StartPoint="1000,500"><LineSegment Point="1100,500" /></PathFigure></PathGeometry></DoubleAnimationUsingPath.PathGeometry></DoubleAnimationUsingPath>)XAML"));
		const auto multipleFigureResult = ExecuteCorpus(
			multipleFigureCorpus, std::nullopt);
		if (multipleFigureResult.Fixtures.front().Error)
			throw std::runtime_error("multiple-figure fixture self-test failed: "
				+ *multipleFigureResult.Fixtures.front().Error);
		CUI_EXPECT_EQ(std::string("passed"),
			multipleFigureResult.Fixtures.front().Status);
		CUI_EXPECT_NEAR(0.0,
			multipleFigureResult.Fixtures.front().Samples[0].Value, 0.000001);
		CUI_EXPECT_NEAR(100.0,
			multipleFigureResult.Fixtures.front().Samples[1].Value, 0.000001);
		CUI_EXPECT_NEAR(1100.0,
			multipleFigureResult.Fixtures.front().Samples[2].Value, 0.000001);
		DesignerModel::DesignDocument multipleFigureDocument;
		std::wstring multipleFigureError;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			BuildRuntimeXaml(multipleFigureCorpus.Fixtures.front()),
			multipleFigureDocument, &multipleFigureError));
		const auto multipleFigureCanonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(multipleFigureDocument);
		CUI_EXPECT_TRUE(multipleFigureCanonical.find(
			"<PathFigure StartPoint=\"1000,500\"") != std::string::npos);
		const auto multipleFigureSnapshot =
			DesignerModel::DesignDocumentSerializer::ToXml(multipleFigureDocument);
		CUI_EXPECT_TRUE(multipleFigureSnapshot.find(
			"<pathSegment type=\"Move\"") != std::string::npos);
		DesignerModel::DesignDocument multipleFigureSnapshotRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			multipleFigureSnapshot, multipleFigureSnapshotRoundTrip,
			&multipleFigureError));
		CUI_EXPECT_TRUE(multipleFigureSnapshotRoundTrip
			== multipleFigureDocument);
		auto orphanMoveSnapshot = multipleFigureSnapshot;
		const auto firstLineSegment = orphanMoveSnapshot.find(
			"<pathSegment type=\"Line\"");
		CUI_EXPECT_TRUE(firstLineSegment != std::string::npos);
		orphanMoveSnapshot.replace(firstLineSegment,
			std::string("<pathSegment type=\"Line\"").size(),
			"<pathSegment type=\"Move\"");
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			orphanMoveSnapshot, multipleFigureSnapshotRoundTrip,
			&multipleFigureError));
		DesignerModel::DesignDocument pathDocument;
		std::wstring pathError;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			BuildRuntimeXaml(pathCorpus.Fixtures.front()), pathDocument, &pathError));
		const auto pathCanonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(pathDocument);
		CUI_EXPECT_TRUE(pathCanonical.find("DoubleAnimationUsingPath")
			!= std::string::npos);
		CUI_EXPECT_TRUE(pathCanonical.find("<LineSegment Point=\"50,100\"")
			!= std::string::npos);
		DesignerModel::DesignDocument pathCanonicalRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			pathCanonical, pathCanonicalRoundTrip, &pathError));
		CUI_EXPECT_TRUE(pathCanonicalRoundTrip == pathDocument);
		const auto pathSnapshot =
			DesignerModel::DesignDocumentSerializer::ToXml(pathDocument);
		CUI_EXPECT_TRUE(pathSnapshot.find("linePath=\"true\"")
			!= std::string::npos);
		CUI_EXPECT_TRUE(pathSnapshot.find("pathSource=\"X\"")
			!= std::string::npos);
		DesignerModel::DesignDocument pathSnapshotRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			pathSnapshot, pathSnapshotRoundTrip, &pathError));
		CUI_EXPECT_TRUE(pathSnapshotRoundTrip == pathDocument);
		auto invalidPathSnapshot = pathSnapshot;
		const auto pathSource = invalidPathSnapshot.find("pathSource=\"X\"");
		CUI_EXPECT_TRUE(pathSource != std::string::npos);
		invalidPathSnapshot.replace(pathSource,
			std::string("pathSource=\"X\"").size(), "pathSource=\"Z\"");
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			invalidPathSnapshot, pathSnapshotRoundTrip, &pathError));
		DesignerModel::RuntimeDocument pathReloadRuntime;
		CUI_EXPECT_TRUE(DesignerModel::RuntimeDocumentLoader::Load(
			pathDocument, pathReloadRuntime, {}, &pathError));
		auto pathReloadXaml = BuildRuntimeXaml(pathCorpus.Fixtures.front());
		const std::string fixtureHostMarkup =
			"<local:AnimationFixtureHost x:Name=\"fixtureHost\" />";
		const auto fixtureHostAt = pathReloadXaml.find(fixtureHostMarkup);
		CUI_EXPECT_TRUE(fixtureHostAt != std::string::npos);
		if (fixtureHostAt != std::string::npos)
			pathReloadXaml.replace(fixtureHostAt, fixtureHostMarkup.size(),
				"<StackPanel x:Name=\"reloadRoot\">"
				+ fixtureHostMarkup
				+ "<Canvas x:Name=\"reloadMarker\" />"
					"</StackPanel>");
		DesignerModel::DesignDocument pathReloadDocument;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			pathReloadXaml, pathReloadDocument, &pathError));
		DesignerModel::RuntimeDocumentReloadMode pathReloadMode{};
		CUI_EXPECT_TRUE(DesignerModel::RuntimeDocumentLoader::Reload(
			pathReloadDocument, pathReloadRuntime, {},
			&pathReloadMode, &pathError));
		CUI_EXPECT_TRUE(pathReloadMode
			!= DesignerModel::RuntimeDocumentReloadMode::Unchanged);
		CUI_EXPECT_TRUE(pathReloadRuntime.FindControlByName(L"reloadMarker")
			!= nullptr);
		auto* pathReloadHost = pathReloadRuntime.FindControlByName(L"fixtureHost");
		auto* pathReloadTarget = pathReloadHost
			? pathReloadHost->FindDeclarativeTemplatePart(L"target") : nullptr;
		CUI_EXPECT_TRUE(pathReloadHost != nullptr && pathReloadTarget != nullptr);
		if (pathReloadHost && pathReloadTarget)
		{
			ClockOverrideScope pathReloadClock(*pathReloadHost, FixtureClockOrigin);
			CUI_EXPECT_TRUE(pathReloadHost->GoToVisualState(
				L"FixtureStates", L"Running", false, &pathError));
			const auto& midpoint = pathCorpus.Fixtures.front().Samples[1];
			const auto midpointTick = FixtureClockOrigin
				+ midpoint.AtMilliseconds;
			pathReloadClock.Set(midpointTick);
			CUI_EXPECT_TRUE(cui::framework::PresentationAccess::
				AdvanceVisualStateAnimations(*pathReloadHost, midpointTick));
			const auto observed = ObserveDesignSample(
				pathCorpus.Fixtures.front(), midpoint,
				*pathReloadHost, *pathReloadTarget);
			CUI_EXPECT_NEAR(30.000001907348633,
				observed.Value, 0.000001);
		}

		const auto json = SerializeResult(result);
		CUI_EXPECT_FALSE(json.empty());
		CUI_EXPECT_EQ('\n', json.back());
		CUI_EXPECT_TRUE(json.find('\r') == std::string::npos);
		CUI_EXPECT_TRUE(json.find("\"corpusSha256\": \"")
			!= std::string::npos);
		CUI_EXPECT_TRUE(json.find("\"rootStoryboardClock\": {")
			!= std::string::npos);
		CUI_EXPECT_TRUE(json.find("\"currentTimeMilliseconds\": null")
			!= std::string::npos);
		CUI_EXPECT_TRUE(json.find("\"isAnimated\": true")
			!= std::string::npos);
		CUI_EXPECT_TRUE(json.find("\"events\": []")
			!= std::string::npos);

		const auto splineCorpus = ParseCorpus(MinimalCorpus(
			"supported",
			R"XAML(<DoubleAnimationUsingKeyFrames Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" Duration="0:0:0.100"><SplineDoubleKeyFrame KeyTime="0:0:0.100" Value="14" KeySpline="0.25,0.1 0.25,1" /></DoubleAnimationUsingKeyFrames>)XAML"));
		const auto splineResult = ExecuteCorpus(splineCorpus, std::nullopt);
		CUI_EXPECT_EQ(std::string("passed"),
			splineResult.Fixtures.front().Status);
		CUI_EXPECT_NEAR(12.024055898129339,
			splineResult.Fixtures.front().Samples[1].Value, 0.00001);

		auto sequentialXml = MinimalCorpus(
			"supported",
			R"XAML(<DoubleAnimationUsingKeyFrames Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" Duration="0:0:0.100"><SplineDoubleKeyFrame KeyTime="0:0:0.100" Value="14" KeySpline="0.25,0.1 0.25,1" /></DoubleAnimationUsingKeyFrames>)XAML");
		const auto midpointSample = sequentialXml.find(
			R"XML(      <sample atMilliseconds="50" label="midpoint" />)XML");
		CUI_EXPECT_TRUE(midpointSample != std::string::npos);
		sequentialXml.insert(midpointSample,
			R"XML(      <sample atMilliseconds="25" label="quarter" />
)XML");
		const auto endSample = sequentialXml.find(
			R"XML(      <sample atMilliseconds="100" label="active-end" />)XML");
		CUI_EXPECT_TRUE(endSample != std::string::npos);
		sequentialXml.insert(endSample,
			R"XML(      <sample atMilliseconds="75" label="three-quarters" />
)XML");
		const auto sequentialCorpus = ParseCorpus(sequentialXml);
		const auto sequential = ExecuteSequentialSamples(
			sequentialCorpus.Fixtures.front());
		CUI_EXPECT_EQ(5ULL, sequential.size());
		CUI_EXPECT_NEAR(8.085217732412737, sequential[1].Value, 0.00001);
		CUI_EXPECT_NEAR(12.025583089399452, sequential[2].Value, 0.00001);
		CUI_EXPECT_NEAR(13.604606747495692, sequential[3].Value, 0.00001);

		const auto nestedCorpus = ParseCorpus(MinimalCorpus("supported",
			R"XAML(<ParallelTimeline BeginTime="0:0:0.025" SpeedRatio="2"><DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100" FillBehavior="HoldEnd" /></ParallelTimeline>)XAML"));
		const auto nestedResult = ExecuteCorpus(nestedCorpus, std::nullopt);
		if (nestedResult.Fixtures.front().Error)
			throw std::runtime_error("nested timeline fixture self-test failed: "
				+ *nestedResult.Fixtures.front().Error);
		CUI_EXPECT_EQ(std::string("passed"),
			nestedResult.Fixtures.front().Status);
		CUI_EXPECT_NEAR(4.0,
			nestedResult.Fixtures.front().Samples[0].Value, 0.0001);
		CUI_EXPECT_NEAR(5.0,
			nestedResult.Fixtures.front().Samples[1].Value, 0.0001);
		CUI_EXPECT_NEAR(10.0,
			nestedResult.Fixtures.front().Samples[2].Value, 0.0001);

		const std::string resourceXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  xmlns:local="urn:cui:animation-resource-self-test" x:Name="ResourceWindow">
  <Window.Resources>
    <ComponentDefinition x:Key="local:ResourceHost" BaseType="Canvas">
      <ComponentDefinition.Events>
        <ComponentEvent Name="BeginOne" RoutingStrategy="Direct" />
        <ComponentEvent Name="BeginTwo" RoutingStrategy="Direct" />
      </ComponentDefinition.Events>
      <ComponentDefinition.Template>
        <Canvas x:Name="target" Canvas.Left="0">
          <Canvas.Resources>
            <Storyboard x:Key="SharedPulse" Duration="0:0:0.100">
              <DoubleAnimation Storyboard.TargetName="target"
                Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10"
                Duration="0:0:0.100" FillBehavior="HoldEnd" />
            </Storyboard>
          </Canvas.Resources>
          <Canvas.Triggers>
            <EventTrigger RoutedEvent="BeginOne">
              <BeginStoryboard x:Name="FirstClock"
                Storyboard="{StaticResource SharedPulse}" />
            </EventTrigger>
            <EventTrigger RoutedEvent="BeginTwo">
              <BeginStoryboard x:Name="SecondClock"
                Storyboard="{StaticResource SharedPulse}" />
            </EventTrigger>
          </Canvas.Triggers>
        </Canvas>
      </ComponentDefinition.Template>
    </ComponentDefinition>
  </Window.Resources>
  <local:ResourceHost x:Name="resourceHost" />
</Window>)XAML";
		DesignerModel::DesignDocument resourceDocument;
		std::wstring resourceError;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			resourceXaml, resourceDocument, &resourceError));
		CUI_EXPECT_EQ(1ULL, resourceDocument.Components.size());
		const auto& resourceComponent = resourceDocument.Components.front();
		CUI_EXPECT_EQ(2ULL, resourceComponent.EventTriggers.size());
		CUI_EXPECT_EQ(std::wstring(L"SharedPulse"),
			resourceComponent.EventTriggers.front().Actions.front()
				.StoryboardResourceKey);
		CUI_EXPECT_EQ(1ULL,
			resourceComponent.Template.front().LocalObjectResources
				.Storyboards.size());
		const auto resourceCanonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(resourceDocument);
		CUI_EXPECT_TRUE(resourceCanonical.find(
			"Storyboard=\"{StaticResource SharedPulse}\"")
			!= std::string::npos);
		DesignerModel::DesignDocument resourceCanonicalRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			resourceCanonical, resourceCanonicalRoundTrip, &resourceError));
		CUI_EXPECT_TRUE(resourceCanonicalRoundTrip == resourceDocument);
		const auto resourceSnapshot =
			DesignerModel::DesignDocumentSerializer::ToXml(resourceDocument);
		CUI_EXPECT_TRUE(resourceSnapshot.find("version=\"48\"")
			!= std::string::npos);
		DesignerModel::DesignDocument resourceSnapshotRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			resourceSnapshot, resourceSnapshotRoundTrip, &resourceError));
		CUI_EXPECT_TRUE(resourceSnapshotRoundTrip == resourceDocument);
		auto mismatchedResourceSnapshot = resourceSnapshot;
		const auto snapshotReference = mismatchedResourceSnapshot.find(
			"storyboardResource=\"SharedPulse\"");
		CUI_EXPECT_TRUE(snapshotReference != std::string::npos);
		mismatchedResourceSnapshot.replace(snapshotReference,
			std::string("storyboardResource=\"SharedPulse\"").size(),
			"storyboardResource=\"MissingPulse\"");
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			mismatchedResourceSnapshot, resourceSnapshotRoundTrip, &resourceError));
		auto missingResourceXaml = resourceXaml;
		const auto resourceReference = missingResourceXaml.find(
			"{StaticResource SharedPulse}");
		CUI_EXPECT_TRUE(resourceReference != std::string::npos);
		missingResourceXaml.replace(resourceReference,
			std::string("{StaticResource SharedPulse}").size(),
			"{StaticResource MissingPulse}");
		DesignerModel::DesignDocument rejectedResourceDocument;
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			missingResourceXaml, rejectedResourceDocument, &resourceError));
		auto literalResourceXaml = resourceXaml;
		const auto literalReference = literalResourceXaml.find(
			"{StaticResource SharedPulse}");
		CUI_EXPECT_TRUE(literalReference != std::string::npos);
		literalResourceXaml.replace(literalReference,
			std::string("{StaticResource SharedPulse}").size(), "SharedPulse");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			literalResourceXaml, rejectedResourceDocument, &resourceError));
		auto wrongTypeResourceXaml = resourceXaml;
		const auto resourcesStart = wrongTypeResourceXaml.find(
			"<Canvas.Resources>");
		CUI_EXPECT_TRUE(resourcesStart != std::string::npos);
		wrongTypeResourceXaml.insert(resourcesStart
			+ std::string("<Canvas.Resources>").size(),
			"<Double x:Key=\"ScalarPulse\">1</Double>");
		const auto wrongTypeReference = wrongTypeResourceXaml.find(
			"{StaticResource SharedPulse}");
		CUI_EXPECT_TRUE(wrongTypeReference != std::string::npos);
		wrongTypeResourceXaml.replace(wrongTypeReference,
			std::string("{StaticResource SharedPulse}").size(),
			"{StaticResource ScalarPulse}");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			wrongTypeResourceXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"不是 Storyboard")
			!= std::wstring::npos);
		const std::string aliasCycleXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="AliasWindow">
  <Window.Resources>
    <StaticResource x:Key="Loop" ResourceKey="Loop" />
  </Window.Resources>
  <Canvas x:Name="content" />
</Window>)XAML";
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			aliasCycleXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"资源环") != std::wstring::npos);

		const std::string deferredResourceXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  xmlns:local="urn:cui:animation-resource-deferred" x:Name="DeferredWindow">
  <Window.Resources>
    <Storyboard x:Key="SharedPulse">
      <DoubleAnimation Storyboard.TargetName="lateTarget"
        Storyboard.TargetProperty="(Canvas.Left)" To="10"
        Duration="0:0:0.100" FillBehavior="HoldEnd" />
    </Storyboard>
    <ComponentDefinition x:Key="local:DocumentHost" BaseType="Canvas">
      <ComponentDefinition.Events>
        <ComponentEvent Name="BeginPulse" RoutingStrategy="Direct" />
      </ComponentDefinition.Events>
      <ComponentDefinition.Template>
        <Canvas x:Name="documentRoot">
          <Canvas.Triggers>
            <EventTrigger RoutedEvent="BeginPulse">
              <BeginStoryboard x:Name="DocumentClock"
                Storyboard="{StaticResource SharedPulse}" />
            </EventTrigger>
          </Canvas.Triggers>
          <Canvas x:Name="lateTarget" Canvas.Left="0" />
        </Canvas>
      </ComponentDefinition.Template>
    </ComponentDefinition>
    <ComponentDefinition x:Key="local:ShadowHost" BaseType="Canvas">
      <ComponentDefinition.Events>
        <ComponentEvent Name="BeginPulse" RoutingStrategy="Direct" />
      </ComponentDefinition.Events>
      <ComponentDefinition.Template>
        <Canvas x:Name="shadowRoot">
          <Canvas.Resources>
            <Storyboard x:Key="SharedPulse">
              <DoubleAnimation Storyboard.TargetName="lateTarget"
                Storyboard.TargetProperty="(Canvas.Left)" To="100"
                Duration="0:0:0.100" FillBehavior="HoldEnd" />
            </Storyboard>
          </Canvas.Resources>
          <Canvas.Triggers>
            <EventTrigger RoutedEvent="BeginPulse">
              <BeginStoryboard x:Name="ShadowClock"
                Storyboard="{StaticResource SharedPulse}" />
            </EventTrigger>
          </Canvas.Triggers>
          <Canvas x:Name="lateTarget" Canvas.Left="0" />
        </Canvas>
      </ComponentDefinition.Template>
    </ComponentDefinition>
  </Window.Resources>
  <StackPanel x:Name="hosts">
    <local:DocumentHost x:Name="documentHost" />
    <local:ShadowHost x:Name="shadowHost" />
  </StackPanel>
</Window>)XAML";
		DesignerModel::DesignDocument deferredDocument;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			deferredResourceXaml, deferredDocument, &resourceError));
		CUI_EXPECT_EQ(1ULL, deferredDocument.Storyboards.size());
		CUI_EXPECT_EQ(2ULL, deferredDocument.Components.size());
		const auto documentHost = std::find_if(deferredDocument.Components.begin(),
			deferredDocument.Components.end(), [](const auto& component)
			{ return component.Type.XamlName == L"DocumentHost"; });
		const auto shadowHost = std::find_if(deferredDocument.Components.begin(),
			deferredDocument.Components.end(), [](const auto& component)
			{ return component.Type.XamlName == L"ShadowHost"; });
		CUI_EXPECT_TRUE(documentHost != deferredDocument.Components.end());
		CUI_EXPECT_TRUE(shadowHost != deferredDocument.Components.end());
		CUI_EXPECT_EQ(std::wstring(L"10"), documentHost->EventTriggers.front()
			.Actions.front().Animations.front().To.Text);
		CUI_EXPECT_EQ(std::wstring(L"100"), shadowHost->EventTriggers.front()
			.Actions.front().Animations.front().To.Text);
		CUI_EXPECT_EQ(1ULL, shadowHost->Template.front().LocalObjectResources
			.Storyboards.size());
		const auto deferredCanonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(deferredDocument);
		DesignerModel::DesignDocument deferredCanonicalRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			deferredCanonical, deferredCanonicalRoundTrip, &resourceError));
		CUI_EXPECT_TRUE(deferredCanonicalRoundTrip == deferredDocument);
		const auto deferredSnapshot =
			DesignerModel::DesignDocumentSerializer::ToXml(deferredDocument);
		DesignerModel::DesignDocument deferredSnapshotRoundTrip;
		if (!DesignerModel::DesignDocumentSerializer::FromXml(
			deferredSnapshot, deferredSnapshotRoundTrip, &resourceError))
			throw std::runtime_error(
				"deferred resource snapshot round-trip failed: "
				+ Convert::UnicodeToUtf8(resourceError));
		CUI_EXPECT_TRUE(deferredSnapshotRoundTrip == deferredDocument);
		auto missingLateTargetXaml = deferredResourceXaml;
		const auto lateTargetReference = missingLateTargetXaml.find(
			"Storyboard.TargetName=\"lateTarget\"");
		CUI_EXPECT_TRUE(lateTargetReference != std::string::npos);
		missingLateTargetXaml.replace(lateTargetReference,
			std::string("Storyboard.TargetName=\"lateTarget\"").size(),
			"Storyboard.TargetName=\"missingTarget\"");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			missingLateTargetXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"找不到模板部件")
			!= std::wstring::npos);

		const std::string styleResourceXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="StyleWindow">
  <Window.Resources>
    <Storyboard x:Key="StylePulse">
      <DoubleAnimation Storyboard.TargetProperty="FontSize" From="12" To="18"
        Duration="0:0:0.100" />
    </Storyboard>
    <Style x:Key="PulseButton" TargetType="Button">
      <Style.Triggers>
        <Trigger Property="IsDefault" Value="true">
          <Trigger.EnterActions>
            <BeginStoryboard x:Name="StyleClock"
              Storyboard="{StaticResource StylePulse}" />
          </Trigger.EnterActions>
          <Trigger.ExitActions>
            <StopStoryboard BeginStoryboardName="StyleClock" />
          </Trigger.ExitActions>
        </Trigger>
      </Style.Triggers>
    </Style>
  </Window.Resources>
  <Button x:Name="styleButton" Style="{StaticResource PulseButton}" />
</Window>)XAML";
		DesignerModel::DesignDocument styleResourceDocument;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			styleResourceXaml, styleResourceDocument, &resourceError));
		CUI_EXPECT_EQ(1ULL, styleResourceDocument.Storyboards.size());
		CUI_EXPECT_EQ(1ULL, styleResourceDocument.StyleSheet.Rules.size());
		const auto& styleAction = styleResourceDocument.StyleSheet.Rules.front()
			.Triggers.front().EnterActions.front();
		CUI_EXPECT_EQ(std::wstring(L"StylePulse"),
			styleAction.StoryboardResourceKey);
		CUI_EXPECT_EQ(1ULL, styleAction.Animations.size());
		const auto styleCanonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(styleResourceDocument);
		DesignerModel::DesignDocument styleCanonicalRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			styleCanonical, styleCanonicalRoundTrip, &resourceError));
		CUI_EXPECT_TRUE(styleCanonicalRoundTrip == styleResourceDocument);
		const auto styleSnapshot =
			DesignerModel::DesignDocumentSerializer::ToXml(styleResourceDocument);
		DesignerModel::DesignDocument styleSnapshotRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::DesignDocumentSerializer::FromXml(
			styleSnapshot, styleSnapshotRoundTrip, &resourceError));
		CUI_EXPECT_TRUE(styleSnapshotRoundTrip == styleResourceDocument);
		auto namedStyleResourceXaml = styleResourceXaml;
		const auto styleTargetProperty = namedStyleResourceXaml.find(
			"Storyboard.TargetProperty=\"FontSize\"");
		CUI_EXPECT_TRUE(styleTargetProperty != std::string::npos);
		namedStyleResourceXaml.insert(styleTargetProperty,
			"Storyboard.TargetName=\"styleButton\" ");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			namedStyleResourceXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"Style Storyboard 不支持")
			!= std::wstring::npos);

		const std::string stateResourceXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  xmlns:local="urn:cui:animation-state-resource" x:Name="StateWindow">
  <Window.Resources>
    <Storyboard x:Key="StatePulse">
      <DoubleAnimation Storyboard.TargetName="lateTarget"
        Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10"
        Duration="0:0:0.100" />
    </Storyboard>
    <Storyboard x:Key="TransitionPulse">
      <DoubleAnimation Storyboard.TargetName="lateTarget"
        Storyboard.TargetProperty="(Canvas.Left)" From="10" To="20"
        Duration="0:0:0.050" />
    </Storyboard>
    <ComponentDefinition x:Key="local:StateHost" BaseType="Canvas">
      <ComponentDefinition.Properties>
        <ComponentProperty Name="Phase" Type="Int" Default="0" />
      </ComponentDefinition.Properties>
      <ComponentDefinition.Template>
        <Canvas x:Name="stateRoot">
          <VisualStateManager.VisualStateGroups>
            <VisualStateGroup x:Name="CommonStates">
              <VisualStateGroup.Transitions>
                <VisualTransition From="Idle" To="Running"
                  Storyboard="{StaticResource TransitionPulse}" />
              </VisualStateGroup.Transitions>
              <VisualState x:Name="Idle"
                Storyboard="{StaticResource StatePulse}" />
              <VisualState x:Name="Running">
                <VisualState.StateTriggers>
                  <StateTrigger Property="Phase" Value="1" />
                </VisualState.StateTriggers>
              </VisualState>
            </VisualStateGroup>
          </VisualStateManager.VisualStateGroups>
          <Canvas x:Name="lateTarget" Canvas.Left="0" />
        </Canvas>
      </ComponentDefinition.Template>
    </ComponentDefinition>
  </Window.Resources>
  <local:StateHost x:Name="stateHost" />
</Window>)XAML";
		DesignerModel::DesignDocument stateResourceDocument;
		if (!DesignerModel::XamlDocumentParser::FromXaml(
			stateResourceXaml, stateResourceDocument, &resourceError))
			throw std::runtime_error("state resource parse failed: "
				+ Convert::UnicodeToUtf8(resourceError));
		CUI_EXPECT_EQ(2ULL, stateResourceDocument.Storyboards.size());
		CUI_EXPECT_EQ(1ULL, stateResourceDocument.Components.size());
		const auto& resourceGroup = stateResourceDocument.Components.front()
			.VisualStateGroups.front();
		CUI_EXPECT_EQ(std::wstring(L"StatePulse"),
			resourceGroup.States.front().StoryboardResourceKey);
		CUI_EXPECT_EQ(std::wstring(L"TransitionPulse"),
			resourceGroup.Transitions.front().StoryboardResourceKey);
		CUI_EXPECT_EQ(1ULL, resourceGroup.States.front().Animations.size());
		CUI_EXPECT_EQ(1ULL, resourceGroup.Transitions.front().Animations.size());
		const auto stateCanonical =
			DesignerModel::XamlDocumentSerializer::ToXaml(stateResourceDocument);
		CUI_EXPECT_TRUE(stateCanonical.find(
			"Storyboard=\"{StaticResource StatePulse}\"") != std::string::npos);
		CUI_EXPECT_TRUE(stateCanonical.find(
			"Storyboard=\"{StaticResource TransitionPulse}\"")
			!= std::string::npos);
		DesignerModel::DesignDocument stateCanonicalRoundTrip;
		CUI_EXPECT_TRUE(DesignerModel::XamlDocumentParser::FromXaml(
			stateCanonical, stateCanonicalRoundTrip, &resourceError));
		CUI_EXPECT_TRUE(stateCanonicalRoundTrip == stateResourceDocument);
		const auto stateSnapshot =
			DesignerModel::DesignDocumentSerializer::ToXml(stateResourceDocument);
		DesignerModel::DesignDocument stateSnapshotRoundTrip;
		if (!DesignerModel::DesignDocumentSerializer::FromXml(
			stateSnapshot, stateSnapshotRoundTrip, &resourceError))
			throw std::runtime_error("state resource snapshot failed: "
				+ Convert::UnicodeToUtf8(resourceError));
		CUI_EXPECT_TRUE(stateSnapshotRoundTrip == stateResourceDocument);
		auto mismatchedStateSnapshot = stateSnapshot;
		const auto stateSnapshotReference = mismatchedStateSnapshot.find(
			"storyboardResource=\"StatePulse\"");
		CUI_EXPECT_TRUE(stateSnapshotReference != std::string::npos);
		mismatchedStateSnapshot.replace(stateSnapshotReference,
			std::string("storyboardResource=\"StatePulse\"").size(),
			"storyboardResource=\"MissingStatePulse\"");
		CUI_EXPECT_FALSE(DesignerModel::DesignDocumentSerializer::FromXml(
			mismatchedStateSnapshot, stateSnapshotRoundTrip, &resourceError));
		auto missingStateResourceXaml = stateResourceXaml;
		const auto stateReference = missingStateResourceXaml.find(
			"{StaticResource StatePulse}");
		CUI_EXPECT_TRUE(stateReference != std::string::npos);
		missingStateResourceXaml.replace(stateReference,
			std::string("{StaticResource StatePulse}").size(),
			"{StaticResource MissingStatePulse}");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			missingStateResourceXaml, rejectedResourceDocument, &resourceError));
		auto dynamicStateResourceXaml = stateResourceXaml;
		const auto dynamicReference = dynamicStateResourceXaml.find(
			"{StaticResource StatePulse}");
		CUI_EXPECT_TRUE(dynamicReference != std::string::npos);
		dynamicStateResourceXaml.replace(dynamicReference,
			std::string("{StaticResource StatePulse}").size(),
			"{DynamicResource StatePulse}");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			dynamicStateResourceXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"严格 StaticResource")
			!= std::wstring::npos);
		const std::string unusedStoryboardXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="UnusedWindow">
  <Window.Resources>
    <Storyboard x:Key="UnusedPulse">
      <DoubleAnimation Storyboard.TargetProperty="Opacity" From="0" To="1"
        Duration="0:0:0.100" />
    </Storyboard>
  </Window.Resources>
  <Canvas x:Name="content" />
</Window>)XAML";
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			unusedStoryboardXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"没有可验证")
			!= std::wstring::npos);
		auto aliasedStateResourceXaml = stateResourceXaml;
		const auto aliasInsert = aliasedStateResourceXaml.find(
			"<ComponentDefinition x:Key=\"local:StateHost\"");
		CUI_EXPECT_TRUE(aliasInsert != std::string::npos);
		aliasedStateResourceXaml.insert(aliasInsert,
			"<StaticResource x:Key=\"StateAlias\" ResourceKey=\"StatePulse\" />\n    ");
		const auto aliasedReference = aliasedStateResourceXaml.find(
			"{StaticResource StatePulse}");
		CUI_EXPECT_TRUE(aliasedReference != std::string::npos);
		aliasedStateResourceXaml.replace(aliasedReference,
			std::string("{StaticResource StatePulse}").size(),
			"{StaticResource StateAlias}");
		CUI_EXPECT_FALSE(DesignerModel::XamlDocumentParser::FromXaml(
			aliasedStateResourceXaml, rejectedResourceDocument, &resourceError));
		CUI_EXPECT_TRUE(resourceError.find(L"alias 当前不受支持")
			!= std::wstring::npos);
	});

	runner.Add("Animation fixture SnapshotAndReplace aligns Design and AOT", []
	{
		const auto result = ExecuteCorpus(
			ParseCorpus(SnapshotReplaceCorpus()), std::nullopt);
		CUI_EXPECT_EQ(1ULL, result.Fixtures.size());
		const auto& fixture = result.Fixtures.front();
		if (fixture.Error)
			throw std::runtime_error(
				"snapshot replacement fixture self-test failed: " + *fixture.Error);
		CUI_EXPECT_EQ(std::string("passed"), fixture.Status);
		CUI_EXPECT_EQ(5ULL, fixture.Samples.size());
		CUI_EXPECT_NEAR(45.0, fixture.Samples[0].Value, 0.0001);
		CUI_EXPECT_NEAR(45.0, fixture.Samples[1].Value, 0.0001);
		CUI_EXPECT_NEAR(200.0, fixture.Samples[2].Value, 0.0001);
		CUI_EXPECT_NEAR(250.0, fixture.Samples[3].Value, 0.0001);
		CUI_EXPECT_NEAR(45.0, fixture.Samples[4].Value, 0.0001);
		CUI_EXPECT_TRUE(std::all_of(
			fixture.Samples.begin(), fixture.Samples.end(),
			[](const auto& sample) { return sample.IsAnimated; }));
	});

	runner.Add("Animation fixture metadata Double aligns Design and AOT", []
	{
		const auto corpus = ParseCorpus(MetadataDoubleCorpus());
		const auto result = ExecuteCorpus(corpus, std::nullopt);
		CUI_EXPECT_EQ(1ULL, result.Fixtures.size());
		const auto& fixture = result.Fixtures.front();
		if (fixture.Error)
			throw std::runtime_error(
				"metadata-double fixture self-test failed: " + *fixture.Error);
		CUI_EXPECT_EQ(std::string("passed"), fixture.Status);
		CUI_EXPECT_EQ(4ULL, fixture.Samples.size());
		CUI_EXPECT_NEAR(
			0.333333333333333, fixture.Samples[0].Value, 1e-12);
		CUI_EXPECT_FALSE(fixture.Samples[0].IsAnimated);
		CUI_EXPECT_NEAR(
			0.123456789012345, fixture.Samples[1].Value, 1e-12);
		CUI_EXPECT_NEAR(
			0.555555555055555, fixture.Samples[2].Value, 1e-12);
		CUI_EXPECT_NEAR(
			0.987654321098765, fixture.Samples[3].Value, 1e-12);
		CUI_EXPECT_TRUE(fixture.Samples[1].IsAnimated);
		CUI_EXPECT_TRUE(fixture.Samples[3].IsAnimated);

		auto looseTolerance = MetadataDoubleCorpus();
		const auto tolerance = looseTolerance.find("tolerance=\"0.000001\"");
		CUI_EXPECT_TRUE(tolerance != std::string::npos);
		looseTolerance.replace(tolerance, std::string("tolerance=\"0.000001\"").size(),
			"tolerance=\"0.0001\"");
		bool rejected = false;
		try { (void)ParseCorpus(looseTolerance); }
		catch (const std::exception&) { rejected = true; }
		CUI_EXPECT_TRUE(rejected);
	});

	runner.Add("Animation fixture gaps and CLI ownership stay explicit", []
	{
		const auto gapCorpus = ParseCorpus(MinimalCorpus("expected-gap"));
		const auto gapResult = ExecuteCorpus(gapCorpus, std::nullopt);
		CUI_EXPECT_EQ(std::string("expected-gap"),
			gapResult.Fixtures.front().Status);
		CUI_EXPECT_TRUE(gapResult.Fixtures.front().Samples.empty());
		const auto deferredCorpus = ParseCorpus(MinimalCorpus("deferred"));
		const auto deferredResult = ExecuteCorpus(deferredCorpus, std::nullopt);
		CUI_EXPECT_EQ(std::string("deferred"),
			deferredResult.Fixtures.front().Status);

		auto synchronousXml = MinimalCorpus("expected-gap");
		const auto compare = synchronousXml.find("compare=\"value,isAnimated\"");
		CUI_EXPECT_TRUE(compare != std::string::npos);
		synchronousXml.insert(
			compare + std::string("compare=\"value,isAnimated\"").size(),
			" oracle=\"synchronous-control\"");
		const auto samples = synchronousXml.find("    <samples>");
		CUI_EXPECT_TRUE(samples != std::string::npos);
		synchronousXml.insert(samples,
			"    <operations><operation atMilliseconds=\"0\" "
			"kind=\"seek-aligned\" value=\"25\" /></operations>\n");
		const auto synchronousCorpus = ParseCorpus(synchronousXml);
		CUI_EXPECT_EQ(std::string("synchronous-control"),
			synchronousCorpus.Fixtures.front().Oracle);
		CUI_EXPECT_EQ(1ULL,
			synchronousCorpus.Fixtures.front().Operations.size());
		const auto synchronousResult = ExecuteCorpus(
			synchronousCorpus, std::nullopt);
		CUI_EXPECT_EQ(std::string("expected-gap"),
			synchronousResult.Fixtures.front().Status);

		const auto extendedEasing = ParseCorpus(MinimalCorpus(
			"supported",
			R"XAML(<DoubleAnimation Storyboard.TargetName="target" Storyboard.TargetProperty="(Canvas.Left)" From="0" To="10" Duration="0:0:0.100"><DoubleAnimation.EasingFunction><BackEase EasingMode="EaseIn" Amplitude="1.5" /></DoubleAnimation.EasingFunction></DoubleAnimation>)XAML"));
		const auto extendedEasingResult = ExecuteCorpus(
			extendedEasing, std::nullopt);
		CUI_EXPECT_EQ(std::string("passed"),
			extendedEasingResult.Fixtures.front().Status);
		CUI_EXPECT_NEAR(-6.25,
			extendedEasingResult.Fixtures.front().Samples[1].Value, 0.000001);

		auto rejects = [](std::string xml)
		{
			bool rejected = false;
			try { (void)ParseCorpus(xml); }
			catch (const std::exception&) { rejected = true; }
			CUI_EXPECT_TRUE(rejected);
		};
		auto missingOperations = MinimalCorpus("expected-gap");
		const auto missingCompare = missingOperations.find(
			"compare=\"value,isAnimated\"");
		missingOperations.insert(
			missingCompare + std::string("compare=\"value,isAnimated\"").size(),
			" oracle=\"synchronous-control\"");
		rejects(std::move(missingOperations));
		auto pendingOperation = synchronousXml;
		const auto alignedKind = pendingOperation.find("kind=\"seek-aligned\"");
		pendingOperation.replace(alignedKind,
			std::string("kind=\"seek-aligned\"").size(), "kind=\"pause\"");
		rejects(std::move(pendingOperation));

		const std::vector<std::wstring_view> unrelated{
			L"--cui-shared-graphics-acquire-child" };
		CUI_EXPECT_FALSE(ParseCommandLineArguments(unrelated).Requested);
		const std::vector<std::wstring_view> requested{
			L"--animation-fixtures", L"fixtures.xml",
			L"--animation-case", L"self-test" };
		const auto parsed = ParseCommandLineArguments(requested);
		CUI_EXPECT_TRUE(parsed.Requested);
		CUI_EXPECT_TRUE(parsed.Options.has_value());
		CUI_EXPECT_EQ(std::string("self-test"),
			*parsed.Options->ExactFixtureId);
		CUI_EXPECT_TRUE(IsUnderAnimationWorkplanRoot(
			L"CUI-Workplans/WPF-Animation-Alignment/runs/self-test.json"));
		CUI_EXPECT_FALSE(IsUnderAnimationWorkplanRoot(
			L"AnimationConformance/self-test.json"));
	});
}
