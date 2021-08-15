
#include "cefclient/browser/root_game_window_win.h"

#include <shellscalingapi.h>

#include "include/base/cef_bind.h"
#include "include/base/cef_build.h"
#include "include/cef_app.h"
#include "cefclient/browser/browser_window_osr_win.h"
#include "cefclient/browser/browser_window_std_win.h"
#include "cefclient/browser/main_context.h"
#include "cefclient/browser/resource.h"
#include "cefclient/browser/temp_window.h"
#include "cefclient/browser/window_test_runner_win.h"
#include "shared/browser/geometry_util.h"
#include "shared/browser/main_message_loop.h"
#include "shared/browser/util_win.h"
#include "shared/common/client_switches.h"

#define MAX_URL_LENGTH 255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT 24

namespace client {

	namespace 
	{

		// Returns true if the process is per monitor DPI aware.
		bool IsProcessPerMonitorDpiAware() {
			enum class PerMonitorDpiAware {
				UNKNOWN = 0,
				PER_MONITOR_DPI_UNAWARE,
				PER_MONITOR_DPI_AWARE,
			};
			static PerMonitorDpiAware per_monitor_dpi_aware = PerMonitorDpiAware::UNKNOWN;
			if (per_monitor_dpi_aware == PerMonitorDpiAware::UNKNOWN) {
				per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_UNAWARE;
				HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
				if (shcore_dll) {
					typedef HRESULT(WINAPI* GetProcessDpiAwarenessPtr)(
						HANDLE, PROCESS_DPI_AWARENESS*);
					GetProcessDpiAwarenessPtr func_ptr =
						reinterpret_cast<GetProcessDpiAwarenessPtr>(
							::GetProcAddress(shcore_dll, "GetProcessDpiAwareness"));
					if (func_ptr) {
						PROCESS_DPI_AWARENESS awareness;
						if (SUCCEEDED(func_ptr(nullptr, &awareness)) &&
							awareness == PROCESS_PER_MONITOR_DPI_AWARE)
							per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
					}
				}
			}
			return per_monitor_dpi_aware == PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
		}

		// DPI value for 1x scale factor.
#define DPI_1X 96.0f

		float GetWindowScaleFactor(HWND hwnd) {
			if (hwnd && IsProcessPerMonitorDpiAware()) {
				typedef UINT(WINAPI* GetDpiForWindowPtr)(HWND);
				static GetDpiForWindowPtr func_ptr = reinterpret_cast<GetDpiForWindowPtr>(
					GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForWindow"));
				if (func_ptr)
					return static_cast<float>(func_ptr(hwnd)) / DPI_1X;
			}

			return client::GetDeviceScaleFactor();
		}

	}  // namespace

	RootGameWindowWin::RootGameWindowWin()
		: with_controls_(false),
		always_on_top_(false),
		with_osr_(false),
		with_extension_(false),
		is_popup_(false),
		start_rect_(),
		_gameWidowRect(),
		initialized_(false),
		hwnd_(NULL),
		_gameWindow(NULL),
		window_destroyed_(false),
		browser_destroyed_(false),
		called_enable_non_client_dpi_scaling_(false) 
	{

	}

	RootGameWindowWin::~RootGameWindowWin() 
	{
		REQUIRE_MAIN_THREAD();

		// The window and browser should already have been destroyed.
		DCHECK(window_destroyed_);
		DCHECK(browser_destroyed_);
	}

	void RootGameWindowWin::Init(RootWindow::Delegate* delegate,
		const RootWindowConfig& config,
		const CefBrowserSettings& settings) {
		DCHECK(delegate);
		DCHECK(!initialized_);

		delegate_ = delegate;
		with_controls_ = config.with_controls;
		always_on_top_ = config.always_on_top;
		with_osr_ = config.with_osr;
		with_extension_ = config.with_extension;

		start_rect_.left = config.bounds.x;
		start_rect_.top = config.bounds.y;
		start_rect_.right = config.bounds.x + config.bounds.width;
		start_rect_.bottom = config.bounds.y + config.bounds.height;
		
		_gameWidowRect = { 0,0,100,100 };

		CreateBrowserWindow(config.url);

		initialized_ = true;

		// Create the native root window on the main thread.
		if (CURRENTLY_ON_MAIN_THREAD()) {
			CreateRootWindow(settings, config.initially_hidden);
		}
		else {
			MAIN_POST_CLOSURE(base::Bind(&RootGameWindowWin::CreateRootWindow, this,
				settings, config.initially_hidden));
		}
	}

