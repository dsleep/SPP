// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPStackUtils.h"

#if _WIN32

#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <Psapi.h>
#include <algorithm>
#include <iterator>
#include <iostream>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")

// Some versions of imagehlp.dll lack the proper packing directives themselves
// so we need to do it.
#pragma pack( push, before_imagehlp, 8 )
#include <imagehlp.h>
#pragma pack( pop, before_imagehlp )

struct module_data 
{
	std::string image_name;
	std::string module_name;
	void *base_address;
	DWORD load_size;
};

//DWORD DumpStackTrace(EXCEPTION_POINTERS *ep);

class symbol {
	typedef IMAGEHLP_SYMBOL64 sym_type;
	sym_type *sym;
	static const int max_name_len = 1024;

public:
	symbol(HANDLE process, DWORD64 address) : sym((sym_type *)::operator new(sizeof(*sym) + max_name_len)) {
		memset(sym, '\0', sizeof(*sym) + max_name_len);
		sym->SizeOfStruct = sizeof(*sym);
		sym->MaxNameLength = max_name_len;
		DWORD64 displacement;

		SymGetSymFromAddr64(process, address, &displacement, sym);
	}

	std::string name() { return std::string(sym->Name); }
	std::string undecorated_name() {
		if (*sym->Name == '\0')
			return "<couldn't map PC to fn name>";
		std::vector<char> und_name(max_name_len);
		UnDecorateSymbolName(sym->Name, &und_name[0], max_name_len, UNDNAME_COMPLETE);
		return std::string(&und_name[0], strlen(&und_name[0]));
	}
};


class get_mod_info {
	HANDLE process;
	static const int buffer_length = 4096;
public:
	get_mod_info(HANDLE h) : process(h) {}

	module_data operator()(HMODULE module) {
		module_data ret;
		char temp[buffer_length];
		MODULEINFO mi;

		GetModuleInformation(process, module, &mi, sizeof(mi));
		ret.base_address = mi.lpBaseOfDll;
		ret.load_size = mi.SizeOfImage;

		GetModuleFileNameExA(process, module, temp, sizeof(temp));
		ret.image_name = temp;
		GetModuleBaseNameA(process, module, temp, sizeof(temp));
		ret.module_name = temp;
		std::vector<char> img(ret.image_name.begin(), ret.image_name.end());
		std::vector<char> mod(ret.module_name.begin(), ret.module_name.end());
		SymLoadModule64(process, 0, &img[0], &mod[0], (DWORD64)ret.base_address, ret.load_size);
		return ret;
	}
};


namespace SPP
{
	LogEntry LOG_STACK("STACK");

	// if you use C++ exception handling: install a translator function
	// with set_se_translator(). In the context of that function (but *not*
	// afterwards), you can either do your stack dump, or save the CONTEXT
	// record as a local copy. Note that you must do the stack dump at the
	// earliest opportunity, to avoid the interesting stack-frames being gone
	// by the time you do the dump.
	uint32_t DumpStackTrace(_EXCEPTION_POINTERS* ep)
	{
		if (IsDebuggerPresent())return 0;

		HANDLE process = GetCurrentProcess();
		HANDLE hThread = GetCurrentThread();
		int frame_number = 0;
		DWORD offset_from_symbol = 0;
		IMAGEHLP_LINE64 line = { 0 };
		std::vector<module_data> modules;
		DWORD cbNeeded;
		std::vector<HMODULE> module_handles(1);

		// Load the symbols:
		// WARNING: You'll need to replace <pdb-search-path> with either NULL
		// or some folder where your clients will be able to find the .pdb file.
		if (!SymInitialize(process, nullptr, false))
			throw(std::logic_error("Unable to initialize symbol handler"));
		DWORD symOptions = SymGetOptions();
		symOptions |= SYMOPT_LOAD_LINES | SYMOPT_UNDNAME;
		SymSetOptions(symOptions);
		EnumProcessModules(process, &module_handles[0], module_handles.size() * sizeof(HMODULE), &cbNeeded);
		module_handles.resize(cbNeeded / sizeof(HMODULE));
		EnumProcessModules(process, &module_handles[0], module_handles.size() * sizeof(HMODULE), &cbNeeded);
		std::transform(module_handles.begin(), module_handles.end(), std::back_inserter(modules), get_mod_info(process));
		void* base = modules[0].base_address;

		// Setup stuff:
		CONTEXT* context = ep->ContextRecord;
#ifdef _M_X64
		STACKFRAME64 frame;
		frame.AddrPC.Offset = context->Rip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context->Rsp;
		frame.AddrStack.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context->Rbp;
		frame.AddrFrame.Mode = AddrModeFlat;
#else
		STACKFRAME64 frame;
		frame.AddrPC.Offset = context->Eip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context->Esp;
		frame.AddrStack.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context->Ebp;
		frame.AddrFrame.Mode = AddrModeFlat;
#endif
		line.SizeOfStruct = sizeof line;
		IMAGE_NT_HEADERS* h = ImageNtHeader(base);
		DWORD image_type = h->FileHeader.Machine;
		int n = 0;

		// Build the string:
		std::ostringstream builder;
		do {
			if (frame.AddrPC.Offset != 0) {
				std::string fnName = symbol(process, frame.AddrPC.Offset).undecorated_name();

				auto FilePathStem = line.FileName ? stdfs::path(line.FileName).filename().generic_string() : std::string("UNKNOWN");
				
				builder << "CALLSTACK" << std::endl;

				if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &offset_from_symbol, &line))
					builder << "(" << n << ")" << "File:" << FilePathStem << " Line#:" << line.LineNumber << " Func:" << fnName << std::endl;
				else builder << "\n";
				if (fnName == "main")
					break;
				if (fnName == "RaiseException") {
					// This is what we get when compiled in Release mode:
					std::cout << "Crash" << std::endl;
					std::cout << "Your program has crashed" << std::endl;
					return EXCEPTION_CONTINUE_SEARCH;
				}
			}
			else
				builder << "(No Symbols: PC == 0)";
			if (!StackWalk64(image_type, process, hThread, &frame, context, NULL,
				SymFunctionTableAccess64, SymGetModuleBase64, NULL))
				break;
			if (++n > 10)
				break;
		} while (frame.AddrReturn.Offset != 0);
		//return EXCEPTION_EXECUTE_HANDLER;
		SymCleanup(process);

		SPP_LOG(LOG_STACK, LOG_INFO, builder.str().c_str());

		//MessageBoxA(
		//	nullptr,
		//	builder.str().c_str(),
		//	"CRASH",
		//	MB_OK
		//);

		std::cout << builder.str();
		return EXCEPTION_CONTINUE_SEARCH;
	}

}

#endif