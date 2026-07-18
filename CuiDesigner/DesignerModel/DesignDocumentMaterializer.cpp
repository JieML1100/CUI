#include "DesignDocumentMaterializer.h"

#include "DesignDocumentControlPool.h"
#include "DesignDocumentGraph.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/Panel.h"
#include "../../CUI/include/Button.h"
#include "../../CUI/include/CheckBox.h"
#include "../../CUI/include/ComboBox.h"
#include "../../CUI/include/DateTimePicker.h"
#include "../../CUI/include/Expander.h"
#include "../../CUI/include/GroupBox.h"
#include "../../CUI/include/Label.h"
#include "../../CUI/include/LinkLabel.h"
#include "../../CUI/include/LoadingRing.h"
#include "../../CUI/include/NumericUpDown.h"
#include "../../CUI/include/PasswordBox.h"
#include "../../CUI/include/PictureBox.h"
#include "../../CUI/include/ProgressBar.h"
#include "../../CUI/include/ProgressRing.h"
#include "../../CUI/include/RadioBox.h"
#include "../../CUI/include/RichTextBox.h"
#include "../../CUI/include/ScrollView.h"
#include "../../CUI/include/Slider.h"
#include "../../CUI/include/Switch.h"
#include "../../CUI/include/TextBox.h"
#include "../../CUI/include/WebBrowser.h"
#include "../../CUI/include/ListView.h"
#include "../../CUI/include/GridView.h"
#include "../../CUI/include/PropertyGrid.h"
#include "../../CUI/include/ChartView.h"
#include "../../CUI/include/ReportView.h"
#include "../../CUI/include/KpiCard.h"
#include "../../CUI/include/FilterBar.h"
#include "../../CUI/include/TreeView.h"
#include "../../CUI/include/TabControl.h"
#include "../../CUI/include/ToolBar.h"
#include "../../CUI/include/Menu.h"
#include "../../CUI/include/StatusBar.h"
#include "../../CUI/include/Toast.h"
#include "../../CUI/include/MediaPlayer.h"
#include "../../CUI/include/NavigationView.h"
#include "../../CUI/include/CalendarView.h"
#include "../../CUI/include/ColorPicker.h"
#include "../../CUI/include/PagedGridView.h"
#include "../../CUI/include/SplitContainer.h"
#include "../../CUI/include/Layout/StackPanel.h"
#include "../../CUI/include/Layout/GridPanel.h"
#include "../../CUI/include/Layout/DockPanel.h"
#include "../../CUI/include/Layout/WrapPanel.h"
#include "../../CUI/include/Layout/RelativePanel.h"
#include <Convert.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <unordered_map>
#include <utility>

using DesignValue = DesignerModel::DesignValue;

namespace
{
	static bool IsSplitContainerControl(Control* control)
	{
		return control && control->Type() == UIClass::UI_SplitContainer;
	}
	
	static SplitContainer* AsSplitContainer(Control* control)
	{
		return IsSplitContainerControl(control) ? (SplitContainer*)control : nullptr;
	}
	
	static void RefreshDesignerPanelLayout(Control* control)
	{
		if (!control) return;
		if (auto* split = dynamic_cast<SplitContainer*>(control))
		{
			split->RefreshSplitterLayout();
			return;
		}
		if (auto* panel = dynamic_cast<Panel*>(control))
		{
			panel->InvalidateLayout();
			panel->PerformLayout();
		}
	}
	
	static bool ApplyTrackedMetadataProperty(
		DesignerControl& designerControl,
		Control& target,
		const std::wstring& propertyName,
		DesignerStyleValue value,
		bool preserveExisting,
		std::wstring* outError = nullptr)
	{
		const auto* metadata = target.FindPropertyMetadata(propertyName);
		const std::wstring canonicalCandidate = metadata
			? metadata->Name() : propertyName;
		const auto existing = std::find_if(
			designerControl.MetadataProperties.begin(),
			designerControl.MetadataProperties.end(),
			[&](const auto& entry)
			{
				return _wcsicmp(entry.first.c_str(), canonicalCandidate.c_str()) == 0;
			});
		if (preserveExisting && existing != designerControl.MetadataProperties.end())
		{
			if (outError) outError->clear();
			return true;
		}
	
		std::wstring canonicalName;
		DesignerStyleValue effective;
		if (!DesignerPropertyCatalog::ApplyAndTrackValue(
			target, designerControl.MetadataProperties, propertyName, value,
			&canonicalName, &effective, outError)) return false;
		return true;
	}
	
	static std::wstring FromUtf8(const std::string& s)
		{
			return Convert::Utf8ToUnicode(s);
		}
	
