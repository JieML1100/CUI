#include "DesignerControlCatalog.h"
#include "DesignerPreviewPluginAbi.h"

#include "DesignerModel/DesignDocumentGraph.h"
#include "DesignerModel/DesignDocumentMaterializer.h"
#include "DesignerEventCatalog.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "../XmlLite/include/Xml.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace DesignerControlCatalog
{
namespace
{
	static_assert(CUI_DESIGNER_PREVIEW_ABI_V1 == 0x00010000u);
	using System::Xml::XmlDocument;
	using System::Xml::XmlElement;
	using System::Xml::XmlNodeType;

	constexpr std::uintmax_t MaximumManifestBytes = 4U * 1024U * 1024U;

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	bool TryFromUtf8(std::string_view value, std::wstring& result)
	{
		result.clear();
		if (value.empty()) return true;
		if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
			return false;
		const int count = ::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0);
		if (count <= 0) return false;
		result.resize(static_cast<size_t>(count));
		return ::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), count) == count;
	}

	bool TryAttribute(
		const XmlElement& element,
		const char* name,
		std::wstring& result,
		bool required,
		const std::wstring& context,
		std::wstring* outError)
	{
		const auto value = element.GetAttribute(name);
		if (value.empty())
		{
			result.clear();
			if (!required) return true;
			return Fail(context + L" 缺少属性 “"
				+ std::wstring(name, name + std::char_traits<char>::length(name))
				+ L"”。", outError);
		}
		if (!TryFromUtf8(value, result))
			return Fail(context + L" 的属性包含无效 UTF-8。", outError);
		return true;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	bool EqualsInsensitive(const std::wstring& left, const wchar_t* right)
	{
		return _wcsicmp(left.c_str(), right) == 0;
	}

	bool IsIdentifier(const std::wstring& value)
	{
		if (value.empty()) return false;
		if (!(value.front() == L'_' || std::iswalpha(value.front()))) return false;
		return std::all_of(value.begin() + 1, value.end(), [](wchar_t ch)
		{
			return ch == L'_' || std::iswalnum(ch);
		});
	}

	const ControlMetadata* FindBuiltIn(UIClass type)
	{
		static const auto controls = ControlRegistry::GetAvailableControls();
		const auto found = std::find_if(
			controls.begin(), controls.end(), [type](const auto& metadata)
			{
				return metadata.Type == type;
			});
		return found == controls.end() ? nullptr : &*found;
	}

	bool ValidateAttributes(
		const XmlElement& element,
		std::initializer_list<const char*> allowed,
		const std::wstring& context,
		std::wstring* outError)
	{
		for (const auto& attribute : element.Attributes())
		{
			if (!attribute) continue;
			const auto& name = attribute->Name();
			const bool known = std::any_of(
				allowed.begin(), allowed.end(), [&](const char* candidate)
				{
					return name == candidate;
				});
			if (!known)
			{
				std::wstring wideName;
				(void)TryFromUtf8(name, wideName);
				return Fail(context + L" 包含未知属性 “" + wideName + L"”。",
					outError);
			}
		}
		return true;
	}

	bool TryPositiveInt(
		const std::wstring& value,
		int defaultValue,
		int& result)
	{
		if (value.empty())
		{
			result = defaultValue;
			return true;
		}
		try
		{
			size_t consumed = 0;
			const auto parsed = std::stoll(value, &consumed, 10);
			if (consumed != value.size() || parsed < 1 || parsed > 100000)
				return false;
			result = static_cast<int>(parsed);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryBool(const std::wstring& value, bool defaultValue, bool& result)
	{
		if (value.empty())
		{
			result = defaultValue;
			return true;
		}
		if (EqualsInsensitive(value, L"true"))
		{
			result = true;
			return true;
		}
		if (EqualsInsensitive(value, L"false"))
		{
			result = false;
			return true;
		}
		return false;
	}

	bool TryInt(const std::wstring& value, int defaultValue, int& result)
	{
		if (value.empty())
		{
			result = defaultValue;
			return true;
		}
		try
		{
			size_t consumed = 0;
			const auto parsed = std::stoll(value, &consumed, 10);
			if (consumed != value.size() || parsed < -100000 || parsed > 100000)
				return false;
			result = static_cast<int>(parsed);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryOptionalDouble(
		std::wstring value,
		std::optional<double>& result)
	{
		value = DesignerStyleSheetUtils::Trim(value);
		if (value.empty())
		{
			result.reset();
			return true;
		}
		try
		{
			size_t consumed = 0;
			const double parsed = std::stod(value, &consumed);
			if (consumed != value.size() || !std::isfinite(parsed)) return false;
			result = parsed;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::wstring DefaultValueText(DesignerStyleValueKind kind)
	{
		switch (kind)
		{
		case DesignerStyleValueKind::Bool: return L"false";
		case DesignerStyleValueKind::String: return {};
		case DesignerStyleValueKind::Color: return L"#00000000";
		case DesignerStyleValueKind::Thickness: return L"0";
		case DesignerStyleValueKind::Size: return L"0, 0";
		case DesignerStyleValueKind::Length: return L"Auto";
		default: return L"0";
		}
	}

	bool TryParseEditor(
		const std::wstring& value,
		ControlPropertyEditorKind& result)
	{
		if (value.empty() || EqualsInsensitive(value, L"Auto"))
			result = ControlPropertyEditorKind::Auto;
		else if (EqualsInsensitive(value, L"Text"))
			result = ControlPropertyEditorKind::Text;
		else if (EqualsInsensitive(value, L"Boolean"))
			result = ControlPropertyEditorKind::Boolean;
		else if (EqualsInsensitive(value, L"Number"))
			result = ControlPropertyEditorKind::Number;
		else if (EqualsInsensitive(value, L"Choice"))
			result = ControlPropertyEditorKind::Choice;
		else if (EqualsInsensitive(value, L"Color"))
			result = ControlPropertyEditorKind::Color;
		else if (EqualsInsensitive(value, L"Thickness"))
			result = ControlPropertyEditorKind::Thickness;
		else if (EqualsInsensitive(value, L"Size"))
			result = ControlPropertyEditorKind::Size;
		else if (EqualsInsensitive(value, L"Length"))
			result = ControlPropertyEditorKind::Length;
		else return false;
		return true;
	}

	bool TryParseEventCategory(
		const std::wstring& value,
		DesignerEventCategory& result)
	{
		if (value.empty() || EqualsInsensitive(value, L"Other"))
			result = DesignerEventCategory::Other;
		else if (EqualsInsensitive(value, L"Action"))
			result = DesignerEventCategory::Action;
		else if (EqualsInsensitive(value, L"Value"))
			result = DesignerEventCategory::Value;
		else if (EqualsInsensitive(value, L"Mouse"))
			result = DesignerEventCategory::Mouse;
		else if (EqualsInsensitive(value, L"Keyboard"))
			result = DesignerEventCategory::Keyboard;
		else if (EqualsInsensitive(value, L"Focus"))
			result = DesignerEventCategory::Focus;
		else if (EqualsInsensitive(value, L"DragDrop"))
			result = DesignerEventCategory::DragDrop;
		else if (EqualsInsensitive(value, L"Layout"))
			result = DesignerEventCategory::Layout;
		else if (EqualsInsensitive(value, L"Lifecycle"))
			result = DesignerEventCategory::Lifecycle;
		else if (EqualsInsensitive(value, L"Data"))
			result = DesignerEventCategory::Data;
		else if (EqualsInsensitive(value, L"Navigation"))
			result = DesignerEventCategory::Navigation;
		else if (EqualsInsensitive(value, L"Media"))
			result = DesignerEventCategory::Media;
		else if (EqualsInsensitive(value, L"Diagnostics"))
			result = DesignerEventCategory::Diagnostics;
		else return false;
		return true;
	}

	bool TryParseEventSignature(
		const std::wstring& value,
		DesignerCustomEventSignature& result)
	{
		if (EqualsInsensitive(value, L"None"))
			result = DesignerCustomEventSignature::None;
		else if (EqualsInsensitive(value, L"Sender"))
			result = DesignerCustomEventSignature::Sender;
		else if (EqualsInsensitive(value, L"SenderBool"))
			result = DesignerCustomEventSignature::SenderBool;
		else if (EqualsInsensitive(value, L"SenderInt"))
			result = DesignerCustomEventSignature::SenderInt;
		else if (EqualsInsensitive(value, L"SenderFloat"))
			result = DesignerCustomEventSignature::SenderFloat;
		else if (EqualsInsensitive(value, L"SenderDouble"))
			result = DesignerCustomEventSignature::SenderDouble;
		else if (EqualsInsensitive(value, L"SenderString"))
			result = DesignerCustomEventSignature::SenderString;
		else if (EqualsInsensitive(value, L"SenderIntInt"))
			result = DesignerCustomEventSignature::SenderIntInt;
		else if (EqualsInsensitive(value, L"SenderIntBool"))
			result = DesignerCustomEventSignature::SenderIntBool;
		else if (EqualsInsensitive(value, L"SenderDoubleDouble"))
			result = DesignerCustomEventSignature::SenderDoubleDouble;
		else if (EqualsInsensitive(value, L"SenderStringString"))
			result = DesignerCustomEventSignature::SenderStringString;
		else return false;
		return true;
	}

	bool ParseCustomEvent(
		const XmlElement& element,
		const std::wstring& controlContext,
		size_t index,
		DesignerCustomEventDescriptor& event,
		std::wstring* outError)
	{
		const auto context = controlContext + L" 的 event #"
			+ std::to_wstring(index);
		if (!ValidateAttributes(element, {
			"name", "displayName", "field", "category", "signature",
			"order", "default" }, context, outError)) return false;

		std::wstring field;
		std::wstring category;
		std::wstring signature;
		std::wstring order;
		std::wstring isDefault;
		if (!TryAttribute(element, "name", event.Name, true, context, outError)
			|| !TryAttribute(element, "displayName", event.DisplayName, false,
				context, outError)
			|| !TryAttribute(element, "field", field, false, context, outError)
			|| !TryAttribute(element, "category", category, false, context, outError)
			|| !TryAttribute(element, "signature", signature, true, context, outError)
			|| !TryAttribute(element, "order", order, false, context, outError)
			|| !TryAttribute(element, "default", isDefault, false, context, outError))
			return false;

		event.Name = DesignerStyleSheetUtils::Trim(event.Name);
		event.DisplayName = DesignerStyleSheetUtils::Trim(event.DisplayName);
		field = DesignerStyleSheetUtils::Trim(field);
		category = DesignerStyleSheetUtils::Trim(category);
		signature = DesignerStyleSheetUtils::Trim(signature);
		order = DesignerStyleSheetUtils::Trim(order);
		isDefault = DesignerStyleSheetUtils::Trim(isDefault);
		if (field.empty()) field = event.Name;
		if (!IsIdentifier(event.Name) || !IsIdentifier(field))
			return Fail(context + L" 的 name/field 必须是标识符。", outError);
		if (!std::all_of(event.Name.begin(), event.Name.end(), [](wchar_t ch)
			{ return ch >= 0 && ch <= 0x7f; })
			|| !std::all_of(field.begin(), field.end(), [](wchar_t ch)
				{ return ch >= 0 && ch <= 0x7f; }))
			return Fail(context + L" 的 name/field 必须是 ASCII 标识符。",
				outError);
		if (event.DisplayName.empty()) event.DisplayName = event.Name;
		event.EventField.reserve(field.size());
		for (const auto ch : field)
			event.EventField.push_back(static_cast<char>(ch));
		if (!TryParseEventCategory(category, event.Category))
			return Fail(context + L" 的 category 无效。", outError);
		if (!TryParseEventSignature(signature, event.Signature))
			return Fail(context + L" 的 signature 无效。", outError);
		if (!TryInt(order, 0, event.Order))
			return Fail(context + L" 的 order 必须是 -100000..100000 的整数。",
				outError);
		if (!TryBool(isDefault, false, event.IsDefault))
			return Fail(context + L" 的 default 必须是 true 或 false。",
				outError);

		for (const auto& child : element.ChildNodes())
			if (child && child->NodeType() != XmlNodeType::Whitespace
				&& child->NodeType() != XmlNodeType::SignificantWhitespace
				&& child->NodeType() != XmlNodeType::Comment)
				return Fail(context + L" 不能包含子内容。", outError);
		if (!DesignerEventCatalog::FromCustomEvent(event))
			return Fail(context + L" 无法转换为安全事件契约。", outError);
		return true;
	}

	bool ParseCustomProperty(
		const XmlElement& element,
		const std::wstring& controlContext,
		size_t index,
		DesignerCustomPropertyDescriptor& property,
		std::wstring* outError)
	{
		const auto context = controlContext + L" 的 property #"
			+ std::to_wstring(index);
		if (!ValidateAttributes(element, {
			"name", "displayName", "category", "categoryOrder", "order",
			"kind", "default", "editor", "minimum", "maximum", "step",
			"bindable", "twoWay" }, context, outError)) return false;

		std::wstring kind;
		std::wstring defaultValue;
		std::wstring editor;
		std::wstring categoryOrder;
		std::wstring order;
		std::wstring minimum;
		std::wstring maximum;
		std::wstring step;
		std::wstring bindable;
		std::wstring twoWay;
		if (!TryAttribute(element, "name", property.Name, true, context, outError)
			|| !TryAttribute(element, "displayName", property.DisplayName, false, context, outError)
			|| !TryAttribute(element, "category", property.Category, false, context, outError)
			|| !TryAttribute(element, "categoryOrder", categoryOrder, false, context, outError)
			|| !TryAttribute(element, "order", order, false, context, outError)
			|| !TryAttribute(element, "kind", kind, true, context, outError)
			|| !TryAttribute(element, "default", defaultValue, false, context, outError)
			|| !TryAttribute(element, "editor", editor, false, context, outError)
			|| !TryAttribute(element, "minimum", minimum, false, context, outError)
			|| !TryAttribute(element, "maximum", maximum, false, context, outError)
			|| !TryAttribute(element, "step", step, false, context, outError)
			|| !TryAttribute(element, "bindable", bindable, false, context, outError)
			|| !TryAttribute(element, "twoWay", twoWay, false, context, outError))
			return false;

		property.Name = DesignerStyleSheetUtils::Trim(property.Name);
		property.DisplayName = DesignerStyleSheetUtils::Trim(property.DisplayName);
		property.Category = DesignerStyleSheetUtils::Trim(property.Category);
		kind = DesignerStyleSheetUtils::Trim(kind);
		defaultValue = DesignerStyleSheetUtils::Trim(defaultValue);
		editor = DesignerStyleSheetUtils::Trim(editor);
		if (!IsIdentifier(property.Name))
			return Fail(context + L" 的 name 必须是标识符。", outError);
		if (property.DisplayName.empty()) property.DisplayName = property.Name;
		if (property.Category.empty()) property.Category = L"自定义";
		if (!DesignerStyleSheetUtils::TryParseValueKind(
			kind, property.DefaultValue.Kind))
			return Fail(context + L" 的 kind 无效。", outError);
		if (defaultValue.empty())
			defaultValue = DefaultValueText(property.DefaultValue.Kind);
		property.DefaultValue.Text = defaultValue;
		BindingValue converted;
		std::wstring conversionError;
		if (!DesignerStyleSheetUtils::TryConvertValue(
			property.DefaultValue, converted, &conversionError))
			return Fail(context + L" 的 default 无效：" + conversionError,
				outError);
		if (!TryParseEditor(editor, property.Editor))
			return Fail(context + L" 的 editor 无效。", outError);
		if (!TryInt(categoryOrder, 500, property.CategoryOrder)
			|| !TryInt(order, 0, property.Order))
			return Fail(context + L" 的 categoryOrder/order 必须是 -100000..100000 的整数。",
				outError);
		if (!TryOptionalDouble(minimum, property.Minimum)
			|| !TryOptionalDouble(maximum, property.Maximum)
			|| !TryOptionalDouble(step, property.Step)
			|| (property.Minimum && property.Maximum
				&& *property.Minimum > *property.Maximum)
			|| (property.Step && *property.Step <= 0.0))
			return Fail(context + L" 的 minimum/maximum/step 无效。", outError);
		if (!TryBool(DesignerStyleSheetUtils::Trim(bindable), true, property.Bindable))
			return Fail(context + L" 的 bindable 必须是 true 或 false。", outError);
		if (!TryBool(DesignerStyleSheetUtils::Trim(twoWay), false,
			property.SupportsTwoWayBinding))
			return Fail(context + L" 的 twoWay 必须是 true 或 false。",
				outError);
		if (!property.Bindable && property.SupportsTwoWayBinding)
			return Fail(context + L" 的 twoWay=true 需要 bindable=true。",
				outError);

		std::unordered_set<std::wstring> choiceValues;
		for (const auto& child : element.ChildNodes())
		{
			if (!child || child->NodeType() == XmlNodeType::Whitespace
				|| child->NodeType() == XmlNodeType::SignificantWhitespace
				|| child->NodeType() == XmlNodeType::Comment)
				continue;
			if (child->NodeType() != XmlNodeType::Element
				|| child->Name() != "choice")
				return Fail(context + L" 只能包含 <choice> 子元素。", outError);
			const auto choiceElement = std::dynamic_pointer_cast<XmlElement>(child);
			if (!choiceElement
				|| !ValidateAttributes(*choiceElement, { "displayName", "value" },
					context + L" 的 choice", outError)) return false;
			for (const auto& nested : choiceElement->ChildNodes())
				if (nested && nested->NodeType() != XmlNodeType::Whitespace
					&& nested->NodeType() != XmlNodeType::SignificantWhitespace
					&& nested->NodeType() != XmlNodeType::Comment)
					return Fail(context + L" 的 choice 不能包含子内容。", outError);
			DesignerCustomPropertyDescriptor::Choice choice;
			if (!TryAttribute(*choiceElement, "displayName", choice.DisplayName,
				true, context + L" 的 choice", outError)
				|| !TryAttribute(*choiceElement, "value", choice.ValueText,
					true, context + L" 的 choice", outError)) return false;
			choice.DisplayName = DesignerStyleSheetUtils::Trim(choice.DisplayName);
			choice.ValueText = DesignerStyleSheetUtils::Trim(choice.ValueText);
			if (choice.DisplayName.empty())
				return Fail(context + L" 的 choice displayName 不能为空。", outError);
			DesignerStyleValue choiceValue{
				property.DefaultValue.Kind, choice.ValueText };
			if (!DesignerStyleSheetUtils::TryConvertValue(
				choiceValue, converted, &conversionError))
				return Fail(context + L" 的 choice value 无效："
					+ conversionError, outError);
			if (!choiceValues.insert(Lower(choice.ValueText)).second)
				return Fail(context + L" 的 choice value 重复。", outError);
			property.Choices.push_back(std::move(choice));
		}
		if (!property.Choices.empty())
		{
			property.Editor = ControlPropertyEditorKind::Choice;
			const auto defaultChoice = std::find_if(
				property.Choices.begin(), property.Choices.end(),
				[&](const auto& choice)
				{
					return choice.ValueText == property.DefaultValue.Text;
				});
			if (defaultChoice == property.Choices.end())
				return Fail(context + L" 的 default 不在 choice 集合中。",
					outError);
		}
		else if (property.Editor == ControlPropertyEditorKind::Choice)
			return Fail(context + L" 使用 Choice editor 时至少需要一个 choice。",
				outError);

		const bool numericKind =
			property.DefaultValue.Kind == DesignerStyleValueKind::Int
			|| property.DefaultValue.Kind == DesignerStyleValueKind::Int64
			|| property.DefaultValue.Kind == DesignerStyleValueKind::Float
			|| property.DefaultValue.Kind == DesignerStyleValueKind::Double;
		if ((property.Minimum || property.Maximum || property.Step) && !numericKind)
			return Fail(context + L" 只有数值 kind 可以使用 minimum/maximum/step。",
				outError);
		if (numericKind && (property.Minimum || property.Maximum))
		{
			BindingValue defaultRuntimeValue;
			if (!DesignerStyleSheetUtils::TryConvertValue(
				property.DefaultValue, defaultRuntimeValue, &conversionError))
				return Fail(context + L" 的 default 无效。", outError);
			double numericDefault = 0.0;
			if (!defaultRuntimeValue.TryGetDouble(numericDefault)
				|| (property.Minimum && numericDefault < *property.Minimum)
				|| (property.Maximum && numericDefault > *property.Maximum))
				return Fail(context + L" 的 default 超出 minimum/maximum 范围。",
					outError);
		}
		return true;
	}

	bool ValidatePortableType(
		const DesignerControlDescriptor& descriptor,
		std::wstring* outError)
	{
		DesignerModel::DesignDocument document;
		DesignerModel::DesignNode node;
		node.Id = 1;
		node.Name = descriptor.Name;
		node.Type = descriptor.Type;
		node.CustomType = descriptor.CustomType;
		document.Nodes.push_back(std::move(node));
		document.NextStableId = 2;
		DesignerModel::DesignDocumentGraph graph;
		return DesignerModel::DesignDocumentGraph::Build(
			document, graph, outError);
	}

	bool ParseControl(
		const XmlElement& element,
		size_t index,
		DesignerControlDescriptor& descriptor,
		std::wstring* outError)
	{
		const auto context = L"控件清单第 " + std::to_wstring(index) + L" 项";
		if (!ValidateAttributes(element, {
			"name", "displayName", "category", "baseType", "xamlPrefix",
			"xamlName", "xamlNamespace", "cppType", "header", "constructor",
			"width", "height", "container" }, context, outError)) return false;
		std::wstring baseType;
		std::wstring width;
		std::wstring height;
		std::wstring container;
		std::wstring constructor;
		if (!TryAttribute(element, "name", descriptor.Name, true, context, outError)
			|| !TryAttribute(element, "displayName", descriptor.DisplayName, false, context, outError)
			|| !TryAttribute(element, "category", descriptor.Category, false, context, outError)
			|| !TryAttribute(element, "baseType", baseType, true, context, outError)
			|| !TryAttribute(element, "xamlPrefix", descriptor.CustomType.XamlPrefix, true, context, outError)
			|| !TryAttribute(element, "xamlName", descriptor.CustomType.XamlName, true, context, outError)
			|| !TryAttribute(element, "xamlNamespace", descriptor.CustomType.XamlNamespace, true, context, outError)
			|| !TryAttribute(element, "cppType", descriptor.CustomType.CppType, true, context, outError)
			|| !TryAttribute(element, "header", descriptor.CustomType.Header, true, context, outError)
			|| !TryAttribute(element, "constructor", constructor, false, context, outError)
			|| !TryAttribute(element, "width", width, false, context, outError)
			|| !TryAttribute(element, "height", height, false, context, outError)
			|| !TryAttribute(element, "container", container, false, context, outError))
			return false;
		auto trim = [](std::wstring& value)
		{
			value = DesignerStyleSheetUtils::Trim(value);
		};
		trim(descriptor.Name);
		trim(descriptor.DisplayName);
		trim(descriptor.Category);
		trim(baseType);
		trim(descriptor.CustomType.XamlPrefix);
		trim(descriptor.CustomType.XamlName);
		trim(descriptor.CustomType.XamlNamespace);
		trim(descriptor.CustomType.CppType);
		trim(descriptor.CustomType.Header);
		trim(constructor);
		trim(width);
		trim(height);
		trim(container);

		if (!IsIdentifier(descriptor.Name)
			|| !IsIdentifier(descriptor.CustomType.XamlPrefix)
			|| !IsIdentifier(descriptor.CustomType.XamlName))
			return Fail(context + L" 的 name、xamlPrefix 和 xamlName 必须是标识符。",
				outError);
		if (_wcsicmp(descriptor.CustomType.XamlPrefix.c_str(), L"x") == 0
			|| _wcsicmp(descriptor.CustomType.XamlPrefix.c_str(), L"d") == 0)
			return Fail(context + L" 使用了保留的 XAML 前缀。", outError);
		if (!DesignerStyleSheetUtils::TryParseUIClass(baseType, descriptor.Type)
			|| !FindBuiltIn(descriptor.Type))
			return Fail(context + L" 的 baseType 不是设计器可实例化的内置控件。",
				outError);

		const auto* base = FindBuiltIn(descriptor.Type);
		if (descriptor.DisplayName.empty()) descriptor.DisplayName = descriptor.Name;
		if (descriptor.Category.empty()) descriptor.Category = L"自定义控件";
		int parsedWidth = 0;
		int parsedHeight = 0;
		if (!TryPositiveInt(width, base->DefaultSize.cx, parsedWidth)
			|| !TryPositiveInt(height, base->DefaultSize.cy, parsedHeight))
			return Fail(context + L" 的 width/height 必须是 1..100000 的整数。",
				outError);
		descriptor.DefaultSize = { parsedWidth, parsedHeight };
		if (!TryBool(container, base->IsContainer, descriptor.IsContainer))
			return Fail(context + L" 的 container 必须是 true 或 false。", outError);

		if (constructor.empty() || EqualsInsensitive(constructor, L"Bounds"))
			descriptor.CustomType.Constructor = DesignerCustomControlConstructor::Bounds;
		else if (EqualsInsensitive(constructor, L"Default"))
			descriptor.CustomType.Constructor = DesignerCustomControlConstructor::Default;
		else if (EqualsInsensitive(constructor, L"TextBounds"))
			descriptor.CustomType.Constructor = DesignerCustomControlConstructor::TextBounds;
		else
			return Fail(context + L" 的 constructor 必须是 Default、Bounds 或 TextBounds。",
				outError);

		std::wstring normalizedCppType;
		std::wstring validationError;
		if (!DesignerModel::DesignCodeBehindModel::TryNormalizeClassName(
			descriptor.CustomType.CppType, normalizedCppType, &validationError)
			|| normalizedCppType.empty())
			return Fail(context + L" 的 cppType 无效：" + validationError, outError);
		descriptor.CustomType.CppType = std::move(normalizedCppType);

		if (!descriptor.IsValid())
			return Fail(context + L" 的控件描述不完整。", outError);
		if (!ValidatePortableType(descriptor, &validationError))
			return Fail(context + L" 无效：" + validationError, outError);

		auto baseProbe =
			DesignerModel::DesignDocumentMaterializer::CreateRuntimeControl(
				descriptor.Type);
		std::unordered_set<std::wstring> propertyNames;
		std::unordered_set<std::wstring> eventNames;
		std::unordered_set<std::wstring> eventFields;
		const auto baseEvents = DesignerEventCatalog::GetControlEvents(
			descriptor.Type);
		size_t propertyIndex = 0;
		size_t eventIndex = 0;
		bool hasDefaultCustomEvent = false;
		for (const auto& child : element.ChildNodes())
		{
			if (!child || child->NodeType() == XmlNodeType::Whitespace
				|| child->NodeType() == XmlNodeType::SignificantWhitespace
				|| child->NodeType() == XmlNodeType::Comment)
				continue;
			if (child->NodeType() != XmlNodeType::Element)
				return Fail(context + L" 只能包含 <property> 或 <event> 子元素。",
					outError);
			const auto element = std::dynamic_pointer_cast<XmlElement>(child);
			if (child->Name() == "property")
			{
				if (++propertyIndex > 256)
					return Fail(context + L" 的自定义属性超过 256 项限制。", outError);
				DesignerCustomPropertyDescriptor property;
				if (!element || !ParseCustomProperty(
					*element, context, propertyIndex, property, outError))
					return false;
				const auto propertyKey = Lower(property.Name);
				if (!propertyNames.insert(propertyKey).second)
					return Fail(context + L" 的自定义属性名重复："
						+ property.Name, outError);
				if (baseProbe && baseProbe->FindPropertyMetadata(property.Name))
					return Fail(context + L" 的自定义属性与基类属性重名："
						+ property.Name, outError);
				if (eventFields.contains(propertyKey)
					|| eventNames.contains(propertyKey))
					return Fail(context + L" 的属性名与事件名/field 重名："
						+ property.Name, outError);
				descriptor.CustomProperties.push_back(std::move(property));
				continue;
			}
			if (child->Name() == "event")
			{
				if (++eventIndex > 256)
					return Fail(context + L" 的自定义事件超过 256 项限制。", outError);
				DesignerCustomEventDescriptor event;
				if (!element || !ParseCustomEvent(
					*element, context, eventIndex, event, outError))
					return false;
				const auto nameKey = Lower(event.Name);
				std::wstring field(event.EventField.begin(), event.EventField.end());
				const auto fieldKey = Lower(field);
				if (!eventNames.insert(nameKey).second)
					return Fail(context + L" 的自定义事件名重复："
						+ event.Name, outError);
				if (!eventFields.insert(fieldKey).second)
					return Fail(context + L" 的自定义事件 field 重复："
						+ field, outError);
				if (propertyNames.contains(fieldKey)
					|| propertyNames.contains(nameKey))
					return Fail(context + L" 的事件名/field 与属性名重名："
						+ field, outError);
				if (baseProbe && (baseProbe->FindPropertyMetadata(event.Name)
					|| baseProbe->FindPropertyMetadata(field)))
					return Fail(context + L" 的自定义事件与基类属性重名："
						+ event.Name, outError);
				const auto baseCollision = std::find_if(
					baseEvents.begin(), baseEvents.end(), [&](const auto& baseEvent)
					{
						return Lower(baseEvent.Name) == nameKey
							|| Lower(std::wstring(
								baseEvent.EventField.begin(),
								baseEvent.EventField.end())) == fieldKey;
					});
				if (baseCollision != baseEvents.end())
					return Fail(context + L" 的自定义事件与基类事件重名："
						+ event.Name, outError);
				if (event.IsDefault && std::exchange(hasDefaultCustomEvent, true))
					return Fail(context + L" 只能声明一个默认自定义事件。", outError);
				descriptor.CustomEvents.push_back(std::move(event));
				continue;
			}
			return Fail(context + L" 只能包含 <property> 或 <event> 子元素。",
				outError);
		}
		if (!DesignerEventCatalog::ValidateCustomEvents(
			descriptor.Type, descriptor.CustomEvents, &validationError))
			return Fail(context + L" 的自定义事件无效：" + validationError,
				outError);
		return true;
	}

	bool ValidateMergedCatalog(
		const std::vector<DesignerControlDescriptor>& descriptors,
		std::wstring* outError)
	{
		std::unordered_set<std::wstring> names;
		std::unordered_set<std::wstring> registryKeys;
		std::unordered_map<std::wstring, std::wstring> prefixNamespaces;
		for (const auto& descriptor : descriptors)
		{
			if (!descriptor.IsValid() || !FindBuiltIn(descriptor.Type)
				|| !IsIdentifier(descriptor.Name))
				return Fail(L"控件描述无效：“" + descriptor.Name + L"”。", outError);
			if (descriptor.IsCustom())
			{
				std::wstring validationError;
				if (!ValidatePortableType(descriptor, &validationError))
					return Fail(L"自定义控件描述无效：“" + descriptor.Name
						+ L"”：" + validationError, outError);
			}
			const auto name = Lower(descriptor.Name);
			if (!names.insert(name).second)
				return Fail(L"控件清单名称重复（不区分大小写）：“"
					+ descriptor.Name + L"”。", outError);
			if (!descriptor.IsCustom()) continue;
			if (!registryKeys.insert(descriptor.CustomType.RegistryKey()).second)
				return Fail(L"自定义 XAML 控件重复：“"
					+ descriptor.CustomType.XamlName + L"”。", outError);
			const auto [found, inserted] = prefixNamespaces.emplace(
				descriptor.CustomType.XamlPrefix,
				descriptor.CustomType.XamlNamespace);
			if (!inserted && found->second != descriptor.CustomType.XamlNamespace)
				return Fail(L"XAML 前缀 “" + descriptor.CustomType.XamlPrefix
					+ L"” 被映射到多个命名空间。", outError);
		}
		return true;
	}
}

std::vector<DesignerControlDescriptor> BuiltInDescriptors()
{
	std::vector<DesignerControlDescriptor> result;
	const auto controls = ControlRegistry::GetAvailableControls();
	result.reserve(controls.size());
	for (const auto& metadata : controls)
		result.push_back(DesignerControlDescriptor::BuiltIn(metadata));
	return result;
}

bool AppendFromXml(
	std::vector<DesignerControlDescriptor>& descriptors,
	std::string_view xml,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (xml.size() > MaximumManifestBytes)
		return Fail(L"控件清单超过 4 MiB 限制。", outError);
	try
	{
		System::Xml::XmlReaderSettings settings;
		settings.DtdProcessing = System::Xml::DtdProcessing::Prohibit;
		settings.MaxCharactersInDocument = MaximumManifestBytes;
		auto document = XmlDocument::Parse(xml, settings);
		const auto root = document ? document->DocumentElement() : nullptr;
		if (!root || root->Name() != "cuiControlCatalog")
			return Fail(L"控件清单根元素必须是 <cuiControlCatalog>。", outError);
		if (!ValidateAttributes(*root, { "schema", "version" },
			L"控件清单根元素", outError)) return false;
		if (root->GetAttribute("schema") != "cui.designer.controls"
			|| root->GetAttribute("version") != "1")
			return Fail(L"控件清单 schema/version 必须是 cui.designer.controls/1。",
				outError);

		auto candidate = descriptors;
		size_t index = 0;
		for (const auto& child : root->ChildNodes())
		{
			if (!child || child->NodeType() == XmlNodeType::Whitespace
				|| child->NodeType() == XmlNodeType::SignificantWhitespace
				|| child->NodeType() == XmlNodeType::Comment)
				continue;
			if (child->NodeType() != XmlNodeType::Element
				|| child->Name() != "control")
				return Fail(L"控件清单只能包含 <control> 子元素。", outError);
			const auto element = std::dynamic_pointer_cast<XmlElement>(child);
			if (!element) return Fail(L"控件清单包含无效元素。", outError);
			++index;
			DesignerControlDescriptor descriptor;
			if (!ParseControl(*element, index, descriptor, outError)) return false;
			candidate.push_back(std::move(descriptor));
		}
		if (index == 0)
			return Fail(L"控件清单至少需要一个 <control>。", outError);
		if (!ValidateMergedCatalog(candidate, outError)) return false;
		descriptors = std::move(candidate);
		return true;
	}
	catch (const std::exception& exception)
	{
		std::wstring message;
		if (!TryFromUtf8(exception.what(), message)) message = L"XML 解析失败";
		return Fail(L"无法解析控件清单：" + message, outError);
	}
	catch (...)
	{
		return Fail(L"无法解析控件清单。", outError);
	}
}

bool AppendFromFile(
	std::vector<DesignerControlDescriptor>& descriptors,
	const std::wstring& filePath,
	std::wstring* outError)
{
	if (outError) outError->clear();
	try
	{
		const std::filesystem::path path(filePath);
		std::error_code error;
		const auto size = std::filesystem::file_size(path, error);
		if (error) return Fail(L"无法读取控件清单：“" + filePath + L"”。", outError);
		if (size > MaximumManifestBytes)
			return Fail(L"控件清单超过 4 MiB 限制。", outError);
		std::ifstream stream(path, std::ios::binary);
		if (!stream) return Fail(L"无法打开控件清单：“" + filePath + L"”。", outError);
		std::string xml{
			std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>() };
		if (!stream.good() && !stream.eof())
			return Fail(L"读取控件清单失败：“" + filePath + L"”。", outError);
		std::vector<DesignerControlDescriptor> candidate = descriptors;
		std::wstring parseError;
		if (!AppendFromXml(candidate, xml, &parseError))
			return Fail(L"控件清单 “" + filePath + L"” 无效：" + parseError,
				outError);
		descriptors = std::move(candidate);
		return true;
	}
	catch (...)
	{
		return Fail(L"读取控件清单失败：“" + filePath + L"”。", outError);
	}
}

bool AttachPreviewFactory(
	std::vector<DesignerControlDescriptor>& descriptors,
	const std::wstring& xamlNamespace,
	const std::wstring& xamlName,
	PreviewFactory factory,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!factory) return Fail(L"预览工厂不能为空。", outError);
	auto found = std::find_if(
		descriptors.begin(), descriptors.end(), [&](const auto& descriptor)
		{
			return descriptor.IsCustom()
				&& descriptor.CustomType.XamlNamespace == xamlNamespace
				&& descriptor.CustomType.XamlName == xamlName;
		});
	if (found == descriptors.end())
		return Fail(L"找不到要注册预览工厂的自定义控件。", outError);
	found->PreviewFactory = std::move(factory);
	return true;
}
}
