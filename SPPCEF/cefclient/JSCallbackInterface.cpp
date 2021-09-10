#include <windows.h>

#include "include/base/cef_scoped_ptr.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "cefclient/browser/main_context_impl.h"
#include "cefclient/browser/main_message_loop_multithreaded_win.h"
#include "cefclient/browser/root_window_manager.h"
#include "cefclient/browser/test_runner.h"

#include "cefclient/browser/client_handler.h"
#include "shared/browser/client_app_browser.h"
#include "shared/browser/main_message_loop_external_pump.h"
#include "shared/browser/main_message_loop_std.h"
#include "shared/common/client_app_other.h"
#include "shared/common/client_switches.h"
#include "shared/renderer/client_app_renderer.h"

#include "JSCallbackInterface.h"

namespace SPP
{
	JavascriptInterface::JavascriptInterface(std::function<void(const std::string&, Json::Value&) >& InRecvFunc)
	{
		_fallbackFunc = InRecvFunc;
	}

	void JavascriptInterface::NativeFromJS_JSON_Callback(const std::string &InJSON)
	{
		Json::Value root;
		Json::CharReaderBuilder Builder;
		Json::CharReader* reader = Builder.newCharReader();
		Json::String Errors;

		bool parsingSuccessful = reader->parse(InJSON.c_str(), InJSON.c_str() + InJSON.length(), &root, &Errors);
		delete reader;

		if (parsingSuccessful)
		{
			Json::Value jsFuncName = root.get("func", Json::Value::nullSingleton());
			if (jsFuncName.isNull() == false)
			{
				Json::Value jsarg = root.get("args", Json::Value::nullSingleton());
				std::string functionName = jsFuncName.asCString();
				auto foundFunction = _nativeFuncMap.find(functionName);
				if (foundFunction != _nativeFuncMap.end())
				{
					foundFunction->second(jsarg);
				}
				else if (_fallbackFunc)
				{
					_fallbackFunc(functionName, jsarg);
				}
				else
				{
					LOG(WARNING) << "NativeFromJS_JSON_Callback couldnt find a match for " << functionName;
				}
			}
			else
			{
				LOG(WARNING) << "NativeFromJS_JSON_Callback called with no func parameter";
			}
		}
	}

	void JavascriptInterface::Add_JSToNativeFunctionHandler(const std::string &InName, NativeFunction InFunction)
	{
		LOG(WARNING) << "Add_JSToNativeFunctionHandler " << InName;
		_nativeFuncMap[InName] = InFunction;
	}
	void JavascriptInterface::Remove_JSToNativeFunctionHandler(const std::string &InName)
	{
		LOG(WARNING) << "Remove_JSToNativeFunctionHandler " << InName;
		_nativeFuncMap.erase(InName);
	}
}