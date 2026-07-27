#include "Binding.h"
#include "BindingList.h"
#include "Control.h"
#include "XamlInfrastructure.h"

#include <algorithm>
#include <cerrno>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <locale>
#include <mutex>
#include <sstream>
#include <unordered_set>

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

	bool IsPropertyNameLess(const std::wstring& a, const std::wstring& b)
	{
		const auto common = (std::min)(a.size(), b.size());
		for (size_t index = 0; index < common; ++index)
		{
			const auto left = std::towlower(a[index]);
			const auto right = std::towlower(b[index]);
			if (left != right) return left < right;
		}
		return a.size() < b.size();
	}

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
			out = value.ToString();
			return true;
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
					auto negate = [](const BindingValue& value,
						const BindingValueConverterContext&, BindingValue& out)
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
						[](const BindingValue& value,
							const BindingValueConverterContext&, BindingValue& out)
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
					auto trim = [](const BindingValue& value,
						const BindingValueConverterContext&, BindingValue& out)
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

	struct MultiConverterRegistryEntry
	{
		MultiBindingValueConverterMetadata Metadata;
		MultiBindingValueConverterRegistry::Factory Factory;
	};

	std::vector<MultiConverterRegistryEntry>& RegisteredMultiBindingConverters()
	{
		static std::vector<MultiConverterRegistryEntry> entries;
		return entries;
	}

	std::mutex& MultiBindingConverterMutex()
	{
		static std::mutex mutex;
		return mutex;
	}

	std::vector<std::unique_ptr<DependencyProperty>>& RegisteredDependencyProperties()
	{
		static std::vector<std::unique_ptr<DependencyProperty>> properties;
		return properties;
	}

	std::vector<std::unique_ptr<DependencyPropertyMetadata>>& RegisteredBindingProperties()
	{
		static std::vector<std::unique_ptr<DependencyPropertyMetadata>> properties;
		return properties;
	}

	struct EffectiveMetadataCacheEntry final
	{
		const DependencyProperty* Property = nullptr;
		std::vector<const DependencyPropertyMetadata*> Layers;
		std::unique_ptr<DependencyPropertyMetadata> Metadata;
	};

	std::vector<EffectiveMetadataCacheEntry>& EffectiveMetadataCache()
	{
		static std::vector<EffectiveMetadataCacheEntry> entries;
		return entries;
	}

	std::size_t& NextDependencyPropertyGlobalIndex()
	{
		static std::size_t value = 0;
		return value;
	}

	std::mutex& BindingPropertyMutex()
	{
		static std::mutex mutex;
		return mutex;
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

bool TryParseBindingPropertyPath(
	const std::wstring& value,
	std::vector<BindingPathStep>& steps)
{
	return TryParseBindingPropertyPathCore(value, steps);
}

namespace
{
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

	struct BindingPathCursor final
	{
		IBindingSource* Source = nullptr;
		IBindingList* List = nullptr;
		std::vector<std::shared_ptr<IBindingSource>> SourceOwners;
		std::vector<std::shared_ptr<IBindingList>> ListOwners;
	};

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
}

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
			return IsPropertyNameLess(left.Name, right.Name);
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

bool MultiBindingValueConverterRegistry::Register(
	MultiBindingValueConverterMetadata metadata,
	Factory factory,
	bool replaceExisting)
{
	metadata.Name = Trim(std::move(metadata.Name));
	if (metadata.Name.empty() || metadata.MinimumInputCount == 0 || !factory)
		return false;
	std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
	auto& entries = RegisteredMultiBindingConverters();
	const auto found = std::find_if(entries.begin(), entries.end(),
		[&](const MultiConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, metadata.Name);
		});
	if (found != entries.end())
	{
		if (!replaceExisting) return false;
		found->Metadata = std::move(metadata);
		found->Factory = std::move(factory);
		return true;
	}
	entries.push_back({ std::move(metadata), std::move(factory) });
	return true;
}

bool MultiBindingValueConverterRegistry::Unregister(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return false;
	std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
	auto& entries = RegisteredMultiBindingConverters();
	const auto found = std::find_if(entries.begin(), entries.end(),
		[&](const MultiConverterRegistryEntry& entry)
		{
			return IsSameProperty(entry.Metadata.Name, normalized);
		});
	if (found == entries.end()) return false;
	entries.erase(found);
	return true;
}

std::optional<MultiBindingValueConverterMetadata>
MultiBindingValueConverterRegistry::Find(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return std::nullopt;
	std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
	for (const auto& entry : RegisteredMultiBindingConverters())
		if (IsSameProperty(entry.Metadata.Name, normalized))
			return entry.Metadata;
	return std::nullopt;
}

