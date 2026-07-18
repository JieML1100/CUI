#pragma once

#include "Core/EventConnection.h"
#include <any>
#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

class Control;

enum class BindingMode
{
	OneWay,
	TwoWay,
	OneWayToSource,
	OneTime
};

enum class DataSourceUpdateMode
{
	OnPropertyChanged,
	OnValidation,
	Never
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
	SourceWriteFailed
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

enum class BindingValueKind
{
	Empty,
	Bool,
	Int,
	Int64,
	Float,
	Double,
	String,
	Object
};

class BindingValue
{
public:
	using Storage = std::variant<std::monostate, bool, int, long long, float, double, std::wstring, std::any>;

	BindingValue();
	BindingValue(bool value);
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
	bool TryGetInt(int& out) const;
	bool TryGetInt64(long long& out) const;
	bool TryGetFloat(float& out) const;
	bool TryGetDouble(double& out) const;
	bool TryGetString(std::wstring& out) const;

	template<typename T>
	bool TryGet(T& out) const
	{
		using Value = std::remove_cv_t<T>;
		if constexpr (std::is_same_v<Value, bool>)
			return TryGetBool(out);
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

private:
	Storage _value;
};

bool TryConvertBindingValue(const BindingValue& value, BindingValueKind targetKind, BindingValue& out);
/** Converts while preserving the concrete type represented by targetValue. */
bool TryConvertBindingValue(const BindingValue& value, const BindingValue& targetValue, BindingValue& out);
/** Compares two already-normalized scalar binding values without stringifying them. */
bool BindingValuesEqual(const BindingValue& left, const BindingValue& right);

/**
 * Optional transform used before metadata conversion in either binding direction.
 * Implementations should return false without modifying application state when a
 * value cannot be converted.
 */
class IBindingValueConverter
{
public:
	virtual ~IBindingValueConverter() = default;
	virtual bool Convert(const BindingValue& value, BindingValue& out) const = 0;
	virtual bool ConvertBack(const BindingValue& value, BindingValue& out) const = 0;
};

/** Function-backed converter for lightweight formatting and unit transforms. */
class DelegateBindingValueConverter final : public IBindingValueConverter
{
public:
	using Function = std::function<bool(const BindingValue&, BindingValue&)>;

	DelegateBindingValueConverter(Function convert, Function convertBack = {});
	bool Convert(const BindingValue& value, BindingValue& out) const override;
	bool ConvertBack(const BindingValue& value, BindingValue& out) const override;

private:
	Function _convert;
	Function _convertBack;
};

/** Discoverable converter metadata used by runtime registration and design tools. */
struct BindingValueConverterMetadata
{
	std::wstring Name;
	/** Empty means that the converter accepts any source kind. */
	BindingValueKind SourceKind = BindingValueKind::Empty;
	/** Empty means that the converter can target any property kind. */
	BindingValueKind TargetKind = BindingValueKind::Empty;
	bool CanConvertBack = true;

	bool operator==(const BindingValueConverterMetadata&) const = default;
};

/**
 * Process-wide named converter registry. Built-in converters are always present;
 * applications may register custom factories before generated forms call BindData.
 */
class BindingValueConverterRegistry final
{
public:
	using Factory = std::function<std::shared_ptr<const IBindingValueConverter>()>;

	static bool Register(
		BindingValueConverterMetadata metadata,
		Factory factory,
		bool replaceExisting = false);
	/** Removes a custom registration. Built-in registrations cannot be removed. */
	static bool Unregister(const std::wstring& name);
	static std::optional<BindingValueConverterMetadata> Find(const std::wstring& name);
	static std::vector<BindingValueConverterMetadata> GetConverters();
	static std::shared_ptr<const IBindingValueConverter> Create(const std::wstring& name);
};

class PropertyChangedEventArgs
{
public:
	std::wstring PropertyName;

