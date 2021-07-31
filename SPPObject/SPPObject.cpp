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


	class SPPMetaStruct : public SPPMetaType
	{
	protected:
		std::shared_ptr< SPPMetaType > _parent;
		std::vector< class SPPField > _fields;

	public:
		virtual void Write(class Serializer& InSerializer, const uint8_t *InData) const override
		{
			if (_parent)
			{
				_parent->Write(InSerializer, InData);
			}

			for (auto& field : _fields)
			{
				field.type->Write(InSerializer, InData + field.offset);
			}
		}
		virtual void Read(class Serializer& InSerializer, uint8_t* OutData) const override
		{
			if (_parent)
			{
				_parent->Read(InSerializer, OutData);
			}

			for (auto& field : _fields)
			{
				field.type->Read(InSerializer, OutData + field.offset);
			}
		}
	};
	
	template<typename T>
	struct TPODType : public SPPMetaType
	{
		static std::shared_ptr< TPODType<T> > GetSharedMeta()
		{
			static std::shared_ptr< TPODType<T> > sO;
			if (!sO) sO = std::make_shared< TPODType<T> >();
			return sO;
		}

		virtual bool Write(class Serializer& InSerializer)
		{

		}
		virtual bool Read(class Serializer& Serializer)
		{

		}
	};

	struct OTexture_META  : public SPPObject_META
	{
		OTexture_META() : SPPObject_META()
		{
			_parent = OTexture::GetStaticMetaType();
			_fields.push_back({ "_width", offsetof(OTexture, _width), TPODType<uint16_t>::GetSharedMeta() });
		}

		virtual SPPObject* Allocate(const ObjectPath& InPath) const
		{
			return new OTexture(InPath);
		}
	};
}