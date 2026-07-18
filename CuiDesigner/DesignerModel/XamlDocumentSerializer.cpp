#include "XamlDocumentSerializer.h"

#include "AtomicFile.h"
#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "DesignDocumentMaterializer.h"
#include "../../XmlLite/include/Xml.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"

#include <Convert.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace DesignerModel
{
namespace
{
	using namespace System::Xml;
	using Element = std::shared_ptr<XmlElement>;

	std::string ToUtf8(const std::wstring& value)
	{
		return Convert::UnicodeToUtf8(value);
	}

	std::wstring FromUtf8(const std::string& value)
	{
		return Convert::Utf8ToUnicode(value);
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	bool Equals(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::string BoolText(bool value)
	{
		return value ? "true" : "false";
	}

	std::wstring NumberText(double value, int precision = 9)
	{
		std::wostringstream stream;
		stream << std::setprecision(precision) << value;
		return stream.str();
	}

	Element Append(
		XmlDocument& document,
		const Element& parent,
		const std::string& name)
	{
		auto child = document.CreateElement(name);
		parent->AppendChild(child);
		return child;
	}

	void Set(const Element& element, const char* name, const std::wstring& value)
	{
		element->SetAttribute(name, ToUtf8(value));
	}

	void WriteDesignValue(
		XmlDocument& document,
		const Element& element,
		const DesignValue& value)
	{
		if (value.is_object())
		{
			element->SetAttribute("type", "object");
			for (const auto& [name, childValue] : value.ObjectItems())
			{
				auto member = Append(document, element, "d:Member");
				member->SetAttribute("name", name);
				WriteDesignValue(document, member, childValue);
			}
			return;
		}
		if (value.is_array())
		{
			element->SetAttribute("type", "array");
			for (const auto& childValue : value.ArrayItems())
				WriteDesignValue(
					document, Append(document, element, "d:Item"), childValue);
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
			element->SetInnerText(BoolText(value.get<bool>()));
			return;
		}
		if (value.is_number_integer())
		{
			element->SetAttribute("type", "integer");
			element->SetInnerText(std::to_string(value.get<long long>()));
			return;
		}
		if (value.is_number_unsigned())
		{
			element->SetAttribute("type", "unsigned");
			element->SetInnerText(std::to_string(value.get<unsigned long long>()));
			return;
		}
		if (value.is_number_float())
		{
			element->SetAttribute("type", "float");
			element->SetInnerText(ToUtf8(NumberText(value.get<double>(), 15)));
			return;
		}
		element->SetAttribute("type", "string");
		element->SetInnerText(value.ToString());
	}

	std::wstring ColorText(double r, double g, double b, double a)
	{
		// Float components retain designer values that are not byte-aligned;
		// #AARRGGBB would silently quantize them during a save/reload cycle.
		return NumberText(r, 9) + L", " + NumberText(g, 9) + L", "
			+ NumberText(b, 9) + L", " + NumberText(a, 9);
	}

	std::wstring ColorText(const D2D1_COLOR_F& color)
	{
		return ColorText(color.r, color.g, color.b, color.a);
	}

	std::optional<std::wstring> ColorText(const DesignValue& value)
	{
		if (!value.is_object()) return std::nullopt;
		return ColorText(
			value.value("r", 0.0), value.value("g", 0.0),
			value.value("b", 0.0), value.value("a", 1.0));
	}

	std::optional<std::wstring> ThicknessText(const DesignValue& value)
	{
		if (!value.is_object()) return std::nullopt;
		const auto left = value.value("l", 0.0);
		const auto top = value.value("t", 0.0);
		const auto right = value.value("r", 0.0);
		const auto bottom = value.value("b", 0.0);
		if (left == top && left == right && left == bottom)
			return NumberText(left);
		if (left == right && top == bottom)
			return NumberText(left) + L", " + NumberText(top);
		return NumberText(left) + L", " + NumberText(top) + L", "
			+ NumberText(right) + L", " + NumberText(bottom);
	}

	std::wstring AnchorText(int anchors)
	{
		std::wstring result;
		auto append = [&](const wchar_t* name)
		{
			if (!result.empty()) result += L", ";
			result += name;
		};
		if ((anchors & AnchorStyles::Left) != 0) append(L"Left");
		if ((anchors & AnchorStyles::Top) != 0) append(L"Top");
		if ((anchors & AnchorStyles::Right) != 0) append(L"Right");
		if ((anchors & AnchorStyles::Bottom) != 0) append(L"Bottom");
		return result.empty() ? L"None" : result;
	}

	bool ContainsName(const DesignValue& object, const std::wstring& name)
	{
		if (!object.is_object()) return false;
		for (const auto& [key, value] : object.ObjectItems())
		{
			(void)value;
			if (Equals(FromUtf8(key), name)) return true;
		}
		return false;
	}

	bool HasBinding(const DesignNode& node, const std::wstring& property)
	{
		return ContainsName(node.Bindings, property);
	}

	bool HasMetadata(const DesignNode& node, const std::wstring& property)
	{
		return node.Props.is_object() && node.Props.contains("metadata")
			&& ContainsName(node.Props["metadata"], property);
	}

	std::wstring BindingMarkup(const DesignValue& value)
	{
		if (!value.is_object())
			throw std::invalid_argument("XAML binding value must be an object");
		const auto source = FromUtf8(value.value("source", std::string{}));
		const auto modeValue = value.value(
			"mode", static_cast<int>(BindingMode::OneWay));
		const auto updateValue = value.value(
			"updateMode", static_cast<int>(DataSourceUpdateMode::OnPropertyChanged));
		if (source.empty()
			|| modeValue < static_cast<int>(BindingMode::OneWay)
			|| modeValue > static_cast<int>(BindingMode::OneTime)
			|| updateValue < static_cast<int>(DataSourceUpdateMode::OnPropertyChanged)
			|| updateValue > static_cast<int>(DataSourceUpdateMode::Never))
			throw std::invalid_argument("XAML binding contains an invalid path or mode");
		std::wstring result = L"{Binding " + source
			+ L", Mode=" + DesignerBindingUtils::BindingModeName(
				static_cast<BindingMode>(modeValue))
			+ L", UpdateMode=" + DesignerBindingUtils::UpdateModeName(
				static_cast<DataSourceUpdateMode>(updateValue));
		const auto converter = FromUtf8(value.value("converter", std::string{}));
		if (!converter.empty()) result += L", Converter=" + converter;
		result += L"}";
		return result;
	}

	std::wstring GridLengthText(const DesignValue& value)
	{
		if (!value.is_object())
			throw std::invalid_argument("Grid length must be an object");
		const auto amount = value.value("value", 1.0);
		const auto unit = value.value("unit", std::string("Auto"));
		if (unit == "Auto") return L"Auto";
		if (unit == "Star")
			return amount == 1.0 ? L"*" : NumberText(amount) + L"*";
		if (unit == "Pixel") return NumberText(amount);
		if (unit == "Percent") return NumberText(amount) + L"%";
		throw std::invalid_argument("Grid length contains an unknown unit");
	}

	class Writer final
	{
	public:
		Writer(const DesignDocument& document, XmlDocument& xml)
			: _document(document), _xml(xml)
		{
			std::wstring error;
			if (!_document.CodeBehind.Validate(&error)
				|| !DesignerDataContextSchemaUtils::Validate(
				_document.DataContextSchema, &error)
				|| !DesignerStyleSheetUtils::Validate(
					_document.StyleSheet, &error)
				|| !DesignDocumentGraph::Build(_document, _graph, &error))
				throw std::invalid_argument(ToUtf8(error));
			DesignDocumentEventIndex eventIndex;
			if (!DesignDocumentEventIndex::Build(
				_document, eventIndex, &error))
				throw std::invalid_argument(ToUtf8(error));
		}

		Element Write()
		{
			auto root = _xml.CreateElement("Form");
			root->SetAttribute("xmlns", "urn:cui");
			root->SetAttribute(
				"xmlns:x", "http://schemas.microsoft.com/winfx/2006/xaml");
			root->SetAttribute("xmlns:d", "urn:cui:designer");
			std::map<std::wstring, std::wstring> customNamespaces;
			for (const auto& node : _document.Nodes)
			{
				if (node.CustomType.Empty()) continue;
				const auto& custom = node.CustomType;
				if (custom.XamlPrefix.empty() || custom.XamlName.empty()
					|| custom.XamlNamespace.empty() || custom.CppType.empty()
					|| custom.Header.empty()
					|| Equals(custom.XamlPrefix, L"x")
					|| Equals(custom.XamlPrefix, L"d"))
					throw std::invalid_argument("Invalid custom control descriptor");
				const auto [found, inserted] = customNamespaces.emplace(
					custom.XamlPrefix, custom.XamlNamespace);
				if (!inserted && found->second != custom.XamlNamespace)
					throw std::invalid_argument(
						"Custom XAML prefix maps to multiple namespaces");
			}
			for (const auto& [prefix, uri] : customNamespaces)
				root->SetAttribute(
					"xmlns:" + ToUtf8(prefix), ToUtf8(uri));
			WriteForm(root);
			WriteSchema(root);
			WriteResources(root);
			for (const auto graphIndex : _graph.Roots())
				WriteControl(
					_document.Nodes[_graph.Nodes()[graphIndex].SourceIndex],
					root, false);
			if (_written.size() != _document.Nodes.size())
				throw std::invalid_argument(
					"XAML writer could not represent an unresolved synthetic parent");
			return root;
		}

	private:
		const DesignDocument& _document;
		XmlDocument& _xml;
		DesignDocumentGraph _graph;
		std::unordered_set<int> _written;

		void WriteForm(const Element& root)
		{
			Set(root, "x:Name", _document.Form.Name);
			if (!_document.CodeBehind.ClassName.empty())
				Set(root, "x:Class", _document.CodeBehind.ClassName);
			if (!_document.CodeBehind.RelativeBasePath.empty())
				Set(root, "d:CodeBehind",
					_document.CodeBehind.RelativeBasePath);
			Set(root, "Text", _document.Form.Text);
			Set(root, "X", std::to_wstring(_document.Form.Location.x));
			Set(root, "Y", std::to_wstring(_document.Form.Location.y));
			Set(root, "Width", std::to_wstring(_document.Form.Size.cx));
			Set(root, "Height", std::to_wstring(_document.Form.Size.cy));
			if (!_document.Form.FontName.empty())
				Set(root, "FontName", _document.Form.FontName);
			Set(root, "FontSize", NumberText(_document.Form.FontSize));
			Set(root, "BackColor", ColorText(_document.Form.BackColor));
			Set(root, "ForeColor", ColorText(_document.Form.ForeColor));
			root->SetAttribute("ShowInTaskBar", BoolText(_document.Form.ShowInTaskBar));
			root->SetAttribute("TopMost", BoolText(_document.Form.TopMost));
			root->SetAttribute("Enable", BoolText(_document.Form.Enable));
			root->SetAttribute("Visible", BoolText(_document.Form.Visible));
			root->SetAttribute("VisibleHead", BoolText(_document.Form.VisibleHead));
			root->SetAttribute("HeadHeight", std::to_string(_document.Form.HeadHeight));
			root->SetAttribute("MinBox", BoolText(_document.Form.MinBox));
			root->SetAttribute("MaxBox", BoolText(_document.Form.MaxBox));
			root->SetAttribute("CloseBox", BoolText(_document.Form.CloseBox));
			root->SetAttribute("CenterTitle", BoolText(_document.Form.CenterTitle));
			root->SetAttribute("AllowResize", BoolText(_document.Form.AllowResize));
			for (const auto& [event, handler] : _document.Form.EventHandlers)
			{
				if (event.empty() || handler.empty()) continue;
				Set(root, ToUtf8(event).c_str(),
					DesignerEventCatalog::IsLegacyEnabledValue(handler)
						? std::wstring(L"Auto") : handler);
			}
		}

		void WriteSchema(const Element& root)
		{
			if (_document.DataContextSchema.empty()) return;
			auto schema = Append(_xml, root, "Form.DataContextSchema");
			auto properties = _document.DataContextSchema;
			DesignerDataContextSchemaUtils::Canonicalize(properties);
			for (const auto& property : properties)
			{
				auto item = Append(_xml, schema, "Property");
				Set(item, "Path", property.Path);
				Set(item, "Kind",
					DesignerDataContextSchemaUtils::ValueKindName(property.ValueKind));
				item->SetAttribute("CanRead", BoolText(property.CanRead));
				item->SetAttribute("CanWrite", BoolText(property.CanWrite));
				item->SetAttribute("CanObserve", BoolText(property.CanObserve));
			}
		}

		void WriteResources(const Element& root)
		{
			if (_document.StyleSheet.Empty()) return;
			auto resources = Append(_xml, root, "Form.Resources");
			auto styleSheet = _document.StyleSheet;
			DesignerStyleSheetUtils::Canonicalize(styleSheet);
			for (const auto& resource : styleSheet.Resources)
			{
				auto item = Append(_xml, resources,
					ToUtf8(DesignerStyleSheetUtils::ValueKindName(
						resource.Value.Kind)));
				Set(item, "x:Key", resource.Key);
				item->SetInnerText(ToUtf8(resource.Value.Text));
			}
			for (const auto& rule : styleSheet.Rules)
			{
				auto style = Append(_xml, resources, "Style");
				if (rule.HasType)
					Set(style, "TargetType",
						DesignerStyleSheetUtils::UIClassName(rule.Type));
				if (!rule.Id.empty()) Set(style, "x:Key", rule.Id);
				if (!rule.Classes.empty())
					Set(style, "Classes",
						DesignerStyleSheetUtils::JoinClasses(rule.Classes));
				if (rule.RequiredStates != ControlStyleState::None)
					Set(style, "RequiredStates",
						DesignerStyleSheetUtils::FormatStates(rule.RequiredStates));
				if (rule.ExcludedStates != ControlStyleState::None)
					Set(style, "ExcludedStates",
						DesignerStyleSheetUtils::FormatStates(rule.ExcludedStates));
				for (const auto& setter : rule.Setters)
				{
					auto item = Append(_xml, style, "Setter");
					Set(item, "Property", setter.PropertyName);
					if (setter.UsesResource)
						Set(item, "Value", L"{StaticResource "
							+ setter.ResourceKey + L"}");
					else
					{
						Set(item, "Kind",
							DesignerStyleSheetUtils::ValueKindName(setter.Literal.Kind));
						Set(item, "Value", setter.Literal.Text);
					}
				}
			}
		}

		void WriteGridDefinitions(
			const DesignNode& node,
			const Element& element,
			DesignValue& residual)
		{
			if (node.Type != UIClass::UI_GridPanel || !residual.is_object()) return;
			for (const auto& [key, containerName, itemName, lengthKey,
				lengthName, minimumName, maximumName] : {
				std::tuple{ "rows", "Grid.RowDefinitions", "RowDefinition",
					"height", "Height", "MinHeight", "MaxHeight" },
				std::tuple{ "columns", "Grid.ColumnDefinitions", "ColumnDefinition",
					"width", "Width", "MinWidth", "MaxWidth" } })
			{
				if (!residual.contains(key) || !residual[key].is_array()) continue;
				auto container = Append(_xml, element, containerName);
				for (const auto& definition : residual[key].ArrayItems())
				{
					if (!definition.is_object() || !definition.contains(lengthKey))
						throw std::invalid_argument("Invalid Grid definition in XAML writer");
					auto item = Append(_xml, container, itemName);
					Set(item, lengthName, GridLengthText(definition[lengthKey]));
					if (definition.contains("min"))
						Set(item, minimumName, NumberText(definition["min"].get<double>()));
					if (definition.contains("max"))
						Set(item, maximumName, NumberText(definition["max"].get<double>()));
				}
				residual.ObjectItems().erase(key);
			}
		}

		void WriteControl(
			const DesignNode& node,
			const Element& parent,
			bool consumeSplitRegion)
		{
			if (!_written.insert(node.Id).second)
				throw std::invalid_argument("XAML control was written more than once");
			const auto elementName = node.CustomType.Empty()
				? DesignerStyleSheetUtils::UIClassName(node.Type)
				: node.CustomType.XamlPrefix + L":" + node.CustomType.XamlName;
			auto element = Append(_xml, parent, ToUtf8(elementName));
			Set(element, "x:Name", node.Name);
			element->SetAttribute("DesignId", std::to_string(node.Id));
			if (node.Locked) Set(element, "d:Locked", L"true");
			if (!node.CustomType.Empty())
			{
				Set(element, "d:CppType", node.CustomType.CppType);
				Set(element, "d:Header", node.CustomType.Header);
				Set(element, "d:BaseType",
					DesignerStyleSheetUtils::UIClassName(node.Type));
				const wchar_t* constructor = L"Bounds";
				if (node.CustomType.Constructor
					== DesignerCustomControlConstructor::Default)
					constructor = L"Default";
				else if (node.CustomType.Constructor
					== DesignerCustomControlConstructor::TextBounds)
					constructor = L"TextBounds";
				Set(element, "d:Constructor", constructor);
			}
			if (!node.CustomEvents.empty())
			{
				auto contracts = Append(_xml, element, "d:CustomEvents");
				for (const auto& contract : node.CustomEvents)
				{
					auto event = Append(_xml, contracts, "d:Event");
					Set(event, "Name", contract.Name);
					Set(event, "DisplayName", contract.DisplayName);
					event->SetAttribute("Field", contract.EventField);
					event->SetAttribute("Category",
						DesignerEventCatalog::GetCategoryName(contract.Category));
					event->SetAttribute("Signature",
						DesignerEventCatalog::GetCustomSignatureName(
							contract.Signature));
					event->SetAttribute("Order", std::to_string(contract.Order));
					event->SetAttribute("Default", BoolText(contract.IsDefault));
				}
			}

			DesignValue residualProps = node.Props.is_object()
				? node.Props : DesignValue::object();
			DesignValue residualExtra = node.Extra.is_object()
				? node.Extra : DesignValue::object();
			DesignValue residualBindings = node.Bindings.is_object()
				? node.Bindings : DesignValue::object();
			WriteControlAttributes(
				node, element, residualProps, residualBindings);
			WriteGridDefinitions(node, element, residualExtra);
			if (consumeSplitRegion && residualExtra.is_object())
				residualExtra.ObjectItems().erase("splitRegion");
			if (!residualProps.empty())
				WriteDesignValue(
					_xml, Append(_xml, element, "d:DesignProps"), residualProps);
			if (!residualBindings.empty())
				WriteDesignValue(
					_xml, Append(_xml, element, "d:DesignBindings"),
					residualBindings);

			if (node.Type == UIClass::UI_TabControl
				&& residualExtra.contains("pages"))
			{
				WriteTabPages(node, element, residualExtra);
			}
			if (!residualExtra.empty())
				WriteDesignValue(
					_xml, Append(_xml, element, "d:DesignExtra"), residualExtra);

			if (node.Type == UIClass::UI_SplitContainer)
				WriteSplitChildren(node, element);
			else
			{
				for (const auto graphIndex : _graph.ChildrenOf(node.Name))
					WriteControl(
						_document.Nodes[_graph.Nodes()[graphIndex].SourceIndex],
						element, false);
			}
		}

		void WriteControlAttributes(
			const DesignNode& node,
			const Element& element,
			DesignValue& residual,
			DesignValue& residualBindings)
		{
			std::map<std::wstring, std::wstring> attributes;
			std::set<std::wstring> projectedAttributes;
			auto propertyProbe = DesignDocumentMaterializer::CreateRuntimeControl(
				node.Type);
			if (!propertyProbe)
				throw std::invalid_argument(
					"XAML writer could not create a property metadata probe");
			const auto supportedProperties =
				DesignerPropertyCatalog::GetStyleProperties(*propertyProbe);
			auto canConsume = [&](const std::wstring& property)
			{
				return !HasBinding(node, property) && !HasMetadata(node, property)
					&& DesignerPropertyCatalog::Find(
						supportedProperties, property) != nullptr;
			};
			auto consumeString = [&](const char* key, const wchar_t* attribute,
				const wchar_t* property)
			{
				if (!residual.contains(key) || !residual[key].is_string()
					|| !canConsume(property)) return;
				attributes[attribute] = FromUtf8(residual[key].get<std::string>());
				projectedAttributes.insert(attribute);
			};
			auto consumeBool = [&](const char* key, const wchar_t* attribute,
				const wchar_t* property)
			{
				if (!residual.contains(key) || !residual[key].is_boolean()
					|| !canConsume(property)) return;
				attributes[attribute] = residual[key].get<bool>() ? L"true" : L"false";
				projectedAttributes.insert(attribute);
			};
			auto consumeNumber = [&](const char* key, const wchar_t* attribute,
				const wchar_t* property)
			{
				if (!residual.contains(key) || !residual[key].is_number()
					|| !canConsume(property)) return;
				attributes[attribute] = FromUtf8(residual[key].ToString());
				projectedAttributes.insert(attribute);
			};

			consumeString("text", L"Text", L"Text");
			consumeBool("enable", L"Enable", L"Enable");
			consumeBool("visible", L"Visible", L"Visible");
			consumeBool("showValidationBorder", L"ShowValidationBorder", L"ShowValidationBorder");
			consumeBool("showValidationToolTip", L"ShowValidationToolTip", L"ShowValidationToolTip");
			consumeNumber("validationBorderThickness", L"ValidationBorderThickness", L"ValidationBorderThickness");
			consumeNumber("validationCornerRadius", L"ValidationCornerRadius", L"ValidationCornerRadius");
			consumeNumber("validationToolTipMaxWidth", L"ValidationToolTipMaxWidth", L"ValidationToolTipMaxWidth");
			consumeString("accessibleDescription", L"AccessibleDescription", L"AccessibleDescription");
			consumeNumber("zIndex", L"ZIndex", L"ZIndex");
			consumeNumber("gridRow", L"Grid.Row", L"GridRow");
			consumeNumber("gridColumn", L"Grid.Column", L"GridColumn");
			consumeNumber("gridRowSpan", L"Grid.RowSpan", L"GridRowSpan");
			consumeNumber("gridColumnSpan", L"Grid.ColumnSpan", L"GridColumnSpan");
			consumeNumber("sizeMode", L"SizeMode", L"SizeMode");
			consumeString("hAlign", L"HorizontalAlignment", L"HAlign");
			consumeString("vAlign", L"VerticalAlignment", L"VAlign");
			consumeString("dock", L"DockPanel.Dock", L"DockPosition");

			if (residual.contains("location") && residual["location"].is_object()
				&& canConsume(L"Left") && canConsume(L"Top"))
			{
				attributes[L"Canvas.Left"] = std::to_wstring(
					residual["location"].value("x", 0));
				attributes[L"Canvas.Top"] = std::to_wstring(
					residual["location"].value("y", 0));
				projectedAttributes.insert(L"Canvas.Left");
				projectedAttributes.insert(L"Canvas.Top");
			}
			if (residual.contains("size") && residual["size"].is_object()
				&& canConsume(L"Width") && canConsume(L"Height")
				&& canConsume(L"LayoutWidth") && canConsume(L"LayoutHeight"))
			{
				attributes[L"Width"] = std::to_wstring(
					residual["size"].value("w", 0));
				attributes[L"Height"] = std::to_wstring(
					residual["size"].value("h", 0));
				projectedAttributes.insert(L"Width");
				projectedAttributes.insert(L"Height");
			}
			for (const auto& [key, attribute, property] : {
				std::tuple{ "backColor", L"BackColor", L"BackColor" },
				std::tuple{ "foreColor", L"ForeColor", L"ForeColor" },
				std::tuple{ "borderColor", L"BorderColor", L"BorderColor" },
				std::tuple{ "bolderColor", L"BorderColor", L"BorderColor" } })
			{
				if (!residual.contains(key) || !canConsume(property)) continue;
				if (const auto text = ColorText(residual[key]))
				{
					attributes[attribute] = *text;
					projectedAttributes.insert(attribute);
				}
			}
			for (const auto& [key, attribute, property] : {
				std::tuple{ "margin", L"Margin", L"Margin" },
				std::tuple{ "padding", L"Padding", L"Padding" } })
			{
				if (!residual.contains(key) || !canConsume(property)) continue;
				if (const auto text = ThicknessText(residual[key]))
				{
					attributes[attribute] = *text;
					projectedAttributes.insert(attribute);
				}
			}
			if (residual.contains("anchor") && residual["anchor"].is_number())
			{
				attributes[L"Anchor"] = AnchorText(residual["anchor"].get<int>());
				residual.ObjectItems().erase("anchor");
			}
			if (residual.contains("font") && residual["font"].is_object())
			{
				const auto& font = residual["font"];
				if (font.contains("name") && font["name"].is_string())
					attributes[L"FontName"] = FromUtf8(font["name"].get<std::string>());
				if (font.contains("size") && font["size"].is_number())
					attributes[L"FontSize"] = FromUtf8(font["size"].ToString());
				residual.ObjectItems().erase("font");
			}
			if (residual.contains("styleId") && residual["styleId"].is_string())
			{
				attributes[L"Style"] = L"{StaticResource "
					+ FromUtf8(residual["styleId"].get<std::string>()) + L"}";
				residual.ObjectItems().erase("styleId");
			}
			if (residual.contains("styleClasses") && residual["styleClasses"].is_array())
			{
				std::vector<std::wstring> classes;
				for (const auto& item : residual["styleClasses"].ArrayItems())
					if (item.is_string()) classes.push_back(FromUtf8(item.get<std::string>()));
				attributes[L"Classes"] = DesignerStyleSheetUtils::JoinClasses(classes);
				residual.ObjectItems().erase("styleClasses");
			}

			if (residual.contains("metadata") && residual["metadata"].is_object())
			{
				DesignValue pending = DesignValue::object();
				for (const auto& [property, stored] : residual["metadata"].ObjectItems())
				{
					const auto propertyName = FromUtf8(property);
					if (HasBinding(node, propertyName)
						|| !DesignerPropertyCatalog::Find(
							supportedProperties, propertyName))
					{
						pending[property] = stored;
						continue;
					}
					if (!stored.is_object() || !stored.contains("value")
						|| !stored["value"].is_string())
						throw std::invalid_argument("Invalid metadata property in XAML writer");
					attributes[propertyName] = FromUtf8(
						stored["value"].get<std::string>());
				}
				if (pending.empty()) residual.ObjectItems().erase("metadata");
				else residual["metadata"] = std::move(pending);
			}

			if (node.Events.is_object())
			{
				for (const auto& [event, handlerValue] : node.Events.ObjectItems())
				{
					if (!handlerValue.is_string() && !handlerValue.is_boolean()) continue;
					const auto handler = handlerValue.is_boolean()
						? (handlerValue.get<bool>() ? std::wstring(L"Auto") : std::wstring{})
						: FromUtf8(handlerValue.get<std::string>());
					if (!handler.empty()) attributes[FromUtf8(event)] =
						DesignerEventCatalog::IsLegacyEnabledValue(handler)
							? std::wstring(L"Auto") : handler;
				}
			}
			if (residualBindings.is_object())
			{
				std::vector<std::string> consumedBindings;
				for (const auto& [property, value] : residualBindings.ObjectItems())
				{
					const auto propertyName = FromUtf8(property);
					if (!DesignerPropertyCatalog::Find(
						supportedProperties, propertyName)) continue;
					attributes[propertyName] = BindingMarkup(value);
					consumedBindings.push_back(property);
				}
				for (const auto& property : consumedBindings)
					residualBindings.ObjectItems().erase(property);
			}

			if (!projectedAttributes.empty())
			{
				std::wstring projectionList;
				for (const auto& name : projectedAttributes)
				{
					if (!projectionList.empty()) projectionList += L", ";
					projectionList += name;
				}
				Set(element, "d:ProjectedProperties", projectionList);
			}

			for (const auto& [name, value] : attributes)
				Set(element, ToUtf8(name).c_str(), value);
		}

		void WriteTabPages(
			const DesignNode& node,
			const Element& element,
			DesignValue& residualExtra)
		{
			const auto pages = residualExtra["pages"];
			if (!pages.is_array())
				throw std::invalid_argument("TabControl pages must be an array");
			for (size_t index = 0; index < pages.size(); ++index)
			{
				const auto& page = pages[index];
				if (!page.is_object())
					throw std::invalid_argument("TabPage descriptor must be an object");
				for (const auto& [key, ignored] : page.ObjectItems())
				{
					(void)ignored;
					if (key != "id" && key != "text")
						throw std::invalid_argument("TabPage contains unsupported persisted fields");
				}
				const auto generatedKey = node.Name + L"#page" + std::to_wstring(index);
				const auto key = FromUtf8(page.value("id", ToUtf8(generatedKey)));
				if (!key.starts_with(node.Name + L"#page"))
					throw std::invalid_argument("TabPage design key is outside its TabControl");
				auto pageElement = Append(_xml, element, "TabPage");
				Set(pageElement, "Header", FromUtf8(page.value("text", std::string("Page"))));
				if (key != generatedKey) Set(pageElement, "d:DesignKey", key);
				for (const auto graphIndex : _graph.ChildrenOf(key))
					WriteControl(
						_document.Nodes[_graph.Nodes()[graphIndex].SourceIndex],
						pageElement, false);
			}
			residualExtra.ObjectItems().erase("pages");
		}

		void WriteSplitChildren(
			const DesignNode& node,
			const Element& element)
		{
			std::vector<const DesignNode*> first;
			std::vector<const DesignNode*> second;
			for (const auto graphIndex : _graph.ChildrenOf(node.Name))
			{
				const auto& child = _document.Nodes[
					_graph.Nodes()[graphIndex].SourceIndex];
				const auto region = child.Extra.is_object()
					? child.Extra.value("splitRegion", std::string("panel1"))
					: std::string("panel1");
				(region == "panel2" ? second : first).push_back(&child);
			}
			auto writeRegion = [&](const char* name, const auto& controls)
			{
				if (controls.empty()) return;
				auto region = Append(_xml, element, name);
				for (const auto* child : controls)
					WriteControl(*child, region, true);
			};
			writeRegion("SplitContainer.FirstPanel", first);
			writeRegion("SplitContainer.SecondPanel", second);
		}
	};
}

std::string XamlDocumentSerializer::ToXaml(const DesignDocument& document)
{
	XmlDocument xml;
	xml.AppendChild(xml.CreateXmlDeclaration("1.0", "utf-8", ""));
	Writer writer(document, xml);
	xml.AppendChild(writer.Write());
	XmlWriterSettings settings;
	settings.Indent = true;
	settings.Encoding = "utf-8";
	return xml.ToString(settings);
}

bool XamlDocumentSerializer::SaveToFile(
	const DesignDocument& document,
	const std::wstring& filePath,
	std::wstring* outError)
{
	try
	{
		return AtomicFile::Write(filePath, ToXaml(document), outError);
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"XAML 保存失败：" + FromUtf8(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"XAML 保存失败：发生未知异常。";
		return false;
	}
}
}
