#pragma once

#include "DesignDocument.h"
#include "../../CUI/include/PropertyPath.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace DesignerModel
{
struct ResolvedTransformAnimationPath
{
	std::wstring CanonicalPath;
	size_t OperationIndex = 0;
	std::wstring TransformType;
	std::wstring PropertyName;
};

/** The object graph adapter selected for a Storyboard.TargetProperty path. */
enum class StoryboardObjectPathKind : unsigned char
{
	None,
	RenderTransform,
	RenderTransformMatrix,
	RectangleGeometryRect,
	RectangleGeometryRadius,
	EllipseGeometryCenter,
	EllipseGeometryRadius,
	GeometryFillRule,
	PathGeometryPoint,
	PathGeometrySize,
	PathGeometryDouble,
	PathGeometryBool,
	PathGeometrySweep,
	GeometryTransform,
	GeometryTransformMatrix,
	SolidColorBrushColor,
	BrushOpacity,
	GradientBrushPoint,
	RadialGradientBrushRadius,
	GradientStopColor,
	GradientStopOffset,
	BrushTransform,
	BrushTransformMatrix,
	BrushRelativeTransform,
	BrushRelativeTransformMatrix,
};

/**
 * Common designer-side result for an indirect Storyboard target. Adapter-
 * specific details remain optional so future Brush and Geometry paths can be
 * added behind the same parser/validator boundary.
 */
struct ResolvedStoryboardObjectPath
{
	StoryboardObjectPathKind Kind = StoryboardObjectPathKind::None;
	std::wstring CanonicalPath;
	std::wstring RootProperty;
	size_t OperationIndex = 0;
	size_t CollectionIndex = 0;
	std::vector<size_t> GeometryChildIndices;
	std::wstring ObjectType;
	std::wstring LeafProperty;
};

inline const DesignValue* StoryboardMetadataObject(
	const DesignNode& node,
	const wchar_t* propertyName) noexcept
{
	const auto* assignment = node.Properties.Find(propertyName);
	return assignment && !assignment->Value.ObjectValue.is_null()
		? &assignment->Value.ObjectValue : nullptr;
}

inline bool StoryboardPathEquals(
	const std::wstring& left,
	const std::wstring& right) noexcept
{
	return left == right;
}

inline std::wstring StoryboardPathLocalType(const std::wstring& value)
{
	const auto separator = value.rfind(L':');
	return separator == std::wstring::npos
		? value : value.substr(separator + 1);
}

inline bool IsMatrixTransformLeaf(const cui::xaml::PropertyPath& path) noexcept
{
	if (path.Segments.empty()) return false;
	const auto& leaf = path.Segments.back();
	return leaf.Kind == cui::xaml::PropertyPathSegmentKind::Property
		&& StoryboardPathEquals(
			StoryboardPathLocalType(leaf.OwnerType), L"MatrixTransform")
		&& StoryboardPathEquals(leaf.Name, L"Matrix");
}

inline bool TryResolveTransformPathLeaf(
	size_t operationIndex,
	const std::wstring& leafOwner,
	const std::wstring& leaf,
	ResolvedTransformAnimationPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	std::wstring canonicalOwner;
	std::wstring canonicalLeaf;
	auto leafAllowed = [&](std::initializer_list<const wchar_t*> names)
	{
		const auto found = std::find_if(names.begin(), names.end(),
			[&](const auto* name) { return StoryboardPathEquals(leaf, name); });
		if (found == names.end()) return false;
		canonicalLeaf = *found;
		return true;
	};
	bool validLeaf = false;
	if (StoryboardPathEquals(leafOwner, L"TranslateTransform"))
	{
		canonicalOwner = L"TranslateTransform";
		validLeaf = leafAllowed({ L"X", L"Y" });
	}
	else if (StoryboardPathEquals(leafOwner, L"ScaleTransform"))
	{
		canonicalOwner = L"ScaleTransform";
		validLeaf = leafAllowed({ L"ScaleX", L"ScaleY", L"CenterX", L"CenterY" });
	}
	else if (StoryboardPathEquals(leafOwner, L"RotateTransform"))
	{
		canonicalOwner = L"RotateTransform";
		validLeaf = leafAllowed({ L"Angle", L"CenterX", L"CenterY" });
	}
	else if (StoryboardPathEquals(leafOwner, L"SkewTransform"))
	{
		canonicalOwner = L"SkewTransform";
		validLeaf = leafAllowed({ L"AngleX", L"AngleY", L"CenterX", L"CenterY" });
	}
	else if (StoryboardPathEquals(leafOwner, L"MatrixTransform"))
	{
		canonicalOwner = L"MatrixTransform";
		validLeaf = leafAllowed({ L"Matrix" });
	}
	if (!validLeaf)
		return fail(L"动画路径的 Transform 类型或末端属性不受支持。");

	output.OperationIndex = operationIndex;
	output.TransformType = std::move(canonicalOwner);
	output.PropertyName = std::move(canonicalLeaf);
	if (outError) outError->clear();
	return true;
}

inline bool TryResolveTransformOperationLeaf(
	const DesignValue& operations,
	size_t operationIndex,
	const std::wstring& leafOwner,
	const std::wstring& leaf,
	ResolvedTransformAnimationPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	if (!operations.is_array() || operationIndex >= operations.size())
		return fail(L"动画目标未声明路径所需的 Transform 操作。");
	const auto& operation = operations[operationIndex];
	if (!operation.is_object() || !operation.contains("type")
		|| !operation["type"].is_string())
		return fail(L"Transform 操作格式无效。");

	ResolvedTransformAnimationPath resolved;
	if (!TryResolveTransformPathLeaf(
		operationIndex, leafOwner, leaf, resolved, nullptr))
		return fail(L"动画路径的 Transform 类型或末端属性与实际操作不匹配。");
	const auto type = operation["type"].get<std::string>();
	const bool typeMatches =
		(type == "translate" && resolved.TransformType == L"TranslateTransform")
		|| (type == "scale" && resolved.TransformType == L"ScaleTransform")
		|| (type == "rotate" && resolved.TransformType == L"RotateTransform")
		|| (type == "skew" && resolved.TransformType == L"SkewTransform")
		|| (type == "matrix" && resolved.TransformType == L"MatrixTransform");
	if (!typeMatches)
		return fail(L"动画路径的 Transform 类型或末端属性与实际操作不匹配。");

	output = std::move(resolved);
	if (outError) outError->clear();
	return true;
}

/** Resolves the first supported object adapter: RenderTransform operations. */
inline bool TryResolveTransformAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	ResolvedTransformAnimationPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	const bool renderTransformRoot = !path.Segments.empty()
		&& path.Segments[0].Kind
			== cui::xaml::PropertyPathSegmentKind::Property
		&& StoryboardPathEquals(path.Segments[0].Name, L"RenderTransform")
		&& (StoryboardPathEquals(
				StoryboardPathLocalType(path.Segments[0].OwnerType), L"Control")
			|| StoryboardPathEquals(
				StoryboardPathLocalType(path.Segments[0].OwnerType), L"UIElement"));
	const bool directTransform = renderTransformRoot
		&& path.Segments.size() == 2
		&& path.Segments[1].Kind
			== cui::xaml::PropertyPathSegmentKind::Property;
	const bool groupedTransform = renderTransformRoot
		&& path.Segments.size() == 4
		&& path.Segments[1].Kind
			== cui::xaml::PropertyPathSegmentKind::Property
		&& path.Segments[2].Kind
			== cui::xaml::PropertyPathSegmentKind::Index
		&& path.Segments[3].Kind
			== cui::xaml::PropertyPathSegmentKind::Property
		&& StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[1].OwnerType), L"TransformGroup")
		&& StoryboardPathEquals(path.Segments[1].Name, L"Children");
	if (!directTransform && !groupedTransform)
		return fail(L"Transform 动画路径必须是 "
			L"(Control.RenderTransform).(TransformType.Property) 或 "
			L"(Control.RenderTransform).(TransformGroup.Children)[n]."
			L"(TransformType.Property)。");

	const size_t operationIndex =
		directTransform ? 0 : path.Segments[2].Index;
	const auto& terminal =
		directTransform ? path.Segments[1] : path.Segments[3];
	const auto leafOwner = StoryboardPathLocalType(terminal.OwnerType);
	const auto& leaf = terminal.Name;
	if (targetName.empty())
	{
		// A Style Storyboard without TargetName targets the styled control.
		// Its concrete RenderTransform belongs to the eventual style instance,
		// so the AOT frontend can validate only the declared object path here.
		if (!TryResolveTransformPathLeaf(
			operationIndex, leafOwner, leaf, output, outError)) return false;
	}
	else
	{
		const auto target = std::find_if(
			component.Template.begin(), component.Template.end(),
			[&](const auto& node)
			{ return StoryboardPathEquals(node.Name, targetName); });
		const auto* renderTransform = target == component.Template.end()
			? nullptr : StoryboardMetadataObject(*target, L"RenderTransform");
		if (!renderTransform || !renderTransform->is_array()
			|| (directTransform && renderTransform->size() != 1)
			|| operationIndex >= renderTransform->size())
			return fail(L"动画目标未声明路径所需的 RenderTransform 操作。");
		if (!TryResolveTransformOperationLeaf(
			*renderTransform, operationIndex,
			leafOwner, leaf, output, outError)) return false;
	}
	output.CanonicalPath = directTransform
		? L"(Control.RenderTransform).(" + output.TransformType
			+ L"." + output.PropertyName + L")"
		: L"(Control.RenderTransform)."
			L"(TransformGroup.Children)["
			+ std::to_wstring(output.OperationIndex)
			+ L"].(" + output.TransformType + L"."
			+ output.PropertyName + L")";
	if (outError) outError->clear();
	return true;
}

