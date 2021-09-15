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

	class SPP_CEFUI_API CEFMessage
	{
	private:
		struct CEFMessageImpl;
		std::unique_ptr<CEFMessageImpl> _impl;

	public:
		CEFMessage(const char* MessageName);
		~CEFMessage();

		bool SetBool(size_t index, bool value);
		bool SetInt(size_t index, int value);
		bool SetDouble(size_t index, double value);
		bool SetString(size_t index, const  std::string& value);

		inline bool SetValue(size_t Idx, const bool& InValue)
		{
			return SetBool(Idx, InValue);
		}
		inline bool SetValue(size_t Idx, const int& InValue)
		{
			return SetInt(Idx, (double)InValue);
		}
		inline bool SetValue(size_t Idx, const double& InValue)
		{
			return SetDouble(Idx, InValue);
		}
		inline bool SetValue(size_t Idx, const std::string& InValue)
		{
			return SetString(Idx, InValue);
		}

		bool Send();
	};

	SPP_CEFUI_API int RunBrowser(void* hInstance, const std::string& StartupURL, 
		const GameBrowserCallbacks& InCallbacks = { nullptr, nullptr, nullptr, nullptr },
		const InputEvents& InInputEvents = { 0 },
		std::function<void(const std::string&, Json::Value&) >* JSFunctionReceiver = nullptr);
}

