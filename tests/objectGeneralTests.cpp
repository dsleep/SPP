// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPObject.h"
#include "SPPSDF.h"

using namespace SPP;

LogEntry LOG_APP("APP");

template<typename Parent>
class ByteArraySerializer : public Parent
{
protected:
	std::vector<uint8_t> _data;
	int64_t _position = 0;

public:

	//TODO SETUP MOVE &&

	ByteArraySerializer() = default;

	ByteArraySerializer(const std::vector<uint8_t>& InData)
	{
		_data = InData;
	}

	virtual void Seek(int64_t DataPos) override
	{
		SE_ASSERT(DataPos >= 0);
		SE_ASSERT(DataPos <= _data.size());
		_position = DataPos;
	}
	virtual int64_t Tell() const override
	{
		return _position;
	}
	virtual int64_t Size() const override
	{
		return _data.size();
	}
	virtual bool Write(const void* Data, int64_t DataIn) override
	{
		int64_t AdditionalSpace = ((_position + DataIn) - _data.size());
		if (AdditionalSpace > 0)
		{
			_data.resize(_data.size() + AdditionalSpace);
		}
		memcpy(_data.data() + _position, Data, DataIn);
		_position += DataIn;
		return true;
	}

	virtual bool Read(void* Data, int64_t DataIn) override
	{
		SE_ASSERT((DataIn + _position) <= (int64_t)_data.size());
		memcpy(Data, _data.data() + _position, DataIn);
		_position += DataIn;
		return true;
	}

	void MakeCurrentPositionStart()
	{
		_data.erase(_data.begin(), _data.begin() + _position);
		_position = 0;
	}

	operator void* () const
	{
		return (void*)_data.data();
	}

	const void* GetData() const
	{
		return (void*)_data.data();
	}

	const std::vector<uint8_t>& GetArray() const
	{
		return _data;
	}

	std::vector<uint8_t>& GetArray()
	{
		return _data;
	}
};


struct MemberInfo
{
	std::string MemberName;
	std::string MemberType;
	size_t MemberSize;
};

class ObjectSerializer : public BinarySerializer
{
private:
	SPP::MetaPath _domain;
	std::set<SPPObject*> _intObjects;
	std::set<SPPObject*> _extObjects;

public:
	ObjectSerializer()
	{

	}
	
	bool WritePODType(const rttr::type& t, rttr::variant& var)
	{
		if (t.is_arithmetic())
		{
			if (t == rttr::type::get<bool>())
				*this << var.to_bool();
			else if (t == rttr::type::get<char>())
				*this << var.to_int8();
			else if (t == rttr::type::get<int8_t>())
				*this << var.to_int8();
			else if (t == rttr::type::get<int16_t>())
				*this << var.to_int16();
			else if (t == rttr::type::get<int32_t>())
				*this << var.to_int32();
			else if (t == rttr::type::get<int64_t>())
				*this << var.to_int64();
			else if (t == rttr::type::get<uint8_t>())
				*this << var.to_uint8();
			else if (t == rttr::type::get<uint16_t>())
				*this << var.to_uint16();
			else if (t == rttr::type::get<uint32_t>())
				*this << var.to_uint32();
			else if (t == rttr::type::get<uint64_t>())
				*this << var.to_uint64();
			else if (t == rttr::type::get<float>())
				*this << var.to_double();
			else if (t == rttr::type::get<double>())
				*this << var.to_double();

			return true;
		}
		else if (t.is_enumeration())
		{
			//bool ok = false;
			//auto result = var.to_string(&ok);
			//if (ok)
			//{
			//	writer.String(var.to_string());
			//}
			//else
			//{
			//	ok = false;
			//	auto value = var.to_uint64(&ok);
			//	if (ok)
			//		writer.Uint64(value);
			//	else
			//		writer.Null();
			//}

			return true;
		}
		else if (t == rttr::type::get<std::string>())
		{
			*this << var.to_string();
			return true;
		}

		return false;
	}

