// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGarbageCollection.h"
#include "SPPLogging.h"

namespace SPP
{
	extern LogEntry LOG_OBJ;

	bool IsObjectProperty(rttr::type& propType)
	{
		auto ObjectType = rttr::type::get<SPPObject>();
		return propType.is_pointer() && propType.is_derived_from(ObjectType);
	}

	void WalkObjects(const rttr::variant& inValue, const std::function<bool(SPPObject*&)>& InFunction)
	{
		auto originalType = inValue.get_type();
		SE_ASSERT(originalType.is_wrapper());

		//original
		rttr::instance orgobj = inValue;
		//dewrap this dealio
		rttr::instance obj = orgobj.get_type().get_raw_type().is_wrapper() ? orgobj.get_wrapped_instance() : orgobj;
		auto curType = obj.get_derived_type();
		auto ObjectType = rttr::type::get<SPPObject>();

		SPPObject* objRef = nullptr;
		if (curType.is_derived_from(ObjectType))
		{
			objRef = obj.try_convert< SPPObject >();

			std::reference_wrapper<SPPObject*> wrappedValue =
				inValue.get_value< std::reference_wrapper<SPPObject*> >();

			if (objRef)
			{
				if (InFunction(wrappedValue.get()) == false)
				{
					return;
				}
			}
			else
			{
				return;
			}
		}

		auto prop_list = curType.get_properties();
		for (auto prop : prop_list)
		{
			rttr::variant org_prop_value = prop.get_value(obj);
			if (!org_prop_value)
				continue; // cannot serialize, because we cannot retrieve the value

			const auto name = prop.get_name().to_string();
			auto propType = org_prop_value.get_type();

			// it is all wrappers
			if (propType.is_wrapper())
			{
				propType = propType.get_wrapped_type();

				if (IsObjectProperty(propType))
				{
					WalkObjects(org_prop_value, InFunction);
				}
				else if (propType.is_sequential_container())
				{
					auto sub_array_view = org_prop_value.create_sequential_view();
					for (auto& item : sub_array_view)
					{
						WalkObjects(item, InFunction);
					}
				}
				else if (propType.is_associative_container())
				{
					SPP_LOG(LOG_OBJ, LOG_INFO, " - associative container UNSUPPORTED!!!");
					return;
				}
				else if (propType.is_class())
				{
					WalkObjects(org_prop_value, InFunction);
				}
			}
		}
	}

	static std::list< SPPObject* > GC_ROOTS;

	static uint8_t const GC_VISIBLE = 0x01;
	static uint8_t const GC_DYING = 0x02;

	void AddToRoot(SPPObject* InObject)
	{
		GC_ROOTS.push_back(InObject);
	}

	void RemoveFromRoot(SPPObject* InObject)
	{
		GC_ROOTS.remove(InObject);
	}

	void GC_MarkAndSweep()
	{
		IterateObjects([](SPPObject* InObj) -> bool
			{
				InObj->SetTempFlags(0);
				return true;
			});

		for (auto& rootItem : GC_ROOTS)
		{
			WalkObjects(std::ref(rootItem), [](SPPObject*& InObj) -> bool
				{
					auto flags = InObj->GetTempFlags();
					if (flags & GC_VISIBLE)
					{
						return false;
					}
					else
					{
						InObj->SetTempFlag(GC_VISIBLE);
						return true;
					}
				});
		}
		// all objects go back to dying or cleared
		IterateObjects([](SPPObject* InObj) -> bool
			{
				// not visible to root
				if (!(InObj->GetTempFlags() & GC_VISIBLE))
				{
					InObj->SetTempFlags(GC_DYING);
				}
				else
				{
					InObj->SetTempFlags(0);
				}
				return true;
			});

		for (auto& rootItem : GC_ROOTS)
		{
			WalkObjects(std::ref(rootItem), [](SPPObject*& InObj) -> bool
				{
					auto flags = InObj->GetTempFlags();

					if (flags & GC_DYING)
					{
						InObj = nullptr;
						return false;
					}

					if (flags & GC_VISIBLE)
					{
						return false;
					}
					else
					{
						InObj->SetTempFlags(GC_VISIBLE);
						return true;
					}
				});
		}

		// all objects go back to dying or cleared
		IterateObjects([](SPPObject* InObj) -> bool
			{
				// not visible to root
				if (InObj->GetTempFlags() & GC_DYING)
				{
					delete InObj;
				}
				return true;
			});
	}
}