#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "../CUI/include/DateTimePicker.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <typeindex>

namespace DesignerPropertyCatalog
{
namespace
{
	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	std::wstring NumberText(double value, int precision = 7)
	{
		std::wostringstream stream;
		stream << std::setprecision(precision) << value;
		return stream.str();
	}

	unsigned int ColorByte(float value)
	{
		const auto normalized = (std::max)(0.0f, (std::min)(1.0f, value));
		return static_cast<unsigned int>(std::lround(normalized * 255.0f));
	}

	std::wstring FormatValue(
		const BindingValue& value,
		DesignerStyleValueKind kind,
		const std::type_index& valueType)
	{
		switch (kind)
		{
		case DesignerStyleValueKind::Bool:
		{
			bool typed = false;
			return value.TryGet(typed) && typed ? L"true" : L"false";
		}
		case DesignerStyleValueKind::Int:
		{
			int typed = 0;
			if (value.Kind() == BindingValueKind::Object)
			{
				if (valueType == std::type_index(typeid(HorizontalAlignment)))
				{
					HorizontalAlignment item{};
					if (value.TryGet(item)) typed = static_cast<int>(item);
				}
				else if (valueType == std::type_index(typeid(VerticalAlignment)))
				{
					VerticalAlignment item{};
					if (value.TryGet(item)) typed = static_cast<int>(item);
				}
				else if (valueType == std::type_index(typeid(Dock)))
				{
					Dock item{};
					if (value.TryGet(item)) typed = static_cast<int>(item);
				}
				else if (valueType == std::type_index(typeid(Orientation)))
				{
					Orientation item{};
					if (value.TryGet(item)) typed = static_cast<int>(item);
				}
				else if (valueType == std::type_index(typeid(DateTimePickerMode)))
				{
					DateTimePickerMode item{};
					if (value.TryGet(item)) typed = static_cast<int>(item);
				}
				else if (valueType == std::type_index(typeid(AccessibleRole)))
				{
					AccessibleRole item{};
					if (value.TryGet(item)) typed = static_cast<int>(item);
				}
				return std::to_wstring(typed);
			}
			return value.TryGet(typed) ? std::to_wstring(typed) : L"0";
		}
		case DesignerStyleValueKind::Int64:
		{
			long long typed = 0;
			return value.TryGet(typed) ? std::to_wstring(typed) : L"0";
		}
		case DesignerStyleValueKind::Float:
		{
			float typed = 0.0f;
			return value.TryGet(typed) ? NumberText(typed) : L"0";
		}
		case DesignerStyleValueKind::Double:
		{
			double typed = 0.0;
			return value.TryGet(typed) ? NumberText(typed, 15) : L"0";
		}
		case DesignerStyleValueKind::String:
		{
			std::wstring typed;
			return value.TryGet(typed) ? typed : L"";
		}
		case DesignerStyleValueKind::Color:
		{
			D2D1_COLOR_F typed{};
			if (!value.TryGet(typed)) return L"#FF0078D4";
			wchar_t text[10]{};
			swprintf_s(text, L"#%02X%02X%02X%02X",
				ColorByte(typed.a), ColorByte(typed.r),
				ColorByte(typed.g), ColorByte(typed.b));
			return text;
		}
		case DesignerStyleValueKind::Thickness:
		{
			Thickness typed;
			if (!value.TryGet(typed)) return L"0";
			if (typed.Left == typed.Top && typed.Left == typed.Right
				&& typed.Left == typed.Bottom)
				return NumberText(typed.Left);
			if (typed.Left == typed.Right && typed.Top == typed.Bottom)
				return NumberText(typed.Left) + L", " + NumberText(typed.Top);
			return NumberText(typed.Left) + L", " + NumberText(typed.Top)
				+ L", " + NumberText(typed.Right) + L", " + NumberText(typed.Bottom);
		}
		case DesignerStyleValueKind::Size:
		{
			SIZE typed{};
			return value.TryGet(typed)
				? std::to_wstring(typed.cx) + L", " + std::to_wstring(typed.cy)
				: L"0, 0";
		}
		case DesignerStyleValueKind::Length:
		{
			cui::layout::Length typed;
			if (!value.TryGet(typed) || typed.IsAuto()) return L"Auto";
			return NumberText(typed.value);
		}
		}
		return L"";
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	ControlPropertyEditorKind ResolveEditor(
		ControlPropertyEditorKind requested,
		DesignerStyleValueKind kind,
		bool hasChoices)
	{
		if (hasChoices) return ControlPropertyEditorKind::Choice;
		if (requested != ControlPropertyEditorKind::Auto) return requested;
		switch (kind)
		{
		case DesignerStyleValueKind::Bool: return ControlPropertyEditorKind::Boolean;
		case DesignerStyleValueKind::Int:
		case DesignerStyleValueKind::Int64:
		case DesignerStyleValueKind::Float:
		case DesignerStyleValueKind::Double:
			return ControlPropertyEditorKind::Number;
		case DesignerStyleValueKind::Color: return ControlPropertyEditorKind::Color;
		case DesignerStyleValueKind::Thickness: return ControlPropertyEditorKind::Thickness;
		case DesignerStyleValueKind::Size: return ControlPropertyEditorKind::Size;
		case DesignerStyleValueKind::Length: return ControlPropertyEditorKind::Length;
		case DesignerStyleValueKind::String:
		default:
			return ControlPropertyEditorKind::Text;
		}
	}

	bool TryCreateDescriptor(
		Control& target,
		const BindingPropertyMetadata& metadata,
		DesignerPropertyDescriptor& out)
	{
		if (!metadata.CanWrite()) return false;
		DesignerStyleValueKind kind;
		if (!TryGetStyleValueKind(metadata, kind)) return false;

		BindingValue sample;
		if (!metadata.TryGet(target, sample))
			(void)metadata.TryGetDefaultValue(sample);
		const auto& design = metadata.Design();
		out.Name = metadata.Name();
		out.DisplayName = design.DisplayName.empty()
			? metadata.Name() : design.DisplayName;
		out.Category = design.Category.empty() ? L"Misc" : design.Category;
		out.CategoryOrder = design.CategoryOrder;
		out.Order = design.Order;
		out.ValueKind = kind;
		out.SampleValue = FormatValue(sample, kind, metadata.ValueType());
		out.Minimum = design.Minimum;
		out.Maximum = design.Maximum;
		out.Step = design.Step;
		out.Persistence = design.Persistence;
		out.Metadata = &metadata;
		for (const auto& choice : design.Choices)
		{
			BindingValue converted;
			BindingValue effective;
			if (!metadata.TryConvert(choice.Value, converted)
				|| !metadata.TryCoerce(target, converted, effective)) continue;
			out.Choices.push_back({
				choice.DisplayName,
				FormatValue(effective, kind, metadata.ValueType()) });
		}
		out.Editor = ResolveEditor(design.Editor, kind, !out.Choices.empty());
		return true;
	}
}

bool TryGetStyleValueKind(
	const BindingPropertyMetadata& metadata,
	DesignerStyleValueKind& out)
{
	switch (metadata.ValueKind())
	{
	case BindingValueKind::Bool: out = DesignerStyleValueKind::Bool; return true;
	case BindingValueKind::Int: out = DesignerStyleValueKind::Int; return true;
	case BindingValueKind::Int64: out = DesignerStyleValueKind::Int64; return true;
	case BindingValueKind::Float: out = DesignerStyleValueKind::Float; return true;
	case BindingValueKind::Double: out = DesignerStyleValueKind::Double; return true;
	case BindingValueKind::String: out = DesignerStyleValueKind::String; return true;
	case BindingValueKind::Object:
		break;
	case BindingValueKind::Empty:
	default:
		return false;
	}

	const auto& type = metadata.ValueType();
	if (type == std::type_index(typeid(D2D1_COLOR_F)))
		out = DesignerStyleValueKind::Color;
	else if (type == std::type_index(typeid(Thickness)))
		out = DesignerStyleValueKind::Thickness;
	else if (type == std::type_index(typeid(SIZE)))
		out = DesignerStyleValueKind::Size;
	else if (type == std::type_index(typeid(cui::layout::Length)))
		out = DesignerStyleValueKind::Length;
	else if (type == std::type_index(typeid(HorizontalAlignment))
		|| type == std::type_index(typeid(VerticalAlignment))
		|| type == std::type_index(typeid(Dock))
		|| type == std::type_index(typeid(Orientation))
		|| type == std::type_index(typeid(DateTimePickerMode))
		|| type == std::type_index(typeid(AccessibleRole)))
		out = DesignerStyleValueKind::Int;
	else
		return false;
	return true;
}

std::vector<DesignerPropertyDescriptor> GetStyleProperties(Control& target)
{
	std::vector<DesignerPropertyDescriptor> result;
	for (const auto* metadata : BindingPropertyRegistry::GetProperties(target))
	{
		if (!metadata) continue;
		DesignerPropertyDescriptor property;
		if (TryCreateDescriptor(target, *metadata, property))
			result.push_back(std::move(property));
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

std::vector<DesignerPropertyDescriptor> GetBrowsableProperties(Control& target)
{
	auto result = GetStyleProperties(target);
	result.erase(std::remove_if(result.begin(), result.end(), [&](const auto& property)
	{
		if (!property.Metadata || !property.Metadata->IsDesignerBrowsable(target)) return true;
		return property.Persistence == ControlPropertyPersistence::Legacy
			|| property.Persistence == ControlPropertyPersistence::Transient;
	}), result.end());
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		if (left.CategoryOrder != right.CategoryOrder)
			return left.CategoryOrder < right.CategoryOrder;
		const auto leftCategory = Lower(left.Category);
		const auto rightCategory = Lower(right.Category);
		if (leftCategory != rightCategory) return leftCategory < rightCategory;
		if (left.Order != right.Order) return left.Order < right.Order;
		const auto leftDisplay = Lower(left.DisplayName);
		const auto rightDisplay = Lower(right.DisplayName);
		if (leftDisplay != rightDisplay) return leftDisplay < rightDisplay;
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

const DesignerPropertyDescriptor* Find(
	const std::vector<DesignerPropertyDescriptor>& properties,
	const std::wstring& name)
{
	const auto found = std::find_if(properties.begin(), properties.end(),
		[&](const DesignerPropertyDescriptor& property)
		{
			return EqualsName(property.Name, name);
		});
	return found == properties.end() ? nullptr : &*found;
}

bool ValidateStyleValue(
	Control& target,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outError)
{
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata)
		return Fail(L"目标类型没有可样式化属性：" + propertyName, outError);
	if (!metadata->CanWrite())
		return Fail(L"目标属性不可写：" + propertyName, outError);
	DesignerStyleValueKind expected;
	if (!TryGetStyleValueKind(*metadata, expected))
		return Fail(L"Designer 尚不支持属性类型：" + propertyName, outError);
	if (value.Kind != expected)
		return Fail(L"属性 " + propertyName + L" 需要 "
			+ DesignerStyleSheetUtils::ValueKindName(expected) + L" 值。", outError);

	BindingValue parsed;
	if (!DesignerStyleSheetUtils::TryConvertValue(value, parsed, outError)) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(parsed, converted)
		|| !metadata->TryCoerce(target, converted, effective))
		return Fail(L"属性值无法通过元数据转换或 Coerce：" + propertyName, outError);
	if (outError) outError->clear();
	return true;
}

bool CaptureValue(
	Control& target,
	const std::wstring& propertyName,
	std::wstring* outCanonicalName,
	DesignerStyleValue& out,
	std::wstring* outError)
{
	const auto properties = GetStyleProperties(target);
	const auto* property = Find(properties, propertyName);
	if (!property)
		return Fail(L"目标类型没有可持久化的元数据属性：" + propertyName, outError);
	out = DesignerStyleValue{ property->ValueKind, property->SampleValue };
	if (outCanonicalName) *outCanonicalName = property->Name;
	if (outError) outError->clear();
	return true;
}

bool ApplyValue(
	Control& target,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outCanonicalName,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	if (!ValidateStyleValue(target, propertyName, value, outError)) return false;
	BindingValue parsed;
	if (!DesignerStyleSheetUtils::TryConvertValue(value, parsed, outError)) return false;
	const auto* metadata = target.FindPropertyMetadata(propertyName);
	if (!metadata || !target.TrySetPropertyValue(metadata->Name(), parsed))
		return Fail(L"无法设置元数据属性；它可能正被 Binding 占用："
			+ propertyName, outError);

	DesignerStyleValue effective;
	std::wstring canonicalName;
	if (!CaptureValue(target, metadata->Name(), &canonicalName, effective, outError))
		return false;
	if (outCanonicalName) *outCanonicalName = std::move(canonicalName);
	if (outEffective) *outEffective = std::move(effective);
	if (outError) outError->clear();
	return true;
}
}
