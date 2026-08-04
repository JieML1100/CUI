#include "../include/XamlObjectMaterializer.h"
#include "../include/BindingConverterRegistry.h"
#include "../include/XamlDocumentCompiler.h"
#include "../include/XamlFrameworkTheme.h"
#include "../../CUI/include/HeaderedItemsControl.h"
#include "../../CUI/include/ToolBar.h"
#include "../include/XamlRuntimeSchema.h"

#include "../../CuiDesigner/DesignerModel/DesignDocumentControlPool.h"
#include "../../CuiDesigner/DesignerModel/DesignDataResourceUtils.h"
#include "../../CuiDesigner/DesignerModel/DesignDocumentGraph.h"
#include "../../CuiDesigner/DesignerModel/StoryboardPropertyPath.h"
#include "../../CuiDesigner/DesignerBindingUtils.h"
#include "../../CuiDesigner/DesignerDataContextSchemaUtils.h"
#include "../../CuiDesigner/DesignerEventCatalog.h"
#include "../../CuiDesigner/DesignerPropertyCatalog.h"
#include "../../CuiDesigner/DesignerStyleSheetUtils.h"
#include "../../CUI/include/Panel.h"
#include "../../CUI/include/XamlInfrastructure.h"
#include "../../CUI/include/RoutedEventInfrastructure.h"
#include "../../CUI/include/DependencyPropertyInfrastructure.h"
#include "../../CUI/include/StyleInfrastructure.h"
#include "../../CUI/include/Button.h"
#include "../../CUI/include/ToggleButton.h"
#include "../../CUI/include/CheckBox.h"
#include "../../CUI/include/ComboBox.h"
#include "../../CUI/include/Expander.h"
#include "../../CUI/include/GroupBox.h"
#include "../../CUI/include/Label.h"
#include "../../CUI/include/LoadingRing.h"
#include "../../CUI/include/NumericUpDown.h"
#include "../../CUI/include/PasswordBox.h"
#include "../../CUI/include/Image.h"
#include "../../CUI/include/ProgressBar.h"
#include "../../CUI/include/ProgressRing.h"
#include "../../CUI/include/RadioButton.h"
#include "../../CUI/include/RichTextBox.h"
#include "../../CUI/include/ScrollViewer.h"
#include "../../CUI/include/Slider.h"
#include "../../CUI/include/Switch.h"
#include "../../CUI/include/TextBox.h"
#include "../../CUI/include/WebBrowser.h"
#include "../../CUI/include/ListView.h"
#include "../../CUI/include/ListBox.h"
#include "../../CUI/include/Selector.h"
#include "../../CUI/include/ChartView.h"
#include "../../CUI/include/TreeView.h"
#include "../../CUI/include/TabControl.h"
#include "../../CUI/include/ToolBar.h"
#include "../../CUI/include/Menu.h"
#include "../../CUI/include/ContextMenu.h"
#include "../../CUI/include/Popup.h"
#include "../../CUI/include/StatusBar.h"
#include "../../CUI/include/MediaPlayer.h"
#include "../../CUI/include/NativeSurface.h"
#include "../../CUI/include/ItemsControl.h"
#include "../../CUI/include/ItemsPresenter.h"
#include "../../CUI/include/ContentPresenter.h"
#include "../../CUI/include/ContentControl.h"
#include "../../CUI/include/HeaderedContentControl.h"
#include "../../CUI/include/TemplateInfrastructure.h"
#include "../../CUI/include/CalendarView.h"
#include "../../CUI/include/Layout/StackPanel.h"
#include "../../CUI/include/Layout/Grid.h"
#include "../../CUI/include/Layout/DockPanel.h"
#include "../../CUI/include/Layout/WrapPanel.h"
#include "../../CUI/include/Layout/RelativePanel.h"
#include <Convert.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using DesignValue = DesignerModel::DesignValue;
using namespace DesignerModel;

namespace
{
	class ExitAction final
	{
	public:
		explicit ExitAction(std::function<void()> action)
			: _action(std::move(action)) {}
		~ExitAction() { if (_action) _action(); }
		ExitAction(const ExitAction&) = delete;
		ExitAction& operator=(const ExitAction&) = delete;
	private:
		std::function<void()> _action;
	};

	static bool IsComponentContentPresenterType(UIClass type) noexcept
	{
		switch (type)
		{
		case UIClass::UI_Panel:
		case UIClass::UI_Canvas:
		case UIClass::UI_StackPanel:
		case UIClass::UI_WrapPanel:
		case UIClass::UI_DockPanel:
		case UIClass::UI_Grid:
		case UIClass::UI_RelativePanel:
		case UIClass::UI_Decorator:
		case UIClass::UI_Border:
			return true;
		default:
			return false;
		}
	}

	static bool IsContentHostType(UIClass type) noexcept
	{
		return type == UIClass::UI_ContentPresenter
			|| IsUIClassAssignableFrom(UIClass::UI_ContentControl, type);
	}

	static bool IsHeaderedContentControlType(UIClass type) noexcept
	{
		return IsUIClassAssignableFrom(
			UIClass::UI_HeaderedContentControl, type)
			|| IsUIClassAssignableFrom(
				UIClass::UI_HeaderedItemsControl, type);
	}

	static bool RegisterTemplateHeaderPresenter(
		Control* owner, ContentPresenter* presenter)
	{
		if (auto* host = dynamic_cast<HeaderedContentControl*>(owner))
			return host->RegisterTemplateHeaderPresenter(presenter);
		if (auto* host = dynamic_cast<HeaderedItemsControl*>(owner))
			return host->RegisterTemplateHeaderPresenter(presenter);
		return false;
	}

	static bool SetVisualHeader(
		Control* owner, std::unique_ptr<Control> header)
	{
		if (auto* host = dynamic_cast<HeaderedContentControl*>(owner))
		{
			host->SetVisualHeader(std::move(header));
			return true;
		}
		if (auto* host = dynamic_cast<HeaderedItemsControl*>(owner))
		{
			host->SetVisualHeader(std::move(header));
			return true;
		}
		return false;
	}

	static bool IsControlTemplateHostType(UIClass type) noexcept
	{
		return IsControlTemplateHostClass(type);
	}

	static bool IsControlTemplateTargetCompatible(
		UIClass actual, UIClass target) noexcept
	{
		return IsUIClassAssignableFrom(target, actual);
	}

	static bool IsControlTemplateTargetCompatible(
		const DesignerModel::DesignNode& actual,
		const DesignerModel::DesignControlTemplate& target) noexcept
	{
		if (!target.TargetComponentType.Empty())
			return !actual.ComponentType.Empty()
				&& actual.ComponentType.RegistryKey()
					== target.TargetComponentType.RegistryKey();
		return IsControlTemplateTargetCompatible(actual.Type, target.TargetType);
	}

