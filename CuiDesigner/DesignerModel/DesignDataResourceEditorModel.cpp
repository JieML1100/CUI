#include "DesignDataResourceEditorModel.h"
#include "DesignDataResourceUtils.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include <algorithm>
#include <unordered_set>

namespace DesignerModel::DesignDataResourceEditorModel
{
namespace
{
	bool Equals(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	bool IsLocal(const std::wstring& sourceDictionary)
	{
		return sourceDictionary.empty();
	}

	void RewriteReferencedType(
		DesignerDataContextSchema& schema,
		const std::wstring& oldName,
		const std::wstring& newName)
	{
		for (auto& property : schema)
		{
			if (property.ObjectKind == DesignerDataObjectKind::BindingList
				&& Equals(property.ItemType, oldName))
				property.ItemType = newName;
			else if (property.ObjectKind == DesignerDataObjectKind::BindingSource
				&& Equals(property.DataType, oldName))
				property.DataType = newName;
		}
	}

	std::wstring NodeDataListKey(const DesignNode& node)
	{
		return node.Structure.ItemsSourceResource;
	}

	std::wstring NodeDataTemplateKey(const DesignNode& node)
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

	void RewriteDataListReference(
		std::vector<DesignNode>& nodes,
		const std::wstring& oldKey,
		const std::wstring& newKey)
	{
		for (auto& node : nodes)
			if (Equals(NodeDataListKey(node), oldKey))
				node.Structure.ItemsSourceResource = newKey;
	}

	void RewriteDataTemplateReference(
		std::vector<DesignNode>& nodes,
		const std::wstring& oldKey,
		const std::wstring& newKey)
	{
		for (auto& node : nodes)
		{
			if (Equals(NodeDataTemplateKey(node), oldKey))
				node.Structure.ItemTemplate = newKey;
			if (Equals(NodeContentTemplateKey(node), oldKey))
				node.Structure.ContentTemplate = newKey;
			if (Equals(NodeHeaderTemplateKey(node), oldKey))
				node.Structure.HeaderTemplate = newKey;
		}
	}

	std::wstring CollectionViewItemType(
		const DesignDocument& document,
		const DesignCollectionViewSource& view,
		std::unordered_set<std::wstring>& visited)
	{
		if (!visited.insert(view.Key).second) return {};
		if (const auto* list = document.FindDataList(view.SourceResource))
			return list->ItemType;
		if (const auto* sourceView = document.FindCollectionView(view.SourceResource))
			return CollectionViewItemType(document, *sourceView, visited);
		if (const auto* property = DesignerDataContextSchemaUtils::Find(
			document.DataContextSchema, view.SourceBindingPath))
			return property->ItemType;
		return {};
	}

	bool PathStartsWith(
		const std::wstring& path,
		const std::wstring& prefix)
	{
		return Equals(path, prefix)
			|| (path.size() > prefix.size()
				&& (path[prefix.size()] == L'.' || path[prefix.size()] == L'[')
				&& path.compare(0, prefix.size(), prefix) == 0);
	}

	std::wstring RewritePathPrefix(
		const std::wstring& path,
		const std::wstring& oldPrefix,
		const std::wstring& newPrefix)
	{
		return PathStartsWith(path, oldPrefix)
			? newPrefix + path.substr(oldPrefix.size()) : path;
	}

	void RewriteMetadataPath(
		DesignNode& node,
		const wchar_t* propertyName,
		const std::wstring& oldPath,
		const std::wstring& newPath)
	{
		auto* assignment = node.Properties.Find(propertyName);
		if (!assignment) return;
		assignment->Value.Text = RewritePathPrefix(
			assignment->Value.Text, oldPath, newPath);
	}

	void RewriteItemProjectionPaths(
		DesignDocument& document,
		std::vector<DesignNode>& nodes,
		const DesignerDataContextSchema& sourceSchema,
		const std::wstring& itemTypeName,
		const std::wstring& oldPath,
		const std::wstring& newPath)
	{
		for (auto& node : nodes)
		{
			std::wstring nodeItemType;
			const auto listKey = NodeDataListKey(node);
			if (!listKey.empty())
			{
				if (const auto* list = document.FindDataList(listKey))
					nodeItemType = list->ItemType;
			}
			if (nodeItemType.empty() && node.Bindings.contains(L"ItemsSource"))
			{
				const auto& binding = node.Bindings.at(L"ItemsSource");
				if (!binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.IsMultiBinding()) continue;
				const auto& sourcePath = binding.SourceProperty;
				if (const auto* source = DesignerDataContextSchemaUtils::Find(
					sourceSchema, sourcePath);
					source && source->ObjectKind
						== DesignerDataObjectKind::BindingList)
					nodeItemType = source->ItemType;
			}
			if (!Equals(nodeItemType, itemTypeName)) continue;
			RewriteMetadataPath(
				node, L"DisplayMemberPath", oldPath, newPath);
			RewriteMetadataPath(
				node, L"SelectedValuePath", oldPath, newPath);
		}
	}

	bool CommitCandidate(
		DesignDocument& document,
		DesignDocument candidate,
		std::wstring* outError)
	{
		if (!DesignDataResourceUtils::ValidateAndCanonicalize(
			candidate, outError)) return false;
		document = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
}

bool UpsertDataType(
	DesignDocument& document,
	const std::wstring& originalName,
	DesignDataTypeDefinition definition,
	std::wstring* outError)
{
	definition.Name = DesignerBindingUtils::Trim(definition.Name);
	auto candidate = document;
	auto existing = originalName.empty() ? candidate.DataTypes.end()
		: std::find_if(candidate.DataTypes.begin(), candidate.DataTypes.end(),
			[&](const auto& item) { return Equals(item.Name, originalName); });
	if (existing != candidate.DataTypes.end()
		&& !IsLocal(existing->SourceDictionary))
		return Fail(L"合并字典中的 DataType 必须在来源文件中编辑。", outError);
	const auto collision = std::find_if(
		candidate.DataTypes.begin(), candidate.DataTypes.end(),
		[&](const auto& item) { return Equals(item.Name, definition.Name); });
	if (collision != candidate.DataTypes.end() && collision != existing)
		return Fail(L"DataType 已存在：" + definition.Name, outError);
	definition.SourceDictionary.clear();
	if (existing == candidate.DataTypes.end())
		candidate.DataTypes.push_back(std::move(definition));
	else
	{
		const auto oldName = existing->Name;
		*existing = std::move(definition);
		if (!Equals(oldName, existing->Name) || oldName != existing->Name)
		{
			RewriteReferencedType(candidate.DataContextSchema, oldName, existing->Name);
			for (auto& type : candidate.DataTypes)
				RewriteReferencedType(type.Properties, oldName, existing->Name);
			for (auto& list : candidate.DataLists)
				if (Equals(list.ItemType, oldName)) list.ItemType = existing->Name;
			for (auto& itemTemplate : candidate.DataTemplates)
				if (Equals(itemTemplate.DataType, oldName))
					itemTemplate.DataType = existing->Name;
		}
	}
	return CommitCandidate(document, std::move(candidate), outError);
}

bool RemoveDataType(
	DesignDocument& document,
	const std::wstring& name,
	std::wstring* outError)
{
	auto candidate = document;
	const auto found = std::find_if(
		candidate.DataTypes.begin(), candidate.DataTypes.end(),
		[&](const auto& item) { return Equals(item.Name, name); });
	if (found == candidate.DataTypes.end())
		return Fail(L"找不到 DataType：" + name, outError);
	if (!IsLocal(found->SourceDictionary))
		return Fail(L"合并字典中的 DataType 必须在来源文件中删除。", outError);
	auto schemaUses = [&](const DesignerDataContextSchema& schema)
	{
		return std::any_of(schema.begin(), schema.end(), [&](const auto& property)
		{
			return (property.ObjectKind == DesignerDataObjectKind::BindingList
				&& Equals(property.ItemType, name))
				|| (property.ObjectKind == DesignerDataObjectKind::BindingSource
					&& Equals(property.DataType, name));
		});
	};
	if (schemaUses(candidate.DataContextSchema))
		return Fail(L"DataContext 仍引用 DataType：" + name, outError);
	for (const auto& type : candidate.DataTypes)
		if (!Equals(type.Name, name) && schemaUses(type.Properties))
			return Fail(L"DataType " + type.Name + L" 仍引用 " + name, outError);
	for (const auto& list : candidate.DataLists)
		if (Equals(list.ItemType, name))
			return Fail(L"DataList " + list.Key + L" 仍引用 " + name, outError);
	for (const auto& itemTemplate : candidate.DataTemplates)
		if (Equals(itemTemplate.DataType, name))
			return Fail(L"DataTemplate " + itemTemplate.DisplayName()
				+ L" 仍引用 " + name, outError);
	candidate.DataTypes.erase(found);
	return CommitCandidate(document, std::move(candidate), outError);
}

bool UpsertDataTypeProperty(
	DesignDocument& document,
	const std::wstring& typeName,
	const std::wstring& originalPath,
	DesignerDataContextProperty property,
	std::wstring* outError)
{
	property.Path = DesignerDataContextSchemaUtils::NormalizePath(property.Path);
	const auto normalizedType = DesignerBindingUtils::Trim(typeName);
	if (originalPath.empty())
	{
		if (const auto* existing = document.FindDataType(normalizedType))
		{
			auto definition = *existing;
			definition.Properties.push_back(std::move(property));
			return UpsertDataType(document, existing->Name,
				std::move(definition), outError);
		}
		DesignDataTypeDefinition definition;
		definition.Name = normalizedType;
		definition.Properties.push_back(std::move(property));
		return UpsertDataType(document, L"", std::move(definition), outError);
	}

	auto candidate = document;
	auto type = std::find_if(candidate.DataTypes.begin(), candidate.DataTypes.end(),
		[&](const auto& item) { return Equals(item.Name, normalizedType); });
	if (type == candidate.DataTypes.end())
		return Fail(L"找不到 DataType：" + normalizedType, outError);
	if (!IsLocal(type->SourceDictionary))
		return Fail(L"合并字典中的 DataType 必须在来源文件中编辑。", outError);
	auto selected = std::find_if(type->Properties.begin(), type->Properties.end(),
		[&](const auto& item) { return Equals(item.Path, originalPath); });
	if (selected == type->Properties.end())
		return Fail(L"找不到 DataType 字段：" + originalPath, outError);
	const auto collision = std::find_if(type->Properties.begin(), type->Properties.end(),
		[&](const auto& item)
		{
			return &item != &*selected && Equals(item.Path, property.Path);
		});
	if (collision != type->Properties.end())
		return Fail(L"DataType 字段已存在：" + property.Path, outError);
	const auto oldPath = selected->Path;
	const auto newPath = property.Path;
	*selected = property;
	if (!Equals(oldPath, newPath) || oldPath != newPath)
	{
		for (auto& item : type->Properties)
			if (&item != &*selected)
				item.Path = RewritePathPrefix(item.Path, oldPath, newPath);
		for (auto& list : candidate.DataLists)
		{
			if (!Equals(list.ItemType, type->Name)) continue;
			for (auto& record : list.Records)
			{
				std::map<std::wstring, std::wstring> rewritten;
				for (const auto& [path, value] : record.Fields)
				{
					const auto destination = RewritePathPrefix(path, oldPath, newPath);
					if (std::any_of(rewritten.begin(), rewritten.end(),
						[&](const auto& item) { return Equals(item.first, destination); }))
						return Fail(L"字段重命名导致 DataRecord 路径冲突："
							+ destination, outError);
					rewritten.emplace(destination, value);
				}
				record.Fields = std::move(rewritten);
			}
		}
		for (auto& view : candidate.CollectionViews)
		{
			std::unordered_set<std::wstring> visited;
			if (!Equals(CollectionViewItemType(candidate, view, visited), type->Name))
				continue;
			for (auto& sort : view.SortDescriptions)
				sort.PropertyName = RewritePathPrefix(
					sort.PropertyName, oldPath, newPath);
			for (auto& filter : view.FilterDescriptions)
				filter.PropertyName = RewritePathPrefix(
					filter.PropertyName, oldPath, newPath);
			for (auto& group : view.GroupDescriptions)
				group.PropertyName = RewritePathPrefix(
					group.PropertyName, oldPath, newPath);
			for (auto& aggregate : view.AggregateDescriptions)
				if (!aggregate.PropertyName.empty())
					aggregate.PropertyName = RewritePathPrefix(
						aggregate.PropertyName, oldPath, newPath);
		}
		for (auto& itemTemplate : candidate.DataTemplates)
		{
			if (!Equals(itemTemplate.DataType, type->Name)) continue;
			for (auto& node : itemTemplate.Template)
			{
				for (auto& [target, binding] : node.Bindings)
				{
					(void)target;
					(void)DesignerBindingUtils::VisitLeafBindingDefinitions(
						binding, [&](DesignerDataBinding& child)
						{
							if (!child.ElementName.empty()
								|| child.RelativeSource
									!= DesignerBindingRelativeSource::None) return true;
							child.SourceProperty = RewritePathPrefix(
								child.SourceProperty, oldPath, newPath);
							return true;
						});
				}
			}
		}
		RewriteItemProjectionPaths(candidate, candidate.Nodes,
			candidate.DataContextSchema, type->Name, oldPath, newPath);
		for (auto& itemTemplate : candidate.DataTemplates)
		{
			const auto* ownerType = candidate.FindDataType(itemTemplate.DataType);
			if (ownerType)
				RewriteItemProjectionPaths(candidate, itemTemplate.Template,
					ownerType->Properties, type->Name, oldPath, newPath);
		}
	}
	return CommitCandidate(document, std::move(candidate), outError);
}

bool UpsertDataList(
	DesignDocument& document,
	const std::wstring& originalKey,
	DesignDataList definition,
	std::wstring* outError)
{
	definition.Key = DesignerBindingUtils::Trim(definition.Key);
	definition.ItemType = DesignerBindingUtils::Trim(definition.ItemType);
	auto candidate = document;
	auto existing = originalKey.empty() ? candidate.DataLists.end()
		: std::find_if(candidate.DataLists.begin(), candidate.DataLists.end(),
			[&](const auto& item) { return Equals(item.Key, originalKey); });
	if (existing != candidate.DataLists.end()
		&& !IsLocal(existing->SourceDictionary))
		return Fail(L"合并字典中的 DataList 必须在来源文件中编辑。", outError);
	const auto collision = std::find_if(
		candidate.DataLists.begin(), candidate.DataLists.end(),
		[&](const auto& item) { return Equals(item.Key, definition.Key); });
	if (collision != candidate.DataLists.end() && collision != existing)
		return Fail(L"DataList 已存在：" + definition.Key, outError);
	if (candidate.FindCollectionView(definition.Key))
		return Fail(L"资源键已被 CollectionViewSource 使用："
			+ definition.Key, outError);
	definition.SourceDictionary.clear();
	if (existing == candidate.DataLists.end())
		candidate.DataLists.push_back(std::move(definition));
	else
	{
		const auto oldKey = existing->Key;
		*existing = std::move(definition);
		if (!Equals(oldKey, existing->Key) || oldKey != existing->Key)
		{
			RewriteDataListReference(candidate.Nodes, oldKey, existing->Key);
			for (auto& itemTemplate : candidate.DataTemplates)
				RewriteDataListReference(
					itemTemplate.Template, oldKey, existing->Key);
			for (auto& view : candidate.CollectionViews)
				if (Equals(view.SourceResource, oldKey))
					view.SourceResource = existing->Key;
		}
	}
	return CommitCandidate(document, std::move(candidate), outError);
}

bool RemoveDataList(
	DesignDocument& document,
	const std::wstring& key,
	std::wstring* outError)
{
	auto candidate = document;
	const auto found = std::find_if(
		candidate.DataLists.begin(), candidate.DataLists.end(),
		[&](const auto& item) { return Equals(item.Key, key); });
	if (found == candidate.DataLists.end())
		return Fail(L"找不到 DataList：" + key, outError);
	if (!IsLocal(found->SourceDictionary))
		return Fail(L"合并字典中的 DataList 必须在来源文件中删除。", outError);
	auto usedBy = [&](const std::vector<DesignNode>& nodes)
	{
		return std::any_of(nodes.begin(), nodes.end(), [&](const auto& node)
			{ return Equals(NodeDataListKey(node), key); });
	};
	if (usedBy(candidate.Nodes))
		return Fail(L"文档控件仍引用 DataList：" + key, outError);
	for (const auto& itemTemplate : candidate.DataTemplates)
		if (usedBy(itemTemplate.Template))
			return Fail(L"DataTemplate " + itemTemplate.DisplayName()
				+ L" 仍引用 DataList：" + key, outError);
	for (const auto& view : candidate.CollectionViews)
		if (Equals(view.SourceResource, key))
			return Fail(L"CollectionViewSource " + view.Key
				+ L" 仍引用 DataList：" + key, outError);
	candidate.DataLists.erase(found);
	return CommitCandidate(document, std::move(candidate), outError);
}

bool UpsertDataTemplate(
	DesignDocument& document,
	const std::wstring& originalKey,
	DesignDataTemplate definition,
	std::wstring* outError)
{
	definition.Key = DesignerBindingUtils::Trim(definition.Key);
	definition.DataType = DesignerBindingUtils::Trim(definition.DataType);
	auto candidate = document;
	auto existing = originalKey.empty() ? candidate.DataTemplates.end()
		: std::find_if(candidate.DataTemplates.begin(), candidate.DataTemplates.end(),
			[&](const auto& item) { return Equals(item.Key, originalKey); });
	if (existing != candidate.DataTemplates.end()
		&& !IsLocal(existing->SourceDictionary))
		return Fail(L"合并字典中的 DataTemplate 必须在来源文件中编辑。", outError);
	const auto collision = std::find_if(candidate.DataTemplates.begin(),
		candidate.DataTemplates.end(),
		[&](const auto& item)
		{ return item.HasSameResourceIdentity(definition); });
	if (collision != candidate.DataTemplates.end() && collision != existing)
		return Fail(L"DataTemplate 已存在：" + definition.DisplayName(), outError);
	definition.SourceDictionary.clear();
	if (existing == candidate.DataTemplates.end())
		candidate.DataTemplates.push_back(std::move(definition));
	else
	{
		const auto oldKey = existing->Key;
		*existing = std::move(definition);
		if (!Equals(oldKey, existing->Key) || oldKey != existing->Key)
		{
			RewriteDataTemplateReference(candidate.Nodes, oldKey, existing->Key);
			for (auto& item : candidate.DataTemplates)
				RewriteDataTemplateReference(item.Template, oldKey, existing->Key);
			for (auto& style : candidate.GroupStyles)
				if (Equals(style.HeaderTemplate, oldKey))
					style.HeaderTemplate = existing->Key;
		}
	}
	return CommitCandidate(document, std::move(candidate), outError);
}

bool RemoveDataTemplate(
	DesignDocument& document,
	const std::wstring& key,
	std::wstring* outError)
{
	auto candidate = document;
	const auto found = std::find_if(candidate.DataTemplates.begin(),
		candidate.DataTemplates.end(),
		[&](const auto& item) { return Equals(item.Key, key); });
	if (found == candidate.DataTemplates.end())
		return Fail(L"找不到 DataTemplate：" + key, outError);
	if (!IsLocal(found->SourceDictionary))
		return Fail(L"合并字典中的 DataTemplate 必须在来源文件中删除。", outError);
	auto usedBy = [&](const std::vector<DesignNode>& nodes)
	{
		return std::any_of(nodes.begin(), nodes.end(), [&](const auto& node)
			{ return Equals(NodeDataTemplateKey(node), key)
				|| Equals(NodeContentTemplateKey(node), key)
				|| Equals(NodeHeaderTemplateKey(node), key); });
	};
	if (usedBy(candidate.Nodes))
		return Fail(L"文档控件仍引用 DataTemplate：" + key, outError);
	for (const auto& item : candidate.DataTemplates)
		if (!Equals(item.Key, key) && usedBy(item.Template))
			return Fail(L"DataTemplate " + item.Key
				+ L" 仍引用 DataTemplate：" + key, outError);
	for (const auto& style : candidate.GroupStyles)
		if (Equals(style.HeaderTemplate, key))
			return Fail(L"GroupStyle " + style.Key
				+ L" 仍引用 DataTemplate：" + key, outError);
	candidate.DataTemplates.erase(found);
	return CommitCandidate(document, std::move(candidate), outError);
}
}
