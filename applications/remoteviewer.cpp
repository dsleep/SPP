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
#include "SPPHandledTimers.h"

#include "SPPStackUtils.h"

#include <set>

#include "SPPMath.h"

#include "SPPVideo.h"
#include "SPPNetworkConnection.h"
#include "SPPNetworkMessenger.h"
#include "SPPMemory.h"
#include "SPPApplication.h"

#include "SPPPlatformCore.h"

#include <iostream>
#include <mutex>

#include <wx/wx.h>
#include <wx/glcanvas.h>

//#if PLATFORM_WINDOWS && HAS_WINRT
//	#include "SPPWinRTBTE.h"
//#endif

//#if PLATFORM_MAC
//    #include "SPPMacBT.h"
//#endif

#include "SPPReflection.h"

#include "./SPPRemoteApplicationwxWidgets/SPP_RD_AppConfig.inl"

APPConfig GAppConfig;

SPP_OVERLOAD_ALLOCATORS

using namespace SPP;

LogEntry LOG_APP("APP");


class MyApp;
MyApp* GApp = nullptr;

std::string GConnectionPWDFromCMD;

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
static std::atomic_bool bDCRequest{ 0 };

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
		
		auto oParent = GetParent();

		if (!oParent->IsShown())
		{
			oParent->Show();
		}

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
	void mouseEvent(wxMouseEvent& event);

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
EVT_MOTION(BasicGLPane::mouseEvent)

EVT_LEFT_DOWN(BasicGLPane::mouseEvent)
EVT_LEFT_UP(BasicGLPane::mouseEvent)
EVT_RIGHT_DOWN(BasicGLPane::mouseEvent)
EVT_RIGHT_UP(BasicGLPane::mouseEvent)
EVT_MIDDLE_DOWN(BasicGLPane::mouseEvent)
EVT_MIDDLE_UP(BasicGLPane::mouseEvent)

EVT_ENTER_WINDOW(BasicGLPane::mouseEvent)
EVT_LEAVE_WINDOW(BasicGLPane::mouseEvent)
EVT_MOUSEWHEEL(BasicGLPane::mouseEvent)

EVT_SIZE(BasicGLPane::resized)
EVT_KEY_DOWN(BasicGLPane::keyPressed)
EVT_KEY_UP(BasicGLPane::keyReleased)
EVT_PAINT(BasicGLPane::render)
END_EVENT_TABLE()

const uint8_t KeyBoardMessage = 0x02;
const uint8_t MouseMessage = 0x03;
const uint8_t BTMessage = 0x04;

const uint8_t MS_ButtonOrMove = 0x01;
const uint8_t MS_Window = 0x02;

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

	auto eventType = event.GetEventType();
	bool bIsButtonEvent = event.IsButton() || (eventType == wxEVT_MOUSEWHEEL);
	bool bIsMoveEvent = (eventType == wxEVT_MOTION);
	bool bWindowEvent = (eventType == wxEVT_ENTER_WINDOW || eventType == wxEVT_LEAVE_WINDOW);

	//SPP_LOG(LOG_APP, LOG_INFO, "_MouseEvent: %d", eventType);
	
	if (bWindowEvent)
	{
		BinaryBlobSerializer thisMessage;
		thisMessage << MouseMessage;
		thisMessage << MS_Window;

		int32_t xPos = event.GetX() * GetContentScaleFactor();
		int32_t yPos = event.GetY() * GetContentScaleFactor();
		Vector3 remap(xPos, yPos, 1);
		Vector3 RemappedPosition = remap * virtualToReal;

		uint8_t EnterState = (eventType == wxEVT_ENTER_WINDOW) ? 1 : 0;
		thisMessage << EnterState;
		thisMessage << (uint16_t)RemappedPosition[0];
		thisMessage << (uint16_t)RemappedPosition[1];

		GInputHandler.PushEvent(thisMessage.GetArray());
	}
	else if (bIsButtonEvent || bIsMoveEvent)
	{
		int32_t curButton = event.GetButton();			
		int32_t wheelDelta = event.GetWheelRotation();
		int8_t mouseWheel = (int8_t)std::clamp< int32_t>(wheelDelta, std::numeric_limits< int8_t>::min(), std::numeric_limits<int8_t>::max());

		uint8_t bDown = 0;
		uint8_t ActualButton = 0;
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

		int32_t xPos = event.GetX() * GetContentScaleFactor();
		int32_t yPos = event.GetY() * GetContentScaleFactor();

		Vector3 remap(xPos, yPos, 1);
		Vector3 RemappedPosition = remap * virtualToReal;

		if (RemappedPosition[0] >= 0 && RemappedPosition[1] >= 0)
		{
			BinaryBlobSerializer thisMessage;
			thisMessage << MouseMessage;
			thisMessage << MS_ButtonOrMove;

			thisMessage << ActualButton;
			thisMessage << bDown;
			thisMessage << DownState;
			thisMessage << (uint16_t)RemappedPosition[0];
			thisMessage << (uint16_t)RemappedPosition[1];
			thisMessage << mouseWheel;

			GInputHandler.PushEvent(thisMessage.GetArray());
		}
	}
}

