#include "../include/XmlInternal.h"

namespace System::Xml {

struct XPathName {
    bool wildcard = false;
    std::string name;
    std::string prefix;
    std::string localName;
    bool attribute = false;
};

struct XPathStep {
    enum class Axis {
        Child,
        Descendant,
        DescendantOrSelf,
        Parent,
        Ancestor,
        AncestorOrSelf,
        FollowingSibling,
        PrecedingSibling,
        Following,
        Preceding,
        Self,
        Attribute,
        Namespace,
    };

    Axis axis = Axis::Child;
    bool descendant = false;
    bool self = false;
    bool attribute = false;
    bool textNode = false;
    bool commentNode = false;
    bool processingInstructionNode = false;
    bool nodeTest = false;
    std::string processingInstructionTarget;
    XPathName name;
    struct PredicatePathSegment {
        enum class Kind {
            Self,
            Parent,
            SelfElement,
            ParentElement,
            SelfText,
            SelfComment,
            SelfProcessingInstruction,
            DescendantNode,
            DescendantElement,
            DescendantText,
            DescendantComment,
            DescendantProcessingInstruction,
            DescendantOrSelfNode,
            DescendantOrSelfElement,
            DescendantOrSelfText,
            DescendantOrSelfComment,
            DescendantOrSelfProcessingInstruction,
            AncestorNode,
            AncestorElement,
            AncestorText,
            AncestorComment,
            AncestorProcessingInstruction,
            AncestorOrSelfNode,
            AncestorOrSelfElement,
            AncestorOrSelfText,
            AncestorOrSelfComment,
            AncestorOrSelfProcessingInstruction,
            FollowingSiblingNode,
            FollowingSiblingElement,
            FollowingSiblingText,
            FollowingSiblingComment,
            FollowingSiblingProcessingInstruction,
            PrecedingSiblingNode,
            PrecedingSiblingElement,
            PrecedingSiblingText,
            PrecedingSiblingComment,
            PrecedingSiblingProcessingInstruction,
            FollowingNode,
            FollowingElement,
            FollowingText,
            FollowingComment,
            FollowingProcessingInstruction,
            PrecedingNode,
            PrecedingElement,
            PrecedingText,
            PrecedingComment,
            PrecedingProcessingInstruction,
            Node,
            Element,
            Attribute,
            Namespace,
            Text,
            Comment,
            ProcessingInstruction,
        };

        Kind kind = Kind::Element;
        XPathName name;
        std::string processingInstructionTarget;
    };

    struct PredicateTarget {
        enum class Kind {
            None,
            Literal,
            Text,
            Attribute,
            ContextNode,
            ChildPath,
            ContextText,
            Id,
            Name,
            LocalName,
            NamespaceUri,
            UnparsedEntityUri,
            Position,
            LastPosition,
            BooleanLiteral,
            PredicateExpression,
            Not,
            Boolean,
            Contains,
            StartsWith,
            EndsWith,
            Number,
            String,
            Concat,
            Translate,
            NormalizeSpace,
            StringLength,
            Count,
            Sum,
            Floor,
            Ceiling,
            Round,
            Negate,
            Add,
            Subtract,
            Multiply,
            Divide,
            Modulo,
            Substring,
            SubstringBefore,
            SubstringAfter,
        };

        Kind kind = Kind::None;
        std::string literal;
        bool numericLiteral = false;
        XPathName name;
        std::vector<PredicatePathSegment> path;
        std::string pathExpression;
        std::string predicateExpression;
        std::vector<PredicateTarget> arguments;
    };

    struct Predicate {
        enum class Kind {
            Exists,
            Equals,
            NotEquals,
            LessThan,
            LessThanOrEqual,
            GreaterThan,
            GreaterThanOrEqual,
            True,
            False,
            Contains,
            StartsWith,
            EndsWith,
            PositionEquals,
            Last,
            And,
            Or,
            Not,
        };

