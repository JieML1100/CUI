#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "List.h"
#include "Tuple.h"
#include "sqlite/sqlite3.h"



enum class SqliteType {
	INT32,
	INT64,
	FLOAT,
	DOUBLE,
	TEXT,
	BLOB,
	DATETIME
};
typedef std::function<bool(int columns, char** colData, char** colNames, sqlite3_stmt* stmt)> SEL_CALLBAC;
class ColumnValue {
	template<typename T>
	static SqliteType _getType(T v) {
		if constexpr (std::is_same_v<T, int16_t>)
			return SqliteType::INT32;
		else if constexpr (std::is_same_v<T, uint16_t>)
			return SqliteType::INT32;
		else if constexpr (std::is_same_v<T, int32_t>)
			return SqliteType::INT32;
		else if constexpr (std::is_same_v<T, uint32_t>)
			return SqliteType::INT32;
		else if constexpr (std::is_same_v<T, int64_t>)
			return SqliteType::INT64;
		else if constexpr (std::is_same_v<T, uint64_t>)
			return SqliteType::INT64;
		else if constexpr (std::is_same_v<T, float>)
			return SqliteType::FLOAT;
		else if constexpr (std::is_same_v<T, double>)
			return SqliteType::DOUBLE;
		else if constexpr (std::is_same_v<T, char*>)
			return SqliteType::TEXT;
		else if constexpr (std::is_same_v<T, const char*>)
			return SqliteType::TEXT;
		else if constexpr (std::is_same_v<T, std::string>) 
			return SqliteType::TEXT;
		else if constexpr (std::is_same_v<T, const std::string>) 
			return SqliteType::TEXT;
		else if constexpr (std::is_same_v<T, std::vector<char>>) 
			return SqliteType::BLOB;
		else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) 
			return SqliteType::BLOB;
		return SqliteType::BLOB;
	}
public:
	std::string columnName;
	std::string value;
	SqliteType DataType;
	ColumnValue(std::string _columnName, char* _value) : columnName(_columnName), value(_value), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, const char* _value) : columnName(_columnName), value(_value), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, std::string _value) : columnName(_columnName), value(_value), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, int8_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, uint8_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, int16_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, uint16_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, int32_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, uint32_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, int64_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, uint64_t _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, float _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, double _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(_getType(_value)) {}
	ColumnValue(std::string _columnName, std::vector<uint8_t> _value) : columnName(_columnName), value((char*)_value.data(), _value.size()), DataType(SqliteType::BLOB) {}
	ColumnValue(std::string _columnName, std::vector<char> _value) : columnName(_columnName), value((char*)_value.data(), _value.size()), DataType(SqliteType::BLOB) {}

	template<typename T>
	ColumnValue(std::string _columnName, T _value) : columnName(_columnName), value((char*)&_value, sizeof(_value)), DataType(SqliteType::BLOB) {}
};
class SqliteHelper {
public:
	sqlite3* pDB = nullptr;
	char* path = nullptr;
	SqliteHelper(const char* _path);
	void Open();
	bool IsTableExist(std::string tableName);
	void DeleteTable(std::string tableName);
	void CreateTable(std::string tableName, std::vector<Tuple<std::string, SqliteType>> columus);
	void Close();
	void Select(std::string sql, SEL_CALLBAC callback);
	List<List<std::string>> Select(std::string sql);
	bool Insert(const std::string tableName, const std::vector<ColumnValue>& columnValues);
	int Excute(std::string sql);
	int Excute(const char* sql);
};