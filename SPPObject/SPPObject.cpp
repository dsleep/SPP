#include "SPPObject.h"
#include "SPPString.h"
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <memory>
#include <functional>
#include <mutex>

namespace SPP
{
	static SPPObject* GFirstObject = nullptr;
	void SPPObject::Link()
	{
		GFirstObject = this;
		this->nextObj = GFirstObject;
		if (GFirstObject) GFirstObject->prevObj = this;
	}
	void SPPObject::Unlink()
	{
		if (GFirstObject == this)
		{
			GFirstObject = nextObj;
		}
		else
		{
			SE_ASSERT(GFirstObject->prevObj);

			GFirstObject->prevObj->nextObj = nextObj;
			nextObj->prevObj = prevObj;

			nextObj = nullptr;
			prevObj = nullptr;
		}
	}


	const NumberedString& MetaPath::operator[](uint32_t index) const
	{
		return _path[index];
	}

	bool MetaPath::operator< (const MetaPath& cmpTo) const
	{
		if (_path.size() != cmpTo._path.size())
		{
			return _path.size() < cmpTo._path.size();
		}

		for (int32_t Iter = 0; Iter < _path.size(); Iter++)
		{
			if (_path[Iter] != cmpTo[Iter])
			{
				return _path[Iter] < cmpTo[Iter];
			}
		}

		return false;
	}


	bool MetaPath::operator==(const MetaPath& cmpTo) const
	{
		if (_path.size() != cmpTo._path.size())
		{
			return false;
		}

		for (int32_t Iter = 0; Iter < _path.size(); Iter++)
		{
			if (_path[Iter] != cmpTo[Iter])
			{
				return false;
			}
		}

		return true;
	}

	SPPObject::SPPObject(const MetaPath& InPath) : _path(InPath)
	{
		Link();
	}

	SPPObject::~SPPObject()
	{
		Unlink();
	}


	MetaPath::MetaPath(const char* InPath)
	{
		auto splitPath = std::str_split(InPath, '.');

		SE_ASSERT(!splitPath.empty());

		_path.reserve(splitPath.size());
		for (auto& curStr : splitPath)
		{
			_path.push_back(curStr.c_str());
		}
	}

	size_t MetaPath::Hash() const
	{
		if(_path.empty())return 0;

		size_t hashValue = _path[0].GetID();

		for (int32_t Iter = 1; Iter < _path.size(); Iter++)
		{
			hashValue ^= _path[Iter].GetID();;
		}

		return hashValue;
	}	


	
	struct TextureFactors
	{
	public:
		int32_t MipSomething;
		float Scalar;
	};

	class OTexture : public SPPObject
	{
		RTTR_ENABLE(SPPObject);

	protected:
		uint16_t _width;
		uint16_t _height;
		TextureFactors _factors;

	public:

		OTexture(const MetaPath& InPath) : SPPObject(InPath) { }


		RTTR_REGISTRATION_FRIEND
	};	
}

using namespace SPP;

RTTR_REGISTRATION
{
	rttr::registration::class_<TextureFactors>("TextureFactors")
		.property("MipSomething", &TextureFactors::MipSomething)
		.property("Scalar", &TextureFactors::Scalar)
	;

	rttr::registration::class_<OTexture>("OTexture")
		.property("_width", &OTexture::_width)
		.property("_width", &OTexture::_height)
		.property("_factors", &OTexture::_factors)
	;
}

