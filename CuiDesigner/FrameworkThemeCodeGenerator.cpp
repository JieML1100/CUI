#include "FrameworkThemeCodeGenerator.h"

#include "CodeGenerator.h"
#include "DesignerStyleSheetUtils.h"
#include "DesignerModel/AtomicFile.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/XamlDocumentParser.h"
#include "../CUI/include/GroupStyle.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace DesignerModel
{
namespace
{
	void SetError(std::wstring* outError, std::wstring message)
	{
		if (outError) *outError = std::move(message);
	}

	std::wstring Widen(std::string_view value)
	{
		return std::wstring(value.begin(), value.end());
	}

	std::string NarrowAscii(std::wstring_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const auto character : value)
		{
			if (character > 0x7f)
				throw std::invalid_argument(
					"Generated C++ type names must be ASCII");
			result.push_back(static_cast<char>(character));
		}
		return result;
	}

	struct QualifiedName final
	{
		std::string NamespaceName;
		std::string Leaf;

		std::string Qualified() const
		{
			return NamespaceName.empty()
				? Leaf : NamespaceName + "::" + Leaf;
		}
	};

	bool ParseQualifiedName(
		const std::wstring& value,
		QualifiedName& result,
		std::wstring* outError)
	{
		std::wstring normalized;
		if (!DesignCodeBehindModel::TryNormalizeClassName(
			value, normalized, outError))
			return false;
		const auto narrow = NarrowAscii(normalized);
		const auto split = narrow.rfind("::");
		result = {};
		if (split == std::string::npos)
			result.Leaf = narrow;
		else
		{
			result.NamespaceName = narrow.substr(0, split);
			result.Leaf = narrow.substr(split + 2);
		}
		return !result.Leaf.empty();
	}

	std::string ProviderHeader(
		const QualifiedName& provider)
	{
		std::ostringstream code;
		code << "#pragma once\n\n";
		if (provider.NamespaceName.empty()
			&& provider.Leaf == "CuiGeneratedFrameworkTheme")
		{
			code << "#include \"CuiGeneratedFrameworkTheme.h\"\n";
			return code.str();
		}
		code << "#include <memory>\n";
		code << "#include <string>\n\n";
		code << "class Control;\n";
		code << "class ControlStyleSheet;\n";
		code << "enum class DependencyPropertyValueSource : unsigned char;\n\n";
		if (!provider.NamespaceName.empty())
			code << "namespace " << provider.NamespaceName << "\n{\n\n";
		code << "class " << provider.Leaf << " final\n";
		code << "{\n";
		code << "public:\n";
		code << "\t" << provider.Leaf << "() = delete;\n\n";
		code << "\tstatic std::shared_ptr<const ControlStyleSheet> "
			"DefaultStyleSheet(\n";
		code << "\t\tstd::wstring* outError = nullptr);\n";
		code << "\tstatic bool Apply(\n";
		code << "\t\tControl& root,\n";
		code << "\t\tbool recursive = true,\n";
		code << "\t\tstd::wstring* outError = nullptr);\n";
		code << "\tstatic bool InstallTemplateValue(\n";
		code << "\t\tControl& target,\n";
		code << "\t\tconst std::wstring& templateKey,\n";
		code << "\t\tDependencyPropertyValueSource valueSource,\n";
		code << "\t\tstd::wstring* outError = nullptr);\n";
		code << "\tstatic bool ApplyTemplateVisualStates(\n";
		code << "\t\tControl& target,\n";
		code << "\t\tconst std::wstring& templateKey,\n";
		code << "\t\tstd::wstring* outError = nullptr);\n";
		code << "};\n";
		if (!provider.NamespaceName.empty())
			code << "\n}\n";
		return code.str();
	}

	std::string ProviderSource(
		const QualifiedName& provider,
		const QualifiedName& program,
		const std::string& publicHeaderBaseName,
		const std::string& programHeaderBaseName)
	{
		const auto programGenerated = program.Qualified() + "Generated";
		std::ostringstream code;
		code << "#include \"" << publicHeaderBaseName << ".g.h\"\n";
		code << "#include \"" << programHeaderBaseName << ".g.h\"\n";
		code << "#include \"Style.h\"\n";
		code << "#include \"StyleInfrastructure.h\"\n\n";
		code << "#include \"TemplateInfrastructure.h\"\n\n";
		code << "#include <exception>\n";
		code << "#include <mutex>\n";
		code << "#include <utility>\n\n";
		code << "namespace\n{\n";
		code << "\tstruct CuiGeneratedThemeCache final\n";
		code << "\t{\n";
		code << "\t\tstd::once_flag Initialize;\n";
		code << "\t\tstd::shared_ptr<const ControlStyleSheet> Styles;\n";
		code << "\t\tstd::wstring Error;\n";
		code << "\t};\n\n";
		code << "\tCuiGeneratedThemeCache& ThemeCache()\n";
		code << "\t{\n";
		code << "\t\tstatic CuiGeneratedThemeCache cache;\n";
		code << "\t\treturn cache;\n";
		code << "\t}\n\n";
		code << "\tvoid EnsureThemeBuilt()\n";
		code << "\t{\n";
		code << "\t\tauto& cache = ThemeCache();\n";
		code << "\t\tstd::call_once(cache.Initialize, [&]\n";
		code << "\t\t{\n";
		code << "\t\t\ttry\n";
		code << "\t\t\t{\n";
		code << "\t\t\t\tcache.Styles = " << programGenerated
			<< "::Build();\n";
		code << "\t\t\t\tif (!cache.Styles)\n";
		code << "\t\t\t\t\tcache.Error = "
			"L\"静态 Generic.xaml 未生成主题样式表。\";\n";
		code << "\t\t\t}\n";
		code << "\t\t\tcatch (const std::exception&)\n";
		code << "\t\t\t{\n";
		code << "\t\t\t\tcache.Error = "
			"L\"静态 Generic.xaml 构造发生运行时异常。\";\n";
		code << "\t\t\t}\n";
		code << "\t\t\tcatch (...)\n";
		code << "\t\t\t{\n";
		code << "\t\t\t\tcache.Error = "
			"L\"静态 Generic.xaml 构造发生未知异常。\";\n";
		code << "\t\t\t}\n";
		code << "\t\t});\n";
		code << "\t}\n";
		code << "}\n\n";
		code << "std::shared_ptr<const ControlStyleSheet>\n";
		code << provider.Qualified()
			<< "::DefaultStyleSheet(std::wstring* outError)\n";
		code << "{\n";
		code << "\tEnsureThemeBuilt();\n";
		code << "\tconst auto& cache = ThemeCache();\n";
		code << "\tif (outError) *outError = cache.Error;\n";
		code << "\treturn cache.Styles;\n";
		code << "}\n\n";
		code << "bool " << provider.Qualified() << "::Apply(\n";
		code << "\tControl& root,\n";
		code << "\tbool recursive,\n";
		code << "\tstd::wstring* outError)\n";
		code << "{\n";
		code << "\tauto styles = DefaultStyleSheet(outError);\n";
		code << "\tif (!styles) return false;\n";
		code << "\tif (!cui::framework::StyleAccess::SetTheme(\n";
		code << "\t\troot, std::move(styles), recursive))\n";
		code << "\t{\n";
		code << "\t\tif (outError) *outError = "
			"L\"静态 Generic.xaml 主题无法应用到控件树。\";\n";
		code << "\t\treturn false;\n";
		code << "\t}\n";
		code << "\tif (outError) outError->clear();\n";
		code << "\treturn true;\n";
		code << "}\n\n";
		code << "bool " << provider.Qualified()
			<< "::InstallTemplateValue(\n";
		code << "\tControl& target,\n";
		code << "\tconst std::wstring& templateKey,\n";
		code << "\tDependencyPropertyValueSource valueSource,\n";
		code << "\tstd::wstring* outError)\n";
		code << "{\n";
		code << "\tauto styles = DefaultStyleSheet(outError);\n";
		code << "\tBindingValue value;\n";
		code << "\tControlTemplateReference reference;\n";
		code << "\tif (!styles || !styles->TryGetResource("
			"templateKey, value)\n";
		code << "\t\t|| !value.TryGet(reference) || !reference)\n";
		code << "\t{\n";
		code << "\t\tif (outError) *outError = "
			"L\"静态 Generic.xaml ControlTemplate 资源不存在：\" "
			"+ templateKey;\n";
		code << "\t\treturn false;\n";
		code << "\t}\n";
		code << "\tif (!cui::framework::TemplateAccess::SetTemplate(\n";
		code << "\t\ttarget, std::move(reference), valueSource))\n";
		code << "\t{\n";
		code << "\t\tif (outError) *outError = "
			"L\"静态 Generic.xaml Template 值安装失败：\" "
			"+ templateKey;\n";
		code << "\t\treturn false;\n";
		code << "\t}\n";
		code << "\tif (outError) outError->clear();\n";
		code << "\treturn true;\n";
		code << "}\n\n";
		code << "bool " << provider.Qualified()
			<< "::ApplyTemplateVisualStates(\n";
		code << "\tControl& target,\n";
		code << "\tconst std::wstring& templateKey,\n";
		code << "\tstd::wstring* outError)\n";
		code << "{\n";
		code << "\t(void)target;\n";
		code << "\tauto styles = DefaultStyleSheet(outError);\n";
		code << "\tBindingValue value;\n";
		code << "\tControlTemplateReference reference;\n";
		code << "\tif (!styles || !styles->TryGetResource("
			"templateKey, value)\n";
		code << "\t\t|| !value.TryGet(reference) || !reference)\n";
		code << "\t{\n";
		code << "\t\tif (outError) *outError = "
			"L\"静态 Generic.xaml VisualState 模板资源不存在：\" "
			"+ templateKey;\n";
		code << "\t\treturn false;\n";
		code << "\t}\n";
		code << "\t// The generated template factory installs its compiled "
			"VisualState groups while applying the template.\n";
		code << "\tif (outError) outError->clear();\n";
		code << "\treturn true;\n";
		code << "}\n";
		return code.str();
	}

	bool ResolveOutputBase(
		const FrameworkThemeCodeGenerationOptions& options,
		std::filesystem::path& outputBase,
		std::wstring* outError)
	{
		if (options.OutputBasePath.empty())
		{
			SetError(outError,
				L"compile-theme 必须通过 --output 指定无扩展名输出基路径。");
			return false;
		}
		std::error_code error;
		outputBase = std::filesystem::absolute(
			std::filesystem::path(options.OutputBasePath), error);
		if (error)
		{
			SetError(outError,
				L"无法解析静态主题输出路径：" + Widen(error.message()));
			return false;
		}
		outputBase = outputBase.lexically_normal();
		const auto fileName = outputBase.filename().wstring();
		if (fileName.empty() || outputBase.has_extension())
		{
			SetError(outError, L"静态主题输出必须是无扩展名文件基路径。");
			return false;
		}
		return true;
	}

	struct ThemeClosureStatistics final
	{
		std::size_t SourceStyleRules = 0;
		std::size_t RetainedStyleRules = 0;
		std::size_t SourceControlTemplates = 0;
		std::size_t RetainedControlTemplates = 0;
		std::size_t SourceResources = 0;
		std::size_t RetainedResources = 0;
	};

	template<class T>
	void RetainSelected(
		std::vector<T>& values,
		const std::vector<bool>& selected)
	{
		std::vector<T> retained;
		retained.reserve(values.size());
		for (std::size_t index = 0; index < values.size(); ++index)
			if (index < selected.size() && selected[index])
				retained.push_back(std::move(values[index]));
		values = std::move(retained);
	}

	class FrameworkThemeDependencyClosure final
	{
	public:
		bool Apply(
			DesignDocument& theme,
			const std::vector<DesignDocument>& roots,
			const FrameworkThemeCodeGenerationOptions& options,
			ThemeClosureStatistics& statistics,
			std::wstring* outError)
		{
			DesignerStyleSheetUtils::Canonicalize(theme.StyleSheet);
			statistics.SourceStyleRules = theme.StyleSheet.Rules.size();
			statistics.SourceControlTemplates = theme.ControlTemplates.size();
			statistics.SourceResources = theme.StyleSheet.Resources.size();

			for (const auto& root : roots) AddDocumentUsage(root);
			for (const auto& typeName : options.PreservedTypes)
			{
				UIClass type = UIClass::UI_Base;
				if (!DesignerStyleSheetUtils::TryParseUIClass(typeName, type)
					|| type == UIClass::UI_Base)
				{
					SetError(outError,
						L"无法保留未知的主题控件类型：" + typeName);
					return false;
				}
				AddType(type);
			}
			for (const auto& key : options.PreservedResources)
			{
				const auto hasKey = [&]
				{
					if (std::any_of(theme.StyleSheet.Resources.begin(),
						theme.StyleSheet.Resources.end(), [&](const auto& value)
							{ return value.Key == key; })) return true;
					if (std::any_of(theme.StyleSheet.Rules.begin(),
						theme.StyleSheet.Rules.end(), [&](const auto& value)
							{ return value.Id == key; })) return true;
					if (std::any_of(theme.ControlTemplates.begin(),
						theme.ControlTemplates.end(), [&](const auto& value)
							{ return value.Key == key; })) return true;
					if (std::any_of(theme.DataTemplates.begin(),
						theme.DataTemplates.end(), [&](const auto& value)
							{ return value.Key == key; })) return true;
					if (std::any_of(theme.ItemsPanelTemplates.begin(),
						theme.ItemsPanelTemplates.end(), [&](const auto& value)
							{ return value.Key == key; })) return true;
					if (std::any_of(theme.GroupStyles.begin(),
						theme.GroupStyles.end(), [&](const auto& value)
							{ return value.Key == key; })) return true;
					if (std::any_of(theme.DataLists.begin(),
						theme.DataLists.end(), [&](const auto& value)
							{ return value.Key == key; })) return true;
					return std::any_of(theme.CollectionViews.begin(),
						theme.CollectionViews.end(), [&](const auto& value)
							{ return value.Key == key; });
				}();
				if (!hasKey)
				{
					SetError(outError,
						L"无法保留不存在的主题资源：" + key);
					return false;
				}
				AddResource(key);
			}

			std::vector<bool> selectedRules(
				theme.StyleSheet.Rules.size(), false);
			std::vector<bool> selectedComponents(
				theme.Components.size(), false);
			std::vector<bool> selectedControlTemplates(
				theme.ControlTemplates.size(), false);
			std::vector<bool> selectedDataTemplates(
				theme.DataTemplates.size(), false);
			std::vector<bool> selectedItemsPanelTemplates(
				theme.ItemsPanelTemplates.size(), false);
			std::vector<bool> selectedGroupStyles(
				theme.GroupStyles.size(), false);
			std::vector<bool> selectedDataLists(
				theme.DataLists.size(), false);
			std::vector<bool> selectedCollectionViews(
				theme.CollectionViews.size(), false);

			bool progressed = false;
			do
			{
				progressed = false;
				for (std::size_t index = 0;
					index < theme.StyleSheet.Rules.size(); ++index)
				{
					if (selectedRules[index]) continue;
					const auto& rule = theme.StyleSheet.Rules[index];
					if (!Matches(rule)) continue;
					selectedRules[index] = true;
					progressed = true;
					AddRuleUsage(rule, true);
				}
				for (std::size_t index = 0;
					index < theme.Components.size(); ++index)
				{
					if (selectedComponents[index]) continue;
					const auto& definition = theme.Components[index];
					if (!_componentTypes.contains(
						definition.Type.RegistryKey())) continue;
					selectedComponents[index] = true;
					progressed = true;
					AddComponentUsage(definition);
				}
				for (std::size_t index = 0;
					index < theme.ControlTemplates.size(); ++index)
				{
					if (selectedControlTemplates[index]) continue;
					const auto& definition = theme.ControlTemplates[index];
					const bool implicitMatch = definition.IsImplicit()
						&& ((definition.TargetType != UIClass::UI_Base
							&& _types.contains(definition.TargetType))
							|| (!definition.TargetComponentType.Empty()
								&& _componentTypes.contains(
									definition.TargetComponentType.RegistryKey())));
					if (!implicitMatch
						&& (definition.Key.empty()
							|| !_resources.contains(definition.Key))) continue;
					selectedControlTemplates[index] = true;
					progressed = true;
					AddControlTemplateUsage(definition);
				}
				for (std::size_t index = 0;
					index < theme.DataTemplates.size(); ++index)
				{
					if (selectedDataTemplates[index]) continue;
					const auto& definition = theme.DataTemplates[index];
					const bool implicitMatch = definition.IsImplicit()
						&& !definition.DataType.empty()
						&& _dataTypes.contains(definition.DataType);
					if (!implicitMatch
						&& (definition.Key.empty()
							|| !_resources.contains(definition.Key))) continue;
					selectedDataTemplates[index] = true;
					progressed = true;
					AddDataTemplateUsage(definition);
				}
				for (std::size_t index = 0;
					index < theme.ItemsPanelTemplates.size(); ++index)
				{
					if (selectedItemsPanelTemplates[index]) continue;
					const auto& definition = theme.ItemsPanelTemplates[index];
					if (definition.Key.empty()
						|| !_resources.contains(definition.Key)) continue;
					selectedItemsPanelTemplates[index] = true;
					progressed = true;
					AddItemsPanelTemplateUsage(definition);
				}
				for (std::size_t index = 0;
					index < theme.GroupStyles.size(); ++index)
				{
					if (selectedGroupStyles[index]) continue;
					const auto& definition = theme.GroupStyles[index];
					if (definition.Key.empty()
						|| !_resources.contains(definition.Key)) continue;
					selectedGroupStyles[index] = true;
					progressed = true;
					AddGroupStyleUsage(definition);
				}
				for (std::size_t index = 0;
					index < theme.DataLists.size(); ++index)
				{
					if (selectedDataLists[index]) continue;
					const auto& definition = theme.DataLists[index];
					if (definition.Key.empty()
						|| !_resources.contains(definition.Key)) continue;
					selectedDataLists[index] = true;
					progressed = true;
					if (!definition.ItemType.empty())
						_dataTypes.insert(definition.ItemType);
				}
				for (std::size_t index = 0;
					index < theme.CollectionViews.size(); ++index)
				{
					if (selectedCollectionViews[index]) continue;
					const auto& definition = theme.CollectionViews[index];
					if (definition.Key.empty()
						|| !_resources.contains(definition.Key)) continue;
					selectedCollectionViews[index] = true;
					progressed = true;
					AddResource(definition.SourceResource);
				}
			}
			while (progressed);

			std::vector<bool> selectedDataTypes(
				theme.DataTypes.size(), false);
			for (std::size_t index = 0;
				index < theme.DataTypes.size(); ++index)
				selectedDataTypes[index] = _dataTypes.contains(
					theme.DataTypes[index].Name);

			std::vector<bool> selectedResources(
				theme.StyleSheet.Resources.size(), false);
			for (std::size_t index = 0;
				index < theme.StyleSheet.Resources.size(); ++index)
				selectedResources[index] = _resources.contains(
					theme.StyleSheet.Resources[index].Key);

			RetainSelected(theme.StyleSheet.Rules, selectedRules);
			RetainSelected(theme.StyleSheet.Resources, selectedResources);
			RetainSelected(theme.Components, selectedComponents);
			RetainSelected(theme.ControlTemplates, selectedControlTemplates);
			RetainSelected(theme.DataTemplates, selectedDataTemplates);
			RetainSelected(theme.DataTypes, selectedDataTypes);
			RetainSelected(
				theme.ItemsPanelTemplates, selectedItemsPanelTemplates);
			RetainSelected(theme.GroupStyles, selectedGroupStyles);
			RetainSelected(theme.DataLists, selectedDataLists);
			RetainSelected(theme.CollectionViews, selectedCollectionViews);

			statistics.RetainedStyleRules = theme.StyleSheet.Rules.size();
			statistics.RetainedControlTemplates =
				theme.ControlTemplates.size();
			statistics.RetainedResources = theme.StyleSheet.Resources.size();
			return true;
		}

	private:
		std::set<UIClass> _types;
		std::set<std::wstring> _runtimeTypes;
		std::set<std::wstring> _componentTypes;
		std::set<std::wstring> _dataTypes;
		std::set<std::wstring> _resources;

		void AddType(UIClass type)
		{
			if (type == UIClass::UI_Base || !_types.insert(type).second)
				return;
			// DataGrid materializes these chrome/container controls from native
			// code rather than from the authored application tree. A closure theme
			// rooted only at DataGrid must still retain their implicit styles.
			if (type == UIClass::UI_DataGrid)
			{
				AddType(UIClass::UI_DataGridCell);
				AddType(UIClass::UI_DataGridColumnHeader);
				AddType(UIClass::UI_DataGridRowHeader);
				// DataGrid creates these column elements in native code, so their
				// automatic keyed styles are invisible to authored-XAML closure
				// discovery and must be retained explicitly.
				AddResource(L"CuiDataGridTextElementStyle");
				AddResource(L"CuiDataGridTextEditingElementStyle");
				AddResource(L"CuiDataGridCheckBoxElementStyle");
			}
			if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type))
			{
				const auto container = GetDefaultItemContainerType(type);
				if (container != UIClass::UI_Base && container != type)
					AddType(container);
			}
		}

		void AddResource(const std::wstring& key)
		{
			if (!key.empty()) _resources.insert(key);
		}

		void AddRuntimeType(const RuntimeTypeId& type)
		{
			if (type.Valid()) _runtimeTypes.insert(type.RegistryKey());
		}

		void AddComponentType(const DesignerComponentType& type)
		{
			if (!type.Empty()) _componentTypes.insert(type.RegistryKey());
		}

		void AddAnimationUsage(const DesignerVisualStateAnimation& animation)
		{
			if (animation.FromUsesResource)
				AddResource(animation.FromResourceKey);
			if (animation.ToUsesResource)
				AddResource(animation.ToResourceKey);
			if (animation.ByUsesResource)
				AddResource(animation.ByResourceKey);
			for (const auto& frame : animation.KeyFrames)
				if (frame.UsesResource) AddResource(frame.ResourceKey);
		}

		void AddActionUsage(const DesignerEventTriggerAction& action)
		{
			for (const auto& animation : action.Animations)
				AddAnimationUsage(animation);
		}

		void AddActionsUsage(
			const std::vector<DesignerEventTriggerAction>& actions)
		{
			for (const auto& action : actions) AddActionUsage(action);
		}

		void AddVisualStateGroupsUsage(
			const std::vector<DesignerVisualStateGroup>& groups)
		{
			for (const auto& group : groups)
			{
				for (const auto& state : group.States)
				{
					for (const auto& setter : state.Setters)
						if (setter.UsesResource)
							AddResource(setter.ResourceKey);
					for (const auto& animation : state.Animations)
						AddAnimationUsage(animation);
				}
				for (const auto& transition : group.Transitions)
					for (const auto& animation : transition.Animations)
						AddAnimationUsage(animation);
			}
		}

		void AddEventTriggersUsage(
			const std::vector<DesignerEventTrigger>& triggers)
		{
			for (const auto& trigger : triggers)
				AddActionsUsage(trigger.Actions);
		}

		void AddSetterUsage(const DesignerStyleSetter& setter)
		{
			if (setter.UsesResource) AddResource(setter.ResourceKey);
		}

		bool TryParseBasedOnType(
			const std::wstring& key,
			UIClass& type) const
		{
			auto text = DesignerStyleSheetUtils::Trim(key);
			if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
				return false;
			text = DesignerStyleSheetUtils::Trim(
				text.substr(1, text.size() - 2));
			if (text.size() < 6) return false;
			std::wstring prefix = text.substr(0, 6);
			std::transform(prefix.begin(), prefix.end(), prefix.begin(),
				[](wchar_t value)
				{
					return static_cast<wchar_t>(std::towlower(value));
				});
			if (prefix != L"x:type") return false;
			text = DesignerStyleSheetUtils::Trim(text.substr(6));
			const auto separator = text.find(L':');
			if (separator != std::wstring::npos)
				text = DesignerStyleSheetUtils::Trim(
					text.substr(separator + 1));
			return DesignerStyleSheetUtils::TryParseUIClass(text, type);
		}

		void AddBasedOnUsage(const std::wstring& basedOn)
		{
			if (basedOn.empty()) return;
			UIClass type = UIClass::UI_Base;
			if (TryParseBasedOnType(basedOn, type))
				AddType(type);
			else
				AddResource(basedOn);
		}

		void AddRuleUsage(
			const DesignerStyleRule& rule,
			bool addTarget)
		{
			if (addTarget)
			{
				if (rule.HasType) AddType(rule.Type);
				AddRuntimeType(rule.XamlType);
				AddComponentType(rule.ComponentType);
			}
			AddBasedOnUsage(rule.BasedOn);
			for (const auto& setter : rule.Setters)
				AddSetterUsage(setter);
			AddActionsUsage(rule.EnterActions);
			AddActionsUsage(rule.ExitActions);
			for (const auto& trigger : rule.Triggers)
			{
				for (const auto& setter : trigger.Setters)
					AddSetterUsage(setter);
				AddActionsUsage(trigger.EnterActions);
				AddActionsUsage(trigger.ExitActions);
			}
		}

		void AddStyleSheetUsage(const DesignerStyleSheet& sheet)
		{
			for (const auto& rule : sheet.Rules)
				AddRuleUsage(rule, true);
		}

		void AddObjectResourcesUsage(
			const DesignObjectResourceDictionary& resources)
		{
			for (const auto& definition : resources.Components)
				AddComponentUsage(definition);
			for (const auto& definition : resources.ControlTemplates)
				AddControlTemplateUsage(definition);
			for (const auto& definition : resources.DataTemplates)
				AddDataTemplateUsage(definition);
			for (const auto& definition : resources.ItemsPanelTemplates)
				AddItemsPanelTemplateUsage(definition);
			for (const auto& definition : resources.GroupStyles)
				AddGroupStyleUsage(definition);
		}

		void AddNodeUsage(const DesignNode& node)
		{
			AddType(node.Type);
			AddRuntimeType(node.XamlType);
			AddComponentType(node.ComponentType);
			AddResource(node.Properties.StyleResourceKey);
			for (const auto& [name, assignment] : node.Properties.Values)
			{
				(void)name;
				AddResource(assignment.ResourceKey);
				AddResource(assignment.DynamicResourceKey);
			}
			AddResource(node.Structure.ItemsSourceResource);
			AddResource(node.Structure.ItemTemplate);
			AddResource(node.Structure.ContentTemplate);
			AddResource(node.Structure.HeaderTemplate);
			AddResource(node.Structure.ControlTemplate);
			AddResource(node.Structure.GroupStyle);
			AddResource(node.Structure.ItemsPanel);
			AddResource(node.Structure.ItemContainerStyle);
			AddResource(node.TemplateState.AppliedControlTemplate);
			AddResource(node.TemplateState.AppliedControlTemplateResource);
			AddStyleSheetUsage(node.LocalResources);
			AddObjectResourcesUsage(node.LocalObjectResources);
		}

		void AddNodesUsage(const std::vector<DesignNode>& nodes)
		{
			for (const auto& node : nodes) AddNodeUsage(node);
		}

		void AddComponentUsage(const DesignComponentDefinition& definition)
		{
			AddComponentType(definition.Type);
			AddType(definition.BaseType);
			for (const auto& property : definition.Properties)
				AddResource(property.DefaultResourceKey);
			AddNodesUsage(definition.Template);
			AddVisualStateGroupsUsage(definition.VisualStateGroups);
			AddEventTriggersUsage(definition.EventTriggers);
		}

		void AddControlTemplateUsage(const DesignControlTemplate& definition)
		{
			AddType(definition.TargetType);
			AddComponentType(definition.TargetComponentType);
			AddNodesUsage(definition.Template);
			AddVisualStateGroupsUsage(definition.VisualStateGroups);
			AddEventTriggersUsage(definition.EventTriggers);
		}

		void AddDataTemplateUsage(const DesignDataTemplate& definition)
		{
			if (!definition.DataType.empty())
				_dataTypes.insert(definition.DataType);
			AddNodesUsage(definition.Template);
		}

		void AddItemsPanelTemplateUsage(
			const DesignItemsPanelTemplate& definition)
		{
			switch (definition.Value.Kind)
			{
			case ItemsPanelKind::Wrap:
				AddType(UIClass::UI_WrapPanel);
				break;
			case ItemsPanelKind::VirtualizingStack:
				AddType(UIClass::UI_Panel);
				break;
			case ItemsPanelKind::Stack:
			default:
				AddType(UIClass::UI_StackPanel);
				break;
			}
		}

		void AddGroupStyleUsage(const DesignGroupStyle& definition)
		{
			if (!definition.HeaderTemplate.empty())
				AddResource(definition.HeaderTemplate);
			else
				_dataTypes.insert(
					std::wstring(CollectionViewGroupDataTypeName));
		}

		void AddDocumentUsage(const DesignDocument& document)
		{
			AddNodeUsage(document.Window);
			AddNodesUsage(document.Nodes);
			AddStyleSheetUsage(document.StyleSheet);
			for (const auto& property : document.DataContextSchema)
			{
				if (!property.ItemType.empty())
					_dataTypes.insert(property.ItemType);
				if (!property.DataType.empty())
					_dataTypes.insert(property.DataType);
			}
			for (const auto& definition : document.Components)
				AddComponentUsage(definition);
			for (const auto& definition : document.ControlTemplates)
				AddControlTemplateUsage(definition);
			for (const auto& definition : document.DataTemplates)
				AddDataTemplateUsage(definition);
			for (const auto& definition : document.ItemsPanelTemplates)
				AddItemsPanelTemplateUsage(definition);
			for (const auto& definition : document.GroupStyles)
				AddGroupStyleUsage(definition);
			for (const auto& dataList : document.DataLists)
				if (!dataList.ItemType.empty())
					_dataTypes.insert(dataList.ItemType);
			for (const auto& view : document.CollectionViews)
				AddResource(view.SourceResource);
		}

		bool Matches(const DesignerStyleRule& rule) const
		{
			if (!rule.Id.empty() && _resources.contains(rule.Id))
				return true;
			// A TargetType on a keyed Style is a compatibility constraint, not
			// an implicit-style root. Pull keyed rules only through their key.
			if (!rule.Id.empty()) return false;
			if (!rule.ComponentType.Empty())
				return _componentTypes.contains(
					rule.ComponentType.RegistryKey());
			if (rule.HasType && _types.contains(rule.Type))
				return true;
			return !rule.HasType && rule.XamlType.Valid()
				&& _runtimeTypes.contains(rule.XamlType.RegistryKey());
		}
	};

	bool LoadClosureRoots(
		const std::vector<std::wstring>& paths,
		std::vector<DesignDocument>& roots,
		std::vector<std::wstring>& normalizedPaths,
		std::wstring* outError)
	{
		std::set<std::wstring> unique;
		for (const auto& sourcePath : paths)
		{
			std::error_code pathError;
			auto path = std::filesystem::absolute(
				std::filesystem::path(sourcePath), pathError);
			if (pathError)
			{
				SetError(outError, L"无法解析主题闭包 XAML 根路径："
					+ sourcePath + L"：" + Widen(pathError.message()));
				return false;
			}
			path = path.lexically_normal();
			if (!unique.insert(path.wstring()).second) continue;

			XamlDocumentParseOptions parseOptions;
			parseOptions.CaptureSourceSpans = false;
			parseOptions.ResourceBasePath = path.parent_path().wstring();
			DesignDocument document;
			std::wstring parseError;
			if (!XamlDocumentParser::LoadFromFile(
				path.wstring(), document, parseOptions, &parseError))
			{
				SetError(outError, L"主题闭包无法读取应用 XAML 根 "
					+ path.wstring() + L"："
					+ (parseError.empty()
						? L"未知解析错误。" : parseError));
				return false;
			}
			normalizedPaths.push_back(path.wstring());
			roots.push_back(std::move(document));
		}
		return true;
	}
}

