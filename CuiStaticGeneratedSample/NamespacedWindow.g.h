#pragma once
#include "Binding.h"
#include "Button.h"
#include "Control.h"
#include "Controls/StatusBadge.h"
#include "Form.h"
#include <vector>

namespace Acme::Views
{

class MainWindowGenerated : public Form
{
protected:
	Button* namespaceButton = nullptr;
	Acme::Controls::StatusBadge* statusBadge = nullptr;
	std::vector<EventConnection> _generatedEventConnections;

	virtual void HandleNamespacedDrop(Control* sender, std::vector<std::wstring> files);
	virtual void HandleNamespacedClick(Control* sender, MouseEventArgs e);
	virtual void HandleNamespacedPropertyChanged(Control* sender, const ControlPropertyChangedEventArgs& e);
	virtual void HandleNamespacedValidationChanged(const BindingValidationChangedEventArgs& e);
	virtual void HandleSeverityInvoked(Control* sender, int value);

public:
	// Stable identities shared by static and dynamic document paths.
	struct ControlIds final
	{
		static constexpr int namespaceButton = 77;
		static constexpr int statusBadge = 78;
	};

	// Type-safe x:Name accessors; ownership remains with the generated Form.
	[[nodiscard]] Button* GetNamespaceButton() noexcept { return namespaceButton; }
	[[nodiscard]] const Button* GetNamespaceButton() const noexcept { return namespaceButton; }
	[[nodiscard]] Acme::Controls::StatusBadge* GetStatusBadge() noexcept { return statusBadge; }
	[[nodiscard]] const Acme::Controls::StatusBadge* GetStatusBadge() const noexcept { return statusBadge; }

	MainWindowGenerated();
	virtual ~MainWindowGenerated();
};

}