std::vector<MultiBindingValueConverterMetadata>
MultiBindingValueConverterRegistry::GetConverters()
{
	std::vector<MultiBindingValueConverterMetadata> result;
	{
		std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
		for (const auto& entry : RegisteredMultiBindingConverters())
			result.push_back(entry.Metadata);
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		return IsPropertyNameLess(left.Name, right.Name);
	});
	return result;
}

std::shared_ptr<const IMultiBindingValueConverter>
MultiBindingValueConverterRegistry::Create(const std::wstring& name)
{
	const auto normalized = Trim(name);
	if (normalized.empty()) return {};
	Factory factory;
	{
		std::lock_guard<std::mutex> lock(MultiBindingConverterMutex());
		for (const auto& entry : RegisteredMultiBindingConverters())
			if (IsSameProperty(entry.Metadata.Name, normalized))
			{
				factory = entry.Factory;
				break;
			}
	}
	if (!factory) return {};
	try { return factory(); }
	catch (...) { return {}; }
}

DependencyProperty::DependencyProperty(
	std::wstring name,
	BindingValueKind valueKind,
	std::type_index valueType,
	std::type_index ownerType,
	std::size_t globalIndex,
	Validator validator,
	std::shared_ptr<const unsigned char> readOnlyAuthorization)
	: _name(std::move(name)),
	  _valueKind(valueKind),
	  _valueType(valueType),
	  _ownerType(ownerType),
	  _globalIndex(globalIndex),
	  _validator(std::move(validator)),
	  _readOnlyAuthorization(std::move(readOnlyAuthorization))
{
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
	std::wstring inheritanceKey,
	DependencyPropertyDesignMetadata design)
	: _name(std::move(name)),
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
		  : defaultUpdateMode),
	  _inheritanceKey(std::move(inheritanceKey)),
	  _design(std::move(design))
{
}

bool DependencyPropertyMetadata::HasSameInheritanceIdentity(
	const DependencyPropertyMetadata& other) const noexcept
{
	if (this == &other) return true;
	if (_property && _property == other._property) return true;
	return !_inheritanceKey.empty()
		&& _inheritanceKey == other._inheritanceKey
		&& _valueKind == other._valueKind
		&& _valueType == other._valueType;
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

bool DependencyPropertyMetadata::IsDesignerBrowsable(DependencyObject& target) const
{
	if (!_design.Browsable || !Matches(target)) return false;
	return !_design.BrowsableWhen || _design.BrowsableWhen(target);
}

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
	if (!_valueConverter) _valueConverter = base._valueConverter;
	if (!_coercer) _coercer = base._coercer;
	if (!_comparer) _comparer = base._comparer;
	if (!_getter) _getter = base._getter;
	if (!_setter) _setter = base._setter;
	_usesEffectiveValueStorage =
		_usesEffectiveValueStorage || base._usesEffectiveValueStorage;
	if (!_subscriber) _subscriber = base._subscriber;
	if (!_hasDefaultValue && base._hasDefaultValue)
	{
		_defaultValue = base._defaultValue;
		_hasDefaultValue = true;
	}
	_flags |= base._flags;
	if (_inheritanceKey.empty()) _inheritanceKey = base._inheritanceKey;
	if (_defaultUpdateMode == DataSourceUpdateMode::OnPropertyChanged)
		_defaultUpdateMode = base._defaultUpdateMode;
	if (_design.DisplayName.empty()) _design.DisplayName = base._design.DisplayName;
	if (_design.Category == L"Misc") _design.Category = base._design.Category;
	if (_design.Choices.empty()) _design.Choices = base._design.Choices;
	if (!_design.Minimum) _design.Minimum = base._design.Minimum;
	if (!_design.Maximum) _design.Maximum = base._design.Maximum;
	if (!_design.Step) _design.Step = base._design.Step;
	if (_design.Persistence == DependencyPropertyPersistence::Automatic)
		_design.Persistence = base._design.Persistence;
	if (!_design.BrowsableWhen) _design.BrowsableWhen = base._design.BrowsableWhen;

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
	if (_subscriber && Matches(target) && handler)
		return _subscriber(target, std::move(handler), updateMode);
	return {};
}