template<typename Ser>
bool SerializePoDTypes(const rttr::type& t, rttr::variant& var, Ser& InSer)
{
	if (t.is_arithmetic())
	{
		//if (t == rttr::type::get<bool>())
		//	writer.Bool(var.to_bool());
		//else if (t == rttr::type::get<char>())
		//	writer.Bool(var.to_bool());
		//else if (t == rttr::type::get<int8_t>())
		//	writer.Int(var.to_int8());
		//else if (t == rttr::type::get<int16_t>())
		//	writer.Int(var.to_int16());
		//else if (t == rttr::type::get<int32_t>())
		//	writer.Int(var.to_int32());
		//else if (t == rttr::type::get<int64_t>())
		//	writer.Int64(var.to_int64());
		//else if (t == rttr::type::get<uint8_t>())
		//	writer.Uint(var.to_uint8());
		//else if (t == rttr::type::get<uint16_t>())
		//	writer.Uint(var.to_uint16());
		//else if (t == rttr::type::get<uint32_t>())
		//	writer.Uint(var.to_uint32());
		//else if (t == rttr::type::get<uint64_t>())
		//	writer.Uint64(var.to_uint64());
		//else if (t == rttr::type::get<float>())
		//	writer.Double(var.to_double());
		//else if (t == rttr::type::get<double>())
		//	writer.Double(var.to_double());

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
		//writer.String(var.to_string());
		return true;
	}

	return false;
}

template<typename Ser>
void SerializeArray(const rttr::variant_sequential_view& view, Ser& InSer)
{
	for (const auto& item : view)
	{
		if (item.is_sequential_container())
		{
			SerializeArray(item.create_sequential_view(), writer);
		}
		else
		{
			rttr::variant wrapped_var = item.extract_wrapped_value();
			rttr::type value_type = wrapped_var.get_type();
			if (value_type.is_arithmetic() || value_type == rttr::type::get<std::string>() || value_type.is_enumeration())
			{
				SerializePoDTypes(value_type, wrapped_var, writer);
			}
			else // object
			{
				SerializeObject(wrapped_var, writer);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////////

template<typename Ser>
void SerializeAssociativeContainer(const rttr::variant_associative_view& view, Ser& InSer)
{
	if (view.is_key_only_type())
	{
		for (auto& item : view)
		{
			SerializeVariant(item.first, InSer);
		}
	}
	else
	{
		for (auto& item : view)
		{
			//writer.StartObject();
			//writer.String(key_name.data(), static_cast<rapidjson::SizeType>(key_name.length()), false);

			//write_variant(item.first, writer);

			//writer.String(value_name.data(), static_cast<rapidjson::SizeType>(value_name.length()), false);

			//write_variant(item.second, writer);

			//writer.EndObject();
		}
	}
}

template<typename Ser>
bool SerializeVariant( rttr::variant& var, Ser& InSer)
{
	auto value_type = var.get_type();
	auto wrapped_type = value_type.is_wrapper() ? value_type.get_wrapped_type() : value_type;
	bool is_wrapper = wrapped_type != value_type;

	if (SerializePoDTypes(is_wrapper ? wrapped_type : value_type,
		is_wrapper ? var.extract_wrapped_value() : var, InSer))
	{
	}
	else if (var.is_sequential_container())
	{
		SerializeArray(var.create_sequential_view(), InSer);
	}
	else if (var.is_associative_container())
	{
		SerializeAssociativeContainer(var.create_associative_view(), InSer);
	}
	else
	{
		auto child_props = is_wrapper ? wrapped_type.get_properties() : value_type.get_properties();
		if (!child_props.empty())
		{
			SerializeObject(var, InSer);
		}
		else
		{
			//bool ok = false;
			//auto text = var.to_string(&ok);
			//if (!ok)
			//{
			//	writer.String(text);
			//	return false;
			//}

			//writer.String(text);
		}
	}

}

template<typename Ser>
void SerializeObject( rttr::instance& InObjectInstance, Ser &InSer)
{
	rttr::instance obj = InObjectInstance.get_type().get_raw_type().is_wrapper() ?
		InObjectInstance.get_wrapped_instance() : InObjectInstance;

	auto prop_list = obj.get_derived_type().get_properties();
	for (auto prop : prop_list)
	{
		if (prop.get_metadata("NO_SERIALIZE"))
			continue;

		rttr::variant prop_value = prop.get_value(obj);
		if (!prop_value)
			continue; // cannot serialize, because we cannot retrieve the value

		const auto name = prop.get_name();
		//writer.String(name.data(), static_cast<rapidjson::SizeType>(name.length()), false);
		if (!SerializeVariant(prop_value, InSer))
		{
			std::cerr << "cannot serialize property: " << name << std::endl;
		}
	}

}