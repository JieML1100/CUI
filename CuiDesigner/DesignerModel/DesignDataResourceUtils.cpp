#include "DesignDataResourceUtils.h"
#include "DesignDocumentGraph.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerStyleSheetUtils.h"
#include <GroupStyle.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <functional>
#include <optional>
#include <unordered_set>

namespace DesignerModel::DesignDataResourceUtils
{
namespace
{
	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	bool IsIdentifier(const std::wstring& value)
	{
		if (value.empty() || !(std::iswalpha(value.front())
			|| value.front() == L'_')) return false;
		return std::all_of(value.begin() + 1, value.end(), [](wchar_t ch)
		{
			return std::iswalnum(ch) || ch == L'_';
		});
	}

	bool IsControlTemplateHostType(UIClass type) noexcept
	{
		return IsControlTemplateHostClass(type);
	}

	bool IsControlTemplateTargetCompatible(
		UIClass actual, UIClass target) noexcept
	{
		return IsUIClassAssignableFrom(target, actual);
	}

	bool IsControlTemplateTargetCompatible(
		const DesignNode& actual,
		const DesignControlTemplate& target) noexcept
	{
		if (!target.TargetComponentType.Empty())
			return !actual.ComponentType.Empty()
				&& actual.ComponentType.RegistryKey()
					== target.TargetComponentType.RegistryKey();
		return IsControlTemplateTargetCompatible(actual.Type, target.TargetType);
	}

	bool TryValueKind(
		BindingValueKind kind,
		DesignerStyleValueKind& result)
	{
		switch (kind)
		{
		case BindingValueKind::Bool: result = DesignerStyleValueKind::Bool; return true;
		case BindingValueKind::NullableBool:
			result = DesignerStyleValueKind::NullableBool;
			return true;
		case BindingValueKind::Int: result = DesignerStyleValueKind::Int; return true;
		case BindingValueKind::Int64: result = DesignerStyleValueKind::Int64; return true;
		case BindingValueKind::Float: result = DesignerStyleValueKind::Float; return true;
		case BindingValueKind::Double: result = DesignerStyleValueKind::Double; return true;
		case BindingValueKind::String: result = DesignerStyleValueKind::String; return true;
		default: return false;
		}
	}

	BindingValue DefaultValue(BindingValueKind kind)
	{
		switch (kind)
		{
		case BindingValueKind::Bool: return BindingValue(false);
		case BindingValueKind::NullableBool:
			return BindingValue(NullableBool{});
		case BindingValueKind::Int: return BindingValue(0);
		case BindingValueKind::Int64: return BindingValue(static_cast<long long>(0));
		case BindingValueKind::Float: return BindingValue(0.0f);
		case BindingValueKind::Double: return BindingValue(0.0);
		case BindingValueKind::String: return BindingValue(std::wstring{});
		case BindingValueKind::Empty: return BindingValue(std::wstring{});
		default: return {};
		}
	}

	std::wstring ParentPath(const std::wstring& path)
	{
		const auto separator = path.rfind(L'.');
		return separator == std::wstring::npos ? L"" : path.substr(0, separator);
	}

