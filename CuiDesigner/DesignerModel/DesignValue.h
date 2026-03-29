#pragma once

#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace DesignerModel
{
class DesignValue
{
public:
	using object_t = std::map<std::string, DesignValue>;
	using array_t = std::vector<DesignValue>;

	DesignValue() = default;
	DesignValue(std::nullptr_t) : _value(nullptr) {}
	DesignValue(bool value) : _value(value) {}
	DesignValue(int value) : _value((int64_t)value) {}
	DesignValue(long value) : _value((int64_t)value) {}
	DesignValue(long long value) : _value((int64_t)value) {}
	DesignValue(unsigned int value) : _value((uint64_t)value) {}
	DesignValue(unsigned long value) : _value((uint64_t)value) {}
	DesignValue(unsigned long long value) : _value((uint64_t)value) {}
	DesignValue(float value) : _value((double)value) {}
	DesignValue(double value) : _value(value) {}
	DesignValue(const char* value) : _value(value ? std::string(value) : std::string()) {}
	DesignValue(const std::string& value) : _value(value) {}
	DesignValue(std::string&& value) : _value(std::move(value)) {}
	DesignValue(const object_t& value) : _value(value) {}
	DesignValue(object_t&& value) : _value(std::move(value)) {}
	DesignValue(const array_t& value) : _value(value) {}
	DesignValue(array_t&& value) : _value(std::move(value)) {}
	DesignValue(std::initializer_list<std::pair<const char*, DesignValue>> init)
	{
		object_t object;
		for (const auto& item : init)
		{
			object[item.first] = item.second;
		}
		_value = std::move(object);
	}

	static DesignValue object()
	{
		return DesignValue(object_t{});
	}

	static DesignValue array()
	{
		return DesignValue(array_t{});
	}

	bool is_null() const { return std::holds_alternative<std::nullptr_t>(_value); }
	bool is_boolean() const { return std::holds_alternative<bool>(_value); }
	bool is_string() const { return std::holds_alternative<std::string>(_value); }
	bool is_object() const { return std::holds_alternative<object_t>(_value); }
	bool is_array() const { return std::holds_alternative<array_t>(_value); }
	bool is_number_integer() const { return std::holds_alternative<int64_t>(_value); }
	bool is_number_unsigned() const { return std::holds_alternative<uint64_t>(_value); }
	bool is_number_float() const { return std::holds_alternative<double>(_value); }
	bool is_number() const { return is_number_integer() || is_number_unsigned() || is_number_float(); }

	bool empty() const
	{
		if (is_object()) return std::get<object_t>(_value).empty();
		if (is_array()) return std::get<array_t>(_value).empty();
		if (is_string()) return std::get<std::string>(_value).empty();
		return false;
	}

	std::size_t size() const
	{
		if (is_object()) return std::get<object_t>(_value).size();
		if (is_array()) return std::get<array_t>(_value).size();
		if (is_string()) return std::get<std::string>(_value).size();
		return 0;
	}

	bool contains(const std::string& key) const
	{
		if (!is_object()) return false;
		return std::get<object_t>(_value).find(key) != std::get<object_t>(_value).end();
	}

	DesignValue& operator[](const std::string& key)
	{
		EnsureObject();
		return std::get<object_t>(_value)[key];
	}

	DesignValue& operator[](const char* key)
	{
		return (*this)[std::string(key ? key : "")];
	}

	const DesignValue& operator[](const std::string& key) const
	{
		static const DesignValue nullValue;
		if (!is_object()) return nullValue;
		const object_t& object = std::get<object_t>(_value);
		auto it = object.find(key);
		if (it == object.end())
			return nullValue;
		return it->second;
	}

	const DesignValue& operator[](const char* key) const
	{
		return (*this)[std::string(key ? key : "")];
	}

	DesignValue& operator[](std::size_t index)
	{
		if (!is_array()) throw std::runtime_error("DesignValue is not an array.");
		return std::get<array_t>(_value).at(index);
	}

	const DesignValue& operator[](std::size_t index) const
	{
		if (!is_array()) throw std::runtime_error("DesignValue is not an array.");
		return std::get<array_t>(_value).at(index);
	}

	void push_back(const DesignValue& value)
	{
		EnsureArray();
		std::get<array_t>(_value).push_back(value);
	}

	void push_back(DesignValue&& value)
	{
		EnsureArray();
		std::get<array_t>(_value).push_back(std::move(value));
	}

	array_t::iterator begin()
	{
		EnsureArray();
		return std::get<array_t>(_value).begin();
	}

	array_t::iterator end()
	{
		EnsureArray();
		return std::get<array_t>(_value).end();
	}

	array_t::const_iterator begin() const
	{
		if (!is_array()) return _emptyArray.begin();
		return std::get<array_t>(_value).begin();
	}

	array_t::const_iterator end() const
	{
		if (!is_array()) return _emptyArray.end();
		return std::get<array_t>(_value).end();
	}

	const object_t& ObjectItems() const
	{
		if (!is_object()) return _emptyObject;
		return std::get<object_t>(_value);
	}

	object_t& ObjectItems()
	{
		EnsureObject();
		return std::get<object_t>(_value);
	}

	const array_t& ArrayItems() const
	{
		if (!is_array()) return _emptyArray;
		return std::get<array_t>(_value);
	}

	array_t& ArrayItems()
	{
		EnsureArray();
		return std::get<array_t>(_value);
	}

	template<typename T>
	T get() const
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			if (is_boolean()) return std::get<bool>(_value);
		}
		else if constexpr (std::is_same_v<T, std::string>)
		{
			if (is_string()) return std::get<std::string>(_value);
		}
		else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<T, bool>)
		{
			if (is_number_integer()) return (T)std::get<int64_t>(_value);
			if (is_number_unsigned()) return (T)std::get<uint64_t>(_value);
			if (is_number_float()) return (T)std::get<double>(_value);
		}
		else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
		{
			if (is_number_unsigned()) return (T)std::get<uint64_t>(_value);
			if (is_number_integer()) return (T)std::get<int64_t>(_value);
			if (is_number_float()) return (T)std::get<double>(_value);
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if (is_number_float()) return (T)std::get<double>(_value);
			if (is_number_integer()) return (T)std::get<int64_t>(_value);
			if (is_number_unsigned()) return (T)std::get<uint64_t>(_value);
		}

		throw std::runtime_error("DesignValue type mismatch.");
	}

	template<typename T>
	T value(const std::string& key, const T& defaultValue) const
	{
		if (!is_object()) return defaultValue;
		const auto& object = std::get<object_t>(_value);
		auto it = object.find(key);
		if (it == object.end()) return defaultValue;
		try
		{
			return it->second.get<T>();
		}
		catch (...)
		{
			return defaultValue;
		}
	}

	template<typename T>
	T value(const char* key, const T& defaultValue) const
	{
		return value(std::string(key ? key : ""), defaultValue);
	}

	std::string ToString() const
	{
		if (is_null()) return "null";
		if (is_boolean()) return std::get<bool>(_value) ? "true" : "false";
		if (is_number_integer()) return std::to_string(std::get<int64_t>(_value));
		if (is_number_unsigned()) return std::to_string(std::get<uint64_t>(_value));
		if (is_number_float()) return std::to_string(std::get<double>(_value));
		if (is_string()) return std::get<std::string>(_value);
		return is_object() ? "{object}" : "[array]";
	}

	bool operator==(const DesignValue& other) const = default;

private:
	void EnsureObject()
	{
		if (!is_object()) _value = object_t{};
	}

	void EnsureArray()
	{
		if (!is_array()) _value = array_t{};
	}

	std::variant<std::nullptr_t, bool, int64_t, uint64_t, double, std::string, object_t, array_t> _value = nullptr;
	inline static const object_t _emptyObject{};
	inline static const array_t _emptyArray{};
};
}
