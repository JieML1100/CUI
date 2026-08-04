#include "../include/BindingConverterRegistry.h"

#include <algorithm>
#include <cwctype>
#include <mutex>
#include <utility>

#if !CUI_ENABLE_DYNAMIC_XAML
#error BindingConverterRegistry.Design.cpp requires the Design runtime flavor.
#endif

namespace
{
	std::wstring Trim(std::wstring value)
	{
		auto isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
		value.erase(value.begin(), std::find_if(
			value.begin(), value.end(),
			[&](wchar_t ch) { return !isSpace(ch); }));
		value.erase(std::find_if(
			value.rbegin(), value.rend(),
			[&](wchar_t ch) { return !isSpace(ch); }).base(), value.end());
		return value;
	}

	bool IsSameProperty(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	bool IsPropertyNameLess(const std::wstring& left, const std::wstring& right)
	{
		const auto common = (std::min)(left.size(), right.size());
		for (std::size_t index = 0; index < common; ++index)
		{
			const auto leftCharacter = std::towlower(left[index]);
			const auto rightCharacter = std::towlower(right[index]);
			if (leftCharacter != rightCharacter)
				return leftCharacter < rightCharacter;
		}
		return left.size() < right.size();
	}

	struct ConverterRegistryEntry
	{
		BindingValueConverterMetadata Metadata;
		BindingValueConverterRegistry::Factory Factory;
	};

	const std::vector<ConverterRegistryEntry>& BuiltInBindingConverters()
	{
		static const std::vector<ConverterRegistryEntry> entries = {
			{
				{ L"BooleanNegation", BindingValueKind::Bool, BindingValueKind::Bool, true },
				[] { return GetBuiltInBindingValueConverter(
					BuiltInBindingValueConverter::BooleanNegation); }
			},
			{
				{ L"StringIsNotEmpty", BindingValueKind::String, BindingValueKind::Bool, false },
				[] { return GetBuiltInBindingValueConverter(
					BuiltInBindingValueConverter::StringIsNotEmpty); }
			},
			{
				{ L"StringTrim", BindingValueKind::String, BindingValueKind::String, true },
				[] { return GetBuiltInBindingValueConverter(
					BuiltInBindingValueConverter::StringTrim); }
			}
		};
		return entries;
	}

	std::vector<ConverterRegistryEntry>& RegisteredBindingConverters()
	{
		static std::vector<ConverterRegistryEntry> entries;
		return entries;
	}

	std::mutex& BindingConverterMutex()
	{
		static std::mutex mutex;
		return mutex;
	}

	struct MultiConverterRegistryEntry
	{
		MultiBindingValueConverterMetadata Metadata;
		MultiBindingValueConverterRegistry::Factory Factory;
	};

	std::vector<MultiConverterRegistryEntry>& RegisteredMultiBindingConverters()
	{
		static std::vector<MultiConverterRegistryEntry> entries;
		return entries;
	}

	std::mutex& MultiBindingConverterMutex()
	{
		static std::mutex mutex;
		return mutex;
	}
}

bool BindingValueConverterRegistry::Register(
	BindingValueConverterMetadata metadata,
	Factory factory,
	bool replaceExisting)
{
	metadata.Name = Trim(std::move(metadata.Name));
	if (metadata.Name.empty() || !factory) return false;

	std::lock_guard<std::mutex> lock(BindingConverterMutex());
	auto& registered = RegisteredBindingConverters();
	auto existing = std::find_if(registered.begin(), registered.end(),
		[&](const ConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, metadata.Name);
		});
	if (existing != registered.end())
	{
		if (!replaceExisting) return false;
		existing->Metadata = std::move(metadata);
		existing->Factory = std::move(factory);
		return true;
	}

	const auto& builtIns = BuiltInBindingConverters();
	const bool shadowsBuiltIn = std::any_of(builtIns.begin(), builtIns.end(),
		[&](const ConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, metadata.Name);
		});
	if (shadowsBuiltIn && !replaceExisting) return false;

	registered.push_back({ std::move(metadata), std::move(factory) });
	return true;
}

bool BindingValueConverterRegistry::Unregister(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return false;

	std::lock_guard<std::mutex> lock(BindingConverterMutex());
	auto& registered = RegisteredBindingConverters();
	auto existing = std::find_if(registered.begin(), registered.end(),
		[&](const ConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, normalized);
		});
	if (existing == registered.end()) return false;
	registered.erase(existing);
	return true;
}

