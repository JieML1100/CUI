#ifndef CUI_BINDING_H_INCLUDED
#define CUI_BINDING_H_INCLUDED
#pragma once

#include "Core/EventConnection.h"
#include "CuiBuildFeatures.h"
#include <any>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

class Control;
class DependencyObject;
class DependencyProperty;
class IBindingSource;
class IBindingList;

#if CUI_ENABLE_DYNAMIC_XAML
namespace cui::details
{
	class DependencyPropertyStandaloneAccess;
}
#endif

/**
 * Metadata observation callback shared by declaration-only headers.
 *
 * Keeping this alias independent from DependencyPropertyMetadata allows
 * DependencyObject-derived classes to declare overrides without requiring the
 * metadata class body to be complete at that exact parse point.
 */
using DependencyPropertyChangeHandler = std::function<void()>;

/**
 * Stable, name-free identity for one property exposed by an IBindingSource.
 *
 * Zero is reserved for "all properties" / object-level validation.  AOT
 * generated code hashes the canonical schema member at compile time and keeps
 * only this token in Production execution tables.
 */
struct BindingSourcePropertyToken final
{
	std::uint64_t Value = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return Value != 0;
	}
	constexpr bool operator==(
		const BindingSourcePropertyToken&) const noexcept = default;
};

/** 64-bit FNV-1a over stable UTF-32 code units. */
[[nodiscard]] constexpr BindingSourcePropertyToken
MakeBindingSourcePropertyToken(std::wstring_view name) noexcept
{
	if (name.empty()) return {};
	std::uint64_t hash = 14695981039346656037ull;
	for (const wchar_t character : name)
	{
		const auto codeUnit = static_cast<std::uint32_t>(character);
		for (unsigned shift = 0; shift != 32; shift += 8)
		{
			hash ^= static_cast<std::uint8_t>(codeUnit >> shift);
			hash *= 1099511628211ull;
		}
	}
	return { hash == 0 ? 1ull : hash };
}

/**
 * One dependency-property member literal expressed in the active runtime
 * flavor. Design retains the authored name for schema discovery; Production
 * lowers the same source literal directly to its stable token.
 */
#if CUI_ENABLE_DYNAMIC_XAML
template<std::size_t Size>
[[nodiscard]] inline std::wstring DependencyPropertyRegistrationLiteral(
	const wchar_t (&name)[Size])
{
	static_assert(Size > 1,
		"Dependency-property registration names cannot be empty");
	return std::wstring(name, Size - 1);
}
#else
template<std::size_t Size>
[[nodiscard]] consteval BindingSourcePropertyToken
DependencyPropertyRegistrationLiteral(const wchar_t (&name)[Size]) noexcept
{
	static_assert(Size > 1,
		"Dependency-property registration names cannot be empty");
	return MakeBindingSourcePropertyToken(
		std::wstring_view(name, Size - 1));
}
#endif

enum class BindingMode
{
	OneWay,
	TwoWay,
	OneWayToSource,
	OneTime,
	/** Resolve from the target property's metadata. */
	Default
};

enum class DataSourceUpdateMode
{
	OnPropertyChanged,
	OnValidation,
	Never,
	/** Resolve from the target property's metadata. */
	Default
};

/** Stable diagnostic codes for binding configuration and update failures. */
enum class BindingError
{
	None,
	InvalidTarget,
	InvalidSource,
	EmptyTargetProperty,
	EmptySourceProperty,
	InvalidSourcePropertyPath,
	DuplicateTargetProperty,
	TargetPropertyNotFound,
	TargetNotReadable,
	TargetNotWritable,
	TargetNotObservable,
	SourceUnavailable,
	SourceNotReadable,
	SourceNotWritable,
	SourceNotObservable,
	SourcePathUnresolved,
	SourceReadFailed,
	TargetReadFailed,
	TargetConversionFailed,
	TargetWriteFailed,
	SourceConversionFailed,
	SourceWriteFailed,
	InvalidStringFormat,
	StringFormatFailed,
	InvalidMultiBinding,
	MultiBindingConverterFailed
};

const wchar_t* BindingErrorMessage(BindingError error) noexcept;

/** Severity reported by a binding source for a field or object-level issue. */
enum class BindingValidationSeverity
{
	Info,
	Warning,
	Error
};

const wchar_t* BindingValidationSeverityName(
	BindingValidationSeverity severity) noexcept;

/** One stable, user-facing validation issue reported by a binding source. */
struct BindingValidationIssue
{
	std::wstring Message;
	BindingValidationSeverity Severity = BindingValidationSeverity::Error;
	std::wstring Code;

	bool operator==(const BindingValidationIssue&) const = default;
};

/** Adds binding context when a control aggregates issues from several bindings. */
struct BindingValidationResult
{
	std::wstring TargetProperty;
	std::wstring SourceProperty;
	BindingValidationIssue Issue;

	bool operator==(const BindingValidationResult&) const = default;
};

/**
 * Compact WPF Nullable<Boolean> value used by ToggleButton.IsChecked.
 *
 * Unlike std::optional<bool>, conversion in a boolean context reflects the
 * contained value rather than merely HasValue().  This preserves the behavior
 * of existing native `if (toggle.IsChecked)` call sites while still retaining
 * an explicit indeterminate/null state in BindingValue.
 */
class NullableBool final
{
public:
	constexpr NullableBool() noexcept = default;
	constexpr NullableBool(std::nullopt_t) noexcept {}
	constexpr NullableBool(bool value) noexcept
		: _hasValue(true), _value(value)
	{
	}

	constexpr bool HasValue() const noexcept { return _hasValue; }
	constexpr bool GetValueOrDefault(bool fallback = false) const noexcept
	{
		return _hasValue ? _value : fallback;
	}
	constexpr operator bool() const noexcept
	{
		return _hasValue && _value;
	}

	constexpr bool operator==(const NullableBool&) const noexcept = default;
	friend constexpr bool operator==(
		NullableBool left, bool right) noexcept
	{
		return left._hasValue && left._value == right;
	}
	friend constexpr bool operator==(
		bool left, NullableBool right) noexcept
	{
		return right == left;
	}

private:
	bool _hasValue = false;
	bool _value = false;
};

enum class BindingValueKind
{
	Empty,
	Bool,
	Int,
	Int64,
	Float,
	Double,
	String,
	Object,
	/** Appended so the stable numeric identities above remain unchanged. */
	NullableBool
};

class BindingValue
{
public:
	using Storage = std::variant<std::monostate, bool, int, long long, float,
		double, std::wstring, std::any, NullableBool>;

	BindingValue();
	BindingValue(bool value);
	BindingValue(NullableBool value);
	BindingValue(int value);
	BindingValue(long long value);
	BindingValue(float value);
	BindingValue(double value);
	BindingValue(const wchar_t* value);
	BindingValue(const std::wstring& value);
	BindingValue(std::wstring&& value);

	template<typename T>
		requires (!std::is_same_v<std::remove_cvref_t<T>, BindingValue>
			&& !std::is_same_v<std::remove_cvref_t<T>, bool>
			&& !std::is_same_v<std::remove_cvref_t<T>, NullableBool>
			&& !std::is_same_v<std::remove_cvref_t<T>, int>
			&& !std::is_same_v<std::remove_cvref_t<T>, long long>
			&& !std::is_same_v<std::remove_cvref_t<T>, float>
			&& !std::is_same_v<std::remove_cvref_t<T>, double>
			&& !std::is_same_v<std::remove_cvref_t<T>, std::wstring>
			&& !std::is_convertible_v<T, const wchar_t*>)
	explicit BindingValue(T&& value)
		: _value(std::any(std::forward<T>(value)))
	{
	}

	BindingValueKind Kind() const;
	bool Empty() const;
	std::wstring ToString() const;

	bool TryGetBool(bool& out) const;
	bool TryGetNullableBool(NullableBool& out) const;
	bool TryGetInt(int& out) const;
	bool TryGetInt64(long long& out) const;
	bool TryGetFloat(float& out) const;
	bool TryGetDouble(double& out) const;
	bool TryGetString(std::wstring& out) const;
	/**
	 * Returns a non-owning view only when the stored value is already a String.
	 * Unlike TryGetString this never formats or copies a value; the view remains
	 * valid until this BindingValue is assigned or destroyed.
	 */
	bool TryGetStringView(std::wstring_view& out) const noexcept;

