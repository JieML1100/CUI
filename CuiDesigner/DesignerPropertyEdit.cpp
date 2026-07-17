#include "DesignerPropertyEdit.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include <algorithm>
#include <cwctype>

namespace
{
	bool NamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	std::wstring Trim(std::wstring value)
	{
		const auto first = std::find_if_not(value.begin(), value.end(), iswspace);
		const auto last = std::find_if_not(value.rbegin(), value.rend(), iswspace).base();
		if (first >= last) return {};
		return std::wstring(first, last);
	}

	std::wstring TargetPrefix(const DesignerPropertyEditTarget& target)
	{
		if (!target.Control || target.Control->Name.empty()) return L"控件：";
		return L"控件 " + target.Control->Name + L"：";
	}

	std::wstring NormalizeDesignValue(
		const DesignerControlPropertyDescriptor& property,
		std::wstring valueText)
	{
		if (property.Editor == DesignerControlPropertyEditorKind::FontName)
		{
			valueText = Trim(std::move(valueText));
			if (NamesEqual(valueText, L"<Default>")) valueText.clear();
		}
		return valueText;
	}

	const DesignerCustomPropertyDescriptor* FindCustomProperty(
		const DesignerControl& control,
		const std::wstring& name)
	{
		const auto found = std::find_if(
			control.CustomProperties.begin(), control.CustomProperties.end(),
			[&](const auto& property)
			{
				return NamesEqual(property.Name, name);
			});
		return found == control.CustomProperties.end() ? nullptr : &*found;
	}

	void PublishCustomPreviewValue(
		DesignerControl& control,
		const std::wstring& propertyName,
		const DesignerStyleValue& value)
	{
		if (control.PreviewPropertyChanged)
			control.PreviewPropertyChanged(propertyName, value);
	}

	bool ValidateCustomValue(
		const DesignerCustomPropertyDescriptor& property,
		const std::wstring& valueText,
		DesignerStyleValue& value,
		std::wstring* outError)
	{
		value = { property.DefaultValue.Kind, valueText };
		BindingValue converted;
		if (!DesignerStyleSheetUtils::TryConvertValue(value, converted, outError))
			return false;
		if (property.Minimum || property.Maximum)
		{
			double numeric = 0.0;
			if (!converted.TryGetDouble(numeric))
			{
				if (outError) *outError = L"属性声明了数值范围，但值不是数值。";
				return false;
			}
			if (property.Minimum && numeric < *property.Minimum)
			{
				if (outError) *outError = L"值小于允许的最小值。";
				return false;
			}
			if (property.Maximum && numeric > *property.Maximum)
			{
				if (outError) *outError = L"值大于允许的最大值。";
				return false;
			}
		}
		if (!property.Choices.empty())
		{
			const auto found = std::find_if(
				property.Choices.begin(), property.Choices.end(),
				[&](const auto& choice)
				{
					return choice.ValueText == valueText;
				});
			if (found == property.Choices.end())
			{
				if (outError) *outError = L"值不在自定义属性的可选集合中。";
				return false;
			}
		}
		if (outError) outError->clear();
		return true;
	}

	bool RestoreSnapshots(
		const std::vector<DesignerPropertyEditTarget>& targets,
		const DesignerPropertyRow& row,
		const std::vector<DesignerPropertyValueSnapshot>& snapshots,
		size_t count)
	{
		count = (std::min)(count, snapshots.size());
		bool restored = true;
		for (size_t index = count; index > 0; --index)
		{
			try
			{
				restored = DesignerPropertyEdit::RestoreSnapshot(
					targets[index - 1], row, snapshots[index - 1]) && restored;
			}
			catch (...)
			{
				restored = false;
			}
		}
		return restored;
	}
}

bool DesignerPropertyValueSnapshot::EquivalentTo(
	const DesignerPropertyValueSnapshot& other) const noexcept
{
	if (UsesLegacyPersistence != other.UsesLegacyPersistence
		|| CanonicalPropertyName != other.CanonicalPropertyName)
		return false;
	if (UsesLegacyPersistence)
		return EffectiveSerializedValue == other.EffectiveSerializedValue;
	return DesignValue == other.DesignValue
		&& HasLocalValue == other.HasLocalValue
		&& (!HasLocalValue
			|| LocalSerializedValue == other.LocalSerializedValue)
		&& HasTrackedValue == other.HasTrackedValue
		&& (!HasTrackedValue || TrackedValue == other.TrackedValue);
}

