#include "DesignerStyleSheetUtils.h"
#include "DesignerPropertyCatalog.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <unordered_set>

namespace DesignerStyleSheetUtils
{
namespace
{
	constexpr uint32_t KnownStateMask =
		static_cast<uint32_t>(ControlStyleState::Hovered)
		| static_cast<uint32_t>(ControlStyleState::Focused)
		| static_cast<uint32_t>(ControlStyleState::Pressed)
		| static_cast<uint32_t>(ControlStyleState::Disabled)
		| static_cast<uint32_t>(ControlStyleState::Checked)
		| static_cast<uint32_t>(ControlStyleState::Selected);

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	bool EqualsName(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::vector<std::wstring> Split(const std::wstring& value, wchar_t delimiter)
	{
		std::vector<std::wstring> result;
		std::wstring current;
		for (wchar_t ch : value)
		{
			if (ch == delimiter)
			{
				result.push_back(Trim(current));
				current.clear();
			}
			else
			{
				current.push_back(ch);
			}
		}
		result.push_back(Trim(current));
		return result;
	}

	bool TryParseLongLong(const std::wstring& text, long long& out)
	{
		const auto trimmed = Trim(text);
		if (trimmed.empty()) return false;
		wchar_t* end = nullptr;
		errno = 0;
		const auto value = std::wcstoll(trimmed.c_str(), &end, 10);
		if (errno == ERANGE || !end || *end != L'\0') return false;
		out = value;
		return true;
	}

	bool TryParseDouble(const std::wstring& text, double& out)
	{
		const auto trimmed = Trim(text);
		if (trimmed.empty()) return false;
		wchar_t* end = nullptr;
		errno = 0;
		const auto value = std::wcstod(trimmed.c_str(), &end);
		if (errno == ERANGE || !end || *end != L'\0' || !std::isfinite(value))
			return false;
		out = value;
		return true;
	}

	bool TryParseFloat(const std::wstring& text, float& out)
	{
		double value = 0.0;
		if (!TryParseDouble(text, value)
			|| value < -static_cast<double>((std::numeric_limits<float>::max)())
			|| value > static_cast<double>((std::numeric_limits<float>::max)()))
			return false;
		out = static_cast<float>(value);
		return true;
	}

	bool TryParseHexByte(const std::wstring& value, size_t offset, unsigned int& out)
	{
		if (offset + 2 > value.size()) return false;
		wchar_t buffer[3]{ value[offset], value[offset + 1], L'\0' };
		wchar_t* end = nullptr;
		const auto parsed = std::wcstoul(buffer, &end, 16);
		if (!end || *end != L'\0' || parsed > 255) return false;
		out = static_cast<unsigned int>(parsed);
		return true;
	}

	bool TryParseColor(const std::wstring& text, D2D1_COLOR_F& out)
	{
		const auto value = Trim(text);
		if (value.size() == 7 || value.size() == 9)
		{
			if (value.front() != L'#') return false;
			unsigned int a = 255, r = 0, g = 0, b = 0;
			const size_t rgbOffset = value.size() == 9 ? 3 : 1;
			if (value.size() == 9 && !TryParseHexByte(value, 1, a)) return false;
			if (!TryParseHexByte(value, rgbOffset, r)
				|| !TryParseHexByte(value, rgbOffset + 2, g)
				|| !TryParseHexByte(value, rgbOffset + 4, b)) return false;
			out = D2D1_COLOR_F{
				static_cast<float>(r) / 255.0f,
				static_cast<float>(g) / 255.0f,
				static_cast<float>(b) / 255.0f,
				static_cast<float>(a) / 255.0f };
			return true;
		}

		auto parts = Split(value, L',');
		if (parts.size() != 3 && parts.size() != 4) return false;
		float channels[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		for (size_t index = 0; index < parts.size(); ++index)
		{
			if (!TryParseFloat(parts[index], channels[index])
				|| channels[index] < 0.0f || channels[index] > 1.0f) return false;
		}
		out = D2D1_COLOR_F{ channels[0], channels[1], channels[2], channels[3] };
		return true;
	}

	bool TryParseFloatList(
		const std::wstring& text,
		std::initializer_list<size_t> allowedCounts,
		std::vector<float>& out)
	{
		auto parts = Split(text, L',');
		if (std::find(allowedCounts.begin(), allowedCounts.end(), parts.size())
			== allowedCounts.end()) return false;
		out.clear();
		out.reserve(parts.size());
		for (const auto& part : parts)
		{
			float value = 0.0f;
			if (!TryParseFloat(part, value)) return false;
			out.push_back(value);
		}
		return true;
	}

	bool TryParseStateName(const std::wstring& value, ControlStyleState& out)
	{
		if (EqualsName(value, L"Hovered")) out = ControlStyleState::Hovered;
		else if (EqualsName(value, L"Focused")) out = ControlStyleState::Focused;
		else if (EqualsName(value, L"Pressed")) out = ControlStyleState::Pressed;
		else if (EqualsName(value, L"Disabled")) out = ControlStyleState::Disabled;
		else if (EqualsName(value, L"Checked")) out = ControlStyleState::Checked;
		else if (EqualsName(value, L"Selected")) out = ControlStyleState::Selected;
		else return false;
		return true;
	}

	bool ContainsName(const std::vector<std::wstring>& values, const std::wstring& value)
	{
		return std::any_of(values.begin(), values.end(),
			[&](const std::wstring& item) { return EqualsName(item, value); });
	}

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}
}

std::wstring Trim(const std::wstring& value)
{
	size_t begin = 0;
	while (begin < value.size() && std::iswspace(value[begin])) ++begin;
	size_t end = value.size();
	while (end > begin && std::iswspace(value[end - 1])) --end;
	return value.substr(begin, end - begin);
}

std::wstring ValueKindName(DesignerStyleValueKind kind)
{
	switch (kind)
	{
	case DesignerStyleValueKind::Bool: return L"Bool";
	case DesignerStyleValueKind::Int: return L"Int";
	case DesignerStyleValueKind::Int64: return L"Int64";
	case DesignerStyleValueKind::Float: return L"Float";
	case DesignerStyleValueKind::Double: return L"Double";
	case DesignerStyleValueKind::String: return L"String";
	case DesignerStyleValueKind::Color: return L"Color";
	case DesignerStyleValueKind::Thickness: return L"Thickness";
	case DesignerStyleValueKind::Size: return L"Size";
	case DesignerStyleValueKind::Length: return L"Length";
	}
	return L"String";
}

bool TryParseValueKind(const std::wstring& value, DesignerStyleValueKind& out)
{
	for (auto kind : { DesignerStyleValueKind::Bool, DesignerStyleValueKind::Int,
		DesignerStyleValueKind::Int64, DesignerStyleValueKind::Float,
		DesignerStyleValueKind::Double, DesignerStyleValueKind::String,
		DesignerStyleValueKind::Color, DesignerStyleValueKind::Thickness,
		DesignerStyleValueKind::Size, DesignerStyleValueKind::Length })
	{
		if (EqualsName(Trim(value), ValueKindName(kind)))
		{
			out = kind;
			return true;
		}
	}
	return false;
}

std::vector<std::wstring> ValueKindNames()
{
	return { L"Bool", L"Int", L"Int64", L"Float", L"Double", L"String",
		L"Color", L"Thickness", L"Size", L"Length" };
}

std::wstring UIClassName(UIClass type)
{
#define CUI_STYLE_WIDEN_INNER(value) L##value
#define CUI_STYLE_WIDEN(value) CUI_STYLE_WIDEN_INNER(value)
#define CUI_STYLE_UI_NAME(name) case UIClass::UI_##name: return CUI_STYLE_WIDEN(#name)
	switch (type)
	{
	CUI_STYLE_UI_NAME(Base); CUI_STYLE_UI_NAME(Label); CUI_STYLE_UI_NAME(LinkLabel);
	CUI_STYLE_UI_NAME(Button); CUI_STYLE_UI_NAME(PictureBox); CUI_STYLE_UI_NAME(TextBox);
	CUI_STYLE_UI_NAME(RichTextBox); CUI_STYLE_UI_NAME(PasswordBox); CUI_STYLE_UI_NAME(ComboBox);
	CUI_STYLE_UI_NAME(ListView); CUI_STYLE_UI_NAME(ListBox); CUI_STYLE_UI_NAME(GridView);
	CUI_STYLE_UI_NAME(PropertyGrid); CUI_STYLE_UI_NAME(CheckBox); CUI_STYLE_UI_NAME(RadioBox);
	CUI_STYLE_UI_NAME(ProgressBar); CUI_STYLE_UI_NAME(LoadingRing); CUI_STYLE_UI_NAME(ProgressRing);
	CUI_STYLE_UI_NAME(TreeView); CUI_STYLE_UI_NAME(Panel); CUI_STYLE_UI_NAME(GroupBox);
	CUI_STYLE_UI_NAME(ScrollView); CUI_STYLE_UI_NAME(TabPage); CUI_STYLE_UI_NAME(TabControl);
	CUI_STYLE_UI_NAME(Switch); CUI_STYLE_UI_NAME(Menu); CUI_STYLE_UI_NAME(MenuItem);
	CUI_STYLE_UI_NAME(ToolBar); CUI_STYLE_UI_NAME(StatusBar); CUI_STYLE_UI_NAME(Slider);
	CUI_STYLE_UI_NAME(WebBrowser); CUI_STYLE_UI_NAME(MediaPlayer); CUI_STYLE_UI_NAME(StackPanel);
	CUI_STYLE_UI_NAME(GridPanel); CUI_STYLE_UI_NAME(DockPanel); CUI_STYLE_UI_NAME(WrapPanel);
	CUI_STYLE_UI_NAME(RelativePanel); CUI_STYLE_UI_NAME(SplitContainer); CUI_STYLE_UI_NAME(DateTimePicker);
	CUI_STYLE_UI_NAME(ToolTip); CUI_STYLE_UI_NAME(ContextMenu); CUI_STYLE_UI_NAME(ToastHost);
	CUI_STYLE_UI_NAME(ChartView); CUI_STYLE_UI_NAME(ReportView); CUI_STYLE_UI_NAME(KpiCard);
	CUI_STYLE_UI_NAME(FilterBar); CUI_STYLE_UI_NAME(NavigationView); CUI_STYLE_UI_NAME(SideBar);
	CUI_STYLE_UI_NAME(BreadcrumbBar); CUI_STYLE_UI_NAME(CalendarView); CUI_STYLE_UI_NAME(DateRangePicker);
	CUI_STYLE_UI_NAME(ColorPicker); CUI_STYLE_UI_NAME(PagedGridView); CUI_STYLE_UI_NAME(NumericUpDown);
	CUI_STYLE_UI_NAME(Expander); CUI_STYLE_UI_NAME(CUSTOM);
	}
#undef CUI_STYLE_UI_NAME
#undef CUI_STYLE_WIDEN
#undef CUI_STYLE_WIDEN_INNER
	return L"Base";
}

bool TryParseUIClass(const std::wstring& value, UIClass& out)
{
	const auto name = Trim(value);
	for (int numeric = static_cast<int>(UIClass::UI_Base);
		numeric <= static_cast<int>(UIClass::UI_CUSTOM); ++numeric)
	{
		const auto candidate = static_cast<UIClass>(numeric);
		if (EqualsName(name, UIClassName(candidate)))
		{
			out = candidate;
			return true;
		}
	}
	return false;
}

std::vector<std::wstring> UIClassNames(bool includeAny)
{
	std::vector<std::wstring> result;
	if (includeAny) result.push_back(L"Any");
	for (int numeric = static_cast<int>(UIClass::UI_Base);
		numeric <= static_cast<int>(UIClass::UI_CUSTOM); ++numeric)
		result.push_back(UIClassName(static_cast<UIClass>(numeric)));
	return result;
}

std::wstring FormatStates(ControlStyleState states)
{
	std::wstring result;
	for (const auto& [state, name] : {
		std::pair{ ControlStyleState::Hovered, L"Hovered" },
		std::pair{ ControlStyleState::Focused, L"Focused" },
		std::pair{ ControlStyleState::Pressed, L"Pressed" },
		std::pair{ ControlStyleState::Disabled, L"Disabled" },
		std::pair{ ControlStyleState::Checked, L"Checked" },
		std::pair{ ControlStyleState::Selected, L"Selected" } })
	{
		if ((states & state) == ControlStyleState::None) continue;
		if (!result.empty()) result += L", ";
		result += name;
	}
	return result;
}

bool TryParseStates(const std::wstring& value, ControlStyleState& out)
{
	out = ControlStyleState::None;
	std::wstring normalized = value;
	std::replace(normalized.begin(), normalized.end(), L'|', L',');
	for (const auto& part : Split(normalized, L','))
	{
		if (part.empty()) continue;
		ControlStyleState state = ControlStyleState::None;
		if (!TryParseStateName(part, state)) return false;
		out |= state;
	}
	return true;
}

std::vector<std::wstring> SplitClasses(const std::wstring& value)
{
	std::vector<std::wstring> result;
	for (const auto& item : Split(value, L','))
	{
		if (!item.empty() && !ContainsName(result, item)) result.push_back(item);
	}
	return result;
}

std::wstring JoinClasses(const std::vector<std::wstring>& classes)
{
	std::wstring result;
	for (const auto& value : classes)
	{
		if (!result.empty()) result += L", ";
		result += value;
	}
	return result;
}

bool TryConvertValue(
	const DesignerStyleValue& value,
	BindingValue& out,
	std::wstring* outError)
{
	const auto invalid = [&]()
	{
		return Fail(L"样式值不是有效的 " + ValueKindName(value.Kind)
			+ L"：" + value.Text, outError);
	};

	switch (value.Kind)
	{
	case DesignerStyleValueKind::Bool:
	{
		const auto text = Lower(Trim(value.Text));
		if (text == L"true" || text == L"1") out = BindingValue(true);
		else if (text == L"false" || text == L"0") out = BindingValue(false);
		else return invalid();
		return true;
	}
	case DesignerStyleValueKind::Int:
	{
		long long parsed = 0;
		if (!TryParseLongLong(value.Text, parsed)
			|| parsed < (std::numeric_limits<int>::min)()
			|| parsed > (std::numeric_limits<int>::max)()) return invalid();
		out = BindingValue(static_cast<int>(parsed));
		return true;
	}
	case DesignerStyleValueKind::Int64:
	{
		long long parsed = 0;
		if (!TryParseLongLong(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::Float:
	{
		float parsed = 0.0f;
		if (!TryParseFloat(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::Double:
	{
		double parsed = 0.0;
		if (!TryParseDouble(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::String:
		out = BindingValue(value.Text);
		return true;
	case DesignerStyleValueKind::Color:
	{
		D2D1_COLOR_F parsed{};
		if (!TryParseColor(value.Text, parsed)) return invalid();
		out = BindingValue(parsed);
		return true;
	}
	case DesignerStyleValueKind::Thickness:
	{
		std::vector<float> parsed;
		if (!TryParseFloatList(value.Text, { 1, 2, 4 }, parsed)) return invalid();
		Thickness thickness;
		if (parsed.size() == 1) thickness = Thickness(parsed[0]);
		else if (parsed.size() == 2) thickness = Thickness(parsed[0], parsed[1]);
		else thickness = Thickness(parsed[0], parsed[1], parsed[2], parsed[3]);
		out = BindingValue(thickness);
		return true;
	}
	case DesignerStyleValueKind::Size:
	{
		auto parts = Split(value.Text, L',');
		long long width = 0, height = 0;
		if (parts.size() != 2 || !TryParseLongLong(parts[0], width)
			|| !TryParseLongLong(parts[1], height)
			|| width < (std::numeric_limits<LONG>::min)()
			|| width > (std::numeric_limits<LONG>::max)()
			|| height < (std::numeric_limits<LONG>::min)()
			|| height > (std::numeric_limits<LONG>::max)()) return invalid();
		out = BindingValue(SIZE{ static_cast<LONG>(width), static_cast<LONG>(height) });
		return true;
	}
	case DesignerStyleValueKind::Length:
	{
		if (EqualsName(Trim(value.Text), L"Auto"))
		{
			out = BindingValue(cui::layout::Length::Auto());
			return true;
		}
		float parsed = 0.0f;
		if (!TryParseFloat(value.Text, parsed) || parsed < 0.0f) return invalid();
		out = BindingValue(cui::layout::Length::Fixed(parsed));
		return true;
	}
	}
	return invalid();
}

void Canonicalize(DesignerStyleSheet& styleSheet)
{
	for (auto& resource : styleSheet.Resources)
	{
		resource.Key = Trim(resource.Key);
		if (resource.Value.Kind != DesignerStyleValueKind::String)
			resource.Value.Text = Trim(resource.Value.Text);
	}
	for (auto& rule : styleSheet.Rules)
	{
		rule.Id = Trim(rule.Id);
		auto classes = rule.Classes;
		rule.Classes.clear();
		for (auto& value : classes)
		{
			value = Trim(value);
			if (!value.empty() && !ContainsName(rule.Classes, value))
				rule.Classes.push_back(std::move(value));
		}
		for (auto& setter : rule.Setters)
		{
			setter.PropertyName = Trim(setter.PropertyName);
			setter.ResourceKey = Trim(setter.ResourceKey);
			if (setter.Literal.Kind != DesignerStyleValueKind::String)
				setter.Literal.Text = Trim(setter.Literal.Text);
		}
	}
}

bool Validate(const DesignerStyleSheet& styleSheet, std::wstring* outError)
{
	if (outError) outError->clear();
	std::vector<std::wstring> resourceKeys;
	for (const auto& resource : styleSheet.Resources)
	{
		const auto key = Trim(resource.Key);
		if (key.empty()) return Fail(L"样式资源键不能为空。", outError);
		if (ContainsName(resourceKeys, key))
			return Fail(L"样式资源键重复：" + key, outError);
		BindingValue value;
		if (!TryConvertValue(resource.Value, value, outError)) return false;
		resourceKeys.push_back(key);
	}

	for (size_t ruleIndex = 0; ruleIndex < styleSheet.Rules.size(); ++ruleIndex)
	{
		const auto& rule = styleSheet.Rules[ruleIndex];
		const auto required = static_cast<uint32_t>(rule.RequiredStates);
		const auto excluded = static_cast<uint32_t>(rule.ExcludedStates);
		if ((required & ~KnownStateMask) != 0 || (excluded & ~KnownStateMask) != 0)
			return Fail(L"样式规则包含未知状态。", outError);
		if ((required & excluded) != 0)
			return Fail(L"同一状态不能同时出现在规则的必需和排除状态中。", outError);
		if (rule.Setters.empty())
			return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1) + L" 没有 Setter。", outError);

		std::vector<std::wstring> properties;
		for (const auto& setter : rule.Setters)
		{
			const auto property = Trim(setter.PropertyName);
			if (property.empty()) return Fail(L"样式 Setter 属性名不能为空。", outError);
			if (ContainsName(properties, property))
				return Fail(L"同一规则中的 Setter 属性重复：" + property, outError);
			properties.push_back(property);
			if (setter.UsesResource)
			{
				const auto key = Trim(setter.ResourceKey);
				if (key.empty() || !ContainsName(resourceKeys, key))
					return Fail(L"样式 Setter 引用了不存在的资源：" + key, outError);
			}
			else
			{
				BindingValue value;
				if (!TryConvertValue(setter.Literal, value, outError)) return false;
			}
		}
	}
	return true;
}

bool ValidateAgainstPropertyMetadata(
	const DesignerStyleSheet& styleSheet,
	const ControlFactory& controlFactory,
	std::wstring* outError)
{
	if (!Validate(styleSheet, outError)) return false;
	if (!controlFactory)
	{
		if (outError) outError->clear();
		return true;
	}

	for (size_t ruleIndex = 0; ruleIndex < styleSheet.Rules.size(); ++ruleIndex)
	{
		const auto& rule = styleSheet.Rules[ruleIndex];
		auto target = controlFactory(rule.HasType ? rule.Type : UIClass::UI_Base);
		// Unknown/custom runtime types may not have a Designer probe. Keep their
		// structurally valid declarations for forward compatibility.
		if (!target) continue;
		const auto properties = DesignerPropertyCatalog::GetStyleProperties(*target);

		for (const auto& setter : rule.Setters)
		{
			const auto* property = DesignerPropertyCatalog::Find(
				properties, setter.PropertyName);
			if (!property)
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L" 的目标类型没有可样式化属性：" + setter.PropertyName,
					outError);

			const DesignerStyleValue* value = &setter.Literal;
			if (setter.UsesResource)
			{
				const auto resource = std::find_if(
					styleSheet.Resources.begin(), styleSheet.Resources.end(),
					[&](const DesignerStyleResource& item)
					{
						return EqualsName(item.Key, setter.ResourceKey);
					});
				if (resource == styleSheet.Resources.end())
					return Fail(L"样式 Setter 引用了不存在的资源："
						+ setter.ResourceKey, outError);
				value = &resource->Value;
			}

			std::wstring validationError;
			if (!DesignerPropertyCatalog::ValidateStyleValue(
				*target, setter.PropertyName, *value, &validationError))
				return Fail(L"样式规则 " + std::to_wstring(ruleIndex + 1)
					+ L"：" + validationError, outError);
		}
	}
	if (outError) outError->clear();
	return true;
}

bool BuildRuntimeStyleSheet(
	const DesignerStyleSheet& source,
	std::shared_ptr<ControlStyleSheet>& out,
	std::wstring* outError)
{
	auto styleSheet = source;
	Canonicalize(styleSheet);
	if (!Validate(styleSheet, outError)) return false;

	auto runtime = std::make_shared<ControlStyleSheet>();
	for (const auto& resource : styleSheet.Resources)
	{
		BindingValue value;
		if (!TryConvertValue(resource.Value, value, outError)) return false;
		if (!runtime->SetResource(resource.Key, std::move(value)))
			return Fail(L"无法创建样式资源：" + resource.Key, outError);
	}
	for (const auto& rule : styleSheet.Rules)
	{
		ControlStyleSelector selector;
		if (rule.HasType) selector.Type = rule.Type;
		selector.Id = rule.Id;
		selector.Classes = rule.Classes;
		selector.RequiredStates = rule.RequiredStates;
		selector.ExcludedStates = rule.ExcludedStates;
		std::vector<ControlStyleSetter> setters;
		setters.reserve(rule.Setters.size());
		for (const auto& setter : rule.Setters)
		{
			if (setter.UsesResource)
				setters.push_back(ControlStyleSetter::Resource(
					setter.PropertyName, setter.ResourceKey));
			else
			{
				BindingValue value;
				if (!TryConvertValue(setter.Literal, value, outError)) return false;
				setters.emplace_back(setter.PropertyName, std::move(value));
			}
		}
		runtime->AddRule(std::move(selector), std::move(setters));
	}
	out = std::move(runtime);
	return true;
}
}