	template<typename T>
	bool TryGet(T& out) const
	{
		using Value = std::remove_cv_t<T>;
		if constexpr (std::is_same_v<Value, bool>)
			return TryGetBool(out);
		else if constexpr (std::is_same_v<Value, NullableBool>)
			return TryGetNullableBool(out);
		else if constexpr (std::is_same_v<Value, int>)
			return TryGetInt(out);
		else if constexpr (std::is_same_v<Value, long long>)
			return TryGetInt64(out);
		else if constexpr (std::is_same_v<Value, float>)
			return TryGetFloat(out);
		else if constexpr (std::is_same_v<Value, double>)
			return TryGetDouble(out);
		else if constexpr (std::is_same_v<Value, std::wstring>)
			return TryGetString(out);
		else
		{
			if (Kind() == BindingValueKind::Object)
			{
				if (const auto* exact = std::any_cast<Value>(&std::get<std::any>(_value)))
				{
					out = *exact;
					return true;
				}
			}

			if constexpr (std::is_enum_v<Value>)
			{
				long long numeric = 0;
				if (!TryGetInt64(numeric)) return false;
				using Underlying = std::underlying_type_t<Value>;
				if constexpr (std::is_unsigned_v<Underlying>)
				{
					if (numeric < 0
						|| static_cast<unsigned long long>(numeric)
							> static_cast<unsigned long long>((std::numeric_limits<Underlying>::max)()))
						return false;
				}
				else if (numeric < static_cast<long long>((std::numeric_limits<Underlying>::min)())
					|| numeric > static_cast<long long>((std::numeric_limits<Underlying>::max)()))
				{
					return false;
				}
				out = static_cast<Value>(static_cast<Underlying>(numeric));
				return true;
			}
			else if constexpr (std::is_integral_v<Value>)
			{
				long long numeric = 0;
				if (!TryGetInt64(numeric)) return false;
				if constexpr (std::is_unsigned_v<Value>)
				{
					if (numeric < 0
						|| static_cast<unsigned long long>(numeric)
							> static_cast<unsigned long long>((std::numeric_limits<Value>::max)()))
						return false;
				}
				else if (numeric < static_cast<long long>((std::numeric_limits<Value>::min)())
					|| numeric > static_cast<long long>((std::numeric_limits<Value>::max)()))
				{
					return false;
				}
				out = static_cast<Value>(numeric);
				return true;
			}
			else if constexpr (std::is_floating_point_v<Value>)
			{
				double numeric = 0.0;
				if (!TryGetDouble(numeric)) return false;
				if constexpr (sizeof(Value) < sizeof(double))
				{
					if (std::isfinite(numeric)
						&& (numeric < static_cast<double>(-(std::numeric_limits<Value>::max)())
							|| numeric > static_cast<double>((std::numeric_limits<Value>::max)())))
						return false;
				}
				out = static_cast<Value>(numeric);
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	const std::type_info& Type() const noexcept;

	const Storage& Raw() const { return _value; }
	/** Projects the normalized value into command/event APIs that use std::any. */
	std::any ToAny() const;

private:
	Storage _value;
};

#if CUI_ENABLE_DYNAMIC_XAML
/** One parsed member or indexer in a WPF-style Binding PropertyPath. */
enum class BindingPathStepKind : uint8_t
{
	Property,
	Indexer
};

struct BindingPathStep final
{
	BindingPathStepKind Kind = BindingPathStepKind::Property;
	std::wstring Value;

	bool operator==(const BindingPathStep&) const = default;
};
#endif

/** One name-free operation in an AOT-lowered binding path. */
enum class CompiledBindingPathStepKind : std::uint8_t
{
	Property,
	ListIndex
};

enum class CompiledBindingPathCapabilities : std::uint8_t
{
	None = 0,
	Read = 1 << 0,
	Write = 1 << 1,
	Observe = 1 << 2
};

[[nodiscard]] constexpr CompiledBindingPathCapabilities operator|(
	CompiledBindingPathCapabilities left,
	CompiledBindingPathCapabilities right) noexcept
{
	return static_cast<CompiledBindingPathCapabilities>(
		static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr bool HasCompiledBindingPathCapability(
	CompiledBindingPathCapabilities value,
	CompiledBindingPathCapabilities capability) noexcept
{
	return (static_cast<std::uint8_t>(value)
		& static_cast<std::uint8_t>(capability)) != 0;
}

struct CompiledSourceHandle;
struct CompiledBindingPathStep;

/**
 * Whole-endpoint operations for one AOT-resolved binding source property.
 *
 * Object identifies the stable source/provider instance and Context is an
 * accessor-specific immutable operand (for example, a DependencyProperty).
 * Generated/native endpoints may therefore call a concrete property directly;
 * the legacy CompiledBindingPath lane remains the explicit IBindingSource
 * adapter for sources whose concrete contract is unknown at build time.
 */
struct CompiledSourceOps final
{
	using CapabilitiesCallback = CompiledBindingPathCapabilities(*)(
		const CompiledSourceHandle&);
	using ValueKindCallback = BindingValueKind(*)(
		const CompiledSourceHandle&);
	using LifetimeCallback = std::weak_ptr<const void>(*)(
		const CompiledSourceHandle&);
	using ReadCallback = bool(*)(
		const CompiledSourceHandle&, BindingValue&);
	using WriteCallback = bool(*)(
		const CompiledSourceHandle&, const BindingValue&);
	using SubscribeCallback = EventConnection(*)(
		const CompiledSourceHandle&, DependencyPropertyChangeHandler);
	using ValidationCallback = std::vector<BindingValidationIssue>(*)(
		const CompiledSourceHandle&);
	using SubscribeValidationCallback = EventConnection(*)(
		const CompiledSourceHandle&, DependencyPropertyChangeHandler);

	CapabilitiesCallback Capabilities = nullptr;
	ValueKindCallback ValueKind = nullptr;
	LifetimeCallback Lifetime = nullptr;
	ReadCallback Read = nullptr;
	WriteCallback Write = nullptr;
	SubscribeCallback Subscribe = nullptr;
	ValidationCallback Validation = nullptr;
	SubscribeValidationCallback SubscribeValidation = nullptr;
};

/** Compact, non-owning handle to one already-resolved source endpoint. */
struct CompiledSourceHandle final
{
	void* Object = nullptr;
	const void* Context = nullptr;
	const CompiledSourceOps* Ops = nullptr;

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return Object != nullptr && Ops != nullptr;
	}
	[[nodiscard]] CompiledBindingPathCapabilities Capabilities() const
	{
		return *this && Ops->Capabilities
			? Ops->Capabilities(*this)
			: CompiledBindingPathCapabilities::None;
	}
	[[nodiscard]] BindingValueKind ValueKind() const
	{
		return *this && Ops->ValueKind
			? Ops->ValueKind(*this) : BindingValueKind::Empty;
	}
	[[nodiscard]] std::weak_ptr<const void> Lifetime() const
	{
		return *this && Ops->Lifetime
			? Ops->Lifetime(*this) : std::weak_ptr<const void>{};
	}
};

static_assert(std::is_trivially_copyable_v<CompiledSourceHandle>);
static_assert(std::is_standard_layout_v<CompiledSourceHandle>);

/**
 * Resolves one property step against the current path cursor without a name or
 * BindingSourcePropertyToken lookup. A non-null resolver is authoritative: an
 * empty result is a hard endpoint-resolution failure and must never fall back
 * to the external-source token adapter.
 */
using CompiledBindingPathEndpointResolver =
	CompiledSourceHandle(*)(IBindingSource&) noexcept;

namespace cui::binding
{
	/** Creates an exact DependencyObject/DependencyProperty source endpoint. */
	[[nodiscard]] CompiledSourceHandle MakeCompiledDependencyPropertySource(
		DependencyObject& source,
		const DependencyProperty& property) noexcept;
	/**
	 * Resolves an exact DependencyProperty endpoint from a path cursor source.
	 * Returns empty when the cursor is not a DependencyObject.
	 */
	[[nodiscard]] CompiledSourceHandle ResolveCompiledDependencyPropertySource(
		IBindingSource& source,
		const DependencyProperty& property) noexcept;

	/**
	 * Wraps one explicitly unknown external C++ source property in the compact
	 * endpoint ABI. The process-lifetime descriptor supplies the already-lowered
	 * token, capabilities and value kind; only value access remains virtual.
	 */
	[[nodiscard]] CompiledSourceHandle MakeCompiledBindingSourcePropertyAdapter(
		IBindingSource& source,
		const CompiledBindingPathStep& property) noexcept;
}

struct CompiledBindingPathStep final
{
	CompiledBindingPathStepKind Kind = CompiledBindingPathStepKind::Property;
	CompiledBindingPathCapabilities Capabilities =
		CompiledBindingPathCapabilities::Read
		| CompiledBindingPathCapabilities::Observe;
	BindingValueKind ValueKind = BindingValueKind::Empty;
	BindingSourcePropertyToken Property;
	std::uint32_t ListIndex = 0;
	CompiledBindingPathEndpointResolver EndpointResolver = nullptr;

	constexpr bool operator==(const CompiledBindingPathStep&) const noexcept = default;
};

inline constexpr std::uint32_t CompiledBindingPathVersion = 2;

/**
 * Non-owning view of an immutable, process-lifetime AOT path table.
 * Generated code must keep the referenced step array alive for the Binding.
 */
struct CompiledBindingPathView final
{
	std::span<const CompiledBindingPathStep> Steps;
	std::uint32_t Version = CompiledBindingPathVersion;

	constexpr CompiledBindingPathView() noexcept = default;
	constexpr explicit CompiledBindingPathView(
		std::span<const CompiledBindingPathStep> steps,
		std::uint32_t version = CompiledBindingPathVersion) noexcept
		: Steps(steps), Version(version)
	{
	}
	template<std::size_t Size>
	constexpr CompiledBindingPathView(
		const CompiledBindingPathStep (&steps)[Size]) noexcept
		: Steps(steps), Version(CompiledBindingPathVersion)
	{
	}

	[[nodiscard]] constexpr bool Empty() const noexcept { return Steps.empty(); }
};

#if CUI_ENABLE_DYNAMIC_XAML
/**
 * Parses paths such as Profile.Name, People[0].Name,
 * (AutomationProperties.Name) and Settings['accent.color']. Quoted keys
 * escape their quote by doubling it.
 */
bool TryParseBindingPropertyPath(
	const std::wstring& value,
	std::vector<BindingPathStep>& steps);
#endif

bool TryConvertBindingValue(const BindingValue& value, BindingValueKind targetKind, BindingValue& out);
/** Converts while preserving the concrete type represented by targetValue. */
bool TryConvertBindingValue(const BindingValue& value, const BindingValue& targetValue, BindingValue& out);
/** Compares two already-normalized scalar binding values without stringifying them. */
bool BindingValuesEqual(const BindingValue& left, const BindingValue& right);

/** Validates/formats the single-value composite syntax used by Binding.StringFormat. */
bool IsValidBindingStringFormat(const std::wstring& format) noexcept;
bool TryFormatBindingValue(
	const BindingValue& value,
	const std::wstring& format,
	std::wstring& out);
/** MultiBinding counterpart supporting indexed placeholders such as {0} and {1}. */
bool IsValidMultiBindingStringFormat(
	const std::wstring& format,
	size_t valueCount) noexcept;
bool TryFormatBindingValues(
	const std::vector<BindingValue>& values,
	const std::wstring& format,
	std::wstring& out);

/** Extensible converter call context. */
struct BindingValueConverterContext final
{
	const BindingValue* Parameter = nullptr;
	BindingValueKind TargetKind = BindingValueKind::Empty;
};

/**
 * Optional transform used before metadata conversion in either binding direction.
 * Implementations should return false without modifying application state when a
 * value cannot be converted.
 */
class IBindingValueConverter
{
public:
	virtual ~IBindingValueConverter() = default;
	virtual bool Convert(
		const BindingValue& value,
		const BindingValueConverterContext& context,
		BindingValue& out) const
	{
		(void)value;
		(void)context;
		(void)out;
		return false;
	}
	virtual bool ConvertBack(
		const BindingValue& value,
		const BindingValueConverterContext& context,
		BindingValue& out) const
	{
		(void)value;
		(void)context;
		(void)out;
		return false;
	}
};

/** Function-backed converter for lightweight formatting and unit transforms. */
class DelegateBindingValueConverter final : public IBindingValueConverter
{
public:
	using ContextFunction = std::function<bool(
		const BindingValue&,
		const BindingValueConverterContext&,
		BindingValue&)>;

	DelegateBindingValueConverter(
		ContextFunction convert,
		ContextFunction convertBack = {});
	bool Convert(
		const BindingValue& value,
		const BindingValueConverterContext& context,
		BindingValue& out) const override;
	bool ConvertBack(
		const BindingValue& value,
		const BindingValueConverterContext& context,
		BindingValue& out) const override;

private:
	ContextFunction _contextConvert;
	ContextFunction _contextConvertBack;
};

/** Stable identities emitted by AOT code for framework-provided converters. */
enum class BuiltInBindingValueConverter : std::uint8_t
{
	BooleanNegation,
	StringIsNotEmpty,
	StringTrim
};

/** Returns one process-lifetime singleton without consulting the name registry. */
std::shared_ptr<const IBindingValueConverter>
GetBuiltInBindingValueConverter(BuiltInBindingValueConverter converter);

/** Context supplied to WPF-style IMultiValueConverter implementations. */
struct MultiBindingValueConverterContext final
{
	const BindingValue* Parameter = nullptr;
	BindingValueKind TargetKind = BindingValueKind::Empty;
};

class IMultiBindingValueConverter
{
public:
	virtual ~IMultiBindingValueConverter() = default;
	virtual bool Convert(
		const std::vector<BindingValue>& values,
		const MultiBindingValueConverterContext& context,
		BindingValue& out) const = 0;
	virtual bool ConvertBack(
		const BindingValue& value,
		size_t targetCount,
		const MultiBindingValueConverterContext& context,
		std::vector<BindingValue>& out) const
	{
		(void)value;
		(void)targetCount;
		(void)context;
		(void)out;
		return false;
	}
};

class DelegateMultiBindingValueConverter final
	: public IMultiBindingValueConverter
{
public:
	using ConvertFunction = std::function<bool(
		const std::vector<BindingValue>&,
		const MultiBindingValueConverterContext&,
		BindingValue&)>;
	using ConvertBackFunction = std::function<bool(
		const BindingValue&,
		size_t,
		const MultiBindingValueConverterContext&,
		std::vector<BindingValue>&)>;

	DelegateMultiBindingValueConverter(
		ConvertFunction convert,
		ConvertBackFunction convertBack = {});
	bool Convert(
		const std::vector<BindingValue>& values,
		const MultiBindingValueConverterContext& context,
		BindingValue& out) const override;
	bool ConvertBack(
		const BindingValue& value,
		size_t targetCount,
		const MultiBindingValueConverterContext& context,
		std::vector<BindingValue>& out) const override;

private:
	ConvertFunction _convert;
	ConvertBackFunction _convertBack;
};

class PropertyChangedEventArgs
{
public:
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring PropertyName;
#endif
	BindingSourcePropertyToken PropertyToken;

	PropertyChangedEventArgs() = default;
#if CUI_ENABLE_DYNAMIC_XAML
	explicit PropertyChangedEventArgs(std::wstring propertyName);
#endif
	explicit PropertyChangedEventArgs(BindingSourcePropertyToken propertyToken) noexcept;
};

class PropertyChangedEvent
{
public:
	using Handler = std::function<void(const PropertyChangedEventArgs&)>;

	PropertyChangedEvent() = default;
	// Subscriptions belong to the publisher object, not to copied/moved value state.
	PropertyChangedEvent(const PropertyChangedEvent&) noexcept {}
	PropertyChangedEvent(PropertyChangedEvent&&) noexcept {}
	PropertyChangedEvent& operator=(const PropertyChangedEvent&) noexcept { return *this; }
	PropertyChangedEvent& operator=(PropertyChangedEvent&&) noexcept { return *this; }

	size_t Add(Handler handler);
	EventConnection Subscribe(Handler handler);
	void Remove(size_t token);
	/** Production immediately lowers names to tokens; event args never retain them. */
	void Notify(const std::wstring& propertyName);
	void Notify(BindingSourcePropertyToken propertyToken);
	void Notify(const PropertyChangedEventArgs& args);
	void Clear();
	size_t Count() const noexcept;

private:
	struct State;
	std::shared_ptr<State> _state;
};

class BindingValidationChangedEventArgs
{
public:
	/** Empty means object-level validation or that every property may have changed. */
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring PropertyName;
#endif
	BindingSourcePropertyToken PropertyToken;

	BindingValidationChangedEventArgs() = default;
#if CUI_ENABLE_DYNAMIC_XAML
	explicit BindingValidationChangedEventArgs(std::wstring propertyName);
#endif
	explicit BindingValidationChangedEventArgs(
		BindingSourcePropertyToken propertyToken) noexcept;
};

/** RAII-observable validation notification independent from value changes. */
class BindingValidationChangedEvent
{
public:
	using function_type = void(const BindingValidationChangedEventArgs&);
	using std_function_type = std::function<function_type>;
	using Handler = std_function_type;

	BindingValidationChangedEvent() = default;
	// Subscriptions belong to the publisher object, not to copied/moved value state.
	BindingValidationChangedEvent(const BindingValidationChangedEvent&) noexcept {}
	BindingValidationChangedEvent(BindingValidationChangedEvent&&) noexcept {}
	BindingValidationChangedEvent& operator=(
		const BindingValidationChangedEvent&) noexcept { return *this; }
	BindingValidationChangedEvent& operator=(
		BindingValidationChangedEvent&&) noexcept { return *this; }

	size_t Add(Handler handler);
	EventConnection Subscribe(Handler handler);
	void Remove(size_t token);
	/** Production immediately lowers names to tokens; event args never retain them. */
	void Notify(const std::wstring& propertyName);
	void Notify(BindingSourcePropertyToken propertyToken);
	void Notify(const BindingValidationChangedEventArgs& args);
	void Clear();
	size_t Count() const noexcept;

private:
	struct State;
	std::shared_ptr<State> _state;
};

class INotifyPropertyChanged
{
public:
	virtual ~INotifyPropertyChanged() = default;
	virtual PropertyChangedEvent& PropertyChanged() = 0;
};

/** Discoverable metadata for one property exposed by an IBindingSource. */
struct BindingSourcePropertyMetadata
{
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring Name;
#endif
	BindingValueKind ValueKind = BindingValueKind::Empty;
	std::type_index ValueType{ typeid(void) };
	bool CanRead = true;
	bool CanWrite = true;
	bool CanObserve = true;

	BindingSourcePropertyMetadata() = default;
	BindingSourcePropertyMetadata(
		BindingValueKind valueKind,
		std::type_index valueType,
		bool canRead = true,
		bool canWrite = true,
		bool canObserve = true) noexcept
		: ValueKind(valueKind),
		  ValueType(valueType),
		  CanRead(canRead),
		  CanWrite(canWrite),
		  CanObserve(canObserve)
	{
	}
#if CUI_ENABLE_DYNAMIC_XAML
	BindingSourcePropertyMetadata(
		std::wstring name,
		BindingValueKind valueKind,
		std::type_index valueType,
		bool canRead = true,
		bool canWrite = true,
		bool canObserve = true)
		: BindingSourcePropertyMetadata(
			valueKind, valueType, canRead, canWrite, canObserve)
	{
		Name = std::move(name);
	}
#else
	/**
	 * Transitional aggregate-spelling compatibility for generated sources that
	 * already emit an empty Design-name slot. No name is retained in Production.
	 */
	BindingSourcePropertyMetadata(
		std::nullptr_t,
		BindingValueKind valueKind,
		std::type_index valueType,
		bool canRead = true,
		bool canWrite = true,
		bool canObserve = true) noexcept
		: BindingSourcePropertyMetadata(
			valueKind, valueType, canRead, canWrite, canObserve)
	{
	}
#endif

	bool operator==(const BindingSourcePropertyMetadata&) const = default;
};

class IBindingSource : public INotifyPropertyChanged
{
public:
	IBindingSource() = default;
	IBindingSource(const IBindingSource&) noexcept {}
	IBindingSource(IBindingSource&&) noexcept {}
	IBindingSource& operator=(const IBindingSource&) noexcept { return *this; }
	IBindingSource& operator=(IBindingSource&&) noexcept { return *this; }
	/**
	 * Name-free AOT surface. Production sources must implement this contract;
	 * Design keeps the historical defaults so dynamically discovered sources
	 * remain source compatible with the XAML editor.
	 */
	virtual bool TryGetValue(
		BindingSourcePropertyToken property,
		BindingValue& out) const
#if CUI_ENABLE_DYNAMIC_XAML
	{
		(void)property;
		(void)out;
		return false;
	}
#else
		= 0;
#endif
	virtual bool TrySetValue(
		BindingSourcePropertyToken property,
		const BindingValue& value)
#if CUI_ENABLE_DYNAMIC_XAML
	{
		(void)property;
		(void)value;
		return false;
	}
#else
		= 0;
#endif
	virtual bool TryGetPropertyMetadata(
		BindingSourcePropertyToken property,
		BindingSourcePropertyMetadata& out) const
#if CUI_ENABLE_DYNAMIC_XAML
	{
		(void)property;
		(void)out;
		return false;
	}
#else
		= 0;
#endif
#if CUI_ENABLE_DYNAMIC_XAML
	/** Dynamic/name compatibility surface owned exclusively by the Design ABI. */
	virtual bool TryGetValue(
		const std::wstring& propertyName,
		BindingValue& out) const = 0;
	virtual bool TrySetValue(
		const std::wstring& propertyName,
		const BindingValue& value) = 0;
	/** Optional discovery API. Existing custom sources may keep the defaults. */
	virtual bool TryGetPropertyMetadata(
		const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const
	{
		(void)propertyName;
		(void)out;
		return false;
	}
	virtual std::vector<BindingSourcePropertyMetadata> GetProperties() const
	{
		return {};
	}
	/** Optional field/object validation. Empty propertyName addresses object-level issues. */
	virtual std::vector<BindingValidationIssue> GetValidationIssues(
		const std::wstring& propertyName) const
	{
		(void)propertyName;
		return {};
	}
#endif
	virtual std::vector<BindingValidationIssue> GetValidationIssues(
		BindingSourcePropertyToken property) const
	{
		(void)property;
		return {};
	}
	/** Returns null when the source exposes only snapshot validation state. */
	virtual BindingValidationChangedEvent* ValidationChanged() noexcept
	{
		return nullptr;
	}
	std::weak_ptr<const void> BindingLifetime() const
	{
		if (!_bindingLifetime)
			_bindingLifetime = std::make_shared<int>(0);
		return _bindingLifetime;
	}

private:
	mutable std::shared_ptr<const void> _bindingLifetime;
};

/**
 * Optional WPF IEditableObject-shaped transaction capability for one binding
 * record. Controls discover it with dynamic_cast; ordinary IBindingSource
 * implementations remain read/write compatible without implementing it.
 *
 * BeginEdit must retain at most this record's pending state. Repeated calls
 * while a transaction is active are idempotent. EndEdit accepts the pending
 * values and CancelEdit restores the values captured by BeginEdit.
 */
class IEditableBindingSource
{
public:
	virtual ~IEditableBindingSource() = default;
	virtual bool BeginEdit() = 0;
	virtual bool EndEdit() = 0;
	virtual bool CancelEdit() = 0;
};

#if CUI_ENABLE_DYNAMIC_XAML
/** Reads a dynamic member/indexer path from any Design binding source. */
bool TryGetBindingPathValue(
	const IBindingSource& source,
	const std::wstring& path,
	BindingValue& out);
#endif
/** Executes an AOT path without parsing or retaining a source-path string. */
bool TryGetBindingPathValue(
	const IBindingSource& source,
	CompiledBindingPathView path,
	BindingValue& out);

/**
 * Owns an intermediate IBindingSource used by a dotted source property path.
 * Keeping the reference explicit avoids unsafe raw pointers inside BindingValue.
 */
class BindingSourceReference final
{
public:
	BindingSourceReference() = default;
	explicit BindingSourceReference(std::shared_ptr<IBindingSource> source)
		: _source(std::move(source)) {}

	template<typename T>
		requires std::is_base_of_v<IBindingSource, T>
	explicit BindingSourceReference(std::shared_ptr<T> source)
		: _source(std::move(source)) {}

	IBindingSource* Get() const noexcept { return _source.get(); }
	const std::shared_ptr<IBindingSource>& Shared() const noexcept { return _source; }
	explicit operator bool() const noexcept { return static_cast<bool>(_source); }
	bool operator==(const BindingSourceReference& other) const noexcept
	{
		return _source == other._source;
	}

private:
	std::shared_ptr<IBindingSource> _source;
};

/**
 * Stable binding source identity whose current backing object may be replaced.
 * Bindings subscribe once to the proxy and are refreshed when an inherited
 * DataContext changes, including replacement of an intermediate object.
 */
class BindingSourceProxy final : public IBindingSource
{
public:
	BindingSourceProxy() = default;
	explicit BindingSourceProxy(BindingSourceReference source);

	void SetSource(BindingSourceReference source);
	const BindingSourceReference& Source() const noexcept { return _source; }

#if CUI_ENABLE_DYNAMIC_XAML
	bool TryGetValue(const std::wstring& propertyName,
		BindingValue& out) const override;
#endif
	bool TryGetValue(BindingSourcePropertyToken property,
		BindingValue& out) const override;
#if CUI_ENABLE_DYNAMIC_XAML
	bool TrySetValue(const std::wstring& propertyName,
		const BindingValue& value) override;
#endif
	bool TrySetValue(BindingSourcePropertyToken property,
		const BindingValue& value) override;
#if CUI_ENABLE_DYNAMIC_XAML
	bool TryGetPropertyMetadata(const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const override;
#endif
	bool TryGetPropertyMetadata(BindingSourcePropertyToken property,
		BindingSourcePropertyMetadata& out) const override;
#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<BindingSourcePropertyMetadata> GetProperties() const override;
	std::vector<BindingValidationIssue> GetValidationIssues(
		const std::wstring& propertyName) const override;
#endif
	std::vector<BindingValidationIssue> GetValidationIssues(
		BindingSourcePropertyToken property) const override;
	BindingValidationChangedEvent* ValidationChanged() noexcept override
	{
		return &_validationChanged;
	}
	PropertyChangedEvent& PropertyChanged() override { return _propertyChanged; }

private:
	BindingSourceReference _source;
	EventConnection _propertyConnection;
	EventConnection _validationConnection;
	PropertyChangedEvent _propertyChanged;
	BindingValidationChangedEvent _validationChanged;
	void Attach();
};

/** Collects object and field issues along a dotted source path. */
#if CUI_ENABLE_DYNAMIC_XAML
std::vector<BindingValidationIssue> GetBindingValidationIssuesForPath(
	const IBindingSource& source,
	const std::wstring& sourcePropertyPath);
#endif
std::vector<BindingValidationIssue> GetBindingValidationIssuesForPath(
	const IBindingSource& source,
	CompiledBindingPathView sourcePropertyPath);

class ObservableObject : public IBindingSource, public IEditableBindingSource
{
public:
	PropertyChangedEvent& PropertyChanged() override { return _propertyChanged; }
	BindingValidationChangedEvent* ValidationChanged() noexcept override
	{
		return &_validationChanged;
	}

	/** Design overrides IBindingSource; Production keeps this only as an explicit
	 * ObservableObject convenience and never exposes it through the source ABI. */
	bool TryGetValue(const std::wstring& propertyName, BindingValue& out) const;
	bool TryGetValue(BindingSourcePropertyToken property, BindingValue& out) const override;
	bool TrySetValue(const std::wstring& propertyName, const BindingValue& value);
	bool TrySetValue(BindingSourcePropertyToken property,
		const BindingValue& value) override;
	bool TryGetPropertyMetadata(
		const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const;
	bool TryGetPropertyMetadata(
		BindingSourcePropertyToken property,
		BindingSourcePropertyMetadata& out) const override;
	#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<BindingSourcePropertyMetadata> GetProperties() const override;
	#endif
	std::vector<BindingValidationIssue> GetValidationIssues(
		const std::wstring& propertyName) const;
	std::vector<BindingValidationIssue> GetValidationIssues(
		BindingSourcePropertyToken property) const override;
	bool HasValidationIssues() const noexcept;
	bool HasValidationErrors() const noexcept;
	bool HasValidationErrors(const std::wstring& propertyName) const;
	bool HasValidationErrors(BindingSourcePropertyToken property) const;
	bool BeginEdit() override;
	bool EndEdit() override;
	bool CancelEdit() override;

	/** Defines metadata and an optional initial value without requiring CanWrite. */
	#if CUI_ENABLE_DYNAMIC_XAML
	bool DefineProperty(
		BindingSourcePropertyMetadata metadata,
		const BindingValue& initialValue = {},
		bool replaceExisting = false);
	#endif
	bool DefineProperty(
		BindingSourcePropertyToken property,
		BindingSourcePropertyMetadata metadata,
		const BindingValue& initialValue = {},
		bool replaceExisting = false);

	template<typename T>
	bool DefineProperty(
		std::wstring name,
		T initialValue,
		bool canRead = true,
		bool canWrite = true,
		bool canObserve = true,
		bool replaceExisting = false)
	{
		BindingValue value(std::move(initialValue));
		#if CUI_ENABLE_DYNAMIC_XAML
		return DefineProperty(
			{ std::move(name), value.Kind(), std::type_index(value.Type()),
				canRead, canWrite, canObserve },
			value,
			replaceExisting);
		#else
		const auto property = MakeBindingSourcePropertyToken(name);
		return property && DefineProperty(
			property,
			{ value.Kind(), std::type_index(value.Type()),
				canRead, canWrite, canObserve },
			value,
			replaceExisting);
		#endif
	}
	bool RemoveProperty(const std::wstring& propertyName);
	bool RemoveProperty(BindingSourcePropertyToken property);

	template<typename T>
	T GetValue(const std::wstring& propertyName, const T& defaultValue = T{}) const
	{
		BindingValue value;
		if (!TryGetValue(propertyName, value))
			return defaultValue;

		T result{};
		return value.TryGet(result) ? result : defaultValue;
	}

	template<typename T>
	T GetValue(BindingSourcePropertyToken property, const T& defaultValue = T{}) const
	{
		BindingValue value;
		if (!TryGetValue(property, value))
			return defaultValue;

		T result{};
		return value.TryGet(result) ? result : defaultValue;
	}

	template<typename T>
	bool SetValue(const std::wstring& propertyName, T value)
	{
		return TrySetValue(propertyName, BindingValue(std::move(value)));
	}

	template<typename T>
	bool SetValue(BindingSourcePropertyToken property, T value)
	{
		return TrySetValue(property, BindingValue(std::move(value)));
	}

protected:
	void OnPropertyChanged(const std::wstring& propertyName);
	void OnPropertyChanged(BindingSourcePropertyToken property);
	/** Replaces the issues for one property; empty propertyName is object-level. */
	bool SetValidationIssues(
		const std::wstring& propertyName,
		std::vector<BindingValidationIssue> issues);
	bool SetValidationIssues(
		BindingSourcePropertyToken property,
		std::vector<BindingValidationIssue> issues);
	bool SetValidationError(
		const std::wstring& propertyName,
		std::wstring message,
		std::wstring code = {});
	bool SetValidationError(
		BindingSourcePropertyToken property,
		std::wstring message,
		std::wstring code = {});
	bool ClearValidationIssues(const std::wstring& propertyName);
	bool ClearValidationIssues(BindingSourcePropertyToken property);
	bool ClearAllValidationIssues();
	/** Updates declared read-only properties from a derived view-model. */
	bool SetCurrentValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		bool notify = true);
	bool SetCurrentValue(
		BindingSourcePropertyToken property,
		const BindingValue& value,
		bool notify = true);

	template<typename T>
		requires (!std::is_same_v<std::remove_cvref_t<T>, BindingValue>)
	bool SetCurrentValue(const std::wstring& propertyName, T value, bool notify = true)
	{
		return SetCurrentValue(propertyName, BindingValue(std::move(value)), notify);
	}

	template<typename T>
		requires (!std::is_same_v<std::remove_cvref_t<T>, BindingValue>)
	bool SetCurrentValue(BindingSourcePropertyToken property, T value, bool notify = true)
	{
		return SetCurrentValue(property, BindingValue(std::move(value)), notify);
	}

private:
	bool NormalizeValue(
		BindingSourcePropertyMetadata& metadata,
		const BindingValue& value,
		BindingValue& out) const;
	PropertyChangedEvent _propertyChanged;
	BindingValidationChangedEvent _validationChanged;
	std::unordered_map<std::uint64_t, BindingValue> _values;
	std::unordered_map<std::uint64_t, BindingSourcePropertyMetadata> _metadata;
	std::unordered_map<std::uint64_t, std::vector<BindingValidationIssue>>
		_validationIssues;
	// Allocated only for the one record which opted into an edit transaction.
	std::optional<std::unordered_map<std::uint64_t, BindingValue>>
		_editValues;
};

/** Ordered sources contributing to a DependencyObject property's effective value. */
enum class DependencyPropertyValueSource : unsigned char
{
	Default = 0,
	Inherited = 1,
	Theme = 2,
	Style = 3,
	/** Values supplied by a ControlTemplate, including TemplateBinding. */
	Template = 4,
	/** Setter values supplied by the active template visual state. */
	VisualState = 5,
	/** Literal values and local expressions such as Binding/DynamicResource. */
	Local = 6,
	/** Values supplied by active animation clocks. */
	Animation = 7
};

const wchar_t* DependencyPropertyValueSourceName(
	DependencyPropertyValueSource source) noexcept;

/** Expression identity stored inside one effective-value source slot. */
enum class DependencyPropertyExpressionKind : unsigned char
{
	None = 0,
	Binding,
	DynamicResource,
	TemplateBinding,
	Animation
};

const wchar_t* DependencyPropertyExpressionKindName(
	DependencyPropertyExpressionKind kind) noexcept;

/** Runtime work automatically requested after an effective control property changes. */
enum class DependencyPropertyFlags : unsigned char
{
	None = 0,
	AffectsMeasure = 1u << 0,
	AffectsArrange = 1u << 1,
	AffectsRender = 1u << 2,
	/** The effective value flows through the logical tree to matching properties. */
	Inherits = 1u << 4,
	/** BindingMode::Default resolves to TwoWay instead of OneWay. */
	BindsTwoWayByDefault = 1u << 5,
	/** Changing the property invalidates the logical parent's measure pass. */
	AffectsParentMeasure = 1u << 6,
	/** Changing the property invalidates the logical parent's arrange pass. */
	AffectsParentArrange = 1u << 7
};

constexpr DependencyPropertyFlags operator|(
	DependencyPropertyFlags left,
	DependencyPropertyFlags right) noexcept
{
	return static_cast<DependencyPropertyFlags>(
		static_cast<unsigned char>(left) | static_cast<unsigned char>(right));
}

constexpr DependencyPropertyFlags& operator|=(
	DependencyPropertyFlags& left,
	DependencyPropertyFlags right) noexcept
{
	left = left | right;
	return left;
}

constexpr bool HasDependencyPropertyFlag(
	DependencyPropertyFlags value,
	DependencyPropertyFlags flag) noexcept
{
	return (static_cast<unsigned char>(value)
		& static_cast<unsigned char>(flag)) != 0;
}

/** Preferred editor used by metadata-driven design tools. */
#if CUI_ENABLE_DESIGN_METADATA
enum class DependencyPropertyEditorKind : unsigned char
{
	Auto,
	Text,
	Boolean,
	Number,
	Choice,
	Color,
	Thickness,
	Size,
	Length
};

/** Where a design tool should persist an edited property value. */
enum class DependencyPropertyPersistence : unsigned char
{
	/** Let the design tool choose its native metadata representation. */
	Automatic,
	/** A native XAML member owns persistence for this property. */
	Native,
	/** Persist through the generic typed metadata-property bag. */
	Metadata,
	/** Runtime-only state that should not be persisted by a design tool. */
	Transient
};

/** One strongly typed item for a Choice editor. */
struct DependencyPropertyChoice
{
	std::wstring DisplayName;
	BindingValue Value;
};

/**
 * Optional presentation contract consumed by metadata-driven design tools.
 * Empty/default values preserve the historical discoverable-property behavior.
 */
struct DependencyPropertyDesignMetadata
{
	bool Browsable = true;
	std::wstring DisplayName;
	std::wstring Category = L"Misc";
	int CategoryOrder = 1000;
	int Order = 0;
	DependencyPropertyEditorKind Editor = DependencyPropertyEditorKind::Auto;
	std::vector<DependencyPropertyChoice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	DependencyPropertyPersistence Persistence = DependencyPropertyPersistence::Automatic;
	/** Optional target-sensitive visibility, evaluated after Browsable. */
	std::function<bool(DependencyObject&)> BrowsableWhen;
};
#endif

/**
 * Behavioral metadata layered on top of a bindable property registration.
 * Validate describes the property-wide value contract and is evaluated before
 * target-sensitive coercion. Coerce returns nullopt when it cannot produce an
 * effective value, or the effective value to apply.
 */
template<typename TOwner, typename TValue>
struct DependencyPropertyOptions
{
	std::optional<TValue> DefaultValue;
	DependencyPropertyFlags Flags = DependencyPropertyFlags::None;
	std::function<std::optional<TValue>(TOwner&, const TValue&)> Coerce;
	std::function<void(TOwner&, const TValue&, const TValue&)> Changed;
	std::function<bool(const TValue&, const TValue&)> Equals;
#if CUI_ENABLE_DESIGN_METADATA
	DependencyPropertyDesignMetadata Design;
#endif
	/** Concrete trigger used when Binding requests DataSourceUpdateMode::Default. */
	DataSourceUpdateMode DefaultUpdateMode =
		DataSourceUpdateMode::OnPropertyChanged;
	/** Exposes a readable/observable property without a public property-system setter. */
	bool IsReadOnly = false;
	/** Optional WPF-style type conversion used before coercion and assignment. */
	std::function<std::optional<TValue>(const BindingValue&)> Convert;
	/**
	 * WPF-style ValidateValueCallback. Unlike Coerce this callback is
	 * target-independent and validates both registered defaults and every
	 * proposed value before it enters an effective-value slot.
	 */
	std::function<bool(const TValue&)> Validate;
};

class DependencyPropertyKey;
class DependencyPropertyMetadata;
#if CUI_ENABLE_DYNAMIC_XAML
class DependencyPropertyMetadataCache;
#endif
class DependencyPropertyRegistration;
class DependencyPropertyKeyRegistration;
class DependencyPropertyMetadataRegistration;
class DependencyPropertyRegistry;

#if !CUI_ENABLE_DYNAMIC_XAML
namespace cui::property_system_detail
{
	[[nodiscard]] inline const std::wstring& EmptyDependencyPropertyText() noexcept
	{
		static const std::wstring empty;
		return empty;
	}
}
#endif

/**
 * Process-lifetime identity of one dependency property.
 *
 * Metadata may vary by owner/derived type, while this identity and its
 * validation/read-only contract remain stable across AddOwner and overrides.
 */
class DependencyProperty final
{
public:
	const std::wstring& Name() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _name;
#else
		return cui::property_system_detail::EmptyDependencyPropertyText();
#endif
	}
	BindingSourcePropertyToken BindingSourceToken() const noexcept
	{
		return _bindingSourceToken;
	}
	BindingValueKind ValueKind() const noexcept { return _valueKind; }
	const std::type_index& ValueType() const noexcept { return _valueType; }
	const std::type_index& OwnerType() const noexcept { return _ownerType; }
	std::size_t GlobalIndex() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _globalIndex;
#else
		return reinterpret_cast<std::size_t>(this);
#endif
	}
	bool ReadOnly() const noexcept
	{
		return static_cast<bool>(_readOnlyAuthorization);
	}
	bool IsValidValue(const BindingValue& value) const;

	DependencyProperty(const DependencyProperty&) = delete;
	DependencyProperty& operator=(const DependencyProperty&) = delete;

private:
	using Validator = std::function<bool(const BindingValue&)>;

#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _name;
#endif
	BindingSourcePropertyToken _bindingSourceToken;
	BindingValueKind _valueKind = BindingValueKind::Empty;
	std::type_index _valueType{ typeid(void) };
	std::type_index _ownerType{ typeid(void) };
#if CUI_ENABLE_DYNAMIC_XAML
	std::size_t _globalIndex = 0;
#endif
	Validator _validator;
	std::shared_ptr<const unsigned char> _readOnlyAuthorization;
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design-only immutable metadata index used by the legacy schema registry. */
	std::shared_ptr<DependencyPropertyMetadataCache> _metadataCache;
#endif
	/** Accessor-owned default metadata in Production; declarative metadata in Design. */
	const DependencyPropertyMetadata* _standaloneMetadata = nullptr;
#if !CUI_ENABLE_DYNAMIC_XAML
	/**
	 * Intrusive, property-local relation layers. Nodes live in accessor-local
	 * statics, so publication needs no owner table, allocation, or lock.
	 */
	mutable std::atomic<const DependencyPropertyMetadataRegistration*>
		_staticMetadataRelations{ nullptr };
#endif

	DependencyProperty(
		std::wstring name,
		BindingValueKind valueKind,
		std::type_index valueType,
		std::type_index ownerType,
		std::size_t globalIndex,
		Validator validator,
		std::shared_ptr<const unsigned char> readOnlyAuthorization);

	bool Authorizes(const DependencyPropertyKey& key) const noexcept;

	friend class DependencyPropertyKey;
	friend class DependencyPropertyMetadata;
#if CUI_ENABLE_DYNAMIC_XAML
	friend class DependencyPropertyMetadataCache;
#endif
	friend class DependencyPropertyRegistration;
	friend class DependencyPropertyKeyRegistration;
	friend class DependencyPropertyMetadataRegistration;
	friend class DependencyPropertyRegistry;
	friend class DependencyObject;
};

/**
 * One target dependency-property operand lowered either to its
 * process-lifetime identity or, only for a dynamically materialized design
 * document, to a late-bound name. Production instances contain exactly the
 * identity pointer, so repeated Style/Trigger entries neither retain nor
 * resolve target-property strings at runtime.
 */
class DependencyPropertyReference final
{
public:
	DependencyPropertyReference() = default;
	explicit DependencyPropertyReference(
		const DependencyProperty& property) noexcept
		: _property(&property)
	{
	}
#if CUI_ENABLE_DYNAMIC_XAML
	explicit DependencyPropertyReference(std::wstring dynamicName)
		: _dynamicName(std::move(dynamicName))
	{
	}
#endif

	[[nodiscard]] const DependencyProperty* Identity() const noexcept
	{
		return _property;
	}
	[[nodiscard]] bool IsCompiled() const noexcept
	{
		return _property != nullptr;
	}
	[[nodiscard]] bool Empty() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return !_property && _dynamicName.empty();
#else
		return !_property;
#endif
	}
	[[nodiscard]] const std::wstring& Name() const noexcept
	{
		if (_property) return _property->Name();
#if CUI_ENABLE_DYNAMIC_XAML
		return _dynamicName;
#else
		static const std::wstring empty;
		return empty;
#endif
	}

	[[nodiscard]] bool Matches(
		const DependencyPropertyReference& other) const noexcept
	{
		if (_property && other._property) return _property == other._property;
		if (_property || other._property) return false;
#if CUI_ENABLE_DYNAMIC_XAML
		return _dynamicName == other._dynamicName;
#else
		return true;
#endif
	}
	[[nodiscard]] bool Matches(
		const DependencyProperty* property,
		const std::wstring& propertyName) const noexcept
	{
		if (_property) return property && _property == property;
#if CUI_ENABLE_DYNAMIC_XAML
		return _dynamicName == propertyName;
#else
		return !property && propertyName.empty();
#endif
	}

private:
	const DependencyProperty* _property = nullptr;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _dynamicName;
#endif
};

#if !CUI_ENABLE_DYNAMIC_XAML
static_assert(sizeof(DependencyPropertyReference)
	== sizeof(const DependencyProperty*));
#endif

/**
 * One binding-source operand. Production contains only the dependency-property
 * identity used by TemplateBinding. Name/path storage belongs exclusively to
 * the dynamic Design lane; ordinary AOT bindings use CompiledBindingPathView.
 */
class BindingSourcePropertyReference final
{
public:
	BindingSourcePropertyReference() = default;
	explicit BindingSourcePropertyReference(
		const DependencyProperty& property) noexcept
		: _property(&property)
	{
	}
#if CUI_ENABLE_DYNAMIC_XAML
	explicit BindingSourcePropertyReference(std::wstring propertyPath)
		: _propertyPath(std::move(propertyPath))
	{
	}
#endif

	[[nodiscard]] const DependencyProperty* Identity() const noexcept
	{
		return _property;
	}
	[[nodiscard]] bool IsCompiled() const noexcept
	{
		return _property != nullptr;
	}
	[[nodiscard]] bool Empty() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return !_property && _propertyPath.empty();
#else
		return !_property;
#endif
	}
	[[nodiscard]] const std::wstring& Name() const noexcept
	{
		if (_property) return _property->Name();
#if CUI_ENABLE_DYNAMIC_XAML
		return _propertyPath;
#else
		static const std::wstring empty;
		return empty;
#endif
	}

private:
	const DependencyProperty* _property = nullptr;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _propertyPath;
#endif
};

#if !CUI_ENABLE_DYNAMIC_XAML
static_assert(sizeof(BindingSourcePropertyReference)
	== sizeof(const DependencyProperty*));
#endif

/** Capability object required to mutate one registered read-only property. */
class DependencyPropertyKey final
{
public:
	const DependencyProperty& Property() const noexcept { return *_property; }

private:
	const DependencyProperty* _property = nullptr;
	std::shared_ptr<const unsigned char> _authorization;

	DependencyPropertyKey(
		const DependencyProperty& property,
		std::shared_ptr<const unsigned char> authorization)
		: _property(&property),
		  _authorization(std::move(authorization))
	{
	}

	friend class DependencyObject;
	friend class DependencyProperty;
	friend class DependencyPropertyRegistry;
	friend class DependencyPropertyKeyRegistration;
};

/**
 * Per-type behavior for a DependencyProperty. Reading, writing, subscribing,
 * defaults, coercion and invalidation may be overridden without changing the
 * stable property identity used by effective-value storage.
 */
class DependencyPropertyMetadata final
{
public:
	using ChangeHandler = DependencyPropertyChangeHandler;

	const DependencyProperty& Property() const noexcept { return *_property; }
	const std::wstring& Name() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _property ? _property->Name() : _name;
#else
		return cui::property_system_detail::EmptyDependencyPropertyText();
#endif
	}
	BindingValueKind ValueKind() const noexcept
	{
		return _property ? _property->ValueKind() : _valueKind;
	}
	const std::type_index& ValueType() const noexcept
	{
		return _property ? _property->ValueType() : _valueType;
	}
	const std::type_index& OwnerType() const noexcept { return _ownerType; }
	bool CanRead() const noexcept
	{
		return _usesEffectiveValueStorage || static_cast<bool>(_getter);
	}
	bool CanWrite() const noexcept
	{
		return (_usesEffectiveValueStorage || static_cast<bool>(_setter))
			&& !IsReadOnly();
	}
	bool UsesEffectiveValueStorage() const noexcept
	{
		return _usesEffectiveValueStorage;
	}
	bool IsReadOnly() const noexcept
	{
		return _property ? _property->ReadOnly() : _isReadOnly;
	}
	bool CanObserve() const noexcept
	{
		return _usesGenericObservation || static_cast<bool>(_subscriber);
	}
	bool UsesGenericObservation() const noexcept
	{
		return _usesGenericObservation;
	}
	bool HasDefaultValue() const noexcept { return _hasDefaultValue; }
	DependencyPropertyFlags Flags() const noexcept { return _flags; }
	DataSourceUpdateMode DefaultUpdateMode() const noexcept
	{
		return _defaultUpdateMode;
	}
	const std::wstring& InheritanceKey() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _inheritanceKey;
#else
		return cui::property_system_detail::EmptyDependencyPropertyText();
#endif
	}
#if CUI_ENABLE_DESIGN_METADATA
	const DependencyPropertyDesignMetadata& Design() const noexcept { return _design; }
	bool IsDesignerBrowsable(DependencyObject& target) const;
#endif
	bool HasSameInheritanceIdentity(
		const DependencyPropertyMetadata& other) const noexcept;

