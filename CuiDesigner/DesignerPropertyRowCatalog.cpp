#include "DesignerPropertyRowCatalog.h"
#include "DesignerBindingUtils.h"
#include "../CUI/include/Style.h"
#include <algorithm>
#include <cwctype>
#include <sstream>

namespace DesignerPropertyRowCatalog
{
namespace
{
	bool NamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	const wchar_t* RowSourceName(DesignerPropertyRowSource source)
	{
		switch (source)
		{
		case DesignerPropertyRowSource::Form: return L"Form 窗体";
		case DesignerPropertyRowSource::ControlDesign: return L"Designer 设计器";
		case DesignerPropertyRowSource::CustomDescriptor: return L"Custom 自定义";
		case DesignerPropertyRowSource::RuntimeMetadata: return L"Runtime 运行时";
		default: return L"";
		}
	}

	const wchar_t* ValueSourceAliases(ControlPropertyValueSource source)
	{
		switch (source)
		{
		case ControlPropertyValueSource::Theme: return L"Theme 主题";
		case ControlPropertyValueSource::Style: return L"Style 样式";
		case ControlPropertyValueSource::Binding: return L"Binding 绑定";
		case ControlPropertyValueSource::Local: return L"Local 本地";
		case ControlPropertyValueSource::Default:
		default:
			return L"Default 默认";
		}
	}

	const wchar_t* EditorAliases(DesignerPropertyRowEditorKind editor)
	{
		switch (editor)
		{
		case DesignerPropertyRowEditorKind::Boolean: return L"Boolean Bool 布尔";
		case DesignerPropertyRowEditorKind::Choice: return L"Choice Enum 选项 枚举";
		case DesignerPropertyRowEditorKind::Color: return L"Color 颜色";
		case DesignerPropertyRowEditorKind::Thickness: return L"Thickness 边距";
		case DesignerPropertyRowEditorKind::FloatSlider: return L"Float Slider 浮点 滑块";
		case DesignerPropertyRowEditorKind::FontName: return L"Font Name 字体";
		case DesignerPropertyRowEditorKind::FontSize: return L"Font Size 字号";
		case DesignerPropertyRowEditorKind::Anchor: return L"Anchor 锚点";
		case DesignerPropertyRowEditorKind::Text:
		default:
			return L"Text 文本";
		}
	}

	const wchar_t* DiagnosticKindAliases(
		DesignerPropertyDiagnosticKind kind)
	{
		switch (kind)
		{
		case DesignerPropertyDiagnosticKind::Binding:
			return L"Binding DataBinding 绑定";
		case DesignerPropertyDiagnosticKind::Validation:
			return L"Validation 校验 验证";
		case DesignerPropertyDiagnosticKind::Style:
			return L"Style 样式 Rule 规则";
		case DesignerPropertyDiagnosticKind::Theme:
			return L"Theme 主题 Rule 规则";
		}
		return L"";
	}

	template<typename TValue>
	const TValue* FindNamed(
		const std::map<std::wstring, TValue>& values,
		const std::wstring& name)
	{
		const auto found = std::find_if(values.begin(), values.end(),
			[&name](const auto& entry)
			{
				return NamesEqual(entry.first, name);
			});
		return found == values.end() ? nullptr : &found->second;
	}

	std::wstring StyleIssueDescription(
		const ControlStyleResolutionIssue& issue)
	{
		switch (issue.Code)
		{
		case ControlStyleResolutionIssueCode::MissingResource:
			return L"缺少资源 " + issue.ResourceKey;
		case ControlStyleResolutionIssueCode::PropertyNotFound:
			return L"找不到属性 " + issue.PropertyName;
		case ControlStyleResolutionIssueCode::PropertyNotWritable:
			return L"属性不可写 " + issue.PropertyName;
		case ControlStyleResolutionIssueCode::InvalidValue:
			return L"属性值无效 " + issue.PropertyName;
		}
		return L"未知样式解析错误";
	}

