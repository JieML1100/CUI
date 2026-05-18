#include "../include/XmlInternal.h"

namespace System::Xml {

void XmlSchemaSet::AddXml(std::string_view xml) {
    static const std::string kSchemaNamespace = "http://www.w3.org/2001/XMLSchema";

    const auto document = XmlDocument::Parse(xml);
    const auto root = document->DocumentElement();
    if (root == nullptr || root->LocalName() != "schema" || root->NamespaceURI() != kSchemaNamespace) {
        throw XmlException("XmlSchemaSet::AddXml requires an XML Schema document");
    }

    const std::string targetNamespace = root->GetAttribute("targetNamespace");

    const auto makeQualifiedRuleKey = [](const std::string& localName, const std::string& namespaceUri) {
        return namespaceUri + "\n" + localName;
    };

    std::unordered_map<std::string, const XmlElement*> declaredSimpleTypes;
    std::unordered_map<std::string, const XmlElement*> declaredComplexTypes;
    std::unordered_map<std::string, const XmlElement*> declaredElements;
    std::unordered_map<std::string, const XmlElement*> declaredGroups;
    std::unordered_map<std::string, const XmlElement*> declaredAttributes;
    std::unordered_map<std::string, const XmlElement*> declaredAttributeGroups;
    std::unordered_set<std::string> resolvingSimpleTypes;
    std::unordered_set<std::string> resolvingComplexTypes;
    std::unordered_set<std::string> resolvingElements;
    std::unordered_set<std::string> resolvedLocalSimpleTypes;
    std::unordered_set<std::string> resolvedLocalComplexTypes;
    std::unordered_set<std::string> resolvedLocalElements;
    std::unordered_set<std::string> resolvingGroups;
    std::unordered_set<std::string> resolvingAttributes;
    std::unordered_set<std::string> resolvingAttributeGroups;
    std::unordered_map<std::string, Particle> resolvedGroups;
    std::unordered_map<std::string, AttributeUse> resolvedAttributes;
    std::unordered_map<std::string, std::vector<AttributeUse>> resolvedAttributeGroups;
    std::unordered_map<std::string, bool> resolvedAttributeGroupAnyAllowed;
    std::unordered_map<std::string, std::string> resolvedAttributeGroupAnyNamespace;
    std::unordered_map<std::string, std::string> resolvedAttributeGroupAnyProcessContents;

    for (const auto& child : root->ChildNodes()) {
        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
            continue;
        }
        const auto* childElement = static_cast<const XmlElement*>(child.get());
        if (childElement->NamespaceURI() != kSchemaNamespace) {
            continue;
        }

        if (childElement->LocalName() == "simpleType") {
            const std::string name = childElement->GetAttribute("name");
            if (name.empty()) {
                throw XmlException("Top-level XML Schema simpleType declarations require a name");
            }
            declaredSimpleTypes.emplace(makeQualifiedRuleKey(name, targetNamespace), childElement);
        } else if (childElement->LocalName() == "complexType") {
            const std::string name = childElement->GetAttribute("name");
            if (name.empty()) {
                throw XmlException("Top-level XML Schema complexType declarations require a name");
            }
            declaredComplexTypes.emplace(makeQualifiedRuleKey(name, targetNamespace), childElement);
        } else if (childElement->LocalName() == "element") {
            const std::string name = childElement->GetAttribute("name");
            if (name.empty()) {
                throw XmlException("Top-level XML Schema element declarations require a name");
            }
            declaredElements.emplace(makeQualifiedRuleKey(name, targetNamespace), childElement);
        } else if (childElement->LocalName() == "group") {
            const std::string name = childElement->GetAttribute("name");
            if (name.empty()) {
                throw XmlException("Top-level XML Schema group declarations require a name");
            }
            declaredGroups.emplace(makeQualifiedRuleKey(name, targetNamespace), childElement);
        } else if (childElement->LocalName() == "attribute") {
            const std::string name = childElement->GetAttribute("name");
            if (name.empty()) {
                throw XmlException("Top-level XML Schema attribute declarations require a name");
            }
            declaredAttributes.emplace(makeQualifiedRuleKey(name, targetNamespace), childElement);
        } else if (childElement->LocalName() == "attributeGroup") {
            const std::string name = childElement->GetAttribute("name");
            if (name.empty()) {
                throw XmlException("Top-level XML Schema attributeGroup declarations require a name");
            }
            declaredAttributeGroups.emplace(makeQualifiedRuleKey(name, targetNamespace), childElement);
        }
    }

    auto parseOccurs = [](const std::string& value, std::size_t defaultValue) {
        if (value.empty()) {
            return defaultValue;
        }
        if (value == "unbounded") {
            return std::numeric_limits<std::size_t>::max();
        }
        return static_cast<std::size_t>(std::stoull(value));
    };

    const auto parseProcessContentsValue = [](const XmlElement& element) {
        std::string_view processContents;
        if (!TryGetAttributeValueViewInternal(element, "processContents", processContents) || processContents.empty()) {
            return std::string("strict");
        }
        if (processContents == "strict" || processContents == "lax" || processContents == "skip") {
            return std::string(processContents);
        }

        throw XmlException("XML Schema wildcard processContents must be one of strict, lax, or skip");
    };

    const auto parseWildcardNamespaceConstraintValue = [](const XmlElement& element) {
        std::string value = element.GetAttribute("namespace");
        if (value.empty()) {
            return std::string("##any");
        }

        std::istringstream tokens(value);
        std::string token;
        while (tokens >> token) {
            if (token.rfind("##", 0) == 0
                && token != "##any"
                && token != "##other"
                && token != "##targetNamespace"
                && token != "##local") {
                throw XmlException("XML Schema wildcard namespace must use only ##any, ##other, ##targetNamespace, ##local, or explicit namespace URIs");
            }
        }

        return value;
    };

    const auto parseSchemaBooleanAttribute = [](const XmlElement& element, const std::string& attributeName) {
        std::string_view value;
        if (!TryGetAttributeValueViewInternal(element, attributeName, value) || value.empty()) {
            return false;
        }
        if (value == "true" || value == "1") {
            return true;
        }
        if (value == "false" || value == "0") {
            return false;
        }

        throw XmlException("XML Schema attribute '" + attributeName + "' must be one of true, false, 1, or 0");
    };

    const auto parseFormValue = [](const std::string& value, const std::string& label) {
        if (value.empty()) {
            return value;
        }
        if (value == "qualified" || value == "unqualified") {
            return value;
        }

        throw XmlException("XML Schema " + label + " must be 'qualified' or 'unqualified'");
    };

    const auto parseAttributeUseValue = [](const XmlElement& element) {
        std::string_view use;
        if (!TryGetAttributeValueViewInternal(element, "use", use) || use.empty() || use == "optional") {
            return std::string("optional");
        }
        if (use == "required") {
            return std::string("required");
        }

        throw XmlException("XML Schema attribute use must be 'optional' or 'required'");
    };

