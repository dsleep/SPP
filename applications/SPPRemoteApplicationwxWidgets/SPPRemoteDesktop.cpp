// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <memory>
#include <thread>
#include <optional>
#include <sstream>
#include <set>
 

#include "SPPString.h"
#include "SPPEngine.h"
#include "SPPApplication.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include "SPPReflection.h"

#include "SPPOctree.h"

#include "SPPPythonInterface.h"

#include "ThreadPool.h"

#include "SPPCEFUI.h"

#include "SPPJsonUtils.h"

#include "cefclient/JSCallbackInterface.h"
#include <condition_variable>

#include "SPPPlatformCore.h"

#define MAX_LOADSTRING 100

SPP_OVERLOAD_ALLOCATORS

using namespace std::chrono_literals;
using namespace SPP;

struct SubTypeInfo
{
	Json::Value subTypes;
	std::set< rttr::type > typeSet;
};


void GetObjectPropertiesAsJSON(Json::Value& rootValue, SubTypeInfo& subTypes, const rttr::instance& inValue)
{
	rttr::instance obj = inValue.get_type().get_raw_type().is_wrapper() ? inValue.get_wrapped_instance() : inValue;
	auto curType = obj.get_derived_type();

	//auto baseObjType = rttr::type::get<SPPObject>();
	//if (baseObjType.is_base_of(curType))
	//{
	//	//curType = obj.get
	//}

	//SPP_QL("GetPropertiesAsJSON % s", curType.get_name().data());

	auto prop_list = curType.get_properties();
	for (auto prop : prop_list)
	{
		rttr::variant prop_value = prop.get_value(obj);
		if (!prop_value)
			continue; // cannot serialize, because we cannot retrieve the value

		const auto name = prop.get_name().to_string();
		//SPP_QL(" - prop %s", name.data());

		const auto propType = prop_value.get_type();

		//
		if (propType.is_class())
		{
			// does this confirm its inline struct and part of class?!
			//SE_ASSERT(propType.is_wrapper() == false);

			Json::Value nestedInfo;
			GetObjectPropertiesAsJSON(nestedInfo, subTypes, prop_value);

			if (!nestedInfo.isNull())
			{
				if (subTypes.typeSet.count(propType) == 0)
				{
					Json::Value subType;
					subType["type"] = "struct";
					subTypes.subTypes[propType.get_name().data()] = subType;
					subTypes.typeSet.insert(propType);
				}

				Json::Value propInfo;
				propInfo["name"] = name.c_str();
				propInfo["type"] = propType.get_name().data();
				propInfo["value"] = nestedInfo;

				rootValue.append(propInfo);
			}
		}
		else if (propType.is_arithmetic() || propType.is_enumeration())
		{
			if (propType.is_enumeration() && subTypes.typeSet.count(propType) == 0)
			{
				rttr::enumeration enumType = propType.get_enumeration();
				auto EnumValues = enumType.get_names();
				Json::Value subType;
				Json::Value enumValues;

				for (auto& enumV : EnumValues)
				{
					enumValues.append(enumV.data());
				}

				subType["type"] = "enum";
				subType["values"] = enumValues;

				subTypes.subTypes[enumType.get_name().data()] = subType;
				subTypes.typeSet.insert(propType);
			}

			Json::Value propInfo;

			bool bok = false;
			auto stringValue = prop_value.to_string(&bok);

			SE_ASSERT(bok);

			//SPP_QL("   - %s", stringValue.c_str());

			propInfo["name"] = name.c_str();
			propInfo["type"] = propType.get_name().data();
			propInfo["value"] = stringValue;

			rootValue.append(propInfo);
		}
	}
}


void JSFunctionReceiver(const std::string& InFunc, Json::Value& InValue)
{
	/*auto EditorType = rttr::type::get<EditorEngine>();

	rttr::method foundMethod = EditorType.get_method(InFunc);
	if (foundMethod)
	{
		auto paramInfos = foundMethod.get_parameter_infos();

		uint32_t jsonParamCount = InValue.isNull() ? 0 : InValue.size();

		if (paramInfos.size() == jsonParamCount)
		{
			if (!jsonParamCount)
			{
				foundMethod.invoke(*GEd);
			}
			else
			{
				std::list<rttr::variant> argRefs;
				std::vector<rttr::argument> args;

				int32_t Iter = 0;
				for (auto& curParam : paramInfos)
				{
					auto curParamType = curParam.get_type();
					auto jsonParamValue = InValue[Iter];
					if (curParamType.is_arithmetic() && jsonParamValue.isNumeric())
					{

					}
					else if (curParamType == rttr::type::get<std::string>() &&
						jsonParamValue.isString())
					{
						argRefs.push_back(std::string(jsonParamValue.asCString()));
						args.push_back(argRefs.back());
					}
					Iter++;
				}

				if (args.size() == paramInfos.size())
				{
					foundMethod.invoke_variadic(*GEd, args);
				}
			}
		}
	}*/
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	//Alloc Console
	//print some stuff to the console
	//make sure to include #include "stdio.h"
	//note, you must use the #include <iostream>/ using namespace std
	//to use the iostream... #incldue "iostream.h" didn't seem to work
	//in my VC 6
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");

	SPP::IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());

	// setup global asset path
	SPP::GAssetPath = stdfs::absolute(stdfs::current_path() / "..\\Assets\\").generic_string();

	{
		std::function<void(const std::string&, Json::Value&) > jsFuncRecv = JSFunctionReceiver;

		std::thread runCEF([hInstance, &jsFuncRecv]()
		{
			SPP::RunBrowser(hInstance,
				"http://spp/assets/web/remotedesktop/index.html",
				{
					nullptr,
					nullptr,
					nullptr,
					nullptr,
				},
				{
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
				},
				& jsFuncRecv);
		});

		runCEF.join();
	}

	return 0;
}