        Kind kind = Kind::Exists;
        PredicateTarget target;
        PredicateTarget comparisonTarget;
        bool hasComparisonTarget = false;
        std::size_t position = 0;
        std::string source;
        std::vector<Predicate> operands;
    };
    std::vector<Predicate> predicates;
};

struct XPathContext {
    const XmlNode* node = nullptr;
    const XmlNode* namespaceParent = nullptr;
};

bool IsXPathNamespaceDeclarationAttribute(const XmlAttribute& attribute);

bool IsXPathNamespaceContext(const XPathContext& context) {
    return context.node != nullptr
        && context.namespaceParent != nullptr
        && context.node->NodeType() == XmlNodeType::Attribute
        && IsXPathNamespaceDeclarationAttribute(*static_cast<const XmlAttribute*>(context.node));
}

const XmlNode* GetXPathContextParent(const XPathContext& context) {
    if (IsXPathNamespaceContext(context)) {
        return context.namespaceParent;
    }

    return context.node == nullptr ? nullptr : context.node->ParentNode();
}

void CollectAncestorContexts(const XPathContext& context, std::vector<XPathContext>& contexts) {
    const XmlNode* current = GetXPathContextParent(context);
    while (current != nullptr) {
        contexts.push_back({current});
        current = current->ParentNode();
    }
}

std::vector<const XmlNode*> ResolveXPathIdTargetNodes(
    const XmlNode& node,
    const XPathStep::PredicateTarget& argument,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count);

std::string EvaluateXPathUnparsedEntityUri(
    const XmlNode& node,
    const XPathStep::PredicateTarget& argument,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count);

std::shared_ptr<XmlNode> FindSharedNode(const XmlNode& node) {
    const auto* parent = node.ParentNode();
    if (parent == nullptr) {
        return nullptr;
    }

    if (node.NodeType() == XmlNodeType::Attribute && parent->NodeType() == XmlNodeType::Element) {
        if (auto shared = static_cast<const XmlAttribute&>(node).SharedFromOwnerElement(); shared != nullptr) {
            return shared;
        }

        const auto& attributes = static_cast<const XmlElement*>(parent)->Attributes();
        const auto found = std::find_if(attributes.begin(), attributes.end(), [&node](const auto& attribute) {
            return attribute.get() == &node;
        });
        return found == attributes.end() ? nullptr : *found;
    }

    if (auto shared = node.SharedFromParent(); shared != nullptr) {
        return shared;
    }

    const auto& siblings = parent->ChildNodes();
    const auto found = std::find_if(siblings.begin(), siblings.end(), [&node](const auto& child) {
        return child.get() == &node;
    });
    return found == siblings.end() ? nullptr : *found;
}

std::shared_ptr<XmlNode> GetXPathResultSharedNode(const XmlNode& node) {
    if (auto shared = FindSharedNode(node); shared != nullptr) {
        return shared;
    }

    if (node.NodeType() == XmlNodeType::Document) {
        return std::shared_ptr<XmlNode>(const_cast<XmlNode*>(&node), [](XmlNode*) {});
    }

    return nullptr;
}

XmlNodeList CreateXPathNodeListFromSingleNode(const XmlNode* node) {
    if (node == nullptr) {
        return {};
    }

    auto shared = GetXPathResultSharedNode(*node);
    if (shared == nullptr) {
        return {};
    }

    std::vector<std::shared_ptr<XmlNode>> nodes;
    nodes.push_back(std::move(shared));
    return XmlNodeList(std::move(nodes));
}

XmlNodeList CreateXPathNodeListFromContexts(const std::vector<XPathContext>& contexts) {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    nodes.reserve(contexts.size());
    for (const auto& context : contexts) {
        if (context.node == nullptr) {
            continue;
        }

        if (auto shared = GetXPathResultSharedNode(*context.node); shared != nullptr) {
            nodes.push_back(std::move(shared));
        }
    }

    return XmlNodeList(std::move(nodes));
}

XmlNodeList CreateXPathNodeListFromRawNodes(const std::vector<const XmlNode*>& rawNodes) {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    nodes.reserve(rawNodes.size());
    for (const auto* node : rawNodes) {
        if (node == nullptr) {
            continue;
        }

        if (auto shared = GetXPathResultSharedNode(*node); shared != nullptr) {
            nodes.push_back(std::move(shared));
        }
    }

    return XmlNodeList(std::move(nodes));
}

const XmlElement* FindTopElement(const XmlElement& element) {
    const XmlElement* current = &element;
    while (current->ParentNode() != nullptr && current->ParentNode()->NodeType() == XmlNodeType::Element) {
        current = static_cast<const XmlElement*>(current->ParentNode());
    }
    return current;
}

XPathName ParseXPathName(std::string token, bool attribute) {
    XPathName name;
    name.attribute = attribute;
    name.name = std::move(token);
    name.wildcard = name.name == "*" || name.name == "@*";

    std::string working = name.name;
    if (attribute && !working.empty() && working.front() == '@') {
        working.erase(working.begin());
    }

    if (working == "*") {
        name.wildcard = true;
        name.localName.clear();
        return name;
    }

    const auto separator = working.find(':');
    if (separator == std::string::npos) {
        name.localName = working;
        return name;
    }

    name.prefix = working.substr(0, separator);
    name.localName = working.substr(separator + 1);
    if (name.localName == "*") {
        name.wildcard = true;
    }
    return name;
}

bool MatchesXPathQualifiedName(const XmlNode& node, const XPathName& name, const XmlNamespaceManager* namespaces);
bool IsXPathTextNode(const XmlNode& node);
bool IsXPathVisibleNode(const XmlNode& node);
bool TryParseXPathProcessingInstructionNodeTest(const std::string& expression, std::string& target);
XPathStep::PredicateTarget ParseXPathPredicateTarget(std::string_view expression, const std::string& predicateText);
std::vector<XPathStep> ParseXPathSteps(std::string_view xpath, bool& absolutePath);
std::vector<XPathContext> EvaluateXPathContextsFromContext(
    const XPathContext& start,
    std::string_view xpath,
    const XmlNamespaceManager* namespaces);
std::vector<std::string> ExtractXPathPredicateTargetValues(
    const XmlNode& node,
    const XPathStep::PredicateTarget& target,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count);
std::string CoerceXPathTargetValuesToString(
    const XPathStep::PredicateTarget& target,
    const std::vector<std::string>& values);
bool CoerceXPathTargetValuesToBoolean(
    const XPathStep::PredicateTarget& target,
    const std::vector<std::string>& values);
double CoerceXPathTargetValuesToNumber(
    const XPathStep::PredicateTarget& target,
    const std::vector<std::string>& values);
std::string TrimAsciiWhitespace(std::string value);

[[noreturn]] void ThrowUnsupportedXPathFeature(const std::string& detail) {
    throw XmlException("Unsupported XPath feature: " + detail);
}

[[noreturn]] void ThrowInvalidXPathPredicate(const std::string& detail) {
    throw XmlException("Invalid XPath predicate: " + detail);
}

bool IsXPathNodeSetTargetKind(XPathStep::PredicateTarget::Kind kind) {
    return kind == XPathStep::PredicateTarget::Kind::ContextNode
        || kind == XPathStep::PredicateTarget::Kind::Text
        || kind == XPathStep::PredicateTarget::Kind::Attribute
    || kind == XPathStep::PredicateTarget::Kind::ChildPath
    || kind == XPathStep::PredicateTarget::Kind::Id;
}

void SortXPathUnionNodesInDocumentOrder(std::vector<std::shared_ptr<XmlNode>>& nodes);
void SortXPathUnionNodesInDocumentOrder(std::vector<const XmlNode*>& nodes);

bool UsesCompiledXPathPredicateTargetPath(const XPathStep::PredicateTarget& target) {
    return target.kind == XPathStep::PredicateTarget::Kind::ChildPath
        && !target.pathExpression.empty();
}

bool TryMatchSimpleXPathSelfExpression(
    const XmlNode& node,
    std::string_view expression,
    const XmlNamespaceManager* namespaces,
    const XmlNode*& matched) {
    const std::string normalized = TrimAsciiWhitespace(std::string(expression));
    if (normalized == "." || normalized == "self::node()") {
        matched = IsXPathVisibleNode(node) ? &node : nullptr;
        return true;
    }
    if (normalized == "self::text()") {
        matched = IsXPathTextNode(node) ? &node : nullptr;
        return true;
    }
    if (normalized == "self::comment()") {
        matched = node.NodeType() == XmlNodeType::Comment ? &node : nullptr;
        return true;
    }

    std::string processingInstructionTarget;
    if (normalized.size() > 6
        && normalized.rfind("self::", 0) == 0
        && TryParseXPathProcessingInstructionNodeTest(normalized.substr(6), processingInstructionTarget)) {
        matched = node.NodeType() == XmlNodeType::ProcessingInstruction
                && (processingInstructionTarget.empty() || node.Name() == processingInstructionTarget)
            ? &node
            : nullptr;
        return true;
    }

    if (normalized == "self::*") {
        matched = node.NodeType() == XmlNodeType::Element ? &node : nullptr;
        return true;
    }

    if (normalized.size() > 6 && normalized.rfind("self::", 0) == 0) {
        const auto name = ParseXPathName(normalized.substr(6), false);
        matched = node.NodeType() == XmlNodeType::Element && MatchesXPathQualifiedName(node, name, namespaces)
            ? &node
            : nullptr;
        return true;
    }

    matched = nullptr;
    return false;
}

const XmlNode* MatchSimpleXPathPredicateSelfTarget(
    const XmlNode& node,
    const XPathStep::PredicateTarget& target,
    const XmlNamespaceManager* namespaces) {
    if (target.kind != XPathStep::PredicateTarget::Kind::ChildPath) {
        return nullptr;
    }

    if (UsesCompiledXPathPredicateTargetPath(target)) {
        const XmlNode* matched = nullptr;
        return TryMatchSimpleXPathSelfExpression(node, target.pathExpression, namespaces, matched) ? matched : nullptr;
    }

    if (target.path.size() != 1) {
        return nullptr;
    }

    const auto& segment = target.path.front();
    switch (segment.kind) {
    case XPathStep::PredicatePathSegment::Kind::Self:
        return IsXPathVisibleNode(node) ? &node : nullptr;
    case XPathStep::PredicatePathSegment::Kind::SelfElement:
        return node.NodeType() == XmlNodeType::Element && MatchesXPathQualifiedName(node, segment.name, namespaces)
            ? &node
            : nullptr;
    case XPathStep::PredicatePathSegment::Kind::SelfText:
        return IsXPathTextNode(node) ? &node : nullptr;
    case XPathStep::PredicatePathSegment::Kind::SelfComment:
        return node.NodeType() == XmlNodeType::Comment ? &node : nullptr;
    case XPathStep::PredicatePathSegment::Kind::SelfProcessingInstruction:
        return node.NodeType() == XmlNodeType::ProcessingInstruction
                && (segment.processingInstructionTarget.empty() || node.Name() == segment.processingInstructionTarget)
            ? &node
            : nullptr;
    default:
        return nullptr;
    }
}

std::vector<const XmlNode*> CollectXPathNodeSetInDocumentOrder(const XmlNodeList& matches) {
    std::vector<const XmlNode*> nodes;
    nodes.reserve(matches.Count());
    for (const auto& match : matches) {
        if (match != nullptr) {
            nodes.push_back(match.get());
        }
    }

    SortXPathUnionNodesInDocumentOrder(nodes);
    return nodes;
}

bool ShouldUseCompiledXPathPredicateTargetPath(std::string_view expression) {
    return expression.find('[') != std::string_view::npos
        || expression.find("::") != std::string_view::npos
        || expression.find("//") != std::string_view::npos
        || expression.find('|') != std::string_view::npos
        || (!expression.empty() && expression.front() == '/');
}

bool ShouldUseDocumentOrderedXPathPredicateTargetPath(std::string_view expression) {
    return expression.find("preceding::") != std::string_view::npos
        || expression.find("preceding-sibling::") != std::string_view::npos
        || expression.find("ancestor::") != std::string_view::npos
        || expression.find("ancestor-or-self::") != std::string_view::npos;
}

bool IsXPathNumericTargetKind(const XPathStep::PredicateTarget& target) {
    return target.kind == XPathStep::PredicateTarget::Kind::Number
        || target.kind == XPathStep::PredicateTarget::Kind::Count
        || target.kind == XPathStep::PredicateTarget::Kind::StringLength
        || target.kind == XPathStep::PredicateTarget::Kind::Position
        || target.kind == XPathStep::PredicateTarget::Kind::LastPosition
        || target.kind == XPathStep::PredicateTarget::Kind::Sum
        || target.kind == XPathStep::PredicateTarget::Kind::Floor
        || target.kind == XPathStep::PredicateTarget::Kind::Ceiling
        || target.kind == XPathStep::PredicateTarget::Kind::Round
        || target.kind == XPathStep::PredicateTarget::Kind::Negate
        || target.kind == XPathStep::PredicateTarget::Kind::Add
        || target.kind == XPathStep::PredicateTarget::Kind::Subtract
        || target.kind == XPathStep::PredicateTarget::Kind::Multiply
        || target.kind == XPathStep::PredicateTarget::Kind::Divide
        || target.kind == XPathStep::PredicateTarget::Kind::Modulo
        || (target.kind == XPathStep::PredicateTarget::Kind::Literal && target.numericLiteral);
}

bool IsXPathBooleanTargetKind(const XPathStep::PredicateTarget::Kind kind) {
    return kind == XPathStep::PredicateTarget::Kind::PredicateExpression
    || kind == XPathStep::PredicateTarget::Kind::Boolean
    || kind == XPathStep::PredicateTarget::Kind::Not
    || kind == XPathStep::PredicateTarget::Kind::Contains
    || kind == XPathStep::PredicateTarget::Kind::StartsWith
    || kind == XPathStep::PredicateTarget::Kind::EndsWith
        || kind == XPathStep::PredicateTarget::Kind::BooleanLiteral;
}

std::string NormalizeXPathPublicExpression(std::string_view expression) {
    std::string normalized = TrimAsciiWhitespace(std::string(expression));
    if (normalized.empty()) {
        throw XmlException("XPath expression cannot be empty");
    }

    return normalized;
}

XPathStep::PredicateTarget ParseXPathPublicTarget(std::string_view expression) {
    const std::string normalized = NormalizeXPathPublicExpression(expression);
    return ParseXPathPredicateTarget(normalized, normalized);
}

void ValidateXPathPublicExpressionSyntax(const std::string& normalized, const XPathStep::PredicateTarget& target) {
    if (!IsXPathNodeSetTargetKind(target.kind) || target.kind == XPathStep::PredicateTarget::Kind::Id) {
        return;
    }

    bool absolutePath = false;
    (void)ParseXPathSteps(normalized, absolutePath);
}

std::vector<XPathNavigator> CreateXPathNavigatorVector(const std::vector<XPathContext>& contexts) {
    std::vector<XPathNavigator> result;
    result.reserve(contexts.size());
    for (const auto& context : contexts) {
        if (context.node != nullptr) {
            result.emplace_back(context.node, context.namespaceParent);
        }
    }
    return result;
}

std::vector<XPathNavigator> EvaluateXPathNodeSetValue(
    const XPathContext& context,
    const std::string& expression,
    const XPathStep::PredicateTarget& target,
    const XmlNamespaceManager* namespaces) {
    if (context.node == nullptr) {
        throw XmlException("Cannot evaluate XPath on an empty navigator");
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Id) {
        std::vector<const XmlNode*> nodes = ResolveXPathIdTargetNodes(*context.node, target.arguments.front(), namespaces, 1, 1);
        SortXPathUnionNodesInDocumentOrder(nodes);

        std::vector<XPathNavigator> result;
        result.reserve(nodes.size());
        for (const auto* node : nodes) {
            if (node != nullptr) {
                result.emplace_back(node);
            }
        }
        return result;
    }

    return CreateXPathNavigatorVector(EvaluateXPathContextsFromContext(context, expression, namespaces));
}

XPathValue EvaluateXPathValueFromContext(
    const XPathContext& context,
    std::string_view expression,
    const XmlNamespaceManager* namespaces) {
    const std::string normalized = NormalizeXPathPublicExpression(expression);
    const auto target = ParseXPathPublicTarget(normalized);

    if (IsXPathNodeSetTargetKind(target.kind)) {
        return XPathValue(EvaluateXPathNodeSetValue(context, normalized, target, namespaces));
    }

    if (context.node == nullptr) {
        throw XmlException("Cannot evaluate XPath on an empty navigator");
    }

    const auto values = ExtractXPathPredicateTargetValues(*context.node, target, namespaces, 1, 1);
    if (IsXPathBooleanTargetKind(target.kind)) {
        return XPathValue(CoerceXPathTargetValuesToBoolean(target, values));
    }

    if (IsXPathNumericTargetKind(target)) {
        return XPathValue(CoerceXPathTargetValuesToNumber(target, values));
    }

    return XPathValue(CoerceXPathTargetValuesToString(target, values));
}

std::string TrimAsciiWhitespace(std::string value);
bool TryParseXPathNumber(const std::string& expression, double& value);

std::string FormatXPathNumber(double value) {
    if (std::isnan(value)) {
        return "NaN";
    }

    if (std::isinf(value)) {
        return value < 0 ? "-Infinity" : "Infinity";
    }

    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    std::string text = stream.str();
    const auto exponent = text.find_first_of("eE");
    if (exponent != std::string::npos) {
        return text;
    }

    if (const auto decimal = text.find('.'); decimal != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }

    if (text == "-0") {
        return "0";
    }
    return text.empty() ? "0" : text;
}

double ParseXPathNumberOrNaN(const std::string& text) {
    double value = 0.0;
    if (TryParseXPathNumber(TrimAsciiWhitespace(text), value)) {
        return value;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

bool ParseXPathBooleanLiteral(const std::string& text) {
    return text == "true";
}

double RoundXPathNumber(double value) {
    if (std::isnan(value) || std::isinf(value) || value == 0.0) {
        return value;
    }

    return std::floor(value + 0.5);
}

std::string TranslateXPathString(const std::string& value, const std::string& from, const std::string& to) {
    std::string translated;
    translated.reserve(value.size());

    for (const char ch : value) {
        const auto position = from.find(ch);
        if (position == std::string::npos) {
            translated.push_back(ch);
            continue;
        }

        if (position < to.size()) {
            translated.push_back(to[position]);
        }
    }

    return translated;
}

bool TryParseXPathProcessingInstructionNodeTest(
    const std::string& expression,
    std::string& target);

bool IsWrappedXPathExpression(std::string_view expression);

bool MatchesXPathElement(const XmlNode& node, const XPathStep& step, const XmlNamespaceManager* namespaces);

void CollectFollowingContexts(const XPathContext& context, std::vector<XPathContext>& contexts);
void CollectFollowingContexts(const XmlNode& node, std::vector<XPathContext>& contexts);

void CollectPrecedingContexts(const XPathContext& context, std::vector<XPathContext>& contexts);
void CollectPrecedingContexts(const XmlNode& node, std::vector<XPathContext>& contexts);

bool CollectPrecedingCandidates(
    const std::shared_ptr<XmlNode>& current,
    const XmlNode& target,
    const std::unordered_set<const XmlNode*>& ancestors,
    std::vector<XPathContext>& preceding) {
    if (current == nullptr) {
        return false;
    }

    if (current.get() == &target) {
        return true;
    }

    if (ancestors.find(current.get()) == ancestors.end()) {
        preceding.push_back({current.get()});
    }

    for (const auto& child : current->ChildNodes()) {
        if (CollectPrecedingCandidates(child, target, ancestors, preceding)) {
            return true;
        }
    }

    return false;
}

bool CollectMatchingPrecedingCandidates(
    const std::shared_ptr<XmlNode>& current,
    const XmlNode& target,
    const std::unordered_set<const XmlNode*>& ancestors,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& preceding) {
    if (current == nullptr) {
        return false;
    }

    if (current.get() == &target) {
        return true;
    }

    if (ancestors.find(current.get()) == ancestors.end()
        && MatchesXPathElement(*current, step, namespaces)) {
        preceding.push_back({current.get()});
    }

    for (const auto& child : current->ChildNodes()) {
        if (CollectMatchingPrecedingCandidates(child, target, ancestors, step, namespaces, preceding)) {
            return true;
        }
    }

    return false;
}

void SortXPathUnionNodesInDocumentOrder(std::vector<std::shared_ptr<XmlNode>>& nodes);

void CollectXPathDescendantNodes(const XmlNode& node, std::vector<const XmlNode*>& nodes) {
    for (const auto& child : node.ChildNodes()) {
        if (!child) {
            continue;
        }

        nodes.push_back(child.get());
        CollectXPathDescendantNodes(*child, nodes);
    }
}

const XmlNode* GetXPathPredicatePathParent(const XmlNode& node, const XmlNode* namespaceParent) {
    if (namespaceParent != nullptr
        && node.NodeType() == XmlNodeType::Attribute
        && IsXPathNamespaceDeclarationAttribute(*static_cast<const XmlAttribute*>(&node))) {
        return namespaceParent;
    }

    return node.ParentNode();
}

void CollectXPathAncestorNodes(const XmlNode& node, std::vector<const XmlNode*>& nodes, bool includeSelf, const XmlNode* namespaceParent = nullptr) {
    const XmlNode* current = includeSelf ? &node : GetXPathPredicatePathParent(node, namespaceParent);
    if (includeSelf) {
        current = GetXPathPredicatePathParent(node, namespaceParent);
    }
    if (includeSelf) {
        nodes.push_back(&node);
    }
    while (current != nullptr) {
        nodes.push_back(current);
        current = current->ParentNode();
    }
}

void CollectXPathSiblingNodes(const XmlNode& node, std::vector<const XmlNode*>& nodes, bool following) {
    if (node.NodeType() == XmlNodeType::Attribute) {
        return;
    }

    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }

    const auto& siblings = parent->ChildNodes();
    if (following) {
        bool found = false;
        for (const auto& sibling : siblings) {
            if (!sibling) {
                continue;
            }

            if (found) {
                nodes.push_back(sibling.get());
            } else if (sibling.get() == &node) {
                found = true;
            }
        }
        return;
    }

    std::vector<const XmlNode*> preceding;
    for (const auto& sibling : siblings) {
        if (!sibling) {
            continue;
        }
        if (sibling.get() == &node) {
            break;
        }
        preceding.push_back(sibling.get());
    }

    for (auto index = preceding.size(); index > 0; --index) {
        nodes.push_back(preceding[index - 1]);
    }
}

void CollectXPathDocumentOrderNodes(const XmlNode& node, std::vector<const XmlNode*>& nodes, bool following, const XmlNode* namespaceParent = nullptr) {
    std::vector<XPathContext> documentOrderContexts;
    XPathContext context{&node, namespaceParent};
    if (following) {
        CollectFollowingContexts(context, documentOrderContexts);
    } else {
        CollectPrecedingContexts(context, documentOrderContexts);
    }

    for (const auto& item : documentOrderContexts) {
        if (item.node != nullptr) {
            nodes.push_back(item.node);
        }
    }
}

std::string GetXPathPredicatePathNodeStringValue(const XmlNode& node) {
    return (node.NodeType() == XmlNodeType::Element || node.NodeType() == XmlNodeType::Document)
        ? node.InnerText()
        : node.Value();
}

bool IsXPathVisibleNode(const XmlNode& node) {
    return node.NodeType() != XmlNodeType::XmlDeclaration
        && node.NodeType() != XmlNodeType::DocumentType;
}

bool IsXPathNamespaceDeclarationAttribute(const XmlAttribute& attribute) {
    return attribute.Name() == "xmlns"
        || attribute.Prefix() == "xmlns"
        || attribute.NamespaceURI() == "http://www.w3.org/2000/xmlns/";
}

bool IsXPathVisibleAttribute(const XmlAttribute& attribute) {
    return !IsXPathNamespaceDeclarationAttribute(attribute);
}

bool MatchesXPathNamespace(const XmlAttribute& attribute, const XPathStep& step) {
    if (!IsXPathNamespaceDeclarationAttribute(attribute)) {
        return false;
    }

    if (step.nodeTest) {
        return true;
    }

    if (step.textNode || step.commentNode || step.processingInstructionNode) {
        return false;
    }

    if (step.name.wildcard) {
        return true;
    }

    if (!step.name.prefix.empty()) {
        return false;
    }

    return NamespaceDeclarationPrefixView(attribute.Name()) == step.name.localName;
}

bool MatchesXPathNamespaceName(const XmlAttribute& attribute, const XPathName& name) {
    if (!IsXPathNamespaceDeclarationAttribute(attribute)) {
        return false;
    }

    if (name.wildcard) {
        return true;
    }

    if (!name.prefix.empty()) {
        return false;
    }

    return NamespaceDeclarationPrefixView(attribute.Name()) == name.localName;
}

void CollectXPathNamespaceNodes(const XmlNode& node, std::vector<const XmlNode*>& nodes) {
    if (node.NodeType() != XmlNodeType::Element) {
        return;
    }

    std::unordered_set<std::string> seenPrefixes;
    auto current = static_cast<const XmlElement*>(&node);
    while (current != nullptr) {
        for (const auto& attribute : current->Attributes()) {
            if (attribute == nullptr || !IsXPathNamespaceDeclarationAttribute(*attribute)) {
                continue;
            }

            const std::string prefix(NamespaceDeclarationPrefixView(attribute->Name()));
            if (seenPrefixes.insert(prefix).second) {
                nodes.push_back(attribute.get());
            }
        }

        const auto* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }
}

struct XPathNavigatorShortcutResult {
    bool handled = false;
    XPathContext context;
};

XPathNavigatorShortcutResult TryEvaluateXPathNavigatorShortcut(const XPathContext& context, std::string_view xpath) {
    if (context.node == nullptr) {
        return {};
    }

    std::string normalized = TrimAsciiWhitespace(std::string(xpath));
    while (IsWrappedXPathExpression(normalized)) {
        normalized = TrimAsciiWhitespace(normalized.substr(1, normalized.size() - 2));
    }

    if (normalized == "." || normalized == "self::node()") {
        return {true, context};
    }

    if (normalized == ".." || normalized == "parent::node()") {
        return {true, {GetXPathContextParent(context)}};
    }

    if (normalized == "/") {
        const XmlNode* current = context.namespaceParent != nullptr ? context.namespaceParent : context.node;
        while (current->ParentNode() != nullptr) {
            current = current->ParentNode();
        }
        return {true, {current}};
    }

    return {};
}

XmlNodeList EvaluateXPathFromNode(const XmlNode& node, std::string_view xpath, const XmlNamespaceManager* namespaces);

bool IsXPathPredicatePathDescendantAxis(
    XPathStep::PredicatePathSegment::Kind kind,
    bool& includeSelf) {
    switch (kind) {
    case XPathStep::PredicatePathSegment::Kind::DescendantNode:
    case XPathStep::PredicatePathSegment::Kind::DescendantElement:
    case XPathStep::PredicatePathSegment::Kind::DescendantText:
    case XPathStep::PredicatePathSegment::Kind::DescendantComment:
    case XPathStep::PredicatePathSegment::Kind::DescendantProcessingInstruction:
        includeSelf = false;
        return true;
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfNode:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfElement:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfText:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfComment:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfProcessingInstruction:
        includeSelf = true;
        return true;
    default:
        return false;
    }
}

bool IsXPathPredicatePathAncestorAxis(
    XPathStep::PredicatePathSegment::Kind kind,
    bool& includeSelf) {
    switch (kind) {
    case XPathStep::PredicatePathSegment::Kind::AncestorNode:
    case XPathStep::PredicatePathSegment::Kind::AncestorElement:
    case XPathStep::PredicatePathSegment::Kind::AncestorText:
    case XPathStep::PredicatePathSegment::Kind::AncestorComment:
    case XPathStep::PredicatePathSegment::Kind::AncestorProcessingInstruction:
        includeSelf = false;
        return true;
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfNode:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfElement:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfText:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfComment:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfProcessingInstruction:
        includeSelf = true;
        return true;
    default:
        return false;
    }
}

bool IsXPathPredicatePathSiblingAxis(
    XPathStep::PredicatePathSegment::Kind kind,
    bool& following) {
    switch (kind) {
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingNode:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingElement:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingText:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingComment:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingProcessingInstruction:
        following = true;
        return true;
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingNode:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingElement:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingText:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingComment:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingProcessingInstruction:
        following = false;
        return true;
    default:
        return false;
    }
}

bool IsXPathPredicatePathDocumentOrderAxis(
    XPathStep::PredicatePathSegment::Kind kind,
    bool& following) {
    switch (kind) {
    case XPathStep::PredicatePathSegment::Kind::FollowingNode:
    case XPathStep::PredicatePathSegment::Kind::FollowingElement:
    case XPathStep::PredicatePathSegment::Kind::FollowingText:
    case XPathStep::PredicatePathSegment::Kind::FollowingComment:
    case XPathStep::PredicatePathSegment::Kind::FollowingProcessingInstruction:
        following = true;
        return true;
    case XPathStep::PredicatePathSegment::Kind::PrecedingNode:
    case XPathStep::PredicatePathSegment::Kind::PrecedingElement:
    case XPathStep::PredicatePathSegment::Kind::PrecedingText:
    case XPathStep::PredicatePathSegment::Kind::PrecedingComment:
    case XPathStep::PredicatePathSegment::Kind::PrecedingProcessingInstruction:
        following = false;
        return true;
    default:
        return false;
    }
}

bool MatchesXPathPredicatePathDescendantCandidate(
    const XmlNode& node,
    const XPathStep::PredicatePathSegment& segment,
    const XmlNamespaceManager* namespaces) {
    switch (segment.kind) {
    case XPathStep::PredicatePathSegment::Kind::DescendantNode:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfNode:
        return true;
    case XPathStep::PredicatePathSegment::Kind::DescendantElement:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfElement:
        return node.NodeType() == XmlNodeType::Element && MatchesXPathQualifiedName(node, segment.name, namespaces);
    case XPathStep::PredicatePathSegment::Kind::DescendantText:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfText:
        return IsXPathTextNode(node);
    case XPathStep::PredicatePathSegment::Kind::DescendantComment:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfComment:
        return node.NodeType() == XmlNodeType::Comment;
    case XPathStep::PredicatePathSegment::Kind::DescendantProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::DescendantOrSelfProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::AncestorProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::FollowingProcessingInstruction:
    case XPathStep::PredicatePathSegment::Kind::PrecedingProcessingInstruction:
        return node.NodeType() == XmlNodeType::ProcessingInstruction
            && (segment.processingInstructionTarget.empty() || node.Name() == segment.processingInstructionTarget);
    case XPathStep::PredicatePathSegment::Kind::AncestorNode:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfNode:
        return true;
    case XPathStep::PredicatePathSegment::Kind::AncestorElement:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfElement:
        return node.NodeType() == XmlNodeType::Element && MatchesXPathQualifiedName(node, segment.name, namespaces);
    case XPathStep::PredicatePathSegment::Kind::AncestorText:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfText:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingText:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingText:
    case XPathStep::PredicatePathSegment::Kind::FollowingText:
    case XPathStep::PredicatePathSegment::Kind::PrecedingText:
        return IsXPathTextNode(node);
    case XPathStep::PredicatePathSegment::Kind::AncestorComment:
    case XPathStep::PredicatePathSegment::Kind::AncestorOrSelfComment:
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingComment:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingComment:
    case XPathStep::PredicatePathSegment::Kind::FollowingComment:
    case XPathStep::PredicatePathSegment::Kind::PrecedingComment:
        return node.NodeType() == XmlNodeType::Comment;
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingNode:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingNode:
    case XPathStep::PredicatePathSegment::Kind::FollowingNode:
    case XPathStep::PredicatePathSegment::Kind::PrecedingNode:
        return true;
    case XPathStep::PredicatePathSegment::Kind::FollowingSiblingElement:
    case XPathStep::PredicatePathSegment::Kind::PrecedingSiblingElement:
    case XPathStep::PredicatePathSegment::Kind::FollowingElement:
    case XPathStep::PredicatePathSegment::Kind::PrecedingElement:
        return node.NodeType() == XmlNodeType::Element && MatchesXPathQualifiedName(node, segment.name, namespaces);
    default:
        return false;
    }
}

std::vector<XPathStep::PredicatePathSegment> ParseXPathRelativeNamePath(const std::string& expression) {
    std::vector<XPathStep::PredicatePathSegment> path;
    std::size_t start = 0;
    while (start < expression.size()) {
        const auto separator = expression.find('/', start);
        std::string segment = Trim(expression.substr(start, separator == std::string::npos ? std::string::npos : separator - start));
        if (segment.empty()) {
            ThrowUnsupportedXPathFeature("predicate path [" + expression + "]");
        }

        XPathStep::PredicatePathSegment parsed;
        const auto axisDelimiter = segment.find("::");
        if (axisDelimiter != std::string::npos) {
            const std::string axisName = segment.substr(0, axisDelimiter);
            std::string nodeTestPart = segment.substr(axisDelimiter + 2);

            if (axisName == "child") {
                segment = std::move(nodeTestPart);
            } else if (axisName == "attribute") {
                if (!nodeTestPart.empty() && nodeTestPart.front() != '@') {
                    nodeTestPart.insert(nodeTestPart.begin(), '@');
                }
                segment = std::move(nodeTestPart);
            } else if (axisName == "self") {
                if (nodeTestPart == "node()") {
                    segment = ".";
                } else if (nodeTestPart == "text()") {
                    parsed.kind = XPathStep::PredicatePathSegment::Kind::SelfText;
                } else if (nodeTestPart == "comment()") {
                    parsed.kind = XPathStep::PredicatePathSegment::Kind::SelfComment;
                } else if (TryParseXPathProcessingInstructionNodeTest(nodeTestPart, parsed.processingInstructionTarget)) {
                    parsed.kind = XPathStep::PredicatePathSegment::Kind::SelfProcessingInstruction;
                } else {
                    parsed.kind = XPathStep::PredicatePathSegment::Kind::SelfElement;
                    parsed.name = ParseXPathName(nodeTestPart, false);
                }
            } else if (axisName == "parent") {
                if (nodeTestPart == "node()") {
                    segment = "..";
                } else {
                    parsed.kind = XPathStep::PredicatePathSegment::Kind::ParentElement;
                    parsed.name = ParseXPathName(nodeTestPart, false);
                }
            } else if (axisName == "descendant" || axisName == "descendant-or-self") {
                const bool includeSelf = axisName == "descendant-or-self";
                if (nodeTestPart == "node()") {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::DescendantOrSelfNode
                        : XPathStep::PredicatePathSegment::Kind::DescendantNode;
                } else if (nodeTestPart == "text()") {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::DescendantOrSelfText
                        : XPathStep::PredicatePathSegment::Kind::DescendantText;
                } else if (nodeTestPart == "comment()") {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::DescendantOrSelfComment
                        : XPathStep::PredicatePathSegment::Kind::DescendantComment;
                } else if (TryParseXPathProcessingInstructionNodeTest(nodeTestPart, parsed.processingInstructionTarget)) {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::DescendantOrSelfProcessingInstruction
                        : XPathStep::PredicatePathSegment::Kind::DescendantProcessingInstruction;
                } else {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::DescendantOrSelfElement
                        : XPathStep::PredicatePathSegment::Kind::DescendantElement;
                    parsed.name = ParseXPathName(nodeTestPart, false);
                }
            } else if (axisName == "ancestor" || axisName == "ancestor-or-self") {
                const bool includeSelf = axisName == "ancestor-or-self";
                if (nodeTestPart == "node()") {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::AncestorOrSelfNode
                        : XPathStep::PredicatePathSegment::Kind::AncestorNode;
                } else if (nodeTestPart == "text()") {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::AncestorOrSelfText
                        : XPathStep::PredicatePathSegment::Kind::AncestorText;
                } else if (nodeTestPart == "comment()") {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::AncestorOrSelfComment
                        : XPathStep::PredicatePathSegment::Kind::AncestorComment;
                } else if (TryParseXPathProcessingInstructionNodeTest(nodeTestPart, parsed.processingInstructionTarget)) {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::AncestorOrSelfProcessingInstruction
                        : XPathStep::PredicatePathSegment::Kind::AncestorProcessingInstruction;
                } else {
                    parsed.kind = includeSelf
                        ? XPathStep::PredicatePathSegment::Kind::AncestorOrSelfElement
                        : XPathStep::PredicatePathSegment::Kind::AncestorElement;
                    parsed.name = ParseXPathName(nodeTestPart, false);
                }
            } else if (axisName == "following-sibling" || axisName == "preceding-sibling") {
                const bool following = axisName == "following-sibling";
                if (nodeTestPart == "node()") {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingSiblingNode
                        : XPathStep::PredicatePathSegment::Kind::PrecedingSiblingNode;
                } else if (nodeTestPart == "text()") {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingSiblingText
                        : XPathStep::PredicatePathSegment::Kind::PrecedingSiblingText;
                } else if (nodeTestPart == "comment()") {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingSiblingComment
                        : XPathStep::PredicatePathSegment::Kind::PrecedingSiblingComment;
                } else if (TryParseXPathProcessingInstructionNodeTest(nodeTestPart, parsed.processingInstructionTarget)) {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingSiblingProcessingInstruction
                        : XPathStep::PredicatePathSegment::Kind::PrecedingSiblingProcessingInstruction;
                } else {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingSiblingElement
                        : XPathStep::PredicatePathSegment::Kind::PrecedingSiblingElement;
                    parsed.name = ParseXPathName(nodeTestPart, false);
                }
            } else if (axisName == "following" || axisName == "preceding") {
                const bool following = axisName == "following";
                if (nodeTestPart == "node()") {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingNode
                        : XPathStep::PredicatePathSegment::Kind::PrecedingNode;
                } else if (nodeTestPart == "text()") {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingText
                        : XPathStep::PredicatePathSegment::Kind::PrecedingText;
                } else if (nodeTestPart == "comment()") {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingComment
                        : XPathStep::PredicatePathSegment::Kind::PrecedingComment;
                } else if (TryParseXPathProcessingInstructionNodeTest(nodeTestPart, parsed.processingInstructionTarget)) {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingProcessingInstruction
                        : XPathStep::PredicatePathSegment::Kind::PrecedingProcessingInstruction;
                } else {
                    parsed.kind = following
                        ? XPathStep::PredicatePathSegment::Kind::FollowingElement
                        : XPathStep::PredicatePathSegment::Kind::PrecedingElement;
                    parsed.name = ParseXPathName(nodeTestPart, false);
                }
            } else if (axisName == "namespace") {
                if (nodeTestPart == "node()"
                    || nodeTestPart == "text()"
                    || nodeTestPart == "comment()"
                    || TryParseXPathProcessingInstructionNodeTest(nodeTestPart, parsed.processingInstructionTarget)) {
                    ThrowUnsupportedXPathFeature("predicate path [" + expression + "]");
                }

                parsed.kind = XPathStep::PredicatePathSegment::Kind::Namespace;
                parsed.name = ParseXPathName(nodeTestPart, false);
            } else {
                ThrowUnsupportedXPathFeature("predicate path [" + expression + "]");
            }
        }

        if (parsed.kind == XPathStep::PredicatePathSegment::Kind::SelfElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::ParentElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::SelfText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::SelfComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::SelfProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantOrSelfNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantOrSelfElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantOrSelfText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantOrSelfComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::DescendantOrSelfProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorOrSelfNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorOrSelfElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorOrSelfText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorOrSelfComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::AncestorOrSelfProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingSiblingNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingSiblingElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingSiblingText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingSiblingComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingSiblingProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingSiblingNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingSiblingElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingSiblingText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingSiblingComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingSiblingProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::FollowingProcessingInstruction
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingNode
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingElement
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingText
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingComment
            || parsed.kind == XPathStep::PredicatePathSegment::Kind::PrecedingProcessingInstruction) {
        } else if (segment == ".") {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Self;
        } else if (segment == "..") {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Parent;
        } else if (segment == "node()") {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Node;
        } else if (segment == "text()") {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Text;
        } else if (segment == "comment()") {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Comment;
        } else if (TryParseXPathProcessingInstructionNodeTest(segment, parsed.processingInstructionTarget)) {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::ProcessingInstruction;
        } else if (!segment.empty() && segment[0] == '@') {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Attribute;
            parsed.name = ParseXPathName(segment, true);
        } else {
            parsed.kind = XPathStep::PredicatePathSegment::Kind::Element;
            parsed.name = ParseXPathName(segment, false);
        }

        path.push_back(std::move(parsed));
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }

    if (path.empty()) {
        ThrowUnsupportedXPathFeature("predicate path [" + expression + "]");
    }

    return path;
}

bool MatchesXPathElementPathValue(
    const XmlNode& node,
    const std::vector<XPathStep::PredicatePathSegment>& path,
    const std::string& value,
    const XmlNamespaceManager* namespaces,
    std::size_t index = 0,
    const XmlNode* namespaceParent = nullptr) {
    if (index >= path.size()) {
        return false;
    }

    const auto& segment = path[index];
    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Self) {
        return MatchesXPathElementPathValue(node, path, value, namespaces, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfElement) {
        if (node.NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(node, segment.name, namespaces)) {
            return false;
        }

        if (index + 1 == path.size()) {
            return node.InnerText() == value;
        }

        return MatchesXPathElementPathValue(node, path, value, namespaces, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfText) {
        if (!IsXPathTextNode(node)) {
            return false;
        }

        if (index + 1 == path.size()) {
            return node.Value() == value;
        }

        return MatchesXPathElementPathValue(node, path, value, namespaces, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfComment) {
        if (node.NodeType() != XmlNodeType::Comment) {
            return false;
        }

        if (index + 1 == path.size()) {
            return node.Value() == value;
        }

        return MatchesXPathElementPathValue(node, path, value, namespaces, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfProcessingInstruction) {
        if (node.NodeType() != XmlNodeType::ProcessingInstruction
            || (!segment.processingInstructionTarget.empty() && node.Name() != segment.processingInstructionTarget)) {
            return false;
        }

        if (index + 1 == path.size()) {
            return node.Value() == value;
        }

        return MatchesXPathElementPathValue(node, path, value, namespaces, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Parent) {
        const auto* parent = GetXPathPredicatePathParent(node, namespaceParent);
        if (parent == nullptr) {
            return false;
        }

        if (index + 1 == path.size()) {
            return parent->InnerText() == value;
        }

        return MatchesXPathElementPathValue(*parent, path, value, namespaces, index + 1);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::ParentElement) {
        const auto* parent = GetXPathPredicatePathParent(node, namespaceParent);
        if (parent == nullptr || parent->NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(*parent, segment.name, namespaces)) {
            return false;
        }

        if (index + 1 == path.size()) {
            return parent->InnerText() == value;
        }

        return MatchesXPathElementPathValue(*parent, path, value, namespaces, index + 1);
    }

    bool descendantIncludeSelf = false;
    if (IsXPathPredicatePathDescendantAxis(segment.kind, descendantIncludeSelf)) {
        std::vector<const XmlNode*> candidates;
        if (descendantIncludeSelf) {
            candidates.push_back(&node);
        }
        CollectXPathDescendantNodes(node, candidates);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (GetXPathPredicatePathNodeStringValue(*candidate) == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*candidate, path, value, namespaces, index + 1, candidate == &node ? namespaceParent : nullptr)) {
                return true;
            }
        }
        return false;
    }

    bool ancestorIncludeSelf = false;
    if (IsXPathPredicatePathAncestorAxis(segment.kind, ancestorIncludeSelf)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathAncestorNodes(node, candidates, ancestorIncludeSelf, namespaceParent);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (GetXPathPredicatePathNodeStringValue(*candidate) == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*candidate, path, value, namespaces, index + 1, candidate == &node ? namespaceParent : nullptr)) {
                return true;
            }
        }
        return false;
    }

    bool siblingFollowing = false;
    if (IsXPathPredicatePathSiblingAxis(segment.kind, siblingFollowing)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathSiblingNodes(node, candidates, siblingFollowing);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (GetXPathPredicatePathNodeStringValue(*candidate) == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*candidate, path, value, namespaces, index + 1)) {
                return true;
            }
        }
        return false;
    }

