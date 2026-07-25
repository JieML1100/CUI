#include "NamespacedWindow.g.h"
#include "XamlInfrastructure.h"
#include "DependencyPropertyInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "XamlFrameworkTheme.h"
#include "HeaderedContentControl.h"
#include "HeaderedItemsControl.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

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
	// namespaceButton_template_PART_Chrome
	auto __owned_namespaceButton_template_PART_Chrome = std::make_unique<Border>();
	auto* namespaceButton_template_PART_Chrome = __owned_namespaceButton_template_PART_Chrome.get();
	(void)namespaceButton_template_PART_Chrome->ClearPropertyValues();
	static const auto __xamlType_namespaceButton_template_PART_Chrome = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"Border" }, {});
	if (!__xamlType_namespaceButton_template_PART_Chrome || !namespaceButton_template_PART_Chrome || !cui::framework::XamlAccess::SetTypeDescriptor(*namespaceButton_template_PART_Chrome, __xamlType_namespaceButton_template_PART_Chrome))
		throw std::runtime_error("Generated XAML type attachment failed");
	(void)cui::framework::DependencyPropertyAccess::SetValue(*namespaceButton_template_PART_Chrome, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);
	// namespaceButton_template_PART_ContentPresenter
	auto __owned_namespaceButton_template_PART_ContentPresenter = std::make_unique<ContentPresenter>();
	auto* namespaceButton_template_PART_ContentPresenter = __owned_namespaceButton_template_PART_ContentPresenter.get();
	(void)namespaceButton_template_PART_ContentPresenter->ClearPropertyValues();
	static const auto __xamlType_namespaceButton_template_PART_ContentPresenter = DeclarativeTypeDescriptor::Create(
		RuntimeTypeId{ L"urn:cui", L"ContentPresenter" }, {});
	if (!__xamlType_namespaceButton_template_PART_ContentPresenter || !namespaceButton_template_PART_ContentPresenter || !cui::framework::XamlAccess::SetTypeDescriptor(*namespaceButton_template_PART_ContentPresenter, __xamlType_namespaceButton_template_PART_ContentPresenter))
		throw std::runtime_error("Generated XAML type attachment failed");
	(void)cui::framework::DependencyPropertyAccess::SetValue(*namespaceButton_template_PART_ContentPresenter, L"Focusable", BindingValue(false), DependencyPropertyValueSource::Theme);

	// Establish the ControlTemplate namescope before properties/bindings.
	cui::framework::XamlAccess::SetTemplatedParent(*namespaceButton_template_PART_Chrome, namespaceButton);
	if (!cui::framework::XamlAccess::RegisterTemplatePart(*namespaceButton, L"PART_Chrome", namespaceButton_template_PART_Chrome))
		throw std::runtime_error("Generated ControlTemplate part registration failed");
	cui::framework::XamlAccess::SetTemplatedParent(*namespaceButton_template_PART_ContentPresenter, namespaceButton);
	if (!cui::framework::XamlAccess::RegisterTemplatePart(*namespaceButton, L"PART_ContentPresenter", namespaceButton_template_PART_ContentPresenter))
		throw std::runtime_error("Generated ControlTemplate part registration failed");
	{
		auto* __contentOwner_namespaceButton_template_PART_ContentPresenter = dynamic_cast<ContentControl*>(namespaceButton);
		auto* __contentPresenter_namespaceButton_template_PART_ContentPresenter = dynamic_cast<ContentPresenter*>(namespaceButton_template_PART_ContentPresenter);
		if (!__contentOwner_namespaceButton_template_PART_ContentPresenter || !__contentPresenter_namespaceButton_template_PART_ContentPresenter || !cui::framework::TemplateAccess::RegisterContentPresenter(*__contentOwner_namespaceButton_template_PART_ContentPresenter, __contentPresenter_namespaceButton_template_PART_ContentPresenter))
			throw std::runtime_error("Generated ContentPresenter registration failed");
	}

	namespaceButton->CommandTarget = this;
	// XAML authored Local properties/resources
	(void)namespaceButton->TrySetPropertyValue(L"Width", BindingValue(cui::layout::Length::Fixed(120.f)));
	(void)namespaceButton->TrySetPropertyValue(L"Height", BindingValue(cui::layout::Length::Fixed(24.f)));
	(void)namespaceButton->TrySetPropertyValue(L"AllowDrop", BindingValue(true));
	(void)namespaceButton->TrySetPropertyValue(L"Command", BindingValue(L"Demo.Static.Refresh"));
	(void)namespaceButton->TrySetPropertyValue(L"CommandParameter", BindingValue(L"static-input"));



	if (!namespaceButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"Background", *namespaceButton, L"Background"))
		throw std::runtime_error("Generated TemplateBinding installation failed");
	if (!namespaceButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"BorderBrush", *namespaceButton, L"BorderBrush"))
		throw std::runtime_error("Generated TemplateBinding installation failed");
	if (!namespaceButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"BorderThickness", *namespaceButton, L"BorderThickness"))
		throw std::runtime_error("Generated TemplateBinding installation failed");
	if (!namespaceButton_template_PART_Chrome->DataBindings.AddTemplateBinding(L"Padding", *namespaceButton, L"Padding"))
		throw std::runtime_error("Generated TemplateBinding installation failed");
	if (!namespaceButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"Content", *namespaceButton, L"Content"))
		throw std::runtime_error("Generated TemplateBinding installation failed");
	if (!namespaceButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"ContentTemplate", *namespaceButton, L"ContentTemplate"))
		throw std::runtime_error("Generated TemplateBinding installation failed");
	if (!namespaceButton_template_PART_ContentPresenter->DataBindings.AddTemplateBinding(L"DisplayMemberPath", *namespaceButton, L"DisplayMemberPath"))
		throw std::runtime_error("Generated TemplateBinding installation failed");

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
	this->SetVisualContent(std::move(__owned_namespaceButton));
	cui::framework::TemplateAccess::SetTemplateRoot(*namespaceButton, std::move(__owned_namespaceButton_template_PART_Chrome));
	cui::framework::XamlAccess::SetLogicalParent(*namespaceButton_template_PART_Chrome, nullptr);
	namespaceButton_template_PART_Chrome->SetChild(std::move(__owned_namespaceButton_template_PART_ContentPresenter));



	std::wstring __frameworkThemeError;
	if (!CuiRuntime::XamlFrameworkTheme::Apply(*this, true, &__frameworkThemeError))
		throw std::runtime_error("Generated Generic.xaml theme installation failed");
	// XAML Window Local 属性/资源表达式
	(void)this->TrySetPropertyValue(L"Height", BindingValue(cui::layout::Length::Fixed(600.f)));
	(void)this->TrySetPropertyValue(L"Title", BindingValue(L"Acme::Views::MainWindow"));
	(void)this->TrySetPropertyValue(L"Width", BindingValue(cui::layout::Length::Fixed(800.f)));

	if (!CuiRuntime::XamlFrameworkTheme::ApplyTemplateVisualStates(*namespaceButton, L"CuiButtonTemplate", &__frameworkThemeError))
		throw std::runtime_error("Generated Generic.xaml visual-state installation failed");

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

