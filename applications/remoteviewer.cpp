// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

//
#if PLATFORM_WINDOWS
	#include <windows.h>	
	#include <wx/msw/msvcrt.h>
	#pragma comment(lib, "opengl32.lib")
	#include <GL/gl.h>

	#ifdef SendMessage
		#undef SendMessage
	#endif
#endif

#include "SPPCore.h"
#include "SPPNatTraversal.h"
#include "json/json.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"
#include "SPPJsonUtils.h"

#include <set>

#include "SPPMath.h"

#include "SPPVideo.h"
#include "SPPNetworkConnection.h"
#include "SPPNetworkMessenger.h"
#include "SPPMemory.h"
#include "SPPApplication.h"

#include "SPPPlatformCore.h"

#include <mutex>

#include <wx/wx.h>
#include <wx/glcanvas.h>

#if PLATFORM_WINDOWS && HAS_WINRT
	#include "SPPWinRTBTE.h"
#endif

SPP_OVERLOAD_ALLOCATORS

using namespace SPP;

LogEntry LOG_APP("APP");

IPv4_SocketAddress RemoteCoordAddres;
std::string StunURL;
uint16_t StunPort;

class MyApp;
MyApp* GApp = nullptr;

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string AppName;
	std::string AppCL;
};

class VideoConnection;

static std::vector<uint8_t> startMessage = { 0, 1, 2, 3 };
static std::vector<uint8_t> endMessage = { 3, 2, 1, 0 };

class SimpleJSONPeerReader
{
protected:
	std::vector<uint8_t> streamData;
	std::vector<uint8_t> recvBuffer;
	std::shared_ptr< Interface_PeerConnection > _peerLink;
	std::function<void(const std::string&)> _handler;
public:
	SimpleJSONPeerReader(std::shared_ptr< Interface_PeerConnection > InPeer,
		std::function<void(const std::string&)> InMsgHandler) : _peerLink(InPeer), _handler(InMsgHandler)
	{
	}

	bool IsValid()
	{
		if (_peerLink)
		{
			return true;
		}
		return false;
	}

	void Tick()
	{
		if (_peerLink)
		{
			if (_peerLink->IsBroken())
			{
				SPP_LOG(LOG_APP, LOG_INFO, "PEER LINK BROKEN");
				_peerLink.reset();
				return;
			}
		}
		else
		{
			return;
		}

		recvBuffer.resize(std::numeric_limits<uint16_t>::max());
		auto DataRecv = _peerLink->Receive(recvBuffer.data(), recvBuffer.size());
		if (DataRecv > 0)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "GOT BT DATA: %d", DataRecv);

			streamData.insert(streamData.end(), recvBuffer.begin(), recvBuffer.begin() + DataRecv);

			auto FindStart = std::search(streamData.begin(), streamData.end(), startMessage.begin(), startMessage.end());

			if (FindStart != streamData.end())
			{
				auto FindEnd = std::search(FindStart, streamData.end(), endMessage.begin(), endMessage.end());

				if (FindEnd != streamData.end())
				{
					std::string messageString(FindStart + startMessage.size(), FindEnd);
					MessageReceived(messageString);
					streamData.erase(streamData.begin(), FindEnd + endMessage.size());
				}
			}

			// just in case it gets stupid big
			if (streamData.size() > 500)
			{
				streamData.clear();
			}
		}
	}

	void MessageReceived(const std::string& InMessage)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "MessageReceived: %s", InMessage.c_str());
		if (_handler)
		{
			_handler(InMessage);
		}
	}

	void SendMessage(const void* buffer, uint16_t sendSize)
	{
		_peerLink->Send(buffer, sendSize);
	}
};


/// <summary>
/// 
/// </summary>
/// 
/// class MyFrame;
/// 

class InputHandler
{
private:
	bool _bIsReceiving = false;
	std::mutex _handlerLock;
	std::list< std::vector<uint8_t> > _events;

public:
	void StartReceiving()
	{
		std::unique_lock<std::mutex> lock(_handlerLock);
		_bIsReceiving = true;
	}

	void StopReceiving()
	{
		std::unique_lock<std::mutex> lock(_handlerLock);
		_bIsReceiving = false;
		_events.clear();
	}

	void PushEvent(std::vector<uint8_t> &InEvent)
	{
		std::unique_lock<std::mutex> lock(_handlerLock);
		if (!_bIsReceiving) return;
		std::vector<uint8_t> localEvent;
		std::swap(localEvent, InEvent);
		_events.push_back(localEvent);
	}	

	void GetEvents(std::list< std::vector<uint8_t> > &oEvents)
	{
		std::unique_lock<std::mutex> lock(_handlerLock);
		if (!_bIsReceiving) return;
		std::swap(oEvents, _events);
		_events.clear();
	}
};

InputHandler GInputHandler;

class BasicGLPane : public wxGLCanvas
{
private:
	wxGLContext* m_context;

	int32_t VideoSizeX = 0;
	int32_t VideoSizeY = 0;

	GLuint tex2D = 0;
	Matrix3x3 virtualToReal;

