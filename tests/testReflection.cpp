// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPReflection.h"
#include <iostream>

using namespace SPP;

LogEntry LOG_APP("APP");

void UpdateName(const std::string &InName)
{


}

void UpdateHealth(int32_t InValue, int32_t InValue2 = 12)
{


}

void UpdateConfig(const std::string& InValue)
{


}

template<typename... Args>
void CallCPPReflected(const std::string& InMethod, const Args&... InArgs)
{
	using namespace rttr;

	const int ArgCount = sizeof...(Args);

	method meth = type::get_global_method(InMethod.c_str());
	if (meth) // check if the function was found
	{
		auto paramInfos = meth.get_parameter_infos();
			//return_value = meth.invoke({}, 9.0, 3.0); // invoke with empty instance
			//if (return_value.is_valid() && return_value.is_type<double>())
			//	std::cout << return_value.get_value<double>() << std::endl; // 
				
		std::vector< variant > variants;
		variants.reserve(ArgCount);
		auto loop = [&](auto&& input)
		{
			variants.push_back(input);
		};

		(loop(InArgs), ...);

		std::cout << "Total Params: " << paramInfos.size() << std::endl;

		std::vector< type > desiredTypes;
		desiredTypes.reserve(paramInfos.size());
		for (const auto& info : paramInfos)
		{
			desiredTypes.push_back(info.get_type());
			// print all names of the parameter types and its position in the paramter list
			std::cout << " paramtype: '" << info.get_type().get_name() << "'\n"
				<< "index: " << info.get_index()
				<< "default: " << info.has_default_value()
				<< std::endl;
		}

		std::vector< argument > arguments;
		arguments.reserve(ArgCount);
		for(int32_t Iter =0; Iter < variants.size(); ++Iter)
		{
			if (variants[Iter].get_type() != desiredTypes[Iter])
			{
				if (variants[Iter].get_type().is_arithmetic() && desiredTypes[Iter].is_arithmetic())
				{
					variants[Iter].convert((const type)desiredTypes[Iter]);
				}				

				std::cout << "CONVERTING paramtype: '" << variants[Iter].get_type().get_name() << std::endl;
			}
			else
			{
				std::cout << "PASSING paramtype: '" << variants[Iter].get_type().get_name() << std::endl;
			}
			arguments.push_back(variants[Iter]);
		}

		auto results = meth.invoke_variadic({}, arguments);
		if (results.is_valid() == false)
		{
			std::cout << "FAILED!" << std::endl;
		}
	}
}

RTTR_REGISTRATION
{
	using namespace rttr;
	registration::method("UpdateConfig", &UpdateConfig);
	registration::method("UpdateHealth", &UpdateHealth);
	registration::method("UpdateName", &UpdateName);
}

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	CallCPPReflected("UpdateName", std::string("newname"));
	CallCPPReflected("UpdateName", "newname");
	CallCPPReflected("UpdateHealth", 2.45, 50);
	
	return 0;
}