	void SerializeArray(const rttr::variant_sequential_view& view)
	{
		//for (const auto& item : view)
		//{
		//	//if (item.is_sequential_container())
		//	//{
		//	//	SerializeArray(item.create_sequential_view(), writer);
		//	//}
		//	//else
		//	//{
		//	//	rttr::variant wrapped_var = item.extract_wrapped_value();
		//	//	rttr::type value_type = wrapped_var.get_type();
		//	//	if (value_type.is_arithmetic() || value_type == rttr::type::get<std::string>() || value_type.is_enumeration())
		//	//	{
		//	//		SerializePoDTypes(value_type, wrapped_var, writer);
		//	//	}
		//	//	else // object
		//	//	{
		//	//		SerializeObject(wrapped_var, writer);
		//	//	}
		//	//}
		//}
	}


	/////////////////////////////////////////////////////////////////////////////////////////

	void SerializeAssociativeContainer(const rttr::variant_associative_view& view)
	{
		//if (view.is_key_only_type())
		//{
		//	for (auto& item : view)
		//	{
		//		SerializeVariant(item.first, InSer);
		//	}
		//}
		//else
		//{
		//	for (auto& item : view)
		//	{
		//		//writer.StartObject();
		//		//writer.String(key_name.data(), static_cast<rapidjson::SizeType>(key_name.length()), false);

		//		//write_variant(item.first, writer);

		//		//writer.String(value_name.data(), static_cast<rapidjson::SizeType>(value_name.length()), false);

		//		//write_variant(item.second, writer);

		//		//writer.EndObject();
		//	}
		//}
	}


	void WriteObject(const SPPObject* Value)
	{

	}

	bool WriteVariant(rttr::variant& var)
	{
		auto value_type = var.get_type();
		auto wrapped_type = value_type.is_wrapper() ? value_type.get_wrapped_type() : value_type;
		bool is_wrapper = wrapped_type != value_type;

		
		if (WritePODType(is_wrapper ? wrapped_type : value_type, is_wrapper ? var.extract_wrapped_value() : var))
		{
		}
		else if (var.is_sequential_container())
		{
			SerializeArray(var.create_sequential_view());
		}
		else if (var.is_associative_container())
		{
			SerializeAssociativeContainer(var.create_associative_view());
		}
		else
		{
			auto ObjectType = rttr::type::get<SPPObject>();

			if (wrapped_type.is_derived_from(ObjectType))
			{
				SPP_LOG(LOG_APP, LOG_INFO, "SerializeObject %s", wrapped_type.get_name().data());

			}
			else
			{
				auto child_props = is_wrapper ? wrapped_type.get_properties() : value_type.get_properties();
				if (!child_props.empty())
				{
					WriteStruct(var);
				}
				else
				{
					//HMM			
				}
			}
		}

		return true;
	}

	void WriteStruct(const rttr::instance& inValue)
	{
		rttr::instance obj = inValue.get_type().get_raw_type().is_wrapper() ?
			inValue.get_wrapped_instance() : inValue;
		auto curType = obj.get_type();
		SPP_LOG(LOG_APP, LOG_INFO, "SerializeObject %s", curType.get_name().data());


		auto ObjectType = rttr::type::get<SPPObject>();

		if (curType.is_derived_from(ObjectType))
		{

		}

		//InObjectInstance.get_type()

		auto prop_list = curType.get_properties();
		for (auto prop : prop_list)
		{
			if (prop.get_metadata("NO_SERIALIZE"))
				continue;

			rttr::variant prop_value = prop.get_value(obj);
			if (!prop_value)
				continue; // cannot serialize, because we cannot retrieve the value


			const auto name = prop.get_name().to_string();
			SPP_LOG(LOG_APP, LOG_INFO, " - prop %s", name.data());
			MemberInfo member{ name, prop.get_type().get_name().to_string(), 0 };

			*this << member.MemberName;
			*this << member.MemberType;
			*this << member.MemberSize;
			auto BeforeMember = Tell();

			//writer.String(name.data(), static_cast<rapidjson::SizeType>(name.length()), false);
			if (!WriteVariant(prop_value))
			{
				SPP_LOG(LOG_APP, LOG_INFO, " - cannot serialize property: %s", name.data());
			}

			auto AfterMember = Tell();

			Seek(BeforeMember - sizeof(size_t));
			*this << (size_t)AfterMember - BeforeMember;
			Seek(AfterMember);

			SPP_LOG(LOG_APP, LOG_INFO, " - size: %d", (size_t)AfterMember - BeforeMember);
		}
	}