inline bool IsTransformAnimationPath(const std::wstring& value) noexcept
{
	constexpr std::wstring_view controlPrefix = L"(Control.RenderTransform)";
	constexpr std::wstring_view elementPrefix = L"(UIElement.RenderTransform)";
	return value.starts_with(controlPrefix)
		|| value.starts_with(elementPrefix);
}

/** Resolves zero or more GeometryGroup.Children[index] hops after Clip. */
inline bool TryResolveDesignGeometryTreeTarget(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const cui::xaml::PropertyPath& path,
	const DesignValue*& outGeometry,
	std::vector<size_t>& outChildIndices,
	size_t& outLeafStart,
	std::wstring& outCanonicalPrefix,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	if (targetName.empty())
		return fail(L"Geometry 复合属性路径只能定位模板具名控件。");
	if (path.Segments.empty()
		|| path.Segments[0].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| !StoryboardPathEquals(path.Segments[0].Name, L"Clip")
		|| (!StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[0].OwnerType), L"Control")
			&& !StoryboardPathEquals(
				StoryboardPathLocalType(path.Segments[0].OwnerType), L"UIElement")))
		return fail(L"Geometry 复合动画路径必须以 (Control.Clip) 开始。");
	const auto target = std::find_if(
		component.Template.begin(), component.Template.end(),
		[&](const auto& node)
		{ return StoryboardPathEquals(node.Name, targetName); });
	const auto* clip = target == component.Template.end()
		? nullptr : StoryboardMetadataObject(*target, L"Clip");
	if (!clip || !clip->is_object())
		return fail(L"动画目标必须显式声明 Control.Clip Geometry。");

	outChildIndices.clear();
	outCanonicalPrefix = L"(Control.Clip)";
	const DesignValue* geometry = clip;
	size_t cursor = 1;
	while (cursor < path.Segments.size()
		&& path.Segments[cursor].Kind
			== cui::xaml::PropertyPathSegmentKind::Property
		&& StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[cursor].OwnerType),
			L"GeometryGroup")
		&& StoryboardPathEquals(path.Segments[cursor].Name, L"Children"))
	{
		if (cursor + 1 >= path.Segments.size()
			|| path.Segments[cursor + 1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index)
			return fail(L"GeometryGroup.Children 后必须提供有效索引。");
		if (geometry->value("type", std::string{}) != "group"
			|| !geometry->contains("children")
			|| !(*geometry)["children"].is_array())
			return fail(L"GeometryGroup.Children 路径所有者与实际 Geometry 类型不匹配。");
		const auto index = path.Segments[cursor + 1].Index;
		if (index >= (*geometry)["children"].size())
			return fail(L"GeometryGroup.Children 动画索引超出范围。");
		const auto& child = (*geometry)["children"][index];
		if (!child.is_object()) return fail(L"GeometryGroup 子 Geometry 数据无效。");
		outChildIndices.push_back(index);
		geometry = &child;
		outCanonicalPrefix += L".(GeometryGroup.Children)["
			+ std::to_wstring(index) + L"]";
		cursor += 2;
	}
	if (cursor >= path.Segments.size())
		return fail(L"GeometryGroup.Children 索引后缺少动画末端属性。");
	outGeometry = geometry;
	outLeafStart = cursor;
	return true;
}

