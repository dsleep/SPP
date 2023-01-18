// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/renderer/client_renderer.h"

#include <sstream>
#include <string>

#include "include/cef_crash_util.h"
#include "include/cef_dom.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"

// Forward declarations.
void SetList(CefRefPtr<CefV8Value> source, CefRefPtr<CefListValue> target);
void SetList(CefRefPtr<CefListValue> source, CefRefPtr<CefV8Value> target);

// Transfer a V8 value to a List index.
void SetListValue(CefRefPtr<CefListValue> list, int index,
	CefRefPtr<CefV8Value> value) {
	if (value->IsArray()) {
		CefRefPtr<CefListValue> new_list = CefListValue::Create();
		SetList(value, new_list);
		list->SetList(index, new_list);
	}
	else if (value->IsString()) {
		list->SetString(index, value->GetStringValue());
	}
	else if (value->IsBool()) {
		list->SetBool(index, value->GetBoolValue());
	}
	else if (value->IsInt()) {
		list->SetInt(index, value->GetIntValue());
	}
	else if (value->IsDouble()) {
		list->SetDouble(index, value->GetDoubleValue());
	}
}

// Transfer a V8 array to a List.
void SetList(CefRefPtr<CefV8Value> source, CefRefPtr<CefListValue> target) {
	//ASSERT(source->IsArray());

	int arg_length = source->GetArrayLength();
	if (arg_length == 0)
		return;

	// Start with null types in all spaces.
	target->SetSize(arg_length);

	for (int i = 0; i < arg_length; ++i)
		SetListValue(target, i, source->GetValue(i));
}

// Transfer a List value to a V8 array index.
void SetListValue(CefRefPtr<CefV8Value> list, int index,
	CefRefPtr<CefListValue> value) {
	CefRefPtr<CefV8Value> new_value;

	CefValueType type = value->GetType(index);
	switch (type) {
	case VTYPE_LIST: {
		CefRefPtr<CefListValue> list = value->GetList(index);
		new_value = CefV8Value::CreateArray(list->GetSize());
		SetList(list, new_value);
	} break;
	case VTYPE_BOOL:
		new_value = CefV8Value::CreateBool(value->GetBool(index));
		break;
	case VTYPE_DOUBLE:
		new_value = CefV8Value::CreateDouble(value->GetDouble(index));
		break;
	case VTYPE_INT:
		new_value = CefV8Value::CreateInt(value->GetInt(index));
		break;
	case VTYPE_STRING:
		new_value = CefV8Value::CreateString(value->GetString(index));
		break;
	default:
		break;
	}

	if (new_value.get()) {
		list->SetValue(index, new_value);
	}
	else {
		list->SetValue(index, CefV8Value::CreateNull());
	}
}

CefRefPtr<CefV8Value> GetListIndexAsValue(int index, CefRefPtr<CefListValue> value)
{
	CefRefPtr<CefV8Value> new_value = CefV8Value::CreateNull();

	CefValueType type = value->GetType(index);
	switch (type) {
	case VTYPE_LIST: {
		CefRefPtr<CefListValue> list = value->GetList(index);
		new_value = CefV8Value::CreateArray(list->GetSize());
		SetList(list, new_value);
	} break;
	case VTYPE_BOOL:
		new_value = CefV8Value::CreateBool(value->GetBool(index));
		break;
	case VTYPE_DOUBLE:
		new_value = CefV8Value::CreateDouble(value->GetDouble(index));
		break;
	case VTYPE_INT:
		new_value = CefV8Value::CreateInt(value->GetInt(index));
		break;
	case VTYPE_STRING:
		new_value = CefV8Value::CreateString(value->GetString(index));
		break;
	//case VTYPE_BINARY:
	//	new_value = CefV8Value::CreateArrayBuffer 
	//	break;
	default:
		break;
	}

	return new_value;
}

// Transfer a List to a V8 array.
void SetList(CefRefPtr<CefListValue> source, CefRefPtr<CefV8Value> target) {
	//ASSERT(target->IsArray());

	int arg_length = source->GetSize();
	if (arg_length == 0)
		return;

	for (int i = 0; i < arg_length; ++i)
		SetListValue(target, i, source);
}

