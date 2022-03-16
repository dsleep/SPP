// SPPEngine.cpp : Defines the entry point for the application.
//

#include <windows.h>
#include <filesystem>

#include "include/base/cef_scoped_ptr.h"
#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "cefclient/browser/main_context_impl.h"
#include "cefclient/browser/main_message_loop_multithreaded_win.h"
#include "cefclient/browser/root_window_manager.h"
#include "cefclient/browser/test_runner.h"

#include "cefclient/browser/root_game_window_win.h"


#include "cefclient/browser/client_handler.h"
#include "shared/browser/client_app_browser.h"
#include "shared/browser/main_message_loop_external_pump.h"
#include "shared/browser/main_message_loop_std.h"
#include "shared/common/client_app_other.h"
#include "shared/common/client_switches.h"
#include "shared/renderer/client_app_renderer.h"

#include "SPPCEFUI.h"

#include "cefclient/JSCallbackInterface.h"

SPP_OVERLOAD_ALLOCATORS

using namespace client;

namespace SPP
{
    struct CEFMessageList::ListImpl
    {
        CefRefPtr<CefListValue> cefList;

        ListImpl()
        {
            cefList = CefListValue::Create();
        }
    };


    CEFMessageList::CEFMessageList() : _impl(new ListImpl())
    {
    }

    CEFMessageList::~CEFMessageList()
    {
    }

    bool CEFMessageList::SetBool(size_t index, bool value)
    {
        return _impl->cefList->SetBool(index, value);
    }
    bool CEFMessageList::SetInt(size_t index, int value)
    {
        return _impl->cefList->SetInt(index, value);
    }
    bool CEFMessageList::SetDouble(size_t index, double value)
    {
        return _impl->cefList->SetDouble(index, value);
    }
    bool CEFMessageList::SetString(size_t index, const std::string& value)
    {
        return _impl->cefList->SetString(index, value);
    }
    bool CEFMessageList::SetList(size_t index, const CEFMessageList& value)
    {        
        return _impl->cefList->SetList(index, value._impl->cefList);
    }


    struct CEFMessage::CEFMessageImpl
    {
        CefRefPtr<CefProcessMessage> msg;
        CefRefPtr<CefListValue> args;

        CEFMessageImpl(const char* MessageName)
        {
            msg = CefProcessMessage::Create(MessageName);
            args = msg->GetArgumentList();
        }
    };

    CEFMessage::CEFMessage(const char* MessageName) : _implMsg(new CEFMessageImpl(MessageName))
    {
        // kinda wasteful it deallocates other but simple
        _impl->cefList = _implMsg->args;
    }

    CEFMessage::~CEFMessage()
    {
    }
    bool CEFMessage::Send()
    {
        if (client::MainContextImpl::Get() &&
            client::MainContextImpl::Get()->GetRootWindowManager() &&
            client::MainContextImpl::Get()->GetRootWindowManager()->GetActiveBrowser() &&
            client::MainContextImpl::Get()->GetRootWindowManager()->GetActiveBrowser()->GetMainFrame())
        {
            client::MainContextImpl::Get()->GetRootWindowManager()->GetActiveBrowser()->GetMainFrame()->SendProcessMessage(PID_RENDERER, _implMsg->msg);
            return true;
        }
        return false;
    }

