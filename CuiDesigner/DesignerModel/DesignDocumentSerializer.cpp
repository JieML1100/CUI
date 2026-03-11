#include "DesignDocumentSerializer.h"
#include <CppUtils/Utils/Convert.h>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace DesignerModel
{
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

	static Json ColorToJson(const D2D1_COLOR_F& c)
	{
		return Json{ {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
	}

	static D2D1_COLOR_F ColorFromJson(const Json& j, const D2D1_COLOR_F& def)
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
		case UIClass::UI_Panel: return "Panel";
		case UIClass::UI_ScrollView: return "ScrollView";
		case UIClass::UI_StackPanel: return "StackPanel";
		case UIClass::UI_GridPanel: return "GridPanel";
		case UIClass::UI_DockPanel: return "DockPanel";
		case UIClass::UI_WrapPanel: return "WrapPanel";
		case UIClass::UI_RelativePanel: return "RelativePanel";
		case UIClass::UI_CheckBox: return "CheckBox";
		case UIClass::UI_RadioBox: return "RadioBox";
		case UIClass::UI_ComboBox: return "ComboBox";
		case UIClass::UI_GridView: return "GridView";
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
		case UIClass::UI_WebBrowser: return "WebBrowser";
		case UIClass::UI_MediaPlayer: return "MediaPlayer";
		case UIClass::UI_TabPage: return "TabPage";
		default: return "Base";
		}
	}

	static bool TryParseUIClass(const std::string& s, UIClass& out)
	{
		if (s == "Label") { out = UIClass::UI_Label; return true; }
		if (s == "LinkLabel") { out = UIClass::UI_LinkLabel; return true; }
		if (s == "Button") { out = UIClass::UI_Button; return true; }
		if (s == "TextBox") { out = UIClass::UI_TextBox; return true; }
		if (s == "RichTextBox") { out = UIClass::UI_RichTextBox; return true; }
		if (s == "PasswordBox") { out = UIClass::UI_PasswordBox; return true; }
		if (s == "DateTimePicker") { out = UIClass::UI_DateTimePicker; return true; }
		if (s == "Panel") { out = UIClass::UI_Panel; return true; }
		if (s == "ScrollView") { out = UIClass::UI_ScrollView; return true; }
		if (s == "StackPanel") { out = UIClass::UI_StackPanel; return true; }
		if (s == "GridPanel") { out = UIClass::UI_GridPanel; return true; }
		if (s == "DockPanel") { out = UIClass::UI_DockPanel; return true; }
		if (s == "WrapPanel") { out = UIClass::UI_WrapPanel; return true; }
		if (s == "RelativePanel") { out = UIClass::UI_RelativePanel; return true; }
		if (s == "CheckBox") { out = UIClass::UI_CheckBox; return true; }
		if (s == "RadioBox") { out = UIClass::UI_RadioBox; return true; }
		if (s == "ComboBox") { out = UIClass::UI_ComboBox; return true; }
		if (s == "GridView") { out = UIClass::UI_GridView; return true; }
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
		if (s == "WebBrowser") { out = UIClass::UI_WebBrowser; return true; }
		if (s == "MediaPlayer") { out = UIClass::UI_MediaPlayer; return true; }
		if (s == "TabPage") { out = UIClass::UI_TabPage; return true; }
		return false;
	}
}