namespace client {
	namespace renderer {

		namespace {

			// Must match the value in client_handler.cc.
			const char kFocusedNodeChangedMessage[] = "ClientRenderer.FocusedNodeChanged";

			class GenericV8Handler : public CefV8Handler
			{
			public:
				using CallbackMap = std::map<std::pair<std::string, int>, std::pair<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value> > >;
				CallbackMap callback_map_;

				GenericV8Handler() {}

				virtual bool Execute(const CefString& name,
					CefRefPtr<CefV8Value> object,
					const CefV8ValueList& arguments,
					CefRefPtr<CefV8Value>& retval,
					CefString& exception) OVERRIDE
				{
					if (name == "RegisterJS" &&
						arguments.size() == 1 &&
						arguments[0]->IsFunction())
					{
						std::string message_name = "JS_INVOKE";
						CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
						int browser_id = context->GetBrowser()->GetIdentifier();
						callback_map_.insert(std::make_pair(std::make_pair(message_name, browser_id), std::make_pair(context, arguments[0])));
					}				

					// In the CefV8Handler::Execute implementation for “setMessageCallback”.
					if (name == "RegisterJSFunction" &&
						arguments.size() == 2 &&
						arguments[0]->IsString() &&
						arguments[1]->IsFunction())
					{
						std::string message_name = arguments[0]->GetStringValue();
						CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
						int browser_id = context->GetBrowser()->GetIdentifier();
						callback_map_.insert(std::make_pair(std::make_pair(message_name, browser_id), std::make_pair(context, arguments[1])));
					}

					if (name == "CallNativeWithJSON" &&
						arguments.size() == 1 &&
						arguments[0]->IsString())
					{
						CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();

						// Create the message object.
						CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("CallNativeWithJSON");
						// Retrieve the argument list object.
						CefRefPtr<CefListValue> args = msg->GetArgumentList();
						// Populate the argument values.
						args->SetString(0, arguments[0]->GetStringValue());
						// Send the process message to the main frame in the render process.
						// Use PID_BROWSER instead when sending a message to the browser process.
						context->GetFrame()->SendProcessMessage(PID_BROWSER, msg);

						return true;
					}

					return true;
				}

				bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
					CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					CefProcessId source_process,
					CefRefPtr<CefProcessMessage> message)
				{
					// Execute the registered JavaScript callback if any.
					if (!callback_map_.empty())
					{
						const CefString& message_name = message->GetName();
						CallbackMap::const_iterator it = callback_map_.find(std::make_pair(message_name.ToString(), browser->GetIdentifier()));

						if (it != callback_map_.end())
						{
							// Keep a local reference to the objects. The callback may remove itself
							// from the callback map.
							CefRefPtr<CefV8Context> context = it->second.first;
							CefRefPtr<CefV8Value> callback = it->second.second;

							// Enter the context.
							context->Enter();

							CefV8ValueList arguments;

							// Second argument is the list of message arguments.
							CefRefPtr<CefListValue> list = message->GetArgumentList();
							CefRefPtr<CefV8Value> args = CefV8Value::CreateArray(list->GetSize());

							for (int i = 0; i < list->GetSize(); ++i)
								arguments.push_back(GetListIndexAsValue(i, list));

							bool handled = false;
							// Execute the callback.
							CefRefPtr<CefV8Value> retval = callback->ExecuteFunction(NULL, arguments);
							if (retval.get())
							{
								if (retval->IsBool())
									handled = retval->GetBoolValue();
							}

							// Exit the context.
							context->Exit();

							return true;
						}
					}

					return false;
				}

				void OnContextReleased(CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					CefRefPtr<CefV8Context> context) {
					// Remove any JavaScript callbacks registered for the context that has been released.
					if (!callback_map_.empty()) {
						CallbackMap::iterator it = callback_map_.begin();
						for (; it != callback_map_.end();) {
							if (it->second.first->IsSame(context))
								callback_map_.erase(it++);
							else
								++it;
						}
					}
				}

