#include "DesignerPropertyRowCatalog.h"
#include "DesignerBindingUtils.h"
#include "DesignerDataContextSchemaUtils.h"
#include "../CUI/include/Style.h"
#include "../CUI/include/StyleInfrastructure.h"
#include "../CUI/include/GroupStyle.h"
#include <algorithm>
#include <cwctype>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace DesignerPropertyRowCatalog
{
namespace
{
	bool NamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
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
		case DesignerPropertyRowSource::Window: return L"Window 窗体";
		case DesignerPropertyRowSource::ControlDesign: return L"Designer 设计器";
		case DesignerPropertyRowSource::RuntimeMetadata: return L"Runtime 运行时";
		default: return L"";
		}
	}

	const wchar_t* ValueSourceAliases(DependencyPropertyValueSource source)
	{
	switch (source)
	{
	case DependencyPropertyValueSource::Inherited: return L"Inherited 继承";
	case DependencyPropertyValueSource::Theme: return L"Theme 主题";
	case DependencyPropertyValueSource::Style: return L"Style 样式";
	case DependencyPropertyValueSource::Template: return L"Template 模板";
	case DependencyPropertyValueSource::VisualState: return L"VisualState 视觉状态";
	case DependencyPropertyValueSource::Local: return L"Local 本地";
	case DependencyPropertyValueSource::Animation: return L"Animation 动画";
	case DependencyPropertyValueSource::Default:
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
		case DesignerPropertyRowEditorKind::FontFamily: return L"Font Name 字体";
		case DesignerPropertyRowEditorKind::FontSize: return L"Font Size 字号";
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
		DependencyPropertyValueSource source)
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
				+ std::to_wstring(setter.RuleId)
				+ (setter.IsConditional ? L"（Trigger）" : L"（Setter）");
			if (row.EffectiveValueSource
				&& *row.EffectiveValueSource != source)
				diagnostic.Details = L"候选值被更高优先级的 "
					+ std::wstring(DependencyPropertyValueSourceName(
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
		const auto style =
			cui::framework::StyleAccess::DocumentStyles(*control);
		const auto theme = cui::framework::StyleAccess::Theme(*control);
		const auto styleResolution = style
			? style->Resolve(*control) : ControlStyleResolution{};
		const auto themeResolution = theme
			? theme->Resolve(*control, true) : ControlStyleResolution{};

		for (auto& row : rows)
		{
			if (control->FindPropertyMetadata(row.Name))
			{
				row.EffectiveValueSource =
					control->GetPropertyValueSource(row.Name);
				const auto* metadata = control->FindPropertyMetadata(row.Name);
				row.IsReadOnly = row.IsReadOnly
					|| (metadata && !metadata->CanWrite())
					|| control->GetPropertyExpressionKind(row.Name)
						== DependencyPropertyExpressionKind::Binding;
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
						+ L", " + DesignerBindingUtils::UpdateSourceTriggerName(
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
					DependencyPropertyValueSource::Style);
			if (theme)
				AppendStyleDiagnostics(row, themeResolution,
					DesignerPropertyDiagnosticKind::Theme,
					DependencyPropertyValueSource::Theme);
		}
	}

	void SortRows(std::vector<DesignerPropertyRow>& rows)
	{
		// Metadata may be owned by different WPF layers while sharing one display
		// category.  A category is one property-grid group, so normalize its order
		// before sorting instead of splitting it whenever owners chose different
		// CategoryOrder values.
		std::unordered_map<std::wstring, int> categoryOrders;
		for (const auto& row : rows)
		{
			const auto category = Lower(row.Category);
			const auto [entry, inserted] = categoryOrders.emplace(
				category, row.CategoryOrder);
			if (!inserted) entry->second = (std::min)(entry->second, row.CategoryOrder);
		}
		std::sort(rows.begin(), rows.end(), [&](const auto& left, const auto& right)
		{
			const auto leftCategory = Lower(left.Category);
			const auto rightCategory = Lower(right.Category);
			const auto leftCategoryOrder = categoryOrders.at(leftCategory);
			const auto rightCategoryOrder = categoryOrders.at(rightCategory);
			if (leftCategoryOrder != rightCategoryOrder)
				return leftCategoryOrder < rightCategoryOrder;
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

	DesignerPropertyRowEditorKind ControlDesignEditor(
		DesignerDependencyPropertyEditorKind editor)
	{
		switch (editor)
		{
		case DesignerDependencyPropertyEditorKind::Boolean:
			return DesignerPropertyRowEditorKind::Boolean;
		case DesignerDependencyPropertyEditorKind::FontFamily:
			return DesignerPropertyRowEditorKind::FontFamily;
		case DesignerDependencyPropertyEditorKind::FontSize:
			return DesignerPropertyRowEditorKind::FontSize;
		case DesignerDependencyPropertyEditorKind::Choice:
			return DesignerPropertyRowEditorKind::Choice;
		case DesignerDependencyPropertyEditorKind::Text:
		default:
			return DesignerPropertyRowEditorKind::Text;
		}
	}

	DesignerPropertyRowEditorKind RuntimeEditor(
		const DesignerPropertyDescriptor& property)
	{
		if (NamesEqual(property.Name, L"FontFamily"))
			return DesignerPropertyRowEditorKind::FontFamily;
		if (NamesEqual(property.Name, L"FontSize"))
			return DesignerPropertyRowEditorKind::FontSize;
		if (property.Editor == DependencyPropertyEditorKind::Choice
			&& !property.Choices.empty())
			return DesignerPropertyRowEditorKind::Choice;
		switch (property.Editor)
		{
		case DependencyPropertyEditorKind::Boolean:
			return DesignerPropertyRowEditorKind::Boolean;
		case DependencyPropertyEditorKind::Color:
			return DesignerPropertyRowEditorKind::Color;
		case DependencyPropertyEditorKind::Thickness:
			return DesignerPropertyRowEditorKind::Thickness;
		case DependencyPropertyEditorKind::Number:
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

}

std::vector<DesignerPropertyRow> GetWindowRows(
	const DesignerModel::DesignNode& window)
{
	std::vector<DesignerPropertyRow> rows;
	for (const auto& property :
		DesignerPropertyCatalog::GetNodeProperties(window.Type))
	{
		DesignerStyleValue current;
		if (!DesignerPropertyCatalog::CaptureNodeValue(
			window, property.Name, current)) continue;
		DesignerPropertyRow row;
		row.Source = DesignerPropertyRowSource::Window;
		row.Name = property.Name;
		row.DisplayName = property.DisplayName;
		row.Category = property.Category;
		row.CategoryOrder = property.CategoryOrder;
		row.Order = property.Order;
		row.Value = std::move(current);
		row.Editor = RuntimeEditor(property);
		row.Minimum = property.Minimum;
		row.Maximum = property.Maximum;
		row.Step = property.Step;
		for (const auto& choice : property.Choices)
			row.Choices.push_back({ choice.DisplayName, choice.ValueText });
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
	const auto* dataTemplates = context.ScopedDataTemplates.empty()
		? context.DataTemplates : &context.ScopedDataTemplates;
	const auto* controlTemplates = context.ScopedControlTemplates.empty()
		? context.ControlTemplates : &context.ScopedControlTemplates;
	const auto* itemsPanelTemplates = context.ScopedItemsPanelTemplates.empty()
		? context.ItemsPanelTemplates : &context.ScopedItemsPanelTemplates;
	const auto* groupStyles = context.ScopedGroupStyles.empty()
		? context.GroupStyles : &context.ScopedGroupStyles;
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
		if (property.Editor == DesignerDependencyPropertyEditorKind::Choice)
		{
			auto resourceItemType = [&](const std::wstring& key)
			{
				std::unordered_set<std::wstring> visited;
				std::function<std::wstring(const std::wstring&)> resolve;
				resolve = [&](const std::wstring& current) -> std::wstring
				{
					if (!visited.insert(current).second) return {};
					if (context.DataLists)
					{
						const auto list = std::find_if(
							context.DataLists->begin(), context.DataLists->end(),
							[&](const auto& item) { return NamesEqual(item.Key, current); });
						if (list != context.DataLists->end()) return list->ItemType;
					}
					if (!context.CollectionViews) return {};
					const auto view = std::find_if(
						context.CollectionViews->begin(), context.CollectionViews->end(),
						[&](const auto& item) { return NamesEqual(item.Key, current); });
					if (view == context.CollectionViews->end()) return {};
					if (!view->SourceResource.empty())
						return resolve(view->SourceResource);
					const auto* property = context.DataContextSchema
						? DesignerDataContextSchemaUtils::Find(
							*context.DataContextSchema, view->SourceBindingPath) : nullptr;
					return property ? property->ItemType : std::wstring{};
				};
				return resolve(key);
			};
			row.Choices.push_back({
				NamesEqual(property.Name, L"Template")
					|| NamesEqual(property.Name, L"ItemTemplate")
					|| NamesEqual(property.Name, L"ContentTemplate")
					|| NamesEqual(property.Name, L"HeaderTemplate")
					? L"(自动)" : L"(无)",
				L"" });
			if (NamesEqual(property.Name, L"Template")
				&& controlTemplates)
			{
				auto compatible = [&](const DesignerModel::DesignControlTemplate& item)
				{
					if (item.IsImplicit()) return false;
					if (!item.TargetComponentType.Empty())
						return !target.ComponentType.Empty()
							&& target.ComponentType.RegistryKey()
								== item.TargetComponentType.RegistryKey();
					if (!target.ComponentType.Empty()) return false;
					return IsUIClassAssignableFrom(
						item.TargetType, target.Type);
				};
				for (const auto& item : *controlTemplates)
					if (compatible(item))
						row.Choices.push_back({ item.Key, item.Key });
			}
			else if (NamesEqual(property.Name, L"ItemsSourceResource"))
			{
				std::wstring requiredType;
				const auto currentTemplate = target.DesignStrings.find(L"itemTemplate");
				if (currentTemplate != target.DesignStrings.end()
					&& dataTemplates)
				{
					const auto found = std::find_if(
						dataTemplates->begin(), dataTemplates->end(),
						[&](const auto& item)
						{
							return NamesEqual(item.Key, currentTemplate->second);
						});
					if (found != dataTemplates->end())
						requiredType = found->DataType;
				}
				if (context.DataLists)
					for (const auto& item : *context.DataLists)
						if (requiredType.empty()
							|| NamesEqual(item.ItemType, requiredType))
							row.Choices.push_back({ item.Key, item.Key });
				if (context.CollectionViews)
					for (const auto& item : *context.CollectionViews)
					{
						const auto itemType = resourceItemType(item.Key);
						if (requiredType.empty() || NamesEqual(itemType, requiredType))
							row.Choices.push_back({ item.Key, item.Key });
					}
			}
			else if (NamesEqual(property.Name, L"ItemTemplate")
				&& dataTemplates)
			{
				std::wstring requiredType;
				const auto currentList = target.DesignStrings.find(
					L"itemsSourceResource");
				if (currentList != target.DesignStrings.end())
				{
					requiredType = resourceItemType(currentList->second);
				}
				for (const auto& item : *dataTemplates)
					if (!item.IsImplicit() && (requiredType.empty()
						|| NamesEqual(item.DataType, requiredType)))
						row.Choices.push_back({ item.Key, item.Key });
			}
			else if ((NamesEqual(property.Name, L"ContentTemplate")
				|| NamesEqual(property.Name, L"HeaderTemplate"))
				&& dataTemplates)
			{
				std::wstring requiredType;
				bool supportsDataTemplate = true;
				const auto binding = target.DataBindings.find(
					NamesEqual(property.Name, L"HeaderTemplate")
						? L"Header" : L"Content");
				if (binding != target.DataBindings.end()
					&& binding->second.ElementName.empty()
					&& binding->second.RelativeSource
						== DesignerBindingRelativeSource::None
					&& !binding->second.IsMultiBinding()
					&& context.DataContextSchema)
					if (const auto* source = DesignerDataContextSchemaUtils::Find(
						*context.DataContextSchema,
						binding->second.SourceProperty))
					{
						supportsDataTemplate = source->ObjectKind
							== DesignerDataObjectKind::BindingSource
							&& !source->DataType.empty();
						if (supportsDataTemplate) requiredType = source->DataType;
					}
				if (supportsDataTemplate)
					for (const auto& item : *dataTemplates)
						if (!item.IsImplicit() && (requiredType.empty()
							|| NamesEqual(item.DataType, requiredType)))
							row.Choices.push_back({ item.Key, item.Key });
			}
			else if (NamesEqual(property.Name, L"ItemContainerStyle")
				&& context.StyleSheet)
			{
				const auto containerType =
					GetDefaultItemContainerType(target.Type);
				for (const auto& rule : context.StyleSheet->Rules)
					if (!rule.Id.empty() && rule.ComponentType.Empty()
						&& (!rule.HasType
							|| rule.Type == UIClass::UI_Base
							|| rule.Type == containerType))
						row.Choices.push_back({ rule.Id, rule.Id });
			}
			else if (NamesEqual(property.Name, L"GroupStyle")
				&& groupStyles)
			{
				for (const auto& style : *groupStyles)
				{
					std::wstring headerType;
					if (dataTemplates && !style.HeaderTemplate.empty())
					{
						const auto header = std::find_if(
							dataTemplates->begin(),
							dataTemplates->end(), [&](const auto& item)
							{ return NamesEqual(item.Key, style.HeaderTemplate); });
						if (header != dataTemplates->end())
							headerType = header->DataType;
					}
					if (headerType.empty() || NamesEqual(headerType,
						std::wstring(CollectionViewGroupDataTypeName)))
						row.Choices.push_back({ style.Key, style.Key });
				}
			}
			else if (NamesEqual(property.Name, L"ItemsPanel")
				&& itemsPanelTemplates)
			{
				for (const auto& item : *itemsPanelTemplates)
					row.Choices.push_back({ item.Key, item.Key });
			}
		}
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
			row.IsReadOnly = property.Metadata && !property.Metadata->CanWrite();
			row.CanReset = property.Metadata && property.Metadata->CanWrite()
				&& property.Metadata->HasDefaultValue();
			for (const auto& choice : property.Choices)
				row.Choices.push_back({ choice.DisplayName, choice.ValueText });
			rows.push_back(std::move(row));
		}
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
			searchable += L" " + std::wstring(DependencyPropertyValueSourceName(
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
