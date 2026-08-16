#include "DesignerBindingUtils.h"
#include "../CuiRuntime/include/BindingConverterRegistry.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include <TreeInfrastructure.h>
#include <Convert.h>
#include <cwctype>
#include <unordered_set>
#include <utility>

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

	std::wstring LocalTypeName(const std::wstring& token)
	{
		const auto separator = token.find(L':');
		return separator == std::wstring::npos
			? token : token.substr(separator + 1);
	}

	bool IsBuiltInTypeMatch(Control& candidate, const std::wstring& typeName)
	{
		UIClass requested = UIClass::UI_Base;
		if (!DesignerStyleSheetUtils::TryParseUIClass(
			LocalTypeName(typeName), requested)) return false;
		auto actual = candidate.Type();
		for (;;)
		{
			if (actual == requested) return true;
			if (actual == UIClass::UI_Base) return false;
			actual = GetUIClassBase(actual);
		}
	}

	bool IsAncestorTypeMatch(
		Control& candidate,
		const DesignerDataBinding& binding)
	{
		if (binding.AncestorTypeNamespace.empty())
			return IsBuiltInTypeMatch(candidate, binding.AncestorType);
		return candidate.GetDeclarativeTypeNamespace()
			== binding.AncestorTypeNamespace
			&& candidate.GetDeclarativeTypeName()
				== LocalTypeName(binding.AncestorType);
	}

	class AncestorBindingSource final : public IBindingSource
	{
	public:
		AncestorBindingSource(Control& target, DesignerDataBinding binding)
			: _target(&target), _binding(std::move(binding))
		{
			Attach();
		}

		bool TryGetValue(const std::wstring& propertyName,
			BindingValue& out) const override
		{
			return _source && _source->TryGetValue(propertyName, out);
		}

		bool TrySetValue(const std::wstring& propertyName,
			const BindingValue& value) override
		{
			return _source && _source->TrySetValue(propertyName, value);
		}

		bool TryGetPropertyMetadata(const std::wstring& propertyName,
			BindingSourcePropertyMetadata& out) const override
		{
			return _source && _source->TryGetPropertyMetadata(propertyName, out);
		}

		std::vector<BindingSourcePropertyMetadata> GetProperties() const override
		{
			return _source ? _source->GetProperties()
				: std::vector<BindingSourcePropertyMetadata>{};
		}

		std::vector<BindingValidationIssue> GetValidationIssues(
			const std::wstring& propertyName) const override
		{
			return _source ? _source->GetValidationIssues(propertyName)
				: std::vector<BindingValidationIssue>{};
		}

		BindingValidationChangedEvent* ValidationChanged() noexcept override
		{
			return &_validationChanged;
		}

		PropertyChangedEvent& PropertyChanged() override
		{
			return _propertyChanged;
		}

	private:
		Control* _target = nullptr;
		DesignerDataBinding _binding;
		Control* _source = nullptr;
		std::vector<EventConnection> _parentConnections;
		EventConnection _sourcePropertyConnection;
		EventConnection _sourceValidationConnection;
		PropertyChangedEvent _propertyChanged;
		BindingValidationChangedEvent _validationChanged;
		bool _attaching = false;

		void Attach()
		{
			if (_attaching) return;
			_attaching = true;
			_parentConnections.clear();
			_sourcePropertyConnection.Disconnect();
			_sourceValidationConnection.Disconnect();

			if (_target)
			{
				std::unordered_set<Control*> visited;
				for (auto* item = _target;
					item && visited.insert(item).second;
					item = item->GetRoutedParent())
				{
					_parentConnections.push_back(
						cui::framework::TreeAccess::SubscribeVisualParentChanged(
							*item,
							[this](Control*, Control*, Control*) { Attach(); }));
					_parentConnections.push_back(
						cui::framework::TreeAccess::SubscribeLogicalParentChanged(
							*item,
							[this](Control*, Control*, Control*) { Attach(); }));
					_parentConnections.push_back(
						cui::framework::TreeAccess::SubscribeTemplatedParentChanged(
							*item,
							[this](Control*, Control*, Control*) { Attach(); }));
				}
			}
			auto* next = _target
				? DesignerBindingUtils::FindAncestorSource(*_target, _binding)
				: nullptr;
			const bool changed = next != _source;
			_source = next;
			if (_source)
			{
				_sourcePropertyConnection = _source->PropertyChanged().Subscribe(
					[this](const PropertyChangedEventArgs& e)
					{
						_propertyChanged.Notify(e.PropertyName);
					});
				if (auto* validation = _source->ValidationChanged())
				{
					_sourceValidationConnection = validation->Subscribe(
						[this](const BindingValidationChangedEventArgs& e)
						{
							_validationChanged.Notify(e.PropertyName);
						});
				}
			}
			_attaching = false;
			if (changed)
			{
				_propertyChanged.Notify(L"");
				_validationChanged.Notify(L"");
			}
		}
	};
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
	std::vector<BindingPathStep> steps;
	return TryParseBindingPropertyPath(path, steps);
}

