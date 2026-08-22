#include "NamespacedWindow.g.h"
#include "Canvas.h"
#include "Layout/Grid.h"
#include "Layout/DockPanel.h"
#include "Layout/RelativePanel.h"
#include "Border.h"
#include "Button.h"
#include "Canvas.h"
#include "ContentPresenter.h"
#include "Layout/StackPanel.h"
#include "ControlTemplate.h"
#include "DependencyPropertyInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "TreeInfrastructure.h"
#include "CuiGeneratedFrameworkTheme.h"
#include "HeaderedContentControl.h"
#include "HeaderedItemsControl.h"
#include "Style.h"
#include "Resource.h"
#include "Utils.h"
#include <array>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
	class CuiGeneratedControlTemplate final
		: public IControlTemplate,
		  public std::enable_shared_from_this<CuiGeneratedControlTemplate>
	{
	public:
		using ApplyCallback = std::function<bool(
			Control&, std::wstring*)>;
		using HostFactory = std::function<std::unique_ptr<Control>()>;

		CuiGeneratedControlTemplate(
			UIClass targetType,
			std::wstring identity,
			HostFactory hostFactory)
			: _targetType(targetType),
			  _identity(std::move(identity)),
			  _hostFactory(std::move(hostFactory)) {}

		void SetApplyCallback(ApplyCallback value)
		{
			_apply = std::move(value);
		}

		UIClass TargetType() const noexcept override
		{
			return _targetType;
		}

		bool Apply(Control& owner,
			std::wstring* outError = nullptr) const override
		{
			if (!IsUIClassAssignableFrom(_targetType, owner.Type()))
			{
				if (outError) *outError =
					L"生成的 ControlTemplate TargetType 与宿主不兼容：" + _identity;
				return false;
			}
			if (!_apply)
			{
				if (outError) *outError =
					L"生成的 ControlTemplate 尚未完成初始化：" + _identity;
				return false;
			}
			return _apply(owner, outError);
		}

		std::unique_ptr<Control> Build(
			std::wstring* outError = nullptr) const override
		{
			auto owner = _hostFactory ? _hostFactory() : nullptr;
			if (!owner)
			{
				if (outError) *outError =
					L"生成的 ControlTemplate 无法构造宿主：" + _identity;
				return {};
			}
			auto self = std::static_pointer_cast<const IControlTemplate>(shared_from_this());
			if (!cui::framework::TemplateAccess::SetTemplate(
				*owner, ControlTemplateReference(std::move(self)),
				DependencyPropertyValueSource::Local))
			{
				if (outError) *outError =
					L"生成的 ControlTemplate 无法写入宿主：" + _identity;
				return {};
			}
			(void)owner->ApplyTemplate();
			if (!cui::framework::TemplateAccess::GetTemplateRoot(*owner)
				|| !owner->LastTemplateError().empty())
			{
				if (outError) *outError = owner->LastTemplateError().empty()
					? L"生成的 ControlTemplate 未生成视觉根：" + _identity
					: owner->LastTemplateError();
				return {};
			}
			if (outError) outError->clear();
			return owner;
		}

	private:
		UIClass _targetType = UIClass::UI_Base;
		std::wstring _identity;
		HostFactory _hostFactory;
		ApplyCallback _apply;
	};
}

Acme::Views::MainWindowGenerated::MainWindowGenerated()
	: Window()
{
}

