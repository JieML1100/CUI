#include "DesignerControlPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "../CUI/include/StyleInfrastructure.h"
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
		return left == right;
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
		case DesignerStyleValueKind::NullableBool:
		{
			NullableBool typed;
			(void)value.TryGet(typed);
			result.Text = !typed.HasValue()
				? L"{x:Null}"
				: typed.GetValueOrDefault() ? L"true" : L"false";
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
		DesignerDependencyPropertyEditorKind editor,
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
				DesignerDependencyPropertyEditorKind::Text, false), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.Name);
				},
				[](DesignerControl& target, DesignerControlPropertyContext& context,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed)) return false;
					const auto previousName = target.Name;
					target.Name = context.MakeUniqueName
						? context.MakeUniqueName(target, typed) : std::move(typed);
					if (context.RewriteElementNameReferences
						&& previousName != target.Name)
						context.RewriteElementNameReferences(
							previousName, target.Name);
					if (context.SyncDefaultNameCounter)
						context.SyncDefaultNameCounter(target.Type, target.Name);
					return true;
				});

			add(Property(L"Locked", L"Common", 0, 20,
				DesignerStyleValueKind::Bool,
				DesignerDependencyPropertyEditorKind::Boolean, true), always,
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

			add(Property(L"Style", L"Appearance", 200, 10,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Text, true), always,
				[](const DesignerControl& target, const DesignerControlPropertyContext&)
				{
					return BindingValue(target.ControlInstance
						? cui::framework::StyleAccess::ResourceKey(
							*target.ControlInstance)
						: std::wstring{});
				},
				[](DesignerControl& target, DesignerControlPropertyContext&,
					const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed) || !target.ControlInstance) return false;
					cui::framework::StyleAccess::SetResourceKey(
						*target.ControlInstance, Trim(std::move(typed)));
					return true;
				},
				[](const DesignerControl&, const DesignerControlPropertyContext&)
				{
					return BindingValue(std::wstring{});
				});

			auto itemsControlOnly = [](const DesignerControl& target)
			{
				return IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, target.Type);
			};
			auto itemTemplateControl = [](const DesignerControl& target)
			{
				return IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, target.Type);
			};
			auto dataListControl = [](const DesignerControl& target)
			{
				return IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, target.Type);
			};
			auto contentHostOnly = [](const DesignerControl& target)
			{
				return target.Type == UIClass::UI_ContentPresenter
					|| IsUIClassAssignableFrom(
						UIClass::UI_ContentControl, target.Type);
			};
			auto controlTemplateHost = [](const DesignerControl& target)
			{
				return !target.ComponentType.Empty()
					|| IsControlTemplateHostClass(target.Type);
			};
			auto headeredContentOnly = [](const DesignerControl& target)
			{
				return IsUIClassAssignableFrom(
						UIClass::UI_HeaderedContentControl, target.Type)
					|| IsUIClassAssignableFrom(
						UIClass::UI_HeaderedItemsControl, target.Type);
			};
			auto designString = [](const wchar_t* key)
			{
				return [key](const DesignerControl& target,
					const DesignerControlPropertyContext&)
				{
					const auto found = target.DesignStrings.find(key);
					return BindingValue(found == target.DesignStrings.end()
						? std::wstring{} : found->second);
				};
			};
			auto setDesignString = [](const wchar_t* key)
			{
				return [key](DesignerControl& target,
					DesignerControlPropertyContext&, const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed)) return false;
					typed = Trim(std::move(typed));
					if (typed.empty()) target.DesignStrings.erase(key);
					else target.DesignStrings[key] = std::move(typed);
					return true;
				};
			};
			auto emptyString = [](const DesignerControl&,
				const DesignerControlPropertyContext&)
			{
				return BindingValue(std::wstring{});
			};
			add(Property(L"Template", L"Appearance", 200, 5,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true),
				controlTemplateHost, designString(L"controlTemplate"),
				setDesignString(L"controlTemplate"), emptyString);
			add(Property(L"ItemsSourceResource", L"Data", 600, 20,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), dataListControl,
				designString(L"itemsSourceResource"),
				setDesignString(L"itemsSourceResource"), emptyString);
			add(Property(L"ItemTemplate", L"Data", 600, 30,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), itemTemplateControl,
				designString(L"itemTemplate"),
				setDesignString(L"itemTemplate"), emptyString);
			add(Property(L"ContentTemplate", L"Data", 80, 30,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), contentHostOnly,
				designString(L"contentTemplate"),
				setDesignString(L"contentTemplate"), emptyString);
			add(Property(L"HeaderTemplate", L"Data", 80, 50,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), headeredContentOnly,
				designString(L"headerTemplate"),
				setDesignString(L"headerTemplate"), emptyString);
			add(Property(L"GroupStyle", L"Data", 600, 40,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), itemsControlOnly,
				designString(L"groupStyle"),
				setDesignString(L"groupStyle"), emptyString);
			add(Property(L"ItemsPanel", L"Layout", 550, 20,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), itemsControlOnly,
				designString(L"itemsPanel"),
				setDesignString(L"itemsPanel"), emptyString);
			add(Property(L"ItemContainerStyle", L"Appearance", 500, 35,
				DesignerStyleValueKind::String,
				DesignerDependencyPropertyEditorKind::Choice, true), itemsControlOnly,
				designString(L"itemContainerStyle"),
				setDesignString(L"itemContainerStyle"), emptyString);

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