	static void ValueToMenuSubItems(const DesignValue& arr, std::vector<MenuItem*>& out, MenuItem* owner)
		{
			if (!owner) return;
			if (!arr.is_array()) return;
			for (auto& j : arr)
			{
				if (!j.is_object()) continue;
				bool sep = j.value("separator", false);
				if (sep)
				{
					auto* separatorItem = owner->AddSeparator();
					if (!separatorItem) continue;
					continue;
				}
				auto text = FromUtf8(j.value("text", std::string()));
				int id = j.value("id", 0);
				auto* subItem = owner->AddSubItem(text, id);
				if (!subItem) continue;
				subItem->Shortcut = FromUtf8(j.value("shortcut", std::string()));
				subItem->Enable = j.value("enable", true);
				if (j.contains("subItems"))
				{
					ValueToMenuSubItems(j["subItems"], out, subItem);
				}
			}
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
	
	static std::wstring ColorToMetadataText(const D2D1_COLOR_F& color)
		{
			auto byte = [](float value) -> unsigned int
			{
				return static_cast<unsigned int>(std::lround(
					(std::clamp)(value, 0.0f, 1.0f) * 255.0f));
			};
			wchar_t text[10]{};
			swprintf_s(text, L"#%02X%02X%02X%02X",
				byte(color.a), byte(color.r), byte(color.g), byte(color.b));
			return text;
		}
	
	static Thickness ThicknessFromValue(const DesignValue& j, const Thickness& def)
		{
			Thickness t = def;
			if (j.is_object())
			{
				t.Left = j.value("l", def.Left);
				t.Top = j.value("t", def.Top);
				t.Right = j.value("r", def.Right);
				t.Bottom = j.value("b", def.Bottom);
			}
			return t;
		}
	
	static bool TryParseHAlign(const std::string& s, HorizontalAlignment& out)
		{
			if (s == "Left") { out = HorizontalAlignment::Left; return true; }
			if (s == "Center") { out = HorizontalAlignment::Center; return true; }
			if (s == "Right") { out = HorizontalAlignment::Right; return true; }
			if (s == "Stretch") { out = HorizontalAlignment::Stretch; return true; }
			return false;
		}
	
	static bool TryParseVAlign(const std::string& s, VerticalAlignment& out)
		{
			if (s == "Top") { out = VerticalAlignment::Top; return true; }
			if (s == "Center") { out = VerticalAlignment::Center; return true; }
			if (s == "Bottom") { out = VerticalAlignment::Bottom; return true; }
			if (s == "Stretch") { out = VerticalAlignment::Stretch; return true; }
			return false;
		}
	
	static bool TryParseDock(const std::string& s, Dock& out)
		{
			if (s == "Left") { out = Dock::Left; return true; }
			if (s == "Top") { out = Dock::Top; return true; }
			if (s == "Right") { out = Dock::Right; return true; }
			if (s == "Bottom") { out = Dock::Bottom; return true; }
			if (s == "Fill") { out = Dock::Fill; return true; }
			return false;
		}
	
	static bool TryParseOrientation(const std::string& s, Orientation& out)
		{
			if (s == "Horizontal") { out = Orientation::Horizontal; return true; }
			if (s == "Vertical") { out = Orientation::Vertical; return true; }
			return false;
		}
	
	static bool TryParseSizeUnit(const std::string& s, SizeUnit& out)
		{
			if (s == "Pixel") { out = SizeUnit::Pixel; return true; }
			if (s == "Percent") { out = SizeUnit::Percent; return true; }
			if (s == "Auto") { out = SizeUnit::Auto; return true; }
			if (s == "Star") { out = SizeUnit::Star; return true; }
			return false;
		}
	
	static GridLength GridLengthFromValue(const DesignValue& j, const GridLength& def)
		{
			GridLength gl = def;
			if (!j.is_object()) return gl;
			gl.Value = j.value("value", def.Value);
			SizeUnit u = def.Unit;
			if (j.contains("unit") && j["unit"].is_string())
			{
				TryParseSizeUnit(j["unit"].get<std::string>(), u);
			}
			gl.Unit = u;
			return gl;
		}
	
	static void ValueToTreeNodes(const DesignValue& j, std::vector<TreeNode*>& outNodes)
		{
			if (!j.is_array()) return;
			for (auto& it : j)
			{
				if (!it.is_object()) continue;
				auto text = FromUtf8(it.value("text", std::string()));
				auto* node = new TreeNode(text);
				node->Expand = it.value("expand", false);
				if (it.contains("children"))
					ValueToTreeNodes(it["children"], node->Children);
				outNodes.push_back(node);
			}
		}
	
	static void ValueToListViewItems(const DesignValue& j, std::vector<ListViewItem>& outItems)
		{
			if (!j.is_array()) return;
			for (auto& it : j)
			{
				if (!it.is_object()) continue;
				ListViewItem item(FromUtf8(it.value("text", std::string())));
				item.SubText = FromUtf8(it.value("subText", std::string()));
				item.Checked = it.value("checked", false);
				item.Selected = it.value("selected", false);
				item.Enabled = it.value("enabled", true);
				if (it.contains("subItems") && it["subItems"].is_array())
				{
					for (auto& sj : it["subItems"])
						if (sj.is_string()) item.SubItems.push_back(FromUtf8(sj.get<std::string>()));
				}
				outItems.push_back(std::move(item));
			}
		}
	
	static void ValueToPropertyGridItems(const DesignValue& j, std::vector<PropertyGridItem>& outItems)
		{
			if (!j.is_array()) return;
			for (auto& it : j)
			{
				if (!it.is_object()) continue;
				PropertyGridItem item;
				item.Category = FromUtf8(it.value("category", std::string()));
				item.Name = FromUtf8(it.value("name", std::string()));
				item.Value = FromUtf8(it.value("value", std::string()));
				item.Description = FromUtf8(it.value("description", std::string()));
				item.ValueType = (PropertyGridValueType)it.value("type", (int)PropertyGridValueType::Text);
				item.ReadOnly = it.value("readOnly", false);
				item.IsMixed = it.value("isMixed", false);
				item.CanReset = it.value("canReset", false);
				item.Minimum = it.value("minimum", 0.0);
				item.Maximum = it.value("maximum", 1.0);
				item.Step = it.value("step", 0.01);
				item.Tag = static_cast<UINT64>(
					it.value("tag", static_cast<unsigned long long>(0)));
				if (it.contains("options") && it["options"].is_array())
				{
					for (auto& oj : it["options"])
						if (oj.is_string()) item.Options.push_back(FromUtf8(oj.get<std::string>()));
				}
				outItems.push_back(std::move(item));
			}
		}
}

std::unique_ptr<Control>
DesignerModel::DesignDocumentMaterializer::CreateRuntimeControl(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_Base: return std::make_unique<Control>();
	case UIClass::UI_Label: return std::make_unique<Label>(L"标签", 0, 0);
	case UIClass::UI_LinkLabel: return std::make_unique<LinkLabel>(L"链接标签", 0, 0);
	case UIClass::UI_Button: return std::make_unique<Button>(L"按钮", 0, 0, 120, 30);
	case UIClass::UI_TextBox: return std::make_unique<TextBox>(L"", 0, 0, 200, 25);
	case UIClass::UI_RichTextBox: return std::make_unique<RichTextBox>(L"", 0, 0, 300, 160);
	case UIClass::UI_PasswordBox: return std::make_unique<PasswordBox>(L"", 0, 0, 200, 25);
	case UIClass::UI_DateTimePicker: return std::make_unique<DateTimePicker>(L"", 0, 0, 200, 28);
	case UIClass::UI_NumericUpDown: return std::make_unique<NumericUpDown>(0, 0, 140, 30);
	case UIClass::UI_Panel: return std::make_unique<Panel>(0, 0, 200, 200);
	case UIClass::UI_GroupBox: return std::make_unique<GroupBox>(L"GroupBox", 0, 0, 240, 180);
	case UIClass::UI_Expander: return std::make_unique<Expander>(L"Expander", 0, 0, 260, 160);
	case UIClass::UI_ScrollView: return std::make_unique<ScrollView>(0, 0, 240, 200);
	case UIClass::UI_StackPanel: return std::make_unique<StackPanel>(0, 0, 200, 200);
	case UIClass::UI_GridPanel: return std::make_unique<GridPanel>(0, 0, 200, 200);
	case UIClass::UI_DockPanel: return std::make_unique<DockPanel>(0, 0, 200, 200);
	case UIClass::UI_WrapPanel: return std::make_unique<WrapPanel>(0, 0, 200, 200);
	case UIClass::UI_RelativePanel: return std::make_unique<RelativePanel>(0, 0, 200, 200);
	case UIClass::UI_SplitContainer: return std::make_unique<SplitContainer>(0, 0, 360, 220);
	case UIClass::UI_CheckBox: return std::make_unique<CheckBox>(L"复选框", 0, 0);
	case UIClass::UI_RadioBox: return std::make_unique<RadioBox>(L"单选框", 0, 0);
	case UIClass::UI_ComboBox: return std::make_unique<ComboBox>(L"", 0, 0, 150, 25);
	case UIClass::UI_ListView: return std::make_unique<ListView>(0, 0, 320, 220);
	case UIClass::UI_ListBox: return std::make_unique<ListBox>(0, 0, 220, 180);
	case UIClass::UI_GridView: return std::make_unique<GridView>(0, 0, 360, 200);
	case UIClass::UI_PropertyGrid: return std::make_unique<PropertyGridView>(0, 0, 300, 320);
	case UIClass::UI_ChartView: return std::make_unique<ChartView>(0, 0, 420, 260);
	case UIClass::UI_ReportView: return std::make_unique<ReportView>(0, 0, 480, 300);
	case UIClass::UI_KpiCard: return std::make_unique<KpiCard>(0, 0, 220, 132);
	case UIClass::UI_FilterBar: return std::make_unique<FilterBar>(0, 0, 640, 48);
	case UIClass::UI_TreeView: return std::make_unique<TreeView>(0, 0, 220, 220);
	case UIClass::UI_ProgressBar: return std::make_unique<ProgressBar>(0, 0, 200, 20);
	case UIClass::UI_LoadingRing: return std::make_unique<LoadingRing>(0, 0, 48, 48);
	case UIClass::UI_ProgressRing: return std::make_unique<ProgressRing>(0, 0, 72, 72);
	case UIClass::UI_Slider: return std::make_unique<Slider>(0, 0, 200, 30);
	case UIClass::UI_PictureBox: return std::make_unique<PictureBox>(0, 0, 150, 150);
	case UIClass::UI_Switch: return std::make_unique<Switch>(0, 0, 60, 30);
	case UIClass::UI_TabControl: return std::make_unique<TabControl>(0, 0, 360, 240);
	case UIClass::UI_ToolBar: return std::make_unique<ToolBar>(0, 0, 360, 34);
	case UIClass::UI_Menu: return std::make_unique<Menu>(0, 0, 600, 28);
	case UIClass::UI_StatusBar: return std::make_unique<StatusBar>(0, 0, 600, 26);
	case UIClass::UI_ToastHost: return std::make_unique<ToastHost>(0, 0, 340, 260);
	case UIClass::UI_WebBrowser: return std::make_unique<WebBrowser>(0, 0, 500, 360);
	case UIClass::UI_MediaPlayer: return std::make_unique<MediaPlayer>(0, 0, 640, 360);
	case UIClass::UI_NavigationView: return std::make_unique<NavigationView>(0, 0, 220, 360);
	case UIClass::UI_SideBar: return std::make_unique<SideBar>(0, 0, 200, 360);
	case UIClass::UI_BreadcrumbBar: return std::make_unique<BreadcrumbBar>(0, 0, 320, 32);
	case UIClass::UI_CalendarView: return std::make_unique<CalendarView>(0, 0, 280, 300);
	case UIClass::UI_DateRangePicker: return std::make_unique<DateRangePicker>(L"", 0, 0, 240, 30);
	case UIClass::UI_ColorPicker: return std::make_unique<ColorPicker>(0, 0, 180, 30);
	case UIClass::UI_PagedGridView: return std::make_unique<PagedGridView>(0, 0, 520, 320);
	default: return nullptr;
	}
}