	std::atomic_bool _lockImage;
	std::vector<uint8_t> _imageData;

	void _KeyboardEvent(wxKeyEvent& event, bool bKeyDown);
	void _MouseEvent(wxMouseEvent& event);
	void _IncomingVideoImageData(int32_t ImageX, int32_t ImageY, const void* ImageData)
	{
		VideoSizeX = ImageX;
		VideoSizeY = ImageY;

		glBindTexture(GL_TEXTURE_2D, tex2D);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ImageX, ImageY, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageData);
		glBindTexture(GL_TEXTURE_2D, 0);

		_lockImage.exchange(false);

		wxWindow::Refresh();
	}

public:
	BasicGLPane(wxFrame* parent, int* args);
	virtual ~BasicGLPane();
	
	void resized(wxSizeEvent& evt);

	int getWidth();
	int getHeight();

	void render(wxPaintEvent& evt);

	// events
	void mouseMoved(wxMouseEvent& event);
	void mouseDown(wxMouseEvent& event);
	void mouseWheelMoved(wxMouseEvent& event);
	void mouseReleased(wxMouseEvent& event);
	void rightClick(wxMouseEvent& event);
	void mouseLeftWindow(wxMouseEvent& event);
	void keyPressed(wxKeyEvent& event);
	void keyReleased(wxKeyEvent& event);

	//MUST BE ON MAIN LOOP THREAD
	void THREADSAFE_IncomingVideoImageData(int32_t ImageX, int32_t ImageY, const void* ImageData)
	{
		auto bDidLock = (_lockImage.exchange(true) == false);

		if (bDidLock)
		{
			const uint32_t ImageSize = ImageX * ImageY * 4;
			if(_imageData.size() != ImageSize)
				_imageData.resize(ImageSize);
			memcpy(_imageData.data(), ImageData, ImageSize);

			auto appInstance = (wxApp*)wxApp::GetInstance();
			appInstance->GetTopWindow()->GetEventHandler()->CallAfter([ImageX, ImageY, ImageData = _imageData.data(), OurFrame = this]()
				{
					OurFrame->_IncomingVideoImageData(ImageX, ImageY, ImageData);
				});
		}
	}

	

	DECLARE_EVENT_TABLE()
};

BasicGLPane* GGLPane = nullptr;

BEGIN_EVENT_TABLE(BasicGLPane, wxGLCanvas)
EVT_MOTION(BasicGLPane::mouseMoved)
EVT_LEFT_DOWN(BasicGLPane::mouseDown)
EVT_LEFT_UP(BasicGLPane::mouseReleased)
EVT_RIGHT_DOWN(BasicGLPane::rightClick)
EVT_LEAVE_WINDOW(BasicGLPane::mouseLeftWindow)
EVT_SIZE(BasicGLPane::resized)
EVT_KEY_DOWN(BasicGLPane::keyPressed)
EVT_KEY_UP(BasicGLPane::keyReleased)
EVT_MOUSEWHEEL(BasicGLPane::mouseWheelMoved)
EVT_PAINT(BasicGLPane::render)
END_EVENT_TABLE()

const uint8_t KeyBoardMessage = 0x02;
const uint8_t MouseMessage = 0x03;

void BasicGLPane::_KeyboardEvent(wxKeyEvent& event, bool bKeyDown)
{
	uint8_t bDown = bKeyDown ? 1 : 0;
	int32_t keyCode = event.GetKeyCode();

	BinaryBlobSerializer thisMessage;
	thisMessage << KeyBoardMessage;
	thisMessage << bDown;
	thisMessage << keyCode;

	GInputHandler.PushEvent(thisMessage.GetArray());	
}

void BasicGLPane::_MouseEvent(wxMouseEvent& event)
{
	//wxMOUSE_BTN_NONE = 0,
	//wxMOUSE_BTN_LEFT = 1,
	//wxMOUSE_BTN_MIDDLE = 2,
	//wxMOUSE_BTN_RIGHT = 3

	int32_t curButton = event.GetButton();
	uint8_t ActualButton = event.GetButton();
	uint8_t bDown = 0;

	if (event.ButtonDown())
	{
		ActualButton = (uint8_t)curButton;
		bDown = 1;
	}
	else if (event.ButtonUp())
	{
		ActualButton = (uint8_t)curButton;
	}

	uint8_t DownState = 0;
	DownState |= event.LeftIsDown() ? 0x01 : 0;
	DownState |= event.MiddleIsDown() ? 0x02 : 0;
	DownState |= event.RightIsDown() ? 0x04 : 0;

	int32_t xPos = event.GetX();
	int32_t yPos = event.GetY();
	
	Vector3 remap(xPos, yPos, 1);
	Vector3 RemappedPosition = remap * virtualToReal;

	if (RemappedPosition[0] >= 0 && RemappedPosition[1] >= 0)
	{		
		BinaryBlobSerializer thisMessage;
		thisMessage << MouseMessage;
		thisMessage << ActualButton;
		thisMessage << bDown;
		thisMessage << DownState;
		thisMessage << (uint16_t)RemappedPosition[0];
		thisMessage << (uint16_t)RemappedPosition[1];

		GInputHandler.PushEvent(thisMessage.GetArray());
	}
}

