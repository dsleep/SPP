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

	static std::map< std::string, SPPObject_META* >& GetObjectMetaMap()
	{
		static std::map< std::string, SPPObject_META* > sO;
		return sO;
	}

	static const SPPObject_META* GetObjectMetaType(const char* ObjectType)
	{
		auto& MetaMap = GetObjectMetaMap();
		auto foundEle = MetaMap.find(ObjectType);
		return (foundEle != MetaMap.end()) ? foundEle->second : nullptr;
	}

	static void RegisterObjectMetaType(const char* ObjectType, SPPObject_META* InMetaType)
	{
		auto& MetaMap = GetObjectMetaMap();
		MetaMap[ObjectType] = InMetaType;
	}


	std::shared_ptr<SPPObject_META> SPPObject::GetStaticMetaType()
	{
		static std::shared_ptr<SPPObject_META> sO;
		if (!sO)
		{
			sO = std::make_shared< SPPObject_META >();
			RegisterObjectMetaType(GetStaticClassName(), sO.get());
		}
		return sO;
	}

	std::shared_ptr< SPPObject_META> SPPObject::GetMetaType() const
	{
		return SPPObject::GetStaticMetaType();
	}

	const char* SPPObject::GetStaticClassName()
	{
		return "SPPObject";
	}

	const char* SPPObject::GetOurClassName() const
	{
		return SPPObject::GetStaticClassName();
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

	SPPObject* AllocateObject(const SPPObject_META& MetaType, const ObjectPath& InPath)
	{
		return MetaType.Allocate(InPath);
	}

	SPPObject* AllocateObject(const char* ObjectType, const ObjectPath& InPath)
	{
		auto* MetaType = GetObjectMetaType(ObjectType);
		return MetaType ? AllocateObject(*MetaType, InPath) : nullptr;
	}

	class OTexture : public SPPObject
	{
		friend struct OTexture_META;

	protected:
		uint16_t _width;
		uint16_t _height;

	public:

		OTexture(const ObjectPath& InPath) : SPPObject(InPath) { }
		virtual const char* GetOurClassName() const
		{
			return OTexture::GetStaticClassName();
		}
		virtual std::shared_ptr<SPPObject_META> GetMetaType() const
		{
			return OTexture::GetStaticMetaType();
		}
		virtual ~OTexture() = default;
		
		static std::shared_ptr<SPPObject_META> GetStaticMetaType()
		{
			static std::shared_ptr<SPPObject_META> sO;
			if (!sO)
			{
				sO = std::make_shared< SPPObject_META >();
				RegisterObjectMetaType(GetStaticClassName(), sO.get());
			}
			return sO;
		}
		static const char* GetStaticClassName()
		{
			return "Texture";
		}
	};

	class SPPInt32Field : public SPPField
	{
	protected:
		std::string _name;
		uint32_t _offset;
		std::shared_ptr< SPPMetaType > _type;

	public:

	};

	struct OTexture_META  : public SPPObject_META
	{
		OTexture_META() : SPPObject_META()
		{
			_parent = OTexture::GetStaticMetaType();
			//_fields.push_back();
		}

		virtual SPPObject* Allocate(const ObjectPath& InPath) const
		{
			return new OTexture(InPath);
		}
	};
}