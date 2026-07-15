#include "Binding.h"
#include "Control.h"

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <mutex>
#include <sstream>

namespace
{
	std::wstring Trim(std::wstring value)
	{
		auto isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](wchar_t ch) { return !isSpace(ch); }));
		value.erase(std::find_if(value.rbegin(), value.rend(), [&](wchar_t ch) { return !isSpace(ch); }).base(), value.end());
		return value;
	}

	std::wstring Lower(std::wstring value)
	{
		for (auto& ch : value)
			ch = (wchar_t)std::towlower(ch);
		return value;
	}

	bool IsSameProperty(const std::wstring& a, const std::wstring& b)
	{
		return Lower(a) == Lower(b);
	}

	bool TryParsePropertyPath(
		const std::wstring& value,
		std::vector<std::wstring>& segments)
	{
		segments.clear();
		size_t start = 0;
		while (start <= value.size())
		{
			const size_t separator = value.find(L'.', start);
			const size_t end = separator == std::wstring::npos ? value.size() : separator;
			auto segment = Trim(value.substr(start, end - start));
			if (segment.empty())
			{
				segments.clear();
				return false;
			}
			segments.push_back(std::move(segment));
			if (separator == std::wstring::npos) break;
			start = separator + 1;
		}
		return !segments.empty();
	}

	bool TryParseBool(const std::wstring& value, bool& out)
	{
		auto text = Lower(Trim(value));
		if (text == L"true" || text == L"1" || text == L"yes" || text == L"on")
		{
			out = true;
			return true;
		}
		if (text == L"false" || text == L"0" || text == L"no" || text == L"off" || text.empty())
		{
			out = false;
			return true;
		}
		return false;
	}

	bool TryParseInt64(const std::wstring& value, long long& out)
	{
		auto text = Trim(value);
		if (text.empty()) return false;
		errno = 0;
		wchar_t* end = nullptr;
		long long parsed = std::wcstoll(text.c_str(), &end, 10);
		if (!end || *end != L'\0' || errno == ERANGE) return false;
		out = parsed;
		return true;
	}

	bool TryParseDouble(const std::wstring& value, double& out)
	{
		auto text = Trim(value);
		if (text.empty()) return false;
		errno = 0;
		wchar_t* end = nullptr;
		double parsed = std::wcstod(text.c_str(), &end);
		if (!end || *end != L'\0' || errno == ERANGE || !std::isfinite(parsed))
			return false;
		out = parsed;
		return true;
	}

	std::wstring NumberToString(auto value)
	{
		std::wostringstream oss;
		oss << value;
		return oss.str();
	}

	bool BindingValuesEqual(const BindingValue& a, const BindingValue& b)
	{
		if (a.Kind() != b.Kind())
			return false;

		switch (a.Kind())
		{
		case BindingValueKind::Empty:
			return true;
		case BindingValueKind::Bool:
		{
			bool av = false, bv = false;
			return a.TryGetBool(av) && b.TryGetBool(bv) && av == bv;
		}
		case BindingValueKind::Int:
		{
			int av = 0, bv = 0;
			return a.TryGetInt(av) && b.TryGetInt(bv) && av == bv;
		}
		case BindingValueKind::Int64:
		{
			long long av = 0, bv = 0;
			return a.TryGetInt64(av) && b.TryGetInt64(bv) && av == bv;
		}
		case BindingValueKind::Float:
		{
			float av = 0, bv = 0;
			return a.TryGetFloat(av) && b.TryGetFloat(bv) && av == bv;
		}
		case BindingValueKind::Double:
		{
			double av = 0, bv = 0;
			return a.TryGetDouble(av) && b.TryGetDouble(bv) && av == bv;
		}
		case BindingValueKind::String:
		{
			std::wstring av, bv;
			return a.TryGetString(av) && b.TryGetString(bv) && av == bv;
		}
		case BindingValueKind::Object:
			// Arbitrary values deliberately do not assume operator==. Property
			// owners can still suppress notifications before calling SetValue.
			return false;
		}
		return false;
	}

	std::type_index BindingValueTypeForKind(BindingValueKind kind)
	{
		switch (kind)
		{
		case BindingValueKind::Bool: return std::type_index(typeid(bool));
		case BindingValueKind::Int: return std::type_index(typeid(int));
		case BindingValueKind::Int64: return std::type_index(typeid(long long));
		case BindingValueKind::Float: return std::type_index(typeid(float));
		case BindingValueKind::Double: return std::type_index(typeid(double));
		case BindingValueKind::String: return std::type_index(typeid(std::wstring));
		case BindingValueKind::Object:
		case BindingValueKind::Empty:
		default: return std::type_index(typeid(void));
		}
	}

	bool IsSourceToTargetMode(BindingMode mode)
	{
		return mode == BindingMode::OneWay || mode == BindingMode::TwoWay || mode == BindingMode::OneTime;
	}

	bool IsTargetToSourceMode(BindingMode mode)
	{
		return mode == BindingMode::TwoWay || mode == BindingMode::OneWayToSource;
	}

	std::vector<BindingValidationIssue> NormalizeValidationIssues(
		std::vector<BindingValidationIssue> issues)
	{
		std::vector<BindingValidationIssue> normalized;
		normalized.reserve(issues.size());
		for (auto& issue : issues)
		{
			issue.Message = Trim(std::move(issue.Message));
			issue.Code = Trim(std::move(issue.Code));
			if (issue.Message.empty()) continue;
			if (std::find(normalized.begin(), normalized.end(), issue) == normalized.end())
				normalized.push_back(std::move(issue));
		}
		return normalized;
	}

	bool ContainsValidationError(
		const std::vector<BindingValidationIssue>& issues) noexcept
	{
		return std::any_of(issues.begin(), issues.end(), [](const auto& issue)
		{
			return issue.Severity == BindingValidationSeverity::Error;
		});
	}

	struct ConverterRegistryEntry
	{
		BindingValueConverterMetadata Metadata;
		BindingValueConverterRegistry::Factory Factory;
	};

	const std::vector<ConverterRegistryEntry>& BuiltInBindingConverters()
	{
		static const std::vector<ConverterRegistryEntry> entries = {
			{
				{ L"BooleanNegation", BindingValueKind::Bool, BindingValueKind::Bool, true },
				[]
				{
					auto negate = [](const BindingValue& value, BindingValue& out)
					{
						bool booleanValue = false;
						if (!value.TryGetBool(booleanValue)) return false;
						out = BindingValue(!booleanValue);
						return true;
					};
					return std::make_shared<DelegateBindingValueConverter>(negate, negate);
				}
			},
			{
				{ L"StringIsNotEmpty", BindingValueKind::String, BindingValueKind::Bool, false },
				[]
				{
					return std::make_shared<DelegateBindingValueConverter>(
						[](const BindingValue& value, BindingValue& out)
						{
							std::wstring text;
							if (!value.TryGetString(text)) return false;
							out = BindingValue(!text.empty());
							return true;
						});
				}
			},
			{
				{ L"StringTrim", BindingValueKind::String, BindingValueKind::String, true },
				[]
				{
					auto trim = [](const BindingValue& value, BindingValue& out)
					{
						std::wstring text;
						if (!value.TryGetString(text)) return false;
						out = BindingValue(Trim(std::move(text)));
						return true;
					};
					return std::make_shared<DelegateBindingValueConverter>(trim, trim);
				}
			}
		};
		return entries;
	}

	std::vector<ConverterRegistryEntry>& RegisteredBindingConverters()
	{
		static std::vector<ConverterRegistryEntry> entries;
		return entries;
	}

	std::mutex& BindingConverterMutex()
	{
		static std::mutex mutex;
		return mutex;
	}

	std::vector<std::unique_ptr<BindingPropertyMetadata>>& RegisteredBindingProperties()
	{
		static std::vector<std::unique_ptr<BindingPropertyMetadata>> properties;
		return properties;
	}

	std::mutex& BindingPropertyMutex()
	{
		static std::mutex mutex;
		return mutex;
	}
}

