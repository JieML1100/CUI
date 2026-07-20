#include "DesignDocument.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/GroupStyle.h"
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace DesignerModel
{
bool DesignFormModel::operator==(const DesignFormModel& other) const
{
	return Name == other.Name
		&& Text == other.Text
		&& FontName == other.FontName
		&& FontSize == other.FontSize
		&& Size.cx == other.Size.cx
		&& Size.cy == other.Size.cy
		&& Location.x == other.Location.x
		&& Location.y == other.Location.y
		&& BackColor.r == other.BackColor.r
		&& BackColor.g == other.BackColor.g
		&& BackColor.b == other.BackColor.b
		&& BackColor.a == other.BackColor.a
		&& ForeColor.r == other.ForeColor.r
		&& ForeColor.g == other.ForeColor.g
		&& ForeColor.b == other.ForeColor.b
		&& ForeColor.a == other.ForeColor.a
		&& ShowInTaskBar == other.ShowInTaskBar
		&& TopMost == other.TopMost
		&& Enable == other.Enable
		&& Visible == other.Visible
		&& VisibleHead == other.VisibleHead
		&& HeadHeight == other.HeadHeight
		&& MinBox == other.MinBox
		&& MaxBox == other.MaxBox
		&& CloseBox == other.CloseBox
		&& CenterTitle == other.CenterTitle
		&& AllowResize == other.AllowResize
		&& EventHandlers == other.EventHandlers;
}

bool DesignCodeBehindModel::TryNormalizeRelativeBasePath(
	const std::wstring& value,
	std::wstring& normalized,
	std::wstring* outError)
{
	normalized.clear();
	if (outError) outError->clear();
	if (value.empty()) return true;
	try
	{
		const std::filesystem::path input(value);
		if (input.empty() || input.is_absolute()
			|| input.has_root_name() || input.has_root_directory())
		{
			if (outError) *outError = L"代码导出关联必须是相对于设计文件的路径。";
			return false;
		}
		const auto path = input.lexically_normal();
		const auto fileName = path.filename().wstring();
		if (fileName.empty() || fileName == L"." || fileName == L"..")
		{
			if (outError) *outError = L"代码导出关联必须包含文件基名。";
			return false;
		}
		if (path.has_extension())
		{
			if (outError) *outError = L"代码导出关联应为不带 .h/.cpp 扩展名的基路径。";
			return false;
		}
		normalized = path.generic_wstring();
		return true;
	}
	catch (...)
	{
		if (outError) *outError = L"代码导出关联路径无效。";
		return false;
	}
}

bool DesignCodeBehindModel::TryNormalizeClassName(
	const std::wstring& value,
	std::wstring& normalized,
	std::wstring* outError)
{
	normalized.clear();
	if (outError) outError->clear();
	if (value.empty()) return true;
	std::vector<std::wstring> segments;
	size_t begin = 0;
	for (size_t position = 0; position <= value.size();)
	{
		const bool end = position == value.size();
		const bool dotted = !end && value[position] == L'.';
		const bool qualified = !end && value[position] == L':'
			&& position + 1 < value.size() && value[position + 1] == L':';
		if (!end && !dotted && !qualified)
		{
			if (value[position] == L':')
			{
				if (outError) *outError = L"x:Class 中的命名空间分隔符必须是 :: 或 .。";
				return false;
			}
			++position;
			continue;
		}
		auto segment = value.substr(begin, position - begin);
		if (segment.empty())
		{
			if (outError) *outError = L"x:Class 不能包含空命名空间段。";
			return false;
		}
		std::wstring validation;
		if (!DesignerEventCatalog::ValidateHandlerName(segment, &validation))
		{
			if (outError) *outError = L"x:Class 段 “" + segment
				+ L"” 无效：" + validation;
			return false;
		}
		segments.push_back(std::move(segment));
		if (end) break;
		position += qualified ? 2 : 1;
		begin = position;
	}
	for (const auto& segment : segments)
	{
		if (!normalized.empty()) normalized += L"::";
		normalized += segment;
	}
	return true;
}

bool DesignCodeBehindModel::Validate(std::wstring* outError) const
{
	if (outError) outError->clear();
	if (ClassName.empty())
	{
		if (RelativeBasePath.empty()) return true;
		if (outError) *outError = L"代码导出关联缺少 x:Class 类名。";
		return false;
	}
	std::wstring normalizedClass;
	if (!TryNormalizeClassName(ClassName, normalizedClass, outError)) return false;
	std::wstring normalized;
	return TryNormalizeRelativeBasePath(
		RelativeBasePath, normalized, outError);
}

bool DesignCodeBehindModel::operator==(
	const DesignCodeBehindModel& other) const
{
	return ClassName == other.ClassName
		&& RelativeBasePath == other.RelativeBasePath;
}

DesignObjectResourceDictionary::DesignObjectResourceDictionary() = default;
DesignObjectResourceDictionary::~DesignObjectResourceDictionary() = default;
DesignObjectResourceDictionary::DesignObjectResourceDictionary(
	const DesignObjectResourceDictionary&) = default;
DesignObjectResourceDictionary::DesignObjectResourceDictionary(
	DesignObjectResourceDictionary&&) noexcept = default;
DesignObjectResourceDictionary& DesignObjectResourceDictionary::operator=(
	const DesignObjectResourceDictionary&) = default;
DesignObjectResourceDictionary& DesignObjectResourceDictionary::operator=(
	DesignObjectResourceDictionary&&) noexcept = default;

bool DesignObjectResourceDictionary::Empty() const noexcept
{
	return Components.empty() && ControlTemplates.empty() && DataTemplates.empty()
		&& ItemsPanelTemplates.empty() && GroupStyles.empty();
}

bool DesignObjectResourceDictionary::operator==(
	const DesignObjectResourceDictionary& other) const
{
	return Components == other.Components
		&& ControlTemplates == other.ControlTemplates
		&& DataTemplates == other.DataTemplates
		&& ItemsPanelTemplates == other.ItemsPanelTemplates
		&& GroupStyles == other.GroupStyles;
}

bool DesignNode::operator==(const DesignNode& other) const
{
	return Id == other.Id
		&& ParentId == other.ParentId
		&& ParentRef == other.ParentRef
		&& Name == other.Name
		&& Type == other.Type
		&& ComponentType == other.ComponentType
		&& ComponentContentProperty == other.ComponentContentProperty
		&& PresentedComponentContent == other.PresentedComponentContent
		&& TemplateContentSource == other.TemplateContentSource
		&& Order == other.Order
		&& Locked == other.Locked
		&& Props == other.Props
		&& Extra == other.Extra
		&& Events == other.Events
		&& Bindings == other.Bindings
		&& LocalResources == other.LocalResources
		&& LocalObjectResources == other.LocalObjectResources
		&& TemplateBindings == other.TemplateBindings
		&& TemplateEventBindings == other.TemplateEventBindings;
}

bool DesignDataTemplate::HasSameResourceIdentity(
	const DesignDataTemplate& other) const noexcept
{
	if (IsImplicit() != other.IsImplicit()) return false;
	const auto& left = IsImplicit() ? DataType : Key;
	const auto& right = other.IsImplicit() ? other.DataType : other.Key;
	return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

std::wstring DesignDataTemplate::DisplayName() const
{
	return IsImplicit() ? L"{DataType " + DataType + L"}" : Key;
}

bool DesignControlTemplate::HasSameResourceIdentity(
	const DesignControlTemplate& other) const noexcept
{
	if (IsImplicit() != other.IsImplicit()) return false;
	if (IsImplicit())
	{
		if (TargetComponentType.Empty() != other.TargetComponentType.Empty())
			return false;
		return TargetComponentType.Empty()
			? TargetType == other.TargetType
			: TargetComponentType.RegistryKey()
				== other.TargetComponentType.RegistryKey();
	}
	return _wcsicmp(Key.c_str(), other.Key.c_str()) == 0;
}

std::wstring DesignControlTemplate::DisplayName() const
{
	return IsImplicit()
		? L"{TargetType " + (TargetComponentType.Empty()
			? DesignerStyleSheetUtils::UIClassName(TargetType)
			: TargetComponentType.XamlPrefix + L":"
				+ TargetComponentType.XamlName) + L"}"
		: Key;
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const DesignerComponentType& type) const
{
	return FindComponent(type.XamlNamespace, type.XamlName);
}

DesignObjectResourceDictionary DesignDocument::VisibleObjectResources(
	const DesignNode& origin) const
{
	return VisibleObjectResources(Nodes, origin);
}

DesignObjectResourceDictionary DesignDocument::VisibleObjectResources(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin) const
{
	DesignObjectResourceDictionary result;
	auto append = [&](const DesignObjectResourceDictionary& source)
	{
		for (const auto& component : source.Components)
		{
			result.Components.erase(std::remove_if(
				result.Components.begin(), result.Components.end(),
				[&](const auto& current)
				{
					return _wcsicmp(current.Type.XamlNamespace.c_str(),
						component.Type.XamlNamespace.c_str()) == 0
						&& _wcsicmp(current.Type.XamlName.c_str(),
							component.Type.XamlName.c_str()) == 0;
				}), result.Components.end());
			result.Components.push_back(component);
		}
		for (const auto& dataTemplate : source.DataTemplates)
		{
			result.DataTemplates.erase(std::remove_if(
				result.DataTemplates.begin(), result.DataTemplates.end(),
				[&](const auto& current)
				{ return current.HasSameResourceIdentity(dataTemplate); }),
				result.DataTemplates.end());
			result.DataTemplates.push_back(dataTemplate);
		}
		for (const auto& controlTemplate : source.ControlTemplates)
		{
			result.ControlTemplates.erase(std::remove_if(
				result.ControlTemplates.begin(), result.ControlTemplates.end(),
				[&](const auto& current)
				{ return current.HasSameResourceIdentity(controlTemplate); }),
				result.ControlTemplates.end());
			result.ControlTemplates.push_back(controlTemplate);
		}
		for (const auto& itemsPanel : source.ItemsPanelTemplates)
		{
			result.ItemsPanelTemplates.erase(std::remove_if(
				result.ItemsPanelTemplates.begin(),
				result.ItemsPanelTemplates.end(), [&](const auto& current)
				{ return _wcsicmp(current.Key.c_str(), itemsPanel.Key.c_str()) == 0; }),
				result.ItemsPanelTemplates.end());
			result.ItemsPanelTemplates.push_back(itemsPanel);
		}
		for (const auto& groupStyle : source.GroupStyles)
		{
			result.GroupStyles.erase(std::remove_if(
				result.GroupStyles.begin(), result.GroupStyles.end(),
				[&](const auto& current)
				{ return _wcsicmp(current.Key.c_str(), groupStyle.Key.c_str()) == 0; }),
				result.GroupStyles.end());
			result.GroupStyles.push_back(groupStyle);
		}
	};
	DesignObjectResourceDictionary documentResources;
	documentResources.Components = Components;
	documentResources.ControlTemplates = ControlTemplates;
	documentResources.DataTemplates = DataTemplates;
	documentResources.ItemsPanelTemplates = ItemsPanelTemplates;
	documentResources.GroupStyles = GroupStyles;
	append(documentResources);

	std::unordered_map<int, const DesignNode*> byId;
	std::unordered_map<std::wstring, const DesignNode*> byName;
	for (const auto& node : scopeNodes)
	{
		byId.emplace(node.Id, &node);
		byName.emplace(node.Name, &node);
	}
	std::vector<const DesignNode*> route;
	std::unordered_set<int> visited;
	for (const DesignNode* node = &origin;
		node && visited.insert(node->Id).second;)
	{
		route.push_back(node);
		if (node->ParentId > 0)
		{
			const auto parent = byId.find(node->ParentId);
			node = parent == byId.end() ? nullptr : parent->second;
			continue;
		}
		if (node->ParentRef.empty()) break;
		auto parentName = node->ParentRef;
		if (const auto page = parentName.find(L"#page");
			page != std::wstring::npos) parentName.resize(page);
		const auto parent = byName.find(parentName);
		node = parent == byName.end() ? nullptr : parent->second;
	}
	for (auto node = route.rbegin(); node != route.rend(); ++node)
		append((*node)->LocalObjectResources);
	return result;
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const DesignNode& origin,
	const DesignerComponentType& type) const
{
	return FindComponent(Nodes, origin, type);
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin,
	const DesignerComponentType& type) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope;)
	{
		const auto local = std::find_if(
			scope->LocalObjectResources.Components.rbegin(),
			scope->LocalObjectResources.Components.rend(), [&](const auto& component)
			{
				return _wcsicmp(component.Type.XamlNamespace.c_str(),
					type.XamlNamespace.c_str()) == 0
					&& _wcsicmp(component.Type.XamlName.c_str(),
						type.XamlName.c_str()) == 0;
			});
		if (local != scope->LocalObjectResources.Components.rend()) return &*local;
		scope = parentOf(*scope);
	}
	return FindComponent(type);
}

