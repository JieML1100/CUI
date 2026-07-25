#include "DesignDocumentClipboard.h"
#include "DesignDataResourceUtils.h"
#include "DesignDocumentGraph.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/GroupStyle.h"
#include <Convert.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string_view>
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

	std::wstring IdentityKey(std::wstring value)
	{
		return value;
	}

	struct Ownership
	{
		std::unordered_map<int, int> OwnerById;
		std::unordered_map<int, const DesignNode*> NodeById;
	};

	Ownership BuildOwnership(const std::vector<DesignNode>& nodes)
	{
		Ownership result;
		result.OwnerById.reserve(nodes.size());
		result.NodeById.reserve(nodes.size());
		for (const auto& node : nodes)
			result.NodeById.emplace(node.Id, &node);

		for (const auto& node : nodes)
		{
			if (node.ParentId > 0)
			{
				result.OwnerById.emplace(node.Id, node.ParentId);
				continue;
			}
			if (node.ParentRef.empty()) continue;
			for (const auto& candidate : nodes)
				if (candidate.Name == node.ParentRef)
				{
					result.OwnerById.emplace(node.Id, candidate.Id);
					break;
				}
		}
		return result;
	}

	Ownership BuildOwnership(const DesignDocument& document)
	{
		return BuildOwnership(document.Nodes);
	}

	struct LexicalResource
	{
		const DesignerStyleResource* Resource = nullptr;
		int OwnerId = 0;
	};

	struct LexicalComponent
	{
		const DesignComponentDefinition* Definition = nullptr;
		int OwnerId = 0;
	};

	LexicalComponent FindLexicalComponent(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		const DesignerComponentType& type)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources = foundNode->second->LocalObjectResources.Components;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition) { return definition.Type == type; });
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { document.FindComponent(type), 0 };
	}

	struct LexicalDataTemplate
	{
		const DesignDataTemplate* Definition = nullptr;
		int OwnerId = 0;
	};

	struct LexicalControlTemplate
	{
		const DesignControlTemplate* Definition = nullptr;
		int OwnerId = 0;
	};

	LexicalControlTemplate FindLexicalControlTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		const std::wstring& key)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources =
				foundNode->second->LocalObjectResources.ControlTemplates;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition)
				{
					return !definition.IsImplicit()
						&& IdentityKey(definition.Key) == IdentityKey(key);
				});
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { document.FindControlTemplate(key), 0 };
	}

	LexicalControlTemplate FindLexicalImplicitControlTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources =
				foundNode->second->LocalObjectResources.ControlTemplates;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition)
				{
					if (!definition.IsImplicit()) return false;
					return node.ComponentType.Empty()
						? definition.TargetComponentType.Empty()
							&& definition.TargetType == node.Type
						: !definition.TargetComponentType.Empty()
							&& definition.TargetComponentType.RegistryKey()
								== node.ComponentType.RegistryKey();
				});
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { node.ComponentType.Empty()
			? document.FindImplicitControlTemplate(node.Type)
			: document.FindImplicitControlTemplate(node.ComponentType), 0 };
	}

	LexicalDataTemplate FindLexicalDataTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		const std::wstring& key)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources = foundNode->second->LocalObjectResources.DataTemplates;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition)
				{ return IdentityKey(definition.Key) == IdentityKey(key); });
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { document.FindDataTemplate(key), 0 };
	}

	LexicalDataTemplate FindLexicalImplicitDataTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		const std::wstring& dataType)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources = foundNode->second->LocalObjectResources.DataTemplates;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition)
				{
					return definition.IsImplicit()
						&& IdentityKey(definition.DataType) == IdentityKey(dataType);
				});
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { document.FindImplicitDataTemplate(dataType), 0 };
	}

	struct LexicalItemsPanelTemplate
	{
		const DesignItemsPanelTemplate* Definition = nullptr;
		int OwnerId = 0;
	};

	LexicalItemsPanelTemplate FindLexicalItemsPanelTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		const std::wstring& key)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources = foundNode->second->LocalObjectResources.
				ItemsPanelTemplates;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition)
				{ return IdentityKey(definition.Key) == IdentityKey(key); });
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { document.FindItemsPanelTemplate(key), 0 };
	}

	struct LexicalGroupStyle
	{
		const DesignGroupStyle* Definition = nullptr;
		int OwnerId = 0;
	};

	LexicalGroupStyle FindLexicalGroupStyle(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		const std::wstring& key)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources =
				foundNode->second->LocalObjectResources.GroupStyles;
			const auto found = std::find_if(resources.rbegin(), resources.rend(),
				[&](const auto& definition)
				{ return IdentityKey(definition.Key) == IdentityKey(key); });
			if (found != resources.rend()) return { &*found, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return { document.FindGroupStyle(key), 0 };
	}

	LexicalResource FindLexicalResource(
		const DesignNode& node,
		const Ownership& ownership,
		const std::wstring& key)
	{
		std::unordered_set<int> visited;
		auto current = node.Id;
		while (current > 0 && visited.insert(current).second)
		{
			const auto foundNode = ownership.NodeById.find(current);
			if (foundNode == ownership.NodeById.end()) break;
			const auto& resources = foundNode->second->LocalResources.Resources;
			const auto foundResource = std::find_if(
				resources.rbegin(), resources.rend(),
				[&](const DesignerStyleResource& resource)
				{
					return IdentityKey(resource.Key) == IdentityKey(key);
				});
			if (foundResource != resources.rend())
				return { &*foundResource, current };
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		return {};
	}

	void AppendStyleScope(
		DesignerStyleSheet& target,
		const DesignerStyleSheet& source)
	{
		DesignerStyleSheetUtils::AppendLexicalScope(target, source);
	}

	bool MakeLocalStylesPortable(
		const DesignDocument& source,
		const DesignNode& original,
		const Ownership& ownership,
		const std::unordered_set<int>& included,
		DesignNode& output,
		std::wstring* outError)
	{
		if (output.LocalResources.Rules.empty()) return true;
		DesignerStyleSheet visible = source.StyleSheet;
		std::vector<const DesignNode*> route;
		std::unordered_set<int> visited;
		for (auto current = original.Id;
			current > 0 && visited.insert(current).second;)
		{
			const auto node = ownership.NodeById.find(current);
			if (node == ownership.NodeById.end()) break;
			route.push_back(node->second);
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		for (auto node = route.rbegin(); node != route.rend(); ++node)
			AppendStyleScope(visible, (*node)->LocalResources);
		DesignerStyleSheet inherited;
		std::wstring error;
		if (!DesignerStyleSheetUtils::ResolveInheritance(
			visible, inherited, &error))
			return Fail(L"无法展开局部 Style 依赖：" + error, outError);
		if (inherited.Rules.size() < output.LocalResources.Rules.size())
			return Fail(L"局部 Style 依赖范围无效。", outError);
		output.LocalResources.Rules.assign(
			inherited.Rules.end() - output.LocalResources.Rules.size(),
			inherited.Rules.end());
		std::unordered_set<std::wstring> usedKeys;
		for (const auto& resource : output.LocalResources.Resources)
			usedKeys.insert(IdentityKey(resource.Key));
		size_t aliasIndex = 1;
		auto isolateStatic = [&](std::wstring& key) -> bool
		{
			const auto lexical = FindLexicalResource(original, ownership, key);
			if (lexical.Resource && included.contains(lexical.OwnerId)) return true;
			const auto found = std::find_if(
				visible.Resources.rbegin(), visible.Resources.rend(),
				[&](const auto& resource)
				{ return IdentityKey(resource.Key) == IdentityKey(key); });
			if (found == visible.Resources.rend())
				return Fail(L"局部 Style 引用了不可见资源：" + key, outError);
			std::wstring alias;
			do alias = L"CuiClipboardStatic_" + std::to_wstring(aliasIndex++);
			while (!usedKeys.insert(IdentityKey(alias)).second);
			auto resource = *found;
			resource.Key = alias;
			resource.SourceDictionary.clear();
			output.LocalResources.Resources.push_back(std::move(resource));
			key = std::move(alias);
			return true;
		};
		auto rewriteAnimation = [&](auto& animation)
		{
			if (animation.HasFrom && animation.FromUsesResource
				&& !isolateStatic(animation.FromResourceKey)) return false;
			if (animation.HasTo && animation.ToUsesResource
				&& !isolateStatic(animation.ToResourceKey)) return false;
			if (animation.HasBy && animation.ByUsesResource
				&& !isolateStatic(animation.ByResourceKey)) return false;
			for (auto& frame : animation.KeyFrames)
				if (frame.UsesResource
					&& !isolateStatic(frame.ResourceKey)) return false;
			return true;
		};
		auto rewriteActions = [&](auto& actions)
		{
			for (auto& action : actions)
				for (auto& animation : action.Animations)
					if (!rewriteAnimation(animation)) return false;
			return true;
		};
		auto rewriteSetters = [&](auto& setters)
		{
			for (auto& setter : setters)
				if (setter.PropertyName != L"Template"
					&& setter.UsesResource && !setter.UsesDynamicResource
					&& !isolateStatic(setter.ResourceKey)) return false;
			return true;
		};
		for (auto& rule : output.LocalResources.Rules)
		{
			if (!rewriteSetters(rule.Setters)
				|| !rewriteActions(rule.EnterActions)
				|| !rewriteActions(rule.ExitActions)) return false;
			for (auto& trigger : rule.Triggers)
				if (!rewriteSetters(trigger.Setters)
					|| !rewriteActions(trigger.EnterActions)
					|| !rewriteActions(trigger.ExitActions)) return false;
		}
		return true;
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
		if (used.insert(IdentityKey(desired)).second) return desired;

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
			if (used.insert(IdentityKey(candidate)).second) return candidate;
			if (suffix == (std::numeric_limits<unsigned long long>::max)())
				throw std::overflow_error("Designer clipboard name space exhausted");
		}
	}

	bool OffsetRoot(
		DesignNode& node,
		int offsetX,
		int offsetY,
		std::wstring* outError)
	{
		for (const auto& [name, delta] : {
			std::pair{ L"Canvas.Left", offsetX },
			std::pair{ L"Canvas.Top", offsetY } })
		{
			auto* assignment = node.Properties.Find(name);
			if (!assignment) continue;
			try
			{
				size_t consumed = 0;
				const auto original = std::stof(
					assignment->Value.Text, &consumed);
				if (consumed != assignment->Value.Text.size()
					|| !std::isfinite(original))
					return Fail(L"Canvas 坐标不是有限数字。", outError);
				const auto shiftedValue = static_cast<double>(original)
					+ static_cast<double>(delta);
				if (!std::isfinite(shiftedValue)
					|| shiftedValue < -(std::numeric_limits<float>::max)()
					|| shiftedValue > (std::numeric_limits<float>::max)())
					return Fail(L"粘贴偏移超出 Canvas 坐标范围。", outError);
				const auto shifted = static_cast<float>(shiftedValue);
				char buffer[64]{};
				const auto converted = std::to_chars(
					std::begin(buffer), std::end(buffer), shifted,
					std::chars_format::general,
					std::numeric_limits<float>::max_digits10);
				if (converted.ec != std::errc{})
					return Fail(L"Canvas 坐标无法序列化。", outError);
				assignment->Value.Text = Convert::Utf8ToUnicode(
					std::string(buffer, converted.ptr));
			}
			catch (const std::invalid_argument&)
			{
				return Fail(L"Canvas 坐标不是数字。", outError);
			}
			catch (const std::out_of_range&)
			{
				return Fail(L"Canvas 坐标超出浮点范围。", outError);
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
		return ParentOrderKey(node.ParentId, node.ParentRef);
	}

	bool CollectBindingSchema(
		const DesignDocument& document,
		const std::unordered_set<int>* includedNodeIds,
		DesignerDataContextSchema& output,
		std::wstring* outError)
	{
		output.clear();
		auto includePath = [&](const std::wstring& rawPath)
		{
			std::vector<BindingPathStep> steps;
			if (!TryParseBindingPropertyPath(rawPath, steps)) return false;
			std::wstring schemaPath;
			for (const auto& step : steps)
			{
				if (step.Kind == BindingPathStepKind::Indexer) break;
				if (!schemaPath.empty()) schemaPath += L'.';
				schemaPath += step.Value;
			}
			const auto sourcePath =
				DesignerDataContextSchemaUtils::NormalizePath(schemaPath);
			if (sourcePath.empty()) return true;
			if (!DesignerDataContextSchemaUtils::IsValidPath(sourcePath))
				return false;
			size_t separator = 0;
			while (separator != std::wstring::npos)
			{
				separator = sourcePath.find(L'.', separator);
				const auto prefix = separator == std::wstring::npos
					? sourcePath : sourcePath.substr(0, separator);
				if (!DesignerDataContextSchemaUtils::Find(output, prefix))
				{
					const auto* declared = DesignerDataContextSchemaUtils::Find(
						document.DataContextSchema, prefix);
					output.push_back(declared ? *declared
						: DesignerDataContextProperty{ prefix,
							BindingValueKind::Empty, true, true, true });
				}
				if (separator != std::wstring::npos) ++separator;
			}
			return true;
		};
		std::queue<std::wstring> pendingViews;
		std::unordered_set<std::wstring> viewKeys;
		std::function<bool(const DesignerDataBinding&, const std::wstring&)> includeBinding;
		includeBinding = [&](const DesignerDataBinding& binding,
			const std::wstring& nodeName)
		{
			if (binding.IsMultiBinding())
			{
				for (const auto& child : binding.ChildBindings)
					if (!includeBinding(child, nodeName)) return false;
				return true;
			}
			if (!binding.ElementName.empty()
				|| binding.RelativeSource != DesignerBindingRelativeSource::None)
				return true;
			if (!includePath(binding.SourceProperty))
				return Fail(L"控件 " + nodeName + L" 的绑定源路径无效。", outError);
			return true;
		};
		for (const auto& node : document.Nodes)
		{
			if (includedNodeIds && !includedNodeIds->contains(node.Id)) continue;
			for (const auto& [targetProperty, binding] : node.Bindings)
			{
				(void)targetProperty;
				if (!includeBinding(binding, node.Name)) return false;
			}
			const auto& resourceKey = node.Structure.ItemsSourceResource;
			if (const auto* view = document.FindCollectionView(resourceKey);
				view && viewKeys.insert(IdentityKey(view->Key)).second)
				pendingViews.push(view->Key);
		}
		while (!pendingViews.empty())
		{
			const auto* view = document.FindCollectionView(pendingViews.front());
			pendingViews.pop();
			if (!view) continue;
			if (!view->SourceBindingPath.empty()
				&& !includePath(view->SourceBindingPath))
				return Fail(L"CollectionViewSource Binding 路径无效。", outError);
			if (const auto* dependency = document.FindCollectionView(
				view->SourceResource);
				dependency && viewKeys.insert(IdentityKey(dependency->Key)).second)
				pendingViews.push(dependency->Key);
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

	bool CollectComponentDependencies(
		const DesignDocument& source,
		const std::unordered_set<int>& includedNodeIds,
		std::vector<DesignComponentDefinition>& output,
		std::wstring* outError)
	{
		output.clear();
		std::unordered_set<std::wstring> keys;
		struct PendingComponent
		{
			DesignerComponentType Type;
			std::wstring SubjectName;
			const std::vector<DesignNode>* ScopeNodes = nullptr;
			const DesignNode* ScopeNode = nullptr;
		};
		std::queue<PendingComponent> pending;
		auto include = [&](const DesignerComponentType& type,
			const std::wstring& subjectName,
			const std::vector<DesignNode>& scopeNodes,
			const DesignNode& scopeNode)
		{
			if (type.Empty()) return;
			const auto key = IdentityKey(type.RegistryKey());
			if (keys.insert(key).second)
				pending.push({ type, subjectName, &scopeNodes, &scopeNode });
		};
		for (const auto& node : source.Nodes)
		{
			if (!includedNodeIds.contains(node.Id)) continue;
			include(node.ComponentType, node.Name, source.Nodes, node);
			for (const auto& [storedName, value] : node.Events)
			{
				(void)value;
				DesignerComponentType ownerType;
				std::wstring eventName;
				if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
					storedName, ownerType, eventName))
					include(ownerType, node.Name, source.Nodes, node);
			}
		}
		while (!pending.empty())
		{
			auto request = std::move(pending.front());
			pending.pop();
			const auto* definition = request.ScopeNodes && request.ScopeNode
				? source.FindComponent(*request.ScopeNodes,
					*request.ScopeNode, request.Type)
				: source.FindComponent(request.Type);
			if (!definition)
				return Fail(L"控件 " + request.SubjectName
					+ L" 引用了缺失的组件定义。", outError);
			auto portable = *definition;
			portable.SourceDictionary.clear();
			output.push_back(std::move(portable));
			for (const auto& templateNode : definition->Template)
			{
				include(templateNode.ComponentType, definition->Type.XamlName,
					definition->Template, templateNode);
				for (const auto& [storedName, value]
					: templateNode.Events)
				{
					(void)value;
					DesignerComponentType ownerType;
					std::wstring eventName;
					if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
						storedName, ownerType, eventName))
						include(ownerType, definition->Type.XamlName,
							definition->Template, templateNode);
				}
			}
		}
		return true;
	}

	bool MergeComponentDependencies(
		const DesignDocument& target,
		const DesignDocument& fragment,
		DesignDocument& candidate,
		std::wstring* outError)
	{
		for (const auto& definition : fragment.Components)
		{
			if (const auto* existing = target.FindComponent(definition.Type))
			{
				auto existingContract = *existing;
				auto incomingContract = definition;
				existingContract.SourceDictionary.clear();
				incomingContract.SourceDictionary.clear();
				if (existingContract != incomingContract)
					return Fail(L"目标文档包含同名但契约不同的组件："
						+ definition.Type.XamlName, outError);
				continue;
			}
			const auto prefixConflict = std::find_if(
				target.Components.begin(), target.Components.end(),
				[&](const DesignComponentDefinition& current)
				{
					return IdentityKey(current.Type.XamlPrefix)
						== IdentityKey(definition.Type.XamlPrefix)
						&& IdentityKey(current.Type.XamlNamespace)
							!= IdentityKey(definition.Type.XamlNamespace);
				});
			if (prefixConflict != target.Components.end())
				return Fail(L"组件命名空间前缀冲突："
					+ definition.Type.XamlPrefix, outError);
			candidate.Components.push_back(definition);
		}
		return true;
	}

	std::wstring NodeItemTemplateKey(const DesignNode& node)
	{
		return node.Structure.ItemTemplate;
	}

	std::wstring NodeContentTemplateKey(const DesignNode& node)
	{
		return node.Structure.ContentTemplate;
	}

	std::wstring NodeHeaderTemplateKey(const DesignNode& node)
	{
		return node.Structure.HeaderTemplate;
	}

	std::wstring NodeControlTemplateKey(const DesignNode& node)
	{
		return node.Structure.ControlTemplate;
	}

	bool IsControlTemplateHostType(UIClass type) noexcept
	{
		return IsControlTemplateHostClass(type);
	}

	std::wstring NodeDataListKey(const DesignNode& node)
	{
		return node.Structure.ItemsSourceResource;
	}

	std::wstring NodeItemsPanelKey(const DesignNode& node)
	{
		return node.Structure.ItemsPanel;
	}

	std::wstring NodeGroupStyleKey(const DesignNode& node)
	{
		return node.Structure.GroupStyle;
	}

	std::wstring ResourceItemType(
		const DesignDocument& document,
		const std::wstring& resourceKey,
		const DesignerDataContextSchema& bindingSchema)
	{
		std::unordered_set<std::wstring> visited;
		auto key = resourceKey;
		while (!key.empty() && visited.insert(IdentityKey(key)).second)
		{
			if (const auto* list = document.FindDataList(key)) return list->ItemType;
			const auto* view = document.FindCollectionView(key);
			if (!view) return {};
			if (!view->SourceResource.empty())
			{
				key = view->SourceResource;
				continue;
			}
			const auto* property = DesignerDataContextSchemaUtils::Find(
				bindingSchema, view->SourceBindingPath);
			return property
				&& property->ObjectKind == DesignerDataObjectKind::BindingList
				? property->ItemType : std::wstring{};
		}
		return {};
	}

	std::wstring NodeItemsSourceItemType(
		const DesignDocument& document,
		const DesignNode& node,
		const DesignerDataContextSchema& bindingSchema)
	{
		if (const auto key = NodeDataListKey(node); !key.empty())
			return ResourceItemType(document, key, bindingSchema);
		const auto found = node.Bindings.find(L"ItemsSource");
		if (found == node.Bindings.end()) return {};
		const auto& binding = found->second;
		if (!binding.ElementName.empty()
			|| binding.RelativeSource != DesignerBindingRelativeSource::None
			|| binding.IsMultiBinding()) return {};
		const auto* property = DesignerDataContextSchemaUtils::Find(
			bindingSchema, binding.SourceProperty);
		return property
			&& property->ObjectKind == DesignerDataObjectKind::BindingList
			? property->ItemType : std::wstring{};
	}

	std::wstring NodeContentDataType(
		const DesignNode& node,
		const DesignerDataContextSchema& bindingSchema)
	{
		const auto found = node.Bindings.find(L"Content");
		if (found == node.Bindings.end()) return {};
		const auto& binding = found->second;
		if (!binding.ElementName.empty()
			|| binding.RelativeSource != DesignerBindingRelativeSource::None
			|| binding.IsMultiBinding()) return {};
		const auto* property = DesignerDataContextSchemaUtils::Find(
			bindingSchema, binding.SourceProperty);
		return property
			&& property->ObjectKind == DesignerDataObjectKind::BindingSource
			? property->DataType : std::wstring{};
	}

	std::wstring NodeHeaderDataType(
		const DesignNode& node,
		const DesignerDataContextSchema& bindingSchema)
	{
		const auto found = node.Bindings.find(L"Header");
		if (found == node.Bindings.end()) return {};
		const auto& binding = found->second;
		if (!binding.ElementName.empty()
			|| binding.RelativeSource != DesignerBindingRelativeSource::None
			|| binding.IsMultiBinding()) return {};
		const auto* property = DesignerDataContextSchemaUtils::Find(
			bindingSchema, binding.SourceProperty);
		return property
			&& property->ObjectKind == DesignerDataObjectKind::BindingSource
			? property->DataType : std::wstring{};
	}

	void RewriteNodeItemTemplate(
		DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& keyMap)
	{
		const auto key = NodeItemTemplateKey(node);
		if (!key.empty())
		{
			const auto found = keyMap.find(IdentityKey(key));
			if (found != keyMap.end())
				node.Structure.ItemTemplate = found->second;
		}
		const auto contentKey = NodeContentTemplateKey(node);
		if (!contentKey.empty())
		{
			const auto contentFound = keyMap.find(IdentityKey(contentKey));
			if (contentFound != keyMap.end())
				node.Structure.ContentTemplate = contentFound->second;
		}
		const auto headerKey = NodeHeaderTemplateKey(node);
		if (!headerKey.empty())
		{
			const auto headerFound = keyMap.find(IdentityKey(headerKey));
			if (headerFound != keyMap.end())
				node.Structure.HeaderTemplate = headerFound->second;
		}
	}

	void RewriteNodeDataList(
		DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& keyMap)
	{
		const auto key = NodeDataListKey(node);
		if (key.empty()) return;
		const auto found = keyMap.find(IdentityKey(key));
		if (found != keyMap.end())
			node.Structure.ItemsSourceResource = found->second;
	}

	void RewriteNodeItemsPanel(
		DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& keyMap)
	{
		const auto key = NodeItemsPanelKey(node);
		if (key.empty()) return;
		const auto found = keyMap.find(IdentityKey(key));
		if (found != keyMap.end())
			node.Structure.ItemsPanel = found->second;
	}

	void RewriteNodeGroupStyle(
		DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& keyMap)
	{
		const auto key = NodeGroupStyleKey(node);
		if (key.empty()) return;
		const auto found = keyMap.find(IdentityKey(key));
		if (found != keyMap.end())
			node.Structure.GroupStyle = found->second;
	}

	void ForEachNodePropertyResource(
		const DesignNode& node,
		const std::function<void(const std::wstring&, bool)>& callback)
	{
		for (const auto& [property, assignment] : node.Properties.Values)
		{
			(void)property;
			if (!assignment.ResourceKey.empty())
				callback(assignment.ResourceKey, false);
			if (!assignment.DynamicResourceKey.empty())
				callback(assignment.DynamicResourceKey, true);
		}
	}

	void RewriteNodePropertyResources(
		DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& keyMap,
		const DesignNode* scopeNode = nullptr,
		const Ownership* ownership = nullptr)
	{
		for (auto& [property, assignment] : node.Properties.Values)
		{
			(void)property;
			for (auto* key : {
				&assignment.ResourceKey, &assignment.DynamicResourceKey })
			{
				if (key->empty()) continue;
				if (scopeNode && ownership
					&& FindLexicalResource(*scopeNode, *ownership, *key).Resource)
					continue;
				const auto found = keyMap.find(IdentityKey(*key));
				if (found != keyMap.end())
					*key = found->second;
			}
		}
	}

	std::wstring NodeElementBindingName(const DesignerDataBinding& binding)
	{
		return binding.ElementName;
	}

	void CollectNodeElementBindingNames(
		const DesignerDataBinding& binding,
		std::vector<std::wstring>& output)
	{
		if (binding.IsMultiBinding())
		{
			for (const auto& child : binding.ChildBindings)
				CollectNodeElementBindingNames(child, output);
			return;
		}
		const auto name = NodeElementBindingName(binding);
		if (!name.empty()) output.push_back(name);
	}

	void RewriteBindingElementNames(
		DesignerDataBinding& binding,
		const std::unordered_map<std::wstring, std::wstring>& nameMap)
	{
		if (binding.IsMultiBinding())
		{
			for (auto& child : binding.ChildBindings)
				RewriteBindingElementNames(child, nameMap);
			return;
		}
		const auto sourceName = NodeElementBindingName(binding);
		if (sourceName.empty()) return;
		const auto found = nameMap.find(sourceName);
		if (found != nameMap.end())
			binding.ElementName = found->second;
	}

	void RewriteCommandTarget(
		std::wstring& target,
		const std::unordered_map<std::wstring, std::wstring>& nameMap)
	{
		if (target.empty()) return;
		const auto found = nameMap.find(target);
		if (found != nameMap.end()) target = found->second;
	}

	void RewriteNodeElementReferences(
		DesignNode& node,
		const std::unordered_map<std::wstring, std::wstring>& nameMap)
	{
		for (auto& [targetProperty, binding] : node.Bindings)
		{
			(void)targetProperty;
			RewriteBindingElementNames(binding, nameMap);
		}
		for (auto& binding : node.InputBindings)
			RewriteCommandTarget(binding.CommandTarget, nameMap);
		RewriteCommandTarget(node.Structure.CommandTarget, nameMap);
	}

	bool CollectDataDependencies(
		const DesignDocument& source,
		const std::unordered_set<int>& includedNodeIds,
		const DesignerDataContextSchema& bindingSchema,
		std::vector<DesignDataTypeDefinition>& outputTypes,
		std::vector<DesignDataTemplate>& outputTemplates,
		std::vector<DesignItemsPanelTemplate>& outputPanels,
		std::vector<DesignGroupStyle>& outputGroupStyles,
		std::vector<DesignDataList>& outputLists,
		std::vector<DesignCollectionViewSource>& outputViews,
		std::wstring* outError)
	{
		outputTypes.clear();
		outputTemplates.clear();
		outputPanels.clear();
		outputGroupStyles.clear();
		outputLists.clear();
		outputViews.clear();
		std::unordered_set<std::wstring> templateIdentities;
		struct PendingDataTemplate
		{
			std::wstring Key;
			std::wstring DataType;
			bool Implicit = false;
			const std::vector<DesignNode>* ScopeNodes = nullptr;
			const DesignNode* ScopeNode = nullptr;
		};
		std::queue<PendingDataTemplate> pendingTemplates;
		auto includeTemplateFromNode = [&](const DesignNode& node,
			const std::vector<DesignNode>& scopeNodes,
			const DesignerDataContextSchema& schema)
		{
			const auto itemKey = NodeItemTemplateKey(node);
			const auto contentKey = NodeContentTemplateKey(node);
			const auto headerKey = NodeHeaderTemplateKey(node);
			auto enqueueExplicit = [&](const std::wstring& key)
			{
				if (key.empty()) return;
				if (templateIdentities.insert(L"key:" + IdentityKey(key)).second)
					pendingTemplates.push({ key, {}, false, &scopeNodes, &node });
			};
			enqueueExplicit(itemKey);
			enqueueExplicit(contentKey);
			enqueueExplicit(headerKey);
			const bool itemsPresenter = IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, node.Type);
			const bool contentPresenter =
				node.Type == UIClass::UI_ContentPresenter
				|| IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, node.Type);
			const bool headerPresenter =
				IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, node.Type)
				|| IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, node.Type);
			auto enqueueImplicit = [&](const std::wstring& dataType)
			{
				if (dataType.empty()) return;
				const auto* definition = source.FindImplicitDataTemplate(
					scopeNodes, node, dataType);
				if (!definition) return;
				const auto identity = L"type:" + IdentityKey(definition->DataType);
				if (templateIdentities.insert(identity).second)
					pendingTemplates.push({ {}, definition->DataType, true,
						&scopeNodes, &node });
			};
			if (itemsPresenter && itemKey.empty())
				enqueueImplicit(NodeItemsSourceItemType(source, node, schema));
			if (contentPresenter && contentKey.empty())
				enqueueImplicit(NodeContentDataType(node, schema));
			if (headerPresenter && headerKey.empty())
				enqueueImplicit(NodeHeaderDataType(node, schema));
		};
		std::unordered_set<std::wstring> groupStyleKeys;
		auto includeGroupStyleFromNode = [&](const DesignNode& node,
			const std::vector<DesignNode>& scopeNodes)
		{
			const auto groupKey = NodeGroupStyleKey(node);
			if (groupKey.empty()
				|| !groupStyleKeys.insert(IdentityKey(groupKey)).second) return true;
			const auto* style = source.FindGroupStyle(
				scopeNodes, node, groupKey);
			if (!style) return Fail(
				L"控件片段引用了缺失的 GroupStyle：" + groupKey, outError);
			auto portable = *style;
			portable.SourceDictionary.clear();
			outputGroupStyles.push_back(std::move(portable));
			const auto* header = source.FindGroupStyleHeaderTemplate(
				scopeNodes, node, groupKey);
			if (!header && !style->HeaderTemplate.empty())
				return Fail(L"GroupStyle 引用了缺失的 DataTemplate："
					+ style->HeaderTemplate, outError);
			if (header)
			{
				const auto identity = header->IsImplicit()
					? L"type:" + IdentityKey(header->DataType)
					: L"key:" + IdentityKey(header->Key);
				if (templateIdentities.insert(identity).second)
				{
					const auto* owner = source.FindLocalGroupStyleOwner(
						scopeNodes, node, groupKey);
					pendingTemplates.push({ header->Key, header->DataType,
						header->IsImplicit(),
						owner ? &scopeNodes : nullptr, owner });
				}
			}
			return true;
		};
		for (const auto& node : source.Nodes)
			if (includedNodeIds.contains(node.Id))
			{
				includeTemplateFromNode(node, source.Nodes, bindingSchema);
				if (!includeGroupStyleFromNode(node, source.Nodes)) return false;
			}
		while (!pendingTemplates.empty())
		{
			auto request = std::move(pendingTemplates.front());
			pendingTemplates.pop();
			const auto* definition = request.Implicit
				? (request.ScopeNodes && request.ScopeNode
					? source.FindImplicitDataTemplate(*request.ScopeNodes,
						*request.ScopeNode, request.DataType)
					: source.FindImplicitDataTemplate(request.DataType))
				: (request.ScopeNodes && request.ScopeNode
					? source.FindDataTemplate(*request.ScopeNodes,
						*request.ScopeNode, request.Key)
					: source.FindDataTemplate(request.Key));
			if (!definition)
				return Fail(L"控件片段引用了缺失的 DataTemplate："
					+ (request.Implicit
						? L"{DataType " + request.DataType + L"}" : request.Key),
					outError);
			auto portable = *definition;
			portable.SourceDictionary.clear();
			outputTemplates.push_back(std::move(portable));
			DesignerDataContextSchema nestedSchema;
			if (const auto* type = source.FindDataType(definition->DataType))
				nestedSchema = type->Properties;
			else if (DesignDataResourceUtils::IsCollectionViewGroupDataType(
				definition->DataType))
				nestedSchema = DesignDataResourceUtils::BuildCollectionViewGroupSchema();
			if (definition->Hierarchical && definition->ItemsSourceBinding)
			{
				const auto* property = DesignerDataContextSchemaUtils::Find(
					nestedSchema,
					definition->ItemsSourceBinding->SourceProperty);
				if (property && property->ObjectKind
					== DesignerDataObjectKind::BindingList
					&& !property->ItemType.empty())
				{
					const auto* childTemplate = request.ScopeNodes
						&& request.ScopeNode
						? source.FindImplicitDataTemplate(*request.ScopeNodes,
							*request.ScopeNode, property->ItemType)
						: source.FindImplicitDataTemplate(property->ItemType);
					if (childTemplate)
					{
						const auto identity = L"type:"
							+ IdentityKey(childTemplate->DataType);
						if (templateIdentities.insert(identity).second)
							pendingTemplates.push({ {}, childTemplate->DataType,
								true, request.ScopeNodes, request.ScopeNode });
					}
				}
			}
			for (const auto& node : definition->Template)
			{
				includeTemplateFromNode(
					node, definition->Template, nestedSchema);
				if (!includeGroupStyleFromNode(node, definition->Template)) return false;
			}
		}
		std::unordered_set<std::wstring> panelKeys;
		auto includePanelFromNode = [&](const DesignNode& node,
			const std::vector<DesignNode>& scopeNodes)
		{
			const auto key = NodeItemsPanelKey(node);
			if (key.empty() || !panelKeys.insert(IdentityKey(key)).second) return true;
			const auto* definition = source.FindItemsPanelTemplate(
				scopeNodes, node, key);
			if (!definition) return false;
			auto portable = *definition;
			portable.SourceDictionary.clear();
			outputPanels.push_back(std::move(portable));
			return true;
		};
		for (const auto& node : source.Nodes)
			if (includedNodeIds.contains(node.Id)
				&& !includePanelFromNode(node, source.Nodes))
				return Fail(L"控件片段引用了缺失的 ItemsPanelTemplate。", outError);
		for (const auto& definition : outputTemplates)
			for (const auto& node : definition.Template)
				if (!includePanelFromNode(node, definition.Template))
					return Fail(L"DataTemplate 引用了缺失的 ItemsPanelTemplate。", outError);
		std::unordered_set<std::wstring> listKeys;
		std::unordered_set<std::wstring> viewKeys;
		std::function<bool(const std::wstring&)> includeListResource;
		includeListResource = [&](const std::wstring& key)
		{
			if (key.empty()) return true;
			if (const auto* definition = source.FindDataList(key))
			{
				if (!listKeys.insert(IdentityKey(definition->Key)).second) return true;
				auto portable = *definition;
				portable.SourceDictionary.clear();
				outputLists.push_back(std::move(portable));
				return true;
			}
			const auto* definition = source.FindCollectionView(key);
			if (!definition) return false;
			if (!viewKeys.insert(IdentityKey(definition->Key)).second) return true;
			auto portable = *definition;
			portable.SourceDictionary.clear();
			outputViews.push_back(std::move(portable));
			return definition->SourceResource.empty()
				|| includeListResource(definition->SourceResource);
		};
		for (const auto& node : source.Nodes)
			if (includedNodeIds.contains(node.Id)
				&& !includeListResource(NodeDataListKey(node)))
				return Fail(L"控件片段引用了缺失的列表资源。", outError);
		for (const auto& definition : outputTemplates)
			for (const auto& node : definition.Template)
				if (!includeListResource(NodeDataListKey(node)))
					return Fail(L"DataTemplate 引用了缺失的列表资源。", outError);

		std::unordered_set<std::wstring> typeNames;
		std::queue<std::wstring> pendingTypes;
		auto includeType = [&](const std::wstring& name)
		{
			if (!name.empty()
				&& !DesignDataResourceUtils::IsCollectionViewGroupDataType(name)
				&& typeNames.insert(IdentityKey(name)).second)
				pendingTypes.push(name);
		};
		for (const auto& property : bindingSchema)
		{
			if (property.ObjectKind == DesignerDataObjectKind::BindingList)
				includeType(property.ItemType);
			else if (property.ObjectKind == DesignerDataObjectKind::BindingSource)
				includeType(property.DataType);
		}
		for (const auto& definition : outputTemplates)
			includeType(definition.DataType);
		for (const auto& definition : outputLists)
			includeType(definition.ItemType);
		while (!pendingTypes.empty())
		{
			const auto name = pendingTypes.front();
			pendingTypes.pop();
			const auto* definition = source.FindDataType(name);
			if (!definition)
				return Fail(L"控件片段引用了缺失的 DataType：" + name,
					outError);
			auto portable = *definition;
			portable.SourceDictionary.clear();
			outputTypes.push_back(std::move(portable));
			for (const auto& property : definition->Properties)
			{
				if (property.ObjectKind == DesignerDataObjectKind::BindingList)
					includeType(property.ItemType);
				else if (property.ObjectKind == DesignerDataObjectKind::BindingSource)
					includeType(property.DataType);
			}
		}
		return true;
	}

	bool MergeDataDependencies(
		const DesignDocument& target,
		const DesignDocument& fragment,
		DesignDocument& candidate,
		std::unordered_map<std::wstring, std::wstring>& templateKeyMap,
		std::unordered_map<std::wstring, std::wstring>& panelKeyMap,
		std::unordered_map<std::wstring, std::wstring>& groupStyleKeyMap,
		std::unordered_map<std::wstring, std::wstring>& dataListKeyMap,
		std::wstring* outError)
	{
		for (const auto& definition : fragment.DataTypes)
		{
			if (const auto* existing = target.FindDataType(definition.Name))
			{
				auto left = *existing;
				auto right = definition;
				left.SourceDictionary.clear();
				right.SourceDictionary.clear();
				if (left != right)
					return Fail(L"目标文档包含同名但契约不同的 DataType："
						+ definition.Name, outError);
				continue;
			}
			candidate.DataTypes.push_back(definition);
		}

		std::unordered_set<std::wstring> usedPanelKeys;
		for (const auto& item : target.ItemsPanelTemplates)
			usedPanelKeys.insert(IdentityKey(item.Key));
		for (const auto& definition : fragment.ItemsPanelTemplates)
		{
			std::wstring destinationKey = definition.Key;
			if (const auto* existing = target.FindItemsPanelTemplate(definition.Key))
			{
				auto left = *existing;
				auto right = definition;
				left.SourceDictionary.clear();
				right.SourceDictionary.clear();
				if (left == right)
				{
					panelKeyMap.emplace(IdentityKey(definition.Key), existing->Key);
					continue;
				}
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedPanelKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			usedPanelKeys.insert(IdentityKey(destinationKey));
			panelKeyMap.emplace(IdentityKey(definition.Key), destinationKey);
		}
		for (const auto& definition : fragment.ItemsPanelTemplates)
		{
			const auto mapped = panelKeyMap.find(IdentityKey(definition.Key));
			if (mapped == panelKeyMap.end()) continue;
			if (target.FindItemsPanelTemplate(mapped->second)) continue;
			auto imported = definition;
			imported.Key = mapped->second;
			imported.SourceDictionary.clear();
			candidate.ItemsPanelTemplates.push_back(std::move(imported));
		}

		std::unordered_set<std::wstring> usedKeys;
		for (const auto& item : target.DataTemplates)
			if (!item.IsImplicit()) usedKeys.insert(IdentityKey(item.Key));
		for (const auto& definition : fragment.DataTemplates)
		{
			if (definition.IsImplicit())
			{
				if (const auto* existing = target.FindImplicitDataTemplate(
					definition.DataType))
				{
					auto left = *existing;
					auto right = definition;
					left.SourceDictionary.clear();
					right.SourceDictionary.clear();
					if (left != right)
						return Fail(L"目标文档包含同 DataType 但契约不同的隐式 DataTemplate："
							+ definition.DataType, outError);
				}
				continue;
			}
			std::wstring destinationKey = definition.Key;
			if (const auto* existing = target.FindDataTemplate(definition.Key))
			{
				auto left = *existing;
				auto right = definition;
				left.SourceDictionary.clear();
				right.SourceDictionary.clear();
				if (left == right)
				{
					templateKeyMap.emplace(IdentityKey(definition.Key), existing->Key);
					continue;
				}
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			usedKeys.insert(IdentityKey(destinationKey));
			templateKeyMap.emplace(IdentityKey(definition.Key), destinationKey);
		}
		std::unordered_set<std::wstring> usedGroupStyleKeys;
		for (const auto& item : target.GroupStyles)
			usedGroupStyleKeys.insert(IdentityKey(item.Key));
		for (const auto& definition : fragment.GroupStyles)
		{
			std::wstring destinationKey = definition.Key;
			if (const auto* existing = target.FindGroupStyle(definition.Key))
			{
				auto left = *existing;
				auto right = definition;
				if (const auto mapped = templateKeyMap.find(IdentityKey(right.HeaderTemplate));
					mapped != templateKeyMap.end()) right.HeaderTemplate = mapped->second;
				left.SourceDictionary.clear();
				right.SourceDictionary.clear();
				if (left == right)
				{
					groupStyleKeyMap.emplace(IdentityKey(definition.Key), existing->Key);
					continue;
				}
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedGroupStyleKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			usedGroupStyleKeys.insert(IdentityKey(destinationKey));
			groupStyleKeyMap.emplace(IdentityKey(definition.Key), destinationKey);
		}
		for (const auto& definition : fragment.GroupStyles)
		{
			const auto mapped = groupStyleKeyMap.find(IdentityKey(definition.Key));
			if (mapped == groupStyleKeyMap.end()
				|| target.FindGroupStyle(mapped->second)) continue;
			auto imported = definition;
			imported.Key = mapped->second;
			imported.SourceDictionary.clear();
			if (const auto header = templateKeyMap.find(IdentityKey(imported.HeaderTemplate));
				header != templateKeyMap.end()) imported.HeaderTemplate = header->second;
			candidate.GroupStyles.push_back(std::move(imported));
		}
		std::unordered_set<std::wstring> usedListKeys;
		for (const auto& item : target.DataLists)
			usedListKeys.insert(IdentityKey(item.Key));
		for (const auto& item : target.CollectionViews)
			usedListKeys.insert(IdentityKey(item.Key));
		for (const auto& definition : fragment.DataLists)
		{
			std::wstring destinationKey = definition.Key;
			if (const auto* existing = target.FindDataList(definition.Key))
			{
				auto left = *existing;
				auto right = definition;
				left.SourceDictionary.clear();
				right.SourceDictionary.clear();
				if (left == right)
				{
					dataListKeyMap.emplace(IdentityKey(definition.Key), existing->Key);
					continue;
				}
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedListKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			else if (target.FindCollectionView(definition.Key))
			{
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedListKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			usedListKeys.insert(IdentityKey(destinationKey));
			dataListKeyMap.emplace(IdentityKey(definition.Key), destinationKey);
		}
		for (const auto& definition : fragment.CollectionViews)
		{
			std::wstring destinationKey = definition.Key;
			if (const auto* existing = target.FindCollectionView(definition.Key))
			{
				auto left = *existing;
				auto right = definition;
				if (!right.SourceResource.empty())
					if (const auto source = dataListKeyMap.find(
						IdentityKey(right.SourceResource)); source != dataListKeyMap.end())
						right.SourceResource = source->second;
				left.SourceDictionary.clear();
				right.SourceDictionary.clear();
				if (left == right)
				{
					dataListKeyMap.emplace(IdentityKey(definition.Key), existing->Key);
					continue;
				}
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedListKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			else if (target.FindDataList(definition.Key))
			{
				const auto base = definition.Key + L"_Copy";
				destinationKey = base;
				int suffix = 2;
				while (usedListKeys.contains(IdentityKey(destinationKey)))
					destinationKey = base + std::to_wstring(suffix++);
			}
			usedListKeys.insert(IdentityKey(destinationKey));
			dataListKeyMap.emplace(IdentityKey(definition.Key), destinationKey);
		}
		for (const auto& definition : fragment.DataLists)
		{
			const auto mapped = dataListKeyMap.find(IdentityKey(definition.Key));
			if (mapped == dataListKeyMap.end()) continue;
			if (target.FindDataList(mapped->second)) continue;
			auto imported = definition;
			imported.Key = mapped->second;
			imported.SourceDictionary.clear();
			candidate.DataLists.push_back(std::move(imported));
		}
		for (const auto& definition : fragment.CollectionViews)
		{
			const auto mapped = dataListKeyMap.find(IdentityKey(definition.Key));
			if (mapped == dataListKeyMap.end()) continue;
			if (target.FindCollectionView(mapped->second)) continue;
			auto imported = definition;
			imported.Key = mapped->second;
			imported.SourceDictionary.clear();
			if (!imported.SourceResource.empty())
			{
				const auto source = dataListKeyMap.find(
					IdentityKey(imported.SourceResource));
				if (source != dataListKeyMap.end())
					imported.SourceResource = source->second;
			}
			candidate.CollectionViews.push_back(std::move(imported));
		}
		for (const auto& definition : fragment.DataTemplates)
		{
			const auto mapped = templateKeyMap.find(IdentityKey(definition.Key));
			if (definition.IsImplicit())
			{
				if (target.FindImplicitDataTemplate(definition.DataType)) continue;
			}
			else
			{
				if (mapped == templateKeyMap.end()) continue;
				if (target.FindDataTemplate(mapped->second)) continue;
			}
			auto imported = definition;
			if (!definition.IsImplicit()) imported.Key = mapped->second;
			imported.SourceDictionary.clear();
			for (auto& node : imported.Template)
			{
				RewriteNodeItemTemplate(node, templateKeyMap);
				RewriteNodeDataList(node, dataListKeyMap);
				RewriteNodeItemsPanel(node, panelKeyMap);
				RewriteNodeGroupStyle(node, groupStyleKeyMap);
			}
			candidate.DataTemplates.push_back(std::move(imported));
		}
		return true;
	}

	bool EqualsStyleName(
		const std::wstring& left,
		const std::wstring& right)
	{
		return IdentityKey(left) == IdentityKey(right);
	}

	std::wstring NodeStyleId(const DesignNode& node)
	{
		return node.Properties.StyleResourceKey;
	}

	std::wstring NodeItemContainerStyleId(const DesignNode& node)
	{
		if ((node.Type != UIClass::UI_ListBox
			&& node.Type != UIClass::UI_ComboBox
			&& node.Type != UIClass::UI_TreeView)) return {};
		return node.Structure.ItemContainerStyle;
	}

	bool StyleRuleMatchesNode(
		const DesignerStyleRule& rule,
		const DesignNode& node)
	{
		if (rule.HasType && rule.Type != UIClass::UI_Base
			&& rule.Type != node.Type) return false;
		if (!rule.ComponentType.Empty()
			&& rule.ComponentType != node.ComponentType) return false;
		if (rule.XamlType.Valid() && rule.XamlType != node.XamlType) return false;
		const auto styleKey = NodeStyleId(node);
		return styleKey.empty()
			? rule.Id.empty()
			: !rule.Id.empty() && EqualsStyleName(rule.Id, styleKey);
	}

	bool StyleRuleMatchesGeneratedContainer(
		const DesignerStyleRule& rule,
		const DesignNode& node)
	{
		if ((node.Type != UIClass::UI_ListBox
			&& node.Type != UIClass::UI_ComboBox
			&& node.Type != UIClass::UI_TreeView)
			|| !rule.ComponentType.Empty()) return false;
		const auto containerType = node.Type == UIClass::UI_ComboBox
			? UIClass::UI_ComboBoxItem
			: node.Type == UIClass::UI_TreeView
				? UIClass::UI_TreeViewItem : UIClass::UI_ListBoxItem;
		if (rule.HasType && rule.Type != UIClass::UI_Base
			&& rule.Type != containerType) return false;
		const auto styleId = NodeItemContainerStyleId(node);
		return styleId.empty()
			? rule.Id.empty()
			: !rule.Id.empty() && EqualsStyleName(rule.Id, styleId);
	}

	bool StyleRuleMatchesNodeTree(
		const DesignerStyleRule& rule,
		const DesignNode& node)
	{
		return StyleRuleMatchesNode(rule, node)
			|| StyleRuleMatchesGeneratedContainer(rule, node);
	}

	DesignerStyleSheet VisibleStylesForNode(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership)
	{
		DesignerStyleSheet visible = document.StyleSheet;
		std::vector<const DesignNode*> route;
		std::unordered_set<int> visited;
		for (auto current = node.Id;
			current > 0 && visited.insert(current).second;)
		{
			const auto found = ownership.NodeById.find(current);
			if (found == ownership.NodeById.end()) break;
			route.push_back(found->second);
			const auto parent = ownership.OwnerById.find(current);
			if (parent == ownership.OwnerById.end()) break;
			current = parent->second;
		}
		for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
			AppendStyleScope(visible, (*scope)->LocalResources);
		return visible;
	}

	bool ResolveNodeControlTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		LexicalControlTemplate& resolved,
		std::wstring& key,
		std::wstring* outError)
	{
		key = NodeControlTemplateKey(node);
		if (key.empty())
		{
			DesignerStyleSheet styles;
			if (!DesignerStyleSheetUtils::ResolveInheritance(
				VisibleStylesForNode(document, node, ownership),
				styles, outError)) return false;
			for (auto item = styles.Rules.rbegin(); item != styles.Rules.rend(); ++item)
			{
				const auto& rule = *item;
				if (!StyleRuleMatchesNode(rule, node)) continue;
				const auto setter = std::find_if(
					rule.Setters.begin(), rule.Setters.end(), [](const auto& candidate)
					{ return EqualsStyleName(candidate.PropertyName, L"Template"); });
				if (setter == rule.Setters.end()) continue;
				if (!setter->UsesResource || setter->UsesDynamicResource
					|| setter->ResourceKey.empty())
					return Fail(L"无法复制无效的 Style.Template。", outError);
				key = setter->ResourceKey;
				break;
			}
		}
		resolved = key.empty()
			? FindLexicalImplicitControlTemplate(document, node, ownership)
			: FindLexicalControlTemplate(document, node, ownership, key);
		return true;
	}

	bool ResolveGeneratedContainerControlTemplate(
		const DesignDocument& document,
		const DesignNode& node,
		const Ownership& ownership,
		LexicalControlTemplate& resolved,
		std::wstring& key,
		std::wstring* outError)
	{
		resolved = {};
		key.clear();
		if (node.Type != UIClass::UI_ListBox
			&& node.Type != UIClass::UI_ComboBox
			&& node.Type != UIClass::UI_TreeView) return true;

		DesignerStyleSheet styles;
		if (!DesignerStyleSheetUtils::ResolveInheritance(
			VisibleStylesForNode(document, node, ownership),
			styles, outError)) return false;
		for (auto item = styles.Rules.rbegin(); item != styles.Rules.rend(); ++item)
		{
			const auto& rule = *item;
			if (!StyleRuleMatchesGeneratedContainer(rule, node)) continue;
			const auto setter = std::find_if(
				rule.Setters.begin(), rule.Setters.end(), [](const auto& candidate)
				{ return EqualsStyleName(candidate.PropertyName, L"Template"); });
			if (setter == rule.Setters.end()) continue;
			if (!setter->UsesResource || setter->UsesDynamicResource
				|| setter->ResourceKey.empty())
				return Fail(L"无法复制无效的 ItemContainerStyle.Template。", outError);
			key = setter->ResourceKey;
			break;
		}

		if (!key.empty())
		{
			resolved = FindLexicalControlTemplate(document, node, ownership, key);
			return true;
		}
		auto container = node;
		container.Type = node.Type == UIClass::UI_ComboBox
			? UIClass::UI_ComboBoxItem
			: node.Type == UIClass::UI_TreeView
				? UIClass::UI_TreeViewItem : UIClass::UI_ListBoxItem;
		container.ComponentType = {};
		resolved = FindLexicalImplicitControlTemplate(
			document, container, ownership);
		return true;
	}

	bool CollectStyleDependencies(
		const DesignDocument& document,
		const std::unordered_set<int>* includedNodeIds,
		DesignerStyleSheet& output,
		std::wstring* outError)
	{
		output = {};
		std::wstring styleError;
		DesignerStyleSheet effectiveStyleSheet;
		if (!document.StyleSheet.Empty())
		{
			if (!DesignerStyleSheetUtils::Validate(
				document.StyleSheet, &styleError, document.ResourceBasePath,
				document.Resources))
				return Fail(L"无法复制无效的样式表：" + styleError, outError);
			if (!DesignerStyleSheetUtils::ResolveInheritance(
				document.StyleSheet, effectiveStyleSheet, &styleError))
				return Fail(L"无法解析复制样式依赖：" + styleError, outError);
		}

		std::unordered_set<std::wstring> requiredResources;
		std::unordered_set<std::wstring> optionalResources;
		const auto documentOwnership = BuildOwnership(document);
		std::vector<DesignerStyleResource> promotedLexicalResources;
		std::unordered_map<std::wstring, size_t> promotedLexicalResourceIndices;
		auto collectNodeResources = [&](const DesignNode& node,
			const Ownership& ownership,
			const std::unordered_set<int>* containedOwnerIds) -> bool
		{
			bool valid = true;
			ForEachNodePropertyResource(node,
				[&](const std::wstring& rawKey, bool optional)
				{
					if (!valid) return;
					const auto key = IdentityKey(rawKey);
					const auto lexical = FindLexicalResource(
						node, ownership, rawKey);
					if (lexical.Resource)
					{
						if (!containedOwnerIds
							|| containedOwnerIds->contains(lexical.OwnerId))
							return;
						auto portable = *lexical.Resource;
						portable.SourceDictionary.clear();
						const auto existing =
							promotedLexicalResourceIndices.find(key);
						if (existing == promotedLexicalResourceIndices.end())
						{
							promotedLexicalResourceIndices.emplace(
								key, promotedLexicalResources.size());
							promotedLexicalResources.push_back(
								std::move(portable));
						}
						else if (promotedLexicalResources[existing->second]
							!= portable)
						{
							valid = false;
							return;
						}
					}
					requiredResources.insert(key);
					if (optional) optionalResources.insert(key);
				});
			if (!valid)
				return Fail(L"复制范围从不同父级继承了同名但不同值的局部资源："
					+ node.Name, outError);
			return true;
		};
		std::queue<DesignerComponentType> pendingComponents;
		std::unordered_set<std::wstring> componentKeys;
		auto includeComponent = [&](const DesignerComponentType& type)
		{
			if (!type.Empty() && componentKeys.insert(
				IdentityKey(type.RegistryKey())).second)
				pendingComponents.push(type);
		};
		for (const auto& node : document.Nodes)
			if (!includedNodeIds || includedNodeIds->contains(node.Id))
			{
				if (!collectNodeResources(
					node, documentOwnership, includedNodeIds)) return false;
				includeComponent(node.ComponentType);
				for (const auto& [storedName, value] : node.Events)
					{
						(void)value;
						DesignerComponentType ownerType;
						std::wstring eventName;
						if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
							storedName, ownerType, eventName))
							includeComponent(ownerType);
					}
			}
		// A captured fragment already contains only the DataTemplates in its
		// dependency closure. During paste there is no synthetic included-id set,
		// so their direct property resources must be scanned explicitly.
		if (!includedNodeIds)
			for (const auto& definition : document.DataTemplates)
			{
				const auto templateOwnership = BuildOwnership(
					definition.Template);
				for (const auto& node : definition.Template)
				{
					if (!collectNodeResources(
						node, templateOwnership, nullptr)) return false;
					includeComponent(node.ComponentType);
					for (const auto& [storedName, value] : node.Events)
						{
							(void)value;
							DesignerComponentType ownerType;
							std::wstring eventName;
							if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
								storedName, ownerType, eventName))
							includeComponent(ownerType);
					}
				}
			}
		while (!pendingComponents.empty())
		{
			const auto type = pendingComponents.front();
			pendingComponents.pop();
			const auto* definition = document.FindComponent(type);
			if (!definition) continue;
			for (const auto& property : definition->Properties)
				if (!property.DefaultResourceKey.empty())
					requiredResources.insert(IdentityKey(property.DefaultResourceKey));
			auto collectAnimationResources = [&](const auto& animation)
			{
				if (animation.HasTo && animation.ToUsesResource)
					requiredResources.insert(IdentityKey(animation.ToResourceKey));
				if (animation.HasFrom && animation.FromUsesResource)
					requiredResources.insert(IdentityKey(animation.FromResourceKey));
				if (animation.HasBy && animation.ByUsesResource)
					requiredResources.insert(IdentityKey(animation.ByResourceKey));
				for (const auto& keyFrame : animation.KeyFrames)
					if (keyFrame.UsesResource)
						requiredResources.insert(IdentityKey(keyFrame.ResourceKey));
			};
			for (const auto& group : definition->VisualStateGroups)
			{
				for (const auto& transition : group.Transitions)
					for (const auto& animation : transition.Animations)
						collectAnimationResources(animation);
				for (const auto& state : group.States)
				{
					for (const auto& setter : state.Setters)
						if (setter.UsesResource)
							requiredResources.insert(IdentityKey(setter.ResourceKey));
					for (const auto& animation : state.Animations)
						collectAnimationResources(animation);
				}
			}
			for (const auto& trigger : definition->EventTriggers)
				for (const auto& action : trigger.Actions)
					for (const auto& animation : action.Animations)
						collectAnimationResources(animation);
			const auto templateOwnership = BuildOwnership(
				definition->Template);
			for (const auto& node : definition->Template)
			{
				if (!collectNodeResources(
					node, templateOwnership, nullptr)) return false;
				includeComponent(node.ComponentType);
				for (const auto& [storedName, value] : node.Events)
					{
						(void)value;
						DesignerComponentType ownerType;
						std::wstring eventName;
						if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
							storedName, ownerType, eventName))
							includeComponent(ownerType);
					}
			}
		}
		auto collectStyleActionResources = [&](const auto& actions)
		{
			for (const auto& action : actions)
				for (const auto& animation : action.Animations)
				{
					if (animation.HasTo && animation.ToUsesResource)
						requiredResources.insert(IdentityKey(animation.ToResourceKey));
					if (animation.HasFrom && animation.FromUsesResource)
						requiredResources.insert(IdentityKey(animation.FromResourceKey));
					if (animation.HasBy && animation.ByUsesResource)
						requiredResources.insert(IdentityKey(animation.ByResourceKey));
					for (const auto& keyFrame : animation.KeyFrames)
						if (keyFrame.UsesResource)
							requiredResources.insert(IdentityKey(keyFrame.ResourceKey));
				}
		};
		for (const auto& rule : effectiveStyleSheet.Rules)
		{
			const bool matches = std::any_of(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const DesignNode& node)
				{
					return (!includedNodeIds
						|| includedNodeIds->contains(node.Id))
						&& StyleRuleMatchesNodeTree(rule, node);
				});
			if (!matches) continue;
			auto portableRule = rule;
			portableRule.SourceDictionary.clear();
			output.Rules.push_back(std::move(portableRule));
			auto collectStyleSetters = [&](const auto& setters)
			{
				for (const auto& setter : setters)
					if (setter.UsesResource
						&& !EqualsStyleName(setter.PropertyName, L"Template"))
					{
						requiredResources.insert(IdentityKey(setter.ResourceKey));
						if (setter.UsesDynamicResource)
							optionalResources.insert(IdentityKey(setter.ResourceKey));
					}
			};
			collectStyleSetters(rule.Setters);
			collectStyleActionResources(rule.EnterActions);
			collectStyleActionResources(rule.ExitActions);
			for (const auto& trigger : rule.Triggers)
			{
				collectStyleSetters(trigger.Setters);
				collectStyleActionResources(trigger.EnterActions);
				collectStyleActionResources(trigger.ExitActions);
			}
		}

		auto unresolvedResources = requiredResources;
		for (const auto& resource : promotedLexicalResources)
		{
			output.Resources.push_back(resource);
			unresolvedResources.erase(IdentityKey(resource.Key));
		}
		for (const auto& resource : effectiveStyleSheet.Resources)
			if (requiredResources.contains(IdentityKey(resource.Key))
				&& !promotedLexicalResourceIndices.contains(
					IdentityKey(resource.Key)))
			{
				auto portableResource = resource;
				portableResource.SourceDictionary.clear();
				output.Resources.push_back(std::move(portableResource));
				unresolvedResources.erase(IdentityKey(resource.Key));
			}
		for (const auto& key : optionalResources)
			unresolvedResources.erase(key);
		if (!unresolvedResources.empty())
			return Fail(L"控件属性引用了缺失的样式资源："
				+ *unresolvedResources.begin(), outError);
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
					return StyleRuleMatchesNodeTree(rule, node);
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
		if (!DesignerStyleSheetUtils::ResolveInheritance(
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
	};

	void ApplyIsolatedNodeStyle(
		DesignNode& node,
		const IsolatedNodeStyle& style)
	{
		node.Properties.StyleResourceKey = style.Id;
	}

	bool MergeStyleDependencies(
		const DesignDocument& target,
		const DesignDocument& fragment,
		const std::unordered_map<std::wstring, std::wstring>& nameMap,
		DesignDocument& candidate,
		std::unordered_map<int, IsolatedNodeStyle>& isolatedStyles,
		std::unordered_map<int, std::wstring>& isolatedContainerStyles,
		std::unordered_map<std::wstring, std::wstring>& resourceMap,
		std::wstring* outError)
	{
		resourceMap.clear();
		DesignerStyleSheet dependencies;
		if (!CollectStyleDependencies(
			fragment, nullptr, dependencies, outError)) return false;
		if (dependencies.Empty()) return true;
		if (CanReuseStyleDependencies(target, fragment, dependencies))
		{
			for (const auto& resource : dependencies.Resources)
			{
				const auto* existing = FindStyleResource(target.StyleSheet, resource.Key);
				resourceMap.emplace(IdentityKey(resource.Key),
					existing ? existing->Key : resource.Key);
			}
			return true;
		}

		std::unordered_set<std::wstring> usedSelectorNames;
		std::unordered_set<std::wstring> usedResourceNames;
		for (const auto& node : target.Nodes)
		{
			const auto id = NodeStyleId(node);
			if (!id.empty()) usedSelectorNames.insert(IdentityKey(id));
			const auto containerId = NodeItemContainerStyleId(node);
			if (!containerId.empty()) usedSelectorNames.insert(IdentityKey(containerId));
		}
		for (const auto& rule : target.StyleSheet.Rules)
		{
			if (!rule.Id.empty()) usedSelectorNames.insert(IdentityKey(rule.Id));
		}
		for (const auto& resource : target.StyleSheet.Resources)
			usedResourceNames.insert(IdentityKey(resource.Key));

		for (const auto& resource : dependencies.Resources)
		{
			auto imported = resource;
			imported.Key = MakeUniqueName(
				L"CuiPasteResource_" + StyleToken(resource.Key),
				usedResourceNames);
			resourceMap.emplace(IdentityKey(resource.Key), imported.Key);
			candidate.StyleSheet.Resources.push_back(std::move(imported));
		}
		auto remapImportedRuleResources = [&](DesignerStyleRule& imported,
			const std::wstring& context)
		{
			auto remapSetters = [&](auto& setters)
			{
				for (auto& setter : setters)
				{
					if (!setter.UsesResource
						|| EqualsStyleName(setter.PropertyName, L"Template")) continue;
					const auto found = resourceMap.find(IdentityKey(setter.ResourceKey));
					if (found == resourceMap.end())
						return Fail(L"无法重映射" + context + L"样式资源："
							+ setter.ResourceKey, outError);
					setter.ResourceKey = found->second;
				}
				return true;
			};
			auto remapActions = [&](auto& actions)
			{
				for (auto& action : actions)
					for (auto& animation : action.Animations)
					{
						auto remap = [&](bool usesResource,
							std::wstring& key)
						{
							if (!usesResource) return true;
							const auto found = resourceMap.find(IdentityKey(key));
							if (found == resourceMap.end())
								return Fail(L"无法重映射" + context
									+ L"动画资源：" + key, outError);
							key = found->second;
							return true;
						};
						if (!remap(animation.HasFrom && animation.FromUsesResource,
							animation.FromResourceKey)
							|| !remap(animation.HasTo && animation.ToUsesResource,
								animation.ToResourceKey)
							|| !remap(animation.HasBy && animation.ByUsesResource,
								animation.ByResourceKey)) return false;
						for (auto& keyFrame : animation.KeyFrames)
							if (!remap(keyFrame.UsesResource,
								keyFrame.ResourceKey)) return false;
					}
				return true;
			};
			if (!remapSetters(imported.Setters)
				|| !remapActions(imported.EnterActions)
				|| !remapActions(imported.ExitActions)) return false;
			for (auto& trigger : imported.Triggers)
				if (!remapSetters(trigger.Setters)
					|| !remapActions(trigger.EnterActions)
					|| !remapActions(trigger.ExitActions)) return false;
			return true;
		};

		for (const auto& node : fragment.Nodes)
		{
			std::vector<const DesignerStyleRule*> matchingRules;
			for (const auto& rule : dependencies.Rules)
			{
				if (!StyleRuleMatchesNode(rule, node)) continue;
				matchingRules.push_back(&rule);
			}
			if (matchingRules.empty()) continue;

			IsolatedNodeStyle isolated;
			isolated.Id = MakeUniqueName(
				L"CuiPasteStyle_" + StyleToken(nameMap.at(node.Name)),
				usedSelectorNames);

			for (const auto* sourceRule : matchingRules)
			{
				auto imported = *sourceRule;
				imported.Id = isolated.Id;
				if (!remapImportedRuleResources(imported, L"粘贴"))
					return false;
				candidate.StyleSheet.Rules.push_back(std::move(imported));
			}
			isolatedStyles.emplace(node.Id, std::move(isolated));
		}

		for (const auto& node : fragment.Nodes)
		{
			std::vector<const DesignerStyleRule*> matchingRules;
			for (const auto& rule : dependencies.Rules)
				if (StyleRuleMatchesGeneratedContainer(rule, node))
					matchingRules.push_back(&rule);
			if (matchingRules.empty()) continue;
			const auto isolatedId = MakeUniqueName(
				L"CuiPasteItemContainer_" + StyleToken(nameMap.at(node.Name)),
				usedSelectorNames);
			for (const auto* sourceRule : matchingRules)
			{
				auto imported = *sourceRule;
				imported.Id = isolatedId;
				if (!remapImportedRuleResources(imported, L"粘贴项容器"))
					return false;
				candidate.StyleSheet.Rules.push_back(std::move(imported));
			}
			isolatedContainerStyles.emplace(node.Id, isolatedId);
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
		candidate.Window.Name = L"Clipboard";
		DesignerStyleValue clipboardTitle;
		clipboardTitle.Kind = DesignerStyleValueKind::String;
		clipboardTitle.Text = L"CUI Clipboard";
		if (!DesignerPropertyCatalog::ApplyNodeValue(
			candidate.Window, L"Title", clipboardTitle,
			nullptr, nullptr, outError))
			return false;
		for (const auto* dimension : { L"Width", L"Height" })
		{
			DesignerStyleValue value;
			if (!DesignerPropertyCatalog::CaptureNodeValue(
				source.Window, dimension, value, nullptr, outError)
				|| !DesignerPropertyCatalog::ApplyNodeValue(
					candidate.Window, dimension, value, nullptr, nullptr, outError))
				return false;
		}
		if (!CollectBindingSchema(
			source, &included, candidate.DataContextSchema, outError)) return false;
		std::vector<const DesignControlTemplate*> usedControlTemplates;
		auto includeControlTemplate = [&](const DesignControlTemplate* definition)
		{
			if (definition && std::find(usedControlTemplates.begin(),
				usedControlTemplates.end(), definition) == usedControlTemplates.end())
				usedControlTemplates.push_back(definition);
		};
		for (const auto& node : source.Nodes)
		{
			if (!included.contains(node.Id)) continue;
			if (IsControlTemplateHostType(node.Type)
				|| !node.ComponentType.Empty())
			{
				std::wstring key;
				LexicalControlTemplate resolved;
				if (!ResolveNodeControlTemplate(
					source, node, ownership, resolved, key, outError)) return false;
				if (!resolved.Definition && !key.empty())
					return Fail(L"控件片段引用了缺失的 ControlTemplate："
						+ key, outError);
				includeControlTemplate(resolved.Definition);
			}
			if (node.Type == UIClass::UI_ListBox
				|| node.Type == UIClass::UI_ComboBox
				|| node.Type == UIClass::UI_TreeView)
			{
				std::wstring key;
				LexicalControlTemplate resolved;
				if (!ResolveGeneratedContainerControlTemplate(
					source, node, ownership, resolved, key, outError)) return false;
				if (!resolved.Definition && !key.empty())
					return Fail(L"项控件片段引用了缺失的项容器 ControlTemplate："
						+ key, outError);
				includeControlTemplate(resolved.Definition);
			}
		}
		DesignDocument dependencySource = source;
		int dependencyId = 1;
		for (const auto& node : dependencySource.Nodes)
			dependencyId = (std::max)(dependencyId, node.Id + 1);
		auto projectTemplateNodes = [&](const std::vector<DesignNode>& nodes)
		{
			std::unordered_map<int, int> templateIdMap;
			for (const auto& templateNode : nodes)
				templateIdMap.emplace(templateNode.Id, dependencyId++);
			for (const auto& templateNode : nodes)
			{
				auto node = templateNode;
				node.Id = templateIdMap.at(templateNode.Id);
				if (templateNode.ParentId > 0)
				{
					const auto parent = templateIdMap.find(
						templateNode.ParentId);
					if (parent != templateIdMap.end())
						node.ParentId = parent->second;
				}
				included.insert(node.Id);
				dependencySource.Nodes.push_back(std::move(node));
			}
		};
		for (const auto* definition : usedControlTemplates)
			projectTemplateNodes(definition->Template);
		if (!CollectDataDependencies(
			dependencySource, included, candidate.DataContextSchema,
			candidate.DataTypes, candidate.DataTemplates,
			candidate.ItemsPanelTemplates,
			candidate.GroupStyles,
			candidate.DataLists, candidate.CollectionViews, outError)) return false;
		for (const auto& definition : candidate.DataTemplates)
			projectTemplateNodes(definition.Template);
		// DataTemplate and ControlTemplate dependencies are projected into the temporary document as
		// roots so the existing style-closure collector can inspect them. Preserve
		// the declaration/use scope's visible values in that temporary global scope;
		// the final fragment still stores the same values on its promoted local root.
		for (const auto& original : source.Nodes)
		{
			if (!included.contains(original.Id)) continue;
			DesignerStyleSheet visible = source.StyleSheet;
			std::vector<const DesignNode*> route;
			std::unordered_set<int> visited;
			for (auto current = original.Id;
				current > 0 && visited.insert(current).second;)
			{
				const auto found = ownership.NodeById.find(current);
				if (found == ownership.NodeById.end()) break;
				route.push_back(found->second);
				const auto parent = ownership.OwnerById.find(current);
				if (parent == ownership.OwnerById.end()) break;
				current = parent->second;
			}
			for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
				AppendStyleScope(visible, (*scope)->LocalResources);
			for (auto resource : visible.Resources)
			{
				resource.SourceDictionary.clear();
				dependencySource.StyleSheet.Resources.erase(std::remove_if(
					dependencySource.StyleSheet.Resources.begin(),
					dependencySource.StyleSheet.Resources.end(), [&](const auto& current)
					{ return IdentityKey(current.Key) == IdentityKey(resource.Key); }),
					dependencySource.StyleSheet.Resources.end());
				dependencySource.StyleSheet.Resources.push_back(std::move(resource));
			}
		}
		if (!CollectStyleDependencies(
			dependencySource, &included, candidate.StyleSheet, outError)) return false;
		if (!CollectComponentDependencies(
			dependencySource, included, candidate.Components, outError)) return false;
		candidate.Nodes.reserve(included.size());
		std::unordered_set<std::wstring> includedNames;
		for (const auto& sourceNode : source.Nodes)
			if (included.contains(sourceNode.Id))
				includedNames.insert(sourceNode.Name);
		int rootOrder = 0;
		for (const auto& sourceNode : source.Nodes)
		{
			if (!included.contains(sourceNode.Id)) continue;
			for (const auto& [targetProperty, binding]
				: sourceNode.Bindings)
				{
					std::vector<std::wstring> elementNames;
					CollectNodeElementBindingNames(binding, elementNames);
					for (const auto& elementName : elementNames)
						if (!includedNames.contains(elementName))
							return Fail(L"控件 " + sourceNode.Name + L" 的绑定 "
								+ targetProperty
								+ L" 引用了复制范围外的 ElementName："
								+ elementName, outError);
				}
			for (const auto& binding : sourceNode.InputBindings)
				if (!binding.CommandTarget.empty()
					&& !includedNames.contains(binding.CommandTarget))
					return Fail(L"控件 " + sourceNode.Name
						+ L" 的 InputBinding.CommandTarget 引用了复制范围外的 x:Name："
						+ binding.CommandTarget, outError);
			if (!sourceNode.Structure.CommandTarget.empty()
				&& !includedNames.contains(sourceNode.Structure.CommandTarget))
				return Fail(L"控件 " + sourceNode.Name
					+ L" 的 CommandTarget 引用了复制范围外的 x:Name："
					+ sourceNode.Structure.CommandTarget, outError);
			auto node = sourceNode;
			if (!MakeLocalStylesPortable(
				source, sourceNode, ownership, included, node, outError))
				return false;
			if (roots.contains(node.Id))
			{
				node.ParentId = 0;
				node.ParentRef.clear();
				node.Order = rootOrder++;
			}
			candidate.Nodes.push_back(std::move(node));
		}
		std::unordered_map<int, DesignNode*> candidateById;
		for (auto& node : candidate.Nodes) candidateById.emplace(node.Id, &node);
		std::unordered_set<int> promotedValueScopes;
		auto fragmentRootId = [&](int id)
		{
			for (;;)
			{
				const auto parent = ownership.OwnerById.find(id);
				if (parent == ownership.OwnerById.end()
					|| !included.contains(parent->second)) return id;
				id = parent->second;
			}
		};
		auto promoteVisibleValues = [&](const DesignNode& original,
			DesignNode& root)
		{
			if (!promotedValueScopes.insert(root.Id).second) return;
			DesignerStyleSheet visible = source.StyleSheet;
			std::vector<const DesignNode*> route;
			std::unordered_set<int> visited;
			for (auto current = original.Id;
				current > 0 && visited.insert(current).second;)
			{
				const auto found = ownership.NodeById.find(current);
				if (found == ownership.NodeById.end()) break;
				route.push_back(found->second);
				const auto parent = ownership.OwnerById.find(current);
				if (parent == ownership.OwnerById.end()) break;
				current = parent->second;
			}
			for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
				AppendStyleScope(visible, (*scope)->LocalResources);
			for (auto resource : visible.Resources)
			{
				resource.SourceDictionary.clear();
				candidate.StyleSheet.Resources.erase(std::remove_if(
					candidate.StyleSheet.Resources.begin(),
					candidate.StyleSheet.Resources.end(), [&](const auto& current)
					{ return IdentityKey(current.Key) == IdentityKey(resource.Key); }),
					candidate.StyleSheet.Resources.end());
				candidate.StyleSheet.Resources.push_back(resource);
				root.LocalResources.Resources.erase(std::remove_if(
					root.LocalResources.Resources.begin(),
					root.LocalResources.Resources.end(), [&](const auto& current)
					{ return IdentityKey(current.Key) == IdentityKey(resource.Key); }),
					root.LocalResources.Resources.end());
				root.LocalResources.Resources.push_back(std::move(resource));
			}
		};
		for (const auto& original : source.Nodes)
		{
			if (!included.contains(original.Id)) continue;
			auto* root = candidateById.at(fragmentRootId(original.Id));
			if (!original.ComponentType.Empty())
			{
				const auto resolved = FindLexicalComponent(
					source, original, ownership, original.ComponentType);
				if (resolved.Definition && resolved.OwnerId > 0
					&& !included.contains(resolved.OwnerId))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					root->LocalObjectResources.Components.erase(std::remove_if(
						root->LocalObjectResources.Components.begin(),
						root->LocalObjectResources.Components.end(), [&](const auto& current)
						{ return current.Type == portable.Type; }),
						root->LocalObjectResources.Components.end());
					root->LocalObjectResources.Components.push_back(
						std::move(portable));
					promoteVisibleValues(original, *root);
				}
			}
			if (IsControlTemplateHostType(original.Type)
				|| !original.ComponentType.Empty())
			{
				std::wstring controlTemplateKey;
				LexicalControlTemplate resolved;
				if (!ResolveNodeControlTemplate(source, original, ownership,
					resolved, controlTemplateKey, outError)) return false;
				if (!resolved.Definition && !controlTemplateKey.empty())
					return Fail(L"控件 " + original.Name
						+ L" 引用了缺失的 ControlTemplate："
						+ controlTemplateKey, outError);
				if (resolved.Definition
					&& (resolved.OwnerId == 0
						|| !included.contains(resolved.OwnerId)))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					auto existing = std::find_if(
						root->LocalObjectResources.ControlTemplates.begin(),
						root->LocalObjectResources.ControlTemplates.end(),
						[&](const auto& current)
						{
							return current.HasSameResourceIdentity(portable);
						});
					if (existing != root->LocalObjectResources.ControlTemplates.end())
					{
						auto current = *existing;
						current.SourceDictionary.clear();
						if (current != portable)
							return Fail(L"复制范围内存在同身份但契约不同的 ControlTemplate："
								+ portable.DisplayName(), outError);
					}
					else root->LocalObjectResources.ControlTemplates.push_back(
						std::move(portable));
					promoteVisibleValues(original, *root);
				}
			}
			if (original.Type == UIClass::UI_ListBox
				|| original.Type == UIClass::UI_ComboBox
				|| original.Type == UIClass::UI_TreeView)
			{
				std::wstring controlTemplateKey;
				LexicalControlTemplate resolved;
				if (!ResolveGeneratedContainerControlTemplate(
					source, original, ownership, resolved,
					controlTemplateKey, outError)) return false;
				if (!resolved.Definition && !controlTemplateKey.empty())
					return Fail(L"项控件 " + original.Name
						+ L" 引用了缺失的项容器 ControlTemplate："
						+ controlTemplateKey, outError);
				if (resolved.Definition
					&& (resolved.OwnerId == 0
						|| !included.contains(resolved.OwnerId)))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					auto existing = std::find_if(
						root->LocalObjectResources.ControlTemplates.begin(),
						root->LocalObjectResources.ControlTemplates.end(),
						[&](const auto& current)
						{ return current.HasSameResourceIdentity(portable); });
					if (existing != root->LocalObjectResources.ControlTemplates.end())
					{
						auto current = *existing;
						current.SourceDictionary.clear();
						if (current != portable)
							return Fail(L"复制范围内存在同身份但契约不同的项容器 ControlTemplate："
								+ portable.DisplayName(), outError);
					}
					else root->LocalObjectResources.ControlTemplates.push_back(
						std::move(portable));
					promoteVisibleValues(original, *root);
				}
			}
			const auto itemTemplateKey = NodeItemTemplateKey(original);
			const auto contentTemplateKey = NodeContentTemplateKey(original);
			const auto headerTemplateKey = NodeHeaderTemplateKey(original);
			auto promoteExplicitTemplate = [&](const std::wstring& templateKey)
			{
				if (templateKey.empty()) return;
				const auto resolved = FindLexicalDataTemplate(
					source, original, ownership, templateKey);
				if (resolved.Definition && resolved.OwnerId > 0
					&& !included.contains(resolved.OwnerId))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					root->LocalObjectResources.DataTemplates.erase(std::remove_if(
						root->LocalObjectResources.DataTemplates.begin(),
						root->LocalObjectResources.DataTemplates.end(), [&](const auto& current)
						{ return IdentityKey(current.Key) == IdentityKey(portable.Key); }),
						root->LocalObjectResources.DataTemplates.end());
					root->LocalObjectResources.DataTemplates.push_back(
						std::move(portable));
					promoteVisibleValues(original, *root);
				}
			};
			promoteExplicitTemplate(itemTemplateKey);
			promoteExplicitTemplate(contentTemplateKey);
			promoteExplicitTemplate(headerTemplateKey);

			auto promoteImplicitTemplate = [&](const std::wstring& dataType)
			{
				if (dataType.empty()) return;
				const auto resolved = dataType.empty()
					? LexicalDataTemplate{}
					: FindLexicalImplicitDataTemplate(
						source, original, ownership, dataType);
				if (resolved.Definition && resolved.OwnerId > 0
					&& !included.contains(resolved.OwnerId))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					root->LocalObjectResources.DataTemplates.erase(std::remove_if(
						root->LocalObjectResources.DataTemplates.begin(),
						root->LocalObjectResources.DataTemplates.end(), [&](const auto& current)
						{ return current.HasSameResourceIdentity(portable); }),
						root->LocalObjectResources.DataTemplates.end());
					root->LocalObjectResources.DataTemplates.push_back(
						std::move(portable));
					promoteVisibleValues(original, *root);
				}
			};
			if (itemTemplateKey.empty()
				&& IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, original.Type))
				promoteImplicitTemplate(NodeItemsSourceItemType(
					source, original, source.DataContextSchema));
			if (contentTemplateKey.empty()
				&& (original.Type == UIClass::UI_ContentPresenter
					|| IsUIClassAssignableFrom(
						UIClass::UI_ContentControl, original.Type)))
				promoteImplicitTemplate(NodeContentDataType(
					original, source.DataContextSchema));
			if (headerTemplateKey.empty()
				&& (IsUIClassAssignableFrom(
						UIClass::UI_HeaderedContentControl, original.Type)
					|| IsUIClassAssignableFrom(
						UIClass::UI_HeaderedItemsControl, original.Type)))
				promoteImplicitTemplate(NodeHeaderDataType(
					original, source.DataContextSchema));
			const auto panelKey = NodeItemsPanelKey(original);
			if (!panelKey.empty())
			{
				const auto resolved = FindLexicalItemsPanelTemplate(
					source, original, ownership, panelKey);
				if (resolved.Definition && resolved.OwnerId > 0
					&& !included.contains(resolved.OwnerId))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					root->LocalObjectResources.ItemsPanelTemplates.erase(
						std::remove_if(
							root->LocalObjectResources.ItemsPanelTemplates.begin(),
							root->LocalObjectResources.ItemsPanelTemplates.end(),
							[&](const auto& current)
							{ return IdentityKey(current.Key) == IdentityKey(portable.Key); }),
						root->LocalObjectResources.ItemsPanelTemplates.end());
					root->LocalObjectResources.ItemsPanelTemplates.push_back(
						std::move(portable));
				}
			}
			const auto groupKey = NodeGroupStyleKey(original);
			if (!groupKey.empty())
			{
				const auto resolved = FindLexicalGroupStyle(
					source, original, ownership, groupKey);
				if (resolved.Definition && resolved.OwnerId > 0
					&& !included.contains(resolved.OwnerId))
				{
					auto portable = *resolved.Definition;
					portable.SourceDictionary.clear();
					root->LocalObjectResources.GroupStyles.erase(std::remove_if(
						root->LocalObjectResources.GroupStyles.begin(),
						root->LocalObjectResources.GroupStyles.end(),
						[&](const auto& current)
						{ return IdentityKey(current.Key) == IdentityKey(portable.Key); }),
						root->LocalObjectResources.GroupStyles.end());
					root->LocalObjectResources.GroupStyles.push_back(
						std::move(portable));
					promoteVisibleValues(original, *root);
				}
				if (resolved.Definition && resolved.OwnerId > 0)
				{
					const auto declaration = ownership.NodeById.find(
						resolved.OwnerId);
					if (declaration != ownership.NodeById.end())
					{
						const auto header = resolved.Definition->HeaderTemplate.empty()
							? FindLexicalImplicitDataTemplate(source,
								*declaration->second, ownership,
								std::wstring(CollectionViewGroupDataTypeName))
							: FindLexicalDataTemplate(source,
								*declaration->second, ownership,
								resolved.Definition->HeaderTemplate);
						if (header.Definition && header.OwnerId > 0
							&& !included.contains(header.OwnerId))
						{
							auto portable = *header.Definition;
							portable.SourceDictionary.clear();
							root->LocalObjectResources.DataTemplates.erase(
								std::remove_if(
									root->LocalObjectResources.DataTemplates.begin(),
									root->LocalObjectResources.DataTemplates.end(),
									[&](const auto& current)
									{ return current.HasSameResourceIdentity(portable); }),
								root->LocalObjectResources.DataTemplates.end());
							root->LocalObjectResources.DataTemplates.push_back(
								std::move(portable));
							promoteVisibleValues(*declaration->second, *root);
						}
					}
				}
			}
		}
		// A copied Style.Template rule must live in the same lexical resource
		// scope as the ControlTemplate promoted above. Keeping that rule at the
		// fragment document level would create an impossible downward
		// StaticResource reference and fail validation after paste.
		std::vector<DesignerStyleRule> documentRules;
		documentRules.reserve(candidate.StyleSheet.Rules.size());
		for (auto rule : candidate.StyleSheet.Rules)
		{
			std::vector<std::wstring> templateKeys;
			for (const auto& setter : rule.Setters)
				if (setter.PropertyName == L"Template"
					&& setter.UsesResource && !setter.UsesDynamicResource
					&& !setter.ResourceKey.empty())
					templateKeys.push_back(setter.ResourceKey);
			if (templateKeys.empty())
			{
				documentRules.push_back(std::move(rule));
				continue;
			}

			bool localized = false;
			for (const auto rootId : roots)
			{
				auto* root = candidateById.at(rootId);
				const bool allVisible = std::all_of(
					templateKeys.begin(), templateKeys.end(), [&](const auto& key)
					{
						return std::any_of(
							root->LocalObjectResources.ControlTemplates.begin(),
							root->LocalObjectResources.ControlTemplates.end(),
							[&](const auto& definition)
							{
								return !definition.IsImplicit()
									&& definition.Key == key;
							});
					});
				if (!allVisible) continue;
				auto localRule = rule;
				localRule.SourceDictionary.clear();
				root->LocalResources.Rules.push_back(std::move(localRule));
				localized = true;
			}
			if (!localized) documentRules.push_back(std::move(rule));
		}
		candidate.StyleSheet.Rules = std::move(documentRules);
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
		std::unordered_map<int, std::wstring> contentPropertyByRoot;
		contentPropertyByRoot.reserve(effectiveTargets.size());
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
			const DesignNode* destinationParent = nullptr;
			if (destination.ParentId > 0)
			{
				const auto* resolved = targetGraph.FindById(destination.ParentId);
				if (!resolved)
					return Fail(L"粘贴目标父控件已经不存在。", outError);
				const auto& parent = target.Nodes[resolved->SourceIndex];
				destinationParent = &parent;
				const auto* fragmentRoot = fragmentGraph.FindById(
					destination.FragmentRootId);
				if (parent.Type == UIClass::UI_TabControl
					&& (!fragmentRoot || fragment.Nodes[
						fragmentRoot->SourceIndex].Type != UIClass::UI_TabItem))
					return Fail(L"TabControl 只能接收 TabItem。", outError);
			}
			else if (!destination.ParentRef.empty())
			{
				const auto* resolved = targetGraph.FindByName(
					destination.ParentRef);
				if (!resolved)
					return Fail(L"粘贴目标控件已经不存在："
						+ destination.ParentRef, outError);
				destinationParent = &target.Nodes[resolved->SourceIndex];
			}

			std::wstring requestedContent;
			if (destinationParent && !destinationParent->ComponentType.Empty())
			{
				const auto* definition = target.FindComponent(
					destinationParent->ComponentType);
				if (!definition)
					return Fail(L"粘贴目标引用了缺失的组件定义。", outError);
				const auto* fragmentRoot = fragmentGraph.FindById(
					destination.FragmentRootId);
				if (!fragmentRoot)
					return Fail(L"无法解析剪贴板片段根。", outError);
				requestedContent = destination.ComponentContentProperty
					? *destination.ComponentContentProperty
					: fragment.Nodes[fragmentRoot->SourceIndex]
						.ComponentContentProperty;
				if (requestedContent.empty())
				{
					const auto defaultContent = std::find_if(
						definition->ContentProperties.begin(),
						definition->ContentProperties.end(),
						[](const auto& property) { return property.IsDefault; });
					if (defaultContent == definition->ContentProperties.end())
						return Fail(L"目标组件没有默认视觉内容属性。", outError);
					requestedContent = defaultContent->Name;
				}
				const auto contract = std::find_if(
					definition->ContentProperties.begin(),
					definition->ContentProperties.end(), [&](const auto& property)
					{
						return IdentityKey(property.Name) == IdentityKey(requestedContent);
					});
				if (contract == definition->ContentProperties.end())
					return Fail(L"目标组件不存在视觉内容属性："
						+ requestedContent, outError);
				requestedContent = contract->Name;
			}
			else if (destination.ComponentContentProperty
				&& !destination.ComponentContentProperty->empty())
			{
				return Fail(L"普通容器不能指定组件视觉内容属性。", outError);
			}
			contentPropertyByRoot.emplace(
				destination.FragmentRootId, std::move(requestedContent));
		}

		// Validate single-valued content before mutating the candidate. Existing
		// public children and all roots in this paste participate in the count.
		for (const auto& destination : effectiveTargets)
		{
			if (destination.ParentId <= 0) continue;
			const auto* resolved = targetGraph.FindById(destination.ParentId);
			if (!resolved) continue;
			const auto& parent = target.Nodes[resolved->SourceIndex];
			if (parent.ComponentType.Empty()) continue;
			const auto* definition = target.FindComponent(parent.ComponentType);
			if (!definition) continue;
			const auto& slot = contentPropertyByRoot.at(
				destination.FragmentRootId);
			const auto contract = std::find_if(
				definition->ContentProperties.begin(),
				definition->ContentProperties.end(), [&](const auto& property)
				{
					return IdentityKey(property.Name) == IdentityKey(slot);
				});
			if (contract == definition->ContentProperties.end()
				|| contract->Cardinality !=
					DesignerComponentContentCardinality::Single) continue;
			size_t count = std::count_if(
				target.Nodes.begin(), target.Nodes.end(), [&](const auto& node)
				{
					return (node.ParentId == parent.Id
						|| node.ParentRef == parent.Name)
						&& IdentityKey(node.ComponentContentProperty) == IdentityKey(slot);
				});
			count += std::count_if(
				effectiveTargets.begin(), effectiveTargets.end(),
				[&](const auto& candidate)
				{
					return candidate.ParentId == parent.Id
						&& IdentityKey(contentPropertyByRoot.at(
							candidate.FragmentRootId)) == IdentityKey(slot);
				});
			if (count > 1)
				return Fail(L"组件单值视觉内容属性已经被占用："
					+ slot, outError);
		}

		DesignDocument candidate = target;
		if (!MergeBindingSchema(target, fragment, candidate, outError)) return false;
		std::unordered_map<std::wstring, std::wstring> templateKeyMap;
		std::unordered_map<std::wstring, std::wstring> panelKeyMap;
		std::unordered_map<std::wstring, std::wstring> groupStyleKeyMap;
		std::unordered_map<std::wstring, std::wstring> dataListKeyMap;
		if (!MergeDataDependencies(
			target, fragment, candidate, templateKeyMap,
			panelKeyMap, groupStyleKeyMap,
			dataListKeyMap, outError)) return false;
		if (!MergeComponentDependencies(
			target, fragment, candidate, outError)) return false;
		std::unordered_set<std::wstring> usedNames;
		usedNames.reserve(target.Nodes.size() + fragment.Nodes.size());
		for (const auto& node : target.Nodes) usedNames.insert(IdentityKey(node.Name));

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
		std::unordered_map<int, std::wstring> isolatedContainerStyles;
		isolatedContainerStyles.reserve(fragment.Nodes.size());
		std::unordered_map<std::wstring, std::wstring> styleResourceMap;
		const auto ownership = BuildOwnership(fragment);
		if (!MergeStyleDependencies(
			target, fragment, nameMap, candidate,
			isolatedStyles, isolatedContainerStyles,
			styleResourceMap, outError)) return false;
		for (const auto& definition : fragment.DataTemplates)
		{
			const auto mapped = templateKeyMap.find(IdentityKey(definition.Key));
			if (definition.IsImplicit())
			{
				if (target.FindImplicitDataTemplate(definition.DataType)) continue;
			}
			else if (mapped == templateKeyMap.end()
				|| target.FindDataTemplate(mapped->second)) continue;
			const auto imported = std::find_if(
				candidate.DataTemplates.begin(), candidate.DataTemplates.end(),
				[&](const auto& item)
				{
					return definition.IsImplicit()
						? item.IsImplicit()
							&& IdentityKey(item.DataType) == IdentityKey(definition.DataType)
						: IdentityKey(item.Key) == IdentityKey(mapped->second);
				});
			if (imported != candidate.DataTemplates.end())
			{
				const auto templateOwnership = BuildOwnership(
					imported->Template);
				for (auto& node : imported->Template)
					RewriteNodePropertyResources(
						node, styleResourceMap, &node, &templateOwnership);
			}
		}
		for (const auto& definition : fragment.Components)
		{
			if (target.FindComponent(definition.Type)) continue;
			const auto imported = std::find_if(
				candidate.Components.begin(), candidate.Components.end(),
				[&](const auto& item) { return item.Type == definition.Type; });
			if (imported == candidate.Components.end()) continue;
			for (auto& property : imported->Properties)
				if (!property.DefaultResourceKey.empty())
				{
					const auto mapped = styleResourceMap.find(
						IdentityKey(property.DefaultResourceKey));
					if (mapped != styleResourceMap.end())
						property.DefaultResourceKey = mapped->second;
				}
			const auto templateOwnership = BuildOwnership(imported->Template);
			for (auto& node : imported->Template)
				RewriteNodePropertyResources(
					node, styleResourceMap, &node, &templateOwnership);
			auto remapAnimationResources = [&](auto& animation)
			{
				if (animation.HasTo && animation.ToUsesResource)
				{
					const auto mapped = styleResourceMap.find(
						IdentityKey(animation.ToResourceKey));
					if (mapped != styleResourceMap.end())
						animation.ToResourceKey = mapped->second;
				}
				if (animation.HasFrom && animation.FromUsesResource)
				{
					const auto mapped = styleResourceMap.find(
						IdentityKey(animation.FromResourceKey));
					if (mapped != styleResourceMap.end())
						animation.FromResourceKey = mapped->second;
				}
				if (animation.HasBy && animation.ByUsesResource)
				{
					const auto mapped = styleResourceMap.find(
						IdentityKey(animation.ByResourceKey));
					if (mapped != styleResourceMap.end())
						animation.ByResourceKey = mapped->second;
				}
				for (auto& keyFrame : animation.KeyFrames)
					if (keyFrame.UsesResource)
					{
						const auto mapped = styleResourceMap.find(
							IdentityKey(keyFrame.ResourceKey));
						if (mapped != styleResourceMap.end())
							keyFrame.ResourceKey = mapped->second;
					}
			};
			for (auto& group : imported->VisualStateGroups)
			{
				for (auto& transition : group.Transitions)
					for (auto& animation : transition.Animations)
						remapAnimationResources(animation);
				for (auto& state : group.States)
				{
					for (auto& setter : state.Setters)
						if (setter.UsesResource)
						{
							const auto mapped = styleResourceMap.find(
								IdentityKey(setter.ResourceKey));
							if (mapped != styleResourceMap.end())
								setter.ResourceKey = mapped->second;
						}
					for (auto& animation : state.Animations)
						remapAnimationResources(animation);
				}
			}
			for (auto& trigger : imported->EventTriggers)
				for (auto& action : trigger.Actions)
					for (auto& animation : action.Animations)
						remapAnimationResources(animation);
		}

		// Conventional handler names carry the source control identity. Build
		// this map from handlers that are actually owned by a copied node, then
		// apply it to every copied reference. This keeps deliberate sharing
		// within a pasted subtree intact while custom/external handler names stay
		// untouched.
		std::unordered_map<std::wstring, std::wstring> handlerNameMap;
		std::unordered_set<std::wstring> ambiguousHandlerNames;
		for (const auto& node : fragment.Nodes)
		{
			const auto& newName = nameMap.at(node.Name);
			for (const auto& [eventName, handlerValue]
				: node.Events)
			{
				auto event = eventName;
				DesignerComponentType attachedOwner;
				std::wstring attachedEvent;
				if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
					event, attachedOwner, attachedEvent))
					event = std::move(attachedEvent);
				const auto& storedHandler = handlerValue;
				const auto conventionalName =
					DesignerEventCatalog::MakeDefaultHandlerName(
						node.Name, event);
				if (DesignerEventCatalog::NormalizeHandlerName(storedHandler)
					!= conventionalName
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
			RewriteNodeElementReferences(node, nameMap);
			RewriteNodeItemTemplate(node, templateKeyMap);
			RewriteNodeDataList(node, dataListKeyMap);
			RewriteNodeItemsPanel(node, panelKeyMap);
			RewriteNodeGroupStyle(node, groupStyleKeyMap);
			RewriteNodePropertyResources(
				node, styleResourceMap, &original, &ownership);
			const auto isolatedStyle = isolatedStyles.find(original.Id);
			if (isolatedStyle != isolatedStyles.end())
				ApplyIsolatedNodeStyle(node, isolatedStyle->second);
			const auto isolatedContainerStyle =
				isolatedContainerStyles.find(original.Id);
			if (isolatedContainerStyle != isolatedContainerStyles.end())
				node.Structure.ItemContainerStyle =
					isolatedContainerStyle->second;
			for (auto& [eventName, handlerValue] : node.Events)
				{
					(void)eventName;
					const auto found = handlerNameMap.find(
						handlerValue);
					if (found != handlerNameMap.end())
						handlerValue = found->second;
				}
			if (rootIndices.contains(index))
			{
				const auto& destination = *targetByRoot.at(original.Id);
				if (node.Structure.ChildRole == DesignNodeChildRole::Header)
					node.Structure.ChildRole = DesignNodeChildRole::Default;
				node.ComponentContentProperty =
					contentPropertyByRoot.at(original.Id);
				node.PresentedComponentContent.clear();
				node.ParentId = destination.ParentId;
				if (destination.ParentId > 0)
				{
					const auto* resolved = targetGraph.FindById(
						destination.ParentId);
					node.ParentRef = target.Nodes[
						resolved->SourceIndex].Name;
				}
				else node.ParentRef = destination.ParentRef;
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