// some useful events to use
void BasicGLPane::mouseMoved(wxMouseEvent& event) 
{
	_MouseEvent(event);
}
void BasicGLPane::mouseDown(wxMouseEvent& event) 
{
	_MouseEvent(event);
}
void BasicGLPane::mouseWheelMoved(wxMouseEvent& event) 
{
	_MouseEvent(event);
}
void BasicGLPane::mouseReleased(wxMouseEvent& event) 
{
	_MouseEvent(event);
}
void BasicGLPane::rightClick(wxMouseEvent& event) 
{
	_MouseEvent(event);
}
void BasicGLPane::mouseLeftWindow(wxMouseEvent& event) 
{
	_MouseEvent(event);
}
void BasicGLPane::keyPressed(wxKeyEvent& event) 
{
	_KeyboardEvent(event,true);
}
void BasicGLPane::keyReleased(wxKeyEvent& event) 
{
	_KeyboardEvent(event,false);
}

BasicGLPane::BasicGLPane(wxFrame* parent, int* args) :
	wxGLCanvas(parent, wxID_ANY, args, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
	m_context = new wxGLContext(this);
	// To avoid flashing on MSW
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);

	wxGLCanvas::SetCurrent(*m_context);

	glGenTextures(1, &tex2D);
	glBindTexture(GL_TEXTURE_2D, tex2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	uint32_t ImageData = 0;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &ImageData);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDisable(GL_DEPTH);
	glDisable(GL_LIGHTING);
	glDepthFunc(GL_ALWAYS);

	GGLPane = this;
}

BasicGLPane::~BasicGLPane()
{
	GGLPane = nullptr;
	wxGLCanvas::SetCurrent(*m_context);
	glDeleteTextures(1, &tex2D);
	delete m_context;
}

void BasicGLPane::resized(wxSizeEvent& evt)
{
	//wxGLCanvas::OnSize(evt);
	Refresh();
}

int BasicGLPane::getWidth()
{
	return GetSize().x;
}

int BasicGLPane::getHeight()
{
	return GetSize().y;
}

void BasicGLPane::render(wxPaintEvent& evt)
{
	if (!IsShown()) return;

	wxGLCanvas::SetCurrent(*m_context);
	wxPaintDC paintscope(this); // only to be used in paint events. use wxClientDC to paint outside the paint event

	int32_t WindowSizeX = getWidth();
	int32_t WindowSizeY = getHeight();

	float WindowAspectRatio = (float)WindowSizeX / (float)WindowSizeY;
	float VideoAspectRatio = (float)VideoSizeX / (float)VideoSizeY;
	int32_t VideoDrawWidth = 0;
	int32_t VideoDrawHeight = 0;

	// video wider aspect than window
	if (VideoAspectRatio <= WindowAspectRatio)
	{
		VideoDrawHeight = WindowSizeY;
		VideoDrawWidth = (int32_t)(VideoDrawHeight * VideoAspectRatio);
	}
	else
	{
		VideoDrawWidth = WindowSizeX;
		VideoDrawHeight = (int32_t)(VideoDrawWidth / VideoAspectRatio);
	}

	glViewport(0, 0, WindowSizeX, WindowSizeY);
	glClear(GL_COLOR_BUFFER_BIT);

	float ScaleX = (float)VideoSizeX / (float)VideoDrawWidth;
	float ScaleY = (float)VideoSizeY / (float)VideoDrawHeight;

	int32_t ShiftAmountX = (WindowSizeX - VideoDrawWidth) / 2;
	int32_t ShiftAmountY = (WindowSizeY - VideoDrawHeight) / 2;

	virtualToReal <<
		ScaleX, 0, 0,
		0, ScaleY, 0,
		(float)-ShiftAmountX * ScaleX, (float)-ShiftAmountY * ScaleY, 1.0f;

	glViewport(ShiftAmountX, ShiftAmountY, WindowSizeX, WindowSizeY);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WindowSizeX, 0, WindowSizeY, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glBindTexture(GL_TEXTURE_2D, tex2D);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	glTexCoord2i(0, 0); glVertex2i(0, VideoDrawHeight);  //you should probably change these vertices.
	glTexCoord2i(0, 1); glVertex2i(0, 0);
	glTexCoord2i(1, 1); glVertex2i(VideoDrawWidth, 0);
	glTexCoord2i(1, 0); glVertex2i(VideoDrawWidth, VideoDrawHeight);

	glEnd();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFlush(); //don't need this with GLUT_DOUBLE and glutSwapBuffers
	SwapBuffers();


}

class MyApp : public wxApp
{
private:
	bool OnInit();
	wxFrame* _frame = nullptr;
	BasicGLPane* _glPane = nullptr;

public:
	BasicGLPane* GetGLPane()
	{
		return _glPane;
	}
};