	bool Matches(const DependencyObject& target) const;
	bool TryConvert(const BindingValue& value, BindingValue& out) const;
	bool IsValidValue(const BindingValue& value) const;
	bool TryCoerce(DependencyObject& target, const BindingValue& value, BindingValue& out) const;
	bool ValuesEqual(const BindingValue& left, const BindingValue& right) const;
	bool TryGetDefaultValue(BindingValue& out) const;
	bool TryGet(DependencyObject& target, BindingValue& out) const;
	bool TrySet(DependencyObject& target, const BindingValue& value) const;
	EventConnection Subscribe(DependencyObject& target, ChangeHandler handler, DataSourceUpdateMode updateMode) const;

private:
	using Matcher = std::function<bool(const DependencyObject&)>;
	using ValueConverter = std::function<bool(const BindingValue&, BindingValue&)>;
	using Validator = std::function<bool(const BindingValue&)>;
	using Coercer = std::function<bool(DependencyObject&, const BindingValue&, BindingValue&)>;
	using Comparer = std::function<bool(const BindingValue&, const BindingValue&)>;
	using Getter = std::function<bool(DependencyObject&, BindingValue&)>;
	using Setter = std::function<bool(DependencyObject&, const BindingValue&)>;
	using Subscriber = std::function<EventConnection(DependencyObject&, ChangeHandler, DataSourceUpdateMode)>;
	using Changed = std::function<void(DependencyObject&, const BindingValue&, const BindingValue&)>;

#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _name;
#endif
	BindingValueKind _valueKind = BindingValueKind::Empty;
	std::type_index _valueType{ typeid(void) };
	std::type_index _ownerType{ typeid(void) };
	Matcher _matcher;
	ValueConverter _valueConverter;
	Validator _validator;
	Coercer _coercer;
	Comparer _comparer;
	Getter _getter;
	Setter _setter;
	Subscriber _subscriber;
	Changed _changed;
	BindingValue _defaultValue;
	bool _hasDefaultValue = false;
	bool _usesEffectiveValueStorage = false;
	bool _usesGenericObservation = false;
	DependencyPropertyFlags _flags = DependencyPropertyFlags::None;
	bool _isReadOnly = false;
	DataSourceUpdateMode _defaultUpdateMode =
		DataSourceUpdateMode::OnPropertyChanged;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _inheritanceKey;
#endif
#if CUI_ENABLE_DESIGN_METADATA
	DependencyPropertyDesignMetadata _design;
#endif
	const DependencyProperty* _property = nullptr;

