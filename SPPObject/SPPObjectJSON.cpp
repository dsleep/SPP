// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPObjectJSON.h"
#include "SPPLogging.h"

namespace SPP
{
	extern LogEntry LOG_OBJ;

#if 0
	void WalkObjectProperties(const rttr::variant& inValue, 
		const std::function<bool(SPPObject*&)>& InFunction)
	{
		auto originalType = inValue.get_type();
		SE_ASSERT(originalType.is_wrapper());

		//original
		rttr::instance orgobj = inValue;
		//dewrap this dealio
		rttr::instance obj = orgobj.get_type().get_raw_type().is_wrapper() ? orgobj.get_wrapped_instance() : orgobj;
		auto curType = obj.get_derived_type();
		auto ObjectType = rttr::type::get<SPPObject>();

		SPPObject* objRef = nullptr;
		if (curType.is_derived_from(ObjectType))
		{
			objRef = obj.try_convert< SPPObject >();

			std::reference_wrapper<SPPObject*> wrappedValue =
				inValue.get_value< std::reference_wrapper<SPPObject*> >();

			if (objRef)
			{
				if (InFunction(wrappedValue.get()) == false)
				{
					return;
				}
			}
			else
			{
				return;
			}
		}

		auto prop_list = curType.get_properties();
		for (auto prop : prop_list)
		{
			rttr::variant org_prop_value = prop.get_value(obj);
			if (!org_prop_value)
				continue; // cannot serialize, because we cannot retrieve the value

			const auto name = prop.get_name().to_string();
			auto propType = org_prop_value.get_type();

			// it is all wrappers
			if (propType.is_wrapper())
			{
				propType = propType.get_wrapped_type();

				if (IsObjectProperty(propType))
				{
					WalkObjects(org_prop_value, InFunction);
				}
				else if (propType.is_sequential_container())
				{
					auto sub_array_view = org_prop_value.create_sequential_view();
					for (auto& item : sub_array_view)
					{
						WalkObjects(item, InFunction);
					}
				}
				else if (propType.is_associative_container())
				{
					SPP_LOG(LOG_APP, LOG_INFO, " - associative container UNSUPPORTED!!!");
					return;
				}
				else if (propType.is_class())
				{
					SPP_LOG(LOG_APP, LOG_INFO, " - class");
					WalkObjects(org_prop_value, InFunction);
				}
			}
		}
	}
#endif

	void INTERNAL_ObjectToJSON(const rttr::variant& inValue, Json::Value& CurrentContainer, int32_t depth)
	{
		auto originalType = inValue.get_type();
		SE_ASSERT(originalType.is_wrapper());

		rttr::instance orgobj = inValue;
		rttr::instance obj = orgobj.get_type().get_raw_type().is_wrapper() ? orgobj.get_wrapped_instance() : orgobj;
		auto curType = obj.get_derived_type();
		auto ObjectType = rttr::type::get<SPPObject>();

		SPPObject* objRef = nullptr;
		if (curType.is_derived_from(ObjectType))
		{
			objRef = obj.try_convert< SPPObject >();

			std::reference_wrapper<SPPObject*> wrappedValue =
				inValue.get_value< std::reference_wrapper<SPPObject*> >();

			if (objRef)
			{
				CurrentContainer["ObjName"] = objRef->GetPath().ToString();
				CurrentContainer["ObjGUID"] = objRef->GetGUID().ToString();
				if (depth != 0)
				{
					return;
				}
			}
			else
			{
				CurrentContainer["ObjName"] = "NULL";
				return;
			}
		}

		Json::Value properties;
		auto prop_list = curType.get_properties();
		for (auto prop : prop_list)
		{
			rttr::variant org_prop_value = prop.get_value(obj);
			if (!org_prop_value)
				continue; // cannot serialize, because we cannot retrieve the value

			const auto name = prop.get_name().to_string();
			auto propType = org_prop_value.get_type();
			bool bWasWrapped = false;

			if (propType.is_wrapper())
			{
				propType = propType.get_wrapped_type();
				bWasWrapped = true;
			}

			Json::Value newProperty;
			newProperty["PropName"] = name;
			newProperty["PropType"] = propType.get_name().to_string();

			if (propType.is_arithmetic() ||
				propType.is_enumeration() ||
				propType == rttr::type::get<std::string>())
			{
				auto wrappedValue = bWasWrapped ? org_prop_value.extract_wrapped_value() : org_prop_value;
				bool bOk = false;
				newProperty["PropValue"] = wrappedValue.to_string(&bOk);
				SE_ASSERT(bOk);
			}
			else if (propType.is_sequential_container())
			{
				auto sub_array_view = org_prop_value.create_sequential_view();
				auto arraySize = sub_array_view.get_size();
				newProperty["ArraySize"] = (int32_t)arraySize;

				if (arraySize > 0)
				{
					Json::Value arrayProperty;
					for (auto& item : sub_array_view)
					{
						Json::Value arrayElement;
						INTERNAL_ObjectToJSON(item, arrayElement, depth + 1);
						arrayProperty.append(arrayElement);
					}
					newProperty["PropValue"] = arrayProperty;
				}
			}
			else if (propType.is_associative_container())
			{
				SPP_LOG(LOG_OBJ, LOG_INFO, " - associative container UNSUPPORTED!!!");
				continue;
			}
			else if (propType.is_class() || propType.is_pointer())
			{
				INTERNAL_ObjectToJSON(org_prop_value, newProperty, depth+1);
			}

			properties.append(newProperty);
		}

		CurrentContainer["Properties"] = properties;
	}

	void ObjectToJSON(const SPPObject* InObject, Json::Value &CurrentContainer)
	{
		INTERNAL_ObjectToJSON(std::ref(InObject), CurrentContainer,0);
	}
}
