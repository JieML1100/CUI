#include "DesignDocumentClipboard.h"
#include "DesignDocumentGraph.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include <Convert.h>
#include <algorithm>
#include <bit>
#include <cwctype>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace DesignerModel
{
namespace
{
	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	struct Ownership
	{
		std::unordered_map<int, int> OwnerById;
		std::unordered_map<int, const DesignNode*> NodeById;
	};

	Ownership BuildOwnership(const DesignDocument& document)
	{
		Ownership result;
		result.OwnerById.reserve(document.Nodes.size());
		result.NodeById.reserve(document.Nodes.size());
		for (const auto& node : document.Nodes)
			result.NodeById.emplace(node.Id, &node);

		for (const auto& node : document.Nodes)
		{
			if (node.ParentId > 0)
			{
				result.OwnerById.emplace(node.Id, node.ParentId);
				continue;
			}
			if (node.ParentRef.empty()) continue;
			for (const auto& candidate : document.Nodes)
			{
				if (candidate.Type != UIClass::UI_TabControl) continue;
				const auto prefix = candidate.Name + L"#page";
				if (!node.ParentRef.starts_with(prefix)) continue;
				result.OwnerById.emplace(node.Id, candidate.Id);
				break;
			}
		}
		return result;
	}

	bool IsDescendantOfSelection(
		int id,
		const std::unordered_set<int>& selected,
		const Ownership& ownership)
	{
		std::unordered_set<int> visited;
		auto current = id;
		while (true)
		{
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) return false;
			current = parent->second;
			if (!visited.insert(current).second) return false;
			if (selected.contains(current)) return true;
		}
	}

	std::wstring MakeUniqueName(
		const std::wstring& desired,
		std::unordered_set<std::wstring>& used)
	{
		if (used.insert(Lower(desired)).second) return desired;

		auto digit = desired.size();
		while (digit > 0 && std::iswdigit(desired[digit - 1])) --digit;
		const auto stem = digit == desired.size()
			? desired : desired.substr(0, digit);
		unsigned long long suffix = 1;
		if (digit != desired.size())
		{
			try
			{
				suffix = std::stoull(desired.substr(digit)) + 1;
			}
			catch (...) { suffix = 1; }
		}
		for (;; ++suffix)
		{
			const auto candidate = stem + std::to_wstring(suffix);
			if (used.insert(Lower(candidate)).second) return candidate;
			if (suffix == (std::numeric_limits<unsigned long long>::max)())
				throw std::overflow_error("Designer clipboard name space exhausted");
		}
	}

	void RemapTabPageKeys(
		DesignNode& node,
		const std::wstring& oldName,
		const std::wstring& newName)
	{
		if (node.Type != UIClass::UI_TabControl
			|| !node.Extra.is_object()
			|| !node.Extra.contains("pages")
			|| !node.Extra["pages"].is_array()) return;
		const auto prefix = oldName + L"#page";
		for (auto& page : node.Extra["pages"].ArrayItems())
		{
			if (!page.is_object() || !page.contains("id")
				|| !page["id"].is_string()) continue;
			const auto oldKey = Convert::Utf8ToUnicode(
				page["id"].get<std::string>());
			if (!oldKey.starts_with(prefix)) continue;
			const auto newKey = newName + oldKey.substr(oldName.size());
			page["id"] = Convert::UnicodeToUtf8(newKey);
		}
	}

	bool OffsetRoot(
		DesignNode& node,
		int offsetX,
		int offsetY,
		std::wstring* outError)
	{
		if (!node.Props.is_object()) return true;
		auto shift = [&](long long original, int delta,
			long long& shifted) -> bool
		{
			shifted = original + static_cast<long long>(delta);
			if (shifted < (std::numeric_limits<int>::min)()
				|| shifted > (std::numeric_limits<int>::max)())
				return Fail(L"粘贴偏移超出控件坐标范围。", outError);
			return true;
		};

		if (node.Props.contains("location")
			&& node.Props["location"].is_object())
		{
			auto& location = node.Props["location"];
			for (const auto& [key, delta] : {
				std::pair{ "x", offsetX }, std::pair{ "y", offsetY } })
			{
				if (!location.contains(key)
					|| !location[key].is_number()) continue;
				long long shifted = 0;
				if (!shift(location[key].get<long long>(), delta, shifted))
					return false;
				location[key] = static_cast<int>(shifted);
			}
		}

		// Human-authored XAML stores Canvas.Left/Canvas.Top as generic
		// property metadata. Canonical clipboard XAML may instead carry the
		// exact legacy location object. Keep both representations aligned so
		// materialization cannot overwrite the requested paste placement.
		if (node.Props.contains("metadata")
			&& node.Props["metadata"].is_object())
		{
			auto& metadata = node.Props["metadata"];
			for (const auto& [name, delta] : {
				std::pair{ "Left", offsetX }, std::pair{ "Top", offsetY } })
			{
				if (!metadata.contains(name) || !metadata[name].is_object()
					|| !metadata[name].contains("value")
					|| !metadata[name]["value"].is_string()) continue;
				const auto text = metadata[name]["value"].get<std::string>();
				try
				{
					size_t consumed = 0;
					const auto original = std::stoll(text, &consumed);
					if (consumed != text.size()) continue;
					long long shifted = 0;
					if (!shift(original, delta, shifted)) return false;
					metadata[name]["value"] = std::to_string(shifted);
				}
				catch (const std::invalid_argument&) { continue; }
				catch (const std::out_of_range&)
				{
					return Fail(L"粘贴坐标元数据超出整数范围。", outError);
				}
			}
		}
		return true;
	}

	std::wstring ParentOrderKey(int parentId, const std::wstring& parentRef)
	{
		return parentId > 0
			? L"id:" + std::to_wstring(parentId)
			: L"ref:" + parentRef;
	}

	std::wstring ParentOrderKey(const DesignNode& node)
	{
		auto key = ParentOrderKey(node.ParentId, node.ParentRef);
		if (node.Extra.is_object())
		{
			const auto region = node.Extra.value(
				"splitRegion", std::string{});
			if (!region.empty())
				key += L"|split:" + Convert::Utf8ToUnicode(region);
		}
		return key;
	}

	bool HasSyntheticParent(
		const DesignDocument& document,
		const std::wstring& parentRef)
	{
		if (parentRef.empty()) return false;
		for (const auto& node : document.Nodes)
		{
			if (node.Type != UIClass::UI_TabControl
				|| !node.Extra.is_object()
				|| !node.Extra.contains("pages")
				|| !node.Extra["pages"].is_array()) continue;
			for (const auto& page : node.Extra["pages"].ArrayItems())
			{
				if (!page.is_object() || !page.contains("id")
					|| !page["id"].is_string()) continue;
				if (Convert::Utf8ToUnicode(page["id"].get<std::string>())
					== parentRef) return true;
			}
		}
		return false;
	}

	bool CollectBindingSchema(
		const DesignDocument& document,
		const std::unordered_set<int>* includedNodeIds,
		DesignerDataContextSchema& output,
		std::wstring* outError)
	{
		output.clear();
		for (const auto& node : document.Nodes)
		{
			if (includedNodeIds && !includedNodeIds->contains(node.Id)) continue;
			if (node.Bindings.is_null()) continue;
			if (!node.Bindings.is_object())
				return Fail(L"控件 " + node.Name + L" 的绑定集合格式无效。", outError);

			for (const auto& [targetProperty, binding] : node.Bindings.ObjectItems())
			{
				(void)targetProperty;
				if (!binding.is_object()
					|| !binding.contains("source")
					|| !binding["source"].is_string())
					return Fail(L"控件 " + node.Name + L" 的绑定源路径格式无效。", outError);
				const auto sourcePath =
					DesignerDataContextSchemaUtils::NormalizePath(
						Convert::Utf8ToUnicode(
							binding["source"].get<std::string>()));
				if (!DesignerDataContextSchemaUtils::IsValidPath(sourcePath))
					return Fail(L"控件 " + node.Name + L" 的绑定源路径无效。", outError);

				size_t separator = 0;
				while (separator != std::wstring::npos)
				{
					separator = sourcePath.find(L'.', separator);
					const auto prefix = separator == std::wstring::npos
						? sourcePath : sourcePath.substr(0, separator);
					if (!DesignerDataContextSchemaUtils::Find(output, prefix))
					{
						const auto* declared =
							DesignerDataContextSchemaUtils::Find(
								document.DataContextSchema, prefix);
						output.push_back(declared
							? *declared
							: DesignerDataContextProperty{
								prefix, BindingValueKind::Empty,
								true, true, true });
					}
					if (separator != std::wstring::npos) ++separator;
				}
			}
		}

		DesignerDataContextSchemaUtils::Canonicalize(output);
		std::wstring schemaError;
		if (!DesignerDataContextSchemaUtils::Validate(output, &schemaError))
			return Fail(L"控件绑定依赖的 DataContext Schema 无效："
				+ schemaError, outError);
		return true;
	}

	bool MergeBindingSchema(
		const DesignDocument& target,
		const DesignDocument& fragment,
		DesignDocument& candidate,
		std::wstring* outError)
	{
		DesignerDataContextSchema fragmentDependencies;
		if (!CollectBindingSchema(
			fragment, nullptr, fragmentDependencies, outError)) return false;
		if (fragmentDependencies.empty()) return true;

		// An empty target schema deliberately means permissive binding. Once a
		// pasted dependency makes it explicit, preserve that behavior for the
		// target's existing bindings by declaring their paths as Unknown first.
		bool changed = false;
		if (candidate.DataContextSchema.empty())
		{
			DesignerDataContextSchema targetDependencies;
			if (!CollectBindingSchema(
				target, nullptr, targetDependencies, outError)) return false;
			candidate.DataContextSchema = std::move(targetDependencies);
			changed = !candidate.DataContextSchema.empty();
		}

		for (const auto& dependency : fragmentDependencies)
		{
			// The destination describes its actual DataContext and is authoritative
			// when both documents declare the same path. Binding validation after
			// paste will reject an incompatible destination capability or type.
			if (!DesignerDataContextSchemaUtils::Find(
				candidate.DataContextSchema, dependency.Path))
			{
				candidate.DataContextSchema.push_back(dependency);
				changed = true;
			}
		}
		if (!changed) return true;
		DesignerDataContextSchemaUtils::Canonicalize(candidate.DataContextSchema);
		std::wstring schemaError;
		if (!DesignerDataContextSchemaUtils::Validate(
			candidate.DataContextSchema, &schemaError))
			return Fail(L"粘贴会产生无效的 DataContext Schema："
				+ schemaError, outError);
		return true;
	}

	bool EqualsStyleName(
		const std::wstring& left,
		const std::wstring& right)
	{
		return Lower(left) == Lower(right);
	}

	std::wstring NodeStyleId(const DesignNode& node)
	{
		if (!node.Props.is_object()
			|| !node.Props.contains("styleId")
			|| !node.Props["styleId"].is_string()) return {};
		return Convert::Utf8ToUnicode(
			node.Props["styleId"].get<std::string>());
	}

	std::vector<std::wstring> NodeStyleClasses(const DesignNode& node)
	{
		std::vector<std::wstring> result;
		if (!node.Props.is_object()
			|| !node.Props.contains("styleClasses")
			|| !node.Props["styleClasses"].is_array()) return result;
		for (const auto& value : node.Props["styleClasses"].ArrayItems())
			if (value.is_string())
				result.push_back(Convert::Utf8ToUnicode(
					value.get<std::string>()));
		return result;
	}

	bool ContainsStyleName(
		const std::vector<std::wstring>& values,
		const std::wstring& value)
	{
		return std::any_of(values.begin(), values.end(),
			[&](const auto& candidate)
			{
				return EqualsStyleName(candidate, value);
			});
	}

	bool StyleRuleMatchesNode(
		const DesignerStyleRule& rule,
		const DesignNode& node)
	{
		if (rule.HasType && rule.Type != UIClass::UI_Base
			&& rule.Type != node.Type) return false;
		if (!rule.Id.empty()
			&& !EqualsStyleName(rule.Id, NodeStyleId(node))) return false;
		const auto classes = NodeStyleClasses(node);
		for (const auto& required : rule.Classes)
			if (!ContainsStyleName(classes, required)) return false;
		return true;
	}

	uint32_t StyleStateCount(const DesignerStyleRule& rule)
	{
		return static_cast<uint32_t>(std::popcount(
			static_cast<uint32_t>(rule.RequiredStates)))
			+ static_cast<uint32_t>(std::popcount(
				static_cast<uint32_t>(rule.ExcludedStates)));
	}

	uint32_t StyleConditionCount(const DesignerStyleRule& rule)
	{
		return StyleStateCount(rule)
			+ static_cast<uint32_t>(rule.DataConditions.size());
	}

	uint32_t StyleRuleSpecificity(const DesignerStyleRule& rule)
	{
		const uint32_t id = rule.Id.empty() ? 0u : 1u;
		const uint32_t qualifiers = static_cast<uint32_t>(rule.Classes.size())
			+ StyleConditionCount(rule);
		const uint32_t exactType = rule.HasType
			&& rule.Type != UIClass::UI_Base ? 1u : 0u;
		return id * 1'000'000u + qualifiers * 1'000u + exactType;
	}

	bool CollectStyleDependencies(
		const DesignDocument& document,
		const std::unordered_set<int>* includedNodeIds,
		DesignerStyleSheet& output,
		std::wstring* outError)
	{
		output = {};
		if (document.StyleSheet.Empty()) return true;
		std::wstring styleError;
		if (!DesignerStyleSheetUtils::Validate(
			document.StyleSheet, &styleError, document.ResourceBasePath,
			document.Resources))
			return Fail(L"无法复制无效的样式表：" + styleError, outError);
		DesignerStyleSheet effectiveStyleSheet;
		if (!DesignerStyleSheetUtils::ExpandRuntimeRules(
			document.StyleSheet, effectiveStyleSheet, &styleError))
			return Fail(L"无法展开复制样式依赖：" + styleError, outError);

		std::unordered_set<std::wstring> requiredResources;
		for (const auto& rule : effectiveStyleSheet.Rules)
		{
			const bool matches = std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const DesignNode& node)
				{
					return (!includedNodeIds
						|| includedNodeIds->contains(node.Id))
						&& StyleRuleMatchesNode(rule, node);
				});
			if (!matches) continue;
			auto portableRule = rule;
			portableRule.SourceDictionary.clear();
			output.Rules.push_back(std::move(portableRule));
			for (const auto& setter : rule.Setters)
				if (setter.UsesResource)
					requiredResources.insert(Lower(setter.ResourceKey));
		}

		for (const auto& resource : effectiveStyleSheet.Resources)
			if (requiredResources.contains(Lower(resource.Key)))
			{
				auto portableResource = resource;
				portableResource.SourceDictionary.clear();
				output.Resources.push_back(std::move(portableResource));
			}
		DesignerStyleSheetUtils::Canonicalize(output);
		if (!DesignerStyleSheetUtils::Validate(
			output, &styleError, document.ResourceBasePath,
			document.Resources))
			return Fail(L"控件样式依赖无效：" + styleError, outError);
		return true;
	}

	std::vector<DesignerStyleRule> RelevantStyleRules(
		const DesignerStyleSheet& sheet,
		const std::vector<DesignNode>& nodes)
	{
		std::vector<DesignerStyleRule> result;
		for (const auto& rule : sheet.Rules)
			if (std::any_of(nodes.begin(), nodes.end(),
				[&](const DesignNode& node)
				{
					return StyleRuleMatchesNode(rule, node);
				}))
				result.push_back(rule);
		return result;
	}

	const DesignerStyleResource* FindStyleResource(
		const DesignerStyleSheet& sheet,
		const std::wstring& key)
	{
		const auto found = std::find_if(
			sheet.Resources.begin(), sheet.Resources.end(),
			[&](const DesignerStyleResource& resource)
			{
				return EqualsStyleName(resource.Key, key);
			});
		return found == sheet.Resources.end() ? nullptr : &*found;
	}

	bool CanReuseStyleDependencies(
		const DesignDocument& target,
		const DesignDocument& fragment,
		const DesignerStyleSheet& dependencies)
	{
		DesignerStyleSheet effectiveTarget;
		if (!DesignerStyleSheetUtils::ExpandRuntimeRules(
			target.StyleSheet, effectiveTarget)) return false;
		auto relevant = RelevantStyleRules(effectiveTarget, fragment.Nodes);
		for (auto& rule : relevant) rule.SourceDictionary.clear();
		if (relevant != dependencies.Rules) return false;
		for (const auto& resource : dependencies.Resources)
		{
			const auto* existing = FindStyleResource(
				target.StyleSheet, resource.Key);
			if (!existing || *existing != resource) return false;
		}
		return true;
	}

	std::wstring StyleToken(std::wstring value)
	{
		for (auto& ch : value)
			if (!std::iswalnum(ch) && ch != L'_') ch = L'_';
		while (!value.empty() && value.back() == L'_') value.pop_back();
		return value.empty() ? L"Style" : value;
	}

	struct IsolatedNodeStyle
	{
		std::wstring Id;
		std::vector<std::wstring> Classes;
	};

	void ApplyIsolatedNodeStyle(
		DesignNode& node,
		const IsolatedNodeStyle& style)
	{
		if (!node.Props.is_object()) node.Props = DesignValue::object();
		node.Props["styleId"] = Convert::UnicodeToUtf8(style.Id);
		auto& properties = node.Props.ObjectItems();
		if (style.Classes.empty()) properties.erase("styleClasses");
		else
		{
			auto classes = DesignValue::array();
			for (const auto& value : style.Classes)
				classes.push_back(Convert::UnicodeToUtf8(value));
			node.Props["styleClasses"] = std::move(classes);
		}
	}

	bool MergeStyleDependencies(
		const DesignDocument& target,
		const DesignDocument& fragment,
		const std::unordered_map<std::wstring, std::wstring>& nameMap,
		DesignDocument& candidate,
		std::unordered_map<int, IsolatedNodeStyle>& isolatedStyles,
		std::wstring* outError)
	{
		DesignerStyleSheet dependencies;
		if (!CollectStyleDependencies(
			fragment, nullptr, dependencies, outError)) return false;
		if (dependencies.Empty()
			|| CanReuseStyleDependencies(target, fragment, dependencies)) return true;

		std::unordered_set<std::wstring> usedSelectorNames;
		std::unordered_set<std::wstring> usedResourceNames;
		for (const auto& node : target.Nodes)
		{
			const auto id = NodeStyleId(node);
			if (!id.empty()) usedSelectorNames.insert(Lower(id));
			for (const auto& value : NodeStyleClasses(node))
				usedSelectorNames.insert(Lower(value));
		}
		for (const auto& rule : target.StyleSheet.Rules)
		{
			if (!rule.Id.empty()) usedSelectorNames.insert(Lower(rule.Id));
			for (const auto& value : rule.Classes)
				usedSelectorNames.insert(Lower(value));
		}
		for (const auto& resource : target.StyleSheet.Resources)
			usedResourceNames.insert(Lower(resource.Key));

		std::unordered_map<std::wstring, std::wstring> resourceMap;
		for (const auto& resource : dependencies.Resources)
		{
			auto imported = resource;
			imported.Key = MakeUniqueName(
				L"CuiPasteResource_" + StyleToken(resource.Key),
				usedResourceNames);
			resourceMap.emplace(Lower(resource.Key), imported.Key);
			candidate.StyleSheet.Resources.push_back(std::move(imported));
		}

		for (const auto& node : fragment.Nodes)
		{
			std::vector<const DesignerStyleRule*> matchingRules;
			std::vector<uint32_t> specificities;
			uint32_t maximumConditionCount = 0;
			for (const auto& rule : dependencies.Rules)
			{
				if (!StyleRuleMatchesNode(rule, node)) continue;
				matchingRules.push_back(&rule);
				specificities.push_back(StyleRuleSpecificity(rule));
				maximumConditionCount = (std::max)(
					maximumConditionCount, StyleConditionCount(rule));
			}
			if (matchingRules.empty()) continue;

			auto distinctSpecificities = specificities;
			std::sort(distinctSpecificities.begin(), distinctSpecificities.end());
			distinctSpecificities.erase(std::unique(
				distinctSpecificities.begin(), distinctSpecificities.end()),
				distinctSpecificities.end());
			IsolatedNodeStyle isolated;
			isolated.Id = MakeUniqueName(
				L"CuiPasteStyle_" + StyleToken(nameMap.at(node.Name)),
				usedSelectorNames);

			std::vector<size_t> extraClassCounts;
			extraClassCounts.reserve(matchingRules.size());
			size_t maximumExtraClassCount = 0;
			for (size_t index = 0; index < matchingRules.size(); ++index)
			{
				const auto rank = static_cast<uint32_t>(std::lower_bound(
					distinctSpecificities.begin(), distinctSpecificities.end(),
					specificities[index]) - distinctSpecificities.begin());
				const auto desiredQualifierCount = maximumConditionCount + rank;
				const auto extra = static_cast<size_t>(
					desiredQualifierCount - StyleConditionCount(*matchingRules[index]));
				extraClassCounts.push_back(extra);
				maximumExtraClassCount = (std::max)(maximumExtraClassCount, extra);
			}
			for (size_t index = 0; index < maximumExtraClassCount; ++index)
				isolated.Classes.push_back(MakeUniqueName(
					isolated.Id + L"_Q" + std::to_wstring(index + 1),
					usedSelectorNames));

			for (size_t index = 0; index < matchingRules.size(); ++index)
			{
				auto imported = *matchingRules[index];
				imported.Id = isolated.Id;
				imported.Classes.assign(
					isolated.Classes.begin(),
					isolated.Classes.begin() + extraClassCounts[index]);
				for (auto& setter : imported.Setters)
				{
					if (!setter.UsesResource) continue;
					const auto found = resourceMap.find(Lower(setter.ResourceKey));
					if (found == resourceMap.end())
						return Fail(L"无法重映射粘贴样式资源："
							+ setter.ResourceKey, outError);
					setter.ResourceKey = found->second;
				}
				candidate.StyleSheet.Rules.push_back(std::move(imported));
			}
			isolatedStyles.emplace(node.Id, std::move(isolated));
		}

		std::wstring styleError;
		if (!DesignerStyleSheetUtils::Validate(
			candidate.StyleSheet, &styleError, candidate.ResourceBasePath,
			candidate.Resources))
			return Fail(L"粘贴会产生无效的隔离样式：" + styleError, outError);
		return true;
	}
}

