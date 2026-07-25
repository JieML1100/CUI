#include "NamespacedWindow.h"

#include <CuiRuntime.h>
#include <EventInfrastructure.h>
#include <InputInfrastructure.h>
#include <TemplateInfrastructure.h>

#include <iostream>
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
