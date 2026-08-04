#pragma once

#include "../../CUI/include/Binding.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if !CUI_ENABLE_DYNAMIC_XAML
#error BindingConverterRegistry is available only in the Design runtime.
#endif

/** Discoverable converter metadata used by runtime registration and design tools. */
struct BindingValueConverterMetadata
{
	std::wstring Name;
	/** Empty means that the converter accepts any source kind. */
	BindingValueKind SourceKind = BindingValueKind::Empty;
	/** Empty means that the converter can target any property kind. */
	BindingValueKind TargetKind = BindingValueKind::Empty;
	bool CanConvertBack = true;

	bool operator==(const BindingValueConverterMetadata&) const = default;
};

/**
 * Process-wide named converter registry. Built-in converters are always present;
 * applications may register custom factories before Design materialization.
 */
class BindingValueConverterRegistry final
{
public:
	using Factory = std::function<std::shared_ptr<const IBindingValueConverter>()>;

	static bool Register(
		BindingValueConverterMetadata metadata,
		Factory factory,
		bool replaceExisting = false);
	/** Removes a custom registration. Built-in registrations cannot be removed. */
	static bool Unregister(const std::wstring& name);
	static std::optional<BindingValueConverterMetadata> Find(const std::wstring& name);
	static std::vector<BindingValueConverterMetadata> GetConverters();
	static std::shared_ptr<const IBindingValueConverter> Create(const std::wstring& name);
};

struct MultiBindingValueConverterMetadata
{
	std::wstring Name;
	std::size_t MinimumInputCount = 2;
	BindingValueKind TargetKind = BindingValueKind::Empty;
	bool CanConvertBack = false;

	bool operator==(const MultiBindingValueConverterMetadata&) const = default;
};

class MultiBindingValueConverterRegistry final
{
public:
	using Factory = std::function<
		std::shared_ptr<const IMultiBindingValueConverter>()>;

	static bool Register(
		MultiBindingValueConverterMetadata metadata,
		Factory factory,
		bool replaceExisting = false);
	static bool Unregister(const std::wstring& name);
	static std::optional<MultiBindingValueConverterMetadata> Find(
		const std::wstring& name);
	static std::vector<MultiBindingValueConverterMetadata> GetConverters();
	static std::shared_ptr<const IMultiBindingValueConverter> Create(
		const std::wstring& name);
};