bool DesignDocumentClipboard::Capture(
	const DesignDocument& source,
	const std::vector<int>& selectedNodeIds,
	DesignDocument& fragment,
	std::wstring* outError)
{
	try
	{
		DesignDocumentGraph graph;
		std::wstring graphError;
		if (!DesignDocumentGraph::Build(source, graph, &graphError))
			return Fail(L"无法复制无效设计文档：" + graphError, outError);
		if (selectedNodeIds.empty())
			return Fail(L"没有选中可复制的控件。", outError);

		const auto ownership = BuildOwnership(source);
		std::unordered_set<int> selected;
		selected.reserve(selectedNodeIds.size());
		for (const auto id : selectedNodeIds)
		{
			if (!ownership.NodeById.contains(id))
				return Fail(L"选中控件的稳定 ID 不存在："
					+ std::to_wstring(id), outError);
			selected.insert(id);
		}

		std::unordered_set<int> roots;
		roots.reserve(selected.size());
		for (const auto id : selected)
			if (!IsDescendantOfSelection(id, selected, ownership))
				roots.insert(id);

		std::unordered_map<int, std::vector<int>> children;
		children.reserve(ownership.OwnerById.size());
		for (const auto& [child, owner] : ownership.OwnerById)
			children[owner].push_back(child);
		std::unordered_set<int> included;
		included.reserve(source.Nodes.size());
		std::queue<int> pending;
		for (const auto id : roots) pending.push(id);
		while (!pending.empty())
		{
			const auto id = pending.front();
			pending.pop();
			if (!included.insert(id).second) continue;
			const auto found = children.find(id);
			if (found == children.end()) continue;
			for (const auto child : found->second) pending.push(child);
		}

		DesignDocument candidate;
		candidate.ResourceBasePath = source.ResourceBasePath;
		candidate.Resources = source.Resources;
		candidate.Form.Name = L"Clipboard";
		candidate.Form.Text = L"CUI Clipboard";
		candidate.Form.Size = source.Form.Size;
		if (!CollectBindingSchema(
			source, &included, candidate.DataContextSchema, outError)) return false;
		if (!CollectStyleDependencies(
			source, &included, candidate.StyleSheet, outError)) return false;
		candidate.Nodes.reserve(included.size());
		int rootOrder = 0;
		for (const auto& sourceNode : source.Nodes)
		{
			if (!included.contains(sourceNode.Id)) continue;
			auto node = sourceNode;
			if (roots.contains(node.Id))
			{
				node.ParentId = 0;
				node.ParentRef.clear();
				node.Order = rootOrder++;
			}
			candidate.Nodes.push_back(std::move(node));
		}
		candidate.RecalculateNextStableId();
		DesignDocumentGraph candidateGraph;
		if (!DesignDocumentGraph::Build(candidate, candidateGraph, &graphError))
			return Fail(L"无法构造可移植的控件片段：" + graphError, outError);
		fragment = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& error)
	{
		return Fail(L"复制控件片段失败："
			+ Convert::Utf8ToUnicode(error.what()),
			outError);
	}
	catch (...)
	{
		return Fail(L"复制控件片段时发生未知异常。", outError);
	}
}

