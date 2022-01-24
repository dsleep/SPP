// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPCore.h"
#include "SPPString.h"
#include "SPPMemory.h"
#include "SPPSerialization.h"
#include "SPPJsonUtils.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"


#include "SPPPlatformCore.h"

#include <thread>  

SPP_OVERLOAD_ALLOCATORS

using namespace SPP;
using namespace std::chrono_literals;

#if PLATFORM_WINDOWS
    #include <wx/msw/msvcrt.h>
#endif

#include <wx/wx.h>
#include <wx/filedlg.h>

uint32_t GProcessID = 0;
std::string GIPMemoryID;
std::unique_ptr< std::thread > GWorkerThread;
std::unique_ptr< IPCMappedMemory > GIPCMem;
const int32_t MemSize = 2 * 1024 * 1024;

enum
{
	BTN_Connect = wxID_HIGHEST + 1, // declares an id which will be used to call our button
	BTN_Disconnect,
	LB_ServerList
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

struct HostFromCoord
{
	std::string NAME;
	std::string APPNAME;
	std::string APPCL;
	std::string GUID;
};

inline bool operator==(const HostFromCoord& InA, const HostFromCoord& InB) noexcept
{
	return InA.NAME == InB.NAME &&
		InA.APPNAME == InB.APPNAME &&
		InA.APPCL == InB.APPCL &&
		InA.GUID == InB.GUID;
}

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
		
private:
	wxListBox* _serverList = nullptr;
	std::vector< HostFromCoord> _hostList;

	wxStaticText* _WorkerStatus = nullptr;
	wxStaticText* _BTStatus = nullptr;
	wxStaticText* _ConnectionStatus = nullptr;
	wxStaticText* _CoordStatus = nullptr;
	wxStaticText* _StunStatus = nullptr;

	wxTextCtrl* _passwordTB = nullptr;

	wxButton* _startButton = nullptr;
	void OnButton_Connect(wxCommandEvent& event);
	void OnButton_Disconnect(wxCommandEvent& event);

	bool _bWorker = false;
	bool _bCoord = false;
	bool _bStun = false;
	uint8_t _connectStatus = 0;
	bool _bBT = false;

