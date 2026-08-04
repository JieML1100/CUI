#include "NamespacedWindow.h"

#include <InputInfrastructure.h>
#include <Border.h>
#include <Canvas.h>
#include <PresentationInfrastructure.h>
#include <TemplateInfrastructure.h>

#include <iostream>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

int wmain()
{
	Acme::Views::MainWindow window;
	auto staticDataContext = std::make_shared<ObservableObject>();
	staticDataContext->SetValue(
		Acme::Views::MainWindowGenerated::DataContextProperties::Caption,
		std::wstring(L"Namespaced"));
	if (!window.BindData(BindingSourceReference(staticDataContext)))
	{
		std::wcerr << L"CUI static generated binding failed.\n";
		return 1;
	}
	auto* staticButton = window.GetNamespaceButton();
	if (staticButton)
		(void)staticButton->ApplyTemplate();
	auto* staticChrome = staticButton
		? staticButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Chrome"))
		: nullptr;
	auto* staticPresenter = staticButton
		? staticButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ContentPresenter"))
		: nullptr;
	auto* staticTemplateRoot = staticButton
		? cui::framework::TemplateAccess::GetTemplateRoot(*staticButton)
		: nullptr;
	if (!staticButton || staticButton->GetDisplayText() != L"Namespaced"
		|| !staticChrome || !staticPresenter
		|| !staticButton->GetTemplate()
		|| staticButton->GetPropertyValueSource(Control::TemplateProperty())
			!= DependencyPropertyValueSource::Theme
		|| !staticTemplateRoot
		|| staticChrome->GetVisualParent() != staticTemplateRoot
		|| staticTemplateRoot->GetVisualParent() != staticButton
		|| staticChrome->GetLogicalParent() != nullptr
		|| staticChrome->GetTemplatedParent() != staticButton
		|| staticButton->GetPropertyValueSource(Control::BackgroundProperty())
			!= DependencyPropertyValueSource::Theme
		|| staticChrome->GetPropertyValueSource(Border::BackgroundProperty())
			!= DependencyPropertyValueSource::Template
		|| staticButton->GetCurrentVisualState(
			MakeVisualStateGroupToken(L"CommonStates"))
			!= MakeVisualStateToken(L"Normal")
		|| staticButton->GetDataContext().Get() != staticDataContext.get()
		|| !staticButton->HasAuthoredCommandTarget()
		|| staticButton->CommandTarget != &window
		|| staticButton->GetPropertyValueSource(Control::DataContextProperty())
			!= DependencyPropertyValueSource::Inherited)
	{
		std::wcerr << L"CUI static generated sample failed:"
			<< L" button=" << (staticButton != nullptr)
			<< L", text=" << (staticButton
				? staticButton->GetDisplayText() : L"<null>")
			<< L", chrome=" << (staticChrome != nullptr)
			<< L", presenter=" << (staticPresenter != nullptr)
			<< L", templateRoot=" << (staticTemplateRoot != nullptr)
			<< L", chromeVisualParent=" << (staticChrome
				&& staticChrome->GetVisualParent() == staticTemplateRoot)
			<< L", rootVisualParent=" << (staticTemplateRoot
				&& staticTemplateRoot->GetVisualParent() == staticButton)
			<< L", logicalParent=" << (staticChrome
				&& staticChrome->GetLogicalParent() == nullptr)
			<< L", templatedParent=" << (staticChrome
				&& staticChrome->GetTemplatedParent() == staticButton)
			<< L", buttonBackgroundSource="
			<< (staticButton ? static_cast<int>(
				staticButton->GetPropertyValueSource(
					Control::BackgroundProperty())) : -1)
			<< L", chromeBackgroundSource="
			<< (staticChrome ? static_cast<int>(
				staticChrome->GetPropertyValueSource(
					Border::BackgroundProperty())) : -1)
			<< L", stateToken=" << (staticButton
				? staticButton->GetCurrentVisualState(
					MakeVisualStateGroupToken(L"CommonStates")).Value
				: 0ULL)
			<< L", dataContext=" << (staticButton
				&& staticButton->GetDataContext().Get()
					== staticDataContext.get())
			<< L", commandTargetAuthored=" << (staticButton
				&& staticButton->HasAuthoredCommandTarget())
			<< L", commandTarget=" << (staticButton
				&& staticButton->CommandTarget == &window)
			<< L", dataContextSource="
			<< (staticButton ? static_cast<int>(
				staticButton->GetPropertyValueSource(
					Control::DataContextProperty())) : -1)
			<< L'\n';
			return 1;
		}

	auto* authorTemplateButton = window.GetAuthorTemplateButton();
	auto* styleTemplateButton = window.GetStyleTemplateButton();
	if (authorTemplateButton)
		(void)authorTemplateButton->ApplyTemplate();
	if (styleTemplateButton)
		(void)styleTemplateButton->ApplyTemplate();
	auto* authorChrome = authorTemplateButton
		? authorTemplateButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"StaticAuthorChrome"))
		: nullptr;
	auto* authorPresenter = authorTemplateButton
		? authorTemplateButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"StaticAuthorPresenter"))
		: nullptr;
	auto* nestedButton = authorTemplateButton
		? dynamic_cast<Button*>(
			authorTemplateButton->FindDeclarativeTemplatePart(
				MakeTemplatePartToken(L"StaticNestedButton")))
		: nullptr;
	if (nestedButton)
		(void)nestedButton->ApplyTemplate();
	auto* styledChrome = styleTemplateButton
		? styleTemplateButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"StaticAuthorChrome"))
		: nullptr;
	if (!authorTemplateButton || !styleTemplateButton
		|| !authorChrome || !authorPresenter || !nestedButton
		|| !nestedButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Chrome"))
		|| !styledChrome
		|| authorTemplateButton->GetPropertyValueSource(Control::TemplateProperty())
			!= DependencyPropertyValueSource::Local
		|| styleTemplateButton->GetPropertyValueSource(Control::TemplateProperty())
			!= DependencyPropertyValueSource::Style
		|| authorChrome->GetPropertyValueSource(Border::PaddingProperty())
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
	styleTemplateButton->IsDefault = true;
	const auto styleDefaultTick = ::GetTickCount64();
	if (!styleTemplateButton->HasActiveVisualStateAnimations()
		|| !cui::framework::PresentationAccess::
			AdvanceVisualStateAnimations(
				*styleTemplateButton, styleDefaultTick + 200)
		|| std::abs(styleTemplateButton->FontSize - 18.0f) > 0.01f
		|| styleTemplateButton->GetPropertyValueSource(Control::FontSizeProperty())
			!= DependencyPropertyValueSource::Animation)
	{
		std::wcerr
			<< L"Generated static Style Trigger.EnterActions "
				L"did not run its Storyboard.\n";
		return 1;
	}
	styleTemplateButton->IsDefault = false;
	if (std::abs(styleTemplateButton->FontSize - 14.0f) > 0.01f
		|| styleTemplateButton->GetPropertyValueSource(Control::FontSizeProperty())
			!= DependencyPropertyValueSource::Style)
	{
		std::wcerr
			<< L"Generated static Style Trigger.ExitActions "
				L"did not stop and restore its Storyboard.\n";
		return 1;
	}
	if (authorTemplateButton->GetCurrentVisualState(
			MakeVisualStateGroupToken(L"AuthorCommonStates"))
			!= MakeVisualStateToken(L"Normal")
		|| std::abs(Canvas::GetTop(*authorChrome)) > 0.001f)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate did not enter "
				L"its fallback VisualState.\n";
		return 1;
	}
	authorTemplateButton->IsDefault = true;
	const auto authorDefaultTick = ::GetTickCount64();
	if (authorTemplateButton->GetCurrentVisualState(
			MakeVisualStateGroupToken(L"AuthorCommonStates"))
			!= MakeVisualStateToken(L"Defaulted")
		|| !authorTemplateButton->HasActiveVisualStateAnimations()
		|| !cui::framework::PresentationAccess::
			AdvanceVisualStateAnimations(
				*authorTemplateButton, authorDefaultTick + 300)
		|| std::abs(Canvas::GetTop(*authorChrome) - 4.0f) > 0.01f
		|| authorChrome->GetPropertyValueSource(Border::BackgroundProperty())
			!= DependencyPropertyValueSource::VisualState)
	{
		std::wcerr
			<< L"Generated authored ControlTemplate VisualState/"
				L"Storyboard/Transition did not run.\n";
		return 1;
	}
	authorTemplateButton->IsDefault = false;
	(void)cui::framework::PresentationAccess::
		AdvanceVisualStateAnimations(
			*authorTemplateButton, ::GetTickCount64() + 300);
	if (authorTemplateButton->GetCurrentVisualState(
			MakeVisualStateGroupToken(L"AuthorCommonStates"))
			!= MakeVisualStateToken(L"Normal")
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
			MakeTemplatePartToken(L"StaticAlternateChrome"))
		|| authorTemplateButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"StaticAuthorChrome"))
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
			MakeTemplatePartToken(L"StaticAuthorChrome"))
		|| !authorTemplateButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"StaticNestedButton"))
		|| authorTemplateButton->GetCurrentVisualState(
			MakeVisualStateGroupToken(L"AuthorCommonStates"))
			!= MakeVisualStateToken(L"Normal"))
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
			MakeTemplatePartToken(L"StaticAuthorChrome"))
		|| !builtTemplateButton->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"StaticNestedButton"))
		|| builtTemplateButton->GetCurrentVisualState(
			MakeVisualStateGroupToken(L"AuthorCommonStates"))
			!= MakeVisualStateToken(L"Normal")
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
			|| !staticButton->FindDeclarativeTemplatePart(
				MakeTemplatePartToken(L"PART_Chrome"))
			|| !staticButton->FindDeclarativeTemplatePart(
				MakeTemplatePartToken(L"PART_ContentPresenter"))
			|| !staticButton->LastTemplateError().empty())
		{
			std::wcerr << L"Generated Theme Template did not reapply.\n";
			return 1;
		}
	staticDataContext->SetValue(
		Acme::Views::MainWindowGenerated::DataContextProperties::Caption,
		std::wstring(L"Namespaced updated"));
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

	std::wcout << L"CUI static generated sample passed.\n";
	return 0;
}
