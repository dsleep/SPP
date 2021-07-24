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


	SPP_OBJECT_API std::unordered_map< NumberedString, std::function< class SPPObject* (const ObjectPath&) >, NumberedString::HASH >& INTERNEL_GetObjectAllocationMap();

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


	class SPP_OBJECT_API SPPObject
	{
	protected:
		ObjectPath _path;
		SPPObject(const ObjectPath& InPath);

	public:		
		virtual const char* GetOurClassName() const = 0;
		static std::shared_ptr<SPPObject> _createobject(const ObjectPath& pathIn, const char* ObjName, bool bGlobalRef=false);

		template<typename T>
		static std::shared_ptr<T> CreateObject(const ObjectPath& pathIn, bool bGlobalRef = false)
		{
			return std::dynamic_pointer_cast<T>(_createobject(pathIn, T::GetStaticClassName(), bGlobalRef) );
		}
	};

	template<typename T>
	struct TObjectReference
	{
	private:
		static_assert(std::is_base_of<SPPObject, T>::value, "Must be based on object");
		std::weak_ptr<T> _reference;

	public:
		TObjectReference() = default;

		T* operator->() 
		{
			if (auto lckItem = _reference.lock())
			{
				return lckItem.get();
			}

			return nullptr;
		}

		TObjectReference& operator= (const TObjectReference &inOperator)
		{
			_reference = inOperator;
			return *this;
		}

		TObjectReference& operator= (std::shared_ptr<T> inOperator)
		{
			_reference = inOperator;
			return *this;
		}

		operator bool() const
		{
			auto lckItem = _reference.lock();
			return (lckItem);
		}

		void Clear()
		{
			_reference.reset();
		}
	};

	template<typename T>
	struct TOwningObjectReference
	{
	private:
		static_assert(std::is_base_of<SPPObject, T>::value, "Must be based on object");
		std::shared_ptr<T> _reference;

	public:
		TOwningObjectReference() = default;

		T* operator->()
		{
			return _reference.get();
		}

		operator bool() const
		{
			return (_reference);
		}

		void Clear()
		{
			_reference.reset();
		}
	};

}