size_t DesignerPropertyValueSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this)
		+ DesignValue.Text.capacity() * sizeof(wchar_t)
		+ LocalSerializedValue.Text.capacity() * sizeof(wchar_t)
		+ EffectiveSerializedValue.Text.capacity() * sizeof(wchar_t)
		+ TrackedValue.Text.capacity() * sizeof(wchar_t)
		+ CanonicalPropertyName.capacity() * sizeof(wchar_t);
}

bool DesignerPropertyTargetSnapshot::EquivalentTo(
	const DesignerPropertyTargetSnapshot& other) const noexcept
{
	return TargetName == other.TargetName
		&& TargetType == other.TargetType
		&& Value.EquivalentTo(other.Value);
}

size_t DesignerPropertyTargetSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this)
		+ TargetName.capacity() * sizeof(wchar_t)
		+ Value.GetEstimatedMemoryUsage();
}

bool DesignerPropertyBatchSnapshot::EquivalentTo(
	const DesignerPropertyBatchSnapshot& other) const noexcept
{
	if (Source != other.Source || PropertyName != other.PropertyName
		|| Targets.size() != other.Targets.size())
		return false;
	for (size_t index = 0; index < Targets.size(); ++index)
		if (!Targets[index].EquivalentTo(other.Targets[index])) return false;
	return true;
}

size_t DesignerPropertyBatchSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	size_t result = sizeof(*this)
		+ PropertyName.capacity() * sizeof(wchar_t)
		+ Targets.capacity() * sizeof(DesignerPropertyTargetSnapshot);
	for (const auto& target : Targets)
		result += target.GetEstimatedMemoryUsage();
	return result;
}

DesignerPropertyEditResult DesignerPropertyEditResult::Success(
	size_t appliedCount)
{
	return { true, appliedCount, {} };
}

DesignerPropertyEditResult DesignerPropertyEditResult::Failure(
	std::wstring error,
	size_t appliedCount)
{
	if (error.empty()) error = L"属性修改被拒绝。";
	return { false, appliedCount, std::move(error) };
}