bool ValidateDataGridColumnBindingSource(
	const DesignerDataBinding& binding,
	UIClass* outSourceType,
	std::wstring* outError,
	UIClass elementNameSourceType)
{
	const auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		if (outSourceType) *outSourceType = UIClass::UI_Base;
		return false;
	};
	if (outSourceType) *outSourceType = UIClass::UI_Base;
	if (binding.IsMultiBinding())
	{
		if (!binding.SourceProperty.empty() || !binding.ElementName.empty()
			|| binding.RelativeSource != DesignerBindingRelativeSource::None
			|| !binding.AncestorType.empty()
			|| !binding.AncestorTypeNamespace.empty()
			|| binding.AncestorLevel != 1)
			return fail(L"DataGrid 列 MultiBinding 的源只能由 Binding 子项声明。");
		if (binding.ChildBindings.size() < 2)
			return fail(L"DataGrid 列 MultiBinding 至少需要两个 Binding 子项。");
		const auto converterName = Trim(binding.Converter);
		if (!binding.Converter.empty() && converterName.empty())
			return fail(L"DataGrid 列 MultiBinding Converter 名称不能为空白。");
		if (converterName.empty())
		{
			if (!binding.StringFormat
				|| !IsValidMultiBindingStringFormat(
					*binding.StringFormat, binding.ChildBindings.size()))
				return fail(L"DataGrid 列 MultiBinding 需要 Converter 或有效的 StringFormat。");
			if (binding.Mode != BindingMode::OneWay
				&& binding.Mode != BindingMode::OneTime)
				return fail(L"无 Converter 的 DataGrid 列 MultiBinding 必须显式使用 OneWay 或 OneTime。");
		}
		else
		{
			if (binding.StringFormat
				&& !IsValidBindingStringFormat(*binding.StringFormat))
				return fail(L"DataGrid 列 MultiBinding StringFormat 语法无效。");
		}
		for (size_t index = 0; index < binding.ChildBindings.size(); ++index)
		{
			const auto& child = binding.ChildBindings[index];
			if (child.IsMultiBinding())
				return fail(L"DataGrid 列 MultiBinding 不支持嵌套 MultiBinding。");
			std::wstring childError;
			if (!ValidateDataGridColumnBindingSource(
				child, nullptr, &childError, UIClass::UI_Base))
				return fail(L"DataGrid 列 MultiBinding 第 "
					+ std::to_wstring(index + 1) + L" 个源无效：" + childError);
		}
		if (outError) outError->clear();
		return true;
	}
	if (binding.SourceProperty.empty()
		|| !IsValidSourcePath(binding.SourceProperty))
		return fail(L"DataGrid 列 Binding 必须包含有效的源路径。");
	if (!binding.ElementName.empty()
		&& binding.RelativeSource != DesignerBindingRelativeSource::None)
		return fail(L"DataGrid 列 Binding 不能同时声明 ElementName 与 RelativeSource。");

	auto validateExactNativeProperty = [&](UIClass sourceType,
		const std::wstring& sourceDescription) -> bool
	{
		std::vector<BindingPathStep> steps;
		if (!TryParseBindingPropertyPath(binding.SourceProperty, steps)
			|| steps.size() != 1
			|| steps.front().Kind != BindingPathStepKind::Property)
			return fail(L"DataGrid 列 " + sourceDescription
				+ L" 当前仅支持单个依赖属性路径。");
		if (sourceType == UIClass::UI_Base) return true;
		if (sourceType == UIClass::UI_CUSTOM)
			return fail(L"DataGrid 列 ElementName 当前仅支持内置控件。");
		const auto* metadata = CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
			sourceType, steps.front().Value);
		if (!metadata)
			return fail(L"DataGrid 列 " + sourceDescription
				+ L"源类型不存在依赖属性：" + steps.front().Value);
		const bool reads = binding.Mode == BindingMode::Default
			|| binding.Mode == BindingMode::OneWay
			|| binding.Mode == BindingMode::TwoWay
			|| binding.Mode == BindingMode::OneTime;
		const bool writes = binding.Mode == BindingMode::Default
			|| binding.Mode == BindingMode::TwoWay
			|| binding.Mode == BindingMode::OneWayToSource;
		if (reads && !metadata->CanRead())
			return fail(L"DataGrid 列 " + sourceDescription
				+ L"源属性不可读：" + steps.front().Value);
		if (writes && !metadata->CanWrite())
			return fail(L"DataGrid 列 " + sourceDescription
				+ L"源属性不可写：" + steps.front().Value);
		if (reads && binding.Mode != BindingMode::OneTime
			&& !metadata->CanObserve())
			return fail(L"DataGrid 列 " + sourceDescription
				+ L"源属性不可观察：" + steps.front().Value);
		if (outSourceType) *outSourceType = sourceType;
		return true;
	};

	if (!binding.ElementName.empty())
	{
		if (binding.ElementName.find_first_of(L".,={} \t\r\n")
			!= std::wstring::npos)
			return fail(L"DataGrid 列 Binding ElementName 必须是直接 x:Name。");
		if (!binding.AncestorType.empty()
			|| !binding.AncestorTypeNamespace.empty()
			|| binding.AncestorLevel != 1)
			return fail(L"AncestorType/AncestorLevel 只能用于 FindAncestor。");
		if (!validateExactNativeProperty(
			elementNameSourceType, L"ElementName")) return false;
		if (outError) outError->clear();
		return true;
	}

	if (binding.RelativeSource == DesignerBindingRelativeSource::None)
	{
		if (!binding.AncestorType.empty()
			|| !binding.AncestorTypeNamespace.empty()
			|| binding.AncestorLevel != 1)
			return fail(L"AncestorType/AncestorLevel 只能用于 FindAncestor。");
		if (outError) outError->clear();
		return true;
	}
	if (binding.RelativeSource != DesignerBindingRelativeSource::FindAncestor)
		return fail(L"DataGrid 列 Binding 仅支持 RelativeSource FindAncestor。");
	if (!binding.AncestorTypeNamespace.empty())
		return fail(L"DataGrid 列 FindAncestor 仅支持内置 DataGrid 类型。");
	if (binding.AncestorLevel != 1)
		return fail(L"DataGrid 列 FindAncestor 仅支持 AncestorLevel=1。");

	UIClass ancestorType = UIClass::UI_Base;
	if (!DesignerStyleSheetUtils::TryParseUIClass(
			binding.AncestorType, ancestorType)
		|| (ancestorType != UIClass::UI_DataGridCell
			&& ancestorType != UIClass::UI_DataGridRow
			&& ancestorType != UIClass::UI_DataGrid))
		return fail(L"DataGrid 列 FindAncestor 的 AncestorType 仅支持 "
			L"DataGridCell、DataGridRow 或 DataGrid。");

	if (!validateExactNativeProperty(ancestorType, L"FindAncestor "))
		return false;
	if (outError) outError->clear();
	return true;
}

DesignerDataContextSchema BuildSourceSchema(const IBindingSource& source)
{
	DesignerDataContextSchema schema;
	std::wstring ignored;
	if (!DesignerDataContextSchemaUtils::BuildFromBindingSource(
		source, schema, &ignored)) schema.clear();
	DesignerDataContextSchemaUtils::Canonicalize(schema);
	return schema;
}

void WriteOptionalLiteral(
	DesignerModel::DesignValue& object,
	const char* valueKey,
	const char* kindKey,
	const std::optional<DesignerStyleValue>& value)
{
	if (!value) return;
	object[valueKey] = Convert::UnicodeToUtf8(value->Text);
	object[kindKey] = Convert::UnicodeToUtf8(
		DesignerStyleSheetUtils::ValueKindName(value->Kind));
}

