#include "NamespacedWindow.g.h"
#include "Canvas.h"
#include "Layout/Grid.h"
#include "Layout/DockPanel.h"
#include "Layout/RelativePanel.h"
#include "Border.h"
#include "Button.h"
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
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.2f, 0.4f, 0.6f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, Control::BorderThicknessProperty(), BindingValue(Thickness(2.f, 2.f, 2.f, 2.f)), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton, Button::ContentProperty(), BindingValue(L"nested Generic.xaml host"), DependencyPropertyValueSource::Template);
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
			{
				// AOT interaction program: process-static structure plus call-local values and targets.
				const BindingValue __cuiInteraction_values[] = {
					BindingValue(true),
					BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.917647f, 0.94902f, 1.f, 1.f}; return value; }()),
					BindingValue(4.f),
					BindingValue(0.f),
					BindingValue(0.f),
					BindingValue(30.f)
				};
				static const CompiledInteractionPropertyOperand __cuiInteraction_property_operands[] = {
					{ 0u, DependencyPropertyReference(Button::IsDefaultProperty()) },
					{ 1u, DependencyPropertyReference(Control::BackgroundProperty()) },
					{ 1u, DependencyPropertyReference(Control::CanvasTopProperty()) },
					{ 1u, DependencyPropertyReference(Control::CanvasLeftProperty()) }
				};
				static constexpr CompiledInteractionConditionOp __cuiInteraction_conditions[] = {
					{ 0u, 0u }
				};
				static constexpr CompiledInteractionSetterOp __cuiInteraction_setters[] = {
					{ 1u, 1u },
					{ 2u, 3u }
				};
				static constexpr CompiledInteractionAnimationOp __cuiInteraction_animations[] = {
					{ DeclarativeAnimationKind::Double, 2u, CompiledInteractionInvalidIndex, CompiledInteractionInvalidIndex, 2u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, 0ULL, 80ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut },
					{ DeclarativeAnimationKind::Double, 3u, CompiledInteractionInvalidIndex, 4u, 5u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, 0ULL, 200ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, true, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut }
				};
				static constexpr CompiledInteractionStateOp __cuiInteraction_states[] = {
					{ VisualStateToken{ 10757986138372673727ULL }, { 0u, 1u }, { 0u, 0u }, { 0u, 1u }, { 0u, 1u } },
					{ VisualStateToken{ 175525885933743510ULL }, { 1u, 0u }, { 0u, 0u }, { 1u, 1u }, { 1u, 0u } }
				};
				static constexpr CompiledInteractionTransitionOp __cuiInteraction_transitions[] = {
					{ 1u, 0u, 120ULL, DeclarativeEasingKind::Quadratic, DeclarativeEasingMode::EaseInOut, { 1u, 0u } }
				};
				static constexpr uint32_t __cuiInteraction_group_condition_operands[] = {
					0u
				};
				static constexpr CompiledInteractionGroupOp __cuiInteraction_groups[] = {
					{ VisualStateGroupToken{ 7493761989248210713ULL }, { 0u, 2u }, { 0u, 1u }, 1u, { 0u, 1u } }
				};
				static constexpr CompiledInteractionStoryboardOp __cuiInteraction_storyboards[] = {
					{ { 1u, 1u } }
				};
				static constexpr CompiledInteractionActionOp __cuiInteraction_actions[] = {
					{ DeclarativeStoryboardActionKind::Begin, 0u }
				};
				static const CompiledInteractionEventTriggerOp __cuiInteraction_event_triggers[] = {
					{ nullptr, static_cast<RoutedEventId>(11), { 0u, 1u } }
				};
				static const CompiledInteractionProgramView __cuiInteractionProgram{
					CompiledInteractionProgramViewVersion,
					2u,
					std::span<const CompiledInteractionPropertyOperand>{ __cuiInteraction_property_operands }, // PropertyOperands
					{}, // ObjectPathChildIndices
					{}, // ObjectPaths
					std::span<const CompiledInteractionConditionOp>{ __cuiInteraction_conditions }, // Conditions
					std::span<const CompiledInteractionSetterOp>{ __cuiInteraction_setters }, // Setters
					{}, // KeyFrames
					std::span<const CompiledInteractionAnimationOp>{ __cuiInteraction_animations }, // Animations
					{}, // StateEvents
					std::span<const CompiledInteractionStateOp>{ __cuiInteraction_states }, // States
					std::span<const CompiledInteractionTransitionOp>{ __cuiInteraction_transitions }, // Transitions
					std::span<const uint32_t>{ __cuiInteraction_group_condition_operands }, // GroupConditionOperands
					std::span<const CompiledInteractionGroupOp>{ __cuiInteraction_groups }, // Groups
					std::span<const CompiledInteractionStoryboardOp>{ __cuiInteraction_storyboards }, // Storyboards
					std::span<const CompiledInteractionActionOp>{ __cuiInteraction_actions }, // Actions
					std::span<const CompiledInteractionEventTriggerOp>{ __cuiInteraction_event_triggers } // EventTriggers
				};
				std::array<Control*, 2> __cuiInteractionTargets{
					&(__templateOwner),
					__cuiStaticTemplateOwner1_template_StaticAuthorChrome
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
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, Control::BackgroundProperty(), BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1_COLOR_F{0.909804f, 0.960784f, 0.933333f, 1.f}; return value; }()), DependencyPropertyValueSource::Template);
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
		{ DeclarativeAnimationKind::Double, 0u, CompiledInteractionInvalidIndex, 2u, 3u, CompiledInteractionInvalidIndex, { 0u, 0u }, false, false, 0ULL, 100ULL, DeclarativeRepeatBehaviorKind::Count, 1.0, 0ULL, false, DeclarativeTimelineFillBehavior::HoldEnd, 1.0, 0.0, 0.0, DeclarativeEasingKind::Linear, DeclarativeEasingMode::EaseOut }
	};
	static constexpr CompiledInteractionStoryboardOp __styleSheet_program_storyboards[] = {
		{ { 0u, 1u } }
	};
	static constexpr CompiledInteractionActionOp __styleSheet_program_actions[] = {
		{ DeclarativeStoryboardActionKind::Begin, 0u },
		{ DeclarativeStoryboardActionKind::Stop, 0u }
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
			std::span<const CompiledInteractionAnimationOp>{ __styleSheet_program_animations }, // Animations
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