bool MyApp::OnInit()
{
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
	_frame = new wxFrame(nullptr, -1, wxT("Remote Viewer"), wxPoint(50, 50), wxSize(400, 200));
#if PLATFORM_WINDOWS
	_frame->SetIcon(wxICON(sppapp));
#endif
	int args[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0 };

	_glPane = new BasicGLPane(_frame, args);
	sizer->Add(_glPane, 1, wxEXPAND);

	_frame->SetSizer(sizer);
	_frame->SetAutoLayout(true);

	_frame->Show(true);

#if PLATFORM_WINDOWS
	auto hWnd = _frame->GetHandle();
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	ShowWindow(hWnd, SW_SHOWNORMAL);
#endif

	return true;
}

IMPLEMENT_APP_NO_MAIN(MyApp);

#if 0
class SimpleGlutApp
{
private:
	int32_t VideoSizeX = 0;
	int32_t VideoSizeY = 0;

	GLuint tex2D = 0;

	HWND _ourWindow = nullptr;

	Matrix3x3 virtualToReal;

	VideoConnection* _parentConnect = nullptr;

	bool bIsAllDone = false;

public:
	SimpleGlutApp(VideoConnection* InParentConnection) : _parentConnect(InParentConnection)
	{
	}	

	// fix this better
	~SimpleGlutApp()
	{
		glDeleteTextures(1, &tex2D);
		DestroyWindow(_ourWindow);
	}		

	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		const auto pApp = reinterpret_cast<SimpleGlutApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		if (pApp)
		{
			return pApp->LocalWindowProc(hWnd, uMsg, wParam, lParam);
		}

		switch (uMsg)
		{
		case WM_CREATE:
		{
			// Save the DXSample* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
		}
		return 0;
		}

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	void SetupOpenglGLAssets()
	{
		glGenTextures(1, &tex2D);
		glBindTexture(GL_TEXTURE_2D, tex2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		uint32_t ImageData = 0;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &ImageData);
		glBindTexture(GL_TEXTURE_2D, 0);

		glDisable(GL_DEPTH);
		glDisable(GL_LIGHTING);
		glDepthFunc(GL_ALWAYS);
	}

	HWND CreateOpenGLWindow(char* title, int x, int y, int width, int height, BYTE type, DWORD flags)
	{
		int         pf;
		HDC         hDC;
		HWND        hWnd;
		WNDCLASS    wc;
		PIXELFORMATDESCRIPTOR pfd;
		static HINSTANCE hInstance = 0;

		/* only register the window class once - use hInstance as a flag. */
		if (!hInstance)
		{
			hInstance = GetModuleHandle(NULL);
			wc.style = CS_OWNDC;
			wc.lpfnWndProc = (WNDPROC)WindowProc;
			wc.cbClsExtra = 0;
			wc.cbWndExtra = 0;
			wc.hInstance = hInstance;
			wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
			wc.hCursor = LoadCursor(NULL, IDC_ARROW);
			wc.hbrBackground = NULL;
			wc.lpszMenuName = NULL;
			wc.lpszClassName = L"RemoteWindow";

			if (!RegisterClass(&wc))
			{
				MessageBoxA(NULL, "RegisterClass() failed: Cannot register window class.", "Error", MB_OK);
				return NULL;
			}
		}

		_ourWindow = hWnd = CreateWindowA("RemoteWindow",
			title,
			WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
			x, y, width, height, NULL, NULL, hInstance, this);

		if (hWnd == NULL) {
			MessageBoxA(NULL, "CreateWindow() failed:  Cannot create a window.", "Error", MB_OK);
			return NULL;
		}

		hDC = GetDC(hWnd);

		/* there is no guarantee that the contents of the stack that become
		   the pfd are zeroed, therefore _make sure_ to clear these bits. */
		memset(&pfd, 0, sizeof(pfd));
		pfd.nSize = sizeof(pfd);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | flags;
		pfd.iPixelType = type;
		pfd.cColorBits = 32;

		pf = ChoosePixelFormat(hDC, &pfd);
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
		ReleaseDC(hWnd, hDC);

		ShowWindow(hWnd, SW_SHOWDEFAULT);
		ShowWindow(hWnd, SW_SHOWDEFAULT);
		ShowWindow(hWnd, SW_SHOWNORMAL);

		return hWnd;
	}

