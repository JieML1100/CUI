#pragma once

#include "../../CuiDesigner/DesignerModel/DesignDocument.h"

#include <memory>
#include <string>

class Control;
class ControlStyleSheet;

namespace CuiRuntime
{
/**
 * Access to the build-embedded Generic.xaml projection. The XAML dictionary is
 * the source of truth; this class only caches its parsed/runtime projections.
 */
class XamlFrameworkTheme final
{
public:
	static std::shared_ptr<const DesignerModel::DesignDocument>
		DefaultDocument(std::wstring* outError = nullptr);
	static std::shared_ptr<const ControlStyleSheet>
		DefaultStyleSheet(std::wstring* outError = nullptr);

	static bool Apply(
		Control& root,
		bool recursive = true,
		std::wstring* outError = nullptr);
	/** Installs the Theme Template value before a precompiled tree is attached. */
	static bool InstallTemplateValue(
		Control& owner,
		const std::wstring& resourceKey,
		std::wstring* outError = nullptr);
	static bool ApplyTemplateVisualStates(
		Control& owner,
		const std::wstring& resourceKey,
		std::wstring* outError = nullptr);
};
}
