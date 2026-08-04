#pragma once

#include <memory>
#include <string>

class Control;
class ControlStyleSheet;
enum class DependencyPropertyValueSource : unsigned char;

/**
 * Build-generated Generic.xaml provider.
 *
 * The implementation lives in CuiGeneratedTheme and is produced entirely at
 * build time. This declaration intentionally exposes no parser, designer
 * document, or runtime-XAML type.
 */
class CuiGeneratedFrameworkTheme final
{
public:
	CuiGeneratedFrameworkTheme() = delete;

	static std::shared_ptr<const ControlStyleSheet> DefaultStyleSheet(
		std::wstring* outError = nullptr);
	static bool Apply(
		Control& root,
		bool recursive = true,
		std::wstring* outError = nullptr);

	/**
	 * Sets only the compiled ControlTemplate for this control, without
	 * applying the full recursive style environment. Used by generated
	 * template factories when a nested control sources its template from
	 * the built-in theme.
	 */
	static bool InstallTemplateValue(
		Control& target,
		const std::wstring& templateKey,
		DependencyPropertyValueSource valueSource,
		std::wstring* outError = nullptr);

	/**
	 * Applies compiled VisualState groups sourced from the built-in theme
	 * to a control whose template was already installed. Called after the
	 * template visual tree is fully constructed.
	 */
	static bool ApplyTemplateVisualStates(
		Control& target,
		const std::wstring& templateKey,
		std::wstring* outError = nullptr);
};
