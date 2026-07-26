#include "NamespacedWindow.g.h"
#include "Border.h"
#include "Button.h"
#include "ContentPresenter.h"
#include "Layout/StackPanel.h"
#include "ControlTemplate.h"
#include "XamlInfrastructure.h"
#include "DependencyPropertyInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "XamlFrameworkTheme.h"
#include "HeaderedContentControl.h"
#include "HeaderedItemsControl.h"
#include "Style.h"
#include "Resource.h"
#include <functional>
#include <memory>
#include <stdexcept>
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
			if (!cui::framework::XamlAccess::SetTemplate(
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

	static const auto __xamlType_this = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"Window" }, {});
	if (!__xamlType_this || !cui::framework::XamlAccess::SetTypeDescriptor(*this, __xamlType_this))
		throw std::runtime_error("Generated XAML type attachment failed");

	// 创建控件
	// staticRoot
	auto __owned_staticRoot = std::make_unique<StackPanel>();
	staticRoot = __owned_staticRoot.get();
	(void)staticRoot->ClearPropertyValues();
	static const auto __xamlType_staticRoot = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"StackPanel" }, {});
	if (!__xamlType_staticRoot || !staticRoot || !cui::framework::XamlAccess::SetTypeDescriptor(*staticRoot, __xamlType_staticRoot))
		throw std::runtime_error("Generated XAML type attachment failed");
	(void)cui::framework::DependencyPropertyAccess::SetValue(*staticRoot, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
	cui::framework::DesignIdentityAccess::Set(*staticRoot, 1);
	// namespaceButton
	auto __owned_namespaceButton = std::make_unique<Button>();
	namespaceButton = __owned_namespaceButton.get();
	(void)namespaceButton->ClearPropertyValues();
	static const auto __xamlType_namespaceButton = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"Button" }, {});
	if (!__xamlType_namespaceButton || !namespaceButton || !cui::framework::XamlAccess::SetTypeDescriptor(*namespaceButton, __xamlType_namespaceButton))
		throw std::runtime_error("Generated XAML type attachment failed");
	(void)cui::framework::DependencyPropertyAccess::SetValue(*namespaceButton, L"Focusable", BindingValue(true), DependencyPropertyValueSource::Theme);
	cui::framework::DesignIdentityAccess::Set(*namespaceButton, 77);
	// authorTemplateButton
	auto __owned_authorTemplateButton = std::make_unique<Button>();
	authorTemplateButton = __owned_authorTemplateButton.get();
	(void)authorTemplateButton->ClearPropertyValues();
	static const auto __xamlType_authorTemplateButton = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"Button" }, {});
	if (!__xamlType_authorTemplateButton || !authorTemplateButton || !cui::framework::XamlAccess::SetTypeDescriptor(*authorTemplateButton, __xamlType_authorTemplateButton))
		throw std::runtime_error("Generated XAML type attachment failed");
	(void)cui::framework::DependencyPropertyAccess::SetValue(*authorTemplateButton, L"Focusable", BindingValue(true), DependencyPropertyValueSource::Theme);
	cui::framework::DesignIdentityAccess::Set(*authorTemplateButton, 78);
	// styleTemplateButton
	auto __owned_styleTemplateButton = std::make_unique<Button>();
	styleTemplateButton = __owned_styleTemplateButton.get();
	(void)styleTemplateButton->ClearPropertyValues();
	static const auto __xamlType_styleTemplateButton = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"Button" }, {});
	if (!__xamlType_styleTemplateButton || !styleTemplateButton || !cui::framework::XamlAccess::SetTypeDescriptor(*styleTemplateButton, __xamlType_styleTemplateButton))
		throw std::runtime_error("Generated XAML type attachment failed");
	(void)cui::framework::DependencyPropertyAccess::SetValue(*styleTemplateButton, L"Focusable", BindingValue(true), DependencyPropertyValueSource::Theme);
	cui::framework::DesignIdentityAccess::Set(*styleTemplateButton, 79);

	std::wstring __frameworkThemeError;

	// Repeatable pure-C++ factories for authored ControlTemplate resources.
	auto __controlTemplate_StaticAuthorButtonTemplate_1 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_Button, L"StaticAuthorButtonTemplate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<Button>();
			(void)result->ClearPropertyValues();
			static const auto descriptor = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"Button" }, {});
			if (!descriptor || !cui::framework::XamlAccess::SetTypeDescriptor(*result, descriptor))
				return {};
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, L"Focusable", BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_StaticAuthorButtonTemplate_1 = __controlTemplate_StaticAuthorButtonTemplate_1;
	auto __controlTemplate_StaticAuthorButtonTemplateAlternate_2 = std::make_shared<CuiGeneratedControlTemplate>(
		UIClass::UI_Button, L"StaticAuthorButtonTemplateAlternate", []() -> std::unique_ptr<Control>
		{
			auto result = std::make_unique<Button>();
			(void)result->ClearPropertyValues();
			static const auto descriptor = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"Button" }, {});
			if (!descriptor || !cui::framework::XamlAccess::SetTypeDescriptor(*result, descriptor))
				return {};
			(void)cui::framework::DependencyPropertyAccess::SetValue(*result, L"Focusable", BindingValue(true), DependencyPropertyValueSource::Theme);
			return result;
		});
	std::weak_ptr<const IControlTemplate> __weak_controlTemplate_StaticAuthorButtonTemplateAlternate_2 = __controlTemplate_StaticAuthorButtonTemplateAlternate_2;

	__controlTemplate_StaticAuthorButtonTemplate_1->SetApplyCallback([this, __weak_controlTemplate_StaticAuthorButtonTemplate_1, __weak_controlTemplate_StaticAuthorButtonTemplateAlternate_2](Control& __templateOwner, std::wstring* outError) -> bool
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
			static const auto __xamlType___cuiStaticTemplateOwner1_template_StaticAuthorChrome = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"Border" }, {});
			if (!__xamlType___cuiStaticTemplateOwner1_template_StaticAuthorChrome || !__cuiStaticTemplateOwner1_template_StaticAuthorChrome || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, __xamlType___cuiStaticTemplateOwner1_template_StaticAuthorChrome))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner1_template_stackPanel1
			auto __owned___cuiStaticTemplateOwner1_template_stackPanel1 = std::make_unique<StackPanel>();
			auto* __cuiStaticTemplateOwner1_template_stackPanel1 = __owned___cuiStaticTemplateOwner1_template_stackPanel1.get();
			(void)__cuiStaticTemplateOwner1_template_stackPanel1->ClearPropertyValues();
			static const auto __xamlType___cuiStaticTemplateOwner1_template_stackPanel1 = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"StackPanel" }, {});
			if (!__xamlType___cuiStaticTemplateOwner1_template_stackPanel1 || !__cuiStaticTemplateOwner1_template_stackPanel1 || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner1_template_stackPanel1, __xamlType___cuiStaticTemplateOwner1_template_stackPanel1))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_stackPanel1, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner1_template_StaticAuthorPresenter
			auto __owned___cuiStaticTemplateOwner1_template_StaticAuthorPresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner1_template_StaticAuthorPresenter = __owned___cuiStaticTemplateOwner1_template_StaticAuthorPresenter.get();
			(void)__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->ClearPropertyValues();
			static const auto __xamlType___cuiStaticTemplateOwner1_template_StaticAuthorPresenter = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"ContentPresenter" }, {});
			if (!__xamlType___cuiStaticTemplateOwner1_template_StaticAuthorPresenter || !__cuiStaticTemplateOwner1_template_StaticAuthorPresenter || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner1_template_StaticAuthorPresenter, __xamlType___cuiStaticTemplateOwner1_template_StaticAuthorPresenter))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorPresenter, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner1_template_StaticNestedButton
			auto __owned___cuiStaticTemplateOwner1_template_StaticNestedButton = std::make_unique<Button>();
			auto* __cuiStaticTemplateOwner1_template_StaticNestedButton = __owned___cuiStaticTemplateOwner1_template_StaticNestedButton.get();
			(void)__cuiStaticTemplateOwner1_template_StaticNestedButton->ClearPropertyValues();
			static const auto __xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"Button" }, {});
			if (!__xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton || !__cuiStaticTemplateOwner1_template_StaticNestedButton || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner1_template_StaticNestedButton, __xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton, L"Focusable", BindingValue(true), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome
			auto __owned___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome = std::make_unique<Border>();
			auto* __cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome = __owned___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome.get();
			(void)__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome->ClearPropertyValues();
			static const auto __xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"Border" }, {});
			if (!__xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome || !__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome, __xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter
			auto __owned___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter = __owned___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter.get();
			(void)__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter->ClearPropertyValues();
			static const auto __xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"ContentPresenter" }, {});
			if (!__xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter || !__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter, __xamlType___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);

			if (!CuiRuntime::XamlFrameworkTheme::InstallTemplateValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton, L"CuiButtonTemplate", &__templateThemeError))
				return fail(L"嵌套 Generic.xaml Template 安装失败：" + __templateThemeError);

			// Establish a fresh template namescope for this application.
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, &__templateOwner);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(__templateOwner, L"StaticAuthorChrome", __cuiStaticTemplateOwner1_template_StaticAuthorChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_stackPanel1, &__templateOwner);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(__templateOwner, L"stackPanel1", __cuiStaticTemplateOwner1_template_stackPanel1))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticAuthorPresenter, &__templateOwner);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(__templateOwner, L"StaticAuthorPresenter", __cuiStaticTemplateOwner1_template_StaticAuthorPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner1_template_StaticAuthorPresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticNestedButton, &__templateOwner);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(__templateOwner, L"StaticNestedButton", __cuiStaticTemplateOwner1_template_StaticNestedButton))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome, __cuiStaticTemplateOwner1_template_StaticNestedButton);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(*__cuiStaticTemplateOwner1_template_StaticNestedButton, L"PART_Chrome", __cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter, __cuiStaticTemplateOwner1_template_StaticNestedButton);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(*__cuiStaticTemplateOwner1_template_StaticNestedButton, L"PART_ContentPresenter", __cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(__cuiStaticTemplateOwner1_template_StaticNestedButton);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, L"Background", BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1::ColorF(0.2f, 0.4f, 0.6f, 1.f); return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, L"BorderThickness", BindingValue(Thickness(2.f, 2.f, 2.f, 2.f)), DependencyPropertyValueSource::Template);
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner1_template_StaticNestedButton, L"Content", BindingValue(L"nested Generic.xaml host"), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorChrome->DataBindings.AddTemplateBinding(L"Padding", __templateOwner, L"Padding"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->DataBindings.AddTemplateBinding(L"Content", __templateOwner, L"Content"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->DataBindings.AddTemplateBinding(L"ContentTemplate", __templateOwner, L"ContentTemplate"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticAuthorPresenter->DataBindings.AddTemplateBinding(L"DisplayMemberPath", __templateOwner, L"DisplayMemberPath"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"Background", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"Background"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"BorderBrush", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"BorderBrush"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"BorderThickness", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"BorderThickness"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"Padding", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"Padding"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"Content", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"Content"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"ContentTemplate", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"ContentTemplate"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"DisplayMemberPath", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"DisplayMemberPath"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"HorizontalAlignment", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"HorizontalContentAlignment"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"VerticalAlignment", *__cuiStaticTemplateOwner1_template_StaticNestedButton, L"VerticalContentAlignment"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner1_template_StaticAuthorChrome));
			cui::framework::XamlAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticAuthorChrome, nullptr);
			__cuiStaticTemplateOwner1_template_StaticAuthorChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner1_template_stackPanel1));
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticAuthorPresenter));
			__cuiStaticTemplateOwner1_template_stackPanel1->AddOwned(std::move(__owned___cuiStaticTemplateOwner1_template_StaticNestedButton));
			cui::framework::TemplateAccess::SetTemplateRoot(*__cuiStaticTemplateOwner1_template_StaticNestedButton, std::move(__owned___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome));
			cui::framework::XamlAccess::SetLogicalParent(*__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome, nullptr);
			__cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_Chrome->SetChild(std::move(__owned___cuiStaticTemplateOwner1_template_StaticNestedButton_template_PART_ContentPresenter));
			if (!CuiRuntime::XamlFrameworkTheme::Apply(__templateOwner, true, &__templateThemeError))
				return fail(L"ControlTemplate 子树主题应用失败：" + __templateThemeError);
			if (auto documentStyles = cui::framework::StyleAccess::DocumentStyles(__templateOwner);
				documentStyles && !cui::framework::StyleAccess::SetDocumentStyles(__templateOwner, std::move(documentStyles), true))
				return fail(L"ControlTemplate 子树文档样式应用失败。");
			{
				std::vector<DeclarativeVisualStateGroupDefinition> visualStateGroups;
				std::vector<DeclarativeEventTriggerDefinition> eventTriggers;
				{
					DeclarativeVisualStateGroupDefinition group;
					group.Name = L"AuthorCommonStates";
					{
						DeclarativeVisualStateDefinition state;
						state.Name = L"PointerOver";
						state.Conditions.push_back({ L"IsMouseOver", BindingValue(true) });
						state.Setters.push_back({ L"StaticAuthorChrome", L"Background", BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1::ColorF(0.917647f, 0.94902f, 1.f, 1.f); return value; }()) });
						{
							DeclarativeVisualStateAnimation animation;
							animation.Kind = DeclarativeAnimationKind::Double;
							animation.TargetName = L"StaticAuthorChrome";
							animation.PropertyName = L"Canvas.Top";
							animation.To = BindingValue(4.f);
							animation.IsAdditive = false;
							animation.IsCumulative = false;
							animation.BeginTimeMilliseconds = 0ULL;
							animation.DurationMilliseconds = 80ULL;
							animation.RepeatBehavior = DeclarativeRepeatBehaviorKind::Count;
							animation.RepeatCount = 1.0;
							animation.RepeatDurationMilliseconds = 0ULL;
							animation.AutoReverse = false;
							animation.FillBehavior = DeclarativeTimelineFillBehavior::HoldEnd;
							animation.SpeedRatio = 1.0;
							animation.AccelerationRatio = 0.0;
							animation.DecelerationRatio = 0.0;
							animation.Easing = DeclarativeEasingKind::Linear;
							animation.EasingMode = DeclarativeEasingMode::EaseOut;
							state.Animations.push_back(std::move(animation));
						}
						group.States.push_back(std::move(state));
					}
					{
						DeclarativeVisualStateDefinition state;
						state.Name = L"Normal";
						state.Setters.push_back({ L"StaticAuthorChrome", L"Canvas.Top", BindingValue(0.f) });
						group.States.push_back(std::move(state));
					}
					{
						DeclarativeVisualTransitionDefinition transition;
						transition.FromState = L"Normal";
						transition.ToState = L"PointerOver";
						transition.GeneratedDurationMilliseconds = 120ULL;
						transition.GeneratedEasing = DeclarativeEasingKind::Quadratic;
						transition.GeneratedEasingMode = DeclarativeEasingMode::EaseInOut;
						group.Transitions.push_back(std::move(transition));
					}
					visualStateGroups.push_back(std::move(group));
				}
				{
					DeclarativeEventTriggerDefinition trigger;
					trigger.EventName = L"Click";
					{
						DeclarativeEventTriggerActionDefinition action;
						action.Kind = DeclarativeStoryboardActionKind::Begin;
						action.StoryboardName = L"StaticAuthorClickClock";
						{
							DeclarativeVisualStateAnimation animation;
							animation.Kind = DeclarativeAnimationKind::Double;
							animation.TargetName = L"StaticAuthorChrome";
							animation.PropertyName = L"Canvas.Left";
							animation.From = BindingValue(0.f);
							animation.To = BindingValue(30.f);
							animation.IsAdditive = false;
							animation.IsCumulative = false;
							animation.BeginTimeMilliseconds = 0ULL;
							animation.DurationMilliseconds = 200ULL;
							animation.RepeatBehavior = DeclarativeRepeatBehaviorKind::Count;
							animation.RepeatCount = 1.0;
							animation.RepeatDurationMilliseconds = 0ULL;
							animation.AutoReverse = true;
							animation.FillBehavior = DeclarativeTimelineFillBehavior::HoldEnd;
							animation.SpeedRatio = 1.0;
							animation.AccelerationRatio = 0.0;
							animation.DecelerationRatio = 0.0;
							animation.Easing = DeclarativeEasingKind::Linear;
							animation.EasingMode = DeclarativeEasingMode::EaseOut;
							action.Animations.push_back(std::move(animation));
						}
						trigger.Actions.push_back(std::move(action));
					}
					eventTriggers.push_back(std::move(trigger));
				}
				std::wstring interactionError;
				if (!cui::framework::XamlAccess::DefineInteractions(__templateOwner, std::move(visualStateGroups), std::move(eventTriggers), &interactionError))
					return fail(L"ControlTemplate 声明交互安装失败：" + interactionError);
			}
			if (!CuiRuntime::XamlFrameworkTheme::ApplyTemplateVisualStates(*__cuiStaticTemplateOwner1_template_StaticNestedButton, L"CuiButtonTemplate", &__templateThemeError))
				return fail(L"嵌套 Generic.xaml VisualState 安装失败：" + __templateThemeError);
			cui::framework::TemplateAccess::CompleteTemplateApplication(*__cuiStaticTemplateOwner1_template_StaticNestedButton);
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

	__controlTemplate_StaticAuthorButtonTemplateAlternate_2->SetApplyCallback([this, __weak_controlTemplate_StaticAuthorButtonTemplate_1, __weak_controlTemplate_StaticAuthorButtonTemplateAlternate_2](Control& __templateOwner, std::wstring* outError) -> bool
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
			static const auto __xamlType___cuiStaticTemplateOwner2_template_StaticAlternateChrome = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"Border" }, {});
			if (!__xamlType___cuiStaticTemplateOwner2_template_StaticAlternateChrome || !__cuiStaticTemplateOwner2_template_StaticAlternateChrome || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, __xamlType___cuiStaticTemplateOwner2_template_StaticAlternateChrome))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
			// __cuiStaticTemplateOwner2_template_StaticAlternatePresenter
			auto __owned___cuiStaticTemplateOwner2_template_StaticAlternatePresenter = std::make_unique<ContentPresenter>();
			auto* __cuiStaticTemplateOwner2_template_StaticAlternatePresenter = __owned___cuiStaticTemplateOwner2_template_StaticAlternatePresenter.get();
			(void)__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->ClearPropertyValues();
			static const auto __xamlType___cuiStaticTemplateOwner2_template_StaticAlternatePresenter = DeclarativeTypeDescriptor::Create(
				RuntimeTypeId{ L"urn:cui", L"ContentPresenter" }, {});
			if (!__xamlType___cuiStaticTemplateOwner2_template_StaticAlternatePresenter || !__cuiStaticTemplateOwner2_template_StaticAlternatePresenter || !cui::framework::XamlAccess::SetTypeDescriptor(*__cuiStaticTemplateOwner2_template_StaticAlternatePresenter, __xamlType___cuiStaticTemplateOwner2_template_StaticAlternatePresenter))
				throw std::runtime_error("Generated XAML type attachment failed");
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternatePresenter, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);

			// Establish a fresh template namescope for this application.
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, &__templateOwner);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(__templateOwner, L"StaticAlternateChrome", __cuiStaticTemplateOwner2_template_StaticAlternateChrome))
				return fail(L"ControlTemplate 部件注册失败。");
			cui::framework::XamlAccess::SetTemplatedParent(*__cuiStaticTemplateOwner2_template_StaticAlternatePresenter, &__templateOwner);
			if (!cui::framework::XamlAccess::RegisterTemplatePart(__templateOwner, L"StaticAlternatePresenter", __cuiStaticTemplateOwner2_template_StaticAlternatePresenter))
				return fail(L"ControlTemplate 部件注册失败。");
			{
				auto* contentOwner = dynamic_cast<ContentControl*>(&__templateOwner);
				auto* presenter = dynamic_cast<ContentPresenter*>(__cuiStaticTemplateOwner2_template_StaticAlternatePresenter);
				if (!contentOwner || !presenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*contentOwner, presenter))
					return fail(L"ControlTemplate ContentPresenter 注册失败。");
			}
			// ControlTemplate-authored properties/resources
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, L"Background", BindingValue([] { cui::drawing::Brush value; value.Kind = cui::drawing::BrushKind::Solid; value.MappingMode = cui::drawing::BrushMappingMode::RelativeToBoundingBox; value.Opacity = 1.f; value.Color = D2D1::ColorF(0.909804f, 0.960784f, 0.933333f, 1.f); return value; }()), DependencyPropertyValueSource::Template);
			(void)cui::framework::DependencyPropertyAccess::SetValue(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, L"BorderThickness", BindingValue(Thickness(3.f, 3.f, 3.f, 3.f)), DependencyPropertyValueSource::Template);
			if (!__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->DataBindings.AddTemplateBinding(L"Content", __templateOwner, L"Content"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->DataBindings.AddTemplateBinding(L"ContentTemplate", __templateOwner, L"ContentTemplate"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			if (!__cuiStaticTemplateOwner2_template_StaticAlternatePresenter->DataBindings.AddTemplateBinding(L"DisplayMemberPath", __templateOwner, L"DisplayMemberPath"))
				return fail(L"ControlTemplate TemplateBinding 安装失败。");
			cui::framework::TemplateAccess::SetTemplateRoot(__templateOwner, std::move(__owned___cuiStaticTemplateOwner2_template_StaticAlternateChrome));
			cui::framework::XamlAccess::SetLogicalParent(*__cuiStaticTemplateOwner2_template_StaticAlternateChrome, nullptr);
			__cuiStaticTemplateOwner2_template_StaticAlternateChrome->SetChild(std::move(__owned___cuiStaticTemplateOwner2_template_StaticAlternatePresenter));
			if (!CuiRuntime::XamlFrameworkTheme::Apply(__templateOwner, true, &__templateThemeError))
				return fail(L"ControlTemplate 子树主题应用失败：" + __templateThemeError);
			if (auto documentStyles = cui::framework::StyleAccess::DocumentStyles(__templateOwner);
				documentStyles && !cui::framework::StyleAccess::SetDocumentStyles(__templateOwner, std::move(documentStyles), true))
				return fail(L"ControlTemplate 子树文档样式应用失败。");
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
	(void)staticRoot->TrySetPropertyValue(L"Orientation", BindingValue(1));

	namespaceButton->CommandTarget = this;
	// XAML authored Local properties/resources
	(void)namespaceButton->TrySetPropertyValue(L"Width", BindingValue(cui::layout::Length::Fixed(120.f)));
	(void)namespaceButton->TrySetPropertyValue(L"Height", BindingValue(cui::layout::Length::Fixed(24.f)));
	(void)namespaceButton->TrySetPropertyValue(L"AllowDrop", BindingValue(true));
	(void)namespaceButton->TrySetPropertyValue(L"Command", BindingValue(L"Demo.Static.Refresh"));
	(void)namespaceButton->TrySetPropertyValue(L"CommandParameter", BindingValue(L"static-input"));

	// XAML authored Local properties/resources
	(void)authorTemplateButton->TrySetPropertyValue(L"Content", BindingValue(L"author template"));
	(void)authorTemplateButton->TrySetPropertyValue(L"Width", BindingValue(cui::layout::Length::Fixed(260.f)));
	(void)authorTemplateButton->TrySetPropertyValue(L"Height", BindingValue(cui::layout::Length::Fixed(72.f)));
	(void)authorTemplateButton->TrySetPropertyValue(L"Padding", BindingValue(Thickness(6.f, 6.f, 6.f, 6.f)));

	cui::framework::StyleAccess::SetResourceKey(*styleTemplateButton, L"StaticAuthorButtonStyle");
	// XAML authored Local properties/resources
	(void)styleTemplateButton->TrySetPropertyValue(L"Content", BindingValue(L"Style.Template"));
	(void)styleTemplateButton->TrySetPropertyValue(L"Width", BindingValue(cui::layout::Length::Fixed(220.f)));
	(void)styleTemplateButton->TrySetPropertyValue(L"Height", BindingValue(cui::layout::Length::Fixed(48.f)));

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


	if (!CuiRuntime::XamlFrameworkTheme::Apply(*this, true, &__frameworkThemeError))
		throw std::runtime_error("Generated Generic.xaml theme installation failed");
	// 文档级控件样式
	auto __styleSheet = std::make_shared<ControlStyleSheet>();
	__styleSheet->SetResource(L"StaticTemplateAccent", BindingValue(D2D1::ColorF(0.2f, 0.4f, 0.6f, 1.f)));
	__styleSheet->SetResource(L"StaticAuthorButtonTemplate", BindingValue(ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplate_1)));
	__styleSheet->SetResource(L"StaticAuthorButtonTemplateAlternate", BindingValue(ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplateAlternate_2)));
	ControlStyleSelector __styleSelector1;
	__styleSelector1.Type = UIClass::UI_Button;
	__styleSelector1.DeclarativeTypeNamespace = L"urn:cui";
	__styleSelector1.DeclarativeTypeName = L"Button";
	__styleSelector1.StyleResourceKey = L"StaticAuthorButtonStyle";
	__styleSheet->AddRule(std::move(__styleSelector1), {
		ControlStyleSetter::Resource(L"Template", L"StaticAuthorButtonTemplate"),
		ControlStyleSetter(L"FontSize", BindingValue(14.0))
	});
	ControlStyleSelector __styleSelector2;
	__styleSelector2.Type = UIClass::UI_Button;
	__styleSelector2.DeclarativeTypeNamespace = L"urn:cui";
	__styleSelector2.DeclarativeTypeName = L"Button";
	__styleSelector2.StyleResourceKey = L"StaticAuthorButtonStyle";
	__styleSelector2.PropertyConditions.push_back({ L"IsMouseOver", BindingValue(true) });
	std::vector<DeclarativeEventTriggerActionDefinition> __styleEnterActions2;
	{
		DeclarativeEventTriggerActionDefinition action;
		action.Kind = DeclarativeStoryboardActionKind::Begin;
		action.StoryboardName = L"StaticStyleHoverClock";
		{
			DeclarativeVisualStateAnimation animation;
			animation.Kind = DeclarativeAnimationKind::Double;
			animation.TargetName = L"";
			animation.PropertyName = L"FontSize";
			animation.From = BindingValue(14.0);
			animation.To = BindingValue(18.0);
			animation.IsAdditive = false;
			animation.IsCumulative = false;
			animation.BeginTimeMilliseconds = 0ULL;
			animation.DurationMilliseconds = 100ULL;
			animation.RepeatBehavior = DeclarativeRepeatBehaviorKind::Count;
			animation.RepeatCount = 1.0;
			animation.RepeatDurationMilliseconds = 0ULL;
			animation.AutoReverse = false;
			animation.FillBehavior = DeclarativeTimelineFillBehavior::HoldEnd;
			animation.SpeedRatio = 1.0;
			animation.AccelerationRatio = 0.0;
			animation.DecelerationRatio = 0.0;
			animation.Easing = DeclarativeEasingKind::Linear;
			animation.EasingMode = DeclarativeEasingMode::EaseOut;
			action.Animations.push_back(std::move(animation));
		}
		__styleEnterActions2.push_back(std::move(action));
	}
	std::vector<DeclarativeEventTriggerActionDefinition> __styleExitActions2;
	{
		DeclarativeEventTriggerActionDefinition action;
		action.Kind = DeclarativeStoryboardActionKind::Stop;
		action.StoryboardName = L"StaticStyleHoverClock";
		__styleExitActions2.push_back(std::move(action));
	}
	__styleSheet->AddRule(std::move(__styleSelector2), {
	}, std::move(__styleEnterActions2), std::move(__styleExitActions2));
	cui::framework::StyleAccess::SetDocumentStyles(*this, __styleSheet, true);

	if (!cui::framework::XamlAccess::SetTemplate(*authorTemplateButton, ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplate_1), DependencyPropertyValueSource::Local))
		throw std::runtime_error("Generated authored Control.Template installation failed");
	if (!cui::framework::XamlAccess::SetTemplate(*styleTemplateButton, ControlTemplateReference(__controlTemplate_StaticAuthorButtonTemplate_1), DependencyPropertyValueSource::Style))
		throw std::runtime_error("Generated authored Control.Template installation failed");

	// XAML Window Local 属性/资源表达式
	(void)this->TrySetPropertyValue(L"Height", BindingValue(cui::layout::Length::Fixed(600.f)));
	(void)this->TrySetPropertyValue(L"Title", BindingValue(L"Acme::Views::MainWindow"));
	(void)this->TrySetPropertyValue(L"Width", BindingValue(cui::layout::Length::Fixed(800.f)));

	if (staticRoot->GetTemplate())
	{
		(void)staticRoot->ApplyTemplate();
		if (!cui::framework::TemplateAccess::GetTemplateRoot(*staticRoot) || !staticRoot->LastTemplateError().empty())
			throw std::runtime_error("Generated ControlTemplate application failed");
	}
	if (namespaceButton->GetTemplate())
	{
		(void)namespaceButton->ApplyTemplate();
		if (!cui::framework::TemplateAccess::GetTemplateRoot(*namespaceButton) || !namespaceButton->LastTemplateError().empty())
			throw std::runtime_error("Generated ControlTemplate application failed");
	}
	if (authorTemplateButton->GetTemplate())
	{
		(void)authorTemplateButton->ApplyTemplate();
		if (!cui::framework::TemplateAccess::GetTemplateRoot(*authorTemplateButton) || !authorTemplateButton->LastTemplateError().empty())
			throw std::runtime_error("Generated ControlTemplate application failed");
	}
	if (styleTemplateButton->GetTemplate())
	{
		(void)styleTemplateButton->ApplyTemplate();
		if (!cui::framework::TemplateAccess::GetTemplateRoot(*styleTemplateButton) || !styleTemplateButton->LastTemplateError().empty())
			throw std::runtime_error("Generated ControlTemplate application failed");
	}

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
		cuiBindingAttached = namespaceButton->DataBindings.Add(L"Content", namespaceButton->DataContextSource(), L"Caption", BindingMode::Default, DataSourceUpdateMode::Default) != nullptr;
		if (!cuiBindingAttached)
		{
			success = false;
		}
	}
	return success;
}

void Acme::Views::MainWindowGenerated::HandleWindowContentRendered(Window* sender)
{
	(void)sender;
}

void Acme::Views::MainWindowGenerated::HandleStaticRefreshCanExecute(Control* sender, CanExecuteRoutedEventArgs& e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindowGenerated::HandleStaticRefreshExecuted(Control* sender, ExecutedRoutedEventArgs& e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindowGenerated::HandleNamespacedClick(Control* sender, RoutedEventArgs& e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindowGenerated::HandleNamespacedDrop(Control* sender, DragEventArgs& e)
{
	(void)sender;
	(void)e;
}

