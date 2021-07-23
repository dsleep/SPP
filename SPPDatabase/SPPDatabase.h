// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <functional>
#include <map>

#if _WIN32 && !defined(SPP_DATABASE_STATIC)

	#ifdef SPP_DATABASE_EXPORT
		#define SPP_DATABASE_API __declspec(dllexport)
	#else
		#define SPP_DATABASE_API __declspec(dllimport)
	#endif

#else
	
	#define SPP_DATABASE_API 

#endif

namespace SPP
{
	enum class ESQLFieldType
	{
		Text,
		Integer,
		Float,
		Direct
	};

	struct TableField
	{
		std::string Name;
		ESQLFieldType Type = ESQLFieldType::Text;
	};

	class SPP_DATABASE_API SQLLiteDatabase
	{
	private:
		struct DBImpl;
		std::unique_ptr<DBImpl> _impl;

	public:
		SQLLiteDatabase();
		~SQLLiteDatabase();

		bool Connect(const char* FileName);
		bool GenerateTable(const char* TableName, const std::vector<TableField>& InFields);
		bool Close();
		bool RunSQL(const char* SQLCommand, std::function< int(int, char**, char**) > InCallback = nullptr);
	};
}