bool DesignerModel::DesignDocumentMaterializer::Materialize(
	const DesignDocument& document,
	MaterializedControlTree& output,
	std::wstring* outError)
{
	return Materialize(
		document, output, DesignDocumentMaterializationOptions{}, outError);
}

bool DesignerModel::DesignDocumentMaterializer::Materialize(
	const DesignDocument& document,
	MaterializedControlTree& output,
	const DesignDocumentMaterializationOptions& options,
	std::wstring* outError)
{
	try
	{
		const auto createBaseControl = options.ControlFactory
			? options.ControlFactory
			: std::function<std::unique_ptr<Control>(UIClass)>(
				DesignDocumentMaterializer::CreateRuntimeControl);
		const DesignDocumentControlPool::Factory createControl =
			[&](const DesignNode& node) -> std::unique_ptr<Control>
			{
				if (!node.CustomType.Empty())
				{
					if (options.CustomControlFactory)
					{
						auto control = options.CustomControlFactory(node);
						if (control)
							return control->Type() == node.Type
								? std::move(control) : nullptr;
					}
					if (!options.AllowCustomControlProxy) return nullptr;
				}
				return createBaseControl(node.Type);
			};
		if (!DesignerDataContextSchemaUtils::Validate(document.DataContextSchema, outError))
			return false;
		if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			document.StyleSheet,
			createBaseControl,
			outError))
			return false;
		DesignDocumentGraph documentGraph;
		if (!DesignDocumentGraph::Build(
			document, documentGraph, outError))
			return false;
		DesignDocumentControlPool controlPool;
		if (!DesignDocumentControlPool::Build(
			document,
			documentGraph,
			createControl,
			controlPool,
			outError))
			return false;

		const int stagingWidth = document.Form.Size.cx > 0
			? document.Form.Size.cx : 1;
		const int stagingHeight = document.Form.Size.cy > 0
			? document.Form.Size.cy : 1;
		Panel stagingRoot(0, 0, stagingWidth, stagingHeight);
		MaterializedControlTree candidate;
		auto dataContextSchema = document.DataContextSchema;
		DesignerDataContextSchemaUtils::Canonicalize(dataContextSchema);

		struct Pending
		{
			std::wstring name;
			int id = 0;
			UIClass type = UIClass::UI_Base;
			std::wstring parent;
			int order = -1;
			bool locked = false;
			DesignValue props;
			DesignValue extra;
			DesignValue events;
			DesignValue bindings;
			DesignerCustomControlType customType;
			std::vector<DesignerCustomEventDescriptor> customEvents;
		};
		std::vector<Pending> items;
		items.reserve(document.Nodes.size());

		for (const auto& resolved : documentGraph.Nodes())
		{
			const auto& node = document.Nodes[resolved.SourceIndex];
			Pending p;
			p.name = node.Name;
			p.id = node.Id;
			p.type = node.Type;
			p.parent = resolved.ParentKey;
			p.order = node.Order;
			p.locked = node.Locked;
			p.props = node.Props.is_object() ? node.Props : DesignValue::object();
			p.extra = node.Extra.is_object() ? node.Extra : DesignValue::object();
			p.events = node.Events.is_object() ? node.Events : DesignValue::object();
			p.bindings = node.Bindings.is_object() ? node.Bindings : DesignValue::object();
			p.customType = node.CustomType;
			p.customEvents = node.CustomEvents;
			items.push_back(std::move(p));
		}
		std::unordered_map<std::wstring, std::shared_ptr<DesignerControl>> dcOf;
		dcOf.reserve(items.size());
		std::unordered_map<std::wstring, Control*> instOf;
		instOf.reserve(items.size());

		std::unordered_map<std::wstring, Control*> tabPageOf;
		tabPageOf.reserve(64);

		for (auto& it : items)
		{
			Control* c = controlPool.FindById(it.id);
			if (!c) return false;
			auto dc = std::make_shared<DesignerControl>(
				c, it.name, it.type, nullptr, it.id);
			dc->CustomType = it.customType;
			dc->CustomEvents = it.customEvents;
			dc->IsLocked = it.locked;
			dcOf[it.name] = dc;
			instOf[it.name] = c;
		}

		for (auto& it : items)
		{
			auto dcIt = dcOf.find(it.name);
			if (dcIt == dcOf.end()) continue;
			auto dc = dcIt->second;
			auto* c = dc->ControlInstance;
			if (!c) continue;

			if (it.events.is_object())
			{
				dc->EventHandlers.clear();
				for (const auto& [eventName, eventValue] : it.events.ObjectItems())
				{
					std::wstring k = FromUtf8(eventName);
					if (k.empty()) continue;
					if (eventValue.is_boolean())
					{
						if (eventValue.get<bool>())
							dc->EventHandlers[k] = L"1";
					}
					else if (eventValue.is_string())
					{
						std::wstring v = FromUtf8(eventValue.get<std::string>());
						if (!v.empty()) dc->EventHandlers[k] = v;
					}
				}
			}

			if (it.bindings.is_object())
			{
				dc->DataBindings.clear();
				for (const auto& [targetName, bindingValue] : it.bindings.ObjectItems())
				{
					if (!bindingValue.is_object())
					{
						if (outError) *outError = L"控件 " + it.name + L" 的数据绑定格式无效。";
						return false;
					}
					const std::wstring targetProperty = FromUtf8(targetName);
					const std::wstring sourceProperty = FromUtf8(
						bindingValue.value("source", std::string()));
					const int modeValue = bindingValue.value(
						"mode", static_cast<int>(BindingMode::OneWay));
					const int updateModeValue = bindingValue.value(
						"updateMode", static_cast<int>(DataSourceUpdateMode::OnPropertyChanged));
					const std::wstring converter = DesignerBindingUtils::Trim(FromUtf8(
						bindingValue.value("converter", std::string())));
					if (modeValue < static_cast<int>(BindingMode::OneWay)
						|| modeValue > static_cast<int>(BindingMode::OneTime)
						|| updateModeValue < static_cast<int>(DataSourceUpdateMode::OnPropertyChanged)
						|| updateModeValue > static_cast<int>(DataSourceUpdateMode::Never))
					{
						if (outError) *outError = L"控件 " + it.name + L" 的数据绑定参数无效。";
						return false;
					}

					DesignerDataBinding binding{
						DesignerBindingUtils::Trim(sourceProperty),
						static_cast<BindingMode>(modeValue),
						static_cast<DataSourceUpdateMode>(updateModeValue),
						converter };
					if (!it.customType.Empty()
						&& options.AllowDeferredCustomMetadata
						&& !c->FindPropertyMetadata(targetProperty))
					{
						if (targetProperty.empty()
							|| !DesignerBindingUtils::IsValidSourcePath(
								binding.SourceProperty))
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的延迟自定义属性绑定无效。";
							return false;
						}
						dc->DataBindings[targetProperty] = std::move(binding);
						continue;
					}
					const BindingPropertyMetadata* metadata = nullptr;
					std::wstring validationError;
					if (!DesignerBindingUtils::Validate(
						*c, targetProperty, binding, &metadata, &validationError,
						&dataContextSchema))
					{
						if (outError) *outError = L"控件 " + it.name + L"：" + validationError;
						return false;
					}

					dc->DataBindings[metadata->Name()] = std::move(binding);
				}
			}

			if (it.props.is_object())
			{
				c->Text = FromUtf8(it.props.value("text", std::string()));
				c->SetStyleId(it.props.contains("styleId") && it.props["styleId"].is_string()
					? FromUtf8(it.props["styleId"].get<std::string>())
					: std::wstring{});
				c->ClearStyleClasses();
				if (it.props.contains("styleClasses") && it.props["styleClasses"].is_array())
				{
					for (const auto& styleClass : it.props["styleClasses"])
					{
						if (styleClass.is_string())
							c->AddStyleClass(FromUtf8(styleClass.get<std::string>()));
					}
				}
				if (it.props.contains("location"))
				{
					auto& l = it.props["location"];
					if (l.is_object())
						c->Location = { l.value("x", 0), l.value("y", 0) };
				}
				if (it.props.contains("size"))
				{
					auto& s = it.props["size"];
					if (s.is_object())
						c->Size = { s.value("w", c->Size.cx), s.value("h", c->Size.cy) };
				}
				c->Enable = it.props.value("enable", true);
				c->Visible = it.props.value("visible", true);
				c->BackColor = ColorFromValue(it.props.contains("backColor") ? it.props["backColor"] : DesignValue(), c->BackColor);
				c->ForeColor = ColorFromValue(it.props.contains("foreColor") ? it.props["foreColor"] : DesignValue(), c->ForeColor);
				DesignValue borderColorValue = it.props.contains("borderColor")
					? it.props["borderColor"]
					: (it.props.contains("bolderColor") ? it.props["bolderColor"] : DesignValue());
				c->BorderColor = ColorFromValue(borderColorValue, c->BorderColor);
				c->ShowValidationBorder = it.props.value("showValidationBorder", c->ShowValidationBorder);
				c->ShowValidationToolTip = it.props.value("showValidationToolTip", c->ShowValidationToolTip);
				c->ValidationBorderThickness = (float)it.props.value(
					"validationBorderThickness", (double)c->ValidationBorderThickness);
				c->ValidationCornerRadius = (float)it.props.value(
					"validationCornerRadius", (double)c->ValidationCornerRadius);
				c->ValidationToolTipMaxWidth = (float)it.props.value(
					"validationToolTipMaxWidth", (double)c->ValidationToolTipMaxWidth);
				if (it.props.contains("accessibleDescription")
					&& it.props["accessibleDescription"].is_string())
					c->AccessibleDescription = FromUtf8(
						it.props["accessibleDescription"].get<std::string>());
				c->Margin = ThicknessFromValue(it.props.contains("margin") ? it.props["margin"] : DesignValue(), c->Margin);
				c->Padding = ThicknessFromValue(it.props.contains("padding") ? it.props["padding"] : DesignValue(), c->Padding);
				c->AnchorStyles = (uint8_t)it.props.value("anchor", (int)c->AnchorStyles);
				HorizontalAlignment ha = c->HAlign;
				VerticalAlignment va = c->VAlign;
				Dock dk = c->DockPosition;
				if (it.props.contains("hAlign") && it.props["hAlign"].is_string())
					TryParseHAlign(it.props["hAlign"].get<std::string>(), ha);
				if (it.props.contains("vAlign") && it.props["vAlign"].is_string())
					TryParseVAlign(it.props["vAlign"].get<std::string>(), va);
				if (it.props.contains("dock") && it.props["dock"].is_string())
					TryParseDock(it.props["dock"].get<std::string>(), dk);
				c->HAlign = ha;
				c->VAlign = va;
				c->DockPosition = dk;
				c->ZIndex = it.props.value("zIndex", c->ZIndex);
				c->GridRow = it.props.value("gridRow", c->GridRow);
				c->GridColumn = it.props.value("gridColumn", c->GridColumn);
				c->GridRowSpan = it.props.value("gridRowSpan", c->GridRowSpan);
				c->GridColumnSpan = it.props.value("gridColumnSpan", c->GridColumnSpan);
				c->SizeMode = (ImageSizeMode)it.props.value("sizeMode", (int)c->SizeMode);

				// Font：有显式设置则创建新对象，否则跟随窗体字体/框架默认
				if (it.props.contains("font") && it.props["font"].is_object())
				{
					auto& fj = it.props["font"];
					std::wstring fn = FromUtf8(fj.value("name", std::string()));
					float fs = (float)fj.value("size", (double)GetDefaultFontObject()->FontSize);
					if (fs < 1.0f) fs = 1.0f;
					if (fs > 200.0f) fs = 200.0f;
					if (fn.empty()) fn = GetDefaultFontObject()->FontName;
					c->Font = new ::Font(fn, fs);
				}
				else
				{
					c->SetFontEx(nullptr, false);
				}

				if (it.props.contains("metadata") && it.props["metadata"].is_object())
				{
					using MetadataEntry = std::pair<const std::string*, const DesignValue*>;
					std::vector<MetadataEntry> metadataEntries;
					for (const auto& [propertyKey, propertyValue]
						: it.props["metadata"].ObjectItems())
					{
						metadataEntries.emplace_back(&propertyKey, &propertyValue);
					}
					std::stable_sort(metadataEntries.begin(), metadataEntries.end(),
						[c](const MetadataEntry& left, const MetadataEntry& right)
						{
							const auto leftName = FromUtf8(*left.first);
							const auto rightName = FromUtf8(*right.first);
							const auto* leftMetadata = c->FindPropertyMetadata(leftName);
							const auto* rightMetadata = c->FindPropertyMetadata(rightName);
							if (leftMetadata && rightMetadata)
							{
								const auto& leftDesign = leftMetadata->Design();
								const auto& rightDesign = rightMetadata->Design();
								if (leftDesign.CategoryOrder != rightDesign.CategoryOrder)
									return leftDesign.CategoryOrder < rightDesign.CategoryOrder;
								if (leftDesign.Order != rightDesign.Order)
									return leftDesign.Order < rightDesign.Order;
							}
							else if (leftMetadata != rightMetadata)
							{
								return leftMetadata != nullptr;
							}
							return _wcsicmp(leftName.c_str(), rightName.c_str()) < 0;
						});

					for (const auto& [propertyKeyPointer, propertyValuePointer]
						: metadataEntries)
					{
						const auto& propertyKey = *propertyKeyPointer;
						const auto& propertyValue = *propertyValuePointer;
						if (!propertyValue.is_object()
							|| !propertyValue.contains("kind")
							|| !propertyValue["kind"].is_string()
							|| !propertyValue.contains("value")
							|| !propertyValue["value"].is_string())
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的元数据属性格式无效。";
							return false;
						}
						DesignerStyleValue value;
						if (!DesignerStyleSheetUtils::TryParseValueKind(
							FromUtf8(propertyValue["kind"].get<std::string>()), value.Kind))
						{
							if (outError) *outError = L"控件 " + it.name
								+ L" 的元数据属性类型无效。";
							return false;
						}
						value.Text = FromUtf8(propertyValue["value"].get<std::string>());
						const auto propertyName = FromUtf8(propertyKey);
						if (!it.customType.Empty()
							&& options.AllowDeferredCustomMetadata
							&& !c->FindPropertyMetadata(propertyName))
						{
							dc->MetadataProperties[propertyName] = std::move(value);
							continue;
						}
						std::wstring canonicalName;
						DesignerStyleValue effective;
						std::wstring metadataError;
						if (!DesignerPropertyCatalog::ApplyAndTrackValue(
							*c, dc->MetadataProperties,
							propertyName, value,
							&canonicalName, &effective, &metadataError))
						{
							if (outError) *outError = L"控件 " + it.name + L"：" + metadataError;
							return false;
						}
					}
				}
			}

			auto migrateLegacyMetadata = [&](const wchar_t* propertyName,
				DesignerStyleValue value) -> bool
			{
				std::wstring metadataError;
				if (!ApplyTrackedMetadataProperty(
					*dc, *c, propertyName, std::move(value), true, &metadataError))
				{
					if (outError) *outError = L"控件 " + it.name
						+ L" 的旧格式属性迁移失败：" + metadataError;
					return false;
				}
				return true;
			};

			if (it.extra.is_object())
			{
				if (it.type == UIClass::UI_GridPanel)
				{
					auto* gridPanel = (GridPanel*)c;
					gridPanel->ClearRows();
					gridPanel->ClearColumns();
					if (it.extra.contains("rows") && it.extra["rows"].is_array())
					{
						for (auto& r : it.extra["rows"])
						{
							if (!r.is_object()) continue;
							GridLength h = GridLengthFromValue(r.contains("height") ? r["height"] : DesignValue(), GridLength::Auto());
							float minH = r.value("min", 0.0f);
							float maxH = r.value("max", FLT_MAX);
							gridPanel->AddRow(h, minH, maxH);
						}
					}
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& col : it.extra["columns"])
						{
							if (!col.is_object()) continue;
							GridLength w = GridLengthFromValue(col.contains("width") ? col["width"] : DesignValue(), GridLength::Auto());
							float minW = col.value("min", 0.0f);
							float maxW = col.value("max", FLT_MAX);
							gridPanel->AddColumn(w, minW, maxW);
						}
					}
				}
				else if (it.type == UIClass::UI_TabControl)
				{
					auto* tabControl = (TabControl*)c;
					if (it.extra.contains("selectedIndex")
						&& !migrateLegacyMetadata(L"SelectedIndex", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"selectedIndex", tabControl->SelectedIndex)) })) return false;
					if (it.extra.contains("titleHeight")
						&& !migrateLegacyMetadata(L"TitleHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value(
								"titleHeight", static_cast<float>(tabControl->TitleHeight))) })) return false;
					if (it.extra.contains("titleWidth")
						&& !migrateLegacyMetadata(L"TitleWidth", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value(
								"titleWidth", static_cast<float>(tabControl->TitleWidth))) })) return false;
					if (it.extra.contains("titlePosition")
						&& !migrateLegacyMetadata(L"TitlePosition", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"titlePosition", static_cast<int>(tabControl->TitlePosition))) })) return false;
					if (it.extra.contains("animationMode")
						&& !migrateLegacyMetadata(L"AnimationMode", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"animationMode", static_cast<int>(tabControl->AnimationMode))) })) return false;
					if (it.extra.contains("pages") && it.extra["pages"].is_array())
					{
						for (auto& pj : it.extra["pages"])
						{
							if (!pj.is_object()) continue;
							std::wstring id = FromUtf8(pj.value("id", std::string()));
							auto text = FromUtf8(pj.value("text", std::string("Page")));
							auto* page = tabControl->AddPage(text);
							if (page)
								tabPageOf[id] = page;
						}
					}
				}
				else if (it.type == UIClass::UI_StackPanel)
				{
					Orientation o;
					if (it.extra.contains("orientation") && it.extra["orientation"].is_string() && TryParseOrientation(it.extra["orientation"].get<std::string>(), o))
					{
						if (!migrateLegacyMetadata(L"Orientation", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(o)) })) return false;
					}
					if (it.extra.contains("spacing"))
					{
						if (!migrateLegacyMetadata(L"Spacing", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("spacing", 0.0f)) })) return false;
					}
					HorizontalAlignment horizontalAlignment = HorizontalAlignment::Stretch;
					VerticalAlignment verticalAlignment = VerticalAlignment::Stretch;
					if (it.extra.contains("horizontalContentAlignment")
						&& it.extra["horizontalContentAlignment"].is_string()
						&& TryParseHAlign(it.extra["horizontalContentAlignment"].get<std::string>(), horizontalAlignment))
					{
						if (!migrateLegacyMetadata(L"HorizontalContentAlignment", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(horizontalAlignment)) })) return false;
					}
					if (it.extra.contains("verticalContentAlignment")
						&& it.extra["verticalContentAlignment"].is_string()
						&& TryParseVAlign(it.extra["verticalContentAlignment"].get<std::string>(), verticalAlignment))
					{
						if (!migrateLegacyMetadata(L"VerticalContentAlignment", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(verticalAlignment)) })) return false;
					}
				}
				else if (it.type == UIClass::UI_WrapPanel)
				{
					Orientation o;
					if (it.extra.contains("orientation") && it.extra["orientation"].is_string() && TryParseOrientation(it.extra["orientation"].get<std::string>(), o))
					{
						if (!migrateLegacyMetadata(L"Orientation", {
							DesignerStyleValueKind::Int,
							std::to_wstring(static_cast<int>(o)) })) return false;
					}
					if (it.extra.contains("itemWidth"))
					{
						if (!migrateLegacyMetadata(L"ItemWidth", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("itemWidth", 0.0f)) })) return false;
					}
					if (it.extra.contains("itemHeight"))
					{
						if (!migrateLegacyMetadata(L"ItemHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("itemHeight", 0.0f)) })) return false;
					}
				}
				else if (it.type == UIClass::UI_DockPanel)
				{
					if (it.extra.contains("lastChildFill"))
					{
						if (!migrateLegacyMetadata(L"LastChildFill", {
							DesignerStyleValueKind::Bool,
							it.extra.value("lastChildFill", true) ? L"true" : L"false" }))
							return false;
					}
				}
				else if (it.type == UIClass::UI_ToolBar)
				{
					auto* toolBar = (ToolBar*)c;
					if (it.extra.contains("padding")
						&& !migrateLegacyMetadata(L"HorizontalPadding", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"padding", toolBar->HorizontalPadding)) })) return false;
					if (it.extra.contains("gap")
						&& !migrateLegacyMetadata(L"Gap", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("gap", toolBar->Gap)) })) return false;
					if (it.extra.contains("itemHeight")
						&& !migrateLegacyMetadata(L"ItemHeight", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"itemHeight", toolBar->ItemHeight)) })) return false;
				}
				else if (it.type == UIClass::UI_ScrollView)
				{
					auto* scrollView = (ScrollView*)c;
					if (it.extra.contains("scrollBackColor")
						&& !migrateLegacyMetadata(L"ScrollBackColor", {
							DesignerStyleValueKind::Color,
							ColorToMetadataText(ColorFromValue(
								it.extra["scrollBackColor"], scrollView->ScrollBackColor)) })) return false;
					if (it.extra.contains("scrollForeColor")
						&& !migrateLegacyMetadata(L"ScrollForeColor", {
							DesignerStyleValueKind::Color,
							ColorToMetadataText(ColorFromValue(
								it.extra["scrollForeColor"], scrollView->ScrollForeColor)) })) return false;
					if (it.extra.contains("autoContentSize")
						&& !migrateLegacyMetadata(L"AutoContentSize", {
							DesignerStyleValueKind::Bool,
							it.extra.value("autoContentSize", scrollView->AutoContentSize)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("contentSize") && it.extra["contentSize"].is_object())
					{
						auto& cs = it.extra["contentSize"];
						if (!migrateLegacyMetadata(L"ContentSize", {
							DesignerStyleValueKind::Size,
							std::to_wstring(cs.value("w", scrollView->ContentSize.cx))
								+ L", " + std::to_wstring(cs.value("h", scrollView->ContentSize.cy)) })) return false;
					}
					if (it.extra.contains("alwaysShowVScroll")
						&& !migrateLegacyMetadata(L"AlwaysShowVScroll", {
							DesignerStyleValueKind::Bool,
							it.extra.value("alwaysShowVScroll", scrollView->AlwaysShowVScroll)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("alwaysShowHScroll")
						&& !migrateLegacyMetadata(L"AlwaysShowHScroll", {
							DesignerStyleValueKind::Bool,
							it.extra.value("alwaysShowHScroll", scrollView->AlwaysShowHScroll)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("mouseWheelStep")
						&& !migrateLegacyMetadata(L"MouseWheelStep", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("mouseWheelStep", scrollView->MouseWheelStep)) })) return false;
					// Scroll offsets are observable runtime state, not design configuration.
					// Old files remain readable, but new saves intentionally omit them.
					scrollView->ScrollXOffset = it.extra.value("scrollXOffset", scrollView->ScrollXOffset);
					scrollView->ScrollYOffset = it.extra.value("scrollYOffset", scrollView->ScrollYOffset);
				}
				else if (it.type == UIClass::UI_ComboBox)
				{
					auto* comboBox = (ComboBox*)c;
					std::vector<std::wstring> items;
					if (it.extra.contains("items") && it.extra["items"].is_array())
					{
						for (auto& sj : it.extra["items"])
							if (sj.is_string()) items.push_back(FromUtf8(sj.get<std::string>()));
					}
					comboBox->Items = items;
					if (it.extra.contains("expandCount")
						&& !migrateLegacyMetadata(L"ExpandCount", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"expandCount", comboBox->ExpandCount)) })) return false;
					if (it.extra.contains("selectedIndex")
						&& !migrateLegacyMetadata(L"SelectedIndex", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"selectedIndex", comboBox->SelectedIndex)) })) return false;
				}
			else if (it.type == UIClass::UI_ListView || it.type == UIClass::UI_ListBox)
			{
				auto* listView = (ListView*)c;
				if (it.extra.contains("viewMode")
					&& !migrateLegacyMetadata(L"ViewMode", {
						DesignerStyleValueKind::Int,
						std::to_wstring(it.extra.value(
							"viewMode", static_cast<int>(listView->ViewMode))) })) return false;
				if (it.extra.contains("selectionMode")
					&& !migrateLegacyMetadata(L"SelectionMode", {
						DesignerStyleValueKind::Int,
						std::to_wstring(it.extra.value(
							"selectionMode", static_cast<int>(listView->SelectionMode))) })) return false;
				if (it.extra.contains("showCheckBoxes")
					&& !migrateLegacyMetadata(L"ShowCheckBoxes", {
						DesignerStyleValueKind::Bool,
						it.extra.value("showCheckBoxes", listView->ShowCheckBoxes)
							? L"true" : L"false" })) return false;
				if (it.extra.contains("showColumnHeaders")
					&& !migrateLegacyMetadata(L"ShowColumnHeaders", {
						DesignerStyleValueKind::Bool,
						it.extra.value("showColumnHeaders", listView->ShowColumnHeaders)
							? L"true" : L"false" })) return false;
				if (it.extra.contains("alternatingRows")
					&& !migrateLegacyMetadata(L"AlternatingRows", {
						DesignerStyleValueKind::Bool,
						it.extra.value("alternatingRows", listView->AlternatingRows)
							? L"true" : L"false" })) return false;
				if (it.extra.contains("rowHeight")
					&& !migrateLegacyMetadata(L"RowHeight", {
						DesignerStyleValueKind::Float,
						std::to_wstring(it.extra.value("rowHeight", listView->RowHeight)) })) return false;
				if (it.extra.contains("tileHeight")
					&& !migrateLegacyMetadata(L"TileHeight", {
						DesignerStyleValueKind::Float,
						std::to_wstring(it.extra.value("tileHeight", listView->TileHeight)) })) return false;
				if (it.extra.contains("iconSize")
					&& !migrateLegacyMetadata(L"IconSize", {
						DesignerStyleValueKind::Float,
						std::to_wstring(it.extra.value("iconSize", listView->IconSize)) })) return false;
				if (it.extra.contains("selectedItemBackColor")
					&& !migrateLegacyMetadata(L"SelectedItemBackColor", {
						DesignerStyleValueKind::Color,
						ColorToMetadataText(ColorFromValue(
							it.extra["selectedItemBackColor"], listView->SelectedItemBackColor)) })) return false;
				if (it.extra.contains("underMouseItemBackColor")
					&& !migrateLegacyMetadata(L"UnderMouseItemBackColor", {
						DesignerStyleValueKind::Color,
						ColorToMetadataText(ColorFromValue(
							it.extra["underMouseItemBackColor"], listView->UnderMouseItemBackColor)) })) return false;
				if (it.extra.contains("selectedItemForeColor")
					&& !migrateLegacyMetadata(L"SelectedItemForeColor", {
						DesignerStyleValueKind::Color,
						ColorToMetadataText(ColorFromValue(
							it.extra["selectedItemForeColor"], listView->SelectedItemForeColor)) })) return false;
				listView->ClearColumns();
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& cj : it.extra["columns"])
						{
							if (!cj.is_object()) continue;
							ListViewColumn col;
							col.Header = FromUtf8(cj.value("header", std::string()));
							col.Width = cj.value("width", col.Width);
							col.Align = (ListViewCellAlign)cj.value("align", (int)col.Align);
							listView->Columns.push_back(col);
						}
					}
				std::vector<ListViewItem> items;
				if (it.extra.contains("items"))
					ValueToListViewItems(it.extra["items"], items);
				listView->SetItems(std::move(items));
			}
				else if (it.type == UIClass::UI_GridView)
				{
					auto* gridView = (GridView*)c;
					auto update = gridView->DeferUpdates();
					gridView->ClearColumns();
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& cj : it.extra["columns"])
						{
							if (!cj.is_object()) continue;
							GridViewColumn col;
							col.Name = FromUtf8(cj.value("name", std::string()));
							col.Width = cj.value("width", col.Width);
							col.Type = (ColumnType)cj.value("type", (int)col.Type);
							col.CanEdit = cj.value("canEdit", col.CanEdit);
							col.ButtonText = FromUtf8(cj.value("buttonText", std::string()));
							if (cj.contains("comboBoxItems") && cj["comboBoxItems"].is_array())
							{
								for (const auto& item : cj["comboBoxItems"])
								{
									if (item.is_string())
										col.ComboBoxItems.push_back(FromUtf8(item.get<std::string>()));
								}
							}
							gridView->AddColumn(col);
						}
					}
				}
				else if (it.type == UIClass::UI_PropertyGrid)
				{
					auto* pg = (PropertyGridView*)c;
					if (it.extra.contains("showHeader")
						&& !migrateLegacyMetadata(L"ShowHeader", {
							DesignerStyleValueKind::Bool,
							it.extra.value("showHeader", pg->ShowHeader) ? L"true" : L"false" })) return false;
					if (it.extra.contains("showCategories")
						&& !migrateLegacyMetadata(L"ShowCategories", {
							DesignerStyleValueKind::Bool,
							it.extra.value("showCategories", pg->ShowCategories) ? L"true" : L"false" })) return false;
					if (it.extra.contains("alternatingRows")
						&& !migrateLegacyMetadata(L"AlternatingRows", {
							DesignerStyleValueKind::Bool,
							it.extra.value("alternatingRows", pg->AlternatingRows) ? L"true" : L"false" })) return false;
					if (it.extra.contains("allowEditing")
						&& !migrateLegacyMetadata(L"AllowEditing", {
							DesignerStyleValueKind::Bool,
							it.extra.value("allowEditing", pg->AllowEditing) ? L"true" : L"false" })) return false;
					if (it.extra.contains("rowHeight")
						&& !migrateLegacyMetadata(L"RowHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("rowHeight", pg->RowHeight)) })) return false;
					if (it.extra.contains("categoryHeight")
						&& !migrateLegacyMetadata(L"CategoryHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("categoryHeight", pg->CategoryHeight)) })) return false;
					if (it.extra.contains("nameColumnWidth")
						&& !migrateLegacyMetadata(L"NameColumnWidth", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("nameColumnWidth", pg->NameColumnWidth)) })) return false;
					std::vector<PropertyGridItem> items;
					if (it.extra.contains("items"))
						ValueToPropertyGridItems(it.extra["items"], items);
					pg->SetItems(std::move(items));
				}
				else if (it.type == UIClass::UI_TreeView)
				{
					auto* treeView = (TreeView*)c;
					if (treeView->Root)
					{
						for (auto node : treeView->Root->Children) delete node;
						treeView->Root->Children.clear();
						if (it.extra.contains("nodes"))
							ValueToTreeNodes(it.extra["nodes"], treeView->Root->Children);
					}
					treeView->SelectedBackColor = ColorFromValue(it.extra.contains("selectedBackColor") ? it.extra["selectedBackColor"] : DesignValue(), treeView->SelectedBackColor);
					treeView->UnderMouseItemBackColor = ColorFromValue(it.extra.contains("underMouseItemBackColor") ? it.extra["underMouseItemBackColor"] : DesignValue(), treeView->UnderMouseItemBackColor);
					treeView->SelectedForeColor = ColorFromValue(it.extra.contains("selectedForeColor") ? it.extra["selectedForeColor"] : DesignValue(), treeView->SelectedForeColor);
				}
				else if (it.type == UIClass::UI_ProgressBar)
				{
					((ProgressBar*)c)->PercentageValue = it.extra.value("percentageValue", ((ProgressBar*)c)->PercentageValue);
				}
				else if (it.type == UIClass::UI_LoadingRing)
				{
					((LoadingRing*)c)->Active = it.extra.value("active", ((LoadingRing*)c)->Active);
				}
				else if (it.type == UIClass::UI_ProgressRing)
				{
					auto* progressRing = (ProgressRing*)c;
					progressRing->PercentageValue = it.extra.value("percentageValue", progressRing->PercentageValue);
					progressRing->ShowPercentage = it.extra.value("showPercentage", progressRing->ShowPercentage);
				}
				else if (it.type == UIClass::UI_DateTimePicker)
				{
					auto* dateTimePicker = (DateTimePicker*)c;
					if (it.extra.contains("value") && it.extra["value"].is_object())
					{
						SYSTEMTIME st = dateTimePicker->Value;
						auto& v = it.extra["value"];
						st.wYear = (WORD)v.value("year", (int)st.wYear);
						st.wMonth = (WORD)v.value("month", (int)st.wMonth);
						st.wDay = (WORD)v.value("day", (int)st.wDay);
						st.wHour = (WORD)v.value("hour", (int)st.wHour);
						st.wMinute = (WORD)v.value("minute", (int)st.wMinute);
						st.wSecond = (WORD)v.value("second", (int)st.wSecond);
						st.wMilliseconds = (WORD)v.value("milliseconds", (int)st.wMilliseconds);
						dateTimePicker->Value = st;
					}
					dateTimePicker->Mode = (DateTimePickerMode)it.extra.value("mode", (int)dateTimePicker->Mode);
					dateTimePicker->AllowDateSelection = it.extra.value("allowDateSelection", dateTimePicker->AllowDateSelection);
					dateTimePicker->AllowTimeSelection = it.extra.value("allowTimeSelection", dateTimePicker->AllowTimeSelection);
					dateTimePicker->AllowModeSwitch = it.extra.value("allowModeSwitch", dateTimePicker->AllowModeSwitch);
					dateTimePicker->SetExpanded(it.extra.value("expand", dateTimePicker->Expand));
				}
				else if (it.type == UIClass::UI_NumericUpDown)
				{
					auto* numericUpDown = (NumericUpDown*)c;
					if (it.extra.contains("min")
						&& !migrateLegacyMetadata(L"Min", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("min", numericUpDown->Min)) })) return false;
					if (it.extra.contains("max")
						&& !migrateLegacyMetadata(L"Max", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("max", numericUpDown->Max)) })) return false;
					if (it.extra.contains("step")
						&& !migrateLegacyMetadata(L"Step", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("step", numericUpDown->Step)) })) return false;
					if (it.extra.contains("decimalPlaces")
						&& !migrateLegacyMetadata(L"DecimalPlaces", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("decimalPlaces", numericUpDown->DecimalPlaces)) })) return false;
					if (it.extra.contains("snapToStep")
						&& !migrateLegacyMetadata(L"SnapToStep", {
							DesignerStyleValueKind::Bool,
							it.extra.value("snapToStep", numericUpDown->SnapToStep)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("useMouseWheel")
						&& !migrateLegacyMetadata(L"UseMouseWheel", {
							DesignerStyleValueKind::Bool,
							it.extra.value("useMouseWheel", numericUpDown->UseMouseWheel)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("value")
						&& !migrateLegacyMetadata(L"Value", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("value", numericUpDown->Value)) })) return false;
				}
				else if (it.type == UIClass::UI_GroupBox)
				{
					auto* groupBox = (GroupBox*)c;
					if (it.extra.contains("captionMarginLeft")
						&& !migrateLegacyMetadata(L"CaptionMarginLeft", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("captionMarginLeft", (double)groupBox->CaptionMarginLeft)) })) return false;
					if (it.extra.contains("captionPaddingX")
						&& !migrateLegacyMetadata(L"CaptionPaddingX", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("captionPaddingX", (double)groupBox->CaptionPaddingX)) })) return false;
					if (it.extra.contains("captionPaddingY")
						&& !migrateLegacyMetadata(L"CaptionPaddingY", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("captionPaddingY", (double)groupBox->CaptionPaddingY)) })) return false;
				}
				else if (it.type == UIClass::UI_Expander)
				{
					auto* expander = (Expander*)c;
					if (it.extra.contains("headerHeight")
						&& !migrateLegacyMetadata(L"HeaderHeight", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("headerHeight", (double)expander->HeaderHeight)) })) return false;
					if (it.extra.contains("animationDurationMs")
						&& !migrateLegacyMetadata(L"AnimationDurationMs", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("animationDurationMs", (int)expander->AnimationDurationMs)) })) return false;
					if (it.extra.contains("isExpanded")
						&& !migrateLegacyMetadata(L"IsExpanded", {
							DesignerStyleValueKind::Bool,
							it.extra.value("isExpanded", expander->IsExpanded)
								? L"true" : L"false" })) return false;
				}
				else if (it.type == UIClass::UI_SplitContainer)
				{
					Orientation orientation = Orientation::Horizontal;
					if (it.extra.contains("splitOrientation") && it.extra["splitOrientation"].is_string())
					{
						if (TryParseOrientation(
							it.extra["splitOrientation"].get<std::string>(), orientation))
						{
							if (!migrateLegacyMetadata(L"SplitOrientation", {
								DesignerStyleValueKind::Int,
								std::to_wstring(static_cast<int>(orientation)) })) return false;
						}
					}
					if (it.extra.contains("splitterWidth"))
					{
						if (!migrateLegacyMetadata(L"SplitterWidth", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("splitterWidth", 6)) })) return false;
					}
					if (it.extra.contains("panel1MinSize"))
					{
						if (!migrateLegacyMetadata(L"Panel1MinSize", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("panel1MinSize", 48)) })) return false;
					}
					if (it.extra.contains("panel2MinSize"))
					{
						if (!migrateLegacyMetadata(L"Panel2MinSize", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("panel2MinSize", 48)) })) return false;
					}
					if (it.extra.contains("splitterDistance"))
					{
						if (!migrateLegacyMetadata(L"SplitterDistance", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value("splitterDistance", 160)) })) return false;
					}
					if (it.extra.contains("isSplitterFixed"))
					{
						if (!migrateLegacyMetadata(L"IsSplitterFixed", {
							DesignerStyleValueKind::Bool,
							it.extra.value("isSplitterFixed", false) ? L"true" : L"false" }))
							return false;
					}
				}
				else if (it.type == UIClass::UI_Slider)
				{
					auto* slider = (Slider*)c;
					if (it.extra.contains("min")
						&& !migrateLegacyMetadata(L"Min", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("min", slider->Min)) })) return false;
					if (it.extra.contains("max")
						&& !migrateLegacyMetadata(L"Max", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("max", slider->Max)) })) return false;
					if (it.extra.contains("step")
						&& !migrateLegacyMetadata(L"Step", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("step", slider->Step)) })) return false;
					if (it.extra.contains("snapToStep")
						&& !migrateLegacyMetadata(L"SnapToStep", {
							DesignerStyleValueKind::Bool,
							it.extra.value("snapToStep", slider->SnapToStep)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("value")
						&& !migrateLegacyMetadata(L"Value", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value("value", slider->Value)) })) return false;
				}
				else if (it.type == UIClass::UI_StatusBar)
				{
					auto* statusBar = (StatusBar*)c;
					if (it.extra.contains("topMost")
						&& !migrateLegacyMetadata(L"TopMost", {
							DesignerStyleValueKind::Bool,
							it.extra.value("topMost", statusBar->TopMost)
								? L"true" : L"false" })) return false;
					statusBar->ClearParts();
					if (it.extra.contains("parts") && it.extra["parts"].is_array())
					{
						for (auto& pj : it.extra["parts"])
						{
							if (!pj.is_object()) continue;
							std::wstring text = FromUtf8(pj.value("text", std::string()));
							int w = pj.value("width", 0);
							statusBar->AddPart(text, w);
						}
					}
				}
				else if (it.type == UIClass::UI_MediaPlayer)
				{
					auto* mediaPlayer = (MediaPlayer*)c;
					// 旧文档标量迁移到统一元数据；新文档只在 extra 保留媒体源路径。
					if (it.extra.contains("autoPlay")
						&& !migrateLegacyMetadata(L"AutoPlay", {
							DesignerStyleValueKind::Bool,
							it.extra.value("autoPlay", mediaPlayer->AutoPlay)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("loop")
						&& !migrateLegacyMetadata(L"Loop", {
							DesignerStyleValueKind::Bool,
							it.extra.value("loop", mediaPlayer->Loop)
								? L"true" : L"false" })) return false;
					if (it.extra.contains("volume")
						&& !migrateLegacyMetadata(L"Volume", {
							DesignerStyleValueKind::Double,
							std::to_wstring(it.extra.value("volume", mediaPlayer->Volume)) })) return false;
					if (it.extra.contains("playbackRate")
						&& !migrateLegacyMetadata(L"PlaybackRate", {
							DesignerStyleValueKind::Float,
							std::to_wstring(it.extra.value(
								"playbackRate", (double)mediaPlayer->PlaybackRate)) })) return false;
					if (it.extra.contains("renderMode")
						&& !migrateLegacyMetadata(L"RenderMode", {
							DesignerStyleValueKind::Int,
							std::to_wstring(it.extra.value(
								"renderMode", (int)mediaPlayer->RenderMode)) })) return false;
					if (it.extra.contains("mediaFile") && it.extra["mediaFile"].is_string())
						dc->DesignStrings[L"mediaFile"] = FromUtf8(it.extra["mediaFile"].get<std::string>());
					else
						dc->DesignStrings.erase(L"mediaFile");
				}
				else if (it.type == UIClass::UI_Menu)
				{
					auto* m = (Menu*)c;
					// 清空现有顶层项
					while (m->Count > 0)
					{
						auto* cc = m->operator[](m->Count - 1);
						m->DeleteControl(cc);
					}
					if (it.extra.contains("items") && it.extra["items"].is_array())
					{
						for (auto& ij : it.extra["items"])
						{
							if (!ij.is_object()) continue;
							bool sep = ij.value("separator", false);
							if (sep) continue; // 顶层不支持 separator
							auto text = FromUtf8(ij.value("text", std::string()));
							if (text.empty()) continue;
							auto* top = m->AddItem(text);
							if (!top) continue;
							top->Id = ij.value("id", 0);
							top->Shortcut = FromUtf8(ij.value("shortcut", std::string()));
							top->Enable = ij.value("enable", true);
							if (ij.contains("subItems"))
							{
								std::vector<MenuItem*> subItems;
								ValueToMenuSubItems(ij["subItems"], subItems, top);
							}
						}
					}
				}
			}
		}

		std::unordered_map<std::wstring, std::vector<Pending*>> childrenByParent;
		childrenByParent.reserve(items.size());
		std::vector<Pending*> roots;
		roots.reserve(items.size());
		for (auto& it : items)
		{
			if (it.parent.empty())
			{
				roots.push_back(&it);
				continue;
			}
			childrenByParent[it.parent].push_back(&it);
		}

		auto sortByOrder = [](std::vector<Pending*>& v) {
			std::stable_sort(v.begin(), v.end(), [](const Pending* a, const Pending* b) {
				return a->order < b->order;
			});
		};
		sortByOrder(roots);
		for (auto& kv : childrenByParent) sortByOrder(kv.second);

		std::unordered_set<std::wstring> attached;
		attached.reserve(items.size());

		auto attachOne = [&](Pending* it, Control* runtimeParent, Control* designerParent)
		{
			if (!it) return;
			auto dc = dcOf[it->name];
			if (!dc || !dc->ControlInstance) return;
			auto* c = dc->ControlInstance;
			if (!runtimeParent) runtimeParent = &stagingRoot;
			if (!runtimeParent) return;
			auto owner = controlPool.TakeById(it->id);
			if (!owner || owner.get() != c) return;
			if (runtimeParent->Type() == UIClass::UI_ToolBar)
			{
				((ToolBar*)runtimeParent)->AddOwned(
					std::move(owner));
			}
			else
			{
				runtimeParent->AddOwned(std::move(owner));
			}
			dc->DesignerParent = designerParent;
			candidate.Controls.push_back(dc);
			attached.insert(it->name);
		};

		std::function<void(const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)> attachChildren;
		attachChildren = [&](const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)
		{
			auto it = childrenByParent.find(parentKey);
			if (it == childrenByParent.end()) return;
			if (auto* split = AsSplitContainer(runtimeParent))
			{
				std::vector<Pending*> firstChildren;
				std::vector<Pending*> secondChildren;
				for (auto* ch : it->second)
				{
					std::string region = ch->extra.value("splitRegion", std::string("panel1"));
					if (region == "panel2") secondChildren.push_back(ch);
					else firstChildren.push_back(ch);
				}
				sortByOrder(firstChildren);
				sortByOrder(secondChildren);
				for (auto* ch : firstChildren)
				{
					attachOne(ch, split->FirstPanel(), runtimeParent);
					attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				}
				for (auto* ch : secondChildren)
				{
					attachOne(ch, split->SecondPanel(), runtimeParent);
					attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				}
				return;
			}
			for (auto* ch : it->second)
			{
				attachOne(ch, runtimeParent, designerParent);
				attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				if (ch->type == UIClass::UI_TabControl)
				{
					auto* tabControl = (TabControl*)dcOf[ch->name]->ControlInstance;
					(void)tabControl;
					for (auto& kv : tabPageOf)
					{
						std::wstring prefix = ch->name + L"#page";
						if (kv.first.rfind(prefix, 0) != 0) continue;
						attachChildren(kv.first, kv.second, kv.second);
					}
				}
			}
		};

		for (auto* it : roots)
		{
			attachOne(it, &stagingRoot, nullptr);
			attachChildren(it->name, dcOf[it->name]->ControlInstance, dcOf[it->name]->ControlInstance);
			if (it->type == UIClass::UI_TabControl)
			{
				for (auto& kv : tabPageOf)
				{
					std::wstring prefix = it->name + L"#page";
					if (kv.first.rfind(prefix, 0) != 0) continue;
					attachChildren(kv.first, kv.second, kv.second);
				}
			}
		}

		if (attached.size() != items.size()
			|| controlPool.PendingCount() != 0)
		{
			for (auto& it : items)
			{
				if (attached.find(it.name) == attached.end())
				{
					if (outError) *outError = L"无法解析控件父级引用，未能挂载控件: " + it.name;
					return false;
				}
			}
		}

		std::shared_ptr<ControlStyleSheet> runtimeStyleSheet;
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			document.StyleSheet, runtimeStyleSheet, outError))
			return false;
		if (!document.StyleSheet.Empty()
			&& !stagingRoot.SetStyleSheet(runtimeStyleSheet, true))
		{
			if (outError) *outError =
				L"文档样式表无法应用到完整控件树。";
			return false;
		}

		for (auto& dc : candidate.Controls)
		{
			if (!dc || !dc->ControlInstance) continue;
			RefreshDesignerPanelLayout(dc->ControlInstance);
		}
		while (stagingRoot.Count > 0)
		{
			auto root = stagingRoot.DetachControlAt(0);
			if (!root)
			{
				if (outError) *outError =
					L"材质化完成后无法分离根控件。";
				return false;
			}
			candidate.Roots.push_back(std::move(root));
		}

		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& expander)
	{
		if (outError) *outError = L"加载失败: " + FromUtf8(expander.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"加载失败：未知错误。";
		return false;
	}
}
