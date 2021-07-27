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

	const NumberedString& ObjectPath::operator[](uint32_t index) const
	{
		return _path[index];
	}

	bool ObjectPath::operator< (const ObjectPath& cmpTo) const
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


	bool ObjectPath::operator==(const ObjectPath& cmpTo) const
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

	SPPObject::SPPObject(const ObjectPath& InPath) : _path(InPath)
	{
		Link();
	}

	SPPObject::~SPPObject()
	{
		Unlink();
	}

	ObjectPath::ObjectPath(const char* InPath)
	{
		auto splitPath = std::str_split(InPath, '.');

		SE_ASSERT(!splitPath.empty());

		_path.reserve(splitPath.size());
		for (auto& curStr : splitPath)
		{
			_path.push_back(curStr.c_str());
		}
	}

	size_t ObjectPath::Hash() const
	{
		if(_path.empty())return 0;

		size_t hashValue = _path[0].GetID();

		for (int32_t Iter = 1; Iter < _path.size(); Iter++)
		{
			hashValue ^= _path[Iter].GetID();;
		}

		return hashValue;
	}

	static std::map< std::string, SPPObject_META* > &GetObjectMetaMap()
	{
		static std::map< std::string, SPPObject_META* > sO;
		return sO;
	}

	static const SPPObject_META *GetObjectMetaType(const char* ObjectType)
	{
		auto &MetaMap = GetObjectMetaMap();
		auto foundEle = MetaMap.find(ObjectType);
		return (foundEle != MetaMap.end()) ? foundEle->second : nullptr;
	}

	static void RegisterObjectMetaType(const char* ObjectType, SPPObject_META* InMetaType)
	{
		auto& MetaMap = GetObjectMetaMap();
		MetaMap[ObjectType] = InMetaType;
	}

	//std::shared_ptr< SPPObject_META> SPPObject::GetMetaType()
	//{
	//	static std::shared_ptr<SPPObject_META> sO;
	//	if (!sO)
	//	{
	//		sO = std::make_shared< SPPObject_META >();
	//		RegisterObjectMetaType(GetStaticClassName(), sO.get());
	//	}
	//	return sO;
	//}

	SPPObject* AllocateObject(const SPPObject_META& MetaType, const ObjectPath& InPath)
	{
		return MetaType.Allocate(InPath);
	}

	SPPObject* AllocateObject(const char* ObjectType, const ObjectPath& InPath)
	{
		auto* MetaType = GetObjectMetaType(ObjectType);
		return MetaType ? AllocateObject(*MetaType, InPath) : nullptr;
	}
}