    bool documentOrderFollowing = false;
    if (IsXPathPredicatePathDocumentOrderAxis(segment.kind, documentOrderFollowing)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathDocumentOrderNodes(node, candidates, documentOrderFollowing, namespaceParent);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (GetXPathPredicatePathNodeStringValue(*candidate) == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*candidate, path, value, namespaces, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Node) {
        for (const auto& child : node.ChildNodes()) {
            if (!child) {
                continue;
            }

            if (index + 1 == path.size()) {
                const auto childValue = child->NodeType() == XmlNodeType::Element
                    ? child->InnerText()
                    : child->Value();
                if (childValue == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*child, path, value, namespaces, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Text) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || !IsXPathTextNode(*child)) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (child->Value() == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*child, path, value, namespaces, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Comment) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || child->NodeType() != XmlNodeType::Comment) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (child->Value() == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*child, path, value, namespaces, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::ProcessingInstruction) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || child->NodeType() != XmlNodeType::ProcessingInstruction) {
                continue;
            }

            if (!segment.processingInstructionTarget.empty() && child->Name() != segment.processingInstructionTarget) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (child->Value() == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*child, path, value, namespaces, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Attribute) {
        if (index + 1 != path.size() || node.NodeType() != XmlNodeType::Element) {
            return false;
        }

        for (const auto& attribute : static_cast<const XmlElement*>(&node)->Attributes()) {
            if (!IsXPathNamespaceDeclarationAttribute(*attribute)
                && MatchesXPathQualifiedName(*attribute, segment.name, namespaces)
                && attribute->Value() == value) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Namespace) {
        if (node.NodeType() != XmlNodeType::Element) {
            return false;
        }

        std::vector<const XmlNode*> namespaceNodes;
        CollectXPathNamespaceNodes(node, namespaceNodes);
        for (const auto* namespaceNode : namespaceNodes) {
            if (namespaceNode == nullptr
                || namespaceNode->NodeType() != XmlNodeType::Attribute
                || !MatchesXPathNamespaceName(*static_cast<const XmlAttribute*>(namespaceNode), segment.name)) {
                continue;
            }

            if (index + 1 == path.size()) {
                if (namespaceNode->Value() == value) {
                    return true;
                }
                continue;
            }

            if (MatchesXPathElementPathValue(*namespaceNode, path, value, namespaces, index + 1, &node)) {
                return true;
            }
        }
        return false;
    }

    for (const auto& child : node.ChildNodes()) {
        if (!child || child->NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(*child, segment.name, namespaces)) {
            continue;
        }

        if (index + 1 == path.size()) {
            if (child->InnerText() == value) {
                return true;
            }
            continue;
        }

        if (MatchesXPathElementPathValue(*child, path, value, namespaces, index + 1)) {
            return true;
        }
    }

    return false;
}

std::string ResolveXPathNamespaceUri(const std::string& prefix, const XmlNamespaceManager* namespaces) {
    if (prefix.empty()) {
        return {};
    }

    if (prefix == "xml") {
        return "http://www.w3.org/XML/1998/namespace";
    }
    if (prefix == "xmlns") {
        return "http://www.w3.org/2000/xmlns/";
    }
    if (namespaces == nullptr) {
        return {};
    }

    const auto uri = namespaces->LookupNamespace(prefix);
    if (uri.empty()) {
        throw XmlException("Undefined XPath namespace prefix: " + prefix);
    }
    return uri;
}

bool MatchesXPathQualifiedName(const XmlNode& node, const XPathName& name, const XmlNamespaceManager* namespaces) {
    if (namespaces == nullptr) {
        if (name.wildcard) {
            if (!name.prefix.empty()) {
                const auto separator = node.Name().find(':');
                return separator != std::string::npos && node.Name().substr(0, separator) == name.prefix;
            }
            return true;
        }

        if (!name.prefix.empty()) {
            return node.Name() == name.name;
        }

        return node.Name() == name.name || node.LocalName() == name.localName;
    }

    if (name.prefix.empty()) {
        if (name.wildcard) {
            return true;
        }
        return node.LocalName() == name.localName && node.NamespaceURI().empty();
    }

    const auto expectedNamespace = ResolveXPathNamespaceUri(name.prefix, namespaces);
    if (name.wildcard) {
        return node.NamespaceURI() == expectedNamespace;
    }

    return node.LocalName() == name.localName && node.NamespaceURI() == expectedNamespace;
}

bool IsXPathTextNode(const XmlNode& node) {
    return node.NodeType() == XmlNodeType::Text || node.NodeType() == XmlNodeType::CDATA;
}

std::string TrimAsciiWhitespace(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (first == value.end()) {
        return {};
    }

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return std::string(first, last);
}

std::string_view TrimAsciiWhitespaceView(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

bool IsWrappedXPathExpression(std::string_view expression) {
    if (expression.size() < 2 || expression.front() != '(' || expression.back() != ')') {
        return false;
    }

    int depth = 0;
    char quote = '\0';
    for (std::size_t index = 0; index < expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            --depth;
            if (depth == 0 && index + 1 < expression.size()) {
                return false;
            }
        }
    }

    return depth == 0;
}

std::vector<std::string_view> SplitXPathExpressionTopLevel(std::string_view expression, std::string_view separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    int depth = 0;
    int bracketDepth = 0;
    char quote = '\0';

    for (std::size_t index = 0; index < expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (ch == '[') {
            ++bracketDepth;
            continue;
        }

        if (ch == ']') {
            --bracketDepth;
            continue;
        }

        if (depth == 0 && bracketDepth == 0 && index + separator.size() <= expression.size()
            && expression.substr(index, separator.size()) == separator) {
            parts.push_back(TrimAsciiWhitespaceView(expression.substr(start, index - start)));
            start = index + separator.size();
            index += separator.size() - 1;
        }
    }

    if (start == 0) {
        return {};
    }

    parts.push_back(TrimAsciiWhitespaceView(expression.substr(start)));
    return parts;
}

std::size_t FindXPathExpressionTopLevelCharacter(std::string_view expression, char target) {
    int depth = 0;
    int bracketDepth = 0;
    char quote = '\0';
    for (std::size_t index = 0; index < expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (ch == '[') {
            ++bracketDepth;
            continue;
        }

        if (ch == ']') {
            --bracketDepth;
            continue;
        }

        if (depth == 0 && bracketDepth == 0 && ch == target) {
            return index;
        }
    }

    return std::string_view::npos;
}

std::vector<std::string_view> ParseXPathFunctionArguments(std::string_view expression);

std::string ParseXPathQuotedLiteral(std::string_view expression, const std::string& predicate) {
    if (expression.size() < 2) {
        ThrowUnsupportedXPathFeature("predicate [" + predicate + "]");
    }

    const char quote = expression.front();
    if ((quote != '\'' && quote != '"') || expression.back() != quote) {
        ThrowUnsupportedXPathFeature("predicate [" + predicate + "]");
    }

    return std::string(expression.substr(1, expression.size() - 2));
}

bool TryParseXPathProcessingInstructionNodeTest(
    const std::string& expression,
    std::string& target) {
    constexpr std::string_view prefix = "processing-instruction";
    if (expression.rfind(prefix, 0) != 0 || expression.size() <= prefix.size()) {
        return false;
    }

    const std::string_view wrapped = std::string_view(expression).substr(prefix.size());
    if (!IsWrappedXPathExpression(wrapped)) {
        return false;
    }

    const auto arguments = ParseXPathFunctionArguments(wrapped.substr(1, wrapped.size() - 2));
    if (arguments.size() > 1) {
        ThrowUnsupportedXPathFeature("node test [" + expression + "]");
    }

    if (arguments.empty()) {
        target.clear();
        return true;
    }

    target = ParseXPathQuotedLiteral(arguments.front(), expression);
    return true;
}

bool TryParseXPathNumber(const std::string& expression, double& value) {
    try {
        std::size_t consumed = 0;
        value = std::stod(expression, &consumed);
        return consumed == expression.size();
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string_view> ParseXPathFunctionArguments(std::string_view expression) {
    std::vector<std::string_view> arguments;
    const std::string_view trimmed = TrimAsciiWhitespaceView(expression);
    if (trimmed.empty()) {
        return arguments;
    }

    std::size_t start = 0;
    int depth = 0;
    char quote = '\0';
    for (std::size_t index = 0; index < trimmed.size(); ++index) {
        const char ch = trimmed[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (depth == 0 && ch == ',') {
            arguments.push_back(TrimAsciiWhitespaceView(trimmed.substr(start, index - start)));
            start = index + 1;
        }
    }

    arguments.push_back(TrimAsciiWhitespaceView(trimmed.substr(start)));
    return arguments;
}

XPathStep::PredicateTarget ParseXPathPredicateTarget(std::string_view expression, const std::string& predicate);

std::optional<std::vector<std::string_view>> TryParseXPathFunctionCallArguments(
    const std::string& normalized,
    std::string_view functionName) {
    if (normalized.rfind(functionName, 0) != 0 || normalized.size() <= functionName.size()) {
        return std::nullopt;
    }

    const std::string_view wrapped = std::string_view(normalized).substr(functionName.size());
    if (wrapped.empty() || !IsWrappedXPathExpression(wrapped)) {
        return std::nullopt;
    }

    return ParseXPathFunctionArguments(
        wrapped.substr(1, wrapped.size() - 2));
}

void AppendXPathParsedTargetArguments(
    XPathStep::PredicateTarget& target,
    const std::vector<std::string_view>& arguments,
    const std::string& expression) {
    for (const auto& argument : arguments) {
        target.arguments.push_back(ParseXPathPredicateTarget(argument, expression));
    }
}

bool TryParseXPathFixedArityTarget(
    XPathStep::PredicateTarget& target,
    const std::string& normalized,
    const std::string& expression,
    std::string_view functionName,
    XPathStep::PredicateTarget::Kind kind,
    std::size_t arity) {
    const auto arguments = TryParseXPathFunctionCallArguments(normalized, functionName);
    if (!arguments.has_value()) {
        return false;
    }

    if (arguments->size() != arity) {
        ThrowUnsupportedXPathFeature("predicate [" + expression + "]");
    }

    target.kind = kind;
    AppendXPathParsedTargetArguments(target, *arguments, expression);
    return true;
}

bool TryParseXPathUnaryOptionalTarget(
    XPathStep::PredicateTarget& target,
    const std::string& normalized,
    const std::string& expression,
    std::string_view functionName,
    XPathStep::PredicateTarget::Kind kind) {
    const auto arguments = TryParseXPathFunctionCallArguments(normalized, functionName);
    if (!arguments.has_value()) {
        return false;
    }

    if (arguments->size() > 1) {
        ThrowUnsupportedXPathFeature("predicate [" + expression + "]");
    }

    target.kind = kind;
    AppendXPathParsedTargetArguments(target, *arguments, expression);
    return true;
}

bool TryParseXPathBinaryPredicateFunction(
    XPathStep::Predicate& predicate,
    const std::string& expression,
    const std::string& fullPredicate,
    std::string_view functionName,
    XPathStep::Predicate::Kind kind) {
    const auto arguments = TryParseXPathFunctionCallArguments(expression, functionName);
    if (!arguments.has_value()) {
        return false;
    }

    if (arguments->size() != 2) {
        ThrowUnsupportedXPathFeature("predicate [" + fullPredicate + "]");
    }

    predicate.kind = kind;
    predicate.target = ParseXPathPredicateTarget((*arguments)[0], fullPredicate);
    predicate.comparisonTarget = ParseXPathPredicateTarget((*arguments)[1], fullPredicate);
    predicate.hasComparisonTarget = true;
    return true;
}

std::size_t FindXPathExpressionTopLevelOperator(std::string_view expression, std::string_view token) {
    int depth = 0;
    int bracketDepth = 0;
    char quote = '\0';
    for (std::size_t index = 0; index + token.size() <= expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (ch == '[') {
            ++bracketDepth;
            continue;
        }

        if (ch == ']') {
            --bracketDepth;
            continue;
        }

        if (depth == 0 && bracketDepth == 0 && expression.substr(index, token.size()) == token) {
            return index;
        }
    }

    return std::string_view::npos;
}

std::size_t FindXPathExpressionTopLevelOperatorRightmost(std::string_view expression, std::string_view token) {
    int depth = 0;
    int bracketDepth = 0;
    char quote = '\0';
    std::size_t match = std::string_view::npos;
    for (std::size_t index = 0; index + token.size() <= expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (ch == '[') {
            ++bracketDepth;
            continue;
        }

        if (ch == ']') {
            --bracketDepth;
            continue;
        }

        if (depth == 0 && bracketDepth == 0 && expression.substr(index, token.size()) == token) {
            match = index;
        }
    }

    return match;
}

std::size_t FindXPathTopLevelAdditiveOperator(std::string_view expression) {
    int depth = 0;
    int bracketDepth = 0;
    char quote = '\0';
    std::size_t match = std::string_view::npos;
    for (std::size_t index = 0; index < expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (ch == '[') {
            ++bracketDepth;
            continue;
        }

        if (ch == ']') {
            --bracketDepth;
            continue;
        }

        if (depth != 0 || bracketDepth != 0 || (ch != '+' && ch != '-')) {
            continue;
        }

        if (index == 0) {
            continue;
        }

        const char previous = expression[index - 1];
        const char next = index + 1 < expression.size() ? expression[index + 1] : '\0';
        if (previous == '+' || previous == '-' || previous == '*' || previous == '/' || previous == '(') {
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(previous)) != 0
            && std::isalpha(static_cast<unsigned char>(next)) != 0) {
            continue;
        }

        match = index;
    }

    return match;
}

std::size_t FindXPathTopLevelMultiplyOperator(std::string_view expression) {
    int depth = 0;
    int bracketDepth = 0;
    char quote = '\0';
    std::size_t match = std::string_view::npos;
    for (std::size_t index = 0; index < expression.size(); ++index) {
        const char ch = expression[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++depth;
            continue;
        }

        if (ch == ')') {
            --depth;
            continue;
        }

        if (ch == '[') {
            ++bracketDepth;
            continue;
        }

        if (ch == ']') {
            --bracketDepth;
            continue;
        }

        if (depth == 0 && bracketDepth == 0 && ch == '*') {
            if (index == 0 || index + 1 >= expression.size()) {
                continue;
            }

            const char previous = expression[index - 1];
            if (previous == ':' || previous == '@' || previous == '/') {
                continue;
            }

            match = index;
        }
    }

    return match;
}

bool XPathLanguageMatches(const std::string& language, const std::string& requested) {
    if (requested.empty() || language.empty()) {
        return false;
    }

    std::string normalizedLanguage = language;
    std::string normalizedRequested = requested;
    std::transform(normalizedLanguage.begin(), normalizedLanguage.end(), normalizedLanguage.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(normalizedRequested.begin(), normalizedRequested.end(), normalizedRequested.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalizedLanguage == normalizedRequested) {
        return true;
    }

    return normalizedLanguage.size() > normalizedRequested.size()
        && normalizedLanguage.compare(0, normalizedRequested.size(), normalizedRequested) == 0
        && normalizedLanguage[normalizedRequested.size()] == '-';
}

std::string ResolveXPathLanguage(const XmlNode& node) {
    const XmlNode* current = &node;
    while (current != nullptr) {
        if (current->NodeType() == XmlNodeType::Element) {
            const auto* element = static_cast<const XmlElement*>(current);
            for (const auto& attribute : element->Attributes()) {
                if (attribute != nullptr && attribute->Name() == "xml:lang") {
                    return attribute->Value();
                }
            }
        }
        current = current->ParentNode();
    }
    return {};
}

std::string ParseXPathPredicateLiteral(const std::string& expression, const std::string& predicate) {
    const std::string normalized = TrimAsciiWhitespace(expression);
    if (normalized.empty()) {
        ThrowUnsupportedXPathFeature("predicate [" + predicate + "]");
    }

    if (normalized.front() == '\'' || normalized.front() == '"') {
        return ParseXPathQuotedLiteral(normalized, predicate);
    }

    double numericValue = 0.0;
    if (TryParseXPathNumber(normalized, numericValue)) {
        return normalized;
    }

    ThrowUnsupportedXPathFeature("predicate [" + predicate + "]");
}

std::string NormalizeXPathStringValue(const std::string& value) {
    std::string normalized;
    bool inWhitespace = true;
    for (unsigned char ch : value) {
        if (std::isspace(ch) != 0) {
            inWhitespace = true;
            continue;
        }

        if (!normalized.empty() && inWhitespace) {
            normalized.push_back(' ');
        }
        normalized.push_back(static_cast<char>(ch));
        inWhitespace = false;
    }
    return normalized;
}

bool IsXPathPredicateExpressionTarget(std::string_view expression) {
    return !SplitXPathExpressionTopLevel(expression, " or ").empty()
        || !SplitXPathExpressionTopLevel(expression, " and ").empty()
        || FindXPathExpressionTopLevelOperator(expression, "!=") != std::string::npos
        || FindXPathExpressionTopLevelOperator(expression, ">=") != std::string::npos
        || FindXPathExpressionTopLevelOperator(expression, "<=") != std::string::npos
        || FindXPathExpressionTopLevelOperator(expression, "=") != std::string::npos
        || FindXPathExpressionTopLevelOperator(expression, ">") != std::string::npos
        || FindXPathExpressionTopLevelOperator(expression, "<") != std::string::npos;
}

XPathStep::PredicateTarget ParseXPathPredicateTarget(std::string_view expression, const std::string& predicateText) {
    (void)predicateText;
    const std::string expressionText(expression);
    const std::string normalized = TrimAsciiWhitespace(expressionText);
    XPathStep::PredicateTarget target;
    if (IsWrappedXPathExpression(normalized)) {
        return ParseXPathPredicateTarget(normalized.substr(1, normalized.size() - 2), expressionText);
    }

    if (!normalized.empty() && (normalized.front() == '\'' || normalized.front() == '"')) {
        target.kind = XPathStep::PredicateTarget::Kind::Literal;
        target.literal = ParseXPathQuotedLiteral(normalized, expressionText);
        return target;
    }

    double numericValue = 0.0;
    if (TryParseXPathNumber(normalized, numericValue)) {
        target.kind = XPathStep::PredicateTarget::Kind::Literal;
        target.literal = normalized;
        target.numericLiteral = true;
        return target;
    }

    if (normalized == ".") {
        target.kind = XPathStep::PredicateTarget::Kind::ContextNode;
        return target;
    }

    if (normalized == "text()") {
        target.kind = XPathStep::PredicateTarget::Kind::Text;
        return target;
    }

    if (normalized == "position()") {
        target.kind = XPathStep::PredicateTarget::Kind::Position;
        return target;
    }

    if (normalized == "last()") {
        target.kind = XPathStep::PredicateTarget::Kind::LastPosition;
        return target;
    }

    if (normalized == "true()") {
        target.kind = XPathStep::PredicateTarget::Kind::BooleanLiteral;
        target.literal = "true";
        return target;
    }

    if (normalized == "false()") {
        target.kind = XPathStep::PredicateTarget::Kind::BooleanLiteral;
        target.literal = "false";
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "not", XPathStep::PredicateTarget::Kind::Not, 1)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "lang", XPathStep::PredicateTarget::Kind::Boolean, 1)) {
        target.literal = "lang";
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "contains", XPathStep::PredicateTarget::Kind::Contains, 2)
        || TryParseXPathFixedArityTarget(target, normalized, expressionText, "starts-with", XPathStep::PredicateTarget::Kind::StartsWith, 2)
        || TryParseXPathFixedArityTarget(target, normalized, expressionText, "ends-with", XPathStep::PredicateTarget::Kind::EndsWith, 2)) {
        return target;
    }

    if (IsXPathPredicateExpressionTarget(normalized)) {
        target.kind = XPathStep::PredicateTarget::Kind::PredicateExpression;
        target.predicateExpression = normalized;
        return target;
    }

    if (!normalized.empty() && normalized.front() == '-') {
        target.kind = XPathStep::PredicateTarget::Kind::Negate;
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(1), expressionText));
        return target;
    }