void Acme::Views::MainWindowGenerated::InitializeComponent()
{

	if (_componentInitialized) return;
	_componentInitialized = true;

	// Native constructors are behavior-host implementation details.
	// Begin from the same empty Local-value surface as dynamic XAML.
	(void)this->ClearPropertyValues();

	// 创建控件
	// staticRoot
	auto __owned_staticRoot = std::make_unique<StackPanel>();
	staticRoot = __owned_staticRoot.get();
	(void)staticRoot->ClearPropertyValues();
	// namespaceButton
	auto __owned_namespaceButton = std::make_unique<Button>();
	namespaceButton = __owned_namespaceButton.get();
	(void)namespaceButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*namespaceButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// authorTemplateButton
	auto __owned_authorTemplateButton = std::make_unique<Button>();
	authorTemplateButton = __owned_authorTemplateButton.get();
	(void)authorTemplateButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*authorTemplateButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
	// styleTemplateButton
	auto __owned_styleTemplateButton = std::make_unique<Button>();
	styleTemplateButton = __owned_styleTemplateButton.get();
	(void)styleTemplateButton->ClearPropertyValues();
	(void)cui::framework::DependencyPropertyAccess::SetValue(*styleTemplateButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);

	std::wstring __frameworkThemeError;

	// Repeatable pure-C++ factories for authored ControlTemplate resources.
	auto __controlTemplate_StaticAuthorButtonTemplate_1 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_Button, L"StaticAuthorButtonTemplate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<Button>();
			(void)result->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_StaticAuthorButtonTemplate_1 = __controlTemplate_StaticAuthorButtonTemplate_1;
	auto __controlTemplate_StaticAuthorButtonTemplateAlternate_2 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_Button, L"StaticAuthorButtonTemplateAlternate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<Button>();
			(void)result->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_StaticAuthorButtonTemplateAlternate_2 = __controlTemplate_StaticAuthorButtonTemplateAlternate_2;

	__controlTemplate_StaticAuthorButtonTemplate_1->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner1_template_StaticAuthorChrome
			auto __owned___cuiStaticTemplateOwner1_template_StaticAuthorChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticAuthorChrome = __owned___cuiStaticTemplateOwner1_template_StaticAuthorChrome.get();
			(void)__cuiStaticTemplateOwner1_template_StaticAuthorChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_stackPanel1
			auto __owned___cuiStaticTemplateOwner1_template_stackPanel1 = std::make_unique<StackPanel>();
			auto* __cuiStaticTemplateOwner1_template_stackPanel1 = __owned___cuiStaticTemplateOwner1_template_stackPanel1.get();
			(void)__cuiStaticTemplateOwner1_template_stackPanel1->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticAuthorPresenter
			auto __owned___cuiStaticTemplateOwner1_template_StaticAuthorPresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner1_template_StaticAuthorPresenter = __owned___cuiStaticTemplateOwner1_template_StaticAuthorPresenter.get();
			(void)__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticNestedButton
			auto __owned___cuiStaticTemplateOwner1_template_StaticNestedButton = std::make_unique<Button>();
			auto* __cuiStaticTemplateOwner1_template_StaticNestedButton = __owned___cuiStaticTemplateOwner1_template_StaticNestedButton.get();
			(void)__cuiStaticTemplateOwner1_template_StaticNestedButton->ClearPropertyValues();
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton, Control::FocusableProperty(), BindingValue(true), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner1_template_StaticGradientChrome
			auto __owned___cuiStaticTemplateOwner1_template_StaticGradientChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticGradientChrome = __owned___cuiStaticTemplateOwner1_template_StaticGradientChrome.get();
			(void)__cuiStaticTemplateOwner1_template_StaticGradientChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticRadialChrome
			auto __owned___cuiStaticTemplateOwner1_template_StaticRadialChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticRadialChrome = __owned___cuiStaticTemplateOwner1_template_StaticRadialChrome.get();
			(void)__cuiStaticTemplateOwner1_template_StaticRadialChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticRectangleGeometry
			auto __owned___cuiStaticTemplateOwner1_template_StaticRectangleGeometry = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticRectangleGeometry = __owned___cuiStaticTemplateOwner1_template_StaticRectangleGeometry.get();
			(void)__cuiStaticTemplateOwner1_template_StaticRectangleGeometry->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticEllipseGeometry
			auto __owned___cuiStaticTemplateOwner1_template_StaticEllipseGeometry = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticEllipseGeometry = __owned___cuiStaticTemplateOwner1_template_StaticEllipseGeometry.get();
			(void)__cuiStaticTemplateOwner1_template_StaticEllipseGeometry->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticPathGeometry
			auto __owned___cuiStaticTemplateOwner1_template_StaticPathGeometry = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticPathGeometry = __owned___cuiStaticTemplateOwner1_template_StaticPathGeometry.get();
			(void)__cuiStaticTemplateOwner1_template_StaticPathGeometry->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticGeometryGroup
			auto __owned___cuiStaticTemplateOwner1_template_StaticGeometryGroup = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticGeometryGroup = __owned___cuiStaticTemplateOwner1_template_StaticGeometryGroup.get();
			(void)__cuiStaticTemplateOwner1_template_StaticGeometryGroup->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticRenderTransformDirect
			auto __owned___cuiStaticTemplateOwner1_template_StaticRenderTransformDirect = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticRenderTransformDirect = __owned___cuiStaticTemplateOwner1_template_StaticRenderTransformDirect.get();
			(void)__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticRenderTransformGroup
			auto __owned___cuiStaticTemplateOwner1_template_StaticRenderTransformGroup = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticRenderTransformGroup = __owned___cuiStaticTemplateOwner1_template_StaticRenderTransformGroup.get();
			(void)__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive
			auto __owned___cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive = __owned___cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive.get();
			(void)__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect
			auto __owned___cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect = __owned___cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect.get();
			(void)__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticBrushTransformGroup
			auto __owned___cuiStaticTemplateOwner1_template_StaticBrushTransformGroup = std::make_unique<Canvas>();
			auto* __cuiStaticTemplateOwner1_template_StaticBrushTransformGroup = __owned___cuiStaticTemplateOwner1_template_StaticBrushTransformGroup.get();
			(void)__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup->ClearPropertyValues();
			// __cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup
			auto __owned___cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup = __owned___cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup.get();
			(void)__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 12341456076199799814ULL }, __cuiStaticTemplateOwner1_template_StaticAuthorChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_stackPanel1, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 4045250499287358562ULL }, __cuiStaticTemplateOwner1_template_stackPanel1))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticAuthorPresenter, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 16313057772294406236ULL }, __cuiStaticTemplateOwner1_template_StaticAuthorPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner1_template_StaticAuthorPresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticNestedButton, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 4527577778959069028ULL }, __cuiStaticTemplateOwner1_template_StaticNestedButton))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticGradientChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 14036919521356441033ULL }, __cuiStaticTemplateOwner1_template_StaticGradientChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticRadialChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 14801748414811129354ULL }, __cuiStaticTemplateOwner1_template_StaticRadialChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticRectangleGeometry, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 802574568003800896ULL }, __cuiStaticTemplateOwner1_template_StaticRectangleGeometry))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticEllipseGeometry, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 9520991603996928223ULL }, __cuiStaticTemplateOwner1_template_StaticEllipseGeometry))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticPathGeometry, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 9966843878695937404ULL }, __cuiStaticTemplateOwner1_template_StaticPathGeometry))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticGeometryGroup, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 5106515138602522186ULL }, __cuiStaticTemplateOwner1_template_StaticGeometryGroup))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 16238813631564114336ULL }, __cuiStaticTemplateOwner1_template_StaticRenderTransformDirect))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 16957381120472003336ULL }, __cuiStaticTemplateOwner1_template_StaticRenderTransformGroup))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 7822337182734864695ULL }, __cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 1088511175942659676ULL }, __cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 2581559257923834374ULL }, __cuiStaticTemplateOwner1_template_StaticBrushTransformGroup))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 1782793498343186438ULL }, __cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup))
				return fail(L"ControlTemplate 部件注册失败。");
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.2f, 0.4f, 0.6f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF(0.f, 0.f, 1.f, 1.f); value.RadiusX = 0.f; value.RadiusY = 0.f; value.LocalTransform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Translate; operation.X = 0.05f; operation.Y = -0.1f; value.Operations.push_back(operation); } return value; }(); return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, Control::BorderBrushProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.133333f, 0.266667f, 0.4f, 1.f}; value.Transform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Rotate; operation.Angle = 5.f; operation.CenterX = 0.5f; operation.CenterY = 0.5f; value.Operations.push_back(operation); } return value; }(); return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, Control::BorderThicknessProperty(), BindingValue(Thickness(2.f, 2.f, 2.f, 2.f)), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton, Button::ContentProperty(), BindingValue(L"nested Generic.xaml host"), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGradientChrome, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGradientChrome, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGradientChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::LinearGradient; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::ScRgbLinearInterpolation; value.Opacity = 1.f; value.StartPoint = D2D1::Point2F(0.1f, 0.2f); value.EndPoint = D2D1::Point2F(0.9f, 0.8f); value.GradientStops.push_back({ 0.1f, D2D1_COLOR_F{0.062745f, 0.12549f, 0.188235f, 1.f} }); value.GradientStops.push_back({ 0.9f, D2D1_COLOR_F{0.752941f, 0.815686f, 0.878431f, 1.f} }); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRadialChrome, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRadialChrome, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRadialChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::RadialGradient; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.Center = D2D1::Point2F(0.45f, 0.55f); value.GradientOrigin = D2D1::Point2F(0.35f, 0.65f); value.RadiusX = 0.75f; value.RadiusY = 0.65f; value.GradientStops.push_back({ 0.1f, D2D1_COLOR_F{0.12549f, 0.25098f, 0.376471f, 1.f} }); value.GradientStops.push_back({ 0.9f, D2D1_COLOR_F{0.690196f, 0.439216f, 0.188235f, 1.f} }); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRectangleGeometry, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRectangleGeometry, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRectangleGeometry, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF(1.f, 2.f, 31.f, 42.f); value.RadiusX = 3.5f; value.RadiusY = 4.5f; return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticEllipseGeometry, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticEllipseGeometry, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticEllipseGeometry, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Ellipse; value.Center = D2D1::Point2F(20.5f, 30.5f); value.RadiusX = 8.5f; value.RadiusY = 9.5f; return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticPathGeometry, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticPathGeometry, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticPathGeometry, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Path; value.FillRule = cui::drawing::GeometryFillRule::Nonzero; value.Figures.push_back([] { cui::drawing::PathFigure figure; figure.StartPoint = D2D1::Point2F(1.f, 2.f); figure.IsClosed = false; figure.IsFilled = true; { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Line; segment.Point = D2D1::Point2F(10.f, 12.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Bezier; segment.Point = D2D1::Point2F(0.f, 0.f); segment.Point1 = D2D1::Point2F(21.f, 22.f); segment.Point2 = D2D1::Point2F(23.f, 24.f); segment.Point3 = D2D1::Point2F(25.f, 26.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::QuadraticBezier; segment.Point = D2D1::Point2F(0.f, 0.f); segment.Point1 = D2D1::Point2F(30.f, 31.f); segment.Point2 = D2D1::Point2F(33.f, 34.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Arc; segment.Point = D2D1::Point2F(40.f, 41.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(11.f, 12.f); segment.RotationAngle = 25.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } return figure; }()); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryGroup, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryGroup, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryGroup, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Group; value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF(1.f, 2.f, 31.f, 42.f); value.RadiusX = 3.5f; value.RadiusY = 4.5f; return value; }()); value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Group; value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Ellipse; value.Center = D2D1::Point2F(20.5f, 30.5f); value.RadiusX = 8.5f; value.RadiusY = 9.5f; return value; }()); return value; }()); value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Ellipse; value.Center = D2D1::Point2F(20.5f, 30.5f); value.RadiusX = 8.5f; value.RadiusY = 9.5f; return value; }()); value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Path; value.FillRule = cui::drawing::GeometryFillRule::Nonzero; value.Figures.push_back([] { cui::drawing::PathFigure figure; figure.StartPoint = D2D1::Point2F(1.f, 2.f); figure.IsClosed = false; figure.IsFilled = true; { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Line; segment.Point = D2D1::Point2F(10.f, 12.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Bezier; segment.Point = D2D1::Point2F(0.f, 0.f); segment.Point1 = D2D1::Point2F(21.f, 22.f); segment.Point2 = D2D1::Point2F(23.f, 24.f); segment.Point3 = D2D1::Point2F(25.f, 26.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::QuadraticBezier; segment.Point = D2D1::Point2F(0.f, 0.f); segment.Point1 = D2D1::Point2F(30.f, 31.f); segment.Point2 = D2D1::Point2F(33.f, 34.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(0.f, 0.f); segment.RotationAngle = 0.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } { cui::drawing::PathSegment segment; segment.Kind = cui::drawing::PathSegmentKind::Arc; segment.Point = D2D1::Point2F(40.f, 41.f); segment.Point1 = D2D1::Point2F(0.f, 0.f); segment.Point2 = D2D1::Point2F(0.f, 0.f); segment.Point3 = D2D1::Point2F(0.f, 0.f); segment.Size = D2D1::SizeF(11.f, 12.f); segment.RotationAngle = 25.f; segment.IsLargeArc = false; segment.Sweep = cui::drawing::SweepDirection::Counterclockwise; figure.Segments.push_back(segment); } return figure; }()); return value; }()); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect, Control::RenderTransformProperty(), BindingValue([] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Translate; operation.X = 7.25f; operation.Y = -3.25f; value.Operations.push_back(operation); } return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup, Control::RenderTransformProperty(), BindingValue([] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Translate; operation.X = 2.25f; operation.Y = -3.5f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Scale; operation.ScaleX = 1.25f; operation.ScaleY = 0.75f; operation.CenterX = 2.5f; operation.CenterY = -1.5f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Rotate; operation.Angle = 17.5f; operation.CenterX = 4.5f; operation.CenterY = -0.75f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Skew; operation.AngleX = 12.5f; operation.AngleY = -7.25f; operation.CenterX = 1.25f; operation.CenterY = -0.25f; value.Operations.push_back(operation); } return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Group; value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF(0.f, 0.f, 3.f, 4.f); value.RadiusX = 0.f; value.RadiusY = 0.f; return value; }()); value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Group; value.Children.push_back([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF(0.f, 0.f, 100.f, 60.f); value.RadiusX = 0.f; value.RadiusY = 0.f; value.LocalTransform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Translate; operation.X = 100.f; operation.Y = -100.f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Rotate; operation.Angle = 17.5f; operation.CenterX = 0.5f; operation.CenterY = -2.5f; value.Operations.push_back(operation); } return value; }(); return value; }()); return value; }()); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect, Control::ClipProperty(), BindingValue([] { cui::drawing::Geometry value; value.Kind = cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF(0.f, 0.f, 100.f, 60.f); value.RadiusX = 0.f; value.RadiusY = 0.f; value.LocalTransform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Skew; operation.AngleX = 12.5f; operation.AngleY = -7.25f; operation.CenterX = 0.5f; operation.CenterY = -1.75f; value.Operations.push_back(operation); } return value; }(); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.2f, 0.4f, 0.6f, 1.f}; value.Transform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Skew; operation.AngleX = 12.5f; operation.AngleY = -7.25f; operation.CenterX = 0.5f; operation.CenterY = -0.25f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Matrix; operation.Matrix = D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, 5.f, 6.f); value.Operations.push_back(operation); } return value; }(); return value; }()), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup, Control::WidthProperty(), BindingValue(cui::layout::Length::Fixed(32.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup, Control::HeightProperty(), BindingValue(cui::layout::Length::Fixed(8.f)), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::LinearGradient; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.StartPoint = D2D1::Point2F(0.f, 0.f); value.EndPoint = D2D1::Point2F(1.f, 1.f); value.GradientStops.push_back({ 0.f, D2D1_COLOR_F{0.062745f, 0.12549f, 0.188235f, 1.f} }); value.GradientStops.push_back({ 1.f, D2D1_COLOR_F{0.815686f, 0.878431f, 0.941176f, 1.f} }); value.RelativeTransform = [] { cui::drawing::Transform value; { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Translate; operation.X = 100.f; operation.Y = -100.f; value.Operations.push_back(operation); } { cui::drawing::TransformOperation operation; operation.Kind = cui::drawing::TransformKind::Skew; operation.AngleX = 12.5f; operation.AngleY = -7.25f; operation.CenterX = 0.5f; operation.CenterY = -0.25f; value.Operations.push_back(operation); } return value; }(); return value; }()), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorChrome->DataBindings.AddTemplateBinding(Border::PaddingProperty(), __templateOwner, Control::PaddingProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentProperty(), __templateOwner, Button::ContentProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentTemplateProperty(), __templateOwner, Button::ContentTemplateProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->SetCompiledDisplayMemberPath(static_cast<Button&>(__templateOwner).GetCompiledDisplayMemberPath());
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner1_template_StaticAuthorChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, nullptr);
			__cuiStaticTemplateOwner1_template_StaticAuthorChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner1_template_stackPanel1));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_stackPanel1, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticAuthorPresenter));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticAuthorPresenter, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticNestedButton));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticNestedButton, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticGradientChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticGradientChrome, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticRadialChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticRadialChrome, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticRectangleGeometry));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticRectangleGeometry, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticEllipseGeometry));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticEllipseGeometry, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticPathGeometry));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticPathGeometry, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticGeometryGroup));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticGeometryGroup, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticRenderTransformDirect));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticRenderTransformGroup));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticBrushTransformGroup));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup, nullptr);
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup, nullptr);
			{
				// AOT interaction program: process-static structure plus call-local values and targets.
				const BindingValue __cuiInteraction_values[] = {
					BindingValue(true),
					BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.917647f, 0.94902f, 1.f, 1.f}; return value; }()),
					BindingValue(4.f),
					BindingValue(0.f),
					BindingValue(0.f),
					BindingValue(6.f),
					BindingValue(0.f),
					BindingValue(30.f),
					BindingValue(1.0),
					BindingValue(0.5),
					BindingValue(0.05f),
					BindingValue(0.25f),
					BindingValue(5.f),
					BindingValue(25.f),
					BindingValue(D2D1_COLOR_F{0.133333f, 0.266667f, 0.4f, 1.f}),
					BindingValue(D2D1_COLOR_F{0.4f, 0.666667f, 0.266667f, 1.f}),
					BindingValue(0.9f),
					BindingValue(0.45f),
					BindingValue(cui::core::Point{ 0.1f, 0.2f }),
					BindingValue(cui::core::Point{ 0.4f, 0.5f }),
					BindingValue(cui::core::Point{ 0.9f, 0.8f }),
					BindingValue(cui::core::Point{ 0.6f, 0.5f }),
					BindingValue(D2D1_COLOR_F{0.062745f, 0.12549f, 0.188235f, 1.f}),
					BindingValue(D2D1_COLOR_F{0.878431f, 0.501961f, 0.12549f, 1.f}),
					BindingValue(0.9f),
					BindingValue(0.65f),
					BindingValue(cui::core::Point{ 0.35f, 0.65f }),
					BindingValue(cui::core::Point{ 0.45f, 0.55f }),
					BindingValue(cui::core::Point{ 0.45f, 0.55f }),
					BindingValue(cui::core::Point{ 0.55f, 0.45f }),
					BindingValue(0.75f),
					BindingValue(1.15f),
					BindingValue(0.65f),
					BindingValue(1.05f),
					BindingValue(cui::core::Rect{ 1.f, 2.f, 30.f, 40.f }),
					BindingValue(cui::core::Rect{ 5.f, 6.f, 50.f, 60.f }),
					BindingValue(3.5f),
					BindingValue(9.5f),
					BindingValue(4.5f),
					BindingValue(10.5f),
					BindingValue(cui::core::Point{ 20.5f, 30.5f }),
					BindingValue(cui::core::Point{ 40.5f, 10.5f }),
					BindingValue(8.5f),
					BindingValue(18.5f),
					BindingValue(9.5f),
					BindingValue(19.5f),
					BindingValue(L"Nonzero"),
					BindingValue(L"EvenOdd"),
					BindingValue(L"EvenOdd"),
					BindingValue(L"Nonzero"),
					BindingValue(cui::core::Point{ 1.f, 2.f }),
					BindingValue(cui::core::Point{ 11.f, 12.f }),
					BindingValue(false),
					BindingValue(true),
					BindingValue(true),
					BindingValue(false),
					BindingValue(cui::core::Point{ 10.f, 12.f }),
					BindingValue(cui::core::Point{ 30.f, 32.f }),
					BindingValue(cui::core::Point{ 21.f, 22.f }),
					BindingValue(cui::core::Point{ 41.f, 42.f }),
					BindingValue(cui::core::Point{ 23.f, 24.f }),
					BindingValue(cui::core::Point{ 43.f, 44.f }),
					BindingValue(cui::core::Point{ 25.f, 26.f }),
					BindingValue(cui::core::Point{ 45.f, 46.f }),
					BindingValue(cui::core::Point{ 30.f, 31.f }),
					BindingValue(cui::core::Point{ 50.f, 51.f }),
					BindingValue(cui::core::Point{ 33.f, 34.f }),
					BindingValue(cui::core::Point{ 53.f, 54.f }),
					BindingValue(cui::core::Point{ 40.f, 41.f }),
					BindingValue(cui::core::Point{ 60.f, 61.f }),
					BindingValue(cui::core::Size{ 11.f, 12.f }),
					BindingValue(cui::core::Size{ 21.f, 22.f }),
					BindingValue(25.f),
					BindingValue(85.f),
					BindingValue(false),
					BindingValue(true),
					BindingValue(L"Counterclockwise"),
					BindingValue(L"Clockwise"),
					BindingValue(7.25f),
					BindingValue(17.25f),
					BindingValue(-3.5f),
					BindingValue(6.5f),
					BindingValue(0.75f),
					BindingValue(1.75f),
					BindingValue(2.5f),
					BindingValue(12.5f),
					BindingValue(-1.5f),
					BindingValue(8.5f),
					BindingValue(4.5f),
					BindingValue(14.5f),
					BindingValue(-2.5f),
					BindingValue(7.5f),
					BindingValue(12.5f),
					BindingValue(32.5f),
					BindingValue(-7.25f),
					BindingValue(12.75f),
					BindingValue(1.25f),
					BindingValue(11.25f),
					BindingValue(-1.75f),
					BindingValue(8.25f),
					BindingValue(D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, 5.f, 6.f)),
					BindingValue(D2D1::Matrix3x2F(1.5f, 0.1f, 0.2f, 1.75f, 15.f, 26.f))
				};
				static const CompiledInteractionPropertyOperand __cuiInteraction_property_operands[] = {
					{ 0u, DependencyPropertyReference(Button::IsDefaultProperty()) },
					{ 1u, DependencyPropertyReference(Control::BackgroundProperty()) },
					{ 1u, DependencyPropertyReference(Control::CanvasTopProperty()) },
					{ 1u, DependencyPropertyReference(Control::CanvasLeftProperty()) },
					{ 1u, DependencyPropertyReference(Control::OpacityProperty()) },
					{ 1u, DependencyPropertyReference(Control::RenderTransformOriginProperty()) },
					{ 1u, DependencyPropertyReference(Control::ClipProperty()) },
					{ 1u, DependencyPropertyReference(Control::BorderBrushProperty()) },
					{ 2u, DependencyPropertyReference(Control::BackgroundProperty()) },
					{ 3u, DependencyPropertyReference(Control::BackgroundProperty()) },
					{ 4u, DependencyPropertyReference(Control::ClipProperty()) },
					{ 5u, DependencyPropertyReference(Control::ClipProperty()) },
					{ 6u, DependencyPropertyReference(Control::ClipProperty()) },
					{ 7u, DependencyPropertyReference(Control::ClipProperty()) },
					{ 8u, DependencyPropertyReference(Control::RenderTransformProperty()) },
					{ 9u, DependencyPropertyReference(Control::RenderTransformProperty()) },
					{ 10u, DependencyPropertyReference(Control::ClipProperty()) },
					{ 11u, DependencyPropertyReference(Control::BackgroundProperty()) },
					{ 12u, DependencyPropertyReference(Control::BackgroundProperty()) },
					{ 13u, DependencyPropertyReference(Control::ClipProperty()) }
				};
				static constexpr uint32_t __cuiInteraction_object_path_child_indices[] = {
					0u,
					1u,
					0u,
					2u,
					3u,
					3u,
					3u,
					3u,
					3u,
					1u,
					0u
				};
				static constexpr CompiledStoryboardObjectPathOp __cuiInteraction_object_paths[] = {
					{ CompiledStoryboardObjectPathKind::GeometryTransform, CompiledStoryboardObjectPathMember::TransformX, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Translate), CompiledStoryboardObjectPathFlags::DirectTransform, 0u, 0u, 0u, { 0u, 0u }, 14259506015378566175ULL },
					{ CompiledStoryboardObjectPathKind::BrushTransform, CompiledStoryboardObjectPathMember::TransformAngle, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Rotate), CompiledStoryboardObjectPathFlags::DirectTransform, 0u, 0u, 0u, { 0u, 0u }, 9357702240640821353ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushSolidColor, static_cast<uint8_t>(cui::drawing::BrushKind::Solid), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 8788905139745873109ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushOpacity, static_cast<uint8_t>(0xFFu), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 11229588378905187793ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushStartPoint, static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 415234065857161651ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushEndPoint, static_cast<uint8_t>(cui::drawing::BrushKind::LinearGradient), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 17528689014576655492ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushGradientStopColor, static_cast<uint8_t>(0xFFu), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 9216639137684478987ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushGradientStopOffset, static_cast<uint8_t>(0xFFu), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 1u, 0u, { 0u, 0u }, 5501453987660710098ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushGradientOrigin, static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 6778928179935990027ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushCenter, static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 12831192662193771170ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushRadiusX, static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 18410167561017832273ULL },
					{ CompiledStoryboardObjectPathKind::Brush, CompiledStoryboardObjectPathMember::BrushRadiusY, static_cast<uint8_t>(cui::drawing::BrushKind::RadialGradient), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 11412699675781508288ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryRect, static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 4506085006857668161ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryRadiusX, static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 0u }, 10616359997106608561ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryRadiusY, static_cast<uint8_t>(cui::drawing::GeometryKind::Rectangle), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 0u, 1u }, 14919961329762838785ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryCenter, static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 1u, 2u }, 3678833627790061632ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryRadiusX, static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 3u, 0u }, 12582104526512824578ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryRadiusY, static_cast<uint8_t>(cui::drawing::GeometryKind::Ellipse), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 3u, 1u }, 3088348801314391600ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryFillRule, static_cast<uint8_t>(cui::drawing::GeometryKind::Path), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 4u, 0u }, 246646261649670788ULL },
					{ CompiledStoryboardObjectPathKind::Geometry, CompiledStoryboardObjectPathMember::GeometryFillRule, static_cast<uint8_t>(cui::drawing::GeometryKind::Group), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 4u, 0u }, 3748867485992434470ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathFigureStartPoint, static_cast<uint8_t>(0xFFu), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 4u, 0u }, 8953514274425135000ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathFigureIsClosed, static_cast<uint8_t>(0xFFu), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 4u, 0u }, 1843339865799343188ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathFigureIsFilled, static_cast<uint8_t>(0xFFu), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 4u, 1u }, 1453159930813395162ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Line), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 0u, { 5u, 0u }, 8378199260804220002ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint1, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 1u, { 5u, 0u }, 9373848950775774023ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint2, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 1u, { 5u, 1u }, 15012194453915189782ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint3, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Bezier), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 1u, { 6u, 0u }, 4703941521010079717ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint1, static_cast<uint8_t>(cui::drawing::PathSegmentKind::QuadraticBezier), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 2u, { 6u, 0u }, 3549559461219630776ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint2, static_cast<uint8_t>(cui::drawing::PathSegmentKind::QuadraticBezier), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 2u, { 6u, 1u }, 11860466481428708569ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathSegmentPoint, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 3u, { 7u, 0u }, 9732174166833464215ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathArcSize, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 3u, { 7u, 1u }, 7371815262227331436ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathArcRotationAngle, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 3u, { 8u, 0u }, 4926361273260846366ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathArcIsLargeArc, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 3u, { 8u, 1u }, 8630934495325727126ULL },
					{ CompiledStoryboardObjectPathKind::PathGeometry, CompiledStoryboardObjectPathMember::PathArcSweepDirection, static_cast<uint8_t>(cui::drawing::PathSegmentKind::Arc), 0u, CompiledStoryboardObjectPathFlags::HasPathSegment, 0u, 0u, 3u, { 9u, 0u }, 6640483221841690122ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformX, static_cast<uint8_t>(cui::drawing::TransformKind::Translate), 0u, CompiledStoryboardObjectPathFlags::DirectTransform, 0u, 0u, 0u, { 9u, 0u }, 9386594340412624416ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformY, static_cast<uint8_t>(cui::drawing::TransformKind::Translate), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 9u, 0u }, 2717701538906003886ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformScaleY, static_cast<uint8_t>(cui::drawing::TransformKind::Scale), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 1u, 0u, { 9u, 0u }, 7869447313409034105ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformCenterX, static_cast<uint8_t>(cui::drawing::TransformKind::Scale), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 1u, 0u, { 9u, 0u }, 16235649554199954099ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformCenterY, static_cast<uint8_t>(cui::drawing::TransformKind::Scale), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 1u, 0u, { 9u, 0u }, 9238181668963630114ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformCenterX, static_cast<uint8_t>(cui::drawing::TransformKind::Rotate), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 2u, 0u, { 9u, 0u }, 14290771466266296921ULL },
					{ CompiledStoryboardObjectPathKind::GeometryTransform, CompiledStoryboardObjectPathMember::TransformCenterY, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Rotate), CompiledStoryboardObjectPathFlags::None, 0u, 1u, 0u, { 9u, 2u }, 12692277564059924645ULL },
					{ CompiledStoryboardObjectPathKind::BrushTransform, CompiledStoryboardObjectPathMember::TransformAngleX, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Skew), CompiledStoryboardObjectPathFlags::None, 0u, 0u, 0u, { 11u, 0u }, 17987369182574483985ULL },
					{ CompiledStoryboardObjectPathKind::BrushTransform, CompiledStoryboardObjectPathMember::TransformAngleY, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Skew), CompiledStoryboardObjectPathFlags::RelativeTransform, 0u, 1u, 0u, { 11u, 0u }, 15812299853555318813ULL },
					{ CompiledStoryboardObjectPathKind::Transform, CompiledStoryboardObjectPathMember::TransformCenterX, static_cast<uint8_t>(cui::drawing::TransformKind::Skew), 0u, CompiledStoryboardObjectPathFlags::None, 0u, 3u, 0u, { 11u, 0u }, 18083600529611234387ULL },
					{ CompiledStoryboardObjectPathKind::GeometryTransform, CompiledStoryboardObjectPathMember::TransformCenterY, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Skew), CompiledStoryboardObjectPathFlags::DirectTransform, 0u, 0u, 0u, { 11u, 0u }, 10891128205408321985ULL },
					{ CompiledStoryboardObjectPathKind::BrushTransform, CompiledStoryboardObjectPathMember::TransformMatrix, static_cast<uint8_t>(0xFFu), static_cast<uint8_t>(cui::drawing::TransformKind::Matrix), CompiledStoryboardObjectPathFlags::None, 0u, 1u, 0u, { 11u, 0u }, 8671975442601359891ULL }
				};
				static constexpr CompiledInteractionConditionOp __cuiInteraction_conditions[] = {
					{ 0u, 0u }
				};
				static constexpr CompiledInteractionSetterOp __cuiInteraction_setters[] = {
					{ 1u, 1u },
					{ 2u, 3u }
				};
				static constexpr CompiledInteractionKeyFrameOp __cuiInteraction_key_frames[] = {
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 46u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 47u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 48u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 49u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 52u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 53u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 54u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 55u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 74u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 75u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 76u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 77u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 0ULL, 100u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } },
					{ DeclarativeKeyFrameKind::Discrete, 200ULL, 101u, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, 0.f, 0.f, 1.f, 1.f, 0u, { 0.0, 0.0 } }
				};
				static constexpr DeclarativePathAnimationSegment __cuiInteraction_path_segments[] = {
					{ DeclarativePathSegmentKind::Line, { 0.f, 0.f }, { 0.f, 0.f }, { 0.422745f, -0.182729f } },
					{ DeclarativePathSegmentKind::CubicBezier, { 0.493178f, 0.451923f }, { 0.603784f, 0.627402f }, { 0.754562f, 0.34371f } },
					{ DeclarativePathSegmentKind::CubicBezier, { 1.017406f, 0.454686f }, { 1.211832f, 0.618696f }, { 1.223766f, 0.739513f } },
					{ DeclarativePathSegmentKind::CubicBezier, { 1.279069f, 0.827253f }, { 1.354458f, 0.685407f }, { 1.409761f, 0.773147f } },
					{ DeclarativePathSegmentKind::Line, { 0.f, 0.f }, { 0.f, 0.f }, { 0.23675f, -0.216362f } },
					{ DeclarativePathSegmentKind::Move, { 0.f, 0.f }, { 0.f, 0.f }, { 2.783633f, -0.533204f } },
					{ DeclarativePathSegmentKind::Line, { 0.f, 0.f }, { 0.f, 0.f }, { 3.128841f, -0.159823f } },
					{ DeclarativePathSegmentKind::CubicBezier, { 3.350052f, 0.191136f }, { 3.735432f, 0.105345f }, { 3.956644f, 0.456304f } }
				};
				static constexpr CompiledInteractionAnimationOp __cuiInteraction_animations[] = {
					{ DeclarativeAnimationKind::Double, 2u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, 2u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 0u, 0u }, 0ULL, 80ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 3u, CompiledInteractionInvalidIndex, 4u, 5u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 0u, 0u }, 0ULL, 120ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 3u, CompiledInteractionInvalidIndex, 6u, 7u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 0u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, true, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 4u, CompiledInteractionInvalidIndex, 8u, 9u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 0u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 5u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { true, { 0.23675f, -0.216362f }, DeclarativePathAnimationSource::X, false, false, false }, { 0u, 8u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 6u, 0u, 10u, 11u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 7u, 1u, 12u, 13u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Color, 7u, 2u, 14u, 15u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 7u, 3u, 16u, 17u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 8u, 4u, 18u, 19u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 8u, 5u, 20u, 21u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Color, 8u, 6u, 22u, 23u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 8u, 7u, 24u, 25u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 9u, 8u, 26u, 27u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 9u, 9u, 28u, 29u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 9u, 10u, 30u, 31u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 9u, 11u, 32u, 33u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Rect, 10u, 12u, 34u, 35u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 10u, 13u, 36u, 37u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 11u, 14u, 38u, 39u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 11u, 15u, 40u, 41u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 12u, 16u, 42u, 43u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 11u, 17u, 44u, 45u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Object, 13u, 18u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 0u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Object, 11u, 19u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 2u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 13u, 20u, 50u, 51u, CompiledInteractionInvalidIndex, { 4u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Object, 13u, 21u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 4u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Object, 11u, 22u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 6u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 13u, 23u, 56u, 57u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 13u, 24u, 58u, 59u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 11u, 25u, 60u, 61u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 13u, 26u, 62u, 63u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 13u, 27u, 64u, 65u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 11u, 28u, 66u, 67u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Point, 13u, 29u, 68u, 69u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Size, 11u, 30u, 70u, 71u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 13u, 31u, 72u, 73u, CompiledInteractionInvalidIndex, { 8u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Object, 11u, 32u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 8u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Object, 13u, 33u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 10u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 14u, 34u, 78u, 79u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 15u, 35u, 80u, 81u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 15u, 36u, 82u, 83u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 15u, 37u, 84u, 85u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 15u, 38u, 86u, 87u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 15u, 39u, 88u, 89u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 16u, 40u, 90u, 91u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 17u, 41u, 92u, 93u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 18u, 42u, 94u, 95u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 15u, 43u, 96u, 97u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Double, 19u, 44u, 98u, 99u, CompiledInteractionInvalidIndex, { 12u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } },
					{ DeclarativeAnimationKind::Matrix, 17u, 45u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, { 12u, 2u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 8u, 0u }, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } }
				};
				static constexpr CompiledInteractionStateOp __cuiInteraction_states[] = {
					{ VisualStateToken{ 10757986138372673727ULL }, { 0u, 1u }, { 0u, 0u }, { 0u, 1u }, { 0u, 1u }, { 0u, 0u }, { 5ULL, false, 100ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.25, 0.0, 0.0 } },
					{ VisualStateToken{ 175525885933743510ULL }, { 1u, 0u }, { 0u, 0u }, { 1u, 1u }, { 1u, 0u }, { 0u, 0u }, { 0ULL, true, 0ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0 } }
				};
				static constexpr CompiledInteractionTransitionOp __cuiInteraction_transitions[] = {
					{ 1u, 0u, 120ULL, DeclarativeEasingKind::Quadratic, DeclarativeEasingMode::EaseInOut, { 0.0, 0.0 }, { 1u, 1u }, { 0u, 0u }, { 0ULL, true, 0ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0 } }
				};
				static constexpr uint32_t __cuiInteraction_group_condition_operands[] = {
					0u
				};
				static constexpr CompiledInteractionGroupOp __cuiInteraction_groups[] = {
					{ VisualStateGroupToken{ 7493761989248210713ULL }, { 0u, 2u }, { 0u, 1u }, 1u, { 0u, 1u } }
				};
				static constexpr CompiledInteractionStoryboardOp __cuiInteraction_storyboards[] = {
					{ { 2u, 49u }, { 0u, 0u }, { 10ULL, false, 250ULL, DeclarativeRepeatBehaviorKind::Count, 2.0, 0ULL, true, DeclarativeTimelineFillBehavior::HoldEnd, 1.25, 0.1, 0.2 } }
				};
				static constexpr CompiledInteractionActionOp __cuiInteraction_actions[] = {
					{ DeclarativeStoryboardActionKind::Begin, 0u, DeclarativeHandoffBehavior::SnapshotAndReplace }
				};
				static const CompiledInteractionEventTriggerOp __cuiInteraction_event_triggers[] = {
					{ nullptr, static_cast<RoutedEventId>(11), { 0u, 1u } }
				};
				static const CompiledInteractionProgramView __cuiInteractionProgram{
					CompiledInteractionProgramViewVersion,
					14u,
					std::span<const CompiledInteractionPropertyOperand>{ __cuiInteraction_property_operands }, // PropertyOperands
					std::span<const uint32_t>{ __cuiInteraction_object_path_child_indices }, // ObjectPathChildIndices
					std::span<const CompiledStoryboardObjectPathOp>{ __cuiInteraction_object_paths }, // ObjectPaths
					std::span<const CompiledInteractionConditionOp>{ __cuiInteraction_conditions }, // Conditions
					std::span<const CompiledInteractionSetterOp>{ __cuiInteraction_setters }, // Setters
					std::span<const CompiledInteractionKeyFrameOp>{ __cuiInteraction_key_frames }, // KeyFrames
					std::span<const DeclarativePathAnimationSegment>{ __cuiInteraction_path_segments }, // PathSegments
					std::span<const CompiledInteractionAnimationOp>{ __cuiInteraction_animations }, // Animations
					{}, // TimelineGroups
					{}, // StateEvents
					std::span<const CompiledInteractionStateOp>{ __cuiInteraction_states }, // States
					std::span<const CompiledInteractionTransitionOp>{ __cuiInteraction_transitions }, // Transitions
					std::span<const uint32_t>{ __cuiInteraction_group_condition_operands }, // GroupConditionOperands
					std::span<const CompiledInteractionGroupOp>{ __cuiInteraction_groups }, // Groups
					std::span<const CompiledInteractionStoryboardOp>{ __cuiInteraction_storyboards }, // Storyboards
					std::span<const CompiledInteractionActionOp>{ __cuiInteraction_actions }, // Actions
					std::span<const CompiledInteractionEventTriggerOp>{ __cuiInteraction_event_triggers } // EventTriggers
				};
				std::array<Control*, 14> __cuiInteractionTargets{
					&(__templateOwner),
					__cuiStaticTemplateOwner1_template_StaticAuthorChrome,
					__cuiStaticTemplateOwner1_template_StaticGradientChrome,
					__cuiStaticTemplateOwner1_template_StaticRadialChrome,
					__cuiStaticTemplateOwner1_template_StaticRectangleGeometry,
					__cuiStaticTemplateOwner1_template_StaticGeometryGroup,
					__cuiStaticTemplateOwner1_template_StaticEllipseGeometry,
					__cuiStaticTemplateOwner1_template_StaticPathGeometry,
					__cuiStaticTemplateOwner1_template_StaticRenderTransformDirect,
					__cuiStaticTemplateOwner1_template_StaticRenderTransformGroup,
					__cuiStaticTemplateOwner1_template_StaticGeometryTransformRecursive,
					__cuiStaticTemplateOwner1_template_StaticBrushTransformGroup,
					__cuiStaticTemplateOwner1_template_StaticBrushRelativeTransformGroup,
					__cuiStaticTemplateOwner1_template_StaticGeometryTransformDirect
				};
				std::wstring interactionError;
				if (!cui::framework::TemplateAccess::InstallCompiledInteractions(__templateOwner, __cuiInteractionProgram, std::span<const BindingValue>{ __cuiInteraction_values }, std::span<Control* const>{ __cuiInteractionTargets }, &interactionError))
					return fail(L"ControlTemplate 声明交互安装失败：" + interactionError);
			}
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	__controlTemplate_StaticAuthorButtonTemplateAlternate_2->SetApplyCallback([this](Control& __templateOwner, std::wstring* outError) -> bool
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		try
		{
			std::wstring __templateThemeError;
			// __cuiStaticTemplateOwner2_template_StaticAlternateChrome
			auto __owned___cuiStaticTemplateOwner2_template_StaticAlternateChrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner2_template_StaticAlternateChrome = __owned___cuiStaticTemplateOwner2_template_StaticAlternateChrome.get();
			(void)__cuiStaticTemplateOwner2_template_StaticAlternateChrome->ClearPropertyValues();
			// __cuiStaticTemplateOwner2_template_StaticAlternatePresenter
			auto __owned___cuiStaticTemplateOwner2_template_StaticAlternatePresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner2_template_StaticAlternatePresenter = __owned___cuiStaticTemplateOwner2_template_StaticAlternatePresenter.get();
			(void)__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->ClearPropertyValues();

			// Establish a fresh template namescope for this application.
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 5833347753736888225ULL }, __cuiStaticTemplateOwner2_template_StaticAlternateChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_StaticAlternatePresenter, &__templateOwner);
			if (!cui::framework::TemplateAccess::RegisterTemplatePart(__templateOwner, TemplatePartToken{ 7141794723077091073ULL }, __cuiStaticTemplateOwner2_template_StaticAlternatePresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner2_template_StaticAlternatePresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.ColorInterpolationMode = cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.909804f, 0.960784f, 0.933333f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, Control::BorderThicknessProperty(), BindingValue(Thickness(3.f, 3.f, 3.f, 3.f)), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentProperty(), __templateOwner, Button::ContentProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->DataBindings.AddTemplateBinding(ContentPresenter::ContentTemplateProperty(), __templateOwner, Button::ContentTemplateProperty()))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->SetCompiledDisplayMemberPath(static_cast<Button&>(__templateOwner).GetCompiledDisplayMemberPath());
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner2_template_StaticAlternateChrome));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, nullptr);
			__cuiStaticTemplateOwner2_template_StaticAlternateChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner2_template_StaticAlternatePresenter));
			cui::framework::TreeAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_StaticAlternatePresenter, nullptr);
			if (!cui::framework::TemplateAccess::GetTemplateRoot(__templateOwner))
				return fail(L"ControlTemplate 未生成唯一视觉根。");
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception&)
		{
			return fail(L"ControlTemplate 静态构造发生运行时异常。");
		}
		catch (...)
		{
			return fail(L"ControlTemplate 静态构造发生未知异常。");
		}
	});

	// XAML authored Local properties/resources
	staticRoot->SetOrientation(static_cast<Orientation>(1));

	namespaceButton->CommandTarget = this;
	// XAML authored Local properties/resources
	namespaceButton->SetWidth(cui::layout::Length::Fixed(120.f));
	namespaceButton->SetHeight(cui::layout::Length::Fixed(24.f));
	namespaceButton->SetAllowDrop(true);
	namespaceButton->SetCommand(L"Demo.Static.Refresh");
	namespaceButton->SetCommandParameter(BindingValue(L"static-input"));

	// XAML authored Local properties/resources
	authorTemplateButton->SetContent(BindingValue(L"author template"));
	authorTemplateButton->SetWidth(cui::layout::Length::Fixed(260.f));
	authorTemplateButton->SetHeight(cui::layout::Length::Fixed(72.f));
	authorTemplateButton->SetPadding(Thickness(6.f, 6.f, 6.f, 6.f));

	cui::framework::StyleAccess::SetResourceKey(*styleTemplateButton, L"StaticAuthorButtonStyle", false);
	// XAML authored Local properties/resources
	styleTemplateButton->SetContent(BindingValue(L"Style.Template"));
	styleTemplateButton->SetWidth(cui::layout::Length::Fixed(220.f));
	styleTemplateButton->SetHeight(cui::layout::Length::Fixed(48.f));

	// XAML InputBindings
	(void)this->AddInputBinding(KeyBinding{ RoutedCommand(L"Demo.Static.Refresh"), KeyGesture{ Key::F5, ModifierKeys::None }, std::wstring(L"static-input"), namespaceButton });

	// 绑定事件
	_generatedEventConnections.emplace_back(
		this->ContentRendered.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleWindowContentRendered, this)));
	_generatedEventConnections.emplace_back(
		namespaceButton->Click.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleNamespacedClick, this)));
	_generatedEventConnections.emplace_back(
		namespaceButton->OnDrop.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleNamespacedDrop, this)));

	// XAML CommandBindings
	{
		CommandBinding __commandBinding;
		__commandBinding.Command = RoutedCommand(L"Demo.Static.Refresh");
		__commandBinding.CanExecute = [this](Control* sender, CanExecuteRoutedEventArgs& e) { HandleStaticRefreshCanExecute(sender, e); };
		__commandBinding.Executed = [this](Control* sender, ExecutedRoutedEventArgs& e) { HandleStaticRefreshExecuted(sender, e); };
		_generatedEventConnections.emplace_back(this->AddCommandBinding(std::move(__commandBinding)));
	}

	// 组装控件层级（包含布局容器）
	this->SetVisualContent(std::move(__owned_staticRoot));
	staticRoot->AddOwned(std::move(__owned_namespaceButton));

	staticRoot->AddOwned(std::move(__owned_authorTemplateButton));

	staticRoot->AddOwned(std::move(__owned_styleTemplateButton));


	auto __frameworkThemeStyles = CuiGeneratedFrameworkTheme::DefaultStyleSheet(&__frameworkThemeError);
	if (!__frameworkThemeStyles)
		throw std::runtime_error("Generated Generic.xaml theme construction failed");
	// AOT Style 程序：生成期完成分组、索引和连续池布局
	static constexpr D2D1_COLOR_F __styleSheet_program_values_colors[] = {
		D2D1_COLOR_F{0.2f, 0.4f, 0.6f, 1.f}
	};
	static constexpr double __styleSheet_program_values_doubles[] = {
		14.0
	};
	static constexpr bool __styleSheet_program_values_bools[] = {
		true
	};
	static constexpr std::wstring_view __styleSheet_program_strings[] = {
		L"StaticTemplateAccent",
		L"StaticAuthorButtonTemplate",
		L"StaticAuthorButtonTemplateAlternate",
		L"StaticAuthorButtonStyle"
	};
	static constexpr CompiledStyleValuePoolView __styleSheet_program_value_pools[] = {
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_colors),
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_doubles),
		MakeCompiledStyleValuePoolView(__styleSheet_program_values_bools)
	};
	static const CompiledStyleResourceOp __styleSheet_program_resources[] = {
		{ 0u, MakeCompiledStyleStaticValueReference(0u, 0u) },
		{ 1u, 0u },
		{ 2u, 1u }
	};
	static constexpr uint32_t __styleSheet_program_resource_lookup[] = {
		1u,
		2u,
		0u
	};
	static const CompiledStylePropertyConditionOp __styleSheet_program_property_conditions[] = {
		{ DependencyPropertyReference(Button::IsDefaultProperty()), MakeCompiledStyleStaticValueReference(2u, 0u) }
	};
	static const CompiledStyleSetterOp __styleSheet_program_setters[] = {
		{ DependencyPropertyReference(Control::TemplateProperty()), { CompiledStyleOperandKind::StaticResource, 1u } },
		{ DependencyPropertyReference(Control::FontSizeProperty()), { CompiledStyleOperandKind::Literal, MakeCompiledStyleStaticValueReference(1u, 0u) } }
	};
	static const CompiledInteractionPropertyOperand __styleSheet_program_property_operands[] = {
		{ 0u, DependencyPropertyReference(Control::FontSizeProperty()) }
	};
	static constexpr CompiledInteractionAnimationOp __styleSheet_program_animations[] = {
		{ DeclarativeAnimationKind::Double, 0u, CompiledInteractionInvalidIndex, 2u, 3u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, { false, { 0.f, 0.f }, DeclarativePathAnimationSource::X, false, false, false }, { 0u, 0u }, 0ULL, 100ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut, { 0.0, 0.0 } }
	};
	static constexpr CompiledInteractionStoryboardOp __styleSheet_program_storyboards[] = {
		{ { 0u, 1u }, { 0u, 0u }, { 0ULL, true, 0ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0 } }
	};
	static constexpr CompiledInteractionActionOp __styleSheet_program_actions[] = {
		{ DeclarativeStoryboardActionKind::Begin, 0u, DeclarativeHandoffBehavior::SnapshotAndReplace },
		{ DeclarativeStoryboardActionKind::Stop, 0u, DeclarativeHandoffBehavior::SnapshotAndReplace }
	};
	static constexpr CompiledStyleRuleOp __styleSheet_program_rules[] = {
		{ 1u, 0u, { 0u, 0u }, { 0u, 0u }, { 0u, 2u }, { 0u, 0u }, { 0u, 0u } },
		{ 2u, 1u, { 0u, 1u }, { 0u, 0u }, { 2u, 0u }, { 0u, 1u }, { 1u, 1u } }
	};
	static constexpr uint32_t __styleSheet_program_rule_indexes[] = {
		0u,
		1u
	};
	static const DependencyPropertyReference __styleSheet_program_property_watchers[] = {
		DependencyPropertyReference(Button::IsDefaultProperty())
	};
	static constexpr CompiledStyleGroupOp __styleSheet_program_groups[] = {
		{ true, static_cast<UIClass>(7), ComponentTypeToken{ 0ULL }, 3u, { 0u, 2u }, { 0u, 1u }, { 0u, 0u } }
	};
	static const DependencyPropertyReference __styleSheet_program_global_property_watchers[] = {
		DependencyPropertyReference(Button::IsDefaultProperty())
	};
	auto __styleSheet = ControlStyleSheet::CreateCompiled(
		CompiledStyleProgramView{
			CompiledStyleProgramViewVersion,
			std::span<const std::wstring_view>{ __styleSheet_program_strings }, // Strings
			std::span<const CompiledStyleValuePoolView>{ __styleSheet_program_value_pools }, // ValuePools
			std::span<const CompiledStyleResourceOp>{ __styleSheet_program_resources }, // Resources
			std::span<const uint32_t>{ __styleSheet_program_resource_lookup }, // ResourceLookup
			std::span<const CompiledStylePropertyConditionOp>{ __styleSheet_program_property_conditions }, // PropertyConditions
			{}, // DataConditions
			std::span<const CompiledStyleSetterOp>{ __styleSheet_program_setters }, // Setters
			std::span<const CompiledInteractionPropertyOperand>{ __styleSheet_program_property_operands }, // PropertyOperands
			{}, // ObjectPathChildIndices
			{}, // ObjectPaths
			{}, // KeyFrames
			{}, // PathSegments
			std::span<const CompiledInteractionAnimationOp>{ __styleSheet_program_animations }, // Animations
			{}, // TimelineGroups
			std::span<const CompiledInteractionStoryboardOp>{ __styleSheet_program_storyboards }, // Storyboards
			std::span<const CompiledInteractionActionOp>{ __styleSheet_program_actions }, // Actions
			std::span<const CompiledStyleRuleOp>{ __styleSheet_program_rules }, // Rules
			std::span<const uint32_t>{ __styleSheet_program_rule_indexes }, // RuleIndexes
			std::span<const DependencyPropertyReference>{ __styleSheet_program_property_watchers }, // PropertyWatchers
			{}, // DataPathWatchers
			std::span<const CompiledStyleGroupOp>{ __styleSheet_program_groups }, // Groups
			std::span<const DependencyPropertyReference>{ __styleSheet_program_global_property_watchers }, // GlobalPropertyWatchers
			{}, // GlobalDataPathWatchers
			{}, // DataPaths
		},
		std::vector<BindingValue>{
			BindingValue(ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplate_1)),
			BindingValue(ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplateAlternate_2)),
			BindingValue(14.0),
			BindingValue(18.0)
		}
	);

	if (!cui::framework::StyleAccess::SetEnvironment(*this, std::move(__frameworkThemeStyles), std::move(__styleSheet), true))
		throw std::runtime_error("Generated Theme/Document style environment installation failed");

	if (!cui::framework::TemplateAccess::SetTemplate(*authorTemplateButton, ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplate_1), DependencyPropertyValueSource::Local))
		throw std::runtime_error("Generated authored Control.Template installation failed");

	// XAML Window Local 属性/资源表达式
	this->SetHeight(cui::layout::Length::Fixed(600.f));
	this->SetTitle(L"Acme::Views::MainWindow");
	this->SetWidth(cui::layout::Length::Fixed(800.f));

}

Acme::Views::MainWindowGenerated::~MainWindowGenerated()
{
}

bool Acme::Views::MainWindowGenerated::BindData(BindingSourceReference dataContext)
{
	if (!dataContext) return false;
	auto __windowDataContext = dataContext;
	if (!SetDataContext(std::move(dataContext))) return false;
	bool success = true;
	namespaceButton->DataBindings.Clear();
	{
		bool cuiBindingAttached = false;
		static constexpr CompiledBindingPathStep __cuiCompiledBindingPath_1[] =
		{
			{ CompiledBindingPathStepKind::Property, CompiledBindingPathCapabilities::Read | CompiledBindingPathCapabilities::Write | CompiledBindingPathCapabilities::Observe, BindingValueKind::String, BindingSourcePropertyToken{ 7131070297711251227ULL }, 0u },
		};
		cuiBindingAttached = namespaceButton->DataBindings.Add(Button::ContentProperty(), namespaceButton->DataContextSource(), CompiledBindingPathView{ __cuiCompiledBindingPath_1 }, BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	return success;
}
