// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPNumberedString.h"
#include "SPPSerialization.h"
#include <vector>
#include <list>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>

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
		RTTR_ENABLE()
		RTTR_REGISTRATION_FRIEND

	protected:
		MetaPath _path;
		SPPObject(const MetaPath& InPath);

		SPPObject* nextObj = nullptr;
		SPPObject* prevObj = nullptr;

		void Link();
		void Unlink();
		bool Finalize() { return true; }

	public:				
		virtual ~SPPObject();
		const MetaPath &GetPath() const
		{
			return _path;
		}
	};
	
	//SPP_OBJECT_API SPPObject* AllocateObject(const SPPObject_META &MetaType, const MetaPath& InPath);
	SPP_OBJECT_API SPPObject* AllocateObject(const char* ObjectType, const MetaPath& InPath);

	//template<typename ObjectType>
	//ObjectType* TAllocateObject(const MetaPath& InPath)
	//{
		//return static_cast<ObjectType*>(AllocateObject(*ObjectType::GetStaticMetaType(), InPath));
	//}


}