	void AppendStyleDiagnostics(
		DesignerPropertyRow& row,
		const ControlStyleResolution& resolution,
		DesignerPropertyDiagnosticKind kind,
		ControlPropertyValueSource source)
	{
		const auto sourceName = kind == DesignerPropertyDiagnosticKind::Theme
			? L"Theme" : L"Style";
		for (const auto& setter : resolution.Setters)
		{
			if (!NamesEqual(setter.PropertyName, row.Name)) continue;
			DesignerPropertyDiagnostic diagnostic;
			diagnostic.Kind = kind;
			diagnostic.Severity = BindingValidationSeverity::Info;
			diagnostic.Summary = std::wstring(sourceName) + L" 规则 #"
				+ std::to_wstring(setter.RuleId) + L"，特异性 "
				+ std::to_wstring(setter.Specificity);
			if (row.EffectiveValueSource
				&& *row.EffectiveValueSource != source)
				diagnostic.Details = L"候选值被更高优先级的 "
					+ std::wstring(ControlPropertyValueSourceName(
						*row.EffectiveValueSource)) + L" 值遮蔽。";
			else
				diagnostic.Details = L"该规则提供当前有效值。";
			row.Diagnostics.push_back(std::move(diagnostic));
		}
		for (const auto& issue : resolution.Issues)
		{
			if (!issue.PropertyName.empty()
				&& !NamesEqual(issue.PropertyName, row.Name)) continue;
			row.Diagnostics.push_back({
				kind,
				BindingValidationSeverity::Error,
				std::wstring(sourceName) + L" 规则 #"
					+ std::to_wstring(issue.RuleId) + L" 解析失败",
				StyleIssueDescription(issue)
			});
		}
	}

	void AppendControlDiagnostics(
		DesignerControl& target,
		std::vector<DesignerPropertyRow>& rows)
	{
		auto* control = target.ControlInstance;
		if (!control) return;
		const auto style = control->GetStyleSheet();
		const auto theme = control->GetThemeStyleSheet();
		const auto styleResolution = style
			? style->Resolve(*control) : ControlStyleResolution{};
		const auto themeResolution = theme
			? theme->Resolve(*control) : ControlStyleResolution{};

		for (auto& row : rows)
		{
			if (control->FindPropertyMetadata(row.Name))
			{
				row.EffectiveValueSource =
					control->GetPropertyValueSource(row.Name);
				row.IsReadOnly = *row.EffectiveValueSource
					== ControlPropertyValueSource::Binding;
			}

			const auto* configured = FindNamed(target.DataBindings, row.Name);
			const auto* runtime = control->DataBindings.Find(row.Name);
			row.HasConfiguredBinding = configured != nullptr || runtime != nullptr;
			if (configured)
			{
				DesignerPropertyDiagnostic diagnostic;
				diagnostic.Kind = DesignerPropertyDiagnosticKind::Binding;
				diagnostic.Summary = DesignerBindingUtils::Describe(
					row.Name, *configured);
				if (const auto* preview = FindNamed(
					target.BindingPreviewStates, row.Name))
				{
					diagnostic.Details = preview->Message;
					diagnostic.Severity = preview->Status
						== DesignerBindingPreviewStatus::Error
						? BindingValidationSeverity::Error
						: BindingValidationSeverity::Info;
				}
				else
				{
					diagnostic.Details = L"绑定配置尚未建立设计期预览。";
				}
				row.Diagnostics.push_back(std::move(diagnostic));
			}
			else if (runtime)
			{
				row.Diagnostics.push_back({
					DesignerPropertyDiagnosticKind::Binding,
					BindingValidationSeverity::Info,
					L"运行时 Binding：" + runtime->TargetProperty()
						+ L" <- " + runtime->SourceProperty() + L"  ["
						+ DesignerBindingUtils::BindingModeName(runtime->Mode())
						+ L", " + DesignerBindingUtils::UpdateModeName(
							runtime->UpdateMode()) + L"]",
					L"该绑定来自运行时集合，而不是设计文档配置。"
				});
			}

			if (runtime)
			{
				if (!runtime->IsValid() || runtime->LastError() != BindingError::None)
				{
					row.Diagnostics.push_back({
						DesignerPropertyDiagnosticKind::Binding,
						BindingValidationSeverity::Error,
						L"Binding 更新失败",
						runtime->LastErrorMessage()
					});
				}
				for (const auto& issue : runtime->ValidationIssues())
				{
					row.Diagnostics.push_back({
						DesignerPropertyDiagnosticKind::Validation,
						issue.Severity,
						L"校验：" + issue.Message,
						issue.Code.empty() ? L"" : L"代码：" + issue.Code
					});
				}
			}

			if (style)
				AppendStyleDiagnostics(row, styleResolution,
					DesignerPropertyDiagnosticKind::Style,
					ControlPropertyValueSource::Style);
			if (theme)
				AppendStyleDiagnostics(row, themeResolution,
					DesignerPropertyDiagnosticKind::Theme,
					ControlPropertyValueSource::Theme);
		}
	}