	DependencyPropertyMetadata(std::wstring name,
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
		);

	void NotifyChanged(
		DependencyObject& target,
		const BindingValue& oldValue,
		const BindingValue& newValue) const;
	bool CanWriteInternally() const noexcept
	{
		return _usesEffectiveValueStorage || static_cast<bool>(_setter);
	}
	bool TrySetInternal(DependencyObject& target, const BindingValue& value) const;
	/** Applies a value already converted and coerced by the effective-value pipeline. */
	bool TrySetEffective(DependencyObject& target, const BindingValue& value) const;
	void AttachProperty(const DependencyProperty& property) noexcept
	{
		_property = &property;
	}
	void MarkGenericObservation() noexcept
	{
		_usesGenericObservation = true;
	}
	void MergeBaseMetadata(const DependencyPropertyMetadata& base);

	friend class DependencyPropertyRegistry;
#if CUI_ENABLE_DYNAMIC_XAML
	friend class DependencyPropertyMetadataCache;
#endif
	friend class DependencyPropertyRegistration;
	friend class DependencyPropertyKeyRegistration;
	friend class DependencyPropertyMetadataRegistration;
	friend class DependencyObject;
	friend class Control;
#if CUI_ENABLE_DYNAMIC_XAML
	friend class cui::details::DependencyPropertyStandaloneAccess;
#endif
};