	static const DesignerModel::DesignNode* ParentNode(
		const std::vector<DesignerModel::DesignNode>& nodes,
		const DesignerModel::DesignNode& node)
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(nodes.begin(), nodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			if (found != nodes.end()) return &*found;
		}
		if (!node.ParentRef.empty())
		{
			const auto found = std::find_if(nodes.begin(), nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Name == node.ParentRef; });
			if (found != nodes.end()) return &*found;
		}
		return nullptr;
	}

	static void ProjectDirectItemContainerStyles(
		DesignerModel::DesignDocument& document,
		const DesignerModel::DesignDocument* theme)
	{
		for (const auto& owner : document.Nodes)
		{
			if (owner.Structure.ItemContainerStyle.empty()
				|| !IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, owner.Type)) continue;
			const auto containerType = GetDefaultItemContainerType(owner.Type);
			if (containerType == UIClass::UI_Base) continue;
			for (auto& child : document.Nodes)
			{
				if (child.Structure.ChildRole != DesignNodeChildRole::Default
					|| !child.Properties.StyleResourceKey.empty()
					|| !IsUIClassAssignableFrom(containerType, child.Type)) continue;
				const auto* parent = ParentNode(document.Nodes, child);
				if (parent != &owner) continue;
				child.Properties.StyleResourceKey =
					owner.Structure.ItemContainerStyle;
			}
		}

		if (!theme) return;

		// WPF ToolBar.PrepareContainerForItemOverride supplies a type-specific
		// keyed Style to directly hosted controls. Project that resource reference
		// before ControlTemplate expansion so Designer does not first materialize
		// the ordinary Button template and replace it during Theme application.
		// It remains a dynamic resource reference: a consumer may override the
		// ToolBar key, while Designer must not persist the derived element Style.
		for (const auto& owner : document.Nodes)
		{
			if (owner.Type != UIClass::UI_ToolBar
				|| !owner.Structure.ItemContainerStyle.empty()) continue;
			for (auto& child : document.Nodes)
			{
				if (child.Structure.ChildRole != DesignNodeChildRole::Default
					|| !child.Properties.StyleResourceKey.empty()) continue;
				const auto* parent = ParentNode(document.Nodes, child);
				if (parent != &owner) continue;
				const auto* key = ToolBar::DefaultItemStyleResourceKey(
					child.Type);
				if (!key) continue;
				child.Properties.StyleResourceKey = key;
				child.TemplateState.StyleResourceScopeFromTheme = false;
				child.TemplateState.StyleResourceIsAutomatic = true;
			}
		}
	}

	static DesignerStyleSheet VisibleStyleSheet(
		const DesignerModel::DesignDocument& document,
		const std::vector<DesignerModel::DesignNode>& nodes,
		const DesignerModel::DesignNode& origin)
	{
		DesignerStyleSheet result = document.StyleSheet;
		std::vector<const DesignerModel::DesignNode*> route;
		std::unordered_set<int> visited;
		for (auto* scope = &origin;
			scope && visited.insert(scope->Id).second;
			scope = ParentNode(nodes, *scope))
			route.push_back(scope);
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
			DesignerStyleSheetUtils::AppendLexicalScope(
				result, (*scope)->LocalResources);
		return result;
	}

	static bool StyleRuleMatchesNode(
		const DesignerStyleRule& rule,
		const DesignerModel::DesignNode& node)
	{
		if (rule.HasType && rule.Type != UIClass::UI_Base
			&& rule.Type != node.Type) return false;
		if (!rule.ComponentType.Empty()
			&& (node.ComponentType.Empty()
				|| rule.ComponentType.RegistryKey()
					!= node.ComponentType.RegistryKey())) return false;
		if (rule.XamlType.Valid() && rule.XamlType != node.XamlType) return false;
		if (!rule.Id.empty())
		{
			if (node.Properties.StyleResourceKey != rule.Id) return false;
		}
		return true;
	}

	static bool ResolveStyledControlTemplateKeyFromSheet(
		const DesignerStyleSheet& source,
		const DesignerModel::DesignNode& owner,
		std::wstring& key,
		std::wstring* outError,
		bool* outMatchedStyle = nullptr)
	{
		key.clear();
		if (outMatchedStyle) *outMatchedStyle = false;
		DesignerStyleSheet resolved;
		if (!DesignerStyleSheetUtils::ResolveInheritance(
			source, resolved, outError))
			return false;
		const DesignerStyleRule* effectiveStyle = nullptr;
		for (auto item = resolved.Rules.rbegin(); item != resolved.Rules.rend(); ++item)
		{
			if (!StyleRuleMatchesNode(*item, owner)) continue;
			if (!owner.Properties.StyleResourceKey.empty()
				&& item->Id.empty()) continue;
			if (owner.Properties.StyleResourceKey.empty()
				&& !item->Id.empty()) continue;
			effectiveStyle = &*item;
			break;
		}
		if (effectiveStyle)
		{
			if (outMatchedStyle) *outMatchedStyle = true;
			const auto& rule = *effectiveStyle;
			const auto setter = std::find_if(
				rule.Setters.begin(), rule.Setters.end(), [](const auto& candidate)
				{ return candidate.PropertyName == L"Template"; });
			if (setter != rule.Setters.end()
				&& (!setter->UsesResource || setter->UsesDynamicResource
					|| DesignerBindingUtils::Trim(setter->ResourceKey).empty()))
			{
				if (outError) *outError = L"Style.Template 必须使用 StaticResource："
					+ owner.Name;
				return false;
			}
			if (setter != rule.Setters.end())
				key = DesignerBindingUtils::Trim(setter->ResourceKey);
		}
		return true;
	}

	struct EffectiveControlTemplate
	{
		const DesignerModel::DesignControlTemplate* Definition = nullptr;
		std::wstring ResourceKey;
		bool FromTheme = false;
		bool FromExplicitStyle = false;
	};

	static bool ResolveEffectiveControlTemplate(
		const DesignerModel::DesignDocument& document,
		const std::vector<DesignerModel::DesignNode>& nodes,
		const DesignerModel::DesignDocument* theme,
		const DesignerModel::DesignNode& owner,
		EffectiveControlTemplate& result,
		std::wstring* outError)
	{
		result = {};
		bool documentStyleMatched = false;
		bool explicitStyleMatched = false;
		if (!owner.Structure.ControlTemplate.empty())
		{
			result.ResourceKey = owner.Structure.ControlTemplate;
			// StaticResource inside a framework Theme template resolves against
			// the dictionary that defines that template, not the consuming
			// document's resources.
			result.FromTheme = owner.TemplateState.ResourceScopeFromTheme;
		}
		else
		{
			const bool explicitStyle =
				!owner.Properties.StyleResourceKey.empty();
			// Theme-generated nodes must not see a consuming document's same-key
			// Style. Their explicit StaticResource starts in the defining Theme.
			if (!(explicitStyle
				&& owner.TemplateState.StyleResourceScopeFromTheme))
			{
				if (!ResolveStyledControlTemplateKeyFromSheet(
					VisibleStyleSheet(document, nodes, owner), owner,
					result.ResourceKey, outError,
					&documentStyleMatched)) return false;
				explicitStyleMatched =
					explicitStyle && documentStyleMatched;
				result.FromExplicitStyle = explicitStyleMatched
					&& !result.ResourceKey.empty();
			}
			if (explicitStyle && !documentStyleMatched
				&& result.ResourceKey.empty())
			{
				bool themeStyleMatched = false;
				if (theme && !ResolveStyledControlTemplateKeyFromSheet(
					theme->StyleSheet, owner, result.ResourceKey,
					outError, &themeStyleMatched)) return false;
				if (!theme || !themeStyleMatched)
				{
					if (outError) *outError = L"Style StaticResource 不存在："
						+ owner.Properties.StyleResourceKey
						+ L" -> " + owner.Name;
					return false;
				}
				explicitStyleMatched = true;
				result.FromTheme = true;
				result.FromExplicitStyle =
					!result.ResourceKey.empty();
			}
		}

		if (!result.ResourceKey.empty())
		{
			if (!result.FromTheme)
				result.Definition = document.FindControlTemplate(
					nodes, owner, result.ResourceKey);
			if (!result.Definition && theme)
			{
				result.Definition = theme->FindControlTemplate(
					result.ResourceKey);
				result.FromTheme = result.Definition != nullptr;
			}
			if (!result.Definition)
			{
				if (outError) *outError = L"控件 " + owner.Name
					+ L" 引用了不存在的 ControlTemplate："
					+ result.ResourceKey;
				return false;
			}
		}
		else
		{
			// Author resources are above the framework Theme.  In particular, a
			// keyless ControlTemplate is the author's implicit Template value and
			// must be selected before the Theme Style's Template setter.  The old
			// order selected Generic.xaml first, so compilation expanded one tree
			// while the document Style pass later attempted to install another.
			if (!explicitStyleMatched)
				result.Definition = owner.ComponentType.Empty()
					? document.FindImplicitControlTemplate(
						nodes, owner, owner.Type)
					: document.FindImplicitControlTemplate(
						nodes, owner, owner.ComponentType);
			if (!result.Definition && theme)
			{
				auto themeProbe = owner;
				// An explicit Style that has no Template setter still receives the
				// lower-precedence implicit Theme Style's framework template.
				themeProbe.Properties.StyleResourceKey.clear();
				if (!ResolveStyledControlTemplateKeyFromSheet(
					theme->StyleSheet, themeProbe,
					result.ResourceKey, outError)) return false;
				if (!result.ResourceKey.empty())
				{
					result.Definition = theme->FindControlTemplate(
						result.ResourceKey);
					if (!result.Definition)
					{
						if (outError) *outError = L"控件 " + owner.Name
							+ L" 引用了不存在的主题 ControlTemplate："
							+ result.ResourceKey;
						return false;
					}
				}
				else result.Definition = owner.ComponentType.Empty()
					? theme->FindImplicitControlTemplate(owner.Type)
					: theme->FindImplicitControlTemplate(owner.ComponentType);
				result.FromTheme = result.Definition != nullptr;
			}
		}

		if (result.Definition
			&& !IsControlTemplateTargetCompatible(owner, *result.Definition))
		{
			if (outError) *outError = L"ControlTemplate TargetType 与控件类型不兼容："
				+ result.Definition->DisplayName() + L" -> " + owner.Name;
			return false;
		}
		return true;
	}

	static std::wstring ControlTemplateIdentity(
		const DesignerModel::DesignControlTemplate& definition)
	{
		if (!definition.IsImplicit()) return L"key:" + definition.Key;
		return definition.TargetComponentType.Empty()
			? L"type:" + DesignerStyleSheetUtils::UIClassName(
				definition.TargetType)
			: L"component:" + definition.TargetComponentType.RegistryKey();
	}

	static bool RemapCommandTargetName(
		std::wstring& target,
		const std::unordered_map<std::wstring, std::wstring>& nameMap,
		const std::wstring& scopeLabel,
		std::wstring* outError)
	{
		if (target.empty()) return true;
		const auto found = nameMap.find(target);
		if (found == nameMap.end())
		{
			if (outError) *outError = scopeLabel
				+ L" CommandTarget 引用了作用域外控件：" + target;
			return false;
		}
		target = found->second;
		return true;
	}

	static bool RemapNodeCommandTargets(
		DesignerModel::DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& nameMap,
		const std::wstring& scopeLabel,
		std::wstring* outError)
	{
		for (auto& binding : node.InputBindings)
			if (!RemapCommandTargetName(
				binding.CommandTarget, nameMap, scopeLabel, outError)) return false;
		if (!RemapCommandTargetName(
			node.Structure.CommandTarget, nameMap, scopeLabel, outError))
			return false;
		return true;
	}

	static bool ExpandComponentTemplates(
		const DesignerModel::DesignDocument& source,
		const DesignerModel::DesignDocument* theme,
		DesignerModel::DesignDocument& output,
		std::wstring* outError)
	{
		output = source;
		std::unordered_set<std::wstring> names;
		for (const auto& node : output.Nodes) names.insert(node.Name);

		std::function<bool(size_t, std::vector<std::wstring>)> expand;
		expand = [&](size_t instanceIndex,
			std::vector<std::wstring> ancestry) -> bool
		{
			if (instanceIndex >= output.Nodes.size()) return false;
			if (output.Nodes[instanceIndex].TemplateState.ComponentExpanded)
				return true;
			const auto instanceName = output.Nodes[instanceIndex].Name;
			const auto instanceId = output.Nodes[instanceIndex].Id;
			const auto instanceType = output.Nodes[instanceIndex].ComponentType;
			if (instanceType.Empty()) return true;
			const auto* componentDefinition = output.FindComponent(
				output.Nodes, output.Nodes[instanceIndex], instanceType);
			if (!componentDefinition)
			{
				if (outError) *outError = L"组件实例引用了不存在的定义："
					+ instanceType.XamlName;
				return false;
			}
			const auto component = std::make_shared<
				DesignerModel::DesignComponentDefinition>(*componentDefinition);
			EffectiveControlTemplate effectiveTemplate;
			if (!ResolveEffectiveControlTemplate(
				output, output.Nodes, theme, output.Nodes[instanceIndex],
				effectiveTemplate, outError)) return false;
			std::shared_ptr<DesignerModel::DesignControlTemplate> controlTemplate;
			if (effectiveTemplate.Definition)
				controlTemplate = std::make_shared<
					DesignerModel::DesignControlTemplate>(
						*effectiveTemplate.Definition);
			const auto key = instanceType.RegistryKey();
			if (std::find(ancestry.begin(), ancestry.end(), key) != ancestry.end())
			{
				if (outError) *outError = L"组件模板存在递归引用："
					+ instanceType.XamlName;
				return false;
			}
			ancestry.push_back(key);
			std::wstring controlTemplateChain;
			if (controlTemplate)
			{
				const auto identity = ControlTemplateIdentity(*controlTemplate);
				const auto inheritedChain = output.Nodes[instanceIndex]
					.TemplateState.ControlTemplateChain;
				const auto marker = L"|" + identity + L"|";
				if (inheritedChain.find(marker) != std::wstring::npos)
				{
					if (outError) *outError = L"ControlTemplate 存在递归引用："
						+ controlTemplate->DisplayName();
					return false;
				}
				controlTemplateChain = inheritedChain + marker;
			}

			std::vector<size_t> contentChildren;
			for (size_t candidateIndex = 0;
				candidateIndex < output.Nodes.size(); ++candidateIndex)
			{
				const auto& candidate = output.Nodes[candidateIndex];
				if (candidate.ParentId == instanceId
					|| candidate.ParentRef == instanceName)
				{
					if (candidate.ComponentContentProperty.empty())
					{
						if (outError) *outError = L"组件视觉子节点缺少内容属性："
							+ candidate.Name;
						return false;
					}
					const auto contract = std::find_if(
						component->ContentProperties.begin(),
						component->ContentProperties.end(), [&](const auto& content)
						{
							return content.Name
								== candidate.ComponentContentProperty;
						});
					if (contract == component->ContentProperties.end())
					{
						if (outError) *outError = L"组件视觉子节点引用了未知内容属性："
							+ candidate.ComponentContentProperty;
						return false;
					}
					contentChildren.push_back(candidateIndex);
				}
			}
			for (const auto& contract : component->ContentProperties)
			{
				const auto count = std::count_if(
					contentChildren.begin(), contentChildren.end(), [&](size_t index)
					{
						return output.Nodes[index].ComponentContentProperty
							== contract.Name;
					});
				if (contract.Cardinality ==
					DesignerComponentContentCardinality::Single && count > 1)
				{
					if (outError) *outError = L"组件单值内容属性包含多个视觉根："
						+ contract.Name;
					return false;
				}
			}
			output.Nodes[instanceIndex].TemplateState.ComponentExpanded = true;
			const auto& visualTemplate = controlTemplate
				? controlTemplate->Template : component->Template;
			if (controlTemplate)
			{
				if (visualTemplate.empty())
				{
					if (outError) *outError = L"ControlTemplate 没有视觉根："
						+ controlTemplate->DisplayName();
					return false;
				}
				output.Nodes[instanceIndex].TemplateState.ControlTemplateExpanded = true;
				output.Nodes[instanceIndex].TemplateState.AppliedControlTemplate =
					ControlTemplateIdentity(*controlTemplate);
				output.Nodes[instanceIndex].TemplateState.
					AppliedControlTemplateResource = effectiveTemplate.ResourceKey;
				output.Nodes[instanceIndex].TemplateState.
					AppliedControlTemplateFromTheme = effectiveTemplate.FromTheme;
				output.Nodes[instanceIndex].TemplateState.
					AppliedControlTemplateFromStyle =
						effectiveTemplate.FromExplicitStyle;
			}
			if (visualTemplate.empty()) return true;

			std::unordered_map<int, int> idMap;
			std::unordered_map<std::wstring, std::wstring> nameMap;
			for (const auto& local : visualTemplate)
			{
				const int id = output.AllocateNodeId();
				auto name = instanceName + L"@" + local.Name;
				if (!names.insert(name).second)
					name += L"_" + std::to_wstring(id);
				idMap.emplace(local.Id, id);
				nameMap.emplace(local.Name, std::move(name));
			}

			std::vector<size_t> nestedComponents;
			std::unordered_map<std::wstring, std::pair<int, std::wstring>> presenters;
			for (const auto& local : visualTemplate)
			{
				DesignerModel::DesignNode generated = local;
				generated.Id = idMap.at(local.Id);
				generated.Name = nameMap.at(local.Name);
				for (auto& [targetProperty, binding] : generated.Bindings)
				{
					(void)targetProperty;
					std::wstring bindingError;
					if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
						binding, [&](DesignerDataBinding& child)
					{
							if (child.ElementName.empty()) return true;
							const auto authoredName = child.ElementName;
							const auto sourceName = nameMap.find(authoredName);
							if (sourceName == nameMap.end())
							{
								bindingError = L"组件模板 ElementName 引用了作用域外控件："
									+ authoredName;
								return false;
							}
							child.ElementName = sourceName->second;
							return true;
						}))
					{
						if (outError) *outError = bindingError;
						return false;
					}
				}
				if (!RemapNodeCommandTargets(
					generated, nameMap, L"组件模板", outError)) return false;
				if (local.ParentId > 0)
				{
					const auto parent = idMap.find(local.ParentId);
					if (parent == idMap.end())
					{
						if (outError) *outError = L"组件模板包含无效父节点 ID。";
						return false;
					}
					generated.ParentId = parent->second;
					const auto parentName = nameMap.find(local.ParentRef);
					generated.ParentRef = parentName == nameMap.end()
						? std::wstring{} : parentName->second;
				}
				else if (!local.ParentRef.empty())
				{
					const auto parent = nameMap.find(local.ParentRef);
					if (parent == nameMap.end())
					{
						if (outError) *outError = L"组件模板包含无效父节点名称。";
						return false;
					}
					generated.ParentId = 0;
					generated.ParentRef = parent->second;
				}
				else
				{
					generated.ParentId = instanceId;
					generated.ParentRef = instanceName;
					// ComponentDefinition.Template is the component's authored
					// presentation root even when no separate ControlTemplate
					// overrides it.  Attach it through the same framework-owned
					// template-root path so the base control never treats template
					// chrome as ordinary Panel/Content children.
					generated.TemplateState.ControlTemplateRoot = true;
				}
				generated.TemplateState.Generated = true;
				generated.TemplateState.Owner = instanceName;
				generated.TemplateState.PartName = local.Name;
				if (controlTemplate)
				{
					generated.TemplateState.ControlTemplateChain =
						controlTemplateChain;
					generated.TemplateState.ResourceScopeFromTheme =
						effectiveTemplate.FromTheme;
					generated.TemplateState.StyleResourceScopeFromTheme =
						effectiveTemplate.FromTheme;
				}
				output.Nodes.push_back(std::move(generated));
				if (!local.PresentedComponentContent.empty())
				{
					const auto contract = std::find_if(
						component->ContentProperties.begin(),
						component->ContentProperties.end(), [&](const auto& content)
						{
							return content.Name
								== local.PresentedComponentContent;
						});
					if (contract == component->ContentProperties.end()
						|| !local.ComponentType.Empty()
						|| !IsComponentContentPresenterType(local.Type))
					{
						if (outError) *outError = L"组件模板包含无效的内容 Presenter："
							+ local.PresentedComponentContent;
						return false;
					}
					const auto duplicate = std::find_if(
						presenters.begin(), presenters.end(), [&](const auto& entry)
						{
							return entry.first == contract->Name;
						});
					if (duplicate != presenters.end())
					{
						if (outError) *outError = L"组件内容属性拥有重复 Presenter："
							+ contract->Name;
						return false;
					}
					const auto inserted = presenters.emplace(
						contract->Name,
						std::pair{ output.Nodes.back().Id, output.Nodes.back().Name });
					if (!inserted.second)
					{
						if (outError) *outError = L"组件内容属性拥有重复 Presenter："
							+ local.PresentedComponentContent;
						return false;
					}
				}
				if (!output.Nodes.back().ComponentType.Empty())
					nestedComponents.push_back(output.Nodes.size() - 1);
			}
			for (const auto contentIndex : contentChildren)
			{
				auto& child = output.Nodes[contentIndex];
				const auto presenter = std::find_if(
					presenters.begin(), presenters.end(), [&](const auto& entry)
					{
						return entry.first
							== child.ComponentContentProperty;
					});
				if (presenter == presenters.end())
				{
					if (outError) *outError = L"组件内容属性缺少模板 Presenter："
						+ child.ComponentContentProperty;
					return false;
				}
				child.ParentId = presenter->second.first;
				child.ParentRef = presenter->second.second;
				child.TemplateState.ContentOwner = instanceName;
			}

			for (const auto nested : nestedComponents)
				if (!expand(nested, ancestry)) return false;
			return true;
		};

		const size_t publicNodeCount = output.Nodes.size();
		for (size_t index = 0; index < publicNodeCount; ++index)
			if (!expand(index, {})) return false;
		return true;
	}

	static bool ExpandControlTemplates(
		const DesignerModel::DesignDocument& source,
		const DesignerModel::DesignDocument* theme,
		DesignerModel::DesignDocument& output,
		std::wstring* outError)
	{
		output = source;
		std::unordered_set<std::wstring> names;
		for (const auto& node : output.Nodes) names.insert(node.Name);

		const size_t sourceNodeCount = output.Nodes.size();
		for (size_t ownerIndex = 0; ownerIndex < sourceNodeCount; ++ownerIndex)
		{
			if (ownerIndex >= output.Nodes.size()) return false;
			auto& owner = output.Nodes[ownerIndex];
			if (!owner.ComponentType.Empty()
				|| !IsControlTemplateHostType(owner.Type)) continue;
			if (owner.TemplateState.ControlTemplateExpanded) continue;

			EffectiveControlTemplate effectiveTemplate;
			if (!ResolveEffectiveControlTemplate(
				output, output.Nodes, theme, owner,
				effectiveTemplate, outError))
				return false;

			owner.TemplateState.ControlTemplateExpanded = true;
			if (!effectiveTemplate.Definition) continue;
			const auto controlTemplate = std::make_shared<
				DesignerModel::DesignControlTemplate>(
					*effectiveTemplate.Definition);
			if (controlTemplate->Template.empty())
			{
				if (outError) *outError = L"ControlTemplate 没有视觉根："
					+ controlTemplate->DisplayName();
				return false;
			}

			const auto identity = ControlTemplateIdentity(*controlTemplate);
			const auto inheritedChain = owner.TemplateState.ControlTemplateChain;
			const auto marker = L"|" + identity + L"|";
			if (inheritedChain.find(marker) != std::wstring::npos)
			{
				if (outError) *outError = L"ControlTemplate 存在递归引用："
					+ controlTemplate->DisplayName();
				return false;
			}
			const auto chain = inheritedChain + marker;
			owner.TemplateState.AppliedControlTemplate = identity;
			owner.TemplateState.AppliedControlTemplateResource =
				effectiveTemplate.ResourceKey;
			owner.TemplateState.AppliedControlTemplateFromTheme =
				effectiveTemplate.FromTheme;
			owner.TemplateState.AppliedControlTemplateFromStyle =
				effectiveTemplate.FromExplicitStyle;

			const auto ownerName = owner.Name;
			const auto ownerId = owner.Id;
			const auto ownerType = owner.Type;
			std::unordered_map<int, int> idMap;
			std::unordered_map<std::wstring, std::wstring> nameMap;
			std::map<std::wstring, std::pair<int, std::wstring>> contentPresenters;
			size_t itemsPresenterCount = 0;
			for (const auto& local : controlTemplate->Template)
			{
				const int id = output.AllocateNodeId();
				auto name = ownerName + L"@template@" + local.Name;
				if (!names.insert(name).second)
					name += L"_" + std::to_wstring(id);
				idMap.emplace(local.Id, id);
				nameMap.emplace(local.Name, std::move(name));
				if (local.Type == UIClass::UI_ItemsPresenter)
				{
					const bool itemsHost = IsUIClassAssignableFrom(
						UIClass::UI_ItemsControl, ownerType);
					const bool hasAuthoredChild = std::any_of(
						controlTemplate->Template.begin(),
						controlTemplate->Template.end(), [&](const auto& candidate)
						{
							return candidate.ParentId == local.Id
								|| (!local.Name.empty()
									&& candidate.ParentRef == local.Name);
						});
					if (!itemsHost || !local.ComponentType.Empty()
						|| ++itemsPresenterCount > 1 || hasAuthoredChild)
					{
						if (outError) *outError = L"ControlTemplate 包含无效、重复或"
							L"带手工子节点的 ItemsPresenter："
							+ controlTemplate->DisplayName();
						return false;
					}
				}
				if (!local.TemplateContentSource.empty())
				{
					if (local.Type != UIClass::UI_ContentPresenter
						|| !local.ComponentType.Empty()
						|| (local.TemplateContentSource != L"Content"
							&& local.TemplateContentSource != L"Header")
						|| (local.TemplateContentSource == L"Header"
							&& !IsHeaderedContentControlType(owner.Type))
						|| !contentPresenters.emplace(
							local.TemplateContentSource,
							std::pair{ id, nameMap.at(local.Name) }).second)
					{
						if (outError) *outError = L"ControlTemplate 包含无效或重复的 ContentSource："
							+ local.TemplateContentSource;
						return false;
					}
				}
			}

			for (const auto& local : controlTemplate->Template)
			{
				DesignerModel::DesignNode generated = local;
				generated.Id = idMap.at(local.Id);
				generated.Name = nameMap.at(local.Name);
				for (auto& [targetProperty, binding] : generated.Bindings)
				{
					(void)targetProperty;
					std::wstring bindingError;
					if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
						binding, [&](DesignerDataBinding& child)
					{
							if (child.ElementName.empty()) return true;
							const auto authoredName = child.ElementName;
							const auto sourceName = nameMap.find(authoredName);
							if (sourceName == nameMap.end())
							{
								bindingError = L"ControlTemplate ElementName 引用了作用域外控件："
									+ authoredName;
								return false;
							}
							child.ElementName = sourceName->second;
							return true;
						}))
					{
						if (outError) *outError = bindingError;
						return false;
					}
				}
				if (!RemapNodeCommandTargets(
					generated, nameMap, L"ControlTemplate", outError)) return false;
				if (local.ParentId > 0)
				{
					const auto parent = idMap.find(local.ParentId);
					if (parent == idMap.end())
					{
						if (outError) *outError =
							L"ControlTemplate 包含无效父节点 ID。";
						return false;
					}
					generated.ParentId = parent->second;
					const auto parentName = nameMap.find(local.ParentRef);
					generated.ParentRef = parentName == nameMap.end()
						? std::wstring{} : parentName->second;
				}
				else if (!local.ParentRef.empty())
				{
					const auto parent = nameMap.find(local.ParentRef);
					if (parent == nameMap.end())
					{
						if (outError) *outError =
							L"ControlTemplate 包含无效父节点名称。";
						return false;
					}
					generated.ParentId = 0;
					generated.ParentRef = parent->second;
				}
				else
				{
					generated.ParentId = ownerId;
					generated.ParentRef = ownerName;
				}
				generated.TemplateState.Generated = true;
				generated.TemplateState.Owner = ownerName;
				generated.TemplateState.PartName = local.Name;
				generated.TemplateState.ControlTemplateChain = chain;
				generated.TemplateState.ResourceScopeFromTheme =
					effectiveTemplate.FromTheme;
				generated.TemplateState.StyleResourceScopeFromTheme =
					effectiveTemplate.FromTheme;
				if (!local.TemplateContentSource.empty())
				{
					const auto addAlias = [&](const wchar_t* target,
						const wchar_t* sourceProperty) -> bool
					{
						return generated.TemplateBindings.emplace(
							target, sourceProperty).second;
					};
					const bool aliasesAdded = local.TemplateContentSource == L"Header"
						? addAlias(L"Content", L"Header")
							&& addAlias(L"ContentTemplate", L"HeaderTemplate")
							&& addAlias(L"DisplayMemberPath", L"HeaderDisplayMemberPath")
						: addAlias(L"Content", L"Content")
							&& addAlias(L"ContentTemplate", L"ContentTemplate")
							&& addAlias(L"DisplayMemberPath", L"DisplayMemberPath");
					if (!aliasesAdded)
					{
						if (outError) *outError = L"ContentSource 与显式 TemplateBinding 冲突："
							+ local.TemplateContentSource;
						return false;
					}
				}
				if (local.ParentId <= 0 && local.ParentRef.empty())
					generated.TemplateState.ControlTemplateRoot = true;
				output.Nodes.push_back(std::move(generated));
			}

			// Appending generated template nodes can reallocate output.Nodes.
			// Do not read the earlier owner reference after the first push_back.
			if (IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, ownerType)) continue;

			for (size_t childIndex = 0; childIndex < sourceNodeCount; ++childIndex)
			{
				if (childIndex == ownerIndex) continue;
				auto& child = output.Nodes[childIndex];
				if (child.ParentId != ownerId && child.ParentRef != ownerName) continue;
				const bool isHeader = child.Structure.ChildRole
					== DesignNodeChildRole::Header;
				const auto sourceName = isHeader
					? std::wstring(L"Header") : std::wstring(L"Content");
				const auto presenter = contentPresenters.find(sourceName);
				if (presenter == contentPresenters.end())
				{
					// WPF TabItem templates present only the Header. The owning
					// TabControl projects the selected page into its single central
					// content host, so authored page visuals must remain attached to
					// the TabItem logical Content slot instead of requiring a second
					// per-item ContentPresenter.
					if (!isHeader && IsUIClassAssignableFrom(
						UIClass::UI_TabItem, ownerType))
						continue;
					if (outError) *outError = L"ControlTemplate 必须用 ContentPresenter ContentSource=\""
						+ sourceName + L"\" 承载视觉内容：" + ownerName;
					return false;
				}
				child.ParentId = presenter->second.first;
				child.ParentRef = presenter->second.second;
				child.TemplateState.ContentOwner = ownerName;
			}
		}
		return true;
	}

	static std::optional<BindingValueKind> ComponentBindingKind(
		DesignerStyleValueKind kind)
	{
		switch (kind)
		{
		case DesignerStyleValueKind::Bool: return BindingValueKind::Bool;
		case DesignerStyleValueKind::NullableBool:
			return BindingValueKind::NullableBool;
		case DesignerStyleValueKind::Int: return BindingValueKind::Int;
		case DesignerStyleValueKind::Int64: return BindingValueKind::Int64;
		case DesignerStyleValueKind::Float: return BindingValueKind::Float;
		case DesignerStyleValueKind::Double: return BindingValueKind::Double;
		case DesignerStyleValueKind::String: return BindingValueKind::String;
		case DesignerStyleValueKind::Color:
		case DesignerStyleValueKind::Thickness:
		case DesignerStyleValueKind::Point:
		case DesignerStyleValueKind::Vector:
		case DesignerStyleValueKind::Rect:
		case DesignerStyleValueKind::Size:
		case DesignerStyleValueKind::Matrix:
		case DesignerStyleValueKind::Length:
		case DesignerStyleValueKind::Brush:
		case DesignerStyleValueKind::Geometry:
		case DesignerStyleValueKind::Transform:
			return BindingValueKind::Object;
		default: return std::nullopt;
		}
	}

	static bool BuildComponentTypeDescriptorCore(
		const DesignerModel::DesignComponentDefinition& component,
		const DesignerModel::DesignDocument& document,
		std::shared_ptr<const DeclarativeTypeDescriptor>& descriptor,
		std::wstring* outError)
	{
		DesignerStyleSheet lexicalResources;
		const DesignerStyleSheet* visibleResources = &document.StyleSheet;
		for (const auto& owner : document.Nodes)
		{
			const auto declaration = std::find_if(
				owner.LocalObjectResources.Components.begin(),
				owner.LocalObjectResources.Components.end(),
				[&](const auto& candidate) { return &candidate == &component; });
			if (declaration == owner.LocalObjectResources.Components.end()) continue;
			lexicalResources = VisibleStyleSheet(document, document.Nodes, owner);
			visibleResources = &lexicalResources;
			break;
		}
		std::vector<DeclarativeContentPropertyDefinition> contentProperties;
		contentProperties.reserve(component.ContentProperties.size());
		for (const auto& content : component.ContentProperties)
		{
			DeclarativeContentPropertyDefinition definition;
			definition.Name = content.Name;
			definition.DisplayName = content.DisplayName;
			definition.Cardinality = content.Cardinality
				== DesignerComponentContentCardinality::Multiple
				? DeclarativeContentCardinality::Multiple
				: DeclarativeContentCardinality::Single;
			definition.IsDefault = content.IsDefault;
			contentProperties.push_back(std::move(definition));
		}

		std::vector<DeclarativePropertyDefinition> properties;
		properties.reserve(component.Properties.size());
		for (const auto& property : component.Properties)
		{
			const auto kind = ComponentBindingKind(property.DefaultValue.Kind);
			if (!kind)
			{
				if (outError) *outError = L"组件属性 " + property.Name
					+ L" 使用了尚未进入声明属性契约的类型。";
				return false;
			}
			const DesignerStyleValue* source = &property.DefaultValue;
			if (!property.DefaultResourceKey.empty())
			{
				const auto resource = std::find_if(
					visibleResources->Resources.begin(),
					visibleResources->Resources.end(),
					[&](const auto& item)
					{
						return item.Key == property.DefaultResourceKey;
					});
				if (resource == visibleResources->Resources.end())
			{
				if (outError) *outError = L"组件属性引用了不存在的默认资源："
					+ property.DefaultResourceKey;
				return false;
			}
			source = &resource->Value;
				if (source->Kind != property.DefaultValue.Kind)
				{
					if (outError) *outError =
						L"组件属性默认资源类型与声明 Type 不一致："
						+ property.Name;
					return false;
				}
			}
			BindingValue defaultValue;
			std::wstring conversionError;
			if (!DesignerStyleSheetUtils::TryConvertValue(
				*source, defaultValue, &conversionError,
				document.ResourceBasePath, document.Resources))
			{
				if (outError) *outError = L"组件属性 " + property.Name
					+ L" 的默认值无效：" + conversionError;
				return false;
			}
			DeclarativePropertyDefinition definition;
			definition.Name = property.Name;
			definition.ValueKind = *kind;
			definition.DefaultValue = std::move(defaultValue);
			definition.Flags = property.Flags;
			definition.DefaultUpdateMode = property.DefaultUpdateMode;
			definition.IsReadOnly = property.IsReadOnly;
			if (HasDependencyPropertyFlag(
				property.Flags, DependencyPropertyFlags::Inherits))
				definition.InheritanceKey = component.Type.RegistryKey()
					+ L"|" + property.Name;
			definition.Design.DisplayName = property.DisplayName.empty()
				? property.Name : property.DisplayName;
			definition.Design.Category = property.Category;
			definition.Design.CategoryOrder = property.CategoryOrder;
			definition.Design.Order = property.Order;
			definition.Design.Editor = property.Editor;
			for (const auto& choice : property.Choices)
			{
				BindingValue value(std::wstring(choice.Value));
				definition.AllowedValues.push_back(value);
				definition.Design.Choices.push_back({
					choice.DisplayName.empty() ? choice.Value : choice.DisplayName,
					std::move(value)
				});
			}
			definition.Design.Minimum = property.Minimum;
			definition.Design.Maximum = property.Maximum;
			definition.Design.Step = property.Step;
			definition.Design.Persistence = DependencyPropertyPersistence::Metadata;
			properties.push_back(std::move(definition));
		}

		std::vector<DeclarativeEventDefinition> events;
		events.reserve(component.Events.size());
		for (const auto& event : component.Events)
		{
			BindingValueKind payloadKind = BindingValueKind::Empty;
			switch (event.Payload)
			{
			case DesignerComponentEventPayload::None:
				payloadKind = BindingValueKind::Empty; break;
			case DesignerComponentEventPayload::Bool:
				payloadKind = BindingValueKind::Bool; break;
			case DesignerComponentEventPayload::Int:
				payloadKind = BindingValueKind::Int; break;
			case DesignerComponentEventPayload::Int64:
				payloadKind = BindingValueKind::Int64; break;
			case DesignerComponentEventPayload::Float:
				payloadKind = BindingValueKind::Float; break;
			case DesignerComponentEventPayload::Double:
				payloadKind = BindingValueKind::Double; break;
			case DesignerComponentEventPayload::String:
				payloadKind = BindingValueKind::String; break;
			default:
				if (outError) *outError =
					L"组件事件 payload 无效：" + event.Name;
				return false;
			}
			DeclarativeEventDefinition definition;
			definition.Name = event.Name;
			definition.PayloadKind = payloadKind;
			definition.RoutingStrategy = event.RoutingStrategy;
			events.push_back(std::move(definition));
		}

		std::wstring schemaError;
		descriptor = DeclarativeTypeDescriptor::Create(
			{ component.Type.XamlNamespace, component.Type.XamlName },
			std::move(properties), std::move(events),
			std::move(contentProperties), &schemaError);
		if (!descriptor)
		{
			if (outError) *outError = L"组件 Schema 无效：" + schemaError;
			return false;
		}
		if (outError) outError->clear();
		return true;
	}

	static bool MaterializeDeclarativeInteractionsCore(
		Control* owner,
		const std::vector<DesignerVisualStateGroup>& sourceGroups,
		const std::vector<DesignerEventTrigger>& sourceEventTriggers,
		const DesignerModel::DesignDocument& document,
		std::vector<DeclarativeVisualStateGroupDefinition>& groups,
		std::vector<DeclarativeEventTriggerDefinition>& eventTriggers,
		std::wstring* outError)
	{
		// Dynamic XAML retains authored names until Control installs the final
		// component/template descriptor.  Generated Production interactions take
		// the separate compiled identity path and never enter this materializer.
		(void)owner;
		groups.clear();
		eventTriggers.clear();
		if (sourceGroups.empty() && sourceEventTriggers.empty())
		{
			if (outError) outError->clear();
			return true;
		}
		groups.reserve(sourceGroups.size());
		eventTriggers.reserve(sourceEventTriggers.size());
		auto convert = [&](const DesignerStyleValue& value,
			BindingValue& output, const std::wstring& context)
		{
			std::wstring conversionError;
			if (DesignerStyleSheetUtils::TryConvertValue(
				value, output, &conversionError,
				document.ResourceBasePath, document.Resources)) return true;
			if (outError) *outError = context + L"：" + conversionError;
			return false;
		};
		auto assignAnimationProperty = [](
			const std::wstring& propertyName,
			DeclarativeVisualStateAnimation& animation)
		{
			if (ClassifyStoryboardObjectPath(propertyName)
				!= StoryboardObjectPathKind::None)
			{
				animation.Property = {};
				animation.ObjectPath = propertyName;
			}
			else
			{
				animation.Property = DependencyPropertyReference(propertyName);
				animation.ObjectPath.clear();
			}
		};
		auto materializeAnimation = [&](const DesignerVisualStateAnimation& source,
			const std::wstring& context, DeclarativeVisualStateAnimation& animation)
		{
			animation.Kind = source.Kind == DesignerAnimationKind::Color
				? DeclarativeAnimationKind::Color
				: source.Kind == DesignerAnimationKind::Thickness
					? DeclarativeAnimationKind::Thickness
				: source.Kind == DesignerAnimationKind::Point
					? DeclarativeAnimationKind::Point
				: source.Kind == DesignerAnimationKind::Vector
					? DeclarativeAnimationKind::Vector
				: source.Kind == DesignerAnimationKind::Rect
					? DeclarativeAnimationKind::Rect
				: source.Kind == DesignerAnimationKind::Size
					? DeclarativeAnimationKind::Size
				: source.Kind == DesignerAnimationKind::Matrix
					? DeclarativeAnimationKind::Matrix
				: source.Kind == DesignerAnimationKind::Object
					? DeclarativeAnimationKind::Object
					: DeclarativeAnimationKind::Double;
			animation.TargetName = source.TargetName;
			assignAnimationProperty(source.PropertyName, animation);
			auto resolveValue = [&](const DesignerStyleValue& literal,
				bool usesResource, const std::wstring& resourceKey,
				BindingValue& output, const std::wstring& label)
			{
				const DesignerStyleValue* value = &literal;
				if (usesResource)
				{
					const auto resource = std::find_if(
						document.StyleSheet.Resources.begin(),
						document.StyleSheet.Resources.end(),
						[&](const auto& candidate)
						{ return candidate.Key == resourceKey; });
					if (resource == document.StyleSheet.Resources.end())
						value = &literal;
					else value = &resource->Value;
				}
				return convert(*value, output,
					context + L"." + source.PropertyName + L" " + label);
			};
			if (source.HasFrom)
			{
				BindingValue from;
				if (!resolveValue(source.From, source.FromUsesResource,
					source.FromResourceKey, from, L"From")) return false;
				animation.From = std::move(from);
			}
			if (source.KeyFrames.empty())
			{
				if (source.HasTo)
				{
					BindingValue to;
					if (!resolveValue(source.To, source.ToUsesResource,
						source.ToResourceKey, to, L"To")) return false;
					animation.To = std::move(to);
				}
				if (source.HasBy)
				{
					BindingValue by;
					if (!resolveValue(source.By, source.ByUsesResource,
						source.ByResourceKey, by, L"By")) return false;
					animation.By = std::move(by);
				}
			}
			else
			{
				animation.KeyFrames.reserve(source.KeyFrames.size());
				for (const auto& sourceKeyFrame : source.KeyFrames)
				{
					DeclarativeAnimationKeyFrame keyFrame;
					switch (sourceKeyFrame.Kind)
					{
					case DesignerKeyFrameKind::Discrete:
						keyFrame.Kind = DeclarativeKeyFrameKind::Discrete; break;
					case DesignerKeyFrameKind::Easing:
						keyFrame.Kind = DeclarativeKeyFrameKind::Easing; break;
					case DesignerKeyFrameKind::Spline:
						keyFrame.Kind = DeclarativeKeyFrameKind::Spline; break;
					case DesignerKeyFrameKind::Linear:
					default:
						keyFrame.Kind = DeclarativeKeyFrameKind::Linear; break;
					}
					keyFrame.KeyTimeMilliseconds = sourceKeyFrame.KeyTimeMilliseconds;
					if (!resolveValue(sourceKeyFrame.Value,
						sourceKeyFrame.UsesResource, sourceKeyFrame.ResourceKey,
						keyFrame.Value, L"KeyFrame")) return false;
					switch (sourceKeyFrame.Easing)
					{
					case DesignerEasingKind::Quadratic:
						keyFrame.Easing = DeclarativeEasingKind::Quadratic; break;
					case DesignerEasingKind::Cubic:
						keyFrame.Easing = DeclarativeEasingKind::Cubic; break;
					case DesignerEasingKind::Sine:
						keyFrame.Easing = DeclarativeEasingKind::Sine; break;
					case DesignerEasingKind::Linear:
					default:
						keyFrame.Easing = DeclarativeEasingKind::Linear; break;
					}
					keyFrame.EasingMode = sourceKeyFrame.EasingMode
						== DesignerEasingMode::EaseIn
						? DeclarativeEasingMode::EaseIn
						: sourceKeyFrame.EasingMode == DesignerEasingMode::EaseInOut
							? DeclarativeEasingMode::EaseInOut
							: DeclarativeEasingMode::EaseOut;
					keyFrame.KeySplineX1 = sourceKeyFrame.KeySplineX1;
					keyFrame.KeySplineY1 = sourceKeyFrame.KeySplineY1;
					keyFrame.KeySplineX2 = sourceKeyFrame.KeySplineX2;
					keyFrame.KeySplineY2 = sourceKeyFrame.KeySplineY2;
					animation.KeyFrames.push_back(std::move(keyFrame));
				}
			}
			animation.BeginTimeMilliseconds = source.BeginTimeMilliseconds;
			animation.DurationMilliseconds = source.DurationMilliseconds;
			animation.RepeatBehavior = source.RepeatBehavior
				== DesignerRepeatBehaviorKind::Duration
				? DeclarativeRepeatBehaviorKind::Duration
				: source.RepeatBehavior == DesignerRepeatBehaviorKind::Forever
					? DeclarativeRepeatBehaviorKind::Forever
					: DeclarativeRepeatBehaviorKind::Count;
			animation.RepeatCount = source.RepeatCount;
			animation.RepeatDurationMilliseconds =
				source.RepeatDurationMilliseconds;
			animation.IsAdditive = source.IsAdditive;
			animation.IsCumulative = source.IsCumulative;
			animation.AutoReverse = source.AutoReverse;
			animation.FillBehavior = source.FillBehavior
				== DesignerTimelineFillBehavior::Stop
				? DeclarativeTimelineFillBehavior::Stop
				: DeclarativeTimelineFillBehavior::HoldEnd;
			animation.SpeedRatio = source.SpeedRatio;
			animation.AccelerationRatio = source.AccelerationRatio;
			animation.DecelerationRatio = source.DecelerationRatio;
			switch (source.Easing)
			{
			case DesignerEasingKind::Quadratic:
				animation.Easing = DeclarativeEasingKind::Quadratic; break;
			case DesignerEasingKind::Cubic:
				animation.Easing = DeclarativeEasingKind::Cubic; break;
			case DesignerEasingKind::Sine:
				animation.Easing = DeclarativeEasingKind::Sine; break;
			case DesignerEasingKind::Linear:
			default:
				animation.Easing = DeclarativeEasingKind::Linear; break;
			}
			animation.EasingMode = source.EasingMode == DesignerEasingMode::EaseIn
				? DeclarativeEasingMode::EaseIn
				: source.EasingMode == DesignerEasingMode::EaseInOut
					? DeclarativeEasingMode::EaseInOut
					: DeclarativeEasingMode::EaseOut;
			return true;
		};
		for (const auto& sourceGroup : sourceGroups)
		{
			DeclarativeVisualStateGroupDefinition group;
			group.Name = sourceGroup.Name;
			group.States.reserve(sourceGroup.States.size());
			for (const auto& sourceState : sourceGroup.States)
			{
				DeclarativeVisualStateDefinition state;
				state.Name = sourceState.Name;
				state.EventNames = sourceState.EventNames;
				for (const auto& sourceCondition : sourceState.Conditions)
				{
					DeclarativeVisualStateCondition condition;
					if (!convert(sourceCondition.Value, condition.Value,
						L"视觉状态条件 " + sourceState.Name + L"."
							+ sourceCondition.PropertyName)) return false;
					condition.Property = DependencyPropertyReference(
						sourceCondition.PropertyName);
					state.Conditions.push_back(std::move(condition));
				}
				for (const auto& sourceSetter : sourceState.Setters)
				{
					DeclarativeVisualStateSetter setter;
					setter.TargetName = sourceSetter.TargetName;
					const DesignerStyleValue* value = &sourceSetter.Literal;
					if (sourceSetter.UsesResource)
					{
						const auto resource = std::find_if(
							document.StyleSheet.Resources.begin(),
							document.StyleSheet.Resources.end(),
							[&](const auto& candidate)
							{
								return candidate.Key
									== sourceSetter.ResourceKey;
							});
						if (resource != document.StyleSheet.Resources.end())
							value = &resource->Value;
					}
					if (!convert(*value, setter.Value,
						L"视觉状态 Setter " + sourceState.Name + L"."
							+ sourceSetter.PropertyName)) return false;
					setter.Property = DependencyPropertyReference(
						sourceSetter.PropertyName);
					state.Setters.push_back(std::move(setter));
				}
				for (const auto& sourceAnimation : sourceState.Animations)
				{
					DeclarativeVisualStateAnimation animation;
					animation.Kind = sourceAnimation.Kind
						== DesignerAnimationKind::Color
						? DeclarativeAnimationKind::Color
						: sourceAnimation.Kind == DesignerAnimationKind::Thickness
							? DeclarativeAnimationKind::Thickness
						: sourceAnimation.Kind == DesignerAnimationKind::Point
							? DeclarativeAnimationKind::Point
						: sourceAnimation.Kind == DesignerAnimationKind::Vector
							? DeclarativeAnimationKind::Vector
						: sourceAnimation.Kind == DesignerAnimationKind::Rect
							? DeclarativeAnimationKind::Rect
						: sourceAnimation.Kind == DesignerAnimationKind::Size
							? DeclarativeAnimationKind::Size
						: sourceAnimation.Kind == DesignerAnimationKind::Matrix
							? DeclarativeAnimationKind::Matrix
						: sourceAnimation.Kind == DesignerAnimationKind::Object
							? DeclarativeAnimationKind::Object
							: DeclarativeAnimationKind::Double;
					animation.TargetName = sourceAnimation.TargetName;
					assignAnimationProperty(
						sourceAnimation.PropertyName, animation);
					auto resolveValue = [&](const DesignerStyleValue& literal,
						bool usesResource, const std::wstring& resourceKey,
						BindingValue& output, const std::wstring& label)
					{
						const DesignerStyleValue* value = &literal;
						if (usesResource)
						{
							const auto resource = std::find_if(
								document.StyleSheet.Resources.begin(),
								document.StyleSheet.Resources.end(),
								[&](const auto& candidate)
								{ return candidate.Key == resourceKey; });
							if (resource == document.StyleSheet.Resources.end())
							{
								if (outError) *outError = L"视觉状态动画引用了不存在的资源："
									+ resourceKey;
								return false;
							}
							value = &resource->Value;
						}
						return convert(*value, output,
							L"视觉状态动画 " + sourceState.Name + L"."
								+ sourceAnimation.PropertyName + L" " + label);
					};
					if (sourceAnimation.HasFrom)
					{
						BindingValue from;
						if (!resolveValue(sourceAnimation.From,
							sourceAnimation.FromUsesResource,
							sourceAnimation.FromResourceKey,
							from, L"From")) return false;
						animation.From = std::move(from);
					}
					if (sourceAnimation.KeyFrames.empty())
					{
						if (sourceAnimation.HasTo)
						{
							BindingValue to;
							if (!resolveValue(sourceAnimation.To,
								sourceAnimation.ToUsesResource,
								sourceAnimation.ToResourceKey,
								to, L"To")) return false;
							animation.To = std::move(to);
						}
						if (sourceAnimation.HasBy)
						{
							BindingValue by;
							if (!resolveValue(sourceAnimation.By,
								sourceAnimation.ByUsesResource,
								sourceAnimation.ByResourceKey,
								by, L"By")) return false;
							animation.By = std::move(by);
						}
					}
					else
					{
						animation.KeyFrames.reserve(
							sourceAnimation.KeyFrames.size());
						for (const auto& sourceKeyFrame : sourceAnimation.KeyFrames)
						{
							DeclarativeAnimationKeyFrame keyFrame;
							switch (sourceKeyFrame.Kind)
							{
							case DesignerKeyFrameKind::Discrete:
								keyFrame.Kind = DeclarativeKeyFrameKind::Discrete; break;
							case DesignerKeyFrameKind::Easing:
								keyFrame.Kind = DeclarativeKeyFrameKind::Easing; break;
							case DesignerKeyFrameKind::Spline:
								keyFrame.Kind = DeclarativeKeyFrameKind::Spline; break;
							case DesignerKeyFrameKind::Linear:
							default:
								keyFrame.Kind = DeclarativeKeyFrameKind::Linear; break;
							}
							keyFrame.KeyTimeMilliseconds =
								sourceKeyFrame.KeyTimeMilliseconds;
							if (!resolveValue(sourceKeyFrame.Value,
								sourceKeyFrame.UsesResource,
								sourceKeyFrame.ResourceKey,
								keyFrame.Value, L"KeyFrame")) return false;
							switch (sourceKeyFrame.Easing)
							{
							case DesignerEasingKind::Quadratic:
								keyFrame.Easing = DeclarativeEasingKind::Quadratic; break;
							case DesignerEasingKind::Cubic:
								keyFrame.Easing = DeclarativeEasingKind::Cubic; break;
							case DesignerEasingKind::Sine:
								keyFrame.Easing = DeclarativeEasingKind::Sine; break;
							case DesignerEasingKind::Linear:
							default:
								keyFrame.Easing = DeclarativeEasingKind::Linear; break;
							}
							keyFrame.EasingMode = sourceKeyFrame.EasingMode
								== DesignerEasingMode::EaseIn
								? DeclarativeEasingMode::EaseIn
								: sourceKeyFrame.EasingMode
									== DesignerEasingMode::EaseInOut
									? DeclarativeEasingMode::EaseInOut
									: DeclarativeEasingMode::EaseOut;
							keyFrame.KeySplineX1 = sourceKeyFrame.KeySplineX1;
							keyFrame.KeySplineY1 = sourceKeyFrame.KeySplineY1;
							keyFrame.KeySplineX2 = sourceKeyFrame.KeySplineX2;
							keyFrame.KeySplineY2 = sourceKeyFrame.KeySplineY2;
							animation.KeyFrames.push_back(std::move(keyFrame));
						}
					}
					animation.BeginTimeMilliseconds =
						sourceAnimation.BeginTimeMilliseconds;
					animation.DurationMilliseconds =
						sourceAnimation.DurationMilliseconds;
					animation.RepeatBehavior = sourceAnimation.RepeatBehavior
						== DesignerRepeatBehaviorKind::Duration
						? DeclarativeRepeatBehaviorKind::Duration
						: sourceAnimation.RepeatBehavior
							== DesignerRepeatBehaviorKind::Forever
							? DeclarativeRepeatBehaviorKind::Forever
							: DeclarativeRepeatBehaviorKind::Count;
					animation.RepeatCount = sourceAnimation.RepeatCount;
					animation.RepeatDurationMilliseconds =
						sourceAnimation.RepeatDurationMilliseconds;
					animation.IsAdditive = sourceAnimation.IsAdditive;
					animation.IsCumulative = sourceAnimation.IsCumulative;
					animation.AutoReverse = sourceAnimation.AutoReverse;
					animation.FillBehavior = sourceAnimation.FillBehavior
						== DesignerTimelineFillBehavior::Stop
						? DeclarativeTimelineFillBehavior::Stop
						: DeclarativeTimelineFillBehavior::HoldEnd;
					animation.SpeedRatio = sourceAnimation.SpeedRatio;
					animation.AccelerationRatio =
						sourceAnimation.AccelerationRatio;
					animation.DecelerationRatio =
						sourceAnimation.DecelerationRatio;
					switch (sourceAnimation.Easing)
					{
					case DesignerEasingKind::Quadratic:
						animation.Easing = DeclarativeEasingKind::Quadratic; break;
					case DesignerEasingKind::Cubic:
						animation.Easing = DeclarativeEasingKind::Cubic; break;
					case DesignerEasingKind::Sine:
						animation.Easing = DeclarativeEasingKind::Sine; break;
					case DesignerEasingKind::Linear:
					default:
						animation.Easing = DeclarativeEasingKind::Linear; break;
					}
					switch (sourceAnimation.EasingMode)
					{
					case DesignerEasingMode::EaseIn:
						animation.EasingMode = DeclarativeEasingMode::EaseIn; break;
					case DesignerEasingMode::EaseInOut:
						animation.EasingMode = DeclarativeEasingMode::EaseInOut; break;
					case DesignerEasingMode::EaseOut:
					default:
						animation.EasingMode = DeclarativeEasingMode::EaseOut; break;
					}
					state.Animations.push_back(std::move(animation));
				}
				group.States.push_back(std::move(state));
			}
			group.Transitions.reserve(sourceGroup.Transitions.size());
			for (const auto& sourceTransition : sourceGroup.Transitions)
			{
				DeclarativeVisualTransitionDefinition transition;
				transition.FromState = sourceTransition.FromState;
				transition.ToState = sourceTransition.ToState;
				transition.GeneratedDurationMilliseconds =
					sourceTransition.GeneratedDurationMilliseconds;
				switch (sourceTransition.GeneratedEasing)
				{
				case DesignerEasingKind::Quadratic:
					transition.GeneratedEasing =
						DeclarativeEasingKind::Quadratic; break;
				case DesignerEasingKind::Cubic:
					transition.GeneratedEasing =
						DeclarativeEasingKind::Cubic; break;
				case DesignerEasingKind::Sine:
					transition.GeneratedEasing =
						DeclarativeEasingKind::Sine; break;
				case DesignerEasingKind::Linear:
				default:
					transition.GeneratedEasing =
						DeclarativeEasingKind::Linear; break;
				}
				transition.GeneratedEasingMode =
					sourceTransition.GeneratedEasingMode
						== DesignerEasingMode::EaseIn
					? DeclarativeEasingMode::EaseIn
					: sourceTransition.GeneratedEasingMode
						== DesignerEasingMode::EaseInOut
						? DeclarativeEasingMode::EaseInOut
						: DeclarativeEasingMode::EaseOut;
				transition.Animations.reserve(sourceTransition.Animations.size());
				for (const auto& sourceAnimation : sourceTransition.Animations)
				{
					DeclarativeVisualStateAnimation animation;
					if (!materializeAnimation(sourceAnimation,
						L"VisualTransition " + sourceTransition.FromState
							+ L" -> " + sourceTransition.ToState,
						animation)) return false;
					transition.Animations.push_back(std::move(animation));
				}
				group.Transitions.push_back(std::move(transition));
			}
			groups.push_back(std::move(group));
		}
		for (const auto& sourceTrigger : sourceEventTriggers)
		{
			DeclarativeEventTriggerDefinition trigger;
			trigger.EventName = sourceTrigger.EventName;
			if (!DesignerStyleSheetUtils::MaterializeStoryboardActions(
				sourceTrigger.Actions, document.StyleSheet,
				trigger.Actions, outError, document.ResourceBasePath,
				document.Resources,
				L"EventTrigger " + sourceTrigger.EventName)) return false;
			eventTriggers.push_back(std::move(trigger));
		}
		if (outError) outError->clear();
		return true;
	}

	static bool InstallComponentVisualStatesCore(
		Control& control,
		const DesignerModel::DesignComponentDefinition& component,
		const DesignerModel::DesignDocument& document,
		std::wstring* outError)
	{
		std::vector<DeclarativeVisualStateGroupDefinition> groups;
		std::vector<DeclarativeEventTriggerDefinition> eventTriggers;
		if (!MaterializeDeclarativeInteractionsCore(
			&control, component.VisualStateGroups, component.EventTriggers, document,
			groups, eventTriggers, outError)) return false;
		if (groups.empty() && eventTriggers.empty()) return true;
		std::wstring stateError;
		if (!cui::framework::XamlAccess::DefineInteractions(
			control, std::move(groups), std::move(eventTriggers), &stateError))
		{
			if (outError) *outError = L"组件 " + component.Type.XamlName
				+ L" 的声明交互无效：" + stateError;
			return false;
		}
		if (outError) outError->clear();
		return true;
	}

	static bool ApplyTrackedMetadataProperty(
		DesignerControl& designerControl,
		Control& target,
		const std::wstring& propertyName,
		DesignerStyleValue value,
		bool preserveExisting,
		std::wstring* outError = nullptr,
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local)
	{
		const auto* metadata = target.FindPropertyMetadata(propertyName);
		const std::wstring canonicalCandidate = metadata
			? metadata->Name() : propertyName;
		const auto existing = std::find_if(
			designerControl.MetadataProperties.begin(),
			designerControl.MetadataProperties.end(),
			[&](const auto& entry)
			{
				return entry.first == canonicalCandidate;
			});
		if (preserveExisting && existing != designerControl.MetadataProperties.end())
		{
			if (outError) outError->clear();
			return true;
		}

		std::wstring canonicalName;
		DesignerStyleValue effective;
		if (!DesignerPropertyCatalog::ApplyAndTrackValue(
			target, designerControl.MetadataProperties, propertyName, value,
			&canonicalName, &effective, outError, {}, {}, source)) return false;
		designerControl.MetadataPropertyResourceKeys.erase(canonicalName);
		designerControl.MetadataPropertyDynamicResourceKeys.erase(canonicalName);
		return true;
	}

	static std::wstring FromUtf8(const std::string& s)
		{
			return Convert::Utf8ToUnicode(s);
		}

	static bool SetAuthoredCommandTarget(
		Control* source,
		Control* target,
		std::wstring* outError)
	{
		if (auto* button = dynamic_cast<Button*>(source))
		{
			button->SetCommandTarget(target);
			return true;
		}
		if (auto* item = dynamic_cast<MenuItem*>(source))
		{
			item->SetCommandTarget(target);
			return true;
		}
		if (outError) *outError =
			L"CommandTarget source 不是 Button 或 MenuItem。";
		return false;
	}

	static D2D1_COLOR_F ColorFromValue(const DesignValue& j, const D2D1_COLOR_F& def)
		{
			D2D1_COLOR_F c = def;
			if (j.is_object())
			{
				c.r = j.value("r", def.r);
				c.g = j.value("g", def.g);
				c.b = j.value("b", def.b);
				c.a = j.value("a", def.a);
			}
			return c;
		}

	static bool TransformFromValue(
		const DesignValue& value,
		cui::drawing::Transform& output,
		std::wstring* outError)
	{
		if (!value.is_array() || value.empty())
		{
			if (outError) *outError = L"RenderTransform 必须是非空数组。";
			return false;
		}
		output.Operations.clear();
		for (const auto& item : value.ArrayItems())
		{
			if (!item.is_object())
			{
				if (outError) *outError = L"RenderTransform 操作格式无效。";
				return false;
			}
			cui::drawing::TransformOperation operation;
			const auto type = item.value("type", std::string{});
			if (type == "matrix")
			{
				operation.Kind = cui::drawing::TransformKind::Matrix;
				operation.Matrix = D2D1::Matrix3x2F(
					static_cast<float>(item.value("m11", 1.0)),
					static_cast<float>(item.value("m12", 0.0)),
					static_cast<float>(item.value("m21", 0.0)),
					static_cast<float>(item.value("m22", 1.0)),
					static_cast<float>(item.value("dx", 0.0)),
					static_cast<float>(item.value("dy", 0.0)));
			}
			else if (type == "translate")
			{
				operation.Kind = cui::drawing::TransformKind::Translate;
				operation.X = static_cast<float>(item.value("x", 0.0));
				operation.Y = static_cast<float>(item.value("y", 0.0));
			}
			else if (type == "scale")
			{
				operation.Kind = cui::drawing::TransformKind::Scale;
				operation.ScaleX = static_cast<float>(item.value("scaleX", 1.0));
				operation.ScaleY = static_cast<float>(item.value("scaleY", 1.0));
			}
			else if (type == "rotate")
			{
				operation.Kind = cui::drawing::TransformKind::Rotate;
				operation.Angle = static_cast<float>(item.value("angle", 0.0));
			}
			else if (type == "skew")
			{
				operation.Kind = cui::drawing::TransformKind::Skew;
				operation.AngleX = static_cast<float>(item.value("angleX", 0.0));
				operation.AngleY = static_cast<float>(item.value("angleY", 0.0));
			}
			else
			{
				if (outError) *outError = L"RenderTransform 操作类型无效。";
				return false;
			}
			operation.CenterX = static_cast<float>(item.value("centerX", 0.0));
			operation.CenterY = static_cast<float>(item.value("centerY", 0.0));
			output.Operations.push_back(operation);
		}
		return true;
	}

	static bool GeometryFromValue(
		const DesignValue& value,
		cui::drawing::Geometry& output,
		std::wstring* outError)
	{
		if (!value.is_object())
		{
			if (outError) *outError = L"Clip 几何必须是对象。";
			return false;
		}
		output = cui::drawing::Geometry{};
		auto finish = [&]() -> bool
		{
			if (!value.contains("transform")) return true;
			cui::drawing::Transform transform;
			std::wstring transformError;
			if (!TransformFromValue(value["transform"], transform, &transformError))
			{
				if (outError) *outError = L"Geometry.Transform：" + transformError;
				return false;
			}
			output.LocalTransform = std::move(transform);
			return true;
		};
		auto finite = [](std::initializer_list<float> values)
		{
			return std::all_of(values.begin(), values.end(), [](float item)
				{ return std::isfinite(item); });
		};
		const auto type = value.value("type", std::string{});
		if (type == "rectangle")
		{
			const float x = static_cast<float>(value.value("x", 0.0));
			const float y = static_cast<float>(value.value("y", 0.0));
			const float width = static_cast<float>(value.value("width", 0.0));
			const float height = static_cast<float>(value.value("height", 0.0));
			const float radiusX = static_cast<float>(value.value("radiusX", 0.0));
			const float radiusY = static_cast<float>(value.value("radiusY", 0.0));
			if (!std::isfinite(x) || !std::isfinite(y)
				|| !std::isfinite(width) || !std::isfinite(height)
				|| !std::isfinite(radiusX) || !std::isfinite(radiusY)
				|| width < 0.0f || height < 0.0f
				|| radiusX < 0.0f || radiusY < 0.0f)
			{
				if (outError) *outError = L"RectangleGeometry 数值无效。";
				return false;
			}
			output.Kind = cui::drawing::GeometryKind::Rectangle;
			output.Rect = D2D1::RectF(x, y, x + width, y + height);
			output.RadiusX = radiusX;
			output.RadiusY = radiusY;
			return finish();
		}
		if (type == "ellipse")
		{
			const float centerX = static_cast<float>(value.value("centerX", 0.0));
			const float centerY = static_cast<float>(value.value("centerY", 0.0));
			const float radiusX = static_cast<float>(value.value("radiusX", 0.0));
			const float radiusY = static_cast<float>(value.value("radiusY", 0.0));
			if (!std::isfinite(centerX) || !std::isfinite(centerY)
				|| !std::isfinite(radiusX) || !std::isfinite(radiusY)
				|| radiusX < 0.0f || radiusY < 0.0f)
			{
				if (outError) *outError = L"EllipseGeometry 数值无效。";
				return false;
			}
			output.Kind = cui::drawing::GeometryKind::Ellipse;
			output.Center = D2D1::Point2F(centerX, centerY);
			output.RadiusX = radiusX;
			output.RadiusY = radiusY;
			return finish();
		}
		if (type == "path")
		{
			output.Kind = cui::drawing::GeometryKind::Path;
			const auto fillRule = value.value("fillRule", std::string("evenodd"));
			if (fillRule == "nonzero")
				output.FillRule = cui::drawing::GeometryFillRule::Nonzero;
			else if (fillRule != "evenodd")
			{
				if (outError) *outError = L"PathGeometry FillRule 无效。";
				return false;
			}
			if (!value.contains("figures") || !value["figures"].is_array())
			{
				if (outError) *outError = L"PathGeometry 缺少 Figures。";
				return false;
			}
			for (const auto& figureValue : value["figures"].ArrayItems())
			{
				if (!figureValue.is_object() || !figureValue.contains("segments")
					|| !figureValue["segments"].is_array())
				{
					if (outError) *outError = L"PathFigure 格式无效。";
					return false;
				}
				cui::drawing::PathFigure figure;
				figure.StartPoint = D2D1::Point2F(
					static_cast<float>(figureValue.value("startX", 0.0)),
					static_cast<float>(figureValue.value("startY", 0.0)));
				figure.IsClosed = figureValue.value("closed", false);
				figure.IsFilled = figureValue.value("filled", true);
				if (!finite({ figure.StartPoint.x, figure.StartPoint.y }))
				{
					if (outError) *outError = L"PathFigure.StartPoint 数值无效。";
					return false;
				}
				for (const auto& segmentValue : figureValue["segments"].ArrayItems())
				{
					if (!segmentValue.is_object())
					{
						if (outError) *outError = L"PathSegment 格式无效。";
						return false;
					}
					cui::drawing::PathSegment segment;
					const auto segmentType = segmentValue.value("type", std::string{});
					if (segmentType == "line")
						segment.Kind = cui::drawing::PathSegmentKind::Line;
					else if (segmentType == "bezier")
						segment.Kind = cui::drawing::PathSegmentKind::Bezier;
					else if (segmentType == "quadratic")
						segment.Kind = cui::drawing::PathSegmentKind::QuadraticBezier;
					else if (segmentType == "arc")
						segment.Kind = cui::drawing::PathSegmentKind::Arc;
					else
					{
						if (outError) *outError = L"PathSegment 类型无效。";
						return false;
					}
					segment.Point = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x", 0.0)),
						static_cast<float>(segmentValue.value("y", 0.0)));
					segment.Point1 = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x1", 0.0)),
						static_cast<float>(segmentValue.value("y1", 0.0)));
					segment.Point2 = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x2", 0.0)),
						static_cast<float>(segmentValue.value("y2", 0.0)));
					segment.Point3 = D2D1::Point2F(
						static_cast<float>(segmentValue.value("x3", 0.0)),
						static_cast<float>(segmentValue.value("y3", 0.0)));
					segment.Size = D2D1::SizeF(
						static_cast<float>(segmentValue.value("width", 0.0)),
						static_cast<float>(segmentValue.value("height", 0.0)));
					segment.RotationAngle = static_cast<float>(
						segmentValue.value("rotation", 0.0));
					segment.IsLargeArc = segmentValue.value("large", false);
					const auto sweep = segmentValue.value(
						"sweep", std::string("counterclockwise"));
					if (sweep == "clockwise")
						segment.Sweep = cui::drawing::SweepDirection::Clockwise;
					else if (sweep != "counterclockwise")
					{
						if (outError) *outError = L"ArcSegment SweepDirection 无效。";
						return false;
					}
					if (!finite({ segment.Point.x, segment.Point.y,
						segment.Point1.x, segment.Point1.y,
						segment.Point2.x, segment.Point2.y,
						segment.Point3.x, segment.Point3.y,
						segment.Size.width, segment.Size.height,
						segment.RotationAngle })
						|| segment.Size.width < 0.0f || segment.Size.height < 0.0f)
					{
						if (outError) *outError = L"PathSegment 数值无效。";
						return false;
					}
					figure.Segments.push_back(segment);
				}
				output.Figures.push_back(std::move(figure));
			}
			return finish();
		}
		if (type != "group")
		{
			if (outError) *outError = L"Clip 几何类型无效。";
			return false;
		}
		output.Kind = cui::drawing::GeometryKind::Group;
		const auto fillRule = value.value("fillRule", std::string("evenodd"));
		if (fillRule == "nonzero")
			output.FillRule = cui::drawing::GeometryFillRule::Nonzero;
		else if (fillRule == "evenodd")
			output.FillRule = cui::drawing::GeometryFillRule::EvenOdd;
		else
		{
			if (outError) *outError = L"GeometryGroup FillRule 无效。";
			return false;
		}
		if (!value.contains("children") || !value["children"].is_array())
		{
			if (outError) *outError = L"GeometryGroup 缺少 Children。";
			return false;
		}
		for (const auto& childValue : value["children"].ArrayItems())
		{
			cui::drawing::Geometry child;
			if (!GeometryFromValue(childValue, child, outError)) return false;
			output.Children.push_back(std::move(child));
		}
		return finish();
	}

	static std::wstring ColorToMetadataText(const D2D1_COLOR_F& color)
		{
			auto byte = [](float value) -> unsigned int
			{
				return static_cast<unsigned int>(std::lround(
					(std::clamp)(value, 0.0f, 1.0f) * 255.0f));
			};
			wchar_t text[10]{};
			swprintf_s(text, L"#%02X%02X%02X%02X",
				byte(color.a), byte(color.r), byte(color.g), byte(color.b));
			return text;
		}

	static Thickness ThicknessFromValue(const DesignValue& j, const Thickness& def)
		{
			Thickness t = def;
			if (j.is_object())
			{
				t.Left = j.value("l", def.Left);
				t.Top = j.value("t", def.Top);
				t.Right = j.value("r", def.Right);
				t.Bottom = j.value("b", def.Bottom);
			}
			return t;
		}

	static bool TryParseHorizontalAlignment(const std::string& s, HorizontalAlignment& out)
		{
			if (s == "Left") { out = HorizontalAlignment::Left; return true; }
			if (s == "Center") { out = HorizontalAlignment::Center; return true; }
			if (s == "Right") { out = HorizontalAlignment::Right; return true; }
			if (s == "Stretch") { out = HorizontalAlignment::Stretch; return true; }
			return false;
		}

	static bool TryParseVerticalAlignment(const std::string& s, VerticalAlignment& out)
		{
			if (s == "Top") { out = VerticalAlignment::Top; return true; }
			if (s == "Center") { out = VerticalAlignment::Center; return true; }
			if (s == "Bottom") { out = VerticalAlignment::Bottom; return true; }
			if (s == "Stretch") { out = VerticalAlignment::Stretch; return true; }
			return false;
		}

	static bool TryParseDock(const std::string& s, Dock& out)
		{
			if (s == "Left") { out = Dock::Left; return true; }
			if (s == "Top") { out = Dock::Top; return true; }
			if (s == "Right") { out = Dock::Right; return true; }
			if (s == "Bottom") { out = Dock::Bottom; return true; }
			return false;
		}

	static bool TryParseOrientation(const std::string& s, Orientation& out)
		{
			if (s == "Horizontal") { out = Orientation::Horizontal; return true; }
			if (s == "Vertical") { out = Orientation::Vertical; return true; }
			return false;
		}

	static bool TryParseSizeUnit(const std::string& s, SizeUnit& out)
		{
			if (s == "Pixel") { out = SizeUnit::Pixel; return true; }
			if (s == "Auto") { out = SizeUnit::Auto; return true; }
			if (s == "Star") { out = SizeUnit::Star; return true; }
			return false;
		}

	static GridLength GridLengthFromValue(const DesignValue& j, const GridLength& def)
		{
			GridLength gl = def;
			if (!j.is_object()) return gl;
			gl.Value = j.value("value", def.Value);
			SizeUnit u = def.Unit;
			if (j.contains("unit") && j["unit"].is_string())
			{
				TryParseSizeUnit(j["unit"].get<std::string>(), u);
			}
			gl.Unit = u;
			return gl;
		}

	static bool ConnectTemplateEvent(
		Control& source,
		const std::wstring& sourceEvent,
		Control& owner,
		const DesignerComponentEventDescriptor& targetEvent,
		std::wstring* outError)
	{
		EventConnection connection;
		auto raiseNone = [&owner, name = targetEvent.Name]
		{
			owner.RaiseDeclarativeEvent(name);
		};
		const bool textCompositionEvent =
			sourceEvent == L"PreviewTextInputStart"
			|| sourceEvent == L"TextInputStart"
			|| sourceEvent == L"PreviewTextInputUpdate"
			|| sourceEvent == L"TextInputUpdate"
			|| sourceEvent == L"PreviewTextInput"
			|| sourceEvent == L"TextInput";
		const bool dragDropEvent =
			sourceEvent == L"PreviewDragEnter"
			|| sourceEvent == L"DragEnter"
			|| sourceEvent == L"PreviewDragOver"
			|| sourceEvent == L"DragOver"
			|| sourceEvent == L"PreviewDragLeave"
			|| sourceEvent == L"DragLeave"
			|| sourceEvent == L"PreviewDrop"
			|| sourceEvent == L"Drop";
		const bool projectCommittedText =
			sourceEvent == L"PreviewTextInput"
			|| sourceEvent == L"TextInput";
		auto subscribeTextComposition = [&](auto handler) -> EventConnection
		{
			if (sourceEvent == L"PreviewTextInputStart")
				return source.OnPreviewTextInputStart.Subscribe(handler);
			if (sourceEvent == L"TextInputStart")
				return source.OnTextInputStart.Subscribe(handler);
			if (sourceEvent == L"PreviewTextInputUpdate")
				return source.OnPreviewTextInputUpdate.Subscribe(handler);
			if (sourceEvent == L"TextInputUpdate")
				return source.OnTextInputUpdate.Subscribe(handler);
			if (sourceEvent == L"PreviewTextInput")
				return source.OnPreviewTextInput.Subscribe(handler);
			if (sourceEvent == L"TextInput")
				return source.OnTextInput.Subscribe(handler);
			return {};
		};
		auto subscribeDragDrop = [&](auto handler) -> EventConnection
		{
			if (sourceEvent == L"PreviewDragEnter")
				return source.OnPreviewDragEnter.Subscribe(handler);
			if (sourceEvent == L"DragEnter")
				return source.OnDragEnter.Subscribe(handler);
			if (sourceEvent == L"PreviewDragOver")
				return source.OnPreviewDragOver.Subscribe(handler);
			if (sourceEvent == L"DragOver")
				return source.OnDragOver.Subscribe(handler);
			if (sourceEvent == L"PreviewDragLeave")
				return source.OnPreviewDragLeave.Subscribe(handler);
			if (sourceEvent == L"DragLeave")
				return source.OnDragLeave.Subscribe(handler);
			if (sourceEvent == L"PreviewDrop")
				return source.OnPreviewDrop.Subscribe(handler);
			if (sourceEvent == L"Drop")
				return source.OnDrop.Subscribe(handler);
			return {};
		};
		if (targetEvent.Payload == DesignerComponentEventPayload::None)
		{
			if (textCompositionEvent)
				connection = subscribeTextComposition(
					[raiseNone](Control*, TextCompositionEventArgs&) mutable
					{ raiseNone(); });
			else if (dragDropEvent)
				connection = subscribeDragDrop(
					[raiseNone](Control*, DragEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"Click")
				connection = cui::framework::RoutedEventAccess::SubscribeClick(
					source,
					[raiseNone](Control*, RoutedEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"PreviewMouseDown")
				connection = source.OnPreviewMouseDown.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"MouseDown")
				connection = source.OnMouseDown.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"PreviewMouseUp")
				connection = source.OnPreviewMouseUp.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"MouseUp")
				connection = source.OnMouseUp.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"MouseDoubleClick")
				connection = source.OnMouseDoubleClick.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"MouseEnter")
				connection = source.OnMouseEnter.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"MouseLeave")
				connection = source.OnMouseLeave.Subscribe(
					[raiseNone](Control*, MouseEventArgs&) mutable { raiseNone(); });
			else if (sourceEvent == L"PreviewGotKeyboardFocus")
				connection = source.OnPreviewGotKeyboardFocus.Subscribe(
					[raiseNone](Control*, KeyboardFocusChangedEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"GotKeyboardFocus")
				connection = source.OnGotKeyboardFocus.Subscribe(
					[raiseNone](Control*, KeyboardFocusChangedEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"PreviewLostKeyboardFocus")
				connection = source.OnPreviewLostKeyboardFocus.Subscribe(
					[raiseNone](Control*, KeyboardFocusChangedEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"LostKeyboardFocus")
				connection = source.OnLostKeyboardFocus.Subscribe(
					[raiseNone](Control*, KeyboardFocusChangedEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"GotFocus")
				connection = source.OnGotFocus.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"LostFocus")
				connection = source.OnLostFocus.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"SizeChanged")
				connection = source.SizeChanged.Subscribe(
					[raiseNone](Control*, SizeChangedEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"IsVisibleChanged")
				connection = source.IsVisibleChanged.Subscribe(
					[raiseNone](DependencyObject*,
						const DependencyPropertyChangedEventArgs&) mutable
					{ raiseNone(); });
			else if (sourceEvent == L"SelectionChanged")
			{
				if (auto* tabs = dynamic_cast<TabControl*>(&source))
					connection = tabs->SelectionChanged.Subscribe(
						[raiseNone](Control*, SelectionChangedEventArgs&) mutable
						{ raiseNone(); });
			}
			else if (sourceEvent == L"Selected")
			{
				if (auto* item = dynamic_cast<TabItem*>(&source))
					connection = item->Selected.Subscribe(
						[raiseNone](Control*, RoutedEventArgs&) mutable
						{ raiseNone(); });
			}
			else if (sourceEvent == L"Unselected")
			{
				if (auto* item = dynamic_cast<TabItem*>(&source))
					connection = item->Unselected.Subscribe(
						[raiseNone](Control*, RoutedEventArgs&) mutable
						{ raiseNone(); });
			}
			else if (sourceEvent == L"Checked"
				|| sourceEvent == L"Unchecked")
			{
				auto subscribe = [&](auto* owner)
				{
					if (!owner) return;
					if (sourceEvent == L"Checked")
						connection = owner->Checked.Subscribe(
							[raiseNone](Control*, RoutedEventArgs&) mutable
							{ raiseNone(); });
					else
						connection = owner->Unchecked.Subscribe(
							[raiseNone](Control*, RoutedEventArgs&) mutable
							{ raiseNone(); });
				};
				if (auto* toggle = dynamic_cast<ToggleButton*>(&source))
					subscribe(toggle);
				else if (auto* item = dynamic_cast<MenuItem*>(&source))
					subscribe(item);
			}
			else if (sourceEvent == L"Expanded"
				|| sourceEvent == L"Collapsed")
			{
				if (auto* expander = dynamic_cast<Expander*>(&source))
				{
					if (sourceEvent == L"Expanded")
						connection = expander->Expanded.Subscribe(
							[raiseNone](Control*, RoutedEventArgs&) mutable
							{ raiseNone(); });
					else
						connection = expander->Collapsed.Subscribe(
							[raiseNone](Control*, RoutedEventArgs&) mutable
							{ raiseNone(); });
				}
			}
			else if (sourceEvent == L"SubmenuOpened"
				|| sourceEvent == L"SubmenuClosed")
			{
				if (auto* item = dynamic_cast<MenuItem*>(&source))
				{
					if (sourceEvent == L"SubmenuOpened")
						connection = item->SubmenuOpened.Subscribe(
							[raiseNone](Control*, RoutedEventArgs&) mutable
							{ raiseNone(); });
					else
						connection = item->SubmenuClosed.Subscribe(
							[raiseNone](Control*, RoutedEventArgs&) mutable
							{ raiseNone(); });
				}
			}
			else if (sourceEvent == L"Opened")
			{
				if (auto* popup = dynamic_cast<Popup*>(&source))
					connection = popup->Opened.Subscribe(
						[raiseNone](Popup*) mutable { raiseNone(); });
				else if (auto* menu = dynamic_cast<ContextMenu*>(&source))
					connection = menu->Opened.Subscribe(
						[raiseNone](ContextMenu*) mutable { raiseNone(); });
			}
			else if (sourceEvent == L"Closed")
			{
				if (auto* popup = dynamic_cast<Popup*>(&source))
					connection = popup->Closed.Subscribe(
						[raiseNone](Popup*) mutable { raiseNone(); });
				else if (auto* menu = dynamic_cast<ContextMenu*>(&source))
					connection = menu->Closed.Subscribe(
						[raiseNone](ContextMenu*) mutable { raiseNone(); });
			}
			else if (sourceEvent == L"ScrollChanged")
			{
				if (auto* scroll = dynamic_cast<ScrollViewer*>(&source))
					connection = scroll->OnScrollChanged.Subscribe(
						[raiseNone](Control*, ScrollChangedEventArgs&) mutable
						{ raiseNone(); });
			}
		}
		else if (targetEvent.Payload == DesignerComponentEventPayload::Bool
			&& (sourceEvent == L"Checked" || sourceEvent == L"Unchecked"))
		{
			if (auto* toggle = dynamic_cast<ToggleButton*>(&source))
			{
				const bool projectedValue = sourceEvent == L"Checked";
				auto projectValue =
					[&owner, name = targetEvent.Name, projectedValue](
						Control*, RoutedEventArgs&)
					{
						owner.RaiseDeclarativeEvent(name,
							BindingValue(projectedValue));
					};
				connection = projectedValue
					? toggle->Checked.Subscribe(projectValue)
					: toggle->Unchecked.Subscribe(projectValue);
			}
		}
		else if (targetEvent.Payload == DesignerComponentEventPayload::String
			&& textCompositionEvent)
		{
			connection = subscribeTextComposition(
				[&owner, name = targetEvent.Name, projectCommittedText](
					Control*, TextCompositionEventArgs& args)
				{
					owner.RaiseDeclarativeEvent(name, BindingValue(
						projectCommittedText ? args.Text : args.CompositionText));
				});
		}
		else if (targetEvent.Payload == DesignerComponentEventPayload::String
			&& sourceEvent == L"TextChanged")
		{
			auto subscribe = [&]<typename TTextControl>(TTextControl* textControl)
			{
				if (!textControl) return;
				connection = textControl->OnTextChanged.Subscribe(
					[&owner, name = targetEvent.Name](
						Control*, TextChangedEventArgs& args)
					{
						owner.RaiseDeclarativeEvent(
							name, BindingValue(args.NewText));
					});
			};
			if (auto* textBox = dynamic_cast<TextBox*>(&source))
				subscribe(textBox);
			else if (auto* richTextBox = dynamic_cast<RichTextBox*>(&source))
				subscribe(richTextBox);
		}
		if (!connection.Connected())
		{
			if (outError) *outError = L"不支持的模板事件转发："
				+ sourceEvent + L" -> " + targetEvent.Name;
			return false;
		}
		cui::framework::XamlAccess::RetainTemplateEventConnection(
			owner, std::move(connection));
		return true;
	}

	class MaterializedDataTemplate final : public IItemTemplate
	{
	public:
		MaterializedDataTemplate(
			std::shared_ptr<const DesignerModel::DesignDocument> document,
			DesignerModel::DesignDataTemplate definition,
			DesignerModel::DesignObjectResourceDictionary visibleObjects,
			DesignerStyleSheet visibleStyles,
			CuiRuntime::XamlMaterializationOptions options,
			DesignerDataContextSchema schema = {})
			: _document(std::move(document)),
			  _definition(std::move(definition)),
			  _visibleObjects(std::move(visibleObjects)),
			  _visibleStyles(std::move(visibleStyles)),
			  _options(std::move(options)),
			  _schema(std::move(schema))
		{
			_dataType = _definition.DataType;
			if (_document && _schema.empty())
			{
				if (const auto* type = _document->FindDataType(_dataType))
					_schema = type->Properties;
				else if (DesignerModel::DesignDataResourceUtils::
					IsCollectionViewGroupDataType(_dataType))
					_schema = DesignerModel::DesignDataResourceUtils::
						BuildCollectionViewGroupSchema();
			}
		}

		const std::wstring& DataTypeName() const noexcept override
		{
			return _dataType;
		}

		bool IsHierarchical() const noexcept override
		{
			return _definition.Hierarchical;
		}

		bool TryGetVisualChildItemsSource(
			const BindingSourceReference& item,
			BindingListReference& out,
			std::wstring* outError) const override
		{
			out = {};
			if (!_definition.Hierarchical || !_definition.ItemsSourceBinding)
			{
				if (outError) outError->clear();
				return true;
			}
			if (!item)
			{
				if (outError) *outError =
					L"HierarchicalDataTemplate 缺少当前数据项。";
				return false;
			}
			const auto& binding = *_definition.ItemsSourceBinding;
			BindingValue value;
			if (!TryGetBindingPathValue(
				*item.Get(), binding.SourceProperty, value))
			{
				if (outError) *outError =
					L"HierarchicalDataTemplate.ItemsSource 无法读取路径："
					+ binding.SourceProperty;
				return false;
			}
			if (!binding.Converter.empty())
			{
				auto converter = BindingValueConverterRegistry::Create(
					binding.Converter);
				if (!converter)
				{
					if (outError) *outError =
						L"HierarchicalDataTemplate.ItemsSource Converter 不存在："
						+ binding.Converter;
					return false;
				}
				std::optional<BindingValue> parameter;
				std::wstring literalError;
				if (!DesignerBindingUtils::TryConvertOptionalLiteral(
					binding.ConverterParameter, parameter, &literalError))
				{
					if (outError) *outError = literalError;
					return false;
				}
				BindingValue converted;
				BindingValueConverterContext context;
				context.Parameter = parameter ? &*parameter : nullptr;
				context.TargetKind = BindingValueKind::Object;
				if (!converter->Convert(value, context, converted))
				{
					if (outError) *outError =
						L"HierarchicalDataTemplate.ItemsSource Converter 转换失败："
						+ binding.Converter;
					return false;
				}
				value = std::move(converted);
			}
			if (value.Empty())
			{
				if (outError) outError->clear();
				return true;
			}
			if (!value.TryGet(out))
			{
				if (outError) *outError =
					L"HierarchicalDataTemplate.ItemsSource 未返回 BindingList。";
				return false;
			}
			if (outError) outError->clear();
			return true;
		}

		BindingPathObservation ObserveChildItemsSource(
			const BindingSourceReference& item,
			std::function<void()> changed) const override
		{
			if (!_definition.ItemsSourceBinding) return {};
			return ObserveBindingPaths(item,
				{ _definition.ItemsSourceBinding->SourceProperty },
				std::move(changed));
		}

		std::unique_ptr<Control> Build(
			const BindingSourceReference& item,
			size_t,
			std::wstring* outError) const override
		{
			if (!_document || !item)
			{
				if (outError) *outError = L"DataTemplate 缺少文档或数据项。";
				return {};
			}
			if (_schema.empty() || _definition.Template.empty())
			{
				if (outError) *outError = L"DataTemplate 定义不完整："
					+ _definition.DisplayName();
				return {};
			}

			std::call_once(_compileOnce, [this]
			{
				try
				{
					DesignerModel::DesignDocument templateDocument = *_document;
					templateDocument.Nodes = _definition.Template;
					templateDocument.Components = _visibleObjects.Components;
					templateDocument.ControlTemplates = _visibleObjects.ControlTemplates;
					templateDocument.DataTemplates = _visibleObjects.DataTemplates;
					templateDocument.ItemsPanelTemplates =
						_visibleObjects.ItemsPanelTemplates;
					templateDocument.GroupStyles = _visibleObjects.GroupStyles;
					templateDocument.StyleSheet = _visibleStyles;
					templateDocument.DataContextSchema = _schema;
					templateDocument.Window.Events.clear();
					templateDocument.Window.Bindings.clear();
					templateDocument.Window.CommandBindings.clear();
					templateDocument.Window.InputBindings.clear();
					// A template has an item-scoped DataContext. Do not materialize
					// unrelated page-scoped lists just because they share a document.
					std::unordered_set<std::wstring> referencedLists;
					std::unordered_set<const DesignerModel::DesignDataTemplate*>
						referencedTemplates;
					auto resourceItemType = [&](const std::wstring& resourceKey,
						const DesignerDataContextSchema& schema)
					{
						std::unordered_set<std::wstring> visited;
						auto key = resourceKey;
						while (!key.empty() && visited.insert(key).second)
						{
							if (const auto* list = templateDocument.FindDataList(key))
								return list->ItemType;
							const auto* view = templateDocument.FindCollectionView(key);
							if (!view) break;
							if (!view->SourceResource.empty())
							{
								key = view->SourceResource;
								continue;
							}
							const auto* property = DesignerDataContextSchemaUtils::Find(
								schema, view->SourceBindingPath);
							return property
								&& property->ObjectKind
									== DesignerDataObjectKind::BindingList
								? property->ItemType : std::wstring{};
						}
						return std::wstring{};
					};
					std::function<void(
						const std::vector<DesignerModel::DesignNode>&,
						const DesignerDataContextSchema&)> scanNodes;
					auto scanTemplate =
						[&](const DesignerModel::DesignDataTemplate* definition)
					{
						if (!definition
							|| !referencedTemplates.insert(definition).second)
							return;
						DesignerDataContextSchema schema;
						if (const auto* type = templateDocument.FindDataType(
							definition->DataType)) schema = type->Properties;
						else if (DesignerModel::DesignDataResourceUtils::
							IsCollectionViewGroupDataType(definition->DataType))
							schema = DesignerModel::DesignDataResourceUtils::
								BuildCollectionViewGroupSchema();
						scanNodes(definition->Template, schema);
					};
					scanNodes = [&](const std::vector<DesignerModel::DesignNode>& nodes,
						const DesignerDataContextSchema& schema)
					{
						for (const auto& node : nodes)
						{
							if (!node.Structure.ItemsSourceResource.empty())
								referencedLists.insert(
									node.Structure.ItemsSourceResource);
							const DesignerModel::DesignDataTemplate*
								itemTemplate = nullptr;
							if (!node.Structure.ItemTemplate.empty())
								itemTemplate = templateDocument.FindDataTemplate(
									nodes, node, node.Structure.ItemTemplate);
							else if (IsUIClassAssignableFrom(
								UIClass::UI_ItemsControl, node.Type))
							{
								std::wstring itemType;
								if (!node.Structure.ItemsSourceResource.empty())
									itemType = resourceItemType(
										node.Structure.ItemsSourceResource, schema);
								else if (const auto found =
									node.Bindings.find(L"ItemsSource");
									found != node.Bindings.end())
								{
									const auto& binding = found->second;
									if (binding.ElementName.empty()
										&& binding.RelativeSource
											== DesignerBindingRelativeSource::None
										&& !binding.IsMultiBinding())
										if (const auto* property =
											DesignerDataContextSchemaUtils::Find(
												schema, binding.SourceProperty);
											property && property->ObjectKind
												== DesignerDataObjectKind::BindingList)
											itemType = property->ItemType;
								}
								if (!itemType.empty())
									itemTemplate = templateDocument.
										FindImplicitDataTemplate(
											nodes, node, itemType);
							}
							scanTemplate(itemTemplate);
							const DesignerModel::DesignDataTemplate*
								contentTemplate = nullptr;
							if (!node.Structure.ContentTemplate.empty())
								contentTemplate = templateDocument.FindDataTemplate(
									nodes, node, node.Structure.ContentTemplate);
							else if (const auto found =
								node.Bindings.find(L"Content");
								IsContentHostType(node.Type)
									&& found != node.Bindings.end())
							{
								const auto& binding = found->second;
								if (binding.ElementName.empty()
									&& binding.RelativeSource
										== DesignerBindingRelativeSource::None
									&& !binding.IsMultiBinding())
									if (const auto* property =
										DesignerDataContextSchemaUtils::Find(
											schema, binding.SourceProperty);
										property && property->ObjectKind
											== DesignerDataObjectKind::BindingSource
										&& !property->DataType.empty())
										contentTemplate = templateDocument.
											FindImplicitDataTemplate(
												nodes, node, property->DataType);
							}
							scanTemplate(contentTemplate);
							const DesignerModel::DesignDataTemplate*
								headerTemplate = nullptr;
							if (!node.Structure.HeaderTemplate.empty())
								headerTemplate = templateDocument.FindDataTemplate(
									nodes, node, node.Structure.HeaderTemplate);
							else if (const auto found =
								node.Bindings.find(L"Header");
								IsHeaderedContentControlType(node.Type)
									&& found != node.Bindings.end())
							{
								const auto& binding = found->second;
								if (binding.ElementName.empty()
									&& binding.RelativeSource
										== DesignerBindingRelativeSource::None
									&& !binding.IsMultiBinding())
									if (const auto* property =
										DesignerDataContextSchemaUtils::Find(
											schema, binding.SourceProperty);
										property && property->ObjectKind
											== DesignerDataObjectKind::BindingSource
										&& !property->DataType.empty())
										headerTemplate = templateDocument.
											FindImplicitDataTemplate(
												nodes, node, property->DataType);
							}
							scanTemplate(headerTemplate);
							if (!node.Structure.GroupStyle.empty())
								scanTemplate(templateDocument.
									FindGroupStyleHeaderTemplate(
										nodes, node, node.Structure.GroupStyle));
						}
					};
					scanNodes(templateDocument.Nodes, _schema);
					for (bool changed = true; changed;)
					{
						changed = false;
						for (const auto& view : _document->CollectionViews)
							if (referencedLists.contains(view.Key)
								&& !view.SourceResource.empty()
								&& referencedLists.insert(
									view.SourceResource).second)
								changed = true;
					}
					templateDocument.CollectionViews.erase(std::remove_if(
						templateDocument.CollectionViews.begin(),
						templateDocument.CollectionViews.end(),
						[&](const auto& view)
						{ return !referencedLists.contains(view.Key); }),
						templateDocument.CollectionViews.end());
					templateDocument.DataLists.erase(std::remove_if(
						templateDocument.DataLists.begin(),
						templateDocument.DataLists.end(),
						[&](const auto& list)
						{ return !referencedLists.contains(list.Key); }),
						templateDocument.DataLists.end());
					templateDocument.RecalculateNextStableId();
					auto compiled =
						std::make_shared<CuiRuntime::XamlCompiledDocument>();
					CuiRuntime::XamlDocumentCompilationOptions compileOptions;
					compileOptions.Theme = _options.Theme;
					compileOptions.UseFrameworkTheme =
						_options.UseFrameworkTheme;
					if (CuiRuntime::XamlDocumentCompiler::Compile(
						templateDocument, *compiled, compileOptions,
						&_compileError))
						_compiledDocument = std::move(compiled);
				}
				catch (const std::exception& exception)
				{
					_compileError = L"DataTemplate 编译失败："
						+ FromUtf8(exception.what());
				}
				catch (...)
				{
					_compileError = L"DataTemplate 编译失败：未知异常。";
				}
			});
			if (!_compiledDocument)
			{
				if (outError) *outError = _compileError.empty()
					? L"DataTemplate 编译失败。" : _compileError;
				return {};
			}
			CuiRuntime::XamlObjectTree tree;
			auto materializationOptions = _options;
			materializationOptions.Theme = _compiledDocument->Theme;
			materializationOptions.CompiledDocument = _compiledDocument;
			if (!CuiRuntime::XamlObjectMaterializer::Materialize(
				_compiledDocument->Document, tree,
				materializationOptions, outError)) return {};
			if (!tree.ContentRoot)
			{
				if (outError) *outError = L"DataTemplate 必须生成一个视觉根："
					+ _definition.DisplayName();
				return {};
			}
			if (!tree.ContentRoot->SetDataContext(item))
			{
				if (outError) *outError = L"DataTemplate 无法建立记录 DataContext："
					+ _definition.Key;
				return {};
			}

			std::unordered_map<std::wstring, Control*> controlsByName;
			for (const auto& record : tree.Controls)
				if (record && record->ControlInstance)
					controlsByName.emplace(record->Name, record->ControlInstance);
			for (const auto& record : tree.Controls)
			{
				if (!record || !record->ControlInstance) continue;
				for (const auto& [targetProperty, binding] : record->DataBindings)
				{
					if (binding.IsMultiBinding())
					{
						auto resolveSource = [&](const DesignerDataBinding& child,
							DesignerBindingUtils::ResolvedBindingSource& resolved,
							std::wstring* error)
						{
							if (!child.ElementName.empty())
							{
								const auto source = controlsByName.find(child.ElementName);
								if (source == controlsByName.end() || !source->second)
								{
									if (error) *error = L"ElementName 不存在：" + child.ElementName;
									return false;
								}
								resolved.Source = source->second;
							}
							else if (child.RelativeSource
								== DesignerBindingRelativeSource::Self)
								resolved.Source = record->ControlInstance;
							else if (child.RelativeSource
								== DesignerBindingRelativeSource::TemplatedParent)
							{
								if (error) *error = L"DataTemplate 不存在可解析的 TemplatedParent。";
								return false;
							}
							else if (child.RelativeSource
								== DesignerBindingRelativeSource::FindAncestor)
							{
								resolved.OwnedSource = DesignerBindingUtils::CreateAncestorSource(
									*record->ControlInstance, child);
								resolved.Source = resolved.OwnedSource.Get();
							}
							else if (targetProperty == L"DataContext")
								resolved.Source = record->ControlInstance->GetInheritanceParent()
									? &record->ControlInstance->GetInheritanceParent()->DataContextSource()
									: item.Get();
							else resolved.Source =
								&record->ControlInstance->DataContextSource();
							return resolved.Source || resolved.OwnedSource;
						};
						std::wstring installError;
						if (!DesignerBindingUtils::InstallBinding(
							*record->ControlInstance, targetProperty, binding,
							resolveSource, &installError))
						{
							if (outError) *outError = L"DataTemplate MultiBinding："
								+ installError;
							return {};
						}
						continue;
					}
					std::shared_ptr<const IBindingValueConverter> converter;
					if (!binding.Converter.empty())
					{
						converter = BindingValueConverterRegistry::Create(
							binding.Converter);
						if (!converter)
						{
							if (outError) *outError = L"DataTemplate Converter 不存在："
								+ binding.Converter;
							return {};
						}
					}
					std::optional<BindingValue> fallbackValue;
					std::optional<BindingValue> targetNullValue;
					std::optional<BindingValue> converterParameter;
					std::wstring literalError;
					if (!DesignerBindingUtils::TryConvertOptionalLiteral(
						binding.FallbackValue, fallbackValue, &literalError)
						|| !DesignerBindingUtils::TryConvertOptionalLiteral(
							binding.TargetNullValue, targetNullValue, &literalError)
						|| !DesignerBindingUtils::TryConvertOptionalLiteral(
							binding.ConverterParameter, converterParameter, &literalError))
					{
						if (outError) *outError = L"DataTemplate Binding："
							+ literalError;
						return {};
					}
					IBindingSource* bindingSource = nullptr;
					BindingSourceReference ownedBindingSource;
					if (!binding.ElementName.empty())
					{
						const auto source = controlsByName.find(binding.ElementName);
						if (source == controlsByName.end() || !source->second)
						{
							if (outError) *outError = L"DataTemplate ElementName 不存在："
								+ binding.ElementName;
							return {};
						}
						bindingSource = source->second;
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::Self)
						bindingSource = record->ControlInstance;
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::TemplatedParent)
					{
						if (outError) *outError = L"DataTemplate 不存在可解析的 TemplatedParent："
							+ record->Name + L"." + targetProperty;
						return {};
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						ownedBindingSource = DesignerBindingUtils::CreateAncestorSource(
							*record->ControlInstance, binding);
						bindingSource = ownedBindingSource.Get();
					}
					else if (targetProperty == L"DataContext")
					bindingSource = record->ControlInstance->GetInheritanceParent()
						? &record->ControlInstance->GetInheritanceParent()->DataContextSource()
							: item.Get();
					else
						bindingSource = &record->ControlInstance->DataContextSource();
					const auto installed = ownedBindingSource
						? record->ControlInstance->DataBindings.Add(
							targetProperty, std::move(ownedBindingSource),
							binding.SourceProperty, binding.Mode, binding.UpdateMode,
							std::move(converter), std::move(fallbackValue),
							std::move(targetNullValue), std::move(converterParameter),
							binding.StringFormat)
						: record->ControlInstance->DataBindings.Add(
							targetProperty, *bindingSource, binding.SourceProperty,
							binding.Mode, binding.UpdateMode, std::move(converter),
							std::move(fallbackValue), std::move(targetNullValue),
							std::move(converterParameter), binding.StringFormat);
					if (!installed)
					{
						if (outError) *outError = L"DataTemplate 绑定创建失败："
							+ record->Name + L"." + targetProperty;
						return {};
					}
				}
			}
			auto result = std::move(tree.ContentRoot);
			if (outError) outError->clear();
			return result;
		}

	private:
		std::shared_ptr<const DesignerModel::DesignDocument> _document;
		DesignerModel::DesignDataTemplate _definition;
		DesignerModel::DesignObjectResourceDictionary _visibleObjects;
		DesignerStyleSheet _visibleStyles;
		std::wstring _dataType;
		CuiRuntime::XamlMaterializationOptions _options;
		DesignerDataContextSchema _schema;
		mutable std::once_flag _compileOnce;
		mutable std::shared_ptr<const CuiRuntime::XamlCompiledDocument>
			_compiledDocument;
		mutable std::wstring _compileError;
	};

	class MaterializedControlTemplate final : public IControlTemplate,
		public std::enable_shared_from_this<MaterializedControlTemplate>
	{
	public:
		MaterializedControlTemplate(
			std::shared_ptr<const DesignerModel::DesignDocument> document,
			UIClass targetType,
			DesignerModel::DesignObjectResourceDictionary visibleObjects,
			DesignerStyleSheet visibleStyles,
			CuiRuntime::XamlMaterializationOptions options,
			std::wstring styleId,
			std::wstring templateResourceKey,
			DesignerComponentType targetComponentType,
			DesignerModel::DesignControlTemplate templateIdentityDefinition,
			std::shared_ptr<const DesignerModel::DesignDocument>
				templateIdentityDocument = {},
			bool templateIdentityUsesDocumentScope = false)
			: _document(std::move(document)),
			  _templateIdentityDocument(
				  templateIdentityDocument
					  ? std::move(templateIdentityDocument)
					  : _document),
			  _templateIdentityDefinition(
				  std::move(templateIdentityDefinition)),
			  _templateIdentityUsesDocumentScope(
				  templateIdentityUsesDocumentScope),
			  _targetType(targetType),
			  _visibleObjects(std::move(visibleObjects)),
			  _visibleStyles(std::move(visibleStyles)),
			  _options(std::move(options)),
			  _styleId(std::move(styleId)),
			  _templateResourceKey(std::move(templateResourceKey)),
			  _targetComponentType(std::move(targetComponentType)) {}

		UIClass TargetType() const noexcept override { return _targetType; }

		bool Apply(
			Control& owner,
			std::wstring* outError) const override
		{
			if (!_document)
			{
				if (outError) *outError =
					L"ControlTemplate 缺少源文档。";
				return false;
			}
			if (!IsUIClassAssignableFrom(
				_targetType, owner.Type()))
			{
				if (outError) *outError =
					L"ControlTemplate TargetType 与现有宿主不兼容。";
				return false;
			}
			if (!_targetComponentType.Empty())
			{
				const auto& actual = owner.GetDeclarativeTypeId();
				if (actual.NamespaceUri
						!= _targetComponentType.XamlNamespace
					|| actual.LocalName
						!= _targetComponentType.XamlName)
				{
					if (outError) *outError =
						L"ControlTemplate 目标组件与现有宿主不兼容。";
					return false;
				}
			}

			DesignerModel::DesignDocument runtimeDocument = *_document;
			runtimeDocument.Nodes.clear();
			runtimeDocument.Components = _visibleObjects.Components;
			runtimeDocument.ControlTemplates =
				_visibleObjects.ControlTemplates;
			runtimeDocument.DataTemplates =
				_visibleObjects.DataTemplates;
			runtimeDocument.ItemsPanelTemplates =
				_visibleObjects.ItemsPanelTemplates;
			runtimeDocument.GroupStyles = _visibleObjects.GroupStyles;
			runtimeDocument.StyleSheet = _visibleStyles;
			runtimeDocument.Window.Events.clear();
			runtimeDocument.Window.Bindings.clear();
			runtimeDocument.Window.CommandBindings.clear();
			runtimeDocument.Window.InputBindings.clear();
			runtimeDocument.NextStableId = 2;

			DesignerModel::DesignNode root;
			root.Id = 1;
			root.Name = L"__runtimeControlTemplateOwner";
			root.Type = owner.Type();
			root.ComponentType = _targetComponentType;
			root.Order = 0;
			if (!_styleId.empty())
				root.Properties.StyleResourceKey = _styleId;
			if (!_templateResourceKey.empty())
				root.Structure.ControlTemplate = _templateResourceKey;
			runtimeDocument.Nodes.push_back(std::move(root));

			auto options = _options;
			options.ExistingTemplateOwner = &owner;
			options.ExistingTemplateOwnerNodeId = 1;
			CuiRuntime::XamlObjectTree tree;
			std::wstring error;
			if (!CuiRuntime::XamlObjectMaterializer::Materialize(
				runtimeDocument, tree, options, &error))
			{
				if (outError) *outError = std::move(error);
				return false;
			}
			if (tree.ContentRoot
				|| !cui::framework::TemplateAccess::
					GetTemplateRoot(owner))
			{
				if (outError) *outError =
					L"ControlTemplate 未在现有宿主上生成唯一视觉根。";
				return false;
			}
			if (outError) outError->clear();
			return true;
		}

		bool Equals(
			const IControlTemplate& other) const noexcept override
		{
			const auto* typed =
				dynamic_cast<const MaterializedControlTemplate*>(&other);
			if (!typed
				|| _targetType != typed->_targetType
				|| _targetComponentType.RegistryKey()
					!= typed->_targetComponentType.RegistryKey()
				|| _styleId != typed->_styleId
				|| _templateResourceKey
					!= typed->_templateResourceKey
				|| _templateIdentityDefinition
					!= typed->_templateIdentityDefinition)
				return false;
			// A document owns multiple nested ResourceDictionary scopes.  Pointer
			// identity alone therefore cannot distinguish two local templates that
			// reuse the same key or implicit TargetType.  Global Theme/document
			// resources have one stable scope; local resources additionally compare
			// their fully resolved structural and Style scopes.
			if (_templateIdentityDocument.get()
				!= typed->_templateIdentityDocument.get()) return false;
			return (_templateIdentityUsesDocumentScope
					&& typed->_templateIdentityUsesDocumentScope)
				|| (_visibleObjects == typed->_visibleObjects
					&& _visibleStyles == typed->_visibleStyles);
		}

		std::unique_ptr<Control> Build(
			std::wstring* outError) const override
		{
			if (!_document)
			{
				if (outError) *outError = L"ControlTemplate 缺少源文档。";
				return {};
			}
			std::call_once(_buildCompileOnce, [this]
			{
				try
				{
					DesignerModel::DesignDocument runtimeDocument = *_document;
					runtimeDocument.Nodes.clear();
					runtimeDocument.Components = _visibleObjects.Components;
					runtimeDocument.ControlTemplates =
						_visibleObjects.ControlTemplates;
					runtimeDocument.DataTemplates = _visibleObjects.DataTemplates;
					runtimeDocument.ItemsPanelTemplates =
						_visibleObjects.ItemsPanelTemplates;
					runtimeDocument.GroupStyles = _visibleObjects.GroupStyles;
					runtimeDocument.StyleSheet = _visibleStyles;
					runtimeDocument.Window.Events.clear();
					runtimeDocument.Window.Bindings.clear();
					runtimeDocument.Window.CommandBindings.clear();
					runtimeDocument.Window.InputBindings.clear();
					runtimeDocument.NextStableId = 2;

					DesignerModel::DesignNode root;
					root.Id = 1;
					root.Name = L"__runtimeItemContainer";
					root.Type = _targetType;
					root.ComponentType = _targetComponentType;
					root.Order = 0;
					if (!_styleId.empty())
						root.Properties.StyleResourceKey = _styleId;
					// The owning ItemsControl already resolved the effective
					// template. Preserve that result in the cached factory plan.
					if (!_templateResourceKey.empty())
						root.Structure.ControlTemplate = _templateResourceKey;
					runtimeDocument.Nodes.push_back(std::move(root));

					auto compiled =
						std::make_shared<CuiRuntime::XamlCompiledDocument>();
					CuiRuntime::XamlDocumentCompilationOptions compileOptions;
					compileOptions.Theme = _options.Theme;
					compileOptions.UseFrameworkTheme =
						_options.UseFrameworkTheme;
					if (CuiRuntime::XamlDocumentCompiler::Compile(
						runtimeDocument, *compiled, compileOptions,
						&_buildCompileError))
						_buildCompiledDocument = std::move(compiled);
				}
				catch (const std::exception& exception)
				{
					_buildCompileError = L"ControlTemplate 编译失败："
						+ FromUtf8(exception.what());
				}
				catch (...)
				{
					_buildCompileError =
						L"ControlTemplate 编译失败：未知异常。";
				}
			});
			if (!_buildCompiledDocument)
			{
				if (outError) *outError = _buildCompileError.empty()
					? L"ControlTemplate 编译失败。" : _buildCompileError;
				return {};
			}

			CuiRuntime::XamlObjectTree tree;
			std::wstring error;
			auto materializationOptions = _options;
			materializationOptions.Theme = _buildCompiledDocument->Theme;
			materializationOptions.CompiledDocument =
				_buildCompiledDocument;
			if (!CuiRuntime::XamlObjectMaterializer::Materialize(
				_buildCompiledDocument->Document, tree,
				materializationOptions, &error))
			{
				if (outError) *outError = std::move(error);
				return {};
			}
			if (!tree.ContentRoot
				|| tree.ContentRoot->Type() != _targetType)
			{
				if (outError) *outError =
					L"ControlTemplate 未生成唯一且类型兼容的宿主。";
				return {};
			}
			if (outError) outError->clear();
			auto result = std::move(tree.ContentRoot);
			if (_styleId.empty())
			{
				// A generated container is built in a cached, isolated document and
				// then enters its ItemsControl's inheritance tree.  Preserve the
				// factory's already selected effective template in the internal
				// Template source slot so that the parent Theme style cannot replace
				// an author keyless ControlTemplate during that attachment.
				std::shared_ptr<const IControlTemplate> stableTemplate =
					shared_from_this();
				if (!cui::framework::DependencyPropertyAccess::SetValue(
					*result, Control::TemplateProperty(),
					BindingValue(ControlTemplateReference(
						std::move(stableTemplate))),
					DependencyPropertyValueSource::Template))
				{
					if (outError) *outError =
						L"生成项容器无法保留已解析的 ControlTemplate。";
					return {};
				}
				if (!cui::framework::TemplateAccess::GetTemplateRoot(*result)
					&& !result->ApplyTemplate())
				{
					if (outError) *outError = result->LastTemplateError().empty()
						? L"生成项容器无法重新应用 ControlTemplate。"
						: result->LastTemplateError();
					return {};
				}
			}
			return result;
		}

	private:
		std::shared_ptr<const DesignerModel::DesignDocument> _document;
		/**
		 * Semantic source of the selected template resource. Item containers
		 * keep the application document as their materialization context so
		 * DataTypes/DataTemplates remain visible, while a Theme-owned template
		 * must still compare equal to the same resource supplied later by the
		 * inherited Theme style.
		 */
		std::shared_ptr<const DesignerModel::DesignDocument>
			_templateIdentityDocument;
		DesignerModel::DesignControlTemplate _templateIdentityDefinition;
		bool _templateIdentityUsesDocumentScope = false;
		UIClass _targetType = UIClass::UI_Base;
		DesignerModel::DesignObjectResourceDictionary _visibleObjects;
		DesignerStyleSheet _visibleStyles;
		CuiRuntime::XamlMaterializationOptions _options;
		std::wstring _styleId;
		std::wstring _templateResourceKey;
		DesignerComponentType _targetComponentType;
		mutable std::once_flag _buildCompileOnce;
		mutable std::shared_ptr<const CuiRuntime::XamlCompiledDocument>
			_buildCompiledDocument;
		mutable std::wstring _buildCompileError;
	};

	static bool IsDocumentControlTemplateDefinition(
		const DesignerModel::DesignDocument& document,
		const DesignerModel::DesignControlTemplate* definition) noexcept
	{
		return definition && std::any_of(
			document.ControlTemplates.begin(),
			document.ControlTemplates.end(),
			[definition](const auto& candidate)
			{ return &candidate == definition; });
	}

	static std::wstring ImplicitControlTemplateStyleResourceKey(
		const DesignerModel::DesignControlTemplate& definition)
	{
		// Keyless ControlTemplates are author resources, but the runtime Style
		// engine needs a stable resource key to retain their Template setter when
		// an element is attached to a new inheritance parent.  Keep this key in a
		// private-use namespace so it cannot collide with an authored XAML key.
		return L"\xF8FF.CUI.ImplicitControlTemplate."
			+ (definition.TargetComponentType.Empty()
				? L"native." + std::to_wstring(
					static_cast<int>(definition.TargetType))
				: L"component."
					+ definition.TargetComponentType.RegistryKey());
	}

	static bool IsImplicitStyleForControlTemplate(
		const DesignerStyleRule& rule,
		const DesignerModel::DesignControlTemplate& definition)
	{
		if (!rule.Id.empty() || !rule.HasType
			|| rule.Type != definition.TargetType) return false;
		if (!definition.TargetComponentType.Empty())
			return !rule.ComponentType.Empty()
				&& rule.ComponentType.RegistryKey()
					== definition.TargetComponentType.RegistryKey();
		return rule.ComponentType.Empty();
	}

	static std::vector<const DesignerModel::DesignControlTemplate*>
	EffectiveImplicitControlTemplates(
		const std::vector<DesignerModel::DesignControlTemplate>& definitions)
	{
		std::vector<const DesignerModel::DesignControlTemplate*> result;
		std::unordered_set<std::wstring> identities;
		for (auto current = definitions.rbegin();
			current != definitions.rend(); ++current)
		{
			if (!current->IsImplicit()) continue;
			const auto identity = ImplicitControlTemplateStyleResourceKey(*current);
			if (identities.insert(identity).second)
				result.push_back(&*current);
		}
		std::reverse(result.begin(), result.end());
		return result;
	}

	static void ProjectImplicitControlTemplateStyles(
		const std::vector<DesignerModel::DesignControlTemplate>& definitions,
		DesignerStyleSheet& styleSheet)
	{
		for (const auto* definition :
			EffectiveImplicitControlTemplates(definitions))
		{
			if (!definition) continue;
			auto rule = std::find_if(
				styleSheet.Rules.begin(), styleSheet.Rules.end(),
				[&](const DesignerStyleRule& candidate)
				{
					return IsImplicitStyleForControlTemplate(
						candidate, *definition);
				});
			if (rule == styleSheet.Rules.end())
			{
				DesignerStyleRule projected;
				projected.HasType = true;
				projected.Type = definition->TargetType;
				projected.ComponentType = definition->TargetComponentType;
				if (projected.ComponentType.Empty())
					if (const auto* descriptor =
						CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(
							definition->TargetType))
						projected.XamlType = descriptor->TypeId;
				styleSheet.Rules.push_back(std::move(projected));
				rule = std::prev(styleSheet.Rules.end());
			}
			const bool alreadyDefinesTemplate = std::any_of(
				rule->Setters.begin(), rule->Setters.end(),
				[](const DesignerStyleSetter& setter)
				{
					return _wcsicmp(
						setter.PropertyName.c_str(), L"Template") == 0;
				});
			if (alreadyDefinesTemplate) continue;

			DesignerStyleSetter setter;
			setter.PropertyName = L"Template";
			setter.UsesResource = true;
			setter.ResourceKey =
				ImplicitControlTemplateStyleResourceKey(*definition);
			rule->Setters.push_back(std::move(setter));
		}
	}
}