const DesignComponentDefinition* DesignDocument::FindComponent(
	const std::wstring& xamlNamespace,
	const std::wstring& xamlName) const
{
	const auto found = std::find_if(Components.begin(), Components.end(),
		[&](const DesignComponentDefinition& component)
		{
			return _wcsicmp(component.Type.XamlNamespace.c_str(),
				xamlNamespace.c_str()) == 0
				&& _wcsicmp(component.Type.XamlName.c_str(),
					xamlName.c_str()) == 0;
		});
	return found == Components.end() ? nullptr : &*found;
}

bool DesignDocument::HasResourceBackedVisualStates() const noexcept
{
	auto componentUsesResources = [](const auto& component)
	{
		auto animationUsesResource = [](const auto& animation)
		{
			return (animation.HasTo && animation.ToUsesResource)
				|| (animation.HasFrom && animation.FromUsesResource)
				|| (animation.HasBy && animation.ByUsesResource)
				|| std::any_of(animation.KeyFrames.begin(),
					animation.KeyFrames.end(), [](const auto& keyFrame)
					{ return keyFrame.UsesResource; });
		};
		return std::any_of(component.VisualStateGroups.begin(),
				component.VisualStateGroups.end(), [&](const auto& group)
				{
					return std::any_of(group.Transitions.begin(),
						group.Transitions.end(), [&](const auto& transition)
						{
							return std::any_of(transition.Animations.begin(),
								transition.Animations.end(), animationUsesResource);
						}) || std::any_of(group.States.begin(), group.States.end(),
						[&](const auto& state)
						{
							return std::any_of(state.Setters.begin(), state.Setters.end(),
								[](const auto& setter) { return setter.UsesResource; })
								|| std::any_of(state.Animations.begin(), state.Animations.end(),
									animationUsesResource);
						});
				}) || std::any_of(component.EventTriggers.begin(),
				component.EventTriggers.end(), [&](const auto& trigger)
				{
					return std::any_of(trigger.Actions.begin(),
						trigger.Actions.end(), [&](const auto& action)
						{
							return std::any_of(action.Animations.begin(),
								action.Animations.end(), animationUsesResource);
						});
				});
	};
	if (std::any_of(Components.begin(), Components.end(),
		componentUsesResources)) return true;
	if (std::any_of(ControlTemplates.begin(), ControlTemplates.end(),
		componentUsesResources)) return true;
	auto nodesUseResources = [&](const auto& self,
		const std::vector<DesignNode>& nodes) -> bool
	{
		for (const auto& node : nodes)
		{
			for (const auto& component
				: node.LocalObjectResources.Components)
				if (componentUsesResources(component)
					|| self(self, component.Template)) return true;
			for (const auto& dataTemplate
				: node.LocalObjectResources.DataTemplates)
				if (self(self, dataTemplate.Template)) return true;
			for (const auto& controlTemplate
				: node.LocalObjectResources.ControlTemplates)
				if (componentUsesResources(controlTemplate)
					|| self(self, controlTemplate.Template)) return true;
		}
		return false;
	};
	if (nodesUseResources(nodesUseResources, Nodes)) return true;
	for (const auto& component : Components)
		if (nodesUseResources(nodesUseResources, component.Template)) return true;
	for (const auto& dataTemplate : DataTemplates)
		if (nodesUseResources(nodesUseResources, dataTemplate.Template)) return true;
	for (const auto& controlTemplate : ControlTemplates)
		if (nodesUseResources(nodesUseResources, controlTemplate.Template)) return true;
	return false;
}

