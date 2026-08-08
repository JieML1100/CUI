#pragma once
#include "Binding.h"
#include "Button.h"
#include "Control.h"
#include "Layout/LayoutTypes.h"
#include "Layout/StackPanel.h"
#include "RoutedCommand.h"
#include "Window.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Acme::Views
{

class MainWindowGenerated : public Window
{
protected:
	StackPanel* staticRoot = nullptr;
	Button* namespaceButton = nullptr;
	Button* authorTemplateButton = nullptr;
	Button* styleTemplateButton = nullptr;
	std::vector<EventConnection> _generatedEventConnections;
	bool _componentInitialized = false;
	void InitializeComponent();

	virtual void HandleWindowContentRendered(Window* sender) = 0;
	virtual void HandleStaticRefreshCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) = 0;
	virtual void HandleStaticRefreshExecuted(Control* sender, ExecutedRoutedEventArgs& e) = 0;
	virtual void HandleNamespacedClick(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleNamespacedDrop(Control* sender, DragEventArgs& e) = 0;

public:
	// Name-free identities for the authored root DataContext contract.
	struct DataContextProperties final
	{
		static constexpr BindingSourcePropertyToken Caption{ 7131070297711251227ULL };
	};

	// Type-safe x:Name accessors; ownership remains with the generated Window.
	[[nodiscard]] StackPanel* GetStaticRoot() noexcept { return staticRoot; }
	[[nodiscard]] const StackPanel* GetStaticRoot() const noexcept { return staticRoot; }
	[[nodiscard]] Button* GetNamespaceButton() noexcept { return namespaceButton; }
	[[nodiscard]] const Button* GetNamespaceButton() const noexcept { return namespaceButton; }
	[[nodiscard]] Button* GetAuthorTemplateButton() noexcept { return authorTemplateButton; }
	[[nodiscard]] const Button* GetAuthorTemplateButton() const noexcept { return authorTemplateButton; }
	[[nodiscard]] Button* GetStyleTemplateButton() noexcept { return styleTemplateButton; }
	[[nodiscard]] const Button* GetStyleTemplateButton() const noexcept { return styleTemplateButton; }

	MainWindowGenerated();
	virtual ~MainWindowGenerated();
	bool BindData(BindingSourceReference dataContext);
};

}