	LRESULT LocalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void UpdateWindowMessages()
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
				bIsAllDone = true;
				return;
			}
		}
	}

	bool IsDone() const
	{
		return bIsAllDone;
	}

	void Update()
	{	
		UpdateWindowMessages();

		RECT rect;
		GetClientRect(_ourWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		float WindowAspectRatio = (float)WindowSizeX / (float)WindowSizeY;
		float VideoAspectRatio = (float)VideoSizeX / (float)VideoSizeY;
		int32_t VideoDrawWidth = 0;
		int32_t VideoDrawHeight = 0;

		// video wider aspect than window
		if (VideoAspectRatio <= WindowAspectRatio)
		{
			VideoDrawHeight = WindowSizeY;
			VideoDrawWidth = (int32_t)(VideoDrawHeight * VideoAspectRatio);
		}
		else
		{
			VideoDrawWidth = WindowSizeX;
			VideoDrawHeight = (int32_t)(VideoDrawWidth / VideoAspectRatio);
		}

		glViewport(0, 0, WindowSizeX, WindowSizeY);
		glClear(GL_COLOR_BUFFER_BIT);

		float ScaleX = (float)VideoSizeX / (float)VideoDrawWidth;
		float ScaleY = (float)VideoSizeY / (float)VideoDrawHeight;

		int32_t ShiftAmountX = (WindowSizeX - VideoDrawWidth) / 2;
		int32_t ShiftAmountY = (WindowSizeY - VideoDrawHeight) / 2;

		virtualToReal <<
			ScaleX, 0, 0,
			0, ScaleY, 0,
			(float)-ShiftAmountX * ScaleX, (float)-ShiftAmountY * ScaleY, 1.0f;

		glViewport(ShiftAmountX, ShiftAmountY, WindowSizeX, WindowSizeY);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, WindowSizeX, 0, WindowSizeY, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glBindTexture(GL_TEXTURE_2D, tex2D);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);

		glTexCoord2i(0, 0); glVertex2i(0, VideoDrawHeight);  //you should probably change these vertices.
		glTexCoord2i(0, 1); glVertex2i(0, 0);
		glTexCoord2i(1, 1); glVertex2i(VideoDrawWidth, 0);
		glTexCoord2i(1, 0); glVertex2i(VideoDrawWidth, VideoDrawHeight);

		glEnd();
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		glFlush(); //don't need this with GLUT_DOUBLE and glutSwapBuffers
	}

	void DrawImage(int32_t ImageX, int32_t ImageY, const void *ImageData)
	{
		VideoSizeX = ImageX;
		VideoSizeY = ImageY;

		glBindTexture(GL_TEXTURE_2D, tex2D);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ImageX, ImageY, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageData);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

};
#endif

class VideoConnection : public NetworkConnection
{
protected:
	std::unique_ptr< VideoDecodingInterface> VideoDecoder;
	//std::unique_ptr<SimpleGlutApp> app;
	uint16_t CreatedWidth;
	uint16_t CreatedHeight;

	//HDC hDC;				/* device context */
	//HGLRC hRC;				/* opengl context */
	//HWND  hWnd;				/* window */

	std::vector<uint8_t> recvBuffer;

public:
	VideoConnection(std::shared_ptr< Interface_PeerConnection > InPeer) : NetworkConnection(InPeer, false) 
	{ 
		//app = std::make_unique< SimpleGlutApp>(this);
		//hWnd = app->CreateOpenGLWindow("Viewer", 0, 0, 1280, 720, PFD_TYPE_RGBA, 0);
		//hDC = GetDC(hWnd);
		//hRC = wglCreateContext(hDC);
		//wglMakeCurrent(hDC, hRC);

		//app->SetupOpenglGLAssets();

		GInputHandler.StartReceiving();
		recvBuffer.resize(std::numeric_limits<uint16_t>::max());
	}

	virtual ~VideoConnection()
	{
		GInputHandler.StopReceiving();
		//wglMakeCurrent(NULL, NULL);
		//ReleaseDC(hWnd, hDC);
		//wglDeleteContext(hRC);
		//app.reset(nullptr);
	}

	virtual void Tick() override
	{
		auto recvAmmount = _peerLink->Receive(recvBuffer.data(), recvBuffer.size());
		if (recvAmmount > 0)
		{
			ReceivedRawData(recvBuffer.data(), recvAmmount, 0);
		}

		NetworkConnection::Tick();
		
		std::list< std::vector<uint8_t> > inputEvents;
		GInputHandler.GetEvents(inputEvents);

		for (auto& curEvent : inputEvents)
		{
			SendMessage(curEvent.data(), curEvent.size(), EMessageMask::IS_RELIABLE);
		}

		//app->Update();

		//if (app->IsDone())
		//{
		//	CloseDown("ViewerWindow Closed");
		//}
	}

	virtual void MessageReceived(const void* Data, int32_t DataLength)
	{
		MemoryView DataView(Data, DataLength);
		uint16_t VidWidth;
		uint16_t VidHeight;
		DataView >> VidWidth;
		DataView >> VidHeight;

		//if (VidWidth == 0xFFFF)
		//{
		//	SPP_LOG(LOG_APP, LOG_INFO, "MessageReceived: ID %d", VidHeight);
		//	return;
		//}

		//SPP_LOG(LOG_APP, LOG_INFO, "RemoteViewer::Recv Frame %u x %u", VidWidth, VidHeight);

		// gotta reset this bad boy if ratios change
		if (VideoDecoder)
		{
			const auto& vidSettings = VideoDecoder->GetVideoSettings();
			if (vidSettings.width != VidWidth || vidSettings.height != VidHeight)
			{
				VideoDecoder->Finalize();
				VideoDecoder.reset();
			}
		}

		if (!VideoDecoder)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "RemoteViewer::VideoDecoder created %u x %u", VidWidth, VidHeight);

