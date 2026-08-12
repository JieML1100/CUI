#include "CodeGenerator.h"
#include "BindingConverterCatalog.h"
#include "DesignerEventCatalog.h"
#include "DesignerModel/AtomicFile.h"
#include "DesignerModel/CppUserCodeIndex.h"
#include "DesignerModel/DesignDataResourceUtils.h"
#include "DesignerModel/DesignDocumentGraph.h"
#include "DesignerBindingUtils.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "DesignerModel/StoryboardPropertyPath.h"
#include "../CuiRuntime/include/XamlDocumentCompiler.h"
#include "../CuiRuntime/include/XamlObjectMaterializer.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include <GroupStyle.h>
#include <RichTextDocument.h>
#include <algorithm>
#include <array>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <functional>
#include <iterator>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <map>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

static bool IsCppKeyword(const std::string& s);

namespace
{
	constexpr std::uint64_t GeneratedTemplatePartTokenValue(
		std::wstring_view name) noexcept
	{
		if (name.empty()) return 0;
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto codeUnit : name)
		{
			hash ^= static_cast<std::uint64_t>(codeUnit);
			hash *= 1099511628211ULL;
		}
		return hash == 0 ? 1 : hash;
	}

	static std::string GeneratedTemplatePartTokenExpression(
		std::wstring_view name)
	{
		const auto token = GeneratedTemplatePartTokenValue(name);
		if (token == 0)
			throw std::invalid_argument(
				"Static template part cannot have an empty token");
		return "TemplatePartToken{ " + std::to_string(token) + "ULL }";
	}

	static void ValidateGeneratedTemplatePart(
		std::map<std::uint64_t, std::wstring>& namesByToken,
		const std::wstring& name,
		const char* context)
	{
		const auto token = GeneratedTemplatePartTokenValue(name);
		if (token == 0)
			throw std::invalid_argument(
				std::string(context) + " contains an empty template-part token");
		const auto [found, inserted] = namesByToken.emplace(token, name);
		if (inserted) return;
		if (found->second == name)
			throw std::invalid_argument(
				std::string(context) + " contains a duplicate template-part name");
		throw std::invalid_argument(
			std::string(context) + " contains a TemplatePartToken collision");
	}

	constexpr std::uint64_t GeneratedInteractionNameTokenValue(
		std::wstring_view name) noexcept
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto character : name)
		{
			const auto codeUnit = static_cast<std::uint32_t>(character);
			for (unsigned int byte = 0; byte < sizeof(wchar_t); ++byte)
			{
				hash ^= static_cast<unsigned char>(codeUnit >> (byte * 8));
				hash *= 1099511628211ULL;
			}
		}
		return hash == 0 ? 1 : hash;
	}

	static std::uint64_t GeneratedBindingSourcePropertyTokenValue(
		std::wstring_view name)
	{
		const auto token = MakeBindingSourcePropertyToken(name);
		if (!token)
			throw std::invalid_argument(
				"Static Binding source property cannot have an empty token");
		return token.Value;
	}

	static std::string GeneratedBindingSourcePropertyTokenExpression(
		std::wstring_view name)
	{
		return "BindingSourcePropertyToken{ "
			+ std::to_string(
				GeneratedBindingSourcePropertyTokenValue(name))
			+ "ULL }";
	}

	static std::uint64_t GeneratedDataTypeTokenValue(
		std::wstring_view name)
	{
		const auto token = MakeDataTypeToken(name);
		if (!token)
			throw std::invalid_argument(
				"Static data type cannot have an empty token");
		return token.Value;
	}

	static std::string GeneratedDataTypeTokenExpression(
		std::wstring_view name)
	{
		return "DataTypeToken{ "
			+ std::to_string(GeneratedDataTypeTokenValue(name))
			+ "ULL }";
	}

	static std::string GeneratedInt64Literal(long long value)
	{
		if (value == (std::numeric_limits<long long>::min)())
			return "(-9223372036854775807LL - 1LL)";
		return std::to_string(value) + "LL";
	}

	static std::uint64_t GeneratedComponentPropertyTokenValue(
		std::wstring_view name)
	{
		const auto token = MakeComponentPropertyToken(name);
		if (!token)
			throw std::invalid_argument(
				"Static component property cannot have an empty token");
		return token.Value;
	}

	static std::uint64_t GeneratedComponentTypeTokenValue(
		std::wstring_view namespaceUri,
		std::wstring_view localName)
	{
		const auto token = MakeComponentTypeToken(namespaceUri, localName);
		if (!token)
			throw std::invalid_argument(
				"Static component type cannot have an empty token");
		return token.Value;
	}

	static std::string GeneratedComponentTypeTokenExpression(
		std::wstring_view namespaceUri,
		std::wstring_view localName)
	{
		return "ComponentTypeToken{ "
			+ std::to_string(GeneratedComponentTypeTokenValue(
				namespaceUri, localName))
			+ "ULL }";
	}

	static std::wstring GeneratedAncestorLocalTypeName(
		std::wstring_view typeName)
	{
		const auto separator = typeName.find_last_of(L":.");
		return std::wstring(separator == std::wstring_view::npos
			? typeName : typeName.substr(separator + 1));
	}

	static std::string GeneratedFindAncestorTypeExpression(
		const DesignerModel::DesignDocument& document,
		const DesignerDataBinding& binding)
	{
		const auto localName = GeneratedAncestorLocalTypeName(
			binding.AncestorType);
		if (binding.AncestorTypeNamespace.empty())
		{
			UIClass ancestorType = UIClass::UI_Base;
			if (!DesignerStyleSheetUtils::TryParseUIClass(
					localName, ancestorType))
				throw std::invalid_argument(
					"Static FindAncestor type is not a native CUI class");
			return "static_cast<UIClass>("
				+ std::to_string(static_cast<int>(ancestorType)) + ")";
		}
		if (!document.FindComponent(
				binding.AncestorTypeNamespace, localName))
			throw std::invalid_argument(
				"Static FindAncestor component type is not declared");
		return GeneratedComponentTypeTokenExpression(
			binding.AncestorTypeNamespace, localName);
	}

	static const char* CompiledMemberPathAccessor(
		std::wstring_view propertyName) noexcept
	{
		if (propertyName == L"DisplayMemberPath")
			return "DisplayMemberPath";
		if (propertyName == L"SelectedValuePath")
			return "SelectedValuePath";
		if (propertyName == L"HeaderDisplayMemberPath")
			return "HeaderDisplayMemberPath";
		return nullptr;
	}

	static const char* GeneratedBindingValueKindExpression(
		BindingValueKind kind) noexcept
	{
		switch (kind)
		{
		case BindingValueKind::Empty:
			return "BindingValueKind::Empty";
		case BindingValueKind::Bool:
			return "BindingValueKind::Bool";
		case BindingValueKind::NullableBool:
			return "BindingValueKind::NullableBool";
		case BindingValueKind::Int:
			return "BindingValueKind::Int";
		case BindingValueKind::Int64:
			return "BindingValueKind::Int64";
		case BindingValueKind::Float:
			return "BindingValueKind::Float";
		case BindingValueKind::Double:
			return "BindingValueKind::Double";
		case BindingValueKind::String:
			return "BindingValueKind::String";
		case BindingValueKind::Object:
			return "BindingValueKind::Object";
		}
		return "BindingValueKind::Empty";
	}

	static std::string GeneratedBindingPathCapabilitiesExpression(
		bool canRead,
		bool canWrite,
		bool canObserve)
	{
		std::string result;
		auto append = [&](const char* value)
		{
			if (!result.empty()) result += " | ";
			result += value;
		};
		if (canRead)
			append("CompiledBindingPathCapabilities::Read");
		if (canWrite)
			append("CompiledBindingPathCapabilities::Write");
		if (canObserve)
			append("CompiledBindingPathCapabilities::Observe");
		return result.empty()
			? "CompiledBindingPathCapabilities::None"
			: result;
	}

	static void ValidateGeneratedBindingSourcePropertyTokens(
		const DesignerDataContextSchema& schema,
		const std::string& context)
	{
		std::map<std::uint64_t, std::wstring> namesByToken;
		for (const auto& property : schema)
		{
			std::vector<BindingPathStep> steps;
			if (!TryParseBindingPropertyPath(property.Path, steps))
				throw std::invalid_argument(
					context + " contains an invalid binding-source path");
			for (const auto& step : steps)
			{
				const auto token =
					GeneratedBindingSourcePropertyTokenValue(step.Value);
				const auto [found, inserted] =
					namesByToken.emplace(token, step.Value);
				if (!inserted && found->second != step.Value)
					throw std::invalid_argument(
						context
						+ " contains a BindingSourcePropertyToken collision");
			}
		}
	}

	struct GeneratedCompiledObjectPath final
	{
		std::wstring RootProperty;
		std::string Kind;
		std::string Member;
		std::string ExpectedObjectKind = "static_cast<uint8_t>(0xFFu)";
		std::string ExpectedAuxiliaryKind = "0u";
		std::string Flags = "CompiledStoryboardObjectPathFlags::None";
		std::uint32_t Index0 = 0;
		std::uint32_t Index1 = 0;
		std::vector<std::uint32_t> ChildIndices;
		std::uint64_t Identity = 0;
	};

	static std::uint32_t CheckedCompiledInteractionIndex(
		size_t value,
		const char* context)
	{
		if (value >= static_cast<size_t>(
			(std::numeric_limits<std::uint32_t>::max)()))
			throw std::length_error(std::string(context)
				+ " exceeds the compiled interaction limit");
		return static_cast<std::uint32_t>(value);
	}

	static std::string CompiledTransformKindExpression(
		std::wstring_view owner)
	{
		const auto localOwner = DesignerModel::StoryboardPathLocalType(
			std::wstring(owner));
		if (localOwner == L"TranslateTransform")
			return "static_cast<uint8_t>(cui::drawing::TransformKind::Translate)";
		if (localOwner == L"ScaleTransform")
			return "static_cast<uint8_t>(cui::drawing::TransformKind::Scale)";
		if (localOwner == L"RotateTransform")
			return "static_cast<uint8_t>(cui::drawing::TransformKind::Rotate)";
		if (localOwner == L"SkewTransform")
			return "static_cast<uint8_t>(cui::drawing::TransformKind::Skew)";
		if (localOwner == L"MatrixTransform")
			return "static_cast<uint8_t>(cui::drawing::TransformKind::Matrix)";
		throw std::invalid_argument(
			"Compiled Storyboard path has an unknown Transform owner");
	}

	static std::string CompiledTransformMemberExpression(
		std::wstring_view member)
	{
		if (member == L"X")
			return "CompiledStoryboardObjectPathMember::TransformX";
		if (member == L"Y")
			return "CompiledStoryboardObjectPathMember::TransformY";
		if (member == L"ScaleX")
			return "CompiledStoryboardObjectPathMember::TransformScaleX";
		if (member == L"ScaleY")
			return "CompiledStoryboardObjectPathMember::TransformScaleY";
		if (member == L"Angle")
			return "CompiledStoryboardObjectPathMember::TransformAngle";
		if (member == L"AngleX")
			return "CompiledStoryboardObjectPathMember::TransformAngleX";
		if (member == L"AngleY")
			return "CompiledStoryboardObjectPathMember::TransformAngleY";
		if (member == L"CenterX")
			return "CompiledStoryboardObjectPathMember::TransformCenterX";
		if (member == L"CenterY")
			return "CompiledStoryboardObjectPathMember::TransformCenterY";
		if (member == L"Matrix")
			return "CompiledStoryboardObjectPathMember::TransformMatrix";
		throw std::invalid_argument(
			"Compiled Storyboard path has an unknown Transform member");
	}

	static std::string CompiledGeometryKindExpression(
		std::wstring_view owner)
	{
		const auto localOwner = DesignerModel::StoryboardPathLocalType(
			std::wstring(owner));
		if (localOwner == L"RectangleGeometry")
			return "static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle)";
		if (localOwner == L"EllipseGeometry")
			return "static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse)";
		if (localOwner == L"PathGeometry")
			return "static_cast<uint8_t>(cui::drawing::GeometryKind::Path)";
		if (localOwner == L"GeometryGroup")
			return "static_cast<uint8_t>(cui::drawing::GeometryKind::Group)";
		return "static_cast<uint8_t>(0xFFu)";
	}

	static std::string CompiledGeometryMemberExpression(
		std::wstring_view member)
	{
		if (member == L"Rect")
			return "CompiledStoryboardObjectPathMember::GeometryRect";
		if (member == L"Center")
			return "CompiledStoryboardObjectPathMember::GeometryCenter";
		if (member == L"RadiusX")
			return "CompiledStoryboardObjectPathMember::GeometryRadiusX";
		if (member == L"RadiusY")
			return "CompiledStoryboardObjectPathMember::GeometryRadiusY";
		if (member == L"FillRule")
			return "CompiledStoryboardObjectPathMember::GeometryFillRule";
		throw std::invalid_argument(
			"Compiled Storyboard path has an unknown Geometry member");
	}

	static std::string CompiledPathSegmentKindExpression(
		std::wstring_view owner)
	{
		const auto localOwner = DesignerModel::StoryboardPathLocalType(
			std::wstring(owner));
		if (localOwner == L"LineSegment")
			return "static_cast<uint8_t>(cui::drawing::PathSegmentKind::Line)";
		if (localOwner == L"BezierSegment")
			return "static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier)";
		if (localOwner == L"QuadraticBezierSegment")
			return "static_cast<uint8_t>(cui::drawing::PathSegmentKind::QuadraticBezier)";
		if (localOwner == L"ArcSegment")
			return "static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc)";
		return "static_cast<uint8_t>(0xFFu)";
	}

	static std::string CompiledPathMemberExpression(
		std::wstring_view owner,
		std::wstring_view member)
	{
		const auto localOwner = DesignerModel::StoryboardPathLocalType(
			std::wstring(owner));
		if (localOwner == L"PathFigure")
		{
			if (member == L"StartPoint")
				return "CompiledStoryboardObjectPathMember::PathFigureStartPoint";
			if (member == L"IsClosed")
				return "CompiledStoryboardObjectPathMember::PathFigureIsClosed";
			if (member == L"IsFilled")
				return "CompiledStoryboardObjectPathMember::PathFigureIsFilled";
		}
		if (member == L"Point")
			return "CompiledStoryboardObjectPathMember::PathSegmentPoint";
		if (member == L"Point1")
			return "CompiledStoryboardObjectPathMember::PathSegmentPoint1";
		if (member == L"Point2")
			return "CompiledStoryboardObjectPathMember::PathSegmentPoint2";
		if (member == L"Point3")
			return "CompiledStoryboardObjectPathMember::PathSegmentPoint3";
		if (member == L"Size")
			return "CompiledStoryboardObjectPathMember::PathArcSize";
		if (member == L"RotationAngle")
			return "CompiledStoryboardObjectPathMember::PathArcRotationAngle";
		if (member == L"IsLargeArc")
			return "CompiledStoryboardObjectPathMember::PathArcIsLargeArc";
		if (member == L"SweepDirection")
			return "CompiledStoryboardObjectPathMember::PathArcSweepDirection";
		throw std::invalid_argument(
			"Compiled Storyboard path has an unknown PathGeometry member");
	}

	static std::string CompiledBrushKindExpression(
		std::wstring_view owner)
	{
		const auto localOwner = DesignerModel::StoryboardPathLocalType(
			std::wstring(owner));
		if (localOwner == L"SolidColorBrush")
			return "static_cast<uint8_t>(cui::drawing::BrushKind::Solid)";
		if (localOwner == L"LinearGradientBrush")
			return "static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient)";
		if (localOwner == L"RadialGradientBrush")
			return "static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient)";
		if (localOwner == L"ImageBrush")
			return "static_cast<uint8_t>(cui::drawing::BrushKind::Image)";
		return "static_cast<uint8_t>(0xFFu)";
	}

	static std::string CompiledBrushMemberExpression(
		DesignerModel::StoryboardObjectPathKind kind,
		std::wstring_view member)
	{
		using Kind = DesignerModel::StoryboardObjectPathKind;
		if (kind == Kind::SolidColorBrushColor)
			return "CompiledStoryboardObjectPathMember::BrushSolidColor";
		if (kind == Kind::GradientStopColor)
			return "CompiledStoryboardObjectPathMember::BrushGradientStopColor";
		if (kind == Kind::GradientStopOffset)
			return "CompiledStoryboardObjectPathMember::BrushGradientStopOffset";
		if (member == L"Opacity")
			return "CompiledStoryboardObjectPathMember::BrushOpacity";
		if (member == L"StartPoint")
			return "CompiledStoryboardObjectPathMember::BrushStartPoint";
		if (member == L"EndPoint")
			return "CompiledStoryboardObjectPathMember::BrushEndPoint";
		if (member == L"Center")
			return "CompiledStoryboardObjectPathMember::BrushCenter";
		if (member == L"GradientOrigin")
			return "CompiledStoryboardObjectPathMember::BrushGradientOrigin";
		if (member == L"RadiusX")
			return "CompiledStoryboardObjectPathMember::BrushRadiusX";
		if (member == L"RadiusY")
			return "CompiledStoryboardObjectPathMember::BrushRadiusY";
		throw std::invalid_argument(
			"Compiled Storyboard path has an unknown Brush member");
	}

	static GeneratedCompiledObjectPath DecodeGeneratedCompiledObjectPath(
		const std::wstring& text)
	{
		using DesignerKind = DesignerModel::StoryboardObjectPathKind;
		using SegmentKind = cui::xaml::PropertyPathSegmentKind;
		const auto classified = DesignerModel::ClassifyStoryboardObjectPath(text);
		if (classified == DesignerKind::None)
			throw std::invalid_argument(
				"Compiled Storyboard object path is not registered");

		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError)
			|| path.Segments.size() < 2
			|| path.Segments.front().Kind != SegmentKind::Property)
			throw std::invalid_argument(
				"Compiled Storyboard object path cannot be parsed");

		GeneratedCompiledObjectPath output;
		output.RootProperty = path.Segments.front().Name;
		const auto canonical = path.CanonicalText();
		output.Identity = GeneratedInteractionNameTokenValue(canonical);
		auto checkedPathIndex = [](size_t value, const char* context)
		{
			return CheckedCompiledInteractionIndex(value, context);
		};
		auto appendGeometryChildren = [&](size_t& cursor)
		{
			while (cursor + 1 < path.Segments.size()
				&& path.Segments[cursor].Kind == SegmentKind::Property
				&& DesignerModel::StoryboardPathLocalType(
					path.Segments[cursor].OwnerType) == L"GeometryGroup"
				&& path.Segments[cursor].Name == L"Children"
				&& path.Segments[cursor + 1].Kind == SegmentKind::Index)
			{
				output.ChildIndices.push_back(checkedPathIndex(
					path.Segments[cursor + 1].Index,
					"Storyboard Geometry child index"));
				cursor += 2;
			}
		};

		switch (classified)
		{
		case DesignerKind::RenderTransform:
		case DesignerKind::RenderTransformMatrix:
		{
			output.Kind = "CompiledStoryboardObjectPathKind::Transform";
			const auto& leaf = path.Segments.back();
			const bool direct = path.Segments.size() == 2;
			const bool grouped = path.Segments.size() == 4
				&& path.Segments[1].Kind == SegmentKind::Property
				&& DesignerModel::StoryboardPathLocalType(
					path.Segments[1].OwnerType) == L"TransformGroup"
				&& path.Segments[1].Name == L"Children"
				&& path.Segments[2].Kind == SegmentKind::Index;
			if (!direct && !grouped)
				throw std::invalid_argument(
					"Compiled RenderTransform path has an invalid shape");
			output.ExpectedObjectKind = CompiledTransformKindExpression(
				leaf.OwnerType);
			output.Member = CompiledTransformMemberExpression(leaf.Name);
			if (grouped)
				output.Index0 = checkedPathIndex(path.Segments[2].Index,
					"Storyboard Transform operation index");
			break;
		}
		case DesignerKind::RectangleGeometryRect:
		case DesignerKind::RectangleGeometryRadius:
		case DesignerKind::EllipseGeometryCenter:
		case DesignerKind::EllipseGeometryRadius:
		case DesignerKind::GeometryFillRule:
		{
			output.Kind = "CompiledStoryboardObjectPathKind::Geometry";
			size_t cursor = 1;
			appendGeometryChildren(cursor);
			if (cursor + 1 != path.Segments.size())
				throw std::invalid_argument(
					"Compiled Geometry path has an invalid leaf shape");
			const auto& leaf = path.Segments[cursor];
			output.ExpectedObjectKind = CompiledGeometryKindExpression(
				leaf.OwnerType);
			output.Member = CompiledGeometryMemberExpression(leaf.Name);
			break;
		}
		case DesignerKind::PathGeometryPoint:
		case DesignerKind::PathGeometrySize:
		case DesignerKind::PathGeometryDouble:
		case DesignerKind::PathGeometryBool:
		case DesignerKind::PathGeometrySweep:
		{
			output.Kind = "CompiledStoryboardObjectPathKind::PathGeometry";
			size_t cursor = 1;
			appendGeometryChildren(cursor);
			if (cursor + 2 >= path.Segments.size()
				|| path.Segments[cursor].Name != L"Figures"
				|| path.Segments[cursor + 1].Kind != SegmentKind::Index)
				throw std::invalid_argument(
					"Compiled PathGeometry path has no figure index");
			output.Index0 = checkedPathIndex(path.Segments[cursor + 1].Index,
				"Storyboard PathFigure index");
			cursor += 2;
			if (cursor + 2 < path.Segments.size()
				&& path.Segments[cursor].Name == L"Segments"
				&& path.Segments[cursor + 1].Kind == SegmentKind::Index)
			{
				output.Flags =
					"CompiledStoryboardObjectPathFlags::HasPathSegment";
				output.Index1 = checkedPathIndex(path.Segments[cursor + 1].Index,
					"Storyboard PathSegment index");
				cursor += 2;
			}
			if (cursor + 1 != path.Segments.size())
				throw std::invalid_argument(
					"Compiled PathGeometry path has an invalid leaf shape");
			const auto& leaf = path.Segments[cursor];
			output.ExpectedObjectKind = CompiledPathSegmentKindExpression(
				leaf.OwnerType);
			output.Member = CompiledPathMemberExpression(
				leaf.OwnerType, leaf.Name);
			break;
		}
		case DesignerKind::GeometryTransform:
		case DesignerKind::GeometryTransformMatrix:
		{
			output.Kind =
				"CompiledStoryboardObjectPathKind::GeometryTransform";
			size_t cursor = 1;
			appendGeometryChildren(cursor);
			if (cursor + 4 != path.Segments.size()
				|| path.Segments[cursor].Name != L"Transform"
				|| path.Segments[cursor + 1].Kind != SegmentKind::Property
				|| DesignerModel::StoryboardPathLocalType(
					path.Segments[cursor + 1].OwnerType) != L"TransformGroup"
				|| path.Segments[cursor + 1].Name != L"Children"
				|| path.Segments[cursor + 2].Kind != SegmentKind::Index)
				throw std::invalid_argument(
					"Compiled GeometryTransform path has an invalid shape");
			output.Index0 = checkedPathIndex(path.Segments[cursor + 2].Index,
				"Storyboard Geometry Transform operation index");
			const auto& leaf = path.Segments[cursor + 3];
			output.ExpectedAuxiliaryKind = CompiledTransformKindExpression(
				leaf.OwnerType);
			output.Member = CompiledTransformMemberExpression(leaf.Name);
			break;
		}
		case DesignerKind::SolidColorBrushColor:
		case DesignerKind::BrushOpacity:
		case DesignerKind::GradientBrushPoint:
		case DesignerKind::RadialGradientBrushRadius:
		case DesignerKind::GradientStopColor:
		case DesignerKind::GradientStopOffset:
		{
			output.Kind = "CompiledStoryboardObjectPathKind::Brush";
			const bool gradientStop = classified == DesignerKind::GradientStopColor
				|| classified == DesignerKind::GradientStopOffset;
			const auto& ownerSegment = path.Segments[1];
			const auto& leaf = path.Segments.back();
			output.ExpectedObjectKind = CompiledBrushKindExpression(
				ownerSegment.OwnerType);
			output.Member = CompiledBrushMemberExpression(classified, leaf.Name);
			if (gradientStop)
			{
				if (path.Segments.size() != 4
					|| path.Segments[2].Kind != SegmentKind::Index)
					throw std::invalid_argument(
						"Compiled GradientStop path has an invalid shape");
				output.Index0 = checkedPathIndex(path.Segments[2].Index,
					"Storyboard GradientStop index");
			}
			break;
		}
		case DesignerKind::BrushTransform:
		case DesignerKind::BrushTransformMatrix:
		case DesignerKind::BrushRelativeTransform:
		case DesignerKind::BrushRelativeTransformMatrix:
		{
			output.Kind =
				"CompiledStoryboardObjectPathKind::BrushTransform";
			if (path.Segments.size() != 5
				|| path.Segments[1].Kind != SegmentKind::Property
				|| (path.Segments[1].Name != L"Transform"
					&& path.Segments[1].Name != L"RelativeTransform")
				|| path.Segments[2].Kind != SegmentKind::Property
				|| DesignerModel::StoryboardPathLocalType(
					path.Segments[2].OwnerType) != L"TransformGroup"
				|| path.Segments[2].Name != L"Children"
				|| path.Segments[3].Kind != SegmentKind::Index)
				throw std::invalid_argument(
					"Compiled BrushTransform path has an invalid shape");
			output.ExpectedObjectKind = CompiledBrushKindExpression(
				path.Segments[1].OwnerType);
			output.Index0 = checkedPathIndex(path.Segments[3].Index,
				"Storyboard Brush Transform operation index");
			const auto& leaf = path.Segments[4];
			output.ExpectedAuxiliaryKind = CompiledTransformKindExpression(
				leaf.OwnerType);
			output.Member = CompiledTransformMemberExpression(leaf.Name);
			if (path.Segments[1].Name == L"RelativeTransform")
				output.Flags =
					"CompiledStoryboardObjectPathFlags::RelativeTransform";
			break;
		}
		case DesignerKind::None:
		default:
			throw std::invalid_argument(
				"Compiled Storyboard object path kind is unsupported");
		}
		return output;
	}

	struct GeneratedCompiledRange final
	{
		size_t Offset = 0;
		size_t Count = 0;
	};

	static std::string GeneratedCompiledIndexExpression(
		size_t value,
		const char* context)
	{
		return std::to_string(CheckedCompiledInteractionIndex(value, context))
			+ "u";
	}

	static std::string GeneratedCompiledOptionalIndexExpression(
		const std::optional<size_t>& value,
		const char* context)
	{
		return value ? GeneratedCompiledIndexExpression(*value, context)
			: std::string("CompiledInteractionInvalidIndex");
	}

	static std::string GeneratedCompiledRangeExpression(
		const GeneratedCompiledRange& range)
	{
		return "{ " + GeneratedCompiledIndexExpression(
			range.Offset, "Compiled interaction range offset") + ", "
			+ GeneratedCompiledIndexExpression(
				range.Count, "Compiled interaction range count") + " }";
	}

	static const char* GeneratedAnimationKindExpression(
		DeclarativeAnimationKind value) noexcept
	{
		switch (value)
		{
		case DeclarativeAnimationKind::Color:
			return "DeclarativeAnimationKind::Color";
		case DeclarativeAnimationKind::Thickness:
			return "DeclarativeAnimationKind::Thickness";
		case DeclarativeAnimationKind::Point:
			return "DeclarativeAnimationKind::Point";
		case DeclarativeAnimationKind::Vector:
			return "DeclarativeAnimationKind::Vector";
		case DeclarativeAnimationKind::Rect:
			return "DeclarativeAnimationKind::Rect";
		case DeclarativeAnimationKind::Size:
			return "DeclarativeAnimationKind::Size";
		case DeclarativeAnimationKind::Matrix:
			return "DeclarativeAnimationKind::Matrix";
		case DeclarativeAnimationKind::Object:
			return "DeclarativeAnimationKind::Object";
		case DeclarativeAnimationKind::Double:
		default:
			return "DeclarativeAnimationKind::Double";
		}
	}

	static const char* GeneratedEasingExpression(
		DeclarativeEasingKind value) noexcept
	{
		switch (value)
		{
		case DeclarativeEasingKind::Quadratic:
			return "DeclarativeEasingKind::Quadratic";
		case DeclarativeEasingKind::Cubic:
			return "DeclarativeEasingKind::Cubic";
		case DeclarativeEasingKind::Sine:
			return "DeclarativeEasingKind::Sine";
		case DeclarativeEasingKind::Linear:
		default:
			return "DeclarativeEasingKind::Linear";
		}
	}

	static const char* GeneratedEasingModeExpression(
		DeclarativeEasingMode value) noexcept
	{
		switch (value)
		{
		case DeclarativeEasingMode::EaseIn:
			return "DeclarativeEasingMode::EaseIn";
		case DeclarativeEasingMode::EaseInOut:
			return "DeclarativeEasingMode::EaseInOut";
		case DeclarativeEasingMode::EaseOut:
		default:
			return "DeclarativeEasingMode::EaseOut";
		}
	}

	static const char* GeneratedKeyFrameKindExpression(
		DeclarativeKeyFrameKind value) noexcept
	{
		switch (value)
		{
		case DeclarativeKeyFrameKind::Discrete:
			return "DeclarativeKeyFrameKind::Discrete";
		case DeclarativeKeyFrameKind::Easing:
			return "DeclarativeKeyFrameKind::Easing";
		case DeclarativeKeyFrameKind::Spline:
			return "DeclarativeKeyFrameKind::Spline";
		case DeclarativeKeyFrameKind::Linear:
		default:
			return "DeclarativeKeyFrameKind::Linear";
		}
	}

	static const char* GeneratedRepeatBehaviorExpression(
		DeclarativeRepeatBehaviorKind value) noexcept
	{
		switch (value)
		{
		case DeclarativeRepeatBehaviorKind::Duration:
			return "DeclarativeRepeatBehaviorKind::Duration";
		case DeclarativeRepeatBehaviorKind::Forever:
			return "DeclarativeRepeatBehaviorKind::Forever";
		case DeclarativeRepeatBehaviorKind::Count:
		default:
			return "DeclarativeRepeatBehaviorKind::Count";
		}
	}

	static const char* GeneratedFillBehaviorExpression(
		DeclarativeTimelineFillBehavior value) noexcept
	{
		return value == DeclarativeTimelineFillBehavior::Stop
			? "DeclarativeTimelineFillBehavior::Stop"
			: "DeclarativeTimelineFillBehavior::HoldEnd";
	}

	static const char* GeneratedActionKindExpression(
		DeclarativeStoryboardActionKind value) noexcept
	{
		switch (value)
		{
		case DeclarativeStoryboardActionKind::Pause:
			return "DeclarativeStoryboardActionKind::Pause";
		case DeclarativeStoryboardActionKind::Resume:
			return "DeclarativeStoryboardActionKind::Resume";
		case DeclarativeStoryboardActionKind::Stop:
			return "DeclarativeStoryboardActionKind::Stop";
		case DeclarativeStoryboardActionKind::Begin:
		default:
			return "DeclarativeStoryboardActionKind::Begin";
		}
	}

	/**
	 * Shared AOT lowering for VisualState/EventTrigger and Style Trigger
	 * storyboards.  It owns only immutable structural tables; callers own the
	 * call-local BindingValue sidecar and decide whether target slots beyond
	 * the host are legal.
	 */
	struct GeneratedCompiledStoryboardTables final
	{
		using AddValueCallback = std::function<size_t(const BindingValue&)>;
		using PropertyCallback = std::function<std::string(
			const std::wstring&, const std::wstring&, bool)>;
		using TargetCallback = std::function<std::string(const std::wstring&)>;
		using FloatCallback = std::function<std::string(float)>;
		using DoubleCallback = std::function<std::string(double)>;

		struct ActionScope final
		{
			std::unordered_map<
				const DeclarativeEventTriggerActionDefinition*, size_t>
				BeginIndexes;
			std::unordered_map<std::wstring, size_t> NamedBeginIndexes;
		};

		AddValueCallback AddValue;
		PropertyCallback PropertyExpression;
		TargetCallback TargetExpression;
		FloatCallback FloatExpression;
		DoubleCallback DoubleExpression;

		std::vector<std::string> TargetExpressions;
		std::unordered_map<std::string, size_t> TargetIndexes;
		std::vector<std::string> PropertyOperands;
		std::unordered_map<std::string, size_t> PropertyOperandIndexes;
		std::vector<std::string> ObjectPathChildIndices;
		std::vector<std::string> ObjectPaths;
		std::unordered_map<std::wstring, size_t> ObjectPathIndexes;
		std::map<std::uint64_t, std::wstring> ObjectPathIdentities;
		std::vector<std::string> KeyFrames;
		std::vector<std::string> Animations;
		std::vector<GeneratedCompiledRange> Storyboards;
		std::vector<std::string> Actions;

		size_t ResolveTarget(const std::wstring& targetName)
		{
			if (targetName.empty()) return 0;
			const auto expression = TargetExpression
				? TargetExpression(targetName) : std::string{};
			if (expression.empty())
				throw std::invalid_argument(
					"Static declarative interaction target cannot be resolved");
			if (expression == "nullptr") return 0;
			if (const auto found = TargetIndexes.find(expression);
				found != TargetIndexes.end()) return found->second;
			const size_t index = TargetExpressions.size() + 1;
			(void)CheckedCompiledInteractionIndex(index,
				"Compiled interaction target slot");
			TargetExpressions.push_back(expression);
			TargetIndexes.emplace(expression, index);
			return index;
		}

		size_t AddPropertyOperand(
			const std::wstring& targetName,
			const std::wstring& propertyName,
			bool requireWritable)
		{
			const size_t target = ResolveTarget(targetName);
			const auto property = PropertyExpression
				? PropertyExpression(targetName, propertyName, requireWritable)
				: std::string{};
			if (property.empty())
				throw std::invalid_argument(
					"Compiled Storyboard property has no static identity");
			const auto key = std::to_string(target) + ":" + property;
			if (const auto found = PropertyOperandIndexes.find(key);
				found != PropertyOperandIndexes.end()) return found->second;
			const size_t index = PropertyOperands.size();
			(void)CheckedCompiledInteractionIndex(index,
				"Compiled interaction property operand index");
			PropertyOperands.push_back("{ "
				+ GeneratedCompiledIndexExpression(target,
					"Compiled interaction target slot")
				+ ", " + property + " }");
			PropertyOperandIndexes.emplace(key, index);
			return index;
		}

		size_t AddObjectPath(
			const std::wstring& propertyPath,
			std::wstring& rootProperty)
		{
			if (const auto found = ObjectPathIndexes.find(propertyPath);
				found != ObjectPathIndexes.end())
			{
				rootProperty = DecodeGeneratedCompiledObjectPath(propertyPath).
					RootProperty;
				return found->second;
			}
			auto decoded = DecodeGeneratedCompiledObjectPath(propertyPath);
			rootProperty = decoded.RootProperty;
			const auto [identity, inserted] = ObjectPathIdentities.emplace(
				decoded.Identity, propertyPath);
			if (!inserted && identity->second != propertyPath)
				throw std::invalid_argument(
					"Compiled Storyboard object-path identity collision");
			const GeneratedCompiledRange children{
				ObjectPathChildIndices.size(), decoded.ChildIndices.size() };
			for (const auto child : decoded.ChildIndices)
				ObjectPathChildIndices.push_back(std::to_string(child) + "u");
			const size_t index = ObjectPaths.size();
			(void)CheckedCompiledInteractionIndex(index,
				"Compiled Storyboard object-path index");
			ObjectPaths.push_back("{ " + decoded.Kind + ", "
				+ decoded.Member + ", " + decoded.ExpectedObjectKind + ", "
				+ decoded.ExpectedAuxiliaryKind + ", " + decoded.Flags
				+ ", 0u, " + std::to_string(decoded.Index0) + "u, "
				+ std::to_string(decoded.Index1) + "u, "
				+ GeneratedCompiledRangeExpression(children) + ", "
				+ std::to_string(decoded.Identity) + "ULL }");
			ObjectPathIndexes.emplace(propertyPath, index);
			return index;
		}

		size_t AppendAnimation(
			const DeclarativeVisualStateAnimation& animation)
		{
			const auto& propertyPath = animation.PropertyPath();
			std::optional<size_t> objectPathIndex;
			std::wstring operandProperty = propertyPath;
			if (DesignerModel::ClassifyStoryboardObjectPath(propertyPath)
				!= DesignerModel::StoryboardObjectPathKind::None)
				objectPathIndex = AddObjectPath(propertyPath, operandProperty);
			const size_t operandIndex = AddPropertyOperand(
				animation.TargetName, operandProperty, true);
			auto optionalValue = [&](const std::optional<BindingValue>& value)
				-> std::optional<size_t>
				{
					return value ? std::optional<size_t>(AddValue(*value))
						: std::nullopt;
				};
			const auto fromValue = optionalValue(animation.From);
			const auto toValue = optionalValue(animation.To);
			const auto byValue = optionalValue(animation.By);
			const GeneratedCompiledRange keyFrames{
				KeyFrames.size(), animation.KeyFrames.size() };
			for (const auto& frame : animation.KeyFrames)
			{
				const size_t valueIndex = AddValue(frame.Value);
				KeyFrames.push_back("{ "
					+ std::string(GeneratedKeyFrameKindExpression(frame.Kind)) + ", "
					+ std::to_string(frame.KeyTimeMilliseconds) + "ULL, "
					+ GeneratedCompiledIndexExpression(valueIndex,
						"Compiled interaction key-frame value") + ", "
					+ GeneratedEasingExpression(frame.Easing) + ", "
					+ GeneratedEasingModeExpression(frame.EasingMode) + ", "
					+ FloatExpression(frame.KeySplineX1) + ", "
					+ FloatExpression(frame.KeySplineY1) + ", "
					+ FloatExpression(frame.KeySplineX2) + ", "
					+ FloatExpression(frame.KeySplineY2) + " }");
			}
			const size_t index = Animations.size();
			(void)CheckedCompiledInteractionIndex(index,
				"Compiled interaction animation index");
			Animations.push_back("{ "
				+ std::string(GeneratedAnimationKindExpression(animation.Kind)) + ", "
				+ GeneratedCompiledIndexExpression(operandIndex,
					"Compiled interaction animation operand") + ", "
				+ GeneratedCompiledOptionalIndexExpression(objectPathIndex,
					"Compiled Storyboard object-path index") + ", "
				+ GeneratedCompiledOptionalIndexExpression(fromValue,
					"Compiled interaction From value") + ", "
				+ GeneratedCompiledOptionalIndexExpression(toValue,
					"Compiled interaction To value") + ", "
				+ GeneratedCompiledOptionalIndexExpression(byValue,
					"Compiled interaction By value") + ", "
				+ GeneratedCompiledRangeExpression(keyFrames) + ", "
				+ (animation.IsAdditive ? "true" : "false") + ", "
				+ (animation.IsCumulative ? "true" : "false") + ", "
				+ std::to_string(animation.BeginTimeMilliseconds) + "ULL, "
				+ std::to_string(animation.DurationMilliseconds) + "ULL, "
				+ GeneratedRepeatBehaviorExpression(animation.RepeatBehavior) + ", "
				+ DoubleExpression(animation.RepeatCount) + ", "
				+ std::to_string(animation.RepeatDurationMilliseconds) + "ULL, "
				+ (animation.AutoReverse ? "true" : "false") + ", "
				+ GeneratedFillBehaviorExpression(animation.FillBehavior) + ", "
				+ DoubleExpression(animation.SpeedRatio) + ", "
				+ DoubleExpression(animation.AccelerationRatio) + ", "
				+ DoubleExpression(animation.DecelerationRatio) + ", "
				+ GeneratedEasingExpression(animation.Easing) + ", "
				+ GeneratedEasingModeExpression(animation.EasingMode) + " }");
			return index;
		}

		GeneratedCompiledRange AppendAnimations(
			const std::vector<DeclarativeVisualStateAnimation>& animations)
		{
			const GeneratedCompiledRange range{
				Animations.size(), animations.size() };
			for (const auto& animation : animations)
				(void)AppendAnimation(animation);
			return range;
		}

		ActionScope DeclareActionScope(
			const std::vector<const std::vector<
				DeclarativeEventTriggerActionDefinition>*>& lists)
		{
			ActionScope scope;
			for (const auto* list : lists)
			{
				if (!list) continue;
				for (const auto& action : *list)
				{
					if (action.Kind != DeclarativeStoryboardActionKind::Begin)
						continue;
					const size_t storyboardIndex = Storyboards.size();
					(void)CheckedCompiledInteractionIndex(storyboardIndex,
						"Compiled interaction storyboard index");
					Storyboards.push_back({});
					scope.BeginIndexes.emplace(&action, storyboardIndex);
					if (!action.StoryboardName.empty()
						&& !scope.NamedBeginIndexes.emplace(
							action.StoryboardName, storyboardIndex).second)
						throw std::invalid_argument(
							"Compiled BeginStoryboard name is duplicated");
				}
			}
			return scope;
		}

		GeneratedCompiledRange AppendActions(
			const std::vector<DeclarativeEventTriggerActionDefinition>& actions,
			const ActionScope& scope)
		{
			const GeneratedCompiledRange range{ Actions.size(), actions.size() };
			for (const auto& action : actions)
			{
				size_t storyboardIndex = 0;
				if (action.Kind == DeclarativeStoryboardActionKind::Begin)
				{
					const auto found = scope.BeginIndexes.find(&action);
					if (found == scope.BeginIndexes.end() || action.Animations.empty())
						throw std::invalid_argument(
							"Compiled BeginStoryboard has no storyboard slot");
					storyboardIndex = found->second;
					Storyboards[storyboardIndex] = AppendAnimations(action.Animations);
				}
				else
				{
					if (!action.Animations.empty())
						throw std::invalid_argument(
							"Compiled Storyboard control action has animations");
					if (action.StoryboardName.empty())
						throw std::invalid_argument(
							"Compiled Storyboard control action has no name");
					const auto found = scope.NamedBeginIndexes.find(
						action.StoryboardName);
					if (found == scope.NamedBeginIndexes.end())
						throw std::invalid_argument(
							"Compiled Storyboard control action has no Begin slot");
					storyboardIndex = found->second;
				}
				Actions.push_back("{ "
					+ std::string(GeneratedActionKindExpression(action.Kind)) + ", "
					+ GeneratedCompiledIndexExpression(storyboardIndex,
						"Compiled interaction storyboard action index") + " }");
			}
			return range;
		}

		std::vector<std::string> StoryboardElements() const
		{
			std::vector<std::string> elements;
			elements.reserve(Storyboards.size());
			for (const auto& range : Storyboards)
				elements.push_back("{ "
					+ GeneratedCompiledRangeExpression(range) + " }");
			return elements;
		}
	};

	static std::wstring_view RoutedEventLocalName(
		std::wstring_view eventName) noexcept
	{
		const auto separator = eventName.find_last_of(L'.');
		return separator == std::wstring_view::npos
			? eventName : eventName.substr(separator + 1);
	}

	static std::optional<RoutedEventId> FindRoutedEventId(
		std::wstring_view eventName) noexcept
	{
		const auto localName = RoutedEventLocalName(eventName);
		for (unsigned int raw = 1;
			raw < static_cast<unsigned int>(RoutedEventId::Count); ++raw)
		{
			const auto eventId = static_cast<RoutedEventId>(raw);
			const auto& metadata = GetRoutedEventMetadata(eventId);
			if (metadata.Name && localName == metadata.Name) return eventId;
		}
		return std::nullopt;
	}

	static std::string RoutedEventIdExpression(RoutedEventId eventId)
	{
		if (eventId == RoutedEventId::None
			|| eventId == RoutedEventId::Count)
			throw std::invalid_argument(
				"Static EventTrigger has no routed-event identity");
		return "static_cast<RoutedEventId>("
			+ std::to_string(static_cast<unsigned int>(eventId)) + ")";
	}

	static const char* BindingModeToExpr(BindingMode mode)
	{
		switch (mode)
		{
		case BindingMode::Default: return "BindingMode::Default";
		case BindingMode::OneWay: return "BindingMode::OneWay";
		case BindingMode::TwoWay: return "BindingMode::TwoWay";
		case BindingMode::OneWayToSource: return "BindingMode::OneWayToSource";
		case BindingMode::OneTime: return "BindingMode::OneTime";
		}
		return "BindingMode::Default";
	}

	static const char* DataSourceUpdateModeToExpr(DataSourceUpdateMode mode)
	{
		switch (mode)
		{
		case DataSourceUpdateMode::Default: return "DataSourceUpdateMode::Default";
		case DataSourceUpdateMode::OnPropertyChanged: return "DataSourceUpdateMode::OnPropertyChanged";
		case DataSourceUpdateMode::OnValidation: return "DataSourceUpdateMode::OnValidation";
		case DataSourceUpdateMode::Never: return "DataSourceUpdateMode::Never";
		}
		return "DataSourceUpdateMode::OnPropertyChanged";
	}

	static bool HasGeneratedDataBindings(
		const DesignerModel::DesignDocument& document,
		const DesignerStyleSheet& styleSheet)
	{
		const bool hasStyleDataTriggers = std::any_of(
			styleSheet.Rules.begin(), styleSheet.Rules.end(),
			[](const DesignerStyleRule& rule)
			{
				return !rule.DataConditions.empty()
					|| std::any_of(
						rule.Triggers.begin(), rule.Triggers.end(),
						[](const DesignerStyleTrigger& trigger)
						{ return !trigger.DataConditions.empty(); });
			});
		return !document.Window.Bindings.empty()
			|| hasStyleDataTriggers
			|| std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[](const auto& node) { return !node.Bindings.empty(); })
			|| std::any_of(
				document.ControlTemplates.begin(),
				document.ControlTemplates.end(),
				[](const auto& definition)
				{
					return std::any_of(
						definition.Template.begin(),
						definition.Template.end(),
						[](const auto& node)
						{ return !node.Bindings.empty(); });
				})
			|| std::any_of(
				document.Components.begin(), document.Components.end(),
				[](const auto& component)
				{
					return std::any_of(
						component.Template.begin(), component.Template.end(),
						[](const auto& node)
						{
							return !node.Bindings.empty()
								|| !node.TemplateBindings.empty();
						});
				});
	}

	static const char* ComponentValueCppType(
		DesignerStyleValueKind kind) noexcept
	{
		switch (kind)
		{
		case DesignerStyleValueKind::Bool: return "bool";
		case DesignerStyleValueKind::NullableBool: return "NullableBool";
		case DesignerStyleValueKind::Int: return "int";
		case DesignerStyleValueKind::Int64: return "long long";
		case DesignerStyleValueKind::Float: return "float";
		case DesignerStyleValueKind::Double: return "double";
		case DesignerStyleValueKind::String: return "std::wstring";
		case DesignerStyleValueKind::Color: return "D2D1_COLOR_F";
		case DesignerStyleValueKind::Thickness: return "Thickness";
		case DesignerStyleValueKind::Point: return "cui::core::Point";
		case DesignerStyleValueKind::Vector: return "cui::core::Vector";
		case DesignerStyleValueKind::Rect: return "cui::core::Rect";
		case DesignerStyleValueKind::Size: return "cui::core::Size";
		case DesignerStyleValueKind::Matrix: return "D2D1_MATRIX_3X2_F";
		case DesignerStyleValueKind::Length: return "cui::layout::Length";
		case DesignerStyleValueKind::Brush: return "cui::drawing::Brush";
		case DesignerStyleValueKind::Geometry: return "cui::drawing::Geometry";
		case DesignerStyleValueKind::Transform:
			return "cui::drawing::Transform";
		default: return nullptr;
		}
	}

	static const char* ComponentEventPayloadKindExpression(
		DesignerComponentEventPayload payload) noexcept
	{
		switch (payload)
		{
		case DesignerComponentEventPayload::None:
			return "BindingValueKind::Empty";
		case DesignerComponentEventPayload::Bool:
			return "BindingValueKind::Bool";
		case DesignerComponentEventPayload::Int:
			return "BindingValueKind::Int";
		case DesignerComponentEventPayload::Int64:
			return "BindingValueKind::Int64";
		case DesignerComponentEventPayload::Float:
			return "BindingValueKind::Float";
		case DesignerComponentEventPayload::Double:
			return "BindingValueKind::Double";
		case DesignerComponentEventPayload::String:
			return "BindingValueKind::String";
		default: return nullptr;
		}
	}

	static const char* ComponentEventPayloadCppType(
		DesignerComponentEventPayload payload) noexcept
	{
		switch (payload)
		{
		case DesignerComponentEventPayload::Bool: return "bool";
		case DesignerComponentEventPayload::Int: return "int";
		case DesignerComponentEventPayload::Int64: return "long long";
		case DesignerComponentEventPayload::Float: return "float";
		case DesignerComponentEventPayload::Double: return "double";
		case DesignerComponentEventPayload::String: return "std::wstring";
		default: return nullptr;
		}
	}

	static const char* ComponentEventRoutingExpression(
		DeclarativeEventRoutingStrategy strategy) noexcept
	{
		switch (strategy)
		{
		case DeclarativeEventRoutingStrategy::Bubble:
			return "DeclarativeEventRoutingStrategy::Bubble";
		case DeclarativeEventRoutingStrategy::Tunnel:
			return "DeclarativeEventRoutingStrategy::Tunnel";
		case DeclarativeEventRoutingStrategy::Direct:
		default:
			return "DeclarativeEventRoutingStrategy::Direct";
		}
	}

	static std::string KeyToExpr(Key key)
	{
		const auto value = static_cast<int>(key);
		if (key >= Key::A && key <= Key::Z)
			return "Key::" + std::string(1, static_cast<char>(
				'A' + value - static_cast<int>(Key::A)));
		if (key >= Key::D0 && key <= Key::D9)
			return "Key::D" + std::to_string(
				value - static_cast<int>(Key::D0));
		if (key >= Key::F1 && key <= Key::F24)
			return "Key::F" + std::to_string(
				value - static_cast<int>(Key::F1) + 1);
		switch (key)
		{
		case Key::Back: return "Key::Back";
		case Key::Tab: return "Key::Tab";
		case Key::Return: return "Key::Return";
		case Key::Escape: return "Key::Escape";
		case Key::Space: return "Key::Space";
		case Key::PageUp: return "Key::PageUp";
		case Key::PageDown: return "Key::PageDown";
		case Key::Home: return "Key::Home";
		case Key::End: return "Key::End";
		case Key::Left: return "Key::Left";
		case Key::Up: return "Key::Up";
		case Key::Right: return "Key::Right";
		case Key::Down: return "Key::Down";
		case Key::Insert: return "Key::Insert";
		case Key::Delete: return "Key::Delete";
		case Key::OemPlus: return "Key::OemPlus";
		case Key::OemMinus: return "Key::OemMinus";
		case Key::OemComma: return "Key::OemComma";
		case Key::OemPeriod: return "Key::OemPeriod";
		default: return {};
		}
	}

	static std::string ModifierKeysToExpr(ModifierKeys modifiers)
	{
		if (modifiers == ModifierKeys::None) return "ModifierKeys::None";
		std::string result;
		auto append = [&](ModifierKeys flag, const char* expression)
		{
			if (!HasModifier(modifiers, flag)) return;
			if (!result.empty()) result += " | ";
			result += expression;
		};
		append(ModifierKeys::Control, "ModifierKeys::Control");
		append(ModifierKeys::Alt, "ModifierKeys::Alt");
		append(ModifierKeys::Shift, "ModifierKeys::Shift");
		append(ModifierKeys::Windows, "ModifierKeys::Windows");
		return result;
	}

	static const char* MouseActionToExpr(MouseAction action)
	{
		switch (action)
		{
		case MouseAction::LeftClick: return "MouseAction::LeftClick";
		case MouseAction::RightClick: return "MouseAction::RightClick";
		case MouseAction::MiddleClick: return "MouseAction::MiddleClick";
		case MouseAction::WheelClick: return "MouseAction::WheelClick";
		case MouseAction::LeftDoubleClick: return "MouseAction::LeftDoubleClick";
		case MouseAction::RightDoubleClick: return "MouseAction::RightDoubleClick";
		case MouseAction::MiddleDoubleClick: return "MouseAction::MiddleDoubleClick";
		default: return "MouseAction::None";
		}
	}

	struct GeneratedEventBinding
	{
		enum class Kind : unsigned char
		{
			BuiltIn,
			Component,
			AttachedComponent
		};

		std::string ControlVar;
		std::string EventField;
		std::string HandlerName;
		std::string ParamList; // "Control* sender" ...
		std::wstring CommandName;
		Kind BindingKind = Kind::BuiltIn;
		std::wstring EventName;
		std::string ComponentClass;
	};

	struct GeneratedRuntimeEventRoute
	{
		std::string HandlerName;
		std::string ParameterList;
		std::wstring EventName;
		std::string EventField;
		std::string EventOwnerType;
		bool IsWindow = false;
		UIClass ControlType = UIClass::UI_Base;
	};

	static std::string LocalSanitizeCppIdentifier(const std::string& raw)
	{
		std::string out;
		out.reserve(raw.size() + 2);
		for (unsigned char ch : raw)
		{
			if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
				out.push_back((char)ch);
			else
				out.push_back('_');
		}
		if (!out.empty() && (out[0] >= '0' && out[0] <= '9'))
			out.insert(out.begin(), '_');
		if (out.empty()) out = "control";
		return out;
	}

	static bool TryGetEventSignature(UIClass controlType, const std::wstring& eventName,
		std::string& outEventField, std::string& outParamList)
	{
		auto descriptor = DesignerEventCatalog::FindControlEvent(controlType, eventName);
		if (!descriptor) return false;
		outEventField = descriptor->EventField;
		outParamList = descriptor->ParameterList;
		return true;
	}

	static bool TryGetWindowEventSignature(const std::wstring& eventName,
		std::string& outEventField, std::string& outParamList)
	{
		auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
		if (!descriptor) return false;
		outEventField = descriptor->EventField;
		outParamList = descriptor->ParameterList;
		return true;
	}


	static std::string Utf8HandlerName(const std::wstring& storedValue)
	{
		const auto resolved = DesignerEventCatalog::NormalizeHandlerName(
			storedValue);
		if (resolved.empty()) return {};
		const int size = WideCharToMultiByte(
			CP_UTF8, 0, resolved.data(), static_cast<int>(resolved.size()),
			nullptr, 0, nullptr, nullptr);
		std::string result(static_cast<size_t>(std::max(0, size)), '\0');
		if (size > 0)
			WideCharToMultiByte(CP_UTF8, 0, resolved.data(),
				static_cast<int>(resolved.size()), result.data(), size, nullptr, nullptr);
		return result;
	}

	static std::string GenerateUnusedParameterLines(
		const std::string& params, const char* indent = "\t")
	{
		std::ostringstream output;
		size_t begin = 0;
		while (begin < params.size())
		{
			auto comma = params.find(',', begin);
			if (comma == std::string::npos) comma = params.size();
			auto end = comma;
			while (end > begin && std::isspace(
				static_cast<unsigned char>(params[end - 1]))) --end;
			auto nameBegin = end;
			while (nameBegin > begin)
			{
				const auto ch = static_cast<unsigned char>(params[nameBegin - 1]);
				if (!std::isalnum(ch) && ch != '_') break;
				--nameBegin;
			}
			if (nameBegin < end)
				output << indent << "(void)"
					<< params.substr(nameBegin, end - nameBegin) << ";\n";
			begin = comma + 1;
		}
		return output.str();
	}

	/** Removes generated parameter identifiers while preserving their C++ types. */
	static std::string CanonicalGeneratedParameterTypes(
		std::string_view parameters)
	{
		std::string result;
		size_t begin = 0;
		int angleDepth = 0;
		for (size_t position = 0; position <= parameters.size(); ++position)
		{
			const char ch = position < parameters.size()
				? parameters[position] : ',';
			if (ch == '<') ++angleDepth;
			else if (ch == '>' && angleDepth > 0) --angleDepth;
			if (ch != ',' || angleDepth != 0) continue;

			auto first = begin;
			while (first < position && std::isspace(
				static_cast<unsigned char>(parameters[first]))) ++first;
			auto end = position;
			while (end > first && std::isspace(
				static_cast<unsigned char>(parameters[end - 1]))) --end;
			auto nameBegin = end;
			while (nameBegin > first)
			{
				const auto value = static_cast<unsigned char>(
					parameters[nameBegin - 1]);
				if (!std::isalnum(value) && value != '_') break;
				--nameBegin;
			}
			auto typeEnd = nameBegin;
			while (typeEnd > first && std::isspace(
				static_cast<unsigned char>(parameters[typeEnd - 1]))) --typeEnd;
			if (!result.empty()) result += ',';
			result.append(parameters.substr(first, typeEnd - first));
			begin = position + 1;
		}
		return result;
	}

	static bool IsCppIdentifierStart(unsigned char value) noexcept
	{
		return std::isalpha(value) || value == '_';
	}

	static bool IsCppIdentifierPart(unsigned char value) noexcept
	{
		return std::isalnum(value) || value == '_';
	}

	struct QualifiedCppClassName
	{
		std::vector<std::string> Segments;
		std::string NamespaceName;
		std::string UserLeaf;
		std::string GeneratedLeaf;
		std::string QualifiedUser;
		std::string QualifiedGenerated;
	};

	static QualifiedCppClassName ParseQualifiedCppClassName(
		const std::string& value)
	{
		QualifiedCppClassName result;
		size_t begin = 0;
		while (begin <= value.size())
		{
			const auto end = value.find("::", begin);
			const auto segment = value.substr(begin,
				end == std::string::npos ? std::string::npos : end - begin);
			if (segment.empty())
				throw std::invalid_argument("C++ code-behind class identity is invalid");
			if (!IsCppIdentifierStart(static_cast<unsigned char>(segment.front()))
				|| !std::all_of(segment.begin() + 1, segment.end(),
					[](unsigned char ch) { return IsCppIdentifierPart(ch); })
				|| IsCppKeyword(segment))
				throw std::invalid_argument("C++ code-behind class segment is invalid");
			result.Segments.push_back(segment);
			if (end == std::string::npos) break;
			begin = end + 2;
		}
		result.UserLeaf = result.Segments.back();
		result.GeneratedLeaf = result.UserLeaf + "Generated";
		for (size_t index = 0; index < result.Segments.size(); ++index)
		{
			if (index > 0) result.QualifiedUser += "::";
			result.QualifiedUser += result.Segments[index];
			if (index + 1 < result.Segments.size())
			{
				if (!result.NamespaceName.empty()) result.NamespaceName += "::";
				result.NamespaceName += result.Segments[index];
			}
		}
		result.QualifiedGenerated = result.NamespaceName.empty()
			? result.GeneratedLeaf
			: result.NamespaceName + "::" + result.GeneratedLeaf;
		return result;
	}

	static std::optional<std::string> ReadUserClassIdentityMarker(
		std::string_view source)
	{
		constexpr std::string_view beginMarker = "<cui-designer-class>";
		constexpr std::string_view endMarker = "</cui-designer-class>";
		const auto begin = source.find(beginMarker);
		if (begin == std::string_view::npos) return std::nullopt;
		const auto valueBegin = begin + beginMarker.size();
		const auto end = source.find(endMarker, valueBegin);
		if (end == std::string_view::npos) return std::string{};
		return std::string(source.substr(valueBegin, end - valueBegin));
	}

}

CodeGenerator::CodeGenerator(
	std::wstring className,
	const DesignerModel::DesignDocument& document,
	CodeGeneratorOutputKind outputKind)
	: _className(std::move(className)),
	_sourceDocument(document),
	_styleSheet(_sourceDocument.StyleSheet),
	_resourceBasePath(_sourceDocument.ResourceBasePath),
	_outputKind(outputKind)
{
	if (_sourceDocument.Window.Type != UIClass::UI_Window
		|| !_sourceDocument.Window.XamlType.Valid())
		throw std::invalid_argument(
			"CodeGenerator requires an authored XAML Window document");
	for (const auto& node : _sourceDocument.Nodes)
		if (!node.XamlType.Valid() && node.ComponentType.Empty())
			throw std::invalid_argument(
				"Every generated control must have an authored XAML type");
	BuildVarNameMap();
}

const std::unordered_map<std::wstring, CodeGenerator::TypedPropertyInfo>&
CodeGenerator::GetKnownProperties()
{
	// These setters belong to Control itself, so they are safe for every
	// generated native control. Type-specific properties intentionally retain
	// the schema-driven fallback until their owning C++ type is known here.
	static const std::unordered_map<std::wstring, TypedPropertyInfo> properties{
		{ L"IsEnabled", { "SetIsEnabled" } },
		{ L"AllowDrop", { "SetAllowDrop" } },
		{ L"Visibility",
			{ "SetVisibility", false, {}, "Visibility", true } },
		{ L"Background",
			{ "SetBackground", false, {}, {}, false,
				"cui::drawing::Brush" } },
		{ L"Foreground",
			{ "SetForeground", false, {}, {}, false,
				"cui::drawing::Brush" } },
		{ L"BorderBrush",
			{ "SetBorderBrush", false, {}, {}, false,
				"cui::drawing::Brush" } },
		{ L"ClipToBounds", { "SetClipToBounds" } },
		{ L"Clip",
			{ "SetClip", false, {}, {}, false,
				"cui::drawing::Geometry" } },
		{ L"RenderTransform",
			{ "SetRenderTransform", false, {}, {}, false,
				"cui::drawing::Transform" } },
		{ L"RenderTransformOrigin", { "SetRenderTransformOriginDip" } },
		{ L"ZIndex", { "SetZIndex" } },
		{ L"Tag", { "SetTag", true } },
		{ L"Focusable", { "SetFocusable" } },
		{ L"IsTabStop", { "SetIsTabStop" } },
		{ L"TabIndex", { "SetTabIndex" } },
		{ L"FocusManager.IsFocusScope", { "SetIsFocusScope" } },
		{ L"KeyboardNavigation.TabNavigation",
			{ "SetTabNavigation", false, {}, "KeyboardNavigationMode" } },
		{ L"KeyboardNavigation.DirectionalNavigation",
			{ "SetDirectionalNavigation", false, {},
				"KeyboardNavigationMode" } },
		{ L"Cursor", { "SetCursor", false, {}, "CursorKind" } },
		{ L"AutomationProperties.Name", { "SetAutomationName" } },
		{ L"AutomationProperties.FullDescription",
			{ "SetAutomationFullDescription" } },
		{ L"AutomationProperties.HelpText", { "SetAutomationHelpText" } },
		{ L"AutomationProperties.AutomationId", { "SetAutomationId" } },
		{ L"FontFamily", { "SetFontFamily" } },
		{ L"Language", { "SetLanguage" } },
		{ L"FontSize", { "SetFontSize" } },
		{ L"Width", { "SetWidth" } },
		{ L"Height", { "SetHeight" } },
		{ L"MinWidth", { "SetMinWidth" } },
		{ L"MinHeight", { "SetMinHeight" } },
		{ L"MaxWidth", { "SetMaxWidth" } },
		{ L"MaxHeight", { "SetMaxHeight" } },
		{ L"BorderThickness", { "SetBorderThickness" } },
		{ L"Margin", { "SetMargin" } },
		{ L"Padding", { "SetPadding" } },
		{ L"HorizontalAlignment",
			{ "SetHorizontalAlignment", false, {}, "::HorizontalAlignment" } },
		{ L"VerticalAlignment",
			{ "SetVerticalAlignment", false, {}, "::VerticalAlignment" } },
		{ L"HorizontalContentAlignment",
			{ "SetHorizontalContentAlignment", false, {},
				"::HorizontalAlignment" } },
		{ L"VerticalContentAlignment",
			{ "SetVerticalContentAlignment", false, {},
				"::VerticalAlignment" } },
		{ L"Canvas.Left", { "SetLeft", false, "Canvas" } },
		{ L"Canvas.Top", { "SetTop", false, "Canvas" } },
		{ L"Canvas.Right", { "SetRight", false, "Canvas" } },
		{ L"Canvas.Bottom", { "SetBottom", false, "Canvas" } },
		{ L"Grid.Row", { "SetRow", false, "Grid" } },
		{ L"Grid.Column", { "SetColumn", false, "Grid" } },
		{ L"Grid.RowSpan", { "SetRowSpan", false, "Grid" } },
		{ L"Grid.ColumnSpan", { "SetColumnSpan", false, "Grid" } },
		{ L"DockPanel.Dock", { "SetDock", false, "DockPanel", "Dock" } },
	};
	return properties;
}

std::optional<CodeGenerator::TypedPropertyInfo>
CodeGenerator::FindKnownProperty(
	UIClass type,
	const std::wstring& propertyName)
{
	if (const auto common = GetKnownProperties().find(propertyName);
		common != GetKnownProperties().end())
		return common->second;

	auto isOneOf = [type](std::initializer_list<UIClass> types)
	{
		return std::find(types.begin(), types.end(), type) != types.end();
	};
	auto setterName = [](const std::wstring& property)
	{
		std::string result = "Set";
		result.reserve(result.size() + property.size());
		for (const auto character : property)
			result.push_back(static_cast<char>(character));
		return result;
	};
	const bool buttonBase = isOneOf({
		UIClass::UI_ButtonBase,
		UIClass::UI_ToggleButton,
		UIClass::UI_Button,
		UIClass::UI_CheckBox,
		UIClass::UI_RadioButton,
		UIClass::UI_Switch,
		UIClass::UI_DataGridColumnHeader,
		UIClass::UI_DataGridRowHeader,
	});
	const bool toggleButton = isOneOf({
		UIClass::UI_ToggleButton,
		UIClass::UI_CheckBox,
		UIClass::UI_RadioButton,
		UIClass::UI_Switch,
	});
	const bool contentPresenter = type == UIClass::UI_ContentPresenter;
	const bool contentControl = buttonBase || isOneOf({
		UIClass::UI_ContentControl,
		UIClass::UI_HeaderedContentControl,
		UIClass::UI_GroupBox,
		UIClass::UI_Expander,
		UIClass::UI_TabItem,
		UIClass::UI_ScrollViewer,
		UIClass::UI_ListBoxItem,
		UIClass::UI_ListViewItem,
		UIClass::UI_ComboBoxItem,
		UIClass::UI_StatusBarItem,
		UIClass::UI_DataGridRow,
		UIClass::UI_DataGridCell,
		UIClass::UI_DataGridColumnHeader,
		UIClass::UI_Window,
	});
	const bool headeredControl = isOneOf({
		UIClass::UI_HeaderedContentControl,
		UIClass::UI_HeaderedItemsControl,
		UIClass::UI_GroupBox,
		UIClass::UI_Expander,
		UIClass::UI_TabItem,
		UIClass::UI_ToolBar,
		UIClass::UI_MenuItem,
		UIClass::UI_TreeViewItem,
	});
	const bool rangeBase = isOneOf({
		UIClass::UI_RangeBase,
		UIClass::UI_Slider,
		UIClass::UI_ProgressBar,
		UIClass::UI_ProgressRing,
		UIClass::UI_NumericUpDown,
	});
	const bool selector = isOneOf({
		UIClass::UI_Selector,
		UIClass::UI_ListBox,
		UIClass::UI_ListView,
		UIClass::UI_ComboBox,
		UIClass::UI_TabControl,
		UIClass::UI_DataGrid,
	});
	const bool itemsControl =
		IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type);
	const bool textBoxBase = isOneOf({
		UIClass::UI_TextBoxBase,
		UIClass::UI_TextBox,
		UIClass::UI_RichTextBox,
	});
	if (propertyName == L"Text" && isOneOf({
		UIClass::UI_Label,
		UIClass::UI_TextBox,
		UIClass::UI_RichTextBox,
		UIClass::UI_ComboBox,
	}))
		return TypedPropertyInfo{ "SetText" };
	if (propertyName == L"Content" && (contentControl || contentPresenter))
		return TypedPropertyInfo{ "SetContent", true };
	if (propertyName == L"ContentTemplate"
		&& (contentControl || contentPresenter))
		return TypedPropertyInfo{ "SetContentTemplate", true };
	if (propertyName == L"Header" && headeredControl)
		return TypedPropertyInfo{ "SetHeader", true };
	if (propertyName == L"HeaderTemplate" && headeredControl)
		return TypedPropertyInfo{ "SetHeaderTemplate", true };
	if (propertyName == L"HeaderDisplayMemberPath" && headeredControl)
		return TypedPropertyInfo{ "SetHeaderDisplayMemberPath" };
	if (propertyName == L"CornerRadius"
		&& (type == UIClass::UI_Border
			|| type == UIClass::UI_WebBrowser))
		return TypedPropertyInfo{ "SetCornerRadius" };
	if (propertyName == L"Command"
		&& (buttonBase || type == UIClass::UI_MenuItem))
		return TypedPropertyInfo{ "SetCommand" };
	if (propertyName == L"ClickMode" && buttonBase)
		return TypedPropertyInfo{
			"SetClickMode", false, {}, "::ClickMode" };
	if (propertyName == L"IsChecked" && toggleButton)
		return TypedPropertyInfo{ "SetIsChecked" };
	if (propertyName == L"IsThreeState" && toggleButton)
		return TypedPropertyInfo{ "SetIsThreeState" };
	if (propertyName == L"CommandParameter")
	{
		if (buttonBase)
			return TypedPropertyInfo{ "SetCommandParameter", true };
		if (type == UIClass::UI_MenuItem)
			return TypedPropertyInfo{ "SetCommandParameter" };
	}
	if (propertyName == L"InputGestureText"
		&& type == UIClass::UI_MenuItem)
		return TypedPropertyInfo{ "SetInputGestureText" };
	if ((propertyName == L"Minimum"
		|| propertyName == L"Maximum"
		|| propertyName == L"Value") && rangeBase)
		return TypedPropertyInfo{
			propertyName == L"Minimum" ? "SetMinimum"
				: propertyName == L"Maximum" ? "SetMaximum" : "SetValue" };
	if (propertyName == L"SelectedIndex" && selector)
		return TypedPropertyInfo{ "SetSelectedIndex" };
	if (propertyName == L"SelectionMode"
		&& isOneOf({ UIClass::UI_ListBox, UIClass::UI_ListView,
			UIClass::UI_DataGrid }))
		return TypedPropertyInfo{
			"SetSelectionMode", false, {}, "::SelectionMode" };
	if (propertyName == L"IsSynchronizedWithCurrentItem" && selector)
		return TypedPropertyInfo{ "SetIsSynchronizedWithCurrentItem" };
	if (propertyName == L"DisplayMemberPath"
		&& (itemsControl || contentControl || contentPresenter))
		return TypedPropertyInfo{ "SetDisplayMemberPath" };
	if (propertyName == L"SelectedValuePath"
		&& (selector || type == UIClass::UI_TreeView))
		return TypedPropertyInfo{ "SetSelectedValuePath" };
	if (propertyName == L"Orientation" && isOneOf({
		UIClass::UI_StackPanel,
		UIClass::UI_WrapPanel,
		UIClass::UI_Slider,
		UIClass::UI_ProgressBar,
	}))
		return TypedPropertyInfo{
			"SetOrientation", false, {}, "Orientation" };
	if (type == UIClass::UI_WrapPanel
		&& (propertyName == L"ItemWidth"
			|| propertyName == L"ItemHeight"))
		return TypedPropertyInfo{
			propertyName == L"ItemWidth"
				? "SetItemWidth" : "SetItemHeight" };
	if (propertyName == L"LastChildFill"
		&& type == UIClass::UI_DockPanel)
		return TypedPropertyInfo{ "SetLastChildFill" };
	if (propertyName == L"IsExpanded" && isOneOf({
		UIClass::UI_Expander,
		UIClass::UI_TreeViewItem,
	}))
		return TypedPropertyInfo{ "SetIsExpanded" };
	if (propertyName == L"IsSelected" && isOneOf({
		UIClass::UI_TabItem,
		UIClass::UI_TreeViewItem,
		UIClass::UI_ListBoxItem,
		UIClass::UI_ListViewItem,
		UIClass::UI_ComboBoxItem,
		UIClass::UI_DataGridRow,
	}))
		return TypedPropertyInfo{ "SetIsSelected" };
	if (type == UIClass::UI_DataGrid)
	{
		if (propertyName == L"AutoGenerateColumns"
			|| propertyName == L"EnableColumnVirtualization"
			|| propertyName == L"IsReadOnly"
			|| propertyName == L"CanUserSortColumns"
			|| propertyName == L"CanUserResizeColumns"
			|| propertyName == L"ColumnHeaderHeight"
			|| propertyName == L"RowHeaderWidth"
			|| propertyName == L"RowHeight"
			|| propertyName == L"RowBackground"
			|| propertyName == L"AlternatingRowBackground"
			|| propertyName == L"HorizontalGridLinesBrush"
			|| propertyName == L"VerticalGridLinesBrush")
			return TypedPropertyInfo{ setterName(propertyName) };
		if (propertyName == L"SelectionUnit")
			return TypedPropertyInfo{
				"SetSelectionUnit", false, {}, "DataGridSelectionUnit" };
		if (propertyName == L"HeadersVisibility")
			return TypedPropertyInfo{
				"SetHeadersVisibility", false, {},
				"DataGridHeadersVisibility" };
		if (propertyName == L"GridLinesVisibility")
			return TypedPropertyInfo{
				"SetGridLinesVisibility", false, {},
				"DataGridGridLinesVisibility" };
	}
	if (propertyName == L"GroupName"
		&& type == UIClass::UI_RadioButton)
		return TypedPropertyInfo{ "SetGroupName" };
	if ((propertyName == L"IsDefault" || propertyName == L"IsCancel")
		&& type == UIClass::UI_Button)
		return TypedPropertyInfo{
			propertyName == L"IsDefault" ? "SetIsDefault" : "SetIsCancel" };
	if (propertyName == L"Password"
		&& type == UIClass::UI_PasswordBox)
		return TypedPropertyInfo{ "SetPassword" };
	if (type == UIClass::UI_Popup)
	{
		if (propertyName == L"StaysOpen"
			|| propertyName == L"HorizontalOffset"
			|| propertyName == L"VerticalOffset")
			return TypedPropertyInfo{ setterName(propertyName) };
		if (propertyName == L"Placement")
			return TypedPropertyInfo{
				"SetPlacement", false, {}, "PlacementMode" };
	}
	if (textBoxBase)
	{
		if (propertyName == L"IsReadOnly"
			|| propertyName == L"IsReadOnlyCaretVisible"
			|| propertyName == L"AcceptsReturn"
			|| propertyName == L"AcceptsTab"
			|| propertyName == L"AutoWordSelection"
			|| propertyName == L"IsUndoEnabled"
			|| propertyName == L"UndoLimit"
			|| propertyName
				== L"IsInactiveSelectionHighlightEnabled"
			|| propertyName == L"SelectionBrush"
			|| propertyName == L"SelectionOpacity"
			|| propertyName == L"SelectionTextBrush"
			|| propertyName == L"CaretBrush")
			return TypedPropertyInfo{
				setterName(propertyName) };
	}
	if ((textBoxBase || type == UIClass::UI_ScrollViewer)
		&& (propertyName == L"HorizontalScrollBarVisibility"
			|| propertyName == L"VerticalScrollBarVisibility"))
		return TypedPropertyInfo{
			setterName(propertyName),
			false, {}, "ScrollBarVisibility" };
	if (propertyName == L"MaxDropDownHeight"
		&& type == UIClass::UI_ComboBox)
		return TypedPropertyInfo{ "SetMaxDropDownHeight" };
	if (propertyName == L"IsIndeterminate"
		&& type == UIClass::UI_ProgressBar)
		return TypedPropertyInfo{ "SetIsIndeterminate" };
	if (propertyName == L"TickFrequency"
		&& type == UIClass::UI_Slider)
		return TypedPropertyInfo{ "SetTickFrequency" };
	if (propertyName == L"TabStripPlacement"
		&& type == UIClass::UI_TabControl)
		return TypedPropertyInfo{
			"SetTabStripPlacement", false, {}, "Dock" };
	if (propertyName == L"TextWrapping" && isOneOf({
		UIClass::UI_Label,
		UIClass::UI_TextBox,
	}))
		return TypedPropertyInfo{
			"SetTextWrapping", false, {}, "TextWrapping" };
	if (propertyName == L"TextTrimming"
		&& type == UIClass::UI_Label)
		return TypedPropertyInfo{
			"SetTextTrimming", false, {}, "TextTrimming" };
	if (propertyName == L"Source" && type == UIClass::UI_Image)
		return TypedPropertyInfo{
			"SetSource", false, {}, {}, false,
			"std::shared_ptr<BitmapSource>" };
	if (propertyName == L"Stretch" && type == UIClass::UI_Image)
		return TypedPropertyInfo{
			"SetStretch", false, {}, "::Stretch" };
	if (propertyName == L"StretchDirection" && type == UIClass::UI_Image)
		return TypedPropertyInfo{
			"SetStretchDirection", false, {}, "::StretchDirection" };
	if (propertyName == L"Subtitle" && type == UIClass::UI_ChartView)
		return TypedPropertyInfo{ "SetSubtitle" };
	if (propertyName == L"Title" && type == UIClass::UI_ChartView)
		return TypedPropertyInfo{ "SetTitle" };
	if (propertyName == L"PlaceholderText"
		&& type == UIClass::UI_NativeSurface)
		return TypedPropertyInfo{ "SetPlaceholderText" };
	if (propertyName == L"BehaviorKey"
		&& type == UIClass::UI_NativeSurface)
		return TypedPropertyInfo{ "SetBehaviorKey" };
	if (propertyName == L"IsActive"
		&& type == UIClass::UI_LoadingRing)
		return TypedPropertyInfo{ "SetIsActive" };
	if (propertyName == L"DefaultBackgroundColor"
		&& type == UIClass::UI_WebBrowser)
		return TypedPropertyInfo{
			"SetDefaultBackgroundColor", false, {}, {}, false,
			"D2D1_COLOR_F" };
	if (type == UIClass::UI_MediaElement)
	{
		if (propertyName == L"Source")
			return TypedPropertyInfo{ "SetSource" };
		if (propertyName == L"Volume"
			|| propertyName == L"SpeedRatio"
			|| propertyName == L"Loop")
			return TypedPropertyInfo{
				propertyName == L"Volume" ? "SetVolume"
					: propertyName == L"SpeedRatio" ? "SetSpeedRatio" : "SetLoop" };
		if (propertyName == L"LoadedBehavior"
			|| propertyName == L"UnloadedBehavior")
			return TypedPropertyInfo{
				propertyName == L"LoadedBehavior"
					? "SetLoadedBehavior" : "SetUnloadedBehavior",
				false, {}, "MediaState" };
		if (propertyName == L"Stretch")
			return TypedPropertyInfo{
				"SetStretch", false, {},
				"::Stretch" };
		if (propertyName == L"StretchDirection")
			return TypedPropertyInfo{
				"SetStretchDirection", false, {}, "::StretchDirection" };
	}
	if (propertyName == L"Title" && type == UIClass::UI_Window)
		return TypedPropertyInfo{ "SetTitle" };
	return std::nullopt;
}

std::string CodeGenerator::UnwrapBindingValue(
	const std::string& expression)
{
	constexpr std::string_view prefix = "BindingValue(";
	if (!expression.starts_with(prefix) || expression.size() <= prefix.size()
		|| expression.back() != ')')
		return {};
	return expression.substr(
		prefix.size(), expression.size() - prefix.size() - 1);
}

std::string CodeGenerator::GenerateTypedPropertyCall(
	const std::string& target,
	const TypedPropertyInfo& property,
	const std::string& valueExpression,
	bool valueIsBindingValueVariable)
{
	std::string argument;
	if (property.PassBindingValue)
		argument = valueExpression;
	else if (valueIsBindingValueVariable
		&& !property.SharedValueType.empty())
		argument = "CuiGeneratedBindingValueAs<"
			+ property.SharedValueType + ">(" + valueExpression + ")";
	else if (!valueIsBindingValueVariable)
		argument = UnwrapBindingValue(valueExpression);
	if (argument.empty()) return {};
	if (property.NamedEnumValue)
	{
		if (property.ValueType != "Visibility"
			|| !argument.starts_with("L\"")
			|| argument.size() < 4
			|| !argument.ends_with("\""))
			return {};
		const auto name = argument.substr(2, argument.size() - 3);
		auto equalName = [&name](std::string_view expected)
		{
			return name.size() == expected.size()
				&& std::equal(
					name.begin(), name.end(), expected.begin(),
					[](unsigned char left, unsigned char right)
					{
						return std::tolower(left) == std::tolower(right);
					});
		};
		if (equalName("Visible"))
			argument = "Visibility::Visible";
		else if (equalName("Hidden"))
			argument = "Visibility::Hidden";
		else if (equalName("Collapsed"))
			argument = "Visibility::Collapsed";
		else
			return {};
	}
	else if (!property.ValueType.empty())
		argument = "static_cast<" + property.ValueType + ">("
			+ argument + ")";
	if (!property.StaticOwner.empty())
		return property.StaticOwner + "::" + property.SetterName
			+ "(*" + target + ", " + argument + ")";
	return target + "->" + property.SetterName + "(" + argument + ")";
}

bool CodeGenerator::ValidateDocument(
	const DesignerModel::DesignDocument& document,
	std::wstring* outError,
	DesignerModel::XamlDocumentDiagnostic* outDiagnostic)
{
	using namespace DesignerModel;
	if (outDiagnostic)
	{
		*outDiagnostic = {};
		outDiagnostic->Stage = XamlDiagnosticStage::CodeGeneration;
	}
	auto fail = [&](std::wstring message,
		const DesignNode* node = nullptr,
		const std::wstring& member = std::wstring{})
	{
		if (outError) *outError = message;
		if (outDiagnostic)
		{
			outDiagnostic->Message = message;
			outDiagnostic->Member = member;
			if (node)
			{
				outDiagnostic->QName = node->XamlType.Valid()
					? node->XamlType.LocalName
					: DesignerStyleSheetUtils::UIClassName(node->Type);
				const auto* span = member.empty()
					? nullptr : node->Source.FindMember(member);
				outDiagnostic->Apply(span ? *span : node->Source.Element);
			}
			else
			{
				std::wstring symbol;
				if (const auto* span = document.Sources.FindMentionedSymbol(
					message, &symbol))
				{
					if (outDiagnostic->Member.empty())
						outDiagnostic->Member = std::move(symbol);
					outDiagnostic->Apply(*span);
				}
				else outDiagnostic->Apply(document.Sources.Root);
			}
		}
		return false;
	};
	std::wstring richTextError;
	if (!document.ValidateRichTextStructure(&richTextError))
		return fail(std::move(richTextError));

	bool hasLocalItemsPanelTemplates = false;
	for (const auto& node : document.Nodes)
	{
		const auto& local = node.LocalObjectResources;
		if (!local.Components.empty())
			return fail(
				L"静态 C++ 生成不支持 regular node 局部 "
				L"ComponentDefinition；请提升到文档资源。",
				&node, L"Resources");
		if (!local.ControlTemplates.empty())
			return fail(
				L"静态 C++ 生成不支持 regular node 局部 "
				L"ControlTemplate；拒绝回退到动态 XAML 物化。",
				&node, L"Resources");
		if (!local.DataTemplates.empty())
			return fail(
				L"静态 C++ 生成不支持 regular node 局部 "
				L"DataTemplate；拒绝回退到动态 XAML 物化。",
				&node, L"Resources");
		if (!local.GroupStyles.empty())
			return fail(
				L"静态 C++ 生成尚未 lowering regular node 局部 "
				L"GroupStyle；请提升到文档资源。",
				&node, L"Resources");
		hasLocalItemsPanelTemplates = hasLocalItemsPanelTemplates
			|| !local.ItemsPanelTemplates.empty();
	}

	if (!document.Components.empty())
	{
		// Reuse the canonical build-time expander as a structural preflight,
		// but never carry its expanded nodes or framework Theme into production
		// output. ComponentDefinition itself is lowered below into C++ classes.
		CuiRuntime::XamlCompiledDocument compiledComponents;
		CuiRuntime::XamlDocumentCompilationOptions options;
		options.UseFrameworkTheme = false;
		std::wstring componentError;
		if (!CuiRuntime::XamlDocumentCompiler::Compile(
			document, compiledComponents, options, &componentError))
			return fail(componentError.empty()
				? L"ComponentDefinition 无法完成静态结构预检。"
				: std::move(componentError));
	}

	if (!document.DataTypes.empty() || !document.DataLists.empty()
		|| !document.CollectionViews.empty()
		|| !document.ItemsPanelTemplates.empty()
		|| !document.DataTemplates.empty()
		|| !document.GroupStyles.empty()
		|| hasLocalItemsPanelTemplates)
	{
		for (const auto& view : document.CollectionViews)
			if (!view.SourceBindingPath.empty())
				return fail(
					L"静态 CollectionViewSource 禁止保留动态 Source Binding："
					+ view.Key + L" -> " + view.SourceBindingPath);
		auto dataDocument = document;
		std::wstring dataError;
		if (!DesignDataResourceUtils::ValidateAndCanonicalize(
			dataDocument, &dataError))
			return fail(L"数据或 ItemsPanelTemplate 资源无法静态生成："
				+ dataError);
		for (const auto& dataList : dataDocument.DataLists)
		{
			if (!DesignDataResourceUtils::BuildRuntimeList(
				dataDocument, dataList, &dataError))
				return fail(L"DataList " + dataList.Key
					+ L" 无法转换为原生 BindingList："
					+ dataError);
		}
		for (const auto& view : dataDocument.CollectionViews)
		{
			if (!view.SourceBindingPath.empty())
				return fail(
					L"静态 CollectionViewSource 禁止保留动态 Source Binding："
					+ view.Key + L" -> " + view.SourceBindingPath);
			if (!dataDocument.FindDataList(view.SourceResource))
				return fail(
					L"静态 CollectionViewSource 当前必须直接引用已生成的 "
					L"DataList：" + view.Key + L" -> "
					+ view.SourceResource);
		}
		for (size_t nodeIndex = 0;
			nodeIndex < document.Nodes.size(); ++nodeIndex)
		{
			const auto& node = document.Nodes[nodeIndex];
			const auto& canonicalNode = dataDocument.Nodes[nodeIndex];
			if (!node.Structure.ItemsPanel.empty())
			{
				if (!dataDocument.FindItemsPanelTemplate(
					canonicalNode, node.Structure.ItemsPanel))
					return fail(
						L"静态 ItemsPanel 引用了未声明的 "
						L"ItemsPanelTemplate："
						+ node.Structure.ItemsPanel,
						&node, L"ItemsPanel");
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return fail(
						L"静态 ItemsPanel 的目标必须是 ItemsControl。",
						&node, L"ItemsPanel");
			}
			if (!node.Structure.ItemsSourceResource.empty())
			{
				if (!dataDocument.FindDataList(
						node.Structure.ItemsSourceResource)
					&& !dataDocument.FindCollectionView(
						node.Structure.ItemsSourceResource))
					return fail(
						L"静态 ItemsSource 当前只支持直接引用 DataList 或 "
						L"CollectionViewSource："
						+ node.Structure.ItemsSourceResource,
						&node, L"ItemsSource");
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return fail(
						L"静态 DataList ItemsSource 的目标必须是 ItemsControl。",
						&node, L"ItemsSource");
			}
			if (!node.Structure.ItemTemplate.empty())
			{
				const auto* itemTemplate =
					dataDocument.FindDataTemplate(
						node.Structure.ItemTemplate);
				if (!itemTemplate || itemTemplate->IsImplicit())
					return fail(
						L"静态 ItemTemplate 引用了未声明的显式 "
						L"DataTemplate："
						+ node.Structure.ItemTemplate,
						&node, L"ItemTemplate");
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return fail(
						L"静态 ItemTemplate 的目标必须是 ItemsControl。",
						&node, L"ItemTemplate");
			}
			if (!node.Structure.ContentTemplate.empty())
			{
				const auto* contentTemplate =
					dataDocument.FindDataTemplate(
						node.Structure.ContentTemplate);
				if (!contentTemplate || contentTemplate->IsImplicit())
					return fail(
						L"静态 ContentTemplate 引用了未声明的显式 "
						L"DataTemplate："
						+ node.Structure.ContentTemplate,
						&node, L"ContentTemplate");
				if (node.Type != UIClass::UI_ContentPresenter
					&& !IsUIClassAssignableFrom(
						UIClass::UI_ContentControl, node.Type))
					return fail(
						L"静态 ContentTemplate 的目标必须是内容控件。",
						&node, L"ContentTemplate");
			}
			if (!node.Structure.HeaderTemplate.empty())
			{
				const auto* headerTemplate =
					dataDocument.FindDataTemplate(
						node.Structure.HeaderTemplate);
				if (!headerTemplate || headerTemplate->IsImplicit())
					return fail(
						L"静态 HeaderTemplate 引用了未声明的显式 "
						L"DataTemplate："
						+ node.Structure.HeaderTemplate,
						&node, L"HeaderTemplate");
				if (!IsUIClassAssignableFrom(
						UIClass::UI_HeaderedContentControl, node.Type)
					&& !IsUIClassAssignableFrom(
						UIClass::UI_HeaderedItemsControl, node.Type))
					return fail(
						L"静态 HeaderTemplate 的目标必须是 Headered 控件。",
						&node, L"HeaderTemplate");
			}
			if (!node.Structure.GroupStyle.empty())
			{
				if (!dataDocument.FindGroupStyle(
					node.Structure.GroupStyle))
					return fail(
						L"静态 GroupStyle 引用了未声明的资源："
						+ node.Structure.GroupStyle,
						&node, L"GroupStyle");
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return fail(
						L"静态 GroupStyle 的目标必须是 ItemsControl。",
						&node, L"GroupStyle");
			}
		}
		for (const auto& definition : document.ControlTemplates)
			for (const auto& node : definition.Template)
			{
				if (!node.Structure.ItemsPanel.empty())
					return fail(
						L"ControlTemplate 内的 ItemsPanelTemplate "
						L"尚未生成闭包捕获；拒绝静默丢失该资源。",
						&node, L"ItemsPanel");
				if (!node.Structure.ItemsSourceResource.empty())
					return fail(
						L"ControlTemplate 内的数据资源 ItemsSource "
						L"尚未生成闭包捕获；拒绝静默丢失该资源。",
						&node, L"ItemsSource");
			}
	}

	DesignDocumentGraph graph;
	std::wstring validationError;
	if (!DesignDocumentGraph::Build(document, graph, &validationError))
		return fail(std::move(validationError));
	if (!document.ValidateCommandTargetReferences(&validationError))
		return fail(std::move(validationError));
	if (document.Window.Type != UIClass::UI_Window
		|| !document.Window.XamlType.Valid())
		return fail(
			L"静态代码生成要求一个具有 XAML 类型标识的 Window 根节点"
			L"（nativeType=" + std::to_wstring(
				static_cast<int>(document.Window.Type))
			+ L"，xmlns=" + document.Window.XamlType.NamespaceUri
			+ L"，name=" + document.Window.XamlType.LocalName + L"）。",
			&document.Window);

	auto validateNode = [&](const DesignNode& node)
	{
		if (!node.XamlType.Valid() && node.ComponentType.Empty())
			return fail(L"静态代码生成节点缺少 XAML 类型标识: "
				+ node.Name, &node);
			CuiRuntime::XamlTypePropertySchema schema;
			std::wstring schemaError;
			const auto* component = node.ComponentType.Empty()
				? nullptr : document.FindComponent(node.ComponentType);
			if (!node.ComponentType.Empty() && !component)
				return fail(L"静态代码生成节点引用了未声明的组件类型: "
					+ node.ComponentType.XamlName, &node);
			if (!CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
				node.Type, component, document, schema, &schemaError))
				return fail(L"无法解析静态代码生成节点的属性 Schema: "
					+ schemaError, &node);
		for (const auto& [propertyName, assignment] : node.Properties.Values)
		{
			const auto* metadata = schema.FindProperty(propertyName);
			if (!metadata || !metadata->CanWrite())
				return fail(L"静态代码生成节点没有可写属性: "
					+ propertyName, &node, propertyName);
			if (!assignment.DynamicResourceKey.empty()) continue;
			DesignerStyleValue canonical;
			std::wstring propertyError;
			if (!DesignerPropertyCatalog::NormalizeStyleValue(
				*metadata, assignment.Value, canonical, &propertyError,
				document.ResourceBasePath))
				return fail(propertyError.empty()
					? L"静态代码生成属性值无效: " + propertyName
					: std::move(propertyError), &node, propertyName);
			const auto& design = metadata->Design();
			if (design.Minimum || design.Maximum)
			{
				BindingValue parsed;
				BindingValue converted;
				double number = 0.0;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					canonical, parsed, &propertyError,
					document.ResourceBasePath)
					|| !metadata->TryConvert(parsed, converted)
					|| !converted.TryGetDouble(number)
					|| !std::isfinite(number)
					|| (design.Minimum && number < *design.Minimum)
					|| (design.Maximum && number > *design.Maximum))
					return fail(L"属性值超出 Schema 允许范围: "
						+ propertyName, &node, propertyName);
			}
		}
		return true;
	};
	if (!validateNode(document.Window)) return false;
	for (const auto& node : document.Nodes)
		if (!validateNode(node)) return false;
	std::map<std::uint64_t, std::wstring> componentNamesByToken;
	for (const auto& component : document.Components)
	{
		const auto componentToken = MakeComponentTypeToken(
			component.Type.XamlNamespace, component.Type.XamlName);
		if (!componentToken)
			return fail(L"静态 ComponentDefinition 缺少有效的展开 QName："
				+ component.Type.XamlName);
		const auto componentQName = component.Type.RegistryKey();
		const auto [componentIdentity, componentInserted] =
			componentNamesByToken.emplace(
				componentToken.Value, componentQName);
		if (!componentInserted && componentIdentity->second != componentQName)
			return fail(L"静态 ComponentDefinition 发生 ComponentTypeToken 冲突："
				+ componentIdentity->second + L" / " + componentQName);
		std::map<std::uint64_t, std::wstring> propertyNamesByToken;
		for (const auto& property : component.Properties)
		{
			const auto token = MakeComponentPropertyToken(property.Name);
			if (!token)
				return fail(L"静态 ComponentDefinition 包含空属性 token："
					+ component.Type.XamlName);
			const auto [existing, inserted] =
				propertyNamesByToken.emplace(token.Value, property.Name);
			if (inserted) continue;
			return fail(existing->second == property.Name
				? L"静态 ComponentDefinition 包含重复属性："
					+ component.Type.XamlName + L"." + property.Name
				: L"静态 ComponentDefinition 属性发生 "
					L"ComponentPropertyToken 冲突："
					+ component.Type.XamlName + L"." + existing->second
					+ L" / " + property.Name);
		}
		std::unordered_set<std::wstring> componentTemplateNames;
		for (const auto& node : component.Template)
			componentTemplateNames.insert(node.Name);
		for (const auto& node : component.Template)
		{
			if (!node.LocalResources.Empty()
				|| !node.LocalObjectResources.Empty())
				return fail(
					L"静态 ComponentDefinition 模板内部暂不接受局部 "
					L"Resources；请把资源提升到文档 ResourceDictionary。",
					&node, L"Resources");
			if (!node.Events.empty() || !node.CommandBindings.empty())
				return fail(
					L"静态 ComponentDefinition 模板中的代码后置事件或 "
					L"CommandBinding 无法成为独立强类型组件；请使用 "
					L"RaiseEvent 和 InputBinding。",
					&node);
			if (!node.Structure.ItemsPanel.empty()
				|| !node.Structure.ItemsSourceResource.empty()
				|| !node.Structure.ItemTemplate.empty()
				|| !node.Structure.ContentTemplate.empty()
				|| !node.Structure.HeaderTemplate.empty()
				|| !node.Structure.ControlTemplate.empty()
				|| !node.Structure.GroupStyle.empty())
				return fail(
					L"静态 ComponentDefinition 模板内部暂不支持嵌套"
					L"结构资源引用；拒绝回退到动态 XAML 物化。",
					&node);
			for (const auto& [targetProperty, binding] : node.Bindings)
			{
				if (binding.IsMultiBinding()
					&& std::any_of(
						binding.ChildBindings.begin(),
						binding.ChildBindings.end(),
						[](const auto& child)
						{ return child.IsMultiBinding(); }))
					return fail(
						L"静态 ComponentDefinition 不支持嵌套 MultiBinding。",
						&node, targetProperty);
				std::wstring bindingError;
				if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
					binding, [&](const DesignerDataBinding& child)
					{
						if (!child.ElementName.empty()
							&& !componentTemplateNames.contains(
								child.ElementName))
						{
							bindingError =
								L"静态 ComponentDefinition Binding 引用了"
								L"当前模板 namescope 中不存在的 "
								L"ElementName：" + child.ElementName;
							return false;
						}
						if (child.RelativeSource
								== DesignerBindingRelativeSource::FindAncestor
							&& !child.AncestorTypeNamespace.empty()
							&& !document.FindComponent(
								child.AncestorTypeNamespace,
								GeneratedAncestorLocalTypeName(
									child.AncestorType)))
						{
							bindingError = L"静态 ComponentDefinition "
								L"FindAncestor 引用了未声明的组件类型。";
							return false;
						}
						return true;
					}))
					return fail(std::move(bindingError), &node, targetProperty);
			}
			if (!validateNode(node)) return false;
		}
	}
	for (const auto& definition : document.DataTemplates)
	{
		for (const auto& node : definition.Template)
		{
			if (!node.LocalResources.Empty()
				|| !node.LocalObjectResources.Empty())
				return fail(
					L"静态 DataTemplate 暂不接受模板内部 Resources；"
					L"请把资源提升到可见的文档 ResourceDictionary。",
					&node, L"Resources");
			if (!node.Events.empty()
				|| !node.CommandBindings.empty()
				|| !node.InputBindings.empty())
				return fail(
					L"静态 DataTemplate 不允许代码后置事件、"
					L"CommandBinding 或 InputBinding。",
					&node);
			if (!node.Structure.CommandTarget.empty()
				|| !node.Structure.ItemsSourceResource.empty()
				|| !node.Structure.ItemTemplate.empty()
				|| !node.Structure.ContentTemplate.empty()
				|| !node.Structure.HeaderTemplate.empty()
				|| !node.Structure.ControlTemplate.empty()
				|| !node.Structure.GroupStyle.empty()
				|| !node.Structure.ItemsPanel.empty())
				return fail(
					L"静态 DataTemplate 内部暂不支持嵌套对象资源引用；"
					L"拒绝回退到动态 XAML 物化。",
					&node);
			for (const auto& [targetProperty, binding] : node.Bindings)
			{
				if (binding.IsMultiBinding()
					&& std::any_of(
						binding.ChildBindings.begin(),
						binding.ChildBindings.end(),
						[](const auto& child)
						{ return child.IsMultiBinding(); }))
					return fail(
						L"静态 DataTemplate 不支持嵌套 MultiBinding。",
						&node, targetProperty);
				std::wstring bindingError;
				if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
					binding, [&](const DesignerDataBinding& child)
					{
						if (child.RelativeSource
							== DesignerBindingRelativeSource::TemplatedParent)
						{
							bindingError = L"DataTemplate 没有 ControlTemplate 的 "
								L"TemplatedParent；该 Binding 无法静态生成。";
							return false;
						}
						if (child.RelativeSource
								== DesignerBindingRelativeSource::FindAncestor
							&& !child.AncestorTypeNamespace.empty()
							&& !document.FindComponent(
								child.AncestorTypeNamespace,
								GeneratedAncestorLocalTypeName(
									child.AncestorType)))
						{
							bindingError = L"静态 DataTemplate FindAncestor "
								L"引用了未声明的组件类型。";
							return false;
						}
						return true;
					}))
					return fail(std::move(bindingError), &node, targetProperty);
			}
			if (!node.TemplateBindings.empty()
				|| !node.TemplateEventBindings.empty())
				return fail(
					L"DataTemplate 节点不能保留 ControlTemplate Binding。",
					&node);
			if (!validateNode(node)) return false;
		}
	}
	if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
		document.StyleSheet,
		[&](const DesignerStyleRule& rule,
			CuiRuntime::XamlTypePropertySchema& schema,
			std::wstring* schemaError) -> bool
		{
			const auto* component = rule.ComponentType.Empty()
				? nullptr : document.FindComponent(rule.ComponentType);
			if (!rule.ComponentType.Empty() && !component)
			{
				if (schemaError) *schemaError =
					L"样式 TargetType 组件不存在。";
				return false;
			}
			return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
				rule.HasType ? rule.Type : UIClass::UI_Base,
				component, document, schema, schemaError);
		},
		&validationError, document.ResourceBasePath, document.Resources))
		return fail(L"Window.Resources 无法静态生成: " + validationError,
			&document.Window, L"Resources");
	if (outError) outError->clear();
	return true;
}

const std::vector<DesignerComponentEventDescriptor>&
CodeGenerator::ComponentEvents(
	const DesignerModel::DesignNode& node) const noexcept
{
	static const std::vector<DesignerComponentEventDescriptor> empty;
	if (node.ComponentType.Empty()) return empty;
	const auto found = std::find_if(
		_sourceDocument.Components.begin(), _sourceDocument.Components.end(),
		[&](const auto& component)
		{ return component.Type == node.ComponentType; });
	return found == _sourceDocument.Components.end() ? empty : found->Events;
}

std::optional<DesignerEventDescriptor>
CodeGenerator::FindNodeEventDescriptor(
	const DesignerModel::DesignNode& node,
	const std::wstring& eventName) const
{
	DesignerComponentType ownerType;
	std::wstring attachedEventName;
	if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
		eventName, ownerType, attachedEventName))
	{
		const auto* owner = _sourceDocument.FindComponent(ownerType);
		if (!owner) return std::nullopt;
		const auto contract = std::find_if(
			owner->Events.begin(), owner->Events.end(),
			[&](const auto& candidate)
			{ return candidate.Name == attachedEventName; });
		return contract == owner->Events.end()
			? std::nullopt
			: DesignerEventCatalog::FromComponentEvent(*contract);
	}
	return DesignerEventCatalog::FindControlEvent(
		node.Type, eventName, ComponentEvents(node));
}

std::string CodeGenerator::CommandTargetExpression(
	const std::wstring& name) const
{
	if (name.empty()) return "nullptr";
	if (name == _sourceDocument.Window.Name) return "this";
	const auto found = std::find_if(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[&](const auto& node) { return node.Name == name; });
	if (found == _sourceDocument.Nodes.end())
		throw std::invalid_argument(
			"Code generation encountered an unresolved CommandTarget");
	return GetVarName(*found);
}

bool CodeGenerator::InspectUserHandlerDefinitions(
	std::string_view userSource,
	std::vector<CodeGeneratorHandlerDefinitionInspection>& inspections)

{
	return InspectUserHandlerDefinitions({}, userSource, inspections);
}

bool CodeGenerator::InspectUserHandlerDefinitions(
	std::string_view userHeader,
	std::string_view userSource,
	std::vector<CodeGeneratorHandlerDefinitionInspection>& inspections)
{
	inspections.clear();
	_lastError.clear();
	try
	{
		std::vector<std::pair<std::string, std::string>> handlers;
		std::wstring error;
		if (!CollectEventHandlers(handlers, &error))
		{
			_lastError = error.empty()
				? L"无法建立事件处理函数索引。" : std::move(error);
			return false;
		}
		const auto identity = ParseQualifiedCppClassName(
			WStringToString(_className));
		DesignerModel::CppUserCodeIndex headerIndex;
		DesignerModel::CppUserCodeIndex sourceIndex;
		if (!DesignerModel::CppUserCodeIndex::Build(
			userHeader, identity.QualifiedUser, headerIndex, &error))
		{
			_lastError = error.empty()
				? L"无法建立用户头文件事件代码索引。" : std::move(error);
			return false;
		}
		if (!DesignerModel::CppUserCodeIndex::Build(
			userSource, identity.QualifiedUser, sourceIndex, &error))
		{
			_lastError = error.empty()
				? L"无法建立用户源文件事件代码索引。" : std::move(error);
			return false;
		}
		inspections.reserve(handlers.size());
		for (const auto& [name, parameterList] : handlers)
		{
			CodeGeneratorHandlerDefinitionInspection inspection;
			inspection.Name = name;
			inspection.ParameterList = parameterList;
			const auto headerDefinitions = headerIndex.InspectHandler(
				name, parameterList);
			const auto sourceDefinitions = sourceIndex.InspectHandler(
				name, parameterList);
			inspection.HeaderDefinitionCount =
				headerDefinitions.DefinitionCount;
			inspection.HeaderCompatibleDefinitionCount =
				headerDefinitions.CompatibleDefinitionCount;
			inspection.HeaderIncompatibleShapeDefinitionCount =
				headerDefinitions.IncompatibleShapeDefinitionCount;
			inspection.HeaderDeletedCompatibleDefinitionCount =
				headerDefinitions.DeletedCompatibleDefinitionCount;
			inspection.SourceDefinitionCount =
				sourceDefinitions.DefinitionCount;
			inspection.SourceCompatibleDefinitionCount =
				sourceDefinitions.CompatibleDefinitionCount;
			inspection.SourceIncompatibleShapeDefinitionCount =
				sourceDefinitions.IncompatibleShapeDefinitionCount;
			inspection.SourceDeletedCompatibleDefinitionCount =
				sourceDefinitions.DeletedCompatibleDefinitionCount;
			inspection.FirstHeaderDefinitionLine =
				headerDefinitions.FirstDefinitionLine;
			inspection.FirstHeaderCompatibleDefinitionLine =
				headerDefinitions.FirstCompatibleDefinitionLine;
			inspection.FirstSourceDefinitionLine =
				sourceDefinitions.FirstDefinitionLine;
			inspection.FirstSourceCompatibleDefinitionLine =
				sourceDefinitions.FirstCompatibleDefinitionLine;
			inspection.DefinitionCount =
				inspection.HeaderDefinitionCount
				+ inspection.SourceDefinitionCount;
			inspection.CompatibleDefinitionCount =
				inspection.HeaderCompatibleDefinitionCount
				+ inspection.SourceCompatibleDefinitionCount;
			inspection.IncompatibleShapeDefinitionCount =
				inspection.HeaderIncompatibleShapeDefinitionCount
				+ inspection.SourceIncompatibleShapeDefinitionCount;
			inspection.DeletedCompatibleDefinitionCount =
				inspection.HeaderDeletedCompatibleDefinitionCount
				+ inspection.SourceDeletedCompatibleDefinitionCount;
			if (inspection.DefinitionCount == 0)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Missing;
			else if (inspection.CompatibleDefinitionCount > 1)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::DuplicateCompatible;
			else if (inspection.IncompatibleShapeDefinitionCount != 0
				|| inspection.DeletedCompatibleDefinitionCount != 0)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Incompatible;
			else if (inspection.CompatibleDefinitionCount == 1)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Compatible;
			else
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Incompatible;
			inspections.push_back(std::move(inspection));
		}
		return true;
	}
	catch (const std::exception& error)
	{
		_lastError = L"无法检查用户事件处理函数："
			+ StringToWString(error.what());
	}
	catch (...)
	{
		_lastError = L"无法检查用户事件处理函数：发生未知异常。";
	}
	inspections.clear();
	return false;
}

static bool IsCppKeyword(const std::string& s)
{
	static const std::unordered_set<std::string> k = {
		"alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
		"auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
		"class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue",
		"co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast",
		"else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline",
		"int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or",
		"or_eq","private","protected","public","register","reinterpret_cast","requires","return","short",
		"signed","sizeof","static","static_assert","static_cast","struct","switch","synchronized","template",
		"this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using",
		"virtual","void","volatile","wchar_t","while","xor","xor_eq"
	};
	return k.find(s) != k.end();
}

std::string CodeGenerator::SanitizeCppIdentifier(const std::string& raw)
{
	std::string out;
	out.reserve(raw.size() + 2);

	for (unsigned char ch : raw)
	{
		if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
			out.push_back((char)ch);
		else
			out.push_back('_');
	}

	// 不能以数字开头
	if (!out.empty() && (out[0] >= '0' && out[0] <= '9'))
		out.insert(out.begin(), '_');

	// 不能空
	if (out.empty()) out = "control";

	if (IsCppKeyword(out)) out += "_";

	return out;
}

void CodeGenerator::BuildVarNameMap()
{
	_varNameOf.clear();
	_varNameOf.reserve(_sourceDocument.Nodes.size());

	std::unordered_set<std::string> used;
	used.reserve(_sourceDocument.Nodes.size());

	for (const auto& node : _sourceDocument.Nodes)
	{
		std::string base = SanitizeCppIdentifier(
			WStringToString(node.Name));
		// 保守：成员变量建议以小写开头，避免与类型名混淆（仅在安全情况下调整）
		if (!base.empty() && base[0] >= 'A' && base[0] <= 'Z')
			base[0] = (char)(base[0] - 'A' + 'a');

		std::string finalName = base;
		for (int suffix = 2; used.contains(finalName); ++suffix)
			finalName = base + std::to_string(suffix);

		// 二次防御：仍可能撞上关键字（例如 base="this" 调整后）
		if (IsCppKeyword(finalName)) finalName += "_";
		while (used.contains(finalName)) finalName += "_";

		used.insert(finalName);
		_varNameOf[&node] = finalName;
	}
}

std::string CodeGenerator::GetVarName(
	const DesignerModel::DesignNode& node) const
{
	auto it = _varNameOf.find(&node);
	if (it != _varNameOf.end()) return it->second;
	return SanitizeCppIdentifier(WStringToString(node.Name));
}

std::string CodeGenerator::WStringToString(const std::wstring& wstr) const
{
	if (wstr.empty()) return std::string();
	int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, nullptr, nullptr);
	return result;
}

std::wstring CodeGenerator::StringToWString(const std::string& str) const
{
	if (str.empty()) return std::wstring();
	int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
	return result;
}

std::string CodeGenerator::GetControlTypeName(UIClass type) const
{
	switch (type)
	{
	case UIClass::UI_Label: return "Label";
	case UIClass::UI_Button: return "Button";
	case UIClass::UI_TextBox: return "TextBox";
	case UIClass::UI_RichTextBox: return "RichTextBox";
	case UIClass::UI_PasswordBox: return "PasswordBox";
	case UIClass::UI_NumericUpDown: return "NumericUpDown";
	case UIClass::UI_Panel: return "Panel";
	case UIClass::UI_Decorator: return "Decorator";
	case UIClass::UI_Border: return "Border";
	case UIClass::UI_Canvas: return "Canvas";
	case UIClass::UI_GroupBox: return "GroupBox";
	case UIClass::UI_Expander: return "Expander";
	case UIClass::UI_ScrollViewer: return "ScrollViewer";
	case UIClass::UI_Popup: return "Popup";
	case UIClass::UI_StackPanel: return "StackPanel";
	case UIClass::UI_Grid: return "Grid";
	case UIClass::UI_DockPanel: return "DockPanel";
	case UIClass::UI_WrapPanel: return "WrapPanel";
	case UIClass::UI_RelativePanel: return "RelativePanel";
	case UIClass::UI_ToggleButton: return "ToggleButton";
	case UIClass::UI_Calendar: return "Calendar";
	case UIClass::UI_DatePicker: return "DatePicker";
	case UIClass::UI_CalendarView: return "CalendarView";
	case UIClass::UI_Window: return "Window";
	case UIClass::UI_CheckBox: return "CheckBox";
	case UIClass::UI_RadioButton: return "RadioButton";
	case UIClass::UI_ComboBox: return "ComboBox";
	case UIClass::UI_ComboBoxItem: return "ComboBoxItem";
	case UIClass::UI_ListView: return "ListView";
	case UIClass::UI_ListBox: return "ListBox";
	case UIClass::UI_ListViewItem: return "ListViewItem";
	case UIClass::UI_ListBoxItem: return "ListBoxItem";
	case UIClass::UI_ChartView: return "ChartView";
	case UIClass::UI_TreeView: return "TreeView";
	case UIClass::UI_TreeViewItem: return "TreeViewItem";
	case UIClass::UI_ProgressBar: return "ProgressBar";
	case UIClass::UI_LoadingRing: return "LoadingRing";
	case UIClass::UI_ProgressRing: return "ProgressRing";
	case UIClass::UI_Slider: return "Slider";
	case UIClass::UI_Image: return "Image";
	case UIClass::UI_Switch: return "Switch";
	case UIClass::UI_TabControl: return "TabControl";
	case UIClass::UI_TabItem: return "TabItem";
	case UIClass::UI_ToolBar: return "ToolBar";
	case UIClass::UI_Menu: return "Menu";
	case UIClass::UI_MenuItem: return "MenuItem";
	case UIClass::UI_Separator: return "Separator";
	case UIClass::UI_StatusBar: return "StatusBar";
	case UIClass::UI_StatusBarItem: return "StatusBarItem";
	case UIClass::UI_ContextMenu: return "ContextMenu";
	case UIClass::UI_WebBrowser: return "WebBrowser";
	case UIClass::UI_MediaElement: return "MediaElement";
	case UIClass::UI_NativeSurface: return "NativeSurface";
	case UIClass::UI_ItemsControl: return "ItemsControl";
	case UIClass::UI_ContentPresenter: return "ContentPresenter";
	case UIClass::UI_ItemsPresenter: return "ItemsPresenter";
	case UIClass::UI_ContentControl: return "ContentControl";
	case UIClass::UI_DataGrid: return "DataGrid";
	case UIClass::UI_DataGridRow: return "DataGridRow";
	case UIClass::UI_DataGridCell: return "DataGridCell";
	case UIClass::UI_DataGridColumnHeader: return "DataGridColumnHeader";
	case UIClass::UI_DataGridColumnHeadersPresenter:
		return "DataGridColumnHeadersPresenter";
	case UIClass::UI_DataGridRowHeader: return "DataGridRowHeader";
	default: return "Control";
	}
}

std::string CodeGenerator::GetIncludeForType(UIClass type) const
{
	switch (type)
	{
	case UIClass::UI_TabControl:
	case UIClass::UI_TabItem:
		return "TabControl.h";
	case UIClass::UI_ComboBox:
	case UIClass::UI_ComboBoxItem:
		return "ComboBox.h";
	case UIClass::UI_ListBox:
	case UIClass::UI_ListBoxItem:
		return "ListBox.h";
	case UIClass::UI_ListView:
	case UIClass::UI_ListViewItem:
		return "ListView.h";
	case UIClass::UI_DataGrid:
	case UIClass::UI_DataGridRow:
	case UIClass::UI_DataGridCell:
	case UIClass::UI_DataGridColumnHeader:
	case UIClass::UI_DataGridColumnHeadersPresenter:
	case UIClass::UI_DataGridRowHeader:
		return "DataGrid.h";
	case UIClass::UI_TreeView:
	case UIClass::UI_TreeViewItem:
		return "TreeView.h";
	case UIClass::UI_Menu:
	case UIClass::UI_MenuItem:
		return "Menu.h";
	case UIClass::UI_Separator:
		return "Separator.h";
	case UIClass::UI_ToolBar:
		return "ToolBar.h";
	case UIClass::UI_ContextMenu:
		return "ContextMenu.h";
	case UIClass::UI_StatusBar:
	case UIClass::UI_StatusBarItem:
		return "StatusBar.h";
	case UIClass::UI_StackPanel:
		return "Layout/StackPanel.h";
	case UIClass::UI_Grid:
		return "Layout/Grid.h";
	case UIClass::UI_DockPanel:
		return "Layout/DockPanel.h";
	case UIClass::UI_WrapPanel:
		return "Layout/WrapPanel.h";
	case UIClass::UI_RelativePanel:
		return "Layout/RelativePanel.h";
	default:
		return GetControlTypeName(type) + ".h";
	}
}

std::string CodeGenerator::GetComponentClassName(
	const DesignerModel::DesignComponentDefinition& component) const
{
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	std::string result = identity.GeneratedLeaf
		+ SanitizeCppIdentifier(WStringToString(component.Type.XamlName));
	const auto sameLocalNameCount = std::count_if(
		_sourceDocument.Components.begin(), _sourceDocument.Components.end(),
		[&](const auto& candidate)
		{
			return candidate.Type.XamlName == component.Type.XamlName;
		});
	if (sameLocalNameCount > 1)
	{
		const auto found = std::find_if(
			_sourceDocument.Components.begin(), _sourceDocument.Components.end(),
			[&](const auto& candidate) { return &candidate == &component; });
		if (found == _sourceDocument.Components.end())
			throw std::invalid_argument(
				"Generated component is not owned by the source document");
		result += "_" + std::to_string(
			static_cast<size_t>(
				found - _sourceDocument.Components.begin()) + 1);
	}
	return result;
}

std::string CodeGenerator::GetGeneratedControlTypeName(
	const DesignerModel::DesignNode& node) const
{
	if (node.ComponentType.Empty()
		|| _outputKind == CodeGeneratorOutputKind::Window)
		return GetControlTypeName(node.Type);
	const auto* component = _sourceDocument.FindComponent(node.ComponentType);
	if (!component)
		throw std::invalid_argument(
			"Generated component instance has no ComponentDefinition");
	return GetComponentClassName(*component);
}

std::optional<CodeGenerator::TypedPropertyInfo>
CodeGenerator::FindGeneratedProperty(
	const DesignerModel::DesignNode& node,
	const std::wstring& propertyName) const
{
	if (!node.ComponentType.Empty())
	{
		const auto* component =
			_sourceDocument.FindComponent(node.ComponentType);
		if (!component)
			throw std::invalid_argument(
				"Generated component instance has no ComponentDefinition");
		const auto property = std::find_if(
			component->Properties.begin(), component->Properties.end(),
			[&](const auto& candidate)
			{ return candidate.Name == propertyName; });
		if (property != component->Properties.end())
		{
			if (property->IsReadOnly) return std::nullopt;
			const auto* valueType =
				ComponentValueCppType(property->DefaultValue.Kind);
			if (!valueType)
				throw std::invalid_argument(
					"Generated component property type is unsupported");
			TypedPropertyInfo result;
			result.SetterName = "Set" + SanitizeCppIdentifier(
				WStringToString(property->Name));
			result.SharedValueType = valueType;
			return result;
		}
	}
	return FindKnownProperty(node.Type, propertyName);
}

std::string CodeGenerator::FindKnownDependencyPropertyExpression(
	UIClass type,
	const std::wstring& propertyName,
	bool requireWritable) const
{
	auto readOnlyIdentity = [requireWritable](std::string expression)
	{
		return requireWritable ? std::string{} : std::move(expression);
	};
	static const std::unordered_map<std::wstring, std::string>
		readOnlyControlIdentities{
			{ L"IsVisible", "Control::IsVisibleProperty()" },
			{ L"ActualWidth", "Control::ActualWidthProperty()" },
			{ L"ActualHeight", "Control::ActualHeightProperty()" },
			{ L"Validation.HasError",
				"Control::ValidationHasErrorProperty()" },
			{ L"Validation.Errors",
				"Control::ValidationErrorsProperty()" },
			{ L"IsFocused", "Control::IsFocusedProperty()" },
			{ L"IsKeyboardFocused",
				"Control::IsKeyboardFocusedProperty()" },
			{ L"IsKeyboardFocusVisible",
				"Control::IsKeyboardFocusVisibleProperty()" },
			{ L"IsKeyboardFocusWithin",
				"Control::IsKeyboardFocusWithinProperty()" },
			{ L"IsMouseOver", "Control::IsMouseOverProperty()" },
			{ L"IsMouseDirectlyOver",
				"Control::IsMouseDirectlyOverProperty()" },
			{ L"IsMouseCaptured",
				"Control::IsMouseCapturedProperty()" },
			{ L"IsMouseCaptureWithin",
				"Control::IsMouseCaptureWithinProperty()" },
		};
	if (const auto readOnly = readOnlyControlIdentities.find(propertyName);
		readOnly != readOnlyControlIdentities.end())
		return requireWritable ? std::string{} : readOnly->second;
	if (propertyName == L"Template")
		return "Control::TemplateProperty()";
	if (propertyName == L"ItemsPanel")
		return "ItemsControl::ItemsPanelProperty()";
	if (propertyName == L"ItemsSource"
		&& IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type))
		return "ItemsControl::ItemsSourceProperty()";
	if (propertyName == L"SelectionMode"
		&& (type == UIClass::UI_ListBox
			|| type == UIClass::UI_ListView
			|| type == UIClass::UI_DataGrid))
		return "ListBox::SelectionModeProperty()";
	if (type == UIClass::UI_DataGridCell && propertyName == L"IsSelected")
		return "DataGridCell::IsSelectedProperty()";
	if (type == UIClass::UI_DataGridRowHeader
		&& propertyName == L"IsRowSelected")
		return readOnlyIdentity("DataGridRowHeader::IsRowSelectedProperty()");
	if (type == UIClass::UI_DataGrid)
	{
		if (propertyName == L"AutoGenerateColumns")
			return "DataGrid::AutoGenerateColumnsProperty()";
		if (propertyName == L"EnableColumnVirtualization")
			return "DataGrid::EnableColumnVirtualizationProperty()";
		if (propertyName == L"IsReadOnly")
			return "DataGrid::IsReadOnlyProperty()";
		if (propertyName == L"CanUserSortColumns")
			return "DataGrid::CanUserSortColumnsProperty()";
		if (propertyName == L"CanUserResizeColumns")
			return "DataGrid::CanUserResizeColumnsProperty()";
		if (propertyName == L"SelectionUnit")
			return "DataGrid::SelectionUnitProperty()";
		if (propertyName == L"ColumnHeaderHeight")
			return "DataGrid::ColumnHeaderHeightProperty()";
		if (propertyName == L"RowHeaderWidth")
			return "DataGrid::RowHeaderWidthProperty()";
		if (propertyName == L"RowHeaderActualWidth")
			return readOnlyIdentity(
				"DataGrid::RowHeaderActualWidthProperty()");
		if (propertyName == L"RowHeight")
			return "DataGrid::RowHeightProperty()";
		if (propertyName == L"HeadersVisibility")
			return "DataGrid::HeadersVisibilityProperty()";
		if (propertyName == L"GridLinesVisibility")
			return "DataGrid::GridLinesVisibilityProperty()";
		if (propertyName == L"RowBackground")
			return "DataGrid::RowBackgroundProperty()";
		if (propertyName == L"AlternatingRowBackground")
			return "DataGrid::AlternatingRowBackgroundProperty()";
		if (propertyName == L"HorizontalGridLinesBrush")
			return "DataGrid::HorizontalGridLinesBrushProperty()";
		if (propertyName == L"VerticalGridLinesBrush")
			return "DataGrid::VerticalGridLinesBrushProperty()";
	}
	if (type == UIClass::UI_TreeViewItem
		&& propertyName == L"HasItems")
		return readOnlyIdentity("TreeViewItem::HasItemsProperty()");
	if (type == UIClass::UI_MenuItem)
	{
		if (propertyName == L"Icon")
			return "MenuItem::IconProperty()";
		if (propertyName == L"IsCheckable")
			return "MenuItem::IsCheckableProperty()";
		if (propertyName == L"IsChecked")
			return "MenuItem::IsCheckedProperty()";
		if (propertyName == L"IsHighlighted")
			return readOnlyIdentity("MenuItem::IsHighlightedProperty()");
		if (propertyName == L"Role")
			return readOnlyIdentity("MenuItem::RoleProperty()");
		if (propertyName == L"IsSubmenuOpen")
			return "MenuItem::IsSubmenuOpenProperty()";
	}
	if (type == UIClass::UI_ComboBoxItem
		&& propertyName == L"IsHighlighted")
		return readOnlyIdentity("ComboBoxItem::IsHighlightedProperty()");
	if (type == UIClass::UI_ComboBox)
	{
		if (propertyName == L"IsDropDownOpen")
			return "ComboBox::IsDropDownOpenProperty()";
		if (propertyName == L"IsEditable")
			return "ComboBox::IsEditableProperty()";
		if (propertyName == L"IsSelectionBoxHighlighted")
			return readOnlyIdentity(
				"ComboBox::IsSelectionBoxHighlightedProperty()");
	}
	if (type == UIClass::UI_DatePicker)
	{
		if (propertyName == L"Text")
			return "DatePicker::TextProperty()";
		if (propertyName == L"IsDropDownOpen")
			return "DatePicker::IsDropDownOpenProperty()";
		if (propertyName == L"SelectedDate")
			return "DatePicker::SelectedDateProperty()";
		if (propertyName == L"DisplayDate")
			return "DatePicker::DisplayDateProperty()";
		if (propertyName == L"FirstDayOfWeek")
			return "DatePicker::FirstDayOfWeekProperty()";
		if (propertyName == L"IsTodayHighlighted")
			return "DatePicker::IsTodayHighlightedProperty()";
		if (propertyName == L"SelectedDateFormat")
			return "DatePicker::SelectedDateFormatProperty()";
	}
	if (type == UIClass::UI_Calendar
		|| type == UIClass::UI_CalendarView)
	{
		if (propertyName == L"SelectionMode")
			return "CalendarView::SelectionModeProperty()";
		if (propertyName == L"SelectedDate")
			return "CalendarView::SelectedDateProperty()";
		if (propertyName == L"DisplayDate")
			return "CalendarView::DisplayDateProperty()";
		if (propertyName == L"FirstDayOfWeek")
			return "CalendarView::FirstDayOfWeekProperty()";
		if (propertyName == L"IsTodayHighlighted")
			return "CalendarView::IsTodayHighlightedProperty()";
	}
	if (type == UIClass::UI_Slider)
	{
		if (propertyName == L"IsSelectionRangeEnabled")
			return "Slider::IsSelectionRangeEnabledProperty()";
		if (propertyName == L"IsThumbDragging")
			return readOnlyIdentity("Slider::IsThumbDraggingProperty()");
	}
	if (type == UIClass::UI_TabItem
		&& propertyName == L"TabStripPlacement")
		return readOnlyIdentity("TabItem::TabStripPlacementProperty()");
	if (type == UIClass::UI_TabControl)
	{
		if (propertyName == L"SelectedContent")
			return readOnlyIdentity(
				"TabControl::SelectedContentProperty()");
		if (propertyName == L"SelectedContentTemplate")
			return readOnlyIdentity(
				"TabControl::SelectedContentTemplateProperty()");
	}
	if (type == UIClass::UI_Expander
		&& propertyName == L"ExpandDirection")
		return "Expander::ExpandDirectionProperty()";
	if (type == UIClass::UI_Popup
		&& propertyName == L"IsOpen")
		return "Popup::IsOpenProperty()";
	if (type == UIClass::UI_ContextMenu
		&& propertyName == L"IsOpen")
		return "ContextMenu::IsOpenProperty()";
	if (type == UIClass::UI_PasswordBox)
	{
		if (propertyName == L"CaretBrush")
			return "TextBoxBase::CaretBrushProperty()";
		if (propertyName == L"SelectionBrush")
			return "TextBoxBase::SelectionBrushProperty()";
		if (propertyName == L"PasswordChar")
			return "PasswordBox::PasswordCharProperty()";
	}
	if (IsUIClassAssignableFrom(UIClass::UI_ButtonBase, type)
		&& propertyName == L"IsPressed")
		return readOnlyIdentity("ButtonBase::IsPressedProperty()");

	if (GetKnownProperties().contains(propertyName))
	{
		// These structural AddOwner surfaces do not share Control's identity.
		// Select the concrete WPF owner before the common Control map.
		if (type == UIClass::UI_Label
			&& propertyName == L"Background")
			return "Label::BackgroundProperty()";
		if (type == UIClass::UI_Border
			&& propertyName == L"Padding")
			return "Border::PaddingProperty()";
		static const std::unordered_map<std::wstring, std::string>
			specialIdentities{
				{ L"FocusManager.IsFocusScope",
					"Control::IsFocusScopeProperty()" },
				{ L"KeyboardNavigation.TabNavigation",
					"Control::TabNavigationProperty()" },
				{ L"KeyboardNavigation.DirectionalNavigation",
					"Control::DirectionalNavigationProperty()" },
				{ L"AutomationProperties.Name",
					"Control::AutomationNameProperty()" },
				{ L"AutomationProperties.FullDescription",
					"Control::AutomationFullDescriptionProperty()" },
				{ L"AutomationProperties.HelpText",
					"Control::AutomationHelpTextProperty()" },
				{ L"AutomationProperties.AutomationId",
					"Control::AutomationIdProperty()" },
				{ L"Canvas.Left", "Control::CanvasLeftProperty()" },
				{ L"Canvas.Top", "Control::CanvasTopProperty()" },
				{ L"Canvas.Right", "Control::CanvasRightProperty()" },
				{ L"Canvas.Bottom", "Control::CanvasBottomProperty()" },
				{ L"Grid.Row", "Control::GridRowProperty()" },
				{ L"Grid.Column", "Control::GridColumnProperty()" },
				{ L"Grid.RowSpan", "Control::GridRowSpanProperty()" },
				{ L"Grid.ColumnSpan",
					"Control::GridColumnSpanProperty()" },
				{ L"DockPanel.Dock",
					"Control::DockPositionProperty()" },
			};
		if (const auto special = specialIdentities.find(propertyName);
			special != specialIdentities.end())
			return special->second;
		return "Control::"
			+ SanitizeCppIdentifier(WStringToString(propertyName))
			+ "Property()";
	}

	if (!FindKnownProperty(type, propertyName)
		|| propertyName.find(L'.') != std::wstring::npos)
		return {};
	return GetControlTypeName(type) + "::"
		+ SanitizeCppIdentifier(WStringToString(propertyName))
		+ "Property()";
}

std::string CodeGenerator::FindGeneratedDependencyPropertyExpression(
	const DesignerModel::DesignNode& node,
	const std::wstring& propertyName,
	bool requireWritable) const
{
	if (!node.ComponentType.Empty())
	{
		const auto* component =
			_sourceDocument.FindComponent(node.ComponentType);
		if (!component)
			throw std::invalid_argument(
				"Generated component instance has no ComponentDefinition");
		const auto property = std::find_if(
			component->Properties.begin(), component->Properties.end(),
			[&](const auto& candidate)
			{ return candidate.Name == propertyName; });
		if (property != component->Properties.end())
		{
			if (requireWritable && property->IsReadOnly) return {};
			return GetComponentClassName(*component) + "::"
				+ SanitizeCppIdentifier(WStringToString(property->Name))
				+ "Property()";
		}
	}
	return FindKnownDependencyPropertyExpression(
		node.Type, propertyName, requireWritable);
}

std::string CodeGenerator::FindComponentDependencyPropertyExpression(
	const DesignerModel::DesignComponentDefinition& component,
	const std::wstring& propertyName,
	bool requireWritable) const
{
	const auto property = std::find_if(
		component.Properties.begin(), component.Properties.end(),
		[&](const auto& candidate)
		{ return candidate.Name == propertyName; });
	if (property != component.Properties.end())
	{
		if (requireWritable && property->IsReadOnly) return {};
		return GetComponentClassName(component) + "::"
			+ SanitizeCppIdentifier(WStringToString(property->Name))
			+ "Property()";
	}
	return FindKnownDependencyPropertyExpression(
		component.BaseType, propertyName, requireWritable);
}

std::string CodeGenerator::FindStyleDependencyPropertyExpression(
	const DesignerModel::DesignDocument& document,
	const DesignerStyleRule& rule,
	const std::wstring& propertyName,
	bool requireWritable) const
{
	UIClass targetType = rule.Type;
	if (!rule.ComponentType.Empty())
	{
		const auto* component = document.FindComponent(rule.ComponentType);
		if (!component)
			throw std::invalid_argument(
				"Static Style targets an unknown ComponentDefinition");
		const auto property = std::find_if(
			component->Properties.begin(), component->Properties.end(),
			[&propertyName](const auto& candidate)
			{ return candidate.Name == propertyName; });
		if (property != component->Properties.end())
		{
			if (requireWritable && property->IsReadOnly)
				throw std::invalid_argument(
					"Static Style property has no writable "
					"DependencyProperty identity: "
					+ WStringToString(propertyName));
			return GetComponentClassName(*component) + "::"
				+ SanitizeCppIdentifier(WStringToString(property->Name))
				+ "Property()";
		}
		targetType = component->BaseType;
	}
	const auto expression = FindKnownDependencyPropertyExpression(
		targetType, propertyName, requireWritable);
	if (expression.empty())
		throw std::invalid_argument(
			std::string("Static Style property has no ")
			+ (requireWritable ? "writable" : "readable")
			+ " DependencyProperty identity: "
			+ WStringToString(propertyName));
	return expression;
}

std::string CodeGenerator::EscapeWStringLiteral(const std::wstring& s)
{
	std::wstring out;
	out.reserve(s.size());
	for (wchar_t c : s)
	{
		switch (c)
		{
		case L'\\': out += L"\\\\"; break;
		case L'\"': out += L"\\\""; break;
		case L'\r': out += L"\\r"; break;
		case L'\n': out += L"\\n"; break;
		case L'\t': out += L"\\t"; break;
		default: out.push_back(c); break;
		}
	}
	return WStringToString(out);
}

std::string CodeGenerator::FloatLiteral(float v)
{
	// 生成合法 C++ float 字面量：保证有小数点，再加 f 后缀。
	// 例如：0 -> 0.f，1 -> 1.f，0.25 -> 0.25f
	const float eps = 1e-6f;
	if (v == FLT_MAX) return "FLT_MAX";
	if (v == -FLT_MAX) return "-FLT_MAX";

	if (!std::isfinite(v))
	{
		if (v > 0) return "3.402823e+38f";
		if (v < 0) return "-3.402823e+38f";
		return "0.f";
	}

	float av = std::fabs(v);
	float rounded = std::round(v);
	if (std::fabs(v - rounded) <= eps && av <= (float)INT_MAX)
	{
		std::ostringstream oss;
		oss << (int)rounded << ".f";
		return oss.str();
	}

	std::ostringstream oss;
	if ((av != 0.0f && av < 1e-4f) || av >= 1e6f)
	{
		oss.setf(std::ios::scientific);
		oss.precision(6);
		oss << v;
	}
	else
	{
		oss.setf(std::ios::fixed);
		oss.precision(6);
		oss << v;
	}

	std::string s = oss.str();
	while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0')
		s.pop_back();
	if (!s.empty() && s.back() == '.')
		s.push_back('0');
	return s + "f";
}

std::string CodeGenerator::DoubleLiteral(double v)
{
	const double eps = 1e-9;
	if (!std::isfinite(v))
	{
		if (v > 0) return "1.7976931348623157e+308";
		if (v < 0) return "-1.7976931348623157e+308";
		return "0.0";
	}

	double av = std::fabs(v);
	double rounded = std::round(v);
	if (std::fabs(v - rounded) <= eps && av <= (double)INT_MAX)
	{
		std::ostringstream oss;
		oss << (int)rounded << ".0";
		return oss.str();
	}

	std::ostringstream oss;
	if ((av != 0.0 && av < 1e-6) || av >= 1e9)
	{
		oss.setf(std::ios::scientific);
		oss.precision(12);
		oss << v;
	}
	else
	{
		oss.setf(std::ios::fixed);
		oss.precision(12);
		oss << v;
	}

	std::string s = oss.str();
	while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0')
		s.pop_back();
	if (!s.empty() && s.back() == '.')
		s.push_back('0');
	return s.empty() ? "0.0" : s;
}

std::string CodeGenerator::ColorToString(D2D1_COLOR_F color)
{
	std::ostringstream oss;
	// D2D1::ColorF is a helper class derived from D2D1_COLOR_F.  Keeping that
	// exact helper type inside BindingValue defeats metadata converters that
	// intentionally consume the canonical D2D1_COLOR_F value.
	oss << "D2D1_COLOR_F{"
		<< FloatLiteral(color.r) << ", "
		<< FloatLiteral(color.g) << ", "
		<< FloatLiteral(color.b) << ", "
		<< FloatLiteral(color.a) << "}";
	return oss.str();
}

std::string CodeGenerator::ThicknessToString(const Thickness& t)
{
	std::ostringstream oss;
	oss << "Thickness(" << FloatLiteral(t.Left) << ", " << FloatLiteral(t.Top) << ", "
		<< FloatLiteral(t.Right) << ", " << FloatLiteral(t.Bottom) << ")";
	return oss.str();
}

std::string CodeGenerator::GridLengthToCtorString(
	const DesignerModel::DesignGridLength& length)
{
	if (length.Unit == DesignerModel::DesignGridLengthUnit::Auto)
		return "GridLength::Auto()";
	if (length.Unit == DesignerModel::DesignGridLengthUnit::Star)
	{
		std::ostringstream oss;
		oss << "GridLength::Star("
			<< FloatLiteral(static_cast<float>(length.Value)) << ")";
		return oss.str();
	}
	std::ostringstream oss;
	oss << "GridLength::Pixels("
		<< FloatLiteral(static_cast<float>(length.Value)) << ")";
	return oss.str();
}

std::string CodeGenerator::GenerateControlInstantiation(
	const DesignerModel::DesignNode& node, int indent)
{
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(node);
	std::string typeName = GetGeneratedControlTypeName(node);
	code << indentStr << "// " << name << "\n";
	code << indentStr << "auto __owned_" << name
		<< " = std::make_unique<" << typeName << ">();\n";
	if (node.TemplateState.Generated)
		code << indentStr << "auto* " << name
			<< " = __owned_" << name << ".get();\n";
	else
		code << indentStr << name << " = __owned_" << name << ".get();\n";
	code << indentStr << "(void)" << name
		<< "->ClearPropertyValues();\n";
	const bool dynamicOutput =
		_outputKind == CodeGeneratorOutputKind::Window;
	// Built-in AOT controls already have an authoritative UIClass identity.
	// Attaching a declarative descriptor to every instance is a dynamic-XAML
	// compatibility cost and is unnecessary in production-generated code.
	if (node.XamlType.Valid() && dynamicOutput)
	{
		const auto descriptorName = "__xamlType_" + name;
		code << indentStr << "static const auto " << descriptorName
			<< " = DeclarativeTypeDescriptor::Create(\n";
		code << indentStr << "\tRuntimeTypeId{ L\""
			<< EscapeWStringLiteral(node.XamlType.NamespaceUri) << "\", L\""
			<< EscapeWStringLiteral(node.XamlType.LocalName)
			<< "\" }, {});\n";
		code << indentStr << "if (!" << descriptorName << " || !" << name
			<< " || !cui::framework::XamlAccess::SetTypeDescriptor(*" << name
			<< ", " << descriptorName << "))\n";
		code << indentStr << "\tthrow std::runtime_error("
			<< "\"Generated XAML type attachment failed\");\n";
	}
	const auto* xamlType =
		CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(node.Type);
	if (!xamlType)
		throw std::invalid_argument(
			"Code generation encountered an unregistered native XAML type");
	if (dynamicOutput)
		code << indentStr
			<< "(void)cui::framework::DependencyPropertyAccess::SetValue(*"
			<< name << ", L\"Focusable\", BindingValue("
			<< (xamlType->FocusableByDefault ? "true" : "false")
			<< "), DependencyPropertyValueSource::Theme);\n";
	else if (xamlType->FocusableByDefault)
		code << indentStr
			<< "(void)cui::framework::DependencyPropertyAccess::SetValue(*"
			<< name << ", Control::FocusableProperty(), BindingValue(true), "
			"DependencyPropertyValueSource::Theme);\n";
	return code.str();
}

std::string CodeGenerator::GenerateControlCommonProperties(
	const DesignerModel::DesignNode& node,
	int indent)
{
	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	const std::string name = GetVarName(node);
	if ((node.Type == UIClass::UI_Button
		|| node.Type == UIClass::UI_MenuItem)
		&& !node.Structure.CommandTarget.empty())
	{
		code << indentStr << name << "->CommandTarget = "
			<< CommandTargetExpression(
				node.Structure.CommandTarget) << ";\n";
	}
	if (!node.Properties.StyleResourceKey.empty())
		code << indentStr
			<< "cui::framework::StyleAccess::SetResourceKey(*"
			<< name << ", L\""
			<< EscapeWStringLiteral(node.Properties.StyleResourceKey)
			<< "\", "
			<< (_outputKind == CodeGeneratorOutputKind::FrameworkThemeProgram
				|| node.TemplateState.StyleResourceScopeFromTheme
				? "true" : "false")
			<< (node.TemplateState.StyleResourceIsAutomatic
				? ", true" : "") << ");\n";
	return code.str();
}

std::string CodeGenerator::GenerateRelativePanelConstraints(
	const DesignerModel::DesignNode& node,
	const std::string& parentExpression,
	const std::unordered_map<std::wstring, std::string>& controlExpressions,
	int indent,
	bool returnViaFail)
{
	if (!node.Structure.RelativePanel
		|| node.Structure.RelativePanel->Empty()) return {};

	const auto& value = *node.Structure.RelativePanel;
	const auto childExpression = GetVarName(node);
	const std::string indentText(indent, '\t');
	std::ostringstream code;
	auto emitFailure = [&](const std::wstring& message, int extraIndent)
	{
		code << std::string(indent + extraIndent, '\t');
		if (returnViaFail)
			code << "return fail(L\""
				<< EscapeWStringLiteral(message) << "\");\n";
		else
			code << "throw std::runtime_error(Convert::WStringToString(L\""
				<< EscapeWStringLiteral(message) << "\"));\n";
	};

	code << indentText << "{\n";
	code << indentText << "\tauto* __relativePanel = "
		"dynamic_cast<RelativePanel*>(" << parentExpression << ");\n";
	code << indentText << "\tif (!" << childExpression
		<< " || !__relativePanel || " << childExpression
		<< "->GetVisualParent() != __relativePanel)\n";
	emitFailure(
		L"控件 " + node.Name
			+ L" 的 RelativePanel 约束只能应用于 "
				L"RelativePanel 的直接子控件。",
		2);
	code << indentText << "\tRelativeConstraints __relativeConstraints{};\n";

	const std::pair<const std::optional<bool>*, const char*> booleans[] = {
		{ &value.CenterHorizontal, "CenterHorizontal" },
		{ &value.CenterVertical, "CenterVertical" },
		{ &value.AlignLeftWithPanel, "AlignLeftWithPanel" },
		{ &value.AlignTopWithPanel, "AlignTopWithPanel" },
		{ &value.AlignRightWithPanel, "AlignRightWithPanel" },
		{ &value.AlignBottomWithPanel, "AlignBottomWithPanel" },
	};
	for (const auto& [authored, member] : booleans)
	{
		if (!*authored) continue;
		code << indentText << "\t__relativeConstraints." << member
			<< " = " << (**authored ? "true" : "false") << ";\n";
	}

	const std::pair<const std::optional<std::wstring>*, const char*>
		references[] = {
			{ &value.Above, "Above" },
			{ &value.Below, "Below" },
			{ &value.LeftOf, "LeftOf" },
			{ &value.RightOf, "RightOf" },
			{ &value.AlignLeftWith, "AlignLeftWith" },
			{ &value.AlignRightWith, "AlignRightWith" },
			{ &value.AlignTopWith, "AlignTopWith" },
			{ &value.AlignBottomWith, "AlignBottomWith" },
		};
	for (const auto& [authored, member] : references)
	{
		if (!*authored) continue;
		const auto target = controlExpressions.find(**authored);
		if (target == controlExpressions.end())
			throw std::invalid_argument(
				"Code generation encountered an unresolved RelativePanel "
				"constraint target");
		code << indentText << "\t{\n";
		code << indentText << "\t\tauto* __relativeTarget = "
			<< target->second << ";\n";
		code << indentText << "\t\tif (!__relativeTarget "
			"|| __relativeTarget == " << childExpression
			<< " || __relativeTarget->GetVisualParent() "
				"!= __relativePanel)\n";
		emitFailure(
			L"控件 " + node.Name
				+ L" 的 RelativePanel 约束目标必须是同一面板的"
					L"直接兄弟：" + **authored,
			3);
		code << indentText << "\t\t__relativeConstraints."
			<< member << " = __relativeTarget;\n";
		code << indentText << "\t}\n";
	}
	code << indentText << "\t__relativePanel->SetConstraints("
		<< childExpression << ", __relativeConstraints);\n";
	code << indentText << "}\n";
	return code.str();
}

std::string CodeGenerator::GenerateAuthoredProperties(
	const DesignerModel::DesignNode& node,
	int indent,
	const std::unordered_map<std::wstring, std::string>*
		sharedDocumentResources,
	const std::unordered_set<
		const DesignerModel::DesignPropertyAssignment*>*
		sharedDocumentResourceAssignments,
	const std::unordered_set<std::wstring>* omittedProperties)
{
	if (node.Properties.Values.empty()) return "";

	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	const std::string name = GetVarName(node);
	code << indentStr << (node.TemplateState.Generated
		? "// ControlTemplate-authored properties/resources\n"
		: "// XAML authored Local properties/resources\n");
	std::vector<std::pair<std::wstring,
		const DesignerModel::DesignPropertyAssignment*>> orderedProperties;
	orderedProperties.reserve(node.Properties.Values.size());
	for (const auto& [propertyName, assignment]
		: node.Properties.Values)
		orderedProperties.emplace_back(propertyName, &assignment);
	std::stable_sort(orderedProperties.begin(), orderedProperties.end(),
		[&](const auto& left, const auto& right)
		{
			const auto* leftMetadata = CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
				node.Type, left.first);
			const auto* rightMetadata = CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
				node.Type, right.first);
			if (leftMetadata && rightMetadata)
			{
				const auto& leftDesign = leftMetadata->Design();
				const auto& rightDesign = rightMetadata->Design();
				if (leftDesign.CategoryOrder != rightDesign.CategoryOrder)
					return leftDesign.CategoryOrder < rightDesign.CategoryOrder;
				if (leftDesign.Order != rightDesign.Order)
					return leftDesign.Order < rightDesign.Order;
			}
			else if (leftMetadata != rightMetadata)
				return leftMetadata != nullptr;
			return left.first < right.first;
		});
	for (const auto& [propertyName, assignment] : orderedProperties)
	{
		if (omittedProperties
			&& omittedProperties->contains(propertyName)) continue;
		if (_outputKind != CodeGeneratorOutputKind::Window
			&& (propertyName == L"DisplayMemberPath"
				|| propertyName == L"SelectedValuePath"
				|| propertyName == L"HeaderDisplayMemberPath"))
			throw std::invalid_argument(
				"Static member-path property reached the ordinary string "
				"property emitter without a compiled schema context: "
				+ WStringToString(propertyName));
		if (!assignment->DynamicResourceKey.empty())
		{
			if (node.TemplateState.Generated)
			{
				if (_outputKind == CodeGeneratorOutputKind::Window)
					code << indentStr
						<< "(void)cui::framework::DependencyPropertyAccess::"
						<< "SetDynamicResource(*" << name << ", L\""
						<< EscapeWStringLiteral(propertyName) << "\", L\""
						<< EscapeWStringLiteral(
							assignment->DynamicResourceKey)
						<< "\", DependencyPropertyValueSource::Template);\n";
				else
				{
					const auto property =
						FindGeneratedDependencyPropertyExpression(
							node, propertyName, true);
					if (property.empty())
						throw std::invalid_argument(
							"Static ControlTemplate DynamicResource property "
							"has no C++ dependency-property identity: "
							+ WStringToString(propertyName));
					code << indentStr
						<< "(void)cui::framework::DependencyPropertyAccess::"
						<< "SetDynamicResource(*" << name << ", "
						<< property << ", L\""
						<< EscapeWStringLiteral(
							assignment->DynamicResourceKey)
						<< "\", DependencyPropertyValueSource::Template);\n";
				}
			}
			else
			{
				if (_outputKind == CodeGeneratorOutputKind::Window)
					code << indentStr << "(void)" << name
						<< "->SetDynamicResource(L\""
						<< EscapeWStringLiteral(propertyName) << "\", L\""
						<< EscapeWStringLiteral(
							assignment->DynamicResourceKey)
						<< "\");\n";
				else
				{
					const auto property =
						FindGeneratedDependencyPropertyExpression(
							node, propertyName, true);
					if (property.empty())
						throw std::invalid_argument(
							"Static DynamicResource property has no C++ "
							"dependency-property identity: "
							+ WStringToString(propertyName));
					code << indentStr
						<< "(void)cui::framework::DependencyPropertyAccess::"
						<< "SetDynamicResource(*" << name << ", "
						<< property << ", L\""
						<< EscapeWStringLiteral(
							assignment->DynamicResourceKey)
						<< "\", DependencyPropertyValueSource::Local);\n";
				}
			}
			continue;
		}
		const bool usesSharedDocumentResource =
			sharedDocumentResources
			&& sharedDocumentResourceAssignments
			&& sharedDocumentResourceAssignments->contains(assignment);
		std::string valueExpression;
		if (usesSharedDocumentResource)
		{
			const auto shared =
				sharedDocumentResources->find(assignment->ResourceKey);
			if (shared == sharedDocumentResources->end())
				throw std::invalid_argument(
					"Generated StaticResource assignment has no shared value");
			valueExpression = shared->second;
		}
		else
			valueExpression =
				GenerateStyleValueExpression(assignment->Value);
		if (node.TemplateState.Generated)
		{
			if (_outputKind == CodeGeneratorOutputKind::Window)
				code << indentStr
					<< "(void)cui::framework::DependencyPropertyAccess::"
					<< "SetValue(*" << name << ", L\""
					<< EscapeWStringLiteral(propertyName) << "\", "
					<< valueExpression
					<< ", DependencyPropertyValueSource::Template);\n";
			else
			{
				const auto property =
					FindGeneratedDependencyPropertyExpression(
						node, propertyName, true);
				if (property.empty())
					throw std::invalid_argument(
						"Static ControlTemplate property has no C++ "
						"dependency-property identity: "
						+ WStringToString(propertyName));
				code << indentStr
					<< "(void)cui::framework::DependencyPropertyAccess::"
					<< "SetValue(*" << name << ", " << property << ", "
					<< valueExpression
					<< ", DependencyPropertyValueSource::Template);\n";
			}
		}
		else
		{
			const auto typed =
				_outputKind == CodeGeneratorOutputKind::Window
					? std::optional<TypedPropertyInfo>{}
					: FindGeneratedProperty(node, propertyName);
			const auto typedCall = typed
				? GenerateTypedPropertyCall(
					name, *typed, valueExpression,
					usesSharedDocumentResource)
				: std::string{};
			if (!typedCall.empty())
				code << indentStr << typedCall << ";\n";
			else if (_outputKind == CodeGeneratorOutputKind::Window)
				code << indentStr << "(void)" << name
					<< "->TrySetPropertyValue(L\""
					<< EscapeWStringLiteral(propertyName) << "\", "
					<< valueExpression << ");\n";
			else
			{
				const auto property =
					FindGeneratedDependencyPropertyExpression(
						node, propertyName, true);
				if (property.empty())
					throw std::invalid_argument(
						"Static authored property has no writable "
						"DependencyProperty identity: "
						+ WStringToString(propertyName));
				code << indentStr
					<< "(void)cui::framework::DependencyPropertyAccess::"
					<< "SetValue(*" << name << ", " << property << ", "
					<< valueExpression
					<< ", DependencyPropertyValueSource::Local);\n";
			}
		}
	}
	return code.str();
}

std::string CodeGenerator::GenerateTransformExpression(
	const cui::drawing::Transform& value)
{
	std::ostringstream expression;
	expression << "[] { cui::drawing::Transform value; ";
	for (const auto& operation : value.Operations)
	{
		expression << "{ cui::drawing::TransformOperation operation; operation.Kind = ";
		switch (operation.Kind)
		{
		case cui::drawing::TransformKind::Matrix:
			expression << "cui::drawing::TransformKind::Matrix; operation.Matrix = "
				"D2D1::Matrix3x2F(" << FloatLiteral(operation.Matrix._11) << ", "
				<< FloatLiteral(operation.Matrix._12) << ", "
				<< FloatLiteral(operation.Matrix._21) << ", "
				<< FloatLiteral(operation.Matrix._22) << ", "
				<< FloatLiteral(operation.Matrix._31) << ", "
				<< FloatLiteral(operation.Matrix._32) << "); ";
			break;
		case cui::drawing::TransformKind::Translate:
			expression << "cui::drawing::TransformKind::Translate; operation.X = "
				<< FloatLiteral(operation.X) << "; operation.Y = "
				<< FloatLiteral(operation.Y) << "; ";
			break;
		case cui::drawing::TransformKind::Scale:
			expression << "cui::drawing::TransformKind::Scale; operation.ScaleX = "
				<< FloatLiteral(operation.ScaleX) << "; operation.ScaleY = "
				<< FloatLiteral(operation.ScaleY) << "; operation.CenterX = "
				<< FloatLiteral(operation.CenterX) << "; operation.CenterY = "
				<< FloatLiteral(operation.CenterY) << "; ";
			break;
		case cui::drawing::TransformKind::Rotate:
			expression << "cui::drawing::TransformKind::Rotate; operation.Angle = "
				<< FloatLiteral(operation.Angle) << "; operation.CenterX = "
				<< FloatLiteral(operation.CenterX) << "; operation.CenterY = "
				<< FloatLiteral(operation.CenterY) << "; ";
			break;
		case cui::drawing::TransformKind::Skew:
			expression << "cui::drawing::TransformKind::Skew; operation.AngleX = "
				<< FloatLiteral(operation.AngleX) << "; operation.AngleY = "
				<< FloatLiteral(operation.AngleY) << "; operation.CenterX = "
				<< FloatLiteral(operation.CenterX) << "; operation.CenterY = "
				<< FloatLiteral(operation.CenterY) << "; ";
			break;
		}
		expression << "value.Operations.push_back(operation); } ";
	}
	expression << "return value; }()";
	return expression.str();
}

std::string CodeGenerator::GenerateGeometryExpression(
	const cui::drawing::Geometry& geometry)
{
	std::ostringstream expression;
	expression << "[] { cui::drawing::Geometry value; value.Kind = ";
	switch (geometry.Kind)
	{
	case cui::drawing::GeometryKind::Rectangle:
		expression << "cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF("
			<< FloatLiteral(geometry.Rect.left) << ", "
			<< FloatLiteral(geometry.Rect.top) << ", "
			<< FloatLiteral(geometry.Rect.right) << ", "
			<< FloatLiteral(geometry.Rect.bottom) << "); value.RadiusX = "
			<< FloatLiteral(geometry.RadiusX) << "; value.RadiusY = "
			<< FloatLiteral(geometry.RadiusY) << "; ";
		break;
	case cui::drawing::GeometryKind::Ellipse:
		expression << "cui::drawing::GeometryKind::Ellipse; value.Center = D2D1::Point2F("
			<< FloatLiteral(geometry.Center.x) << ", "
			<< FloatLiteral(geometry.Center.y) << "); value.RadiusX = "
			<< FloatLiteral(geometry.RadiusX) << "; value.RadiusY = "
			<< FloatLiteral(geometry.RadiusY) << "; ";
		break;
	case cui::drawing::GeometryKind::Path:
		expression << "cui::drawing::GeometryKind::Path; ";
		if (geometry.FillRule == cui::drawing::GeometryFillRule::Nonzero)
			expression << "value.FillRule = cui::drawing::GeometryFillRule::Nonzero; ";
		for (const auto& figure : geometry.Figures)
		{
			expression << "value.Figures.push_back([] { cui::drawing::PathFigure figure; "
				"figure.StartPoint = D2D1::Point2F("
				<< FloatLiteral(figure.StartPoint.x) << ", "
				<< FloatLiteral(figure.StartPoint.y) << "); figure.IsClosed = "
				<< (figure.IsClosed ? "true" : "false") << "; figure.IsFilled = "
				<< (figure.IsFilled ? "true" : "false") << "; ";
			for (const auto& segment : figure.Segments)
			{
				expression << "{ cui::drawing::PathSegment segment; segment.Kind = ";
				const char* kind = segment.Kind == cui::drawing::PathSegmentKind::Line
					? "cui::drawing::PathSegmentKind::Line"
					: segment.Kind == cui::drawing::PathSegmentKind::Bezier
						? "cui::drawing::PathSegmentKind::Bezier"
						: segment.Kind == cui::drawing::PathSegmentKind::QuadraticBezier
							? "cui::drawing::PathSegmentKind::QuadraticBezier"
							: "cui::drawing::PathSegmentKind::Arc";
				expression << kind << "; segment.Point = D2D1::Point2F("
					<< FloatLiteral(segment.Point.x) << ", "
					<< FloatLiteral(segment.Point.y) << "); segment.Point1 = D2D1::Point2F("
					<< FloatLiteral(segment.Point1.x) << ", "
					<< FloatLiteral(segment.Point1.y) << "); segment.Point2 = D2D1::Point2F("
					<< FloatLiteral(segment.Point2.x) << ", "
					<< FloatLiteral(segment.Point2.y) << "); segment.Point3 = D2D1::Point2F("
					<< FloatLiteral(segment.Point3.x) << ", "
					<< FloatLiteral(segment.Point3.y) << "); segment.Size = D2D1::SizeF("
					<< FloatLiteral(segment.Size.width) << ", "
					<< FloatLiteral(segment.Size.height) << "); segment.RotationAngle = "
					<< FloatLiteral(segment.RotationAngle) << "; segment.IsLargeArc = "
					<< (segment.IsLargeArc ? "true" : "false") << "; segment.Sweep = "
					<< (segment.Sweep == cui::drawing::SweepDirection::Clockwise
						? "cui::drawing::SweepDirection::Clockwise"
						: "cui::drawing::SweepDirection::Counterclockwise")
					<< "; figure.Segments.push_back(segment); } ";
			}
			expression << "return figure; }()); ";
		}
		break;
	case cui::drawing::GeometryKind::Group:
		expression << "cui::drawing::GeometryKind::Group; ";
		if (geometry.FillRule == cui::drawing::GeometryFillRule::Nonzero)
			expression << "value.FillRule = cui::drawing::GeometryFillRule::Nonzero; ";
		for (const auto& child : geometry.Children)
			expression << "value.Children.push_back("
				<< GenerateGeometryExpression(child) << "); ";
		break;
	}
	if (geometry.LocalTransform)
		expression << "value.LocalTransform = "
			<< GenerateTransformExpression(*geometry.LocalTransform) << "; ";
	expression << "return value; }()";
	return expression.str();
}

std::string CodeGenerator::GenerateStyleValueExpression(const DesignerStyleValue& value)
{
	if (value.Kind == DesignerStyleValueKind::ImageSource)
	{
		return "BindingValue(cui::resources::LoadBitmapResource(L\""
			+ EscapeWStringLiteral(value.Text) + "\"))";
	}
	BindingValue runtimeValue;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, runtimeValue, nullptr, _resourceBasePath))
		return "BindingValue()";

	switch (value.Kind)
	{
	case DesignerStyleValueKind::Bool:
	{
		bool parsed = false;
		runtimeValue.TryGet(parsed);
		return std::string("BindingValue(") + (parsed ? "true" : "false") + ")";
	}
	case DesignerStyleValueKind::NullableBool:
	{
		NullableBool parsed;
		if (!runtimeValue.TryGet(parsed) || !parsed.HasValue())
			return "BindingValue(NullableBool{})";
		return std::string("BindingValue(NullableBool(")
			+ (parsed.GetValueOrDefault() ? "true))" : "false))");
	}
	case DesignerStyleValueKind::Int:
	{
		int parsed = 0;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + std::to_string(parsed) + ")";
	}
	case DesignerStyleValueKind::Int64:
	{
		long long parsed = 0;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + GeneratedInt64Literal(parsed) + ")";
	}
	case DesignerStyleValueKind::Float:
	{
		float parsed = 0.0f;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + FloatLiteral(parsed) + ")";
	}
	case DesignerStyleValueKind::Double:
	{
		double parsed = 0.0;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + DoubleLiteral(parsed) + ")";
	}
	case DesignerStyleValueKind::String:
	{
		std::wstring parsed;
		runtimeValue.TryGet(parsed);
		return "BindingValue(L\"" + EscapeWStringLiteral(parsed) + "\")";
	}
	case DesignerStyleValueKind::Color:
	{
		D2D1_COLOR_F parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + ColorToString(parsed) + ")";
	}
	case DesignerStyleValueKind::Thickness:
	{
		Thickness parsed;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + ThicknessToString(parsed) + ")";
	}
	case DesignerStyleValueKind::CornerRadius:
	{
		CornerRadius parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		return "BindingValue(::CornerRadius("
			+ FloatLiteral(parsed.TopLeft) + ", "
			+ FloatLiteral(parsed.TopRight) + ", "
			+ FloatLiteral(parsed.BottomRight) + ", "
			+ FloatLiteral(parsed.BottomLeft) + "))";
	}
	case DesignerStyleValueKind::Point:
	{
		cui::core::Point parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Point{ " + FloatLiteral(parsed.x)
			+ ", " + FloatLiteral(parsed.y) + " })";
	}
	case DesignerStyleValueKind::Vector:
	{
		cui::core::Vector parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Vector{ " + FloatLiteral(parsed.x)
			+ ", " + FloatLiteral(parsed.y) + " })";
	}
	case DesignerStyleValueKind::Rect:
	{
		cui::core::Rect parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Rect{ " + FloatLiteral(parsed.x)
			+ ", " + FloatLiteral(parsed.y)
			+ ", " + FloatLiteral(parsed.width)
			+ ", " + FloatLiteral(parsed.height) + " })";
	}
	case DesignerStyleValueKind::Size:
	{
		cui::core::Size parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Size{ " + FloatLiteral(parsed.width)
			+ ", " + FloatLiteral(parsed.height) + " })";
	}
	case DesignerStyleValueKind::Matrix:
	{
		D2D1_MATRIX_3X2_F parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(D2D1::Matrix3x2F(" + FloatLiteral(parsed._11)
			+ ", " + FloatLiteral(parsed._12)
			+ ", " + FloatLiteral(parsed._21)
			+ ", " + FloatLiteral(parsed._22)
			+ ", " + FloatLiteral(parsed._31)
			+ ", " + FloatLiteral(parsed._32) + "))";
	}
	case DesignerStyleValueKind::Length:
	{
		cui::layout::Length parsed;
		runtimeValue.TryGet(parsed);
		return parsed.IsAuto()
			? "BindingValue(cui::layout::Length::Auto())"
			: "BindingValue(cui::layout::Length::Fixed(" + FloatLiteral(parsed.value) + "))";
	}
	case DesignerStyleValueKind::Brush:
	{
		cui::drawing::Brush parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		if (parsed.Kind == cui::drawing::BrushKind::None)
			return "BindingValue(cui::drawing::NoBrush())";
		std::ostringstream expression;
		expression << "BindingValue([] { cui::drawing::Brush value; value.Kind = "
			<< (parsed.Kind == cui::drawing::BrushKind::Solid
				? "cui::drawing::BrushKind::Solid"
				: parsed.Kind == cui::drawing::BrushKind::LinearGradient
					? "cui::drawing::BrushKind::LinearGradient"
					: parsed.Kind == cui::drawing::BrushKind::RadialGradient
						? "cui::drawing::BrushKind::RadialGradient"
						: "cui::drawing::BrushKind::Image")
			<< "; value.MappingMode = "
			<< (parsed.MappingMode == cui::drawing::BrushMappingMode::Absolute
				? "cui::drawing::BrushMappingMode::Absolute"
				: "cui::drawing::BrushMappingMode::RelativeToBoundingBox")
			<< "; value.Opacity = " << FloatLiteral(parsed.Opacity) << "; ";
		if (parsed.Kind == cui::drawing::BrushKind::Solid)
			expression << "value.Color = " << ColorToString(parsed.Color) << "; ";
		else if (parsed.Kind == cui::drawing::BrushKind::LinearGradient)
			expression << "value.StartPoint = D2D1::Point2F("
				<< FloatLiteral(parsed.StartPoint.x) << ", "
				<< FloatLiteral(parsed.StartPoint.y) << "); value.EndPoint = D2D1::Point2F("
				<< FloatLiteral(parsed.EndPoint.x) << ", "
				<< FloatLiteral(parsed.EndPoint.y) << "); ";
		else if (parsed.Kind == cui::drawing::BrushKind::RadialGradient)
			expression << "value.Center = D2D1::Point2F("
				<< FloatLiteral(parsed.Center.x) << ", "
				<< FloatLiteral(parsed.Center.y) << "); value.GradientOrigin = D2D1::Point2F("
				<< FloatLiteral(parsed.GradientOrigin.x) << ", "
				<< FloatLiteral(parsed.GradientOrigin.y) << "); value.RadiusX = "
				<< FloatLiteral(parsed.RadiusX) << "; value.RadiusY = "
				<< FloatLiteral(parsed.RadiusY) << "; ";
		else
		{
			expression << "value.ImageSource = cui::resources::LoadBitmapResource(L\""
				<< EscapeWStringLiteral(parsed.ImageSource
					? parsed.ImageSource->GetSourceUri() : L"") << "\"); ";
			expression << "value.Stretch = "
				<< (parsed.Stretch == ::Stretch::None
					? "::Stretch::None"
					: parsed.Stretch == ::Stretch::Uniform
						? "::Stretch::Uniform"
						: parsed.Stretch == ::Stretch::UniformToFill
							? "::Stretch::UniformToFill"
							: "::Stretch::Fill") << "; ";
			expression << "value.AlignmentX = "
				<< (parsed.AlignmentX == cui::drawing::ImageBrushAlignmentX::Left
					? "cui::drawing::ImageBrushAlignmentX::Left"
					: parsed.AlignmentX == cui::drawing::ImageBrushAlignmentX::Right
						? "cui::drawing::ImageBrushAlignmentX::Right"
						: "cui::drawing::ImageBrushAlignmentX::Center") << "; ";
			expression << "value.AlignmentY = "
				<< (parsed.AlignmentY == cui::drawing::ImageBrushAlignmentY::Top
					? "cui::drawing::ImageBrushAlignmentY::Top"
					: parsed.AlignmentY == cui::drawing::ImageBrushAlignmentY::Bottom
						? "cui::drawing::ImageBrushAlignmentY::Bottom"
						: "cui::drawing::ImageBrushAlignmentY::Center") << "; ";
		}
		if (parsed.Kind == cui::drawing::BrushKind::LinearGradient
			|| parsed.Kind == cui::drawing::BrushKind::RadialGradient)
			for (const auto& stop : parsed.GradientStops)
				expression << "value.GradientStops.push_back({ "
					<< FloatLiteral(stop.Offset) << ", "
					<< ColorToString(stop.Color) << " }); ";
		if (parsed.Transform)
			expression << "value.Transform = "
				<< GenerateTransformExpression(*parsed.Transform) << "; ";
		if (parsed.RelativeTransform)
			expression << "value.RelativeTransform = "
				<< GenerateTransformExpression(*parsed.RelativeTransform) << "; ";
		expression << "return value; }())";
		return expression.str();
	}
	case DesignerStyleValueKind::Geometry:
	{
		cui::drawing::Geometry parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		return "BindingValue(" + GenerateGeometryExpression(parsed) + ")";
	}
	case DesignerStyleValueKind::Transform:
	{
		cui::drawing::Transform parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		return "BindingValue(" + GenerateTransformExpression(parsed) + ")";
	}
	case DesignerStyleValueKind::ImageSource:
		break;
	}
	return "BindingValue()";
}

std::string CodeGenerator::GenerateBindingValueExpression(
	const BindingValue& value)
{
	switch (value.Kind())
	{
	case BindingValueKind::Empty:
		return "BindingValue()";
	case BindingValueKind::Bool:
	{
		bool parsed = false;
		if (!value.TryGet(parsed)) break;
		return std::string("BindingValue(")
			+ (parsed ? "true)" : "false)");
	}
	case BindingValueKind::NullableBool:
	{
		NullableBool parsed;
		if (!value.TryGet(parsed)) break;
		if (!parsed.HasValue())
			return "BindingValue(NullableBool{})";
		return std::string("BindingValue(NullableBool(")
			+ (parsed.GetValueOrDefault() ? "true))" : "false))");
	}
	case BindingValueKind::Int:
	{
		int parsed = 0;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + std::to_string(parsed) + ")";
	}
	case BindingValueKind::Int64:
	{
		long long parsed = 0;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + GeneratedInt64Literal(parsed) + ")";
	}
	case BindingValueKind::Float:
	{
		float parsed = 0.0f;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + FloatLiteral(parsed) + ")";
	}
	case BindingValueKind::Double:
	{
		double parsed = 0.0;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + DoubleLiteral(parsed) + ")";
	}
	case BindingValueKind::String:
	{
		std::wstring parsed;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(L\"" + EscapeWStringLiteral(parsed) + "\")";
	}
	case BindingValueKind::Object:
		break;
	}

	D2D1_COLOR_F color{};
	if (value.TryGet(color))
		return "BindingValue(" + ColorToString(color) + ")";
	Thickness thickness;
	if (value.TryGet(thickness))
		return "BindingValue(" + ThicknessToString(thickness) + ")";
	CornerRadius cornerRadius;
	if (value.TryGet(cornerRadius))
		return "BindingValue(::CornerRadius("
			+ FloatLiteral(cornerRadius.TopLeft) + ", "
			+ FloatLiteral(cornerRadius.TopRight) + ", "
			+ FloatLiteral(cornerRadius.BottomRight) + ", "
			+ FloatLiteral(cornerRadius.BottomLeft) + "))";
	cui::core::Point point{};
	if (value.TryGet(point))
		return "BindingValue(cui::core::Point{ " + FloatLiteral(point.x)
			+ ", " + FloatLiteral(point.y) + " })";
	cui::core::Vector vector{};
	if (value.TryGet(vector))
		return "BindingValue(cui::core::Vector{ " + FloatLiteral(vector.x)
			+ ", " + FloatLiteral(vector.y) + " })";
	cui::core::Rect rect{};
	if (value.TryGet(rect))
		return "BindingValue(cui::core::Rect{ " + FloatLiteral(rect.x)
			+ ", " + FloatLiteral(rect.y)
			+ ", " + FloatLiteral(rect.width)
			+ ", " + FloatLiteral(rect.height) + " })";
	cui::core::Size size{};
	if (value.TryGet(size))
		return "BindingValue(cui::core::Size{ " + FloatLiteral(size.width)
			+ ", " + FloatLiteral(size.height) + " })";
	D2D1_MATRIX_3X2_F matrix{};
	if (value.TryGet(matrix))
		return "BindingValue(D2D1::Matrix3x2F("
			+ FloatLiteral(matrix._11) + ", " + FloatLiteral(matrix._12)
			+ ", " + FloatLiteral(matrix._21) + ", "
			+ FloatLiteral(matrix._22) + ", " + FloatLiteral(matrix._31)
			+ ", " + FloatLiteral(matrix._32) + "))";
	cui::layout::Length length;
	if (value.TryGet(length))
		return length.IsAuto()
			? "BindingValue(cui::layout::Length::Auto())"
			: "BindingValue(cui::layout::Length::Fixed("
				+ FloatLiteral(length.value) + "))";
	std::shared_ptr<BitmapSource> bitmap;
	if (value.TryGet(bitmap))
		return bitmap
			? "BindingValue(cui::resources::LoadBitmapResource(L\""
				+ EscapeWStringLiteral(bitmap->GetSourceUri()) + "\"))"
			: "BindingValue(std::shared_ptr<BitmapSource>{})";

	cui::drawing::Brush brush;
	if (value.TryGet(brush))
	{
		if (brush.Kind == cui::drawing::BrushKind::None)
			return "BindingValue(cui::drawing::NoBrush())";
		std::ostringstream expression;
		expression << "BindingValue([] { cui::drawing::Brush value; value.Kind = "
			<< (brush.Kind == cui::drawing::BrushKind::Solid
				? "cui::drawing::BrushKind::Solid"
				: brush.Kind == cui::drawing::BrushKind::LinearGradient
					? "cui::drawing::BrushKind::LinearGradient"
					: brush.Kind == cui::drawing::BrushKind::RadialGradient
						? "cui::drawing::BrushKind::RadialGradient"
						: "cui::drawing::BrushKind::Image")
			<< "; value.MappingMode = "
			<< (brush.MappingMode
					== cui::drawing::BrushMappingMode::Absolute
				? "cui::drawing::BrushMappingMode::Absolute"
				: "cui::drawing::BrushMappingMode::RelativeToBoundingBox")
			<< "; value.Opacity = " << FloatLiteral(brush.Opacity) << "; ";
		if (brush.Kind == cui::drawing::BrushKind::Solid)
			expression << "value.Color = "
				<< ColorToString(brush.Color) << "; ";
		else if (brush.Kind == cui::drawing::BrushKind::LinearGradient)
			expression << "value.StartPoint = D2D1::Point2F("
				<< FloatLiteral(brush.StartPoint.x) << ", "
				<< FloatLiteral(brush.StartPoint.y)
				<< "); value.EndPoint = D2D1::Point2F("
				<< FloatLiteral(brush.EndPoint.x) << ", "
				<< FloatLiteral(brush.EndPoint.y) << "); ";
		else if (brush.Kind == cui::drawing::BrushKind::RadialGradient)
			expression << "value.Center = D2D1::Point2F("
				<< FloatLiteral(brush.Center.x) << ", "
				<< FloatLiteral(brush.Center.y)
				<< "); value.GradientOrigin = D2D1::Point2F("
				<< FloatLiteral(brush.GradientOrigin.x) << ", "
				<< FloatLiteral(brush.GradientOrigin.y)
				<< "); value.RadiusX = "
				<< FloatLiteral(brush.RadiusX)
				<< "; value.RadiusY = "
				<< FloatLiteral(brush.RadiusY) << "; ";
		else
		{
			expression
				<< "value.ImageSource = cui::resources::LoadBitmapResource(L\""
				<< EscapeWStringLiteral(brush.ImageSource
					? brush.ImageSource->GetSourceUri() : L"")
				<< "\"); value.Stretch = "
				<< (brush.Stretch
						== ::Stretch::None
					? "::Stretch::None"
					: brush.Stretch
							== ::Stretch::Uniform
						? "::Stretch::Uniform"
						: brush.Stretch
								== ::Stretch::UniformToFill
							? "::Stretch::UniformToFill"
							: "::Stretch::Fill")
				<< "; value.AlignmentX = "
				<< (brush.AlignmentX
						== cui::drawing::ImageBrushAlignmentX::Left
					? "cui::drawing::ImageBrushAlignmentX::Left"
					: brush.AlignmentX
							== cui::drawing::ImageBrushAlignmentX::Right
						? "cui::drawing::ImageBrushAlignmentX::Right"
						: "cui::drawing::ImageBrushAlignmentX::Center")
				<< "; value.AlignmentY = "
				<< (brush.AlignmentY
						== cui::drawing::ImageBrushAlignmentY::Top
					? "cui::drawing::ImageBrushAlignmentY::Top"
					: brush.AlignmentY
							== cui::drawing::ImageBrushAlignmentY::Bottom
						? "cui::drawing::ImageBrushAlignmentY::Bottom"
						: "cui::drawing::ImageBrushAlignmentY::Center")
				<< "; ";
		}
		if (brush.Kind == cui::drawing::BrushKind::LinearGradient
			|| brush.Kind == cui::drawing::BrushKind::RadialGradient)
			for (const auto& stop : brush.GradientStops)
				expression << "value.GradientStops.push_back({ "
					<< FloatLiteral(stop.Offset) << ", "
					<< ColorToString(stop.Color) << " }); ";
		if (brush.Transform)
			expression << "value.Transform = "
				<< GenerateTransformExpression(*brush.Transform) << "; ";
		if (brush.RelativeTransform)
			expression << "value.RelativeTransform = "
				<< GenerateTransformExpression(*brush.RelativeTransform)
				<< "; ";
		expression << "return value; }())";
		return expression.str();
	}
	cui::drawing::Geometry geometry;
	if (value.TryGet(geometry))
		return "BindingValue(" + GenerateGeometryExpression(geometry) + ")";
	cui::drawing::Transform transform;
	if (value.TryGet(transform))
		return "BindingValue(" + GenerateTransformExpression(transform) + ")";

	throw std::invalid_argument(
		"Static declarative interaction contains an unsupported BindingValue type");
}

std::string CodeGenerator::GenerateDeclarativePropertyReference(
	const std::wstring& targetName,
	const std::wstring& propertyName,
	bool requireWritable,
	const DeclarativePropertyResolver& resolver)
{
	if (_outputKind == CodeGeneratorOutputKind::Window)
		return "DependencyPropertyReference(L\""
			+ EscapeWStringLiteral(propertyName) + "\")";
	const auto expression = resolver
		? resolver(targetName, propertyName, requireWritable)
		: std::string{};
	if (expression.empty())
	{
		std::string message = "Static declarative interaction property has no ";
		message += requireWritable ? "writable" : "readable";
		message += " DependencyProperty identity: ";
		if (!targetName.empty())
			message += WStringToString(targetName) + ".";
		message += WStringToString(propertyName);
		throw std::invalid_argument(std::move(message));
	}
	return "DependencyPropertyReference(" + expression + ")";
}

std::string CodeGenerator::GenerateDeclarativeAnimationCode(
	const DeclarativeVisualStateAnimation& animation,
	const std::string& collectionExpression,
	const DeclarativePropertyResolver& resolver,
	const DeclarativeTargetResolver& targetResolver,
	int indent)
{
	const std::string base(indent, '\t');
	const std::string body(indent + 1, '\t');
	std::ostringstream code;
	auto animationKind = [](DeclarativeAnimationKind value)
	{
		switch (value)
		{
		case DeclarativeAnimationKind::Color:
			return "DeclarativeAnimationKind::Color";
		case DeclarativeAnimationKind::Thickness:
			return "DeclarativeAnimationKind::Thickness";
		case DeclarativeAnimationKind::Point:
			return "DeclarativeAnimationKind::Point";
		case DeclarativeAnimationKind::Vector:
			return "DeclarativeAnimationKind::Vector";
		case DeclarativeAnimationKind::Rect:
			return "DeclarativeAnimationKind::Rect";
		case DeclarativeAnimationKind::Size:
			return "DeclarativeAnimationKind::Size";
		case DeclarativeAnimationKind::Matrix:
			return "DeclarativeAnimationKind::Matrix";
		case DeclarativeAnimationKind::Object:
			return "DeclarativeAnimationKind::Object";
		case DeclarativeAnimationKind::Double:
		default:
			return "DeclarativeAnimationKind::Double";
		}
	};
	auto easing = [](DeclarativeEasingKind value)
	{
		switch (value)
		{
		case DeclarativeEasingKind::Quadratic:
			return "DeclarativeEasingKind::Quadratic";
		case DeclarativeEasingKind::Cubic:
			return "DeclarativeEasingKind::Cubic";
		case DeclarativeEasingKind::Sine:
			return "DeclarativeEasingKind::Sine";
		case DeclarativeEasingKind::Linear:
		default:
			return "DeclarativeEasingKind::Linear";
		}
	};
	auto easingMode = [](DeclarativeEasingMode value)
	{
		switch (value)
		{
		case DeclarativeEasingMode::EaseIn:
			return "DeclarativeEasingMode::EaseIn";
		case DeclarativeEasingMode::EaseInOut:
			return "DeclarativeEasingMode::EaseInOut";
		case DeclarativeEasingMode::EaseOut:
		default:
			return "DeclarativeEasingMode::EaseOut";
		}
	};

	code << base << "{\n";
	code << body << "DeclarativeVisualStateAnimation animation;\n";
	code << body << "animation.Kind = "
		<< animationKind(animation.Kind) << ";\n";
	std::string target = "nullptr";
	if (!animation.TargetName.empty())
	{
		target = targetResolver ? targetResolver(animation.TargetName)
			: std::string{};
		if (target.empty())
			throw std::invalid_argument(
				"Static declarative animation target cannot be resolved");
	}
	code << body << "animation.Target = " << target << ";\n";
	const auto& propertyPath = animation.PropertyPath();
	if (DesignerModel::ClassifyStoryboardObjectPath(propertyPath)
		!= DesignerModel::StoryboardObjectPathKind::None)
		code << body << "animation.ObjectPath = L\""
			<< EscapeWStringLiteral(propertyPath) << "\";\n";
	else
		code << body << "animation.Property = "
			<< GenerateDeclarativePropertyReference(
				animation.TargetName, propertyPath, true, resolver)
			<< ";\n";
	if (animation.From)
		code << body << "animation.From = "
			<< GenerateBindingValueExpression(*animation.From) << ";\n";
	if (animation.To)
		code << body << "animation.To = "
			<< GenerateBindingValueExpression(*animation.To) << ";\n";
	if (animation.By)
		code << body << "animation.By = "
			<< GenerateBindingValueExpression(*animation.By) << ";\n";
	code << body << "animation.IsAdditive = "
		<< (animation.IsAdditive ? "true" : "false") << ";\n";
	code << body << "animation.IsCumulative = "
		<< (animation.IsCumulative ? "true" : "false") << ";\n";
	code << body << "animation.BeginTimeMilliseconds = "
		<< animation.BeginTimeMilliseconds << "ULL;\n";
	code << body << "animation.DurationMilliseconds = "
		<< animation.DurationMilliseconds << "ULL;\n";
	code << body << "animation.RepeatBehavior = "
		<< (animation.RepeatBehavior == DeclarativeRepeatBehaviorKind::Duration
			? "DeclarativeRepeatBehaviorKind::Duration"
			: animation.RepeatBehavior == DeclarativeRepeatBehaviorKind::Forever
				? "DeclarativeRepeatBehaviorKind::Forever"
				: "DeclarativeRepeatBehaviorKind::Count")
		<< ";\n";
	code << body << "animation.RepeatCount = "
		<< DoubleLiteral(animation.RepeatCount) << ";\n";
	code << body << "animation.RepeatDurationMilliseconds = "
		<< animation.RepeatDurationMilliseconds << "ULL;\n";
	code << body << "animation.AutoReverse = "
		<< (animation.AutoReverse ? "true" : "false") << ";\n";
	code << body << "animation.FillBehavior = "
		<< (animation.FillBehavior == DeclarativeTimelineFillBehavior::Stop
			? "DeclarativeTimelineFillBehavior::Stop"
			: "DeclarativeTimelineFillBehavior::HoldEnd")
		<< ";\n";
	code << body << "animation.SpeedRatio = "
		<< DoubleLiteral(animation.SpeedRatio) << ";\n";
	code << body << "animation.AccelerationRatio = "
		<< DoubleLiteral(animation.AccelerationRatio) << ";\n";
	code << body << "animation.DecelerationRatio = "
		<< DoubleLiteral(animation.DecelerationRatio) << ";\n";
	code << body << "animation.Easing = "
		<< easing(animation.Easing) << ";\n";
	code << body << "animation.EasingMode = "
		<< easingMode(animation.EasingMode) << ";\n";
	for (const auto& keyFrame : animation.KeyFrames)
	{
		code << body << "{\n";
		code << body << "\tDeclarativeAnimationKeyFrame keyFrame;\n";
		code << body << "\tkeyFrame.Kind = "
			<< (keyFrame.Kind == DeclarativeKeyFrameKind::Discrete
				? "DeclarativeKeyFrameKind::Discrete"
				: keyFrame.Kind == DeclarativeKeyFrameKind::Easing
					? "DeclarativeKeyFrameKind::Easing"
					: keyFrame.Kind == DeclarativeKeyFrameKind::Spline
						? "DeclarativeKeyFrameKind::Spline"
						: "DeclarativeKeyFrameKind::Linear")
			<< ";\n";
		code << body << "\tkeyFrame.KeyTimeMilliseconds = "
			<< keyFrame.KeyTimeMilliseconds << "ULL;\n";
		code << body << "\tkeyFrame.Value = "
			<< GenerateBindingValueExpression(keyFrame.Value) << ";\n";
		code << body << "\tkeyFrame.Easing = "
			<< easing(keyFrame.Easing) << ";\n";
		code << body << "\tkeyFrame.EasingMode = "
			<< easingMode(keyFrame.EasingMode) << ";\n";
		code << body << "\tkeyFrame.KeySplineX1 = "
			<< FloatLiteral(keyFrame.KeySplineX1) << ";\n";
		code << body << "\tkeyFrame.KeySplineY1 = "
			<< FloatLiteral(keyFrame.KeySplineY1) << ";\n";
		code << body << "\tkeyFrame.KeySplineX2 = "
			<< FloatLiteral(keyFrame.KeySplineX2) << ";\n";
		code << body << "\tkeyFrame.KeySplineY2 = "
			<< FloatLiteral(keyFrame.KeySplineY2) << ";\n";
		code << body
			<< "\tanimation.KeyFrames.push_back(std::move(keyFrame));\n";
		code << body << "}\n";
	}
	code << body << collectionExpression
		<< ".push_back(std::move(animation));\n";
	code << base << "}\n";
	return code.str();
}

std::string CodeGenerator::GenerateDeclarativeStoryboardActionsCode(
	const std::vector<DeclarativeEventTriggerActionDefinition>& actions,
	const std::string& collectionExpression,
	const DeclarativePropertyResolver& resolver,
	const DeclarativeTargetResolver& targetResolver,
	int indent)
{
	const std::string base(indent, '\t');
	const std::string body(indent + 1, '\t');
	std::ostringstream code;
	for (const auto& action : actions)
	{
		code << base << "{\n";
		code << body
			<< "DeclarativeEventTriggerActionDefinition action;\n";
		code << body << "action.Kind = "
			<< (action.Kind == DeclarativeStoryboardActionKind::Begin
				? "DeclarativeStoryboardActionKind::Begin"
				: action.Kind == DeclarativeStoryboardActionKind::Pause
					? "DeclarativeStoryboardActionKind::Pause"
					: action.Kind == DeclarativeStoryboardActionKind::Resume
						? "DeclarativeStoryboardActionKind::Resume"
						: "DeclarativeStoryboardActionKind::Stop")
			<< ";\n";
		code << body << "action.StoryboardName = L\""
			<< EscapeWStringLiteral(action.StoryboardName) << "\";\n";
		for (const auto& animation : action.Animations)
			code << GenerateDeclarativeAnimationCode(
				animation, "action.Animations", resolver,
				targetResolver, indent + 1);
		code << body << collectionExpression
			<< ".push_back(std::move(action));\n";
		code << base << "}\n";
	}
	return code.str();
}

std::string CodeGenerator::GenerateDeclarativeInteractionsCode(
	const std::vector<DeclarativeVisualStateGroupDefinition>& visualStateGroups,
	const std::vector<DeclarativeEventTriggerDefinition>& eventTriggers,
	const DeclarativePropertyResolver& resolver,
	const DeclarativeTargetResolver& targetResolver,
	const DeclarativeEventResolver& eventResolver,
	const std::string& targetExpression,
	int indent)
{
	if (visualStateGroups.empty() && eventTriggers.empty()) return {};
	using EmissionRange = GeneratedCompiledRange;

	const std::string tabs(indent, '\t');
	const std::string inner(indent + 1, '\t');
	const std::string deep(indent + 2, '\t');
	std::ostringstream code;
	auto indexExpression = [](size_t value, const char* context)
		{ return GeneratedCompiledIndexExpression(value, context); };
	auto optionalIndexExpression = [](const std::optional<size_t>& value,
		const char* context)
		{ return GeneratedCompiledOptionalIndexExpression(value, context); };
	auto rangeExpression = [](const EmissionRange& range)
		{ return GeneratedCompiledRangeExpression(range); };

	std::vector<std::string> valueElements;
	auto addValue = [&](const BindingValue& value)
	{
		const size_t index = valueElements.size();
		(void)CheckedCompiledInteractionIndex(index,
			"Compiled interaction value index");
		valueElements.push_back(GenerateBindingValueExpression(value));
		return index;
	};
	GeneratedCompiledStoryboardTables storyboardTables{
		addValue,
		[&](const std::wstring& targetName, const std::wstring& propertyName,
			bool requireWritable)
		{
			return GenerateDeclarativePropertyReference(
				targetName, propertyName, requireWritable, resolver);
		},
		targetResolver,
		[&](float value) { return FloatLiteral(value); },
		[&](double value) { return DoubleLiteral(value); }
	};

	std::vector<std::string> conditionElements;
	std::vector<std::string> setterElements;

	std::vector<std::string> stateEventElements;
	std::vector<std::string> stateElements;
	std::vector<std::string> transitionElements;
	std::vector<std::string> groupConditionOperandElements;
	std::vector<std::string> groupElements;
	std::map<std::uint64_t, std::wstring> groupNamesByToken;
	std::map<std::uint64_t, std::wstring> stateNamesByToken;
	for (const auto& sourceGroup : visualStateGroups)
	{
		if (sourceGroup.Name.empty())
			throw std::invalid_argument(
				"Compiled VisualStateGroup has an empty token name");
		const auto groupToken = GeneratedInteractionNameTokenValue(sourceGroup.Name);
		const auto [groupTokenEntry, insertedGroupToken] =
			groupNamesByToken.emplace(groupToken, sourceGroup.Name);
		if (!insertedGroupToken)
			throw std::invalid_argument(groupTokenEntry->second == sourceGroup.Name
				? "Compiled VisualStateGroup name is duplicated"
				: "Compiled VisualStateGroup token collision");

		std::unordered_map<std::wstring, size_t> localStateIndexes;
		for (size_t stateIndex = 0;
			stateIndex < sourceGroup.States.size(); ++stateIndex)
		{
			const auto& state = sourceGroup.States[stateIndex];
			if (state.Name.empty())
				throw std::invalid_argument(
					"Compiled VisualState has an empty token name");
			if (!localStateIndexes.emplace(state.Name, stateIndex).second)
				throw std::invalid_argument(
					"Compiled VisualState name is duplicated in its group");
			const auto token = GeneratedInteractionNameTokenValue(state.Name);
			const auto [entry, inserted] = stateNamesByToken.emplace(token, state.Name);
			if (!inserted && entry->second != state.Name)
				throw std::invalid_argument(
					"Compiled VisualState token collision");
		}

		const EmissionRange states{ stateElements.size(), sourceGroup.States.size() };
		std::optional<size_t> fallbackState;
		std::vector<size_t> groupConditionOperands;
		for (size_t localStateIndex = 0;
			localStateIndex < sourceGroup.States.size(); ++localStateIndex)
		{
			const auto& sourceState = sourceGroup.States[localStateIndex];
			if (sourceState.Conditions.empty()
				&& sourceState.EventNames.empty()
				&& sourceState.Events.empty())
			{
				if (fallbackState)
					throw std::invalid_argument(
						"Compiled VisualStateGroup has multiple fallback states");
				fallbackState = localStateIndex;
			}

			const size_t eventOffset = stateEventElements.size();
			for (const auto& eventName : sourceState.EventNames)
			{
				const auto event = eventResolver
					? eventResolver(eventName)
					: DeclarativeEventReferenceExpression{};
				if (event.Expression.empty() || event.Routed)
					throw std::invalid_argument(
						"Static VisualState event has no component-event identity");
				stateEventElements.push_back(event.Expression);
			}
			const EmissionRange events{ eventOffset,
				stateEventElements.size() - eventOffset };
			const EmissionRange conditions{
				conditionElements.size(), sourceState.Conditions.size() };
			for (const auto& condition : sourceState.Conditions)
			{
				const size_t operandIndex = storyboardTables.AddPropertyOperand(
					{}, condition.Property.Name(), false);
				const size_t valueIndex = addValue(condition.Value);
				conditionElements.push_back("{ "
					+ indexExpression(operandIndex,
						"Compiled interaction condition operand") + ", "
					+ indexExpression(valueIndex,
						"Compiled interaction condition value") + " }");
				if (std::find(groupConditionOperands.begin(),
					groupConditionOperands.end(), operandIndex)
					== groupConditionOperands.end())
					groupConditionOperands.push_back(operandIndex);
			}
			const EmissionRange setters{
				setterElements.size(), sourceState.Setters.size() };
			for (const auto& setter : sourceState.Setters)
			{
				const size_t operandIndex = storyboardTables.AddPropertyOperand(
					setter.TargetName, setter.Property.Name(), true);
				const size_t valueIndex = addValue(setter.Value);
				setterElements.push_back("{ "
					+ indexExpression(operandIndex,
						"Compiled interaction setter operand") + ", "
					+ indexExpression(valueIndex,
						"Compiled interaction setter value") + " }");
			}
			const auto animations = storyboardTables.AppendAnimations(
				sourceState.Animations);
			stateElements.push_back("{ VisualStateToken{ "
				+ std::to_string(GeneratedInteractionNameTokenValue(
					sourceState.Name)) + "ULL }, "
				+ rangeExpression(conditions) + ", "
				+ rangeExpression(events) + ", "
				+ rangeExpression(setters) + ", "
				+ rangeExpression(animations) + " }");
		}
		if (!fallbackState)
			throw std::invalid_argument(
				"Compiled VisualStateGroup has no fallback state");

		const EmissionRange transitions{
			transitionElements.size(), sourceGroup.Transitions.size() };
		std::set<std::pair<size_t, size_t>> transitionSelectors;
		for (const auto& sourceTransition : sourceGroup.Transitions)
		{
			auto resolveState = [&](const std::wstring& name)
				-> std::optional<size_t>
			{
				if (name.empty()) return std::nullopt;
				const auto found = localStateIndexes.find(name);
				if (found == localStateIndexes.end())
					throw std::invalid_argument(
						"Compiled VisualTransition references an unknown state");
				return found->second;
			};
			const auto from = resolveState(sourceTransition.FromState);
			const auto to = resolveState(sourceTransition.ToState);
			const size_t wildcard = (std::numeric_limits<size_t>::max)();
			if (!transitionSelectors.emplace(
				from.value_or(wildcard), to.value_or(wildcard)).second)
				throw std::invalid_argument(
					"Compiled VisualTransition selector is duplicated");
			const auto animations = storyboardTables.AppendAnimations(
				sourceTransition.Animations);
			transitionElements.push_back("{ "
				+ optionalIndexExpression(from,
					"Compiled transition From state") + ", "
				+ optionalIndexExpression(to,
					"Compiled transition To state") + ", "
				+ std::to_string(
					sourceTransition.GeneratedDurationMilliseconds) + "ULL, "
				+ GeneratedEasingExpression(
					sourceTransition.GeneratedEasing) + ", "
				+ GeneratedEasingModeExpression(
					sourceTransition.GeneratedEasingMode) + ", "
				+ rangeExpression(animations) + " }");
		}
		const EmissionRange conditionOperands{
			groupConditionOperandElements.size(), groupConditionOperands.size() };
		for (const auto operand : groupConditionOperands)
			groupConditionOperandElements.push_back(indexExpression(
				operand, "Compiled group condition operand"));
		groupElements.push_back("{ VisualStateGroupToken{ "
			+ std::to_string(groupToken) + "ULL }, "
			+ rangeExpression(states) + ", "
			+ rangeExpression(transitions) + ", "
			+ indexExpression(*fallbackState,
				"Compiled VisualState fallback index") + ", "
			+ rangeExpression(conditionOperands) + " }");
	}

	std::vector<const std::vector<DeclarativeEventTriggerActionDefinition>*>
		eventActionLists;
	eventActionLists.reserve(eventTriggers.size());
	for (const auto& trigger : eventTriggers)
		eventActionLists.push_back(&trigger.Actions);
	const auto eventActionScope = storyboardTables.DeclareActionScope(
		eventActionLists);
	std::vector<std::string> eventTriggerElements;
	for (const auto& sourceTrigger : eventTriggers)
	{
		const auto event = eventResolver
			? eventResolver(sourceTrigger.EventName)
			: DeclarativeEventReferenceExpression{};
		if (event.Expression.empty())
			throw std::invalid_argument(
				"Static EventTrigger has no compiled event identity");
		if (sourceTrigger.Actions.empty())
			throw std::invalid_argument(
				"Compiled EventTrigger has no actions");
		const auto actions = storyboardTables.AppendActions(
			sourceTrigger.Actions, eventActionScope);
		eventTriggerElements.push_back("{ "
			+ std::string(event.Routed ? "nullptr" : event.Expression) + ", "
			+ (event.Routed ? event.Expression
				: std::string("RoutedEventId::None")) + ", "
			+ rangeExpression(actions) + " }");
	}
	const auto storyboardElements = storyboardTables.StoryboardElements();

	code << tabs << "{\n";
	code << inner
		<< "// AOT interaction program: process-static structure plus call-local values and targets.\n";
			auto emitStaticPool = [&] (
		const char* name,
		const char* elementType,
		const std::vector<std::string>& elements,
		bool constexprElements,
		bool processStatic = true)
	{
		if (elements.empty()) return std::string("{}");
		const std::string variable = "__cuiInteraction_" + std::string(name);
		code << inner << (processStatic ? "static " : "")
			<< (constexprElements ? "constexpr " : "const ")
			<< elementType << " " << variable << "[] = {\n";
		for (size_t index = 0; index < elements.size(); ++index)
		{
			code << deep << elements[index];
			if (index + 1 < elements.size()) code << ",";
			code << "\n";
		}
		code << inner << "};\n";
		return "std::span<const " + std::string(elementType)
			+ ">{ " + variable + " }";
	};
	auto emitStateEvents = [&]()
	{
		if (stateEventElements.empty()) return std::string("{}");
		const std::string variable = "__cuiInteraction_state_events";
		code << inner << "static const DeclarativeEventDefinition* const "
			<< variable << "[] = {\n";
		for (size_t index = 0; index < stateEventElements.size(); ++index)
		{
			code << deep << stateEventElements[index];
			if (index + 1 < stateEventElements.size()) code << ",";
			code << "\n";
		}
		code << inner << "};\n";
		return "std::span<const DeclarativeEventDefinition* const>{ "
			+ variable + " }";
	};

	const auto valuesView = emitStaticPool(
		"values", "BindingValue", valueElements, false, false);
	const auto propertyOperandsView = emitStaticPool(
		"property_operands", "CompiledInteractionPropertyOperand",
		storyboardTables.PropertyOperands, false);
	const auto objectPathChildrenView = emitStaticPool(
		"object_path_child_indices", "uint32_t",
		storyboardTables.ObjectPathChildIndices, true);
	const auto objectPathsView = emitStaticPool(
		"object_paths", "CompiledStoryboardObjectPathOp",
		storyboardTables.ObjectPaths, true);
	const auto conditionsView = emitStaticPool(
		"conditions", "CompiledInteractionConditionOp",
		conditionElements, true);
	const auto settersView = emitStaticPool(
		"setters", "CompiledInteractionSetterOp", setterElements, true);
	const auto keyFramesView = emitStaticPool(
		"key_frames", "CompiledInteractionKeyFrameOp",
		storyboardTables.KeyFrames, true);
	const auto animationsView = emitStaticPool(
		"animations", "CompiledInteractionAnimationOp",
		storyboardTables.Animations, true);
	const auto stateEventsView = emitStateEvents();
	const auto statesView = emitStaticPool(
		"states", "CompiledInteractionStateOp", stateElements, true);
	const auto transitionsView = emitStaticPool(
		"transitions", "CompiledInteractionTransitionOp",
		transitionElements, true);
	const auto groupConditionOperandsView = emitStaticPool(
		"group_condition_operands", "uint32_t",
		groupConditionOperandElements, true);
	const auto groupsView = emitStaticPool(
		"groups", "CompiledInteractionGroupOp", groupElements, true);
	const auto storyboardsView = emitStaticPool(
		"storyboards", "CompiledInteractionStoryboardOp",
		storyboardElements, true);
	const auto actionsView = emitStaticPool(
		"actions", "CompiledInteractionActionOp",
		storyboardTables.Actions, true);
	const auto eventTriggersView = emitStaticPool(
		"event_triggers", "CompiledInteractionEventTriggerOp",
		eventTriggerElements, false);

	code << inner
		<< "static const CompiledInteractionProgramView __cuiInteractionProgram{\n";
	code << deep << "CompiledInteractionProgramViewVersion,\n";
	code << deep << indexExpression(storyboardTables.TargetExpressions.size() + 1,
		"Compiled interaction target count") << ",\n";
	const std::pair<const char*, const std::string*> viewFields[] = {
		{ "PropertyOperands", &propertyOperandsView },
		{ "ObjectPathChildIndices", &objectPathChildrenView },
		{ "ObjectPaths", &objectPathsView },
		{ "Conditions", &conditionsView },
		{ "Setters", &settersView },
		{ "KeyFrames", &keyFramesView },
		{ "Animations", &animationsView },
		{ "StateEvents", &stateEventsView },
		{ "States", &statesView },
		{ "Transitions", &transitionsView },
		{ "GroupConditionOperands", &groupConditionOperandsView },
		{ "Groups", &groupsView },
		{ "Storyboards", &storyboardsView },
		{ "Actions", &actionsView },
		{ "EventTriggers", &eventTriggersView },
	};
	for (size_t index = 0; index < std::size(viewFields); ++index)
	{
		const auto& [name, expression] = viewFields[index];
		code << deep << *expression;
		if (index + 1 < std::size(viewFields)) code << ",";
		code << " // " << name << "\n";
	}
	code << inner << "};\n";
	code << inner << "std::array<Control*, "
		<< storyboardTables.TargetExpressions.size() + 1
		<< "> __cuiInteractionTargets{\n";
	code << deep << "&(" << targetExpression << ")";
	if (!storyboardTables.TargetExpressions.empty()) code << ",";
	code << "\n";
	for (size_t index = 0;
		index < storyboardTables.TargetExpressions.size(); ++index)
	{
		code << deep << storyboardTables.TargetExpressions[index];
		if (index + 1 < storyboardTables.TargetExpressions.size()) code << ",";
		code << "\n";
	}
	code << inner << "};\n";
	code << inner << "std::wstring interactionError;\n";
	code << inner
		<< "if (!cui::framework::TemplateAccess::InstallCompiledInteractions("
		<< targetExpression << ", __cuiInteractionProgram, "
		<< valuesView << ", "
			"std::span<Control* const>{ __cuiInteractionTargets }, "
			"&interactionError))\n";
	code << deep << "return fail(L\"ControlTemplate 声明交互安装失败：\" "
		"+ interactionError);\n";
	code << tabs << "}\n";
	return code.str();
}

std::string CodeGenerator::GenerateStyleSheetCode(
	int indent,
	const std::vector<std::pair<std::wstring, std::string>>& objectResources,
	const std::unordered_map<std::wstring, std::string>*
		sharedDocumentResources,
	bool emitEmptyStyleSheet,
	const DesignerStyleSheet* styleSheetOverride,
	const DesignerModel::DesignDocument* styleDocumentOverride,
	const std::string& styleSheetVariable,
	const std::vector<std::pair<std::wstring, std::string>>*
		inlineObjectResources)
{
	const auto& authoredStyleSheet = styleSheetOverride
		? *styleSheetOverride : _styleSheet;
	if (emitEmptyStyleSheet
		&& _outputKind == CodeGeneratorOutputKind::Window)
		throw std::invalid_argument(
			"Only static output can require an empty Style program");
	if (!emitEmptyStyleSheet
		&& authoredStyleSheet.Empty() && objectResources.empty()) return "";
	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	const bool staticOutput =
		_outputKind != CodeGeneratorOutputKind::Window;
	const auto& styleDocument = styleDocumentOverride
		? *styleDocumentOverride : _sourceDocument;
	auto styleSheet = authoredStyleSheet;
	DesignerStyleSheetUtils::Canonicalize(styleSheet);
	DesignerStyleSheet resolvedStyleSheet;
	std::wstring inheritanceError;
	if (!DesignerStyleSheetUtils::ExpandRuntimeRules(
		styleSheet, resolvedStyleSheet, &inheritanceError))
		throw std::invalid_argument(WStringToString(inheritanceError));
	styleSheet = std::move(resolvedStyleSheet);

	if (staticOutput)
	{
		using EmissionRange = GeneratedCompiledRange;
		struct ResourceEmission
		{
			std::wstring Key;
			std::string ValueExpression;
			std::optional<DesignerStyleValue> TypedValue;
		};
		struct TypedValuePoolEmission
		{
			DesignerStyleValueKind Kind = DesignerStyleValueKind::Bool;
			std::string Name;
			std::string ElementType;
			bool ConstexprElements = true;
			std::vector<std::string> Elements;
		};
		struct RuleEmission
		{
			size_t RuleId = 0;
			size_t SourceOrder = 0;
			EmissionRange PropertyConditions;
			EmissionRange DataConditions;
			EmissionRange Setters;
			EmissionRange EnterActions;
			EmissionRange ExitActions;
		};
		struct SelectorIdentity
		{
			bool HasType = false;
			UIClass Type = UIClass::UI_Base;
			std::uint64_t ComponentType = 0;
			std::wstring StyleResourceKey;
		};
		struct GroupEmission
		{
			SelectorIdentity Identity;
			std::optional<size_t> StyleResourceKey;
			EmissionRange RuleIndexes;
			std::vector<std::string> PropertyWatchers;
			std::vector<size_t> DataPathWatchers;
			EmissionRange FlatPropertyWatchers;
			EmissionRange FlatDataPathWatchers;
		};
		struct SetterEmission
		{
			std::string Property;
			std::string Element;
		};
		struct DataPathEmission
		{
			std::wstring AuthoredPath;
			std::string Variable;
			std::vector<std::string> Steps;
		};

		const auto checkedUint32 = [](size_t value, const char* context)
		{
			if (value >= static_cast<size_t>(
				std::numeric_limits<uint32_t>::max()))
				throw std::length_error(std::string(context)
					+ " exceeds the compiled StyleProgram limit");
			return std::to_string(value) + "u";
		};
		const auto rangeExpression = [&](const EmissionRange& range)
		{
			return "{ " + checkedUint32(range.Offset, "Style range offset")
				+ ", " + checkedUint32(range.Count, "Style range count") + " }";
		};
		const auto indexExpression = [&](const std::optional<size_t>& index)
		{
			return index
				? checkedUint32(*index, "Style pool index")
				: std::string("CompiledStyleInvalidIndex");
		};
		auto appendUnique = [](auto& values, const auto& value)
		{
			if (std::find(values.begin(), values.end(), value) == values.end())
				values.push_back(value);
		};
		std::vector<DataPathEmission> dataPaths;
		std::map<std::uint64_t, std::wstring> dataPathNamesByToken;
		auto internDataPath = [&](
			const std::wstring& authoredPath,
			const char* context) -> size_t
		{
			if (const auto found = std::find_if(
					dataPaths.begin(), dataPaths.end(), [&](const auto& candidate)
					{ return candidate.AuthoredPath == authoredPath; });
				found != dataPaths.end())
				return static_cast<size_t>(found - dataPaths.begin());

			std::vector<BindingPathStep> parsed;
			if (!TryParseBindingPropertyPath(authoredPath, parsed)
				|| parsed.empty())
				throw std::invalid_argument(
					std::string(context) + " has an invalid source path");

			DataPathEmission emitted;
			emitted.AuthoredPath = authoredPath;
			emitted.Variable = styleSheetVariable + "_program_data_path_"
				+ std::to_string(dataPaths.size() + 1);
			for (const auto& step : parsed)
			{
				bool listIndex = false;
				std::uint32_t index = 0;
				if (step.Kind == BindingPathStepKind::Indexer
					&& !step.Value.empty()
					&& std::all_of(step.Value.begin(), step.Value.end(),
						[](wchar_t value)
						{ return value >= L'0' && value <= L'9'; }))
				{
					std::uint64_t parsedIndex = 0;
					for (const auto value : step.Value)
					{
						const auto digit = static_cast<std::uint64_t>(
							value - L'0');
						if (parsedIndex
							> ((std::numeric_limits<std::uint32_t>::max)()
								- digit) / 10)
							throw std::length_error(
								std::string(context)
								+ " list index exceeds the AOT limit");
						parsedIndex = parsedIndex * 10 + digit;
					}
					listIndex = true;
					index = static_cast<std::uint32_t>(parsedIndex);
				}

				std::string propertyToken = "{}";
				if (!listIndex)
				{
					const auto token =
						GeneratedBindingSourcePropertyTokenValue(step.Value);
					const auto [existing, inserted] =
						dataPathNamesByToken.emplace(token, step.Value);
					if (!inserted && existing->second != step.Value)
						throw std::invalid_argument(
							"Static Style DataTrigger paths contain a "
							"BindingSourcePropertyToken collision");
					propertyToken = "BindingSourcePropertyToken{ "
						+ std::to_string(token) + "ULL }";
				}
				emitted.Steps.push_back("{ " + std::string(listIndex
						? "CompiledBindingPathStepKind::ListIndex"
						: "CompiledBindingPathStepKind::Property")
					+ ", CompiledBindingPathCapabilities::Read | "
						"CompiledBindingPathCapabilities::Observe, "
					+ (listIndex
						? "BindingValueKind::Object" : "BindingValueKind::Empty")
					+ ", " + propertyToken + ", "
					+ std::to_string(index) + "u }");
			}

			dataPaths.push_back(std::move(emitted));
			return dataPaths.size() - 1;
		};

		std::vector<std::wstring> strings;
		std::unordered_map<std::wstring, size_t> stringIndexes;
		auto internString = [&](const std::wstring& value)
			-> std::optional<size_t>
		{
			if (value.empty()) return std::nullopt;
			if (const auto found = stringIndexes.find(value);
				found != stringIndexes.end()) return found->second;
			const size_t index = strings.size();
			(void)checkedUint32(index, "Style string index");
			strings.push_back(value);
			stringIndexes.emplace(value, index);
			return index;
		};

		std::vector<std::string> values;
		auto addInstanceValue = [&](std::string expression)
		{
			const size_t index = values.size();
			(void)checkedUint32(index, "Style value index");
			values.push_back(std::move(expression));
			return checkedUint32(index, "Style instance value index");
		};

		std::vector<TypedValuePoolEmission> typedValuePools;
		auto typedValuePoolInfo = [](DesignerStyleValueKind kind)
			-> std::optional<std::pair<const char*, const char*>>
		{
			switch (kind)
			{
			case DesignerStyleValueKind::Bool:
				return std::pair{ "bools", "bool" };
			case DesignerStyleValueKind::NullableBool:
				return std::pair{ "nullable_bools", "NullableBool" };
			case DesignerStyleValueKind::Int:
				return std::pair{ "ints", "int" };
			case DesignerStyleValueKind::Int64:
				return std::pair{ "int64s", "long long" };
			case DesignerStyleValueKind::Float:
				return std::pair{ "floats", "float" };
			case DesignerStyleValueKind::Double:
				return std::pair{ "doubles", "double" };
			case DesignerStyleValueKind::String:
				return std::pair{ "string_values", "std::wstring_view" };
			case DesignerStyleValueKind::Color:
				return std::pair{ "colors", "D2D1_COLOR_F" };
			case DesignerStyleValueKind::Thickness:
				return std::pair{ "thicknesses", "Thickness" };
			case DesignerStyleValueKind::CornerRadius:
				return std::pair{ "corner_radii", "::CornerRadius" };
			case DesignerStyleValueKind::Point:
				return std::pair{ "points", "cui::core::Point" };
			case DesignerStyleValueKind::Vector:
				return std::pair{ "vectors", "cui::core::Vector" };
			case DesignerStyleValueKind::Rect:
				return std::pair{ "rects", "cui::core::Rect" };
			case DesignerStyleValueKind::Size:
				return std::pair{ "sizes", "cui::core::Size" };
			case DesignerStyleValueKind::Matrix:
				return std::pair{ "matrices", "D2D1_MATRIX_3X2_F" };
			case DesignerStyleValueKind::Length:
				return std::pair{ "lengths", "cui::layout::Length" };
			default:
				return std::nullopt;
			}
		};
		auto unwrapBindingValue = [](const std::string& expression)
			-> std::optional<std::string>
		{
			constexpr std::string_view prefix = "BindingValue(";
			if (!expression.starts_with(prefix)
				|| expression.size() <= prefix.size()
				|| expression.back() != ')') return std::nullopt;
			return expression.substr(
				prefix.size(), expression.size() - prefix.size() - 1);
		};
		auto addTypedValue = [&](const DesignerStyleValue& value)
			-> std::optional<std::string>
		{
			const auto info = typedValuePoolInfo(value.Kind);
			if (!info) return std::nullopt;
			const auto nativeExpression = unwrapBindingValue(
				GenerateStyleValueExpression(value));
			if (!nativeExpression || nativeExpression->empty())
				return std::nullopt;
			auto pool = std::find_if(
				typedValuePools.begin(), typedValuePools.end(),
				[&](const auto& candidate) { return candidate.Kind == value.Kind; });
			if (pool == typedValuePools.end())
			{
				if (typedValuePools.size()
					>= static_cast<size_t>(CompiledStyleStaticValuePoolLimit))
					throw std::length_error(
						"Style typed value pool count exceeds the compiled limit");
				typedValuePools.push_back({
					value.Kind,
					info->first,
					info->second,
					value.Kind != DesignerStyleValueKind::Matrix,
					{} });
				pool = std::prev(typedValuePools.end());
			}
			const size_t poolIndex = static_cast<size_t>(
				std::distance(typedValuePools.begin(), pool));
			const size_t elementIndex = pool->Elements.size();
			if (elementIndex >= static_cast<size_t>(
				CompiledStyleStaticValueElementLimit))
				throw std::length_error(
					"Style typed value pool exceeds the compiled element limit");
			pool->Elements.push_back(*nativeExpression);
			return "MakeCompiledStyleStaticValueReference("
				+ checkedUint32(poolIndex, "Style typed value pool") + ", "
				+ checkedUint32(elementIndex, "Style typed value element") + ")";
		};
		auto addValue = [&](const DesignerStyleValue* typedValue,
			std::string instanceExpression)
		{
			if (typedValue)
				if (const auto reference = addTypedValue(*typedValue))
					return *reference;
			return addInstanceValue(std::move(instanceExpression));
		};
		const DesignerStyleRule* actionRule = nullptr;
		GeneratedCompiledStoryboardTables styleStoryboardTables{
			[&](const BindingValue& value)
			{
				const size_t index = values.size();
				(void)checkedUint32(index, "Style storyboard value index");
				values.push_back(GenerateBindingValueExpression(value));
				return index;
			},
			[&](const std::wstring& targetName,
				const std::wstring& propertyName,
				bool requireWritable)
			{
				if (!targetName.empty() || !actionRule) return std::string{};
				const DeclarativePropertyResolver actionPropertyResolver =
					[&](const std::wstring& nestedTarget,
						const std::wstring& nestedProperty,
						bool nestedRequireWritable)
					{
						if (!nestedTarget.empty()) return std::string{};
						return FindStyleDependencyPropertyExpression(
							styleDocument, *actionRule, nestedProperty,
							nestedRequireWritable);
					};
				return GenerateDeclarativePropertyReference(
					{}, propertyName, requireWritable, actionPropertyResolver);
			},
			[](const std::wstring&) { return std::string{}; },
			[&](float value) { return FloatLiteral(value); },
			[&](double value) { return DoubleLiteral(value); }
		};

		std::vector<ResourceEmission> resources;
		auto addResource = [&](const std::wstring& key, std::string expression,
			std::optional<DesignerStyleValue> typedValue = std::nullopt)
		{
			if (key.empty()) return;
			const auto existing = std::find_if(
				resources.begin(), resources.end(), [&](const auto& resource)
				{ return resource.Key == key; });
			if (existing == resources.end())
				resources.push_back({
					key, std::move(expression), std::move(typedValue) });
			else
			{
				existing->ValueExpression = std::move(expression);
				existing->TypedValue = std::move(typedValue);
			}
		};
		for (const auto& resource : styleSheet.Resources)
		{
			std::string expression;
			bool usesSharedExpression = false;
			if (sharedDocumentResources)
				if (const auto sharedResource =
						sharedDocumentResources->find(resource.Key);
					sharedResource != sharedDocumentResources->end())
				{
					expression = sharedResource->second;
					usesSharedExpression = true;
				}
			if (expression.empty())
				expression = GenerateStyleValueExpression(resource.Value);
			addResource(resource.Key, std::move(expression),
				usesSharedExpression ? std::nullopt
					: std::optional<DesignerStyleValue>(resource.Value));
		}
		for (const auto& [key, expression] : objectResources)
			addResource(key, expression);

		std::vector<std::string> resourceElements;
		resourceElements.reserve(resources.size());
		for (const auto& resource : resources)
		{
			const auto keyIndex = internString(resource.Key);
			const auto valueIndex = addValue(
				resource.TypedValue ? &*resource.TypedValue : nullptr,
				resource.ValueExpression);
			resourceElements.push_back("{ " + indexExpression(keyIndex)
				+ ", " + valueIndex
				+ " }");
		}
		std::vector<size_t> resourceLookup(resources.size());
		for (size_t index = 0; index < resourceLookup.size(); ++index)
			resourceLookup[index] = index;
		std::sort(resourceLookup.begin(), resourceLookup.end(),
			[&](size_t left, size_t right)
			{ return resources[left].Key < resources[right].Key; });

		std::vector<std::string> propertyConditionElements;
		std::vector<std::string> dataConditionElements;
		std::vector<std::string> setterElements;
		std::vector<RuleEmission> rules;
		std::vector<size_t> ruleIndexes;
		std::vector<GroupEmission> groups;
		std::vector<std::string> globalPropertyWatchers;
		std::vector<size_t> globalDataPathWatchers;
		auto sameSelectorIdentity = [](const SelectorIdentity& left,
			const SelectorIdentity& right)
		{
			return left.HasType == right.HasType
				&& left.Type == right.Type
				&& left.ComponentType == right.ComponentType
				&& left.StyleResourceKey == right.StyleResourceKey;
		};

		for (const auto& rule : styleSheet.Rules)
		{
			actionRule = &rule;
			const DeclarativePropertyResolver stylePropertyResolver =
				[&](const std::wstring& targetName,
					const std::wstring& propertyName,
					bool requireWritable)
				{
					if (!targetName.empty()) return std::string{};
					return FindStyleDependencyPropertyExpression(
						styleDocument, rule, propertyName, requireWritable);
				};

			std::vector<DeclarativeEventTriggerActionDefinition> enterActions;
			std::vector<DeclarativeEventTriggerActionDefinition> exitActions;
			std::wstring actionError;
			if (!DesignerStyleSheetUtils::MaterializeStoryboardActions(
				rule.EnterActions, styleSheet, enterActions, &actionError,
				styleDocument.ResourceBasePath, styleDocument.Resources,
				L"Style Trigger.EnterActions")
				|| !DesignerStyleSheetUtils::MaterializeStoryboardActions(
					rule.ExitActions, styleSheet, exitActions, &actionError,
					styleDocument.ResourceBasePath, styleDocument.Resources,
					L"Style Trigger.ExitActions"))
				throw std::invalid_argument(WStringToString(actionError));

			std::vector<SetterEmission> normalizedSetters;
			for (const auto& setter : rule.Setters)
			{
				if (setter.PropertyName.empty()) continue;
				const auto property = GenerateDeclarativePropertyReference(
					{}, setter.PropertyName, true, stylePropertyResolver);
				std::string operand;
				const std::string* objectExpression = nullptr;
				if (inlineObjectResources
					&& (setter.PropertyName == L"Template"
						|| setter.PropertyName == L"ItemsPanel")
					&& setter.UsesResource
					&& !setter.UsesDynamicResource)
				{
					const auto objectResource = std::find_if(
						inlineObjectResources->begin(),
						inlineObjectResources->end(),
						[&](const auto& resource)
						{ return resource.first == setter.ResourceKey; });
					if (objectResource != inlineObjectResources->end())
						objectExpression = &objectResource->second;
				}
				if (objectExpression)
				{
					const auto valueIndex = addValue(nullptr, *objectExpression);
					operand = "{ CompiledStyleOperandKind::Literal, "
						+ valueIndex + " }";
				}
				else if (setter.UsesResource)
				{
					const auto resourceKey = internString(setter.ResourceKey);
					operand = "{ ";
					operand += setter.UsesDynamicResource
						? "CompiledStyleOperandKind::DynamicResource"
						: "CompiledStyleOperandKind::StaticResource";
					operand += ", " + indexExpression(resourceKey) + " }";
				}
				else
				{
					const auto valueIndex = addValue(
						&setter.Literal,
						GenerateStyleValueExpression(setter.Literal));
					operand = "{ CompiledStyleOperandKind::Literal, "
						+ valueIndex + " }";
				}
				SetterEmission emitted{
					property, "{ " + property + ", " + operand + " }" };
				const auto existing = std::find_if(
					normalizedSetters.begin(), normalizedSetters.end(),
					[&](const auto& candidate)
					{ return candidate.Property == property; });
				if (existing == normalizedSetters.end())
					normalizedSetters.push_back(std::move(emitted));
				else
					*existing = std::move(emitted);
			}
			if (normalizedSetters.empty()
				&& enterActions.empty() && exitActions.empty()) continue;

			SelectorIdentity identity;
			identity.HasType = rule.HasType;
			identity.Type = rule.Type;
			if (!rule.ComponentType.Empty())
				identity.ComponentType = GeneratedComponentTypeTokenValue(
					rule.ComponentType.XamlNamespace,
					rule.ComponentType.XamlName);
			else if (!rule.HasType && rule.XamlType.Valid())
				identity.ComponentType = GeneratedComponentTypeTokenValue(
					rule.XamlType.NamespaceUri, rule.XamlType.LocalName);
			identity.StyleResourceKey = rule.Id;

			if (groups.empty()
				|| !sameSelectorIdentity(groups.back().Identity, identity))
			{
				GroupEmission group;
				group.Identity = identity;
				group.StyleResourceKey = internString(identity.StyleResourceKey);
				group.RuleIndexes.Offset = ruleIndexes.size();
				groups.push_back(std::move(group));
			}
			auto& group = groups.back();

			RuleEmission emittedRule;
			emittedRule.RuleId = rules.size() + 1;
			emittedRule.SourceOrder = rules.size();
			emittedRule.PropertyConditions.Offset =
				propertyConditionElements.size();
			for (const auto& condition : rule.PropertyConditions)
			{
				const auto property = GenerateDeclarativePropertyReference(
					{}, condition.Property, false, stylePropertyResolver);
				const auto valueIndex = addValue(
					&condition.Value,
					GenerateStyleValueExpression(condition.Value));
				propertyConditionElements.push_back("{ " + property + ", "
					+ valueIndex + " }");
				appendUnique(group.PropertyWatchers, property);
				appendUnique(globalPropertyWatchers, property);
			}
			emittedRule.PropertyConditions.Count =
				propertyConditionElements.size()
				- emittedRule.PropertyConditions.Offset;

			emittedRule.DataConditions.Offset = dataConditionElements.size();
			for (const auto& condition : rule.DataConditions)
			{
				const auto pathIndex = internDataPath(
					condition.SourceProperty, "Static Style DataTrigger");
				const auto valueIndex = addValue(
					&condition.Value,
					GenerateStyleValueExpression(condition.Value));
				dataConditionElements.push_back("{ "
					+ checkedUint32(pathIndex, "Style data path")
					+ ", " + valueIndex + " }");
				appendUnique(group.DataPathWatchers, pathIndex);
				appendUnique(globalDataPathWatchers, pathIndex);
			}
			emittedRule.DataConditions.Count = dataConditionElements.size()
				- emittedRule.DataConditions.Offset;

			emittedRule.Setters.Offset = setterElements.size();
			for (auto& setter : normalizedSetters)
				setterElements.push_back(std::move(setter.Element));
			emittedRule.Setters.Count = setterElements.size()
				- emittedRule.Setters.Offset;

			const auto actionScope = styleStoryboardTables.DeclareActionScope(
				{ &enterActions, &exitActions });
			emittedRule.EnterActions = styleStoryboardTables.AppendActions(
				enterActions, actionScope);
			emittedRule.ExitActions = styleStoryboardTables.AppendActions(
				exitActions, actionScope);

			const size_t ruleIndex = rules.size();
			rules.push_back(std::move(emittedRule));
			ruleIndexes.push_back(ruleIndex);
			++group.RuleIndexes.Count;
		}

		std::vector<std::string> propertyWatcherElements;
		std::vector<size_t> dataPathWatcherElements;
		for (auto& group : groups)
		{
			group.FlatPropertyWatchers = {
				propertyWatcherElements.size(), group.PropertyWatchers.size() };
			propertyWatcherElements.insert(
				propertyWatcherElements.end(),
				group.PropertyWatchers.begin(), group.PropertyWatchers.end());
			group.FlatDataPathWatchers = {
				dataPathWatcherElements.size(), group.DataPathWatchers.size() };
			dataPathWatcherElements.insert(
				dataPathWatcherElements.end(),
				group.DataPathWatchers.begin(), group.DataPathWatchers.end());
		}

		std::vector<std::string> stringElements;
		stringElements.reserve(strings.size());
		for (const auto& value : strings)
			stringElements.push_back("L\"" + EscapeWStringLiteral(value) + "\"");
		std::vector<std::string> resourceLookupElements;
		resourceLookupElements.reserve(resourceLookup.size());
		for (const size_t index : resourceLookup)
			resourceLookupElements.push_back(
				checkedUint32(index, "Style resource lookup"));
		std::vector<std::string> ruleElements;
		ruleElements.reserve(rules.size());
		for (const auto& rule : rules)
			ruleElements.push_back("{ "
				+ checkedUint32(rule.RuleId, "Style rule id") + ", "
				+ checkedUint32(rule.SourceOrder, "Style source order") + ", "
				+ rangeExpression(rule.PropertyConditions) + ", "
				+ rangeExpression(rule.DataConditions) + ", "
				+ rangeExpression(rule.Setters) + ", "
				+ rangeExpression(rule.EnterActions) + ", "
				+ rangeExpression(rule.ExitActions) + " }");
		std::vector<std::string> ruleIndexElements;
		ruleIndexElements.reserve(ruleIndexes.size());
		for (const size_t index : ruleIndexes)
			ruleIndexElements.push_back(checkedUint32(index, "Style rule index"));
		std::vector<std::string> dataPathWatcherIndexElements;
		dataPathWatcherIndexElements.reserve(dataPathWatcherElements.size());
		for (const size_t index : dataPathWatcherElements)
			dataPathWatcherIndexElements.push_back(
				checkedUint32(index, "Style data watcher"));
		std::vector<std::string> groupElements;
		groupElements.reserve(groups.size());
		for (const auto& group : groups)
			groupElements.push_back("{ "
				+ std::string(group.Identity.HasType ? "true" : "false")
				+ ", static_cast<UIClass>("
				+ std::to_string(static_cast<int>(group.Identity.Type)) + "), "
				+ "ComponentTypeToken{ "
				+ std::to_string(group.Identity.ComponentType) + "ULL }, "
				+ indexExpression(group.StyleResourceKey) + ", "
				+ rangeExpression(group.RuleIndexes) + ", "
				+ rangeExpression(group.FlatPropertyWatchers) + ", "
				+ rangeExpression(group.FlatDataPathWatchers) + " }");
		std::vector<std::string> globalDataPathWatcherElements;
		globalDataPathWatcherElements.reserve(globalDataPathWatchers.size());
		for (const size_t index : globalDataPathWatchers)
			globalDataPathWatcherElements.push_back(
				checkedUint32(index, "Style global data watcher"));
		const auto styleStoryboardElements =
			styleStoryboardTables.StoryboardElements();
		if (!styleStoryboardTables.TargetExpressions.empty())
			throw std::logic_error(
				"Compiled Style storyboard emitted a named target slot");

		code << indentStr
			<< "// AOT Style 程序：生成期完成分组、索引和连续池布局\n";
		auto emitStaticPool = [&] (
			const char* name,
			const char* elementType,
			const std::vector<std::string>& elements,
			bool constexprElements = false)
		{
			if (elements.empty()) return std::string("{}");
			const std::string variable = styleSheetVariable
				+ "_program_" + name;
			code << indentStr << "static "
				<< (constexprElements ? "constexpr " : "const ")
				<< elementType << " " << variable << "[] = {\n";
			for (size_t index = 0; index < elements.size(); ++index)
			{
				code << indentStr << "\t" << elements[index];
				if (index + 1 < elements.size()) code << ",";
				code << "\n";
			}
			code << indentStr << "};\n";
			return "std::span<const " + std::string(elementType)
				+ ">{ " + variable + " }";
		};

		std::vector<std::string> dataPathViewElements;
		dataPathViewElements.reserve(dataPaths.size());
		for (const auto& path : dataPaths)
		{
			code << indentStr << "static constexpr CompiledBindingPathStep "
				<< path.Variable << "[] = {\n";
			for (size_t index = 0; index < path.Steps.size(); ++index)
			{
				code << indentStr << "\t" << path.Steps[index];
				if (index + 1 < path.Steps.size()) code << ",";
				code << "\n";
			}
			code << indentStr << "};\n";
			dataPathViewElements.push_back(
				"CompiledBindingPathView{ " + path.Variable + " }");
		}
		const auto dataPathsView = emitStaticPool(
			"data_paths", "CompiledBindingPathView",
			dataPathViewElements, true);

		// Scalar/POD literals and all Style storyboard structure are
		// process-lifetime typed arrays. Only complex and instance-bound values
		// remain in the sheet-local BindingValue sidecar.
		std::vector<std::string> typedValuePoolDescriptors;
		typedValuePoolDescriptors.reserve(typedValuePools.size());
		for (const auto& pool : typedValuePools)
		{
			const std::string variable = styleSheetVariable
				+ "_program_values_" + pool.Name;
			code << indentStr << "static "
				<< (pool.ConstexprElements ? "constexpr " : "const ")
				<< pool.ElementType
				<< " " << variable << "[] = {\n";
			for (size_t index = 0; index < pool.Elements.size(); ++index)
			{
				code << indentStr << "\t" << pool.Elements[index];
				if (index + 1 < pool.Elements.size()) code << ",";
				code << "\n";
			}
			code << indentStr << "};\n";
			typedValuePoolDescriptors.push_back(
				"MakeCompiledStyleValuePoolView(" + variable + ")");
		}

		const auto stringsView = emitStaticPool(
			"strings", "std::wstring_view", stringElements, true);
		const auto typedValuePoolsView = emitStaticPool(
			"value_pools", "CompiledStyleValuePoolView",
			typedValuePoolDescriptors, true);
		const auto resourcesView = emitStaticPool(
			"resources", "CompiledStyleResourceOp", resourceElements);
		const auto resourceLookupView = emitStaticPool(
			"resource_lookup", "uint32_t", resourceLookupElements, true);
		const auto propertyConditionsView = emitStaticPool(
			"property_conditions", "CompiledStylePropertyConditionOp",
			propertyConditionElements);
		const auto dataConditionsView = emitStaticPool(
			"data_conditions", "CompiledStyleDataConditionOp",
			dataConditionElements, true);
		const auto settersView = emitStaticPool(
			"setters", "CompiledStyleSetterOp", setterElements);
		const auto propertyOperandsView = emitStaticPool(
			"property_operands", "CompiledInteractionPropertyOperand",
			styleStoryboardTables.PropertyOperands);
		const auto objectPathChildIndicesView = emitStaticPool(
			"object_path_child_indices", "uint32_t",
			styleStoryboardTables.ObjectPathChildIndices, true);
		const auto objectPathsView = emitStaticPool(
			"object_paths", "CompiledStoryboardObjectPathOp",
			styleStoryboardTables.ObjectPaths, true);
		const auto keyFramesView = emitStaticPool(
			"key_frames", "CompiledInteractionKeyFrameOp",
			styleStoryboardTables.KeyFrames, true);
		const auto animationsView = emitStaticPool(
			"animations", "CompiledInteractionAnimationOp",
			styleStoryboardTables.Animations, true);
		const auto storyboardsView = emitStaticPool(
			"storyboards", "CompiledInteractionStoryboardOp",
			styleStoryboardElements, true);
		const auto actionsView = emitStaticPool(
			"actions", "CompiledInteractionActionOp",
			styleStoryboardTables.Actions, true);
		const auto rulesView = emitStaticPool(
			"rules", "CompiledStyleRuleOp", ruleElements, true);
		const auto ruleIndexesView = emitStaticPool(
			"rule_indexes", "uint32_t", ruleIndexElements, true);
		const auto propertyWatchersView = emitStaticPool(
			"property_watchers", "DependencyPropertyReference",
			propertyWatcherElements);
		const auto dataPathWatchersView = emitStaticPool(
			"data_path_watchers", "uint32_t",
			dataPathWatcherIndexElements, true);
		const auto groupsView = emitStaticPool(
			"groups", "CompiledStyleGroupOp", groupElements, true);
		const auto globalPropertyWatchersView = emitStaticPool(
			"global_property_watchers", "DependencyPropertyReference",
			globalPropertyWatchers);
		const auto globalDataPathWatchersView = emitStaticPool(
			"global_data_path_watchers", "uint32_t",
			globalDataPathWatcherElements, true);

		code << indentStr << "auto " << styleSheetVariable
			<< " = ControlStyleSheet::CreateCompiled(\n";
		code << indentStr << "\tCompiledStyleProgramView{\n";
		code << indentStr << "\t\tCompiledStyleProgramViewVersion,\n";
		const std::pair<const char*, const std::string*> viewFields[] = {
			{ "Strings", &stringsView },
			{ "ValuePools", &typedValuePoolsView },
			{ "Resources", &resourcesView },
			{ "ResourceLookup", &resourceLookupView },
			{ "PropertyConditions", &propertyConditionsView },
			{ "DataConditions", &dataConditionsView },
			{ "Setters", &settersView },
			{ "PropertyOperands", &propertyOperandsView },
			{ "ObjectPathChildIndices", &objectPathChildIndicesView },
			{ "ObjectPaths", &objectPathsView },
			{ "KeyFrames", &keyFramesView },
			{ "Animations", &animationsView },
			{ "Storyboards", &storyboardsView },
			{ "Actions", &actionsView },
			{ "Rules", &rulesView },
			{ "RuleIndexes", &ruleIndexesView },
			{ "PropertyWatchers", &propertyWatchersView },
			{ "DataPathWatchers", &dataPathWatchersView },
			{ "Groups", &groupsView },
			{ "GlobalPropertyWatchers", &globalPropertyWatchersView },
			{ "GlobalDataPathWatchers", &globalDataPathWatchersView },
			{ "DataPaths", &dataPathsView },
		};
		for (const auto& [name, expression] : viewFields)
			code << indentStr << "\t\t" << *expression
				<< ", // " << name << "\n";
		code << indentStr << "\t},\n";
		code << indentStr << "\tstd::vector<BindingValue>";
		if (values.empty())
			code << "{}\n";
		else
		{
			code << "{\n";
			for (size_t index = 0; index < values.size(); ++index)
			{
				code << indentStr << "\t\t" << values[index];
				if (index + 1 < values.size()) code << ",";
				code << "\n";
			}
			code << indentStr << "\t}\n";
		}
		code << indentStr << ");\n";
		code << "\n";
		return code.str();
	}

	code << indentStr << "// 文档级控件样式\n";
	code << indentStr
		<< "auto __styleSheet = std::make_shared<ControlStyleSheet>();\n";
	for (const auto& resource : styleSheet.Resources)
	{
		const auto shared = sharedDocumentResources
			? sharedDocumentResources->find(resource.Key)
			: std::unordered_map<std::wstring, std::string>::const_iterator{};
		code << indentStr << "__styleSheet->SetResource(L\""
			<< EscapeWStringLiteral(resource.Key) << "\", "
			<< (sharedDocumentResources
				&& shared != sharedDocumentResources->end()
					? shared->second
					: GenerateStyleValueExpression(resource.Value))
			<< ");\n";
	}
	for (const auto& [key, expression] : objectResources)
		code << indentStr << "__styleSheet->SetResource(L\""
			<< EscapeWStringLiteral(key) << "\", " << expression << ");\n";
	for (size_t index = 0; index < styleSheet.Rules.size(); ++index)
	{
		const auto& rule = styleSheet.Rules[index];
		const DeclarativePropertyResolver stylePropertyResolver =
			[&](const std::wstring& targetName,
				const std::wstring& propertyName,
				bool requireWritable)
			{
				if (!targetName.empty()) return std::string{};
					return FindStyleDependencyPropertyExpression(
						styleDocument, rule, propertyName, requireWritable);
			};
		const auto selectorName = "__styleSelector" + std::to_string(index + 1);
		code << indentStr << "ControlStyleSelector " << selectorName << ";\n";
		if (rule.HasType)
		{
			code << indentStr << selectorName
				<< ".Type = static_cast<UIClass>("
				<< static_cast<int>(rule.Type) << ");\n";
		}
		if (!rule.ComponentType.Empty())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlNamespace)
				<< "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlName) << "\";\n";
		}
		else if (!rule.HasType && rule.XamlType.Valid())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.XamlType.NamespaceUri) << "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.XamlType.LocalName) << "\";\n";
		}
		if (!rule.Id.empty())
			code << indentStr << selectorName << ".StyleResourceKey = L\""
				<< EscapeWStringLiteral(rule.Id) << "\";\n";
		for (const auto& condition : rule.PropertyConditions)
		{
			code << indentStr << selectorName
				<< ".PropertyConditions.push_back({ ";
			if (staticOutput)
				code << stylePropertyResolver(
					{}, condition.Property, false);
			else
				code << "L\"" << EscapeWStringLiteral(
					condition.Property) << "\"";
			code << ", " << GenerateStyleValueExpression(condition.Value)
				<< " });\n";
		}
		for (const auto& condition : rule.DataConditions)
			code << indentStr << selectorName
				<< ".DataConditions.push_back({ L\""
				<< EscapeWStringLiteral(condition.SourceProperty) << "\", "
				<< GenerateStyleValueExpression(condition.Value) << " });\n";
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions;
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions;
		std::wstring actionError;
		if (!DesignerStyleSheetUtils::MaterializeStoryboardActions(
			rule.EnterActions, styleSheet, enterActions, &actionError,
			styleDocument.ResourceBasePath, styleDocument.Resources,
			L"Style Trigger.EnterActions")
			|| !DesignerStyleSheetUtils::MaterializeStoryboardActions(
				rule.ExitActions, styleSheet, exitActions, &actionError,
				styleDocument.ResourceBasePath, styleDocument.Resources,
				L"Style Trigger.ExitActions"))
			throw std::invalid_argument(WStringToString(actionError));
		const auto enterActionsName =
			"__styleEnterActions" + std::to_string(index + 1);
		const auto exitActionsName =
			"__styleExitActions" + std::to_string(index + 1);
		if (!enterActions.empty() || !exitActions.empty())
		{
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< enterActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				enterActions, enterActionsName,
				stylePropertyResolver, DeclarativeTargetResolver{}, indent);
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< exitActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				exitActions, exitActionsName,
				stylePropertyResolver, DeclarativeTargetResolver{}, indent);
		}
		code << indentStr << "__styleSheet->AddRule(std::move("
			<< selectorName << "), {\n";
		std::vector<const DesignerStyleSetter*> emittedSetters;
		for (const auto& setter : rule.Setters)
			emittedSetters.push_back(&setter);
		for (size_t setterIndex = 0;
			setterIndex < emittedSetters.size(); ++setterIndex)
		{
			const auto& setter = *emittedSetters[setterIndex];
			code << indentStr << "\t";
			const auto propertyExpression = staticOutput
				? FindStyleDependencyPropertyExpression(
					styleDocument, rule, setter.PropertyName, true)
				: std::string{};
			if (setter.UsesResource)
			{
				code << (setter.UsesDynamicResource
					? "ControlStyleSetter::DynamicResource("
					: "ControlStyleSetter::Resource(");
				if (staticOutput)
					code << propertyExpression;
				else
					code << "L\"" << EscapeWStringLiteral(
						setter.PropertyName) << "\"";
				code << ", L\"" << EscapeWStringLiteral(setter.ResourceKey)
					<< "\")";
			}
			else
			{
				code << "ControlStyleSetter(";
				if (staticOutput)
					code << propertyExpression;
				else
					code << "L\"" << EscapeWStringLiteral(
						setter.PropertyName) << "\"";
				code << ", " << GenerateStyleValueExpression(setter.Literal)
					<< ")";
			}
			if (setterIndex + 1 < emittedSetters.size()) code << ",";
			code << "\n";
		}
		code << indentStr << "}";
		if (!enterActions.empty() || !exitActions.empty())
			code << ", std::move(" << enterActionsName
				<< "), std::move(" << exitActionsName << ")";
		code << ");\n";
	}
	code << "\n";
	return code.str();
}

std::string CodeGenerator::GenerateLocalResources(
	const DesignerModel::DesignNode& node,
	int indent,
	const DesignerModel::DesignDocument* sourceDocument,
	const std::vector<std::pair<std::wstring, std::string>>*
		visibleObjectResources,
	const std::vector<std::pair<std::wstring, std::string>>*
		ownedObjectResources,
	bool returnViaFail)
{
	if (node.LocalResources.Empty()
		&& (!ownedObjectResources || ownedObjectResources->empty())) return {};
	const auto& document =
		sourceDocument ? *sourceDocument : _sourceDocument;
	const bool staticOutput =
		_outputKind != CodeGeneratorOutputKind::Window;
	const std::string indentStr(indent, '\t');
	const std::string controlName = GetVarName(node);
	const std::string dictionaryName = "__resources_" + controlName;
	DesignerStyleSheet visible = document.StyleSheet;
	std::vector<const DesignerModel::DesignNode*> route;
	for (auto* scope = &node; scope;)
	{
		route.push_back(scope);
		auto found = document.Nodes.end();
		if (scope->ParentId > 0)
			found = std::find_if(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Id == scope->ParentId; });
		else if (!scope->ParentRef.empty())
			found = std::find_if(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Name == scope->ParentRef; });
		scope = found == document.Nodes.end() ? nullptr : &*found;
	}
	for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
		DesignerStyleSheetUtils::AppendLexicalScope(
			visible, (*scope)->LocalResources);
	std::wstring styleError;
	DesignerStyleSheet local;
	if (!DesignerStyleSheetUtils::PrepareLocalRuntimeStyleSheet(
		node.LocalResources, visible, local, &styleError))
		throw std::invalid_argument(WStringToString(styleError));
	std::ostringstream code;
	if (staticOutput)
	{
		code << indentStr << "// 控件级词法资源作用域\n";
		static const std::vector<std::pair<std::wstring, std::string>>
			emptyObjectResources;
		code << GenerateStyleSheetCode(
			indent,
			ownedObjectResources ? *ownedObjectResources : emptyObjectResources,
			nullptr, true, &local, &document, dictionaryName,
			visibleObjectResources);
		code << indentStr << "if (!cui::framework::StyleAccess::SetResources(*"
			<< controlName << ", " << dictionaryName << "))\n";
		if (returnViaFail)
			code << indentStr << "\treturn fail("
				"L\"ControlTemplate 局部 Resources 安装失败。\");\n";
		else
			code << indentStr << "\tthrow std::runtime_error("
				"\"Generated local Resources installation failed\");\n";
		return code.str();
	}
	DesignerStyleSheet expanded;
	if (!DesignerStyleSheetUtils::ExpandRuntimeRules(
		local, expanded, &styleError))
		throw std::invalid_argument(WStringToString(styleError));
	local = std::move(expanded);
	code << indentStr << "// 控件级词法资源作用域\n";
	code << indentStr << "auto " << dictionaryName
		<< " = std::make_shared<ControlStyleSheet>();\n";
	for (const auto& resource : local.Resources)
		code << indentStr << dictionaryName << "->SetResource(L\""
			<< EscapeWStringLiteral(resource.Key) << "\", "
			<< GenerateStyleValueExpression(resource.Value) << ");\n";
	for (size_t index = 0; index < local.Rules.size(); ++index)
	{
		const auto& rule = local.Rules[index];
		const DeclarativePropertyResolver stylePropertyResolver =
			[&](const std::wstring& targetName,
				const std::wstring& propertyName,
				bool requireWritable)
			{
				if (!targetName.empty()) return std::string{};
				return FindStyleDependencyPropertyExpression(
					document, rule, propertyName, requireWritable);
			};
		const auto selectorName = dictionaryName + "_selector_"
			+ std::to_string(index + 1);
		code << indentStr << "ControlStyleSelector " << selectorName << ";\n";
		if (rule.HasType)
			code << indentStr << selectorName
				<< ".Type = static_cast<UIClass>("
				<< static_cast<int>(rule.Type) << ");\n";
		if (!rule.ComponentType.Empty())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlNamespace)
				<< "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlName) << "\";\n";
		}
		else if (!rule.HasType && rule.XamlType.Valid())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.XamlType.NamespaceUri) << "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.XamlType.LocalName) << "\";\n";
		}
		if (!rule.Id.empty())
			code << indentStr << selectorName << ".StyleResourceKey = L\""
				<< EscapeWStringLiteral(rule.Id) << "\";\n";
		for (const auto& condition : rule.PropertyConditions)
		{
			code << indentStr << selectorName
				<< ".PropertyConditions.push_back({ ";
			if (staticOutput)
				code << stylePropertyResolver(
					{}, condition.Property, false);
			else
				code << "L\"" << EscapeWStringLiteral(
					condition.Property) << "\"";
			code << ", " << GenerateStyleValueExpression(condition.Value)
				<< " });\n";
		}
		for (const auto& condition : rule.DataConditions)
			code << indentStr << selectorName
				<< ".DataConditions.push_back({ L\""
				<< EscapeWStringLiteral(condition.SourceProperty) << "\", "
				<< GenerateStyleValueExpression(condition.Value) << " });\n";
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions;
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions;
		std::wstring actionError;
		if (!DesignerStyleSheetUtils::MaterializeStoryboardActions(
			rule.EnterActions, local, enterActions, &actionError,
			document.ResourceBasePath, document.Resources,
			L"Local Style Trigger.EnterActions")
			|| !DesignerStyleSheetUtils::MaterializeStoryboardActions(
				rule.ExitActions, local, exitActions, &actionError,
				document.ResourceBasePath, document.Resources,
				L"Local Style Trigger.ExitActions"))
			throw std::invalid_argument(WStringToString(actionError));
		const auto enterActionsName = dictionaryName + "_enterActions_"
			+ std::to_string(index + 1);
		const auto exitActionsName = dictionaryName + "_exitActions_"
			+ std::to_string(index + 1);
		if (!enterActions.empty() || !exitActions.empty())
		{
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< enterActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				enterActions, enterActionsName,
				stylePropertyResolver, DeclarativeTargetResolver{}, indent);
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< exitActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				exitActions, exitActionsName,
				stylePropertyResolver, DeclarativeTargetResolver{}, indent);
		}
		code << indentStr << dictionaryName << "->AddRule(std::move("
			<< selectorName << "), {\n";
		std::vector<const DesignerStyleSetter*> emittedSetters;
		for (const auto& setter : rule.Setters)
			emittedSetters.push_back(&setter);
		for (size_t setterIndex = 0;
			setterIndex < emittedSetters.size(); ++setterIndex)
		{
			const auto& setter = *emittedSetters[setterIndex];
			code << indentStr << "\t";
			const auto propertyExpression = staticOutput
				? FindStyleDependencyPropertyExpression(
					document, rule, setter.PropertyName, true)
				: std::string{};
			const std::string* objectExpression = nullptr;
			if (visibleObjectResources
				&& setter.PropertyName == L"Template"
				&& setter.UsesResource
				&& !setter.UsesDynamicResource)
			{
				const auto objectResource = std::find_if(
					visibleObjectResources->begin(),
					visibleObjectResources->end(),
					[&](const auto& resource)
					{ return resource.first == setter.ResourceKey; });
				if (objectResource != visibleObjectResources->end())
					objectExpression = &objectResource->second;
			}
			if (objectExpression)
			{
				code << "ControlStyleSetter(";
				if (staticOutput)
					code << propertyExpression;
				else
					code << "L\"Template\"";
				code << ", " << *objectExpression << ")";
			}
			else if (setter.UsesResource)
			{
				code << (setter.UsesDynamicResource
					? "ControlStyleSetter::DynamicResource("
					: "ControlStyleSetter::Resource(");
				if (staticOutput)
					code << propertyExpression;
				else
					code << "L\"" << EscapeWStringLiteral(
						setter.PropertyName) << "\"";
				code << ", L\"" << EscapeWStringLiteral(setter.ResourceKey)
					<< "\")";
			}
			else
			{
				code << "ControlStyleSetter(";
				if (staticOutput)
					code << propertyExpression;
				else
					code << "L\"" << EscapeWStringLiteral(
						setter.PropertyName) << "\"";
				code << ", " << GenerateStyleValueExpression(setter.Literal)
					<< ")";
			}
			if (setterIndex + 1 < emittedSetters.size()) code << ",";
			code << "\n";
		}
		code << indentStr << "}";
		if (!enterActions.empty() || !exitActions.empty())
			code << ", std::move(" << enterActionsName
				<< "), std::move(" << exitActionsName << ")";
		code << ");\n";
	}
	code << indentStr << "if (!cui::framework::StyleAccess::SetResources(*"
		<< controlName << ", " << dictionaryName << "))\n";
	if (returnViaFail)
		code << indentStr << "\treturn fail("
			"L\"ControlTemplate 局部 Resources 安装失败。\");\n";
	else
		code << indentStr << "\tthrow std::runtime_error("
			"\"Generated local Resources installation failed\");\n";
	return code.str();
}

std::string CodeGenerator::GenerateContainerProperties(
	const DesignerModel::DesignNode& node, int indent)
{
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(node);

	if (node.Type == UIClass::UI_Grid)
	{
		const auto& rows = node.Structure.GridRows;
		const auto& columns = node.Structure.GridColumns;
		if (rows || columns)
		{
			code << indentStr << name << "->ClearRows();\n";
			code << indentStr << name << "->ClearColumns();\n";
			if (rows)
				for (const auto& row : *rows)
					code << indentStr << name << "->AddRow("
						<< GridLengthToCtorString(row.Length) << ", "
						<< FloatLiteral(static_cast<float>(row.Minimum)) << ", "
						<< FloatLiteral(static_cast<float>(row.Maximum))
						<< ");\n";
			if (columns)
				for (const auto& column : *columns)
					code << indentStr << name << "->AddColumn("
						<< GridLengthToCtorString(column.Length) << ", "
						<< FloatLiteral(static_cast<float>(column.Minimum)) << ", "
						<< FloatLiteral(static_cast<float>(column.Maximum))
						<< ");\n";
		}
	}

	if (node.Type == UIClass::UI_RichTextBox
		&& node.Structure.Document)
	{
		auto colorExpression = [&](const DesignerModel::DesignColor& color)
		{
			return ColorToString(D2D1_COLOR_F{
				static_cast<float>(color.R), static_cast<float>(color.G),
				static_cast<float>(color.B), static_cast<float>(color.A) });
		};
		auto weightExpression = [](const std::wstring& value)
			-> std::optional<std::string>
		{
			if (value == L"Thin") return "DWRITE_FONT_WEIGHT_THIN";
			if (value == L"ExtraLight") return "DWRITE_FONT_WEIGHT_EXTRA_LIGHT";
			if (value == L"UltraLight") return "DWRITE_FONT_WEIGHT_ULTRA_LIGHT";
			if (value == L"Light") return "DWRITE_FONT_WEIGHT_LIGHT";
			if (value == L"SemiLight") return "DWRITE_FONT_WEIGHT_SEMI_LIGHT";
			if (value == L"Normal") return "DWRITE_FONT_WEIGHT_NORMAL";
			if (value == L"Regular") return "DWRITE_FONT_WEIGHT_REGULAR";
			if (value == L"Medium") return "DWRITE_FONT_WEIGHT_MEDIUM";
			if (value == L"DemiBold") return "DWRITE_FONT_WEIGHT_DEMI_BOLD";
			if (value == L"SemiBold") return "DWRITE_FONT_WEIGHT_SEMI_BOLD";
			if (value == L"Bold") return "DWRITE_FONT_WEIGHT_BOLD";
			if (value == L"ExtraBold") return "DWRITE_FONT_WEIGHT_EXTRA_BOLD";
			if (value == L"UltraBold") return "DWRITE_FONT_WEIGHT_ULTRA_BOLD";
			if (value == L"Black") return "DWRITE_FONT_WEIGHT_BLACK";
			if (value == L"Heavy") return "DWRITE_FONT_WEIGHT_HEAVY";
			if (value == L"ExtraBlack") return "DWRITE_FONT_WEIGHT_EXTRA_BLACK";
			if (value == L"UltraBlack") return "DWRITE_FONT_WEIGHT_ULTRA_BLACK";
			return std::nullopt;
		};
		auto styleExpression = [](const std::wstring& value)
			-> std::optional<std::string>
		{
			if (value == L"Oblique") return "DWRITE_FONT_STYLE_OBLIQUE";
			if (value == L"Italic") return "DWRITE_FONT_STYLE_ITALIC";
			if (value == L"Normal") return "DWRITE_FONT_STYLE_NORMAL";
			return std::nullopt;
		};
		auto stretchExpression = [](const std::wstring& value)
			-> std::optional<std::string>
		{
			if (value == L"UltraCondensed")
				return "DWRITE_FONT_STRETCH_ULTRA_CONDENSED";
			if (value == L"ExtraCondensed")
				return "DWRITE_FONT_STRETCH_EXTRA_CONDENSED";
			if (value == L"Condensed") return "DWRITE_FONT_STRETCH_CONDENSED";
			if (value == L"SemiCondensed")
				return "DWRITE_FONT_STRETCH_SEMI_CONDENSED";
			if (value == L"Normal" || value == L"Medium")
				return "DWRITE_FONT_STRETCH_NORMAL";
			if (value == L"SemiExpanded")
				return "DWRITE_FONT_STRETCH_SEMI_EXPANDED";
			if (value == L"Expanded") return "DWRITE_FONT_STRETCH_EXPANDED";
			if (value == L"ExtraExpanded")
				return "DWRITE_FONT_STRETCH_EXTRA_EXPANDED";
			if (value == L"UltraExpanded")
				return "DWRITE_FONT_STRETCH_ULTRA_EXPANDED";
			return std::nullopt;
		};
		auto alignmentExpression = [](const std::wstring& value)
			-> std::optional<std::string>
		{
			if (value == L"Left") return "::TextAlignment::Left";
			if (value == L"Right") return "::TextAlignment::Right";
			if (value == L"Center") return "::TextAlignment::Center";
			if (value == L"Justify") return "::TextAlignment::Justify";
			return std::nullopt;
		};
		auto flowDirectionExpression = [](const std::wstring& value)
			-> std::optional<std::string>
		{
			if (value == L"LeftToRight")
				return "::FlowDirection::LeftToRight";
			if (value == L"RightToLeft")
				return "::FlowDirection::RightToLeft";
			return std::nullopt;
		};
		auto emitFormatting = [&](const std::string& variable,
			const DesignerModel::DesignTextFormatting& formatting)
		{
			if (formatting.Foreground)
				code << indentStr << variable
					<< "->SetForeground(cui::drawing::MakeSolidColorBrush("
					<< colorExpression(*formatting.Foreground) << "));\n";
			if (formatting.Background)
				code << indentStr << variable
					<< "->SetBackground(cui::drawing::MakeSolidColorBrush("
					<< colorExpression(*formatting.Background) << "));\n";
			if (formatting.FontFamily)
				code << indentStr << variable << "->SetFontFamily(L\""
					<< EscapeWStringLiteral(*formatting.FontFamily) << "\");\n";
			if (formatting.Language)
			{
				if (!IsCanonicalRichTextLanguageTag(*formatting.Language))
					throw std::invalid_argument(
						"RichText Language is not canonical");
				code << indentStr << variable << "->SetLanguage(L\""
					<< EscapeWStringLiteral(*formatting.Language) << "\");\n";
			}
			if (formatting.FontSize)
				code << indentStr << variable << "->SetFontSize("
					<< DoubleLiteral(*formatting.FontSize) << ");\n";
			if (formatting.FontWeight)
			{
				const auto expression = weightExpression(*formatting.FontWeight);
				if (!expression)
					throw std::invalid_argument(
						"RichText FontWeight is not canonical");
				code << indentStr << variable << "->SetFontWeight("
					<< *expression << ");\n";
			}
			if (formatting.FontStretch)
			{
				const auto expression = stretchExpression(*formatting.FontStretch);
				if (!expression)
					throw std::invalid_argument(
						"RichText FontStretch is not canonical");
				code << indentStr << variable << "->SetFontStretch("
					<< *expression << ");\n";
			}
			if (formatting.FontStyle)
			{
				const auto expression = styleExpression(*formatting.FontStyle);
				if (!expression)
					throw std::invalid_argument(
						"RichText FontStyle is not canonical");
				code << indentStr << variable << "->SetFontStyle("
					<< *expression << ");\n";
			}
			if (formatting.Underline)
				code << indentStr << variable << "->SetUnderline("
					<< (*formatting.Underline ? "true" : "false") << ");\n";
			if (formatting.Strikethrough)
				code << indentStr << variable << "->SetStrikethrough("
					<< (*formatting.Strikethrough ? "true" : "false") << ");\n";
		};
		auto emitTextAlignment = [&](const std::string& variable,
			const std::optional<std::wstring>& alignment)
		{
			if (!alignment) return;
			const auto expression = alignmentExpression(*alignment);
			if (!expression)
				throw std::invalid_argument(
					"RichText TextAlignment is not canonical");
			code << indentStr << variable << "->SetTextAlignment("
				<< *expression << ");\n";
		};
		auto emitFlowDirection = [&](const std::string& variable,
			const std::optional<std::wstring>& direction)
		{
			if (!direction) return;
			const auto expression = flowDirectionExpression(*direction);
			if (!expression)
				throw std::invalid_argument(
					"RichText FlowDirection is not canonical");
			code << indentStr << variable << "->SetFlowDirection("
				<< *expression << ");\n";
		};

		const auto documentVariable = "__richDocument_" + name;
		code << indentStr << "auto " << documentVariable
			<< " = std::make_unique<FlowDocument>();\n";
		emitFormatting(documentVariable, *node.Structure.Document);
		emitTextAlignment(
			documentVariable, node.Structure.Document->TextAlignment);
		emitFlowDirection(
			documentVariable, node.Structure.Document->FlowDirection);
		for (std::size_t paragraphIndex = 0;
			paragraphIndex < node.Structure.Document->Paragraphs.size();
			++paragraphIndex)
		{
			const auto& paragraphDefinition =
				node.Structure.Document->Paragraphs[paragraphIndex];
			const auto paragraphVariable = "__richParagraph_" + name
				+ "_" + std::to_string(paragraphIndex + 1);
			code << indentStr << "auto " << paragraphVariable
				<< " = std::make_unique<Paragraph>();\n";
			emitFormatting(paragraphVariable, paragraphDefinition);
			emitTextAlignment(
				paragraphVariable, paragraphDefinition.TextAlignment);
			emitFlowDirection(
				paragraphVariable, paragraphDefinition.FlowDirection);
			std::function<void(
				const DesignerModel::DesignInline&,
				const std::string&, const std::string&)> emitInline;
			emitInline = [&](const DesignerModel::DesignInline& definition,
				const std::string& ownerVariable,
				const std::string& path)
			{
				const auto variable = "__richInline_" + name + "_"
					+ std::to_string(paragraphIndex + 1) + "_" + path;
				switch (definition.Kind)
				{
				case DesignerModel::DesignInlineKind::Run:
					code << indentStr << "auto " << variable
						<< " = std::make_unique<Run>(L\""
						<< EscapeWStringLiteral(definition.Text) << "\");\n";
					break;
				case DesignerModel::DesignInlineKind::Span:
					code << indentStr << "auto " << variable
						<< " = std::make_unique<Span>();\n";
					break;
				case DesignerModel::DesignInlineKind::Bold:
					code << indentStr << "auto " << variable
						<< " = std::make_unique<Bold>();\n";
					break;
				case DesignerModel::DesignInlineKind::Italic:
					code << indentStr << "auto " << variable
						<< " = std::make_unique<Italic>();\n";
					break;
				case DesignerModel::DesignInlineKind::Underline:
					code << indentStr << "auto " << variable
						<< " = std::make_unique<::Underline>();\n";
					break;
				case DesignerModel::DesignInlineKind::LineBreak:
					code << indentStr << "auto " << variable
						<< " = std::make_unique<LineBreak>();\n";
					break;
				default:
					throw std::invalid_argument(
						"RichText Inline kind is not canonical");
				}
				emitFormatting(variable, definition);
				if (definition.Kind != DesignerModel::DesignInlineKind::Run
					&& definition.Kind
						!= DesignerModel::DesignInlineKind::LineBreak)
				{
					for (std::size_t childIndex = 0;
						childIndex < definition.Inlines.size(); ++childIndex)
					{
						emitInline(definition.Inlines[childIndex], variable,
							path + "_" + std::to_string(childIndex + 1));
					}
				}
				code << indentStr << ownerVariable
					<< "->GetInlines().Add(std::move(" << variable << "));\n";
			};
			for (std::size_t inlineIndex = 0;
				inlineIndex < paragraphDefinition.Inlines.size(); ++inlineIndex)
			{
				emitInline(paragraphDefinition.Inlines[inlineIndex],
					paragraphVariable, std::to_string(inlineIndex + 1));
			}
			code << indentStr << documentVariable
				<< "->GetBlocks().Add(std::move(" << paragraphVariable << "));\n";
		}
		code << indentStr << name << "->SetDocument(std::move("
			<< documentVariable << "));\n";
	}

	if (node.Type == UIClass::UI_ChartView
		&& node.Structure.ChartSeries)
	{
		auto colorExpression = [&](const DesignerModel::DesignColor& color)
		{
			return ColorToString(D2D1_COLOR_F{
				static_cast<float>(color.R), static_cast<float>(color.G),
				static_cast<float>(color.B), static_cast<float>(color.A) });
		};
		const auto& chartSeries = *node.Structure.ChartSeries;
		code << indentStr << name << "->Clear();\n";
		for (size_t seriesIndex = 0; seriesIndex < chartSeries.size(); ++seriesIndex)
		{
			const auto& series = chartSeries[seriesIndex];
			const auto seriesVar = "__chartSeries_" + name + "_"
				+ std::to_string(seriesIndex + 1);
			code << indentStr << "ChartSeries " << seriesVar << ";\n";
			code << indentStr << seriesVar << ".Name = L\""
				<< EscapeWStringLiteral(series.Name) << "\";\n";
			if (series.Color)
				code << indentStr << seriesVar << ".Color = "
					<< colorExpression(*series.Color) << ";\n";
			if (!series.Visible)
				code << indentStr << seriesVar << ".Visible = false;\n";
			for (size_t pointIndex = 0;
				pointIndex < series.Points.size(); ++pointIndex)
			{
				const auto& point = series.Points[pointIndex];
				code << indentStr << seriesVar << ".Points.emplace_back(L\""
					<< EscapeWStringLiteral(point.Label) << "\", "
					<< DoubleLiteral(point.Value);
				if (point.Color) code << ", " << colorExpression(*point.Color);
				code << ");\n";
				if (point.Tag != 0) code << indentStr << seriesVar
					<< ".Points.back().Tag = " << point.Tag << "ULL;\n";
			}
			code << indentStr << name << "->AddSeries(" << seriesVar << ");\n";
		}
	}

	return code.str();
}

bool CodeGenerator::CollectEventHandlers(
	std::vector<std::pair<std::string, std::string>>& handlers,
	std::wstring* outError) const
{
	handlers.clear();
	if (outError) outError->clear();
	std::unordered_map<std::string, std::type_index> signatures;
	auto add = [&](const std::wstring& eventName,
		const std::wstring& storedHandler,
		const std::optional<DesignerEventDescriptor>& descriptor) -> bool
	{
		if (storedHandler.empty()) return true;
		if (!descriptor)
		{
			if (outError) *outError = L"无法生成未知事件 “" + eventName + L"”。";
			return false;
		}
		const auto resolved = DesignerEventCatalog::NormalizeHandlerName(
			storedHandler);
		std::wstring validationError;
		if (!DesignerEventCatalog::ValidateHandlerName(resolved, &validationError))
		{
			if (outError) *outError = L"事件 “" + eventName + L"”：" + validationError;
			return false;
		}
		const auto handler = Utf8HandlerName(storedHandler);
		if (handler.empty()) return true;
		auto existing = signatures.find(handler);
		if (existing != signatures.end())
		{
			if (existing->second == descriptor->Signature) return true;
			if (outError) *outError = L"处理函数 “" + resolved
				+ L"” 被参数签名不同的事件复用。";
			return false;
		}
		signatures.emplace(handler, descriptor->Signature);
		handlers.emplace_back(handler, descriptor->ParameterList);
		return true;
	};

	for (const auto& [eventName, storedHandler]
		: _sourceDocument.Window.Events)
		if (!add(eventName, storedHandler,
			DesignerEventCatalog::FindWindowEvent(eventName))) return false;
	for (const auto& binding : _sourceDocument.Window.CommandBindings)
	{
		for (const auto& [eventName, handler] : binding.HandlerRoutes())
			if (handler && !add(eventName, *handler,
				DesignerEventCatalog::FindWindowEvent(eventName))) return false;
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		for (const auto& [eventName, storedHandler] : node.Events)
			if (!add(eventName, storedHandler,
				FindNodeEventDescriptor(node, eventName))) return false;
		for (const auto& binding : node.CommandBindings)
		{
			for (const auto& [eventName, handler] : binding.HandlerRoutes())
				if (handler && !add(eventName, *handler,
					DesignerEventCatalog::FindControlEvent(
					node.Type, eventName,
					ComponentEvents(node)))) return false;
		}
	}
	for (const auto& definition : _sourceDocument.ControlTemplates)
	{
		for (const auto& node : definition.Template)
		{
			for (const auto& [eventName, storedHandler] : node.Events)
				if (!add(eventName, storedHandler,
					DesignerEventCatalog::FindControlEvent(
						node.Type, eventName,
						ComponentEvents(node)))) return false;
			for (const auto& binding : node.CommandBindings)
			{
				for (const auto& [eventName, handler]
					: binding.HandlerRoutes())
					if (handler && !add(eventName, *handler,
						DesignerEventCatalog::FindControlEvent(
							node.Type, eventName,
							ComponentEvents(node)))) return false;
			}
		}
	}
	return true;
}

std::string CodeGenerator::GenerateHeader()
{
	std::ostringstream header;
	const bool dynamicWindow =
		_outputKind == CodeGeneratorOutputKind::Window;
	const bool frameworkThemeProgram =
		_outputKind == CodeGeneratorOutputKind::FrameworkThemeProgram;
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	const auto& className = identity.GeneratedLeaf;
	if (frameworkThemeProgram)
	{
		header << "#pragma once\n\n";
		header << "#include <memory>\n\n";
		header << "class ControlStyleSheet;\n\n";
		if (!identity.NamespaceName.empty())
			header << "namespace " << identity.NamespaceName << "\n{\n\n";
		header << "class " << className << " final\n";
		header << "{\n";
		header << "public:\n";
		header << "\t" << className << "() = delete;\n";
		header << "\t[[nodiscard]] static std::shared_ptr<const "
			"ControlStyleSheet> Build();\n";
		header << "};\n";
		if (!identity.NamespaceName.empty()) header << "\n}\n";
		return header.str();
	}
	std::vector<std::pair<std::string, std::string>> eventHandlers;
	std::wstring eventError;
	if (!CollectEventHandlers(eventHandlers, &eventError))
		throw std::invalid_argument(WStringToString(eventError));

	std::vector<GeneratedRuntimeEventRoute> runtimeRoutes;
	std::set<std::string> runtimeRouteKeys;
	auto appendBuiltInRoute = [&](bool isWindow,
		UIClass controlType,
		const std::wstring& eventName,
		const std::wstring& storedHandler,
		const DesignerEventDescriptor& descriptor)
	{
		const auto handler = Utf8HandlerName(storedHandler);
		if (handler.empty()) return;
		const auto baseDescriptor = isWindow
			? std::optional<DesignerEventDescriptor>{}
			: DesignerEventCatalog::FindControlEvent(
				UIClass::UI_Base, eventName);
		const bool wildcard = baseDescriptor
			&& baseDescriptor->EventField == descriptor.EventField
			&& baseDescriptor->EventOwnerTypeName
				== descriptor.EventOwnerTypeName
			&& baseDescriptor->SameSignature(descriptor);
		const auto effectiveType = wildcard ? UIClass::UI_Base : controlType;
		const auto key = std::string(isWindow ? "F|" : "B|") + handler
			+ "|" + WStringToString(eventName)
			+ "|" + descriptor.EventOwnerTypeName
			+ "|" + descriptor.EventField
			+ "|" + std::to_string(static_cast<int>(effectiveType));
		if (!runtimeRouteKeys.insert(key).second) return;
		GeneratedRuntimeEventRoute route;
		route.HandlerName = handler;
		route.ParameterList = descriptor.ParameterList;
		route.EventName = eventName;
		route.EventField = descriptor.EventField;
		route.EventOwnerType = descriptor.EventOwnerTypeName;
		route.IsWindow = isWindow;
		route.ControlType = effectiveType;
		runtimeRoutes.push_back(std::move(route));
	};

	for (const auto& [eventName, storedHandler]
		: _sourceDocument.Window.Events)
		if (const auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName))
			appendBuiltInRoute(true, UIClass::UI_Base, eventName,
				storedHandler, *descriptor);
	for (const auto& binding : _sourceDocument.Window.CommandBindings)
	{
		for (const auto& [eventName, handler] : binding.HandlerRoutes())
			if (handler && !handler->empty())
				if (const auto descriptor =
					DesignerEventCatalog::FindWindowEvent(eventName))
					appendBuiltInRoute(true, UIClass::UI_Base,
						eventName, *handler, *descriptor);
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		for (const auto& [eventName, storedHandler] : node.Events)
		{
			const auto descriptor = DesignerEventCatalog::FindControlEvent(
				node.Type, eventName, ComponentEvents(node));
			if (!descriptor) continue;
			appendBuiltInRoute(false, node.Type, eventName,
				storedHandler, *descriptor);
		}
		for (const auto& binding : node.CommandBindings)
		{
			for (const auto& [eventName, handler] : binding.HandlerRoutes())
			{
				if (!handler || handler->empty()) continue;
				const auto descriptor = DesignerEventCatalog::FindControlEvent(
					node.Type, eventName, ComponentEvents(node));
				if (descriptor) appendBuiltInRoute(false, node.Type,
					eventName, *handler, *descriptor);
			}
		}
	}
	for (const auto& definition : _sourceDocument.ControlTemplates)
	{
		for (const auto& node : definition.Template)
		{
			for (const auto& [eventName, storedHandler] : node.Events)
			{
				const auto descriptor =
					DesignerEventCatalog::FindControlEvent(
						node.Type, eventName,
						ComponentEvents(node));
				if (descriptor)
					appendBuiltInRoute(
						false, node.Type, eventName,
						storedHandler, *descriptor);
			}
			for (const auto& binding : node.CommandBindings)
			{
				for (const auto& [eventName, handler]
					: binding.HandlerRoutes())
				{
					if (!handler || handler->empty()) continue;
					const auto descriptor =
						DesignerEventCatalog::FindControlEvent(
							node.Type, eventName,
							ComponentEvents(node));
					if (descriptor)
						appendBuiltInRoute(
							false, node.Type, eventName,
							*handler, *descriptor);
				}
			}
		}
	}

	// 收集需要的头文件
	std::set<std::string> includes;
	includes.insert("Control.h");
	includes.insert("Layout/LayoutTypes.h");
	includes.insert("Window.h");
	if (!_sourceDocument.DataLists.empty())
		includes.insert("BindingList.h");
	if (!_sourceDocument.CollectionViews.empty())
		includes.insert("CollectionViewSource.h");
	const bool hasItemsPanelTemplates =
		!_sourceDocument.ItemsPanelTemplates.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.LocalObjectResources.ItemsPanelTemplates.empty();
			});
	if (hasItemsPanelTemplates)
		includes.insert("ItemsPanelTemplate.h");
	if (!_sourceDocument.GroupStyles.empty())
		includes.insert("GroupStyle.h");
	const bool hasCommands = !_sourceDocument.Window.CommandBindings.empty()
		|| !_sourceDocument.Window.InputBindings.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.CommandBindings.empty()
					|| !node.InputBindings.empty();
			})
		|| std::any_of(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[](const auto& definition)
			{
				return std::any_of(
					definition.Template.begin(),
					definition.Template.end(),
					[](const auto& node)
					{
							return !node.CommandBindings.empty()
								|| !node.InputBindings.empty();
						});
			})
		|| std::any_of(
			_sourceDocument.Components.begin(),
			_sourceDocument.Components.end(),
			[](const auto& component)
			{
				return std::any_of(
					component.Template.begin(), component.Template.end(),
					[](const auto& node)
					{
						return !node.CommandBindings.empty()
							|| !node.InputBindings.empty();
					});
			});
	if (hasCommands) includes.insert("RoutedCommand.h");
	const bool hasDataBindings =
		HasGeneratedDataBindings(_sourceDocument, _styleSheet);
	const bool hasStaticDataContextProperties =
		!dynamicWindow && !_sourceDocument.DataContextSchema.empty();
	const bool hasAuthoredProperties =
		!_sourceDocument.Window.Properties.Values.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{ return !node.Properties.Values.empty(); })
		|| std::any_of(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[](const auto& definition)
			{
				return std::any_of(
					definition.Template.begin(),
					definition.Template.end(),
					[](const auto& node)
					{ return !node.Properties.Values.empty(); });
			})
		|| !_sourceDocument.Components.empty();
	if (hasDataBindings || hasAuthoredProperties
		|| hasStaticDataContextProperties)
		includes.insert("Binding.h");

	for (const auto& node : _sourceDocument.Nodes)
		includes.insert(GetIncludeForType(node.Type));
	for (const auto& component : _sourceDocument.Components)
	{
		includes.insert(GetIncludeForType(component.BaseType));
		for (const auto& node : component.Template)
			includes.insert(GetIncludeForType(node.Type));
	}
	auto collectStyleTargetIncludes =
		[&](const DesignerStyleSheet& styleSheet)
		{
			for (const auto& rule : styleSheet.Rules)
				if (rule.HasType)
					includes.insert(GetIncludeForType(rule.Type));
		};
	collectStyleTargetIncludes(_styleSheet);
	collectStyleTargetIncludes(_sourceDocument.Window.LocalResources);
	for (const auto& node : _sourceDocument.Nodes)
		collectStyleTargetIncludes(node.LocalResources);
	for (const auto& definition : _sourceDocument.ControlTemplates)
		for (const auto& node : definition.Template)
			collectStyleTargetIncludes(node.LocalResources);
	for (const auto& dataTemplate : _sourceDocument.DataTemplates)
		for (const auto& node : dataTemplate.Template)
			collectStyleTargetIncludes(node.LocalResources);
	for (const auto& component : _sourceDocument.Components)
		for (const auto& node : component.Template)
			collectStyleTargetIncludes(node.LocalResources);

	// 生成头文件
	header << "#pragma once\n";
	for (const auto& inc : includes)
	{
		header << "#include \"" << inc << "\"\n";
	}
	header << "#include <functional>\n";
	header << "#include <memory>\n";
	header << "#include <string>\n";
	header << "#include <utility>\n";
	header << "#include <vector>\n";
	header << "\n";
	if (!identity.NamespaceName.empty())
		header << "namespace " << identity.NamespaceName << "\n{\n\n";

	if (dynamicWindow && !eventHandlers.empty())
	{
		const auto eventSinkName = identity.UserLeaf + "EventSink";
		header << "class " << eventSinkName << "\n{\n";
		header << "public:\n";
		header << "\t" << eventSinkName << "() = default;\n";
		header << "\tvirtual ~" << eventSinkName
			<< "() { UnregisterDeclarativeEventHandlers(); }\n";
		header << "\t" << eventSinkName << "(const " << eventSinkName
			<< "&) = delete;\n";
		header << "\t" << eventSinkName << "& operator=(const "
			<< eventSinkName << "&) = delete;\n";
		header << "\t" << eventSinkName << "(" << eventSinkName
			<< "&&) = delete;\n";
		header << "\t" << eventSinkName << "& operator=("
			<< eventSinkName << "&&) = delete;\n\n";
		header << "\ttemplate<typename TRegistry>\n";
		header << "\tbool RegisterDeclarativeEventHandlers(\n";
		header << "\t\tTRegistry& registry, std::wstring* outError = nullptr)\n";
		header << "\t{\n";
		header << "\t\ttry\n";
		header << "\t\t{\n";
		header << "\t\t\tauto lifetime = std::make_shared<int>(0);\n";
		header << "\t\t\tauto registration = registry.RegisterScopedBatch(\n";
		header << "\t\t\t[this, lifetime = std::weak_ptr<void>(lifetime)](\n";
		header << "\t\t\t\tauto& routes, std::wstring& error)\n";
		header << "\t\t\t{\n";
		for (const auto& route : runtimeRoutes)
		{
			const auto parameterTypes = CanonicalGeneratedParameterTypes(
				route.ParameterList);
			header << "\t\t\t\tif (!routes.";
			if (route.IsWindow)
			{
				header << "RegisterWindow(\n";
				header << "\t\t\t\t\tL\""
					<< EscapeWStringLiteral(StringToWString(route.HandlerName))
					<< "\", L\"" << EscapeWStringLiteral(route.EventName)
					<< "\", &" << route.EventOwnerType << "::"
					<< route.EventField << ",\n";
			}
			else
			{
				header << "RegisterControl(\n";
				header << "\t\t\t\t\tL\""
					<< EscapeWStringLiteral(StringToWString(route.HandlerName))
					<< "\", static_cast<UIClass>("
					<< static_cast<int>(route.ControlType) << ")"
					<< ", L\"" << EscapeWStringLiteral(route.EventName)
					<< "\", &" << route.EventOwnerType << "::"
					<< route.EventField << ",\n";
			}
			header << "\t\t\t\t\tGuardDeclarativeEventHandler(\n";
			header << "\t\t\t\t\t\tlifetime, std::bind_front(\n";
			header << "\t\t\t\t\t\t\tstatic_cast<void (" << eventSinkName
				<< "::*)(" << parameterTypes << ")>(\n";
			header << "\t\t\t\t\t\t\t\t&" << eventSinkName << "::"
				<< route.HandlerName << "), this)), &error))\n";
			header << "\t\t\t\t\treturn false;\n";
		}
		header << "\t\t\t\treturn true;\n";
		header << "\t\t\t}, outError);\n";
		header << "\t\t\tif (!registration) return false;\n";
		header << "\t\t\tstruct DeclarativeEventRegistration final\n";
		header << "\t\t\t{\n";
		header << "\t\t\t\tdecltype(registration) Lease;\n";
		header << "\t\t\t\tstd::shared_ptr<void> Lifetime;\n";
		header << "\t\t\t\tDeclarativeEventRegistration(\n";
		header << "\t\t\t\t\tdecltype(registration)&& lease,\n";
		header << "\t\t\t\t\tstd::shared_ptr<void> lifetime) noexcept\n";
		header << "\t\t\t\t\t: Lease(std::move(lease)),\n";
		header << "\t\t\t\t\tLifetime(std::move(lifetime)) {}\n";
		header << "\t\t\t};\n";
		header << "\t\t\tauto owned = std::make_shared<DeclarativeEventRegistration>(\n";
		header << "\t\t\t\tstd::move(registration), std::move(lifetime));\n";
		header << "\t\t\t_declarativeEventRegistration = std::move(owned);\n";
		header << "\t\t\tif (outError) outError->clear();\n";
		header << "\t\t\treturn true;\n";
		header << "\t\t}\n";
		header << "\t\tcatch (...)\n";
		header << "\t\t{\n";
		header << "\t\t\tif (outError) *outError =\n";
		header << "\t\t\t\tL\"无法保存声明事件注册租约。\";\n";
		header << "\t\t\treturn false;\n";
		header << "\t\t}\n";
		header << "\t}\n\n";
		header << "\tvoid UnregisterDeclarativeEventHandlers() noexcept\n";
		header << "\t{\n";
		header << "\t\t_declarativeEventRegistration.reset();\n";
		header << "\t}\n\n";
		header << "private:\n";
		header << "\ttemplate<typename TCallback>\n";
		header << "\tstatic auto GuardDeclarativeEventHandler(\n";
		header << "\t\tstd::weak_ptr<void> lifetime, TCallback callback)\n";
		header << "\t{\n";
		header << "\t\treturn [lifetime = std::move(lifetime),\n";
		header << "\t\t\tcallback = std::move(callback)](auto&&... args) mutable\n";
		header << "\t\t{\n";
		header << "\t\t\tauto alive = lifetime.lock();\n";
		header << "\t\t\tif (!alive) return;\n";
		header << "\t\t\tstd::invoke(callback,\n";
		header << "\t\t\t\tstd::forward<decltype(args)>(args)...);\n";
		header << "\t\t};\n";
		header << "\t}\n\n";
		header << "\tstd::shared_ptr<void> _declarativeEventRegistration;\n\n";
		header << "protected:\n";
		for (const auto& handler : eventHandlers)
			header << "\tvirtual void " << handler.first << "("
				<< handler.second << ") = 0;\n";
		header << "};\n\n";
	}

	if (!dynamicWindow)
	{
		for (const auto& component : _sourceDocument.Components)
		{
			const auto componentClass = GetComponentClassName(component);
			const bool hasInheritedProperties = std::any_of(
				component.Properties.begin(), component.Properties.end(),
				[](const auto& property)
				{
					return HasDependencyPropertyFlag(
						property.Flags, DependencyPropertyFlags::Inherits);
				});
			header << "/** Build-time C++ projection of XAML "
				"ComponentDefinition " << WStringToString(
					component.Type.XamlName) << ". */\n";
			header << "class " << componentClass << " final : public "
				<< GetControlTypeName(component.BaseType) << "\n";
			header << "{\n";
			header << "protected:\n";
			header << "\tComponentTypeToken "
				"GetCompiledComponentTypeTokenCore() const noexcept override;\n";
			header << "\tconst DependencyPropertyMetadata* "
				"FindCompiledComponentPropertyCore(\n";
			header << "\t\tComponentPropertyToken property) "
				"const override;\n";
			header << "\tbool IsCompiledComponentPropertyCore(\n";
			header << "\t\tconst DependencyPropertyMetadata& metadata) "
				"const noexcept override;\n\n";
			if (hasInheritedProperties)
			{
				header << "\tvoid VisitDeclaredInheritedProperties(\n";
				header << "\t\tvoid* context, InheritedPropertyVisitor visitor) "
					"const override;\n\n";
			}
			header << "private:\n";
			for (const auto& property : component.Properties)
				if (property.IsReadOnly)
					header << "\tstatic const DependencyPropertyKey& "
						<< SanitizeCppIdentifier(WStringToString(
							property.Name)) << "PropertyKey();\n";
			header << "\tbool InitializeGeneratedTemplate("
				"std::wstring* outError = nullptr);\n";
			for (const auto& node : component.Template)
			{
				const auto localName = SanitizeCppIdentifier(
					WStringToString(node.Name));
				header << "\t" << GetGeneratedControlTypeName(node)
					<< "* _part_" << localName << " = nullptr;\n";
			}
			for (const auto& content : component.ContentProperties)
			{
				const auto memberName = SanitizeCppIdentifier(
					WStringToString(content.Name));
				header << "\tControl* _presenter_" << memberName
					<< " = nullptr;\n";
				if (content.Cardinality
					== DesignerComponentContentCardinality::Single)
					header << "\tControl* _content_" << memberName
						<< " = nullptr;\n";
			}
			header << "\npublic:\n";
			header << "\t" << componentClass << "();\n";
			header << "\t~" << componentClass << "() override = default;\n";
			header << "\t[[nodiscard]] static ComponentTypeToken "
				"ComponentTypeId() noexcept;\n";
			for (const auto& property : component.Properties)
			{
				const auto propertyName = SanitizeCppIdentifier(
					WStringToString(property.Name));
				const auto* cppType =
					ComponentValueCppType(property.DefaultValue.Kind);
				if (!cppType)
					throw std::invalid_argument(
						"Generated component property type is unsupported");
				header << "\t[[nodiscard]] static const "
					"DependencyProperty& " << propertyName
					<< "Property();\n";
				header << "\t[[nodiscard]] " << cppType << " Get"
					<< propertyName << "() const;\n";
				if (property.IsReadOnly)
					header << "\tbool Publish" << propertyName
						<< "(" << cppType << " value);\n";
				else
					header << "\tvoid Set" << propertyName
						<< "(" << cppType << " value);\n";
			}
			for (const auto& event : component.Events)
			{
				const auto eventName = SanitizeCppIdentifier(
					WStringToString(event.Name));
				header << "\t[[nodiscard]] static const "
					"DeclarativeEventDefinition& " << eventName
					<< "Event() noexcept;\n";
				header << "\tEventConnection Subscribe" << eventName
					<< "(DeclarativeEvent::std_function_type handler,\n";
				header << "\t\tbool handledEventsToo = false);\n";
				header << "\tbool Raise" << eventName << "(";
				if (const auto* payloadType =
					ComponentEventPayloadCppType(event.Payload))
					header << payloadType << " value";
				header << ");\n";
			}
			for (const auto& content : component.ContentProperties)
			{
				const auto contentName = SanitizeCppIdentifier(
					WStringToString(content.Name));
				header << "\tbool "
					<< (content.Cardinality
						== DesignerComponentContentCardinality::Single
						? "Set" : "Add")
					<< contentName
					<< "(std::unique_ptr<Control> value);\n";
			}
			for (const auto& node : component.Template)
			{
				if (node.NameIsGenerated) continue;
				const auto partName = SanitizeCppIdentifier(
					WStringToString(node.Name));
				header << "\t[[nodiscard]] "
					<< GetGeneratedControlTypeName(node) << "* Get"
					<< partName << "() noexcept { return _part_"
					<< partName << "; }\n";
				header << "\t[[nodiscard]] const "
					<< GetGeneratedControlTypeName(node) << "* Get"
					<< partName << "() const noexcept { return _part_"
					<< partName << "; }\n";
			}
			header << "};\n\n";
		}
	}

	header << "class " << className << " : public Window";
	if (dynamicWindow && !eventHandlers.empty())
		header << ", public " << identity.UserLeaf << "EventSink";
	header << "\n";
	header << "{\n";
	header << "protected:\n";

	// 声明控件成员
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		std::string name = GetVarName(node);
		std::string typeName = GetGeneratedControlTypeName(node);
		header << "\t" << typeName << "* " << name << " = nullptr;\n";
	}
	header << "\tstd::vector<EventConnection> _generatedEventConnections;\n";
	header << "\tbool _componentInitialized = false;\n";
	header << "\tvoid InitializeComponent();\n";

	// Generated virtual hooks are overridden by declarations in the user class.
	if (!eventHandlers.empty())
	{
		header << "\n";
		for (const auto& handler : eventHandlers)
		{
			header << "\t";
			if (!dynamicWindow) header << "virtual ";
			header << "void " << handler.first << "("
				<< handler.second << ")";
			if (dynamicWindow) header << " override";
			else header << " = 0";
			header << ";\n";
		}
	}

	header << "\n";
	header << "public:\n";
	if (hasStaticDataContextProperties)
	{
		ValidateGeneratedBindingSourcePropertyTokens(
			_sourceDocument.DataContextSchema,
			"Static Window DataContext schema");
		std::vector<std::pair<std::wstring, std::string>> constants;
		constants.reserve(_sourceDocument.DataContextSchema.size());
		std::map<std::string, size_t> baseNameCounts;
		for (const auto& property : _sourceDocument.DataContextSchema)
		{
			const auto path =
				DesignerDataContextSchemaUtils::NormalizePath(property.Path);
			auto name = SanitizeCppIdentifier(WStringToString(path));
			++baseNameCounts[name];
			constants.emplace_back(path, std::move(name));
		}
		std::sort(constants.begin(), constants.end(),
			[](const auto& left, const auto& right)
			{ return left.first < right.first; });
		for (size_t index = 1; index < constants.size(); ++index)
			if (constants[index - 1].first == constants[index].first)
				throw std::invalid_argument(
					"Static Window DataContext schema contains a duplicate path");
		header << "\t// Name-free identities for the authored root DataContext contract.\n";
		header << "\tstruct DataContextProperties final\n\t{\n";
		for (const auto& [path, baseName] : constants)
		{
			auto name = baseName;
			if (baseNameCounts[baseName] > 1)
				name += "_" + std::to_string(
					GeneratedInteractionNameTokenValue(path));
			std::vector<BindingPathStep> steps;
			if (!TryParseBindingPropertyPath(path, steps)
				|| steps.empty())
				throw std::invalid_argument(
					"Static Window DataContext property path is invalid");
			header << "\t\tstatic constexpr BindingSourcePropertyToken "
				<< name << "{ "
				<< GeneratedBindingSourcePropertyTokenValue(
					steps.back().Value)
				<< "ULL };\n";
		}
		header << "\t};\n\n";
	}
	const bool hasAuthoredNamedControls = std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node)
		{ return !node.TemplateState.Generated && !node.NameIsGenerated; });
	if (hasAuthoredNamedControls)
	{
		header << "\t// Type-safe x:Name accessors; ownership remains with the generated Window.\n";
		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated || node.NameIsGenerated) continue;
			auto accessorName = GetVarName(node);
			if (!accessorName.empty() && accessorName.front() >= 'a'
				&& accessorName.front() <= 'z')
				accessorName.front() = static_cast<char>(
					accessorName.front() - 'a' + 'A');
			const auto typeName = GetGeneratedControlTypeName(node);
			header << "\t[[nodiscard]] " << typeName << "* Get"
				<< accessorName << "() noexcept { return "
				<< GetVarName(node) << "; }\n";
			header << "\t[[nodiscard]] const " << typeName << "* Get"
				<< accessorName << "() const noexcept { return "
				<< GetVarName(node) << "; }\n";
		}
		header << "\n";
	}
	header << "\t" << className << "();\n";
	header << "\tvirtual ~" << className << "();\n";
	if (hasDataBindings)
		header << "\tbool BindData(BindingSourceReference dataContext);\n";
	header << "};\n";

	// A zero-owning typed view over the dynamic RuntimeDocument contract. Keep
	// this template independent of CuiRuntime headers; Generic.xaml support is
	// consumed by the generated implementation, not leaked through the API.
	if (dynamicWindow)
	{
		header << "\n";
		header << "// Non-owning typed access for a dynamically loaded document.\n";
		header << "// GetXxx resolves the current instance; ReferenceXxx follows reloads.\n";
		header << "template<typename TDocument>\n";
		header << "class " << identity.UserLeaf << "References final\n";
		header << "{\n";
		header << "public:\n";
		header << "\tusing DocumentReference = decltype(\n";
		header << "\t\tstd::declval<TDocument&>().Reference());\n\n";
		header << "\texplicit " << identity.UserLeaf
			<< "References(TDocument& document) noexcept\n";
		header << "\t\t: _document(document.Reference()) {}\n\n";
		header << "\t[[nodiscard]] explicit operator bool() const noexcept\n";
		header << "\t{\n\t\treturn static_cast<bool>(_document);\n\t}\n";
		header << "\t[[nodiscard]] TDocument* TryDocument() const noexcept\n";
		header << "\t{\n\t\treturn _document.Get();\n\t}\n";
		header << "\t// Precondition: the view is still alive; prefer TryDocument() when uncertain.\n";
		header << "\t[[nodiscard]] TDocument& Document() const noexcept"
			" { return *_document.Get(); }\n";
		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated || node.NameIsGenerated) continue;
			auto accessorName = GetVarName(node);
			if (!accessorName.empty() && accessorName.front() >= 'a'
				&& accessorName.front() <= 'z')
				accessorName.front() = static_cast<char>(
					accessorName.front() - 'a' + 'A');
			const auto typeName = GetGeneratedControlTypeName(node);
			header << "\t[[nodiscard]] " << typeName << "* Get"
				<< accessorName << "() const\n\t{\n";
			header << "\t\treturn _document.template FindControlByName<"
				<< typeName << ">(L\""
				<< EscapeWStringLiteral(node.Name) << "\");\n\t}\n";
			header << "\t[[nodiscard]] auto Reference" << accessorName
				<< "() const\n\t{\n";
			header << "\t\treturn _document.template ReferenceByName<"
				<< typeName << ">(L\""
				<< EscapeWStringLiteral(node.Name) << "\");\n\t}\n";
		}
		header << "\nprivate:\n";
		header << "\tDocumentReference _document;\n";
		header << "};\n";
	}
	if (!identity.NamespaceName.empty())
		header << "\n}\n";

	return header.str();
}

std::string CodeGenerator::GenerateCpp()
{
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	return GenerateCppForBaseName(identity.UserLeaf);
}

std::string CodeGenerator::GenerateHandlerDeclarations()
{
	std::vector<std::pair<std::string, std::string>> handlers;
	std::wstring error;
	if (!CollectEventHandlers(handlers, &error))
		throw std::invalid_argument(WStringToString(error));
	std::ostringstream output;
	output << "// Generated by CUI Designer. Do not edit.\n";
	output << "// Build-owned declarations for the current XAML event surface.\n";
	for (const auto& [name, parameters] : handlers)
		output << "\tvoid " << name << "(" << parameters << ") override;\n";
	return output.str();
}

std::string CodeGenerator::GenerateCppForBaseName(
	const std::string& generatedHeaderBaseName)
{
	const bool dynamicWindow =
		_outputKind == CodeGeneratorOutputKind::Window;
	const bool frameworkThemeProgram =
		_outputKind == CodeGeneratorOutputKind::FrameworkThemeProgram;
	const char* frameworkThemeType = dynamicWindow
		? "CuiRuntime::XamlFrameworkTheme"
		: "CuiGeneratedFrameworkTheme";
	struct StaticControlTemplateBlueprint final
	{
		size_t SourceIndex = 0;
		DesignerModel::DesignDocument Document;
		std::wstring OwnerName;
		std::string VariableName;
		std::vector<DeclarativeVisualStateGroupDefinition> VisualStateGroups;
		std::vector<DeclarativeEventTriggerDefinition> EventTriggers;
	};

	std::vector<StaticControlTemplateBlueprint> templateBlueprints;
	templateBlueprints.reserve(_sourceDocument.ControlTemplates.size());
	for (size_t templateIndex = 0;
		templateIndex < _sourceDocument.ControlTemplates.size();
		++templateIndex)
	{
		const auto& definition =
			_sourceDocument.ControlTemplates[templateIndex];
		if (!definition.TargetComponentType.Empty())
			throw std::invalid_argument(
				"Static ControlTemplate builders require a built-in TargetType");
		const auto* targetDescriptor =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(
				definition.TargetType);
		if (!targetDescriptor || !targetDescriptor->IsConstructible)
			throw std::invalid_argument(
				"Static ControlTemplate builder TargetType is not constructible");

		DesignerModel::DesignDocument synthetic;
		synthetic.Window = _sourceDocument.Window;
		synthetic.Window.Name =
			L"__cuiStaticTemplateWindow"
			+ std::to_wstring(templateIndex + 1);
		synthetic.Window.Properties = {};
		synthetic.Window.Structure = {};
		synthetic.Window.TemplateState = {};
		synthetic.Window.Events.clear();
		synthetic.Window.Bindings.clear();
		synthetic.Window.CommandBindings.clear();
		synthetic.Window.InputBindings.clear();
		synthetic.Window.LocalResources = {};
		synthetic.Window.LocalObjectResources = {};
		synthetic.Window.TemplateBindings.clear();
		synthetic.Window.TemplateEventBindings.clear();
		synthetic.CodeBehind = {};
		synthetic.DataContextSchema = {};
		synthetic.StyleSheet = _sourceDocument.StyleSheet;
		synthetic.Components.clear();
		synthetic.ControlTemplates =
			_sourceDocument.ControlTemplates;
		// A ControlTemplate is compiled in the resource scope in which it was
		// declared. Keep the document-level native object resources visible to
		// the synthetic owner so Style setters such as
		// ItemsPanel="{StaticResource ...}" resolve exactly as they do in the
		// source dictionary. The synthetic document is only used to expand the
		// selected visual tree; retaining these declarations does not introduce
		// a runtime XAML dependency.
		synthetic.DataTypes = _sourceDocument.DataTypes;
		synthetic.DataTemplates = _sourceDocument.DataTemplates;
		synthetic.ItemsPanelTemplates =
			_sourceDocument.ItemsPanelTemplates;
		synthetic.GroupStyles = _sourceDocument.GroupStyles;
		synthetic.DataLists = _sourceDocument.DataLists;
		synthetic.CollectionViews =
			_sourceDocument.CollectionViews;
		synthetic.Nodes.clear();
		synthetic.ResourceBasePath = _sourceDocument.ResourceBasePath;
		synthetic.Resources = _sourceDocument.Resources;
		synthetic.NextStableId = 2;

		std::wstring selectedKey = definition.Key;
		if (selectedKey.empty())
		{
			selectedKey = L"__cuiStaticImplicitControlTemplate"
				+ std::to_wstring(templateIndex + 1);
			synthetic.ControlTemplates[templateIndex].Key = selectedKey;
		}

		DesignerModel::DesignNode owner;
		owner.Id = 1;
		owner.Name = L"__cuiStaticTemplateOwner"
			+ std::to_wstring(templateIndex + 1);
		owner.Type = definition.TargetType;
		owner.XamlType = targetDescriptor->TypeId;
		owner.Order = 0;
		owner.Structure.ControlTemplate = selectedKey;
		synthetic.Nodes.push_back(owner);

		CuiRuntime::XamlCompiledDocument compiledTemplate;
		std::wstring compileError;
		CuiRuntime::XamlDocumentCompilationOptions compilationOptions;
		// Production output links the separately compiled framework theme.
		// Expanding Generic.xaml here would duplicate its complete visual trees
		// into every generated application and preserve the old dynamic-XAML
		// startup/size cost.
		compilationOptions.UseFrameworkTheme = dynamicWindow;
		if (!CuiRuntime::XamlDocumentCompiler::Compile(
			synthetic, compiledTemplate, compilationOptions, &compileError))
			throw std::invalid_argument(
				"Static ControlTemplate blueprint compilation failed: "
				+ WStringToString(compileError));
		const auto compiledOwner = std::find_if(
			compiledTemplate.Document.Nodes.begin(),
			compiledTemplate.Document.Nodes.end(),
			[&](const auto& node)
			{
				return !node.TemplateState.Generated
					&& node.Name == owner.Name;
			});
		if (compiledOwner == compiledTemplate.Document.Nodes.end()
			|| compiledOwner->TemplateState.AppliedControlTemplate.empty()
			|| !std::any_of(
				compiledTemplate.Document.Nodes.begin(),
				compiledTemplate.Document.Nodes.end(),
				[&](const auto& node)
				{
					return node.TemplateState.Generated
						&& node.TemplateState.Owner == owner.Name
						&& node.TemplateState.ControlTemplateRoot;
				}))
			throw std::invalid_argument(
				"Static ControlTemplate blueprint has no generated root");

		StaticControlTemplateBlueprint blueprint;
		blueprint.SourceIndex = templateIndex;
		blueprint.Document = std::move(compiledTemplate.Document);
		blueprint.OwnerName = std::move(owner.Name);
		const auto identityName = definition.Key.empty()
			? L"Implicit_" + DesignerStyleSheetUtils::UIClassName(
				definition.TargetType)
			: definition.Key;
		blueprint.VariableName =
			"__controlTemplate_"
			+ SanitizeCppIdentifier(WStringToString(identityName))
			+ "_" + std::to_string(templateIndex + 1);
		std::wstring interactionError;
		if (!CuiRuntime::XamlObjectMaterializer::
			MaterializeDeclarativeInteractions(
				definition.VisualStateGroups, definition.EventTriggers,
				_sourceDocument, blueprint.VisualStateGroups,
				blueprint.EventTriggers, &interactionError))
			throw std::invalid_argument(
				"Static ControlTemplate interaction lowering failed: "
				+ WStringToString(interactionError));
		templateBlueprints.push_back(std::move(blueprint));
	}

	struct StaticItemContainerProjection final
	{
		const DesignerModel::DesignNode* Owner = nullptr;
		std::string TemplateVariableName;
	};
	std::vector<StaticItemContainerProjection> itemContainerProjections;
	auto parentNode = [&](const DesignerModel::DesignNode& node)
		-> const DesignerModel::DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(
				_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Id == node.ParentId; });
			if (found != _sourceDocument.Nodes.end()) return &*found;
		}
		if (!node.ParentRef.empty())
		{
			const auto found = std::find_if(
				_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Name == node.ParentRef; });
			if (found != _sourceDocument.Nodes.end()) return &*found;
		}
		return nullptr;
	};
	auto visibleStyleSheet =
		[&](const DesignerModel::DesignNode& origin)
	{
		DesignerStyleSheet result = _sourceDocument.StyleSheet;
		std::vector<const DesignerModel::DesignNode*> route;
		std::unordered_set<int> visited;
		for (auto* scope = &origin;
			scope && visited.insert(scope->Id).second;
			scope = parentNode(*scope))
			route.push_back(scope);
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
			DesignerStyleSheetUtils::AppendLexicalScope(
				result, (*scope)->LocalResources);
		return result;
	};
	auto styleMatchesNode = [](
		const DesignerStyleRule& rule,
		const DesignerModel::DesignNode& node)
	{
		if (rule.HasType && rule.Type != UIClass::UI_Base
			&& rule.Type != node.Type) return false;
		if (!rule.ComponentType.Empty()
			&& (node.ComponentType.Empty()
				|| rule.ComponentType.RegistryKey()
					!= node.ComponentType.RegistryKey())) return false;
		if (rule.XamlType.Valid() && rule.XamlType != node.XamlType) return false;
		if (!rule.Id.empty()
			&& node.Properties.StyleResourceKey != rule.Id) return false;
		return true;
	};
	for (const auto& owner : _sourceDocument.Nodes)
	{
		if (owner.TemplateState.Generated
			|| owner.Structure.ItemContainerStyle.empty()) continue;
		if (!IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, owner.Type))
			throw std::invalid_argument(
				"Static ItemContainerStyle target is not an ItemsControl");

		StaticItemContainerProjection projection;
		projection.Owner = &owner;
		const bool supportsContainerTemplate =
			owner.Type == UIClass::UI_ListBox
			|| owner.Type == UIClass::UI_ComboBox
			|| owner.Type == UIClass::UI_TreeView;
		if (!supportsContainerTemplate)
		{
			itemContainerProjections.push_back(std::move(projection));
			continue;
		}

		const auto containerType = GetDefaultItemContainerType(owner.Type);
		const auto* containerDescriptor =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(containerType);
		if (containerType == UIClass::UI_Base || !containerDescriptor)
			throw std::invalid_argument(
				"Static item container type descriptor is missing");
		DesignerModel::DesignNode probe;
		probe.Name = owner.Name + L"#itemContainer";
		probe.ParentRef = owner.Name;
		probe.Type = containerType;
		probe.XamlType = containerDescriptor->TypeId;
		probe.Properties.StyleResourceKey =
			owner.Structure.ItemContainerStyle;

		DesignerStyleSheet resolved;
		std::wstring styleError;
		if (!DesignerStyleSheetUtils::ResolveInheritance(
			visibleStyleSheet(probe), resolved, &styleError))
			throw std::invalid_argument(
				"Static ItemContainerStyle inheritance failed: "
				+ WStringToString(styleError));
		const DesignerStyleRule* effectiveStyle = nullptr;
		for (auto item = resolved.Rules.rbegin();
			item != resolved.Rules.rend(); ++item)
		{
			if (!styleMatchesNode(*item, probe) || item->Id.empty()) continue;
			effectiveStyle = &*item;
			break;
		}
		if (!effectiveStyle)
			throw std::invalid_argument(
				"Static ItemContainerStyle is not visible for its container");
		const auto templateSetter = std::find_if(
			effectiveStyle->Setters.begin(), effectiveStyle->Setters.end(),
			[](const auto& setter)
			{ return setter.PropertyName == L"Template"; });
		if (templateSetter == effectiveStyle->Setters.end())
		{
			itemContainerProjections.push_back(std::move(projection));
			continue;
		}
		const auto templateKey =
			DesignerBindingUtils::Trim(templateSetter->ResourceKey);
		if (!templateSetter->UsesResource
			|| templateSetter->UsesDynamicResource
			|| templateKey.empty())
			throw std::invalid_argument(
				"Static ItemContainerStyle.Template must use StaticResource");
		const auto* definition = _sourceDocument.FindControlTemplate(
			_sourceDocument.Nodes, probe, templateKey);
		if (!definition)
			throw std::invalid_argument(
				"Static ItemContainerStyle.Template resource is missing");
		if (!definition->TargetComponentType.Empty()
			|| !IsUIClassAssignableFrom(
				definition->TargetType, containerType))
			throw std::invalid_argument(
				"Static ItemContainerStyle.Template TargetType is incompatible");

		const auto sourceDefinition = std::find_if(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[&](const auto& candidate) { return &candidate == definition; });
		if (sourceDefinition == _sourceDocument.ControlTemplates.end())
			throw std::invalid_argument(
				"Static local ItemContainerStyle.Template has no native lowering");
		const auto sourceIndex = static_cast<size_t>(
			sourceDefinition - _sourceDocument.ControlTemplates.begin());
		const auto blueprint = std::find_if(
			templateBlueprints.begin(), templateBlueprints.end(),
			[&](const auto& candidate)
			{ return candidate.SourceIndex == sourceIndex; });
		if (blueprint == templateBlueprints.end())
			throw std::invalid_argument(
				"Static ItemContainerStyle.Template factory is missing");
		projection.TemplateVariableName = blueprint->VariableName;
		itemContainerProjections.push_back(std::move(projection));
	}

	struct StaticDataListResource final
	{
		const DesignerModel::DesignDataList* Definition = nullptr;
		const DesignerModel::DesignDataTypeDefinition* ItemType = nullptr;
		std::shared_ptr<ObservableBindingList> Runtime;
		std::string VariableName;
	};
	struct StaticCompiledDataRecordClass final
	{
		const DesignerModel::DesignDataTypeDefinition* ItemType = nullptr;
		std::wstring ContextPath;
		std::vector<const DesignerDataContextProperty*> Properties;
		std::string ClassName;
	};
	struct StaticItemsPanelResource final
	{
		const DesignerModel::DesignItemsPanelTemplate* Definition = nullptr;
		std::string VariableName;
	};
	struct StaticDataTemplateResource final
	{
		const DesignerModel::DesignDataTemplate* Definition = nullptr;
		const DesignerModel::DesignDataTemplate* SourceDefinition = nullptr;
		std::string VariableName;
	};
	struct StaticGroupStyleResource final
	{
		const DesignerModel::DesignGroupStyle* Definition = nullptr;
		std::string VariableName;
		std::string HeaderTemplateVariableName;
	};
	DesignerModel::DesignDocument canonicalDataDocument = _sourceDocument;
	if (!dynamicWindow)
	{
		ValidateGeneratedBindingSourcePropertyTokens(
			canonicalDataDocument.DataContextSchema,
			"Static Window DataContext schema");
		for (const auto& dataType : canonicalDataDocument.DataTypes)
			ValidateGeneratedBindingSourcePropertyTokens(
				dataType.Properties,
				"Static DataType " + WStringToString(dataType.Name));
	}
	std::vector<StaticDataListResource> staticDataLists;
	std::unordered_map<std::wstring, std::string> staticDataListVariables;
	std::vector<StaticItemsPanelResource> staticItemsPanels;
	std::unordered_map<const DesignerModel::DesignItemsPanelTemplate*,
		std::string> staticItemsPanelVariablesByDefinition;
	std::vector<StaticDataTemplateResource> staticDataTemplates;
	std::unordered_map<std::wstring, std::string>
		staticDataTemplateVariables;
	std::unordered_map<std::wstring, std::string>
		staticImplicitDataTemplateVariables;
	std::vector<StaticGroupStyleResource> staticGroupStyles;
	std::unordered_map<std::wstring, std::string>
		staticGroupStyleVariables;
	const bool hasCanonicalLocalItemsPanels = std::any_of(
		canonicalDataDocument.Nodes.begin(), canonicalDataDocument.Nodes.end(),
		[](const auto& node)
		{
			return !node.LocalObjectResources.ItemsPanelTemplates.empty();
		});
	if (!canonicalDataDocument.DataTypes.empty()
		|| !canonicalDataDocument.DataLists.empty()
		|| !canonicalDataDocument.CollectionViews.empty()
		|| !canonicalDataDocument.ItemsPanelTemplates.empty()
		|| !canonicalDataDocument.DataTemplates.empty()
		|| !canonicalDataDocument.GroupStyles.empty()
		|| hasCanonicalLocalItemsPanels)
	{
		std::wstring dataError;
		if (!DesignerModel::DesignDataResourceUtils::ValidateAndCanonicalize(
			canonicalDataDocument, &dataError))
			throw std::invalid_argument(
				"Static data/layout resource validation failed: "
				+ WStringToString(dataError));
		size_t staticItemsPanelCount =
			canonicalDataDocument.ItemsPanelTemplates.size();
		for (const auto& node : canonicalDataDocument.Nodes)
			staticItemsPanelCount +=
				node.LocalObjectResources.ItemsPanelTemplates.size();
		staticItemsPanels.reserve(staticItemsPanelCount);
		for (size_t index = 0;
			index < canonicalDataDocument.ItemsPanelTemplates.size();
			++index)
		{
			const auto& definition =
				canonicalDataDocument.ItemsPanelTemplates[index];
			auto variable = "__itemsPanel_"
				+ SanitizeCppIdentifier(WStringToString(definition.Key))
				+ "_" + std::to_string(index + 1);
			staticItemsPanels.push_back({
				&definition, std::move(variable) });
			staticItemsPanelVariablesByDefinition.emplace(
				&definition, staticItemsPanels.back().VariableName);
		}
		for (size_t nodeIndex = 0;
			nodeIndex < canonicalDataDocument.Nodes.size(); ++nodeIndex)
		{
			const auto& node = canonicalDataDocument.Nodes[nodeIndex];
			for (size_t localIndex = 0;
				localIndex < node.LocalObjectResources.ItemsPanelTemplates.size();
				++localIndex)
			{
				const auto& definition =
					node.LocalObjectResources.ItemsPanelTemplates[localIndex];
				auto variable = "__localItemsPanel_"
					+ SanitizeCppIdentifier(WStringToString(definition.Key))
					+ "_" + std::to_string(nodeIndex + 1)
					+ "_" + std::to_string(localIndex + 1);
				staticItemsPanels.push_back({
					&definition, std::move(variable) });
				staticItemsPanelVariablesByDefinition.emplace(
					&definition, staticItemsPanels.back().VariableName);
			}
		}
		staticDataTemplates.reserve(
			canonicalDataDocument.DataTemplates.size());
		for (size_t index = 0;
			index < canonicalDataDocument.DataTemplates.size();
			++index)
		{
			const auto& definition =
				canonicalDataDocument.DataTemplates[index];
			const auto identity = definition.IsImplicit()
				? L"Implicit_" + definition.DataType
				: definition.Key;
			auto variable = "__dataTemplate_"
				+ SanitizeCppIdentifier(WStringToString(identity))
				+ "_" + std::to_string(index + 1);
			if (definition.IsImplicit())
				staticImplicitDataTemplateVariables.emplace(
					definition.DataType, variable);
			else staticDataTemplateVariables.emplace(
				definition.Key, variable);
			staticDataTemplates.push_back({
				&definition,
				index < _sourceDocument.DataTemplates.size()
					? &_sourceDocument.DataTemplates[index] : &definition,
				std::move(variable) });
		}
		staticGroupStyles.reserve(
			canonicalDataDocument.GroupStyles.size());
		for (size_t index = 0;
			index < canonicalDataDocument.GroupStyles.size();
			++index)
		{
			const auto& definition =
				canonicalDataDocument.GroupStyles[index];
			auto variable = "__groupStyle_"
				+ SanitizeCppIdentifier(WStringToString(definition.Key))
				+ "_" + std::to_string(index + 1);
			std::string headerVariable;
			if (!definition.HeaderTemplate.empty())
			{
				const auto header = staticDataTemplateVariables.find(
					definition.HeaderTemplate);
				if (header == staticDataTemplateVariables.end())
					throw std::invalid_argument(
						"Static GroupStyle HeaderTemplate has no "
						"native lowering");
				headerVariable = header->second;
			}
			else if (const auto implicit =
				staticImplicitDataTemplateVariables.find(
					std::wstring(CollectionViewGroupDataTypeName));
				implicit != staticImplicitDataTemplateVariables.end())
				headerVariable = implicit->second;
			staticGroupStyleVariables.emplace(
				definition.Key, variable);
			staticGroupStyles.push_back({
				&definition, std::move(variable),
				std::move(headerVariable) });
		}
		staticDataLists.reserve(canonicalDataDocument.DataLists.size());
		for (size_t index = 0;
			index < canonicalDataDocument.DataLists.size(); ++index)
		{
			const auto& definition =
				canonicalDataDocument.DataLists[index];
			const auto* itemType =
				canonicalDataDocument.FindDataType(definition.ItemType);
			auto runtime =
				DesignerModel::DesignDataResourceUtils::BuildRuntimeList(
					canonicalDataDocument, definition, &dataError);
			if (!itemType || !runtime)
				throw std::invalid_argument(
					"Static DataList materialization failed: "
					+ WStringToString(dataError));
			auto variable = "__dataList_"
				+ SanitizeCppIdentifier(WStringToString(definition.Key))
				+ "_" + std::to_string(index + 1);
			staticDataListVariables.emplace(definition.Key, variable);
			staticDataLists.push_back({
				&definition, itemType, std::move(runtime),
				std::move(variable) });
		}
	}

	auto staticDataParentPath = [](const std::wstring& path)
	{
		const auto separator = path.rfind(L'.');
		return separator == std::wstring::npos
			? std::wstring{} : path.substr(0, separator);
	};
	auto staticDataLeafName = [](const std::wstring& path)
	{
		const auto separator = path.rfind(L'.');
		return separator == std::wstring::npos
			? path : path.substr(separator + 1);
	};
	auto isStaticDataRecordPath = [](const std::wstring& path)
	{
		std::vector<BindingPathStep> steps;
		if (!TryParseBindingPropertyPath(path, steps) || steps.empty())
			return false;
		for (const auto& step : steps)
		{
			if (step.Kind != BindingPathStepKind::Property
				|| step.Value.empty()
				|| step.Value.find(L'.') != std::wstring::npos)
				return false;
		}
		return true;
	};
	auto staticDataPropertyCppType = [](const DesignerDataContextProperty& property)
		-> std::string
	{
		switch (property.ValueKind)
		{
		case BindingValueKind::Bool:
			return "bool";
		case BindingValueKind::NullableBool:
			return "NullableBool";
		case BindingValueKind::Int:
			return "int";
		case BindingValueKind::Int64:
			return "long long";
		case BindingValueKind::Float:
			return "float";
		case BindingValueKind::Double:
			return "double";
		case BindingValueKind::String:
			return "std::wstring";
		case BindingValueKind::Object:
			switch (property.ObjectKind)
			{
			case DesignerDataObjectKind::BindingSource:
				return "BindingSourceReference";
			case DesignerDataObjectKind::BindingList:
				return "BindingListReference";
			default:
				return "std::shared_ptr<void>";
			}
		default:
			return {};
		}
	};

	std::vector<StaticCompiledDataRecordClass> staticCompiledDataRecordClasses;
	if (!dynamicWindow)
	{
		std::map<std::uint64_t, std::wstring> dataTypeNamesByToken;
		auto validateDataTypeToken = [&](const std::wstring& name)
		{
			const auto token = GeneratedDataTypeTokenValue(name);
			const auto [existing, inserted] =
				dataTypeNamesByToken.emplace(token, name);
			if (!inserted && existing->second != name)
				throw std::invalid_argument(
					"Static data contracts contain a DataTypeToken collision");
		};
		auto validateSchemaDataTypes = [&](const DesignerDataContextSchema& schema)
		{
			for (const auto& property : schema)
			{
				if (property.ValueKind != BindingValueKind::Object) continue;
				if (property.ObjectKind == DesignerDataObjectKind::BindingList
					&& !property.ItemType.empty())
					validateDataTypeToken(property.ItemType);
				else if (property.ObjectKind
						== DesignerDataObjectKind::BindingSource
					&& !property.DataType.empty())
					validateDataTypeToken(property.DataType);
			}
		};
		validateDataTypeToken(
			std::wstring(CollectionViewGroupDataTypeName));
		validateSchemaDataTypes(canonicalDataDocument.DataContextSchema);
		for (const auto& dataType : canonicalDataDocument.DataTypes)
		{
			validateDataTypeToken(dataType.Name);
			validateSchemaDataTypes(dataType.Properties);
		}
		for (const auto& dataTemplate : canonicalDataDocument.DataTemplates)
			validateDataTypeToken(dataTemplate.DataType);
	}

	if (!dynamicWindow && !staticDataLists.empty())
	{
		std::vector<const DesignerModel::DesignDataTypeDefinition*> itemTypes;
		for (const auto& dataList : staticDataLists)
			if (std::find(itemTypes.begin(), itemTypes.end(), dataList.ItemType)
				== itemTypes.end())
				itemTypes.push_back(dataList.ItemType);

		size_t classIndex = 0;
		for (const auto* itemType : itemTypes)
		{
			std::vector<std::wstring> contexts{ L"" };
			for (const auto& property : itemType->Properties)
				if (property.ValueKind == BindingValueKind::Object
					&& property.ObjectKind
						== DesignerDataObjectKind::BindingSource)
					contexts.push_back(property.Path);
			std::sort(contexts.begin(), contexts.end(),
				[](const auto& left, const auto& right)
				{
					const auto leftDepth = static_cast<size_t>(
						std::count(left.begin(), left.end(), L'.'));
					const auto rightDepth = static_cast<size_t>(
						std::count(right.begin(), right.end(), L'.'));
					return leftDepth != rightDepth
						? leftDepth < rightDepth : left < right;
				});
			contexts.erase(std::unique(contexts.begin(), contexts.end()), contexts.end());

			for (const auto& property : itemType->Properties)
				if (!isStaticDataRecordPath(property.Path))
					throw std::invalid_argument(
						"Static DataList record paths must be dotted member names");
				else if (std::find(contexts.begin(), contexts.end(),
					staticDataParentPath(property.Path)) == contexts.end())
					throw std::invalid_argument(
						"Static DataList object hierarchy is incomplete");

			for (const auto& context : contexts)
			{
				StaticCompiledDataRecordClass recordClass;
				recordClass.ItemType = itemType;
				recordClass.ContextPath = context;
				recordClass.ClassName = "CuiGeneratedDataRecord_"
					+ SanitizeCppIdentifier(WStringToString(itemType->Name))
					+ "_" + std::to_string(++classIndex);
				for (const auto& property : itemType->Properties)
					if (staticDataParentPath(property.Path) == context)
					{
						if (staticDataPropertyCppType(property).empty())
							throw std::invalid_argument(
								"Static DataList property type has no compiled lowering");
						recordClass.Properties.push_back(&property);
					}
				std::sort(recordClass.Properties.begin(),
					recordClass.Properties.end(),
					[&](const auto* left, const auto* right)
					{
						return GeneratedBindingSourcePropertyTokenValue(
							staticDataLeafName(left->Path))
							< GeneratedBindingSourcePropertyTokenValue(
								staticDataLeafName(right->Path));
					});
				staticCompiledDataRecordClasses.push_back(
					std::move(recordClass));
			}
		}
	}

	struct StaticCollectionViewResource final
	{
		const DesignerModel::DesignCollectionViewSource* Definition = nullptr;
		const DesignerModel::DesignDataTypeDefinition* ItemType = nullptr;
		std::string VariableName;
		std::string SourceVariableName;
		std::vector<BindingValue> FilterValues;
	};
	std::vector<StaticCollectionViewResource> staticCollectionViews;
	std::unordered_map<std::wstring, std::string>
		staticCollectionViewVariables;
	staticCollectionViews.reserve(
		canonicalDataDocument.CollectionViews.size());
	for (size_t index = 0;
		index < canonicalDataDocument.CollectionViews.size(); ++index)
	{
		const auto& definition =
			canonicalDataDocument.CollectionViews[index];
		if (!definition.SourceBindingPath.empty())
			throw std::invalid_argument(
				"Static CollectionViewSource retained a Source Binding");
		const auto* sourceDefinition =
			canonicalDataDocument.FindDataList(
				definition.SourceResource);
		const auto sourceVariable = staticDataListVariables.find(
			definition.SourceResource);
		const auto* itemType = sourceDefinition
			? canonicalDataDocument.FindDataType(
				sourceDefinition->ItemType) : nullptr;
		if (!sourceDefinition
			|| sourceVariable == staticDataListVariables.end()
			|| !itemType)
			throw std::invalid_argument(
				"Static CollectionViewSource has no native DataList source");
		auto variable = "__collectionView_"
			+ SanitizeCppIdentifier(WStringToString(definition.Key))
			+ "_" + std::to_string(index + 1);
		StaticCollectionViewResource lowered{
			&definition, itemType, variable, sourceVariable->second, {} };
		lowered.FilterValues.reserve(
			definition.FilterDescriptions.size());
		for (const auto& filter : definition.FilterDescriptions)
		{
			BindingValue value;
			if (filter.Operator != CollectionFilterOperator::IsEmpty
				&& filter.Operator
					!= CollectionFilterOperator::IsNotEmpty)
			{
				const auto property = std::find_if(
					itemType->Properties.begin(),
					itemType->Properties.end(),
					[&](const auto& candidate)
					{ return candidate.Path == filter.PropertyName; });
				if (property == itemType->Properties.end())
					throw std::invalid_argument(
						"Static CollectionViewSource filter property is missing");
				DesignerStyleValueKind kind{};
				switch (property->ValueKind)
				{
				case BindingValueKind::Bool:
					kind = DesignerStyleValueKind::Bool;
					break;
				case BindingValueKind::NullableBool:
					kind = DesignerStyleValueKind::NullableBool;
					break;
				case BindingValueKind::Int:
					kind = DesignerStyleValueKind::Int;
					break;
				case BindingValueKind::Int64:
					kind = DesignerStyleValueKind::Int64;
					break;
				case BindingValueKind::Float:
					kind = DesignerStyleValueKind::Float;
					break;
				case BindingValueKind::Double:
					kind = DesignerStyleValueKind::Double;
					break;
				case BindingValueKind::String:
					kind = DesignerStyleValueKind::String;
					break;
				default:
					throw std::invalid_argument(
						"Static CollectionViewSource filter is not scalar");
				}
				std::wstring conversionError;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					{ kind, filter.Value }, value, &conversionError,
					canonicalDataDocument.ResourceBasePath,
					canonicalDataDocument.Resources))
					throw std::invalid_argument(
						"Static CollectionViewSource filter conversion failed: "
						+ WStringToString(conversionError));
			}
			lowered.FilterValues.push_back(std::move(value));
		}
		staticCollectionViewVariables.emplace(
			definition.Key, variable);
		staticCollectionViews.push_back(std::move(lowered));
	}

	std::vector<std::pair<std::wstring, std::string>>
		staticObjectResources;
	for (const auto& blueprint : templateBlueprints)
	{
		const auto& definition =
			_sourceDocument.ControlTemplates[blueprint.SourceIndex];
		if (definition.Key.empty()) continue;
		staticObjectResources.emplace_back(
			definition.Key,
			"BindingValue(ControlTemplateReference("
				+ blueprint.VariableName + "))");
	}
	for (const auto& dataList : staticDataLists)
		staticObjectResources.emplace_back(
			dataList.Definition->Key,
			"BindingValue(BindingListReference("
				+ dataList.VariableName + "))");
	for (const auto& view : staticCollectionViews)
		staticObjectResources.emplace_back(
			view.Definition->Key,
			"BindingValue(BindingListReference("
				+ view.VariableName + "))");
	for (const auto& panel : canonicalDataDocument.ItemsPanelTemplates)
	{
		const auto variable = staticItemsPanelVariablesByDefinition.find(&panel);
		if (variable == staticItemsPanelVariablesByDefinition.end())
			throw std::invalid_argument(
				"Static ItemsPanel descriptor identity is missing");
		staticObjectResources.emplace_back(
			panel.Key,
			"BindingValue(ItemsPanelTemplateReference("
				+ variable->second + "))");
	}
	for (const auto& itemTemplate : staticDataTemplates)
		if (!itemTemplate.Definition->IsImplicit())
			staticObjectResources.emplace_back(
				itemTemplate.Definition->Key,
				"BindingValue(ItemTemplateReference("
					+ itemTemplate.VariableName + "))");
	for (const auto& groupStyle : staticGroupStyles)
		staticObjectResources.emplace_back(
			groupStyle.Definition->Key,
			"BindingValue(GroupStyleReference("
				+ groupStyle.VariableName + "))");
	std::vector<std::pair<std::wstring, std::string>>
		weakStaticObjectResources;
	for (const auto& blueprint : templateBlueprints)
	{
		const auto& definition =
			_sourceDocument.ControlTemplates[blueprint.SourceIndex];
		if (definition.Key.empty()) continue;
		weakStaticObjectResources.emplace_back(
			definition.Key,
			"BindingValue(ControlTemplateReference(__weak_"
				+ blueprint.VariableName.substr(2) + ".lock()))");
	}

	// Resolve ItemsPanel StaticResource references in the dictionary where each
	// Style rule is declared, before BasedOn copies any setters into a nearer
	// lexical scope.  The generated aliases are codegen-only identities for the
	// concrete native descriptors; authored dictionary keys remain unchanged.
	std::unordered_set<std::wstring> reservedItemsPanelAliases;
	auto reserveStyleKeys = [&](const DesignerStyleSheet& sheet)
	{
		for (const auto& resource : sheet.Resources)
			if (!resource.Key.empty())
				reservedItemsPanelAliases.insert(resource.Key);
		for (const auto& rule : sheet.Rules)
			if (!rule.Id.empty()) reservedItemsPanelAliases.insert(rule.Id);
	};
	reserveStyleKeys(canonicalDataDocument.StyleSheet);
	for (const auto& [key, expression] : staticObjectResources)
	{
		(void)expression;
		if (!key.empty()) reservedItemsPanelAliases.insert(key);
	}
	for (const auto& node : canonicalDataDocument.Nodes)
	{
		reserveStyleKeys(node.LocalResources);
		for (const auto& definition
			: node.LocalObjectResources.ControlTemplates)
			if (!definition.Key.empty())
				reservedItemsPanelAliases.insert(definition.Key);
		for (const auto& definition
			: node.LocalObjectResources.DataTemplates)
			if (!definition.Key.empty())
				reservedItemsPanelAliases.insert(definition.Key);
		for (const auto& definition
			: node.LocalObjectResources.ItemsPanelTemplates)
			if (!definition.Key.empty())
				reservedItemsPanelAliases.insert(definition.Key);
		for (const auto& definition
			: node.LocalObjectResources.GroupStyles)
			if (!definition.Key.empty())
				reservedItemsPanelAliases.insert(definition.Key);
	}

	std::unordered_map<const DesignerModel::DesignItemsPanelTemplate*,
		std::wstring> staticItemsPanelAliasesByDefinition;
	staticItemsPanelAliasesByDefinition.reserve(staticItemsPanels.size());
	for (size_t index = 0; index < staticItemsPanels.size(); ++index)
	{
		std::wstring alias;
		size_t suffix = index + 1;
		do
		{
			alias = L"__cui_aot_items_panel_identity_"
				+ std::to_wstring(suffix++);
		}
		while (!reservedItemsPanelAliases.insert(alias).second);
		staticItemsPanelAliasesByDefinition.emplace(
			staticItemsPanels[index].Definition, std::move(alias));
	}

	auto bindItemsPanelSettersToDeclarationScope = [&] (
		DesignerStyleSheet& sheet,
		const DesignerModel::DesignNode* origin)
	{
		auto bindSetters = [&](auto& setters)
		{
			for (auto& setter : setters)
			{
				if (setter.PropertyName != L"ItemsPanel"
					|| !setter.UsesResource
					|| setter.UsesDynamicResource) continue;
				const auto* definition = origin
					? canonicalDataDocument.FindItemsPanelTemplate(
						canonicalDataDocument.Nodes, *origin,
						setter.ResourceKey)
					: canonicalDataDocument.FindItemsPanelTemplate(
						setter.ResourceKey);
				const auto alias = definition
					? staticItemsPanelAliasesByDefinition.find(definition)
					: staticItemsPanelAliasesByDefinition.end();
				if (alias == staticItemsPanelAliasesByDefinition.end())
					throw std::invalid_argument(
						"ItemsPanel Style setter has no declaration-scope native identity");
				setter.ResourceKey = alias->second;
			}
		};
		for (auto& rule : sheet.Rules)
		{
			bindSetters(rule.Setters);
			for (auto& trigger : rule.Triggers)
				bindSetters(trigger.Setters);
		}
	};
	bindItemsPanelSettersToDeclarationScope(
		canonicalDataDocument.StyleSheet, nullptr);
	for (auto& node : canonicalDataDocument.Nodes)
		bindItemsPanelSettersToDeclarationScope(
			node.LocalResources, &node);

	std::vector<std::pair<std::wstring, std::string>>
		documentItemsPanelAliases;
	for (const auto& panel : canonicalDataDocument.ItemsPanelTemplates)
	{
		const auto variable = staticItemsPanelVariablesByDefinition.find(&panel);
		const auto alias = staticItemsPanelAliasesByDefinition.find(&panel);
		if (variable == staticItemsPanelVariablesByDefinition.end()
			|| alias == staticItemsPanelAliasesByDefinition.end())
			throw std::invalid_argument(
				"Document ItemsPanel alias identity is missing");
		documentItemsPanelAliases.emplace_back(
			alias->second,
			"BindingValue(ItemsPanelTemplateReference("
				+ variable->second + "))");
	}

	auto panelObjectExpression = [](const std::string& variable)
	{
		return "BindingValue(ItemsPanelTemplateReference("
			+ variable + "))";
	};
	auto appendOrReplaceObjectResource = [](
		auto& resources, const std::wstring& key, std::string expression)
	{
		const auto existing = std::find_if(
			resources.begin(), resources.end(), [&](const auto& resource)
			{ return resource.first == key; });
		if (existing == resources.end())
			resources.emplace_back(key, std::move(expression));
		else existing->second = std::move(expression);
	};
	auto localItemsPanelObjectResources = [&] (
		const DesignerModel::DesignNode& origin)
	{
		using ObjectResources =
			std::vector<std::pair<std::wstring, std::string>>;
		std::pair<ObjectResources, ObjectResources> result;
		result.first = staticObjectResources;
		result.first.insert(result.first.end(),
			documentItemsPanelAliases.begin(),
			documentItemsPanelAliases.end());
		std::vector<const DesignerModel::DesignNode*> route;
		for (auto* scope = &origin; scope;)
		{
			route.push_back(scope);
			auto parent = canonicalDataDocument.Nodes.end();
			if (scope->ParentId > 0)
				parent = std::find_if(
					canonicalDataDocument.Nodes.begin(),
					canonicalDataDocument.Nodes.end(),
					[&](const auto& candidate)
					{ return candidate.Id == scope->ParentId; });
			else if (!scope->ParentRef.empty())
				parent = std::find_if(
					canonicalDataDocument.Nodes.begin(),
					canonicalDataDocument.Nodes.end(),
					[&](const auto& candidate)
					{ return candidate.Name == scope->ParentRef; });
			scope = parent == canonicalDataDocument.Nodes.end()
				? nullptr : &*parent;
		}
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
		{
			for (const auto& panel
				: (*scope)->LocalObjectResources.ItemsPanelTemplates)
			{
				const auto variable =
					staticItemsPanelVariablesByDefinition.find(&panel);
				const auto alias =
					staticItemsPanelAliasesByDefinition.find(&panel);
				if (variable == staticItemsPanelVariablesByDefinition.end())
					throw std::invalid_argument(
						"Local ItemsPanel descriptor identity is missing");
				if (alias == staticItemsPanelAliasesByDefinition.end())
					throw std::invalid_argument(
						"Local ItemsPanel alias identity is missing");
				appendOrReplaceObjectResource(
					result.first, panel.Key,
					panelObjectExpression(variable->second));
				appendOrReplaceObjectResource(
					result.first, alias->second,
					panelObjectExpression(variable->second));
				if (*scope == &origin)
					result.second.emplace_back(
						panel.Key,
						panelObjectExpression(variable->second));
			}
		}
		return result;
	};

	std::vector<std::pair<
		const DesignerModel::DesignPropertyAssignment*, std::wstring>>
		documentResourceCandidates;
	auto hasLocalResourceInScope = [&](const DesignerModel::DesignNode& origin,
		const std::wstring& key)
	{
		const auto* scope = &origin;
		std::unordered_set<int> visitedIds;
		std::unordered_set<const DesignerModel::DesignNode*> visitedNodes;
		while (scope && visitedNodes.insert(scope).second
			&& (scope->Id <= 0 || visitedIds.insert(scope->Id).second))
		{
			if (std::any_of(
				scope->LocalResources.Resources.begin(),
				scope->LocalResources.Resources.end(),
				[&](const auto& resource) { return resource.Key == key; }))
				return true;
			auto parent = _sourceDocument.Nodes.end();
			if (scope->ParentId > 0)
				parent = std::find_if(
					_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
					[&](const auto& candidate)
					{ return candidate.Id == scope->ParentId; });
			else if (!scope->ParentRef.empty())
				parent = std::find_if(
					_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
					[&](const auto& candidate)
					{ return candidate.Name == scope->ParentRef; });
			scope = parent == _sourceDocument.Nodes.end()
				? nullptr : &*parent;
		}
		return false;
	};
	auto collectDocumentResourceAssignment =
		[&](const DesignerModel::DesignPropertyAssignment& assignment,
			const DesignerModel::DesignNode* origin)
	{
		if (assignment.ResourceKey.empty()
			|| !assignment.DynamicResourceKey.empty()
			|| (origin && hasLocalResourceInScope(
				*origin, assignment.ResourceKey)))
			return;
		documentResourceCandidates.emplace_back(
			&assignment, assignment.ResourceKey);
	};
	for (const auto& [propertyName, assignment]
		: _sourceDocument.Window.Properties.Values)
	{
		(void)propertyName;
		collectDocumentResourceAssignment(assignment, nullptr);
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		for (const auto& [propertyName, assignment] : node.Properties.Values)
		{
			(void)propertyName;
			collectDocumentResourceAssignment(assignment, &node);
		}
	}
	for (const auto& definition : _sourceDocument.DataTemplates)
		for (const auto& node : definition.Template)
			for (const auto& [propertyName, assignment]
				: node.Properties.Values)
			{
				(void)propertyName;
				collectDocumentResourceAssignment(assignment, nullptr);
			}
	std::unordered_map<std::wstring, std::string> sharedDocumentResources;
	std::vector<std::pair<const DesignerStyleResource*, std::string>>
		sharedDocumentResourceDeclarations;
	for (size_t index = 0; index < _styleSheet.Resources.size(); ++index)
	{
		const auto& resource = _styleSheet.Resources[index];
		const bool referenced = std::any_of(
			documentResourceCandidates.begin(),
			documentResourceCandidates.end(),
			[&](const auto& candidate)
			{ return candidate.second == resource.Key; });
		if (!referenced) continue;
		auto variable = "__documentStaticResource_"
			+ SanitizeCppIdentifier(WStringToString(resource.Key))
			+ "_" + std::to_string(index + 1);
		sharedDocumentResources.emplace(resource.Key, variable);
		sharedDocumentResourceDeclarations.emplace_back(
			&resource, std::move(variable));
	}
	std::unordered_set<const DesignerModel::DesignPropertyAssignment*>
		sharedDocumentResourceAssignments;
	for (const auto& candidate : documentResourceCandidates)
		if (sharedDocumentResources.contains(candidate.second))
			sharedDocumentResourceAssignments.insert(candidate.first);

	std::ostringstream cpp;
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	const auto& className = identity.QualifiedGenerated;
	const auto& classLeaf = identity.GeneratedLeaf;
	auto bindingConverterExpression = [&] (
		const std::wstring& authoredName,
		const DesignerDataBinding& authoredBinding)
		-> std::string
	{
		const auto name = DesignerBindingUtils::Trim(authoredName);
		if (name.empty()) return {};
		if (dynamicWindow)
			return "BindingValueConverterRegistry::Create(L\""
				+ EscapeWStringLiteral(name) + "\")";
		for (const auto* builtIn : {
			L"BooleanNegation", L"StringIsNotEmpty", L"StringTrim" })
		{
			if (name == builtIn)
				return "GetBuiltInBindingValueConverter("
					"BuiltInBindingValueConverter::"
					+ WStringToString(name) + ")";
		}
		if (!_bindingConverterCatalog)
			throw std::invalid_argument(
				"Static Binding converter is not a built-in and no typed "
				"converter manifest was supplied");
		const auto* entry = _bindingConverterCatalog->Find(name);
		if (!entry)
			throw std::invalid_argument(
				"Static Binding converter is absent from the typed "
				"converter manifest");
		if (entry->Kind
			!= DesignerModel::BindingConverterCatalogKind::Single)
			throw std::invalid_argument(
				"Static Binding converter resolves to a Multi converter");
		if (!entry->CanConvertBack
			&& (authoredBinding.Mode == BindingMode::TwoWay
				|| authoredBinding.Mode == BindingMode::OneWayToSource))
			throw std::invalid_argument(
				"Static Binding converter does not support ConvertBack for "
				"the authored Binding mode");
		return entry->FactoryCallExpression();
	};
	auto multiBindingConverterExpression = [&] (
		const std::wstring& authoredName,
		const DesignerDataBinding& authoredBinding)
		-> std::string
	{
		const auto name = DesignerBindingUtils::Trim(authoredName);
		if (name.empty()) return {};
		if (dynamicWindow)
			return "MultiBindingValueConverterRegistry::Create(L\""
				+ EscapeWStringLiteral(name) + "\")";
		if (!_bindingConverterCatalog)
			throw std::invalid_argument(
				"Static MultiBinding converter requires a typed converter "
				"manifest entry");
		const auto* entry = _bindingConverterCatalog->Find(name);
		if (!entry)
			throw std::invalid_argument(
				"Static MultiBinding converter is absent from the typed "
				"converter manifest");
		if (entry->Kind
			!= DesignerModel::BindingConverterCatalogKind::Multi)
			throw std::invalid_argument(
				"Static MultiBinding converter resolves to a Single converter");
		if (authoredBinding.ChildBindings.size() < entry->MinimumInputCount)
			throw std::invalid_argument(
				"Static MultiBinding converter received fewer child Bindings "
				"than its typed manifest contract requires");
		if (!entry->CanConvertBack
			&& (authoredBinding.Mode == BindingMode::TwoWay
				|| authoredBinding.Mode == BindingMode::OneWayToSource))
			throw std::invalid_argument(
				"Static MultiBinding converter does not support ConvertBack "
				"for the authored Binding mode");
		return entry->FactoryCallExpression();
	};
	size_t compiledBindingPathCount = 0;
	std::map<std::uint64_t, std::wstring>
		emittedBindingSourceNamesByToken;
	auto emitCompiledBindingPath = [&] (
		const std::wstring& authoredPath,
		const DesignerDataContextSchema* initialSchema,
		std::string_view indent,
		const char* context,
		std::string* outStorageVariable,
		std::string_view firstExactDependencyProperty,
		std::string_view firstExactEndpointResolver) -> std::string
	{
		if (dynamicWindow)
			throw std::logic_error(
				"Dynamic Window must retain its authored binding path");
		std::vector<BindingPathStep> steps;
		if (!TryParseBindingPropertyPath(authoredPath, steps)
			|| steps.empty())
			throw std::invalid_argument(
				std::string(context)
				+ " has an invalid static source property path");

		struct LoweredStep final
		{
			bool ListIndex = false;
			std::wstring Property;
			std::uint32_t Index = 0;
			BindingValueKind ValueKind = BindingValueKind::Empty;
			bool CanRead = true;
			bool CanWrite = true;
			bool CanObserve = true;
		};
		std::vector<LoweredStep> lowered;
		lowered.reserve(steps.size());
		const DesignerDataContextSchema* schema = initialSchema;
		const DesignerDataContextSchema* alternateSchema = nullptr;
		std::wstring schemaPrefix;
		auto cursorKind = DesignerDataObjectKind::BindingSource;
		bool cursorContractKnown = false;
		std::wstring listItemType;
		auto findDataTypeSchema = [&](const std::wstring& name)
			-> const DesignerDataContextSchema*
		{
			if (name.empty()) return nullptr;
			const auto* type = canonicalDataDocument.FindDataType(name);
			return type ? &type->Properties : nullptr;
		};
		auto hasSchemaChildren = [](
			const DesignerDataContextSchema& source,
			const std::wstring& path)
		{
			const auto prefix =
				DesignerDataContextSchemaUtils::NormalizePath(path) + L".";
			return std::any_of(
				source.begin(), source.end(),
				[&](const auto& property)
				{
					return DesignerDataContextSchemaUtils::NormalizePath(
						property.Path).starts_with(prefix);
				});
		};

		for (size_t index = 0; index < steps.size(); ++index)
		{
			const auto& step = steps[index];
			const bool leaf = index + 1 == steps.size();
			if (step.Kind == BindingPathStepKind::Indexer
				&& !step.Value.empty()
				&& std::all_of(
					step.Value.begin(), step.Value.end(),
					[](wchar_t character)
					{
						return character >= L'0'
							&& character <= L'9';
					}))
			{
				std::uint64_t parsed = 0;
				for (const auto character : step.Value)
				{
					const auto digit = static_cast<std::uint64_t>(
						character - L'0');
					if (parsed > ((std::numeric_limits<std::uint32_t>::max)()
						- digit) / 10)
						throw std::length_error(
							std::string(context)
							+ " BindingList index exceeds the AOT limit");
					parsed = parsed * 10 + digit;
				}
				LoweredStep output;
				output.ListIndex = true;
				output.Index = static_cast<std::uint32_t>(parsed);
				output.ValueKind = BindingValueKind::Object;
				output.CanWrite = false;
				lowered.push_back(std::move(output));
				schema = findDataTypeSchema(listItemType);
				alternateSchema = nullptr;
				schemaPrefix.clear();
				cursorKind = DesignerDataObjectKind::BindingSource;
				cursorContractKnown = schema != nullptr;
				listItemType.clear();
				continue;
			}

			if (step.Kind == BindingPathStepKind::Indexer)
			{
				if (cursorKind != DesignerDataObjectKind::BindingSource
					|| !cursorContractKnown)
					throw std::invalid_argument(
						std::string(context)
						+ " has a keyed indexer without an explicit "
							"BindingSource contract");
				LoweredStep output;
				output.Property = step.Value;
				lowered.push_back(std::move(output));
				schema = nullptr;
				alternateSchema = nullptr;
				schemaPrefix.clear();
				cursorKind = leaf
					? DesignerDataObjectKind::Opaque
					: DesignerDataObjectKind::BindingSource;
				cursorContractKnown = false;
				listItemType.clear();
				continue;
			}

			const DesignerDataContextProperty* metadata = nullptr;
			bool syntheticObject = false;
			std::wstring candidatePath = schemaPrefix.empty()
				? step.Value : schemaPrefix + L"." + step.Value;
			if (schema)
			{
				metadata = DesignerDataContextSchemaUtils::Find(
					*schema, candidatePath);
				if (!metadata)
					syntheticObject = hasSchemaChildren(
						*schema, candidatePath);
			}
			if (!metadata && !syntheticObject && alternateSchema)
			{
				schema = alternateSchema;
				alternateSchema = nullptr;
				schemaPrefix.clear();
				candidatePath = step.Value;
				metadata = DesignerDataContextSchemaUtils::Find(
					*schema, candidatePath);
				if (!metadata)
					syntheticObject = hasSchemaChildren(
						*schema, candidatePath);
			}

			LoweredStep output;
			output.Property = step.Value;
			if (metadata)
			{
				output.ValueKind = metadata->ValueKind;
				output.CanRead = metadata->CanRead;
				output.CanWrite = metadata->CanWrite;
				output.CanObserve = metadata->CanObserve;
			}
			else if (syntheticObject)
			{
				output.ValueKind = BindingValueKind::Object;
				output.CanWrite = false;
			}
			lowered.push_back(std::move(output));
			schemaPrefix = candidatePath;
			alternateSchema = nullptr;
			listItemType.clear();
			if (metadata && metadata->ValueKind == BindingValueKind::Object)
			{
				cursorKind = metadata->ObjectKind;
				cursorContractKnown =
					metadata->ObjectKind
						== DesignerDataObjectKind::BindingSource
					|| metadata->ObjectKind
						== DesignerDataObjectKind::BindingList;
				if (metadata->ObjectKind
					== DesignerDataObjectKind::BindingList)
					listItemType = metadata->ItemType;
				else if (metadata->ObjectKind
					== DesignerDataObjectKind::BindingSource)
					alternateSchema =
						findDataTypeSchema(metadata->DataType);
			}
			else if (syntheticObject)
			{
				cursorKind = DesignerDataObjectKind::BindingSource;
				cursorContractKnown = true;
			}
			else
			{
				cursorKind = leaf
					? DesignerDataObjectKind::Opaque
					: DesignerDataObjectKind::BindingSource;
				cursorContractKnown = false;
			}
		}

		const auto variable = "__cuiCompiledBindingPath_"
			+ std::to_string(++compiledBindingPathCount);
		if (outStorageVariable) *outStorageVariable = variable;
		cpp << indent << "static constexpr CompiledBindingPathStep "
			<< variable << "[] =\n";
		cpp << indent << "{\n";
		for (size_t stepIndex = 0; stepIndex < lowered.size(); ++stepIndex)
		{
			const auto& step = lowered[stepIndex];
			const bool exactDependencyProperty = stepIndex == 0
				&& !step.ListIndex && !firstExactDependencyProperty.empty();
			if (exactDependencyProperty)
				cpp << indent
					<< "\t// CUI:AOT binding-path-endpoint=exact-dp\n";
			cpp << indent << "\t{ "
				<< (step.ListIndex
					? "CompiledBindingPathStepKind::ListIndex"
					: "CompiledBindingPathStepKind::Property")
				<< ", "
				<< GeneratedBindingPathCapabilitiesExpression(
					step.CanRead, step.CanWrite, step.CanObserve)
				<< ", "
				<< GeneratedBindingValueKindExpression(step.ValueKind)
				<< ", ";
			if (step.ListIndex || exactDependencyProperty) cpp << "{}";
			else
			{
				const auto token =
					GeneratedBindingSourcePropertyTokenValue(step.Property);
				const auto [found, inserted] =
					emittedBindingSourceNamesByToken.emplace(
						token, step.Property);
				if (!inserted && found->second != step.Property)
					throw std::invalid_argument(
						"Static generated Binding paths contain a "
						"BindingSourcePropertyToken collision");
				cpp << "BindingSourcePropertyToken{ " << token << "ULL }";
			}
			cpp << ", " << step.Index << "u";
			if (exactDependencyProperty)
			{
				cpp << ", +[](IBindingSource& source) noexcept -> "
					"CompiledSourceHandle { return cui::binding::"
					<< firstExactEndpointResolver << "(source, "
					<< firstExactDependencyProperty << "); }";
			}
			cpp << " },\n";
		}
		cpp << indent << "};\n";
		return "CompiledBindingPathView{ " + variable + " }";
	};

	struct StaticBindingOptionsEmission final
	{
		std::wstring ConverterName;
		std::string Fallback = "{}";
		std::string TargetNull = "{}";
		std::string ConverterParameter = "{}";
		std::string StringFormat = "{}";
		bool HasExtendedOptions = false;
	};
	auto lowerBindingOptions = [&](const DesignerDataBinding& binding)
	{
		StaticBindingOptionsEmission result;
		result.ConverterName = DesignerBindingUtils::Trim(binding.Converter);
		if (binding.FallbackValue)
			result.Fallback = GenerateStyleValueExpression(
				*binding.FallbackValue);
		if (binding.TargetNullValue)
			result.TargetNull = GenerateStyleValueExpression(
				*binding.TargetNullValue);
		if (binding.ConverterParameter)
			result.ConverterParameter = GenerateStyleValueExpression(
				*binding.ConverterParameter);
		if (binding.StringFormat)
			result.StringFormat = "std::optional<std::wstring>(L\""
				+ EscapeWStringLiteral(*binding.StringFormat) + "\")";
		result.HasExtendedOptions = binding.FallbackValue.has_value()
			|| binding.TargetNullValue.has_value()
			|| binding.ConverterParameter.has_value()
			|| binding.StringFormat.has_value();
		return result;
	};

	struct StaticBindingSourceSpec final
	{
		/** Adapter source used by the existing compiled-path Binding overload. */
		std::string AdapterExpression;
		/** DependencyObject reference used by the direct-DP endpoint fast path. */
		std::string DirectObjectExpression;
		const DesignerModel::DesignNode* DirectNode = nullptr;
		const DesignerModel::DesignComponentDefinition* DirectComponent = nullptr;
		const StaticCompiledDataRecordClass* DirectRecordClass = nullptr;
		std::string DirectRecordExpression;
		bool DirectWindow = false;
		const DesignerDataContextSchema* SourceSchema = nullptr;
		std::string Guard;
	};
	struct StaticBindingEndpointEmission final
	{
		std::string SourceOperand;
		std::string SourcePathOperand;
		std::string Guard;
		bool DirectDependencyProperty = false;
		bool DirectCompiledRecord = false;

		bool UsesDirectSource() const noexcept
		{
			return DirectDependencyProperty || DirectCompiledRecord;
		}
	};
	auto lowerBindingSourceEndpoint = [&] (
		const DesignerDataBinding& binding,
		const StaticBindingSourceSpec& source,
		std::string_view indent,
		const char* context)
	{
		StaticBindingEndpointEmission result;
		result.SourceOperand = source.AdapterExpression;
		result.Guard = source.Guard;
		result.SourcePathOperand = "L\""
			+ EscapeWStringLiteral(binding.SourceProperty) + "\"";
		if (dynamicWindow) return result;

		// ElementName/Self endpoints whose complete source path is one known
		// DependencyProperty bypass the generic compiled-path adapter entirely.
		// For a longer path, the same exact property identity is embedded in the
		// first v2 path step; later unknown/external members keep the token lane.
		std::string firstExactDependencyProperty;
		std::string firstExactEndpointResolver =
			"ResolveCompiledDependencyPropertySource";
		if (!source.DirectObjectExpression.empty()
			&& (source.DirectWindow || source.DirectNode
				|| source.DirectComponent))
		{
			std::vector<BindingPathStep> steps;
			if (TryParseBindingPropertyPath(binding.SourceProperty, steps)
				&& !steps.empty()
				&& steps.front().Kind == BindingPathStepKind::Property)
			{
				const auto sourceProperty = source.DirectWindow
					? FindKnownDependencyPropertyExpression(
						UIClass::UI_Window, steps.front().Value, false)
					: source.DirectComponent
						? FindComponentDependencyPropertyExpression(
							*source.DirectComponent,
							steps.front().Value, false)
						: FindGeneratedDependencyPropertyExpression(
							*source.DirectNode, steps.front().Value, false);
				if (!sourceProperty.empty() && steps.size() == 1)
				{
					result.SourceOperand =
						"cui::binding::MakeCompiledDependencyPropertySource("
						+ source.DirectObjectExpression + ", "
						+ sourceProperty + ")";
					result.SourcePathOperand.clear();
					result.DirectDependencyProperty = true;
					return result;
				}
				if (!sourceProperty.empty() && steps.size() > 1)
					firstExactDependencyProperty = sourceProperty;
			}
		}
		else if (binding.RelativeSource
			== DesignerBindingRelativeSource::FindAncestor)
		{
			std::vector<BindingPathStep> steps;
			if (TryParseBindingPropertyPath(binding.SourceProperty, steps)
				&& !steps.empty()
				&& steps.front().Kind == BindingPathStepKind::Property)
			{
				const auto localName = GeneratedAncestorLocalTypeName(
					binding.AncestorType);
				if (binding.AncestorTypeNamespace.empty())
				{
					UIClass ancestorType = UIClass::UI_Base;
					if (!DesignerStyleSheetUtils::TryParseUIClass(
							localName, ancestorType))
						throw std::invalid_argument(
							"Static FindAncestor type is not a native CUI class");
					firstExactDependencyProperty =
						FindKnownDependencyPropertyExpression(
							ancestorType, steps.front().Value, false);
				}
				else
				{
					const auto* component = _sourceDocument.FindComponent(
						binding.AncestorTypeNamespace, localName);
					if (!component)
						throw std::invalid_argument(
							"Static FindAncestor component type is not declared");
					firstExactDependencyProperty =
						FindComponentDependencyPropertyExpression(
							*component, steps.front().Value, false);
				}
				if (!firstExactDependencyProperty.empty())
					firstExactEndpointResolver =
						"ResolveCompiledFindAncestorDependencyPropertySource";
			}
		}

		// Embedded AOT data records expose their exact generated thunk-table
		// entry. The one dynamic_cast occurs while attaching the Binding; reads,
		// writes and notifications then bypass token lookup. A native source with
		// the same public schema falls back to the explicit token adapter handle.
		if (source.DirectRecordClass
			&& !source.DirectRecordExpression.empty())
		{
			std::vector<BindingPathStep> steps;
			if (TryParseBindingPropertyPath(binding.SourceProperty, steps)
				&& steps.size() == 1
				&& steps.front().Kind == BindingPathStepKind::Property)
			{
				const auto found = std::find_if(
					source.DirectRecordClass->Properties.begin(),
					source.DirectRecordClass->Properties.end(),
					[&](const auto* property)
					{
						return property
							&& staticDataLeafName(property->Path)
								== steps.front().Value;
					});
				if (found != source.DirectRecordClass->Properties.end())
				{
					std::string storageVariable;
					result.SourcePathOperand = emitCompiledBindingPath(
						binding.SourceProperty, source.SourceSchema, indent,
						context, &storageVariable, {}, {});
					const auto propertyIndex = static_cast<size_t>(
						std::distance(
							source.DirectRecordClass->Properties.begin(), found));
					result.SourceOperand =
						"cui::binding::MakeCompiledRecordPropertySource<"
						+ source.DirectRecordClass->ClassName
						+ ">(" + source.DirectRecordExpression + ", "
						+ std::to_string(propertyIndex) + ", "
						+ storageVariable + "[0])";
					result.SourcePathOperand.clear();
					result.DirectCompiledRecord = true;
					return result;
				}
			}
		}

		result.SourcePathOperand = emitCompiledBindingPath(
			binding.SourceProperty, source.SourceSchema, indent, context, nullptr,
			firstExactDependencyProperty, firstExactEndpointResolver);
		return result;
	};

	auto emitStaticMultiBinding = [&] (
		const DesignerDataBinding& binding,
		const std::string& targetPointer,
		const std::string& targetPropertyOperand,
		auto&& resolveEndpoint,
		std::string_view indent,
		const char* context,
		std::string_view attachedVariable)
	{
		if (!binding.IsMultiBinding())
			throw std::logic_error(
				"Static MultiBinding emitter received an ordinary Binding");
		const std::string indentText(indent);
		cpp << indentText << "std::vector<MultiBindingSource> "
			"cuiMultiSources;\n";
		for (size_t childIndex = 0;
			childIndex < binding.ChildBindings.size(); ++childIndex)
		{
			const auto& child = binding.ChildBindings[childIndex];
			if (child.IsMultiBinding())
				throw std::invalid_argument(
					std::string(context)
					+ " contains a nested MultiBinding");
			const auto childOptions = lowerBindingOptions(child);
			const auto childEndpoint = resolveEndpoint(
				child, indent, context);
			if (!childEndpoint.Guard.empty())
				throw std::invalid_argument(
					std::string(context)
					+ " has a child source requiring a preconstruction guard");
			const auto childSuffix = std::to_string(childIndex + 1);
			const auto childVar = "cuiMultiSource" + childSuffix;
			const auto childConverterVar =
				"cuiMultiChildConverter" + childSuffix;
			if (!childOptions.ConverterName.empty())
				cpp << indentText << "auto " << childConverterVar << " = "
					<< bindingConverterExpression(
						childOptions.ConverterName, child) << ";\n";
			if (childEndpoint.UsesDirectSource())
				cpp << indentText
					<< (childEndpoint.DirectCompiledRecord
						? "// CUI:AOT binding-source=direct-record\n"
						: "// CUI:AOT binding-source=direct-dp\n");
			cpp << indentText << "MultiBindingSource " << childVar
				<< "(" << childEndpoint.SourceOperand << ", ";
			if (!childEndpoint.UsesDirectSource())
				cpp << childEndpoint.SourcePathOperand << ", ";
			cpp << (childOptions.ConverterName.empty()
				? std::string("{}") : childConverterVar)
				<< ", " << childOptions.Fallback
				<< ", " << childOptions.TargetNull
				<< ", " << childOptions.ConverterParameter
				<< ", " << childOptions.StringFormat << ");\n";
			cpp << indentText << childVar << ".Mode = "
				<< BindingModeToExpr(child.Mode) << ";\n";
			cpp << indentText << childVar << ".UpdateMode = "
				<< DataSourceUpdateModeToExpr(child.UpdateMode) << ";\n";
			cpp << indentText << "cuiMultiSources.push_back(std::move("
				<< childVar << "));\n";
		}

		const auto multiOptions = lowerBindingOptions(binding);
		const auto& multiConverterName = multiOptions.ConverterName;
		if (!multiConverterName.empty())
			cpp << indentText << "auto cuiMultiConverter = "
				<< multiBindingConverterExpression(
					multiConverterName, binding)
				<< ";\n";
		cpp << indentText << "const bool " << attachedVariable << " = "
			<< targetPointer << "->DataBindings.AddMulti("
			<< targetPropertyOperand << ", std::move(cuiMultiSources), "
			<< BindingModeToExpr(binding.Mode) << ", "
			<< DataSourceUpdateModeToExpr(binding.UpdateMode) << ", ";
		if (multiConverterName.empty()) cpp << "{}";
		else cpp << "cuiMultiConverter";
		cpp << ", " << multiOptions.Fallback
			<< ", " << multiOptions.TargetNull
			<< ", " << multiOptions.ConverterParameter
			<< ", " << multiOptions.StringFormat
			<< ") != nullptr;\n";
	};

	// 包含头文件
	cpp << "#include \"" << generatedHeaderBaseName << ".g.h\"\n";
	if (!dynamicWindow && _bindingConverterCatalog)
		for (const auto& include : _bindingConverterCatalog->Includes())
			cpp << "#include \"" << include << "\"\n";
	// Attached-property calls are emitted directly instead of going through
	// the name-based dependency-property lookup. Their owners need not appear
	// as visual nodes in every valid authored tree, so include them explicitly.
	cpp << "#include \"Canvas.h\"\n";
	cpp << "#include \"Layout/Grid.h\"\n";
	cpp << "#include \"Layout/DockPanel.h\"\n";
	cpp << "#include \"Layout/RelativePanel.h\"\n";
	std::set<std::string> templateImplementationIncludes;
	for (const auto& blueprint : templateBlueprints)
	{
		templateImplementationIncludes.insert(GetIncludeForType(
			_sourceDocument.ControlTemplates[
				blueprint.SourceIndex].TargetType));
		for (const auto& node : blueprint.Document.Nodes)
			if (node.TemplateState.Generated)
				templateImplementationIncludes.insert(
					GetIncludeForType(node.Type));
	}
	for (const auto& itemTemplate : staticDataTemplates)
		for (const auto& node : itemTemplate.SourceDefinition->Template)
			templateImplementationIncludes.insert(
				GetIncludeForType(node.Type));
	for (const auto& include : templateImplementationIncludes)
		cpp << "#include \"" << include << "\"\n";
	cpp << "#include \"ControlTemplate.h\"\n";
	if (!staticDataLists.empty())
	{
		cpp << "#include \"BindingList.h\"\n";
		if (!dynamicWindow)
			cpp << "#include \"CompiledBindingRecord.h\"\n";
	}
	if (!staticDataTemplates.empty())
	{
		cpp << "#include \"ItemTemplate.h\"\n";
		cpp << "#include \"RelativeSource.h\"\n";
		cpp << "#include \"TreeView.h\"\n";
	}
	if (!staticGroupStyles.empty())
		cpp << "#include \"GroupStyle.h\"\n";
	if (dynamicWindow)
	{
		cpp << "#include \"BindingConverterRegistry.h\"\n";
		cpp << "#include \"XamlInfrastructure.h\"\n";
	}
	cpp << "#include \"DependencyPropertyInfrastructure.h\"\n";
	cpp << "#include \"StyleInfrastructure.h\"\n";
	cpp << "#include \"TemplateInfrastructure.h\"\n";
	cpp << "#include \"TreeInfrastructure.h\"\n";
	if (dynamicWindow)
		cpp << "#include \"XamlFrameworkTheme.h\"\n";
	else if (!frameworkThemeProgram)
		cpp << "#include \"CuiGeneratedFrameworkTheme.h\"\n";
	cpp << "#include \"HeaderedContentControl.h\"\n";
	cpp << "#include \"HeaderedItemsControl.h\"\n";
	// InitializeComponent always owns a ControlStyleSheet, even when the
	// authored dictionary contains only native object resources.
	cpp << "#include \"Style.h\"\n";
	auto usesImageValue = [](const DesignerStyleValue& value)
	{
		return value.Kind == DesignerStyleValueKind::ImageSource
			|| (value.Kind == DesignerStyleValueKind::Brush
				&& value.ObjectValue.is_object()
				&& value.ObjectValue.value("type", std::string{}) == "image");
	};
	auto animationUsesImage = [&](const DesignerVisualStateAnimation& animation)
	{
		if ((animation.HasFrom && usesImageValue(animation.From))
			|| (animation.HasTo && usesImageValue(animation.To))
			|| (animation.HasBy && usesImageValue(animation.By)))
			return true;
		return std::any_of(
			animation.KeyFrames.begin(), animation.KeyFrames.end(),
			[&](const auto& frame) { return usesImageValue(frame.Value); });
	};
	auto actionsUseImage = [&](const auto& actions)
	{
		for (const auto& action : actions)
			if (std::any_of(
				action.Animations.begin(), action.Animations.end(),
				animationUsesImage)) return true;
		return false;
	};
	auto styleSheetUsesImage = [&](const DesignerStyleSheet& sheet)
	{
		if (std::any_of(sheet.Resources.begin(), sheet.Resources.end(),
			[&](const auto& resource)
			{ return usesImageValue(resource.Value); })) return true;
		for (const auto& rule : sheet.Rules)
		{
			if (std::any_of(rule.Setters.begin(), rule.Setters.end(),
				[&](const auto& setter)
				{
						return !setter.UsesResource
							&& usesImageValue(setter.Literal);
					})) return true;
			if (actionsUseImage(rule.EnterActions)
				|| actionsUseImage(rule.ExitActions)) return true;
			for (const auto& trigger : rule.Triggers)
			{
				if (std::any_of(
					trigger.Setters.begin(), trigger.Setters.end(),
					[&](const auto& setter)
					{
						return !setter.UsesResource
							&& usesImageValue(setter.Literal);
					})) return true;
				if (actionsUseImage(trigger.EnterActions)
					|| actionsUseImage(trigger.ExitActions)) return true;
			}
		}
		return false;
	};
	auto propertiesUseImage = [&](const auto& properties)
	{
		return std::any_of(
			properties.Values.begin(), properties.Values.end(),
			[&](const auto& property)
			{ return usesImageValue(property.second.Value); });
	};
	bool usesResources = styleSheetUsesImage(_styleSheet)
		|| propertiesUseImage(_sourceDocument.Window.Properties)
		|| std::any_of(
			templateBlueprints.begin(), templateBlueprints.end(),
			[](const auto& blueprint)
			{
				return !blueprint.VisualStateGroups.empty()
					|| !blueprint.EventTriggers.empty();
			})
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& node)
			{
				return propertiesUseImage(node.Properties)
					|| styleSheetUsesImage(node.LocalResources);
			})
		|| std::any_of(
			templateBlueprints.begin(), templateBlueprints.end(),
			[&](const auto& blueprint)
			{
				return std::any_of(
					blueprint.Document.Nodes.begin(),
					blueprint.Document.Nodes.end(),
					[&](const auto& node)
					{
						return node.TemplateState.Generated
							&& (propertiesUseImage(node.Properties)
								|| styleSheetUsesImage(
									node.LocalResources));
					});
			})
		|| std::any_of(
			staticDataTemplates.begin(), staticDataTemplates.end(),
			[&](const auto& itemTemplate)
			{
				return std::any_of(
					itemTemplate.SourceDefinition->Template.begin(),
					itemTemplate.SourceDefinition->Template.end(),
					[&](const auto& node)
					{ return propertiesUseImage(node.Properties); });
			});
	if (usesResources) cpp << "#include \"Resource.h\"\n";
	cpp << "#include \"Utils.h\"\n";
	cpp << "#include <array>\n";
	cpp << "#include <functional>\n";
	cpp << "#include <memory>\n";
	cpp << "#include <stdexcept>\n";
	cpp << "#include <type_traits>\n";
	cpp << "#include <utility>\n";
	cpp << "#include <vector>\n\n";

	if (!templateBlueprints.empty() || !staticDataTemplates.empty()
		|| !staticCompiledDataRecordClasses.empty()
		|| !sharedDocumentResourceDeclarations.empty())
	{
		cpp << "namespace\n{\n";
		if (!sharedDocumentResourceDeclarations.empty())
		{
			cpp << "\ttemplate<typename TValue>\n";
			cpp << "\tTValue CuiGeneratedBindingValueAs("
				"const BindingValue& value)\n";
			cpp << "\t{\n";
			cpp << "\t\tTValue result{};\n";
			cpp << "\t\tif (value.TryGet(result)) return result;\n";
			cpp << "\t\tif constexpr (std::is_same_v<TValue, "
				"cui::drawing::Brush>)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tD2D1_COLOR_F color{};\n";
			cpp << "\t\t\tif (value.TryGet(color))\n";
			cpp << "\t\t\t\treturn cui::drawing::"
				"MakeSolidColorBrush(color);\n";
			cpp << "\t\t}\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated StaticResource type mismatch\");\n";
			cpp << "\t}\n";
		}
		if (!staticCompiledDataRecordClasses.empty())
		{
			cpp << "\t// AOT DataList records keep typed fields inline and share one\n";
			cpp << "\t// process-static token/metadata/thunk table per record shape.\n";
			for (const auto& recordClass : staticCompiledDataRecordClasses)
			{
				cpp << "\tclass " << recordClass.ClassName
					<< " final : public CompiledBindingRecord\n";
				cpp << "\t{\n";
				cpp << "\tpublic:\n";
				cpp << "\t\texplicit " << recordClass.ClassName << "(";
				for (size_t propertyIndex = 0;
					propertyIndex < recordClass.Properties.size(); ++propertyIndex)
				{
					if (propertyIndex != 0) cpp << ", ";
					cpp << staticDataPropertyCppType(
						*recordClass.Properties[propertyIndex])
						<< " value" << propertyIndex;
				}
				cpp << ")\n";
				cpp << "\t\t\t: CompiledBindingRecord(Properties())";
				for (size_t propertyIndex = 0;
					propertyIndex < recordClass.Properties.size(); ++propertyIndex)
					cpp << ",\n\t\t\t  _value" << propertyIndex
						<< "(std::move(value" << propertyIndex << "))";
				cpp << " {}\n\n";
				cpp << "\tprivate:\n";
				cpp << "\t\tstatic std::span<const CompiledBindingRecordProperty> "
					"Properties()\n";
				cpp << "\t\t{\n";
				if (recordClass.Properties.empty())
				{
					cpp << "\t\t\tstatic const std::array<"
						"CompiledBindingRecordProperty, 0> values{};\n";
				}
				else
				{
					cpp << "\t\t\tstatic const std::array<"
						"CompiledBindingRecordProperty, "
						<< recordClass.Properties.size() << "> values\n";
					cpp << "\t\t\t{{\n";
					for (size_t propertyIndex = 0;
						propertyIndex < recordClass.Properties.size(); ++propertyIndex)
					{
						const auto& property =
							*recordClass.Properties[propertyIndex];
						const auto cppType = staticDataPropertyCppType(property);
						cpp << "\t\t\t\t{ "
							<< GeneratedBindingSourcePropertyTokenExpression(
								staticDataLeafName(property.Path)) << ", "
							<< GeneratedBindingValueKindExpression(property.ValueKind)
							<< ", std::type_index(typeid(" << cppType << ")), "
							<< (property.CanRead ? "true" : "false") << ", "
							<< (property.CanWrite ? "true" : "false") << ", "
							<< (property.CanObserve ? "true" : "false") << ",\n";
						if (property.CanRead)
						{
							cpp << "\t\t\t\t\t+[](const CompiledBindingRecord& "
								"record, BindingValue& out)\n";
							cpp << "\t\t\t\t\t{\n";
							cpp << "\t\t\t\t\t\tconst auto& typed = "
								"static_cast<const " << recordClass.ClassName
								<< "&>(record);\n";
							cpp << "\t\t\t\t\t\tout = BindingValue(typed._value"
								<< propertyIndex << ");\n";
							cpp << "\t\t\t\t\t\treturn true;\n";
							cpp << "\t\t\t\t\t},\n";
						}
						else cpp << "\t\t\t\t\tnullptr,\n";
						if (property.CanWrite)
						{
							cpp << "\t\t\t\t\t+[](CompiledBindingRecord& record, "
								"const BindingValue& value)\n";
							cpp << "\t\t\t\t\t{\n";
							cpp << "\t\t\t\t\t\t" << cppType << " next{};\n";
							cpp << "\t\t\t\t\t\tif (!value.TryGet(next))\n";
							cpp << "\t\t\t\t\t\t\treturn "
								"CompiledBindingRecordWriteResult::Failed;\n";
							cpp << "\t\t\t\t\t\tauto& typed = static_cast<"
								<< recordClass.ClassName << "&>(record);\n";
							cpp << "\t\t\t\t\t\tif (typed._value" << propertyIndex
								<< " == next)\n";
							cpp << "\t\t\t\t\t\t\treturn "
								"CompiledBindingRecordWriteResult::Unchanged;\n";
							cpp << "\t\t\t\t\t\ttyped._value" << propertyIndex
								<< " = std::move(next);\n";
							cpp << "\t\t\t\t\t\treturn "
								"CompiledBindingRecordWriteResult::Changed;\n";
							cpp << "\t\t\t\t\t}\n";
						}
						else cpp << "\t\t\t\t\tnullptr\n";
						cpp << "\t\t\t\t},\n";
					}
					cpp << "\t\t\t}};\n";
				}
				cpp << "\t\t\treturn std::span<const "
					"CompiledBindingRecordProperty>{ values };\n";
				cpp << "\t\t}\n\n";
				for (size_t propertyIndex = 0;
					propertyIndex < recordClass.Properties.size(); ++propertyIndex)
					cpp << "\t\t" << staticDataPropertyCppType(
						*recordClass.Properties[propertyIndex])
						<< " _value" << propertyIndex << ";\n";
				cpp << "\t};\n\n";
			}
		}
		if (!templateBlueprints.empty())
		{
			cpp << "\tclass CuiGeneratedControlTemplate final\n";
		cpp << "\t\t: public IControlTemplate,\n";
		cpp << "\t\t  public std::enable_shared_from_this<"
			"CuiGeneratedControlTemplate>\n";
		cpp << "\t{\n";
		cpp << "\tpublic:\n";
		cpp << "\t\tusing ApplyCallback = std::function<bool(\n";
		cpp << "\t\t\tControl&, std::wstring*)>;\n";
		cpp << "\t\tusing HostFactory = std::function<"
			"std::unique_ptr<Control>()>;\n\n";
		cpp << "\t\tCuiGeneratedControlTemplate(\n";
		cpp << "\t\t\tUIClass targetType,\n";
		cpp << "\t\t\tstd::wstring identity,\n";
		cpp << "\t\t\tHostFactory hostFactory)\n";
		cpp << "\t\t\t: _targetType(targetType),\n";
		cpp << "\t\t\t  _identity(std::move(identity)),\n";
		cpp << "\t\t\t  _hostFactory(std::move(hostFactory)) {}\n\n";
		cpp << "\t\tvoid SetApplyCallback(ApplyCallback value)\n";
		cpp << "\t\t{\n\t\t\t_apply = std::move(value);\n\t\t}\n\n";
		cpp << "\t\tUIClass TargetType() const noexcept override\n";
		cpp << "\t\t{\n\t\t\treturn _targetType;\n\t\t}\n\n";
		cpp << "\t\tbool Apply(Control& owner,\n";
		cpp << "\t\t\tstd::wstring* outError = nullptr) const override\n";
		cpp << "\t\t{\n";
		cpp << "\t\t\tif (!IsUIClassAssignableFrom("
			"_targetType, owner.Type()))\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate TargetType "
			"与宿主不兼容：\" + _identity;\n";
		cpp << "\t\t\t\treturn false;\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\tif (!_apply)\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate 尚未完成初始化：\""
			" + _identity;\n";
		cpp << "\t\t\t\treturn false;\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\treturn _apply(owner, outError);\n";
		cpp << "\t\t}\n\n";
		cpp << "\t\tstd::unique_ptr<Control> Build(\n";
		cpp << "\t\t\tstd::wstring* outError = nullptr) const override\n";
		cpp << "\t\t{\n";
		cpp << "\t\t\tauto owner = _hostFactory ? _hostFactory() : nullptr;\n";
		cpp << "\t\t\tif (!owner)\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate 无法构造宿主：\""
			" + _identity;\n";
		cpp << "\t\t\t\treturn {};\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\tauto self = std::static_pointer_cast<"
			"const IControlTemplate>(shared_from_this());\n";
		cpp << "\t\t\tif (!cui::framework::TemplateAccess::SetTemplate(\n";
		cpp << "\t\t\t\t*owner, ControlTemplateReference(std::move(self)),\n";
		cpp << "\t\t\t\tDependencyPropertyValueSource::Local))\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate 无法写入宿主：\""
			" + _identity;\n";
		cpp << "\t\t\t\treturn {};\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\t(void)owner->ApplyTemplate();\n";
		cpp << "\t\t\tif (!cui::framework::TemplateAccess::"
			"GetTemplateRoot(*owner)\n";
		cpp << "\t\t\t\t|| !owner->LastTemplateError().empty())\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError = "
			"owner->LastTemplateError().empty()\n";
		cpp << "\t\t\t\t\t? L\"生成的 ControlTemplate 未生成视觉根：\""
			" + _identity\n";
		cpp << "\t\t\t\t\t: owner->LastTemplateError();\n";
		cpp << "\t\t\t\treturn {};\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\tif (outError) outError->clear();\n";
		cpp << "\t\t\treturn owner;\n";
		cpp << "\t\t}\n\n";
		cpp << "\tprivate:\n";
		cpp << "\t\tUIClass _targetType = UIClass::UI_Base;\n";
		cpp << "\t\tstd::wstring _identity;\n";
		cpp << "\t\tHostFactory _hostFactory;\n";
			cpp << "\t\tApplyCallback _apply;\n";
			cpp << "\t};\n";
		}
		if (!staticDataTemplates.empty())
		{
			cpp << "\tclass CuiGeneratedItemTemplate final : public IItemTemplate\n";
			cpp << "\t{\n";
			cpp << "\tpublic:\n";
			cpp << "\t\tusing BuildCallback = std::function<std::unique_ptr<Control>(\n";
			cpp << "\t\t\tconst BindingSourceReference&, size_t, std::wstring*)>;\n";
			cpp << "\t\tusing ChildSourceCallback = std::function<bool(\n";
			cpp << "\t\t\tconst BindingSourceReference&, BindingListReference&, std::wstring*)>;\n";
			cpp << "\t\tusing ObserveCallback = std::function<BindingPathObservation(\n";
			cpp << "\t\t\tconst BindingSourceReference&, std::function<void()>)>;\n\n";
			cpp << "\t\tCuiGeneratedItemTemplate(\n";
			cpp << "\t\t\t"
				<< (dynamicWindow ? "std::wstring" : "DataTypeToken")
				<< " dataType,\n";
			cpp << "\t\t\tbool hierarchical,\n";
			cpp << "\t\t\tBuildCallback build,\n";
			cpp << "\t\t\tChildSourceCallback childSource = {},\n";
			cpp << "\t\t\tObserveCallback observe = {})\n";
			cpp << "\t\t\t: _dataType("
				<< (dynamicWindow ? "std::move(dataType)" : "dataType")
				<< "),\n";
			cpp << "\t\t\t  _hierarchical(hierarchical),\n";
			cpp << "\t\t\t  _build(std::move(build)),\n";
			cpp << "\t\t\t  _childSource(std::move(childSource)),\n";
			cpp << "\t\t\t  _observe(std::move(observe)) {}\n\n";
			if (dynamicWindow)
			{
				cpp << "\t\tconst std::wstring& DataTypeName() const noexcept override\n";
				cpp << "\t\t{\n\t\t\treturn _dataType;\n\t\t}\n\n";
			}
			else
			{
				cpp << "\t\tDataTypeToken GetDataTypeToken() const noexcept override\n";
				cpp << "\t\t{\n\t\t\treturn _dataType;\n\t\t}\n\n";
			}
			cpp << "\t\tbool IsHierarchical() const noexcept override\n";
			cpp << "\t\t{\n\t\t\treturn _hierarchical;\n\t\t}\n\n";
			cpp << "\t\tstd::unique_ptr<Control> Build(\n";
			cpp << "\t\t\tconst BindingSourceReference& item,\n";
			cpp << "\t\t\tsize_t index,\n";
			cpp << "\t\t\tstd::wstring* outError = nullptr) const override\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tif (_build) return _build(item, index, outError);\n";
			cpp << "\t\t\tif (outError) *outError = L\"生成的 DataTemplate 尚未初始化。\";\n";
			cpp << "\t\t\treturn {};\n";
			cpp << "\t\t}\n\n";
			cpp << "\t\tbool TryGetVisualChildItemsSource(\n";
			cpp << "\t\t\tconst BindingSourceReference& item,\n";
			cpp << "\t\t\tBindingListReference& out,\n";
			cpp << "\t\t\tstd::wstring* outError = nullptr) const override\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tout = {};\n";
			cpp << "\t\t\tif (_childSource) return _childSource(item, out, outError);\n";
			cpp << "\t\t\tif (outError) outError->clear();\n";
			cpp << "\t\t\treturn true;\n";
			cpp << "\t\t}\n\n";
			cpp << "\t\tBindingPathObservation ObserveChildItemsSource(\n";
			cpp << "\t\t\tconst BindingSourceReference& item,\n";
			cpp << "\t\t\tstd::function<void()> changed) const override\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\treturn _observe\n";
			cpp << "\t\t\t\t? _observe(item, std::move(changed))\n";
			cpp << "\t\t\t\t: BindingPathObservation{};\n";
			cpp << "\t\t}\n\n";
			cpp << "\tprivate:\n";
			cpp << "\t\t"
				<< (dynamicWindow ? "std::wstring" : "DataTypeToken")
				<< " _dataType;\n";
			cpp << "\t\tbool _hierarchical = false;\n";
			cpp << "\t\tBuildCallback _build;\n";
			cpp << "\t\tChildSourceCallback _childSource;\n";
			cpp << "\t\tObserveCallback _observe;\n";
			cpp << "\t};\n";
		}
		cpp << "}\n\n";
	}

	if (!dynamicWindow && !frameworkThemeProgram)
	{
		for (const auto& component : _sourceDocument.Components)
		{
			const auto componentLeaf = GetComponentClassName(component);
			const auto componentClass = identity.NamespaceName.empty()
				? componentLeaf
				: identity.NamespaceName + "::" + componentLeaf;
			const auto baseClass = GetControlTypeName(component.BaseType);
			const bool hasInheritedProperties = std::any_of(
				component.Properties.begin(), component.Properties.end(),
				[](const auto& property)
				{
					return HasDependencyPropertyFlag(
						property.Flags, DependencyPropertyFlags::Inherits);
				});

			cpp << "ComponentTypeToken " << componentClass
				<< "::ComponentTypeId() noexcept\n";
			cpp << "{\n";
			cpp << "\treturn " << GeneratedComponentTypeTokenExpression(
				component.Type.XamlNamespace, component.Type.XamlName) << ";\n";
			cpp << "}\n\n";

			for (const auto& property : component.Properties)
			{
				const auto propertyName = SanitizeCppIdentifier(
					WStringToString(property.Name));
				const auto* cppType =
					ComponentValueCppType(property.DefaultValue.Kind);
				if (!cppType)
					throw std::invalid_argument(
						"Generated component property type is unsupported");

				const DesignerStyleValue* defaultSource =
					&property.DefaultValue;
				if (!property.DefaultResourceKey.empty())
				{
					const auto resource = std::find_if(
						_styleSheet.Resources.begin(),
						_styleSheet.Resources.end(),
						[&](const auto& candidate)
						{
							return candidate.Key
								== property.DefaultResourceKey;
						});
					if (resource == _styleSheet.Resources.end())
						throw std::invalid_argument(
							"Generated component default resource is missing");
					defaultSource = &resource->Value;
				}
				const auto defaultExpression = UnwrapBindingValue(
					GenerateStyleValueExpression(*defaultSource));
				if (defaultExpression.empty())
					throw std::invalid_argument(
						"Generated component default value cannot be lowered");

				if (property.IsReadOnly)
				{
					cpp << "const DependencyPropertyKey& "
						<< componentClass << "::" << propertyName
						<< "PropertyKey()\n";
					cpp << "{\n";
					cpp << "\t// CUI:AOT dependency-property=static\n";
					cpp << "\tstatic const auto value = []\n";
					cpp << "\t{\n";
					cpp << "\t\tDependencyPropertyOptions<"
						<< componentLeaf << ", " << cppType
						<< "> options;\n";
					cpp << "\t\toptions.DefaultValue = "
						<< defaultExpression << ";\n";
					cpp << "\t\toptions.Flags = "
						"static_cast<DependencyPropertyFlags>("
						<< static_cast<unsigned int>(property.Flags)
						<< ");\n";
					cpp << "\t\toptions.DefaultUpdateMode = "
						<< DataSourceUpdateModeToExpr(
							property.DefaultUpdateMode) << ";\n";
					cpp << "\t\treturn DependencyPropertyRegistry::"
						"RegisterReadOnlyStatic<" << componentLeaf << ", "
						<< cppType << ">(\n";
					cpp << "#if CUI_ENABLE_DYNAMIC_XAML\n";
					cpp << "\t\t\tL\""
						<< EscapeWStringLiteral(property.Name) << "\",\n";
					cpp << "#else\n";
					cpp << "\t\t\t// CUI:AOT dependency-property-identity=token\n";
					cpp << "\t\t\t"
						<< GeneratedBindingSourcePropertyTokenExpression(
							property.Name) << ",\n";
					cpp << "#endif\n";
					cpp << "\t\t\tstd::move(options));\n";
					cpp << "\t}();\n";
					cpp << "\treturn *value;\n";
					cpp << "}\n\n";
				}

				cpp << "const DependencyProperty& " << componentClass
					<< "::" << propertyName << "Property()\n";
				cpp << "{\n";
				if (property.IsReadOnly)
				{
					cpp << "\treturn " << propertyName
						<< "PropertyKey().Property();\n";
				}
				else
				{
					cpp << "\t// CUI:AOT dependency-property=static\n";
					cpp << "\tstatic const auto value = []\n";
					cpp << "\t{\n";
					cpp << "\t\tDependencyPropertyOptions<"
						<< componentLeaf << ", " << cppType
						<< "> options;\n";
					cpp << "\t\toptions.DefaultValue = "
						<< defaultExpression << ";\n";
					cpp << "\t\toptions.Flags = "
						"static_cast<DependencyPropertyFlags>("
						<< static_cast<unsigned int>(property.Flags)
						<< ");\n";
					cpp << "\t\toptions.DefaultUpdateMode = "
						<< DataSourceUpdateModeToExpr(
							property.DefaultUpdateMode) << ";\n";
					cpp << "\t\treturn DependencyPropertyRegistry::"
						"RegisterStatic<" << componentLeaf << ", "
						<< cppType << ">(\n";
					cpp << "#if CUI_ENABLE_DYNAMIC_XAML\n";
					cpp << "\t\t\tL\""
						<< EscapeWStringLiteral(property.Name) << "\",\n";
					cpp << "#else\n";
					cpp << "\t\t\t// CUI:AOT dependency-property-identity=token\n";
					cpp << "\t\t\t"
						<< GeneratedBindingSourcePropertyTokenExpression(
							property.Name) << ",\n";
					cpp << "#endif\n";
					cpp << "\t\t\tstd::move(options));\n";
					cpp << "\t}();\n";
					cpp << "\tif (!value) throw std::logic_error("
						"\"Generated component property registration failed\");\n";
					cpp << "\treturn *value;\n";
				}
				cpp << "}\n\n";

				cpp << cppType << " " << componentClass << "::Get"
					<< propertyName << "() const\n";
				cpp << "{\n";
				cpp << "\treturn GetDependencyPropertyValue<"
					<< cppType << ">(" << propertyName
					<< "Property());\n";
				cpp << "}\n\n";
				if (property.IsReadOnly)
				{
					cpp << "bool " << componentClass << "::Publish"
						<< propertyName << "(" << cppType << " value)\n";
					cpp << "{\n";
					cpp << "\treturn TrySetReadOnlyPropertyValue("
						<< propertyName
						<< "PropertyKey(), BindingValue(std::move(value)));\n";
					cpp << "}\n\n";
				}
				else
				{
					cpp << "void " << componentClass << "::Set"
						<< propertyName << "(" << cppType << " value)\n";
					cpp << "{\n";
					cpp << "\t(void)SetDependencyPropertyValue("
						<< propertyName
						<< "Property(), std::move(value));\n";
					cpp << "}\n\n";
				}
			}

			if (hasInheritedProperties)
			{
				cpp << "void " << componentClass
					<< "::VisitDeclaredInheritedProperties(\n";
				cpp << "\tvoid* context, InheritedPropertyVisitor visitor) const\n";
				cpp << "{\n";
				cpp << "\t" << baseClass
					<< "::VisitDeclaredInheritedProperties(context, visitor);\n";
				cpp << "\tif (!visitor) return;\n";
				for (const auto& property : component.Properties)
				{
					if (!HasDependencyPropertyFlag(
						property.Flags, DependencyPropertyFlags::Inherits)) continue;
					cpp << "\tvisitor(context, "
						<< SanitizeCppIdentifier(WStringToString(property.Name))
						<< "Property());\n";
				}
				cpp << "}\n\n";
			}

			cpp << "ComponentTypeToken " << componentClass
				<< "::GetCompiledComponentTypeTokenCore() const noexcept\n";
			cpp << "{\n\treturn ComponentTypeId();\n}\n\n";

			cpp << "const DependencyPropertyMetadata* "
				<< componentClass << "::FindCompiledComponentPropertyCore(\n";
			cpp << "\tComponentPropertyToken property) const\n";
			cpp << "{\n";
			cpp << "\tswitch (property.Value)\n";
			cpp << "\t{\n";
			for (const auto& property : component.Properties)
			{
				const auto propertyName = SanitizeCppIdentifier(
					WStringToString(property.Name));
				cpp << "\tcase "
					<< GeneratedComponentPropertyTokenValue(property.Name)
					<< "ULL:\n";
				cpp << "\t\treturn const_cast<" << componentLeaf
					<< "*>(this)->GetPropertyMetadata("
					<< propertyName << "Property());\n";
			}
			cpp << "\tdefault:\n\t\treturn nullptr;\n";
			cpp << "\t}\n";
			cpp << "}\n\n";

			cpp << "bool " << componentClass
				<< "::IsCompiledComponentPropertyCore(\n";
			cpp << "\tconst DependencyPropertyMetadata& metadata) "
				"const noexcept\n";
			cpp << "{\n";
			cpp << "\treturn metadata.OwnerType() == "
				"std::type_index(typeid(" << componentLeaf << "));\n";
			cpp << "}\n\n";

			for (const auto& event : component.Events)
			{
				const auto eventName = SanitizeCppIdentifier(
					WStringToString(event.Name));
				const auto* payload =
					ComponentEventPayloadKindExpression(event.Payload);
				if (!payload)
					throw std::invalid_argument(
						"Generated component event payload is unsupported");
				cpp << "const DeclarativeEventDefinition& "
					<< componentClass << "::" << eventName
					<< "Event() noexcept\n";
				cpp << "{\n";
				cpp << "\t// Writable storage prevents Release /OPT:ICF from "
					"folding distinct event identities.\n";
				cpp << "\tstatic DeclarativeEventDefinition value("
					<< payload << ", "
					<< ComponentEventRoutingExpression(
						event.RoutingStrategy) << ");\n";
				cpp << "\treturn value;\n";
				cpp << "}\n\n";

				cpp << "EventConnection " << componentClass
					<< "::Subscribe" << eventName << "(\n";
				cpp << "\tDeclarativeEvent::std_function_type handler,\n";
				cpp << "\tbool handledEventsToo)\n";
				cpp << "{\n";
				cpp << "\treturn OnDeclarativeEvent.Subscribe(\n";
				cpp << "\t\t[this, handler = std::move(handler), "
					"handledEventsToo](\n";
				cpp << "\t\t\tControl* sender, DeclarativeEventArgs& args) "
					"mutable\n";
				cpp << "\t\t{\n";
				cpp << "\t\t\tif (args.OriginalSource != this\n";
				cpp << "\t\t\t\t|| args.Definition != &"
					<< eventName << "Event()\n";
				cpp << "\t\t\t\t|| (args.Handled && !handledEventsToo)) "
					"return;\n";
				cpp << "\t\t\tif (handler) handler(sender, args);\n";
				cpp << "\t\t});\n";
				cpp << "}\n\n";

				cpp << "bool " << componentClass << "::Raise"
					<< eventName << "(";
				if (const auto* payloadType =
					ComponentEventPayloadCppType(event.Payload))
					cpp << payloadType << " value";
				cpp << ")\n";
				cpp << "{\n";
				cpp << "\treturn RaiseDeclarativeEvent("
					<< eventName << "Event(), ";
				if (event.Payload == DesignerComponentEventPayload::None)
					cpp << "BindingValue{}";
				else
					cpp << "BindingValue(std::move(value))";
				cpp << ");\n";
				cpp << "}\n\n";
			}

			cpp << componentClass << "::" << componentLeaf << "()\n";
			cpp << "\t: " << baseClass << "()\n";
			cpp << "{\n";
			cpp << "\t(void)ClearPropertyValues();\n";
			cpp << "\tstd::wstring error;\n";
			cpp << "\tif (!InitializeGeneratedTemplate(&error))\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated component template initialization failed: \" "
				"+ Convert::WStringToString(error));\n";
			cpp << "}\n\n";

			cpp << "bool " << componentClass
				<< "::InitializeGeneratedTemplate(std::wstring* outError)\n";
			cpp << "{\n";
			cpp << "\tauto fail = [&](std::wstring message)\n";
			cpp << "\t{\n";
			cpp << "\t\tif (outError) *outError = std::move(message);\n";
			cpp << "\t\treturn false;\n";
			cpp << "\t};\n";
			cpp << "\ttry\n";
			cpp << "\t{\n";

			auto generatedTemplate = component.Template;
			std::map<std::uint64_t, std::wstring> componentPartNamesByToken;
			for (const auto& node : generatedTemplate)
				ValidateGeneratedTemplatePart(
					componentPartNamesByToken, node.Name,
					"Static component namescope");
			for (auto& node : generatedTemplate)
			{
				node.TemplateState.Generated = true;
				node.TemplateState.Owner = component.Type.XamlName;
				node.TemplateState.PartName = node.Name;
				node.TemplateState.ControlTemplateRoot = false;
				cpp << GenerateControlInstantiation(node, 2);
				const auto nodeVar = GetVarName(node);
				const auto partName = SanitizeCppIdentifier(
					WStringToString(node.Name));
				cpp << "\t\t_part_" << partName << " = "
					<< nodeVar << ";\n";
			}
			if (!generatedTemplate.empty()) cpp << "\n";

			for (const auto& node : generatedTemplate)
			{
				const auto nodeVar = GetVarName(node);
				cpp << "\t\tcui::framework::TreeAccess::"
					"SetTemplatedParent(*" << nodeVar << ", this);\n";
				cpp << "\t\tif (!cui::framework::TemplateAccess::"
					"RegisterTemplatePart(*this, "
					<< GeneratedTemplatePartTokenExpression(node.Name) << ", "
					<< nodeVar << "))\n";
				cpp << "\t\t\treturn fail("
					"L\"ComponentDefinition 模板部件注册失败。\");\n";
				if (!node.PresentedComponentContent.empty())
				{
					const auto contentName = SanitizeCppIdentifier(
						WStringToString(node.PresentedComponentContent));
					cpp << "\t\t_presenter_" << contentName << " = "
						<< nodeVar << ";\n";
				}
			}
			if (!generatedTemplate.empty()) cpp << "\n";

			auto findTemplateNode = [&](const std::wstring& name)
				-> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					generatedTemplate.begin(), generatedTemplate.end(),
					[&](const auto& candidate)
					{ return candidate.Name == name; });
				return found == generatedTemplate.end() ? nullptr : &*found;
			};
			auto componentCommandTarget =
				[&](const std::wstring& name) -> std::string
			{
				if (name.empty()) return "nullptr";
				if (name == component.Type.XamlName) return "this";
				const auto* target = findTemplateNode(name);
				if (!target)
					throw std::invalid_argument(
					"Static component CommandTarget cannot be resolved");
				return GetVarName(*target);
			};
			std::unordered_map<std::wstring, std::string>
				componentControlExpressions;
			componentControlExpressions.emplace(
				component.Type.XamlName, "this");
			for (const auto& node : generatedTemplate)
				componentControlExpressions.emplace(
					node.Name, GetVarName(node));

			for (const auto& node : generatedTemplate)
			{
				const auto nodeVar = GetVarName(node);
				if ((node.Type == UIClass::UI_Button
					|| node.Type == UIClass::UI_MenuItem)
					&& !node.Structure.CommandTarget.empty())
					cpp << "\t\t" << nodeVar
						<< "->CommandTarget = "
						<< componentCommandTarget(
							node.Structure.CommandTarget) << ";\n";
				if (!node.Properties.StyleResourceKey.empty())
					cpp << "\t\tcui::framework::StyleAccess::"
						"SetResourceKey(*" << nodeVar << ", L\""
						<< EscapeWStringLiteral(
							node.Properties.StyleResourceKey)
						<< "\", "
						<< (frameworkThemeProgram
							|| node.TemplateState.StyleResourceScopeFromTheme
							? "true" : "false")
						<< (node.TemplateState.StyleResourceIsAutomatic
							? ", true" : "") << ");\n";
				cpp << GenerateAuthoredProperties(node, 2);
				cpp << GenerateContainerProperties(node, 2);
			}

			for (const auto& node : generatedTemplate)
			{
				const auto nodeVar = GetVarName(node);
				for (const auto& [targetProperty, sourceProperty]
					: node.TemplateBindings)
				{
					const auto* targetMemberPath =
						CompiledMemberPathAccessor(targetProperty);
					const auto* sourceMemberPath =
						CompiledMemberPathAccessor(sourceProperty);
					if (targetMemberPath || sourceMemberPath)
					{
						if (!targetMemberPath || !sourceMemberPath)
							throw std::invalid_argument(
								"Static ComponentDefinition member-path "
								"TemplateBinding must connect compiled paths");
						cpp << "\t\t" << nodeVar << "->SetCompiled"
							<< targetMemberPath << "(this->GetCompiled"
							<< sourceMemberPath << "());\n";
						continue;
					}
					const auto targetIdentity =
						FindGeneratedDependencyPropertyExpression(
							node, targetProperty, true);
					const auto sourceIdentity =
						FindComponentDependencyPropertyExpression(
							component, sourceProperty, false);
					if (targetIdentity.empty() || sourceIdentity.empty())
						throw std::invalid_argument(
							"Static ComponentDefinition TemplateBinding has no "
							"DependencyProperty identity: "
							+ GetControlTypeName(node.Type) + "."
							+ WStringToString(targetProperty) + " <- "
							+ GetComponentClassName(component) + "."
							+ WStringToString(sourceProperty));
					cpp << "\t\tif (!" << nodeVar
						<< "->DataBindings.AddTemplateBinding("
						<< targetIdentity << ", *this, "
						<< sourceIdentity << "))\n";
					cpp << "\t\t\treturn fail("
						"L\"ComponentDefinition TemplateBinding 安装失败。\");\n";
				}
			}

			for (const auto& node : generatedTemplate)
			{
				if (node.Bindings.empty()) continue;
				const auto nodeVar = GetVarName(node);
				auto componentParent = [&]()
					-> const DesignerModel::DesignNode*
				{
					if (node.ParentId > 0)
					{
						const auto found = std::find_if(
							generatedTemplate.begin(), generatedTemplate.end(),
							[&](const auto& candidate)
							{ return candidate.Id == node.ParentId; });
						if (found != generatedTemplate.end()) return &*found;
					}
					return node.ParentRef.empty()
						? nullptr : findTemplateNode(node.ParentRef);
				};
				auto resolveComponentBindingEndpoint = [&] (
					const DesignerDataBinding& sourceBinding,
					const std::wstring& targetProperty,
					bool multiSource,
					std::string_view indent,
					const char* context)
				{
					StaticBindingSourceSpec source;
					source.AdapterExpression = multiSource
						? "static_cast<IBindingSource*>(&"
							+ nodeVar + "->DataContextSource())"
						: nodeVar + "->DataContextSource()";
					source.SourceSchema =
						&canonicalDataDocument.DataContextSchema;
					if (!sourceBinding.ElementName.empty())
					{
						const auto* sourceNode = findTemplateNode(
							sourceBinding.ElementName);
						if (!sourceNode)
							throw std::invalid_argument(
								"Static ComponentDefinition ElementName is unresolved");
						const auto sourceVar = GetVarName(*sourceNode);
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(" + sourceVar + ")"
							: "*" + sourceVar;
						source.DirectObjectExpression = "*" + sourceVar;
						source.DirectNode = sourceNode;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::Self)
					{
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(" + nodeVar + ")"
							: "*" + nodeVar;
						source.DirectObjectExpression = "*" + nodeVar;
						source.DirectNode = &node;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::TemplatedParent)
					{
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(this)" : "*this";
						source.DirectObjectExpression = "*this";
						source.DirectComponent = &component;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						source.AdapterExpression =
							"cui::binding::CreateFindAncestorSource(*"
							+ nodeVar + ", "
							+ GeneratedFindAncestorTypeExpression(
								_sourceDocument, sourceBinding) + ", "
							+ std::to_string(sourceBinding.AncestorLevel) + ")";
						source.SourceSchema = nullptr;
					}
					else if (targetProperty == L"DataContext")
					{
						const auto* parent = componentParent();
						const auto parentPointer = parent
							? GetVarName(*parent) : std::string("this");
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(&"
								+ parentPointer + "->DataContextSource())"
							: parentPointer + "->DataContextSource()";
					}
					return lowerBindingSourceEndpoint(
						sourceBinding, source, indent, context);
				};

				for (const auto& [targetProperty, binding] : node.Bindings)
				{
					const auto targetIdentity =
						FindGeneratedDependencyPropertyExpression(
							node, targetProperty, true);
					if (targetIdentity.empty())
						throw std::invalid_argument(
							"Static ComponentDefinition Binding target has no "
							"writable DependencyProperty identity");
					cpp << "\t\t{\n";
					if (binding.IsMultiBinding())
					{
						auto resolveChild = [&] (
							const DesignerDataBinding& child,
							std::string_view indent,
							const char* context)
						{
							return resolveComponentBindingEndpoint(
								child, targetProperty, true, indent, context);
						};
						emitStaticMultiBinding(
							binding, nodeVar, targetIdentity,
							resolveChild, "\t\t\t",
							"Static ComponentDefinition MultiBinding",
							"attached");
					}
					else
					{
						const auto options = lowerBindingOptions(binding);
						const auto endpoint = resolveComponentBindingEndpoint(
							binding, targetProperty, false, "\t\t\t",
							"Static ComponentDefinition Binding");
						if (!options.ConverterName.empty())
						{
							cpp << "\t\t\tauto converter = "
								<< bindingConverterExpression(
									options.ConverterName, binding) << ";\n";
							cpp << "\t\t\tif (!converter)\n";
							cpp << "\t\t\t\treturn fail("
								"L\"ComponentDefinition Binding Converter "
								"不存在。\");\n";
						}
						if (endpoint.UsesDirectSource())
							cpp << "\t\t\t"
								<< (endpoint.DirectCompiledRecord
									? "// CUI:AOT binding-source=direct-record\n"
									: "// CUI:AOT binding-source=direct-dp\n");
						cpp << "\t\t\tconst bool attached = "
							<< endpoint.Guard << nodeVar
							<< "->DataBindings.Add(" << targetIdentity
							<< ", " << endpoint.SourceOperand << ", ";
						if (!endpoint.UsesDirectSource())
							cpp << endpoint.SourcePathOperand << ", ";
						cpp << BindingModeToExpr(binding.Mode) << ", "
							<< DataSourceUpdateModeToExpr(binding.UpdateMode);
						if (!options.ConverterName.empty())
							cpp << ", std::move(converter)";
						else if (options.HasExtendedOptions)
							cpp << ", {}";
						if (options.HasExtendedOptions)
							cpp << ", " << options.Fallback
								<< ", " << options.TargetNull
								<< ", " << options.ConverterParameter
								<< ", " << options.StringFormat;
						cpp << ") != nullptr;\n";
					}
					cpp << "\t\t\tif (!attached)\n";
					cpp << "\t\t\t\treturn fail("
						"L\"ComponentDefinition Binding 安装失败。\");\n";
					cpp << "\t\t}\n";
				}
			}

			for (const auto& node : generatedTemplate)
			{
				const auto nodeVar = GetVarName(node);
				for (const auto& binding : node.InputBindings)
				{
					std::wstring gestureError;
					if (binding.Kind
						== DesignerModel::DesignInputBindingKind::Key)
					{
						KeyGesture gesture;
						if (!TryParseKeyGesture(
							binding.Gesture, gesture, &gestureError))
							throw std::invalid_argument(
								"Static component KeyBinding is invalid");
						const auto keyExpression = KeyToExpr(gesture.Key);
						if (keyExpression.empty())
							throw std::invalid_argument(
								"Static component KeyBinding key is unsupported");
						cpp << "\t\t(void)" << nodeVar
							<< "->AddInputBinding(KeyBinding{ "
							"RoutedCommand(L\""
							<< EscapeWStringLiteral(binding.Command)
							<< "\"), KeyGesture{ " << keyExpression
							<< ", "
							<< ModifierKeysToExpr(gesture.Modifiers)
							<< " }, ";
					}
					else
					{
						MouseGesture gesture;
						if (!TryParseMouseGesture(
							binding.Gesture, gesture, &gestureError))
							throw std::invalid_argument(
								"Static component MouseBinding is invalid");
						cpp << "\t\t(void)" << nodeVar
							<< "->AddInputBinding(MouseBinding{ "
							"RoutedCommand(L\""
							<< EscapeWStringLiteral(binding.Command)
							<< "\"), MouseGesture{ "
							<< MouseActionToExpr(gesture.Action)
							<< ", "
							<< ModifierKeysToExpr(gesture.Modifiers)
							<< " }, ";
					}
					if (binding.CommandParameter.empty())
						cpp << "{}";
					else
						cpp << "std::wstring(L\""
							<< EscapeWStringLiteral(
								binding.CommandParameter) << "\")";
					cpp << ", "
						<< componentCommandTarget(binding.CommandTarget)
						<< " });\n";
				}
			}

			for (const auto& node : generatedTemplate)
			{
				const auto nodeVar = GetVarName(node);
				for (const auto& [sourceEvent, targetEvent]
					: node.TemplateEventBindings)
				{
					const auto sourceDescriptor =
						DesignerEventCatalog::FindControlEvent(
							node.Type, sourceEvent);
					if (!sourceDescriptor)
						throw std::invalid_argument(
							"Static component template event is unknown");
					const auto target = std::find_if(
						component.Events.begin(), component.Events.end(),
						[&](const auto& candidate)
						{ return candidate.Name == targetEvent; });
					if (target == component.Events.end()
						|| target->Payload
							!= DesignerComponentEventPayload::None)
						throw std::invalid_argument(
							"Static component RaiseEvent payload is unsupported");
					const auto targetName = SanitizeCppIdentifier(
						WStringToString(targetEvent));
					cpp << "\t\tcui::framework::TemplateAccess::"
						"RetainTemplateEventConnection(*this,\n";
					cpp << "\t\t\t" << nodeVar << "->"
						<< sourceDescriptor->EventField
						<< ".Subscribe([this](auto&&...)\n";
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\t(void)Raise" << targetName
						<< "();\n";
					cpp << "\t\t\t}));\n";
				}
			}

			std::vector<std::vector<size_t>> componentChildren(
				generatedTemplate.size());
			std::vector<size_t> componentRoots;
			auto findTemplateIndexById =
				[&](int id) -> std::optional<size_t>
			{
				const auto found = std::find_if(
					generatedTemplate.begin(), generatedTemplate.end(),
					[&](const auto& candidate) { return candidate.Id == id; });
				if (found == generatedTemplate.end()) return std::nullopt;
				return static_cast<size_t>(
					found - generatedTemplate.begin());
			};
			auto findTemplateIndexByName =
				[&](const std::wstring& name) -> std::optional<size_t>
			{
				const auto found = std::find_if(
					generatedTemplate.begin(), generatedTemplate.end(),
					[&](const auto& candidate)
					{ return candidate.Name == name; });
				if (found == generatedTemplate.end()) return std::nullopt;
				return static_cast<size_t>(
					found - generatedTemplate.begin());
			};
			for (size_t index = 0;
				index < generatedTemplate.size(); ++index)
			{
				const auto& node = generatedTemplate[index];
				auto parent = node.ParentId > 0
					? findTemplateIndexById(node.ParentId)
					: !node.ParentRef.empty()
						? findTemplateIndexByName(node.ParentRef)
						: std::nullopt;
				if (parent)
					componentChildren[*parent].push_back(index);
				else
					componentRoots.push_back(index);
			}
			auto sortComponentNodes = [&](auto& indexes)
			{
				std::stable_sort(
					indexes.begin(), indexes.end(),
					[&](size_t left, size_t right)
					{
						const auto leftOrder =
							generatedTemplate[left].Order < 0
							? (std::numeric_limits<int>::max)()
							: generatedTemplate[left].Order;
						const auto rightOrder =
							generatedTemplate[right].Order < 0
							? (std::numeric_limits<int>::max)()
							: generatedTemplate[right].Order;
						return leftOrder < rightOrder;
					});
			};
			sortComponentNodes(componentRoots);
			for (auto& children : componentChildren)
				sortComponentNodes(children);
			if (componentRoots.size() != 1)
				throw std::invalid_argument(
					"Static component template must have exactly one root");

			std::function<void(size_t)> emitComponentNode;
			emitComponentNode = [&](size_t index)
			{
				const auto& node = generatedTemplate[index];
				const auto nodeVar = GetVarName(node);
				auto parent = node.ParentId > 0
					? findTemplateIndexById(node.ParentId)
					: !node.ParentRef.empty()
						? findTemplateIndexByName(node.ParentRef)
						: std::nullopt;
				if (!parent)
				{
					cpp << "\t\tif (!cui::framework::TemplateAccess::"
						"SetTemplateRoot(*this, std::move(__owned_"
						<< nodeVar << ")))\n";
					cpp << "\t\t\treturn fail("
						"L\"ComponentDefinition 模板根安装失败。\");\n";
				}
				else
				{
					const auto& parentNode = generatedTemplate[*parent];
					const auto parentVar = GetVarName(parentNode);
					const bool isVisualHeader = node.Structure.ChildRole
						== DesignerModel::DesignNodeChildRole::Header;
					if (isVisualHeader
						&& (IsUIClassAssignableFrom(
								UIClass::UI_HeaderedContentControl,
								parentNode.Type)
							|| IsUIClassAssignableFrom(
								UIClass::UI_HeaderedItemsControl,
								parentNode.Type)))
						cpp << "\t\t" << parentVar
							<< "->SetVisualHeader(std::move(__owned_"
							<< nodeVar << "));\n";
					else if (IsUIClassAssignableFrom(
						UIClass::UI_ItemsControl, parentNode.Type))
						cpp << "\t\t" << parentVar
							<< "->AddItemControl(std::move(__owned_"
							<< nodeVar << "));\n";
					else if (IsUIClassAssignableFrom(
						UIClass::UI_ContentControl, parentNode.Type))
						cpp << "\t\t" << parentVar
							<< "->SetVisualContent(std::move(__owned_"
							<< nodeVar << "));\n";
					else if (parentNode.Type == UIClass::UI_Popup
						|| IsUIClassAssignableFrom(
							UIClass::UI_Decorator, parentNode.Type))
						cpp << "\t\t" << parentVar
							<< "->SetChild(std::move(__owned_"
							<< nodeVar << "));\n";
					else
						cpp << "\t\t" << parentVar
							<< "->AddOwned(std::move(__owned_"
							<< nodeVar << "));\n";
				}
				cpp << "\t\tcui::framework::TreeAccess::"
					"SetLogicalParent(*" << nodeVar << ", nullptr);\n";
				for (const auto child : componentChildren[index])
					emitComponentNode(child);
			};
			emitComponentNode(componentRoots.front());
			for (size_t index = 0;
				index < generatedTemplate.size(); ++index)
			{
				const auto& node = generatedTemplate[index];
				if (!node.Structure.RelativePanel
					|| node.Structure.RelativePanel->Empty()) continue;
				const auto parent = node.ParentId > 0
					? findTemplateIndexById(node.ParentId)
					: !node.ParentRef.empty()
						? findTemplateIndexByName(node.ParentRef)
						: std::nullopt;
				cpp << GenerateRelativePanelConstraints(
					node,
					parent
						? GetVarName(generatedTemplate[*parent])
						: "this",
					componentControlExpressions,
					2,
					true);
			}

			std::vector<DeclarativeVisualStateGroupDefinition>
				componentVisualStates;
			std::vector<DeclarativeEventTriggerDefinition>
				componentEventTriggers;
			std::wstring componentInteractionError;
			if (!CuiRuntime::XamlObjectMaterializer::
				MaterializeDeclarativeInteractions(
					component.VisualStateGroups,
					component.EventTriggers,
					_sourceDocument,
					componentVisualStates,
					componentEventTriggers,
					&componentInteractionError))
				throw std::invalid_argument(
					WStringToString(componentInteractionError));
			const DeclarativePropertyResolver componentPropertyResolver =
				[&](const std::wstring& targetName,
					const std::wstring& propertyName,
					bool requireWritable)
				{
					if (targetName.empty()
						|| targetName == component.Type.XamlName)
						return FindComponentDependencyPropertyExpression(
							component, propertyName, requireWritable);
					const auto* target = findTemplateNode(targetName);
					return target
						? FindGeneratedDependencyPropertyExpression(
							*target, propertyName, requireWritable)
						: std::string{};
				};
			const DeclarativeTargetResolver componentTargetResolver =
				[&](const std::wstring& targetName)
				{
					if (targetName == component.Type.XamlName)
						return std::string("nullptr");
					const auto* target = findTemplateNode(targetName);
					return target ? GetVarName(*target) : std::string{};
				};
			const DeclarativeEventResolver componentEventResolver =
				[&](const std::wstring& eventName)
				{
					const auto componentEvent = std::find_if(
						component.Events.begin(), component.Events.end(),
						[&](const auto& candidate)
						{ return candidate.Name == eventName; });
					if (componentEvent != component.Events.end())
					{
						const auto name = SanitizeCppIdentifier(
							WStringToString(componentEvent->Name));
						return DeclarativeEventReferenceExpression{
							"&" + componentClass + "::" + name + "Event()",
							false };
					}
					const std::wstring localName(RoutedEventLocalName(eventName));
					if (!DesignerEventCatalog::FindControlEvent(
						component.BaseType, localName))
						return DeclarativeEventReferenceExpression{};
					const auto eventId = FindRoutedEventId(localName);
					return eventId
						? DeclarativeEventReferenceExpression{
							RoutedEventIdExpression(*eventId), true }
						: DeclarativeEventReferenceExpression{};
				};
			cpp << GenerateDeclarativeInteractionsCode(
				componentVisualStates, componentEventTriggers,
				componentPropertyResolver, componentTargetResolver,
				componentEventResolver, "*this", 2);
			cpp << "\t\tif (!cui::framework::TemplateAccess::"
				"GetTemplateRoot(*this))\n";
			cpp << "\t\t\treturn fail("
				"L\"ComponentDefinition 未生成模板根。\");\n";
			cpp << "\t\tif (outError) outError->clear();\n";
			cpp << "\t\treturn true;\n";
			cpp << "\t}\n";
			cpp << "\tcatch (...)\n";
			cpp << "\t{\n";
			cpp << "\t\treturn fail("
				"L\"ComponentDefinition 模板初始化发生异常。\");\n";
			cpp << "\t}\n";
			cpp << "}\n\n";
			for (const auto& content : component.ContentProperties)
			{
				const auto contentName = SanitizeCppIdentifier(
					WStringToString(content.Name));
				cpp << "bool " << componentClass << "::"
					<< (content.Cardinality
						== DesignerComponentContentCardinality::Single
						? "Set" : "Add")
					<< contentName
					<< "(std::unique_ptr<Control> value)\n";
				cpp << "{\n";
				cpp << "\tif (!value || !_presenter_"
					<< contentName;
				if (content.Cardinality
					== DesignerComponentContentCardinality::Single)
					cpp << " || _content_" << contentName;
				cpp << ") return false;\n";
				cpp << "\tauto* attached = "
					"cui::framework::TreeAccess::AddOwnedVisualChild(\n";
				cpp << "\t\t*_presenter_" << contentName
					<< ", std::move(value), this);\n";
				cpp << "\tif (!attached) return false;\n";
				if (content.Cardinality
					== DesignerComponentContentCardinality::Single)
					cpp << "\t_content_" << contentName
						<< " = attached;\n";
				cpp << "\treturn true;\n";
				cpp << "}\n\n";
			}
		}
	}

	if (frameworkThemeProgram)
	{
		cpp << "std::shared_ptr<const ControlStyleSheet>\n";
		cpp << className << "::Build()\n";
		cpp << "{\n\n";
	}
	else
	{
		// Do not lower XAML from the generated base constructor.
		// InitializeComponent is called from the user class constructor body,
		// after base construction, so virtual hooks dispatch to authored code.
		cpp << className << "::" << classLeaf << "()\n";
		cpp << "\t: Window()\n";
		cpp << "{\n";
		cpp << "}\n\n";
		cpp << "void " << className << "::InitializeComponent()\n";
		cpp << "{\n\n";
		cpp << "\tif (_componentInitialized) return;\n";
		cpp << "\t_componentInitialized = true;\n\n";
		cpp << "\t// Native constructors are behavior-host implementation details.\n";
		cpp << "\t// Begin from the same empty Local-value surface as dynamic XAML.\n";
		cpp << "\t(void)this->ClearPropertyValues();\n\n";
	}
	if (dynamicWindow)
	{
		cpp << "\tstatic const auto __xamlType_this = "
			"DeclarativeTypeDescriptor::Create(\n";
		cpp << "\t\tRuntimeTypeId{ L\""
			<< EscapeWStringLiteral(_sourceDocument.Window.XamlType.NamespaceUri)
			<< "\", L\""
			<< EscapeWStringLiteral(_sourceDocument.Window.XamlType.LocalName)
			<< "\" }, {});\n";
		cpp << "\tif (!__xamlType_this || "
			"!cui::framework::XamlAccess::SetTypeDescriptor("
			"*this, __xamlType_this))\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated XAML type attachment failed\");\n\n";
	}
	if (!sharedDocumentResourceDeclarations.empty())
	{
		cpp << "\t// WPF StaticResource: one value per document "
			"ResourceDictionary instance.\n";
		for (const auto& [resource, variable]
			: sharedDocumentResourceDeclarations)
			cpp << "\tconst auto " << variable << " = "
				<< GenerateStyleValueExpression(resource->Value) << ";\n";
		cpp << "\n";
	}
	if (!staticItemsPanels.empty())
	{
		auto kindExpression = [](ItemsPanelKind kind)
		{
			switch (kind)
			{
			case ItemsPanelKind::Wrap:
				return "ItemsPanelKind::Wrap";
			case ItemsPanelKind::VirtualizingStack:
				return "ItemsPanelKind::VirtualizingStack";
			default:
				return "ItemsPanelKind::Stack";
			}
		};
		cpp << "\t// ItemsPanelTemplate resources are immutable native layout "
			"descriptors; no XAML factory is retained.\n";
		for (const auto& panel : staticItemsPanels)
		{
			const auto& value = panel.Definition->Value;
			cpp << "\tauto " << panel.VariableName
				<< " = std::make_shared<ItemsPanelTemplate>();\n";
			cpp << "\t" << panel.VariableName << "->Kind = "
				<< kindExpression(value.Kind) << ";\n";
			cpp << "\t" << panel.VariableName << "->Orientation = "
				<< (value.Orientation == Orientation::Horizontal
					? "Orientation::Horizontal"
					: "Orientation::Vertical") << ";\n";
			cpp << "\t" << panel.VariableName << "->ItemWidth = "
				<< FloatLiteral(value.ItemWidth) << ";\n";
			cpp << "\t" << panel.VariableName << "->ItemHeight = "
				<< FloatLiteral(value.ItemHeight) << ";\n";
			cpp << "\t" << panel.VariableName << "->CacheLength = "
				<< FloatLiteral(value.CacheLength) << ";\n";
		}
		cpp << "\n";
	}
	if (!staticDataLists.empty())
	{
		auto parentPath = [](const std::wstring& path)
		{
			const auto separator = path.rfind(L'.');
			return separator == std::wstring::npos
				? std::wstring{} : path.substr(0, separator);
		};
		auto leafName = [](const std::wstring& path)
		{
			const auto separator = path.rfind(L'.');
			return separator == std::wstring::npos
				? path : path.substr(separator + 1);
		};
		auto defaultDataValue = [](BindingValueKind kind)
		{
			switch (kind)
			{
			case BindingValueKind::Bool:
				return BindingValue(false);
			case BindingValueKind::NullableBool:
				return BindingValue(NullableBool{});
			case BindingValueKind::Int:
				return BindingValue(0);
			case BindingValueKind::Int64:
				return BindingValue(static_cast<long long>(0));
			case BindingValueKind::Float:
				return BindingValue(0.0f);
			case BindingValueKind::Double:
				return BindingValue(0.0);
			case BindingValueKind::String:
			case BindingValueKind::Empty:
				return BindingValue(std::wstring{});
			default:
				return BindingValue{};
			}
		};
		auto styleKindForDataValue = [](BindingValueKind kind)
			-> std::optional<DesignerStyleValueKind>
		{
			switch (kind)
			{
			case BindingValueKind::Bool:
				return DesignerStyleValueKind::Bool;
			case BindingValueKind::NullableBool:
				return DesignerStyleValueKind::NullableBool;
			case BindingValueKind::Int:
				return DesignerStyleValueKind::Int;
			case BindingValueKind::Int64:
				return DesignerStyleValueKind::Int64;
			case BindingValueKind::Float:
				return DesignerStyleValueKind::Float;
			case BindingValueKind::Double:
				return DesignerStyleValueKind::Double;
			case BindingValueKind::String:
				return DesignerStyleValueKind::String;
			default:
				return std::nullopt;
			}
		};

		cpp << "\t// Embedded DataList resources lowered to native CUI binding "
			"objects; no runtime XAML schema is retained.\n";
		if (!dynamicWindow)
		{
			cpp << "\t// Production uses immutable lists and generated typed records; "
				"ObservableObject discovery is not linked.\n";
			for (size_t listIndex = 0;
				listIndex < staticDataLists.size(); ++listIndex)
			{
				const auto& dataList = staticDataLists[listIndex];
				const auto itemsName = "__compiledDataItems_"
					+ std::to_string(listIndex + 1);
				cpp << "\tstd::vector<BindingSourceReference> " << itemsName
					<< ";\n";
				cpp << "\t" << itemsName << ".reserve("
					<< dataList.Definition->Records.size() << ");\n";

				std::vector<const StaticCompiledDataRecordClass*> recordClasses;
				for (const auto& recordClass : staticCompiledDataRecordClasses)
					if (recordClass.ItemType == dataList.ItemType)
						recordClasses.push_back(&recordClass);
				auto contextDepth = [](const std::wstring& path)
				{
					return path.empty() ? size_t{ 0 }
						: static_cast<size_t>(
							std::count(path.begin(), path.end(), L'.')) + 1;
				};
				std::sort(recordClasses.begin(), recordClasses.end(),
					[&](const auto* left, const auto* right)
					{
						const auto leftDepth = contextDepth(left->ContextPath);
						const auto rightDepth = contextDepth(right->ContextPath);
						return leftDepth != rightDepth
							? leftDepth > rightDepth
							: left->ContextPath > right->ContextPath;
					});

				for (size_t recordIndex = 0;
					recordIndex < dataList.Definition->Records.size();
					++recordIndex)
				{
					const auto& record =
						dataList.Definition->Records[recordIndex];
					std::map<std::wstring, std::string> objectVariables;
					cpp << "\t{\n";
					for (size_t classOrdinal = 0;
						classOrdinal < recordClasses.size(); ++classOrdinal)
					{
						const auto& recordClass = *recordClasses[classOrdinal];
						const auto recordName = "__compiledDataRecord_"
							+ std::to_string(listIndex + 1) + "_"
							+ std::to_string(recordIndex + 1) + "_"
							+ std::to_string(classOrdinal + 1);
						cpp << "\t\tauto " << recordName << " = std::make_shared<"
							<< recordClass.ClassName << ">(\n";
						for (size_t propertyIndex = 0;
							propertyIndex < recordClass.Properties.size();
							++propertyIndex)
						{
							const auto& property =
								*recordClass.Properties[propertyIndex];
							std::string valueExpression;
							if (property.ValueKind == BindingValueKind::Object)
							{
								if (property.ObjectKind
									== DesignerDataObjectKind::BindingSource)
								{
									const auto nested = objectVariables.find(property.Path);
									if (nested == objectVariables.end())
										throw std::invalid_argument(
											"Static DataList nested record class is missing");
									valueExpression = "BindingSourceReference("
										+ nested->second + ")";
								}
								else if (property.ObjectKind
									== DesignerDataObjectKind::BindingList)
								{
									valueExpression =
										"BindingListReference(std::make_shared<"
										"CompiledBindingList>(std::vector<"
										"BindingSourceReference>{}, "
										+ GeneratedDataTypeTokenExpression(
											property.ItemType) + "))";
								}
								else valueExpression = "std::shared_ptr<void>{}";
							}
							else
							{
								auto value = defaultDataValue(property.ValueKind);
								if (const auto field = record.Fields.find(property.Path);
									field != record.Fields.end())
								{
									const auto kind =
										styleKindForDataValue(property.ValueKind);
									std::wstring conversionError;
									if (!kind
										|| !DesignerStyleSheetUtils::TryConvertValue(
											{ *kind, field->second }, value,
											&conversionError,
											canonicalDataDocument.ResourceBasePath,
											canonicalDataDocument.Resources))
										throw std::invalid_argument(
											"Static DataList scalar conversion failed: "
											+ WStringToString(conversionError));
								}
								valueExpression = UnwrapBindingValue(
									GenerateBindingValueExpression(value));
								if (valueExpression.empty())
									throw std::invalid_argument(
										"Static DataList scalar has no typed lowering");
							}
							cpp << "\t\t\t" << valueExpression;
							if (propertyIndex + 1 != recordClass.Properties.size())
								cpp << ",";
							cpp << "\n";
						}
						cpp << "\t\t);\n";
						objectVariables[recordClass.ContextPath] = recordName;
					}
					const auto root = objectVariables.find(L"");
					if (root == objectVariables.end())
						throw std::invalid_argument(
							"Static DataList root record class is missing");
					cpp << "\t\t" << itemsName << ".emplace_back("
						<< root->second << ");\n";
					cpp << "\t}\n";
				}
				cpp << "\tauto " << dataList.VariableName
					<< " = std::make_shared<CompiledBindingList>(std::move("
					<< itemsName << "), "
					<< GeneratedDataTypeTokenExpression(dataList.ItemType->Name)
					<< ");\n";
			}
		}
		else
		{
			for (size_t listIndex = 0;
				listIndex < staticDataLists.size(); ++listIndex)
			{
				const auto& dataList = staticDataLists[listIndex];
				cpp << "\tauto " << dataList.VariableName
					<< " = std::make_shared<ObservableBindingList>(L\""
					<< EscapeWStringLiteral(dataList.ItemType->Name)
					<< "\");\n";
				for (size_t recordIndex = 0;
					recordIndex < dataList.Definition->Records.size();
					++recordIndex)
				{
					const auto& record =
						dataList.Definition->Records[recordIndex];
					const auto recordName = "__dataRecord_"
						+ std::to_string(listIndex + 1) + "_"
						+ std::to_string(recordIndex + 1);
					cpp << "\t{\n";
					cpp << "\t\tauto " << recordName
						<< " = std::make_shared<ObservableObject>();\n";
					std::map<std::wstring, std::string> objectVariables;
					objectVariables.emplace(L"", recordName);
					for (size_t propertyIndex = 0;
						propertyIndex < dataList.ItemType->Properties.size();
						++propertyIndex)
					{
						const auto& property =
							dataList.ItemType->Properties[propertyIndex];
						const auto parent = objectVariables.find(
							parentPath(property.Path));
						if (parent == objectVariables.end())
							throw std::invalid_argument(
								"Static DataList object hierarchy is incomplete");
						const auto suffix = std::to_string(listIndex + 1)
							+ "_" + std::to_string(recordIndex + 1)
							+ "_" + std::to_string(propertyIndex + 1);
						const auto valueName = "__dataValue_" + suffix;
						std::string valueExpression;
						if (property.ValueKind == BindingValueKind::Object)
						{
							if (property.ObjectKind
								== DesignerDataObjectKind::BindingSource)
							{
								const auto nestedName =
									"__dataObject_" + suffix;
								cpp << "\t\tauto " << nestedName
									<< " = std::make_shared<ObservableObject>();\n";
								objectVariables[property.Path] = nestedName;
								valueExpression =
									"BindingValue(BindingSourceReference("
									+ nestedName + "))";
							}
							else if (property.ObjectKind
								== DesignerDataObjectKind::BindingList)
							{
								const auto nestedName =
									"__dataNestedList_" + suffix;
								cpp << "\t\tauto " << nestedName
									<< " = std::make_shared<ObservableBindingList>(L\""
									<< EscapeWStringLiteral(property.ItemType)
									<< "\");\n";
								valueExpression =
									"BindingValue(BindingListReference("
									+ nestedName + "))";
							}
							else valueExpression =
								"BindingValue(std::shared_ptr<void>{})";
						}
						else
						{
							auto value = defaultDataValue(property.ValueKind);
							if (const auto field =
								record.Fields.find(property.Path);
								field != record.Fields.end())
							{
								const auto kind =
									styleKindForDataValue(property.ValueKind);
								std::wstring conversionError;
								if (!kind
									|| !DesignerStyleSheetUtils::TryConvertValue(
										{ *kind, field->second }, value,
										&conversionError,
										canonicalDataDocument.ResourceBasePath,
										canonicalDataDocument.Resources))
									throw std::invalid_argument(
										"Static DataList scalar conversion failed: "
										+ WStringToString(conversionError));
							}
							valueExpression =
								GenerateBindingValueExpression(value);
						}
						cpp << "\t\tconst auto " << valueName
							<< " = " << valueExpression << ";\n";
						cpp << "\t\tif (!" << parent->second
							<< "->DefineProperty({ L\""
							<< EscapeWStringLiteral(leafName(property.Path))
							<< "\", " << valueName << ".Kind(), "
							"std::type_index(" << valueName << ".Type()), "
							<< (property.CanRead ? "true" : "false") << ", "
							<< (property.CanWrite ? "true" : "false") << ", "
							<< (property.CanObserve ? "true" : "false")
							<< " }, " << valueName << "))\n";
						cpp << "\t\t\tthrow std::runtime_error("
							"\"Generated DataList property construction failed\");\n";
					}
					cpp << "\t\t" << dataList.VariableName
						<< "->Items.push_back(BindingSourceReference("
						<< recordName << "));\n";
					cpp << "\t}\n";
				}
			}
		}
		cpp << "\n";
	}
	if (!staticCollectionViews.empty())
	{
		auto sortDirectionExpression = [](CollectionSortDirection value)
		{
			return value == CollectionSortDirection::Descending
				? "CollectionSortDirection::Descending"
				: "CollectionSortDirection::Ascending";
		};
		auto aggregateFunctionExpression =
			[](CollectionAggregateFunction value)
		{
			switch (value)
			{
			case CollectionAggregateFunction::Sum:
				return "CollectionAggregateFunction::Sum";
			case CollectionAggregateFunction::Average:
				return "CollectionAggregateFunction::Average";
			case CollectionAggregateFunction::Min:
				return "CollectionAggregateFunction::Min";
			case CollectionAggregateFunction::Max:
				return "CollectionAggregateFunction::Max";
			default:
				return "CollectionAggregateFunction::Count";
			}
		};
		auto filterOperatorExpression = [](CollectionFilterOperator value)
		{
			switch (value)
			{
			case CollectionFilterOperator::NotEquals:
				return "CollectionFilterOperator::NotEquals";
			case CollectionFilterOperator::LessThan:
				return "CollectionFilterOperator::LessThan";
			case CollectionFilterOperator::LessThanOrEqual:
				return "CollectionFilterOperator::LessThanOrEqual";
			case CollectionFilterOperator::GreaterThan:
				return "CollectionFilterOperator::GreaterThan";
			case CollectionFilterOperator::GreaterThanOrEqual:
				return "CollectionFilterOperator::GreaterThanOrEqual";
			case CollectionFilterOperator::Contains:
				return "CollectionFilterOperator::Contains";
			case CollectionFilterOperator::StartsWith:
				return "CollectionFilterOperator::StartsWith";
			case CollectionFilterOperator::EndsWith:
				return "CollectionFilterOperator::EndsWith";
			case CollectionFilterOperator::IsEmpty:
				return "CollectionFilterOperator::IsEmpty";
			case CollectionFilterOperator::IsNotEmpty:
				return "CollectionFilterOperator::IsNotEmpty";
			default:
				return "CollectionFilterOperator::Equals";
			}
		};

		cpp << "\t// CollectionViewSource is configured entirely from native "
			"CUI descriptors before its source is attached.\n";
		for (const auto& view : staticCollectionViews)
		{
			const auto& definition = *view.Definition;
			cpp << "\tauto " << view.VariableName
				<< " = std::make_shared<CollectionViewSource>();\n";
			const auto* itemSchema = view.ItemType
				? &view.ItemType->Properties : nullptr;
			std::vector<std::string> groupPaths;
			groupPaths.reserve(definition.GroupDescriptions.size());
			for (const auto& group : definition.GroupDescriptions)
				groupPaths.push_back(emitCompiledBindingPath(
					group.PropertyName, itemSchema, "\t",
					"Static CollectionViewSource GroupDescription", nullptr, {}, {}));
			std::vector<std::string> aggregatePaths;
			aggregatePaths.reserve(
				definition.AggregateDescriptions.size());
			for (const auto& aggregate : definition.AggregateDescriptions)
			{
				if (aggregate.PropertyName.empty())
					aggregatePaths.emplace_back(
						"CompiledBindingPathView{}");
				else
					aggregatePaths.push_back(emitCompiledBindingPath(
						aggregate.PropertyName, itemSchema, "\t",
						"Static CollectionViewSource AggregateDescription", nullptr, {}, {}));
			}
			std::vector<std::string> sortPaths;
			sortPaths.reserve(definition.SortDescriptions.size());
			for (const auto& sort : definition.SortDescriptions)
				sortPaths.push_back(emitCompiledBindingPath(
					sort.PropertyName, itemSchema, "\t",
					"Static CollectionViewSource SortDescription", nullptr, {}, {}));
			std::vector<std::string> filterPaths;
			filterPaths.reserve(definition.FilterDescriptions.size());
			for (const auto& filter : definition.FilterDescriptions)
				filterPaths.push_back(emitCompiledBindingPath(
					filter.PropertyName, itemSchema, "\t",
					"Static CollectionViewSource FilterDescription", nullptr, {}, {}));
			if (!definition.GroupDescriptions.empty())
			{
				cpp << "\t" << view.VariableName
					<< "->SetGroupDescriptions({\n";
				for (size_t groupIndex = 0;
					groupIndex < definition.GroupDescriptions.size();
					++groupIndex)
				{
					const auto& group =
						definition.GroupDescriptions[groupIndex];
					cpp << "\t\tCollectionGroupDescription::FromCompiledPath("
						<< groupPaths[groupIndex] << ", "
						<< sortDirectionExpression(group.Direction)
						<< ", " << (group.IgnoreCase ? "true" : "false")
						<< "),\n";
				}
				cpp << "\t});\n";
			}
			if (!definition.AggregateDescriptions.empty())
			{
				cpp << "\t" << view.VariableName
					<< "->SetAggregateDescriptions({\n";
				for (size_t aggregateIndex = 0;
					aggregateIndex < definition.AggregateDescriptions.size();
					++aggregateIndex)
				{
					const auto& aggregate =
						definition.AggregateDescriptions[aggregateIndex];
					cpp << "\t\tCollectionAggregateDescription::FromCompiledPath(L\""
						<< EscapeWStringLiteral(aggregate.Name)
						<< "\", " << aggregatePaths[aggregateIndex]
						<< ", "
						<< aggregateFunctionExpression(
							aggregate.Function)
						<< "),\n";
				}
				cpp << "\t});\n";
			}
			if (!definition.SortDescriptions.empty())
			{
				cpp << "\t" << view.VariableName
					<< "->SetSortDescriptions({\n";
				for (size_t sortIndex = 0;
					sortIndex < definition.SortDescriptions.size(); ++sortIndex)
				{
					const auto& sort =
						definition.SortDescriptions[sortIndex];
					cpp << "\t\tCollectionSortDescription::FromCompiledPath("
						<< sortPaths[sortIndex] << ", "
						<< sortDirectionExpression(sort.Direction)
						<< ", " << (sort.IgnoreCase ? "true" : "false")
						<< "),\n";
				}
				cpp << "\t});\n";
			}
			if (!definition.FilterDescriptions.empty())
			{
				cpp << "\t" << view.VariableName
					<< "->SetFilterDescriptions({\n";
				for (size_t filterIndex = 0;
					filterIndex < definition.FilterDescriptions.size();
					++filterIndex)
				{
					const auto& filter =
						definition.FilterDescriptions[filterIndex];
					cpp << "\t\tCollectionFilterDescription::FromCompiledPath("
						<< filterPaths[filterIndex] << ", "
						<< filterOperatorExpression(filter.Operator)
						<< ", "
						<< GenerateBindingValueExpression(
							view.FilterValues[filterIndex])
						<< ", "
						<< (filter.IgnoreCase ? "true" : "false")
						<< "),\n";
				}
				cpp << "\t});\n";
			}
			cpp << "\t" << view.VariableName
				<< "->SetSource(BindingListReference("
				<< view.SourceVariableName << "));\n";
			cpp << "\tif (" << view.VariableName
				<< "->GetSource().Get() != "
				<< view.SourceVariableName << ".get())\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated CollectionViewSource installation failed\");\n";
		}
		cpp << "\n";
	}
	if (!staticDataTemplates.empty())
	{
		std::string itemTemplateCapture = "[";
		for (size_t index = 0;
			index < sharedDocumentResourceDeclarations.size(); ++index)
		{
			if (index > 0) itemTemplateCapture += ", ";
			itemTemplateCapture +=
				sharedDocumentResourceDeclarations[index].second;
		}
		itemTemplateCapture += "]";
		cpp << "\t// DataTemplate resources are repeatable native visual "
			"factories. They retain only typed CUI callbacks and values.\n";
		for (size_t templateIndex = 0;
			templateIndex < staticDataTemplates.size(); ++templateIndex)
		{
			const auto& lowered = staticDataTemplates[templateIndex];
			const auto& definition = *lowered.Definition;
			const auto& sourceDefinition = *lowered.SourceDefinition;
			const auto& nodes = sourceDefinition.Template;
			std::unordered_map<std::wstring, std::string>
				dataTemplateControlExpressions;
			for (const auto& node : nodes)
				dataTemplateControlExpressions.emplace(
					node.Name, GetVarName(node));
			auto findTemplateNodeById =
				[&](int id) -> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					nodes.begin(), nodes.end(),
					[&](const auto& node) { return node.Id == id; });
				return found == nodes.end() ? nullptr : &*found;
			};
			auto findTemplateNodeByName =
				[&](const std::wstring& name)
				-> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					nodes.begin(), nodes.end(),
					[&](const auto& node) { return node.Name == name; });
				return found == nodes.end() ? nullptr : &*found;
			};
			auto templateParent =
				[&](const DesignerModel::DesignNode& node)
				-> const DesignerModel::DesignNode*
			{
				if (node.ParentId > 0)
					return findTemplateNodeById(node.ParentId);
				if (!node.ParentRef.empty())
					return findTemplateNodeByName(node.ParentRef);
				return nullptr;
			};
			std::vector<const DesignerModel::DesignNode*> roots;
			std::unordered_map<const DesignerModel::DesignNode*,
				std::vector<const DesignerModel::DesignNode*>>
				templateChildren;
			for (const auto& node : nodes)
			{
				const auto* parent = templateParent(node);
				templateChildren[parent].push_back(&node);
				if (!parent) roots.push_back(&node);
			}
			if (roots.size() != 1)
				throw std::invalid_argument(
					"Static DataTemplate has no unique visual root");
			const auto* root = roots.front();

			cpp << "\tauto " << lowered.VariableName
				<< " = std::make_shared<CuiGeneratedItemTemplate>(\n";
			cpp << "\t\t";
			if (dynamicWindow)
				cpp << "L\"" << EscapeWStringLiteral(definition.DataType)
					<< "\"";
			else cpp << GeneratedDataTypeTokenExpression(definition.DataType);
			cpp << ", " << (definition.Hierarchical ? "true" : "false")
				<< ",\n";
			cpp << "\t\t" << itemTemplateCapture
				<< "(const BindingSourceReference& item, size_t index, "
					"std::wstring* outError) -> std::unique_ptr<Control>\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\t(void)index;\n";
			cpp << "\t\t\tauto fail = [outError](std::wstring message) "
				"-> std::unique_ptr<Control>\n";
			cpp << "\t\t\t{\n";
			cpp << "\t\t\t\tif (outError) *outError = std::move(message);\n";
			cpp << "\t\t\t\treturn {};\n";
			cpp << "\t\t\t};\n";
			cpp << "\t\t\tif (!item)\n";
			cpp << "\t\t\t\treturn fail(L\"DataTemplate 缺少当前数据项。\");\n";
			cpp << "\t\t\ttry\n";
			cpp << "\t\t\t{\n";
			for (const auto& node : nodes)
			{
				auto localNode = node;
				localNode.TemplateState.Generated = true;
				cpp << GenerateControlInstantiation(localNode, 4);
			}
			for (const auto& node : nodes)
			{
				cpp << GenerateControlCommonProperties(node, 4);
				cpp << GenerateAuthoredProperties(
					node, 4, &sharedDocumentResources,
					&sharedDocumentResourceAssignments);
				cpp << GenerateContainerProperties(node, 4);
			}

			auto sortTemplateChildren = [](auto& children)
			{
				std::stable_sort(
					children.begin(), children.end(),
					[](const auto* left, const auto* right)
					{
						const auto leftOrder = left->Order < 0
							? (std::numeric_limits<int>::max)()
							: left->Order;
						const auto rightOrder = right->Order < 0
							? (std::numeric_limits<int>::max)()
							: right->Order;
						return leftOrder < rightOrder;
					});
			};
			std::function<void(const DesignerModel::DesignNode*)>
				emitDataTemplateChildren;
			emitDataTemplateChildren =
				[&](const DesignerModel::DesignNode* parent)
			{
				auto found = templateChildren.find(parent);
				if (found == templateChildren.end()) return;
				auto children = found->second;
				sortTemplateChildren(children);
				for (const auto* child : children)
				{
					emitDataTemplateChildren(child);
					const auto parentVar = GetVarName(*parent);
					const auto childVar = GetVarName(*child);
					const auto isVisualHeader =
						child->Structure.ChildRole
						== DesignerModel::DesignNodeChildRole::Header;
					if (isVisualHeader
						&& (IsUIClassAssignableFrom(
								UIClass::UI_HeaderedContentControl,
								parent->Type)
							|| IsUIClassAssignableFrom(
								UIClass::UI_HeaderedItemsControl,
								parent->Type)))
						cpp << "\t\t\t\t" << parentVar
							<< "->SetVisualHeader(std::move(__owned_"
							<< childVar << "));\n";
					else if (IsUIClassAssignableFrom(
						UIClass::UI_ItemsControl, parent->Type))
						cpp << "\t\t\t\t" << parentVar
							<< "->AddItemControl(std::move(__owned_"
							<< childVar << "));\n";
					else if (IsUIClassAssignableFrom(
						UIClass::UI_ContentControl, parent->Type))
						cpp << "\t\t\t\t" << parentVar
							<< "->SetVisualContent(std::move(__owned_"
							<< childVar << "));\n";
					else if (parent->Type == UIClass::UI_Popup)
						cpp << "\t\t\t\t" << parentVar
							<< "->SetChild(std::move(__owned_"
							<< childVar << "));\n";
					else if (IsUIClassAssignableFrom(
						UIClass::UI_Decorator, parent->Type))
						cpp << "\t\t\t\t" << parentVar
							<< "->SetChild(std::move(__owned_"
							<< childVar << "));\n";
					else cpp << "\t\t\t\t" << parentVar
						<< "->AddOwned(std::move(__owned_"
						<< childVar << "));\n";
				}
			};
			emitDataTemplateChildren(root);
			for (const auto& node : nodes)
			{
				if (!node.Structure.RelativePanel
					|| node.Structure.RelativePanel->Empty()) continue;
				const auto* parent = templateParent(node);
				cpp << GenerateRelativePanelConstraints(
					node,
					parent
						? GetVarName(*parent)
						: "static_cast<Control*>(nullptr)",
					dataTemplateControlExpressions,
					4,
					true);
			}
			const auto rootVar = GetVarName(*root);
			cpp << "\t\t\t\tif (!" << rootVar
				<< "->SetDataContext(item))\n";
			cpp << "\t\t\t\t\treturn fail("
				"L\"DataTemplate DataContext 安装失败。\");\n";

			for (const auto& node : nodes)
			{
				if (node.Bindings.empty()) continue;
				const auto nodeVar = GetVarName(node);
				const auto* itemType =
					canonicalDataDocument.FindDataType(definition.DataType);
				const StaticCompiledDataRecordClass* itemRecordClass = nullptr;
				if (itemType)
				{
					const auto foundRecord = std::find_if(
						staticCompiledDataRecordClasses.begin(),
						staticCompiledDataRecordClasses.end(),
						[&](const auto& candidate)
						{
							return candidate.ItemType == itemType
								&& candidate.ContextPath.empty();
						});
					if (foundRecord != staticCompiledDataRecordClasses.end())
						itemRecordClass = &*foundRecord;
				}
				auto resolveDataTemplateBindingEndpoint = [&] (
					const DesignerDataBinding& sourceBinding,
					const std::wstring& targetProperty,
					bool multiSource,
					std::string_view indent,
					const char* context)
				{
					if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::TemplatedParent)
						throw std::invalid_argument(
							"Static DataTemplate has no TemplatedParent");
					StaticBindingSourceSpec source;
					source.AdapterExpression = multiSource
						? "static_cast<IBindingSource*>(&"
							+ nodeVar + "->DataContextSource())"
						: nodeVar + "->DataContextSource()";
					source.SourceSchema = itemType
						? &itemType->Properties : nullptr;
					if (!sourceBinding.ElementName.empty())
					{
						const auto* sourceNode = findTemplateNodeByName(
							sourceBinding.ElementName);
						if (!sourceNode)
							throw std::invalid_argument(
								"Static DataTemplate ElementName is unresolved");
						const auto sourceVar = GetVarName(*sourceNode);
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(" + sourceVar + ")"
							: "*" + sourceVar;
						source.DirectObjectExpression = "*" + sourceVar;
						source.DirectNode = sourceNode;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::Self)
					{
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(" + nodeVar + ")"
							: "*" + nodeVar;
						source.DirectObjectExpression = "*" + nodeVar;
						source.DirectNode = &node;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						source.AdapterExpression =
							"cui::binding::CreateFindAncestorSource(*"
							+ nodeVar + ", "
							+ GeneratedFindAncestorTypeExpression(
								_sourceDocument, sourceBinding) + ", "
							+ std::to_string(sourceBinding.AncestorLevel) + ")";
						source.SourceSchema = nullptr;
					}
					else if (targetProperty == L"DataContext")
					{
						if (&node == root)
							source.AdapterExpression = multiSource
								? "item" : "*item.Get()";
						else if (multiSource)
							source.AdapterExpression =
								"static_cast<IBindingSource*>(&"
								+ nodeVar + "->GetInheritanceParent()->"
									"DataContextSource())";
						else
						{
							source.Guard = nodeVar
								+ "->GetInheritanceParent() && ";
							source.AdapterExpression = nodeVar
								+ "->GetInheritanceParent()->DataContextSource()";
						}
					}
					if (sourceBinding.ElementName.empty()
						&& sourceBinding.RelativeSource
							== DesignerBindingRelativeSource::None
						&& itemRecordClass)
					{
						source.DirectRecordClass = itemRecordClass;
						source.DirectRecordExpression = "*item.Get()";
					}
					return lowerBindingSourceEndpoint(
						sourceBinding, source, indent, context);
				};
				for (const auto& [targetProperty, binding] : node.Bindings)
				{
					const auto targetIdentity =
						FindGeneratedDependencyPropertyExpression(
							node, targetProperty, true);
					if (targetIdentity.empty())
						throw std::invalid_argument(
							"Static DataTemplate Binding target has no writable "
							"DependencyProperty identity");
					cpp << "\t\t\t\t{\n";
					if (binding.IsMultiBinding())
					{
						auto resolveChild = [&] (
							const DesignerDataBinding& child,
							std::string_view indent,
							const char* context)
						{
							return resolveDataTemplateBindingEndpoint(
								child, targetProperty, true, indent, context);
						};
						emitStaticMultiBinding(
							binding, nodeVar, targetIdentity,
							resolveChild, "\t\t\t\t\t",
							"Static DataTemplate MultiBinding", "attached");
					}
					else
					{
						const auto options = lowerBindingOptions(binding);
						const auto endpoint = resolveDataTemplateBindingEndpoint(
							binding, targetProperty, false, "\t\t\t\t\t",
							"Static DataTemplate Binding");
					if (!options.ConverterName.empty())
					{
						cpp << "\t\t\t\t\tauto converter = "
							<< bindingConverterExpression(
								options.ConverterName, binding)
							<< ";\n";
						cpp << "\t\t\t\t\tif (!converter)\n";
						cpp << "\t\t\t\t\t\treturn fail("
							"L\"DataTemplate Binding Converter 不存在。\");\n";
					}
					if (endpoint.UsesDirectSource())
						cpp << "\t\t\t\t\t"
							<< (endpoint.DirectCompiledRecord
								? "// CUI:AOT binding-source=direct-record\n"
								: "// CUI:AOT binding-source=direct-dp\n");
					cpp << "\t\t\t\t\tconst bool attached = "
						<< endpoint.Guard << nodeVar
						<< "->DataBindings.Add(" << targetIdentity
						<< ", " << endpoint.SourceOperand << ", ";
					if (!endpoint.UsesDirectSource())
						cpp << endpoint.SourcePathOperand << ", ";
					cpp
						<< BindingModeToExpr(binding.Mode)
						<< ", "
						<< DataSourceUpdateModeToExpr(
							binding.UpdateMode);
					if (!options.ConverterName.empty())
						cpp << ", std::move(converter)";
					else if (options.HasExtendedOptions)
						cpp << ", {}";
					if (options.HasExtendedOptions)
						cpp << ", " << options.Fallback
							<< ", " << options.TargetNull
							<< ", " << options.ConverterParameter
							<< ", " << options.StringFormat;
					cpp << ") != nullptr;\n";
					}
					cpp << "\t\t\t\t\tif (!attached)\n";
					cpp << "\t\t\t\t\t\treturn fail("
						"L\"DataTemplate Binding 安装失败。\");\n";
					cpp << "\t\t\t\t}\n";
				}
			}
			cpp << "\t\t\t\tif (outError) outError->clear();\n";
			cpp << "\t\t\t\treturn std::move(__owned_"
				<< rootVar << ");\n";
			cpp << "\t\t\t}\n";
			cpp << "\t\t\tcatch (const std::exception& error)\n";
			cpp << "\t\t\t{\n";
			cpp << "\t\t\t\treturn fail("
				"L\"DataTemplate 静态构造发生运行时异常：\" "
				"+ Convert::StringToWString(error.what()));\n";
			cpp << "\t\t\t}\n";
			cpp << "\t\t\tcatch (...)\n";
			cpp << "\t\t\t{\n";
			cpp << "\t\t\t\treturn fail("
				"L\"DataTemplate 静态构造发生未知异常。\");\n";
			cpp << "\t\t\t}\n";
			cpp << "\t\t}";

			if (definition.Hierarchical
				&& definition.ItemsSourceBinding)
			{
				const auto& binding = *definition.ItemsSourceBinding;
				cpp << ",\n";
				cpp << "\t\t[](const BindingSourceReference& item, "
					"BindingListReference& out, std::wstring* outError)\n";
				cpp << "\t\t{\n";
				cpp << "\t\t\tout = {};\n";
				cpp << "\t\t\tif (!item)\n";
				cpp << "\t\t\t{\n";
				cpp << "\t\t\t\tif (outError) *outError = "
					"L\"HierarchicalDataTemplate 缺少当前数据项。\";\n";
				cpp << "\t\t\t\treturn false;\n";
				cpp << "\t\t\t}\n";
				std::string hierarchicalPathOperand = "L\""
					+ EscapeWStringLiteral(binding.SourceProperty) + "\"";
				if (!dynamicWindow)
				{
					const auto* itemType =
						canonicalDataDocument.FindDataType(
							definition.DataType);
					hierarchicalPathOperand = emitCompiledBindingPath(
						binding.SourceProperty,
						itemType ? &itemType->Properties : nullptr,
						"\t\t\t",
						"Static HierarchicalDataTemplate.ItemsSource", nullptr, {}, {});
				}
				cpp << "\t\t\tBindingValue value;\n";
				cpp << "\t\t\tif (!TryGetBindingPathValue("
					"*item.Get(), " << hierarchicalPathOperand
					<< ", value))\n";
				cpp << "\t\t\t{\n";
				cpp << "\t\t\t\tif (outError) *outError = "
					"L\"HierarchicalDataTemplate.ItemsSource "
					"无法读取路径。\";\n";
				cpp << "\t\t\t\treturn false;\n";
				cpp << "\t\t\t}\n";
				const auto converterName =
					DesignerBindingUtils::Trim(binding.Converter);
				if (!converterName.empty())
				{
					cpp << "\t\t\tauto converter = "
						<< bindingConverterExpression(converterName, binding)
						<< ";\n";
					cpp << "\t\t\tif (!converter)\n";
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tif (outError) *outError = "
						"L\"HierarchicalDataTemplate.ItemsSource "
						"Converter 不存在。\";\n";
					cpp << "\t\t\t\treturn false;\n";
					cpp << "\t\t\t}\n";
					if (binding.ConverterParameter)
						cpp << "\t\t\tconst auto parameter = "
							<< GenerateStyleValueExpression(
								*binding.ConverterParameter)
							<< ";\n";
					cpp << "\t\t\tBindingValue converted;\n";
					cpp << "\t\t\tBindingValueConverterContext context;\n";
					if (binding.ConverterParameter)
						cpp << "\t\t\tcontext.Parameter = &parameter;\n";
					cpp << "\t\t\tcontext.TargetKind = "
						"BindingValueKind::Object;\n";
					cpp << "\t\t\tif (!converter->Convert("
						"value, context, converted))\n";
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tif (outError) *outError = "
						"L\"HierarchicalDataTemplate.ItemsSource "
						"Converter 转换失败。\";\n";
					cpp << "\t\t\t\treturn false;\n";
					cpp << "\t\t\t}\n";
					cpp << "\t\t\tvalue = std::move(converted);\n";
				}
				cpp << "\t\t\tif (!value.Empty() && !value.TryGet(out))\n";
				cpp << "\t\t\t{\n";
				cpp << "\t\t\t\tif (outError) *outError = "
					"L\"HierarchicalDataTemplate.ItemsSource "
					"未返回 BindingList。\";\n";
				cpp << "\t\t\t\treturn false;\n";
				cpp << "\t\t\t}\n";
				cpp << "\t\t\tif (outError) outError->clear();\n";
				cpp << "\t\t\treturn true;\n";
				cpp << "\t\t},\n";
				cpp << "\t\t[](const BindingSourceReference& item, "
					"std::function<void()> changed)\n";
				cpp << "\t\t{\n";
				std::string hierarchicalObservePathOperand = "L\""
					+ EscapeWStringLiteral(binding.SourceProperty) + "\"";
				if (!dynamicWindow)
				{
					const auto* itemType =
						canonicalDataDocument.FindDataType(
							definition.DataType);
					hierarchicalObservePathOperand = emitCompiledBindingPath(
						binding.SourceProperty,
						itemType ? &itemType->Properties : nullptr,
						"\t\t\t",
						"Static HierarchicalDataTemplate observation", nullptr, {}, {});
				}
				cpp << "\t\t\treturn ObserveBindingPaths(item, { "
					<< hierarchicalObservePathOperand
					<< " }, std::move(changed));\n";
				cpp << "\t\t}";
			}
			cpp << ");\n";
		}
		cpp << "\n";
	}
	if (!staticGroupStyles.empty())
	{
		cpp << "\t// GroupStyle resources retain the already-compiled native "
			"header DataTemplate identity.\n";
		for (const auto& groupStyle : staticGroupStyles)
		{
			cpp << "\tauto " << groupStyle.VariableName
				<< " = std::make_shared<GroupStyle>();\n";
			if (!groupStyle.HeaderTemplateVariableName.empty())
				cpp << "\t" << groupStyle.VariableName
					<< "->HeaderTemplate = ItemTemplateReference("
					<< groupStyle.HeaderTemplateVariableName << ");\n";
		}
		cpp << "\n";
	}
	cpp << "\t// 创建控件\n";

	// 1) 先实例化所有可设计控件（不做 AdoptVisualChild）。x:Reference
	// wiring is emitted only after every member pointer has been assigned.
	for (const auto& node : _sourceDocument.Nodes)
		if (!node.TemplateState.Generated)
			cpp << GenerateControlInstantiation(node, 1);
	if (std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node) { return !node.TemplateState.Generated; }))
		cpp << "\n";
		bool emittedStaticItemsPanels = false;
		for (size_t nodeIndex = 0;
			nodeIndex < _sourceDocument.Nodes.size(); ++nodeIndex)
		{
			const auto& node = _sourceDocument.Nodes[nodeIndex];
			if (node.TemplateState.Generated
				|| node.Structure.ItemsPanel.empty()) continue;
			const auto* definition =
				canonicalDataDocument.FindItemsPanelTemplate(
					canonicalDataDocument.Nodes[nodeIndex],
					node.Structure.ItemsPanel);
			const auto panel = definition
				? staticItemsPanelVariablesByDefinition.find(definition)
				: staticItemsPanelVariablesByDefinition.end();
			if (panel == staticItemsPanelVariablesByDefinition.end())
				throw std::invalid_argument(
					"Lexical ItemsPanel resource has no native lowering");
		if (!IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, node.Type))
			throw std::invalid_argument(
				"Static ItemsPanel target is not an ItemsControl");
		if (!emittedStaticItemsPanels)
		{
			cpp << "\t// Install the native ItemsPanel before ItemsSource "
				"creates any item containers.\n";
			emittedStaticItemsPanels = true;
		}
		const auto control = GetVarName(node);
		cpp << "\t" << control
			<< "->SetItemsPanel(ItemsPanelTemplateReference("
			<< panel->second << "));\n";
		cpp << "\tif (" << control
			<< "->GetItemsPanel().Get() != "
			<< panel->second << ".get())\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated ItemsPanel installation failed\");\n";
	}
	if (emittedStaticItemsPanels) cpp << "\n";
	bool emittedStaticItemTemplates = false;
	auto staticItemTypeForResource =
		[&](const std::wstring& resourceKey) -> std::wstring
	{
		if (const auto* dataList =
			canonicalDataDocument.FindDataList(resourceKey))
			return dataList->ItemType;
		if (const auto* view =
			canonicalDataDocument.FindCollectionView(resourceKey))
		{
			if (const auto* dataList =
				canonicalDataDocument.FindDataList(
					view->SourceResource))
				return dataList->ItemType;
		}
		return {};
	};
	auto staticImplicitItemTypeForNode =
		[&](const DesignerModel::DesignNode& node) -> std::wstring
	{
		if (!node.Structure.ItemsSourceResource.empty())
			return staticItemTypeForResource(
				node.Structure.ItemsSourceResource);
		const auto binding = node.Bindings.find(L"ItemsSource");
		if (binding == node.Bindings.end()
			|| binding->second.IsMultiBinding()
			|| !binding->second.ElementName.empty()
			|| binding->second.RelativeSource
				!= DesignerBindingRelativeSource::None)
			return {};
		const auto* property =
			DesignerDataContextSchemaUtils::Find(
				canonicalDataDocument.DataContextSchema,
				binding->second.SourceProperty);
		return property
			&& property->ObjectKind
				== DesignerDataObjectKind::BindingList
			? property->ItemType : std::wstring{};
	};
	std::unordered_map<const DesignerModel::DesignNode*,
		std::unordered_set<std::wstring>> compiledMemberPathProperties;
	bool emittedCompiledMemberPaths = false;
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated
			|| !IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, node.Type)) continue;
		const auto itemTypeName = staticImplicitItemTypeForNode(node);
		const auto* itemType = itemTypeName.empty()
			? nullptr : canonicalDataDocument.FindDataType(itemTypeName);
		auto emitMemberPath = [&](const wchar_t* propertyName,
			const char* setterName, const char* context)
		{
			const auto* assignment = node.Properties.Find(propertyName);
			if (!assignment) return;
			compiledMemberPathProperties[&node].insert(propertyName);
			const auto path = DesignerDataContextSchemaUtils::NormalizePath(
				assignment->Value.Text);
			if (path.empty()) return;
			if (!itemType)
				throw std::invalid_argument(
					std::string(context)
						+ " requires a statically known ItemsSource ItemType");
			const auto operand = emitCompiledBindingPath(
				path, &itemType->Properties, "\t", context, nullptr, {}, {});
			if (!emittedCompiledMemberPaths)
			{
				cpp << "\t// Item display/selection member paths are immutable "
					"token tables installed before ItemsSource realization.\n";
				emittedCompiledMemberPaths = true;
			}
			cpp << "\t" << GetVarName(node) << "->" << setterName
				<< "(" << operand << ");\n";
		};
		emitMemberPath(
			L"DisplayMemberPath", "SetCompiledDisplayMemberPath",
			"Static ItemsControl DisplayMemberPath");
		if (IsUIClassAssignableFrom(UIClass::UI_Selector, node.Type)
			|| node.Type == UIClass::UI_TreeView)
				emitMemberPath(
					L"SelectedValuePath", "SetCompiledSelectedValuePath",
					"Static Selector SelectedValuePath");
	}
	auto boundObjectDataType = [&](const DesignerModel::DesignNode& node,
		const wchar_t* targetProperty,
		const std::wstring& templateKey)
		-> const DesignerModel::DesignDataTypeDefinition*
	{
		const auto binding = node.Bindings.find(targetProperty);
		if (binding != node.Bindings.end()
			&& !binding->second.IsMultiBinding()
			&& binding->second.ElementName.empty()
			&& binding->second.RelativeSource
				== DesignerBindingRelativeSource::None)
		{
			const auto* property = DesignerDataContextSchemaUtils::Find(
				canonicalDataDocument.DataContextSchema,
				binding->second.SourceProperty);
			if (property && property->ObjectKind
				== DesignerDataObjectKind::BindingSource)
				if (const auto* type = canonicalDataDocument.FindDataType(
					property->DataType)) return type;
		}
		if (!templateKey.empty())
			if (const auto* dataTemplate =
				canonicalDataDocument.FindDataTemplate(templateKey))
				return canonicalDataDocument.FindDataType(
					dataTemplate->DataType);
		return nullptr;
	};
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		auto emitSingleValuePath = [&] (
			const wchar_t* propertyName,
			const char* setterName,
			const char* context,
			const DesignerModel::DesignDataTypeDefinition* dataType)
		{
			const auto* assignment = node.Properties.Find(propertyName);
			if (!assignment) return;
			compiledMemberPathProperties[&node].insert(propertyName);
			const auto path = DesignerDataContextSchemaUtils::NormalizePath(
				assignment->Value.Text);
			if (path.empty()) return;
			if (!dataType)
				throw std::invalid_argument(
					std::string(context)
						+ " requires a statically known DataType");
			const auto operand = emitCompiledBindingPath(
				path, &dataType->Properties, "\t", context, nullptr, {}, {});
			if (!emittedCompiledMemberPaths)
			{
				cpp << "\t// Display member paths are immutable token tables "
					"installed before content materialization.\n";
				emittedCompiledMemberPaths = true;
			}
			cpp << "\t" << GetVarName(node) << "->" << setterName
				<< "(" << operand << ");\n";
		};
		if (node.Type == UIClass::UI_ContentPresenter
			|| IsUIClassAssignableFrom(
				UIClass::UI_ContentControl, node.Type))
			emitSingleValuePath(
				L"DisplayMemberPath", "SetCompiledDisplayMemberPath",
				"Static Content DisplayMemberPath",
				boundObjectDataType(
					node, L"Content", node.Structure.ContentTemplate));
		if (IsUIClassAssignableFrom(
				UIClass::UI_HeaderedContentControl, node.Type)
			|| IsUIClassAssignableFrom(
				UIClass::UI_HeaderedItemsControl, node.Type))
			emitSingleValuePath(
				L"HeaderDisplayMemberPath",
				"SetCompiledHeaderDisplayMemberPath",
				"Static HeaderDisplayMemberPath",
				boundObjectDataType(
					node, L"Header", node.Structure.HeaderTemplate));
	}
	if (emittedCompiledMemberPaths) cpp << "\n";

	bool emittedDataGridColumns = false;
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated
			|| node.Type != UIClass::UI_DataGrid) continue;

		const auto control = GetVarName(node);
		bool autoGenerateColumns = true;
		if (node.Bindings.contains(L"AutoGenerateColumns"))
			throw std::invalid_argument(
				"DataGrid.AutoGenerateColumns Binding cannot be installed before "
				"ItemsSource realization; use a scalar value");
		if (const auto* assignment =
			node.Properties.Find(L"AutoGenerateColumns"))
		{
			if (!assignment->ResourceKey.empty()
				|| !assignment->DynamicResourceKey.empty())
				throw std::invalid_argument(
					"DataGrid.AutoGenerateColumns must be a scalar value so it "
					"can be installed before ItemsSource realization");
			const auto typed = FindGeneratedProperty(
				node, L"AutoGenerateColumns");
			const auto valueExpression =
				GenerateStyleValueExpression(assignment->Value);
			const auto scalarValue = UnwrapBindingValue(valueExpression);
			if (scalarValue == "true") autoGenerateColumns = true;
			else if (scalarValue == "false") autoGenerateColumns = false;
			else throw std::invalid_argument(
				"DataGrid.AutoGenerateColumns must be a Boolean scalar");
			const auto call = typed
				? GenerateTypedPropertyCall(
					control, *typed, valueExpression)
				: std::string{};
			if (call.empty())
				throw std::invalid_argument(
					"DataGrid.AutoGenerateColumns has no typed early setter");
			cpp << "\t" << call << ";\n";
			compiledMemberPathProperties[&node].insert(
				L"AutoGenerateColumns");
		}

		const auto itemTypeName = staticImplicitItemTypeForNode(node);
		const auto* itemType = itemTypeName.empty()
			? nullptr : canonicalDataDocument.FindDataType(itemTypeName);
		std::vector<DesignerModel::DesignDataGridColumn> columns =
			node.Structure.DataGridColumns.value_or(
				std::vector<DesignerModel::DesignDataGridColumn>{});
		const size_t explicitColumnCount = columns.size();
		if (!dynamicWindow && autoGenerateColumns)
		{
			const bool hasAuthoredItemsSource =
				!node.Structure.ItemsSourceResource.empty()
				|| node.Bindings.contains(L"ItemsSource")
				|| node.Properties.Find(L"ItemsSource") != nullptr;
			if (!itemType && hasAuthoredItemsSource)
				throw std::invalid_argument(
					"Static DataGrid AutoGenerateColumns requires a statically "
					"known ItemsSource ItemType");
			if (itemType) for (const auto& property : itemType->Properties)
			{
				if (!property.CanRead
					|| property.ValueKind == BindingValueKind::Object) continue;
				DesignerModel::DesignDataGridColumn column;
				column.Kind = property.ValueKind == BindingValueKind::Bool
					|| property.ValueKind == BindingValueKind::NullableBool
					? DesignerModel::DesignDataGridColumnKind::CheckBox
					: DesignerModel::DesignDataGridColumnKind::Text;
				column.Header = property.Path;
				column.IsReadOnly = !property.CanWrite;
				column.IsThreeState = property.ValueKind
					== BindingValueKind::NullableBool;
				column.SortMemberPath = property.Path;
				DesignerDataBinding binding;
				binding.SourceProperty = property.Path;
				binding.Mode = property.CanWrite
					? BindingMode::Default : BindingMode::OneWay;
				column.Binding = std::move(binding);
				columns.push_back(std::move(column));
			}
		}
		if (!node.Structure.DataGridColumns && columns.empty()) continue;

		if (!emittedDataGridColumns)
		{
			cpp << "\t// DataGrid columns are non-visual declarations. Install "
				"their immutable binding paths before ItemsSource realizes "
				"rows.\n";
			emittedDataGridColumns = true;
		}
		cpp << "\t" << control << "->ClearColumns();\n";

		for (size_t columnIndex = 0;
			columnIndex < columns.size();
			++columnIndex)
		{
			const auto& column = columns[columnIndex];
			const auto variable = "__dataGridColumn_"
				+ std::to_string(node.Id) + "_"
				+ std::to_string(columnIndex + 1);
			const char* typeName = nullptr;
			switch (column.Kind)
			{
			case DesignerModel::DesignDataGridColumnKind::Text:
				typeName = "DataGridTextColumn";
				break;
			case DesignerModel::DesignDataGridColumnKind::CheckBox:
				typeName = "DataGridCheckBoxColumn";
				break;
			case DesignerModel::DesignDataGridColumnKind::Template:
				typeName = "DataGridTemplateColumn";
				break;
			}
			if (!typeName)
				throw std::invalid_argument(
					"DataGrid column kind has no native lowering");
			cpp << "\tauto " << variable << " = std::make_unique<"
				<< typeName << ">();\n";
			cpp << "\t" << variable << "->SetHeader(BindingValue(L\""
				<< EscapeWStringLiteral(column.Header) << "\"));\n";

			std::string widthExpression;
			switch (column.Width.Unit)
			{
			case DesignerModel::DesignDataGridLengthUnit::Auto:
				widthExpression = "DataGridLength::Auto()";
				break;
			case DesignerModel::DesignDataGridLengthUnit::SizeToHeader:
				widthExpression = "DataGridLength::SizeToHeader()";
				break;
			case DesignerModel::DesignDataGridLengthUnit::SizeToCells:
				widthExpression = "DataGridLength::SizeToCells()";
				break;
			case DesignerModel::DesignDataGridLengthUnit::Pixel:
				widthExpression = "DataGridLength("
					+ DoubleLiteral(column.Width.Value) + ")";
				break;
			case DesignerModel::DesignDataGridLengthUnit::Star:
				widthExpression = "DataGridLength::Star("
					+ DoubleLiteral(column.Width.Value) + ")";
				break;
			}
			cpp << "\t" << variable << "->SetWidth("
				<< widthExpression << ");\n";
			cpp << "\t" << variable << "->SetMinWidth("
				<< DoubleLiteral(column.MinWidth) << ");\n";
			if (std::isfinite(column.MaxWidth))
				cpp << "\t" << variable << "->SetMaxWidth("
					<< DoubleLiteral(column.MaxWidth) << ");\n";
			cpp << "\t" << variable << "->SetIsReadOnly("
				<< (column.IsReadOnly ? "true" : "false") << ");\n";
			if (column.Kind
				== DesignerModel::DesignDataGridColumnKind::CheckBox)
			{
				if (column.IsThreeState)
					cpp << "\t" << variable
						<< "->SetIsThreeState(true);\n";
			}
			else if (column.IsThreeState)
				throw std::invalid_argument(
					"IsThreeState is only valid for DataGridCheckBoxColumn");
			cpp << "\t" << variable << "->SetCanUserSort("
				<< (column.CanUserSort ? "true" : "false") << ");\n";
			cpp << "\t" << variable << "->SetCanUserResize("
				<< (column.CanUserResize ? "true" : "false") << ");\n";

			if (column.Binding)
			{
				if (column.Kind
					== DesignerModel::DesignDataGridColumnKind::Template)
					throw std::invalid_argument(
						"DataGridTemplateColumn cannot own a BoundColumn Binding");
				const auto& binding = *column.Binding;
				if (binding.IsMultiBinding()
					|| !binding.ElementName.empty()
					|| binding.RelativeSource
						!= DesignerBindingRelativeSource::None)
					throw std::invalid_argument(
						"DataGridBoundColumn currently requires a row DataContext "
						"Binding source");
				const auto path = DesignerDataContextSchemaUtils::NormalizePath(
					binding.SourceProperty);
				if (path.empty())
					throw std::invalid_argument(
						"DataGridBoundColumn Binding.Path cannot be empty");
				if (dynamicWindow)
					cpp << "\t" << variable << "->SetBindingPath(L\""
						<< EscapeWStringLiteral(path) << "\");\n";
				else
				{
					if (!itemType)
						throw std::invalid_argument(
							"Static DataGrid column Binding requires a statically "
							"known ItemsSource ItemType");
					const auto operand = emitCompiledBindingPath(
						path, &itemType->Properties, "\t",
						"Static DataGrid column Binding", nullptr, {}, {});
					cpp << "\t" << variable
						<< "->SetCompiledBindingPath(" << operand << ");\n";
				}
				cpp << "\t" << variable << "->SetBindingMode("
					<< BindingModeToExpr(binding.Mode) << ");\n";
				cpp << "\t" << variable << "->SetDataSourceUpdateMode("
					<< DataSourceUpdateModeToExpr(binding.UpdateMode) << ");\n";
				const auto options = lowerBindingOptions(binding);
				if (!options.ConverterName.empty())
					cpp << "\t" << variable << "->SetBindingConverter("
						<< bindingConverterExpression(
							options.ConverterName, binding) << ");\n";
				if (binding.FallbackValue)
					cpp << "\t" << variable << "->SetFallbackValue("
						<< options.Fallback << ");\n";
				if (binding.TargetNullValue)
					cpp << "\t" << variable << "->SetTargetNullValue("
						<< options.TargetNull << ");\n";
				if (binding.ConverterParameter)
					cpp << "\t" << variable << "->SetConverterParameter("
						<< options.ConverterParameter << ");\n";
				if (binding.StringFormat)
					cpp << "\t" << variable << "->SetStringFormat("
						<< options.StringFormat << ");\n";
			}

			if (!column.SortMemberPath.empty())
			{
				const auto path = DesignerDataContextSchemaUtils::NormalizePath(
					column.SortMemberPath);
				if (dynamicWindow)
					cpp << "\t" << variable << "->SetSortMemberPath(L\""
						<< EscapeWStringLiteral(path) << "\");\n";
				else
				{
					if (!itemType)
						throw std::invalid_argument(
							"Static DataGrid SortMemberPath requires a statically "
							"known ItemsSource ItemType");
					const auto operand = emitCompiledBindingPath(
						path, &itemType->Properties, "\t",
						"Static DataGrid SortMemberPath", nullptr, {}, {});
					cpp << "\t" << variable
						<< "->SetCompiledSortMemberPath(" << operand << ");\n";
				}
			}

			if (column.Kind
				== DesignerModel::DesignDataGridColumnKind::Template)
			{
				auto emitTemplate = [&](const std::wstring& key,
					const char* setter)
				{
					if (key.empty()) return;
					const auto found = staticDataTemplateVariables.find(key);
					if (found == staticDataTemplateVariables.end())
						throw std::invalid_argument(
							"DataGridTemplateColumn template resource has no "
							"native lowering");
					cpp << "\t" << variable << "->" << setter
						<< "(ItemTemplateReference(" << found->second << "));\n";
				};
				emitTemplate(column.CellTemplate, "SetCellTemplate");
				emitTemplate(
					column.CellEditingTemplate, "SetCellEditingTemplate");
			}

			cpp << "\t" << control << "->"
				<< (columnIndex < explicitColumnCount
					? "AddColumn" : "AddAutoGeneratedColumn")
				<< "(std::move("
				<< variable << "));\n";
		}
	}
	if (emittedDataGridColumns) cpp << "\n";
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		const auto control = GetVarName(node);
		if (node.Type == UIClass::UI_TreeView
			&& !staticImplicitDataTemplateVariables.empty())
		{
			if (!emittedStaticItemTemplates)
			{
				cpp << "\t// Install native DataTemplate references before "
					"ItemsSource can realize any item visuals.\n";
				emittedStaticItemTemplates = true;
			}
			cpp << "\t" << control << "->"
				<< (dynamicWindow
					? "SetImplicitItemTemplateResolver(["
					: "SetCompiledImplicitItemTemplateResolver([");
			size_t captureIndex = 0;
			for (const auto& [dataType, variable]
				: staticImplicitDataTemplateVariables)
			{
				(void)dataType;
				if (captureIndex++ > 0) cpp << ", ";
				cpp << variable;
			}
			if (dynamicWindow)
				cpp << "](const std::wstring& itemTypeName) "
					"-> ItemTemplateReference\n";
			else cpp << "](DataTypeToken itemType) -> ItemTemplateReference\n";
			cpp << "\t{\n";
			for (const auto& [dataType, variable]
				: staticImplicitDataTemplateVariables)
			{
				cpp << "\t\tif (";
				if (dynamicWindow)
					cpp << "itemTypeName == L\""
						<< EscapeWStringLiteral(dataType) << "\"";
				else cpp << "itemType == "
					<< GeneratedDataTypeTokenExpression(dataType);
				cpp << ") return ItemTemplateReference("
					<< variable << ");\n";
			}
			cpp << "\t\treturn {};\n";
			cpp << "\t});\n";
		}

		if (IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, node.Type))
		{
			const std::string* templateVariable = nullptr;
			if (!node.Structure.ItemTemplate.empty())
			{
				const auto found = staticDataTemplateVariables.find(
					node.Structure.ItemTemplate);
				if (found == staticDataTemplateVariables.end())
					throw std::invalid_argument(
						"Static ItemTemplate resource has no native lowering");
				templateVariable = &found->second;
			}
			else
			{
				const auto itemType =
					staticImplicitItemTypeForNode(node);
				const auto found =
					staticImplicitDataTemplateVariables.find(itemType);
				if (found != staticImplicitDataTemplateVariables.end())
					templateVariable = &found->second;
			}
			if (templateVariable)
			{
				if (!emittedStaticItemTemplates)
				{
					cpp << "\t// Install native DataTemplate references before "
						"ItemsSource can realize any item visuals.\n";
					emittedStaticItemTemplates = true;
				}
				cpp << "\t" << control
					<< "->SetItemTemplate(ItemTemplateReference("
					<< *templateVariable << "));\n";
				cpp << "\tif (" << control
					<< "->GetItemTemplate().Get() != "
					<< *templateVariable << ".get())\n";
				cpp << "\t\tthrow std::runtime_error("
					"\"Generated ItemTemplate installation failed\");\n";
			}
		}
		if (!node.Structure.ContentTemplate.empty())
		{
			const auto found = staticDataTemplateVariables.find(
				node.Structure.ContentTemplate);
			if (found == staticDataTemplateVariables.end())
				throw std::invalid_argument(
					"Static ContentTemplate resource has no native lowering");
			if (node.Type != UIClass::UI_ContentPresenter
				&& !IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, node.Type))
				throw std::invalid_argument(
					"Static ContentTemplate target is not a content host");
			if (!emittedStaticItemTemplates)
			{
				cpp << "\t// Install native DataTemplate references before "
					"data content is assigned.\n";
				emittedStaticItemTemplates = true;
			}
			cpp << "\t" << control
				<< "->SetContentTemplate(ItemTemplateReference("
				<< found->second << "));\n";
			cpp << "\tif (" << control
				<< "->GetContentTemplate().Get() != "
				<< found->second << ".get())\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated ContentTemplate installation failed\");\n";
		}
		if (!node.Structure.HeaderTemplate.empty())
		{
			const auto found = staticDataTemplateVariables.find(
				node.Structure.HeaderTemplate);
			if (found == staticDataTemplateVariables.end())
				throw std::invalid_argument(
					"Static HeaderTemplate resource has no native lowering");
			if (!IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, node.Type)
				&& !IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, node.Type))
				throw std::invalid_argument(
					"Static HeaderTemplate target is not headered");
			if (!emittedStaticItemTemplates)
			{
				cpp << "\t// Install native DataTemplate references before "
					"header content is assigned.\n";
				emittedStaticItemTemplates = true;
			}
			cpp << "\t" << control
				<< "->SetHeaderTemplate(ItemTemplateReference("
				<< found->second << "));\n";
			cpp << "\tif (" << control
				<< "->GetHeaderTemplate().Get() != "
				<< found->second << ".get())\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated HeaderTemplate installation failed\");\n";
		}
	}
	if (emittedStaticItemTemplates) cpp << "\n";
	bool emittedStaticGroupStyles = false;
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated
			|| node.Structure.GroupStyle.empty()) continue;
		const auto groupStyle = staticGroupStyleVariables.find(
			node.Structure.GroupStyle);
		if (groupStyle == staticGroupStyleVariables.end())
			throw std::invalid_argument(
				"Static GroupStyle resource has no native lowering");
		if (!IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, node.Type))
			throw std::invalid_argument(
				"Static GroupStyle target is not an ItemsControl");
		if (!emittedStaticGroupStyles)
		{
			cpp << "\t// GroupStyle is installed before ItemsSource "
				"realizes grouped headers.\n";
			emittedStaticGroupStyles = true;
		}
		const auto control = GetVarName(node);
		cpp << "\t" << control
			<< "->SetGroupStyle(GroupStyleReference("
			<< groupStyle->second << "));\n";
		cpp << "\tif (" << control
			<< "->GetGroupStyle().Get() != "
			<< groupStyle->second << ".get())\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated GroupStyle installation failed\");\n";
	}
	if (emittedStaticGroupStyles) cpp << "\n";
	if (!frameworkThemeProgram)
		cpp << "\tstd::wstring __frameworkThemeError;\n";

	auto findSourceNodeByName = [&](const std::wstring& name)
		-> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& candidate) { return candidate.Name == name; });
		return found == _sourceDocument.Nodes.end() ? nullptr : &*found;
	};

	if (!templateBlueprints.empty())
	{
		cpp << "\n\t// Repeatable pure-C++ factories for authored "
			"ControlTemplate resources.\n";
		for (const auto& blueprint : templateBlueprints)
		{
			const auto& definition =
				_sourceDocument.ControlTemplates[blueprint.SourceIndex];
			const auto* descriptor =
				CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(
					definition.TargetType);
			if (!descriptor)
				throw std::invalid_argument(
					"Static ControlTemplate TargetType descriptor is missing");
			const auto typeName = GetControlTypeName(definition.TargetType);
			const auto identityText = definition.Key.empty()
				? L"{x:Type "
					+ DesignerStyleSheetUtils::UIClassName(
						definition.TargetType) + L"}"
				: definition.Key;
			cpp << "\tauto " << blueprint.VariableName
				<< " = std::make_shared<CuiGeneratedControlTemplate>(\n";
			cpp << "\t\tUIClass::UI_"
				<< WStringToString(
					DesignerStyleSheetUtils::UIClassName(
						definition.TargetType))
				<< ", L\"" << EscapeWStringLiteral(identityText)
				<< "\", []() -> std::unique_ptr<Control>\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tauto result = std::make_unique<"
				<< typeName << ">();\n";
			cpp << "\t\t\t(void)result->ClearPropertyValues();\n";
			if (dynamicWindow)
			{
				cpp << "\t\t\tstatic const auto descriptor = "
					"DeclarativeTypeDescriptor::Create(\n";
				cpp << "\t\t\t\tRuntimeTypeId{ L\""
					<< EscapeWStringLiteral(descriptor->TypeId.NamespaceUri)
					<< "\", L\""
					<< EscapeWStringLiteral(descriptor->TypeId.LocalName)
					<< "\" }, {});\n";
				cpp << "\t\t\tif (!descriptor || "
					"!cui::framework::XamlAccess::SetTypeDescriptor("
					"*result, descriptor))\n";
				cpp << "\t\t\t\treturn {};\n";
			}
			if (dynamicWindow)
				cpp << "\t\t\t(void)cui::framework::DependencyPropertyAccess::"
					"SetValue(*result, L\"Focusable\", BindingValue("
					<< (descriptor->FocusableByDefault ? "true" : "false")
					<< "), DependencyPropertyValueSource::Theme);\n";
			else if (descriptor->FocusableByDefault)
				cpp << "\t\t\t(void)cui::framework::DependencyPropertyAccess::"
					"SetValue(*result, Control::FocusableProperty(), "
					"BindingValue(true), "
					"DependencyPropertyValueSource::Theme);\n";
			cpp << "\t\t\treturn result;\n";
			cpp << "\t\t});\n";
			cpp << "\tstd::weak_ptr<const IControlTemplate> __weak_"
				<< blueprint.VariableName.substr(2) << " = "
				<< blueprint.VariableName << ";\n";
		}
		cpp << "\n";

		for (const auto& blueprint : templateBlueprints)
		{
			const auto& definition =
				_sourceDocument.ControlTemplates[blueprint.SourceIndex];
			const auto& blueprintDocument = blueprint.Document;
			const auto blueprintOwner = std::find_if(
				blueprintDocument.Nodes.begin(),
				blueprintDocument.Nodes.end(),
				[&](const auto& node)
				{
					return !node.TemplateState.Generated
						&& node.Name == blueprint.OwnerName;
				});
			if (blueprintOwner == blueprintDocument.Nodes.end())
				throw std::invalid_argument(
					"Static ControlTemplate blueprint owner is missing");
			std::vector<const DesignerModel::DesignNode*> generatedNodes;
			for (const auto& node : blueprintDocument.Nodes)
				if (node.TemplateState.Generated)
					generatedNodes.push_back(&node);
			std::map<std::wstring,
				std::map<std::uint64_t, std::wstring>> partNamesByOwnerAndToken;
			for (const auto* node : generatedNodes)
			{
				if (!node || node->TemplateState.PartName.empty()) continue;
				ValidateGeneratedTemplatePart(
					partNamesByOwnerAndToken[node->TemplateState.Owner],
					node->TemplateState.PartName,
					"Static ControlTemplate namescope");
			}
			const auto generatedRoot = std::find_if(
				generatedNodes.begin(), generatedNodes.end(),
				[](const auto* node)
				{
					return node
						&& node->TemplateState.ControlTemplateRoot;
				});
			if (generatedRoot == generatedNodes.end())
				throw std::invalid_argument(
					"Static ControlTemplate blueprint root is missing");
			const auto templateRootVariable = GetVarName(**generatedRoot);

			auto findBlueprintNodeByName =
				[&](const std::wstring& name)
				-> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					blueprintDocument.Nodes.begin(),
					blueprintDocument.Nodes.end(),
					[&](const auto& node)
					{
						return node.Name == name
							|| (node.TemplateState.Generated
								&& node.TemplateState.PartName == name);
					});
				return found == blueprintDocument.Nodes.end()
					? nullptr : &*found;
			};
			std::unordered_map<std::wstring, std::string>
				templateControlExpressions;
			templateControlExpressions.emplace(
				blueprint.OwnerName, "&__templateOwner");
			for (const auto* node : generatedNodes)
			{
				templateControlExpressions.emplace(
					node->Name, GetVarName(*node));
				if (!node->TemplateState.PartName.empty())
					templateControlExpressions.emplace(
						node->TemplateState.PartName, GetVarName(*node));
			}
			auto templateOwnerPointerExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name == blueprint.OwnerName)
					return "&__templateOwner";
				const auto* node = findBlueprintNodeByName(name);
				if (!node || !node->TemplateState.Generated)
					throw std::invalid_argument(
						"Static ControlTemplate owner cannot be resolved");
				return GetVarName(*node);
			};
			auto templateOwnerReferenceExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name == blueprint.OwnerName)
					return "__templateOwner";
				const auto* node = findBlueprintNodeByName(name);
				if (!node || !node->TemplateState.Generated)
					throw std::invalid_argument(
						"Static ControlTemplate owner cannot be resolved");
				return "*" + GetVarName(*node);
			};
			auto commandTargetExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name.empty()) return "nullptr";
				if (name == blueprint.OwnerName)
					return "&__templateOwner";
				if (name == _sourceDocument.Window.Name)
					return "this";
				if (const auto* node = findBlueprintNodeByName(name);
					node && node->TemplateState.Generated)
					return GetVarName(*node);
				if (const auto* node = findSourceNodeByName(name);
					node && !node->TemplateState.Generated)
					return GetVarName(*node);
				throw std::invalid_argument(
					"Static ControlTemplate CommandTarget cannot be resolved");
			};
			auto bindingElementExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name == blueprint.OwnerName)
					return "__templateOwner";
				if (name == _sourceDocument.Window.Name)
					return "*this";
				if (const auto* node = findBlueprintNodeByName(name);
					node && node->TemplateState.Generated)
					return "*" + GetVarName(*node);
				if (const auto* node = findSourceNodeByName(name);
					node && !node->TemplateState.Generated)
					return "*" + GetVarName(*node);
				throw std::invalid_argument(
					"Static ControlTemplate ElementName cannot be resolved");
			};
			auto sourceTemplateIndex =
				[&](const DesignerModel::DesignNode& owner)
				-> std::optional<size_t>
			{
				if (owner.TemplateState.AppliedControlTemplateFromTheme
					|| owner.TemplateState.AppliedControlTemplate.empty())
					return std::nullopt;
				const auto& key =
					owner.TemplateState.AppliedControlTemplateResource;
				const auto* definition = !key.empty()
					? blueprintDocument.FindControlTemplate(
						blueprintDocument.Nodes, owner, key)
					: owner.ComponentType.Empty()
						? blueprintDocument.FindImplicitControlTemplate(
							blueprintDocument.Nodes, owner, owner.Type)
						: blueprintDocument.FindImplicitControlTemplate(
							blueprintDocument.Nodes, owner,
							owner.ComponentType);
				if (!definition) return std::nullopt;
				const auto* begin =
					blueprintDocument.ControlTemplates.data();
				const auto index =
					static_cast<size_t>(definition - begin);
				return index < templateBlueprints.size()
					? std::optional<size_t>{ index } : std::nullopt;
			};

			// A template factory only retains weak references to templates it can
			// actually install. Local style dictionaries are uncommon and may
			// resolve a Template setter through BasedOn, so retain the complete set
			// only for that explicit lexical-resource case.
			std::vector<size_t> templateCaptureIndexes;
			const bool hasLocalTemplateResources = std::any_of(
				generatedNodes.begin(), generatedNodes.end(),
				[](const auto* node)
				{
					return node && !node->LocalResources.Empty();
				});
			if (hasLocalTemplateResources)
			{
				templateCaptureIndexes.reserve(templateBlueprints.size());
				for (size_t index = 0; index < templateBlueprints.size(); ++index)
					templateCaptureIndexes.push_back(index);
			}
			else
			{
				for (const auto* node : generatedNodes)
				{
					if (!node) continue;
					const auto nested = sourceTemplateIndex(*node);
					if (nested && std::find(
						templateCaptureIndexes.begin(),
						templateCaptureIndexes.end(), *nested)
						== templateCaptureIndexes.end())
						templateCaptureIndexes.push_back(*nested);
				}
			}

			cpp << "\t" << blueprint.VariableName
				<< "->SetApplyCallback([";
			bool emittedTemplateCapture = false;
			if (!frameworkThemeProgram)
			{
				cpp << "this";
				emittedTemplateCapture = true;
			}
			for (const auto captureIndex : templateCaptureIndexes)
			{
				const auto& captured = templateBlueprints[captureIndex];
				if (emittedTemplateCapture) cpp << ", ";
				cpp << "__weak_" << captured.VariableName.substr(2);
				emittedTemplateCapture = true;
			}
			cpp << "](Control& __templateOwner, "
				"std::wstring* outError) -> bool\n";
			cpp << "\t{\n";
			cpp << "\t\tauto fail = [&](std::wstring message)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tif (outError) *outError = std::move(message);\n";
			cpp << "\t\t\treturn false;\n";
			cpp << "\t\t};\n";
			cpp << "\t\ttry\n\t\t{\n";
			cpp << "\t\t\tstd::wstring __templateThemeError;\n";

			for (const auto* node : generatedNodes)
				cpp << GenerateControlInstantiation(*node, 3);
			if (!generatedNodes.empty()) cpp << "\n";

			for (const auto* node : generatedNodes)
			{
				if (node->TemplateState.
					AppliedControlTemplateFromTheme)
				{
					if (frameworkThemeProgram)
						throw std::invalid_argument(
							"Compiled framework theme retained a dynamic "
							"framework-template dependency");
					const auto& resourceKey = node->TemplateState.
						AppliedControlTemplateResource;
					if (resourceKey.empty())
						throw std::invalid_argument(
							"Nested Theme ControlTemplate key is missing");
					cpp << "\t\t\tif (!" << frameworkThemeType << "::"
						"InstallTemplateValue(*" << GetVarName(*node)
						<< ", L\"" << EscapeWStringLiteral(resourceKey)
						<< "\", DependencyPropertyValueSource::"
						<< (node->TemplateState.
							AppliedControlTemplateFromStyle
							? "Style" : "Theme")
						<< ", &__templateThemeError))\n";
					cpp << "\t\t\t\treturn fail("
						"L\"嵌套 Generic.xaml Template 安装失败：\" "
						"+ __templateThemeError);\n";
				}
				else if (const auto nestedIndex =
					sourceTemplateIndex(*node))
				{
					const auto& nested =
						templateBlueprints[*nestedIndex];
					const auto lockName = "__nestedTemplate_"
						+ GetVarName(*node);
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto " << lockName
						<< " = __weak_"
						<< nested.VariableName.substr(2)
						<< ".lock();\n";
					cpp << "\t\t\t\tif (!" << lockName
						<< " || !cui::framework::TemplateAccess::"
						"SetTemplate(*" << GetVarName(*node)
						<< ", ControlTemplateReference(std::move("
						<< lockName << ")), "
						"DependencyPropertyValueSource::"
						<< (node->TemplateState.
							AppliedControlTemplateFromStyle
							? "Style" : "Template")
						<< "))\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"嵌套作者 ControlTemplate 安装失败。\");\n";
					cpp << "\t\t\t}\n";
				}
			}
			if (std::any_of(
				generatedNodes.begin(), generatedNodes.end(),
				[](const auto* node)
				{
					return !node->TemplateState.
						AppliedControlTemplate.empty();
				}))
				cpp << "\n";

			cpp << "\t\t\t// Establish a fresh template namescope "
				"for this application.\n";
			for (const auto* node : generatedNodes)
			{
				const auto nodeVar = GetVarName(*node);
				const auto ownerPointer =
					templateOwnerPointerExpression(
						node->TemplateState.Owner);
				const auto ownerReference =
					templateOwnerReferenceExpression(
						node->TemplateState.Owner);
				cpp << "\t\t\tcui::framework::TreeAccess::"
					"SetTemplatedParent(*" << nodeVar << ", "
					<< ownerPointer << ");\n";
				if (!node->TemplateState.PartName.empty())
				{
					cpp << "\t\t\tif (!cui::framework::TemplateAccess::"
						"RegisterTemplatePart(" << ownerReference << ", "
						<< GeneratedTemplatePartTokenExpression(
							node->TemplateState.PartName)
						<< ", " << nodeVar << "))\n";
					cpp << "\t\t\t\treturn fail("
						"L\"ControlTemplate 部件注册失败。\");\n";
				}
				if (node->TemplateContentSource == L"Content")
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto* contentOwner = "
						"dynamic_cast<ContentControl*>("
						<< ownerPointer << ");\n";
					cpp << "\t\t\t\tauto* presenter = "
						"dynamic_cast<ContentPresenter*>("
						<< nodeVar << ");\n";
					cpp << "\t\t\t\tif (!contentOwner || !presenter "
						"|| !cui::framework::TemplateAccess::"
						"RegisterContentPresenter(*contentOwner, presenter))\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate ContentPresenter 注册失败。\");\n";
					cpp << "\t\t\t}\n";
				}
				else if (node->TemplateContentSource == L"Header")
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto* presenter = "
						"dynamic_cast<ContentPresenter*>("
						<< nodeVar << ");\n";
					cpp << "\t\t\t\tbool registered = false;\n";
					cpp << "\t\t\t\tif (auto* contentOwner = "
						"dynamic_cast<HeaderedContentControl*>("
						<< ownerPointer << "))\n";
					cpp << "\t\t\t\t\tregistered = contentOwner->"
						"RegisterTemplateHeaderPresenter(presenter);\n";
					cpp << "\t\t\t\telse if (auto* itemsOwner = "
						"dynamic_cast<HeaderedItemsControl*>("
						<< ownerPointer << "))\n";
					cpp << "\t\t\t\t\tregistered = itemsOwner->"
						"RegisterTemplateHeaderPresenter(presenter);\n";
					cpp << "\t\t\t\tif (!registered)\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate HeaderPresenter 注册失败。\");\n";
					cpp << "\t\t\t}\n";
				}
				if (node->Type == UIClass::UI_ItemsPresenter)
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto* itemsOwner = "
						"dynamic_cast<ItemsControl*>("
						<< ownerPointer << ");\n";
					cpp << "\t\t\t\tauto* presenter = "
						"dynamic_cast<ItemsPresenter*>("
						<< nodeVar << ");\n";
					cpp << "\t\t\t\tif (!itemsOwner || !presenter "
						"|| !cui::framework::TemplateAccess::"
						"RegisterItemsPresenter(*itemsOwner, presenter))\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate ItemsPresenter 注册失败。\");\n";
					cpp << "\t\t\t}\n";
				}
			}

			for (const auto* node : generatedNodes)
			{
				const auto nodeVar = GetVarName(*node);
				if ((node->Type == UIClass::UI_Button
					|| node->Type == UIClass::UI_MenuItem)
					&& !node->Structure.CommandTarget.empty())
					cpp << "\t\t\t" << nodeVar
						<< "->CommandTarget = "
						<< commandTargetExpression(
							node->Structure.CommandTarget)
						<< ";\n";
				if (!node->Properties.StyleResourceKey.empty())
					cpp << "\t\t\tcui::framework::StyleAccess::"
						"SetResourceKey(*" << nodeVar << ", L\""
						<< EscapeWStringLiteral(
							node->Properties.StyleResourceKey)
						<< "\", "
						<< (frameworkThemeProgram
							|| node->TemplateState.StyleResourceScopeFromTheme
							? "true" : "false")
						<< (node->TemplateState.StyleResourceIsAutomatic
							? ", true" : "") << ");\n";
				cpp << GenerateLocalResources(
					*node, 3, &blueprintDocument,
					&weakStaticObjectResources, nullptr, true);
				cpp << GenerateAuthoredProperties(*node, 3);
				cpp << GenerateContainerProperties(*node, 3);
			}

			for (const auto* node : generatedNodes)
			{
				if (node->TemplateBindings.empty()) continue;
				const auto nodeVar = GetVarName(*node);
				const auto ownerReference =
					templateOwnerReferenceExpression(
						node->TemplateState.Owner);
				const auto* templateOwner = findBlueprintNodeByName(
					node->TemplateState.Owner);
				if (!templateOwner)
					throw std::invalid_argument(
						"Static ControlTemplate TemplateBinding owner is missing");
				for (const auto& [targetProperty, sourceProperty]
					: node->TemplateBindings)
				{
					const auto* targetMemberPath =
						CompiledMemberPathAccessor(targetProperty);
					const auto* sourceMemberPath =
						CompiledMemberPathAccessor(sourceProperty);
					if (targetMemberPath || sourceMemberPath)
					{
						if (!targetMemberPath || !sourceMemberPath)
							throw std::invalid_argument(
								"Static ControlTemplate member-path "
								"TemplateBinding must connect compiled paths");
						cpp << "\t\t\t" << nodeVar << "->SetCompiled"
							<< targetMemberPath << "(static_cast<"
							<< GetControlTypeName(templateOwner->Type)
							<< "&>(" << ownerReference << ").GetCompiled"
							<< sourceMemberPath << "());\n";
						continue;
					}
					const auto targetIdentity =
						FindGeneratedDependencyPropertyExpression(
							*node, targetProperty, true);
					const auto sourceIdentity =
						FindGeneratedDependencyPropertyExpression(
							*templateOwner, sourceProperty, false);
					if (targetIdentity.empty() || sourceIdentity.empty())
						throw std::invalid_argument(
							"Static ControlTemplate TemplateBinding has no "
							"DependencyProperty identity: "
							+ GetControlTypeName(node->Type) + "."
							+ WStringToString(targetProperty) + " <- "
							+ GetControlTypeName(templateOwner->Type) + "."
							+ WStringToString(sourceProperty));
					cpp << "\t\t\tif (!" << nodeVar
						<< "->DataBindings.AddTemplateBinding("
						<< targetIdentity << ", " << ownerReference
						<< ", " << sourceIdentity << "))\n";
					cpp << "\t\t\t\treturn fail("
						"L\"ControlTemplate TemplateBinding 安装失败。\");\n";
				}
			}

			for (const auto* node : generatedNodes)
			{
				const auto nodeVar = GetVarName(*node);
				for (const auto& binding : node->InputBindings)
				{
					std::wstring gestureError;
					if (binding.Kind
						== DesignerModel::DesignInputBindingKind::Key)
					{
						KeyGesture gesture;
						if (!TryParseKeyGesture(
							binding.Gesture, gesture, &gestureError))
							throw std::invalid_argument(
								"Static template KeyBinding is invalid");
						const auto keyExpression =
							KeyToExpr(gesture.Key);
						if (keyExpression.empty())
							throw std::invalid_argument(
								"Static template KeyBinding key is unsupported");
						cpp << "\t\t\t(void)" << nodeVar
							<< "->AddInputBinding(KeyBinding{ "
							"RoutedCommand(L\""
							<< EscapeWStringLiteral(binding.Command)
							<< "\"), KeyGesture{ " << keyExpression
							<< ", "
							<< ModifierKeysToExpr(gesture.Modifiers)
							<< " }, ";
					}
					else
					{
						MouseGesture gesture;
						if (!TryParseMouseGesture(
							binding.Gesture, gesture, &gestureError))
							throw std::invalid_argument(
								"Static template MouseBinding is invalid");
						cpp << "\t\t\t(void)" << nodeVar
							<< "->AddInputBinding(MouseBinding{ "
							"RoutedCommand(L\""
							<< EscapeWStringLiteral(binding.Command)
							<< "\"), MouseGesture{ "
							<< MouseActionToExpr(gesture.Action)
							<< ", "
							<< ModifierKeysToExpr(gesture.Modifiers)
							<< " }, ";
					}
					if (binding.CommandParameter.empty())
						cpp << "{}";
					else cpp << "std::wstring(L\""
						<< EscapeWStringLiteral(
							binding.CommandParameter)
						<< "\")";
					cpp << ", "
						<< commandTargetExpression(
							binding.CommandTarget)
						<< " });\n";
				}

				for (const auto& [eventName, storedHandler]
					: node->Events)
				{
					if (storedHandler.empty()) continue;
					const auto descriptor =
						DesignerEventCatalog::FindControlEvent(
							node->Type, eventName,
							ComponentEvents(*node));
					if (!descriptor)
						throw std::invalid_argument(
							"Static template event is unsupported");
					cpp << "\t\t\tcui::framework::TemplateAccess::"
						"RetainTemplateEventConnection(__templateOwner,\n";
					cpp << "\t\t\t\t" << nodeVar << "->"
						<< descriptor->EventField
						<< ".Subscribe(std::bind_front(&"
						<< className << "::"
						<< Utf8HandlerName(storedHandler)
						<< ", this)));\n";
				}

				for (const auto& binding : node->CommandBindings)
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tCommandBinding commandBinding;\n";
					cpp << "\t\t\t\tcommandBinding.Command = "
						"RoutedCommand(L\""
						<< EscapeWStringLiteral(binding.Command)
						<< "\");\n";
					auto emitCanExecute =
						[&](const char* field,
							const std::wstring& storedHandler)
					{
						if (storedHandler.empty()) return;
						cpp << "\t\t\t\tcommandBinding."
							<< field
							<< " = [this](Control* sender, "
							"CanExecuteRoutedEventArgs& e) { "
							<< Utf8HandlerName(storedHandler)
							<< "(sender, e); };\n";
					};
					auto emitExecuted =
						[&](const char* field,
							const std::wstring& storedHandler)
					{
						if (storedHandler.empty()) return;
						cpp << "\t\t\t\tcommandBinding."
							<< field
							<< " = [this](Control* sender, "
							"ExecutedRoutedEventArgs& e) { "
							<< Utf8HandlerName(storedHandler)
							<< "(sender, e); };\n";
					};
					emitCanExecute(
						"PreviewCanExecute",
						binding.PreviewCanExecute);
					emitCanExecute(
						"CanExecute", binding.CanExecute);
					emitExecuted(
						"PreviewExecuted",
						binding.PreviewExecuted);
					emitExecuted(
						"Executed", binding.Executed);
					cpp << "\t\t\t\tcui::framework::TemplateAccess::"
						"RetainTemplateEventConnection(__templateOwner,\n";
					cpp << "\t\t\t\t\t" << nodeVar
						<< "->AddCommandBinding(std::move("
						"commandBinding)));\n";
					cpp << "\t\t\t}\n";
				}
			}

			std::unordered_map<const DesignerModel::DesignNode*,
				std::vector<const DesignerModel::DesignNode*>>
				templateChildren;
			auto findBlueprintNodeById =
				[&](int id) -> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					blueprintDocument.Nodes.begin(),
					blueprintDocument.Nodes.end(),
					[&](const auto& node) { return node.Id == id; });
				return found == blueprintDocument.Nodes.end()
					? nullptr : &*found;
			};
			for (const auto* node : generatedNodes)
			{
				const auto* parent = node->ParentId > 0
					? findBlueprintNodeById(node->ParentId)
					: !node->ParentRef.empty()
						? findBlueprintNodeByName(node->ParentRef)
						: nullptr;
				templateChildren[parent].push_back(node);
			}
			auto sortTemplateChildren = [](auto& children)
			{
				std::stable_sort(
					children.begin(), children.end(),
					[](const auto* left, const auto* right)
					{
						const auto leftOrder = left->Order < 0
							? (std::numeric_limits<int>::max)()
							: left->Order;
						const auto rightOrder = right->Order < 0
							? (std::numeric_limits<int>::max)()
							: right->Order;
						return leftOrder < rightOrder;
					});
			};
			std::function<void(
				const DesignerModel::DesignNode*, int)>
				emitTemplateChildren;
			std::function<void(
				const DesignerModel::DesignNode&, int)>
				emitTemplateControl;
			emitTemplateChildren =
				[&](const DesignerModel::DesignNode* parent, int indent)
			{
				auto found = templateChildren.find(parent);
				if (found == templateChildren.end()) return;
				auto children = found->second;
				sortTemplateChildren(children);
				for (const auto* child : children)
					emitTemplateControl(*child, indent);
			};
			emitTemplateControl =
				[&](const DesignerModel::DesignNode& node, int indent)
			{
				const std::string indentText(indent, '\t');
				const auto nodeVar = GetVarName(node);
				const auto* parent = node.ParentId > 0
					? findBlueprintNodeById(node.ParentId)
					: !node.ParentRef.empty()
						? findBlueprintNodeByName(node.ParentRef)
						: nullptr;
				const bool parentIsTop = parent == &*blueprintOwner;
				const auto parentPointer = parentIsTop
					? std::string("&__templateOwner")
					: parent ? GetVarName(*parent) : std::string{};
				const auto parentReference = parentIsTop
					? std::string("__templateOwner")
					: parent ? "*" + GetVarName(*parent)
						: std::string{};
				const auto parentType = parentIsTop
					? blueprintOwner->Type
					: parent ? parent->Type : UIClass::UI_CUSTOM;
				if (!parent)
					throw std::invalid_argument(
						"Static ControlTemplate node has no parent");
				const bool isVisualHeader =
					node.Structure.ChildRole
					== DesignerModel::DesignNodeChildRole::Header;
				if (node.TemplateState.ControlTemplateRoot)
					cpp << indentText
						<< "cui::framework::TemplateAccess::"
						"SetTemplateRoot(" << parentReference
						<< ", std::move(__owned_" << nodeVar
						<< "));\n";
				else if (isVisualHeader
					&& (IsUIClassAssignableFrom(
							UIClass::UI_HeaderedContentControl,
							parentType)
						|| IsUIClassAssignableFrom(
							UIClass::UI_HeaderedItemsControl,
							parentType)))
					cpp << indentText << parentPointer
						<< "->SetVisualHeader(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, parentType))
					cpp << indentText << parentPointer
						<< "->AddItemControl(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, parentType))
					cpp << indentText << parentPointer
						<< "->SetVisualContent(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (parentType == UIClass::UI_Popup)
					cpp << indentText << parentPointer
						<< "->SetChild(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (IsUIClassAssignableFrom(
					UIClass::UI_Decorator, parentType))
					cpp << indentText << parentPointer
						<< "->SetChild(std::move(__owned_"
						<< nodeVar << "));\n";
				else
					cpp << indentText << parentPointer
						<< "->AddOwned(std::move(__owned_"
						<< nodeVar << "));\n";

				if (!node.TemplateState.ContentOwner.empty())
				{
					const auto logicalOwner =
						node.TemplateState.ContentOwner
							== blueprint.OwnerName
						? std::string("&__templateOwner")
						: templateOwnerPointerExpression(
							node.TemplateState.ContentOwner);
					cpp << indentText
						<< "cui::framework::TreeAccess::"
						"SetLogicalParent(*" << nodeVar << ", "
						<< logicalOwner << ");\n";
				}
				else
					cpp << indentText
						<< "cui::framework::TreeAccess::"
						"SetLogicalParent(*" << nodeVar
						<< ", nullptr);\n";
				emitTemplateChildren(&node, indent);
			};
			emitTemplateChildren(&*blueprintOwner, 3);
			for (const auto* node : generatedNodes)
			{
				if (!node->Structure.RelativePanel
					|| node->Structure.RelativePanel->Empty()) continue;
				const auto* parent = node->ParentId > 0
					? findBlueprintNodeById(node->ParentId)
					: !node->ParentRef.empty()
						? findBlueprintNodeByName(node->ParentRef)
						: nullptr;
				const auto parentExpression =
					parent == &*blueprintOwner
						? std::string("&__templateOwner")
						: parent ? GetVarName(*parent)
							: std::string(
								"static_cast<Control*>(nullptr)");
				cpp << GenerateRelativePanelConstraints(
					*node,
					parentExpression,
					templateControlExpressions,
					3,
					true);
			}

			for (const auto* node : generatedNodes)
			{
				if (node->Bindings.empty()) continue;
				const auto nodeVar = GetVarName(*node);
				auto resolveControlTemplateBindingEndpoint = [&] (
					const DesignerDataBinding& sourceBinding,
					const std::wstring& targetProperty,
					bool multiSource,
					std::string_view indent,
					const char* context)
				{
					StaticBindingSourceSpec source;
					source.AdapterExpression = multiSource
						? "static_cast<IBindingSource*>(&"
							+ nodeVar + "->DataContextSource())"
						: nodeVar + "->DataContextSource()";
					source.SourceSchema =
						&canonicalDataDocument.DataContextSchema;
					if (!sourceBinding.ElementName.empty())
					{
						const auto elementReference =
							bindingElementExpression(sourceBinding.ElementName);
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(&(" + elementReference
								+ "))"
							: elementReference;
						source.DirectObjectExpression = elementReference;
						if (sourceBinding.ElementName == blueprint.OwnerName)
							source.DirectNode = &*blueprintOwner;
						else if (sourceBinding.ElementName
							== _sourceDocument.Window.Name)
							source.DirectWindow = true;
						else if (const auto* sourceNode =
							findBlueprintNodeByName(sourceBinding.ElementName);
							sourceNode && sourceNode->TemplateState.Generated)
							source.DirectNode = sourceNode;
						else if (const auto* sourceNode =
							findSourceNodeByName(sourceBinding.ElementName);
							sourceNode && !sourceNode->TemplateState.Generated)
							source.DirectNode = sourceNode;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::Self)
					{
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(" + nodeVar + ")"
							: "*" + nodeVar;
						source.DirectObjectExpression = "*" + nodeVar;
						source.DirectNode = node;
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::TemplatedParent)
					{
						const auto ownerReference =
							templateOwnerReferenceExpression(
								node->TemplateState.Owner);
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(&(" + ownerReference
								+ "))"
							: ownerReference;
						source.DirectObjectExpression = ownerReference;
						source.DirectNode = findBlueprintNodeByName(
							node->TemplateState.Owner);
						if (!source.DirectNode)
							throw std::invalid_argument(
								"Static ControlTemplate owner is unresolved");
						source.SourceSchema = nullptr;
					}
					else if (sourceBinding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						source.AdapterExpression =
							"cui::binding::CreateFindAncestorSource(*"
							+ nodeVar + ", "
							+ GeneratedFindAncestorTypeExpression(
								_sourceDocument, sourceBinding) + ", "
							+ std::to_string(sourceBinding.AncestorLevel) + ")";
						source.SourceSchema = nullptr;
					}
					else if (targetProperty == L"DataContext")
					{
						if (multiSource)
							source.AdapterExpression =
								"static_cast<IBindingSource*>(&"
								+ nodeVar + "->GetInheritanceParent()->"
									"DataContextSource())";
						else
						{
							source.Guard = nodeVar
								+ "->GetInheritanceParent() && ";
							source.AdapterExpression = nodeVar
								+ "->GetInheritanceParent()->DataContextSource()";
						}
					}
					return lowerBindingSourceEndpoint(
						sourceBinding, source, indent, context);
				};
				for (const auto& [targetProperty, binding]
					: node->Bindings)
				{
					const auto targetIdentity =
						FindGeneratedDependencyPropertyExpression(
							*node, targetProperty, true);
					if (targetIdentity.empty())
						throw std::invalid_argument(
							"Static ControlTemplate Binding target has no "
							"writable DependencyProperty identity");
					cpp << "\t\t\t{\n";
					if (binding.IsMultiBinding())
					{
						auto resolveChild = [&] (
							const DesignerDataBinding& child,
							std::string_view indent,
							const char* context)
						{
							return resolveControlTemplateBindingEndpoint(
								child, targetProperty, true, indent, context);
						};
						emitStaticMultiBinding(
							binding, nodeVar, targetIdentity,
							resolveChild, "\t\t\t\t",
							"Static ControlTemplate MultiBinding", "attached");
					}
					else
					{
						const auto options = lowerBindingOptions(binding);
						const auto endpoint = resolveControlTemplateBindingEndpoint(
							binding, targetProperty, false, "\t\t\t\t",
							"Static ControlTemplate Binding");
					if (!options.ConverterName.empty())
					{
						cpp << "\t\t\t\tauto converter = "
							<< bindingConverterExpression(
								options.ConverterName, binding)
							<< ";\n";
						cpp << "\t\t\t\tif (!converter)\n";
						cpp << "\t\t\t\t\treturn fail("
							"L\"ControlTemplate Binding Converter "
							"不存在。\");\n";
					}
					if (endpoint.UsesDirectSource())
						cpp << "\t\t\t\t"
							<< (endpoint.DirectCompiledRecord
								? "// CUI:AOT binding-source=direct-record\n"
								: "// CUI:AOT binding-source=direct-dp\n");
					cpp << "\t\t\t\tconst bool attached = "
						<< endpoint.Guard << nodeVar
						<< "->DataBindings.Add(" << targetIdentity
						<< ", " << endpoint.SourceOperand << ", ";
					if (!endpoint.UsesDirectSource())
						cpp << endpoint.SourcePathOperand << ", ";
					cpp
						<< BindingModeToExpr(binding.Mode)
						<< ", "
						<< DataSourceUpdateModeToExpr(
							binding.UpdateMode);
					if (!options.ConverterName.empty())
						cpp << ", std::move(converter)";
					else if (options.HasExtendedOptions)
						cpp << ", {}";
					if (options.HasExtendedOptions)
						cpp << ", " << options.Fallback
							<< ", " << options.TargetNull
							<< ", " << options.ConverterParameter
							<< ", " << options.StringFormat;
					cpp << ") != nullptr;\n";
					}
					cpp << "\t\t\t\tif (!attached)\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate Binding 安装失败。\");\n";
					cpp << "\t\t\t}\n";
				}
			}

			auto findInteractionTarget =
				[&](const std::wstring& targetName)
					-> const DesignerModel::DesignNode*
				{
					const auto target = std::find_if(
						generatedNodes.begin(), generatedNodes.end(),
						[&](const auto* node)
						{
							return node
								&& node->TemplateState.Owner == blueprint.OwnerName
								&& (node->Name == targetName
									|| node->TemplateState.PartName == targetName);
						});
					return target == generatedNodes.end() ? nullptr : *target;
				};
			const DeclarativePropertyResolver templatePropertyResolver =
				[&](const std::wstring& targetName,
					const std::wstring& propertyName,
					bool requireWritable) -> std::string
				{
					if (targetName.empty()
						|| targetName == blueprint.OwnerName)
						return FindKnownDependencyPropertyExpression(
							definition.TargetType, propertyName,
							requireWritable);
					const auto* target = findInteractionTarget(targetName);
					return target
						? FindGeneratedDependencyPropertyExpression(
							*target, propertyName, requireWritable)
						: std::string{};
				};
			const DeclarativeTargetResolver templateTargetResolver =
				[&](const std::wstring& targetName)
				{
					if (targetName == blueprint.OwnerName)
						return std::string("nullptr");
					const auto* target = findInteractionTarget(targetName);
					return target ? GetVarName(*target) : std::string{};
				};
			const DeclarativeEventResolver templateEventResolver =
				[&](const std::wstring& eventName)
				{
					const std::wstring localName(RoutedEventLocalName(eventName));
					if (!DesignerEventCatalog::FindControlEvent(
						definition.TargetType, localName))
						return DeclarativeEventReferenceExpression{};
					const auto eventId = FindRoutedEventId(localName);
					return eventId
						? DeclarativeEventReferenceExpression{
							RoutedEventIdExpression(*eventId), true }
						: DeclarativeEventReferenceExpression{};
				};
			cpp << GenerateDeclarativeInteractionsCode(
				blueprint.VisualStateGroups,
				blueprint.EventTriggers,
				templatePropertyResolver, templateTargetResolver,
				templateEventResolver, "__templateOwner", 3);

			for (const auto* node : generatedNodes)
			{
				if (!node->TemplateState.
					AppliedControlTemplateFromTheme) continue;
				if (frameworkThemeProgram)
					throw std::invalid_argument(
						"Compiled framework theme retained dynamic "
						"framework VisualState data");
				const auto& resourceKey = node->TemplateState.
					AppliedControlTemplateResource;
				cpp << "\t\t\tif (!" << frameworkThemeType << "::"
					"ApplyTemplateVisualStates(*"
					<< GetVarName(*node) << ", L\""
					<< EscapeWStringLiteral(resourceKey)
					<< "\", &__templateThemeError))\n";
				cpp << "\t\t\t\treturn fail("
					"L\"嵌套 Generic.xaml VisualState 安装失败：\" "
					"+ __templateThemeError);\n";
			}
			for (const auto* node : generatedNodes)
				if (!node->TemplateState.
					AppliedControlTemplate.empty())
					cpp << "\t\t\tcui::framework::TemplateAccess::"
						"CompleteTemplateApplication(*"
						<< GetVarName(*node) << ");\n";

			cpp << "\t\t\tif (!cui::framework::TemplateAccess::"
				"GetTemplateRoot(__templateOwner))\n";
			cpp << "\t\t\t\treturn fail("
				"L\"ControlTemplate 未生成唯一视觉根。\");\n";
			cpp << "\t\t\tif (outError) outError->clear();\n";
			cpp << "\t\t\treturn true;\n";
			cpp << "\t\t}\n";
			cpp << "\t\tcatch (const std::exception&)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\treturn fail("
				"L\"ControlTemplate 静态构造发生运行时异常。\");\n";
			cpp << "\t\t}\n";
			cpp << "\t\tcatch (...)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\treturn fail("
				"L\"ControlTemplate 静态构造发生未知异常。\");\n";
			cpp << "\t\t}\n";
			cpp << "\t});\n\n";
		}
	}

	if (!itemContainerProjections.empty())
	{
		cpp << "\t// Install the native item-container Style identity and its "
			"compiled ControlTemplate before ItemsSource can realize "
			"containers.\n";
		for (const auto& projection : itemContainerProjections)
		{
			const auto& owner = *projection.Owner;
			const auto control = GetVarName(owner);
			cpp << "\t" << control << "->SetItemContainerStyle(L\""
				<< EscapeWStringLiteral(
					owner.Structure.ItemContainerStyle) << "\");\n";
			cpp << "\tif (" << control
				<< "->GetItemContainerStyle() != L\""
				<< EscapeWStringLiteral(
					owner.Structure.ItemContainerStyle) << "\")\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated ItemContainerStyle installation failed\");\n";
			if (projection.TemplateVariableName.empty()) continue;
			cpp << "\t" << control
				<< "->SetItemContainerTemplate(ControlTemplateReference("
				<< projection.TemplateVariableName << "));\n";
			cpp << "\tif (" << control
				<< "->GetItemContainerTemplate().Get() != "
				<< projection.TemplateVariableName << ".get())\n";
			cpp << "\t\tthrow std::runtime_error("
				"\"Generated ItemContainerTemplate installation failed\");\n";
		}
		cpp << "\n";
	}

	bool emittedStaticItemsSources = false;
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated
			|| node.Structure.ItemsSourceResource.empty()) continue;
		const auto dataListSource = staticDataListVariables.find(
			node.Structure.ItemsSourceResource);
		const auto viewSource = staticCollectionViewVariables.find(
			node.Structure.ItemsSourceResource);
		const auto* sourceVariable =
			dataListSource != staticDataListVariables.end()
				? &dataListSource->second
				: viewSource != staticCollectionViewVariables.end()
					? &viewSource->second : nullptr;
		if (!sourceVariable)
			throw std::invalid_argument(
				"Static ItemsSource resource has no native list lowering");
		if (!IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, node.Type))
			throw std::invalid_argument(
				"Static ItemsSource target is not an ItemsControl");
		if (!emittedStaticItemsSources)
		{
			cpp << "\t// Strongly typed ItemsSource resource wiring happens "
				"after item-container templates and before selection/local "
				"properties are applied.\n";
			emittedStaticItemsSources = true;
		}
		const auto control = GetVarName(node);
		cpp << "\t" << control
			<< "->SetItemsSource(BindingListReference("
			<< *sourceVariable << "));\n";
		cpp << "\tif (" << control
			<< "->GetItemsSource().Get() != "
			<< *sourceVariable << ".get())\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated ItemsSource installation failed for "
			<< control << ": \" + Convert::WStringToString("
			<< control << "->LastTemplateError()));\n";
	}
	if (emittedStaticItemsSources) cpp << "\n";

	// 2) Apply scalar/structured state after the complete namescope exists.
		for (size_t nodeIndex = 0;
			nodeIndex < _sourceDocument.Nodes.size(); ++nodeIndex)
		{
			const auto& node = _sourceDocument.Nodes[nodeIndex];
			if (node.TemplateState.Generated) continue;
			cpp << GenerateControlCommonProperties(node, 1);
			const auto& canonicalNode =
				canonicalDataDocument.Nodes[nodeIndex];
			if (!node.LocalResources.Empty()
				|| !canonicalNode.LocalObjectResources.
					ItemsPanelTemplates.empty())
			{
				auto [visibleObjectResources, ownedObjectResources] =
					localItemsPanelObjectResources(canonicalNode);
				cpp << GenerateLocalResources(
					canonicalNode, 1, &canonicalDataDocument,
					&visibleObjectResources,
					&ownedObjectResources);
			}
			const auto omitted = compiledMemberPathProperties.find(&node);
			cpp << GenerateAuthoredProperties(
				node, 1, &sharedDocumentResources,
				&sharedDocumentResourceAssignments,
				omitted == compiledMemberPathProperties.end()
					? nullptr : &omitted->second);
		cpp << GenerateContainerProperties(node, 1);
		cpp << "\n";
	}

	auto emitInputBindings = [&](const auto& bindings,
		const std::string& target)
	{
		for (const auto& binding : bindings)
		{
			std::wstring gestureError;
			if (binding.Kind == DesignerModel::DesignInputBindingKind::Key)
			{
				KeyGesture gesture;
				if (!TryParseKeyGesture(binding.Gesture, gesture, &gestureError))
					throw std::invalid_argument(
						"Code generation encountered an invalid KeyBinding");
				const auto keyExpression = KeyToExpr(gesture.Key);
				if (keyExpression.empty())
					throw std::invalid_argument(
						"Code generation encountered an unsupported Key identity");
				cpp << "\t(void)" << target << "->AddInputBinding(KeyBinding{ "
					<< "RoutedCommand(L\"" << EscapeWStringLiteral(binding.Command)
					<< "\"), KeyGesture{ " << keyExpression << ", "
					<< ModifierKeysToExpr(gesture.Modifiers) << " }, ";
			}
			else
			{
				MouseGesture gesture;
				if (!TryParseMouseGesture(binding.Gesture, gesture, &gestureError))
					throw std::invalid_argument(
						"Code generation encountered an invalid MouseBinding");
				cpp << "\t(void)" << target << "->AddInputBinding(MouseBinding{ "
					<< "RoutedCommand(L\"" << EscapeWStringLiteral(binding.Command)
					<< "\"), MouseGesture{ " << MouseActionToExpr(gesture.Action)
					<< ", " << ModifierKeysToExpr(gesture.Modifiers) << " }, ";
			}
			if (binding.CommandParameter.empty()) cpp << "{}";
			else cpp << "std::wstring(L\""
				<< EscapeWStringLiteral(binding.CommandParameter) << "\")";
			cpp << ", " << CommandTargetExpression(binding.CommandTarget)
				<< " });\n";
		}
	};
	if (!_sourceDocument.Window.InputBindings.empty())
	{
		cpp << "\t// XAML InputBindings\n";
		emitInputBindings(_sourceDocument.Window.InputBindings, "this");
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		if (node.InputBindings.empty()) continue;
		emitInputBindings(node.InputBindings, GetVarName(node));
	}
	if (!_sourceDocument.Window.InputBindings.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.TemplateState.Generated
					&& !node.InputBindings.empty();
			})) cpp << "\n";

	// Event subscriptions are owned by RAII connections and disconnect before Window teardown.
	{
		std::unordered_map<std::string, std::type_index> sigOf;
		std::vector<GeneratedEventBinding> binds;
		binds.reserve(_sourceDocument.Nodes.size());
		// Window 事件
		for (const auto& kv : _sourceDocument.Window.Events)
		{
			if (kv.first.empty()) continue;
			if (kv.second.empty()) continue;
			const auto descriptor = DesignerEventCatalog::FindWindowEvent(kv.first);
			if (!descriptor) continue;
			std::string handlerName = Utf8HandlerName(kv.second);
			auto itSig = sigOf.find(handlerName);
			if (itSig != sigOf.end()
				&& itSig->second != descriptor->Signature) continue;
			if (itSig == sigOf.end())
				sigOf.emplace(handlerName, descriptor->Signature);
			binds.push_back(GeneratedEventBinding{ "this",
				descriptor->EventField, handlerName, descriptor->ParameterList });
		}

		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated) continue;
			std::string ctrlVar = GetVarName(node);
			for (const auto& kv : node.Events)
			{
				const auto& evNameW = kv.first;
				if (kv.second.empty()) continue;
				const auto descriptor = FindNodeEventDescriptor(
					node, evNameW);
				if (!descriptor) continue;
				std::string handlerName = Utf8HandlerName(kv.second);
				auto itSig = sigOf.find(handlerName);
				if (itSig != sigOf.end()
					&& itSig->second != descriptor->Signature) continue;
				if (itSig == sigOf.end())
					sigOf.emplace(handlerName, descriptor->Signature);
				GeneratedEventBinding generated;
				generated.ControlVar = ctrlVar;
				generated.EventField = descriptor->EventField;
				generated.HandlerName = std::move(handlerName);
				generated.ParamList = descriptor->ParameterList;

				DesignerComponentType attachedOwner;
				std::wstring attachedEvent;
				if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
					evNameW, attachedOwner, attachedEvent))
				{
					const auto* owner =
						_sourceDocument.FindComponent(attachedOwner);
					if (!owner) continue;
					generated.BindingKind =
						GeneratedEventBinding::Kind::AttachedComponent;
					generated.EventName = std::move(attachedEvent);
					const auto leaf = GetComponentClassName(*owner);
					generated.ComponentClass =
						identity.NamespaceName.empty()
						? leaf
						: identity.NamespaceName + "::" + leaf;
				}
				else if (!node.ComponentType.Empty())
				{
					const auto* owner =
						_sourceDocument.FindComponent(node.ComponentType);
					const auto contract = owner
						? std::find_if(
							owner->Events.begin(), owner->Events.end(),
							[&](const auto& candidate)
							{ return candidate.Name == evNameW; })
						: std::vector<
							DesignerComponentEventDescriptor>::
								const_iterator{};
					if (owner && contract != owner->Events.end())
					{
						generated.BindingKind =
							GeneratedEventBinding::Kind::Component;
						generated.EventName = evNameW;
						const auto leaf = GetComponentClassName(*owner);
						generated.ComponentClass =
							identity.NamespaceName.empty()
							? leaf
							: identity.NamespaceName + "::" + leaf;
					}
				}
				binds.push_back(std::move(generated));
			}
		}

		if (!binds.empty())
		{
			cpp << "\t// 绑定事件\n";
			for (const auto& b : binds)
			{
				cpp << "\t_generatedEventConnections.emplace_back(\n";
				if (b.BindingKind
					== GeneratedEventBinding::Kind::Component)
				{
					cpp << "\t\t" << b.ControlVar << "->Subscribe"
						<< SanitizeCppIdentifier(
							WStringToString(b.EventName))
						<< "(std::bind_front(&" << className << "::"
						<< b.HandlerName << ", this)));\n";
				}
				else if (b.BindingKind
					== GeneratedEventBinding::Kind::AttachedComponent)
				{
					cpp << "\t\t" << b.ControlVar
						<< "->OnDeclarativeEvent.Subscribe(\n";
					cpp << "\t\t\t[this](Control* sender, "
						"DeclarativeEventArgs& e)\n";
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tif (e.Handled || e.Definition != &"
						<< b.ComponentClass << "::"
						<< SanitizeCppIdentifier(
							WStringToString(b.EventName))
						<< "Event()) return;\n";
					cpp << "\t\t\t\t" << b.HandlerName
						<< "(sender, e);\n";
					cpp << "\t\t\t}));\n";
				}
				else
				{
					cpp << "\t\t" << b.ControlVar << "->"
						<< b.EventField
						<< ".Subscribe(std::bind_front(&"
						<< className << "::" << b.HandlerName
						<< ", this)));\n";
				}
			}
			cpp << "\n";
		}
	}

	// CommandBinding is a first-class command collection, not four unrelated
	// routed-event subscriptions. Keeping this grouping is required for class
	// bindings, command-source requery, and atomic replacement to share the same
	// command identity and lifetime.
	auto emitCommandBindings = [&](const auto& bindings,
		const std::string& target)
	{
		for (const auto& binding : bindings)
		{
			cpp << "\t{\n";
			cpp << "\t\tCommandBinding __commandBinding;\n";
			cpp << "\t\t__commandBinding.Command = RoutedCommand(L\""
				<< EscapeWStringLiteral(binding.Command) << "\");\n";
			auto emitCanExecute = [&](const char* field,
				const std::wstring& storedHandler)
			{
				if (storedHandler.empty()) return;
				cpp << "\t\t__commandBinding." << field
					<< " = [this](Control* sender, CanExecuteRoutedEventArgs& e) { "
					<< Utf8HandlerName(storedHandler) << "(sender, e); };\n";
			};
			auto emitExecuted = [&](const char* field,
				const std::wstring& storedHandler)
			{
				if (storedHandler.empty()) return;
				cpp << "\t\t__commandBinding." << field
					<< " = [this](Control* sender, ExecutedRoutedEventArgs& e) { "
					<< Utf8HandlerName(storedHandler) << "(sender, e); };\n";
			};
			emitCanExecute("PreviewCanExecute", binding.PreviewCanExecute);
			emitCanExecute("CanExecute", binding.CanExecute);
			emitExecuted("PreviewExecuted", binding.PreviewExecuted);
			emitExecuted("Executed", binding.Executed);
			cpp << "\t\t_generatedEventConnections.emplace_back("
				<< target
				<< "->AddCommandBinding(std::move(__commandBinding)));\n";
			cpp << "\t}\n";
		}
	};
	if (!_sourceDocument.Window.CommandBindings.empty())
	{
		cpp << "\t// XAML CommandBindings\n";
		emitCommandBindings(_sourceDocument.Window.CommandBindings, "this");
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		if (node.CommandBindings.empty()) continue;
		emitCommandBindings(node.CommandBindings, GetVarName(node));
	}
	if (!_sourceDocument.Window.CommandBindings.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.TemplateState.Generated
					&& !node.CommandBindings.empty();
			})) cpp << "\n";

	// 2) Assemble the logical authored hierarchy. ItemsControl children enter
	//    Items; ordinary container children enter the visual collection.
	cpp << "\t// 组装控件层级（包含布局容器）\n";

	std::unordered_map<const DesignerModel::DesignNode*,
		std::vector<const DesignerModel::DesignNode*>> childrenOf;
	childrenOf.reserve(_sourceDocument.Nodes.size());
	auto findNodeById = [&](int id) -> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& node) { return node.Id == id; });
		return found == _sourceDocument.Nodes.end() ? nullptr : &*found;
	};
	auto findNodeByName = [&](const std::wstring& name)
		-> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& node) { return node.Name == name; });
		return found == _sourceDocument.Nodes.end() ? nullptr : &*found;
	};
	std::unordered_map<std::wstring, std::string>
		authoredControlExpressions;
	authoredControlExpressions.emplace(
		_sourceDocument.Window.Name, "this");
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		authoredControlExpressions.emplace(node.Name, GetVarName(node));
		const auto* parent = node.ParentId > 0
			? findNodeById(node.ParentId)
			: !node.ParentRef.empty()
				? findNodeByName(node.ParentRef) : nullptr;
		childrenOf[parent].push_back(&node);
	}

	auto sortAuthoredChildren = [&](auto& list)
	{
		std::stable_sort(list.begin(), list.end(), [](const auto& left,
			const auto& right)
			{
				const auto leftOrder = left->Order < 0
					? (std::numeric_limits<int>::max)() : left->Order;
				const auto rightOrder = right->Order < 0
					? (std::numeric_limits<int>::max)() : right->Order;
				return leftOrder < rightOrder;
			});
	};

	std::function<void(const DesignerModel::DesignNode*,
		const std::string&, int)> emitChildren;
	std::function<void(const DesignerModel::DesignNode&,
		const std::string&, int)> emitControl;

	emitChildren = [&](const DesignerModel::DesignNode* parent,
		const std::string& parentExpr, int indent)
	{
		auto it = childrenOf.find(parent);
		if (it == childrenOf.end()) return;
		auto list = it->second;
		sortAuthoredChildren(list);
		for (const auto* child : list)
			emitControl(*child, parentExpr, indent);
	};

	emitControl = [&](const DesignerModel::DesignNode& node,
		const std::string& parentExpr, int indent)
	{
		std::string childVar = GetVarName(node);
		std::string indentStr(indent, '\t');

		// 添加到父容器
		UIClass parentType = UIClass::UI_CUSTOM;
		const auto* parent = node.ParentId > 0
			? findNodeById(node.ParentId)
			: !node.ParentRef.empty()
				? findNodeByName(node.ParentRef) : nullptr;
		if (parent) parentType = parent->Type;
		const bool isVisualHeader = node.Structure.ChildRole
			== DesignerModel::DesignNodeChildRole::Header;
		if (!parent)
		{
			cpp << indentStr << parentExpr
				<< "->SetVisualContent(std::move(__owned_" << childVar << "));\n";
		}
		else if (node.TemplateState.ControlTemplateRoot)
		{
			cpp << indentStr
				<< "cui::framework::TemplateAccess::SetTemplateRoot(*"
				<< parentExpr << ", std::move(__owned_" << childVar << "));\n";
		}
		else if (!parent->ComponentType.Empty())
		{
			const auto* component =
				_sourceDocument.FindComponent(parent->ComponentType);
			if (!component)
				throw std::invalid_argument(
					"Generated component content owner is missing");
			auto content = std::find_if(
				component->ContentProperties.begin(),
				component->ContentProperties.end(),
				[&](const auto& candidate)
				{
					return candidate.Name
						== node.ComponentContentProperty;
				});
			if (content == component->ContentProperties.end()
				&& node.ComponentContentProperty.empty())
				content = std::find_if(
					component->ContentProperties.begin(),
					component->ContentProperties.end(),
					[](const auto& candidate)
					{ return candidate.IsDefault; });
			if (content == component->ContentProperties.end())
				throw std::invalid_argument(
					"Generated component content property is missing");
			const auto contentName = SanitizeCppIdentifier(
				WStringToString(content->Name));
			cpp << indentStr << "if (!" << parentExpr << "->"
				<< (content->Cardinality
					== DesignerComponentContentCardinality::Single
					? "Set" : "Add")
				<< contentName << "(std::move(__owned_" << childVar
				<< ")))\n";
			cpp << indentStr << "\tthrow std::runtime_error("
				"\"Generated component content attachment failed\");\n";
		}
		else if (isVisualHeader
			&& (IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, parentType)
				|| IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, parentType)))
		{
			cpp << indentStr << parentExpr
				<< "->SetVisualHeader(std::move(__owned_" << childVar << "));\n";
		}
		else if (IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, parentType))
		{
			cpp << indentStr << parentExpr
				<< "->AddItemControl(std::move(__owned_" << childVar << "));\n";
		}
		else if (IsUIClassAssignableFrom(
			UIClass::UI_ContentControl, parentType))
		{
			cpp << indentStr << parentExpr
				<< "->SetVisualContent(std::move(__owned_" << childVar << "));\n";
		}
		else if (parentType == UIClass::UI_Popup)
		{
			cpp << indentStr << parentExpr
				<< "->SetChild(std::move(__owned_" << childVar << "));\n";
		}
		else if (IsUIClassAssignableFrom(
			UIClass::UI_Decorator, parentType))
		{
			cpp << indentStr << parentExpr
				<< "->SetChild(std::move(__owned_" << childVar << "));\n";
		}
		else
		{
			cpp << indentStr << parentExpr << "->AddOwned(std::move(__owned_" << childVar << "));\n";
		}

		if (node.TemplateState.Generated
			&& node.TemplateState.ControlTemplateRoot)
			cpp << indentStr
				<< "cui::framework::TreeAccess::SetLogicalParent(*"
				<< childVar << ", nullptr);\n";
		if (!node.TemplateState.ContentOwner.empty())
		{
			const auto* logicalOwner = findSourceNodeByName(
				node.TemplateState.ContentOwner);
			if (!logicalOwner)
				throw std::invalid_argument(
					"Projected template content has no logical owner");
			cpp << indentStr
				<< "cui::framework::TreeAccess::SetLogicalParent(*"
				<< childVar << ", " << GetVarName(*logicalOwner) << ");\n";
		}

		emitChildren(&node, childVar, indent);

		cpp << "\n";
	};

	// 根级控件由文档 ParentId/ParentRef 唯一决定。
	auto rootsIt = childrenOf.find(nullptr);
	if (rootsIt != childrenOf.end())
	{
		auto roots = rootsIt->second;
		sortAuthoredChildren(roots);
		for (const auto* root : roots)
			emitControl(*root, "this", 1);
	}

	// RelativePanel references are namescope pointers, not attached dependency
	// properties. Apply them only after the complete visual tree exists so
	// forward sibling references have the same validity checks as materialized
	// XAML without retaining a runtime name/property lookup.
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated
			|| !node.Structure.RelativePanel
			|| node.Structure.RelativePanel->Empty()) continue;
		const auto* parent = node.ParentId > 0
			? findNodeById(node.ParentId)
			: !node.ParentRef.empty()
				? findNodeByName(node.ParentRef) : nullptr;
		cpp << GenerateRelativePanelConstraints(
			node,
			parent ? GetVarName(*parent) : "this",
			authoredControlExpressions,
			1,
			false);
	}
	if (std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node)
		{
			return !node.TemplateState.Generated
				&& node.Structure.RelativePanel
				&& !node.Structure.RelativePanel->Empty();
		})) cpp << "\n";

	const bool hasDocumentStyleSheet = frameworkThemeProgram
		|| !_styleSheet.Empty() || !staticObjectResources.empty();
	if (!frameworkThemeProgram)
	{
		cpp << "\tauto __frameworkThemeStyles = " << frameworkThemeType
			<< "::DefaultStyleSheet(&__frameworkThemeError);\n";
		cpp << "\tif (!__frameworkThemeStyles)\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated Generic.xaml theme construction failed\");\n";
	}

	if (!_sourceDocument.Window.Properties.StyleResourceKey.empty())
		cpp << "\tcui::framework::StyleAccess::SetResourceKey(*this, L\""
			<< EscapeWStringLiteral(
				_sourceDocument.Window.Properties.StyleResourceKey)
			<< "\", "
			<< (frameworkThemeProgram
				|| _sourceDocument.Window.TemplateState.StyleResourceScopeFromTheme
				? "true" : "false")
			<< (_sourceDocument.Window.TemplateState.StyleResourceIsAutomatic
				? ", true" : "") << ");\n";
	cpp << GenerateStyleSheetCode(
		1, staticObjectResources,
		&sharedDocumentResources, frameworkThemeProgram,
		&canonicalDataDocument.StyleSheet, &canonicalDataDocument,
		"__styleSheet", &documentItemsPanelAliases);
	if (!frameworkThemeProgram)
	{
		cpp << "\tif (!cui::framework::StyleAccess::SetEnvironment("
			"*this, std::move(__frameworkThemeStyles), ";
		cpp << (hasDocumentStyleSheet
			? "std::move(__styleSheet)" : "nullptr");
		cpp << ", true))\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated Theme/Document style environment installation failed\");\n\n";
	}

	auto findAuthoredTemplateIndex =
		[&](const DesignerModel::DesignNode& owner)
		-> std::optional<size_t>
	{
		if (owner.TemplateState.AppliedControlTemplateFromTheme)
			return std::nullopt;
		const auto key = !owner.Structure.ControlTemplate.empty()
			? owner.Structure.ControlTemplate
			: owner.TemplateState.AppliedControlTemplateResource;
		const DesignerModel::DesignControlTemplate* definition = nullptr;
		if (!key.empty())
			definition = _sourceDocument.FindControlTemplate(
				_sourceDocument.Nodes, owner, key);
		else if (!owner.TemplateState.AppliedControlTemplate.empty())
			definition = owner.ComponentType.Empty()
				? _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.Type)
				: _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.ComponentType);
		else if (_sourceDocument.Nodes.size()
			== std::count_if(
				_sourceDocument.Nodes.begin(),
				_sourceDocument.Nodes.end(),
				[](const auto& node)
				{ return !node.TemplateState.Generated; }))
			definition = owner.ComponentType.Empty()
				? _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.Type)
				: _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.ComponentType);
		if (!definition) return std::nullopt;
		const auto index = static_cast<size_t>(
			definition - _sourceDocument.ControlTemplates.data());
		return index < templateBlueprints.size()
			? std::optional<size_t>{ index } : std::nullopt;
	};
	for (const auto& owner : _sourceDocument.Nodes)
	{
		if (owner.TemplateState.Generated) continue;
		const auto templateIndex =
			findAuthoredTemplateIndex(owner);
		if (!templateIndex) continue;
		const auto& blueprint = templateBlueprints[*templateIndex];
		const auto source = !owner.Structure.ControlTemplate.empty()
			? "DependencyPropertyValueSource::Local"
			: "DependencyPropertyValueSource::Style";
		cpp << "\tif (!cui::framework::TemplateAccess::SetTemplate(*"
			<< GetVarName(owner)
			<< ", ControlTemplateReference("
			<< blueprint.VariableName << "), " << source << "))\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated authored Control.Template installation failed\");\n";
	}
	if (!templateBlueprints.empty()) cpp << "\n";

	if (!_sourceDocument.Window.Properties.Values.empty())
		cpp << "\t// XAML Window Local 属性/资源表达式\n";
	for (const auto& [propertyName, assignment]
		: _sourceDocument.Window.Properties.Values)
	{
		if (!assignment.DynamicResourceKey.empty())
		{
			if (dynamicWindow)
				cpp << "\t(void)this->SetDynamicResource(L\""
					<< EscapeWStringLiteral(propertyName) << "\", L\""
					<< EscapeWStringLiteral(
						assignment.DynamicResourceKey) << "\");\n";
			else
			{
				const auto property =
					FindKnownDependencyPropertyExpression(
						UIClass::UI_Window, propertyName, true);
				if (property.empty())
					throw std::invalid_argument(
						"Static Window DynamicResource property has no C++ "
						"dependency-property identity: "
						+ WStringToString(propertyName));
				cpp << "\t(void)cui::framework::DependencyPropertyAccess::"
					<< "SetDynamicResource(*this, " << property << ", L\""
					<< EscapeWStringLiteral(
						assignment.DynamicResourceKey)
					<< "\", DependencyPropertyValueSource::Local);\n";
			}
		}
		else
		{
			const bool usesSharedDocumentResource =
				sharedDocumentResourceAssignments.contains(&assignment);
			const auto shared = usesSharedDocumentResource
				? sharedDocumentResources.find(assignment.ResourceKey)
				: sharedDocumentResources.end();
			const auto valueExpression =
				shared != sharedDocumentResources.end()
					? shared->second
					: GenerateStyleValueExpression(assignment.Value);
			const auto typed = dynamicWindow
				? std::optional<TypedPropertyInfo>{}
				: FindKnownProperty(UIClass::UI_Window, propertyName);
			const auto typedCall = typed
				? GenerateTypedPropertyCall(
					"this", *typed, valueExpression,
					usesSharedDocumentResource)
				: std::string{};
			if (!typedCall.empty())
				cpp << "\t" << typedCall << ";\n";
			else if (dynamicWindow)
				cpp << "\t(void)this->TrySetPropertyValue(L\""
					<< EscapeWStringLiteral(propertyName) << "\", "
					<< valueExpression << ");\n";
			else
			{
				const auto property =
					FindKnownDependencyPropertyExpression(
						UIClass::UI_Window, propertyName, true);
				if (property.empty())
					throw std::invalid_argument(
						"Static Window property has no writable "
						"DependencyProperty identity: "
						+ WStringToString(propertyName));
				cpp << "\t(void)cui::framework::DependencyPropertyAccess::"
					<< "SetValue(*this, " << property << ", "
					<< valueExpression
					<< ", DependencyPropertyValueSource::Local);\n";
			}
		}
	}
	if (!_sourceDocument.Window.Properties.Values.empty()) cpp << "\n";

	if (dynamicWindow)
	{
		for (const auto& owner : _sourceDocument.Nodes)
		{
			if (owner.TemplateState.Generated) continue;
			cpp << "\tif (" << GetVarName(owner)
				<< "->GetTemplate())\n";
			cpp << "\t{\n";
			cpp << "\t\t(void)" << GetVarName(owner)
				<< "->ApplyTemplate();\n";
			cpp << "\t\tif (!cui::framework::TemplateAccess::"
				"GetTemplateRoot(*" << GetVarName(owner)
				<< ") || !" << GetVarName(owner)
				<< "->LastTemplateError().empty())\n";
			cpp << "\t\t\tthrow std::runtime_error("
				"\"Generated ControlTemplate application failed\");\n";
			cpp << "\t}\n";
		}
		if (std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{ return !node.TemplateState.Generated; }))
			cpp << "\n";
	}

	if (frameworkThemeProgram)
		cpp << "\treturn __styleSheet;\n";
	cpp << "}\n\n";

	if (!frameworkThemeProgram)
	{
		// 析构函数
		cpp << className << "::~" << classLeaf << "()\n";
		cpp << "{\n";
		cpp << "}\n";
	}

	const bool hasDataBindings =
		HasGeneratedDataBindings(_sourceDocument, _styleSheet);
	if (hasDataBindings && !frameworkThemeProgram)
	{
		cpp << "\n";
		cpp << "bool " << className
			<< "::BindData(BindingSourceReference dataContext)\n";
		cpp << "{\n";
		cpp << "\tif (!dataContext) return false;\n";
		cpp << "\tauto __windowDataContext = dataContext;\n";
		cpp << "\tif (!SetDataContext(std::move(dataContext))) return false;\n";
		cpp << "\tbool success = true;\n";
		auto emitBindings = [&](const auto& bindings,
			const std::string& controlVar, bool isWindow,
			const DesignerModel::DesignNode* targetNode)
		{
			if (bindings.empty()) return;
			cpp << "\t" << controlVar << "->DataBindings.Clear();\n";
			auto resolveDocumentBindingEndpoint = [&] (
				const DesignerDataBinding& sourceBinding,
				const std::wstring& targetProperty,
				bool multiSource,
				std::string_view indent,
				const char* context)
			{
				StaticBindingSourceSpec source;
				source.AdapterExpression = multiSource
					? "static_cast<IBindingSource*>(&" + controlVar
						+ "->DataContextSource())"
					: controlVar + "->DataContextSource()";
				source.SourceSchema =
					&canonicalDataDocument.DataContextSchema;
				if (!sourceBinding.ElementName.empty())
				{
					if (sourceBinding.ElementName
						== _sourceDocument.Window.Name)
					{
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(this)"
							: "*this";
						source.DirectObjectExpression = "*this";
						source.DirectWindow = true;
					}
					else
					{
						const auto found = std::find_if(
							_sourceDocument.Nodes.begin(),
							_sourceDocument.Nodes.end(),
							[&](const auto& candidate)
							{
								return candidate.Name
									== sourceBinding.ElementName;
							});
						if (found == _sourceDocument.Nodes.end())
							throw std::invalid_argument(
								"Binding ElementName source is missing");
						const auto sourceVar = GetVarName(*found);
						source.AdapterExpression = multiSource
							? "static_cast<IBindingSource*>(" + sourceVar + ")"
							: "*" + sourceVar;
						source.DirectObjectExpression = "*" + sourceVar;
						source.DirectNode = &*found;
					}
					source.SourceSchema = nullptr;
				}
				else if (sourceBinding.RelativeSource
					== DesignerBindingRelativeSource::Self)
				{
					source.AdapterExpression = multiSource
						? "static_cast<IBindingSource*>(" + controlVar + ")"
						: "*" + controlVar;
					source.DirectObjectExpression = "*" + controlVar;
					source.DirectWindow = isWindow;
					source.DirectNode = targetNode;
					source.SourceSchema = nullptr;
				}
				else if (sourceBinding.RelativeSource
					== DesignerBindingRelativeSource::TemplatedParent)
					throw std::invalid_argument(
						"TemplatedParent requires a template");
				else if (sourceBinding.RelativeSource
					== DesignerBindingRelativeSource::FindAncestor)
				{
					source.AdapterExpression =
						"cui::binding::CreateFindAncestorSource(*"
						+ controlVar + ", "
						+ GeneratedFindAncestorTypeExpression(
							_sourceDocument, sourceBinding) + ", "
						+ std::to_string(sourceBinding.AncestorLevel) + ")";
					source.SourceSchema = nullptr;
				}
				else if (targetProperty == L"DataContext")
				{
					if (isWindow)
						source.AdapterExpression = "__windowDataContext";
					else if (multiSource)
						source.AdapterExpression =
							"static_cast<IBindingSource*>(&"
							+ controlVar + "->GetInheritanceParent()->"
								"DataContextSource())";
					else
					{
						source.Guard = controlVar
							+ "->GetInheritanceParent() && ";
						source.AdapterExpression = controlVar
							+ "->GetInheritanceParent()->DataContextSource()";
					}
				}
				return lowerBindingSourceEndpoint(
					sourceBinding, source, indent, context);
			};
			for (const auto& [targetProperty, binding] : bindings)
			{
				if (binding.IsMultiBinding())
				{
					cpp << "\t{\n";
					std::string targetOperand = "L\""
						+ EscapeWStringLiteral(targetProperty) + "\"";
					if (!dynamicWindow)
					{
						targetOperand = isWindow
							? FindKnownDependencyPropertyExpression(
								UIClass::UI_Window, targetProperty, true)
							: targetNode
								? FindGeneratedDependencyPropertyExpression(
									*targetNode, targetProperty, true)
								: std::string{};
						if (targetOperand.empty())
							throw std::invalid_argument(
								"Static MultiBinding target has no writable "
								"DependencyProperty identity");
					}
					auto resolveChild = [&] (
						const DesignerDataBinding& child,
						std::string_view indent,
						const char* context)
					{
						return resolveDocumentBindingEndpoint(
							child, targetProperty, true, indent, context);
					};
					emitStaticMultiBinding(
						binding, controlVar, targetOperand,
						resolveChild, "\t\t", "Static MultiBinding",
						"cuiBindingAttached");
					cpp << "\t\tsuccess = success && "
						"cuiBindingAttached;\n";
					cpp << "\t}\n";
					continue;
				}
				const auto options = lowerBindingOptions(binding);
				std::string targetOperand = "L\""
					+ EscapeWStringLiteral(targetProperty) + "\"";
				if (!dynamicWindow)
				{
					targetOperand = isWindow
						? FindKnownDependencyPropertyExpression(
							UIClass::UI_Window, targetProperty, true)
						: targetNode
							? FindGeneratedDependencyPropertyExpression(
								*targetNode, targetProperty, true)
							: std::string{};
					if (targetOperand.empty())
						throw std::invalid_argument(
							"Static Binding target has no writable "
							"DependencyProperty identity");
				}
				cpp << "\t{\n";
				cpp << "\t\tbool cuiBindingAttached = false;\n";
				const char* operationIndent = "\t\t";
				const auto endpoint = resolveDocumentBindingEndpoint(
					binding, targetProperty, false, operationIndent,
					"Static Window Binding");
				if (endpoint.UsesDirectSource())
					cpp << operationIndent
						<< (endpoint.DirectCompiledRecord
							? "// CUI:AOT binding-source=direct-record\n"
							: "// CUI:AOT binding-source=direct-dp\n");
				if (options.ConverterName.empty())
				{
					cpp << operationIndent << "cuiBindingAttached = "
						<< endpoint.Guard
						<< controlVar
						<< "->DataBindings.Add(" << targetOperand << ", "
						<< endpoint.SourceOperand << ", ";
					if (!endpoint.UsesDirectSource())
						cpp << endpoint.SourcePathOperand << ", ";
					cpp << BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode);
					if (options.HasExtendedOptions)
						cpp << ", {}, " << options.Fallback << ", "
							<< options.TargetNull << ", "
							<< options.ConverterParameter << ", "
							<< options.StringFormat;
					cpp << ") != nullptr;\n";
				}
				else
				{
					cpp << operationIndent
						<< "auto cuiConverter = "
						<< bindingConverterExpression(
							options.ConverterName, binding)
						<< ";\n";
					cpp << operationIndent
						<< "cuiBindingAttached = cuiConverter && "
						<< endpoint.Guard << controlVar
						<< "->DataBindings.Add(" << targetOperand << ", "
						<< endpoint.SourceOperand << ", ";
					if (!endpoint.UsesDirectSource())
						cpp << endpoint.SourcePathOperand << ", ";
					cpp << BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode)
						<< ", cuiConverter";
					if (options.HasExtendedOptions)
						cpp << ", " << options.Fallback << ", "
							<< options.TargetNull << ", "
							<< options.ConverterParameter << ", "
							<< options.StringFormat;
					cpp << ") != nullptr;\n";
				}
				cpp << "\t\tif (!cuiBindingAttached)\n\t\t{\n";
				cpp << "\t\t\tsuccess = false;\n";
				cpp << "\t\t}\n";
				cpp << "\t}\n";
			}
		};
		emitBindings(_sourceDocument.Window.Bindings, "this", true, nullptr);
		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated) continue;
			if (node.Bindings.empty()) continue;
			emitBindings(node.Bindings, GetVarName(node), false, &node);
		}
		cpp << "\treturn success;\n";
		cpp << "}\n";
	}

	// 事件处理函数定义
	if (dynamicWindow)
	{
		std::vector<std::pair<std::string, std::string>> defs;
		std::wstring eventError;
		if (!CollectEventHandlers(defs, &eventError))
			throw std::invalid_argument(WStringToString(eventError));

		if (!defs.empty())
		{
			cpp << "\n";
			for (const auto& d : defs)
			{
				cpp << "void " << className << "::" << d.first << "(" << d.second << ")\n";
				cpp << "{\n";
				cpp << GenerateUnusedParameterLines(d.second);
				cpp << "}\n\n";
			}
		}
	}
	
	auto source = cpp.str();
	if (source.find("cui::framework::XamlAccess::") == std::string::npos)
	{
		constexpr std::string_view include =
			"#include \"XamlInfrastructure.h\"\n";
		if (const auto position = source.find(include);
			position != std::string::npos)
			source.erase(position, include.size());
	}
	if (source.find("cui::framework::TreeAccess::") == std::string::npos)
	{
		constexpr std::string_view include =
			"#include \"TreeInfrastructure.h\"\n";
		if (const auto position = source.find(include);
			position != std::string::npos)
			source.erase(position, include.size());
	}
	if (!dynamicWindow)
	{
		static constexpr std::string_view forbiddenProductionFragments[] = {
			"#include \"XamlFrameworkTheme.h\"",
			"#include \"XamlInfrastructure.h\"",
			"CuiRuntime::",
			"DesignerModel::",
			"cui::framework::XamlAccess::",
			"DeclarativeTypeDescriptor::Create",
			"XamlObjectMaterializer",
			"RuntimeDocument",
			"std::make_shared<ControlStyleSheet>",
			"->AddRule(",
			"->RemoveRule(",
			"->ClearRules(",
			"->SetResource(",
			"->RemoveResource(",
			"->ClearResources(",
			"ControlStyleSelector",
			"ControlStyleSetter",
			"CompiledStyleProgram{",
		};
		for (const auto fragment : forbiddenProductionFragments)
			if (source.find(fragment) != std::string::npos)
				throw std::invalid_argument(
					"Static C++ generation requires a native lowering for every "
					"authored construct; dynamic XAML fragment escaped: "
					+ std::string(fragment));
	}
	return source;
}

bool CodeGenerator::BuildFilePlan(
	std::wstring headerPath,
	std::wstring cppPath,
	std::vector<CodeGeneratorFileContent>& files)
{
	files.clear();
	_lastError.clear();
	try
	{
		namespace fs = std::filesystem;
		const fs::path userHeaderPath(headerPath);
		const fs::path userCppPath(cppPath);
		if (headerPath.empty() || cppPath.empty())
		{
			_lastError = L"导出路径不能为空。";
			return false;
		}

		std::vector<std::pair<std::string, std::string>> currentHandlers;
		if (!CollectEventHandlers(currentHandlers, &_lastError)) return false;

		const auto baseName = userHeaderPath.stem().wstring();
		const auto baseNameUtf8 = WStringToString(baseName);
		const auto identity = ParseQualifiedCppClassName(
			WStringToString(_className));
		const auto generatedHeaderPath = userHeaderPath.parent_path()
			/ fs::path(baseName + L".g.h");
		const auto generatedCppPath = userCppPath.parent_path()
			/ fs::path(baseName + L".g.cpp");
		const auto handlerIncludePath = userHeaderPath.parent_path()
			/ fs::path(baseName + L".handlers.g.inc");

		DesignerModel::AtomicFileBatchSnapshot inputSnapshot;
		std::wstring snapshotError;
		if (!DesignerModel::AtomicFileBatchSnapshot::Capture({
			userHeaderPath.wstring(),
			userCppPath.wstring(),
			generatedHeaderPath.wstring(),
			generatedCppPath.wstring(),
			handlerIncludePath.wstring(),
		}, inputSnapshot, &snapshotError))
		{
			_lastError = snapshotError.empty()
				? L"无法捕获代码生成输入文件快照。"
				: std::move(snapshotError);
			return false;
		}
		const auto& existingFiles = inputSnapshot.Entries();
		if (existingFiles.size() != 5)
		{
			_lastError = L"代码生成输入文件快照不完整。";
			return false;
		}
		auto requireRecognizedUserFile = [&](
			const DesignerModel::AtomicFileSnapshotEntry& snapshot,
			const char* marker, std::string& content) -> bool
		{
			if (!snapshot.Existed) return true;
			content = snapshot.Content;
			if (content.find(marker) == std::string::npos)
			{
				_lastError = L"为避免覆盖已有代码，未修改文件："
					+ snapshot.FilePath
					+ L"。请选择新的导出文件名，或手动迁移到生成基类结构。";
				return false;
			}
			return true;
		};

		std::string existingUserHeader;
		std::string existingUserCpp;
		if (!requireRecognizedUserFile(existingFiles[0],
			"<cui-designer-user-header>", existingUserHeader) ||
			!requireRecognizedUserFile(existingFiles[1],
			"<cui-designer-user-source>", existingUserCpp))
			return false;
		auto matchesIdentityMarker = [&](const std::string& content)
		{
			const auto marker = ReadUserClassIdentityMarker(content);
			if (marker) return *marker == identity.QualifiedUser;
			return identity.Segments.size() == 1;
		};
		if ((!existingUserHeader.empty()
				&& !matchesIdentityMarker(existingUserHeader))
			|| (!existingUserCpp.empty()
				&& !matchesIdentityMarker(existingUserCpp)))
		{
			_lastError = L"现有 Designer 用户文件属于不同的 C++ 类；"
				L"为避免生成基类与用户类身份混用，请选择新的导出基路径，"
				L"或先手动迁移用户代码。";
			return false;
		}

		DesignerModel::CppUserCodeIndex userHeaderIndex;
		DesignerModel::CppUserCodeIndex userSourceIndex;
		DesignerModel::CppUserHandlerDefinitionInspection headerConstructor;
		DesignerModel::CppUserHandlerDefinitionInspection sourceConstructor;
		std::wstring constructorIndexError;
		if (!existingUserHeader.empty())
		{
			if (!DesignerModel::CppUserCodeIndex::Build(
				existingUserHeader, identity.QualifiedUser,
				userHeaderIndex, &constructorIndexError))
			{
				_lastError = constructorIndexError.empty()
					? L"无法建立用户头文件代码索引。"
					: std::move(constructorIndexError);
				return false;
			}
			const auto classDefinition =
				userHeaderIndex.InspectGeneratedClassDefinition();
			if (classDefinition.DefinitionCount != 1
				|| classDefinition.CompatibleGeneratedBaseCount != 1)
			{
				_lastError = L"用户头文件必须在当前 x:Class namespace 中"
					L"恰好定义一个用户类，并直接继承对应的 Generated 基类。";
				return false;
			}
			headerConstructor = userHeaderIndex.InspectConstructor();
		}
		if (!existingUserCpp.empty())
		{
			if (!DesignerModel::CppUserCodeIndex::Build(
				existingUserCpp, identity.QualifiedUser,
				userSourceIndex, &constructorIndexError))
			{
				_lastError = constructorIndexError.empty()
					? L"无法建立用户源文件代码索引。"
					: std::move(constructorIndexError);
				return false;
			}
			sourceConstructor = userSourceIndex.InspectConstructor();
		}
		const auto compatibleConstructorCount =
			headerConstructor.CompatibleDefinitionCount
			+ sourceConstructor.CompatibleDefinitionCount;
		const auto deletedConstructorCount =
			headerConstructor.DeletedCompatibleDefinitionCount
			+ sourceConstructor.DeletedCompatibleDefinitionCount;
		if (deletedConstructorCount != 0
			|| compatibleConstructorCount > 1
			|| (!existingUserCpp.empty()
				&& compatibleConstructorCount != 1))
		{
			_lastError = deletedConstructorCount != 0
				? L"用户类的默认构造函数已被删除，无法实例化生成窗体。"
				: compatibleConstructorCount > 1
					? L"用户类的默认构造函数在头文件或源文件中存在多个定义。"
					: L"用户类缺少默认构造函数定义；可在头文件中内联，"
						L"或在用户源文件中定义。";
			return false;
		}

		std::map<std::string, std::string> retainedHandlers;
		const auto& oldHandlerInclude = existingFiles[4].Content;
		std::istringstream oldLines(oldHandlerInclude);
		for (std::string line; std::getline(oldLines, line);)
		{
			const auto first = line.find_first_not_of(" \t");
			if (first == std::string::npos || line.compare(first, 5, "void ") != 0) continue;
			const auto open = line.find('(', first + 5);
			const auto semicolon = line.rfind(';');
			const auto close = semicolon == std::string::npos
				? std::string::npos : line.rfind(')', semicolon);
			if (open == std::string::npos || close == std::string::npos
				|| semicolon == std::string::npos || close < open) continue;
			const auto name = line.substr(first + 5, open - (first + 5));
			const auto params = line.substr(open + 1, close - open - 1);
			if (!name.empty()) retainedHandlers[name] = params;
		}
		for (const auto& [name, params] : currentHandlers)
		{
			auto previous = retainedHandlers.find(name);
			if (previous != retainedHandlers.end()
				&& CanonicalGeneratedParameterTypes(previous->second)
					!= CanonicalGeneratedParameterTypes(params))
			{
				_lastError = L"已有用户处理函数 “"
					+ StringToWString(name)
					+ L"” 的参数签名与新事件不兼容。请改用新的函数名。";
				return false;
			}
			retainedHandlers[name] = params;
		}

		std::ostringstream handlerInclude;
		handlerInclude << "// Generated by CUI Designer. Do not edit.\n";
		handlerInclude << "// Declarations are retained after unbinding so existing user definitions keep compiling.\n";
		for (const auto& [name, params] : retainedHandlers)
		{
			const auto inlineDefinitions =
				userHeaderIndex.InspectHandler(name, params);
			if (inlineDefinitions.CompatibleDefinitionCount > 1)
			{
				_lastError = L"用户头文件中的处理函数 “"
					+ StringToWString(name)
					+ L"” 存在多个相同签名的内联定义。";
				return false;
			}
			// A second declaration in the generated include would conflict with
			// an in-class definition of the same member.
			if (inlineDefinitions.CompatibleDefinitionCount == 1) continue;
			const auto active = std::any_of(currentHandlers.begin(), currentHandlers.end(),
				[&](const auto& handler)
				{
					return handler.first == name
						&& CanonicalGeneratedParameterTypes(handler.second)
							== CanonicalGeneratedParameterTypes(params);
				});
			handlerInclude << "\tvoid " << name << "(" << params << ")"
				<< (active ? " override" : "") << ";\n";
		}

		std::ostringstream newUserHeader;
		newUserHeader
			<< "#pragma once\n"
			<< "// <cui-designer-user-header> Created once; safe for user edits.\n"
			<< "// <cui-designer-class>" << identity.QualifiedUser
			<< "</cui-designer-class>\n"
			<< "#include \"" << baseNameUtf8 << ".g.h\"\n\n";
		if (!identity.NamespaceName.empty())
			newUserHeader << "namespace " << identity.NamespaceName << "\n{\n\n";
		newUserHeader
			<< "class " << identity.UserLeaf << " : public "
			<< identity.GeneratedLeaf << "\n"
			<< "{\n"
			<< "public:\n"
			<< "\t" << identity.UserLeaf << "();\n"
			<< "\t~" << identity.UserLeaf << "() override = default;\n\n"
			<< "private:\n"
			<< "#include \"" << baseNameUtf8 << ".handlers.g.inc\"\n"
			<< "};\n";
		if (!identity.NamespaceName.empty()) newUserHeader << "\n}\n";

		std::ostringstream newUserCpp;
		if (existingUserCpp.empty())
		{
			newUserCpp
				<< "// <cui-designer-user-source> Created once; safe for user edits.\n"
				<< "// <cui-designer-class>" << identity.QualifiedUser
				<< "</cui-designer-class>\n"
				<< "#include \"" << baseNameUtf8 << ".h\"\n\n";
			if (headerConstructor.CompatibleDefinitionCount == 0)
				newUserCpp
					<< identity.QualifiedUser << "::" << identity.UserLeaf << "()\n"
					<< "\t: " << identity.QualifiedGenerated << "()\n"
					<< "{\n"
					<< "\tInitializeComponent();\n"
					<< "\t// User initialization belongs here.\n"
					<< "}\n";
		}
		else
			newUserCpp << existingUserCpp;

		auto appendUnusedParameters = [&](std::ostringstream& output,
			const std::string& params)
		{
			size_t begin = 0;
			while (begin < params.size())
			{
				auto comma = params.find(',', begin);
				if (comma == std::string::npos) comma = params.size();
				auto end = comma;
				while (end > begin && std::isspace(
					static_cast<unsigned char>(params[end - 1]))) --end;
				auto nameBegin = end;
				while (nameBegin > begin)
				{
					const auto ch = static_cast<unsigned char>(params[nameBegin - 1]);
					if (!std::isalnum(ch) && ch != '_') break;
					--nameBegin;
				}
				if (nameBegin < end)
					output << "\t(void)" << params.substr(nameBegin, end - nameBegin) << ";\n";
				begin = comma + 1;
			}
		};
		for (const auto& [name, params] : currentHandlers)
		{
			const auto headerDefinitions =
				userHeaderIndex.InspectHandler(name, params);
			const auto sourceDefinitions =
				userSourceIndex.InspectHandler(name, params);
			const auto definitionCount =
				headerDefinitions.DefinitionCount
				+ sourceDefinitions.DefinitionCount;
			const auto compatibleDefinitions =
				headerDefinitions.CompatibleDefinitionCount
				+ sourceDefinitions.CompatibleDefinitionCount;
			const auto incompatibleShapes =
				headerDefinitions.IncompatibleShapeDefinitionCount
				+ sourceDefinitions.IncompatibleShapeDefinitionCount;
			const auto deletedDefinitions =
				headerDefinitions.DeletedCompatibleDefinitionCount
				+ sourceDefinitions.DeletedCompatibleDefinitionCount;
			if (definitionCount != 0)
			{
				if (compatibleDefinitions > 1)
				{
					_lastError = L"用户头文件或源文件中的处理函数 “"
						+ StringToWString(name)
						+ L"” 存在多个相同签名的定义。"
							L"请仅保留一个定义后重新生成。";
					return false;
				}
				if (incompatibleShapes == 0 && deletedDefinitions == 0
					&& compatibleDefinitions == 1) continue;
				_lastError = L"用户头文件或源文件中的处理函数 “"
					+ StringToWString(name)
					+ L"” 的返回类型、static/cv/ref 限定或参数签名"
						L"与设计事件不兼容。"
						L"请修正该定义，或在设计器中改用新的函数名。";
				return false;
			}
			newUserCpp << "\nvoid " << identity.QualifiedUser << "::" << name
				<< "(" << params << ")\n{\n";
			appendUnusedParameters(newUserCpp, params);
			newUserCpp << "}\n";
		}

		files.reserve(5);
		auto appendPlannedFile = [&](size_t snapshotIndex,
			const fs::path& path, std::string content)
		{
			const auto& expected = existingFiles[snapshotIndex];
			files.push_back({ path.wstring(), std::move(content),
				expected.Existed, expected.Content });
		};
		appendPlannedFile(0, userHeaderPath,
			existingUserHeader.empty()
				? newUserHeader.str() : existingUserHeader);
		appendPlannedFile(1, userCppPath, newUserCpp.str());
		appendPlannedFile(2, generatedHeaderPath, GenerateHeader());
		appendPlannedFile(3, generatedCppPath,
			GenerateCppForBaseName(baseNameUtf8));
		appendPlannedFile(4, handlerIncludePath, handlerInclude.str());
		return true;
	}
	catch (const std::exception& error)
	{
		files.clear();
		_lastError = L"准备代码生成计划失败：" + StringToWString(error.what());
		return false;
	}
	catch (...)
	{
		files.clear();
		_lastError = L"准备代码生成计划时发生未知错误。";
		return false;
	}
}

bool CodeGenerator::GenerateFiles(std::wstring headerPath, std::wstring cppPath)
{
	std::vector<CodeGeneratorFileContent> files;
	if (!BuildFilePlan(std::move(headerPath), std::move(cppPath), files))
		return false;
	try
	{
		std::vector<DesignerModel::AtomicFileWriteEntry> writes;
		writes.reserve(files.size());
		for (auto& file : files)
		{
			DesignerModel::AtomicFileWriteEntry write;
			write.FilePath = std::move(file.Path);
			write.Content = std::move(file.Content);
			write.RequireExpectedState = true;
			write.ExpectedExisted = file.ExpectedExisted;
			write.ExpectedContent = std::move(file.ExpectedContent);
			writes.push_back(std::move(write));
		}
		std::wstring writeError;
		if (!DesignerModel::AtomicFile::WriteBatch(writes, &writeError))
		{
			_lastError = L"代码文件批次提交失败；已尝试恢复导出前版本。";
			if (!writeError.empty()) _lastError += L"\n" + writeError;
			return false;
		}
		return true;
	}
	catch (const std::exception& error)
	{
		_lastError = L"提交代码生成计划失败：" + StringToWString(error.what());
		return false;
	}
	catch (...)
	{
		_lastError = L"提交代码生成计划时发生未知错误。";
		return false;
	}
}