namespace DesignerPropertyEdit
{
bool CaptureSnapshot(
	const DesignerPropertyEditTarget& editTarget,
	const DesignerPropertyRow& row,
	DesignerPropertyValueSnapshot& snapshot,
	std::wstring* outError)
{
	snapshot = DesignerPropertyValueSnapshot{};
	if (!editTarget.Control || !editTarget.Control->ControlInstance)
	{
		if (outError) *outError = L"目标已经失效。";
		return false;
	}
	auto& designerControl = *editTarget.Control;
	auto& runtimeControl = *designerControl.ControlInstance;
	snapshot.CanonicalPropertyName = row.Name;
	if (row.Source == DesignerPropertyRowSource::ControlDesign)
		return DesignerControlPropertyCatalog::CaptureValue(
			designerControl,
			editTarget.Context,
			row.Name,
			snapshot.DesignValue,
			outError);
	if (row.Source == DesignerPropertyRowSource::CustomDescriptor)
	{
		const auto* property = FindCustomProperty(designerControl, row.Name);
		if (!property)
		{
			if (outError) *outError = L"自定义属性 schema 已失效。";
			return false;
		}
		const auto tracked = designerControl.MetadataProperties.find(property->Name);
		snapshot.DesignValue = tracked == designerControl.MetadataProperties.end()
			? property->DefaultValue : tracked->second;
		snapshot.HasTrackedValue =
			tracked != designerControl.MetadataProperties.end();
		if (snapshot.HasTrackedValue) snapshot.TrackedValue = tracked->second;
		if (outError) outError->clear();
		return true;
	}
	if (row.Source != DesignerPropertyRowSource::RuntimeMetadata)
	{
		if (outError) *outError = L"当前属性来源不支持控件快照。";
		return false;
	}

	std::wstring canonicalName;
	if (!DesignerPropertyCatalog::CaptureValue(
		runtimeControl,
		row.Name,
		&canonicalName,
		snapshot.EffectiveSerializedValue,
		outError))
		return false;
	if (!canonicalName.empty())
		snapshot.CanonicalPropertyName = std::move(canonicalName);
	if (const auto* metadata = runtimeControl.FindPropertyMetadata(
		snapshot.CanonicalPropertyName))
		snapshot.UsesLegacyPersistence = metadata->Design().Persistence
			== ControlPropertyPersistence::Legacy;

	snapshot.HasLocalValue = runtimeControl.TryGetPropertyValue(
		row.Name,
		ControlPropertyValueSource::Local,
		snapshot.LocalValue);
	if (snapshot.HasLocalValue)
		snapshot.LocalSerializedValue = snapshot.EffectiveSerializedValue;
	const auto tracked = designerControl.MetadataProperties.find(
		snapshot.CanonicalPropertyName);
	snapshot.HasTrackedValue =
		tracked != designerControl.MetadataProperties.end();
	if (snapshot.HasTrackedValue) snapshot.TrackedValue = tracked->second;
	if (outError) outError->clear();
	return true;
}

bool RestoreSnapshot(
	const DesignerPropertyEditTarget& editTarget,
	const DesignerPropertyRow& row,
	const DesignerPropertyValueSnapshot& snapshot,
	std::wstring* outError)
{
	if (!editTarget.Control || !editTarget.Control->ControlInstance)
	{
		if (outError) *outError = L"目标已经失效。";
		return false;
	}
	auto& designerControl = *editTarget.Control;
	auto& runtimeControl = *designerControl.ControlInstance;
	bool restored = false;
	if (row.Source == DesignerPropertyRowSource::ControlDesign)
	{
		auto context = editTarget.Context;
			restored = DesignerControlPropertyCatalog::ApplyValue(
			designerControl,
			context,
			row.Name,
			snapshot.DesignValue,
			nullptr,
				outError);
	}
	else if (row.Source == DesignerPropertyRowSource::CustomDescriptor)
	{
		const auto* property = FindCustomProperty(designerControl, row.Name);
		if (!property)
		{
			if (outError) *outError = L"自定义属性 schema 已失效。";
			return false;
		}
		if (snapshot.HasTrackedValue)
			designerControl.MetadataProperties[property->Name]
				= snapshot.TrackedValue;
		else
			designerControl.MetadataProperties.erase(property->Name);
		PublishCustomPreviewValue(
			designerControl, property->Name,
			snapshot.HasTrackedValue
				? snapshot.TrackedValue : property->DefaultValue);
		restored = true;
	}
	else if (row.Source == DesignerPropertyRowSource::RuntimeMetadata)
	{
		const auto& canonicalName = snapshot.CanonicalPropertyName.empty()
			? row.Name : snapshot.CanonicalPropertyName;
		if (snapshot.UsesLegacyPersistence)
		{
			BindingValue converted;
			restored = DesignerStyleSheetUtils::TryConvertValue(
				snapshot.EffectiveSerializedValue, converted, outError)
				&& runtimeControl.TrySetPropertyBaseValue(
					canonicalName, converted);
			if (restored && runtimeControl.HasPropertyValue(
				canonicalName, ControlPropertyValueSource::Local))
				restored = runtimeControl.ClearPropertyValue(
					canonicalName, ControlPropertyValueSource::Local);
			if (restored)
				designerControl.MetadataProperties.erase(canonicalName);
		}
		else if (snapshot.HasLocalValue)
			restored = runtimeControl.TrySetPropertyValue(
				row.Name,
				snapshot.LocalValue,
				ControlPropertyValueSource::Local);
		else
		{
			restored = runtimeControl.ClearPropertyValue(
				row.Name,
				ControlPropertyValueSource::Local);
			if (!restored)
				restored = !runtimeControl.HasPropertyValue(
					row.Name, ControlPropertyValueSource::Local);
		}
		if (restored && !snapshot.UsesLegacyPersistence)
		{
			if (snapshot.HasTrackedValue)
				designerControl.MetadataProperties[canonicalName]
					= snapshot.TrackedValue;
			else
				designerControl.MetadataProperties.erase(canonicalName);
		}
		else if (outError && outError->empty())
			*outError = L"运行时属性值无法恢复。";
	}
	else if (outError)
		*outError = L"当前属性来源不支持控件快照恢复。";
	if (!restored) return false;

	DesignerPropertyValueSnapshot actual;
	std::wstring captureError;
	if (!CaptureSnapshot(editTarget, row, actual, &captureError))
	{
		if (outError) *outError = std::move(captureError);
		return false;
	}
	if (!actual.EquivalentTo(snapshot))
	{
		if (outError) *outError = L"属性 setter 未恢复到请求的精确状态。";
		return false;
	}
	if (outError) outError->clear();
	return true;
}

DesignerPropertyEditResult Validate(
	const std::vector<DesignerPropertyEditTarget>& targets,
	const DesignerPropertyRow& row,
	const std::wstring& valueText)
{
	if (targets.empty())
		return DesignerPropertyEditResult::Failure(L"没有可编辑的目标控件。");
	if (row.IsReadOnly)
		return DesignerPropertyEditResult::Failure(
			L"至少一个目标的属性由 Binding 占用，不能批量设置 Local 值。");

	for (const auto& editTarget : targets)
	{
		if (!editTarget.Control || !editTarget.Control->ControlInstance)
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(editTarget) + L"目标已经失效。");
		auto& designerControl = *editTarget.Control;
		auto& runtimeControl = *designerControl.ControlInstance;
		std::wstring error;
		if (row.Source == DesignerPropertyRowSource::ControlDesign)
		{
			const auto* property = DesignerControlPropertyCatalog::Find(
				designerControl, row.Name);
			if (!property)
				return DesignerPropertyEditResult::Failure(
					TargetPrefix(editTarget) + L"没有设计器属性 " + row.Name + L"。");
			if (property->ValueKind != row.Value.Kind)
				return DesignerPropertyEditResult::Failure(
					TargetPrefix(editTarget) + L"属性类型与公共属性模型不一致。");
			BindingValue converted;
			const DesignerStyleValue value{
				property->ValueKind,
				NormalizeDesignValue(*property, valueText)
			};
			if (!DesignerStyleSheetUtils::TryConvertValue(value, converted, &error))
				return DesignerPropertyEditResult::Failure(
					TargetPrefix(editTarget) + error);
			continue;
		}
		if (row.Source == DesignerPropertyRowSource::CustomDescriptor)
		{
			const auto* property = FindCustomProperty(designerControl, row.Name);
			if (!property)
				return DesignerPropertyEditResult::Failure(
					TargetPrefix(editTarget) + L"没有自定义属性 "
					+ row.Name + L"。");
			if (property->DefaultValue.Kind != row.Value.Kind)
				return DesignerPropertyEditResult::Failure(
					TargetPrefix(editTarget) + L"属性类型与自定义 schema 不一致。");
			DesignerStyleValue value;
			if (!ValidateCustomValue(*property, valueText, value, &error))
				return DesignerPropertyEditResult::Failure(
					TargetPrefix(editTarget) + error);
			continue;
		}
		if (row.Source != DesignerPropertyRowSource::RuntimeMetadata)
			return DesignerPropertyEditResult::Failure(
				L"当前属性来源不支持控件批量编辑。");
		if (runtimeControl.GetPropertyValueSource(row.Name)
			== ControlPropertyValueSource::Binding)
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(editTarget) + L"属性 " + row.Name
				+ L" 由 Binding 占用，不能设置 Local 值。");
		if (!DesignerPropertyCatalog::ValidateStyleValue(
			runtimeControl,
			row.Name,
			{ row.Value.Kind, valueText },
			&error))
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(editTarget) + error);
	}
	return DesignerPropertyEditResult::Success();
}