std::shared_ptr<const DeclarativeTypeDescriptor>
CuiRuntime::XamlRuntimeSchema::CreateComponentTypeDescriptor(
	const DesignComponentDefinition& component,
	const DesignDocument& document,
	std::wstring* outError)
{
	std::shared_ptr<const DeclarativeTypeDescriptor> descriptor;
	if (!BuildComponentTypeDescriptorCore(
		component, document, descriptor, outError)) return {};
	auto rejectNativeCollision = [&](const std::wstring& name)
	{
		if (!FindNativeProperty(component.BaseType, name)) return false;
		if (outError) *outError = L"声明类型成员不能覆盖控件已有属性："
			+ name;
		return true;
	};
	for (const auto* property : descriptor->Properties())
		if (property && rejectNativeCollision(property->Name())) return {};
	for (const auto& event : descriptor->Events())
		if (rejectNativeCollision(event.Name)) return {};
	for (const auto& content : descriptor->ContentProperties())
		if (rejectNativeCollision(content.Name)) return {};
	return descriptor;
}

bool CuiRuntime::XamlRuntimeSchema::AttachComponentContract(
	Control& control,
	const DesignComponentDefinition& component,
	const DesignDocument& document,
	std::wstring* outError)
{
	auto descriptor = CreateComponentTypeDescriptor(
		component, document, outError);
	if (!descriptor) return false;
	std::wstring attachError;
	if (!cui::framework::XamlAccess::SetTypeDescriptor(
		control, std::move(descriptor), &attachError))
	{
		if (outError) *outError = L"组件 Schema 无法绑定到基类控件："
			+ attachError;
		return false;
	}
	if (outError) outError->clear();
	return true;
}

