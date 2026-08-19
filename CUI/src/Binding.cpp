#include "Binding.h"
#include "BindingList.h"
#include "Control.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_map>
#if CUI_ENABLE_DYNAMIC_XAML
#include <unordered_set>
#endif

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
		return a == b;
	}

#if CUI_ENABLE_DYNAMIC_XAML
	bool TryParseBindingPropertyPathCore(
		const std::wstring& value,
		std::vector<BindingPathStep>& steps)
	{
		steps.clear();
		const auto text = Trim(value);
		if (text.empty()) return false;
		size_t position = 0;
		while (position < text.size())
		{
			while (position < text.size()
				&& std::iswspace(text[position])) ++position;
			if (position >= text.size()) break;

			if (text[position] == L'(')
			{
				const auto close = text.find(L')', position + 1);
				if (close == std::wstring::npos)
				{
					steps.clear();
					return false;
				}
				const auto token = Trim(text.substr(
					position + 1, close - position - 1));
				const auto separator = token.rfind(L'.');
				if (separator == std::wstring::npos)
				{
					steps.clear();
					return false;
				}
				auto owner = Trim(token.substr(0, separator));
				auto property = Trim(token.substr(separator + 1));
				auto validIdentifier = [](const std::wstring& candidate)
				{
					if (candidate.empty()) return false;
					return std::all_of(candidate.begin(), candidate.end(),
						[](wchar_t ch)
						{
							return std::iswalnum(ch) || ch == L'_'
								|| ch == L':';
						});
				};
				if (!validIdentifier(owner) || !validIdentifier(property))
				{
					steps.clear();
					return false;
				}
				steps.push_back({ BindingPathStepKind::Property,
					std::move(owner) + L"." + std::move(property) });
				position = close + 1;
			}
			else if (text[position] != L'[')
			{
				const size_t begin = position;
				while (position < text.size()
					&& text[position] != L'.'
					&& text[position] != L'['
					&& text[position] != L']') ++position;
				auto property = Trim(text.substr(begin, position - begin));
				if (property.empty())
				{
					steps.clear();
					return false;
				}
				steps.push_back({ BindingPathStepKind::Property,
					std::move(property) });
			}
			while (position < text.size())
			{
				while (position < text.size()
					&& std::iswspace(text[position])) ++position;
				if (position >= text.size() || text[position] != L'[') break;
				++position;
				while (position < text.size()
					&& std::iswspace(text[position])) ++position;
				if (position >= text.size())
				{
					steps.clear();
					return false;
				}

				std::wstring key;
				if (text[position] == L'\'' || text[position] == L'"')
				{
					const wchar_t quote = text[position++];
					bool closed = false;
					while (position < text.size())
					{
						const wchar_t ch = text[position++];
						if (ch != quote)
						{
							key.push_back(ch);
							continue;
						}
						if (position < text.size() && text[position] == quote)
						{
							key.push_back(quote);
							++position;
							continue;
						}
						closed = true;
						break;
					}
					if (!closed)
					{
						steps.clear();
						return false;
					}
					while (position < text.size()
						&& std::iswspace(text[position])) ++position;
				}
				else
				{
					const size_t begin = position;
					while (position < text.size()
						&& text[position] != L']')
					{
						if (text[position] == L'[')
						{
							steps.clear();
							return false;
						}
						++position;
					}
					key = Trim(text.substr(begin, position - begin));
				}
				if (key.empty() || position >= text.size()
					|| text[position] != L']')
				{
					steps.clear();
					return false;
				}
				++position;
				steps.push_back({ BindingPathStepKind::Indexer, std::move(key) });
			}

			while (position < text.size()
				&& std::iswspace(text[position])) ++position;
			if (position >= text.size()) break;
			if (text[position] != L'.')
			{
				steps.clear();
				return false;
			}
			++position;
			while (position < text.size()
				&& std::iswspace(text[position])) ++position;
			if (position >= text.size() || text[position] == L'.'
				|| text[position] == L'[' || text[position] == L']')
			{
				steps.clear();
				return false;
			}
		}
		return !steps.empty();
	}