std::unique_ptr<DependencyProperty>
DependencyPropertyRegistry::CreateStandalone(
	DependencyPropertyMetadata& metadata)
{
	if (metadata._name.empty()) return {};
	std::scoped_lock lock(BindingPropertyMutex());
	auto authorization = metadata._isReadOnly
		? std::make_shared<const unsigned char>(0)
		: std::shared_ptr<const unsigned char>{};
	auto property = std::unique_ptr<DependencyProperty>(
		new DependencyProperty(
			metadata._name,
			metadata._valueKind,
			metadata._valueType,
			metadata._ownerType,
			NextDependencyPropertyGlobalIndex()++,
			metadata._validator,
			std::move(authorization)));
	metadata.AttachProperty(*property);
	if (metadata._hasDefaultValue
		&& !property->IsValidValue(metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");
	return property;
}

const DependencyProperty* DependencyPropertyRegistry::Register(
	DependencyPropertyMetadata metadata)
{
	if (metadata._name.empty())
		throw std::invalid_argument(
			"Dependency property name cannot be empty");
	std::scoped_lock lock(BindingPropertyMutex());
	for (const auto& existing : RegisteredBindingProperties())
	{
		if (existing->OwnerType() == metadata.OwnerType()
			&& IsSameProperty(existing->Name(), metadata.Name()))
			throw std::invalid_argument(
				"Dependency property is already registered for this owner");
	}

	auto authorization = metadata._isReadOnly
		? std::make_shared<const unsigned char>(0)
		: std::shared_ptr<const unsigned char>{};
	auto property = std::unique_ptr<DependencyProperty>(
		new DependencyProperty(
			metadata._name,
			metadata._valueKind,
			metadata._valueType,
			metadata._ownerType,
			NextDependencyPropertyGlobalIndex()++,
			metadata._validator,
			std::move(authorization)));
	metadata.AttachProperty(*property);
	if (metadata._hasDefaultValue
		&& !property->IsValidValue(metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");

	const auto* result = property.get();
	RegisteredDependencyProperties().push_back(std::move(property));
	RegisteredBindingProperties().push_back(
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata)));
	return result;
}

DependencyPropertyKey DependencyPropertyRegistry::RegisterReadOnly(
	DependencyPropertyMetadata metadata)
{
	metadata._isReadOnly = true;
	if (metadata._name.empty())
		throw std::invalid_argument(
			"Dependency property name cannot be empty");
	std::scoped_lock lock(BindingPropertyMutex());
	for (const auto& existing : RegisteredBindingProperties())
	{
		if (existing->OwnerType() != metadata.OwnerType()
			|| !IsSameProperty(existing->Name(), metadata.Name()))
			continue;
		throw std::invalid_argument(
			"Dependency property is already registered for this owner");
	}

	auto authorization = std::make_shared<const unsigned char>(0);
	auto property = std::unique_ptr<DependencyProperty>(
		new DependencyProperty(
			metadata._name,
			metadata._valueKind,
			metadata._valueType,
			metadata._ownerType,
			NextDependencyPropertyGlobalIndex()++,
			metadata._validator,
			authorization));
	metadata.AttachProperty(*property);
	if (metadata._hasDefaultValue
		&& !property->IsValidValue(metadata._defaultValue))
		throw std::invalid_argument(
			"Dependency property default value failed validation");

	const auto* result = property.get();
	RegisteredDependencyProperties().push_back(std::move(property));
	RegisteredBindingProperties().push_back(
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata)));
	return DependencyPropertyKey(*result, std::move(authorization));
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::AddOwner(
	const DependencyProperty& property,
	DependencyPropertyMetadata metadata,
	const DependencyPropertyKey* key)
{
	std::scoped_lock lock(BindingPropertyMutex());
	if (property.ReadOnly()
		? (!key || !property.Authorizes(*key))
		: key != nullptr)
		return nullptr;
	if (metadata._valueKind != property.ValueKind()
		|| metadata._valueType != property.ValueType())
		return nullptr;

	bool propertyRegistered = false;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() != &property) continue;
		propertyRegistered = true;
		if (candidate->OwnerType() == metadata.OwnerType())
			return nullptr;
	}
	if (!propertyRegistered) return nullptr;

	metadata.AttachProperty(property);
	if (metadata._hasDefaultValue
		&& !property.IsValidValue(metadata._defaultValue))
		return nullptr;
	auto stored =
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata));
	const auto* result = stored.get();
	RegisteredBindingProperties().push_back(std::move(stored));
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::OverrideMetadata(
	const DependencyProperty& property,
	DependencyPropertyMetadata metadata,
	const DependencyPropertyKey* key)
{
	std::scoped_lock lock(BindingPropertyMutex());
	if (property.ReadOnly()
		? (!key || !property.Authorizes(*key))
		: key != nullptr)
		return nullptr;
	if (metadata._valueKind != property.ValueKind()
		|| metadata._valueType != property.ValueType())
		return nullptr;

	bool propertyRegistered = false;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() != &property) continue;
		propertyRegistered = true;
		if (candidate->OwnerType() == metadata.OwnerType())
			return nullptr;
	}
	if (!propertyRegistered) return nullptr;

	metadata.AttachProperty(property);
	if (metadata._hasDefaultValue
		&& !property.IsValidValue(metadata._defaultValue))
		return nullptr;
	auto stored =
		std::make_unique<DependencyPropertyMetadata>(std::move(metadata));
	const auto* result = stored.get();
	RegisteredBindingProperties().push_back(std::move(stored));
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::ResolveMetadata(
	const DependencyProperty& property,
	std::span<const DependencyPropertyMetadata* const> layers)
{
	if (layers.empty()) return nullptr;

	const DependencyPropertyMetadata* defaultMetadata = nullptr;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() == &property)
		{
			defaultMetadata = candidate.get();
			break;
		}
	}
	if (!defaultMetadata) return nullptr;
	if (layers.size() == 1 && layers.front() == defaultMetadata)
		return defaultMetadata;

	for (const auto& cached : EffectiveMetadataCache())
	{
		if (cached.Property != &property
			|| cached.Layers.size() != layers.size())
			continue;
		if (std::equal(
			cached.Layers.begin(), cached.Layers.end(), layers.begin()))
			return cached.Metadata.get();
	}

	auto effective =
		std::make_unique<DependencyPropertyMetadata>(*defaultMetadata);
	for (const auto* layer : layers)
	{
		if (!layer || layer == defaultMetadata) continue;
		auto derived =
			std::make_unique<DependencyPropertyMetadata>(*layer);
		derived->MergeBaseMetadata(*effective);
		effective = std::move(derived);
	}

	EffectiveMetadataCacheEntry cached;
	cached.Property = &property;
	cached.Layers.assign(layers.begin(), layers.end());
	cached.Metadata = std::move(effective);
	const auto* result = cached.Metadata.get();
	EffectiveMetadataCache().push_back(std::move(cached));
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::Find(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	target.EnsureBindingPropertiesRegistered();
	if (const auto* declarative =
		target.FindDeclarativePropertyMetadata(propertyName))
		return declarative;
	return FindNative(target, propertyName);
}