bool CuiRuntime::XamlDocumentCompiler::Compile(
	const DesignDocument& source,
	XamlCompiledDocument& output,
	const XamlDocumentCompilationOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	if (outDiagnostic)
	{
		*outDiagnostic = {};
		outDiagnostic->Stage = XamlDiagnosticStage::Normalize;
	}
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = message;
		if (outDiagnostic)
		{
			outDiagnostic->Message = std::move(message);
			outDiagnostic->Apply(source.Sources.Root);
		}
		return false;
	};
	try
	{
		auto theme = options.Theme;
		if (!theme && options.UseFrameworkTheme)
		{
			std::wstring themeError;
			theme = XamlFrameworkTheme::DefaultDocument(&themeError);
			if (!theme)
				return fail(themeError.empty()
					? L"无法加载框架 Generic.xaml。" : std::move(themeError));
		}

		auto canonicalSource = source;
		std::wstring error;
		if (!canonicalSource.ValidateCommandTargetReferences(&error))
			return fail(std::move(error));
		if (!DesignDataResourceUtils::ValidateAndCanonicalize(
			canonicalSource, &error, theme.get()))
			return fail(std::move(error));

		DesignDocument expandedDocument = canonicalSource;
		for (int pass = 0; pass < 64; ++pass)
		{
			const auto previousCount = expandedDocument.Nodes.size();
			DesignDocument componentPass;
			if (!ExpandComponentTemplates(
				expandedDocument, theme.get(), componentPass, &error))
				return fail(std::move(error));
			ProjectDirectItemContainerStyles(componentPass, theme.get());
			DesignDocument controlTemplatePass;
			if (!ExpandControlTemplates(
				componentPass, theme.get(), controlTemplatePass, &error))
				return fail(std::move(error));
			expandedDocument = std::move(controlTemplatePass);
			if (expandedDocument.Nodes.size() == previousCount) break;
			if (pass == 63)
				return fail(
					L"模板展开层级超过 64；可能存在间接递归。");
		}
		XamlCompiledDocument candidate;
		candidate.Document = std::move(expandedDocument);
		candidate.Theme = std::move(theme);
		output = std::move(candidate);
		if (outError) outError->clear();
		if (outDiagnostic) *outDiagnostic = {};
		return true;
	}
	catch (const std::exception& exception)
	{
		return fail(L"XAML 文档编译失败：" + FromUtf8(exception.what()));
	}
	catch (...)
	{
		return fail(L"XAML 文档编译失败：未知错误。");
	}
}