    const auto additive = FindXPathTopLevelAdditiveOperator(normalized);
    if (additive != std::string::npos) {
        target.kind = normalized[additive] == '+'
            ? XPathStep::PredicateTarget::Kind::Add
            : XPathStep::PredicateTarget::Kind::Subtract;
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(0, additive), expressionText));
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(additive + 1), expressionText));
        return target;
    }

    const auto divOperator = FindXPathExpressionTopLevelOperatorRightmost(normalized, " div ");
    if (divOperator != std::string::npos) {
        target.kind = XPathStep::PredicateTarget::Kind::Divide;
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(0, divOperator), expressionText));
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(divOperator + 5), expressionText));
        return target;
    }

    const auto modOperator = FindXPathExpressionTopLevelOperatorRightmost(normalized, " mod ");
    if (modOperator != std::string::npos) {
        target.kind = XPathStep::PredicateTarget::Kind::Modulo;
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(0, modOperator), expressionText));
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(modOperator + 5), expressionText));
        return target;
    }

    const auto multiplyOperator = FindXPathTopLevelMultiplyOperator(normalized);
    if (multiplyOperator != std::string::npos) {
        target.kind = XPathStep::PredicateTarget::Kind::Multiply;
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(0, multiplyOperator), expressionText));
        target.arguments.push_back(ParseXPathPredicateTarget(normalized.substr(multiplyOperator + 1), expressionText));
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "boolean", XPathStep::PredicateTarget::Kind::Boolean, 1)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "number", XPathStep::PredicateTarget::Kind::Number)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "string", XPathStep::PredicateTarget::Kind::String)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "id", XPathStep::PredicateTarget::Kind::Id, 1)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "name", XPathStep::PredicateTarget::Kind::Name)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "local-name", XPathStep::PredicateTarget::Kind::LocalName)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "namespace-uri", XPathStep::PredicateTarget::Kind::NamespaceUri)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "unparsed-entity-uri", XPathStep::PredicateTarget::Kind::UnparsedEntityUri, 1)) {
        return target;
    }

    if (const auto arguments = TryParseXPathFunctionCallArguments(normalized, "concat"); arguments.has_value()) {
        if (arguments->size() < 2) {
            ThrowUnsupportedXPathFeature("predicate [" + expressionText + "]");
        }
        target.kind = XPathStep::PredicateTarget::Kind::Concat;
        AppendXPathParsedTargetArguments(target, *arguments, expressionText);
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "translate", XPathStep::PredicateTarget::Kind::Translate, 3)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "normalize-space", XPathStep::PredicateTarget::Kind::NormalizeSpace)) {
        return target;
    }

    if (TryParseXPathUnaryOptionalTarget(target, normalized, expressionText, "string-length", XPathStep::PredicateTarget::Kind::StringLength)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "count", XPathStep::PredicateTarget::Kind::Count, 1)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "sum", XPathStep::PredicateTarget::Kind::Sum, 1)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "floor", XPathStep::PredicateTarget::Kind::Floor, 1)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "ceiling", XPathStep::PredicateTarget::Kind::Ceiling, 1)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "round", XPathStep::PredicateTarget::Kind::Round, 1)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "substring-before", XPathStep::PredicateTarget::Kind::SubstringBefore, 2)) {
        return target;
    }

    if (TryParseXPathFixedArityTarget(target, normalized, expressionText, "substring-after", XPathStep::PredicateTarget::Kind::SubstringAfter, 2)) {
        return target;
    }

    if (const auto arguments = TryParseXPathFunctionCallArguments(normalized, "substring"); arguments.has_value()) {
        if (arguments->size() != 2 && arguments->size() != 3) {
            ThrowUnsupportedXPathFeature("predicate [" + expressionText + "]");
        }
        target.kind = XPathStep::PredicateTarget::Kind::Substring;
        AppendXPathParsedTargetArguments(target, *arguments, expressionText);
        return target;
    }

    if (!normalized.empty() && normalized[0] == '@') {
        target.kind = XPathStep::PredicateTarget::Kind::Attribute;
        target.name = ParseXPathName(normalized, true);
        return target;
    }

    target.kind = XPathStep::PredicateTarget::Kind::ChildPath;
    if (ShouldUseCompiledXPathPredicateTargetPath(normalized)
        || ShouldUseDocumentOrderedXPathPredicateTargetPath(normalized)) {
        target.pathExpression = normalized;
        return target;
    }

    target.path = ParseXPathRelativeNamePath(normalized);
    for (const auto& segment : target.path) {
        if (segment.kind == XPathStep::PredicatePathSegment::Kind::Element) {
            target.name = segment.name;
            break;
        }
    }
    return target;
}

XPathStep::Predicate ParseXPathPredicateExpression(std::string_view predicate);
bool EvaluateXPathPredicate(
    const XPathContext& context,
    const XPathStep::Predicate& predicate,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count);

XPathStep::Predicate ParseXPathFunctionPredicate(const std::string& expression, const std::string& predicate) {
    XPathStep::Predicate result;
    if (TryParseXPathBinaryPredicateFunction(result, expression, predicate, "contains", XPathStep::Predicate::Kind::Contains)
        || TryParseXPathBinaryPredicateFunction(result, expression, predicate, "starts-with", XPathStep::Predicate::Kind::StartsWith)
        || TryParseXPathBinaryPredicateFunction(result, expression, predicate, "ends-with", XPathStep::Predicate::Kind::EndsWith)) {
        return result;
    }

    ThrowUnsupportedXPathFeature("predicate [" + predicate + "]");
}

std::string CoerceXPathTargetValuesToString(
    const XPathStep::PredicateTarget& target,
    const std::vector<std::string>& values) {
    if (values.empty()) {
        return {};
    }

    if (IsXPathBooleanTargetKind(target.kind)) {
        return ParseXPathBooleanLiteral(values.front()) ? "true" : "false";
    }

    if (IsXPathNumericTargetKind(target)) {
        return FormatXPathNumber(ParseXPathNumberOrNaN(values.front()));
    }

    return values.front();
}

bool CoerceXPathTargetValuesToBoolean(
    const XPathStep::PredicateTarget& target,
    const std::vector<std::string>& values) {
    if (IsXPathBooleanTargetKind(target.kind)) {
        return !values.empty() && ParseXPathBooleanLiteral(values.front());
    }

    if (IsXPathNumericTargetKind(target)) {
        const double value = values.empty() ? std::numeric_limits<double>::quiet_NaN() : ParseXPathNumberOrNaN(values.front());
        return !std::isnan(value) && value != 0.0;
    }

    if (IsXPathNodeSetTargetKind(target.kind)) {
        return !values.empty();
    }

    return !CoerceXPathTargetValuesToString(target, values).empty();
}

double CoerceXPathTargetValuesToNumber(
    const XPathStep::PredicateTarget& target,
    const std::vector<std::string>& values) {
    if (IsXPathBooleanTargetKind(target.kind)) {
        return CoerceXPathTargetValuesToBoolean(target, values) ? 1.0 : 0.0;
    }

    return ParseXPathNumberOrNaN(CoerceXPathTargetValuesToString(target, values));
}

bool CompareXPathStrings(
    const std::vector<std::string>& leftValues,
    const std::vector<std::string>& rightValues,
    bool equal) {
    for (const auto& left : leftValues) {
        for (const auto& right : rightValues) {
            if ((left == right) == equal) {
                return true;
            }
        }
    }
    return false;
}

bool CompareXPathNumbers(
    const std::vector<std::string>& leftValues,
    const std::vector<std::string>& rightValues,
    const std::function<bool(double, double)>& comparison) {
    for (const auto& leftText : leftValues) {
        const double left = ParseXPathNumberOrNaN(leftText);
        if (std::isnan(left)) {
            continue;
        }

        for (const auto& rightText : rightValues) {
            const double right = ParseXPathNumberOrNaN(rightText);
            if (!std::isnan(right) && comparison(left, right)) {
                return true;
            }
        }
    }
    return false;
}

void CollectXPathElementPathValues(
    const XmlNode& node,
    const std::vector<XPathStep::PredicatePathSegment>& path,
    const XmlNamespaceManager* namespaces,
    std::vector<std::string>& values,
    std::size_t index = 0,
    const XmlNode* namespaceParent = nullptr) {
    if (index >= path.size()) {
        return;
    }

    const auto& segment = path[index];
    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Self) {
        if (index + 1 == path.size()) {
            values.push_back(node.InnerText());
            return;
        }

        CollectXPathElementPathValues(node, path, namespaces, values, index + 1, namespaceParent);
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfElement) {
        if (node.NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(node, segment.name, namespaces)) {
            return;
        }

        if (index + 1 == path.size()) {
            values.push_back(node.InnerText());
            return;
        }

        CollectXPathElementPathValues(node, path, namespaces, values, index + 1, namespaceParent);
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfText) {
        if (!IsXPathTextNode(node)) {
            return;
        }

        if (index + 1 == path.size()) {
            values.push_back(node.Value());
            return;
        }

        CollectXPathElementPathValues(node, path, namespaces, values, index + 1, namespaceParent);
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfComment) {
        if (node.NodeType() != XmlNodeType::Comment) {
            return;
        }

        if (index + 1 == path.size()) {
            values.push_back(node.Value());
            return;
        }

        CollectXPathElementPathValues(node, path, namespaces, values, index + 1, namespaceParent);
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfProcessingInstruction) {
        if (node.NodeType() != XmlNodeType::ProcessingInstruction
            || (!segment.processingInstructionTarget.empty() && node.Name() != segment.processingInstructionTarget)) {
            return;
        }

        if (index + 1 == path.size()) {
            values.push_back(node.Value());
            return;
        }

        CollectXPathElementPathValues(node, path, namespaces, values, index + 1, namespaceParent);
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Parent) {
        const auto* parent = GetXPathPredicatePathParent(node, namespaceParent);
        if (parent == nullptr) {
            return;
        }

        if (index + 1 == path.size()) {
            values.push_back(parent->InnerText());
            return;
        }

        CollectXPathElementPathValues(*parent, path, namespaces, values, index + 1);
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::ParentElement) {
        const auto* parent = GetXPathPredicatePathParent(node, namespaceParent);
        if (parent == nullptr || parent->NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(*parent, segment.name, namespaces)) {
            return;
        }

        if (index + 1 == path.size()) {
            values.push_back(parent->InnerText());
            return;
        }

        CollectXPathElementPathValues(*parent, path, namespaces, values, index + 1);
        return;
    }

    bool descendantIncludeSelf = false;
    if (IsXPathPredicatePathDescendantAxis(segment.kind, descendantIncludeSelf)) {
        std::vector<const XmlNode*> candidates;
        if (descendantIncludeSelf) {
            candidates.push_back(&node);
        }
        CollectXPathDescendantNodes(node, candidates);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(GetXPathPredicatePathNodeStringValue(*candidate));
                continue;
            }

            CollectXPathElementPathValues(*candidate, path, namespaces, values, index + 1, candidate == &node ? namespaceParent : nullptr);
        }
        return;
    }

    bool ancestorIncludeSelf = false;
    if (IsXPathPredicatePathAncestorAxis(segment.kind, ancestorIncludeSelf)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathAncestorNodes(node, candidates, ancestorIncludeSelf, namespaceParent);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(GetXPathPredicatePathNodeStringValue(*candidate));
                continue;
            }

            CollectXPathElementPathValues(*candidate, path, namespaces, values, index + 1, candidate == &node ? namespaceParent : nullptr);
        }
        return;
    }

    bool siblingFollowing = false;
    if (IsXPathPredicatePathSiblingAxis(segment.kind, siblingFollowing)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathSiblingNodes(node, candidates, siblingFollowing);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(GetXPathPredicatePathNodeStringValue(*candidate));
                continue;
            }

            CollectXPathElementPathValues(*candidate, path, namespaces, values, index + 1);
        }
        return;
    }

    bool documentOrderFollowing = false;
    if (IsXPathPredicatePathDocumentOrderAxis(segment.kind, documentOrderFollowing)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathDocumentOrderNodes(node, candidates, documentOrderFollowing, namespaceParent);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(GetXPathPredicatePathNodeStringValue(*candidate));
                continue;
            }

            CollectXPathElementPathValues(*candidate, path, namespaces, values, index + 1);
        }
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Node) {
        for (const auto& child : node.ChildNodes()) {
            if (!child) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(child->NodeType() == XmlNodeType::Element
                    ? child->InnerText()
                    : child->Value());
                continue;
            }

            CollectXPathElementPathValues(*child, path, namespaces, values, index + 1);
        }
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Text) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || !IsXPathTextNode(*child)) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(child->Value());
                continue;
            }

            CollectXPathElementPathValues(*child, path, namespaces, values, index + 1);
        }
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Comment) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || child->NodeType() != XmlNodeType::Comment) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(child->Value());
                continue;
            }

            CollectXPathElementPathValues(*child, path, namespaces, values, index + 1);
        }
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::ProcessingInstruction) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || child->NodeType() != XmlNodeType::ProcessingInstruction) {
                continue;
            }

            if (!segment.processingInstructionTarget.empty() && child->Name() != segment.processingInstructionTarget) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(child->Value());
                continue;
            }

            CollectXPathElementPathValues(*child, path, namespaces, values, index + 1);
        }
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Attribute) {
        if (index + 1 != path.size() || node.NodeType() != XmlNodeType::Element) {
            return;
        }

        for (const auto& attribute : static_cast<const XmlElement*>(&node)->Attributes()) {
            if (!IsXPathNamespaceDeclarationAttribute(*attribute)
                && MatchesXPathQualifiedName(*attribute, segment.name, namespaces)) {
                values.push_back(attribute->Value());
            }
        }
        return;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Namespace) {
        if (node.NodeType() != XmlNodeType::Element) {
            return;
        }

        std::vector<const XmlNode*> namespaceNodes;
        CollectXPathNamespaceNodes(node, namespaceNodes);
        for (const auto* namespaceNode : namespaceNodes) {
            if (namespaceNode == nullptr
                || namespaceNode->NodeType() != XmlNodeType::Attribute
                || !MatchesXPathNamespaceName(*static_cast<const XmlAttribute*>(namespaceNode), segment.name)) {
                continue;
            }

            if (index + 1 == path.size()) {
                values.push_back(namespaceNode->Value());
                continue;
            }

            CollectXPathElementPathValues(*namespaceNode, path, namespaces, values, index + 1, &node);
        }
        return;
    }

    for (const auto& child : node.ChildNodes()) {
        if (!child || child->NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(*child, segment.name, namespaces)) {
            continue;
        }

        if (index + 1 == path.size()) {
            values.push_back(child->InnerText());
            continue;
        }

        CollectXPathElementPathValues(*child, path, namespaces, values, index + 1);
    }
}