/**
 * Caller-owned process-lifetime storage for one dependency-property identity
 * and its default metadata.
 *
 * Production keeps both objects inline in the accessor's function-local static
 * and never publishes them through the name registry. Design builds retain the
 * existing registry-backed behavior so schema enumeration and metadata tools
 * continue to see the same property surface.
 */
class DependencyPropertyRegistration final
{
public:
	DependencyPropertyRegistration(
		const DependencyPropertyRegistration&) = delete;
	DependencyPropertyRegistration& operator=(
		const DependencyPropertyRegistration&) = delete;
	DependencyPropertyRegistration(
		DependencyPropertyRegistration&&) = delete;
	DependencyPropertyRegistration& operator=(
		DependencyPropertyRegistration&&) = delete;

	[[nodiscard]] explicit operator bool() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _property != nullptr;
#else
		return true;
#endif
	}
	[[nodiscard]] const DependencyProperty& Property() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return *_property;
#else
		return _property;
#endif
	}
	[[nodiscard]] const DependencyProperty& operator*() const noexcept
	{
		return Property();
	}

private:
#if CUI_ENABLE_DYNAMIC_XAML
	const DependencyProperty* _property = nullptr;
	explicit DependencyPropertyRegistration(
		const DependencyProperty* property) noexcept;