const DesignDataTypeDefinition* DesignDocument::FindDataType(
	const std::wstring& name) const
{
	const auto found = std::find_if(DataTypes.begin(), DataTypes.end(),
		[&](const DesignDataTypeDefinition& type)
		{
			return _wcsicmp(type.Name.c_str(), name.c_str()) == 0;
		});
	return found == DataTypes.end() ? nullptr : &*found;
}

const DesignDataTemplate* DesignDocument::FindDataTemplate(
	const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	const auto found = std::find_if(DataTemplates.begin(), DataTemplates.end(),
		[&](const DesignDataTemplate& item)
		{
			return _wcsicmp(item.Key.c_str(), key.c_str()) == 0;
		});
	return found == DataTemplates.end() ? nullptr : &*found;
}

const DesignDataTemplate* DesignDocument::FindDataTemplate(
	const DesignNode& origin,
	const std::wstring& key) const
{
	return FindDataTemplate(Nodes, origin, key);
}

const DesignDataTemplate* DesignDocument::FindDataTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin,
	const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope;)
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.DataTemplates.rbegin(),
			scope->LocalObjectResources.DataTemplates.rend(),
			[&](const auto& item)
			{ return _wcsicmp(item.Key.c_str(), key.c_str()) == 0; });
		if (found != scope->LocalObjectResources.DataTemplates.rend()) return &*found;
		scope = parentOf(*scope);
	}
	return FindDataTemplate(key);
}

