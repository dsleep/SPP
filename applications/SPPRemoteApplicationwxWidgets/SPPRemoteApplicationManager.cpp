// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPCore.h"
#include "SPPReflection.h"
#include "SPPFileSystem.h"
#include "SPPWin32Core.h"
#include "SPPString.h"
#include "SPPMemory.h"

#include "SPPSerialization.h"
#include "SPPJsonUtils.h"
#include "SPPLogging.h"

#include <thread>  

using namespace SPP;
using namespace std::chrono_literals;

#include <wx/msw/msvcrt.h>
#include <wx/wx.h>
#include <wx/filedlg.h>

enum
{
	TEXT_AppPath = wxID_HIGHEST + 1, // declares an id which will be used to call our button
	TEXT_Args,
	MENU_New,
	MENU_Open,
	MENU_Close,
	MENU_Save,
	MENU_SaveAs,
	MENU_Quit,
	BTN_SelectPath,
	BTN_Start,
	BTN_Stop
};

class MyFrame;

class MyApp : public wxApp
{
private:
	MyFrame* _frame = nullptr;
public:
	virtual bool OnInit();

	MyFrame* GetFrame()
	{
		return _frame;
	}
};

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	wxTextCtrl* MainEditBox = nullptr;
	wxTextCtrl* ArgsEditBox = nullptr;
	wxBoxSizer* MainSizer = nullptr;

	bool _bWorker = false;
	bool _bCoord = false;
	bool _bStun = false;
	uint8_t _connectStatus = 0;

	void UpdateStatus(uint8_t ConnectionStatus, bool Worker, bool Coordinator, bool STUN);

private:
	void OnHello(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);


	void OnButton_SelectPath(wxCommandEvent& event);
	void OnButton_Start(wxCommandEvent& event);
	void OnButton_Stop(wxCommandEvent& event);

	void SaveSettings();

	wxDECLARE_EVENT_TABLE();
};
enum
{
	ID_Hello = 1
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
//EVT_MENU(ID_Hello, MyFrame::OnHello)
//EVT_MENU(wxID_EXIT, MyFrame::OnExit)
//EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
EVT_CLOSE(MyFrame::OnClose)
EVT_BUTTON(BTN_SelectPath, MyFrame::OnButton_SelectPath)
EVT_BUTTON(BTN_Start, MyFrame::OnButton_Start)
EVT_BUTTON(BTN_Stop, MyFrame::OnButton_Stop)
wxEND_EVENT_TABLE()
//wxIMPLEMENT_APP(MyApp);
IMPLEMENT_APP_NO_MAIN(MyApp);

bool MyApp::OnInit()
{
	_frame = new MyFrame("Remote Application Manager", wxPoint(50, 50), wxSize(512, 256));
	_frame->Show(true);
	return true;
}

struct AppConfig
{
	RTTR_ENABLE()

public:
	AppConfig()
	{

	}

	AppConfig(std::string InApplicationPath, std::string InApplicationArguments) :
		ApplicationPath(InApplicationPath),
		ApplicationArguments(InApplicationArguments)
	{
	}

