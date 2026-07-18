#include "DesignerControlPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>

namespace DesignerControlPropertyCatalog
{
namespace
{
	struct Entry
	{
		DesignerControlPropertyDescriptor Descriptor;
		std::function<bool(const DesignerControl&)> Browsable;
		std::function<BindingValue(
			const DesignerControl&, const DesignerControlPropertyContext&)> Get;
		std::function<bool(
			DesignerControl&, DesignerControlPropertyContext&, const BindingValue&)> Set;
		std::function<BindingValue(
			const DesignerControl&, const DesignerControlPropertyContext&)> Default;
	};

	bool NamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	std::wstring Trim(std::wstring value)
	{
		const auto first = std::find_if_not(value.begin(), value.end(), iswspace);
		const auto last = std::find_if_not(value.rbegin(), value.rend(), iswspace).base();
		return first < last ? std::wstring(first, last) : std::wstring{};
	}

	std::vector<std::wstring> SplitClasses(const std::wstring& value)
	{
		std::vector<std::wstring> result;
		size_t start = 0;
		while (start <= value.size())
		{
			const auto end = value.find(L',', start);
			auto item = Trim(value.substr(start,
				end == std::wstring::npos ? std::wstring::npos : end - start));
			if (!item.empty()) result.push_back(std::move(item));
			if (end == std::wstring::npos) break;
			start = end + 1;
		}
		return result;
	}

	std::wstring JoinClasses(const Control& target)
	{
		std::wstring result;
		for (const auto& item : target.GetStyleClasses())
		{
			if (!result.empty()) result += L", ";
			result += item;
		}
		return result;
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	std::wstring NumberText(float value)
	{
		std::wostringstream stream;
		stream << std::setprecision(7) << value;
		return stream.str();
	}

	DesignerStyleValue FormatValue(
		const BindingValue& value,
		DesignerStyleValueKind kind)
	{
		DesignerStyleValue result{ kind, L"" };
		switch (kind)
		{
		case DesignerStyleValueKind::Bool:
		{
			bool typed = false;
			(void)value.TryGet(typed);
			result.Text = typed ? L"true" : L"false";
			break;
		}
		case DesignerStyleValueKind::Int:
		{
			int typed = 0;
			(void)value.TryGet(typed);
			result.Text = std::to_wstring(typed);
			break;
		}
		case DesignerStyleValueKind::Float:
		{
			float typed = 0.0f;
			(void)value.TryGet(typed);
			result.Text = NumberText(typed);
			break;
		}
		case DesignerStyleValueKind::String:
			(void)value.TryGet(result.Text);
			break;
		default:
			break;
		}
		return result;
	}

	DesignerControlPropertyDescriptor Property(
		const wchar_t* name,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DesignerStyleValueKind kind,
		DesignerControlPropertyEditorKind editor,
		bool canReset)
	{
		return {
			name, name, category, categoryOrder, order, kind, editor, canReset };
	}

	const std::vector<Entry>& Entries()
	{
		static const std::vector<Entry> entries = []
		{
			std::vector<Entry> result;
			auto always = [](const DesignerControl&) { return true; };
			using Browsable = std::function<bool(const DesignerControl&)>;
			using Getter = std::function<BindingValue(
				const DesignerControl&, const DesignerControlPropertyContext&)>;
			using Setter = std::function<bool(
				DesignerControl&, DesignerControlPropertyContext&, const BindingValue&)>;
			auto add = [&](DesignerControlPropertyDescriptor descriptor,
				Browsable browsable, Getter getter, Setter setter,
				Getter defaultValue = {})
			{
				Entry entry;
				entry.Descriptor = std::move(descriptor);
				entry.Browsable = std::move(browsable);
				entry.Get = std::move(getter);
				entry.Set = std::move(setter);
				entry.Default = std::move(defaultValue);
				result.push_back(std::move(entry));
			};

			add(Property(L"Name", L"Common", 0, 10,
				DesignerStyleValueKind::String,
				DesignerControlPropertyEditorKind::Text, false), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.Name);
				},
				[](DesignerControl& target, DesignerControlPropertyContext& context,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed)) return false;
					target.Name = context.MakeUniqueName
						? context.MakeUniqueName(target, typed) : std::move(typed);
					if (context.SyncDefaultNameCounter)
						context.SyncDefaultNameCounter(target.Type, target.Name);
					return true;
				});

