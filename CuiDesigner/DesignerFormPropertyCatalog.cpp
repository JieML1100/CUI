#include "DesignerFormPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <functional>
#include <iomanip>
#include <sstream>

namespace DesignerFormPropertyCatalog
{
namespace
{
	using FormModel = DesignerModel::DesignFormModel;

	struct Entry
	{
		DesignerFormPropertyDescriptor Descriptor;
		std::function<BindingValue(const FormModel&)> Get;
		std::function<bool(FormModel&, const BindingValue&)> Set;
	};

	bool NamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	std::wstring NumberText(double value, int precision)
	{
		std::wostringstream stream;
		stream << std::setprecision(precision) << value;
		return stream.str();
	}

	unsigned int ColorByte(float value)
	{
		const auto normalized = (std::clamp)(value, 0.0f, 1.0f);
		return static_cast<unsigned int>(std::lround(normalized * 255.0f));
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
			result.Text = NumberText(typed, 7);
			break;
		}
		case DesignerStyleValueKind::String:
		{
			(void)value.TryGet(result.Text);
			break;
		}
		case DesignerStyleValueKind::Color:
		{
			D2D1_COLOR_F typed{};
			(void)value.TryGet(typed);
			wchar_t text[10]{};
			swprintf_s(text, L"#%02X%02X%02X%02X",
				ColorByte(typed.a), ColorByte(typed.r),
				ColorByte(typed.g), ColorByte(typed.b));
			result.Text = text;
			break;
		}
		default:
			break;
		}
		return result;
	}

	DesignerFormPropertyDescriptor Property(
		const wchar_t* name,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DesignerStyleValueKind kind,
		ControlPropertyEditorKind editor,
		std::optional<double> minimum = {},
		std::optional<double> maximum = {})
	{
		DesignerFormPropertyDescriptor result;
		result.Name = name;
		result.DisplayName = name;
		result.Category = category;
		result.CategoryOrder = categoryOrder;
		result.Order = order;
		result.ValueKind = kind;
		result.Editor = editor;
		result.Minimum = minimum;
		result.Maximum = maximum;
		return result;
	}

	const std::vector<Entry>& Entries()
	{
		static const std::vector<Entry> entries = []
		{
			std::vector<Entry> result;
			auto add = [&](DesignerFormPropertyDescriptor descriptor,
				auto getter, auto setter)
			{
				result.push_back({ std::move(descriptor), getter, setter });
			};

			add(Property(L"Name", L"Common", 0, 10,
				DesignerStyleValueKind::String, ControlPropertyEditorKind::Text),
				[](const FormModel& form) { return BindingValue(form.Name); },
				[](FormModel& form, const BindingValue& value)
				{
					std::wstring typed;
					if (!value.TryGet(typed)) return false;
					form.Name = typed.empty() ? L"MainForm" : std::move(typed);
					return true;
				});
			add(Property(L"Text", L"Common", 0, 20,
				DesignerStyleValueKind::String, ControlPropertyEditorKind::Text),
				[](const FormModel& form) { return BindingValue(form.Text); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.Text);
				});
			add(Property(L"Enable", L"Common", 0, 30,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.Enable); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.Enable);
				});
			add(Property(L"Visible", L"Common", 0, 40,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.Visible); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.Visible);
				});

			add(Property(L"X", L"Layout", 100, 10,
				DesignerStyleValueKind::Int, ControlPropertyEditorKind::Number),
				[](const FormModel& form) {
					return BindingValue(static_cast<int>(form.Location.x)); },
				[](FormModel& form, const BindingValue& value)
				{
					int typed = 0;
					if (!value.TryGet(typed)) return false;
					form.Location.x = typed;
					return true;
				});
			add(Property(L"Y", L"Layout", 100, 20,
				DesignerStyleValueKind::Int, ControlPropertyEditorKind::Number),
				[](const FormModel& form) {
					return BindingValue(static_cast<int>(form.Location.y)); },
				[](FormModel& form, const BindingValue& value)
				{
					int typed = 0;
					if (!value.TryGet(typed)) return false;
					form.Location.y = typed;
					return true;
				});
			add(Property(L"Width", L"Layout", 100, 30,
				DesignerStyleValueKind::Int, ControlPropertyEditorKind::Number, 50.0),
				[](const FormModel& form) {
					return BindingValue(static_cast<int>(form.Size.cx)); },
				[](FormModel& form, const BindingValue& value)
				{
					int typed = 0;
					if (!value.TryGet(typed)) return false;
					form.Size.cx = (std::max)(50, typed);
					return true;
				});
			add(Property(L"Height", L"Layout", 100, 40,
				DesignerStyleValueKind::Int, ControlPropertyEditorKind::Number, 50.0),
				[](const FormModel& form) {
					return BindingValue(static_cast<int>(form.Size.cy)); },
				[](FormModel& form, const BindingValue& value)
				{
					int typed = 0;
					if (!value.TryGet(typed)) return false;
					form.Size.cy = (std::max)(50, typed);
					return true;
				});

			add(Property(L"FontName", L"Appearance", 200, 10,
				DesignerStyleValueKind::String, ControlPropertyEditorKind::Choice),
				[](const FormModel& form) { return BindingValue(form.FontName); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.FontName);
				});
			add(Property(L"FontSize", L"Appearance", 200, 20,
				DesignerStyleValueKind::Float, ControlPropertyEditorKind::Choice,
				1.0, 200.0),
				[](const FormModel& form) { return BindingValue(form.FontSize); },
				[](FormModel& form, const BindingValue& value)
				{
					float typed = 0.0f;
					if (!value.TryGet(typed) || !std::isfinite(typed)) return false;
					form.FontSize = (std::clamp)(typed, 1.0f, 200.0f);
					return true;
				});
			add(Property(L"BackColor", L"Appearance", 200, 30,
				DesignerStyleValueKind::Color, ControlPropertyEditorKind::Color),
				[](const FormModel& form) { return BindingValue(form.BackColor); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.BackColor);
				});
			add(Property(L"ForeColor", L"Appearance", 200, 40,
				DesignerStyleValueKind::Color, ControlPropertyEditorKind::Color),
				[](const FormModel& form) { return BindingValue(form.ForeColor); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.ForeColor);
				});
			add(Property(L"VisibleHead", L"Appearance", 200, 50,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.VisibleHead); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.VisibleHead);
				});
			add(Property(L"HeadHeight", L"Appearance", 200, 60,
				DesignerStyleValueKind::Int, ControlPropertyEditorKind::Number, 0.0),
				[](const FormModel& form) { return BindingValue(form.HeadHeight); },
				[](FormModel& form, const BindingValue& value)
				{
					int typed = 0;
					if (!value.TryGet(typed)) return false;
					form.HeadHeight = (std::max)(0, typed);
					return true;
				});
			add(Property(L"CenterTitle", L"Appearance", 200, 70,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.CenterTitle); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.CenterTitle);
				});

			add(Property(L"ShowInTaskBar", L"Behavior", 300, 10,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.ShowInTaskBar); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.ShowInTaskBar);
				});
			add(Property(L"TopMost", L"Behavior", 300, 20,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.TopMost); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.TopMost);
				});
			add(Property(L"MinBox", L"Behavior", 300, 30,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.MinBox); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.MinBox);
				});
			add(Property(L"MaxBox", L"Behavior", 300, 40,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.MaxBox); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.MaxBox);
				});
			add(Property(L"CloseBox", L"Behavior", 300, 50,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.CloseBox); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.CloseBox);
				});
			add(Property(L"AllowResize", L"Behavior", 300, 60,
				DesignerStyleValueKind::Bool, ControlPropertyEditorKind::Boolean),
				[](const FormModel& form) { return BindingValue(form.AllowResize); },
				[](FormModel& form, const BindingValue& value)
				{
					return value.TryGet(form.AllowResize);
				});

			const FormModel defaults;
			for (auto& entry : result)
				entry.Descriptor.DefaultValue = FormatValue(
					entry.Get(defaults), entry.Descriptor.ValueKind);
			return result;
		}();
		return entries;
	}

	const Entry* FindEntry(const std::wstring& propertyName)
	{
		const auto& entries = Entries();
		const auto found = std::find_if(entries.begin(), entries.end(),
			[&](const Entry& entry)
			{
				return NamesEqual(entry.Descriptor.Name, propertyName);
			});
		return found == entries.end() ? nullptr : &*found;
	}
}

