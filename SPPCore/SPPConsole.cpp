// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPConsole.h"
#include "SPPLogging.h"
#include <mutex>

namespace SPP
{
	LogEntry LOG_CONSOLE("CONSOLE");

	struct ConsoleCommand
	{
		std::function< void(const std::string&) > Function;
		std::string Description;
	};

	struct Console::Impl
	{
		std::mutex funcMutex;
		std::map< std::string, ConsoleCommand > funcMap;

		std::mutex cmdMutex;
		std::list< std::string > pendingCommands;
	};

	Console::Console() : _impl(new Impl()) {}

	void Console::RegisterCommand(const std::string& InCommand,
		const std::function< void(const std::string&) >& InFunc,
		const std::string& InDescription)
	{
		SE_ASSERT(!InCommand.empty());
		SE_ASSERT(InFunc);

		ConsoleCommand newCommand{ InFunc, InDescription };

		std::lock_guard<std::mutex> lock(_impl->funcMutex);
		_impl->funcMap[InCommand] = newCommand;
	}

	void Console::UnregisterCommand(const std::string& InCommand)
	{
		std::lock_guard<std::mutex> lock(_impl->funcMutex);
		_impl->funcMap.erase(InCommand);
	}

	void Console::_callCommand(const std::string& InCommand)
	{
		auto foundSpace = InCommand.find(' ');

		//TODO check spacing
		std::string CommandStr = (foundSpace == std::string::npos) ? InCommand : InCommand.substr(0, foundSpace);
		std::string RemainingStr = (foundSpace == std::string::npos) ? "" : InCommand.substr(foundSpace);

		std::lock_guard<std::mutex> lock(_impl->funcMutex);
		auto foundFunc = _impl->funcMap.find(CommandStr);
		if (foundFunc != _impl->funcMap.end())
		{
			foundFunc->second.Function(RemainingStr);
		}
	}

	void Console::CallCommand(const std::string& InCommand)
	{
		std::lock_guard<std::mutex> lock(_impl->cmdMutex);
		_impl->pendingCommands.push_back(InCommand);
	}

	void Console::ProcessCommands()
	{
		std::list< std::string > pendingCommands;
		{
			std::lock_guard<std::mutex> lock(_impl->cmdMutex);
			std::swap(pendingCommands, _impl->pendingCommands);
		}
		for (auto& cmd : pendingCommands)
		{
			_callCommand(cmd);
		}
	}

	Console& GetGlobalConsole()
	{
		static Console sO;
		return sO;
	}
}

