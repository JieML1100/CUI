#pragma once

#include <string>

/**
 * Runs a windowless smoke test against the production DesignerCanvas,
 * PropertyGrid, document snapshot, and undo/redo implementations.
 */
bool RunDesignerSelfTest(std::wstring& report);