bool TryResolveXPathPredicateTargetNode(
    const XmlNode& node,
    const XPathStep::PredicateTarget& target,
    const XmlNamespaceManager* namespaces,
    const XmlNode*& resolved,
    std::size_t index = 0,
    const XmlNode* namespaceParent = nullptr) {
    if (target.kind == XPathStep::PredicateTarget::Kind::ContextNode
        || target.kind == XPathStep::PredicateTarget::Kind::ContextText) {
        resolved = &node;
        return true;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Text) {
        for (const auto& child : node.ChildNodes()) {
            if (child && IsXPathTextNode(*child)) {
                resolved = child.get();
                return true;
            }
        }
        return false;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Attribute) {
        if (node.NodeType() != XmlNodeType::Element) {
            return false;
        }

        for (const auto& attribute : static_cast<const XmlElement*>(&node)->Attributes()) {
            if (attribute
                && !IsXPathNamespaceDeclarationAttribute(*attribute)
                && MatchesXPathQualifiedName(*attribute, target.name, namespaces)) {
                resolved = attribute.get();
                return true;
            }
        }
        return false;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Id) {
        if (target.arguments.size() != 1) {
            return false;
        }

        const auto matches = ResolveXPathIdTargetNodes(node, target.arguments.front(), namespaces, 0, 0);
        if (matches.empty()) {
            return false;
        }

        resolved = matches.front();
        return true;
    }

    if (target.kind != XPathStep::PredicateTarget::Kind::ChildPath) {
        return false;
    }

    if (const auto* simpleSelfMatch = MatchSimpleXPathPredicateSelfTarget(node, target, namespaces); simpleSelfMatch != nullptr) {
        resolved = simpleSelfMatch;
        return true;
    }

    if (UsesCompiledXPathPredicateTargetPath(target)) {
        const auto matches = EvaluateXPathFromNode(node, target.pathExpression, namespaces);
        const auto sortedMatches = CollectXPathNodeSetInDocumentOrder(matches);
        if (sortedMatches.empty()) {
            return false;
        }

        resolved = sortedMatches.front();
        return true;
    }

    if (index >= target.path.size()) {
        return false;
    }

    const auto& segment = target.path[index];
    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Self) {
        if (index + 1 == target.path.size()) {
            resolved = &node;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(node, target, namespaces, resolved, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfElement) {
        if (node.NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(node, segment.name, namespaces)) {
            return false;
        }

        if (index + 1 == target.path.size()) {
            resolved = &node;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(node, target, namespaces, resolved, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfText) {
        if (!IsXPathTextNode(node)) {
            return false;
        }

        if (index + 1 == target.path.size()) {
            resolved = &node;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(node, target, namespaces, resolved, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfComment) {
        if (node.NodeType() != XmlNodeType::Comment) {
            return false;
        }

        if (index + 1 == target.path.size()) {
            resolved = &node;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(node, target, namespaces, resolved, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::SelfProcessingInstruction) {
        if (node.NodeType() != XmlNodeType::ProcessingInstruction
            || (!segment.processingInstructionTarget.empty() && node.Name() != segment.processingInstructionTarget)) {
            return false;
        }

        if (index + 1 == target.path.size()) {
            resolved = &node;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(node, target, namespaces, resolved, index + 1, namespaceParent);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Parent) {
        const auto* parent = GetXPathPredicatePathParent(node, namespaceParent);
        if (parent == nullptr) {
            return false;
        }

        if (index + 1 == target.path.size()) {
            resolved = parent;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(*parent, target, namespaces, resolved, index + 1);
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::ParentElement) {
        const auto* parent = GetXPathPredicatePathParent(node, namespaceParent);
        if (parent == nullptr || parent->NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(*parent, segment.name, namespaces)) {
            return false;
        }

        if (index + 1 == target.path.size()) {
            resolved = parent;
            return true;
        }

        return TryResolveXPathPredicateTargetNode(*parent, target, namespaces, resolved, index + 1);
    }

    bool descendantIncludeSelf = false;
    if (IsXPathPredicatePathDescendantAxis(segment.kind, descendantIncludeSelf)) {
        std::vector<const XmlNode*> candidates;
        if (descendantIncludeSelf) {
            candidates.push_back(&node);
        }
        CollectXPathDescendantNodes(node, candidates);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = candidate;
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*candidate, target, namespaces, resolved, index + 1, candidate == &node ? namespaceParent : nullptr)) {
                return true;
            }
        }
        return false;
    }

    bool ancestorIncludeSelf = false;
    if (IsXPathPredicatePathAncestorAxis(segment.kind, ancestorIncludeSelf)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathAncestorNodes(node, candidates, ancestorIncludeSelf, namespaceParent);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = candidate;
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*candidate, target, namespaces, resolved, index + 1, candidate == &node ? namespaceParent : nullptr)) {
                return true;
            }
        }
        return false;
    }

    bool siblingFollowing = false;
    if (IsXPathPredicatePathSiblingAxis(segment.kind, siblingFollowing)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathSiblingNodes(node, candidates, siblingFollowing);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = candidate;
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*candidate, target, namespaces, resolved, index + 1)) {
                return true;
            }
        }
        return false;
    }

    bool documentOrderFollowing = false;
    if (IsXPathPredicatePathDocumentOrderAxis(segment.kind, documentOrderFollowing)) {
        std::vector<const XmlNode*> candidates;
        CollectXPathDocumentOrderNodes(node, candidates, documentOrderFollowing, namespaceParent);

        for (const XmlNode* candidate : candidates) {
            if (candidate == nullptr || !MatchesXPathPredicatePathDescendantCandidate(*candidate, segment, namespaces)) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = candidate;
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*candidate, target, namespaces, resolved, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Node) {
        for (const auto& child : node.ChildNodes()) {
            if (!child) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = child.get();
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*child, target, namespaces, resolved, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Text) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || !IsXPathTextNode(*child)) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = child.get();
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*child, target, namespaces, resolved, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Comment) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || child->NodeType() != XmlNodeType::Comment) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = child.get();
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*child, target, namespaces, resolved, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::ProcessingInstruction) {
        for (const auto& child : node.ChildNodes()) {
            if (!child || child->NodeType() != XmlNodeType::ProcessingInstruction) {
                continue;
            }

            if (!segment.processingInstructionTarget.empty() && child->Name() != segment.processingInstructionTarget) {
                continue;
            }

            if (index + 1 == target.path.size()) {
                resolved = child.get();
                return true;
            }

            if (TryResolveXPathPredicateTargetNode(*child, target, namespaces, resolved, index + 1)) {
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Attribute) {
        if (index + 1 != target.path.size() || node.NodeType() != XmlNodeType::Element) {
            return false;
        }

        for (const auto& attribute : static_cast<const XmlElement*>(&node)->Attributes()) {
            if (attribute
                && !IsXPathNamespaceDeclarationAttribute(*attribute)
                && MatchesXPathQualifiedName(*attribute, segment.name, namespaces)) {
                resolved = attribute.get();
                return true;
            }
        }
        return false;
    }

    if (segment.kind == XPathStep::PredicatePathSegment::Kind::Namespace) {
        if (node.NodeType() != XmlNodeType::Element) {
            return false;
        }

        std::vector<const XmlNode*> namespaceNodes;
        CollectXPathNamespaceNodes(node, namespaceNodes);
        for (const auto* namespaceNode : namespaceNodes) {
            if (namespaceNode != nullptr
                && namespaceNode->NodeType() == XmlNodeType::Attribute
                && MatchesXPathNamespaceName(*static_cast<const XmlAttribute*>(namespaceNode), segment.name)) {
                if (index + 1 == target.path.size()) {
                    resolved = namespaceNode;
                    return true;
                }

                if (TryResolveXPathPredicateTargetNode(*namespaceNode, target, namespaces, resolved, index + 1, &node)) {
                    return true;
                }
            }
        }
        return false;
    }

    for (const auto& child : node.ChildNodes()) {
        if (!child || child->NodeType() != XmlNodeType::Element || !MatchesXPathQualifiedName(*child, segment.name, namespaces)) {
            continue;
        }

        if (index + 1 == target.path.size()) {
            resolved = child.get();
            return true;
        }

        if (TryResolveXPathPredicateTargetNode(*child, target, namespaces, resolved, index + 1)) {
            return true;
        }
    }

    return false;
}

std::string EvaluateXPathNodeNameFunction(const XmlNode& node, XPathStep::PredicateTarget::Kind kind) {
    switch (node.NodeType()) {
    case XmlNodeType::Element:
        if (kind == XPathStep::PredicateTarget::Kind::Name) {
            return node.Name();
        }
        if (kind == XPathStep::PredicateTarget::Kind::LocalName) {
            return node.LocalName();
        }
        return node.NamespaceURI();
    case XmlNodeType::Attribute:
        if (kind == XPathStep::PredicateTarget::Kind::Name) {
            return node.Name();
        }
        if (kind == XPathStep::PredicateTarget::Kind::LocalName) {
            if (IsXPathNamespaceDeclarationAttribute(static_cast<const XmlAttribute&>(node))) {
                return std::string(NamespaceDeclarationPrefixView(node.Name()));
            }
            return node.LocalName();
        }
        return node.NamespaceURI();
    case XmlNodeType::ProcessingInstruction:
        if (kind == XPathStep::PredicateTarget::Kind::NamespaceUri) {
            return {};
        }
        return node.Name();
    default:
        return {};
    }
}

std::vector<std::string> ExtractXPathPredicateTargetValues(
    const XmlNode& node,
    const XPathStep::PredicateTarget& target,
    const XmlNamespaceManager* namespaces,
    std::size_t position = 0,
    std::size_t count = 0) {
    std::vector<std::string> values;
    auto extractArgumentValues = [&](const XPathStep::PredicateTarget& argument) {
        return ExtractXPathPredicateTargetValues(node, argument, namespaces, position, count);
    };
    auto firstValueOrEmpty = [&](const XPathStep::PredicateTarget& argument) {
        const auto argumentValues = extractArgumentValues(argument);
        return argumentValues.empty() ? std::string{} : argumentValues.front();
    };

    if (target.kind == XPathStep::PredicateTarget::Kind::Literal) {
        values.push_back(target.literal);
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::ContextNode) {
        values.push_back(GetXPathPredicatePathNodeStringValue(node));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::ContextText) {
        values.push_back(node.InnerText());
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::PredicateExpression) {
        XPathContext context;
        context.node = &node;
        values.push_back(EvaluateXPathPredicate(
            context,
            ParseXPathPredicateExpression(target.predicateExpression),
            namespaces,
            position,
            count) ? "true" : "false");
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Text) {
        for (const auto& child : node.ChildNodes()) {
            if (child && IsXPathTextNode(*child)) {
                values.push_back(child->Value());
            }
        }
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Id) {
        if (target.arguments.size() != 1) {
            return values;
        }

        for (const auto* match : ResolveXPathIdTargetNodes(node, target.arguments.front(), namespaces, position, count)) {
            values.push_back(match->InnerText());
        }
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Name
        || target.kind == XPathStep::PredicateTarget::Kind::LocalName
        || target.kind == XPathStep::PredicateTarget::Kind::NamespaceUri) {
        const XmlNode* subjectNode = &node;
        if (!target.arguments.empty()) {
            if (!TryResolveXPathPredicateTargetNode(node, target.arguments.front(), namespaces, subjectNode)) {
                values.push_back({});
                return values;
            }
        }

        values.push_back(EvaluateXPathNodeNameFunction(*subjectNode, target.kind));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::UnparsedEntityUri) {
        if (target.arguments.size() != 1) {
            return values;
        }

        values.push_back(EvaluateXPathUnparsedEntityUri(node, target.arguments.front(), namespaces, position, count));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Position) {
        values.push_back(std::to_string(position));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::LastPosition) {
        values.push_back(std::to_string(count));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Boolean) {
        if (target.literal == "lang") {
            if (target.arguments.size() != 1) {
                return values;
            }

            const auto requestedLanguage = CoerceXPathTargetValuesToString(
                target.arguments.front(),
                extractArgumentValues(target.arguments.front()));
            values.push_back(XPathLanguageMatches(ResolveXPathLanguage(node), requestedLanguage) ? "true" : "false");
            return values;
        }

        if (target.arguments.size() != 1) {
            return values;
        }

        const auto argumentValues = extractArgumentValues(target.arguments.front());
        values.push_back(CoerceXPathTargetValuesToBoolean(target.arguments.front(), argumentValues) ? "true" : "false");
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::BooleanLiteral) {
        values.push_back(target.literal == "true" ? "true" : "false");
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Contains
        || target.kind == XPathStep::PredicateTarget::Kind::StartsWith
        || target.kind == XPathStep::PredicateTarget::Kind::EndsWith) {
        if (target.arguments.size() != 2) {
            return values;
        }

        const auto haystack = CoerceXPathTargetValuesToString(target.arguments[0], extractArgumentValues(target.arguments[0]));
        const auto needle = CoerceXPathTargetValuesToString(target.arguments[1], extractArgumentValues(target.arguments[1]));
        bool matched = false;
        if (target.kind == XPathStep::PredicateTarget::Kind::Contains) {
            matched = haystack.find(needle) != std::string::npos;
        } else if (target.kind == XPathStep::PredicateTarget::Kind::StartsWith) {
            matched = haystack.rfind(needle, 0) == 0;
        } else {
            matched = haystack.size() >= needle.size()
                && haystack.compare(haystack.size() - needle.size(), needle.size(), needle) == 0;
        }

        values.push_back(matched ? "true" : "false");
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Not) {
        if (target.arguments.size() != 1) {
            return values;
        }

        const auto argumentValues = extractArgumentValues(target.arguments.front());
        values.push_back(CoerceXPathTargetValuesToBoolean(target.arguments.front(), argumentValues) ? "false" : "true");
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Number) {
        const XPathStep::PredicateTarget* argument = target.arguments.empty() ? nullptr : &target.arguments.front();
        const auto sourceValues = argument == nullptr
            ? std::vector<std::string>{node.InnerText()}
            : extractArgumentValues(*argument);
        const auto number = argument == nullptr
            ? ParseXPathNumberOrNaN(node.InnerText())
            : CoerceXPathTargetValuesToNumber(*argument, sourceValues);
        values.push_back(FormatXPathNumber(number));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::String) {
        const XPathStep::PredicateTarget* argument = target.arguments.empty() ? nullptr : &target.arguments.front();
        const auto sourceValues = argument == nullptr
            ? std::vector<std::string>{node.InnerText()}
            : extractArgumentValues(*argument);
        values.push_back(argument == nullptr ? node.InnerText() : CoerceXPathTargetValuesToString(*argument, sourceValues));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Concat) {
        std::string concatenated;
        for (const auto& argument : target.arguments) {
            concatenated += CoerceXPathTargetValuesToString(argument, extractArgumentValues(argument));
        }
        values.push_back(concatenated);
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Translate) {
        if (target.arguments.size() != 3) {
            return values;
        }

        values.push_back(TranslateXPathString(
            CoerceXPathTargetValuesToString(target.arguments[0], extractArgumentValues(target.arguments[0])),
            CoerceXPathTargetValuesToString(target.arguments[1], extractArgumentValues(target.arguments[1])),
            CoerceXPathTargetValuesToString(target.arguments[2], extractArgumentValues(target.arguments[2]))));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::NormalizeSpace) {
        const XPathStep::PredicateTarget* argument = target.arguments.empty() ? nullptr : &target.arguments.front();
        const auto sourceValues = argument == nullptr
            ? std::vector<std::string>{node.InnerText()}
            : extractArgumentValues(*argument);
        values.reserve(sourceValues.size());
        for (const auto& sourceValue : sourceValues) {
            values.push_back(NormalizeXPathStringValue(sourceValue));
        }
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::StringLength) {
        const XPathStep::PredicateTarget* argument = target.arguments.empty() ? nullptr : &target.arguments.front();
        const auto sourceValues = argument == nullptr
            ? std::vector<std::string>{node.InnerText()}
            : extractArgumentValues(*argument);
        values.reserve(sourceValues.size());
        for (const auto& sourceValue : sourceValues) {
            values.push_back(std::to_string(sourceValue.size()));
        }
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Count) {
        if (target.arguments.size() != 1) {
            return values;
        }
        values.push_back(std::to_string(extractArgumentValues(target.arguments.front()).size()));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Sum) {
        if (target.arguments.size() != 1) {
            return values;
        }

        const auto sourceValues = extractArgumentValues(target.arguments.front());
        double sum = 0.0;
        bool sawNaN = false;
        for (const auto& sourceValue : sourceValues) {
            const double numeric = ParseXPathNumberOrNaN(sourceValue);
            if (std::isnan(numeric)) {
                sawNaN = true;
                break;
            }
            sum += numeric;
        }

        values.push_back(FormatXPathNumber(sawNaN ? std::numeric_limits<double>::quiet_NaN() : sum));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Floor
        || target.kind == XPathStep::PredicateTarget::Kind::Ceiling
        || target.kind == XPathStep::PredicateTarget::Kind::Round) {
        if (target.arguments.size() != 1) {
            return values;
        }

        const auto sourceValues = extractArgumentValues(target.arguments.front());
        double numeric = CoerceXPathTargetValuesToNumber(target.arguments.front(), sourceValues);
        if (!std::isnan(numeric) && !std::isinf(numeric)) {
            if (target.kind == XPathStep::PredicateTarget::Kind::Floor) {
                numeric = std::floor(numeric);
            } else if (target.kind == XPathStep::PredicateTarget::Kind::Ceiling) {
                numeric = std::ceil(numeric);
            } else {
                numeric = RoundXPathNumber(numeric);
            }
        }

        values.push_back(FormatXPathNumber(numeric));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Negate) {
        if (target.arguments.size() != 1) {
            return values;
        }

        double numeric = CoerceXPathTargetValuesToNumber(
            target.arguments.front(),
            extractArgumentValues(target.arguments.front()));
        if (!std::isnan(numeric)) {
            numeric = -numeric;
        }

        values.push_back(FormatXPathNumber(numeric));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Add
        || target.kind == XPathStep::PredicateTarget::Kind::Subtract
        || target.kind == XPathStep::PredicateTarget::Kind::Multiply
        || target.kind == XPathStep::PredicateTarget::Kind::Divide
        || target.kind == XPathStep::PredicateTarget::Kind::Modulo) {
        if (target.arguments.size() != 2) {
            return values;
        }

        const double left = CoerceXPathTargetValuesToNumber(
            target.arguments[0],
            extractArgumentValues(target.arguments[0]));
        const double right = CoerceXPathTargetValuesToNumber(
            target.arguments[1],
            extractArgumentValues(target.arguments[1]));

        double result = std::numeric_limits<double>::quiet_NaN();
        if (!std::isnan(left) && !std::isnan(right)) {
            switch (target.kind) {
            case XPathStep::PredicateTarget::Kind::Add:
                result = left + right;
                break;
            case XPathStep::PredicateTarget::Kind::Subtract:
                result = left - right;
                break;
            case XPathStep::PredicateTarget::Kind::Multiply:
                result = left * right;
                break;
            case XPathStep::PredicateTarget::Kind::Divide:
                result = left / right;
                break;
            case XPathStep::PredicateTarget::Kind::Modulo:
                result = std::fmod(left, right);
                break;
            default:
                break;
            }
        }

        values.push_back(FormatXPathNumber(result));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::SubstringBefore) {
        if (target.arguments.size() != 2) {
            return values;
        }
        const auto text = firstValueOrEmpty(target.arguments[0]);
        const auto marker = firstValueOrEmpty(target.arguments[1]);
        const auto markerPosition = text.find(marker);
        values.push_back(markerPosition == std::string::npos ? std::string{} : text.substr(0, markerPosition));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::SubstringAfter) {
        if (target.arguments.size() != 2) {
            return values;
        }
        const auto text = firstValueOrEmpty(target.arguments[0]);
        const auto marker = firstValueOrEmpty(target.arguments[1]);
        const auto markerPosition = text.find(marker);
        values.push_back(markerPosition == std::string::npos ? std::string{} : text.substr(markerPosition + marker.size()));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Substring) {
        if (target.arguments.size() != 2 && target.arguments.size() != 3) {
            return values;
        }

        const auto text = firstValueOrEmpty(target.arguments[0]);
        double startNumeric = 0.0;
        if (!TryParseXPathNumber(firstValueOrEmpty(target.arguments[1]), startNumeric)) {
            values.push_back({});
            return values;
        }

        const long long startIndex = std::llround(startNumeric) - 1;
        if (target.arguments.size() == 2) {
            const auto safeStart = static_cast<std::size_t>(std::max<long long>(0, startIndex));
            values.push_back(safeStart >= text.size() ? std::string{} : text.substr(safeStart));
            return values;
        }

        double lengthNumeric = 0.0;
        if (!TryParseXPathNumber(firstValueOrEmpty(target.arguments[2]), lengthNumeric)) {
            values.push_back({});
            return values;
        }

        const long long safeStart = std::max<long long>(0, startIndex);
        const long long safeLength = std::max<long long>(0, std::llround(lengthNumeric));
        if (static_cast<std::size_t>(safeStart) >= text.size() || safeLength == 0) {
            values.push_back({});
            return values;
        }
        values.push_back(text.substr(static_cast<std::size_t>(safeStart), static_cast<std::size_t>(safeLength)));
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::Attribute) {
        if (node.NodeType() != XmlNodeType::Element) {
            return values;
        }

        for (const auto& attribute : static_cast<const XmlElement*>(&node)->Attributes()) {
            if (!IsXPathNamespaceDeclarationAttribute(*attribute)
                && MatchesXPathQualifiedName(*attribute, target.name, namespaces)) {
                values.push_back(attribute->Value());
            }
        }
        return values;
    }

    if (target.kind == XPathStep::PredicateTarget::Kind::ChildPath) {
        if (const auto* simpleSelfMatch = MatchSimpleXPathPredicateSelfTarget(node, target, namespaces); simpleSelfMatch != nullptr) {
            values.push_back(GetXPathPredicatePathNodeStringValue(*simpleSelfMatch));
            return values;
        }

        if (UsesCompiledXPathPredicateTargetPath(target)) {
            const auto matches = EvaluateXPathFromNode(node, target.pathExpression, namespaces);
            const auto sortedMatches = CollectXPathNodeSetInDocumentOrder(matches);
            values.reserve(sortedMatches.size());
            for (const auto* match : sortedMatches) {
                if (match != nullptr) {
                    values.push_back(GetXPathPredicatePathNodeStringValue(*match));
                }
            }
            return values;
        }

        if (node.NodeType() != XmlNodeType::Element) {
            return values;
        }

        CollectXPathElementPathValues(node, target.path, namespaces, values);
    }

    return values;
}

const XmlDocument* ResolveXPathContextDocument(const XmlNode& node) {
    return node.OwnerDocument();
}

bool IsXPathAsciiWhitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

void SkipXPathAttlistWhitespace(std::string_view text, std::size_t& position) {
    while (position < text.size() && IsXPathAsciiWhitespace(text[position])) {
        ++position;
    }
}

std::size_t FindXPathAttlistDeclarationEnd(std::string_view text, std::size_t start) {
    char quote = '\0';
    for (std::size_t position = start; position < text.size(); ++position) {
        const char ch = text[position];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '>') {
            return position;
        }
    }

    return std::string_view::npos;
}

std::optional<std::string> ReadXPathAttlistToken(std::string_view text, std::size_t& position) {
    SkipXPathAttlistWhitespace(text, position);
    if (position >= text.size()) {
        return std::nullopt;
    }

    const char ch = text[position];
    if (ch == '\'' || ch == '"') {
        const char quote = ch;
        const std::size_t start = position++;
        while (position < text.size() && text[position] != quote) {
            ++position;
        }
        if (position < text.size()) {
            ++position;
        }
        return std::string(text.substr(start, position - start));
    }

    if (ch == '(') {
        const std::size_t start = position;
        int depth = 0;
        do {
            if (text[position] == '(') {
                ++depth;
            } else if (text[position] == ')') {
                --depth;
            }
            ++position;
        } while (position < text.size() && depth > 0);
        return std::string(text.substr(start, position - start));
    }

    const std::size_t start = position;
    while (position < text.size() && !IsXPathAsciiWhitespace(text[position])) {
        ++position;
    }
    return std::string(text.substr(start, position - start));
}

std::vector<std::pair<std::string, std::string>> CollectXPathDocumentTypeIdAttributes(const XmlDocument* document) {
    std::vector<std::pair<std::string, std::string>> declarations;
    if (document == nullptr || document->DocumentType() == nullptr) {
        return declarations;
    }

    const std::string_view subset = document->DocumentType()->InternalSubset();
    std::size_t searchStart = 0;
    while (searchStart < subset.size()) {
        const auto declarationStart = subset.find("<!ATTLIST", searchStart);
        if (declarationStart == std::string_view::npos) {
            break;
        }

        const auto declarationEnd = FindXPathAttlistDeclarationEnd(subset, declarationStart + 9);
        if (declarationEnd == std::string_view::npos) {
            break;
        }

        const std::string_view declaration = subset.substr(declarationStart + 9, declarationEnd - declarationStart - 9);
        std::size_t position = 0;
        const auto elementName = ReadXPathAttlistToken(declaration, position);
        if (elementName.has_value()) {
            while (true) {
                auto attributeName = ReadXPathAttlistToken(declaration, position);
                if (!attributeName.has_value()) {
                    break;
                }

                auto attributeType = ReadXPathAttlistToken(declaration, position);
                if (!attributeType.has_value()) {
                    break;
                }

                if (*attributeType == "NOTATION") {
                    if (!ReadXPathAttlistToken(declaration, position).has_value()) {
                        break;
                    }
                }

                const bool isId = *attributeType == "ID";
                auto defaultDeclaration = ReadXPathAttlistToken(declaration, position);
                if (!defaultDeclaration.has_value()) {
                    break;
                }
                if (*defaultDeclaration == "#FIXED") {
                    if (!ReadXPathAttlistToken(declaration, position).has_value()) {
                        break;
                    }
                }

                if (isId) {
                    declarations.emplace_back(*elementName, *attributeName);
                }
            }
        }

        searchStart = declarationEnd + 1;
    }

    return declarations;
}

bool IsXPathIdAttribute(const XmlElement& element, const XmlAttribute& attribute, const XmlDocument* document) {
    if (attribute.Name() == "xml:id"
        || (attribute.LocalName() == "id" && attribute.NamespaceURI() == "http://www.w3.org/XML/1998/namespace")) {
        return true;
    }

    if (attribute.Name() == "id" && attribute.NamespaceURI().empty()) {
        return true;
    }

    for (const auto& [elementName, attributeName] : CollectXPathDocumentTypeIdAttributes(document)) {
        if (element.Name() == elementName && attribute.Name() == attributeName) {
            return true;
        }
    }

    return false;
}

std::unordered_set<std::string> CollectXPathIdTokens(const std::vector<std::string>& sourceValues) {
    std::unordered_set<std::string> tokens;
    for (const auto& sourceValue : sourceValues) {
        std::size_t start = 0;
        while (start < sourceValue.size()) {
            while (start < sourceValue.size() && IsXPathAsciiWhitespace(sourceValue[start])) {
                ++start;
            }

            std::size_t end = start;
            while (end < sourceValue.size() && !IsXPathAsciiWhitespace(sourceValue[end])) {
                ++end;
            }

            if (end > start) {
                tokens.insert(sourceValue.substr(start, end - start));
            }
            start = end;
        }
    }
    return tokens;
}

void CollectXPathIdMatchedNodes(
    const XmlNode& node,
    const std::unordered_set<std::string>& tokens,
    const XmlDocument* document,
    std::vector<const XmlNode*>& matches,
    std::unordered_set<const XmlNode*>& seen) {
    if (tokens.empty()) {
        return;
    }

    if (node.NodeType() == XmlNodeType::Element) {
        const auto& element = static_cast<const XmlElement&>(node);
        for (const auto& attribute : element.Attributes()) {
            if (!attribute || !IsXPathIdAttribute(element, *attribute, document)) {
                continue;
            }

            if (tokens.find(attribute->Value()) != tokens.end() && seen.insert(std::addressof(node)).second) {
                matches.push_back(std::addressof(node));
                break;
            }
        }
    }

    for (const auto& child : node.ChildNodes()) {
        if (child) {
            CollectXPathIdMatchedNodes(*child, tokens, document, matches, seen);
        }
    }
}

std::vector<const XmlNode*> ResolveXPathIdTargetNodes(
    const XmlNode& node,
    const XPathStep::PredicateTarget& argument,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count) {
    const auto document = ResolveXPathContextDocument(node);
    if (document == nullptr || document->DocumentElement() == nullptr) {
        return {};
    }

    const auto argumentValues = ExtractXPathPredicateTargetValues(node, argument, namespaces, position, count);
    const auto tokens = CollectXPathIdTokens(argumentValues);

    std::vector<const XmlNode*> matches;
    std::unordered_set<const XmlNode*> seen;
    CollectXPathIdMatchedNodes(*document->DocumentElement(), tokens, document, matches, seen);
    return matches;
}

