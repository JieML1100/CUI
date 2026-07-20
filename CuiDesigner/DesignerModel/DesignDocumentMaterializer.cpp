#include "DesignDocumentMaterializer.h"

#include "DesignDocumentControlPool.h"
#include "DesignDataResourceUtils.h"
#include "DesignDocumentGraph.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/Panel.h"
#include "../../CUI/include/Button.h"
#include "../../CUI/include/CheckBox.h"
#include "../../CUI/include/ComboBox.h"
#include "../../CUI/include/DateTimePicker.h"
#include "../../CUI/include/Expander.h"
#include "../../CUI/include/GroupBox.h"
#include "../../CUI/include/Label.h"
#include "../../CUI/include/LinkLabel.h"
#include "../../CUI/include/LoadingRing.h"
#include "../../CUI/include/NumericUpDown.h"
#include "../../CUI/include/PasswordBox.h"
#include "../../CUI/include/PictureBox.h"
#include "../../CUI/include/ProgressBar.h"
#include "../../CUI/include/ProgressRing.h"
#include "../../CUI/include/RadioBox.h"
#include "../../CUI/include/RichTextBox.h"
#include "../../CUI/include/ScrollView.h"
#include "../../CUI/include/Slider.h"
#include "../../CUI/include/Switch.h"
#include "../../CUI/include/TextBox.h"
#include "../../CUI/include/WebBrowser.h"
#include "../../CUI/include/ListView.h"
#include "../../CUI/include/ListBox.h"
#include "../../CUI/include/Selector.h"
#include "../../CUI/include/GridView.h"
#include "../../CUI/include/PropertyGrid.h"
#include "../../CUI/include/ChartView.h"
#include "../../CUI/include/ReportView.h"
#include "../../CUI/include/KpiCard.h"
#include "../../CUI/include/FilterBar.h"
#include "../../CUI/include/TreeView.h"
#include "../../CUI/include/TabControl.h"
#include "../../CUI/include/ToolBar.h"
#include "../../CUI/include/Menu.h"
#include "../../CUI/include/StatusBar.h"
#include "../../CUI/include/Toast.h"
#include "../../CUI/include/MediaPlayer.h"
#include "../../CUI/include/NativeSurface.h"
#include "../../CUI/include/ItemsControl.h"
#include "../../CUI/include/ItemsPresenter.h"
#include "../../CUI/include/ContentPresenter.h"
#include "../../CUI/include/ContentControl.h"
#include "../../CUI/include/HeaderedContentControl.h"
#include "../../CUI/include/NavigationView.h"
#include "../../CUI/include/CalendarView.h"
#include "../../CUI/include/ColorPicker.h"
#include "../../CUI/include/PagedGridView.h"
#include "../../CUI/include/SplitContainer.h"
#include "../../CUI/include/Layout/StackPanel.h"
#include "../../CUI/include/Layout/GridPanel.h"
#include "../../CUI/include/Layout/DockPanel.h"
#include "../../CUI/include/Layout/WrapPanel.h"
#include "../../CUI/include/Layout/RelativePanel.h"
#include <Convert.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <exception>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using DesignValue = DesignerModel::DesignValue;

namespace
{
	constexpr const char* TemplateGeneratedKey = "$componentTemplateGenerated";
	constexpr const char* TemplateOwnerKey = "$componentTemplateOwner";
	constexpr const char* TemplateContentOwnerKey = "$componentContentOwner";
	constexpr const char* TemplatePartNameKey = "$componentTemplatePartName";
	constexpr const char* ComponentTemplateExpandedKey = "$componentTemplateExpanded";
	constexpr const char* ControlTemplateExpandedKey = "$controlTemplateExpanded";
	constexpr const char* ControlTemplateRootKey = "$controlTemplateRoot";
	constexpr const char* AppliedControlTemplateKey = "$appliedControlTemplate";
	constexpr const char* AppliedControlTemplateResourceKey =
		"$appliedControlTemplateResource";
	constexpr const char* ControlTemplateChainKey = "$controlTemplateChain";

	static bool IsComponentContentPresenterType(UIClass type) noexcept
	{
		switch (type)
		{
		case UIClass::UI_Panel:
		case UIClass::UI_StackPanel:
		case UIClass::UI_WrapPanel:
		case UIClass::UI_DockPanel:
		case UIClass::UI_GridPanel:
		case UIClass::UI_RelativePanel:
			return true;
		default:
			return false;
		}
	}

	static bool IsContentHostType(UIClass type) noexcept
	{
		return type == UIClass::UI_ContentPresenter
			|| type == UIClass::UI_ContentControl
			|| type == UIClass::UI_SelectorItem
			|| type == UIClass::UI_ComboBoxItem
			|| type == UIClass::UI_TreeViewItem
			|| type == UIClass::UI_Button
			|| type == UIClass::UI_GroupBox
			|| type == UIClass::UI_Expander;
	}

	static bool IsHeaderedContentControlType(UIClass type) noexcept
	{
		return type == UIClass::UI_GroupBox
			|| type == UIClass::UI_Expander
			|| type == UIClass::UI_TreeViewItem;
	}

	static bool IsControlTemplateHostType(UIClass type) noexcept
	{
		return type == UIClass::UI_ContentControl
			|| type == UIClass::UI_SelectorItem
			|| type == UIClass::UI_ComboBoxItem
			|| type == UIClass::UI_TreeViewItem
			|| type == UIClass::UI_Button
			|| type == UIClass::UI_GroupBox
			|| type == UIClass::UI_Expander
			|| type == UIClass::UI_ItemsControl
			|| type == UIClass::UI_ListBox;
	}

