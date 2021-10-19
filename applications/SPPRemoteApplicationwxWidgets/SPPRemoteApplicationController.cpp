// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPCore.h"
#include "SPPWin32Core.h"
#include "SPPString.h"
#include "SPPMemory.h"

#include <thread>  

using namespace SPP;

#include <wx/msw/msvcrt.h>
#include <wx/wx.h>
#include <wx/filedlg.h>

enum
{
	BTN_Connect = wxID_HIGHEST + 1, // declares an id which will be used to call our button
	BTN_Disconnect,
	LB_ServerList
};

class MyApp : public wxApp
{
public:
	virtual bool OnInit();
};

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	
private:
	wxListBox* _serverList = nullptr;

	void OnButton_Connect(wxCommandEvent& event);
	void OnButton_Disconnect(wxCommandEvent& event);
	wxDECLARE_EVENT_TABLE();
};
enum
{
	ID_Hello = 1
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_BUTTON(BTN_Connect, MyFrame::OnButton_Connect)
EVT_BUTTON(BTN_Disconnect, MyFrame::OnButton_Disconnect)

wxEND_EVENT_TABLE()
//wxIMPLEMENT_APP(MyApp);
IMPLEMENT_APP_NO_MAIN(MyApp);

bool MyApp::OnInit()
{
	MyFrame* frame = new MyFrame("Remote Application Controller", wxPoint(50, 50), wxSize(512, 256));
	frame->Show(true);
	return true;
}

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size)
{
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

	auto vertSizer = new wxBoxSizer(wxVERTICAL);	

	auto staticText = new wxStaticText(this, wxID_ANY, "Available Servers:");
	
	wxArrayString strings;
	strings.Add(wxT("First string"));
	strings.Add(wxT("Second string"));
	strings.Add(wxT("Third string"));
	strings.Add(wxT("Fourth string"));
	strings.Add(wxT("Fifth string"));
	strings.Add(wxT("Sixth string"));

	// Create a ListBox with Single-selection list.
	_serverList = new wxListBox(this, LB_ServerList, wxDefaultPosition, wxDefaultSize, strings, wxLB_SINGLE);

	vertSizer->Add(staticText);
	vertSizer->Add(_serverList, 0, wxEXPAND);

	auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	auto startButton = new wxButton(this, BTN_Connect, "Connect", wxDefaultPosition, wxDefaultSize, 0);
	auto stopButton = new wxButton(this, BTN_Disconnect, "Disconnect", wxDefaultPosition, wxDefaultSize, 0);

	buttonSizer->Add(startButton);
	buttonSizer->Add(stopButton);

		
	vertSizer->Add(buttonSizer);

	vertSizer->SetMinSize(512, 100);

	SetSizerAndFit(vertSizer);
}

uint32_t GProcessID = 0;
std::string GIPMemoryID;
std::unique_ptr< std::thread > GWorkerThread;

void WorkerThread(const std::string &AppPath, const std::string &Args)
{
	std::string ArgString = std::string_format("-MEM=%s -APP=\"%s\" -CMDLINE=\"%s\"",
		GIPMemoryID.c_str(),
		AppPath.c_str(),
		Args.c_str());

#if DEBUG
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
			return;
		}
	}
}

void StopThread()
{
	if (GWorkerThread)
	{
		CloseChild(GProcessID);
		GWorkerThread->join();
		GWorkerThread.reset();
	}
}

void MyFrame::OnButton_Connect(wxCommandEvent& event)
{
	StopThread();

	//auto appString = std::string(MainEditBox->GetValue().mb_str());
	//auto argString = std::string(ArgsEditBox->GetValue().mb_str());

	//GWorkerThread.reset(new std::thread(WorkerThread, appString, argString));
}

void MyFrame::OnButton_Disconnect(wxCommandEvent& event)
{
	StopThread();
}

int main(int argc, char* argv[])
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

	GIPMemoryID = std::generate_hex(3);
	IPCMappedMemory ipcMem(GIPMemoryID.c_str(), 1 * 1024 * 1024, true);
		
	auto ourApp = new MyApp();
	// MyWxApp derives from wxApp
	wxApp::SetInstance(ourApp);
	wxEntryStart(argc, argv);
	ourApp->CallOnInit();
	ourApp->OnRun();
	ourApp->OnExit();	
	
	delete ourApp;
	wxEntryCleanup();

	return 0;
}