	void RootGameWindowWin::InitAsPopup(RootWindow::Delegate* delegate,
		bool with_controls,
		bool with_osr,
		const CefPopupFeatures& popupFeatures,
		CefWindowInfo& windowInfo,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& settings)
	{
		CEF_REQUIRE_UI_THREAD();

		DCHECK(delegate);
		DCHECK(!initialized_);

		delegate_ = delegate;
		with_controls_ = with_controls;
		with_osr_ = with_osr;
		is_popup_ = true;

		if (popupFeatures.xSet)
			start_rect_.left = popupFeatures.x;
		if (popupFeatures.ySet)
			start_rect_.top = popupFeatures.y;
		if (popupFeatures.widthSet)
			start_rect_.right = start_rect_.left + popupFeatures.width;
		if (popupFeatures.heightSet)
			start_rect_.bottom = start_rect_.top + popupFeatures.height;

		CreateBrowserWindow(std::string());

		initialized_ = true;

		// The new popup is initially parented to a temporary window. The native root
		// window will be created after the browser is created and the popup window
		// will be re-parented to it at that time.
		browser_window_->GetPopupConfig(TempWindow::GetWindowHandle(), windowInfo,
			client, settings);
	}

	void RootGameWindowWin::Show(ShowMode mode)
	{
		REQUIRE_MAIN_THREAD();

		if (!hwnd_)
			return;

		int nCmdShow = SW_SHOWNORMAL;
		switch (mode) {
		case ShowMinimized:
			nCmdShow = SW_SHOWMINIMIZED;
			break;
		case ShowMaximized:
			nCmdShow = SW_SHOWMAXIMIZED;
			break;
		case ShowNoActivate:
			nCmdShow = SW_SHOWNOACTIVATE;
			break;
		default:
			break;
		}

		ShowWindow(hwnd_, nCmdShow);
		UpdateWindow(hwnd_);
	}

	void RootGameWindowWin::Hide()
	{
		REQUIRE_MAIN_THREAD();

		if (hwnd_)
			ShowWindow(hwnd_, SW_HIDE);
	}

	void RootGameWindowWin::SetBounds(int x, int y, size_t width, size_t height)
	{
		REQUIRE_MAIN_THREAD();
		if (hwnd_) 
		{
			SetWindowPos(hwnd_, NULL, x, y, static_cast<int>(width), static_cast<int>(height), SWP_NOZORDER);
		}
	}

	void RootGameWindowWin::Close(bool force)
	{
		REQUIRE_MAIN_THREAD();

		if (hwnd_)
		{
			if (force) DestroyWindow(hwnd_);
			else PostMessage(hwnd_, WM_CLOSE, 0, 0);
		}
	}

	void RootGameWindowWin::SetDeviceScaleFactor(float device_scale_factor) {
		REQUIRE_MAIN_THREAD();

		if (browser_window_ && with_osr_)
			browser_window_->SetDeviceScaleFactor(device_scale_factor);
	}

	float RootGameWindowWin::GetDeviceScaleFactor() const {
		REQUIRE_MAIN_THREAD();

		if (browser_window_ && with_osr_)
			return browser_window_->GetDeviceScaleFactor();

		NOTREACHED();
		return 0.0f;
	}

	CefRefPtr<CefBrowser> RootGameWindowWin::GetBrowser() const {
		REQUIRE_MAIN_THREAD();

		if (browser_window_)
			return browser_window_->GetBrowser();
		return nullptr;
	}

	ClientWindowHandle RootGameWindowWin::GetWindowHandle() const {
		REQUIRE_MAIN_THREAD();
		return hwnd_;
	}

	bool RootGameWindowWin::WithWindowlessRendering() const {
		REQUIRE_MAIN_THREAD();
		return with_osr_;
	}