#else
	// Property is constructed first so it can take ownership of the authored
	// name and identity validator before the remaining metadata is moved inline.
	DependencyProperty _property;
	DependencyPropertyMetadata _metadata;
	explicit DependencyPropertyRegistration(
		DependencyPropertyMetadata metadata,
		BindingSourcePropertyToken token);
#endif

	friend class DependencyPropertyRegistry;
};

/**
 * Caller-owned storage for one AddOwner/metadata-override relation.
 *
 * Design publishes the relation through the legacy schema registry and keeps
 * only its resulting pointer. Production owns the effective metadata inline,
 * merges its explicit immediate base exactly once, and links the stable node
 * only to the exact DependencyProperty identity.
 */
class DependencyPropertyMetadataRegistration final
{
public:
	DependencyPropertyMetadataRegistration(
		const DependencyPropertyMetadataRegistration&) = delete;
	DependencyPropertyMetadataRegistration& operator=(
		const DependencyPropertyMetadataRegistration&) = delete;
	DependencyPropertyMetadataRegistration(
		DependencyPropertyMetadataRegistration&&) = delete;
	DependencyPropertyMetadataRegistration& operator=(
		DependencyPropertyMetadataRegistration&&) = delete;

	[[nodiscard]] explicit operator bool() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _metadata != nullptr;
#else
		return true;
#endif
	}
	[[nodiscard]] const DependencyProperty& Property() const noexcept
	{
		return Metadata().Property();
	}
	[[nodiscard]] const DependencyPropertyMetadata& Metadata() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return *_metadata;
#else
		return _metadata;
#endif
	}
	[[nodiscard]] const DependencyPropertyMetadata& operator*() const noexcept
	{
		return Metadata();
	}

private:
#if CUI_ENABLE_DYNAMIC_XAML
	const DependencyPropertyMetadata* _metadata = nullptr;
	explicit DependencyPropertyMetadataRegistration(
		const DependencyPropertyMetadata* metadata) noexcept;
#else
	DependencyPropertyMetadata _metadata;
	const DependencyPropertyMetadataRegistration* _immediateBase = nullptr;
	const DependencyPropertyMetadataRegistration* _next = nullptr;

	DependencyPropertyMetadataRegistration(
		const DependencyProperty& property,
		DependencyPropertyMetadata metadata,
		const DependencyPropertyMetadataRegistration* immediateBase);
	[[nodiscard]] bool IsBasedOn(
		const DependencyPropertyMetadataRegistration& candidate) const noexcept;
#endif

	friend class DependencyObject;
	friend class DependencyPropertyRegistry;
};

/** Caller-owned static registration for a read-only dependency property. */
class DependencyPropertyKeyRegistration final
{
public:
	DependencyPropertyKeyRegistration(
		const DependencyPropertyKeyRegistration&) = delete;
	DependencyPropertyKeyRegistration& operator=(
		const DependencyPropertyKeyRegistration&) = delete;
	DependencyPropertyKeyRegistration(
		DependencyPropertyKeyRegistration&&) = delete;
	DependencyPropertyKeyRegistration& operator=(
		DependencyPropertyKeyRegistration&&) = delete;

	[[nodiscard]] explicit operator bool() const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		return _key.Property().ReadOnly();
#else
		return true;
#endif
	}
	[[nodiscard]] const DependencyPropertyKey& Key() const noexcept
	{
		return _key;
	}
	[[nodiscard]] const DependencyPropertyKey& operator*() const noexcept
	{
		return Key();
	}

private:
#if CUI_ENABLE_DYNAMIC_XAML
	DependencyPropertyKey _key;
	explicit DependencyPropertyKeyRegistration(
		DependencyPropertyKey key) noexcept;
#else
	// The empty-owner aliasing shared_ptr used by _property/_key points at this
	// byte without allocating a control block. The wrapper never moves, so the
	// authorization identity is process-stable.
	const unsigned char _authorization = 0;
	DependencyProperty _property;
	DependencyPropertyMetadata _metadata;
	DependencyPropertyKey _key;
	explicit DependencyPropertyKeyRegistration(
		DependencyPropertyMetadata metadata,
		BindingSourcePropertyToken token);
#endif

	friend class DependencyPropertyRegistry;
};