bool TryReadOptionalLiteral(
	const DesignerModel::DesignValue& object,
	const char* valueKey,
	const char* kindKey,
	std::optional<DesignerStyleValue>& value,
	std::wstring* outError)
{
	value.reset();
	if (!object.is_object() || !object.contains(valueKey))
	{
		if (outError) outError->clear();
		return true;
	}
	if (!object[valueKey].is_string())
	{
		if (outError) *outError = L"Binding 缺省值必须是字符串字面量。";
		return false;
	}
	DesignerStyleValueKind kind = DesignerStyleValueKind::String;
	if (object.contains(kindKey))
	{
		if (!object[kindKey].is_string()
			|| !DesignerStyleSheetUtils::TryParseValueKind(
				Convert::Utf8ToUnicode(object[kindKey].get<std::string>()), kind))
		{
			if (outError) *outError = L"Binding 缺省值类型无效。";
			return false;
		}
	}
	value = DesignerStyleValue{
		kind, Convert::Utf8ToUnicode(object[valueKey].get<std::string>()) };
	if (outError) outError->clear();
	return true;
}

bool TryConvertOptionalLiteral(
	const std::optional<DesignerStyleValue>& value,
	std::optional<BindingValue>& output,
	std::wstring* outError)
{
	output.reset();
	if (!value)
	{
		if (outError) outError->clear();
		return true;
	}
	BindingValue converted;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		*value, converted, outError)) return false;
	output = std::move(converted);
	if (outError) outError->clear();
	return true;
}

DesignerModel::DesignValue WriteBindingDefinition(
	const DesignerDataBinding& binding)
{
	DesignerModel::DesignValue result{
		{ "mode", static_cast<int>(binding.Mode) },
		{ "updateMode", static_cast<int>(binding.UpdateMode) }
	};
	if (binding.IsMultiBinding())
	{
		result["kind"] = "MultiBinding";
		auto children = DesignerModel::DesignValue::array();
		for (const auto& child : binding.ChildBindings)
			children.push_back(WriteBindingDefinition(child));
		result["bindings"] = std::move(children);
	}
	else
	{
		result["source"] = Convert::UnicodeToUtf8(binding.SourceProperty);
		if (!binding.ElementName.empty())
			result["elementName"] = Convert::UnicodeToUtf8(binding.ElementName);
		if (binding.RelativeSource != DesignerBindingRelativeSource::None)
		{
			result["relativeSource"] = binding.RelativeSource
				== DesignerBindingRelativeSource::Self ? "Self"
				: binding.RelativeSource
					== DesignerBindingRelativeSource::TemplatedParent
					? "TemplatedParent" : "FindAncestor";
			if (binding.RelativeSource
				== DesignerBindingRelativeSource::FindAncestor)
			{
				result["ancestorType"] = Convert::UnicodeToUtf8(
					binding.AncestorType);
				if (!binding.AncestorTypeNamespace.empty())
					result["ancestorTypeNamespace"] = Convert::UnicodeToUtf8(
						binding.AncestorTypeNamespace);
				if (binding.AncestorLevel != 1)
					result["ancestorLevel"] = binding.AncestorLevel;
			}
		}
	}
	if (!binding.Converter.empty())
		result["converter"] = Convert::UnicodeToUtf8(binding.Converter);
	WriteOptionalLiteral(
		result, "fallbackValue", "fallbackValueKind", binding.FallbackValue);
	WriteOptionalLiteral(
		result, "targetNullValue", "targetNullValueKind", binding.TargetNullValue);
	WriteOptionalLiteral(result, "converterParameter", "converterParameterKind",
		binding.ConverterParameter);
	if (binding.StringFormat)
		result["stringFormat"] = Convert::UnicodeToUtf8(*binding.StringFormat);
	return result;
}