	bool RootGameWindowWin::WithExtension() const {
		REQUIRE_MAIN_THREAD();
		return with_extension_;
	}

	void RootGameWindowWin::CreateBrowserWindow(const std::string& startup_url) 
	{
		if (with_osr_) {
			OsrRendererSettings settings = {};
			MainContext::Get()->PopulateOsrSettings(&settings);
			browser_window_.reset(new BrowserWindowOsrWin(this, startup_url, settings));
		}
		else {
			browser_window_.reset(new BrowserWindowStdWin(this, startup_url));
		}
	}

	void RootGameWindowWin::CreateRootWindow(const CefBrowserSettings& settings, bool initially_hidden) 
	{
		REQUIRE_MAIN_THREAD();
		DCHECK(!hwnd_);

		HINSTANCE hInstance = GetModuleHandle(NULL);

		// Load strings from the resource file.
		const std::wstring& window_title = GetResourceString(IDS_APP_TITLE);
		const std::wstring& window_class = GetResourceString(IDC_CEFCLIENT);

		const cef_color_t background_color = MainContext::Get()->GetBackgroundColor();
		const HBRUSH background_brush = CreateSolidBrush(
			RGB(CefColorGetR(background_color), CefColorGetG(background_color),
				CefColorGetB(background_color)));

		// Register the window class.
		RegisterRootClass(hInstance, window_class, background_brush);

		CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
		const bool no_activate = command_line->HasSwitch(switches::kNoActivate);

		const DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
		DWORD dwExStyle = always_on_top_ ? WS_EX_TOPMOST : 0;
		if (no_activate) {
			// Don't activate the browser window on creation.
			dwExStyle |= WS_EX_NOACTIVATE;
		}

		int x, y, width, height;
		if (::IsRectEmpty(&start_rect_)) {
			// Use the default window position/size.
			x = y = width = height = CW_USEDEFAULT;
		}
		else {
			// Adjust the window size to account for window frame and controls.
			RECT window_rect = start_rect_;
			::AdjustWindowRectEx(&window_rect, dwStyle, with_controls_, dwExStyle);

			x = start_rect_.left;
			y = start_rect_.top;
			width = window_rect.right - window_rect.left;
			height = window_rect.bottom - window_rect.top;
		}

		browser_settings_ = settings;

		// Create the main window initially hidden.
		CreateWindowEx(dwExStyle, window_class.c_str(), window_title.c_str(), dwStyle,
			x, y, width, height, NULL, NULL, hInstance, this);
		CHECK(hwnd_);

		::SetFocus(hwnd_);

		if (!called_enable_non_client_dpi_scaling_ && IsProcessPerMonitorDpiAware()) {
			// This call gets Windows to scale the non-client area when WM_DPICHANGED
			// is fired on Windows versions < 10.0.14393.0.
			// Derived signature; not available in headers.
			typedef LRESULT(WINAPI* EnableChildWindowDpiMessagePtr)(HWND, BOOL);
			static EnableChildWindowDpiMessagePtr func_ptr =
				reinterpret_cast<EnableChildWindowDpiMessagePtr>(GetProcAddress(
					GetModuleHandle(L"user32.dll"), "EnableChildWindowDpiMessage"));
			if (func_ptr)
				func_ptr(hwnd_, TRUE);
		}

		if (!initially_hidden) {
			// Show this window.
			Show(no_activate ? ShowNoActivate : ShowNormal);
		}
	}

	static LRESULT CALLBACK GameWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		REQUIRE_MAIN_THREAD();

