#include "../include/XamlFrameworkTheme.h"

#include "../include/XamlObjectMaterializer.h"
#include "../../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../../CuiDesigner/DesignerStyleSheetUtils.h"
#include "../../CUI/include/StyleInfrastructure.h"
#include "CuiFrameworkTheme.g.h"

#include <mutex>
#include <string>

namespace
{
	struct FrameworkThemeCache final
	{
		std::once_flag Initialize;
		std::shared_ptr<const DesignerModel::DesignDocument> Document;
		std::shared_ptr<const ControlStyleSheet> StyleSheet;
		std::wstring Error;
	};

	FrameworkThemeCache& ThemeCache()
	{
		static FrameworkThemeCache cache;
		return cache;
	}

	void EnsureThemeLoaded()
	{
		auto& cache = ThemeCache();
		std::call_once(cache.Initialize, [&]
		{
			const std::string xaml(
				reinterpret_cast<const char*>(
					CuiRuntime::Generated::GenericXaml),
				CuiRuntime::Generated::GenericXamlSize);
			DesignerModel::DesignDocument document;
			if (!DesignerModel::XamlDocumentParser::FromResourceDictionary(
				xaml, document, &cache.Error))
				return;

			std::shared_ptr<ControlStyleSheet> styleSheet;
			if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
				document.StyleSheet, styleSheet, &cache.Error,
				document.ResourceBasePath, document.Resources))
				return;
			cache.Document =
				std::make_shared<const DesignerModel::DesignDocument>(
					std::move(document));
			cache.StyleSheet = std::move(styleSheet);
			cache.Error.clear();
		});
	}
}

std::shared_ptr<const DesignerModel::DesignDocument>
CuiRuntime::XamlFrameworkTheme::DefaultDocument(std::wstring* outError)
{
	EnsureThemeLoaded();
	const auto& cache = ThemeCache();
	if (outError) *outError = cache.Error;
	return cache.Document;
}

std::shared_ptr<const ControlStyleSheet>
CuiRuntime::XamlFrameworkTheme::DefaultStyleSheet(std::wstring* outError)
{
	EnsureThemeLoaded();
	const auto& cache = ThemeCache();
	if (outError) *outError = cache.Error;
	return cache.StyleSheet;
}

bool CuiRuntime::XamlFrameworkTheme::Apply(
	Control& root,
	bool recursive,
	std::wstring* outError)
{
	auto styleSheet = DefaultStyleSheet(outError);
	if (!styleSheet) return false;
	if (!cui::framework::StyleAccess::SetTheme(
		root, std::move(styleSheet), recursive))
	{
		if (outError) *outError =
			L"Generic.xaml 主题样式无法应用到控件树。";
		return false;
	}
	if (outError) outError->clear();
	return true;
}

bool CuiRuntime::XamlFrameworkTheme::ApplyTemplateVisualStates(
	Control& owner,
	const std::wstring& resourceKey,
	std::wstring* outError)
{
	auto document = DefaultDocument(outError);
	if (!document) return false;
	const auto* definition = document->FindControlTemplate(resourceKey);
	if (!definition)
	{
		if (outError) *outError =
			L"Generic.xaml 中不存在 ControlTemplate：" + resourceKey;
		return false;
	}
	return XamlObjectMaterializer::InstallControlTemplateVisualStates(
		owner, *definition, *document, outError);
}
