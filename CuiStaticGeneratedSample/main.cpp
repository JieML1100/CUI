#include "NamespacedWindow.h"

#include <CuiRuntime.h>
#include <EventInfrastructure.h>
#include <InputInfrastructure.h>
#include <Canvas.h>
#include <PresentationInfrastructure.h>
#include <TemplateInfrastructure.h>

#include <iostream>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace
{
	class DynamicMainWindowEventSink final
		: public Acme::Views::MainWindowEventSink
	{
	public:
		int ClickCount = 0;
		int ContentRenderedCount = 0;

	private:
		void HandleNamespacedDrop(
			Control*, DragEventArgs&) override {}
		void HandleNamespacedClick(Control*, RoutedEventArgs&) override
		{
			++ClickCount;
		}
		void HandleWindowContentRendered(Window*) override
		{
			++ContentRenderedCount;
		}
		void HandleStaticRefreshCanExecute(
			Control*, CanExecuteRoutedEventArgs& e) override
		{
			e.CanExecute = true;
		}
		void HandleStaticRefreshExecuted(
			Control*, ExecutedRoutedEventArgs& e) override
		{
			e.Executed = true;
		}
	};
}

int wmain()
{
	Acme::Views::MainWindow window;
	auto staticDataContext = std::make_shared<ObservableObject>();
	staticDataContext->SetValue(
		L"Caption", std::wstring(L"Namespaced"));
	if (!window.BindData(BindingSourceReference(staticDataContext)))
	{
		std::wcerr << L"CUI static generated binding failed.\n";
		return 1;
	}
	auto* staticButton = window.GetNamespaceButton();
	auto* staticChrome = staticButton
		? staticButton->FindDeclarativeTemplatePart(L"PART_Chrome")
		: nullptr;
	auto* staticPresenter = staticButton
		? staticButton->FindDeclarativeTemplatePart(
			L"PART_ContentPresenter")
		: nullptr;
	if (!staticButton || staticButton->GetDisplayText() != L"Namespaced"
		|| !staticChrome || !staticPresenter
		|| !staticButton->GetTemplate()
		|| staticButton->GetPropertyValueSource(L"Template")
			!= DependencyPropertyValueSource::Theme
		|| cui::framework::TemplateAccess::GetTemplateRoot(*staticButton)
			!= staticChrome
		|| staticChrome->GetVisualParent() != staticButton
		|| staticChrome->GetLogicalParent() != nullptr
		|| staticChrome->GetTemplatedParent() != staticButton
		|| staticButton->GetPropertyValueSource(L"Background")
			!= DependencyPropertyValueSource::Theme
		|| staticChrome->GetPropertyValueSource(L"Background")
			!= DependencyPropertyValueSource::Template
		|| staticButton->GetCurrentVisualState(L"CommonStates") != L"Normal"
		|| staticButton->GetDesignId() != Acme::Views::MainWindowGenerated::
			ControlIds::namespaceButton
		|| staticButton->GetDataContext().Get() != staticDataContext.get()
		|| !staticButton->HasAuthoredCommandTarget()
		|| staticButton->CommandTarget != &window
		|| staticButton->GetPropertyValueSource(L"DataContext")
			!= DependencyPropertyValueSource::Inherited)
	{
		std::wcerr << L"CUI static generated sample failed:"
			<< L" button=" << (staticButton != nullptr)
			<< L", text=" << (staticButton
				? staticButton->GetDisplayText() : L"<null>")
			<< L", chrome=" << (staticChrome != nullptr)
			<< L", presenter=" << (staticPresenter != nullptr)
			<< L", templateRoot=" << (staticButton
				&& cui::framework::TemplateAccess::GetTemplateRoot(*staticButton)
					== staticChrome)
			<< L", visualParent=" << (staticChrome
				&& staticChrome->GetVisualParent() == staticButton)
			<< L", logicalParent=" << (staticChrome
				&& staticChrome->GetLogicalParent() == nullptr)
			<< L", templatedParent=" << (staticChrome
				&& staticChrome->GetTemplatedParent() == staticButton)
			<< L", buttonBackgroundSource="
			<< (staticButton ? static_cast<int>(
				staticButton->GetPropertyValueSource(L"Background")) : -1)
			<< L", chromeBackgroundSource="
			<< (staticChrome ? static_cast<int>(
				staticChrome->GetPropertyValueSource(L"Background")) : -1)
			<< L", state=" << (staticButton
				? staticButton->GetCurrentVisualState(L"CommonStates")
				: L"<null>")
			<< L", designId=" << (staticButton
				? staticButton->GetDesignId() : -1)
			<< L", dataContext=" << (staticButton
				&& staticButton->GetDataContext().Get()
					== staticDataContext.get())
			<< L", commandTargetAuthored=" << (staticButton
				&& staticButton->HasAuthoredCommandTarget())
			<< L", commandTarget=" << (staticButton
				&& staticButton->CommandTarget == &window)
			<< L", dataContextSource="
			<< (staticButton ? static_cast<int>(
				staticButton->GetPropertyValueSource(L"DataContext")) : -1)
			<< L'\n';
			return 1;
		}

	auto* authorTemplateButton = window.GetAuthorTemplateButton();
	auto* styleTemplateButton = window.GetStyleTemplateButton();
	auto* authorChrome = authorTemplateButton
		? authorTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAuthorChrome")
		: nullptr;
	auto* authorPresenter = authorTemplateButton
		? authorTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAuthorPresenter")
		: nullptr;
	auto* nestedButton = authorTemplateButton
		? dynamic_cast<Button*>(
			authorTemplateButton->FindDeclarativeTemplatePart(
				L"StaticNestedButton"))
		: nullptr;
	auto* styledChrome = styleTemplateButton
		? styleTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAuthorChrome")
		: nullptr;
	if (!authorTemplateButton || !styleTemplateButton
		|| !authorChrome || !authorPresenter || !nestedButton
		|| !nestedButton->FindDeclarativeTemplatePart(L"PART_Chrome")
		|| !styledChrome
		|| authorTemplateButton->GetPropertyValueSource(L"Template")
			!= DependencyPropertyValueSource::Local
		|| styleTemplateButton->GetPropertyValueSource(L"Template")
			!= DependencyPropertyValueSource::Style
		|| authorChrome->GetPropertyValueSource(L"Padding")
			!= DependencyPropertyValueSource::Template
		|| cui::framework::TemplateAccess::GetTemplateRoot(
			*authorTemplateButton) != authorChrome
		|| cui::framework::TemplateAccess::GetTemplateRoot(
			*styleTemplateButton) != styledChrome)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate did not initialize.\n";
		return 1;
	}
	cui::framework::InputAccess::PublishPointerOverState(
		*styleTemplateButton, true, true);
	const auto stylePointerTick = ::GetTickCount64();
	if (!styleTemplateButton->HasActiveVisualStateAnimations()
		|| !cui::framework::PresentationAccess::
			AdvanceVisualStateAnimations(
				*styleTemplateButton, stylePointerTick + 200)
		|| std::abs(styleTemplateButton->FontSize - 18.0f) > 0.01f
		|| styleTemplateButton->GetPropertyValueSource(L"FontSize")
			!= DependencyPropertyValueSource::Animation)
	{
		std::wcerr
			<< L"Generated static Style Trigger.EnterActions "
				L"did not run its Storyboard.\n";
		return 1;
	}
	cui::framework::InputAccess::PublishPointerOverState(
		*styleTemplateButton, false, false);
	if (std::abs(styleTemplateButton->FontSize - 14.0f) > 0.01f
		|| styleTemplateButton->GetPropertyValueSource(L"FontSize")
			!= DependencyPropertyValueSource::Style)
	{
		std::wcerr
			<< L"Generated static Style Trigger.ExitActions "
				L"did not stop and restore its Storyboard.\n";
		return 1;
	}
	if (authorTemplateButton->GetCurrentVisualState(
			L"AuthorCommonStates") != L"Normal"
		|| std::abs(Canvas::GetTop(*authorChrome)) > 0.001f)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate did not enter "
				L"its fallback VisualState.\n";
		return 1;
	}
	cui::framework::InputAccess::PublishPointerOverState(
		*authorTemplateButton, true, true);
	const auto authorPointerTick = ::GetTickCount64();
	if (authorTemplateButton->GetCurrentVisualState(
			L"AuthorCommonStates") != L"PointerOver"
		|| !authorTemplateButton->HasActiveVisualStateAnimations()
		|| !cui::framework::PresentationAccess::
			AdvanceVisualStateAnimations(
				*authorTemplateButton, authorPointerTick + 300)
		|| std::abs(Canvas::GetTop(*authorChrome) - 4.0f) > 0.01f
		|| authorChrome->GetPropertyValueSource(L"Background")
			!= DependencyPropertyValueSource::VisualState)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate VisualState/"
				L"Storyboard/Transition did not run.\n";
		return 1;
	}
	cui::framework::InputAccess::PublishPointerOverState(
		*authorTemplateButton, false, false);
	(void)cui::framework::PresentationAccess::
		AdvanceVisualStateAnimations(
			*authorTemplateButton, ::GetTickCount64() + 300);
	if (authorTemplateButton->GetCurrentVisualState(
			L"AuthorCommonStates") != L"Normal"
		|| std::abs(Canvas::GetTop(*authorChrome)) > 0.01f)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate did not leave "
				L"its animated VisualState.\n";
		return 1;
	}
	if (!authorTemplateButton->Invoke()
		|| !authorTemplateButton->HasActiveVisualStateAnimations())
	{
		std::wcerr
			<< L"Generated authored ControlTemplate routed EventTrigger "
				L"did not start its Storyboard.\n";
		return 1;
	}
	const auto authorClickTick = ::GetTickCount64();
	if (!cui::framework::PresentationAccess::
			AdvanceVisualStateAnimations(
				*authorTemplateButton, authorClickTick + 100)
		|| Canvas::GetLeft(*authorChrome) <= 0.0f
		|| Canvas::GetLeft(*authorChrome) >= 30.0f)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate EventTrigger "
				L"Storyboard did not advance.\n";
		return 1;
	}

	const auto firstAuthorTemplate =
		authorTemplateButton->GetTemplate();
	BindingValue alternateTemplateValue;
	ControlTemplateReference alternateAuthorTemplate;
	if (!authorTemplateButton->TryFindResource(
			L"StaticAuthorButtonTemplateAlternate",
			alternateTemplateValue)
		|| !alternateTemplateValue.TryGet(alternateAuthorTemplate)
		|| !alternateAuthorTemplate)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate resource is missing.\n";
		return 1;
	}
	const ControlWeakReference oldAuthorRoot(authorChrome);
	authorTemplateButton->SetTemplate(alternateAuthorTemplate);
	if (!authorTemplateButton->ApplyTemplate()
		|| oldAuthorRoot
		|| !authorTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAlternateChrome")
		|| authorTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAuthorChrome")
		|| authorTemplateButton->HasActiveVisualStateAnimations()
		|| !authorTemplateButton->LastTemplateError().empty())
	{
		std::wcerr
			<< L"Generated authored ControlTemplate did not swap in-place.\n";
		return 1;
	}
	authorTemplateButton->SetTemplate(firstAuthorTemplate);
	if (!authorTemplateButton->ApplyTemplate()
		|| !authorTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAuthorChrome")
		|| !authorTemplateButton->FindDeclarativeTemplatePart(
			L"StaticNestedButton")
		|| authorTemplateButton->GetCurrentVisualState(
			L"AuthorCommonStates") != L"Normal")
	{
		std::wcerr
			<< L"Generated authored ControlTemplate did not reapply.\n";
		return 1;
	}
	std::wstring builtTemplateError;
	auto builtTemplateHost =
		firstAuthorTemplate.Get()->Build(&builtTemplateError);
	auto* builtTemplateButton =
		dynamic_cast<Button*>(builtTemplateHost.get());
	if (!builtTemplateButton
		|| !builtTemplateButton->FindDeclarativeTemplatePart(
			L"StaticAuthorChrome")
		|| !builtTemplateButton->FindDeclarativeTemplatePart(
			L"StaticNestedButton")
		|| builtTemplateButton->GetCurrentVisualState(
			L"AuthorCommonStates") != L"Normal"
		|| !builtTemplateError.empty())
	{
		std::wcerr
			<< L"Generated authored ControlTemplate Build failed: "
			<< builtTemplateError << L'\n';
		return 1;
	}
	if (!builtTemplateButton->Invoke()
		|| !builtTemplateButton->HasActiveVisualStateAnimations())
	{
		std::wcerr
			<< L"Generated authored ControlTemplate Build lost "
				L"its routed EventTrigger.\n";
		return 1;
	}

		const auto generatedThemeTemplate = staticButton->GetTemplate();
		const ControlWeakReference generatedThemeRootLifetime(staticChrome);
		staticButton->SetTemplate({});
		if (generatedThemeRootLifetime.Get() != nullptr
			|| cui::framework::TemplateAccess::GetTemplateRoot(*staticButton))
		{
			std::wcerr << L"Generated Theme Template did not detach.\n";
			return 1;
		}
		staticButton->SetTemplate(generatedThemeTemplate);
		if (!staticButton->ApplyTemplate()
			|| !staticButton->FindDeclarativeTemplatePart(L"PART_Chrome")
			|| !staticButton->FindDeclarativeTemplatePart(
				L"PART_ContentPresenter")
			|| !staticButton->LastTemplateError().empty())
		{
			std::wcerr << L"Generated Theme Template did not reapply.\n";
			return 1;
		}
		staticDataContext->SetValue(
		L"Caption", std::wstring(L"Namespaced updated"));
	if (staticButton->GetDisplayText() != L"Namespaced updated")
	{
		std::wcerr << L"CUI static generated binding did not refresh.\n";
		return 1;
	}
	if (!cui::framework::InputAccess::ProcessCommandInput(
		*staticButton, KeyEventArgs(Key::F5))
		|| staticButton->Tag.ToString() != L"1")
	{
		std::wcerr << L"Generated CommandBinding RAII ownership or explicit CommandTarget failed.\n";
		return 1;
	}

	const std::string dynamicXaml = R"xaml(
