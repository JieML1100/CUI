// <cui-designer-user-source> Created once; safe for user edits.
// <cui-designer-class>Acme::Views::MainWindow</cui-designer-class>
#include "NamespacedWindow.h"

#include <any>

Acme::Views::MainWindow::MainWindow()
	: Acme::Views::MainWindowGenerated()
{
	InitializeComponent();
	// User initialization belongs here.
}

void Acme::Views::MainWindow::HandleNamespacedDrop(
	Control* sender, DragEventArgs& e)
{
	(void)sender;
	e.Effects = DragDropEffects::Copy;
	e.Handled = true;
}

void Acme::Views::MainWindow::HandleNamespacedClick(Control* sender, RoutedEventArgs& e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindow::HandleNamespacedPropertyChanged(DependencyObject* sender, const DependencyPropertyChangedEventArgs& e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindow::HandleNamespacedValidationChanged(const BindingValidationChangedEventArgs& e)
{
	(void)e;
}

void Acme::Views::MainWindow::HandleStaticRefreshCanExecute(
	Control*, CanExecuteRoutedEventArgs& e)
{
	const auto* parameter = std::any_cast<std::wstring>(&e.Parameter);
	e.CanExecute = parameter && *parameter == L"static-input";
}

void Acme::Views::MainWindow::HandleStaticRefreshExecuted(
	Control*, ExecutedRoutedEventArgs& e)
{
	if (auto* button = GetNamespaceButton())
	{
		int invocationCount = 0;
		(void)button->Tag.TryGetInt(invocationCount);
		button->Tag = BindingValue(invocationCount + 1);
	}
	e.Executed = true;
}