const DependencyProperty* DependencyPropertyRegistry::FindProperty(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	const auto* metadata = Find(target, propertyName);
	return metadata ? &metadata->Property() : nullptr;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::GetMetadata(
	DependencyObject& target,
	const DependencyProperty& property)
{
	target.EnsureBindingPropertiesRegistered();
	if (const auto* declarative =
		target.FindDeclarativePropertyMetadata(property.Name());
		declarative && &declarative->Property() == &property)
		return declarative;

	std::scoped_lock lock(BindingPropertyMutex());
	std::vector<const DependencyPropertyMetadata*> layers;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() == &property
			&& candidate->Matches(target)
			&& target.SupportsNativeProperty(*candidate))
			layers.push_back(candidate.get());
	}
	return ResolveMetadata(property, layers);
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::FindNative(
	DependencyObject& target,
	const std::wstring& propertyName)
{
	target.EnsureBindingPropertiesRegistered();
	std::scoped_lock lock(BindingPropertyMutex());
	auto& properties = RegisteredBindingProperties();
	const DependencyProperty* identity = nullptr;
	for (auto it = properties.rbegin(); it != properties.rend(); ++it)
	{
		if (IsSameProperty((*it)->Name(), propertyName)
			&& (*it)->Matches(target)
			&& target.SupportsNativeProperty(**it))
		{
			identity = &(*it)->Property();
			break;
		}
	}
	if (!identity) return nullptr;
	std::vector<const DependencyPropertyMetadata*> layers;
	for (const auto& candidate : properties)
	{
		if (&candidate->Property() == identity
			&& candidate->Matches(target)
			&& target.SupportsNativeProperty(*candidate))
			layers.push_back(candidate.get());
	}
	return ResolveMetadata(*identity, layers);
}

std::vector<const DependencyPropertyMetadata*>
DependencyPropertyRegistry::GetProperties(DependencyObject& target)
{
	target.EnsureBindingPropertiesRegistered();
	std::scoped_lock lock(BindingPropertyMutex());
	std::vector<const DependencyPropertyMetadata*> result =
		target.GetDeclarativePropertyMetadata();
	auto& properties = RegisteredBindingProperties();
	std::unordered_set<std::wstring> effectiveNames;
	effectiveNames.reserve(result.size() + properties.size());
	for (const auto* property : result)
		if (property) effectiveNames.insert(property->Name());
	for (auto it = properties.rbegin(); it != properties.rend(); ++it)
	{
		const auto* candidate = it->get();
		if (!candidate->Matches(target)
			|| !target.SupportsNativeProperty(*candidate))
			continue;
		if (!effectiveNames.insert(candidate->Name()).second)
			continue;
		std::vector<const DependencyPropertyMetadata*> layers;
		for (const auto& layer : properties)
		{
			if (&layer->Property() == &candidate->Property()
				&& layer->Matches(target)
				&& target.SupportsNativeProperty(*layer))
				layers.push_back(layer.get());
		}
		if (const auto* metadata =
			ResolveMetadata(candidate->Property(), layers))
			result.push_back(metadata);
	}
	std::sort(result.begin(), result.end(),
		[](const DependencyPropertyMetadata* left,
			const DependencyPropertyMetadata* right)
		{
			return IsPropertyNameLess(left->Name(), right->Name());
		});
	return result;
}