	PropertyChangedEventArgs() = default;
	explicit PropertyChangedEventArgs(std::wstring propertyName);
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
	void Notify(const std::wstring& propertyName);
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
	std::wstring PropertyName;

	BindingValidationChangedEventArgs() = default;
	explicit BindingValidationChangedEventArgs(std::wstring propertyName);
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
	void Notify(const std::wstring& propertyName);
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
	std::wstring Name;
	BindingValueKind ValueKind = BindingValueKind::Empty;
	std::type_index ValueType{ typeid(void) };
	bool CanRead = true;
	bool CanWrite = true;
	bool CanObserve = true;

	bool operator==(const BindingSourcePropertyMetadata&) const = default;
};

class IBindingSource : public INotifyPropertyChanged
{
public:
	IBindingSource()
		: _bindingLifetime(std::make_shared<int>(0)) {}
	IBindingSource(const IBindingSource&)
		: _bindingLifetime(std::make_shared<int>(0)) {}
	IBindingSource(IBindingSource&&)
		: _bindingLifetime(std::make_shared<int>(0)) {}
	IBindingSource& operator=(const IBindingSource&) noexcept { return *this; }
	IBindingSource& operator=(IBindingSource&&) noexcept { return *this; }
	virtual bool TryGetValue(const std::wstring& propertyName, BindingValue& out) const = 0;
	virtual bool TrySetValue(const std::wstring& propertyName, const BindingValue& value) = 0;
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
	/** Returns null when the source exposes only snapshot validation state. */
	virtual BindingValidationChangedEvent* ValidationChanged() noexcept
	{
		return nullptr;
	}
	std::weak_ptr<const void> BindingLifetime() const noexcept { return _bindingLifetime; }

private:
	std::shared_ptr<const void> _bindingLifetime;
};

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

private:
	std::shared_ptr<IBindingSource> _source;
};

/** Collects object and field issues along a dotted source path. */
std::vector<BindingValidationIssue> GetBindingValidationIssuesForPath(
	const IBindingSource& source,
	const std::wstring& sourcePropertyPath);

class ObservableObject : public IBindingSource
{
public:
	PropertyChangedEvent& PropertyChanged() override { return _propertyChanged; }
	BindingValidationChangedEvent* ValidationChanged() noexcept override
	{
		return &_validationChanged;
	}

	bool TryGetValue(const std::wstring& propertyName, BindingValue& out) const override;
	bool TrySetValue(const std::wstring& propertyName, const BindingValue& value) override;
	bool TryGetPropertyMetadata(
		const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const override;
	std::vector<BindingSourcePropertyMetadata> GetProperties() const override;
	std::vector<BindingValidationIssue> GetValidationIssues(
		const std::wstring& propertyName) const override;
	bool HasValidationIssues() const noexcept;
	bool HasValidationErrors() const noexcept;
	bool HasValidationErrors(const std::wstring& propertyName) const;

	/** Defines metadata and an optional initial value without requiring CanWrite. */
	bool DefineProperty(
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
		return DefineProperty(
			{ std::move(name), value.Kind(), std::type_index(value.Type()),
				canRead, canWrite, canObserve },
			value,
			replaceExisting);
	}
	bool RemoveProperty(const std::wstring& propertyName);

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
	bool SetValue(const std::wstring& propertyName, T value)
	{
		return TrySetValue(propertyName, BindingValue(std::move(value)));
	}

protected:
	void OnPropertyChanged(const std::wstring& propertyName);
	/** Replaces the issues for one property; empty propertyName is object-level. */
	bool SetValidationIssues(
		const std::wstring& propertyName,
		std::vector<BindingValidationIssue> issues);
	bool SetValidationError(
		const std::wstring& propertyName,
		std::wstring message,
		std::wstring code = {});
	bool ClearValidationIssues(const std::wstring& propertyName);
	bool ClearAllValidationIssues();
	/** Updates declared read-only properties from a derived view-model. */
	bool SetCurrentValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		bool notify = true);