	std::string ApplicationPath;
	std::string ApplicationArguments;
};

RTTR_REGISTRATION
{
	rttr::registration::class_<AppConfig>("AppConfig")
		.constructor()
			(
				rttr::policy::ctor::as_raw_ptr
			)
		.property("ApplicationPath", &AppConfig::ApplicationPath)(rttr::policy::prop::as_reference_wrapper)
		.property("ApplicationArguments", &AppConfig::ApplicationArguments)(rttr::policy::prop::as_reference_wrapper)
	;
}

//SPP_REFLECTION_API void PODToJSON(const rttr::instance& inValue, Json::Value& JsonRoot);
//SPP_REFLECTION_API void JSONToPOD(rttr::instance& inValue, const Json::Value& JsonRoot);

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size)
{
	SetIcon(wxICON(sppapp));

	//wxMenu* menuFile = new wxMenu;
	//menuFile->Append(ID_Hello, "&Hello...\tCtrl-H", "Help string shown in status bar for this menu item");
	//menuFile->AppendSeparator();
	//menuFile->Append(wxID_EXIT);
	//wxMenu* menuHelp = new wxMenu;
	//menuHelp->Append(wxID_ABOUT);
	//wxMenuBar* menuBar = new wxMenuBar;
	//menuBar->Append(menuFile, "&File");
	//menuBar->Append(menuHelp, "&Help");
	//SetMenuBar(menuBar);
	CreateStatusBar();
	SetStatusText("Worker not started...");

	MainSizer = new wxBoxSizer(wxVERTICAL);	

	auto staticText = new wxStaticText(this, wxID_ANY, "Application Path:");
	auto pathSizer = new wxBoxSizer(wxHORIZONTAL);	
	auto pathButton = new wxButton(this, BTN_SelectPath, "...", wxDefaultPosition, wxSize(25,25), 0); 

	MainEditBox = new wxTextCtrl(this, TEXT_AppPath, "", wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);
	pathSizer->Add(MainEditBox, 1, wxEXPAND);
	pathSizer->Add(pathButton);
	
	MainSizer->Add(staticText, 0, wxALL, 5);
	MainSizer->Add(pathSizer, 0, wxEXPAND | wxALL, 5);

	auto staticText2 = new wxStaticText(this, wxID_ANY, "Additional Arguments:");
	ArgsEditBox = new wxTextCtrl(this, TEXT_Args, "", wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);

	MainSizer->Add(staticText2, 0, wxALL, 5);
	MainSizer->Add(ArgsEditBox, 0, wxEXPAND | wxALL, 5);

	auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	auto startButton = new wxButton(this, BTN_Start, "START", wxDefaultPosition, wxDefaultSize, 0); 
	auto stopButton = new wxButton(this, BTN_Stop, "STOP", wxDefaultPosition, wxDefaultSize, 0); 

	buttonSizer->Add(startButton);
	buttonSizer->Add(stopButton);

	MainSizer->Add(buttonSizer, 0, wxALL, 5);

	MainSizer->SetMinSize(512, 100);


	//
	AppConfig appConfig;
	Json::Value jsonData;
	if (FileToJson("./RAMSettings.txt", jsonData))
	{
		JSONToPOD(std::ref(appConfig), jsonData);

		MainEditBox->SetValue(appConfig.ApplicationPath.c_str());
		ArgsEditBox->SetValue(appConfig.ApplicationArguments.c_str());
	}

	SetSizerAndFit(MainSizer);
}