/** Resolves WPF-style public Rectangle/Ellipse members through Control.Clip. */
inline bool TryResolveGeometryPropertyAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	const DesignValue* resolvedGeometry = nullptr;
	std::vector<size_t> childIndices;
	size_t leafStart = 0;
	std::wstring canonicalPrefix;
	if (!TryResolveDesignGeometryTreeTarget(component, targetName, path,
		resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
		return false;
	if (path.Segments.size() != leafStart + 1
		|| path.Segments[leafStart].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property)
		return fail(L"Geometry 动画路径必须在目标 Geometry 后定位一个公开成员。");
	const auto& geometry = *resolvedGeometry;
	const auto type = geometry.value("type", std::string{});
	const auto owner = StoryboardPathLocalType(path.Segments[leafStart].OwnerType);
	const auto& property = path.Segments[leafStart].Name;
	auto finiteNumber = [&](const char* key)
	{
		return geometry.contains(key) && geometry[key].is_number()
			&& std::isfinite(geometry[key].get<double>());
	};
	auto assign = [&](StoryboardObjectPathKind kind,
		std::wstring_view objectType, std::wstring_view leaf)
	{
		output = {};
		output.Kind = kind;
		output.RootProperty = L"Clip";
		output.GeometryChildIndices = childIndices;
		output.ObjectType = objectType;
		output.LeafProperty = leaf;
		output.CanonicalPath = canonicalPrefix + L".(" + output.ObjectType
			+ L"." + output.LeafProperty + L")";
		if (outError) outError->clear();
		return true;
	};
	if (type == "rectangle"
		&& StoryboardPathEquals(owner, L"RectangleGeometry"))
	{
		if (StoryboardPathEquals(property, L"Rect"))
		{
			if (animationKind != DesignerAnimationKind::Rect)
				return fail(L"RectangleGeometry.Rect 只接受 RectAnimation。");
			if (!finiteNumber("x") || !finiteNumber("y")
				|| !finiteNumber("width") || !finiteNumber("height")
				|| geometry["width"].get<double>() < 0.0
				|| geometry["height"].get<double>() < 0.0)
				return fail(L"动画目标的 RectangleGeometry.Rect 无效。");
			return assign(StoryboardObjectPathKind::RectangleGeometryRect,
				L"RectangleGeometry", L"Rect");
		}
		if (StoryboardPathEquals(property, L"RadiusX")
			|| StoryboardPathEquals(property, L"RadiusY"))
		{
			if (animationKind != DesignerAnimationKind::Double)
				return fail(L"RectangleGeometry 圆角半径只接受 DoubleAnimation。");
			const bool x = StoryboardPathEquals(property, L"RadiusX");
			const char* key = x ? "radiusX" : "radiusY";
			if (!finiteNumber(key) || geometry[key].get<double>() < 0.0)
				return fail(L"动画目标的 RectangleGeometry 圆角半径无效。");
			return assign(StoryboardObjectPathKind::RectangleGeometryRadius,
				L"RectangleGeometry", x ? L"RadiusX" : L"RadiusY");
		}
	}
	if (type == "ellipse"
		&& StoryboardPathEquals(owner, L"EllipseGeometry"))
	{
		if (StoryboardPathEquals(property, L"Center"))
		{
			if (animationKind != DesignerAnimationKind::Point)
				return fail(L"EllipseGeometry.Center 只接受 PointAnimation。");
			if (!finiteNumber("centerX") || !finiteNumber("centerY"))
				return fail(L"动画目标的 EllipseGeometry.Center 无效。");
			return assign(StoryboardObjectPathKind::EllipseGeometryCenter,
				L"EllipseGeometry", L"Center");
		}
		if (StoryboardPathEquals(property, L"RadiusX")
			|| StoryboardPathEquals(property, L"RadiusY"))
		{
			if (animationKind != DesignerAnimationKind::Double)
				return fail(L"EllipseGeometry 半径只接受 DoubleAnimation。");
			const bool x = StoryboardPathEquals(property, L"RadiusX");
			const char* key = x ? "radiusX" : "radiusY";
			if (!finiteNumber(key) || geometry[key].get<double>() < 0.0)
				return fail(L"动画目标的 EllipseGeometry 半径无效。");
			return assign(StoryboardObjectPathKind::EllipseGeometryRadius,
				L"EllipseGeometry", x ? L"RadiusX" : L"RadiusY");
		}
	}
	if (((type == "path" && StoryboardPathEquals(owner, L"PathGeometry"))
			|| (type == "group" && StoryboardPathEquals(owner, L"GeometryGroup")))
		&& StoryboardPathEquals(property, L"FillRule"))
	{
		if (animationKind != DesignerAnimationKind::Object)
			return fail(L"Geometry.FillRule 只接受 ObjectAnimationUsingKeyFrames。");
		const auto fillRule = geometry.value("fillRule", std::string{});
		if (fillRule != "evenodd" && fillRule != "nonzero")
			return fail(L"动画目标的 Geometry.FillRule 无效。");
		return assign(StoryboardObjectPathKind::GeometryFillRule,
			type == "path" ? L"PathGeometry" : L"GeometryGroup", L"FillRule");
	}
	return fail(L"Geometry 动画路径所有者或末端属性与实际 Clip 类型不匹配。");
}

inline bool TryResolvePathGeometryAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	const DesignValue* resolvedGeometry = nullptr;
	std::vector<size_t> childIndices;
	size_t leafStart = 0;
	std::wstring canonicalPrefix;
	if (!TryResolveDesignGeometryTreeTarget(component, targetName, path,
		resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
		return false;
	const auto remaining = path.Segments.size() - leafStart;
	const bool figurePath = remaining == 3;
	const bool segmentPath = remaining == 5;
	if ((!figurePath && !segmentPath)
		|| path.Segments[leafStart].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[leafStart + 1].Kind
			!= cui::xaml::PropertyPathSegmentKind::Index
		|| path.Segments[leafStart + 2].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| (segmentPath
			&& (path.Segments[leafStart + 3].Kind
					!= cui::xaml::PropertyPathSegmentKind::Index
				|| path.Segments[leafStart + 4].Kind
					!= cui::xaml::PropertyPathSegmentKind::Property))
		|| !StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[leafStart].OwnerType), L"PathGeometry")
		|| !StoryboardPathEquals(path.Segments[leafStart].Name, L"Figures")
		|| !StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[leafStart + 2].OwnerType),
			L"PathFigure")
		|| (segmentPath
			&& !StoryboardPathEquals(
				path.Segments[leafStart + 2].Name, L"Segments")))
		return fail(L"PathGeometry 动画路径必须定位 Figures[n] 的 PathFigure "
			L"成员，或继续定位 Segments[n] 的具体 PathSegment 成员。");
	const auto& geometry = *resolvedGeometry;
	if (geometry.value("type", std::string{}) != "path"
		|| !geometry.contains("figures") || !geometry["figures"].is_array())
		return fail(L"动画目标必须显式声明 PathGeometry 类型的 Control.Clip。");
	const auto figureIndex = path.Segments[leafStart + 1].Index;
	if (figureIndex >= geometry["figures"].size())
		return fail(L"PathGeometry.Figures 动画索引超出范围。");
	const auto& figure = geometry["figures"][figureIndex];
	if (!figure.is_object()) return fail(L"PathFigure 数据无效。");
	auto finiteNumber = [](const DesignValue& object, const char* key)
	{
		return object.contains(key) && object[key].is_number()
			&& std::isfinite(object[key].get<double>());
	};
	auto assign = [&](StoryboardObjectPathKind kind,
		std::wstring canonical, std::wstring_view objectType,
		std::wstring_view leaf, size_t segmentIndex = 0)
	{
		output = {};
		output.Kind = kind;
		output.RootProperty = L"Clip";
		output.GeometryChildIndices = childIndices;
		output.CollectionIndex = figureIndex;
		output.OperationIndex = segmentIndex;
		output.ObjectType = objectType;
		output.LeafProperty = leaf;
		output.CanonicalPath = std::move(canonical);
		if (outError) outError->clear();
		return true;
	};
	auto figureCanonical = [&](std::wstring_view leaf)
	{
		return canonicalPrefix + L".(PathGeometry.Figures)["
			+ std::to_wstring(figureIndex) + L"].(PathFigure."
			+ std::wstring(leaf) + L")";
	};
	if (figurePath)
	{
		const auto& property = path.Segments[leafStart + 2].Name;
		if (StoryboardPathEquals(property, L"StartPoint"))
		{
			if (animationKind != DesignerAnimationKind::Point)
				return fail(L"PathFigure.StartPoint 只接受 PointAnimation。");
			if (!finiteNumber(figure, "startX")
				|| !finiteNumber(figure, "startY"))
				return fail(L"动画目标的 PathFigure.StartPoint 无效。");
			return assign(StoryboardObjectPathKind::PathGeometryPoint,
				figureCanonical(L"StartPoint"), L"PathFigure", L"StartPoint");
		}
		if (StoryboardPathEquals(property, L"IsClosed")
			|| StoryboardPathEquals(property, L"IsFilled"))
		{
			if (animationKind != DesignerAnimationKind::Object)
				return fail(L"PathFigure 布尔成员只接受 ObjectAnimationUsingKeyFrames。");
			const bool closed = StoryboardPathEquals(property, L"IsClosed");
			const char* key = closed ? "closed" : "filled";
			if (!figure.contains(key) || !figure[key].is_boolean())
				return fail(L"动画目标的 PathFigure 布尔成员无效。");
			const auto leaf = closed ? L"IsClosed" : L"IsFilled";
			return assign(StoryboardObjectPathKind::PathGeometryBool,
				figureCanonical(leaf), L"PathFigure", leaf);
		}
		return fail(L"尚未支持该 PathFigure 动画成员。");
	}

	if (!figure.contains("segments") || !figure["segments"].is_array())
		return fail(L"PathFigure.Segments 数据无效。");
	const auto segmentIndex = path.Segments[leafStart + 3].Index;
	if (segmentIndex >= figure["segments"].size())
		return fail(L"PathFigure.Segments 动画索引超出范围。");
	const auto& segment = figure["segments"][segmentIndex];
	if (!segment.is_object()) return fail(L"PathSegment 数据无效。");
	const auto segmentType = segment.value("type", std::string{});
	const auto owner = StoryboardPathLocalType(
		path.Segments[leafStart + 4].OwnerType);
	const auto& property = path.Segments[leafStart + 4].Name;
	auto segmentCanonical = [&](std::wstring_view objectType,
		std::wstring_view leaf)
	{
		return canonicalPrefix + L".(PathGeometry.Figures)["
			+ std::to_wstring(figureIndex)
			+ L"].(PathFigure.Segments)["
			+ std::to_wstring(segmentIndex) + L"].("
			+ std::wstring(objectType) + L"." + std::wstring(leaf) + L")";
	};
	auto point = [&](std::wstring_view objectType, std::wstring_view leaf,
		const char* xKey, const char* yKey)
	{
		if (animationKind != DesignerAnimationKind::Point)
			return fail(L"PathSegment Point 成员只接受 PointAnimation。");
		if (!finiteNumber(segment, xKey) || !finiteNumber(segment, yKey))
			return fail(L"动画目标的 PathSegment Point 无效。");
		return assign(StoryboardObjectPathKind::PathGeometryPoint,
			segmentCanonical(objectType, leaf), objectType, leaf, segmentIndex);
	};
	if (segmentType == "line" && StoryboardPathEquals(owner, L"LineSegment")
		&& StoryboardPathEquals(property, L"Point"))
		return point(L"LineSegment", L"Point", "x", "y");
	if (segmentType == "bezier" && StoryboardPathEquals(owner, L"BezierSegment"))
	{
		if (StoryboardPathEquals(property, L"Point1"))
			return point(L"BezierSegment", L"Point1", "x1", "y1");
		if (StoryboardPathEquals(property, L"Point2"))
			return point(L"BezierSegment", L"Point2", "x2", "y2");
		if (StoryboardPathEquals(property, L"Point3"))
			return point(L"BezierSegment", L"Point3", "x3", "y3");
	}
	if (segmentType == "quadratic"
		&& StoryboardPathEquals(owner, L"QuadraticBezierSegment"))
	{
		if (StoryboardPathEquals(property, L"Point1"))
			return point(L"QuadraticBezierSegment", L"Point1", "x1", "y1");
		if (StoryboardPathEquals(property, L"Point2"))
			return point(L"QuadraticBezierSegment", L"Point2", "x2", "y2");
	}
	if (segmentType == "arc" && StoryboardPathEquals(owner, L"ArcSegment"))
	{
		if (StoryboardPathEquals(property, L"Point"))
			return point(L"ArcSegment", L"Point", "x", "y");
		if (StoryboardPathEquals(property, L"Size"))
		{
			if (animationKind != DesignerAnimationKind::Size)
				return fail(L"ArcSegment.Size 只接受 SizeAnimation。");
			if (!finiteNumber(segment, "width") || !finiteNumber(segment, "height")
				|| segment["width"].get<double>() < 0.0
				|| segment["height"].get<double>() < 0.0)
				return fail(L"动画目标的 ArcSegment.Size 无效。");
			return assign(StoryboardObjectPathKind::PathGeometrySize,
				segmentCanonical(L"ArcSegment", L"Size"),
				L"ArcSegment", L"Size", segmentIndex);
		}
		if (StoryboardPathEquals(property, L"RotationAngle"))
		{
			if (animationKind != DesignerAnimationKind::Double)
				return fail(L"ArcSegment.RotationAngle 只接受 DoubleAnimation。");
			if (!finiteNumber(segment, "rotation"))
				return fail(L"动画目标的 ArcSegment.RotationAngle 无效。");
			return assign(StoryboardObjectPathKind::PathGeometryDouble,
				segmentCanonical(L"ArcSegment", L"RotationAngle"),
				L"ArcSegment", L"RotationAngle", segmentIndex);
		}
		if (StoryboardPathEquals(property, L"IsLargeArc"))
		{
			if (animationKind != DesignerAnimationKind::Object)
				return fail(L"ArcSegment.IsLargeArc 只接受 ObjectAnimationUsingKeyFrames。");
			if (!segment.contains("large") || !segment["large"].is_boolean())
				return fail(L"动画目标的 ArcSegment.IsLargeArc 无效。");
			return assign(StoryboardObjectPathKind::PathGeometryBool,
				segmentCanonical(L"ArcSegment", L"IsLargeArc"),
				L"ArcSegment", L"IsLargeArc", segmentIndex);
		}
		if (StoryboardPathEquals(property, L"SweepDirection"))
		{
			if (animationKind != DesignerAnimationKind::Object)
				return fail(L"ArcSegment.SweepDirection 只接受 ObjectAnimationUsingKeyFrames。");
			const auto sweep = segment.value("sweep", std::string{});
			if (sweep != "clockwise" && sweep != "counterclockwise")
				return fail(L"动画目标的 ArcSegment.SweepDirection 无效。");
			return assign(StoryboardObjectPathKind::PathGeometrySweep,
				segmentCanonical(L"ArcSegment", L"SweepDirection"),
				L"ArcSegment", L"SweepDirection", segmentIndex);
		}
	}
	return fail(L"PathSegment 动画路径所有者或末端属性与实际段类型不匹配。");
}

inline bool TryResolveGeometryTransformAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	const DesignValue* resolvedGeometry = nullptr;
	std::vector<size_t> childIndices;
	size_t leafStart = 0;
	std::wstring canonicalPrefix;
	if (!TryResolveDesignGeometryTreeTarget(component, targetName, path,
		resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
		return false;
	const auto geometryOwner = StoryboardPathLocalType(
		path.Segments[leafStart].OwnerType);
	if (path.Segments.size() != leafStart + 4
		|| path.Segments[leafStart].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[leafStart + 1].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[leafStart + 2].Kind
			!= cui::xaml::PropertyPathSegmentKind::Index
		|| path.Segments[leafStart + 3].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| (!StoryboardPathEquals(geometryOwner, L"Geometry")
			&& !StoryboardPathEquals(geometryOwner, L"RectangleGeometry")
			&& !StoryboardPathEquals(geometryOwner, L"EllipseGeometry")
			&& !StoryboardPathEquals(geometryOwner, L"PathGeometry")
			&& !StoryboardPathEquals(geometryOwner, L"GeometryGroup"))
		|| !StoryboardPathEquals(path.Segments[leafStart].Name, L"Transform")
		|| !StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[leafStart + 1].OwnerType),
			L"TransformGroup")
		|| !StoryboardPathEquals(path.Segments[leafStart + 1].Name, L"Children"))
		return fail(L"Geometry Transform 动画路径必须是 "
			L"(Control.Clip)...(Geometry.Transform)."
			L"(TransformGroup.Children)[n].(TransformType.Property)。");
	const auto& geometry = *resolvedGeometry;
	const auto type = geometry.value("type", std::string{});
	const bool validOwner = StoryboardPathEquals(geometryOwner, L"Geometry")
		|| (type == "rectangle"
			&& StoryboardPathEquals(geometryOwner, L"RectangleGeometry"))
		|| (type == "ellipse"
			&& StoryboardPathEquals(geometryOwner, L"EllipseGeometry"))
		|| (type == "path"
			&& StoryboardPathEquals(geometryOwner, L"PathGeometry"))
		|| (type == "group"
			&& StoryboardPathEquals(geometryOwner, L"GeometryGroup"));
	if (!validOwner)
		return fail(L"Geometry.Transform 路径所有者与实际 Geometry 类型不匹配。");
	if (!geometry.contains("transform") || !geometry["transform"].is_array())
		return fail(L"动画目标没有路径所需的 Geometry.Transform。");

	ResolvedTransformAnimationPath leaf;
	if (!TryResolveTransformOperationLeaf(
		geometry["transform"], path.Segments[leafStart + 2].Index,
		StoryboardPathLocalType(path.Segments[leafStart + 3].OwnerType),
		path.Segments[leafStart + 3].Name, leaf, outError)) return false;
	const bool matrix = StoryboardPathEquals(leaf.TransformType, L"MatrixTransform")
		&& StoryboardPathEquals(leaf.PropertyName, L"Matrix");
	if (animationKind != (matrix ? DesignerAnimationKind::Matrix
		: DesignerAnimationKind::Double))
		return fail(L"Geometry.Transform 数值末端需要 DoubleAnimation，"
			L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
	output = {};
	output.Kind = matrix ? StoryboardObjectPathKind::GeometryTransformMatrix
		: StoryboardObjectPathKind::GeometryTransform;
	output.RootProperty = L"Clip";
	output.GeometryChildIndices = std::move(childIndices);
	output.OperationIndex = leaf.OperationIndex;
	output.ObjectType = std::move(leaf.TransformType);
	output.LeafProperty = std::move(leaf.PropertyName);
	output.CanonicalPath = canonicalPrefix + L".(Geometry.Transform)."
		L"(TransformGroup.Children)[" + std::to_wstring(output.OperationIndex)
		+ L"].(" + output.ObjectType + L"." + output.LeafProperty + L")";
	if (outError) outError->clear();
	return true;
}

inline bool IsGeometryAnimationPath(const std::wstring& value) noexcept
{
	constexpr std::wstring_view controlPrefix = L"(Control.Clip)";
	constexpr std::wstring_view elementPrefix = L"(UIElement.Clip)";
	return value.starts_with(controlPrefix)
		|| value.starts_with(elementPrefix);
}

inline bool TryGetBrushAnimationRoot(
	const cui::xaml::PropertyPath& path,
	std::wstring& rootProperty)
{
	if (path.Segments.size() < 2
		|| path.Segments[0].Kind != cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[1].Kind != cui::xaml::PropertyPathSegmentKind::Property
		|| (!StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[0].OwnerType), L"Control")
			&& !StoryboardPathEquals(
				StoryboardPathLocalType(path.Segments[0].OwnerType), L"UIElement")))
		return false;
	const auto owner = StoryboardPathLocalType(path.Segments[1].OwnerType);
	if (!StoryboardPathEquals(owner, L"Brush")
		&& !StoryboardPathEquals(owner, L"SolidColorBrush")
		&& !StoryboardPathEquals(owner, L"GradientBrush")
		&& !StoryboardPathEquals(owner, L"LinearGradientBrush")
		&& !StoryboardPathEquals(owner, L"RadialGradientBrush")
		&& !StoryboardPathEquals(owner, L"ImageBrush"))
		return false;
	rootProperty = path.Segments[0].Name;
	return !rootProperty.empty();
}

inline bool IsBrushAnimationPath(const std::wstring& value)
{
	cui::xaml::PropertyPath path;
	std::wstring ignored;
	std::wstring rootProperty;
	return cui::xaml::TryParsePropertyPath(value, path, &ignored)
		&& TryGetBrushAnimationRoot(path, rootProperty);
}

inline bool TryResolveBrushPropertyAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	std::wstring rootProperty;
	if (path.Segments.size() != 2
		|| !TryGetBrushAnimationRoot(path, rootProperty))
		return fail(L"Brush 复合属性路径必须是 "
			L"(Control.BrushProperty).(BrushType.Property)。");
	const auto owner = StoryboardPathLocalType(path.Segments[1].OwnerType);
	const DesignValue* brush = nullptr;
	std::string type;
	if (targetName.empty())
	{
		if (StoryboardPathEquals(owner, L"SolidColorBrush")) type = "solid";
		else if (StoryboardPathEquals(owner, L"LinearGradientBrush")) type = "linear";
		else if (StoryboardPathEquals(owner, L"RadialGradientBrush")) type = "radial";
		else if (StoryboardPathEquals(owner, L"ImageBrush")) type = "image";
	}
	else
	{
		const auto target = std::find_if(
			component.Template.begin(), component.Template.end(),
			[&](const auto& node)
			{ return StoryboardPathEquals(node.Name, targetName); });
		brush = target == component.Template.end()
			? nullptr : StoryboardMetadataObject(*target, rootProperty.c_str());
		if (!brush || !brush->is_object())
		{
			// Foreground has an effective system text brush even when XAML does
			// not contribute a local value. WPF therefore permits animating its
			// SolidColorBrush.Color without redundantly authoring Foreground.
			if (StoryboardPathEquals(rootProperty, L"Foreground")
				&& (StoryboardPathEquals(owner, L"Brush")
					|| StoryboardPathEquals(owner, L"SolidColorBrush")))
				type = "solid";
			else
				return fail(L"动画目标必须显式声明 Control."
					+ rootProperty + L" 画刷。");
		}
		else
			type = brush->value("type", std::string{});
		const bool validOwner = StoryboardPathEquals(owner, L"Brush")
			|| (type == "solid" && StoryboardPathEquals(owner, L"SolidColorBrush"))
			|| ((type == "linear" || type == "radial")
				&& StoryboardPathEquals(owner, L"GradientBrush"))
			|| (type == "linear" && StoryboardPathEquals(owner, L"LinearGradientBrush"))
			|| (type == "radial" && StoryboardPathEquals(owner, L"RadialGradientBrush"))
			|| (type == "image" && StoryboardPathEquals(owner, L"ImageBrush"));
		if (!validOwner)
			return fail(L"Brush 动画路径所有者与实际画刷类型不匹配。");
	}

	auto finiteNumber = [&](const char* key)
	{
		return brush && brush->contains(key) && (*brush)[key].is_number()
			&& std::isfinite((*brush)[key].get<double>());
	};
	auto finitePoint = [&](const char* x, const char* y)
	{
		return finiteNumber(x) && finiteNumber(y);
	};
	auto finiteColor = [&](const DesignValue& color)
	{
		if (!color.is_object()) return false;
		for (const char* key : { "r", "g", "b", "a" })
			if (!color.contains(key) || !color[key].is_number()
				|| !std::isfinite(color[key].get<double>())) return false;
		return true;
	};
	const auto& property = path.Segments[1].Name;
	output = {};
	output.RootProperty = rootProperty;
	if (StoryboardPathEquals(property, L"Opacity"))
	{
		if (animationKind != DesignerAnimationKind::Double)
			return fail(L"Brush.Opacity 只接受 DoubleAnimation。");
		if (brush && (!finiteNumber("opacity")
			|| (*brush)["opacity"].get<double>() < 0.0
			|| (*brush)["opacity"].get<double>() > 1.0))
			return fail(L"动画目标的 Brush.Opacity 无效。");
		output.Kind = StoryboardObjectPathKind::BrushOpacity;
		output.ObjectType = L"Brush";
		output.LeafProperty = L"Opacity";
	}
	else if (StoryboardPathEquals(property, L"Color")
		&& type == "solid" && StoryboardPathEquals(owner, L"SolidColorBrush"))
	{
		if (animationKind != DesignerAnimationKind::Color)
			return fail(L"SolidColorBrush.Color 只接受 ColorAnimation。");
		if (brush && (!brush->contains("color")
			|| !finiteColor((*brush)["color"])))
			return fail(L"动画目标的 SolidColorBrush.Color 无效。");
		output.Kind = StoryboardObjectPathKind::SolidColorBrushColor;
		output.ObjectType = L"SolidColorBrush";
		output.LeafProperty = L"Color";
	}
	else if (type == "linear" && StoryboardPathEquals(owner, L"LinearGradientBrush")
		&& (StoryboardPathEquals(property, L"StartPoint")
			|| StoryboardPathEquals(property, L"EndPoint")))
	{
		if (animationKind != DesignerAnimationKind::Point)
			return fail(L"LinearGradientBrush 点属性只接受 PointAnimation。");
		const bool start = StoryboardPathEquals(property, L"StartPoint");
		if (brush && !finitePoint(
			start ? "startX" : "endX", start ? "startY" : "endY"))
			return fail(L"动画目标的 LinearGradientBrush 点属性无效。");
		output.Kind = StoryboardObjectPathKind::GradientBrushPoint;
		output.ObjectType = L"LinearGradientBrush";
		output.LeafProperty = start ? L"StartPoint" : L"EndPoint";
	}
	else if (type == "radial" && StoryboardPathEquals(owner, L"RadialGradientBrush")
		&& (StoryboardPathEquals(property, L"Center")
			|| StoryboardPathEquals(property, L"GradientOrigin")))
	{
		if (animationKind != DesignerAnimationKind::Point)
			return fail(L"RadialGradientBrush 点属性只接受 PointAnimation。");
		const bool center = StoryboardPathEquals(property, L"Center");
		if (brush && !finitePoint(center ? "centerX" : "originX",
			center ? "centerY" : "originY"))
			return fail(L"动画目标的 RadialGradientBrush 点属性无效。");
		output.Kind = StoryboardObjectPathKind::GradientBrushPoint;
		output.ObjectType = L"RadialGradientBrush";
		output.LeafProperty = center ? L"Center" : L"GradientOrigin";
	}
	else if (type == "radial" && StoryboardPathEquals(owner, L"RadialGradientBrush")
		&& (StoryboardPathEquals(property, L"RadiusX")
			|| StoryboardPathEquals(property, L"RadiusY")))
	{
		if (animationKind != DesignerAnimationKind::Double)
			return fail(L"RadialGradientBrush 半径只接受 DoubleAnimation。");
		const bool x = StoryboardPathEquals(property, L"RadiusX");
		const char* key = x ? "radiusX" : "radiusY";
		if (brush && (!finiteNumber(key) || (*brush)[key].get<double>() < 0.0))
			return fail(L"动画目标的 RadialGradientBrush 半径无效。");
		output.Kind = StoryboardObjectPathKind::RadialGradientBrushRadius;
		output.ObjectType = L"RadialGradientBrush";
		output.LeafProperty = x ? L"RadiusX" : L"RadiusY";
	}
	else return fail(L"Brush 动画路径末端属性与实际画刷类型不匹配。");

	output.CanonicalPath = L"(Control." + rootProperty + L").("
		+ output.ObjectType + L"." + output.LeafProperty + L")";
	if (outError) outError->clear();
	return true;
}

inline bool TryResolveGradientStopAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	std::wstring rootProperty;
	const auto gradientOwner = path.Segments.size() > 1
		? StoryboardPathLocalType(path.Segments[1].OwnerType) : std::wstring{};
	if (path.Segments.size() != 4
		|| path.Segments[0].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[1].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[2].Kind
			!= cui::xaml::PropertyPathSegmentKind::Index
		|| path.Segments[3].Kind
			!= cui::xaml::PropertyPathSegmentKind::Property
		|| !TryGetBrushAnimationRoot(path, rootProperty)
		|| (!StoryboardPathEquals(gradientOwner, L"GradientBrush")
			&& !StoryboardPathEquals(gradientOwner, L"LinearGradientBrush")
			&& !StoryboardPathEquals(gradientOwner, L"RadialGradientBrush"))
		|| !StoryboardPathEquals(path.Segments[1].Name, L"GradientStops")
		|| !StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[3].OwnerType), L"GradientStop")
		|| (!StoryboardPathEquals(path.Segments[3].Name, L"Color")
			&& !StoryboardPathEquals(path.Segments[3].Name, L"Offset")))
		return fail(L"GradientStop 复合动画路径必须是 "
			L"(Control.BrushProperty).(GradientBrush.GradientStops)[n]."
			L"(GradientStop.Color|Offset)。");

	const bool color = StoryboardPathEquals(path.Segments[3].Name, L"Color");
	if ((color && animationKind != DesignerAnimationKind::Color)
		|| (!color && animationKind != DesignerAnimationKind::Double))
		return fail(L"GradientStop.Color 需要 ColorAnimation，"
			L"GradientStop.Offset 需要 DoubleAnimation。");
	if (!targetName.empty())
	{
		const auto target = std::find_if(
			component.Template.begin(), component.Template.end(),
			[&](const auto& node)
			{ return StoryboardPathEquals(node.Name, targetName); });
		const auto* brushValue = target == component.Template.end()
			? nullptr : StoryboardMetadataObject(*target, rootProperty.c_str());
		if (!brushValue || !brushValue->is_object())
			return fail(L"动画目标必须显式声明渐变 Control."
				+ rootProperty + L"。");
		const auto& brush = *brushValue;
		const auto type = brush.value("type", std::string{});
		const bool validOwner = StoryboardPathEquals(gradientOwner, L"GradientBrush")
			|| (type == "linear"
				&& StoryboardPathEquals(gradientOwner, L"LinearGradientBrush"))
			|| (type == "radial"
				&& StoryboardPathEquals(gradientOwner, L"RadialGradientBrush"));
		if (!validOwner || (type != "linear" && type != "radial")
			|| !brush.contains("stops") || !brush["stops"].is_array()
			|| path.Segments[2].Index >= brush["stops"].size())
			return fail(L"动画目标没有路径所需的渐变 GradientStop。");
		const auto& stop = brush["stops"][path.Segments[2].Index];
		if (!stop.is_object() || !stop.contains("offset")
			|| !stop["offset"].is_number() || !stop.contains("color")
			|| !stop["color"].is_object())
			return fail(L"动画目标的 GradientStop 格式无效。");
		const auto offset = stop["offset"].get<double>();
		const auto& stopColor = stop["color"];
		auto finiteColorPart = [&](const char* name)
		{
			return stopColor.contains(name) && stopColor[name].is_number()
				&& std::isfinite(stopColor[name].get<double>());
		};
		if (!std::isfinite(offset) || offset < 0.0 || offset > 1.0
			|| !finiteColorPart("r") || !finiteColorPart("g")
			|| !finiteColorPart("b") || !finiteColorPart("a"))
			return fail(L"动画目标的 GradientStop 值无效。");
	}

	output = {};
	output.Kind = color ? StoryboardObjectPathKind::GradientStopColor
		: StoryboardObjectPathKind::GradientStopOffset;
	output.RootProperty = rootProperty;
	output.CollectionIndex = path.Segments[2].Index;
	output.ObjectType = L"GradientStop";
	output.LeafProperty = color ? L"Color" : L"Offset";
	output.CanonicalPath = L"(Control." + rootProperty + L")."
		L"(GradientBrush.GradientStops)["
		+ std::to_wstring(output.CollectionIndex) + L"].(GradientStop."
		+ output.LeafProperty + L")";
	if (outError) outError->clear();
	return true;
}

inline bool TryResolveBrushTransformAnimationPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	if (targetName.empty())
		return fail(L"Brush Transform 复合属性路径只能定位模板具名控件。");
	cui::xaml::PropertyPath path;
	std::wstring parseError;
	if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
		return fail(L"Storyboard.TargetProperty：" + parseError);
	std::wstring rootProperty;
	const auto brushOwner = path.Segments.size() > 1
		? StoryboardPathLocalType(path.Segments[1].OwnerType) : std::wstring{};
	if (path.Segments.size() != 5
		|| path.Segments[0].Kind != cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[1].Kind != cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[2].Kind != cui::xaml::PropertyPathSegmentKind::Property
		|| path.Segments[3].Kind != cui::xaml::PropertyPathSegmentKind::Index
		|| path.Segments[4].Kind != cui::xaml::PropertyPathSegmentKind::Property
		|| !TryGetBrushAnimationRoot(path, rootProperty)
		|| (!StoryboardPathEquals(brushOwner, L"Brush")
			&& !StoryboardPathEquals(brushOwner, L"SolidColorBrush")
			&& !StoryboardPathEquals(brushOwner, L"GradientBrush")
			&& !StoryboardPathEquals(brushOwner, L"LinearGradientBrush")
			&& !StoryboardPathEquals(brushOwner, L"RadialGradientBrush")
			&& !StoryboardPathEquals(brushOwner, L"ImageBrush"))
		|| (!StoryboardPathEquals(path.Segments[1].Name, L"Transform")
			&& !StoryboardPathEquals(path.Segments[1].Name, L"RelativeTransform"))
		|| !StoryboardPathEquals(
			StoryboardPathLocalType(path.Segments[2].OwnerType), L"TransformGroup")
		|| !StoryboardPathEquals(path.Segments[2].Name, L"Children"))
		return fail(L"Brush Transform 动画路径必须是 "
			L"(Control.BrushProperty).(Brush.Transform|RelativeTransform)."
			L"(TransformGroup.Children)[n].(TransformType.Property)。");

	const auto target = std::find_if(
		component.Template.begin(), component.Template.end(),
		[&](const auto& node)
		{ return StoryboardPathEquals(node.Name, targetName); });
	const auto* brushValue = target == component.Template.end()
		? nullptr : StoryboardMetadataObject(*target, rootProperty.c_str());
	if (!brushValue || !brushValue->is_object())
		return fail(L"动画目标必须显式声明包含变换的 Control."
			+ rootProperty + L"。");
	const auto& brush = *brushValue;
	const auto type = brush.value("type", std::string{});
	const bool validOwner = StoryboardPathEquals(brushOwner, L"Brush")
		|| (type == "solid" && StoryboardPathEquals(brushOwner, L"SolidColorBrush"))
		|| ((type == "linear" || type == "radial")
			&& StoryboardPathEquals(brushOwner, L"GradientBrush"))
		|| (type == "linear" && StoryboardPathEquals(brushOwner, L"LinearGradientBrush"))
		|| (type == "radial" && StoryboardPathEquals(brushOwner, L"RadialGradientBrush"))
		|| (type == "image" && StoryboardPathEquals(brushOwner, L"ImageBrush"));
	if (!validOwner)
		return fail(L"Brush Transform 路径所有者与实际画刷类型不匹配。");
	const bool relative = StoryboardPathEquals(
		path.Segments[1].Name, L"RelativeTransform");
	const char* transformKey = relative ? "relativeTransform" : "transform";
	if (!brush.contains(transformKey) || !brush[transformKey].is_array())
		return fail(L"动画目标没有路径所需的 Brush Transform。");

	ResolvedTransformAnimationPath leaf;
	if (!TryResolveTransformOperationLeaf(
		brush[transformKey], path.Segments[3].Index,
		StoryboardPathLocalType(path.Segments[4].OwnerType),
		path.Segments[4].Name, leaf, outError)) return false;
	const bool matrix = StoryboardPathEquals(leaf.TransformType, L"MatrixTransform")
		&& StoryboardPathEquals(leaf.PropertyName, L"Matrix");
	if (animationKind != (matrix ? DesignerAnimationKind::Matrix
		: DesignerAnimationKind::Double))
		return fail(L"Brush Transform 数值末端需要 DoubleAnimation，"
			L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
	output = {};
	output.Kind = relative
		? (matrix ? StoryboardObjectPathKind::BrushRelativeTransformMatrix
			: StoryboardObjectPathKind::BrushRelativeTransform)
		: (matrix ? StoryboardObjectPathKind::BrushTransformMatrix
			: StoryboardObjectPathKind::BrushTransform);
	output.RootProperty = rootProperty;
	output.OperationIndex = leaf.OperationIndex;
	output.ObjectType = std::move(leaf.TransformType);
	output.LeafProperty = std::move(leaf.PropertyName);
	output.CanonicalPath = L"(Control." + rootProperty + L").(Brush."
		+ std::wstring(relative ? L"RelativeTransform" : L"Transform")
		+ L").(TransformGroup.Children)["
		+ std::to_wstring(output.OperationIndex) + L"].("
		+ output.ObjectType + L"." + output.LeafProperty + L")";
	if (outError) outError->clear();
	return true;
}