	template<typename T>
		requires (!std::is_same_v<std::remove_cvref_t<T>, BindingValue>)
	bool SetCurrentValue(const std::wstring& propertyName, T value, bool notify = true)
	{
		return SetCurrentValue(propertyName, BindingValue(std::move(value)), notify);
	}

private:
	bool NormalizeValue(
		BindingSourcePropertyMetadata& metadata,
		const BindingValue& value,
		BindingValue& out) const;
	PropertyChangedEvent _propertyChanged;
	BindingValidationChangedEvent _validationChanged;
	std::unordered_map<std::wstring, BindingValue> _values;
	std::unordered_map<std::wstring, BindingSourcePropertyMetadata> _metadata;
	std::unordered_map<std::wstring, std::vector<BindingValidationIssue>>
		_validationIssues;
};

/** Ordered sources contributing to a Control property's effective value. */
enum class ControlPropertyValueSource : unsigned char
{
	Default = 0,
	Theme = 1,
	Style = 2,
	Binding = 3,
	Local = 4
};

const wchar_t* ControlPropertyValueSourceName(
	ControlPropertyValueSource source) noexcept;

/** Runtime work automatically requested after an effective control property changes. */
enum class ControlPropertyFlags : unsigned char
{
	None = 0,
	AffectsMeasure = 1u << 0,
	AffectsArrange = 1u << 1,
	AffectsRender = 1u << 2,
	/** Direct property-wrapper assignment creates a Local value immediately. */
	TracksLocalValue = 1u << 3
};

constexpr ControlPropertyFlags operator|(
	ControlPropertyFlags left,
	ControlPropertyFlags right) noexcept
{
	return static_cast<ControlPropertyFlags>(
		static_cast<unsigned char>(left) | static_cast<unsigned char>(right));
}

constexpr ControlPropertyFlags& operator|=(
	ControlPropertyFlags& left,
	ControlPropertyFlags right) noexcept
{
	left = left | right;
	return left;
}

constexpr bool HasControlPropertyFlag(
	ControlPropertyFlags value,
	ControlPropertyFlags flag) noexcept
{
	return (static_cast<unsigned char>(value)
		& static_cast<unsigned char>(flag)) != 0;
}

/** Preferred editor used by metadata-driven design tools. */
enum class ControlPropertyEditorKind : unsigned char
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
enum class ControlPropertyPersistence : unsigned char
{
	/** Let the design tool choose its native metadata representation. */
	Automatic,
	/** A compatibility field already owns persistence for this property. */
	Legacy,
	/** Persist through the generic typed metadata-property bag. */
	Metadata,
	/** Runtime-only state that should not be persisted by a design tool. */
	Transient
};

/** One strongly typed item for a Choice editor. */
struct ControlPropertyChoice
{
	std::wstring DisplayName;
	BindingValue Value;
};

/**
 * Optional presentation contract consumed by metadata-driven design tools.
 * Empty/default values preserve the historical discoverable-property behavior.
 */
struct ControlPropertyDesignMetadata
{
	bool Browsable = true;
	std::wstring DisplayName;
	std::wstring Category = L"Misc";
	int CategoryOrder = 1000;
	int Order = 0;
	ControlPropertyEditorKind Editor = ControlPropertyEditorKind::Auto;
	std::vector<ControlPropertyChoice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	ControlPropertyPersistence Persistence = ControlPropertyPersistence::Automatic;
	/** Optional target-sensitive visibility, evaluated after Browsable. */
	std::function<bool(Control&)> BrowsableWhen;
};

/**
 * Behavioral metadata layered on top of a bindable property registration.
 * Coerce returns nullopt to reject a value, or the effective value to apply.
 */
template<typename TOwner, typename TValue>
struct ControlPropertyOptions
{
	std::optional<TValue> DefaultValue;
	ControlPropertyFlags Flags = ControlPropertyFlags::None;
	std::function<std::optional<TValue>(TOwner&, const TValue&)> Coerce;
	std::function<void(TOwner&, const TValue&, const TValue&)> Changed;
	std::function<bool(const TValue&, const TValue&)> Equals;
	ControlPropertyDesignMetadata Design;
};

/**
 * Describes one Control property. Reading, writing, subscribing, defaults,
 * coercion and invalidation are supplied by the owner, so Binding, Designer
 * and styles can share one property contract.
 */
class BindingPropertyMetadata final
{
public:
	using ChangeHandler = std::function<void()>;

