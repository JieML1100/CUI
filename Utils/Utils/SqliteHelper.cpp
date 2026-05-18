#pragma once
#include "SqliteHelper.h"
#include "Utils.h"

const std::string SqliteTypeName[] = {
	"INTEGER",
	"LONG",
	"REAL",
	"TEXT",
	"BLOB",
	"DATETIME"
};
SqliteHelper::SqliteHelper(const char* _path) {
	this->path = (char*)_path;
}
void SqliteHelper::Open() {
	int nRes = sqlite3_open(this->path, &this->pDB);
	if (nRes != SQLITE_OK) {
		std::cerr << "SQL error: " << sqlite3_errmsg(this->pDB) << std::endl;
	}
}
bool SqliteHelper::IsTableExist(std::string tableName) {
	sqlite3_stmt* stmt;
	const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
	int rc = sqlite3_prepare_v2(this->pDB, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK)
		return false;
	sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return rc == SQLITE_ROW;
}
void SqliteHelper::DeleteTable(std::string tableName) {
	this->Excute(StringHelper::Format("DROP TABLE [%s];", tableName.c_str()));
}
void SqliteHelper::CreateTable(std::string tableName, std::vector<Tuple<std::string, SqliteType>> columus) {
	if (IsTableExist(tableName)) {
		this->DeleteTable(tableName);
	}
	std::stringstream sql;
	sql << "CREATE TABLE [" << tableName << "] ([Id] INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\n";
	for (int i = 0; i < columus.size(); i++) {
		auto c = columus[i];
		sql << "[" << c.Item1 << "] " << SqliteTypeName[(int)c.Item2];
		if (i < columus.size() - 1) {
			sql << ",\n";
		}
	}
	sql << ");";
	std::string str = sql.str();
	this->Excute(str);
}
void SqliteHelper::Close() {
	sqlite3_close(this->pDB);
}
void SqliteHelper::Select(std::string sql, SEL_CALLBAC callback) {
	char* cErrMsg = nullptr;
	sqlite3_exec(this->pDB, sql.c_str(), [](void* pram, int argc, char** argv, char** azColName, sqlite3_stmt* stmt)->int {
			SEL_CALLBAC* callback = (SEL_CALLBAC*)pram;
			return  callback->operator()(argc, argv, azColName, stmt);
		}, &callback, &cErrMsg);
}
List<List<std::string>> SqliteHelper::Select(std::string sql) {
	char* cErrMsg = nullptr;
	List<List<std::string>> result = List<List<std::string>>();
	sqlite3_exec(this->pDB, sql.c_str(), [](void* pram, int argc, char** argv, char** azColName, sqlite3_stmt* stmt)->int {
			List<List<std::string>>* list = (List<List<std::string>>*)pram;

			List<std::string > row = List<std::string >();
			for (int i = 0; i < argc; i++) {
				if (argv[i] && *argv[i])
					row.Add(std::string(argv[i]));
				else
					row.Add(std::string(""));
			}
			list->Add(row);
			return 0;
		}, &result, &cErrMsg);
	return result;
}
bool SqliteHelper::Insert(const std::string tableName, const std::vector<ColumnValue>& columnValues) {
	std::string sql = "INSERT INTO " + tableName + " (";
	std::string values = "(";

	for (size_t i = 0; i < columnValues.size(); ++i) {
		sql += columnValues[i].columnName;
		values += "?";
		if (i < columnValues.size() - 1) {
			sql += ", ";
			values += ", ";
		}
	}
	sql += ") VALUES " + values + ")";
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(pDB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "SQL prepare error: " << sqlite3_errmsg(pDB) << std::endl;
		return false;
	}
	for (size_t i = 0; i < columnValues.size(); ++i) {
		if (columnValues[i].value.size() == 0 || columnValues[i].value == "NULL" || columnValues[i].value == "null") {
			sqlite3_bind_null(stmt, i + 1);
		}
		else  {
			switch (columnValues[i].DataType) {
			case SqliteType::TEXT:
				sqlite3_bind_text(stmt, i + 1, columnValues[i].value.c_str(), -1, SQLITE_TRANSIENT);
				break;
			case SqliteType::INT32:
				sqlite3_bind_int(stmt, i + 1, *(int*)columnValues[i].value.data());
				break;
			case SqliteType::INT64:
				sqlite3_bind_int64(stmt, i + 1, *(int64_t*)columnValues[i].value.data());
				break;
			case SqliteType::FLOAT:
				sqlite3_bind_double(stmt, i + 1, *(float*)columnValues[i].value.data());
				break;
			case SqliteType::DOUBLE:
				sqlite3_bind_double(stmt, i + 1, *(double*)columnValues[i].value.data());
				break;
			case SqliteType::BLOB:
				sqlite3_bind_blob(stmt, i + 1, columnValues[i].value.c_str(), columnValues[i].value.size(), SQLITE_TRANSIENT);
				break;
			case SqliteType::DATETIME:
				sqlite3_bind_text(stmt, i + 1, columnValues[i].value.c_str(), -1, SQLITE_TRANSIENT);
				break;
			default:
				std::cerr << "Unsupported data type for column: " << columnValues[i].columnName << std::endl;
				sqlite3_finalize(stmt);
				return false;
			}
		}
	}
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		std::cerr << "SQL step error: " << sqlite3_errmsg(pDB) << std::endl;
		sqlite3_finalize(stmt);
		return false;
	}
	sqlite3_finalize(stmt);
	return true;
}
int SqliteHelper::Excute(std::string sql) {
	char* cErrMsg = nullptr;
	return sqlite3_exec(pDB, sql.c_str(), NULL, NULL, &cErrMsg);
}
int SqliteHelper::Excute(const char* sql) {
	char* cErrMsg = nullptr;
	return sqlite3_exec(pDB, sql, NULL, NULL, &cErrMsg);
}