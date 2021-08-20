// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include <windows.h>	
#include <GL/gl.h>			

#ifdef SendMessage
#undef SendMessage
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
#include "SPPWin32Core.h"

#pragma comment(lib, "opengl32.lib")

using namespace SPP;

LogEntry LOG_APP("APP");

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string AppName;
	std::string SDP;
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

public:
	SimpleJSONPeerReader(std::shared_ptr< Interface_PeerConnection > InPeer) : _peerLink(InPeer)
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
					streamData.clear();
				}
			}

			if (streamData.size() > 500)
			{
				streamData.clear();
			}
		}
	}

	void MessageReceived(const std::string& InMessage)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "MessageReceived: %s", InMessage.c_str());
	}
};

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

class VideoConnection : public NetworkConnection
{
protected:
	std::unique_ptr< VideoDecodingInterface> VideoDecoder;
	std::unique_ptr<SimpleGlutApp> app;
	uint16_t CreatedWidth;
	uint16_t CreatedHeight;

	HDC hDC;				/* device context */
	HGLRC hRC;				/* opengl context */
	HWND  hWnd;				/* window */

	std::vector<uint8_t> recvBuffer;

public:
	VideoConnection(std::shared_ptr< Interface_PeerConnection > InPeer) : NetworkConnection(InPeer, false) 
	{ 
		app = std::make_unique< SimpleGlutApp>(this);

		hWnd = app->CreateOpenGLWindow("Viewer", 0, 0, 1280, 720, PFD_TYPE_RGBA, 0);

		hDC = GetDC(hWnd);
		hRC = wglCreateContext(hDC);
		wglMakeCurrent(hDC, hRC);

		app->SetupOpenglGLAssets();

		recvBuffer.resize(std::numeric_limits<uint16_t>::max());
	}

	virtual ~VideoConnection()
	{
		wglMakeCurrent(NULL, NULL);
		ReleaseDC(hWnd, hDC);
		wglDeleteContext(hRC);

		app.reset(nullptr);
	}

	virtual void Tick() override
	{
		auto recvAmmount = _peerLink->Receive(recvBuffer.data(), recvBuffer.size());
		if (recvAmmount > 0)
		{
			ReceivedRawData(recvBuffer.data(), recvAmmount, 0);
		}

		NetworkConnection::Tick();
		app->Update();

		if (app->IsDone())
		{
			CloseDown("ViewerWindow Closed");
		}
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
					app->DrawImage(CreatedWidth, CreatedHeight, InData);
				}, VideoSettings{ VidWidth, VidHeight, 4, 3, 32 } );
		}

		DataView.RebuildViewFromCurrent();
		VideoDecoder->Decode(DataView.GetData(), DataView.Size());
	}
};

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


bool ParseCC(const std::string& InCmdLn, const std::string& InValue, std::string& OutValue)
{
	if (StartsWith(InCmdLn, InValue))
	{
		OutValue = std::string(InCmdLn.begin() + InValue.size(), InCmdLn.end());
		return true;
	}
	return false;
}

IPv4_SocketAddress RemoteCoordAddres;
std::string StunURL;
uint16_t StunPort;

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);


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
	AddDLLSearchPath("../3rdParty/libav/bin");

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
	
	auto LastRequestJoins = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	std::unique_ptr<UDP_SQL_Coordinator> coordinator = std::make_unique<UDP_SQL_Coordinator>(RemoteCoordAddres);
	
	coordinator->SetKeyPair("GUID", ThisRUNGUID);
	coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
	coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");
	
	std::shared_ptr< VideoConnection > videoConnection;

	//BLUTETOOTH STUFFS
	std::shared_ptr< SimpleJSONPeerReader > JSONParserConnection;
	std::shared_ptr< BlueToothSocket > listenSocket = std::make_shared<BlueToothSocket>();
	listenSocket->Listen();

	using namespace std::chrono_literals;

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
				Json::Value NameValue = CurrentEle.get("NAME", Json::Value::nullSingleton());
				Json::Value GuidValue = CurrentEle.get("GUID", Json::Value::nullSingleton());

				Hosts[GuidValue.asCString()] = RemoteClient{
					std::chrono::steady_clock::now(),
					std::string(NameValue.asCString()),
					std::string(AppNameValue.asCString()) 
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
				JSONParserConnection = std::make_shared< SimpleJSONPeerReader >(newBTConnection);
				SPP_LOG(LOG_APP, LOG_INFO, "HAS BLUETOOTH CONNECT");
			}
		}
		//

		//write status
		{
			Json::Value JsonMessage;
			JsonMessage["COORD"] = coordinator->IsConnected();
			JsonMessage["RESOLVEDSDP"] = (juiceSocket && juiceSocket->IsReady());
			JsonMessage["BLUETOOTH"] = (!!JSONParserConnection);

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
						Json::Value SingleHost;
						SingleHost["NAME"] = value.Name;
						SingleHost["APPNAME"] = value.AppName;
						SingleHost["GUID"] = key;
						HostValues.append(SingleHost);
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
			char GUIDToJoin[7];
			ipcMem.ReadMemory(GUIDToJoin, 6, 1 * 1024 * 1024);
			GUIDToJoin[6] = 0;

			if (GUIDToJoin[0])
			{
				std::string GUIDStr = GUIDToJoin;
				SPP_LOG(LOG_APP, LOG_INFO, "JOIN REQUEST!!!: %s", GUIDStr.c_str());

				for (auto& [key, value] : Hosts)
				{
					if (key == GUIDStr)
					{
						if (juiceSocket->HasRemoteSDP() == false)
						{
							coordinator->SetKeyPair("GUIDCONNECTTO", key);
						}
					}
				}
			}			
						
			memset(GUIDToJoin, 0, 6);
			ipcMem.WriteMemory(GUIDToJoin, 6, 1 * 1024 * 1024);
		}

		if (juiceSocket->IsReady())
		{
			coordinator->SetKeyPair("SDP", std::string(juiceSocket->GetSDP_BASE64()));

			if (!videoConnection &&
				std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastRequestJoins).count() > 1)
			{
				{
					auto SQLRequest = std::string_format("SELECT GUID, NAME, APPNAME FROM clients WHERE APPNAME != ''");
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

	return 0;
}