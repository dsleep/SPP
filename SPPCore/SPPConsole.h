// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <functional>

namespace SPP
{
	class SPP_CORE_API Console
	{
	private:
		struct Impl;
		std::unique_ptr<Impl> _impl;

		void _callCommand(const std::string& InCommand);

	public:
		Console();
		void RegisterCommand(const std::string& InCommand,
			const std::function< void(const std::string&) >& InFunc,
			const std::string& InDescription = "");
		void UnregisterCommand(const std::string& InCommand);

		void CallCommand(const std::string& InCommand);
		void ProcessCommands();
	};

	SPP_CORE_API Console &GetGlobalConsole();
}

#define REGISTER_CONSOLE_COMMAND(InCmd, InFunc, InDesc) 						\
		SPP_AUTOREG_START														\
			SPP::GetGlobalConsole().RegisterCommand(InCmd, InFunc, InDesc);		\
		SPP_AUTOREG_END													 


