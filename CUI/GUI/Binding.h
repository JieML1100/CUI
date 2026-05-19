#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
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

enum class BindingValueKind
{
	Empty,
	Bool,
	Int,
	Int64,
	Float,
	Double,
	String
};

class BindingValue
{
public:
	using Storage = std::variant<std::monostate, bool, int, long long, float, double, std::wstring>;

	BindingValue();
	BindingValue(bool value);
	BindingValue(int value);
	BindingValue(long long value);
	BindingValue(float value);
	BindingValue(double value);
	BindingValue(const wchar_t* value);
	BindingValue(const std::wstring& value);
	BindingValue(std::wstring&& value);

	BindingValueKind Kind() const;
	bool Empty() const;
	std::wstring ToString() const;

	bool TryGetBool(bool& out) const;
	bool TryGetInt(int& out) const;
	bool TryGetInt64(long long& out) const;
	bool TryGetFloat(float& out) const;
	bool TryGetDouble(double& out) const;
	bool TryGetString(std::wstring& out) const;

	const Storage& Raw() const { return _value; }

private:
	Storage _value;
};

bool TryConvertBindingValue(const BindingValue& value, BindingValueKind targetKind, BindingValue& out);

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

	size_t Add(Handler handler);
	void Remove(size_t token);
	void Notify(const std::wstring& propertyName);
	void Clear();

private:
	size_t _nextToken = 1;
	std::vector<std::pair<size_t, Handler>> _handlers;
};

class INotifyPropertyChanged
{
public:
	virtual ~INotifyPropertyChanged() = default;
	virtual PropertyChangedEvent& PropertyChanged() = 0;
};

class IBindingSource : public INotifyPropertyChanged
{
public:
	virtual bool TryGetValue(const std::wstring& propertyName, BindingValue& out) const = 0;
	virtual bool TrySetValue(const std::wstring& propertyName, const BindingValue& value) = 0;
};

class ObservableObject : public IBindingSource
{
public:
	PropertyChangedEvent& PropertyChanged() override { return _propertyChanged; }

	bool TryGetValue(const std::wstring& propertyName, BindingValue& out) const override;
	bool TrySetValue(const std::wstring& propertyName, const BindingValue& value) override;

	template<typename T>
	T GetValue(const std::wstring& propertyName, const T& defaultValue = T{}) const
	{
		BindingValue value;
		if (!TryGetValue(propertyName, value))
			return defaultValue;

		if constexpr (std::is_same_v<T, std::wstring>)
		{
			std::wstring result;
			return value.TryGetString(result) ? result : defaultValue;
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			bool result = false;
			return value.TryGetBool(result) ? result : defaultValue;
		}
		else if constexpr (std::is_same_v<T, int>)
		{
			int result = 0;
			return value.TryGetInt(result) ? result : defaultValue;
		}
		else if constexpr (std::is_same_v<T, long long>)
		{
			long long result = 0;
			return value.TryGetInt64(result) ? result : defaultValue;
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			float result = 0.0f;
			return value.TryGetFloat(result) ? result : defaultValue;
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			double result = 0.0;
			return value.TryGetDouble(result) ? result : defaultValue;
		}
		else
		{
			static_assert(!sizeof(T*), "ObservableObject::GetValue only supports common binding value types.");
		}
	}

	template<typename T>
	bool SetValue(const std::wstring& propertyName, T value)
	{
		return TrySetValue(propertyName, BindingValue(std::move(value)));
	}

protected:
	void OnPropertyChanged(const std::wstring& propertyName);

private:
	PropertyChangedEvent _propertyChanged;
	std::unordered_map<std::wstring, BindingValue> _values;
};

class Binding
{
public:
	Binding(Control* target,
		std::wstring targetProperty,
		IBindingSource* source,
		std::wstring sourceProperty,
		BindingMode mode = BindingMode::OneWay,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::OnPropertyChanged);
	~Binding();

	Binding(const Binding&) = delete;
	Binding& operator=(const Binding&) = delete;

	const std::wstring& TargetProperty() const { return _targetProperty; }
	const std::wstring& SourceProperty() const { return _sourceProperty; }
	BindingMode Mode() const { return _mode; }
	DataSourceUpdateMode UpdateMode() const { return _updateMode; }

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
	BindingMode _mode = BindingMode::OneWay;
	DataSourceUpdateMode _updateMode = DataSourceUpdateMode::OnPropertyChanged;
	std::shared_ptr<State> _state;
	bool _updatingTarget = false;
	bool _updatingSource = false;

	void Attach();
	void AttachSourceChangedHandler();
	void AttachTargetChangedHandlers();
	void OnSourcePropertyChanged(const PropertyChangedEventArgs& e);
	void OnTargetPropertyChanged();
};

class BindingCollection
{
public:
	explicit BindingCollection(Control* owner);

	Binding* Add(const std::wstring& targetProperty,
		IBindingSource* source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::OneWay,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::OnPropertyChanged);

	Binding* Add(const std::wstring& targetProperty,
		IBindingSource& source,
		const std::wstring& sourceProperty,
		BindingMode mode = BindingMode::OneWay,
		DataSourceUpdateMode updateMode = DataSourceUpdateMode::OnPropertyChanged)
	{
		return Add(targetProperty, &source, sourceProperty, mode, updateMode);
	}

	void Clear();
	size_t Count() const;
	Binding* operator[](size_t index);
	const Binding* operator[](size_t index) const;

private:
	Control* _owner = nullptr;
	std::vector<std::unique_ptr<Binding>> _items;
};

#define CUI_BINDING_WIDEN2(x) L##x
#define CUI_BINDING_WIDEN(x) CUI_BINDING_WIDEN2(x)

#define CUI_BINDABLE_PROPERTY(type, name) \
	__declspec(property(get = Get##name, put = Set##name)) type name; \
	type Get##name() const { return this->GetValue<type>(CUI_BINDING_WIDEN(#name)); } \
	void Set##name(type value) { this->SetValue<type>(CUI_BINDING_WIDEN(#name), std::move(value)); }