DesignerPropertyEditResult Apply(
	const std::vector<DesignerPropertyEditTarget>& targets,
	const DesignerPropertyRow& row,
	const std::wstring& valueText)
{
	auto validation = Validate(targets, row, valueText);
	if (!validation) return validation;
	std::vector<DesignerPropertyValueSnapshot> snapshots(targets.size());
	for (size_t index = 0; index < targets.size(); ++index)
	{
		std::wstring error;
		if (!CaptureSnapshot(targets[index], row, snapshots[index], &error))
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(targets[index]) + error);
	}
	size_t applied = 0;
	for (size_t index = 0; index < targets.size(); ++index)
	{
		const auto& editTarget = targets[index];
		auto& designerControl = *editTarget.Control;
		auto& runtimeControl = *designerControl.ControlInstance;
		std::wstring error;
		bool succeeded = false;
		try
		{
			if (row.Source == DesignerPropertyRowSource::ControlDesign)
			{
				const auto* property = DesignerControlPropertyCatalog::Find(
					designerControl, row.Name);
				if (property)
				{
					auto context = editTarget.Context;
					succeeded = DesignerControlPropertyCatalog::ApplyValue(
						designerControl,
						context,
						property->Name,
						{ property->ValueKind,
							NormalizeDesignValue(*property, valueText) },
						nullptr,
						&error);
				}
			}
			else if (row.Source == DesignerPropertyRowSource::CustomDescriptor)
			{
				const auto* property = FindCustomProperty(
					designerControl, row.Name);
				DesignerStyleValue value;
				if (property && ValidateCustomValue(
					*property, valueText, value, &error))
				{
					designerControl.MetadataProperties[property->Name]
						= std::move(value);
					PublishCustomPreviewValue(
						designerControl, property->Name,
						designerControl.MetadataProperties[property->Name]);
					succeeded = true;
				}
			}
			else
			{
				succeeded = DesignerPropertyCatalog::ApplyAndTrackValue(
					runtimeControl,
					designerControl.MetadataProperties,
					row.Name,
					{ row.Value.Kind, valueText },
					nullptr,
					nullptr,
					&error);
			}
		}
		catch (...)
		{
			error = L"属性 setter 抛出异常。";
		}
		if (!succeeded)
		{
			const bool restored = RestoreSnapshots(
				targets, row, snapshots, index + 1);
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(editTarget) + error
				+ (restored ? L"" : L" 回滚未能完整恢复所有目标。"));
		}
		++applied;
	}
	return DesignerPropertyEditResult::Success(applied);
}

