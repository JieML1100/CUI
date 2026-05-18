#pragma once

#include <string>
#include <string_view>

namespace System::Xml {

class XmlResolver {
public:
    virtual ~XmlResolver() = default;
    virtual std::string ResolveUri(std::string_view baseUri, std::string_view relativeUri) const;
    virtual std::string GetEntity(std::string_view absoluteUri) const;
};

class XmlUrlResolver final : public XmlResolver {
public:
    std::string ResolveUri(std::string_view baseUri, std::string_view relativeUri) const override;
    std::string GetEntity(std::string_view absoluteUri) const override;
};

}  // namespace System::Xml