<Window xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="NamespacedRuntimeWindow"
      ContentRendered="HandleWindowContentRendered">
  <Button x:Name="namespaceButton" DesignId="77"
          Content="Dynamic" Width="120" Height="24"
          Click="HandleNamespacedClick" />
</Window>)xaml";

	std::wstring error;
	DynamicMainWindowEventSink handlers;
	DesignerModel::RuntimeEventHandlerRegistry eventHandlers;
	if (!handlers.RegisterDeclarativeEventHandlers(eventHandlers, &error)
		|| eventHandlers.HandlerCount() != 5)
	{
		std::wcerr << L"Generated event registration failed: " << error << L'\n';
		return 1;
	}

	Window host;
	DesignerModel::RuntimeDocument document;
	DesignerModel::RuntimeDocumentLoadOptions options;
	options.ControlEventResolver = eventHandlers.ControlResolver();
	options.RequireControlEventResolver = true;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXamlIntoWindow(
		dynamicXaml, host, document, options,
		eventHandlers.WindowResolver(), &error))
	{
		std::wcerr << L"Dynamic XAML load failed: " << error << L'\n';
		return 1;
	}

	Acme::Views::MainWindowReferences<DesignerModel::RuntimeDocument>
		references(document);
	auto buttonReference = references.ReferenceNamespaceButton();
	auto* dynamicChrome = buttonReference
		? buttonReference->FindDeclarativeTemplatePart(L"PART_Chrome")
		: nullptr;
	if (!buttonReference || buttonReference.Get() != references.GetNamespaceButton()
		|| buttonReference->GetDisplayText() != L"Dynamic"
		|| !dynamicChrome
		|| cui::framework::TemplateAccess::GetTemplateRoot(*buttonReference)
			!= dynamicChrome
		|| dynamicChrome->GetTemplatedParent() != buttonReference.Get()
		|| buttonReference->GetPropertyValueSource(L"Background")
			!= DependencyPropertyValueSource::Theme
		|| buttonReference->GetCurrentVisualState(L"CommonStates") != L"Normal")
	{
		std::wcerr << L"Generated dynamic reference lookup failed.\n";
		return 1;
	}
	buttonReference->Click.Invoke(buttonReference.Get(), RoutedEventArgs{});
	cui::framework::EventAccess::Raise(host.ContentRendered, &host);
	if (handlers.ClickCount != 1 || handlers.ContentRenderedCount != 1)
	{
		std::wcerr << L"Generated event routes did not invoke.\n";
		return 1;
	}

	const std::string reloadedXaml = R"xaml(