bool CuiRuntime::XamlObjectMaterializer::Materialize(
	const DesignDocument& document,
	XamlObjectTree& output,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	return Materialize(
		document, output, XamlMaterializationOptions{}, outError, outDiagnostic);
}

std::vector<std::pair<std::wstring, BindingValue>>
CuiRuntime::XamlObjectMaterializer::BuildControlTemplateStyleResources(
	std::shared_ptr<const DesignDocument> document,
	const XamlMaterializationOptions& options)
{
	std::vector<std::pair<std::wstring, BindingValue>> result;
	if (!document) return result;
	DesignObjectResourceDictionary visibleObjects;
	visibleObjects.Components = document->Components;
	visibleObjects.ControlTemplates = document->ControlTemplates;
	visibleObjects.DataTemplates = document->DataTemplates;
	visibleObjects.ItemsPanelTemplates = document->ItemsPanelTemplates;
	visibleObjects.GroupStyles = document->GroupStyles;
	auto nestedOptions = options;
	nestedOptions.ExistingTemplateOwner = nullptr;
	nestedOptions.ExistingTemplateOwnerNodeId = 0;
	result.reserve(document->ControlTemplates.size());
	for (const auto& definition : document->ControlTemplates)
	{
		if (definition.Key.empty()) continue;
		result.emplace_back(
			definition.Key,
			BindingValue(ControlTemplateReference(
				std::make_shared<MaterializedControlTemplate>(
					document, definition.TargetType, visibleObjects,
					document->StyleSheet, nestedOptions, std::wstring{},
					definition.Key, definition.TargetComponentType,
					definition,
					std::shared_ptr<const DesignDocument>{}, true))));
	}
	for (const auto* definition :
		EffectiveImplicitControlTemplates(document->ControlTemplates))
	{
		if (!definition) continue;
		result.emplace_back(
			ImplicitControlTemplateStyleResourceKey(*definition),
			BindingValue(ControlTemplateReference(
				std::make_shared<MaterializedControlTemplate>(
					document, definition->TargetType, visibleObjects,
					document->StyleSheet, nestedOptions, std::wstring{},
					std::wstring{}, definition->TargetComponentType,
					*definition,
					std::shared_ptr<const DesignDocument>{}, true))));
	}
	return result;
}

std::vector<std::pair<std::wstring, BindingValue>>
CuiRuntime::XamlObjectMaterializer::BuildStructuralStyleResources(
	std::shared_ptr<const DesignDocument> document,
	const XamlMaterializationOptions& options)
{
	if (!document) return {};
	auto result =
		DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
			document->ItemsPanelTemplates);
	auto controlTemplates =
		BuildControlTemplateStyleResources(document, options);
	result.insert(
		result.end(),
		std::make_move_iterator(controlTemplates.begin()),
		std::make_move_iterator(controlTemplates.end()));
	return result;
}