const DependencyPropertyMetadata* DependencyPropertyRegistry::FindRegistered(
	std::span<const std::type_index> ownerTypes,
	const std::wstring& propertyName)
{
	std::scoped_lock lock(BindingPropertyMutex());
	const DependencyProperty* identity = nullptr;
	for (auto it = RegisteredBindingProperties().rbegin();
		it != RegisteredBindingProperties().rend(); ++it)
	{
		const auto* candidate = it->get();
		if (!IsSameProperty(candidate->Name(), propertyName)) continue;
		if (std::find(ownerTypes.begin(), ownerTypes.end(), candidate->OwnerType())
			!= ownerTypes.end())
		{
			identity = &candidate->Property();
			break;
		}
	}
	if (!identity) return nullptr;
	std::vector<const DependencyPropertyMetadata*> layers;
	for (const auto& candidate : RegisteredBindingProperties())
	{
		if (&candidate->Property() == identity
			&& std::find(
				ownerTypes.begin(), ownerTypes.end(), candidate->OwnerType())
				!= ownerTypes.end())
			layers.push_back(candidate.get());
	}
	return ResolveMetadata(*identity, layers);
}

std::vector<const DependencyPropertyMetadata*>
DependencyPropertyRegistry::GetRegisteredProperties(
	std::span<const std::type_index> ownerTypes,
	std::function<bool(const DependencyPropertyMetadata&)> include)
{
	std::scoped_lock lock(BindingPropertyMutex());
	std::vector<const DependencyPropertyMetadata*> result;
	std::unordered_set<std::wstring> effectiveNames;
	for (auto it = RegisteredBindingProperties().rbegin();
		it != RegisteredBindingProperties().rend(); ++it)
	{
		const auto* candidate = it->get();
		if (std::find(ownerTypes.begin(), ownerTypes.end(), candidate->OwnerType())
			== ownerTypes.end()) continue;
		if (include && !include(*candidate)) continue;
		if (!effectiveNames.insert(candidate->Name()).second)
			continue;
		std::vector<const DependencyPropertyMetadata*> layers;
		for (const auto& layer : RegisteredBindingProperties())
		{
			if (&layer->Property() == &candidate->Property()
				&& std::find(
					ownerTypes.begin(), ownerTypes.end(), layer->OwnerType())
					!= ownerTypes.end()
				&& (!include || include(*layer)))
				layers.push_back(layer.get());
		}
		if (const auto* metadata =
			ResolveMetadata(candidate->Property(), layers))
			result.push_back(metadata);
	}
	std::sort(result.begin(), result.end(),
		[](const DependencyPropertyMetadata* left,
			const DependencyPropertyMetadata* right)
		{
			return IsPropertyNameLess(left->Name(), right->Name());
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
			return IsPropertyNameLess(left.Name, right.Name);
		});
	return result;
}

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
	_propertyChanged.Notify(L"");
	_validationChanged.Notify(L"");
}

bool BindingSourceProxy::TryGetValue(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	return _source && _source.Get()->TryGetValue(propertyName, out);
}

bool BindingSourceProxy::TrySetValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return _source && _source.Get()->TrySetValue(propertyName, value);
}

bool BindingSourceProxy::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	return _source
		&& _source.Get()->TryGetPropertyMetadata(propertyName, out);
}

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

void BindingSourceProxy::Attach()
{
	if (!_source) return;
	_propertyConnection = _source.Get()->PropertyChanged().Subscribe(
		[this](const PropertyChangedEventArgs& args)
		{
			_propertyChanged.Notify(args.PropertyName);
		});
	if (auto* validation = _source.Get()->ValidationChanged())
		_validationConnection = validation->Subscribe(
			[this](const BindingValidationChangedEventArgs& args)
			{
				_validationChanged.Notify(args.PropertyName);
			});
}

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
	: Binding(target, std::move(targetProperty), source,
		std::move(sourceProperty), mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}

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
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _source(source),
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
	if (_source)
		_sourceLifetime = _source->BindingLifetime();
	Attach();
}

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
	: Binding(target, std::move(targetProperty), std::move(source),
		std::move(sourceProperty), mode, updateMode, std::move(converter),
		std::move(fallbackValue), std::move(targetNullValue),
		std::move(converterParameter), std::move(stringFormat),
		DependencyPropertyValueSource::Local,
		DependencyPropertyExpressionKind::Binding)
{
}

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
	std::optional<std::wstring> stringFormat,
	DependencyPropertyValueSource targetValueSource,
	DependencyPropertyExpressionKind expressionKind)
	: _target(target),
	  _ownedSource(std::move(source)),
	  _source(_ownedSource.Get()),
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
	if (_source)
		_sourceLifetime = _source->BindingLifetime();
	Attach();
}