			private:
				IMPLEMENT_REFCOUNTING(GenericV8Handler);
			};

			class ClientRenderDelegate : public ClientAppRenderer::Delegate 
			{
				//DS
			private:
				CefRefPtr<GenericV8Handler> _handler;

			public:
				ClientRenderDelegate() : last_node_is_editable_(false) 
				{
					_handler = new GenericV8Handler();
				}

				void OnWebKitInitialized(CefRefPtr<ClientAppRenderer> app) OVERRIDE {
					if (CefCrashReportingEnabled()) {
						// Set some crash keys for testing purposes. Keys must be defined in the
						// "crash_reporter.cfg" file. See cef_crash_util.h for details.
						CefSetCrashKeyValue("testkey_small1", "value1_small_renderer");
						CefSetCrashKeyValue("testkey_small2", "value2_small_renderer");
						CefSetCrashKeyValue("testkey_medium1", "value1_medium_renderer");
						CefSetCrashKeyValue("testkey_medium2", "value2_medium_renderer");
						CefSetCrashKeyValue("testkey_large1", "value1_large_renderer");
						CefSetCrashKeyValue("testkey_large2", "value2_large_renderer");
					}

					// Create the renderer-side router for query handling.
					CefMessageRouterConfig config;
					message_router_ = CefMessageRouterRendererSide::Create(config);
				}

				void OnContextCreated(CefRefPtr<ClientAppRenderer> app,
					CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					CefRefPtr<CefV8Context> context) OVERRIDE {
					message_router_->OnContextCreated(browser, frame, context);

					//DS 
					CefRefPtr<CefV8Value> object = context->GetGlobal();
					object->SetValue("RegisterJSFunction",
						CefV8Value::CreateFunction("RegisterJSFunction", _handler),
						V8_PROPERTY_ATTRIBUTE_NONE);
					object->SetValue("CallNativeWithJSON",
						CefV8Value::CreateFunction("CallNativeWithJSON", _handler),
						V8_PROPERTY_ATTRIBUTE_NONE);
					object->SetValue("RegisterJS",
						CefV8Value::CreateFunction("RegisterJS", _handler),
						V8_PROPERTY_ATTRIBUTE_NONE);
				}

				void OnContextReleased(CefRefPtr<ClientAppRenderer> app,
					CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					CefRefPtr<CefV8Context> context) OVERRIDE {
					message_router_->OnContextReleased(browser, frame, context);

					//DS 
					_handler->OnContextReleased(browser, frame, context);
				}

				void OnFocusedNodeChanged(CefRefPtr<ClientAppRenderer> app,
					CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					CefRefPtr<CefDOMNode> node) OVERRIDE {
					bool is_editable = (node.get() && node->IsEditable());
					if (is_editable != last_node_is_editable_) {
						// Notify the browser of the change in focused element type.
						last_node_is_editable_ = is_editable;
						CefRefPtr<CefProcessMessage> message =
							CefProcessMessage::Create(kFocusedNodeChangedMessage);
						message->GetArgumentList()->SetBool(0, is_editable);
						frame->SendProcessMessage(PID_BROWSER, message);
					}
				}

				bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
					CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					CefProcessId source_process,
					CefRefPtr<CefProcessMessage> message) OVERRIDE 
				{
					if (message_router_->OnProcessMessageReceived(browser, frame, source_process, message))
					{
						return true;
					}

					//DS 
					return _handler->OnProcessMessageReceived(app, browser, frame, source_process, message);
				}

			private:
				bool last_node_is_editable_;

				// Handles the renderer side of query routing.
				CefRefPtr<CefMessageRouterRendererSide> message_router_;

				DISALLOW_COPY_AND_ASSIGN(ClientRenderDelegate);
				IMPLEMENT_REFCOUNTING(ClientRenderDelegate);
			};

		}  // namespace

		void CreateDelegates(ClientAppRenderer::DelegateSet& delegates) {
			delegates.insert(new ClientRenderDelegate);
		}

	}  // namespace renderer
}  // namespace client
