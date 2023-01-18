// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPReflection.h"
#include "SPPLogging.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_REFL("REFLECTION");

	uint32_t GetReflectionVersion()
	{
		return 1;
	}

	void PrintJSONValue(const Json::Value& val)
	{
		if (val.isString()) {
			SPP_LOG(LOG_REFL, LOG_INFO, "string(%s)", val.asString().c_str());
		}
		else if (val.isBool()) {
			SPP_LOG(LOG_REFL, LOG_INFO, "bool(%d)", val.asBool());
		}
		else if (val.isInt()) {
			SPP_LOG(LOG_REFL, LOG_INFO, "int(%d)", val.asInt());
		}
		else if (val.isUInt()) {
			SPP_LOG(LOG_REFL, LOG_INFO, "uint(%u)", val.asUInt());
		}
		else if (val.isDouble()) {
			SPP_LOG(LOG_REFL, LOG_INFO, "double(%f)", val.asDouble());
		}
		else
		{
			SPP_LOG(LOG_REFL, LOG_INFO, "unknown type = [% d]", val.type());
		}
	}

	bool PrintJSONTree(const Json::Value& root, unsigned short depth /* = 0 */)
	{
		depth += 1;
		SPP_LOG(LOG_REFL, LOG_INFO," {type = [% d], size = % d}", root.type(), root.size());

		if (root.size() > 0)
		{
			printf("\n");
			for (Json::Value::const_iterator itr = root.begin(); itr != root.end(); itr++) {
				// Print depth. 
				for (int tab = 0; tab < depth; tab++) {
					SPP_LOG(LOG_REFL, LOG_INFO, "-");
				}
				SPP_LOG(LOG_REFL, LOG_INFO, " subvalue(");
				PrintJSONValue(itr.key());
				SPP_LOG(LOG_REFL, LOG_INFO, ") -");
				PrintJSONTree(*itr, depth);
			}
			return true;
		}
		else {
			printf(" ");
			PrintJSONValue(root);
			printf("\n");
		}
		return true;
	}

	void PODToJSON(const rttr::variant& inValue, Json::Value& InJsonValue)
	{
		auto originalType = inValue.get_type();
		SE_ASSERT(originalType.is_wrapper());

		//rttr::instance obj = originalType.is_wrapper() ? inValue.extract_wrapped_value() : inValue;
		auto curType = originalType.is_wrapper() ? originalType.get_wrapped_type() : originalType;
		auto prop_list = curType.get_properties();
		for (auto prop : prop_list)
		{
			rttr::variant org_prop_value = prop.get_value(inValue);

			auto org_prop_type = prop.get_type();

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

			if (propType.is_arithmetic() ||
				propType.is_enumeration() ||
				propType == rttr::type::get<std::string>())
			{
				auto wrappedValue = bWasWrapped ? org_prop_value.extract_wrapped_value() : org_prop_value;
				bool bOk = false;
				InJsonValue[name] = wrappedValue.to_string(&bOk);
				SE_ASSERT(bOk);
			}
			else if (propType.is_sequential_container())
			{
				SPP_LOG(LOG_REFL, LOG_INFO, " - sequential container UNSUPPORTED!!!");
				continue;
			}
			else if (propType.is_associative_container())
			{
				SPP_LOG(LOG_REFL, LOG_INFO, " - associative container UNSUPPORTED!!!");
				continue;
			}
			else if (propType.is_class() || propType.is_pointer())
			{				
				Json::Value newProperties;
				PODToJSON(org_prop_value, newProperties);
				InJsonValue[name] = newProperties;
				continue;
			}			
		}		
	}

	template<typename NumericType>
	bool impl_NumericConvert(const rttr::instance& obj, rttr::property& InProperty, const std::string& InValue)
	{
		if (InProperty.get_type() == rttr::type::get<NumericType>())
		{
			std::stringstream ssConvert(InValue);
			NumericType realValue = { 0 };
			ssConvert >> realValue;

			// we there set it
			if (InProperty.set_value(obj, realValue) == false)
			{
				SPP_LOG(LOG_REFL, LOG_INFO, "SetPropertyValue number failed");
			}

			return true;
		}
		return false;
	}

	template<typename ... Types>
	bool NumericConvert(const rttr::instance& obj, rttr::property& InProperty, const std::string& InValue)
	{
		bool any_of = (impl_NumericConvert< Types>(obj, InProperty, InValue) || ...);
		return any_of;
	}


	template<typename NumericType>
	bool impl_NumericConvert(rttr::variant& InVar, const std::string& InValue)
	{
		auto VarType = InVar.get_type();
		SE_ASSERT(VarType.is_wrapper());
		VarType = VarType.get_wrapped_type();
		if (VarType == rttr::type::get<NumericType>())
		{
			std::stringstream ssConvert(InValue);
			NumericType realValue = { 0 };
			ssConvert >> realValue;

			std::reference_wrapper<NumericType> wrappedValue =
				InVar.get_value< std::reference_wrapper<NumericType> >();
			wrappedValue.get() = realValue;

			return true;
		}
		return false;
	}

	template<typename ... Types>
	bool NumericConvert(rttr::variant& InVar, const std::string& InValue)
	{
		bool any_of = (impl_NumericConvert< Types>(InVar, InValue) || ...);
		return any_of;
	}

	bool SetPropertyValue(const rttr::instance& obj, 
		rttr::property& curPoperty, 
		const std::string& InValue)
	{
		auto propType = curPoperty.get_type();
		if (propType.is_wrapper())
		{
			propType = propType.get_wrapped_type();
		}

		if (propType.is_arithmetic())
		{
			return NumericConvert<bool,
				char,
				float,
				double,

				int8_t,
				int16_t,
				int32_t,
				int64_t,

				uint8_t,
				uint16_t,
				uint32_t,
				uint64_t>(obj, curPoperty, InValue);
		}
		else if (propType.is_enumeration())
		{
			rttr::enumeration enumType = propType.get_enumeration();

			if (curPoperty.set_value(obj, enumType.name_to_value(InValue)) == false)
			{
				SPP_LOG(LOG_REFL, LOG_INFO, "SetPropertyValue enum failed");
			}

			return true;
		}
		else if (propType == rttr::type::get<std::string>())
		{
			if (curPoperty.set_value(obj, InValue) == false)
			{
				SPP_LOG(LOG_REFL, LOG_INFO, "SetPropertyValue string failed");
			}

			return true;
		}

		return false;
	}

	bool SetVariantValue(rttr::variant& InVariant, const std::string& InValue)
	{
		auto VarType = InVariant.get_type();
		SE_ASSERT(VarType.is_wrapper());
		VarType = VarType.get_wrapped_type();
				
		if (VarType.is_arithmetic())
		{
			return NumericConvert<bool,
				char,
				float,
				double,

				int8_t,
				int16_t,
				int32_t,
				int64_t,

				uint8_t,
				uint16_t,
				uint32_t,
				uint64_t>(InVariant, InValue);
		}
		else if (VarType.is_enumeration())
		{
			//TBD
			//rttr::enumeration enumType = propType.get_enumeration();

			//if (curPoperty.set_value(obj, enumType.name_to_value(InValue)) == false)
			//{
			//	SPP_LOG(LOG_REFL, LOG_INFO, "SetPropertyValue enum failed");
			//}

			return true;
		}
		else if (VarType == rttr::type::get<std::string>())
		{
			std::reference_wrapper<std::string> wrappedValue =
				InVariant.get_value< std::reference_wrapper<std::string> >();
			wrappedValue.get() = InValue;
			return true;
		}

		return false;
	}

	void JSONToPOD(const rttr::instance& inValue, const Json::Value& InJsonValue)
	{
		auto originalType = inValue.get_type();
		SE_ASSERT(originalType.is_wrapper());

		SPP_LOG(LOG_REFL, LOG_INFO, "JSONToPOD: %s", originalType.get_name().to_string().c_str());

		rttr::instance orgobj = inValue;
		rttr::instance obj = orgobj.get_type().get_raw_type().is_wrapper() ? orgobj.get_wrapped_instance() : orgobj;
		auto curType = obj.get_derived_type();
				
		for (Json::Value::const_iterator itr = InJsonValue.begin(); itr != InJsonValue.end(); itr++)
		{
			std::string PropName = itr.key().asCString();
			std::string PropValue = (*itr).asCString();

			auto prop = curType.get_property(PropName.c_str());

			if (prop.is_valid())
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

				if (propType.is_arithmetic() ||
					propType.is_enumeration() ||
					propType == rttr::type::get<std::string>())
				{			
					SetVariantValue(org_prop_value, PropValue);
				}
				else if (propType.is_sequential_container())
				{
					SPP_LOG(LOG_REFL, LOG_INFO, " - sequential container UNSUPPORTED!!!");
					continue;
				}
				else if (propType.is_associative_container())
				{
					SPP_LOG(LOG_REFL, LOG_INFO, " - associative container UNSUPPORTED!!!");
					continue;
				}
				else if (propType.is_class() || propType.is_pointer())
				{
					//INTERNAL_ObjectToJSON(org_prop_value, newProperty, depth + 1);
					SPP_LOG(LOG_REFL, LOG_INFO, " - not SUPPORTED yet...");
					continue;
				}

			}
		}
	}
}