    const auto validateAnnotationOnlySchemaChildren = [](const XmlElement& element, const std::string& label) {
        for (const auto& child : element.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() == kSchemaNamespace && childElement->LocalName() != "annotation") {
                throw XmlException("XML Schema " + label + " can only declare annotation children");
            }
        }
    };

    const auto parseAnnotation = [&](const XmlElement& annotatedElement) {
        XmlSchemaSet::Annotation annotation;
        for (const auto& child : annotatedElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace || childElement->LocalName() != "annotation") {
                continue;
            }

            for (const auto& annotationChild : childElement->ChildNodes()) {
                if (annotationChild == nullptr || annotationChild->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* annotationChildElement = static_cast<const XmlElement*>(annotationChild.get());
                if (annotationChildElement->NamespaceURI() != kSchemaNamespace) {
                    continue;
                }

                XmlSchemaSet::AnnotationEntry entry;
                entry.Source = annotationChildElement->GetAttribute("source");
                entry.Language = annotationChildElement->GetAttribute("xml:lang");
                entry.Content = annotationChildElement->InnerXml();

                if (annotationChildElement->LocalName() == "appinfo") {
                    annotation.AppInfo.push_back(std::move(entry));
                } else if (annotationChildElement->LocalName() == "documentation") {
                    annotation.Documentation.push_back(std::move(entry));
                } else {
                    throw XmlException("XML Schema annotation can only declare appinfo or documentation children");
                }
            }
        }
        return annotation;
    };

    const std::string schemaFinalDefault = root->GetAttribute("finalDefault");
    const std::string schemaBlockDefault = root->GetAttribute("blockDefault");
    const std::string schemaElementFormDefault = parseFormValue(root->GetAttribute("elementFormDefault"), "elementFormDefault");
    const std::string schemaAttributeFormDefault = parseFormValue(root->GetAttribute("attributeFormDefault"), "attributeFormDefault");

    const auto validateParticleOccursRange = [](std::size_t minOccurs, std::size_t maxOccurs) {
        if (maxOccurs != std::numeric_limits<std::size_t>::max() && minOccurs > maxOccurs) {
            throw XmlException("XML Schema particles cannot declare minOccurs greater than maxOccurs");
        }
    };

    const auto validateDeclaredDefaultOrFixedValue = [&](const auto& type,
                                                         const std::optional<std::string>& value,
                                                         const std::string& valueKind,
                                                         const std::string& declarationKind,
                                                         const std::string& declarationName,
                                                         const XmlElement& schemaElement) {
        if (!type.has_value() || !value.has_value()) {
            return;
        }

        ValidateSchemaSimpleValue(
            *type,
            *value,
            valueKind + " value of " + declarationKind + " '" + declarationName + "'",
            &schemaElement);
    };

    const auto upsertRule = [this](ElementRule rule) {
        auto found = std::find_if(elements_.begin(), elements_.end(), [&](const auto& existing) {
            return existing.name == rule.name && existing.namespaceUri == rule.namespaceUri;
        });
        if (found == elements_.end()) {
            elements_.push_back(std::move(rule));
        } else {
            *found = std::move(rule);
        }
    };

    const auto upsertSimpleType = [this](NamedSimpleTypeRule typeRule) {
        auto found = std::find_if(simpleTypes_.begin(), simpleTypes_.end(), [&](const auto& existing) {
            return existing.name == typeRule.name && existing.namespaceUri == typeRule.namespaceUri;
        });
        if (found == simpleTypes_.end()) {
            simpleTypes_.push_back(std::move(typeRule));
        } else {
            *found = std::move(typeRule);
        }
    };

    const auto upsertComplexType = [this](NamedComplexTypeRule typeRule) {
        auto found = std::find_if(complexTypes_.begin(), complexTypes_.end(), [&](const auto& existing) {
            return existing.name == typeRule.name && existing.namespaceUri == typeRule.namespaceUri;
        });
        if (found == complexTypes_.end()) {
            complexTypes_.push_back(std::move(typeRule));
        } else {
            *found = std::move(typeRule);
        }
    };

    const auto upsertAttribute = [this](NamedAttributeRule attributeRule) {
        auto found = std::find_if(attributes_.begin(), attributes_.end(), [&](const auto& existing) {
            return existing.name == attributeRule.name && existing.namespaceUri == attributeRule.namespaceUri;
        });
        if (found == attributes_.end()) {
            attributes_.push_back(std::move(attributeRule));
        } else {
            *found = std::move(attributeRule);
        }
    };

    const auto upsertGroup = [this](NamedGroupRule groupRule) {
        auto found = std::find_if(groupRules_.begin(), groupRules_.end(), [&](const auto& existing) {
            return existing.name == groupRule.name && existing.namespaceUri == groupRule.namespaceUri;
        });
        if (found == groupRules_.end()) {
            groupRules_.push_back(std::move(groupRule));
        } else {
            *found = std::move(groupRule);
        }
    };

    const auto upsertAttributeGroup = [this](NamedAttributeGroupRule attributeGroupRule) {
        auto found = std::find_if(attributeGroupRules_.begin(), attributeGroupRules_.end(), [&](const auto& existing) {
            return existing.name == attributeGroupRule.name && existing.namespaceUri == attributeGroupRule.namespaceUri;
        });
        if (found == attributeGroupRules_.end()) {
            attributeGroupRules_.push_back(std::move(attributeGroupRule));
        } else {
            *found = std::move(attributeGroupRule);
        }
    };

    const auto upsertSchemaAnnotation = [this](NamedSchemaAnnotation schemaAnnotation) {
        auto found = std::find_if(schemaAnnotations_.begin(), schemaAnnotations_.end(), [&](const auto& existing) {
            return existing.namespaceUri == schemaAnnotation.namespaceUri;
        });
        if (found == schemaAnnotations_.end()) {
            schemaAnnotations_.push_back(std::move(schemaAnnotation));
        } else {
            *found = std::move(schemaAnnotation);
        }
    };

    const auto upsertGroupAnnotation = [this](NamedGroupAnnotation groupAnnotation) {
        auto found = std::find_if(groups_.begin(), groups_.end(), [&](const auto& existing) {
            return existing.name == groupAnnotation.name && existing.namespaceUri == groupAnnotation.namespaceUri;
        });
        if (found == groups_.end()) {
            groups_.push_back(std::move(groupAnnotation));
        } else {
            *found = std::move(groupAnnotation);
        }
    };

    const auto upsertAttributeGroupAnnotation = [this](NamedAttributeGroupAnnotation groupAnnotation) {
        auto found = std::find_if(attributeGroups_.begin(), attributeGroups_.end(), [&](const auto& existing) {
            return existing.name == groupAnnotation.name && existing.namespaceUri == groupAnnotation.namespaceUri;
        });
        if (found == attributeGroups_.end()) {
            attributeGroups_.push_back(std::move(groupAnnotation));
        } else {
            *found = std::move(groupAnnotation);
        }
    };

    const auto upsertIdentityConstraint = [this](ElementRule::IdentityConstraint constraint) {
        auto found = std::find_if(identityConstraints_.begin(), identityConstraints_.end(), [&](const auto& existing) {
            return existing.name == constraint.name && existing.namespaceUri == constraint.namespaceUri;
        });
        if (found != identityConstraints_.end()) {
            throw XmlException("XML Schema identity constraint '" + constraint.name + "' is declared more than once");
        }
        identityConstraints_.push_back(std::move(constraint));
    };

    const auto resolveTypeName = [&](const XmlElement& context, const std::string& qualifiedName) {
        std::pair<std::string, std::string> resolved;
        if (qualifiedName.empty()) {
            return resolved;
        }

        const auto separator = qualifiedName.find(':');
        if (separator == std::string::npos) {
            resolved.first = qualifiedName;
            resolved.second = targetNamespace;
            return resolved;
        }

        const std::string prefix = qualifiedName.substr(0, separator);
        resolved.first = qualifiedName.substr(separator + 1);
        resolved.second = context.GetNamespaceOfPrefix(prefix);
        return resolved;
    };

    upsertSchemaAnnotation(NamedSchemaAnnotation{targetNamespace, parseAnnotation(*root)});

    const auto resolveBuiltinSimpleType = [](const std::string& qualifiedName) -> std::optional<SimpleTypeRule> {
        const auto descriptor = ResolveBuiltinSimpleTypeDescriptor(qualifiedName);
        if (!descriptor.has_value()) {
            return std::nullopt;
        }

        SimpleTypeRule rule;
        rule.baseType = descriptor->baseType;
        rule.whiteSpace = descriptor->whiteSpace;
        if (descriptor->variety == BuiltinSimpleTypeDescriptor::Variety::List) {
            rule.variety = SimpleTypeRule::Variety::List;

            SimpleTypeRule itemType;
            itemType.baseType = descriptor->itemType;
            itemType.whiteSpace = descriptor->itemWhiteSpace;
            rule.itemType = std::make_shared<SimpleTypeRule>(itemType);
        }
        return rule;
    };

    const auto resolveBuiltinComplexType = [](const std::string& qualifiedName) -> std::optional<ComplexTypeRule> {
        if (qualifiedName != "xs:anyType") {
            return std::nullopt;
        }

        ComplexTypeRule rule;
        rule.namedTypeName = "anyType";
        rule.namedTypeNamespaceUri = "http://www.w3.org/2001/XMLSchema";
        rule.allowsText = true;
        rule.anyAttributeAllowed = true;
        rule.anyAttributeNamespaceConstraint = "##any";
        rule.anyAttributeProcessContents = "lax";

        Particle particle;
        particle.kind = Particle::Kind::Any;
        particle.namespaceUri = "##any";
        particle.processContents = "lax";
        particle.minOccurs = 0;
        particle.maxOccurs = std::numeric_limits<std::size_t>::max();
        rule.particle = particle;
        return rule;
    };

    const auto containsDerivationToken = [](const std::string& value, const std::string& token) {
        if (value == "#all") {
            return true;
        }

        std::istringstream stream(value);
        std::string current;
        while (stream >> current) {
            if (current == token) {
                return true;
            }
        }
        return false;
    };

    const auto effectiveDerivationControlValue = [](const std::string& localValue, const std::string& schemaDefaultValue) {
        return localValue.empty() ? schemaDefaultValue : localValue;
    };

    const auto wildcardNamespaceTokenMatches = [&](const std::string& token, const std::string& namespaceUri) {
        if (token.empty() || token == "##any") {
            return true;
        }
        if (token == "##other") {
            return namespaceUri != targetNamespace;
        }
        if (token == "##targetNamespace") {
            return namespaceUri == targetNamespace;
        }
        if (token == "##local") {
            return namespaceUri.empty();
        }
        return namespaceUri == token;
    };

    const auto wildcardTokenIsSubsetOfConstraint = [&](const std::string& token, const std::string& baseConstraint) {
        const std::string normalizedBase = baseConstraint.empty() ? "##any" : baseConstraint;
        if (normalizedBase == "##any") {
            return true;
        }
        if (token == "##any") {
            return normalizedBase == "##any";
        }
        if (token == "##other") {
            std::istringstream baseTokens(normalizedBase);
            std::string baseToken;
            while (baseTokens >> baseToken) {
                if (baseToken == "##other" || baseToken == "##any") {
                    return true;
                }
            }
            return false;
        }
        const std::string namespaceUri = token == "##targetNamespace" ? targetNamespace : (token == "##local" ? std::string{} : token);
        std::istringstream baseTokens(normalizedBase);
        std::string baseToken;
        while (baseTokens >> baseToken) {
            if (wildcardNamespaceTokenMatches(baseToken, namespaceUri)) {
                return true;
            }
        }
        return false;
    };

    std::function<SimpleTypeRule(const std::string&, const std::string&, const std::string&)> ensureSimpleTypeResolved;
    std::function<ComplexTypeRule(const std::string&, const std::string&, const std::string&)> ensureComplexTypeResolved;
    std::function<ElementRule(const std::string&, const std::string&, const std::string&)> ensureElementResolved;
    std::function<Particle(const std::string&, const std::string&, const std::string&)> ensureGroupResolved;
    std::function<AttributeUse(const std::string&, const std::string&, const std::string&)> ensureAttributeResolved;
    std::function<std::vector<AttributeUse>(const std::string&, const std::string&, const std::string&)> ensureAttributeGroupResolved;
    std::function<bool(const std::string&, const std::string&)> matchesWildcardNamespace;

    const auto ensureSimpleTypeFinalAllowsDerivation = [&](const SimpleTypeRule& baseRule,
        const std::string& qualifiedName,
        const std::string& derivationMethod,
        const std::string& description) {
        const bool blocked = (derivationMethod == "restriction" && baseRule.finalRestriction)
            || (derivationMethod == "list" && baseRule.finalList)
            || (derivationMethod == "union" && baseRule.finalUnion);
        if (!blocked) {
            return;
        }

        throw XmlException(
            "XML Schema " + description + " base '" + qualifiedName + "' blocks "
            + derivationMethod + " derivation via simpleType final");
    };

    const auto resolveSimpleTypeReference = [&](const XmlElement& context, const std::string& qualifiedName) -> SimpleTypeRule {
        if (const auto builtinType = resolveBuiltinSimpleType(qualifiedName); builtinType.has_value()) {
            return *builtinType;
        }

        const auto [localTypeName, typeNamespaceUri] = resolveTypeName(context, qualifiedName);
        return ensureSimpleTypeResolved(localTypeName, typeNamespaceUri, qualifiedName);
    };

    const auto resolveElementDeclarationNamespace = [&](const XmlElement& element, bool isGlobalDeclaration) {
        if (isGlobalDeclaration) {
            return targetNamespace;
        }

        const std::string form = parseFormValue(element.GetAttribute("form"), "element form");
        if (form.empty() && schemaElementFormDefault.empty()) {
            return targetNamespace;
        }
        const std::string effectiveForm = form.empty() ? schemaElementFormDefault : form;
        return effectiveForm == "qualified" ? targetNamespace : std::string{};
    };

    const auto resolveAttributeDeclarationNamespace = [&](const XmlElement& attribute, bool isGlobalDeclaration) {
        if (isGlobalDeclaration) {
            return targetNamespace;
        }

        const std::string form = parseFormValue(attribute.GetAttribute("form"), "attribute form");
        const std::string effectiveForm = form.empty() ? schemaAttributeFormDefault : form;
        return effectiveForm == "qualified" ? targetNamespace : std::string{};
    };

    const auto isSimpleTypeFacetElement = [&](const XmlElement& facetElement) {
        if (facetElement.NamespaceURI() != kSchemaNamespace) {
            return false;
        }

        const std::string& localName = facetElement.LocalName();
        return localName == "enumeration"
            || localName == "whiteSpace"
            || localName == "pattern"
            || localName == "length"
            || localName == "minLength"
            || localName == "maxLength"
            || localName == "minInclusive"
            || localName == "maxInclusive"
            || localName == "minExclusive"
            || localName == "maxExclusive"
            || localName == "totalDigits"
            || localName == "fractionDigits";
    };

    const auto applyFacetToSimpleType = [&](SimpleTypeRule& rule, const XmlElement& facetElement) {
        validateAnnotationOnlySchemaChildren(facetElement, "simpleType facet");
        const std::string& localName = facetElement.LocalName();
        if (localName == "enumeration") {
            rule.enumerationValues.push_back(facetElement.GetAttribute("value"));
        } else if (localName == "whiteSpace") {
            const std::string value = facetElement.GetAttribute("value");
            if (value != "preserve" && value != "replace" && value != "collapse") {
                throw XmlException("XML Schema whiteSpace facet value must be 'preserve', 'replace', or 'collapse'");
            }
            rule.whiteSpace = value;
        } else if (localName == "pattern") {
            rule.pattern = facetElement.GetAttribute("value");
        } else if (localName == "length") {
            rule.length = static_cast<std::size_t>(std::stoull(facetElement.GetAttribute("value")));
        } else if (localName == "minLength") {
            rule.minLength = static_cast<std::size_t>(std::stoull(facetElement.GetAttribute("value")));
        } else if (localName == "maxLength") {
            rule.maxLength = static_cast<std::size_t>(std::stoull(facetElement.GetAttribute("value")));
        } else if (localName == "minInclusive") {
            rule.minInclusive = facetElement.GetAttribute("value");
        } else if (localName == "maxInclusive") {
            rule.maxInclusive = facetElement.GetAttribute("value");
        } else if (localName == "minExclusive") {
            rule.minExclusive = facetElement.GetAttribute("value");
        } else if (localName == "maxExclusive") {
            rule.maxExclusive = facetElement.GetAttribute("value");
        } else if (localName == "totalDigits") {
            rule.totalDigits = static_cast<std::size_t>(std::stoull(facetElement.GetAttribute("value")));
        } else if (localName == "fractionDigits") {
            rule.fractionDigits = static_cast<std::size_t>(std::stoull(facetElement.GetAttribute("value")));
        }

        if (rule.length.has_value() && (rule.minLength.has_value() || rule.maxLength.has_value())) {
            throw XmlException("XML Schema simpleType cannot declare length together with minLength or maxLength facets");
        }
        if (rule.minLength.has_value() && rule.maxLength.has_value() && *rule.minLength > *rule.maxLength) {
            throw XmlException("XML Schema simpleType cannot declare minLength greater than maxLength");
        }
        if (rule.totalDigits.has_value() && rule.fractionDigits.has_value() && *rule.fractionDigits > *rule.totalDigits) {
            throw XmlException("XML Schema simpleType cannot declare fractionDigits greater than totalDigits");
        }
    };

    std::function<SimpleTypeRule(const XmlElement&)> parseSimpleType;
    std::function<ComplexTypeRule(const XmlElement&)> parseComplexType;
    std::function<ElementRule(const XmlElement&, bool)> parseElementRule;
    std::function<AttributeUse(const XmlElement&, bool)> parseAttributeUse;
    std::function<std::vector<AttributeUse>(const XmlElement&, bool&, std::string&, std::string&)> parseAttributeGroup;
    std::function<ElementRule::IdentityConstraint(const XmlElement&)> parseIdentityConstraint;
    std::function<void(const SimpleTypeRule&, const SimpleTypeRule&, const std::string&)> validateDerivedSimpleTypeRestriction;
    const auto tryCompileIdentityConstraintXPath = [](const std::string& xpath,
                                                      const std::vector<std::pair<std::string, std::string>>& namespaceBindings,
                                                      const bool allowAttributeTerminal)
        -> std::optional<ElementRule::IdentityConstraint::CompiledPath> {
        using CompiledPath = ElementRule::IdentityConstraint::CompiledPath;
        using CompiledPathStep = ElementRule::IdentityConstraint::CompiledPathStep;

        const auto lookupNamespaceUriInBindings = [&](const std::string& prefix) {
            for (auto it = namespaceBindings.rbegin(); it != namespaceBindings.rend(); ++it) {
                if (it->first == prefix) {
                    return it->second;
                }
            }
            return std::string{};
        };

        const auto trim = [](std::string_view text) {
            std::size_t start = 0;
            while (start < text.size() && IsWhitespace(text[start])) {
                ++start;
            }

            std::size_t end = text.size();
            while (end > start && IsWhitespace(text[end - 1])) {
                --end;
            }

            return std::string(text.substr(start, end - start));
        };

        const auto parseQualifiedName = [&](const std::string& qualifiedName,
                                            std::string& localName,
                                            std::string& namespaceUri,
                                            const bool allowWildcard) {
            const auto [prefix, parsedLocalName] = SplitQualifiedName(qualifiedName);
            if (parsedLocalName.empty()) {
                return false;
            }
            if ((!allowWildcard || parsedLocalName != "*") && parsedLocalName.find('*') != std::string::npos) {
                return false;
            }

            try {
                if (!prefix.empty()) {
                    (void)XmlConvert::VerifyNCName(prefix);
                }
                if (parsedLocalName != "*") {
                    (void)XmlConvert::VerifyNCName(parsedLocalName);
                }
            } catch (const XmlException&) {
                return false;
            }

            localName = parsedLocalName;
            namespaceUri.clear();
            if (!prefix.empty()) {
                namespaceUri = lookupNamespaceUriInBindings(prefix);
                if (namespaceUri.empty()) {
                    return false;
                }
            }
            return true;
        };

        if (xpath.empty()) {
            return std::nullopt;
        }

        CompiledPath compiledPath;
        std::size_t position = 0;
        bool nextStepIsDescendant = false;
        if (xpath.size() >= 2 && xpath[0] == '/' && xpath[1] == '/') {
            nextStepIsDescendant = true;
            position = 2;
            if (position >= xpath.size()) {
                return std::nullopt;
            }
        } else if (!xpath.empty() && xpath[0] == '/') {
            return std::nullopt;
        }
        while (position <= xpath.size()) {
            std::size_t segmentEnd = position;
            while (segmentEnd < xpath.size() && xpath[segmentEnd] != '/') {
                ++segmentEnd;
            }
            const std::string segment = xpath.substr(position, segmentEnd - position);
            if (segment.empty()) {
                return std::nullopt;
            }

            const bool isLastSegment = segmentEnd == xpath.size();
            if (segment == ".") {
                if (nextStepIsDescendant) {
                    return std::nullopt;
                }
                compiledPath.steps.push_back(CompiledPathStep{CompiledPathStep::Kind::Self, {}, {}});
            } else {
                CompiledPathStep step;
                std::string qualifiedName = segment;
                if (!segment.empty() && segment.front() == '@') {
                    if (!allowAttributeTerminal || !isLastSegment || segment.size() == 1) {
                        return std::nullopt;
                    }
                    step.kind = CompiledPathStep::Kind::Attribute;
                    qualifiedName = segment.substr(1);
                } else {
                    constexpr std::string_view selfAxisPrefix = "self::";
                    constexpr std::string_view childAxisPrefix = "child::";
                    constexpr std::string_view attributeAxisPrefix = "attribute::";
                    constexpr std::string_view descendantAxisPrefix = "descendant::";
                    constexpr std::string_view descendantOrSelfAxisPrefix = "descendant-or-self::";
                    constexpr std::string_view parentAxisPrefix = "parent::";
                    constexpr std::string_view ancestorAxisPrefix = "ancestor::";
                    constexpr std::string_view ancestorOrSelfAxisPrefix = "ancestor-or-self::";
                    constexpr std::string_view followingSiblingAxisPrefix = "following-sibling::";
                    constexpr std::string_view precedingSiblingAxisPrefix = "preceding-sibling::";
                    constexpr std::string_view followingAxisPrefix = "following::";
                    constexpr std::string_view precedingAxisPrefix = "preceding::";
                    bool allowPredicateOnStep = false;
                    if (segment.rfind(selfAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Self;
                        qualifiedName = segment.substr(selfAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                    } else if (segment.rfind(childAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Element;
                        qualifiedName = segment.substr(childAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else if (segment.rfind(attributeAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant || !allowAttributeTerminal || !isLastSegment) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Attribute;
                        qualifiedName = segment.substr(attributeAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                    } else if (segment.rfind(descendantOrSelfAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::DescendantOrSelfElement;
                        qualifiedName = segment.substr(descendantOrSelfAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else if (segment.rfind(descendantAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::DescendantElement;
                        qualifiedName = segment.substr(descendantAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else if (segment.rfind(parentAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Parent;
                        qualifiedName = segment.substr(parentAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                    } else if (segment.rfind(ancestorAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Ancestor;
                        qualifiedName = segment.substr(ancestorAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                    } else if (segment.rfind(ancestorOrSelfAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::AncestorOrSelf;
                        qualifiedName = segment.substr(ancestorOrSelfAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                    } else if (segment.rfind(followingSiblingAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::FollowingSibling;
                        qualifiedName = segment.substr(followingSiblingAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else if (segment.rfind(precedingSiblingAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::PrecedingSibling;
                        qualifiedName = segment.substr(precedingSiblingAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else if (segment.rfind(followingAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Following;
                        qualifiedName = segment.substr(followingAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else if (segment.rfind(precedingAxisPrefix, 0) == 0) {
                        if (nextStepIsDescendant) {
                            return std::nullopt;
                        }
                        step.kind = CompiledPathStep::Kind::Preceding;
                        qualifiedName = segment.substr(precedingAxisPrefix.size());
                        if (qualifiedName.empty()) {
                            return std::nullopt;
                        }
                        allowPredicateOnStep = true;
                    } else {
                        step.kind = nextStepIsDescendant
                            ? CompiledPathStep::Kind::DescendantElement
                            : CompiledPathStep::Kind::Element;
                        allowPredicateOnStep = true;

                        const auto predicateStart = segment.find('[');
                        if (predicateStart != std::string::npos) {
                            if (segment.back() != ']'
                                || segment.find('[', predicateStart + 1) != std::string::npos
                                || segment.find(']', predicateStart + 1) != segment.size() - 1) {
                                return std::nullopt;
                            }

                            qualifiedName = segment.substr(0, predicateStart);
                        }
                    }

                    if (allowPredicateOnStep) {
                        const auto predicateStart = qualifiedName.find('[');
                        if (predicateStart != std::string::npos) {
                            return std::nullopt;
                        }

                        const auto segmentPredicateStart = segment.find('[');
                        if (segmentPredicateStart != std::string::npos) {
                            if (segment.back() != ']'
                                || segment.find('[', segmentPredicateStart + 1) != std::string::npos
                                || segment.find(']', segmentPredicateStart + 1) != segment.size() - 1) {
                                return std::nullopt;
                            }

                            const std::string predicate = trim(std::string_view(segment).substr(segmentPredicateStart + 1, segment.size() - segmentPredicateStart - 2));
                            const std::size_t equalsPosition = predicate.find('=');
                            if (qualifiedName.empty()
                                || equalsPosition == std::string::npos
                                || predicate.find('=', equalsPosition + 1) != std::string::npos) {
                                return std::nullopt;
                            }

                            const std::string left = trim(std::string_view(predicate).substr(0, equalsPosition));
                            const std::string right = trim(std::string_view(predicate).substr(equalsPosition + 1));
                            if (left.size() <= 1 || left.front() != '@' || right.size() < 2) {
                                return std::nullopt;
                            }

                            const char quote = right.front();
                            if ((quote != '\'' && quote != '"') || right.back() != quote) {
                                return std::nullopt;
                            }

                            if (!parseQualifiedName(left.substr(1), step.predicateAttributeLocalName, step.predicateAttributeNamespaceUri, false)) {
                                return std::nullopt;
                            }
                            step.predicateAttributeValue = right.substr(1, right.size() - 2);
                            qualifiedName = segment.substr(0, segmentPredicateStart);
                            const auto axisDelimiter = qualifiedName.find("::");
                            if (axisDelimiter != std::string::npos) {
                                qualifiedName = qualifiedName.substr(axisDelimiter + 2);
                            }
                        }
                    }
                }

                if (qualifiedName.find_first_of("()|") != std::string::npos
                    || qualifiedName.find("::") != std::string::npos) {
                    return std::nullopt;
                }

                if (!parseQualifiedName(qualifiedName, step.localName, step.namespaceUri, true)) {
                    return std::nullopt;
                }

                compiledPath.steps.push_back(std::move(step));
            }

            nextStepIsDescendant = false;
            if (segmentEnd == xpath.size()) {
                break;
            }

            if (xpath[segmentEnd] != '/') {
                return std::nullopt;
            }

            std::size_t nextPosition = segmentEnd + 1;
            if (nextPosition < xpath.size() && xpath[nextPosition] == '/') {
                nextStepIsDescendant = true;
                ++nextPosition;
            }
            if (nextPosition >= xpath.size()) {
                return std::nullopt;
            }
            position = nextPosition;
        }

        return compiledPath.steps.empty() ? std::nullopt : std::optional<CompiledPath>(std::move(compiledPath));
    };

    parseSimpleType = [&](const XmlElement& simpleTypeElement) -> SimpleTypeRule {
        const std::string effectiveFinal = effectiveDerivationControlValue(simpleTypeElement.GetAttribute("final"), schemaFinalDefault);
        const bool finalRestriction = containsDerivationToken(effectiveFinal, "restriction");
        const bool finalList = containsDerivationToken(effectiveFinal, "list");
        const bool finalUnion = containsDerivationToken(effectiveFinal, "union");
        const auto applySimpleTypeFinalFlags = [&](SimpleTypeRule& target) {
            target.finalRestriction = finalRestriction;
            target.finalList = finalList;
            target.finalUnion = finalUnion;
        };

        const XmlSchemaSet::Annotation declarationAnnotation = parseAnnotation(simpleTypeElement);
        SimpleTypeRule rule;
        applySimpleTypeFinalFlags(rule);
        rule.annotation = declarationAnnotation;
        rule.namedTypeName.clear();
        rule.namedTypeNamespaceUri.clear();
        rule.derivationMethod = SimpleTypeRule::DerivationMethod::None;
        rule.derivationBaseName.clear();
        rule.derivationBaseNamespaceUri.clear();

        std::size_t derivationChildCount = 0;
        for (const auto& child : simpleTypeElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }
            if (childElement->LocalName() == "restriction"
                || childElement->LocalName() == "list"
                || childElement->LocalName() == "union") {
                ++derivationChildCount;
            }
        }

        if (derivationChildCount == 0) {
            throw XmlException("XML Schema simpleType requires exactly one restriction, list, or union child");
        }
        if (derivationChildCount > 1) {
            throw XmlException("XML Schema simpleType can only declare one restriction, list, or union child");
        }

        for (const auto& child : simpleTypeElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }

            if (childElement->LocalName() == "restriction") {
                const std::string base = childElement->GetAttribute("base");
                std::optional<SimpleTypeRule> restrictionBaseRule;
                std::string restrictionBaseLabel;
                if (!base.empty()) {
                    if (const auto builtinType = resolveBuiltinSimpleType(base); builtinType.has_value()) {
                        rule = *builtinType;
                        restrictionBaseRule = *builtinType;
                        restrictionBaseLabel = base;
                        applySimpleTypeFinalFlags(rule);
                        rule.namedTypeName.clear();
                        rule.namedTypeNamespaceUri.clear();
                        rule.derivationMethod = SimpleTypeRule::DerivationMethod::Restriction;
                        rule.derivationBaseName.clear();
                        rule.derivationBaseNamespaceUri.clear();
                    } else {
                        const auto [baseLocalName, baseNamespaceUri] = resolveTypeName(*childElement, base);
                        const SimpleTypeRule baseRule = resolveSimpleTypeReference(*childElement, base);
                        ensureSimpleTypeFinalAllowsDerivation(baseRule, base, "restriction", "simpleType restriction");
                        rule = baseRule;
                        restrictionBaseRule = baseRule;
                        restrictionBaseLabel = base;
                        applySimpleTypeFinalFlags(rule);
                        rule.namedTypeName.clear();
                        rule.namedTypeNamespaceUri.clear();
                        rule.derivationMethod = SimpleTypeRule::DerivationMethod::Restriction;
                        rule.derivationBaseName = baseLocalName;
                        rule.derivationBaseNamespaceUri = baseNamespaceUri;
                    }
                }
                for (const auto& restrictionChild : childElement->ChildNodes()) {
                    if (restrictionChild == nullptr || restrictionChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* facet = static_cast<const XmlElement*>(restrictionChild.get());
                    if (facet->LocalName() == "simpleType" && facet->NamespaceURI() == kSchemaNamespace) {
                        rule = parseSimpleType(*facet);
                        restrictionBaseRule = rule;
                        restrictionBaseLabel = "inline simpleType";
                        applySimpleTypeFinalFlags(rule);
                    } else if (isSimpleTypeFacetElement(*facet)) {
                        applyFacetToSimpleType(rule, *facet);
                    } else if (facet->NamespaceURI() == kSchemaNamespace
                        && facet->LocalName() != "annotation") {
                        throw XmlException("XML Schema simpleType restriction can only declare annotation, simpleType, and facet children");
                    }
                }

                if (!restrictionBaseRule.has_value()) {
                    throw XmlException("XML Schema simpleType restriction requires a base attribute or inline simpleType");
                }
                validateDerivedSimpleTypeRestriction(*restrictionBaseRule, rule,
                    "simpleType restriction base '" + restrictionBaseLabel + "'");
                rule.annotation = declarationAnnotation;
                return rule;
            }

            if (childElement->LocalName() == "list") {
                rule.variety = SimpleTypeRule::Variety::List;
                rule.baseType = "xs:string";
                rule.whiteSpace = "collapse";
                rule.namedTypeName.clear();
                rule.namedTypeNamespaceUri.clear();
                rule.derivationMethod = SimpleTypeRule::DerivationMethod::List;
                rule.derivationBaseName.clear();
                rule.derivationBaseNamespaceUri.clear();

                const std::string itemType = childElement->GetAttribute("itemType");
                if (!itemType.empty()) {
                    const SimpleTypeRule itemTypeRule = resolveSimpleTypeReference(*childElement, itemType);
                    ensureSimpleTypeFinalAllowsDerivation(itemTypeRule, itemType, "list", "list simpleType");
                    rule.itemType = std::make_shared<SimpleTypeRule>(itemTypeRule);
                }

                std::size_t inlineSimpleTypeCount = 0;
                for (const auto& listChild : childElement->ChildNodes()) {
                    if (listChild == nullptr || listChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* listChildElement = static_cast<const XmlElement*>(listChild.get());
                    if (listChildElement->LocalName() == "simpleType" && listChildElement->NamespaceURI() == kSchemaNamespace) {
                        ++inlineSimpleTypeCount;
                        if (inlineSimpleTypeCount > 1) {
                            throw XmlException("XML Schema list simpleType can only declare one inline simpleType child");
                        }
                        if (!itemType.empty()) {
                            throw XmlException("XML Schema list simpleType cannot combine an itemType attribute with an inline simpleType child");
                        }
                        rule.itemType = std::make_shared<SimpleTypeRule>(parseSimpleType(*listChildElement));
                    } else if (listChildElement->NamespaceURI() == kSchemaNamespace
                        && listChildElement->LocalName() != "annotation") {
                        throw XmlException("XML Schema list simpleType can only declare annotation and an optional inline simpleType child");
                    }
                }

                if (rule.itemType == nullptr) {
                    throw XmlException("XML Schema list simpleType requires an itemType");
                }
                rule.annotation = declarationAnnotation;
                return rule;
            }

            if (childElement->LocalName() == "union") {
                rule.variety = SimpleTypeRule::Variety::Union;
                rule.baseType = "xs:string";
                rule.namedTypeName.clear();
                rule.namedTypeNamespaceUri.clear();
                rule.derivationMethod = SimpleTypeRule::DerivationMethod::Union;
                rule.derivationBaseName.clear();
                rule.derivationBaseNamespaceUri.clear();

                const std::string memberTypes = childElement->GetAttribute("memberTypes");
                if (!memberTypes.empty()) {
                    std::istringstream members(memberTypes);
                    std::string memberTypeName;
                    while (members >> memberTypeName) {
                        const SimpleTypeRule memberTypeRule = resolveSimpleTypeReference(*childElement, memberTypeName);
                        ensureSimpleTypeFinalAllowsDerivation(memberTypeRule, memberTypeName, "union", "union simpleType");
                        rule.memberTypes.push_back(memberTypeRule);
                    }
                }

                std::size_t inlineSimpleTypeCount = 0;
                for (const auto& unionChild : childElement->ChildNodes()) {
                    if (unionChild == nullptr || unionChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* unionChildElement = static_cast<const XmlElement*>(unionChild.get());
                    if (unionChildElement->LocalName() == "simpleType" && unionChildElement->NamespaceURI() == kSchemaNamespace) {
                        ++inlineSimpleTypeCount;
                        if (!memberTypes.empty()) {
                            throw XmlException("XML Schema union simpleType cannot combine a memberTypes attribute with inline simpleType children");
                        }
                        rule.memberTypes.push_back(parseSimpleType(*unionChildElement));
                    } else if (unionChildElement->NamespaceURI() == kSchemaNamespace
                        && unionChildElement->LocalName() != "annotation") {
                        throw XmlException("XML Schema union simpleType can only declare annotation and optional inline simpleType children");
                    }
                }

                if (rule.memberTypes.empty()) {
                    throw XmlException("XML Schema union simpleType requires at least one member type");
                }
                rule.annotation = declarationAnnotation;
                return rule;
            }
        }
        rule.annotation = declarationAnnotation;
        return rule;
    };

    parseIdentityConstraint = [&](const XmlElement& constraintElement) -> ElementRule::IdentityConstraint {
        ElementRule::IdentityConstraint constraint;
        if (constraintElement.LocalName() == "key") {
            constraint.kind = ElementRule::IdentityConstraint::Kind::Key;
        } else if (constraintElement.LocalName() == "unique") {
            constraint.kind = ElementRule::IdentityConstraint::Kind::Unique;
        } else {
            constraint.kind = ElementRule::IdentityConstraint::Kind::KeyRef;
        }

        constraint.name = constraintElement.GetAttribute("name");
        constraint.namespaceUri = targetNamespace;
        if (constraint.name.empty()) {
            throw XmlException("XML Schema identity constraints require a name");
        }

        constraint.annotation = parseAnnotation(constraintElement);

        const auto namespaceBindings = CollectInScopeNamespaceBindings(constraintElement);
        constraint.namespaceBindings = namespaceBindings;
        constraint.compiledFieldPaths.clear();

        if (constraint.kind == ElementRule::IdentityConstraint::Kind::KeyRef) {
            const std::string refer = constraintElement.GetAttribute("refer");
            if (refer.empty()) {
                throw XmlException("XML Schema keyref constraints require a refer attribute");
            }
            const auto [referName, referNamespaceUri] = resolveTypeName(constraintElement, refer);
            constraint.referName = referName;
            constraint.referNamespaceUri = referNamespaceUri;
        } else if (!constraintElement.GetAttribute("refer").empty()) {
            throw XmlException("XML Schema key and unique constraints cannot specify a refer attribute");
        }

        for (const auto& child : constraintElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }
            if (childElement->LocalName() == "selector") {
                if (!constraint.selectorXPath.empty()) {
                    throw XmlException("XML Schema identity constraints can only declare a single selector");
                }
                validateAnnotationOnlySchemaChildren(*childElement, "xs:selector");
                constraint.selectorXPath = childElement->GetAttribute("xpath");
            } else if (childElement->LocalName() == "field") {
                validateAnnotationOnlySchemaChildren(*childElement, "xs:field");
                constraint.fieldXPaths.push_back(childElement->GetAttribute("xpath"));
            } else if (childElement->LocalName() != "annotation") {
                throw XmlException("XML Schema identity constraints can only declare annotation, selector, and field children");
            }
        }

        if (constraint.selectorXPath.empty()) {
            throw XmlException("XML Schema identity constraints require a selector xpath");
        }
        constraint.compiledSelectorPath = tryCompileIdentityConstraintXPath(
            constraint.selectorXPath,
            constraint.namespaceBindings,
            false);
        if (constraint.fieldXPaths.empty()) {
            throw XmlException("XML Schema identity constraints require at least one field xpath");
        }

        constraint.compiledFieldPaths.reserve(constraint.fieldXPaths.size());
        for (const auto& fieldXPath : constraint.fieldXPaths) {
            if (fieldXPath.empty()) {
                throw XmlException("XML Schema identity constraint field xpath cannot be empty");
            }
            constraint.compiledFieldPaths.push_back(tryCompileIdentityConstraintXPath(
                fieldXPath,
                constraint.namespaceBindings,
                true));
        }

        upsertIdentityConstraint(constraint);
        return constraint;
    };

    ensureSimpleTypeResolved = [&](const std::string& localTypeName, const std::string& typeNamespaceUri, const std::string& qualifiedName) -> SimpleTypeRule {
        const std::string ruleKey = makeQualifiedRuleKey(localTypeName, typeNamespaceUri);
        const auto declaration = declaredSimpleTypes.find(ruleKey);
        if (declaration == declaredSimpleTypes.end()) {
            if (const auto* namedType = FindSimpleTypeRule(localTypeName, typeNamespaceUri)) {
                return *namedType;
            }
            throw XmlException("XML Schema simpleType '" + qualifiedName + "' is not supported");
        }

        if (resolvedLocalSimpleTypes.find(ruleKey) != resolvedLocalSimpleTypes.end()) {
            if (const auto* namedType = FindSimpleTypeRule(localTypeName, typeNamespaceUri)) {
                return *namedType;
            }
        }

        if (!resolvingSimpleTypes.insert(ruleKey).second) {
            throw XmlException("XML Schema simpleType '" + qualifiedName + "' contains a circular type reference");
        }

        SimpleTypeRule rule;
        try {
            rule = parseSimpleType(*declaration->second);
        } catch (...) {
            resolvingSimpleTypes.erase(ruleKey);
            throw;
        }
        resolvingSimpleTypes.erase(ruleKey);

        rule.namedTypeName = localTypeName;
        rule.namedTypeNamespaceUri = typeNamespaceUri;

        upsertSimpleType(NamedSimpleTypeRule{localTypeName, typeNamespaceUri, rule});
        resolvedLocalSimpleTypes.insert(ruleKey);
        return rule;
    };

    const auto applyComplexType = [](ElementRule& elementRule, const ComplexTypeRule& complexTypeRule) {
        elementRule.allowsText = complexTypeRule.allowsText;
        elementRule.textType = complexTypeRule.textType;
        elementRule.contentModel = complexTypeRule.contentModel;
        elementRule.attributes = complexTypeRule.attributes;
        elementRule.children = complexTypeRule.children;
        elementRule.particle = complexTypeRule.particle;
        elementRule.anyAttributeAllowed = complexTypeRule.anyAttributeAllowed;
        elementRule.anyAttributeNamespaceConstraint = complexTypeRule.anyAttributeNamespaceConstraint;
        elementRule.anyAttributeProcessContents = complexTypeRule.anyAttributeProcessContents;
    };

    std::function<Particle(const XmlElement&)> parseParticle;
    const auto appendAttributes = [](std::vector<AttributeUse>& destination, const std::vector<AttributeUse>& source) {
        destination.insert(destination.end(), source.begin(), source.end());
    };
    const auto mergeAnyAttribute = [](bool& destinationAllowed, std::string& destinationConstraint, std::string& destinationProcessContents,
                                      bool sourceAllowed, const std::string& sourceConstraint, const std::string& sourceProcessContents) {
        if (!sourceAllowed) {
            return;
        }
        destinationAllowed = true;
        destinationConstraint = sourceConstraint;
        destinationProcessContents = sourceProcessContents;
    };

    matchesWildcardNamespace = [&](const std::string& namespaceConstraint, const std::string& namespaceUri) {
        if (namespaceConstraint.empty() || namespaceConstraint == "##any") {
            return true;
        }
        std::istringstream tokens(namespaceConstraint);
        std::string token;
        while (tokens >> token) {
            if (wildcardNamespaceTokenMatches(token, namespaceUri)) {
                return true;
            }
        }
        return false;
    };

    const auto makeFlatParticleFromRule = [&](const ComplexTypeRule& rule) -> std::optional<Particle> {
        if (rule.particle.has_value()) {
            return rule.particle;
        }
        if (rule.children.empty()) {
            return std::nullopt;
        }

        Particle particle;
        particle.kind = rule.contentModel == ContentModel::Choice ? Particle::Kind::Choice : Particle::Kind::Sequence;
        for (const auto& childRule : rule.children) {
            Particle childParticle;
            childParticle.kind = Particle::Kind::Element;
            childParticle.name = childRule.name;
            childParticle.namespaceUri = childRule.namespaceUri;
            childParticle.elementSimpleType = childRule.declaredSimpleType;
            if (childRule.declaredComplexType) {
                childParticle.elementComplexType = std::make_shared<ComplexTypeRule>(*childRule.declaredComplexType);
            } else if (const auto* declaredRule = FindElementRule(childRule.name, childRule.namespaceUri)) {
                childParticle.elementSimpleType = declaredRule->declaredSimpleType;
                if (declaredRule->declaredComplexType.has_value()) {
                    childParticle.elementComplexType = std::make_shared<ComplexTypeRule>(*declaredRule->declaredComplexType);
                }
                childParticle.elementIsNillable = declaredRule->isNillable;
                childParticle.elementDefaultValue = declaredRule->defaultValue;
                childParticle.elementFixedValue = declaredRule->fixedValue;
                childParticle.elementBlockRestriction = declaredRule->blockRestriction;
                childParticle.elementBlockExtension = declaredRule->blockExtension;
                childParticle.elementFinalRestriction = declaredRule->finalRestriction;
                childParticle.elementFinalExtension = declaredRule->finalExtension;
            }
            childParticle.elementIsNillable = childRule.isNillable;
            childParticle.elementDefaultValue = childRule.defaultValue;
            childParticle.elementFixedValue = childRule.fixedValue;
            childParticle.elementBlockRestriction = childRule.blockRestriction;
            childParticle.elementBlockExtension = childRule.blockExtension;
            childParticle.elementFinalRestriction = childRule.finalRestriction;
            childParticle.elementFinalExtension = childRule.finalExtension;
            childParticle.minOccurs = childRule.minOccurs;
            childParticle.maxOccurs = childRule.maxOccurs;
            particle.children.push_back(childParticle);
        }
        return particle;
    };

    const auto applyParticleToRule = [](ComplexTypeRule& rule, const Particle& particle) {
        const bool isFlatParticle = particle.minOccurs == 1
            && particle.maxOccurs == 1
            && particle.kind != Particle::Kind::All
            && std::all_of(particle.children.begin(), particle.children.end(), [](const auto& childParticle) {
                return childParticle.kind == Particle::Kind::Element;
            });

        if (isFlatParticle) {
            rule.particle.reset();
            rule.contentModel = particle.kind == Particle::Kind::Choice ? ContentModel::Choice : ContentModel::Sequence;
            rule.children.clear();
            for (const auto& childParticle : particle.children) {
                ChildUse childUse;
                childUse.name = childParticle.name;
                childUse.namespaceUri = childParticle.namespaceUri;
                childUse.declaredSimpleType = childParticle.elementSimpleType;
                if (childParticle.elementComplexType) {
                    childUse.declaredComplexType = std::make_shared<ComplexTypeRule>(*childParticle.elementComplexType);
                }
                childUse.isNillable = childParticle.elementIsNillable;
                childUse.defaultValue = childParticle.elementDefaultValue;
                childUse.fixedValue = childParticle.elementFixedValue;
                childUse.blockRestriction = childParticle.elementBlockRestriction;
                childUse.blockExtension = childParticle.elementBlockExtension;
                childUse.finalRestriction = childParticle.elementFinalRestriction;
                childUse.finalExtension = childParticle.elementFinalExtension;
                childUse.minOccurs = childParticle.minOccurs;
                childUse.maxOccurs = childParticle.maxOccurs;
                rule.children.push_back(childUse);
            }
            return;
        }

        rule.particle = particle;
        rule.children.clear();
        rule.contentModel = ContentModel::Empty;
    };

    const auto upsertAttributeUse = [](std::vector<AttributeUse>& attributes, const AttributeUse& attributeUse) {
        const auto existing = std::find_if(attributes.begin(), attributes.end(), [&](const auto& current) {
            return current.name == attributeUse.name && current.namespaceUri == attributeUse.namespaceUri;
        });
        if (existing != attributes.end()) {
            *existing = attributeUse;
        } else {
            attributes.push_back(attributeUse);
        }
    };

    const auto eraseAttributeUse = [](std::vector<AttributeUse>& attributes, const std::string& name, const std::string& namespaceUri) {
        attributes.erase(std::remove_if(attributes.begin(), attributes.end(), [&](const auto& current) {
            return current.name == name && current.namespaceUri == namespaceUri;
        }), attributes.end());
    };

    const auto simpleTypeBaseFamily = [](const SimpleTypeRule& rule) {
        if (rule.variety == SimpleTypeRule::Variety::List) {
            return std::string("list");
        }
        if (rule.variety == SimpleTypeRule::Variety::Union) {
            return std::string("union");
        }
        return rule.baseType.empty() ? std::string("xs:string") : rule.baseType;
    };

    const auto whitespaceStrength = [](const std::optional<std::string>& value) {
        if (!value.has_value() || *value == "preserve") {
            return 0;
        }
        if (*value == "replace") {
            return 1;
        }
        if (*value == "collapse") {
            return 2;
        }
        return -1;
    };

    const auto compareFacetNumbers = [](const std::string& baseType, const std::string& left, const std::string& right) {
        if (IsPracticalIntegerBuiltinType(baseType)) {
            return ComparePracticalIntegerValues(ParsePracticalIntegerOrThrow(left), ParsePracticalIntegerOrThrow(right));
        }
        if (baseType == "xs:decimal") {
            return ComparePracticalDecimalValues(ParsePracticalDecimalOrThrow(left), ParsePracticalDecimalOrThrow(right));
        }
        if (baseType == "xs:float") {
            const float leftValue = XmlConvert::ToSingle(left);
            const float rightValue = XmlConvert::ToSingle(right);
            if (leftValue < rightValue) {
                return -1;
            }
            if (leftValue > rightValue) {
                return 1;
            }
            return 0;
        }

        const double leftValue = XmlConvert::ToDouble(left);
        const double rightValue = XmlConvert::ToDouble(right);
        if (leftValue < rightValue) {
            return -1;
        }
        if (leftValue > rightValue) {
            return 1;
        }
        return 0;
    };

    validateDerivedSimpleTypeRestriction = [&](const SimpleTypeRule& baseRule, const SimpleTypeRule& derivedRule, const std::string& label) {
        struct RestrictionTemporalValue {
            long long primary = 0;
            int secondary = 0;
            int fractionalNanoseconds = 0;
        };

        const auto compareRestrictionTemporalValues = [](const RestrictionTemporalValue& left, const RestrictionTemporalValue& right) {
            if (left.primary != right.primary) {
                return left.primary < right.primary ? -1 : 1;
            }
            if (left.secondary != right.secondary) {
                return left.secondary < right.secondary ? -1 : 1;
            }
            if (left.fractionalNanoseconds != right.fractionalNanoseconds) {
                return left.fractionalNanoseconds < right.fractionalNanoseconds ? -1 : 1;
            }
            return 0;
        };

        const auto normalizeRestrictionTemporalValue = [&](const std::string& baseType, const std::string& lexicalValue) -> RestrictionTemporalValue {
            const auto parseFixedWidthNumber = [&](const std::string& text, const std::size_t start, const std::size_t length) {
                if (start + length > text.size()) {
                    throw XmlException("invalid temporal lexical form");
                }
                int value = 0;
                for (std::size_t index = 0; index < length; ++index) {
                    const char ch = text[start + index];
                    if (!std::isdigit(static_cast<unsigned char>(ch))) {
                        throw XmlException("invalid temporal lexical form");
                    }
                    value = value * 10 + (ch - '0');
                }
                return value;
            };

            const auto parseTimezoneOffsetMinutes = [&](const std::string& text, const std::size_t start) {
                if (start >= text.size()) {
                    return 0;
                }
                if (text[start] == 'Z') {
                    if (start + 1 != text.size()) {
                        throw XmlException("invalid temporal lexical form");
                    }
                    return 0;
                }
                if ((text[start] != '+' && text[start] != '-') || start + 6 != text.size() || text[start + 3] != ':') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int hours = parseFixedWidthNumber(text, start + 1, 2);
                const int minutes = parseFixedWidthNumber(text, start + 4, 2);
                if (hours > 14 || minutes > 59 || (hours == 14 && minutes != 0)) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int totalMinutes = hours * 60 + minutes;
                return text[start] == '-' ? -totalMinutes : totalMinutes;
            };

            const auto parseFractionalNanoseconds = [&](const std::string& digits) {
                if (digits.empty()) {
                    throw XmlException("invalid temporal lexical form");
                }
                int value = 0;
                for (std::size_t index = 0; index < 9; ++index) {
                    value *= 10;
                    if (index < digits.size()) {
                        const char ch = digits[index];
                        if (!std::isdigit(static_cast<unsigned char>(ch))) {
                            throw XmlException("invalid temporal lexical form");
                        }
                        value += ch - '0';
                    }
                }
                for (std::size_t index = 9; index < digits.size(); ++index) {
                    if (!std::isdigit(static_cast<unsigned char>(digits[index]))) {
                        throw XmlException("invalid temporal lexical form");
                    }
                }
                return value;
            };

            const auto isLeapYear = [](const int year) {
                return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            };

            const auto daysInMonth = [&](const int year, const int month) {
                static const int kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
                if (month < 1 || month > 12) {
                    return 0;
                }
                if (month == 2 && isLeapYear(year)) {
                    return 29;
                }
                return kDaysInMonth[month - 1];
            };

            const auto daysFromCivil = [](int year, const unsigned month, const unsigned day) -> long long {
                year -= month <= 2 ? 1 : 0;
                const long long era = static_cast<long long>(year >= 0 ? year : year - 399) / 400LL;
                const unsigned yoe = static_cast<unsigned>(year - static_cast<int>(era * 400LL));
                const unsigned shiftedMonth = month > 2 ? month - 3 : month + 9;
                const unsigned doy = (153 * shiftedMonth + 2) / 5 + day - 1;
                const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
                return era * 146097LL + static_cast<long long>(doe) - 719468LL;
            };

            auto normalizeDateTimeComponents = [](long long& dayValue, int& secondsOfDay, const int offsetMinutes) {
                secondsOfDay -= offsetMinutes * 60;
                while (secondsOfDay < 0) {
                    secondsOfDay += 24 * 60 * 60;
                    --dayValue;
                }
                while (secondsOfDay >= 24 * 60 * 60) {
                    secondsOfDay -= 24 * 60 * 60;
                    ++dayValue;
                }
            };

            if (baseType == "xs:date") {
                if (lexicalValue.size() < 10 || lexicalValue[4] != '-' || lexicalValue[7] != '-') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
                const int month = parseFixedWidthNumber(lexicalValue, 5, 2);
                const int day = parseFixedWidthNumber(lexicalValue, 8, 2);
                if (day < 1 || day > daysInMonth(year, month)) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 10);
                long long absoluteDay = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
                int secondsOfDay = 0;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, 0 };
            }

            if (baseType == "xs:gYear") {
                if (lexicalValue.size() < 4) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 4);
                long long absoluteDay = daysFromCivil(year, 1, 1);
                int secondsOfDay = 0;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, 0 };
            }

            if (baseType == "xs:gYearMonth") {
                if (lexicalValue.size() < 7 || lexicalValue[4] != '-') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
                const int month = parseFixedWidthNumber(lexicalValue, 5, 2);
                if (month < 1 || month > 12) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 7);
                long long absoluteDay = daysFromCivil(year, static_cast<unsigned>(month), 1);
                int secondsOfDay = 0;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, 0 };
            }

            if (baseType == "xs:gMonth") {
                if (lexicalValue.size() < 4 || lexicalValue[0] != '-' || lexicalValue[1] != '-') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int month = parseFixedWidthNumber(lexicalValue, 2, 2);
                if (month < 1 || month > 12) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 4);
                long long absoluteDay = daysFromCivil(2000, static_cast<unsigned>(month), 1);
                int secondsOfDay = 0;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, 0 };
            }

            if (baseType == "xs:gDay") {
                if (lexicalValue.size() < 5 || lexicalValue[0] != '-' || lexicalValue[1] != '-' || lexicalValue[2] != '-') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int day = parseFixedWidthNumber(lexicalValue, 3, 2);
                if (day < 1 || day > 31) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 5);
                long long absoluteDay = daysFromCivil(2000, 1, static_cast<unsigned>(day));
                int secondsOfDay = 0;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, 0 };
            }

            if (baseType == "xs:gMonthDay") {
                if (lexicalValue.size() < 7 || lexicalValue[0] != '-' || lexicalValue[1] != '-' || lexicalValue[4] != '-') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int month = parseFixedWidthNumber(lexicalValue, 2, 2);
                const int day = parseFixedWidthNumber(lexicalValue, 5, 2);
                if (day < 1 || day > daysInMonth(2000, month)) {
                    throw XmlException("invalid temporal lexical form");
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 7);
                long long absoluteDay = daysFromCivil(2000, static_cast<unsigned>(month), static_cast<unsigned>(day));
                int secondsOfDay = 0;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, 0 };
            }

            if (baseType == "xs:time") {
                if (lexicalValue.size() < 8 || lexicalValue[2] != ':' || lexicalValue[5] != ':') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int hour = parseFixedWidthNumber(lexicalValue, 0, 2);
                const int minute = parseFixedWidthNumber(lexicalValue, 3, 2);
                const int second = parseFixedWidthNumber(lexicalValue, 6, 2);
                if (hour > 23 || minute > 59 || second > 59) {
                    throw XmlException("invalid temporal lexical form");
                }
                std::size_t timezoneStart = 8;
                int fractionalNanoseconds = 0;
                if (timezoneStart < lexicalValue.size() && lexicalValue[timezoneStart] == '.') {
                    const std::size_t fractionStart = timezoneStart + 1;
                    std::size_t fractionEnd = fractionStart;
                    while (fractionEnd < lexicalValue.size() && std::isdigit(static_cast<unsigned char>(lexicalValue[fractionEnd]))) {
                        ++fractionEnd;
                    }
                    fractionalNanoseconds = parseFractionalNanoseconds(lexicalValue.substr(fractionStart, fractionEnd - fractionStart));
                    timezoneStart = fractionEnd;
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, timezoneStart);
                int secondsOfDay = hour * 3600 + minute * 60 + second - timezoneOffsetMinutes * 60;
                secondsOfDay %= 24 * 60 * 60;
                if (secondsOfDay < 0) {
                    secondsOfDay += 24 * 60 * 60;
                }
                return RestrictionTemporalValue{ 0, secondsOfDay, fractionalNanoseconds };
            }

            if (baseType == "xs:dateTime") {
                if (lexicalValue.size() < 19 || lexicalValue[4] != '-' || lexicalValue[7] != '-' || lexicalValue[10] != 'T'
                    || lexicalValue[13] != ':' || lexicalValue[16] != ':') {
                    throw XmlException("invalid temporal lexical form");
                }
                const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
                const int month = parseFixedWidthNumber(lexicalValue, 5, 2);
                const int day = parseFixedWidthNumber(lexicalValue, 8, 2);
                const int hour = parseFixedWidthNumber(lexicalValue, 11, 2);
                const int minute = parseFixedWidthNumber(lexicalValue, 14, 2);
                const int second = parseFixedWidthNumber(lexicalValue, 17, 2);
                if (day < 1 || day > daysInMonth(year, month) || hour > 23 || minute > 59 || second > 59) {
                    throw XmlException("invalid temporal lexical form");
                }
                std::size_t timezoneStart = 19;
                int fractionalNanoseconds = 0;
                if (timezoneStart < lexicalValue.size() && lexicalValue[timezoneStart] == '.') {
                    const std::size_t fractionStart = timezoneStart + 1;
                    std::size_t fractionEnd = fractionStart;
                    while (fractionEnd < lexicalValue.size() && std::isdigit(static_cast<unsigned char>(lexicalValue[fractionEnd]))) {
                        ++fractionEnd;
                    }
                    fractionalNanoseconds = parseFractionalNanoseconds(lexicalValue.substr(fractionStart, fractionEnd - fractionStart));
                    timezoneStart = fractionEnd;
                }
                const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, timezoneStart);
                long long absoluteDay = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
                int secondsOfDay = hour * 3600 + minute * 60 + second;
                normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
                return RestrictionTemporalValue{ absoluteDay, secondsOfDay, fractionalNanoseconds };
            }

            throw XmlException("unsupported temporal base type");
        };

        if (baseRule.variety != derivedRule.variety) {
            throw XmlException("XML Schema " + label + " must preserve the base simpleType variety when using restriction");
        }

        const std::string baseFamily = simpleTypeBaseFamily(baseRule);
        const std::string derivedFamily = simpleTypeBaseFamily(derivedRule);
        if (baseFamily != derivedFamily) {
            throw XmlException("XML Schema " + label + " cannot change the base simpleType primitive when using restriction");
        }

        if (!baseRule.enumerationValues.empty()) {
            if (derivedRule.enumerationValues.empty()) {
                throw XmlException("XML Schema " + label + " cannot remove enumeration constraints from the base simpleType");
            }
            const bool subset = std::all_of(derivedRule.enumerationValues.begin(), derivedRule.enumerationValues.end(), [&](const auto& value) {
                return std::find(baseRule.enumerationValues.begin(), baseRule.enumerationValues.end(), value) != baseRule.enumerationValues.end();
            });
            if (!subset) {
                throw XmlException("XML Schema " + label + " cannot add enumeration values outside the base simpleType");
            }
        }

        if (baseRule.pattern.has_value() && derivedRule.pattern != baseRule.pattern) {
            throw XmlException("XML Schema " + label + " cannot change the base pattern facet");
        }

        if (whitespaceStrength(derivedRule.whiteSpace) < whitespaceStrength(baseRule.whiteSpace)) {
            throw XmlException("XML Schema " + label + " cannot weaken the base whiteSpace facet");
        }

        if (baseRule.length.has_value() && derivedRule.length != baseRule.length) {
            throw XmlException("XML Schema " + label + " cannot change an exact length facet from the base simpleType");
        }
        if (baseRule.minLength.has_value() && (!derivedRule.minLength.has_value() || *derivedRule.minLength < *baseRule.minLength)) {
            throw XmlException("XML Schema " + label + " cannot relax the base minLength facet");
        }
        if (baseRule.maxLength.has_value() && (!derivedRule.maxLength.has_value() || *derivedRule.maxLength > *baseRule.maxLength)) {
            throw XmlException("XML Schema " + label + " cannot relax the base maxLength facet");
        }
        if (baseRule.totalDigits.has_value() && (!derivedRule.totalDigits.has_value() || *derivedRule.totalDigits > *baseRule.totalDigits)) {
            throw XmlException("XML Schema " + label + " cannot relax the base totalDigits facet");
        }
        if (baseRule.fractionDigits.has_value() && (!derivedRule.fractionDigits.has_value() || *derivedRule.fractionDigits > *baseRule.fractionDigits)) {
            throw XmlException("XML Schema " + label + " cannot relax the base fractionDigits facet");
        }

        const bool numericFamily = IsPracticalIntegerBuiltinType(baseFamily) || baseFamily == "xs:decimal" || baseFamily == "xs:double" || baseFamily == "xs:float";
        if (numericFamily) {
            if (baseRule.minInclusive.has_value()
                && (!derivedRule.minInclusive.has_value() || compareFacetNumbers(baseFamily, *derivedRule.minInclusive, *baseRule.minInclusive) < 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base minInclusive facet");
            }
            if (baseRule.minExclusive.has_value()
                && (!derivedRule.minExclusive.has_value() || compareFacetNumbers(baseFamily, *derivedRule.minExclusive, *baseRule.minExclusive) < 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base minExclusive facet");
            }
            if (baseRule.maxInclusive.has_value()
                && (!derivedRule.maxInclusive.has_value() || compareFacetNumbers(baseFamily, *derivedRule.maxInclusive, *baseRule.maxInclusive) > 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxInclusive facet");
            }
            if (baseRule.maxExclusive.has_value()
                && (!derivedRule.maxExclusive.has_value() || compareFacetNumbers(baseFamily, *derivedRule.maxExclusive, *baseRule.maxExclusive) > 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxExclusive facet");
            }
        }

        const bool durationFamily = baseFamily == "xs:duration";
        if (durationFamily) {
            if (baseRule.minInclusive.has_value()
                && (!derivedRule.minInclusive.has_value() || ComparePracticalDurationValues(ParsePracticalDurationOrThrow(*derivedRule.minInclusive), ParsePracticalDurationOrThrow(*baseRule.minInclusive)) < 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base minInclusive facet");
            }
            if (baseRule.minExclusive.has_value()
                && (!derivedRule.minExclusive.has_value() || ComparePracticalDurationValues(ParsePracticalDurationOrThrow(*derivedRule.minExclusive), ParsePracticalDurationOrThrow(*baseRule.minExclusive)) < 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base minExclusive facet");
            }
            if (baseRule.maxInclusive.has_value()
                && (!derivedRule.maxInclusive.has_value() || ComparePracticalDurationValues(ParsePracticalDurationOrThrow(*derivedRule.maxInclusive), ParsePracticalDurationOrThrow(*baseRule.maxInclusive)) > 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxInclusive facet");
            }
            if (baseRule.maxExclusive.has_value()
                && (!derivedRule.maxExclusive.has_value() || ComparePracticalDurationValues(ParsePracticalDurationOrThrow(*derivedRule.maxExclusive), ParsePracticalDurationOrThrow(*baseRule.maxExclusive)) > 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxExclusive facet");
            }
        }

        const bool temporalFamily = baseFamily == "xs:date" || baseFamily == "xs:time" || baseFamily == "xs:dateTime"
            || baseFamily == "xs:gYear" || baseFamily == "xs:gYearMonth"
            || baseFamily == "xs:gMonth" || baseFamily == "xs:gDay" || baseFamily == "xs:gMonthDay";
        if (temporalFamily) {
            if (baseRule.minInclusive.has_value()
                && (!derivedRule.minInclusive.has_value() || compareRestrictionTemporalValues(normalizeRestrictionTemporalValue(baseFamily, *derivedRule.minInclusive), normalizeRestrictionTemporalValue(baseFamily, *baseRule.minInclusive)) < 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base minInclusive facet");
            }
            if (baseRule.minExclusive.has_value()
                && (!derivedRule.minExclusive.has_value() || compareRestrictionTemporalValues(normalizeRestrictionTemporalValue(baseFamily, *derivedRule.minExclusive), normalizeRestrictionTemporalValue(baseFamily, *baseRule.minExclusive)) < 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base minExclusive facet");
            }
            if (baseRule.maxInclusive.has_value()
                && (!derivedRule.maxInclusive.has_value() || compareRestrictionTemporalValues(normalizeRestrictionTemporalValue(baseFamily, *derivedRule.maxInclusive), normalizeRestrictionTemporalValue(baseFamily, *baseRule.maxInclusive)) > 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxInclusive facet");
            }
            if (baseRule.maxExclusive.has_value()
                && (!derivedRule.maxExclusive.has_value() || compareRestrictionTemporalValues(normalizeRestrictionTemporalValue(baseFamily, *derivedRule.maxExclusive), normalizeRestrictionTemporalValue(baseFamily, *baseRule.maxExclusive)) > 0)) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxExclusive facet");
            }
        }
    };

    const auto validateAttributeRestrictionLegality = [&](const std::vector<AttributeUse>& baseAttributes, const std::vector<AttributeUse>& restrictedAttributes,
                                                          bool baseAnyAttributeAllowed, const std::string& baseAnyAttributeNamespaceConstraint,
                                                          bool allowNewAttributes, const std::string& label) {
        for (const auto& restrictedAttribute : restrictedAttributes) {
            const auto baseIt = std::find_if(baseAttributes.begin(), baseAttributes.end(), [&](const auto& baseAttribute) {
                return baseAttribute.name == restrictedAttribute.name && baseAttribute.namespaceUri == restrictedAttribute.namespaceUri;
            });
            if (baseIt == baseAttributes.end()) {
                if (allowNewAttributes
                    || (baseAnyAttributeAllowed && matchesWildcardNamespace(baseAnyAttributeNamespaceConstraint, restrictedAttribute.namespaceUri))) {
                    continue;
                }
                throw XmlException("XML Schema " + label + " cannot add attribute '" + restrictedAttribute.name + "' that is absent from the base type");
            }
            if (baseIt->required && !restrictedAttribute.required) {
                throw XmlException("XML Schema " + label + " cannot relax required attribute '" + restrictedAttribute.name + "'");
            }
            if (baseIt->fixedValue.has_value()) {
                if (!restrictedAttribute.fixedValue.has_value()) {
                    throw XmlException("XML Schema " + label + " cannot remove fixed value from inherited attribute '" + restrictedAttribute.name + "'");
                }
                if (*restrictedAttribute.fixedValue != *baseIt->fixedValue) {
                    throw XmlException("XML Schema " + label + " cannot change fixed value of inherited attribute '" + restrictedAttribute.name + "'");
                }
            }
            if (baseIt->defaultValue.has_value()) {
                if (restrictedAttribute.fixedValue.has_value()) {
                    if (*restrictedAttribute.fixedValue != *baseIt->defaultValue) {
                        throw XmlException("XML Schema " + label + " cannot change default value of inherited attribute '" + restrictedAttribute.name + "'");
                    }
                } else {
                    if (!restrictedAttribute.defaultValue.has_value()) {
                        throw XmlException("XML Schema " + label + " cannot remove default value from inherited attribute '" + restrictedAttribute.name + "'");
                    }
                    if (*restrictedAttribute.defaultValue != *baseIt->defaultValue) {
                        throw XmlException("XML Schema " + label + " cannot change default value of inherited attribute '" + restrictedAttribute.name + "'");
                    }
                }
            }
            if (baseIt->type.has_value()) {
                if (!restrictedAttribute.type.has_value()) {
                    throw XmlException("XML Schema " + label + " must preserve the type of inherited attribute '" + restrictedAttribute.name + "'");
                }
                validateDerivedSimpleTypeRestriction(*baseIt->type, *restrictedAttribute.type,
                    label + " attribute '" + restrictedAttribute.name + "'");
            }
        }

        for (const auto& baseAttribute : baseAttributes) {
            if (!baseAttribute.required) {
                continue;
            }
            const bool stillPresent = std::any_of(restrictedAttributes.begin(), restrictedAttributes.end(), [&](const auto& restrictedAttribute) {
                return restrictedAttribute.name == baseAttribute.name
                    && restrictedAttribute.namespaceUri == baseAttribute.namespaceUri
                    && restrictedAttribute.required;
            });
            if (!stillPresent) {
                throw XmlException("XML Schema " + label + " cannot remove required attribute '" + baseAttribute.name + "'");
            }
        }
    };

    const auto processContentsStrength = [](const std::string& processContents) {
        if (processContents == "skip") {
            return 0;
        }
        if (processContents == "lax") {
            return 1;
        }
        return 2;
    };

    const auto wildcardConstraintIsSubsetOf = [&](const std::string& derivedConstraint, const std::string& baseConstraint) {
        const std::string normalizedDerived = derivedConstraint.empty() ? "##any" : derivedConstraint;
        const std::string normalizedBase = baseConstraint.empty() ? "##any" : baseConstraint;
        if (normalizedBase == "##any") {
            return true;
        }
        if (normalizedDerived == normalizedBase) {
            return true;
        }
        std::istringstream derivedTokens(normalizedDerived);
        std::string derivedToken;
        while (derivedTokens >> derivedToken) {
            if (!wildcardTokenIsSubsetOfConstraint(derivedToken, normalizedBase)) {
                return false;
            }
        }
        return true;
    };

    const auto validateAnyAttributeRestrictionLegality = [&](bool baseAllowed, const std::string& baseConstraint, const std::string& baseProcessContents,
                                                            bool derivedAllowed, const std::string& derivedConstraint, const std::string& derivedProcessContents,
                                                            bool allowNewWildcard, const std::string& label) {
        if (!derivedAllowed) {
            return;
        }
        if (!baseAllowed) {
            if (allowNewWildcard) {
                return;
            }
            throw XmlException("XML Schema " + label + " cannot add an anyAttribute wildcard that is absent from the base type");
        }
        if (!wildcardConstraintIsSubsetOf(derivedConstraint, baseConstraint)) {
            throw XmlException("XML Schema " + label + " cannot widen the base anyAttribute namespace constraint");
        }
        if (processContentsStrength(derivedProcessContents) < processContentsStrength(baseProcessContents)) {
            throw XmlException("XML Schema " + label + " cannot weaken the base anyAttribute processContents");
        }
    };

    std::function<void(const Particle&, const Particle&, const std::string&)> validateDerivedParticleRestriction;
    std::function<bool(const Particle&, const Particle&)> particleShapesCanCorrespond;
    std::function<void(const ComplexTypeRule&, const ComplexTypeRule&, const std::string&)> validateAnonymousComplexTypeRestriction;
    std::function<bool(const ComplexTypeRule&, const ComplexTypeRule&)> anonymousComplexTypesAreEquivalent;

    const auto unwrapEquivalentParticle = [](const Particle& particle) -> const Particle* {
        const Particle* current = &particle;
        while (current->minOccurs == 1
            && current->maxOccurs == 1
            && (current->kind == Particle::Kind::Sequence || current->kind == Particle::Kind::Choice)
            && current->children.size() == 1) {
            current = &current->children.front();
        }
        return current;
    };

    particleShapesCanCorrespond = [&](const Particle& baseParticle, const Particle& derivedParticle) {
        const Particle& normalizedBase = *unwrapEquivalentParticle(baseParticle);
        const Particle& normalizedDerived = *unwrapEquivalentParticle(derivedParticle);

        if (normalizedBase.kind == Particle::Kind::Choice) {
            return std::any_of(normalizedBase.children.begin(), normalizedBase.children.end(), [&](const auto& baseChild) {
                return particleShapesCanCorrespond(baseChild, derivedParticle);
            });
        }
        if (normalizedBase.kind != normalizedDerived.kind) {
            return false;
        }
        if (normalizedBase.kind == Particle::Kind::Element) {
            return normalizedBase.name == normalizedDerived.name && normalizedBase.namespaceUri == normalizedDerived.namespaceUri;
        }
        return true;
    };

    const auto choiceBranchCanBeReused = [&](const Particle& particle) {
        return unwrapEquivalentParticle(particle)->kind == Particle::Kind::Choice;
    };

    std::function<bool(const SimpleTypeRule&, const SimpleTypeRule&, bool&)> simpleTypeDerivesFrom;
    std::function<bool(const ComplexTypeRule&, const ComplexTypeRule&, bool&, bool&)> complexTypeDerivesFrom;

    const auto elementParticleTypeCanRestrictBase = [&](const Particle& baseParticle,
        const Particle& derivedParticle,
        bool& usesRestriction,
        bool& usesExtension,
        std::string& detailError) {
        usesRestriction = false;
        usesExtension = false;
        detailError.clear();
        if (baseParticle.elementComplexType) {
            if (!derivedParticle.elementComplexType) {
                return false;
            }
            if (baseParticle.elementComplexType->namedTypeName.empty()) {
                try {
                    validateAnonymousComplexTypeRestriction(*baseParticle.elementComplexType, *derivedParticle.elementComplexType,
                        "child element '" + baseParticle.name + "'");
                } catch (const XmlException& exception) {
                    detailError = exception.what();
                    return false;
                }
                usesRestriction = !anonymousComplexTypesAreEquivalent(*baseParticle.elementComplexType, *derivedParticle.elementComplexType);
                return true;
            }
            if (!complexTypeDerivesFrom(*derivedParticle.elementComplexType, *baseParticle.elementComplexType, usesRestriction, usesExtension)) {
                return false;
            }
            return !usesExtension;
        }
        if (baseParticle.elementSimpleType.has_value()) {
            if (!derivedParticle.elementSimpleType.has_value()) {
                return false;
            }
            return simpleTypeDerivesFrom(*derivedParticle.elementSimpleType, *baseParticle.elementSimpleType, usesRestriction);
        }
        return true;
    };

    const auto elementParticleDeclarationCanRestrictBase = [&](const Particle& baseParticle,
        const Particle& derivedParticle,
        std::string& error) {
        if (derivedParticle.elementIsNillable && !baseParticle.elementIsNillable) {
            error = "XML Schema child element restriction cannot make a base child element nillable";
            return false;
        }

        if (baseParticle.elementFixedValue.has_value()) {
            if (!derivedParticle.elementFixedValue.has_value()) {
                error = "XML Schema child element restriction cannot remove the base child element fixed value";
                return false;
            }
            if (*derivedParticle.elementFixedValue != *baseParticle.elementFixedValue) {
                error = "XML Schema child element restriction cannot change the base child element fixed value";
                return false;
            }
        }

        if (baseParticle.elementDefaultValue.has_value()) {
            if (derivedParticle.elementFixedValue.has_value()) {
                if (*derivedParticle.elementFixedValue != *baseParticle.elementDefaultValue) {
                    error = "XML Schema child element restriction cannot change the base child element default value";
                    return false;
                }
            } else {
                if (!derivedParticle.elementDefaultValue.has_value()) {
                    error = "XML Schema child element restriction cannot remove the base child element default value";
                    return false;
                }
                if (*derivedParticle.elementDefaultValue != *baseParticle.elementDefaultValue) {
                    error = "XML Schema child element restriction cannot change the base child element default value";
                    return false;
                }
            }
        }

        return true;
    };

    validateDerivedParticleRestriction = [&](const Particle& baseParticle, const Particle& derivedParticle, const std::string& label) {
        const Particle* normalizedBase = &baseParticle;
        const Particle* normalizedDerived = &derivedParticle;
        if (baseParticle.kind != derivedParticle.kind) {
            normalizedBase = unwrapEquivalentParticle(baseParticle);
            normalizedDerived = unwrapEquivalentParticle(derivedParticle);
        }

        if (normalizedBase->kind == Particle::Kind::Choice && normalizedDerived->kind != Particle::Kind::Choice) {
            std::string lastChoiceSelectionError;
            bool foundShapeCandidate = false;
            for (std::size_t index = 0; index < normalizedBase->children.size(); ++index) {
                if (!particleShapesCanCorrespond(normalizedBase->children[index], *normalizedDerived)) {
                    continue;
                }
                foundShapeCandidate = true;
                try {
                    if (normalizedDerived->minOccurs < normalizedBase->minOccurs) {
                        throw XmlException("XML Schema " + label + " cannot relax the base minOccurs");
                    }
                    if (normalizedDerived->maxOccurs > normalizedBase->maxOccurs) {
                        throw XmlException("XML Schema " + label + " cannot relax the base maxOccurs");
                    }
                    validateDerivedParticleRestriction(normalizedBase->children[index], *normalizedDerived, label + " choice selection");
                    return;
                } catch (const XmlException& exception) {
                    lastChoiceSelectionError = exception.what();
                }
            }
            if (!foundShapeCandidate) {
                throw XmlException("XML Schema " + label + " cannot select a branch outside the base choice");
            }
            if (!lastChoiceSelectionError.empty()) {
                throw XmlException(lastChoiceSelectionError);
            }
            throw XmlException("XML Schema " + label + " cannot select a branch outside the base choice");
        }

        if (normalizedBase->kind == Particle::Kind::Any) {
            if (normalizedDerived->minOccurs < normalizedBase->minOccurs) {
                throw XmlException("XML Schema " + label + " cannot relax the base minOccurs");
            }
            if (normalizedDerived->maxOccurs > normalizedBase->maxOccurs) {
                throw XmlException("XML Schema " + label + " cannot relax the base maxOccurs");
            }
            if (normalizedDerived->kind == Particle::Kind::Any) {
                if (!wildcardConstraintIsSubsetOf(normalizedDerived->namespaceUri, normalizedBase->namespaceUri)) {
                    throw XmlException("XML Schema " + label + " cannot widen the base wildcard namespace constraint");
                }
                if (processContentsStrength(normalizedDerived->processContents) < processContentsStrength(normalizedBase->processContents)) {
                    throw XmlException("XML Schema " + label + " cannot weaken the base wildcard processContents");
                }
            }
            return;
        }

        if (normalizedBase->kind != normalizedDerived->kind) {
            throw XmlException("XML Schema " + label + " must preserve the base particle kind when using restriction");
        }
        if (normalizedDerived->minOccurs < normalizedBase->minOccurs) {
            throw XmlException("XML Schema " + label + " cannot relax the base minOccurs");
        }
        if (normalizedDerived->maxOccurs > normalizedBase->maxOccurs) {
            throw XmlException("XML Schema " + label + " cannot relax the base maxOccurs");
        }

        if (normalizedBase->kind == Particle::Kind::Element) {
            if (normalizedBase->name != normalizedDerived->name || normalizedBase->namespaceUri != normalizedDerived->namespaceUri) {
                throw XmlException("XML Schema " + label + " cannot replace a base child element with a different element");
            }
            bool usesRestriction = false;
            bool usesExtension = false;
            std::string elementTypeError;
            if (!elementParticleTypeCanRestrictBase(*normalizedBase, *normalizedDerived, usesRestriction, usesExtension, elementTypeError)) {
                if (!elementTypeError.empty()) {
                    throw XmlException(elementTypeError);
                }
                throw XmlException("XML Schema " + label + " cannot replace a base child element with an incompatible element type");
            }
            if (usesRestriction) {
                if (normalizedBase->elementBlockRestriction) {
                    throw XmlException("XML Schema " + label + " cannot use a child element type restriction because the base element declaration blocks restriction derivation");
                }
                if (normalizedBase->elementFinalRestriction) {
                    throw XmlException("XML Schema " + label + " cannot use a child element type restriction because the base element declaration final blocks restriction derivation");
                }
            }
            if (usesExtension) {
                if (normalizedBase->elementBlockExtension) {
                    throw XmlException("XML Schema " + label + " cannot use a child element type extension because the base element declaration blocks extension derivation");
                }
                if (normalizedBase->elementFinalExtension) {
                    throw XmlException("XML Schema " + label + " cannot use a child element type extension because the base element declaration final blocks extension derivation");
                }
            }
            std::string declarationRestrictionError;
            if (!elementParticleDeclarationCanRestrictBase(*normalizedBase, *normalizedDerived, declarationRestrictionError)) {
                throw XmlException(declarationRestrictionError);
            }
            return;
        }

        if (normalizedBase->kind == Particle::Kind::Choice) {
            std::vector<bool> used(normalizedBase->children.size(), false);
            std::string lastChoiceError;
            std::function<bool(std::size_t, std::vector<bool>&)> tryMatchChoiceBranches;

            tryMatchChoiceBranches = [&](std::size_t derivedIndex, std::vector<bool>& currentUsed) {
                if (derivedIndex >= normalizedDerived->children.size()) {
                    return true;
                }

                const Particle& currentDerivedChild = normalizedDerived->children[derivedIndex];
                bool foundShapeCandidate = false;
                for (std::size_t baseIndex = 0; baseIndex < normalizedBase->children.size(); ++baseIndex) {
                    if ((currentUsed[baseIndex] && !choiceBranchCanBeReused(normalizedBase->children[baseIndex]))
                        || !particleShapesCanCorrespond(normalizedBase->children[baseIndex], currentDerivedChild)) {
                        continue;
                    }

                    foundShapeCandidate = true;
                    std::vector<bool> nextUsed = currentUsed;
                    if (!choiceBranchCanBeReused(normalizedBase->children[baseIndex])) {
                        nextUsed[baseIndex] = true;
                    }

                    try {
                        validateDerivedParticleRestriction(
                            normalizedBase->children[baseIndex],
                            currentDerivedChild,
                            label + " choice branch");
                    } catch (const XmlException& exception) {
                        lastChoiceError = exception.what();
                        continue;
                    }

                    if (tryMatchChoiceBranches(derivedIndex + 1, nextUsed)) {
                        currentUsed = std::move(nextUsed);
                        return true;
                    }
                }

                if (!foundShapeCandidate) {
                    lastChoiceError = "XML Schema " + label + " cannot add a new choice branch that is absent from the base type";
                }
                return false;
            };

            if (!tryMatchChoiceBranches(0, used)) {
                if (!lastChoiceError.empty()) {
                    throw XmlException(lastChoiceError);
                }
                throw XmlException("XML Schema " + label + " cannot add a new choice branch that is absent from the base type");
            }
            return;
        }

        if (normalizedBase->kind == Particle::Kind::All) {
            std::vector<bool> used(normalizedBase->children.size(), false);
            std::string lastAllError;
            std::function<bool(std::size_t, std::vector<bool>&)> tryMatchAllParticles;

            tryMatchAllParticles = [&](std::size_t derivedIndex, std::vector<bool>& currentUsed) {
                if (derivedIndex >= normalizedDerived->children.size()) {
                    for (std::size_t baseIndex = 0; baseIndex < normalizedBase->children.size(); ++baseIndex) {
                        if (!currentUsed[baseIndex] && normalizedBase->children[baseIndex].minOccurs > 0) {
                            lastAllError = "XML Schema " + label + " cannot omit required base all particles";
                            return false;
                        }
                    }
                    return true;
                }

                const Particle& currentDerivedChild = normalizedDerived->children[derivedIndex];
                bool foundShapeCandidate = false;
                for (std::size_t baseIndex = 0; baseIndex < normalizedBase->children.size(); ++baseIndex) {
                    if (currentUsed[baseIndex]
                        || !particleShapesCanCorrespond(normalizedBase->children[baseIndex], currentDerivedChild)) {
                        continue;
                    }

                    foundShapeCandidate = true;
                    std::vector<bool> nextUsed = currentUsed;
                    nextUsed[baseIndex] = true;

                    try {
                        validateDerivedParticleRestriction(
                            normalizedBase->children[baseIndex],
                            currentDerivedChild,
                            label + " all particle");
                    } catch (const XmlException& exception) {
                        lastAllError = exception.what();
                        continue;
                    }

                    if (tryMatchAllParticles(derivedIndex + 1, nextUsed)) {
                        currentUsed = std::move(nextUsed);
                        return true;
                    }
                }

                if (!foundShapeCandidate && lastAllError.empty()) {
                    lastAllError = "XML Schema " + label + " cannot add a new all particle that is absent from the base type";
                }
                return false;
            };

            if (!tryMatchAllParticles(0, used)) {
                if (!lastAllError.empty()) {
                    throw XmlException(lastAllError);
                }
                throw XmlException("XML Schema " + label + " cannot add a new all particle that is absent from the base type");
            }
            return;
        }

        std::string lastSequenceError;
        std::function<bool(std::size_t, std::size_t)> tryMatchSequenceParticles;

        tryMatchSequenceParticles = [&](std::size_t derivedIndex, std::size_t baseIndex) {
            if (derivedIndex >= normalizedDerived->children.size()) {
                for (std::size_t remainingBaseIndex = baseIndex; remainingBaseIndex < normalizedBase->children.size(); ++remainingBaseIndex) {
                    if (normalizedBase->children[remainingBaseIndex].minOccurs > 0) {
                        lastSequenceError = "XML Schema " + label + " cannot omit required base sequence particles";
                        return false;
                    }
                }
                return true;
            }

            const Particle& currentDerivedChild = normalizedDerived->children[derivedIndex];
            bool foundShapeCandidate = false;
            for (std::size_t candidateBaseIndex = baseIndex; candidateBaseIndex < normalizedBase->children.size(); ++candidateBaseIndex) {
                const Particle& candidateBaseChild = normalizedBase->children[candidateBaseIndex];
                bool skippedRequired = false;
                for (std::size_t skippedIndex = baseIndex; skippedIndex < candidateBaseIndex; ++skippedIndex) {
                    if (normalizedBase->children[skippedIndex].minOccurs > 0) {
                        skippedRequired = true;
                        break;
                    }
                }
                if (skippedRequired) {
                    lastSequenceError = "XML Schema " + label + " cannot omit or reorder required base sequence particles";
                    break;
                }

                const Particle& normalizedCandidateBaseChild = *unwrapEquivalentParticle(candidateBaseChild);
                const bool canTryDerivedSlice = (normalizedCandidateBaseChild.kind == Particle::Kind::Sequence
                    || normalizedCandidateBaseChild.kind == Particle::Kind::Choice)
                    && currentDerivedChild.kind != Particle::Kind::Sequence;

                auto trySequenceCandidate = [&](const Particle& derivedCandidate, std::size_t consumedDerivedCount) {
                    foundShapeCandidate = true;
                    try {
                        validateDerivedParticleRestriction(
                            candidateBaseChild,
                            derivedCandidate,
                            label + " sequence particle");
                    } catch (const XmlException& exception) {
                        lastSequenceError = exception.what();
                        return false;
                    }

                    return tryMatchSequenceParticles(derivedIndex + consumedDerivedCount, candidateBaseIndex + 1);
                };

                if (particleShapesCanCorrespond(candidateBaseChild, currentDerivedChild)
                    && trySequenceCandidate(currentDerivedChild, 1)) {
                    return true;
                }

                if (canTryDerivedSlice) {
                    const std::size_t maxSliceLength = normalizedDerived->children.size() - derivedIndex;
                    for (std::size_t sliceLength = 2; sliceLength <= maxSliceLength; ++sliceLength) {
                        Particle derivedSlice;
                        derivedSlice.kind = Particle::Kind::Sequence;
                        for (std::size_t sliceIndex = 0; sliceIndex < sliceLength; ++sliceIndex) {
                            derivedSlice.children.push_back(normalizedDerived->children[derivedIndex + sliceIndex]);
                        }
                        if (trySequenceCandidate(derivedSlice, sliceLength)) {
                            return true;
                        }
                    }
                }
            }

            if (!foundShapeCandidate && lastSequenceError.empty()) {
                lastSequenceError = "XML Schema " + label + " cannot add a new sequence particle that is absent from the base type";
            }
            return false;
        };

        if (!tryMatchSequenceParticles(0, 0)) {
            if (!lastSequenceError.empty()) {
                throw XmlException(lastSequenceError);
            }
            throw XmlException("XML Schema " + label + " cannot add a new sequence particle that is absent from the base type");
        }
    };

    validateAnonymousComplexTypeRestriction = [&](const ComplexTypeRule& baseRule,
        const ComplexTypeRule& derivedRule,
        const std::string& label) {
        if (baseRule.allowsText != derivedRule.allowsText) {
            throw XmlException("XML Schema " + label + " must preserve the base complexType text/content shape when using restriction");
        }

        if (baseRule.textType.has_value()) {
            if (!derivedRule.textType.has_value()) {
                throw XmlException("XML Schema " + label + " must preserve the base complexType text type when using restriction");
            }
            validateDerivedSimpleTypeRestriction(*baseRule.textType, *derivedRule.textType, label + " text content");
        } else if (derivedRule.textType.has_value()) {
            throw XmlException("XML Schema " + label + " cannot introduce text content absent from the base complexType");
        }

        const std::optional<Particle> baseParticle = makeFlatParticleFromRule(baseRule);
        const std::optional<Particle> derivedParticle = makeFlatParticleFromRule(derivedRule);
        if (derivedParticle.has_value()) {
            if (!baseParticle.has_value()) {
                throw XmlException("XML Schema " + label + " cannot introduce child particles when the base complexType has no element content");
            }
            validateDerivedParticleRestriction(*baseParticle, *derivedParticle, label);
        } else if (baseParticle.has_value()) {
            throw XmlException("XML Schema " + label + " cannot remove child particles from the base complexType");
        }

        validateAttributeRestrictionLegality(
            baseRule.attributes,
            derivedRule.attributes,
            baseRule.anyAttributeAllowed,
            baseRule.anyAttributeNamespaceConstraint,
            false,
            label);
        validateAnyAttributeRestrictionLegality(
            baseRule.anyAttributeAllowed,
            baseRule.anyAttributeNamespaceConstraint,
            baseRule.anyAttributeProcessContents,
            derivedRule.anyAttributeAllowed,
            derivedRule.anyAttributeNamespaceConstraint,
            derivedRule.anyAttributeProcessContents,
            false,
            label);
    };

    anonymousComplexTypesAreEquivalent = [&](const ComplexTypeRule& leftRule, const ComplexTypeRule& rightRule) {
        try {
            validateAnonymousComplexTypeRestriction(leftRule, rightRule, "anonymous complexType equivalence");
            validateAnonymousComplexTypeRestriction(rightRule, leftRule, "anonymous complexType equivalence");
            return true;
        } catch (const XmlException&) {
            return false;
        }
    };

    const auto mergeComplexContent = [&](ComplexTypeRule& destination, const std::optional<Particle>& extensionParticle) {
        if (!extensionParticle.has_value()) {
            return;
        }

        const auto baseParticle = makeFlatParticleFromRule(destination);
        if (!baseParticle.has_value()) {
            destination.particle = extensionParticle;
            destination.children.clear();
            destination.contentModel = ContentModel::Empty;
            return;
        }

        Particle merged;
        merged.kind = Particle::Kind::Sequence;
        merged.children.push_back(*baseParticle);
        merged.children.push_back(*extensionParticle);
        destination.particle = merged;
        destination.children.clear();
        destination.contentModel = ContentModel::Empty;
    };

    const auto validateAttributeGroupReferenceLegality = [&](const XmlElement& attributeGroupElement) {
        std::string_view nameView;
        if (TryGetAttributeValueViewInternal(attributeGroupElement, "name", nameView) && !nameView.empty()) {
            throw XmlException("XML Schema attributeGroup references cannot specify a name");
        }
        for (const auto& child : attributeGroupElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() == kSchemaNamespace
                && childElement->LocalName() != "annotation") {
                throw XmlException("XML Schema attributeGroup references cannot declare local attribute or wildcard children");
            }
        }
    };

    const auto applyAttributeChildrenToRule = [&](ComplexTypeRule& rule, const XmlElement& parentElement) {
        std::size_t anyAttributeCount = 0;
        bool inAttributeSection = false;
        for (const auto& child : parentElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }

            if (childElement->LocalName() == "attribute") {
                inAttributeSection = true;
                rule.attributes.push_back(parseAttributeUse(*childElement, false));
            } else if (childElement->LocalName() == "attributeGroup") {
                inAttributeSection = true;
                const std::string attributeGroupRef = childElement->GetAttribute("ref");
                if (attributeGroupRef.empty()) {
                    throw XmlException("XML Schema attributeGroup references require a ref");
                }
                validateAttributeGroupReferenceLegality(*childElement);
                const auto [localName, namespaceUri] = resolveTypeName(*childElement, attributeGroupRef);
                const auto referencedAttributes = ensureAttributeGroupResolved(localName, namespaceUri, attributeGroupRef);
                appendAttributes(rule.attributes, referencedAttributes);
                mergeAnyAttribute(
                    rule.anyAttributeAllowed,
                    rule.anyAttributeNamespaceConstraint,
                    rule.anyAttributeProcessContents,
                    resolvedAttributeGroupAnyAllowed[makeQualifiedRuleKey(localName, namespaceUri)],
                    resolvedAttributeGroupAnyNamespace[makeQualifiedRuleKey(localName, namespaceUri)],
                    resolvedAttributeGroupAnyProcessContents[makeQualifiedRuleKey(localName, namespaceUri)]);
            } else if (childElement->LocalName() == "anyAttribute") {
                inAttributeSection = true;
                ++anyAttributeCount;
                if (anyAttributeCount > 1) {
                    throw XmlException("XML Schema " + parentElement.LocalName() + " declarations can declare at most one anyAttribute child");
                }
                validateAnnotationOnlySchemaChildren(*childElement, "xs:anyAttribute");
                rule.anyAttributeAllowed = true;
                rule.anyAttributeNamespaceConstraint = parseWildcardNamespaceConstraintValue(*childElement);
                rule.anyAttributeProcessContents = parseProcessContentsValue(*childElement);
            } else if (inAttributeSection && childElement->LocalName() != "annotation") {
                throw XmlException("XML Schema " + parentElement.LocalName() + " declarations can only declare annotation, attribute, attributeGroup, and anyAttribute children");
            }
        }
    };

    parseAttributeUse = [&](const XmlElement& attributeElement, bool isGlobalDeclaration) -> AttributeUse {
        const std::string attributeRef = attributeElement.GetAttribute("ref");
        if (!attributeRef.empty()) {
            std::string_view nameView;
            std::string_view typeView;
            std::string_view defaultValueView;
            std::string_view fixedValueView;
            std::string_view formView;
            if (TryGetAttributeValueViewInternal(attributeElement, "name", nameView) && !nameView.empty()) {
                throw XmlException("XML Schema attribute references cannot specify a name");
            }
            if (TryGetAttributeValueViewInternal(attributeElement, "type", typeView) && !typeView.empty()) {
                throw XmlException("XML Schema attribute references cannot specify a type attribute");
            }
            if ((TryGetAttributeValueViewInternal(attributeElement, "default", defaultValueView) && !defaultValueView.empty())
                || (TryGetAttributeValueViewInternal(attributeElement, "fixed", fixedValueView) && !fixedValueView.empty())) {
                throw XmlException("XML Schema attribute references cannot specify default or fixed values");
            }
            if (TryGetAttributeValueViewInternal(attributeElement, "form", formView) && !formView.empty()) {
                throw XmlException("XML Schema attribute references cannot specify a form attribute");
            }
            for (const auto& attributeChild : attributeElement.ChildNodes()) {
                if (attributeChild == nullptr || attributeChild->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* attributeChildElement = static_cast<const XmlElement*>(attributeChild.get());
                if (attributeChildElement->NamespaceURI() == kSchemaNamespace && attributeChildElement->LocalName() == "simpleType") {
                    throw XmlException("XML Schema attribute references cannot declare an inline simpleType");
                }
                if (attributeChildElement->NamespaceURI() == kSchemaNamespace && attributeChildElement->LocalName() != "annotation") {
                    throw XmlException("XML Schema attribute references can only declare annotation children");
                }
            }
            const auto [localName, namespaceUri] = resolveTypeName(attributeElement, attributeRef);
            AttributeUse referenced = ensureAttributeResolved(localName, namespaceUri, attributeRef);
            const std::string use = parseAttributeUseValue(attributeElement);
            if (!use.empty()) {
                referenced.required = use == "required";
            }
            return referenced;
        }

        AttributeUse attributeUse;
        attributeUse.annotation = parseAnnotation(attributeElement);
        attributeUse.name = attributeElement.GetAttribute("name");
        attributeUse.namespaceUri = resolveAttributeDeclarationNamespace(attributeElement, isGlobalDeclaration);
        if (isGlobalDeclaration) {
            std::string_view minOccursView;
            std::string_view maxOccursView;
            if ((TryGetAttributeValueViewInternal(attributeElement, "minOccurs", minOccursView) && !minOccursView.empty())
                || (TryGetAttributeValueViewInternal(attributeElement, "maxOccurs", maxOccursView) && !maxOccursView.empty())) {
                throw XmlException("XML Schema global attribute declarations cannot specify minOccurs or maxOccurs");
            }
        }
        attributeUse.required = parseAttributeUseValue(attributeElement) == "required";
        const std::string defaultValue = attributeElement.GetAttribute("default");
        const std::string fixedValue = attributeElement.GetAttribute("fixed");
        if (!defaultValue.empty() && !fixedValue.empty()) {
            throw XmlException("XML Schema attribute declarations cannot specify both default and fixed values");
        }
        if (attributeUse.required && !defaultValue.empty()) {
            throw XmlException("XML Schema required attributes cannot specify a default value");
        }
        if (!defaultValue.empty()) {
            attributeUse.defaultValue = defaultValue;
        }
        if (!fixedValue.empty()) {
            attributeUse.fixedValue = fixedValue;
        }
        const std::string attributeType = attributeElement.GetAttribute("type");
        if (!attributeType.empty()) {
            if (const auto builtinType = resolveBuiltinSimpleType(attributeType); builtinType.has_value()) {
                attributeUse.type = builtinType;
            } else {
                const auto [localTypeName, typeNamespaceUri] = resolveTypeName(attributeElement, attributeType);
                attributeUse.type = ensureSimpleTypeResolved(localTypeName, typeNamespaceUri, attributeType);
            }
        }
        if (attributeUse.name.empty()) {
            throw XmlException("XML Schema attribute declarations require a name or ref");
        }
        std::size_t inlineSimpleTypeCount = 0;
        for (const auto& attributeChild : attributeElement.ChildNodes()) {
            if (attributeChild == nullptr || attributeChild->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* attributeChildElement = static_cast<const XmlElement*>(attributeChild.get());
            if (attributeChildElement->LocalName() == "simpleType" && attributeChildElement->NamespaceURI() == kSchemaNamespace) {
                ++inlineSimpleTypeCount;
                if (inlineSimpleTypeCount > 1) {
                    throw XmlException("XML Schema attribute declarations can only declare one inline simpleType child");
                }
                if (!attributeType.empty()) {
                    throw XmlException("XML Schema attribute declarations cannot combine a type attribute with an inline simpleType child");
                }
                attributeUse.type = parseSimpleType(*attributeChildElement);
            } else if (attributeChildElement->NamespaceURI() == kSchemaNamespace
                && attributeChildElement->LocalName() != "annotation") {
                throw XmlException("XML Schema attribute declarations can only declare annotation or inline simpleType children");
            }
        }

        validateDeclaredDefaultOrFixedValue(
            attributeUse.type,
            attributeUse.defaultValue,
            "default",
            "attribute",
            attributeUse.name,
            attributeElement);
        validateDeclaredDefaultOrFixedValue(
            attributeUse.type,
            attributeUse.fixedValue,
            "fixed",
            "attribute",
            attributeUse.name,
            attributeElement);

        return attributeUse;
    };

    parseAttributeGroup = [&](const XmlElement& attributeGroupElement, bool& anyAttributeAllowed, std::string& anyAttributeNamespaceConstraint, std::string& anyAttributeProcessContents) -> std::vector<AttributeUse> {
        std::vector<AttributeUse> attributes;
        std::size_t anyAttributeCount = 0;
        for (const auto& child : attributeGroupElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }
            if (childElement->LocalName() == "attribute") {
                attributes.push_back(parseAttributeUse(*childElement, false));
            } else if (childElement->LocalName() == "attributeGroup") {
                const std::string attributeGroupRef = childElement->GetAttribute("ref");
                if (attributeGroupRef.empty()) {
                    throw XmlException("XML Schema attributeGroup references require a ref");
                }
                validateAttributeGroupReferenceLegality(*childElement);
                const auto [localName, namespaceUri] = resolveTypeName(*childElement, attributeGroupRef);
                const auto referencedAttributes = ensureAttributeGroupResolved(localName, namespaceUri, attributeGroupRef);
                appendAttributes(attributes, referencedAttributes);
                mergeAnyAttribute(
                    anyAttributeAllowed,
                    anyAttributeNamespaceConstraint,
                    anyAttributeProcessContents,
                    resolvedAttributeGroupAnyAllowed[makeQualifiedRuleKey(localName, namespaceUri)],
                    resolvedAttributeGroupAnyNamespace[makeQualifiedRuleKey(localName, namespaceUri)],
                    resolvedAttributeGroupAnyProcessContents[makeQualifiedRuleKey(localName, namespaceUri)]);
            } else if (childElement->LocalName() == "anyAttribute") {
                ++anyAttributeCount;
                if (anyAttributeCount > 1) {
                    throw XmlException("XML Schema attributeGroup declarations can declare at most one anyAttribute child");
                }
                validateAnnotationOnlySchemaChildren(*childElement, "xs:anyAttribute");
                anyAttributeAllowed = true;
                anyAttributeNamespaceConstraint = parseWildcardNamespaceConstraintValue(*childElement);
                anyAttributeProcessContents = parseProcessContentsValue(*childElement);
            } else if (childElement->LocalName() != "annotation") {
                throw XmlException("XML Schema attributeGroup declarations can only declare annotation, attribute, attributeGroup, and anyAttribute children");
            }
        }
        return attributes;
    };

    const auto validateAllChildParticleOccurs = [&](const XmlElement& childElement) {
        const std::string childLocalName = childElement.LocalName();
        if (childLocalName != "element" && childLocalName != "any" && childLocalName != "group") {
            throw XmlException("XML Schema xs:all currently supports only child element particles, wildcards, and group references");
        }

        const std::size_t childMinOccurs = parseOccurs(childElement.GetAttribute("minOccurs"), 1);
        const std::size_t childMaxOccurs = parseOccurs(childElement.GetAttribute("maxOccurs"), 1);
        if (childLocalName == "element" || childLocalName == "any") {
            if (childMaxOccurs > 1) {
                throw XmlException(
                    "XML Schema xs:all " + std::string(childLocalName == "element" ? "child elements" : "child wildcards")
                    + " currently support maxOccurs up to 1");
            }
            if (childMinOccurs > 1) {
                throw XmlException(
                    "XML Schema xs:all " + std::string(childLocalName == "element" ? "child elements" : "child wildcards")
                    + " currently support minOccurs up to 1");
            }
            return;
        }

        if (childMaxOccurs > 1) {
            throw XmlException("XML Schema xs:all group references currently support maxOccurs up to 1");
        }
        if (childMinOccurs > 1) {
            throw XmlException("XML Schema xs:all group references currently support minOccurs up to 1");
        }
    };

    const auto validateAllChildParticleShape = [&](const XmlElement& childElement, const Particle& childParticle) {
        if (childElement.LocalName() == "group" && childParticle.kind != Particle::Kind::All) {
            throw XmlException("XML Schema xs:all group references currently support only groups that resolve to xs:all");
        }
    };

    const auto applyAttributeRestrictionsToRule = [&](ComplexTypeRule& rule, const XmlElement& parentElement, bool allowNewAttributes, const std::string& label) {
        const std::vector<AttributeUse> baseAttributes = rule.attributes;
        std::vector<AttributeUse> restrictedAttributes = rule.attributes;
        const bool baseAnyAttributeAllowed = rule.anyAttributeAllowed;
        const std::string baseAnyAttributeNamespaceConstraint = rule.anyAttributeNamespaceConstraint;
        const std::string baseAnyAttributeProcessContents = rule.anyAttributeProcessContents;
        bool anyAttributeAllowed = rule.anyAttributeAllowed;
        std::string anyAttributeNamespaceConstraint = rule.anyAttributeNamespaceConstraint;
        std::string anyAttributeProcessContents = rule.anyAttributeProcessContents;
        std::size_t anyAttributeCount = 0;

        for (const auto& child : parentElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }

            if (childElement->LocalName() == "attribute") {
                if (AttributeValueEqualsInternal(*childElement, "use", "prohibited")) {
                    std::string name = childElement->GetAttribute("name");
                    std::string namespaceUri;
                    const std::string attributeRef = childElement->GetAttribute("ref");
                    if (!attributeRef.empty()) {
                        const auto resolved = resolveTypeName(*childElement, attributeRef);
                        name = resolved.first;
                        namespaceUri = resolved.second;
                    }
                    if (name.empty()) {
                        throw XmlException("XML Schema prohibited attribute restrictions require a name or ref");
                    }
                    eraseAttributeUse(restrictedAttributes, name, namespaceUri);
                    continue;
                }

                upsertAttributeUse(restrictedAttributes, parseAttributeUse(*childElement, false));
            } else if (childElement->LocalName() == "attributeGroup") {
                const std::string attributeGroupRef = childElement->GetAttribute("ref");
                if (attributeGroupRef.empty()) {
                    throw XmlException("XML Schema attributeGroup references require a ref");
                }
                validateAttributeGroupReferenceLegality(*childElement);
                const auto [localName, namespaceUri] = resolveTypeName(*childElement, attributeGroupRef);
                const auto referencedAttributes = ensureAttributeGroupResolved(localName, namespaceUri, attributeGroupRef);
                for (const auto& referencedAttribute : referencedAttributes) {
                    upsertAttributeUse(restrictedAttributes, referencedAttribute);
                }
                const std::string ruleKey = makeQualifiedRuleKey(localName, namespaceUri);
                mergeAnyAttribute(
                    anyAttributeAllowed,
                    anyAttributeNamespaceConstraint,
                    anyAttributeProcessContents,
                    resolvedAttributeGroupAnyAllowed[ruleKey],
                    resolvedAttributeGroupAnyNamespace[ruleKey],
                    resolvedAttributeGroupAnyProcessContents[ruleKey]);
            } else if (childElement->LocalName() == "anyAttribute") {
                ++anyAttributeCount;
                if (anyAttributeCount > 1) {
                    throw XmlException("XML Schema " + parentElement.LocalName() + " declarations can declare at most one anyAttribute child");
                }
                validateAnnotationOnlySchemaChildren(*childElement, "xs:anyAttribute");
                anyAttributeAllowed = true;
                anyAttributeNamespaceConstraint = parseWildcardNamespaceConstraintValue(*childElement);
                anyAttributeProcessContents = parseProcessContentsValue(*childElement);
            }
        }

        validateAttributeRestrictionLegality(
            baseAttributes,
            restrictedAttributes,
            baseAnyAttributeAllowed,
            baseAnyAttributeNamespaceConstraint,
            allowNewAttributes,
            label);
        validateAnyAttributeRestrictionLegality(
            baseAnyAttributeAllowed,
            baseAnyAttributeNamespaceConstraint,
            baseAnyAttributeProcessContents,
            anyAttributeAllowed,
            anyAttributeNamespaceConstraint,
            anyAttributeProcessContents,
            allowNewAttributes,
            label);
        rule.attributes = std::move(restrictedAttributes);
        rule.anyAttributeAllowed = anyAttributeAllowed;
        rule.anyAttributeNamespaceConstraint = anyAttributeNamespaceConstraint;
        rule.anyAttributeProcessContents = anyAttributeProcessContents;
    };

    parseParticle = [&](const XmlElement& particleElement) -> Particle {
        Particle particle;
        particle.minOccurs = parseOccurs(particleElement.GetAttribute("minOccurs"), 1);
        particle.maxOccurs = parseOccurs(particleElement.GetAttribute("maxOccurs"), 1);

        if (particleElement.LocalName() == "element") {
            validateParticleOccursRange(particle.minOccurs, particle.maxOccurs);
            particle.kind = Particle::Kind::Element;
            const std::string elementRef = particleElement.GetAttribute("ref");
            if (!elementRef.empty()) {
                std::string_view nameView;
                std::string_view typeView;
                std::string_view defaultValueView;
                std::string_view fixedValueView;
                std::string_view nillableView;
                std::string_view blockView;
                std::string_view finalView;
                std::string_view formView;
                    std::string_view abstractView;
                    std::string_view substitutionGroupView;
                if (TryGetAttributeValueViewInternal(particleElement, "name", nameView) && !nameView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a name");
                }
                if (TryGetAttributeValueViewInternal(particleElement, "type", typeView) && !typeView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a type attribute");
                }
                if ((TryGetAttributeValueViewInternal(particleElement, "default", defaultValueView) && !defaultValueView.empty())
                    || (TryGetAttributeValueViewInternal(particleElement, "fixed", fixedValueView) && !fixedValueView.empty())) {
                    throw XmlException("XML Schema element references cannot specify default or fixed values");
                }
                if (TryGetAttributeValueViewInternal(particleElement, "nillable", nillableView) && !nillableView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a nillable attribute");
                }
                if (TryGetAttributeValueViewInternal(particleElement, "block", blockView) && !blockView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a block attribute");
                }
                if (TryGetAttributeValueViewInternal(particleElement, "final", finalView) && !finalView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a final attribute");
                }
                if (TryGetAttributeValueViewInternal(particleElement, "form", formView) && !formView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a form attribute");
                }
                    if (TryGetAttributeValueViewInternal(particleElement, "abstract", abstractView) && !abstractView.empty()) {
                        throw XmlException("XML Schema element references cannot specify an abstract attribute");
                    }
                    if (TryGetAttributeValueViewInternal(particleElement, "substitutionGroup", substitutionGroupView) && !substitutionGroupView.empty()) {
                        throw XmlException("XML Schema element references cannot specify a substitutionGroup attribute");
                    }
                for (const auto& child : particleElement.ChildNodes()) {
                    if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* childElement = static_cast<const XmlElement*>(child.get());
                    if (childElement->NamespaceURI() != kSchemaNamespace) {
                        continue;
                    }
                    if (childElement->LocalName() == "simpleType" || childElement->LocalName() == "complexType") {
                        throw XmlException("XML Schema element references cannot declare an inline simpleType or complexType child");
                    }
                    if (childElement->LocalName() == "key"
                        || childElement->LocalName() == "unique"
                        || childElement->LocalName() == "keyref") {
                        throw XmlException("XML Schema element references cannot declare identity constraints");
                    }
                    if (childElement->LocalName() != "annotation") {
                        throw XmlException("XML Schema element references can only declare annotation children");
                    }
                }
                const auto [localName, namespaceUri] = resolveTypeName(particleElement, elementRef);
                const ElementRule referencedRule = ensureElementResolved(localName, namespaceUri, elementRef);
                particle.name = referencedRule.name;
                particle.namespaceUri = referencedRule.namespaceUri;
                particle.elementSimpleType = referencedRule.declaredSimpleType;
                if (referencedRule.declaredComplexType.has_value()) {
                    particle.elementComplexType = std::make_shared<ComplexTypeRule>(*referencedRule.declaredComplexType);
                }
                particle.elementIsNillable = referencedRule.isNillable;
                particle.elementDefaultValue = referencedRule.defaultValue;
                particle.elementFixedValue = referencedRule.fixedValue;
                particle.elementBlockRestriction = referencedRule.blockRestriction;
                particle.elementBlockExtension = referencedRule.blockExtension;
                particle.elementFinalRestriction = referencedRule.finalRestriction;
                particle.elementFinalExtension = referencedRule.finalExtension;
            } else {
                const ElementRule localRule = parseElementRule(particleElement, false);
                particle.name = localRule.name;
                particle.namespaceUri = localRule.namespaceUri;
                if (particle.name.empty()) {
                    throw XmlException("XML Schema child element declarations require a name or ref");
                }
                particle.elementSimpleType = localRule.declaredSimpleType;
                if (localRule.declaredComplexType.has_value()) {
                    particle.elementComplexType = std::make_shared<ComplexTypeRule>(*localRule.declaredComplexType);
                }
                particle.elementIsNillable = localRule.isNillable;
                particle.elementDefaultValue = localRule.defaultValue;
                particle.elementFixedValue = localRule.fixedValue;
                particle.elementBlockRestriction = localRule.blockRestriction;
                particle.elementBlockExtension = localRule.blockExtension;
                particle.elementFinalRestriction = localRule.finalRestriction;
                particle.elementFinalExtension = localRule.finalExtension;
                upsertRule(localRule);
            }
            return particle;
        }

        if (particleElement.LocalName() == "any") {
            validateParticleOccursRange(particle.minOccurs, particle.maxOccurs);
            validateAnnotationOnlySchemaChildren(particleElement, "xs:any");
            particle.kind = Particle::Kind::Any;
            particle.namespaceUri = parseWildcardNamespaceConstraintValue(particleElement);
            particle.processContents = parseProcessContentsValue(particleElement);
            return particle;
        }

        if (particleElement.LocalName() == "group") {
            validateParticleOccursRange(particle.minOccurs, particle.maxOccurs);
            const std::string groupRef = particleElement.GetAttribute("ref");
            if (groupRef.empty()) {
                throw XmlException("XML Schema group references require a ref");
            }
            std::string_view nameView;
            if (TryGetAttributeValueViewInternal(particleElement, "name", nameView) && !nameView.empty()) {
                throw XmlException("XML Schema group references cannot specify a name");
            }
            for (const auto& child : particleElement.ChildNodes()) {
                if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* childElement = static_cast<const XmlElement*>(child.get());
                if (childElement->NamespaceURI() == kSchemaNamespace
                    && childElement->LocalName() != "annotation") {
                    throw XmlException("XML Schema group references cannot declare inline content model children");
                }
            }

            const std::size_t minOccurs = particle.minOccurs;
            const std::size_t maxOccurs = particle.maxOccurs;
            const auto [localName, namespaceUri] = resolveTypeName(particleElement, groupRef);
            particle = ensureGroupResolved(localName, namespaceUri, groupRef);
            particle.minOccurs = minOccurs;
            particle.maxOccurs = maxOccurs;
            return particle;
        }

        if (particleElement.LocalName() == "all") {
            particle.kind = Particle::Kind::All;
            if (particle.maxOccurs > 1) {
                throw XmlException("XML Schema xs:all currently supports maxOccurs up to 1");
            }
            if (particle.minOccurs > 1) {
                throw XmlException("XML Schema xs:all currently supports minOccurs up to 1");
            }
        } else {
            particle.kind = particleElement.LocalName() == "choice" ? Particle::Kind::Choice : Particle::Kind::Sequence;
        }
        validateParticleOccursRange(particle.minOccurs, particle.maxOccurs);
        for (const auto& child : particleElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }
            if (childElement->LocalName() == "annotation") {
                continue;
            }
            if (particle.kind == Particle::Kind::All) {
                validateAllChildParticleOccurs(*childElement);
            }
            if (childElement->LocalName() == "element"
                || childElement->LocalName() == "any"
                || childElement->LocalName() == "group"
                || childElement->LocalName() == "sequence"
                || childElement->LocalName() == "choice"
                || childElement->LocalName() == "all") {
                Particle childParticle = parseParticle(*childElement);
                if (particle.kind == Particle::Kind::All) {
                    validateAllChildParticleShape(*childElement, childParticle);
                }
                particle.children.push_back(std::move(childParticle));
            } else {
                throw XmlException("XML Schema sequence, choice, and all compositors can only declare annotation or particle children");
            }
        }
        return particle;
    };

    ensureComplexTypeResolved = [&](const std::string& localTypeName, const std::string& typeNamespaceUri, const std::string& qualifiedName) -> ComplexTypeRule {
        const std::string ruleKey = makeQualifiedRuleKey(localTypeName, typeNamespaceUri);
        const auto declaration = declaredComplexTypes.find(ruleKey);
        if (declaration == declaredComplexTypes.end()) {
            if (const auto* namedType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                return *namedType;
            }
            throw XmlException("XML Schema complexType '" + qualifiedName + "' is not supported");
        }

        if (resolvedLocalComplexTypes.find(ruleKey) != resolvedLocalComplexTypes.end()) {
            if (const auto* namedType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                return *namedType;
            }
        }

        if (!resolvingComplexTypes.insert(ruleKey).second) {
            throw XmlException("XML Schema complexType '" + qualifiedName + "' contains a circular type reference");
        }

        ComplexTypeRule rule;
        try {
            rule = parseComplexType(*declaration->second);
        } catch (...) {
            resolvingComplexTypes.erase(ruleKey);
            throw;
        }
        resolvingComplexTypes.erase(ruleKey);

        rule.namedTypeName = localTypeName;
        rule.namedTypeNamespaceUri = typeNamespaceUri;

        upsertComplexType(NamedComplexTypeRule{localTypeName, typeNamespaceUri, rule});
        resolvedLocalComplexTypes.insert(ruleKey);
        return rule;
    };

    ensureElementResolved = [&](const std::string& localName, const std::string& namespaceUri, const std::string& qualifiedName) -> ElementRule {
        const std::string ruleKey = makeQualifiedRuleKey(localName, namespaceUri);
        const auto declaration = declaredElements.find(ruleKey);
        if (declaration == declaredElements.end()) {
            if (const auto* rule = FindElementRule(localName, namespaceUri)) {
                return *rule;
            }
            throw XmlException("XML Schema element '" + qualifiedName + "' is not supported");
        }

        if (resolvedLocalElements.find(ruleKey) != resolvedLocalElements.end()) {
            if (const auto* rule = FindElementRule(localName, namespaceUri)) {
                return *rule;
            }
        }

        if (!resolvingElements.insert(ruleKey).second) {
            throw XmlException("XML Schema element '" + qualifiedName + "' contains a circular element reference");
        }

        ElementRule rule;
        try {
            rule = parseElementRule(*declaration->second, true);
        } catch (...) {
            resolvingElements.erase(ruleKey);
            throw;
        }
        resolvingElements.erase(ruleKey);

        upsertRule(rule);
        resolvedLocalElements.insert(ruleKey);
        return rule;
    };

    ensureGroupResolved = [&](const std::string& localName, const std::string& namespaceUri, const std::string& qualifiedName) -> Particle {
        const std::string ruleKey = makeQualifiedRuleKey(localName, namespaceUri);
        if (const auto found = resolvedGroups.find(ruleKey); found != resolvedGroups.end()) {
            return found->second;
        }

        const auto declaration = declaredGroups.find(ruleKey);
        if (declaration == declaredGroups.end()) {
            if (const auto* rule = FindGroupRule(localName, namespaceUri)) {
                resolvedGroups[ruleKey] = *rule;
                return *rule;
            }
            throw XmlException("XML Schema group '" + qualifiedName + "' is not supported");
        }
        if (!resolvingGroups.insert(ruleKey).second) {
            throw XmlException("XML Schema group '" + qualifiedName + "' contains a circular group reference");
        }

        Particle particle;
        std::size_t contentChildCount = 0;
        try {
            for (const auto& child : declaration->second->ChildNodes()) {
                if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* childElement = static_cast<const XmlElement*>(child.get());
                if (childElement->NamespaceURI() != kSchemaNamespace) {
                    continue;
                }
                if (childElement->LocalName() == "sequence"
                    || childElement->LocalName() == "choice"
                    || childElement->LocalName() == "group"
                    || childElement->LocalName() == "all") {
                    ++contentChildCount;
                    if (contentChildCount > 1) {
                        throw XmlException("XML Schema group declarations can only declare one sequence, choice, all, or group child");
                    }
                    particle = parseParticle(*childElement);
                } else if (childElement->LocalName() != "annotation") {
                    throw XmlException("XML Schema group declarations can only declare annotation and a single content model child");
                }
            }
        } catch (...) {
            resolvingGroups.erase(ruleKey);
            throw;
        }
        resolvingGroups.erase(ruleKey);

        if (contentChildCount == 0) {
            throw XmlException("XML Schema group '" + qualifiedName + "' must contain a sequence, choice, all, or group");
        }
        resolvedGroups[ruleKey] = particle;
        upsertGroup(NamedGroupRule{localName, namespaceUri, particle});
        return particle;
    };

    ensureAttributeResolved = [&](const std::string& localName, const std::string& namespaceUri, const std::string& qualifiedName) -> AttributeUse {
        const std::string ruleKey = makeQualifiedRuleKey(localName, namespaceUri);
        if (const auto found = resolvedAttributes.find(ruleKey); found != resolvedAttributes.end()) {
            return found->second;
        }

        const auto declaration = declaredAttributes.find(ruleKey);
        if (declaration == declaredAttributes.end()) {
            if (const auto* namedAttribute = FindAttributeRule(localName, namespaceUri)) {
                return *namedAttribute;
            }
            throw XmlException("XML Schema attribute '" + qualifiedName + "' is not supported");
        }
        if (!resolvingAttributes.insert(ruleKey).second) {
            throw XmlException("XML Schema attribute '" + qualifiedName + "' contains a circular attribute reference");
        }

        AttributeUse attributeUse;
        try {
            attributeUse = parseAttributeUse(*declaration->second, true);
        } catch (...) {
            resolvingAttributes.erase(ruleKey);
            throw;
        }
        resolvingAttributes.erase(ruleKey);

        resolvedAttributes[ruleKey] = attributeUse;
        upsertAttribute(NamedAttributeRule{localName, namespaceUri, attributeUse});
        return attributeUse;
    };

    ensureAttributeGroupResolved = [&](const std::string& localName, const std::string& namespaceUri, const std::string& qualifiedName) -> std::vector<AttributeUse> {
        const std::string ruleKey = makeQualifiedRuleKey(localName, namespaceUri);
        if (const auto found = resolvedAttributeGroups.find(ruleKey); found != resolvedAttributeGroups.end()) {
            return found->second;
        }

        const auto declaration = declaredAttributeGroups.find(ruleKey);
        if (declaration == declaredAttributeGroups.end()) {
            if (const auto* namedAttributeGroup = FindAttributeGroupRule(localName, namespaceUri)) {
                resolvedAttributeGroups[ruleKey] = namedAttributeGroup->attributes;
                resolvedAttributeGroupAnyAllowed[ruleKey] = namedAttributeGroup->anyAttributeAllowed;
                resolvedAttributeGroupAnyNamespace[ruleKey] = namedAttributeGroup->anyAttributeNamespaceConstraint;
                resolvedAttributeGroupAnyProcessContents[ruleKey] = namedAttributeGroup->anyAttributeProcessContents;
                return namedAttributeGroup->attributes;
            }
            throw XmlException("XML Schema attributeGroup '" + qualifiedName + "' is not supported");
        }
        if (!resolvingAttributeGroups.insert(ruleKey).second) {
            throw XmlException("XML Schema attributeGroup '" + qualifiedName + "' contains a circular attributeGroup reference");
        }

        std::vector<AttributeUse> attributes;
        bool anyAttributeAllowed = false;
        std::string anyAttributeNamespaceConstraint = "##any";
        std::string anyAttributeProcessContents = "strict";
        try {
            attributes = parseAttributeGroup(*declaration->second, anyAttributeAllowed, anyAttributeNamespaceConstraint, anyAttributeProcessContents);
        } catch (...) {
            resolvingAttributeGroups.erase(ruleKey);
            throw;
        }
        resolvingAttributeGroups.erase(ruleKey);

        resolvedAttributeGroups[ruleKey] = attributes;
        resolvedAttributeGroupAnyAllowed[ruleKey] = anyAttributeAllowed;
        resolvedAttributeGroupAnyNamespace[ruleKey] = anyAttributeNamespaceConstraint;
        resolvedAttributeGroupAnyProcessContents[ruleKey] = anyAttributeProcessContents;
        upsertAttributeGroup(NamedAttributeGroupRule{
            localName,
            namespaceUri,
            AttributeGroupRule{attributes, anyAttributeAllowed, anyAttributeNamespaceConstraint, anyAttributeProcessContents},
        });
        return attributes;
    };

    parseComplexType = [&](const XmlElement& complexTypeElement) -> ComplexTypeRule {
        const XmlSchemaSet::Annotation declarationAnnotation = parseAnnotation(complexTypeElement);
        const bool declaredComplexTypeIsAbstract = parseSchemaBooleanAttribute(complexTypeElement, "abstract");
        ComplexTypeRule rule;
        rule.annotation = declarationAnnotation;
        rule.namedTypeName.clear();
        rule.namedTypeNamespaceUri.clear();
        rule.derivationMethod = ComplexTypeRule::DerivationMethod::None;
        rule.derivationBaseName.clear();
        rule.derivationBaseNamespaceUri.clear();
        rule.isAbstract = declaredComplexTypeIsAbstract;
        rule.allowsText = parseSchemaBooleanAttribute(complexTypeElement, "mixed");
        rule.contentModel = ContentModel::Empty;
        const std::string effectiveBlock = effectiveDerivationControlValue(complexTypeElement.GetAttribute("block"), schemaBlockDefault);
        const std::string effectiveFinal = effectiveDerivationControlValue(complexTypeElement.GetAttribute("final"), schemaFinalDefault);
        rule.blockRestriction = containsDerivationToken(effectiveBlock, "restriction");
        rule.blockExtension = containsDerivationToken(effectiveBlock, "extension");
        rule.finalRestriction = containsDerivationToken(effectiveFinal, "restriction");
        rule.finalExtension = containsDerivationToken(effectiveFinal, "extension");

        std::size_t topLevelContentModelCount = 0;
        bool hasSimpleContentChild = false;
        bool hasComplexContentChild = false;
        std::size_t nonAnnotationSchemaChildCount = 0;
        for (const auto& child : complexTypeElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }

            const std::string localName = childElement->LocalName();
            if (localName != "annotation") {
                ++nonAnnotationSchemaChildCount;
            }
            const bool isTopLevelContentModel = localName == "simpleContent"
                || localName == "complexContent"
                || localName == "sequence"
                || localName == "choice"
                || localName == "group"
                || localName == "all"
                || localName == "any";

            if (isTopLevelContentModel) {
                ++topLevelContentModelCount;
            }
            if (localName == "simpleContent") {
                hasSimpleContentChild = true;
            } else if (localName == "complexContent") {
                hasComplexContentChild = true;
            }
        }

        if (topLevelContentModelCount > 1) {
            throw XmlException(
                "XML Schema complexType can only declare one top-level content model child");
        }
        if ((hasSimpleContentChild || hasComplexContentChild) && nonAnnotationSchemaChildCount > 1) {
            throw XmlException(
                "XML Schema complexType simpleContent/complexContent must be the only top-level content child");
        }

        struct SimpleContentBaseInfo {
            SimpleTypeRule textType;
            std::vector<AttributeUse> attributes;
            bool attributesMayBeIntroduced = false;
            bool anyAttributeAllowed = false;
            std::string anyAttributeNamespaceConstraint = "##any";
            std::string anyAttributeProcessContents = "strict";
        };

        const auto resolveSimpleContentBase = [&](const XmlElement& context, const std::string& baseType, const std::string& description) {
            SimpleContentBaseInfo baseInfo;
            if (const auto builtinType = resolveBuiltinSimpleType(baseType); builtinType.has_value()) {
                baseInfo.textType = *builtinType;
                baseInfo.attributesMayBeIntroduced = true;
                return baseInfo;
            }

            const auto [localTypeName, typeNamespaceUri] = resolveTypeName(context, baseType);
            if (const auto* namedSimpleType = FindSimpleTypeRule(localTypeName, typeNamespaceUri)) {
                baseInfo.textType = *namedSimpleType;
                baseInfo.attributesMayBeIntroduced = true;
                return baseInfo;
            }
            if (declaredSimpleTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredSimpleTypes.end()) {
                baseInfo.textType = ensureSimpleTypeResolved(localTypeName, typeNamespaceUri, baseType);
                baseInfo.attributesMayBeIntroduced = true;
                return baseInfo;
            }

            ComplexTypeRule baseRule;
            if (const auto builtinComplexType = resolveBuiltinComplexType(baseType); builtinComplexType.has_value()) {
                baseRule = *builtinComplexType;
            } else if (const auto* namedComplexType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                baseRule = *namedComplexType;
            } else if (declaredComplexTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredComplexTypes.end()) {
                baseRule = ensureComplexTypeResolved(localTypeName, typeNamespaceUri, baseType);
            } else {
                throw XmlException("XML Schema " + description + " base '" + baseType + "' is not supported");
            }

            if (!baseRule.allowsText || !baseRule.textType.has_value()) {
                throw XmlException("XML Schema " + description + " base '" + baseType + "' must have simple content");
            }

            baseInfo.textType = *baseRule.textType;
            baseInfo.attributes = baseRule.attributes;
            baseInfo.anyAttributeAllowed = baseRule.anyAttributeAllowed;
            baseInfo.anyAttributeNamespaceConstraint = baseRule.anyAttributeNamespaceConstraint;
            baseInfo.anyAttributeProcessContents = baseRule.anyAttributeProcessContents;
            return baseInfo;
        };

        const auto ensureComplexTypeFinalAllowsDerivation = [&](const ComplexTypeRule& baseRule,
            const std::string& baseType,
            const std::string& description,
            bool isRestriction) {
            const bool blocked = isRestriction ? baseRule.finalRestriction : baseRule.finalExtension;
            if (!blocked) {
                return;
            }

            throw XmlException(
                "XML Schema " + description + " base '" + baseType + "' blocks "
                + (isRestriction ? std::string("restriction") : std::string("extension"))
                + " derivation via complexType final");
        };

        for (const auto& child : complexTypeElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }

            if (childElement->LocalName() == "simpleContent") {
                const XmlElement* extensionElement = nullptr;
                const XmlElement* restrictionElement = nullptr;
                int extensionCount = 0;
                int restrictionCount = 0;
                for (const auto& contentChild : childElement->ChildNodes()) {
                    if (contentChild == nullptr || contentChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* contentChildElement = static_cast<const XmlElement*>(contentChild.get());
                    if (contentChildElement->NamespaceURI() == kSchemaNamespace
                        && contentChildElement->LocalName() == "extension") {
                        extensionElement = contentChildElement;
                        ++extensionCount;
                    }
                    if (contentChildElement->NamespaceURI() == kSchemaNamespace
                        && contentChildElement->LocalName() == "restriction") {
                        restrictionElement = contentChildElement;
                        ++restrictionCount;
                    }
                }
                if (extensionCount + restrictionCount != 1) {
                    throw XmlException("XML Schema simpleContent must declare exactly one extension or restriction child");
                }

                if (restrictionElement != nullptr) {
                    const std::string baseType = restrictionElement->GetAttribute("base");
                    if (baseType.empty()) {
                        throw XmlException("XML Schema simpleContent restriction requires a base type");
                    }

                    const auto baseInfo = resolveSimpleContentBase(*restrictionElement, baseType, "simpleContent restriction");
                    const auto [localTypeName, typeNamespaceUri] = resolveTypeName(*restrictionElement, baseType);
                    if (const auto* namedComplexType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                        ensureComplexTypeFinalAllowsDerivation(*namedComplexType, baseType, "simpleContent restriction", true);
                    } else if (declaredComplexTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredComplexTypes.end()) {
                        ensureComplexTypeFinalAllowsDerivation(
                            ensureComplexTypeResolved(localTypeName, typeNamespaceUri, baseType),
                            baseType,
                            "simpleContent restriction",
                            true);
                    }
                    rule.allowsText = true;
                    rule.derivationMethod = ComplexTypeRule::DerivationMethod::Restriction;
                    rule.derivationBaseName = localTypeName;
                    rule.derivationBaseNamespaceUri = typeNamespaceUri;
                    rule.textType = baseInfo.textType;
                    rule.attributes = baseInfo.attributes;
                    rule.anyAttributeAllowed = baseInfo.anyAttributeAllowed;
                    rule.anyAttributeNamespaceConstraint = baseInfo.anyAttributeNamespaceConstraint;
                    rule.anyAttributeProcessContents = baseInfo.anyAttributeProcessContents;

                    for (const auto& restrictionChild : restrictionElement->ChildNodes()) {
                        if (restrictionChild == nullptr || restrictionChild->NodeType() != XmlNodeType::Element) {
                            continue;
                        }
                        const auto* restrictionChildElement = static_cast<const XmlElement*>(restrictionChild.get());
                        if (restrictionChildElement->LocalName() == "simpleType" && restrictionChildElement->NamespaceURI() == kSchemaNamespace) {
                            rule.textType = parseSimpleType(*restrictionChildElement);
                        } else if (isSimpleTypeFacetElement(*restrictionChildElement) && rule.textType.has_value()) {
                            applyFacetToSimpleType(*rule.textType, *restrictionChildElement);
                        } else if (restrictionChildElement->NamespaceURI() == kSchemaNamespace
                            && restrictionChildElement->LocalName() != "annotation"
                            && restrictionChildElement->LocalName() != "attribute"
                            && restrictionChildElement->LocalName() != "attributeGroup"
                            && restrictionChildElement->LocalName() != "anyAttribute") {
                            throw XmlException("XML Schema simpleContent restriction can only declare annotation, simpleType, facet, or attribute children");
                        }
                    }

                    if (rule.textType.has_value()) {
                        validateDerivedSimpleTypeRestriction(baseInfo.textType, *rule.textType,
                            "simpleContent restriction base '" + baseType + "'");
                    }

                    applyAttributeRestrictionsToRule(rule, *restrictionElement, baseInfo.attributesMayBeIntroduced,
                        "simpleContent restriction base '" + baseType + "'");
                    rule.annotation = declarationAnnotation;
                    return rule;
                }

                const std::string baseType = extensionElement->GetAttribute("base");
                if (baseType.empty()) {
                    throw XmlException("XML Schema simpleContent extension requires a base type");
                }

                rule.allowsText = true;
                if (const auto builtinType = resolveBuiltinSimpleType(baseType); builtinType.has_value()) {
                    rule.textType = builtinType;
                    rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                    rule.derivationBaseName.clear();
                    rule.derivationBaseNamespaceUri.clear();
                } else {
                    const auto [localTypeName, typeNamespaceUri] = resolveTypeName(*extensionElement, baseType);
                    if (const auto* namedSimpleType = FindSimpleTypeRule(localTypeName, typeNamespaceUri)) {
                        rule.textType = *namedSimpleType;
                        rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                        rule.derivationBaseName.clear();
                        rule.derivationBaseNamespaceUri.clear();
                    } else if (declaredSimpleTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredSimpleTypes.end()) {
                        rule.textType = ensureSimpleTypeResolved(localTypeName, typeNamespaceUri, baseType);
                        rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                        rule.derivationBaseName.clear();
                        rule.derivationBaseNamespaceUri.clear();
                    } else if (const auto builtinComplexType = resolveBuiltinComplexType(baseType); builtinComplexType.has_value()) {
                        if (!builtinComplexType->allowsText || !builtinComplexType->textType.has_value()) {
                            throw XmlException("XML Schema simpleContent extension base '" + baseType + "' must have simple content");
                        }
                        rule = *builtinComplexType;
                        rule.isAbstract = declaredComplexTypeIsAbstract;
                        rule.allowsText = true;
                        rule.namedTypeName.clear();
                        rule.namedTypeNamespaceUri.clear();
                        rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                        rule.derivationBaseName = localTypeName;
                        rule.derivationBaseNamespaceUri = typeNamespaceUri;
                    } else if (const auto* namedComplexType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                        ensureComplexTypeFinalAllowsDerivation(*namedComplexType, baseType, "simpleContent extension", false);
                        if (!namedComplexType->allowsText || !namedComplexType->textType.has_value()) {
                            throw XmlException("XML Schema simpleContent extension base '" + baseType + "' must have simple content");
                        }
                        rule = *namedComplexType;
                        rule.isAbstract = declaredComplexTypeIsAbstract;
                        rule.allowsText = true;
                        rule.namedTypeName.clear();
                        rule.namedTypeNamespaceUri.clear();
                        rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                        rule.derivationBaseName = localTypeName;
                        rule.derivationBaseNamespaceUri = typeNamespaceUri;
                    } else if (declaredComplexTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredComplexTypes.end()) {
                        const ComplexTypeRule baseRule = ensureComplexTypeResolved(localTypeName, typeNamespaceUri, baseType);
                        ensureComplexTypeFinalAllowsDerivation(baseRule, baseType, "simpleContent extension", false);
                        if (!baseRule.allowsText || !baseRule.textType.has_value()) {
                            throw XmlException("XML Schema simpleContent extension base '" + baseType + "' must have simple content");
                        }
                        rule = baseRule;
                        rule.isAbstract = declaredComplexTypeIsAbstract;
                        rule.allowsText = true;
                        rule.namedTypeName.clear();
                        rule.namedTypeNamespaceUri.clear();
                        rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                        rule.derivationBaseName = localTypeName;
                        rule.derivationBaseNamespaceUri = typeNamespaceUri;
                    } else {
                        throw XmlException("XML Schema simpleContent base '" + baseType + "' is not supported");
                    }
                }

                for (const auto& extensionChild : extensionElement->ChildNodes()) {
                    if (extensionChild == nullptr || extensionChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* extensionChildElement = static_cast<const XmlElement*>(extensionChild.get());
                    if (extensionChildElement->NamespaceURI() != kSchemaNamespace) {
                        continue;
                    }
                    if (extensionChildElement->LocalName() != "annotation"
                        && extensionChildElement->LocalName() != "attribute"
                        && extensionChildElement->LocalName() != "attributeGroup"
                        && extensionChildElement->LocalName() != "anyAttribute") {
                        throw XmlException("XML Schema simpleContent extension can only declare annotation or attribute children");
                    }
                }

                applyAttributeChildrenToRule(rule, *extensionElement);
                rule.annotation = declarationAnnotation;
                return rule;
            }

            if (childElement->LocalName() == "complexContent") {
                const bool mixedComplexContent = parseSchemaBooleanAttribute(*childElement, "mixed");
                const XmlElement* extensionElement = nullptr;
                const XmlElement* restrictionElement = nullptr;
                int extensionCount = 0;
                int restrictionCount = 0;
                for (const auto& contentChild : childElement->ChildNodes()) {
                    if (contentChild == nullptr || contentChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* contentChildElement = static_cast<const XmlElement*>(contentChild.get());
                    if (contentChildElement->NamespaceURI() == kSchemaNamespace
                        && contentChildElement->LocalName() == "extension") {
                        extensionElement = contentChildElement;
                        ++extensionCount;
                    }
                    if (contentChildElement->NamespaceURI() == kSchemaNamespace
                        && contentChildElement->LocalName() == "restriction") {
                        restrictionElement = contentChildElement;
                        ++restrictionCount;
                    }
                }
                if (extensionCount + restrictionCount != 1) {
                    throw XmlException("XML Schema complexContent must declare exactly one extension or restriction child");
                }

                if (restrictionElement != nullptr) {
                    const std::string baseType = restrictionElement->GetAttribute("base");
                    if (baseType.empty()) {
                        throw XmlException("XML Schema complexContent restriction requires a base type");
                    }

                    const auto [localTypeName, typeNamespaceUri] = resolveTypeName(*restrictionElement, baseType);
                    if (const auto builtinComplexType = resolveBuiltinComplexType(baseType); builtinComplexType.has_value()) {
                        rule = *builtinComplexType;
                        rule.isAbstract = declaredComplexTypeIsAbstract;
                    } else if (const auto* namedComplexType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                        ensureComplexTypeFinalAllowsDerivation(*namedComplexType, baseType, "complexContent restriction", true);
                        rule = *namedComplexType;
                        rule.isAbstract = declaredComplexTypeIsAbstract;
                    } else if (declaredComplexTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredComplexTypes.end()) {
                        rule = ensureComplexTypeResolved(localTypeName, typeNamespaceUri, baseType);
                        rule.isAbstract = declaredComplexTypeIsAbstract;
                        ensureComplexTypeFinalAllowsDerivation(rule, baseType, "complexContent restriction", true);
                    } else {
                        throw XmlException("XML Schema complexContent base '" + baseType + "' is not supported");
                    }

                    rule.namedTypeName.clear();
                    rule.namedTypeNamespaceUri.clear();
                    rule.derivationMethod = ComplexTypeRule::DerivationMethod::Restriction;
                    rule.derivationBaseName = localTypeName;
                    rule.derivationBaseNamespaceUri = typeNamespaceUri;
                    if (mixedComplexContent) {
                        rule.allowsText = true;
                    }

                    const std::optional<Particle> baseParticle = makeFlatParticleFromRule(rule);

                    for (const auto& restrictionChild : restrictionElement->ChildNodes()) {
                        if (restrictionChild == nullptr || restrictionChild->NodeType() != XmlNodeType::Element) {
                            continue;
                        }
                        const auto* restrictionChildElement = static_cast<const XmlElement*>(restrictionChild.get());
                        if (restrictionChildElement->NamespaceURI() != kSchemaNamespace) {
                            continue;
                        }
                        if (restrictionChildElement->LocalName() == "sequence"
                            || restrictionChildElement->LocalName() == "choice"
                            || restrictionChildElement->LocalName() == "group"
                            || restrictionChildElement->LocalName() == "all"
                            || restrictionChildElement->LocalName() == "any") {
                            const Particle derivedParticle = parseParticle(*restrictionChildElement);
                            if (!baseParticle.has_value()) {
                                throw XmlException("XML Schema complexContent restriction base '" + baseType + "' cannot introduce child particles when the base type has no element content");
                            }
                            validateDerivedParticleRestriction(*baseParticle, derivedParticle,
                                "complexContent restriction base '" + baseType + "'");
                            applyParticleToRule(rule, derivedParticle);
                            break;
                        } else if (restrictionChildElement->LocalName() != "annotation"
                            && restrictionChildElement->LocalName() != "attribute"
                            && restrictionChildElement->LocalName() != "attributeGroup"
                            && restrictionChildElement->LocalName() != "anyAttribute") {
                            throw XmlException("XML Schema complexContent restriction can only declare annotation, particle, or attribute children");
                        }
                    }

                    applyAttributeRestrictionsToRule(rule, *restrictionElement, false,
                        "complexContent restriction base '" + baseType + "'");
                    rule.annotation = declarationAnnotation;
                    return rule;
                }

                const std::string baseType = extensionElement->GetAttribute("base");
                if (baseType.empty()) {
                    throw XmlException("XML Schema complexContent extension requires a base type");
                }

                const auto [localTypeName, typeNamespaceUri] = resolveTypeName(*extensionElement, baseType);
                if (const auto builtinComplexType = resolveBuiltinComplexType(baseType); builtinComplexType.has_value()) {
                    rule = *builtinComplexType;
                    rule.isAbstract = declaredComplexTypeIsAbstract;
                } else if (const auto* namedComplexType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                    ensureComplexTypeFinalAllowsDerivation(*namedComplexType, baseType, "complexContent extension", false);
                    rule = *namedComplexType;
                    rule.isAbstract = declaredComplexTypeIsAbstract;
                } else if (declaredComplexTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredComplexTypes.end()) {
                    rule = ensureComplexTypeResolved(localTypeName, typeNamespaceUri, baseType);
                    rule.isAbstract = declaredComplexTypeIsAbstract;
                    ensureComplexTypeFinalAllowsDerivation(rule, baseType, "complexContent extension", false);
                } else {
                    throw XmlException("XML Schema complexContent base '" + baseType + "' is not supported");
                }

                rule.namedTypeName.clear();
                rule.namedTypeNamespaceUri.clear();
                rule.derivationMethod = ComplexTypeRule::DerivationMethod::Extension;
                rule.derivationBaseName = localTypeName;
                rule.derivationBaseNamespaceUri = typeNamespaceUri;
                if (mixedComplexContent) {
                    rule.allowsText = true;
                }

                std::optional<Particle> extensionParticle;
                for (const auto& extensionChild : extensionElement->ChildNodes()) {
                    if (extensionChild == nullptr || extensionChild->NodeType() != XmlNodeType::Element) {
                        continue;
                    }
                    const auto* extensionChildElement = static_cast<const XmlElement*>(extensionChild.get());
                    if (extensionChildElement->NamespaceURI() != kSchemaNamespace) {
                        continue;
                    }
                    if (extensionChildElement->LocalName() == "sequence"
                        || extensionChildElement->LocalName() == "choice"
                        || extensionChildElement->LocalName() == "group"
                        || extensionChildElement->LocalName() == "all"
                        || extensionChildElement->LocalName() == "any") {
                        extensionParticle = parseParticle(*extensionChildElement);
                        break;
                    } else if (extensionChildElement->LocalName() != "annotation"
                        && extensionChildElement->LocalName() != "attribute"
                        && extensionChildElement->LocalName() != "attributeGroup"
                        && extensionChildElement->LocalName() != "anyAttribute") {
                        throw XmlException("XML Schema complexContent extension can only declare annotation, particle, or attribute children");
                    }
                }

                mergeComplexContent(rule, extensionParticle);
                applyAttributeChildrenToRule(rule, *extensionElement);
                rule.annotation = declarationAnnotation;
                return rule;
            }

            if (childElement->LocalName() == "sequence"
                || childElement->LocalName() == "choice"
                || childElement->LocalName() == "group"
                || childElement->LocalName() == "all"
                || childElement->LocalName() == "any") {
                applyParticleToRule(rule, parseParticle(*childElement));
            } else if (childElement->LocalName() == "attribute"
                || childElement->LocalName() == "attributeGroup"
                || childElement->LocalName() == "anyAttribute") {
                applyAttributeChildrenToRule(rule, complexTypeElement);
                break;
            }
        }

        rule.annotation = declarationAnnotation;
        return rule;
    };

    simpleTypeDerivesFrom = [&](const SimpleTypeRule& derivedType,
        const SimpleTypeRule& baseType,
        bool& usesRestriction) {
        if (!derivedType.namedTypeName.empty()
            && derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return true;
        }
        if (derivedType.namedTypeName.empty()
            && baseType.namedTypeName.empty()
            && derivedType.derivationMethod == SimpleTypeRule::DerivationMethod::None
            && baseType.derivationMethod == SimpleTypeRule::DerivationMethod::None
            && derivedType.variety == baseType.variety) {
            bool builtinUsesRestriction = false;
            if (BuiltinSimpleTypeDerivesFrom(derivedType.baseType, baseType.baseType, builtinUsesRestriction)) {
                usesRestriction = usesRestriction || builtinUsesRestriction;
                return true;
            }
        }

        const SimpleTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == SimpleTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            }
            if (!baseType.namedTypeName.empty()
                && current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
            if (baseType.namedTypeName.empty()
                && current->derivationMethod == SimpleTypeRule::DerivationMethod::Restriction) {
                bool builtinUsesRestriction = false;
                if (BuiltinSimpleTypeDerivesFrom(current->baseType, baseType.baseType, builtinUsesRestriction)) {
                    usesRestriction = usesRestriction || builtinUsesRestriction;
                    return true;
                }
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = FindSimpleTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
            if (!baseType.namedTypeName.empty()
                && current->namedTypeName == baseType.namedTypeName
                && current->namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
        }
    };

    complexTypeDerivesFrom = [&](const ComplexTypeRule& derivedType,
        const ComplexTypeRule& baseType,
        bool& usesRestriction,
        bool& usesExtension) {
        if (baseType.namedTypeName == "anyType"
            && baseType.namedTypeNamespaceUri == "http://www.w3.org/2001/XMLSchema") {
            return true;
        }
        if (!derivedType.namedTypeName.empty()
            && derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return true;
        }

        const ComplexTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == ComplexTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            } else if (current->derivationMethod == ComplexTypeRule::DerivationMethod::Extension) {
                usesExtension = true;
            }
            if (current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = FindComplexTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
            if (!baseType.namedTypeName.empty()
                && current->namedTypeName == baseType.namedTypeName
                && current->namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
        }
    };

    const auto elementTypeCanJoinSubstitutionHead = [&](const ElementRule& memberRule,
        const ElementRule& headRule,
        bool& usesRestriction,
        bool& usesExtension) {
        if (headRule.declaredComplexType.has_value()) {
            if (!memberRule.declaredComplexType.has_value()) {
                return false;
            }
            return complexTypeDerivesFrom(*memberRule.declaredComplexType, *headRule.declaredComplexType, usesRestriction, usesExtension);
        }
        if (headRule.declaredSimpleType.has_value()) {
            if (!memberRule.declaredSimpleType.has_value()) {
                return false;
            }
            return simpleTypeDerivesFrom(*memberRule.declaredSimpleType, *headRule.declaredSimpleType, usesRestriction);
        }
        return true;
    };

    parseElementRule = [&](const XmlElement& schemaElement, bool isGlobalDeclaration) -> ElementRule {
        const std::string elementRef = schemaElement.GetAttribute("ref");
        if (!elementRef.empty()) {
            std::string_view nameView;
            std::string_view typeView;
            std::string_view defaultValueView;
            std::string_view fixedValueView;
            std::string_view nillableView;
            std::string_view blockView;
            std::string_view finalView;
            std::string_view formView;
                std::string_view abstractView;
                std::string_view substitutionGroupView;
            if (TryGetAttributeValueViewInternal(schemaElement, "name", nameView) && !nameView.empty()) {
                throw XmlException("XML Schema element references cannot specify a name");
            }
            if (TryGetAttributeValueViewInternal(schemaElement, "type", typeView) && !typeView.empty()) {
                throw XmlException("XML Schema element references cannot specify a type attribute");
            }
            if ((TryGetAttributeValueViewInternal(schemaElement, "default", defaultValueView) && !defaultValueView.empty())
                || (TryGetAttributeValueViewInternal(schemaElement, "fixed", fixedValueView) && !fixedValueView.empty())) {
                throw XmlException("XML Schema element references cannot specify default or fixed values");
            }
            if (TryGetAttributeValueViewInternal(schemaElement, "nillable", nillableView) && !nillableView.empty()) {
                throw XmlException("XML Schema element references cannot specify a nillable attribute");
            }
            if (TryGetAttributeValueViewInternal(schemaElement, "block", blockView) && !blockView.empty()) {
                throw XmlException("XML Schema element references cannot specify a block attribute");
            }
            if (TryGetAttributeValueViewInternal(schemaElement, "final", finalView) && !finalView.empty()) {
                throw XmlException("XML Schema element references cannot specify a final attribute");
            }
            if (TryGetAttributeValueViewInternal(schemaElement, "form", formView) && !formView.empty()) {
                throw XmlException("XML Schema element references cannot specify a form attribute");
            }
                if (TryGetAttributeValueViewInternal(schemaElement, "abstract", abstractView) && !abstractView.empty()) {
                    throw XmlException("XML Schema element references cannot specify an abstract attribute");
                }
                if (TryGetAttributeValueViewInternal(schemaElement, "substitutionGroup", substitutionGroupView) && !substitutionGroupView.empty()) {
                    throw XmlException("XML Schema element references cannot specify a substitutionGroup attribute");
                }
            for (const auto& child : schemaElement.ChildNodes()) {
                if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* childElement = static_cast<const XmlElement*>(child.get());
                if (childElement->NamespaceURI() != kSchemaNamespace) {
                    continue;
                }
                if (childElement->LocalName() == "simpleType" || childElement->LocalName() == "complexType") {
                    throw XmlException("XML Schema element references cannot declare an inline simpleType or complexType child");
                }
                if (childElement->LocalName() == "key"
                    || childElement->LocalName() == "unique"
                    || childElement->LocalName() == "keyref") {
                    throw XmlException("XML Schema element references cannot declare identity constraints");
                }
                if (childElement->LocalName() != "annotation") {
                    throw XmlException("XML Schema element references can only declare annotation children");
                }
            }
            const auto [localName, namespaceUri] = resolveTypeName(schemaElement, elementRef);
            return ensureElementResolved(localName, namespaceUri, elementRef);
        }

        ElementRule rule;
        rule.annotation = parseAnnotation(schemaElement);
        rule.name = schemaElement.GetAttribute("name");
        rule.namespaceUri = resolveElementDeclarationNamespace(schemaElement, isGlobalDeclaration);
        if (isGlobalDeclaration) {
            std::string_view minOccursView;
            std::string_view maxOccursView;
            if ((TryGetAttributeValueViewInternal(schemaElement, "minOccurs", minOccursView) && !minOccursView.empty())
                || (TryGetAttributeValueViewInternal(schemaElement, "maxOccurs", maxOccursView) && !maxOccursView.empty())) {
                throw XmlException("XML Schema global element declarations cannot specify minOccurs or maxOccurs");
            }
        }
        rule.isAbstract = parseSchemaBooleanAttribute(schemaElement, "abstract");
        rule.isNillable = parseSchemaBooleanAttribute(schemaElement, "nillable");
        const std::string defaultValue = schemaElement.GetAttribute("default");
        const std::string fixedValue = schemaElement.GetAttribute("fixed");
        if (!defaultValue.empty() && !fixedValue.empty()) {
            throw XmlException("XML Schema element declarations cannot specify both default and fixed values");
        }
        if (!defaultValue.empty()) {
            rule.defaultValue = defaultValue;
        }
        if (!fixedValue.empty()) {
            rule.fixedValue = fixedValue;
        }
        const std::string effectiveBlock = effectiveDerivationControlValue(schemaElement.GetAttribute("block"), schemaBlockDefault);
        const std::string effectiveFinal = effectiveDerivationControlValue(schemaElement.GetAttribute("final"), schemaFinalDefault);
        rule.blockSubstitution = containsDerivationToken(effectiveBlock, "substitution");
        rule.blockRestriction = containsDerivationToken(effectiveBlock, "restriction");
        rule.blockExtension = containsDerivationToken(effectiveBlock, "extension");
        rule.finalSubstitution = containsDerivationToken(effectiveFinal, "substitution");
        rule.finalRestriction = containsDerivationToken(effectiveFinal, "restriction");
        rule.finalExtension = containsDerivationToken(effectiveFinal, "extension");
        rule.allowsText = true;
        rule.contentModel = ContentModel::Empty;
        if (rule.name.empty()) {
            throw XmlException("XML Schema element declarations require a name");
        }

        const auto validateDeclaredDefaultOrFixedForElementRule = [&](const ElementRule& candidateRule) {
            if ((candidateRule.defaultValue.has_value() || candidateRule.fixedValue.has_value())
                && (!candidateRule.allowsText || candidateRule.particle.has_value() || !candidateRule.children.empty())) {
                throw XmlException("XML Schema element default/fixed values are only supported for simple-content elements");
            }

            validateDeclaredDefaultOrFixedValue(
                candidateRule.textType,
                candidateRule.defaultValue,
                "default",
                "element",
                candidateRule.name,
                schemaElement);
            validateDeclaredDefaultOrFixedValue(
                candidateRule.textType,
                candidateRule.fixedValue,
                "fixed",
                "element",
                candidateRule.name,
                schemaElement);
        };

        const std::string substitutionGroup = schemaElement.GetAttribute("substitutionGroup");
        std::string substitutionGroupHeadName;
        std::string substitutionGroupHeadNamespaceUri;
        if (rule.isAbstract && !substitutionGroup.empty()) {
            throw XmlException("XML Schema element declarations cannot specify both abstract and substitutionGroup attributes");
        }
        if (!substitutionGroup.empty()) {
            const auto resolvedHead = resolveTypeName(schemaElement, substitutionGroup);
            substitutionGroupHeadName = resolvedHead.first;
            substitutionGroupHeadNamespaceUri = resolvedHead.second;
        }

        const std::string typeAttribute = schemaElement.GetAttribute("type");
        if (!typeAttribute.empty()) {
            if (const auto builtinType = resolveBuiltinSimpleType(typeAttribute); builtinType.has_value()) {
                rule.textType = builtinType;
                rule.declaredSimpleType = *builtinType;
            } else if (const auto builtinComplexType = resolveBuiltinComplexType(typeAttribute); builtinComplexType.has_value()) {
                applyComplexType(rule, *builtinComplexType);
                rule.declaredComplexType = *builtinComplexType;
            } else {
                const auto [localTypeName, typeNamespaceUri] = resolveTypeName(schemaElement, typeAttribute);
                if (const auto* namedSimpleType = FindSimpleTypeRule(localTypeName, typeNamespaceUri)) {
                    rule.textType = *namedSimpleType;
                    rule.declaredSimpleType = *namedSimpleType;
                } else if (declaredSimpleTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredSimpleTypes.end()) {
                    rule.textType = ensureSimpleTypeResolved(localTypeName, typeNamespaceUri, typeAttribute);
                    rule.declaredSimpleType = *rule.textType;
                } else if (const auto* namedComplexType = FindComplexTypeRule(localTypeName, typeNamespaceUri)) {
                    applyComplexType(rule, *namedComplexType);
                    rule.declaredComplexType = *namedComplexType;
                } else if (declaredComplexTypes.find(makeQualifiedRuleKey(localTypeName, typeNamespaceUri)) != declaredComplexTypes.end()) {
                    const ComplexTypeRule resolvedType = ensureComplexTypeResolved(localTypeName, typeNamespaceUri, typeAttribute);
                    applyComplexType(rule, resolvedType);
                    rule.declaredComplexType = resolvedType;
                } else {
                    throw XmlException("XML Schema element type '" + typeAttribute + "' is not supported");
                }
            }
        }

        const XmlElement* complexType = nullptr;
        const XmlElement* simpleType = nullptr;
        std::size_t inlineComplexTypeCount = 0;
        std::size_t inlineSimpleTypeCount = 0;
        for (const auto& child : schemaElement.ChildNodes()) {
            if (child != nullptr && child->NodeType() == XmlNodeType::Element) {
                const auto* childElement = static_cast<const XmlElement*>(child.get());
                if (childElement->LocalName() == "complexType" && childElement->NamespaceURI() == kSchemaNamespace) {
                    ++inlineComplexTypeCount;
                    complexType = childElement;
                } else if (childElement->LocalName() == "simpleType" && childElement->NamespaceURI() == kSchemaNamespace) {
                    ++inlineSimpleTypeCount;
                    simpleType = childElement;
                } else if (childElement->NamespaceURI() == kSchemaNamespace
                    && childElement->LocalName() != "annotation"
                    && childElement->LocalName() != "key"
                    && childElement->LocalName() != "unique"
                    && childElement->LocalName() != "keyref") {
                    throw XmlException("XML Schema element declarations can only declare annotation, inline type, or identity constraint children");
                }
            }
        }

        const std::size_t inlineTypeChildCount = inlineComplexTypeCount + inlineSimpleTypeCount;
        if (inlineTypeChildCount > 1) {
            throw XmlException("XML Schema element declarations can only declare one inline simpleType or complexType child");
        }
        if (!typeAttribute.empty() && inlineTypeChildCount > 0) {
            throw XmlException("XML Schema element declarations cannot combine a type attribute with an inline simpleType or complexType child");
        }

        if (simpleType != nullptr) {
            rule.textType = parseSimpleType(*simpleType);
            rule.declaredSimpleType = *rule.textType;
            rule.allowsText = true;
            for (const auto& child : schemaElement.ChildNodes()) {
                if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* childElement = static_cast<const XmlElement*>(child.get());
                if (childElement->NamespaceURI() != kSchemaNamespace) {
                    continue;
                }
                if (childElement->LocalName() == "key"
                    || childElement->LocalName() == "unique"
                    || childElement->LocalName() == "keyref") {
                    rule.identityConstraints.push_back(parseIdentityConstraint(*childElement));
                }
            }
            if (!substitutionGroup.empty()) {
                const ElementRule headRule = ensureElementResolved(substitutionGroupHeadName, substitutionGroupHeadNamespaceUri, substitutionGroup);
                if (headRule.finalSubstitution) {
                    throw XmlException("XML Schema element '" + rule.name + "' cannot join substitutionGroup '" + substitutionGroup + "' because the head blocks substitution derivation");
                }
                bool usesRestriction = false;
                bool usesExtension = false;
                if (!elementTypeCanJoinSubstitutionHead(rule, headRule, usesRestriction, usesExtension)) {
                    throw XmlException("XML Schema element '" + rule.name + "' must use the same type as, or a type derived from, substitutionGroup head '" + substitutionGroup + "'");
                }
                if ((usesRestriction && headRule.finalRestriction) || (usesExtension && headRule.finalExtension)) {
                    throw XmlException("XML Schema element '" + rule.name + "' cannot join substitutionGroup '" + substitutionGroup + "' because the head blocks the required type derivation");
                }
                rule.substitutionGroupHeadName = substitutionGroupHeadName;
                rule.substitutionGroupHeadNamespaceUri = substitutionGroupHeadNamespaceUri;
            }
            validateDeclaredDefaultOrFixedForElementRule(rule);
            return rule;
        }

        if (complexType == nullptr) {
            for (const auto& child : schemaElement.ChildNodes()) {
                if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* childElement = static_cast<const XmlElement*>(child.get());
                if (childElement->NamespaceURI() != kSchemaNamespace) {
                    continue;
                }
                if (childElement->LocalName() == "key"
                    || childElement->LocalName() == "unique"
                    || childElement->LocalName() == "keyref") {
                    rule.identityConstraints.push_back(parseIdentityConstraint(*childElement));
                }
            }
            if (!substitutionGroup.empty()) {
                const ElementRule headRule = ensureElementResolved(substitutionGroupHeadName, substitutionGroupHeadNamespaceUri, substitutionGroup);
                if (headRule.finalSubstitution) {
                    throw XmlException("XML Schema element '" + rule.name + "' cannot join substitutionGroup '" + substitutionGroup + "' because the head blocks substitution derivation");
                }
                bool usesRestriction = false;
                bool usesExtension = false;
                if (!elementTypeCanJoinSubstitutionHead(rule, headRule, usesRestriction, usesExtension)) {
                    throw XmlException("XML Schema element '" + rule.name + "' must use the same type as, or a type derived from, substitutionGroup head '" + substitutionGroup + "'");
                }
                if ((usesRestriction && headRule.finalRestriction) || (usesExtension && headRule.finalExtension)) {
                    throw XmlException("XML Schema element '" + rule.name + "' cannot join substitutionGroup '" + substitutionGroup + "' because the head blocks the required type derivation");
                }
                rule.substitutionGroupHeadName = substitutionGroupHeadName;
                rule.substitutionGroupHeadNamespaceUri = substitutionGroupHeadNamespaceUri;
            }
            validateDeclaredDefaultOrFixedForElementRule(rule);
            return rule;
        }

        const ComplexTypeRule inlineComplexType = parseComplexType(*complexType);
        applyComplexType(rule, inlineComplexType);
        rule.declaredComplexType = inlineComplexType;

        for (const auto& child : schemaElement.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }
            if (childElement->LocalName() == "key"
                || childElement->LocalName() == "unique"
                || childElement->LocalName() == "keyref") {
                rule.identityConstraints.push_back(parseIdentityConstraint(*childElement));
            }
        }

        if (!substitutionGroup.empty()) {
            const ElementRule headRule = ensureElementResolved(substitutionGroupHeadName, substitutionGroupHeadNamespaceUri, substitutionGroup);
            if (headRule.finalSubstitution) {
                throw XmlException("XML Schema element '" + rule.name + "' cannot join substitutionGroup '" + substitutionGroup + "' because the head blocks substitution derivation");
            }
            bool usesRestriction = false;
            bool usesExtension = false;
            if (!elementTypeCanJoinSubstitutionHead(rule, headRule, usesRestriction, usesExtension)) {
                throw XmlException("XML Schema element '" + rule.name + "' must use the same type as, or a type derived from, substitutionGroup head '" + substitutionGroup + "'");
            }
            if ((usesRestriction && headRule.finalRestriction) || (usesExtension && headRule.finalExtension)) {
                throw XmlException("XML Schema element '" + rule.name + "' cannot join substitutionGroup '" + substitutionGroup + "' because the head blocks the required type derivation");
            }
            rule.substitutionGroupHeadName = substitutionGroupHeadName;
            rule.substitutionGroupHeadNamespaceUri = substitutionGroupHeadNamespaceUri;
        }

        validateDeclaredDefaultOrFixedForElementRule(rule);

        return rule;
    };

    for (const auto& [ruleKey, declaration] : declaredSimpleTypes) {
        (void)ruleKey;
        const std::string name = declaration->GetAttribute("name");
        (void)ensureSimpleTypeResolved(name, targetNamespace, name);
    }

    for (const auto& [ruleKey, declaration] : declaredComplexTypes) {
        (void)ruleKey;
        const std::string name = declaration->GetAttribute("name");
        (void)ensureComplexTypeResolved(name, targetNamespace, name);
    }

    for (const auto& [ruleKey, declaration] : declaredGroups) {
        (void)ruleKey;
        const std::string name = declaration->GetAttribute("name");
        upsertGroupAnnotation(NamedGroupAnnotation{name, targetNamespace, parseAnnotation(*declaration)});
        (void)ensureGroupResolved(name, targetNamespace, name);
    }

    for (const auto& [ruleKey, declaration] : declaredAttributes) {
        (void)ruleKey;
        const std::string name = declaration->GetAttribute("name");
        (void)ensureAttributeResolved(name, targetNamespace, name);
    }

    for (const auto& [ruleKey, declaration] : declaredAttributeGroups) {
        (void)ruleKey;
        const std::string name = declaration->GetAttribute("name");
        upsertAttributeGroupAnnotation(NamedAttributeGroupAnnotation{name, targetNamespace, parseAnnotation(*declaration)});
        (void)ensureAttributeGroupResolved(name, targetNamespace, name);
    }

    for (const auto& [ruleKey, declaration] : declaredElements) {
        (void)ruleKey;
        const std::string name = declaration->GetAttribute("name");
        (void)ensureElementResolved(name, targetNamespace, name);
    }

    for (const auto& child : root->ChildNodes()) {
        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
            continue;
        }
        const auto* childElement = static_cast<const XmlElement*>(child.get());
        if (childElement->LocalName() != "element" || childElement->NamespaceURI() != kSchemaNamespace) {
            continue;
        }
        const std::string name = childElement->GetAttribute("name");
        if (!name.empty()) {
            (void)ensureElementResolved(name, targetNamespace, name);
        }
    }

    for (const auto& constraint : identityConstraints_) {
        if (constraint.kind != ElementRule::IdentityConstraint::Kind::KeyRef) {
            continue;
        }
        const auto* referencedConstraint = FindIdentityConstraint(constraint.referName, constraint.referNamespaceUri);
        if (referencedConstraint == nullptr) {
            throw XmlException("XML Schema keyref '" + constraint.name + "' refers to an unknown identity constraint");
        }
        if (referencedConstraint->kind == ElementRule::IdentityConstraint::Kind::KeyRef) {
            throw XmlException("XML Schema keyref '" + constraint.name + "' must refer to a key or unique constraint");
        }
        if (referencedConstraint->fieldXPaths.size() != constraint.fieldXPaths.size()) {
            throw XmlException("XML Schema keyref '" + constraint.name + "' must declare the same number of fields as its referenced key/unique constraint");
        }
    }
}

void XmlSchemaSet::AddFile(const std::string& path) {
    static const std::string kSchemaNamespace = "http://www.w3.org/2001/XMLSchema";

    const std::filesystem::path schemaPath = NormalizeSchemaPath(std::filesystem::path(path));
    const std::string normalizedPath = schemaPath.generic_string();
    if (loadedSchemaFiles_.find(normalizedPath) != loadedSchemaFiles_.end()) {
        return;
    }
    if (!activeSchemaFiles_.insert(normalizedPath).second) {
        throw XmlException("Circular XML Schema include/import/override/redefine detected for file: " + normalizedPath);
    }

    try {
        std::ifstream stream(schemaPath, std::ios::binary);
        if (!stream) {
            throw XmlException("Failed to open XML schema file: " + normalizedPath);
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const std::string xml = buffer.str();

        const auto document = XmlDocument::Parse(xml);
        const auto root = document->DocumentElement();
        if (root == nullptr || root->LocalName() != "schema" || root->NamespaceURI() != kSchemaNamespace) {
            throw XmlException("XmlSchemaSet::AddFile requires an XML Schema document");
        }

        const std::string targetNamespace = root->GetAttribute("targetNamespace");
        const auto loadReferencedSchema = [&](const XmlElement& referenceElement, const char* referenceKind) {
            const std::string referenceKindString(referenceKind);
            const std::string schemaLocation = referenceElement.GetAttribute("schemaLocation");
            if (schemaLocation.empty()) {
                if (referenceKindString == "import") {
                    const std::string importedNamespace = referenceElement.GetAttribute("namespace");
                    const auto loadedNamespace = std::find_if(schemaAnnotations_.begin(), schemaAnnotations_.end(), [&](const auto& schemaAnnotation) {
                        return schemaAnnotation.namespaceUri == importedNamespace;
                    });
                    if (loadedNamespace == schemaAnnotations_.end()) {
                        throw XmlException("XML Schema import without schemaLocation requires the namespace to already be loaded into the XmlSchemaSet");
                    }
                    return;
                }
                throw XmlException(std::string("XML Schema ") + referenceKind + " requires a schemaLocation when loaded via AddFile");
            }

            const std::filesystem::path referencedPath = NormalizeSchemaPath(schemaPath.parent_path() / std::filesystem::path(schemaLocation));
            std::ifstream referencedStream(referencedPath, std::ios::binary);
            if (!referencedStream) {
                throw XmlException("Failed to open referenced XML schema file: " + referencedPath.generic_string());
            }

            std::ostringstream referencedBuffer;
            referencedBuffer << referencedStream.rdbuf();
            const auto referencedDocument = XmlDocument::Parse(referencedBuffer.str());
            const auto referencedRoot = referencedDocument->DocumentElement();
            if (referencedRoot == nullptr || referencedRoot->LocalName() != "schema" || referencedRoot->NamespaceURI() != kSchemaNamespace) {
                throw XmlException("Referenced XML schema file is not a valid XML Schema document: " + referencedPath.generic_string());
            }

            const std::string referencedTargetNamespace = referencedRoot->GetAttribute("targetNamespace");
            if (referenceKindString == "include" || referenceKindString == "override" || referenceKindString == "redefine") {
                if (referencedTargetNamespace != targetNamespace) {
                    throw XmlException(
                        std::string("XML Schema ") + referenceKind + " requires the referenced schema to use the same targetNamespace");
                }
            } else {
                const std::string importedNamespace = referenceElement.GetAttribute("namespace");
                if (!importedNamespace.empty()) {
                    if (referencedTargetNamespace != importedNamespace) {
                        throw XmlException(
                            "XML Schema import requires the referenced schema targetNamespace to match the declared namespace");
                    }
                } else if (!referencedTargetNamespace.empty()) {
                    throw XmlException(
                        "XML Schema import without a namespace can only reference a schema with no targetNamespace");
                }
            }

            AddFile(referencedPath.string());
        };

        for (const auto& child : root->ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            if (childElement->NamespaceURI() != kSchemaNamespace) {
                continue;
            }

            if (childElement->LocalName() == "include") {
                loadReferencedSchema(*childElement, "include");
            } else if (childElement->LocalName() == "import") {
                loadReferencedSchema(*childElement, "import");
            } else if (childElement->LocalName() == "override") {
                loadReferencedSchema(*childElement, "override");
            } else if (childElement->LocalName() == "redefine") {
                loadReferencedSchema(*childElement, "redefine");
            }
        }

        AddXml(BuildSchemaAddXmlPayload(*root, schemaPath, kSchemaNamespace));
        loadedSchemaFiles_.insert(normalizedPath);
        activeSchemaFiles_.erase(normalizedPath);
    } catch (...) {
        activeSchemaFiles_.erase(normalizedPath);
        throw;
    }
}

std::size_t XmlSchemaSet::Count() const noexcept {
    return elements_.size();
}

bool XmlSchemaSet::HasIdentityConstraints() const noexcept {
    return !identityConstraints_.empty();
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindSchemaAnnotation(std::string_view namespaceUri) const {
    return FindStoredSchemaAnnotation(namespaceUri);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindElementAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto* rule = FindElementRule(localName, namespaceUri);
    return rule == nullptr ? nullptr : std::addressof(rule->annotation);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindSimpleTypeAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto* rule = FindSimpleTypeRule(localName, namespaceUri);
    return rule == nullptr ? nullptr : std::addressof(rule->annotation);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindComplexTypeAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto* rule = FindComplexTypeRule(localName, namespaceUri);
    return rule == nullptr ? nullptr : std::addressof(rule->annotation);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindAttributeAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto* rule = FindAttributeRule(localName, namespaceUri);
    return rule == nullptr ? nullptr : std::addressof(rule->annotation);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    return FindStoredGroupAnnotation(localName, namespaceUri);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindAttributeGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    return FindStoredAttributeGroupAnnotation(localName, namespaceUri);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindIdentityConstraintAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto* constraint = FindIdentityConstraint(localName, namespaceUri);
    return constraint == nullptr ? nullptr : std::addressof(constraint->annotation);
}

const XmlSchemaSet::ElementRule* XmlSchemaSet::FindElementRule(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(elements_.begin(), elements_.end(), [&](const auto& rule) {
        return rule.name == localName && rule.namespaceUri == namespaceUri;
    });
    return found == elements_.end() ? nullptr : std::addressof(*found);
}

const XmlSchemaSet::SimpleTypeRule* XmlSchemaSet::FindSimpleTypeRule(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(simpleTypes_.begin(), simpleTypes_.end(), [&](const auto& rule) {
        return rule.name == localName && rule.namespaceUri == namespaceUri;
    });
    return found == simpleTypes_.end() ? nullptr : std::addressof(found->rule);
}

const XmlSchemaSet::ComplexTypeRule* XmlSchemaSet::FindComplexTypeRule(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(complexTypes_.begin(), complexTypes_.end(), [&](const auto& rule) {
        return rule.name == localName && rule.namespaceUri == namespaceUri;
    });
    return found == complexTypes_.end() ? nullptr : std::addressof(found->rule);
}

const XmlSchemaSet::AttributeUse* XmlSchemaSet::FindAttributeRule(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(attributes_.begin(), attributes_.end(), [&](const auto& rule) {
        return rule.name == localName && rule.namespaceUri == namespaceUri;
    });
    return found == attributes_.end() ? nullptr : std::addressof(found->rule);
}

const XmlSchemaSet::Particle* XmlSchemaSet::FindGroupRule(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(groupRules_.begin(), groupRules_.end(), [&](const auto& rule) {
        return rule.name == localName && rule.namespaceUri == namespaceUri;
    });
    return found == groupRules_.end() ? nullptr : std::addressof(found->rule);
}

const XmlSchemaSet::AttributeGroupRule* XmlSchemaSet::FindAttributeGroupRule(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(attributeGroupRules_.begin(), attributeGroupRules_.end(), [&](const auto& rule) {
        return rule.name == localName && rule.namespaceUri == namespaceUri;
    });
    return found == attributeGroupRules_.end() ? nullptr : std::addressof(found->rule);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindStoredSchemaAnnotation(std::string_view namespaceUri) const {
    const auto found = std::find_if(schemaAnnotations_.begin(), schemaAnnotations_.end(), [&](const auto& annotation) {
        return annotation.namespaceUri == namespaceUri;
    });
    return found == schemaAnnotations_.end() ? nullptr : std::addressof(found->annotation);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindStoredGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(groups_.begin(), groups_.end(), [&](const auto& annotation) {
        return annotation.name == localName && annotation.namespaceUri == namespaceUri;
    });
    return found == groups_.end() ? nullptr : std::addressof(found->annotation);
}

const XmlSchemaSet::Annotation* XmlSchemaSet::FindStoredAttributeGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const {
    const auto found = std::find_if(attributeGroups_.begin(), attributeGroups_.end(), [&](const auto& annotation) {
        return annotation.name == localName && annotation.namespaceUri == namespaceUri;
    });
    return found == attributeGroups_.end() ? nullptr : std::addressof(found->annotation);
}

const XmlSchemaSet::ElementRule::IdentityConstraint* XmlSchemaSet::FindIdentityConstraint(
    std::string_view localName,
    std::string_view namespaceUri) const {
    const auto found = std::find_if(identityConstraints_.begin(), identityConstraints_.end(), [&](const auto& constraint) {
        return constraint.name == localName && constraint.namespaceUri == namespaceUri;
    });
    return found == identityConstraints_.end() ? nullptr : std::addressof(*found);
}


}  // namespace System::Xml