// some useful events to use
void BasicGLPane::mouseEvent(wxMouseEvent& event) 
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
	return GetSize().x * GetContentScaleFactor();
}

int BasicGLPane::getHeight()
{
    
	return GetSize().y * GetContentScaleFactor();
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


class MyFrame : public wxFrame
{
private:

public:
	/*  The class constructor takes as parameters: the Display Manager used in
	 *  the application (declared in MyApp), the Environment which contains
	 *  the support and graphs already loaded and the name of the file containing
	 *  the graph to be edited. */
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) : wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		

	}
	~MyFrame()
	{
	}
	void OnClose(wxCloseEvent& event)
	{
		bDCRequest = true;
		Show(false);
		event.Veto();
	}
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_CLOSE(MyFrame::OnClose)
END_EVENT_TABLE()

class MyApp : public wxApp
{
private:
	bool OnInit();
	MyFrame* _frame = nullptr;
	BasicGLPane* _glPane = nullptr;

public:
	wxFrame* GetFrame()
	{
		return _frame;
	}
	BasicGLPane* GetGLPane()
	{
		return _glPane;
	}
};

bool MyApp::OnInit()
{
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
	_frame = new MyFrame(wxT("Remote Viewer"), wxPoint(50, 50), wxSize(1280, 720));
#if PLATFORM_WINDOWS
	_frame->SetIcon(wxICON(sppapp));
#endif
	int args[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0 };

	_glPane = new BasicGLPane(_frame, args);
	sizer->Add(_glPane, 1, wxEXPAND);

	_frame->SetSizer(sizer);
	_frame->SetAutoLayout(true);

	_frame->Show(false);
	_frame->Show(true);
	_frame->Show(false);

	return true;
}

IMPLEMENT_APP_NO_MAIN(MyApp);

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

	//std::function<void(const void*, uint16_t)> _btSendMessage;
	//using BtSendFunc = decltype(_btSendMessage);

public:
	VideoConnection(std::shared_ptr< Interface_PeerConnection > InPeer/*, BtSendFunc InSendBTMessage*/) : NetworkConnection(InPeer, false) //, _btSendMessage(InSendBTMessage)
	{ 
		SetPassword(GConnectionPWDFromCMD);
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
		auto doDC = bDCRequest.exchange(false);

		if (doDC)
		{
			CloseDown("Clicked Close");
			NetworkConnection::Tick();
			return;
		}

		if (!_peerLink->IsWrappedPeer())
		{
			int32_t recvAmmount = 0;
			while ((recvAmmount = _peerLink->Receive(recvBuffer.data(), recvBuffer.size())) > 0)
			{
				ReceivedRawData(recvBuffer.data(), recvAmmount, 0);
			}
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

		uint8_t MessageType = 0;

		DataView >> MessageType;

		if (MessageType == 1)
		{
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
					}, VideoSettings{ VidWidth, VidHeight, 4, 3, 32 });
			}

			DataView.RebuildViewFromCurrent();
			VideoDecoder->Decode(DataView.GetData(), DataView.Size());
		}
		//else if (MessageType == 2)
		//{
		//	if (_btSendMessage)
		//	{
		//		BinaryBlobSerializer thisMessage;
		//		thisMessage << (uint8_t)1;
		//		_btSendMessage(thisMessage.GetData(), thisMessage.Size());
		//	}
		//}
	}
};