	const std::wstring& Name() const noexcept { return _name; }
	BindingValueKind ValueKind() const noexcept { return _valueKind; }
	const std::type_index& ValueType() const noexcept { return _valueType; }
	const std::type_index& OwnerType() const noexcept { return _ownerType; }
	bool CanRead() const noexcept { return static_cast<bool>(_getter); }
	bool CanWrite() const noexcept { return static_cast<bool>(_setter); }
	bool CanObserve() const noexcept { return static_cast<bool>(_subscriber); }
	bool HasDefaultValue() const noexcept { return _hasDefaultValue; }
	ControlPropertyFlags Flags() const noexcept { return _flags; }
	const ControlPropertyDesignMetadata& Design() const noexcept { return _design; }
	bool IsDesignerBrowsable(Control& target) const;

	bool Matches(const Control& target) const;
	bool TryConvert(const BindingValue& value, BindingValue& out) const;
	bool TryCoerce(Control& target, const BindingValue& value, BindingValue& out) const;
	bool ValuesEqual(const BindingValue& left, const BindingValue& right) const;
	bool TryGetDefaultValue(BindingValue& out) const;
	bool TryGet(Control& target, BindingValue& out) const;
	bool TrySet(Control& target, const BindingValue& value) const;
	EventConnection Subscribe(Control& target, ChangeHandler handler, DataSourceUpdateMode updateMode) const;

private:
	using Matcher = std::function<bool(const Control&)>;
	using ValueConverter = std::function<bool(const BindingValue&, BindingValue&)>;
	using Coercer = std::function<bool(Control&, const BindingValue&, BindingValue&)>;
	using Comparer = std::function<bool(const BindingValue&, const BindingValue&)>;
	using Getter = std::function<bool(Control&, BindingValue&)>;
	using Setter = std::function<bool(Control&, const BindingValue&)>;
	using Subscriber = std::function<EventConnection(Control&, ChangeHandler, DataSourceUpdateMode)>;
	using Changed = std::function<void(Control&, const BindingValue&, const BindingValue&)>;

	std::wstring _name;
	BindingValueKind _valueKind = BindingValueKind::Empty;
	std::type_index _valueType{ typeid(void) };
	std::type_index _ownerType{ typeid(void) };
	Matcher _matcher;
	ValueConverter _valueConverter;
	Coercer _coercer;
	Comparer _comparer;
	Getter _getter;
	Setter _setter;
	Subscriber _subscriber;
	Changed _changed;
	BindingValue _defaultValue;
	bool _hasDefaultValue = false;
	ControlPropertyFlags _flags = ControlPropertyFlags::None;
	ControlPropertyDesignMetadata _design;

	BindingPropertyMetadata(std::wstring name,
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
		ControlPropertyDesignMetadata design);

	void NotifyChanged(
		Control& target,
		const BindingValue& oldValue,
		const BindingValue& newValue) const;