	static bool IsControlTemplateTargetCompatible(
		UIClass actual, UIClass target) noexcept
	{
		return actual == target
			|| (target == UIClass::UI_ContentControl
				&& (actual == UIClass::UI_SelectorItem
					|| actual == UIClass::UI_ComboBoxItem
					|| actual == UIClass::UI_TreeViewItem
					|| actual == UIClass::UI_Button
					|| actual == UIClass::UI_GroupBox
					|| actual == UIClass::UI_Expander))
			|| (target == UIClass::UI_ItemsControl
				&& actual == UIClass::UI_ListBox);
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
				{ return _wcsicmp(candidate.Name.c_str(), node.ParentRef.c_str()) == 0; });
			if (found != nodes.end()) return &*found;
		}
		return nullptr;
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

	static bool HasStyleClass(
		const DesignerModel::DesignNode& node,
		const std::wstring& expected)
	{
		if (!node.Props.is_object() || !node.Props.contains("styleClasses")
			|| !node.Props["styleClasses"].is_array()) return false;
		return std::any_of(node.Props["styleClasses"].begin(),
			node.Props["styleClasses"].end(), [&](const auto& item)
			{
				return item.is_string()
					&& _wcsicmp(Convert::Utf8ToUnicode(
						item.get<std::string>()).c_str(), expected.c_str()) == 0;
			});
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
		if (!rule.Id.empty())
		{
			const auto id = node.Props.is_object()
				&& node.Props.contains("styleId")
				&& node.Props["styleId"].is_string()
				? Convert::Utf8ToUnicode(
					node.Props["styleId"].get<std::string>())
				: std::wstring{};
			if (_wcsicmp(id.c_str(), rule.Id.c_str()) != 0) return false;
		}
		return std::all_of(rule.Classes.begin(), rule.Classes.end(),
			[&](const auto& item) { return HasStyleClass(node, item); });
	}

	static bool ResolveStyledControlTemplateKey(
		const DesignerModel::DesignDocument& document,
		const std::vector<DesignerModel::DesignNode>& nodes,
		const DesignerModel::DesignNode& owner,
		std::wstring& key,
		std::wstring* outError)
	{
		key.clear();
		DesignerStyleSheet resolved;
		if (!DesignerStyleSheetUtils::ResolveInheritance(
			VisibleStyleSheet(document, nodes, owner), resolved, outError))
			return false;
		uint32_t winningSpecificity = 0;
		size_t winningOrder = 0;
		bool found = false;
		for (size_t order = 0; order < resolved.Rules.size(); ++order)
		{
			const auto& rule = resolved.Rules[order];
			if (!StyleRuleMatchesNode(rule, owner)) continue;
			const auto setter = std::find_if(
				rule.Setters.begin(), rule.Setters.end(), [](const auto& candidate)
				{ return _wcsicmp(candidate.PropertyName.c_str(), L"Template") == 0; });
			if (setter == rule.Setters.end()) continue;
			if (!setter->UsesResource || setter->UsesDynamicResource
				|| DesignerBindingUtils::Trim(setter->ResourceKey).empty())
			{
				if (outError) *outError = L"Style.Template 必须使用 StaticResource："
					+ owner.Name;
				return false;
			}
			const uint32_t specificity = (!rule.Id.empty() ? 1'000'000u : 0u)
				+ static_cast<uint32_t>(rule.Classes.size()) * 1'000u
				+ ((!rule.ComponentType.Empty()
					|| (rule.HasType && rule.Type != UIClass::UI_Base)) ? 1u : 0u);
			if (!found || specificity > winningSpecificity
				|| (specificity == winningSpecificity && order >= winningOrder))
			{
				key = DesignerBindingUtils::Trim(setter->ResourceKey);
				winningSpecificity = specificity;
				winningOrder = order;
				found = true;
			}
		}
		return true;
	}

	struct EffectiveControlTemplate
	{
		const DesignerModel::DesignControlTemplate* Definition = nullptr;
		std::wstring ResourceKey;
	};

	static bool ResolveEffectiveControlTemplate(
		const DesignerModel::DesignDocument& document,
		const std::vector<DesignerModel::DesignNode>& nodes,
		const DesignerModel::DesignNode& owner,
		EffectiveControlTemplate& result,
		std::wstring* outError)
	{
		result = {};
		if (owner.Extra.is_object() && owner.Extra.contains("controlTemplate"))
		{
			if (!owner.Extra["controlTemplate"].is_string())
			{
				if (outError) *outError = L"Control.Template 格式无效：" + owner.Name;
				return false;
			}
			result.ResourceKey = Convert::Utf8ToUnicode(
				owner.Extra["controlTemplate"].get<std::string>());
		}
		else if (!ResolveStyledControlTemplateKey(
			document, nodes, owner, result.ResourceKey, outError)) return false;

		if (!result.ResourceKey.empty())
		{
			result.Definition = document.FindControlTemplate(
				nodes, owner, result.ResourceKey);
			if (!result.Definition)
			{
				if (outError) *outError = L"控件 " + owner.Name
					+ L" 引用了不存在的 ControlTemplate："
					+ result.ResourceKey;
				return false;
			}
		}
		else result.Definition = owner.ComponentType.Empty()
			? document.FindImplicitControlTemplate(nodes, owner, owner.Type)
			: document.FindImplicitControlTemplate(
				nodes, owner, owner.ComponentType);

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

	static bool ExpandComponentTemplates(
		const DesignerModel::DesignDocument& source,
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
			if (output.Nodes[instanceIndex].Extra.is_object()
				&& output.Nodes[instanceIndex].Extra.value(
					ComponentTemplateExpandedKey, false)) return true;
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
				output, output.Nodes, output.Nodes[instanceIndex],
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
				const auto inheritedChain = output.Nodes[instanceIndex].Extra.is_object()
					? Convert::Utf8ToUnicode(
						output.Nodes[instanceIndex].Extra.value(
							ControlTemplateChainKey, std::string{}))
					: std::wstring{};
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
							return _wcsicmp(content.Name.c_str(),
								candidate.ComponentContentProperty.c_str()) == 0;
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
						return _wcsicmp(output.Nodes[index].ComponentContentProperty.c_str(),
							contract.Name.c_str()) == 0;
					});
				if (contract.Cardinality ==
					DesignerComponentContentCardinality::Single && count > 1)
				{
					if (outError) *outError = L"组件单值内容属性包含多个视觉根："
						+ contract.Name;
					return false;
				}
			}
			if (!output.Nodes[instanceIndex].Extra.is_object())
				output.Nodes[instanceIndex].Extra = DesignValue::object();
			output.Nodes[instanceIndex].Extra[ComponentTemplateExpandedKey] = true;
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
				output.Nodes[instanceIndex].Extra[ControlTemplateExpandedKey] = true;
				output.Nodes[instanceIndex].Extra[AppliedControlTemplateKey] =
					Convert::UnicodeToUtf8(
						ControlTemplateIdentity(*controlTemplate));
				output.Nodes[instanceIndex].Extra[
					AppliedControlTemplateResourceKey] = Convert::UnicodeToUtf8(
						effectiveTemplate.ResourceKey);
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
				if (generated.Bindings.is_object())
					for (auto& [targetProperty, binding]
						: generated.Bindings.ObjectItems())
					{
						(void)targetProperty;
						std::wstring bindingError;
						if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
							binding, [&](DesignerModel::DesignValue& child)
							{
								if (!child.contains("elementName")
									|| !child["elementName"].is_string()) return true;
								const auto authoredName = Convert::Utf8ToUnicode(
									child["elementName"].get<std::string>());
								const auto sourceName = nameMap.find(authoredName);
								if (sourceName == nameMap.end())
								{
									bindingError = L"组件模板 ElementName 引用了作用域外控件："
										+ authoredName;
									return false;
								}
								child["elementName"] = Convert::UnicodeToUtf8(
									sourceName->second);
								return true;
							}, &bindingError))
						{
							if (outError) *outError = bindingError;
							return false;
						}
					}
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
				}
				if (!generated.Extra.is_object())
					generated.Extra = DesignValue::object();
				generated.Extra[TemplateGeneratedKey] = true;
				generated.Extra[TemplateOwnerKey] =
					Convert::UnicodeToUtf8(instanceName);
				generated.Extra[TemplatePartNameKey] =
					Convert::UnicodeToUtf8(local.Name);
				if (controlTemplate)
					generated.Extra[ControlTemplateChainKey] =
						Convert::UnicodeToUtf8(controlTemplateChain);
				output.Nodes.push_back(std::move(generated));
				if (!local.PresentedComponentContent.empty())
				{
					const auto contract = std::find_if(
						component->ContentProperties.begin(),
						component->ContentProperties.end(), [&](const auto& content)
						{
							return _wcsicmp(content.Name.c_str(),
								local.PresentedComponentContent.c_str()) == 0;
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
							return _wcsicmp(entry.first.c_str(),
								contract->Name.c_str()) == 0;
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
						return _wcsicmp(entry.first.c_str(),
							child.ComponentContentProperty.c_str()) == 0;
					});
				if (presenter == presenters.end())
				{
					if (outError) *outError = L"组件内容属性缺少模板 Presenter："
						+ child.ComponentContentProperty;
					return false;
				}
				child.ParentId = presenter->second.first;
				child.ParentRef = presenter->second.second;
				if (!child.Extra.is_object()) child.Extra = DesignValue::object();
				child.Extra[TemplateContentOwnerKey] =
					Convert::UnicodeToUtf8(instanceName);
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
			if (!owner.Extra.is_object()) owner.Extra = DesignValue::object();
			if (owner.Extra.value(ControlTemplateExpandedKey, false)) continue;

			EffectiveControlTemplate effectiveTemplate;
			if (!ResolveEffectiveControlTemplate(
				output, output.Nodes, owner, effectiveTemplate, outError))
				return false;

			owner.Extra[ControlTemplateExpandedKey] = true;
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
			const auto inheritedChain = Convert::Utf8ToUnicode(
				owner.Extra.value(ControlTemplateChainKey, std::string{}));
			const auto marker = L"|" + identity + L"|";
			if (inheritedChain.find(marker) != std::wstring::npos)
			{
				if (outError) *outError = L"ControlTemplate 存在递归引用："
					+ controlTemplate->DisplayName();
				return false;
			}
			const auto chain = inheritedChain + marker;
			owner.Extra[AppliedControlTemplateKey] =
				Convert::UnicodeToUtf8(identity);
			owner.Extra[AppliedControlTemplateResourceKey] =
				Convert::UnicodeToUtf8(effectiveTemplate.ResourceKey);

			const auto ownerName = owner.Name;
			const auto ownerId = owner.Id;
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
					const bool itemsHost = owner.Type == UIClass::UI_ItemsControl
						|| owner.Type == UIClass::UI_ListBox;
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
				if (generated.Bindings.is_object())
					for (auto& [targetProperty, binding]
						: generated.Bindings.ObjectItems())
					{
						(void)targetProperty;
						std::wstring bindingError;
						if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
							binding, [&](DesignerModel::DesignValue& child)
							{
								if (!child.contains("elementName")
									|| !child["elementName"].is_string()) return true;
								const auto authoredName = Convert::Utf8ToUnicode(
									child["elementName"].get<std::string>());
								const auto sourceName = nameMap.find(authoredName);
								if (sourceName == nameMap.end())
								{
									bindingError = L"ControlTemplate ElementName 引用了作用域外控件："
										+ authoredName;
									return false;
								}
								child["elementName"] = Convert::UnicodeToUtf8(
									sourceName->second);
								return true;
							}, &bindingError))
						{
							if (outError) *outError = bindingError;
							return false;
						}
					}
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
				if (!generated.Extra.is_object())
					generated.Extra = DesignValue::object();
				generated.Extra[TemplateGeneratedKey] = true;
				generated.Extra[TemplateOwnerKey] =
					Convert::UnicodeToUtf8(ownerName);
				generated.Extra[TemplatePartNameKey] =
					Convert::UnicodeToUtf8(local.Name);
				generated.Extra[ControlTemplateChainKey] =
					Convert::UnicodeToUtf8(chain);
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
					generated.Extra[ControlTemplateRootKey] = true;
				output.Nodes.push_back(std::move(generated));
			}

			if (owner.Type == UIClass::UI_ItemsControl
				|| owner.Type == UIClass::UI_ListBox) continue;

			for (size_t childIndex = 0; childIndex < sourceNodeCount; ++childIndex)
			{
				if (childIndex == ownerIndex) continue;
				auto& child = output.Nodes[childIndex];
				if (child.ParentId != ownerId && child.ParentRef != ownerName) continue;
				const bool isHeader = child.Extra.is_object()
					&& child.Extra.value(
						"headeredRegion", std::string{}) == "header";
				const auto sourceName = isHeader
					? std::wstring(L"Header") : std::wstring(L"Content");
				const auto presenter = contentPresenters.find(sourceName);
				if (presenter == contentPresenters.end())
				{
					if (outError) *outError = L"ControlTemplate 必须用 ContentPresenter ContentSource=\""
						+ sourceName + L"\" 承载视觉内容：" + ownerName;
					return false;
				}
				child.ParentId = presenter->second.first;
				child.ParentRef = presenter->second.second;
				if (!child.Extra.is_object()) child.Extra = DesignValue::object();
				child.Extra[TemplateContentOwnerKey] =
					Convert::UnicodeToUtf8(ownerName);
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

	static bool InstallComponentContractCore(
		Control& control,
		const DesignerModel::DesignComponentDefinition& component,
		const DesignerModel::DesignDocument& document,
		std::wstring* outError)
	{
		bool foundDefaultContent = false;
		std::vector<std::wstring> contentNames;
		for (const auto& content : component.ContentProperties)
		{
			if (content.Name.empty()
				|| std::any_of(contentNames.begin(), contentNames.end(),
					[&](const auto& existing)
					{
						return _wcsicmp(existing.c_str(), content.Name.c_str()) == 0;
					})
				|| control.FindPropertyMetadata(content.Name)
				|| std::any_of(component.Properties.begin(), component.Properties.end(),
					[&](const auto& property)
					{
						return _wcsicmp(property.Name.c_str(), content.Name.c_str()) == 0;
					})
				|| std::any_of(component.Events.begin(), component.Events.end(),
					[&](const auto& event)
					{
						return _wcsicmp(event.Name.c_str(), content.Name.c_str()) == 0;
					})
				|| (content.IsDefault && foundDefaultContent))
			{
				if (outError) *outError = L"组件内容属性契约无效：" + content.Name;
				return false;
			}
			contentNames.push_back(content.Name);
			foundDefaultContent = foundDefaultContent || content.IsDefault;
		}
		for (const auto& property : component.Properties)
		{
			const auto kind = ComponentBindingKind(property.DefaultValue.Kind);
			if (!kind)
			{
				if (outError) *outError = L"组件属性 " + property.Name
					+ L" 使用了尚未进入动态属性契约的类型。";
				return false;
			}
			const DesignerStyleValue* source = &property.DefaultValue;
			if (!property.DefaultResourceKey.empty())
			{
				const auto resource = std::find_if(
					document.StyleSheet.Resources.begin(),
					document.StyleSheet.Resources.end(),
					[&](const auto& item)
					{
						return _wcsicmp(item.Key.c_str(),
							property.DefaultResourceKey.c_str()) == 0;
					});
				if (resource != document.StyleSheet.Resources.end())
					source = &resource->Value;
				if (source->Kind != property.DefaultValue.Kind)
				{
					if (outError) *outError = L"组件属性默认资源类型与声明 Type 不一致："
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
			DynamicControlPropertyDefinition definition;
			definition.Name = property.Name;
			definition.ValueKind = *kind;
			definition.DefaultValue = std::move(defaultValue);
			definition.Flags = property.Flags;
			definition.DefaultUpdateMode = property.DefaultUpdateMode;
			definition.IsReadOnly = property.IsReadOnly;
			if (HasControlPropertyFlag(
				property.Flags, ControlPropertyFlags::Inherits))
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
			definition.Design.Persistence = ControlPropertyPersistence::Metadata;
			std::wstring definitionError;
			if (!control.DefineDynamicProperty(
				std::move(definition), &definitionError))
			{
				if (outError) *outError = L"组件属性 " + property.Name
					+ L" 无法安装：" + definitionError;
				return false;
			}
		}
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
				if (outError) *outError = L"组件事件 payload 无效：" + event.Name;
				return false;
			}
			std::wstring definitionError;
			DynamicControlEventDefinition definition;
			definition.Name = event.Name;
			definition.PayloadKind = payloadKind;
			definition.OwnerNamespace = component.Type.XamlNamespace;
			definition.OwnerTypeName = component.Type.XamlName;
			definition.RoutingStrategy = event.RoutingStrategy;
			if (!control.DefineDynamicEvent(
				std::move(definition), &definitionError))
			{
				if (outError) *outError = L"组件事件 " + event.Name
					+ L" 无法安装：" + definitionError;
				return false;
			}
		}
		return true;
	}

	static bool InstallComponentVisualStatesCore(
		Control& control,
		const DesignerModel::DesignComponentDefinition& component,
		const DesignerModel::DesignDocument& document,
		std::wstring* outError)
	{
		if (component.VisualStateGroups.empty()
			&& component.EventTriggers.empty()) return true;
		std::vector<DeclarativeVisualStateGroupDefinition> groups;
		groups.reserve(component.VisualStateGroups.size());
		std::vector<DeclarativeEventTriggerDefinition> eventTriggers;
		eventTriggers.reserve(component.EventTriggers.size());
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
			animation.PropertyName = source.PropertyName;
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
						{ return _wcsicmp(candidate.Key.c_str(),
							resourceKey.c_str()) == 0; });
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
		for (const auto& sourceGroup : component.VisualStateGroups)
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
					condition.PropertyName = sourceCondition.PropertyName;
					if (!convert(sourceCondition.Value, condition.Value,
						L"视觉状态条件 " + sourceState.Name + L"."
							+ sourceCondition.PropertyName)) return false;
					state.Conditions.push_back(std::move(condition));
				}
				for (const auto& sourceSetter : sourceState.Setters)
				{
					DeclarativeVisualStateSetter setter;
					setter.TargetName = sourceSetter.TargetName;
					setter.PropertyName = sourceSetter.PropertyName;
					const DesignerStyleValue* value = &sourceSetter.Literal;
					if (sourceSetter.UsesResource)
					{
						const auto resource = std::find_if(
							document.StyleSheet.Resources.begin(),
							document.StyleSheet.Resources.end(),
							[&](const auto& candidate)
							{
								return _wcsicmp(candidate.Key.c_str(),
									sourceSetter.ResourceKey.c_str()) == 0;
							});
						if (resource != document.StyleSheet.Resources.end())
							value = &resource->Value;
					}
					if (!convert(*value, setter.Value,
						L"视觉状态 Setter " + sourceState.Name + L"."
							+ sourceSetter.PropertyName)) return false;
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
					animation.PropertyName = sourceAnimation.PropertyName;
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
								{ return _wcsicmp(candidate.Key.c_str(),
									resourceKey.c_str()) == 0; });
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
		for (const auto& sourceTrigger : component.EventTriggers)
		{
			DeclarativeEventTriggerDefinition trigger;
			trigger.EventName = sourceTrigger.EventName;
			trigger.Actions.reserve(sourceTrigger.Actions.size());
			for (const auto& sourceAction : sourceTrigger.Actions)
			{
				DeclarativeEventTriggerActionDefinition action;
				action.Kind = sourceAction.Kind
					== DesignerStoryboardActionKind::Begin
					? DeclarativeStoryboardActionKind::Begin
					: sourceAction.Kind == DesignerStoryboardActionKind::Pause
						? DeclarativeStoryboardActionKind::Pause
						: sourceAction.Kind == DesignerStoryboardActionKind::Resume
							? DeclarativeStoryboardActionKind::Resume
							: DeclarativeStoryboardActionKind::Stop;
				action.StoryboardName = sourceAction.StoryboardName;
				action.Animations.reserve(sourceAction.Animations.size());
				for (const auto& sourceAnimation : sourceAction.Animations)
				{
					DeclarativeVisualStateAnimation animation;
					if (!materializeAnimation(sourceAnimation,
						L"EventTrigger " + sourceTrigger.EventName,
						animation)) return false;
					action.Animations.push_back(std::move(animation));
				}
				trigger.Actions.push_back(std::move(action));
			}
			eventTriggers.push_back(std::move(trigger));
		}
		std::wstring stateError;
		if (!control.DefineDeclarativeInteractions(
			std::move(groups), std::move(eventTriggers), &stateError))
		{
			if (outError) *outError = L"组件 " + component.Type.XamlName
				+ L" 的声明交互无效：" + stateError;
			return false;
		}
		return true;
	}

	static bool IsSplitContainerControl(Control* control)
	{
		return control && control->Type() == UIClass::UI_SplitContainer;
	}

	static SplitContainer* AsSplitContainer(Control* control)
	{
		return IsSplitContainerControl(control) ? (SplitContainer*)control : nullptr;
	}

	static void RefreshDesignerPanelLayout(Control* control)
	{
		if (!control) return;
		if (auto* split = dynamic_cast<SplitContainer*>(control))
		{
			split->RefreshSplitterLayout();
			return;
		}
		if (auto* panel = dynamic_cast<Panel*>(control))
		{
			panel->InvalidateLayout();
			panel->UpdateLayout();
		}
	}

	static bool ApplyTrackedMetadataProperty(
		DesignerControl& designerControl,
		Control& target,
		const std::wstring& propertyName,
		DesignerStyleValue value,
		bool preserveExisting,
		std::wstring* outError = nullptr)
	{
		const auto* metadata = target.FindPropertyMetadata(propertyName);
		const std::wstring canonicalCandidate = metadata
			? metadata->Name() : propertyName;
		const auto existing = std::find_if(
			designerControl.MetadataProperties.begin(),
			designerControl.MetadataProperties.end(),
			[&](const auto& entry)
			{
				return _wcsicmp(entry.first.c_str(), canonicalCandidate.c_str()) == 0;
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
			&canonicalName, &effective, outError)) return false;
		designerControl.MetadataPropertyResourceKeys.erase(canonicalName);
		designerControl.MetadataPropertyDynamicResourceKeys.erase(canonicalName);
		return true;
	}

	static std::wstring FromUtf8(const std::string& s)
		{
			return Convert::Utf8ToUnicode(s);
		}

	static void ValueToMenuSubItems(const DesignValue& arr, std::vector<MenuItem*>& out, MenuItem* owner)
		{
			if (!owner) return;
			if (!arr.is_array()) return;
			for (auto& j : arr)
			{
				if (!j.is_object()) continue;
				bool sep = j.value("separator", false);
				if (sep)
				{
					auto* separatorItem = owner->AddSeparator();
					if (!separatorItem) continue;
					continue;
				}
				auto text = FromUtf8(j.value("text", std::string()));
				int id = j.value("id", 0);
				auto* subItem = owner->AddSubItem(text, id);
				if (!subItem) continue;
				subItem->Shortcut = FromUtf8(j.value("shortcut", std::string()));
				subItem->Enable = j.value("enable", true);
				if (j.contains("subItems"))
				{
					ValueToMenuSubItems(j["subItems"], out, subItem);
				}
			}
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

	static void ValueToGridRows(
		const DesignValue& value,
		std::vector<GridViewRow>& output)
	{
		if (!value.is_array()) return;
		for (const auto& rowValue : value)
		{
			if (!rowValue.is_object() || !rowValue.contains("cells")
				|| !rowValue["cells"].is_array()) continue;
			GridViewRow row;
			for (const auto& cellValue : rowValue["cells"])
			{
				if (!cellValue.is_object()) continue;
				CellValue cell;
				if (cellValue.contains("checked"))
					cell.SetBool(cellValue.value("checked", false));
				else if (cellValue.contains("selectedIndex"))
					cell.SetComboSelection(
						cellValue.value("selectedIndex", -1),
						FromUtf8(cellValue.value("value", std::string{})));
				else if (cellValue.contains("tag"))
					cell.SetTag(cellValue.value("tag", static_cast<long long>(0)));
				else
					cell.SetText(FromUtf8(cellValue.value("value", std::string{})));
				row.Cells.push_back(std::move(cell));
			}
			output.push_back(std::move(row));
		}
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

	static bool TryParseHAlign(const std::string& s, HorizontalAlignment& out)
		{
			if (s == "Left") { out = HorizontalAlignment::Left; return true; }
			if (s == "Center") { out = HorizontalAlignment::Center; return true; }
			if (s == "Right") { out = HorizontalAlignment::Right; return true; }
			if (s == "Stretch") { out = HorizontalAlignment::Stretch; return true; }
			return false;
		}

	static bool TryParseVAlign(const std::string& s, VerticalAlignment& out)
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
			if (s == "Fill") { out = Dock::Fill; return true; }
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
			if (s == "Percent") { out = SizeUnit::Percent; return true; }
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

	static void ValueToTreeNodes(const DesignValue& j, std::vector<TreeNode*>& outNodes)
		{
			if (!j.is_array()) return;
			for (auto& it : j)
			{
				if (!it.is_object()) continue;
				auto text = FromUtf8(it.value("text", std::string()));
				auto* node = new TreeNode(text);
				node->Expand = it.value("expand", false);
				if (it.contains("children"))
					ValueToTreeNodes(it["children"], node->Children);
				outNodes.push_back(node);
			}
		}

	static void ValueToListViewItems(const DesignValue& j, std::vector<ListViewItem>& outItems)
		{
			if (!j.is_array()) return;
			for (auto& it : j)
			{
				if (!it.is_object()) continue;
				ListViewItem item(FromUtf8(it.value("text", std::string())));
				item.SubText = FromUtf8(it.value("subText", std::string()));
				item.Checked = it.value("checked", false);
				item.Selected = it.value("selected", false);
				item.Enabled = it.value("enabled", true);
				if (it.contains("subItems") && it["subItems"].is_array())
				{
					for (auto& sj : it["subItems"])
						if (sj.is_string()) item.SubItems.push_back(FromUtf8(sj.get<std::string>()));
				}
				outItems.push_back(std::move(item));
			}
		}

	static void ValueToPropertyGridItems(const DesignValue& j, std::vector<PropertyGridItem>& outItems)
		{
			if (!j.is_array()) return;
			for (auto& it : j)
			{
				if (!it.is_object()) continue;
				PropertyGridItem item;
				item.Category = FromUtf8(it.value("category", std::string()));
				item.Name = FromUtf8(it.value("name", std::string()));
				item.Value = FromUtf8(it.value("value", std::string()));
				item.Description = FromUtf8(it.value("description", std::string()));
				item.ValueType = (PropertyGridValueType)it.value("type", (int)PropertyGridValueType::Text);
				item.ReadOnly = it.value("readOnly", false);
				item.IsMixed = it.value("isMixed", false);
				item.CanReset = it.value("canReset", false);
				item.Minimum = it.value("minimum", 0.0);
				item.Maximum = it.value("maximum", 1.0);
				item.Step = it.value("step", 0.01);
				item.Tag = static_cast<UINT64>(
					it.value("tag", static_cast<unsigned long long>(0)));
				if (it.contains("options") && it["options"].is_array())
				{
					for (auto& oj : it["options"])
						if (oj.is_string()) item.Options.push_back(FromUtf8(oj.get<std::string>()));
				}
				outItems.push_back(std::move(item));
			}
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
		if (targetEvent.Payload == DesignerComponentEventPayload::None)
		{
			if (sourceEvent == L"OnMouseClick")
				connection = source.OnMouseClick.Subscribe(
					[raiseNone](Control*, MouseEventArgs) mutable { raiseNone(); });
			else if (sourceEvent == L"OnMouseDoubleClick")
				connection = source.OnMouseDoubleClick.Subscribe(
					[raiseNone](Control*, MouseEventArgs) mutable { raiseNone(); });
			else if (sourceEvent == L"OnMouseEnter")
				connection = source.OnMouseEnter.Subscribe(
					[raiseNone](Control*, MouseEventArgs) mutable { raiseNone(); });
			else if (sourceEvent == L"OnMouseLeave")
				connection = source.OnMouseLeave.Subscribe(
					[raiseNone](Control*, MouseEventArgs) mutable { raiseNone(); });
			else if (sourceEvent == L"OnGotFocus")
				connection = source.OnGotFocus.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnLostFocus")
				connection = source.OnLostFocus.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnPaint")
				connection = source.OnPaint.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnClose")
				connection = source.OnClose.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnMoved")
				connection = source.OnMoved.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnSizeChanged")
				connection = source.OnSizeChanged.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnSelectedChanged")
				connection = source.OnSelectedChanged.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
			else if (sourceEvent == L"OnScrollChanged")
				connection = source.OnScrollChanged.Subscribe(
					[raiseNone](Control*) mutable { raiseNone(); });
		}
		else if (targetEvent.Payload == DesignerComponentEventPayload::Bool
			&& sourceEvent == L"OnChecked")
		{
			connection = source.OnChecked.Subscribe(
				[&owner, name = targetEvent.Name](Control* sender)
				{
					owner.RaiseDeclarativeEvent(
						name, BindingValue(sender && sender->Checked));
				});
		}
		else if (targetEvent.Payload == DesignerComponentEventPayload::String
			&& sourceEvent == L"OnTextChanged")
		{
			connection = source.OnTextChanged.Subscribe(
				[&owner, name = targetEvent.Name](
					Control*, std::wstring, std::wstring value)
				{
					owner.RaiseDeclarativeEvent(name, BindingValue(std::move(value)));
				});
		}
		else if (targetEvent.Payload == DesignerComponentEventPayload::String
			&& sourceEvent == L"OnDropText")
		{
			connection = source.OnDropText.Subscribe(
				[&owner, name = targetEvent.Name](Control*, std::wstring value)
				{
					owner.RaiseDeclarativeEvent(name, BindingValue(std::move(value)));
				});
		}
		if (!connection.Connected())
		{
			if (outError) *outError = L"不支持的模板事件转发："
				+ sourceEvent + L" -> " + targetEvent.Name;
			return false;
		}
		owner.RetainEventConnection(std::move(connection));
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
			DesignerModel::DesignDocumentMaterializationOptions options,
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

		bool TryGetChildItemsSource(
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
			templateDocument.Form.EventHandlers.clear();
			// A template has an item-scoped DataContext. Do not materialize unrelated
			// page-scoped list views merely because they share the owning document.
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
						&& property->ObjectKind == DesignerDataObjectKind::BindingList
						? property->ItemType : std::wstring{};
				}
				return std::wstring{};
			};
			std::function<void(const std::vector<DesignerModel::DesignNode>&,
				const DesignerDataContextSchema&)> scanNodes;
			auto scanTemplate = [&](const DesignerModel::DesignDataTemplate* definition)
			{
				if (!definition || !referencedTemplates.insert(definition).second)
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
					if (!node.Extra.is_object()) continue;
					auto include = [&](const char* name,
						std::unordered_set<std::wstring>& keys)
					{
						if (node.Extra.contains(name)
							&& node.Extra[name].is_string())
							keys.insert(FromUtf8(
								node.Extra[name].get<std::string>()));
					};
					include("itemsSourceResource", referencedLists);
					const DesignerModel::DesignDataTemplate* itemTemplate = nullptr;
					if (node.Extra.contains("itemTemplate")
						&& node.Extra["itemTemplate"].is_string())
					{
						const auto key = FromUtf8(
							node.Extra["itemTemplate"].get<std::string>());
						itemTemplate = templateDocument.FindDataTemplate(
							nodes, node, key);
					}
					else if (node.Type == UIClass::UI_ItemsControl
						|| node.Type == UIClass::UI_ListBox)
					{
						std::wstring itemType;
						if (node.Extra.contains("itemsSourceResource")
							&& node.Extra["itemsSourceResource"].is_string())
							itemType = resourceItemType(FromUtf8(
								node.Extra["itemsSourceResource"].get<std::string>()), schema);
						else if (node.Bindings.is_object()
							&& node.Bindings.contains("ItemsSource")
							&& node.Bindings["ItemsSource"].is_object())
						{
							const auto& binding = node.Bindings["ItemsSource"];
							if (binding.value("elementName", std::string{}).empty()
								&& binding.value("relativeSource", std::string{}).empty()
								&& !binding.contains("bindings"))
								if (const auto* property = DesignerDataContextSchemaUtils::Find(
									schema, FromUtf8(binding.value(
										"source", std::string{})));
									property && property->ObjectKind
										== DesignerDataObjectKind::BindingList)
									itemType = property->ItemType;
						}
						if (!itemType.empty()) itemTemplate = templateDocument.
							FindImplicitDataTemplate(nodes, node, itemType);
					}
					scanTemplate(itemTemplate);
					const DesignerModel::DesignDataTemplate* contentTemplate = nullptr;
					if (node.Extra.contains("contentTemplate")
						&& node.Extra["contentTemplate"].is_string())
					{
						const auto key = FromUtf8(
							node.Extra["contentTemplate"].get<std::string>());
						contentTemplate = templateDocument.FindDataTemplate(
							nodes, node, key);
					}
					else if (IsContentHostType(node.Type)
						&& node.Bindings.is_object()
						&& node.Bindings.contains("Content")
						&& node.Bindings["Content"].is_object())
					{
						const auto& binding = node.Bindings["Content"];
						if (binding.value("elementName", std::string{}).empty()
							&& binding.value("relativeSource", std::string{}).empty()
							&& !binding.contains("bindings"))
							if (const auto* property =
								DesignerDataContextSchemaUtils::Find(
									schema, FromUtf8(binding.value(
										"source", std::string{})));
								property && property->ObjectKind
									== DesignerDataObjectKind::BindingSource
								&& !property->DataType.empty())
								contentTemplate = templateDocument.
									FindImplicitDataTemplate(
										nodes, node, property->DataType);
					}
					scanTemplate(contentTemplate);
					const DesignerModel::DesignDataTemplate* headerTemplate = nullptr;
					if (node.Extra.contains("headerTemplate")
						&& node.Extra["headerTemplate"].is_string())
					{
						const auto key = FromUtf8(
							node.Extra["headerTemplate"].get<std::string>());
						headerTemplate = templateDocument.FindDataTemplate(
							nodes, node, key);
					}
					else if (IsHeaderedContentControlType(node.Type)
						&& node.Bindings.is_object()
						&& node.Bindings.contains("Header")
						&& node.Bindings["Header"].is_object())
					{
						const auto& binding = node.Bindings["Header"];
						if (binding.value("elementName", std::string{}).empty()
							&& binding.value("relativeSource", std::string{}).empty()
							&& !binding.contains("bindings"))
							if (const auto* property =
								DesignerDataContextSchemaUtils::Find(
									schema, FromUtf8(binding.value(
										"source", std::string{})));
								property && property->ObjectKind
									== DesignerDataObjectKind::BindingSource
								&& !property->DataType.empty())
								headerTemplate = templateDocument.
									FindImplicitDataTemplate(
										nodes, node, property->DataType);
					}
					scanTemplate(headerTemplate);
					if (node.Extra.contains("groupStyle")
						&& node.Extra["groupStyle"].is_string())
						{
							const auto key = FromUtf8(
								node.Extra["groupStyle"].get<std::string>());
							const auto* header = templateDocument.
								FindGroupStyleHeaderTemplate(nodes, node, key);
							scanTemplate(header);
						}
				}
			};
			scanNodes(templateDocument.Nodes, _schema);
			for (bool changed = true; changed;)
			{
				changed = false;
				for (const auto& view : _document->CollectionViews)
					if (referencedLists.contains(view.Key)
						&& !view.SourceResource.empty()
						&& referencedLists.insert(view.SourceResource).second)
						changed = true;
			}
			templateDocument.CollectionViews.erase(std::remove_if(
				templateDocument.CollectionViews.begin(),
				templateDocument.CollectionViews.end(), [&](const auto& view)
				{ return !referencedLists.contains(view.Key); }),
				templateDocument.CollectionViews.end());
			templateDocument.DataLists.erase(std::remove_if(
				templateDocument.DataLists.begin(),
				templateDocument.DataLists.end(), [&](const auto& list)
				{ return !referencedLists.contains(list.Key); }),
				templateDocument.DataLists.end());
			templateDocument.RecalculateNextStableId();
			DesignerModel::MaterializedControlTree tree;
			if (!DesignerModel::DesignDocumentMaterializer::Materialize(
				templateDocument, tree, _options, outError)) return {};
			if (tree.Roots.size() != 1 || !tree.Roots.front())
			{
				if (outError) *outError = L"DataTemplate 必须生成一个视觉根："
					+ _definition.DisplayName();
				return {};
			}
			if (!tree.Roots.front()->SetDataContext(item))
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
								resolved.Source = record->ControlInstance->Parent
									? &record->ControlInstance->Parent->DataContextSource()
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
						bindingSource = record->ControlInstance->Parent
							? &record->ControlInstance->Parent->DataContextSource()
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
			auto result = std::move(tree.Roots.front());
			if (outError) outError->clear();
			return result;
		}

	private:
		std::shared_ptr<const DesignerModel::DesignDocument> _document;
		DesignerModel::DesignDataTemplate _definition;
		DesignerModel::DesignObjectResourceDictionary _visibleObjects;
		DesignerStyleSheet _visibleStyles;
		std::wstring _dataType;
		DesignerModel::DesignDocumentMaterializationOptions _options;
		DesignerDataContextSchema _schema;
	};

	class MaterializedControlTemplate final : public IControlTemplate
	{
	public:
		MaterializedControlTemplate(
			std::shared_ptr<const DesignerModel::DesignDocument> document,
			UIClass targetType,
			DesignerModel::DesignObjectResourceDictionary visibleObjects,
			DesignerStyleSheet visibleStyles,
			DesignerModel::DesignDocumentMaterializationOptions options,
			std::wstring styleId)
			: _document(std::move(document)),
			  _targetType(targetType),
			  _visibleObjects(std::move(visibleObjects)),
			  _visibleStyles(std::move(visibleStyles)),
			  _options(std::move(options)),
			  _styleId(std::move(styleId)) {}

		UIClass TargetType() const noexcept override { return _targetType; }

		std::unique_ptr<Control> Build(
			std::wstring* outError) const override
		{
			if (!_document)
			{
				if (outError) *outError = L"ControlTemplate 缺少源文档。";
				return {};
			}
			DesignerModel::DesignDocument runtimeDocument = *_document;
			runtimeDocument.Nodes.clear();
			runtimeDocument.Components = _visibleObjects.Components;
			runtimeDocument.ControlTemplates = _visibleObjects.ControlTemplates;
			runtimeDocument.DataTemplates = _visibleObjects.DataTemplates;
			runtimeDocument.ItemsPanelTemplates =
				_visibleObjects.ItemsPanelTemplates;
			runtimeDocument.GroupStyles = _visibleObjects.GroupStyles;
			runtimeDocument.StyleSheet = _visibleStyles;
			runtimeDocument.Form.EventHandlers.clear();
			runtimeDocument.NextStableId = 2;

			DesignerModel::DesignNode root;
			root.Id = 1;
			root.Name = L"__runtimeListBoxItem";
			root.Type = _targetType;
			root.Order = 0;
			root.Props = DesignValue::object();
			root.Extra = DesignValue::object();
			if (!_styleId.empty())
				root.Props["styleId"] = Convert::UnicodeToUtf8(_styleId);
			runtimeDocument.Nodes.push_back(std::move(root));

			DesignerModel::MaterializedControlTree tree;
			std::wstring error;
			if (!DesignerModel::DesignDocumentMaterializer::Materialize(
				runtimeDocument, tree, _options, &error))
			{
				if (outError) *outError = std::move(error);
				return {};
			}
			if (tree.Roots.size() != 1
				|| !tree.Roots.front()
				|| tree.Roots.front()->Type() != _targetType)
			{
				if (outError) *outError =
					L"ControlTemplate 未生成唯一且类型兼容的宿主。";
				return {};
			}
			if (outError) outError->clear();
			auto result = std::move(tree.Roots.front());
			tree.Roots.clear();
			return result;
		}

	private:
		std::shared_ptr<const DesignerModel::DesignDocument> _document;
		UIClass _targetType = UIClass::UI_Base;
		DesignerModel::DesignObjectResourceDictionary _visibleObjects;
		DesignerStyleSheet _visibleStyles;
		DesignerModel::DesignDocumentMaterializationOptions _options;
		std::wstring _styleId;
	};
}