	wxDECLARE_EVENT_TABLE();

public:
	void UpdateStatus(uint8_t ConnectionStatus, bool Worker, bool Coordinator, bool STUN, bool BT);
	void SetHosts(const std::vector< HostFromCoord >& InHosts);
};
enum
{
	ID_Hello = 1
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_BUTTON(BTN_Connect, MyFrame::OnButton_Connect)
EVT_BUTTON(BTN_Disconnect, MyFrame::OnButton_Disconnect)

wxEND_EVENT_TABLE()
IMPLEMENT_APP_NO_MAIN(MyApp);

bool MyApp::OnInit()
{
	_frame = new MyFrame("Remote Application Controller", wxPoint(50, 50), wxSize(640, 256));
	_frame->Show(true);
	return true;
}

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size)
{
#if PLATFORM_WINDOWS
	SetIcon(wxICON(sppapp));
#endif
	CreateStatusBar();
		
	auto StatusString = std::string_format("TIPS Remote Application Controller : V%s(%s)", SPP::GetGitTag(), SPP::GetGitHash() );
	SetStatusText(StatusString.c_str());

	auto vertSizer = new wxBoxSizer(wxVERTICAL);	

	auto staticText = new wxStaticText(this, wxID_ANY, "Available Servers:");
	
	
	wxArrayString strings;

	// Create a ListBox with Single-selection list.
	_serverList = new wxListBox(this, LB_ServerList, wxDefaultPosition, wxDefaultSize, strings, wxLB_SINGLE);

	vertSizer->Add(staticText, 0, wxLEFT | wxTOP, 5);
	vertSizer->Add(_serverList, 0, wxEXPAND | wxALL, 5);

	//TODO
	//auto staticText2 = new wxStaticText(this, wxID_ANY, "Password:");
	//vertSizer->Add(staticText2, 0, wxLEFT, 5);
	//
	//_passwordTB = new wxTextCtrl(this, wxID_ANY,
	//	L"",
	//	wxDefaultPosition, 
	//	wxDefaultSize, 
	//	wxTE_PASSWORD);

	//vertSizer->Add(_passwordTB, 0, wxEXPAND | wxALL, 5);

	vertSizer->SetSizeHints(this);

	auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	_startButton = new wxButton(this, BTN_Connect, "Connect", wxDefaultPosition, wxDefaultSize, 0);
	auto stopButton = new wxButton(this, BTN_Disconnect, "Disconnect", wxDefaultPosition, wxDefaultSize, 0);

	buttonSizer->Add(_startButton);
	buttonSizer->Add(stopButton, 0, wxLEFT, 5);
	buttonSizer->SetSizeHints(this);
		
	vertSizer->Add(buttonSizer,0, wxALL, 5);

	_WorkerStatus = new wxStaticText(this, wxID_ANY, "Worker: NOT OK");
	_WorkerStatus->SetForegroundColour(wxColour("#ff0000"));
	_BTStatus = new wxStaticText(this, wxID_ANY, "BT: NOT OK");
	_BTStatus->SetForegroundColour(wxColour("#ff0000"));
	_ConnectionStatus = new wxStaticText(this, wxID_ANY, "Connection: NOT OK");
	_ConnectionStatus->SetForegroundColour(wxColour("#ff0000"));
	_CoordStatus = new wxStaticText(this, wxID_ANY, "Coordinator: NOT OK");
	_CoordStatus->SetForegroundColour(wxColour("#ff0000"));
	_StunStatus = new wxStaticText(this, wxID_ANY, "STUN: NOT OK");
	_StunStatus->SetForegroundColour(wxColour("#ff0000"));

	vertSizer->Add(_WorkerStatus, 0, wxALL, 5);
	vertSizer->Add(_BTStatus, 0, wxALL, 5);
	vertSizer->Add(_ConnectionStatus, 0, wxALL, 5);
	vertSizer->Add(_CoordStatus, 0, wxALL, 5);
	vertSizer->Add(_StunStatus, 0, wxALL, 5);

	vertSizer->SetMinSize(640, 100);

	SetSizerAndFit(vertSizer);
}

void MyFrame::UpdateStatus(uint8_t ConnectionStatus, bool Worker, bool Coordinator, bool STUN, bool BT)
{
	bool bDoUpdate = false;

	if (Worker != _bWorker)
	{
		_bWorker = Worker;
		_WorkerStatus->SetLabelText(_bWorker ? "Worker: OK" : "Worker: NOT OK");
		_WorkerStatus->SetForegroundColour(_bWorker ? wxColour("#00ff00") : wxColour("#ff0000") );
	}
	if (Coordinator != _bCoord)
	{
		_bCoord = Coordinator;
		_CoordStatus->SetLabelText(_bCoord ? "Coordinator: OK" : "Coordinator: NOT OK");
		_CoordStatus->SetForegroundColour(_bCoord ? wxColour("#00ff00") : wxColour("#ff0000"));
	}
	if (STUN != _bStun)
	{
		_bStun = STUN;
		_StunStatus->SetLabelText(_bStun ? "STUN: OK" : "STUN: NOT OK");
		_StunStatus->SetForegroundColour(_bStun ? wxColour("#00ff00") : wxColour("#ff0000"));
	}
	if (ConnectionStatus != _connectStatus)
	{
		_connectStatus = ConnectionStatus;
		_ConnectionStatus->SetLabelText(_connectStatus ? "Connection: OK" : "Connection: NOT OK");
		_ConnectionStatus->SetForegroundColour(_connectStatus ? wxColour("#00ff00") : wxColour("#ff0000"));
	}
	if (BT != _bBT)
	{
		_bBT = BT;
		_BTStatus->SetLabelText(_bBT ? "BT: OK" : "BT: NOT OK");
		_BTStatus->SetForegroundColour(_bBT ? wxColour("#00ff00") : wxColour("#ff0000"));
	}
}

