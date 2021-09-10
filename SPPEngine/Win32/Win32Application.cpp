// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPEngine.h"
#include "SPPApplication.h"
#include "SPPLogging.h"
#include <functional>

// Windows Header Files
#include <windows.h>
#include <windowsx.h>
#include <cstdint>
#include "shellapi.h"

#include <map>

#include <thread>
#include <chrono>

namespace SPP
{
	LogEntry LOG_Win32App("Win32App");

	std::map<uint32_t, std::string> Win32MessageMap =
	{
	  { 0, "WM_NULL"},
	  { 1, "WM_CREATE" },
	  { 2, "WM_DESTROY" },
	  { 3, "WM_MOVE" },
	  { 5, "WM_SIZE" },
	  { 6, "WM_ACTIVATE" },
	  { 7, "WM_SETFOCUS" },
	  { 8, "WM_KILLFOCUS" },
	  { 10, "WM_ENABLE" },
	  { 11, "WM_SETREDRAW" },
	  { 12, "WM_SETTEXT" },
	  { 13, "WM_GETTEXT" },
	  { 14, "WM_GETTEXTLENGTH" },
	  { 15, "WM_PAINT" },
	  { 16, "WM_CLOSE" },
	  { 17, "WM_QUERYENDSESSION" },
	  { 18, "WM_QUIT" },
	  { 19, "WM_QUERYOPEN" },
	  { 20, "WM_ERASEBKGND" },
	  { 21, "WM_SYSCOLORCHANGE" },
	  { 22, "WM_ENDSESSION" },
	  { 24, "WM_SHOWWINDOW" },
	  { 25, "WM_CTLCOLOR" },
	  { 26, "WM_WININICHANGE" },
	  { 27, "WM_DEVMODECHANGE" },
	  { 28, "WM_ACTIVATEAPP" },
	  { 29, "WM_FONTCHANGE" },
	  { 30, "WM_TIMECHANGE" },
	  { 31, "WM_CANCELMODE" },
	  { 32, "WM_SETCURSOR" },
	  { 33, "WM_MOUSEACTIVATE" },
	  { 34, "WM_CHILDACTIVATE" },
	  { 35, "WM_QUEUESYNC" },
	  { 36, "WM_GETMINMAXINFO" },
	  { 38, "WM_PAINTICON" },
	  { 39, "WM_ICONERASEBKGND" },
	  { 40, "WM_NEXTDLGCT" },
	  { 42, "WM_SPOOLERSTATUS" },
	  { 43, "WM_DRAWITEM" },
	  { 44, "WM_MEASUREITEM" },
	  { 45, "WM_DELETEITEM" },
	  { 46, "WM_VKEYTOITEM" },
	  { 47, "WM_CHARTOITEM" },
	  { 48, "WM_SETFONT" },
	  { 49, "WM_GETFONT" },
	  { 50, "WM_SETHOTKEY" },
	  { 51, "WM_GETHOTKEY" },
	  { 55, "WM_QUERYDRAGICON" },
	  { 57, "WM_COMPAREITEM" },
	  { 61, "WM_GETOBJECT" },
	  { 65, "WM_COMPACTING" },
	  { 68, "WM_COMMNOTIFY" },
	  { 70, "WM_WINDOWPOSCHANGING" },
	  { 71, "WM_WINDOWPOSCHANGED" },
	  { 72, "WM_POWER" },
	  { 73, "WM_COPYGLOBALDATA" },
	  { 74, "WM_COPYDATA" },
	  { 75, "WM_CANCELJOURNA" },
	  { 78, "WM_NOTIFY" },
	  { 80, "WM_INPUTLANGCHANGEREQUEST" },
	  { 81, "WM_INPUTLANGCHANGE" },
	  { 82, "WM_TCARD" },
	  { 83, "WM_HELP" },
	  { 84, "WM_USERCHANGED" },
	  { 85, "WM_NOTIFYFORMAT" },
	  { 123, "WM_CONTEXTMENU" },
	  { 124, "WM_STYLECHANGING" },
	  { 125, "WM_STYLECHANGED" },
	  { 126, "WM_DISPLAYCHANGE" },
	  { 127, "WM_GETICON" },
	  { 128, "WM_SETICON" },
	  { 129, "WM_NCCREATE" },
	  { 130, "WM_NCDESTROY" },
	  { 131, "WM_NCCALCSIZE" },
	  { 132, "WM_NCHITTEST" },
	  { 133, "WM_NCPAINT" },
	  { 134, "WM_NCACTIVATE" },
	  { 135, "WM_GETDLGCODE" },
	  { 136, "WM_SYNCPAINT" },
	  { 160, "WM_NCMOUSEMOVE" },
	  { 161, "WM_NCLBUTTONDOWN" },
	  { 162, "WM_NCLBUTTONUP" },
	  { 163, "WM_NCLBUTTONDBLCLK" },
	  { 164, "WM_NCRBUTTONDOWN" },
	  { 165, "WM_NCRBUTTONUP" },
	  { 166, "WM_NCRBUTTONDBLCLK" },
	  { 167, "WM_NCMBUTTONDOWN" },
	  { 168, "WM_NCMBUTTONUP" },
	  { 169, "WM_NCMBUTTONDBLCLK" },
	  { 171, "WM_NCXBUTTONDOWN" },
	  { 172, "WM_NCXBUTTONUP" },
	  { 173, "WM_NCXBUTTONDBLCLK" },
	  { 176, "EM_GETSE" },
	  { 177, "EM_SETSE" },
	  { 178, "EM_GETRECT" },
	  { 179, "EM_SETRECT" },
	  { 180, "EM_SETRECTNP" },
	  { 181, "EM_SCROL" },
	  { 182, "EM_LINESCROL" },
	  { 183, "EM_SCROLLCARET" },
	  { 185, "EM_GETMODIFY" },
	  { 187, "EM_SETMODIFY" },
	  { 188, "EM_GETLINECOUNT" },
	  { 189, "EM_LINEINDEX" },
	  { 190, "EM_SETHANDLE" },
	  { 191, "EM_GETHANDLE" },
	  { 192, "EM_GETTHUMB" },
	  { 193, "EM_LINELENGTH" },
	  { 194, "EM_REPLACESE" },
	  { 195, "EM_SETFONT" },
	  { 196, "EM_GETLINE" },
	  { 197, "EM_LIMITTEXT" },
	  { 197, "EM_SETLIMITTEXT" },
	  { 198, "EM_CANUNDO" },
	  { 199, "EM_UNDO" },
	  { 200, "EM_FMTLINES" },
	  { 201, "EM_LINEFROMCHAR" },
	  { 202, "EM_SETWORDBREAK" },
	  { 203, "EM_SETTABSTOPS" },
	  { 204, "EM_SETPASSWORDCHAR" },
	  { 205, "EM_EMPTYUNDOBUFFER" },
	  { 206, "EM_GETFIRSTVISIBLELINE" },
	  { 207, "EM_SETREADONLY" },
	  { 209, "EM_SETWORDBREAKPROC" },
	  { 209, "EM_GETWORDBREAKPROC" },
	  { 210, "EM_GETPASSWORDCHAR" },
	  { 211, "EM_SETMARGINS" },
	  { 212, "EM_GETMARGINS" },
	  { 213, "EM_GETLIMITTEXT" },
	  { 214, "EM_POSFROMCHAR" },
	  { 215, "EM_CHARFROMPOS" },
	  { 216, "EM_SETIMESTATUS" },
	  { 217, "EM_GETIMESTATUS" },
	  { 224, "SBM_SETPOS" },
	  { 225, "SBM_GETPOS" },
	  { 226, "SBM_SETRANGE" },
	  { 227, "SBM_GETRANGE" },
	  { 228, "SBM_ENABLE_ARROWS" },
	  { 230, "SBM_SETRANGEREDRAW" },
	  { 233, "SBM_SETSCROLLINFO" },
	  { 234, "SBM_GETSCROLLINFO" },
	  { 235, "SBM_GETSCROLLBARINFO" },
	  { 240, "BM_GETCHECK" },
	  { 241, "BM_SETCHECK" },
	  { 242, "BM_GETSTATE" },
	  { 243, "BM_SETSTATE" },
	  { 244, "BM_SETSTYLE" },
	  { 245, "BM_CLICK" },
	  { 246, "BM_GETIMAGE" },
	  { 247, "BM_SETIMAGE" },
	  { 248, "BM_SETDONTCLICK" },
	  { 255, "WM_INPUT" },
	  { 256, "WM_KEYDOWN" },
	  { 256, "WM_KEYFIRST" },
	  { 257, "WM_KEYUP" },
	  { 258, "WM_CHAR" },
	  { 259, "WM_DEADCHAR" },
	  { 260, "WM_SYSKEYDOWN" },
	  { 261, "WM_SYSKEYUP" },
	  { 262, "WM_SYSCHAR" },
	  { 263, "WM_SYSDEADCHAR" },
	  { 264, "WM_KEYLAST" },
	  { 265, "WM_UNICHAR" },
	  { 265, "WM_WNT_CONVERTREQUESTEX" },
	  { 266, "WM_CONVERTREQUEST" },
	  { 267, "WM_CONVERTRESULT" },
	  { 268, "WM_INTERIM" },
	  { 269, "WM_IME_STARTCOMPOSITION" },
	  { 270, "WM_IME_ENDCOMPOSITION" },
	  { 271, "WM_IME_COMPOSITION" },
	  { 271, "WM_IME_KEYLAST" },
	  { 272, "WM_INITDIALOG" },
	  { 273, "WM_COMMAND" },
	  { 274, "WM_SYSCOMMAND" },
	  { 275, "WM_TIMER" },
	  { 276, "WM_HSCROL" },
	  { 277, "WM_VSCROL" },
	  { 278, "WM_INITMENU" },
	  { 279, "WM_INITMENUPOPUP" },
	  { 280, "WM_SYSTIMER" },
	  { 287, "WM_MENUSELECT" },
	  { 288, "WM_MENUCHAR" },
	  { 289, "WM_ENTERIDLE" },
	  { 290, "WM_MENURBUTTONUP" },
	  { 291, "WM_MENUDRAG" },
	  { 292, "WM_MENUGETOBJECT" },
	  { 293, "WM_UNINITMENUPOPUP" },
	  { 294, "WM_MENUCOMMAND" },
	  { 295, "WM_CHANGEUISTATE" },
	  { 296, "WM_UPDATEUISTATE" },
	  { 297, "WM_QUERYUISTATE" },
	  { 306, "WM_CTLCOLORMSGBOX" },
	  { 307, "WM_CTLCOLOREDIT" },
	  { 308, "WM_CTLCOLORLISTBOX" },
	  { 309, "WM_CTLCOLORBTN" },
	  { 310, "WM_CTLCOLORDLG" },
	  { 311, "WM_CTLCOLORSCROLLBAR" },
	  { 312, "WM_CTLCOLORSTATIC" },
	  { 512, "WM_MOUSEFIRST" },
	  { 512, "WM_MOUSEMOVE" },
	  { 513, "WM_LBUTTONDOWN" },
	  { 514, "WM_LBUTTONUP" },
	  { 515, "WM_LBUTTONDBLCLK" },
	  { 516, "WM_RBUTTONDOWN" },
	  { 517, "WM_RBUTTONUP" },
	  { 518, "WM_RBUTTONDBLCLK" },
	  { 519, "WM_MBUTTONDOWN" },
	  { 520, "WM_MBUTTONUP" },
	  { 521, "WM_MBUTTONDBLCLK" },
	  { 521, "WM_MOUSELAST" },
	  { 522, "WM_MOUSEWHEE" },
	  { 523, "WM_XBUTTONDOWN" },
	  { 524, "WM_XBUTTONUP" },
	  { 525, "WM_XBUTTONDBLCLK" },
	  { 528, "WM_PARENTNOTIFY" },
	  { 529, "WM_ENTERMENULOOP" },
	  { 530, "WM_EXITMENULOOP" },
	  { 531, "WM_NEXTMENU" },
	  { 532, "WM_SIZING" },
	  { 533, "WM_CAPTURECHANGED" },
	  { 534, "WM_MOVING" },
	  { 536, "WM_POWERBROADCAST" },
	  { 537, "WM_DEVICECHANGE" },
	  { 544, "WM_MDICREATE" },
	  { 545, "WM_MDIDESTROY" },
	  { 546, "WM_MDIACTIVATE" },
	  { 547, "WM_MDIRESTORE" },
	  { 548, "WM_MDINEXT" },
	  { 549, "WM_MDIMAXIMIZE" },
	  { 550, "WM_MDITILE" },
	  { 551, "WM_MDICASCADE" },
	  { 552, "WM_MDIICONARRANGE" },
	  { 553, "WM_MDIGETACTIVE" },
	  { 560, "WM_MDISETMENU" },
	  { 561, "WM_ENTERSIZEMOVE" },
	  { 562, "WM_EXITSIZEMOVE" },
	  { 563, "WM_DROPFILES" },
	  { 564, "WM_MDIREFRESHMENU" },
	  { 640, "WM_IME_REPORT" },
	  { 641, "WM_IME_SETCONTEXT" },
	  { 642, "WM_IME_NOTIFY" },
	  { 643, "WM_IME_CONTRO" },
	  { 644, "WM_IME_COMPOSITIONFUL" },
	  { 645, "WM_IME_SELECT" },
	  { 646, "WM_IME_CHAR" },
	  { 648, "WM_IME_REQUEST" },
	  { 656, "WM_IMEKEYDOWN" },
	  { 656, "WM_IME_KEYDOWN" },
	  { 657, "WM_IMEKEYUP" },
	  { 657, "WM_IME_KEYUP" },
	  { 672, "WM_NCMOUSEHOVER" },
	  { 673, "WM_MOUSEHOVER" },
	  { 674, "WM_NCMOUSELEAVE" },
	  { 675, "WM_MOUSELEAVE" },
	  { 768, "WM_CUT" },
	  { 769, "WM_COPY" },
	  { 770, "WM_PASTE" },
	  { 771, "WM_CLEAR" },
	  { 772, "WM_UNDO" },
	  { 773, "WM_RENDERFORMAT" },
	  { 774, "WM_RENDERALLFORMATS" },
	  { 775, "WM_DESTROYCLIPBOARD" },
	  { 776, "WM_DRAWCLIPBOARD" },
	  { 777, "WM_PAINTCLIPBOARD" },
	  { 778, "WM_VSCROLLCLIPBOARD" },
	  { 779, "WM_SIZECLIPBOARD" },
	  { 780, "WM_ASKCBFORMATNAME" },
	  { 781, "WM_CHANGECBCHAIN" },
	  { 782, "WM_HSCROLLCLIPBOARD" },
	  { 783, "WM_QUERYNEWPALETTE" },
	  { 784, "WM_PALETTEISCHANGING" },
	  { 785, "WM_PALETTECHANGED" },
	  { 786, "WM_HOTKEY" },
	  { 791, "WM_PRINT" },
	  { 792, "WM_PRINTCLIENT" },
	  { 793, "WM_APPCOMMAND" },
	  { 856, "WM_HANDHELDFIRST" },
	  { 863, "WM_HANDHELDLAST" },
	  { 864, "WM_AFXFIRST" },
	  { 895, "WM_AFXLAST" },
	  { 896, "WM_PENWINFIRST" },
	  { 897, "WM_RCRESULT" },
	  { 898, "WM_HOOKRCRESULT" },
	  { 899, "WM_GLOBALRCCHANGE" },
	  { 899, "WM_PENMISCINFO" },
	  { 900, "WM_SKB" },
	  { 901, "WM_HEDITCT" },
	  { 901, "WM_PENCT" },
	  { 902, "WM_PENMISC" },
	  { 903, "WM_CTLINIT" },
	  { 904, "WM_PENEVENT" },
	  { 911, "WM_PENWINLAST" },
	  { 1024, "WM_USER" }
	};