std::string EvaluateXPathUnparsedEntityUri(
    const XmlNode& node,
    const XPathStep::PredicateTarget& argument,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count) {
    const auto document = ResolveXPathContextDocument(node);
    if (document == nullptr || document->DocumentType() == nullptr) {
        return {};
    }

    const auto argumentValues = ExtractXPathPredicateTargetValues(node, argument, namespaces, position, count);
    const auto entityName = CoerceXPathTargetValuesToString(argument, argumentValues);
    if (entityName.empty() || !HasDocumentUnparsedEntityDeclaration(document, entityName)) {
        return {};
    }

    const auto entityNode = document->DocumentType()->Entities().GetNamedItem(entityName);
    if (entityNode == nullptr || entityNode->NodeType() != XmlNodeType::Entity) {
        return {};
    }

    return static_cast<const XmlEntity*>(entityNode.get())->SystemId();
}

bool EvaluateXPathPredicate(
    const XPathContext& context,
    const XPathStep::Predicate& predicate,
    const XmlNamespaceManager* namespaces,
    std::size_t position,
    std::size_t count) {
    if (predicate.kind == XPathStep::Predicate::Kind::And) {
        return std::all_of(predicate.operands.begin(), predicate.operands.end(), [&](const auto& operand) {
            return EvaluateXPathPredicate(context, operand, namespaces, position, count);
        });
    }

    if (predicate.kind == XPathStep::Predicate::Kind::Or) {
        return std::any_of(predicate.operands.begin(), predicate.operands.end(), [&](const auto& operand) {
            return EvaluateXPathPredicate(context, operand, namespaces, position, count);
        });
    }

    if (predicate.kind == XPathStep::Predicate::Kind::Not) {
        return predicate.operands.size() == 1
            && !EvaluateXPathPredicate(context, predicate.operands.front(), namespaces, position, count);
    }

    if (predicate.kind == XPathStep::Predicate::Kind::True) {
        return true;
    }

    if (predicate.kind == XPathStep::Predicate::Kind::False) {
        return false;
    }

    if (predicate.kind == XPathStep::Predicate::Kind::Last) {
        return count != 0 && position == count;
    }

    if (predicate.kind == XPathStep::Predicate::Kind::PositionEquals) {
        return predicate.position != 0 && position == predicate.position;
    }

    if (context.node == nullptr) {
        return false;
    }

    if (predicate.kind == XPathStep::Predicate::Kind::Exists) {
        const XmlNode* matched = nullptr;
        if (TryMatchSimpleXPathSelfExpression(*context.node, predicate.source, namespaces, matched)) {
            return matched != nullptr;
        }
    }

    const auto values = ExtractXPathPredicateTargetValues(*context.node, predicate.target, namespaces, position, count);
    if (predicate.kind == XPathStep::Predicate::Kind::Exists) {
        if (IsXPathNumericTargetKind(predicate.target)) {
            const double numericValue = CoerceXPathTargetValuesToNumber(predicate.target, values);
            return !std::isnan(numericValue) && numericValue == static_cast<double>(position);
        }

        return CoerceXPathTargetValuesToBoolean(predicate.target, values);
    }

    const auto comparisonValues = predicate.hasComparisonTarget
        ? ExtractXPathPredicateTargetValues(*context.node, predicate.comparisonTarget, namespaces, position, count)
        : std::vector<std::string>{};

    if (predicate.kind == XPathStep::Predicate::Kind::Equals) {
        if (IsXPathBooleanTargetKind(predicate.target.kind) || IsXPathBooleanTargetKind(predicate.comparisonTarget.kind)) {
            return CoerceXPathTargetValuesToBoolean(predicate.target, values)
                == CoerceXPathTargetValuesToBoolean(predicate.comparisonTarget, comparisonValues);
        }

        if (IsXPathNumericTargetKind(predicate.target) || IsXPathNumericTargetKind(predicate.comparisonTarget)) {
            return CompareXPathNumbers(values, comparisonValues, [](double left, double right) {
                return left == right;
            });
        }

        return CompareXPathStrings(values, comparisonValues, true);
    }

    if (predicate.kind == XPathStep::Predicate::Kind::NotEquals) {
        if (IsXPathBooleanTargetKind(predicate.target.kind) || IsXPathBooleanTargetKind(predicate.comparisonTarget.kind)) {
            return CoerceXPathTargetValuesToBoolean(predicate.target, values)
                != CoerceXPathTargetValuesToBoolean(predicate.comparisonTarget, comparisonValues);
        }

        if (IsXPathNumericTargetKind(predicate.target) || IsXPathNumericTargetKind(predicate.comparisonTarget)) {
            return CompareXPathNumbers(values, comparisonValues, [](double left, double right) {
                return left != right;
            });
        }

        return CompareXPathStrings(values, comparisonValues, false);
    }

    if (predicate.kind == XPathStep::Predicate::Kind::LessThan
        || predicate.kind == XPathStep::Predicate::Kind::LessThanOrEqual
        || predicate.kind == XPathStep::Predicate::Kind::GreaterThan
        || predicate.kind == XPathStep::Predicate::Kind::GreaterThanOrEqual) {
        return CompareXPathNumbers(values, comparisonValues, [&](double left, double right) {
            if (predicate.kind == XPathStep::Predicate::Kind::LessThan) {
                return left < right;
            }
            if (predicate.kind == XPathStep::Predicate::Kind::LessThanOrEqual) {
                return left <= right;
            }
            if (predicate.kind == XPathStep::Predicate::Kind::GreaterThan) {
                return left > right;
            }
            return left >= right;
        });
    }

    if (predicate.kind == XPathStep::Predicate::Kind::Contains) {
        const auto marker = CoerceXPathTargetValuesToString(predicate.comparisonTarget, comparisonValues);
        return std::any_of(values.begin(), values.end(), [&](const auto& value) {
            return value.find(marker) != std::string::npos;
        });
    }

    if (predicate.kind == XPathStep::Predicate::Kind::StartsWith) {
        const auto prefix = CoerceXPathTargetValuesToString(predicate.comparisonTarget, comparisonValues);
        return std::any_of(values.begin(), values.end(), [&](const auto& value) {
            return value.rfind(prefix, 0) == 0;
        });
    }

    if (predicate.kind == XPathStep::Predicate::Kind::EndsWith) {
        const auto suffix = CoerceXPathTargetValuesToString(predicate.comparisonTarget, comparisonValues);
        return std::any_of(values.begin(), values.end(), [&](const auto& value) {
            return value.size() >= suffix.size()
                && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        });
    }

    return false;
}

std::string ExtractXPathPredicate(std::string_view token, std::size_t& position) {
    if (position >= token.size() || token[position] != '[') {
        ThrowInvalidXPathPredicate(std::string(token));
    }

    int depth = 0;
    const auto start = position + 1;
    while (position < token.size()) {
        if (token[position] == '[') {
            ++depth;
        } else if (token[position] == ']') {
            --depth;
            if (depth == 0) {
                const auto value = std::string(token.substr(start, position - start));
                ++position;
                return value;
            }
        }
        ++position;
    }

    ThrowInvalidXPathPredicate(std::string(token));
}

XPathStep::Predicate ParseXPathPredicate(const std::string& predicate) {
    const std::string normalized = TrimAsciiWhitespace(predicate);
    return ParseXPathPredicateExpression(normalized);
}

XPathStep::Predicate ParseXPathPredicateExpression(std::string_view predicate) {
    const std::string predicateText(predicate);
    std::string normalized = TrimAsciiWhitespace(std::string(predicate));
    while (IsWrappedXPathExpression(normalized)) {
        normalized = TrimAsciiWhitespace(normalized.substr(1, normalized.size() - 2));
    }

    XPathStep::Predicate result;
    result.source = normalized;
    const auto orParts = SplitXPathExpressionTopLevel(normalized, " or ");
    if (!orParts.empty()) {
        result.kind = XPathStep::Predicate::Kind::Or;
        result.operands.reserve(orParts.size());
        for (const auto& part : orParts) {
            result.operands.push_back(ParseXPathPredicateExpression(part));
        }
        return result;
    }

    const auto andParts = SplitXPathExpressionTopLevel(normalized, " and ");
    if (!andParts.empty()) {
        result.kind = XPathStep::Predicate::Kind::And;
        result.operands.reserve(andParts.size());
        for (const auto& part : andParts) {
            result.operands.push_back(ParseXPathPredicateExpression(part));
        }
        return result;
    }

    if (const auto arguments = TryParseXPathFunctionCallArguments(normalized, "not"); arguments.has_value()) {
        if (arguments->size() != 1) {
            ThrowUnsupportedXPathFeature("predicate [" + predicateText + "]");
        }

        result.kind = XPathStep::Predicate::Kind::Not;
        result.operands.push_back(ParseXPathPredicateExpression((*arguments)[0]));
        return result;
    }

    if (normalized == "last()") {
        result.kind = XPathStep::Predicate::Kind::Last;
        return result;
    }

    if (normalized == "true()") {
        result.kind = XPathStep::Predicate::Kind::True;
        return result;
    }

    if (normalized == "false()") {
        result.kind = XPathStep::Predicate::Kind::False;
        return result;
    }

    if (!normalized.empty() && std::all_of(normalized.begin(), normalized.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) {
        result.kind = XPathStep::Predicate::Kind::PositionEquals;
        std::size_t pos = 0;
        for (char ch : normalized) {
            pos = pos * 10 + (ch - '0');
        }
        result.position = pos;
        return result;
    }

    if (normalized.rfind("contains(", 0) == 0
        || normalized.rfind("starts-with(", 0) == 0
        || normalized.rfind("ends-with(", 0) == 0) {
        return ParseXPathFunctionPredicate(normalized, predicateText);
    }

    std::size_t operatorPosition = FindXPathExpressionTopLevelOperator(normalized, "!=");
    if (operatorPosition != std::string::npos) {
        result.kind = XPathStep::Predicate::Kind::NotEquals;
        result.target = ParseXPathPredicateTarget(normalized.substr(0, operatorPosition), predicateText);
        result.comparisonTarget = ParseXPathPredicateTarget(normalized.substr(operatorPosition + 2), predicateText);
        result.hasComparisonTarget = true;
        return result;
    }

    operatorPosition = FindXPathExpressionTopLevelOperator(normalized, ">=");
    if (operatorPosition != std::string::npos) {
        result.kind = XPathStep::Predicate::Kind::GreaterThanOrEqual;
        result.target = ParseXPathPredicateTarget(normalized.substr(0, operatorPosition), predicateText);
        result.comparisonTarget = ParseXPathPredicateTarget(normalized.substr(operatorPosition + 2), predicateText);
        result.hasComparisonTarget = true;
        return result;
    }

    operatorPosition = FindXPathExpressionTopLevelOperator(normalized, "<=");
    if (operatorPosition != std::string::npos) {
        result.kind = XPathStep::Predicate::Kind::LessThanOrEqual;
        result.target = ParseXPathPredicateTarget(normalized.substr(0, operatorPosition), predicateText);
        result.comparisonTarget = ParseXPathPredicateTarget(normalized.substr(operatorPosition + 2), predicateText);
        result.hasComparisonTarget = true;
        return result;
    }

    operatorPosition = FindXPathExpressionTopLevelOperator(normalized, "=");
    if (operatorPosition != std::string::npos) {
        result.kind = XPathStep::Predicate::Kind::Equals;
        result.target = ParseXPathPredicateTarget(normalized.substr(0, operatorPosition), predicateText);
        result.comparisonTarget = ParseXPathPredicateTarget(normalized.substr(operatorPosition + 1), predicateText);
        result.hasComparisonTarget = true;
        return result;
    }

    operatorPosition = FindXPathExpressionTopLevelOperator(normalized, ">");
    if (operatorPosition != std::string::npos) {
        result.kind = XPathStep::Predicate::Kind::GreaterThan;
        result.target = ParseXPathPredicateTarget(normalized.substr(0, operatorPosition), predicateText);
        result.comparisonTarget = ParseXPathPredicateTarget(normalized.substr(operatorPosition + 1), predicateText);
        result.hasComparisonTarget = true;
        return result;
    }

    operatorPosition = FindXPathExpressionTopLevelOperator(normalized, "<");
    if (operatorPosition != std::string::npos) {
        result.kind = XPathStep::Predicate::Kind::LessThan;
        result.target = ParseXPathPredicateTarget(normalized.substr(0, operatorPosition), predicateText);
        result.comparisonTarget = ParseXPathPredicateTarget(normalized.substr(operatorPosition + 1), predicateText);
        result.hasComparisonTarget = true;
        return result;
    }

    result.kind = XPathStep::Predicate::Kind::Exists;
    result.target = ParseXPathPredicateTarget(normalized, predicateText);
    return result;
}

void ApplyXPathPredicates(
    std::vector<XPathContext>& results,
    const std::vector<XPathStep::Predicate>& predicates,
    const XmlNamespaceManager* namespaces) {
    std::vector<XPathContext> scratch;
    for (const auto& predicate : predicates) {
        scratch.clear();
        scratch.reserve(results.size());
        for (std::size_t index = 0; index < results.size(); ++index) {
            if (EvaluateXPathPredicate(results[index], predicate, namespaces, index + 1, results.size())) {
                scratch.push_back(results[index]);
            }
        }
        results.swap(scratch);
    }
}

std::vector<std::string_view> SplitXPathUnion(std::string_view xpath) {
    std::vector<std::string_view> parts;
    int bracketDepth = 0;
    char quote = '\0';
    std::size_t start = 0;

    for (std::size_t index = 0; index < xpath.size(); ++index) {
        const char ch = xpath[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }
        if (ch == '[') {
            ++bracketDepth;
            continue;
        }
        if (ch == ']') {
            --bracketDepth;
            continue;
        }
        if (ch == '(' ) {
            ++bracketDepth;
            continue;
        }
        if (ch == ')') {
            --bracketDepth;
            continue;
        }
        if (bracketDepth == 0 && ch == '|') {
            parts.push_back(TrimAsciiWhitespaceView(xpath.substr(start, index - start)));
            start = index + 1;
        }
    }
    parts.push_back(TrimAsciiWhitespaceView(xpath.substr(start)));
    return parts;
}

std::vector<XPathStep> ParseXPathSteps(std::string_view xpath, bool& absolutePath);

struct CompiledXPathBranch {
    std::string expression;
    bool absolutePath = false;
    std::vector<XPathStep> steps;
};

struct CompiledXPathExpression {
    std::vector<CompiledXPathBranch> branches;
};

CompiledXPathExpression CompileXPathExpression(const std::string& xpath) {
    std::string normalizedExpression = TrimAsciiWhitespace(xpath);
    while (IsWrappedXPathExpression(normalizedExpression)) {
        normalizedExpression = TrimAsciiWhitespace(normalizedExpression.substr(1, normalizedExpression.size() - 2));
    }

    const auto parts = SplitXPathUnion(normalizedExpression);
    CompiledXPathExpression compiled;
    compiled.branches.reserve(parts.size());
    for (const auto& part : parts) {
        std::string normalizedPart = TrimAsciiWhitespace(std::string(part));
        while (IsWrappedXPathExpression(normalizedPart)) {
            normalizedPart = TrimAsciiWhitespace(normalizedPart.substr(1, normalizedPart.size() - 2));
        }

        if (normalizedPart.empty()) {
            throw XmlException("XPath expression cannot be empty");
        }

        bool absolutePath = false;
        auto steps = ParseXPathSteps(normalizedPart, absolutePath);
        compiled.branches.push_back(CompiledXPathBranch{normalizedPart, absolutePath, std::move(steps)});
    }
    return compiled;
}

std::shared_ptr<const CompiledXPathExpression> GetCompiledXPathExpression(std::string_view xpath) {
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, std::shared_ptr<const CompiledXPathExpression>> cache;
    static constexpr std::size_t kMaxCachedXPathExpressions = 256;

    const std::string xpathKey(xpath);

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        const auto found = cache.find(xpathKey);
        if (found != cache.end()) {
            return found->second;
        }
    }

    auto compiled = std::make_shared<const CompiledXPathExpression>(CompileXPathExpression(xpathKey));

    std::lock_guard<std::mutex> lock(cacheMutex);
    const auto found = cache.find(xpathKey);
    if (found != cache.end()) {
        return found->second;
    }
    if (cache.size() >= kMaxCachedXPathExpressions) {
        cache.clear();
    }
    cache.emplace(xpathKey, compiled);
    return compiled;
}

const XmlNode* GetXPathDocumentOrderRoot(const XmlNode& node) {
    const XmlNode* root = &node;
    while (root->ParentNode() != nullptr) {
        root = root->ParentNode();
    }
    return root;
}

void BuildXPathDocumentOrderIndex(
    const XmlNode& node,
    std::unordered_map<const XmlNode*, std::size_t>& order,
    std::size_t& nextOrdinal) {
    order.emplace(&node, nextOrdinal++);

    if (node.NodeType() == XmlNodeType::Element) {
        for (const auto& attribute : static_cast<const XmlElement*>(&node)->Attributes()) {
            if (attribute != nullptr && IsXPathVisibleAttribute(*attribute)) {
                order.emplace(attribute.get(), nextOrdinal++);
            }
        }
    }

    for (const auto& child : node.ChildNodes()) {
        BuildXPathDocumentOrderIndex(*child, order, nextOrdinal);
    }
}

void SortXPathUnionNodesInDocumentOrder(std::vector<std::shared_ptr<XmlNode>>& nodes) {
    if (nodes.size() < 2) {
        return;
    }

    std::unordered_map<const XmlNode*, std::size_t> order;
    std::unordered_set<const XmlNode*> indexedRoots;
    std::size_t nextOrdinal = 0;
    for (const auto& node : nodes) {
        if (node == nullptr) {
            continue;
        }

        const XmlNode* root = GetXPathDocumentOrderRoot(*node);
        if (!indexedRoots.insert(root).second) {
            continue;
        }

        BuildXPathDocumentOrderIndex(*root, order, nextOrdinal);
    }

    std::stable_sort(nodes.begin(), nodes.end(), [&](const auto& left, const auto& right) {
        if (left == nullptr || right == nullptr) {
            return left != nullptr && right == nullptr;
        }

        const auto leftIt = order.find(left.get());
        const auto rightIt = order.find(right.get());
        if (leftIt == order.end() || rightIt == order.end()) {
            return left.get() < right.get();
        }

        return leftIt->second < rightIt->second;
    });
}

void SortXPathUnionNodesInDocumentOrder(std::vector<const XmlNode*>& nodes) {
    if (nodes.size() < 2) {
        return;
    }

    std::unordered_map<const XmlNode*, std::size_t> order;
    std::unordered_set<const XmlNode*> indexedRoots;
    std::size_t nextOrdinal = 0;
    for (const auto* node : nodes) {
        if (node == nullptr) {
            continue;
        }

        const XmlNode* root = GetXPathDocumentOrderRoot(*node);
        if (!indexedRoots.insert(root).second) {
            continue;
        }

        BuildXPathDocumentOrderIndex(*root, order, nextOrdinal);
    }

    std::stable_sort(nodes.begin(), nodes.end(), [&](const XmlNode* left, const XmlNode* right) {
        if (left == nullptr || right == nullptr) {
            return left != nullptr && right == nullptr;
        }

        const auto leftIt = order.find(left);
        const auto rightIt = order.find(right);
        if (leftIt == order.end() || rightIt == order.end()) {
            return left < right;
        }

        return leftIt->second < rightIt->second;
    });
}

void SortXPathContextsInDocumentOrder(std::vector<XPathContext>& contexts) {
    if (contexts.size() < 2) {
        return;
    }

    std::unordered_map<const XmlNode*, std::size_t> order;
    std::unordered_set<const XmlNode*> indexedRoots;
    std::size_t nextOrdinal = 0;
    for (const auto& context : contexts) {
        if (context.node == nullptr) {
            continue;
        }

        const XmlNode* root = GetXPathDocumentOrderRoot(*context.node);
        if (!indexedRoots.insert(root).second) {
            continue;
        }

        BuildXPathDocumentOrderIndex(*root, order, nextOrdinal);
    }

    std::stable_sort(contexts.begin(), contexts.end(), [&](const XPathContext& left, const XPathContext& right) {
        const auto leftIt = left.node == nullptr ? order.end() : order.find(left.node);
        const auto rightIt = right.node == nullptr ? order.end() : order.find(right.node);
        const std::size_t leftOrdinal = leftIt == order.end() ? std::numeric_limits<std::size_t>::max() : leftIt->second;
        const std::size_t rightOrdinal = rightIt == order.end() ? std::numeric_limits<std::size_t>::max() : rightIt->second;
        return leftOrdinal < rightOrdinal;
    });
}