inline StoryboardObjectPathKind ClassifyStoryboardObjectPath(
	const std::wstring& value)
{
	if (IsTransformAnimationPath(value))
	{
		cui::xaml::PropertyPath path;
		std::wstring ignored;
		return cui::xaml::TryParsePropertyPath(value, path, &ignored)
			&& IsMatrixTransformLeaf(path)
			? StoryboardObjectPathKind::RenderTransformMatrix
			: StoryboardObjectPathKind::RenderTransform;
	}
	if (IsGeometryAnimationPath(value))
	{
		cui::xaml::PropertyPath path;
		std::wstring ignored;
		if (!cui::xaml::TryParsePropertyPath(value, path, &ignored)
			|| path.Segments.size() < 2) return StoryboardObjectPathKind::None;
		size_t leafStart = 1;
		while (leafStart + 1 < path.Segments.size()
			&& path.Segments[leafStart].Kind
				== cui::xaml::PropertyPathSegmentKind::Property
			&& StoryboardPathEquals(
				StoryboardPathLocalType(path.Segments[leafStart].OwnerType),
				L"GeometryGroup")
			&& StoryboardPathEquals(path.Segments[leafStart].Name, L"Children")
			&& path.Segments[leafStart + 1].Kind
				== cui::xaml::PropertyPathSegmentKind::Index)
			leafStart += 2;
		if (leafStart >= path.Segments.size())
			return StoryboardObjectPathKind::None;
		if (StoryboardPathEquals(path.Segments[leafStart].Name, L"Transform"))
			return IsMatrixTransformLeaf(path)
				? StoryboardObjectPathKind::GeometryTransformMatrix
				: StoryboardObjectPathKind::GeometryTransform;
		if (StoryboardPathEquals(path.Segments[leafStart].Name, L"Figures"))
		{
			if (path.Segments.size() == leafStart + 3)
			{
				const auto owner = StoryboardPathLocalType(
					path.Segments[leafStart + 2].OwnerType);
				const auto& property = path.Segments[leafStart + 2].Name;
				if (!StoryboardPathEquals(owner, L"PathFigure"))
					return StoryboardObjectPathKind::None;
				if (StoryboardPathEquals(property, L"StartPoint"))
					return StoryboardObjectPathKind::PathGeometryPoint;
				if (StoryboardPathEquals(property, L"IsClosed")
					|| StoryboardPathEquals(property, L"IsFilled"))
					return StoryboardObjectPathKind::PathGeometryBool;
				return StoryboardObjectPathKind::None;
			}
			if (path.Segments.size() != leafStart + 5)
				return StoryboardObjectPathKind::None;
			const auto owner = StoryboardPathLocalType(
				path.Segments[leafStart + 4].OwnerType);
			const auto& property = path.Segments[leafStart + 4].Name;
			const bool point = (StoryboardPathEquals(owner, L"LineSegment")
				&& StoryboardPathEquals(property, L"Point"))
				|| (StoryboardPathEquals(owner, L"BezierSegment")
					&& (StoryboardPathEquals(property, L"Point1")
						|| StoryboardPathEquals(property, L"Point2")
						|| StoryboardPathEquals(property, L"Point3")))
				|| (StoryboardPathEquals(owner, L"QuadraticBezierSegment")
					&& (StoryboardPathEquals(property, L"Point1")
						|| StoryboardPathEquals(property, L"Point2")))
				|| (StoryboardPathEquals(owner, L"ArcSegment")
					&& StoryboardPathEquals(property, L"Point"));
			if (point) return StoryboardObjectPathKind::PathGeometryPoint;
			if (StoryboardPathEquals(owner, L"ArcSegment")
				&& StoryboardPathEquals(property, L"Size"))
				return StoryboardObjectPathKind::PathGeometrySize;
			if (StoryboardPathEquals(owner, L"ArcSegment")
				&& StoryboardPathEquals(property, L"RotationAngle"))
				return StoryboardObjectPathKind::PathGeometryDouble;
			if (StoryboardPathEquals(owner, L"ArcSegment")
				&& StoryboardPathEquals(property, L"IsLargeArc"))
				return StoryboardObjectPathKind::PathGeometryBool;
			if (StoryboardPathEquals(owner, L"ArcSegment")
				&& StoryboardPathEquals(property, L"SweepDirection"))
				return StoryboardObjectPathKind::PathGeometrySweep;
			return StoryboardObjectPathKind::None;
		}
		if (path.Segments.size() != leafStart + 1)
			return StoryboardObjectPathKind::None;
		const auto owner = StoryboardPathLocalType(
			path.Segments[leafStart].OwnerType);
		const auto& property = path.Segments[leafStart].Name;
		if (StoryboardPathEquals(owner, L"RectangleGeometry")
			&& StoryboardPathEquals(property, L"Rect"))
			return StoryboardObjectPathKind::RectangleGeometryRect;
		if (StoryboardPathEquals(owner, L"RectangleGeometry")
			&& (StoryboardPathEquals(property, L"RadiusX")
				|| StoryboardPathEquals(property, L"RadiusY")))
			return StoryboardObjectPathKind::RectangleGeometryRadius;
		if (StoryboardPathEquals(owner, L"EllipseGeometry")
			&& StoryboardPathEquals(property, L"Center"))
			return StoryboardObjectPathKind::EllipseGeometryCenter;
		if (StoryboardPathEquals(owner, L"EllipseGeometry")
			&& (StoryboardPathEquals(property, L"RadiusX")
				|| StoryboardPathEquals(property, L"RadiusY")))
			return StoryboardObjectPathKind::EllipseGeometryRadius;
		if ((StoryboardPathEquals(owner, L"PathGeometry")
				|| StoryboardPathEquals(owner, L"GeometryGroup"))
			&& StoryboardPathEquals(property, L"FillRule"))
			return StoryboardObjectPathKind::GeometryFillRule;
		return StoryboardObjectPathKind::None;
	}
	if (IsBrushAnimationPath(value))
	{
		cui::xaml::PropertyPath path;
		std::wstring ignored;
		if (!cui::xaml::TryParsePropertyPath(value, path, &ignored)
			|| path.Segments.size() < 2) return StoryboardObjectPathKind::None;
		if (StoryboardPathEquals(path.Segments[1].Name, L"Transform"))
			return IsMatrixTransformLeaf(path)
				? StoryboardObjectPathKind::BrushTransformMatrix
				: StoryboardObjectPathKind::BrushTransform;
		if (StoryboardPathEquals(path.Segments[1].Name, L"RelativeTransform"))
			return IsMatrixTransformLeaf(path)
				? StoryboardObjectPathKind::BrushRelativeTransformMatrix
				: StoryboardObjectPathKind::BrushRelativeTransform;
		if (path.Segments.size() == 2)
		{
			const auto& property = path.Segments[1].Name;
			if (StoryboardPathEquals(property, L"Color"))
				return StoryboardObjectPathKind::SolidColorBrushColor;
			if (StoryboardPathEquals(property, L"Opacity"))
				return StoryboardObjectPathKind::BrushOpacity;
			if (StoryboardPathEquals(property, L"StartPoint")
				|| StoryboardPathEquals(property, L"EndPoint")
				|| StoryboardPathEquals(property, L"Center")
				|| StoryboardPathEquals(property, L"GradientOrigin"))
				return StoryboardObjectPathKind::GradientBrushPoint;
			if (StoryboardPathEquals(property, L"RadiusX")
				|| StoryboardPathEquals(property, L"RadiusY"))
				return StoryboardObjectPathKind::RadialGradientBrushRadius;
		}
		constexpr std::wstring_view colorSuffix = L"(GradientStop.Color)";
		constexpr std::wstring_view offsetSuffix = L"(GradientStop.Offset)";
		if (value.ends_with(colorSuffix))
			return StoryboardObjectPathKind::GradientStopColor;
		if (value.ends_with(offsetSuffix))
			return StoryboardObjectPathKind::GradientStopOffset;
	}
	return StoryboardObjectPathKind::None;
}

inline std::wstring StoryboardAnimationRootProperty(
	const std::wstring& value)
{
	switch (ClassifyStoryboardObjectPath(value))
	{
	case StoryboardObjectPathKind::RenderTransform:
	case StoryboardObjectPathKind::RenderTransformMatrix:
		return L"RenderTransform";
	case StoryboardObjectPathKind::RectangleGeometryRect:
	case StoryboardObjectPathKind::RectangleGeometryRadius:
	case StoryboardObjectPathKind::EllipseGeometryCenter:
	case StoryboardObjectPathKind::EllipseGeometryRadius:
	case StoryboardObjectPathKind::GeometryFillRule:
	case StoryboardObjectPathKind::PathGeometryPoint:
	case StoryboardObjectPathKind::PathGeometrySize:
	case StoryboardObjectPathKind::PathGeometryDouble:
	case StoryboardObjectPathKind::PathGeometryBool:
	case StoryboardObjectPathKind::PathGeometrySweep:
	case StoryboardObjectPathKind::GeometryTransform:
	case StoryboardObjectPathKind::GeometryTransformMatrix:
		return L"Clip";
	case StoryboardObjectPathKind::GradientStopColor:
	case StoryboardObjectPathKind::GradientStopOffset:
	case StoryboardObjectPathKind::SolidColorBrushColor:
	case StoryboardObjectPathKind::BrushOpacity:
	case StoryboardObjectPathKind::GradientBrushPoint:
	case StoryboardObjectPathKind::RadialGradientBrushRadius:
	case StoryboardObjectPathKind::BrushTransform:
	case StoryboardObjectPathKind::BrushTransformMatrix:
	case StoryboardObjectPathKind::BrushRelativeTransform:
	case StoryboardObjectPathKind::BrushRelativeTransformMatrix:
	{
		cui::xaml::PropertyPath path;
		std::wstring ignored;
		std::wstring rootProperty;
		return cui::xaml::TryParsePropertyPath(value, path, &ignored)
			&& TryGetBrushAnimationRoot(path, rootProperty)
			? rootProperty : value;
	}
	case StoryboardObjectPathKind::None:
	default:
		return value;
	}
}

