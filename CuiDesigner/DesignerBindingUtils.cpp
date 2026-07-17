#include "DesignerBindingUtils.h"
#include "DesignerDataContextSchemaUtils.h"
#include <cwctype>

namespace DesignerBindingUtils
{
namespace
{
	bool EqualsIgnoreCase(const std::wstring& left, const std::wstring& right)
	{
		if (left.size() != right.size()) return false;
		for (size_t i = 0; i < left.size(); ++i)
		{
			if (std::towlower(left[i]) != std::towlower(right[i])) return false;
		}
		return true;
	}

	bool IsSourceToTarget(BindingMode mode) noexcept
	{
		return mode == BindingMode::OneWay
			|| mode == BindingMode::TwoWay
			|| mode == BindingMode::OneTime;
	}

	bool IsTargetToSource(BindingMode mode) noexcept
	{
		return mode == BindingMode::TwoWay
			|| mode == BindingMode::OneWayToSource;
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

bool IsValidSourcePath(const std::wstring& path)
{
	size_t start = 0;
	while (start <= path.size())
	{
		const size_t separator = path.find(L'.', start);
		const size_t end = separator == std::wstring::npos ? path.size() : separator;
		if (Trim(path.substr(start, end - start)).empty()) return false;
		if (separator == std::wstring::npos) return true;
		start = separator + 1;
	}
	return false;
}

const wchar_t* BindingModeName(BindingMode mode) noexcept
{
	switch (mode)
	{
	case BindingMode::OneWay: return L"OneWay";
	case BindingMode::TwoWay: return L"TwoWay";
	case BindingMode::OneWayToSource: return L"OneWayToSource";
	case BindingMode::OneTime: return L"OneTime";
	}
	return L"OneWay";
}

bool TryParseBindingMode(const std::wstring& value, BindingMode& mode)
{
	const auto text = Trim(value);
	if (EqualsIgnoreCase(text, L"OneWay")) { mode = BindingMode::OneWay; return true; }
	if (EqualsIgnoreCase(text, L"TwoWay")) { mode = BindingMode::TwoWay; return true; }
	if (EqualsIgnoreCase(text, L"OneWayToSource")) { mode = BindingMode::OneWayToSource; return true; }
	if (EqualsIgnoreCase(text, L"OneTime")) { mode = BindingMode::OneTime; return true; }
	return false;
}

const wchar_t* UpdateModeName(DataSourceUpdateMode mode) noexcept
{
	switch (mode)
	{
	case DataSourceUpdateMode::OnPropertyChanged: return L"OnPropertyChanged";
	case DataSourceUpdateMode::OnValidation: return L"OnValidation";
	case DataSourceUpdateMode::Never: return L"Never";
	}
	return L"OnPropertyChanged";
}

bool TryParseUpdateMode(const std::wstring& value, DataSourceUpdateMode& mode)
{
	const auto text = Trim(value);
	if (EqualsIgnoreCase(text, L"OnPropertyChanged"))
	{
		mode = DataSourceUpdateMode::OnPropertyChanged;
		return true;
	}
	if (EqualsIgnoreCase(text, L"OnValidation"))
	{
		mode = DataSourceUpdateMode::OnValidation;
		return true;
	}
	if (EqualsIgnoreCase(text, L"Never"))
	{
		mode = DataSourceUpdateMode::Never;
		return true;
	}
	return false;
}

const wchar_t* ValueKindName(BindingValueKind kind) noexcept
{
	switch (kind)
	{
	case BindingValueKind::Empty: return L"Empty";
	case BindingValueKind::Bool: return L"Bool";
	case BindingValueKind::Int: return L"Int";
	case BindingValueKind::Int64: return L"Int64";
	case BindingValueKind::Float: return L"Float";
	case BindingValueKind::Double: return L"Double";
	case BindingValueKind::String: return L"String";
	case BindingValueKind::Object: return L"Object";
	}
	return L"Unknown";
}

bool IsModeStructurallyCompatible(
	const BindingPropertyMetadata& metadata,
	BindingMode mode) noexcept
{
	return (!IsSourceToTarget(mode) || metadata.CanWrite())
		&& (!IsTargetToSource(mode) || metadata.CanRead());
}

bool IsCompatible(
	const BindingPropertyMetadata& metadata,
	const DesignerDataBinding& binding) noexcept
{
	return IsModeStructurallyCompatible(metadata, binding.Mode)
		&& (!IsTargetToSource(binding.Mode)
			|| binding.UpdateMode == DataSourceUpdateMode::Never
			|| metadata.CanObserve());
}

bool IsModeStructurallyCompatible(
	const TargetMetadata& metadata,
	BindingMode mode) noexcept
{
	return (!IsSourceToTarget(mode) || metadata.CanWrite)
		&& (!IsTargetToSource(mode) || metadata.CanRead);
}

bool ValidateTarget(
	const TargetMetadata& target,
	const DesignerDataBinding& binding,
	std::wstring* outError,
	const DesignerDataContextSchema* sourceSchema)
{
	if (target.Name.empty())
	{
		if (outError) *outError = L"请选择目标属性。";
		return false;
	}
	if (!IsValidSourcePath(binding.SourceProperty))
	{
		if (outError) *outError = L"源路径无效：路径及每个点分段都不能为空。";
		return false;
	}

	if (!IsModeStructurallyCompatible(target, binding.Mode))
	{
		if (outError) *outError = L"目标属性的读写能力不支持 "
			+ std::wstring(BindingModeName(binding.Mode)) + L"。";
		return false;
	}
	const bool targetToSource = binding.Mode == BindingMode::TwoWay
		|| binding.Mode == BindingMode::OneWayToSource;
	if (targetToSource
		&& binding.UpdateMode != DataSourceUpdateMode::Never
		&& !target.CanObserve)
	{
		if (outError) *outError = L"该目标属性没有变更通知；请使用 Never 更新策略或改用单向模式。";
		return false;
	}

	const DesignerDataContextProperty* sourceProperty = nullptr;
	if (sourceSchema && !sourceSchema->empty())
	{
		sourceProperty = DesignerDataContextSchemaUtils::Find(
			*sourceSchema, binding.SourceProperty);
		if (!sourceProperty)
		{
			if (outError) *outError = L"源路径未在 DataContext Schema 中声明："
				+ Trim(binding.SourceProperty);
			return false;
		}

		const bool sourceToTarget = IsSourceToTarget(binding.Mode);
		const auto normalizedSourcePath =
			DesignerDataContextSchemaUtils::NormalizePath(binding.SourceProperty);
		size_t separator = normalizedSourcePath.find(L'.');
		while (separator != std::wstring::npos)
		{
			const auto prefix = normalizedSourcePath.substr(0, separator);
			if (const auto* intermediate =
				DesignerDataContextSchemaUtils::Find(*sourceSchema, prefix))
			{
				if (!intermediate->CanRead)
				{
					if (outError) *outError = L"DataContext 中间属性不可读："
						+ intermediate->Path;
					return false;
				}
				if (sourceToTarget
					&& binding.Mode != BindingMode::OneTime
					&& !intermediate->CanObserve)
				{
					if (outError) *outError = L"DataContext 中间属性没有变更通知："
						+ intermediate->Path;
					return false;
				}
			}
			separator = normalizedSourcePath.find(L'.', separator + 1);
		}
		if (sourceToTarget && !sourceProperty->CanRead)
		{
			if (outError) *outError = L"DataContext 源属性不可读：" + sourceProperty->Path;
			return false;
		}
		if (sourceToTarget
			&& binding.Mode != BindingMode::OneTime
			&& !sourceProperty->CanObserve)
		{
			if (outError) *outError = L"DataContext 源属性没有变更通知；请使用 OneTime 或修改 Schema："
				+ sourceProperty->Path;
			return false;
		}
		if (targetToSource
			&& binding.UpdateMode != DataSourceUpdateMode::Never
			&& !sourceProperty->CanWrite)
		{
			if (outError) *outError = L"DataContext 源属性不可写：" + sourceProperty->Path;
			return false;
		}
	}

	const auto converterName = Trim(binding.Converter);
	if (!binding.Converter.empty() && converterName.empty())
	{
		if (outError) *outError = L"Converter 名称不能为空白。";
		return false;
	}
	if (!converterName.empty())
	{
		const auto converter = BindingValueConverterRegistry::Find(converterName);
		if (converter)
		{
			if (sourceProperty
				&& sourceProperty->ValueKind != BindingValueKind::Empty
				&& converter->SourceKind != BindingValueKind::Empty
				&& converter->SourceKind != sourceProperty->ValueKind)
			{
				if (outError) *outError = L"Converter " + converter->Name
					+ L" 的源值类型与 DataContext Schema 不兼容。";
				return false;
			}
			if (converter->TargetKind != BindingValueKind::Empty
				&& converter->TargetKind != target.ValueKind)
			{
				if (outError) *outError = L"Converter " + converter->Name
					+ L" 的目标值类型与属性不兼容。";
				return false;
			}
			if (targetToSource && !converter->CanConvertBack)
			{
				if (outError) *outError = L"Converter " + converter->Name
					+ L" 不支持 ConvertBack，不能用于当前绑定模式。";
				return false;
			}
		}
	}

	if (outError) outError->clear();
	return true;
}

bool Validate(
	Control& target,
	const std::wstring& targetProperty,
	const DesignerDataBinding& binding,
	const BindingPropertyMetadata** outMetadata,
	std::wstring* outError,
	const DesignerDataContextSchema* sourceSchema)
{
	if (outMetadata) *outMetadata = nullptr;
	const auto* metadata = BindingPropertyRegistry::Find(target, targetProperty);
	if (!metadata)
	{
		if (outError) *outError = targetProperty.empty()
			? L"请选择目标属性。"
			: L"目标属性不存在：" + targetProperty;
		return false;
	}
	const TargetMetadata portable{
		metadata->Name(), metadata->ValueKind(),
		metadata->CanRead(), metadata->CanWrite(), metadata->CanObserve() };
	if (!ValidateTarget(portable, binding, outError, sourceSchema))
		return false;
	if (outMetadata) *outMetadata = metadata;
	return true;
}

std::wstring Describe(
	const std::wstring& targetProperty,
	const DesignerDataBinding& binding)
{
	std::wstring description = targetProperty + L" <- " + binding.SourceProperty + L"  ["
		+ BindingModeName(binding.Mode) + L", "
		+ UpdateModeName(binding.UpdateMode);
	if (!binding.Converter.empty())
		description += L", Converter=" + binding.Converter;
	description += L"]";
	return description;
}
}