DesignerPropertyEditResult Reset(
	const std::vector<DesignerPropertyEditTarget>& targets,
	const DesignerPropertyRow& row)
{
	if (targets.empty())
		return DesignerPropertyEditResult::Failure(L"没有可恢复的目标控件。");
	if (row.IsReadOnly)
		return DesignerPropertyEditResult::Failure(
			L"至少一个目标的属性由 Binding 占用，不能恢复 Local 值。");
	if (!row.CanReset)
		return DesignerPropertyEditResult::Failure(
			L"属性 " + row.Name + L" 没有可用的默认值。");
	std::vector<DesignerPropertyValueSnapshot> snapshots(targets.size());
	for (size_t index = 0; index < targets.size(); ++index)
	{
		std::wstring error;
		if (!targets[index].Control || !targets[index].Control->ControlInstance)
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(targets[index]) + L"目标已经失效。");
		if (!CaptureSnapshot(targets[index], row, snapshots[index], &error))
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(targets[index]) + error);
	}
	size_t applied = 0;
	for (size_t index = 0; index < targets.size(); ++index)
	{
		const auto& editTarget = targets[index];
		if (!editTarget.Control || !editTarget.Control->ControlInstance)
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(editTarget) + L"目标已经失效。", applied);
		auto& designerControl = *editTarget.Control;
		auto& runtimeControl = *designerControl.ControlInstance;
		std::wstring error;
		bool succeeded = false;
		try
		{
			if (row.Source == DesignerPropertyRowSource::ControlDesign)
			{
				auto context = editTarget.Context;
				succeeded = DesignerControlPropertyCatalog::ResetValue(
					designerControl, context, row.Name, nullptr, &error);
			}
			else if (row.Source == DesignerPropertyRowSource::CustomDescriptor)
			{
				const auto* property = FindCustomProperty(
					designerControl, row.Name);
				if (property)
				{
					designerControl.MetadataProperties.erase(property->Name);
					PublishCustomPreviewValue(
						designerControl, property->Name,
						property->DefaultValue);
					succeeded = true;
				}
				else error = L"自定义属性 schema 已失效。";
			}
			else if (row.Source == DesignerPropertyRowSource::RuntimeMetadata)
			{
				succeeded = DesignerPropertyCatalog::ResetAndUntrackValue(
					runtimeControl,
					designerControl.MetadataProperties,
					row.Name,
					nullptr,
					nullptr,
					&error);
			}
		}
		catch (...)
		{
			error = L"属性 Reset setter 抛出异常。";
		}
		if (!succeeded)
		{
			const bool restored = RestoreSnapshots(
				targets, row, snapshots, index + 1);
			return DesignerPropertyEditResult::Failure(
				TargetPrefix(editTarget) + error
				+ (restored ? L"" : L" 回滚未能完整恢复所有目标。"));
		}
		++applied;
	}
	return DesignerPropertyEditResult::Success(applied);
}
}
