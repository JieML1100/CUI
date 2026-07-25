#include "DesignDocumentSerializer.h"
#include "AtomicFile.h"
#include "XamlDocumentParser.h"
#include "XamlDocumentSerializer.h"
#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "../../CuiRuntime/include/XamlRuntimeSchema.h"
#include "DesignDataResourceUtils.h"
#include "StoryboardPropertyPath.h"
#include "../../XmlLite/include/Xml.h"
#include <Application.h>
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <Convert.h>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace DesignerModel
{
using namespace System::Xml;

static DesignerStyleSheet VisibleStyleScope(
	const DesignDocument& document,
	const std::vector<DesignNode>& nodes,
	const DesignNode& origin);

namespace
{
	static std::string ToUtf8(const std::wstring& s)
	{
		return Convert::UnicodeToUtf8(s);
	}

	static std::wstring FromUtf8(const std::string& s)
	{
		return Convert::Utf8ToUnicode(s);
	}

	static D2D1_COLOR_F ColorFromValue(const DesignValue& j, const D2D1_COLOR_F& def)
	{
		D2D1_COLOR_F c = def;
		if (j.is_object())
		{
			c.r = j.value("r", def.r);
			c.g = j.value("g", def.g);
			c.b = j.value("b", def.b);
			c.a = j.value("a", def.a);
		}
		return c;
	}

	static std::string UIClassToString(UIClass t)
	{
		if (t == UIClass::UI_Base) return "Any";
		if (t == UIClass::UI_CUSTOM) return "CUSTOM";
		const auto* descriptor =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(t);
		if (!descriptor)
			throw std::invalid_argument(
				"UIClass has no canonical built-in XAML type");
		return ToUtf8(descriptor->TypeId.LocalName);
	}

	static bool TryParseUIClass(const std::string& s, UIClass& out)
	{
		if (s == "Any") { out = UIClass::UI_Base; return true; }
		if (s == "CUSTOM") { out = UIClass::UI_CUSTOM; return true; }
		const auto* descriptor = CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
			CuiRuntime::XamlRuntimeSchema::CuiNamespace, FromUtf8(s));
		if (!descriptor) return false;
		out = descriptor->NativeType;
		return true;
	}

	static bool TryParseConstructibleUIClass(
		const std::string& value,
		UIClass& out)
	{
		if (!TryParseUIClass(value, out)) return false;
		// UI_Base is an internal wildcard for selectors and routed
		// infrastructure, never an authorable visual node.
		if (out == UIClass::UI_Base) return false;
		if (out == UIClass::UI_CUSTOM) return true;
		const auto* descriptor =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(out);
		return descriptor && descriptor->IsConstructible;
	}

	static bool IsComponentContentPresenterType(UIClass type) noexcept
	{
		switch (type)
		{
		case UIClass::UI_Canvas:
		case UIClass::UI_StackPanel:
		case UIClass::UI_WrapPanel:
		case UIClass::UI_DockPanel:
		case UIClass::UI_Grid:
		case UIClass::UI_RelativePanel:
		case UIClass::UI_Decorator:
		case UIClass::UI_Border:
			return true;
		default:
			return false;
		}
	}

	static std::vector<DesignComponentDefinition> SnapshotComponents(
		const DesignDocument& document)
	{
		auto components = document.Components;
		for (auto& component : components)
			for (auto& node : component.Template)
			{
				node.LocalResources.Rules.clear();
				node.LocalObjectResources = {};
			}
		return components;
	}

	static void SanitizeObjectContextNodes(std::vector<DesignNode>& nodes)
	{
		for (auto& node : nodes)
		{
			node.LocalResources.Rules.clear();
			node.LocalObjectResources = {};
		}
	}

	static DesignDocument BuildLocalObjectResourceSnapshot(
		const DesignDocument& document,
		const std::vector<DesignNode>& nodes,
		const DesignNode& node)
	{
		DesignDocument snapshot;
		snapshot.Window.Name = L"LocalObjectResourceScope";
		snapshot.StyleSheet = VisibleStyleScope(document, nodes, node);
		snapshot.DataContextSchema = document.DataContextSchema;
		snapshot.DataTypes = document.DataTypes;
		snapshot.DataLists = document.DataLists;
		snapshot.CollectionViews = document.CollectionViews;
		auto visible = document.VisibleObjectResources(nodes, node);
		for (const auto& local : node.LocalObjectResources.Components)
			visible.Components.erase(std::remove_if(
				visible.Components.begin(), visible.Components.end(),
				[&](const auto& current)
				{ return current.Type == local.Type; }), visible.Components.end());
		for (const auto& local : node.LocalObjectResources.DataTemplates)
			visible.DataTemplates.erase(std::remove_if(
				visible.DataTemplates.begin(), visible.DataTemplates.end(),
				[&](const auto& current)
				{ return current.HasSameResourceIdentity(local); }),
				visible.DataTemplates.end());
		for (const auto& local : node.LocalObjectResources.ControlTemplates)
			visible.ControlTemplates.erase(std::remove_if(
				visible.ControlTemplates.begin(), visible.ControlTemplates.end(),
				[&](const auto& current)
				{ return current.HasSameResourceIdentity(local); }),
				visible.ControlTemplates.end());
		for (const auto& local : node.LocalObjectResources.ItemsPanelTemplates)
			visible.ItemsPanelTemplates.erase(std::remove_if(
				visible.ItemsPanelTemplates.begin(),
				visible.ItemsPanelTemplates.end(), [&](const auto& current)
				{ return std::wcscmp(current.Key.c_str(), local.Key.c_str()) == 0; }),
				visible.ItemsPanelTemplates.end());
		for (const auto& local : node.LocalObjectResources.GroupStyles)
			visible.GroupStyles.erase(std::remove_if(
				visible.GroupStyles.begin(), visible.GroupStyles.end(),
				[&](const auto& current)
				{ return std::wcscmp(current.Key.c_str(), local.Key.c_str()) == 0; }),
				visible.GroupStyles.end());
		for (auto& component : visible.Components)
			SanitizeObjectContextNodes(component.Template);
		for (auto& dataTemplate : visible.DataTemplates)
			SanitizeObjectContextNodes(dataTemplate.Template);
		for (auto& controlTemplate : visible.ControlTemplates)
			SanitizeObjectContextNodes(controlTemplate.Template);
		snapshot.Components = std::move(visible.Components);
		snapshot.ControlTemplates = std::move(visible.ControlTemplates);
		snapshot.DataTemplates = std::move(visible.DataTemplates);
		snapshot.ItemsPanelTemplates = std::move(visible.ItemsPanelTemplates);
		snapshot.GroupStyles = std::move(visible.GroupStyles);
		snapshot.Components.insert(snapshot.Components.end(),
			node.LocalObjectResources.Components.begin(),
			node.LocalObjectResources.Components.end());
		snapshot.DataTemplates.insert(snapshot.DataTemplates.end(),
			node.LocalObjectResources.DataTemplates.begin(),
			node.LocalObjectResources.DataTemplates.end());
		snapshot.ControlTemplates.insert(snapshot.ControlTemplates.end(),
			node.LocalObjectResources.ControlTemplates.begin(),
			node.LocalObjectResources.ControlTemplates.end());
		snapshot.ItemsPanelTemplates.insert(snapshot.ItemsPanelTemplates.end(),
			node.LocalObjectResources.ItemsPanelTemplates.begin(),
			node.LocalObjectResources.ItemsPanelTemplates.end());
		snapshot.GroupStyles.insert(snapshot.GroupStyles.end(),
			node.LocalObjectResources.GroupStyles.begin(),
			node.LocalObjectResources.GroupStyles.end());
		return snapshot;
	}

	static DesignValue LocalObjectResourcesToValue(
		const DesignDocument& document,
		const std::vector<DesignNode>& nodes,
		const DesignNode& node)
	{
		const auto snapshot = BuildLocalObjectResourceSnapshot(
			document, nodes, node);
		return {
			{ "componentCount", static_cast<int>(
				node.LocalObjectResources.Components.size()) },
			{ "dataTemplateCount", static_cast<int>(
				node.LocalObjectResources.DataTemplates.size()) },
			{ "controlTemplateCount", static_cast<int>(
				node.LocalObjectResources.ControlTemplates.size()) },
			{ "itemsPanelTemplateCount", static_cast<int>(
				node.LocalObjectResources.ItemsPanelTemplates.size()) },
			{ "groupStyleCount", static_cast<int>(
				node.LocalObjectResources.GroupStyles.size()) },
			{ "xml", DesignDocumentSerializer::ToXml(snapshot) }
		};
	}

	static bool LocalObjectResourcesFromValue(
		const DesignValue& value,
		DesignObjectResourceDictionary& output,
		std::wstring* outError,
		const std::wstring& resourceBasePath)
	{
		if (!value.is_object()
			|| !value.contains("componentCount")
			|| !value["componentCount"].is_number_integer()
			|| !value.contains("dataTemplateCount")
			|| !value["dataTemplateCount"].is_number_integer()
			|| (value.contains("controlTemplateCount")
				&& !value["controlTemplateCount"].is_number_integer())
			|| (value.contains("itemsPanelTemplateCount")
				&& !value["itemsPanelTemplateCount"].is_number_integer())
			|| (value.contains("groupStyleCount")
				&& !value["groupStyleCount"].is_number_integer())
			|| !value.contains("xml") || !value["xml"].is_string())
		{
			if (outError) *outError = L"Template local object resource snapshot is malformed.";
			return false;
		}
		const auto componentCount = value["componentCount"].get<int>();
		const auto dataTemplateCount = value["dataTemplateCount"].get<int>();
		const auto controlTemplateCount = value.value(
			"controlTemplateCount", 0);
		const auto itemsPanelTemplateCount = value.value(
			"itemsPanelTemplateCount", 0);
		const auto groupStyleCount = value.value("groupStyleCount", 0);
		if (componentCount < 0 || dataTemplateCount < 0
			|| controlTemplateCount < 0
			|| itemsPanelTemplateCount < 0 || groupStyleCount < 0)
		{
			if (outError) *outError = L"Template local object resource count is invalid.";
			return false;
		}
		DesignDocument snapshot;
		if (!DesignDocumentSerializer::FromXml(
			value["xml"].get<std::string>(), snapshot,
			outError, resourceBasePath)) return false;
		if (static_cast<size_t>(componentCount) > snapshot.Components.size()
			|| static_cast<size_t>(controlTemplateCount)
				> snapshot.ControlTemplates.size()
			|| static_cast<size_t>(dataTemplateCount) > snapshot.DataTemplates.size()
			|| static_cast<size_t>(itemsPanelTemplateCount)
				> snapshot.ItemsPanelTemplates.size()
			|| static_cast<size_t>(groupStyleCount) > snapshot.GroupStyles.size())
		{
			if (outError) *outError = L"Template local object resource snapshot is incomplete.";
			return false;
		}
		output.Components.assign(snapshot.Components.end() - componentCount,
			snapshot.Components.end());
		output.DataTemplates.assign(
			snapshot.DataTemplates.end() - dataTemplateCount,
			snapshot.DataTemplates.end());
		output.ControlTemplates.assign(
			snapshot.ControlTemplates.end() - controlTemplateCount,
			snapshot.ControlTemplates.end());
		output.ItemsPanelTemplates.assign(
			snapshot.ItemsPanelTemplates.end() - itemsPanelTemplateCount,
			snapshot.ItemsPanelTemplates.end());
		output.GroupStyles.assign(snapshot.GroupStyles.end() - groupStyleCount,
			snapshot.GroupStyles.end());
		return true;
	}

	static DesignValue LocalResourcesToValue(
		const DesignerStyleSheet& sheet,
		const DesignDocument& document,
		const std::vector<DesignNode>& nodes,
		const DesignNode& node)
	{
		DesignValue value = DesignValue::object();
		DesignValue merged = DesignValue::array();
		for (const auto& source : sheet.MergedDictionaries)
			merged.push_back(ToUtf8(source));
		value["mergedDictionaries"] = std::move(merged);
		DesignValue resources = DesignValue::array();
		for (const auto& resource : sheet.Resources)
		{
			DesignValue item = {
				{ "key", ToUtf8(resource.Key) },
				{ "kind", ToUtf8(DesignerStyleSheetUtils::ValueKindName(
					resource.Value.Kind)) },
				{ "text", ToUtf8(resource.Value.Text) },
				{ "sourceDictionary", ToUtf8(resource.SourceDictionary) }
			};
			if (!resource.Value.ObjectValue.is_null())
				item["object"] = resource.Value.ObjectValue;
			resources.push_back(std::move(item));
		}
		value["resources"] = std::move(resources);
		if (!sheet.Rules.empty())
		{
			DesignDocument snapshot;
			snapshot.Window.Name = L"TemplateLocalStyleScope";
			snapshot.StyleSheet = VisibleStyleScope(document, nodes, node);
			snapshot.Components = SnapshotComponents(document);
			value["rulesSnapshot"] = {
				{ "localCount", static_cast<int>(sheet.Rules.size()) },
				{ "xml", DesignDocumentSerializer::ToXml(snapshot) }
			};
		}
		return value;
	}

	static bool LocalResourcesFromValue(
		const DesignValue& value,
		DesignerStyleSheet& sheet,
		std::wstring* outError,
		const std::wstring& resourceBasePath)
	{
		if (!value.is_object())
		{
			if (outError) *outError = L"Template localResources is malformed.";
			return false;
		}
		DesignerStyleSheet candidate;
		if (value.contains("mergedDictionaries"))
		{
			if (!value["mergedDictionaries"].is_array()) return false;
			for (const auto& source : value["mergedDictionaries"].ArrayItems())
			{
				if (!source.is_string()) return false;
				candidate.MergedDictionaries.push_back(
					FromUtf8(source.get<std::string>()));
			}
		}
		if (value.contains("resources"))
		{
			if (!value["resources"].is_array()) return false;
			for (const auto& item : value["resources"].ArrayItems())
			{
				if (!item.is_object() || !item.contains("key")
					|| !item["key"].is_string() || !item.contains("kind")
					|| !item["kind"].is_string()) return false;
				DesignerStyleResource resource;
				resource.Key = FromUtf8(item["key"].get<std::string>());
				resource.Value.Text = FromUtf8(
					item.value("text", std::string{}));
				resource.SourceDictionary = FromUtf8(
					item.value("sourceDictionary", std::string{}));
				if (!DesignerStyleSheetUtils::TryParseValueKind(
					FromUtf8(item["kind"].get<std::string>()),
					resource.Value.Kind)) return false;
				if (item.contains("object"))
					resource.Value.ObjectValue = item["object"];
				candidate.Resources.push_back(std::move(resource));
			}
		}
		if (value.contains("rulesSnapshot"))
		{
			const auto& rules = value["rulesSnapshot"];
			if (!rules.is_object()
				|| !rules.contains("localCount")
				|| !rules["localCount"].is_number_integer()
				|| !rules.contains("xml") || !rules["xml"].is_string())
			{
				if (outError) *outError = L"Template local style snapshot is malformed.";
				return false;
			}
			const auto localCount = rules["localCount"].get<int>();
			if (localCount < 0)
			{
				if (outError) *outError = L"Template local style count is invalid.";
				return false;
			}
			DesignDocument snapshot;
			if (!DesignDocumentSerializer::FromXml(
				rules["xml"].get<std::string>(), snapshot,
				outError, resourceBasePath)) return false;
			if (static_cast<size_t>(localCount) > snapshot.StyleSheet.Rules.size())
			{
				if (outError) *outError = L"Template local style snapshot is incomplete.";
				return false;
			}
			candidate.Rules.assign(
				snapshot.StyleSheet.Rules.end() - localCount,
				snapshot.StyleSheet.Rules.end());
		}
		DesignerStyleSheetUtils::Canonicalize(candidate);
		sheet = std::move(candidate);
		return true;
	}

	static DesignValue TemplateNodeToValue(
		const DesignNode& node,
		const DesignDocument& document,
		const std::vector<DesignNode>& nodes)
	{
		DesignValue value = {
			{ "id", node.Id }, { "parentId", node.ParentId },
			{ "parent", ToUtf8(node.ParentRef) }, { "name", ToUtf8(node.Name) },
			{ "type", UIClassToString(node.Type) }, { "order", node.Order },
			{ "locked", node.Locked },
			{ "properties", EncodeDesignNodeProperties(node.Properties) },
			{ "structure", EncodeDesignNodeStructure(node.Type, node.Structure) },
			{ "events", EncodeDesignNodeEvents(node.Events) },
			{ "bindings", EncodeDesignNodeBindings(node.Bindings) },
			{ "commandBindings", EncodeDesignCommandBindings(node.CommandBindings) },
			{ "inputBindings", EncodeDesignInputBindings(node.InputBindings) }
		};
		if (!node.ComponentType.Empty())
		{
			value["componentPrefix"] = ToUtf8(node.ComponentType.XamlPrefix);
			value["componentName"] = ToUtf8(node.ComponentType.XamlName);
			value["componentNamespace"] = ToUtf8(
				node.ComponentType.XamlNamespace);
		}
		if (node.XamlType.Valid())
		{
			value["xamlNamespace"] = ToUtf8(node.XamlType.NamespaceUri);
			value["xamlName"] = ToUtf8(node.XamlType.LocalName);
		}
		if (!node.ComponentContentProperty.empty())
			value["componentContentProperty"] = ToUtf8(
				node.ComponentContentProperty);
		if (!node.PresentedComponentContent.empty())
			value["presentedComponentContent"] = ToUtf8(
				node.PresentedComponentContent);
		DesignValue templateBindings = DesignValue::object();
		for (const auto& [target, source] : node.TemplateBindings)
			templateBindings[ToUtf8(target)] = ToUtf8(source);
		value["templateBindings"] = std::move(templateBindings);
		DesignValue templateEvents = DesignValue::object();
		for (const auto& [sourceEvent, componentEvent]
			: node.TemplateEventBindings)
			templateEvents[ToUtf8(sourceEvent)] = ToUtf8(componentEvent);
		value["templateEvents"] = std::move(templateEvents);
		if (!node.LocalResources.Empty())
			value["localResources"] = LocalResourcesToValue(
				node.LocalResources, document, nodes, node);
		if (!node.LocalObjectResources.Empty())
			value["localObjectResources"] = LocalObjectResourcesToValue(
				document, nodes, node);
		return value;
	}

	static bool TemplateNodeFromValue(
		const DesignValue& value,
		DesignNode& node,
		std::wstring* outError,
		const std::wstring& resourceBasePath)
	{
		if (!value.is_object()
			|| !value.contains("id") || !value["id"].is_number_integer()
			|| !value.contains("name") || !value["name"].is_string()
			|| !value.contains("type") || !value["type"].is_string())
		{
			if (outError) *outError = L"Component template node is malformed.";
			return false;
		}
		const std::initializer_list<std::string_view> allowedMembers{
			"id", "parentId", "parent", "name", "type", "order", "locked",
			"properties", "structure", "events", "bindings",
			"commandBindings", "inputBindings",
			"componentPrefix", "componentName", "componentNamespace",
			"xamlNamespace", "xamlName", "componentContentProperty",
			"presentedComponentContent", "templateBindings", "templateEvents",
			"localResources", "localObjectResources"
		};
		for (const auto& [name, member] : value.ObjectItems())
		{
			(void)member;
			if (std::find(
				allowedMembers.begin(), allowedMembers.end(),
				std::string_view(name)) != allowedMembers.end()) continue;
			if (outError)
				*outError = L"Current template node contains unsupported member: "
					+ FromUtf8(name) + L".";
			return false;
		}
		node.Id = value["id"].get<int>();
		node.ParentId = value.value("parentId", 0);
		node.ParentRef = FromUtf8(value.value("parent", std::string{}));
		node.Name = FromUtf8(value["name"].get<std::string>());
		if (node.Id < 1 || node.Name.empty()
			|| !TryParseConstructibleUIClass(
				value["type"].get<std::string>(), node.Type))
		{
			if (outError) *outError = L"Component template node identity is invalid.";
			return false;
		}
		node.Order = value.value("order", -1);
		node.Locked = value.value("locked", false);
		if (value.contains("xamlNamespace") || value.contains("xamlName"))
		{
			node.XamlType.NamespaceUri = FromUtf8(
				value.value("xamlNamespace", std::string{}));
			node.XamlType.LocalName = FromUtf8(
				value.value("xamlName", std::string{}));
			if (!node.XamlType.Valid())
			{
				if (outError) *outError = L"Template node has an invalid XAML type identity.";
				return false;
			}
		}
		DesignValue encodedProperties = DesignValue::object();
		if (value.contains("properties"))
		{
			if (!value["properties"].is_object())
			{
				if (outError) *outError = L"Template node properties are malformed.";
				return false;
			}
			encodedProperties = value["properties"];
		}
		if (!DecodeDesignNodeProperties(
			encodedProperties, node.Properties, outError)) return false;
		DesignValue encodedEvents = DesignValue::object();
		if (value.contains("events"))
		{
			if (!value["events"].is_object())
			{
				if (outError) *outError = L"Template node events are malformed.";
				return false;
			}
			encodedEvents = value["events"];
		}
		if (!DecodeDesignNodeEvents(encodedEvents, node.Events, outError))
			return false;
		DesignValue encodedBindings = DesignValue::object();
		if (value.contains("bindings"))
		{
			if (!value["bindings"].is_object())
			{
				if (outError) *outError = L"Template node bindings are malformed.";
				return false;
			}
			encodedBindings = value["bindings"];
		}
		if (!DecodeDesignNodeBindings(encodedBindings, node.Bindings, outError))
			return false;
		DesignValue encodedCommandBindings = DesignValue::array();
		if (value.contains("commandBindings"))
		{
			if (!value["commandBindings"].is_array())
			{
				if (outError) *outError = L"Template node command bindings are malformed.";
				return false;
			}
			encodedCommandBindings = value["commandBindings"];
		}
		if (!DecodeDesignCommandBindings(
			encodedCommandBindings, node.CommandBindings, outError)) return false;
		DesignValue encodedInputBindings = DesignValue::array();
		if (value.contains("inputBindings"))
		{
			if (!value["inputBindings"].is_array())
			{
				if (outError) *outError = L"Template node input bindings are malformed.";
				return false;
			}
			encodedInputBindings = value["inputBindings"];
		}
		if (!DecodeDesignInputBindings(
			encodedInputBindings, node.InputBindings, outError)) return false;
		DesignValue encodedStructure = DesignValue::object();
		if (value.contains("structure"))
		{
			if (!value["structure"].is_object())
			{
				if (outError) *outError = L"Template node structure is malformed.";
				return false;
			}
			encodedStructure = value["structure"];
		}
		if (!DecodeDesignNodeStructure(
			node.Type, encodedStructure, node.Structure, outError)) return false;
		if (value.contains("localResources")
			&& !LocalResourcesFromValue(
				value["localResources"], node.LocalResources,
				outError, resourceBasePath))
			return false;
		if (value.contains("localObjectResources")
			&& !LocalObjectResourcesFromValue(
				value["localObjectResources"], node.LocalObjectResources,
				outError, resourceBasePath)) return false;
		node.ComponentContentProperty = FromUtf8(
			value.value("componentContentProperty", std::string{}));
		node.PresentedComponentContent = FromUtf8(
			value.value("presentedComponentContent", std::string{}));
		if (value.contains("componentName"))
		{
			node.ComponentType.XamlPrefix = FromUtf8(
				value.value("componentPrefix", std::string{}));
			node.ComponentType.XamlName = FromUtf8(
				value.value("componentName", std::string{}));
			node.ComponentType.XamlNamespace = FromUtf8(
				value.value("componentNamespace", std::string{}));
			if (node.ComponentType.XamlPrefix.empty()
				|| node.ComponentType.XamlName.empty()
				|| node.ComponentType.XamlNamespace.empty())
			{
				if (outError) *outError = L"Component template contains an invalid component reference.";
				return false;
			}
		}
		if (value.contains("templateBindings"))
		{
			const auto& bindings = value["templateBindings"];
			if (!bindings.is_object()) return false;
			for (const auto& [target, source] : bindings.ObjectItems())
			{
				if (!source.is_string() || target.empty()
					|| source.get<std::string>().empty()) return false;
				node.TemplateBindings.emplace(
					FromUtf8(target), FromUtf8(source.get<std::string>()));
			}
		}
		if (value.contains("templateEvents"))
		{
			const auto& events = value["templateEvents"];
			if (!events.is_object()) return false;
			for (const auto& [sourceEvent, componentEvent] : events.ObjectItems())
			{
				if (!componentEvent.is_string() || sourceEvent.empty()
					|| componentEvent.get<std::string>().empty()) return false;
				node.TemplateEventBindings.emplace(
					FromUtf8(sourceEvent),
					FromUtf8(componentEvent.get<std::string>()));
			}
		}
		return true;
	}

	static std::shared_ptr<XmlElement> FindChildElement(const std::shared_ptr<XmlElement>& parent, std::string_view name)
	{
		if (!parent) return nullptr;
		for (const auto& child : parent->ChildNodes())
		{
			if (child && child->NodeType() == XmlNodeType::Element && child->Name() == name)
			{
				return std::static_pointer_cast<XmlElement>(child);
			}
		}
		return nullptr;
	}

	static std::vector<std::shared_ptr<XmlElement>> FindChildElements(const std::shared_ptr<XmlElement>& parent, std::string_view name)
	{
		std::vector<std::shared_ptr<XmlElement>> elements;
		if (!parent) return elements;
		for (const auto& child : parent->ChildNodes())
		{
			if (child && child->NodeType() == XmlNodeType::Element && child->Name() == name)
			{
				elements.push_back(std::static_pointer_cast<XmlElement>(child));
			}
		}
		return elements;
	}

	static bool ValidateElementShape(
		const std::shared_ptr<XmlElement>& element,
		std::initializer_list<std::string_view> allowedAttributes,
		std::initializer_list<std::string_view> allowedChildren,
		std::wstring_view context,
		std::wstring* outError,
		bool allowRepeatedChildren = false)
	{
		if (!element) return false;
		for (const auto& attribute : element->Attributes())
		{
			if (!attribute) continue;
			const auto name = std::string_view(attribute->Name());
			if (std::find(allowedAttributes.begin(), allowedAttributes.end(), name)
				!= allowedAttributes.end()) continue;
			if (outError)
			{
				*outError = std::wstring(context)
					+ L" contains unsupported attribute: "
					+ FromUtf8(attribute->Name()) + L".";
			}
			return false;
		}
		std::unordered_set<std::string> childNames;
		for (const auto& child : element->ChildNodes())
		{
			if (!child || child->NodeType() != XmlNodeType::Element) continue;
			const auto name = std::string_view(child->Name());
			if (std::find(allowedChildren.begin(), allowedChildren.end(), name)
				== allowedChildren.end())
			{
				if (outError)
				{
					*outError = std::wstring(context)
						+ L" contains unsupported child element: "
						+ FromUtf8(child->Name()) + L".";
				}
				return false;
			}
			if (allowRepeatedChildren
				|| childNames.insert(child->Name()).second) continue;
			if (outError)
			{
				*outError = std::wstring(context)
					+ L" contains duplicate child element: "
					+ FromUtf8(child->Name()) + L".";
			}
			return false;
		}
		return true;
	}

	static std::string BoolToString(bool value)
	{
		return value ? "true" : "false";
	}

	static bool TryParseBool(std::string value, bool& out)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
			return (char)std::tolower(ch);
		});
		if (value == "true" || value == "1")
		{
			out = true;
			return true;
		}
		if (value == "false" || value == "0")
		{
			out = false;
			return true;
		}
		return false;
	}

	template<typename T>
	static bool TryParseIntegral(const std::string& value, T& out)
	{
		try
		{
			if constexpr (std::is_unsigned_v<T>)
			{
				unsigned long long parsed = std::stoull(value);
				out = (T)parsed;
			}
			else
			{
				long long parsed = std::stoll(value);
				out = (T)parsed;
			}
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	static bool TryParseDouble(const std::string& value, double& out)
	{
		try
		{
			out = std::stod(value);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	static std::shared_ptr<XmlElement> AppendElement(XmlDocument& document, const std::shared_ptr<XmlElement>& parent, const std::string& name)
	{
		auto element = document.CreateElement(name);
		parent->AppendChild(element);
		return element;
	}

	static void SetColorAttributes(const std::shared_ptr<XmlElement>& element, const D2D1_COLOR_F& color)
	{
		if (!element) return;
		element->SetAttribute("r", std::to_string(color.r));
		element->SetAttribute("g", std::to_string(color.g));
		element->SetAttribute("b", std::to_string(color.b));
		element->SetAttribute("a", std::to_string(color.a));
	}

	static D2D1_COLOR_F ColorFromXmlElement(const std::shared_ptr<XmlElement>& element, const D2D1_COLOR_F& def)
	{
		D2D1_COLOR_F color = def;
		if (!element) return color;

		double value = 0.0;
		if (TryParseDouble(element->GetAttribute("r"), value)) color.r = (float)value;
		if (TryParseDouble(element->GetAttribute("g"), value)) color.g = (float)value;
		if (TryParseDouble(element->GetAttribute("b"), value)) color.b = (float)value;
		if (TryParseDouble(element->GetAttribute("a"), value)) color.a = (float)value;
		return color;
	}

	static void WriteValue(XmlDocument& document, const std::shared_ptr<XmlElement>& element, const DesignValue& value)
	{
		if (!element) return;

		if (value.is_object())
		{
			element->SetAttribute("type", "object");
			for (const auto& [key, childValue] : value.ObjectItems())
			{
				auto member = AppendElement(document, element, "member");
				member->SetAttribute("name", key);
				WriteValue(document, member, childValue);
			}
			return;
		}

		if (value.is_array())
		{
			element->SetAttribute("type", "array");
			for (const auto& itemValue : value.ArrayItems())
			{
				auto item = AppendElement(document, element, "item");
				WriteValue(document, item, itemValue);
			}
			return;
		}

		if (value.is_null())
		{
			element->SetAttribute("type", "null");
			return;
		}

		if (value.is_boolean())
		{
			element->SetAttribute("type", "boolean");
			element->SetInnerText(BoolToString(value.get<bool>()));
			return;
		}

		if (value.is_number_unsigned())
		{
			element->SetAttribute("type", "unsigned");
			element->SetInnerText(std::to_string(value.get<unsigned long long>()));
			return;
		}

		if (value.is_number_integer())
		{
			element->SetAttribute("type", "integer");
			element->SetInnerText(std::to_string(value.get<long long>()));
			return;
		}

		if (value.is_number_float())
		{
			element->SetAttribute("type", "float");
			std::ostringstream text;
			text << std::setprecision(std::numeric_limits<double>::max_digits10)
				<< value.get<double>();
			element->SetInnerText(text.str());
			return;
		}

		element->SetAttribute("type", "string");
		element->SetInnerText(value.ToString());
	}

	static bool ReadValue(const std::shared_ptr<XmlElement>& element, DesignValue& out, std::wstring* outError)
	{
		if (!element)
		{
			out = DesignValue();
			return true;
		}

		std::string type = element->GetAttribute("type");
		if (type.empty()) type = "object";

		if (type == "object")
		{
			DesignValue object = DesignValue::object();
			for (const auto& child : FindChildElements(element, "member"))
			{
				std::string name = child->GetAttribute("name");
				if (name.empty())
				{
					if (outError) *outError = L"XML object member is missing the name attribute.";
					return false;
				}
				DesignValue childValue;
				if (!ReadValue(child, childValue, outError))
				{
					return false;
				}
				object[name] = std::move(childValue);
			}
			out = std::move(object);
			return true;
		}

		if (type == "array")
		{
			DesignValue array = DesignValue::array();
			for (const auto& child : FindChildElements(element, "item"))
			{
				DesignValue childValue;
				if (!ReadValue(child, childValue, outError))
				{
					return false;
				}
				array.push_back(std::move(childValue));
			}
			out = std::move(array);
			return true;
		}

		if (type == "null")
		{
			out = nullptr;
			return true;
		}

		if (type == "boolean")
		{
			bool parsed = false;
			if (!TryParseBool(element->InnerText(), parsed))
			{
				if (outError) *outError = L"Invalid XML boolean value: " + FromUtf8(element->InnerText());
				return false;
			}
			out = parsed;
			return true;
		}

		if (type == "integer")
		{
			long long parsed = 0;
			if (!TryParseIntegral(element->InnerText(), parsed))
			{
				if (outError) *outError = L"Invalid XML integer value: " + FromUtf8(element->InnerText());
				return false;
			}
			out = parsed;
			return true;
		}

		if (type == "unsigned")
		{
			unsigned long long parsed = 0;
			if (!TryParseIntegral(element->InnerText(), parsed))
			{
				if (outError) *outError = L"Invalid XML unsigned value: " + FromUtf8(element->InnerText());
				return false;
			}
			out = parsed;
			return true;
		}

		if (type == "float")
		{
			double parsed = 0.0;
			if (!TryParseDouble(element->InnerText(), parsed))
			{
				if (outError) *outError = L"Invalid XML float value: " + FromUtf8(element->InnerText());
				return false;
			}
			out = parsed;
			return true;
		}

		if (type == "string")
		{
			out = element->InnerText();
			return true;
		}

		if (outError) *outError = L"Unsupported XML value type: " + FromUtf8(type);
		return false;
	}

	static bool TryReadBoolAttribute(const std::shared_ptr<XmlElement>& element, const char* name, bool& out)
	{
		if (!element || !element->HasAttribute(name)) return false;
		return TryParseBool(element->GetAttribute(name), out);
	}

	template<typename T>
	static bool TryReadIntegralAttribute(const std::shared_ptr<XmlElement>& element, const char* name, T& out)
	{
		if (!element || !element->HasAttribute(name)) return false;
		return TryParseIntegral(element->GetAttribute(name), out);
	}

	static bool TryReadFloatAttribute(const std::shared_ptr<XmlElement>& element, const char* name, float& out)
	{
		if (!element || !element->HasAttribute(name)) return false;
		double value = 0.0;
		if (!TryParseDouble(element->GetAttribute(name), value)) return false;
		out = (float)value;
		return true;
	}

	static std::string FloatText(float value)
	{
		std::ostringstream text;
		text << std::setprecision(std::numeric_limits<float>::max_digits10)
			<< value;
		return text.str();
	}

	static std::string DoubleText(double value)
	{
		std::ostringstream text;
		text << std::setprecision(std::numeric_limits<double>::max_digits10)
			<< value;
		return text.str();
	}

	static void WriteVisualStateAnimationSnapshot(
		XmlDocument& xml,
		const std::shared_ptr<XmlElement>& parent,
		const DesignerVisualStateAnimation& animation)
	{
		auto item = AppendElement(xml, parent, "animation");
		item->SetAttribute("type", animation.Kind == DesignerAnimationKind::Color
			? "Color" : animation.Kind == DesignerAnimationKind::Object
				? "Object" : animation.Kind == DesignerAnimationKind::Thickness
					? "Thickness" : animation.Kind == DesignerAnimationKind::Point
						? "Point" : animation.Kind == DesignerAnimationKind::Vector
						? "Vector" : animation.Kind == DesignerAnimationKind::Rect
						? "Rect" : animation.Kind == DesignerAnimationKind::Size
						? "Size" : animation.Kind == DesignerAnimationKind::Matrix
						? "Matrix" : "Double");
		if (!animation.TargetName.empty())
			item->SetAttribute("target", ToUtf8(animation.TargetName));
		item->SetAttribute("property", ToUtf8(animation.PropertyName));
		if (animation.KeyFrames.empty() && animation.HasFrom)
		{
			if (animation.FromUsesResource)
			{
				item->SetAttribute("fromResource", ToUtf8(animation.FromResourceKey));
				item->SetAttribute("fromFallbackKind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(animation.From.Kind)));
				if (!animation.From.ObjectValue.is_null())
					WriteValue(xml, AppendElement(
						xml, item, "fromFallbackObjectValue"),
						animation.From.ObjectValue);
				else item->SetAttribute(
					"fromFallback", ToUtf8(animation.From.Text));
			}
			else
			{
				item->SetAttribute("fromKind", ToUtf8(
				DesignerStyleSheetUtils::ValueKindName(animation.From.Kind)));
				if (!animation.From.ObjectValue.is_null())
					WriteValue(xml, AppendElement(xml, item, "fromObjectValue"),
						animation.From.ObjectValue);
				else item->SetAttribute("from", ToUtf8(animation.From.Text));
			}
		}
		if (animation.KeyFrames.empty() && animation.HasTo)
		{
			if (animation.ToUsesResource)
			{
				item->SetAttribute("toResource", ToUtf8(animation.ToResourceKey));
				item->SetAttribute("toFallbackKind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(animation.To.Kind)));
				if (!animation.To.ObjectValue.is_null())
					WriteValue(xml, AppendElement(
						xml, item, "toFallbackObjectValue"),
						animation.To.ObjectValue);
				else item->SetAttribute(
					"toFallback", ToUtf8(animation.To.Text));
			}
			else
			{
				item->SetAttribute("toKind", ToUtf8(
				DesignerStyleSheetUtils::ValueKindName(animation.To.Kind)));
				if (!animation.To.ObjectValue.is_null())
					WriteValue(xml, AppendElement(xml, item, "toObjectValue"),
						animation.To.ObjectValue);
				else item->SetAttribute("to", ToUtf8(animation.To.Text));
			}
		}
		if (animation.KeyFrames.empty() && animation.HasBy)
		{
			if (animation.ByUsesResource)
			{
				item->SetAttribute("byResource", ToUtf8(animation.ByResourceKey));
				item->SetAttribute("byFallbackKind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(animation.By.Kind)));
				if (!animation.By.ObjectValue.is_null())
					WriteValue(xml, AppendElement(
						xml, item, "byFallbackObjectValue"),
						animation.By.ObjectValue);
				else item->SetAttribute(
					"byFallback", ToUtf8(animation.By.Text));
			}
			else
			{
				item->SetAttribute("byKind", ToUtf8(
				DesignerStyleSheetUtils::ValueKindName(animation.By.Kind)));
				if (!animation.By.ObjectValue.is_null())
					WriteValue(xml, AppendElement(xml, item, "byObjectValue"),
						animation.By.ObjectValue);
				else item->SetAttribute("by", ToUtf8(animation.By.Text));
			}
		}
		item->SetAttribute("beginTimeMs", std::to_string(
			animation.BeginTimeMilliseconds));
		item->SetAttribute("durationMs", std::to_string(
			animation.DurationMilliseconds));
		item->SetAttribute("repeatBehavior",
			animation.RepeatBehavior == DesignerRepeatBehaviorKind::Duration
				? "Duration"
				: animation.RepeatBehavior == DesignerRepeatBehaviorKind::Forever
					? "Forever" : "Count");
		item->SetAttribute("repeatCount", DoubleText(animation.RepeatCount));
		item->SetAttribute("repeatDurationMs", std::to_string(
			animation.RepeatDurationMilliseconds));
		item->SetAttribute("autoReverse",
			animation.AutoReverse ? "true" : "false");
		item->SetAttribute("isAdditive",
			animation.IsAdditive ? "true" : "false");
		item->SetAttribute("isCumulative",
			animation.IsCumulative ? "true" : "false");
		item->SetAttribute("fillBehavior",
			animation.FillBehavior == DesignerTimelineFillBehavior::Stop
				? "Stop" : "HoldEnd");
		item->SetAttribute("speedRatio", DoubleText(animation.SpeedRatio));
		item->SetAttribute("accelerationRatio",
			DoubleText(animation.AccelerationRatio));
		item->SetAttribute("decelerationRatio",
			DoubleText(animation.DecelerationRatio));
		const char* easing = "Linear";
		switch (animation.Easing)
		{
		case DesignerEasingKind::Quadratic: easing = "Quadratic"; break;
		case DesignerEasingKind::Cubic: easing = "Cubic"; break;
		case DesignerEasingKind::Sine: easing = "Sine"; break;
		case DesignerEasingKind::Linear:
		default: break;
		}
		item->SetAttribute("easing", easing);
		const char* easingMode = animation.EasingMode == DesignerEasingMode::EaseIn
			? "EaseIn" : animation.EasingMode == DesignerEasingMode::EaseInOut
				? "EaseInOut" : "EaseOut";
		item->SetAttribute("easingMode", easingMode);
		for (const auto& keyFrame : animation.KeyFrames)
		{
			auto frame = AppendElement(xml, item, "keyFrame");
			const char* kind = "Linear";
			switch (keyFrame.Kind)
			{
			case DesignerKeyFrameKind::Discrete: kind = "Discrete"; break;
			case DesignerKeyFrameKind::Easing: kind = "Easing"; break;
			case DesignerKeyFrameKind::Spline: kind = "Spline"; break;
			case DesignerKeyFrameKind::Linear:
			default: break;
			}
			frame->SetAttribute("kind", kind);
			frame->SetAttribute("keyTimeMs", std::to_string(
				keyFrame.KeyTimeMilliseconds));
			if (keyFrame.UsesResource)
			{
				frame->SetAttribute("resource", ToUtf8(keyFrame.ResourceKey));
				frame->SetAttribute("fallbackValueKind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(keyFrame.Value.Kind)));
				if (!keyFrame.Value.ObjectValue.is_null())
					WriteValue(xml, AppendElement(
						xml, frame, "fallbackObjectValue"),
						keyFrame.Value.ObjectValue);
				else frame->SetAttribute(
					"fallbackValue", ToUtf8(keyFrame.Value.Text));
			}
			else
			{
				frame->SetAttribute("valueKind", ToUtf8(
				DesignerStyleSheetUtils::ValueKindName(keyFrame.Value.Kind)));
				if (!keyFrame.Value.ObjectValue.is_null())
					WriteValue(xml, AppendElement(xml, frame, "objectValue"),
						keyFrame.Value.ObjectValue);
				else frame->SetAttribute("value", ToUtf8(keyFrame.Value.Text));
			}
			if (keyFrame.Kind == DesignerKeyFrameKind::Easing)
			{
				const char* frameEasing = "Linear";
				switch (keyFrame.Easing)
				{
				case DesignerEasingKind::Quadratic: frameEasing = "Quadratic"; break;
				case DesignerEasingKind::Cubic: frameEasing = "Cubic"; break;
				case DesignerEasingKind::Sine: frameEasing = "Sine"; break;
				case DesignerEasingKind::Linear:
				default: break;
				}
				frame->SetAttribute("easing", frameEasing);
				frame->SetAttribute("easingMode",
					keyFrame.EasingMode == DesignerEasingMode::EaseIn
						? "EaseIn"
						: keyFrame.EasingMode == DesignerEasingMode::EaseInOut
							? "EaseInOut" : "EaseOut");
			}
			else if (keyFrame.Kind == DesignerKeyFrameKind::Spline)
			{
				frame->SetAttribute("x1", FloatText(keyFrame.KeySplineX1));
				frame->SetAttribute("y1", FloatText(keyFrame.KeySplineY1));
				frame->SetAttribute("x2", FloatText(keyFrame.KeySplineX2));
				frame->SetAttribute("y2", FloatText(keyFrame.KeySplineY2));
			}
		}
	}

	static bool ReadVisualStateAnimationSnapshot(
		const std::shared_ptr<XmlElement>& element,
		DesignerVisualStateAnimation& animation,
		std::wstring* outError)
	{
		auto fail = [&](const wchar_t* message)
		{
			if (outError) *outError = message;
			return false;
		};
		const auto type = element->GetAttribute("type");
		if (std::strcmp(type.c_str(), "Double") == 0)
			animation.Kind = DesignerAnimationKind::Double;
		else if (std::strcmp(type.c_str(), "Color") == 0)
			animation.Kind = DesignerAnimationKind::Color;
		else if (std::strcmp(type.c_str(), "Object") == 0)
			animation.Kind = DesignerAnimationKind::Object;
		else if (std::strcmp(type.c_str(), "Thickness") == 0)
			animation.Kind = DesignerAnimationKind::Thickness;
		else if (std::strcmp(type.c_str(), "Point") == 0)
			animation.Kind = DesignerAnimationKind::Point;
		else if (std::strcmp(type.c_str(), "Vector") == 0)
			animation.Kind = DesignerAnimationKind::Vector;
		else if (std::strcmp(type.c_str(), "Rect") == 0)
			animation.Kind = DesignerAnimationKind::Rect;
		else if (std::strcmp(type.c_str(), "Size") == 0)
			animation.Kind = DesignerAnimationKind::Size;
		else if (std::strcmp(type.c_str(), "Matrix") == 0)
			animation.Kind = DesignerAnimationKind::Matrix;
		else return fail(L"Component visual-state animation type is invalid.");
		animation.TargetName = FromUtf8(element->GetAttribute("target"));
		animation.PropertyName = FromUtf8(element->GetAttribute("property"));
		if (animation.PropertyName.empty()
			|| !TryParseIntegral(element->GetAttribute("beginTimeMs"),
				animation.BeginTimeMilliseconds)
			|| !TryParseIntegral(element->GetAttribute("durationMs"),
				animation.DurationMilliseconds))
			return fail(L"Component visual-state animation timing or property is invalid.");
		const auto repeatBehavior = element->GetAttribute("repeatBehavior");
		if (repeatBehavior.empty()
			|| std::strcmp(repeatBehavior.c_str(), "Count") == 0)
		{
			animation.RepeatBehavior = DesignerRepeatBehaviorKind::Count;
			if (!repeatBehavior.empty()
				&& (!TryParseDouble(element->GetAttribute("repeatCount"),
					animation.RepeatCount)
					|| !std::isfinite(animation.RepeatCount)
					|| animation.RepeatCount <= 0.0))
				return fail(L"Component animation repeat count is invalid.");
		}
		else if (std::strcmp(repeatBehavior.c_str(), "Duration") == 0)
		{
			animation.RepeatBehavior = DesignerRepeatBehaviorKind::Duration;
			if (!TryParseIntegral(element->GetAttribute("repeatDurationMs"),
				animation.RepeatDurationMilliseconds)
				|| animation.RepeatDurationMilliseconds == 0)
				return fail(L"Component animation repeat duration is invalid.");
		}
		else if (std::strcmp(repeatBehavior.c_str(), "Forever") == 0)
			animation.RepeatBehavior = DesignerRepeatBehaviorKind::Forever;
		else return fail(L"Component animation repeat behavior is invalid.");
		if (element->HasAttribute("autoReverse")
			&& !TryReadBoolAttribute(
				element, "autoReverse", animation.AutoReverse))
			return fail(L"Component animation AutoReverse is invalid.");
		if (element->HasAttribute("isAdditive")
			&& !TryReadBoolAttribute(
				element, "isAdditive", animation.IsAdditive))
			return fail(L"Component animation IsAdditive is invalid.");
		if (element->HasAttribute("isCumulative")
			&& !TryReadBoolAttribute(
				element, "isCumulative", animation.IsCumulative))
			return fail(L"Component animation IsCumulative is invalid.");
		const auto fillBehavior = element->GetAttribute("fillBehavior");
		if (fillBehavior.empty()
			|| std::strcmp(fillBehavior.c_str(), "HoldEnd") == 0)
			animation.FillBehavior = DesignerTimelineFillBehavior::HoldEnd;
		else if (std::strcmp(fillBehavior.c_str(), "Stop") == 0)
			animation.FillBehavior = DesignerTimelineFillBehavior::Stop;
		else return fail(L"Component animation FillBehavior is invalid.");
		if (element->HasAttribute("speedRatio")
			&& (!TryParseDouble(element->GetAttribute("speedRatio"),
				animation.SpeedRatio)
				|| !std::isfinite(animation.SpeedRatio)
				|| animation.SpeedRatio <= 0.0))
			return fail(L"Component animation SpeedRatio is invalid.");
		if (element->HasAttribute("accelerationRatio")
			&& (!TryParseDouble(element->GetAttribute("accelerationRatio"),
				animation.AccelerationRatio)
				|| !std::isfinite(animation.AccelerationRatio)
				|| animation.AccelerationRatio < 0.0
				|| animation.AccelerationRatio > 1.0))
			return fail(L"Component animation AccelerationRatio is invalid.");
		if (element->HasAttribute("decelerationRatio")
			&& (!TryParseDouble(element->GetAttribute("decelerationRatio"),
				animation.DecelerationRatio)
				|| !std::isfinite(animation.DecelerationRatio)
				|| animation.DecelerationRatio < 0.0
				|| animation.DecelerationRatio > 1.0))
			return fail(L"Component animation DecelerationRatio is invalid.");
		if (animation.AccelerationRatio + animation.DecelerationRatio > 1.0)
			return fail(L"Component animation acceleration and deceleration ratios are invalid.");
		const auto fromResource = FromUtf8(element->GetAttribute("fromResource"));
		const auto fromKind = FromUtf8(element->GetAttribute("fromKind"));
		const auto fromFallbackKind = FromUtf8(
			element->GetAttribute("fromFallbackKind"));
		if (!fromResource.empty() && !fromKind.empty())
			return fail(L"Component visual-state animation From is ambiguous.");
		if (!fromResource.empty())
		{
			animation.HasFrom = true;
			animation.FromUsesResource = true;
			animation.FromResourceKey = fromResource;
		}
		if (!fromKind.empty())
		{
			animation.HasFrom = true;
			if (!DesignerStyleSheetUtils::TryParseValueKind(
				fromKind, animation.From.Kind))
				return fail(L"Component visual-state animation From kind is invalid.");
			if (const auto object = FindChildElement(element, "fromObjectValue"))
			{
				if (!ReadValue(object, animation.From.ObjectValue, outError)) return false;
			}
			else animation.From.Text = FromUtf8(element->GetAttribute("from"));
		}
		else if (!fromFallbackKind.empty())
		{
			if (fromResource.empty()
				|| !DesignerStyleSheetUtils::TryParseValueKind(
					fromFallbackKind, animation.From.Kind))
				return fail(L"Component visual-state animation From fallback is invalid.");
			if (const auto object = FindChildElement(
				element, "fromFallbackObjectValue"))
			{
				if (!ReadValue(object, animation.From.ObjectValue, outError)) return false;
			}
			else animation.From.Text = FromUtf8(
				element->GetAttribute("fromFallback"));
		}
		const auto keyFrameElements = FindChildElements(element, "keyFrame");
		const bool keyFrameAnimation = !keyFrameElements.empty();
		if (keyFrameAnimation && animation.HasFrom)
			return fail(L"Component key-frame animation cannot declare From.");
		const auto toResource = FromUtf8(element->GetAttribute("toResource"));
		const auto toKind = FromUtf8(element->GetAttribute("toKind"));
		const auto toFallbackKind = FromUtf8(
			element->GetAttribute("toFallbackKind"));
		const bool hasTo = !toResource.empty() || !toKind.empty()
			|| element->HasAttribute("to");
		if (keyFrameAnimation && hasTo)
			return fail(L"Component key-frame animation cannot declare To.");
		if (!toResource.empty() && !toKind.empty())
			return fail(L"Component visual-state animation To is ambiguous.");
		if (!keyFrameAnimation && !toResource.empty())
		{
			animation.HasTo = true;
			animation.ToUsesResource = true;
			animation.ToResourceKey = toResource;
		}
		if (!keyFrameAnimation && !toKind.empty())
		{
			animation.HasTo = true;
			if (!DesignerStyleSheetUtils::TryParseValueKind(
				toKind, animation.To.Kind))
				return fail(L"Component visual-state animation To is invalid.");
			if (const auto object = FindChildElement(element, "toObjectValue"))
			{
				if (!ReadValue(object, animation.To.ObjectValue, outError)) return false;
			}
			else animation.To.Text = FromUtf8(element->GetAttribute("to"));
		}
		else if (!keyFrameAnimation && !toFallbackKind.empty())
		{
			if (toResource.empty()
				|| !DesignerStyleSheetUtils::TryParseValueKind(
					toFallbackKind, animation.To.Kind))
				return fail(L"Component visual-state animation To fallback is invalid.");
			if (const auto object = FindChildElement(
				element, "toFallbackObjectValue"))
			{
				if (!ReadValue(object, animation.To.ObjectValue, outError)) return false;
			}
			else animation.To.Text = FromUtf8(
				element->GetAttribute("toFallback"));
		}
		const auto byResource = FromUtf8(element->GetAttribute("byResource"));
		const auto byKind = FromUtf8(element->GetAttribute("byKind"));
		const auto byFallbackKind = FromUtf8(
			element->GetAttribute("byFallbackKind"));
		const bool hasBy = !byResource.empty() || !byKind.empty()
			|| element->HasAttribute("by");
		if (keyFrameAnimation && hasBy)
			return fail(L"Component key-frame animation cannot declare By.");
		if (!byResource.empty() && !byKind.empty())
			return fail(L"Component visual-state animation By is ambiguous.");
		if (!keyFrameAnimation && !byResource.empty())
		{
			animation.HasBy = true;
			animation.ByUsesResource = true;
			animation.ByResourceKey = byResource;
		}
		if (!keyFrameAnimation && !byKind.empty())
		{
			animation.HasBy = true;
			if (!DesignerStyleSheetUtils::TryParseValueKind(
				byKind, animation.By.Kind))
				return fail(L"Component visual-state animation By is invalid.");
			if (const auto object = FindChildElement(element, "byObjectValue"))
			{
				if (!ReadValue(object, animation.By.ObjectValue, outError)) return false;
			}
			else animation.By.Text = FromUtf8(element->GetAttribute("by"));
		}
		else if (!keyFrameAnimation && !byFallbackKind.empty())
		{
			if (byResource.empty()
				|| !DesignerStyleSheetUtils::TryParseValueKind(
					byFallbackKind, animation.By.Kind))
				return fail(L"Component visual-state animation By fallback is invalid.");
			if (const auto object = FindChildElement(
				element, "byFallbackObjectValue"))
			{
				if (!ReadValue(object, animation.By.ObjectValue, outError)) return false;
			}
			else animation.By.Text = FromUtf8(
				element->GetAttribute("byFallback"));
		}
		const auto easing = element->GetAttribute("easing");
		if (std::strcmp(easing.c_str(), "Linear") == 0)
			animation.Easing = DesignerEasingKind::Linear;
		else if (std::strcmp(easing.c_str(), "Quadratic") == 0)
			animation.Easing = DesignerEasingKind::Quadratic;
		else if (std::strcmp(easing.c_str(), "Cubic") == 0)
			animation.Easing = DesignerEasingKind::Cubic;
		else if (std::strcmp(easing.c_str(), "Sine") == 0)
			animation.Easing = DesignerEasingKind::Sine;
		else return fail(L"Component visual-state animation easing is invalid.");
		const auto easingMode = element->GetAttribute("easingMode");
		if (std::strcmp(easingMode.c_str(), "EaseIn") == 0)
			animation.EasingMode = DesignerEasingMode::EaseIn;
		else if (std::strcmp(easingMode.c_str(), "EaseOut") == 0)
			animation.EasingMode = DesignerEasingMode::EaseOut;
		else if (std::strcmp(easingMode.c_str(), "EaseInOut") == 0)
			animation.EasingMode = DesignerEasingMode::EaseInOut;
		else return fail(L"Component visual-state animation easing mode is invalid.");
		for (const auto& frameElement : keyFrameElements)
		{
			DesignerAnimationKeyFrame frame;
			const auto kind = frameElement->GetAttribute("kind");
			if (std::strcmp(kind.c_str(), "Discrete") == 0)
				frame.Kind = DesignerKeyFrameKind::Discrete;
			else if (std::strcmp(kind.c_str(), "Linear") == 0)
				frame.Kind = DesignerKeyFrameKind::Linear;
			else if (std::strcmp(kind.c_str(), "Easing") == 0)
				frame.Kind = DesignerKeyFrameKind::Easing;
			else if (std::strcmp(kind.c_str(), "Spline") == 0)
				frame.Kind = DesignerKeyFrameKind::Spline;
			else return fail(L"Component animation key-frame kind is invalid.");
			if (!TryParseIntegral(frameElement->GetAttribute("keyTimeMs"),
				frame.KeyTimeMilliseconds))
				return fail(L"Component animation KeyTime is invalid.");
			frame.ResourceKey = FromUtf8(frameElement->GetAttribute("resource"));
			const auto valueKind = FromUtf8(frameElement->GetAttribute("valueKind"));
			const auto objectValue = FindChildElement(frameElement, "objectValue");
			const auto fallbackKind = FromUtf8(
				frameElement->GetAttribute("fallbackValueKind"));
			const auto fallbackObject = FindChildElement(
				frameElement, "fallbackObjectValue");
			if (!frame.ResourceKey.empty() && (!valueKind.empty() || objectValue))
				return fail(L"Component animation key-frame value is ambiguous.");
			frame.UsesResource = !frame.ResourceKey.empty();
			if (!valueKind.empty())
			{
				if (valueKind.empty()
					|| !DesignerStyleSheetUtils::TryParseValueKind(
						valueKind, frame.Value.Kind))
					return fail(L"Component animation key-frame value is missing or invalid.");
				if (objectValue)
				{
					if (frameElement->HasAttribute("value")
						|| !ReadValue(objectValue, frame.Value.ObjectValue, outError))
						return fail(L"Component animation key-frame object value is invalid.");
				}
				else frame.Value.Text = FromUtf8(frameElement->GetAttribute("value"));
			}
			else if (!fallbackKind.empty())
			{
				if (!frame.UsesResource
					|| !DesignerStyleSheetUtils::TryParseValueKind(
						fallbackKind, frame.Value.Kind))
					return fail(L"Component animation key-frame fallback is invalid.");
				if (fallbackObject)
				{
					if (!ReadValue(
						fallbackObject, frame.Value.ObjectValue, outError)) return false;
				}
				else frame.Value.Text = FromUtf8(
					frameElement->GetAttribute("fallbackValue"));
			}
			if (frame.Kind == DesignerKeyFrameKind::Easing)
			{
				const auto frameEasing = frameElement->GetAttribute("easing");
				if (std::strcmp(frameEasing.c_str(), "Linear") == 0)
					frame.Easing = DesignerEasingKind::Linear;
				else if (std::strcmp(frameEasing.c_str(), "Quadratic") == 0)
					frame.Easing = DesignerEasingKind::Quadratic;
				else if (std::strcmp(frameEasing.c_str(), "Cubic") == 0)
					frame.Easing = DesignerEasingKind::Cubic;
				else if (std::strcmp(frameEasing.c_str(), "Sine") == 0)
					frame.Easing = DesignerEasingKind::Sine;
				else return fail(L"Component animation key-frame easing is invalid.");
				const auto frameMode = frameElement->GetAttribute("easingMode");
				if (std::strcmp(frameMode.c_str(), "EaseIn") == 0)
					frame.EasingMode = DesignerEasingMode::EaseIn;
				else if (std::strcmp(frameMode.c_str(), "EaseOut") == 0)
					frame.EasingMode = DesignerEasingMode::EaseOut;
				else if (std::strcmp(frameMode.c_str(), "EaseInOut") == 0)
					frame.EasingMode = DesignerEasingMode::EaseInOut;
				else return fail(L"Component animation key-frame easing mode is invalid.");
			}
			else if (frame.Kind == DesignerKeyFrameKind::Spline)
			{
				if (!TryReadFloatAttribute(frameElement, "x1", frame.KeySplineX1)
					|| !TryReadFloatAttribute(frameElement, "y1", frame.KeySplineY1)
					|| !TryReadFloatAttribute(frameElement, "x2", frame.KeySplineX2)
					|| !TryReadFloatAttribute(frameElement, "y2", frame.KeySplineY2)
					|| frame.KeySplineX1 < 0.0f || frame.KeySplineX1 > 1.0f
					|| frame.KeySplineY1 < 0.0f || frame.KeySplineY1 > 1.0f
					|| frame.KeySplineX2 < 0.0f || frame.KeySplineX2 > 1.0f
					|| frame.KeySplineY2 < 0.0f || frame.KeySplineY2 > 1.0f)
					return fail(L"Component animation KeySpline is invalid.");
			}
			animation.KeyFrames.push_back(std::move(frame));
		}
		std::stable_sort(animation.KeyFrames.begin(), animation.KeyFrames.end(),
			[](const auto& left, const auto& right)
			{ return left.KeyTimeMilliseconds < right.KeyTimeMilliseconds; });
		if (animation.Kind == DesignerAnimationKind::Object
			&& (animation.KeyFrames.empty() || animation.HasFrom || animation.HasTo
				|| animation.HasBy || animation.IsAdditive || animation.IsCumulative
				|| animation.Easing != DesignerEasingKind::Linear
				|| std::any_of(animation.KeyFrames.begin(), animation.KeyFrames.end(),
					[](const auto& frame)
					{ return frame.Kind != DesignerKeyFrameKind::Discrete; })))
			return fail(L"Object animation must contain only discrete key frames and cannot declare endpoints, easing, additive, or cumulative behavior.");
		return true;
	}

	static void WriteStoryboardActionsSnapshot(
		XmlDocument& xml,
		const std::shared_ptr<XmlElement>& parent,
		const char* containerName,
		const std::vector<DesignerEventTriggerAction>& actions)
	{
		if (actions.empty()) return;
		auto container = AppendElement(xml, parent, containerName);
		for (const auto& action : actions)
		{
			const char* name = action.Kind == DesignerStoryboardActionKind::Begin
				? "beginStoryboard"
				: action.Kind == DesignerStoryboardActionKind::Pause
					? "pauseStoryboard"
				: action.Kind == DesignerStoryboardActionKind::Resume
					? "resumeStoryboard" : "stopStoryboard";
			auto item = AppendElement(xml, container, name);
			if (action.Kind == DesignerStoryboardActionKind::Begin)
			{
				if (!action.StoryboardName.empty())
					item->SetAttribute("name", ToUtf8(action.StoryboardName));
				for (const auto& animation : action.Animations)
					WriteVisualStateAnimationSnapshot(xml, item, animation);
			}
			else item->SetAttribute(
				"beginStoryboardName", ToUtf8(action.StoryboardName));
		}
	}

	static bool ReadStoryboardActionsSnapshot(
		const std::shared_ptr<XmlElement>& parent,
		const char* containerName,
		std::vector<DesignerEventTriggerAction>& actions,
		std::wstring* outError)
	{
		const auto container = FindChildElement(parent, containerName);
		if (!container) return true;
		for (const auto& child : container->ChildNodes())
		{
			if (!child || child->NodeType() != XmlNodeType::Element) continue;
			const auto element = std::static_pointer_cast<XmlElement>(child);
			DesignerEventTriggerAction action;
			if (std::strcmp(element->Name().c_str(), "beginStoryboard") == 0)
			{
				action.Kind = DesignerStoryboardActionKind::Begin;
				action.StoryboardName = FromUtf8(element->GetAttribute("name"));
				for (const auto& animationElement : FindChildElements(
					element, "animation"))
				{
					DesignerVisualStateAnimation animation;
					if (!ReadVisualStateAnimationSnapshot(
						animationElement, animation, outError)) return false;
					action.Animations.push_back(std::move(animation));
				}
				if (action.Animations.empty())
				{
					if (outError) *outError =
						L"Style BeginStoryboard has no animations.";
					return false;
				}
			}
			else
			{
				if (std::strcmp(element->Name().c_str(), "pauseStoryboard") == 0)
					action.Kind = DesignerStoryboardActionKind::Pause;
				else if (std::strcmp(element->Name().c_str(), "resumeStoryboard") == 0)
					action.Kind = DesignerStoryboardActionKind::Resume;
				else if (std::strcmp(element->Name().c_str(), "stopStoryboard") == 0)
					action.Kind = DesignerStoryboardActionKind::Stop;
				else
				{
					if (outError) *outError = L"Style TriggerAction is invalid.";
					return false;
				}
				action.StoryboardName = FromUtf8(
					element->GetAttribute("beginStoryboardName"));
				if (action.StoryboardName.empty())
				{
					if (outError) *outError =
						L"Style Storyboard control action has no name.";
					return false;
				}
			}
			actions.push_back(std::move(action));
		}
		if (actions.empty())
		{
			if (outError) *outError = L"Style action collection is empty.";
			return false;
		}
		return true;
	}

	static bool TryReadDoubleAttribute(
		const std::shared_ptr<XmlElement>& element,
		const char* name,
		double& out)
	{
		return element && element->HasAttribute(name)
			&& TryParseDouble(element->GetAttribute(name), out);
	}
}

bool DesignDocumentSerializer::SaveToFile(const DesignDocument& document, const std::wstring& filePath, std::wstring* outError)
{
	try
	{
		if (outError) outError->clear();
		if (filePath.empty())
		{
			if (outError) *outError = L"File path is empty.";
			return false;
		}
		if (!DesignerDataContextSchemaUtils::Validate(
			document.DataContextSchema, outError))
		{
			return false;
		}

		return AtomicFile::Write(filePath, ToXml(document), outError);
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"Save failed: " + FromUtf8(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"Save failed: unknown error.";
		return false;
	}
}

bool DesignDocumentSerializer::LoadFromFile(const std::wstring& filePath, DesignDocument& document, std::wstring* outError)
{
	try
	{
		if (filePath.empty())
		{
			if (outError) *outError = L"File path is empty.";
			return false;
		}

		std::ifstream f(filePath, std::ios::binary);
		if (!f.is_open())
		{
			if (outError) *outError = L"Failed to open file for reading.";
			return false;
		}

		std::stringstream ss;
		ss << f.rdbuf();
		std::string content = ss.str();
		size_t first = content.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			if (outError) *outError = L"Design file is empty.";
			return false;
		}

		if (content[first] != '<')
		{
			if (outError) *outError = L"Unsupported design file format. Please use XML design files.";
			return false;
		}

		const auto resourceBasePath =
			std::filesystem::absolute(std::filesystem::path(filePath))
			.parent_path().lexically_normal().wstring();
		if (!FromXml(content, document, outError, resourceBasePath)) return false;
		return true;
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"Load failed: " + FromUtf8(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"Load failed: unknown error.";
		return false;
	}
}

static void AppendStyleScope(
	DesignerStyleSheet& target,
	const DesignerStyleSheet& source)
{
	DesignerStyleSheetUtils::AppendLexicalScope(target, source);
}

static DesignerStyleSheet VisibleStyleScope(
	const DesignDocument& document,
	const std::vector<DesignNode>& nodes,
	const DesignNode& origin)
{
	DesignerStyleSheet result = document.StyleSheet;
	std::unordered_map<int, const DesignNode*> byId;
	std::unordered_map<std::wstring, const DesignNode*> byName;
	byId.reserve(nodes.size());
	byName.reserve(nodes.size());
	for (const auto& node : nodes)
	{
		byId.emplace(node.Id, &node);
		byName.emplace(node.Name, &node);
	}
	std::vector<const DesignNode*> route;
	std::unordered_set<int> visited;
	for (const DesignNode* current = &origin;
		current && visited.insert(current->Id).second;)
	{
		route.push_back(current);
		if (current->ParentId > 0)
		{
			const auto parent = byId.find(current->ParentId);
			current = parent == byId.end() ? nullptr : parent->second;
			continue;
		}
		if (current->ParentRef.empty()) break;
		const auto owner = byName.find(current->ParentRef);
		current = owner == byName.end() ? nullptr : owner->second;
	}
	for (auto current = route.rbegin(); current != route.rend(); ++current)
		AppendStyleScope(result, (*current)->LocalResources);
	return result;
}

static DesignerStyleSheet VisibleStyleScope(
	const DesignDocument& document,
	const DesignNode& origin)
{
	return VisibleStyleScope(document, document.Nodes, origin);
}

static void WriteLocalResourcesSnapshot(
	XmlDocument& xml,
	const std::shared_ptr<XmlElement>& control,
	const DesignDocument& document,
	const DesignNode& node)
{
	const auto& source = node.LocalResources;
	if (source.Empty() && node.LocalObjectResources.Empty()) return;
	auto sheet = source;
	DesignerStyleSheetUtils::Canonicalize(sheet);
	auto element = AppendElement(xml, control, "localResources");
	if (!sheet.MergedDictionaries.empty())
	{
		auto dictionaries = AppendElement(xml, element, "mergedDictionaries");
		for (const auto& sourceUri : sheet.MergedDictionaries)
		{
			auto item = AppendElement(xml, dictionaries, "dictionary");
			item->SetAttribute("source", ToUtf8(sourceUri));
		}
	}
	for (const auto& resource : sheet.Resources)
	{
		auto item = AppendElement(xml, element, "resource");
		item->SetAttribute("key", ToUtf8(resource.Key));
		item->SetAttribute("kind", ToUtf8(
			DesignerStyleSheetUtils::ValueKindName(resource.Value.Kind)));
		if (!resource.SourceDictionary.empty())
			item->SetAttribute("sourceDictionary",
				ToUtf8(resource.SourceDictionary));
		if (resource.Value.ObjectValue.is_null())
			item->SetInnerText(ToUtf8(resource.Value.Text));
		else
			WriteValue(xml, AppendElement(xml, item, "objectValue"),
				resource.Value.ObjectValue);
	}
	if (!sheet.Rules.empty())
	{
		DesignDocument snapshot;
		snapshot.Window.Name = L"LocalStyleScope";
		snapshot.StyleSheet = VisibleStyleScope(document, node);
		snapshot.Components = SnapshotComponents(document);
		auto rules = AppendElement(xml, element, "rulesSnapshot");
		rules->SetAttribute("localCount", std::to_string(sheet.Rules.size()));
		rules->SetInnerText(DesignDocumentSerializer::ToXml(snapshot));
	}
	if (!node.LocalObjectResources.Empty())
	{
		const auto snapshot = BuildLocalObjectResourceSnapshot(
			document, document.Nodes, node);
		auto objects = AppendElement(xml, element, "objectResourcesSnapshot");
		objects->SetAttribute("componentCount", std::to_string(
			node.LocalObjectResources.Components.size()));
		objects->SetAttribute("dataTemplateCount", std::to_string(
			node.LocalObjectResources.DataTemplates.size()));
		objects->SetAttribute("controlTemplateCount", std::to_string(
			node.LocalObjectResources.ControlTemplates.size()));
		objects->SetAttribute("itemsPanelTemplateCount", std::to_string(
			node.LocalObjectResources.ItemsPanelTemplates.size()));
		objects->SetAttribute("groupStyleCount", std::to_string(
			node.LocalObjectResources.GroupStyles.size()));
		objects->SetInnerText(DesignDocumentSerializer::ToXml(snapshot));
	}
}

static bool ReadLocalResourcesSnapshot(
	const std::shared_ptr<XmlElement>& control,
	DesignerStyleSheet& output,
	DesignObjectResourceDictionary& objectOutput,
	const std::wstring& resourceBasePath,
	const std::shared_ptr<ResourceLoadContext>& resources,
	std::wstring* outError)
{
	const auto element = FindChildElement(control, "localResources");
	if (!element) return true;
	DesignerStyleSheet candidate;
	if (auto dictionaries = FindChildElement(element, "mergedDictionaries"))
		for (const auto& item : FindChildElements(dictionaries, "dictionary"))
			candidate.MergedDictionaries.push_back(
				FromUtf8(item->GetAttribute("source")));
	for (const auto& item : FindChildElements(element, "resource"))
	{
		DesignerStyleResource resource;
		resource.Key = FromUtf8(item->GetAttribute("key"));
		resource.SourceDictionary = FromUtf8(
			item->GetAttribute("sourceDictionary"));
		if (!DesignerStyleSheetUtils::TryParseValueKind(
			FromUtf8(item->GetAttribute("kind")), resource.Value.Kind))
		{
			if (outError) *outError =
				L"控件局部资源包含无效的值类型。";
			return false;
		}
		if (const auto object = FindChildElement(item, "objectValue"))
		{
			if (!ReadValue(object, resource.Value.ObjectValue, outError))
				return false;
		}
		else resource.Value.Text = FromUtf8(item->InnerText());
		candidate.Resources.push_back(std::move(resource));
	}
	if (const auto rules = FindChildElement(element, "rulesSnapshot"))
	{
		size_t localCount = 0;
		try
		{
			localCount = static_cast<size_t>(std::stoull(
				rules->GetAttribute("localCount")));
		}
		catch (...)
		{
			if (outError) *outError = L"控件局部样式快照数量无效。";
			return false;
		}
		DesignDocument snapshot;
		if (!DesignDocumentSerializer::FromXml(
			rules->InnerText(), snapshot, outError, resourceBasePath)) return false;
		if (localCount > snapshot.StyleSheet.Rules.size())
		{
			if (outError) *outError = L"控件局部样式快照不完整。";
			return false;
		}
		candidate.Rules.assign(
			snapshot.StyleSheet.Rules.end() - localCount,
			snapshot.StyleSheet.Rules.end());
	}
	DesignObjectResourceDictionary objectCandidate;
	if (const auto objects = FindChildElement(
		element, "objectResourcesSnapshot"))
	{
		size_t componentCount = 0;
		size_t dataTemplateCount = 0;
		size_t controlTemplateCount = 0;
		size_t itemsPanelTemplateCount = 0;
		size_t groupStyleCount = 0;
		try
		{
			componentCount = static_cast<size_t>(std::stoull(
				objects->GetAttribute("componentCount")));
			dataTemplateCount = static_cast<size_t>(std::stoull(
				objects->GetAttribute("dataTemplateCount")));
			const auto controlTemplateCountText = objects->GetAttribute(
				"controlTemplateCount");
			if (!controlTemplateCountText.empty())
				controlTemplateCount = static_cast<size_t>(
					std::stoull(controlTemplateCountText));
			const auto itemsPanelCountText = objects->GetAttribute(
				"itemsPanelTemplateCount");
			const auto groupStyleCountText = objects->GetAttribute("groupStyleCount");
			if (!itemsPanelCountText.empty())
				itemsPanelTemplateCount = static_cast<size_t>(
					std::stoull(itemsPanelCountText));
			if (!groupStyleCountText.empty())
				groupStyleCount = static_cast<size_t>(std::stoull(groupStyleCountText));
		}
		catch (...)
		{
			if (outError) *outError = L"控件局部对象资源快照数量无效。";
			return false;
		}
		DesignDocument snapshot;
		if (!DesignDocumentSerializer::FromXml(
			objects->InnerText(), snapshot, outError, resourceBasePath)) return false;
		if (componentCount > snapshot.Components.size()
			|| controlTemplateCount > snapshot.ControlTemplates.size()
			|| dataTemplateCount > snapshot.DataTemplates.size()
			|| itemsPanelTemplateCount > snapshot.ItemsPanelTemplates.size()
			|| groupStyleCount > snapshot.GroupStyles.size())
		{
			if (outError) *outError = L"控件局部对象资源快照不完整。";
			return false;
		}
		objectCandidate.Components.assign(
			snapshot.Components.end() - componentCount,
			snapshot.Components.end());
		objectCandidate.DataTemplates.assign(
			snapshot.DataTemplates.end() - dataTemplateCount,
			snapshot.DataTemplates.end());
		objectCandidate.ControlTemplates.assign(
			snapshot.ControlTemplates.end() - controlTemplateCount,
			snapshot.ControlTemplates.end());
		objectCandidate.ItemsPanelTemplates.assign(
			snapshot.ItemsPanelTemplates.end() - itemsPanelTemplateCount,
			snapshot.ItemsPanelTemplates.end());
		objectCandidate.GroupStyles.assign(
			snapshot.GroupStyles.end() - groupStyleCount,
			snapshot.GroupStyles.end());
	}
	DesignerStyleSheetUtils::Canonicalize(candidate);
	auto values = candidate;
	values.Rules.clear();
	if (!DesignerStyleSheetUtils::Validate(
		values, outError, resourceBasePath, resources)) return false;
	output = std::move(candidate);
	objectOutput = std::move(objectCandidate);
	return true;
}

std::string DesignDocumentSerializer::ToXml(const DesignDocument& input)
{
	auto canonical = input;
	std::wstring dataResourceError;
	if (!DesignDataResourceUtils::ValidateAndCanonicalize(
		canonical, &dataResourceError))
		throw std::invalid_argument(ToUtf8(dataResourceError));
	const auto& document = canonical;
	std::wstring codeBehindError;
	if (!document.CodeBehind.Validate(&codeBehindError))
		throw std::invalid_argument(ToUtf8(codeBehindError));
	XmlDocument xml;
	xml.AppendChild(xml.CreateXmlDeclaration("1.0", "utf-8", ""));

	auto root = xml.CreateElement("designDocument");
	root->SetAttribute("schema", document.Schema);
	root->SetAttribute("version", std::to_string(DesignDocument::CurrentSchemaVersion));
	DesignDocumentGraph graph;
	std::wstring graphError;
	if (!DesignDocumentGraph::Build(document, graph, &graphError))
		throw std::invalid_argument(ToUtf8(graphError));
	DesignDocumentEventIndex eventIndex;
	if (!DesignDocumentEventIndex::Build(document, eventIndex, &graphError))
		throw std::invalid_argument(ToUtf8(graphError));
	if (!document.ValidateCommandTargetReferences(&graphError))
		throw std::invalid_argument(ToUtf8(graphError));
	for (const auto& resolved : graph.Nodes())
	{
		const auto& node = document.Nodes[resolved.SourceIndex];
		if (node.ParentRef != resolved.ParentKey)
			throw std::invalid_argument(
				"Design document parentId and parent name disagree");
	}
	root->SetAttribute("nextId", std::to_string(document.NextStableId));
	xml.AppendChild(root);

	auto window = AppendElement(xml, root, "window");
	window->SetAttribute("name", ToUtf8(document.Window.Name));
	window->SetAttribute("type", UIClassToString(document.Window.Type));
	window->SetAttribute("xamlNamespace",
		ToUtf8(document.Window.XamlType.NamespaceUri));
	window->SetAttribute("xamlName",
		ToUtf8(document.Window.XamlType.LocalName));
	WriteValue(xml, AppendElement(xml, window, "properties"),
		EncodeDesignNodeProperties(document.Window.Properties));
	WriteValue(xml, AppendElement(xml, window, "events"),
		EncodeDesignNodeEvents(document.Window.Events));
	WriteValue(xml, AppendElement(xml, window, "bindings"),
		EncodeDesignNodeBindings(document.Window.Bindings));
	WriteValue(xml, AppendElement(xml, window, "commandBindings"),
		EncodeDesignCommandBindings(document.Window.CommandBindings));
	WriteValue(xml, AppendElement(xml, window, "inputBindings"),
		EncodeDesignInputBindings(document.Window.InputBindings));

	if (!document.CodeBehind.Empty())
	{
		auto codeBehind = AppendElement(xml, root, "codeBehind");
		codeBehind->SetAttribute("class", ToUtf8(document.CodeBehind.ClassName));
		if (!document.CodeBehind.RelativeBasePath.empty())
			codeBehind->SetAttribute("relativeBasePath",
				ToUtf8(document.CodeBehind.RelativeBasePath));
	}

	if (!document.DataContextSchema.empty())
	{
		auto schema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(schema);
		auto dataContext = AppendElement(xml, root, "dataContext");
		for (const auto& property : schema)
		{
			auto item = AppendElement(xml, dataContext, "property");
			item->SetAttribute("path", ToUtf8(
				DesignerDataContextSchemaUtils::NormalizePath(property.Path)));
			item->SetAttribute("kind", ToUtf8(
				DesignerDataContextSchemaUtils::ValueKindName(property.ValueKind)));
			if (property.ValueKind == BindingValueKind::Object)
				item->SetAttribute("objectType", ToUtf8(
					DesignerDataContextSchemaUtils::ObjectKindName(property.ObjectKind)));
			if (!property.ItemType.empty())
				item->SetAttribute("itemType", ToUtf8(property.ItemType));
			if (!property.DataType.empty())
				item->SetAttribute("dataType", ToUtf8(property.DataType));
			item->SetAttribute("read", BoolToString(property.CanRead));
			item->SetAttribute("write", BoolToString(property.CanWrite));
			item->SetAttribute("observe", BoolToString(property.CanObserve));
		}
	}

	if (!document.DataTypes.empty())
	{
		auto dataTypes = AppendElement(xml, root, "dataTypes");
		for (const auto& definition : document.DataTypes)
		{
			auto type = AppendElement(xml, dataTypes, "dataType");
			type->SetAttribute("name", ToUtf8(definition.Name));
			if (!definition.SourceDictionary.empty())
				type->SetAttribute("sourceDictionary",
					ToUtf8(definition.SourceDictionary));
			auto properties = definition.Properties;
			DesignerDataContextSchemaUtils::Canonicalize(properties);
			for (const auto& property : properties)
			{
				auto item = AppendElement(xml, type, "property");
				item->SetAttribute("path", ToUtf8(
					DesignerDataContextSchemaUtils::NormalizePath(property.Path)));
				item->SetAttribute("kind", ToUtf8(
					DesignerDataContextSchemaUtils::ValueKindName(property.ValueKind)));
				if (property.ValueKind == BindingValueKind::Object)
					item->SetAttribute("objectType", ToUtf8(
						DesignerDataContextSchemaUtils::ObjectKindName(property.ObjectKind)));
				if (!property.ItemType.empty())
					item->SetAttribute("itemType", ToUtf8(property.ItemType));
				if (!property.DataType.empty())
					item->SetAttribute("dataType", ToUtf8(property.DataType));
				item->SetAttribute("read", BoolToString(property.CanRead));
				item->SetAttribute("write", BoolToString(property.CanWrite));
				item->SetAttribute("observe", BoolToString(property.CanObserve));
			}
		}
	}

	if (!document.DataLists.empty())
	{
		auto dataLists = AppendElement(xml, root, "dataLists");
		for (const auto& definition : document.DataLists)
		{
			auto list = AppendElement(xml, dataLists, "dataList");
			list->SetAttribute("key", ToUtf8(definition.Key));
			list->SetAttribute("itemType", ToUtf8(definition.ItemType));
			if (!definition.SourceDictionary.empty())
				list->SetAttribute("sourceDictionary",
					ToUtf8(definition.SourceDictionary));
			for (const auto& record : definition.Records)
			{
				auto item = AppendElement(xml, list, "record");
				for (const auto& [path, value] : record.Fields)
				{
					auto field = AppendElement(xml, item, "field");
					field->SetAttribute("path", ToUtf8(path));
					field->SetAttribute("value", ToUtf8(value));
				}
			}
		}
	}

	if (!document.CollectionViews.empty())
	{
		auto views = AppendElement(xml, root, "collectionViews");
		for (const auto& definition : document.CollectionViews)
		{
			auto view = AppendElement(xml, views, "collectionView");
			view->SetAttribute("key", ToUtf8(definition.Key));
			if (!definition.SourceResource.empty())
				view->SetAttribute("sourceResource", ToUtf8(definition.SourceResource));
			if (!definition.SourceBindingPath.empty())
				view->SetAttribute("sourceBindingPath", ToUtf8(definition.SourceBindingPath));
			if (!definition.SourceDictionary.empty())
				view->SetAttribute("sourceDictionary", ToUtf8(definition.SourceDictionary));
			for (const auto& authored : definition.GroupDescriptions)
			{
				auto item = AppendElement(xml, view, "group");
				item->SetAttribute("property", ToUtf8(authored.PropertyName));
				item->SetAttribute("direction", authored.Direction
					== CollectionSortDirection::Descending ? "descending" : "ascending");
				item->SetAttribute("ignoreCase", BoolToString(authored.IgnoreCase));
			}
			for (const auto& authored : definition.AggregateDescriptions)
			{
				auto item = AppendElement(xml, view, "aggregate");
				item->SetAttribute("name", ToUtf8(authored.Name));
				item->SetAttribute("property", ToUtf8(authored.PropertyName));
				item->SetAttribute("function", std::to_string(
					static_cast<int>(authored.Function)));
			}
			for (const auto& authored : definition.SortDescriptions)
			{
				auto item = AppendElement(xml, view, "sort");
				item->SetAttribute("property", ToUtf8(authored.PropertyName));
				item->SetAttribute("direction", authored.Direction
					== CollectionSortDirection::Descending ? "descending" : "ascending");
				item->SetAttribute("ignoreCase", BoolToString(authored.IgnoreCase));
			}
			for (const auto& authored : definition.FilterDescriptions)
			{
				auto item = AppendElement(xml, view, "filter");
				item->SetAttribute("property", ToUtf8(authored.PropertyName));
				item->SetAttribute("operator", std::to_string(
					static_cast<int>(authored.Operator)));
				item->SetAttribute("value", ToUtf8(authored.Value));
				item->SetAttribute("ignoreCase", BoolToString(authored.IgnoreCase));
			}
		}
	}

	if (!document.ItemsPanelTemplates.empty())
	{
		auto templates = AppendElement(xml, root, "itemsPanelTemplates");
		for (const auto& definition : document.ItemsPanelTemplates)
		{
			auto item = AppendElement(xml, templates, "itemsPanelTemplate");
			item->SetAttribute("key", ToUtf8(definition.Key));
			item->SetAttribute("kind",
				definition.Value.Kind == ItemsPanelKind::Wrap ? "wrap"
				: definition.Value.Kind == ItemsPanelKind::VirtualizingStack
					? "virtualizingStack" : "stack");
			item->SetAttribute("orientation",
				definition.Value.Orientation == Orientation::Horizontal
					? "horizontal" : "vertical");
			item->SetAttribute("itemWidth", FloatText(definition.Value.ItemWidth));
			item->SetAttribute("itemHeight", FloatText(definition.Value.ItemHeight));
			item->SetAttribute("cacheLength", FloatText(definition.Value.CacheLength));
			if (!definition.SourceDictionary.empty())
				item->SetAttribute("sourceDictionary",
					ToUtf8(definition.SourceDictionary));
		}
	}

	if (!document.DataTemplates.empty())
	{
		auto dataTemplates = AppendElement(xml, root, "dataTemplates");
		for (const auto& definition : document.DataTemplates)
		{
			auto item = AppendElement(xml, dataTemplates, "dataTemplate");
			if (!definition.IsImplicit())
				item->SetAttribute("key", ToUtf8(definition.Key));
			item->SetAttribute("dataType", ToUtf8(definition.DataType));
			if (definition.Hierarchical)
				item->SetAttribute("hierarchical", "true");
			if (!definition.SourceDictionary.empty())
				item->SetAttribute("sourceDictionary",
					ToUtf8(definition.SourceDictionary));
			DesignValue templateValue = DesignValue::array();
			for (const auto& node : definition.Template)
				templateValue.push_back(TemplateNodeToValue(
					node, document, definition.Template));
			WriteValue(xml, AppendElement(xml, item, "template"),
				templateValue);
			if (definition.ItemsSourceBinding)
				WriteValue(xml, AppendElement(xml, item, "itemsSource"),
					DesignerBindingUtils::WriteBindingDefinition(
						*definition.ItemsSourceBinding));
		}
	}

	if (!document.ControlTemplates.empty())
	{
		DesignDocument snapshot = document;
		snapshot.Window.Name = L"ControlTemplateResourceScope";
		snapshot.Window.Events.clear();
		snapshot.CodeBehind = {};
		snapshot.Nodes.clear();
		snapshot.RecalculateNextStableId();
		auto templates = AppendElement(xml, root, "controlTemplatesSnapshot");
		templates->SetInnerText(
			XamlDocumentSerializer::ToXaml(snapshot));
	}

	if (!document.GroupStyles.empty())
	{
		auto styles = AppendElement(xml, root, "groupStyles");
		for (const auto& definition : document.GroupStyles)
		{
			auto item = AppendElement(xml, styles, "groupStyle");
			item->SetAttribute("key", ToUtf8(definition.Key));
			item->SetAttribute("headerTemplate", ToUtf8(definition.HeaderTemplate));
			if (!definition.SourceDictionary.empty())
				item->SetAttribute("sourceDictionary",
					ToUtf8(definition.SourceDictionary));
		}
	}

	if (!document.StyleSheet.Empty())
	{
		auto styleSheet = document.StyleSheet;
		DesignerStyleSheetUtils::Canonicalize(styleSheet);
		auto styleSheetElement = AppendElement(xml, root, "styleSheet");
		if (!styleSheet.MergedDictionaries.empty())
		{
			auto dictionaries = AppendElement(
				xml, styleSheetElement, "mergedDictionaries");
			for (const auto& source : styleSheet.MergedDictionaries)
			{
				auto item = AppendElement(xml, dictionaries, "dictionary");
				item->SetAttribute("source", ToUtf8(source));
			}
		}
		if (!styleSheet.Resources.empty())
		{
			auto resources = AppendElement(xml, styleSheetElement, "resources");
			for (const auto& resource : styleSheet.Resources)
			{
				auto item = AppendElement(xml, resources, "resource");
				item->SetAttribute("key", ToUtf8(resource.Key));
				item->SetAttribute("kind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(resource.Value.Kind)));
				if (!resource.SourceDictionary.empty())
					item->SetAttribute("sourceDictionary", ToUtf8(
						resource.SourceDictionary));
				if (resource.Value.ObjectValue.is_null())
					item->SetInnerText(ToUtf8(resource.Value.Text));
				else
					WriteValue(xml, AppendElement(xml, item, "objectValue"),
						resource.Value.ObjectValue);
			}
		}
		if (!styleSheet.Rules.empty())
		{
			auto rules = AppendElement(xml, styleSheetElement, "rules");
			for (const auto& rule : styleSheet.Rules)
			{
				auto item = AppendElement(xml, rules, "rule");
				if (rule.HasType)
					item->SetAttribute("type", ToUtf8(
						DesignerStyleSheetUtils::UIClassName(rule.Type)));
				if (rule.XamlType.Valid())
				{
					item->SetAttribute("xamlNamespace", ToUtf8(
						rule.XamlType.NamespaceUri));
					item->SetAttribute("xamlName", ToUtf8(
						rule.XamlType.LocalName));
				}
				if (!rule.ComponentType.Empty())
				{
					item->SetAttribute("componentPrefix", ToUtf8(
						rule.ComponentType.XamlPrefix));
					item->SetAttribute("componentName", ToUtf8(
						rule.ComponentType.XamlName));
					item->SetAttribute("componentNamespace", ToUtf8(
						rule.ComponentType.XamlNamespace));
				}
				if (!rule.Id.empty()) item->SetAttribute("id", ToUtf8(rule.Id));
				if (!rule.BasedOn.empty())
					item->SetAttribute("basedOn", ToUtf8(rule.BasedOn));
				if (!rule.SourceDictionary.empty())
					item->SetAttribute("sourceDictionary", ToUtf8(
						rule.SourceDictionary));
				for (const auto& setter : rule.Setters)
				{
					auto setterElement = AppendElement(xml, item, "setter");
					setterElement->SetAttribute("property", ToUtf8(setter.PropertyName));
					if (setter.UsesResource)
					{
						setterElement->SetAttribute("resource", ToUtf8(setter.ResourceKey));
						if (setter.UsesDynamicResource)
							setterElement->SetAttribute("dynamicResource", "true");
					}
					else
					{
						setterElement->SetAttribute("kind", ToUtf8(
							DesignerStyleSheetUtils::ValueKindName(setter.Literal.Kind)));
						if (setter.Literal.ObjectValue.is_null())
							setterElement->SetInnerText(ToUtf8(setter.Literal.Text));
						else
							WriteValue(xml, AppendElement(xml, setterElement, "objectValue"),
								setter.Literal.ObjectValue);
					}
				}
				if (!rule.Triggers.empty())
				{
					auto triggersElement = AppendElement(xml, item, "triggers");
					for (const auto& trigger : rule.Triggers)
					{
						auto triggerElement = AppendElement(
							xml, triggersElement, "trigger");
						if (!trigger.DataConditions.empty())
						{
							auto conditionsElement = AppendElement(
								xml, triggerElement, "dataConditions");
							for (const auto& condition : trigger.DataConditions)
							{
								auto conditionElement = AppendElement(
									xml, conditionsElement, "condition");
								conditionElement->SetAttribute("source", ToUtf8(
									condition.SourceProperty));
								conditionElement->SetAttribute("kind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(
										condition.Value.Kind)));
								if (condition.Value.ObjectValue.is_null())
									conditionElement->SetInnerText(ToUtf8(
										condition.Value.Text));
								else
									WriteValue(xml, AppendElement(xml,
										conditionElement, "objectValue"),
										condition.Value.ObjectValue);
							}
						}
						if (!trigger.PropertyConditions.empty())
						{
							auto conditionsElement = AppendElement(
								xml, triggerElement, "propertyConditions");
							for (const auto& condition : trigger.PropertyConditions)
							{
								auto conditionElement = AppendElement(
									xml, conditionsElement, "condition");
								conditionElement->SetAttribute("property",
									ToUtf8(condition.Property));
								conditionElement->SetAttribute("kind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(
										condition.Value.Kind)));
								conditionElement->SetInnerText(ToUtf8(
									condition.Value.Text));
							}
						}
						for (const auto& setter : trigger.Setters)
						{
							auto setterElement = AppendElement(
								xml, triggerElement, "setter");
							setterElement->SetAttribute(
								"property", ToUtf8(setter.PropertyName));
							if (setter.UsesResource)
							{
								setterElement->SetAttribute(
									"resource", ToUtf8(setter.ResourceKey));
								if (setter.UsesDynamicResource)
									setterElement->SetAttribute(
										"dynamicResource", "true");
							}
							else
							{
								setterElement->SetAttribute("kind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(
										setter.Literal.Kind)));
								if (setter.Literal.ObjectValue.is_null())
									setterElement->SetInnerText(
										ToUtf8(setter.Literal.Text));
								else
									WriteValue(xml, AppendElement(
										xml, setterElement, "objectValue"),
										setter.Literal.ObjectValue);
							}
						}
						WriteStoryboardActionsSnapshot(
							xml, triggerElement, "enterActions",
							trigger.EnterActions);
						WriteStoryboardActionsSnapshot(
							xml, triggerElement, "exitActions",
							trigger.ExitActions);
					}
				}
			}
		}
	}

	if (!document.Components.empty())
	{
		auto components = AppendElement(xml, root, "components");
		for (const auto& definition : document.Components)
		{
			auto component = AppendElement(xml, components, "component");
			component->SetAttribute("prefix", ToUtf8(definition.Type.XamlPrefix));
			component->SetAttribute("name", ToUtf8(definition.Type.XamlName));
			component->SetAttribute("namespace", ToUtf8(definition.Type.XamlNamespace));
			component->SetAttribute("baseType", UIClassToString(definition.BaseType));
			component->SetAttribute("displayName", ToUtf8(definition.DisplayName));
			component->SetAttribute("category", ToUtf8(definition.Category));
			if (!definition.SourceDictionary.empty())
				component->SetAttribute("sourceDictionary",
					ToUtf8(definition.SourceDictionary));
			for (const auto& property : definition.Properties)
			{
				auto item = AppendElement(xml, component, "property");
				item->SetAttribute("name", ToUtf8(property.Name));
				item->SetAttribute("displayName", ToUtf8(property.DisplayName));
				item->SetAttribute("category", ToUtf8(property.Category));
				item->SetAttribute("categoryOrder", std::to_string(property.CategoryOrder));
				item->SetAttribute("order", std::to_string(property.Order));
				item->SetAttribute("kind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(property.DefaultValue.Kind)));
				if (!property.DefaultResourceKey.empty())
					item->SetAttribute("defaultResource",
						ToUtf8(property.DefaultResourceKey));
				if (property.DefaultValue.ObjectValue.is_null())
					item->SetAttribute("default", ToUtf8(property.DefaultValue.Text));
				else
					WriteValue(xml, AppendElement(xml, item, "objectValue"),
						property.DefaultValue.ObjectValue);
				item->SetAttribute("editor", std::to_string(
					static_cast<int>(property.Editor)));
				item->SetAttribute("flags", std::to_string(
					static_cast<unsigned int>(property.Flags)));
				item->SetAttribute("defaultUpdateMode", std::to_string(
					static_cast<int>(property.DefaultUpdateMode)));
				item->SetAttribute("readOnly",
					property.IsReadOnly ? "1" : "0");
				if (property.Minimum)
					item->SetAttribute("minimum", std::to_string(*property.Minimum));
				if (property.Maximum)
					item->SetAttribute("maximum", std::to_string(*property.Maximum));
				if (property.Step)
					item->SetAttribute("step", std::to_string(*property.Step));
				for (const auto& choice : property.Choices)
				{
					auto value = AppendElement(xml, item, "choice");
					value->SetAttribute("value", ToUtf8(choice.Value));
					value->SetAttribute("displayName", ToUtf8(choice.DisplayName));
				}
			}
			for (const auto& property : definition.ContentProperties)
			{
				auto item = AppendElement(xml, component, "contentProperty");
				item->SetAttribute("name", ToUtf8(property.Name));
				item->SetAttribute("displayName", ToUtf8(property.DisplayName));
				item->SetAttribute("cardinality",
					property.Cardinality == DesignerComponentContentCardinality::Multiple
						? "multiple" : "single");
				item->SetAttribute("default", property.IsDefault ? "true" : "false");
			}
			for (const auto& eventContract : definition.Events)
			{
				auto item = AppendElement(xml, component, "event");
				item->SetAttribute("name", ToUtf8(eventContract.Name));
				item->SetAttribute("displayName", ToUtf8(
					eventContract.DisplayName));
				item->SetAttribute("category",
					DesignerEventCatalog::GetCategoryName(eventContract.Category));
				item->SetAttribute("payload",
					DesignerEventCatalog::GetComponentPayloadName(
						eventContract.Payload));
				if (eventContract.RoutingStrategy
					!= DeclarativeEventRoutingStrategy::Direct)
					item->SetAttribute("routingStrategy",
						DesignerEventCatalog::GetComponentRoutingStrategyName(
							eventContract.RoutingStrategy));
				item->SetAttribute("order", std::to_string(eventContract.Order));
				item->SetAttribute("default",
					eventContract.IsDefault ? "true" : "false");
			}
			for (const auto& group : definition.VisualStateGroups)
			{
				auto groupElement = AppendElement(
					xml, component, "visualStateGroup");
				groupElement->SetAttribute("name", ToUtf8(group.Name));
				for (const auto& transition : group.Transitions)
				{
					auto item = AppendElement(xml, groupElement, "visualTransition");
					if (!transition.FromState.empty())
						item->SetAttribute("from", ToUtf8(transition.FromState));
					if (!transition.ToState.empty())
						item->SetAttribute("to", ToUtf8(transition.ToState));
					item->SetAttribute("generatedDurationMs", std::to_string(
						transition.GeneratedDurationMilliseconds));
					const char* easing = "Linear";
					switch (transition.GeneratedEasing)
					{
					case DesignerEasingKind::Quadratic: easing = "Quadratic"; break;
					case DesignerEasingKind::Cubic: easing = "Cubic"; break;
					case DesignerEasingKind::Sine: easing = "Sine"; break;
					case DesignerEasingKind::Linear:
					default: break;
					}
					item->SetAttribute("generatedEasing", easing);
					item->SetAttribute("generatedEasingMode",
						transition.GeneratedEasingMode == DesignerEasingMode::EaseIn
							? "EaseIn"
							: transition.GeneratedEasingMode
								== DesignerEasingMode::EaseInOut
								? "EaseInOut" : "EaseOut");
					for (const auto& animation : transition.Animations)
						WriteVisualStateAnimationSnapshot(xml, item, animation);
				}
				for (const auto& state : group.States)
				{
					auto stateElement = AppendElement(
						xml, groupElement, "visualState");
					stateElement->SetAttribute("name", ToUtf8(state.Name));
					for (const auto& condition : state.Conditions)
					{
						auto item = AppendElement(xml, stateElement, "condition");
						item->SetAttribute("property", ToUtf8(
							condition.PropertyName));
						item->SetAttribute("kind", ToUtf8(
							DesignerStyleSheetUtils::ValueKindName(
								condition.Value.Kind)));
						if (condition.Value.ObjectValue.is_null())
							item->SetInnerText(ToUtf8(condition.Value.Text));
						else WriteValue(xml, AppendElement(
							xml, item, "objectValue"), condition.Value.ObjectValue);
					}
					for (const auto& eventName : state.EventNames)
					{
						auto item = AppendElement(xml, stateElement, "eventTrigger");
						item->SetAttribute("event", ToUtf8(eventName));
					}
					for (const auto& setter : state.Setters)
					{
						auto item = AppendElement(xml, stateElement, "setter");
						if (!setter.TargetName.empty())
							item->SetAttribute("target", ToUtf8(setter.TargetName));
						item->SetAttribute("property", ToUtf8(setter.PropertyName));
						if (setter.UsesResource)
						{
							item->SetAttribute("resource", ToUtf8(setter.ResourceKey));
							item->SetAttribute("fallbackKind", ToUtf8(
								DesignerStyleSheetUtils::ValueKindName(
									setter.Literal.Kind)));
							if (setter.Literal.ObjectValue.is_null())
								item->SetInnerText(ToUtf8(setter.Literal.Text));
							else WriteValue(xml, AppendElement(
								xml, item, "fallbackObjectValue"),
								setter.Literal.ObjectValue);
						}
						else
						{
							item->SetAttribute("kind", ToUtf8(
							DesignerStyleSheetUtils::ValueKindName(
								setter.Literal.Kind)));
							if (setter.Literal.ObjectValue.is_null())
								item->SetInnerText(ToUtf8(setter.Literal.Text));
							else WriteValue(xml, AppendElement(
								xml, item, "objectValue"), setter.Literal.ObjectValue);
						}
					}
					for (const auto& animation : state.Animations)
					{
						if (animation.Kind == DesignerAnimationKind::Double
							|| animation.Kind == DesignerAnimationKind::Color
							|| animation.Kind == DesignerAnimationKind::Object
							|| animation.Kind == DesignerAnimationKind::Thickness
							|| animation.Kind == DesignerAnimationKind::Point
							|| animation.Kind == DesignerAnimationKind::Vector
							|| animation.Kind == DesignerAnimationKind::Rect
							|| animation.Kind == DesignerAnimationKind::Size
							|| animation.Kind == DesignerAnimationKind::Matrix)
						{
							WriteVisualStateAnimationSnapshot(xml, stateElement, animation);
							continue;
						}
						auto item = AppendElement(xml, stateElement, "animation");
						item->SetAttribute("type",
							animation.Kind == DesignerAnimationKind::Color
								? "Color" : "Double");
						if (!animation.TargetName.empty())
							item->SetAttribute("target", ToUtf8(animation.TargetName));
						item->SetAttribute("property", ToUtf8(animation.PropertyName));
						if (animation.KeyFrames.empty() && animation.HasFrom)
						{
							if (animation.FromUsesResource)
								item->SetAttribute("fromResource",
									ToUtf8(animation.FromResourceKey));
							else
							{
								item->SetAttribute("fromKind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(
										animation.From.Kind)));
								item->SetAttribute("from", ToUtf8(animation.From.Text));
							}
						}
						if (animation.KeyFrames.empty() && animation.HasTo)
						{
							if (animation.ToUsesResource)
								item->SetAttribute("toResource",
									ToUtf8(animation.ToResourceKey));
							else
							{
								item->SetAttribute("toKind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(animation.To.Kind)));
								item->SetAttribute("to", ToUtf8(animation.To.Text));
							}
						}
						if (animation.KeyFrames.empty() && animation.HasBy)
						{
							if (animation.ByUsesResource)
								item->SetAttribute("byResource",
									ToUtf8(animation.ByResourceKey));
							else
							{
								item->SetAttribute("byKind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(
										animation.By.Kind)));
								item->SetAttribute("by", ToUtf8(animation.By.Text));
							}
						}
						item->SetAttribute("beginTimeMs", std::to_string(
							animation.BeginTimeMilliseconds));
						item->SetAttribute("durationMs", std::to_string(
							animation.DurationMilliseconds));
						item->SetAttribute("repeatBehavior",
							animation.RepeatBehavior
								== DesignerRepeatBehaviorKind::Duration
								? "Duration"
								: animation.RepeatBehavior
									== DesignerRepeatBehaviorKind::Forever
									? "Forever" : "Count");
						item->SetAttribute("repeatCount",
							DoubleText(animation.RepeatCount));
						item->SetAttribute("repeatDurationMs", std::to_string(
							animation.RepeatDurationMilliseconds));
						item->SetAttribute("autoReverse",
							animation.AutoReverse ? "true" : "false");
						item->SetAttribute("isAdditive",
							animation.IsAdditive ? "true" : "false");
						item->SetAttribute("isCumulative",
							animation.IsCumulative ? "true" : "false");
						item->SetAttribute("fillBehavior",
							animation.FillBehavior
								== DesignerTimelineFillBehavior::Stop
								? "Stop" : "HoldEnd");
						item->SetAttribute("speedRatio",
							DoubleText(animation.SpeedRatio));
						item->SetAttribute("accelerationRatio",
							DoubleText(animation.AccelerationRatio));
						item->SetAttribute("decelerationRatio",
							DoubleText(animation.DecelerationRatio));
						const char* easing = "Linear";
						switch (animation.Easing)
						{
						case DesignerEasingKind::Quadratic: easing = "Quadratic"; break;
						case DesignerEasingKind::Cubic: easing = "Cubic"; break;
						case DesignerEasingKind::Sine: easing = "Sine"; break;
						case DesignerEasingKind::Linear:
						default: break;
						}
						item->SetAttribute("easing", easing);
						const char* easingMode = "EaseOut";
						switch (animation.EasingMode)
						{
						case DesignerEasingMode::EaseIn: easingMode = "EaseIn"; break;
						case DesignerEasingMode::EaseInOut: easingMode = "EaseInOut"; break;
						case DesignerEasingMode::EaseOut:
						default: break;
						}
						item->SetAttribute("easingMode", easingMode);
						for (const auto& keyFrame : animation.KeyFrames)
						{
							auto frame = AppendElement(xml, item, "keyFrame");
							const char* kind = "Linear";
							switch (keyFrame.Kind)
							{
							case DesignerKeyFrameKind::Discrete: kind = "Discrete"; break;
							case DesignerKeyFrameKind::Easing: kind = "Easing"; break;
							case DesignerKeyFrameKind::Spline: kind = "Spline"; break;
							case DesignerKeyFrameKind::Linear:
							default: break;
							}
							frame->SetAttribute("kind", kind);
							frame->SetAttribute("keyTimeMs", std::to_string(
								keyFrame.KeyTimeMilliseconds));
							if (keyFrame.UsesResource)
								frame->SetAttribute("resource", ToUtf8(keyFrame.ResourceKey));
							else
							{
								frame->SetAttribute("valueKind", ToUtf8(
									DesignerStyleSheetUtils::ValueKindName(keyFrame.Value.Kind)));
								frame->SetAttribute("value", ToUtf8(keyFrame.Value.Text));
							}
							if (keyFrame.Kind == DesignerKeyFrameKind::Easing)
							{
								const char* frameEasing = "Linear";
								switch (keyFrame.Easing)
								{
								case DesignerEasingKind::Quadratic: frameEasing = "Quadratic"; break;
								case DesignerEasingKind::Cubic: frameEasing = "Cubic"; break;
								case DesignerEasingKind::Sine: frameEasing = "Sine"; break;
								case DesignerEasingKind::Linear:
								default: break;
								}
								frame->SetAttribute("easing", frameEasing);
								const char* frameMode = keyFrame.EasingMode
									== DesignerEasingMode::EaseIn ? "EaseIn"
									: keyFrame.EasingMode == DesignerEasingMode::EaseInOut
										? "EaseInOut" : "EaseOut";
								frame->SetAttribute("easingMode", frameMode);
							}
							else if (keyFrame.Kind == DesignerKeyFrameKind::Spline)
							{
								frame->SetAttribute("x1", FloatText(keyFrame.KeySplineX1));
								frame->SetAttribute("y1", FloatText(keyFrame.KeySplineY1));
								frame->SetAttribute("x2", FloatText(keyFrame.KeySplineX2));
								frame->SetAttribute("y2", FloatText(keyFrame.KeySplineY2));
							}
						}
					}
				}
			}
			for (const auto& trigger : definition.EventTriggers)
			{
				auto triggerElement = AppendElement(
					xml, component, "componentEventTrigger");
				triggerElement->SetAttribute("event", ToUtf8(trigger.EventName));
				for (const auto& action : trigger.Actions)
				{
					const char* actionName = action.Kind
						== DesignerStoryboardActionKind::Begin
						? "beginStoryboard"
						: action.Kind == DesignerStoryboardActionKind::Pause
							? "pauseStoryboard"
							: action.Kind == DesignerStoryboardActionKind::Resume
								? "resumeStoryboard" : "stopStoryboard";
					auto actionElement = AppendElement(
						xml, triggerElement, actionName);
					if (action.Kind == DesignerStoryboardActionKind::Begin)
					{
						if (!action.StoryboardName.empty())
							actionElement->SetAttribute(
								"name", ToUtf8(action.StoryboardName));
						for (const auto& animation : action.Animations)
							WriteVisualStateAnimationSnapshot(
								xml, actionElement, animation);
					}
					else actionElement->SetAttribute(
						"beginStoryboardName", ToUtf8(action.StoryboardName));
				}
			}
			if (!definition.Template.empty())
			{
				DesignValue templateValue = DesignValue::array();
				for (auto& node : definition.Template)
					templateValue.push_back(TemplateNodeToValue(
						node, document, definition.Template));
				WriteValue(xml, AppendElement(xml, component, "template"),
					templateValue);
			}
		}
	}

	auto controls = AppendElement(xml, root, "controls");
	for (const auto& node : document.Nodes)
	{
		auto control = AppendElement(xml, controls, "control");
		control->SetAttribute("id", std::to_string(node.Id));
		control->SetAttribute("name", ToUtf8(node.Name));
		control->SetAttribute("type", UIClassToString(node.Type));
		if (node.XamlType.Valid())
		{
			control->SetAttribute("xamlNamespace",
				ToUtf8(node.XamlType.NamespaceUri));
			control->SetAttribute("xamlName", ToUtf8(node.XamlType.LocalName));
		}
		if (!node.ComponentType.Empty())
		{
			control->SetAttribute("componentPrefix",
				ToUtf8(node.ComponentType.XamlPrefix));
			control->SetAttribute("componentName",
				ToUtf8(node.ComponentType.XamlName));
			control->SetAttribute("componentNamespace",
				ToUtf8(node.ComponentType.XamlNamespace));
		}
		if (!node.ComponentContentProperty.empty())
			control->SetAttribute("componentContentProperty",
				ToUtf8(node.ComponentContentProperty));
		if (!node.PresentedComponentContent.empty())
			control->SetAttribute("presentedComponentContent",
				ToUtf8(node.PresentedComponentContent));
		control->SetAttribute("order", std::to_string(node.Order));
		if (node.Locked) control->SetAttribute("locked", "true");
		if (node.ParentId > 0)
		{
			control->SetAttribute("parentId", std::to_string(node.ParentId));
		}
		if (!node.ParentRef.empty())
		{
			control->SetAttribute("parent", ToUtf8(node.ParentRef));
		}

		WriteValue(xml, AppendElement(xml, control, "properties"),
			EncodeDesignNodeProperties(node.Properties));
		WriteValue(xml, AppendElement(xml, control, "structure"),
			EncodeDesignNodeStructure(node.Type, node.Structure));
		WriteValue(xml, AppendElement(xml, control, "events"),
			EncodeDesignNodeEvents(node.Events));
		WriteValue(xml, AppendElement(xml, control, "bindings"),
			EncodeDesignNodeBindings(node.Bindings));
		WriteValue(xml, AppendElement(xml, control, "commandBindings"),
			EncodeDesignCommandBindings(node.CommandBindings));
		WriteValue(xml, AppendElement(xml, control, "inputBindings"),
			EncodeDesignInputBindings(node.InputBindings));
		WriteLocalResourcesSnapshot(xml, control, document, node);
	}

	XmlWriterSettings settings;
	settings.Indent = true;
	settings.Encoding = "utf-8";
	return xml.ToString(settings);
}

bool DesignDocumentSerializer::FromXml(
	const std::string& xmlText,
	DesignDocument& output,
	std::wstring* outError,
	const std::wstring& resourceBasePath)
{
	XmlDocument xml;
	xml.LoadXml(xmlText);

	auto root = xml.DocumentElement();
	if (!root || root->Name() != "designDocument")
	{
		if (outError) *outError = L"Invalid CUI Designer XML file: missing root element.";
		return false;
	}
	if (!ValidateElementShape(
		root,
		{ "schema", "version", "nextId" },
		{ "window", "codeBehind", "dataContext", "dataTypes", "dataLists",
			"collectionViews", "itemsPanelTemplates", "dataTemplates",
			"controlTemplatesSnapshot", "groupStyles", "styleSheet",
			"components", "controls" },
		L"Current design snapshot", outError))
	{
		return false;
	}

	if (root->GetAttribute("schema") != "cui.designer")
	{
		if (outError) *outError = L"Invalid CUI Designer file: schema mismatch.";
		return false;
	}

	int version = 0;
	if (!TryReadIntegralAttribute(root, "version", version)
		|| version != DesignDocument::CurrentSchemaVersion)
	{
		if (outError) *outError = L"Only the current design snapshot schema is supported.";
		return false;
	}

	int persistedNextId = 1;
	if (!TryReadIntegralAttribute(root, "nextId", persistedNextId)
		|| persistedNextId < 1)
	{
		if (outError) *outError = L"Current design snapshot is missing a valid nextId.";
		return false;
	}

	auto controls = FindChildElement(root, "controls");
	if (!controls)
	{
		if (outError) *outError = L"Design file is missing the controls element.";
		return false;
	}
	if (!ValidateElementShape(
		controls, {}, { "control" }, L"Current controls snapshot",
		outError, true))
	{
		return false;
	}

	// Parse into an isolated document and publish it only after every structural,
	// resource, component, and event-index validation has succeeded.  Callers use
	// FromXml for live reload as well as file loading, so a rejected snapshot must
	// not partially clear or rewrite the currently active document.
	DesignDocument document;
	document.ResourceBasePath = resourceBasePath;
	document.Resources = std::make_shared<ResourceLoadContext>(
		Application::GetResourceResolver());
	document.Schema = "cui.designer";
	document.SchemaVersion = DesignDocument::CurrentSchemaVersion;
	document.NextStableId = persistedNextId;
	{
		if (auto codeBehind = FindChildElement(root, "codeBehind"))
		{
			if (!ValidateElementShape(
				codeBehind, { "class", "relativeBasePath" }, {},
				L"Current code-behind snapshot", outError))
			{
				return false;
			}
			if (!DesignCodeBehindModel::TryNormalizeClassName(
				FromUtf8(codeBehind->GetAttribute("class")),
				document.CodeBehind.ClassName, outError)) return false;
			const auto rawPath =
				FromUtf8(codeBehind->GetAttribute("relativeBasePath"));
			if (!DesignCodeBehindModel::TryNormalizeRelativeBasePath(
				rawPath, document.CodeBehind.RelativeBasePath, outError)
				|| !document.CodeBehind.Validate(outError))
				return false;
		}
	}

	auto window = FindChildElement(root, "window");
	if (!window)
	{
		if (outError) *outError =
			L"Design file must contain the schema-owned Window node.";
		return false;
	}
	if (!ValidateElementShape(
		window,
		{ "name", "type", "xamlNamespace", "xamlName" },
		{ "properties", "events", "bindings", "commandBindings",
			"inputBindings" },
		L"Current Window snapshot", outError))
	{
		return false;
	}
	document.Window.Name = FromUtf8(window->GetAttribute("name"));
	if (document.Window.Name.empty())
	{
		if (outError) *outError = L"Window node name cannot be empty.";
		return false;
	}
	if (window->GetAttribute("type") != UIClassToString(UIClass::UI_Window))
	{
		if (outError) *outError = L"Document root node must have Window native type.";
		return false;
	}
	document.Window.Type = UIClass::UI_Window;
	document.Window.XamlType.NamespaceUri = FromUtf8(
		window->GetAttribute("xamlNamespace"));
	document.Window.XamlType.LocalName = FromUtf8(
		window->GetAttribute("xamlName"));
	if (!document.Window.XamlType.Valid())
	{
		if (outError) *outError = L"Window node has an invalid XAML type identity.";
		return false;
	}
	DesignValue encodedWindowProperties = DesignValue::object();
	if (auto properties = FindChildElement(window, "properties");
		properties && !ReadValue(properties, encodedWindowProperties, outError))
		return false;
	if (!DecodeDesignNodeProperties(
		encodedWindowProperties, document.Window.Properties, outError)) return false;
	DesignValue encodedWindowEvents = DesignValue::object();
	if (auto events = FindChildElement(window, "events");
		events && !ReadValue(events, encodedWindowEvents, outError)) return false;
	if (!DecodeDesignNodeEvents(
		encodedWindowEvents, document.Window.Events, outError)) return false;
	DesignValue encodedWindowBindings = DesignValue::object();
	if (auto bindings = FindChildElement(window, "bindings");
		bindings && !ReadValue(bindings, encodedWindowBindings, outError))
		return false;
	if (!DecodeDesignNodeBindings(
		encodedWindowBindings, document.Window.Bindings, outError)) return false;
	DesignValue encodedWindowCommandBindings = DesignValue::array();
	if (auto bindings = FindChildElement(window, "commandBindings");
		bindings && !ReadValue(bindings, encodedWindowCommandBindings, outError))
		return false;
	if (!DecodeDesignCommandBindings(encodedWindowCommandBindings,
		document.Window.CommandBindings, outError)) return false;
	DesignValue encodedWindowInputBindings = DesignValue::array();
	if (auto bindings = FindChildElement(window, "inputBindings");
		bindings && !ReadValue(bindings, encodedWindowInputBindings, outError))
		return false;
	if (!DecodeDesignInputBindings(encodedWindowInputBindings,
		document.Window.InputBindings, outError)) return false;

	if (auto dataContext = FindChildElement(root, "dataContext"))
	{
		for (const auto& item : FindChildElements(dataContext, "property"))
		{
			DesignerDataContextProperty property;
			property.Path = DesignerDataContextSchemaUtils::NormalizePath(
				FromUtf8(item->GetAttribute("path")));
			if (!DesignerDataContextSchemaUtils::TryParseValueKind(
				FromUtf8(item->GetAttribute("kind")), property.ValueKind))
			{
				if (outError) *outError = L"DataContext Schema contains an invalid value kind.";
				return false;
			}
			const auto objectType = FromUtf8(item->GetAttribute("objectType"));
			if (!objectType.empty()
				&& !DesignerDataContextSchemaUtils::TryParseObjectKind(
					objectType, property.ObjectKind))
			{
				if (outError) *outError = L"DataContext Schema contains an invalid object type.";
				return false;
			}
			property.ItemType = FromUtf8(item->GetAttribute("itemType"));
			property.DataType = FromUtf8(item->GetAttribute("dataType"));
			TryReadBoolAttribute(item, "read", property.CanRead);
			TryReadBoolAttribute(item, "write", property.CanWrite);
			TryReadBoolAttribute(item, "observe", property.CanObserve);
			document.DataContextSchema.push_back(std::move(property));
		}
		DesignerDataContextSchemaUtils::Canonicalize(document.DataContextSchema);
		if (!DesignerDataContextSchemaUtils::Validate(
			document.DataContextSchema, outError))
		{
			return false;
		}
	}

	{
		if (auto dataTypes = FindChildElement(root, "dataTypes"))
		{
			for (const auto& item : FindChildElements(dataTypes, "dataType"))
			{
				DesignDataTypeDefinition definition;
				definition.Name = DesignerBindingUtils::Trim(
					FromUtf8(item->GetAttribute("name")));
				definition.SourceDictionary = FromUtf8(
					item->GetAttribute("sourceDictionary"));
				if (definition.Name.empty() || document.FindDataType(definition.Name))
				{
					if (outError) *outError = L"DataType identity is missing or duplicated.";
					return false;
				}
				for (const auto& propertyElement : FindChildElements(item, "property"))
				{
					DesignerDataContextProperty property;
					property.Path = DesignerDataContextSchemaUtils::NormalizePath(
						FromUtf8(propertyElement->GetAttribute("path")));
					if (!DesignerDataContextSchemaUtils::TryParseValueKind(
						FromUtf8(propertyElement->GetAttribute("kind")),
						property.ValueKind))
					{
						if (outError) *outError = L"DataType property contains an invalid value kind.";
						return false;
					}
					const auto objectType = FromUtf8(
						propertyElement->GetAttribute("objectType"));
					if (!objectType.empty()
						&& !DesignerDataContextSchemaUtils::TryParseObjectKind(
							objectType, property.ObjectKind))
					{
						if (outError) *outError = L"DataType property contains an invalid object contract.";
						return false;
					}
					property.ItemType = FromUtf8(
						propertyElement->GetAttribute("itemType"));
					property.DataType = FromUtf8(
						propertyElement->GetAttribute("dataType"));
					TryReadBoolAttribute(propertyElement, "read", property.CanRead);
					TryReadBoolAttribute(propertyElement, "write", property.CanWrite);
					TryReadBoolAttribute(propertyElement, "observe", property.CanObserve);
					definition.Properties.push_back(std::move(property));
				}
				DesignerDataContextSchemaUtils::Canonicalize(definition.Properties);
				std::wstring schemaError;
				if (definition.Properties.empty()
					|| !DesignerDataContextSchemaUtils::Validate(
						definition.Properties, &schemaError))
				{
					if (outError) *outError = definition.Properties.empty()
						? L"DataType must declare at least one property."
						: L"Invalid DataType " + definition.Name + L": " + schemaError;
					return false;
				}
				document.DataTypes.push_back(std::move(definition));
			}
		}

		auto validateReferencedTypes = [&](const DesignerDataContextSchema& schema,
			const std::wstring& owner)
		{
			for (const auto& property : schema)
			{
				if (property.ObjectKind == DesignerDataObjectKind::BindingList
					&& !property.ItemType.empty()
					&& !document.FindDataType(property.ItemType))
				{
					if (outError) *outError = owner
						+ L" references an unknown item DataType: "
						+ property.ItemType;
					return false;
				}
				if (property.ObjectKind == DesignerDataObjectKind::BindingSource
					&& !property.DataType.empty()
					&& !document.FindDataType(property.DataType))
				{
					if (outError) *outError = owner
						+ L" references an unknown DataType: "
						+ property.DataType;
					return false;
				}
			}
			return true;
		};
		if (!validateReferencedTypes(document.DataContextSchema, L"DataContext"))
			return false;
		for (const auto& definition : document.DataTypes)
			if (!validateReferencedTypes(definition.Properties,
				L"DataType " + definition.Name)) return false;

		if (auto dataLists = FindChildElement(root, "dataLists"))
			{
				for (const auto& item : FindChildElements(dataLists, "dataList"))
				{
					DesignDataList definition;
					definition.Key = FromUtf8(item->GetAttribute("key"));
					definition.ItemType = FromUtf8(item->GetAttribute("itemType"));
					definition.SourceDictionary = FromUtf8(
						item->GetAttribute("sourceDictionary"));
					for (const auto& recordElement : FindChildElements(item, "record"))
					{
						DesignDataRecord record;
						for (const auto& field : FindChildElements(recordElement, "field"))
						{
							const auto path = FromUtf8(field->GetAttribute("path"));
							if (path.empty() || record.Fields.contains(path))
							{
								if (outError) *outError = L"DataList record field is missing or duplicated.";
								return false;
							}
							record.Fields.emplace(path,
								FromUtf8(field->GetAttribute("value")));
						}
						definition.Records.push_back(std::move(record));
					}
					document.DataLists.push_back(std::move(definition));
				}
			}

		if (auto views = FindChildElement(root, "collectionViews"))
			{
				for (const auto& item : FindChildElements(views, "collectionView"))
				{
					DesignCollectionViewSource definition;
					definition.Key = FromUtf8(item->GetAttribute("key"));
					definition.SourceResource = FromUtf8(
						item->GetAttribute("sourceResource"));
					definition.SourceBindingPath = FromUtf8(
						item->GetAttribute("sourceBindingPath"));
					definition.SourceDictionary = FromUtf8(
						item->GetAttribute("sourceDictionary"));
					for (const auto& source : FindChildElements(item, "group"))
						{
							DesignCollectionGroupDescription group;
							group.PropertyName = FromUtf8(source->GetAttribute("property"));
							const auto direction = source->GetAttribute("direction");
							if (direction == "ascending")
								group.Direction = CollectionSortDirection::Ascending;
							else if (direction == "descending")
								group.Direction = CollectionSortDirection::Descending;
							else
							{
								if (outError) *outError = L"CollectionViewSource group direction is invalid.";
								return false;
							}
							TryReadBoolAttribute(source, "ignoreCase", group.IgnoreCase);
							definition.GroupDescriptions.push_back(std::move(group));
						}
					for (const auto& source : FindChildElements(item, "aggregate"))
						{
							DesignCollectionAggregateDescription aggregate;
							aggregate.Name = FromUtf8(source->GetAttribute("name"));
							aggregate.PropertyName = FromUtf8(
								source->GetAttribute("property"));
							try
							{
								const auto function = std::stoi(
									source->GetAttribute("function"));
								if (function < static_cast<int>(
									CollectionAggregateFunction::Count)
									|| function > static_cast<int>(
										CollectionAggregateFunction::Max))
									throw std::out_of_range("function");
								aggregate.Function = static_cast<
									CollectionAggregateFunction>(function);
							}
							catch (...)
							{
								if (outError) *outError = L"CollectionViewSource aggregate function is invalid.";
								return false;
							}
							definition.AggregateDescriptions.push_back(
								std::move(aggregate));
						}
					for (const auto& source : FindChildElements(item, "sort"))
					{
						DesignCollectionSortDescription sort;
						sort.PropertyName = FromUtf8(source->GetAttribute("property"));
						const auto direction = source->GetAttribute("direction");
						if (direction == "ascending")
							sort.Direction = CollectionSortDirection::Ascending;
						else if (direction == "descending")
							sort.Direction = CollectionSortDirection::Descending;
						else
						{
							if (outError) *outError = L"CollectionViewSource sort direction is invalid.";
							return false;
						}
						TryReadBoolAttribute(source, "ignoreCase", sort.IgnoreCase);
						definition.SortDescriptions.push_back(std::move(sort));
					}
					for (const auto& source : FindChildElements(item, "filter"))
					{
						DesignCollectionFilterDescription filter;
						filter.PropertyName = FromUtf8(source->GetAttribute("property"));
						filter.Value = FromUtf8(source->GetAttribute("value"));
						try
						{
							const auto op = std::stoi(source->GetAttribute("operator"));
							if (op < static_cast<int>(CollectionFilterOperator::Equals)
								|| op > static_cast<int>(CollectionFilterOperator::IsNotEmpty))
								throw std::out_of_range("operator");
							filter.Operator = static_cast<CollectionFilterOperator>(op);
						}
						catch (...)
						{
							if (outError) *outError = L"CollectionViewSource filter operator is invalid.";
							return false;
						}
						TryReadBoolAttribute(source, "ignoreCase", filter.IgnoreCase);
						definition.FilterDescriptions.push_back(std::move(filter));
					}
					document.CollectionViews.push_back(std::move(definition));
				}
			}

		if (auto templates = FindChildElement(root, "itemsPanelTemplates"))
			{
				for (const auto& item : FindChildElements(
					templates, "itemsPanelTemplate"))
				{
					DesignItemsPanelTemplate definition;
					definition.Key = DesignerBindingUtils::Trim(
						FromUtf8(item->GetAttribute("key")));
					definition.SourceDictionary = FromUtf8(
						item->GetAttribute("sourceDictionary"));
					const auto kind = item->GetAttribute("kind");
					if (kind == "stack") definition.Value.Kind = ItemsPanelKind::Stack;
					else if (kind == "wrap") definition.Value.Kind = ItemsPanelKind::Wrap;
					else if (kind == "virtualizingStack")
						definition.Value.Kind = ItemsPanelKind::VirtualizingStack;
					else
					{
						if (outError) *outError = L"ItemsPanelTemplate kind is invalid.";
						return false;
					}
					const auto orientation = item->GetAttribute("orientation");
					if (orientation == "vertical")
						definition.Value.Orientation = Orientation::Vertical;
					else if (orientation == "horizontal")
						definition.Value.Orientation = Orientation::Horizontal;
					else
					{
						if (outError) *outError = L"ItemsPanelTemplate orientation is invalid.";
						return false;
					}
					if (!TryReadFloatAttribute(item, "itemWidth", definition.Value.ItemWidth)
						|| !TryReadFloatAttribute(item, "itemHeight", definition.Value.ItemHeight)
						|| !TryReadFloatAttribute(item, "cacheLength", definition.Value.CacheLength))
					{
						if (outError) *outError = L"ItemsPanelTemplate numeric configuration is invalid.";
						return false;
					}
					if (definition.Key.empty()
						|| document.FindItemsPanelTemplate(definition.Key))
					{
						if (outError) *outError = L"ItemsPanelTemplate key is missing or duplicated.";
						return false;
					}
					document.ItemsPanelTemplates.push_back(std::move(definition));
				}
			}

		if (auto dataTemplates = FindChildElement(root, "dataTemplates"))
		{
			for (const auto& item : FindChildElements(
				dataTemplates, "dataTemplate"))
			{
				DesignDataTemplate definition;
				definition.Key = DesignerBindingUtils::Trim(
					FromUtf8(item->GetAttribute("key")));
				definition.DataType = DesignerBindingUtils::Trim(
					FromUtf8(item->GetAttribute("dataType")));
				if (const auto hierarchical = item->GetAttribute("hierarchical");
					!hierarchical.empty())
				{
					if (!TryParseBool(hierarchical, definition.Hierarchical))
					{
						if (outError) *outError = L"DataTemplate hierarchical flag is invalid.";
						return false;
					}
				}
				definition.SourceDictionary = FromUtf8(
					item->GetAttribute("sourceDictionary"));
				if (auto source = FindChildElement(item, "itemsSource"))
				{
					if (!definition.Hierarchical)
					{
						if (outError) *outError = L"DataTemplate ItemsSource requires HierarchicalDataTemplate.";
						return false;
					}
					DesignValue bindingValue;
					DesignerDataBinding binding;
					if (!ReadValue(source, bindingValue, outError)
						|| !DesignerBindingUtils::TryReadBindingDefinition(
							bindingValue, binding, outError)) return false;
					definition.ItemsSourceBinding = std::move(binding);
				}
				const auto* dataType = document.FindDataType(definition.DataType);
				const bool groupType = DesignDataResourceUtils::
					IsCollectionViewGroupDataType(definition.DataType);
				const auto bindingSchema = dataType ? dataType->Properties
					: DesignDataResourceUtils::BuildCollectionViewGroupSchema();
				const bool duplicate = definition.IsImplicit()
					? document.FindImplicitDataTemplate(definition.DataType) != nullptr
					: document.FindDataTemplate(definition.Key) != nullptr;
				if ((!dataType && !groupType) || duplicate)
				{
					if (outError) *outError = L"DataTemplate identity, DataType, or key is invalid.";
					return false;
				}
				auto templateElement = FindChildElement(item, "template");
				DesignValue templateValue;
				if (!templateElement
					|| !ReadValue(templateElement, templateValue, outError)
					|| !templateValue.is_array()) return false;
				for (const auto& value : templateValue.ArrayItems())
				{
					DesignNode node;
					if (!TemplateNodeFromValue(
						value, node, outError, resourceBasePath)) return false;
					if (!node.Events.empty())
					{
						if (outError) *outError = L"DataTemplate does not support code-behind events.";
						return false;
					}
					for (const auto& [target, binding] : node.Bindings)
					{
						(void)target;
						if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
							binding, [&](const DesignerDataBinding& leaf)
							{
								const auto path = DesignerBindingUtils::Trim(
									leaf.SourceProperty);
								if (path.empty()
									|| DesignerDataContextSchemaUtils::Find(
										bindingSchema, path)
									|| (groupType
										&& DesignerDataContextSchemaUtils::IsValidPath(path)
									&& (path.starts_with(L"FirstItem.")
										|| path.starts_with(L"Aggregates."))))
									return true;
								if (outError) *outError = L"DataTemplate binding path is not declared by its DataType: " + path;
								return false;
							})) return false;
					}
					definition.Template.push_back(std::move(node));
				}
				DesignDocument templateDocument;
				templateDocument.Nodes = definition.Template;
				templateDocument.RecalculateNextStableId();
				DesignDocumentGraph templateGraph;
				if (!DesignDocumentGraph::Build(
					templateDocument, templateGraph, outError)
					|| templateGraph.Roots().size() != 1)
				{
					if (outError && outError->empty())
						*outError = L"DataTemplate must contain exactly one visual root.";
					return false;
				}
				document.DataTemplates.push_back(std::move(definition));
			}
		}
		if (auto templates = FindChildElement(
				root, "controlTemplatesSnapshot"))
			{
				DesignDocument snapshot;
				XamlDocumentParseOptions options;
				options.ResourceBasePath = resourceBasePath;
				options.Resources = document.Resources;
				std::wstring templateError;
				if (!XamlDocumentParser::FromXaml(
					templates->InnerText(), snapshot, options, &templateError))
				{
					if (outError) *outError =
						L"ControlTemplate snapshot is invalid: " + templateError;
					return false;
				}
				document.ControlTemplates =
					std::move(snapshot.ControlTemplates);
			}
		if (auto styles = FindChildElement(root, "groupStyles"))
			{
				for (const auto& item : FindChildElements(styles, "groupStyle"))
				{
					DesignGroupStyle definition;
					definition.Key = DesignerBindingUtils::Trim(
						FromUtf8(item->GetAttribute("key")));
					definition.HeaderTemplate = DesignerBindingUtils::Trim(
						FromUtf8(item->GetAttribute("headerTemplate")));
					definition.SourceDictionary = FromUtf8(
						item->GetAttribute("sourceDictionary"));
					if (definition.Key.empty()
						|| document.FindGroupStyle(definition.Key))
					{
						if (outError) *outError = L"GroupStyle configuration is invalid.";
						return false;
					}
					document.GroupStyles.push_back(std::move(definition));
				}
			}
	}

	{
		if (auto styleSheet = FindChildElement(root, "styleSheet"))
		{
			if (auto dictionaries = FindChildElement(
				styleSheet, "mergedDictionaries"))
			{
				for (const auto& item : FindChildElements(
					dictionaries, "dictionary"))
					document.StyleSheet.MergedDictionaries.push_back(
						FromUtf8(item->GetAttribute("source")));
			}
			if (auto resources = FindChildElement(styleSheet, "resources"))
			{
				for (const auto& item : FindChildElements(resources, "resource"))
				{
					DesignerStyleResource resource;
					resource.Key = FromUtf8(item->GetAttribute("key"));
					resource.SourceDictionary = FromUtf8(
						item->GetAttribute("sourceDictionary"));
					if (!DesignerStyleSheetUtils::TryParseValueKind(
						FromUtf8(item->GetAttribute("kind")), resource.Value.Kind))
					{
						if (outError) *outError = L"样式资源包含无效的值类型。";
						return false;
					}
					if (const auto object = FindChildElement(item, "objectValue"); object)
					{
						if (!ReadValue(object, resource.Value.ObjectValue, outError))
							return false;
					}
					else resource.Value.Text = FromUtf8(item->InnerText());
					document.StyleSheet.Resources.push_back(std::move(resource));
				}
			}
			if (auto rules = FindChildElement(styleSheet, "rules"))
			{
				for (const auto& item : FindChildElements(rules, "rule"))
				{
					DesignerStyleRule rule;
					rule.SourceDictionary = FromUtf8(
						item->GetAttribute("sourceDictionary"));
					const auto type = FromUtf8(item->GetAttribute("type"));
					if (!type.empty())
					{
						rule.HasType = true;
						if (!DesignerStyleSheetUtils::TryParseUIClass(type, rule.Type))
						{
							if (outError) *outError = L"样式规则包含无效的控件类型。";
							return false;
						}
					}
					if (item->HasAttribute("xamlNamespace")
						|| item->HasAttribute("xamlName"))
					{
						rule.XamlType.NamespaceUri = FromUtf8(
							item->GetAttribute("xamlNamespace"));
						rule.XamlType.LocalName = FromUtf8(
							item->GetAttribute("xamlName"));
						if (!rule.HasType || !rule.XamlType.Valid())
						{
							if (outError) *outError = L"样式规则包含无效的内置 XAML TargetType。";
							return false;
						}
					}
					if (item->HasAttribute("componentName"))
					{
						rule.ComponentType.XamlPrefix = FromUtf8(
							item->GetAttribute("componentPrefix"));
						rule.ComponentType.XamlName = FromUtf8(
							item->GetAttribute("componentName"));
						rule.ComponentType.XamlNamespace = FromUtf8(
							item->GetAttribute("componentNamespace"));
						if (!rule.HasType || rule.ComponentType.XamlPrefix.empty()
							|| rule.ComponentType.XamlName.empty()
							|| rule.ComponentType.XamlNamespace.empty())
						{
							if (outError) *outError = L"样式规则包含无效的组件 TargetType。";
							return false;
						}
					}
					rule.Id = FromUtf8(item->GetAttribute("id"));
					rule.BasedOn = FromUtf8(item->GetAttribute("basedOn"));
					for (const auto& setterElement : FindChildElements(item, "setter"))
					{
						DesignerStyleSetter setter;
						setter.PropertyName = FromUtf8(setterElement->GetAttribute("property"));
						setter.ResourceKey = FromUtf8(setterElement->GetAttribute("resource"));
						setter.UsesResource = !setter.ResourceKey.empty();
						if (setterElement->HasAttribute("dynamicResource")
							&& !TryReadBoolAttribute(setterElement,
								"dynamicResource", setter.UsesDynamicResource))
						{
							if (outError) *outError = L"样式 Setter 的动态资源标记无效。";
							return false;
						}
						if (!setter.UsesResource)
						{
							if (!DesignerStyleSheetUtils::TryParseValueKind(
								FromUtf8(setterElement->GetAttribute("kind")), setter.Literal.Kind))
							{
								if (outError) *outError = L"样式 Setter 包含无效的值类型。";
								return false;
							}
							if (const auto object = FindChildElement(
								setterElement, "objectValue"); object)
							{
								if (!ReadValue(object, setter.Literal.ObjectValue, outError))
									return false;
							}
							else setter.Literal.Text = FromUtf8(
								setterElement->InnerText());
						}
						rule.Setters.push_back(std::move(setter));
					}
					if (const auto triggersElement = FindChildElement(item, "triggers"))
					{
						for (const auto& triggerElement : FindChildElements(
							triggersElement, "trigger"))
						{
							if (!ValidateElementShape(
								triggerElement, {},
								{ "dataConditions", "propertyConditions", "setter",
									"enterActions", "exitActions" },
								L"Current Style Trigger snapshot", outError, true))
							{
								return false;
							}
							const auto dataConditionContainers = FindChildElements(
								triggerElement, "dataConditions");
							const auto propertyConditionContainers = FindChildElements(
								triggerElement, "propertyConditions");
							if (dataConditionContainers.size() > 1
								|| propertyConditionContainers.size() > 1
								|| (!dataConditionContainers.empty()
									&& !propertyConditionContainers.empty()))
							{
								if (outError) *outError =
									L"Current Style Trigger snapshot must contain one condition family.";
								return false;
							}
							DesignerStyleTrigger trigger;
							auto readDataCondition = [&](const auto& dataConditionElement,
								DesignerStyleDataCondition& condition)
							{
								condition.SourceProperty = FromUtf8(
									dataConditionElement->GetAttribute("source"));
								if (!DesignerStyleSheetUtils::TryParseValueKind(
									FromUtf8(dataConditionElement->GetAttribute("kind")),
									condition.Value.Kind))
								{
									if (outError) *outError =
										L"样式 DataTrigger 包含无效的值类型。";
									return false;
								}
								if (const auto object = FindChildElement(
									dataConditionElement, "objectValue"))
								{
									if (!ReadValue(object,
										condition.Value.ObjectValue, outError)) return false;
								}
								else condition.Value.Text = FromUtf8(
									dataConditionElement->InnerText());
								return true;
							};
							const auto dataConditionsElement = FindChildElement(
								triggerElement, "dataConditions");
							if (dataConditionsElement)
							{
								if (!ValidateElementShape(
									dataConditionsElement, {}, { "condition" },
									L"Current DataTrigger conditions snapshot",
									outError, true)) return false;
								for (const auto& conditionElement : FindChildElements(
									dataConditionsElement, "condition"))
								{
									if (!ValidateElementShape(
										conditionElement, { "source", "kind" },
										{ "objectValue" },
										L"Current DataTrigger condition snapshot",
										outError)) return false;
									DesignerStyleDataCondition condition;
									if (!readDataCondition(conditionElement, condition))
										return false;
									trigger.DataConditions.push_back(std::move(condition));
								}
							}
							else if (const auto propertyConditionsElement =
								FindChildElement(triggerElement, "propertyConditions"))
							{
								if (!ValidateElementShape(
									propertyConditionsElement, {}, { "condition" },
									L"Current Trigger conditions snapshot",
									outError, true)) return false;
								for (const auto& conditionElement : FindChildElements(
									propertyConditionsElement, "condition"))
								{
									if (!ValidateElementShape(
										conditionElement, { "property", "kind" }, {},
										L"Current Trigger condition snapshot",
										outError)) return false;
									DesignerStylePropertyCondition condition;
									condition.Property = FromUtf8(
										conditionElement->GetAttribute("property"));
									if (!DesignerStyleSheetUtils::TryParseValueKind(
										FromUtf8(conditionElement->GetAttribute("kind")),
										condition.Value.Kind))
									{
										if (outError) *outError =
											L"样式 Trigger 包含无效的属性值类型。";
										return false;
									}
									condition.Value.Text = FromUtf8(
										conditionElement->InnerText());
									trigger.PropertyConditions.push_back(
										std::move(condition));
								}
							}
							for (const auto& setterElement : FindChildElements(
								triggerElement, "setter"))
							{
								DesignerStyleSetter setter;
								setter.PropertyName = FromUtf8(
									setterElement->GetAttribute("property"));
								setter.ResourceKey = FromUtf8(
									setterElement->GetAttribute("resource"));
								setter.UsesResource = !setter.ResourceKey.empty();
								if (setterElement->HasAttribute("dynamicResource")
									&& !TryReadBoolAttribute(setterElement,
										"dynamicResource", setter.UsesDynamicResource))
								{
									if (outError) *outError =
										L"样式 Trigger Setter 的动态资源标记无效。";
									return false;
								}
								if (!setter.UsesResource)
								{
									if (!DesignerStyleSheetUtils::TryParseValueKind(
										FromUtf8(setterElement->GetAttribute("kind")),
										setter.Literal.Kind))
									{
										if (outError) *outError =
											L"样式 Trigger Setter 包含无效的值类型。";
										return false;
									}
									if (const auto object = FindChildElement(
										setterElement, "objectValue"); object)
									{
										if (!ReadValue(object,
											setter.Literal.ObjectValue, outError)) return false;
									}
									else setter.Literal.Text = FromUtf8(
										setterElement->InnerText());
								}
								trigger.Setters.push_back(std::move(setter));
							}
							if (!ReadStoryboardActionsSnapshot(
								triggerElement, "enterActions",
								trigger.EnterActions, outError)
								|| !ReadStoryboardActionsSnapshot(
									triggerElement, "exitActions",
									trigger.ExitActions, outError)) return false;
							rule.Triggers.push_back(std::move(trigger));
						}
					}
					document.StyleSheet.Rules.push_back(std::move(rule));
				}
			}
			DesignerStyleSheetUtils::Canonicalize(document.StyleSheet);
			if (!DesignerStyleSheetUtils::Validate(
				document.StyleSheet, outError, document.ResourceBasePath,
				document.Resources))
				return false;
		}
	}

	{
		if (auto components = FindChildElement(root, "components"))
		{
			for (const auto& component : FindChildElements(components, "component"))
			{
				DesignComponentDefinition definition;
				definition.Type.XamlPrefix = FromUtf8(
					component->GetAttribute("prefix"));
				definition.Type.XamlName = FromUtf8(
					component->GetAttribute("name"));
				definition.Type.XamlNamespace = FromUtf8(
					component->GetAttribute("namespace"));
				definition.DisplayName = FromUtf8(
					component->GetAttribute("displayName"));
				definition.Category = FromUtf8(
					component->GetAttribute("category"));
				definition.SourceDictionary = FromUtf8(
					component->GetAttribute("sourceDictionary"));
				if (definition.Type.XamlPrefix.empty()
					|| definition.Type.XamlName.empty()
					|| definition.Type.XamlNamespace.empty()
					|| !TryParseUIClass(
						component->GetAttribute("baseType"), definition.BaseType)
					|| definition.BaseType == UIClass::UI_TabItem
					|| document.FindComponent(definition.Type))
				{
					if (outError) *outError = L"Component definition has invalid identity or baseType.";
					return false;
				}
				for (const auto& item : FindChildElements(component, "property"))
				{
					DesignerComponentPropertyDescriptor property;
					property.Name = FromUtf8(item->GetAttribute("name"));
					property.DisplayName = FromUtf8(
						item->GetAttribute("displayName"));
					property.Category = FromUtf8(item->GetAttribute("category"));
					property.DefaultResourceKey = FromUtf8(
						item->GetAttribute("defaultResource"));
					if (property.Name.empty()
						|| !TryReadIntegralAttribute(
							item, "categoryOrder", property.CategoryOrder)
						|| !TryReadIntegralAttribute(item, "order", property.Order)
						|| !DesignerStyleSheetUtils::TryParseValueKind(
							FromUtf8(item->GetAttribute("kind")),
							property.DefaultValue.Kind))
					{
						if (outError) *outError = L"Component property schema is invalid.";
						return false;
					}
					if (const auto object = FindChildElement(item, "objectValue"); object)
					{
						if (!ReadValue(object, property.DefaultValue.ObjectValue, outError))
							return false;
					}
					else property.DefaultValue.Text = FromUtf8(
						item->GetAttribute("default"));
					int editor = 0;
					unsigned int flags = 0;
					int defaultUpdateMode = 0;
					constexpr auto supportedPropertyFlags =
						static_cast<unsigned int>(
							DependencyPropertyFlags::AffectsMeasure
							| DependencyPropertyFlags::AffectsArrange
							| DependencyPropertyFlags::AffectsRender
							| DependencyPropertyFlags::Inherits
							| DependencyPropertyFlags::BindsTwoWayByDefault
							| DependencyPropertyFlags::AffectsParentMeasure
							| DependencyPropertyFlags::AffectsParentArrange);
					if (!TryReadIntegralAttribute(item, "editor", editor)
						|| editor < static_cast<int>(DependencyPropertyEditorKind::Auto)
						|| editor > static_cast<int>(DependencyPropertyEditorKind::Length)
						|| !TryReadIntegralAttribute(item, "flags", flags)
						|| (flags & ~supportedPropertyFlags) != 0
						|| !TryReadIntegralAttribute(
							item, "defaultUpdateMode", defaultUpdateMode)
						|| defaultUpdateMode < static_cast<int>(
							DataSourceUpdateMode::OnPropertyChanged)
						|| defaultUpdateMode > static_cast<int>(
							DataSourceUpdateMode::Never))
					{
						if (outError) *outError = L"Component property metadata is invalid.";
						return false;
					}
					property.Editor = static_cast<DependencyPropertyEditorKind>(editor);
					property.Flags = static_cast<DependencyPropertyFlags>(flags);
					property.DefaultUpdateMode =
						static_cast<DataSourceUpdateMode>(defaultUpdateMode);
					if (!TryReadBoolAttribute(
						item, "readOnly", property.IsReadOnly))
					{
						if (outError) *outError = L"Component property readOnly metadata is invalid.";
						return false;
					}
					if (property.IsReadOnly && HasDependencyPropertyFlag(
						property.Flags, DependencyPropertyFlags::BindsTwoWayByDefault))
					{
						if (outError) *outError = L"Read-only component property cannot bind two-way by default.";
						return false;
					}
					if (property.IsReadOnly && property.DefaultUpdateMode
						!= DataSourceUpdateMode::OnPropertyChanged)
					{
						if (outError) *outError = L"Read-only component property cannot declare a source update trigger.";
						return false;
					}
					double number = 0.0;
					if (item->HasAttribute("minimum"))
					{
						if (!TryReadDoubleAttribute(item, "minimum", number)) return false;
						property.Minimum = number;
					}
					if (item->HasAttribute("maximum"))
					{
						if (!TryReadDoubleAttribute(item, "maximum", number)) return false;
						property.Maximum = number;
					}
					if (item->HasAttribute("step"))
					{
						if (!TryReadDoubleAttribute(item, "step", number)) return false;
						property.Step = number;
					}
					for (const auto& value : FindChildElements(item, "choice"))
					{
						DesignerComponentPropertyChoice choice;
						choice.Value = FromUtf8(value->GetAttribute("value"));
						choice.DisplayName = FromUtf8(value->GetAttribute("displayName"));
						if (choice.Value.empty()
							|| std::any_of(property.Choices.begin(), property.Choices.end(),
								[&](const auto& existing)
								{
									return std::wcscmp(existing.Value.c_str(), choice.Value.c_str()) == 0;
								}))
						{
							if (outError) *outError = L"Component enum choice is invalid or duplicated.";
							return false;
						}
						if (choice.DisplayName.empty()) choice.DisplayName = choice.Value;
						property.Choices.push_back(std::move(choice));
					}
					if (!property.Choices.empty()
						&& property.DefaultResourceKey.empty())
					{
						if (property.DefaultValue.Kind != DesignerStyleValueKind::String
							|| property.Editor != DependencyPropertyEditorKind::Choice)
						{
							if (outError) *outError = L"Component enum property metadata is invalid.";
							return false;
						}
						const auto selected = std::find_if(
							property.Choices.begin(), property.Choices.end(),
							[&](const auto& choice)
							{
								return std::wcscmp(choice.Value.c_str(),
									property.DefaultValue.Text.c_str()) == 0;
							});
						if (selected == property.Choices.end())
						{
							if (outError) *outError = L"Component enum default is outside its choices.";
							return false;
						}
						property.DefaultValue.Text = selected->Value;
					}
					const DesignerStyleValue* effectiveDefault = &property.DefaultValue;
					if (!property.DefaultResourceKey.empty())
					{
						const auto resource = std::find_if(
							document.StyleSheet.Resources.begin(),
							document.StyleSheet.Resources.end(),
							[&](const auto& candidate)
							{
								return std::wcscmp(candidate.Key.c_str(),
									property.DefaultResourceKey.c_str()) == 0;
							});
						if (resource == document.StyleSheet.Resources.end()
							|| resource->Value.Kind != property.DefaultValue.Kind)
						{
							if (outError) *outError = L"Component property default resource is missing or has the wrong type.";
							return false;
						}
						effectiveDefault = &resource->Value;
					}
					BindingValue convertedDefault;
					std::wstring conversionError;
					if (!DesignerStyleSheetUtils::TryConvertValue(
						*effectiveDefault, convertedDefault, &conversionError,
						document.ResourceBasePath, document.Resources))
					{
						if (outError) *outError = L"Component property default is invalid: "
							+ conversionError;
						return false;
					}
					definition.Properties.push_back(std::move(property));
				}
				for (const auto& item : FindChildElements(component, "contentProperty"))
				{
					DesignerComponentContentPropertyDescriptor property;
					property.Name = FromUtf8(item->GetAttribute("name"));
					property.DisplayName = FromUtf8(item->GetAttribute("displayName"));
					const auto cardinality = item->GetAttribute("cardinality");
					if (cardinality == "single")
						property.Cardinality = DesignerComponentContentCardinality::Single;
					else if (cardinality == "multiple")
						property.Cardinality = DesignerComponentContentCardinality::Multiple;
					else
					{
						if (outError) *outError = L"Component content cardinality is invalid.";
						return false;
					}
					if (!TryParseBool(item->GetAttribute("default"), property.IsDefault)
						|| property.Name.empty()
						|| std::any_of(definition.ContentProperties.begin(),
							definition.ContentProperties.end(), [&](const auto& existing)
							{
								return std::wcscmp(existing.Name.c_str(), property.Name.c_str()) == 0;
							})
						|| std::any_of(definition.Properties.begin(), definition.Properties.end(),
							[&](const auto& existing)
							{
								return std::wcscmp(existing.Name.c_str(), property.Name.c_str()) == 0;
							})
						|| (property.IsDefault && std::any_of(
							definition.ContentProperties.begin(),
							definition.ContentProperties.end(),
							[](const auto& existing) { return existing.IsDefault; })))
					{
						if (outError) *outError = L"Component content property schema is invalid.";
						return false;
					}
					if (property.DisplayName.empty()) property.DisplayName = property.Name;
					definition.ContentProperties.push_back(std::move(property));
				}
				for (const auto& item : FindChildElements(component, "event"))
				{
					DesignerComponentEventDescriptor eventContract;
					eventContract.Name = FromUtf8(item->GetAttribute("name"));
					eventContract.DisplayName = FromUtf8(
						item->GetAttribute("displayName"));
					if (eventContract.Name.empty()
						|| !DesignerEventCatalog::TryParseCategory(
							FromUtf8(item->GetAttribute("category")),
							eventContract.Category)
						|| !DesignerEventCatalog::TryParseComponentPayload(
							FromUtf8(item->GetAttribute("payload")),
							eventContract.Payload)
						|| !DesignerEventCatalog::TryParseComponentRoutingStrategy(
							FromUtf8(item->GetAttribute("routingStrategy")),
							eventContract.RoutingStrategy)
						|| !TryReadIntegralAttribute(
							item, "order", eventContract.Order)
						|| !TryReadBoolAttribute(
							item, "default", eventContract.IsDefault))
					{
						if (outError) *outError = L"Component event schema is invalid.";
						return false;
					}
					definition.Events.push_back(std::move(eventContract));
				}
				{
					std::wstring validationError;
					if (!DesignerEventCatalog::ValidateComponentEvents(
						definition.BaseType, definition.Events, &validationError))
					{
						if (outError) *outError = std::move(validationError);
						return false;
					}
				}
				std::vector<std::pair<std::wstring, std::wstring>>
					visualStateControlledProperties;
				for (const auto& groupElement : FindChildElements(
					component, "visualStateGroup"))
				{
					DesignerVisualStateGroup group;
					group.Name = FromUtf8(groupElement->GetAttribute("name"));
					if (group.Name.empty()
						|| std::any_of(definition.VisualStateGroups.begin(),
							definition.VisualStateGroups.end(), [&](const auto& existing)
							{ return std::wcscmp(existing.Name.c_str(), group.Name.c_str()) == 0; }))
					{
						if (outError) *outError = L"Component visual-state group name is invalid or duplicated.";
						return false;
					}
					int fallbackCount = 0;
					std::vector<std::wstring> groupEvents;
					for (const auto& stateElement : FindChildElements(
						groupElement, "visualState"))
					{
						DesignerVisualState state;
						state.Name = FromUtf8(stateElement->GetAttribute("name"));
						if (state.Name.empty()
							|| std::any_of(group.States.begin(), group.States.end(),
								[&](const auto& existing)
								{ return std::wcscmp(existing.Name.c_str(), state.Name.c_str()) == 0; }))
						{
							if (outError) *outError = L"Component visual-state name is invalid or duplicated.";
							return false;
						}
						for (const auto& conditionElement : FindChildElements(
							stateElement, "condition"))
						{
							DesignerVisualStateCondition condition;
							condition.PropertyName = FromUtf8(
								conditionElement->GetAttribute("property"));
							if (condition.PropertyName.empty()
								|| !DesignerStyleSheetUtils::TryParseValueKind(
									FromUtf8(conditionElement->GetAttribute("kind")),
									condition.Value.Kind)
								|| std::any_of(state.Conditions.begin(),
									state.Conditions.end(), [&](const auto& existing)
									{ return std::wcscmp(existing.PropertyName.c_str(),
										condition.PropertyName.c_str()) == 0; }))
							{
								if (outError) *outError = L"Component visual-state condition is invalid or duplicated.";
								return false;
							}
							if (const auto object = FindChildElement(
								conditionElement, "objectValue"); object)
							{
								if (!ReadValue(object, condition.Value.ObjectValue, outError))
									return false;
							}
							else condition.Value.Text = FromUtf8(
								conditionElement->InnerText());
							state.Conditions.push_back(std::move(condition));
						}
						for (const auto& eventElement : FindChildElements(
							stateElement, "eventTrigger"))
						{
							const auto eventName = FromUtf8(
								eventElement->GetAttribute("event"));
							const auto event = std::find_if(
								definition.Events.begin(), definition.Events.end(),
								[&](const auto& candidate)
								{ return std::wcscmp(candidate.Name.c_str(), eventName.c_str()) == 0; });
							if (event == definition.Events.end()
								|| std::any_of(groupEvents.begin(), groupEvents.end(),
									[&](const auto& existing)
									{ return std::wcscmp(existing.c_str(), eventName.c_str()) == 0; }))
							{
								if (outError) *outError = L"Component visual-state event trigger is missing or duplicated.";
								return false;
							}
							groupEvents.push_back(event->Name);
							state.EventNames.push_back(event->Name);
						}
						if (!state.Conditions.empty() && !state.EventNames.empty())
						{
							if (outError) *outError = L"Component visual state mixes property and event triggers.";
							return false;
						}
						if (state.Conditions.empty() && state.EventNames.empty())
							++fallbackCount;
						for (const auto& setterElement : FindChildElements(
							stateElement, "setter"))
						{
							DesignerVisualStateSetter setter;
							setter.TargetName = FromUtf8(
								setterElement->GetAttribute("target"));
							setter.PropertyName = FromUtf8(
								setterElement->GetAttribute("property"));
							setter.ResourceKey = FromUtf8(
								setterElement->GetAttribute("resource"));
							setter.UsesResource = !setter.ResourceKey.empty();
							if (setter.PropertyName.empty()
								|| std::any_of(state.Setters.begin(), state.Setters.end(),
									[&](const auto& existing)
									{
										return std::wcscmp(existing.TargetName.c_str(),
											setter.TargetName.c_str()) == 0
											&& std::wcscmp(existing.PropertyName.c_str(),
												setter.PropertyName.c_str()) == 0;
									}))
							{
								if (outError) *outError = L"Component visual-state setter is invalid or duplicated.";
								return false;
							}
							if (setter.UsesResource
								&& setterElement->HasAttribute("kind"))
							{
								if (outError) *outError = L"Component visual-state setter value is ambiguous.";
								return false;
							}
							const auto valueKind = setter.UsesResource
								? setterElement->GetAttribute("fallbackKind")
								: setterElement->GetAttribute("kind");
							if (!valueKind.empty())
							{
								if (!DesignerStyleSheetUtils::TryParseValueKind(
									FromUtf8(valueKind),
									setter.Literal.Kind))
								{
									if (outError) *outError = L"Component visual-state setter kind is invalid.";
									return false;
								}
								const auto object = FindChildElement(setterElement,
									setter.UsesResource
										? "fallbackObjectValue" : "objectValue");
								if (object)
								{
									if (!ReadValue(object, setter.Literal.ObjectValue, outError))
										return false;
								}
								else setter.Literal.Text = FromUtf8(
									setterElement->InnerText());
							}
							const auto controlledKey = setter.TargetName + L"|"
								+ setter.PropertyName;
							const auto controlled = std::find_if(
								visualStateControlledProperties.begin(),
								visualStateControlledProperties.end(),
								[&](const auto& existing)
								{ return std::wcscmp(existing.first.c_str(), controlledKey.c_str()) == 0; });
							if (controlled != visualStateControlledProperties.end()
								&& std::wcscmp(controlled->second.c_str(), group.Name.c_str()) != 0)
							{
								if (outError) *outError = L"Different visual-state groups control the same property.";
								return false;
							}
							if (controlled == visualStateControlledProperties.end())
								visualStateControlledProperties.emplace_back(
									controlledKey, group.Name);
							state.Setters.push_back(std::move(setter));
						}
						for (const auto& animationElement : FindChildElements(
							stateElement, "animation"))
						{
							DesignerVisualStateAnimation animation;
							const auto type = animationElement->GetAttribute("type");
							if (std::strcmp(type.c_str(), "Double") == 0
								|| std::strcmp(type.c_str(), "Color") == 0
								|| std::strcmp(type.c_str(), "Object") == 0
								|| std::strcmp(type.c_str(), "Thickness") == 0
								|| std::strcmp(type.c_str(), "Point") == 0
								|| std::strcmp(type.c_str(), "Vector") == 0
								|| std::strcmp(type.c_str(), "Rect") == 0
								|| std::strcmp(type.c_str(), "Size") == 0
								|| std::strcmp(type.c_str(), "Matrix") == 0)
							{
								if (!ReadVisualStateAnimationSnapshot(
									animationElement, animation, outError)) return false;
								const bool duplicate = std::any_of(
									state.Setters.begin(), state.Setters.end(), [&](const auto& existing)
									{ return std::wcscmp(existing.TargetName.c_str(), animation.TargetName.c_str()) == 0
										&& std::wcscmp(existing.PropertyName.c_str(), animation.PropertyName.c_str()) == 0; })
									|| std::any_of(state.Animations.begin(), state.Animations.end(),
										[&](const auto& existing)
										{ return std::wcscmp(existing.TargetName.c_str(), animation.TargetName.c_str()) == 0
											&& std::wcscmp(existing.PropertyName.c_str(), animation.PropertyName.c_str()) == 0; });
								if (duplicate)
								{
									if (outError) *outError = L"Component visual-state Setter/animation target is duplicated.";
									return false;
								}
								const auto controlledKey = animation.TargetName + L"|"
									+ animation.PropertyName;
								const auto controlled = std::find_if(
									visualStateControlledProperties.begin(),
									visualStateControlledProperties.end(), [&](const auto& existing)
									{ return std::wcscmp(existing.first.c_str(), controlledKey.c_str()) == 0; });
								if (controlled != visualStateControlledProperties.end()
									&& std::wcscmp(controlled->second.c_str(), group.Name.c_str()) != 0)
								{
									if (outError) *outError = L"Different visual-state groups animate the same property.";
									return false;
								}
								if (controlled == visualStateControlledProperties.end())
									visualStateControlledProperties.emplace_back(
										controlledKey, group.Name);
								state.Animations.push_back(std::move(animation));
								continue;
							}
							if (std::strcmp(type.c_str(), "Double") == 0)
								animation.Kind = DesignerAnimationKind::Double;
							else if (std::strcmp(type.c_str(), "Color") == 0)
								animation.Kind = DesignerAnimationKind::Color;
							else
							{
								if (outError) *outError = L"Component visual-state animation type is invalid.";
								return false;
							}
							animation.TargetName = FromUtf8(
								animationElement->GetAttribute("target"));
							animation.PropertyName = FromUtf8(
								animationElement->GetAttribute("property"));
							if (animation.PropertyName.empty()
								|| !TryParseIntegral(
									animationElement->GetAttribute("beginTimeMs"),
									animation.BeginTimeMilliseconds)
								|| !TryParseIntegral(
									animationElement->GetAttribute("durationMs"),
									animation.DurationMilliseconds))
							{
								if (outError) *outError = L"Component visual-state animation timing or property is invalid.";
								return false;
							}
							const auto repeatBehavior =
								animationElement->GetAttribute("repeatBehavior");
							if (repeatBehavior.empty()
								|| std::strcmp(repeatBehavior.c_str(), "Count") == 0)
							{
								animation.RepeatBehavior =
									DesignerRepeatBehaviorKind::Count;
								if (!repeatBehavior.empty()
									&& (!TryParseDouble(
										animationElement->GetAttribute("repeatCount"),
										animation.RepeatCount)
										|| !std::isfinite(animation.RepeatCount)
										|| animation.RepeatCount <= 0.0))
								{
									if (outError) *outError =
										L"Component animation repeat count is invalid.";
									return false;
								}
							}
							else if (std::strcmp(
								repeatBehavior.c_str(), "Duration") == 0)
							{
								animation.RepeatBehavior =
									DesignerRepeatBehaviorKind::Duration;
								if (!TryParseIntegral(
									animationElement->GetAttribute("repeatDurationMs"),
									animation.RepeatDurationMilliseconds)
									|| animation.RepeatDurationMilliseconds == 0)
								{
									if (outError) *outError =
										L"Component animation repeat duration is invalid.";
									return false;
								}
							}
							else if (std::strcmp(
								repeatBehavior.c_str(), "Forever") == 0)
								animation.RepeatBehavior =
									DesignerRepeatBehaviorKind::Forever;
							else
							{
								if (outError) *outError =
									L"Component animation repeat behavior is invalid.";
								return false;
							}
							if (animationElement->HasAttribute("autoReverse")
								&& !TryReadBoolAttribute(animationElement,
									"autoReverse", animation.AutoReverse))
							{
								if (outError) *outError =
									L"Component animation AutoReverse is invalid.";
								return false;
							}
							if (animationElement->HasAttribute("isAdditive")
								&& !TryReadBoolAttribute(animationElement,
									"isAdditive", animation.IsAdditive))
							{
								if (outError) *outError =
									L"Component animation IsAdditive is invalid.";
								return false;
							}
							if (animationElement->HasAttribute("isCumulative")
								&& !TryReadBoolAttribute(animationElement,
									"isCumulative", animation.IsCumulative))
							{
								if (outError) *outError =
									L"Component animation IsCumulative is invalid.";
								return false;
							}
							const auto fillBehavior =
								animationElement->GetAttribute("fillBehavior");
							if (fillBehavior.empty()
								|| std::strcmp(fillBehavior.c_str(), "HoldEnd") == 0)
								animation.FillBehavior =
									DesignerTimelineFillBehavior::HoldEnd;
							else if (std::strcmp(fillBehavior.c_str(), "Stop") == 0)
								animation.FillBehavior =
									DesignerTimelineFillBehavior::Stop;
							else
							{
								if (outError) *outError =
									L"Component animation FillBehavior is invalid.";
								return false;
							}
							if (animationElement->HasAttribute("speedRatio")
								&& (!TryParseDouble(
									animationElement->GetAttribute("speedRatio"),
									animation.SpeedRatio)
									|| !std::isfinite(animation.SpeedRatio)
									|| animation.SpeedRatio <= 0.0))
							{
								if (outError) *outError =
									L"Component animation SpeedRatio is invalid.";
								return false;
							}
							if (animationElement->HasAttribute("accelerationRatio")
								&& (!TryParseDouble(
									animationElement->GetAttribute("accelerationRatio"),
									animation.AccelerationRatio)
									|| !std::isfinite(animation.AccelerationRatio)
									|| animation.AccelerationRatio < 0.0
									|| animation.AccelerationRatio > 1.0))
							{
								if (outError) *outError =
									L"Component animation AccelerationRatio is invalid.";
								return false;
							}
							if (animationElement->HasAttribute("decelerationRatio")
								&& (!TryParseDouble(
									animationElement->GetAttribute("decelerationRatio"),
									animation.DecelerationRatio)
									|| !std::isfinite(animation.DecelerationRatio)
									|| animation.DecelerationRatio < 0.0
									|| animation.DecelerationRatio > 1.0))
							{
								if (outError) *outError =
									L"Component animation DecelerationRatio is invalid.";
								return false;
							}
							if (animation.AccelerationRatio
								+ animation.DecelerationRatio > 1.0)
							{
								if (outError) *outError =
									L"Component animation acceleration and deceleration ratios are invalid.";
								return false;
							}
							const auto fromResource = FromUtf8(
								animationElement->GetAttribute("fromResource"));
							const auto fromKind = FromUtf8(
								animationElement->GetAttribute("fromKind"));
							if (!fromResource.empty() && !fromKind.empty())
							{
								if (outError) *outError = L"Component visual-state animation From is ambiguous.";
								return false;
							}
							if (!fromResource.empty())
							{
								animation.HasFrom = true;
								animation.FromUsesResource = true;
								animation.FromResourceKey = fromResource;
							}
							else if (!fromKind.empty())
							{
								animation.HasFrom = true;
								if (!DesignerStyleSheetUtils::TryParseValueKind(
									fromKind, animation.From.Kind))
								{
									if (outError) *outError = L"Component visual-state animation From kind is invalid.";
									return false;
								}
								animation.From.Text = FromUtf8(
									animationElement->GetAttribute("from"));
							}
							const auto keyFrameElements = FindChildElements(
								animationElement, "keyFrame");
							const bool keyFrameAnimation = !keyFrameElements.empty();
							if (keyFrameAnimation && animation.HasFrom)
							{
								if (outError) *outError = L"Component key-frame animation cannot declare From.";
								return false;
							}
							const auto toResource = FromUtf8(
								animationElement->GetAttribute("toResource"));
							const auto toKind = FromUtf8(
								animationElement->GetAttribute("toKind"));
							const bool hasTo = !toResource.empty() || !toKind.empty()
								|| animationElement->HasAttribute("to");
							if (keyFrameAnimation && hasTo)
							{
								if (outError) *outError = L"Component key-frame animation cannot declare To.";
								return false;
							}
							if (!keyFrameAnimation && !toResource.empty()
								&& (!toKind.empty() || animationElement->HasAttribute("to")))
							{
								if (outError) *outError = L"Component visual-state animation To is ambiguous.";
								return false;
							}
							if (!keyFrameAnimation && !toResource.empty())
							{
								animation.HasTo = true;
								animation.ToUsesResource = true;
								animation.ToResourceKey = toResource;
							}
							else if (!keyFrameAnimation && hasTo)
							{
								animation.HasTo = true;
								if (toKind.empty()
									|| !DesignerStyleSheetUtils::TryParseValueKind(
										toKind, animation.To.Kind))
								{
									if (outError) *outError = L"Component visual-state animation To is invalid.";
									return false;
								}
								animation.To.Text = FromUtf8(
									animationElement->GetAttribute("to"));
							}
							const auto byResource = FromUtf8(
								animationElement->GetAttribute("byResource"));
							const auto byKind = FromUtf8(
								animationElement->GetAttribute("byKind"));
							const bool hasBy = !byResource.empty() || !byKind.empty()
								|| animationElement->HasAttribute("by");
							if (keyFrameAnimation && hasBy)
							{
								if (outError) *outError = L"Component key-frame animation cannot declare By.";
								return false;
							}
							if (!keyFrameAnimation && !byResource.empty()
								&& (!byKind.empty() || animationElement->HasAttribute("by")))
							{
								if (outError) *outError = L"Component visual-state animation By is ambiguous.";
								return false;
							}
							if (!keyFrameAnimation && !byResource.empty())
							{
								animation.HasBy = true;
								animation.ByUsesResource = true;
								animation.ByResourceKey = byResource;
							}
							else if (!keyFrameAnimation && hasBy)
							{
								animation.HasBy = true;
								if (byKind.empty()
									|| !DesignerStyleSheetUtils::TryParseValueKind(
										byKind, animation.By.Kind))
								{
									if (outError) *outError = L"Component visual-state animation By is invalid.";
									return false;
								}
								animation.By.Text = FromUtf8(
									animationElement->GetAttribute("by"));
							}
							const auto easing = animationElement->GetAttribute("easing");
							if (std::strcmp(easing.c_str(), "Linear") == 0)
								animation.Easing = DesignerEasingKind::Linear;
							else if (std::strcmp(easing.c_str(), "Quadratic") == 0)
								animation.Easing = DesignerEasingKind::Quadratic;
							else if (std::strcmp(easing.c_str(), "Cubic") == 0)
								animation.Easing = DesignerEasingKind::Cubic;
							else if (std::strcmp(easing.c_str(), "Sine") == 0)
								animation.Easing = DesignerEasingKind::Sine;
							else
							{
								if (outError) *outError = L"Component visual-state animation easing is invalid.";
								return false;
							}
							const auto easingMode = animationElement->GetAttribute("easingMode");
							if (std::strcmp(easingMode.c_str(), "EaseIn") == 0)
								animation.EasingMode = DesignerEasingMode::EaseIn;
							else if (std::strcmp(easingMode.c_str(), "EaseOut") == 0)
								animation.EasingMode = DesignerEasingMode::EaseOut;
							else if (std::strcmp(easingMode.c_str(), "EaseInOut") == 0)
								animation.EasingMode = DesignerEasingMode::EaseInOut;
							else
							{
								if (outError) *outError = L"Component visual-state animation easing mode is invalid.";
								return false;
							}
							for (const auto& keyFrameElement : keyFrameElements)
							{
								DesignerAnimationKeyFrame keyFrame;
								const auto frameKind = keyFrameElement->GetAttribute("kind");
								if (std::strcmp(frameKind.c_str(), "Discrete") == 0)
									keyFrame.Kind = DesignerKeyFrameKind::Discrete;
								else if (std::strcmp(frameKind.c_str(), "Linear") == 0)
									keyFrame.Kind = DesignerKeyFrameKind::Linear;
								else if (std::strcmp(frameKind.c_str(), "Easing") == 0)
									keyFrame.Kind = DesignerKeyFrameKind::Easing;
								else if (std::strcmp(frameKind.c_str(), "Spline") == 0)
									keyFrame.Kind = DesignerKeyFrameKind::Spline;
								else
								{
									if (outError) *outError = L"Component animation key-frame kind is invalid.";
									return false;
								}
								if (!TryParseIntegral(keyFrameElement->GetAttribute("keyTimeMs"),
									keyFrame.KeyTimeMilliseconds))
								{
									if (outError) *outError = L"Component animation KeyTime is invalid.";
									return false;
								}
								keyFrame.ResourceKey = FromUtf8(
									keyFrameElement->GetAttribute("resource"));
								const auto valueKind = FromUtf8(
									keyFrameElement->GetAttribute("valueKind"));
								if (!keyFrame.ResourceKey.empty() && !valueKind.empty())
								{
									if (outError) *outError = L"Component animation key-frame value is ambiguous.";
									return false;
								}
								keyFrame.UsesResource = !keyFrame.ResourceKey.empty();
								if (!keyFrame.UsesResource)
								{
									if (valueKind.empty()
										|| !DesignerStyleSheetUtils::TryParseValueKind(
											valueKind, keyFrame.Value.Kind))
									{
										if (outError) *outError = L"Component animation key-frame value is missing or invalid.";
										return false;
									}
									keyFrame.Value.Text = FromUtf8(
										keyFrameElement->GetAttribute("value"));
								}
								if (keyFrame.Kind == DesignerKeyFrameKind::Easing)
								{
									const auto frameEasing = keyFrameElement->GetAttribute("easing");
									if (std::strcmp(frameEasing.c_str(), "Linear") == 0)
										keyFrame.Easing = DesignerEasingKind::Linear;
									else if (std::strcmp(frameEasing.c_str(), "Quadratic") == 0)
										keyFrame.Easing = DesignerEasingKind::Quadratic;
									else if (std::strcmp(frameEasing.c_str(), "Cubic") == 0)
										keyFrame.Easing = DesignerEasingKind::Cubic;
									else if (std::strcmp(frameEasing.c_str(), "Sine") == 0)
										keyFrame.Easing = DesignerEasingKind::Sine;
									else
									{
										if (outError) *outError = L"Component animation key-frame easing is invalid.";
										return false;
									}
									const auto frameMode = keyFrameElement->GetAttribute("easingMode");
									if (std::strcmp(frameMode.c_str(), "EaseIn") == 0)
										keyFrame.EasingMode = DesignerEasingMode::EaseIn;
									else if (std::strcmp(frameMode.c_str(), "EaseOut") == 0)
										keyFrame.EasingMode = DesignerEasingMode::EaseOut;
									else if (std::strcmp(frameMode.c_str(), "EaseInOut") == 0)
										keyFrame.EasingMode = DesignerEasingMode::EaseInOut;
									else
									{
										if (outError) *outError = L"Component animation key-frame easing mode is invalid.";
										return false;
									}
								}
								else if (keyFrame.Kind == DesignerKeyFrameKind::Spline)
								{
									if (!TryReadFloatAttribute(keyFrameElement, "x1", keyFrame.KeySplineX1)
										|| !TryReadFloatAttribute(keyFrameElement, "y1", keyFrame.KeySplineY1)
										|| !TryReadFloatAttribute(keyFrameElement, "x2", keyFrame.KeySplineX2)
										|| !TryReadFloatAttribute(keyFrameElement, "y2", keyFrame.KeySplineY2)
										|| keyFrame.KeySplineX1 < 0.0f || keyFrame.KeySplineX1 > 1.0f
										|| keyFrame.KeySplineY1 < 0.0f || keyFrame.KeySplineY1 > 1.0f
										|| keyFrame.KeySplineX2 < 0.0f || keyFrame.KeySplineX2 > 1.0f
										|| keyFrame.KeySplineY2 < 0.0f || keyFrame.KeySplineY2 > 1.0f)
									{
										if (outError) *outError = L"Component animation KeySpline is invalid.";
										return false;
									}
								}
								animation.KeyFrames.push_back(std::move(keyFrame));
							}
							std::stable_sort(animation.KeyFrames.begin(),
								animation.KeyFrames.end(), [](const auto& left, const auto& right)
							{
								return left.KeyTimeMilliseconds < right.KeyTimeMilliseconds;
							});
							const bool objectPath = ClassifyStoryboardObjectPath(
								animation.PropertyName) != StoryboardObjectPathKind::None;
							const auto rootProperty = StoryboardAnimationRootProperty(
								animation.PropertyName);
							const bool duplicate = std::any_of(
								state.Setters.begin(), state.Setters.end(), [&](const auto& existing)
								{ return std::wcscmp(existing.TargetName.c_str(), animation.TargetName.c_str()) == 0
									&& std::wcscmp(existing.PropertyName.c_str(), rootProperty.c_str()) == 0; })
								|| std::any_of(state.Animations.begin(), state.Animations.end(),
									[&](const auto& existing)
									{
										if (std::wcscmp(existing.TargetName.c_str(), animation.TargetName.c_str()) != 0)
											return false;
									const bool existingPath = ClassifyStoryboardObjectPath(
										existing.PropertyName) != StoryboardObjectPathKind::None;
									const auto existingRoot = StoryboardAnimationRootProperty(
										existing.PropertyName);
									return std::wcscmp(existing.PropertyName.c_str(), animation.PropertyName.c_str()) == 0
										|| (std::wcscmp(existingRoot.c_str(), rootProperty.c_str()) == 0
											&& (!existingPath || !objectPath));
									});
							if (duplicate)
							{
								if (outError) *outError = L"Component visual-state Setter/animation target is duplicated.";
								return false;
							}
							const auto controlledKey = animation.TargetName + L"|"
								+ rootProperty;
							const auto controlled = std::find_if(
								visualStateControlledProperties.begin(),
								visualStateControlledProperties.end(),
								[&](const auto& existing)
								{ return std::wcscmp(existing.first.c_str(), controlledKey.c_str()) == 0; });
							if (controlled != visualStateControlledProperties.end()
								&& std::wcscmp(controlled->second.c_str(), group.Name.c_str()) != 0)
							{
								if (outError) *outError = L"Different visual-state groups animate the same property.";
								return false;
							}
							if (controlled == visualStateControlledProperties.end())
								visualStateControlledProperties.emplace_back(
									controlledKey, group.Name);
							state.Animations.push_back(std::move(animation));
						}
						group.States.push_back(std::move(state));
					}
					for (const auto& transitionElement : FindChildElements(
						groupElement, "visualTransition"))
					{
						DesignerVisualTransition transition;
						transition.FromState = FromUtf8(
							transitionElement->GetAttribute("from"));
						transition.ToState = FromUtf8(
							transitionElement->GetAttribute("to"));
						auto stateExists = [&](const std::wstring& name)
						{
							return name.empty() || std::any_of(
								group.States.begin(), group.States.end(),
								[&](const auto& state)
								{ return std::wcscmp(state.Name.c_str(), name.c_str()) == 0; });
						};
						if (!stateExists(transition.FromState)
							|| !stateExists(transition.ToState)
							|| std::any_of(group.Transitions.begin(),
								group.Transitions.end(), [&](const auto& existing)
								{
									return std::wcscmp(existing.FromState.c_str(),
										transition.FromState.c_str()) == 0
										&& std::wcscmp(existing.ToState.c_str(),
											transition.ToState.c_str()) == 0;
								}))
						{
							if (outError) *outError = L"Component visual transition states are missing or duplicated.";
							return false;
						}
						if (!TryParseIntegral(
							transitionElement->GetAttribute("generatedDurationMs"),
							transition.GeneratedDurationMilliseconds))
						{
							if (outError) *outError = L"Component visual transition duration is invalid.";
							return false;
						}
						const auto easing = transitionElement->GetAttribute(
							"generatedEasing");
						if (std::strcmp(easing.c_str(), "Linear") == 0)
							transition.GeneratedEasing = DesignerEasingKind::Linear;
						else if (std::strcmp(easing.c_str(), "Quadratic") == 0)
							transition.GeneratedEasing = DesignerEasingKind::Quadratic;
						else if (std::strcmp(easing.c_str(), "Cubic") == 0)
							transition.GeneratedEasing = DesignerEasingKind::Cubic;
						else if (std::strcmp(easing.c_str(), "Sine") == 0)
							transition.GeneratedEasing = DesignerEasingKind::Sine;
						else
						{
							if (outError) *outError = L"Component visual transition easing is invalid.";
							return false;
						}
						const auto mode = transitionElement->GetAttribute(
							"generatedEasingMode");
						if (std::strcmp(mode.c_str(), "EaseIn") == 0)
							transition.GeneratedEasingMode = DesignerEasingMode::EaseIn;
						else if (std::strcmp(mode.c_str(), "EaseOut") == 0)
							transition.GeneratedEasingMode = DesignerEasingMode::EaseOut;
						else if (std::strcmp(mode.c_str(), "EaseInOut") == 0)
							transition.GeneratedEasingMode = DesignerEasingMode::EaseInOut;
						else
						{
							if (outError) *outError = L"Component visual transition easing mode is invalid.";
							return false;
						}
						for (const auto& animationElement : FindChildElements(
							transitionElement, "animation"))
						{
							DesignerVisualStateAnimation animation;
							if (!ReadVisualStateAnimationSnapshot(
								animationElement, animation, outError)) return false;
							const bool objectPath = ClassifyStoryboardObjectPath(
								animation.PropertyName) != StoryboardObjectPathKind::None;
							const auto rootProperty = StoryboardAnimationRootProperty(
								animation.PropertyName);
							for (const auto& existing : transition.Animations)
							{
								if (std::wcscmp(existing.TargetName.c_str(),
									animation.TargetName.c_str()) != 0) continue;
								const bool existingPath = ClassifyStoryboardObjectPath(
									existing.PropertyName) != StoryboardObjectPathKind::None;
								const auto existingRoot = StoryboardAnimationRootProperty(
									existing.PropertyName);
								if (std::wcscmp(existing.PropertyName.c_str(),
									animation.PropertyName.c_str()) == 0
									|| (std::wcscmp(existingRoot.c_str(), rootProperty.c_str()) == 0
									&& (!existingPath || !objectPath)))
								{
									if (outError) *outError = L"Component visual transition animation target is duplicated.";
									return false;
								}
							}
							const auto controlledKey = animation.TargetName + L"|"
								+ rootProperty;
							const auto controlled = std::find_if(
								visualStateControlledProperties.begin(),
								visualStateControlledProperties.end(),
								[&](const auto& existing)
								{ return std::wcscmp(existing.first.c_str(),
									controlledKey.c_str()) == 0; });
							if (controlled != visualStateControlledProperties.end()
								&& std::wcscmp(controlled->second.c_str(),
									group.Name.c_str()) != 0)
							{
								if (outError) *outError = L"Different visual-state groups animate the same transition property.";
								return false;
							}
							if (controlled == visualStateControlledProperties.end())
								visualStateControlledProperties.emplace_back(
									controlledKey, group.Name);
							transition.Animations.push_back(std::move(animation));
						}
						group.Transitions.push_back(std::move(transition));
					}
					if (group.States.empty() || fallbackCount != 1)
					{
						if (outError) *outError = L"Component visual-state group requires exactly one fallback state.";
						return false;
					}
					definition.VisualStateGroups.push_back(std::move(group));
				}
				std::vector<std::wstring> beginStoryboardNames;
				std::vector<std::wstring> referencedStoryboardNames;
				for (const auto& triggerElement : FindChildElements(
					component, "componentEventTrigger"))
				{
					DesignerEventTrigger trigger;
					trigger.EventName = FromUtf8(
						triggerElement->GetAttribute("event"));
					const auto event = std::find_if(definition.Events.begin(),
						definition.Events.end(), [&](const auto& candidate)
						{ return std::wcscmp(candidate.Name.c_str(),
							trigger.EventName.c_str()) == 0; });
					if (event == definition.Events.end())
					{
						if (outError) *outError = L"Component EventTrigger event is invalid.";
						return false;
					}
					trigger.EventName = event->Name;
					for (const auto& child : triggerElement->ChildNodes())
					{
						if (!child || child->NodeType() != XmlNodeType::Element) continue;
						auto actionElement = std::static_pointer_cast<XmlElement>(child);
						const auto& actionName = actionElement->Name();
						DesignerEventTriggerAction action;
						if (actionName == "beginStoryboard")
						{
							action.Kind = DesignerStoryboardActionKind::Begin;
							action.StoryboardName = FromUtf8(
								actionElement->GetAttribute("name"));
							if (!action.StoryboardName.empty())
							{
								if (std::any_of(beginStoryboardNames.begin(),
									beginStoryboardNames.end(), [&](const auto& existing)
									{ return std::wcscmp(existing.c_str(),
										action.StoryboardName.c_str()) == 0; }))
								{
									if (outError) *outError = L"Component BeginStoryboard name is duplicated.";
									return false;
								}
								beginStoryboardNames.push_back(action.StoryboardName);
							}
							for (const auto& animationElement : FindChildElements(
								actionElement, "animation"))
							{
								DesignerVisualStateAnimation animation;
								if (!ReadVisualStateAnimationSnapshot(
									animationElement, animation, outError)) return false;
								const bool objectPath = ClassifyStoryboardObjectPath(
									animation.PropertyName) != StoryboardObjectPathKind::None;
								const auto rootProperty = StoryboardAnimationRootProperty(
									animation.PropertyName);
								for (const auto& existing : action.Animations)
								{
									if (std::wcscmp(existing.TargetName.c_str(),
										animation.TargetName.c_str()) != 0) continue;
									const bool existingPath = ClassifyStoryboardObjectPath(
										existing.PropertyName) != StoryboardObjectPathKind::None;
									const auto existingRoot = StoryboardAnimationRootProperty(
										existing.PropertyName);
									if (std::wcscmp(existing.PropertyName.c_str(),
										animation.PropertyName.c_str()) == 0
										|| (std::wcscmp(existingRoot.c_str(), rootProperty.c_str()) == 0
											&& (!existingPath || !objectPath)))
									{
										if (outError) *outError = L"Component BeginStoryboard animation target is duplicated.";
										return false;
									}
								}
								action.Animations.push_back(std::move(animation));
							}
							if (action.Animations.empty())
							{
								if (outError) *outError = L"Component BeginStoryboard is empty.";
								return false;
							}
						}
						else
						{
							if (actionName == "pauseStoryboard")
								action.Kind = DesignerStoryboardActionKind::Pause;
							else if (actionName == "resumeStoryboard")
								action.Kind = DesignerStoryboardActionKind::Resume;
							else if (actionName == "stopStoryboard")
								action.Kind = DesignerStoryboardActionKind::Stop;
							else
							{
								if (outError) *outError = L"Component EventTrigger action is invalid.";
								return false;
							}
							action.StoryboardName = FromUtf8(
								actionElement->GetAttribute("beginStoryboardName"));
							if (action.StoryboardName.empty())
							{
								if (outError) *outError = L"Component Storyboard control action is missing its name.";
								return false;
							}
							referencedStoryboardNames.push_back(action.StoryboardName);
						}
						trigger.Actions.push_back(std::move(action));
					}
					if (trigger.Actions.empty())
					{
						if (outError) *outError = L"Component EventTrigger has no actions.";
						return false;
					}
					definition.EventTriggers.push_back(std::move(trigger));
				}
				for (const auto& name : referencedStoryboardNames)
					if (std::none_of(beginStoryboardNames.begin(),
						beginStoryboardNames.end(), [&](const auto& candidate)
						{ return std::wcscmp(candidate.c_str(), name.c_str()) == 0; }))
					{
						if (outError) *outError = L"Component Storyboard control action references an unknown BeginStoryboard.";
						return false;
					}
				for (const auto& content : definition.ContentProperties)
					if (std::any_of(definition.Events.begin(), definition.Events.end(),
						[&](const auto& event)
						{
							return std::wcscmp(event.Name.c_str(), content.Name.c_str()) == 0;
						}))
					{
						if (outError) *outError = L"Component content property conflicts with an event.";
						return false;
					}
				if (auto templateElement = FindChildElement(component, "template"))
				{
					DesignValue templateValue;
					if (!ReadValue(templateElement, templateValue, outError)
						|| !templateValue.is_array()) return false;
					for (const auto& value : templateValue.ArrayItems())
					{
						DesignNode node;
						if (!TemplateNodeFromValue(
							value, node, outError, resourceBasePath)) return false;
						for (const auto& [target, source] : node.TemplateBindings)
						{
							(void)target;
							if (std::none_of(definition.Properties.begin(),
								definition.Properties.end(), [&](const auto& property)
							{
									return std::wcscmp(property.Name.c_str(), source.c_str()) == 0;
								}))
							{
								if (outError) *outError = L"Component template binding references an unknown property.";
								return false;
							}
						}
						definition.Template.push_back(std::move(node));
					}
					DesignDocument templateDocument;
					templateDocument.Nodes = definition.Template;
					templateDocument.RecalculateNextStableId();
					DesignDocumentGraph templateGraph;
					if (!DesignDocumentGraph::Build(
						templateDocument, templateGraph, outError)
						|| templateGraph.Roots().size() != 1)
					{
						if (outError && outError->empty())
							*outError = L"Component template must contain exactly one visual root.";
						return false;
					}
				}
				if (!definition.ContentProperties.empty() && definition.Template.empty())
				{
					if (outError) *outError = L"Component content properties require a template.";
					return false;
				}
				for (const auto& content : definition.ContentProperties)
				{
					const auto count = std::count_if(
						definition.Template.begin(), definition.Template.end(),
						[&](const auto& node)
						{
							return std::wcscmp(node.PresentedComponentContent.c_str(),
								content.Name.c_str()) == 0;
						});
					if (count != 1)
					{
						if (outError) *outError = L"Component content property must have exactly one presenter.";
						return false;
					}
				}
				for (auto& node : definition.Template)
				{
					if (node.PresentedComponentContent.empty()) continue;
					const auto content = std::find_if(
						definition.ContentProperties.begin(),
						definition.ContentProperties.end(), [&](const auto& property)
						{
							return std::wcscmp(property.Name.c_str(),
								node.PresentedComponentContent.c_str()) == 0;
						});
					if (content == definition.ContentProperties.end()
						|| !node.ComponentType.Empty()
						|| !IsComponentContentPresenterType(node.Type))
					{
						if (outError) *outError = L"Component template contains an invalid content presenter.";
						return false;
					}
					node.PresentedComponentContent = content->Name;
				}
				if (!definition.VisualStateGroups.empty()
					|| !definition.EventTriggers.empty())
				{
					std::wstring stateError;
					CuiRuntime::XamlTypePropertySchema hostSchema;
					if (!CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
						definition.BaseType, &definition, document,
						hostSchema, &stateError))
					{
						if (outError) *outError = L"Component visual-state host contract is invalid: "
							+ stateError;
						return false;
					}
					auto resolveTarget = [&](const std::wstring& targetName,
						CuiRuntime::XamlTypePropertySchema& schema)
					{
						const DesignComponentDefinition* targetComponent = nullptr;
						UIClass targetType = definition.BaseType;
						if (targetName.empty())
							targetComponent = &definition;
						else
						{
							const auto node = std::find_if(
								definition.Template.begin(), definition.Template.end(),
								[&](const auto& candidate)
								{ return std::wcscmp(candidate.Name.c_str(), targetName.c_str()) == 0; });
							if (node == definition.Template.end()) return false;
							targetType = node->Type;
							if (!node->ComponentType.Empty())
							{
								targetComponent = document.FindComponent(node->ComponentType);
								if (!targetComponent) return false;
							}
						}
						return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
							targetType, targetComponent, document, schema, &stateError);
					};
					auto validateTransitionAnimation = [&](const auto& animation)
					{
						const auto objectPathKind = ClassifyStoryboardObjectPath(
							animation.PropertyName);
						const bool objectPath = objectPathKind
							!= StoryboardObjectPathKind::None;
						ResolvedStoryboardObjectPath resolvedObjectPath;
						CuiRuntime::XamlTypePropertySchema targetSchema;
						const bool targetResolved = resolveTarget(
							animation.TargetName, targetSchema);
						const auto* metadata = !objectPath && targetResolved
							? targetSchema.FindProperty(animation.PropertyName) : nullptr;
						const bool compatible = objectPath
							? TryResolveStoryboardObjectPath(definition,
								animation.TargetName, animation.PropertyName,
								animation.Kind, resolvedObjectPath, &stateError)
							: metadata && metadata->CanWrite()
								&& (animation.Kind == DesignerAnimationKind::Object
									? true
									: animation.Kind == DesignerAnimationKind::Double
									? metadata->ValueKind() == BindingValueKind::Int
										|| metadata->ValueKind() == BindingValueKind::Int64
										|| metadata->ValueKind() == BindingValueKind::Float
										|| metadata->ValueKind() == BindingValueKind::Double
									: animation.Kind == DesignerAnimationKind::Thickness
										? metadata->ValueKind() == BindingValueKind::Object
											&& metadata->ValueType()
												== std::type_index(typeid(Thickness))
									: animation.Kind == DesignerAnimationKind::Point
										? metadata->ValueKind() == BindingValueKind::Object
											&& metadata->ValueType()
												== std::type_index(typeid(cui::core::Point))
									: animation.Kind == DesignerAnimationKind::Vector
										? metadata->ValueKind() == BindingValueKind::Object
											&& metadata->ValueType()
												== std::type_index(typeid(cui::core::Vector))
									: animation.Kind == DesignerAnimationKind::Rect
										? metadata->ValueKind() == BindingValueKind::Object
											&& metadata->ValueType()
												== std::type_index(typeid(cui::core::Rect))
									: animation.Kind == DesignerAnimationKind::Size
										? metadata->ValueKind() == BindingValueKind::Object
											&& metadata->ValueType()
												== std::type_index(typeid(cui::core::Size))
									: animation.Kind == DesignerAnimationKind::Matrix
										? metadata->ValueKind() == BindingValueKind::Object
											&& metadata->ValueType()
												== std::type_index(typeid(D2D1_MATRIX_3X2_F))
									: metadata->ValueKind() == BindingValueKind::Object
										&& metadata->ValueType()
											== std::type_index(typeid(D2D1_COLOR_F)));
						if (!targetResolved || !compatible) return false;
						auto validateEndpoint = [&](const DesignerStyleValue& literal,
							bool usesResource, const std::wstring& resourceKey,
							bool isDelta = false)
						{
							const DesignerStyleValue* authored = &literal;
							if (usesResource)
							{
								const auto resource = std::find_if(
									document.StyleSheet.Resources.begin(),
									document.StyleSheet.Resources.end(),
									[&](const auto& candidate)
									{ return std::wcscmp(candidate.Key.c_str(),
										resourceKey.c_str()) == 0; });
								if (resource == document.StyleSheet.Resources.end())
									return false;
								authored = &resource->Value;
							}
							BindingValue source;
							if (!DesignerStyleSheetUtils::TryConvertValue(
								*authored, source, &stateError,
								document.ResourceBasePath, document.Resources)) return false;
							if (objectPath)
								return ValidateStoryboardObjectPathValue(
									objectPathKind, source, isDelta);
							BindingValue converted;
							return metadata->TryConvert(source, converted);
						};
						return animation.KeyFrames.empty()
							? (!animation.HasFrom || validateEndpoint(animation.From,
								animation.FromUsesResource, animation.FromResourceKey))
								&& (!animation.HasTo || validateEndpoint(animation.To,
									animation.ToUsesResource, animation.ToResourceKey))
								&& (!animation.HasBy || validateEndpoint(animation.By,
									animation.ByUsesResource, animation.ByResourceKey, true))
							: std::all_of(animation.KeyFrames.begin(),
								animation.KeyFrames.end(), [&](const auto& keyFrame)
								{
									return validateEndpoint(keyFrame.Value,
										keyFrame.UsesResource, keyFrame.ResourceKey);
								});
					};
					for (const auto& group : definition.VisualStateGroups)
					{
						for (const auto& transition : group.Transitions)
							for (const auto& animation : transition.Animations)
								if (!validateTransitionAnimation(animation))
								{
									if (outError) *outError = L"Component visual-transition animation value, resource, target, or type is invalid.";
									return false;
								}
						for (const auto& state : group.States)
						{
							for (const auto& condition : state.Conditions)
							{
								const auto* metadata = hostSchema.FindProperty(
									condition.PropertyName);
								BindingValue source;
								BindingValue converted;
								if (!metadata || !metadata->CanRead()
									|| !DesignerStyleSheetUtils::TryConvertValue(
										condition.Value, source, &stateError,
										document.ResourceBasePath, document.Resources)
									|| !metadata->TryConvert(source, converted))
								{
									if (outError) *outError = L"Component visual-state condition property or value is invalid.";
									return false;
								}
							}
							for (const auto& setter : state.Setters)
							{
								CuiRuntime::XamlTypePropertySchema targetSchema;
								const bool targetResolved = resolveTarget(
									setter.TargetName, targetSchema);
								const auto* metadata = targetResolved
									? targetSchema.FindProperty(setter.PropertyName) : nullptr;
								const DesignerStyleValue* authored = &setter.Literal;
								if (setter.UsesResource)
								{
									const auto resource = std::find_if(
										document.StyleSheet.Resources.begin(),
										document.StyleSheet.Resources.end(),
										[&](const auto& candidate)
										{ return std::wcscmp(candidate.Key.c_str(),
											setter.ResourceKey.c_str()) == 0; });
									if (resource == document.StyleSheet.Resources.end())
									{
										if (outError) *outError = L"Component visual-state setter resource is missing.";
										return false;
									}
									authored = &resource->Value;
								}
								BindingValue source;
								BindingValue converted;
								if (!targetResolved || !metadata || !metadata->CanWrite()
									|| !DesignerStyleSheetUtils::TryConvertValue(
										*authored, source, &stateError,
										document.ResourceBasePath, document.Resources)
									|| !metadata->TryConvert(source, converted))
								{
									if (outError) *outError = L"Component visual-state setter target, property, or value is invalid.";
									return false;
								}
							}
							for (const auto& animation : state.Animations)
							{
								const auto objectPathKind = ClassifyStoryboardObjectPath(
									animation.PropertyName);
								const bool objectPath = objectPathKind
									!= StoryboardObjectPathKind::None;
								ResolvedStoryboardObjectPath resolvedObjectPath;
								CuiRuntime::XamlTypePropertySchema targetSchema;
								const bool targetResolved = resolveTarget(
									animation.TargetName, targetSchema);
								const auto* metadata = !objectPath && targetResolved
									? targetSchema.FindProperty(animation.PropertyName) : nullptr;
								const bool compatible = objectPath
									? TryResolveStoryboardObjectPath(definition,
										animation.TargetName, animation.PropertyName,
										animation.Kind, resolvedObjectPath, &stateError)
									: metadata && metadata->CanWrite()
										&& (animation.Kind == DesignerAnimationKind::Object
											? true
											: animation.Kind == DesignerAnimationKind::Double
											? metadata->ValueKind() == BindingValueKind::Int
												|| metadata->ValueKind() == BindingValueKind::Int64
												|| metadata->ValueKind() == BindingValueKind::Float
												|| metadata->ValueKind() == BindingValueKind::Double
											: animation.Kind == DesignerAnimationKind::Thickness
												? metadata->ValueKind() == BindingValueKind::Object
													&& metadata->ValueType()
														== std::type_index(typeid(Thickness))
											: animation.Kind == DesignerAnimationKind::Point
												? metadata->ValueKind() == BindingValueKind::Object
													&& metadata->ValueType()
														== std::type_index(typeid(cui::core::Point))
											: animation.Kind == DesignerAnimationKind::Vector
												? metadata->ValueKind() == BindingValueKind::Object
													&& metadata->ValueType()
														== std::type_index(typeid(cui::core::Vector))
											: animation.Kind == DesignerAnimationKind::Rect
												? metadata->ValueKind() == BindingValueKind::Object
													&& metadata->ValueType()
														== std::type_index(typeid(cui::core::Rect))
											: animation.Kind == DesignerAnimationKind::Size
												? metadata->ValueKind() == BindingValueKind::Object
													&& metadata->ValueType()
														== std::type_index(typeid(cui::core::Size))
											: animation.Kind == DesignerAnimationKind::Matrix
												? metadata->ValueKind() == BindingValueKind::Object
													&& metadata->ValueType()
														== std::type_index(typeid(D2D1_MATRIX_3X2_F))
											: metadata->ValueKind() == BindingValueKind::Object
												&& metadata->ValueType()
													== std::type_index(typeid(D2D1_COLOR_F)));
								if (!targetResolved || !compatible)
								{
									if (outError) *outError = objectPath
										&& !stateError.empty()
										? stateError
										: L"Component visual-state animation target or type is invalid.";
									return false;
								}
								auto validateEndpoint = [&](const DesignerStyleValue& literal,
									bool usesResource, const std::wstring& resourceKey,
									bool isDelta = false)
								{
									const DesignerStyleValue* authored = &literal;
									if (usesResource)
									{
										const auto resource = std::find_if(
											document.StyleSheet.Resources.begin(),
											document.StyleSheet.Resources.end(),
											[&](const auto& candidate)
											{ return std::wcscmp(candidate.Key.c_str(),
												resourceKey.c_str()) == 0; });
										if (resource == document.StyleSheet.Resources.end())
											return false;
										authored = &resource->Value;
									}
									BindingValue source;
									if (!DesignerStyleSheetUtils::TryConvertValue(
										*authored, source, &stateError,
										document.ResourceBasePath, document.Resources))
										return false;
									if (objectPath)
										return ValidateStoryboardObjectPathValue(
											objectPathKind, source, isDelta);
									BindingValue converted;
									return metadata->TryConvert(source, converted);
								};
								const bool endpointsValid = animation.KeyFrames.empty()
									? (!animation.HasFrom || validateEndpoint(
										animation.From, animation.FromUsesResource,
										animation.FromResourceKey))
										&& (!animation.HasTo || validateEndpoint(animation.To,
											animation.ToUsesResource,
											animation.ToResourceKey))
										&& (!animation.HasBy || validateEndpoint(animation.By,
											animation.ByUsesResource,
											animation.ByResourceKey, true))
									: std::all_of(animation.KeyFrames.begin(),
										animation.KeyFrames.end(), [&](const auto& keyFrame)
										{
											return validateEndpoint(keyFrame.Value,
												keyFrame.UsesResource, keyFrame.ResourceKey);
										});
								if (!endpointsValid)
								{
									if (outError) *outError = L"Component visual-state animation value or resource is invalid.";
									return false;
								}
							}
						}
					}
					for (const auto& trigger : definition.EventTriggers)
						for (const auto& action : trigger.Actions)
							for (const auto& animation : action.Animations)
								if (!validateTransitionAnimation(animation))
								{
									if (outError) *outError = L"Component EventTrigger animation value, resource, target, or type is invalid.";
									return false;
								}
				}
				document.Components.push_back(std::move(definition));
			}
		}
	}
	for (const auto& rule : document.StyleSheet.Rules)
	{
		if (rule.ComponentType.Empty()) continue;
		const auto* component = document.FindComponent(rule.ComponentType);
		if (!component || !rule.HasType || component->BaseType != rule.Type)
		{
			if (outError) *outError = L"样式规则引用了不存在或 BaseType 不匹配的组件。";
			return false;
		}
	}

	std::unordered_set<std::wstring> nameSet;
	std::unordered_set<int> idSet;
	for (const auto& control : FindChildElements(controls, "control"))
	{
		if (!ValidateElementShape(
			control,
			{ "id", "name", "type", "xamlNamespace", "xamlName",
				"componentPrefix", "componentName", "componentNamespace",
				"componentContentProperty", "presentedComponentContent",
				"order", "locked", "parentId", "parent" },
			{ "properties", "structure", "events", "bindings",
				"commandBindings", "inputBindings", "localResources" },
			L"Current control snapshot", outError))
		{
			return false;
		}
		DesignNode node;
		node.Name = FromUtf8(control->GetAttribute("name"));
		std::string type = control->GetAttribute("type");
		if (node.Name.empty()
			|| !TryParseConstructibleUIClass(type, node.Type))
		{
			if (outError) *outError = L"Control entry is missing name/type or uses an unsupported type.";
			return false;
		}
		if (nameSet.find(node.Name) != nameSet.end())
		{
			if (outError) *outError = L"Duplicate control Name: " + node.Name;
			return false;
		}
		nameSet.insert(node.Name);

		if (!TryReadIntegralAttribute(control, "id", node.Id)
			|| node.Id < 1)
		{
			if (outError) *outError = L"Control entry is missing a valid stable id: " + node.Name;
			return false;
		}
		if (!idSet.insert(node.Id).second)
		{
			if (outError) *outError = L"Duplicate control stable id: " + std::to_wstring(node.Id);
			return false;
		}
		if (control->HasAttribute("parentId")
			&& (!TryReadIntegralAttribute(control, "parentId", node.ParentId)
				|| node.ParentId < 1))
		{
			if (outError) *outError = L"Control entry has an invalid parentId: " + node.Name;
			return false;
		}
		node.ParentRef = FromUtf8(control->GetAttribute("parent"));
		if (control->HasAttribute("xamlNamespace")
			|| control->HasAttribute("xamlName"))
		{
			node.XamlType.NamespaceUri = FromUtf8(
				control->GetAttribute("xamlNamespace"));
			node.XamlType.LocalName = FromUtf8(
				control->GetAttribute("xamlName"));
			if (!node.XamlType.Valid())
			{
				if (outError) *outError = L"Control entry has an invalid XAML type identity: "
					+ node.Name;
				return false;
			}
		}
		node.ComponentContentProperty = FromUtf8(
			control->GetAttribute("componentContentProperty"));
		node.PresentedComponentContent = FromUtf8(
			control->GetAttribute("presentedComponentContent"));
		if (!node.PresentedComponentContent.empty())
		{
			if (outError) *outError = L"Public controls cannot declare component content presenters: "
				+ node.Name;
			return false;
		}
		if (control->HasAttribute("componentName"))
		{
			node.ComponentType.XamlPrefix = FromUtf8(
				control->GetAttribute("componentPrefix"));
			node.ComponentType.XamlName = FromUtf8(
				control->GetAttribute("componentName"));
			node.ComponentType.XamlNamespace = FromUtf8(
				control->GetAttribute("componentNamespace"));
		}
		if (!TryReadIntegralAttribute(control, "order", node.Order))
		{
			node.Order = -1;
		}
		if (control->HasAttribute("locked")
			&& !TryReadBoolAttribute(control, "locked", node.Locked))
		{
			if (outError) *outError = L"Control entry has an invalid locked value: "
				+ node.Name;
			return false;
		}

		auto properties = FindChildElement(control, "properties");
		DesignValue encodedProperties = DesignValue::object();
		if (properties && !ReadValue(properties, encodedProperties, outError))
			return false;
		if (!DecodeDesignNodeProperties(
			encodedProperties, node.Properties, outError))
		{
			if (outError && !outError->empty())
				*outError = L"Control " + node.Name + L" properties: " + *outError;
			return false;
		}

		auto structure = FindChildElement(control, "structure");
		DesignValue encodedStructure = DesignValue::object();
		if (structure && !ReadValue(structure, encodedStructure, outError))
			return false;
		if (!DecodeDesignNodeStructure(
			node.Type, encodedStructure, node.Structure, outError))
		{
			if (outError && !outError->empty())
				*outError = L"Control " + node.Name + L" structure: " + *outError;
			return false;
		}

		DesignValue encodedEvents = DesignValue::object();
		if (auto events = FindChildElement(control, "events");
			events && !ReadValue(events, encodedEvents, outError)) return false;
		if (!DecodeDesignNodeEvents(encodedEvents, node.Events, outError))
		{
			if (outError && !outError->empty())
				*outError = L"Control " + node.Name + L" events: " + *outError;
			return false;
		}

		DesignValue encodedBindings = DesignValue::object();
		if (auto bindings = FindChildElement(control, "bindings");
			bindings && !ReadValue(bindings, encodedBindings, outError)) return false;
		if (!DecodeDesignNodeBindings(encodedBindings, node.Bindings, outError))
		{
			if (outError && !outError->empty())
				*outError = L"Control " + node.Name + L" bindings: " + *outError;
			return false;
		}
		DesignValue encodedCommandBindings = DesignValue::array();
		if (auto bindings = FindChildElement(control, "commandBindings");
			bindings && !ReadValue(bindings, encodedCommandBindings, outError))
			return false;
		if (!DecodeDesignCommandBindings(
			encodedCommandBindings, node.CommandBindings, outError))
		{
			if (outError && !outError->empty())
				*outError = L"Control " + node.Name
					+ L" command bindings: " + *outError;
			return false;
		}
		DesignValue encodedInputBindings = DesignValue::array();
		if (auto bindings = FindChildElement(control, "inputBindings");
			bindings && !ReadValue(bindings, encodedInputBindings, outError))
			return false;
		if (!DecodeDesignInputBindings(
			encodedInputBindings, node.InputBindings, outError))
		{
			if (outError && !outError->empty())
				*outError = L"Control " + node.Name
					+ L" input bindings: " + *outError;
			return false;
		}
		if (!ReadLocalResourcesSnapshot(
			control, node.LocalResources, node.LocalObjectResources,
			document.ResourceBasePath,
			document.Resources, outError)) return false;

		document.Nodes.push_back(std::move(node));
	}

	std::unordered_map<int, DesignNode*> nodeById;
	nodeById.reserve(document.Nodes.size());
	for (auto& node : document.Nodes)
	{
		nodeById.emplace(node.Id, &node);
	}

	for (auto& node : document.Nodes)
	{
		if (node.ParentId > 0)
		{
			const auto parent = nodeById.find(node.ParentId);
			// ID is authoritative; keep the name reference canonical and human-readable.
			if (parent != nodeById.end())
				node.ParentRef = parent->second->Name;
		}
	}

	DesignDocumentGraph graph;
	if (!DesignDocumentGraph::Build(document, graph, outError)) return false;
	for (const auto& node : document.Nodes)
	{
		if (node.ComponentType.Empty()) continue;
		const auto* component = document.FindComponent(
			document.Nodes, node, node.ComponentType);
		if (!component || component->BaseType != node.Type)
		{
			if (outError) *outError = L"Control entry references an invalid component: "
				+ node.Name;
			return false;
		}
	}
	for (const auto& node : document.Nodes)
	{
		if (node.LocalResources.Rules.empty()) continue;
		auto visible = VisibleStyleScope(document, node);
		DesignerStyleSheetUtils::Canonicalize(visible);
		if (!DesignerStyleSheetUtils::Validate(
			visible, outError, document.ResourceBasePath,
			document.Resources)) return false;
	}
	for (auto& node : document.Nodes)
	{
		const DesignNode* parent = nullptr;
		if (node.ParentId > 0)
		{
			const auto found = nodeById.find(node.ParentId);
			if (found != nodeById.end()) parent = found->second;
		}
		if (parent && !parent->ComponentType.Empty())
		{
			const auto* definition = document.FindComponent(
				document.Nodes, *parent, parent->ComponentType);
			if (!definition)
			{
				if (outError) *outError = L"Component child references a missing component definition: "
					+ node.Name;
				return false;
			}
			const auto content = std::find_if(
				definition->ContentProperties.begin(),
				definition->ContentProperties.end(), [&](const auto& property)
				{
					return std::wcscmp(property.Name.c_str(),
						node.ComponentContentProperty.c_str()) == 0;
				});
			if (node.ComponentContentProperty.empty()
				|| content == definition->ContentProperties.end())
			{
				if (outError) *outError = L"Component child references an invalid visual content property: "
					+ node.Name;
				return false;
			}
			node.ComponentContentProperty = content->Name;
			if (content->Cardinality ==
				DesignerComponentContentCardinality::Single)
			{
				const auto count = std::count_if(
					document.Nodes.begin(), document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == parent->Id
							&& std::wcscmp(
								candidate.ComponentContentProperty.c_str(),
								content->Name.c_str()) == 0;
					});
				if (count > 1)
				{
					if (outError) *outError = L"Component single visual content property contains multiple roots.";
					return false;
				}
			}
		}
		else if (!node.ComponentContentProperty.empty())
		{
			if (outError) *outError = L"Non-component parent cannot own a component visual content property: "
				+ node.Name;
			return false;
		}
	}
	if (!DesignDataResourceUtils::ValidateAndCanonicalize(document, outError))
		return false;
	DesignDocumentEventIndex eventIndex;
	if (!DesignDocumentEventIndex::Build(document, eventIndex, outError))
		return false;
	if (!document.ValidateCommandTargetReferences(outError))
		return false;
	output = std::move(document);
	return true;
}
}