	class SPP_ENGINE_API Win32Application : public ApplicationWindow
	{
	private:
		RECT _originalClip = { 0 };
		bool _bIsFocused = false;
		HWND m_hwnd = nullptr;
		HBITMAP _hBackground = nullptr;

	public:
		Win32Application() {}
		int Run(HINSTANCE hInstance, int nCmdShow);
		HWND GetHwnd() { return m_hwnd; }

		virtual bool Initialize(int32_t Width, int32_t Height, void* hInstance = nullptr, AppFlags Flags = AppFlags::None) override;
		virtual void DrawImageToWindow(int32_t Width, int32_t Height, const void* InData, int32_t InDataSize, uint8_t BPP) override;
		virtual int32_t Run() override;
		virtual int32_t RunOnce() override;
		virtual void* GetOSWindow() override
		{
			return m_hwnd;
		}

	protected:
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	};

	bool Win32Application::Initialize(int32_t Width, int32_t Height, void* hInstance, AppFlags Flags)
	{
		// Initialize the window class.
		WNDCLASSEX windowClass = { 0 };
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = (Flags == AppFlags::SupportOpenGL) ? CS_OWNDC : (CS_HREDRAW | CS_VREDRAW);
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = (HINSTANCE)hInstance;
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.lpszClassName = L"SPPWIN32";
		RegisterClassEx(&windowClass);

		_width = Width;
		_height = Height;

		RECT windowRect = { 0, 0, (LONG)_width, (LONG)_height }; // static_cast<long>(pFramework->GetWidth()), static_cast<long>(pFramework->GetHeight())};
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		// Create the window and store a handle to it.
		m_hwnd = CreateWindow(
			windowClass.lpszClassName,
			L"APP",
			WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			nullptr,		// We have no parent window.
			nullptr,		// We aren't using menus.
			(HINSTANCE)hInstance,
			(LPVOID)this);

		if (Flags == AppFlags::SupportOpenGL)
		{
			auto hDC = GetDC(m_hwnd);

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
				return 0;
			}
			if (SetPixelFormat(hDC, pf, &pfd) == FALSE)
			{
				MessageBoxA(NULL, "SetPixelFormat() failed: Cannot set format specified.", "Error", MB_OK);
				return 0;
			}

			DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
			ReleaseDC(m_hwnd, hDC);
		}