	void SortRows(std::vector<DesignerPropertyRow>& rows)
	{
		std::sort(rows.begin(), rows.end(), [](const auto& left, const auto& right)
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
			const auto leftName = Lower(left.Name);
			const auto rightName = Lower(right.Name);
			if (leftName != rightName) return leftName < rightName;
			return static_cast<unsigned char>(left.Source)
				< static_cast<unsigned char>(right.Source);
		});
	}

	bool ContainsName(
		const std::vector<DesignerPropertyRow>& rows,
		const std::wstring& name)
	{
		return std::any_of(rows.begin(), rows.end(), [&](const auto& row)
		{
			return NamesEqual(row.Name, name);
		});
	}

	bool ChoicesEqual(
		const std::vector<DesignerPropertyRow::Choice>& left,
		const std::vector<DesignerPropertyRow::Choice>& right)
	{
		if (left.size() != right.size()) return false;
		for (size_t index = 0; index < left.size(); ++index)
		{
			if (left[index].DisplayName != right[index].DisplayName
				|| left[index].ValueText != right[index].ValueText)
				return false;
		}
		return true;
	}

	bool RowsAreBatchCompatible(
		const DesignerPropertyRow& left,
		const DesignerPropertyRow& right)
	{
		return left.Source == right.Source
			&& left.Value.Kind == right.Value.Kind
			&& left.Editor == right.Editor
			&& left.Minimum == right.Minimum
			&& left.Maximum == right.Maximum
			&& left.Step == right.Step
			&& ChoicesEqual(left.Choices, right.Choices);
	}

	DesignerPropertyRowEditorKind FormEditor(
		const DesignerFormPropertyDescriptor& property)
	{
		if (NamesEqual(property.Name, L"FontName"))
			return DesignerPropertyRowEditorKind::FontName;
		if (NamesEqual(property.Name, L"FontSize"))
			return DesignerPropertyRowEditorKind::FontSize;
		switch (property.Editor)
		{
		case ControlPropertyEditorKind::Boolean:
			return DesignerPropertyRowEditorKind::Boolean;
		case ControlPropertyEditorKind::Color:
			return DesignerPropertyRowEditorKind::Color;
		case ControlPropertyEditorKind::Thickness:
			return DesignerPropertyRowEditorKind::Thickness;
		case ControlPropertyEditorKind::Choice:
			return DesignerPropertyRowEditorKind::Choice;
		default:
			return DesignerPropertyRowEditorKind::Text;
		}
	}

