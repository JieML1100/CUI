#include "DesignDocumentSerializer.h"
#include "AtomicFile.h"
#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "../../XmlLite/include/Xml.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include <algorithm>
#include <cctype>
#include <Convert.h>
#include <type_traits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace DesignerModel
{
using namespace System::Xml;

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
		switch (t)
		{
		case UIClass::UI_Label: return "Label";
		case UIClass::UI_LinkLabel: return "LinkLabel";
		case UIClass::UI_Button: return "Button";
		case UIClass::UI_TextBox: return "TextBox";
		case UIClass::UI_RichTextBox: return "RichTextBox";
		case UIClass::UI_PasswordBox: return "PasswordBox";
		case UIClass::UI_DateTimePicker: return "DateTimePicker";
		case UIClass::UI_NumericUpDown: return "NumericUpDown";
		case UIClass::UI_Panel: return "Panel";
		case UIClass::UI_GroupBox: return "GroupBox";
		case UIClass::UI_Expander: return "Expander";
		case UIClass::UI_ScrollView: return "ScrollView";
		case UIClass::UI_StackPanel: return "StackPanel";
		case UIClass::UI_GridPanel: return "GridPanel";
		case UIClass::UI_DockPanel: return "DockPanel";
		case UIClass::UI_WrapPanel: return "WrapPanel";
		case UIClass::UI_RelativePanel: return "RelativePanel";
		case UIClass::UI_SplitContainer: return "SplitContainer";
		case UIClass::UI_CheckBox: return "CheckBox";
		case UIClass::UI_RadioBox: return "RadioBox";
		case UIClass::UI_ComboBox: return "ComboBox";
		case UIClass::UI_ListView: return "ListView";
		case UIClass::UI_ListBox: return "ListBox";
		case UIClass::UI_GridView: return "GridView";
		case UIClass::UI_PropertyGrid: return "PropertyGrid";
		case UIClass::UI_ChartView: return "ChartView";
		case UIClass::UI_ReportView: return "ReportView";
		case UIClass::UI_KpiCard: return "KpiCard";
		case UIClass::UI_FilterBar: return "FilterBar";
		case UIClass::UI_TreeView: return "TreeView";
		case UIClass::UI_ProgressBar: return "ProgressBar";
		case UIClass::UI_LoadingRing: return "LoadingRing";
		case UIClass::UI_ProgressRing: return "ProgressRing";
		case UIClass::UI_Slider: return "Slider";
		case UIClass::UI_PictureBox: return "PictureBox";
		case UIClass::UI_Switch: return "Switch";
		case UIClass::UI_TabControl: return "TabControl";
		case UIClass::UI_ToolBar: return "ToolBar";
		case UIClass::UI_Menu: return "Menu";
		case UIClass::UI_StatusBar: return "StatusBar";
		case UIClass::UI_ToastHost: return "ToastHost";
		case UIClass::UI_WebBrowser: return "WebBrowser";
		case UIClass::UI_MediaPlayer: return "MediaPlayer";
		case UIClass::UI_TabPage: return "TabPage";
		default: return "Base";
		}
	}

	static bool TryParseUIClass(const std::string& s, UIClass& out)
	{
		if (s == "Base" || s == "Control") { out = UIClass::UI_Base; return true; }
		if (s == "Label") { out = UIClass::UI_Label; return true; }
		if (s == "LinkLabel") { out = UIClass::UI_LinkLabel; return true; }
		if (s == "Button") { out = UIClass::UI_Button; return true; }
		if (s == "TextBox") { out = UIClass::UI_TextBox; return true; }
		if (s == "RichTextBox") { out = UIClass::UI_RichTextBox; return true; }
		if (s == "PasswordBox") { out = UIClass::UI_PasswordBox; return true; }
		if (s == "DateTimePicker") { out = UIClass::UI_DateTimePicker; return true; }
		if (s == "NumericUpDown") { out = UIClass::UI_NumericUpDown; return true; }
		if (s == "Panel") { out = UIClass::UI_Panel; return true; }
		if (s == "GroupBox") { out = UIClass::UI_GroupBox; return true; }
		if (s == "Expander") { out = UIClass::UI_Expander; return true; }
		if (s == "ScrollView") { out = UIClass::UI_ScrollView; return true; }
		if (s == "StackPanel") { out = UIClass::UI_StackPanel; return true; }
		if (s == "GridPanel") { out = UIClass::UI_GridPanel; return true; }
		if (s == "DockPanel") { out = UIClass::UI_DockPanel; return true; }
		if (s == "WrapPanel") { out = UIClass::UI_WrapPanel; return true; }
		if (s == "RelativePanel") { out = UIClass::UI_RelativePanel; return true; }
		if (s == "SplitContainer") { out = UIClass::UI_SplitContainer; return true; }
		if (s == "CheckBox") { out = UIClass::UI_CheckBox; return true; }
		if (s == "RadioBox") { out = UIClass::UI_RadioBox; return true; }
		if (s == "ComboBox") { out = UIClass::UI_ComboBox; return true; }
		if (s == "ListView") { out = UIClass::UI_ListView; return true; }
		if (s == "ListBox") { out = UIClass::UI_ListBox; return true; }
		if (s == "GridView") { out = UIClass::UI_GridView; return true; }
		if (s == "PropertyGrid") { out = UIClass::UI_PropertyGrid; return true; }
		if (s == "ChartView") { out = UIClass::UI_ChartView; return true; }
		if (s == "ReportView") { out = UIClass::UI_ReportView; return true; }
		if (s == "KpiCard") { out = UIClass::UI_KpiCard; return true; }
		if (s == "FilterBar") { out = UIClass::UI_FilterBar; return true; }
		if (s == "TreeView") { out = UIClass::UI_TreeView; return true; }
		if (s == "ProgressBar") { out = UIClass::UI_ProgressBar; return true; }
		if (s == "LoadingRing") { out = UIClass::UI_LoadingRing; return true; }
		if (s == "ProgressRing") { out = UIClass::UI_ProgressRing; return true; }
		if (s == "Slider") { out = UIClass::UI_Slider; return true; }
		if (s == "PictureBox") { out = UIClass::UI_PictureBox; return true; }
		if (s == "Switch") { out = UIClass::UI_Switch; return true; }
		if (s == "TabControl") { out = UIClass::UI_TabControl; return true; }
		if (s == "ToolBar") { out = UIClass::UI_ToolBar; return true; }
		if (s == "Menu") { out = UIClass::UI_Menu; return true; }
		if (s == "StatusBar") { out = UIClass::UI_StatusBar; return true; }
		if (s == "ToastHost") { out = UIClass::UI_ToastHost; return true; }
		if (s == "WebBrowser") { out = UIClass::UI_WebBrowser; return true; }
		if (s == "MediaPlayer") { out = UIClass::UI_MediaPlayer; return true; }
		if (s == "TabPage") { out = UIClass::UI_TabPage; return true; }
		return false;
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
			element->SetInnerText(std::to_string(value.get<double>()));
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

		return FromXml(content, document, outError);
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

std::string DesignDocumentSerializer::ToXml(const DesignDocument& document)
{
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
	for (const auto& resolved : graph.Nodes())
	{
		const auto& node = document.Nodes[resolved.SourceIndex];
		if (node.ParentRef != resolved.ParentKey)
			throw std::invalid_argument(
				"Design document parentId and parent name disagree");
	}
	root->SetAttribute("nextId", std::to_string(document.NextStableId));
	xml.AppendChild(root);

	auto form = AppendElement(xml, root, "form");
	form->SetAttribute("name", ToUtf8(document.Form.Name));
	form->SetAttribute("text", ToUtf8(document.Form.Text));
	form->SetAttribute("fontName", ToUtf8(document.Form.FontName));
	form->SetAttribute("fontSize", std::to_string(document.Form.FontSize));
	form->SetAttribute("showInTaskBar", BoolToString(document.Form.ShowInTaskBar));
	form->SetAttribute("topMost", BoolToString(document.Form.TopMost));
	form->SetAttribute("enable", BoolToString(document.Form.Enable));
	form->SetAttribute("visible", BoolToString(document.Form.Visible));
	form->SetAttribute("visibleHead", BoolToString(document.Form.VisibleHead));
	form->SetAttribute("headHeight", std::to_string(document.Form.HeadHeight));
	form->SetAttribute("minBox", BoolToString(document.Form.MinBox));
	form->SetAttribute("maxBox", BoolToString(document.Form.MaxBox));
	form->SetAttribute("closeBox", BoolToString(document.Form.CloseBox));
	form->SetAttribute("centerTitle", BoolToString(document.Form.CenterTitle));
	form->SetAttribute("allowResize", BoolToString(document.Form.AllowResize));

	auto size = AppendElement(xml, form, "size");
	size->SetAttribute("width", std::to_string(document.Form.Size.cx));
	size->SetAttribute("height", std::to_string(document.Form.Size.cy));

	auto location = AppendElement(xml, form, "location");
	location->SetAttribute("x", std::to_string(document.Form.Location.x));
	location->SetAttribute("y", std::to_string(document.Form.Location.y));

	SetColorAttributes(AppendElement(xml, form, "backColor"), document.Form.BackColor);
	SetColorAttributes(AppendElement(xml, form, "foreColor"), document.Form.ForeColor);

	if (!document.Form.EventHandlers.empty())
	{
		auto events = AppendElement(xml, form, "events");
		for (const auto& kv : document.Form.EventHandlers)
		{
			if (kv.first.empty() || kv.second.empty()) continue;
			auto event = AppendElement(xml, events, "event");
			event->SetAttribute("name", ToUtf8(kv.first));
			event->SetAttribute("handler", ToUtf8(kv.second));
		}
	}

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
			item->SetAttribute("read", BoolToString(property.CanRead));
			item->SetAttribute("write", BoolToString(property.CanWrite));
			item->SetAttribute("observe", BoolToString(property.CanObserve));
		}
	}

	if (!document.StyleSheet.Empty())
	{
		auto styleSheet = document.StyleSheet;
		DesignerStyleSheetUtils::Canonicalize(styleSheet);
		auto styleSheetElement = AppendElement(xml, root, "styleSheet");
		if (!styleSheet.Resources.empty())
		{
			auto resources = AppendElement(xml, styleSheetElement, "resources");
			for (const auto& resource : styleSheet.Resources)
			{
				auto item = AppendElement(xml, resources, "resource");
				item->SetAttribute("key", ToUtf8(resource.Key));
				item->SetAttribute("kind", ToUtf8(
					DesignerStyleSheetUtils::ValueKindName(resource.Value.Kind)));
				item->SetInnerText(ToUtf8(resource.Value.Text));
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
				if (!rule.Id.empty()) item->SetAttribute("id", ToUtf8(rule.Id));
				if (rule.RequiredStates != ControlStyleState::None)
					item->SetAttribute("requiredStates", ToUtf8(
						DesignerStyleSheetUtils::FormatStates(rule.RequiredStates)));
				if (rule.ExcludedStates != ControlStyleState::None)
					item->SetAttribute("excludedStates", ToUtf8(
						DesignerStyleSheetUtils::FormatStates(rule.ExcludedStates)));
				for (const auto& styleClass : rule.Classes)
				{
					auto classElement = AppendElement(xml, item, "class");
					classElement->SetAttribute("name", ToUtf8(styleClass));
				}
				for (const auto& setter : rule.Setters)
				{
					auto setterElement = AppendElement(xml, item, "setter");
					setterElement->SetAttribute("property", ToUtf8(setter.PropertyName));
					if (setter.UsesResource)
						setterElement->SetAttribute("resource", ToUtf8(setter.ResourceKey));
					else
					{
						setterElement->SetAttribute("kind", ToUtf8(
							DesignerStyleSheetUtils::ValueKindName(setter.Literal.Kind)));
						setterElement->SetInnerText(ToUtf8(setter.Literal.Text));
					}
				}
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
		if (!node.CustomType.Empty())
		{
			control->SetAttribute("customPrefix", ToUtf8(node.CustomType.XamlPrefix));
			control->SetAttribute("customName", ToUtf8(node.CustomType.XamlName));
			control->SetAttribute("customNamespace", ToUtf8(node.CustomType.XamlNamespace));
			control->SetAttribute("customCppType", ToUtf8(node.CustomType.CppType));
			control->SetAttribute("customHeader", ToUtf8(node.CustomType.Header));
			const char* constructor = "Bounds";
			if (node.CustomType.Constructor
				== DesignerCustomControlConstructor::Default)
				constructor = "Default";
			else if (node.CustomType.Constructor
				== DesignerCustomControlConstructor::TextBounds)
				constructor = "TextBounds";
			control->SetAttribute("customConstructor", constructor);
		}
		if (!node.CustomEvents.empty())
		{
			auto customEvents = AppendElement(xml, control, "customEvents");
			for (const auto& contract : node.CustomEvents)
			{
				auto event = AppendElement(xml, customEvents, "event");
				event->SetAttribute("name", ToUtf8(contract.Name));
				event->SetAttribute("displayName", ToUtf8(contract.DisplayName));
				event->SetAttribute("field", contract.EventField);
				event->SetAttribute("category",
					DesignerEventCatalog::GetCategoryName(contract.Category));
				event->SetAttribute("signature",
					DesignerEventCatalog::GetCustomSignatureName(
						contract.Signature));
				event->SetAttribute("order", std::to_string(contract.Order));
				event->SetAttribute("default",
					contract.IsDefault ? "true" : "false");
			}
		}
		control->SetAttribute("order", std::to_string(node.Order));
		if (node.ParentId > 0)
		{
			control->SetAttribute("parentId", std::to_string(node.ParentId));
		}
		if (!node.ParentRef.empty())
		{
			control->SetAttribute("parent", ToUtf8(node.ParentRef));
		}

		WriteValue(xml, AppendElement(xml, control, "props"), node.Props.is_object() ? node.Props : DesignValue::object());
		if (!node.Extra.is_null())
		{
			WriteValue(xml, AppendElement(xml, control, "extra"), node.Extra);
		}
		if (!node.Events.is_null())
		{
			WriteValue(xml, AppendElement(xml, control, "events"), node.Events);
		}
		if (!node.Bindings.is_null())
		{
			WriteValue(xml, AppendElement(xml, control, "bindings"), node.Bindings);
		}
	}

	XmlWriterSettings settings;
	settings.Indent = true;
	settings.Encoding = "utf-8";
	return xml.ToString(settings);
}

bool DesignDocumentSerializer::FromXml(const std::string& xmlText, DesignDocument& document, std::wstring* outError)
{
	XmlDocument xml;
	xml.LoadXml(xmlText);

	auto root = xml.DocumentElement();
	if (!root || root->Name() != "designDocument")
	{
		if (outError) *outError = L"Invalid CUI Designer XML file: missing root element.";
		return false;
	}

	if (root->GetAttribute("schema") != "cui.designer")
	{
		if (outError) *outError = L"Invalid CUI Designer file: schema mismatch.";
		return false;
	}

	int version = 0;
	if (!TryReadIntegralAttribute(root, "version", version)
		|| version < 1
		|| version > DesignDocument::CurrentSchemaVersion)
	{
		if (outError) *outError = L"Unsupported design file version.";
		return false;
	}
	int persistedNextId = 1;
	if (version >= 4
		&& (!TryReadIntegralAttribute(root, "nextId", persistedNextId)
			|| persistedNextId < 1))
	{
		if (outError) *outError = L"Design file v4+ is missing a valid nextId.";
		return false;
	}

	auto controls = FindChildElement(root, "controls");
	if (!controls)
	{
		if (outError) *outError = L"Design file is missing the controls element.";
		return false;
	}

	document.Clear();
	document.Schema = "cui.designer";
	document.SchemaVersion = DesignDocument::CurrentSchemaVersion;
	if (version >= 4) document.NextStableId = persistedNextId;
	if (version >= 5)
	{
		if (auto codeBehind = FindChildElement(root, "codeBehind"))
		{
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

	if (auto form = FindChildElement(root, "form"))
	{
		document.Form.Name = FromUtf8(form->GetAttribute("name"));
		if (document.Form.Name.empty()) document.Form.Name = L"MainForm";
		document.Form.Text = FromUtf8(form->GetAttribute("text"));
		document.Form.FontName = FromUtf8(form->GetAttribute("fontName"));
		TryReadFloatAttribute(form, "fontSize", document.Form.FontSize);
		if (document.Form.FontSize < 1.0f) document.Form.FontSize = 1.0f;
		if (document.Form.FontSize > 200.0f) document.Form.FontSize = 200.0f;
		TryReadBoolAttribute(form, "showInTaskBar", document.Form.ShowInTaskBar);
		TryReadBoolAttribute(form, "topMost", document.Form.TopMost);
		TryReadBoolAttribute(form, "enable", document.Form.Enable);
		TryReadBoolAttribute(form, "visible", document.Form.Visible);
		TryReadBoolAttribute(form, "visibleHead", document.Form.VisibleHead);
		TryReadIntegralAttribute(form, "headHeight", document.Form.HeadHeight);
		if (document.Form.HeadHeight < 0) document.Form.HeadHeight = 0;
		TryReadBoolAttribute(form, "minBox", document.Form.MinBox);
		TryReadBoolAttribute(form, "maxBox", document.Form.MaxBox);
		TryReadBoolAttribute(form, "closeBox", document.Form.CloseBox);
		TryReadBoolAttribute(form, "centerTitle", document.Form.CenterTitle);
		TryReadBoolAttribute(form, "allowResize", document.Form.AllowResize);

		if (auto size = FindChildElement(form, "size"))
		{
			TryReadIntegralAttribute(size, "width", document.Form.Size.cx);
			TryReadIntegralAttribute(size, "height", document.Form.Size.cy);
		}
		if (auto location = FindChildElement(form, "location"))
		{
			TryReadIntegralAttribute(location, "x", document.Form.Location.x);
			TryReadIntegralAttribute(location, "y", document.Form.Location.y);
		}

		document.Form.BackColor = ColorFromXmlElement(FindChildElement(form, "backColor"), document.Form.BackColor);
		document.Form.ForeColor = ColorFromXmlElement(FindChildElement(form, "foreColor"), document.Form.ForeColor);

		if (auto events = FindChildElement(form, "events"))
		{
			for (const auto& event : FindChildElements(events, "event"))
			{
				std::wstring name = FromUtf8(event->GetAttribute("name"));
				if (name.empty()) continue;
				std::wstring handler = FromUtf8(event->GetAttribute("handler"));
				document.Form.EventHandlers[name] = handler.empty() ? L"1" : handler;
			}
		}
	}

	if (version >= 2)
	{
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
	}

	if (version >= 3)
	{
		if (auto styleSheet = FindChildElement(root, "styleSheet"))
		{
			if (auto resources = FindChildElement(styleSheet, "resources"))
			{
				for (const auto& item : FindChildElements(resources, "resource"))
				{
					DesignerStyleResource resource;
					resource.Key = FromUtf8(item->GetAttribute("key"));
					if (!DesignerStyleSheetUtils::TryParseValueKind(
						FromUtf8(item->GetAttribute("kind")), resource.Value.Kind))
					{
						if (outError) *outError = L"样式资源包含无效的值类型。";
						return false;
					}
					resource.Value.Text = FromUtf8(item->InnerText());
					document.StyleSheet.Resources.push_back(std::move(resource));
				}
			}
			if (auto rules = FindChildElement(styleSheet, "rules"))
			{
				for (const auto& item : FindChildElements(rules, "rule"))
				{
					DesignerStyleRule rule;
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
					rule.Id = FromUtf8(item->GetAttribute("id"));
					if (!DesignerStyleSheetUtils::TryParseStates(
						FromUtf8(item->GetAttribute("requiredStates")), rule.RequiredStates)
						|| !DesignerStyleSheetUtils::TryParseStates(
							FromUtf8(item->GetAttribute("excludedStates")), rule.ExcludedStates))
					{
						if (outError) *outError = L"样式规则包含无效的状态名称。";
						return false;
					}
					for (const auto& classElement : FindChildElements(item, "class"))
						rule.Classes.push_back(FromUtf8(classElement->GetAttribute("name")));
					for (const auto& setterElement : FindChildElements(item, "setter"))
					{
						DesignerStyleSetter setter;
						setter.PropertyName = FromUtf8(setterElement->GetAttribute("property"));
						setter.ResourceKey = FromUtf8(setterElement->GetAttribute("resource"));
						setter.UsesResource = !setter.ResourceKey.empty();
						if (!setter.UsesResource)
						{
							if (!DesignerStyleSheetUtils::TryParseValueKind(
								FromUtf8(setterElement->GetAttribute("kind")), setter.Literal.Kind))
							{
								if (outError) *outError = L"样式 Setter 包含无效的值类型。";
								return false;
							}
							setter.Literal.Text = FromUtf8(setterElement->InnerText());
						}
						rule.Setters.push_back(std::move(setter));
					}
					document.StyleSheet.Rules.push_back(std::move(rule));
				}
			}
			DesignerStyleSheetUtils::Canonicalize(document.StyleSheet);
			if (!DesignerStyleSheetUtils::Validate(document.StyleSheet, outError))
				return false;
		}
	}

	std::unordered_set<std::wstring> nameSet;
	std::unordered_set<int> idSet;
	for (const auto& control : FindChildElements(controls, "control"))
	{
		DesignNode node;
		node.Name = FromUtf8(control->GetAttribute("name"));
		std::string type = control->GetAttribute("type");
		if (node.Name.empty() || !TryParseUIClass(type, node.Type))
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

		if (version >= 4)
		{
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
		}
		else
		{
			node.Id = document.AllocateNodeId();
		}
		node.ParentRef = FromUtf8(control->GetAttribute("parent"));
		if (control->HasAttribute("customName")
			|| control->HasAttribute("customCppType"))
		{
			node.CustomType.XamlPrefix = FromUtf8(
				control->GetAttribute("customPrefix"));
			node.CustomType.XamlName = FromUtf8(
				control->GetAttribute("customName"));
			node.CustomType.XamlNamespace = FromUtf8(
				control->GetAttribute("customNamespace"));
			node.CustomType.CppType = FromUtf8(
				control->GetAttribute("customCppType"));
			node.CustomType.Header = FromUtf8(
				control->GetAttribute("customHeader"));
			const auto constructor = control->GetAttribute("customConstructor");
			if (constructor == "Default")
				node.CustomType.Constructor =
					DesignerCustomControlConstructor::Default;
			else if (constructor == "TextBounds")
				node.CustomType.Constructor =
					DesignerCustomControlConstructor::TextBounds;
			else if (constructor.empty() || constructor == "Bounds")
				node.CustomType.Constructor =
					DesignerCustomControlConstructor::Bounds;
			else
			{
				if (outError) *outError = L"Control entry has an invalid custom constructor: "
					+ node.Name;
				return false;
			}
			if (node.CustomType.XamlPrefix.empty()
				|| node.CustomType.XamlName.empty()
				|| node.CustomType.XamlNamespace.empty()
				|| node.CustomType.CppType.empty()
				|| node.CustomType.Header.empty())
			{
				if (outError) *outError = L"Control entry has an incomplete custom type: "
					+ node.Name;
				return false;
			}
		}
		if (auto customEvents = FindChildElement(control, "customEvents"))
		{
			const auto customEventsText = customEvents->InnerText();
			if (FindChildElements(control, "customEvents").size() != 1
				|| !customEvents->Attributes().empty()
				|| std::any_of(customEventsText.begin(),
					customEventsText.end(),
					[](unsigned char ch) { return !std::isspace(ch); }))
			{
				if (outError) *outError = L"Control entry has an invalid customEvents container: "
					+ node.Name;
				return false;
			}
			for (const auto& child : customEvents->ChildNodes())
			{
				if (child && child->NodeType() == XmlNodeType::Element
					&& child->Name() != "event")
				{
					if (outError) *outError = L"Control customEvents contains an unsupported element: "
						+ node.Name;
					return false;
				}
			}
			for (const auto& event : FindChildElements(customEvents, "event"))
			{
				for (const auto& attribute : event->Attributes())
				{
					if (!attribute) continue;
					const auto& name = attribute->Name();
					if (name != "name" && name != "displayName"
						&& name != "field" && name != "category"
						&& name != "signature" && name != "order"
						&& name != "default")
					{
						if (outError) *outError = L"Control custom event contains an unsupported attribute: "
							+ node.Name;
						return false;
					}
				}
				bool hasElementContent = false;
				for (const auto& child : event->ChildNodes())
					if (child && child->NodeType() == XmlNodeType::Element)
						hasElementContent = true;
				const auto eventText = event->InnerText();
				if (hasElementContent
					|| std::any_of(eventText.begin(),
						eventText.end(),
						[](unsigned char ch) { return !std::isspace(ch); }))
				{
					if (outError) *outError = L"Control custom event cannot contain child elements: "
						+ node.Name;
					return false;
				}
				DesignerCustomEventDescriptor contract;
				contract.Name = FromUtf8(event->GetAttribute("name"));
				contract.DisplayName = FromUtf8(
					event->GetAttribute("displayName"));
				contract.EventField = event->GetAttribute("field");
				DesignerEventCategory category{};
				DesignerCustomEventSignature signature{};
				if (contract.Name.empty() || contract.EventField.empty()
					|| !DesignerEventCatalog::TryParseCategory(
						FromUtf8(event->GetAttribute("category")), category)
					|| !DesignerEventCatalog::TryParseCustomSignature(
						FromUtf8(event->GetAttribute("signature")), signature)
					|| !TryReadIntegralAttribute(event, "order", contract.Order)
					|| !TryReadBoolAttribute(event, "default", contract.IsDefault))
				{
					if (outError) *outError = L"Control entry has an invalid custom event: "
						+ node.Name;
					return false;
				}
				contract.Category = category;
				contract.Signature = signature;
				if (contract.DisplayName.empty()) contract.DisplayName = contract.Name;
				node.CustomEvents.push_back(std::move(contract));
			}
			std::wstring validationError;
			if (!DesignerEventCatalog::ValidateCustomEvents(
				node.Type, node.CustomEvents, &validationError))
			{
				if (outError) *outError = L"Control entry has invalid custom events: "
					+ node.Name + L"：" + validationError;
				return false;
			}
		}
		if (!TryReadIntegralAttribute(control, "order", node.Order))
		{
			node.Order = -1;
		}

		auto props = FindChildElement(control, "props");
		if (props)
		{
			if (!ReadValue(props, node.Props, outError)) return false;
		}
		else
		{
			node.Props = DesignValue::object();
		}

		auto extra = FindChildElement(control, "extra");
		if (extra)
		{
			if (!ReadValue(extra, node.Extra, outError)) return false;
		}
		else
		{
			node.Extra = DesignValue::object();
		}

		auto events = FindChildElement(control, "events");
		if (events)
		{
			if (!ReadValue(events, node.Events, outError)) return false;
		}
		else
		{
			node.Events = DesignValue::object();
		}

		auto bindings = FindChildElement(control, "bindings");
		if (bindings)
		{
			if (!ReadValue(bindings, node.Bindings, outError)) return false;
		}
		else
		{
			node.Bindings = DesignValue::object();
		}

		document.Nodes.push_back(std::move(node));
	}

	std::unordered_map<int, DesignNode*> nodeById;
	std::unordered_map<std::wstring, int> idByName;
	nodeById.reserve(document.Nodes.size());
	idByName.reserve(document.Nodes.size());
	for (auto& node : document.Nodes)
	{
		nodeById.emplace(node.Id, &node);
		idByName.emplace(node.Name, node.Id);
	}

	for (auto& node : document.Nodes)
	{
		if (version < 4)
		{
			const auto legacyParent = idByName.find(node.ParentRef);
			if (legacyParent != idByName.end())
				node.ParentId = legacyParent->second;
			continue;
		}

		if (node.ParentId > 0)
		{
			const auto parent = nodeById.find(node.ParentId);
			// ID is authoritative; keep the name reference canonical and human-readable.
			if (parent != nodeById.end())
				node.ParentRef = parent->second->Name;
		}
	}

	if (version < 4)
		document.RecalculateNextStableId();
	DesignDocumentGraph graph;
	if (!DesignDocumentGraph::Build(document, graph, outError)) return false;
	DesignDocumentEventIndex eventIndex;
	return DesignDocumentEventIndex::Build(document, eventIndex, outError);
}
}