void MyFrame::OnButton_SelectPath(wxCommandEvent& event)
{
	wxFileDialog openFileDialog(this, _("Select exe file"), "", "",	"EXE files (*.exe)|*.exe", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if(openFileDialog.ShowModal() == wxID_CANCEL)
		return;     // the user changed idea...

	auto filePath = openFileDialog.GetPath();
	MainEditBox->SetLabelText(filePath);
}

void MyFrame::OnClose(wxCloseEvent& event)
{
	SaveSettings();
	Destroy();
}

void MyFrame::OnExit(wxCommandEvent& event)
{
	SaveSettings();
	Close(true);
}

void MyFrame::SaveSettings()
{
	AppConfig appConfig( 
		(std::string)MainEditBox->GetValue().mb_str(),
		(std::string)ArgsEditBox->GetValue().mb_str() );

	Json::Value jsonData;
	PODToJSON(std::ref(appConfig), jsonData);
	JsonToFile("./RAMSettings.txt", jsonData);	
}

void MyFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox("This is a wxWidgets' Hello world sample", "About Hello World", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHello(wxCommandEvent& event)
{
	wxLogMessage("Hello world from wxWidgets!");
}

void MyFrame::UpdateStatus(uint8_t ConnectionStatus, bool Worker, bool Coordinator, bool STUN)
{
	bool bDoUpdate = false;

	if (Worker != _bWorker)
	{
		_bWorker = Worker;
		bDoUpdate = true;
	}
	if (Coordinator != _bCoord)
	{
		_bCoord = Coordinator;
		bDoUpdate = true;
	}
	if (STUN != _bStun)
	{
		_bStun = STUN;
		bDoUpdate = true;
	}
	if (ConnectionStatus != _connectStatus)
	{
		_connectStatus = ConnectionStatus;
		bDoUpdate = true;
	}
	if (bDoUpdate)
	{
		std::string ArgString = std::string_format("Worker: %s, Coordinator: %s, STUN: %s, Connection: %s",
			_bWorker ? "GOOD" : "BAD",
			_bCoord ? "GOOD" : "NOT CONNECTED",
			_bStun ? "GOOD" : "NOT CONNECTED",
			_connectStatus ? "GOOD" : "NOT CONNECTED");

		SetStatusText(ArgString.c_str());
	}
}

uint32_t GProcessID = 0;
std::string GIPMemoryID;
std::unique_ptr< std::thread > GWorkerThread;

void WorkerThread(const std::string &AppPath, const std::string &Args)
{
	GIPMemoryID = std::generate_hex(3);
	const int32_t MemSize = 1 * 1024 * 1024;
	IPCMappedMemory ipcMem(GIPMemoryID.c_str(), MemSize, true);

	std::string ArgString = std::string_format("-MEM=%s -APP=\"%s\" -CMDLINE=\"%s\"",
		GIPMemoryID.c_str(),
		AppPath.c_str(),
		Args.c_str());

#if _DEBUG
	GProcessID = CreateChildProcess("applicationhostd.exe",
#else
	GProcessID = CreateChildProcess("applicationhost.exe",
#endif
		ArgString.c_str());

	while (true)
	{
		// do stuff...
		if (!IsChildRunning(GProcessID))
		{
			// child exited/crashed
			auto appInstance = (MyApp*)wxApp::GetInstance();
			if (appInstance && appInstance->GetTopWindow())
			{
				appInstance->GetTopWindow()->GetEventHandler()->CallAfter([appInstance]()
					{
						appInstance->GetFrame()->UpdateStatus(0, false, false, false);
					});
			}
			break;
		}
		else
		{
			auto memAccess = ipcMem.Lock();

			MemoryView inMem(memAccess, MemSize);
			uint32_t dataSize = 0;
			inMem >> dataSize;
			if (dataSize)
			{
				inMem.RebuildViewFromCurrent();
				Json::Value outRoot;
				if (MemoryToJson(inMem.GetData(), dataSize, outRoot))
				{
					auto hasCoord = outRoot["COORD"].asUInt();
					auto resolvedStun = outRoot["RESOLVEDSDP"].asUInt();
					auto conncetionStatus = outRoot["CONNSTATUS"].asUInt();

					auto appInstance = (MyApp*)wxApp::GetInstance();
					appInstance->GetTopWindow()->GetEventHandler()->CallAfter([appInstance, conncetionStatus, hasCoord, resolvedStun]()
						{
							appInstance->GetFrame()->UpdateStatus(conncetionStatus, true, hasCoord, resolvedStun);
						});
				}
			}

			*(uint32_t*)memAccess = 0;
			ipcMem.Release();
			std::this_thread::sleep_for(250ms);
		}
	}
}

void StopThread()
{
	if (GWorkerThread)
	{
		CloseChild(GProcessID);
		if (GWorkerThread->joinable())
		{
			GWorkerThread->join();
		}
		GWorkerThread.reset();
	}
}

void MyFrame::OnButton_Start(wxCommandEvent& event)
{
	StopThread();

	auto appString = std::string(MainEditBox->GetValue().mb_str());
	auto argString = std::string(ArgsEditBox->GetValue().mb_str());

	GWorkerThread.reset(new std::thread(WorkerThread, appString, argString));
}

void MyFrame::OnButton_Stop(wxCommandEvent& event)
{
	StopThread();
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	IntializeCore(nullptr);

#if 1
	_CrtSetDbgFlag(0);
#else
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetBreakAlloc(9554);
	_CrtSetBreakAlloc(9553);
	_CrtSetBreakAlloc(9552);
#endif

	{

		auto ourApp = new MyApp();
		// MyWxApp derives from wxApp
		wxApp::SetInstance(ourApp);
		int argc = 0;
		char** argv = nullptr;
		wxEntryStart(argc, argv);
		ourApp->CallOnInit();

#if 0// _DEBUG
		CreateChildProcess("SPPRemoteApplicationControllerd.exe", "", true);
		CreateChildProcess("simpleconnectioncoordinatord.exe", "", true);
#endif

		ourApp->OnRun();

		StopThread();

		ourApp->OnExit();

		delete ourApp;
		wxEntryCleanup();
	}

	return 0;
}