namespace
{
	bool TryReadBindingDefinitionCore(
		const DesignerModel::DesignValue& value,
		DesignerDataBinding& binding,
		std::wstring* outError,
		size_t depth)
	{
		auto fail = [&](const std::wstring& message)
		{
			if (outError) *outError = message;
			return false;
		};
		if (!value.is_object() || depth > 32)
			return fail(L"Binding 定义无效或嵌套过深。");
		binding = {};
		const int mode = value.value(
			"mode", static_cast<int>(BindingMode::OneWay));
		const int updateMode = value.value("updateMode",
			static_cast<int>(DataSourceUpdateMode::OnPropertyChanged));
		if (mode < static_cast<int>(BindingMode::OneWay)
			|| mode > static_cast<int>(BindingMode::Default)
			|| updateMode < static_cast<int>(
				DataSourceUpdateMode::OnPropertyChanged)
			|| updateMode > static_cast<int>(DataSourceUpdateMode::Default))
			return fail(L"Binding Mode 或 UpdateMode 无效。");
		binding.Mode = static_cast<BindingMode>(mode);
		binding.UpdateMode = static_cast<DataSourceUpdateMode>(updateMode);
		if (value.contains("converter"))
		{
			if (!value["converter"].is_string())
				return fail(L"Binding Converter 必须是字符串。");
			binding.Converter = Convert::Utf8ToUnicode(
				value["converter"].get<std::string>());
		}
		std::wstring literalError;
		if (!TryReadOptionalLiteral(value, "fallbackValue", "fallbackValueKind",
			binding.FallbackValue, &literalError)
			|| !TryReadOptionalLiteral(value, "targetNullValue",
				"targetNullValueKind", binding.TargetNullValue, &literalError)
			|| !TryReadOptionalLiteral(value, "converterParameter",
				"converterParameterKind", binding.ConverterParameter,
				&literalError))
			return fail(literalError);
		if (value.contains("stringFormat"))
		{
			if (!value["stringFormat"].is_string())
				return fail(L"Binding StringFormat 必须是字符串。");
			binding.StringFormat = Convert::Utf8ToUnicode(
				value["stringFormat"].get<std::string>());
		}

		const bool multi = value.value("kind", std::string{}) == "MultiBinding"
			|| value.contains("bindings");
		if (multi)
		{
			if (!value.contains("bindings") || !value["bindings"].is_array()
				|| value["bindings"].size() < 2)
				return fail(L"MultiBinding 至少需要两个 Binding 子项。");
			for (const auto& childValue : value["bindings"].ArrayItems())
			{
				DesignerDataBinding child;
				if (!TryReadBindingDefinitionCore(
					childValue, child, outError, depth + 1)) return false;
				if (child.IsMultiBinding())
					return fail(L"MultiBinding 不支持嵌套 MultiBinding。");
				binding.ChildBindings.push_back(std::move(child));
			}
			if (!binding.Converter.empty())
			{
				if (binding.StringFormat
					&& !IsValidBindingStringFormat(*binding.StringFormat))
					return fail(L"MultiBinding StringFormat 语法无效。");
			}
			else if (!binding.StringFormat
				|| !IsValidMultiBindingStringFormat(
					*binding.StringFormat, binding.ChildBindings.size()))
				return fail(L"MultiBinding 需要 Converter 或有效的 StringFormat。");
		}
		else
		{
			if (!value.contains("source") || !value["source"].is_string())
				return fail(L"Binding Source 路径无效。");
			binding.SourceProperty = Convert::Utf8ToUnicode(
				value["source"].get<std::string>());
			if (value.contains("elementName"))
			{
				if (!value["elementName"].is_string())
					return fail(L"Binding ElementName 无效。");
				binding.ElementName = Convert::Utf8ToUnicode(
					value["elementName"].get<std::string>());
			}
			const auto relative = Convert::Utf8ToUnicode(
				value.value("relativeSource", std::string{}));
			if (relative == L"Self")
				binding.RelativeSource = DesignerBindingRelativeSource::Self;
			else if (relative == L"TemplatedParent")
				binding.RelativeSource = DesignerBindingRelativeSource::TemplatedParent;
			else if (relative == L"FindAncestor")
				binding.RelativeSource = DesignerBindingRelativeSource::FindAncestor;
			else if (!relative.empty())
				return fail(L"Binding RelativeSource 无效。");
			binding.AncestorType = Convert::Utf8ToUnicode(
				value.value("ancestorType", std::string{}));
			binding.AncestorTypeNamespace = Convert::Utf8ToUnicode(
				value.value("ancestorTypeNamespace", std::string{}));
			binding.AncestorLevel = value.value("ancestorLevel", 1);
			if (binding.StringFormat
				&& !IsValidBindingStringFormat(*binding.StringFormat))
				return fail(L"Binding StringFormat 语法无效。");
		}
		if (outError) outError->clear();
		return true;
	}
}

bool TryReadBindingDefinition(
	const DesignerModel::DesignValue& value,
	DesignerDataBinding& binding,
	std::wstring* outError)
{
	return TryReadBindingDefinitionCore(value, binding, outError, 0);
}

bool VisitLeafBindingDefinitions(
	const DesignerDataBinding& binding,
	const std::function<bool(const DesignerDataBinding&)>& visitor)
{
	if (!binding.IsMultiBinding()) return visitor(binding);
	for (const auto& child : binding.ChildBindings)
		if (!VisitLeafBindingDefinitions(child, visitor)) return false;
	return true;
}

bool VisitLeafBindingDefinitions(
	DesignerDataBinding& binding,
	const std::function<bool(DesignerDataBinding&)>& visitor)
{
	if (!binding.IsMultiBinding()) return visitor(binding);
	for (auto& child : binding.ChildBindings)
		if (!VisitLeafBindingDefinitions(child, visitor)) return false;
	return true;
}

bool InstallBinding(
	Control& target,
	const std::wstring& targetProperty,
	const DesignerDataBinding& binding,
	const BindingSourceResolver& resolveSource,
	std::wstring* outError)
{
	auto fail = [&](const std::wstring& message)
	{
		if (outError) *outError = message;
		return false;
	};
	auto convertOptions = [&](const DesignerDataBinding& item,
		std::optional<BindingValue>& fallbackValue,
		std::optional<BindingValue>& targetNullValue,
		std::optional<BindingValue>& converterParameter)
	{
		std::wstring literalError;
		if (!TryConvertOptionalLiteral(
			item.FallbackValue, fallbackValue, &literalError)
			|| !TryConvertOptionalLiteral(
				item.TargetNullValue, targetNullValue, &literalError)
			|| !TryConvertOptionalLiteral(
				item.ConverterParameter, converterParameter, &literalError))
			return fail(literalError);
		return true;
	};

	std::optional<BindingValue> fallbackValue;
	std::optional<BindingValue> targetNullValue;
	std::optional<BindingValue> converterParameter;
	if (!convertOptions(binding, fallbackValue,
		targetNullValue, converterParameter)) return false;

	if (binding.IsMultiBinding())
	{
		std::shared_ptr<const IMultiBindingValueConverter> converter;
		const auto converterName = Trim(binding.Converter);
		if (!converterName.empty())
		{
			converter = MultiBindingValueConverterRegistry::Create(converterName);
			if (!converter)
				return fail(L"无法创建 MultiBinding Converter：" + converterName);
		}
		std::vector<MultiBindingSource> sources;
		sources.reserve(binding.ChildBindings.size());
		for (size_t index = 0; index < binding.ChildBindings.size(); ++index)
		{
			const auto& child = binding.ChildBindings[index];
			ResolvedBindingSource resolved;
			std::wstring resolveError;
			if (!resolveSource(child, resolved, &resolveError)
				|| (!resolved.Source && !resolved.OwnedSource))
				return fail(L"第 " + std::to_wstring(index + 1)
					+ L" 个 MultiBinding 源无法解析"
					+ (resolveError.empty() ? L"。" : L"：" + resolveError));
			std::shared_ptr<const IBindingValueConverter> childConverter;
			const auto childConverterName = Trim(child.Converter);
			if (!childConverterName.empty())
			{
				childConverter = BindingValueConverterRegistry::Create(
					childConverterName);
				if (!childConverter)
					return fail(L"无法创建第 " + std::to_wstring(index + 1)
						+ L" 个 Binding Converter：" + childConverterName);
			}
			std::optional<BindingValue> childFallback;
			std::optional<BindingValue> childTargetNull;
			std::optional<BindingValue> childParameter;
			if (!convertOptions(child, childFallback,
				childTargetNull, childParameter)) return false;
			MultiBindingSource source = resolved.OwnedSource
				? MultiBindingSource(std::move(resolved.OwnedSource),
					child.SourceProperty, std::move(childConverter),
					std::move(childFallback), std::move(childTargetNull),
					std::move(childParameter), child.StringFormat)
				: MultiBindingSource(resolved.Source, child.SourceProperty,
					std::move(childConverter), std::move(childFallback),
					std::move(childTargetNull), std::move(childParameter),
					child.StringFormat);
			source.Mode = child.Mode;
			source.UpdateMode = child.UpdateMode;
			sources.push_back(std::move(source));
		}
		if (!target.DataBindings.AddMulti(targetProperty, std::move(sources),
			binding.Mode, binding.UpdateMode, std::move(converter),
			std::move(fallbackValue), std::move(targetNullValue),
			std::move(converterParameter), binding.StringFormat))
			return fail(L"MultiBinding 创建失败："
				+ std::wstring(target.DataBindings.LastErrorMessage()));
	}
	else
	{
		ResolvedBindingSource resolved;
		std::wstring resolveError;
		if (!resolveSource(binding, resolved, &resolveError)
			|| (!resolved.Source && !resolved.OwnedSource))
			return fail(resolveError.empty()
				? L"Binding 源无法解析。" : resolveError);
		std::shared_ptr<const IBindingValueConverter> converter;
		const auto converterName = Trim(binding.Converter);
		if (!converterName.empty())
		{
			converter = BindingValueConverterRegistry::Create(converterName);
			if (!converter)
				return fail(L"无法创建 Converter：" + converterName);
		}
		auto* installed = resolved.OwnedSource
			? target.DataBindings.Add(targetProperty,
				std::move(resolved.OwnedSource), binding.SourceProperty,
				binding.Mode, binding.UpdateMode, std::move(converter),
				std::move(fallbackValue), std::move(targetNullValue),
				std::move(converterParameter), binding.StringFormat)
			: target.DataBindings.Add(targetProperty, resolved.Source,
				binding.SourceProperty, binding.Mode, binding.UpdateMode,
				std::move(converter), std::move(fallbackValue),
				std::move(targetNullValue), std::move(converterParameter),
				binding.StringFormat);
		if (!installed)
			return fail(L"Binding 创建失败："
				+ std::wstring(target.DataBindings.LastErrorMessage()));
	}
	if (outError) outError->clear();
	return true;
}