bool DesignerModel::DesignDocumentMaterializer::InstallComponentContract(
	Control& control,
	const DesignComponentDefinition& component,
	const DesignDocument& document,
	std::wstring* outError)
{
	return InstallComponentContractCore(
		control, component, document, outError);
}

std::unique_ptr<Control>
DesignerModel::DesignDocumentMaterializer::CreateRuntimeControl(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_Base: return std::make_unique<Control>();
	case UIClass::UI_Label: return std::make_unique<Label>(L"标签", 0, 0);
	case UIClass::UI_LinkLabel: return std::make_unique<LinkLabel>(L"链接标签", 0, 0);
	case UIClass::UI_Button: return std::make_unique<Button>(L"按钮", 0, 0, 120, 30);
	case UIClass::UI_TextBox: return std::make_unique<TextBox>(L"", 0, 0, 200, 25);
	case UIClass::UI_RichTextBox: return std::make_unique<RichTextBox>(L"", 0, 0, 300, 160);
	case UIClass::UI_PasswordBox: return std::make_unique<PasswordBox>(L"", 0, 0, 200, 25);
	case UIClass::UI_DateTimePicker: return std::make_unique<DateTimePicker>(L"", 0, 0, 200, 28);
	case UIClass::UI_NumericUpDown: return std::make_unique<NumericUpDown>(0, 0, 140, 30);
	case UIClass::UI_Panel: return std::make_unique<Panel>(0, 0, 200, 200);
	case UIClass::UI_GroupBox: return std::make_unique<GroupBox>(L"GroupBox", 0, 0, 240, 180);
	case UIClass::UI_Expander: return std::make_unique<Expander>(L"Expander", 0, 0, 260, 160);
	case UIClass::UI_ScrollView: return std::make_unique<ScrollView>(0, 0, 240, 200);
	case UIClass::UI_StackPanel: return std::make_unique<StackPanel>(0, 0, 200, 200);
	case UIClass::UI_GridPanel: return std::make_unique<GridPanel>(0, 0, 200, 200);
	case UIClass::UI_DockPanel: return std::make_unique<DockPanel>(0, 0, 200, 200);
	case UIClass::UI_WrapPanel: return std::make_unique<WrapPanel>(0, 0, 200, 200);
	case UIClass::UI_RelativePanel: return std::make_unique<RelativePanel>(0, 0, 200, 200);
	case UIClass::UI_SplitContainer: return std::make_unique<SplitContainer>(0, 0, 360, 220);
	case UIClass::UI_CheckBox: return std::make_unique<CheckBox>(L"复选框", 0, 0);
	case UIClass::UI_RadioBox: return std::make_unique<RadioBox>(L"单选框", 0, 0);
	case UIClass::UI_ComboBox: return std::make_unique<ComboBox>(L"", 0, 0, 150, 25);
	case UIClass::UI_ListView: return std::make_unique<ListView>(0, 0, 320, 220);
	case UIClass::UI_ListBox: return std::make_unique<ListBox>(0, 0, 220, 180);
	case UIClass::UI_GridView: return std::make_unique<GridView>(0, 0, 360, 200);
	case UIClass::UI_PropertyGrid: return std::make_unique<PropertyGridView>(0, 0, 300, 320);
	case UIClass::UI_ChartView: return std::make_unique<ChartView>(0, 0, 420, 260);
	case UIClass::UI_ReportView: return std::make_unique<ReportView>(0, 0, 480, 300);
	case UIClass::UI_KpiCard: return std::make_unique<KpiCard>(0, 0, 220, 132);
	case UIClass::UI_FilterBar: return std::make_unique<FilterBar>(0, 0, 640, 48);
	case UIClass::UI_TreeView: return std::make_unique<TreeView>(0, 0, 220, 220);
	case UIClass::UI_ProgressBar: return std::make_unique<ProgressBar>(0, 0, 200, 20);
	case UIClass::UI_LoadingRing: return std::make_unique<LoadingRing>(0, 0, 48, 48);
	case UIClass::UI_ProgressRing: return std::make_unique<ProgressRing>(0, 0, 72, 72);
	case UIClass::UI_Slider: return std::make_unique<Slider>(0, 0, 200, 30);
	case UIClass::UI_PictureBox: return std::make_unique<PictureBox>(0, 0, 150, 150);
	case UIClass::UI_Switch: return std::make_unique<Switch>(0, 0, 60, 30);
	case UIClass::UI_TabControl: return std::make_unique<TabControl>(0, 0, 360, 240);
	case UIClass::UI_ToolBar: return std::make_unique<ToolBar>(0, 0, 360, 34);
	case UIClass::UI_Menu: return std::make_unique<Menu>(0, 0, 600, 28);
	case UIClass::UI_StatusBar: return std::make_unique<StatusBar>(0, 0, 600, 26);
	case UIClass::UI_ToastHost: return std::make_unique<ToastHost>(0, 0, 340, 260);
	case UIClass::UI_WebBrowser: return std::make_unique<WebBrowser>(0, 0, 500, 360);
	case UIClass::UI_MediaPlayer: return std::make_unique<MediaPlayer>(0, 0, 640, 360);
	case UIClass::UI_NativeSurface: return std::make_unique<NativeSurface>(0, 0, 320, 180);
	case UIClass::UI_ItemsControl: return std::make_unique<ItemsControl>(0, 0, 260, 220);
	case UIClass::UI_ContentPresenter: return std::make_unique<ContentPresenter>(0, 0, 260, 120);
	case UIClass::UI_ItemsPresenter: return std::make_unique<ItemsPresenter>(0, 0, 260, 120);
	case UIClass::UI_ContentControl: return std::make_unique<ContentControl>(0, 0, 260, 140);
	case UIClass::UI_SelectorItem: return std::make_unique<SelectorItem>();
	case UIClass::UI_ComboBoxItem: return std::make_unique<ComboBoxItem>();
	case UIClass::UI_TreeViewItem: return std::make_unique<TreeViewItem>();
	case UIClass::UI_NavigationView: return std::make_unique<NavigationView>(0, 0, 220, 360);
	case UIClass::UI_SideBar: return std::make_unique<SideBar>(0, 0, 200, 360);
	case UIClass::UI_BreadcrumbBar: return std::make_unique<BreadcrumbBar>(0, 0, 320, 32);
	case UIClass::UI_CalendarView: return std::make_unique<CalendarView>(0, 0, 280, 300);
	case UIClass::UI_DateRangePicker: return std::make_unique<DateRangePicker>(L"", 0, 0, 240, 30);
	case UIClass::UI_ColorPicker: return std::make_unique<ColorPicker>(0, 0, 180, 30);
	case UIClass::UI_PagedGridView: return std::make_unique<PagedGridView>(0, 0, 520, 320);
	default: return nullptr;
	}
}

