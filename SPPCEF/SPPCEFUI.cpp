// SPPEngine.cpp : Defines the entry point for the application.
//

#include <windows.h>
#include <filesystem>

#include "include/base/cef_scoped_ptr.h"
#include "include/base/cef_bind.h"

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

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

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

    struct ColoRGBA
    {       
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };

    class OffScreenHandler : public CefClient,
        public CefDisplayHandler,
        public CefLifeSpanHandler,
        public CefLoadHandler,
        public CefRenderHandler
    {
    private:
        int desiredWidth = 1280;
        int desiredHeight = 720;
        std::vector< ColoRGBA > ColorData;
        std::function< void(const std::string&) > _JSONNativeFunc;

        std::function< void(int,int,int,int,const void*,int,int) > _OnPaint;

    public:
        OffScreenHandler()
        {

        }
        ~OffScreenHandler()
        {

        }


        void SetJSONToNativeFunc( const std::function< void(const std::string&) > &InJSONNativeFunc )
        {
            _JSONNativeFunc = InJSONNativeFunc;
        }

        void SetPaintFunction(const std::function< void(int, int, int, int, const void*, int, int) >& InOnPaint)
        {
            _OnPaint = InOnPaint;
        }

        CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE { return this; }

        // CefClient methods:
        virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE {
            return this;
        }
        virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
            return this;
        }
        virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE { return this; }

        // CefDisplayHandler methods:
        virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
            const CefString& title) OVERRIDE
        {
            CEF_REQUIRE_UI_THREAD();
            // Set the title of the window using platform APIs.
            PlatformTitleChange(browser, title);
        }

        // CefLifeSpanHandler methods:
        virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE
        {
            CEF_REQUIRE_UI_THREAD();

            // Add to the list of existing browsers.
            browser_list_.push_back(browser);
        }

        virtual bool DoClose(CefRefPtr<CefBrowser> browser) OVERRIDE
        {
            CEF_REQUIRE_UI_THREAD();

            // Closing the main window requires special handling. See the DoClose()
            // documentation in the CEF header for a detailed destription of this
            // process.
            if (browser_list_.size() == 1) {
                // Set a flag to indicate that the window close should be allowed.
                is_closing_ = true;
            }

            // Allow the close. For windowed browsers this will result in the OS close
            // event being sent.
            return false;
        }
        virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE
        {
            CEF_REQUIRE_UI_THREAD();

            // Remove from the list of existing browsers.
            BrowserList::iterator bit = browser_list_.begin();
            for (; bit != browser_list_.end(); ++bit) {
                if ((*bit)->IsSame(browser)) {
                    browser_list_.erase(bit);
                    break;
                }
            }

            if (browser_list_.empty()) {
                // All browser windows have closed. Quit the application message loop.
                CefQuitMessageLoop();
            }
        }

        // CefLoadHandler methods:
        virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            ErrorCode errorCode,
            const CefString& errorText,
            const CefString& failedUrl) OVERRIDE
        {
            CEF_REQUIRE_UI_THREAD();
                       
            // Don't display an error for downloaded files.
            if (errorCode == ERR_ABORTED)
                return;

            // Display a load error message using a data: URI.
            std::stringstream ss;
            ss << "<html><body bgcolor=\"white\">"
                "<h2>Failed to load URL "
                << std::string(failedUrl) << " with error " << std::string(errorText)
                << " (" << errorCode << ").</h2></body></html>";

            //what to do here?
            //frame->LoadURL(GetDataURI(ss.str(), "text/html"));
        }

        // Request that all existing browser windows close.
        void CloseAllBrowsers(bool force_close)
        {
            if (!CefCurrentlyOn(TID_UI)) {
                // Execute on the UI thread.
                CefPostTask(TID_UI, base::Bind(&OffScreenHandler::CloseAllBrowsers, this, force_close));
                return;
            }

            if (browser_list_.empty())
                return;

            BrowserList::const_iterator it = browser_list_.begin();
            for (; it != browser_list_.end(); ++it)
                (*it)->GetHost()->CloseBrowser(force_close);
        }

        bool IsClosing() const { return is_closing_; }

        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefProcessId source_process,
            CefRefPtr<CefProcessMessage> message) OVERRIDE
        {
            // Check for messages from the client renderer.
            std::string message_name = message->GetName();

            if (message_name == "CallNativeWithJSON")
            {
                auto ArgumentList = message->GetArgumentList();

                if (ArgumentList->GetSize() == 1)
                {
                    std::string JSONMessage = message->GetArgumentList()->GetString(0).ToString();
                    if (_JSONNativeFunc)
                    {
                        _JSONNativeFunc(JSONMessage);
                    }
                }

                return true;
            }

            return false;
        }

        //RENDERING

        bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) OVERRIDE
        {
            CEF_REQUIRE_UI_THREAD();
            return false;
        }

        void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) OVERRIDE
        {
            rect.x = rect.y = 0;
            rect.width = desiredWidth;
            rect.height = desiredHeight;
        }
        bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
            int viewX,
            int viewY,
            int& screenX,
            int& screenY) OVERRIDE
        {            
            screenX = viewX;
            screenY = viewY;
            return true;
        }
        bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) OVERRIDE
        {
            CefRect view_rect;
            GetViewRect(browser, view_rect);

            screen_info.device_scale_factor = 1.0f;

            // The screen info rectangles are used by the renderer to create and position
            // popups. Keep popups inside the view rectangle.
            screen_info.rect = view_rect;
            screen_info.available_rect = view_rect;
            return true;
        }
        void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) OVERRIDE
        {

        }
        void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) OVERRIDE
        {

        }
        void OnPaint(CefRefPtr<CefBrowser> browser,
            CefRenderHandler::PaintElementType type,
            const CefRenderHandler::RectList& dirtyRects,
            const void* buffer,
            int width,
            int height) OVERRIDE
        {   
            ColorData.resize(width * height);
            
            for (auto Iter = dirtyRects.begin(); Iter != dirtyRects.end(); Iter++)
            {
                const CefRect& rect = *Iter;

                
                _OnPaint(rect.x, rect.y, rect.width, rect.height, buffer, width, height);


            }
        }

        bool IsKeyDown(WPARAM wparam) {
            return (GetKeyState(wparam) & 0x8000) != 0;
        }

        int GetCefMouseModifiers(WPARAM wparam)
        {
            int modifiers = 0;
            if (wparam & MK_CONTROL)
                modifiers |= EVENTFLAG_CONTROL_DOWN;
            if (wparam & MK_SHIFT)
                modifiers |= EVENTFLAG_SHIFT_DOWN;
            if (IsKeyDown(VK_MENU))
                modifiers |= EVENTFLAG_ALT_DOWN;
            if (wparam & MK_LBUTTON)
                modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
            if (wparam & MK_MBUTTON)
                modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
            if (wparam & MK_RBUTTON)
                modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

            // Low bit set from GetKeyState indicates "toggled".
            if (::GetKeyState(VK_NUMLOCK) & 1)
                modifiers |= EVENTFLAG_NUM_LOCK_ON;
            if (::GetKeyState(VK_CAPITAL) & 1)
                modifiers |= EVENTFLAG_CAPS_LOCK_ON;
            return modifiers;
        }

        void MouseEvent()
        {
            if (browser_list_.empty())
                return;

            auto &topBrowser = browser_list_.front();
            auto browser_host = topBrowser->GetHost();

            //browser_host->WasResized();

            //CefMouseEvent mouse_event;
            //mouse_event.x = x;
            //mouse_event.y = y;
            //mouse_event.modifiers = GetCefMouseModifiers(wParam);
            //browser_host->SendMouseClickEvent(mouse_event, btnType, false, last_click_count_);

            //CefMouseEvent mouse_event;
            //mouse_event.x = x;
            //mouse_event.y = y;
            //mouse_event.modifiers = GetCefMouseModifiers(wParam);
            //browser_host->SendMouseMoveEvent(mouse_event, false);

            //CefMouseEvent mouse_event;
            //mouse_event.x = screen_point.x;
            //mouse_event.y = screen_point.y;
            //mouse_event.modifiers = GetCefMouseModifiers(wParam);
            //browser_host->SendMouseWheelEvent(mouse_event,
            //    IsKeyDown(VK_SHIFT) ? delta : 0,
            //    !IsKeyDown(VK_SHIFT) ? delta : 0);
        }


    private:
        // Platform-specific implementation.
        void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
            const CefString& title)
        {}

        // List of existing browser windows. Only accessed on the CEF UI thread.
        typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
        BrowserList browser_list_;

        bool is_closing_;

        // Include the default reference counting implementation.
        IMPLEMENT_REFCOUNTING(OffScreenHandler);
    };

    int RunOffscreenBrowser(void* hInstance,
        const std::string& StartupURL,
        const OSRBrowserCallbacks& InCallbacks,
        const InputEvents& InInputEvents,
        std::function<void(const std::string&, Json::Value&) >* JSFunctionReceiver)
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
        settings.windowless_rendering_enabled = true;

        // Initialize CEF.
        context->Initialize(main_args, settings, app, sandbox_info);

        CefWindowInfo window_info;
        window_info.SetAsWindowless(nullptr);
        window_info.x = 0;
        window_info.y = 0;
        window_info.width = 512;
        window_info.height = 512;
        window_info.shared_texture_enabled = false;
        window_info.external_begin_frame_enabled = false;

        CefRefPtr<OffScreenHandler> handler = new OffScreenHandler();
        CefBrowserSettings browser_settings;
        MainContext::Get()->PopulateBrowserSettings(&browser_settings);

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

        handler->SetJSONToNativeFunc(std::bind(&JavascriptInterface::NativeFromJS_JSON_Callback, localInterface.get(), std::placeholders::_1));
        handler->SetPaintFunction(InCallbacks.OnPaint);

        // Create the browser asynchronously.
        CefBrowserHost::CreateBrowser(window_info, handler, StartupURL, browser_settings, nullptr, nullptr);
        
        while (true)
        {
            for(int32_t Iter = 0; Iter < 10; Iter++)
            CefDoMessageLoopWork();
            std::this_thread::sleep_for(10ms);
        }

        //if (InCallbacks.Shutdown)
        //{
        //    InCallbacks.Shutdown();
        //}

        // Shut down CEF.
        context->Shutdown();

        // Release objects in reverse order of creation.
        context.reset();

        return 0;
    }

    int RunBrowser(void* hInstance, 
        const std::string& StartupURL,
        const GameBrowserCallbacks& InCallbacks,
        const InputEvents& InInputEvents,
        bool bWithGameWindow,
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

        window_config.with_game_window = bWithGameWindow;

        // Create the first window.
        context->GetRootWindowManager()->CreateRootWindow(window_config);

        ClientHandler* client_handler = nullptr;
        RootGameWindowWin* mainGameWindow = nullptr;

        {
            auto RootManager = context->GetRootWindowManager();
            auto RootWindow = RootManager->GetActiveRootWindow();
            auto client = RootManager->GetActiveBrowser()->GetHost()->GetClient();
            client_handler = static_cast<ClientHandler*>(client.get());

            if (bWithGameWindow)
            {
                mainGameWindow = static_cast<RootGameWindowWin*>(RootWindow.get());
            }
        }

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

        if (bWithGameWindow)
        {
            if (InCallbacks.Initialized)
            {
                InCallbacks.Initialized(mainGameWindow->GetHWND());
            }

            mainGameWindow->SetMouseMove(InInputEvents.mouseMove);
            mainGameWindow->SetMouseDown(InInputEvents.mouseDown);
            mainGameWindow->SetMouseUp(InInputEvents.mouseUp);

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
        }

        client_handler->_JSONNativeFunc = std::bind(&JavascriptInterface::NativeFromJS_JSON_Callback, localInterface.get(), std::placeholders::_1);

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
