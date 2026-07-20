#include "NamespacedWindow.h"

#include <CuiRuntime.h>

#include <iostream>
#include <string>
#include <utility>

namespace
{
	class DynamicMainWindowEventSink final
		: public Acme::Views::MainWindowEventSink
	{
	public:
		int ClickCount = 0;
		int ShownCount = 0;

	private:
		void HandleNamespacedDrop(
			Control*, std::vector<std::wstring>) override {}
		void HandleNamespacedClick(Control*, MouseEventArgs) override
		{
			++ClickCount;
		}
		void HandleNamespacedPropertyChanged(
			Control*, const ControlPropertyChangedEventArgs&) override {}
		void HandleNamespacedValidationChanged(
			const BindingValidationChangedEventArgs&) override {}
		void HandleWindowShown(Form*) override
		{
			++ShownCount;
		}
	};
}

int wmain()
{
	Acme::Views::MainWindow window;
	auto* staticButton = window.GetNamespaceButton();
	if (!staticButton || staticButton->Text != L"Namespaced"
		|| staticButton->DesignId != Acme::Views::MainWindowGenerated::
			ControlIds::namespaceButton)
	{
		std::wcerr << L"CUI static generated sample failed.\n";
		return 1;
	}

	const std::string dynamicXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="NamespacedRuntimeForm"
      OnShown="HandleWindowShown">
  <Button x:Name="namespaceButton" DesignId="77"
          Text="Dynamic" Width="120" Height="24"
          Click="HandleNamespacedClick" />
</Form>)xaml";

	std::wstring error;
	DynamicMainWindowEventSink handlers;
	DesignerModel::RuntimeEventHandlerRegistry eventHandlers;
	if (!handlers.RegisterDynamicEventHandlers(eventHandlers, &error)
		|| eventHandlers.HandlerCount() != 5)
	{
		std::wcerr << L"Generated event registration failed: " << error << L'\n';
		return 1;
	}

	Form host;
	DesignerModel::RuntimeDocument document;
	DesignerModel::RuntimeDocumentLoadOptions options;
	options.ControlEventResolver = eventHandlers.ControlResolver();
	options.RequireControlEventResolver = true;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXamlIntoForm(
		dynamicXaml, host, document, options,
		eventHandlers.FormResolver(), &error))
	{
		std::wcerr << L"Dynamic XAML load failed: " << error << L'\n';
		return 1;
	}

	Acme::Views::MainWindowReferences<DesignerModel::RuntimeDocument>
		references(document);
	auto buttonReference = references.ReferenceNamespaceButton();
	if (!buttonReference || buttonReference.Get() != references.GetNamespaceButton()
		|| buttonReference->Text != L"Dynamic")
	{
		std::wcerr << L"Generated dynamic reference lookup failed.\n";
		return 1;
	}
	buttonReference->OnMouseClick.Invoke(buttonReference.Get(), MouseEventArgs{});
	host.OnShown.Invoke(&host);
	if (handlers.ClickCount != 1 || handlers.ShownCount != 1)
	{
		std::wcerr << L"Generated event routes did not invoke.\n";
		return 1;
	}

	const std::string reloadedXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="NamespacedRuntimeForm"
      OnShown="HandleWindowShown">
  <Button x:Name="namespaceButton" DesignId="77"
          Text="Reloaded" Width="120" Height="24"
          Click="HandleNamespacedClick" />
</Form>)xaml";
	DesignerModel::RuntimeDocumentReloadMode reloadMode{};
	if (!DesignerModel::RuntimeDocumentLoader::ReloadXaml(
		reloadedXaml, document, {}, &reloadMode, &error)
		|| reloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| buttonReference.Get() != references.GetNamespaceButton()
		|| buttonReference->Text != L"Reloaded")
	{
		std::wcerr << L"Generated reference did not follow reload: "
			<< error << L'\n';
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