std::optional<BindingValueConverterMetadata> BindingValueConverterRegistry::Find(
	const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return std::nullopt;

	{
		std::lock_guard<std::mutex> lock(BindingConverterMutex());
		for (const auto& entry : RegisteredBindingConverters())
		{
			if (IsSameProperty(entry.Metadata.Name, normalized))
				return entry.Metadata;
		}
	}
	for (const auto& entry : BuiltInBindingConverters())
	{
		if (IsSameProperty(entry.Metadata.Name, normalized))
			return entry.Metadata;
	}
	return std::nullopt;
}

std::vector<BindingValueConverterMetadata>
BindingValueConverterRegistry::GetConverters()
{
	std::vector<BindingValueConverterMetadata> result;
	for (const auto& entry : BuiltInBindingConverters())
		result.push_back(entry.Metadata);

	{
		std::lock_guard<std::mutex> lock(BindingConverterMutex());
		for (const auto& entry : RegisteredBindingConverters())
		{
			auto existing = std::find_if(result.begin(), result.end(),
				[&](const BindingValueConverterMetadata& metadata)
				{
					return IsSameProperty(metadata.Name, entry.Metadata.Name);
				});
			if (existing == result.end()) result.push_back(entry.Metadata);
			else *existing = entry.Metadata;
		}
	}

	std::sort(result.begin(), result.end(),
		[](const auto& left, const auto& right)
		{
			return IsPropertyNameLess(left.Name, right.Name);
		});
	return result;
}

std::shared_ptr<const IBindingValueConverter>
BindingValueConverterRegistry::Create(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return {};

	Factory factory;
	{
		std::lock_guard<std::mutex> lock(BindingConverterMutex());
		for (const auto& entry : RegisteredBindingConverters())
		{
			if (IsSameProperty(entry.Metadata.Name, normalized))
			{
				factory = entry.Factory;
				break;
			}
		}
	}
	if (!factory)
	{
		for (const auto& entry : BuiltInBindingConverters())
		{
			if (IsSameProperty(entry.Metadata.Name, normalized))
			{
				factory = entry.Factory;
				break;
			}
		}
	}
	if (!factory) return {};
	try
	{
		return factory();
	}
	catch (...)
	{
		return {};
	}
}

bool MultiBindingValueConverterRegistry::Register(
	MultiBindingValueConverterMetadata metadata,
	Factory factory,
	bool replaceExisting)
{
	metadata.Name = Trim(std::move(metadata.Name));
	if (metadata.Name.empty() || metadata.MinimumInputCount == 0 || !factory)
		return false;
	std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
	auto& entries = RegisteredMultiBindingConverters();
	const auto found = std::find_if(entries.begin(), entries.end(),
		[&](const MultiConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, metadata.Name);
		});
	if (found != entries.end())
	{
		if (!replaceExisting) return false;
		found->Metadata = std::move(metadata);
		found->Factory = std::move(factory);
		return true;
	}
	entries.push_back({ std::move(metadata), std::move(factory) });
	return true;
}

bool MultiBindingValueConverterRegistry::Unregister(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return false;
	std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
	auto& entries = RegisteredMultiBindingConverters();
	const auto found = std::find_if(entries.begin(), entries.end(),
		[&](const MultiConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, normalized);
		});
	if (found == entries.end()) return false;
	entries.erase(found);
	return true;
}

std::optional<MultiBindingValueConverterMetadata>
MultiBindingValueConverterRegistry::Find(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return std::nullopt;
	std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
	for (const auto& entry : RegisteredMultiBindingConverters())
		if (IsSameProperty(entry.Metadata.Name, normalized))
			return entry.Metadata;
	return std::nullopt;
}

std::vector<MultiBindingValueConverterMetadata>
MultiBindingValueConverterRegistry::GetConverters()
{
	std::vector<MultiBindingValueConverterMetadata> result;
	{
		std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
		for (const auto& entry : RegisteredMultiBindingConverters())
			result.push_back(entry.Metadata);
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return IsPropertyNameLess(left.Name, right.Name);
	});
	return result;
}

std::shared_ptr<const IMultiBindingValueConverter>
MultiBindingValueConverterRegistry::Create(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return {};
	Factory factory;
	{
		std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
		for (const auto& entry : RegisteredMultiBindingConverters())
			if (IsSameProperty(entry.Metadata.Name, normalized))
			{
				factory = entry.Factory;
				break;
			}
	}
	if (!factory) return {};
	try { return factory(); }
	catch (...) { return {}; }
}
