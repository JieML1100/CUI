#include "RuntimeTypeMetadata.h"

#include <functional>

std::size_t RuntimeTypeIdHash::operator()(
	const RuntimeTypeId& value) const noexcept
{
	const auto namespaceHash = std::hash<std::wstring>{}(value.NamespaceUri);
	const auto nameHash = std::hash<std::wstring>{}(value.LocalName);
	return namespaceHash ^ (nameHash + static_cast<std::size_t>(0x9e3779b9)
		+ (namespaceHash << 6) + (namespaceHash >> 2));
}
