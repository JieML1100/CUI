#include "../include/XamlFrameworkTheme.h"

#include "../include/XamlObjectMaterializer.h"
#include "../include/XamlRuntimeSchema.h"
#include "../../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../../CuiDesigner/DesignerStyleSheetUtils.h"
#include "../../CUI/include/StyleInfrastructure.h"
#include "../../CUI/include/XamlInfrastructure.h"
#include "../../CUI/include/XamlSchema.h"
#include "CuiFrameworkTheme.g.h"

#include <iterator>
#include <mutex>
#include <string>

namespace
{
	struct FrameworkThemeCache final
	{
		std::once_flag Initialize;
		std::shared_ptr<const DesignerModel::DesignDocument> Document;
		std::shared_ptr<const ControlStyleSheet> StyleSheet;
		std::shared_ptr<XamlSchemaContext> SchemaContext;
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

			auto sharedDocument =
				std::make_shared<const DesignerModel::DesignDocument>(
					std::move(document));
			auto schemaContext = std::make_shared<XamlSchemaContext>();
			const DesignerStyleSheetUtils::RulePropertySchemaResolver
				schemaResolver =
				[sharedDocument, schemaContext](const DesignerStyleRule& rule,
					CuiRuntime::XamlTypePropertySchema& schema,
					std::wstring* error) -> bool
				{
					const auto* component = rule.ComponentType.Empty()
						? nullptr
						: sharedDocument->FindComponent(rule.ComponentType);
					if (!rule.ComponentType.Empty() && !component)
					{
						if (error) *error = L"样式 TargetType 组件不存在。";
						return false;
					}
					if (!CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
						rule.HasType ? rule.Type : UIClass::UI_Base,
						component, *sharedDocument, schema, error))
						return false;
					if (!schema.DeclarativeType) return true;

					const auto declarativePropertyCount =
						schema.DeclarativeType->Properties().size();
					if (declarativePropertyCount > schema.Properties.size())
					{
						if (error) *error = L"组件属性 Schema 数量无效。";
						return false;
					}
					auto canonical = schemaContext->GetOrAdd(
						schema.DeclarativeType, error);
					if (!canonical) return false;
					schema.Properties.erase(
						schema.Properties.begin(),
						schema.Properties.begin() + declarativePropertyCount);
					schema.DeclarativeType = std::move(canonical);
					const auto properties = schema.DeclarativeType->Properties();
					schema.Properties.insert(schema.Properties.begin(),
						properties.begin(), properties.end());
					if (error) error->clear();
					return true;
				};
			std::shared_ptr<ControlStyleSheet> styleSheet;
			auto structuralResources =
				DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
					sharedDocument->ItemsPanelTemplates);
			auto controlTemplates =
				CuiRuntime::XamlObjectMaterializer::
					BuildControlTemplateStyleResources(sharedDocument);
			structuralResources.insert(
				structuralResources.end(),
				std::make_move_iterator(controlTemplates.begin()),
				std::make_move_iterator(controlTemplates.end()));
			if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
				sharedDocument->StyleSheet, styleSheet, &cache.Error,
				sharedDocument->ResourceBasePath,
				sharedDocument->Resources,
				structuralResources, schemaResolver))
				return;
			cache.Document = std::move(sharedDocument);
			cache.StyleSheet = std::move(styleSheet);
			cache.SchemaContext = std::move(schemaContext);
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

bool CuiRuntime::XamlFrameworkTheme::InstallTemplateValue(
	Control& owner,
	const std::wstring& resourceKey,
	DependencyPropertyValueSource valueSource,
	std::wstring* outError)
{
	auto styleSheet = DefaultStyleSheet(outError);
	BindingValue value;
	ControlTemplateReference reference;
	if (!styleSheet
		|| !styleSheet->TryGetResource(resourceKey, value)
		|| !value.TryGet(reference) || !reference)
	{
		if (outError) *outError =
			L"Generic.xaml ControlTemplate 运行期资源不存在："
			+ resourceKey;
		return false;
	}
	if (!cui::framework::XamlAccess::SetTemplate(
		owner, reference, valueSource))
	{
		if (outError) *outError =
			L"Generic.xaml Control.Template Theme 值安装失败："
			+ resourceKey;
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