#endif

	bool TryParseBool(const std::wstring& value, bool& out)
	{
		auto text = Lower(Trim(value));
		if (text == L"true")
		{
			out = true;
			return true;
		}
		if (text == L"false")
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
		oss.imbue(std::locale::classic());
		oss << value;
		return oss.str();
	}

	bool IsNumericBindingKind(BindingValueKind kind) noexcept
	{
		return kind == BindingValueKind::Int
			|| kind == BindingValueKind::Int64
			|| kind == BindingValueKind::Float
			|| kind == BindingValueKind::Double;
	}

	bool TryParseFormatPrecision(
		const std::wstring& text,
		size_t start,
		int defaultValue,
		int& out) noexcept
	{
		if (start == text.size())
		{
			out = defaultValue;
			return true;
		}
		int value = 0;
		for (size_t index = start; index < text.size(); ++index)
		{
			if (!std::iswdigit(text[index])) return false;
			value = value * 10 + static_cast<int>(text[index] - L'0');
			if (value > 99) return false;
		}
		out = value;
		return true;
	}

	void AddInvariantGrouping(std::wstring& text)
	{
		const auto decimal = text.find(L'.');
		const size_t end = decimal == std::wstring::npos ? text.size() : decimal;
		const size_t begin = !text.empty() && (text.front() == L'-'
			|| text.front() == L'+') ? 1 : 0;
		if (end <= begin + 3) return;
		for (size_t position = end; position > begin + 3;)
		{
			position -= 3;
			text.insert(position, 1, L',');
		}
	}

	bool IsBindingFormatSpecSyntaxValid(const std::wstring& spec) noexcept
	{
		if (spec.empty()) return true;
		const wchar_t code = spec.front();
		if (std::iswalpha(code))
		{
			const auto normalized = static_cast<wchar_t>(std::towupper(code));
			if (normalized != L'C' && normalized != L'D'
				&& normalized != L'E' && normalized != L'F'
				&& normalized != L'G' && normalized != L'N'
				&& normalized != L'P' && normalized != L'X') return false;
			int ignored = 0;
			return TryParseFormatPrecision(spec, 1,
				normalized == L'G' ? -1 : 2, ignored);
		}
		return std::all_of(spec.begin(), spec.end(), [](wchar_t ch)
		{
			return ch == L'0' || ch == L'#' || ch == L'.'
				|| ch == L',' || ch == L'%' || ch == L' ';
		});
	}

	bool TryFormatSingleBindingValue(
		const BindingValue& value,
		const std::wstring& spec,
		std::wstring& out)
	{
		if (spec.empty())
		{
			return value.TryGetString(out);
		}
		if (!IsBindingFormatSpecSyntaxValid(spec)
			|| !IsNumericBindingKind(value.Kind())) return false;

		const wchar_t rawCode = spec.front();
		const wchar_t code = std::iswalpha(rawCode)
			? static_cast<wchar_t>(std::towupper(rawCode)) : L'\0';
		if (code == L'D' || code == L'X')
		{
			if (value.Kind() != BindingValueKind::Int
				&& value.Kind() != BindingValueKind::Int64) return false;
			long long numeric = 0;
			if (!value.TryGetInt64(numeric)) return false;
			int precision = 0;
			if (!TryParseFormatPrecision(spec, 1, 0, precision)) return false;
			std::wostringstream stream;
			stream.imbue(std::locale::classic());
			if (code == L'D')
			{
				const bool negative = numeric < 0;
				const auto magnitude = negative
					? static_cast<unsigned long long>(-(numeric + 1)) + 1
					: static_cast<unsigned long long>(numeric);
				if (negative) stream << L'-';
				stream << std::setfill(L'0') << std::setw(precision) << magnitude;
			}
			else
			{
				if (rawCode == L'X') stream << std::uppercase;
				stream << std::hex << std::setfill(L'0') << std::setw(precision)
					<< static_cast<unsigned long long>(numeric);
			}
			out = stream.str();
			return true;
		}

		double numeric = 0.0;
		if (!value.TryGetDouble(numeric) || !std::isfinite(numeric)) return false;
		if (code != L'\0')
		{
			int precision = 2;
			if (!TryParseFormatPrecision(spec, 1,
				code == L'G' ? -1 : 2, precision)) return false;
			if (code == L'P') numeric *= 100.0;
			std::wostringstream stream;
			stream.imbue(std::locale::classic());
			if (code == L'E')
			{
				if (rawCode == L'E') stream << std::uppercase;
				stream << std::scientific << std::setprecision(precision) << numeric;
			}
			else if (code == L'G')
			{
				stream << std::defaultfloat;
				if (precision >= 0) stream << std::setprecision(precision);
				stream << numeric;
			}
			else stream << std::fixed << std::setprecision(precision) << numeric;
			out = stream.str();
			if (code == L'N' || code == L'C') AddInvariantGrouping(out);
			if (code == L'C') out.insert(out.front() == L'-' ? 1 : 0, 1, L'$');
			if (code == L'P') out += L'%';
			return true;
		}

		const auto decimal = spec.find(L'.');
		const auto percent = spec.find(L'%') != std::wstring::npos;
		const size_t fractionStart = decimal == std::wstring::npos
			? spec.size() : decimal + 1;
		int maximumDecimals = 0;
		int minimumDecimals = 0;
		for (size_t index = fractionStart; index < spec.size(); ++index)
		{
			if (spec[index] == L'0' || spec[index] == L'#')
			{
				++maximumDecimals;
				if (spec[index] == L'0') ++minimumDecimals;
			}
		}
		if (percent) numeric *= 100.0;
		std::wostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::fixed << std::setprecision(maximumDecimals) << numeric;
		out = stream.str();
		if (maximumDecimals > minimumDecimals)
		{
			while (!out.empty() && out.back() == L'0'
				&& maximumDecimals > minimumDecimals)
			{
				out.pop_back();
				--maximumDecimals;
			}
			if (!out.empty() && out.back() == L'.') out.pop_back();
		}
		const auto integralPatternEnd = decimal == std::wstring::npos
			? spec.size() : decimal;
		const int minimumIntegerDigits = static_cast<int>(std::count(
			spec.begin(), spec.begin() + integralPatternEnd, L'0'));
		const size_t signOffset = !out.empty() && out.front() == L'-' ? 1 : 0;
		const size_t outputDecimal = out.find(L'.');
		const size_t integerDigits = (outputDecimal == std::wstring::npos
			? out.size() : outputDecimal) - signOffset;
		if (integerDigits < static_cast<size_t>(minimumIntegerDigits))
			out.insert(signOffset,
				static_cast<size_t>(minimumIntegerDigits) - integerDigits, L'0');
		if (spec.find(L',') != std::wstring::npos) AddInvariantGrouping(out);
		if (percent) out += L'%';
		return true;
	}

	bool ProcessBindingStringFormat(
		const std::vector<BindingValue>* values,
		size_t valueCount,
		const std::wstring& rawFormat,
		std::wstring* output) noexcept
	{
		try
		{
			std::wstring format = rawFormat;
			if (format.starts_with(L"{}")) format.erase(0, 2);
			std::wstring result;
			for (size_t position = 0; position < format.size();)
			{
				const wchar_t ch = format[position++];
				if (ch == L'}')
				{
					if (position < format.size() && format[position] == L'}')
					{
						result.push_back(L'}');
						++position;
						continue;
					}
					return false;
				}
				if (ch != L'{')
				{
					result.push_back(ch);
					continue;
				}
				if (position < format.size() && format[position] == L'{')
				{
					result.push_back(L'{');
					++position;
					continue;
				}

				while (position < format.size()
					&& std::iswspace(format[position])) ++position;
				if (position >= format.size()
					|| !std::iswdigit(format[position]))
					return false;
				size_t valueIndex = 0;
				while (position < format.size()
					&& std::iswdigit(format[position]))
				{
					const auto digit = static_cast<size_t>(format[position++] - L'0');
					if (valueIndex > ((std::numeric_limits<size_t>::max)() - digit) / 10)
						return false;
					valueIndex = valueIndex * 10 + digit;
				}
				if (valueIndex >= valueCount) return false;
				while (position < format.size()
					&& std::iswspace(format[position])) ++position;

				int alignment = 0;
				if (position < format.size() && format[position] == L',')
				{
					++position;
					while (position < format.size()
						&& std::iswspace(format[position])) ++position;
					int sign = 1;
					if (position < format.size()
						&& (format[position] == L'-' || format[position] == L'+'))
						sign = format[position++] == L'-' ? -1 : 1;
					if (position >= format.size()
						|| !std::iswdigit(format[position])) return false;
					while (position < format.size()
						&& std::iswdigit(format[position]))
					{
						alignment = alignment * 10
							+ static_cast<int>(format[position++] - L'0');
						if (alignment > 100000) return false;
					}
					alignment *= sign;
					while (position < format.size()
						&& std::iswspace(format[position])) ++position;
				}

				std::wstring spec;
				if (position < format.size() && format[position] == L':')
				{
					const size_t begin = ++position;
					while (position < format.size() && format[position] != L'}')
					{
						if (format[position] == L'{') return false;
						++position;
					}
					spec = format.substr(begin, position - begin);
				}
				if (position >= format.size() || format[position++] != L'}'
					|| !IsBindingFormatSpecSyntaxValid(spec)) return false;

				std::wstring formatted;
				if (values && !TryFormatSingleBindingValue(
					(*values)[valueIndex], spec, formatted))
					return false;
				if (values)
				{
					const size_t width = static_cast<size_t>(alignment < 0
						? -static_cast<long long>(alignment) : alignment);
					if (formatted.size() < width)
					{
						const auto padding = width - formatted.size();
						if (alignment < 0) formatted.append(padding, L' ');
						else formatted.insert(0, padding, L' ');
					}
					result += formatted;
				}
			}
			if (output) *output = std::move(result);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool BindingValuesEqualCore(const BindingValue& a, const BindingValue& b)
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
		case BindingValueKind::NullableBool:
		{
			NullableBool av, bv;
			return a.TryGetNullableBool(av)
				&& b.TryGetNullableBool(bv) && av == bv;
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
			std::wstring_view av, bv;
			return a.TryGetStringView(av) && b.TryGetStringView(bv) && av == bv;
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
		case BindingValueKind::NullableBool:
			return std::type_index(typeid(NullableBool));
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

	bool BindingSourceMetadataNameMatches(
		const BindingSourcePropertyMetadata& metadata,
		const std::wstring& name) noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return metadata.Name.empty() || metadata.Name == name;
#else
		(void)metadata;
		(void)name;
		return true;
#endif
	}

}

bool IsValidBindingStringFormat(const std::wstring& format) noexcept
{
	return ProcessBindingStringFormat(nullptr, 1, format, nullptr);
}

bool TryFormatBindingValue(
	const BindingValue& value,
	const std::wstring& format,
	std::wstring& out)
{
	const std::vector<BindingValue> values{ value };
	return ProcessBindingStringFormat(&values, values.size(), format, &out);
}

bool IsValidMultiBindingStringFormat(
	const std::wstring& format,
	size_t valueCount) noexcept
{
	return valueCount != 0
		&& ProcessBindingStringFormat(nullptr, valueCount, format, nullptr);
}

bool TryFormatBindingValues(
	const std::vector<BindingValue>& values,
	const std::wstring& format,
	std::wstring& out)
{
	return !values.empty()
		&& ProcessBindingStringFormat(&values, values.size(), format, &out);
}

#if CUI_ENABLE_DYNAMIC_XAML
bool TryParseBindingPropertyPath(
	const std::wstring& value,
	std::vector<BindingPathStep>& steps)
{
	return TryParseBindingPropertyPathCore(value, steps);
}
#endif

CompiledSourceHandle cui::binding::MakeCompiledBindingSourcePropertyAdapter(
	IBindingSource& source,
	const CompiledBindingPathStep& property) noexcept
{
	if (property.Kind != CompiledBindingPathStepKind::Property
		|| property.EndpointResolver || !property.Property)
		return {};

	static const CompiledSourceOps operations{
		// Capabilities
		+[](const CompiledSourceHandle& endpoint)
		{
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			return descriptor
				? descriptor->Capabilities
				: CompiledBindingPathCapabilities::None;
		},
		// ValueKind
		+[](const CompiledSourceHandle& endpoint)
		{
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			return descriptor
				? descriptor->ValueKind : BindingValueKind::Empty;
		},
		// Lifetime
		+[](const CompiledSourceHandle& endpoint)
		{
			auto* bindingSource = static_cast<IBindingSource*>(endpoint.Object);
			return bindingSource
				? bindingSource->BindingLifetime() : std::weak_ptr<const void>{};
		},
		// Read
		+[](const CompiledSourceHandle& endpoint, BindingValue& out)
		{
			auto* bindingSource = static_cast<IBindingSource*>(endpoint.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			return bindingSource && descriptor
				&& HasCompiledBindingPathCapability(
					descriptor->Capabilities,
					CompiledBindingPathCapabilities::Read)
				&& bindingSource->TryGetValue(descriptor->Property, out);
		},
		// Write
		+[](const CompiledSourceHandle& endpoint, const BindingValue& value)
		{
			auto* bindingSource = static_cast<IBindingSource*>(endpoint.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			return bindingSource && descriptor
				&& HasCompiledBindingPathCapability(
					descriptor->Capabilities,
					CompiledBindingPathCapabilities::Write)
				&& bindingSource->TrySetValue(descriptor->Property, value);
		},
		// Subscribe
		+[](const CompiledSourceHandle& endpoint,
			DependencyPropertyChangeHandler handler)
		{
			auto* bindingSource = static_cast<IBindingSource*>(endpoint.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			if (!bindingSource || !descriptor || !handler
				|| !HasCompiledBindingPathCapability(
					descriptor->Capabilities,
					CompiledBindingPathCapabilities::Observe))
				return EventConnection{};
			const auto expectedProperty = descriptor->Property;
			return bindingSource->PropertyChanged().Subscribe(
				[expectedProperty, handler = std::move(handler)](
					const PropertyChangedEventArgs& eventArgs)
				{
					if (eventArgs.PropertyToken
						&& eventArgs.PropertyToken != expectedProperty) return;
					handler();
				});
		},
		// Validation
		+[](const CompiledSourceHandle& endpoint)
		{
			auto* bindingSource = static_cast<IBindingSource*>(endpoint.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			return bindingSource && descriptor
				? bindingSource->GetValidationIssues(descriptor->Property)
				: std::vector<BindingValidationIssue>{};
		},
		// SubscribeValidation
		+[](const CompiledSourceHandle& endpoint,
			DependencyPropertyChangeHandler handler)
		{
			auto* bindingSource = static_cast<IBindingSource*>(endpoint.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingPathStep*>(endpoint.Context);
			if (!bindingSource || !descriptor || !handler)
				return EventConnection{};
			auto* validationChanged = bindingSource->ValidationChanged();
			if (!validationChanged) return EventConnection{};
			const auto expectedProperty = descriptor->Property;
			return validationChanged->Subscribe(
				[expectedProperty, handler = std::move(handler)](
					const BindingValidationChangedEventArgs& eventArgs)
				{
					if (eventArgs.PropertyToken
						&& eventArgs.PropertyToken != expectedProperty) return;
					handler();
				});
		}
	};

	return { &source, &property, &operations };
}

namespace
{
#if CUI_ENABLE_DYNAMIC_XAML
	bool TryParseBindingListIndex(const std::wstring& value, size_t& out)
	{
		if (value.empty()) return false;
		unsigned long long parsed = 0;
		for (const wchar_t ch : value)
		{
			if (!std::iswdigit(ch)) return false;
			const auto digit = static_cast<unsigned long long>(ch - L'0');
			if (parsed > ((std::numeric_limits<unsigned long long>::max)() - digit) / 10)
				return false;
			parsed = parsed * 10 + digit;
		}
		if (parsed > static_cast<unsigned long long>((std::numeric_limits<size_t>::max)()))
			return false;
		out = static_cast<size_t>(parsed);
		return true;
	}
#endif

	struct BindingPathCursor final
	{
		IBindingSource* Source = nullptr;
		IBindingList* List = nullptr;
		std::vector<std::shared_ptr<IBindingSource>> SourceOwners;
		std::vector<std::shared_ptr<IBindingList>> ListOwners;
	};

	CompiledSourceHandle ResolveCompiledBindingPathEndpoint(
		const BindingPathCursor& cursor,
		const CompiledBindingPathStep& step) noexcept
	{
		if (!cursor.Source
			|| step.Kind != CompiledBindingPathStepKind::Property)
			return {};
		// An exact resolver is authoritative. In particular, an empty result
		// must not silently turn into token dispatch on the same source.
		if (step.EndpointResolver)
			return step.EndpointResolver(*cursor.Source);
		return cui::binding::MakeCompiledBindingSourcePropertyAdapter(
			*cursor.Source, step);
	}

	bool SetBindingPathCursor(
		const BindingValue& value,
		BindingPathCursor& cursor)
	{
		BindingSourceReference source;
		if (value.TryGet(source) && source)
		{
			cursor.SourceOwners.push_back(source.Shared());
			cursor.Source = source.Get();
			cursor.List = nullptr;
			return true;
		}
		BindingListReference list;
		if (value.TryGet(list) && list)
		{
			cursor.ListOwners.push_back(list.Shared());
			cursor.List = list.Get();
			cursor.Source = nullptr;
			return true;
		}
		return false;
	}

#if CUI_ENABLE_DYNAMIC_XAML
	bool TryReadBindingPathStep(
		const BindingPathCursor& cursor,
		const BindingPathStep& step,
		BindingValue& out)
	{
		if (cursor.Source)
			return cursor.Source->TryGetValue(step.Value, out);
		if (!cursor.List || step.Kind != BindingPathStepKind::Indexer)
			return false;
		size_t index = 0;
		BindingSourceReference item;
		if (!TryParseBindingListIndex(step.Value, index)
			|| !cursor.List->TryGetItem(index, item) || !item)
			return false;
		out = BindingValue(std::move(item));
		return true;
	}
#endif

	bool TryReadCompiledBindingPathStep(
		const BindingPathCursor& cursor,
		const CompiledBindingPathStep& step,
		BindingValue& out)
	{
		if (step.Kind == CompiledBindingPathStepKind::Property)
		{
			const auto endpoint = ResolveCompiledBindingPathEndpoint(cursor, step);
			return endpoint && endpoint.Ops->Read
				&& endpoint.Ops->Read(endpoint, out);
		}
		if (step.Kind != CompiledBindingPathStepKind::ListIndex || !cursor.List)
			return false;
		BindingSourceReference item;
		if (!cursor.List->TryGetItem(step.ListIndex, item) || !item)
			return false;
		out = BindingValue(std::move(item));
		return true;
	}

#if CUI_ENABLE_DYNAMIC_XAML
	bool ResolveBindingPathOwner(
		IBindingSource& source,
		const std::vector<BindingPathStep>& path,
		BindingPathCursor& cursor)
	{
		cursor = {};
		cursor.Source = &source;
		if (path.empty()) return false;
		for (size_t index = 0; index + 1 < path.size(); ++index)
		{
			BindingValue value;
			if (!TryReadBindingPathStep(cursor, path[index], value)
				|| !SetBindingPathCursor(value, cursor))
				return false;
		}
		return true;
	}
#endif

	bool ResolveCompiledBindingPathOwner(
		IBindingSource& source,
		CompiledBindingPathView path,
		BindingPathCursor& cursor)
	{
		cursor = {};
		cursor.Source = &source;
		if (path.Version != CompiledBindingPathVersion || path.Empty())
			return false;
		for (size_t index = 0; index + 1 < path.Steps.size(); ++index)
		{
			BindingValue value;
			if (!TryReadCompiledBindingPathStep(cursor, path.Steps[index], value)
				|| !SetBindingPathCursor(value, cursor))
				return false;
		}
		return true;
	}
}

#if CUI_ENABLE_DYNAMIC_XAML
bool TryGetBindingPathValue(
	const IBindingSource& source,
	const std::wstring& path,
	BindingValue& out)
{
	std::vector<BindingPathStep> steps;
	if (!TryParseBindingPropertyPath(path, steps)) return false;
	BindingPathCursor cursor;
	if (!ResolveBindingPathOwner(
		const_cast<IBindingSource&>(source), steps, cursor)) return false;
	return TryReadBindingPathStep(cursor, steps.back(), out);
}
#endif

bool TryGetBindingPathValue(
	const IBindingSource& source,
	CompiledBindingPathView path,
	BindingValue& out)
{
	BindingPathCursor cursor;
	if (!ResolveCompiledBindingPathOwner(
		const_cast<IBindingSource&>(source), path, cursor)) return false;
	return TryReadCompiledBindingPathStep(cursor, path.Steps.back(), out);
}

bool BindingValuesEqual(const BindingValue& left, const BindingValue& right)
{
	return BindingValuesEqualCore(left, right);
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
	case BindingError::InvalidSourcePropertyPath: return L"The source property path has invalid member or indexer syntax.";
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
	case BindingError::InvalidStringFormat: return L"Binding.StringFormat is invalid or the target property is not String.";
	case BindingError::StringFormatFailed: return L"The converted source value could not be formatted.";
	case BindingError::InvalidMultiBinding: return L"The MultiBinding configuration is invalid.";
	case BindingError::MultiBindingConverterFailed: return L"The multi-value converter could not convert the current values.";
	}
	return L"Unknown binding error.";
}

const wchar_t* DependencyPropertyValueSourceName(
	DependencyPropertyValueSource source) noexcept
{
	switch (source)
	{
	case DependencyPropertyValueSource::Default: return L"Default";
	case DependencyPropertyValueSource::Inherited: return L"Inherited";
	case DependencyPropertyValueSource::Theme: return L"Theme";
	case DependencyPropertyValueSource::Style: return L"Style";
	case DependencyPropertyValueSource::Template: return L"Template";
	case DependencyPropertyValueSource::VisualState: return L"VisualState";
	case DependencyPropertyValueSource::Local: return L"Local";
	case DependencyPropertyValueSource::Animation: return L"Animation";
	}
	return L"Unknown";
}

const wchar_t* DependencyPropertyExpressionKindName(
	DependencyPropertyExpressionKind kind) noexcept
{
	switch (kind)
	{
	case DependencyPropertyExpressionKind::None: return L"None";
	case DependencyPropertyExpressionKind::Binding: return L"Binding";
	case DependencyPropertyExpressionKind::DynamicResource: return L"DynamicResource";
	case DependencyPropertyExpressionKind::TemplateBinding: return L"TemplateBinding";
	case DependencyPropertyExpressionKind::Animation: return L"Animation";
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
BindingValue::BindingValue(NullableBool value) : _value(value) {}
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
	case 8: return BindingValueKind::NullableBool;
	default: return BindingValueKind::Empty;
	}
}

const std::type_info& BindingValue::Type() const noexcept
{
	switch (Kind())
	{
	case BindingValueKind::Bool: return typeid(bool);
	case BindingValueKind::NullableBool: return typeid(NullableBool);
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

std::any BindingValue::ToAny() const
{
	return std::visit([](const auto& value) -> std::any
	{
		using Value = std::remove_cvref_t<decltype(value)>;
		if constexpr (std::is_same_v<Value, std::monostate>)
			return {};
		else if constexpr (std::is_same_v<Value, std::any>)
			return value;
		else
			return std::any(value);
	}, _value);
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
	case BindingValueKind::NullableBool:
	{
		const auto value = std::get<NullableBool>(_value);
		if (!value.HasValue()) return false;
		out = value.GetValueOrDefault();
		return true;
	}
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

bool BindingValue::TryGetNullableBool(NullableBool& out) const
{
	switch (Kind())
	{
	case BindingValueKind::Empty:
		out = NullableBool{};
		return true;
	case BindingValueKind::NullableBool:
		out = std::get<NullableBool>(_value);
		return true;
	default:
	{
		bool value = false;
		if (!TryGetBool(value)) return false;
		out = NullableBool(value);
		return true;
	}
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
	case BindingValueKind::NullableBool:
	{
		const auto value = std::get<NullableBool>(_value);
		if (!value.HasValue()) return false;
		out = value.GetValueOrDefault() ? 1 : 0;
		return true;
	}
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
	case BindingValueKind::NullableBool:
	{
		const auto value = std::get<NullableBool>(_value);
		if (!value.HasValue()) return false;
		out = value.GetValueOrDefault() ? 1.0 : 0.0;
		return true;
	}
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
	case BindingValueKind::NullableBool:
	{
		const auto value = std::get<NullableBool>(_value);
		out = !value.HasValue()
			? L"{x:Null}"
			: value.GetValueOrDefault() ? L"True" : L"False";
		return true;
	}
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
	{
		BindingSourceReference source;
		if (!TryGet(source) || !source) return false;
		const auto* display = dynamic_cast<const IBindingSourceDisplayText*>(
			source.Get());
		return display && display->TryGetBindingDisplayText(out);
	}
	}
	return false;
}

bool BindingValue::TryGetStringView(std::wstring_view& out) const noexcept
{
	out = {};
	if (Kind() != BindingValueKind::String) return false;
	out = std::get<std::wstring>(_value);
	return true;
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
	case BindingValueKind::NullableBool:
	{
		NullableBool result;
		if (!value.TryGetNullableBool(result)) return false;
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
	ContextFunction convert,
	ContextFunction convertBack)
	: _contextConvert(std::move(convert)),
	  _contextConvertBack(std::move(convertBack))
{
}

bool DelegateBindingValueConverter::Convert(
	const BindingValue& value,
	const BindingValueConverterContext& context,
	BindingValue& out) const
{
	return _contextConvert && _contextConvert(value, context, out);
}

bool DelegateBindingValueConverter::ConvertBack(
	const BindingValue& value,
	const BindingValueConverterContext& context,
	BindingValue& out) const
{
	return _contextConvertBack && _contextConvertBack(value, context, out);
}

std::shared_ptr<const IBindingValueConverter>
GetBuiltInBindingValueConverter(BuiltInBindingValueConverter converter)
{
	switch (converter)
	{
	case BuiltInBindingValueConverter::BooleanNegation:
	{
		static const auto value = []
		{
			auto negate = [](const BindingValue& input,
				const BindingValueConverterContext&, BindingValue& out)
			{
				bool booleanValue = false;
				if (!input.TryGetBool(booleanValue)) return false;
				out = BindingValue(!booleanValue);
				return true;
			};
			return std::make_shared<const DelegateBindingValueConverter>(
				negate, negate);
		}();
		return value;
	}
	case BuiltInBindingValueConverter::StringIsNotEmpty:
	{
		static const auto value =
			std::make_shared<const DelegateBindingValueConverter>(
				[](const BindingValue& input,
					const BindingValueConverterContext&, BindingValue& out)
				{
					std::wstring text;
					if (!input.TryGetString(text)) return false;
					out = BindingValue(!text.empty());
					return true;
				});
		return value;
	}
	case BuiltInBindingValueConverter::StringTrim:
	{
		static const auto value = []
		{
			auto trim = [](const BindingValue& input,
				const BindingValueConverterContext&, BindingValue& out)
			{
				std::wstring text;
				if (!input.TryGetString(text)) return false;
				out = BindingValue(Trim(std::move(text)));
				return true;
			};
			return std::make_shared<const DelegateBindingValueConverter>(
				trim, trim);
		}();
		return value;
	}
	default:
		return {};
	}
}

DelegateMultiBindingValueConverter::DelegateMultiBindingValueConverter(
	ConvertFunction convert,
	ConvertBackFunction convertBack)
	: _convert(std::move(convert)), _convertBack(std::move(convertBack))
{
}

bool DelegateMultiBindingValueConverter::Convert(
	const std::vector<BindingValue>& values,
	const MultiBindingValueConverterContext& context,
	BindingValue& out) const
{
	return _convert && _convert(values, context, out);
}

bool DelegateMultiBindingValueConverter::ConvertBack(
	const BindingValue& value,
	size_t targetCount,
	const MultiBindingValueConverterContext& context,
	std::vector<BindingValue>& out) const
{
	return _convertBack
		&& _convertBack(value, targetCount, context, out);
}

DependencyProperty::DependencyProperty(
	std::wstring name,
	BindingValueKind valueKind,
	std::type_index valueType,
	std::type_index ownerType,
	std::size_t globalIndex,
	Validator validator,
	std::shared_ptr<const unsigned char> readOnlyAuthorization)
	:
#if CUI_ENABLE_DYNAMIC_XAML
	  _name(std::move(name)),
	  _bindingSourceToken(MakeBindingSourcePropertyToken(_name)),
#else
	  _bindingSourceToken(MakeBindingSourcePropertyToken(name)),
#endif
	  _valueKind(valueKind),
	  _valueType(valueType),
	  _ownerType(ownerType),
#if CUI_ENABLE_DYNAMIC_XAML
	  _globalIndex(globalIndex),
#endif
	  _validator(std::move(validator)),
	  _readOnlyAuthorization(std::move(readOnlyAuthorization))
{
#if !CUI_ENABLE_DYNAMIC_XAML
	(void)globalIndex;
#endif
}

bool DependencyProperty::IsValidValue(const BindingValue& value) const
{
	return !_validator || _validator(value);
}

bool DependencyProperty::Authorizes(
	const DependencyPropertyKey& key) const noexcept
{
	return key._property == this
		&& _readOnlyAuthorization
		&& key._authorization == _readOnlyAuthorization;
}

DependencyPropertyMetadata::DependencyPropertyMetadata(
	std::wstring name,
	BindingValueKind valueKind,
	std::type_index valueType,
	std::type_index ownerType,
	Matcher matcher,
	ValueConverter valueConverter,
	Validator validator,
	Coercer coercer,
	Comparer comparer,
	Getter getter,
	Setter setter,
	Subscriber subscriber,
	Changed changed,
	BindingValue defaultValue,
	bool hasDefaultValue,
	bool usesEffectiveValueStorage,
	DependencyPropertyFlags flags,
	bool isReadOnly,
	DataSourceUpdateMode defaultUpdateMode,
	std::wstring inheritanceKey
#if CUI_ENABLE_DESIGN_METADATA
	, DependencyPropertyDesignMetadata design
	#endif
	)
	:
#if CUI_ENABLE_DYNAMIC_XAML
	  _name(std::move(name)),
#endif
	  _valueKind(valueKind),
	  _valueType(valueType),
	  _ownerType(ownerType),
	  _matcher(std::move(matcher)),
	  _valueConverter(std::move(valueConverter)),
	  _validator(std::move(validator)),
	  _coercer(std::move(coercer)),
	  _comparer(std::move(comparer)),
	  _getter(std::move(getter)),
	  _setter(std::move(setter)),
	  _subscriber(std::move(subscriber)),
	  _changed(std::move(changed)),
	  _defaultValue(std::move(defaultValue)),
	  _hasDefaultValue(hasDefaultValue),
	  _usesEffectiveValueStorage(usesEffectiveValueStorage),
	  _flags(flags),
	  _isReadOnly(isReadOnly),
	  _defaultUpdateMode(defaultUpdateMode == DataSourceUpdateMode::Default
		  ? DataSourceUpdateMode::OnPropertyChanged
		  : defaultUpdateMode)
#if CUI_ENABLE_DYNAMIC_XAML
	  , _inheritanceKey(std::move(inheritanceKey))
#endif
#if CUI_ENABLE_DESIGN_METADATA
	  , _design(std::move(design))
#endif
{
#if !CUI_ENABLE_DYNAMIC_XAML
	(void)name;
	(void)inheritanceKey;
#endif
}

#if CUI_ENABLE_DYNAMIC_XAML
DependencyPropertyRegistration::DependencyPropertyRegistration(
	const DependencyProperty* property) noexcept
	: _property(property)
{
}

DependencyPropertyMetadataRegistration::
	DependencyPropertyMetadataRegistration(
		const DependencyPropertyMetadata* metadata) noexcept
	: _metadata(metadata)
{
}

DependencyPropertyKeyRegistration::DependencyPropertyKeyRegistration(
	DependencyPropertyKey key) noexcept
	: _key(std::move(key))
{
}
#else
DependencyPropertyRegistration::DependencyPropertyRegistration(
	DependencyPropertyMetadata metadata,
	BindingSourcePropertyToken token)
	: _property(
		{},
		metadata._valueKind,
		metadata._valueType,
		metadata._ownerType,
		static_cast<std::size_t>(token.Value),
		std::move(metadata._validator),
		{}),
	  _metadata(std::move(metadata))
{
	_property._bindingSourceToken = token;
	// Static properties never consume the process-wide registry counter. The
	// exact identity address is collision-free even when unrelated owners expose
	// the same BindingSourcePropertyToken/member name.
	_metadata.AttachProperty(_property);
	_property._standaloneMetadata = &_metadata;
	if (_metadata._hasDefaultValue
		&& !_property.IsValidValue(_metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");
}

DependencyPropertyMetadataRegistration::
	DependencyPropertyMetadataRegistration(
		const DependencyProperty& property,
		DependencyPropertyMetadata metadata,
		const DependencyPropertyMetadataRegistration* immediateBase)
	: _metadata(std::move(metadata)),
	  _immediateBase(immediateBase)
{
	if (_metadata._valueKind != property.ValueKind()
		|| _metadata._valueType != property.ValueType())
		throw std::invalid_argument(
			"Static metadata relation must preserve dependency-property value type");
	if (immediateBase && &immediateBase->Property() != &property)
		throw std::invalid_argument(
			"Immediate-base metadata must belong to the same dependency property");

	const DependencyPropertyMetadata* baseMetadata = immediateBase
		? &immediateBase->Metadata() : property._standaloneMetadata;
	if (!baseMetadata || &baseMetadata->Property() != &property)
		throw std::invalid_argument(
			"Static metadata relation requires exact default metadata");

	_metadata.AttachProperty(property);
	_metadata.MergeBaseMetadata(*baseMetadata);
	if (_metadata._hasDefaultValue
		&& !property.IsValidValue(_metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");

	auto* head = property._staticMetadataRelations.load(
		std::memory_order_relaxed);
	do
	{
		_next = head;
	}
	while (!property._staticMetadataRelations.compare_exchange_weak(
		head, this, std::memory_order_release, std::memory_order_relaxed));
}

bool DependencyPropertyMetadataRegistration::IsBasedOn(
	const DependencyPropertyMetadataRegistration& candidate) const noexcept
{
	for (auto* current = _immediateBase; current;
		current = current->_immediateBase)
	{
		if (current == &candidate) return true;
	}
	return false;
}

DependencyPropertyKeyRegistration::DependencyPropertyKeyRegistration(
	DependencyPropertyMetadata metadata,
	BindingSourcePropertyToken token)
	: _property(
		{},
		metadata._valueKind,
		metadata._valueType,
		metadata._ownerType,
		static_cast<std::size_t>(token.Value),
		std::move(metadata._validator),
		std::shared_ptr<const unsigned char>{
			std::shared_ptr<const unsigned char>{}, &_authorization }),
	  _metadata(std::move(metadata)),
	  _key(_property, std::shared_ptr<const unsigned char>{
		  std::shared_ptr<const unsigned char>{}, &_authorization })
{
	_property._bindingSourceToken = token;
	_metadata.AttachProperty(_property);
	_property._standaloneMetadata = &_metadata;
	if (_metadata._hasDefaultValue
		&& !_property.IsValidValue(_metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");
}
#endif

bool DependencyPropertyMetadata::HasSameInheritanceIdentity(
	const DependencyPropertyMetadata& other) const noexcept
{
#if CUI_ENABLE_DYNAMIC_XAML
	if (this == &other) return true;
	if (_property && _property == other._property) return true;
	return !_inheritanceKey.empty()
		&& _inheritanceKey == other._inheritanceKey
		&& _valueKind == other._valueKind
		&& _valueType == other._valueType;
#else
	return _property && _property == other._property;
#endif
}

BindingMode ResolveBindingMode(
	const DependencyPropertyMetadata& target,
	BindingMode requested) noexcept
{
	if (requested != BindingMode::Default) return requested;
	return HasDependencyPropertyFlag(
		target.Flags(), DependencyPropertyFlags::BindsTwoWayByDefault)
		? BindingMode::TwoWay
		: BindingMode::OneWay;
}

DataSourceUpdateMode ResolveDataSourceUpdateMode(
	const DependencyPropertyMetadata& target,
	DataSourceUpdateMode requested) noexcept
{
	return requested == DataSourceUpdateMode::Default
		? target.DefaultUpdateMode()
		: requested;
}

#if CUI_ENABLE_DESIGN_METADATA
bool DependencyPropertyMetadata::IsDesignerBrowsable(DependencyObject& target) const
{
	if (!_design.Browsable || !Matches(target)) return false;
	return !_design.BrowsableWhen || _design.BrowsableWhen(target);
}
#endif

bool DependencyPropertyMetadata::TryConvert(
	const BindingValue& value,
	BindingValue& out) const
{
	return _valueConverter && _valueConverter(value, out);
}

bool DependencyPropertyMetadata::IsValidValue(
	const BindingValue& value) const
{
	return _property
		? _property->IsValidValue(value)
		: (!_validator || _validator(value));
}

void DependencyPropertyMetadata::MergeBaseMetadata(
	const DependencyPropertyMetadata& base)
{
	// A derived/AddOwner layer with its own CLR accessor remains
	// accessor-backed even when the identity's default owner uses the generic
	// effective-value store. Otherwise the merged metadata accepts writes into
	// a hidden slot and never updates the derived owner's backing field.
	const bool hasOwnAccessor = static_cast<bool>(_getter)
		|| static_cast<bool>(_setter);
	if (!_valueConverter) _valueConverter = base._valueConverter;
	if (!_coercer) _coercer = base._coercer;
	if (!_comparer) _comparer = base._comparer;
	if (!_getter) _getter = base._getter;
	if (!_setter) _setter = base._setter;
	_usesEffectiveValueStorage = hasOwnAccessor
		? false
		: (_usesEffectiveValueStorage || base._usesEffectiveValueStorage);
	if (!_subscriber)
	{
		_subscriber = base._subscriber;
		_usesGenericObservation = base._usesGenericObservation;
	}
	if (!_hasDefaultValue && base._hasDefaultValue)
	{
		_defaultValue = base._defaultValue;
		_hasDefaultValue = true;
	}
	_flags |= base._flags;
#if CUI_ENABLE_DYNAMIC_XAML
	if (_inheritanceKey.empty()) _inheritanceKey = base._inheritanceKey;
#endif
	if (_defaultUpdateMode == DataSourceUpdateMode::OnPropertyChanged)
		_defaultUpdateMode = base._defaultUpdateMode;
#if CUI_ENABLE_DESIGN_METADATA
	if (_design.DisplayName.empty()) _design.DisplayName = base._design.DisplayName;
	if (_design.Category == L"Misc") _design.Category = base._design.Category;
	if (_design.Choices.empty()) _design.Choices = base._design.Choices;
	if (!_design.Minimum) _design.Minimum = base._design.Minimum;
	if (!_design.Maximum) _design.Maximum = base._design.Maximum;
	if (!_design.Step) _design.Step = base._design.Step;
	if (_design.Persistence == DependencyPropertyPersistence::Automatic)
		_design.Persistence = base._design.Persistence;
	if (!_design.BrowsableWhen) _design.BrowsableWhen = base._design.BrowsableWhen;
#endif

	if (base._changed && _changed)
	{
		auto baseChanged = base._changed;
		auto derivedChanged = std::move(_changed);
		_changed = [
			baseChanged = std::move(baseChanged),
			derivedChanged = std::move(derivedChanged)](
			DependencyObject& target,
			const BindingValue& oldValue,
			const BindingValue& newValue)
		{
			baseChanged(target, oldValue, newValue);
			derivedChanged(target, oldValue, newValue);
		};
	}
	else if (!_changed)
	{
		_changed = base._changed;
	}
}

bool DependencyPropertyMetadata::TryCoerce(
	DependencyObject& target,
	const BindingValue& value,
	BindingValue& out) const
{
	if (!Matches(target) || !IsValidValue(value)) return false;
	if (_coercer)
		return _coercer(target, value, out) && IsValidValue(out);
	out = value;
	return true;
}

bool DependencyPropertyMetadata::ValuesEqual(
	const BindingValue& left,
	const BindingValue& right) const
{
	return _comparer && _comparer(left, right);
}

bool DependencyPropertyMetadata::TryGetDefaultValue(BindingValue& out) const
{
	if (!_hasDefaultValue) return false;
	out = _defaultValue;
	return true;
}

bool DependencyPropertyMetadata::Matches(const DependencyObject& target) const
{
	return _matcher && _matcher(target);
}

bool DependencyPropertyMetadata::TryGet(DependencyObject& target, BindingValue& out) const
{
	if (!Matches(target)) return false;
	if (_usesEffectiveValueStorage)
		return target.TryGetEffectivePropertyValue(*this, out);
	return _getter && _getter(target, out);
}

bool DependencyPropertyMetadata::TrySet(DependencyObject& target, const BindingValue& value) const
{
	if (_isReadOnly) return false;
	return TrySetInternal(target, value);
}

bool DependencyPropertyMetadata::TrySetInternal(
	DependencyObject& target,
	const BindingValue& value) const
{
	target.VerifyAccess();
	if (!Matches(target)) return false;
	if (_usesEffectiveValueStorage)
		return target.TrySetPropertyValue(Property(), value);
	if (!_setter) return false;
	BindingValue converted;
	if (!TryConvert(value, converted)) return false;
	BindingValue effective;
	if (!TryCoerce(target, converted, effective)) return false;
	return TrySetEffective(target, effective);
}

bool DependencyPropertyMetadata::TrySetEffective(
	DependencyObject& target,
	const BindingValue& value) const
{
	if (!_setter || !Matches(target)) return false;
	auto* controlTarget = dynamic_cast<Control*>(&target);
	const ControlWeakReference targetReference(controlTarget);
	BindingValue oldValue;
	const bool hadOldValue = _getter && _getter(target, oldValue);
	const auto changeVersion = target._propertyChangeVersion;
	if (!_setter(target, value)) return false;
	if (controlTarget && !targetReference) return true;
	if (target._propertyChangeVersion != changeVersion)
		return true;

	BindingValue newValue = value;
	if (_getter) _getter(target, newValue);
	if (!hadOldValue || !ValuesEqual(oldValue, newValue))
		target.ApplyPropertyMetadataChange(*this, oldValue, newValue);
	return true;
}

void DependencyPropertyMetadata::NotifyChanged(
	DependencyObject& target,
	const BindingValue& oldValue,
	const BindingValue& newValue) const
{
	if (_changed) _changed(target, oldValue, newValue);
}

EventConnection DependencyPropertyMetadata::Subscribe(
	DependencyObject& target,
	ChangeHandler handler,
	DataSourceUpdateMode updateMode) const
{
	if (_usesGenericObservation && Matches(target) && handler)
		return target.SubscribeDefaultPropertyChange(
			Property(), std::move(handler), updateMode);
	if (_subscriber && Matches(target) && handler)
		return _subscriber(target, std::move(handler), updateMode);
	return {};
}


#if !CUI_ENABLE_DYNAMIC_XAML
const DependencyPropertyMetadata* DependencyPropertyRegistry::GetMetadata(
	DependencyObject& target,
	const DependencyProperty& property)
{
	// Accessor-owned Production identities resolve only their exact, intrusive
	// relation chain. The hook lets a derived type touch a sparse relation
	// accessor without anchoring an aggregate owner table.
	if (const auto* exact =
		target.ResolveExactDependencyPropertyMetadata(property))
	{
		if (&exact->Property() == &property
			&& exact->Matches(target)
			&& target.SupportsNativeProperty(*exact))
			return exact;
	}
	const auto* metadata = property._standaloneMetadata;
	return metadata
		&& &metadata->Property() == &property
		&& metadata->Matches(target)
		&& target.SupportsNativeProperty(*metadata)
		? metadata
		: nullptr;
}
#endif


#if CUI_ENABLE_DYNAMIC_XAML
PropertyChangedEventArgs::PropertyChangedEventArgs(std::wstring propertyName)
	: PropertyName(std::move(propertyName)),
	  PropertyToken(MakeBindingSourcePropertyToken(PropertyName))
{
}
#endif

PropertyChangedEventArgs::PropertyChangedEventArgs(
	BindingSourcePropertyToken propertyToken) noexcept
	: PropertyToken(propertyToken)
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
#if CUI_ENABLE_DYNAMIC_XAML
	Notify(PropertyChangedEventArgs(propertyName));
#else
	Notify(MakeBindingSourcePropertyToken(propertyName));
#endif
}

void PropertyChangedEvent::Notify(
	BindingSourcePropertyToken propertyToken)
{
	Notify(PropertyChangedEventArgs(propertyToken));
}

void PropertyChangedEvent::Notify(const PropertyChangedEventArgs& args)
{
	if (!_state || _state->Handlers.empty()) return;
	auto snapshot = _state->Handlers;
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

#if CUI_ENABLE_DYNAMIC_XAML
BindingValidationChangedEventArgs::BindingValidationChangedEventArgs(
	std::wstring propertyName)
	: PropertyName(std::move(propertyName)),
	  PropertyToken(MakeBindingSourcePropertyToken(PropertyName))
{
}
#endif

BindingValidationChangedEventArgs::BindingValidationChangedEventArgs(
	BindingSourcePropertyToken propertyToken) noexcept
	: PropertyToken(propertyToken)
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
#if CUI_ENABLE_DYNAMIC_XAML
	Notify(BindingValidationChangedEventArgs(propertyName));
#else
	Notify(MakeBindingSourcePropertyToken(propertyName));
#endif
}

void BindingValidationChangedEvent::Notify(
	BindingSourcePropertyToken propertyToken)
{
	Notify(BindingValidationChangedEventArgs(propertyToken));
}

void BindingValidationChangedEvent::Notify(
	const BindingValidationChangedEventArgs& args)
{
	if (!_state || _state->Handlers.empty()) return;
	auto snapshot = _state->Handlers;
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
	if (name.empty()) return false;
	const auto property = MakeBindingSourcePropertyToken(name);
#if CUI_ENABLE_DYNAMIC_XAML
	auto metadata = _metadata.find(property.Value);
	if (metadata != _metadata.end() && !metadata->second.Name.empty()
		&& metadata->second.Name != name) return false;
#endif
	return TryGetValue(property, out);
}

bool ObservableObject::TryGetValue(
	BindingSourcePropertyToken property,
	BindingValue& out) const
{
	if (!property) return false;
	auto metadata = _metadata.find(property.Value);
	if (metadata != _metadata.end() && !metadata->second.CanRead)
		return false;
	auto it = _values.find(property.Value);
	if (it == _values.end())
		return false;
	out = it->second;
	return true;
}

bool ObservableObject::TrySetValue(const std::wstring& propertyName, const BindingValue& value)
{
	const auto name = Trim(propertyName);
	if (name.empty()) return false;
	const auto property = MakeBindingSourcePropertyToken(name);
	auto metadata = _metadata.find(property.Value);
#if CUI_ENABLE_DYNAMIC_XAML
	if (metadata != _metadata.end() && !metadata->second.Name.empty()
		&& metadata->second.Name != name) return false;
#endif
	if (metadata != _metadata.end() && !metadata->second.CanWrite)
		return false;
	return SetCurrentValue(name, value, true);
}

bool ObservableObject::TrySetValue(
	BindingSourcePropertyToken property,
	const BindingValue& value)
{
	if (!property) return false;
	auto metadata = _metadata.find(property.Value);
	if (metadata != _metadata.end() && !metadata->second.CanWrite)
		return false;
	return SetCurrentValue(property, value, true);
}

bool ObservableObject::BeginEdit()
{
	if (_editValues) return true;
	_editValues.emplace(_values);
	return true;
}

bool ObservableObject::EndEdit()
{
	_editValues.reset();
	return true;
}

bool ObservableObject::CancelEdit()
{
	if (!_editValues) return true;
	std::vector<std::uint64_t> changed;
	changed.reserve(_values.size() + _editValues->size());
	for (const auto& [property, value] : _values)
	{
		const auto original = _editValues->find(property);
		if (original == _editValues->end()
			|| !BindingValuesEqual(value, original->second))
			changed.push_back(property);
	}
	for (const auto& [property, value] : *_editValues)
	{
		(void)value;
		if (_values.find(property) == _values.end())
			changed.push_back(property);
	}
	_values.swap(*_editValues);
	_editValues.reset();
	std::sort(changed.begin(), changed.end());
	changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
	// Restore the complete record before publishing the first notification so
	// every reentrant binding read observes one coherent pre-edit state.
	for (const auto property : changed)
	{
		const auto metadata = _metadata.find(property);
		if (metadata != _metadata.end() && !metadata->second.CanObserve)
			continue;
#if CUI_ENABLE_DYNAMIC_XAML
		if (metadata != _metadata.end() && !metadata->second.Name.empty())
			OnPropertyChanged(metadata->second.Name);
		else
#endif
			OnPropertyChanged(BindingSourcePropertyToken{ property });
	}
	return true;
}

bool ObservableObject::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	const auto name = Trim(propertyName);
	if (name.empty()) return false;
	const auto property = MakeBindingSourcePropertyToken(name);
	const auto it = _metadata.find(property.Value);
#if CUI_ENABLE_DYNAMIC_XAML
	if (it != _metadata.end() && !it->second.Name.empty()
		&& it->second.Name != name) return false;
#endif
	if (it == _metadata.end()) return false;
	out = it->second;
	return true;
}

bool ObservableObject::TryGetPropertyMetadata(
	BindingSourcePropertyToken property,
	BindingSourcePropertyMetadata& out) const
{
	if (!property) return false;
	const auto it = _metadata.find(property.Value);
	if (it == _metadata.end()) return false;
	out = it->second;
	return true;
}

#if CUI_ENABLE_DYNAMIC_XAML
std::vector<BindingSourcePropertyMetadata> ObservableObject::GetProperties() const
{
	std::vector<BindingSourcePropertyMetadata> result;
	result.reserve(_metadata.size());
	for (const auto& [property, metadata] : _metadata)
	{
		(void)property;
		result.push_back(metadata);
	}
	std::sort(result.begin(), result.end(),
		[](const auto& left, const auto& right)
		{
			const auto common = (std::min)(left.Name.size(), right.Name.size());
			for (std::size_t index = 0; index < common; ++index)
			{
				const auto leftCharacter = std::towlower(left.Name[index]);
				const auto rightCharacter = std::towlower(right.Name[index]);
				if (leftCharacter != rightCharacter)
					return leftCharacter < rightCharacter;
			}
			return left.Name.size() < right.Name.size();
		});
	return result;
}
#endif

BindingSourceProxy::BindingSourceProxy(BindingSourceReference source)
	: _source(std::move(source))
{
	Attach();
}

void BindingSourceProxy::SetSource(BindingSourceReference source)
{
	if (_source == source) return;
	_propertyConnection.Disconnect();
	_validationConnection.Disconnect();
	_source = std::move(source);
	Attach();
#if CUI_ENABLE_DYNAMIC_XAML
	_propertyChanged.Notify(L"");
	_validationChanged.Notify(L"");
#else
	_propertyChanged.Notify(BindingSourcePropertyToken{});
	_validationChanged.Notify(BindingSourcePropertyToken{});
#endif
}

#if CUI_ENABLE_DYNAMIC_XAML
bool BindingSourceProxy::TryGetValue(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	return _source && _source.Get()->TryGetValue(propertyName, out);
}
#endif

bool BindingSourceProxy::TryGetValue(
	BindingSourcePropertyToken property,
	BindingValue& out) const
{
	if (!_source) return false;
	// RootValue is a framework-reserved endpoint.  Resolve it before asking the
	// record so an authored/hash-colliding property can never replace the WPF
	// `{Binding}` object identity.
	if (property == BindingRootValuePropertyToken())
	{
		if (const auto* provider = dynamic_cast<
			const IBindingRootValueProvider*>(_source.Get()); provider
			&& provider->TryGetBindingRootValue(out)) return true;
		out = BindingValue(_source);
		return true;
	}
	return _source.Get()->TryGetValue(property, out);
}

#if CUI_ENABLE_DYNAMIC_XAML
bool BindingSourceProxy::TrySetValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return _source && _source.Get()->TrySetValue(propertyName, value);
}
#endif

bool BindingSourceProxy::TrySetValue(
	BindingSourcePropertyToken property,
	const BindingValue& value)
{
	if (property == BindingRootValuePropertyToken()) return false;
	return _source && _source.Get()->TrySetValue(property, value);
}

#if CUI_ENABLE_DYNAMIC_XAML
bool BindingSourceProxy::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	return _source
		&& _source.Get()->TryGetPropertyMetadata(propertyName, out);
}
#endif

bool BindingSourceProxy::TryGetPropertyMetadata(
	BindingSourcePropertyToken property,
	BindingSourcePropertyMetadata& out) const
{
	if (!_source) return false;
	if (property != BindingRootValuePropertyToken())
		return _source.Get()->TryGetPropertyMetadata(property, out);
	if (const auto* provider = dynamic_cast<
		const IBindingRootValueProvider*>(_source.Get()); provider
		&& provider->TryGetBindingRootValueMetadata(out)) return true;
#if CUI_ENABLE_DYNAMIC_XAML
	out = { std::wstring{}, BindingValueKind::Object,
		std::type_index(typeid(BindingSourceReference)), true, false, true };
#else
	out = { BindingValueKind::Object,
		std::type_index(typeid(BindingSourceReference)), true, false, true };
#endif
	return true;
}

#if CUI_ENABLE_DYNAMIC_XAML
std::vector<BindingSourcePropertyMetadata> BindingSourceProxy::GetProperties() const
{
	return _source ? _source.Get()->GetProperties()
		: std::vector<BindingSourcePropertyMetadata>{};
}

std::vector<BindingValidationIssue> BindingSourceProxy::GetValidationIssues(
	const std::wstring& propertyName) const
{
	return _source ? _source.Get()->GetValidationIssues(propertyName)
		: std::vector<BindingValidationIssue>{};
}
#endif

std::vector<BindingValidationIssue> BindingSourceProxy::GetValidationIssues(
	BindingSourcePropertyToken property) const
{
	// The reserved root endpoint describes the projected object/value itself;
	// an authored property whose token collides with it must not leak its
	// validation state into `{Binding}`.
	if (property == BindingRootValuePropertyToken()) return {};
	return _source ? _source.Get()->GetValidationIssues(property)
		: std::vector<BindingValidationIssue>{};
}

void BindingSourceProxy::Attach()
{
	if (!_source) return;
	_propertyConnection = _source.Get()->PropertyChanged().Subscribe(
		[this](const PropertyChangedEventArgs& args)
		{
			if (args.PropertyToken == BindingRootValuePropertyToken()) return;
			_propertyChanged.Notify(args);
		});
	if (auto* validation = _source.Get()->ValidationChanged())
		_validationConnection = validation->Subscribe(
			[this](const BindingValidationChangedEventArgs& args)
			{
				if (args.PropertyToken == BindingRootValuePropertyToken()) return;
				_validationChanged.Notify(args);
			});
}

#if CUI_ENABLE_DYNAMIC_XAML
std::vector<BindingValidationIssue> GetBindingValidationIssuesForPath(
	const IBindingSource& source,
	const std::wstring& sourcePropertyPath)
{
	std::vector<BindingPathStep> path;
	if (!TryParseBindingPropertyPath(sourcePropertyPath, path)) return {};

	std::vector<BindingValidationIssue> result;
	auto append = [&result](std::vector<BindingValidationIssue> issues)
	{
		for (auto& issue : NormalizeValidationIssues(std::move(issues)))
		{
			if (std::find(result.begin(), result.end(), issue) == result.end())
				result.push_back(std::move(issue));
		}
	};

	BindingPathCursor cursor;
	cursor.Source = const_cast<IBindingSource*>(&source);
	for (size_t index = 0; index < path.size(); ++index)
	{
		if (cursor.Source)
		{
			append(cursor.Source->GetValidationIssues(L""));
			append(cursor.Source->GetValidationIssues(path[index].Value));
		}
		if (index + 1 == path.size()) break;

		BindingValue value;
		if (!TryReadBindingPathStep(cursor, path[index], value)
			|| !SetBindingPathCursor(value, cursor))
			break;
	}
	return result;
}
#endif

std::vector<BindingValidationIssue> ObservableObject::GetValidationIssues(
	const std::wstring& propertyName) const
{
	const auto name = Trim(propertyName);
	const auto property = MakeBindingSourcePropertyToken(name);
	if (property)
	{
		const auto metadata = _metadata.find(property.Value);
		if (metadata != _metadata.end()
			&& !BindingSourceMetadataNameMatches(metadata->second, name)) return {};
	}
	return GetValidationIssues(property);
}

std::vector<BindingValidationIssue> ObservableObject::GetValidationIssues(
	BindingSourcePropertyToken property) const
{
	const auto it = _validationIssues.find(property.Value);
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
	const auto name = Trim(propertyName);
	const auto property = MakeBindingSourcePropertyToken(name);
	if (property)
	{
		const auto metadata = _metadata.find(property.Value);
		if (metadata != _metadata.end()
			&& !BindingSourceMetadataNameMatches(metadata->second, name)) return false;
	}
	return HasValidationErrors(property);
}

bool ObservableObject::HasValidationErrors(
	BindingSourcePropertyToken property) const
{
	const auto it = _validationIssues.find(property.Value);
	return it != _validationIssues.end() && ContainsValidationError(it->second);
}

bool ObservableObject::SetValidationIssues(
	const std::wstring& propertyName,
	std::vector<BindingValidationIssue> issues)
{
	const auto name = Trim(propertyName);
	const auto property = MakeBindingSourcePropertyToken(name);
	if (property)
	{
		const auto metadata = _metadata.find(property.Value);
		if (metadata != _metadata.end()
			&& !BindingSourceMetadataNameMatches(metadata->second, name)) return false;
	}
	auto normalized = NormalizeValidationIssues(std::move(issues));
	const auto existing = _validationIssues.find(property.Value);
	if (normalized.empty())
	{
		if (existing == _validationIssues.end()) return false;
		_validationIssues.erase(existing);
#if CUI_ENABLE_DYNAMIC_XAML
		_validationChanged.Notify(name);
#else
		_validationChanged.Notify(property);
#endif
		return true;
	}
	if (existing != _validationIssues.end() && existing->second == normalized)
		return false;
	_validationIssues[property.Value] = std::move(normalized);
#if CUI_ENABLE_DYNAMIC_XAML
	_validationChanged.Notify(name);
#else
	_validationChanged.Notify(property);
#endif
	return true;
}

bool ObservableObject::SetValidationIssues(
	BindingSourcePropertyToken property,
	std::vector<BindingValidationIssue> issues)
{
	auto normalized = NormalizeValidationIssues(std::move(issues));
	const auto existing = _validationIssues.find(property.Value);
	if (normalized.empty())
	{
		if (existing == _validationIssues.end()) return false;
		_validationIssues.erase(existing);
		_validationChanged.Notify(property);
		return true;
	}
	if (existing != _validationIssues.end() && existing->second == normalized)
		return false;
	_validationIssues[property.Value] = std::move(normalized);
	_validationChanged.Notify(property);
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

bool ObservableObject::SetValidationError(
	BindingSourcePropertyToken property,
	std::wstring message,
	std::wstring code)
{
	if (Trim(message).empty()) return ClearValidationIssues(property);
	return SetValidationIssues(property,
		{ { std::move(message), BindingValidationSeverity::Error,
			std::move(code) } });
}

bool ObservableObject::ClearValidationIssues(
	const std::wstring& propertyName)
{
	const auto name = Trim(propertyName);
	const auto property = MakeBindingSourcePropertyToken(name);
	if (property)
	{
		const auto metadata = _metadata.find(property.Value);
		if (metadata != _metadata.end()
			&& !BindingSourceMetadataNameMatches(metadata->second, name)) return false;
	}
	const auto existing = _validationIssues.find(property.Value);
	if (existing == _validationIssues.end()) return false;
	_validationIssues.erase(existing);
#if CUI_ENABLE_DYNAMIC_XAML
	_validationChanged.Notify(name);
#else
	_validationChanged.Notify(property);
#endif
	return true;
}

bool ObservableObject::ClearValidationIssues(
	BindingSourcePropertyToken property)
{
	const auto existing = _validationIssues.find(property.Value);
	if (existing == _validationIssues.end()) return false;
	_validationIssues.erase(existing);
	_validationChanged.Notify(property);
	return true;
}

bool ObservableObject::ClearAllValidationIssues()
{
	if (_validationIssues.empty()) return false;
	_validationIssues.clear();
#if CUI_ENABLE_DYNAMIC_XAML
	_validationChanged.Notify(L"");
#else
	_validationChanged.Notify(BindingSourcePropertyToken{});
#endif
	return true;
}

#if CUI_ENABLE_DYNAMIC_XAML
bool ObservableObject::DefineProperty(
	BindingSourcePropertyMetadata metadata,
	const BindingValue& initialValue,
	bool replaceExisting)
{
	metadata.Name = Trim(std::move(metadata.Name));
	if (metadata.Name.empty()) return false;
	const auto property = MakeBindingSourcePropertyToken(metadata.Name);
	return DefineProperty(property, std::move(metadata),
		initialValue, replaceExisting);
}
#endif

bool ObservableObject::DefineProperty(
	BindingSourcePropertyToken property,
	BindingSourcePropertyMetadata metadata,
	const BindingValue& initialValue,
	bool replaceExisting)
{
	if (!property) return false;
	const auto existing = _metadata.find(property.Value);
#if CUI_ENABLE_DYNAMIC_XAML
	metadata.Name = Trim(std::move(metadata.Name));
	if (!metadata.Name.empty()
		&& MakeBindingSourcePropertyToken(metadata.Name) != property)
		return false;
	if (existing != _metadata.end()
		&& !existing->second.Name.empty() && !metadata.Name.empty()
		&& existing->second.Name != metadata.Name) return false;
#endif
	if (existing != _metadata.end() && !replaceExisting) return false;
	const bool existed = existing != _metadata.end();
	const auto oldValue = _values.find(property.Value);
	const bool hadValue = oldValue != _values.end();
	BindingValue previous;
	if (hadValue) previous = oldValue->second;

	BindingValue normalized;
	if (!NormalizeValue(metadata, initialValue, normalized)) return false;
	const bool changed = !hadValue || !BindingValuesEqual(previous, normalized);
	_metadata[property.Value] = metadata;
	_values[property.Value] = std::move(normalized);
	if (existed && changed && metadata.CanObserve)
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (metadata.Name.empty()) OnPropertyChanged(property);
		else OnPropertyChanged(metadata.Name);
#else
		OnPropertyChanged(property);
#endif
	}
	return true;
}

bool ObservableObject::RemoveProperty(const std::wstring& propertyName)
{
	const auto name = Trim(propertyName);
	if (name.empty()) return false;
	const auto property = MakeBindingSourcePropertyToken(name);
	const auto metadata = _metadata.find(property.Value);
	if (metadata != _metadata.end()
		&& !BindingSourceMetadataNameMatches(metadata->second, name)) return false;
	return RemoveProperty(property);
}

bool ObservableObject::RemoveProperty(BindingSourcePropertyToken property)
{
	if (!property) return false;
	const auto metadata = _metadata.find(property.Value);
	if (metadata == _metadata.end()) return false;
	const bool notify = metadata->second.CanObserve;
#if CUI_ENABLE_DYNAMIC_XAML
	const auto name = metadata->second.Name;
#endif
	_metadata.erase(metadata);
	_values.erase(property.Value);
	const bool validationRemoved = _validationIssues.erase(property.Value) != 0;
	if (validationRemoved)
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (name.empty()) _validationChanged.Notify(property);
		else _validationChanged.Notify(name);
#else
		_validationChanged.Notify(property);
#endif
	}
	if (notify)
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (name.empty()) OnPropertyChanged(property);
		else OnPropertyChanged(name);
#else
		OnPropertyChanged(property);
#endif
	}
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
	const auto property = MakeBindingSourcePropertyToken(name);

	auto metadataIt = _metadata.find(property.Value);
	if (metadataIt == _metadata.end())
	{
		BindingSourcePropertyMetadata metadata;
#if CUI_ENABLE_DYNAMIC_XAML
		metadata.Name = name;
#endif
		BindingValue normalized;
		if (!NormalizeValue(metadata, value, normalized)) return false;
		_metadata[property.Value] = metadata;
		_values[property.Value] = std::move(normalized);
		if (notify && metadata.CanObserve) OnPropertyChanged(name);
		return true;
	}
	if (!BindingSourceMetadataNameMatches(metadataIt->second, name)) return false;

	BindingValue next;
	if (!NormalizeValue(metadataIt->second, value, next)) return false;
	auto valueIt = _values.find(property.Value);
	if (valueIt != _values.end() && BindingValuesEqual(valueIt->second, next))
		return true;
	_values[property.Value] = std::move(next);
	if (notify && metadataIt->second.CanObserve) OnPropertyChanged(name);
	return true;
}

bool ObservableObject::SetCurrentValue(
	BindingSourcePropertyToken property,
	const BindingValue& value,
	bool notify)
{
	if (!property) return false;
	auto metadataIt = _metadata.find(property.Value);
	if (metadataIt == _metadata.end())
	{
		BindingSourcePropertyMetadata metadata;
		BindingValue normalized;
		if (!NormalizeValue(metadata, value, normalized)) return false;
		_metadata[property.Value] = metadata;
		_values[property.Value] = std::move(normalized);
		if (notify && metadata.CanObserve) OnPropertyChanged(property);
		return true;
	}

	BindingValue next;
	if (!NormalizeValue(metadataIt->second, value, next)) return false;
	auto valueIt = _values.find(property.Value);
	if (valueIt != _values.end() && BindingValuesEqual(valueIt->second, next))
		return true;
	_values[property.Value] = std::move(next);
	if (notify && metadataIt->second.CanObserve) OnPropertyChanged(property);
	return true;
}

void ObservableObject::OnPropertyChanged(const std::wstring& propertyName)
{
#if CUI_ENABLE_DYNAMIC_XAML
	_propertyChanged.Notify(propertyName);
#else
	_propertyChanged.Notify(MakeBindingSourcePropertyToken(propertyName));
#endif
}

void ObservableObject::OnPropertyChanged(
	BindingSourcePropertyToken property)
{
	_propertyChanged.Notify(property);
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding::Binding(DependencyObject* target,
	std::wstring targetProperty,
	IBindingSource* source,
	std::wstring sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(std::move(targetProperty)),
		source, BindingSourcePropertyReference(std::move(sourceProperty)),
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}
#endif

#if CUI_ENABLE_DYNAMIC_XAML
Binding::Binding(DependencyObject* target,
	const DependencyProperty& targetProperty,
	IBindingSource* source,
	std::wstring sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(targetProperty), source,
		BindingSourcePropertyReference(std::move(sourceProperty)),
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}
#endif

Binding::Binding(DependencyObject* target,
	const DependencyProperty& targetProperty,
	IBindingSource* source,
	CompiledBindingPathView sourcePath,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(targetProperty), source,
		sourcePath, mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}

Binding::Binding(DependencyObject* target,
	const DependencyProperty& targetProperty,
	CompiledSourceHandle source,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(targetProperty), source,
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding::Binding(DependencyObject* target,
	DependencyPropertyReference targetProperty,
	IBindingSource* source,
	BindingSourcePropertyReference sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _sourceStorage(source, sourceProperty.Empty()
		  ? BindingRootValuePath() : CompiledBindingPathView{}),
	  _targetProperty(std::move(targetProperty)),
	  _sourceProperty(std::move(sourceProperty)),
	  _mode(mode),
	  _updateMode(updateMode),
	  _converter(std::move(converter)),
	  _fallbackValue(std::move(fallbackValue)),
	  _targetNullValue(std::move(targetNullValue)),
	  _converterParameter(std::move(converterParameter)),
	  _stringFormat(std::move(stringFormat)),
	  _state(std::make_shared<State>()),
	  _targetValueSource(targetValueSource),
	  _expressionKind(expressionKind)
{
	_state->Owner = this;
	if (_target)
		_targetLifetime = _target->BindingLifetime();
	if (auto* adapterSource = AdapterSource())
		_sourceLifetime = adapterSource->BindingLifetime();
	Attach();
}
#endif

#if CUI_ENABLE_DYNAMIC_XAML
Binding::Binding(DependencyObject* target,
	std::wstring targetProperty,
	BindingSourceReference source,
	std::wstring sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(std::move(targetProperty)),
		std::move(source), BindingSourcePropertyReference(std::move(sourceProperty)),
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}
#endif

#if CUI_ENABLE_DYNAMIC_XAML
Binding::Binding(DependencyObject* target,
	const DependencyProperty& targetProperty,
	BindingSourceReference source,
	std::wstring sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(targetProperty),
		std::move(source), BindingSourcePropertyReference(std::move(sourceProperty)),
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}
#endif

Binding::Binding(DependencyObject* target,
	const DependencyProperty& targetProperty,
	BindingSourceReference source,
	CompiledBindingPathView sourcePath,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Binding(target, DependencyPropertyReference(targetProperty),
		std::move(source), sourcePath, mode, updateMode,
		std::move(converter), std::move(fallbackValue),
		std::move(targetNullValue), std::move(converterParameter),
		std::move(stringFormat), DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding::Binding(DependencyObject* target,
	DependencyPropertyReference targetProperty,
	BindingSourceReference source,
	BindingSourcePropertyReference sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _sourceStorage(std::move(source), sourceProperty.Empty()
		  ? BindingRootValuePath() : CompiledBindingPathView{}),
	  _targetProperty(std::move(targetProperty)),
	  _sourceProperty(std::move(sourceProperty)),
	  _mode(mode),
	  _updateMode(updateMode),
	  _converter(std::move(converter)),
	  _fallbackValue(std::move(fallbackValue)),
	  _targetNullValue(std::move(targetNullValue)),
	  _converterParameter(std::move(converterParameter)),
	  _stringFormat(std::move(stringFormat)),
	  _state(std::make_shared<State>()),
	  _targetValueSource(targetValueSource),
	  _expressionKind(expressionKind)
{
	_state->Owner = this;
	if (_target)
		_targetLifetime = _target->BindingLifetime();
	if (auto* adapterSource = AdapterSource())
		_sourceLifetime = adapterSource->BindingLifetime();
	Attach();
}
#endif

Binding::Binding(DependencyObject* target,
	DependencyPropertyReference targetProperty,
	IBindingSource* source,
	CompiledBindingPathView sourcePath,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _sourceStorage(source, sourcePath),
	  _targetProperty(std::move(targetProperty)),
	  _mode(mode),
	  _updateMode(updateMode),
	  _converter(std::move(converter)),
	  _fallbackValue(std::move(fallbackValue)),
	  _targetNullValue(std::move(targetNullValue)),
	  _converterParameter(std::move(converterParameter)),
	  _stringFormat(std::move(stringFormat)),
	  _state(std::make_shared<State>()),
	  _targetValueSource(targetValueSource),
	  _expressionKind(expressionKind)
{
	_state->Owner = this;
	if (_target) _targetLifetime = _target->BindingLifetime();
	if (auto* adapterSource = AdapterSource())
		_sourceLifetime = adapterSource->BindingLifetime();
	Attach();
}

Binding::Binding(DependencyObject* target,
	DependencyPropertyReference targetProperty,
	BindingSourceReference source,
	CompiledBindingPathView sourcePath,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _sourceStorage(std::move(source), sourcePath),
	  _targetProperty(std::move(targetProperty)),
	  _mode(mode),
	  _updateMode(updateMode),
	  _converter(std::move(converter)),
	  _fallbackValue(std::move(fallbackValue)),
	  _targetNullValue(std::move(targetNullValue)),
	  _converterParameter(std::move(converterParameter)),
	  _stringFormat(std::move(stringFormat)),
	  _state(std::make_shared<State>()),
	  _targetValueSource(targetValueSource),
	  _expressionKind(expressionKind)
{
	_state->Owner = this;
	if (_target) _targetLifetime = _target->BindingLifetime();
	if (auto* adapterSource = AdapterSource())
		_sourceLifetime = adapterSource->BindingLifetime();
	Attach();
}

Binding::Binding(DependencyObject* target,
	DependencyPropertyReference targetProperty,
	CompiledSourceHandle source,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _sourceStorage(source),
	  _targetProperty(std::move(targetProperty)),
	  _mode(mode),
	  _updateMode(updateMode),
	  _converter(std::move(converter)),
	  _fallbackValue(std::move(fallbackValue)),
	  _targetNullValue(std::move(targetNullValue)),
	  _converterParameter(std::move(converterParameter)),
	  _stringFormat(std::move(stringFormat)),
	  _state(std::make_shared<State>()),
	  _targetValueSource(targetValueSource),
	  _expressionKind(expressionKind)
{
	_state->Owner = this;
	if (_target) _targetLifetime = _target->BindingLifetime();
	if (const auto directSource = DirectSource())
		_sourceLifetime = directSource.Lifetime();
	Attach();
}

Binding::Binding(DependencyObject* target,
	const DependencyProperty& targetProperty,
	DependencyObject& source,
	const DependencyProperty& sourceProperty,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _sourceStorage(static_cast<IBindingSource*>(&source), {}),
	  _sourceDependencyObject(&source),
	  _targetProperty(targetProperty),
	  _sourceProperty(sourceProperty),
	  _mode(BindingMode::OneWay),
	  _updateMode(DataSourceUpdateMode::Never),
	  _state(std::make_shared<State>()),
	  _targetValueSource(targetValueSource),
	  _expressionKind(expressionKind)
{
	_state->Owner = this;
	if (_target)
		_targetLifetime = _target->BindingLifetime();
	_sourceLifetime = source.BindingLifetime();
	Attach();
}

Binding::~Binding()
{
	if (_state)
		_state->Owner = nullptr;
	_targetConnection.Disconnect();
	if (_ownsTargetValue && IsTargetAlive() && _targetMetadata)
		_target->ClearBindingPropertyValue(
			*_targetMetadata, this, _targetValueSource, _expressionKind);
	_ownsTargetValue = false;
	_sourceConnections.clear();
	_sourcePathOwners.clear();
	_sourcePathListOwners.clear();
	_sourceValidationConnections.clear();
	_validationPathConnections.clear();
	_validationPathOwners.clear();
	_validationPathListOwners.clear();
}

bool Binding::IsTargetAlive() const noexcept
{
	return _target && !_targetLifetime.expired()
		&& !_target->IsDestroying();
}

bool Binding::HasValidationErrors() const noexcept
{
	return ContainsValidationError(ValidationIssues());
}

void Binding::Attach()
{
	if (!Validate())
		return;
	if (IsSourceToTargetMode(_mode))
	{
		if (!_targetMetadata || !_target->TryAttachBindingPropertyExpression(
			*_targetMetadata, this, _targetValueSource, _expressionKind))
		{
			_isValid = false;
			Fail(BindingError::TargetWriteFailed);
			return;
		}
		_ownsTargetValue = true;
	}

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
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	_isValid = false;
	if (!IsTargetAlive()) return Fail(BindingError::InvalidTarget);
	if (!directSource && !AdapterSource())
		return Fail(BindingError::InvalidSource);
	if (directSource
		&& (!directSource.Ops->Capabilities
			|| !directSource.Ops->ValueKind
			|| !directSource.Ops->Lifetime))
		return Fail(BindingError::InvalidSource);
	if (_targetProperty.Empty()) return Fail(BindingError::EmptyTargetProperty);
	if (!directSource && sourcePath.Empty() && _sourceProperty.Empty())
		return Fail(BindingError::EmptySourceProperty);
	if (!directSource && !sourcePath.Empty())
	{
		if (sourcePath.Version != CompiledBindingPathVersion)
			return Fail(BindingError::InvalidSourcePropertyPath);
		for (const auto& step : sourcePath.Steps)
		{
			if (step.Kind == CompiledBindingPathStepKind::Property)
			{
				if (!step.EndpointResolver && !step.Property)
					return Fail(BindingError::InvalidSourcePropertyPath);
			}
			else if (step.Kind != CompiledBindingPathStepKind::ListIndex
				|| step.EndpointResolver)
				return Fail(BindingError::InvalidSourcePropertyPath);
		}
	}
#if CUI_ENABLE_DYNAMIC_XAML
	else if (!directSource && !_sourceProperty.IsCompiled()
		&& !TryParseBindingPropertyPath(SourceProperty(), _sourcePath))
		return Fail(BindingError::InvalidSourcePropertyPath);
#endif
	if (!IsSourceAlive()) return Fail(BindingError::SourceUnavailable);

	if (_targetProperty.IsCompiled())
	{
		_targetMetadata = _target->GetPropertyMetadata(
			*_targetProperty.Identity());
	}
#if CUI_ENABLE_DYNAMIC_XAML
	else
	{
		_targetMetadata = _target->FindPropertyMetadata(TargetProperty());
	}
#endif
	if (!_targetMetadata) return Fail(BindingError::TargetPropertyNotFound);
	// One successful dynamic lookup becomes the same stable identity used by
	// compiled bindings. All subsequent refresh/clear/duplicate operations are
	// identity based, while the dynamic constructor remains available to the
	// designer loader.
	_targetProperty = DependencyPropertyReference(_targetMetadata->Property());
	if (_targetMetadata->IsReadOnly())
		return Fail(BindingError::TargetNotWritable);
	_mode = ResolveBindingMode(*_targetMetadata, _mode);
	_updateMode = ResolveDataSourceUpdateMode(*_targetMetadata, _updateMode);
	if (_stringFormat
		&& (_targetMetadata->ValueKind() != BindingValueKind::String
			|| !IsValidBindingStringFormat(*_stringFormat)))
		return Fail(BindingError::InvalidStringFormat);
	if (IsSourceToTargetMode(_mode) && !_targetMetadata->CanWrite())
		return Fail(BindingError::TargetNotWritable);
	if (IsSourceToTargetMode(_mode)
		&& !_target->CanAcquireBindingPropertyValue(
			*_targetMetadata, this, _targetValueSource, _expressionKind))
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
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	auto* const adapterSource = AdapterSource();
	if (directSource)
	{
		const auto capabilities = directSource.Capabilities();
		if (IsSourceToTargetMode(_mode)
			&& (!HasCompiledBindingPathCapability(capabilities,
					CompiledBindingPathCapabilities::Read)
				|| !directSource.Ops->Read))
			return Fail(BindingError::SourceNotReadable);
		if (IsSourceToTargetMode(_mode) && _mode != BindingMode::OneTime
			&& (!HasCompiledBindingPathCapability(capabilities,
					CompiledBindingPathCapabilities::Observe)
				|| !directSource.Ops->Subscribe))
			return Fail(BindingError::SourceNotObservable);
		if (IsTargetToSourceMode(_mode)
			&& _updateMode != DataSourceUpdateMode::Never
			&& (!HasCompiledBindingPathCapability(capabilities,
					CompiledBindingPathCapabilities::Write)
				|| !directSource.Ops->Write))
			return Fail(BindingError::SourceNotWritable);
		return true;
	}
	if (!sourcePath.Empty())
	{
		BindingPathCursor cursor;
		cursor.Source = adapterSource;
		for (size_t index = 0;
			index < sourcePath.Steps.size(); ++index)
		{
			const auto& step = sourcePath.Steps[index];
			const bool leaf = index + 1 == sourcePath.Steps.size();

			if (cursor.Source)
			{
				if (step.Kind != CompiledBindingPathStepKind::Property)
					return Fail(BindingError::SourceNotReadable);
				const auto endpoint = ResolveCompiledBindingPathEndpoint(
					cursor, step);
				if (!endpoint)
					return Fail(leaf && IsTargetToSourceMode(_mode)
						&& _updateMode != DataSourceUpdateMode::Never
						? BindingError::SourceNotWritable
						: BindingError::SourceNotReadable);
				const auto capabilities = endpoint.Capabilities();
				if ((!leaf || IsSourceToTargetMode(_mode))
					&& (!HasCompiledBindingPathCapability(capabilities,
							CompiledBindingPathCapabilities::Read)
						|| !endpoint.Ops->Read))
					return Fail(BindingError::SourceNotReadable);
				if (IsSourceToTargetMode(_mode)
					&& _mode != BindingMode::OneTime
					&& (!HasCompiledBindingPathCapability(capabilities,
							CompiledBindingPathCapabilities::Observe)
						|| !endpoint.Ops->Subscribe))
					return Fail(BindingError::SourceNotObservable);
				if (leaf && IsTargetToSourceMode(_mode)
					&& _updateMode != DataSourceUpdateMode::Never
					&& (!HasCompiledBindingPathCapability(capabilities,
							CompiledBindingPathCapabilities::Write)
						|| !endpoint.Ops->Write))
					return Fail(BindingError::SourceNotWritable);
			}
			else if (cursor.List)
			{
				if (step.Kind != CompiledBindingPathStepKind::ListIndex)
					return Fail(BindingError::SourceNotReadable);
				if ((!leaf || IsSourceToTargetMode(_mode))
					&& !HasCompiledBindingPathCapability(step.Capabilities,
						CompiledBindingPathCapabilities::Read))
					return Fail(BindingError::SourceNotReadable);
				if (IsSourceToTargetMode(_mode)
					&& _mode != BindingMode::OneTime
					&& !HasCompiledBindingPathCapability(step.Capabilities,
						CompiledBindingPathCapabilities::Observe))
					return Fail(BindingError::SourceNotObservable);
				if (leaf && IsTargetToSourceMode(_mode)
					&& _updateMode != DataSourceUpdateMode::Never)
					return Fail(BindingError::SourceNotWritable);
			}
			if (leaf) break;
			BindingValue value;
			if (!TryReadCompiledBindingPathStep(cursor, step, value)
				|| !SetBindingPathCursor(value, cursor)) break;
		}
		return true;
	}
	if (_sourceProperty.IsCompiled())
	{
		if (!_sourceDependencyObject)
			return Fail(BindingError::InvalidSource);
		_sourceMetadata = _sourceDependencyObject->GetPropertyMetadata(
			*_sourceProperty.Identity());
		if (!_sourceMetadata) return Fail(BindingError::SourceNotReadable);
		if (IsSourceToTargetMode(_mode) && !_sourceMetadata->CanRead())
			return Fail(BindingError::SourceNotReadable);
		if (IsSourceToTargetMode(_mode)
			&& _mode != BindingMode::OneTime
			&& !_sourceMetadata->CanObserve())
			return Fail(BindingError::SourceNotObservable);
		if (IsTargetToSourceMode(_mode)
			&& _updateMode != DataSourceUpdateMode::Never
			&& !_sourceMetadata->CanWrite())
			return Fail(BindingError::SourceNotWritable);
		return true;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	if (!adapterSource || _sourcePath.empty()) return true;
	BindingPathCursor cursor;
	cursor.Source = adapterSource;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		const bool leaf = index + 1 == _sourcePath.size();
		if (cursor.List)
		{
			if (_sourcePath[index].Kind != BindingPathStepKind::Indexer)
				return Fail(BindingError::SourceNotReadable);
			size_t parsedIndex = 0;
			if (!TryParseBindingListIndex(
				_sourcePath[index].Value, parsedIndex))
				return Fail(BindingError::InvalidSourcePropertyPath);
			if (leaf && IsTargetToSourceMode(_mode)
				&& _updateMode != DataSourceUpdateMode::Never)
				return Fail(BindingError::SourceNotWritable);
		}
		else if (cursor.Source)
		{
			BindingSourcePropertyMetadata metadata;
			if (cursor.Source->TryGetPropertyMetadata(
				_sourcePath[index].Value, metadata))
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
		}
		if (leaf) break;

		BindingValue value;
		if (!TryReadBindingPathStep(cursor, _sourcePath[index], value)
			|| !SetBindingPathCursor(value, cursor))
			break;
	}
	return true;
#else
	return Fail(BindingError::InvalidSourcePropertyPath);
#endif
}

void Binding::AttachSourceChangedHandlers()
{
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	_sourceConnections.clear();
	_sourcePathOwners.clear();
	_sourcePathListOwners.clear();
	if (!IsSourceAlive() || _mode == BindingMode::OneWayToSource || _mode == BindingMode::OneTime)
		return;
	if (directSource)
	{
		std::weak_ptr<State> weakState = _state;
		auto connection = directSource.Ops->Subscribe(
			directSource,
			[weakState]
			{
				auto state = weakState.lock();
				if (!state || !state->Owner) return;
				state->Owner->OnSourcePathChanged();
			});
		if (!connection.Connected())
		{
			_isValid = false;
			_lastError = BindingError::SourceNotObservable;
			return;
		}
		_sourceConnections.push_back(std::move(connection));
		return;
	}
	if (!sourcePath.Empty())
	{
		BindingPathCursor cursor;
		cursor.Source = AdapterSource();
		for (size_t index = 0;
			index < sourcePath.Steps.size(); ++index)
		{
			const auto& step = sourcePath.Steps[index];
			std::weak_ptr<State> weakState = _state;
			EventConnection connection;
			if (cursor.Source
				&& step.Kind == CompiledBindingPathStepKind::Property)
			{
				const auto endpoint = ResolveCompiledBindingPathEndpoint(
					cursor, step);
				if (!endpoint || !endpoint.Ops->Subscribe)
				{
					_isValid = false;
					_lastError = BindingError::SourceNotObservable;
					return;
				}
				connection = endpoint.Ops->Subscribe(
					endpoint,
					[weakState]()
					{
						auto state = weakState.lock();
						if (!state || !state->Owner) return;
						state->Owner->OnSourcePathChanged();
					});
			}
			else if (cursor.List
				&& step.Kind == CompiledBindingPathStepKind::ListIndex)
			{
				connection = cursor.List->SubscribeChanged(
					[weakState](const CollectionChangedEventArgs&)
					{
						auto state = weakState.lock();
						if (!state || !state->Owner) return;
						state->Owner->OnSourcePathChanged();
					});
			}
			if (connection.Connected())
				_sourceConnections.push_back(std::move(connection));
			else
			{
				_isValid = false;
				_lastError = BindingError::SourceNotObservable;
				return;
			}
			if (index + 1 == sourcePath.Steps.size()) break;
			BindingValue value;
			if (!TryReadCompiledBindingPathStep(cursor, step, value)
				|| !SetBindingPathCursor(value, cursor)) break;
		}
		_sourcePathOwners = std::move(cursor.SourceOwners);
		_sourcePathListOwners = std::move(cursor.ListOwners);
		return;
	}
	if (_sourceProperty.IsCompiled())
	{
		if (!_sourceDependencyObject || !_sourceMetadata) return;
		std::weak_ptr<State> weakState = _state;
		auto connection = _sourceMetadata->Subscribe(
			*_sourceDependencyObject,
			[weakState]()
			{
				auto state = weakState.lock();
				if (!state || !state->Owner) return;
				state->Owner->OnSourcePathChanged();
			},
			DataSourceUpdateMode::OnPropertyChanged);
		if (connection.Connected())
			_sourceConnections.push_back(std::move(connection));
		return;
	}

#if CUI_ENABLE_DYNAMIC_XAML
	BindingPathCursor cursor;
	cursor.Source = AdapterSource();
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		std::weak_ptr<State> weakState = _state;
		EventConnection connection;
		if (cursor.Source)
		{
			const std::wstring expectedProperty = _sourcePath[index].Value;
			const auto expectedToken =
				MakeBindingSourcePropertyToken(expectedProperty);
			connection = cursor.Source->PropertyChanged().Subscribe(
				[weakState, expectedProperty, expectedToken](
					const PropertyChangedEventArgs& e)
				{
					if (e.PropertyToken
						? e.PropertyToken != expectedToken
						: (!e.PropertyName.empty()
							&& !IsSameProperty(e.PropertyName, expectedProperty))) return;
					auto state = weakState.lock();
					if (!state || !state->Owner) return;
					state->Owner->OnSourcePathChanged();
				});
		}
		else if (cursor.List)
		{
			connection = cursor.List->SubscribeChanged(
				[weakState](const CollectionChangedEventArgs&)
				{
					auto state = weakState.lock();
					if (!state || !state->Owner) return;
					state->Owner->OnSourcePathChanged();
				});
		}
		if (connection.Connected())
			_sourceConnections.push_back(std::move(connection));

		if (index + 1 == _sourcePath.size())
			break;

		BindingValue value;
		if (!TryReadBindingPathStep(cursor, _sourcePath[index], value)
			|| !SetBindingPathCursor(value, cursor))
			break;
	}
	_sourcePathOwners = std::move(cursor.SourceOwners);
	_sourcePathListOwners = std::move(cursor.ListOwners);
#endif
}

void Binding::AttachValidationChangedHandlers()
{
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	auto* const adapterSource = AdapterSource();
	_sourceValidationConnections.clear();
	_validationPathConnections.clear();
	_validationPathOwners.clear();
	_validationPathListOwners.clear();
	if (!IsSourceAlive()) return;
	if (directSource)
	{
		if (!directSource.Ops->SubscribeValidation) return;
		std::weak_ptr<State> weakState = _state;
		auto connection = directSource.Ops->SubscribeValidation(
			directSource,
			[weakState]
			{
				auto state = weakState.lock();
				if (!state || !state->Owner) return;
				state->Owner->RefreshValidation();
			});
		if (connection.Connected())
			_sourceValidationConnections.push_back(std::move(connection));
		return;
	}
	if (!sourcePath.Empty())
	{
		BindingPathCursor cursor;
		cursor.Source = adapterSource;
		for (size_t index = 0;
			index < sourcePath.Steps.size(); ++index)
		{
			const auto& step = sourcePath.Steps[index];
			CompiledSourceHandle endpoint;
			if (cursor.Source
				&& step.Kind == CompiledBindingPathStepKind::Property)
			{
				endpoint = ResolveCompiledBindingPathEndpoint(cursor, step);
				if (!endpoint)
				{
					_isValid = false;
					_lastError = BindingError::SourceNotReadable;
					return;
				}
				if (endpoint.Ops->SubscribeValidation)
				{
					std::weak_ptr<State> weakState = _state;
					auto connection = endpoint.Ops->SubscribeValidation(
						endpoint,
						[weakState]()
						{
							auto state = weakState.lock();
							if (!state || !state->Owner) return;
							state->Owner->RefreshValidation();
						});
					if (connection.Connected())
						_sourceValidationConnections.push_back(
							std::move(connection));
				}
			}

			if (index + 1 == sourcePath.Steps.size()) break;
			if (_mode == BindingMode::OneWayToSource
				|| _mode == BindingMode::OneTime)
			{
				std::weak_ptr<State> weakState = _state;
				EventConnection connection;
				if (cursor.Source
					&& step.Kind == CompiledBindingPathStepKind::Property)
				{
					if (endpoint && endpoint.Ops->Subscribe)
						connection = endpoint.Ops->Subscribe(
							endpoint,
							[weakState]()
							{
								auto state = weakState.lock();
								if (!state || !state->Owner) return;
								state->Owner->OnValidationPathChanged();
							});
				}
				else if (cursor.List
					&& step.Kind == CompiledBindingPathStepKind::ListIndex)
				{
					connection = cursor.List->SubscribeChanged(
						[weakState](const CollectionChangedEventArgs&)
						{
							auto state = weakState.lock();
							if (!state || !state->Owner) return;
							state->Owner->OnValidationPathChanged();
						});
				}
				if (connection.Connected())
					_validationPathConnections.push_back(std::move(connection));
			}

			BindingValue value;
			if (!TryReadCompiledBindingPathStep(cursor, step, value)
				|| !SetBindingPathCursor(value, cursor)) break;
		}
		_validationPathOwners = std::move(cursor.SourceOwners);
		_validationPathListOwners = std::move(cursor.ListOwners);
		return;
	}
	if (_sourceProperty.IsCompiled())
	{
		if (auto* validationChanged = adapterSource->ValidationChanged())
		{
			std::weak_ptr<State> weakState = _state;
			const auto expectedToken =
				_sourceProperty.Identity()->BindingSourceToken();
			auto connection = validationChanged->Subscribe(
				[weakState, expectedToken](
					const BindingValidationChangedEventArgs& e)
				{
					if (e.PropertyToken
						&& e.PropertyToken != expectedToken) return;
					auto state = weakState.lock();
					if (!state || !state->Owner) return;
					state->Owner->RefreshValidation();
				});
			if (connection.Connected())
				_sourceValidationConnections.push_back(std::move(connection));
		}
		return;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	if (_sourcePath.empty()) return;

	BindingPathCursor cursor;
	cursor.Source = adapterSource;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		const std::wstring expectedProperty = _sourcePath[index].Value;
		const auto expectedToken =
			MakeBindingSourcePropertyToken(expectedProperty);
		if (cursor.Source)
		{
			if (auto* validationChanged = cursor.Source->ValidationChanged())
			{
				std::weak_ptr<State> weakState = _state;
				auto connection = validationChanged->Subscribe(
					[weakState, expectedProperty, expectedToken](
						const BindingValidationChangedEventArgs& e)
					{
						if (e.PropertyToken
							? e.PropertyToken != expectedToken
							: (!e.PropertyName.empty()
								&& !IsSameProperty(e.PropertyName, expectedProperty))) return;
						auto state = weakState.lock();
						if (!state || !state->Owner) return;
						state->Owner->RefreshValidation();
					});
				if (connection.Connected())
					_sourceValidationConnections.push_back(std::move(connection));
			}
		}

		if (index + 1 == _sourcePath.size()) break;

		if (_mode == BindingMode::OneWayToSource || _mode == BindingMode::OneTime)
		{
			std::weak_ptr<State> weakState = _state;
			EventConnection connection;
			if (cursor.Source)
				connection = cursor.Source->PropertyChanged().Subscribe(
					[weakState, expectedProperty, expectedToken](
						const PropertyChangedEventArgs& e)
					{
						if (e.PropertyToken
							? e.PropertyToken != expectedToken
							: (!e.PropertyName.empty()
								&& !IsSameProperty(e.PropertyName, expectedProperty))) return;
						auto state = weakState.lock();
						if (!state || !state->Owner) return;
						state->Owner->OnValidationPathChanged();
					});
			else if (cursor.List)
				connection = cursor.List->SubscribeChanged(
					[weakState](const CollectionChangedEventArgs&)
					{
						auto state = weakState.lock();
						if (!state || !state->Owner) return;
						state->Owner->OnValidationPathChanged();
					});
			if (connection.Connected())
				_validationPathConnections.push_back(std::move(connection));
		}

		BindingValue value;
		if (!TryReadBindingPathStep(cursor, _sourcePath[index], value)
			|| !SetBindingPathCursor(value, cursor))
			break;
	}
	_validationPathOwners = std::move(cursor.SourceOwners);
	_validationPathListOwners = std::move(cursor.ListOwners);
#endif
}

void Binding::AttachTargetChangedHandlers()
{
	if (!IsTargetAlive() || !_targetMetadata || !IsTargetToSourceMode(_mode)
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
	if (DirectSource())
	{
		if (!ValidateSourceMetadata())
		{
			const auto contractError = _lastError;
			RefreshValidation();
			if (IsSourceToTargetMode(_mode))
				(void)ApplyFallbackValue(contractError);
			return;
		}
		RefreshValidation();
		UpdateTarget();
		return;
	}
	if (!CompiledSourcePath().Empty() && !ValidateSourceMetadata())
	{
		const auto contractError = _lastError;
		RefreshValidation();
		if (IsSourceToTargetMode(_mode))
			(void)ApplyFallbackValue(contractError);
		return;
	}
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
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	auto* const adapterSource = AdapterSource();
	std::vector<BindingValidationIssue> next;
	if (IsSourceAlive())
	{
		if (directSource)
		{
			if (directSource.Ops->Validation)
				next = directSource.Ops->Validation(directSource);
		}
		else if (!sourcePath.Empty())
			next = GetBindingValidationIssuesForPath(
				*adapterSource, sourcePath);
		else if (_sourceProperty.IsCompiled())
		{
			const CompiledBindingPathStep sourceStep{
				CompiledBindingPathStepKind::Property,
				CompiledBindingPathCapabilities::Read
					| CompiledBindingPathCapabilities::Observe,
				_sourceProperty.Identity()->ValueKind(),
				_sourceProperty.Identity()->BindingSourceToken(), 0 };
			next = GetBindingValidationIssuesForPath(*adapterSource,
				CompiledBindingPathView{ std::span{ &sourceStep, size_t{ 1 } } });
		}
#if CUI_ENABLE_DYNAMIC_XAML
		else
			next = GetBindingValidationIssuesForPath(
				*adapterSource, SourceProperty());
#endif
	}
	if (next == _validationIssues) return;
	_validationIssues = std::move(next);
	if (directSource)
		_validationChanged.Notify(BindingSourcePropertyToken{});
	else if (!sourcePath.Empty()
		&& sourcePath.Steps.back().Kind
			== CompiledBindingPathStepKind::Property)
		_validationChanged.Notify(sourcePath.Steps.back().Property);
	else if (_sourceProperty.IsCompiled())
		_validationChanged.Notify(
			_sourceProperty.Identity()->BindingSourceToken());
#if CUI_ENABLE_DYNAMIC_XAML
	else
		_validationChanged.Notify(SourceProperty());
#endif
}

void Binding::OnTargetPropertyChanged()
{
	if (_ownsTargetValue && IsTargetAlive()
		&& (!_targetMetadata || !_target->IsBindingExpressionOwner(
			*_targetMetadata, this, _targetValueSource, _expressionKind)))
		return;
	UpdateSource();
}

bool Binding::UpdateTarget()
{
	if (!_isValid || !IsTargetAlive()
		|| !_targetMetadata || !_targetMetadata->CanWrite()
		|| !IsSourceToTargetMode(_mode) || _updatingSource)
		return false;
	if (!IsSourceAlive())
		return ApplyFallbackValue(BindingError::SourceUnavailable);

	BindingValue sourceValue;
	BindingError sourceError = BindingError::None;
	if (!TryReadSourcePathValue(sourceValue, sourceError))
		return ApplyFallbackValue(sourceError);
	if (sourceValue.Empty() && _targetNullValue)
	{
		if (ApplyTargetValue(*_targetNullValue)) return true;
		if (_lastError == BindingError::TargetWriteFailed) return false;
		return ApplyFallbackValue(BindingError::TargetConversionFailed);
	}

	BindingValue value = sourceValue;
	if (_converter)
	{
		const BindingValueConverterContext context{
			_converterParameter ? &*_converterParameter : nullptr,
			_targetMetadata->ValueKind() };
		if (!_converter->Convert(sourceValue, context, value))
			return ApplyFallbackValue(BindingError::TargetConversionFailed);
	}
	if (_stringFormat)
	{
		std::wstring formatted;
		if (!TryFormatBindingValue(value, *_stringFormat, formatted))
			return ApplyFallbackValue(BindingError::StringFormatFailed);
		value = BindingValue(std::move(formatted));
	}
	if (ApplyTargetValue(value)) return true;
	if (_lastError == BindingError::TargetWriteFailed) return false;
	return ApplyFallbackValue(BindingError::TargetConversionFailed);
}

bool Binding::ApplyTargetValue(const BindingValue& value)
{
	if (!IsTargetAlive() || !_targetMetadata) return false;
	BindingValue converted;
	if (!_targetMetadata->TryConvert(value, converted))
		return Fail(BindingError::TargetConversionFailed);
	_updatingTarget = true;
	const bool ok = _target->TrySetBindingPropertyValue(
		*_targetMetadata, converted, this,
		_targetValueSource, _expressionKind);
	_updatingTarget = false;
	if (!IsTargetAlive())
		return Fail(BindingError::InvalidTarget);
	if (!ok) return Fail(BindingError::TargetWriteFailed);
	_lastError = BindingError::None;
	return true;
}

void Binding::DetachReplacedTargetExpression() noexcept
{
	_targetConnection.Disconnect();
	_sourceConnections.clear();
	_sourcePathOwners.clear();
	_sourcePathListOwners.clear();
	_sourceValidationConnections.clear();
	_validationPathConnections.clear();
	_validationPathOwners.clear();
	_validationPathListOwners.clear();
	if (_state) _state->Owner = nullptr;
	_ownsTargetValue = false;
	_isValid = false;
}

bool Binding::ApplyFallbackValue(BindingError sourceError)
{
	if (!_fallbackValue) return Fail(sourceError);
	if (ApplyTargetValue(*_fallbackValue)) return true;
	return false;
}

bool Binding::UpdateSource()
{
	const auto directSource = DirectSource();
	if (!_isValid || !IsTargetAlive()
		|| !_targetMetadata || !_targetMetadata->CanRead()
		|| !IsTargetToSourceMode(_mode) || _updatingTarget)
		return false;
	if (!IsSourceAlive())
		return Fail(BindingError::SourceUnavailable);

	BindingValue value;
	if (!_targetMetadata->TryGet(*_target, value))
		return Fail(BindingError::TargetReadFailed);
	BindingValue sourceValue;
	BindingError sourceError = BindingError::None;
	const bool hasSourceValue = TryReadSourcePathValue(sourceValue, sourceError);
	if (!hasSourceValue && sourceError == BindingError::SourcePathUnresolved)
		return Fail(sourceError);

	if (_converter)
	{
		BindingValue converted;
		const BindingValueConverterContext context{
			_converterParameter ? &*_converterParameter : nullptr,
			hasSourceValue ? sourceValue.Kind()
				: (directSource
					? directSource.ValueKind() : BindingValueKind::Empty) };
		if (!_converter->ConvertBack(value, context, converted))
			return Fail(BindingError::SourceConversionFailed);
		value = std::move(converted);
	}
	else if (directSource
		&& !hasSourceValue
		&& directSource.ValueKind() != BindingValueKind::Empty)
	{
		BindingValue converted;
		if (!TryConvertBindingValue(
			value, directSource.ValueKind(), converted))
			return Fail(BindingError::SourceConversionFailed);
		value = std::move(converted);
	}

	if (hasSourceValue)
	{
		BindingValue converted;
		if (!TryConvertBindingValue(value, sourceValue, converted))
			return Fail(BindingError::SourceConversionFailed);
		value = std::move(converted);
	}

	_updatingSource = true;
	bool ok = TryWriteSourcePathValue(value, sourceError);
	_updatingSource = false;
	if (!ok) return Fail(sourceError);
	_lastError = BindingError::None;
	return true;
}

bool Binding::TryReadSourcePathValue(
	BindingValue& out,
	BindingError& error) const
{
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	auto* const adapterSource = AdapterSource();
	error = BindingError::None;
	if (!IsSourceAlive())
	{
		error = BindingError::SourceUnavailable;
		return false;
	}
	if (directSource)
	{
		if (!directSource.Ops->Read
			|| !directSource.Ops->Read(directSource, out))
		{
			error = BindingError::SourceReadFailed;
			return false;
		}
		return true;
	}
	if (!sourcePath.Empty())
	{
		BindingPathCursor cursor;
		if (!ResolveCompiledBindingPathOwner(
			*adapterSource, sourcePath, cursor))
		{
			error = BindingError::SourcePathUnresolved;
			return false;
		}
		if (!TryReadCompiledBindingPathStep(
			cursor, sourcePath.Steps.back(), out))
		{
			error = BindingError::SourceReadFailed;
			return false;
		}
		return true;
	}
	if (_sourceProperty.IsCompiled())
	{
		if (!_sourceDependencyObject || !_sourceMetadata
			|| !_sourceMetadata->TryGet(*_sourceDependencyObject, out))
		{
			error = BindingError::SourceReadFailed;
			return false;
		}
		return true;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	if (_sourcePath.empty())
	{
		error = BindingError::SourceUnavailable;
		return false;
	}
	BindingPathCursor cursor;
	if (!ResolveBindingPathOwner(*adapterSource, _sourcePath, cursor))
	{
		error = BindingError::SourcePathUnresolved;
		return false;
	}
	if (!TryReadBindingPathStep(cursor, _sourcePath.back(), out))
	{
		error = BindingError::SourceReadFailed;
		return false;
	}
	return true;
#else
	error = BindingError::InvalidSourcePropertyPath;
	return false;
#endif
}

bool Binding::TryWriteSourcePathValue(
	const BindingValue& value,
	BindingError& error) const
{
	const auto directSource = DirectSource();
	const auto sourcePath = CompiledSourcePath();
	auto* const adapterSource = AdapterSource();
	error = BindingError::None;
	if (!IsSourceAlive())
	{
		error = BindingError::SourceUnavailable;
		return false;
	}
	if (directSource)
	{
		if (!directSource.Ops->Write
			|| !directSource.Ops->Write(directSource, value))
		{
			error = BindingError::SourceWriteFailed;
			return false;
		}
		return true;
	}
	if (!sourcePath.Empty())
	{
		BindingPathCursor cursor;
		if (!ResolveCompiledBindingPathOwner(
			*adapterSource, sourcePath, cursor))
		{
			error = BindingError::SourcePathUnresolved;
			return false;
		}
		const auto& leaf = sourcePath.Steps.back();
		const auto endpoint = ResolveCompiledBindingPathEndpoint(cursor, leaf);
		if (!endpoint || !endpoint.Ops->Write
			|| !endpoint.Ops->Write(endpoint, value))
		{
			error = BindingError::SourceWriteFailed;
			return false;
		}
		return true;
	}
	if (_sourceProperty.IsCompiled())
	{
		if (!_sourceDependencyObject || !_sourceMetadata
			|| !_sourceMetadata->TrySet(*_sourceDependencyObject, value))
		{
			error = BindingError::SourceWriteFailed;
			return false;
		}
		return true;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	if (_sourcePath.empty())
	{
		error = BindingError::SourceUnavailable;
		return false;
	}
	BindingPathCursor cursor;
	if (!ResolveBindingPathOwner(*adapterSource, _sourcePath, cursor))
	{
		error = BindingError::SourcePathUnresolved;
		return false;
	}
	if (!cursor.Source
		|| !cursor.Source->TrySetValue(_sourcePath.back().Value, value))
	{
		error = BindingError::SourceWriteFailed;
		return false;
	}
	return true;
#else
	error = BindingError::InvalidSourcePropertyPath;
	return false;
#endif
}

#if CUI_ENABLE_DYNAMIC_XAML
MultiBindingSource::MultiBindingSource(
	IBindingSource* source,
	std::wstring sourceProperty,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: _sourceStorage(source, sourceProperty.empty()
		? BindingRootValuePath() : CompiledBindingPathView{}),
	  SourceProperty(std::move(sourceProperty)),
	  Converter(std::move(converter)),
	  FallbackValue(std::move(fallbackValue)),
	  TargetNullValue(std::move(targetNullValue)),
	  ConverterParameter(std::move(converterParameter)),
	  StringFormat(std::move(stringFormat))
{
}

MultiBindingSource::MultiBindingSource(
	BindingSourceReference source,
	std::wstring sourceProperty,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: _sourceStorage(std::move(source), sourceProperty.empty()
		? BindingRootValuePath() : CompiledBindingPathView{}),
	  SourceProperty(std::move(sourceProperty)),
	  Converter(std::move(converter)),
	  FallbackValue(std::move(fallbackValue)),
	  TargetNullValue(std::move(targetNullValue)),
	  ConverterParameter(std::move(converterParameter)),
	  StringFormat(std::move(stringFormat))
{
}
#endif

MultiBindingSource::MultiBindingSource(
	IBindingSource* source,
	CompiledBindingPathView sourcePath,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: _sourceStorage(source, sourcePath),
	  Converter(std::move(converter)),
	  FallbackValue(std::move(fallbackValue)),
	  TargetNullValue(std::move(targetNullValue)),
	  ConverterParameter(std::move(converterParameter)),
	  StringFormat(std::move(stringFormat))
{
}

MultiBindingSource::MultiBindingSource(
	BindingSourceReference source,
	CompiledBindingPathView sourcePath,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: _sourceStorage(std::move(source), sourcePath),
	  Converter(std::move(converter)),
	  FallbackValue(std::move(fallbackValue)),
	  TargetNullValue(std::move(targetNullValue)),
	  ConverterParameter(std::move(converterParameter)),
	  StringFormat(std::move(stringFormat))
{
}

MultiBindingSource::MultiBindingSource(
	CompiledSourceHandle source,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: _sourceStorage(source),
	  Converter(std::move(converter)),
	  FallbackValue(std::move(fallbackValue)),
	  TargetNullValue(std::move(targetNullValue)),
	  ConverterParameter(std::move(converterParameter)),
	  StringFormat(std::move(stringFormat))
{
}

namespace
{
	struct MultiBindingSlotState final
	{
		bool HasValue = false;
		BindingValue Value;
	};

	struct MultiBindingResultState final
	{
		bool HasValue = false;
		BindingValue Value;
	};

	class MultiBindingSlotConverter final : public IBindingValueConverter
	{
	public:
		MultiBindingSlotConverter(
			std::shared_ptr<const IBindingValueConverter> inner,
			std::optional<std::wstring> stringFormat)
			: _inner(std::move(inner)), _stringFormat(std::move(stringFormat))
		{
		}

		bool Convert(
			const BindingValue& value,
			const BindingValueConverterContext& context,
			BindingValue& out) const override
		{
			BindingValue converted = value;
			const BindingValueConverterContext innerContext{
				context.Parameter, BindingValueKind::Empty };
			if (_inner && !_inner->Convert(value, innerContext, converted))
				return false;
			if (_stringFormat)
			{
				std::wstring formatted;
				if (!TryFormatBindingValue(converted, *_stringFormat, formatted))
					return false;
				converted = BindingValue(std::move(formatted));
			}
			out = BindingValue(MultiBindingSlotState{
				true, std::move(converted) });
			return true;
		}

		bool ConvertBack(
			const BindingValue& value,
			const BindingValueConverterContext& context,
			BindingValue& out) const override
		{
			MultiBindingSlotState state;
			if (!value.TryGet(state) || !state.HasValue) return false;
			if (!_inner)
			{
				out = std::move(state.Value);
				return true;
			}
			const BindingValueConverterContext innerContext{
				context.Parameter, context.TargetKind };
			return _inner->ConvertBack(state.Value, innerContext, out);
		}

	private:
		std::shared_ptr<const IBindingValueConverter> _inner;
		std::optional<std::wstring> _stringFormat;
	};

	class MultiBindingResultConverter final : public IBindingValueConverter
	{
	public:
		bool Convert(
			const BindingValue& value,
			const BindingValueConverterContext&,
			BindingValue& out) const override
		{
			MultiBindingResultState state;
			if (!value.TryGet(state) || !state.HasValue) return false;
			out = std::move(state.Value);
			return true;
		}

		bool ConvertBack(
			const BindingValue& value,
			const BindingValueConverterContext&,
			BindingValue& out) const override
		{
			out = BindingValue(MultiBindingResultState{ true, value });
			return true;
		}
	};

	BindingValue WrapMultiBindingSlotValue(const BindingValue& value)
	{
		return BindingValue(MultiBindingSlotState{ true, value });
	}

	class MultiBindingEndpoint final : public DependencyObject
	{
	public:
		static const DependencyProperty& ValueProperty()
		{
			static const auto registration = []
			{
				DependencyPropertyOptions<
					MultiBindingEndpoint, BindingValue> options;
				options.DefaultValue = BindingValue{};
				return DependencyPropertyRegistry::RegisterStatic<
					MultiBindingEndpoint, BindingValue>(
						DependencyPropertyRegistrationLiteral(L"Value"),
						std::move(options));
			}();
			return *registration;
		}

#if CUI_ENABLE_DYNAMIC_XAML
		void EnsureBindingPropertiesRegistered() override
		{
			(void)ValueProperty();
		}
#endif
	};
}

struct MultiBinding::State final
{
	DependencyPropertyReference TargetProperty;
	BindingMode Mode = BindingMode::Default;
	DataSourceUpdateMode UpdateMode = DataSourceUpdateMode::Default;
	std::shared_ptr<const IMultiBindingValueConverter> Converter;
	std::optional<BindingValue> TargetNullValue;
	std::optional<BindingValue> ConverterParameter;
	std::optional<std::wstring> StringFormat;
	BindingValueKind TargetKind = BindingValueKind::Empty;
	MultiBindingEndpoint ResultEndpoint;
	std::vector<std::unique_ptr<MultiBindingEndpoint>> SlotEndpoints;
	std::vector<std::unique_ptr<Binding>> ChildBindings;
	std::unique_ptr<Binding> TargetExpression;
	EventConnection ResultConnection;
	std::vector<EventConnection> SlotConnections;
	std::vector<EventConnection> ChildValidationConnections;
	BindingValidationChangedEvent Validation;
	BindingError Error = BindingError::None;
	bool Initializing = true;
	bool Recomputing = false;
	bool WritingBack = false;
	bool ManualUpdateSource = false;

	void SetResult(MultiBindingResultState state)
	{
		Recomputing = true;
		(void)ResultEndpoint.TrySetPropertyValue(
			MultiBindingEndpoint::ValueProperty(),
			BindingValue(std::move(state)));
		Recomputing = false;
	}

	void Recompute()
	{
		if (Initializing || WritingBack) return;
		std::vector<BindingValue> values;
		values.reserve(SlotEndpoints.size());
		for (const auto& endpoint : SlotEndpoints)
		{
			BindingValue wrapped;
			MultiBindingSlotState state;
			if (!endpoint
				|| !endpoint->TryGetPropertyValue(
					MultiBindingEndpoint::ValueProperty(), wrapped)
				|| !wrapped.TryGet(state) || !state.HasValue)
			{
				Error = BindingError::SourceUnavailable;
				SetResult({});
				return;
			}
			values.push_back(std::move(state.Value));
		}

		BindingValue result;
		if (Converter)
		{
			const MultiBindingValueConverterContext context{
				ConverterParameter ? &*ConverterParameter : nullptr,
				TargetKind };
			if (!Converter->Convert(values, context, result))
			{
				Error = BindingError::MultiBindingConverterFailed;
				SetResult({});
				return;
			}
			if (StringFormat)
			{
				std::wstring formatted;
				if (!TryFormatBindingValue(result, *StringFormat, formatted))
				{
					Error = BindingError::StringFormatFailed;
					SetResult({});
					return;
				}
				result = BindingValue(std::move(formatted));
			}
		}
		else
		{
			std::wstring formatted;
			if (!StringFormat
				|| !TryFormatBindingValues(values, *StringFormat, formatted))
			{
				Error = BindingError::StringFormatFailed;
				SetResult({});
				return;
			}
			result = BindingValue(std::move(formatted));
		}
		if (result.Empty() && TargetNullValue)
			result = *TargetNullValue;
		Error = BindingError::None;
		SetResult({ true, std::move(result) });
	}

	void WriteBack(const MultiBindingResultState& state)
	{
		if (!IsTargetToSourceMode(Mode) || Recomputing || WritingBack
			|| !state.HasValue || !Converter) return;
		const MultiBindingValueConverterContext context{
			ConverterParameter ? &*ConverterParameter : nullptr,
			TargetKind };
		std::vector<BindingValue> values;
		if (!Converter->ConvertBack(
			state.Value, SlotEndpoints.size(), context, values)
			|| values.size() != SlotEndpoints.size())
		{
			Error = BindingError::MultiBindingConverterFailed;
			return;
		}
		WritingBack = true;
		bool success = true;
		BindingError writeError = BindingError::SourceWriteFailed;
		for (size_t index = 0; index < values.size(); ++index)
		{
			if (!SlotEndpoints[index]
				|| !SlotEndpoints[index]->TrySetCurrentPropertyValue(
					MultiBindingEndpoint::ValueProperty(),
					WrapMultiBindingSlotValue(values[index])))
			{
				success = false;
				break;
			}
		}
		for (size_t index = 0;
			success && index < ChildBindings.size(); ++index)
		{
			auto* child = ChildBindings[index].get();
			if (!child || !IsTargetToSourceMode(child->Mode())) continue;
			const bool mustCommit = child->UpdateMode()
				== DataSourceUpdateMode::OnValidation
				|| (child->UpdateMode() == DataSourceUpdateMode::Never
					&& (ManualUpdateSource
						|| UpdateMode == DataSourceUpdateMode::Never));
			if (mustCommit && !child->UpdateSource())
			{
				success = false;
				writeError = child->LastError() == BindingError::None
					? BindingError::SourceWriteFailed : child->LastError();
			}
			else if (!mustCommit && child->UpdateMode()
				== DataSourceUpdateMode::OnPropertyChanged
				&& child->LastError() != BindingError::None)
			{
				success = false;
				writeError = child->LastError();
			}
		}
		WritingBack = false;
		Error = BindingError::None;
		Recompute();
		if (!success) Error = writeError;
	}

	bool UpdateTarget()
	{
		if (!TargetExpression || !IsSourceToTargetMode(Mode)) return false;
		const bool previousInitializing = Initializing;
		Initializing = true;
		BindingError childError = BindingError::None;
		for (const auto& childBinding : ChildBindings)
		{
			auto* child = childBinding.get();
			if (!child || !IsSourceToTargetMode(child->Mode())) continue;
			if (!child->UpdateTarget() && childError == BindingError::None)
				childError = child->LastError() == BindingError::None
					? BindingError::SourceReadFailed : child->LastError();
		}
		Initializing = previousInitializing;
		if (childError != BindingError::None)
		{
			Error = childError;
			return false;
		}
		Error = BindingError::None;
		Recompute();
		if (Error != BindingError::None) return false;
		if (!TargetExpression->UpdateTarget())
		{
			Error = TargetExpression->LastError() == BindingError::None
				? BindingError::TargetWriteFailed : TargetExpression->LastError();
			return false;
		}
		return true;
	}

	bool UpdateSource()
	{
		if (!TargetExpression || !IsTargetToSourceMode(Mode)) return false;
		Error = BindingError::None;
		ManualUpdateSource = true;
		const bool updated = TargetExpression->UpdateSource();
		ManualUpdateSource = false;
		if (!updated && Error == BindingError::None)
			Error = TargetExpression->LastError() == BindingError::None
				? BindingError::SourceWriteFailed : TargetExpression->LastError();
		return updated && Error == BindingError::None;
	}
};

#if CUI_ENABLE_DYNAMIC_XAML
MultiBinding::MultiBinding(
	DependencyObject* target,
	std::wstring targetProperty,
	std::vector<MultiBindingSource> sources,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IMultiBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: MultiBinding(target,
		DependencyPropertyReference(std::move(targetProperty)),
		std::move(sources), mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat))
{
}

#endif

MultiBinding::MultiBinding(
	DependencyObject* target,
	const DependencyProperty& targetProperty,
	std::vector<MultiBindingSource> sources,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IMultiBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: MultiBinding(target, DependencyPropertyReference(targetProperty),
		std::move(sources), mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat))
{
}

MultiBinding::MultiBinding(
	DependencyObject* target,
	DependencyPropertyReference targetProperty,
	std::vector<MultiBindingSource> sources,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IMultiBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: _state(std::make_shared<State>())
{
	auto& state = *_state;
	state.TargetProperty = std::move(targetProperty);
	state.Mode = mode;
	state.UpdateMode = updateMode;
	state.Converter = std::move(converter);
	state.TargetNullValue = std::move(targetNullValue);
	state.ConverterParameter = std::move(converterParameter);
	state.StringFormat = std::move(stringFormat);
	if (!target || state.TargetProperty.Empty() || sources.size() < 2)
	{
		state.Error = BindingError::InvalidMultiBinding;
		return;
	}
	const DependencyPropertyMetadata* targetMetadata = nullptr;
	if (state.TargetProperty.IsCompiled())
	{
		targetMetadata = target->GetPropertyMetadata(
			*state.TargetProperty.Identity());
	}
#if CUI_ENABLE_DYNAMIC_XAML
	else
	{
		targetMetadata = target->FindPropertyMetadata(
			state.TargetProperty.Name());
	}
#endif
	if (!targetMetadata)
	{
		state.Error = BindingError::TargetPropertyNotFound;
		return;
	}
	state.TargetProperty = DependencyPropertyReference(
		targetMetadata->Property());
	if (targetMetadata->IsReadOnly())
	{
		state.Error = BindingError::TargetNotWritable;
		return;
	}
	state.Mode = ResolveBindingMode(*targetMetadata, state.Mode);
	state.UpdateMode = ResolveDataSourceUpdateMode(
		*targetMetadata, state.UpdateMode);
	state.TargetKind = targetMetadata->ValueKind();
	if (!state.Converter && !state.StringFormat)
	{
		state.Error = BindingError::InvalidMultiBinding;
		return;
	}
	if (IsTargetToSourceMode(state.Mode) && !state.Converter)
	{
		state.Error = BindingError::InvalidMultiBinding;
		return;
	}
	if (state.StringFormat
		&& (state.TargetKind != BindingValueKind::String
			|| (state.Converter
				? !IsValidBindingStringFormat(*state.StringFormat)
				: !IsValidMultiBindingStringFormat(
					*state.StringFormat, sources.size()))))
	{
		state.Error = BindingError::InvalidStringFormat;
		return;
	}

	state.SlotEndpoints.reserve(sources.size());
	state.ChildBindings.reserve(sources.size());
	state.SlotConnections.reserve(sources.size());
	state.ChildValidationConnections.reserve(sources.size());
	for (size_t index = 0; index < sources.size(); ++index)
	{
		auto& source = sources[index];
		const auto directSource = source.DirectSource();
		const auto sourcePath = source.SourcePath();
		bool missingSourcePath = sourcePath.Empty();
#if CUI_ENABLE_DYNAMIC_XAML
		missingSourcePath = missingSourcePath && source.SourceProperty.empty();
#endif
		if ((!directSource && !source.AdapterSource())
			|| (!directSource && missingSourcePath)
			|| (source.StringFormat
				&& !IsValidBindingStringFormat(*source.StringFormat)))
		{
			state.Error = BindingError::InvalidMultiBinding;
			return;
		}
		state.SlotEndpoints.push_back(
			std::make_unique<MultiBindingEndpoint>());
	}

	const std::weak_ptr<State> weakState = _state;
	state.ResultConnection =
		state.ResultEndpoint.OnPropertyValueChanged.Subscribe(
		[weakState](DependencyObject*,
			const DependencyPropertyChangedEventArgs& args)
		{
			auto state = weakState.lock();
			if (!state) return;
			if (args.Property == &MultiBindingEndpoint::ValueProperty())
			{
				MultiBindingResultState result;
				if (args.NewValue.TryGet(result)) state->WriteBack(result);
			}
		});
	for (const auto& endpoint : state.SlotEndpoints)
	{
		state.SlotConnections.push_back(
			endpoint->OnPropertyValueChanged.Subscribe(
			[weakState](DependencyObject*,
				const DependencyPropertyChangedEventArgs&)
			{
				if (auto state = weakState.lock()) state->Recompute();
			}));
	}

	auto forwardValidation = [weakState](
		const BindingValidationChangedEventArgs&)
		{
			if (auto state = weakState.lock())
#if CUI_ENABLE_DYNAMIC_XAML
				state->Validation.Notify(state->TargetProperty.Name());
#else
				state->Validation.Notify(
					state->TargetProperty.Identity()->BindingSourceToken());
#endif
		};

	for (size_t index = 0; index < sources.size(); ++index)
	{
		auto& source = sources[index];
		const auto directSource = source.DirectSource();
		const auto sourcePath = source.SourcePath();
		auto* const adapterSource = source.AdapterSource();
		const bool ownsSource = source.OwnedSource() != nullptr;
		const auto requestedChildMode = source.Mode.value_or(state.Mode);
		const auto childMode = requestedChildMode == BindingMode::Default
			? state.Mode : requestedChildMode;
		const auto requestedChildUpdateMode = source.UpdateMode.value_or(
			state.UpdateMode);
		const auto childUpdateMode = requestedChildUpdateMode
			== DataSourceUpdateMode::Default
			? state.UpdateMode : requestedChildUpdateMode;
		auto slotConverter = std::make_shared<MultiBindingSlotConverter>(
			std::move(source.Converter), std::move(source.StringFormat));
		std::optional<BindingValue> slotFallback = BindingValue(
			MultiBindingSlotState{});
		if (source.FallbackValue)
			slotFallback = WrapMultiBindingSlotValue(*source.FallbackValue);
		std::optional<BindingValue> slotNull;
		if (source.TargetNullValue)
			slotNull = WrapMultiBindingSlotValue(*source.TargetNullValue);
		std::unique_ptr<Binding> child;
		if (directSource)
		{
			child = std::make_unique<Binding>(
				state.SlotEndpoints[index].get(),
				MultiBindingEndpoint::ValueProperty(), directSource,
				childMode, childUpdateMode, std::move(slotConverter),
				std::move(slotFallback), std::move(slotNull),
				std::move(source.ConverterParameter));
		}
		else if (ownsSource)
		{
			if (!sourcePath.Empty())
				child = std::make_unique<Binding>(
					state.SlotEndpoints[index].get(),
					MultiBindingEndpoint::ValueProperty(),
					source.TakeOwnedSource(), sourcePath,
					childMode, childUpdateMode, std::move(slotConverter),
					std::move(slotFallback), std::move(slotNull),
					std::move(source.ConverterParameter));
#if CUI_ENABLE_DYNAMIC_XAML
			else
				child = std::make_unique<Binding>(
					state.SlotEndpoints[index].get(),
					MultiBindingEndpoint::ValueProperty(),
					source.TakeOwnedSource(), source.SourceProperty,
					childMode, childUpdateMode, std::move(slotConverter),
					std::move(slotFallback), std::move(slotNull),
					std::move(source.ConverterParameter));
#endif
		}
		else if (!sourcePath.Empty())
			child = std::make_unique<Binding>(
				state.SlotEndpoints[index].get(),
				MultiBindingEndpoint::ValueProperty(), adapterSource,
				sourcePath, childMode, childUpdateMode,
				std::move(slotConverter), std::move(slotFallback),
				std::move(slotNull), std::move(source.ConverterParameter));
#if CUI_ENABLE_DYNAMIC_XAML
		else
			child = std::make_unique<Binding>(
				state.SlotEndpoints[index].get(),
				MultiBindingEndpoint::ValueProperty(), adapterSource,
				source.SourceProperty, childMode, childUpdateMode,
				std::move(slotConverter), std::move(slotFallback),
				std::move(slotNull), std::move(source.ConverterParameter));
#endif
		if (!child || !child->IsValid())
		{
			state.Error = child
				? child->LastError() : BindingError::InvalidMultiBinding;
			return;
		}
		auto validationConnection =
			child->ValidationChanged().Subscribe(forwardValidation);
		const bool hasValidationIssues = child->HasValidationIssues();
		state.ChildBindings.push_back(std::move(child));
		state.ChildValidationConnections.push_back(
			std::move(validationConnection));
		if (hasValidationIssues)
#if CUI_ENABLE_DYNAMIC_XAML
			state.Validation.Notify(state.TargetProperty.Name());
#else
			state.Validation.Notify(
				state.TargetProperty.Identity()->BindingSourceToken());
#endif
	}
	state.Initializing = false;
	state.Recompute();
	static constexpr CompiledBindingPathStep resultPathSteps[] = {
		{ CompiledBindingPathStepKind::Property,
			CompiledBindingPathCapabilities::Read
				| CompiledBindingPathCapabilities::Write
				| CompiledBindingPathCapabilities::Observe,
			BindingValueKind::Object,
			{}, 0,
			+[](IBindingSource& source) noexcept -> CompiledSourceHandle
			{
				return cui::binding::ResolveCompiledDependencyPropertySource(
					source, MultiBindingEndpoint::ValueProperty());
			} }
	};
	state.TargetExpression = std::make_unique<Binding>(
		target, *state.TargetProperty.Identity(), &state.ResultEndpoint,
		CompiledBindingPathView{ resultPathSteps },
		state.Mode, state.UpdateMode,
		std::make_shared<MultiBindingResultConverter>(),
		std::move(fallbackValue));
	if (!state.TargetExpression->IsValid())
	{
		state.Error = state.TargetExpression->LastError();
		state.TargetExpression.reset();
	}
}

MultiBinding::~MultiBinding() = default;

const std::wstring& MultiBinding::TargetProperty() const noexcept
{
	static const std::wstring empty;
	return _state ? _state->TargetProperty.Name() : empty;
}

const DependencyProperty* MultiBinding::TargetPropertyIdentity() const noexcept
{
	return _state ? _state->TargetProperty.Identity() : nullptr;
}

BindingMode MultiBinding::Mode() const noexcept
{
	return _state ? _state->Mode : BindingMode::Default;
}

DataSourceUpdateMode MultiBinding::UpdateMode() const noexcept
{
	return _state ? _state->UpdateMode : DataSourceUpdateMode::Default;
}

size_t MultiBinding::SourceCount() const noexcept
{
	return _state ? _state->ChildBindings.size() : 0;
}

bool MultiBinding::IsValid() const noexcept
{
	return _state && _state->TargetExpression
		&& _state->TargetExpression->IsValid();
}

BindingError MultiBinding::LastError() const noexcept
{
	if (!_state) return BindingError::InvalidMultiBinding;
	if (_state->Error != BindingError::None) return _state->Error;
	return _state->TargetExpression
		? _state->TargetExpression->LastError()
		: BindingError::InvalidMultiBinding;
}

const wchar_t* MultiBinding::LastErrorMessage() const noexcept
{
	return BindingErrorMessage(LastError());
}

Binding* MultiBinding::TargetBinding() noexcept
{
	return _state ? _state->TargetExpression.get() : nullptr;
}

const Binding* MultiBinding::TargetBinding() const noexcept
{
	return _state ? _state->TargetExpression.get() : nullptr;
}

bool MultiBinding::UpdateTarget()
{
	auto state = _state;
	return state && state->UpdateTarget();
}

bool MultiBinding::UpdateSource()
{
	auto state = _state;
	return state && state->UpdateSource();
}

std::vector<BindingValidationResult> MultiBinding::GetValidationResults() const
{
	std::vector<BindingValidationResult> result;
	auto state = _state;
	if (!state) return result;
	for (size_t index = 0; index < state->ChildBindings.size(); ++index)
	{
		const auto& child = state->ChildBindings[index];
		if (!child) continue;
		for (const auto& issue : child->ValidationIssues())
			result.push_back({
				state->TargetProperty.Name(),
				child->SourceProperty(),
				issue });
	}
	return result;
}

std::vector<BindingValidationIssue> GetBindingValidationIssuesForPath(
	const IBindingSource& source,
	CompiledBindingPathView sourcePropertyPath)
{
	if (sourcePropertyPath.Version != CompiledBindingPathVersion
		|| sourcePropertyPath.Empty()) return {};

	std::vector<BindingValidationIssue> result;
	auto append = [&result](std::vector<BindingValidationIssue> issues)
	{
		for (auto& issue : NormalizeValidationIssues(std::move(issues)))
		{
			if (std::find(result.begin(), result.end(), issue) == result.end())
				result.push_back(std::move(issue));
		}
	};

	BindingPathCursor cursor;
	cursor.Source = const_cast<IBindingSource*>(&source);
	for (size_t index = 0; index < sourcePropertyPath.Steps.size(); ++index)
	{
		const auto& step = sourcePropertyPath.Steps[index];
		if (cursor.Source)
		{
			append(cursor.Source->GetValidationIssues(
				BindingSourcePropertyToken{}));
			if (step.Kind == CompiledBindingPathStepKind::Property)
			{
				const auto endpoint = ResolveCompiledBindingPathEndpoint(
					cursor, step);
				if (!endpoint) break;
				if (endpoint.Ops->Validation)
					append(endpoint.Ops->Validation(endpoint));
			}
		}
		if (index + 1 == sourcePropertyPath.Steps.size()) break;

		BindingValue value;
		if (!TryReadCompiledBindingPathStep(cursor, step, value)
			|| !SetBindingPathCursor(value, cursor))
			break;
	}
	return result;
}

bool MultiBinding::HasValidationIssues() const
{
	auto state = _state;
	return state && std::any_of(
		state->ChildBindings.begin(), state->ChildBindings.end(),
		[](const auto& child)
		{
			return child && child->HasValidationIssues();
		});
}

bool MultiBinding::HasValidationErrors() const
{
	auto state = _state;
	return state && std::any_of(
		state->ChildBindings.begin(), state->ChildBindings.end(),
		[](const auto& child)
		{
			return child && child->HasValidationErrors();
		});
}

BindingValidationChangedEvent& MultiBinding::ValidationChanged() noexcept
{
	return _state->Validation;
}

BindingCollection::BindingCollection(DependencyObject* owner)
	: _owner(owner),
	  _callbackState(std::make_shared<CallbackState>())
{
	_callbackState->Owner = this;
}

BindingCollection::~BindingCollection()
{
	if (_callbackState) _callbackState->Owner = nullptr;
	_validationConnections.clear();
	_multiValidationConnections.clear();
	_items.clear();
	_multiItems.clear();
	_owner = nullptr;
}

void BindingCollection::NotifyValidationChanged(
	const std::wstring& targetProperty)
{
	auto* owner = _owner;
	const auto ownerLifetime = owner
		? owner->BindingLifetime() : std::weak_ptr<const void>{};
#if CUI_ENABLE_DYNAMIC_XAML
	_validationChanged.Notify(targetProperty);
#else
	_validationChanged.Notify(MakeBindingSourcePropertyToken(targetProperty));
#endif
	// Validation listeners may remove the complete binding collection. Do not
	// read this object again after publishing the snapshot.
	if (owner && !ownerLifetime.expired() && !owner->IsDestroying())
		owner->OnBindingValidationChanged(targetProperty);
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding* BindingCollection::Add(const std::wstring& targetProperty,
	IBindingSource* source,
	const std::wstring& sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
	const auto* targetMetadata = _owner->FindPropertyMetadata(targetProperty);
	const auto* targetIdentity = targetMetadata
		? &targetMetadata->Property() : nullptr;
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
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
		std::move(converter),
		std::move(fallbackValue),
		std::move(targetNullValue),
		std::move(converterParameter),
		std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(targetProperty);
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty);
	_lastError = BindingError::None;
	return result;
}
#endif

#if CUI_ENABLE_DYNAMIC_XAML
Binding* BindingCollection::Add(const DependencyProperty& targetProperty,
	IBindingSource* source,
	const std::wstring& sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner, targetProperty, source, sourceProperty,
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}
#endif

Binding* BindingCollection::Add(const DependencyProperty& targetProperty,
	IBindingSource* source,
	CompiledBindingPathView sourcePath,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner, targetProperty, source, sourcePath,
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}

Binding* BindingCollection::Add(const DependencyProperty& targetProperty,
	CompiledSourceHandle source,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner, targetProperty, source, mode, updateMode,
		std::move(converter), std::move(fallbackValue),
		std::move(targetNullValue), std::move(converterParameter),
		std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding* BindingCollection::Add(const std::wstring& targetProperty,
	BindingSourceReference source,
	const std::wstring& sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
	const auto* targetMetadata = _owner->FindPropertyMetadata(targetProperty);
	const auto* targetIdentity = targetMetadata
		? &targetMetadata->Property() : nullptr;
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner, targetProperty, std::move(source), sourceProperty,
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(targetProperty);
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty);
	_lastError = BindingError::None;
	return result;
}
#endif

#if CUI_ENABLE_DYNAMIC_XAML
Binding* BindingCollection::Add(const DependencyProperty& targetProperty,
	BindingSourceReference source,
	const std::wstring& sourceProperty,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner, targetProperty, std::move(source), sourceProperty,
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}
#endif

Binding* BindingCollection::Add(const DependencyProperty& targetProperty,
	BindingSourceReference source,
	CompiledBindingPathView sourcePath,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
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
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::make_unique<Binding>(
		_owner, targetProperty, std::move(source), sourcePath,
		mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding* BindingCollection::AddTemplateBinding(
	const std::wstring& targetProperty,
	IBindingSource& templatedParent,
	const std::wstring& sourceProperty)
{
	if (!_owner)
	{
		_lastError = BindingError::InvalidTarget;
		return nullptr;
	}
	const auto* targetMetadata = _owner->FindPropertyMetadata(targetProperty);
	const auto* targetIdentity = targetMetadata
		? &targetMetadata->Property() : nullptr;
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::unique_ptr<Binding>(new Binding(
		_owner, DependencyPropertyReference(targetProperty), &templatedParent,
		BindingSourcePropertyReference(sourceProperty),
		BindingMode::OneWay, DataSourceUpdateMode::Never,
		std::shared_ptr<const IBindingValueConverter>{},
		std::optional<BindingValue>{}, std::optional<BindingValue>{},
		std::optional<BindingValue>{}, std::optional<std::wstring>{},
		DependencyPropertyValueSource::Template,
		DependencyPropertyExpressionKind::TemplateBinding));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(targetProperty);
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty);
	_lastError = BindingError::None;
	return result;
}
#endif

Binding* BindingCollection::AddTemplateBinding(
	const DependencyProperty& targetProperty,
	DependencyObject& templatedParent,
	const DependencyProperty& sourceProperty)
{
	if (!_owner)
	{
		_lastError = BindingError::InvalidTarget;
		return nullptr;
	}
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::unique_ptr<Binding>(new Binding(
		_owner, targetProperty, templatedParent, sourceProperty,
		DependencyPropertyValueSource::Template,
		DependencyPropertyExpressionKind::TemplateBinding));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_items.push_back(std::move(binding));
	_validationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}

#if CUI_ENABLE_DYNAMIC_XAML
MultiBinding* BindingCollection::AddMulti(
	const std::wstring& targetProperty,
	std::vector<MultiBindingSource> sources,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IMultiBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
{
	if (!_owner)
	{
		_lastError = BindingError::InvalidTarget;
		return nullptr;
	}
	const auto* targetMetadata = _owner->FindPropertyMetadata(targetProperty);
	const auto* targetIdentity = targetMetadata
		? &targetMetadata->Property() : nullptr;
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty, targetIdentity](const auto& binding)
		{
			return binding
				&& (targetIdentity
					? binding->TargetPropertyIdentity() == targetIdentity
					: IsSameProperty(binding->TargetProperty(), targetProperty));
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}
	auto binding = std::make_unique<MultiBinding>(
		_owner, targetProperty, std::move(sources), mode, updateMode,
		std::move(converter), std::move(fallbackValue),
		std::move(targetNullValue), std::move(converterParameter),
		std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(targetProperty);
		});
	_multiItems.push_back(std::move(binding));
	_multiValidationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues()) NotifyValidationChanged(targetProperty);
	_lastError = BindingError::None;
	return result;
}
#endif

MultiBinding* BindingCollection::AddMulti(
	const DependencyProperty& targetProperty,
	std::vector<MultiBindingSource> sources,
	BindingMode mode,
	DataSourceUpdateMode updateMode,
	std::shared_ptr<const IMultiBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
{
	if (!_owner)
	{
		_lastError = BindingError::InvalidTarget;
		return nullptr;
	}
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		}) || std::any_of(
		_multiItems.begin(), _multiItems.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}
	auto binding = std::make_unique<MultiBinding>(
		_owner, targetProperty, std::move(sources), mode, updateMode,
		std::move(converter), std::move(fallbackValue),
		std::move(targetNullValue), std::move(converterParameter),
		std::move(stringFormat));
	if (!binding->IsValid())
	{
		_lastError = binding->LastError();
		return nullptr;
	}
	auto* result = binding.get();
	const std::weak_ptr<CallbackState> callbackState = _callbackState;
	auto validationConnection = result->ValidationChanged().Subscribe(
		[callbackState, property = &targetProperty](
			const BindingValidationChangedEventArgs&)
		{
			auto state = callbackState.lock();
			if (state && state->Owner)
				state->Owner->NotifyValidationChanged(property->Name());
		});
	_multiItems.push_back(std::move(binding));
	_multiValidationConnections.push_back(std::move(validationConnection));
	if (result->HasValidationIssues())
		NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return result;
}

void BindingCollection::Clear()
{
	const bool hadValidation = HasValidationIssues();
	_validationConnections.clear();
	_multiValidationConnections.clear();
	_items.clear();
	_multiItems.clear();
	if (hadValidation) NotifyValidationChanged(L"");
	_lastError = BindingError::None;
}

Binding* BindingCollection::Find(const DependencyProperty& targetProperty)
{
	const auto found = std::find_if(_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (found != _items.end()) return found->get();
	auto* multi = FindMulti(targetProperty);
	return multi ? multi->TargetBinding() : nullptr;
}

const Binding* BindingCollection::Find(
	const DependencyProperty& targetProperty) const
{
	const auto found = std::find_if(_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (found != _items.end()) return found->get();
	const auto* multi = FindMulti(targetProperty);
	return multi ? multi->TargetBinding() : nullptr;
}

MultiBinding* BindingCollection::FindMulti(
	const DependencyProperty& targetProperty)
{
	const auto found = std::find_if(
		_multiItems.begin(), _multiItems.end(), [&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	return found == _multiItems.end() ? nullptr : found->get();
}

const MultiBinding* BindingCollection::FindMulti(
	const DependencyProperty& targetProperty) const
{
	const auto found = std::find_if(
		_multiItems.begin(), _multiItems.end(), [&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	return found == _multiItems.end() ? nullptr : found->get();
}

bool BindingCollection::Remove(const DependencyProperty& targetProperty)
{
	const auto found = std::find_if(_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& binding->TargetPropertyIdentity() == &targetProperty;
		});
	if (found == _items.end())
	{
		const auto multi = std::find_if(
			_multiItems.begin(), _multiItems.end(),
			[&targetProperty](const auto& binding)
			{
				return binding
					&& binding->TargetPropertyIdentity() == &targetProperty;
			});
		if (multi == _multiItems.end()) return false;
		const size_t index = static_cast<size_t>(multi - _multiItems.begin());
		const bool hadValidation = (*multi)->HasValidationIssues();
		if (index < _multiValidationConnections.size())
			_multiValidationConnections.erase(
				_multiValidationConnections.begin() + index);
		_multiItems.erase(multi);
		if (hadValidation) NotifyValidationChanged(targetProperty.Name());
		_lastError = BindingError::None;
		return true;
	}
	const size_t index = static_cast<size_t>(found - _items.begin());
	const bool hadValidation = (*found)->HasValidationIssues();
	if (index < _validationConnections.size())
		_validationConnections.erase(_validationConnections.begin() + index);
	_items.erase(found);
	if (hadValidation) NotifyValidationChanged(targetProperty.Name());
	_lastError = BindingError::None;
	return true;
}

bool BindingCollection::UpdateTarget(const DependencyProperty& targetProperty)
{
	if (auto* multi = FindMulti(targetProperty))
	{
		const bool updated = multi->UpdateTarget();
		_lastError = updated ? BindingError::None : multi->LastError();
		return updated;
	}
	if (auto* binding = Find(targetProperty))
	{
		const bool updated = binding->UpdateTarget();
		_lastError = updated ? BindingError::None : binding->LastError();
		return updated;
	}
	_lastError = BindingError::TargetPropertyNotFound;
	return false;
}

bool BindingCollection::UpdateSource(const DependencyProperty& targetProperty)
{
	if (auto* multi = FindMulti(targetProperty))
	{
		const bool updated = multi->UpdateSource();
		_lastError = updated ? BindingError::None : multi->LastError();
		return updated;
	}
	if (auto* binding = Find(targetProperty))
	{
		const bool updated = binding->UpdateSource();
		_lastError = updated ? BindingError::None : binding->LastError();
		return updated;
	}
	_lastError = BindingError::TargetPropertyNotFound;
	return false;
}

#if CUI_ENABLE_DYNAMIC_XAML
Binding* BindingCollection::Find(const std::wstring& targetProperty)
{
	const auto found = std::find_if(_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		});
	if (found != _items.end()) return found->get();
	auto* multi = FindMulti(targetProperty);
	return multi ? multi->TargetBinding() : nullptr;
}

const Binding* BindingCollection::Find(
	const std::wstring& targetProperty) const
{
	const auto found = std::find_if(_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		});
	if (found != _items.end()) return found->get();
	const auto* multi = FindMulti(targetProperty);
	return multi ? multi->TargetBinding() : nullptr;
}

MultiBinding* BindingCollection::FindMulti(
	const std::wstring& targetProperty)
{
	const auto found = std::find_if(
		_multiItems.begin(), _multiItems.end(), [&](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		});
	return found == _multiItems.end() ? nullptr : found->get();
}

const MultiBinding* BindingCollection::FindMulti(
	const std::wstring& targetProperty) const
{
	const auto found = std::find_if(
		_multiItems.begin(), _multiItems.end(), [&](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		});
	return found == _multiItems.end() ? nullptr : found->get();
}

bool BindingCollection::Remove(const std::wstring& targetProperty)
{
	const auto found = std::find_if(_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		});
	if (found == _items.end())
	{
		const auto multi = std::find_if(
			_multiItems.begin(), _multiItems.end(), [&](const auto& binding)
			{
				return binding && IsSameProperty(
					binding->TargetProperty(), targetProperty);
			});
		if (multi == _multiItems.end()) return false;
		const size_t index = static_cast<size_t>(multi - _multiItems.begin());
		const bool hadValidation = (*multi)->HasValidationIssues();
		if (index < _multiValidationConnections.size())
			_multiValidationConnections.erase(
				_multiValidationConnections.begin() + index);
		_multiItems.erase(multi);
		if (hadValidation) NotifyValidationChanged(targetProperty);
		_lastError = BindingError::None;
		return true;
	}
	const size_t index = static_cast<size_t>(found - _items.begin());
	const bool hadValidation = (*found)->HasValidationIssues();
	if (index < _validationConnections.size())
		_validationConnections.erase(_validationConnections.begin() + index);
	_items.erase(found);
	if (hadValidation) NotifyValidationChanged(targetProperty);
	_lastError = BindingError::None;
	return true;
}

bool BindingCollection::UpdateTarget(const std::wstring& targetProperty)
{
	if (auto* multi = FindMulti(targetProperty))
	{
		const bool updated = multi->UpdateTarget();
		_lastError = updated ? BindingError::None : multi->LastError();
		return updated;
	}
	if (auto* binding = Find(targetProperty))
	{
		const bool updated = binding->UpdateTarget();
		_lastError = updated ? BindingError::None : binding->LastError();
		return updated;
	}
	_lastError = BindingError::TargetPropertyNotFound;
	return false;
}

bool BindingCollection::UpdateSource(const std::wstring& targetProperty)
{
	if (auto* multi = FindMulti(targetProperty))
	{
		const bool updated = multi->UpdateSource();
		_lastError = updated ? BindingError::None : multi->LastError();
		return updated;
	}
	if (auto* binding = Find(targetProperty))
	{
		const bool updated = binding->UpdateSource();
		_lastError = updated ? BindingError::None : binding->LastError();
		return updated;
	}
	_lastError = BindingError::TargetPropertyNotFound;
	return false;
}
#endif

size_t BindingCollection::Count() const
{
	return _items.size() + _multiItems.size();
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
	for (const auto& binding : _multiItems)
	{
		if (!binding) continue;
		auto values = binding->GetValidationResults();
		result.insert(result.end(),
			std::make_move_iterator(values.begin()),
			std::make_move_iterator(values.end()));
	}
	return result;
}

bool BindingCollection::HasValidationIssues() const
{
	return std::any_of(_items.begin(), _items.end(), [](const auto& binding)
	{
		return binding && binding->HasValidationIssues();
	}) || std::any_of(_multiItems.begin(), _multiItems.end(), [](const auto& binding)
	{
		return binding && binding->HasValidationIssues();
	});
}

bool BindingCollection::HasValidationErrors() const
{
	return std::any_of(_items.begin(), _items.end(), [](const auto& binding)
	{
		return binding && binding->HasValidationErrors();
	}) || std::any_of(_multiItems.begin(), _multiItems.end(), [](const auto& binding)
	{
		return binding && binding->HasValidationErrors();
	});
}

Binding* BindingCollection::operator[](size_t index)
{
	if (index < _items.size()) return _items[index].get();
	index -= _items.size();
	return index < _multiItems.size()
		? _multiItems[index]->TargetBinding() : nullptr;
}

const Binding* BindingCollection::operator[](size_t index) const
{
	if (index < _items.size()) return _items[index].get();
	index -= _items.size();
	return index < _multiItems.size()
		? _multiItems[index]->TargetBinding() : nullptr;
}
