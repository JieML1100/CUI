#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace System::Xml {

class XmlElement;

class XmlSchemaSet final {
public:
    struct AnnotationEntry {
        std::string Source;
        std::string Language;
        std::string Content;
    };

    struct Annotation {
        std::vector<AnnotationEntry> AppInfo;
        std::vector<AnnotationEntry> Documentation;

        bool Empty() const noexcept {
            return AppInfo.empty() && Documentation.empty();
        }
    };

    XmlSchemaSet() = default;

    void AddXml(std::string_view xml);
    void AddFile(const std::string& path);
    std::size_t Count() const noexcept;
    bool HasIdentityConstraints() const noexcept;
    const Annotation* FindSchemaAnnotation(std::string_view namespaceUri) const;
    const Annotation* FindElementAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindSimpleTypeAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindComplexTypeAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindAttributeAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindAttributeGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindIdentityConstraintAnnotation(std::string_view localName, std::string_view namespaceUri) const;

private:
    enum class ContentModel {
        Empty,
        Sequence,
        Choice,
    };

    struct SimpleTypeRule {
        enum class Variety {
            Atomic,
            List,
            Union,
        };

        enum class DerivationMethod {
            None,
            Restriction,
            List,
            Union,
        };

        Variety variety = Variety::Atomic;
        std::string baseType = "xs:string";
        std::string namedTypeName;
        std::string namedTypeNamespaceUri;
        DerivationMethod derivationMethod = DerivationMethod::None;
        std::string derivationBaseName;
        std::string derivationBaseNamespaceUri;
        std::shared_ptr<SimpleTypeRule> itemType;
        std::vector<SimpleTypeRule> memberTypes;
        std::vector<std::string> enumerationValues;
        std::optional<std::string> whiteSpace;
        std::optional<std::string> pattern;
        std::optional<std::size_t> length;
        std::optional<std::size_t> minLength;
        std::optional<std::size_t> maxLength;
        std::optional<std::string> minInclusive;
        std::optional<std::string> maxInclusive;
        std::optional<std::string> minExclusive;
        std::optional<std::string> maxExclusive;
        std::optional<std::size_t> totalDigits;
        std::optional<std::size_t> fractionDigits;
        Annotation annotation;
        bool finalRestriction = false;
        bool finalList = false;
        bool finalUnion = false;
    };

    struct AttributeUse {
        std::string name;
        std::string namespaceUri;
        bool required = false;
        std::optional<SimpleTypeRule> type;
        std::optional<std::string> defaultValue;
        std::optional<std::string> fixedValue;
        bool isAny = false;
        std::string namespaceConstraint = "##any";
        std::string processContents = "strict";
        Annotation annotation;
    };

    struct ComplexTypeRule;

    struct ChildUse {
        std::string name;
        std::string namespaceUri;
        std::optional<SimpleTypeRule> declaredSimpleType;
        std::shared_ptr<ComplexTypeRule> declaredComplexType;
        bool isNillable = false;
        std::optional<std::string> defaultValue;
        std::optional<std::string> fixedValue;
        bool blockRestriction = false;
        bool blockExtension = false;
        bool finalRestriction = false;
        bool finalExtension = false;
        std::size_t minOccurs = 1;
        std::size_t maxOccurs = 1;
    };

    struct Particle {
        enum class Kind {
            Element,
            Any,
            Sequence,
            Choice,
            All,
        };

        Kind kind = Kind::Element;
        std::string name;
        std::string namespaceUri;
        std::optional<SimpleTypeRule> elementSimpleType;
        std::shared_ptr<ComplexTypeRule> elementComplexType;
        bool elementIsNillable = false;
        std::optional<std::string> elementDefaultValue;
        std::optional<std::string> elementFixedValue;
        bool elementBlockRestriction = false;
        bool elementBlockExtension = false;
        bool elementFinalRestriction = false;
        bool elementFinalExtension = false;
        std::string processContents = "strict";
        std::size_t minOccurs = 1;
        std::size_t maxOccurs = 1;
        std::vector<Particle> children;
    };

    struct ComplexTypeRule {
        enum class DerivationMethod {
            None,
            Restriction,
            Extension,
        };

        std::string namedTypeName;
        std::string namedTypeNamespaceUri;
        DerivationMethod derivationMethod = DerivationMethod::None;
        std::string derivationBaseName;
        std::string derivationBaseNamespaceUri;
        bool allowsText = false;
        std::optional<SimpleTypeRule> textType;
        ContentModel contentModel = ContentModel::Empty;
        std::vector<AttributeUse> attributes;
        std::vector<ChildUse> children;
        std::optional<Particle> particle;
        bool anyAttributeAllowed = false;
        std::string anyAttributeNamespaceConstraint = "##any";
        std::string anyAttributeProcessContents = "strict";
        bool blockRestriction = false;
        bool blockExtension = false;
        bool finalRestriction = false;
        bool finalExtension = false;
        bool isAbstract = false;
        Annotation annotation;
    };

    struct ElementRule {
        struct IdentityConstraint {
            enum class Kind {
                Key,
                Unique,
                KeyRef,
            };

            struct CompiledPathStep {
                enum class Kind {
                    Self,
                    Parent,
                    Ancestor,
                    AncestorOrSelf,
                    FollowingSibling,
                    PrecedingSibling,
                    Following,
                    Preceding,
                    Element,
                    DescendantElement,
                    DescendantOrSelfElement,
                    Attribute,
                };

                Kind kind = Kind::Element;
                std::string localName;
                std::string namespaceUri;
                std::string predicateAttributeLocalName;
                std::string predicateAttributeNamespaceUri;
                std::optional<std::string> predicateAttributeValue;
            };