Binding::~Binding()
{
	if (_state)
		_state->Owner = nullptr;
	_targetConnection.Disconnect();
	if (_ownsTargetValue && IsTargetAlive())
		_target->ClearBindingPropertyValue(
			_targetProperty, this, _targetValueSource, _expressionKind);
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
		if (!_target->TryAttachBindingPropertyExpression(
			_targetProperty, this, _targetValueSource, _expressionKind))
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
	_isValid = false;
	if (!IsTargetAlive()) return Fail(BindingError::InvalidTarget);
	if (!_source) return Fail(BindingError::InvalidSource);
	if (_targetProperty.empty()) return Fail(BindingError::EmptyTargetProperty);
	if (_sourceProperty.empty()) return Fail(BindingError::EmptySourceProperty);
	if (!TryParseBindingPropertyPath(_sourceProperty, _sourcePath))
		return Fail(BindingError::InvalidSourcePropertyPath);
	if (!IsSourceAlive()) return Fail(BindingError::SourceUnavailable);

	_targetMetadata = DependencyPropertyRegistry::Find(*_target, _targetProperty);
	if (!_targetMetadata) return Fail(BindingError::TargetPropertyNotFound);
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
			_targetProperty, this, _targetValueSource, _expressionKind))
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
	BindingPathCursor cursor;
	cursor.Source = _source;
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
}

