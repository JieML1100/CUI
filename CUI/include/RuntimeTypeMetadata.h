#pragma once

#include "Binding.h"
#include "CuiBuildFeatures.h"
#include "Event.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

/**
 * Stable, allocation-free identity for an AOT data-record type.
 *
 * Zero intentionally means "unspecified" so an untyped template remains a
 * valid catch-all.  Production presentation paths compare only this token;
 * Design may retain the originating name for diagnostics.
 */
struct DataTypeToken final
{
	std::uint64_t Value = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return Value != 0;
	}
	constexpr bool operator==(const DataTypeToken&) const noexcept = default;
};

[[nodiscard]] constexpr DataTypeToken MakeDataTypeToken(
	std::wstring_view canonicalName) noexcept
{
	return { MakeBindingSourcePropertyToken(canonicalName).Value };
}

[[nodiscard]] constexpr bool AreDataTypesCompatible(
	DataTypeToken actual,
	DataTypeToken expected) noexcept
{
	return !actual || !expected || actual == expected;
}

/**
 * Stable, allocation-free identity for one compiled ComponentDefinition type.
 *
 * Zero means "not a component". Production compares only this token; the
 * originating XAML QName remains a Design-side diagnostic value. The AOT
 * compiler rejects distinct QNames that hash to the same token.
 */
struct ComponentTypeToken final
{
	std::uint64_t Value = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return Value != 0;
	}
	constexpr bool operator==(const ComponentTypeToken&) const noexcept = default;
};

/** 64-bit FNV-1a over the expanded namespace URI and local name. */
[[nodiscard]] constexpr ComponentTypeToken MakeComponentTypeToken(
	std::wstring_view namespaceUri,
	std::wstring_view localName) noexcept
{
	if (namespaceUri.empty() || localName.empty()) return {};
	std::uint64_t hash = 14695981039346656037ull;
	auto append = [&hash](std::uint32_t codeUnit) constexpr
	{
		for (unsigned shift = 0; shift != 32; shift += 8)
		{
			hash ^= static_cast<std::uint8_t>(codeUnit >> shift);
			hash *= 1099511628211ull;
		}
	};
	for (const wchar_t character : namespaceUri)
		append(static_cast<std::uint32_t>(character));
	// This marker cannot be confused with a wchar_t code unit and therefore
	// keeps the two QName segments structurally distinct without concatenating.
	append(0xffffffffu);
	for (const wchar_t character : localName)
		append(static_cast<std::uint32_t>(character));
	return { hash == 0 ? 1ull : hash };
}

/**
 * Canonical runtime identity of a compiled framework or component type.
 * Namespace prefixes are document syntax and never enter runtime identity.
 */
struct RuntimeTypeId final
{
	std::wstring NamespaceUri;
	std::wstring LocalName;

	bool Empty() const noexcept
	{
		return NamespaceUri.empty() && LocalName.empty();
	}

	bool Valid() const noexcept
	{
		return !NamespaceUri.empty() && !LocalName.empty();
	}

	std::wstring RegistryKey() const
	{
		return NamespaceUri + L"|" + LocalName;
	}

	bool operator==(const RuntimeTypeId&) const = default;
};

struct RuntimeTypeIdHash final
{
	std::size_t operator()(const RuntimeTypeId& value) const noexcept;
};

using DeclarativeEventRoutingStrategy = RoutedEventRoutingStrategy;

/** Small compiled event contract; dynamic XAML descriptors reuse this shape. */
struct DeclarativeEventDefinition final
{
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring Name;
#endif
	BindingValueKind PayloadKind = BindingValueKind::Empty;
	DeclarativeEventRoutingStrategy RoutingStrategy =
		DeclarativeEventRoutingStrategy::Direct;

	constexpr DeclarativeEventDefinition() noexcept = default;
	constexpr DeclarativeEventDefinition(
		BindingValueKind payloadKind,
		DeclarativeEventRoutingStrategy routingStrategy) noexcept
		: PayloadKind(payloadKind), RoutingStrategy(routingStrategy)
	{
	}
#if CUI_ENABLE_DYNAMIC_XAML
	DeclarativeEventDefinition(
		std::wstring name,
		BindingValueKind payloadKind,
		DeclarativeEventRoutingStrategy routingStrategy)
		: Name(std::move(name)),
		  PayloadKind(payloadKind),
		  RoutingStrategy(routingStrategy)
	{
	}
#endif
};