		RootGameWindowWin* self = nullptr;
		if (message != WM_NCCREATE) 
		{
			self = GetUserDataPtr<RootGameWindowWin*>(hWnd);
			if (!self)
			{
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		
		static POINT originalCursorPt = {};

		switch (message)
		{
		case WM_RBUTTONDOWN:
			SetCapture(hWnd);
			GetCursorPos(&originalCursorPt);
			ShowCursor(false);
			break;

		case WM_RBUTTONUP:
			ReleaseCapture();
			ShowCursor(true);
			break;

		case WM_MOUSEMOVE:
		{
			//GetCursorPos(&curpoint);
			if (wParam == MK_RBUTTON)
			{
				POINT currentCursorPt = { 0,0 };
				GetCursorPos(&currentCursorPt);

				SetCursorPos(originalCursorPt.x, originalCursorPt.y);

				if (self->_mouseMove)
				{
					self->_mouseMove(currentCursorPt.x - originalCursorPt.x, currentCursorPt.y - originalCursorPt.y);
				}			
			}
		}
		break;

		case WM_NCCREATE: {
			CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
			self = reinterpret_cast<RootGameWindowWin*>(cs->lpCreateParams);
			DCHECK(self);
			// Associate |self| with the main window.
			SetUserDataPtr(hWnd, self);
		} break;

		case WM_NCDESTROY:
			// Clear the reference to |self|.
			SetUserDataPtr(hWnd, NULL);
			break;
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	// static
	void RootGameWindowWin::RegisterRootClass(HINSTANCE hInstance,
		const std::wstring& window_class,
		HBRUSH background_brush) {
		// Only register the class one time.
		static bool class_registered = false;
		if (class_registered)
			return;
		class_registered = true;

		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style = CS_OWNDC;// CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = RootWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CEFCLIENT));
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = background_brush;
		wcex.lpszMenuName = MAKEINTRESOURCE(IDC_CEFCLIENT);
		wcex.lpszClassName = window_class.c_str();
		wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		RegisterClassEx(&wcex);


		{
			// Initialize the window class.
			WNDCLASSEX windowClass = { 0 };
			windowClass.cbSize = sizeof(WNDCLASSEX);
			windowClass.style = CS_HREDRAW | CS_VREDRAW;
			windowClass.lpfnWndProc = GameWndProc;
			windowClass.hInstance = (HINSTANCE)hInstance;
			windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
			windowClass.lpszClassName = L"GAMEWINDOW";
			RegisterClassEx(&windowClass);
		}
	}

	// static
	LRESULT CALLBACK RootGameWindowWin::RootWndProc(HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam) 
	{
		REQUIRE_MAIN_THREAD();

		RootGameWindowWin* self = nullptr;
		if (message != WM_NCCREATE) {
			self = GetUserDataPtr<RootGameWindowWin*>(hWnd);
			if (!self)
				return DefWindowProc(hWnd, message, wParam, lParam);
			DCHECK_EQ(hWnd, self->hwnd_);
		}

		// Callback for the main window
		switch (message) {
		case WM_COMMAND:
			if (self->OnCommand(LOWORD(wParam)))
				return 0;
			break;

		case WM_GETOBJECT: {
			// Only the lower 32 bits of lParam are valid when checking the object id
			// because it sometimes gets sign-extended incorrectly (but not always).
			DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(lParam));

			// Accessibility readers will send an OBJID_CLIENT message.
			if (static_cast<DWORD>(OBJID_CLIENT) == obj_id) {
				if (self->GetBrowser() && self->GetBrowser()->GetHost())
					self->GetBrowser()->GetHost()->SetAccessibilityState(STATE_ENABLED);
			}
		} break;

		case WM_PAINT:
			self->OnPaint();
			return 0;

		case WM_ACTIVATE:
			self->OnActivate(LOWORD(wParam) != WA_INACTIVE);
			// Allow DefWindowProc to set keyboard focus.
			break;

		case WM_SETFOCUS:
			self->OnFocus();
			return 0;

		case WM_SIZE:
			self->OnSize(wParam == SIZE_MINIMIZED);
			break;

		case WM_MOVING:
		case WM_MOVE:
			self->OnMove();
			return 0;

		case WM_DPICHANGED:
			self->OnDpiChanged(wParam, lParam);
			break;

		case WM_ERASEBKGND:
			if (self->OnEraseBkgnd())
				break;
			// Don't erase the background.
			return 0;

		case WM_ENTERMENULOOP:
			if (!wParam) {
				// Entering the menu loop for the application menu.
				CefSetOSModalLoop(true);
			}
			break;

		case WM_EXITMENULOOP:
			if (!wParam) {
				// Exiting the menu loop for the application menu.
				CefSetOSModalLoop(false);
			}
			break;

		case WM_CLOSE:
			if (self->OnClose())
				return 0;  // Cancel the close.
			break;

		case WM_NCHITTEST: {
			LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
			if (hit == HTCLIENT)
			{
				POINTS points = MAKEPOINTS(lParam);
				POINT point = { points.x, points.y };
				::ScreenToClient(hWnd, &point);
				//if (::PtInRegion(self->draggable_region_, point.x, point.y)) {
				//	// If cursor is inside a draggable region return HTCAPTION to allow
				//	// dragging.
				//	return HTCAPTION;
				//}
			}
			return hit;
		}

		case WM_NCCREATE: {
			CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
			self = reinterpret_cast<RootGameWindowWin*>(cs->lpCreateParams);
			DCHECK(self);
			// Associate |self| with the main window.
			SetUserDataPtr(hWnd, self);
			self->hwnd_ = hWnd;

			self->OnNCCreate(cs);
		} break;

		case WM_CREATE:
			self->OnCreate(reinterpret_cast<CREATESTRUCT*>(lParam));
			break;

		case WM_NCDESTROY:
			// Clear the reference to |self|.
			SetUserDataPtr(hWnd, NULL);
			self->hwnd_ = NULL;
			self->OnDestroyed();
			break;
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	void RootGameWindowWin::OnPaint() {
		PAINTSTRUCT ps;
		BeginPaint(hwnd_, &ps);
		EndPaint(hwnd_, &ps);
	}

	void RootGameWindowWin::OnFocus() {
		// Selecting "Close window" from the task bar menu may send a focus
		// notification even though the window is currently disabled (e.g. while a
		// modal JS dialog is displayed).
		if (browser_window_ && ::IsWindowEnabled(hwnd_))
			browser_window_->SetFocus(true);
	}

	void RootGameWindowWin::OnActivate(bool active) {
		if (active)
			delegate_->OnRootWindowActivated(this);
	}


	void RootGameWindowWin::OnGameRegionWindowSet(int32_t rect[4])
	{
		_gameWidowRect.left = rect[0];
		_gameWidowRect.right = rect[1];
		_gameWidowRect.top = rect[2];
		_gameWidowRect.bottom = rect[3];
				
		SendMessage(hwnd_, WM_SIZE, 0, 0);
	}

	int FindExtraWindowHeight(HWND h)
	{
		RECT w, c;
		GetWindowRect(h, &w);
		GetClientRect(h, &c);
		return (w.bottom - w.top) - (c.bottom - c.top);
	}

	void RootGameWindowWin::OnSize(bool minimized)
	{
		if (minimized) {
			// Notify the browser window that it was hidden and do nothing further.
			if (browser_window_)
				browser_window_->Hide();
			return;
		}

		if (browser_window_)
			browser_window_->Show();

		auto shiftHeight = 20;// FindExtraWindowHeight(hwnd_);

		RECT rect;
		GetClientRect(hwnd_, &rect);

		// |browser_hwnd| may be NULL if the browser has not yet been created.
		HWND browser_hwnd = NULL;
		if (browser_window_)
			browser_hwnd = browser_window_->GetWindowHandle();


		const float device_scale_factor = GetWindowScaleFactor(_gameWindow);

		// Resize all controls.
		HDWP hdwp = BeginDeferWindowPos(browser_hwnd ? 2 : 1);
		
		hdwp = DeferWindowPos(hdwp, _gameWindow, NULL, 
			_gameWidowRect.left * device_scale_factor, 
			(_gameWidowRect.top - shiftHeight) * device_scale_factor,
			(_gameWidowRect.right - _gameWidowRect.left) * device_scale_factor,
			(_gameWidowRect.bottom - _gameWidowRect.top - shiftHeight) * device_scale_factor,
			SWP_NOZORDER);

		if (browser_hwnd)
		{
			hdwp = DeferWindowPos(hdwp, browser_hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
		}

		BOOL result = EndDeferWindowPos(hdwp);
		ALLOW_UNUSED_LOCAL(result);
		DCHECK(result);
	}

	void RootGameWindowWin::OnMove()
	{
		// Notify the browser of move events so that popup windows are displayed
		// in the correct location and dismissed when the window moves.
		CefRefPtr<CefBrowser> browser = GetBrowser();
		if (browser)
			browser->GetHost()->NotifyMoveOrResizeStarted();
	}

	void RootGameWindowWin::OnDpiChanged(WPARAM wParam, LPARAM lParam)
	{
		if (LOWORD(wParam) != HIWORD(wParam)) {
			NOTIMPLEMENTED() << "Received non-square scaling factors";
			return;
		}

		if (browser_window_ && with_osr_) 
		{
			// Scale factor for the new display.
			const float display_scale_factor = static_cast<float>(LOWORD(wParam)) / DPI_1X;
			browser_window_->SetDeviceScaleFactor(display_scale_factor);
		}

		// Suggested size and position of the current window scaled for the new DPI.
		const RECT* rect = reinterpret_cast<RECT*>(lParam);
		SetBounds(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
	}

	bool RootGameWindowWin::OnEraseBkgnd() {
		// Erase the background when the browser does not exist.
		return (GetBrowser() == nullptr);
	}

	bool RootGameWindowWin::OnCommand(UINT id) {
		if (id >= ID_TESTS_FIRST && id <= ID_TESTS_LAST) {
			delegate_->OnTest(this, id);
			return true;
		}

		switch (id) {
		case IDM_EXIT:
			delegate_->OnExit(this);
			return true;		
		}

		return false;
	}
	
	void RootGameWindowWin::OnNCCreate(LPCREATESTRUCT lpCreateStruct) {
		if (IsProcessPerMonitorDpiAware()) {
			// This call gets Windows to scale the non-client area when WM_DPICHANGED
			// is fired on Windows versions >= 10.0.14393.0.
			typedef BOOL(WINAPI* EnableNonClientDpiScalingPtr)(HWND);
			static EnableNonClientDpiScalingPtr func_ptr =
				reinterpret_cast<EnableNonClientDpiScalingPtr>(GetProcAddress(
					GetModuleHandle(L"user32.dll"), "EnableNonClientDpiScaling"));
			called_enable_non_client_dpi_scaling_ = !!(func_ptr && func_ptr(hwnd_));
		}
	}

	void RootGameWindowWin::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		const HINSTANCE hInstance = lpCreateStruct->hInstance;

		RECT rect;
		GetClientRect(hwnd_, &rect);

		if (with_controls_)
		{
			// Create the child controls.
			int x_offset = 0;

			_gameWindow = CreateWindow(	L"GAMEWINDOW", 
				L"game", 
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
				x_offset, 0, 300, 300, hwnd_, nullptr, hInstance, this);
			CHECK(_gameWindow);		

			{
				auto hDC = GetDC(_gameWindow);

				PIXELFORMATDESCRIPTOR pfd;

				/* there is no guarantee that the contents of the stack that become
				   the pfd are zeroed, therefore _make sure_ to clear these bits. */
				memset(&pfd, 0, sizeof(pfd));
				pfd.nSize = sizeof(pfd);
				pfd.nVersion = 1;
				pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
				pfd.iPixelType = PFD_TYPE_RGBA;
				pfd.cColorBits = 32;

				auto pf = ChoosePixelFormat(hDC, &pfd);
				if (pf == 0)
				{
					MessageBoxA(NULL, "ChoosePixelFormat() failed: Cannot find a suitable pixel format.", "Error", MB_OK);
					//return 0;
				}
				if (SetPixelFormat(hDC, pf, &pfd) == FALSE)
				{
					MessageBoxA(NULL, "SetPixelFormat() failed: Cannot set format specified.", "Error", MB_OK);
					//return 0;
				}

				DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
				ReleaseDC(_gameWindow, hDC);
			}
						
			if (!with_osr_) 
			{
				// Remove the menu items that are only used with OSR.
				HMENU hMenu = ::GetMenu(hwnd_);
				if (hMenu)
				{
					HMENU hTestMenu = ::GetSubMenu(hMenu, 2);
					if (hTestMenu) 
					{
						::RemoveMenu(hTestMenu, ID_TESTS_OSR_FPS, MF_BYCOMMAND);
						::RemoveMenu(hTestMenu, ID_TESTS_OSR_DSF, MF_BYCOMMAND);
					}
				}
			}
		}
		else
		{
			// No controls so also remove the default menu.
			::SetMenu(hwnd_, NULL);
		}

		const float device_scale_factor = GetWindowScaleFactor(hwnd_);

		if (with_osr_) 
		{
			browser_window_->SetDeviceScaleFactor(device_scale_factor);
		}

		if (!is_popup_)
		{
			// Create the browser window.
			CefRect cef_rect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
			browser_window_->CreateBrowser(hwnd_, cef_rect, browser_settings_, nullptr,	delegate_->GetRequestContext(this));
		}
		else
		{
			// With popups we already have a browser window. Parent the browser window
			// to the root window and show it in the correct location.
			browser_window_->ShowPopup(hwnd_, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
		}
	}

	bool RootGameWindowWin::OnClose() {
		if (browser_window_ && !browser_window_->IsClosing()) {
			CefRefPtr<CefBrowser> browser = GetBrowser();
			if (browser) {
				// Notify the browser window that we would like to close it. This
				// will result in a call to ClientHandler::DoClose() if the
				// JavaScript 'onbeforeunload' event handler allows it.
				browser->GetHost()->CloseBrowser(false);

				// Cancel the close.
				return true;
			}
		}

		// Allow the close.
		return false;
	}

	void RootGameWindowWin::OnDestroyed() {
		window_destroyed_ = true;
		NotifyDestroyedIfDone();
	}

	void RootGameWindowWin::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
		REQUIRE_MAIN_THREAD();

		if (is_popup_) {
			// For popup browsers create the root window once the browser has been
			// created.
			CreateRootWindow(CefBrowserSettings(), false);
		}
		else {
			// Make sure the browser is sized correctly.
			OnSize(false);
		}

		delegate_->OnBrowserCreated(this, browser);
	}

	void RootGameWindowWin::OnBrowserWindowDestroyed() {
		REQUIRE_MAIN_THREAD();

		browser_window_.reset();

		if (!window_destroyed_) {
			// The browser was destroyed first. This could be due to the use of
			// off-screen rendering or execution of JavaScript window.close().
			// Close the RootWindow.
			Close(true);
		}

		browser_destroyed_ = true;
		NotifyDestroyedIfDone();
	}

	void RootGameWindowWin::OnSetFullscreen(bool fullscreen) {
		REQUIRE_MAIN_THREAD();

		CefRefPtr<CefBrowser> browser = GetBrowser();
		if (browser) {
			scoped_ptr<window_test::WindowTestRunnerWin> test_runner(
				new window_test::WindowTestRunnerWin());
			if (fullscreen)
				test_runner->Maximize(browser);
			else
				test_runner->Restore(browser);
		}
	}

	void RootGameWindowWin::OnAutoResize(const CefSize& new_size) {
		REQUIRE_MAIN_THREAD();

		if (!hwnd_)
			return;

		int new_width = new_size.width;

		// Make the window wide enough to drag by the top menu bar.
		if (new_width < 200)
			new_width = 200;

		const float device_scale_factor = GetWindowScaleFactor(hwnd_);
		RECT rect = { 0, 0, LogicalToDevice(new_width, device_scale_factor),
					 LogicalToDevice(new_size.height, device_scale_factor) };
		DWORD style = GetWindowLong(hwnd_, GWL_STYLE);
		DWORD ex_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
		bool has_menu = !(style & WS_CHILD) && (GetMenu(hwnd_) != NULL);

		// The size value is for the client area. Calculate the whole window size
		// based on the current style.
		AdjustWindowRectEx(&rect, style, has_menu, ex_style);

		// Size the window. The left/top values may be negative.
		// Also show the window if it's not currently visible.
		SetWindowPos(hwnd_, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,	SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}

	void RootGameWindowWin::OnSetLoadingState(bool isLoading,
		bool canGoBack,
		bool canGoForward) 
	{
		REQUIRE_MAIN_THREAD();

		EnableWindow(_gameWindow, TRUE);

		if (!isLoading && GetWindowLongPtr(hwnd_, GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
			// Done with the initial navigation. Remove the WS_EX_NOACTIVATE style so
			// that future mouse clicks inside the browser correctly activate and focus
			// the window. For the top-level window removing this style causes Windows
			// to display the task bar button.
			SetWindowLongPtr(hwnd_, GWL_EXSTYLE,
				GetWindowLongPtr(hwnd_, GWL_EXSTYLE) & ~WS_EX_NOACTIVATE);

			if (browser_window_) {
				HWND browser_hwnd = browser_window_->GetWindowHandle();
				SetWindowLongPtr(
					browser_hwnd, GWL_EXSTYLE,
					GetWindowLongPtr(browser_hwnd, GWL_EXSTYLE) & ~WS_EX_NOACTIVATE);
			}
		}
	}

	namespace {

		LPCWSTR kParentWndProc = L"CefParentWndProc";
		LPCWSTR kDraggableRegion = L"CefDraggableRegion";

		LRESULT CALLBACK SubclassedWindowProc(HWND hWnd,
			UINT message,
			WPARAM wParam,
			LPARAM lParam) {
			WNDPROC hParentWndProc =
				reinterpret_cast<WNDPROC>(::GetPropW(hWnd, kParentWndProc));
			HRGN hRegion = reinterpret_cast<HRGN>(::GetPropW(hWnd, kDraggableRegion));

			if (message == WM_NCHITTEST) {
				LRESULT hit = CallWindowProc(hParentWndProc, hWnd, message, wParam, lParam);
				if (hit == HTCLIENT) {
					POINTS points = MAKEPOINTS(lParam);
					POINT point = { points.x, points.y };
					::ScreenToClient(hWnd, &point);
					if (::PtInRegion(hRegion, point.x, point.y)) {
						// Let the parent window handle WM_NCHITTEST by returning HTTRANSPARENT
						// in child windows.
						return HTTRANSPARENT;
					}
				}
				return hit;
			}

			return CallWindowProc(hParentWndProc, hWnd, message, wParam, lParam);
		}

		void SubclassWindow(HWND hWnd, HRGN hRegion) {
			HANDLE hParentWndProc = ::GetPropW(hWnd, kParentWndProc);
			if (hParentWndProc) {
				return;
			}

			SetLastError(0);
			LONG_PTR hOldWndProc = SetWindowLongPtr(
				hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubclassedWindowProc));
			if (hOldWndProc == 0 && GetLastError() != ERROR_SUCCESS) {
				return;
			}

			::SetPropW(hWnd, kParentWndProc, reinterpret_cast<HANDLE>(hOldWndProc));
			::SetPropW(hWnd, kDraggableRegion, reinterpret_cast<HANDLE>(hRegion));
		}

		void UnSubclassWindow(HWND hWnd) {
			LONG_PTR hParentWndProc =
				reinterpret_cast<LONG_PTR>(::GetPropW(hWnd, kParentWndProc));
			if (hParentWndProc) {
				LONG_PTR hPreviousWndProc =
					SetWindowLongPtr(hWnd, GWLP_WNDPROC, hParentWndProc);
				ALLOW_UNUSED_LOCAL(hPreviousWndProc);
				DCHECK_EQ(hPreviousWndProc,
					reinterpret_cast<LONG_PTR>(SubclassedWindowProc));
			}

			::RemovePropW(hWnd, kParentWndProc);
			::RemovePropW(hWnd, kDraggableRegion);
		}

		BOOL CALLBACK SubclassWindowsProc(HWND hwnd, LPARAM lParam) {
			SubclassWindow(hwnd, reinterpret_cast<HRGN>(lParam));
			return TRUE;
		}

		BOOL CALLBACK UnSubclassWindowsProc(HWND hwnd, LPARAM lParam) {
			UnSubclassWindow(hwnd);
			return TRUE;
		}

	}  // namespace
	

	void RootGameWindowWin::NotifyDestroyedIfDone() {
		// Notify once both the window and the browser have been destroyed.
		if (window_destroyed_ && browser_destroyed_)
			delegate_->OnRootWindowDestroyed(this);
	}

}  // namespace client