/// <summary>
/// 
/// </summary>
/// <param name="ThisRUNGUID"></param>
/// <param name="LanAddr"></param>
void MainWithLanOnly(const std::string& ThisRUNGUID, const std::string &LanAddr)//, IPCMappedMemory& ipcMem)
{			
	std::shared_ptr<UDPSocket> serverSocket = std::make_shared<UDPSocket>();
	std::shared_ptr< UDPSendWrapped > videoSocket = std::make_shared<UDPSendWrapped>(serverSocket, IPv4_SocketAddress(LanAddr.c_str()));

	std::shared_ptr< VideoConnection > videoConnection = std::make_shared< VideoConnection >(videoSocket);
	videoConnection->CreateTranscoderStack(
		// allow reliability to UDP
		std::make_shared< ReliabilityTranscoder >(),
		// push on the splitter so we can ignore sizes
		std::make_shared< MessageSplitTranscoder >());
	// we are the client
	videoConnection->Connect();
	
	TimerController mainController(16ms);

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(std::numeric_limits<uint16_t>::max());
		
	//VIDEO UPDATES
	mainController.AddTimer(16.6666ms, true, [&]()
		{
			auto CurrentTime = std::chrono::high_resolution_clock::now();

			IPv4_SocketAddress recvAddr;
			int32_t DataRecv = 0;
			while ((DataRecv = serverSocket->ReceiveFrom(recvAddr, BufferRead.data(), BufferRead.size())) > 0)
			{
				if (videoConnection &&
					videoSocket->GetRemoteAddress() == recvAddr)
				{
					videoConnection->ReceivedRawData(BufferRead.data(), DataRecv, 0);
				}
			}

			// if we have a connection it handles it all
			if (videoConnection)
			{
				videoConnection->Tick();

				if (videoConnection->IsValid() == false)
				{
					videoConnection.reset();
					SPP_LOG(LOG_APP, LOG_INFO, "Connection dropped resetting...");
				}
			}
		});

	mainController.Run();
}

struct IPCMotionState
{
	int32_t buttonState[2];
	float motionXY[2];
	float orientationQuaternion[4];
};