Control* FindAncestorSource(
	Control& target,
	const DesignerDataBinding& binding) noexcept
{
	if (binding.RelativeSource != DesignerBindingRelativeSource::FindAncestor
		|| binding.AncestorLevel < 1 || Trim(binding.AncestorType).empty())
		return nullptr;
	int remaining = binding.AncestorLevel;
	std::unordered_set<Control*> visited;
	for (auto* candidate = target.GetRoutedParent();
		candidate && visited.insert(candidate).second;
		candidate = candidate->GetRoutedParent())
	{
		if (!IsAncestorTypeMatch(*candidate, binding)) continue;
		if (--remaining == 0) return candidate;
	}
	return nullptr;
}

BindingSourceReference CreateAncestorSource(
	Control& target,
	const DesignerDataBinding& binding)
{
	if (binding.RelativeSource != DesignerBindingRelativeSource::FindAncestor)
		return {};
	return BindingSourceReference(
		std::make_shared<AncestorBindingSource>(target, binding));
}

const wchar_t* BindingModeName(BindingMode mode) noexcept
{
	switch (mode)
	{
	case BindingMode::Default: return L"Default";
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
	if (EqualsIgnoreCase(text, L"Default")) { mode = BindingMode::Default; return true; }
	if (EqualsIgnoreCase(text, L"OneWay")) { mode = BindingMode::OneWay; return true; }
	if (EqualsIgnoreCase(text, L"TwoWay")) { mode = BindingMode::TwoWay; return true; }
	if (EqualsIgnoreCase(text, L"OneWayToSource")) { mode = BindingMode::OneWayToSource; return true; }
	if (EqualsIgnoreCase(text, L"OneTime")) { mode = BindingMode::OneTime; return true; }
	return false;
}

BindingMode ResolveBindingMode(
	const TargetMetadata& target,
	BindingMode requested) noexcept
{
	if (requested != BindingMode::Default) return requested;
	return HasDependencyPropertyFlag(
		target.Flags, DependencyPropertyFlags::BindsTwoWayByDefault)
		? BindingMode::TwoWay
		: BindingMode::OneWay;
}

DataSourceUpdateMode ResolveUpdateMode(
	const TargetMetadata& target,
	DataSourceUpdateMode requested) noexcept
{
	return requested == DataSourceUpdateMode::Default
		? (target.DefaultUpdateMode == DataSourceUpdateMode::Default
			? DataSourceUpdateMode::OnPropertyChanged
			: target.DefaultUpdateMode)
		: requested;
}

const wchar_t* UpdateModeName(DataSourceUpdateMode mode) noexcept
{
	switch (mode)
	{
	case DataSourceUpdateMode::Default: return L"Default";
	case DataSourceUpdateMode::OnPropertyChanged: return L"OnPropertyChanged";
	case DataSourceUpdateMode::OnValidation: return L"OnValidation";
	case DataSourceUpdateMode::Never: return L"Never";
	}
	return L"OnPropertyChanged";
}

const wchar_t* UpdateSourceTriggerName(DataSourceUpdateMode mode) noexcept
{
	switch (mode)
	{
	case DataSourceUpdateMode::Default: return L"Default";
	case DataSourceUpdateMode::OnPropertyChanged: return L"PropertyChanged";
	case DataSourceUpdateMode::OnValidation: return L"LostFocus";
	case DataSourceUpdateMode::Never: return L"Explicit";
	}
	return L"Default";
}

bool TryParseUpdateMode(const std::wstring& value, DataSourceUpdateMode& mode)
{
	const auto text = Trim(value);
	if (EqualsIgnoreCase(text, L"Default"))
	{
		mode = DataSourceUpdateMode::Default;
		return true;
	}
	if (EqualsIgnoreCase(text, L"OnPropertyChanged")
		|| EqualsIgnoreCase(text, L"PropertyChanged"))
	{
		mode = DataSourceUpdateMode::OnPropertyChanged;
		return true;
	}
	if (EqualsIgnoreCase(text, L"OnValidation")
		|| EqualsIgnoreCase(text, L"LostFocus")
		|| EqualsIgnoreCase(text, L"Validation"))
	{
		mode = DataSourceUpdateMode::OnValidation;
		return true;
	}
	if (EqualsIgnoreCase(text, L"Never")
		|| EqualsIgnoreCase(text, L"Explicit"))
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
	case BindingValueKind::NullableBool: return L"NullableBool";
	case BindingValueKind::Int: return L"Int";
	case BindingValueKind::Int64: return L"Int64";
	case BindingValueKind::Float: return L"Float";
	case BindingValueKind::Double: return L"Double";
	case BindingValueKind::String: return L"String";
	case BindingValueKind::Object: return L"Object";
	}
	return L"Unknown";
}

