#pragma once
// <cui-designer-user-header> Created once; safe for user edits.
// <cui-designer-class>Acme::Views::MainWindow</cui-designer-class>
#include "NamespacedWindow.g.h"

namespace Acme::Views
{

class MainWindow : public MainWindowGenerated
{
public:
	MainWindow();
	~MainWindow() override = default;

private:
#include "NamespacedWindow.handlers.g.inc"
};

}