const wchar_t* BindingErrorMessage(BindingError error) noexcept
{
	switch (error)
	{
	case BindingError::None: return L"No binding error.";
	case BindingError::InvalidTarget: return L"The binding target is null.";
	case BindingError::InvalidSource: return L"The binding source is null.";
	case BindingError::EmptyTargetProperty: return L"The target property name is empty.";
	case BindingError::EmptySourceProperty: return L"The source property name is empty.";
	case BindingError::InvalidSourcePropertyPath: return L"The source property path contains an empty segment.";
	case BindingError::DuplicateTargetProperty: return L"The target property already has a binding.";
	case BindingError::TargetPropertyNotFound: return L"No binding metadata is registered for the target property.";
	case BindingError::TargetNotReadable: return L"The target property is not readable for this binding mode.";
	case BindingError::TargetNotWritable: return L"The target property is not writable for this binding mode.";
	case BindingError::TargetNotObservable: return L"The target property does not expose change notifications.";
	case BindingError::SourceUnavailable: return L"The binding source is no longer available.";
	case BindingError::SourceNotReadable: return L"The source property is not readable for this binding mode.";
	case BindingError::SourceNotWritable: return L"The source property is not writable for this binding mode.";
	case BindingError::SourceNotObservable: return L"The source property does not expose change notifications.";
	case BindingError::SourcePathUnresolved: return L"An intermediate binding source in the property path is unavailable.";
	case BindingError::SourceReadFailed: return L"The source property could not be read.";
	case BindingError::TargetReadFailed: return L"The target property could not be read.";
	case BindingError::TargetConversionFailed: return L"The source value could not be converted to the target property type.";
	case BindingError::TargetWriteFailed: return L"The target property could not be written.";
	case BindingError::SourceConversionFailed: return L"The target value could not be converted to the source property type.";
	case BindingError::SourceWriteFailed: return L"The source property could not be written.";
	}
	return L"Unknown binding error.";
}

const wchar_t* ControlPropertyValueSourceName(
	ControlPropertyValueSource source) noexcept
{
	switch (source)
	{
	case ControlPropertyValueSource::Default: return L"Default";
	case ControlPropertyValueSource::Theme: return L"Theme";
	case ControlPropertyValueSource::Style: return L"Style";
	case ControlPropertyValueSource::Binding: return L"Binding";
	case ControlPropertyValueSource::Local: return L"Local";
	}
	return L"Unknown";
}

const wchar_t* BindingValidationSeverityName(
	BindingValidationSeverity severity) noexcept
{
	switch (severity)
	{
	case BindingValidationSeverity::Info: return L"Info";
	case BindingValidationSeverity::Warning: return L"Warning";
	case BindingValidationSeverity::Error: return L"Error";
	}
	return L"Unknown";
}

BindingValue::BindingValue() : _value(std::monostate{}) {}
BindingValue::BindingValue(bool value) : _value(value) {}
BindingValue::BindingValue(int value) : _value(value) {}
BindingValue::BindingValue(long long value) : _value(value) {}
BindingValue::BindingValue(float value) : _value(value) {}
BindingValue::BindingValue(double value) : _value(value) {}
BindingValue::BindingValue(const wchar_t* value) : _value(std::wstring(value ? value : L"")) {}
BindingValue::BindingValue(const std::wstring& value) : _value(value) {}
BindingValue::BindingValue(std::wstring&& value) : _value(std::move(value)) {}

BindingValueKind BindingValue::Kind() const
{
	switch (_value.index())
	{
	case 1: return BindingValueKind::Bool;
	case 2: return BindingValueKind::Int;
	case 3: return BindingValueKind::Int64;
	case 4: return BindingValueKind::Float;
	case 5: return BindingValueKind::Double;
	case 6: return BindingValueKind::String;
	case 7: return BindingValueKind::Object;
	default: return BindingValueKind::Empty;
	}
}

const std::type_info& BindingValue::Type() const noexcept
{
	switch (Kind())
	{
	case BindingValueKind::Bool: return typeid(bool);
	case BindingValueKind::Int: return typeid(int);
	case BindingValueKind::Int64: return typeid(long long);
	case BindingValueKind::Float: return typeid(float);
	case BindingValueKind::Double: return typeid(double);
	case BindingValueKind::String: return typeid(std::wstring);
	case BindingValueKind::Object: return std::get<std::any>(_value).type();
	case BindingValueKind::Empty:
	default: return typeid(void);
	}
}

bool BindingValue::Empty() const
{
	return Kind() == BindingValueKind::Empty;
}

std::wstring BindingValue::ToString() const
{
	std::wstring result;
	if (TryGetString(result))
		return result;
	return L"";
}

bool BindingValue::TryGetBool(bool& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Bool:
		out = std::get<bool>(_value);
		return true;
	case BindingValueKind::Int:
		out = std::get<int>(_value) != 0;
		return true;
	case BindingValueKind::Int64:
		out = std::get<long long>(_value) != 0;
		return true;
	case BindingValueKind::Float:
		out = std::get<float>(_value) != 0.0f;
		return true;
	case BindingValueKind::Double:
		out = std::get<double>(_value) != 0.0;
		return true;
	case BindingValueKind::String:
		return TryParseBool(std::get<std::wstring>(_value), out);
	default:
		return false;
	}
}

bool BindingValue::TryGetInt(int& out) const
{
	long long value = 0;
	if (!TryGetInt64(value)) return false;
	if (value < static_cast<long long>((std::numeric_limits<int>::min)())
		|| value > static_cast<long long>((std::numeric_limits<int>::max)()))
		return false;
	out = (int)value;
	return true;
}

