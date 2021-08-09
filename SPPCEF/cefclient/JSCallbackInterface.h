#pragma once 

#include "SPPCEFUI.h"
#include <functional>
#include "json/json.h"

namespace SPP
{
	class SPP_CEFUI_API JavascriptInterface
	{
	public:
		using NativeFunction = std::function<void(Json::Value) >;

	private:
		std::map < std::string, NativeFunction > _nativeFuncMap;

	public:
		JavascriptInterface() = default;
		JavascriptInterface(const std::map < std::string, NativeFunction > &InMap);

		void Add_JSToNativeFunctionHandler(const std::string &InName, NativeFunction InFunction);
		void Remove_JSToNativeFunctionHandler(const std::string &InName);
			   
		//BINDED TO CEF
		void NativeFromJS_JSON_Callback(const std::string &InJSON);

		template<typename... Args>
		static inline void CallJS(const char* MessageName, const Args&... InArgs)
		{
			CEFMessage messageCreate(MessageName);

			int32_t Iter = 0;
			auto loop = [&](auto && input)
			{
				messageCreate.SetValue(Iter, input);
				Iter++;
			};

			(loop(InArgs), ...);

			messageCreate.Send();
		}
	};	
}