bool CuiRuntime::XamlObjectMaterializer::Materialize(
	const DesignDocument& sourceDocument,
	XamlObjectTree& output,
	const XamlMaterializationOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	if (outDiagnostic)
	{
		*outDiagnostic = {};
		outDiagnostic->Stage = XamlDiagnosticStage::Materialize;
	}
	bool completed = false;
	XamlSourceSpan activeSpan;
	std::wstring activeQName;
	std::wstring activeMember;
	auto setDiagnosticContext = [&](const DesignNode* node,
		const std::wstring& member = std::wstring{})
	{
		activeSpan = {};
		activeQName.clear();
		activeMember = member;
		if (!node) return;
		activeQName = !node->ComponentType.Empty()
			? node->ComponentType.XamlName
			: node->XamlType.Valid() ? node->XamlType.LocalName
				: DesignerStyleSheetUtils::UIClassName(node->Type);
		if (!member.empty())
			if (const auto* span = node->Source.FindMember(member))
				activeSpan = *span;
		if (!activeSpan.Valid()) activeSpan = node->Source.Element;
	};
	ExitAction diagnosticExit([&]
	{
		if (completed || !outDiagnostic) return;
		outDiagnostic->Stage = XamlDiagnosticStage::Materialize;
		outDiagnostic->Message = outError ? *outError : std::wstring{};
		outDiagnostic->QName = activeQName;
		outDiagnostic->Member = activeMember;
		if (!activeSpan.Valid())
		{
			std::wstring symbol;
			if (const auto* span = sourceDocument.Sources.FindMentionedSymbol(
				outDiagnostic->Message, &symbol))
			{
				activeSpan = *span;
				if (outDiagnostic->Member.empty())
					outDiagnostic->Member = std::move(symbol);
			}
			else activeSpan = sourceDocument.Sources.Root;
		}
		outDiagnostic->Apply(activeSpan);
	});
	try
	{
		XamlCompiledDocument compiledStorage;
		const XamlCompiledDocument* compiledPlan =
			options.CompiledDocument.get();
		if (!compiledPlan)
		{
			XamlDocumentCompilationOptions compilationOptions;
			compilationOptions.Theme = options.Theme;
			compilationOptions.UseFrameworkTheme = options.UseFrameworkTheme;
			if (!XamlDocumentCompiler::Compile(
				sourceDocument, compiledStorage, compilationOptions, outError))
				return false;
			compiledPlan = &compiledStorage;
		}
		const auto& compiled = *compiledPlan;
		const DesignDocument& document = compiled.Document;
		const auto templateDocument = options.CompiledDocument
			? std::shared_ptr<const DesignDocument>(
				options.CompiledDocument, &options.CompiledDocument->Document)
			: std::make_shared<const DesignDocument>(document);
		const auto createBaseControl = options.ControlFactory
			? options.ControlFactory
			: std::function<std::unique_ptr<Control>(UIClass)>(
				XamlRuntimeSchema::CreateNativeControl);
		auto createXamlControl = [&](UIClass type) -> std::unique_ptr<Control>
		{
			auto control = createBaseControl(type);
			if (!control) return nullptr;
			// A native constructor initializes its behavior host through ordinary
			// property setters, but those values are implementation defaults rather
			// than authored XAML locals.  Keeping them in the Local precedence slot
			// would suppress Style, Theme, inherited and metadata defaults.  A XAML
			// instance therefore starts with no constructor-authored Local values;
			// the materializer installs only values actually present in markup.
			(void)control->ClearPropertyValues();
			return control;
		};
		auto schemaContext = options.SchemaContext
			? options.SchemaContext : std::make_shared<XamlSchemaContext>();
		auto buildRuntimeStylePropertySchema =
			[schemaContext](const DesignDocument& schemaDocument,
				const DesignerStyleRule& rule,
				XamlTypePropertySchema& schema,
				std::wstring* error) -> bool
			{
				const auto* component = rule.ComponentType.Empty()
					? nullptr : schemaDocument.FindComponent(rule.ComponentType);
				if (!rule.ComponentType.Empty() && !component)
				{
					if (error) *error = L"样式 TargetType 组件不存在。";
					return false;
				}
				if (!XamlRuntimeSchema::BuildPropertySchema(
					rule.HasType ? rule.Type : UIClass::UI_Base,
					component, schemaDocument, schema, error))
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
				schema.Properties.insert(
					schema.Properties.begin(), properties.begin(), properties.end());
				if (error) error->clear();
				return true;
			};
		const DesignerStyleSheetUtils::RulePropertySchemaResolver
			documentStyleSchemaResolver =
			[&](const DesignerStyleRule& rule,
				XamlTypePropertySchema& schema,
				std::wstring* error)
			{
				return buildRuntimeStylePropertySchema(
					document, rule, schema, error);
			};
		auto nestedOptions = options;
		nestedOptions.SchemaContext = schemaContext;
		nestedOptions.Theme = compiled.Theme;
		nestedOptions.CompiledDocument.reset();
		nestedOptions.ExistingTemplateOwner = nullptr;
		nestedOptions.ExistingTemplateOwnerNodeId = 0;
		std::unordered_map<const DesignComponentDefinition*,
			std::shared_ptr<const DeclarativeTypeDescriptor>> componentSchemas;
		auto resolveComponentSchema = [&]
			(const DesignComponentDefinition* component,
			 std::shared_ptr<const DeclarativeTypeDescriptor>& descriptor,
			 std::wstring* error) -> bool
		{
			if (!component)
			{
				if (error) *error = L"组件定义不存在。";
				return false;
			}
			const auto found = componentSchemas.find(component);
			if (found != componentSchemas.end())
			{
				descriptor = found->second;
				if (error) error->clear();
				return true;
			}
			if (!BuildComponentTypeDescriptorCore(
				*component, document, descriptor, error)) return false;
			descriptor = schemaContext->GetOrAdd(
				std::move(descriptor), error);
			if (!descriptor) return false;
			componentSchemas.emplace(component, descriptor);
			return true;
		};
		const DesignDocumentControlPool::Factory createControl =
			[&](const DesignNode& node) -> std::unique_ptr<Control>
			{
				if (!node.ComponentType.Empty())
				{
					const auto* component = document.FindComponent(
						document.Nodes, node, node.ComponentType);
					if (!component || component->BaseType != node.Type)
						return nullptr;
					return createXamlControl(component->BaseType);
				}
				return createXamlControl(node.Type);
			};
		if (!DesignerDataContextSchemaUtils::Validate(document.DataContextSchema, outError))
			return false;
		if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
			document.StyleSheet,
			documentStyleSchemaResolver,
			outError,
			document.ResourceBasePath,
			document.Resources))
			return false;
		DesignDocumentGraph documentGraph;
		if (!DesignDocumentGraph::Build(
			document, documentGraph, outError))
			return false;
		if ((options.ExistingTemplateOwner == nullptr)
			!= (options.ExistingTemplateOwnerNodeId == 0))
		{
			if (outError) *outError =
				L"ApplyTemplate 借用宿主参数不完整。";
			return false;
		}
		if (options.ExistingTemplateOwner)
		{
			const auto borrowed = std::find_if(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& node)
				{
					return node.Id == options.ExistingTemplateOwnerNodeId;
				});
			if (borrowed == document.Nodes.end()
				|| borrowed->TemplateState.Generated
				|| borrowed->Type
					!= options.ExistingTemplateOwner->Type())
			{
				if (outError) *outError =
					L"ApplyTemplate 借用宿主与合成根不兼容。";
				return false;
			}
		}
		DesignDocumentControlPool controlPool;
		if (!DesignDocumentControlPool::Build(
			document,
			documentGraph,
			createControl,
			controlPool,
			outError,
			[&](const DesignNode& node) -> Control*
			{
				return options.ExistingTemplateOwner
					&& node.Id == options.ExistingTemplateOwnerNodeId
					? options.ExistingTemplateOwner : nullptr;
			}))
			return false;

		// The staging owner exists only to hold the content tree transactionally.
		// It must not project Window dimensions into a second, fake layout root.
		Panel stagingRoot;
		XamlObjectTree candidate;
		auto dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(dataContextSchema);

		struct Pending
		{
			const DesignNode* source = nullptr;
			std::wstring name;
			int id = 0;
			UIClass type = UIClass::UI_Base;
			RuntimeTypeId xamlType;
			std::wstring parent;
			int order = -1;
			bool locked = false;
			DesignNodeProperties properties;
			DesignNodeStructure structure;
			/** Transient serialization projection for collection materializers. */
			DesignValue extra;
			DesignEventHandlerMap events;
			DesignBindingMap bindings;
			std::vector<DesignCommandBinding> commandBindings;
			std::vector<DesignInputBinding> inputBindings;
			DesignerStyleSheet localResources;
			DesignObjectResourceDictionary localObjectResources;
			std::map<std::wstring, std::wstring> templateBindings;
			std::map<std::wstring, std::wstring> templateEventBindings;
			std::wstring templateOwner;
			std::wstring templatePartName;
			std::wstring contentOwner;
			std::wstring presentedContent;
			std::wstring templateContentSource;
			bool templateGenerated = false;
			bool templateResourceFromTheme = false;
			bool styleResourceFromTheme = false;
			bool styleResourceIsAutomatic = false;
			bool controlTemplateRoot = false;
			bool borrowedTemplateOwner = false;
			DesignerComponentType componentType;
		};
		std::vector<Pending> items;
		items.reserve(document.Nodes.size());

		for (const auto& resolved : documentGraph.Nodes())
		{
			const auto& node = document.Nodes[resolved.SourceIndex];
			Pending p;
			p.source = &node;
			p.name = node.Name;
			p.id = node.Id;
			p.type = node.Type;
			p.xamlType = node.XamlType;
			p.parent = resolved.ParentKey;
			p.order = node.Order;
			p.locked = node.Locked;
			p.properties = node.Properties;
			p.structure = node.Structure;
			p.extra = EncodeDesignNodeStructure(node.Type, node.Structure);
			p.events = node.Events;
			p.bindings = node.Bindings;
			p.commandBindings = node.CommandBindings;
			p.inputBindings = node.InputBindings;
			p.localResources = node.LocalResources;
			p.localObjectResources = node.LocalObjectResources;
			p.templateBindings = node.TemplateBindings;
			p.templateEventBindings = node.TemplateEventBindings;
			p.presentedContent = node.PresentedComponentContent;
			p.templateContentSource = node.TemplateContentSource;
			p.templateGenerated = node.TemplateState.Generated;
			p.templateResourceFromTheme =
				node.TemplateState.Generated
				&& node.TemplateState.ResourceScopeFromTheme;
			p.styleResourceFromTheme =
				node.TemplateState.StyleResourceScopeFromTheme;
			p.styleResourceIsAutomatic =
				node.TemplateState.StyleResourceIsAutomatic;
			p.templateOwner = node.TemplateState.Owner;
			p.templatePartName = node.TemplateState.PartName;
			p.contentOwner = node.TemplateState.ContentOwner;
			p.controlTemplateRoot = node.TemplateState.ControlTemplateRoot;
			p.borrowedTemplateOwner = options.ExistingTemplateOwner
				&& node.Id == options.ExistingTemplateOwnerNodeId;
			p.componentType = node.ComponentType;
			items.push_back(std::move(p));
		}

		auto projectSchema = [&](const std::wstring& prefix)
		{
			if (prefix.empty()) return dataContextSchema;
			DesignerDataContextSchema result;
			const auto normalizedPrefix =
				DesignerDataContextSchemaUtils::NormalizePath(prefix);
			const auto childPrefix = normalizedPrefix + L".";
			for (const auto& property : dataContextSchema)
			{
				const auto path = DesignerDataContextSchemaUtils::NormalizePath(
					property.Path);
				if (!path.starts_with(childPrefix)) continue;
				auto projected = property;
				projected.Path = path.substr(normalizedPrefix.size() + 1);
				result.push_back(std::move(projected));
			}
			DesignerDataContextSchemaUtils::Canonicalize(result);
			return result;
		};
		std::unordered_map<std::wstring, const Pending*> pendingByName;
		for (const auto& item : items) pendingByName.emplace(item.name, &item);
		auto findScopedResource = [&](const Pending& origin,
			const std::wstring& key) -> const DesignerStyleResource*
		{
			const Pending* scope = &origin;
			while (scope)
			{
				const auto found = std::find_if(
					scope->localResources.Resources.rbegin(),
					scope->localResources.Resources.rend(),
					[&](const auto& resource)
					{ return resource.Key == key; });
				if (found != scope->localResources.Resources.rend()) return &*found;
				if (scope->parent.empty()) break;
				const auto parent = pendingByName.find(scope->parent);
				if (parent != pendingByName.end())
					scope = parent->second;
				else scope = nullptr;
			}
			const auto& resourceDocument =
				origin.templateResourceFromTheme && compiled.Theme
				? *compiled.Theme : document;
			const auto found = std::find_if(
				resourceDocument.StyleSheet.Resources.rbegin(),
				resourceDocument.StyleSheet.Resources.rend(),
				[&](const auto& resource)
				{ return resource.Key == key; });
			return found == resourceDocument.StyleSheet.Resources.rend()
				? nullptr : &*found;
		};
		auto appendStyleScope = [](DesignerStyleSheet& target,
			const DesignerStyleSheet& source)
		{
			DesignerStyleSheetUtils::AppendLexicalScope(target, source);
		};
		auto visibleStyleScope = [&](const Pending& origin)
		{
			DesignerStyleSheet result = document.StyleSheet;
			std::vector<const Pending*> route;
			for (const Pending* scope = &origin; scope;)
			{
				route.push_back(scope);
				if (scope->parent.empty()) break;
				const auto parent = pendingByName.find(scope->parent);
				if (parent != pendingByName.end()) scope = parent->second;
				else scope = nullptr;
			}
			for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
				appendStyleScope(result, (*scope)->localResources);
			return result;
		};
		auto visibleItemsPanelTemplates = [&](const Pending& origin)
		{
			auto result = document.ItemsPanelTemplates;
			std::vector<const Pending*> route;
			for (const Pending* scope = &origin; scope;)
			{
				route.push_back(scope);
				if (scope->parent.empty()) break;
				const auto parent = pendingByName.find(scope->parent);
				scope = parent == pendingByName.end()
					? nullptr : parent->second;
			}
			for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
			{
				for (const auto& definition :
					(*scope)->localObjectResources.ItemsPanelTemplates)
				{
					result.erase(std::remove_if(
						result.begin(), result.end(),
						[&](const auto& current)
						{ return current.Key == definition.Key; }),
						result.end());
					result.push_back(definition);
				}
			}
			return result;
		};
		auto prepareLocalStyleSheet = [&](const Pending& origin,
			DesignerStyleSheet& output) -> bool
		{
			const auto visible = visibleStyleScope(origin);
			return DesignerStyleSheetUtils::PrepareLocalRuntimeStyleSheet(
				origin.localResources, visible, output, outError);
		};
		std::unordered_map<std::wstring, std::optional<std::wstring>> contextPrefixes;
		std::unordered_map<std::wstring, std::optional<std::wstring>> inheritedPrefixes;
		std::unordered_set<std::wstring> resolvingContexts;
		std::function<std::optional<std::wstring>(const Pending&)> resolveContext;
		resolveContext = [&](const Pending& item) -> std::optional<std::wstring>
		{
			if (const auto found = contextPrefixes.find(item.name);
				found != contextPrefixes.end()) return found->second;
			if (!resolvingContexts.insert(item.name).second) return std::nullopt;
			std::optional<std::wstring> inherited = std::wstring{};
			if (!item.parent.empty())
			{
				const auto parent = pendingByName.find(item.parent);
				inherited = parent == pendingByName.end()
					? std::nullopt : resolveContext(*parent->second);
			}
			inheritedPrefixes[item.name] = inherited;
			auto effective = inherited;
			for (const auto& [target, binding] : item.bindings)
			{
				if (target != L"DataContext") continue;
				if (binding.IsMultiBinding())
				{
					effective.reset();
					break;
				}
				const bool explicitSource = !binding.ElementName.empty()
					|| binding.RelativeSource
						!= DesignerBindingRelativeSource::None;
				if (!explicitSource && inherited)
				{
					const auto path = DesignerBindingUtils::Trim(
						binding.SourceProperty);
					effective = inherited->empty()
						? path : *inherited + L"." + path;
				}
				else effective.reset();
				break;
			}
			resolvingContexts.erase(item.name);
			contextPrefixes[item.name] = effective;
			return effective;
		};
		for (const auto& item : items) (void)resolveContext(item);
		std::unordered_map<std::wstring, DesignerDataContextSchema> contextSchemas;
		std::unordered_map<std::wstring, DesignerDataContextSchema> inheritedSchemas;
		for (const auto& item : items)
		{
			if (const auto prefix = contextPrefixes[item.name])
				contextSchemas.emplace(item.name, projectSchema(*prefix));
			if (const auto prefix = inheritedPrefixes[item.name])
				inheritedSchemas.emplace(item.name, projectSchema(*prefix));
		}
		std::unordered_map<std::wstring, std::shared_ptr<DesignerControl>> dcOf;
		dcOf.reserve(items.size());
		std::unordered_map<std::wstring, Control*> instOf;
		instOf.reserve(items.size());
		auto resolveItemType = [&](const Pending& item)
			-> const DesignDataTypeDefinition*
		{
			if (!item.structure.ItemsSourceResource.empty())
			{
				std::wstring key = item.structure.ItemsSourceResource;
				std::unordered_set<std::wstring> visited;
				while (visited.insert(key).second)
				{
					if (const auto* list = document.FindDataList(key))
						return document.FindDataType(list->ItemType);
					const auto* view = document.FindCollectionView(key);
					if (!view) break;
					if (!view->SourceBindingPath.empty())
					{
						const auto* property = DesignerDataContextSchemaUtils::Find(
							dataContextSchema, view->SourceBindingPath);
						return property
							? document.FindDataType(property->ItemType) : nullptr;
					}
					key = view->SourceResource;
				}
			}
			if (const auto found = item.bindings.find(L"ItemsSource");
				found != item.bindings.end())
			{
				const auto& binding = found->second;
				if (!binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.IsMultiBinding()) return nullptr;
				const auto& path = binding.SourceProperty;
				const auto scoped = contextSchemas.find(item.name);
				const auto& schema = scoped == contextSchemas.end()
					? dataContextSchema : scoped->second;
				const auto* property = DesignerDataContextSchemaUtils::Find(
					schema, path);
				return property ? document.FindDataType(property->ItemType) : nullptr;
			}
			return nullptr;
		};
		auto resolveContentType = [&](const Pending& item)
			-> const DesignDataTypeDefinition*
		{
			const auto found = item.bindings.find(L"Content");
			if (found == item.bindings.end()) return nullptr;
			const auto& binding = found->second;
			if (!binding.ElementName.empty()
				|| binding.RelativeSource != DesignerBindingRelativeSource::None
				|| binding.IsMultiBinding()) return nullptr;
			const auto& path = binding.SourceProperty;
			const auto scoped = contextSchemas.find(item.name);
			const auto& schema = scoped == contextSchemas.end()
				? dataContextSchema : scoped->second;
			const auto* property = DesignerDataContextSchemaUtils::Find(
				schema, path);
			return property && property->ObjectKind
				== DesignerDataObjectKind::BindingSource
				? document.FindDataType(property->DataType) : nullptr;
		};
		auto resolveHeaderType = [&](const Pending& item)
			-> const DesignDataTypeDefinition*
		{
			const auto found = item.bindings.find(L"Header");
			if (found == item.bindings.end()) return nullptr;
			const auto& binding = found->second;
			if (!binding.ElementName.empty()
				|| binding.RelativeSource != DesignerBindingRelativeSource::None
				|| binding.IsMultiBinding()) return nullptr;
			const auto& path = binding.SourceProperty;
			const auto scoped = contextSchemas.find(item.name);
			const auto& schema = scoped == contextSchemas.end()
				? dataContextSchema : scoped->second;
			const auto* property = DesignerDataContextSchemaUtils::Find(
				schema, path);
			return property && property->ObjectKind
				== DesignerDataObjectKind::BindingSource
				? document.FindDataType(property->DataType) : nullptr;
		};
		for (auto& it : items)
		{
			setDiagnosticContext(it.source);
			Control* c = controlPool.FindById(it.id);
			if (!c) return false;
			if (!it.borrowedTemplateOwner && !it.componentType.Empty())
			{
				const auto* component = it.source
					? document.FindComponent(document.Nodes, *it.source, it.componentType)
					: document.FindComponent(it.componentType);
				std::wstring componentError;
				std::shared_ptr<const DeclarativeTypeDescriptor> descriptor;
				if (!resolveComponentSchema(
					component, descriptor, &componentError)
					|| !cui::framework::XamlAccess::SetTypeDescriptor(
						*c, std::move(descriptor), &componentError))
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的组件契约无效：" + componentError;
					return false;
				}
			}
			else if (!it.borrowedTemplateOwner)
			{
				const BuiltInXamlTypeDescriptor* descriptor = it.xamlType.Valid()
					? XamlRuntimeSchema::FindBuiltInType(
						it.xamlType.NamespaceUri, it.xamlType.LocalName)
					: XamlRuntimeSchema::DefaultTypeFor(it.type);
				std::wstring schemaError;
				if (!descriptor || descriptor->NativeType != it.type
					|| !XamlRuntimeSchema::AttachBuiltInType(
						*c, *descriptor, *schemaContext, &schemaError))
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的内置 XAML 类型身份无效："
						+ (schemaError.empty() ? L"XAML 类型与 native behavior 不匹配。"
							: schemaError);
					return false;
				}
				it.xamlType = descriptor->TypeId;
			}
			auto dc = std::make_shared<DesignerControl>(
				c, it.name, it.type, nullptr, it.id);
			dc->XamlType = it.xamlType;
			if (!it.localResources.Empty())
				dc->LocalResources =
					std::make_shared<DesignerStyleSheet>(it.localResources);
			if (!it.localObjectResources.Empty())
				dc->LocalObjectResources =
					std::make_shared<DesignObjectResourceDictionary>(
						it.localObjectResources);
			const bool hasLocalStructuralResources =
				!it.localObjectResources.ControlTemplates.empty()
				|| !it.localObjectResources.ItemsPanelTemplates.empty();
			if (!it.borrowedTemplateOwner
				&& (!it.localResources.Empty()
					|| hasLocalStructuralResources))
			{
				DesignerStyleSheet runtimeSource;
				if (!prepareLocalStyleSheet(it, runtimeSource)) return false;
				std::shared_ptr<ControlStyleSheet> localResources;
				auto structuralResources =
					DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
						visibleItemsPanelTemplates(it));
				auto visibleObjects = it.source
					? document.VisibleObjectResources(
						document.Nodes, *it.source)
					: DesignObjectResourceDictionary{};
				const auto visibleStyles = visibleStyleScope(it);
				for (const auto& definition :
					visibleObjects.ControlTemplates)
				{
					if (definition.Key.empty()) continue;
					const auto* selectedDefinition = it.source
						? document.FindControlTemplate(
							document.Nodes, *it.source, definition.Key)
						: document.FindControlTemplate(definition.Key);
					const bool documentTemplate =
						IsDocumentControlTemplateDefinition(
							document, selectedDefinition);
					structuralResources.emplace_back(
						definition.Key,
						BindingValue(ControlTemplateReference(
							std::make_shared<MaterializedControlTemplate>(
								templateDocument, definition.TargetType,
								visibleObjects, visibleStyles, nestedOptions,
								std::wstring{}, definition.Key,
								definition.TargetComponentType, definition,
								std::shared_ptr<const DesignDocument>{},
								documentTemplate))));
				}
				for (const auto* definition :
					EffectiveImplicitControlTemplates(
						visibleObjects.ControlTemplates))
				{
					if (!definition) continue;
					const auto* selectedDefinition = it.source
						? (definition->TargetComponentType.Empty()
							? document.FindImplicitControlTemplate(
								document.Nodes, *it.source,
								definition->TargetType)
							: document.FindImplicitControlTemplate(
								document.Nodes, *it.source,
								definition->TargetComponentType))
						: (definition->TargetComponentType.Empty()
							? document.FindImplicitControlTemplate(
								definition->TargetType)
							: document.FindImplicitControlTemplate(
								definition->TargetComponentType));
					const bool documentTemplate =
						IsDocumentControlTemplateDefinition(
							document, selectedDefinition);
					structuralResources.emplace_back(
						ImplicitControlTemplateStyleResourceKey(*definition),
						BindingValue(ControlTemplateReference(
							std::make_shared<MaterializedControlTemplate>(
								templateDocument, definition->TargetType,
								visibleObjects, visibleStyles, nestedOptions,
								std::wstring{}, std::wstring{},
								definition->TargetComponentType, *definition,
								std::shared_ptr<const DesignDocument>{},
								documentTemplate))));
				}
				ProjectImplicitControlTemplateStyles(
					it.localObjectResources.ControlTemplates, runtimeSource);
				if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
					runtimeSource, localResources, outError,
					document.ResourceBasePath, document.Resources,
					structuralResources, documentStyleSchemaResolver)
					|| !cui::framework::StyleAccess::SetResources(
						*c, std::move(localResources)))
				{
					if (outError && outError->empty())
						*outError = L"控件 " + it.name
							+ L" 的局部 ResourceDictionary 无法安装。";
					return false;
				}
			}
			dc->ComponentType = it.componentType;
			dc->ComponentContentProperty =
				it.source ? it.source->ComponentContentProperty : std::wstring{};
			if (!it.componentType.Empty())
				if (const auto* component = it.source
					? document.FindComponent(document.Nodes, *it.source, it.componentType)
					: document.FindComponent(it.componentType))
				{
					dc->ComponentEvents = component->Events;
					dc->ComponentContentProperties = component->ContentProperties;
				}
			dc->IsLocked = it.locked;
			auto designString = [&](const wchar_t* key, const std::wstring& value)
			{
				if (!value.empty()) dc->DesignStrings[key] = value;
			};
			designString(L"controlTemplate", it.structure.ControlTemplate);
			designString(L"itemTemplate", it.structure.ItemTemplate);
			designString(L"itemsSourceResource", it.structure.ItemsSourceResource);
			designString(L"itemContainerStyle", it.structure.ItemContainerStyle);
			designString(L"itemsPanel", it.structure.ItemsPanel);
			designString(L"groupStyle", it.structure.GroupStyle);
			designString(L"contentTemplate", it.structure.ContentTemplate);
			designString(L"headerTemplate", it.structure.HeaderTemplate);
			if (it.structure.ChildRole == DesignNodeChildRole::Header)
				dc->DesignStrings[L"headeredRegion"] = L"header";
			dcOf[it.name] = dc;
			instOf[it.name] = c;
		}

		// Publish the effective XAML ControlTemplate as a real dependency-
		// property value before the already compiled initial visual subtree is
		// attached. SetControlTemplateRoot can then adopt that subtree as the
		// first applied instance without a detach/rebuild cycle.
		for (const auto& it : items)
		{
			if (it.borrowedTemplateOwner || !it.source
				|| it.source->TemplateState.
					AppliedControlTemplate.empty()) continue;
			const bool fromTheme = it.source->TemplateState.
				AppliedControlTemplateFromTheme;
			const auto sourceDocument = fromTheme
				? compiled.Theme : templateDocument;
			if (!sourceDocument)
			{
				if (outError) *outError =
					L"ControlTemplate 运行期来源已丢失：" + it.name;
				return false;
			}
			const auto& resourceKey = it.source->TemplateState.
				AppliedControlTemplateResource;
			const DesignControlTemplate* definition = nullptr;
			if (!resourceKey.empty())
				definition = fromTheme
					? sourceDocument->FindControlTemplate(resourceKey)
					: sourceDocument->FindControlTemplate(
						document.Nodes, *it.source, resourceKey);
			else definition = fromTheme
				? (it.componentType.Empty()
					? sourceDocument->FindImplicitControlTemplate(it.type)
					: sourceDocument->FindImplicitControlTemplate(
						it.componentType))
				: (it.componentType.Empty()
					? sourceDocument->FindImplicitControlTemplate(
						document.Nodes, *it.source, it.type)
					: sourceDocument->FindImplicitControlTemplate(
						document.Nodes, *it.source, it.componentType));
			if (!definition)
			{
				if (outError) *outError =
					L"ControlTemplate 运行期引用无法解析：" + it.name;
				return false;
			}

			DesignObjectResourceDictionary visibleObjects;
			DesignerStyleSheet visibleStyles;
			if (fromTheme)
			{
				visibleObjects.Components = sourceDocument->Components;
				visibleObjects.ControlTemplates =
					sourceDocument->ControlTemplates;
				visibleObjects.DataTemplates =
					sourceDocument->DataTemplates;
				visibleObjects.ItemsPanelTemplates =
					sourceDocument->ItemsPanelTemplates;
				visibleObjects.GroupStyles = sourceDocument->GroupStyles;
				visibleStyles = sourceDocument->StyleSheet;
			}
			else
			{
				visibleObjects = document.VisibleObjectResources(
					document.Nodes, *it.source);
				visibleStyles = visibleStyleScope(it);
			}
			const bool documentTemplate = fromTheme
				|| IsDocumentControlTemplateDefinition(document, definition);
			auto reference = ControlTemplateReference(
				std::make_shared<MaterializedControlTemplate>(
					sourceDocument, definition->TargetType,
					std::move(visibleObjects),
					std::move(visibleStyles), nestedOptions, std::wstring{},
					resourceKey, definition->TargetComponentType, *definition,
					std::shared_ptr<const DesignDocument>{}, documentTemplate));
			const auto valueSource =
				!it.structure.ControlTemplate.empty()
				? (it.templateGenerated
					? DependencyPropertyValueSource::Template
					: DependencyPropertyValueSource::Local)
				// A keyed Style remains an explicit Style even when its resource
				// dictionary is the framework Theme. Publish its already-expanded
				// Template at the same precedence used by RefreshStyleValues so the
				// Theme pass does not detach an equivalent initial template tree.
				: it.source->TemplateState.AppliedControlTemplateFromStyle
					? DependencyPropertyValueSource::Style
				: fromTheme
					? DependencyPropertyValueSource::Theme
					: DependencyPropertyValueSource::Style;
			const auto host = instOf.find(it.name);
			if (host == instOf.end() || !host->second
				|| !cui::framework::XamlAccess::SetTemplate(
					*host->second, reference, valueSource))
			{
				if (outError) *outError =
					L"Control.Template 依赖属性安装失败：" + it.name;
				return false;
			}
		}

		// Establish the runtime template relationship before any namescope,
		// TemplateBinding, ContentPresenter, or ItemsPresenter registration.
		// The string owner key is only a materialization locator; consumers below
		// must read the relationship from the instantiated control.
		for (const auto& it : items)
		{
			if (!it.templateGenerated) continue;
			const auto owner = instOf.find(it.templateOwner);
			const auto generated = instOf.find(it.name);
			if (it.templateOwner.empty() || owner == instOf.end()
				|| generated == instOf.end() || !owner->second
				|| !generated->second)
			{
				if (outError) *outError = L"ControlTemplate 生成节点缺少 TemplatedParent："
					+ it.name;
				return false;
			}
			cui::framework::XamlAccess::SetTemplatedParent(
				*generated->second, owner->second);
		}
		for (auto& it : items)
		{
			setDiagnosticContext(it.source);
			auto dcIt = dcOf.find(it.name);
			if (dcIt == dcOf.end()) continue;
			auto dc = dcIt->second;
			auto* c = dc->ControlInstance;
			if (!c) continue;
			if (it.borrowedTemplateOwner) continue;
			const auto authoredValueSource = it.templateGenerated
				? DependencyPropertyValueSource::Template
				: DependencyPropertyValueSource::Local;

			dc->EventHandlers.clear();
			for (const auto& [eventName, eventHandler] : it.events)
			{
					setDiagnosticContext(it.source, eventName);
					const auto& k = eventName;
					if (k.empty()) continue;
					std::wstring v = DesignerEventCatalog::NormalizeHandlerName(
						eventHandler);
					std::wstring validationError;
					if (v.empty() || !DesignerEventCatalog::ValidateHandlerName(
						v, &validationError))
					{
						if (outError) *outError = L"控件 " + it.name
							+ L" 的事件处理函数无效：" + k;
						return false;
					}
					dc->EventHandlers[k] = std::move(v);
			}
			dc->CommandBindings = it.commandBindings;
			dc->InputBindings = it.inputBindings;
			c->ClearInputBindings();
			for (const auto& binding : it.inputBindings)
			{
				std::wstring gestureError;
				bool added = false;
				Control* commandTarget = nullptr;
				const bool targetsWindow = !binding.CommandTarget.empty()
					&& binding.CommandTarget == document.Window.Name;
				if (!binding.CommandTarget.empty())
				{
					if (targetsWindow) commandTarget = &stagingRoot;
					else
					{
						const auto target = instOf.find(binding.CommandTarget);
						if (target == instOf.end() || !target->second)
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的 InputBinding.CommandTarget 无法解析："
								+ binding.CommandTarget;
							return false;
						}
						commandTarget = target->second;
					}
				}
				const auto bindingIndex = c->GetInputBindings().size();
				if (binding.Kind == DesignInputBindingKind::Key)
				{
					KeyGesture gesture;
					added = TryParseKeyGesture(
						binding.Gesture, gesture, &gestureError)
						&& c->AddInputBinding(KeyBinding{
							RoutedCommand(binding.Command), gesture,
							binding.CommandParameter, commandTarget });
				}
				else
				{
					MouseGesture gesture;
					added = TryParseMouseGesture(
						binding.Gesture, gesture, &gestureError)
						&& c->AddInputBinding(MouseBinding{
							RoutedCommand(binding.Command), gesture,
							binding.CommandParameter, commandTarget });
				}
				if (!added)
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的 InputBinding 无效：" + gestureError;
					return false;
				}
				if (!binding.CommandTarget.empty())
					candidate.InputBindingTargets.push_back({
						ControlWeakReference(c), it.name, bindingIndex,
						binding.CommandTarget, targetsWindow });
			}

			dc->DataBindings.clear();
			for (const auto& [targetProperty, binding] : it.bindings)
			{
					setDiagnosticContext(it.source, targetProperty);
					DesignerDataContextSchema elementSourceSchema;
					const DesignerDataContextSchema* sourceSchema = nullptr;
					if (targetProperty == L"DataContext")
					{
						const auto scoped = inheritedSchemas.find(it.name);
						if (scoped != inheritedSchemas.end())
							sourceSchema = &scoped->second;
					}
					else
					{
						const auto scoped = contextSchemas.find(it.name);
						if (scoped != contextSchemas.end())
							sourceSchema = &scoped->second;
					}
					if (!binding.ElementName.empty())
					{
						const auto source = instOf.find(binding.ElementName);
						if (source == instOf.end() || !source->second)
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的 ElementName 引用了不存在的控件："
								+ binding.ElementName;
							return false;
						}
						elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
							*source->second);
						sourceSchema = &elementSourceSchema;
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::Self)
					{
						elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(*c);
						sourceSchema = &elementSourceSchema;
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::TemplatedParent)
					{
						auto* source = c->GetTemplatedParent();
						if (!it.templateGenerated || !source)
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的 TemplatedParent 无法解析。";
							return false;
						}
						elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
							*source);
						sourceSchema = &elementSourceSchema;
					}
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
					{
						if (auto* source = DesignerBindingUtils::FindAncestorSource(
							*c, binding))
						{
							elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
								*source);
							sourceSchema = &elementSourceSchema;
						}
						else sourceSchema = nullptr;
					}
					if (binding.IsMultiBinding()) sourceSchema = nullptr;
					const DependencyPropertyMetadata* metadata = nullptr;
					std::wstring validationError;
					if (!DesignerBindingUtils::Validate(
						*c, targetProperty, binding, &metadata, &validationError,
						sourceSchema))
					{
						if (outError) *outError = L"控件 " + it.name + L"：" + validationError;
						return false;
					}

					dc->DataBindings[metadata->Name()] = binding;
			}

			cui::framework::StyleAccess::SetResourceKey(
				*c, it.properties.StyleResourceKey,
				it.styleResourceFromTheme,
				it.styleResourceIsAutomatic);

			using PropertyEntry = std::pair<
				const std::wstring*, const DesignPropertyAssignment*>;
			std::vector<PropertyEntry> propertyEntries;
			for (const auto& [propertyName, assignment] : it.properties.Values)
				propertyEntries.emplace_back(&propertyName, &assignment);
			std::stable_sort(propertyEntries.begin(), propertyEntries.end(),
				[c](const PropertyEntry& left, const PropertyEntry& right)
				{
					const auto* leftMetadata =
						c->FindPropertyMetadata(*left.first);
					const auto* rightMetadata =
						c->FindPropertyMetadata(*right.first);
					if (leftMetadata && rightMetadata)
					{
						const auto& leftDesign = leftMetadata->Design();
						const auto& rightDesign = rightMetadata->Design();
						if (leftDesign.CategoryOrder != rightDesign.CategoryOrder)
							return leftDesign.CategoryOrder < rightDesign.CategoryOrder;
						if (leftDesign.Order != rightDesign.Order)
							return leftDesign.Order < rightDesign.Order;
					}
					else if (leftMetadata != rightMetadata)
						return leftMetadata != nullptr;
					return *left.first < *right.first;
				});

			for (const auto& [propertyNamePointer, assignmentPointer]
				: propertyEntries)
			{
				const auto& propertyName = *propertyNamePointer;
				const auto& assignment = *assignmentPointer;
				setDiagnosticContext(it.source, propertyName);
				if (!assignment.ResourceKey.empty()
					&& !assignment.DynamicResourceKey.empty())
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的属性 " + propertyName
						+ L" 不能同时使用静态和动态资源。";
					return false;
				}

				DesignerStyleValue value = assignment.Value;
				if (!assignment.ResourceKey.empty())
				{
					const auto* resource = findScopedResource(
						it, assignment.ResourceKey);
					if (!resource)
					{
						if (outError) *outError = L"控件 " + it.name
							+ L" 的属性 " + propertyName
							+ L" 引用了不存在的资源：" + assignment.ResourceKey;
						return false;
					}
					value = resource->Value;
				}
				if (!assignment.DynamicResourceKey.empty()
					&& assignment.DynamicResourceKey.find_first_not_of(L" \t\r\n")
						== std::wstring::npos)
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的属性 " + propertyName + L" 动态资源键为空。";
					return false;
				}

				std::wstring canonicalName;
				DesignerStyleValue effective;
				std::wstring metadataError;
				const auto& propertyResourceDocument =
					it.templateResourceFromTheme && compiled.Theme
						? *compiled.Theme : document;
				if (!DesignerPropertyCatalog::ApplyAndTrackValue(
					*c, dc->MetadataProperties, propertyName, value,
					&canonicalName, &effective, &metadataError,
					propertyResourceDocument.ResourceBasePath,
					propertyResourceDocument.Resources,
					authoredValueSource))
				{
					if (outError) *outError = L"控件 " + it.name
						+ L"：" + metadataError;
					return false;
				}
				if (!assignment.ResourceKey.empty())
					dc->MetadataPropertyResourceKeys[canonicalName] =
						assignment.ResourceKey;
				if (!assignment.DynamicResourceKey.empty())
				{
					if (!cui::framework::DependencyPropertyAccess::SetDynamicResource(
						*c, canonicalName, assignment.DynamicResourceKey,
						authoredValueSource))
					{
						if (outError) *outError = L"控件 " + it.name
							+ L" 无法安装属性 " + canonicalName
							+ L" 的动态资源表达式。";
						return false;
					}
					dc->MetadataPropertyDynamicResourceKeys[canonicalName] =
						assignment.DynamicResourceKey;
				}
			}

			if (it.extra.is_object())
			{
				if (it.type == UIClass::UI_Grid)
				{
					auto* gridPanel = (Grid*)c;
					gridPanel->ClearRows();
					gridPanel->ClearColumns();
					if (it.extra.contains("rows") && it.extra["rows"].is_array())
					{
						for (auto& r : it.extra["rows"])
						{
							if (!r.is_object()) continue;
							GridLength h = GridLengthFromValue(r.contains("height") ? r["height"] : DesignValue(), GridLength::Auto());
							float minH = r.value("min", 0.0f);
							float maxH = r.value("max", FLT_MAX);
							gridPanel->AddRow(h, minH, maxH);
						}
					}
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& col : it.extra["columns"])
						{
							if (!col.is_object()) continue;
							GridLength w = GridLengthFromValue(col.contains("width") ? col["width"] : DesignValue(), GridLength::Auto());
							float minW = col.value("min", 0.0f);
							float maxW = col.value("max", FLT_MAX);
							gridPanel->AddColumn(w, minW, maxW);
						}
					}
				}
				else if (it.type == UIClass::UI_ChartView)
				{
					auto* chart = static_cast<ChartView*>(c);
					chart->Clear();
					if (it.extra.contains("series") && it.extra["series"].is_array())
					{
						for (const auto& seriesValue : it.extra["series"])
						{
							if (!seriesValue.is_object()) continue;
							ChartSeries series;
							series.Name = FromUtf8(seriesValue.value("name", std::string{}));
							series.Visible = seriesValue.value("visible", true);
							if (seriesValue.contains("color"))
								series.Color = ColorFromValue(seriesValue["color"], series.Color);
							if (seriesValue.contains("points")
								&& seriesValue["points"].is_array())
							{
								for (const auto& pointValue : seriesValue["points"])
								{
									if (!pointValue.is_object()) continue;
									ChartPoint point(
										FromUtf8(pointValue.value("label", std::string{})),
										pointValue.value("value", 0.0));
									point.Tag = pointValue.value("tag", static_cast<UINT64>(0));
									point.UseCustomColor = pointValue.value("useCustomColor", false)
										&& pointValue.contains("color");
									if (point.UseCustomColor)
										point.Color = ColorFromValue(pointValue["color"], point.Color);
									series.Points.push_back(std::move(point));
								}
							}
							chart->AddSeries(series);
						}
					}
				}
				else if (it.type == UIClass::UI_MediaPlayer)
				{
					if (it.extra.contains("mediaFile") && it.extra["mediaFile"].is_string())
						dc->DesignStrings[L"mediaFile"] = FromUtf8(it.extra["mediaFile"].get<std::string>());
					else
						dc->DesignStrings.erase(L"mediaFile");
				}
			}
		}

		// Resolve authored ICommandSource targets only after every named control
		// and recursive MenuItem has been created. Window targets use the staging
		// root as a transient weak placeholder; the returned record keeps the
		// authored target pending until RuntimeDocument receives the real Window.
		for (const auto& item : items)
		{
			if (item.structure.CommandTarget.empty()) continue;
			const auto source = instOf.find(item.name);
			if (source == instOf.end() || !source->second)
			{
				if (outError) *outError = L"控件 " + item.name
					+ L" 的 CommandTarget source 无法解析。";
				return false;
			}
			candidate.CommandTargets.push_back({
				ControlWeakReference(source->second), item.name, {},
				item.structure.CommandTarget,
				item.structure.CommandTarget == document.Window.Name });
		}
		for (auto& reference : candidate.CommandTargets)
		{
			auto* source = reference.Source.Get();
			Control* target = nullptr;
			if (reference.TargetsWindow) target = &stagingRoot;
			else
			{
				const auto found = instOf.find(reference.TargetName);
				if (found != instOf.end()) target = found->second;
			}
			if (!source || !target
				|| !SetAuthoredCommandTarget(source, target, outError))
			{
				if (outError && outError->empty())
					*outError = L"CommandTarget 无法解析："
						+ reference.SourceName + L" -> "
						+ reference.TargetName;
				return false;
			}
		}

		std::unordered_map<std::wstring, BindingListReference> materializedLists;
		for (const auto& definition : document.DataLists)
		{
			auto runtime = DesignDataResourceUtils::BuildRuntimeList(
				document, definition, outError);
			if (!runtime) return false;
			materializedLists.emplace(
				definition.Key, BindingListReference(std::move(runtime)));
		}
		std::unordered_map<std::wstring,
			std::shared_ptr<CollectionViewSource>> materializedViews;
		for (const auto& definition : document.CollectionViews)
		{
			auto runtime = std::make_shared<CollectionViewSource>();
			materializedViews.emplace(definition.Key, runtime);
			materializedLists.emplace(
				definition.Key, BindingListReference(runtime));
			candidate.CollectionViews.push_back(std::move(runtime));
		}
		std::unordered_set<std::wstring> configuredViews;
		while (configuredViews.size() < document.CollectionViews.size())
		{
			bool progressed = false;
			for (const auto& definition : document.CollectionViews)
			{
				if (configuredViews.contains(definition.Key)) continue;
				auto runtime = materializedViews.at(definition.Key);
				std::wstring itemTypeName;
				if (!definition.SourceBindingPath.empty())
				{
					const auto* sourceProperty = DesignerDataContextSchemaUtils::Find(
						dataContextSchema, definition.SourceBindingPath);
					if (!sourceProperty)
					{
						if (outError) *outError = L"CollectionViewSource Source Binding 未声明："
							+ definition.SourceBindingPath;
						return false;
					}
					itemTypeName = sourceProperty->ItemType;
					runtime->SetSourceBindingPath(definition.SourceBindingPath);
				}
				else
				{
					const auto source = materializedLists.find(
						definition.SourceResource);
					if (source == materializedLists.end())
					{
						if (outError) *outError = L"CollectionViewSource Source 资源未物化："
							+ definition.SourceResource;
						return false;
					}
					if (const auto dependency = materializedViews.find(
						definition.SourceResource);
						dependency != materializedViews.end()
						&& !configuredViews.contains(dependency->first)) continue;
					runtime->SetSource(source->second);
					itemTypeName = source->second.Get()->ItemTypeName();
				}
				const auto* dataType = document.FindDataType(itemTypeName);
				if (!dataType)
				{
					if (outError) *outError = L"CollectionViewSource ItemType 未声明："
						+ itemTypeName;
					return false;
				}
				std::vector<CollectionGroupDescription> groups;
				for (const auto& authored : definition.GroupDescriptions)
					groups.push_back({ authored.PropertyName,
						authored.Direction, authored.IgnoreCase });
				runtime->SetGroupDescriptions(std::move(groups));
				std::vector<CollectionAggregateDescription> aggregates;
				for (const auto& authored : definition.AggregateDescriptions)
					aggregates.push_back({ authored.Name, authored.PropertyName,
						authored.Function });
				runtime->SetAggregateDescriptions(std::move(aggregates));
				std::vector<CollectionSortDescription> sorts;
				for (const auto& authored : definition.SortDescriptions)
					sorts.push_back({ authored.PropertyName,
						authored.Direction, authored.IgnoreCase });
				runtime->SetSortDescriptions(std::move(sorts));
				std::vector<CollectionFilterDescription> filters;
				for (const auto& authored : definition.FilterDescriptions)
				{
					const auto* property = DesignerDataContextSchemaUtils::Find(
						dataType->Properties, authored.PropertyName);
					if (!property)
					{
						if (outError) *outError = L"CollectionViewSource 筛选属性未声明："
							+ authored.PropertyName;
						return false;
					}
					DesignerStyleValueKind kind{};
					switch (property->ValueKind)
					{
					case BindingValueKind::Bool: kind = DesignerStyleValueKind::Bool; break;
					case BindingValueKind::NullableBool:
						kind = DesignerStyleValueKind::NullableBool;
						break;
					case BindingValueKind::Int: kind = DesignerStyleValueKind::Int; break;
					case BindingValueKind::Int64: kind = DesignerStyleValueKind::Int64; break;
					case BindingValueKind::Float: kind = DesignerStyleValueKind::Float; break;
					case BindingValueKind::Double: kind = DesignerStyleValueKind::Double; break;
					case BindingValueKind::String: kind = DesignerStyleValueKind::String; break;
					default:
						if (outError) *outError = L"CollectionViewSource 筛选属性不是标量："
							+ authored.PropertyName;
						return false;
					}
					BindingValue value;
					if (authored.Operator != CollectionFilterOperator::IsEmpty
						&& authored.Operator != CollectionFilterOperator::IsNotEmpty
						&& !DesignerStyleSheetUtils::TryConvertValue(
							{ kind, authored.Value }, value, outError,
							document.ResourceBasePath, document.Resources)) return false;
					filters.push_back({ authored.PropertyName,
						authored.Operator, std::move(value), authored.IgnoreCase });
				}
				runtime->SetFilterDescriptions(std::move(filters));
				configuredViews.insert(definition.Key);
				progressed = true;
			}
			if (!progressed)
			{
				if (outError) *outError = L"CollectionViewSource 依赖无法解析。";
				return false;
			}
		}
		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, it.type)) continue;
			const auto found = instOf.find(it.name);
			auto* itemsControl = found == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(found->second);
			const auto containerType = GetDefaultItemContainerType(it.type);
			const auto* containerDescriptor =
				XamlRuntimeSchema::DefaultTypeFor(containerType);
			if (!itemsControl || !containerDescriptor)
			{
				if (outError) *outError = L"项容器 XAML 类型不存在：" + it.name;
				return false;
			}
			itemsControl->SetGeneratedContainerInitializer(
				[containerDescriptor, schemaContext](
					Control& container, std::wstring* error)
				{
					return XamlRuntimeSchema::AttachBuiltInType(
						container, *containerDescriptor, *schemaContext, error);
				});
			if (!itemsControl->LastTemplateError().empty())
			{
				if (outError) *outError = itemsControl->LastTemplateError();
				return false;
			}
		}
		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsUIClassAssignableFrom(UIClass::UI_ItemsControl, it.type)
				|| !it.extra.is_object()
				|| !it.extra.contains("groupStyle")) continue;
			const auto found = instOf.find(it.name);
			auto* itemsControl = found == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(found->second);
			if (!itemsControl || !it.extra["groupStyle"].is_string())
			{
				if (outError) *outError = L"ItemsControl GroupStyle 格式无效："
					+ it.name;
				return false;
			}
			const auto key = FromUtf8(
				it.extra["groupStyle"].get<std::string>());
			const auto* definition = it.source
				? document.FindGroupStyle(document.Nodes, *it.source, key)
				: document.FindGroupStyle(key);
			if (!definition)
			{
				if (outError) *outError = L"ItemsControl 引用了不存在的 GroupStyle："
					+ key;
				return false;
			}
			auto style = std::make_shared<GroupStyle>();
			const auto* headerTemplate = it.source
				? document.FindGroupStyleHeaderTemplate(
					document.Nodes, *it.source, key)
				: definition->HeaderTemplate.empty()
					? document.FindImplicitDataTemplate(
						std::wstring(CollectionViewGroupDataTypeName))
					: document.FindDataTemplate(definition->HeaderTemplate);
			if (!headerTemplate && !definition->HeaderTemplate.empty())
			{
				if (outError) *outError = L"GroupStyle 引用了不存在的 DataTemplate："
					+ definition->HeaderTemplate;
				return false;
			}
			if (headerTemplate)
			{
				const DesignCollectionViewSource* groupView = nullptr;
				if (it.extra.contains("itemsSourceResource")
					&& it.extra["itemsSourceResource"].is_string())
					groupView = document.FindCollectionView(FromUtf8(
						it.extra["itemsSourceResource"].get<std::string>()));
				const auto groupSchema = DesignerModel::DesignDataResourceUtils::
					BuildCollectionViewGroupSchema(resolveItemType(it), groupView
						? &groupView->AggregateDescriptions : nullptr);
				const auto* declaration = it.source
					? document.FindLocalGroupStyleOwner(
						document.Nodes, *it.source, key) : nullptr;
				auto visibleObjects = declaration
					? document.VisibleObjectResources(document.Nodes, *declaration)
					: DesignObjectResourceDictionary{};
				if (!declaration)
				{
					visibleObjects.Components = document.Components;
					visibleObjects.ControlTemplates = document.ControlTemplates;
					visibleObjects.DataTemplates = document.DataTemplates;
					visibleObjects.ItemsPanelTemplates = document.ItemsPanelTemplates;
					visibleObjects.GroupStyles = document.GroupStyles;
				}
				DesignerStyleSheet headerStyles = document.StyleSheet;
				if (declaration)
					if (const auto pending = pendingByName.find(declaration->Name);
						pending != pendingByName.end())
						headerStyles = visibleStyleScope(*pending->second);
				style->HeaderTemplate = ItemTemplateReference(
					std::make_shared<MaterializedDataTemplate>(
						templateDocument, *headerTemplate,
						std::move(visibleObjects), std::move(headerStyles),
						nestedOptions, groupSchema));
			}
			itemsControl->SetGroupStyle(GroupStyleReference(std::move(style)));
			if (!itemsControl->LastTemplateError().empty())
			{
				if (outError) *outError = itemsControl->LastTemplateError();
				return false;
			}
		}
		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsUIClassAssignableFrom(UIClass::UI_ItemsControl, it.type)
				|| !it.extra.is_object()
				|| !it.extra.contains("itemsPanel")) continue;
			const auto found = instOf.find(it.name);
			auto* itemsControl = found == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(found->second);
			if (!itemsControl || !it.extra["itemsPanel"].is_string())
			{
				if (outError) *outError = L"ItemsControl ItemsPanel 格式无效："
					+ it.name;
				return false;
			}
			const auto key = FromUtf8(
				it.extra["itemsPanel"].get<std::string>());
			const auto* definition = it.source
				? document.FindItemsPanelTemplate(document.Nodes, *it.source, key)
				: document.FindItemsPanelTemplate(key);
			if (!definition)
			{
				if (outError) *outError = L"ItemsControl 引用了不存在的 ItemsPanelTemplate："
					+ key;
				return false;
			}
			const auto itemsPanel = ItemsPanelTemplateReference(
				std::make_shared<ItemsPanelTemplate>(definition->Value));
			const auto valueSource = it.templateGenerated
				? DependencyPropertyValueSource::Template
				: DependencyPropertyValueSource::Local;
			if (!cui::framework::DependencyPropertyAccess::SetValue(
				*itemsControl, L"ItemsPanel",
				BindingValue(itemsPanel), valueSource))
			{
				if (outError) *outError =
					L"ItemsControl ItemsPanel 依赖属性安装失败："
					+ it.name;
				return false;
			}
			if (!itemsControl->LastTemplateError().empty())
			{
				if (outError) *outError = itemsControl->LastTemplateError();
				return false;
			}
		}
		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsUIClassAssignableFrom(UIClass::UI_ItemsControl, it.type)
				|| !it.extra.is_object()
				|| !it.extra.contains("itemContainerStyle")) continue;
			const auto found = instOf.find(it.name);
			auto* itemsControl = found == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(found->second);
			if (!itemsControl
				|| !it.extra["itemContainerStyle"].is_string())
			{
				if (outError) *outError = L"ItemContainerStyle 格式无效："
					+ it.name;
				return false;
			}
			const auto styleId = FromUtf8(
				it.extra["itemContainerStyle"].get<std::string>());
			itemsControl->SetItemContainerStyle(styleId);
		}
		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| (it.type != UIClass::UI_ListBox
				&& it.type != UIClass::UI_ComboBox
				&& it.type != UIClass::UI_TreeView)) continue;
			const auto found = instOf.find(it.name);
			auto* selector = found == instOf.end()
				? nullptr : dynamic_cast<Selector*>(found->second);
			auto* tree = found == instOf.end()
				? nullptr : dynamic_cast<TreeView*>(found->second);
			if (!selector && !tree)
			{
				if (outError) *outError =
					L"项容器模板宿主类型无效：" + it.name;
				return false;
			}
			const auto containerType = GetDefaultItemContainerType(it.type);
			const auto containerStyle = tree
				? tree->GetItemContainerStyle()
				: selector->GetItemContainerStyle();

			DesignNode probe;
			probe.Name = it.name + L"#itemContainer";
			probe.ParentRef = it.name;
			probe.Type = containerType;
			if (const auto* descriptor =
				XamlRuntimeSchema::DefaultTypeFor(containerType))
				probe.XamlType = descriptor->TypeId;
			if (!containerStyle.empty())
				probe.Properties.StyleResourceKey = containerStyle;
			EffectiveControlTemplate effective;
			if (!ResolveEffectiveControlTemplate(
				document, document.Nodes, compiled.Theme.get(),
				probe, effective, outError))
				return false;
			if (!effective.Definition) continue;

			auto visibleObjects = it.source
				? document.VisibleObjectResources(document.Nodes, *it.source)
				: DesignObjectResourceDictionary{};
			if (!it.source)
			{
				visibleObjects.Components = document.Components;
				visibleObjects.ControlTemplates = document.ControlTemplates;
				visibleObjects.DataTemplates = document.DataTemplates;
				visibleObjects.ItemsPanelTemplates = document.ItemsPanelTemplates;
				visibleObjects.GroupStyles = document.GroupStyles;
			}
			const bool documentTemplate = effective.FromTheme
				|| IsDocumentControlTemplateDefinition(
					document, effective.Definition);
			auto runtimeTemplate = ControlTemplateReference(
				std::make_shared<MaterializedControlTemplate>(
					templateDocument, containerType,
					std::move(visibleObjects), visibleStyleScope(it), nestedOptions,
					containerStyle, effective.Definition->Key,
					DesignerComponentType{}, *effective.Definition,
					effective.FromTheme ? compiled.Theme : templateDocument,
					documentTemplate));
			if (selector)
				selector->SetItemContainerTemplate(runtimeTemplate);
			else tree->SetItemContainerTemplate(runtimeTemplate);
			const auto& templateError = selector
				? selector->LastTemplateError()
				: tree->LastTemplateError();
			if (!templateError.empty())
			{
				if (outError) *outError = templateError;
				return false;
			}
		}
		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !it.extra.is_object()
				|| !it.extra.contains("itemsSourceResource")) continue;
			const auto found = instOf.find(it.name);
			if (found == instOf.end()
				|| !it.extra["itemsSourceResource"].is_string())
			{
				if (outError) *outError = L"ItemsSource DataList 格式无效："
					+ it.name;
				return false;
			}
			const auto key = FromUtf8(
				it.extra["itemsSourceResource"].get<std::string>());
			const auto* definition = document.FindDataList(key);
			const auto* viewDefinition = document.FindCollectionView(key);
			if (!definition && !viewDefinition)
			{
				if (outError) *outError = L"控件 " + it.name
					+ L" 引用了不存在的列表资源：" + key;
				return false;
			}
			const auto source = materializedLists.find(
				definition ? definition->Key : viewDefinition->Key);
			if (source == materializedLists.end())
			{
				if (outError) *outError = L"列表资源未物化：" + key;
				return false;
			}
			const auto& reference = source->second;
			bool accepted = false;
			std::wstring rejection;
			if (auto* target = dynamic_cast<ItemsControl*>(found->second))
			{
				target->SetItemsSource(reference);
				accepted = target->GetItemsSource() == reference;
				rejection = target->LastTemplateError();
			}
			else
			{
				if (outError) *outError = L"控件不支持 DataList ItemsSource："
					+ it.name;
				return false;
			}
			if (!accepted)
			{
				if (outError) *outError = L"控件 " + it.name
					+ L" 拒绝了 ItemsSource 物化"
					+ (rejection.empty() ? L"。" : L"：" + rejection);
				return false;
			}
		}

		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, it.type)) continue;
			const auto found = instOf.find(it.name);
			auto* itemsControl = found == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(found->second);
			auto* tree = found == instOf.end()
				? nullptr : dynamic_cast<TreeView*>(found->second);
			if (!itemsControl)
			{
				if (outError) *outError = L"项控件物化类型不匹配：" + it.name;
				return false;
			}
			const bool hasExplicitTemplate = it.extra.is_object()
				&& it.extra.contains("itemTemplate");
			if (hasExplicitTemplate && !it.extra["itemTemplate"].is_string())
			{
				if (outError) *outError = L"ItemsControl ItemTemplate 格式无效："
					+ it.name;
				return false;
			}
			const auto key = hasExplicitTemplate ? FromUtf8(
				it.extra["itemTemplate"].get<std::string>()) : std::wstring{};
			const auto* itemType = resolveItemType(it);
			const auto* dataTemplate = hasExplicitTemplate
				? (it.source
					? document.FindDataTemplate(document.Nodes, *it.source, key)
					: document.FindDataTemplate(key))
				: itemType
					? (it.source
						? document.FindImplicitDataTemplate(
							document.Nodes, *it.source, itemType->Name)
						: document.FindImplicitDataTemplate(itemType->Name))
					: nullptr;
			auto visibleObjects = it.source
				? document.VisibleObjectResources(document.Nodes, *it.source)
				: DesignObjectResourceDictionary{};
			if (!it.source)
			{
				visibleObjects.Components = document.Components;
				visibleObjects.ControlTemplates = document.ControlTemplates;
				visibleObjects.DataTemplates = document.DataTemplates;
				visibleObjects.ItemsPanelTemplates = document.ItemsPanelTemplates;
				visibleObjects.GroupStyles = document.GroupStyles;
			}
			const auto itemStyles = visibleStyleScope(it);
			if (tree)
			{
				auto implicitTemplates = std::make_shared<
					std::unordered_map<std::wstring, ItemTemplateReference>>();
				for (const auto& definition : visibleObjects.DataTemplates)
				{
					if (!definition.IsImplicit()) continue;
					(*implicitTemplates)[definition.DataType] = ItemTemplateReference(
						std::make_shared<MaterializedDataTemplate>(
							templateDocument, definition, visibleObjects,
							itemStyles, nestedOptions));
				}
				tree->SetImplicitItemTemplateResolver(
					[implicitTemplates](const std::wstring& itemType)
					{
						const auto found = implicitTemplates->find(itemType);
						return found == implicitTemplates->end()
							? ItemTemplateReference{} : found->second;
					});
			}
			if (!dataTemplate && !hasExplicitTemplate) continue;
			if (!dataTemplate)
			{
				if (outError) *outError = L"ItemsControl 引用了不存在的 DataTemplate："
					+ key;
				return false;
			}
			if (hasExplicitTemplate && itemType
				&& itemType->Name != dataTemplate->DataType)
			{
				if (outError) *outError = L"ItemsControl " + it.name
					+ L" 的 ItemsSource ItemType 与 DataTemplate.DataType 不一致。";
				return false;
			}
			auto runtimeTemplate = ItemTemplateReference(
				std::make_shared<MaterializedDataTemplate>(
					templateDocument, *dataTemplate,
					visibleObjects, itemStyles, nestedOptions));
			itemsControl->SetItemTemplate(runtimeTemplate);
			const bool installed = static_cast<bool>(
				itemsControl->GetItemTemplate());
			if (!installed)
			{
				if (outError) *outError = L"项控件无法安装 DataTemplate："
					+ dataTemplate->DisplayName();
				return false;
			}
		}

		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsContentHostType(it.type)) continue;
			const auto found = instOf.find(it.name);
			auto* presenter = found == instOf.end()
				? nullptr : dynamic_cast<ContentPresenter*>(found->second);
			auto* contentControl = found == instOf.end()
				? nullptr : dynamic_cast<ContentControl*>(found->second);
			if (!presenter && !contentControl)
			{
				if (outError) *outError =
					L"内容控件物化类型不匹配：" + it.name;
				return false;
			}
			const auto visualChildCount = std::count_if(
				items.begin(), items.end(), [&](const auto& candidate)
					{
						return candidate.parent == it.name
							&& !candidate.controlTemplateRoot
							&& (!candidate.extra.is_object()
								|| candidate.extra.value(
									"headeredRegion", std::string{}) != "header");
					});
			const bool hasContentBinding = it.bindings.contains(L"Content");
			const bool hasContentValue = it.properties.Find(L"Content");
			const bool hasExplicitTemplate = it.extra.is_object()
				&& it.extra.contains("contentTemplate");
			if (presenter && visualChildCount != 0
				&& it.templateContentSource.empty())
			{
				if (outError) *outError =
					L"ContentPresenter 不接受直接视觉子节点："
					+ (it.name.empty() ? L"<unnamed>" : it.name)
					+ L"；Parent=" + (it.parent.empty() ? L"<root>" : it.parent)
					+ L"；TemplateOwner="
					+ (it.templateOwner.empty() ? L"<none>" : it.templateOwner);
				return false;
			}
			if (presenter && visualChildCount != 0
				&& (hasContentBinding || hasContentValue || hasExplicitTemplate))
			{
				if (outError) *outError = L"ControlTemplate ContentPresenter 的视觉槽"
					L"不能与数据 Content 同时使用：" + it.name;
				return false;
			}
			if (contentControl && (visualChildCount > 1
				|| (visualChildCount != 0 && (hasContentBinding
					|| hasContentValue || hasExplicitTemplate))))
			{
				if (outError) *outError = L"ContentControl " + it.name
					+ L" 最多接受一个直接视觉子节点，且不能与数据内容同时使用。";
				return false;
			}
			if (visualChildCount != 0) continue;
			if (hasContentValue && (hasContentBinding || hasExplicitTemplate))
			{
				if (outError) *outError = L"内容控件 " + it.name
					+ L" 的标量 Content 与 Binding/ContentTemplate 冲突。";
				return false;
			}
			if (hasExplicitTemplate && !it.extra["contentTemplate"].is_string())
			{
				if (outError) *outError =
					L"内容控件 ContentTemplate 格式无效：" + it.name;
				return false;
			}
			const auto key = hasExplicitTemplate ? FromUtf8(
				it.extra["contentTemplate"].get<std::string>()) : std::wstring{};
			const auto* contentType = resolveContentType(it);
			if (presenter)
				presenter->SetContentTypeName(contentType ? contentType->Name : L"");
			else
				contentControl->SetContentTypeName(contentType ? contentType->Name : L"");
			const auto* dataTemplate = hasExplicitTemplate
				? (it.source
					? document.FindDataTemplate(document.Nodes, *it.source, key)
					: document.FindDataTemplate(key))
				: contentType
					? (it.source
						? document.FindImplicitDataTemplate(
							document.Nodes, *it.source, contentType->Name)
						: document.FindImplicitDataTemplate(contentType->Name))
					: nullptr;
			if (!dataTemplate && !hasExplicitTemplate)
				continue;
			if (!dataTemplate)
			{
				if (outError) *outError =
					L"内容控件引用了不存在的 DataTemplate：" + key;
				return false;
			}
			if (hasExplicitTemplate && contentType
				&& contentType->Name != dataTemplate->DataType)
			{
				if (outError) *outError = L"内容控件 " + it.name
					+ L" 的 Content DataType 与 ContentTemplate.DataType 不一致。";
				return false;
			}
			auto visibleObjects = it.source
				? document.VisibleObjectResources(document.Nodes, *it.source)
				: DesignObjectResourceDictionary{};
			if (!it.source)
			{
				visibleObjects.Components = document.Components;
				visibleObjects.ControlTemplates = document.ControlTemplates;
				visibleObjects.DataTemplates = document.DataTemplates;
				visibleObjects.ItemsPanelTemplates = document.ItemsPanelTemplates;
				visibleObjects.GroupStyles = document.GroupStyles;
			}
			auto runtimeTemplate = ItemTemplateReference(
				std::make_shared<MaterializedDataTemplate>(
					templateDocument, *dataTemplate,
					std::move(visibleObjects), visibleStyleScope(it), nestedOptions));
			if (presenter) presenter->SetContentTemplate(runtimeTemplate);
			else contentControl->SetContentTemplate(runtimeTemplate);
			const bool installed = presenter
				? static_cast<bool>(presenter->GetContentTemplate())
				: static_cast<bool>(contentControl->GetContentTemplate());
			const auto& contentError = presenter
				? presenter->LastTemplateError()
				: contentControl->LastContentError();
			if (!installed || !contentError.empty())
			{
				if (outError) *outError = contentError.empty()
					? L"内容控件无法安装 DataTemplate："
						+ dataTemplate->DisplayName()
					: contentError;
				return false;
			}
		}

		for (auto& it : items)
		{
			if (it.borrowedTemplateOwner
				|| !IsHeaderedContentControlType(it.type)) continue;
			const auto found = instOf.find(it.name);
			auto* headeredContent = found == instOf.end()
				? nullptr : dynamic_cast<HeaderedContentControl*>(found->second);
			auto* headeredItems = found == instOf.end()
				? nullptr : dynamic_cast<HeaderedItemsControl*>(found->second);
			if (!headeredContent && !headeredItems)
			{
				if (outError) *outError =
					L"Headered control 物化类型不匹配：" + it.name;
				return false;
			}
			auto setHeaderTypeName = [&](std::wstring value)
			{
				if (headeredContent)
					headeredContent->SetHeaderTypeName(std::move(value));
				else headeredItems->SetHeaderTypeName(std::move(value));
			};
			auto setHeaderTemplate = [&](ItemTemplateReference value)
			{
				if (headeredContent)
					headeredContent->SetHeaderTemplate(std::move(value));
				else headeredItems->SetHeaderTemplate(std::move(value));
			};
			auto hasHeaderTemplate = [&]()
			{
				return headeredContent
					? static_cast<bool>(headeredContent->GetHeaderTemplate())
					: static_cast<bool>(headeredItems->GetHeaderTemplate());
			};
			auto lastHeaderError = [&]() -> const std::wstring&
			{
				return headeredContent
					? headeredContent->LastHeaderError()
					: headeredItems->LastHeaderError();
			};
			const auto visualHeaderCount = std::count_if(
				items.begin(), items.end(), [&](const auto& candidate)
				{
					return candidate.parent == it.name
						&& candidate.extra.is_object()
						&& candidate.extra.value(
							"headeredRegion", std::string{}) == "header";
				});
			const bool hasHeaderBinding = it.bindings.contains(L"Header");
			const bool hasHeaderValue = it.properties.Find(L"Header");
			const bool hasExplicitTemplate = it.extra.is_object()
				&& it.extra.contains("headerTemplate");
			if (visualHeaderCount > 1 || (visualHeaderCount != 0
				&& (hasHeaderBinding || hasHeaderValue || hasExplicitTemplate)))
			{
				if (outError) *outError = L"HeaderedContentControl " + it.name
					+ L" 最多接受一个视觉 Header，且不能与数据 Header 同时使用。";
				return false;
			}
			if (visualHeaderCount != 0) continue;
			if (hasHeaderValue && (hasHeaderBinding || hasExplicitTemplate))
			{
				if (outError) *outError = L"HeaderedContentControl " + it.name
					+ L" 的标量 Header 与 Binding/HeaderTemplate 冲突。";
				return false;
			}
			if (hasExplicitTemplate && !it.extra["headerTemplate"].is_string())
			{
				if (outError) *outError =
					L"HeaderTemplate 格式无效：" + it.name;
				return false;
			}
			const auto key = hasExplicitTemplate ? FromUtf8(
				it.extra["headerTemplate"].get<std::string>()) : std::wstring{};
			const auto* headerType = resolveHeaderType(it);
			setHeaderTypeName(headerType ? headerType->Name : L"");
			const auto* dataTemplate = hasExplicitTemplate
				? (it.source
					? document.FindDataTemplate(document.Nodes, *it.source, key)
					: document.FindDataTemplate(key))
				: headerType
					? (it.source
						? document.FindImplicitDataTemplate(
							document.Nodes, *it.source, headerType->Name)
						: document.FindImplicitDataTemplate(headerType->Name))
					: nullptr;
			if (!dataTemplate && !hasExplicitTemplate)
				continue;
			if (!dataTemplate)
			{
				if (outError) *outError =
					L"Header 引用了不存在的 DataTemplate：" + key;
				return false;
			}
			if (hasExplicitTemplate && headerType
				&& headerType->Name != dataTemplate->DataType)
			{
				if (outError) *outError = L"HeaderedContentControl " + it.name
					+ L" 的 Header DataType 与 HeaderTemplate.DataType 不一致。";
				return false;
			}
			auto visibleObjects = it.source
				? document.VisibleObjectResources(document.Nodes, *it.source)
				: DesignObjectResourceDictionary{};
			if (!it.source)
			{
				visibleObjects.Components = document.Components;
				visibleObjects.ControlTemplates = document.ControlTemplates;
				visibleObjects.DataTemplates = document.DataTemplates;
				visibleObjects.ItemsPanelTemplates = document.ItemsPanelTemplates;
				visibleObjects.GroupStyles = document.GroupStyles;
			}
			setHeaderTemplate(ItemTemplateReference(
				std::make_shared<MaterializedDataTemplate>(
					templateDocument, *dataTemplate,
					std::move(visibleObjects), visibleStyleScope(it), nestedOptions)));
			if (!hasHeaderTemplate() || !lastHeaderError().empty())
			{
				if (outError) *outError = lastHeaderError().empty()
					? L"HeaderedContentControl 无法安装 DataTemplate："
						+ dataTemplate->DisplayName()
					: lastHeaderError();
				return false;
			}
		}

		for (auto& it : items)
		{
			if (it.type != UIClass::UI_NativeSurface) continue;
			auto found = instOf.find(it.name);
			auto* surface = found == instOf.end()
				? nullptr : dynamic_cast<NativeSurface*>(found->second);
			if (!surface || !it.source)
			{
				if (outError) *outError = L"NativeSurface 材质化类型不匹配：" + it.name;
				return false;
			}
			if (surface->GetBehaviorKey().empty()) continue;
			std::unique_ptr<INativeSurfaceBehavior> behavior;
			if (options.NativeSurfaceBehaviorFactory)
				behavior = options.NativeSurfaceBehaviorFactory(*it.source, *surface);
			if (behavior)
			{
				surface->SetBehavior(std::move(behavior));
				continue;
			}
			if (!options.AllowNativeSurfacePlaceholder)
			{
				if (outError) *outError = L"未注册 NativeSurface 行为："
					+ surface->GetBehaviorKey() + L"（控件 " + it.name + L"）";
				return false;
			}
		}

		for (auto& it : items)
		{
			if (it.templateBindings.empty()) continue;
			const auto target = instOf.find(it.name);
			auto* templatedParent = target == instOf.end() || !target->second
				? nullptr : target->second->GetTemplatedParent();
			if (target == instOf.end() || !target->second || !templatedParent)
			{
				if (outError) *outError = L"组件模板绑定无法解析所属实例：" + it.name;
				return false;
			}
			for (const auto& [targetProperty, sourceProperty]
				: it.templateBindings)
			{
				if (!target->second->DataBindings.AddTemplateBinding(
					targetProperty, *templatedParent, sourceProperty))
				{
					if (outError) *outError = L"组件模板绑定创建失败："
						+ it.templateOwner + L"." + sourceProperty + L" -> "
						+ it.name + L"." + targetProperty + L"（"
						+ target->second->DataBindings.LastErrorMessage() + L"）";
					return false;
				}
			}
		}

		// Generated component-template nodes are intentionally absent from the
		// public DesignerControl list, so every template-local binding source is
		// connected while the expanded namescope and templated parent are known.
		for (auto& it : items)
		{
			if (!it.templateGenerated) continue;
			const auto targetRecord = dcOf.find(it.name);
			if (targetRecord == dcOf.end() || !targetRecord->second
				|| !targetRecord->second->ControlInstance) continue;
			auto* target = targetRecord->second->ControlInstance;
			auto stagedInheritanceParent = [&]() -> Control*
			{
				const auto parent = instOf.find(it.parent);
				if (parent != instOf.end() && parent->second)
					return parent->second;
				// A generated root is not attached yet. Its eventual visual
				// parent is the template owner, but TemplatedParent itself is
				// deliberately not exposed as FrameworkParent.
				return target->GetTemplatedParent();
			};
			for (const auto& [targetProperty, binding]
				: targetRecord->second->DataBindings)
			{
				if (binding.IsMultiBinding())
				{
					auto resolveSource = [&](const DesignerDataBinding& child,
						DesignerBindingUtils::ResolvedBindingSource& resolved,
						std::wstring* error)
					{
						if (!child.ElementName.empty())
						{
							const auto source = instOf.find(child.ElementName);
							if (source == instOf.end() || !source->second)
							{
								if (error) *error = L"ElementName 无法解析：" + child.ElementName;
								return false;
							}
							resolved.Source = source->second;
						}
						else if (child.RelativeSource
							== DesignerBindingRelativeSource::Self)
							resolved.Source = target;
						else if (child.RelativeSource
							== DesignerBindingRelativeSource::TemplatedParent)
						{
							auto* owner = target->GetTemplatedParent();
							if (!owner)
							{
								if (error) *error = L"TemplatedParent 无法解析。";
								return false;
							}
							resolved.Source = owner;
						}
						else if (child.RelativeSource
							== DesignerBindingRelativeSource::FindAncestor)
						{
							resolved.OwnedSource = DesignerBindingUtils::CreateAncestorSource(
								*target, child);
							resolved.Source = resolved.OwnedSource.Get();
						}
						else if (targetProperty == L"DataContext")
						{
							auto* parent = stagedInheritanceParent();
							resolved.Source = parent
								? &parent->DataContextSource()
								: &target->DataContextSource();
						}
						else resolved.Source = &target->DataContextSource();
						return resolved.Source || resolved.OwnedSource;
					};
					std::wstring installError;
					if (!DesignerBindingUtils::InstallBinding(*target,
						targetProperty, binding, resolveSource, &installError))
					{
						if (outError) *outError = L"组件模板 MultiBinding："
							+ installError;
						return false;
					}
					continue;
				}
				IBindingSource* bindingSource = nullptr;
				BindingSourceReference ownedBindingSource;
				if (!binding.ElementName.empty())
				{
					const auto source = instOf.find(binding.ElementName);
					if (source == instOf.end() || !source->second)
					{
						if (outError) *outError = L"组件模板 ElementName 无法解析："
							+ binding.ElementName;
						return false;
					}
					bindingSource = source->second;
				}
				else if (binding.RelativeSource
					== DesignerBindingRelativeSource::Self)
					bindingSource = target;
				else if (binding.RelativeSource
					== DesignerBindingRelativeSource::TemplatedParent)
				{
					auto* owner = target->GetTemplatedParent();
					if (!owner)
					{
						if (outError) *outError = L"组件模板 TemplatedParent 无法解析："
							+ it.name;
						return false;
					}
					bindingSource = owner;
				}
				else if (binding.RelativeSource
					== DesignerBindingRelativeSource::FindAncestor)
				{
					ownedBindingSource = DesignerBindingUtils::CreateAncestorSource(
						*target, binding);
					bindingSource = ownedBindingSource.Get();
				}
				else if (targetProperty == L"DataContext")
				{
					auto* parent = stagedInheritanceParent();
					bindingSource = parent
						? &parent->DataContextSource()
						: &target->DataContextSource();
				}
				else
					bindingSource = &target->DataContextSource();
				std::shared_ptr<const IBindingValueConverter> converter;
				if (!binding.Converter.empty())
				{
					converter = BindingValueConverterRegistry::Create(binding.Converter);
					if (!converter)
					{
						if (outError) *outError = L"组件模板 Converter 不存在："
							+ binding.Converter;
						return false;
					}
				}
				std::optional<BindingValue> fallbackValue;
				std::optional<BindingValue> targetNullValue;
				std::optional<BindingValue> converterParameter;
				std::wstring literalError;
				if (!DesignerBindingUtils::TryConvertOptionalLiteral(
					binding.FallbackValue, fallbackValue, &literalError)
					|| !DesignerBindingUtils::TryConvertOptionalLiteral(
						binding.TargetNullValue, targetNullValue, &literalError)
					|| !DesignerBindingUtils::TryConvertOptionalLiteral(
						binding.ConverterParameter, converterParameter, &literalError))
				{
					if (outError) *outError = L"组件模板 Binding：" + literalError;
					return false;
				}
				const auto installed = ownedBindingSource
					? target->DataBindings.Add(
						targetProperty, std::move(ownedBindingSource),
						binding.SourceProperty, binding.Mode, binding.UpdateMode,
						std::move(converter), std::move(fallbackValue),
						std::move(targetNullValue), std::move(converterParameter),
						binding.StringFormat)
					: target->DataBindings.Add(
						targetProperty, *bindingSource, binding.SourceProperty,
						binding.Mode, binding.UpdateMode, std::move(converter),
						std::move(fallbackValue), std::move(targetNullValue),
						std::move(converterParameter), binding.StringFormat);
				if (!installed)
				{
					if (outError) *outError = L"组件模板绑定创建失败："
						+ it.name + L"." + targetProperty;
					return false;
				}
			}
		}

		for (auto& it : items)
		{
			if (it.templateEventBindings.empty()) continue;
			const auto source = instOf.find(it.name);
			const auto ownerRecord = dcOf.find(it.templateOwner);
			auto* owner = source == instOf.end() || !source->second
				? nullptr : source->second->GetTemplatedParent();
			if (source == instOf.end() || ownerRecord == dcOf.end()
				|| !source->second || !owner || !ownerRecord->second
				|| ownerRecord->second->ControlInstance != owner)
			{
				if (outError) *outError = L"组件模板事件无法解析所属实例：" + it.name;
				return false;
			}
			for (const auto& [sourceEvent, componentEvent]
				: it.templateEventBindings)
			{
				const auto contract = std::find_if(
					ownerRecord->second->ComponentEvents.begin(),
					ownerRecord->second->ComponentEvents.end(),
					[&](const auto& candidate)
					{ return candidate.Name == componentEvent; });
				if (contract == ownerRecord->second->ComponentEvents.end())
				{
					if (outError) *outError = L"组件模板事件引用了不存在的契约："
						+ componentEvent;
					return false;
				}
				if (!ConnectTemplateEvent(
					*source->second, sourceEvent, *owner,
					*contract, outError)) return false;
			}
		}

		std::unordered_map<std::wstring, std::vector<Pending*>> childrenByParent;
		childrenByParent.reserve(items.size());
		std::vector<Pending*> roots;
		roots.reserve(items.size());
		for (auto& it : items)
		{
			if (it.parent.empty())
			{
				roots.push_back(&it);
				continue;
			}
			childrenByParent[it.parent].push_back(&it);
		}

		auto sortByOrder = [](std::vector<Pending*>& v) {
			std::stable_sort(v.begin(), v.end(), [](const Pending* a, const Pending* b) {
				return a->order < b->order;
			});
		};
		sortByOrder(roots);
		for (auto& kv : childrenByParent) sortByOrder(kv.second);

		std::unordered_set<std::wstring> attached;
		attached.reserve(items.size());

		// Attaching a templated control to its styled parent can change the
		// effective Template and clear its declarative template scope.  Register
		// the already materialized scope only after that owner attachment has
		// stabilized, immediately before SetTemplateRoot invokes presentation
		// callbacks which are allowed to query template parts.
		std::unordered_set<std::wstring> registeredTemplateScopes;
		registeredTemplateScopes.reserve(items.size());
		auto registerTemplateScope =
			[&](const std::wstring& ownerName) -> bool
			{
				if (registeredTemplateScopes.contains(ownerName)) return true;

				for (const auto& candidate : items)
				{
					if (!candidate.templateGenerated
						|| candidate.templateOwner != ownerName
						|| candidate.templatePartName.empty()) continue;
					const auto owner = instOf.find(ownerName);
					const auto part = instOf.find(candidate.name);
					if (owner == instOf.end() || part == instOf.end()
						|| !owner->second || !part->second
						|| !cui::framework::XamlAccess::RegisterTemplatePart(
							*owner->second, candidate.templatePartName,
							part->second))
					{
						if (outError) *outError = L"组件模板部件注册失败："
							+ ownerName + L"." + candidate.templatePartName;
						return false;
					}
				}

				for (const auto& candidate : items)
				{
					if (candidate.templateOwner != ownerName
						|| candidate.presentedContent.empty()) continue;
					const auto owner = dcOf.find(ownerName);
					const auto presenter = instOf.find(candidate.name);
					if (owner == dcOf.end() || !owner->second
						|| presenter == instOf.end() || !presenter->second)
						continue;
					owner->second->ComponentContentPresenters[
						candidate.presentedContent] = presenter->second;
					if (!owner->second->ControlInstance
						|| !cui::framework::XamlAccess::RegisterContentPresenter(
							*owner->second->ControlInstance,
							candidate.presentedContent, presenter->second))
					{
						if (outError) *outError =
							L"组件内容 Presenter 注册失败："
							+ ownerName + L"." + candidate.presentedContent;
						return false;
					}
				}

				for (const auto& candidate : items)
				{
					if (candidate.templateOwner != ownerName
						|| candidate.templateContentSource.empty()) continue;
					const auto owner = instOf.find(ownerName);
					const auto presenter = instOf.find(candidate.name);
					auto* contentPresenter = presenter == instOf.end()
						? nullptr
						: dynamic_cast<ContentPresenter*>(presenter->second);
					auto* contentHost = owner == instOf.end()
						? nullptr
						: dynamic_cast<ContentControl*>(owner->second);
					bool registered = false;
					if (candidate.templateContentSource == L"Content"
						&& contentHost)
						registered =
							cui::framework::TemplateAccess::
								RegisterContentPresenter(
									*contentHost, contentPresenter);
					else if (candidate.templateContentSource == L"Header")
						registered = owner != instOf.end()
							&& RegisterTemplateHeaderPresenter(
								owner->second, contentPresenter);
					if (!registered)
					{
						if (outError) *outError =
							L"ControlTemplate ContentSource 注册失败："
							+ ownerName + L"."
							+ candidate.templateContentSource;
						return false;
					}
				}

				for (const auto& candidate : items)
				{
					if (!candidate.templateGenerated
						|| candidate.templateOwner != ownerName
						|| candidate.type != UIClass::UI_ItemsPresenter)
						continue;
					const auto owner = instOf.find(ownerName);
					const auto presenter = instOf.find(candidate.name);
					auto* itemsControl = owner == instOf.end()
						? nullptr : dynamic_cast<ItemsControl*>(owner->second);
					auto* itemsPresenter = presenter == instOf.end()
						? nullptr
						: dynamic_cast<ItemsPresenter*>(presenter->second);
					if (!itemsControl || !itemsPresenter
						|| !cui::framework::TemplateAccess::
							RegisterItemsPresenter(
								*itemsControl, itemsPresenter))
					{
						if (outError) *outError =
							L"ControlTemplate ItemsPresenter 注册失败："
							+ ownerName + L"."
							+ candidate.templatePartName;
						return false;
					}
				}

				registeredTemplateScopes.insert(ownerName);
				return true;
			};

		auto attachOne = [&](Pending* it, Control* runtimeParent,
			Control* designerParent) -> bool
		{
			if (!it) return true;
			auto dc = dcOf[it->name];
			if (!dc || !dc->ControlInstance) return true;
			auto* c = dc->ControlInstance;
			if (it->borrowedTemplateOwner)
			{
				dc->DesignerParent = designerParent;
				attached.insert(it->name);
				return true;
			}
			if (!runtimeParent) runtimeParent = &stagingRoot;
			if (!runtimeParent) return true;
			if (it->controlTemplateRoot)
			{
				// Parent attachment may already have applied the effective
				// Theme template.  Tear down that transient instance first so
				// its namescope cannot collide with the compiler-expanded
				// initial tree.  Registration must still precede the new root
				// attachment because presentation callbacks resolve parts.
				if (cui::framework::TemplateAccess::GetTemplateRoot(
					*runtimeParent))
					(void)cui::framework::TemplateAccess::
						DetachTemplateRoot(*runtimeParent);
				if (!registerTemplateScope(it->templateOwner))
					return false;
			}
			auto owner = controlPool.TakeById(it->id);
			if (!owner || owner.get() != c) return true;
			// A ControlTemplate slot keeps the authored Header marker for
			// Designer capture, but its physical owner is the generated
			// ContentPresenter rather than the HeaderedContentControl.
			const bool isVisualHeader = it->contentOwner.empty()
				&& it->extra.is_object()
				&& it->extra.value(
					"headeredRegion", std::string{}) == "header";
			if (it->controlTemplateRoot)
			{
				if (auto* contentHost = dynamic_cast<ContentControl*>(runtimeParent))
					cui::framework::TemplateAccess::SetTemplateRoot(
						*contentHost, std::move(owner));
				else if (auto* itemsHost = dynamic_cast<ItemsControl*>(runtimeParent))
					cui::framework::TemplateAccess::SetTemplateRoot(
						*itemsHost, std::move(owner));
				else if (runtimeParent)
					cui::framework::TemplateAccess::SetTemplateRoot(
						*runtimeParent, std::move(owner));
				else
					throw std::logic_error(
						"ControlTemplate root parent is not a supported template host");
			}
			else if (isVisualHeader)
			{
				if (!SetVisualHeader(runtimeParent, std::move(owner)))
					throw std::logic_error(
						"Header visual parent is not a headered control");
			}
			else if (auto* itemsControl =
				dynamic_cast<ItemsControl*>(runtimeParent))
			{
				itemsControl->AddItemControl(std::move(owner));
			}
			else
			{
				runtimeParent->AddOwned(std::move(owner));
			}
			// ControlTemplate-generated visuals are never authored logical
			// children. Their WPF inheritance context follows the containing
			// visual tree; projected content is reassigned to its content owner
			// immediately below.
			if (it->templateGenerated && it->contentOwner.empty())
				cui::framework::XamlAccess::SetLogicalParent(*c, nullptr);
			if (!it->contentOwner.empty())
			{
				const auto logicalParent = instOf.find(it->contentOwner);
				if (logicalParent == instOf.end() || !logicalParent->second)
					throw std::logic_error(
						"Projected content is missing its logical owner");
				cui::framework::XamlAccess::SetLogicalParent(
					*c, logicalParent->second);
				dc->DesignerParent = logicalParent->second;
			}
			else dc->DesignerParent = designerParent;
			if (!it->templateGenerated)
				candidate.Controls.push_back(dc);
			attached.insert(it->name);
			return true;
		};

		std::function<bool(const std::wstring& parentKey,
			Control* runtimeParent, Control* designerParent)> attachChildren;
		attachChildren = [&](const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)
		{
			auto it = childrenByParent.find(parentKey);
			if (it == childrenByParent.end()) return true;
			for (auto* ch : it->second)
			{
				if (!attachOne(ch, runtimeParent, designerParent))
					return false;
				if (!attachChildren(ch->name,
					dcOf[ch->name]->ControlInstance,
					dcOf[ch->name]->ControlInstance))
					return false;
			}
			return true;
		};

		for (auto* it : roots)
		{
			if (!attachOne(it, &stagingRoot, nullptr))
				return false;
			if (!attachChildren(it->name,
				dcOf[it->name]->ControlInstance,
				dcOf[it->name]->ControlInstance))
				return false;
		}

		for (const auto& it : items)
		{
			if (!it.structure.RelativePanel
				|| it.structure.RelativePanel->Empty()) continue;
			const auto& value = *it.structure.RelativePanel;
			const auto child = instOf.find(it.name);
			const auto parent = instOf.find(it.parent);
			auto* relativePanel = parent == instOf.end()
				? nullptr : dynamic_cast<RelativePanel*>(parent->second);
			if (child == instOf.end() || !child->second || !relativePanel
				|| child->second->GetVisualParent() != relativePanel)
			{
				if (outError) *outError = L"控件 " + it.name
					+ L" 的 RelativePanel 约束只能应用于 "
						L"RelativePanel 的直接子控件。";
				return false;
			}
			RelativeConstraints constraints;
			const std::pair<const std::optional<bool>*, bool RelativeConstraints::*> booleans[] = {
				{ &value.CenterHorizontal, &RelativeConstraints::CenterHorizontal },
				{ &value.CenterVertical, &RelativeConstraints::CenterVertical },
				{ &value.AlignLeftWithPanel, &RelativeConstraints::AlignLeftWithPanel },
				{ &value.AlignTopWithPanel, &RelativeConstraints::AlignTopWithPanel },
				{ &value.AlignRightWithPanel, &RelativeConstraints::AlignRightWithPanel },
				{ &value.AlignBottomWithPanel, &RelativeConstraints::AlignBottomWithPanel }
			};
			const std::pair<const std::optional<std::wstring>*, Control* RelativeConstraints::*> references[] = {
				{ &value.Above, &RelativeConstraints::Above },
				{ &value.Below, &RelativeConstraints::Below },
				{ &value.LeftOf, &RelativeConstraints::LeftOf },
				{ &value.RightOf, &RelativeConstraints::RightOf },
				{ &value.AlignLeftWith, &RelativeConstraints::AlignLeftWith },
				{ &value.AlignRightWith, &RelativeConstraints::AlignRightWith },
				{ &value.AlignTopWith, &RelativeConstraints::AlignTopWith },
				{ &value.AlignBottomWith, &RelativeConstraints::AlignBottomWith }
			};
			for (const auto& [authored, member] : booleans)
			{
				if (*authored) constraints.*member = **authored;
			}
			for (const auto& [authored, member] : references)
			{
				if (!*authored) continue;
				const auto& targetName = **authored;
				const auto target = instOf.find(targetName);
				if (target == instOf.end() || !target->second
					|| target->second == child->second
					|| target->second->GetVisualParent() != relativePanel)
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的 RelativePanel 约束目标必须是同一面板的"
							L"直接兄弟：" + targetName;
					return false;
				}
				constraints.*member = target->second;
			}
			relativePanel->SetConstraints(child->second, constraints);
		}

		if (attached.size() != items.size()
			|| controlPool.PendingCount() != 0)
		{
			for (auto& it : items)
			{
				if (attached.find(it.name) == attached.end())
				{
					if (outError) *outError = L"无法解析控件父级引用，未能挂载控件: " + it.name;
					return false;
				}
			}
		}
		auto validateTemplateNameScopes =
			[&](const wchar_t* stage) -> bool
			{
				for (const auto& it : items)
				{
					if (!it.templateGenerated
						|| it.templateOwner.empty()
						|| it.templatePartName.empty()) continue;
					const auto owner = instOf.find(it.templateOwner);
					const auto part = instOf.find(it.name);
					if (owner != instOf.end() && part != instOf.end()
						&& owner->second && part->second
						&& owner->second->FindDeclarativeTemplatePart(
							it.templatePartName) == part->second)
						continue;
					if (outError)
						*outError = L"ControlTemplate 模板部件在 "
							+ std::wstring(stage)
							+ L" 阶段丢失："
							+ it.templateOwner + L"."
							+ it.templatePartName;
					return false;
				}
				return true;
			};
		if (!validateTemplateNameScopes(L"visual attach"))
			return false;

		std::shared_ptr<const ControlStyleSheet> runtimeThemeStyleSheet;
		if (compiled.Theme)
		{
			auto frameworkTheme = XamlFrameworkTheme::DefaultDocument();
			if (frameworkTheme.get() == compiled.Theme.get())
				runtimeThemeStyleSheet =
					XamlFrameworkTheme::DefaultStyleSheet(outError);
			else
			{
				std::shared_ptr<ControlStyleSheet> customThemeStyleSheet;
				auto structuralResources =
					DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
						compiled.Theme->ItemsPanelTemplates);
				auto controlTemplateResources =
					XamlObjectMaterializer::
						BuildControlTemplateStyleResources(
							compiled.Theme, nestedOptions);
				structuralResources.insert(
					structuralResources.end(),
					std::make_move_iterator(
						controlTemplateResources.begin()),
					std::make_move_iterator(
						controlTemplateResources.end()));
				const DesignerStyleSheetUtils::RulePropertySchemaResolver
					themeStyleSchemaResolver =
					[&](const DesignerStyleRule& rule,
						XamlTypePropertySchema& schema,
						std::wstring* error)
					{
						return buildRuntimeStylePropertySchema(
							*compiled.Theme, rule, schema, error);
					};
				auto runtimeThemeSource = compiled.Theme->StyleSheet;
				ProjectImplicitControlTemplateStyles(
					compiled.Theme->ControlTemplates, runtimeThemeSource);
				if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
					runtimeThemeSource, customThemeStyleSheet, outError,
					compiled.Theme->ResourceBasePath,
					compiled.Theme->Resources,
					structuralResources, themeStyleSchemaResolver))
					return false;
				runtimeThemeStyleSheet = std::move(customThemeStyleSheet);
			}
			if (!runtimeThemeStyleSheet) return false;
		}

		std::shared_ptr<ControlStyleSheet> runtimeStyleSheet;
		auto documentStructuralResources =
			DesignerStyleSheetUtils::BuildItemsPanelStyleResources(
				document.ItemsPanelTemplates);
		auto documentControlTemplateResources =
			XamlObjectMaterializer::BuildControlTemplateStyleResources(
				templateDocument, nestedOptions);
		documentStructuralResources.insert(
			documentStructuralResources.end(),
			std::make_move_iterator(
				documentControlTemplateResources.begin()),
			std::make_move_iterator(
				documentControlTemplateResources.end()));
		auto runtimeDocumentStyleSource = document.StyleSheet;
		ProjectImplicitControlTemplateStyles(
			document.ControlTemplates, runtimeDocumentStyleSource);
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			runtimeDocumentStyleSource, runtimeStyleSheet, outError,
			document.ResourceBasePath, document.Resources,
			documentStructuralResources, documentStyleSchemaResolver))
			return false;
		const bool hasThemeStyles = compiled.Theme
			&& !compiled.Theme->StyleSheet.Empty();
		const bool hasDocumentStyles = !document.StyleSheet.Empty()
			|| !documentStructuralResources.empty();
		// Install both sheets as one XAML initialization transaction. A keyed
		// ToolBar Style is a dynamic resource reference, so applying Theme alone
		// first could transiently replace a document override's already-expanded
		// template and destroy its namescope before document Style is attached.
		if ((hasThemeStyles || hasDocumentStyles)
			&& !cui::framework::StyleAccess::SetEnvironment(
				stagingRoot,
				hasThemeStyles ? runtimeThemeStyleSheet : nullptr,
				hasDocumentStyles ? runtimeStyleSheet : nullptr,
				true))
		{
			if (outError) *outError =
				L"框架主题/文档样式环境无法应用到完整控件树。";
			return false;
		}
		if (!validateTemplateNameScopes(L"Style environment apply"))
			return false;
		candidate.DocumentStyleSheet = runtimeStyleSheet;

		for (auto& it : items)
		{
			if (it.componentType.Empty()
				|| (it.source && !it.source->TemplateState.
					AppliedControlTemplate.empty())) continue;
			const auto host = instOf.find(it.name);
			const auto* component = it.source
				? document.FindComponent(document.Nodes, *it.source, it.componentType)
				: document.FindComponent(it.componentType);
			if (host == instOf.end() || !host->second || !component)
			{
				if (outError) *outError = L"组件视觉状态无法解析宿主：" + it.name;
				return false;
			}
			if (!InstallComponentVisualStatesCore(
				*host->second, *component, document, outError)) return false;
		}

		for (auto& it : items)
		{
			if (!it.source || it.source->TemplateState.
				AppliedControlTemplate.empty()) continue;
			const bool fromTheme = it.source->TemplateState.
				AppliedControlTemplateFromTheme;
			const auto* resourceDocument = fromTheme
				? compiled.Theme.get() : &document;
			if (!resourceDocument)
			{
				if (outError) *outError =
					L"ControlTemplate 主题来源已丢失：" + it.name;
				return false;
			}
			const DesignerModel::DesignControlTemplate* definition = nullptr;
			const auto& resourceKey = it.source->TemplateState.
				AppliedControlTemplateResource;
			if (!resourceKey.empty())
				definition = fromTheme
					? resourceDocument->FindControlTemplate(resourceKey)
					: resourceDocument->FindControlTemplate(
						document.Nodes, *it.source, resourceKey);
			else definition = fromTheme
				? (it.componentType.Empty()
					? resourceDocument->FindImplicitControlTemplate(it.type)
					: resourceDocument->FindImplicitControlTemplate(
						it.componentType))
				: (it.componentType.Empty()
					? resourceDocument->FindImplicitControlTemplate(
						document.Nodes, *it.source, it.type)
					: resourceDocument->FindImplicitControlTemplate(
						document.Nodes, *it.source, it.componentType));
			const auto host = instOf.find(it.name);
			if (!definition || host == instOf.end() || !host->second)
			{
				if (outError) *outError =
					L"ControlTemplate 视觉状态无法解析宿主：" + it.name;
				return false;
			}
			if (!XamlObjectMaterializer::InstallControlTemplateVisualStates(
				*host->second, *definition,
				*resourceDocument, outError)) return false;
		}

		if (!options.ExistingTemplateOwner)
		{
			for (const auto& it : items)
			{
				if (!it.source || it.source->TemplateState.
					AppliedControlTemplate.empty()) continue;
				const auto host = instOf.find(it.name);
				if (host != instOf.end() && host->second)
					cui::framework::TemplateAccess::
						CompleteTemplateApplication(*host->second);
			}
		}

		if (options.DeclarativeComponentBehaviorFactory)
		{
			// Attach inner template components before their owning component so an
			// outer behavior observes a fully initialized visual subtree.
			for (auto position = items.rbegin(); position != items.rend(); ++position)
			{
				const auto& it = *position;
				if (it.componentType.Empty()
					|| it.borrowedTemplateOwner) continue;
				const auto host = instOf.find(it.name);
				if (host == instOf.end() || !host->second)
				{
					if (outError) *outError = L"组件 Behavior 无法解析宿主：" + it.name;
					return false;
				}
				DeclarativeComponentBehaviorContext context{
					*host->second, it.id, it.name,
					host->second->GetDeclarativeTypeId() };
				std::unique_ptr<IDeclarativeComponentBehavior> behavior;
				try
				{
					behavior = options.DeclarativeComponentBehaviorFactory(context);
				}
				catch (...)
				{
					if (outError) *outError = L"组件 Behavior 工厂抛出异常："
						+ it.componentType.XamlName + L"（" + it.name + L"）";
					return false;
				}
				if (!behavior) continue;
				std::wstring behaviorError;
				if (!cui::framework::XamlAccess::SetComponentBehavior(
					*host->second, std::move(behavior), context, &behaviorError))
				{
					if (outError) *outError = L"组件 Behavior 附加失败："
						+ it.componentType.XamlName + L"（" + it.name + L"）："
						+ behaviorError;
					return false;
				}
			}
		}
		if (stagingRoot.VisualChildCount() > 1)
		{
			if (outError) *outError =
				L"Window 只能物化一个 Content；规范文档不能包含多个顶层节点。";
			return false;
		}
		if (stagingRoot.VisualChildCount() == 1)
		{
			auto root = stagingRoot.DetachVisualChildAt(0);
			if (!root)
			{
				if (outError) *outError =
					L"物化完成后无法分离 Content。";
				return false;
			}
			candidate.ContentRoot = std::move(root);
		}

		output = std::move(candidate);
		if (outError) outError->clear();
		completed = true;
		if (outDiagnostic) *outDiagnostic = {};
		return true;
	}
	catch (const std::exception& expander)
	{
		if (outError) *outError = L"加载失败: " + FromUtf8(expander.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"加载失败：未知错误。";
		return false;
	}
}

