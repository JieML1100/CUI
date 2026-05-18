#include "../include/XmlNamespaceManager.h"

namespace System::Xml {

XmlNamespaceManager::XmlNamespaceManager() {
    scopes_.push_back({});
    scopes_.back().emplace("xml", "http://www.w3.org/XML/1998/namespace");
    scopes_.back().emplace("xmlns", "http://www.w3.org/2000/xmlns/");
}

void XmlNamespaceManager::PushScope() {
    scopes_.push_back({});
}

bool XmlNamespaceManager::PopScope() {
    if (scopes_.size() <= 1) {
        return false;
    }

    scopes_.pop_back();
    return true;
}

void XmlNamespaceManager::AddNamespace(std::string_view prefix, std::string_view uri) {
    scopes_.back()[std::string(prefix)] = std::string(uri);
}

std::string XmlNamespaceManager::LookupNamespace(std::string_view prefix) const {
    if (prefix == "xml") {
        return "http://www.w3.org/XML/1998/namespace";
    }
    if (prefix == "xmlns") {
        return "http://www.w3.org/2000/xmlns/";
    }

    const std::string prefixStr(prefix);
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        const auto found = it->find(prefixStr);
        if (found != it->end()) {
            return found->second;
        }
    }

    return {};
}

std::string XmlNamespaceManager::LookupPrefix(std::string_view uri) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        const auto found = std::find_if(it->begin(), it->end(), [&uri](const auto& pair) {
            return pair.second == uri;
        });
        if (found != it->end()) {
            return found->first;
        }
    }

    return {};
}

bool XmlNamespaceManager::HasNamespace(std::string_view prefix) const {
    return !LookupNamespace(prefix).empty();
}

}  // namespace System::Xml