const DesignDataTemplate* DesignDocument::FindImplicitDataTemplate(
	const std::wstring& dataType) const
{
	if (dataType.empty()) return nullptr;
	const auto found = std::find_if(DataTemplates.rbegin(), DataTemplates.rend(),
		[&](const DesignDataTemplate& item)
		{
			return item.IsImplicit()
				&& _wcsicmp(item.DataType.c_str(), dataType.c_str()) == 0;
		});
	return found == DataTemplates.rend() ? nullptr : &*found;
}

const DesignDataTemplate* DesignDocument::FindImplicitDataTemplate(
	const DesignNode& origin, const std::wstring& dataType) const
{
	return FindImplicitDataTemplate(Nodes, origin, dataType);
}

const DesignDataTemplate* DesignDocument::FindImplicitDataTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& dataType) const
{
	if (dataType.empty()) return nullptr;
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.DataTemplates.rbegin(),
			scope->LocalObjectResources.DataTemplates.rend(),
			[&](const auto& item)
			{
				return item.IsImplicit()
					&& _wcsicmp(item.DataType.c_str(), dataType.c_str()) == 0;
			});
		if (found != scope->LocalObjectResources.DataTemplates.rend()) return &*found;
	}
	return FindImplicitDataTemplate(dataType);
}

const DesignControlTemplate* DesignDocument::FindControlTemplate(
	const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	const auto found = std::find_if(
		ControlTemplates.rbegin(), ControlTemplates.rend(),
		[&](const DesignControlTemplate& item)
		{ return !item.IsImplicit()
			&& _wcsicmp(item.Key.c_str(), key.c_str()) == 0; });
	return found == ControlTemplates.rend() ? nullptr : &*found;
}

