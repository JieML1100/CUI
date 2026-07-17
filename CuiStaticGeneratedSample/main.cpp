#include "NamespacedWindow.h"

#include <CuiRuntime.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace
{
	class DynamicMainWindowEventSink final
		: public Acme::Views::MainWindowEventSink
	{
	public:
		int ClickCount = 0;
		int LastSeverity = -1;
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
		void HandleSeverityInvoked(Control*, int value) override
		{
			LastSeverity = value;
		}
		void HandleWindowShown(Form*) override
		{
			++ShownCount;
		}
	};
}

int wmain()
{
	Acme::Views::MainWindow window;
	auto* button = window.GetNamespaceButton();
	if (!button || button->Text != L"Namespaced"
		|| button->DesignId != Acme::Views::MainWindowGenerated::
			ControlIds::namespaceButton)
	{
		std::wcerr << L"CUI static generated sample failed: typed x:Name access mismatch.\n";
		return 1;
	}
	button->OnMouseClick.Invoke(button, MouseEventArgs{});
	auto* badge = window.GetStatusBadge();
	if (!badge || badge->Text != L"Custom control"
		|| badge->DesignId != Acme::Views::MainWindowGenerated::
			ControlIds::statusBadge)
	{
		std::wcerr << L"CUI static generated sample failed: custom control mismatch.\n";
		return 1;
	}
	badge->OnSeverityInvoked.Invoke(badge, 2);

	// The same generated header also provides a zero-owning typed view for a
	// dynamically loaded document. Its persistent reference resolves by stable
	// DesignId on every access, so it remains valid across runtime reloads.
	const std::string dynamicXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer"
      xmlns:sample="urn:cui:samples"
      x:Name="NamespacedRuntimeForm"
      OnShown="HandleWindowShown">
  <Button x:Name="namespaceButton" DesignId="77"
          Text="Dynamic" Width="120" Height="24"
          Click="HandleNamespacedClick" />
  <sample:StatusBadge x:Name="statusBadge" DesignId="78"
          d:CppType="Acme.Controls.StatusBadge"
          d:Header="Controls/StatusBadge.h"
          d:BaseType="Button" d:Constructor="Bounds"
          Text="Dynamic custom" Width="150" Height="30"
          OnSeverityInvoked="HandleSeverityInvoked">
    <d:CustomEvents>
      <d:Event Name="OnSeverityInvoked" Field="OnSeverityInvoked"
               Signature="SenderInt" Category="Action" Default="true" />
    </d:CustomEvents>
  </sample:StatusBadge>
</Form>)xaml";
	std::wstring runtimeError;
	DynamicMainWindowEventSink dynamicHandlers;
	DesignerModel::RuntimeEventHandlerRegistry conflictingHandlers;
	if (!conflictingHandlers.RegisterControl(
		L"HandleNamespacedClick", UIClass::UI_Base, L"OnMouseClick",
		&Control::OnMouseClick, [](Control*, MouseEventArgs) {}, &runtimeError)
		|| dynamicHandlers.RegisterDynamicEventHandlers(
			conflictingHandlers, &runtimeError)
		|| conflictingHandlers.HandlerCount() != 1)
	{
		std::wcerr << L"CUI generated event batch did not roll back: "
			<< runtimeError << L'\n';
		return 1;
	}
	DesignerModel::RuntimeEventHandlerRegistry dynamicEventHandlers;
	if (!dynamicHandlers.RegisterDynamicEventHandlers(
		dynamicEventHandlers, &runtimeError)
		|| dynamicEventHandlers.HandlerCount() != 6)
	{
		std::wcerr << L"CUI generated event registration failed: "
			<< runtimeError << L'\n';
		return 1;
	}
	if (dynamicHandlers.RegisterDynamicEventHandlers(
		dynamicEventHandlers, &runtimeError)
		|| dynamicEventHandlers.HandlerCount() != 6
		|| runtimeError.find(L"已注册") == std::wstring::npos)
	{
		std::wcerr << L"CUI generated event re-registration was not atomic: "
			<< runtimeError << L'\n';
		return 1;
	}
	auto dynamicCustomControls =
		std::make_shared<DesignerModel::RuntimeCustomControlRegistry>();
	if (!dynamicCustomControls->Register(
		L"urn:cui:samples", L"StatusBadge",
		[](const DesignerModel::DesignNode&)
		{
			return std::make_unique<Acme::Controls::StatusBadge>(0, 0, 150, 30);
		}, &runtimeError))
	{
		std::wcerr << L"CUI generated reference custom factory failed: "
			<< runtimeError << L'\n';
		return 1;
	}
	Form dynamicHost;
	DesignerModel::RuntimeDocument runtimeDocument;
	DesignerModel::RuntimeDocumentLoadOptions dynamicOptions;
	dynamicOptions.CustomControls = dynamicCustomControls;
	dynamicOptions.ControlEventResolver = dynamicEventHandlers.ControlResolver();
	dynamicOptions.RequireControlEventResolver = true;
	if (!DesignerModel::RuntimeDocumentLoader::LoadXamlIntoForm(
		dynamicXaml, dynamicHost, runtimeDocument, dynamicOptions,
		dynamicEventHandlers.FormResolver(), &runtimeError))
	{
		std::wcerr << L"CUI generated reference sample failed to load XAML: "
			<< runtimeError << L'\n';
		return 1;
	}
	Acme::Views::MainWindowReferences<DesignerModel::RuntimeDocument>
		references(runtimeDocument);
	auto dynamicButtonReference = references.ReferenceNamespaceButton();
	auto dynamicBadgeReference = references.ReferenceStatusBadge();
	auto* dynamicButton = references.GetNamespaceButton();
	if (!dynamicButton || dynamicButtonReference.Get() != dynamicButton
		|| dynamicButton->Text != L"Dynamic"
		|| !dynamicBadgeReference
		|| dynamicBadgeReference.Get() != references.GetStatusBadge())
	{
		std::wcerr << L"CUI generated dynamic references failed initial lookup.\n";
		return 1;
	}
	dynamicButton->OnMouseClick.Invoke(dynamicButton, MouseEventArgs{});
	dynamicBadgeReference->OnSeverityInvoked.Invoke(
		dynamicBadgeReference.Get(), 7);
	dynamicHost.OnShown.Invoke(&dynamicHost);
	if (dynamicHandlers.ClickCount != 1 || dynamicHandlers.LastSeverity != 7
		|| dynamicHandlers.ShownCount != 1)
	{
		std::wcerr << L"CUI generated dynamic event routes did not invoke.\n";
		return 1;
	}

	const std::string reloadedXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer"
      xmlns:sample="urn:cui:samples"
      x:Name="NamespacedRuntimeForm"
      OnShown="HandleWindowShown">
  <Button x:Name="namespaceButton" DesignId="77"
          Text="Reloaded" Width="120" Height="24"
          Click="HandleNamespacedClick" />
  <sample:StatusBadge x:Name="statusBadge" DesignId="78"
          d:CppType="Acme.Controls.StatusBadge"
          d:Header="Controls/StatusBadge.h"
          d:BaseType="Button" d:Constructor="Bounds"
          Text="Reloaded custom" Width="150" Height="30"
          OnSeverityInvoked="HandleSeverityInvoked">
    <d:CustomEvents>
      <d:Event Name="OnSeverityInvoked" Field="OnSeverityInvoked"
               Signature="SenderInt" Category="Action" Default="true" />
    </d:CustomEvents>
  </sample:StatusBadge>