namespace
{
	bool AreBindingKindsCompatible(
		BindingValueKind expected,
		BindingValueKind actual) noexcept
	{
		return expected == actual
			|| ((expected == BindingValueKind::Bool
					|| expected == BindingValueKind::NullableBool)
				&& (actual == BindingValueKind::Bool
					|| actual == BindingValueKind::NullableBool));
	}
}

bool IsModeStructurallyCompatible(
	const DependencyPropertyMetadata& metadata,
	BindingMode mode) noexcept
{
	if (metadata.IsReadOnly()) return false;
	mode = ::ResolveBindingMode(metadata, mode);
	return (!IsSourceToTarget(mode) || metadata.CanWrite())
		&& (!IsTargetToSource(mode) || metadata.CanRead());
}

bool IsCompatible(
	const DependencyPropertyMetadata& metadata,
	const DesignerDataBinding& binding) noexcept
{
	const auto mode = ::ResolveBindingMode(metadata, binding.Mode);
	const auto updateMode = ::ResolveDataSourceUpdateMode(
		metadata, binding.UpdateMode);
	return IsModeStructurallyCompatible(metadata, mode)
		&& (!IsTargetToSource(mode)
			|| updateMode == DataSourceUpdateMode::Never
			|| metadata.CanObserve());
}

bool IsModeStructurallyCompatible(
	const TargetMetadata& metadata,
	BindingMode mode) noexcept
{
	if (metadata.IsReadOnly) return false;
	mode = ResolveBindingMode(metadata, mode);
	return (!IsSourceToTarget(mode) || metadata.CanWrite)
		&& (!IsTargetToSource(mode) || metadata.CanRead);
}