			CreatedWidth = VidWidth;
			CreatedHeight = VidHeight;
			VideoDecoder = CreateVideoDecoder([&](const void* InData, int32_t InDataSize)
				{
					//SPP_LOG(LOG_APP, LOG_INFO, "DECODED FRAME: %d", InDataSize);
					GGLPane->THREADSAFE_IncomingVideoImageData(CreatedWidth, CreatedHeight, InData);
				}, VideoSettings{ VidWidth, VidHeight, 4, 3, 32 } );
		}

		DataView.RebuildViewFromCurrent();
		VideoDecoder->Decode(DataView.GetData(), DataView.Size());
	}
};

#if 0
LRESULT SimpleGlutApp::LocalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static PAINTSTRUCT ps;

	if (uMsg >= WM_KEYDOWN && uMsg <= WM_KEYUP)
	{
		BinaryBlobSerializer thisMessage;
		thisMessage << uMsg;
		thisMessage << wParam;
		thisMessage << lParam;
		_parentConnect->SendMessage(thisMessage.GetData(), thisMessage.Size(), EMessageMask::IS_RELIABLE);

		//SPP_LOG(LOG_APP, LOG_INFO, "%u : %u : %u", uMsg, wParam, lParam);

	}
	else if (uMsg >= WM_MOUSEMOVE && uMsg <= WM_MOUSELAST)
	{
		uint16_t X = (uint16_t)(lParam & 0xFFFFF);
		uint16_t Y = (uint16_t)((lParam >> 16) & 0xFFFFF);

		Vector3 remap(X, Y, 1);
		Vector3 RemappedPosition = remap * virtualToReal;

		if (RemappedPosition[0] >= 0 && RemappedPosition[1] >= 0)
		{
			lParam = (lParam & ~LPARAM(0xFFFFFFFF)) | (uint16_t)RemappedPosition[0] | LPARAM((uint16_t)(RemappedPosition[1])) << 16;

			BinaryBlobSerializer thisMessage;
			thisMessage << uMsg;
			thisMessage << wParam;
			thisMessage << lParam;
			_parentConnect->SendMessage(thisMessage.GetData(), thisMessage.Size(), EMessageMask::IS_RELIABLE);
		}

		//SendMessage 
		//SPP_LOG(LOG_APP, LOG_INFO, "%u : %u : %u", uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_PAINT:
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		return 0;

	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
#endif


bool ParseCC(const std::string& InCmdLn, const std::string& InValue, std::string& OutValue)
{
	if (StartsWith(InCmdLn, InValue))
	{
		OutValue = std::string(InCmdLn.begin() + InValue.size(), InCmdLn.end());
		return true;
	}
	return false;
}

template<typename T, typename S>
struct LocalBTEWatcher : public IBTEWatcher
{
private:
	T& funcAdd;
	S& funcState;
public:
	LocalBTEWatcher(T& inT, S& inS) : funcAdd(inT), funcState(inS) { }
	virtual void IncomingData(uint8_t* InData, size_t DataSize) override
	{
		std::string strConv(InData, InData + DataSize);
		funcAdd(strConv);
	}
	virtual void StateChange(EBTEState InState) override
	{
		funcState(InState);
	}
};

void SPPApp(int argc, char* argv[])
{
	{
		Json::Value JsonConfig;
		SE_ASSERT(FileToJson("config.txt", JsonConfig));

		Json::Value STUN_URL = JsonConfig.get("STUN_URL", Json::Value::nullSingleton());
		Json::Value STUN_PORT = JsonConfig.get("STUN_PORT", Json::Value::nullSingleton());
		Json::Value COORDINATOR_IP = JsonConfig.get("COORDINATOR_IP", Json::Value::nullSingleton());

		SE_ASSERT(!STUN_URL.isNull());
		SE_ASSERT(!STUN_PORT.isNull());
		SE_ASSERT(!COORDINATOR_IP.isNull());

		StunURL = STUN_URL.asCString();
		StunPort = STUN_PORT.asUInt();
		RemoteCoordAddres = IPv4_SocketAddress(COORDINATOR_IP.asCString());
	}

	auto ThisRUNGUID = std::generate_hex(3);
#if PLATFORM_WINDOWS
	AddDLLSearchPath("../3rdParty/libav/bin");
#endif
	std::string IPMemoryID;
	std::string AppPath;
	std::string AppCommandline;

	for (int i = 0; i < argc; ++i)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "CC(%d):%s", i, argv[i]);

		auto Arg = std::string(argv[i]);
		ParseCC(Arg, "-MEM=", IPMemoryID);
		ParseCC(Arg, "-APP=", AppPath);
		ParseCC(Arg, "-CMDLINE=", AppCommandline);
	}

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY: %s", IPMemoryID.c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "EXE PATH: %s", AppPath.c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "APP COMMAND LINE: %s", AppCommandline.c_str());

	IPCMappedMemory ipcMem(IPMemoryID.c_str(), 2 * 1024 * 1024, false);

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY VALID: %d", ipcMem.IsValid());
	SPP_LOG(LOG_APP, LOG_INFO, "RUN GUID: %s", ThisRUNGUID.c_str());

	// START OS NETWORKING
	GetOSNetwork();

	//
	auto juiceSocket = std::make_shared<UDPJuiceSocket>(StunURL.c_str(), StunPort);

	auto LastBTTime = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	auto LastRequestJoins = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	std::unique_ptr<UDP_SQL_Coordinator> coordinator = std::make_unique<UDP_SQL_Coordinator>(RemoteCoordAddres);

	coordinator->SetKeyPair("GUID", ThisRUNGUID);
	coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
	coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");

	std::shared_ptr< VideoConnection > videoConnection;

	using namespace std::chrono_literals;

	//BLUTETOOTH STUFFS
	bool bBTEConnected = false;
	std::shared_ptr< SimpleJSONPeerReader > JSONParserConnection;
	//START UP RFCOMM BT
	std::shared_ptr< BlueToothSocket > listenSocket = std::make_shared<BlueToothSocket>();
	listenSocket->Listen();
	//START UP BTE
