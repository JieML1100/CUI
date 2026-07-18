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
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
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

	std::wstring PublicPropertyName(const std::wstring& property)
	{
		if (Equals(property, L"Left")) return L"Canvas.Left";
		if (Equals(property, L"Top")) return L"Canvas.Top";
		if (Equals(property, L"LayoutWidth")) return L"Width";
		if (Equals(property, L"LayoutHeight")) return L"Height";
		if (Equals(property, L"HAlign")) return L"HorizontalAlignment";
		if (Equals(property, L"VAlign")) return L"VerticalAlignment";
		if (Equals(property, L"DockPosition")) return L"DockPanel.Dock";
		if (Equals(property, L"GridRow")) return L"Grid.Row";
		if (Equals(property, L"GridColumn")) return L"Grid.Column";
		if (Equals(property, L"GridRowSpan")) return L"Grid.RowSpan";
		if (Equals(property, L"GridColumnSpan")) return L"Grid.ColumnSpan";
		return property;
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

	std::wstring ColorText(double r, double g, double b, double a)
	{
		auto channel = [](double value)
		{
			return static_cast<unsigned int>(std::lround(
				(std::clamp)(value, 0.0, 1.0) * 255.0));
		};
		wchar_t text[10]{};
		swprintf_s(text, L"#%02X%02X%02X%02X",
			channel(a), channel(r), channel(g), channel(b));
		return text;
	}

	std::wstring PublicPropertyValue(
		UIClass type,
		const std::wstring& property,
		const std::wstring& value)
	{
		auto probe = DesignDocumentMaterializer::CreateRuntimeControl(type);
		if (!probe) return value;
		const auto descriptors = DesignerPropertyCatalog::GetStyleProperties(*probe);
		const auto* descriptor = DesignerPropertyCatalog::Find(descriptors, property);
		if (!descriptor) return value;
		for (const auto& choice : descriptor->Choices)
			if (Equals(choice.ValueText, value)) return choice.DisplayName;
		return value;
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

	std::wstring EnumText(
		int value,
		std::initializer_list<const wchar_t*> names)
	{
		if (value >= 0 && static_cast<size_t>(value) < names.size())
			return *(names.begin() + value);
		return std::to_wstring(value);
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
					_document.StyleSheet, &error,
					_document.ResourceBasePath, _document.Resources)
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
			const DesignFormModel defaults;
			Set(root, "x:Name", _document.Form.Name);
			if (!_document.CodeBehind.ClassName.empty())
				Set(root, "x:Class", _document.CodeBehind.ClassName);
			if (!_document.CodeBehind.RelativeBasePath.empty())
				Set(root, "d:CodeBehind",
					_document.CodeBehind.RelativeBasePath);
			if (_document.Form.Text != defaults.Text)
				Set(root, "Text", _document.Form.Text);
			if (_document.Form.Location.x != defaults.Location.x)
				Set(root, "X", std::to_wstring(_document.Form.Location.x));
			if (_document.Form.Location.y != defaults.Location.y)
				Set(root, "Y", std::to_wstring(_document.Form.Location.y));
			if (_document.Form.Size.cx != defaults.Size.cx)
				Set(root, "Width", std::to_wstring(_document.Form.Size.cx));
			if (_document.Form.Size.cy != defaults.Size.cy)
				Set(root, "Height", std::to_wstring(_document.Form.Size.cy));
			if (!_document.Form.FontName.empty())
				Set(root, "FontName", _document.Form.FontName);
			if (_document.Form.FontSize != defaults.FontSize)
				Set(root, "FontSize", NumberText(_document.Form.FontSize));
			if (ColorText(_document.Form.BackColor) != ColorText(defaults.BackColor))
				Set(root, "BackColor", ColorText(_document.Form.BackColor));
			if (ColorText(_document.Form.ForeColor) != ColorText(defaults.ForeColor))
				Set(root, "ForeColor", ColorText(_document.Form.ForeColor));
			auto setBool = [&](const char* name, bool value, bool defaultValue)
			{
				if (value != defaultValue) root->SetAttribute(name, BoolText(value));
			};
			setBool("ShowInTaskBar", _document.Form.ShowInTaskBar, defaults.ShowInTaskBar);
			setBool("TopMost", _document.Form.TopMost, defaults.TopMost);
			setBool("Enable", _document.Form.Enable, defaults.Enable);
			setBool("Visible", _document.Form.Visible, defaults.Visible);
			setBool("VisibleHead", _document.Form.VisibleHead, defaults.VisibleHead);
			if (_document.Form.HeadHeight != defaults.HeadHeight)
				root->SetAttribute("HeadHeight", std::to_string(_document.Form.HeadHeight));
			setBool("MinBox", _document.Form.MinBox, defaults.MinBox);
			setBool("MaxBox", _document.Form.MaxBox, defaults.MaxBox);
			setBool("CloseBox", _document.Form.CloseBox, defaults.CloseBox);
			setBool("CenterTitle", _document.Form.CenterTitle, defaults.CenterTitle);
			setBool("AllowResize", _document.Form.AllowResize, defaults.AllowResize);
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

		Element WriteBrush(
			const Element& parent,
			const DesignValue& value,
			const std::wstring& resourceKey = {})
		{
			if (!value.is_object())
				throw std::invalid_argument("Brush value must be an object");
			const auto type = value.value("type", std::string{});
			const char* brushName = nullptr;
			if (type == "solid") brushName = "SolidColorBrush";
			else if (type == "linear") brushName = "LinearGradientBrush";
			else if (type == "radial") brushName = "RadialGradientBrush";
			else if (type == "image") brushName = "ImageBrush";
			else throw std::invalid_argument("Brush type is invalid");
			auto brush = Append(_xml, parent, brushName);
			if (!resourceKey.empty()) Set(brush, "x:Key", resourceKey);
			if (type == "solid")
			{
				if (!value.contains("color"))
					throw std::invalid_argument("SolidColorBrush requires Color");
				const auto color = ColorText(value["color"]);
				if (!color) throw std::invalid_argument("SolidColorBrush Color is invalid");
				Set(brush, "Color", *color);
			}
			else if (type == "image")
			{
				const auto source = value.value("source", std::string{});
				if (source.empty())
					throw std::invalid_argument("ImageBrush requires ImageSource");
				brush->SetAttribute("ImageSource", source.c_str());
				const auto stretch = value.value("stretch", std::string("fill"));
				if (stretch == "none") Set(brush, "Stretch", L"None");
				else if (stretch == "uniform") Set(brush, "Stretch", L"Uniform");
				else if (stretch == "uniformToFill") Set(brush, "Stretch", L"UniformToFill");
				else if (stretch != "fill")
					throw std::invalid_argument("ImageBrush Stretch is invalid");
				const auto alignmentX = value.value("alignmentX", std::string("center"));
				if (alignmentX == "left") Set(brush, "AlignmentX", L"Left");
				else if (alignmentX == "right") Set(brush, "AlignmentX", L"Right");
				else if (alignmentX != "center")
					throw std::invalid_argument("ImageBrush AlignmentX is invalid");
				const auto alignmentY = value.value("alignmentY", std::string("center"));
				if (alignmentY == "top") Set(brush, "AlignmentY", L"Top");
				else if (alignmentY == "bottom") Set(brush, "AlignmentY", L"Bottom");
				else if (alignmentY != "center")
					throw std::invalid_argument("ImageBrush AlignmentY is invalid");
			}
			else
			{
				const auto mapping = value.value("mapping", std::string("relative"));
				if (mapping == "absolute") Set(brush, "MappingMode", L"Absolute");
				else if (mapping != "relative")
					throw std::invalid_argument("Gradient brush MappingMode is invalid");
				if (type == "linear")
				{
					Set(brush, "StartPoint", NumberText(value.value("startX", 0.0))
						+ L"," + NumberText(value.value("startY", 0.0)));
					Set(brush, "EndPoint", NumberText(value.value("endX", 1.0))
						+ L"," + NumberText(value.value("endY", 1.0)));
				}
				else
				{
					Set(brush, "Center", NumberText(value.value("centerX", 0.5))
						+ L"," + NumberText(value.value("centerY", 0.5)));
					Set(brush, "GradientOrigin", NumberText(value.value("originX", 0.5))
						+ L"," + NumberText(value.value("originY", 0.5)));
					Set(brush, "RadiusX", NumberText(value.value("radiusX", 0.5)));
					Set(brush, "RadiusY", NumberText(value.value("radiusY", 0.5)));
				}
				if (!value.contains("stops") || !value["stops"].is_array()
					|| value["stops"].size() < 2)
					throw std::invalid_argument("Gradient brush requires at least two stops");
				for (const auto& stopValue : value["stops"])
				{
					if (!stopValue.is_object() || !stopValue.contains("color"))
						throw std::invalid_argument("GradientStop is invalid");
					const auto color = ColorText(stopValue["color"]);
					if (!color) throw std::invalid_argument("GradientStop Color is invalid");
					auto stop = Append(_xml, brush, "GradientStop");
					Set(stop, "Color", *color);
					Set(stop, "Offset", NumberText(stopValue.value("offset", 0.0)));
				}
			}
			const auto opacity = value.value("opacity", 1.0);
			if (opacity != 1.0) Set(brush, "Opacity", NumberText(opacity));
			return brush;
		}

		Element WriteTransformOperation(
			const Element& parent,
			const DesignValue& value)
		{
			if (!value.is_object())
				throw std::invalid_argument("Transform operation must be an object");
			const auto type = value.value("type", std::string{});
			Element operation;
			if (type == "matrix")
			{
				operation = Append(_xml, parent, "MatrixTransform");
				Set(operation, "Matrix",
					NumberText(value.value("m11", 1.0)) + L"," +
					NumberText(value.value("m12", 0.0)) + L"," +
					NumberText(value.value("m21", 0.0)) + L"," +
					NumberText(value.value("m22", 1.0)) + L"," +
					NumberText(value.value("dx", 0.0)) + L"," +
					NumberText(value.value("dy", 0.0)));
			}
			else if (type == "translate")
			{
				operation = Append(_xml, parent, "TranslateTransform");
				const auto x = value.value("x", 0.0);
				const auto y = value.value("y", 0.0);
				if (x != 0.0) Set(operation, "X", NumberText(x));
				if (y != 0.0) Set(operation, "Y", NumberText(y));
			}
			else if (type == "scale")
			{
				operation = Append(_xml, parent, "ScaleTransform");
				const auto x = value.value("scaleX", 1.0);
				const auto y = value.value("scaleY", 1.0);
				if (x != 1.0) Set(operation, "ScaleX", NumberText(x));
				if (y != 1.0) Set(operation, "ScaleY", NumberText(y));
			}
			else if (type == "rotate")
			{
				operation = Append(_xml, parent, "RotateTransform");
				const auto angle = value.value("angle", 0.0);
				if (angle != 0.0) Set(operation, "Angle", NumberText(angle));
			}
			else if (type == "skew")
			{
				operation = Append(_xml, parent, "SkewTransform");
				const auto x = value.value("angleX", 0.0);
				const auto y = value.value("angleY", 0.0);
				if (x != 0.0) Set(operation, "AngleX", NumberText(x));
				if (y != 0.0) Set(operation, "AngleY", NumberText(y));
			}
			else throw std::invalid_argument("Transform operation type is invalid");
			if (type == "scale" || type == "rotate" || type == "skew")
			{
				const auto centerX = value.value("centerX", 0.0);
				const auto centerY = value.value("centerY", 0.0);
				if (centerX != 0.0) Set(operation, "CenterX", NumberText(centerX));
				if (centerY != 0.0) Set(operation, "CenterY", NumberText(centerY));
			}
			return operation;
		}

		Element WriteTransformObject(
			const Element& parent,
			const DesignValue& values,
			const std::wstring& resourceKey = {})
		{
			if (!values.is_array() || values.empty())
				throw std::invalid_argument("Transform must be a non-empty array");
			Element transform;
			if (values.size() == 1)
				transform = WriteTransformOperation(parent, values[size_t{ 0 }]);
			else
			{
				transform = Append(_xml, parent, "TransformGroup");
				for (const auto& value : values.ArrayItems())
					WriteTransformOperation(transform, value);
			}
			if (!resourceKey.empty()) Set(transform, "x:Key", resourceKey);
			return transform;
		}

		Element WriteGeometryObject(
			const Element& parent,
			const DesignValue& value,
			const std::wstring& resourceKey = {})
		{
			if (!value.is_object())
				throw std::invalid_argument("Geometry must be an object");
			const auto type = value.value("type", std::string{});
			Element geometry;
			if (type == "rectangle")
			{
				geometry = Append(_xml, parent, "RectangleGeometry");
				Set(geometry, "Rect",
					NumberText(value.value("x", 0.0)) + L","
					+ NumberText(value.value("y", 0.0)) + L","
					+ NumberText(value.value("width", 0.0)) + L","
					+ NumberText(value.value("height", 0.0)));
				const auto radiusX = value.value("radiusX", 0.0);
				const auto radiusY = value.value("radiusY", 0.0);
				if (radiusX != 0.0) Set(geometry, "RadiusX", NumberText(radiusX));
				if (radiusY != 0.0) Set(geometry, "RadiusY", NumberText(radiusY));
			}
			else if (type == "ellipse")
			{
				geometry = Append(_xml, parent, "EllipseGeometry");
				Set(geometry, "Center",
					NumberText(value.value("centerX", 0.0)) + L","
					+ NumberText(value.value("centerY", 0.0)));
				Set(geometry, "RadiusX", NumberText(value.value("radiusX", 0.0)));
				Set(geometry, "RadiusY", NumberText(value.value("radiusY", 0.0)));
			}
			else if (type == "path")
			{
				geometry = Append(_xml, parent, "PathGeometry");
				const auto fillRule = value.value("fillRule", std::string("evenodd"));
				if (fillRule == "nonzero") Set(geometry, "FillRule", L"Nonzero");
				else if (fillRule != "evenodd")
					throw std::invalid_argument("PathGeometry FillRule is invalid");
				if (!value.contains("figures") || !value["figures"].is_array())
					throw std::invalid_argument("PathGeometry Figures must be an array");
				for (const auto& figureValue : value["figures"].ArrayItems())
				{
					if (!figureValue.is_object())
						throw std::invalid_argument("PathFigure must be an object");
					auto figure = Append(_xml, geometry, "PathFigure");
					Set(figure, "StartPoint",
						NumberText(figureValue.value("startX", 0.0)) + L","
						+ NumberText(figureValue.value("startY", 0.0)));
					if (figureValue.value("closed", false))
						Set(figure, "IsClosed", L"true");
					if (!figureValue.value("filled", true))
						Set(figure, "IsFilled", L"false");
					if (!figureValue.contains("segments")
						|| !figureValue["segments"].is_array())
						throw std::invalid_argument("PathFigure Segments must be an array");
					for (const auto& segmentValue : figureValue["segments"].ArrayItems())
					{
						if (!segmentValue.is_object())
							throw std::invalid_argument("PathSegment must be an object");
						const auto segmentType = segmentValue.value("type", std::string{});
						Element segment;
						if (segmentType == "line")
						{
							segment = Append(_xml, figure, "LineSegment");
							Set(segment, "Point",
								NumberText(segmentValue.value("x", 0.0)) + L","
								+ NumberText(segmentValue.value("y", 0.0)));
						}
						else if (segmentType == "bezier" || segmentType == "quadratic")
						{
							const int count = segmentType == "bezier" ? 3 : 2;
							segment = Append(_xml, figure, segmentType == "bezier"
								? "BezierSegment" : "QuadraticBezierSegment");
							for (int point = 1; point <= count; ++point)
							{
								const auto suffix = std::to_string(point);
								Set(segment, ("Point" + suffix).c_str(),
									NumberText(segmentValue.value("x" + suffix, 0.0))
									+ L"," + NumberText(segmentValue.value(
										"y" + suffix, 0.0)));
							}
						}
						else if (segmentType == "arc")
						{
							segment = Append(_xml, figure, "ArcSegment");
							Set(segment, "Point",
								NumberText(segmentValue.value("x", 0.0)) + L","
								+ NumberText(segmentValue.value("y", 0.0)));
							Set(segment, "Size",
								NumberText(segmentValue.value("width", 0.0)) + L","
								+ NumberText(segmentValue.value("height", 0.0)));
							const auto rotation = segmentValue.value("rotation", 0.0);
							if (rotation != 0.0)
								Set(segment, "RotationAngle", NumberText(rotation));
							if (segmentValue.value("large", false))
								Set(segment, "IsLargeArc", L"true");
							const auto sweep = segmentValue.value(
								"sweep", std::string("counterclockwise"));
							if (sweep == "clockwise")
								Set(segment, "SweepDirection", L"Clockwise");
							else if (sweep != "counterclockwise")
								throw std::invalid_argument(
									"ArcSegment SweepDirection is invalid");
						}
						else throw std::invalid_argument("PathSegment type is invalid");
					}
				}
			}
			else if (type == "group")
			{
				geometry = Append(_xml, parent, "GeometryGroup");
				const auto fillRule = value.value("fillRule", std::string("evenodd"));
				if (fillRule == "nonzero") Set(geometry, "FillRule", L"Nonzero");
				else if (fillRule != "evenodd")
					throw std::invalid_argument("GeometryGroup FillRule is invalid");
				if (!value.contains("children") || !value["children"].is_array())
					throw std::invalid_argument("GeometryGroup Children must be an array");
				for (const auto& child : value["children"].ArrayItems())
					WriteGeometryObject(geometry, child);
			}
			else throw std::invalid_argument("Geometry type is invalid");
			if (!resourceKey.empty()) Set(geometry, "x:Key", resourceKey);
			if (value.contains("transform"))
			{
				auto transformProperty = Append(_xml, geometry, "Geometry.Transform");
				WriteTransformObject(transformProperty, value["transform"]);
			}
			return geometry;
		}

		void WriteStyleSetter(
			const Element& parent,
			const DesignerStyleSetter& setter)
		{
			auto item = Append(_xml, parent, "Setter");
			Set(item, "Property", setter.PropertyName);
			if (setter.UsesResource)
				Set(item, "Value", L"{StaticResource "
					+ setter.ResourceKey + L"}");
			else if (setter.Literal.Kind == DesignerStyleValueKind::Brush)
			{
				auto value = Append(_xml, item, "Setter.Value");
				WriteBrush(value, setter.Literal.ObjectValue);
			}
			else if (setter.Literal.Kind == DesignerStyleValueKind::Geometry)
			{
				auto value = Append(_xml, item, "Setter.Value");
				WriteGeometryObject(value, setter.Literal.ObjectValue);
			}
			else if (setter.Literal.Kind == DesignerStyleValueKind::Transform)
			{
				auto value = Append(_xml, item, "Setter.Value");
				WriteTransformObject(value, setter.Literal.ObjectValue);
			}
			else
			{
				Set(item, "Kind",
					DesignerStyleSheetUtils::ValueKindName(setter.Literal.Kind));
				Set(item, "Value", setter.Literal.Text);
			}
		}

		void WriteDataTrigger(
			const Element& parent,
			const std::vector<DesignerStyleDataCondition>& conditions,
			const std::vector<DesignerStyleSetter>& setters)
		{
			if (conditions.empty())
				throw std::invalid_argument("DataTrigger requires a condition");
			Element trigger;
			if (conditions.size() == 1)
			{
				trigger = Append(_xml, parent, "DataTrigger");
				Set(trigger, "Binding", L"{Binding "
					+ conditions.front().SourceProperty + L"}");
				Set(trigger, "Value", conditions.front().Value.Text);
			}
			else
			{
				trigger = Append(_xml, parent, "MultiDataTrigger");
				auto conditionList = Append(
					_xml, trigger, "MultiDataTrigger.Conditions");
				for (const auto& condition : conditions)
				{
					auto item = Append(_xml, conditionList, "Condition");
					Set(item, "Binding", L"{Binding "
						+ condition.SourceProperty + L"}");
					Set(item, "Value", condition.Value.Text);
				}
			}
			for (const auto& setter : setters)
				WriteStyleSetter(trigger, setter);
		}

		void WriteResources(const Element& root)
		{
			if (_document.StyleSheet.Empty()) return;
			auto resources = Append(_xml, root, "Form.Resources");
			auto styleSheet = _document.StyleSheet;
			DesignerStyleSheetUtils::Canonicalize(styleSheet);
			Element resourceTarget = resources;
			if (!styleSheet.MergedDictionaries.empty())
			{
				auto dictionary = Append(_xml, resources, "ResourceDictionary");
				auto merged = Append(
					_xml, dictionary, "ResourceDictionary.MergedDictionaries");
				for (const auto& source : styleSheet.MergedDictionaries)
				{
					auto imported = Append(_xml, merged, "ResourceDictionary");
					Set(imported, "Source", source);
				}
				resourceTarget = dictionary;
			}
			for (const auto& resource : styleSheet.Resources)
			{
				if (!resource.SourceDictionary.empty()) continue;
				if (resource.Value.Kind == DesignerStyleValueKind::Brush)
				{
					WriteBrush(resourceTarget, resource.Value.ObjectValue, resource.Key);
					continue;
				}
				if (resource.Value.Kind == DesignerStyleValueKind::ImageSource)
				{
					auto item = Append(_xml, resourceTarget, "BitmapImage");
					Set(item, "x:Key", resource.Key);
					Set(item, "UriSource", resource.Value.Text);
					continue;
				}
				if (resource.Value.Kind == DesignerStyleValueKind::Geometry)
				{
					WriteGeometryObject(
						resourceTarget, resource.Value.ObjectValue, resource.Key);
					continue;
				}
				if (resource.Value.Kind == DesignerStyleValueKind::Transform)
				{
					WriteTransformObject(
						resourceTarget, resource.Value.ObjectValue, resource.Key);
					continue;
				}
				auto item = Append(_xml, resourceTarget,
					ToUtf8(DesignerStyleSheetUtils::ValueKindName(
						resource.Value.Kind)));
				Set(item, "x:Key", resource.Key);
				item->SetInnerText(ToUtf8(resource.Value.Text));
			}
			for (const auto& rule : styleSheet.Rules)
			{
				if (!rule.SourceDictionary.empty()) continue;
				auto style = Append(_xml, resourceTarget, "Style");
				if (rule.HasType)
					Set(style, "TargetType",
						DesignerStyleSheetUtils::UIClassName(rule.Type));
				if (!rule.Id.empty()) Set(style, "x:Key", rule.Id);
				if (!rule.BasedOn.empty())
					Set(style, "BasedOn", L"{StaticResource "
						+ rule.BasedOn + L"}");
				if (!rule.Classes.empty())
					Set(style, "Classes",
						DesignerStyleSheetUtils::JoinClasses(rule.Classes));
				if (rule.RequiredStates != ControlStyleState::None)
					Set(style, "RequiredStates",
						DesignerStyleSheetUtils::FormatStates(rule.RequiredStates));
				if (rule.ExcludedStates != ControlStyleState::None)
					Set(style, "ExcludedStates",
						DesignerStyleSheetUtils::FormatStates(rule.ExcludedStates));
				if (rule.DataConditions.empty())
					for (const auto& setter : rule.Setters)
						WriteStyleSetter(style, setter);
				if (!rule.Triggers.empty() || !rule.DataConditions.empty())
				{
					auto triggers = Append(_xml, style, "Style.Triggers");
					if (!rule.DataConditions.empty())
						WriteDataTrigger(triggers, rule.DataConditions, rule.Setters);
					for (const auto& trigger : rule.Triggers)
					{
						if (!trigger.DataConditions.empty())
						{
							WriteDataTrigger(
								triggers, trigger.DataConditions, trigger.Setters);
							continue;
						}
						const bool multi = trigger.Conditions.size() > 1;
						auto triggerElement = Append(
							_xml, triggers, multi ? "MultiTrigger" : "Trigger");
						if (multi)
						{
							auto conditions = Append(
								_xml, triggerElement, "MultiTrigger.Conditions");
							for (const auto& condition : trigger.Conditions)
							{
								auto conditionElement = Append(_xml, conditions, "Condition");
								Set(conditionElement, "Property", condition.Property);
								Set(conditionElement, "Value",
									condition.Value ? L"true" : L"false");
							}
						}
						else if (!trigger.Conditions.empty())
						{
							Set(triggerElement, "Property", trigger.Conditions.front().Property);
							Set(triggerElement, "Value",
								trigger.Conditions.front().Value ? L"true" : L"false");
						}
						for (const auto& setter : trigger.Setters)
							WriteStyleSetter(triggerElement, setter);
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
			WriteStructuredProperties(node, element, residualExtra);
			if (consumeSplitRegion && residualExtra.is_object())
				residualExtra.ObjectItems().erase("splitRegion");
			if (!residualProps.empty())
				throw std::invalid_argument(
					"Control contains properties without a public XAML representation");
			if (!residualBindings.empty())
				throw std::invalid_argument(
					"Control contains bindings without a public XAML representation");

			if (node.Type == UIClass::UI_TabControl
				&& residualExtra.contains("pages"))
			{
				WriteTabPages(node, element, residualExtra);
			}
			if (!residualExtra.empty())
				throw std::invalid_argument(
					"Control contains structured data without a public XAML representation");

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
			auto defaultControl =
				DesignDocumentMaterializer::CreateRuntimeControl(node.Type);
			const auto defaultProperties = defaultControl
				? DesignerPropertyCatalog::GetStyleProperties(*defaultControl)
				: std::vector<DesignerPropertyDescriptor>{};
			auto isDefault = [&](const std::wstring& property,
				const std::wstring& value)
			{
				const auto* descriptor = DesignerPropertyCatalog::Find(
					defaultProperties, property);
				if (!descriptor) return false;
				auto normalized = value;
				for (const auto& choice : descriptor->Choices)
					if (Equals(choice.DisplayName, normalized))
					{
						normalized = choice.ValueText;
						break;
					}
				return normalized == descriptor->SampleValue;
			};
			auto canWriteLegacy = [&](const std::wstring& property)
			{
				return !HasBinding(node, property) && !HasMetadata(node, property);
			};
			auto consumeString = [&](const char* key, const wchar_t* attribute,
				const wchar_t* property)
			{
				if (!residual.contains(key)) return;
				if (!residual[key].is_string())
					throw std::invalid_argument("Invalid string control property");
				const auto value = FromUtf8(residual[key].get<std::string>());
				if (canWriteLegacy(property) && !isDefault(property, value))
					attributes[attribute] = value;
				residual.ObjectItems().erase(key);
			};
			auto consumeBool = [&](const char* key, const wchar_t* attribute,
				const wchar_t* property)
			{
				if (!residual.contains(key)) return;
				if (!residual[key].is_boolean())
					throw std::invalid_argument("Invalid Boolean control property");
				const auto value = residual[key].get<bool>() ? L"true" : L"false";
				if (canWriteLegacy(property) && !isDefault(property, value))
					attributes[attribute] = value;
				residual.ObjectItems().erase(key);
			};
			auto consumeNumber = [&](const char* key, const wchar_t* attribute,
				const wchar_t* property)
			{
				if (!residual.contains(key)) return;
				if (!residual[key].is_number())
					throw std::invalid_argument("Invalid numeric control property");
				const auto value = residual[key].is_number_float()
						? NumberText(residual[key].get<double>(), 15)
						: FromUtf8(residual[key].ToString());
				if (canWriteLegacy(property) && !isDefault(property, value))
					attributes[attribute] = value;
				residual.ObjectItems().erase(key);
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
			if (node.Type == UIClass::UI_PictureBox)
				consumeNumber("sizeMode", L"SizeMode", L"SizeMode");
			else if (residual.contains("sizeMode"))
				residual.ObjectItems().erase("sizeMode");
			consumeString("hAlign", L"HorizontalAlignment", L"HAlign");
			consumeString("vAlign", L"VerticalAlignment", L"VAlign");
			consumeString("dock", L"DockPanel.Dock", L"DockPosition");

			if (residual.contains("location"))
			{
				if (!residual["location"].is_object())
					throw std::invalid_argument("Invalid control location");
				if (canWriteLegacy(L"Left"))
					attributes[L"Canvas.Left"] = std::to_wstring(
						residual["location"].value("x", 0));
				if (canWriteLegacy(L"Top"))
					attributes[L"Canvas.Top"] = std::to_wstring(
						residual["location"].value("y", 0));
				residual.ObjectItems().erase("location");
			}
			if (residual.contains("size"))
			{
				if (!residual["size"].is_object())
					throw std::invalid_argument("Invalid control size");
				if (canWriteLegacy(L"Width")
					&& canWriteLegacy(L"LayoutWidth"))
					attributes[L"Width"] = std::to_wstring(
						residual["size"].value("w", 0));
				if (canWriteLegacy(L"Height")
					&& canWriteLegacy(L"LayoutHeight"))
					attributes[L"Height"] = std::to_wstring(
						residual["size"].value("h", 0));
				residual.ObjectItems().erase("size");
			}
			for (const auto& [key, attribute, property] : {
				std::tuple{ "backColor", L"BackColor", L"BackColor" },
				std::tuple{ "foreColor", L"ForeColor", L"ForeColor" },
				std::tuple{ "borderColor", L"BorderColor", L"BorderColor" },
				std::tuple{ "bolderColor", L"BorderColor", L"BorderColor" } })
			{
				if (!residual.contains(key)) continue;
				const auto text = ColorText(residual[key]);
				if (!text)
					throw std::invalid_argument("Invalid color control property");
				if (canWriteLegacy(property) && !isDefault(property, *text))
					attributes[attribute] = *text;
				residual.ObjectItems().erase(key);
			}
			for (const auto& [key, attribute, property] : {
				std::tuple{ "margin", L"Margin", L"Margin" },
				std::tuple{ "padding", L"Padding", L"Padding" } })
			{
				if (!residual.contains(key)) continue;
				const auto text = ThicknessText(residual[key]);
				if (!text)
					throw std::invalid_argument("Invalid thickness control property");
				if (canWriteLegacy(property) && !isDefault(property, *text))
					attributes[attribute] = *text;
				residual.ObjectItems().erase(key);
			}
			if (residual.contains("anchor") && residual["anchor"].is_number())
			{
				if (canWriteLegacy(L"Anchor") && (!defaultControl
					|| residual["anchor"].get<int>()
						!= static_cast<int>(defaultControl->AnchorStyles)))
					attributes[L"Anchor"] = AnchorText(residual["anchor"].get<int>());
				residual.ObjectItems().erase("anchor");
			}
			if (residual.contains("font") && residual["font"].is_object())
			{
				const auto& font = residual["font"];
				if (font.contains("name") && font["name"].is_string()
					&& canWriteLegacy(L"FontName"))
					attributes[L"FontName"] = FromUtf8(font["name"].get<std::string>());
				if (font.contains("size") && font["size"].is_number()
					&& canWriteLegacy(L"FontSize"))
					attributes[L"FontSize"] = font["size"].is_number_float()
						? NumberText(font["size"].get<double>(), 15)
						: FromUtf8(font["size"].ToString());
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
				for (const auto& [property, stored] : residual["metadata"].ObjectItems())
				{
					const auto propertyName = FromUtf8(property);
					if (!stored.is_object() || !stored.contains("value")
						|| !stored["value"].is_string())
						throw std::invalid_argument("Invalid metadata property in XAML writer");
					if (!HasBinding(node, propertyName))
						attributes[PublicPropertyName(propertyName)] =
							PublicPropertyValue(
								node.Type, propertyName, FromUtf8(
									stored["value"].get<std::string>()));
				}
				residual.ObjectItems().erase("metadata");
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
				for (const auto& [property, value] : residualBindings.ObjectItems())
				{
					const auto propertyName = FromUtf8(property);
					attributes[PublicPropertyName(propertyName)] = BindingMarkup(value);
				}
				residualBindings = DesignValue::object();
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

		void WriteStringItems(
			const Element& owner,
			const char* propertyElement,
			const DesignValue& values)
		{
			if (!values.is_array() || values.empty()) return;
			auto property = Append(_xml, owner, propertyElement);
			for (const auto& value : values.ArrayItems())
			{
				if (!value.is_string())
					throw std::invalid_argument("String item collection contains a non-string value");
				auto item = Append(_xml, property, "x:String");
				item->SetInnerText(value.get<std::string>());
			}
		}

		void WriteMenuItems(
			const Element& property,
			const DesignValue& values,
			bool allowSeparator)
		{
			if (!values.is_array())
				throw std::invalid_argument("Menu.Items must be an array");
			for (const auto& value : values.ArrayItems())
			{
				if (!value.is_object())
					throw std::invalid_argument("Menu item must be an object");
				if (value.value("separator", false))
				{
					if (!allowSeparator)
						throw std::invalid_argument(
							"Menu top-level items cannot be separators");
					Append(_xml, property, "Separator");
					continue;
				}
				auto item = Append(_xml, property, "MenuItem");
				Set(item, "Header", FromUtf8(value.value("text", std::string{})));
				const auto id = value.value("id", 0);
				if (id != 0) item->SetAttribute("CommandId", std::to_string(id));
				const auto shortcut = value.value("shortcut", std::string{});
				if (!shortcut.empty()) item->SetAttribute("Shortcut", shortcut);
				if (!value.value("enable", true)) item->SetAttribute("IsEnabled", "false");
				if (value.contains("subItems") && value["subItems"].is_array()
					&& !value["subItems"].empty())
				{
					auto children = Append(_xml, item, "MenuItem.Items");
					WriteMenuItems(children, value["subItems"], true);
				}
			}
		}

		void WriteTreeItems(const Element& property, const DesignValue& values)
		{
			if (!values.is_array())
				throw std::invalid_argument("TreeView.Items must be an array");
			for (const auto& value : values.ArrayItems())
			{
				if (!value.is_object())
					throw std::invalid_argument("TreeView item must be an object");
				auto item = Append(_xml, property, "TreeViewItem");
				Set(item, "Header", FromUtf8(value.value("text", std::string{})));
				if (value.value("expand", false)) item->SetAttribute("IsExpanded", "true");
				if (value.contains("children") && value["children"].is_array()
					&& !value["children"].empty())
				{
					auto children = Append(_xml, item, "TreeViewItem.Items");
					WriteTreeItems(children, value["children"]);
				}
			}
		}

		void WriteStructuredProperties(
			const DesignNode& node,
			const Element& element,
			DesignValue& extra)
		{
			if (!extra.is_object())
				throw std::invalid_argument("Control structured data must be an object");
			auto requireArray = [&](const char* key, const char* message)
				-> const DesignValue&
			{
				const auto& value = static_cast<const DesignValue&>(extra)[key];
				if (!value.is_array()) throw std::invalid_argument(message);
				return value;
			};
			auto writeTransformOperation = [&](const Element& parent,
				const DesignValue& value)
			{
				if (!value.is_object())
					throw std::invalid_argument("Transform operation must be an object");
				const auto type = value.value("type", std::string{});
				Element operation;
				if (type == "matrix")
				{
					operation = Append(_xml, parent, "MatrixTransform");
					Set(operation, "Matrix",
						NumberText(value.value("m11", 1.0)) + L"," +
						NumberText(value.value("m12", 0.0)) + L"," +
						NumberText(value.value("m21", 0.0)) + L"," +
						NumberText(value.value("m22", 1.0)) + L"," +
						NumberText(value.value("dx", 0.0)) + L"," +
						NumberText(value.value("dy", 0.0)));
				}
				else if (type == "translate")
				{
					operation = Append(_xml, parent, "TranslateTransform");
					const auto x = value.value("x", 0.0);
					const auto y = value.value("y", 0.0);
					if (x != 0.0) Set(operation, "X", NumberText(x));
					if (y != 0.0) Set(operation, "Y", NumberText(y));
				}
				else if (type == "scale")
				{
					operation = Append(_xml, parent, "ScaleTransform");
					const auto x = value.value("scaleX", 1.0);
					const auto y = value.value("scaleY", 1.0);
					if (x != 1.0) Set(operation, "ScaleX", NumberText(x));
					if (y != 1.0) Set(operation, "ScaleY", NumberText(y));
				}
				else if (type == "rotate")
				{
					operation = Append(_xml, parent, "RotateTransform");
					const auto angle = value.value("angle", 0.0);
					if (angle != 0.0) Set(operation, "Angle", NumberText(angle));
				}
				else if (type == "skew")
				{
					operation = Append(_xml, parent, "SkewTransform");
					const auto x = value.value("angleX", 0.0);
					const auto y = value.value("angleY", 0.0);
					if (x != 0.0) Set(operation, "AngleX", NumberText(x));
					if (y != 0.0) Set(operation, "AngleY", NumberText(y));
				}
				else throw std::invalid_argument("Transform operation type is invalid");
				if (type == "scale" || type == "rotate" || type == "skew")
				{
					const auto centerX = value.value("centerX", 0.0);
					const auto centerY = value.value("centerY", 0.0);
					if (centerX != 0.0) Set(operation, "CenterX", NumberText(centerX));
					if (centerY != 0.0) Set(operation, "CenterY", NumberText(centerY));
				}
			};
			auto writeTransform = [&](const Element& property,
				const DesignValue& values)
			{
				if (!values.is_array() || values.empty())
					throw std::invalid_argument("Transform must be a non-empty array");
				if (values.size() == 1)
					writeTransformOperation(property, values[size_t{ 0 }]);
				else
				{
					auto group = Append(_xml, property, "TransformGroup");
					for (const auto& value : values.ArrayItems())
						writeTransformOperation(group, value);
				}
			};
			if (extra.contains("clip"))
			{
				const auto& clip = extra["clip"];
				auto writeGeometry = [&](auto&& self,
					const Element& parent, const DesignValue& value) -> void
				{
					if (!value.is_object())
						throw std::invalid_argument("Control.Clip geometry must be an object");
					const auto type = value.value("type", std::string{});
					Element geometry;
					if (type == "rectangle")
					{
						geometry = Append(_xml, parent, "RectangleGeometry");
						Set(geometry, "Rect",
							NumberText(value.value("x", 0.0)) + L","
							+ NumberText(value.value("y", 0.0)) + L","
							+ NumberText(value.value("width", 0.0)) + L","
							+ NumberText(value.value("height", 0.0)));
						const auto radiusX = value.value("radiusX", 0.0);
						const auto radiusY = value.value("radiusY", 0.0);
						if (radiusX != 0.0) Set(geometry, "RadiusX", NumberText(radiusX));
						if (radiusY != 0.0) Set(geometry, "RadiusY", NumberText(radiusY));
					}
					else if (type == "ellipse")
					{
						geometry = Append(_xml, parent, "EllipseGeometry");
						Set(geometry, "Center",
							NumberText(value.value("centerX", 0.0)) + L","
							+ NumberText(value.value("centerY", 0.0)));
						Set(geometry, "RadiusX",
							NumberText(value.value("radiusX", 0.0)));
						Set(geometry, "RadiusY",
							NumberText(value.value("radiusY", 0.0)));
					}
					else if (type == "path")
					{
						geometry = Append(_xml, parent, "PathGeometry");
						const auto fillRule = value.value("fillRule", std::string("evenodd"));
						if (fillRule == "nonzero") Set(geometry, "FillRule", L"Nonzero");
						else if (fillRule != "evenodd")
							throw std::invalid_argument("PathGeometry FillRule is invalid");
						if (!value.contains("figures") || !value["figures"].is_array())
							throw std::invalid_argument("PathGeometry Figures must be an array");
						for (const auto& figureValue : value["figures"].ArrayItems())
						{
							if (!figureValue.is_object())
								throw std::invalid_argument("PathFigure must be an object");
							auto figure = Append(_xml, geometry, "PathFigure");
							Set(figure, "StartPoint",
								NumberText(figureValue.value("startX", 0.0)) + L","
								+ NumberText(figureValue.value("startY", 0.0)));
							if (figureValue.value("closed", false))
								Set(figure, "IsClosed", L"true");
							if (!figureValue.value("filled", true))
								Set(figure, "IsFilled", L"false");
							if (!figureValue.contains("segments")
								|| !figureValue["segments"].is_array())
								throw std::invalid_argument("PathFigure Segments must be an array");
							for (const auto& segmentValue
								: figureValue["segments"].ArrayItems())
							{
								if (!segmentValue.is_object())
									throw std::invalid_argument("PathSegment must be an object");
								const auto segmentType = segmentValue.value(
									"type", std::string{});
								Element segment;
								if (segmentType == "line")
								{
									segment = Append(_xml, figure, "LineSegment");
									Set(segment, "Point",
										NumberText(segmentValue.value("x", 0.0)) + L","
										+ NumberText(segmentValue.value("y", 0.0)));
								}
								else if (segmentType == "bezier")
								{
									segment = Append(_xml, figure, "BezierSegment");
									for (int point = 1; point <= 3; ++point)
									{
										const auto suffix = std::to_string(point);
										Set(segment, ("Point" + suffix).c_str(),
											NumberText(segmentValue.value("x" + suffix, 0.0))
											+ L"," + NumberText(segmentValue.value(
												"y" + suffix, 0.0)));
									}
								}
								else if (segmentType == "quadratic")
								{
									segment = Append(_xml, figure, "QuadraticBezierSegment");
									for (int point = 1; point <= 2; ++point)
									{
										const auto suffix = std::to_string(point);
										Set(segment, ("Point" + suffix).c_str(),
											NumberText(segmentValue.value("x" + suffix, 0.0))
											+ L"," + NumberText(segmentValue.value(
												"y" + suffix, 0.0)));
									}
								}
								else if (segmentType == "arc")
								{
									segment = Append(_xml, figure, "ArcSegment");
									Set(segment, "Point",
										NumberText(segmentValue.value("x", 0.0)) + L","
										+ NumberText(segmentValue.value("y", 0.0)));
									Set(segment, "Size",
										NumberText(segmentValue.value("width", 0.0)) + L","
										+ NumberText(segmentValue.value("height", 0.0)));
									const auto rotation = segmentValue.value("rotation", 0.0);
									if (rotation != 0.0)
										Set(segment, "RotationAngle", NumberText(rotation));
									if (segmentValue.value("large", false))
										Set(segment, "IsLargeArc", L"true");
									const auto sweep = segmentValue.value(
										"sweep", std::string("counterclockwise"));
									if (sweep == "clockwise")
										Set(segment, "SweepDirection", L"Clockwise");
									else if (sweep != "counterclockwise")
										throw std::invalid_argument(
											"ArcSegment SweepDirection is invalid");
								}
								else throw std::invalid_argument(
									"PathSegment type is invalid");
							}
						}
					}
					else if (type == "group")
					{
						geometry = Append(_xml, parent, "GeometryGroup");
						const auto fillRule = value.value("fillRule", std::string("evenodd"));
						if (fillRule == "nonzero") Set(geometry, "FillRule", L"Nonzero");
						else if (fillRule != "evenodd")
							throw std::invalid_argument("GeometryGroup FillRule is invalid");
						if (!value.contains("children") || !value["children"].is_array())
							throw std::invalid_argument("GeometryGroup Children must be an array");
						for (const auto& child : value["children"].ArrayItems())
							self(self, geometry, child);
					}
					else
						throw std::invalid_argument("Control.Clip geometry type is invalid");
					if (value.contains("transform"))
					{
						auto transformProperty = Append(_xml, geometry,
							"Geometry.Transform");
						writeTransform(transformProperty, value["transform"]);
					}
				};
				auto property = Append(_xml, element, "Control.Clip");
				writeGeometry(writeGeometry, property, clip);
				extra.ObjectItems().erase("clip");
			}
			if (extra.contains("renderTransformOrigin"))
			{
				const auto& origin = extra["renderTransformOrigin"];
				if (!origin.is_object())
					throw std::invalid_argument("RenderTransformOrigin must be an object");
				Set(element, "RenderTransformOrigin",
					NumberText(origin.value("x", 0.0)) + L","
					+ NumberText(origin.value("y", 0.0)));
				extra.ObjectItems().erase("renderTransformOrigin");
			}
			if (extra.contains("renderTransform"))
			{
				const auto& values = requireArray(
					"renderTransform", "Control.RenderTransform must be an array");
				auto property = Append(_xml, element, "Control.RenderTransform");
				writeTransform(property, values);
				extra.ObjectItems().erase("renderTransform");
			}
			if (extra.contains("foregroundBrush"))
			{
				const auto& value = extra["foregroundBrush"];
				if (!value.is_object())
					throw std::invalid_argument("Control.Foreground must be an object");
				auto property = Append(_xml, element, "Control.Foreground");
				WriteBrush(property, value);
				extra.ObjectItems().erase("foregroundBrush");
			}
			if (node.Type == UIClass::UI_MediaPlayer && extra.contains("mediaFile"))
			{
				if (!extra["mediaFile"].is_string())
					throw std::invalid_argument("MediaPlayer Source must be a string");
				Set(element, "Source", FromUtf8(extra["mediaFile"].get<std::string>()));
				extra.ObjectItems().erase("mediaFile");
			}
			if ((node.Type == UIClass::UI_NavigationView
				|| node.Type == UIClass::UI_SideBar)
				&& extra.contains("navigationItems"))
			{
				const auto owner = node.Type == UIClass::UI_SideBar
					? "SideBar.Items" : "NavigationView.Items";
				auto items = Append(_xml, element, owner);
				for (const auto& value : requireArray("navigationItems",
					"NavigationView.Items must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("NavigationView item must be an object");
					const int kind = value.value("kind", 0);
					auto item = Append(_xml, items, kind == 1
						? "NavigationViewHeader" : kind == 2
						? "NavigationViewSeparator" : "NavigationViewItem");
					const auto text = value.value("text", std::string{});
					if (!text.empty()) item->SetAttribute("Text", text);
					const auto itemValue = value.value("value", std::string{});
					if (!itemValue.empty()) item->SetAttribute("Value", itemValue);
					const auto badge = value.value("badgeText", std::string{});
					if (!badge.empty()) item->SetAttribute("BadgeText", badge);
					const auto icon = value.value("icon", std::string{});
					if (!icon.empty()) item->SetAttribute("Icon", icon);
					const bool defaultEnabled = kind == 0;
					const bool enabled = value.value("enabled", defaultEnabled);
					if (enabled != defaultEnabled)
						item->SetAttribute("IsEnabled", BoolText(enabled));
					if (value.value("selected", false))
						item->SetAttribute("IsSelected", "true");
					if (value.value("tag", static_cast<unsigned long long>(0)) != 0)
						item->SetAttribute("Tag", value["tag"].ToString());
				}
				extra.ObjectItems().erase("navigationItems");
			}
			if (node.Type == UIClass::UI_BreadcrumbBar
				&& extra.contains("breadcrumbItems"))
			{
				auto items = Append(_xml, element, "BreadcrumbBar.Items");
				for (const auto& value : requireArray("breadcrumbItems",
					"BreadcrumbBar.Items must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("BreadcrumbBar item must be an object");
					auto item = Append(_xml, items, "BreadcrumbBarItem");
					const auto text = value.value("text", std::string{});
					if (!text.empty()) item->SetAttribute("Text", text);
					const auto itemValue = value.value("value", std::string{});
					if (!itemValue.empty()) item->SetAttribute("Value", itemValue);
					if (!value.value("enabled", true)) item->SetAttribute("IsEnabled", "false");
					if (value.value("tag", static_cast<unsigned long long>(0)) != 0)
						item->SetAttribute("Tag", value["tag"].ToString());
				}
				extra.ObjectItems().erase("breadcrumbItems");
			}
			if (node.Type == UIClass::UI_FilterBar && extra.contains("filterItems"))
			{
				auto items = Append(_xml, element, "FilterBar.Items");
				for (const auto& value : requireArray("filterItems",
					"FilterBar.Items must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("FilterBar item must be an object");
					auto item = Append(_xml, items, "FilterBarItem");
					const auto text = value.value("text", std::string{});
					if (!text.empty()) item->SetAttribute("Text", text);
					const auto itemValue = value.value("value", std::string{});
					if (!itemValue.empty()) item->SetAttribute("Value", itemValue);
					if (value.value("selected", false)) item->SetAttribute("IsSelected", "true");
					if (!value.value("enabled", true)) item->SetAttribute("IsEnabled", "false");
					if (value.value("tag", static_cast<unsigned long long>(0)) != 0)
						item->SetAttribute("Tag", value["tag"].ToString());
				}
				extra.ObjectItems().erase("filterItems");
			}
			if (node.Type == UIClass::UI_KpiCard && extra.contains("sparkline"))
			{
				auto values = Append(_xml, element, "KpiCard.Sparkline");
				for (const auto& value : requireArray("sparkline",
					"KpiCard.Sparkline must be an array").ArrayItems())
				{
					if (!value.is_number())
						throw std::invalid_argument("KpiCard sparkline value must be numeric");
					auto item = Append(_xml, values, "x:Double");
					item->SetInnerText(ToUtf8(NumberText(value.get<double>(), 15)));
				}
				extra.ObjectItems().erase("sparkline");
			}
			if (node.Type == UIClass::UI_ChartView && extra.contains("series"))
			{
				auto seriesProperty = Append(_xml, element, "ChartView.Series");
				for (const auto& value : requireArray("series",
					"ChartView.Series must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("Chart series must be an object");
					auto series = Append(_xml, seriesProperty, "ChartSeries");
					const auto name = value.value("name", std::string{});
					if (!name.empty()) series->SetAttribute("Name", name);
					if (value.contains("color"))
					{
						const auto color = ColorText(value["color"]);
						if (!color) throw std::invalid_argument("Chart series Color is invalid");
						Set(series, "Color", *color);
					}
					if (!value.value("visible", true)) series->SetAttribute("IsVisible", "false");
					if (value.contains("points") && value["points"].is_array()
						&& !value["points"].empty())
					{
						auto points = Append(_xml, series, "ChartSeries.Points");
						for (const auto& pointValue : value["points"])
						{
							if (!pointValue.is_object())
								throw std::invalid_argument("Chart point must be an object");
							auto point = Append(_xml, points, "ChartPoint");
							const auto label = pointValue.value("label", std::string{});
							if (!label.empty()) point->SetAttribute("Label", label);
							Set(point, "Value", NumberText(pointValue.value("value", 0.0), 15));
							if (pointValue.value("useCustomColor", false)
								&& pointValue.contains("color"))
							{
								const auto color = ColorText(pointValue["color"]);
								if (!color) throw std::invalid_argument("Chart point Color is invalid");
								Set(point, "Color", *color);
							}
							if (pointValue.value("tag", static_cast<unsigned long long>(0)) != 0)
								point->SetAttribute("Tag", pointValue["tag"].ToString());
						}
					}
				}
				extra.ObjectItems().erase("series");
			}
			if (node.Type == UIClass::UI_ReportView && extra.contains("reportColumns"))
			{
				auto columns = Append(_xml, element, "ReportView.Columns");
				for (const auto& value : requireArray("reportColumns",
					"ReportView.Columns must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("Report column must be an object");
					auto column = Append(_xml, columns, "ReportColumn");
					const auto header = value.value("header", std::string{});
					if (!header.empty()) column->SetAttribute("Header", header);
					const auto width = value.value("width", 120.0);
					if (width != 120.0) Set(column, "Width", NumberText(width));
					const auto align = value.value("align", 0);
					if (align != 0) Set(column, "Align",
						EnumText(align, { L"Left", L"Center", L"Right" }));
					if (!value.value("sortable", true)) column->SetAttribute("IsSortable", "false");
				}
				extra.ObjectItems().erase("reportColumns");
			}
			if (node.Type == UIClass::UI_ReportView && extra.contains("reportRows"))
			{
				auto rows = Append(_xml, element, "ReportView.Rows");
				for (const auto& value : requireArray("reportRows",
					"ReportView.Rows must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("Report row must be an object");
					const int kind = value.value("kind", 0);
					const auto rowName = kind == 1 ? "ReportGroup"
						: kind == 2 ? "ReportSummary" : "ReportRow";
					auto row = Append(_xml, rows, rowName);
					const auto caption = value.value("caption", std::string{});
					if (!caption.empty()) row->SetAttribute("Caption", caption);
					if (kind == 1 && !value.value("expanded", true))
						row->SetAttribute("IsExpanded", "false");
					if (value.value("tag", static_cast<unsigned long long>(0)) != 0)
						row->SetAttribute("Tag", value["tag"].ToString());
					if (value.contains("cells") && value["cells"].is_array()
						&& !value["cells"].empty())
						WriteStringItems(row, (std::string(rowName) + ".Cells").c_str(),
							value["cells"]);
				}
				extra.ObjectItems().erase("reportRows");
			}
			if (node.Type == UIClass::UI_ComboBox && extra.contains("items"))
			{
				for (const auto& value : requireArray(
					"items", "ComboBox.Items must be an array").ArrayItems())
				{
					if (!value.is_string())
						throw std::invalid_argument("ComboBox item must be a string");
					auto item = Append(_xml, element, "ComboBoxItem");
					item->SetAttribute("Content", value.get<std::string>());
				}
				extra.ObjectItems().erase("items");
			}
			if ((node.Type == UIClass::UI_ListView || node.Type == UIClass::UI_ListBox)
				&& extra.contains("columns"))
			{
				const auto owner = node.Type == UIClass::UI_ListBox ? "ListBox" : "ListView";
				auto columns = Append(_xml, element, std::string(owner) + ".Columns");
				for (const auto& value : requireArray(
					"columns", "ListView.Columns must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("ListView column must be an object");
					auto column = Append(_xml, columns, "ListViewColumn");
					const auto header = value.value("header", std::string{});
					if (!header.empty()) column->SetAttribute("Header", header);
					const auto width = value.value("width", 120.0);
					if (width != 120.0) Set(column, "Width", NumberText(width));
					const auto align = value.value("align", 0);
					if (align != 0) Set(column, "HorizontalAlignment",
						EnumText(align, { L"Left", L"Center", L"Right" }));
				}
				extra.ObjectItems().erase("columns");
			}
			if ((node.Type == UIClass::UI_ListView || node.Type == UIClass::UI_ListBox)
				&& extra.contains("items"))
			{
				for (const auto& value : requireArray(
					"items", "ListView.Items must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("ListView item must be an object");
					auto item = Append(_xml, element,
						node.Type == UIClass::UI_ListBox ? "ListBoxItem" : "ListViewItem");
					const auto text = value.value("text", std::string{});
					if (!text.empty()) item->SetAttribute("Content", text);
					const auto subText = value.value("subText", std::string{});
					if (!subText.empty()) item->SetAttribute("SubText", subText);
					if (value.value("checked", false)) item->SetAttribute("IsChecked", "true");
					if (value.value("selected", false)) item->SetAttribute("IsSelected", "true");
					if (!value.value("enabled", true)) item->SetAttribute("IsEnabled", "false");
					if (value.contains("subItems"))
						WriteStringItems(item,
							node.Type == UIClass::UI_ListBox
								? "ListBoxItem.SubItems" : "ListViewItem.SubItems",
							value["subItems"]);
				}
				extra.ObjectItems().erase("items");
			}
			const bool dataGrid = node.Type == UIClass::UI_GridView
				|| node.Type == UIClass::UI_PagedGridView;
			if (dataGrid && extra.contains("columns"))
			{
				const auto owner = node.Type == UIClass::UI_PagedGridView
					? "PagedGridView" : "GridView";
				auto columns = Append(_xml, element, std::string(owner) + ".Columns");
				for (const auto& value : requireArray(
					"columns", "GridView.Columns must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("GridView column must be an object");
					auto column = Append(_xml, columns, "GridViewColumn");
					const auto header = value.value("name", std::string{});
					if (!header.empty()) column->SetAttribute("Header", header);
					const auto width = value.value("width", 120.0);
					if (width != 120.0) Set(column, "Width", NumberText(width));
					const auto columnType = value.value("type", 0);
					if (columnType != 0) Set(column, "Type", EnumText(columnType,
						{ L"Text", L"Image", L"Check", L"Button", L"ComboBox", L"LinkedText" }));
					if (!value.value("canEdit", true)) column->SetAttribute("CanEdit", "false");
					const auto buttonText = value.value("buttonText", std::string{});
					if (!buttonText.empty()) column->SetAttribute("ButtonText", buttonText);
					if (value.contains("comboBoxItems"))
						WriteStringItems(column, "GridViewColumn.Items", value["comboBoxItems"]);
				}
				extra.ObjectItems().erase("columns");
			}
			if (dataGrid && extra.contains("rows"))
			{
				const auto owner = node.Type == UIClass::UI_PagedGridView
					? "PagedGridView" : "GridView";
				auto rows = Append(_xml, element, std::string(owner) + ".Rows");
				for (const auto& rowValue : requireArray(
					"rows", "GridView.Rows must be an array").ArrayItems())
				{
					if (!rowValue.is_object() || !rowValue.contains("cells")
						|| !rowValue["cells"].is_array())
						throw std::invalid_argument("GridView row must contain cells");
					auto row = Append(_xml, rows, "GridViewRow");
					for (const auto& cellValue : rowValue["cells"])
					{
						if (!cellValue.is_object())
							throw std::invalid_argument("GridView cell must be an object");
						auto cell = Append(_xml, row, "GridViewCell");
						if (cellValue.contains("value"))
						{
							if (!cellValue["value"].is_string())
								throw std::invalid_argument("GridView cell Value must be a string");
							cell->SetAttribute("Value", cellValue["value"].get<std::string>());
						}
						if (cellValue.contains("checked"))
							cell->SetAttribute("IsChecked",
								BoolText(cellValue["checked"].get<bool>()));
						if (cellValue.contains("tag"))
							cell->SetAttribute("Tag", cellValue["tag"].ToString());
						if (cellValue.contains("selectedIndex"))
							cell->SetAttribute("SelectedIndex",
								cellValue["selectedIndex"].ToString());
					}
				}
				extra.ObjectItems().erase("rows");
			}
			if (node.Type == UIClass::UI_PropertyGrid && extra.contains("items"))
			{
				auto items = Append(_xml, element, "PropertyGrid.Items");
				for (const auto& value : requireArray(
					"items", "PropertyGrid.Items must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("PropertyGrid item must be an object");
					auto item = Append(_xml, items, "PropertyGridItem");
					for (const auto& [key, attribute] : {
						std::pair{ "category", "Category" }, std::pair{ "name", "Name" },
						std::pair{ "value", "Value" }, std::pair{ "description", "Description" } })
						if (value.contains(key) && value[key].is_string()
							&& !value[key].get<std::string>().empty())
							item->SetAttribute(attribute, value[key].get<std::string>());
					const auto valueType = value.value("type", 0);
					if (valueType != 0) Set(item, "Type", EnumText(valueType,
						{ L"Text", L"Number", L"Bool", L"Enum", L"Color", L"ReadOnly",
						  L"Action", L"Slider", L"Anchor", L"EditableEnum" }));
					if (value.value("readOnly", false)) item->SetAttribute("IsReadOnly", "true");
					if (value.value("isMixed", false)) item->SetAttribute("IsMixed", "true");
					if (value.value("canReset", false)) item->SetAttribute("CanReset", "true");
					if (value.value("minimum", 0.0) != 0.0)
						Set(item, "Minimum", NumberText(value.value("minimum", 0.0), 15));
					if (value.value("maximum", 1.0) != 1.0)
						Set(item, "Maximum", NumberText(value.value("maximum", 1.0), 15));
					if (value.value("step", 0.01) != 0.01)
						Set(item, "Step", NumberText(value.value("step", 0.01), 15));
					if (value.value("tag", static_cast<unsigned long long>(0)) != 0)
						item->SetAttribute("Tag", value["tag"].ToString());
					if (value.contains("options"))
						WriteStringItems(item, "PropertyGridItem.Options", value["options"]);
				}
				extra.ObjectItems().erase("items");
			}
			if (node.Type == UIClass::UI_TreeView && extra.contains("nodes"))
			{
				auto items = Append(_xml, element, "TreeView.Items");
				WriteTreeItems(items,
					requireArray("nodes", "TreeView.Items must be an array"));
				extra.ObjectItems().erase("nodes");
			}
			if (node.Type == UIClass::UI_StatusBar && extra.contains("parts"))
			{
				auto items = Append(_xml, element, "StatusBar.Items");
				for (const auto& value : requireArray(
					"parts", "StatusBar.Items must be an array").ArrayItems())
				{
					if (!value.is_object())
						throw std::invalid_argument("StatusBar item must be an object");
					auto item = Append(_xml, items, "StatusBarItem");
					const auto text = value.value("text", std::string{});
					if (!text.empty()) item->SetAttribute("Text", text);
					const auto width = value.value("width", 0);
					if (width != 0) item->SetAttribute("Width", std::to_string(width));
				}
				extra.ObjectItems().erase("parts");
			}
			if (node.Type == UIClass::UI_Menu && extra.contains("items"))
			{
				auto items = Append(_xml, element, "Menu.Items");
				WriteMenuItems(items,
					requireArray("items", "Menu.Items must be an array"), false);
				extra.ObjectItems().erase("items");
			}
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