bool BindingValue::TryGetInt64(long long& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Bool:
		out = std::get<bool>(_value) ? 1 : 0;
		return true;
	case BindingValueKind::Int:
		out = std::get<int>(_value);
		return true;
	case BindingValueKind::Int64:
		out = std::get<long long>(_value);
		return true;
	case BindingValueKind::Float:
	{
		const double value = static_cast<double>(std::get<float>(_value));
		if (!std::isfinite(value)
			|| value < -9223372036854775808.0
			|| value >= 9223372036854775808.0)
			return false;
		out = static_cast<long long>(value);
		return true;
	}
	case BindingValueKind::Double:
	{
		const double value = std::get<double>(_value);
		if (!std::isfinite(value)
			|| value < -9223372036854775808.0
			|| value >= 9223372036854775808.0)
			return false;
		out = static_cast<long long>(value);
		return true;
	}
	case BindingValueKind::String:
		return TryParseInt64(std::get<std::wstring>(_value), out);
	default:
		return false;
	}
}

bool BindingValue::TryGetFloat(float& out) const
{
	double value = 0.0;
	if (!TryGetDouble(value)) return false;
	if (std::isfinite(value)
		&& (value < static_cast<double>(-(std::numeric_limits<float>::max)())
			|| value > static_cast<double>((std::numeric_limits<float>::max)())))
		return false;
	out = (float)value;
	return true;
}

bool BindingValue::TryGetDouble(double& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Bool:
		out = std::get<bool>(_value) ? 1.0 : 0.0;
		return true;
	case BindingValueKind::Int:
		out = (double)std::get<int>(_value);
		return true;
	case BindingValueKind::Int64:
		out = (double)std::get<long long>(_value);
		return true;
	case BindingValueKind::Float:
		out = (double)std::get<float>(_value);
		return true;
	case BindingValueKind::Double:
		out = std::get<double>(_value);
		return true;
	case BindingValueKind::String:
		return TryParseDouble(std::get<std::wstring>(_value), out);
	default:
		return false;
	}
}

bool BindingValue::TryGetString(std::wstring& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Empty:
		out = L"";
		return true;
	case BindingValueKind::Bool:
		out = std::get<bool>(_value) ? L"True" : L"False";
		return true;
	case BindingValueKind::Int:
		out = NumberToString(std::get<int>(_value));
		return true;
	case BindingValueKind::Int64:
		out = NumberToString(std::get<long long>(_value));
		return true;
	case BindingValueKind::Float:
		out = NumberToString(std::get<float>(_value));
		return true;
	case BindingValueKind::Double:
		out = NumberToString(std::get<double>(_value));
		return true;
	case BindingValueKind::String:
		out = std::get<std::wstring>(_value);
		return true;
	case BindingValueKind::Object:
		return false;
	}
	return false;
}