std::vector<XPathStep> ParseXPathSteps(std::string_view xpath, bool& absolutePath) {
    if (xpath.empty()) {
        throw XmlException("XPath expression cannot be empty");
    }

    std::vector<XPathStep> steps;
    std::size_t position = 0;
    absolutePath = false;

    if (xpath[position] == '/') {
        absolutePath = true;
    }

    while (position < xpath.size()) {
        XPathStep step;
        if (xpath[position] == '/') {
            if (position + 1 < xpath.size() && xpath[position + 1] == '/') {
                step.descendant = true;
                position += 2;
            } else {
                ++position;
            }
        }

        if (position >= xpath.size()) {
            break;
        }

        const auto tokenStart = position;
        int bracketDepth = 0;
        while (position < xpath.size()) {
            const char ch = xpath[position];
            if (ch == '[') {
                ++bracketDepth;
            } else if (ch == ']') {
                --bracketDepth;
            } else if (ch == '/' && bracketDepth == 0) {
                break;
            }
            ++position;
        }

        std::string token(xpath.substr(tokenStart, position - tokenStart));
        if (token.empty()) {
            continue;
        }

        if (token == ".") {
            step.self = true;
            step.axis = XPathStep::Axis::Self;
            steps.push_back(std::move(step));
            continue;
        }

        if (token == "..") {
            step.axis = XPathStep::Axis::Parent;
            step.nodeTest = true;
            steps.push_back(std::move(step));
            continue;
        }

        if (token[0] == '@') {
            const auto predicateStart = token.find('[');
            const std::string stepName = predicateStart == std::string::npos ? token : token.substr(0, predicateStart);
            step.attribute = true;
            step.axis = XPathStep::Axis::Attribute;
            step.name = ParseXPathName(stepName, true);
            if (predicateStart != std::string::npos) {
                std::size_t predicatePosition = predicateStart;
                while (predicatePosition < token.size()) {
                    if (token[predicatePosition] != '[') {
                        ThrowInvalidXPathPredicate(token);
                    }
                    const auto predicate = ExtractXPathPredicate(token, predicatePosition);
                    step.predicates.push_back(ParseXPathPredicate(predicate));
                }
            }
            steps.push_back(std::move(step));
            continue;
        }

        const auto predicateStart = token.find('[');
        std::string stepName = predicateStart == std::string::npos ? token : token.substr(0, predicateStart);

        // Parse explicit axis specifiers
        const auto axisDelimiter = stepName.find("::");
        if (axisDelimiter != std::string::npos) {
            const std::string axisName = stepName.substr(0, axisDelimiter);
            std::string nodeTestPart = stepName.substr(axisDelimiter + 2);

            if (axisName == "child") {
                step.axis = XPathStep::Axis::Child;
            } else if (axisName == "descendant") {
                step.axis = XPathStep::Axis::Descendant;
                step.descendant = true;
            } else if (axisName == "descendant-or-self") {
                step.axis = XPathStep::Axis::DescendantOrSelf;
                step.descendant = true;
            } else if (axisName == "parent") {
                step.axis = XPathStep::Axis::Parent;
            } else if (axisName == "ancestor") {
                step.axis = XPathStep::Axis::Ancestor;
            } else if (axisName == "ancestor-or-self") {
                step.axis = XPathStep::Axis::AncestorOrSelf;
            } else if (axisName == "following-sibling") {
                step.axis = XPathStep::Axis::FollowingSibling;
            } else if (axisName == "preceding-sibling") {
                step.axis = XPathStep::Axis::PrecedingSibling;
            } else if (axisName == "following") {
                step.axis = XPathStep::Axis::Following;
            } else if (axisName == "preceding") {
                step.axis = XPathStep::Axis::Preceding;
            } else if (axisName == "self") {
                step.axis = XPathStep::Axis::Self;
                // Don't set step.self=true; the axis + name filter handles self::name
            } else if (axisName == "attribute") {
                step.axis = XPathStep::Axis::Attribute;
                step.attribute = true;
            } else if (axisName == "namespace") {
                step.axis = XPathStep::Axis::Namespace;
            } else {
                ThrowUnsupportedXPathFeature("axis [" + axisName + "]");
            }

            stepName = nodeTestPart;
        }

        if (stepName == "node()") {
            step.nodeTest = true;
        } else if (stepName == "text()") {
            step.textNode = true;
        } else if (stepName == "comment()") {
            step.commentNode = true;
        } else if (TryParseXPathProcessingInstructionNodeTest(stepName, step.processingInstructionTarget)) {
            step.processingInstructionNode = true;
        } else if (step.attribute || step.axis == XPathStep::Axis::Attribute) {
            step.name = ParseXPathName(stepName, true);
        } else {
            step.name = ParseXPathName(stepName, false);
        }

        if (predicateStart != std::string::npos) {
            std::size_t predicatePosition = predicateStart;
            while (predicatePosition < token.size()) {
                if (token[predicatePosition] != '[') {
                    ThrowInvalidXPathPredicate(token);
                }
                const auto predicate = ExtractXPathPredicate(token, predicatePosition);
                step.predicates.push_back(ParseXPathPredicate(predicate));
            }
        }

        steps.push_back(std::move(step));
    }

    return steps;
}

bool MatchesXPathElement(const XmlNode& node, const XPathStep& step, const XmlNamespaceManager* namespaces) {
    if (step.nodeTest) {
        return IsXPathVisibleNode(node);
    }
    if (step.textNode) {
        return IsXPathTextNode(node);
    }
    if (step.commentNode) {
        return node.NodeType() == XmlNodeType::Comment;
    }
    if (step.processingInstructionNode) {
        return node.NodeType() == XmlNodeType::ProcessingInstruction
            && (step.processingInstructionTarget.empty() || node.Name() == step.processingInstructionTarget);
    }
    return node.NodeType() == XmlNodeType::Element && MatchesXPathQualifiedName(node, step.name, namespaces);
}

bool MatchesXPathAttribute(const XmlAttribute& attribute, const XPathStep& step, const XmlNamespaceManager* namespaces) {
    if (!IsXPathVisibleAttribute(attribute)) {
        return false;
    }

    if (step.nodeTest) {
        return true;
    }

    if (step.textNode || step.commentNode || step.processingInstructionNode) {
        return false;
    }

    return MatchesXPathQualifiedName(attribute, step.name, namespaces);
}

void CollectDescendantElementContexts(const XmlNode& node, std::vector<XPathContext>& contexts) {
    for (const auto& child : node.ChildNodes()) {
        contexts.push_back({child.get()});
        CollectDescendantElementContexts(*child, contexts);
    }
}

void CollectMatchingDescendantElementContexts(
    const XmlNode& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    for (const auto& child : node.ChildNodes()) {
        if (MatchesXPathElement(*child, step, namespaces)) {
            contexts.push_back({child.get()});
        }
        CollectMatchingDescendantElementContexts(*child, step, namespaces, contexts);
    }
}

void CollectMatchingNodeAndDescendants(
    const std::shared_ptr<XmlNode>& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    if (node == nullptr) {
        return;
    }

    if (MatchesXPathElement(*node, step, namespaces)) {
        contexts.push_back({node.get()});
    }

    CollectMatchingDescendantElementContexts(*node, step, namespaces, contexts);
}

void CollectMatchingAttributesFromElement(
    const XmlElement& element,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    for (const auto& attribute : element.Attributes()) {
        if (attribute != nullptr
            && IsXPathVisibleAttribute(*attribute)
            && MatchesXPathAttribute(*attribute, step, namespaces)) {
            contexts.push_back({attribute.get()});
        }
    }
}

void CollectMatchingNamespaceContexts(
    const XmlNode& node,
    const XPathStep& step,
    std::vector<XPathContext>& contexts) {
    if (node.NodeType() != XmlNodeType::Element) {
        return;
    }

    std::unordered_set<std::string> seenPrefixes;
    auto collectFromElement = [&](const XmlElement& element) {
        for (const auto& attribute : element.Attributes()) {
            if (attribute == nullptr || !IsXPathNamespaceDeclarationAttribute(*attribute)) {
                continue;
            }

            const std::string prefix(NamespaceDeclarationPrefixView(attribute->Name()));
            if (!seenPrefixes.insert(prefix).second) {
                continue;
            }

            if (MatchesXPathNamespace(*attribute, step)) {
                contexts.push_back({attribute.get(), &node});
            }
        }
    };

    auto current = static_cast<const XmlElement*>(&node);
    while (current != nullptr) {
        collectFromElement(*current);

        const auto* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }
}

void CollectMatchingDescendantAttributeContexts(
    const XmlNode& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    for (const auto& child : node.ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Element) {
            CollectMatchingAttributesFromElement(*static_cast<const XmlElement*>(child.get()), step, namespaces, contexts);
        }
        CollectMatchingDescendantAttributeContexts(*child, step, namespaces, contexts);
    }
}

void CollectFollowingSiblingContexts(const XmlNode& node, std::vector<XPathContext>& contexts) {
    if (node.NodeType() == XmlNodeType::Attribute) {
        return;
    }

    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }
    const auto& siblings = parent->ChildNodes();
    bool found = false;
    for (const auto& sibling : siblings) {
        if (found) {
            contexts.push_back({sibling.get()});
        } else if (sibling.get() == &node) {
            found = true;
        }
    }
}

void CollectMatchingFollowingSiblingContexts(
    const XmlNode& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    if (node.NodeType() == XmlNodeType::Attribute) {
        return;
    }

    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }

    const auto& siblings = parent->ChildNodes();
    bool found = false;
    for (const auto& sibling : siblings) {
        if (found) {
            if (MatchesXPathElement(*sibling, step, namespaces)) {
                contexts.push_back({sibling.get()});
            }
        } else if (sibling.get() == &node) {
            found = true;
        }
    }
}

void CollectPrecedingSiblingContexts(const XmlNode& node, std::vector<XPathContext>& contexts) {
    if (node.NodeType() == XmlNodeType::Attribute) {
        return;
    }

    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }

    const auto& siblings = parent->ChildNodes();
    std::size_t nodeIndex = siblings.size();
    for (std::size_t index = 0; index < siblings.size(); ++index) {
        if (siblings[index].get() == &node) {
            nodeIndex = index;
            break;
        }
    }

    if (nodeIndex == siblings.size()) {
        return;
    }

    // XPath preceding-sibling returns in reverse document order.
    for (std::size_t index = nodeIndex; index > 0; --index) {
        const auto& sibling = siblings[index - 1];
        contexts.push_back({sibling.get()});
    }
}

void CollectMatchingPrecedingSiblingContexts(
    const XmlNode& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    if (node.NodeType() == XmlNodeType::Attribute) {
        return;
    }

    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }

    const auto& siblings = parent->ChildNodes();
    std::size_t nodeIndex = siblings.size();
    for (std::size_t index = 0; index < siblings.size(); ++index) {
        if (siblings[index].get() == &node) {
            nodeIndex = index;
            break;
        }
    }

    if (nodeIndex == siblings.size()) {
        return;
    }

    for (std::size_t index = nodeIndex; index > 0; --index) {
        const auto& sibling = siblings[index - 1];
        if (MatchesXPathElement(*sibling, step, namespaces)) {
            contexts.push_back({sibling.get()});
        }
    }
}

void CollectFollowingContexts(const XPathContext& context, std::vector<XPathContext>& contexts) {
    if (context.node == nullptr) {
        return;
    }

    // Following axis: all nodes after the current node in document order,
    // excluding descendants.
    if (context.node->NodeType() == XmlNodeType::Attribute) {
        const XmlNode* parent = GetXPathContextParent(context);
        if (parent == nullptr) {
            return;
        }

        CollectDescendantElementContexts(*parent, contexts);

        const XmlNode* current = parent;
        while (current != nullptr) {
            const XmlNode* ancestorParent = current->ParentNode();
            if (ancestorParent == nullptr) {
                break;
            }
            const auto& siblings = ancestorParent->ChildNodes();
            bool found = false;
            for (const auto& sibling : siblings) {
                if (found) {
                    contexts.push_back({sibling.get()});
                    CollectDescendantElementContexts(*sibling, contexts);
                } else if (sibling.get() == current) {
                    found = true;
                }
            }
            current = ancestorParent;
        }
        return;
    }

    const XmlNode* current = context.node;
    while (current != nullptr) {
        const XmlNode* parent = current->ParentNode();
        if (parent == nullptr) {
            break;
        }
        const auto& siblings = parent->ChildNodes();
        bool found = false;
        for (const auto& sibling : siblings) {
            if (found) {
                contexts.push_back({sibling.get()});
                CollectDescendantElementContexts(*sibling, contexts);
            } else if (sibling.get() == current) {
                found = true;
            }
        }
        current = parent;
    }
}

void CollectFollowingContexts(const XmlNode& node, std::vector<XPathContext>& contexts) {
    CollectFollowingContexts(XPathContext{&node}, contexts);
}

void CollectMatchingFollowingContexts(
    const XPathContext& context,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    if (context.node == nullptr) {
        return;
    }

    if (context.node->NodeType() == XmlNodeType::Attribute) {
        const XmlNode* parent = GetXPathContextParent(context);
        if (parent == nullptr) {
            return;
        }

        CollectMatchingDescendantElementContexts(*parent, step, namespaces, contexts);

        const XmlNode* current = parent;
        while (current != nullptr) {
            const XmlNode* ancestorParent = current->ParentNode();
            if (ancestorParent == nullptr) {
                break;
            }
            const auto& siblings = ancestorParent->ChildNodes();
            bool found = false;
            for (const auto& sibling : siblings) {
                if (found) {
                    CollectMatchingNodeAndDescendants(sibling, step, namespaces, contexts);
                } else if (sibling.get() == current) {
                    found = true;
                }
            }
            current = ancestorParent;
        }
        return;
    }

    const XmlNode* current = context.node;
    while (current != nullptr) {
        const XmlNode* parent = current->ParentNode();
        if (parent == nullptr) {
            break;
        }
        const auto& siblings = parent->ChildNodes();
        bool found = false;
        for (const auto& sibling : siblings) {
            if (found) {
                CollectMatchingNodeAndDescendants(sibling, step, namespaces, contexts);
            } else if (sibling.get() == current) {
                found = true;
            }
        }
        current = parent;
    }
}

void CollectMatchingFollowingContexts(
    const XmlNode& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    CollectMatchingFollowingContexts(XPathContext{&node}, step, namespaces, contexts);
}

void CollectPrecedingContexts(const XPathContext& context, std::vector<XPathContext>& contexts) {
    if (context.node == nullptr) {
        return;
    }

    // Preceding axis: all nodes before the current node in reverse document order,
    // excluding ancestors.
    if (context.node->NodeType() == XmlNodeType::Attribute) {
        const XmlNode* parent = GetXPathContextParent(context);
        if (parent != nullptr) {
            CollectPrecedingContexts(*parent, contexts);
        }
        return;
    }

    if (context.node->ParentNode() == nullptr) {
        return;
    }

    std::unordered_set<const XmlNode*> ancestors;
    for (const XmlNode* ancestor = context.node->ParentNode(); ancestor != nullptr; ancestor = ancestor->ParentNode()) {
        ancestors.insert(ancestor);
    }

    // Collect preceding candidates directly in document order, then reverse once
    const XmlNode* root = context.node;
    while (root->ParentNode() != nullptr) {
        root = root->ParentNode();
    }

    std::vector<XPathContext> preceding;
    for (const auto& child : root->ChildNodes()) {
        if (CollectPrecedingCandidates(child, *context.node, ancestors, preceding)) {
            break;
        }
    }

    // Reverse for preceding axis (reverse document order)
    for (auto reverseIndex = preceding.size(); reverseIndex > 0; --reverseIndex) {
        contexts.push_back(preceding[reverseIndex - 1]);
    }
}

void CollectPrecedingContexts(const XmlNode& node, std::vector<XPathContext>& contexts) {
    CollectPrecedingContexts(XPathContext{&node}, contexts);
}

void CollectMatchingPrecedingContexts(
    const XPathContext& context,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    if (context.node == nullptr) {
        return;
    }

    if (context.node->NodeType() == XmlNodeType::Attribute) {
        const XmlNode* parent = GetXPathContextParent(context);
        if (parent != nullptr) {
            CollectMatchingPrecedingContexts(XPathContext{parent}, step, namespaces, contexts);
        }
        return;
    }

    if (context.node->ParentNode() == nullptr) {
        return;
    }

    std::unordered_set<const XmlNode*> ancestors;
    for (const XmlNode* ancestor = context.node->ParentNode(); ancestor != nullptr; ancestor = ancestor->ParentNode()) {
        ancestors.insert(ancestor);
    }

    const XmlNode* root = context.node;
    while (root->ParentNode() != nullptr) {
        root = root->ParentNode();
    }

    std::vector<XPathContext> preceding;
    for (const auto& child : root->ChildNodes()) {
        if (CollectMatchingPrecedingCandidates(child, *context.node, ancestors, step, namespaces, preceding)) {
            break;
        }
    }

    for (auto reverseIndex = preceding.size(); reverseIndex > 0; --reverseIndex) {
        contexts.push_back(preceding[reverseIndex - 1]);
    }
}

void CollectMatchingPrecedingContexts(
    const XmlNode& node,
    const XPathStep& step,
    const XmlNamespaceManager* namespaces,
    std::vector<XPathContext>& contexts) {
    CollectMatchingPrecedingContexts(XPathContext{&node}, step, namespaces, contexts);
}

std::vector<XPathContext> ApplyXPathStep(const std::vector<XPathContext>& current, const XPathStep& step, const XmlNamespaceManager* namespaces) {
    std::vector<XPathContext> results;

    for (const auto& context : current) {
        if (context.node == nullptr) {
            continue;
        }

        std::vector<XPathContext> contextResults;
        auto appendContextResults = [&](std::vector<XPathContext>& values) {
            ApplyXPathPredicates(values, step.predicates, namespaces);
            for (auto& value : values) {
                results.push_back(std::move(value));
            }
        };

        if (step.self || (step.axis == XPathStep::Axis::Self
                && !step.nodeTest
                && !step.textNode
                && !step.commentNode
                && !step.processingInstructionNode
                && step.name.name.empty())) {
            if (context.node != nullptr) {
                contextResults.push_back(context);
            }
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::Self) {
            if (context.node != nullptr && MatchesXPathElement(*context.node, step, namespaces)) {
                contextResults.push_back(context);
            }
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::Parent) {
            const XmlNode* parent = GetXPathContextParent(context);
            if (parent != nullptr) {
                if (MatchesXPathElement(*parent, step, namespaces)) {
                    contextResults.push_back({parent});
                }
            }
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::Ancestor || step.axis == XPathStep::Axis::AncestorOrSelf) {
            if (step.axis == XPathStep::Axis::AncestorOrSelf && context.node != nullptr) {
                if (MatchesXPathElement(*context.node, step, namespaces)) {
                    contextResults.push_back(context);
                }
            }
            std::vector<XPathContext> ancestors;
            CollectAncestorContexts(context, ancestors);
            for (const auto& ancestor : ancestors) {
                if (MatchesXPathElement(*ancestor.node, step, namespaces)) {
                    contextResults.push_back(ancestor);
                }
            }
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::FollowingSibling) {
            CollectMatchingFollowingSiblingContexts(*context.node, step, namespaces, contextResults);
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::PrecedingSibling) {
            CollectMatchingPrecedingSiblingContexts(*context.node, step, namespaces, contextResults);
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::Following) {
            CollectMatchingFollowingContexts(context, step, namespaces, contextResults);
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::Preceding) {
            CollectMatchingPrecedingContexts(context, step, namespaces, contextResults);
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::Namespace) {
            CollectMatchingNamespaceContexts(*context.node, step, contextResults);
            appendContextResults(contextResults);
            continue;
        }

        if (step.attribute) {
            if (step.descendant) {
                CollectMatchingDescendantAttributeContexts(*context.node, step, namespaces, contextResults);
            } else if (context.node->NodeType() == XmlNodeType::Element) {
                CollectMatchingAttributesFromElement(*static_cast<const XmlElement*>(context.node), step, namespaces, contextResults);
            }
            appendContextResults(contextResults);
            continue;
        }

        if (step.axis == XPathStep::Axis::DescendantOrSelf
            && context.node != nullptr
            && MatchesXPathElement(*context.node, step, namespaces)) {
            contextResults.push_back(context);
        }

        if (step.descendant) {
            CollectMatchingDescendantElementContexts(*context.node, step, namespaces, contextResults);
        } else {
            for (const auto& child : context.node->ChildNodes()) {
                if (MatchesXPathElement(*child, step, namespaces)) {
                    contextResults.push_back({child.get()});
                }
            }
        }

        appendContextResults(contextResults);
    }

    // Deduplicate results (XPath node-sets do not contain duplicates)
    if (step.axis == XPathStep::Axis::Parent
        || step.axis == XPathStep::Axis::Ancestor
        || step.axis == XPathStep::Axis::AncestorOrSelf
        || step.axis == XPathStep::Axis::FollowingSibling
        || step.axis == XPathStep::Axis::PrecedingSibling
        || step.axis == XPathStep::Axis::Following
        || step.axis == XPathStep::Axis::Preceding) {
        std::unordered_set<const XmlNode*> seen;
        std::vector<XPathContext> unique;
        unique.reserve(results.size());
        for (auto& context : results) {
            if (seen.insert(context.node).second) {
                unique.push_back(std::move(context));
            }
        }
        results = std::move(unique);
    }

    SortXPathContextsInDocumentOrder(results);

    return results;
}

std::vector<XPathContext> FilterXPathStepMatches(const std::vector<XPathContext>& current, const XPathStep& step, const XmlNamespaceManager* namespaces) {
    std::vector<XPathContext> results;
    for (const auto& context : current) {
        if (context.node == nullptr) {
            continue;
        }

        if (step.self) {
            results.push_back(context);
            continue;
        }

        if (step.attribute) {
            if (context.node->NodeType() != XmlNodeType::Element) {
                continue;
            }
            for (const auto& attribute : static_cast<const XmlElement*>(context.node)->Attributes()) {
                if (MatchesXPathAttribute(*attribute, step, namespaces)) {
                    results.push_back({attribute.get()});
                }
            }
            continue;
        }

        if (MatchesXPathElement(*context.node, step, namespaces)) {
            results.push_back(context);
        }
    }

    ApplyXPathPredicates(results, step.predicates, namespaces);

    return results;
}

XmlNodeList EvaluateCompiledXPathSingleFromDocument(
    const XmlDocument& document,
    const CompiledXPathBranch& compiled,
    const XmlNamespaceManager* namespaces) {
    if (compiled.steps.empty()) {
        if (compiled.absolutePath) {
            return CreateXPathNodeListFromSingleNode(&document);
        }
        return {};
    }

    std::vector<XPathContext> current;
    std::size_t stepIndex = 0;

    if (!compiled.absolutePath) {
        current.push_back({&document});
    } else {
        current.push_back({&document});
        current = ApplyXPathStep(current, compiled.steps.front(), namespaces);
        stepIndex = 1;
    }

    for (; stepIndex < compiled.steps.size(); ++stepIndex) {
        current = ApplyXPathStep(current, compiled.steps[stepIndex], namespaces);
        if (current.empty()) {
            break;
        }
    }

    return CreateXPathNodeListFromContexts(current);
}

XmlNodeList EvaluateXPathSingleFromDocument(const XmlDocument& document, std::string_view xpath, const XmlNamespaceManager* namespaces) {
    const auto compiled = GetCompiledXPathExpression(xpath);
    return EvaluateCompiledXPathSingleFromDocument(document, compiled->branches.front(), namespaces);
}

XmlNodeList EvaluateXPathFromDocument(const XmlDocument& document, std::string_view xpath, const XmlNamespaceManager* namespaces) {
    const auto compiled = GetCompiledXPathExpression(xpath);
    if (compiled->branches.size() == 1) {
        return EvaluateCompiledXPathSingleFromDocument(document, compiled->branches.front(), namespaces);
    }

    std::vector<const XmlNode*> merged;
    std::unordered_set<const XmlNode*> seen;
    for (const auto& branch : compiled->branches) {
        const auto partResult = EvaluateCompiledXPathSingleFromDocument(document, branch, namespaces);
        for (const auto& node : partResult) {
            if (node != nullptr && seen.insert(node.get()).second) {
                merged.push_back(node.get());
            }
        }
    }
    SortXPathUnionNodesInDocumentOrder(merged);
    return CreateXPathNodeListFromRawNodes(merged);
}