TargetMetadata ProjectTargetMetadata(
	const DependencyPropertyMetadata& metadata)
{
	return {
		metadata.Name(), metadata.ValueKind(),
		metadata.CanRead(), metadata.CanWrite(), metadata.CanObserve(),
		DesignerDataContextSchemaUtils::ObjectKindForValueType(
			metadata.ValueType()), metadata.Flags(),
		metadata.DefaultUpdateMode(), metadata.IsReadOnly() };
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
	if (target.IsReadOnly)
	{
		if (outError) *outError = L"只读属性不能作为 Binding 或 MultiBinding 目标。";
		return false;
	}
	const auto effectiveMode = ResolveBindingMode(target, binding.Mode);
	const auto effectiveUpdateMode = ResolveUpdateMode(
		target, binding.UpdateMode);
	if (binding.IsMultiBinding())
	{
		if (!binding.SourceProperty.empty() || !binding.ElementName.empty()
			|| binding.RelativeSource != DesignerBindingRelativeSource::None
			|| !binding.AncestorType.empty()
			|| !binding.AncestorTypeNamespace.empty()
			|| binding.AncestorLevel != 1)
		{
			if (outError) *outError = L"MultiBinding 的源只能由 Binding 子项声明。";
			return false;
		}
		if (binding.ChildBindings.size() < 2)
		{
			if (outError) *outError = L"MultiBinding 至少需要两个 Binding 子项。";
			return false;
		}
		if (!IsModeStructurallyCompatible(target, effectiveMode))
		{
			if (outError) *outError = L"目标属性的读写能力不支持 "
				+ std::wstring(BindingModeName(effectiveMode)) + L"。";
			return false;
		}
		const bool targetToSource = IsTargetToSource(effectiveMode);
		if (targetToSource
			&& effectiveUpdateMode != DataSourceUpdateMode::Never
			&& !target.CanObserve)
		{
			if (outError) *outError = L"该目标属性没有变更通知；请使用 Never 更新策略或改用单向模式。";
			return false;
		}
		const auto converterName = Trim(binding.Converter);
		if (!binding.Converter.empty() && converterName.empty())
		{
			if (outError) *outError = L"Converter 名称不能为空白。";
			return false;
		}
		if (binding.StringFormat
			&& target.ValueKind != BindingValueKind::String)
		{
			if (outError) *outError = L"MultiBinding StringFormat 只能用于 String 目标属性。";
			return false;
		}
		if (converterName.empty())
		{
			if (!binding.StringFormat
				|| !IsValidMultiBindingStringFormat(
					*binding.StringFormat, binding.ChildBindings.size()))
			{
				if (outError) *outError = L"MultiBinding 需要 Converter 或有效的 StringFormat。";
				return false;
			}
		}
		else
		{
			if (binding.StringFormat
				&& !IsValidBindingStringFormat(*binding.StringFormat))
			{
				if (outError) *outError = L"MultiBinding Converter 后的 StringFormat 语法无效。";
				return false;
			}
			if (const auto converter =
				MultiBindingValueConverterRegistry::Find(converterName))
			{
				if (converter->MinimumInputCount > binding.ChildBindings.size())
				{
					if (outError) *outError = L"MultiBinding Converter "
						+ converter->Name + L" 需要更多源值。";
					return false;
				}
				if (converter->TargetKind != BindingValueKind::Empty
					&& !AreBindingKindsCompatible(
						target.ValueKind, converter->TargetKind))
				{
					if (outError) *outError = L"MultiBinding Converter "
						+ converter->Name + L" 的目标值类型与属性不兼容。";
					return false;
				}
				if (targetToSource && !converter->CanConvertBack)
				{
					if (outError) *outError = L"MultiBinding Converter "
						+ converter->Name + L" 不支持 ConvertBack。";
					return false;
				}
			}
		}
		if (targetToSource && converterName.empty())
		{
			if (outError) *outError = L"可回写的 MultiBinding 必须声明 Converter。";
			return false;
		}
		for (size_t index = 0; index < binding.ChildBindings.size(); ++index)
		{
			auto child = binding.ChildBindings[index];
			if (child.IsMultiBinding())
			{
				if (outError) *outError = L"MultiBinding 不支持嵌套 MultiBinding。";
				return false;
			}
			TargetMetadata slot{
				L"MultiBinding slot", child.StringFormat
					? BindingValueKind::String : BindingValueKind::Empty,
				true, true, true, DesignerDataObjectKind::Opaque };
			if (child.Mode == BindingMode::Default)
				child.Mode = effectiveMode;
			if (child.UpdateMode == DataSourceUpdateMode::Default)
				child.UpdateMode = effectiveUpdateMode;
			std::wstring childError;
			if (!ValidateTarget(slot, child, &childError, sourceSchema))
			{
				if (outError) *outError = L"MultiBinding 第 "
					+ std::to_wstring(index + 1) + L" 个源无效：" + childError;
				return false;
			}
		}
		if (outError) outError->clear();
		return true;
	}
	if (!binding.ElementName.empty()
		&& binding.RelativeSource != DesignerBindingRelativeSource::None)
	{
		if (outError) *outError = L"ElementName 与 RelativeSource 不能同时使用。";
		return false;
	}
	if (binding.RelativeSource == DesignerBindingRelativeSource::FindAncestor)
	{
		if (Trim(binding.AncestorType).empty() || binding.AncestorLevel < 1)
		{
			if (outError) *outError = L"FindAncestor 需要有效的 AncestorType 和 AncestorLevel。";
			return false;
		}
		if (binding.AncestorTypeNamespace.empty())
		{
			UIClass type = UIClass::UI_Base;
			if (!DesignerStyleSheetUtils::TryParseUIClass(
				LocalTypeName(binding.AncestorType), type))
			{
				if (outError) *outError = L"FindAncestor 的 AncestorType 未解析为内置控件或 XAML 组件。";
				return false;
			}
		}
	}
	else if (!binding.AncestorType.empty()
		|| !binding.AncestorTypeNamespace.empty() || binding.AncestorLevel != 1)
	{
		if (outError) *outError = L"AncestorType/AncestorLevel 只能用于 FindAncestor。";
		return false;
	}
	if (!IsValidSourcePath(binding.SourceProperty))
	{
		if (outError) *outError = L"源路径无效：路径及每个点分段都不能为空。";
		return false;
	}
	if (binding.StringFormat
		&& (target.ValueKind != BindingValueKind::String
			|| !IsValidBindingStringFormat(*binding.StringFormat)))
	{
		if (outError) *outError = target.ValueKind != BindingValueKind::String
			? L"StringFormat 只能用于 String 目标属性。"
			: L"StringFormat 复合格式语法无效。";
		return false;
	}

	if (!IsModeStructurallyCompatible(target, effectiveMode))
	{
		if (outError) *outError = L"目标属性的读写能力不支持 "
			+ std::wstring(BindingModeName(effectiveMode)) + L"。";
		return false;
	}
	const bool targetToSource = effectiveMode == BindingMode::TwoWay
		|| effectiveMode == BindingMode::OneWayToSource;
	if (targetToSource
		&& effectiveUpdateMode != DataSourceUpdateMode::Never
		&& !target.CanObserve)
	{
		if (outError) *outError = L"该目标属性没有变更通知；请使用 Never 更新策略或改用单向模式。";
		return false;
	}

	const DesignerDataContextProperty* sourceProperty = nullptr;
	if (sourceSchema && !sourceSchema->empty())
	{
		std::vector<BindingPathStep> pathSteps;
		(void)TryParseBindingPropertyPath(binding.SourceProperty, pathSteps);
		std::wstring schemaPath;
		const BindingPathStep* firstIndexer = nullptr;
		for (const auto& step : pathSteps)
		{
			if (step.Kind == BindingPathStepKind::Indexer)
			{
				firstIndexer = &step;
				break;
			}
			if (!schemaPath.empty()) schemaPath += L'.';
			schemaPath += step.Value;
		}
		const bool indexedPath = firstIndexer != nullptr;
		sourceProperty = schemaPath.empty() ? nullptr
			: DesignerDataContextSchemaUtils::Find(*sourceSchema, schemaPath);
		if (!indexedPath && !sourceProperty)
		{
			if (outError) *outError = L"源路径未在 DataContext Schema 中声明："
				+ Trim(binding.SourceProperty);
			return false;
		}
		if (indexedPath && !schemaPath.empty() && !sourceProperty)
		{
			if (outError) *outError = L"索引器容器未在 DataContext Schema 中声明："
				+ schemaPath;
			return false;
		}

		const bool sourceToTarget = IsSourceToTarget(effectiveMode);
		const auto normalizedSourcePath = indexedPath ? schemaPath
			: DesignerDataContextSchemaUtils::NormalizePath(binding.SourceProperty);
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
					&& effectiveMode != BindingMode::OneTime
					&& !intermediate->CanObserve)
				{
					if (outError) *outError = L"DataContext 中间属性没有变更通知："
						+ intermediate->Path;
					return false;
				}
			}
			separator = normalizedSourcePath.find(L'.', separator + 1);
		}
		if (sourceProperty && sourceToTarget && !sourceProperty->CanRead)
		{
			if (outError) *outError = L"DataContext 源属性不可读：" + sourceProperty->Path;
			return false;
		}
		if (sourceProperty && sourceToTarget
			&& effectiveMode != BindingMode::OneTime
			&& !sourceProperty->CanObserve)
		{
			if (outError) *outError = L"DataContext 源属性没有变更通知；请使用 OneTime 或修改 Schema："
				+ sourceProperty->Path;
			return false;
		}
		if (sourceProperty && !indexedPath && targetToSource
			&& effectiveUpdateMode != DataSourceUpdateMode::Never
			&& !sourceProperty->CanWrite)
		{
			if (outError) *outError = L"DataContext 源属性不可写：" + sourceProperty->Path;
			return false;
		}
		if (indexedPath && sourceProperty)
		{
			const bool numericIndex = std::all_of(
				firstIndexer->Value.begin(), firstIndexer->Value.end(),
				[](wchar_t ch) { return std::iswdigit(ch) != 0; });
			if (sourceProperty->ObjectKind == DesignerDataObjectKind::BindingList
				&& !numericIndex)
			{
				if (outError) *outError = L"BindingList 索引器需要非负数字下标："
					+ sourceProperty->Path;
				return false;
			}
			if (sourceProperty->ObjectKind != DesignerDataObjectKind::BindingList
				&& sourceProperty->ObjectKind != DesignerDataObjectKind::BindingSource)
			{
				if (outError) *outError = L"索引器需要 BindingList 或 BindingSource 容器："
					+ sourceProperty->Path;
				return false;
			}
			if (targetToSource
				&& effectiveUpdateMode != DataSourceUpdateMode::Never
				&& sourceProperty->ObjectKind == DesignerDataObjectKind::BindingList
				&& pathSteps.back().Kind == BindingPathStepKind::Indexer)
			{
				if (outError) *outError = L"BindingList 数字索引器是只读的，不能作为 TwoWay/OneWayToSource 叶节点。";
				return false;
			}
			// The item/key result has its own runtime metadata; the container's
			// value kind must not be mistaken for the indexed leaf kind.
			sourceProperty = nullptr;
		}
		if (sourceProperty && target.ValueKind == BindingValueKind::Object
			&& target.ObjectKind != DesignerDataObjectKind::Opaque
			&& sourceProperty->ObjectKind != target.ObjectKind)
		{
			if (outError) *outError = L"DataContext 源属性的对象契约与目标属性不兼容：目标需要 "
				+ std::wstring(DesignerDataContextSchemaUtils::ObjectKindName(
					target.ObjectKind)) + L"。";
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
				&& !AreBindingKindsCompatible(
					converter->SourceKind, sourceProperty->ValueKind))
			{
				if (outError) *outError = L"Converter " + converter->Name
					+ L" 的源值类型与 DataContext Schema 不兼容。";
				return false;
			}
			if (target.ValueKind != BindingValueKind::Empty
				&& converter->TargetKind != BindingValueKind::Empty
				&& !AreBindingKindsCompatible(
					target.ValueKind, converter->TargetKind))
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
	const DependencyPropertyMetadata** outMetadata,
	std::wstring* outError,
	const DesignerDataContextSchema* sourceSchema)
{
	if (outMetadata) *outMetadata = nullptr;
	const auto* metadata = DependencyPropertyRegistry::Find(target, targetProperty);
	if (!metadata)
	{
		if (outError) *outError = targetProperty.empty()
			? L"请选择目标属性。"
			: L"目标属性不存在：" + targetProperty;
		return false;
	}
	const auto portable = ProjectTargetMetadata(*metadata);
	if (!ValidateTarget(portable, binding, outError, sourceSchema))
		return false;
	for (const auto& [name, value] : {
		std::pair{ L"FallbackValue", &binding.FallbackValue },
		std::pair{ L"TargetNullValue", &binding.TargetNullValue } })
	{
		if (!*value) continue;
		std::wstring literalError;
		if (!DesignerPropertyCatalog::ValidateStyleValue(
			target, targetProperty, **value, &literalError))
		{
			if (outError) *outError = std::wstring(name) + L" 无效："
				+ literalError;
			return false;
		}
	}
	if (binding.ConverterParameter)
	{
		std::optional<BindingValue> converted;
		std::wstring literalError;
		if (!TryConvertOptionalLiteral(
			binding.ConverterParameter, converted, &literalError))
		{
			if (outError) *outError = L"ConverterParameter 无效：" + literalError;
			return false;
		}
	}
	if (outMetadata) *outMetadata = metadata;
	return true;
}

