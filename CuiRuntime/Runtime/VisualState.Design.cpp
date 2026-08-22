#include "Control.h"
#include "PropertyPath.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#if !CUI_ENABLE_DYNAMIC_XAML
#error VisualState.Design.cpp requires the Design runtime flavor.
#endif

namespace
{
	using PathSegment = cui::xaml::PropertyPathSegment;
	using PathSegmentKind = cui::xaml::PropertyPathSegmentKind;

	std::wstring_view LocalTypeName(std::wstring_view value) noexcept
	{
		const auto separator = value.rfind(L':');
		return separator == std::wstring_view::npos
			? value : value.substr(separator + 1);
	}

	bool TransformKind(
		std::wstring_view owner, uint8_t& value) noexcept
	{
		owner = LocalTypeName(owner);
		if (owner == L"MatrixTransform")
			value = static_cast<uint8_t>(cui::drawing::TransformKind::Matrix);
		else if (owner == L"TranslateTransform")
			value = static_cast<uint8_t>(cui::drawing::TransformKind::Translate);
		else if (owner == L"ScaleTransform")
			value = static_cast<uint8_t>(cui::drawing::TransformKind::Scale);
		else if (owner == L"RotateTransform")
			value = static_cast<uint8_t>(cui::drawing::TransformKind::Rotate);
		else if (owner == L"SkewTransform")
			value = static_cast<uint8_t>(cui::drawing::TransformKind::Skew);
		else return false;
		return true;
	}

	bool TransformMember(
		std::wstring_view name,
		CompiledStoryboardObjectPathMember& value) noexcept
	{
		if (name == L"X") value = CompiledStoryboardObjectPathMember::TransformX;
		else if (name == L"Y") value = CompiledStoryboardObjectPathMember::TransformY;
		else if (name == L"ScaleX") value = CompiledStoryboardObjectPathMember::TransformScaleX;
		else if (name == L"ScaleY") value = CompiledStoryboardObjectPathMember::TransformScaleY;
		else if (name == L"Angle") value = CompiledStoryboardObjectPathMember::TransformAngle;
		else if (name == L"AngleX") value = CompiledStoryboardObjectPathMember::TransformAngleX;
		else if (name == L"AngleY") value = CompiledStoryboardObjectPathMember::TransformAngleY;
		else if (name == L"CenterX") value = CompiledStoryboardObjectPathMember::TransformCenterX;
		else if (name == L"CenterY") value = CompiledStoryboardObjectPathMember::TransformCenterY;
		else if (name == L"Matrix") value = CompiledStoryboardObjectPathMember::TransformMatrix;
		else return false;
		return true;
	}