	std::wstring LeafName(const std::wstring& path)
	{
		const auto separator = path.rfind(L'.');
		return separator == std::wstring::npos ? path : path.substr(separator + 1);
	}
}

bool IsCollectionViewGroupDataType(const std::wstring& name)
{
	return DesignerBindingUtils::Trim(name)
		== CollectionViewGroupDataTypeName;
}

DesignerDataContextSchema BuildCollectionViewGroupSchema(
	const DesignDataTypeDefinition* itemType,
	const std::vector<DesignCollectionAggregateDescription>* aggregates)
{
	DesignerDataContextSchema result{
		{ L"Key", BindingValueKind::String, true, false, true },
		{ L"Name", BindingValueKind::String, true, false, true },
		{ L"PropertyName", BindingValueKind::String, true, false, true },
		{ L"Level", BindingValueKind::Int, true, false, true },
		{ L"StartIndex", BindingValueKind::Int64, true, false, true },
		{ L"ItemCount", BindingValueKind::Int64, true, false, true },
		{ L"IsBottomLevel", BindingValueKind::Bool, true, false, true },
		{ L"FirstItem", BindingValueKind::Object, true, false, true,
			DesignerDataObjectKind::BindingSource },
		{ L"Items", BindingValueKind::Object, true, false, true,
			DesignerDataObjectKind::BindingList,
			itemType ? itemType->Name : std::wstring{} },
		{ L"Aggregates", BindingValueKind::Object, true, false, true,
			DesignerDataObjectKind::BindingSource }
	};
	if (itemType)
		for (const auto& property : itemType->Properties)
		{
			auto nested = property;
			nested.Path = L"FirstItem." + nested.Path;
			result.push_back(std::move(nested));
		}
	if (aggregates)
		for (const auto& aggregate : *aggregates)
		{
			BindingValueKind kind = BindingValueKind::Empty;
			if (aggregate.Function == CollectionAggregateFunction::Count)
				kind = BindingValueKind::Int64;
			else if (aggregate.Function == CollectionAggregateFunction::Sum
				|| aggregate.Function == CollectionAggregateFunction::Average)
				kind = BindingValueKind::Double;
			else if (itemType)
				if (const auto* property = DesignerDataContextSchemaUtils::Find(
					itemType->Properties, aggregate.PropertyName))
					kind = property->ValueKind;
			result.push_back({ L"Aggregates." + aggregate.Name, kind,
				true, false, true });
		}
	DesignerDataContextSchemaUtils::Canonicalize(result);
	return result;
}

bool ValidateAndCanonicalize(
	DesignDocument& document,
	std::wstring* outError,
	const DesignDocument* fallbackResources)
{
	DesignerDataContextSchemaUtils::Canonicalize(document.DataContextSchema);
	std::wstring rootSchemaError;
	if (!DesignerDataContextSchemaUtils::Validate(
		document.DataContextSchema, &rootSchemaError))
		return Fail(L"DataContext：" + rootSchemaError, outError);
	std::unordered_set<std::wstring> typeNames;
	for (auto& type : document.DataTypes)
	{
		type.Name = DesignerBindingUtils::Trim(type.Name);
		if (IsCollectionViewGroupDataType(type.Name))
			return Fail(L"DataType 名称 CollectionViewGroup 由运行时保留。", outError);
		if (!IsIdentifier(type.Name) || !typeNames.insert(type.Name).second)
			return Fail(L"DataType 名称无效或重复：" + type.Name, outError);
		DesignerDataContextSchemaUtils::Canonicalize(type.Properties);
		std::wstring schemaError;
		if (type.Properties.empty()
			|| !DesignerDataContextSchemaUtils::Validate(
				type.Properties, &schemaError))
			return Fail(type.Properties.empty()
				? L"DataType " + type.Name + L" 至少需要一个属性。"
				: L"DataType " + type.Name + L"：" + schemaError, outError);
	}

	auto validateReferencedTypes = [&](DesignerDataContextSchema& schema,
		const std::wstring& owner)
	{
		for (auto& property : schema)
		{
			if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			{
				const auto* itemType = document.FindDataType(property.ItemType);
				if (!itemType)
					return Fail(owner + L" 引用了未声明的 ItemType："
						+ property.ItemType, outError);
				property.ItemType = itemType->Name;
			}
			else if (property.ObjectKind == DesignerDataObjectKind::BindingSource
				&& !property.DataType.empty())
			{
				const auto* dataType = document.FindDataType(property.DataType);
				if (!dataType)
					return Fail(owner + L" 引用了未声明的 DataType："
						+ property.DataType, outError);
				property.DataType = dataType->Name;
			}
		}
		return true;
	};
	if (!validateReferencedTypes(document.DataContextSchema, L"DataContext"))
		return false;
	for (auto& type : document.DataTypes)
		if (!validateReferencedTypes(type.Properties, L"DataType " + type.Name))
			return false;

	std::unordered_set<std::wstring> resourceKeys;
	for (const auto& type : document.DataTypes)
		resourceKeys.insert(type.Name);
	for (const auto& resource : document.StyleSheet.Resources)
		if (!resource.Key.empty()
			&& !resourceKeys.insert(resource.Key).second)
			return Fail(L"资源键重复：" + resource.Key, outError);
	std::unordered_set<std::wstring> panelTemplateKeys;
	for (auto& item : document.ItemsPanelTemplates)
	{
		item.Key = DesignerBindingUtils::Trim(item.Key);
		if (!IsIdentifier(item.Key)
			|| !panelTemplateKeys.insert(item.Key).second)
			return Fail(L"ItemsPanelTemplate 键无效或重复：" + item.Key,
				outError);
		if (!resourceKeys.insert(item.Key).second)
			return Fail(L"资源键重复：" + item.Key, outError);
		auto& value = item.Value;
		if (value.Kind != ItemsPanelKind::Stack
			&& value.Kind != ItemsPanelKind::Wrap
			&& value.Kind != ItemsPanelKind::VirtualizingStack)
			return Fail(L"ItemsPanelTemplate 面板类型无效：" + item.Key,
				outError);
		if (value.Orientation != Orientation::Horizontal
			&& value.Orientation != Orientation::Vertical)
			return Fail(L"ItemsPanelTemplate Orientation 无效：" + item.Key,
				outError);
		if (!std::isfinite(value.ItemWidth) || value.ItemWidth < 0.0f
			|| !std::isfinite(value.ItemHeight) || value.ItemHeight < 0.0f
			|| !std::isfinite(value.CacheLength) || value.CacheLength < 0.0f)
			return Fail(L"ItemsPanelTemplate 数值配置无效：" + item.Key,
				outError);
		if (value.Kind == ItemsPanelKind::VirtualizingStack
			&& (value.Orientation != Orientation::Vertical
				|| value.ItemHeight <= 0.0f))
			return Fail(L"VirtualizingStackPanel 只支持 Vertical，且必须声明正数 ItemHeight："
				+ item.Key, outError);
		if (value.Kind == ItemsPanelKind::Stack)
		{
			value.ItemWidth = 0.0f;
			value.ItemHeight = 0.0f;
			value.CacheLength = 1.0f;
		}
		else if (value.Kind == ItemsPanelKind::Wrap)
		{
			value.CacheLength = 1.0f;
		}
		else value.ItemWidth = 0.0f;
	}
	std::unordered_set<std::wstring> templateKeys;
	std::unordered_set<std::wstring> implicitTemplateTypes;
	auto validateHierarchicalTemplate = [&](DesignDataTemplate& item,
		const DesignerDataContextSchema& schema,
		const std::wstring& owner) -> bool
	{
		if (!item.Hierarchical)
		{
			if (item.ItemsSourceBinding)
				return Fail(owner + L" 的普通 DataTemplate 不能声明 ItemsSource。",
					outError);
			return true;
		}
		if (!item.ItemsSourceBinding) return true;
		const auto& binding = *item.ItemsSourceBinding;
		if (binding.IsMultiBinding() || !binding.ElementName.empty()
			|| binding.RelativeSource != DesignerBindingRelativeSource::None)
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource 必须以当前数据项为 Binding 源。",
				outError);
		if (binding.SourceProperty.empty()
			|| !DesignerBindingUtils::IsValidSourcePath(binding.SourceProperty))
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource Binding 路径无效。",
				outError);
		if (binding.Mode == BindingMode::TwoWay
			|| binding.Mode == BindingMode::OneWayToSource)
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource 只支持读取模式。",
				outError);
		if (binding.StringFormat || binding.FallbackValue
			|| binding.TargetNullValue)
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource 不能把列表格式化或替换为标量。",
				outError);
		const auto* property = DesignerDataContextSchemaUtils::Find(
			schema, binding.SourceProperty);
		if (!property || !property->CanRead)
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource 未声明为可读路径："
				+ binding.SourceProperty, outError);
		if (property->ObjectKind != DesignerDataObjectKind::BindingList)
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource 路径不是 BindingList："
				+ binding.SourceProperty, outError);
		if (property->ObjectKind == DesignerDataObjectKind::BindingList
			&& (property->ItemType.empty()
				|| !document.FindDataType(property->ItemType)))
			return Fail(owner
				+ L" 的 HierarchicalDataTemplate.ItemsSource ItemType 未声明："
				+ property->ItemType, outError);
		return true;
	};
	for (auto& item : document.DataTemplates)
	{
		item.Key = DesignerBindingUtils::Trim(item.Key);
		item.DataType = DesignerBindingUtils::Trim(item.DataType);
		if (item.IsImplicit())
		{
			if (!implicitTemplateTypes.insert(item.DataType).second)
				return Fail(L"隐式 DataTemplate DataType 重复："
					+ item.DataType, outError);
		}
		else
		{
			if (!IsIdentifier(item.Key)
				|| !templateKeys.insert(item.Key).second)
				return Fail(L"DataTemplate 键无效或重复：" + item.Key, outError);
			if (!resourceKeys.insert(item.Key).second)
				return Fail(L"资源键重复：" + item.Key, outError);
		}
		const auto* dataType = document.FindDataType(item.DataType);
		const bool groupType = IsCollectionViewGroupDataType(item.DataType);
		if (!dataType && !groupType)
			return Fail(L"DataTemplate " + item.DisplayName()
				+ L" 引用了未声明的 DataType：" + item.DataType, outError);
		item.DataType = dataType ? dataType->Name
			: std::wstring(CollectionViewGroupDataTypeName);
		const auto bindingSchema = dataType ? dataType->Properties
			: BuildCollectionViewGroupSchema();
		if (!validateHierarchicalTemplate(item, bindingSchema,
			L"DataTemplate " + item.DisplayName())) return false;
		if (item.Template.empty())
			return Fail(L"DataTemplate " + item.DisplayName()
				+ L" 没有视觉根。", outError);
		DesignDocument templateDocument;
		templateDocument.Nodes = item.Template;
		templateDocument.RecalculateNextStableId();
		DesignDocumentGraph graph;
		std::wstring graphError;
		if (!DesignDocumentGraph::Build(templateDocument, graph, &graphError)
			|| graph.Roots().size() != 1)
			return Fail(L"DataTemplate " + item.DisplayName()
				+ L" 必须包含一个有效视觉根：" + graphError, outError);
		auto projectTemplateSchema = [&](const std::wstring& prefix)
		{
			if (prefix.empty()) return bindingSchema;
			DesignerDataContextSchema result;
			const auto normalized = DesignerDataContextSchemaUtils::NormalizePath(prefix);
			const auto childPrefix = normalized + L".";
			for (const auto& property : bindingSchema)
			{
				const auto path = DesignerDataContextSchemaUtils::NormalizePath(
					property.Path);
				if (!path.starts_with(childPrefix)) continue;
				auto projected = property;
				projected.Path = path.substr(normalized.size() + 1);
				result.push_back(std::move(projected));
			}
			DesignerDataContextSchemaUtils::Canonicalize(result);
			return result;
		};
		std::unordered_map<std::wstring, std::optional<std::wstring>> templateContexts;
		std::unordered_map<std::wstring, std::optional<std::wstring>> templateInherited;
		for (const auto& resolved : graph.Nodes())
		{
			const auto& node = item.Template[resolved.SourceIndex];
			std::optional<std::wstring> inherited = resolved.ParentKey.empty()
				? std::optional<std::wstring>(std::wstring{})
				: templateContexts[resolved.ParentKey];
			templateInherited[node.Name] = inherited;
			auto effective = inherited;
			for (const auto& [target, binding] : node.Bindings)
			{
				if (target != L"DataContext") continue;
				if (binding.IsMultiBinding())
				{
					effective.reset();
					break;
				}
				if (inherited && binding.ElementName.empty()
					&& binding.RelativeSource == DesignerBindingRelativeSource::None)
				{
					const auto path = DesignerBindingUtils::Trim(
						binding.SourceProperty);
					effective = inherited->empty() ? path
						: *inherited + L"." + path;
				}
				else effective.reset();
				break;
			}
			templateContexts[node.Name] = effective;
		}
		for (const auto& node : item.Template)
		{
			if (!node.Events.empty())
				return Fail(L"DataTemplate " + item.DisplayName()
					+ L" 不允许代码后置事件。", outError);
			for (const auto& [target, binding] : node.Bindings)
			{
				const bool dataContextTarget = target == L"DataContext";
				const auto& prefix = dataContextTarget
					? templateInherited[node.Name] : templateContexts[node.Name];
				const auto scopedSchema = prefix
					? projectTemplateSchema(*prefix) : DesignerDataContextSchema{};
				std::wstring bindingError;
				if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
					binding, [&](const DesignerDataBinding& child)
					{
						if (!child.ElementName.empty()
							|| child.RelativeSource
								!= DesignerBindingRelativeSource::None)
							return true;
						const auto path = DesignerBindingUtils::Trim(
							child.SourceProperty);
						if (path.empty()
							|| DesignerDataContextSchemaUtils::Find(scopedSchema, path)
							|| (groupType && (path.starts_with(L"FirstItem.")
								|| path.starts_with(L"Aggregates."))
								&& DesignerDataContextSchemaUtils::IsValidPath(path))) return true;
						bindingError = L"DataTemplate " + item.DisplayName()
							+ L" 的绑定路径未在 DataType 中声明：" + path;
						return false;
					}))
					return Fail(bindingError, outError);
			}
		}
	}
	std::unordered_set<std::wstring> controlTemplateKeys;
	std::unordered_set<std::wstring> implicitControlTemplateTypes;
	for (auto& item : document.ControlTemplates)
	{
		item.Key = DesignerBindingUtils::Trim(item.Key);
		if (item.TargetComponentType.Empty())
		{
			if (!IsControlTemplateHostType(item.TargetType))
				return Fail(L"ControlTemplate TargetType 无效："
					+ item.DisplayName(), outError);
		}
		else
		{
			const auto* component = document.FindComponent(
				item.TargetComponentType);
			if (!component || component->BaseType != item.TargetType)
				return Fail(L"ControlTemplate TargetType 引用了不存在或"
					L"BaseType 不一致的组件：" + item.DisplayName(), outError);
			item.TargetComponentType = component->Type;
		}
		if (item.IsImplicit())
		{
			const auto identity = item.TargetComponentType.Empty()
				? L"builtin:" + std::to_wstring(static_cast<int>(item.TargetType))
				: L"component:" + item.TargetComponentType.RegistryKey();
			if (!implicitControlTemplateTypes.insert(identity).second)
				return Fail(L"隐式 ControlTemplate TargetType 重复："
					+ item.DisplayName(), outError);
		}
		else
		{
			if (!IsIdentifier(item.Key)
				|| !controlTemplateKeys.insert(item.Key).second)
				return Fail(L"ControlTemplate 键无效或重复："
					+ item.Key, outError);
			if (!resourceKeys.insert(item.Key).second)
				return Fail(L"资源键重复：" + item.Key, outError);
		}
		DesignDocument templateDocument;
		templateDocument.Nodes = item.Template;
		templateDocument.RecalculateNextStableId();
		DesignDocumentGraph graph;
		std::wstring graphError;
		if (item.Template.empty()
			|| !DesignDocumentGraph::Build(
				templateDocument, graph, &graphError)
			|| graph.Roots().size() != 1)
			return Fail(L"ControlTemplate " + item.DisplayName()
				+ L" 必须包含一个有效视觉根：" + graphError, outError);
	}
	auto validateTemplateSetters = [&](const DesignerStyleSheet& sheet,
		const std::vector<DesignNode>* nodes,
		const DesignNode* origin,
		const std::wstring& owner)
	{
		for (const auto& rule : sheet.Rules)
			for (const auto& setter : rule.Setters)
			{
				const bool controlTemplate =
					setter.PropertyName == L"Template";
				const bool itemsPanel =
					setter.PropertyName == L"ItemsPanel";
				if (!controlTemplate && !itemsPanel)
					continue;
				if (!setter.UsesResource || setter.UsesDynamicResource
					|| DesignerBindingUtils::Trim(setter.ResourceKey).empty())
					return Fail(owner
						+ L" 的 Style." + setter.PropertyName
						+ L" 必须使用 StaticResource。", outError);
				if (itemsPanel)
				{
					const auto* definition = nodes && origin
						? document.FindItemsPanelTemplate(
							*nodes, *origin, setter.ResourceKey)
						: document.FindItemsPanelTemplate(setter.ResourceKey);
					if (!definition)
						return Fail(owner
							+ L" 的 Style.ItemsPanel 引用了未声明的 "
							L"ItemsPanelTemplate：" + setter.ResourceKey,
							outError);
					if (rule.HasType && !IsUIClassAssignableFrom(
						UIClass::UI_ItemsControl, rule.Type))
						return Fail(owner
							+ L" 的 Style.ItemsPanel 只能用于 "
							L"ItemsControl 派生类型。", outError);
					continue;
				}
				const auto* definition = nodes && origin
					? document.FindControlTemplate(
						*nodes, *origin, setter.ResourceKey)
					: document.FindControlTemplate(setter.ResourceKey);
				if (!definition)
					return Fail(owner + L" 的 Style.Template 引用了未声明的 "
						L"ControlTemplate：" + setter.ResourceKey, outError);
				if (!rule.HasType) continue;
				DesignNode target;
				target.Type = rule.Type;
				target.ComponentType = rule.ComponentType;
				if (!IsControlTemplateTargetCompatible(target, *definition))
					return Fail(owner
						+ L" 的 Style.Template TargetType 与 Style TargetType 不兼容："
						+ definition->DisplayName(), outError);
			}
		return true;
	};
	if (!validateTemplateSetters(
		document.StyleSheet, nullptr, nullptr, L"文档资源")) return false;
	std::unordered_set<std::wstring> groupStyleKeys;
	for (auto& item : document.GroupStyles)
	{
		item.Key = DesignerBindingUtils::Trim(item.Key);
		item.HeaderTemplate = DesignerBindingUtils::Trim(item.HeaderTemplate);
		if (!IsIdentifier(item.Key)
			|| !groupStyleKeys.insert(item.Key).second)
			return Fail(L"GroupStyle 键无效或重复：" + item.Key, outError);
		if (!resourceKeys.insert(item.Key).second)
			return Fail(L"资源键重复：" + item.Key, outError);
		if (!item.HeaderTemplate.empty())
		{
			const auto* header = document.FindDataTemplate(item.HeaderTemplate);
			if (!header) return Fail(L"GroupStyle " + item.Key
				+ L" 引用了未声明的 DataTemplate：" + item.HeaderTemplate,
				outError);
			item.HeaderTemplate = header->Key;
			if (!IsCollectionViewGroupDataType(header->DataType))
				return Fail(L"GroupStyle " + item.Key
					+ L" 的 HeaderTemplate.DataType 必须为 CollectionViewGroup。",
					outError);
		}
	}

	std::unordered_set<std::wstring> listKeys;
	for (auto& list : document.DataLists)
	{
		list.Key = DesignerBindingUtils::Trim(list.Key);
		list.ItemType = DesignerBindingUtils::Trim(list.ItemType);
		if (!IsIdentifier(list.Key)
			|| !listKeys.insert(list.Key).second)
			return Fail(L"DataList 键无效或重复：" + list.Key, outError);
		if (!resourceKeys.insert(list.Key).second)
			return Fail(L"资源键重复：" + list.Key, outError);
		const auto* dataType = document.FindDataType(list.ItemType);
		if (!dataType)
			return Fail(L"DataList " + list.Key
				+ L" 引用了未声明的 DataType：" + list.ItemType, outError);
		list.ItemType = dataType->Name;
		for (auto& record : list.Records)
		{
			std::map<std::wstring, std::wstring> canonical;
			for (const auto& [rawPath, text] : record.Fields)
			{
				const auto path = DesignerDataContextSchemaUtils::NormalizePath(rawPath);
				const auto* property = DesignerDataContextSchemaUtils::Find(
					dataType->Properties, path);
				DesignerStyleValueKind kind{};
				if (!property || !TryValueKind(property->ValueKind, kind))
					return Fail(L"DataList " + list.Key
						+ L" 包含未知或非标量字段：" + path, outError);
				if (std::any_of(canonical.begin(), canonical.end(), [&](const auto& value)
					{ return value.first == property->Path; }))
					return Fail(L"DataList " + list.Key
						+ L" 包含重复字段：" + property->Path, outError);
				BindingValue converted;
				std::wstring conversionError;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					{ kind, text }, converted, &conversionError,
					document.ResourceBasePath, document.Resources))
					return Fail(L"DataList " + list.Key + L" 字段 "
						+ property->Path + L"：" + conversionError, outError);
				canonical.emplace(property->Path, text);
			}
			record.Fields = std::move(canonical);
		}
	}

	std::unordered_set<std::wstring> viewKeys;
	for (auto& view : document.CollectionViews)
	{
		view.Key = DesignerBindingUtils::Trim(view.Key);
		view.SourceResource = DesignerBindingUtils::Trim(view.SourceResource);
		view.SourceBindingPath = DesignerDataContextSchemaUtils::NormalizePath(
			view.SourceBindingPath);
		if (!IsIdentifier(view.Key)
			|| !viewKeys.insert(view.Key).second)
			return Fail(L"CollectionViewSource 键无效或重复：" + view.Key,
				outError);
		if (!resourceKeys.insert(view.Key).second)
			return Fail(L"资源键重复：" + view.Key, outError);
		if (view.SourceResource.empty() == view.SourceBindingPath.empty())
			return Fail(L"CollectionViewSource " + view.Key
				+ L" 必须且只能声明一种 Source。", outError);
	}
	std::map<std::wstring, std::wstring> viewItemTypes;
	std::unordered_set<std::wstring> resolvingViews;
	std::function<const DesignDataTypeDefinition*(DesignCollectionViewSource&)>
		resolveViewType;
	resolveViewType = [&](DesignCollectionViewSource& view)
		-> const DesignDataTypeDefinition*
	{
		const auto identity = view.Key;
		if (const auto cached = viewItemTypes.find(identity);
			cached != viewItemTypes.end()) return document.FindDataType(cached->second);
		if (!resolvingViews.insert(identity).second)
		{
			Fail(L"CollectionViewSource Source 存在循环引用：" + view.Key,
				outError);
			return nullptr;
		}
		const DesignDataTypeDefinition* itemType = nullptr;
		if (!view.SourceResource.empty())
		{
			if (const auto* list = document.FindDataList(view.SourceResource))
			{
				view.SourceResource = list->Key;
				itemType = document.FindDataType(list->ItemType);
			}
			else if (auto* sourceView = const_cast<DesignCollectionViewSource*>(
				document.FindCollectionView(view.SourceResource)))
			{
				view.SourceResource = sourceView->Key;
				itemType = resolveViewType(*sourceView);
			}
			else Fail(L"CollectionViewSource " + view.Key
				+ L" 引用了未声明的列表资源：" + view.SourceResource, outError);
		}
		else
		{
			const auto* property = DesignerDataContextSchemaUtils::Find(
				document.DataContextSchema, view.SourceBindingPath);
			if (!property || !property->CanRead
				|| property->ObjectKind != DesignerDataObjectKind::BindingList)
				Fail(L"CollectionViewSource " + view.Key
					+ L" 的 Source Binding 必须指向可读 BindingList："
					+ view.SourceBindingPath, outError);
			else
			{
				view.SourceBindingPath = property->Path;
				itemType = document.FindDataType(property->ItemType);
			}
		}
		resolvingViews.erase(identity);
		if (itemType) viewItemTypes.emplace(identity, itemType->Name);
		return itemType;
	};
	for (auto& view : document.CollectionViews)
	{
		const auto* itemType = resolveViewType(view);
		if (!itemType) return false;
		std::unordered_set<std::wstring> groupPaths;
		for (auto& group : view.GroupDescriptions)
		{
			if (group.Direction != CollectionSortDirection::Ascending
				&& group.Direction != CollectionSortDirection::Descending)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的分组方向无效。", outError);
			group.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
				group.PropertyName);
			const auto* property = DesignerDataContextSchemaUtils::Find(
				itemType->Properties, group.PropertyName);
			DesignerStyleValueKind kind{};
			if (!property || !property->CanRead
				|| !TryValueKind(property->ValueKind, kind))
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的分组路径不是可读标量：" + group.PropertyName,
					outError);
			if (!groupPaths.insert(property->Path).second)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 包含重复分组路径：" + property->Path, outError);
			group.PropertyName = property->Path;
		}
		if (!view.AggregateDescriptions.empty()
			&& view.GroupDescriptions.empty())
			return Fail(L"CollectionViewSource " + view.Key
				+ L" 的 AggregateDescriptions 需要至少一个 GroupDescription。",
				outError);
		std::unordered_set<std::wstring> aggregateNames;
		for (auto& aggregate : view.AggregateDescriptions)
		{
			if (aggregate.Function < CollectionAggregateFunction::Count
				|| aggregate.Function > CollectionAggregateFunction::Max)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的聚合函数无效。", outError);
			aggregate.Name = DesignerBindingUtils::Trim(aggregate.Name);
			aggregate.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
				aggregate.PropertyName);
			if (!IsIdentifier(aggregate.Name)
				|| !aggregateNames.insert(aggregate.Name).second)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的聚合名称无效或重复：" + aggregate.Name, outError);
			if (aggregate.Function == CollectionAggregateFunction::Count)
			{
				aggregate.PropertyName.clear();
				continue;
			}
			const auto* property = DesignerDataContextSchemaUtils::Find(
				itemType->Properties, aggregate.PropertyName);
			DesignerStyleValueKind kind{};
			if (!property || !property->CanRead
				|| !TryValueKind(property->ValueKind, kind))
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的聚合路径不是可读标量："
					+ aggregate.PropertyName, outError);
			const bool numeric = property->ValueKind == BindingValueKind::Bool
				|| property->ValueKind == BindingValueKind::NullableBool
				|| property->ValueKind == BindingValueKind::Int
				|| property->ValueKind == BindingValueKind::Int64
				|| property->ValueKind == BindingValueKind::Float
				|| property->ValueKind == BindingValueKind::Double;
			if ((aggregate.Function == CollectionAggregateFunction::Sum
				|| aggregate.Function == CollectionAggregateFunction::Average)
				&& !numeric)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的 Sum/Average 只能用于数值属性：" + property->Path,
					outError);
			aggregate.PropertyName = property->Path;
		}
		std::unordered_set<std::wstring> sortPaths;
		for (auto& sort : view.SortDescriptions)
		{
			sort.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
				sort.PropertyName);
			const auto* property = DesignerDataContextSchemaUtils::Find(
				itemType->Properties, sort.PropertyName);
			DesignerStyleValueKind kind{};
			if (!property || !property->CanRead
				|| !TryValueKind(property->ValueKind, kind))
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的排序路径不是可读标量：" + sort.PropertyName,
					outError);
			if (!sortPaths.insert(property->Path).second)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 包含重复排序路径：" + property->Path, outError);
			sort.PropertyName = property->Path;
		}
		for (auto& filter : view.FilterDescriptions)
		{
			filter.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
				filter.PropertyName);
			const auto* property = DesignerDataContextSchemaUtils::Find(
				itemType->Properties, filter.PropertyName);
			DesignerStyleValueKind kind{};
			if (!property || !property->CanRead
				|| !TryValueKind(property->ValueKind, kind))
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的筛选路径不是可读标量：" + filter.PropertyName,
					outError);
			filter.PropertyName = property->Path;
			const bool stringOperator = filter.Operator
				== CollectionFilterOperator::Contains
				|| filter.Operator == CollectionFilterOperator::StartsWith
				|| filter.Operator == CollectionFilterOperator::EndsWith;
			if (stringOperator && property->ValueKind != BindingValueKind::String)
				return Fail(L"CollectionViewSource " + view.Key
					+ L" 的文本筛选运算符只能用于 String："
					+ property->Path, outError);
			const bool valueless = filter.Operator
				== CollectionFilterOperator::IsEmpty
				|| filter.Operator == CollectionFilterOperator::IsNotEmpty;
			if (valueless)
			{
				filter.Value.clear();
				continue;
			}
			BindingValue converted;
			std::wstring conversionError;
			if (!DesignerStyleSheetUtils::TryConvertValue(
				{ kind, filter.Value }, converted, &conversionError,
				document.ResourceBasePath, document.Resources))
				return Fail(L"CollectionViewSource " + view.Key + L" 筛选条件 "
					+ property->Path + L"：" + conversionError, outError);
		}
	}

	auto validateMemberExpressions = [&](const DesignNode& node,
		const std::wstring& owner)
	{
		for (const auto& [propertyName, assignment] : node.Properties.Values)
		{
			(void)assignment;
			if (node.Bindings.contains(propertyName)
				|| node.TemplateBindings.contains(propertyName))
				return Fail(owner + L" 的属性 " + propertyName
					+ L" 同时包含多个本地值表达式。", outError);
		}
		for (const auto& [propertyName, binding] : node.Bindings)
		{
			(void)binding;
			if (node.TemplateBindings.contains(propertyName))
				return Fail(owner + L" 的属性 " + propertyName
					+ L" 同时包含 Binding 和 TemplateBinding。", outError);
		}
		return true;
	};

	std::function<bool(std::vector<DesignNode>&,
		const DesignerDataContextSchema&, const std::wstring&,
		std::optional<UIClass>)>
		validateNodeResources;
	validateNodeResources = [&](std::vector<DesignNode>& nodes,
		const DesignerDataContextSchema& schema,
		const std::wstring& owner,
		std::optional<UIClass> itemsPresenterTarget) -> bool
	{
		const auto itemsPresenterCount = std::count_if(
			nodes.begin(), nodes.end(), [](const auto& candidate)
			{
				return candidate.Type == UIClass::UI_ItemsPresenter
					&& !candidate.TemplateState.Generated;
			});
		if (itemsPresenterCount > 1)
			return Fail(owner + L" 最多只能包含一个 ItemsPresenter。", outError);
		if (itemsPresenterCount != 0
			&& (!itemsPresenterTarget
				|| !IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, *itemsPresenterTarget)))
			return Fail(owner + L" 的 ItemsPresenter 只能用于 ItemsControl "
				L"派生类型的 ControlTemplate。", outError);
		for (auto& node : nodes)
		{
			if (!validateMemberExpressions(node,
				owner + L"/" + (node.Name.empty()
					? DesignerStyleSheetUtils::UIClassName(node.Type)
					: node.Name))) return false;
			if (!validateTemplateSetters(
				node.LocalResources, &nodes, &node,
				owner + L"/" + node.Name)) return false;
			if (node.Structure.ChildRole == DesignNodeChildRole::Header)
			{
				const auto parent = std::find_if(
					nodes.begin(), nodes.end(), [&](const auto& candidate)
					{
						return (node.ParentId > 0 && candidate.Id == node.ParentId)
							|| (!node.ParentRef.empty()
								&& candidate.Name == node.ParentRef);
					});
				if (parent == nodes.end()
					|| (!IsUIClassAssignableFrom(
							UIClass::UI_HeaderedContentControl, parent->Type)
						&& !IsUIClassAssignableFrom(
							UIClass::UI_HeaderedItemsControl, parent->Type)))
					return Fail(owner
						+ L" 的视觉 Header 必须直属于 HeaderedContentControl "
							L"或 HeaderedItemsControl。",
						outError);
			}
			const bool headeredContentControl =
				IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, node.Type);
			const bool headeredItemsControl =
				IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, node.Type);
			const bool headeredControl =
				headeredContentControl || headeredItemsControl;
			auto isDirectChild = [&](const auto& candidate)
			{
				return !candidate.TemplateState.Generated
					&& ((node.Id > 0 && candidate.ParentId == node.Id)
					|| (!node.Name.empty() && !candidate.ParentRef.empty()
						&& candidate.ParentRef == node.Name));
			};
			auto isHeaderChild = [&](const auto& candidate)
			{
				return isDirectChild(candidate)
					&& candidate.Structure.ChildRole
						== DesignNodeChildRole::Header;
			};
			const auto visualChildCount = std::count_if(
				nodes.begin(), nodes.end(), [&](const auto& candidate)
				{
					return isDirectChild(candidate) && !isHeaderChild(candidate);
				});
			const auto visualHeaderCount = std::count_if(
				nodes.begin(), nodes.end(), isHeaderChild);
			if (node.Type == UIClass::UI_ItemsPresenter
				&& visualChildCount != 0)
				return Fail(owner + L" 的 ItemsPresenter 不接受手工视觉子节点。",
					outError);
			const bool contentHost = node.Type == UIClass::UI_ContentPresenter
				|| IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, node.Type);
			const bool visualContentControl =
				IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, node.Type);
			const bool hasContentBinding = node.Bindings.contains(L"Content");
			const bool hasContentTemplate = !node.Structure.ContentTemplate.empty();
			const bool hasContentValue = node.Properties.Find(L"Content");
			const bool hasHeaderBinding = node.Bindings.contains(L"Header");
			const bool hasHeaderTemplate = !node.Structure.HeaderTemplate.empty();
			const bool hasHeaderValue = node.Properties.Find(L"Header");
			if (!node.Structure.ControlTemplate.empty())
			{
				if (!IsControlTemplateHostType(node.Type)
					&& node.ComponentType.Empty())
					return Fail(owner + L" 的 Control.Template 格式无效。",
						outError);
				const auto& key = node.Structure.ControlTemplate;
				auto* controlTemplate = document.FindControlTemplate(
					nodes, node, key);
				if (!controlTemplate && fallbackResources)
					controlTemplate =
						fallbackResources->FindControlTemplate(key);
				if (!controlTemplate)
					return Fail(owner + L" 引用了未声明的 ControlTemplate："
						+ key, outError);
				if (!IsControlTemplateTargetCompatible(node, *controlTemplate))
					return Fail(owner
						+ L" 的 Control.Template TargetType 与控件类型不兼容："
						+ controlTemplate->DisplayName(), outError);
				node.Structure.ControlTemplate = controlTemplate->Key;
			}
			if (node.Type == UIClass::UI_ContentPresenter
				&& visualChildCount != 0
				&& node.TemplateContentSource.empty())
				return Fail(owner
					+ L" 的 ContentPresenter 不接受直接视觉子节点。", outError);
			if (visualContentControl
				&& (visualChildCount > 1 || (visualChildCount != 0
					&& (hasContentBinding || hasContentTemplate
						|| hasContentValue))))
				return Fail(owner
					+ L" 的 ContentControl 最多接受一个直接视觉子节点，且不能与数据内容同时使用。",
					outError);
			if (node.Type == UIClass::UI_Popup
				&& (visualChildCount > 1 || hasContentBinding
					|| hasContentTemplate || hasContentValue))
				return Fail(owner
					+ L" 的 Popup 只接受一个视觉 Child，不拥有 Content 或 "
						L"ContentTemplate。",
					outError);
			if (IsUIClassAssignableFrom(
					UIClass::UI_Decorator, node.Type)
				&& (visualChildCount > 1 || hasContentBinding
					|| hasContentTemplate || hasContentValue))
				return Fail(owner
					+ L" 的 Decorator 只接受一个视觉 Child，不拥有 Content 或 "
						L"ContentTemplate。",
					outError);
			if (hasContentValue)
			{
				if (!contentHost)
					return Fail(owner + L" 的 Content 属性格式无效。", outError);
				if (hasContentBinding)
					return Fail(owner
						+ L" 不能同时声明 Content 值和 Binding。",
						outError);
				if (hasContentTemplate)
					return Fail(owner
						+ L" 的标量 Content 当前不能使用 DataTemplate。",
						outError);
			}
			if (visualHeaderCount > 1 || (visualHeaderCount != 0
				&& (hasHeaderBinding || hasHeaderTemplate || hasHeaderValue)))
				return Fail(owner
					+ L" 的 HeaderedContentControl 最多接受一个视觉 Header，且不能与数据 Header 同时使用。",
					outError);
			if (hasHeaderValue)
			{
				if (!headeredControl)
					return Fail(owner + L" 的 Header 属性格式无效。", outError);
				if (hasHeaderBinding)
					return Fail(owner
						+ L" 不能同时声明 Header 值和 Binding。",
						outError);
				if (hasHeaderTemplate)
					return Fail(owner
						+ L" 的标量 Header 当前不能使用 DataTemplate。",
						outError);
			}
			const DesignDataList* dataList = nullptr;
			const DesignCollectionViewSource* collectionView = nullptr;
			const DesignDataTemplate* dataTemplate = nullptr;
			const DesignDataTemplate* contentTemplate = nullptr;
			const DesignDataTemplate* headerTemplate = nullptr;
			const DesignGroupStyle* groupStyle = nullptr;
			const DesignItemsPanelTemplate* itemsPanel = nullptr;
			if (!node.Structure.ItemsSourceResource.empty())
			{
				const auto& key = node.Structure.ItemsSourceResource;
				dataList = document.FindDataList(key);
				collectionView = document.FindCollectionView(key);
				if (!dataList && !collectionView)
					return Fail(owner + L" 引用了未声明的列表资源：" + key,
						outError);
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return Fail(owner + L" 的控件类型不支持列表资源 ItemsSource。",
						outError);
				if (node.Bindings.contains(L"ItemsSource"))
					return Fail(owner
						+ L" 不能同时声明 Binding 与 StaticResource ItemsSource。",
						outError);
				node.Structure.ItemsSourceResource =
					dataList ? dataList->Key : collectionView->Key;
			}
			if (!node.Structure.ItemTemplate.empty())
			{
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return Fail(owner + L" 的 ItemTemplate 格式无效。", outError);
				const auto& key = node.Structure.ItemTemplate;
				dataTemplate = document.FindDataTemplate(nodes, node, key);
				if (!dataTemplate && fallbackResources)
					dataTemplate = fallbackResources->FindDataTemplate(key);
				if (!dataTemplate)
					return Fail(owner + L" 引用了未声明的 DataTemplate：" + key,
						outError);
			}
			if (!node.Structure.ContentTemplate.empty())
			{
				if (!contentHost)
					return Fail(owner + L" 的 ContentTemplate 格式无效。", outError);
				const auto& key = node.Structure.ContentTemplate;
				contentTemplate = document.FindDataTemplate(nodes, node, key);
				if (!contentTemplate && fallbackResources)
					contentTemplate =
						fallbackResources->FindDataTemplate(key);
				if (!contentTemplate)
					return Fail(owner + L" 引用了未声明的 DataTemplate：" + key,
						outError);
				node.Structure.ContentTemplate = contentTemplate->Key;
			}
			if (!node.Structure.HeaderTemplate.empty())
			{
				if (!headeredControl)
					return Fail(owner + L" 的 HeaderTemplate 格式无效。", outError);
				const auto& key = node.Structure.HeaderTemplate;
				headerTemplate = document.FindDataTemplate(nodes, node, key);
				if (!headerTemplate && fallbackResources)
					headerTemplate =
						fallbackResources->FindDataTemplate(key);
				if (!headerTemplate)
					return Fail(owner + L" 引用了未声明的 DataTemplate：" + key,
						outError);
				node.Structure.HeaderTemplate = headerTemplate->Key;
			}
			if (!node.Structure.ItemsPanel.empty())
			{
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return Fail(owner + L" 的 ItemsPanel 格式无效。", outError);
				const auto& key = node.Structure.ItemsPanel;
				itemsPanel = document.FindItemsPanelTemplate(nodes, node, key);
				if (!itemsPanel && fallbackResources)
					itemsPanel =
						fallbackResources->FindItemsPanelTemplate(key);
				if (!itemsPanel)
					return Fail(owner + L" 引用了未声明的 ItemsPanelTemplate："
						+ key, outError);
				node.Structure.ItemsPanel = itemsPanel->Key;
			}
			if (!node.Structure.GroupStyle.empty())
			{
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return Fail(owner + L" 的 GroupStyle 格式无效。", outError);
				const auto& key = node.Structure.GroupStyle;
				groupStyle = document.FindGroupStyle(nodes, node, key);
				if (!groupStyle && fallbackResources)
					groupStyle = fallbackResources->FindGroupStyle(key);
				if (!groupStyle)
					return Fail(owner + L" 引用了未声明的 GroupStyle：" + key,
						outError);
				node.Structure.GroupStyle = groupStyle->Key;
			}
			if (!node.Structure.ItemContainerStyle.empty())
			{
				if (!IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type))
					return Fail(owner + L" 的 ItemContainerStyle 格式无效。",
						outError);
				const auto& key = node.Structure.ItemContainerStyle;
				const DesignerStyleRule* style = nullptr;
				const auto localStyle = std::find_if(
					document.StyleSheet.Rules.begin(),
					document.StyleSheet.Rules.end(),
					[&](const auto& rule)
					{
						return !rule.Id.empty() && rule.Id == key;
					});
				if (localStyle != document.StyleSheet.Rules.end())
					style = &*localStyle;
				if (!style && fallbackResources)
				if (const auto fallbackStyle = std::find_if(
					fallbackResources->StyleSheet.Rules.begin(),
					fallbackResources->StyleSheet.Rules.end(),
					[&](const auto& rule)
					{
						return !rule.Id.empty() && rule.Id == key;
					});
					fallbackStyle != fallbackResources->StyleSheet.Rules.end())
					style = &*fallbackStyle;
				if (!style)
					return Fail(owner + L" 引用了未声明的 Style：" + key,
						outError);
				const auto expectedContainer =
					GetDefaultItemContainerType(node.Type);
				if (!style->ComponentType.Empty()
					|| (style->HasType
						&& style->Type != UIClass::UI_Base
						&& style->Type != expectedContainer))
					return Fail(owner
						+ L" 的 ItemContainerStyle TargetType 与默认项容器不匹配。",
						outError);
				node.Structure.ItemContainerStyle = style->Id;
			}
			const std::wstring resourceItemType = dataList ? dataList->ItemType
				: collectionView ? viewItemTypes[collectionView->Key] : L"";
			if (!resourceItemType.empty() && dataTemplate
				&& resourceItemType != dataTemplate->DataType)
				return Fail(owner
					+ L" 的列表资源 ItemType 与 DataTemplate.DataType 不一致。",
					outError);
			if (dataTemplate && node.Bindings.contains(L"ItemsSource"))
			{
				const auto& binding = node.Bindings.at(L"ItemsSource");
				const bool explicitSource = !binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.IsMultiBinding();
				const auto* property = explicitSource ? nullptr
					: DesignerDataContextSchemaUtils::Find(
						schema, binding.SourceProperty);
				if (property && (property->ObjectKind
						!= DesignerDataObjectKind::BindingList
					|| property->ItemType != dataTemplate->DataType))
					return Fail(owner
						+ L" 的 ItemsSource 与 DataTemplate 类型不一致。",
						outError);
			}
			if (contentTemplate && node.Bindings.contains(L"Content"))
			{
				const auto& binding = node.Bindings.at(L"Content");
				const bool explicitSource = !binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.IsMultiBinding();
				const auto* property = explicitSource ? nullptr
					: DesignerDataContextSchemaUtils::Find(
						schema, binding.SourceProperty);
				if (property && (property->ObjectKind
						!= DesignerDataObjectKind::BindingSource
					|| property->DataType.empty()
					|| property->DataType != contentTemplate->DataType))
					return Fail(owner
						+ L" 的 Content 与 ContentTemplate 类型不一致。",
						outError);
			}
			if (headerTemplate && node.Bindings.contains(L"Header"))
			{
				const auto& binding = node.Bindings.at(L"Header");
				const bool explicitSource = !binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.IsMultiBinding();
				const auto* property = explicitSource ? nullptr
					: DesignerDataContextSchemaUtils::Find(
						schema, binding.SourceProperty);
				if (property && (property->ObjectKind
						!= DesignerDataObjectKind::BindingSource
					|| property->DataType.empty()
					|| property->DataType != headerTemplate->DataType))
					return Fail(owner
						+ L" 的 Header 与 HeaderTemplate 类型不一致。",
						outError);
			}

			const DesignDataTypeDefinition* itemType = nullptr;
			if (dataList) itemType = document.FindDataType(dataList->ItemType);
			if (collectionView) itemType = document.FindDataType(
				viewItemTypes[collectionView->Key]);
			if (!itemType && node.Bindings.contains(L"ItemsSource"))
			{
				const auto& binding = node.Bindings.at(L"ItemsSource");
				const bool explicitSource = !binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.IsMultiBinding();
				const auto* sourceProperty = explicitSource ? nullptr :
					DesignerDataContextSchemaUtils::Find(
						schema, binding.SourceProperty);
				if (sourceProperty
					&& sourceProperty->ObjectKind
						== DesignerDataObjectKind::BindingList)
					itemType = document.FindDataType(sourceProperty->ItemType);
			}
			if (!itemType && dataTemplate)
				itemType = document.FindDataType(dataTemplate->DataType);
			if (itemType && groupStyle)
			{
				const auto& groupStyleKey = node.Structure.GroupStyle;
				auto* header = document.FindGroupStyleHeaderTemplate(
					nodes, node, groupStyleKey);
				if (!header && fallbackResources)
					header = groupStyle->HeaderTemplate.empty()
						? fallbackResources->FindImplicitDataTemplate(
							std::wstring(CollectionViewGroupDataTypeName))
						: fallbackResources->FindDataTemplate(
							groupStyle->HeaderTemplate);
				if (!header && !groupStyle->HeaderTemplate.empty())
					return Fail(owner + L" 的 GroupStyle " + groupStyle->Key
						+ L" 引用了未声明的 DataTemplate："
						+ groupStyle->HeaderTemplate, outError);
				if (header)
				{
					const auto groupSchema = BuildCollectionViewGroupSchema(itemType,
						collectionView ? &collectionView->AggregateDescriptions : nullptr);
					for (const auto& templateNode : header->Template)
					{
						for (const auto& [target, binding]
							: templateNode.Bindings)
						{
							(void)target;
							std::wstring bindingError;
							if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
								binding, [&](const DesignerDataBinding& child)
								{
									if (!child.ElementName.empty()
										|| child.RelativeSource
											!= DesignerBindingRelativeSource::None)
										return true;
									const auto path = DesignerBindingUtils::Trim(
										child.SourceProperty);
									if (path.empty() || DesignerDataContextSchemaUtils::Find(
										groupSchema, path)) return true;
									bindingError = owner
										+ L" 的 GroupStyle.HeaderTemplate 绑定路径未在 CollectionViewGroup 或其 FirstItem 中声明："
										+ path;
									return false;
								}))
								return Fail(bindingError, outError);
						}
					}
				}
			}
			std::unordered_set<std::wstring> localObjectKeys;
			std::unordered_set<std::wstring> localImplicitTemplateTypes;
			std::unordered_set<std::wstring> localImplicitControlTemplateTypes;
			for (const auto& resource : node.LocalResources.Resources)
				if (!resource.Key.empty()
					&& !localObjectKeys.insert(resource.Key).second)
					return Fail(owner + L" 的局部资源键重复："
						+ resource.Key, outError);
			for (const auto& rule : node.LocalResources.Rules)
				if (!rule.Id.empty()
					&& !localObjectKeys.insert(rule.Id).second)
					return Fail(owner + L" 的局部资源键重复："
						+ rule.Id, outError);
			for (auto& localTemplate
				: node.LocalObjectResources.ControlTemplates)
			{
				localTemplate.Key = DesignerBindingUtils::Trim(localTemplate.Key);
				if (localTemplate.TargetComponentType.Empty())
				{
					if (!IsControlTemplateHostType(localTemplate.TargetType))
						return Fail(owner
							+ L" 的局部 ControlTemplate TargetType 无效："
							+ localTemplate.DisplayName(), outError);
				}
				else
				{
					const auto* component = document.FindComponent(
						nodes, node, localTemplate.TargetComponentType);
					if (!component || component->BaseType
						!= localTemplate.TargetType)
						return Fail(owner
							+ L" 的局部 ControlTemplate TargetType 引用了"
							L"不存在或 BaseType 不一致的组件："
							+ localTemplate.DisplayName(), outError);
					localTemplate.TargetComponentType = component->Type;
				}
				if (localTemplate.IsImplicit())
				{
					const auto identity = localTemplate.TargetComponentType.Empty()
						? L"builtin:" + std::to_wstring(
							static_cast<int>(localTemplate.TargetType))
						: L"component:"
							+ localTemplate.TargetComponentType.RegistryKey();
					if (!localImplicitControlTemplateTypes.insert(identity).second)
						return Fail(owner
							+ L" 的局部隐式 ControlTemplate TargetType 重复："
							+ localTemplate.DisplayName(), outError);
				}
				else if (!IsIdentifier(localTemplate.Key)
					|| !localObjectKeys.insert(localTemplate.Key).second)
					return Fail(owner
						+ L" 的局部 ControlTemplate 键无效或重复："
						+ localTemplate.Key, outError);
				DesignDocument templateDocument;
				templateDocument.Nodes = localTemplate.Template;
				templateDocument.RecalculateNextStableId();
				DesignDocumentGraph templateGraph;
				std::wstring templateGraphError;
				if (localTemplate.Template.empty()
					|| !DesignDocumentGraph::Build(
						templateDocument, templateGraph, &templateGraphError)
					|| templateGraph.Roots().size() != 1)
					return Fail(owner + L" 的局部 ControlTemplate "
						+ localTemplate.DisplayName()
						+ L" 必须包含一个有效视觉根："
						+ templateGraphError, outError);
				if (!validateNodeResources(localTemplate.Template, schema,
					owner + L"/ControlTemplate "
						+ localTemplate.DisplayName(),
					localTemplate.TargetType)) return false;
			}
			for (auto& localTemplate : node.LocalObjectResources.DataTemplates)
			{
				localTemplate.Key = DesignerBindingUtils::Trim(localTemplate.Key);
				localTemplate.DataType = DesignerBindingUtils::Trim(
					localTemplate.DataType);
				if (localTemplate.IsImplicit())
				{
					if (!localImplicitTemplateTypes.insert(
						localTemplate.DataType).second)
						return Fail(owner
							+ L" 的局部隐式 DataTemplate DataType 重复："
							+ localTemplate.DataType, outError);
				}
				else if (!IsIdentifier(localTemplate.Key)
					|| !localObjectKeys.insert(localTemplate.Key).second)
					return Fail(owner + L" 的局部 DataTemplate 键无效或重复："
						+ localTemplate.Key, outError);
				const auto* localType = document.FindDataType(localTemplate.DataType);
				const bool groupType = IsCollectionViewGroupDataType(
					localTemplate.DataType);
				if (!localType && !groupType)
					return Fail(owner + L" 的局部 DataTemplate "
						+ localTemplate.DisplayName()
						+ L" 引用了未声明的 DataType："
						+ localTemplate.DataType, outError);
				localTemplate.DataType = localType ? localType->Name
					: std::wstring(CollectionViewGroupDataTypeName);
				const auto localSchema = localType ? localType->Properties
					: BuildCollectionViewGroupSchema();
				if (!validateHierarchicalTemplate(localTemplate, localSchema,
					owner + L"/DataTemplate "
						+ localTemplate.DisplayName())) return false;
				if (!validateNodeResources(localTemplate.Template,
					localSchema, owner + L"/DataTemplate "
						+ localTemplate.DisplayName(), std::nullopt)) return false;
			}
			for (auto& localPanel : node.LocalObjectResources.ItemsPanelTemplates)
			{
				localPanel.Key = DesignerBindingUtils::Trim(localPanel.Key);
				if (!IsIdentifier(localPanel.Key)
					|| !localObjectKeys.insert(localPanel.Key).second)
					return Fail(owner
						+ L" 的局部 ItemsPanelTemplate 键无效或重复："
						+ localPanel.Key, outError);
				auto& value = localPanel.Value;
				if ((value.Kind != ItemsPanelKind::Stack
						&& value.Kind != ItemsPanelKind::Wrap
						&& value.Kind != ItemsPanelKind::VirtualizingStack)
					|| (value.Orientation != Orientation::Horizontal
						&& value.Orientation != Orientation::Vertical)
					|| !std::isfinite(value.ItemWidth) || value.ItemWidth < 0.0f
					|| !std::isfinite(value.ItemHeight) || value.ItemHeight < 0.0f
					|| !std::isfinite(value.CacheLength) || value.CacheLength < 0.0f)
					return Fail(owner
						+ L" 的局部 ItemsPanelTemplate 配置无效："
						+ localPanel.Key, outError);
				if (value.Kind == ItemsPanelKind::VirtualizingStack
					&& (value.Orientation != Orientation::Vertical
						|| value.ItemHeight <= 0.0f))
					return Fail(owner
						+ L" 的局部 VirtualizingStackPanel 只支持 Vertical，且必须声明正数 ItemHeight："
						+ localPanel.Key, outError);
				if (value.Kind == ItemsPanelKind::Stack)
				{
					value.ItemWidth = 0.0f;
					value.ItemHeight = 0.0f;
					value.CacheLength = 1.0f;
				}
				else if (value.Kind == ItemsPanelKind::Wrap)
				{
					value.CacheLength = 1.0f;
				}
				else value.ItemWidth = 0.0f;
			}
			for (auto& localGroup : node.LocalObjectResources.GroupStyles)
			{
				localGroup.Key = DesignerBindingUtils::Trim(localGroup.Key);
				localGroup.HeaderTemplate = DesignerBindingUtils::Trim(
					localGroup.HeaderTemplate);
				if (!IsIdentifier(localGroup.Key)
					|| !localObjectKeys.insert(localGroup.Key).second)
					return Fail(owner + L" 的局部 GroupStyle 键无效或重复："
						+ localGroup.Key, outError);
				if (!localGroup.HeaderTemplate.empty())
				{
					const auto* header = document.FindDataTemplate(
						nodes, node, localGroup.HeaderTemplate);
					if (!header)
						return Fail(owner + L" 的局部 GroupStyle "
							+ localGroup.Key
							+ L" 引用了未声明的 DataTemplate："
							+ localGroup.HeaderTemplate, outError);
					localGroup.HeaderTemplate = header->Key;
					if (!IsCollectionViewGroupDataType(header->DataType))
						return Fail(owner + L" 的局部 GroupStyle "
							+ localGroup.Key
							+ L" 的 HeaderTemplate.DataType 必须为 CollectionViewGroup。",
							outError);
				}
			}
			for (auto& localComponent : node.LocalObjectResources.Components)
				if (!validateNodeResources(localComponent.Template, schema,
					owner + L"/Component " + localComponent.Type.XamlName,
					std::nullopt))
					return false;
			const DesignDataTypeDefinition* contentType = nullptr;
			if (contentHost && node.Bindings.contains(L"Content"))
			{
				const auto& binding = node.Bindings.at(L"Content");
				if (binding.ElementName.empty()
					&& binding.RelativeSource == DesignerBindingRelativeSource::None
					&& !binding.IsMultiBinding())
				{
					const auto* property = DesignerDataContextSchemaUtils::Find(
						schema, binding.SourceProperty);
					if (property && property->ObjectKind
						== DesignerDataObjectKind::BindingSource)
						contentType = document.FindDataType(property->DataType);
				}
			}
			if (!contentType && contentTemplate)
				contentType = document.FindDataType(contentTemplate->DataType);
			const DesignDataTypeDefinition* headerType = nullptr;
			if (headeredControl && node.Bindings.contains(L"Header"))
			{
				const auto& binding = node.Bindings.at(L"Header");
				if (binding.ElementName.empty()
					&& binding.RelativeSource == DesignerBindingRelativeSource::None
					&& !binding.IsMultiBinding())
				{
					const auto* property = DesignerDataContextSchemaUtils::Find(
						schema, binding.SourceProperty);
					if (property && property->ObjectKind
						== DesignerDataObjectKind::BindingSource)
						headerType = document.FindDataType(property->DataType);
				}
			}
			if (!headerType && headerTemplate)
				headerType = document.FindDataType(headerTemplate->DataType);
			if (!itemType && !contentType && !headerType) continue;

			auto validatePath = [&](const wchar_t* propertyName,
				const DesignDataTypeDefinition* sourceType)
			{
				if (!sourceType) return true;
				auto* assignment = node.Properties.Find(propertyName);
				if (assignment)
				{
					const auto path = DesignerDataContextSchemaUtils::NormalizePath(
						assignment->Value.Text);
					if (path.empty()) return true;
					const auto* property = DesignerDataContextSchemaUtils::Find(
						sourceType->Properties, path);
					if (!property || !property->CanRead)
						return Fail(owner + L" 的 " + propertyName
							+ L" 未在 DataType " + sourceType->Name
							+ L" 中声明为可读路径：" + path, outError);
					assignment->Value.Text = property->Path;
					return true;
				}
				return true;
			};
			if (!validatePath(L"DisplayMemberPath",
				contentHost ? contentType : itemType)
				|| !validatePath(L"HeaderDisplayMemberPath", headerType)
				|| !validatePath(L"SelectedValuePath", itemType)) return false;
		}
		return true;
	};
	if (!validateMemberExpressions(document.Window, L"Window")) return false;
	if (!validateNodeResources(document.Nodes, document.DataContextSchema,
		L"文档控件", std::nullopt)) return false;
	for (auto& item : document.DataTemplates)
	{
		const auto* type = document.FindDataType(item.DataType);
		if (type && !validateNodeResources(item.Template, type->Properties,
			L"DataTemplate " + item.DisplayName(), std::nullopt))
			return false;
	}
	for (auto& item : document.ControlTemplates)
		if (!validateNodeResources(item.Template, document.DataContextSchema,
			L"ControlTemplate " + item.DisplayName(), item.TargetType)) return false;
	if (outError) outError->clear();
	return true;
}