std::vector<std::wstring>
FrameworkThemeCodeGenerationResult::OutputFiles() const
{
	return {
		GeneratedHeaderPath,
		GeneratedSourcePath,
		ProgramHeaderPath,
		ProgramSourcePath,
	};
}

bool FrameworkThemeCodeGenerator::GenerateFile(
	const std::wstring& resourceDictionaryPath,
	const FrameworkThemeCodeGenerationOptions& options,
	FrameworkThemeCodeGenerationResult* outResult,
	std::wstring* outError)
{
	try
	{
		if (resourceDictionaryPath.empty())
		{
			SetError(outError, L"compile-theme 缺少 ResourceDictionary 路径。");
			return false;
		}

		std::filesystem::path outputBase;
		if (!ResolveOutputBase(options, outputBase, outError))
			return false;

		QualifiedName provider;
		if (!ParseQualifiedName(
			options.ClassName.empty()
				? L"CuiGeneratedFrameworkTheme" : options.ClassName,
			provider, outError))
			return false;
		QualifiedName program = provider;
		program.Leaf += "Program";

		XamlDocumentParseOptions parseOptions;
		parseOptions.CaptureSourceSpans = false;
		std::error_code pathError;
		const auto input = std::filesystem::absolute(
			std::filesystem::path(resourceDictionaryPath), pathError);
		if (pathError)
		{
			SetError(outError,
				L"无法解析 ResourceDictionary 路径："
				+ Widen(pathError.message()));
			return false;
		}
		parseOptions.ResourceBasePath = input.parent_path().wstring();

		DesignDocument document;
		if (!XamlDocumentParser::LoadResourceDictionary(
			input.wstring(), document, parseOptions, outError))
			return false;

		const bool applicationClosure = !options.RootDocuments.empty()
			|| !options.PreservedTypes.empty()
			|| !options.PreservedResources.empty();
		std::vector<DesignDocument> closureRoots;
		std::vector<std::wstring> normalizedRootPaths;
		ThemeClosureStatistics closureStatistics;
		if (applicationClosure)
		{
			if (!LoadClosureRoots(
				options.RootDocuments,
				closureRoots,
				normalizedRootPaths,
				outError)) return false;
			FrameworkThemeDependencyClosure closure;
			if (!closure.Apply(
				document,
				closureRoots,
				options,
				closureStatistics,
				outError)) return false;
		}
		else
		{
			closureStatistics.SourceStyleRules =
				closureStatistics.RetainedStyleRules =
					document.StyleSheet.Rules.size();
			closureStatistics.SourceControlTemplates =
				closureStatistics.RetainedControlTemplates =
					document.ControlTemplates.size();
			closureStatistics.SourceResources =
				closureStatistics.RetainedResources =
					document.StyleSheet.Resources.size();
		}
		document.Window.Name = L"__cuiGeneratedFrameworkThemeProgram";
		document.Window.Properties = {};
		document.Window.Structure = {};
		document.Window.Events.clear();
		document.Window.Bindings.clear();
		document.Window.CommandBindings.clear();
		document.Window.InputBindings.clear();
		document.Window.LocalResources = {};
		document.Window.LocalObjectResources = {};
		document.Window.TemplateBindings.clear();
		document.Window.TemplateEventBindings.clear();
		document.Nodes.clear();
		document.CodeBehind = {};

		std::wstring validationError;
		if (!CodeGenerator::ValidateDocument(document, &validationError))
		{
			SetError(outError, validationError.empty()
				? L"Generic.xaml 不满足静态主题代码生成约束。"
				: std::move(validationError));
			return false;
		}

		CodeGenerator programGenerator(
			std::wstring(options.ClassName.empty()
				? L"CuiGeneratedFrameworkTheme" : options.ClassName)
				+ L"Program",
			document,
			CodeGeneratorOutputKind::FrameworkThemeProgram);
		const auto publicHeaderBase =
			NarrowAscii(outputBase.filename().wstring());
		const auto programBase = outputBase.wstring() + L".program";
		const auto programHeaderBase =
			NarrowAscii(std::filesystem::path(programBase).filename().wstring());

		const auto generatedHeader = ProviderHeader(provider);
		const auto generatedSource = ProviderSource(
			provider, program, publicHeaderBase, programHeaderBase);
		const auto programHeader = programGenerator.GenerateHeader();
		const auto programSource =
			programGenerator.GenerateCppForHeader(programHeaderBase);

		const auto generatedHeaderPath = outputBase.wstring() + L".g.h";
		const auto generatedSourcePath = outputBase.wstring() + L".g.cpp";
		const auto programHeaderPath = programBase + L".g.h";
		const auto programSourcePath = programBase + L".g.cpp";
		std::error_code directoryError;
		std::filesystem::create_directories(
			outputBase.parent_path(), directoryError);
		if (directoryError)
		{
			SetError(outError,
				L"无法创建静态主题输出目录："
				+ Widen(directoryError.message()));
			return false;
		}
		const std::vector<AtomicFileWriteEntry> writes{
			{ generatedHeaderPath, generatedHeader },
			{ generatedSourcePath, generatedSource },
			{ programHeaderPath, programHeader },
			{ programSourcePath, programSource },
		};
		if (!AtomicFile::WriteBatch(writes, outError))
			return false;

		if (outResult)
		{
			outResult->ResourceDictionaryPath = input.wstring();
			outResult->OutputBasePath = outputBase.wstring();
			outResult->ClassName =
				options.ClassName.empty()
					? L"CuiGeneratedFrameworkTheme" : options.ClassName;
			outResult->GeneratedHeaderPath = generatedHeaderPath;
			outResult->GeneratedSourcePath = generatedSourcePath;
			outResult->ProgramHeaderPath = programHeaderPath;
			outResult->ProgramSourcePath = programSourcePath;
			outResult->RootDocuments = std::move(normalizedRootPaths);
			outResult->ApplicationClosure = applicationClosure;
			outResult->SourceStyleRuleCount =
				closureStatistics.SourceStyleRules;
			outResult->RetainedStyleRuleCount =
				closureStatistics.RetainedStyleRules;
			outResult->SourceControlTemplateCount =
				closureStatistics.SourceControlTemplates;
			outResult->RetainedControlTemplateCount =
				closureStatistics.RetainedControlTemplates;
			outResult->SourceResourceCount =
				closureStatistics.SourceResources;
			outResult->RetainedResourceCount =
				closureStatistics.RetainedResources;
		}
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError, L"静态主题代码生成失败："
			+ Widen(error.what()));
	}
	catch (...)
	{
		SetError(outError, L"静态主题代码生成失败：未知异常。");
	}
	return false;
}
}