	bool GeometryKind(
		std::wstring_view owner, uint8_t& value) noexcept
	{
		owner = LocalTypeName(owner);
		if (owner == L"RectangleGeometry")
			value = static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle);
		else if (owner == L"EllipseGeometry")
			value = static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse);
		else if (owner == L"PathGeometry")
			value = static_cast<uint8_t>(cui::drawing::GeometryKind::Path);
		else if (owner == L"GeometryGroup")
			value = static_cast<uint8_t>(cui::drawing::GeometryKind::Group);
		else if (owner == L"Geometry")
			value = (std::numeric_limits<uint8_t>::max)();
		else return false;
		return true;
	}

	bool GeometryMember(
		std::wstring_view name,
		CompiledStoryboardObjectPathMember& value) noexcept
	{
		if (name == L"Rect") value = CompiledStoryboardObjectPathMember::GeometryRect;
		else if (name == L"Center") value = CompiledStoryboardObjectPathMember::GeometryCenter;
		else if (name == L"RadiusX") value = CompiledStoryboardObjectPathMember::GeometryRadiusX;
		else if (name == L"RadiusY") value = CompiledStoryboardObjectPathMember::GeometryRadiusY;
		else if (name == L"FillRule") value = CompiledStoryboardObjectPathMember::GeometryFillRule;
		else return false;
		return true;
	}

	bool PathSegmentKindValue(
		std::wstring_view owner, uint8_t& value) noexcept
	{
		owner = LocalTypeName(owner);
		if (owner == L"LineSegment")
			value = static_cast<uint8_t>(cui::drawing::PathSegmentKind::Line);
		else if (owner == L"BezierSegment")
			value = static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier);
		else if (owner == L"QuadraticBezierSegment")
			value = static_cast<uint8_t>(cui::drawing::PathSegmentKind::QuadraticBezier);
		else if (owner == L"ArcSegment")
			value = static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc);
		else return false;
		return true;
	}

	bool PathMember(
		std::wstring_view owner,
		std::wstring_view name,
		CompiledStoryboardObjectPathMember& value) noexcept
	{
		owner = LocalTypeName(owner);
		if (owner == L"PathFigure" && name == L"StartPoint")
			value = CompiledStoryboardObjectPathMember::PathFigureStartPoint;
		else if (owner == L"PathFigure" && name == L"IsClosed")
			value = CompiledStoryboardObjectPathMember::PathFigureIsClosed;
		else if (owner == L"PathFigure" && name == L"IsFilled")
			value = CompiledStoryboardObjectPathMember::PathFigureIsFilled;
		else if (name == L"Point") value = CompiledStoryboardObjectPathMember::PathSegmentPoint;
		else if (name == L"Point1") value = CompiledStoryboardObjectPathMember::PathSegmentPoint1;
		else if (name == L"Point2") value = CompiledStoryboardObjectPathMember::PathSegmentPoint2;
		else if (name == L"Point3") value = CompiledStoryboardObjectPathMember::PathSegmentPoint3;
		else if (name == L"Size") value = CompiledStoryboardObjectPathMember::PathArcSize;
		else if (name == L"RotationAngle") value = CompiledStoryboardObjectPathMember::PathArcRotationAngle;
		else if (name == L"IsLargeArc") value = CompiledStoryboardObjectPathMember::PathArcIsLargeArc;
		else if (name == L"SweepDirection") value = CompiledStoryboardObjectPathMember::PathArcSweepDirection;
		else return false;
		return true;
	}

	bool BrushKind(
		std::wstring_view owner, uint8_t& value) noexcept
	{
		owner = LocalTypeName(owner);
		if (owner == L"SolidColorBrush")
			value = static_cast<uint8_t>(cui::drawing::BrushKind::Solid);
		else if (owner == L"LinearGradientBrush")
			value = static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient);
		else if (owner == L"RadialGradientBrush")
			value = static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient);
		else if (owner == L"ImageBrush")
			value = static_cast<uint8_t>(cui::drawing::BrushKind::Image);
		else if (owner == L"Brush" || owner == L"GradientBrush")
			value = (std::numeric_limits<uint8_t>::max)();
		else return false;
		return true;
	}

	bool BrushMember(
		const std::vector<PathSegment>& segments,
		CompiledStoryboardObjectPathMember& value) noexcept
	{
		const auto& leaf = segments.back();
		if (segments.size() == 2 && leaf.Name == L"Color")
			value = CompiledStoryboardObjectPathMember::BrushSolidColor;
		else if (segments.size() == 4 && leaf.Name == L"Color")
			value = CompiledStoryboardObjectPathMember::BrushGradientStopColor;
		else if (segments.size() == 4 && leaf.Name == L"Offset")
			value = CompiledStoryboardObjectPathMember::BrushGradientStopOffset;
		else if (leaf.Name == L"Opacity") value = CompiledStoryboardObjectPathMember::BrushOpacity;
		else if (leaf.Name == L"StartPoint") value = CompiledStoryboardObjectPathMember::BrushStartPoint;
		else if (leaf.Name == L"EndPoint") value = CompiledStoryboardObjectPathMember::BrushEndPoint;
		else if (leaf.Name == L"Center") value = CompiledStoryboardObjectPathMember::BrushCenter;
		else if (leaf.Name == L"GradientOrigin") value = CompiledStoryboardObjectPathMember::BrushGradientOrigin;
		else if (leaf.Name == L"RadiusX") value = CompiledStoryboardObjectPathMember::BrushRadiusX;
		else if (leaf.Name == L"RadiusY") value = CompiledStoryboardObjectPathMember::BrushRadiusY;
		else return false;
		return true;
	}

	bool DecodeObjectPath(
		const std::wstring& text,
		CompiledStoryboardObjectPathOp& output,
		std::vector<uint32_t>& childIndices,
		std::wstring& rootProperty,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError)
			|| path.Segments.size() < 2
			|| path.Segments.front().Kind != PathSegmentKind::Property)
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto& segments = path.Segments;
		rootProperty = segments.front().Name;
		output = {};
		output.ExpectedObjectKind = (std::numeric_limits<uint8_t>::max)();
		output.Identity = MakeCompiledInteractionNameToken(path.CanonicalText());
		auto checkedIndex = [&](size_t value, uint32_t& destination)
		{
			if (value > (std::numeric_limits<uint32_t>::max)()) return false;
			destination = static_cast<uint32_t>(value);
			return true;
		};

		if (rootProperty == L"RenderTransform")
		{
			const bool direct = segments.size() == 2;
			const bool grouped = segments.size() == 4
				&& segments[1].Kind == PathSegmentKind::Property
				&& LocalTypeName(segments[1].OwnerType) == L"TransformGroup"
				&& segments[1].Name == L"Children"
				&& segments[2].Kind == PathSegmentKind::Index;
			if (!direct && !grouped)
				return fail(L"RenderTransform 动画路径形状无效。");
			output.Kind = CompiledStoryboardObjectPathKind::Transform;
			if (!TransformKind(segments.back().OwnerType, output.ExpectedObjectKind)
				|| !TransformMember(segments.back().Name, output.Member)
				|| (grouped && !checkedIndex(segments[2].Index, output.Index0)))
				return fail(L"RenderTransform 动画路径末端无效。");
			if (direct)
				output.Flags =
					CompiledStoryboardObjectPathFlags::DirectTransform;
		}
		else if (rootProperty == L"Clip")
		{
			size_t cursor = 1;
			while (cursor + 1 < segments.size()
				&& segments[cursor].Kind == PathSegmentKind::Property
				&& LocalTypeName(segments[cursor].OwnerType) == L"GeometryGroup"
				&& segments[cursor].Name == L"Children"
				&& segments[cursor + 1].Kind == PathSegmentKind::Index)
			{
				if (segments[cursor + 1].Index
					> (std::numeric_limits<uint32_t>::max)())
					return fail(L"GeometryGroup.Children 索引超出编译范围。");
				childIndices.push_back(static_cast<uint32_t>(
					segments[cursor + 1].Index));
				cursor += 2;
			}
			if (cursor >= segments.size())
				return fail(L"Clip 动画路径缺少末端。");
			if (segments[cursor].Name == L"Transform")
			{
				const bool direct = cursor + 2 == segments.size()
					&& segments[cursor + 1].Kind == PathSegmentKind::Property;
				const bool grouped = cursor + 4 == segments.size()
					&& segments[cursor + 1].Name == L"Children"
					&& LocalTypeName(segments[cursor + 1].OwnerType) == L"TransformGroup"
					&& segments[cursor + 2].Kind == PathSegmentKind::Index;
				if (!direct && !grouped)
					return fail(L"Geometry.Transform 动画路径形状无效。");
				output.Kind = CompiledStoryboardObjectPathKind::GeometryTransform;
				if (!GeometryKind(segments[cursor].OwnerType, output.ExpectedObjectKind)
					|| !TransformKind(segments[direct ? cursor + 1 : cursor + 3].OwnerType,
						output.ExpectedAuxiliaryKind)
					|| !TransformMember(segments[direct ? cursor + 1 : cursor + 3].Name,
						output.Member)
					|| (grouped
						&& !checkedIndex(segments[cursor + 2].Index, output.Index0)))
					return fail(L"Geometry.Transform 动画路径末端无效。");
				if (direct)
					output.Flags =
						CompiledStoryboardObjectPathFlags::DirectTransform;
			}
			else if (segments[cursor].Name == L"Figures")
			{
				if (cursor + 2 >= segments.size()
					|| segments[cursor + 1].Kind != PathSegmentKind::Index
					|| !checkedIndex(segments[cursor + 1].Index, output.Index0))
					return fail(L"PathGeometry 动画路径缺少 Figure 索引。");
				cursor += 2;
				bool hasSegment = false;
				if (cursor + 2 < segments.size()
					&& segments[cursor].Name == L"Segments"
					&& segments[cursor + 1].Kind == PathSegmentKind::Index)
				{
					hasSegment = true;
					if (!checkedIndex(segments[cursor + 1].Index, output.Index1))
						return fail(L"PathGeometry Segment 索引超出编译范围。");
					cursor += 2;
				}
				if (cursor + 1 != segments.size()
					|| !PathMember(segments[cursor].OwnerType,
						segments[cursor].Name, output.Member))
					return fail(L"PathGeometry 动画路径末端无效。");
				output.Kind = CompiledStoryboardObjectPathKind::PathGeometry;
				if (hasSegment)
				{
					output.Flags = CompiledStoryboardObjectPathFlags::HasPathSegment;
					if (!PathSegmentKindValue(
						segments[cursor].OwnerType, output.ExpectedObjectKind))
						return fail(L"PathGeometry Segment 类型无效。");
				}
			}
			else
			{
				if (cursor + 1 != segments.size())
					return fail(L"Geometry 动画路径形状无效。");
				output.Kind = CompiledStoryboardObjectPathKind::Geometry;
				if (!GeometryKind(segments[cursor].OwnerType, output.ExpectedObjectKind)
					|| !GeometryMember(segments[cursor].Name, output.Member))
					return fail(L"Geometry 动画路径末端无效。");
			}
		}
		else if (rootProperty == L"Background"
			|| rootProperty == L"Foreground"
			|| rootProperty == L"BorderBrush")
		{
			const bool directTransform = segments.size() == 3
				&& (segments[1].Name == L"Transform"
					|| segments[1].Name == L"RelativeTransform")
				&& segments[2].Kind == PathSegmentKind::Property;
			const bool groupedTransform = segments.size() == 5
				&& (segments[1].Name == L"Transform"
					|| segments[1].Name == L"RelativeTransform")
				&& LocalTypeName(segments[2].OwnerType) == L"TransformGroup"
				&& segments[2].Name == L"Children"
				&& segments[3].Kind == PathSegmentKind::Index;
			if (directTransform || groupedTransform)
			{
				output.Kind = CompiledStoryboardObjectPathKind::BrushTransform;
				if (!BrushKind(segments[1].OwnerType, output.ExpectedObjectKind)
					|| !TransformKind(segments[directTransform ? 2 : 4].OwnerType,
						output.ExpectedAuxiliaryKind)
					|| !TransformMember(segments[directTransform ? 2 : 4].Name,
						output.Member)
					|| (groupedTransform
						&& !checkedIndex(segments[3].Index, output.Index0)))
					return fail(L"Brush.Transform 动画路径末端无效。");
				if (directTransform)
					output.Flags = output.Flags
						| CompiledStoryboardObjectPathFlags::DirectTransform;
				if (segments[1].Name == L"RelativeTransform")
					output.Flags = output.Flags
						| CompiledStoryboardObjectPathFlags::RelativeTransform;
			}
			else
			{
				const bool direct = segments.size() == 2;
				const bool gradientStop = segments.size() == 4
					&& segments[1].Name == L"GradientStops"
					&& segments[2].Kind == PathSegmentKind::Index;
				if (!direct && !gradientStop)
					return fail(L"Brush 动画路径形状无效。");
				output.Kind = CompiledStoryboardObjectPathKind::Brush;
				if (!BrushKind(segments[1].OwnerType, output.ExpectedObjectKind)
					|| !BrushMember(segments, output.Member)
					|| (gradientStop
						&& !checkedIndex(segments[2].Index, output.Index0)))
					return fail(L"Brush 动画路径末端无效。");
			}
		}
		else
			return fail(L"尚未注册可处理此 Storyboard.TargetProperty 的对象路径适配器。");

		output.ChildIndices = {
			0, static_cast<uint32_t>(childIndices.size()) };
		return true;
	}
}