void Binding::AttachSourceChangedHandlers()
{
	_sourceConnections.clear();
	_sourcePathOwners.clear();
	_sourcePathListOwners.clear();
	if (!IsSourceAlive() || _mode == BindingMode::OneWayToSource || _mode == BindingMode::OneTime)
		return;

	BindingPathCursor cursor;
	cursor.Source = _source;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		std::weak_ptr<State> weakState = _state;
		EventConnection connection;
		if (cursor.Source)
		{
			const std::wstring expectedProperty = _sourcePath[index].Value;
			connection = cursor.Source->PropertyChanged().Subscribe(
				[weakState, expectedProperty](const PropertyChangedEventArgs& e)
				{
					if (!e.PropertyName.empty()
						&& !IsSameProperty(e.PropertyName, expectedProperty)) return;
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
}

void Binding::AttachValidationChangedHandlers()
{
	_sourceValidationConnections.clear();
	_validationPathConnections.clear();
	_validationPathOwners.clear();
	_validationPathListOwners.clear();
	if (!IsSourceAlive() || _sourcePath.empty()) return;

	BindingPathCursor cursor;
	cursor.Source = _source;
	for (size_t index = 0; index < _sourcePath.size(); ++index)
	{
		const std::wstring expectedProperty = _sourcePath[index].Value;
		if (cursor.Source)
		{
			if (auto* validationChanged = cursor.Source->ValidationChanged())
			{
				std::weak_ptr<State> weakState = _state;
				auto connection = validationChanged->Subscribe(
					[weakState, expectedProperty](
						const BindingValidationChangedEventArgs& e)
					{
						if (!e.PropertyName.empty()
							&& !IsSameProperty(e.PropertyName, expectedProperty)) return;
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
					[weakState, expectedProperty](const PropertyChangedEventArgs& e)
					{
						if (!e.PropertyName.empty()
							&& !IsSameProperty(e.PropertyName, expectedProperty)) return;
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
	if (_ownsTargetValue && IsTargetAlive()
		&& !_target->IsBindingExpressionOwner(
			_targetProperty, this, _targetValueSource, _expressionKind))
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
		_targetProperty, converted, this,
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
			hasSourceValue ? sourceValue.Kind() : BindingValueKind::Empty };
		if (!_converter->ConvertBack(value, context, converted))
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
	error = BindingError::None;
	if (!IsSourceAlive() || _sourcePath.empty())
	{
		error = BindingError::SourceUnavailable;
		return false;
	}
	BindingPathCursor cursor;
	if (!ResolveBindingPathOwner(*_source, _sourcePath, cursor))
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
}

bool Binding::TryWriteSourcePathValue(
	const BindingValue& value,
	BindingError& error) const
{
	error = BindingError::None;
	if (!IsSourceAlive() || _sourcePath.empty())
	{
		error = BindingError::SourceUnavailable;
		return false;
	}
	BindingPathCursor cursor;
	if (!ResolveBindingPathOwner(*_source, _sourcePath, cursor))
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
}

MultiBindingSource::MultiBindingSource(
	IBindingSource* source,
	std::wstring sourceProperty,
	std::shared_ptr<const IBindingValueConverter> converter,
	std::optional<BindingValue> fallbackValue,
	std::optional<BindingValue> targetNullValue,
	std::optional<BindingValue> converterParameter,
	std::optional<std::wstring> stringFormat)
	: Source(source),
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
	: Source(source.Get()),
	  OwnedSource(std::move(source)),
	  SourceProperty(std::move(sourceProperty)),
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

	std::shared_ptr<const DeclarativeTypeDescriptor>
		GetMultiBindingCollectorSchema(std::size_t slotCount)
	{
		static std::mutex mutex;
		static std::unordered_map<std::size_t,
			std::shared_ptr<const DeclarativeTypeDescriptor>> schemas;
		std::scoped_lock lock(mutex);
		const auto found = schemas.find(slotCount);
		if (found != schemas.end()) return found->second;

		std::vector<DeclarativePropertyDefinition> properties;
		properties.reserve(slotCount + 1);
		DeclarativePropertyDefinition result;
		result.Name = L"CombinedValue";
		result.ValueKind = BindingValueKind::Object;
		result.DefaultValue = BindingValue(MultiBindingResultState{});
		properties.push_back(std::move(result));
		for (std::size_t index = 0; index < slotCount; ++index)
		{
			DeclarativePropertyDefinition slot;
			slot.Name = L"Value" + std::to_wstring(index);
			slot.ValueKind = BindingValueKind::Object;
			slot.DefaultValue = BindingValue(MultiBindingSlotState{});
			properties.push_back(std::move(slot));
		}
		auto descriptor = DeclarativeTypeDescriptor::Create(
			{ L"urn:cui:internal:binding",
			  L"MultiBindingCollector" + std::to_wstring(slotCount) },
			std::move(properties));
		if (descriptor) schemas.emplace(slotCount, descriptor);
		return descriptor;
	}
}

struct MultiBinding::State final
{
	static constexpr const wchar_t* ResultProperty = L"CombinedValue";

	std::wstring TargetProperty;
	BindingMode Mode = BindingMode::Default;
	DataSourceUpdateMode UpdateMode = DataSourceUpdateMode::Default;
	std::vector<std::wstring> SourceProperties;
	std::shared_ptr<const IMultiBindingValueConverter> Converter;
	std::optional<BindingValue> TargetNullValue;
	std::optional<BindingValue> ConverterParameter;
	std::optional<std::wstring> StringFormat;
	BindingValueKind TargetKind = BindingValueKind::Empty;
	Control Collector;
	std::vector<std::wstring> SlotProperties;
	std::unique_ptr<Binding> TargetExpression;
	EventConnection CollectorConnection;
	EventConnection ChildValidationConnection;
	BindingValidationChangedEvent Validation;
	BindingError Error = BindingError::None;
	bool Initializing = true;
	bool Recomputing = false;
	bool WritingBack = false;
	bool ManualUpdateSource = false;

	bool InitializeCollector(std::size_t slotCount)
	{
		auto descriptor = GetMultiBindingCollectorSchema(slotCount);
		return descriptor
			&& cui::framework::XamlAccess::SetTypeDescriptor(
				Collector, std::move(descriptor));
	}

	void SetResult(MultiBindingResultState state)
	{
		Recomputing = true;
		(void)Collector.TrySetPropertyValue(
			ResultProperty, BindingValue(std::move(state)));
		Recomputing = false;
	}

	void Recompute()
	{
		if (Initializing || WritingBack) return;
		std::vector<BindingValue> values;
		values.reserve(SlotProperties.size());
		for (const auto& slot : SlotProperties)
		{
			BindingValue wrapped;
			MultiBindingSlotState state;
			if (!Collector.TryGetPropertyValue(slot, wrapped)
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
			state.Value, SlotProperties.size(), context, values)
			|| values.size() != SlotProperties.size())
		{
			Error = BindingError::MultiBindingConverterFailed;
			return;
		}
		WritingBack = true;
		bool success = true;
		BindingError writeError = BindingError::SourceWriteFailed;
		for (size_t index = 0; index < values.size(); ++index)
		{
			if (!Collector.TrySetCurrentPropertyValue(
				SlotProperties[index], WrapMultiBindingSlotValue(values[index])))
			{
				success = false;
				break;
			}
		}
		for (size_t index = 0; success && index < SlotProperties.size(); ++index)
		{
			auto* child = Collector.DataBindings.Find(SlotProperties[index]);
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
		for (const auto& slot : SlotProperties)
		{
			auto* child = Collector.DataBindings.Find(slot);
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
	if (!target || state.TargetProperty.empty() || sources.size() < 2)
	{
		state.Error = BindingError::InvalidMultiBinding;
		return;
	}
	const auto* targetMetadata = DependencyPropertyRegistry::Find(
		*target, state.TargetProperty);
	if (!targetMetadata)
	{
		state.Error = BindingError::TargetPropertyNotFound;
		return;
	}
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

	if (!state.InitializeCollector(sources.size()))
	{
		state.Error = BindingError::InvalidMultiBinding;
		return;
	}
	state.SlotProperties.reserve(sources.size());
	state.SourceProperties.reserve(sources.size());
	for (size_t index = 0; index < sources.size(); ++index)
	{
		auto& source = sources[index];
		const auto requestedChildMode = source.Mode.value_or(state.Mode);
		const auto childMode = requestedChildMode == BindingMode::Default
			? state.Mode : requestedChildMode;
		const auto requestedChildUpdateMode = source.UpdateMode.value_or(
			state.UpdateMode);
		const auto childUpdateMode = requestedChildUpdateMode
			== DataSourceUpdateMode::Default
			? state.UpdateMode : requestedChildUpdateMode;
		if ((!source.Source && !source.OwnedSource)
			|| source.SourceProperty.empty()
			|| (source.StringFormat
				&& !IsValidBindingStringFormat(*source.StringFormat)))
		{
			state.Error = BindingError::InvalidMultiBinding;
			return;
		}
		const auto slot = L"Value" + std::to_wstring(index);
		state.SlotProperties.push_back(slot);
		state.SourceProperties.push_back(source.SourceProperty);
	}

	const std::weak_ptr<State> weakState = _state;
	state.CollectorConnection = state.Collector.OnPropertyValueChanged.Subscribe(
		[weakState](DependencyObject*,
			const DependencyPropertyChangedEventArgs& args)
		{
			auto state = weakState.lock();
			if (!state) return;
			if (IsSameProperty(args.PropertyName, State::ResultProperty))
			{
				MultiBindingResultState result;
				if (args.NewValue.TryGet(result)) state->WriteBack(result);
			}
			else state->Recompute();
		});
	state.ChildValidationConnection =
		state.Collector.DataBindings.ValidationChanged().Subscribe(
			[weakState](const BindingValidationChangedEventArgs&)
			{
				auto state = weakState.lock();
				if (state)
					state->Validation.Notify(state->TargetProperty);
			});

	for (size_t index = 0; index < sources.size(); ++index)
	{
		auto& source = sources[index];
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
		Binding* child = source.OwnedSource
			? state.Collector.DataBindings.Add(
				state.SlotProperties[index], std::move(source.OwnedSource),
				source.SourceProperty, childMode, childUpdateMode, std::move(slotConverter),
				std::move(slotFallback), std::move(slotNull),
				std::move(source.ConverterParameter))
			: state.Collector.DataBindings.Add(
				state.SlotProperties[index], source.Source,
				source.SourceProperty, childMode, childUpdateMode, std::move(slotConverter),
				std::move(slotFallback), std::move(slotNull),
				std::move(source.ConverterParameter));
		if (!child)
		{
			state.Error = state.Collector.DataBindings.LastError();
			return;
		}
	}
	state.Initializing = false;
	state.Recompute();
	state.TargetExpression = std::make_unique<Binding>(
		target, state.TargetProperty, &state.Collector, State::ResultProperty,
		state.Mode, state.UpdateMode, std::make_shared<MultiBindingResultConverter>(),
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
	return _state ? _state->TargetProperty : empty;
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
	return _state ? _state->SourceProperties.size() : 0;
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
	const auto childResults = state->Collector.DataBindings.GetValidationResults();
	for (size_t index = 0; index < childResults.size(); ++index)
	{
		const auto slot = std::find(
			state->SlotProperties.begin(), state->SlotProperties.end(),
			childResults[index].TargetProperty);
		const auto sourceIndex = slot == state->SlotProperties.end()
			? size_t{ 0 }
			: static_cast<size_t>(slot - state->SlotProperties.begin());
		result.push_back({ state->TargetProperty,
			sourceIndex < state->SourceProperties.size()
				? state->SourceProperties[sourceIndex] : std::wstring{},
			childResults[index].Issue });
	}
	return result;
}

bool MultiBinding::HasValidationIssues() const
{
	auto state = _state;
	return state && state->Collector.DataBindings.HasValidationIssues();
}

bool MultiBinding::HasValidationErrors() const
{
	auto state = _state;
	return state && state->Collector.DataBindings.HasValidationErrors();
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
	_validationChanged.Notify(targetProperty);
	// Validation listeners may remove the complete binding collection. Do not
	// read this object again after publishing the snapshot.
	if (owner && !ownerLifetime.expired() && !owner->IsDestroying())
		owner->OnBindingValidationChanged(targetProperty);
}

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
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		}) || FindMulti(targetProperty) != nullptr;
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
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		}) || FindMulti(targetProperty) != nullptr;
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
	const bool duplicateTarget = std::any_of(
		_items.begin(), _items.end(),
		[&targetProperty](const auto& binding)
		{
			return binding
				&& IsSameProperty(binding->TargetProperty(), targetProperty);
		}) || FindMulti(targetProperty) != nullptr;
	if (duplicateTarget)
	{
		_lastError = BindingError::DuplicateTargetProperty;
		return nullptr;
	}

	auto binding = std::unique_ptr<Binding>(new Binding(
		_owner, targetProperty, &templatedParent, sourceProperty,
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
	if (Find(targetProperty) || FindMulti(targetProperty))
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