XmlNodeList EvaluateXPathSingleFromElement(const XmlElement& element, std::string_view xpath, const XmlNamespaceManager* namespaces);

XmlNodeList EvaluateXPathFromNode(const XmlNode& node, std::string_view xpath, const XmlNamespaceManager* namespaces);

XmlNodeList EvaluateCompiledXPathSingleFromElement(
    const XmlElement& element,
    const CompiledXPathBranch& compiled,
    const XmlNamespaceManager* namespaces) {
    if (compiled.steps.empty()) {
        if (compiled.absolutePath) {
            if (element.OwnerDocument() != nullptr) {
                return CreateXPathNodeListFromSingleNode(element.OwnerDocument());
            }

            return CreateXPathNodeListFromSingleNode(FindTopElement(element));
        }
        return {};
    }

    std::vector<XPathContext> current;
    std::size_t stepIndex = 0;

    if (compiled.absolutePath) {
        if (element.OwnerDocument() != nullptr) {
            return EvaluateCompiledXPathSingleFromDocument(*element.OwnerDocument(), compiled, namespaces);
        }

        const auto* topElement = FindTopElement(element);
        auto sharedTop = FindSharedNode(*topElement);
        if (sharedTop == nullptr) {
            return {};
        }
        current.push_back({topElement});
        if (compiled.steps.front().descendant) {
            current = ApplyXPathStep(current, compiled.steps.front(), namespaces);
        } else {
            current = FilterXPathStepMatches(current, compiled.steps.front(), namespaces);
        }
        stepIndex = 1;
    } else {
        current.push_back({&element});
    }

    for (; stepIndex < compiled.steps.size(); ++stepIndex) {
        current = ApplyXPathStep(current, compiled.steps[stepIndex], namespaces);
        if (current.empty()) {
            break;
        }
    }

    return CreateXPathNodeListFromContexts(current);
}

XmlNodeList EvaluateXPathSingleFromElement(const XmlElement& element, std::string_view xpath, const XmlNamespaceManager* namespaces) {
    const auto compiled = GetCompiledXPathExpression(xpath);
    return EvaluateCompiledXPathSingleFromElement(element, compiled->branches.front(), namespaces);
}

XmlNodeList EvaluateXPathFromElement(const XmlElement& element, std::string_view xpath, const XmlNamespaceManager* namespaces) {
    const auto compiled = GetCompiledXPathExpression(xpath);
    if (compiled->branches.size() == 1) {
        return EvaluateCompiledXPathSingleFromElement(element, compiled->branches.front(), namespaces);
    }

    std::vector<const XmlNode*> merged;
    std::unordered_set<const XmlNode*> seen;
    for (const auto& branch : compiled->branches) {
        const auto partResult = EvaluateCompiledXPathSingleFromElement(element, branch, namespaces);
        for (const auto& node : partResult) {
            if (node != nullptr && seen.insert(node.get()).second) {
                merged.push_back(node.get());
            }
        }
    }
    SortXPathUnionNodesInDocumentOrder(merged);
    return CreateXPathNodeListFromRawNodes(merged);
}

XmlNodeList EvaluateCompiledXPathSingleFromNode(
    const XmlNode& node,
    const CompiledXPathBranch& compiled,
    const XmlNamespaceManager* namespaces) {
    if (node.NodeType() == XmlNodeType::Document) {
        return EvaluateCompiledXPathSingleFromDocument(static_cast<const XmlDocument&>(node), compiled, namespaces);
    }

    if (node.NodeType() == XmlNodeType::Element) {
        return EvaluateCompiledXPathSingleFromElement(static_cast<const XmlElement&>(node), compiled, namespaces);
    }

    if (compiled.steps.empty()) {
        if (compiled.absolutePath) {
            if (node.OwnerDocument() != nullptr) {
                return CreateXPathNodeListFromSingleNode(node.OwnerDocument());
            }

            const XmlNode* top = &node;
            while (top->ParentNode() != nullptr) {
                top = top->ParentNode();
            }
            return CreateXPathNodeListFromSingleNode(top);
        }
        return {};
    }

    std::vector<XPathContext> current;
    std::size_t stepIndex = 0;

    if (compiled.absolutePath) {
        if (node.OwnerDocument() != nullptr) {
            return EvaluateCompiledXPathSingleFromDocument(*node.OwnerDocument(), compiled, namespaces);
        }

        const XmlNode* top = &node;
        while (top->ParentNode() != nullptr) {
            top = top->ParentNode();
        }

        auto sharedTop = GetXPathResultSharedNode(*top);
        if (sharedTop == nullptr) {
            return {};
        }

        current.push_back({top});
        if (compiled.steps.front().descendant) {
            current = ApplyXPathStep(current, compiled.steps.front(), namespaces);
        } else {
            current = FilterXPathStepMatches(current, compiled.steps.front(), namespaces);
        }
        stepIndex = 1;
    } else {
        auto shared = GetXPathResultSharedNode(node);
        if (shared == nullptr) {
            return {};
        }
        current.push_back({&node});
    }

    for (; stepIndex < compiled.steps.size(); ++stepIndex) {
        current = ApplyXPathStep(current, compiled.steps[stepIndex], namespaces);
        if (current.empty()) {
            break;
        }
    }

    return CreateXPathNodeListFromContexts(current);
}

XmlNodeList EvaluateXPathFromNode(const XmlNode& node, std::string_view xpath, const XmlNamespaceManager* namespaces) {
    const auto compiled = GetCompiledXPathExpression(xpath);
    if (compiled->branches.size() == 1) {
        return EvaluateCompiledXPathSingleFromNode(node, compiled->branches.front(), namespaces);
    }

    std::vector<const XmlNode*> merged;
    std::unordered_set<const XmlNode*> seen;
    for (const auto& branch : compiled->branches) {
        const auto partResult = EvaluateCompiledXPathSingleFromNode(node, branch, namespaces);
        for (const auto& match : partResult) {
            if (match != nullptr && seen.insert(match.get()).second) {
                merged.push_back(match.get());
            }
        }
    }
    SortXPathUnionNodesInDocumentOrder(merged);
    return CreateXPathNodeListFromRawNodes(merged);
}

std::vector<XPathContext> EvaluateCompiledXPathContextsFromContext(
    const XPathContext& start,
    const CompiledXPathBranch& compiled,
    const XmlNamespaceManager* namespaces) {
    if (start.node == nullptr) {
        return {};
    }

    if (compiled.steps.empty()) {
        if (compiled.absolutePath) {
            const XmlNode* root = start.namespaceParent != nullptr ? start.namespaceParent : start.node;
            while (root != nullptr && root->ParentNode() != nullptr) {
                root = root->ParentNode();
            }
            return root == nullptr ? std::vector<XPathContext>{} : std::vector<XPathContext>{{root}};
        }

        return {};
    }

    std::vector<XPathContext> current;
    std::size_t stepIndex = 0;

    if (compiled.absolutePath) {
        bool startsFromDocument = false;
        if (start.node->OwnerDocument() != nullptr) {
            current.push_back({start.node->OwnerDocument()});
            startsFromDocument = true;
        } else {
            const XmlNode* top = start.namespaceParent != nullptr ? start.namespaceParent : start.node;
            while (top->ParentNode() != nullptr) {
                top = top->ParentNode();
            }
            current.push_back({top});
            startsFromDocument = top != nullptr && top->NodeType() == XmlNodeType::Document;
        }

        if (startsFromDocument || compiled.steps.front().descendant) {
            current = ApplyXPathStep(current, compiled.steps.front(), namespaces);
        } else {
            current = FilterXPathStepMatches(current, compiled.steps.front(), namespaces);
        }
        stepIndex = 1;
    } else {
        current.push_back(start);
    }

    for (; stepIndex < compiled.steps.size(); ++stepIndex) {
        current = ApplyXPathStep(current, compiled.steps[stepIndex], namespaces);
        if (current.empty()) {
            break;
        }
    }

    return current;
}

std::vector<XPathContext> EvaluateXPathContextsFromContext(
    const XPathContext& start,
    std::string_view xpath,
    const XmlNamespaceManager* namespaces) {
    const auto compiled = GetCompiledXPathExpression(xpath);
    if (compiled->branches.size() == 1) {
        return EvaluateCompiledXPathContextsFromContext(start, compiled->branches.front(), namespaces);
    }

    std::vector<XPathContext> merged;
    std::unordered_set<const XmlNode*> seen;
    for (const auto& branch : compiled->branches) {
        auto part = EvaluateCompiledXPathContextsFromContext(start, branch, namespaces);
        for (auto& item : part) {
            if (item.node != nullptr && seen.insert(item.node).second) {
                merged.push_back(std::move(item));
            }
        }
    }

    SortXPathContextsInDocumentOrder(merged);
    return merged;
}

XPathNavigator::XPathNavigator(const XmlNode* node, const XmlNode* namespaceParent)
    : node_(node),
      namespaceParent_(namespaceParent) {
}

XPathValue::XPathValue(std::vector<XPathNavigator> nodeSet)
    : type_(XPathValueType::NodeSet),
      nodeSet_(std::move(nodeSet)) {
}

XPathValue::XPathValue(std::string stringValue)
    : type_(XPathValueType::String),
      stringValue_(std::move(stringValue)) {
}

XPathValue::XPathValue(double numberValue)
    : type_(XPathValueType::Number),
      numberValue_(numberValue) {
}

XPathValue::XPathValue(bool booleanValue)
    : type_(XPathValueType::Boolean),
      booleanValue_(booleanValue) {
}

XPathValueType XPathValue::Type() const noexcept {
    return type_;
}

bool XPathValue::IsNodeSet() const noexcept {
    return type_ == XPathValueType::NodeSet;
}

bool XPathValue::IsString() const noexcept {
    return type_ == XPathValueType::String;
}

bool XPathValue::IsNumber() const noexcept {
    return type_ == XPathValueType::Number;
}

bool XPathValue::IsBoolean() const noexcept {
    return type_ == XPathValueType::Boolean;
}

const std::vector<XPathNavigator>& XPathValue::AsNodeSet() const {
    if (!IsNodeSet()) {
        throw XmlException("XPath result is not a node-set");
    }

    return nodeSet_;
}

const std::string& XPathValue::AsString() const {
    if (!IsString()) {
        throw XmlException("XPath result is not a string");
    }

    return stringValue_;
}

double XPathValue::AsNumber() const {
    if (!IsNumber()) {
        throw XmlException("XPath result is not a number");
    }

    return numberValue_;
}

bool XPathValue::AsBoolean() const {
    if (!IsBoolean()) {
        throw XmlException("XPath result is not a boolean");
    }

    return booleanValue_;
}

XPathExpression::XPathExpression(std::string expression)
    : expression_(std::move(expression)) {
}

XPathExpression XPathExpression::Compile(std::string_view xpath) {
    const std::string normalized = NormalizeXPathPublicExpression(xpath);
    const auto target = ParseXPathPublicTarget(normalized);
    ValidateXPathPublicExpressionSyntax(normalized, target);
    return XPathExpression(normalized);
}

const std::string& XPathExpression::Expression() const noexcept {
    return expression_;
}

bool XPathExpression::Empty() const noexcept {
    return expression_.empty();
}

bool XPathNavigator::IsEmpty() const noexcept {
    return node_ == nullptr;
}

XmlNodeType XPathNavigator::NodeType() const {
    return node_ == nullptr ? XmlNodeType::None : node_->NodeType();
}

std::string XPathNavigator::Name() const {
    if (node_ == nullptr) {
        return {};
    }

    switch (node_->NodeType()) {
    case XmlNodeType::Element:
    case XmlNodeType::Attribute:
    case XmlNodeType::ProcessingInstruction:
        return node_->Name();
    default:
        return {};
    }
}

std::string XPathNavigator::LocalName() const {
    if (node_ == nullptr) {
        return {};
    }

    switch (node_->NodeType()) {
    case XmlNodeType::Element:
    case XmlNodeType::Attribute:
        return node_->LocalName();
    case XmlNodeType::ProcessingInstruction:
        return node_->Name();
    default:
        return {};
    }
}

std::string XPathNavigator::Prefix() const {
    if (node_ == nullptr) {
        return {};
    }

    switch (node_->NodeType()) {
    case XmlNodeType::Element:
    case XmlNodeType::Attribute:
        return node_->Prefix();
    default:
        return {};
    }
}

std::string XPathNavigator::NamespaceURI() const {
    if (node_ == nullptr) {
        return {};
    }

    switch (node_->NodeType()) {
    case XmlNodeType::Element:
    case XmlNodeType::Attribute:
        return node_->NamespaceURI();
    default:
        return {};
    }
}

std::string XPathNavigator::Value() const {
    return node_ == nullptr ? std::string{} : GetXPathPredicatePathNodeStringValue(*node_);
}

std::string XPathNavigator::InnerXml() const {
    return node_ == nullptr ? std::string{} : node_->InnerXml();
}

std::string XPathNavigator::OuterXml() const {
    return node_ == nullptr ? std::string{} : node_->OuterXml();
}

XPathNavigator XPathNavigator::Clone() const {
    return XPathNavigator(node_, namespaceParent_);
}

bool XPathNavigator::MoveToFirstChild() {
    if (node_ == nullptr || node_->NodeType() == XmlNodeType::Attribute) {
        return false;
    }
    for (const auto& child : node_->ChildNodes()) {
        if (child != nullptr && IsXPathVisibleNode(*child)) {
            node_ = child.get();
            namespaceParent_ = nullptr;
            return true;
        }
    }
    return false;
}

bool XPathNavigator::MoveToNext() {
    if (node_ == nullptr) {
        return false;
    }
    if (node_->NodeType() == XmlNodeType::Attribute) {
        if (namespaceParent_ != nullptr && IsXPathNamespaceDeclarationAttribute(*static_cast<const XmlAttribute*>(node_))) {
            return false;
        }
        const auto* parent = node_->ParentNode();
        if (parent == nullptr || parent->NodeType() != XmlNodeType::Element) {
            return false;
        }
        const auto& attributes = static_cast<const XmlElement*>(parent)->Attributes();
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            if (attributes[index].get() != node_) {
                continue;
            }
            for (std::size_t next = index + 1; next < attributes.size(); ++next) {
                if (!IsXPathNamespaceDeclarationAttribute(*attributes[next])) {
                    node_ = attributes[next].get();
                    namespaceParent_ = nullptr;
                    return true;
                }
            }
            return false;
        }
        return false;
    }
    for (auto sibling = node_->NextSibling(); sibling != nullptr; sibling = sibling->NextSibling()) {
        if (IsXPathVisibleNode(*sibling)) {
            node_ = sibling.get();
            namespaceParent_ = nullptr;
            return true;
        }
    }
    return false;
}

bool XPathNavigator::MoveToPrevious() {
    if (node_ == nullptr) {
        return false;
    }
    if (node_->NodeType() == XmlNodeType::Attribute) {
        if (namespaceParent_ != nullptr && IsXPathNamespaceDeclarationAttribute(*static_cast<const XmlAttribute*>(node_))) {
            return false;
        }
        const auto* parent = node_->ParentNode();
        if (parent == nullptr || parent->NodeType() != XmlNodeType::Element) {
            return false;
        }
        const auto& attributes = static_cast<const XmlElement*>(parent)->Attributes();
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            if (attributes[index].get() != node_) {
                continue;
            }
            for (auto previous = index; previous > 0; --previous) {
                if (!IsXPathNamespaceDeclarationAttribute(*attributes[previous - 1])) {
                    node_ = attributes[previous - 1].get();
                    namespaceParent_ = nullptr;
                    return true;
                }
            }
            return false;
        }
        return false;
    }
    for (auto sibling = node_->PreviousSibling(); sibling != nullptr; sibling = sibling->PreviousSibling()) {
        if (IsXPathVisibleNode(*sibling)) {
            node_ = sibling.get();
            namespaceParent_ = nullptr;
            return true;
        }
    }
    return false;
}

bool XPathNavigator::MoveToParent() {
    if (node_ == nullptr) {
        return false;
    }

    if (namespaceParent_ != nullptr && node_->NodeType() == XmlNodeType::Attribute
        && IsXPathNamespaceDeclarationAttribute(*static_cast<const XmlAttribute*>(node_))) {
        node_ = namespaceParent_;
        namespaceParent_ = nullptr;
        return true;
    }

    if (node_->ParentNode() == nullptr) {
        return false;
    }
    node_ = node_->ParentNode();
    namespaceParent_ = nullptr;
    return true;
}

bool XPathNavigator::MoveToFirstAttribute() {
    if (node_ == nullptr || node_->NodeType() != XmlNodeType::Element) {
        return false;
    }
    const auto& attributes = static_cast<const XmlElement*>(node_)->Attributes();
    for (const auto& attribute : attributes) {
        if (attribute != nullptr && !IsXPathNamespaceDeclarationAttribute(*attribute)) {
            node_ = attribute.get();
            namespaceParent_ = nullptr;
            return true;
        }
    }
    return false;
}

bool XPathNavigator::MoveToNextAttribute() {
    if (node_ == nullptr || node_->NodeType() != XmlNodeType::Attribute) {
        return false;
    }
    return MoveToNext();
}

void XPathNavigator::MoveToRoot() {
    if (node_ == nullptr) {
        return;
    }
    if (namespaceParent_ != nullptr) {
        node_ = namespaceParent_;
        namespaceParent_ = nullptr;
    }
    while (node_->ParentNode() != nullptr) {
        node_ = node_->ParentNode();
    }
}

XPathNavigator XPathNavigator::SelectSingleNode(std::string_view xpath) const {
    const auto shortcut = TryEvaluateXPathNavigatorShortcut({node_, namespaceParent_}, xpath);
    if (shortcut.handled) {
        return XPathNavigator(shortcut.context.node, shortcut.context.namespaceParent);
    }

    const auto selected = EvaluateXPathContextsFromContext({node_, namespaceParent_}, xpath, nullptr);
    if (selected.empty() || selected.front().node == nullptr) {
        return {};
    }

    return XPathNavigator(selected.front().node, selected.front().namespaceParent);
}

XPathNavigator XPathNavigator::SelectSingleNode(std::string_view xpath, const XmlNamespaceManager& namespaces) const {
    const auto shortcut = TryEvaluateXPathNavigatorShortcut({node_, namespaceParent_}, xpath);
    if (shortcut.handled) {
        return XPathNavigator(shortcut.context.node, shortcut.context.namespaceParent);
    }

    const auto selected = EvaluateXPathContextsFromContext({node_, namespaceParent_}, xpath, &namespaces);
    if (selected.empty() || selected.front().node == nullptr) {
        return {};
    }

    return XPathNavigator(selected.front().node, selected.front().namespaceParent);
}

std::vector<XPathNavigator> XPathNavigator::Select(std::string_view xpath) const {
    std::vector<XPathNavigator> result;
    if (node_ == nullptr) {
        return result;
    }

    const auto shortcut = TryEvaluateXPathNavigatorShortcut({node_, namespaceParent_}, xpath);
    if (shortcut.handled) {
        if (shortcut.context.node != nullptr) {
            result.emplace_back(shortcut.context.node, shortcut.context.namespaceParent);
        }
        return result;
    }

    const auto selected = EvaluateXPathContextsFromContext({node_, namespaceParent_}, xpath, nullptr);
    result.reserve(selected.size());
    for (const auto& item : selected) {
        if (item.node != nullptr) {
            result.emplace_back(item.node, item.namespaceParent);
        }
    }
    return result;
}

std::vector<XPathNavigator> XPathNavigator::Select(std::string_view xpath, const XmlNamespaceManager& namespaces) const {
    std::vector<XPathNavigator> result;
    if (node_ == nullptr) {
        return result;
    }

    const auto shortcut = TryEvaluateXPathNavigatorShortcut({node_, namespaceParent_}, xpath);
    if (shortcut.handled) {
        if (shortcut.context.node != nullptr) {
            result.emplace_back(shortcut.context.node, shortcut.context.namespaceParent);
        }
        return result;
    }

    const auto selected = EvaluateXPathContextsFromContext({node_, namespaceParent_}, xpath, &namespaces);
    result.reserve(selected.size());
    for (const auto& item : selected) {
        if (item.node != nullptr) {
            result.emplace_back(item.node, item.namespaceParent);
        }
    }
    return result;
}

XPathValue XPathNavigator::Evaluate(std::string_view xpath) const {
    return EvaluateXPathValueFromContext({node_, namespaceParent_}, xpath, nullptr);
}

XPathValue XPathNavigator::Evaluate(std::string_view xpath, const XmlNamespaceManager& namespaces) const {
    return EvaluateXPathValueFromContext({node_, namespaceParent_}, xpath, &namespaces);
}

XPathValue XPathNavigator::Evaluate(const XPathExpression& expression) const {
    return Evaluate(expression.Expression());
}

XPathValue XPathNavigator::Evaluate(const XPathExpression& expression, const XmlNamespaceManager& namespaces) const {
    return Evaluate(expression.Expression(), namespaces);
}

const XmlNode* XPathNavigator::UnderlyingNode() const noexcept {
    return node_;
}

XPathDocument::XPathDocument() : document_(std::make_shared<XmlDocument>()) {
}

XPathDocument::XPathDocument(std::string_view xml) : document_(XmlDocument::Parse(xml)) {
}

std::shared_ptr<XPathDocument> XPathDocument::Parse(std::string_view xml) {
    return std::make_shared<XPathDocument>(xml);
}

void XPathDocument::LoadXml(std::string_view xml) {
    if (document_ == nullptr) {
        document_ = std::make_shared<XmlDocument>();
    }
    document_->LoadXml(xml);
}

void XPathDocument::Load(const std::string& path) {
    if (document_ == nullptr) {
        document_ = std::make_shared<XmlDocument>();
    }
    document_->Load(path);
}

void XPathDocument::Load(std::istream& stream) {
    if (document_ == nullptr) {
        document_ = std::make_shared<XmlDocument>();
    }
    document_->Load(stream);
}

XPathNavigator XPathDocument::CreateNavigator() const {
    return XPathNavigator(document_.get());
}

const XmlDocument& XPathDocument::Document() const noexcept {
    return *document_;
}


}  // namespace System::Xml
