#include "DesignerDataContextSchemaUtils.h"
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

	bool EqualsIgnoreCase(const std::wstring& left, const std::wstring& right)
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
		BindingValueKind::Int,
		BindingValueKind::Int64,
		BindingValueKind::Float,
		BindingValueKind::Double,
		BindingValueKind::String,
		BindingValueKind::Object })
	{
		if (EqualsIgnoreCase(value, ValueKindName(candidate)))
		{
			kind = candidate;
			return true;
		}
	}
	if (EqualsIgnoreCase(value, L"Empty"))
	{
		kind = BindingValueKind::Empty;
		return true;
	}
	return false;
}

const DesignerDataContextProperty* Find(
	const DesignerDataContextSchema& schema,
	const std::wstring& path)
{
	const auto normalized = NormalizePath(path);
	const auto it = std::find_if(schema.begin(), schema.end(),
		[&](const DesignerDataContextProperty& property)
		{
			return EqualsIgnoreCase(NormalizePath(property.Path), normalized);
		});
	return it == schema.end() ? nullptr : &*it;
}

std::vector<std::wstring> GetPaths(const DesignerDataContextSchema& schema)
{
	std::vector<std::wstring> result;
	result.reserve(schema.size());
	for (const auto& property : schema)
		result.push_back(NormalizePath(property.Path));
	std::sort(result.begin(), result.end(),
		[](const auto& left, const auto& right) { return Lower(left) < Lower(right); });
	return result;
}

void Canonicalize(DesignerDataContextSchema& schema)
{
	for (auto& property : schema)
		property.Path = NormalizePath(property.Path);
	std::sort(schema.begin(), schema.end(),
		[](const auto& left, const auto& right)
		{
			return Lower(left.Path) < Lower(right.Path);
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
			|| schema[i].ValueKind > BindingValueKind::Object)
		{
			if (outError) *outError = L"DataContext 属性 " + path + L" 的值类型无效。";
			return false;
		}

		for (size_t j = 0; j < i; ++j)
		{
			const auto otherPath = NormalizePath(schema[j].Path);
			if (EqualsIgnoreCase(path, otherPath))
			{
				if (outError) *outError = L"DataContext 属性路径重复：" + path;
				return false;
			}
		}
	}

	for (const auto& parent : schema)
	{
		const auto parentPath = NormalizePath(parent.Path);
		const auto prefix = Lower(parentPath + L".");
		const bool hasChildren = std::any_of(schema.begin(), schema.end(),
			[&](const DesignerDataContextProperty& candidate)
			{
				const auto candidatePath = Lower(NormalizePath(candidate.Path));
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
	return NormalizePath(property.Path) + L" : " + ValueKindName(property.ValueKind)
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
			discovered.push_back(std::move(property));

			if (depth + 1 >= maxDepth || metadata.ValueKind != BindingValueKind::Object
				|| !metadata.CanRead)
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