void MyFrame::SetHosts(const std::vector< HostFromCoord >& InHosts)
{
	if (_hostList == InHosts) return;
		
	_serverList->Clear();
	wxArrayString strings;
	for (auto& curHost : InHosts)
	{
		stdfs::path pathString(curHost.APPCL.c_str());
		std::string APPCL = curHost.APPCL;
		if (pathString.has_filename())
		{
			APPCL = pathString.filename().generic_string();
		}

		std::string ArgString = std::string_format("(%s):%s=%s",
			curHost.NAME.c_str(),
			curHost.APPNAME.c_str(),
			APPCL.c_str());

		strings.push_back(ArgString.c_str());
	}
	_serverList->Append(strings);	
	_hostList = InHosts;
}


void WorkerThread()
{
	std::string ArgString = std::string_format("-MEM=%s",
		GIPMemoryID.c_str());

#if _DEBUG    
	GProcessID = CreateChildProcess("remoteviewerd",
#else
	GProcessID = CreateChildProcess("remoteviewer",
#endif
		ArgString.c_str(),false);

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
						appInstance->GetFrame()->UpdateStatus(0, false, false, false, false);
					});
			}
			break;
		}
		else
		{
			auto memAccess = GIPCMem->Lock();

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
					auto btConn = outRoot["BLUETOOTH"].asUInt();
					
					auto conncetionStatus = outRoot["CONNSTATUS"].asUInt();
										
					auto appInstance = (MyApp*)wxApp::GetInstance();
					appInstance->GetTopWindow()->GetEventHandler()->CallAfter([appInstance, conncetionStatus, hasCoord, resolvedStun, btConn]()
						{
							appInstance->GetFrame()->UpdateStatus(conncetionStatus, true, hasCoord, resolvedStun, btConn);
						});
					

					Json::Value hostList = outRoot.get("HOSTS", Json::Value::nullSingleton());
					if (!hostList.isNull() && hostList.isArray())
					{
						std::vector< HostFromCoord > hosts;
						for (int32_t Iter = 0; Iter < hostList.size(); Iter++)
						{
							auto currentHost = hostList[Iter];

							hosts.push_back({
								currentHost["NAME"].asCString(),
								currentHost["APPNAME"].asCString(),
								currentHost["APPCL"].asCString(),
								currentHost["GUID"].asCString() });
						}

						auto appInstance = (MyApp*)wxApp::GetInstance();
						appInstance->GetTopWindow()->GetEventHandler()->CallAfter([appInstance, hosts]()
							{
								appInstance->GetFrame()->SetHosts(hosts);
							});
					}
				}
			}		

			*(uint32_t*)memAccess = 0;
			GIPCMem->Release();
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

void MyFrame::OnButton_Connect(wxCommandEvent& event)
{
	auto selection = _serverList->GetSelection();
	if (selection >= 0 && selection < _hostList.size())
	{
		_startButton->SetLabel("Connecting...");
		_startButton->Enable(false);

		uint8_t hasData = ~(uint8_t)0;

		BinaryBlobSerializer outData;
		outData << hasData;
		outData << _hostList[selection].GUID;
		outData << _hostList[selection].APPCL;
		GIPCMem->WriteMemory(outData.GetData(), outData.Size(), 1 * 1024 * 1024);
	}

}

void MyFrame::OnButton_Disconnect(wxCommandEvent& event)
{
	
}

#if PLATFORM_WINDOWS
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
#else
    int main(int argc, char *argv[])
#endif
{
	IntializeCore(nullptr);

	GIPMemoryID = std::generate_hex(3);	
	GIPCMem.reset(new IPCMappedMemory(GIPMemoryID.c_str(), MemSize, true));

#if PLATFORM_WINDOWS
	_CrtSetDbgFlag(0);
#endif
		
	auto ourApp = new MyApp();
	// MyWxApp derives from wxApp
	wxApp::SetInstance(ourApp);
#if PLATFORM_WINDOWS
	int argc = 0;
	char** argv = nullptr;
#endif
	wxEntryStart(argc, argv);
	ourApp->CallOnInit();

	// start thread
	GWorkerThread.reset(new std::thread(WorkerThread));

	ourApp->OnRun();

	StopThread();

	ourApp->OnExit();	
	
	delete ourApp;
	wxEntryCleanup();

	return 0;
}