		ShowWindow(m_hwnd, SW_SHOW);

		// hide cursor and grab original clip region
		//ShowCursor(false);
		//GetClipCursor(&_originalClip);

		// setup the clip and set initial centered mouse
		//GetClientRect(m_hwnd, &windowRect);
		//MapWindowPoints(m_hwnd, GetParent(m_hwnd), (LPPOINT)&windowRect, 2);
		//ClipCursor(&windowRect);

		//POINT setPosition{ (windowRect.left + windowRect.right) / 2, (windowRect.top + windowRect.bottom) / 2 };
		//SetCursorPos(setPosition.x, setPosition.y);

		return true;
	}

	void Win32Application::DrawImageToWindow(int32_t Width, int32_t Height, const void* InData, int32_t InDataSize, uint8_t BPP)
	{
		HDC hDC = GetDC(m_hwnd);

		if (_hBackground)
		{
			DeleteObject(_hBackground);
			_hBackground = nullptr;
		}

		_hBackground = CreateBitmap(Width, Height, 1, BPP * 8, InData);
		InvalidateRect(m_hwnd, nullptr, false);
	}

	int32_t Win32Application::RunOnce()
	{
		MSG msg = {};
		if (PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE))
		{
			if (WM_QUIT == msg.message)
			{
				return -1;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		return 0;
	}

	void DrawHBITMAPtoHDC(HBITMAP hBitmap, HDC hdc)
	{
		BITMAP bm;
		HDC MemDCExercising = CreateCompatibleDC(hdc);
		HBITMAP oldbitmap = (HBITMAP)SelectObject(MemDCExercising, hBitmap);
		GetObject(hBitmap, sizeof(bm), &bm);
		BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, MemDCExercising, 0, 0, SRCCOPY);
		SelectObject(MemDCExercising, oldbitmap);
		DeleteDC(MemDCExercising);
	}

	int32_t Win32Application::Run()
	{

		using namespace std::chrono_literals;

		ShowWindow(m_hwnd, SW_SHOW);

		MSG msg = {};
		while (true)
		{
			MSG msg = { 0 };
			while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) == TRUE)
			{
				if (GetMessage(&msg, NULL, 0, 0))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				else
				{
					return 0;
				}
			}

			if (_msgLoop)
			{
				_msgLoop();
			}

			std::this_thread::sleep_for(0ms);
		}

		// Return this part of the WM_QUIT message to Windows.
		return static_cast<char>(msg.wParam);
	}

	// Main message handler for the sample.
	LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		static bool s_in_sizemove = false;
		static bool s_in_suspend = false;
		static bool s_minimized = false;
		static bool s_fullscreen = false;
		// Set s_fullscreen to true if defaulting to fullscreen.

		const auto pApp = reinterpret_cast<Win32Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		switch (message)
		{
		case WM_CREATE:
		{
			// Save the DXSample* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
		}
		return 0;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			if (pApp->_hBackground)
			{
				DrawHBITMAPtoHDC(pApp->_hBackground, hdc);
			}

			EndPaint(hWnd, &ps);
		}
		return 0;

		//case WM_SETFOCUS:
		//	pApp->_bIsFocused = true;
		//	break;

		//case WM_KILLFOCUS:
		//	pApp->_bIsFocused = false;
		//	break;

		//case WM_MOVE:
		//	if (pApp->_windowMoved)
		//		pApp->_windowMoved();
		//	return 0;

		//case WM_SIZE:
		//	if (wParam == SIZE_MINIMIZED)
		//	{
		//		if (!s_minimized)
		//		{
		//			s_minimized = true;
		//			if (!s_in_suspend && pApp->_onSuspend)
		//				pApp->_onSuspend();
		//			s_in_suspend = true;
		//		}
		//	}
		//	else if (s_minimized)
		//	{
		//		s_minimized = false;
		//		if (s_in_suspend && pApp->_onResume)
		//			pApp->_onResume();
		//		s_in_suspend = false;
		//	}
		//	else if (!s_in_sizemove && pApp->_onSizeChanged)
		//	{
		//		pApp->_width = LOWORD(lParam);
		//		pApp->_height = HIWORD(lParam);
		//		pApp->_onSizeChanged(pApp->_width, pApp->_height);
		//	}
		//	return 0;

		//case WM_ENTERSIZEMOVE:
		//	s_in_sizemove = true;
		//	return 0;

		//case WM_EXITSIZEMOVE:
		//	s_in_sizemove = false;
		//	if (pApp->_onSizeChanged)
		//	{
		//		RECT rc;
		//		GetClientRect(hWnd, &rc);

		//		const auto w = rc.right - rc.left;
		//		const auto h = rc.bottom - rc.top;

		//		if (pApp->_width != w || pApp->_height != h)
		//		{
		//			pApp->_onSizeChanged(w, h);
		//			pApp->_width = w;
		//			pApp->_height = h;
		//		}
		//	}
		//	return 0;

		case WM_KEYDOWN:
			if (pApp->keyDown) pApp->keyDown(static_cast<uint8_t>(wParam));
			return 0;

		case WM_KEYUP:
			if (pApp->keyUp) pApp->keyUp(static_cast<uint8_t>(wParam));
			return 0;

		case WM_LBUTTONDOWN:
			if (pApp->mouseDown)
				pApp->mouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (uint8_t)EMouseButton::Left);
			return 0;

		case WM_LBUTTONUP:
			if (pApp->mouseUp)
				pApp->mouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (uint8_t)EMouseButton::Left);
			return 0;

		case WM_RBUTTONDOWN:
			if (pApp->mouseDown)
				pApp->mouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (uint8_t)EMouseButton::Right);
			return 0;

		case WM_RBUTTONUP:
			if (pApp->mouseUp)
				pApp->mouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (uint8_t)EMouseButton::Right);
			return 0;

		case WM_MBUTTONDOWN:
			if (pApp->mouseDown)
				pApp->mouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (uint8_t)EMouseButton::Middle);
			return 0;

		case WM_MBUTTONUP:
			if (pApp->mouseUp)
				pApp->mouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (uint8_t)EMouseButton::Middle);
			return 0;

			//case WM_MOUSEMOVE:
			//	if (pApp->mouseMove)
			//		pApp->mouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			//	return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;
		}

		// Handle any messages the switch statement didn't.
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	SPP_ENGINE_API std::unique_ptr< ApplicationWindow > CreateApplication(const char* appType)
	{
		return std::make_unique< Win32Application >();
	}

}