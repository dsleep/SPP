// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPObject.h"
#include "json/json.h"
#include "SPPLogging.h"
#include "SPPJsonUtils.h"

#include <filesystem>

using namespace SPP;

LogEntry LOG_OBJGEN("ObjGen");

struct FieldInfo
{
	std::string name;
	std::string type;
	std::string defaultvalue;

	void Report()
	{
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "name: %s", name.c_str());
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "type: %s", type.c_str());
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "defaultvalue: %s", defaultvalue.c_str());
	}
};

struct FunctionInfo
{
	std::string name;
	std::string return_type;
	std::vector< FieldInfo > fields;

	void Report()
	{
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "name: %s", name.c_str());
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "return_type: %s", return_type.c_str());

		for (auto& iter : fields)
		{
			iter.Report();
		}
	}
};

struct StructInfo
{
	std::string name;
	std::string parent;
	std::string description;
	std::vector< FieldInfo > fields;
	std::vector< FunctionInfo > functions;
	std::vector< StructInfo > structs;

	void Report()
	{
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "name: %s", name.c_str());
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "parent: %s", parent.c_str());
		SPP_LOG(LOG_OBJGEN, LOG_INFO, "description: %s", description.c_str());

		for (auto& iter : fields)
		{
			iter.Report();
		}
		for (auto& iter : functions)
		{
			iter.Report();
		}
		for (auto& iter : structs)
		{
			iter.Report();
		}
	}
};

std::string GetJsonValue(const char* ValueName, Json::Value& InValue, bool bRequired)
{
	Json::Value outValue = InValue.get(ValueName, Json::Value::nullSingleton());
	if (outValue.isNull() == false && outValue.isString())
	{
		return outValue.asCString();
	}
	SE_ASSERT(!bRequired);
	return "";
}

void ParseField(FieldInfo &oField, Json::Value InObject)
{
	oField.name = GetJsonValue("name", InObject, true);
	oField.type = GetJsonValue("type", InObject, true);
	oField.defaultvalue = GetJsonValue("default", InObject, false);
}

void ParseFunction(FunctionInfo& oFunction, Json::Value InObject )
{
	oFunction.name = GetJsonValue("name", InObject, true);
	oFunction.return_type = GetJsonValue("return_type", InObject, false);

	Json::Value FieldsValue = InObject.get("fields", Json::Value::nullSingleton());
	if (!FieldsValue.isNull())
	{
		SE_ASSERT(FieldsValue.isArray());
		for (uint32_t Iter = 0; Iter < FieldsValue.size(); Iter++)
		{
			FieldInfo field;
			ParseField(field, FieldsValue[Iter]);
			oFunction.fields.push_back(field);
		}
	}
}

void ParseObject(StructInfo&oObject, Json::Value InObject )
{
	oObject.name = GetJsonValue("name", InObject, true);
	oObject.parent = GetJsonValue("parent", InObject, false);
	oObject.description = GetJsonValue("description", InObject, false);

	Json::Value FieldsValue = InObject.get("fields", Json::Value::nullSingleton());
	Json::Value FunctionsValue = InObject.get("functions", Json::Value::nullSingleton());

	if (!FieldsValue.isNull())
	{
		SE_ASSERT(FieldsValue.isArray());
		for (uint32_t Iter = 0; Iter < FieldsValue.size(); Iter++)
		{
			FieldInfo field;
			ParseField(field, FieldsValue[Iter]);
			oObject.fields.push_back(field);
		}
	}
	if (!FunctionsValue.isNull())
	{
		SE_ASSERT(FunctionsValue.isArray());
		for (uint32_t Iter = 0; Iter < FunctionsValue.size(); Iter++)
		{
			FunctionInfo function;
			ParseFunction(function, FunctionsValue[Iter]);
			oObject.functions.push_back(function);
		}
	}
}

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	Json::Value JsonConfig;
	SE_ASSERT(FileToJson("../Projects/SimpleTest/Classes.txt", JsonConfig));
	
	Json::Value Objects = JsonConfig.get("classes", Json::Value::nullSingleton());
	SE_ASSERT(Objects.isArray());

	for(uint32_t Iter = 0; Iter < Objects.size(); Iter++)
	{
		StructInfo objectinfo;
		ParseObject(objectinfo, Objects[Iter]);
		objectinfo.Report();
	}

	return 0;
}