	DesignerPropertyRowEditorKind ControlDesignEditor(
		DesignerControlPropertyEditorKind editor)
	{
		switch (editor)
		{
		case DesignerControlPropertyEditorKind::Boolean:
			return DesignerPropertyRowEditorKind::Boolean;
		case DesignerControlPropertyEditorKind::FontName:
			return DesignerPropertyRowEditorKind::FontName;
		case DesignerControlPropertyEditorKind::FontSize:
			return DesignerPropertyRowEditorKind::FontSize;
		case DesignerControlPropertyEditorKind::Anchor:
			return DesignerPropertyRowEditorKind::Anchor;
		case DesignerControlPropertyEditorKind::Text:
		default:
			return DesignerPropertyRowEditorKind::Text;
		}
	}

	DesignerPropertyRowEditorKind RuntimeEditor(
		const DesignerPropertyDescriptor& property)
	{
		if (property.Editor == ControlPropertyEditorKind::Choice
			&& !property.Choices.empty())
			return DesignerPropertyRowEditorKind::Choice;
		switch (property.Editor)
		{
		case ControlPropertyEditorKind::Boolean:
			return DesignerPropertyRowEditorKind::Boolean;
		case ControlPropertyEditorKind::Color:
			return DesignerPropertyRowEditorKind::Color;
		case ControlPropertyEditorKind::Thickness:
			return DesignerPropertyRowEditorKind::Thickness;
		case ControlPropertyEditorKind::Number:
			if (property.Minimum && property.Maximum
				&& *property.Minimum < *property.Maximum
				&& (property.ValueKind == DesignerStyleValueKind::Float
					|| property.ValueKind == DesignerStyleValueKind::Double))
				return DesignerPropertyRowEditorKind::FloatSlider;
			return DesignerPropertyRowEditorKind::Text;
		default:
			return DesignerPropertyRowEditorKind::Text;
		}
	}

	DesignerPropertyRowEditorKind CustomEditor(
		const DesignerCustomPropertyDescriptor& property)
	{
		if (!property.Choices.empty()
			|| property.Editor == ControlPropertyEditorKind::Choice)
			return DesignerPropertyRowEditorKind::Choice;
		switch (property.Editor)
		{
		case ControlPropertyEditorKind::Boolean:
			return DesignerPropertyRowEditorKind::Boolean;
		case ControlPropertyEditorKind::Color:
			return DesignerPropertyRowEditorKind::Color;
		case ControlPropertyEditorKind::Thickness:
			return DesignerPropertyRowEditorKind::Thickness;
		case ControlPropertyEditorKind::Number:
			if (property.Minimum && property.Maximum
				&& *property.Minimum < *property.Maximum
				&& (property.DefaultValue.Kind == DesignerStyleValueKind::Float
					|| property.DefaultValue.Kind == DesignerStyleValueKind::Double))
				return DesignerPropertyRowEditorKind::FloatSlider;
			return DesignerPropertyRowEditorKind::Text;
		case ControlPropertyEditorKind::Auto:
			switch (property.DefaultValue.Kind)
			{
			case DesignerStyleValueKind::Bool:
				return DesignerPropertyRowEditorKind::Boolean;
			case DesignerStyleValueKind::Color:
				return DesignerPropertyRowEditorKind::Color;
			case DesignerStyleValueKind::Thickness:
				return DesignerPropertyRowEditorKind::Thickness;
			default:
				return DesignerPropertyRowEditorKind::Text;
			}
		default:
			return DesignerPropertyRowEditorKind::Text;
		}
	}
}

std::vector<DesignerPropertyRow> GetFormRows(
	const DesignerModel::DesignFormModel& form)
{
	std::vector<DesignerPropertyRow> rows;
	for (const auto& property : DesignerFormPropertyCatalog::GetProperties())
	{
		DesignerStyleValue current;
		if (!DesignerFormPropertyCatalog::CaptureValue(
			form, property.Name, current)) continue;
		DesignerPropertyRow row;
		row.Source = DesignerPropertyRowSource::Form;
		row.Name = property.Name;
		row.DisplayName = property.DisplayName;
		row.Category = property.Category;
		row.CategoryOrder = property.CategoryOrder;
		row.Order = property.Order;
		row.Value = std::move(current);
		row.Editor = FormEditor(property);
		row.Minimum = property.Minimum;
		row.Maximum = property.Maximum;
		row.CanReset = true;
		rows.push_back(std::move(row));
	}
	SortRows(rows);
	return rows;
}