/** Resolves every currently registered object-property adapter. */
inline bool TryResolveStoryboardObjectPath(
	const DesignComponentDefinition& component,
	const std::wstring& targetName,
	const std::wstring& text,
	DesignerAnimationKind animationKind,
	ResolvedStoryboardObjectPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	output = {};
	switch (ClassifyStoryboardObjectPath(text))
	{
	case StoryboardObjectPathKind::RenderTransform:
	case StoryboardObjectPathKind::RenderTransformMatrix:
	{
		ResolvedTransformAnimationPath resolved;
		if (!TryResolveTransformAnimationPath(
			component, targetName, text, resolved, outError)) return false;
		const bool matrix = StoryboardPathEquals(
			resolved.TransformType, L"MatrixTransform")
			&& StoryboardPathEquals(resolved.PropertyName, L"Matrix");
		if (animationKind != (matrix ? DesignerAnimationKind::Matrix
			: DesignerAnimationKind::Double))
			return fail(L"RenderTransform 数值末端需要 DoubleAnimation，"
				L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
		output.Kind = matrix ? StoryboardObjectPathKind::RenderTransformMatrix
			: StoryboardObjectPathKind::RenderTransform;
		output.CanonicalPath = std::move(resolved.CanonicalPath);
		output.RootProperty = L"RenderTransform";
		output.OperationIndex = resolved.OperationIndex;
		output.ObjectType = std::move(resolved.TransformType);
		output.LeafProperty = std::move(resolved.PropertyName);
		return true;
	}
	case StoryboardObjectPathKind::RectangleGeometryRect:
	case StoryboardObjectPathKind::RectangleGeometryRadius:
	case StoryboardObjectPathKind::EllipseGeometryCenter:
	case StoryboardObjectPathKind::EllipseGeometryRadius:
	case StoryboardObjectPathKind::GeometryFillRule:
		return TryResolveGeometryPropertyAnimationPath(
			component, targetName, text, animationKind, output, outError);
	case StoryboardObjectPathKind::PathGeometryPoint:
	case StoryboardObjectPathKind::PathGeometrySize:
	case StoryboardObjectPathKind::PathGeometryDouble:
	case StoryboardObjectPathKind::PathGeometryBool:
	case StoryboardObjectPathKind::PathGeometrySweep:
		return TryResolvePathGeometryAnimationPath(
			component, targetName, text, animationKind, output, outError);
	case StoryboardObjectPathKind::GeometryTransform:
	case StoryboardObjectPathKind::GeometryTransformMatrix:
		return TryResolveGeometryTransformAnimationPath(
			component, targetName, text, animationKind, output, outError);
	case StoryboardObjectPathKind::SolidColorBrushColor:
	case StoryboardObjectPathKind::BrushOpacity:
	case StoryboardObjectPathKind::GradientBrushPoint:
	case StoryboardObjectPathKind::RadialGradientBrushRadius:
		return TryResolveBrushPropertyAnimationPath(
			component, targetName, text, animationKind, output, outError);
	case StoryboardObjectPathKind::GradientStopColor:
	case StoryboardObjectPathKind::GradientStopOffset:
		return TryResolveGradientStopAnimationPath(
			component, targetName, text, animationKind, output, outError);
	case StoryboardObjectPathKind::BrushTransform:
	case StoryboardObjectPathKind::BrushTransformMatrix:
	case StoryboardObjectPathKind::BrushRelativeTransform:
	case StoryboardObjectPathKind::BrushRelativeTransformMatrix:
		return TryResolveBrushTransformAnimationPath(
			component, targetName, text, animationKind, output, outError);
	case StoryboardObjectPathKind::None:
	default:
		return fail(L"尚未注册可处理此 Storyboard.TargetProperty 的对象路径适配器。");
	}
}

inline DesignerStyleValueKind StoryboardObjectPathValueKind(
	StoryboardObjectPathKind kind) noexcept
{
	switch (kind)
	{
	case StoryboardObjectPathKind::RectangleGeometryRect:
		return DesignerStyleValueKind::Rect;
	case StoryboardObjectPathKind::EllipseGeometryCenter:
	case StoryboardObjectPathKind::PathGeometryPoint:
		return DesignerStyleValueKind::Point;
	case StoryboardObjectPathKind::PathGeometrySize:
		return DesignerStyleValueKind::Size;
	case StoryboardObjectPathKind::RenderTransformMatrix:
	case StoryboardObjectPathKind::GeometryTransformMatrix:
	case StoryboardObjectPathKind::BrushTransformMatrix:
	case StoryboardObjectPathKind::BrushRelativeTransformMatrix:
		return DesignerStyleValueKind::Matrix;
	case StoryboardObjectPathKind::PathGeometryBool:
		return DesignerStyleValueKind::Bool;
	case StoryboardObjectPathKind::GeometryFillRule:
	case StoryboardObjectPathKind::PathGeometrySweep:
		return DesignerStyleValueKind::String;
	case StoryboardObjectPathKind::GradientStopColor:
	case StoryboardObjectPathKind::SolidColorBrushColor:
		return DesignerStyleValueKind::Color;
	case StoryboardObjectPathKind::GradientBrushPoint:
		return DesignerStyleValueKind::Point;
	case StoryboardObjectPathKind::RenderTransform:
	case StoryboardObjectPathKind::GeometryTransform:
	case StoryboardObjectPathKind::RectangleGeometryRadius:
	case StoryboardObjectPathKind::EllipseGeometryRadius:
	case StoryboardObjectPathKind::PathGeometryDouble:
	case StoryboardObjectPathKind::GradientStopOffset:
	case StoryboardObjectPathKind::BrushOpacity:
	case StoryboardObjectPathKind::RadialGradientBrushRadius:
	case StoryboardObjectPathKind::BrushTransform:
	case StoryboardObjectPathKind::BrushRelativeTransform:
	case StoryboardObjectPathKind::None:
	default:
		return DesignerStyleValueKind::Float;
	}
}

inline bool ValidateStoryboardObjectPathValue(
	StoryboardObjectPathKind kind,
	const BindingValue& value,
	bool isDelta = false) noexcept
{
	if (kind == StoryboardObjectPathKind::RenderTransformMatrix
		|| kind == StoryboardObjectPathKind::GeometryTransformMatrix
		|| kind == StoryboardObjectPathKind::BrushTransformMatrix
		|| kind == StoryboardObjectPathKind::BrushRelativeTransformMatrix)
	{
		D2D1_MATRIX_3X2_F matrix{};
		return value.TryGet(matrix) && std::isfinite(matrix._11)
			&& std::isfinite(matrix._12) && std::isfinite(matrix._21)
			&& std::isfinite(matrix._22) && std::isfinite(matrix._31)
			&& std::isfinite(matrix._32);
	}
	if (kind == StoryboardObjectPathKind::RenderTransform
		|| kind == StoryboardObjectPathKind::GeometryTransform
		|| kind == StoryboardObjectPathKind::RectangleGeometryRadius
		|| kind == StoryboardObjectPathKind::EllipseGeometryRadius
		|| kind == StoryboardObjectPathKind::PathGeometryDouble
		|| kind == StoryboardObjectPathKind::GradientStopOffset
		|| kind == StoryboardObjectPathKind::BrushOpacity
		|| kind == StoryboardObjectPathKind::RadialGradientBrushRadius
		|| kind == StoryboardObjectPathKind::BrushTransform
		|| kind == StoryboardObjectPathKind::BrushRelativeTransform)
	{
		double number = 0.0;
		if (!value.TryGetDouble(number) || !std::isfinite(number)
			|| number < -(std::numeric_limits<float>::max)()
			|| number > (std::numeric_limits<float>::max)()) return false;
		if (isDelta) return true;
		if (kind == StoryboardObjectPathKind::GradientStopOffset
			|| kind == StoryboardObjectPathKind::BrushOpacity)
			return number >= 0.0 && number <= 1.0;
		return (kind != StoryboardObjectPathKind::RadialGradientBrushRadius
			&& kind != StoryboardObjectPathKind::RectangleGeometryRadius
			&& kind != StoryboardObjectPathKind::EllipseGeometryRadius)
			|| number >= 0.0;
	}
	if (kind == StoryboardObjectPathKind::RectangleGeometryRect)
	{
		cui::core::Rect rect{};
		return value.TryGet(rect) && std::isfinite(rect.x)
			&& std::isfinite(rect.y) && std::isfinite(rect.width)
			&& std::isfinite(rect.height)
			&& rect.width >= 0.0f && rect.height >= 0.0f;
	}
	if (kind == StoryboardObjectPathKind::GradientStopColor
		|| kind == StoryboardObjectPathKind::SolidColorBrushColor)
	{
		D2D1_COLOR_F color{};
		return value.TryGet(color) && std::isfinite(color.r)
			&& std::isfinite(color.g) && std::isfinite(color.b)
			&& std::isfinite(color.a);
	}
	if (kind == StoryboardObjectPathKind::GradientBrushPoint)
	{
		cui::core::Point point{};
		return value.TryGet(point) && std::isfinite(point.x)
			&& std::isfinite(point.y);
	}
	if (kind == StoryboardObjectPathKind::EllipseGeometryCenter)
	{
		cui::core::Point point{};
		return value.TryGet(point) && std::isfinite(point.x)
			&& std::isfinite(point.y);
	}
	if (kind == StoryboardObjectPathKind::PathGeometryPoint)
	{
		cui::core::Point point{};
		return value.TryGet(point) && std::isfinite(point.x)
			&& std::isfinite(point.y);
	}
	if (kind == StoryboardObjectPathKind::PathGeometrySize)
	{
		cui::core::Size size{};
		return value.TryGet(size) && std::isfinite(size.width)
			&& std::isfinite(size.height)
			&& (isDelta || (size.width >= 0.0f && size.height >= 0.0f));
	}
	if (kind == StoryboardObjectPathKind::PathGeometryBool)
	{
		bool flag = false;
		return !isDelta && value.TryGet(flag);
	}
	if (kind == StoryboardObjectPathKind::PathGeometrySweep)
	{
		std::wstring sweep;
		return !isDelta && value.TryGet(sweep)
			&& (StoryboardPathEquals(sweep, L"Clockwise")
				|| StoryboardPathEquals(sweep, L"Counterclockwise"));
	}
	if (kind == StoryboardObjectPathKind::GeometryFillRule)
	{
		std::wstring fillRule;
		return !isDelta && value.TryGet(fillRule)
			&& (StoryboardPathEquals(fillRule, L"EvenOdd")
				|| StoryboardPathEquals(fillRule, L"Nonzero"));
	}
	return false;
}
}
