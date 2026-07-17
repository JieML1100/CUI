#include "NamespacedWindow.h"

#include <iostream>

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
	std::wcout << L"CUI static generated sample passed: namespaced x:Class, custom controls, typed x:Name access, generated/user split, stable IDs, and std::bind_front event wiring compile and run.\n";
	return 0;
}