	friend class BindingPropertyRegistry;
	friend class Control;
};

class BindingPropertyRegistry final
{
public:
	template<typename TOwner, typename TValue>
	static const BindingPropertyMetadata* Register(
		std::wstring name,
		std::function<TValue(TOwner&)> getter,
		std::function<void(TOwner&, const TValue&)> setter,
		std::function<EventConnection(TOwner&, BindingPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber = {},
		ControlPropertyOptions<TOwner, TValue> options = {});

	static const BindingPropertyMetadata* Find(Control& target, const std::wstring& propertyName);
	/** Returns the effective metadata set for target, with derived overrides applied. */
	static std::vector<const BindingPropertyMetadata*> GetProperties(Control& target);

private:
	static const BindingPropertyMetadata* Register(BindingPropertyMetadata metadata);
};

// Preferred general names. The Binding-prefixed names remain source-compatible.
using ControlPropertyMetadata = BindingPropertyMetadata;
using ControlPropertyRegistry = BindingPropertyRegistry;

class Binding
{
public:
	Binding(Control* target,
		std::wstring targetProperty,
		IBindingSource* source,
		std::wstring sourceProperty,
		BindingMode mode = BindingMode::OneWay,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::OnPropertyChanged,
		std::shared_ptr<const IBindingValueConverter> converter = {});
	~Binding();

	Binding(const Binding&) = delete;
	Binding& operator=(const Binding&) = delete;

	const std::wstring& TargetProperty() const { return _targetProperty; }
	const std::wstring& SourceProperty() const { return _sourceProperty; }
	BindingMode Mode() const { return _mode; }
	DataSourceUpdateMode UpdateMode() const { return _updateMode; }
	const std::shared_ptr<const IBindingValueConverter>& Converter() const noexcept { return _converter; }
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

	Control* _target = nullptr;
	IBindingSource* _source = nullptr;
	std::wstring _targetProperty;
	std::wstring _sourceProperty;
	std::vector<std::wstring> _sourcePath;
	BindingMode _mode = BindingMode::OneWay;
	DataSourceUpdateMode _updateMode = DataSourceUpdateMode::OnPropertyChanged;
	std::shared_ptr<const IBindingValueConverter> _converter;
	std::shared_ptr<State> _state;
	std::weak_ptr<const void> _sourceLifetime;
	std::vector<EventConnection> _sourceConnections;
	std::vector<std::shared_ptr<IBindingSource>> _sourcePathOwners;
	std::vector<EventConnection> _sourceValidationConnections;
	std::vector<EventConnection> _validationPathConnections;
	std::vector<std::shared_ptr<IBindingSource>> _validationPathOwners;
	EventConnection _targetConnection;
	BindingValidationChangedEvent _validationChanged;
	std::vector<BindingValidationIssue> _validationIssues;
	bool _updatingTarget = false;
	bool _updatingSource = false;
	bool _ownsTargetValue = false;
	bool _isValid = false;
	BindingError _lastError = BindingError::None;
	const BindingPropertyMetadata* _targetMetadata = nullptr;

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
	bool ResolveSourceOwner(
		IBindingSource*& owner,
		std::vector<std::shared_ptr<IBindingSource>>& keepAlive) const;
	bool Fail(BindingError error) noexcept
	{
		_lastError = error;
		return false;
	}
	bool IsSourceAlive() const noexcept { return _source && !_sourceLifetime.expired(); }
};

class BindingCollection
{
public:
	explicit BindingCollection(Control* owner);

	Binding* Add(const std::wstring& targetProperty,
		IBindingSource* source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::OneWay,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::OnPropertyChanged,
		std::shared_ptr<const IBindingValueConverter> converter = {});

	Binding* Add(const std::wstring& targetProperty,
		IBindingSource& source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::OneWay,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::OnPropertyChanged,
		std::shared_ptr<const IBindingValueConverter> converter = {})
	{
		return Add(targetProperty, &source, sourceProperty, mode, updateMode, std::move(converter));
	}

	void Clear();
	/** Finds a binding by target property using the same case-insensitive identity as Add. */
	Binding* Find(const std::wstring& targetProperty);
	const Binding* Find(const std::wstring& targetProperty) const;
	/** Removes one binding without disturbing bindings owned by other target properties. */
	bool Remove(const std::wstring& targetProperty);
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
	Control* _owner = nullptr;
	std::vector<std::unique_ptr<Binding>> _items;
	std::vector<EventConnection> _validationConnections;
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