const DesignControlTemplate* DesignDocument::FindControlTemplate(
	const DesignNode& origin, const std::wstring& key) const
{
	return FindControlTemplate(Nodes, origin, key);
}

const DesignControlTemplate* DesignDocument::FindControlTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	if (key.empty()) return nullptr;
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ControlTemplates.rbegin(),
			scope->LocalObjectResources.ControlTemplates.rend(),
			[&](const auto& item)
			{ return !item.IsImplicit()
				&& _wcsicmp(item.Key.c_str(), key.c_str()) == 0; });
		if (found != scope->LocalObjectResources.ControlTemplates.rend())
			return &*found;
	}
	return FindControlTemplate(key);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	UIClass targetType) const
{
	const auto found = std::find_if(
		ControlTemplates.rbegin(), ControlTemplates.rend(),
		[&](const DesignControlTemplate& item)
		{ return item.IsImplicit() && item.TargetComponentType.Empty()
			&& item.TargetType == targetType; });
	return found == ControlTemplates.rend() ? nullptr : &*found;
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const DesignerComponentType& targetType) const
{
	if (targetType.Empty()) return nullptr;
	const auto key = targetType.RegistryKey();
	const auto found = std::find_if(
		ControlTemplates.rbegin(), ControlTemplates.rend(),
		[&](const DesignControlTemplate& item)
		{ return item.IsImplicit() && !item.TargetComponentType.Empty()
			&& item.TargetComponentType.RegistryKey() == key; });
	return found == ControlTemplates.rend() ? nullptr : &*found;
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const DesignNode& origin, UIClass targetType) const
{
	return FindImplicitControlTemplate(Nodes, origin, targetType);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const DesignNode& origin,
	const DesignerComponentType& targetType) const
{
	return FindImplicitControlTemplate(Nodes, origin, targetType);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, UIClass targetType) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ControlTemplates.rbegin(),
			scope->LocalObjectResources.ControlTemplates.rend(),
			[&](const auto& item)
			{ return item.IsImplicit() && item.TargetComponentType.Empty()
				&& item.TargetType == targetType; });
		if (found != scope->LocalObjectResources.ControlTemplates.rend())
			return &*found;
	}
	return FindImplicitControlTemplate(targetType);
}