bool TryConvertBindingValue(const BindingValue& value, BindingValueKind targetKind, BindingValue& out)
{
	switch (targetKind)
	{
	case BindingValueKind::Empty:
		out = BindingValue();
		return true;
	case BindingValueKind::Bool:
	{
		bool result = false;
		if (!value.TryGetBool(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Int:
	{
		int result = 0;
		if (!value.TryGetInt(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Int64:
	{
		long long result = 0;
		if (!value.TryGetInt64(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Float:
	{
		float result = 0.0f;
		if (!value.TryGetFloat(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::Double:
	{
		double result = 0.0;
		if (!value.TryGetDouble(result)) return false;
		out = BindingValue(result);
		return true;
	}
	case BindingValueKind::String:
	{
		std::wstring result;
		if (!value.TryGetString(result)) return false;
		out = BindingValue(std::move(result));
		return true;
	}
	case BindingValueKind::Object:
		if (value.Kind() != BindingValueKind::Object) return false;
		out = value;
		return true;
	}
	return false;
}

bool TryConvertBindingValue(
	const BindingValue& value,
	const BindingValue& targetValue,
	BindingValue& out)
{
	if (targetValue.Kind() == BindingValueKind::Empty)
	{
		out = value;
		return true;
	}

	if (targetValue.Kind() == BindingValueKind::Object)
	{
		if (value.Kind() != BindingValueKind::Object
			|| std::type_index(value.Type()) != std::type_index(targetValue.Type()))
			return false;
		out = value;
		return true;
	}

	return TryConvertBindingValue(value, targetValue.Kind(), out);
}

DelegateBindingValueConverter::DelegateBindingValueConverter(
	Function convert,
	Function convertBack)
	: _convert(std::move(convert)), _convertBack(std::move(convertBack))
{
}

bool DelegateBindingValueConverter::Convert(
	const BindingValue& value,
	BindingValue& out) const
{
	return _convert && _convert(value, out);
}

bool DelegateBindingValueConverter::ConvertBack(
	const BindingValue& value,
	BindingValue& out) const
{
	return _convertBack && _convertBack(value, out);
}

bool BindingValueConverterRegistry::Register(
	BindingValueConverterMetadata metadata,
	Factory factory,
	bool replaceExisting)
{
	metadata.Name = Trim(std::move(metadata.Name));
	if (metadata.Name.empty() || !factory) return false;

	std::lock_guard<std::mutex> lock(BindingConverterMutex());
	auto& registered = RegisteredBindingConverters();
	auto existing = std::find_if(registered.begin(), registered.end(),
		[&](const ConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, metadata.Name);
		});
	if (existing != registered.end())
	{
		if (!replaceExisting) return false;
		existing->Metadata = std::move(metadata);
		existing->Factory = std::move(factory);
		return true;
	}

	const auto& builtIns = BuiltInBindingConverters();
	const bool shadowsBuiltIn = std::any_of(builtIns.begin(), builtIns.end(),
		[&](const ConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, metadata.Name);
		});
	if (shadowsBuiltIn && !replaceExisting) return false;

	registered.push_back({ std::move(metadata), std::move(factory) });
	return true;
}

bool BindingValueConverterRegistry::Unregister(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return false;

	std::lock_guard<std::mutex> lock(BindingConverterMutex());
	auto& registered = RegisteredBindingConverters();
	auto existing = std::find_if(registered.begin(), registered.end(),
		[&](const ConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, normalized);
		});
	if (existing == registered.end()) return false;
	registered.erase(existing);
	return true;
}

std::optional<BindingValueConverterMetadata> BindingValueConverterRegistry::Find(
	const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return std::nullopt;

	{
		std::lock_guard<std::mutex> lock(BindingConverterMutex());
		for (const auto& entry : RegisteredBindingConverters())
		{
			if (IsSameProperty(entry.Metadata.Name, normalized))
				return entry.Metadata;
		}
	}
	for (const auto& entry : BuiltInBindingConverters())
	{
		if (IsSameProperty(entry.Metadata.Name, normalized))
			return entry.Metadata;
	}
	return std::nullopt;
}

std::vector<BindingValueConverterMetadata> BindingValueConverterRegistry::GetConverters()
{
	std::vector<BindingValueConverterMetadata> result;
	for (const auto& entry : BuiltInBindingConverters())
		result.push_back(entry.Metadata);

	{
		std::lock_guard<std::mutex> lock(BindingConverterMutex());
		for (const auto& entry : RegisteredBindingConverters())
		{
			auto existing = std::find_if(result.begin(), result.end(),
				[&](const BindingValueConverterMetadata& metadata)
				{
					return IsSameProperty(metadata.Name, entry.Metadata.Name);
				});
			if (existing == result.end()) result.push_back(entry.Metadata);
			else *existing = entry.Metadata;
		}
	}

	std::sort(result.begin(), result.end(),
		[](const auto& left, const auto& right)
		{
			return Lower(left.Name) < Lower(right.Name);
		});
	return result;
}

std::shared_ptr<const IBindingValueConverter> BindingValueConverterRegistry::Create(
	const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return {};

	Factory factory;
	{
		std::lock_guard<std::mutex> lock(BindingConverterMutex());
		for (const auto& entry : RegisteredBindingConverters())
		{
			if (IsSameProperty(entry.Metadata.Name, normalized))
			{
				factory = entry.Factory;
				break;
			}
		}
	}
	if (!factory)
	{
		for (const auto& entry : BuiltInBindingConverters())
		{
			if (IsSameProperty(entry.Metadata.Name, normalized))
			{
				factory = entry.Factory;
				break;
			}
		}
	}
	if (!factory) return {};
	try
	{
		return factory();
	}
	catch (...)
	{
		return {};
	}
}

BindingPropertyMetadata::BindingPropertyMetadata(
	std::wstring name,
	BindingValueKind valueKind,
	std::type_index valueType,
	std::type_index ownerType,
	Matcher matcher,
	ValueConverter valueConverter,
	Coercer coercer,
	Comparer comparer,
	Getter getter,
	Setter setter,
	Subscriber subscriber,
	Changed changed,
	BindingValue defaultValue,
	bool hasDefaultValue,
	ControlPropertyFlags flags,
	ControlPropertyDesignMetadata design)
	: _name(std::move(name)),
	  _valueKind(valueKind),
	  _valueType(valueType),
	  _ownerType(ownerType),
	  _matcher(std::move(matcher)),
	  _valueConverter(std::move(valueConverter)),
	  _coercer(std::move(coercer)),
	  _comparer(std::move(comparer)),
	  _getter(std::move(getter)),
	  _setter(std::move(setter)),
	  _subscriber(std::move(subscriber)),
	  _changed(std::move(changed)),
	  _defaultValue(std::move(defaultValue)),
	  _hasDefaultValue(hasDefaultValue),
	  _flags(flags),
	  _design(std::move(design))
{
}

bool BindingPropertyMetadata::IsDesignerBrowsable(Control& target) const
{
	if (!_design.Browsable || !Matches(target)) return false;
	return !_design.BrowsableWhen || _design.BrowsableWhen(target);
}

bool BindingPropertyMetadata::TryConvert(
	const BindingValue& value,
	BindingValue& out) const
{
	return _valueConverter && _valueConverter(value, out);
}

bool BindingPropertyMetadata::TryCoerce(
	Control& target,
	const BindingValue& value,
	BindingValue& out) const
{
	if (!Matches(target)) return false;
	if (_coercer) return _coercer(target, value, out);
	out = value;
	return true;
}

bool BindingPropertyMetadata::ValuesEqual(
	const BindingValue& left,
	const BindingValue& right) const
{
	return _comparer && _comparer(left, right);
}

bool BindingPropertyMetadata::TryGetDefaultValue(BindingValue& out) const
{
	if (!_hasDefaultValue) return false;
	out = _defaultValue;
	return true;
}

bool BindingPropertyMetadata::Matches(const Control& target) const
{
	return _matcher && _matcher(target);
}

bool BindingPropertyMetadata::TryGet(Control& target, BindingValue& out) const
{
	return _getter && Matches(target) && _getter(target, out);
}

bool BindingPropertyMetadata::TrySet(Control& target, const BindingValue& value) const
{
	if (!_setter || !Matches(target)) return false;
	BindingValue converted;
	if (!TryConvert(value, converted)) return false;
	BindingValue effective;
	if (!TryCoerce(target, converted, effective)) return false;

	BindingValue oldValue;
	const bool hadOldValue = _getter && _getter(target, oldValue);
	const auto changeVersion = target._propertyChangeVersion;
	if (!_setter(target, effective)) return false;
	if (target._propertyChangeVersion != changeVersion)
		return true;

	BindingValue newValue = effective;
	if (_getter) _getter(target, newValue);
	if (!hadOldValue || !ValuesEqual(oldValue, newValue))
		target.ApplyPropertyMetadataChange(*this, oldValue, newValue);
	return true;
}

void BindingPropertyMetadata::NotifyChanged(
	Control& target,
	const BindingValue& oldValue,
	const BindingValue& newValue) const
{
	if (_changed) _changed(target, oldValue, newValue);
}

EventConnection BindingPropertyMetadata::Subscribe(
	Control& target,
	ChangeHandler handler,
	DataSourceUpdateMode updateMode) const
{
	if (_subscriber && Matches(target) && handler)
		return _subscriber(target, std::move(handler), updateMode);
	return {};
}

const BindingPropertyMetadata* BindingPropertyRegistry::Register(BindingPropertyMetadata metadata)
{
	if (metadata.Name().empty()) return nullptr;
	std::scoped_lock lock(BindingPropertyMutex());
	auto& properties = RegisteredBindingProperties();
	for (const auto& existing : properties)
	{
		if (existing->OwnerType() == metadata.OwnerType()
			&& IsSameProperty(existing->Name(), metadata.Name()))
			return existing.get();
	}

	auto property = std::make_unique<BindingPropertyMetadata>(std::move(metadata));
	const auto* result = property.get();
	properties.push_back(std::move(property));
	return result;
}

const BindingPropertyMetadata* BindingPropertyRegistry::Find(
	Control& target,
	const std::wstring& propertyName)
{
	target.EnsureBindingPropertiesRegistered();
	std::scoped_lock lock(BindingPropertyMutex());
	auto& properties = RegisteredBindingProperties();
	for (auto it = properties.rbegin(); it != properties.rend(); ++it)
	{
		if (IsSameProperty((*it)->Name(), propertyName) && (*it)->Matches(target))
			return it->get();
	}
	return nullptr;
}

std::vector<const BindingPropertyMetadata*> BindingPropertyRegistry::GetProperties(Control& target)
{
	target.EnsureBindingPropertiesRegistered();
	std::scoped_lock lock(BindingPropertyMutex());
	std::vector<const BindingPropertyMetadata*> result;
	auto& properties = RegisteredBindingProperties();
	for (auto it = properties.rbegin(); it != properties.rend(); ++it)
	{
		const auto* property = it->get();
		if (!property->Matches(target))
			continue;
		const bool shadowed = std::any_of(
			result.begin(), result.end(),
			[property](const BindingPropertyMetadata* existing)
			{
				return IsSameProperty(existing->Name(), property->Name());
			});
		if (!shadowed)
			result.push_back(property);
	}
	std::sort(result.begin(), result.end(),
		[](const BindingPropertyMetadata* left, const BindingPropertyMetadata* right)
		{
			return Lower(left->Name()) < Lower(right->Name());
		});
	return result;
}

PropertyChangedEventArgs::PropertyChangedEventArgs(std::wstring propertyName)
	: PropertyName(std::move(propertyName))
{
}

struct PropertyChangedEvent::State final
{
	size_t NextToken = 1;
	std::vector<std::pair<size_t, Handler>> Handlers;
};

size_t PropertyChangedEvent::Add(Handler handler)
{
	if (!handler) return 0;
	if (!_state) _state = std::make_shared<State>();
	const size_t token = _state->NextToken++;
	_state->Handlers.push_back({ token, std::move(handler) });
	return token;
}

EventConnection PropertyChangedEvent::Subscribe(Handler handler)
{
	const size_t token = Add(std::move(handler));
	if (token == 0) return {};
	std::weak_ptr<State> weakState = _state;
	return EventConnection([weakState, token]()
	{
		if (auto state = weakState.lock())
		{
			state->Handlers.erase(
				std::remove_if(state->Handlers.begin(), state->Handlers.end(),
					[token](const auto& item) { return item.first == token; }),
				state->Handlers.end());
		}
	});
}

void PropertyChangedEvent::Remove(size_t token)
{
	if (!_state || token == 0) return;
	_state->Handlers.erase(
		std::remove_if(_state->Handlers.begin(), _state->Handlers.end(), [token](const auto& item) { return item.first == token; }),
		_state->Handlers.end());
}

void PropertyChangedEvent::Notify(const std::wstring& propertyName)
{
	if (!_state || _state->Handlers.empty()) return;
	auto snapshot = _state->Handlers;
	PropertyChangedEventArgs args(propertyName);
	for (auto& item : snapshot)
	{
		if (item.second)
			item.second(args);
	}
}

void PropertyChangedEvent::Clear()
{
	if (_state) _state->Handlers.clear();
}

size_t PropertyChangedEvent::Count() const noexcept
{
	return _state ? _state->Handlers.size() : 0;
}

BindingValidationChangedEventArgs::BindingValidationChangedEventArgs(
	std::wstring propertyName)
	: PropertyName(std::move(propertyName))
{
}

struct BindingValidationChangedEvent::State final
{
	size_t NextToken = 1;
	std::vector<std::pair<size_t, Handler>> Handlers;
};

size_t BindingValidationChangedEvent::Add(Handler handler)
{
	if (!handler) return 0;
	if (!_state) _state = std::make_shared<State>();
	const size_t token = _state->NextToken++;
	_state->Handlers.push_back({ token, std::move(handler) });
	return token;
}

EventConnection BindingValidationChangedEvent::Subscribe(Handler handler)
{
	const size_t token = Add(std::move(handler));
	if (token == 0) return {};
	std::weak_ptr<State> weakState = _state;
	return EventConnection([weakState, token]()
	{
		if (auto state = weakState.lock())
		{
			state->Handlers.erase(
				std::remove_if(state->Handlers.begin(), state->Handlers.end(),
					[token](const auto& item) { return item.first == token; }),
				state->Handlers.end());
		}
	});
}

void BindingValidationChangedEvent::Remove(size_t token)
{
	if (!_state || token == 0) return;
	_state->Handlers.erase(
		std::remove_if(_state->Handlers.begin(), _state->Handlers.end(),
			[token](const auto& item) { return item.first == token; }),
		_state->Handlers.end());
}

void BindingValidationChangedEvent::Notify(const std::wstring& propertyName)
{
	if (!_state || _state->Handlers.empty()) return;
	auto snapshot = _state->Handlers;
	BindingValidationChangedEventArgs args(propertyName);
	for (auto& item : snapshot)
	{
		if (item.second)
			item.second(args);
	}
}

void BindingValidationChangedEvent::Clear()
{
	if (_state) _state->Handlers.clear();
}

size_t BindingValidationChangedEvent::Count() const noexcept
{
	return _state ? _state->Handlers.size() : 0;
}

bool ObservableObject::TryGetValue(const std::wstring& propertyName, BindingValue& out) const
{
	const auto name = Trim(propertyName);
	auto metadata = _metadata.find(name);
	if (metadata != _metadata.end() && !metadata->second.CanRead)
		return false;
	auto it = _values.find(name);
	if (it == _values.end())
		return false;
	out = it->second;
	return true;
}

bool ObservableObject::TrySetValue(const std::wstring& propertyName, const BindingValue& value)
{
	const auto name = Trim(propertyName);
	auto metadata = _metadata.find(name);
	if (metadata != _metadata.end() && !metadata->second.CanWrite)
		return false;
	return SetCurrentValue(name, value, true);
}

bool ObservableObject::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	const auto it = _metadata.find(Trim(propertyName));
	if (it == _metadata.end()) return false;
	out = it->second;
	return true;
}

std::vector<BindingSourcePropertyMetadata> ObservableObject::GetProperties() const
{
	std::vector<BindingSourcePropertyMetadata> result;
	result.reserve(_metadata.size());
	for (const auto& [name, metadata] : _metadata)
	{
		(void)name;
		result.push_back(metadata);
	}
	std::sort(result.begin(), result.end(),
		[](const auto& left, const auto& right)
		{
			return Lower(left.Name) < Lower(right.Name);
		});
	return result;
}

std::vector<BindingValidationIssue> GetBindingValidationIssuesForPath(
	const IBindingSource& source,
	const std::wstring& sourcePropertyPath)
{
	std::vector<std::wstring> path;
	if (!TryParsePropertyPath(sourcePropertyPath, path)) return {};

	std::vector<BindingValidationIssue> result;
	auto append = [&result](std::vector<BindingValidationIssue> issues)
	{
		for (auto& issue : NormalizeValidationIssues(std::move(issues)))
		{
			if (std::find(result.begin(), result.end(), issue) == result.end())
				result.push_back(std::move(issue));
		}
	};

	const IBindingSource* current = &source;
	for (size_t index = 0; index < path.size(); ++index)
	{
		append(current->GetValidationIssues(L""));
		append(current->GetValidationIssues(path[index]));
		if (index + 1 == path.size()) break;

		BindingValue value;
		BindingSourceReference reference;
		if (!current->TryGetValue(path[index], value)
			|| !value.TryGet(reference)
			|| !reference)
			break;
		current = reference.Get();
	}
	return result;
}

std::vector<BindingValidationIssue> ObservableObject::GetValidationIssues(
	const std::wstring& propertyName) const
{
	const auto it = _validationIssues.find(Trim(propertyName));
	return it == _validationIssues.end()
		? std::vector<BindingValidationIssue>{}
		: it->second;
}

bool ObservableObject::HasValidationIssues() const noexcept
{
	return std::any_of(_validationIssues.begin(), _validationIssues.end(),
		[](const auto& item) { return !item.second.empty(); });
}

bool ObservableObject::HasValidationErrors() const noexcept
{
	return std::any_of(_validationIssues.begin(), _validationIssues.end(),
		[](const auto& item) { return ContainsValidationError(item.second); });
}

bool ObservableObject::HasValidationErrors(
	const std::wstring& propertyName) const
{
	const auto it = _validationIssues.find(Trim(propertyName));
	return it != _validationIssues.end() && ContainsValidationError(it->second);
}

bool ObservableObject::SetValidationIssues(
	const std::wstring& propertyName,
	std::vector<BindingValidationIssue> issues)
{
	const auto name = Trim(propertyName);
	auto normalized = NormalizeValidationIssues(std::move(issues));
	const auto existing = _validationIssues.find(name);
	if (normalized.empty())
	{
		if (existing == _validationIssues.end()) return false;
		_validationIssues.erase(existing);
		_validationChanged.Notify(name);
		return true;
	}
	if (existing != _validationIssues.end() && existing->second == normalized)
		return false;
	_validationIssues[name] = std::move(normalized);
	_validationChanged.Notify(name);
	return true;
}

bool ObservableObject::SetValidationError(
	const std::wstring& propertyName,
	std::wstring message,
	std::wstring code)
{
	if (Trim(message).empty()) return ClearValidationIssues(propertyName);
	return SetValidationIssues(propertyName,
		{ { std::move(message), BindingValidationSeverity::Error,
			std::move(code) } });
}

bool ObservableObject::ClearValidationIssues(
	const std::wstring& propertyName)
{
	const auto name = Trim(propertyName);
	const auto existing = _validationIssues.find(name);
	if (existing == _validationIssues.end()) return false;
	_validationIssues.erase(existing);
	_validationChanged.Notify(name);
	return true;
}

bool ObservableObject::ClearAllValidationIssues()
{
	if (_validationIssues.empty()) return false;
	_validationIssues.clear();
	_validationChanged.Notify(L"");
	return true;
}

bool ObservableObject::DefineProperty(
	BindingSourcePropertyMetadata metadata,
	const BindingValue& initialValue,
	bool replaceExisting)
{
	metadata.Name = Trim(std::move(metadata.Name));
	if (metadata.Name.empty()) return false;
	const auto existing = _metadata.find(metadata.Name);
	if (existing != _metadata.end() && !replaceExisting) return false;
	const bool existed = existing != _metadata.end();
	const auto oldValue = _values.find(metadata.Name);
	const bool hadValue = oldValue != _values.end();
	BindingValue previous;
	if (hadValue) previous = oldValue->second;

	BindingValue normalized;
	if (!NormalizeValue(metadata, initialValue, normalized)) return false;
	const bool changed = !hadValue || !BindingValuesEqual(previous, normalized);
	_metadata[metadata.Name] = metadata;
	_values[metadata.Name] = std::move(normalized);
	if (existed && changed && metadata.CanObserve)
		OnPropertyChanged(metadata.Name);
	return true;
}

bool ObservableObject::RemoveProperty(const std::wstring& propertyName)
{
	const auto normalized = Trim(propertyName);
	const auto metadata = _metadata.find(normalized);
	if (metadata == _metadata.end()) return false;
	const bool notify = metadata->second.CanObserve;
	const auto name = metadata->second.Name;
	_metadata.erase(metadata);
	_values.erase(normalized);
	const bool validationRemoved = _validationIssues.erase(normalized) != 0;
	if (validationRemoved) _validationChanged.Notify(name);
	if (notify) OnPropertyChanged(name);
	return true;
}

bool ObservableObject::NormalizeValue(
	BindingSourcePropertyMetadata& metadata,
	const BindingValue& value,
	BindingValue& out) const
{
	if (metadata.ValueKind != BindingValueKind::Empty
		&& metadata.ValueKind != BindingValueKind::Object)
	{
		metadata.ValueType = BindingValueTypeForKind(metadata.ValueKind);
	}
	if (metadata.ValueKind == BindingValueKind::Empty)
	{
		out = value;
		if (!value.Empty())
		{
			metadata.ValueKind = value.Kind();
			metadata.ValueType = std::type_index(value.Type());
		}
		return true;
	}

	if (value.Empty())
	{
		out = value;
		return true;
	}
	if (metadata.ValueKind == BindingValueKind::Object)
	{
		if (value.Kind() != BindingValueKind::Object) return false;
		const std::type_index valueType(value.Type());
		if (metadata.ValueType != std::type_index(typeid(void))
			&& metadata.ValueType != valueType)
			return false;
		metadata.ValueType = valueType;
		out = value;
		return true;
	}

	if (!TryConvertBindingValue(value, metadata.ValueKind, out)) return false;
	metadata.ValueType = std::type_index(out.Type());
	return true;
}

bool ObservableObject::SetCurrentValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	bool notify)
{
	const auto name = Trim(propertyName);
	if (name.empty()) return false;

	auto metadataIt = _metadata.find(name);
	if (metadataIt == _metadata.end())
	{
		BindingSourcePropertyMetadata metadata;
		metadata.Name = name;
		BindingValue normalized;
		if (!NormalizeValue(metadata, value, normalized)) return false;
		_metadata[name] = metadata;
		_values[name] = std::move(normalized);
		if (notify && metadata.CanObserve) OnPropertyChanged(name);
		return true;
	}

	BindingValue next;
	if (!NormalizeValue(metadataIt->second, value, next)) return false;
	auto valueIt = _values.find(name);
	if (valueIt != _values.end() && BindingValuesEqual(valueIt->second, next))
		return true;
	_values[name] = std::move(next);
	if (notify && metadataIt->second.CanObserve) OnPropertyChanged(name);
	return true;
}

void ObservableObject::OnPropertyChanged(const std::wstring& propertyName)
{
	_propertyChanged.Notify(propertyName);
}

Binding::Binding(Control* target,
	std::wstring targetProperty,
	IBindingSource* source,
	std::wstring sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter)
	: _target(target),
	_source(source),
	_targetProperty(std::move(targetProperty)),
	_sourceProperty(std::move(sourceProperty)),
	  _mode(mode),
	  _updateMode(updateMode),
	  _converter(std::move(converter)),
	  _state(std::make_shared<State>())
{
	_state->Owner = this;
	if (_source)
		_sourceLifetime = _source->BindingLifetime();
	Attach();
}

Binding::~Binding()
{
	if (_state)
		_state->Owner = nullptr;
	_targetConnection.Disconnect();
	if (_ownsTargetValue && _target && !_target->_isDestroying)
		_target->ClearBindingPropertyValue(_targetProperty, this);
	_ownsTargetValue = false;
	_sourceConnections.clear();
	_sourcePathOwners.clear();
	_sourceValidationConnections.clear();
	_validationPathConnections.clear();
	_validationPathOwners.clear();
}

bool Binding::HasValidationErrors() const noexcept
{
	return ContainsValidationError(ValidationIssues());
}

void Binding::Attach()
{
	if (!Validate())
		return;

	AttachSourceChangedHandlers();
	AttachTargetChangedHandlers();
	if (!_isValid)
		return;
	AttachValidationChangedHandlers();
	RefreshValidation();

	if (_mode == BindingMode::OneWayToSource)
		UpdateSource();
	else if (IsSourceToTargetMode(_mode))
		UpdateTarget();
}

bool Binding::Validate()
{
	_isValid = false;
	if (!_target) return Fail(BindingError::InvalidTarget);
	if (!_source) return Fail(BindingError::InvalidSource);
	if (_targetProperty.empty()) return Fail(BindingError::EmptyTargetProperty);
	if (_sourceProperty.empty()) return Fail(BindingError::EmptySourceProperty);
	if (!TryParsePropertyPath(_sourceProperty, _sourcePath))
		return Fail(BindingError::InvalidSourcePropertyPath);
	if (!IsSourceAlive()) return Fail(BindingError::SourceUnavailable);

	_targetMetadata = BindingPropertyRegistry::Find(*_target, _targetProperty);
	if (!_targetMetadata) return Fail(BindingError::TargetPropertyNotFound);
	if (IsSourceToTargetMode(_mode) && !_targetMetadata->CanWrite())
		return Fail(BindingError::TargetNotWritable);
	if (IsSourceToTargetMode(_mode)
		&& !_target->CanAcquireBindingPropertyValue(_targetProperty, this))
		return Fail(BindingError::DuplicateTargetProperty);
	if (IsTargetToSourceMode(_mode) && !_targetMetadata->CanRead())
		return Fail(BindingError::TargetNotReadable);
	if (IsTargetToSourceMode(_mode)
		&& _updateMode != DataSourceUpdateMode::Never
		&& !_targetMetadata->CanObserve())
		return Fail(BindingError::TargetNotObservable);
	if (!ValidateSourceMetadata()) return false;

	_isValid = true;
	_lastError = BindingError::None;
	return true;
}

bool Binding::ValidateSourceMetadata()
{
	if (!_source || _sourcePath.empty()) return true;
	IBindingSource* current = _source;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		const bool leaf = index + 1 == _sourcePath.size();
		BindingSourcePropertyMetadata metadata;
		if (current->TryGetPropertyMetadata(_sourcePath[index], metadata))
		{
			if ((!leaf || IsSourceToTargetMode(_mode)) && !metadata.CanRead)
				return Fail(BindingError::SourceNotReadable);
			if (IsSourceToTargetMode(_mode)
				&& _mode != BindingMode::OneTime
				&& !metadata.CanObserve)
				return Fail(BindingError::SourceNotObservable);
			if (leaf
				&& IsTargetToSourceMode(_mode)
				&& _updateMode != DataSourceUpdateMode::Never
				&& !metadata.CanWrite)
				return Fail(BindingError::SourceNotWritable);
		}
		if (leaf) break;

		BindingValue value;
		BindingSourceReference reference;
		if (!current->TryGetValue(_sourcePath[index], value)
			|| !value.TryGet(reference)
			|| !reference)
			break;
		current = reference.Get();
	}
	return true;
}

void Binding::AttachSourceChangedHandlers()
{
	_sourceConnections.clear();
	_sourcePathOwners.clear();
	if (!IsSourceAlive() || _mode == BindingMode::OneWayToSource || _mode == BindingMode::OneTime)
		return;

	IBindingSource* current = _source;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		const std::wstring expectedProperty = _sourcePath[index];
		std::weak_ptr<State> weakState = _state;
		auto connection = current->PropertyChanged().Subscribe(
			[weakState, expectedProperty](const PropertyChangedEventArgs& e)
		{
			if (!e.PropertyName.empty() && !IsSameProperty(e.PropertyName, expectedProperty))
				return;
			auto state = weakState.lock();
			if (!state || !state->Owner) return;
			state->Owner->OnSourcePathChanged();
		});
		if (connection.Connected())
			_sourceConnections.push_back(std::move(connection));

		if (index + 1 == _sourcePath.size())
			break;

		BindingValue value;
		BindingSourceReference reference;
		if (!current->TryGetValue(expectedProperty, value)
			|| !value.TryGet(reference)
			|| !reference)
			break;

		_sourcePathOwners.push_back(reference.Shared());
		current = reference.Get();
	}
}

void Binding::AttachValidationChangedHandlers()
{
	_sourceValidationConnections.clear();
	_validationPathConnections.clear();
	_validationPathOwners.clear();
	if (!IsSourceAlive() || _sourcePath.empty()) return;

	IBindingSource* current = _source;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		const std::wstring expectedProperty = _sourcePath[index];
		if (auto* validationChanged = current->ValidationChanged())
		{
			std::weak_ptr<State> weakState = _state;
			auto connection = validationChanged->Subscribe(
				[weakState, expectedProperty](
					const BindingValidationChangedEventArgs& e)
				{
					if (!e.PropertyName.empty()
						&& !IsSameProperty(e.PropertyName, expectedProperty))
						return;
					auto state = weakState.lock();
					if (!state || !state->Owner) return;
					state->Owner->RefreshValidation();
				});
			if (connection.Connected())
				_sourceValidationConnections.push_back(std::move(connection));
		}

		if (index + 1 == _sourcePath.size()) break;

		if (_mode == BindingMode::OneWayToSource || _mode == BindingMode::OneTime)
		{
			std::weak_ptr<State> weakState = _state;
			auto connection = current->PropertyChanged().Subscribe(
				[weakState, expectedProperty](const PropertyChangedEventArgs& e)
				{
					if (!e.PropertyName.empty()
						&& !IsSameProperty(e.PropertyName, expectedProperty))
						return;
					auto state = weakState.lock();
					if (!state || !state->Owner) return;
					state->Owner->OnValidationPathChanged();
				});
			if (connection.Connected())
				_validationPathConnections.push_back(std::move(connection));
		}

		BindingValue value;
		BindingSourceReference reference;
		if (!current->TryGetValue(expectedProperty, value)
			|| !value.TryGet(reference)
			|| !reference)
			break;
		_validationPathOwners.push_back(reference.Shared());
		current = reference.Get();
	}
}

void Binding::AttachTargetChangedHandlers()
{
	if (!_target || !_targetMetadata || !IsTargetToSourceMode(_mode)
		|| _updateMode == DataSourceUpdateMode::Never || !_targetMetadata->CanObserve())
		return;

	std::weak_ptr<State> weakState = _state;
	auto updateSource = [weakState]()
		{
			auto state = weakState.lock();
			if (!state || !state->Owner) return;
			state->Owner->OnTargetPropertyChanged();
		};

	_targetConnection = _targetMetadata->Subscribe(*_target, std::move(updateSource), _updateMode);
	if (!_targetConnection.Connected())
	{
		_isValid = false;
		_lastError = BindingError::TargetNotObservable;
	}
}

void Binding::OnSourcePathChanged()
{
	AttachSourceChangedHandlers();
	AttachValidationChangedHandlers();
	RefreshValidation();
	UpdateTarget();
}

void Binding::OnValidationPathChanged()
{
	AttachValidationChangedHandlers();
	RefreshValidation();
}

void Binding::RefreshValidation()
{
	std::vector<BindingValidationIssue> next;
	if (IsSourceAlive())
		next = GetBindingValidationIssuesForPath(*_source, _sourceProperty);
	if (next == _validationIssues) return;
	_validationIssues = std::move(next);
	_validationChanged.Notify(_sourceProperty);
}

void Binding::OnTargetPropertyChanged()
{
	UpdateSource();
}

bool Binding::UpdateTarget()
{
	if (!_isValid || !_target || !_targetMetadata || !_targetMetadata->CanWrite()
		|| !IsSourceToTargetMode(_mode) || _updatingSource)
		return false;
	if (!IsSourceAlive())
		return Fail(BindingError::SourceUnavailable);

	IBindingSource* sourceOwner = nullptr;
	std::vector<std::shared_ptr<IBindingSource>> keepAlive;
	if (!ResolveSourceOwner(sourceOwner, keepAlive))
		return Fail(BindingError::SourcePathUnresolved);

	BindingValue sourceValue;
	if (!sourceOwner->TryGetValue(_sourcePath.back(), sourceValue))
		return Fail(BindingError::SourceReadFailed);

	BindingValue value = sourceValue;
	if (_converter && !_converter->Convert(sourceValue, value))
		return Fail(BindingError::TargetConversionFailed);

	BindingValue converted;
	if (!_targetMetadata->TryConvert(value, converted))
		return Fail(BindingError::TargetConversionFailed);

	_updatingTarget = true;
	bool ok = _target->TrySetBindingPropertyValue(
		_targetProperty, converted, this);
	_updatingTarget = false;
	if (!ok) return Fail(BindingError::TargetWriteFailed);
	_ownsTargetValue = true;
	_lastError = BindingError::None;
	return true;
}

bool Binding::UpdateSource()
{
	if (!_isValid || !_target || !_targetMetadata || !_targetMetadata->CanRead()
		|| !IsTargetToSourceMode(_mode) || _updatingTarget)
		return false;
	if (!IsSourceAlive())
		return Fail(BindingError::SourceUnavailable);

	BindingValue value;
	if (!_targetMetadata->TryGet(*_target, value))
		return Fail(BindingError::TargetReadFailed);

	if (_converter)
	{
		BindingValue converted;
		if (!_converter->ConvertBack(value, converted))
			return Fail(BindingError::SourceConversionFailed);
		value = std::move(converted);
	}

	IBindingSource* sourceOwner = nullptr;
	std::vector<std::shared_ptr<IBindingSource>> keepAlive;
	if (!ResolveSourceOwner(sourceOwner, keepAlive))
		return Fail(BindingError::SourcePathUnresolved);

	BindingValue sourceValue;
	if (sourceOwner->TryGetValue(_sourcePath.back(), sourceValue))
	{
		BindingValue converted;
		if (!TryConvertBindingValue(value, sourceValue, converted))
			return Fail(BindingError::SourceConversionFailed);
		value = std::move(converted);
	}

	_updatingSource = true;
	bool ok = sourceOwner->TrySetValue(_sourcePath.back(), value);
	_updatingSource = false;
	if (!ok) return Fail(BindingError::SourceWriteFailed);
	_lastError = BindingError::None;
	return true;
}

bool Binding::ResolveSourceOwner(
	IBindingSource*& owner,
	std::vector<std::shared_ptr<IBindingSource>>& keepAlive) const
{
	owner = nullptr;
	keepAlive.clear();
	if (!IsSourceAlive() || _sourcePath.empty())
		return false;

	IBindingSource* current = _source;
	for (size_t index = 0; index + 1 < _sourcePath.size(); ++index)
	{
		BindingValue value;
		BindingSourceReference reference;
		if (!current->TryGetValue(_sourcePath[index], value)
			|| !value.TryGet(reference)
			|| !reference)
			return false;

		keepAlive.push_back(reference.Shared());
		current = reference.Get();
	}

	owner = current;
	return true;
}

BindingCollection::BindingCollection(Control* owner)
	: _owner(owner)
{
}

void BindingCollection::NotifyValidationChanged(
	const std::wstring& targetProperty)
{
	_validationChanged.Notify(targetProperty);
	if (_owner) _owner->OnBindingValidationChanged(targetProperty);
}

Binding* BindingCollection::Add(const std::wstring& targetProperty,
	IBindingSource* source,
	const std::wstring& sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter)
{
	if (!_owner)
	{
		_lastError = BindingError::InvalidTarget;
		return nullptr;
	}
	if (!source)
	{
		_lastError = BindingError::InvalidSource;
		return nullptr;
	}
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner,
		targetProperty,
		source,
		sourceProperty,
		mode,
		updateMode,
		std::move(converter));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	auto validationConnection = result->ValidationChanged().Subscribe(
		[this, targetProperty](const BindingValidationChangedEventArgs&)
		{
			NotifyValidationChanged(targetProperty);
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty);
	_lastError = BindingError::None;
	return result;
}

void BindingCollection::Clear()
{
	const bool hadValidation = HasValidationIssues();
	_validationConnections.clear();
	_items.clear();
	if (hadValidation) NotifyValidationChanged(L"");
	_lastError = BindingError::None;
}

size_t BindingCollection::Count() const
{
	return _items.size();
}

std::vector<BindingValidationResult> BindingCollection::GetValidationResults() const
{
	std::vector<BindingValidationResult> result;
	for (const auto& binding : _items)
	{
		if (!binding) continue;
		for (const auto& issue : binding->ValidationIssues())
		{
			result.push_back({ binding->TargetProperty(),
				binding->SourceProperty(), issue });
		}
	}
	return result;
}

bool BindingCollection::HasValidationIssues() const
{
	return std::any_of(_items.begin(), _items.end(), [](const auto& binding)
	{
		return binding && binding->HasValidationIssues();
	});
}

bool BindingCollection::HasValidationErrors() const
{
	return std::any_of(_items.begin(), _items.end(), [](const auto& binding)
	{
		return binding && binding->HasValidationErrors();
	});
}

Binding* BindingCollection::operator[](size_t index)
{
	return index < _items.size() ? _items[index].get() : nullptr;
}

const Binding* BindingCollection::operator[](size_t index) const
{
	return index < _items.size() ? _items[index].get() : nullptr;
}