</Form>)xaml";
	DesignerModel::RuntimeDocumentReloadMode reloadMode{};
	if (!DesignerModel::RuntimeDocumentLoader::ReloadXaml(
		reloadedXaml, runtimeDocument, {}, &reloadMode, &runtimeError)
		|| reloadMode != DesignerModel::RuntimeDocumentReloadMode::InPlace
		|| dynamicButtonReference.Get() != references.GetNamespaceButton()
		|| dynamicBadgeReference.Get() != references.GetStatusBadge()
		|| dynamicButtonReference->Text != L"Reloaded"
		|| dynamicBadgeReference->Text != L"Reloaded custom")
	{
		std::wcerr << L"CUI generated dynamic references did not follow reload: "
			<< runtimeError << L'\n';
		return 1;
	}
	dynamicButtonReference->OnMouseClick.Invoke(
		dynamicButtonReference.Get(), MouseEventArgs{});
	dynamicBadgeReference->OnSeverityInvoked.Invoke(
		dynamicBadgeReference.Get(), 9);
	dynamicHost.OnShown.Invoke(&dynamicHost);
	if (dynamicHandlers.ClickCount != 2 || dynamicHandlers.LastSeverity != 9
		|| dynamicHandlers.ShownCount != 2)
	{
		std::wcerr << L"CUI generated dynamic event routes did not survive reload.\n";
		return 1;
	}

	// The generated view and its persistent control references both follow
	// RuntimeDocument move construction without owning document or controls.
	DesignerModel::RuntimeDocument movedRuntimeDocument(
		std::move(runtimeDocument));
	if (references.TryDocument() != &movedRuntimeDocument
		|| references.GetNamespaceButton()
			!= movedRuntimeDocument.FindControlByDesignId<Button>(77)
		|| dynamicButtonReference.Get()
			!= movedRuntimeDocument.FindControlByDesignId<Button>(77)
		|| dynamicBadgeReference.Get()
			!= movedRuntimeDocument.FindControlByDesignId<
				Acme::Controls::StatusBadge>(78))
	{
		std::wcerr << L"CUI generated dynamic references did not follow document move.\n";
		return 1;
	}
	runtimeDocument = std::move(movedRuntimeDocument);
	if (references.TryDocument() != &runtimeDocument
		|| dynamicButtonReference.Get() != references.GetNamespaceButton()
		|| dynamicBadgeReference.Get() != references.GetStatusBadge())
	{
		std::wcerr << L"CUI generated dynamic references did not follow move restoration.\n";
		return 1;
	}

	using DynamicReferences =
		Acme::Views::MainWindowReferences<DesignerModel::RuntimeDocument>;
	std::optional<DynamicReferences> expiredReferences;
	{
		DesignerModel::RuntimeDocument temporaryDocument;
		if (!DesignerModel::RuntimeDocumentLoader::LoadXaml(
			dynamicXaml, temporaryDocument, dynamicOptions, &runtimeError))
		{
			std::wcerr << L"CUI generated expiring view setup failed: "
				<< runtimeError << L'\n';
			return 1;
		}
		expiredReferences.emplace(temporaryDocument);
		if (!static_cast<bool>(*expiredReferences)
			|| expiredReferences->TryDocument() != &temporaryDocument
			|| !expiredReferences->GetNamespaceButton())
		{
			std::wcerr << L"CUI generated expiring view failed initial lookup.\n";
			return 1;
		}
	}
	if (static_cast<bool>(*expiredReferences)
		|| expiredReferences->TryDocument()
		|| expiredReferences->GetNamespaceButton()
		|| expiredReferences->ReferenceNamespaceButton())
	{
		std::wcerr << L"CUI generated document view did not expire safely.\n";
		return 1;
	}

	// Moving the same generated sink to another registry releases the prior
	// route lease and expires the weak lifetime gate used by EventConnections
	// that the already-loaded document still owns.
	DesignerModel::RuntimeEventHandlerRegistry replacementEventHandlers;
	if (!dynamicHandlers.RegisterDynamicEventHandlers(
		replacementEventHandlers, &runtimeError)
		|| dynamicEventHandlers.HandlerCount() != 0
		|| replacementEventHandlers.HandlerCount() != 6)
	{
		std::wcerr << L"CUI generated event lease replacement failed: "
			<< runtimeError << L'\n';
		return 1;
	}
	dynamicButtonReference->OnMouseClick.Invoke(
		dynamicButtonReference.Get(), MouseEventArgs{});
	dynamicBadgeReference->OnSeverityInvoked.Invoke(
		dynamicBadgeReference.Get(), 11);
	dynamicHost.OnShown.Invoke(&dynamicHost);
	if (dynamicHandlers.ClickCount != 2 || dynamicHandlers.LastSeverity != 9
		|| dynamicHandlers.ShownCount != 2)
	{
		std::wcerr << L"CUI expired event lifetime gate still invoked handlers.\n";
		return 1;
	}
	dynamicHandlers.UnregisterDynamicEventHandlers();
	if (replacementEventHandlers.HandlerCount() != 0)
	{
		std::wcerr << L"CUI explicit generated event unregister kept routes.\n";
		return 1;
	}

	DesignerModel::RuntimeEventHandlerRegistry automaticEventHandlers;
	{
		DynamicMainWindowEventSink automaticHandlers;
		if (!automaticHandlers.RegisterDynamicEventHandlers(
			automaticEventHandlers, &runtimeError)
			|| automaticEventHandlers.HandlerCount() != 6)
		{
			std::wcerr << L"CUI automatic event lease setup failed: "
				<< runtimeError << L'\n';
			return 1;
		}
	}
	if (automaticEventHandlers.HandlerCount() != 0)
	{
		std::wcerr << L"CUI event sink destruction kept registered routes.\n";
		return 1;
	}

	std::wcout << L"CUI static generated sample passed: namespaced x:Class, custom controls, inline user-header handlers, static and reload-safe dynamic typed references, atomic lifetime-scoped generated event registration, generated/user split, stable IDs, and std::bind_front event wiring compile and run.\n";
	return 0;
}
