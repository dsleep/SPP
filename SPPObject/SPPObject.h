// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPGUID.h"
#include "SPPNumberedString.h"
#include "SPPSerialization.h"
#include <vector>
#include <list>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>

#include "json/json.h"

#include <rttr/registration>
#include <rttr/registration_friend>

#if _WIN32 && !defined(SPP_OBJECT_STATIC)

	#ifdef SPP_OBJECT_EXPORT
		#define SPP_OBJECT_API __declspec(dllexport)
	#else
		#define SPP_OBJECT_API __declspec(dllimport)
	#endif

#else

	#define SPP_OBJECT_API 

#endif


namespace SPP
{	
	class SPP_OBJECT_API MetaPath
	{
	private:
		std::vector< NumberedString > _path;

	public:
		size_t Levels() const
		{
			return _path.size();
		}

		MetaPath() = default;
		MetaPath(const char* InPath);

		size_t Hash() const;

		std::string ToString() const;
		std::string TopLevelName() const;

		bool InDomain(const MetaPath &DomainToCheck) const;

		const NumberedString &operator[](uint32_t index) const;

		bool operator< (const MetaPath& cmpTo) const;
		bool operator==(const MetaPath& cmpTo) const;

		struct HASH
		{
			size_t operator()(const MetaPath& InValue) const
			{
				return InValue.Hash();
			}
		};
	};

	class SPP_OBJECT_API SPPObject
	{
		friend SPP_OBJECT_API void IterateObjects(const std::function<bool(SPPObject*)>& InFunction);

		RTTR_ENABLE()
		RTTR_REGISTRATION_FRIEND

		NO_COPY_ALLOWED(SPPObject);
		NO_MOVE_ALLOWED(SPPObject);

	protected:
		MetaPath _path;
		GUID _guid;
		SPPObject(const MetaPath& InPath);

		uint8_t _tempFlags = 0;

		SPPObject* nextObj = nullptr;
		SPPObject* prevObj = nullptr;

		void Link();
		void Unlink();
		bool Finalize() { return true; }

	public:				
		virtual ~SPPObject();
		const GUID& GetGUID() const
		{
			return _guid;
		}
		const MetaPath &GetPath() const
		{
			return _path;
		}
		void SetTempFlags(uint8_t FlagsIn)
		{
			_tempFlags = FlagsIn;
		}
		void SetTempFlag(uint8_t FlagIn)
		{
			_tempFlags |= FlagIn;
		}
		const uint8_t GetTempFlags() const
		{
			return _tempFlags;
		}
	};
	
	SPP_OBJECT_API SPPObject* AllocateObject(const rttr::type &InType, const MetaPath& InPath);
	SPP_OBJECT_API SPPObject* AllocateObject(const char* ObjectType, const MetaPath& InPath);

	template<typename ObjType>
	ObjType* AllocateObject(const MetaPath& InPath)
	{
		auto curType = rttr::type::get<ObjType>();
		return (ObjType*)AllocateObject(curType, InPath);
	}

	SPP_OBJECT_API SPPObject* GetObjectByGUID(const GUID &InGuid);
	SPP_OBJECT_API void IterateObjects(const std::function<bool(SPPObject*)> &InFunction);

	template<typename T>
	class WeakObjPtr
	{
	private:
		GUID _objguid;

	public:
		WeakObjPtr() 
		{
			static_assert(std::is_base_of_v<SPPObject, T>, "Must be OBJECT!!!");
		}
		WeakObjPtr(T* InObj)
		{
			if (InObj)
			{
				_objguid = InObj->GetGUID();
			}
		}
		WeakObjPtr &operator=(T* InObj)
		{
			if (InObj)
			{
				_objguid = InObj->GetGUID();
			}
			else
			{
				_objguid = GUID();
			}
		}
		bool IsValid() const
		{
			if (_objguid.IsValid())
			{
				return (GetObjectByGUID(_objguid) != nullptr);
			}
			return false;
		}
		T* Get()
		{
			if (_objguid.IsValid())
			{
				return GetObjectByGUID(_objguid);
			}
			return nullptr;
		}
	};

	//template<typename ObjectType>
	//ObjectType* TAllocateObject(const MetaPath& InPath)
	//{
		//return static_cast<ObjectType*>(AllocateObject(*ObjectType::GetStaticMetaType(), InPath));
	//}

	SPP_OBJECT_API void SetObjectValue(const rttr::instance& inValue, const std::vector<std::string>& stringStack, const std::string& Value, uint8_t depth);
}