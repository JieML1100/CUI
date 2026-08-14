#include "DesignerDataContextSchemaUtils.h"
#include "../CUI/include/BindingList.h"
#include <algorithm>
#include <cwctype>
#include <exception>
#include <functional>
#include <unordered_set>

namespace DesignerDataContextSchemaUtils
{
namespace
{
	std::wstring Trim(const std::wstring& value)
	{
		size_t begin = 0;
		while (begin < value.size() && std::iswspace(value[begin])) ++begin;
		size_t end = value.size();
		while (end > begin && std::iswspace(value[end - 1])) --end;
		return value.substr(begin, end - begin);
	}

	std::wstring Lower(const std::wstring& value)
	{
		std::wstring result = value;
		std::transform(result.begin(), result.end(), result.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return result;
	}

	bool EqualsToken(const std::wstring& left, const std::wstring& right)
	{
		return Lower(left) == Lower(right);
	}
}

std::wstring NormalizePath(const std::wstring& path)
{
	std::wstring result;
	size_t start = 0;
	while (start <= path.size())
	{
		const size_t separator = path.find(L'.', start);
		const size_t end = separator == std::wstring::npos ? path.size() : separator;
		if (!result.empty()) result += L'.';
		result += Trim(path.substr(start, end - start));
		if (separator == std::wstring::npos) break;
		start = separator + 1;
	}
	return result;
}

bool IsValidPath(const std::wstring& path)
{
	if (path.empty()) return false;
	size_t start = 0;
	while (start <= path.size())
	{
		const size_t separator = path.find(L'.', start);
		const size_t end = separator == std::wstring::npos ? path.size() : separator;
		if (Trim(path.substr(start, end - start)).empty()) return false;
		if (separator == std::wstring::npos) return true;
		start = separator + 1;
	}
	return false;
}

const wchar_t* ValueKindName(BindingValueKind kind) noexcept
{
	switch (kind)
	{
	case BindingValueKind::Empty: return L"Unknown";
	case BindingValueKind::Bool: return L"Bool";
	case BindingValueKind::NullableBool: return L"NullableBool";
	case BindingValueKind::Int: return L"Int";
	case BindingValueKind::Int64: return L"Int64";
	case BindingValueKind::Float: return L"Float";
	case BindingValueKind::Double: return L"Double";
	case BindingValueKind::String: return L"String";
	case BindingValueKind::Object: return L"Object";
	}
	return L"Unknown";
}

bool TryParseValueKind(const std::wstring& text, BindingValueKind& kind)
{
	const auto value = Trim(text);
	for (const auto candidate : {
		BindingValueKind::Empty,
		BindingValueKind::Bool,
		BindingValueKind::NullableBool,
		BindingValueKind::Int,
		BindingValueKind::Int64,
		BindingValueKind::Float,
		BindingValueKind::Double,
		BindingValueKind::String,
		BindingValueKind::Object })
	{
		if (EqualsToken(value, ValueKindName(candidate)))
		{
			kind = candidate;
			return true;
		}
	}
	if (EqualsToken(value, L"Empty"))
	{
		kind = BindingValueKind::Empty;
		return true;
	}
	return false;
}

const wchar_t* ObjectKindName(DesignerDataObjectKind kind) noexcept
{
	switch (kind)
	{
	case DesignerDataObjectKind::Opaque: return L"Opaque";
	case DesignerDataObjectKind::BindingSource: return L"BindingSource";
	case DesignerDataObjectKind::BindingList: return L"BindingList";
	}
	return L"Opaque";
}

bool TryParseObjectKind(
	const std::wstring& text, DesignerDataObjectKind& kind)
{
	const auto value = Trim(text);
	for (const auto candidate : {
		DesignerDataObjectKind::Opaque,
		DesignerDataObjectKind::BindingSource,
		DesignerDataObjectKind::BindingList })
	{
		if (EqualsToken(value, ObjectKindName(candidate)))
		{
			kind = candidate;
			return true;
		}
	}
	return false;
}

DesignerDataObjectKind ObjectKindForValueType(
	const std::type_index& type) noexcept
{
	if (type == std::type_index(typeid(BindingListReference)))
		return DesignerDataObjectKind::BindingList;
	if (type == std::type_index(typeid(BindingSourceReference)))
		return DesignerDataObjectKind::BindingSource;
	return DesignerDataObjectKind::Opaque;
}

const DesignerDataContextProperty* Find(
	const DesignerDataContextSchema& schema,
	const std::wstring& path)
{
	const auto normalized = NormalizePath(path);
	const auto it = std::find_if(schema.begin(), schema.end(),
		[&](const DesignerDataContextProperty& property)
		{
			return NormalizePath(property.Path) == normalized;
		});
	return it == schema.end() ? nullptr : &*it;
}

std::vector<std::wstring> GetPaths(const DesignerDataContextSchema& schema)
{
	std::vector<std::wstring> result;
	result.reserve(schema.size());
	for (const auto& property : schema)
		result.push_back(NormalizePath(property.Path));
	std::sort(result.begin(), result.end());
	return result;
}

void Canonicalize(DesignerDataContextSchema& schema)
{
	for (auto& property : schema)
	{
		property.Path = NormalizePath(property.Path);
		property.ItemType = Trim(property.ItemType);
		property.DataType = Trim(property.DataType);
		if (property.ValueKind != BindingValueKind::Object)
		{
			property.ObjectKind = DesignerDataObjectKind::Opaque;
			property.ItemType.clear();
			property.DataType.clear();
		}
		else if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			property.DataType.clear();
		else if (property.ObjectKind != DesignerDataObjectKind::BindingSource)
		{
			property.ItemType.clear();
			property.DataType.clear();
		}
	}
	for (auto& property : schema)
	{
		if (property.ValueKind != BindingValueKind::Object
			|| property.ObjectKind != DesignerDataObjectKind::Opaque)
			continue;
		const auto prefix = property.Path + L".";
		if (std::any_of(schema.begin(), schema.end(),
			[&](const DesignerDataContextProperty& candidate)
			{
				return candidate.Path.starts_with(prefix);
			}))
			property.ObjectKind = DesignerDataObjectKind::BindingSource;
	}
	std::sort(schema.begin(), schema.end(),
		[](const auto& left, const auto& right)
		{
			return left.Path < right.Path;
		});
}

bool Validate(const DesignerDataContextSchema& schema, std::wstring* outError)
{
	for (size_t i = 0; i < schema.size(); ++i)
	{
		const auto path = NormalizePath(schema[i].Path);
		if (!IsValidPath(path))
		{
			if (outError) *outError = L"DataContext 属性路径无效。";
			return false;
		}
		if (schema[i].ValueKind < BindingValueKind::Empty
			|| schema[i].ValueKind > BindingValueKind::NullableBool)
		{
			if (outError) *outError = L"DataContext 属性 " + path + L" 的值类型无效。";
			return false;
		}
		if (schema[i].ValueKind != BindingValueKind::Object
			&& schema[i].ObjectKind != DesignerDataObjectKind::Opaque)
		{
			if (outError) *outError = L"DataContext 属性 " + path
				+ L" 不是 Object，不能声明对象契约。";
			return false;
		}
		if (schema[i].ObjectKind == DesignerDataObjectKind::BindingList
			&& Trim(schema[i].ItemType).empty())
		{
			if (outError) *outError = L"DataContext 集合属性 " + path
				+ L" 必须声明 ItemType。";
			return false;
		}
		if (schema[i].ObjectKind != DesignerDataObjectKind::BindingList
			&& !Trim(schema[i].ItemType).empty())
		{
			if (outError) *outError = L"DataContext 属性 " + path
				+ L" 只有 BindingList 才能声明 ItemType。";
			return false;
		}
		if (schema[i].ObjectKind != DesignerDataObjectKind::BindingSource
			&& !Trim(schema[i].DataType).empty())
		{
			if (outError) *outError = L"DataContext 属性 " + path
				+ L" 只有 BindingSource 才能声明 DataType。";
			return false;
		}

		for (size_t j = 0; j < i; ++j)
		{
			const auto otherPath = NormalizePath(schema[j].Path);
			if (path == otherPath)
			{
				if (outError) *outError = L"DataContext 属性路径重复：" + path;
				return false;
			}
		}
	}

	for (const auto& parent : schema)
	{
		const auto parentPath = NormalizePath(parent.Path);
		const auto prefix = parentPath + L".";
		const bool hasChildren = std::any_of(schema.begin(), schema.end(),
			[&](const DesignerDataContextProperty& candidate)
			{
				const auto candidatePath = NormalizePath(candidate.Path);
				return candidatePath.starts_with(prefix);
			});
		if (hasChildren
			&& parent.ValueKind != BindingValueKind::Object
			&& parent.ValueKind != BindingValueKind::Empty)
		{
			if (outError) *outError = L"DataContext 属性 " + parentPath
				+ L" 包含子路径，因此类型必须为 Object 或 Unknown。";
			return false;
		}
		if (hasChildren
			&& parent.ValueKind == BindingValueKind::Object
			&& parent.ObjectKind != DesignerDataObjectKind::BindingSource)
		{
			if (outError) *outError = L"DataContext 属性 " + parentPath
				+ L" 包含子路径，因此 ObjectType 必须为 BindingSource。";
			return false;
		}
	}

	if (outError) outError->clear();
	return true;
}

std::wstring Describe(const DesignerDataContextProperty& property)
{
	std::wstring capabilities;
	if (property.CanRead) capabilities += L"R";
	if (property.CanWrite) capabilities += L"W";
	if (property.CanObserve) capabilities += L"O";
	if (capabilities.empty()) capabilities = L"-";
	auto type = std::wstring(ValueKindName(property.ValueKind));
	if (property.ValueKind == BindingValueKind::Object)
	{
		type += L"/" + std::wstring(ObjectKindName(property.ObjectKind));
		if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			type += L"<" + property.ItemType + L">";
		else if (property.ObjectKind == DesignerDataObjectKind::BindingSource
			&& !property.DataType.empty())
			type += L"<" + property.DataType + L">";
	}
	return NormalizePath(property.Path) + L" : " + type
		+ L"  [" + capabilities + L"]";
}

bool BuildFromBindingSource(
	const IBindingSource& source,
	DesignerDataContextSchema& schema,
	std::wstring* outError,
	size_t maxDepth)
{
	if (maxDepth == 0)
	{
		if (outError) *outError = L"DataContext 元数据发现深度必须大于 0。";
		return false;
	}

	DesignerDataContextSchema discovered;
	std::unordered_set<const IBindingSource*> activeSources;
	bool rootHasMetadata = false;
	std::function<bool(const IBindingSource&, const std::wstring&, size_t)> visit;
	visit = [&](const IBindingSource& current, const std::wstring& prefix, size_t depth)
	{
		const auto properties = current.GetProperties();
		if (depth == 0) rootHasMetadata = !properties.empty();
		if (!activeSources.insert(&current).second) return true;

		for (const auto& metadata : properties)
		{
			const auto name = NormalizePath(metadata.Name);
			if (!IsValidPath(name) || name.find(L'.') != std::wstring::npos)
			{
				if (outError) *outError = L"运行时源包含无法导入的属性名：" + metadata.Name;
				activeSources.erase(&current);
				return false;
			}

			DesignerDataContextProperty property;
			property.Path = prefix.empty() ? name : prefix + L"." + name;
			property.ValueKind = metadata.ValueKind;
			property.CanRead = metadata.CanRead;
			property.CanWrite = metadata.CanWrite;
			property.CanObserve = metadata.CanObserve;
			property.ObjectKind = ObjectKindForValueType(metadata.ValueType);
			if (metadata.ValueKind == BindingValueKind::Object
				&& metadata.CanRead)
			{
				BindingValue value;
				if (current.TryGetValue(metadata.Name, value))
				{
					BindingListReference list;
					BindingSourceReference nested;
					if (value.TryGet(list) && list)
					{
						property.ObjectKind = DesignerDataObjectKind::BindingList;
						property.ItemType = list.Get()->ItemTypeName();
					}
					else if (value.TryGet(nested) && nested)
					{
						property.ObjectKind = DesignerDataObjectKind::BindingSource;
					}
				}
			}
			discovered.push_back(std::move(property));

			if (depth + 1 >= maxDepth || metadata.ValueKind != BindingValueKind::Object
				|| !metadata.CanRead
				|| discovered.back().ObjectKind != DesignerDataObjectKind::BindingSource)
				continue;

			BindingValue value;
			BindingSourceReference reference;
			if (current.TryGetValue(metadata.Name, value)
				&& value.TryGet(reference)
				&& reference
				&& activeSources.find(reference.Get()) == activeSources.end())
			{
				const auto nestedPrefix = discovered.back().Path;
				if (!visit(*reference.Get(), nestedPrefix, depth + 1))
				{
					activeSources.erase(&current);
					return false;
				}
			}
		}

		activeSources.erase(&current);
		return true;
	};

	try
	{
		if (!visit(source, L"", 0)) return false;
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"读取运行时数据源元数据失败："
			+ std::wstring(exception.what(), exception.what() + std::char_traits<char>::length(exception.what()));
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"读取运行时数据源元数据失败。";
		return false;
	}
	if (!rootHasMetadata)
	{
		if (outError) *outError = L"运行时数据源没有公开可发现的属性元数据。";
		return false;
	}

	Canonicalize(discovered);
	if (!Validate(discovered, outError)) return false;
	schema = std::move(discovered);
	if (outError) outError->clear();
	return true;
}
}