namespace cui::framework::design
{
bool ResolveVisualStateAnimationOperands(
	const DeclarativeVisualStateAnimation& source,
	Control& owner,
	Control*& target,
	const DependencyPropertyMetadata*& metadata,
	std::optional<CompiledStoryboardObjectPathOp>& objectPath,
	std::vector<uint32_t>& objectPathChildIndices,
	std::wstring& propertyPath,
	std::wstring* outError)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	target = source.Target ? source.Target : &owner;
	if (!source.Target && !source.TargetName.empty())
		target = owner.FindDeclarativeTemplatePart(source.TargetName);
	if (!target)
		return fail(L"Storyboard 找不到模板部件：" + source.TargetName);
	propertyPath = source.PropertyPath();
	if (propertyPath.empty())
		return fail(L"Storyboard.TargetProperty 不能为空。");
	if (source.ObjectPath.empty())
	{
		metadata = source.Property.Identity()
			? target->GetPropertyMetadata(*source.Property.Identity())
			: target->FindPropertyMetadata(source.Property.Name());
		if (!metadata)
			return fail(L"Storyboard.TargetProperty 属性不存在："
				+ propertyPath);
		return true;
	}
	if (!source.Property.Empty())
		return fail(L"Storyboard.TargetProperty 不能同时声明直接属性与对象路径："
			+ propertyPath);
	CompiledStoryboardObjectPathOp operation;
	std::wstring rootProperty;
	if (!DecodeObjectPath(source.ObjectPath, operation,
		objectPathChildIndices, rootProperty, outError)) return false;
	metadata = target->FindPropertyMetadata(rootProperty);
	if (!metadata)
		return fail(L"Storyboard.TargetProperty 根属性不存在："
			+ rootProperty);
	objectPath = std::move(operation);
	return true;
}
}