const DesignControlTemplate* DesignDocument::FindImplicitControlTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin,
	const DesignerComponentType& targetType) const
{
	if (targetType.Empty()) return nullptr;
	const auto key = targetType.RegistryKey();
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			if (found != scopeNodes.end()) return &*found;
		}
		if (!node.ParentRef.empty())
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate)
				{ return _wcsicmp(candidate.Name.c_str(), node.ParentRef.c_str()) == 0; });
			if (found != scopeNodes.end()) return &*found;
		}
		return nullptr;
	};
	std::unordered_set<int> visited;
	for (const DesignNode* scope = &origin;
		scope && visited.insert(scope->Id).second; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ControlTemplates.rbegin(),
			scope->LocalObjectResources.ControlTemplates.rend(),
			[&](const auto& item)
			{ return item.IsImplicit() && !item.TargetComponentType.Empty()
				&& item.TargetComponentType.RegistryKey() == key; });
		if (found != scope->LocalObjectResources.ControlTemplates.rend())
			return &*found;
	}
	return FindImplicitControlTemplate(targetType);
}

const DesignItemsPanelTemplate* DesignDocument::FindItemsPanelTemplate(
	const std::wstring& key) const
{
	const auto found = std::find_if(
		ItemsPanelTemplates.begin(), ItemsPanelTemplates.end(),
		[&](const auto& item)
		{
			return _wcsicmp(item.Key.c_str(), key.c_str()) == 0;
		});
	return found == ItemsPanelTemplates.end() ? nullptr : &*found;
}

const DesignItemsPanelTemplate* DesignDocument::FindItemsPanelTemplate(
	const DesignNode& origin, const std::wstring& key) const
{
	return FindItemsPanelTemplate(Nodes, origin, key);
}

const DesignItemsPanelTemplate* DesignDocument::FindItemsPanelTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.ItemsPanelTemplates.rbegin(),
			scope->LocalObjectResources.ItemsPanelTemplates.rend(),
			[&](const auto& item)
			{ return _wcsicmp(item.Key.c_str(), key.c_str()) == 0; });
		if (found != scope->LocalObjectResources.ItemsPanelTemplates.rend())
			return &*found;
	}
	return FindItemsPanelTemplate(key);
}

const DesignGroupStyle* DesignDocument::FindGroupStyle(
	const std::wstring& key) const
{
	const auto found = std::find_if(GroupStyles.begin(), GroupStyles.end(),
		[&](const DesignGroupStyle& item)
		{
			return _wcsicmp(item.Key.c_str(), key.c_str()) == 0;
		});
	return found == GroupStyles.end() ? nullptr : &*found;
}

const DesignGroupStyle* DesignDocument::FindGroupStyle(
	const DesignNode& origin, const std::wstring& key) const
{
	return FindGroupStyle(Nodes, origin, key);
}

const DesignGroupStyle* DesignDocument::FindGroupStyle(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
	{
		const auto found = std::find_if(
			scope->LocalObjectResources.GroupStyles.rbegin(),
			scope->LocalObjectResources.GroupStyles.rend(),
			[&](const auto& item)
			{ return _wcsicmp(item.Key.c_str(), key.c_str()) == 0; });
		if (found != scope->LocalObjectResources.GroupStyles.rend()) return &*found;
	}
	return FindGroupStyle(key);
}