			add(Property(L"Locked", L"Common", 0, 20,
				DesignerStyleValueKind::Bool,
				DesignerControlPropertyEditorKind::Boolean, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.IsLocked);
				},
				[](DesignerControl& target, DesignerControlPropertyContext&,
					const BindingValue& value)
				{
					bool typed = false;
					if (!value.TryGet(typed)) return false;
					target.IsLocked = typed;
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(false);
				});

			add(Property(L"Anchor", L"Layout", 100, 10,
				DesignerStyleValueKind::Int,
				DesignerControlPropertyEditorKind::Anchor, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.ControlInstance
						? static_cast<int>(target.ControlInstance->AnchorStyles) : 0);
				},
				[](DesignerControl& target, DesignerControlPropertyContext& context,
					const BindingValue& value)
				{
					int typed = 0;
					if (!value.TryGet(typed) || !target.ControlInstance) return false;
					constexpr int known = AnchorStyles::Left | AnchorStyles::Top
						| AnchorStyles::Right | AnchorStyles::Bottom;
					const auto effective = static_cast<uint8_t>(typed & known);
					if (context.ApplyAnchorStylesKeepingBounds)
						context.ApplyAnchorStylesKeepingBounds(
							target.ControlInstance, effective);
					else
						target.ControlInstance->AnchorStyles = effective;
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(0);
				});

			add(Property(L"StyleId", L"Appearance", 200, 10,
				DesignerStyleValueKind::String,
				DesignerControlPropertyEditorKind::Text, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.ControlInstance
						? target.ControlInstance->GetStyleId() : std::wstring{});
				},
				[](DesignerControl& target, DesignerControlPropertyContext&,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed) || !target.ControlInstance) return false;
					target.ControlInstance->SetStyleId(Trim(std::move(typed)));
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(std::wstring{});
				});

			add(Property(L"StyleClasses", L"Appearance", 200, 20,
				DesignerStyleValueKind::String,
				DesignerControlPropertyEditorKind::Text, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.ControlInstance
						? JoinClasses(*target.ControlInstance) : std::wstring{});
				},
				[](DesignerControl& target, DesignerControlPropertyContext&,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed) || !target.ControlInstance) return false;
					target.ControlInstance->ClearStyleClasses();
					for (auto& item : SplitClasses(typed))
						target.ControlInstance->AddStyleClass(std::move(item));
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(std::wstring{});
				});

			add(Property(L"FontName", L"Appearance", 200, 30,
				DesignerStyleValueKind::String,
				DesignerControlPropertyEditorKind::FontName, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext& context)
				{
					if (!target.ControlInstance) return BindingValue(std::wstring{});
					auto* font = target.ControlInstance->Font;
					const bool inherited = context.SharedFont
						? font == context.SharedFont : font == GetDefaultFontObject();
					return BindingValue(inherited || !font
						? std::wstring{} : font->FontName);
				},
				[](DesignerControl& target, DesignerControlPropertyContext& context,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed) || !target.ControlInstance) return false;
					typed = Trim(std::move(typed));
					if (typed.empty())
					{
						target.ControlInstance->SetFontEx(context.SharedFont, false);
						return true;
					}
					auto* current = target.ControlInstance->Font;
					const float size = current
						? current->FontSize : GetDefaultFontObject()->FontSize;
					target.ControlInstance->Font = new ::Font(typed, size);
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(std::wstring{});
				});

			add(Property(L"FontSize", L"Appearance", 200, 40,
				DesignerStyleValueKind::Float,
				DesignerControlPropertyEditorKind::FontSize, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					auto* font = target.ControlInstance
						? target.ControlInstance->Font : GetDefaultFontObject();
					return BindingValue(font ? font->FontSize : 18.0f);
				},
				[](DesignerControl& target, DesignerControlPropertyContext&,
					const BindingValue& value)
				{
					float typed = 0.0f;
					if (!value.TryGet(typed) || !std::isfinite(typed)
						|| !target.ControlInstance) return false;
					typed = (std::clamp)(typed, 1.0f, 200.0f);
					auto* current = target.ControlInstance->Font;
					const auto name = current
						? current->FontName : GetDefaultFontObject()->FontName;
					target.ControlInstance->Font = new ::Font(name, typed);
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext& context)
				{
					auto* font = context.SharedFont
						? context.SharedFont : GetDefaultFontObject();
					return BindingValue(font ? font->FontSize : 18.0f);
				});

			auto mediaOnly = [](const DesignerControl& target)
			{
				return target.Type == UIClass::UI_MediaPlayer;
			};
			add(Property(L"MediaFile", L"Data", 600, 10,
				DesignerStyleValueKind::String,
				DesignerControlPropertyEditorKind::Text, true), mediaOnly,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					const auto found = target.DesignStrings.find(L"mediaFile");
					return BindingValue(found == target.DesignStrings.end()
						? std::wstring{} : found->second);
				},
				[](DesignerControl& target, DesignerControlPropertyContext&,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed)) return false;
					typed = Trim(std::move(typed));
					if (typed.empty()) target.DesignStrings.erase(L"mediaFile");
					else target.DesignStrings[L"mediaFile"] = std::move(typed);
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(std::wstring{});
				});

			return result;
		}();
		return entries;
	}

	const Entry* FindEntry(
		const DesignerControl& target,
		const std::wstring& propertyName)
	{
		const auto& entries = Entries();
		const auto found = std::find_if(entries.begin(), entries.end(),
			[&](const Entry& entry)
			{
				return NamesEqual(entry.Descriptor.Name, propertyName)
					&& (!entry.Browsable || entry.Browsable(target));
			});
		return found == entries.end() ? nullptr : &*found;
	}
}