bool DesignDocumentSerializer::SaveToFile(const DesignDocument& document, const std::wstring& filePath, std::wstring* outError)
{
	try
	{
		if (filePath.empty())
		{
			if (outError) *outError = L"File path is empty.";
			return false;
		}

		std::string out = ToJson(document).dump(2);
		std::ofstream f(filePath, std::ios::binary);
		if (!f.is_open())
		{
			if (outError) *outError = L"Failed to open file for writing.";
			return false;
		}
		f.write(out.data(), (std::streamsize)out.size());
		return true;
	}
	catch (const std::exception& ex)
	{
		if (outError) *outError = L"Save failed: " + FromUtf8(ex.what());
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
		Json root = Json::parse(ss.str(), nullptr, true, true);
		return FromJson(root, document, outError);
	}
	catch (const std::exception& ex)
	{
		if (outError) *outError = L"Load failed: " + FromUtf8(ex.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"Load failed: unknown error.";
		return false;
	}
}

Json DesignDocumentSerializer::ToJson(const DesignDocument& document)
{
	Json root;
	root["schema"] = document.Schema;
	root["version"] = document.SchemaVersion;

	Json formObj = Json{
		{"name", ToUtf8(document.Form.Name)},
		{"text", ToUtf8(document.Form.Text)},
		{"font", Json{{"name", ToUtf8(document.Form.FontName)}, {"size", document.Form.FontSize}}},
		{"size", Json{{"w", document.Form.Size.cx}, {"h", document.Form.Size.cy}}},
		{"location", Json{{"x", document.Form.Location.x}, {"y", document.Form.Location.y}}},
		{"backColor", ColorToJson(document.Form.BackColor)},
		{"foreColor", ColorToJson(document.Form.ForeColor)},
		{"showInTaskBar", document.Form.ShowInTaskBar},
		{"topMost", document.Form.TopMost},
		{"enable", document.Form.Enable},
		{"visible", document.Form.Visible},
		{"visibleHead", document.Form.VisibleHead},
		{"headHeight", document.Form.HeadHeight},
		{"minBox", document.Form.MinBox},
		{"maxBox", document.Form.MaxBox},
		{"closeBox", document.Form.CloseBox},
		{"centerTitle", document.Form.CenterTitle},
		{"allowResize", document.Form.AllowResize}
	};

	if (!document.Form.EventHandlers.empty())
	{
		Json events = Json::object();
		for (const auto& kv : document.Form.EventHandlers)
		{
			if (kv.first.empty() || kv.second.empty()) continue;
			events[ToUtf8(kv.first)] = true;
		}
		if (!events.empty())
		{
			formObj["events"] = events;
		}
	}
	root["form"] = formObj;

	Json controls = Json::array();
	for (const auto& node : document.Nodes)
	{
		Json item;
		item["name"] = ToUtf8(node.Name);
		item["type"] = UIClassToString(node.Type);
		if (node.ParentRef.empty()) item["parent"] = nullptr;
		else item["parent"] = ToUtf8(node.ParentRef);
		item["order"] = node.Order;
		item["props"] = node.Props.is_object() ? node.Props : Json::object();
		if (node.Extra.is_object() && !node.Extra.empty()) item["extra"] = node.Extra;
		if (node.Events.is_object() && !node.Events.empty()) item["events"] = node.Events;
		controls.push_back(item);
	}
	root["controls"] = controls;
	return root;
}

bool DesignDocumentSerializer::FromJson(const Json& root, DesignDocument& document, std::wstring* outError)
{
	if (root.value("schema", std::string()) != "cui.designer")
	{
		if (outError) *outError = L"Invalid CUI Designer file: schema mismatch.";
		return false;
	}

	int ver = root.value("version", 0);
	if (ver != 1)
	{
		if (outError) *outError = L"Unsupported design file version.";
		return false;
	}

	if (!root.contains("controls") || !root["controls"].is_array())
	{
		if (outError) *outError = L"Design file is missing the controls array.";
		return false;
	}

	document.Clear();
	document.Schema = "cui.designer";
	document.SchemaVersion = ver;

	if (root.contains("form") && root["form"].is_object())
	{
		auto& form = root["form"];
		document.Form.Name = FromUtf8(form.value("name", std::string()));
		if (document.Form.Name.empty()) document.Form.Name = L"MainForm";
		document.Form.Text = FromUtf8(form.value("text", std::string()));
		if (form.contains("font") && form["font"].is_object())
		{
			auto& fj = form["font"];
			document.Form.FontName = FromUtf8(fj.value("name", std::string()));
			document.Form.FontSize = (float)fj.value("size", (double)document.Form.FontSize);
			if (document.Form.FontSize < 1.0f) document.Form.FontSize = 1.0f;
			if (document.Form.FontSize > 200.0f) document.Form.FontSize = 200.0f;
		}
		document.Form.ShowInTaskBar = form.value("showInTaskBar", document.Form.ShowInTaskBar);
		document.Form.TopMost = form.value("topMost", document.Form.TopMost);
		document.Form.Enable = form.value("enable", document.Form.Enable);
		document.Form.Visible = form.value("visible", document.Form.Visible);
		document.Form.VisibleHead = form.value("visibleHead", document.Form.VisibleHead);
		document.Form.HeadHeight = form.value("headHeight", document.Form.HeadHeight);
		if (document.Form.HeadHeight < 0) document.Form.HeadHeight = 0;
		document.Form.MinBox = form.value("minBox", document.Form.MinBox);
		document.Form.MaxBox = form.value("maxBox", document.Form.MaxBox);
		document.Form.CloseBox = form.value("closeBox", document.Form.CloseBox);
		document.Form.CenterTitle = form.value("centerTitle", document.Form.CenterTitle);
		document.Form.AllowResize = form.value("allowResize", document.Form.AllowResize);
		if (form.contains("size") && form["size"].is_object())
		{
			document.Form.Size.cx = form["size"].value("w", document.Form.Size.cx);
			document.Form.Size.cy = form["size"].value("h", document.Form.Size.cy);
		}
		if (form.contains("location") && form["location"].is_object())
		{
			document.Form.Location.x = form["location"].value("x", document.Form.Location.x);
			document.Form.Location.y = form["location"].value("y", document.Form.Location.y);
		}
		document.Form.BackColor = ColorFromJson(form.value("backColor", Json()), document.Form.BackColor);
		document.Form.ForeColor = ColorFromJson(form.value("foreColor", Json()), document.Form.ForeColor);
		if (form.contains("events") && form["events"].is_object())
		{
			for (auto it = form["events"].begin(); it != form["events"].end(); ++it)
			{
				std::wstring name = FromUtf8(it.key());
				if (name.empty()) continue;
				if (it.value().is_boolean())
				{
					if (it.value().get<bool>()) document.Form.EventHandlers[name] = L"1";
				}
				else if (it.value().is_string())
				{
					auto v = FromUtf8(it.value().get<std::string>());
					if (!v.empty()) document.Form.EventHandlers[name] = v;
					else document.Form.EventHandlers[name] = L"1";
				}
			}
		}
	}

	std::unordered_set<std::wstring> nameSet;
	for (auto& j : root["controls"])
	{
		if (!j.is_object()) continue;

		DesignNode node;
		node.Name = FromUtf8(j.value("name", std::string()));
		std::string typeStr = j.value("type", std::string());
		if (node.Name.empty() || !TryParseUIClass(typeStr, node.Type))
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

		node.Id = document.AllocateNodeId();
		if (j.contains("parent") && j["parent"].is_string())
		{
			node.ParentRef = FromUtf8(j["parent"].get<std::string>());
		}
		node.Order = j.value("order", -1);
		node.Props = j.contains("props") && j["props"].is_object() ? j["props"] : Json::object();
		node.Extra = j.contains("extra") && j["extra"].is_object() ? j["extra"] : Json::object();
		node.Events = j.contains("events") && j["events"].is_object() ? j["events"] : Json::object();
		document.Nodes.push_back(std::move(node));
	}

	document.RecalculateNextStableId();
	return true;
}
}