const std::vector<DesignerFormPropertyDescriptor>& GetProperties()
{
	static const std::vector<DesignerFormPropertyDescriptor> properties = []
	{
		std::vector<DesignerFormPropertyDescriptor> result;
		result.reserve(Entries().size());
		for (const auto& entry : Entries()) result.push_back(entry.Descriptor);
		return result;
	}();
	return properties;
}

const DesignerFormPropertyDescriptor* Find(const std::wstring& propertyName)
{
	const auto& properties = GetProperties();
	const auto found = std::find_if(properties.begin(), properties.end(),
		[&](const DesignerFormPropertyDescriptor& property)
		{
			return NamesEqual(property.Name, propertyName);
		});
	return found == properties.end() ? nullptr : &*found;
}

bool CaptureValue(
	const FormModel& form,
	const std::wstring& propertyName,
	DesignerStyleValue& out,
	std::wstring* outError)
{
	const auto* entry = FindEntry(propertyName);
	if (!entry) return Fail(L"窗体没有可设计属性：" + propertyName, outError);
	out = FormatValue(entry->Get(form), entry->Descriptor.ValueKind);
	if (outError) outError->clear();
	return true;
}

bool ApplyValue(
	FormModel& form,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	const auto* entry = FindEntry(propertyName);
	if (!entry) return Fail(L"窗体没有可设计属性：" + propertyName, outError);
	if (value.Kind != entry->Descriptor.ValueKind)
		return Fail(L"窗体属性类型不匹配：" + entry->Descriptor.Name, outError);
	BindingValue converted;
	if (!DesignerStyleSheetUtils::TryConvertValue(value, converted, outError))
		return false;
	if (!entry->Set(form, converted))
		return Fail(L"窗体属性值无效：" + entry->Descriptor.Name, outError);
	DesignerStyleValue effective = FormatValue(
		entry->Get(form), entry->Descriptor.ValueKind);
	if (outEffective) *outEffective = std::move(effective);
	if (outError) outError->clear();
	return true;
}

bool ResetValue(
	FormModel& form,
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outError)
{
	const auto* entry = FindEntry(propertyName);
	if (!entry) return Fail(L"窗体没有可重置属性：" + propertyName, outError);
	return ApplyValue(
		form, entry->Descriptor.Name, entry->Descriptor.DefaultValue,
		outEffective, outError);
}
}
