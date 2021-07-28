// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPNumberedString.h"
#include <vector>
#include <list>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>

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
	class SPP_OBJECT_API ObjectPath
	{
	private:
		std::vector< NumberedString > _path;

	public:
		size_t Levels() const
		{
			return _path.size();
		}

		ObjectPath() = default;
		ObjectPath(const char* InPath);

		size_t Hash() const;

		const NumberedString &operator[](uint32_t index) const;

		bool operator< (const ObjectPath& cmpTo) const;
		bool operator==(const ObjectPath& cmpTo) const;

		struct HASH
		{
			size_t operator()(const ObjectPath& InValue) const
			{
				return InValue.Hash();
			}
		};
	};


	#define DEFINE_SPP_OBJECT(ourClass,parentClass)	\
		public: \
			ourClass(const ObjectPath& InPath) : parentClass(InPath) { } \
			ourClass(ourClass const&) = delete;	\
			ourClass& operator=(ourClass const&) = delete; \
			virtual ~ourClass() { } \
			static const char *GetStaticClassName()  \
			{ \
				return #ourClass; \
			} \
			virtual const char *GetOurClassName() const override \
			{ \
				return #ourClass; \
			} 		

	#define IMPLEMENT_SPP_OBJECT(ourClass)	\
			namespace REGAUTO##ourClass \
			{ \
				struct RegisterMe \
				{ \
					RegisterMe() \
					{ \
						auto &rMap = INTERNEL_GetObjectAllocationMap(); \
						rMap[NumberedString(ourClass::GetStaticClassName())] = [](const ObjectPath& InPath) { return new ourClass(InPath); }; \
					} \
				}; \
				RegisterMe _autoREG; \
			}


	struct SPP_OBJECT_API SPPMetaType
	{		
		std::string _name;
	};

	struct SPP_OBJECT_API SPPString_META : public SPPMetaType
	{
		SPPString_META()
		{
			_name = "std::string";
		}

		static std::shared_ptr<SPPString_META> GetSharedMeta()
		{
			static std::shared_ptr<SPPString_META> sO;
			if (!sO) sO = std::make_shared< SPPString_META >();
			return sO;
		}
	};

	class SPP_OBJECT_API SPPField
	{
	protected:
		std::string _name;
		uint32_t _offset;
		std::shared_ptr< SPPMetaType > _type;

	public:

	};

	class SPPMetaStruct : public SPPMetaType
	{
	protected:
		std::shared_ptr< SPPMetaType > _parent;
		std::vector< class SPPField > _fields;
	};

	struct SPPObject_META;

	class SPP_OBJECT_API SPPObject
	{
		friend struct SPPObject_META;
	protected:
		ObjectPath _path;
		SPPObject(const ObjectPath& InPath);

		SPPObject* nextObj = nullptr;
		SPPObject* prevObj = nullptr;

		void Link();
		void Unlink();
		bool Finalize() { return true; }

	public:		
		 
		virtual const char* GetOurClassName() const;
		virtual ~SPPObject();
		virtual std::shared_ptr<SPPObject_META> GetMetaType() const;

		static std::shared_ptr<SPPObject_META> GetStaticMetaType();
		static const char* GetStaticClassName();
	};

	struct SPP_OBJECT_API SPPObject_META : public SPPMetaStruct
	{
		virtual SPPObject* Allocate(const ObjectPath& InPath) const
		{
			return new SPPObject(InPath);
		}
	};

	SPP_OBJECT_API SPPObject* AllocateObject(const SPPObject_META &MetaType, const ObjectPath& InPath);
	SPP_OBJECT_API SPPObject* AllocateObject(const char* ObjectType, const ObjectPath& InPath);

	template<typename ObjectType>
	ObjectType* TAllocateObject(const ObjectPath& InPath)
	{
		return static_cast<ObjectType*>(AllocateObject(*ObjectType::GetStaticMetaType(), InPath));
	}


}