#if PLATFORM_WINDOWS && HAS_WINRT
	auto sendBTDataTOManager = [&videoConnection, &LastBTTime](const std::string& InMessage)
	{
		if (videoConnection && videoConnection->IsValid() && videoConnection->IsConnected())
		{
			BinaryBlobSerializer thisMessage;
			thisMessage << (uint8_t)4;
			thisMessage << InMessage;
			videoConnection->SendMessage(thisMessage.GetData(), thisMessage.Size(), EMessageMask::IS_RELIABLE);
		}
	};	

	auto inStateChange = [&bBTEConnected](EBTEState InState)
	{
		bBTEConnected = (InState == EBTEState::Connected ? true : false);
	};

	LocalBTEWatcher oWatcher(sendBTDataTOManager, inStateChange);
	BTEWatcher watcher;
	watcher.WatchForData("366DEE95-85A3-41C1-A507-8C3E02342000",
		{
			{ "366DEE95-85A3-41C1-A507-8C3E02342001", &oWatcher }
		});
#endif

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(1024);

	std::map<std::string, RemoteClient> Hosts;

	coordinator->SetSQLRequestCallback([&Hosts, &juiceSocket](const std::string& InValue)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "CALLBACK: %s", InValue.c_str());

			Json::Value root;
			Json::CharReaderBuilder Builder;
			Json::CharReader* reader = Builder.newCharReader();
			std::string Errors;

			bool parsingSuccessful = reader->parse((char*)InValue.data(), (char*)(InValue.data() + InValue.length()), &root, &Errors);
			delete reader;
			if (!parsingSuccessful)
			{
				return;
			}

			for (int32_t Iter = 0; Iter < root.size(); Iter++)
			{
				auto CurrentEle = root[Iter];

				Json::Value ConnectToValue = CurrentEle.get("GUIDCONNECTTO", Json::Value::nullSingleton());
				Json::Value SDPValue = CurrentEle.get("SDP", Json::Value::nullSingleton());

				if (!ConnectToValue.isNull() && !SDPValue.isNull())
				{
					juiceSocket->SetRemoteSDP_BASE64(SDPValue.asCString());
					return;
				}

				Json::Value AppNameValue = CurrentEle.get("APPNAME", Json::Value::nullSingleton());
				Json::Value AppCL = CurrentEle.get("APPCL", Json::Value(""));
				Json::Value NameValue = CurrentEle.get("NAME", Json::Value::nullSingleton());
				Json::Value GuidValue = CurrentEle.get("GUID", Json::Value::nullSingleton());

				Hosts[GuidValue.asCString()] = RemoteClient{
					std::chrono::steady_clock::now(),
					std::string(NameValue.asCString()),
					std::string(AppNameValue.asCString()),
					std::string(AppCL.asCString())					
				};
			}
		});

	while (true)
	{
		coordinator->Update();
		auto CurrentTime = std::chrono::steady_clock::now();

		//BLUETOOTH SYSTEM
		if (JSONParserConnection)
		{
			if (JSONParserConnection->IsValid())
			{
				LastBTTime = std::chrono::steady_clock::now();
				JSONParserConnection->Tick();
			}
			else
			{
				JSONParserConnection.reset();
			}
		}
		else
		{
			auto newBTConnection = listenSocket->Accept();
			if (newBTConnection)
			{
				JSONParserConnection = std::make_shared< SimpleJSONPeerReader >(newBTConnection, sendBTDataTOManager);
				SPP_LOG(LOG_APP, LOG_INFO, "HAS BLUETOOTH CONNECT");
			}
		}
		if (bBTEConnected)
		{
			LastBTTime = std::chrono::steady_clock::now();
		}
		//
		 
		//write status
		{
			Json::Value JsonMessage;
			JsonMessage["COORD"] = coordinator->IsConnected();
			JsonMessage["RESOLVEDSDP"] = (juiceSocket && juiceSocket->IsReady());
			JsonMessage["BLUETOOTH"] = std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - LastBTTime).count() < 1000;

			if (!videoConnection)
			{
				std::string ConnectKey;
				coordinator->GetLocalKeyValue("GUIDCONNECTTO", ConnectKey);
				JsonMessage["CONNSTATUS"] = ConnectKey.empty() ? 0 : 1;
			}
			else
			{
				if (videoConnection->IsConnected())
				{
					JsonMessage["CONNSTATUS"] = 2;
				}
				else
				{
					JsonMessage["CONNSTATUS"] = 1;
				}
			}

			if (Hosts.empty() == false)
			{
				Json::Value HostValues;
				for (auto& [key, value] : Hosts)
				{
					if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - value.LastUpdate).count() < 5)
					{
						auto appCLArgs = std::str_split(value.AppCL, ';');
						if (appCLArgs.empty()) appCLArgs.push_back("");
						for (auto& appCL : appCLArgs)
						{
							Json::Value SingleHost;
							SingleHost["NAME"] = value.Name;
							SingleHost["APPNAME"] = value.AppName;
							SingleHost["APPCL"] = appCL;
							SingleHost["GUID"] = key;
							HostValues.append(SingleHost);
						}						
					}
				}
				JsonMessage["HOSTS"] = HostValues;
			}

			Json::StreamWriterBuilder wbuilder;
			std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

			// our status
			{
				BinaryBlobSerializer outData;
				outData << (uint32_t)StrMessage.length();
				outData.Write(StrMessage.c_str(), StrMessage.length() + 1);
				ipcMem.WriteMemory(outData.GetData(), outData.Size());
			}

			//BinaryBlobSerializer outData;
			//outData << (uint32_t)StrMessage.length();
			//outData.Write(StrMessage.c_str(), StrMessage.length() + 1);
			//ipcMem.WriteMemory(outData.GetData(), outData.Size());

			// app wants to connect
			auto memLock = ipcMem.Lock() + (1 * 1024 * 1024);

			MemoryView inMem(memLock, 1 * 1024 * 1024);
			uint8_t hasData = 0;
			inMem >> hasData;

			if (hasData)
			{
				std::string GUIDStr;
				std::string AppCLStr;

				inMem >> GUIDStr;
				inMem >> AppCLStr;

				SPP_LOG(LOG_APP, LOG_INFO, "JOIN REQUEST!!!: %s:%s", GUIDStr.c_str(), AppCLStr.c_str());

				for (auto& [key, value] : Hosts)
				{
					if (key == GUIDStr)
					{
						if (juiceSocket->HasRemoteSDP() == false)
						{
							coordinator->SetKeyPair("GUIDCONNECTTO", key);
							// set the string we want them to run
							coordinator->SetKeyPair("APPCL", AppCLStr);
						}
					}
				}

				memLock[0] = 0;
			}
			
			ipcMem.Release();
		}

		if (juiceSocket->IsReady())
		{
			coordinator->SetKeyPair("SDP", std::string(juiceSocket->GetSDP_BASE64()));

			if (!videoConnection &&
				std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastRequestJoins).count() > 1)
			{
				{
					auto SQLRequest = std::string_format("SELECT GUID, NAME, APPNAME, APPCL FROM clients WHERE APPNAME != ''");
					coordinator->SQLRequest(SQLRequest.c_str());
				}
				{
					auto SQLRequest = std::string_format("SELECT * FROM clients WHERE GUIDCONNECTTO = '%s'", ThisRUNGUID.c_str());
					coordinator->SQLRequest(SQLRequest.c_str());
				}
				LastRequestJoins = CurrentTime;
			}
		}

		// if we have a connection it handles it all
		if (videoConnection)
		{
			videoConnection->Tick();

			if (videoConnection->IsValid() == false)
			{
				videoConnection.reset();
				juiceSocket = std::make_shared<UDPJuiceSocket>(StunURL.c_str(), StunPort);
				SPP_LOG(LOG_APP, LOG_INFO, "Connection dropped resetting sockets");
			}
			else if (videoConnection->IsConnected())
			{
				coordinator->SetKeyPair("GUIDCONNECTTO", "");
			}
		}
		else
		{
			if (juiceSocket->HasProblem())
			{
				juiceSocket = std::make_shared<UDPJuiceSocket>(StunURL.c_str(), StunPort);
				SPP_LOG(LOG_APP, LOG_INFO, "Resetting juice socket from problem (error on join usually)");
			}
			else if (juiceSocket->IsConnected())
			{
				videoConnection = std::make_shared< VideoConnection >(juiceSocket);
				videoConnection->CreateTranscoderStack(
					// allow reliability to UDP
					std::make_shared< ReliabilityTranscoder >(),
					// push on the splitter so we can ignore sizes
					std::make_shared< MessageSplitTranscoder >());
				// we are the client
				videoConnection->Connect();
			}
		}

		std::this_thread::sleep_for(1ms);
	}
}

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);
	
#if PLATFORM_WINDOWS
	_CrtSetDbgFlag(0);
#endif

	GApp = new MyApp();
	// MyWxApp derives from wxApp
	wxApp::SetInstance(GApp);
	wxEntryStart(argc, argv);
	GApp->CallOnInit();

	std::thread ourApp(SPPApp, argc, argv);

	GApp->OnRun();	
	//GApp->ExitMainLoop();
	GApp->OnExit();
	delete GApp;
	wxEntryCleanup();

	if (ourApp.joinable())
	{
		ourApp.join();
	}

	return 0;
}