struct ControlVisualStateDesignSidecar final
{
	struct State final
	{
		std::wstring Name;
		VisualStateToken Token;
	};

	struct Group final
	{
		std::wstring Name;
		VisualStateGroupToken Token;
		std::vector<State> States;
	};

	std::vector<Group> Groups;
};

namespace
{
	std::shared_ptr<ControlVisualStateDesignSidecar> BuildNameSidecar(
		const std::vector<DeclarativeVisualStateGroupDefinition>& definitions,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return std::shared_ptr<ControlVisualStateDesignSidecar>{};
		};

		auto sidecar = std::make_shared<ControlVisualStateDesignSidecar>();
		sidecar->Groups.reserve(definitions.size());
		for (const auto& sourceGroup : definitions)
		{
			ControlVisualStateDesignSidecar::Group group;
			group.Name = sourceGroup.Name;
			group.Token = MakeVisualStateGroupToken(group.Name);
			if (!group.Token)
				return fail(L"视觉状态组名称不能为空。");
			for (const auto& existing : sidecar->Groups)
				if (existing.Token == group.Token && existing.Name != group.Name)
					return fail(L"视觉状态组 token 冲突："
						+ existing.Name + L" / " + group.Name);

			group.States.reserve(sourceGroup.States.size());
			for (const auto& sourceState : sourceGroup.States)
			{
				ControlVisualStateDesignSidecar::State state{
					sourceState.Name, MakeVisualStateToken(sourceState.Name) };
				if (!state.Token)
					return fail(L"视觉状态名称不能为空。");
				for (const auto& existing : group.States)
					if (existing.Token == state.Token && existing.Name != state.Name)
						return fail(L"视觉状态 token 冲突："
							+ existing.Name + L" / " + state.Name);
				group.States.push_back(std::move(state));
			}
			sidecar->Groups.push_back(std::move(group));
		}
		return sidecar;
	}
}

