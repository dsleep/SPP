// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPDatabase.h"
#include "SPPLogging.h"
#include "SPPString.h"
#include <sqllite/sqlite3.h>

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_SQLDB("SQLLITE");

	struct SQLLiteDatabase::DBImpl
	{
		sqlite3* db = nullptr;
		std::string CurrentDBPath;
	};

	SQLLiteDatabase::SQLLiteDatabase() : _impl(new DBImpl())
	{

	}

	SQLLiteDatabase::~SQLLiteDatabase()
	{

	}

	bool SQLLiteDatabase::Connect(const char* FileName)
	{
		if (_impl->db != nullptr)
		{
			SPP_LOG(LOG_SQLDB, LOG_WARNING, "DB ALREADY ACTIVE!!!");
			return false;
		}

		int rc = sqlite3_open(FileName, &_impl->db);

		if (rc)
		{
			SPP_LOG(LOG_SQLDB, LOG_WARNING, "Can't open database: (%s) %s", FileName, sqlite3_errmsg(_impl->db));
			return false;
		}

		_impl->CurrentDBPath = FileName;
		SPP_LOG(LOG_SQLDB, LOG_INFO, "Connected to %s", FileName);

		return true;
	}

	bool SQLLiteDatabase::GenerateTable(const char* TableName, const std::vector<TableField>& InFields)
	{
		std::string TableFields;

		for (const auto& field : InFields)
		{
			if (!TableFields.empty())TableFields += ", ";
			TableFields += std::string_format("\"%s\" %s", field.Name.c_str(), "TEXT");
		}

		auto SQLCommand = std::string_format("CREATE TABLE \"%s\" (%s, PRIMARY KEY(\"%s\") );", TableName, TableFields.c_str(), InFields.front().Name.c_str());
		RunSQL(SQLCommand.c_str());

		return true;
	}

	bool SQLLiteDatabase::Close()
	{
		if (_impl->db == nullptr)
		{
			SPP_LOG(LOG_SQLDB, LOG_WARNING, "DB ALREADY CLOSED!!!");
			return false;
		}

		SPP_LOG(LOG_SQLDB, LOG_INFO, "Closed DB Connection %s", _impl->CurrentDBPath.c_str());
		sqlite3_close(_impl->db);

		_impl->db = nullptr;
		_impl->CurrentDBPath.clear();

		return true;
	}
		
	using CALLBACK_FUNCTION = std::function< int(int, char**, char**) >;
	static int SimpleWrapper(void* data, int argc, char** argv, char** azColName)
	{
		if (data)
		{
			CALLBACK_FUNCTION* thisCallback = static_cast<CALLBACK_FUNCTION*>(data);
			return (*thisCallback)(argc, argv, azColName);
		}
		return 0;
	}

	bool SQLLiteDatabase::RunSQL(const char* SQLCommand, std::function< int(int, char**, char**) > InCallback)
	{
		if (_impl->db == nullptr)
		{
			SPP_LOG(LOG_SQLDB, LOG_WARNING, "DB IS NULL");
		}

		//THIS IS NOT ASYNC ELSE &InCallback would be dumbness
		char* zErrMsg = nullptr;
		int  rc = sqlite3_exec(_impl->db, SQLCommand, SimpleWrapper, InCallback ? &InCallback : nullptr, &zErrMsg);
		if (rc != SQLITE_OK)
		{
			SPP_LOG(LOG_SQLDB, LOG_WARNING, "SQL error: %s", zErrMsg);
			sqlite3_free(zErrMsg);
			return false;
		}

		return true;
	}
}