/// <summary>
/// 
/// </summary>
/// <param name="ThisRUNGUID"></param>
/// <param name="IncomingGUID"></param>
void MainWithNatTraverasl(const std::string& ThisRUNGUID, const std::string &IncomingGUID, const std::string &InMemshareID)
{
	auto LastBTTime = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	auto LastRequestJoins = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	using namespace std::chrono_literals;

	std::shared_ptr< VideoConnection > videoConnection;

	std::unique_ptr< IPCMappedMemory> appIPC;
	std::unique_ptr< SimpleIPCMessageQueue<IPCMotionState> > msgQueue;
	appIPC = std::make_unique<IPCMappedMemory>(InMemshareID.c_str(), sizeof(IPCMotionState) * 200, true);
	msgQueue = std::make_unique< SimpleIPCMessageQueue<IPCMotionState> >(*appIPC);
		

	auto juiceSocket = std::make_shared<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
	std::unique_ptr<UDP_SQL_Coordinator> coordinator = std::make_unique<UDP_SQL_Coordinator>(GAppConfig.coord.addr.c_str());

	coordinator->SetPassword(GAppConfig.coord.pwd);
	coordinator->SetKeyPair("GUID", ThisRUNGUID);
	coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
	coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");
	coordinator->SetKeyPair("GUIDCONNECTTO", IncomingGUID);

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(std::numeric_limits<uint16_t>::max());

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
			}
		});


	TimerController mainController(16ms);

	// COORDINATOR UPDATES
	mainController.AddTimer(50ms, true, [&]()
		{
			coordinator->Update();
		});

	//JUICE UPDATES
	mainController.AddTimer(33ms, true, [&]()
		{
			auto CurrentTime = std::chrono::high_resolution_clock::now();
			if (juiceSocket->IsReady())
			{
				coordinator->SetKeyPair("SDP", std::string(juiceSocket->GetSDP_BASE64()));

				if (!videoConnection &&
					std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastRequestJoins).count() > 1)
				{
					auto SQLRequest = std::string_format("SELECT * FROM clients WHERE GUID = '%s'", IncomingGUID.c_str());
					coordinator->SQLRequest(SQLRequest.c_str());					
					LastRequestJoins = CurrentTime;
				}
			}
		});

	// CHECK IPC
	mainController.AddTimer(3ms, true, [&]()
		{
			auto IPCMessages = msgQueue->GetMessages();
			for (const auto& curMessage : IPCMessages)
			{
				if (videoConnection && videoConnection->IsValid())
				{
					//BTMessage
					BinaryBlobSerializer thisMessage;
					thisMessage << BTMessage;
					thisMessage.Write(&curMessage, sizeof(IPCMotionState));

					videoConnection->SendMessage(thisMessage.GetData(), thisMessage.Tell(), EMessageMask::IS_RELIABLE);					
				}
			}
		});

	//VIDEO UPDATES
	mainController.AddTimer(41.6ms, true, [&]()
		{
			auto CurrentTime = std::chrono::high_resolution_clock::now();
			// if we have a connection it handles it all
			if (videoConnection)
			{
				videoConnection->Tick();

				if (videoConnection->IsValid() == false)
				{
					videoConnection.reset();
					juiceSocket = std::make_shared<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
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
					juiceSocket = std::make_shared<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
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
		});

	mainController.Run();
}

void SPPApp(int argc, char* argv[])
{
	{
		Json::Value jsonData;
		SE_ASSERT(FileToJson("./remotecontrol.config.txt", jsonData));
		auto coordRef = std::ref(GAppConfig);
		JSONToPOD(coordRef, jsonData);
	}

	auto ThisRUNGUID = std::generate_hex(3);
#if PLATFORM_WINDOWS
	AddDLLSearchPath("../3rdParty/libav/bin");
#endif

	auto CCMap = std::BuildCCMap(argc, argv);
	auto lanAddr = MapFindOrDefault(CCMap, "lanaddr");
	auto connectionGUID = MapFindOrDefault(CCMap, "connectionID");
	auto memshareID = MapFindOrDefault(CCMap, "MEMSHARE");

	GConnectionPWDFromCMD = MapFindOrDefault(CCMap, "pwd");

	SPP_LOG(LOG_APP, LOG_INFO, "RUN GUID: %s MEM SHARE: %s", ThisRUNGUID.c_str(), memshareID.c_str());

	// START OS NETWORKING
	GetOSNetwork();

	//
	//if (lanAddr.length())
	//{
	//	MainWithLanOnly(ThisRUNGUID, lanAddr);
	//}
	//else if(connectionGUID.length())
	{
		MainWithNatTraverasl(ThisRUNGUID, connectionGUID, memshareID);
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

	std::thread ourApp([argc, argv]()
		{
			SPPApp(argc, argv);
		});

	GApp->OnRun();

	GApp->OnExit();
	delete GApp;
	wxEntryCleanup();

	if (ourApp.joinable())
	{
		ourApp.join();
	}

	return 0;
}