bool Control::DefineVisualStateGroups(
	std::vector<DeclarativeVisualStateGroupDefinition> groups,
	std::wstring* outError)
{
	return DefineDeclarativeInteractions(std::move(groups), {}, outError);
}

bool Control::DefineDeclarativeInteractions(
	std::vector<DeclarativeVisualStateGroupDefinition> groups,
	std::vector<DeclarativeEventTriggerDefinition> eventTriggers,
	std::wstring* outError)
{
	auto sidecar = BuildNameSidecar(groups, outError);
	if (!sidecar && !groups.empty()) return false;
	if (!InstallDesignInteractionDefinitions(
		std::move(groups), std::move(eventTriggers), outError)) return false;
	_visualStateDesignSidecar = std::move(sidecar);
	if (outError) outError->clear();
	return true;
}

bool Control::GoToVisualState(
	const std::wstring& groupName,
	const std::wstring& stateName,
	std::wstring* outError)
{
	return GoToVisualState(groupName, stateName, true, outError);
}

bool Control::GoToVisualState(
	const std::wstring& groupName,
	const std::wstring& stateName,
	bool useTransitions,
	std::wstring* outError)
{
	if (!_visualStateDesignSidecar)
	{
		if (outError) *outError = L"控件未安装可按名称访问的视觉状态组。";
		return false;
	}
	const auto group = std::find_if(
		_visualStateDesignSidecar->Groups.begin(),
		_visualStateDesignSidecar->Groups.end(),
		[&](const auto& candidate) { return candidate.Name == groupName; });
	if (group == _visualStateDesignSidecar->Groups.end())
	{
		if (outError) *outError = L"视觉状态组不存在：" + groupName;
		return false;
	}
	const auto state = std::find_if(group->States.begin(), group->States.end(),
		[&](const auto& candidate) { return candidate.Name == stateName; });
	if (state == group->States.end())
	{
		if (outError) *outError = L"视觉状态不存在：" + stateName;
		return false;
	}
	return GoToVisualState(group->Token, state->Token, useTransitions, outError);
}