bool CuiRuntime::XamlObjectMaterializer::MaterializeDeclarativeInteractions(
	Control& owner,
	const std::vector<DesignerVisualStateGroup>& visualStateGroups,
	const std::vector<DesignerEventTrigger>& eventTriggers,
	const DesignerModel::DesignDocument& resourceDocument,
	std::vector<DeclarativeVisualStateGroupDefinition>& outVisualStateGroups,
	std::vector<DeclarativeEventTriggerDefinition>& outEventTriggers,
	std::wstring* outError)
{
	return MaterializeDeclarativeInteractionsCore(
		&owner, visualStateGroups, eventTriggers, resourceDocument,
		outVisualStateGroups, outEventTriggers, outError);
}

bool CuiRuntime::XamlObjectMaterializer::MaterializeDeclarativeInteractions(
	const std::vector<DesignerVisualStateGroup>& visualStateGroups,
	const std::vector<DesignerEventTrigger>& eventTriggers,
	const DesignerModel::DesignDocument& resourceDocument,
	std::vector<DeclarativeVisualStateGroupDefinition>& outVisualStateGroups,
	std::vector<DeclarativeEventTriggerDefinition>& outEventTriggers,
	std::wstring* outError)
{
	return MaterializeDeclarativeInteractionsCore(
		nullptr, visualStateGroups, eventTriggers, resourceDocument,
		outVisualStateGroups, outEventTriggers, outError);
}

bool CuiRuntime::XamlObjectMaterializer::InstallControlTemplateVisualStates(
	Control& owner,
	const DesignControlTemplate& definition,
	const DesignDocument& resourceDocument,
	std::wstring* outError)
{
	std::vector<DeclarativeVisualStateGroupDefinition> groups;
	std::vector<DeclarativeEventTriggerDefinition> eventTriggers;
	if (!MaterializeDeclarativeInteractionsCore(
		&owner, definition.VisualStateGroups, definition.EventTriggers,
		resourceDocument, groups, eventTriggers, outError)) return false;
	if (groups.empty() && eventTriggers.empty()) return true;
	std::wstring stateError;
	if (!cui::framework::XamlAccess::DefineInteractions(
		owner, std::move(groups), std::move(eventTriggers), &stateError))
	{
		if (outError) *outError =
			L"ControlTemplate 声明交互无效：" + stateError;
		return false;
	}
	if (outError) outError->clear();
	return true;
}