std::shared_ptr<ObservableBindingList> BuildRuntimeList(
	const DesignDocument& document,
	const DesignDataList& list,
	std::wstring* outError)
{
	const auto* canonicalList = document.FindDataList(list.Key);
	const auto* dataType = canonicalList
		? document.FindDataType(canonicalList->ItemType) : nullptr;
	if (!canonicalList || !dataType)
		return Fail(L"DataList 定义不完整：" + list.Key, outError), nullptr;

	auto result = std::make_shared<ObservableBindingList>(dataType->Name);
	for (const auto& record : canonicalList->Records)
	{
		auto root = std::make_shared<ObservableObject>();
		std::map<std::wstring, std::shared_ptr<ObservableObject>> objects;
		objects.emplace(L"", root);
		for (const auto& property : dataType->Properties)
		{
			const auto parent = objects.find(ParentPath(property.Path));
			if (parent == objects.end())
				return Fail(L"DataType 对象层级无法实例化：" + property.Path,
					outError), nullptr;
			BindingValue value;
			if (property.ValueKind == BindingValueKind::Object)
			{
				if (property.ObjectKind == DesignerDataObjectKind::BindingSource)
				{
					auto nested = std::make_shared<ObservableObject>();
					objects[property.Path] = nested;
					value = BindingValue(BindingSourceReference(nested));
				}
				else if (property.ObjectKind == DesignerDataObjectKind::BindingList)
					value = BindingValue(BindingListReference(
						std::make_shared<ObservableBindingList>(property.ItemType)));
				else value = BindingValue(std::shared_ptr<void>{});
			}
			else
			{
				value = DefaultValue(property.ValueKind);
				const auto field = record.Fields.find(property.Path);
				if (field != record.Fields.end())
				{
					DesignerStyleValueKind kind{};
					if (!TryValueKind(property.ValueKind, kind)
						|| !DesignerStyleSheetUtils::TryConvertValue(
							{ kind, field->second }, value, outError,
							document.ResourceBasePath, document.Resources)) return {};
				}
			}
			BindingSourcePropertyMetadata metadata{
				LeafName(property.Path), property.ValueKind,
				std::type_index(value.Type()), property.CanRead,
				property.CanWrite, property.CanObserve };
			if (!parent->second->DefineProperty(std::move(metadata), value))
				return Fail(L"DataList 字段无法实例化：" + property.Path,
					outError), nullptr;
		}
		result->Items.push_back(BindingSourceReference(root));
	}
	if (outError) outError->clear();
	return result;
}
}