std::vector<DesignerPropertyRow> GetControlRows(
	DesignerControl& target,
	const DesignerControlPropertyContext& context)
{
	std::vector<DesignerPropertyRow> rows;
	for (const auto& property :
		DesignerControlPropertyCatalog::GetProperties(target))
	{
		DesignerStyleValue current;
		if (!DesignerControlPropertyCatalog::CaptureValue(
			target, context, property.Name, current)) continue;
		DesignerPropertyRow row;
		row.Source = DesignerPropertyRowSource::ControlDesign;
		row.Name = property.Name;
		row.DisplayName = property.DisplayName;
		row.Category = property.Category;
		row.CategoryOrder = property.CategoryOrder;
		row.Order = property.Order;
		row.Value = std::move(current);
		row.Editor = ControlDesignEditor(property.Editor);
		row.CanReset = property.CanReset;
		rows.push_back(std::move(row));
	}

	if (target.ControlInstance)
	{
		for (const auto& property :
			DesignerPropertyCatalog::GetPropertyGridProperties(
				*target.ControlInstance))
		{
			if (ContainsName(rows, property.Name)) continue;
			DesignerPropertyRow row;
			row.Source = DesignerPropertyRowSource::RuntimeMetadata;
			row.Name = property.Name;
			row.DisplayName = property.DisplayName;
			row.Category = property.Category;
			row.CategoryOrder = property.CategoryOrder;
			row.Order = property.Order;
			row.Value = { property.ValueKind, property.SampleValue };
			row.Editor = RuntimeEditor(property);
			row.Minimum = property.Minimum;
			row.Maximum = property.Maximum;
			row.Step = property.Step;
			row.EffectiveValueSource =
				target.ControlInstance->GetPropertyValueSource(property.Name);
			row.CanReset = property.Metadata && property.Metadata->CanWrite()
				&& property.Metadata->HasDefaultValue();
			for (const auto& choice : property.Choices)
				row.Choices.push_back({ choice.DisplayName, choice.ValueText });
			rows.push_back(std::move(row));
		}
	}

	for (const auto& property : target.CustomProperties)
	{
		if (ContainsName(rows, property.Name)) continue;
		DesignerPropertyRow row;
		row.Source = DesignerPropertyRowSource::CustomDescriptor;
		row.Name = property.Name;
		row.DisplayName = property.DisplayName;
		row.Category = property.Category;
		row.CategoryOrder = property.CategoryOrder;
		row.Order = property.Order;
		const auto stored = target.MetadataProperties.find(property.Name);
		row.Value = stored == target.MetadataProperties.end()
			? property.DefaultValue : stored->second;
		row.Editor = CustomEditor(property);
		row.Minimum = property.Minimum;
		row.Maximum = property.Maximum;
		row.Step = property.Step;
		row.EffectiveValueSource = stored == target.MetadataProperties.end()
			? ControlPropertyValueSource::Default
			: ControlPropertyValueSource::Local;
		row.CanReset = true;
		for (const auto& choice : property.Choices)
			row.Choices.push_back({ choice.DisplayName, choice.ValueText });
		rows.push_back(std::move(row));
	}
	AppendControlDiagnostics(target, rows);

	SortRows(rows);
	return rows;
}

