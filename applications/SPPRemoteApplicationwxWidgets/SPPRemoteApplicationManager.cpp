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

class MyApp : public wxApp
{
public:
	virtual bool OnInit();
};

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	wxTextCtrl* MainEditBox = nullptr;
	wxTextCtrl* ArgsEditBox = nullptr;
	wxBoxSizer* MainSizer = nullptr;

	void OnButton_SelectPath(wxCommandEvent& event);
	void OnButton_Start(wxCommandEvent& event);
	void OnButton_Stop(wxCommandEvent& event);

private:
	void OnHello(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	wxDECLARE_EVENT_TABLE();
};
enum
{
	ID_Hello = 1
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_Hello, MyFrame::OnHello)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
EVT_BUTTON(BTN_SelectPath, MyFrame::OnButton_SelectPath)
EVT_BUTTON(BTN_Start, MyFrame::OnButton_Start)
EVT_BUTTON(BTN_Stop, MyFrame::OnButton_Stop)
wxEND_EVENT_TABLE()
//wxIMPLEMENT_APP(MyApp);
IMPLEMENT_APP_NO_MAIN(MyApp);

bool MyApp::OnInit()
{
	MyFrame* frame = new MyFrame("Remote Application Manager", wxPoint(50, 50), wxSize(512, 256));
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

	MainSizer = new wxBoxSizer(wxVERTICAL);	

	auto staticText = new wxStaticText(this, wxID_ANY, "Application Path:");
	auto pathSizer = new wxBoxSizer(wxHORIZONTAL);	
	auto pathButton = new wxButton(this, BTN_SelectPath, "...", wxDefaultPosition, wxSize(25,25), 0); 

	MainEditBox = new wxTextCtrl(this, TEXT_AppPath, "", wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);
	pathSizer->Add(MainEditBox, 1, wxEXPAND);
	pathSizer->Add(pathButton);
	MainSizer->Add(staticText);
	MainSizer->Add(pathSizer, 0, wxEXPAND);

	auto staticText2 = new wxStaticText(this, wxID_ANY, "Additional Arguments:");
	ArgsEditBox = new wxTextCtrl(this, TEXT_Args, "", wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxTextCtrlNameStr);

	MainSizer->Add(staticText2);
	MainSizer->Add(ArgsEditBox, 0, wxEXPAND);

	auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	auto startButton = new wxButton(this, BTN_Start, "START", wxDefaultPosition, wxDefaultSize, 0); 
	auto stopButton = new wxButton(this, BTN_Stop, "STOP", wxDefaultPosition, wxDefaultSize, 0); 

	buttonSizer->Add(startButton);
	buttonSizer->Add(stopButton);

	MainSizer->Add(buttonSizer);

	MainSizer->SetMinSize(512, 100);

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

void MyFrame::OnExit(wxCommandEvent& event)
{
	Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox("This is a wxWidgets' Hello world sample", "About Hello World", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHello(wxCommandEvent& event)
{
	wxLogMessage("Hello world from wxWidgets!");
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