const DesignDataTemplate* DesignDocument::FindGroupStyleHeaderTemplate(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& groupStyleKey) const
{
	if (const auto* scope = FindLocalGroupStyleOwner(
		scopeNodes, origin, groupStyleKey))
	{
		const auto style = std::find_if(
			scope->LocalObjectResources.GroupStyles.rbegin(),
			scope->LocalObjectResources.GroupStyles.rend(), [&](const auto& item)
			{ return _wcsicmp(item.Key.c_str(), groupStyleKey.c_str()) == 0; });
		if (style == scope->LocalObjectResources.GroupStyles.rend()) return nullptr;
		return style->HeaderTemplate.empty()
			? FindImplicitDataTemplate(scopeNodes, *scope,
				std::wstring(CollectionViewGroupDataTypeName))
			: FindDataTemplate(scopeNodes, *scope, style->HeaderTemplate);
	}
	const auto* style = FindGroupStyle(groupStyleKey);
	if (!style) return nullptr;
	return style->HeaderTemplate.empty()
		? FindImplicitDataTemplate(std::wstring(CollectionViewGroupDataTypeName))
		: FindDataTemplate(style->HeaderTemplate);
}

const DesignNode* DesignDocument::FindLocalGroupStyleOwner(
	const std::vector<DesignNode>& scopeNodes,
	const DesignNode& origin, const std::wstring& key) const
{
	auto parentOf = [&](const DesignNode& node) -> const DesignNode*
	{
		if (node.ParentId > 0)
		{
			const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
				[&](const auto& candidate) { return candidate.Id == node.ParentId; });
			return found == scopeNodes.end() ? nullptr : &*found;
		}
		if (node.ParentRef.empty()) return nullptr;
		auto name = node.ParentRef;
		if (const auto page = name.find(L"#page"); page != std::wstring::npos)
			name.resize(page);
		const auto found = std::find_if(scopeNodes.begin(), scopeNodes.end(),
			[&](const auto& candidate)
			{ return _wcsicmp(candidate.Name.c_str(), name.c_str()) == 0; });
		return found == scopeNodes.end() ? nullptr : &*found;
	};
	for (const DesignNode* scope = &origin; scope; scope = parentOf(*scope))
		if (std::any_of(
			scope->LocalObjectResources.GroupStyles.rbegin(),
			scope->LocalObjectResources.GroupStyles.rend(), [&](const auto& item)
			{ return _wcsicmp(item.Key.c_str(), key.c_str()) == 0; })) return scope;
	return nullptr;
}

const DesignDataList* DesignDocument::FindDataList(
	const std::wstring& key) const
{
	const auto found = std::find_if(DataLists.begin(), DataLists.end(),
		[&](const DesignDataList& item)
		{
			return _wcsicmp(item.Key.c_str(), key.c_str()) == 0;
		});
	return found == DataLists.end() ? nullptr : &*found;
}

const DesignCollectionViewSource* DesignDocument::FindCollectionView(
	const std::wstring& key) const
{
	const auto found = std::find_if(
		CollectionViews.begin(), CollectionViews.end(),
		[&](const DesignCollectionViewSource& item)
		{
			return _wcsicmp(item.Key.c_str(), key.c_str()) == 0;
		});
	return found == CollectionViews.end() ? nullptr : &*found;
}

int DesignDocument::AllocateNodeId()
{
	if (NextStableId < 1
		|| NextStableId == (std::numeric_limits<int>::max)())
		throw std::overflow_error("Designer stable node id space exhausted");
	return NextStableId++;
}

void DesignDocument::RecalculateNextStableId()
{
	int maxId = 0;
	for (const auto& node : Nodes)
	{
		maxId = (std::max)(maxId, node.Id);
	}
	if (maxId == (std::numeric_limits<int>::max)())
		throw std::overflow_error("Designer stable node id space exhausted");
	NextStableId = (std::max)(1, maxId + 1);
}

void DesignDocument::Clear()
{
	*this = DesignDocument();
}

bool DesignDocument::operator==(const DesignDocument& other) const
{
	return Schema == other.Schema
		&& SchemaVersion == other.SchemaVersion
		&& NextStableId == other.NextStableId
		&& Form == other.Form
		&& CodeBehind == other.CodeBehind
		&& DataContextSchema == other.DataContextSchema
		&& StyleSheet == other.StyleSheet
		&& Components == other.Components
		&& ControlTemplates == other.ControlTemplates
		&& DataTypes == other.DataTypes
		&& DataTemplates == other.DataTemplates
		&& ItemsPanelTemplates == other.ItemsPanelTemplates
		&& GroupStyles == other.GroupStyles
		&& DataLists == other.DataLists
		&& CollectionViews == other.CollectionViews
		&& Nodes == other.Nodes;
}
}
