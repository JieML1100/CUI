#include "../include/XmlUtilityInternal.h"

namespace System::Xml {

std::string XmlResolver::ResolveUri(std::string_view baseUri, std::string_view relativeUri) const {
    if (relativeUri.empty()) {
        return std::string(baseUri);
    }
    if (baseUri.empty()) {
        return std::string(relativeUri);
    }
    return (std::filesystem::path(std::string(baseUri)).parent_path() / std::filesystem::path(std::string(relativeUri))).lexically_normal().string();
}

std::string XmlResolver::GetEntity(std::string_view absoluteUri) const {
    const std::string uriStr(absoluteUri);
    std::ifstream stream(std::filesystem::path(uriStr), std::ios::binary);
    if (!stream) {
        throw XmlException("Failed to resolve XML entity: " + uriStr);
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::string XmlUrlResolver::ResolveUri(std::string_view baseUri, std::string_view relativeUri) const {
    return XmlResolver::ResolveUri(baseUri, relativeUri);
}

std::string XmlUrlResolver::GetEntity(std::string_view absoluteUri) const {
    return XmlResolver::GetEntity(absoluteUri);
}

}  // namespace System::Xml
