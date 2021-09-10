#pragma once

#include "SPPEngine.h"
#include "SPPApplication.h"
#include <functional>
#include "json/json.h"

#if _WIN32 && !defined(SPP_CEFUI_STATIC)

	#ifdef SPP_CEFUI_EXPORT
		#define SPP_CEFUI_API __declspec(dllexport)
	#else
		#define SPP_CEFUI_API __declspec(dllimport)
	#endif

	#else

		#define SPP_CEFUI_API 

#endif

namespace SPP
{
	struct GameBrowserCallbacks
	{
		std::function< void(void*) > Initialized;
		std::function< void(void) > Update;
		std::function< void(int32_t, int32_t) > WindowResized;
		std::function< void() > Shutdown;
	};

	SPP_CEFUI_API int RunBrowser(void* hInstance, const std::string& StartupURL, 
		const GameBrowserCallbacks& InCallbacks = { nullptr, nullptr, nullptr, nullptr },
		const InputEvents& InInputEvents = { 0 },
		std::function<void(const std::string&, Json::Value&) >* JSFunctionReceiver = nullptr);
}