std::vector<DesignerControlPropertyDescriptor> GetProperties(
	const DesignerControl& target)
{
	std::vector<DesignerControlPropertyDescriptor> result;
	for (const auto& entry : Entries())
	{
		if (!entry.Browsable || entry.Browsable(target))
			result.push_back(entry.Descriptor);
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		if (left.CategoryOrder != right.CategoryOrder)
			return left.CategoryOrder < right.CategoryOrder;
		const auto leftCategory = Lower(left.Category);
		const auto rightCategory = Lower(right.Category);
		if (leftCategory != rightCategory) return leftCategory < rightCategory;
		if (left.Order != right.Order) return left.Order < right.Order;
		return Lower(left.Name) < Lower(right.Name);
	});
	return result;
}

const DesignerControlPropertyDescriptor* Find(
	const DesignerControl& target,
	const std::wstring& propertyName)
{
	const auto* entry = FindEntry(target, propertyName);
	return entry ? &entry->Descriptor : nullptr;
}

bool CaptureValue(
	const DesignerControl& target,
	const DesignerControlPropertyContext& context,
	const std::wstring& propertyName,
	DesignerStyleValue& out,
	std::wstring* outError)
{
	const auto* entry = FindEntry(target, propertyName);
	if (!entry)
		return Fail(L"控件没有设计器专用属性：" + propertyName, outError);
	out = FormatValue(entry->Get(target, context), entry->Descriptor.ValueKind);
	return true;
}

bool ApplyValue(
	DesignerControl& target,
	DesignerControlPropertyContext& context,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	const auto* entry = FindEntry(target, propertyName);
	if (!entry)
		return Fail(L"控件没有设计器专用属性：" + propertyName, outError);
	if (value.Kind != entry->Descriptor.ValueKind)
		return Fail(L"设计器专用属性类型不匹配：" + propertyName, outError);
	BindingValue converted;
	if (!DesignerStyleSheetUtils::TryConvertValue(value, converted, outError))
		return false;
	if (!entry->Set(target, context, converted))
		return Fail(L"无法应用设计器专用属性：" + propertyName, outError);
	if (outEffective)
		return CaptureValue(target, context, propertyName, *outEffective, outError);
	return true;
}

bool ResetValue(
	DesignerControl& target,
	DesignerControlPropertyContext& context,
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	const auto* entry = FindEntry(target, propertyName);
	if (!entry)
		return Fail(L"控件没有设计器专用属性：" + propertyName, outError);
	if (!entry->Descriptor.CanReset || !entry->Default)
		return Fail(L"设计器专用属性没有默认值：" + propertyName, outError);
	if (!entry->Set(target, context, entry->Default(target, context)))
		return Fail(L"无法恢复设计器专用属性：" + propertyName, outError);
	if (outEffective)
		return CaptureValue(target, context, propertyName, *outEffective, outError);
	return true;
}
}