            struct CompiledPath {
                std::vector<CompiledPathStep> steps;
            };

            Kind kind = Kind::Key;
            std::string name;
            std::string namespaceUri;
            std::string selectorXPath;
            std::optional<CompiledPath> compiledSelectorPath;
            std::vector<std::string> fieldXPaths;
            std::vector<std::optional<CompiledPath>> compiledFieldPaths;
            std::vector<std::pair<std::string, std::string>> namespaceBindings;
            std::string referName;
            std::string referNamespaceUri;
            Annotation annotation;
        };

        std::string name;
        std::string namespaceUri;
        bool isAbstract = false;
        bool isNillable = false;
        std::optional<std::string> defaultValue;
        std::optional<std::string> fixedValue;
        bool blockSubstitution = false;
        bool blockRestriction = false;
        bool blockExtension = false;
        bool finalSubstitution = false;
        bool finalRestriction = false;
        bool finalExtension = false;
        std::string substitutionGroupHeadName;
        std::string substitutionGroupHeadNamespaceUri;
        std::optional<SimpleTypeRule> declaredSimpleType;
        std::optional<ComplexTypeRule> declaredComplexType;
        bool allowsText = true;
        std::optional<SimpleTypeRule> textType;
        ContentModel contentModel = ContentModel::Empty;
        std::vector<AttributeUse> attributes;
        std::vector<ChildUse> children;
        std::optional<Particle> particle;
        std::vector<IdentityConstraint> identityConstraints;
        bool anyAttributeAllowed = false;
        std::string anyAttributeNamespaceConstraint = "##any";
        std::string anyAttributeProcessContents = "strict";
        Annotation annotation;
    };

    struct NamedSimpleTypeRule {
        std::string name;
        std::string namespaceUri;
        SimpleTypeRule rule;
    };

    struct NamedComplexTypeRule {
        std::string name;
        std::string namespaceUri;
        ComplexTypeRule rule;
    };

    struct NamedAttributeRule {
        std::string name;
        std::string namespaceUri;
        AttributeUse rule;
    };

    struct NamedGroupRule {
        std::string name;
        std::string namespaceUri;
        Particle rule;
    };

    struct AttributeGroupRule {
        std::vector<AttributeUse> attributes;
        bool anyAttributeAllowed = false;
        std::string anyAttributeNamespaceConstraint = "##any";
        std::string anyAttributeProcessContents = "strict";
    };

    struct NamedAttributeGroupRule {
        std::string name;
        std::string namespaceUri;
        AttributeGroupRule rule;
    };

    struct NamedSchemaAnnotation {
        std::string namespaceUri;
        Annotation annotation;
    };

    struct NamedGroupAnnotation {
        std::string name;
        std::string namespaceUri;
        Annotation annotation;
    };

    struct NamedAttributeGroupAnnotation {
        std::string name;
        std::string namespaceUri;
        Annotation annotation;
    };

    friend std::string NormalizeSchemaSimpleTypeValue(const SimpleTypeRule& type, std::string_view lexicalValue);
    friend void ValidateSchemaSimpleValueWithQNameResolver(
        const SimpleTypeRule& type,
        std::string_view value,
        std::string_view label,
        const std::function<std::string(const std::string&)>& resolveNamespaceUri,
        const std::function<bool(const std::string&)>& hasNotationDeclaration,
        const std::function<bool(const std::string&)>& hasUnparsedEntityDeclaration);
    friend void ValidateSchemaSimpleValue(
        const SimpleTypeRule& type,
        std::string_view value,
        std::string_view label,
        const XmlElement* contextElement);
    friend void ValidateSchemaSimpleValue(
        const SimpleTypeRule& type,
        std::string_view value,
        std::string_view label,
        const std::vector<std::pair<std::string, std::string>>& namespaceBindings,
        const std::unordered_set<std::string>& notationDeclarationNames,
        const std::unordered_set<std::string>& unparsedEntityDeclarationNames);

    std::vector<ElementRule> elements_;
    std::vector<NamedSimpleTypeRule> simpleTypes_;
    std::vector<NamedComplexTypeRule> complexTypes_;
    std::vector<NamedAttributeRule> attributes_;
    std::vector<NamedGroupRule> groupRules_;
    std::vector<NamedAttributeGroupRule> attributeGroupRules_;
    std::vector<NamedSchemaAnnotation> schemaAnnotations_;
    std::vector<NamedGroupAnnotation> groups_;
    std::vector<NamedAttributeGroupAnnotation> attributeGroups_;
    std::vector<ElementRule::IdentityConstraint> identityConstraints_;
    std::unordered_set<std::string> loadedSchemaFiles_;
    std::unordered_set<std::string> activeSchemaFiles_;

    const ElementRule* FindElementRule(std::string_view localName, std::string_view namespaceUri) const;
    const SimpleTypeRule* FindSimpleTypeRule(std::string_view localName, std::string_view namespaceUri) const;
    const ComplexTypeRule* FindComplexTypeRule(std::string_view localName, std::string_view namespaceUri) const;
    const AttributeUse* FindAttributeRule(std::string_view localName, std::string_view namespaceUri) const;
    const Particle* FindGroupRule(std::string_view localName, std::string_view namespaceUri) const;
    const AttributeGroupRule* FindAttributeGroupRule(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindStoredSchemaAnnotation(std::string_view namespaceUri) const;
    const Annotation* FindStoredGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const Annotation* FindStoredAttributeGroupAnnotation(std::string_view localName, std::string_view namespaceUri) const;
    const ElementRule::IdentityConstraint* FindIdentityConstraint(std::string_view localName, std::string_view namespaceUri) const;

    friend class XmlDocument;
};

}  // namespace System::Xml