bool Control::GoToVisualState(
	const std::wstring& stateName,
	std::wstring* outError)
{
	return GoToVisualState(stateName, true, outError);
}

bool Control::GoToVisualState(
	const std::wstring& stateName,
	bool useTransitions,
	std::wstring* outError)
{
	if (!_visualStateDesignSidecar)
	{
		if (outError) *outError = L"控件未安装可按名称访问的视觉状态组。";
		return false;
	}
	const ControlVisualStateDesignSidecar::Group* foundGroup = nullptr;
	const ControlVisualStateDesignSidecar::State* foundState = nullptr;
	for (const auto& group : _visualStateDesignSidecar->Groups)
		for (const auto& state : group.States)
			if (state.Name == stateName)
			{
				if (foundState)
				{
					if (outError) *outError =
						L"视觉状态名称跨组重复，请指定组："
						+ stateName;
					return false;
				}
				foundGroup = &group;
				foundState = &state;
			}
	if (!foundState)
	{
		if (outError) *outError = L"视觉状态不存在：" + stateName;
		return false;
	}
	return GoToVisualState(
		foundGroup->Token, foundState->Token, useTransitions, outError);
}

std::wstring Control::GetCurrentVisualState(
	const std::wstring& groupName) const
{
	if (!_visualStateDesignSidecar) return {};
	const auto group = std::find_if(
		_visualStateDesignSidecar->Groups.begin(),
		_visualStateDesignSidecar->Groups.end(),
		[&](const auto& candidate) { return candidate.Name == groupName; });
	if (group == _visualStateDesignSidecar->Groups.end()) return {};
	const auto token = GetCurrentVisualState(group->Token);
	const auto state = std::find_if(group->States.begin(), group->States.end(),
		[&](const auto& candidate) { return candidate.Token == token; });
	return state == group->States.end() ? std::wstring{} : state->Name;
}
