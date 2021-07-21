// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPObject.h"
#include <vector>
#include <list>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>

namespace SPP
{
	class SPP_CORE_API SPPField
	{
	protected:
		std::string _name;
		int32_t _offset;

	public:
		SPPField() = default;

		virtual uint32_t GetSize() const = 0;
		virtual const char *GetType() const = 0;
	};

	class SPP_CORE_API SPPArrayField : public SPPField
	{
	protected:
		std::shared_ptr< SPPField > _baseField;
		int32_t _count;

	public:
		virtual uint32_t GetSize() const
		{
			return (_baseField->GetSize() * _count);
		}
		virtual const char* GetType() const
		{
			return "array";
		}
	};

	class SPP_CORE_API SPPInt8Field : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(int8_t); }
		virtual const char* GetType() const { return "int8_t"; }
	};

	class SPP_CORE_API SPPUInt8Field : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(uint8_t); }
		virtual const char* GetType() const { return "uint8_t"; }
	};

	class SPP_CORE_API SPPInt16Field : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(uint8_t); }
		virtual const char* GetType() const { return "uint8_t"; }
	};

	class SPP_CORE_API SPPUInt16Field : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(uint8_t); }
		virtual const char* GetType() const { return "uint8_t"; }
	};

	class SPP_CORE_API SPPInt32Field : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(uint8_t); }
		virtual const char* GetType() const { return "uint8_t"; }
	};

	class SPP_CORE_API SPPUInt32Field : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(uint8_t); }
		virtual const char* GetType() const { return "uint8_t"; }
	};

	class SPP_CORE_API SPPFloatField : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(float); }
		virtual const char* GetType() const { return "float"; }
	};

	class SPP_CORE_API SPPDoubleField : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(double); }
		virtual const char* GetType() const { return "double"; }
	};

	class SPP_CORE_API SPPStringField : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(double); }
		virtual const char* GetType() const { return "double"; }
	};
		
	class SPP_CORE_API SPPObjectField : public SPPField
	{
		virtual uint32_t GetSize() const { return sizeof(void*); }
		virtual const char* GetType() const { return "object"; }
	};

	class SPP_CORE_API SPPStruct
	{
	protected:
		ObjectPath _name;		
		std::shared_ptr< SPPField > _fields;

	public:		
		virtual const char* GetOurClassName() const = 0;
		static std::shared_ptr<SPPObject> _createobject(const ObjectPath& pathIn, const char* ObjName, bool bGlobalRef=false);

		template<typename T>
		static std::shared_ptr<T> CreateObject(const ObjectPath& pathIn, bool bGlobalRef = false)
		{
			return std::dynamic_pointer_cast<T>(_createobject(pathIn, T::GetStaticClassName(), bGlobalRef) );
		}
	};

	

}