<Window xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="NamespacedRuntimeWindow"
      ContentRendered="HandleWindowContentRendered">
  <Button x:Name="namespaceButton" DesignId="77"
          Content="Reloaded" Width="120" Height="24"
          Click="HandleNamespacedClick" />
</Window>)xaml";
	DesignerModel::RuntimeDocumentReloadMode reloadMode{};
	const bool reloadSucceeded = DesignerModel::RuntimeDocumentLoader::ReloadXaml(
		reloadedXaml, document, {}, &reloadMode, &error);
	auto* reloadedButton = references.GetNamespaceButton();
	if (!reloadSucceeded
		|| reloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| buttonReference.Get() != reloadedButton
		|| !buttonReference
		|| buttonReference->GetDisplayText() != L"Reloaded")
	{
		std::wcerr << L"Generated reference did not follow reload: success="
			<< reloadSucceeded << L", mode=" << static_cast<int>(reloadMode)
			<< L", same=" << (buttonReference.Get() == reloadedButton)
			<< L", alive=" << static_cast<bool>(buttonReference)
			<< L", text="
			<< (buttonReference ? buttonReference->GetDisplayText() : L"<dead>")
			<< L", error=" << error << L'\n';
		return 1;
	}

	DesignerModel::RuntimeDocument moved(std::move(document));
	if (references.TryDocument() != &moved
		|| buttonReference.Get()
			!= moved.FindControlByDesignId<Button>(77))
	{
		std::wcerr << L"Generated reference did not follow document move.\n";
		return 1;
	}

	std::wcout << L"CUI static generated sample passed.\n";
	return 0;
}