    int RunBrowser(void* hInstance, 
        const std::string& StartupURL,
        const GameBrowserCallbacks& InCallbacks,
        const InputEvents& InInputEvents,
        std::function<void(const std::string &,Json::Value&) >* JSFunctionReceiver)
    {
        // Enable High-DPI support on Windows 7 or newer.
        CefEnableHighDPISupport();

        CefMainArgs main_args((HINSTANCE)hInstance);

        void* sandbox_info = nullptr;

#if defined(CEF_USE_SANDBOX)
        // Manage the life span of the sandbox information object. This is necessary
        // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
        CefScopedSandboxInfo scoped_sandbox;
        sandbox_info = scoped_sandbox.sandbox_info();
#endif

        // Parse command-line arguments.
        CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
        command_line->InitFromString(::GetCommandLineW());

        // Create a ClientApp of the correct type.
        CefRefPtr<CefApp> app;
        ClientApp::ProcessType process_type = ClientApp::GetProcessType(command_line);
        if (process_type == ClientApp::BrowserProcess)
            app = new ClientAppBrowser();
        else if (process_type == ClientApp::RendererProcess)
            app = new ClientAppRenderer();
        else if (process_type == ClientApp::OtherProcess)
            app = new ClientAppOther();

        // Execute the secondary process, if any.
        int exit_code = CefExecuteProcess(main_args, app, sandbox_info);
        if (exit_code >= 0)
            return exit_code;

        // Create the main context object.
        scoped_ptr<MainContextImpl> context(new MainContextImpl(command_line, true));

        CefSettings settings;
        std::filesystem::path SetupSubProcessPath = std::filesystem::current_path() / "CEFProcess.exe";
        CefString(&settings.browser_subprocess_path).FromString(SetupSubProcessPath.generic_string().c_str());

#if !defined(CEF_USE_SANDBOX)
        settings.no_sandbox = true;
#endif

        // Applications should specify a unique GUID here to enable trusted downloads.
        CefString(&settings.application_client_id_for_file_scanning)
            .FromString("9A8DE24D-B822-4C6C-8259-5A848FEA1E68");

        // Populate the settings based on command line arguments.
        context->PopulateSettings(&settings);

        //DS
        settings.external_message_pump = true;


        // Create the main message loop object.
        scoped_ptr<MainMessageLoop> message_loop;
        if (settings.multi_threaded_message_loop)
            message_loop.reset(new MainMessageLoopMultithreadedWin);
        else if (settings.external_message_pump)
            message_loop = MainMessageLoopExternalPump::Create(InCallbacks.Update, InInputEvents.keyDown, InInputEvents.keyUp);
        else
            message_loop.reset(new MainMessageLoopStd);

        // Initialize CEF.
        context->Initialize(main_args, settings, app, sandbox_info);

        // Register scheme handlers.
        test_runner::RegisterSchemeHandlers();

        RootWindowConfig window_config;
        window_config.always_on_top = false;// command_line->HasSwitch(switches::kAlwaysOnTop);
        window_config.with_controls = false;//!command_line->HasSwitch(switches::kHideControls);
        window_config.with_osr = settings.windowless_rendering_enabled ? true : false;
        window_config.url = StartupURL;

        window_config.bounds.width = 512;
        window_config.bounds.height = 512;

        // Create the first window.
        context->GetRootWindowManager()->CreateRootWindow(window_config);

        ClientHandler* client_handler = nullptr;

#if USING_GAME_WINDOW
        RootGameWindowWin* mainGameWindow = nullptr;
#endif
        {
            auto RootManager = context->GetRootWindowManager();
            auto RootWindow = RootManager->GetActiveRootWindow();
            auto client = RootManager->GetActiveBrowser()->GetHost()->GetClient();
            client_handler = static_cast<ClientHandler*>(client.get());
#if USING_GAME_WINDOW
            mainGameWindow = static_cast<RootGameWindowWin*>(RootWindow.get());
#endif
        }

#if USING_GAME_WINDOW
        if (InCallbacks.Initialized)
        {
            InCallbacks.Initialized(mainGameWindow->GetHWND());
        }   

        mainGameWindow->SetMouseMove(InInputEvents.mouseMove);
        mainGameWindow->SetMouseDown(InInputEvents.mouseDown);
        mainGameWindow->SetMouseUp(InInputEvents.mouseUp);
        
        std::unique_ptr<JavascriptInterface>  localInterface;

        // interface map
        if (JSFunctionReceiver)
        {
            localInterface = std::make_unique< JavascriptInterface >(*JSFunctionReceiver);
        }
        else
        {
            localInterface = std::make_unique< JavascriptInterface >();
        }

        localInterface->Add_JSToNativeFunctionHandler("UpdatedGameViewRegion", [mainGameWindow](Json::Value InArguments)
            {
                if (InArguments.isNull() == false)
                {
                    SE_ASSERT(InArguments.size() == 4);

                    int32_t rect[4];
                    for (int32_t Iter = 0; Iter < 4; Iter++)
                    {
                        rect[Iter] = InArguments[Iter].asInt();
                    }

                    mainGameWindow->OnGameRegionWindowSet(rect);
                }
            });

        client_handler->_JSONNativeFunc = std::bind(&JavascriptInterface::NativeFromJS_JSON_Callback, localInterface.get(), std::placeholders::_1);               
#endif

        // Run the message loop. This will block until Quit() is called by the
        // RootWindowManager after all windows have been destroyed.
        int result = message_loop->Run();

        if (InCallbacks.Shutdown)
        {
            InCallbacks.Shutdown();
        }

        // Shut down CEF.
        context->Shutdown();

        // Release objects in reverse order of creation.
        message_loop.reset();
        context.reset();

        return result;
	}
}
