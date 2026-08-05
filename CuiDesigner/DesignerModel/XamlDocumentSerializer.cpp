#include "XamlDocumentSerializer.h"
#include "DesignDataResourceUtils.h"

#include "AtomicFile.h"
#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "../../CuiRuntime/include/XamlRuntimeSchema.h"
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
#include <functional>
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
	constexpr const char* RelativePanelConstraintsKey =
		"relativePanelConstraints";

	std::string ToUtf8(const std::wstring& value)
	{
		return Convert::UnicodeToUtf8(value);
	}

	std::wstring FromUtf8(const std::string& value)
	{
		return Convert::Utf8ToUnicode(value);
	}

	bool Equals(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	std::wstring BuiltInXamlTypeName(
		UIClass nativeType,
		const RuntimeTypeId& xamlType)
	{
		const CuiRuntime::BuiltInXamlTypeDescriptor* descriptor = nullptr;
		if (xamlType.Valid())
			descriptor = CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
				xamlType.NamespaceUri, xamlType.LocalName);
		else
			descriptor = CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(nativeType);
		if (!descriptor || descriptor->NativeType != nativeType)
			throw std::invalid_argument(
				"Control has an invalid built-in XAML type identity");
		return descriptor->TypeId.LocalName;
	}

	std::wstring BuiltInXamlTypeName(const DesignNode& node)
	{
		return BuiltInXamlTypeName(node.Type, node.XamlType);
	}

	std::wstring ControlXamlTypeName(const DesignNode& node)
	{
		return !node.ComponentType.Empty()
			? node.ComponentType.XamlPrefix + L":" + node.ComponentType.XamlName
			: BuiltInXamlTypeName(node);
	}

	std::wstring PublicPropertyName(const std::wstring& property)
	{
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

	std::wstring TimeSpanText(unsigned long long milliseconds)
	{
		const auto days = milliseconds / 86400000ULL;
		milliseconds %= 86400000ULL;
		const auto hours = milliseconds / 3600000ULL;
		milliseconds %= 3600000ULL;
		const auto minutes = milliseconds / 60000ULL;
		milliseconds %= 60000ULL;
		const auto seconds = milliseconds / 1000ULL;
		const auto fraction = milliseconds % 1000ULL;
		std::wostringstream stream;
		if (days > 0) stream << days << L'.';
		stream << (days > 0 ? std::setw(2) : std::setw(1)) << std::setfill(L'0')
			<< hours << L':' << std::setw(2) << minutes << L':'
			<< std::setw(2) << seconds;
		if (fraction > 0) stream << L'.' << std::setw(3) << fraction;
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
		const auto metadata = CuiRuntime::XamlRuntimeSchema::NativeProperties(type);
		const auto descriptors = DesignerPropertyCatalog::GetStyleProperties(metadata);
		const auto* descriptor = DesignerPropertyCatalog::Find(descriptors, property);
		if (!descriptor) return value;
		for (const auto& choice : descriptor->Choices)
			if (Equals(choice.ValueText, value)) return choice.DisplayName;
		auto enumName = [&](const std::wstring& expected,
			std::initializer_list<const wchar_t*> names)
			-> std::optional<std::wstring>
		{
			if (!Equals(property, expected)) return std::nullopt;
			int index = 0;
			try { index = std::stoi(value); }
			catch (...) { return std::nullopt; }
			int current = 0;
			for (const auto* name : names)
				if (current++ == index) return std::wstring(name);
			return std::nullopt;
		};
		if (const auto name = enumName(
			L"HorizontalAlignment", { L"Left", L"Center", L"Right", L"Stretch" })) return *name;
		if (const auto name = enumName(
			L"VerticalAlignment", { L"Top", L"Center", L"Bottom", L"Stretch" })) return *name;
		if (const auto name = enumName(L"DockPanel.Dock",
			{ L"Left", L"Top", L"Right", L"Bottom", L"Fill" })) return *name;
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

	std::wstring EnumText(
		int value,
		std::initializer_list<const wchar_t*> names)
	{
		if (value >= 0 && static_cast<size_t>(value) < names.size())
			return *(names.begin() + value);
		return std::to_wstring(value);
	}

	bool HasBinding(const DesignNode& node, const std::wstring& property)
	{
		return node.Bindings.contains(property);
	}

	bool HasMetadata(const DesignNode& node, const std::wstring& property)
	{
		return node.Properties.Find(property) != nullptr;
	}

	std::wstring QuoteBindingLiteral(const std::wstring& value)
	{
		std::wstring result = L"'";
		for (const auto ch : value)
		{
			result.push_back(ch);
			if (ch == L'\'') result.push_back(ch);
		}
		result.push_back(L'\'');
		return result;
	}

	std::wstring BindingMarkup(const DesignerDataBinding& binding)
	{
		if (binding.IsMultiBinding())
			throw std::invalid_argument("MultiBinding cannot use markup syntax");
		if (binding.SourceProperty.empty()
			|| (binding.RelativeSource != DesignerBindingRelativeSource::None
				&& !binding.ElementName.empty())
			|| (binding.RelativeSource == DesignerBindingRelativeSource::FindAncestor
				&& (binding.AncestorType.empty() || binding.AncestorLevel < 1))
			|| (binding.RelativeSource != DesignerBindingRelativeSource::FindAncestor
				&& (!binding.AncestorType.empty()
					|| !binding.AncestorTypeNamespace.empty()
					|| binding.AncestorLevel != 1)))
			throw std::invalid_argument("XAML binding contains an invalid path or mode");
		if (binding.StringFormat
			&& !IsValidBindingStringFormat(*binding.StringFormat))
			throw std::invalid_argument("XAML binding StringFormat is invalid");
		std::wstring result = L"{Binding " + binding.SourceProperty;
		if (binding.Mode != BindingMode::Default)
			result += L", Mode="
				+ std::wstring(DesignerBindingUtils::BindingModeName(binding.Mode));
		if (binding.UpdateMode != DataSourceUpdateMode::Default)
			result += L", UpdateSourceTrigger=" + std::wstring(
				DesignerBindingUtils::UpdateSourceTriggerName(binding.UpdateMode));
		if (!binding.Converter.empty()) result += L", Converter=" + binding.Converter;
		if (binding.ConverterParameter)
			result += L", ConverterParameter="
				+ QuoteBindingLiteral(binding.ConverterParameter->Text);
		if (binding.StringFormat)
			result += L", StringFormat=" + QuoteBindingLiteral(*binding.StringFormat);
		if (!binding.ElementName.empty())
			result += L", ElementName=" + binding.ElementName;
		if (binding.FallbackValue)
			result += L", FallbackValue="
				+ QuoteBindingLiteral(binding.FallbackValue->Text);
		if (binding.TargetNullValue)
			result += L", TargetNullValue="
				+ QuoteBindingLiteral(binding.TargetNullValue->Text);
		if (binding.RelativeSource == DesignerBindingRelativeSource::FindAncestor)
		{
			result += L", RelativeSource={RelativeSource FindAncestor, AncestorType={x:Type "
				+ binding.AncestorType + L"}";
			if (binding.AncestorLevel != 1)
				result += L", AncestorLevel=" + std::to_wstring(binding.AncestorLevel);
			result += L"}";
		}
		else if (binding.RelativeSource != DesignerBindingRelativeSource::None)
			result += L", RelativeSource={RelativeSource "
				+ std::wstring(binding.RelativeSource
					== DesignerBindingRelativeSource::Self
						? L"Self" : L"TemplatedParent") + L"}";
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
		throw std::invalid_argument("Grid length contains an unknown unit");
	}

	class Writer final
	{
	public:
		Writer(
			const DesignDocument& document,
			XmlDocument& xml,
			const DesignComponentDefinition* templateComponent = nullptr)
			: _document(document), _xml(xml),
			  _templateComponent(templateComponent)
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
			auto root = _xml.CreateElement("Window");
			root->SetAttribute("xmlns", "urn:cui");
			root->SetAttribute(
				"xmlns:x", "http://schemas.microsoft.com/winfx/2006/xaml");
			root->SetAttribute("xmlns:d", "urn:cui:designer");
			std::map<std::wstring, std::wstring> customNamespaces;
			auto addNamespace = [&](const DesignerComponentType& type)
			{
				if (type.Empty()) return;
				if (type.XamlPrefix.empty() || type.XamlName.empty()
					|| type.XamlNamespace.empty()
					|| Equals(type.XamlPrefix, L"x")
					|| Equals(type.XamlPrefix, L"d"))
					throw std::invalid_argument("Invalid XAML component identity");
				const auto [found, inserted] = customNamespaces.emplace(
					type.XamlPrefix, type.XamlNamespace);
				if (!inserted && found->second != type.XamlNamespace)
					throw std::invalid_argument(
						"Component XAML prefix maps to multiple namespaces");
			};
			std::function<void(const std::vector<DesignNode>&)> scanNodes;
			std::function<void(const DesignObjectResourceDictionary&)> scanObjects;
			scanObjects = [&](const DesignObjectResourceDictionary& resources)
			{
				for (const auto& component : resources.Components)
				{
					addNamespace(component.Type);
					scanNodes(component.Template);
				}
				for (const auto& dataTemplate : resources.DataTemplates)
					scanNodes(dataTemplate.Template);
				for (const auto& controlTemplate : resources.ControlTemplates)
				{
					addNamespace(controlTemplate.TargetComponentType);
					scanNodes(controlTemplate.Template);
				}
			};
			scanNodes = [&](const std::vector<DesignNode>& nodes)
			{
				for (const auto& node : nodes)
				{
					addNamespace(node.ComponentType);
					scanObjects(node.LocalObjectResources);
				}
			};
			DesignObjectResourceDictionary documentObjects;
			documentObjects.Components = _document.Components;
			documentObjects.ControlTemplates = _document.ControlTemplates;
			documentObjects.DataTemplates = _document.DataTemplates;
			scanObjects(documentObjects);
			scanNodes(_document.Nodes);
			for (const auto& [prefix, uri] : customNamespaces)
				root->SetAttribute(
					"xmlns:" + ToUtf8(prefix), ToUtf8(uri));
			WriteWindow(root);
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
		const DesignComponentDefinition* _templateComponent = nullptr;
		DesignDocumentGraph _graph;
		std::unordered_set<int> _written;

		void WriteControlForest(const Element& parent)
		{
			for (const auto graphIndex : _graph.Roots())
				WriteControl(
					_document.Nodes[_graph.Nodes()[graphIndex].SourceIndex],
					parent, false);
			if (_written.size() != _document.Nodes.size())
				throw std::invalid_argument(
					"XAML writer could not represent the complete control forest");
		}

		void WriteWindow(const Element& root)
		{
			Set(root, "x:Name", _document.Window.Name);
			if (!_document.CodeBehind.ClassName.empty())
				Set(root, "x:Class", _document.CodeBehind.ClassName);
			if (!_document.CodeBehind.RelativeBasePath.empty())
				Set(root, "d:CodeBehind",
					_document.CodeBehind.RelativeBasePath);
			DesignNodeProperties residualProperties = _document.Window.Properties;
			DesignBindingMap residualBindings = _document.Window.Bindings;
			WriteControlAttributes(
				_document.Window, root, residualProperties, residualBindings);
			WriteMultiBindingProperties(
				_document.Window, root, residualBindings);
			DesignValue residualStructure = DesignValue::object();
			WriteStructuredProperties(
				_document.Window, root, residualProperties, residualStructure);
			WriteCommandAndInputBindings(
				_document.Window, root, L"Window");
			if (!residualProperties.Empty() || !residualBindings.empty()
				|| !residualStructure.empty())
				throw std::invalid_argument(
					"Window contains members without a public XAML representation");
		}

		void WriteSchema(const Element& root)
		{
			if (_document.DataContextSchema.empty()) return;
			auto schema = Append(_xml, root, "Window.DataContextSchema");
			auto properties = _document.DataContextSchema;
			DesignerDataContextSchemaUtils::Canonicalize(properties);
			for (const auto& property : properties)
			{
				auto item = Append(_xml, schema, "Property");
				Set(item, "Path", property.Path);
				Set(item, "Kind",
					DesignerDataContextSchemaUtils::ValueKindName(property.ValueKind));
				if (property.ValueKind == BindingValueKind::Object)
					Set(item, "ObjectType",
						DesignerDataContextSchemaUtils::ObjectKindName(property.ObjectKind));
				if (!property.ItemType.empty()) Set(item, "ItemType", property.ItemType);
				if (!property.DataType.empty()) Set(item, "DataType", property.DataType);
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
			auto writeBrushTransform = [&](const char* key, const char* suffix)
			{
				if (!value.contains(key)) return;
				if (!value[key].is_array() || value[key].empty())
					throw std::invalid_argument("Brush transform must be a non-empty array");
				const auto propertyName = std::string(brushName) + suffix;
				auto property = Append(_xml, brush, propertyName.c_str());
				WriteTransformObject(property, value[key]);
			};
			writeBrushTransform("transform", ".Transform");
			writeBrushTransform("relativeTransform", ".RelativeTransform");
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
				Set(item, "Value", (setter.UsesDynamicResource
					? L"{DynamicResource " : L"{StaticResource ")
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

		void WriteVisualStateSetter(
			const Element& parent,
			const DesignerVisualStateSetter& setter)
		{
			auto item = Append(_xml, parent, "Setter");
			if (!setter.TargetName.empty())
				Set(item, "TargetName", setter.TargetName);
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

		void WriteVisualStateAnimation(
			const Element& storyboard,
			const DesignComponentDefinition& component,
			const DesignerVisualStateAnimation& animation)
		{
			const auto valueType = animation.Kind == DesignerAnimationKind::Color
				? std::string("Color")
				: animation.Kind == DesignerAnimationKind::Object
					? std::string("Object")
					: animation.Kind == DesignerAnimationKind::Thickness
						? std::string("Thickness")
						: animation.Kind == DesignerAnimationKind::Point
							? std::string("Point")
						: animation.Kind == DesignerAnimationKind::Vector
							? std::string("Vector")
						: animation.Kind == DesignerAnimationKind::Rect
							? std::string("Rect")
						: animation.Kind == DesignerAnimationKind::Size
							? std::string("Size")
						: animation.Kind == DesignerAnimationKind::Matrix
							? std::string("Matrix") : std::string("Double");
			if (animation.Kind == DesignerAnimationKind::Object
				&& animation.KeyFrames.empty())
				throw std::invalid_argument(
					"ObjectAnimationUsingKeyFrames requires key frames");
			const auto animationName = valueType + (animation.KeyFrames.empty()
				? "Animation" : "AnimationUsingKeyFrames");
			auto item = Append(_xml, storyboard, animationName);
			if (!animation.TargetName.empty())
				Set(item, "Storyboard.TargetName", animation.TargetName);
			Set(item, "Storyboard.TargetProperty",
				PublicPropertyName(animation.PropertyName));
			if (animation.KeyFrames.empty())
			{
				if (animation.HasFrom)
					Set(item, "From", animation.FromUsesResource
						? L"{StaticResource " + animation.FromResourceKey + L"}"
						: animation.From.Text);
				if (animation.HasTo)
					Set(item, "To", animation.ToUsesResource
						? L"{StaticResource " + animation.ToResourceKey + L"}"
						: animation.To.Text);
				if (animation.HasBy)
					Set(item, "By", animation.ByUsesResource
						? L"{StaticResource " + animation.ByResourceKey + L"}"
						: animation.By.Text);
			}
			Set(item, "Duration", TimeSpanText(animation.DurationMilliseconds));
			if (animation.BeginTimeMilliseconds > 0)
				Set(item, "BeginTime", TimeSpanText(
					animation.BeginTimeMilliseconds));
			switch (animation.RepeatBehavior)
			{
			case DesignerRepeatBehaviorKind::Duration:
				Set(item, "RepeatBehavior", TimeSpanText(
					animation.RepeatDurationMilliseconds));
				break;
			case DesignerRepeatBehaviorKind::Forever:
				Set(item, "RepeatBehavior", L"Forever");
				break;
			case DesignerRepeatBehaviorKind::Count:
			default:
				if (animation.RepeatCount != 1.0)
					Set(item, "RepeatBehavior",
						NumberText(animation.RepeatCount, 17) + L"x");
				break;
			}
			if (animation.AutoReverse) Set(item, "AutoReverse", L"true");
			if (animation.Kind != DesignerAnimationKind::Object)
			{
				if (animation.IsAdditive) Set(item, "IsAdditive", L"true");
				if (animation.IsCumulative) Set(item, "IsCumulative", L"true");
			}
			if (animation.FillBehavior == DesignerTimelineFillBehavior::Stop)
				Set(item, "FillBehavior", L"Stop");
			if (animation.SpeedRatio != 1.0)
				Set(item, "SpeedRatio", NumberText(animation.SpeedRatio, 17));
			if (animation.AccelerationRatio != 0.0)
				Set(item, "AccelerationRatio",
					NumberText(animation.AccelerationRatio, 17));
			if (animation.DecelerationRatio != 0.0)
				Set(item, "DecelerationRatio",
					NumberText(animation.DecelerationRatio, 17));
			auto writeEasing = [&](const Element& owner,
				const std::string& ownerName,
				DesignerEasingKind kind,
				DesignerEasingMode mode)
			{
				auto easingProperty = Append(_xml, owner,
					ownerName + ".EasingFunction");
				const char* easingName = "CubicEase";
				switch (kind)
				{
				case DesignerEasingKind::Quadratic: easingName = "QuadraticEase"; break;
				case DesignerEasingKind::Sine: easingName = "SineEase"; break;
				case DesignerEasingKind::Cubic:
				case DesignerEasingKind::Linear:
				default: break;
				}
				auto easing = Append(_xml, easingProperty, easingName);
				if (mode != DesignerEasingMode::EaseOut)
					Set(easing, "EasingMode",
						mode == DesignerEasingMode::EaseIn
							? L"EaseIn" : L"EaseInOut");
			};
			if (animation.KeyFrames.empty())
			{
				if (animation.Easing != DesignerEasingKind::Linear)
					writeEasing(item, animationName,
						animation.Easing, animation.EasingMode);
				return;
			}
			for (const auto& keyFrame : animation.KeyFrames)
			{
				std::string prefix;
				if (animation.Kind == DesignerAnimationKind::Object
					&& keyFrame.Kind != DesignerKeyFrameKind::Discrete)
					throw std::invalid_argument(
						"ObjectAnimationUsingKeyFrames only supports DiscreteObjectKeyFrame");
				switch (keyFrame.Kind)
				{
				case DesignerKeyFrameKind::Discrete: prefix = "Discrete"; break;
				case DesignerKeyFrameKind::Easing: prefix = "Easing"; break;
				case DesignerKeyFrameKind::Spline: prefix = "Spline"; break;
				case DesignerKeyFrameKind::Linear:
				default: prefix = "Linear"; break;
				}
				const auto keyFrameName = prefix + valueType + "KeyFrame";
				auto frame = Append(_xml, item, keyFrameName);
				Set(frame, "KeyTime", TimeSpanText(
					keyFrame.KeyTimeMilliseconds));
				if (keyFrame.UsesResource)
					Set(frame, "Value", L"{StaticResource "
						+ keyFrame.ResourceKey + L"}");
				else if (animation.Kind == DesignerAnimationKind::Object
					&& keyFrame.Value.Kind == DesignerStyleValueKind::Brush)
				{
					auto value = Append(_xml, frame, keyFrameName + ".Value");
					WriteBrush(value, keyFrame.Value.ObjectValue);
				}
				else if (animation.Kind == DesignerAnimationKind::Object
					&& keyFrame.Value.Kind == DesignerStyleValueKind::Geometry)
				{
					auto value = Append(_xml, frame, keyFrameName + ".Value");
					WriteGeometryObject(value, keyFrame.Value.ObjectValue);
				}
				else if (animation.Kind == DesignerAnimationKind::Object
					&& keyFrame.Value.Kind == DesignerStyleValueKind::Transform)
				{
					auto value = Append(_xml, frame, keyFrameName + ".Value");
					WriteTransformObject(value, keyFrame.Value.ObjectValue);
				}
				else
				{
					auto text = keyFrame.Value.Text;
					if (animation.Kind == DesignerAnimationKind::Object)
					{
						UIClass targetType = component.BaseType;
						if (!animation.TargetName.empty())
						{
							const auto target = std::find_if(component.Template.begin(),
								component.Template.end(), [&](const auto& node)
								{ return Equals(node.Name, animation.TargetName); });
							if (target != component.Template.end()) targetType = target->Type;
						}
						text = PublicPropertyValue(
							targetType, animation.PropertyName, text);
					}
					Set(frame, "Value", text);
				}
				if (keyFrame.Kind == DesignerKeyFrameKind::Easing
					&& keyFrame.Easing != DesignerEasingKind::Linear)
					writeEasing(frame, keyFrameName,
						keyFrame.Easing, keyFrame.EasingMode);
				else if (keyFrame.Kind == DesignerKeyFrameKind::Spline)
					Set(frame, "KeySpline",
						NumberText(keyFrame.KeySplineX1) + L","
						+ NumberText(keyFrame.KeySplineY1) + L" "
						+ NumberText(keyFrame.KeySplineX2) + L","
						+ NumberText(keyFrame.KeySplineY2));
			}
		}

		void WriteGeneratedTransitionEasing(
			const Element& transition,
			DesignerEasingKind kind,
			DesignerEasingMode mode)
		{
			if (kind == DesignerEasingKind::Linear) return;
			auto property = Append(
				_xml, transition, "VisualTransition.GeneratedEasingFunction");
			const char* easingName = kind == DesignerEasingKind::Quadratic
				? "QuadraticEase" : kind == DesignerEasingKind::Sine
					? "SineEase" : "CubicEase";
			auto easing = Append(_xml, property, easingName);
			if (mode != DesignerEasingMode::EaseOut)
				Set(easing, "EasingMode", mode == DesignerEasingMode::EaseIn
					? L"EaseIn" : L"EaseInOut");
		}

		void WriteVisualStateGroups(
			const Element& root,
			const DesignComponentDefinition& component)
		{
			if (component.VisualStateGroups.empty()) return;
			auto groups = Append(
				_xml, root, "VisualStateManager.VisualStateGroups");
			for (const auto& group : component.VisualStateGroups)
			{
				auto groupElement = Append(_xml, groups, "VisualStateGroup");
				Set(groupElement, "x:Name", group.Name);
				if (!group.Transitions.empty())
				{
					auto transitions = Append(
						_xml, groupElement, "VisualStateGroup.Transitions");
					for (const auto& source : group.Transitions)
					{
						auto transition = Append(_xml, transitions, "VisualTransition");
						if (!source.FromState.empty())
							Set(transition, "From", source.FromState);
						if (!source.ToState.empty())
							Set(transition, "To", source.ToState);
						if (source.GeneratedDurationMilliseconds > 0)
							Set(transition, "GeneratedDuration", TimeSpanText(
								source.GeneratedDurationMilliseconds));
						WriteGeneratedTransitionEasing(transition,
							source.GeneratedEasing,
							source.GeneratedEasingMode);
						if (!source.Animations.empty())
						{
							auto property = Append(
								_xml, transition, "VisualTransition.Storyboard");
							auto storyboard = Append(_xml, property, "Storyboard");
							for (const auto& animation : source.Animations)
								WriteVisualStateAnimation(storyboard, component, animation);
						}
					}
				}
				for (const auto& state : group.States)
				{
					auto stateElement = Append(_xml, groupElement, "VisualState");
					Set(stateElement, "x:Name", state.Name);
					if (!state.Conditions.empty() || !state.EventNames.empty())
					{
						auto triggers = Append(
							_xml, stateElement, "VisualState.StateTriggers");
						for (const auto& condition : state.Conditions)
						{
							if (!condition.Value.ObjectValue.is_null())
								throw std::invalid_argument(
									"VisualState StateTrigger requires a scalar value");
							auto trigger = Append(_xml, triggers, "StateTrigger");
							Set(trigger, "Property", condition.PropertyName);
							Set(trigger, "Value", condition.Value.Text);
						}
						for (const auto& eventName : state.EventNames)
						{
							auto trigger = Append(_xml, triggers, "EventTrigger");
							Set(trigger, "Event", eventName);
						}
					}
					if (!state.Setters.empty())
					{
						auto setters = Append(
							_xml, stateElement, "VisualState.Setters");
						for (const auto& setter : state.Setters)
							WriteVisualStateSetter(setters, setter);
					}
					if (!state.Animations.empty())
					{
						auto storyboardProperty = Append(
							_xml, stateElement, "VisualState.Storyboard");
						auto storyboard = Append(
							_xml, storyboardProperty, "Storyboard");
						for (const auto& animation : state.Animations)
							WriteVisualStateAnimation(storyboard, component, animation);
					}
				}
			}
		}

		void WriteEventTriggers(
			const Element& root,
			const DesignComponentDefinition& component,
			const std::wstring& rootElementName)
		{
			if (component.EventTriggers.empty()) return;
			auto triggers = Append(
				_xml, root, ToUtf8(rootElementName + L".Triggers"));
			for (const auto& source : component.EventTriggers)
			{
				auto trigger = Append(_xml, triggers, "EventTrigger");
				Set(trigger, "RoutedEvent", source.EventName);
				for (const auto& action : source.Actions)
				{
					const char* actionName = action.Kind
						== DesignerStoryboardActionKind::Begin
						? "BeginStoryboard"
						: action.Kind == DesignerStoryboardActionKind::Pause
							? "PauseStoryboard"
							: action.Kind == DesignerStoryboardActionKind::Resume
								? "ResumeStoryboard" : "StopStoryboard";
					auto item = Append(_xml, trigger, actionName);
					if (action.Kind == DesignerStoryboardActionKind::Begin)
					{
						if (!action.StoryboardName.empty())
							Set(item, "x:Name", action.StoryboardName);
						auto storyboard = Append(_xml, item, "Storyboard");
						for (const auto& animation : action.Animations)
							WriteVisualStateAnimation(
								storyboard, component, animation);
					}
					else Set(item, "BeginStoryboardName", action.StoryboardName);
				}
			}
		}

		void WriteStyleTriggerActions(
			const Element& trigger,
			const std::string& triggerName,
			const std::string& propertyName,
			const std::vector<DesignerEventTriggerAction>& actions,
			const DesignComponentDefinition& target)
		{
			if (actions.empty()) return;
			auto property = Append(
				_xml, trigger, triggerName + "." + propertyName);
			for (const auto& action : actions)
			{
				const char* actionName = action.Kind
					== DesignerStoryboardActionKind::Begin
					? "BeginStoryboard"
					: action.Kind == DesignerStoryboardActionKind::Pause
						? "PauseStoryboard"
					: action.Kind == DesignerStoryboardActionKind::Resume
						? "ResumeStoryboard" : "StopStoryboard";
				auto item = Append(_xml, property, actionName);
				if (action.Kind == DesignerStoryboardActionKind::Begin)
				{
					if (!action.StoryboardName.empty())
						Set(item, "x:Name", action.StoryboardName);
					auto storyboard = Append(_xml, item, "Storyboard");
					for (const auto& animation : action.Animations)
						WriteVisualStateAnimation(storyboard, target, animation);
				}
				else Set(item, "BeginStoryboardName", action.StoryboardName);
			}
		}

		void WriteDataTrigger(
			const Element& parent,
			const std::vector<DesignerStyleDataCondition>& conditions,
			const std::vector<DesignerStyleSetter>& setters,
			const std::vector<DesignerEventTriggerAction>& enterActions,
			const std::vector<DesignerEventTriggerAction>& exitActions,
			const DesignComponentDefinition& target)
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
			const auto triggerName = conditions.size() == 1
				? std::string("DataTrigger") : std::string("MultiDataTrigger");
			WriteStyleTriggerActions(trigger, triggerName,
				"EnterActions", enterActions, target);
			WriteStyleTriggerActions(trigger, triggerName,
				"ExitActions", exitActions, target);
			for (const auto& setter : setters)
				WriteStyleSetter(trigger, setter);
		}

		void WriteStyleRule(
			const Element& resourceTarget,
			const DesignerStyleRule& rule,
			const DesignNode* scopeNode = nullptr)
		{
			DesignComponentDefinition styleTarget;
			styleTarget.BaseType = rule.HasType ? rule.Type : UIClass::UI_Base;
			if (!rule.ComponentType.Empty())
				if (const auto* component = scopeNode
					? _document.FindComponent(
						_document.Nodes, *scopeNode, rule.ComponentType)
					: _document.FindComponent(rule.ComponentType))
					styleTarget = *component;
			auto style = Append(_xml, resourceTarget, "Style");
			if (rule.HasType)
				Set(style, "TargetType", rule.ComponentType.Empty()
					? BuiltInXamlTypeName(rule.Type, rule.XamlType)
					: rule.ComponentType.XamlPrefix + L":"
						+ rule.ComponentType.XamlName);
			if (!rule.Id.empty()) Set(style, "x:Key", rule.Id);
			if (!rule.BasedOn.empty())
				Set(style, "BasedOn", L"{StaticResource "
					+ rule.BasedOn + L"}");
			for (const auto& setter : rule.Setters)
				WriteStyleSetter(style, setter);
			if (rule.Triggers.empty()) return;
			auto triggers = Append(_xml, style, "Style.Triggers");
			for (const auto& trigger : rule.Triggers)
			{
				if (!trigger.DataConditions.empty())
				{
					WriteDataTrigger(
						triggers, trigger.DataConditions, trigger.Setters,
						trigger.EnterActions, trigger.ExitActions, styleTarget);
					continue;
				}
				const size_t conditionCount = trigger.PropertyConditions.size();
				const bool multi = conditionCount > 1;
				auto triggerElement = Append(
					_xml, triggers, multi ? "MultiTrigger" : "Trigger");
				if (multi)
				{
					auto conditions = Append(
						_xml, triggerElement, "MultiTrigger.Conditions");
					for (const auto& condition : trigger.PropertyConditions)
					{
						auto item = Append(_xml, conditions, "Condition");
						Set(item, "Property", condition.Property);
						Set(item, "Value", condition.Value.Text);
					}
				}
				else if (!trigger.PropertyConditions.empty())
				{
					Set(triggerElement, "Property",
						trigger.PropertyConditions.front().Property);
					Set(triggerElement, "Value",
						trigger.PropertyConditions.front().Value.Text);
				}
				WriteStyleTriggerActions(triggerElement,
					multi ? "MultiTrigger" : "Trigger", "EnterActions",
					trigger.EnterActions, styleTarget);
				WriteStyleTriggerActions(triggerElement,
					multi ? "MultiTrigger" : "Trigger", "ExitActions",
					trigger.ExitActions, styleTarget);
				for (const auto& setter : trigger.Setters)
					WriteStyleSetter(triggerElement, setter);
			}
		}

		void WriteResources(const Element& root)
		{
			if (_document.StyleSheet.Empty() && _document.Components.empty()
				&& _document.ControlTemplates.empty()
				&& _document.DataTypes.empty() && _document.DataTemplates.empty()
				&& _document.ItemsPanelTemplates.empty()
				&& _document.GroupStyles.empty()
				&& _document.DataLists.empty()
				&& _document.CollectionViews.empty()) return;
			auto resources = Append(_xml, root, "Window.Resources");
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
			for (const auto& type : _document.DataTypes)
			{
				if (!type.SourceDictionary.empty()) continue;
				auto definition = Append(_xml, resourceTarget, "DataType");
				Set(definition, "x:Key", type.Name);
				auto properties = Append(_xml, definition, "DataType.Properties");
				auto schema = type.Properties;
				DesignerDataContextSchemaUtils::Canonicalize(schema);
				for (const auto& property : schema)
				{
					auto item = Append(_xml, properties, "Property");
					Set(item, "Path", property.Path);
					Set(item, "Kind",
						DesignerDataContextSchemaUtils::ValueKindName(property.ValueKind));
					if (property.ValueKind == BindingValueKind::Object)
						Set(item, "ObjectType",
							DesignerDataContextSchemaUtils::ObjectKindName(
								property.ObjectKind));
					if (!property.ItemType.empty()) Set(item, "ItemType", property.ItemType);
					if (!property.DataType.empty()) Set(item, "DataType", property.DataType);
					item->SetAttribute("CanRead", BoolText(property.CanRead));
					item->SetAttribute("CanWrite", BoolText(property.CanWrite));
					item->SetAttribute("CanObserve", BoolText(property.CanObserve));
				}
			}
			for (const auto& list : _document.DataLists)
			{
				if (!list.SourceDictionary.empty()) continue;
				auto definition = Append(_xml, resourceTarget, "DataList");
				Set(definition, "x:Key", list.Key);
				Set(definition, "ItemType", list.ItemType);
				for (const auto& record : list.Records)
				{
					auto item = Append(_xml, definition, "DataRecord");
					for (const auto& [path, value] : record.Fields)
					{
						if (path.find(L'.') == std::wstring::npos)
							item->SetAttribute(ToUtf8(path), ToUtf8(value));
						else
						{
							auto field = Append(_xml, item, "Field");
							Set(field, "Path", path);
							Set(field, "Value", value);
						}
					}
				}
			}
			for (const auto& view : _document.CollectionViews)
			{
				if (!view.SourceDictionary.empty()) continue;
				auto definition = Append(
					_xml, resourceTarget, "CollectionViewSource");
				Set(definition, "x:Key", view.Key);
				Set(definition, "Source", !view.SourceResource.empty()
					? L"{StaticResource " + view.SourceResource + L"}"
					: L"{Binding " + view.SourceBindingPath + L"}");
				if (!view.GroupDescriptions.empty())
				{
					auto groups = Append(_xml, definition,
						"CollectionViewSource.GroupDescriptions");
					for (const auto& group : view.GroupDescriptions)
					{
						auto item = Append(_xml, groups, "GroupDescription");
						Set(item, "PropertyName", group.PropertyName);
						if (group.Direction == CollectionSortDirection::Descending)
							Set(item, "Direction", L"Descending");
						if (!group.IgnoreCase) Set(item, "IgnoreCase", L"false");
					}
				}
				if (!view.FilterDescriptions.empty())
				{
					auto filters = Append(_xml, definition,
						"CollectionViewSource.FilterDescriptions");
					for (const auto& filter : view.FilterDescriptions)
					{
						auto item = Append(_xml, filters, "FilterDescription");
						Set(item, "PropertyName", filter.PropertyName);
						const wchar_t* name = L"Equals";
						switch (filter.Operator)
						{
						case CollectionFilterOperator::NotEquals: name = L"NotEquals"; break;
						case CollectionFilterOperator::LessThan: name = L"LessThan"; break;
						case CollectionFilterOperator::LessThanOrEqual: name = L"LessThanOrEqual"; break;
						case CollectionFilterOperator::GreaterThan: name = L"GreaterThan"; break;
						case CollectionFilterOperator::GreaterThanOrEqual: name = L"GreaterThanOrEqual"; break;
						case CollectionFilterOperator::Contains: name = L"Contains"; break;
						case CollectionFilterOperator::StartsWith: name = L"StartsWith"; break;
						case CollectionFilterOperator::EndsWith: name = L"EndsWith"; break;
						case CollectionFilterOperator::IsEmpty: name = L"IsEmpty"; break;
						case CollectionFilterOperator::IsNotEmpty: name = L"IsNotEmpty"; break;
						case CollectionFilterOperator::Equals: break;
						}
						if (filter.Operator != CollectionFilterOperator::Equals)
							Set(item, "Operator", name);
						if (filter.Operator != CollectionFilterOperator::IsEmpty
							&& filter.Operator != CollectionFilterOperator::IsNotEmpty)
							Set(item, "Value", filter.Value);
						if (!filter.IgnoreCase) Set(item, "IgnoreCase", L"false");
					}
				}
				if (!view.AggregateDescriptions.empty())
				{
					auto aggregates = Append(_xml, definition,
						"CollectionViewSource.AggregateDescriptions");
					for (const auto& aggregate : view.AggregateDescriptions)
					{
						auto item = Append(_xml, aggregates, "AggregateDescription");
						Set(item, "Name", aggregate.Name);
						if (!aggregate.PropertyName.empty())
							Set(item, "PropertyName", aggregate.PropertyName);
						const wchar_t* function = L"Count";
						switch (aggregate.Function)
						{
						case CollectionAggregateFunction::Sum: function = L"Sum"; break;
						case CollectionAggregateFunction::Average: function = L"Average"; break;
						case CollectionAggregateFunction::Min: function = L"Min"; break;
						case CollectionAggregateFunction::Max: function = L"Max"; break;
						case CollectionAggregateFunction::Count: break;
						}
						if (aggregate.Function != CollectionAggregateFunction::Count)
							Set(item, "Function", function);
					}
				}
				if (!view.SortDescriptions.empty())
				{
					auto sorts = Append(_xml, definition,
						"CollectionViewSource.SortDescriptions");
					for (const auto& sort : view.SortDescriptions)
					{
						auto item = Append(_xml, sorts, "SortDescription");
						Set(item, "PropertyName", sort.PropertyName);
						if (sort.Direction == CollectionSortDirection::Descending)
							Set(item, "Direction", L"Descending");
						if (!sort.IgnoreCase) Set(item, "IgnoreCase", L"false");
					}
				}
			}
			for (const auto& component : _document.Components)
			{
				if (!component.SourceDictionary.empty()) continue;
				auto definition = Append(
					_xml, resourceTarget, "ComponentDefinition");
				Set(definition, "x:Key", component.Type.XamlPrefix
					+ L":" + component.Type.XamlName);
				Set(definition, "BaseType",
					DesignerStyleSheetUtils::UIClassName(component.BaseType));
				if (!component.DisplayName.empty()
					&& component.DisplayName != component.Type.XamlName)
					Set(definition, "DisplayName", component.DisplayName);
				if (!component.Category.empty() && component.Category != L"Components")
					Set(definition, "Category", component.Category);
				if (!component.Properties.empty())
				{
					auto properties = Append(
						_xml, definition, "ComponentDefinition.Properties");
					for (const auto& property : component.Properties)
					{
						auto item = Append(_xml, properties, "ComponentProperty");
						Set(item, "Name", property.Name);
						Set(item, "Type", property.Choices.empty()
							? DesignerStyleSheetUtils::ValueKindName(property.DefaultValue.Kind)
							: L"Enum");
						if (!property.DefaultResourceKey.empty())
							Set(item, "Default", L"{StaticResource "
								+ property.DefaultResourceKey + L"}");
						else if (property.DefaultValue.ObjectValue.is_null())
							Set(item, "Default", property.DefaultValue.Text);
						if (!property.DisplayName.empty()
							&& property.DisplayName != property.Name)
							Set(item, "DisplayName", property.DisplayName);
						if (!property.Category.empty() && property.Category != L"Component")
							Set(item, "Category", property.Category);
						if (property.CategoryOrder != 500)
							Set(item, "CategoryOrder", std::to_wstring(property.CategoryOrder));
						if (property.Order != 0)
							Set(item, "Order", std::to_wstring(property.Order));
						const wchar_t* editor = nullptr;
						switch (property.Editor)
						{
						case DependencyPropertyEditorKind::Text: editor = L"Text"; break;
						case DependencyPropertyEditorKind::Boolean: editor = L"Boolean"; break;
						case DependencyPropertyEditorKind::Number: editor = L"Number"; break;
						case DependencyPropertyEditorKind::Choice: editor = L"Choice"; break;
						case DependencyPropertyEditorKind::Color: editor = L"Color"; break;
						case DependencyPropertyEditorKind::Thickness: editor = L"Thickness"; break;
						case DependencyPropertyEditorKind::Size: editor = L"Size"; break;
						case DependencyPropertyEditorKind::Length: editor = L"Length"; break;
						case DependencyPropertyEditorKind::Auto: break;
						}
						if (editor) Set(item, "Editor", editor);
						if (property.Minimum) Set(item, "Minimum", NumberText(*property.Minimum));
						if (property.Maximum) Set(item, "Maximum", NumberText(*property.Maximum));
						if (property.Step) Set(item, "Step", NumberText(*property.Step));
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::AffectsMeasure))
							Set(item, "AffectsMeasure", L"true");
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::AffectsArrange))
							Set(item, "AffectsArrange", L"true");
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::AffectsRender))
							Set(item, "AffectsRender", L"true");
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::AffectsParentMeasure))
							Set(item, "AffectsParentMeasure", L"true");
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::AffectsParentArrange))
							Set(item, "AffectsParentArrange", L"true");
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::Inherits))
							Set(item, "Inherits", L"true");
						if (HasDependencyPropertyFlag(
							property.Flags, DependencyPropertyFlags::BindsTwoWayByDefault))
							Set(item, "BindsTwoWayByDefault", L"true");
						if (property.IsReadOnly)
							Set(item, "ReadOnly", L"true");
						if (property.DefaultUpdateMode
							!= DataSourceUpdateMode::OnPropertyChanged)
							Set(item, "DefaultUpdateSourceTrigger",
								DesignerBindingUtils::UpdateSourceTriggerName(
									property.DefaultUpdateMode));
						if (property.DefaultResourceKey.empty()
							&& !property.DefaultValue.ObjectValue.is_null())
						{
							auto value = Append(
								_xml, item, "ComponentProperty.Default");
							if (property.DefaultValue.Kind == DesignerStyleValueKind::Brush)
								WriteBrush(value, property.DefaultValue.ObjectValue);
							else if (property.DefaultValue.Kind == DesignerStyleValueKind::Geometry)
								WriteGeometryObject(value, property.DefaultValue.ObjectValue);
							else if (property.DefaultValue.Kind == DesignerStyleValueKind::Transform)
								WriteTransformObject(value, property.DefaultValue.ObjectValue);
						}
						if (!property.Choices.empty())
						{
							auto choices = Append(
								_xml, item, "ComponentProperty.Choices");
							for (const auto& choice : property.Choices)
							{
								auto value = Append(_xml, choices, "ComponentChoice");
								Set(value, "Value", choice.Value);
								if (!choice.DisplayName.empty()
									&& choice.DisplayName != choice.Value)
									Set(value, "DisplayName", choice.DisplayName);
							}
						}
					}
				}
				if (!component.ContentProperties.empty())
				{
					auto properties = Append(
						_xml, definition, "ComponentDefinition.ContentProperties");
					for (const auto& property : component.ContentProperties)
					{
						auto item = Append(_xml, properties, "ComponentContentProperty");
						Set(item, "Name", property.Name);
						if (!property.DisplayName.empty()
							&& property.DisplayName != property.Name)
							Set(item, "DisplayName", property.DisplayName);
						Set(item, "Cardinality",
							property.Cardinality == DesignerComponentContentCardinality::Multiple
								? L"Multiple" : L"Single");
						if (property.IsDefault) Set(item, "Default", L"true");
					}
				}
				if (!component.Events.empty())
				{
					auto events = Append(
						_xml, definition, "ComponentDefinition.Events");
					for (const auto& event : component.Events)
					{
						auto item = Append(_xml, events, "ComponentEvent");
						Set(item, "Name", event.Name);
						if (!event.DisplayName.empty()
							&& event.DisplayName != event.Name)
							Set(item, "DisplayName", event.DisplayName);
						if (event.Category != DesignerEventCategory::Other)
							Set(item, "Category", FromUtf8(
								DesignerEventCatalog::GetCategoryName(event.Category)));
						if (event.Payload != DesignerComponentEventPayload::None)
							Set(item, "Payload", FromUtf8(
								DesignerEventCatalog::GetComponentPayloadName(
									event.Payload)));
						if (event.RoutingStrategy
							!= DeclarativeEventRoutingStrategy::Direct)
							Set(item, "RoutingStrategy", FromUtf8(
								DesignerEventCatalog::GetComponentRoutingStrategyName(
									event.RoutingStrategy)));
						if (event.Order != 0)
							Set(item, "Order", std::to_wstring(event.Order));
						if (event.IsDefault) Set(item, "Default", L"true");
					}
				}
				if (!component.Template.empty())
				{
					auto templateElement = Append(
						_xml, definition, "ComponentDefinition.Template");
					DesignDocument templateDocument = _document;
					templateDocument.Nodes = component.Template;
					templateDocument.Window.Events.clear();
					templateDocument.RecalculateNextStableId();
					Writer templateWriter(templateDocument, _xml, &component);
					templateWriter.WriteControlForest(templateElement);
				}
			}
			for (const auto& item : _document.ControlTemplates)
			{
				if (!item.SourceDictionary.empty()) continue;
				auto definition = Append(
					_xml, resourceTarget, "ControlTemplate");
				if (!item.IsImplicit()) Set(definition, "x:Key", item.Key);
				Set(definition, "TargetType",
					item.TargetComponentType.Empty()
						? DesignerStyleSheetUtils::UIClassName(item.TargetType)
						: item.TargetComponentType.XamlPrefix + L":"
							+ item.TargetComponentType.XamlName);
				DesignDocument templateDocument = _document;
				templateDocument.Nodes = item.Template;
				templateDocument.Window.Events.clear();
				templateDocument.RecalculateNextStableId();
				DesignComponentDefinition templateContext;
				templateContext.BaseType = item.TargetType;
				templateContext.Template = item.Template;
				templateContext.VisualStateGroups = item.VisualStateGroups;
				templateContext.EventTriggers = item.EventTriggers;
				Writer templateWriter(
					templateDocument, _xml, &templateContext);
				templateWriter.WriteControlForest(definition);
			}
			for (const auto& item : _document.ItemsPanelTemplates)
			{
				if (!item.SourceDictionary.empty()) continue;
				auto definition = Append(
					_xml, resourceTarget, "ItemsPanelTemplate");
				Set(definition, "x:Key", item.Key);
				const char* panelName = item.Value.Kind == ItemsPanelKind::Wrap
					? "WrapPanel"
					: item.Value.Kind == ItemsPanelKind::VirtualizingStack
						? "VirtualizingStackPanel" : "StackPanel";
				auto panel = Append(_xml, definition, panelName);
				const auto defaultOrientation = item.Value.Kind == ItemsPanelKind::Wrap
					? Orientation::Horizontal : Orientation::Vertical;
				if (item.Value.Orientation != defaultOrientation)
					Set(panel, "Orientation",
						item.Value.Orientation == Orientation::Vertical
							? L"Vertical" : L"Horizontal");
				if (item.Value.ItemWidth != 0.0f)
					Set(panel, "ItemWidth", NumberText(item.Value.ItemWidth));
				if (item.Value.ItemHeight != 0.0f)
					Set(panel, "ItemHeight", NumberText(item.Value.ItemHeight));
				if (item.Value.Kind == ItemsPanelKind::VirtualizingStack
					&& item.Value.CacheLength != 1.0f)
					Set(panel, "CacheLength", NumberText(item.Value.CacheLength));
			}
			for (const auto& item : _document.DataTemplates)
			{
				if (!item.SourceDictionary.empty()) continue;
				auto definition = Append(_xml, resourceTarget,
					item.Hierarchical
						? "HierarchicalDataTemplate" : "DataTemplate");
				if (!item.IsImplicit()) Set(definition, "x:Key", item.Key);
				Set(definition, "DataType", item.DataType);
				if (item.ItemsSourceBinding)
					Set(definition, "ItemsSource",
						BindingMarkup(*item.ItemsSourceBinding));
				DesignDocument templateDocument = _document;
				templateDocument.Nodes = item.Template;
				if (const auto* dataType = _document.FindDataType(item.DataType))
					templateDocument.DataContextSchema = dataType->Properties;
				templateDocument.Window.Events.clear();
				templateDocument.RecalculateNextStableId();
				Writer templateWriter(templateDocument, _xml);
				templateWriter.WriteControlForest(definition);
			}
			for (const auto& item : _document.GroupStyles)
			{
				if (!item.SourceDictionary.empty()) continue;
				auto definition = Append(_xml, resourceTarget, "GroupStyle");
				Set(definition, "x:Key", item.Key);
				if (!item.HeaderTemplate.empty())
					Set(definition, "HeaderTemplate", L"{StaticResource "
						+ item.HeaderTemplate + L"}");
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
				DesignComponentDefinition styleTarget;
				styleTarget.BaseType = rule.HasType ? rule.Type : UIClass::UI_Base;
				if (!rule.ComponentType.Empty())
					if (const auto* component = _document.FindComponent(
						rule.ComponentType)) styleTarget = *component;
				auto style = Append(_xml, resourceTarget, "Style");
				if (rule.HasType)
					Set(style, "TargetType", rule.ComponentType.Empty()
						? BuiltInXamlTypeName(rule.Type, rule.XamlType)
						: rule.ComponentType.XamlPrefix + L":"
							+ rule.ComponentType.XamlName);
				if (!rule.Id.empty()) Set(style, "x:Key", rule.Id);
				if (!rule.BasedOn.empty())
					Set(style, "BasedOn", L"{StaticResource "
						+ rule.BasedOn + L"}");
				if (rule.DataConditions.empty() && rule.PropertyConditions.empty())
					for (const auto& setter : rule.Setters)
						WriteStyleSetter(style, setter);
				if (!rule.Triggers.empty() || !rule.DataConditions.empty()
					|| !rule.PropertyConditions.empty())
				{
					auto triggers = Append(_xml, style, "Style.Triggers");
					if (!rule.DataConditions.empty())
						WriteDataTrigger(triggers, rule.DataConditions, rule.Setters,
							rule.EnterActions, rule.ExitActions, styleTarget);
					if (!rule.PropertyConditions.empty())
					{
						const bool multi = rule.PropertyConditions.size() > 1;
						auto triggerElement = Append(
							_xml, triggers, multi ? "MultiTrigger" : "Trigger");
						if (multi)
						{
							auto conditions = Append(
								_xml, triggerElement, "MultiTrigger.Conditions");
							for (const auto& condition : rule.PropertyConditions)
							{
								auto item = Append(_xml, conditions, "Condition");
								Set(item, "Property", condition.Property);
								Set(item, "Value", condition.Value.Text);
							}
						}
						else
						{
							Set(triggerElement, "Property",
								rule.PropertyConditions.front().Property);
							Set(triggerElement, "Value",
								rule.PropertyConditions.front().Value.Text);
						}
						WriteStyleTriggerActions(triggerElement,
							multi ? "MultiTrigger" : "Trigger", "EnterActions",
							rule.EnterActions, styleTarget);
						WriteStyleTriggerActions(triggerElement,
							multi ? "MultiTrigger" : "Trigger", "ExitActions",
							rule.ExitActions, styleTarget);
						for (const auto& setter : rule.Setters)
							WriteStyleSetter(triggerElement, setter);
					}
					for (const auto& trigger : rule.Triggers)
					{
						if (!trigger.DataConditions.empty())
						{
							WriteDataTrigger(
								triggers, trigger.DataConditions, trigger.Setters,
								trigger.EnterActions, trigger.ExitActions, styleTarget);
							continue;
						}
						const size_t conditionCount = trigger.PropertyConditions.size();
						const bool multi = conditionCount > 1;
						auto triggerElement = Append(
							_xml, triggers, multi ? "MultiTrigger" : "Trigger");
						if (multi)
						{
							auto conditions = Append(
								_xml, triggerElement, "MultiTrigger.Conditions");
							for (const auto& condition : trigger.PropertyConditions)
							{
								auto conditionElement = Append(_xml, conditions, "Condition");
								Set(conditionElement, "Property", condition.Property);
								Set(conditionElement, "Value", condition.Value.Text);
							}
						}
						else if (!trigger.PropertyConditions.empty())
						{
							Set(triggerElement, "Property",
								trigger.PropertyConditions.front().Property);
							Set(triggerElement, "Value",
								trigger.PropertyConditions.front().Value.Text);
						}
						WriteStyleTriggerActions(triggerElement,
							multi ? "MultiTrigger" : "Trigger", "EnterActions",
							trigger.EnterActions, styleTarget);
						WriteStyleTriggerActions(triggerElement,
							multi ? "MultiTrigger" : "Trigger", "ExitActions",
							trigger.ExitActions, styleTarget);
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
			if (node.Type != UIClass::UI_Grid || !residual.is_object()) return;
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

		void WriteLocalResources(
			const DesignNode& node,
			const Element& element,
			const std::wstring& ownerName)
		{
			if (node.LocalResources.Empty()
				&& node.LocalObjectResources.Empty()) return;
			auto resources = Append(
				_xml, element, ToUtf8(ownerName + L".Resources"));
			Element target = resources;
			if (!node.LocalResources.MergedDictionaries.empty())
			{
				auto dictionary = Append(_xml, resources, "ResourceDictionary");
				auto merged = Append(
					_xml, dictionary, "ResourceDictionary.MergedDictionaries");
				for (const auto& source
					: node.LocalResources.MergedDictionaries)
				{
					auto item = Append(_xml, merged, "ResourceDictionary");
					Set(item, "Source", source);
				}
				target = dictionary;
			}
			for (const auto& resource : node.LocalResources.Resources)
			{
				if (!resource.SourceDictionary.empty()) continue;
				if (resource.Value.Kind == DesignerStyleValueKind::Brush)
				{
					WriteBrush(target, resource.Value.ObjectValue, resource.Key);
					continue;
				}
				if (resource.Value.Kind == DesignerStyleValueKind::ImageSource)
				{
					auto item = Append(_xml, target, "BitmapImage");
					Set(item, "x:Key", resource.Key);
					Set(item, "UriSource", resource.Value.Text);
					continue;
				}
				if (resource.Value.Kind == DesignerStyleValueKind::Geometry)
				{
					WriteGeometryObject(
						target, resource.Value.ObjectValue, resource.Key);
					continue;
				}
				if (resource.Value.Kind == DesignerStyleValueKind::Transform)
				{
					WriteTransformObject(
						target, resource.Value.ObjectValue, resource.Key);
					continue;
				}
				auto item = Append(_xml, target,
					ToUtf8(DesignerStyleSheetUtils::ValueKindName(
						resource.Value.Kind)));
				Set(item, "x:Key", resource.Key);
				item->SetInnerText(ToUtf8(resource.Value.Text));
			}
			for (const auto& rule : node.LocalResources.Rules)
				if (rule.SourceDictionary.empty())
					WriteStyleRule(target, rule, &node);
			WriteLocalObjectResources(node, target);
		}

		void WriteLocalObjectResources(
			const DesignNode& node,
			const Element& target)
		{
			if (node.LocalObjectResources.Empty()) return;
			DesignDocument scoped;
			scoped.Window.Name = L"LocalObjectResourceScope";
			scoped.ResourceBasePath = _document.ResourceBasePath;
			scoped.Resources = _document.Resources;
			scoped.DataContextSchema = _document.DataContextSchema;
			scoped.DataTypes = _document.DataTypes;
			scoped.DataLists = _document.DataLists;
			scoped.CollectionViews = _document.CollectionViews;
			auto visible = _document.VisibleObjectResources(
				_document.Nodes, node);
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
					{ return Equals(current.Key, local.Key); }),
					visible.ItemsPanelTemplates.end());
			for (const auto& local : node.LocalObjectResources.GroupStyles)
				visible.GroupStyles.erase(std::remove_if(
					visible.GroupStyles.begin(), visible.GroupStyles.end(),
					[&](const auto& current)
					{ return Equals(current.Key, local.Key); }),
					visible.GroupStyles.end());
			for (auto& component : visible.Components)
				component.SourceDictionary = L"__cui_lexical_context__";
			for (auto& dataTemplate : visible.DataTemplates)
				dataTemplate.SourceDictionary = L"__cui_lexical_context__";
			for (auto& controlTemplate : visible.ControlTemplates)
				controlTemplate.SourceDictionary = L"__cui_lexical_context__";
			for (auto& itemsPanel : visible.ItemsPanelTemplates)
				itemsPanel.SourceDictionary = L"__cui_lexical_context__";
			for (auto& groupStyle : visible.GroupStyles)
				groupStyle.SourceDictionary = L"__cui_lexical_context__";
			scoped.Components = std::move(visible.Components);
			scoped.ControlTemplates = std::move(visible.ControlTemplates);
			scoped.DataTemplates = std::move(visible.DataTemplates);
			scoped.ItemsPanelTemplates = std::move(visible.ItemsPanelTemplates);
			scoped.GroupStyles = std::move(visible.GroupStyles);
			scoped.Components.insert(scoped.Components.end(),
				node.LocalObjectResources.Components.begin(),
				node.LocalObjectResources.Components.end());
			scoped.DataTemplates.insert(scoped.DataTemplates.end(),
				node.LocalObjectResources.DataTemplates.begin(),
				node.LocalObjectResources.DataTemplates.end());
			scoped.ControlTemplates.insert(scoped.ControlTemplates.end(),
				node.LocalObjectResources.ControlTemplates.begin(),
				node.LocalObjectResources.ControlTemplates.end());
			scoped.ItemsPanelTemplates.insert(scoped.ItemsPanelTemplates.end(),
				node.LocalObjectResources.ItemsPanelTemplates.begin(),
				node.LocalObjectResources.ItemsPanelTemplates.end());
			scoped.GroupStyles.insert(scoped.GroupStyles.end(),
				node.LocalObjectResources.GroupStyles.begin(),
				node.LocalObjectResources.GroupStyles.end());

			XmlDocument temporary;
			Writer writer(scoped, temporary);
			const auto root = writer.Write();
			Element resources;
			for (const auto& child : root->ChildNodes())
			{
				auto element = std::dynamic_pointer_cast<XmlElement>(child);
				if (element && element->Name() == "Window.Resources")
				{
					resources = std::move(element);
					break;
				}
			}
			if (!resources) return;
			for (const auto& child : resources->ChildNodes())
			{
				auto element = std::dynamic_pointer_cast<XmlElement>(child);
				if (!element || (element->Name() != "ComponentDefinition"
					&& element->Name() != "DataTemplate"
					&& element->Name() != "ControlTemplate"
					&& element->Name() != "ItemsPanelTemplate"
					&& element->Name() != "GroupStyle")) continue;
				target->AppendChild(_xml.ImportNode(*element, true));
			}
		}

		void WriteControl(
			const DesignNode& node,
			const Element& parent,
			bool consumePlacementMarker)
		{
			if (!_written.insert(node.Id).second)
				throw std::invalid_argument("XAML control was written more than once");
			const auto elementName = ControlXamlTypeName(node);
			auto element = Append(_xml, parent, ToUtf8(elementName));
			Set(element, "x:Name", node.Name);
			element->SetAttribute("DesignId", std::to_string(node.Id));
			if (node.Locked) Set(element, "d:Locked", L"true");
			if (!node.PresentedComponentContent.empty())
				Set(element, "ComponentSlot.Presents",
					node.PresentedComponentContent);
			if (!node.TemplateContentSource.empty())
				Set(element, "ContentSource", node.TemplateContentSource);

			DesignNodeProperties residualProperties = node.Properties;
			DesignValue residualExtra = EncodeDesignNodeStructure(
				node.Type, node.Structure);
			if (residualExtra.contains("commandTarget"))
			{
				if ((node.Type != UIClass::UI_Button
						&& node.Type != UIClass::UI_MenuItem)
					|| !residualExtra["commandTarget"].is_string())
					throw std::invalid_argument(
						"CommandTarget is only valid on Button or MenuItem");
				const auto target = FromUtf8(
					residualExtra["commandTarget"].get<std::string>());
				if (target.empty())
					throw std::invalid_argument(
						"CommandTarget must name an element");
				Set(element, "CommandTarget",
					L"{x:Reference " + target + L"}");
				residualExtra.ObjectItems().erase("commandTarget");
			}
			DesignBindingMap residualBindings = node.Bindings;
			WriteControlAttributes(
				node, element, residualProperties, residualBindings);
			WriteRelativePanelConstraints(element, residualExtra);
			WriteLocalResources(node, element, elementName);
			WriteMultiBindingProperties(node, element, residualBindings);
			WriteGridDefinitions(node, element, residualExtra);
			WriteStructuredProperties(
				node, element, residualProperties, residualExtra);
			WriteCommandAndInputBindings(node, element, elementName);
			if (_templateComponent && node.ParentId == 0
				&& node.ParentRef.empty())
			{
				WriteEventTriggers(
					element, *_templateComponent, elementName);
				WriteVisualStateGroups(element, *_templateComponent);
			}
			if (consumePlacementMarker && residualExtra.is_object())
			{
				residualExtra.ObjectItems().erase("headeredRegion");
			}
			if (!residualProperties.Empty())
				throw std::invalid_argument(
					"Control contains properties without a public XAML representation");
			if (!residualBindings.empty())
				throw std::invalid_argument(
					"Control contains bindings without a public XAML representation");

			if (!residualExtra.empty())
			{
				std::string message = "Control '"
					+ ToUtf8(node.Name)
					+ "' contains structured data without a public XAML representation";
				if (residualExtra.is_object())
				{
					message += ": ";
					bool first = true;
					for (const auto& [key, ignored]
						: residualExtra.ObjectItems())
					{
						(void)ignored;
						if (!first) message += ", ";
						message += key;
						first = false;
					}
				}
				throw std::invalid_argument(std::move(message));
			}

			if (IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, node.Type)
				|| IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, node.Type))
				WriteHeaderedChildren(node, element);
			else if (!node.ComponentType.Empty())
			{
				const auto* component = _document.FindComponent(
					_document.Nodes, node, node.ComponentType);
				if (!component || component->ContentProperties.empty())
				{
					for (const auto graphIndex : _graph.ChildrenOf(node.Name))
						WriteControl(
							_document.Nodes[_graph.Nodes()[graphIndex].SourceIndex],
							element, false);
				}
				else
				{
					for (const auto& property : component->ContentProperties)
					{
						std::vector<const DesignNode*> children;
						for (const auto graphIndex : _graph.ChildrenOf(node.Name))
						{
							const auto& child = _document.Nodes[
								_graph.Nodes()[graphIndex].SourceIndex];
							if (Equals(child.ComponentContentProperty, property.Name))
								children.push_back(&child);
						}
						if (children.empty()) continue;
						if (property.IsDefault)
						{
							for (const auto* child : children)
								WriteControl(*child, element, false);
						}
						else
						{
							auto propertyElement = Append(_xml, element, ToUtf8(
								component->Type.XamlPrefix + L":"
								+ component->Type.XamlName + L"." + property.Name));
							for (const auto* child : children)
								WriteControl(*child, propertyElement, false);
						}
					}
					for (const auto graphIndex : _graph.ChildrenOf(node.Name))
					{
						const auto& child = _document.Nodes[
							_graph.Nodes()[graphIndex].SourceIndex];
						const auto contract = std::find_if(
							component->ContentProperties.begin(),
							component->ContentProperties.end(), [&](const auto& property)
							{
								return Equals(property.Name,
									child.ComponentContentProperty);
							});
						if (contract == component->ContentProperties.end())
							throw std::invalid_argument(
								"Component visual child has no content property");
					}
				}
			}
			else
			{
				for (const auto graphIndex : _graph.ChildrenOf(node.Name))
					WriteControl(
						_document.Nodes[_graph.Nodes()[graphIndex].SourceIndex],
						element, false);
			}
		}

		void WriteBindingElementAttributes(
			const Element& element,
			const DesignerDataBinding& binding,
			bool includeMode)
		{
			if (!binding.SourceProperty.empty())
				Set(element, "Path", binding.SourceProperty);
			if (includeMode)
			{
				if (binding.Mode != BindingMode::Default)
					Set(element, "Mode",
						DesignerBindingUtils::BindingModeName(binding.Mode));
				if (binding.UpdateMode != DataSourceUpdateMode::Default)
					Set(element, "UpdateSourceTrigger",
						DesignerBindingUtils::UpdateSourceTriggerName(
							binding.UpdateMode));
			}
			if (!binding.Converter.empty()) Set(element, "Converter", binding.Converter);
			if (binding.ConverterParameter)
				Set(element, "ConverterParameter", binding.ConverterParameter->Text);
			if (binding.StringFormat) Set(element, "StringFormat", *binding.StringFormat);
			if (!binding.ElementName.empty()) Set(element, "ElementName", binding.ElementName);
			if (binding.FallbackValue)
				Set(element, "FallbackValue", binding.FallbackValue->Text);
			if (binding.TargetNullValue)
				Set(element, "TargetNullValue", binding.TargetNullValue->Text);
			if (binding.RelativeSource == DesignerBindingRelativeSource::FindAncestor)
			{
				std::wstring relative = L"{RelativeSource FindAncestor, AncestorType={x:Type "
					+ binding.AncestorType + L"}";
				if (binding.AncestorLevel != 1)
					relative += L", AncestorLevel="
						+ std::to_wstring(binding.AncestorLevel);
				relative += L"}";
				Set(element, "RelativeSource", relative);
			}
			else if (binding.RelativeSource != DesignerBindingRelativeSource::None)
				Set(element, "RelativeSource",
					binding.RelativeSource == DesignerBindingRelativeSource::Self
						? L"{RelativeSource Self}"
						: L"{RelativeSource TemplatedParent}");
		}

		void WriteMultiBindingProperties(
			const DesignNode& node,
			const Element& element,
			DesignBindingMap& residualBindings)
		{
			std::vector<std::wstring> consumed;
			for (const auto& [property, binding] : residualBindings)
			{
				if (!binding.IsMultiBinding()) continue;
				const auto owner = ControlXamlTypeName(node);
				auto propertyElement = Append(_xml, element,
					ToUtf8(owner + L"." + PublicPropertyName(property)));
				auto multi = Append(_xml, propertyElement, "MultiBinding");
				WriteBindingElementAttributes(multi, binding, true);
				for (const auto& child : binding.ChildBindings)
				{
					auto childElement = Append(_xml, multi, "Binding");
					WriteBindingElementAttributes(childElement, child, true);
				}
				consumed.push_back(property);
			}
			for (const auto& property : consumed)
				residualBindings.erase(property);
		}

		void WriteRelativePanelConstraints(
			const Element& element,
			DesignValue& residualExtra)
		{
			if (!residualExtra.is_object()
				|| !residualExtra.contains(RelativePanelConstraintsKey)) return;
			const auto constraints = residualExtra[RelativePanelConstraintsKey];
			if (!constraints.is_object())
				throw std::invalid_argument("Invalid RelativePanel constraints");
			const std::pair<const char*, const wchar_t*> booleans[] = {
				{ "centerHorizontal", L"CenterHorizontal" },
				{ "centerVertical", L"CenterVertical" },
				{ "alignLeftWithPanel", L"AlignLeftWithPanel" },
				{ "alignTopWithPanel", L"AlignTopWithPanel" },
				{ "alignRightWithPanel", L"AlignRightWithPanel" },
				{ "alignBottomWithPanel", L"AlignBottomWithPanel" }
			};
			const std::pair<const char*, const wchar_t*> references[] = {
				{ "above", L"Above" }, { "below", L"Below" },
				{ "leftOf", L"LeftOf" }, { "rightOf", L"RightOf" },
				{ "alignLeftWith", L"AlignLeftWith" },
				{ "alignRightWith", L"AlignRightWith" },
				{ "alignTopWith", L"AlignTopWith" },
				{ "alignBottomWith", L"AlignBottomWith" }
			};
			std::unordered_set<std::string> consumed;
			for (const auto& [key, member] : booleans)
			{
				if (!constraints.contains(key)) continue;
				if (!constraints[key].is_boolean())
					throw std::invalid_argument(
						"Invalid RelativePanel Boolean constraint");
				Set(element, ToUtf8(L"RelativePanel." + std::wstring(member)).c_str(),
					constraints[key].get<bool>() ? L"true" : L"false");
				consumed.insert(key);
			}
			for (const auto& [key, member] : references)
			{
				if (!constraints.contains(key)) continue;
				if (!constraints[key].is_string()
					|| constraints[key].get<std::string>().empty())
					throw std::invalid_argument(
						"Invalid RelativePanel reference constraint");
				Set(element, ToUtf8(L"RelativePanel." + std::wstring(member)).c_str(),
					FromUtf8(constraints[key].get<std::string>()));
				consumed.insert(key);
			}
			for (const auto& [key, ignored] : constraints.ObjectItems())
			{
				(void)ignored;
				if (!consumed.contains(key))
					throw std::invalid_argument(
						"Unknown RelativePanel constraint");
			}
			residualExtra.ObjectItems().erase(RelativePanelConstraintsKey);
		}

		void WriteControlAttributes(
			const DesignNode& node,
			const Element& element,
			DesignNodeProperties& residual,
			DesignBindingMap& residualBindings)
		{
			std::map<std::wstring, std::wstring> attributes;
			if (!residual.StyleResourceKey.empty())
			{
				attributes[L"Style"] = L"{StaticResource "
					+ residual.StyleResourceKey + L"}";
				residual.StyleResourceKey.clear();
			}
			std::vector<std::wstring> consumedProperties;
			for (const auto& [property, assignment] : residual.Values)
			{
				const bool structuredObject =
					!assignment.Value.ObjectValue.is_null()
					&& assignment.ResourceKey.empty()
					&& assignment.DynamicResourceKey.empty()
					&& ((assignment.Value.Kind == DesignerStyleValueKind::Brush
							&& (Equals(property, L"Background")
								|| Equals(property, L"Foreground")
								|| Equals(property, L"BorderBrush")))
						|| (assignment.Value.Kind
								== DesignerStyleValueKind::Transform
							&& Equals(property, L"RenderTransform"))
						|| (assignment.Value.Kind
								== DesignerStyleValueKind::Geometry
							&& Equals(property, L"Clip")));
				if (structuredObject && !HasBinding(node, property)) continue;
				if (!HasBinding(node, property))
				{
					if (!assignment.DynamicResourceKey.empty())
						attributes[PublicPropertyName(property)] =
							L"{DynamicResource "
							+ assignment.DynamicResourceKey + L"}";
					else if (!assignment.ResourceKey.empty())
						attributes[PublicPropertyName(property)] =
							L"{StaticResource " + assignment.ResourceKey + L"}";
					else
						attributes[PublicPropertyName(property)] =
							PublicPropertyValue(
								node.Type, property, assignment.Value.Text);
				}
				consumedProperties.push_back(property);
			}
			for (const auto& property : consumedProperties)
				residual.Remove(property);

			for (const auto& [event, storedHandler] : node.Events)
			{
					const auto handler = DesignerEventCatalog::NormalizeHandlerName(storedHandler);
					if (handler.empty())
						throw std::invalid_argument(
							"Control event handler cannot be empty");
					const auto& storedName = event;
					DesignerComponentType ownerType;
					std::wstring routedEventName;
					std::wstring attributeName = storedName;
					if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
						storedName, ownerType, routedEventName))
					{
						const auto* owner = _document.FindComponent(
							_document.Nodes, node, ownerType);
						if (!owner || owner->Type.XamlPrefix.empty())
							throw std::invalid_argument(
								"Attached component event owner is missing");
						attributeName = owner->Type.XamlPrefix + L":"
							+ owner->Type.XamlName + L"." + routedEventName;
					}
					attributes[attributeName] = handler;
			}
			for (const auto& [sourceEvent, componentEvent]
				: node.TemplateEventBindings)
			{
				if (attributes.contains(sourceEvent))
					throw std::invalid_argument(
						"RaiseEvent conflicts with another event value");
				attributes[sourceEvent] = L"{RaiseEvent " + componentEvent + L"}";
			}
			std::vector<std::wstring> consumedBindings;
			for (const auto& [property, binding] : residualBindings)
			{
				if (binding.IsMultiBinding()) continue;
				attributes[PublicPropertyName(property)] = BindingMarkup(binding);
				consumedBindings.push_back(property);
			}
			for (const auto& property : consumedBindings)
				residualBindings.erase(property);
			for (const auto& [property, source] : node.TemplateBindings)
			{
				const auto name = PublicPropertyName(property);
				if (attributes.contains(name))
					throw std::invalid_argument(
						"TemplateBinding conflicts with another property value");
				attributes[name] = L"{TemplateBinding " + source + L"}";
			}

			for (const auto& [name, value] : attributes)
				Set(element, ToUtf8(name).c_str(), value);
		}

		void WriteCommandAndInputBindings(
			const DesignNode& node,
			const Element& element,
			const std::wstring& owner)
		{
			if (!node.CommandBindings.empty())
			{
				auto property = Append(_xml, element,
					ToUtf8(owner + L".CommandBindings"));
				for (const auto& binding : node.CommandBindings)
				{
					auto item = Append(_xml, property, "CommandBinding");
					Set(item, "Command", binding.Command);
					if (!binding.PreviewCanExecute.empty())
						Set(item, "PreviewCanExecute", binding.PreviewCanExecute);
					if (!binding.CanExecute.empty())
						Set(item, "CanExecute", binding.CanExecute);
					if (!binding.PreviewExecuted.empty())
						Set(item, "PreviewExecuted", binding.PreviewExecuted);
					if (!binding.Executed.empty())
						Set(item, "Executed", binding.Executed);
				}
			}
			if (!node.InputBindings.empty())
			{
				auto property = Append(_xml, element,
					ToUtf8(owner + L".InputBindings"));
				for (const auto& binding : node.InputBindings)
				{
					auto item = Append(_xml, property,
						binding.Kind == DesignInputBindingKind::Mouse
							? "MouseBinding" : "KeyBinding");
					Set(item, "Command", binding.Command);
					Set(item, "Gesture", binding.Gesture);
					if (!binding.CommandParameter.empty())
						Set(item, "CommandParameter", binding.CommandParameter);
					if (!binding.CommandTarget.empty())
						Set(item, "CommandTarget",
							L"{x:Reference " + binding.CommandTarget + L"}");
				}
			}
		}

		void WriteStructuredProperties(
			const DesignNode& node,
			const Element& element,
			DesignNodeProperties& properties,
			DesignValue& extra)
		{
			if (!extra.is_object())
				throw std::invalid_argument("Control structured data must be an object");
			auto moveObject = [&](const wchar_t* property, const char* key,
				DesignerStyleValueKind expectedKind)
			{
				const auto* assignment = properties.Find(property);
				if (!assignment) return;
				if (assignment->Value.Kind != expectedKind
					|| assignment->Value.ObjectValue.is_null()
					|| !assignment->ResourceKey.empty()
					|| !assignment->DynamicResourceKey.empty())
					throw std::invalid_argument(
						"Structured property assignment is malformed");
				extra[key] = assignment->Value.ObjectValue;
				properties.Remove(property);
			};
			moveObject(L"Background", "backgroundBrush",
				DesignerStyleValueKind::Brush);
			moveObject(L"Foreground", "foregroundBrush",
				DesignerStyleValueKind::Brush);
			moveObject(L"BorderBrush", "borderBrush",
				DesignerStyleValueKind::Brush);
			moveObject(L"RenderTransform", "renderTransform",
				DesignerStyleValueKind::Transform);
			moveObject(L"Clip", "clip", DesignerStyleValueKind::Geometry);
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
					NumberText(origin.value("x", 0.0)) + L", "
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
			for (const auto& [key, propertyName] : {
				std::pair{ "backgroundBrush", "Background" },
				std::pair{ "foregroundBrush", "Foreground" },
				std::pair{ "borderBrush", "BorderBrush" } })
			{
				if (!extra.contains(key)) continue;
				const auto& value = extra[key];
				if (!value.is_object())
					throw std::invalid_argument("Control brush must be an object");
				auto property = Append(_xml, element,
					std::string("Control.") + propertyName);
				WriteBrush(property, value);
				extra.ObjectItems().erase(key);
			}
			if (IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, node.Type)
				&& extra.contains("itemTemplate"))
			{
				if (!extra["itemTemplate"].is_string())
					throw std::invalid_argument("ItemsControl ItemTemplate must be a resource key");
				Set(element, "ItemTemplate", L"{StaticResource "
					+ FromUtf8(extra["itemTemplate"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("itemTemplate");
			}
			if ((IsControlTemplateHostClass(node.Type)
				|| !node.ComponentType.Empty())
				&& extra.contains("controlTemplate"))
			{
				if (!extra["controlTemplate"].is_string())
					throw std::invalid_argument(
						"Control Template must be a resource key");
				Set(element, "Template", L"{StaticResource "
					+ FromUtf8(extra["controlTemplate"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("controlTemplate");
			}
			if ((node.Type == UIClass::UI_ContentPresenter
				|| IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, node.Type))
				&& extra.contains("contentTemplate"))
			{
				if (!extra["contentTemplate"].is_string())
					throw std::invalid_argument(
						"Content host ContentTemplate must be a resource key");
				Set(element, "ContentTemplate", L"{StaticResource "
					+ FromUtf8(extra["contentTemplate"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("contentTemplate");
			}
			if ((IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, node.Type)
				|| IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, node.Type))
				&& extra.contains("headerTemplate"))
			{
				if (!extra["headerTemplate"].is_string())
					throw std::invalid_argument(
						"Headered content HeaderTemplate must be a resource key");
				Set(element, "HeaderTemplate", L"{StaticResource "
					+ FromUtf8(extra["headerTemplate"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("headerTemplate");
			}
			if (IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, node.Type)
				&& extra.contains("itemsPanel"))
			{
				if (!extra["itemsPanel"].is_string())
					throw std::invalid_argument(
						"ItemsControl ItemsPanel must be an ItemsPanelTemplate resource key");
				Set(element, "ItemsPanel", L"{StaticResource "
					+ FromUtf8(extra["itemsPanel"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("itemsPanel");
			}
			if (IsUIClassAssignableFrom(
				UIClass::UI_ItemsControl, node.Type)
				&& extra.contains("groupStyle"))
			{
				if (!extra["groupStyle"].is_string())
					throw std::invalid_argument(
						"ItemsControl GroupStyle must be a GroupStyle resource key");
				Set(element, "GroupStyle", L"{StaticResource "
					+ FromUtf8(extra["groupStyle"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("groupStyle");
			}
			if (IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, node.Type)
				&& extra.contains("itemContainerStyle"))
			{
				if (!extra["itemContainerStyle"].is_string())
					throw std::invalid_argument(
						"ItemContainerStyle must be a style resource key");
				Set(element, "ItemContainerStyle", L"{StaticResource "
					+ FromUtf8(extra["itemContainerStyle"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("itemContainerStyle");
			}
			if (extra.contains("itemsSourceResource"))
			{
				if (!extra["itemsSourceResource"].is_string())
					throw std::invalid_argument("ItemsSource must be a DataList resource key");
				Set(element, "ItemsSource", L"{StaticResource "
					+ FromUtf8(extra["itemsSourceResource"].get<std::string>()) + L"}");
				extra.ObjectItems().erase("itemsSourceResource");
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
		}

		void WriteHeaderedChildren(
			const DesignNode& node,
			const Element& element)
		{
			const DesignNode* header = nullptr;
			std::vector<const DesignNode*> content;
			for (const auto graphIndex : _graph.ChildrenOf(node.Name))
			{
				const auto& child = _document.Nodes[
					_graph.Nodes()[graphIndex].SourceIndex];
				const bool isHeader = child.Structure.ChildRole
					== DesignNodeChildRole::Header;
				if (isHeader)
				{
					if (header)
						throw std::invalid_argument(
							"Header slot contains multiple visuals");
					header = &child;
				}
				else content.push_back(&child);
			}
			const bool headeredItems = IsUIClassAssignableFrom(
				UIClass::UI_HeaderedItemsControl, node.Type);
			if (!headeredItems && content.size() > 1)
				throw std::invalid_argument(
					"Content slot contains multiple visuals");
			if (header)
			{
				const auto owner = ControlXamlTypeName(node);
				auto property = Append(_xml, element,
					ToUtf8(owner + L".Header"));
				WriteControl(*header, property, true);
			}
			for (const auto* child : content)
				WriteControl(*child, element, false);
		}
	};
}

std::string XamlDocumentSerializer::ToXaml(const DesignDocument& input)
{
	auto canonical = input;
	std::wstring validationError;
	if (!canonical.ValidateCommandTargetReferences(&validationError))
		throw std::invalid_argument(ToUtf8(validationError));
	if (!DesignDataResourceUtils::ValidateAndCanonicalize(
		canonical, &validationError))
		throw std::invalid_argument(ToUtf8(validationError));
	const auto& document = canonical;
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