class DependencyPropertyRegistry final
{
public:
#if CUI_ENABLE_DYNAMIC_XAML
	/**
	 * Registers a WPF-style property whose value lives only in the
	 * DependencyObject effective-value store. CLR-shaped wrappers call
	 * DependencyObject Get/Set helpers; metadata callbacks own side effects.
	 */
	template<typename TOwner, typename TValue>
	static const DependencyProperty* Register(
		std::wstring name,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	/**
	 * Registers a slot-backed property while retaining an owner-specific
	 * observation trigger (for example TextBox.Text's LostFocus source update).
	 * The subscriber is behavior metadata; it does not become value storage.
	 */
	template<typename TOwner, typename TValue>
	static const DependencyProperty* Register(
		std::wstring name,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static const DependencyProperty* Register(
		std::wstring name,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif
	/**
	 * Creates accessor-owned static storage in Production and preserves normal
	 * registry publication in Design. Each call result must be retained in a
	 * function-local static; unlike Register, Production returns no global owner.
	 */
	template<typename TOwner, typename TValue>
	static DependencyPropertyRegistration RegisterStatic(
		std::wstring name,
		DependencyPropertyOptions<TOwner, TValue> options = {});
#if !CUI_ENABLE_DYNAMIC_XAML
	/**
	 * Production-only AOT entry point. The schema compiler supplies the stable
	 * member token directly, so neither the authored name nor a first-touch name
	 * hash is retained by the executable.
	 */
	template<typename TOwner, typename TValue>
	static DependencyPropertyRegistration RegisterStatic(
		BindingSourcePropertyToken token,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static DependencyPropertyRegistration RegisterStatic(
		BindingSourcePropertyToken token,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static DependencyPropertyRegistration RegisterStatic(
		BindingSourcePropertyToken token,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif
	template<typename TOwner, typename TValue>
	static DependencyPropertyRegistration RegisterStatic(
		std::wstring name,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static DependencyPropertyRegistration RegisterStatic(
		std::wstring name,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TOwner, typename TValue>
	static DependencyPropertyKey RegisterReadOnly(
		std::wstring name,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static DependencyPropertyKey RegisterReadOnly(
		std::wstring name,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif
	template<typename TOwner, typename TValue>
	static DependencyPropertyKeyRegistration RegisterReadOnlyStatic(
		std::wstring name,
		DependencyPropertyOptions<TOwner, TValue> options = {});
#if !CUI_ENABLE_DYNAMIC_XAML
	template<typename TOwner, typename TValue>
	static DependencyPropertyKeyRegistration RegisterReadOnlyStatic(
		BindingSourcePropertyToken token,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static DependencyPropertyKeyRegistration RegisterReadOnlyStatic(
		BindingSourcePropertyToken token,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif
	template<typename TOwner, typename TValue>
	static DependencyPropertyKeyRegistration RegisterReadOnlyStatic(
		std::wstring name,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TOwner, typename TValue>
	static const DependencyProperty* AddOwner(
		const DependencyProperty& property,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static const DependencyProperty* AddOwner(
		const DependencyProperty& property,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif
	/**
	 * Accessor-owned AddOwner relation. Production publishes only an intrusive
	 * layer on the exact property identity; Design mirrors the legacy registry.
	 */
	template<typename TOwner, typename TValue>
	static DependencyPropertyMetadataRegistration AddOwnerStatic(
		const DependencyProperty& property,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static DependencyPropertyMetadataRegistration AddOwnerStatic(
		const DependencyProperty& property,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
	/**
	 * Overrides metadata after forcing the owner's immediate C++ base to
	 * register. The explicit immediate base keeps WPF inheritance order
	 * deterministic even when a most-derived static accessor is touched first.
	 */
#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TOwner, typename TImmediateBase, typename TValue>
	static const DependencyPropertyMetadata* OverrideMetadata(
		const DependencyProperty& property,
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif
	/** Accessor-owned writable metadata override merged with default metadata. */
	template<typename TOwner, typename TImmediateBase, typename TValue>
	static DependencyPropertyMetadataRegistration OverrideMetadataStatic(
		const DependencyProperty& property,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	/**
	 * Accessor-owned override merged with an explicit immediate-base relation.
	 * This overload makes multi-level override chains independent of first-touch
	 * and publication order.
	 */
	template<typename TOwner, typename TImmediateBase, typename TValue>
	static DependencyPropertyMetadataRegistration OverrideMetadataStatic(
		const DependencyProperty& property,
		const DependencyPropertyMetadataRegistration& immediateBase,
		DependencyPropertyOptions<TOwner, TValue> options = {});
#if CUI_ENABLE_DYNAMIC_XAML
	template<typename TOwner, typename TValue>
	static const DependencyProperty* AddOwner(
		const DependencyPropertyKey& key,
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TValue>
	static const DependencyProperty* AddOwner(
		const DependencyPropertyKey& key,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		DependencyPropertyOptions<TOwner, TValue> options = {});
	template<typename TOwner, typename TImmediateBase, typename TValue>
	static const DependencyPropertyMetadata* OverrideMetadata(
		const DependencyPropertyKey& key,
		DependencyPropertyOptions<TOwner, TValue> options = {});
#endif

#if CUI_ENABLE_DYNAMIC_XAML
	static const DependencyPropertyMetadata* Find(DependencyObject& target, const std::wstring& propertyName);
	static const DependencyProperty* FindProperty(
		DependencyObject& target,
		const std::wstring& propertyName);
	/** Finds only C++ framework metadata; declarative schema members are excluded. */
	static const DependencyPropertyMetadata* FindNative(
		DependencyObject& target,
		const std::wstring& propertyName);
	/** Returns the effective metadata set for target, with derived overrides applied. */
	static std::vector<const DependencyPropertyMetadata*> GetProperties(DependencyObject& target);
	/**
	 * Schema-only native lookup. Callers supply the concrete C++ owner's base
	 * type closure after invoking its static dependency-property registrar.
	 * No DependencyObject instance is constructed or consulted.
	 */
	static const DependencyPropertyMetadata* FindRegistered(
		std::span<const std::type_index> ownerTypes,
		const std::wstring& propertyName);
	/** Schema-only effective native metadata with derived overrides applied. */
	static std::vector<const DependencyPropertyMetadata*> GetRegisteredProperties(
		std::span<const std::type_index> ownerTypes,
		std::function<bool(const DependencyPropertyMetadata&)> include = {});
	static const DependencyPropertyMetadata* Find(
		DependencyObject& target,
		BindingSourcePropertyToken property);
#endif
	static const DependencyPropertyMetadata* GetMetadata(
		DependencyObject& target,
		const DependencyProperty& property);

private:
	template<typename TOwner, typename TValue>
	static DependencyPropertyMetadata CreateMetadata(
		std::wstring name,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(
			TOwner&,
			DependencyPropertyMetadata::ChangeHandler,
			DataSourceUpdateMode)> subscriber,
		DependencyPropertyOptions<TOwner, TValue> options,
		bool usesEffectiveValueStorage,
		bool includeValidator);
#if CUI_ENABLE_DYNAMIC_XAML
	static const DependencyProperty* Register(
		DependencyPropertyMetadata metadata);
	static DependencyPropertyKey RegisterReadOnly(
		DependencyPropertyMetadata metadata);
	static const DependencyPropertyMetadata* AddOwner(
		const DependencyProperty& property,
		DependencyPropertyMetadata metadata,
		const DependencyPropertyKey* key);
	static const DependencyPropertyMetadata* OverrideMetadata(
		const DependencyProperty& property,
		DependencyPropertyMetadata metadata,
		const DependencyPropertyKey* key);
	static const DependencyPropertyMetadata* ResolveMetadata(
		const DependencyProperty& property,
		std::span<const DependencyPropertyMetadata* const> layers);
	static const DependencyPropertyMetadata* FindNativeCore(
		DependencyObject& target,
		const std::wstring& propertyName);
	static std::unique_ptr<DependencyProperty> CreateStandalone(
		DependencyPropertyMetadata& metadata);
#endif

#if CUI_ENABLE_DYNAMIC_XAML
	friend class cui::details::DependencyPropertyStandaloneAccess;
#endif
};

/** Resolves BindingMode::Default using the target property's behavior flags. */
BindingMode ResolveBindingMode(
	const DependencyPropertyMetadata& target,
	BindingMode requested) noexcept;

/** Resolves DataSourceUpdateMode::Default using the target property's metadata. */
DataSourceUpdateMode ResolveDataSourceUpdateMode(
	const DependencyPropertyMetadata& target,
	DataSourceUpdateMode requested) noexcept;

/**
 * Mutually-exclusive source operand shared by Binding and MultiBindingSource.
 *
 * Adapter-backed bindings retain either a borrowed source or an owning source
 * together with their compiled path. Direct AOT endpoints retain only the
 * compact handle. Keeping these lanes in one tagged RAII value prevents every
 * Binding from paying for both representations at once.
 */
class BindingSourceStorage final
{
public:
	BindingSourceStorage() = default;
	BindingSourceStorage(
		IBindingSource* source,
		CompiledBindingPathView path) noexcept
		: _value(BorrowedAdapter{ source, path })
	{
	}
	BindingSourceStorage(
		BindingSourceReference source,
		CompiledBindingPathView path) noexcept
		: _value(OwnedAdapter{ std::move(source), path })
	{
	}
	explicit BindingSourceStorage(CompiledSourceHandle source) noexcept
		: _value(source)
	{
	}

	[[nodiscard]] IBindingSource* AdapterSource() const noexcept
	{
		if (const auto* source = std::get_if<BorrowedAdapter>(&_value))
			return source->Source;
		if (const auto* source = std::get_if<OwnedAdapter>(&_value))
			return source->Source.Get();
		return nullptr;
	}
	[[nodiscard]] const BindingSourceReference* OwnedSource() const noexcept
	{
		if (const auto* source = std::get_if<OwnedAdapter>(&_value))
			return &source->Source;
		return nullptr;
	}
	[[nodiscard]] BindingSourceReference TakeOwnedSource() noexcept
	{
		if (auto* source = std::get_if<OwnedAdapter>(&_value))
			return std::move(source->Source);
		return {};
	}
	[[nodiscard]] CompiledSourceHandle DirectSource() const noexcept
	{
		if (const auto* source = std::get_if<CompiledSourceHandle>(&_value))
			return *source;
		return {};
	}
	[[nodiscard]] CompiledBindingPathView CompiledPath() const noexcept
	{
		if (const auto* source = std::get_if<BorrowedAdapter>(&_value))
			return source->Path;
		if (const auto* source = std::get_if<OwnedAdapter>(&_value))
			return source->Path;
		return {};
	}

private:
	struct BorrowedAdapter final
	{
		IBindingSource* Source = nullptr;
		CompiledBindingPathView Path;
	};
	struct OwnedAdapter final
	{
		BindingSourceReference Source;
		CompiledBindingPathView Path;
	};

	std::variant<std::monostate, BorrowedAdapter, OwnedAdapter,
		CompiledSourceHandle> _value;
};

// The previous representation permanently retained all four operands (72 B
// on x64). The tagged lane is 48 B and must retain at least one pointer-width
// of that saving as the alternatives evolve.
static_assert(sizeof(void*) != 8 || sizeof(BindingSourceStorage) == 48);
static_assert(sizeof(void*) != 8
	|| sizeof(BindingSourceStorage)
		<= sizeof(BindingSourceReference) + sizeof(IBindingSource*)
			+ sizeof(CompiledSourceHandle)
			+ sizeof(CompiledBindingPathView) - sizeof(void*));

class Binding
{
public:
#if CUI_ENABLE_DYNAMIC_XAML
	Binding(DependencyObject* target,
		std::wstring targetProperty,
		IBindingSource* source,
		std::wstring sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	Binding(DependencyObject* target,
		const DependencyProperty& targetProperty,
		IBindingSource* source,
		CompiledBindingPathView sourcePath,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
	Binding(DependencyObject* target,
		const DependencyProperty& targetProperty,
		CompiledSourceHandle source,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#if CUI_ENABLE_DYNAMIC_XAML
	Binding(DependencyObject* target,
		const DependencyProperty& targetProperty,
		IBindingSource* source,
		std::wstring sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
#if CUI_ENABLE_DYNAMIC_XAML
	Binding(DependencyObject* target,
		std::wstring targetProperty,
		BindingSourceReference source,
		std::wstring sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	Binding(DependencyObject* target,
		const DependencyProperty& targetProperty,
		BindingSourceReference source,
		CompiledBindingPathView sourcePath,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#if CUI_ENABLE_DYNAMIC_XAML
	Binding(DependencyObject* target,
		const DependencyProperty& targetProperty,
		BindingSourceReference source,
		std::wstring sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	~Binding();

	Binding(const Binding&) = delete;
	Binding& operator=(const Binding&) = delete;

	const std::wstring& TargetProperty() const { return _targetProperty.Name(); }
	const DependencyProperty* TargetPropertyIdentity() const noexcept
	{
		return _targetMetadata
			? &_targetMetadata->Property() : _targetProperty.Identity();
	}
	const std::wstring& SourceProperty() const { return _sourceProperty.Name(); }
	const DependencyProperty* SourcePropertyIdentity() const noexcept
	{
		return _sourceProperty.Identity();
	}
	[[nodiscard]] CompiledBindingPathView CompiledSourcePath() const noexcept
	{
		return _sourceStorage.CompiledPath();
	}
	[[nodiscard]] bool UsesCompiledSourcePath() const noexcept
	{
		return !CompiledSourcePath().Empty();
	}
	[[nodiscard]] bool UsesDirectSource() const noexcept
	{
		return static_cast<bool>(_sourceStorage.DirectSource());
	}
	BindingMode Mode() const { return _mode; }
	DataSourceUpdateMode UpdateMode() const { return _updateMode; }
	const std::shared_ptr<const IBindingValueConverter>& Converter() const noexcept { return _converter; }
	const std::optional<BindingValue>& FallbackValue() const noexcept
	{
		return _fallbackValue;
	}
	const std::optional<BindingValue>& TargetNullValue() const noexcept
	{
		return _targetNullValue;
	}
	const std::optional<BindingValue>& ConverterParameter() const noexcept
	{
		return _converterParameter;
	}
	const std::optional<std::wstring>& StringFormat() const noexcept
	{
		return _stringFormat;
	}
	bool IsValid() const noexcept { return _isValid; }
	BindingError LastError() const noexcept { return _lastError; }
	const wchar_t* LastErrorMessage() const noexcept { return BindingErrorMessage(_lastError); }
	const std::vector<BindingValidationIssue>& ValidationIssues() const noexcept
	{
		if (IsSourceAlive()) return _validationIssues;
		static const std::vector<BindingValidationIssue> empty;
		return empty;
	}
	bool HasValidationIssues() const noexcept { return !ValidationIssues().empty(); }
	bool HasValidationErrors() const noexcept;
	BindingValidationChangedEvent& ValidationChanged() noexcept
	{
		return _validationChanged;
	}

	bool UpdateTarget();
	bool UpdateSource();

private:
	struct State
	{
		Binding* Owner = nullptr;
	};

	DependencyObject* _target = nullptr;
	BindingSourceStorage _sourceStorage;
	DependencyObject* _sourceDependencyObject = nullptr;
	DependencyPropertyReference _targetProperty;
	BindingSourcePropertyReference _sourceProperty;
#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<BindingPathStep> _sourcePath;
#endif
	BindingMode _mode = BindingMode::Default;
	DataSourceUpdateMode _updateMode = DataSourceUpdateMode::Default;
	std::shared_ptr<const IBindingValueConverter> _converter;
	std::optional<BindingValue> _fallbackValue;
	std::optional<BindingValue> _targetNullValue;
	std::optional<BindingValue> _converterParameter;
	std::optional<std::wstring> _stringFormat;
	std::shared_ptr<State> _state;
	std::weak_ptr<const void> _targetLifetime;
	std::weak_ptr<const void> _sourceLifetime;
	std::vector<EventConnection> _sourceConnections;
	std::vector<std::shared_ptr<IBindingSource>> _sourcePathOwners;
	std::vector<std::shared_ptr<IBindingList>> _sourcePathListOwners;
	std::vector<EventConnection> _sourceValidationConnections;
	std::vector<EventConnection> _validationPathConnections;
	std::vector<std::shared_ptr<IBindingSource>> _validationPathOwners;
	std::vector<std::shared_ptr<IBindingList>> _validationPathListOwners;
	EventConnection _targetConnection;
	BindingValidationChangedEvent _validationChanged;
	std::vector<BindingValidationIssue> _validationIssues;
	bool _updatingTarget = false;
	bool _updatingSource = false;
	bool _ownsTargetValue = false;
	bool _isValid = false;
	BindingError _lastError = BindingError::None;
	const DependencyPropertyMetadata* _targetMetadata = nullptr;
	const DependencyPropertyMetadata* _sourceMetadata = nullptr;
	DependencyPropertyValueSource _targetValueSource =
		DependencyPropertyValueSource::Local;
	DependencyPropertyExpressionKind _expressionKind =
		DependencyPropertyExpressionKind::Binding;

	Binding(DependencyObject* target,
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
		DependencyPropertyExpressionKind expressionKind);
	Binding(DependencyObject* target,
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
		DependencyPropertyExpressionKind expressionKind);
	Binding(DependencyObject* target,
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
		DependencyPropertyExpressionKind expressionKind);
#if CUI_ENABLE_DYNAMIC_XAML
	Binding(DependencyObject* target,
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
		DependencyPropertyExpressionKind expressionKind);
	Binding(DependencyObject* target,
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
		DependencyPropertyExpressionKind expressionKind);
#endif
	Binding(DependencyObject* target,
		const DependencyProperty& targetProperty,
		DependencyObject& source,
		const DependencyProperty& sourceProperty,
		DependencyPropertyValueSource targetValueSource,
		DependencyPropertyExpressionKind expressionKind);

	void Attach();
	bool Validate();
	bool ValidateSourceMetadata();
	void AttachSourceChangedHandlers();
	void AttachValidationChangedHandlers();
	void AttachTargetChangedHandlers();
	void OnSourcePathChanged();
	void OnValidationPathChanged();
	void RefreshValidation();
	void OnTargetPropertyChanged();
	bool ApplyTargetValue(const BindingValue& value);
	bool ApplyFallbackValue(BindingError sourceError);
	bool TryReadSourcePathValue(
		BindingValue& out, BindingError& error) const;
	bool TryWriteSourcePathValue(
		const BindingValue& value, BindingError& error) const;
	bool Fail(BindingError error) noexcept
	{
		_lastError = error;
		return false;
	}
	bool IsTargetAlive() const noexcept;
	[[nodiscard]] IBindingSource* AdapterSource() const noexcept
	{
		return _sourceStorage.AdapterSource();
	}
	[[nodiscard]] CompiledSourceHandle DirectSource() const noexcept
	{
		return _sourceStorage.DirectSource();
	}
	bool IsSourceAlive() const noexcept
	{
		return (DirectSource() || AdapterSource())
			&& !_sourceLifetime.expired();
	}
	void DetachReplacedTargetExpression() noexcept;

	friend class Control;
	friend class DependencyObject;
	friend class BindingCollection;
};

/** One child expression consumed by MultiBinding. */
struct MultiBindingSource final
{
private:
	BindingSourceStorage _sourceStorage;

public:
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring SourceProperty;
#endif
	std::shared_ptr<const IBindingValueConverter> Converter;
	std::optional<BindingValue> FallbackValue;
	std::optional<BindingValue> TargetNullValue;
	std::optional<BindingValue> ConverterParameter;
	std::optional<std::wstring> StringFormat;
	std::optional<BindingMode> Mode;
	std::optional<DataSourceUpdateMode> UpdateMode;

	MultiBindingSource() = default;
#if CUI_ENABLE_DYNAMIC_XAML
	MultiBindingSource(
		IBindingSource* source,
		std::wstring sourceProperty,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
	MultiBindingSource(
		BindingSourceReference source,
		std::wstring sourceProperty,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	MultiBindingSource(
		IBindingSource* source,
		CompiledBindingPathView sourcePath,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
	MultiBindingSource(
		BindingSourceReference source,
		CompiledBindingPathView sourcePath,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
	MultiBindingSource(
		CompiledSourceHandle source,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});

	[[nodiscard]] IBindingSource* AdapterSource() const noexcept
	{
		return _sourceStorage.AdapterSource();
	}
	[[nodiscard]] const BindingSourceReference* OwnedSource() const noexcept
	{
		return _sourceStorage.OwnedSource();
	}
	[[nodiscard]] BindingSourceReference TakeOwnedSource() noexcept
	{
		return _sourceStorage.TakeOwnedSource();
	}
	[[nodiscard]] CompiledBindingPathView SourcePath() const noexcept
	{
		return _sourceStorage.CompiledPath();
	}
	[[nodiscard]] CompiledSourceHandle DirectSource() const noexcept
	{
		return _sourceStorage.DirectSource();
	}

	[[nodiscard]] bool UsesCompiledSourcePath() const noexcept
	{
		return !SourcePath().Empty();
	}
	[[nodiscard]] bool UsesDirectSource() const noexcept
	{
		return static_cast<bool>(DirectSource());
	}
};

#if !CUI_ENABLE_DYNAMIC_XAML
// Production x64 budgets measured after the mutually-exclusive source cutover.
static_assert(sizeof(void*) != 8 || sizeof(Binding) <= 808);
static_assert(sizeof(void*) != 8 || sizeof(MultiBindingSource) <= 368);
#endif

/**
 * WPF-style multi-source binding. Child expressions reuse Binding itself, so
 * PropertyPath observation, validation, fallbacks, and source lifetime rules
 * stay identical to ordinary bindings.
 */
class MultiBinding final
{
public:
#if CUI_ENABLE_DYNAMIC_XAML
	MultiBinding(
		DependencyObject* target,
		std::wstring targetProperty,
		std::vector<MultiBindingSource> sources,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IMultiBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	MultiBinding(
		DependencyObject* target,
		const DependencyProperty& targetProperty,
		std::vector<MultiBindingSource> sources,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IMultiBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
	~MultiBinding();

	MultiBinding(const MultiBinding&) = delete;
	MultiBinding& operator=(const MultiBinding&) = delete;

	const std::wstring& TargetProperty() const noexcept;
	const DependencyProperty* TargetPropertyIdentity() const noexcept;
	BindingMode Mode() const noexcept;
	DataSourceUpdateMode UpdateMode() const noexcept;
	size_t SourceCount() const noexcept;
	bool IsValid() const noexcept;
	BindingError LastError() const noexcept;
	const wchar_t* LastErrorMessage() const noexcept;
	Binding* TargetBinding() noexcept;
	const Binding* TargetBinding() const noexcept;
	/** Pulls every readable child source and refreshes the combined target value. */
	bool UpdateTarget();
	/** Reads the target, runs ConvertBack, and commits Explicit child bindings. */
	bool UpdateSource();
	std::vector<BindingValidationResult> GetValidationResults() const;
	bool HasValidationIssues() const;
	bool HasValidationErrors() const;
	BindingValidationChangedEvent& ValidationChanged() noexcept;

private:
	struct State;
	std::shared_ptr<State> _state;
	MultiBinding(
		DependencyObject* target,
		DependencyPropertyReference targetProperty,
		std::vector<MultiBindingSource> sources,
		BindingMode mode,
		DataSourceUpdateMode updateMode,
		std::shared_ptr<const IMultiBindingValueConverter> converter,
		std::optional<BindingValue> fallbackValue,
		std::optional<BindingValue> targetNullValue,
		std::optional<BindingValue> converterParameter,
		std::optional<std::wstring> stringFormat);
};

class BindingCollection
{
public:
	explicit BindingCollection(DependencyObject* owner);
	~BindingCollection();

#if CUI_ENABLE_DYNAMIC_XAML
	Binding* Add(const std::wstring& targetProperty,
		IBindingSource* source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	Binding* Add(const DependencyProperty& targetProperty,
		IBindingSource* source,
		CompiledBindingPathView sourcePath,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
	Binding* Add(const DependencyProperty& targetProperty,
		CompiledSourceHandle source,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#if CUI_ENABLE_DYNAMIC_XAML
	Binding* Add(const DependencyProperty& targetProperty,
		IBindingSource* source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
#if CUI_ENABLE_DYNAMIC_XAML
	Binding* Add(const std::wstring& targetProperty,
		BindingSourceReference source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	Binding* Add(const DependencyProperty& targetProperty,
		BindingSourceReference source,
		CompiledBindingPathView sourcePath,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#if CUI_ENABLE_DYNAMIC_XAML
	Binding* Add(const DependencyProperty& targetProperty,
		BindingSourceReference source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif

#if CUI_ENABLE_DYNAMIC_XAML
	Binding* Add(const std::wstring& targetProperty,
		IBindingSource& source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {})
	{
		return Add(targetProperty, &source, sourceProperty, mode, updateMode,
			std::move(converter), std::move(fallbackValue),
			std::move(targetNullValue), std::move(converterParameter),
			std::move(stringFormat));
	}
#endif
	Binding* Add(const DependencyProperty& targetProperty,
		IBindingSource& source,
		CompiledBindingPathView sourcePath,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {})
	{
		return Add(targetProperty, &source, sourcePath, mode, updateMode,
			std::move(converter), std::move(fallbackValue),
			std::move(targetNullValue), std::move(converterParameter),
			std::move(stringFormat));
	}
#if CUI_ENABLE_DYNAMIC_XAML
	Binding* Add(const DependencyProperty& targetProperty,
		IBindingSource& source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {})
	{
		return Add(targetProperty, &source, sourceProperty, mode, updateMode,
			std::move(converter), std::move(fallbackValue),
			std::move(targetNullValue), std::move(converterParameter),
			std::move(stringFormat));
	}
#endif
#if CUI_ENABLE_DYNAMIC_XAML
	/** Installs a one-way TemplateBinding in the Template precedence slot. */
	Binding* AddTemplateBinding(
		const std::wstring& targetProperty,
		IBindingSource& templatedParent,
		const std::wstring& sourceProperty);
#endif
	/** Compiled TemplateBinding: neither endpoint performs name lookup. */
	Binding* AddTemplateBinding(
		const DependencyProperty& targetProperty,
		DependencyObject& templatedParent,
		const DependencyProperty& sourceProperty);
#if CUI_ENABLE_DYNAMIC_XAML
	MultiBinding* AddMulti(
		const std::wstring& targetProperty,
		std::vector<MultiBindingSource> sources,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IMultiBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});
#endif
	MultiBinding* AddMulti(
		const DependencyProperty& targetProperty,
		std::vector<MultiBindingSource> sources,
		BindingMode mode = BindingMode::Default,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::Default,
		std::shared_ptr<const IMultiBindingValueConverter> converter = {},
		std::optional<BindingValue> fallbackValue = {},
		std::optional<BindingValue> targetNullValue = {},
		std::optional<BindingValue> converterParameter = {},
		std::optional<std::wstring> stringFormat = {});

	void Clear();
	/** Finds a binding by its exact canonical target-property identity. */
	Binding* Find(const DependencyProperty& targetProperty);
	const Binding* Find(const DependencyProperty& targetProperty) const;
	MultiBinding* FindMulti(const DependencyProperty& targetProperty);
	const MultiBinding* FindMulti(const DependencyProperty& targetProperty) const;
	bool Remove(const DependencyProperty& targetProperty);
	bool UpdateTarget(const DependencyProperty& targetProperty);
	bool UpdateSource(const DependencyProperty& targetProperty);
#if CUI_ENABLE_DYNAMIC_XAML
	/** Designer compatibility path for unresolved target-property names. */
	Binding* Find(const std::wstring& targetProperty);
	const Binding* Find(const std::wstring& targetProperty) const;
	MultiBinding* FindMulti(const std::wstring& targetProperty);
	const MultiBinding* FindMulti(const std::wstring& targetProperty) const;
	/** Removes one binding without disturbing bindings owned by other target properties. */
	bool Remove(const std::wstring& targetProperty);
	/** Refreshes either a Binding or MultiBinding selected by target property. */
	bool UpdateTarget(const std::wstring& targetProperty);
	/** Commits either a Binding or MultiBinding selected by target property. */
	bool UpdateSource(const std::wstring& targetProperty);
#endif
	size_t Count() const;
	std::vector<BindingValidationResult> GetValidationResults() const;
	bool HasValidationIssues() const;
	bool HasValidationErrors() const;
	BindingValidationChangedEvent& ValidationChanged() noexcept
	{
		return _validationChanged;
	}
	BindingError LastError() const noexcept { return _lastError; }
	const wchar_t* LastErrorMessage() const noexcept { return BindingErrorMessage(_lastError); }
	Binding* operator[](size_t index);
	const Binding* operator[](size_t index) const;

private:
	struct CallbackState
	{
		BindingCollection* Owner = nullptr;
	};
	DependencyObject* _owner = nullptr;
	std::shared_ptr<CallbackState> _callbackState;
	std::vector<std::unique_ptr<Binding>> _items;
	std::vector<std::unique_ptr<MultiBinding>> _multiItems;
	std::vector<EventConnection> _validationConnections;
	std::vector<EventConnection> _multiValidationConnections;
	BindingValidationChangedEvent _validationChanged;
	BindingError _lastError = BindingError::None;
	void NotifyValidationChanged(const std::wstring& targetProperty);
};

#define CUI_BINDING_WIDEN2(x) L##x
#define CUI_BINDING_WIDEN(x) CUI_BINDING_WIDEN2(x)

#define CUI_BINDABLE_PROPERTY(type, name) \
	__declspec(property(get = Get##name, put = Set##name)) type name; \
	type Get##name() const { return this->GetValue<type>(CUI_BINDING_WIDEN(#name)); } \
	void Set##name(type value) { this->SetValue<type>(CUI_BINDING_WIDEN(#name), std::move(value)); }

#endif // CUI_BINDING_H_INCLUDED