bool DesignDocumentClipboard::PasteAtRoot(
	const DesignDocument& target,
	const DesignDocument& fragment,
	int offsetX,
	int offsetY,
	DesignDocument& merged,
	DesignClipboardPasteResult* outResult,
	std::wstring* outError)
{
	return Paste(target, fragment, {}, offsetX, offsetY,
		merged, outResult, outError);
}

bool DesignDocumentClipboard::Paste(
	const DesignDocument& target,
	const DesignDocument& fragment,
	const std::vector<DesignClipboardRootTarget>& rootTargets,
	int offsetX,
	int offsetY,
	DesignDocument& merged,
	DesignClipboardPasteResult* outResult,
	std::wstring* outError)
{
	try
	{
		DesignDocumentGraph targetGraph;
		DesignDocumentGraph fragmentGraph;
		std::wstring graphError;
		if (!DesignDocumentGraph::Build(target, targetGraph, &graphError))
			return Fail(L"无法粘贴到无效设计文档：" + graphError, outError);
		if (!DesignDocumentGraph::Build(fragment, fragmentGraph, &graphError))
			return Fail(L"剪贴板中的 CUI XAML 无效：" + graphError, outError);
		if (fragment.Nodes.empty() || fragmentGraph.Roots().empty())
			return Fail(L"剪贴板中的 CUI XAML 不包含可粘贴控件。", outError);

		std::vector<DesignClipboardRootTarget> effectiveTargets = rootTargets;
		if (effectiveTargets.empty())
		{
			effectiveTargets.reserve(fragmentGraph.Roots().size());
			for (const auto graphIndex : fragmentGraph.Roots())
			{
				DesignClipboardRootTarget destination;
				destination.FragmentRootId = fragment.Nodes[
					fragmentGraph.Nodes()[graphIndex].SourceIndex].Id;
				effectiveTargets.push_back(std::move(destination));
			}
		}
		if (effectiveTargets.size() != fragmentGraph.Roots().size())
			return Fail(L"粘贴目标必须覆盖每一个控件片段根。", outError);

		std::unordered_map<int, const DesignClipboardRootTarget*> targetByRoot;
		targetByRoot.reserve(effectiveTargets.size());
		std::unordered_set<int> fragmentRootIds;
		fragmentRootIds.reserve(fragmentGraph.Roots().size());
		for (const auto graphIndex : fragmentGraph.Roots())
			fragmentRootIds.insert(fragment.Nodes[
				fragmentGraph.Nodes()[graphIndex].SourceIndex].Id);
		for (const auto& destination : effectiveTargets)
		{
			if (!fragmentRootIds.contains(destination.FragmentRootId))
				return Fail(L"粘贴目标引用了不存在的片段根："
					+ std::to_wstring(destination.FragmentRootId), outError);
			if (!targetByRoot.emplace(
				destination.FragmentRootId, &destination).second)
				return Fail(L"同一个片段根不能指定多个粘贴目标。", outError);
			if (destination.ParentId < 0)
				return Fail(L"粘贴目标 parentId 不能为负数。", outError);
			if (destination.InsertIndex && *destination.InsertIndex < 0)
				return Fail(L"粘贴目标插入序号不能为负数。", outError);
			if (destination.ParentId > 0)
			{
				const auto* resolved = targetGraph.FindById(destination.ParentId);
				if (!resolved)
					return Fail(L"粘贴目标父控件已经不存在。", outError);
				const auto& parent = target.Nodes[resolved->SourceIndex];
				if (parent.Type == UIClass::UI_TabControl)
					return Fail(L"TabControl 必须通过具体 TabPage 接收控件。", outError);
				if (destination.SplitRegion
					&& !destination.SplitRegion->empty()
					&& parent.Type != UIClass::UI_SplitContainer)
					return Fail(L"只有 SplitContainer 才能指定粘贴区域。", outError);
			}
			else if (!destination.ParentRef.empty())
			{
				if (!HasSyntheticParent(target, destination.ParentRef))
					return Fail(L"粘贴目标 TabPage 已经不存在："
						+ destination.ParentRef, outError);
				if (destination.SplitRegion
					&& !destination.SplitRegion->empty())
					return Fail(L"TabPage 不能指定 SplitContainer 区域。", outError);
			}
			else if (destination.SplitRegion
				&& !destination.SplitRegion->empty())
			{
				return Fail(L"窗体根不能指定 SplitContainer 区域。", outError);
			}
		}

		DesignDocument candidate = target;
		if (!MergeBindingSchema(target, fragment, candidate, outError)) return false;
		std::unordered_set<std::wstring> usedNames;
		usedNames.reserve(target.Nodes.size() + fragment.Nodes.size());
		for (const auto& node : target.Nodes) usedNames.insert(Lower(node.Name));

		std::unordered_map<int, int> idMap;
		std::unordered_map<std::wstring, std::wstring> nameMap;
		idMap.reserve(fragment.Nodes.size());
		nameMap.reserve(fragment.Nodes.size());
		for (const auto& node : fragment.Nodes)
		{
			idMap.emplace(node.Id, candidate.AllocateNodeId());
			nameMap.emplace(node.Name, MakeUniqueName(node.Name, usedNames));
		}

		std::unordered_map<int, IsolatedNodeStyle> isolatedStyles;
		isolatedStyles.reserve(fragment.Nodes.size());
		if (!MergeStyleDependencies(
			target, fragment, nameMap, candidate,
			isolatedStyles, outError)) return false;

		// Conventional handler names carry the source control identity. Build
		// this map from handlers that are actually owned by a copied node, then
		// apply it to every copied reference. This keeps deliberate sharing
		// within a pasted subtree intact while custom/external handler names stay
		// untouched.
		std::unordered_map<std::wstring, std::wstring> handlerNameMap;
		std::unordered_set<std::wstring> ambiguousHandlerNames;
		for (const auto& node : fragment.Nodes)
		{
			if (!node.Events.is_object()) continue;
			const auto& newName = nameMap.at(node.Name);
			for (const auto& [eventName, handlerValue]
				: node.Events.ObjectItems())
			{
				if (!handlerValue.is_string()) continue;
				const auto event = Convert::Utf8ToUnicode(eventName);
				const auto storedHandler = Convert::Utf8ToUnicode(
					handlerValue.get<std::string>());
				const auto conventionalName =
					DesignerEventCatalog::MakeDefaultHandlerName(
						node.Name, event);
				if (DesignerEventCatalog::ResolveHandlerName(
					storedHandler, node.Name, event) != conventionalName
					|| ambiguousHandlerNames.contains(conventionalName)) continue;
				const auto remappedName =
					DesignerEventCatalog::MakeDefaultHandlerName(
						newName, event);
				const auto [found, inserted] = handlerNameMap.emplace(
					conventionalName, remappedName);
				if (!inserted && found->second != remappedName)
				{
					handlerNameMap.erase(found);
					ambiguousHandlerNames.insert(conventionalName);
				}
			}
		}

		std::unordered_set<size_t> rootIndices;
		rootIndices.reserve(fragmentGraph.Roots().size());
		std::unordered_map<int, size_t> fragmentRootSequence;
		fragmentRootSequence.reserve(fragmentGraph.Roots().size());
		for (size_t sequence = 0;
			sequence < fragmentGraph.Roots().size(); ++sequence)
		{
			const auto index = fragmentGraph.Roots()[sequence];
			rootIndices.insert(index);
			fragmentRootSequence.emplace(
				fragment.Nodes[fragmentGraph.Nodes()[index].SourceIndex].Id,
				sequence);
		}
		std::unordered_map<std::wstring, int> nextOrderByParent;
		nextOrderByParent.reserve(target.Nodes.size() + effectiveTargets.size());
		for (const auto& node : target.Nodes)
		{
			auto& next = nextOrderByParent[ParentOrderKey(node)];
			next = (std::max)(next, node.Order + 1);
		}
		struct PendingInsertion
		{
			int NodeId = 0;
			int Index = 0;
			size_t Sequence = 0;
		};
		std::unordered_map<std::wstring, std::vector<PendingInsertion>>
			insertionsByParent;
		std::unordered_set<std::wstring> implicitPasteParents;
		std::unordered_set<int> pastedRootIds;
		pastedRootIds.reserve(fragmentGraph.Roots().size());

		const auto ownership = BuildOwnership(fragment);
		DesignClipboardPasteResult result;
		result.NodeIds.reserve(fragment.Nodes.size());
		result.RootIds.reserve(fragmentGraph.Roots().size());
		result.RootNames.reserve(fragmentGraph.Roots().size());
		for (size_t index = 0; index < fragment.Nodes.size(); ++index)
		{
			const auto& original = fragment.Nodes[index];
			auto node = original;
			node.Id = idMap.at(original.Id);
			node.Name = nameMap.at(original.Name);
			RemapTabPageKeys(node, original.Name, node.Name);
			const auto isolatedStyle = isolatedStyles.find(original.Id);
			if (isolatedStyle != isolatedStyles.end())
				ApplyIsolatedNodeStyle(node, isolatedStyle->second);
			if (node.Events.is_object())
				for (auto& [eventName, handlerValue]
					: node.Events.ObjectItems())
				{
					(void)eventName;
					if (!handlerValue.is_string()) continue;
					const auto found = handlerNameMap.find(
						Convert::Utf8ToUnicode(
							handlerValue.get<std::string>()));
					if (found != handlerNameMap.end())
						handlerValue = Convert::UnicodeToUtf8(found->second);
				}
			if (rootIndices.contains(index))
			{
				const auto& destination = *targetByRoot.at(original.Id);
				node.ParentId = destination.ParentId;
				if (destination.ParentId > 0)
				{
					const auto* resolved = targetGraph.FindById(
						destination.ParentId);
					node.ParentRef = target.Nodes[
						resolved->SourceIndex].Name;
				}
				else node.ParentRef = destination.ParentRef;
				if (destination.SplitRegion)
				{
					if (node.Extra.is_object())
						node.Extra.ObjectItems().erase("splitRegion");
					if (!destination.SplitRegion->empty())
					{
						if (*destination.SplitRegion != "panel1"
							&& *destination.SplitRegion != "panel2")
							return Fail(L"SplitContainer 粘贴区域无效。", outError);
						node.Extra["splitRegion"] =
							*destination.SplitRegion;
					}
				}
				if (node.ParentId > 0)
				{
					const auto* resolved = targetGraph.FindById(node.ParentId);
					const auto& parent = target.Nodes[resolved->SourceIndex];
					if (parent.Type == UIClass::UI_SplitContainer)
					{
						const auto region = node.Extra.is_object()
							? node.Extra.value("splitRegion", std::string{})
							: std::string{};
						if (region != "panel1" && region != "panel2")
							return Fail(L"粘贴到 SplitContainer 时必须指定 First 或 Second 区域。", outError);
					}
				}
				const auto parentOrderKey = ParentOrderKey(node);
				node.Order = nextOrderByParent[parentOrderKey]++;
				if ((node.Type == UIClass::UI_Menu
					|| node.Type == UIClass::UI_StatusBar)
					&& (node.ParentId > 0 || !node.ParentRef.empty()))
					return Fail(L"Menu 和 StatusBar 只能粘贴到窗体根。", outError);
				if (!OffsetRoot(node, offsetX, offsetY, outError)) return false;
				result.RootIds.push_back(node.Id);
				result.RootNames.push_back(node.Name);
				pastedRootIds.insert(node.Id);
				if (destination.InsertIndex)
				{
					insertionsByParent[parentOrderKey].push_back({
						node.Id,
						*destination.InsertIndex,
						fragmentRootSequence.at(original.Id) });
				}
				else implicitPasteParents.insert(parentOrderKey);
			}
			else if (original.ParentId > 0)
			{
				node.ParentId = idMap.at(original.ParentId);
				const auto parent = ownership.NodeById.at(original.ParentId);
				node.ParentRef = nameMap.at(parent->Name);
			}
			else
			{
				const auto ownerFound = ownership.OwnerById.find(original.Id);
				if (ownerFound == ownership.OwnerById.end())
					return Fail(L"剪贴板控件缺少可复制的父级："
						+ original.Name, outError);
				const auto owner = ownership.NodeById.at(ownerFound->second);
				node.ParentId = 0;
				node.ParentRef = nameMap.at(owner->Name)
					+ original.ParentRef.substr(owner->Name.size());
			}
			result.NodeIds.push_back(node.Id);
			candidate.Nodes.push_back(std::move(node));
		}

		for (auto& [parentKey, insertions] : insertionsByParent)
		{
			if (implicitPasteParents.contains(parentKey))
				return Fail(
					L"同一个粘贴目标不能混用追加与指定插入序号。",
					outError);

			std::vector<DesignNode*> siblings;
			for (auto& node : candidate.Nodes)
			{
				if (pastedRootIds.contains(node.Id)
					|| ParentOrderKey(node) != parentKey) continue;
				siblings.push_back(&node);
			}
			std::stable_sort(siblings.begin(), siblings.end(),
				[](const DesignNode* left, const DesignNode* right)
				{
					return left->Order < right->Order;
				});
			for (const auto& insertion : insertions)
				if (static_cast<size_t>(insertion.Index) > siblings.size())
					return Fail(L"粘贴目标插入序号超出同级控件范围。", outError);

			std::stable_sort(insertions.begin(), insertions.end(),
				[](const PendingInsertion& left,
					const PendingInsertion& right)
				{
					if (left.Index != right.Index)
						return left.Index < right.Index;
					return left.Sequence < right.Sequence;
				});
			size_t inserted = 0;
			for (const auto& insertion : insertions)
			{
				const auto found = std::find_if(
					candidate.Nodes.begin(), candidate.Nodes.end(),
					[&insertion](const DesignNode& node)
					{
						return node.Id == insertion.NodeId;
					});
				if (found == candidate.Nodes.end())
					return Fail(L"无法定位待插入的粘贴控件。", outError);
				siblings.insert(
					siblings.begin() + insertion.Index + inserted, &*found);
				++inserted;
			}
			for (size_t order = 0; order < siblings.size(); ++order)
				siblings[order]->Order = static_cast<int>(order);
		}

		DesignDocumentGraph mergedGraph;
		if (!DesignDocumentGraph::Build(candidate, mergedGraph, &graphError))
			return Fail(L"粘贴会产生无效设计文档：" + graphError, outError);
		merged = std::move(candidate);
		if (outResult) *outResult = std::move(result);
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& error)
	{
		return Fail(L"粘贴控件片段失败："
			+ Convert::Utf8ToUnicode(error.what()),
			outError);
	}
	catch (...)
	{
		return Fail(L"粘贴控件片段时发生未知异常。", outError);
	}
}
}
