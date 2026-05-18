#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace System::Xml {

class XmlNamespaceManager final {
public:
    XmlNamespaceManager();

    void PushScope();
    bool PopScope();
    void AddNamespace(std::string_view prefix, std::string_view uri);
    std::string LookupNamespace(std::string_view prefix) const;
    std::string LookupPrefix(std::string_view uri) const;
    bool HasNamespace(std::string_view prefix) const;

private:
    std::vector<std::unordered_map<std::string, std::string>> scopes_;
};

}  // namespace System::Xml