bool DesignerModel::DesignDocumentMaterializer::Materialize(
	const DesignDocument& document,
	MaterializedControlTree& output,
	std::wstring* outError)
{
	return Materialize(
		document, output, DesignDocumentMaterializationOptions{}, outError);
}

bool DesignerModel::DesignDocumentMaterializer::Materialize(
	const DesignDocument& sourceDocument,
	MaterializedControlTree& output,
	const DesignDocumentMaterializationOptions& options,
	std::wstring* outError)
{
	try
	{
		auto canonicalSource = sourceDocument;
		if (!DesignDataResourceUtils::ValidateAndCanonicalize(
			canonicalSource, outError)) return false;
		DesignDocument expandedDocument = canonicalSource;
		for (int pass = 0; pass < 64; ++pass)
		{
			const auto previousCount = expandedDocument.Nodes.size();
			DesignDocument componentPass;
			if (!ExpandComponentTemplates(
				expandedDocument, componentPass, outError)) return false;
			DesignDocument controlTemplatePass;
			if (!ExpandControlTemplates(
				componentPass, controlTemplatePass, outError)) return false;
			expandedDocument = std::move(controlTemplatePass);
			if (expandedDocument.Nodes.size() == previousCount) break;
			if (pass == 63)
			{
				if (outError) *outError =
					L"模板展开层级超过 64；可能存在间接递归。";
				return false;
			}
		}
		const DesignDocument& document = expandedDocument;
		const auto templateDocument =
			std::make_shared<const DesignDocument>(document);
		const auto createBaseControl = options.ControlFactory
			? options.ControlFactory
			: std::function<std::unique_ptr<Control>(UIClass)>(
				DesignDocumentMaterializer::CreateRuntimeControl);
		const DesignDocumentControlPool::Factory createControl =
			[&](const DesignNode& node) -> std::unique_ptr<Control>
			{
				if (!node.ComponentType.Empty())
				{
					const auto* component = document.FindComponent(
						document.Nodes, node, node.ComponentType);
					if (!component || component->BaseType != node.Type)
						return nullptr;
					return createBaseControl(component->BaseType);
				}
				return createBaseControl(node.Type);
			};
		if (!DesignerDataContextSchemaUtils::Validate(document.DataContextSchema, outError))
			return false;
		if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
			document.StyleSheet,
			[&](const DesignerStyleRule& rule) -> std::unique_ptr<Control>
			{
				auto probe = createBaseControl(
					rule.HasType ? rule.Type : UIClass::UI_Base);
				if (!probe || rule.ComponentType.Empty()) return probe;
				const auto* component = document.FindComponent(rule.ComponentType);
				std::wstring ignored;
				if (!component || !InstallComponentContractCore(
					*probe, *component, document, &ignored)) return nullptr;
				return probe;
			},
			outError,
			document.ResourceBasePath,
			document.Resources))
			return false;
		DesignDocumentGraph documentGraph;
		if (!DesignDocumentGraph::Build(
			document, documentGraph, outError))
			return false;
		DesignDocumentControlPool controlPool;
		if (!DesignDocumentControlPool::Build(
			document,
			documentGraph,
			createControl,
			controlPool,
			outError))
			return false;

		const int stagingWidth = document.Form.Size.cx > 0
			? document.Form.Size.cx : 1;
		const int stagingHeight = document.Form.Size.cy > 0
			? document.Form.Size.cy : 1;
		Panel stagingRoot(0, 0, stagingWidth, stagingHeight);
		MaterializedControlTree candidate;
		auto dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(dataContextSchema);

		struct Pending
		{
			const DesignNode* source = nullptr;
			std::wstring name;
			int id = 0;
			UIClass type = UIClass::UI_Base;
			std::wstring parent;
			int order = -1;
			bool locked = false;
			DesignValue props;
			DesignValue extra;
			DesignValue events;
			DesignValue bindings;
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
			bool controlTemplateRoot = false;
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
			p.parent = resolved.ParentKey;
			p.order = node.Order;
			p.locked = node.Locked;
			p.props = node.Props.is_object() ? node.Props : DesignValue::object();
			p.extra = node.Extra.is_object() ? node.Extra : DesignValue::object();
			p.events = node.Events.is_object() ? node.Events : DesignValue::object();
			p.bindings = node.Bindings.is_object() ? node.Bindings : DesignValue::object();
			p.localResources = node.LocalResources;
			p.localObjectResources = node.LocalObjectResources;
			p.templateBindings = node.TemplateBindings;
			p.templateEventBindings = node.TemplateEventBindings;
			p.presentedContent = node.PresentedComponentContent;
			p.templateContentSource = node.TemplateContentSource;
			if (p.extra.is_object())
			{
				p.templateGenerated = p.extra.value(TemplateGeneratedKey, false);
				p.templateOwner = FromUtf8(
					p.extra.value(TemplateOwnerKey, std::string{}));
				p.templatePartName = FromUtf8(
					p.extra.value(TemplatePartNameKey, std::string{}));
				p.contentOwner = FromUtf8(
					p.extra.value(TemplateContentOwnerKey, std::string{}));
				p.controlTemplateRoot = p.extra.value(
					ControlTemplateRootKey, false);
			}
			p.componentType = node.ComponentType;
			items.push_back(std::move(p));
		}

		auto projectSchema = [&](const std::wstring& prefix)
		{
			if (prefix.empty()) return dataContextSchema;
			DesignerDataContextSchema result;
			const auto normalizedPrefix =
				DesignerDataContextSchemaUtils::NormalizePath(prefix);
			auto lower = [](std::wstring value)
			{
				std::transform(value.begin(), value.end(), value.begin(),
					[](wchar_t ch) { return (wchar_t)std::towlower(ch); });
				return value;
			};
			const auto childPrefix = lower(normalizedPrefix + L".");
			for (const auto& property : dataContextSchema)
			{
				const auto path = DesignerDataContextSchemaUtils::NormalizePath(
					property.Path);
				if (!lower(path).starts_with(childPrefix)) continue;
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
					{ return _wcsicmp(resource.Key.c_str(), key.c_str()) == 0; });
				if (found != scope->localResources.Resources.rend()) return &*found;
				if (scope->parent.empty()) break;
				const auto parent = pendingByName.find(scope->parent);
				if (parent != pendingByName.end())
					scope = parent->second;
				else
				{
					const auto page = scope->parent.find(L"#page");
					const auto owner = page == std::wstring::npos
						? pendingByName.end()
						: pendingByName.find(scope->parent.substr(0, page));
					scope = owner == pendingByName.end() ? nullptr : owner->second;
				}
			}
			const auto found = std::find_if(
				document.StyleSheet.Resources.rbegin(),
				document.StyleSheet.Resources.rend(),
				[&](const auto& resource)
				{ return _wcsicmp(resource.Key.c_str(), key.c_str()) == 0; });
			return found == document.StyleSheet.Resources.rend()
				? nullptr : &*found;
		};
		auto appendStyleScope = [](DesignerStyleSheet& target,
			const DesignerStyleSheet& source)
		{
			for (const auto& dictionary : source.MergedDictionaries)
				if (std::none_of(target.MergedDictionaries.begin(),
					target.MergedDictionaries.end(), [&](const auto& current)
					{ return _wcsicmp(current.c_str(), dictionary.c_str()) == 0; }))
					target.MergedDictionaries.push_back(dictionary);
			for (const auto& resource : source.Resources)
			{
				target.Resources.erase(std::remove_if(
					target.Resources.begin(), target.Resources.end(),
					[&](const auto& current)
					{
						return _wcsicmp(
							current.Key.c_str(), resource.Key.c_str()) == 0;
					}), target.Resources.end());
				target.Resources.push_back(resource);
			}
			target.Rules.insert(
				target.Rules.end(), source.Rules.begin(), source.Rules.end());
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
				else
				{
					const auto page = scope->parent.find(L"#page");
					const auto owner = page == std::wstring::npos
						? pendingByName.end()
						: pendingByName.find(scope->parent.substr(0, page));
					scope = owner == pendingByName.end()
						? nullptr : owner->second;
				}
			}
			for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
				appendStyleScope(result, (*scope)->localResources);
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
			if (item.bindings.is_object())
				for (const auto& [target, binding] : item.bindings.ObjectItems())
				{
					if (_wcsicmp(FromUtf8(target).c_str(), L"DataContext") != 0
						|| !binding.is_object()) continue;
					if (binding.contains("bindings"))
					{
						effective.reset();
						break;
					}
					const bool explicitSource = !binding.value(
						"elementName", std::string{}).empty()
						|| !binding.value("relativeSource", std::string{}).empty();
					if (!explicitSource && inherited)
					{
						const auto path = DesignerBindingUtils::Trim(FromUtf8(
							binding.value("source", std::string{})));
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

		std::unordered_map<std::wstring, Control*> tabPageOf;
		tabPageOf.reserve(64);

		auto resolveItemType = [&](const Pending& item)
			-> const DesignDataTypeDefinition*
		{
			if (item.extra.is_object()
				&& item.extra.contains("itemsSourceResource")
				&& item.extra["itemsSourceResource"].is_string())
			{
				std::wstring key = FromUtf8(
					item.extra["itemsSourceResource"].get<std::string>());
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
			if (item.bindings.is_object()
				&& item.bindings.contains("ItemsSource")
				&& item.bindings["ItemsSource"].is_object())
			{
				if (item.bindings["ItemsSource"].contains("elementName")
					&& item.bindings["ItemsSource"]["elementName"].is_string()
					&& !item.bindings["ItemsSource"]["elementName"]
						.get<std::string>().empty())
					return nullptr;
				if (item.bindings["ItemsSource"].contains("relativeSource")
					&& item.bindings["ItemsSource"]["relativeSource"].is_string()
					&& !item.bindings["ItemsSource"]["relativeSource"]
						.get<std::string>().empty())
					return nullptr;
				const auto path = FromUtf8(item.bindings["ItemsSource"].value(
					"source", std::string{}));
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
			if (!item.bindings.is_object()
				|| !item.bindings.contains("Content")
				|| !item.bindings["Content"].is_object()) return nullptr;
			const auto& binding = item.bindings["Content"];
			if (!binding.value("elementName", std::string{}).empty()
				|| !binding.value("relativeSource", std::string{}).empty()
				|| binding.contains("bindings")) return nullptr;
			const auto path = FromUtf8(binding.value("source", std::string{}));
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
			if (!item.bindings.is_object()
				|| !item.bindings.contains("Header")
				|| !item.bindings["Header"].is_object()) return nullptr;
			const auto& binding = item.bindings["Header"];
			if (!binding.value("elementName", std::string{}).empty()
				|| !binding.value("relativeSource", std::string{}).empty()
				|| binding.contains("bindings")) return nullptr;
			const auto path = FromUtf8(binding.value("source", std::string{}));
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
			Control* c = controlPool.FindById(it.id);
			if (!c) return false;
			if (!it.componentType.Empty())
			{
				const auto* component = it.source
					? document.FindComponent(document.Nodes, *it.source, it.componentType)
					: document.FindComponent(it.componentType);
				std::wstring componentError;
				if (component)
					c->SetDeclarativeTypeIdentity(
						component->Type.XamlNamespace,
						component->Type.XamlName);
				if (!component || !InstallComponentContractCore(
					*c, *component, document, &componentError))
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的组件契约无效：" + componentError;
					return false;
				}
			}
			auto dc = std::make_shared<DesignerControl>(
				c, it.name, it.type, nullptr, it.id);
			if (!it.localResources.Empty())
				dc->LocalResources =
					std::make_shared<DesignerStyleSheet>(it.localResources);
			if (!it.localObjectResources.Empty())
				dc->LocalObjectResources =
					std::make_shared<DesignObjectResourceDictionary>(
						it.localObjectResources);
			if (!it.localResources.Empty())
			{
				DesignerStyleSheet runtimeSource;
				if (!prepareLocalStyleSheet(it, runtimeSource)) return false;
				std::shared_ptr<ControlStyleSheet> localResources;
				if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
					runtimeSource, localResources, outError,
					document.ResourceBasePath, document.Resources)
					|| !c->SetResourceDictionary(std::move(localResources)))
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
			if (it.extra.is_object())
			{
				if (it.extra.contains("controlTemplate")
					&& it.extra["controlTemplate"].is_string())
					dc->DesignStrings[L"controlTemplate"] = FromUtf8(
						it.extra["controlTemplate"].get<std::string>());
				if (it.extra.contains("itemTemplate")
					&& it.extra["itemTemplate"].is_string())
					dc->DesignStrings[L"itemTemplate"] = FromUtf8(
						it.extra["itemTemplate"].get<std::string>());
				if (it.extra.contains("itemsSourceResource")
					&& it.extra["itemsSourceResource"].is_string())
					dc->DesignStrings[L"itemsSourceResource"] = FromUtf8(
						it.extra["itemsSourceResource"].get<std::string>());
				if (it.extra.contains("itemContainerStyle")
					&& it.extra["itemContainerStyle"].is_string())
					dc->DesignStrings[L"itemContainerStyle"] = FromUtf8(
						it.extra["itemContainerStyle"].get<std::string>());
				if (it.extra.contains("itemsPanel")
					&& it.extra["itemsPanel"].is_string())
					dc->DesignStrings[L"itemsPanel"] = FromUtf8(
						it.extra["itemsPanel"].get<std::string>());
				if (it.extra.contains("groupStyle")
					&& it.extra["groupStyle"].is_string())
					dc->DesignStrings[L"groupStyle"] = FromUtf8(
						it.extra["groupStyle"].get<std::string>());
				if (it.extra.contains("contentTemplate")
					&& it.extra["contentTemplate"].is_string())
					dc->DesignStrings[L"contentTemplate"] = FromUtf8(
						it.extra["contentTemplate"].get<std::string>());
				if (it.extra.contains("contentText")
					&& it.extra["contentText"].is_string())
					dc->DesignStrings[L"contentText"] = FromUtf8(
						it.extra["contentText"].get<std::string>());
				if (it.extra.contains("headerTemplate")
					&& it.extra["headerTemplate"].is_string())
					dc->DesignStrings[L"headerTemplate"] = FromUtf8(
						it.extra["headerTemplate"].get<std::string>());
				if (it.extra.contains("headerText")
					&& it.extra["headerText"].is_string())
					dc->DesignStrings[L"headerText"] = FromUtf8(
						it.extra["headerText"].get<std::string>());
				if (it.extra.contains("headeredRegion")
					&& it.extra["headeredRegion"].is_string())
					dc->DesignStrings[L"headeredRegion"] = FromUtf8(
						it.extra["headeredRegion"].get<std::string>());
			}
			dcOf[it.name] = dc;
			instOf[it.name] = c;
		}
		for (const auto& it : items)
		{
			if (!it.templateGenerated || it.templateOwner.empty()
				|| it.templatePartName.empty()) continue;
			const auto owner = instOf.find(it.templateOwner);
			const auto part = instOf.find(it.name);
			if (owner == instOf.end() || part == instOf.end()
				|| !owner->second || !part->second
				|| !owner->second->RegisterDeclarativeTemplatePart(
					it.templatePartName, part->second))
			{
				if (outError) *outError = L"组件模板部件注册失败："
					+ it.templateOwner + L"." + it.templatePartName;
				return false;
			}
		}

		for (const auto& it : items)
		{
			if (it.presentedContent.empty() || it.templateOwner.empty()) continue;
			const auto owner = dcOf.find(it.templateOwner);
			const auto presenter = instOf.find(it.name);
			if (owner == dcOf.end() || !owner->second
				|| presenter == instOf.end() || !presenter->second) continue;
			owner->second->ComponentContentPresenters[
				it.presentedContent] = presenter->second;
			if (!owner->second->ControlInstance
				|| !owner->second->ControlInstance
					->RegisterDeclarativeContentPresenter(
						it.presentedContent, presenter->second))
			{
				if (outError) *outError = L"组件内容 Presenter 注册失败："
					+ it.templateOwner + L"." + it.presentedContent;
				return false;
			}
		}

		for (const auto& it : items)
		{
			if (it.templateContentSource.empty()
				|| it.templateOwner.empty()) continue;
			const auto owner = instOf.find(it.templateOwner);
			const auto presenter = instOf.find(it.name);
			auto* contentPresenter = presenter == instOf.end()
				? nullptr : dynamic_cast<ContentPresenter*>(presenter->second);
			auto* contentHost = owner == instOf.end()
				? nullptr : dynamic_cast<ContentControl*>(owner->second);
			bool registered = false;
			if (it.templateContentSource == L"Content" && contentHost)
				registered = contentHost->RegisterTemplateContentPresenter(
					contentPresenter);
			else if (it.templateContentSource == L"Header")
			{
				auto* headered = owner == instOf.end()
					? nullptr : dynamic_cast<HeaderedContentControl*>(owner->second);
				registered = headered
					&& headered->RegisterTemplateHeaderPresenter(contentPresenter);
			}
			if (!registered)
			{
				if (outError) *outError = L"ControlTemplate ContentSource 注册失败："
					+ it.templateOwner + L"." + it.templateContentSource;
				return false;
			}
		}

		for (const auto& it : items)
		{
			if (!it.templateGenerated || it.templateOwner.empty()
				|| it.type != UIClass::UI_ItemsPresenter) continue;
			const auto owner = instOf.find(it.templateOwner);
			const auto presenter = instOf.find(it.name);
			auto* itemsControl = owner == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(owner->second);
			auto* itemsPresenter = presenter == instOf.end()
				? nullptr : dynamic_cast<ItemsPresenter*>(presenter->second);
			if (!itemsControl || !itemsPresenter
				|| !itemsControl->RegisterTemplateItemsPresenter(itemsPresenter))
			{
				if (outError) *outError = L"ControlTemplate ItemsPresenter 注册失败："
					+ it.templateOwner + L"." + it.templatePartName;
				return false;
			}
		}

		for (auto& it : items)
		{
			auto dcIt = dcOf.find(it.name);
			if (dcIt == dcOf.end()) continue;
			auto dc = dcIt->second;
			auto* c = dc->ControlInstance;
			if (!c) continue;

			if (it.events.is_object())
			{
				dc->EventHandlers.clear();
				for (const auto& [eventName, eventValue] : it.events.ObjectItems())
				{
					std::wstring k = FromUtf8(eventName);
					if (k.empty()) continue;
					if (eventValue.is_boolean())
					{
						if (eventValue.get<bool>())
							dc->EventHandlers[k] = L"1";
					}
					else if (eventValue.is_string())
					{
						std::wstring v = FromUtf8(eventValue.get<std::string>());
						if (!v.empty()) dc->EventHandlers[k] = v;
					}
				}
			}

			if (it.bindings.is_object())
			{
				dc->DataBindings.clear();
				for (const auto& [targetName, bindingValue] : it.bindings.ObjectItems())
				{
					if (!bindingValue.is_object())
					{
						if (outError) *outError = L"控件 " + it.name + L" 的数据绑定格式无效。";
						return false;
					}
					const std::wstring targetProperty = FromUtf8(targetName);
					DesignerDataBinding binding;
					std::wstring bindingReadError;
					if (!DesignerBindingUtils::TryReadBindingDefinition(
						bindingValue, binding, &bindingReadError))
					{
						if (outError) *outError = L"控件 " + it.name + L"："
							+ bindingReadError;
						return false;
					}
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
						const auto source = instOf.find(it.templateOwner);
						if (!it.templateGenerated || source == instOf.end()
							|| !source->second)
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的 TemplatedParent 无法解析。";
							return false;
						}
						elementSourceSchema = DesignerBindingUtils::BuildSourceSchema(
							*source->second);
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
					const BindingPropertyMetadata* metadata = nullptr;
					std::wstring validationError;
					if (!DesignerBindingUtils::Validate(
						*c, targetProperty, binding, &metadata, &validationError,
						sourceSchema))
					{
						if (outError) *outError = L"控件 " + it.name + L"：" + validationError;
						return false;
					}

					dc->DataBindings[metadata->Name()] = std::move(binding);
				}
			}

			if (it.props.is_object())
			{
				if (it.props.contains("text") && it.props["text"].is_string())
					c->Text = FromUtf8(it.props["text"].get<std::string>());
				c->SetStyleId(it.props.contains("styleId") && it.props["styleId"].is_string()
					? FromUtf8(it.props["styleId"].get<std::string>())
					: std::wstring{});
				c->ClearStyleClasses();
				if (it.props.contains("styleClasses") && it.props["styleClasses"].is_array())
				{
					for (const auto& styleClass : it.props["styleClasses"])
					{
						if (styleClass.is_string())
							c->AddStyleClass(FromUtf8(styleClass.get<std::string>()));
					}
				}
				if (it.props.contains("location"))
				{
					auto& l = it.props["location"];
					if (l.is_object())
						c->Location = { l.value("x", 0), l.value("y", 0) };
				}
				if (it.props.contains("size"))
				{
					auto& s = it.props["size"];
					if (s.is_object())
						c->Size = { s.value("w", c->Size.cx), s.value("h", c->Size.cy) };
				}
				c->Enable = it.props.value("enable", true);
				c->Visible = it.props.value("visible", true);
				c->BackColor = ColorFromValue(it.props.contains("backColor") ? it.props["backColor"] : DesignValue(), c->BackColor);
				c->ForeColor = ColorFromValue(it.props.contains("foreColor") ? it.props["foreColor"] : DesignValue(), c->ForeColor);
				DesignValue borderColorValue = it.props.contains("borderColor")
					? it.props["borderColor"]
					: (it.props.contains("bolderColor") ? it.props["bolderColor"] : DesignValue());
				c->BorderColor = ColorFromValue(borderColorValue, c->BorderColor);
				c->ShowValidationBorder = it.props.value("showValidationBorder", c->ShowValidationBorder);
				c->ShowValidationToolTip = it.props.value("showValidationToolTip", c->ShowValidationToolTip);
				c->ValidationBorderThickness = (float)it.props.value(
					"validationBorderThickness", (double)c->ValidationBorderThickness);
				c->ValidationCornerRadius = (float)it.props.value(
					"validationCornerRadius", (double)c->ValidationCornerRadius);
				c->ValidationToolTipMaxWidth = (float)it.props.value(
					"validationToolTipMaxWidth", (double)c->ValidationToolTipMaxWidth);
				if (it.props.contains("accessibleDescription")
					&& it.props["accessibleDescription"].is_string())
					c->AccessibleDescription = FromUtf8(
						it.props["accessibleDescription"].get<std::string>());
				c->Margin = ThicknessFromValue(it.props.contains("margin") ? it.props["margin"] : DesignValue(), c->Margin);
				c->Padding = ThicknessFromValue(it.props.contains("padding") ? it.props["padding"] : DesignValue(), c->Padding);
				c->AnchorStyles = (uint8_t)it.props.value("anchor", (int)c->AnchorStyles);
				HorizontalAlignment ha = c->HAlign;
				VerticalAlignment va = c->VAlign;
				Dock dk = c->DockPosition;
				if (it.props.contains("hAlign") && it.props["hAlign"].is_string())
					TryParseHAlign(it.props["hAlign"].get<std::string>(), ha);
				if (it.props.contains("vAlign") && it.props["vAlign"].is_string())
					TryParseVAlign(it.props["vAlign"].get<std::string>(), va);
				if (it.props.contains("dock") && it.props["dock"].is_string())
					TryParseDock(it.props["dock"].get<std::string>(), dk);
				c->HAlign = ha;
				c->VAlign = va;
				c->DockPosition = dk;
				c->ZIndex = it.props.value("zIndex", c->ZIndex);
				c->GridRow = it.props.value("gridRow", c->GridRow);
				c->GridColumn = it.props.value("gridColumn", c->GridColumn);
				c->GridRowSpan = it.props.value("gridRowSpan", c->GridRowSpan);
				c->GridColumnSpan = it.props.value("gridColumnSpan", c->GridColumnSpan);
				c->SizeMode = (ImageSizeMode)it.props.value("sizeMode", (int)c->SizeMode);

				// Font：有显式设置则创建新对象，否则跟随窗体字体/框架默认
				if (it.props.contains("font") && it.props["font"].is_object())
				{
					auto& fj = it.props["font"];
					std::wstring fn = FromUtf8(fj.value("name", std::string()));
					float fs = (float)fj.value("size", (double)GetDefaultFontObject()->FontSize);
					if (fs < 1.0f) fs = 1.0f;
					if (fs > 200.0f) fs = 200.0f;
					if (fn.empty()) fn = GetDefaultFontObject()->FontName;
					c->Font = new ::Font(fn, fs);
				}
				if (it.props.contains("metadata") && it.props["metadata"].is_object())
				{
					using MetadataEntry = std::pair<const std::string*, const DesignValue*>;
					std::vector<MetadataEntry> metadataEntries;
					for (const auto& [propertyKey, propertyValue]
						: it.props["metadata"].ObjectItems())
					{
						metadataEntries.emplace_back(&propertyKey, &propertyValue);
					}
					std::stable_sort(metadataEntries.begin(), metadataEntries.end(),
						[c](const MetadataEntry& left, const MetadataEntry& right)
						{
							const auto leftName = FromUtf8(*left.first);
							const auto rightName = FromUtf8(*right.first);
							const auto* leftMetadata = c->FindPropertyMetadata(leftName);
							const auto* rightMetadata = c->FindPropertyMetadata(rightName);
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
							{
								return leftMetadata != nullptr;
							}
							return _wcsicmp(leftName.c_str(), rightName.c_str()) < 0;
						});

					for (const auto& [propertyKeyPointer, propertyValuePointer]
						: metadataEntries)
					{
						const auto& propertyKey = *propertyKeyPointer;
						const auto& propertyValue = *propertyValuePointer;
						if (!propertyValue.is_object()
							|| !propertyValue.contains("kind")
							|| !propertyValue["kind"].is_string()
							|| !propertyValue.contains("value")
							|| !propertyValue["value"].is_string())
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的元数据属性格式无效。";
							return false;
						}
						DesignerStyleValue value;
						if (!DesignerStyleSheetUtils::TryParseValueKind(
							FromUtf8(propertyValue["kind"].get<std::string>()), value.Kind))
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的元数据属性类型无效。";
							return false;
						}
						value.Text = FromUtf8(propertyValue["value"].get<std::string>());
						if (propertyValue.contains("object"))
							value.ObjectValue = propertyValue["object"];
						std::wstring authoredResourceKey;
						std::wstring authoredDynamicResourceKey;
						if (propertyValue.contains("resourceKey")
							&& propertyValue.contains("dynamicResourceKey"))
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的元数据属性不能同时使用静态和动态资源。";
							return false;
						}
						if (propertyValue.contains("resourceKey"))
						{
							if (!propertyValue["resourceKey"].is_string())
							{
								if (outError) *outError = L"控件 " + it.name
									+ L" 的元数据资源引用格式无效。";
								return false;
							}
							authoredResourceKey = FromUtf8(
								propertyValue["resourceKey"].get<std::string>());
							const auto* resource = findScopedResource(
								it, authoredResourceKey);
							if (!resource)
							{
								if (outError) *outError = L"控件 " + it.name
									+ L" 的属性 " + FromUtf8(propertyKey)
									+ L" 引用了不存在的资源：" + authoredResourceKey;
								return false;
							}
							value = resource->Value;
						}
						if (propertyValue.contains("dynamicResourceKey"))
						{
							if (!propertyValue["dynamicResourceKey"].is_string())
							{
								if (outError) *outError = L"控件 " + it.name
									+ L" 的动态资源引用格式无效。";
								return false;
							}
							authoredDynamicResourceKey = FromUtf8(
								propertyValue["dynamicResourceKey"].get<std::string>());
							if (authoredDynamicResourceKey.empty())
							{
								if (outError) *outError = L"控件 " + it.name
									+ L" 的动态资源键不能为空。";
								return false;
							}
						}
						const auto propertyName = FromUtf8(propertyKey);
						std::wstring canonicalName;
						DesignerStyleValue effective;
						std::wstring metadataError;
						if (!DesignerPropertyCatalog::ApplyAndTrackValue(
							*c, dc->MetadataProperties,
							propertyName, value,
							&canonicalName, &effective, &metadataError,
							document.ResourceBasePath, document.Resources))
						{
							if (outError) *outError = L"控件 " + it.name + L"：" + metadataError;
							return false;
						}
						if (!authoredResourceKey.empty())
							dc->MetadataPropertyResourceKeys[canonicalName]
								= std::move(authoredResourceKey);
						if (!authoredDynamicResourceKey.empty())
						{
							if (!c->SetDynamicResource(
								canonicalName, authoredDynamicResourceKey))
							{
								if (outError) *outError = L"控件 " + it.name
									+ L" 无法安装属性 " + canonicalName
									+ L" 的动态资源表达式。";
								return false;
							}
							dc->MetadataPropertyDynamicResourceKeys[canonicalName]
								= std::move(authoredDynamicResourceKey);
						}
					}
				}
			}

			auto migrateLegacyMetadata = [&](const wchar_t* propertyName,
				DesignerStyleValue value) -> bool
			{
				std::wstring metadataError;
				if (!ApplyTrackedMetadataProperty(
					*dc, *c, propertyName, std::move(value), true, &metadataError))
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的旧格式属性迁移失败：" + metadataError;
					return false;
				}
				return true;
			};

			if (it.extra.is_object())
			{
				if (it.extra.contains("clip"))
				{
					cui::drawing::Geometry clip;
					std::wstring clipError;
					if (!GeometryFromValue(it.extra["clip"], clip, &clipError))
					{
						if (outError) *outError = L"控件 " + it.name + L"：" + clipError;
						return false;
					}
					c->SetClip(clip);
				}
				if (it.extra.contains("renderTransformOrigin"))
				{
					const auto& origin = it.extra["renderTransformOrigin"];
					if (!origin.is_object())
					{
						if (outError) *outError = L"控件 " + it.name
							+ L" 的 RenderTransformOrigin 格式无效。";
						return false;
					}
					c->SetRenderTransformOrigin(D2D1::Point2F(
						static_cast<float>(origin.value("x", 0.0)),
						static_cast<float>(origin.value("y", 0.0))));
				}
				if (it.extra.contains("renderTransform"))
				{
					cui::drawing::Transform transform;
					std::wstring transformError;
					if (!TransformFromValue(
						it.extra["renderTransform"], transform, &transformError))
					{
						if (outError) *outError = L"控件 " + it.name + L"：" + transformError;
						return false;
					}
					c->SetRenderTransform(transform);
				}
				if (it.extra.contains("foregroundBrush"))
				{
					DesignerStyleValue brushValue;
					brushValue.Kind = DesignerStyleValueKind::Brush;
					brushValue.ObjectValue = it.extra["foregroundBrush"];
					BindingValue convertedBrush;
					cui::drawing::Brush brush;
					std::wstring brushError;
					if (!DesignerStyleSheetUtils::TryConvertValue(
						brushValue, convertedBrush, &brushError,
						document.ResourceBasePath, document.Resources)
						|| !convertedBrush.TryGet(brush))
					{
						if (outError) *outError = L"控件 " + it.name + L"：" + brushError;
						return false;
					}
					c->SetForegroundBrush(brush);
				}
				if (it.type == UIClass::UI_GridPanel)
				{
					auto* gridPanel = (GridPanel*)c;
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
				else if (it.type == UIClass::UI_TabControl)
				{
					auto* tabControl = (TabControl*)c;
					if (it.extra.contains("selectedIndex")
						&& !migrateLegacyMetadata(L"SelectedIndex", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"selectedIndex", tabControl->SelectedIndex)) })) return false;
					if (it.extra.contains("titleHeight")
						&& !migrateLegacyMetadata(L"TitleHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value(
								"titleHeight", static_cast<float>(tabControl->TitleHeight))) })) return false;
					if (it.extra.contains("titleWidth")
						&& !migrateLegacyMetadata(L"TitleWidth", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value(
								"titleWidth", static_cast<float>(tabControl->TitleWidth))) })) return false;
					if (it.extra.contains("titlePosition")
						&& !migrateLegacyMetadata(L"TitlePosition", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"titlePosition", static_cast<int>(tabControl->TitlePosition))) })) return false;
					if (it.extra.contains("animationMode")
						&& !migrateLegacyMetadata(L"AnimationMode", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"animationMode", static_cast<int>(tabControl->AnimationMode))) })) return false;
					if (it.extra.contains("pages") && it.extra["pages"].is_array())
					{
						for (auto& pj : it.extra["pages"])
						{
							if (!pj.is_object()) continue;
							std::wstring id = FromUtf8(pj.value("id", std::string()));
							auto text = FromUtf8(pj.value("text", std::string("Page")));
							auto* page = tabControl->AddPage(text);
							if (page)
								tabPageOf[id] = page;
						}
					}
				}
				else if (it.type == UIClass::UI_StackPanel)
				{
					Orientation o;
					if (it.extra.contains("orientation") && it.extra["orientation"].is_string() && TryParseOrientation(it.extra["orientation"].get<std::string>(), o))
					{
						if (!migrateLegacyMetadata(L"Orientation", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(o)) })) return false;
					}
					if (it.extra.contains("spacing"))
					{
						if (!migrateLegacyMetadata(L"Spacing", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("spacing", 0.0f)) })) return false;
					}
					HorizontalAlignment horizontalAlignment = HorizontalAlignment::Stretch;
					VerticalAlignment verticalAlignment = VerticalAlignment::Stretch;
					if (it.extra.contains("horizontalContentAlignment")
						&& it.extra["horizontalContentAlignment"].is_string()
						&& TryParseHAlign(it.extra["horizontalContentAlignment"].get<std::string>(), horizontalAlignment))
					{
						if (!migrateLegacyMetadata(L"HorizontalContentAlignment", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(horizontalAlignment)) })) return false;
					}
					if (it.extra.contains("verticalContentAlignment")
						&& it.extra["verticalContentAlignment"].is_string()
						&& TryParseVAlign(it.extra["verticalContentAlignment"].get<std::string>(), verticalAlignment))
					{
						if (!migrateLegacyMetadata(L"VerticalContentAlignment", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(verticalAlignment)) })) return false;
					}
				}
				else if (it.type == UIClass::UI_WrapPanel)
				{
					Orientation o;
					if (it.extra.contains("orientation") && it.extra["orientation"].is_string() && TryParseOrientation(it.extra["orientation"].get<std::string>(), o))
					{
						if (!migrateLegacyMetadata(L"Orientation", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(o)) })) return false;
					}
					if (it.extra.contains("itemWidth"))
					{
						if (!migrateLegacyMetadata(L"ItemWidth", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("itemWidth", 0.0f)) })) return false;
					}
					if (it.extra.contains("itemHeight"))
					{
						if (!migrateLegacyMetadata(L"ItemHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("itemHeight", 0.0f)) })) return false;
					}
				}
				else if (it.type == UIClass::UI_DockPanel)
				{
					if (it.extra.contains("lastChildFill"))
					{
						if (!migrateLegacyMetadata(L"LastChildFill", {
							DesignerStyleValueKind::Bool,
							it.extra.value("lastChildFill", true) ? L"true" : L"false" }))
							return false;
					}
				}
				else if (it.type == UIClass::UI_ToolBar)
				{
					auto* toolBar = (ToolBar*)c;
					if (it.extra.contains("padding")
						&& !migrateLegacyMetadata(L"HorizontalPadding", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"padding", toolBar->HorizontalPadding)) })) return false;
					if (it.extra.contains("gap")
						&& !migrateLegacyMetadata(L"Gap", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("gap", toolBar->Gap)) })) return false;
					if (it.extra.contains("itemHeight")
						&& !migrateLegacyMetadata(L"ItemHeight", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"itemHeight", toolBar->ItemHeight)) })) return false;
				}
				else if (it.type == UIClass::UI_ScrollView)
				{
					auto* scrollView = (ScrollView*)c;
					if (it.extra.contains("scrollBackColor")
						&& !migrateLegacyMetadata(L"ScrollBackColor", {
							DesignerStyleValueKind::Color,
							ColorToMetadataText(ColorFromValue(
								it.extra["scrollBackColor"], scrollView->ScrollBackColor)) })) return false;
					if (it.extra.contains("scrollForeColor")
						&& !migrateLegacyMetadata(L"ScrollForeColor", {
							DesignerStyleValueKind::Color,
							ColorToMetadataText(ColorFromValue(
								it.extra["scrollForeColor"], scrollView->ScrollForeColor)) })) return false;
					if (it.extra.contains("autoContentSize")
						&& !migrateLegacyMetadata(L"AutoContentSize", {
							DesignerStyleValueKind::Bool,
							it.extra.value("autoContentSize", scrollView->AutoContentSize)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("contentSize") && it.extra["contentSize"].is_object())
					{
						auto& cs = it.extra["contentSize"];
						if (!migrateLegacyMetadata(L"ContentSize", {
							DesignerStyleValueKind::Size,
							std::to_wstring(cs.value("w", scrollView->ContentSize.cx))
								+ L", " + std::to_wstring(cs.value("h", scrollView->ContentSize.cy)) })) return false;
					}
					if (it.extra.contains("alwaysShowVScroll")
						&& !migrateLegacyMetadata(L"AlwaysShowVScroll", {
							DesignerStyleValueKind::Bool,
							it.extra.value("alwaysShowVScroll", scrollView->AlwaysShowVScroll)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("alwaysShowHScroll")
						&& !migrateLegacyMetadata(L"AlwaysShowHScroll", {
							DesignerStyleValueKind::Bool,
							it.extra.value("alwaysShowHScroll", scrollView->AlwaysShowHScroll)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("mouseWheelStep")
						&& !migrateLegacyMetadata(L"MouseWheelStep", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("mouseWheelStep", scrollView->MouseWheelStep)) })) return false;
					// Scroll offsets are observable runtime state, not design configuration.
					// Old files remain readable, but new saves intentionally omit them.
					scrollView->ScrollXOffset = it.extra.value("scrollXOffset", scrollView->ScrollXOffset);
					scrollView->ScrollYOffset = it.extra.value("scrollYOffset", scrollView->ScrollYOffset);
				}
				else if (it.type == UIClass::UI_ComboBox)
				{
					auto* comboBox = (ComboBox*)c;
					std::vector<std::wstring> items;
					if (it.extra.contains("items") && it.extra["items"].is_array())
					{
						for (auto& sj : it.extra["items"])
							if (sj.is_string()) items.push_back(FromUtf8(sj.get<std::string>()));
					}
					comboBox->Items = items;
					if (it.extra.contains("expandCount")
						&& !migrateLegacyMetadata(L"ExpandCount", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"expandCount", comboBox->ExpandCount)) })) return false;
					if (it.extra.contains("selectedIndex")
						&& !migrateLegacyMetadata(L"SelectedIndex", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"selectedIndex", comboBox->SelectedIndex)) })) return false;
				}
			else if (it.type == UIClass::UI_ListView)
			{
				auto* listView = (ListView*)c;
				if (it.extra.contains("viewMode")
					&& !migrateLegacyMetadata(L"ViewMode", {
						DesignerStyleValueKind::Int,
						std::to_wstring(it.extra.value(
							"viewMode", static_cast<int>(listView->ViewMode))) })) return false;
				if (it.extra.contains("selectionMode")
					&& !migrateLegacyMetadata(L"SelectionMode", {
						DesignerStyleValueKind::Int,
						std::to_wstring(it.extra.value(
							"selectionMode", static_cast<int>(listView->SelectionMode))) })) return false;
				if (it.extra.contains("showCheckBoxes")
					&& !migrateLegacyMetadata(L"ShowCheckBoxes", {
						DesignerStyleValueKind::Bool,
						it.extra.value("showCheckBoxes", listView->ShowCheckBoxes)
							? L"true" : L"false" })) return false;
				if (it.extra.contains("showColumnHeaders")
					&& !migrateLegacyMetadata(L"ShowColumnHeaders", {
						DesignerStyleValueKind::Bool,
						it.extra.value("showColumnHeaders", listView->ShowColumnHeaders)
							? L"true" : L"false" })) return false;
				if (it.extra.contains("alternatingRows")
					&& !migrateLegacyMetadata(L"AlternatingRows", {
						DesignerStyleValueKind::Bool,
						it.extra.value("alternatingRows", listView->AlternatingRows)
							? L"true" : L"false" })) return false;
				if (it.extra.contains("rowHeight")
					&& !migrateLegacyMetadata(L"RowHeight", {
						DesignerStyleValueKind::Float,
						std::to_wstring(it.extra.value("rowHeight", listView->RowHeight)) })) return false;
				if (it.extra.contains("tileHeight")
					&& !migrateLegacyMetadata(L"TileHeight", {
						DesignerStyleValueKind::Float,
						std::to_wstring(it.extra.value("tileHeight", listView->TileHeight)) })) return false;
				if (it.extra.contains("iconSize")
					&& !migrateLegacyMetadata(L"IconSize", {
						DesignerStyleValueKind::Float,
						std::to_wstring(it.extra.value("iconSize", listView->IconSize)) })) return false;
				if (it.extra.contains("selectedItemBackColor")
					&& !migrateLegacyMetadata(L"SelectedItemBackColor", {
						DesignerStyleValueKind::Color,
						ColorToMetadataText(ColorFromValue(
							it.extra["selectedItemBackColor"], listView->SelectedItemBackColor)) })) return false;
				if (it.extra.contains("underMouseItemBackColor")
					&& !migrateLegacyMetadata(L"UnderMouseItemBackColor", {
						DesignerStyleValueKind::Color,
						ColorToMetadataText(ColorFromValue(
							it.extra["underMouseItemBackColor"], listView->UnderMouseItemBackColor)) })) return false;
				if (it.extra.contains("selectedItemForeColor")
					&& !migrateLegacyMetadata(L"SelectedItemForeColor", {
						DesignerStyleValueKind::Color,
						ColorToMetadataText(ColorFromValue(
							it.extra["selectedItemForeColor"], listView->SelectedItemForeColor)) })) return false;
				listView->ClearColumns();
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& cj : it.extra["columns"])
						{
							if (!cj.is_object()) continue;
							ListViewColumn col;
							col.Header = FromUtf8(cj.value("header", std::string()));
							col.Width = cj.value("width", col.Width);
							col.Align = (ListViewCellAlign)cj.value("align", (int)col.Align);
							listView->Columns.push_back(col);
						}
					}
				std::vector<ListViewItem> items;
				if (it.extra.contains("items"))
					ValueToListViewItems(it.extra["items"], items);
				listView->SetItems(std::move(items));
			}
				else if (it.type == UIClass::UI_GridView)
				{
					auto* gridView = (GridView*)c;
					auto update = gridView->DeferUpdates();
					gridView->ClearColumns();
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& cj : it.extra["columns"])
						{
							if (!cj.is_object()) continue;
							GridViewColumn col;
							col.Name = FromUtf8(cj.value("name", std::string()));
							col.Width = cj.value("width", col.Width);
							col.Type = (ColumnType)cj.value("type", (int)col.Type);
							col.CanEdit = cj.value("canEdit", col.CanEdit);
							col.ButtonText = FromUtf8(cj.value("buttonText", std::string()));
							if (cj.contains("comboBoxItems") && cj["comboBoxItems"].is_array())
							{
								for (const auto& item : cj["comboBoxItems"])
								{
									if (item.is_string())
										col.ComboBoxItems.push_back(FromUtf8(item.get<std::string>()));
								}
							}
							gridView->AddColumn(col);
						}
					}
					std::vector<GridViewRow> rows;
					if (it.extra.contains("rows"))
						ValueToGridRows(it.extra["rows"], rows);
					gridView->SetRows(std::move(rows));
				}
				else if (it.type == UIClass::UI_PagedGridView)
				{
					auto* gridView = static_cast<PagedGridView*>(c);
					auto update = gridView->DeferUpdates();
					gridView->ClearColumns();
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (const auto& columnValue : it.extra["columns"])
						{
							if (!columnValue.is_object()) continue;
							GridViewColumn column;
							column.Name = FromUtf8(columnValue.value("name", std::string{}));
							column.Width = static_cast<float>(columnValue.value("width", 120.0));
							column.Type = static_cast<ColumnType>(columnValue.value("type", 0));
							column.CanEdit = columnValue.value("canEdit", true);
							column.ButtonText = FromUtf8(
								columnValue.value("buttonText", std::string{}));
							if (columnValue.contains("comboBoxItems")
								&& columnValue["comboBoxItems"].is_array())
								for (const auto& option : columnValue["comboBoxItems"])
									if (option.is_string()) column.ComboBoxItems.push_back(
										FromUtf8(option.get<std::string>()));
							gridView->AddColumn(column);
						}
					}
					std::vector<GridViewRow> rows;
					if (it.extra.contains("rows"))
						ValueToGridRows(it.extra["rows"], rows);
					gridView->SetRows(std::move(rows));
				}
				else if (it.type == UIClass::UI_PropertyGrid)
				{
					auto* pg = (PropertyGridView*)c;
					if (it.extra.contains("showHeader")
						&& !migrateLegacyMetadata(L"ShowHeader", {
							DesignerStyleValueKind::Bool,
							it.extra.value("showHeader", pg->ShowHeader) ? L"true" : L"false" })) return false;
					if (it.extra.contains("showCategories")
						&& !migrateLegacyMetadata(L"ShowCategories", {
							DesignerStyleValueKind::Bool,
							it.extra.value("showCategories", pg->ShowCategories) ? L"true" : L"false" })) return false;
					if (it.extra.contains("alternatingRows")
						&& !migrateLegacyMetadata(L"AlternatingRows", {
							DesignerStyleValueKind::Bool,
							it.extra.value("alternatingRows", pg->AlternatingRows) ? L"true" : L"false" })) return false;
					if (it.extra.contains("allowEditing")
						&& !migrateLegacyMetadata(L"AllowEditing", {
							DesignerStyleValueKind::Bool,
							it.extra.value("allowEditing", pg->AllowEditing) ? L"true" : L"false" })) return false;
					if (it.extra.contains("rowHeight")
						&& !migrateLegacyMetadata(L"RowHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("rowHeight", pg->RowHeight)) })) return false;
					if (it.extra.contains("categoryHeight")
						&& !migrateLegacyMetadata(L"CategoryHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("categoryHeight", pg->CategoryHeight)) })) return false;
					if (it.extra.contains("nameColumnWidth")
						&& !migrateLegacyMetadata(L"NameColumnWidth", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("nameColumnWidth", pg->NameColumnWidth)) })) return false;
					std::vector<PropertyGridItem> items;
					if (it.extra.contains("items"))
						ValueToPropertyGridItems(it.extra["items"], items);
					pg->SetItems(std::move(items));
				}
				else if (it.type == UIClass::UI_TreeView)
				{
					auto* treeView = (TreeView*)c;
					if (treeView->Root)
					{
						for (auto node : treeView->Root->Children) delete node;
						treeView->Root->Children.clear();
						if (it.extra.contains("nodes"))
							ValueToTreeNodes(it.extra["nodes"], treeView->Root->Children);
					}
					treeView->SelectedBackColor = ColorFromValue(it.extra.contains("selectedBackColor") ? it.extra["selectedBackColor"] : DesignValue(), treeView->SelectedBackColor);
					treeView->UnderMouseItemBackColor = ColorFromValue(it.extra.contains("underMouseItemBackColor") ? it.extra["underMouseItemBackColor"] : DesignValue(), treeView->UnderMouseItemBackColor);
					treeView->SelectedForeColor = ColorFromValue(it.extra.contains("selectedForeColor") ? it.extra["selectedForeColor"] : DesignValue(), treeView->SelectedForeColor);
				}
				else if (it.type == UIClass::UI_ProgressBar)
				{
					((ProgressBar*)c)->PercentageValue = it.extra.value("percentageValue", ((ProgressBar*)c)->PercentageValue);
				}
				else if (it.type == UIClass::UI_LoadingRing)
				{
					((LoadingRing*)c)->Active = it.extra.value("active", ((LoadingRing*)c)->Active);
				}
				else if (it.type == UIClass::UI_ProgressRing)
				{
					auto* progressRing = (ProgressRing*)c;
					progressRing->PercentageValue = it.extra.value("percentageValue", progressRing->PercentageValue);
					progressRing->ShowPercentage = it.extra.value("showPercentage", progressRing->ShowPercentage);
				}
				else if (it.type == UIClass::UI_DateTimePicker)
				{
					auto* dateTimePicker = (DateTimePicker*)c;
					if (it.extra.contains("value") && it.extra["value"].is_object())
					{
						SYSTEMTIME st = dateTimePicker->Value;
						auto& v = it.extra["value"];
						st.wYear = (WORD)v.value("year", (int)st.wYear);
						st.wMonth = (WORD)v.value("month", (int)st.wMonth);
						st.wDay = (WORD)v.value("day", (int)st.wDay);
						st.wHour = (WORD)v.value("hour", (int)st.wHour);
						st.wMinute = (WORD)v.value("minute", (int)st.wMinute);
						st.wSecond = (WORD)v.value("second", (int)st.wSecond);
						st.wMilliseconds = (WORD)v.value("milliseconds", (int)st.wMilliseconds);
						dateTimePicker->Value = st;
					}
					dateTimePicker->Mode = (DateTimePickerMode)it.extra.value("mode", (int)dateTimePicker->Mode);
					dateTimePicker->AllowDateSelection = it.extra.value("allowDateSelection", dateTimePicker->AllowDateSelection);
					dateTimePicker->AllowTimeSelection = it.extra.value("allowTimeSelection", dateTimePicker->AllowTimeSelection);
					dateTimePicker->AllowModeSwitch = it.extra.value("allowModeSwitch", dateTimePicker->AllowModeSwitch);
					dateTimePicker->SetExpanded(it.extra.value("expand", dateTimePicker->Expand));
				}
				else if (it.type == UIClass::UI_NumericUpDown)
				{
					auto* numericUpDown = (NumericUpDown*)c;
					if (it.extra.contains("min")
						&& !migrateLegacyMetadata(L"Min", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("min", numericUpDown->Min)) })) return false;
					if (it.extra.contains("max")
						&& !migrateLegacyMetadata(L"Max", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("max", numericUpDown->Max)) })) return false;
					if (it.extra.contains("step")
						&& !migrateLegacyMetadata(L"Step", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("step", numericUpDown->Step)) })) return false;
					if (it.extra.contains("decimalPlaces")
						&& !migrateLegacyMetadata(L"DecimalPlaces", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("decimalPlaces", numericUpDown->DecimalPlaces)) })) return false;
					if (it.extra.contains("snapToStep")
						&& !migrateLegacyMetadata(L"SnapToStep", {
							DesignerStyleValueKind::Bool,
							it.extra.value("snapToStep", numericUpDown->SnapToStep)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("useMouseWheel")
						&& !migrateLegacyMetadata(L"UseMouseWheel", {
							DesignerStyleValueKind::Bool,
							it.extra.value("useMouseWheel", numericUpDown->UseMouseWheel)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("value")
						&& !migrateLegacyMetadata(L"Value", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("value", numericUpDown->Value)) })) return false;
				}
				else if (it.type == UIClass::UI_GroupBox)
				{
					auto* groupBox = (GroupBox*)c;
					if (it.extra.contains("captionMarginLeft")
						&& !migrateLegacyMetadata(L"CaptionMarginLeft", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("captionMarginLeft", (double)groupBox->CaptionMarginLeft)) })) return false;
					if (it.extra.contains("captionPaddingX")
						&& !migrateLegacyMetadata(L"CaptionPaddingX", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("captionPaddingX", (double)groupBox->CaptionPaddingX)) })) return false;
					if (it.extra.contains("captionPaddingY")
						&& !migrateLegacyMetadata(L"CaptionPaddingY", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("captionPaddingY", (double)groupBox->CaptionPaddingY)) })) return false;
				}
				else if (it.type == UIClass::UI_Expander)
				{
					auto* expander = (Expander*)c;
					if (it.extra.contains("headerHeight")
						&& !migrateLegacyMetadata(L"HeaderHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("headerHeight", (double)expander->HeaderHeight)) })) return false;
					if (it.extra.contains("animationDurationMs")
						&& !migrateLegacyMetadata(L"AnimationDurationMs", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("animationDurationMs", (int)expander->AnimationDurationMs)) })) return false;
					if (it.extra.contains("isExpanded")
						&& !migrateLegacyMetadata(L"IsExpanded", {
							DesignerStyleValueKind::Bool,
							it.extra.value("isExpanded", expander->IsExpanded)
								? L"true" : L"false" })) return false;
				}
				else if (it.type == UIClass::UI_SplitContainer)
				{
					Orientation orientation = Orientation::Horizontal;
					if (it.extra.contains("splitOrientation") && it.extra["splitOrientation"].is_string())
					{
						if (TryParseOrientation(
							it.extra["splitOrientation"].get<std::string>(), orientation))
						{
							if (!migrateLegacyMetadata(L"SplitOrientation", {
								DesignerStyleValueKind::Int,
								std::to_wstring(static_cast<int>(orientation)) })) return false;
						}
					}
					if (it.extra.contains("splitterWidth"))
					{
						if (!migrateLegacyMetadata(L"SplitterWidth", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("splitterWidth", 6)) })) return false;
					}
					if (it.extra.contains("panel1MinSize"))
					{
						if (!migrateLegacyMetadata(L"Panel1MinSize", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("panel1MinSize", 48)) })) return false;
					}
					if (it.extra.contains("panel2MinSize"))
					{
						if (!migrateLegacyMetadata(L"Panel2MinSize", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("panel2MinSize", 48)) })) return false;
					}
					if (it.extra.contains("splitterDistance"))
					{
						if (!migrateLegacyMetadata(L"SplitterDistance", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("splitterDistance", 160)) })) return false;
					}
					if (it.extra.contains("isSplitterFixed"))
					{
						if (!migrateLegacyMetadata(L"IsSplitterFixed", {
							DesignerStyleValueKind::Bool,
							it.extra.value("isSplitterFixed", false) ? L"true" : L"false" }))
							return false;
					}
				}
				else if (it.type == UIClass::UI_Slider)
				{
					auto* slider = (Slider*)c;
					if (it.extra.contains("min")
						&& !migrateLegacyMetadata(L"Min", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("min", slider->Min)) })) return false;
					if (it.extra.contains("max")
						&& !migrateLegacyMetadata(L"Max", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("max", slider->Max)) })) return false;
					if (it.extra.contains("step")
						&& !migrateLegacyMetadata(L"Step", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("step", slider->Step)) })) return false;
					if (it.extra.contains("snapToStep")
						&& !migrateLegacyMetadata(L"SnapToStep", {
							DesignerStyleValueKind::Bool,
							it.extra.value("snapToStep", slider->SnapToStep)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("value")
						&& !migrateLegacyMetadata(L"Value", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("value", slider->Value)) })) return false;
				}
				else if (it.type == UIClass::UI_StatusBar)
				{
					auto* statusBar = (StatusBar*)c;
					if (it.extra.contains("topMost")
						&& !migrateLegacyMetadata(L"TopMost", {
							DesignerStyleValueKind::Bool,
							it.extra.value("topMost", statusBar->TopMost)
								? L"true" : L"false" })) return false;
					statusBar->ClearParts();
					if (it.extra.contains("parts") && it.extra["parts"].is_array())
					{
						for (auto& pj : it.extra["parts"])
						{
							if (!pj.is_object()) continue;
							std::wstring text = FromUtf8(pj.value("text", std::string()));
							int w = pj.value("width", 0);
							statusBar->AddPart(text, w);
						}
					}
				}
				else if (it.type == UIClass::UI_NavigationView
					|| it.type == UIClass::UI_SideBar)
				{
					auto* navigation = static_cast<NavigationView*>(c);
					const int requestedSelection = navigation->SelectedIndex;
					navigation->ClearItems();
					int itemSelection = -1;
					if (it.extra.contains("navigationItems")
						&& it.extra["navigationItems"].is_array())
					{
						for (const auto& value : it.extra["navigationItems"])
						{
							if (!value.is_object()) continue;
							NavigationViewItem item;
							item.Text = FromUtf8(value.value("text", std::string{}));
							item.Value = FromUtf8(value.value("value", std::string{}));
							item.BadgeText = FromUtf8(value.value("badgeText", std::string{}));
							const auto iconUri = FromUtf8(
								value.value("icon", std::string{}));
							if (!iconUri.empty())
							{
								BindingValue convertedIcon;
								std::wstring imageError;
								if (!DesignerStyleSheetUtils::TryConvertValue(
									{ DesignerStyleValueKind::ImageSource, iconUri },
									convertedIcon, &imageError,
									document.ResourceBasePath, document.Resources)
									|| !convertedIcon.TryGet(item.Icon))
								{
									if (outError) *outError = L"控件 " + it.name
										+ L" 的导航图标加载失败：" + imageError;
									return false;
								}
							}
							const int kind = (std::clamp)(value.value("kind", 0), 0, 2);
							item.Kind = static_cast<NavigationViewItemKind>(kind);
							item.Enabled = value.value("enabled", kind == 0);
							item.Selected = false;
							item.Tag = value.value("tag", static_cast<UINT64>(0));
							const int index = navigation->AddItem(item);
							if (kind == 0 && value.value("selected", false)
								&& itemSelection < 0) itemSelection = index;
						}
					}
					const int selection = itemSelection >= 0
						? itemSelection : requestedSelection;
					if (selection >= 0) navigation->SelectItem(selection);
					else navigation->ClearSelection();
				}
				else if (it.type == UIClass::UI_BreadcrumbBar)
				{
					auto* breadcrumb = static_cast<BreadcrumbBar*>(c);
					const int requestedSelection = breadcrumb->SelectedIndex;
					breadcrumb->ClearItems();
					if (it.extra.contains("breadcrumbItems")
						&& it.extra["breadcrumbItems"].is_array())
					{
						for (const auto& value : it.extra["breadcrumbItems"])
						{
							if (!value.is_object()) continue;
							BreadcrumbBarItem item(
								FromUtf8(value.value("text", std::string{})),
								FromUtf8(value.value("value", std::string{})));
							item.Enabled = value.value("enabled", true);
							item.Tag = value.value("tag", static_cast<UINT64>(0));
							breadcrumb->AddItem(item);
						}
					}
					if (requestedSelection >= 0)
						breadcrumb->SelectItem(requestedSelection);
				}
				else if (it.type == UIClass::UI_FilterBar)
				{
					auto* filter = static_cast<FilterBar*>(c);
					filter->ClearItems();
					if (it.extra.contains("filterItems")
						&& it.extra["filterItems"].is_array())
					{
						for (const auto& value : it.extra["filterItems"])
						{
							if (!value.is_object()) continue;
							FilterBarItem item(
								FromUtf8(value.value("text", std::string{})),
								FromUtf8(value.value("value", std::string{})),
								value.value("selected", false));
							item.Enabled = value.value("enabled", true);
							item.Tag = value.value("tag", static_cast<UINT64>(0));
							filter->AddItem(item);
						}
					}
				}
				else if (it.type == UIClass::UI_KpiCard)
				{
					auto* kpi = static_cast<KpiCard*>(c);
					std::vector<double> values;
					if (it.extra.contains("sparkline")
						&& it.extra["sparkline"].is_array())
					{
						for (const auto& value : it.extra["sparkline"])
							if (value.is_number()) values.push_back(value.get<double>());
					}
					kpi->SetSparkline(values);
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
				else if (it.type == UIClass::UI_ReportView)
				{
					auto* report = static_cast<ReportView*>(c);
					report->Clear();
					if (it.extra.contains("reportColumns")
						&& it.extra["reportColumns"].is_array())
					{
						for (const auto& value : it.extra["reportColumns"])
						{
							if (!value.is_object()) continue;
							report->AddColumn(ReportColumn(
								FromUtf8(value.value("header", std::string{})),
								static_cast<float>(value.value("width", 120.0)),
								static_cast<ReportCellAlign>((std::clamp)(
									value.value("align", 0), 0, 2)),
								value.value("sortable", true)));
						}
					}
					if (it.extra.contains("reportRows")
						&& it.extra["reportRows"].is_array())
					{
						for (const auto& value : it.extra["reportRows"])
						{
							if (!value.is_object()) continue;
							std::vector<std::wstring> cells;
							if (value.contains("cells") && value["cells"].is_array())
								for (const auto& cell : value["cells"])
									if (cell.is_string()) cells.push_back(FromUtf8(cell.get<std::string>()));
							const int kind = (std::clamp)(value.value("kind", 0), 0, 2);
							const auto caption = FromUtf8(value.value("caption", std::string{}));
							ReportRow row = kind == 1
								? ReportRow::Group(caption, value.value("expanded", true))
								: kind == 2 ? ReportRow::Summary(caption, std::move(cells))
								: ReportRow(std::move(cells));
							row.Caption = caption;
							row.Tag = value.value("tag", static_cast<UINT64>(0));
							report->AddRow(row);
						}
					}
				}
				else if (it.type == UIClass::UI_MediaPlayer)
				{
					auto* mediaPlayer = (MediaPlayer*)c;
					// 旧文档标量迁移到统一元数据；新文档只在 extra 保留媒体源路径。
					if (it.extra.contains("autoPlay")
						&& !migrateLegacyMetadata(L"AutoPlay", {
							DesignerStyleValueKind::Bool,
							it.extra.value("autoPlay", mediaPlayer->AutoPlay)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("loop")
						&& !migrateLegacyMetadata(L"Loop", {
							DesignerStyleValueKind::Bool,
							it.extra.value("loop", mediaPlayer->Loop)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("volume")
						&& !migrateLegacyMetadata(L"Volume", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("volume", mediaPlayer->Volume)) })) return false;
					if (it.extra.contains("playbackRate")
						&& !migrateLegacyMetadata(L"PlaybackRate", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value(
								"playbackRate", (double)mediaPlayer->PlaybackRate)) })) return false;
					if (it.extra.contains("renderMode")
						&& !migrateLegacyMetadata(L"RenderMode", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"renderMode", (int)mediaPlayer->RenderMode)) })) return false;
					if (it.extra.contains("mediaFile") && it.extra["mediaFile"].is_string())
						dc->DesignStrings[L"mediaFile"] = FromUtf8(it.extra["mediaFile"].get<std::string>());
					else
						dc->DesignStrings.erase(L"mediaFile");
				}
				else if (it.type == UIClass::UI_Menu)
				{
					auto* m = (Menu*)c;
					// 清空现有顶层项
					while (m->Count > 0)
					{
						auto* cc = m->operator[](m->Count - 1);
						m->DeleteControl(cc);
					}
					if (it.extra.contains("items") && it.extra["items"].is_array())
					{
						for (auto& ij : it.extra["items"])
						{
							if (!ij.is_object()) continue;
							bool sep = ij.value("separator", false);
							if (sep) continue; // 顶层不支持 separator
							auto text = FromUtf8(ij.value("text", std::string()));
							if (text.empty()) continue;
							auto* top = m->AddItem(text);
							if (!top) continue;
							top->Id = ij.value("id", 0);
							top->Shortcut = FromUtf8(ij.value("shortcut", std::string()));
							top->Enable = ij.value("enable", true);
							if (ij.contains("subItems"))
							{
								std::vector<MenuItem*> subItems;
								ValueToMenuSubItems(ij["subItems"], subItems, top);
							}
						}
					}
				}
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
			if ((it.type != UIClass::UI_ItemsControl
				&& it.type != UIClass::UI_ListBox)
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
			style->HeaderIndent = definition->HeaderIndent;
			style->HeaderSpacing = definition->HeaderSpacing;
			style->HeaderHeight = definition->HeaderHeight;
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
						options, groupSchema));
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
			if ((it.type != UIClass::UI_ItemsControl
				&& it.type != UIClass::UI_ListBox)
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
			itemsControl->SetItemsPanel(ItemsPanelTemplateReference(
				std::make_shared<ItemsPanelTemplate>(definition->Value)));
			if (!itemsControl->LastTemplateError().empty())
			{
				if (outError) *outError = itemsControl->LastTemplateError();
				return false;
			}
		}
		for (auto& it : items)
		{
			if ((it.type != UIClass::UI_ListBox
				&& it.type != UIClass::UI_ComboBox
				&& it.type != UIClass::UI_TreeView)
				|| !it.extra.is_object()
				|| !it.extra.contains("itemContainerStyle")) continue;
			const auto found = instOf.find(it.name);
			auto* selector = found == instOf.end()
				? nullptr : dynamic_cast<Selector*>(found->second);
			auto* combo = found == instOf.end()
				? nullptr : dynamic_cast<ComboBox*>(found->second);
			auto* tree = found == instOf.end()
				? nullptr : dynamic_cast<TreeView*>(found->second);
			if ((!selector && !combo && !tree)
				|| !it.extra["itemContainerStyle"].is_string())
			{
				if (outError) *outError = L"ItemContainerStyle 格式无效："
					+ it.name;
				return false;
			}
			const auto styleId = FromUtf8(
				it.extra["itemContainerStyle"].get<std::string>());
			if (selector) selector->SetItemContainerStyle(styleId);
			else if (combo) combo->SetItemContainerStyle(styleId);
			else tree->SetItemContainerStyle(styleId);
		}
		for (auto& it : items)
		{
			if (it.type != UIClass::UI_ListBox
				&& it.type != UIClass::UI_ComboBox
				&& it.type != UIClass::UI_TreeView) continue;
			const auto found = instOf.find(it.name);
			auto* selector = found == instOf.end()
				? nullptr : dynamic_cast<Selector*>(found->second);
			auto* combo = found == instOf.end()
				? nullptr : dynamic_cast<ComboBox*>(found->second);
			auto* tree = found == instOf.end()
				? nullptr : dynamic_cast<TreeView*>(found->second);
			if (!selector && !combo && !tree)
			{
				if (outError) *outError =
					L"项容器模板宿主类型无效：" + it.name;
				return false;
			}
			if (combo) combo->SetUseGeneratedItemContainers(true);
			if (tree) tree->SetUseGeneratedItemContainers(true);
			const auto containerType = combo
				? UIClass::UI_ComboBoxItem
				: tree ? UIClass::UI_TreeViewItem : UIClass::UI_SelectorItem;
			const auto containerStyle = combo
				? combo->GetItemContainerStyle()
				: tree ? tree->GetItemContainerStyle()
					: selector->GetItemContainerStyle();

			DesignNode probe;
			probe.Name = it.name + L"#itemContainer";
			probe.ParentRef = it.name;
			probe.Type = containerType;
			probe.Props = DesignValue::object();
			probe.Extra = DesignValue::object();
			if (!containerStyle.empty())
				probe.Props["styleId"] = Convert::UnicodeToUtf8(
					containerStyle);
			EffectiveControlTemplate effective;
			if (!ResolveEffectiveControlTemplate(
				document, document.Nodes, probe, effective, outError))
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
			auto runtimeTemplate = ControlTemplateReference(
				std::make_shared<MaterializedControlTemplate>(
					templateDocument, containerType,
					std::move(visibleObjects), visibleStyleScope(it), options,
					containerStyle));
			if (selector)
				selector->SetItemContainerTemplate(runtimeTemplate);
			else if (combo) combo->SetItemContainerTemplate(runtimeTemplate);
			else tree->SetItemContainerTemplate(runtimeTemplate);
			const auto& templateError = selector
				? selector->LastTemplateError()
				: combo ? combo->LastTemplateError() : tree->LastTemplateError();
			if (!templateError.empty())
			{
				if (outError) *outError = templateError;
				return false;
			}
		}
		for (auto& it : items)
		{
			if (!it.extra.is_object()
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
			if (auto* target = dynamic_cast<ItemsControl*>(found->second))
				target->SetItemsSource(reference);
			else if (auto* target = dynamic_cast<ComboBox*>(found->second))
				target->SetItemsSource(reference);
			else if (auto* target = dynamic_cast<TreeView*>(found->second))
				target->SetItemsSource(reference);
			else if (auto* target = dynamic_cast<ListView*>(found->second))
				target->SetItemsSource(reference);
			else
			{
				if (outError) *outError = L"控件不支持 DataList ItemsSource："
					+ it.name;
				return false;
			}
		}

		for (auto& it : items)
		{
			if (it.type != UIClass::UI_ItemsControl
				&& it.type != UIClass::UI_ListBox
				&& it.type != UIClass::UI_ComboBox
				&& it.type != UIClass::UI_TreeView) continue;
			const auto found = instOf.find(it.name);
			auto* itemsControl = found == instOf.end()
				? nullptr : dynamic_cast<ItemsControl*>(found->second);
			auto* combo = found == instOf.end()
				? nullptr : dynamic_cast<ComboBox*>(found->second);
			auto* tree = found == instOf.end()
				? nullptr : dynamic_cast<TreeView*>(found->second);
			if (!itemsControl && !combo && !tree)
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
					auto key = definition.DataType;
					std::transform(key.begin(), key.end(), key.begin(),
						[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
					(*implicitTemplates)[std::move(key)] = ItemTemplateReference(
						std::make_shared<MaterializedDataTemplate>(
							templateDocument, definition, visibleObjects,
							itemStyles, options));
				}
				tree->SetImplicitItemTemplateResolver(
					[implicitTemplates](const std::wstring& itemType)
					{
						auto key = itemType;
						std::transform(key.begin(), key.end(), key.begin(),
							[](wchar_t ch)
							{ return static_cast<wchar_t>(std::towlower(ch)); });
						const auto found = implicitTemplates->find(key);
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
				&& _wcsicmp(itemType->Name.c_str(),
					dataTemplate->DataType.c_str()) != 0)
			{
				if (outError) *outError = L"ItemsControl " + it.name
					+ L" 的 ItemsSource ItemType 与 DataTemplate.DataType 不一致。";
				return false;
			}
			auto runtimeTemplate = ItemTemplateReference(
				std::make_shared<MaterializedDataTemplate>(
					templateDocument, *dataTemplate,
					visibleObjects, itemStyles, options));
			if (itemsControl) itemsControl->SetItemTemplate(runtimeTemplate);
			else if (combo) combo->SetItemTemplate(runtimeTemplate);
			else tree->SetItemTemplate(runtimeTemplate);
			const bool installed = itemsControl
				? static_cast<bool>(itemsControl->GetItemTemplate())
				: combo ? static_cast<bool>(combo->GetItemTemplate())
					: static_cast<bool>(tree->GetItemTemplate());
			if (!installed)
			{
				if (outError) *outError = L"项控件无法安装 DataTemplate："
					+ dataTemplate->DisplayName();
				return false;
			}
		}

		for (auto& it : items)
		{
			if (!IsContentHostType(it.type)) continue;
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
			const bool hasContentBinding = it.bindings.is_object()
				&& it.bindings.contains("Content");
			const bool hasTextContent = it.extra.is_object()
				&& it.extra.contains("contentText");
			const bool hasExplicitTemplate = it.extra.is_object()
				&& it.extra.contains("contentTemplate");
			if (presenter && visualChildCount != 0
				&& it.templateContentSource.empty())
			{
				if (outError) *outError =
					L"ContentPresenter 不接受直接视觉子节点：" + it.name;
				return false;
			}
			if (presenter && visualChildCount != 0
				&& (hasContentBinding || hasTextContent || hasExplicitTemplate))
			{
				if (outError) *outError = L"ControlTemplate ContentPresenter 的视觉槽"
					L"不能与数据 Content 同时使用：" + it.name;
				return false;
			}
			if (contentControl && (visualChildCount > 1
				|| (visualChildCount != 0 && (hasContentBinding
					|| hasTextContent || hasExplicitTemplate))))
			{
				if (outError) *outError = L"ContentControl " + it.name
					+ L" 最多接受一个直接视觉子节点，且不能与数据内容同时使用。";
				return false;
			}
			if (visualChildCount != 0) continue;
			if (hasTextContent && (!it.extra["contentText"].is_string()
				|| hasContentBinding || hasExplicitTemplate))
			{
				if (outError) *outError = L"内容控件 " + it.name
					+ L" 的文本 Content 格式无效或与 Binding/ContentTemplate 冲突。";
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
			{
				if (hasTextContent)
				{
					const auto value = FromUtf8(
						it.extra["contentText"].get<std::string>());
					if (presenter) presenter->SetContent(BindingValue(value));
					else contentControl->SetContent(BindingValue(value));
					const auto& contentError = presenter
						? presenter->LastTemplateError()
						: contentControl->LastContentError();
					if (!contentError.empty())
					{
						if (outError) *outError = contentError;
						return false;
					}
				}
				continue;
			}
			if (!dataTemplate)
			{
				if (outError) *outError =
					L"内容控件引用了不存在的 DataTemplate：" + key;
				return false;
			}
			if (hasExplicitTemplate && contentType
				&& _wcsicmp(contentType->Name.c_str(),
					dataTemplate->DataType.c_str()) != 0)
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
					std::move(visibleObjects), visibleStyleScope(it), options));
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
			if (!IsHeaderedContentControlType(it.type)) continue;
			const auto found = instOf.find(it.name);
			auto* headered = found == instOf.end()
				? nullptr : dynamic_cast<HeaderedContentControl*>(found->second);
			if (!headered)
			{
				if (outError) *outError =
					L"HeaderedContentControl 物化类型不匹配：" + it.name;
				return false;
			}
			const auto visualHeaderCount = std::count_if(
				items.begin(), items.end(), [&](const auto& candidate)
				{
					return candidate.parent == it.name
						&& candidate.extra.is_object()
						&& candidate.extra.value(
							"headeredRegion", std::string{}) == "header";
				});
			const bool hasHeaderBinding = it.bindings.is_object()
				&& it.bindings.contains("Header");
			const bool hasTextHeader = it.extra.is_object()
				&& it.extra.contains("headerText");
			const bool hasExplicitTemplate = it.extra.is_object()
				&& it.extra.contains("headerTemplate");
			if (visualHeaderCount > 1 || (visualHeaderCount != 0
				&& (hasHeaderBinding || hasTextHeader || hasExplicitTemplate)))
			{
				if (outError) *outError = L"HeaderedContentControl " + it.name
					+ L" 最多接受一个视觉 Header，且不能与数据 Header 同时使用。";
				return false;
			}
			if (visualHeaderCount != 0) continue;
			if (hasTextHeader && (!it.extra["headerText"].is_string()
				|| hasHeaderBinding || hasExplicitTemplate))
			{
				if (outError) *outError = L"HeaderedContentControl " + it.name
					+ L" 的文本 Header 格式无效或与 Binding/HeaderTemplate 冲突。";
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
			headered->SetHeaderTypeName(headerType ? headerType->Name : L"");
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
			{
				if (hasTextHeader)
				{
					headered->SetHeader(BindingValue(FromUtf8(
						it.extra["headerText"].get<std::string>())));
					if (!headered->LastHeaderError().empty())
					{
						if (outError) *outError = headered->LastHeaderError();
						return false;
					}
				}
				continue;
			}
			if (!dataTemplate)
			{
				if (outError) *outError =
					L"Header 引用了不存在的 DataTemplate：" + key;
				return false;
			}
			if (hasExplicitTemplate && headerType
				&& _wcsicmp(headerType->Name.c_str(),
					dataTemplate->DataType.c_str()) != 0)
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
			headered->SetHeaderTemplate(ItemTemplateReference(
				std::make_shared<MaterializedDataTemplate>(
					templateDocument, *dataTemplate,
					std::move(visibleObjects), visibleStyleScope(it), options)));
			if (!headered->GetHeaderTemplate()
				|| !headered->LastHeaderError().empty())
			{
				if (outError) *outError = headered->LastHeaderError().empty()
					? L"HeaderedContentControl 无法安装 DataTemplate："
						+ dataTemplate->DisplayName()
					: headered->LastHeaderError();
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
			const auto source = instOf.find(it.templateOwner);
			if (target == instOf.end() || source == instOf.end()
				|| !target->second || !source->second)
			{
				if (outError) *outError = L"组件模板绑定无法解析所属实例：" + it.name;
				return false;
			}
			for (const auto& [targetProperty, sourceProperty]
				: it.templateBindings)
			{
				if (!target->second->DataBindings.Add(
					targetProperty, *source->second, sourceProperty,
					BindingMode::OneWay, DataSourceUpdateMode::Never))
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
							const auto owner = instOf.find(it.templateOwner);
							if (owner == instOf.end() || !owner->second)
							{
								if (error) *error = L"TemplatedParent 无法解析。";
								return false;
							}
							resolved.Source = owner->second;
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
							const auto parent = instOf.find(it.parent);
							resolved.Source = parent != instOf.end() && parent->second
								? &parent->second->DataContextSource()
								: &target->DataContextSource();
						}
						else resolved.Source = &target->DataContextSource();
						return resolved.Source || resolved.OwnedSource;
					};
					if (binding.Mode != BindingMode::OneWayToSource)
						(void)target->ClearPropertyValue(
							targetProperty, ControlPropertyValueSource::Local);
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
					const auto owner = instOf.find(it.templateOwner);
					if (owner == instOf.end() || !owner->second)
					{
						if (outError) *outError = L"组件模板 TemplatedParent 无法解析："
							+ it.name;
						return false;
					}
					bindingSource = owner->second;
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
					const auto parent = instOf.find(it.parent);
					bindingSource = parent != instOf.end() && parent->second
						? &parent->second->DataContextSource()
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
				if (binding.Mode != BindingMode::OneWayToSource)
					(void)target->ClearPropertyValue(
						targetProperty, ControlPropertyValueSource::Local);
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
			const auto owner = instOf.find(it.templateOwner);
			const auto ownerRecord = dcOf.find(it.templateOwner);
			if (source == instOf.end() || owner == instOf.end()
				|| ownerRecord == dcOf.end() || !source->second
				|| !owner->second || !ownerRecord->second)
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
					*source->second, sourceEvent, *owner->second,
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

		auto attachOne = [&](Pending* it, Control* runtimeParent, Control* designerParent)
		{
			if (!it) return;
			auto dc = dcOf[it->name];
			if (!dc || !dc->ControlInstance) return;
			auto* c = dc->ControlInstance;
			if (!runtimeParent) runtimeParent = &stagingRoot;
			if (!runtimeParent) return;
			auto owner = controlPool.TakeById(it->id);
			if (!owner || owner.get() != c) return;
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
					contentHost->SetControlTemplateRoot(std::move(owner));
				else if (auto* itemsHost = dynamic_cast<ItemsControl*>(runtimeParent))
					itemsHost->SetControlTemplateRoot(std::move(owner));
				else
					throw std::logic_error(
						"ControlTemplate root parent is not a supported template host");
			}
			else if (isVisualHeader)
			{
				auto* headered = dynamic_cast<HeaderedContentControl*>(runtimeParent);
				if (!headered)
					throw std::logic_error(
						"Header visual parent is not a HeaderedContentControl");
				headered->SetVisualHeader(std::move(owner));
			}
			else if (runtimeParent->Type() == UIClass::UI_ToolBar)
			{
				((ToolBar*)runtimeParent)->AddOwned(
					std::move(owner));
			}
			else
			{
				runtimeParent->AddOwned(std::move(owner));
			}
			if (!it->contentOwner.empty())
			{
				const auto logicalParent = instOf.find(it->contentOwner);
				dc->DesignerParent = logicalParent == instOf.end()
					? designerParent : logicalParent->second;
			}
			else dc->DesignerParent = designerParent;
			if (!it->templateGenerated)
				candidate.Controls.push_back(dc);
			attached.insert(it->name);
		};

		std::function<void(const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)> attachChildren;
		attachChildren = [&](const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)
		{
			auto it = childrenByParent.find(parentKey);
			if (it == childrenByParent.end()) return;
			if (auto* split = AsSplitContainer(runtimeParent))
			{
				std::vector<Pending*> firstChildren;
				std::vector<Pending*> secondChildren;
				for (auto* ch : it->second)
				{
					std::string region = ch->extra.value("splitRegion", std::string("panel1"));
					if (region == "panel2") secondChildren.push_back(ch);
					else firstChildren.push_back(ch);
				}
				sortByOrder(firstChildren);
				sortByOrder(secondChildren);
				for (auto* ch : firstChildren)
				{
					attachOne(ch, split->FirstPanel(), runtimeParent);
					attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				}
				for (auto* ch : secondChildren)
				{
					attachOne(ch, split->SecondPanel(), runtimeParent);
					attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				}
				return;
			}
			for (auto* ch : it->second)
			{
				attachOne(ch, runtimeParent, designerParent);
				attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				if (ch->type == UIClass::UI_TabControl)
				{
					auto* tabControl = (TabControl*)dcOf[ch->name]->ControlInstance;
					(void)tabControl;
					for (auto& kv : tabPageOf)
					{
						std::wstring prefix = ch->name + L"#page";
						if (kv.first.rfind(prefix, 0) != 0) continue;
						attachChildren(kv.first, kv.second, kv.second);
					}
				}
			}
		};

		for (auto* it : roots)
		{
			attachOne(it, &stagingRoot, nullptr);
			attachChildren(it->name, dcOf[it->name]->ControlInstance, dcOf[it->name]->ControlInstance);
			if (it->type == UIClass::UI_TabControl)
			{
				for (auto& kv : tabPageOf)
				{
					std::wstring prefix = it->name + L"#page";
					if (kv.first.rfind(prefix, 0) != 0) continue;
					attachChildren(kv.first, kv.second, kv.second);
				}
			}
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

		std::shared_ptr<ControlStyleSheet> runtimeStyleSheet;
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			document.StyleSheet, runtimeStyleSheet, outError,
			document.ResourceBasePath, document.Resources))
			return false;
		if (!document.StyleSheet.Empty()
			&& !stagingRoot.SetStyleSheet(runtimeStyleSheet, true))
		{
			if (outError) *outError =
				L"文档样式表无法应用到完整控件树。";
			return false;
		}

		for (auto& it : items)
		{
			if (it.componentType.Empty()
				|| (it.extra.is_object()
					&& it.extra.contains(AppliedControlTemplateKey))) continue;
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
			if (!it.source || !it.extra.is_object()
				|| !it.extra.contains(AppliedControlTemplateKey)) continue;
			const DesignerModel::DesignControlTemplate* definition = nullptr;
			const auto resourceKey = it.extra.contains(
				AppliedControlTemplateResourceKey)
				&& it.extra[AppliedControlTemplateResourceKey].is_string()
				? FromUtf8(it.extra[AppliedControlTemplateResourceKey]
					.get<std::string>())
				: std::wstring{};
			if (!resourceKey.empty())
				definition = document.FindControlTemplate(
					document.Nodes, *it.source, resourceKey);
			else definition = it.componentType.Empty()
				? document.FindImplicitControlTemplate(
					document.Nodes, *it.source, it.type)
				: document.FindImplicitControlTemplate(
					document.Nodes, *it.source, it.componentType);
			const auto host = instOf.find(it.name);
			if (!definition || host == instOf.end() || !host->second)
			{
				if (outError) *outError =
					L"ControlTemplate 视觉状态无法解析宿主：" + it.name;
				return false;
			}
			DesignComponentDefinition templateContext;
			if (!it.componentType.Empty())
				if (const auto* component = document.FindComponent(
					document.Nodes, *it.source, it.componentType))
					templateContext = *component;
			templateContext.BaseType = definition->TargetType;
			templateContext.Template = definition->Template;
			templateContext.VisualStateGroups = definition->VisualStateGroups;
			templateContext.EventTriggers = definition->EventTriggers;
			if (!InstallComponentVisualStatesCore(
				*host->second, templateContext, document, outError)) return false;
		}

		for (auto& dc : candidate.Controls)
		{
			if (!dc || !dc->ControlInstance) continue;
			RefreshDesignerPanelLayout(dc->ControlInstance);
		}
		if (options.DeclarativeComponentBehaviorFactory)
		{
			// Attach inner template components before their owning component so an
			// outer behavior observes a fully initialized visual subtree.
			for (auto position = items.rbegin(); position != items.rend(); ++position)
			{
				const auto& it = *position;
				if (it.componentType.Empty()) continue;
				const auto host = instOf.find(it.name);
				if (host == instOf.end() || !host->second)
				{
					if (outError) *outError = L"组件 Behavior 无法解析宿主：" + it.name;
					return false;
				}
				DeclarativeComponentBehaviorContext context{
					*host->second, it.id, it.name,
					it.componentType.XamlNamespace,
					it.componentType.XamlName };
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
				if (!host->second->SetDeclarativeComponentBehavior(
					std::move(behavior), context, &behaviorError))
				{
					if (outError) *outError = L"组件 Behavior 附加失败："
						+ it.componentType.XamlName + L"（" + it.name + L"）："
						+ behaviorError;
					return false;
				}
			}
		}
		while (stagingRoot.Count > 0)
		{
			auto root = stagingRoot.DetachControlAt(0);
			if (!root)
			{
				if (outError) *outError =
					L"材质化完成后无法分离根控件。";
				return false;
			}
			candidate.Roots.push_back(std::move(root));
		}

		output = std::move(candidate);
		if (outError) outError->clear();
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