std::wstring Describe(
	const std::wstring& targetProperty,
	const DesignerDataBinding& binding)
{
	std::wstring description = targetProperty + L" <- ";
	if (binding.IsMultiBinding())
	{
		description += L"MultiBinding(" + std::to_wstring(
			binding.ChildBindings.size()) + L")  ["
			+ BindingModeName(binding.Mode) + L", "
			+ UpdateModeName(binding.UpdateMode);
		if (!binding.Converter.empty())
			description += L", Converter=" + binding.Converter;
		if (binding.ConverterParameter)
			description += L", ConverterParameter='"
				+ binding.ConverterParameter->Text + L"'";
		if (binding.StringFormat)
			description += L", StringFormat='" + *binding.StringFormat + L"'";
		if (binding.FallbackValue)
			description += L", FallbackValue='" + binding.FallbackValue->Text + L"'";
		if (binding.TargetNullValue)
			description += L", TargetNullValue='" + binding.TargetNullValue->Text + L"'";
		return description + L"]";
	}
	if (!binding.ElementName.empty())
		description += L"ElementName=" + binding.ElementName + L".";
	else if (binding.RelativeSource == DesignerBindingRelativeSource::Self)
		description += L"RelativeSource=Self.";
	else if (binding.RelativeSource
		== DesignerBindingRelativeSource::TemplatedParent)
		description += L"RelativeSource=TemplatedParent.";
	else if (binding.RelativeSource
		== DesignerBindingRelativeSource::FindAncestor)
	{
		description += L"RelativeSource=FindAncestor("
			+ binding.AncestorType;
		if (binding.AncestorLevel != 1)
			description += L", Level=" + std::to_wstring(binding.AncestorLevel);
		description += L").";
	}
	description += binding.SourceProperty + L"  ["
		+ BindingModeName(binding.Mode) + L", "
		+ UpdateModeName(binding.UpdateMode);
	if (!binding.Converter.empty())
		description += L", Converter=" + binding.Converter;
	if (binding.ConverterParameter)
		description += L", ConverterParameter='"
			+ binding.ConverterParameter->Text + L"'";
	if (binding.StringFormat)
		description += L", StringFormat='" + *binding.StringFormat + L"'";
	if (binding.FallbackValue)
		description += L", FallbackValue='" + binding.FallbackValue->Text + L"'";
	if (binding.TargetNullValue)
		description += L", TargetNullValue='" + binding.TargetNullValue->Text + L"'";
	description += L"]";
	return description;
}
}