std::vector<DesignerPropertyRow> GetCommonControlRows(
	const std::vector<std::vector<DesignerPropertyRow>>& controlRows)
{
	if (controlRows.empty()) return {};
	if (controlRows.size() == 1) return controlRows.front();

	std::vector<DesignerPropertyRow> result;
	result.reserve(controlRows.front().size());
	for (const auto& primaryRow : controlRows.front())
	{
		// Name is an object identity, not a meaningful batch-editable value.
		if (NamesEqual(primaryRow.Name, L"Name")) continue;
		DesignerPropertyRow common = primaryRow;
		common.IsReadOnly = primaryRow.IsReadOnly;
		bool presentEverywhere = true;
		for (size_t controlIndex = 1;
			controlIndex < controlRows.size(); ++controlIndex)
		{
			const auto* candidate = Find(
				controlRows[controlIndex], primaryRow.Name);
			if (!candidate || !RowsAreBatchCompatible(primaryRow, *candidate))
			{
				presentEverywhere = false;
				break;
			}
			if (candidate->Value != primaryRow.Value)
				common.HasMixedValue = true;
			if (candidate->EffectiveValueSource
				!= primaryRow.EffectiveValueSource)
				common.HasMixedValueSource = true;
			if (candidate->Diagnostics != primaryRow.Diagnostics)
				common.HasMixedDiagnostics = true;
			common.HasConfiguredBinding = common.HasConfiguredBinding
				|| candidate->HasConfiguredBinding;
			common.IsReadOnly = common.IsReadOnly || candidate->IsReadOnly;
			common.CanReset = common.CanReset && candidate->CanReset;
		}
		if (!presentEverywhere) continue;
		if (common.HasMixedValueSource)
			common.EffectiveValueSource.reset();
		if (common.HasMixedDiagnostics)
			common.Diagnostics.clear();
		result.push_back(std::move(common));
	}
	return result;
}

const DesignerPropertyRow* Find(
	const std::vector<DesignerPropertyRow>& rows,
	const std::wstring& propertyName)
{
	const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row)
	{
		return NamesEqual(row.Name, propertyName);
	});
	return found == rows.end() ? nullptr : &*found;
}

bool MatchesFilterText(
	const std::wstring& searchableText,
	const std::wstring& filterText)
{
	const auto haystack = Lower(searchableText);
	std::wistringstream stream(Lower(filterText));
	std::wstring token;
	while (stream >> token)
	{
		if (haystack.find(token) == std::wstring::npos) return false;
	}
	return true;
}

std::vector<DesignerPropertyRow> FilterRows(
	const std::vector<DesignerPropertyRow>& rows,
	const std::wstring& filterText)
{
	std::vector<DesignerPropertyRow> result;
	result.reserve(rows.size());
	for (const auto& row : rows)
	{
		std::wstring searchable = row.Name + L" " + row.DisplayName
			+ L" " + row.Category + L" " + row.Value.Text
			+ L" " + RowSourceName(row.Source)
			+ L" " + EditorAliases(row.Editor);
		if (row.EffectiveValueSource)
			searchable += L" " + std::wstring(ControlPropertyValueSourceName(
				*row.EffectiveValueSource)) + L" "
				+ ValueSourceAliases(*row.EffectiveValueSource);
		if (row.HasMixedValue)
			searchable += L" Multiple Mixed 多个值 混合值";
		if (row.HasMixedValueSource)
			searchable += L" Mixed Source 混合来源";
		if (row.IsReadOnly)
			searchable += L" Readonly Read-only 只读";
		if (row.HasConfiguredBinding)
			searchable += L" Configured Binding 已配置绑定";
		if (row.HasMixedDiagnostics)
			searchable += L" Mixed Diagnostics 诊断不一致";
		for (const auto& diagnostic : row.Diagnostics)
		{
			searchable += L" " + diagnostic.Summary + L" "
				+ diagnostic.Details + L" "
				+ DiagnosticKindAliases(diagnostic.Kind) + L" "
				+ BindingValidationSeverityName(diagnostic.Severity);
		}
		for (const auto& choice : row.Choices)
			searchable += L" " + choice.DisplayName + L" " + choice.ValueText;
		if (MatchesFilterText(searchable, filterText)) result.push_back(row);
	}
	return result;
}
}
