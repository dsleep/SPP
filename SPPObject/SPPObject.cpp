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
	namespace internal
	{
		static std::mutex MapLock;
		static uint32_t nameIdx = 1;

		static std::map<std::string, uint32_t>& GetNameToID()
		{
			static std::map<std::string, uint32_t> sO;
			return sO;
		}

		static std::map<uint32_t, const char*>& GetIDToName()
		{
			static std::map<uint32_t, const char*> sO;
			return sO;
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

	struct Referencer
	{
		virtual ~Referencer() {}
		virtual std::shared_ptr< SPPObject > GetObject() = 0;
	};


	struct StrongReferencer : public Referencer
	{
		std::shared_ptr< SPPObject > _obj;

		StrongReferencer(std::shared_ptr< SPPObject > InObj) : _obj(InObj) {}

		virtual ~StrongReferencer() {}
		virtual std::shared_ptr< SPPObject > GetObject() override
		{
			return _obj;
		}
	};

	struct WeakReferencer : public Referencer
	{
		std::weak_ptr< SPPObject > _obj;

		WeakReferencer(std::shared_ptr< SPPObject > InObj) : _obj(InObj) {}

		virtual ~WeakReferencer() {}
		virtual std::shared_ptr< SPPObject > GetObject() override
		{
			return _obj.lock();
		}
	};

	static std::unordered_map<ObjectPath, Referencer*, ObjectPath::HASH>& GetObjectMapTable()
	{
		static std::unordered_map<ObjectPath, Referencer*, ObjectPath::HASH> sO;
		return sO;
	}


	std::shared_ptr<SPPObject> GetObject(const ObjectPath &pathIn)
	{
		auto& curTable = GetObjectMapTable();

		auto found = curTable.find(pathIn);

		if (found != curTable.end())
		{
			return found->second->GetObject();
		}

		return nullptr;
	}

	std::unordered_map< NumberedString, std::function< SPPObject* (const ObjectPath& ) >, NumberedString::HASH >& INTERNEL_GetObjectAllocationMap()
	{
		static std::unordered_map< NumberedString, std::function< SPPObject* (const ObjectPath& ) >, NumberedString::HASH > sO;
		return sO;
	}

	SPPObject::SPPObject(const ObjectPath& InPath) : _path(InPath)
	{

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

	std::shared_ptr<SPPObject> SPPObject::_createobject(const ObjectPath& pathIn, const char *ObjName, bool bGlobalRef)
	{
		auto& curTable = GetObjectMapTable();

		auto& allocMap = INTERNEL_GetObjectAllocationMap();
		NumberedString sObjName(ObjName);
		auto allocFnc = allocMap.find(sObjName);

		SE_ASSERT(allocFnc != allocMap.end());
				
		auto found = curTable.find(pathIn);

		if (found != curTable.end())
		{
			auto HasObject = found->second->GetObject();
			if (HasObject)
			{
				return HasObject;
			}
			delete found->second;
		}

		SPPObject* allocatedObject = allocFnc->second(pathIn);
		SE_ASSERT(allocatedObject != nullptr);

		auto oObjectRef = std::shared_ptr<SPPObject>(allocatedObject);

		if (bGlobalRef)
		{
			curTable[pathIn] = new StrongReferencer(oObjectRef);
		}
		else
		{
			curTable[pathIn] = new WeakReferencer(oObjectRef);
		}

		return oObjectRef;
	}
}