	//void WriteObject(SPPObject*& Value)
	//{
	//	rttr::instance obj = Value;
	//	auto curType = Value->get_type();

	//	SPP_LOG(LOG_APP, LOG_INFO, "SerializeObject %s", curType.get_name().data());

	//	//InObjectInstance.get_type()

	//	auto prop_list = curType.get_properties();
	//	for (auto prop : prop_list)
	//	{
	//		if (prop.get_metadata("NO_SERIALIZE"))
	//			continue;

	//		rttr::variant prop_value = prop.get_value(obj);
	//		if (!prop_value)
	//			continue; // cannot serialize, because we cannot retrieve the value

	//		auto propType = prop_value.get_type();
	//		auto ObjectType = rttr::type::get<SPPObject>();

	//		if (propType.is_derived_from(ObjectType))
	//		{

	//		}
	//		else
	//		{

	//			auto child_props = is_wrapper ? wrapped_type.get_properties() : value_type.get_properties();
	//			if (!child_props.empty())
	//			{
	//				WriteStruct(var);
	//			}
	//		}
	//		SPP_LOG(LOG_APP, LOG_INFO, " - size: %d", (size_t)AfterMember - BeforeMember);
	//	}
	//}

	//
	void PopulateObjectLinks(rttr::variant& var)
	{
		auto ObjectType = rttr::type::get<SPPObject>();

		rttr::instance obj = var;
		auto curType = var.get_type();

		SPP_LOG(LOG_APP, LOG_INFO, "PopulateObjectLinks %s", curType.get_name().data());

		if (curType.is_derived_from(ObjectType))
		{
			SE_ASSERT(curType.is_pointer());

			auto curObject = var.get_value<SPPObject*>();

			if (curObject)
			{
				if (curObject->GetPath().InDomain(_domain))
				{
					auto [iter, inserted] = _intObjects.insert(curObject);
					if (inserted == false)
					{						
						return;
					}
					else
					{
						SPP_LOG(LOG_APP, LOG_INFO, "Added inteneral %s", curObject->GetPath().ToString().c_str());
					}
				}
				else
				{
					_extObjects.insert(curObject);
					return;
				}
			}
			else
			{
				return;
			}
		}
		else if (curType.is_pointer())
		{
			return;
		}

		auto prop_list = curType.get_properties();
		for (auto prop : prop_list)
		{
			if (prop.get_metadata("NO_SERIALIZE"))
				continue;

			rttr::variant prop_value = prop.get_value(obj);
			if (!prop_value)
				continue; // cannot serialize, because we cannot retrieve the value

			SPP_LOG(LOG_APP, LOG_INFO, " - prop %s", prop.get_name().data());

			PopulateObjectLinks(var);
		}
	}
};

class OWorld : public SPPObject
{
private:

public:
};


void GetPropertiesAsJSON(const rttr::instance& inValue)
{
	rttr::instance obj = inValue.get_type().get_raw_type().is_wrapper() ?
		inValue.get_wrapped_instance() : inValue;
	auto curType = obj.get_type();

	SPP_LOG(LOG_APP, LOG_INFO, "GetPropertiesAsJSON %s", curType.get_name().data());

	auto prop_list = curType.get_properties();
	for (auto prop : prop_list)
	{
		rttr::variant prop_value = prop.get_value(obj);
		if (!prop_value)
			continue; // cannot serialize, because we cannot retrieve the value


		const auto name = prop.get_name().to_string();
		SPP_LOG(LOG_APP, LOG_INFO, " - prop %s", name.data());

		const auto propType = prop_value.get_type();

		if (propType.is_arithmetic())
		{

		}
	}
}




int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	auto CurrentObject = (OElement*) AllocateObject("OElement", "Asset.Textures.Grass.Draw");
	auto CurrentEntity = (OEntity*) AllocateObject("OEntity", "Asset.Textures.Grass");

	CurrentObject->_parent = CurrentEntity;

	std::vector<SPPObject*> objects;
	objects.push_back(CurrentObject);

	ByteArraySerializer< ObjectSerializer> objData;


	rttr::variant asVariant(CurrentObject);
	objData.PopulateObjectLinks(asVariant);

	objData.WriteVariant(asVariant);

	//objData << objects;
	//CurrentObject

	return 0;
}
