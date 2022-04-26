// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// 
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <memory>
#include <thread>
#include <optional>

#include "SPPString.h"
#include "SPPEngine.h"
#include "SPPApplication.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPTextures.h"
#include "SPPMesh.h"
#include "SPPLogging.h"
#include "SPPSceneRendering.h"
#include "SPPOctree.h"
#include "SPPTerrain.h"
#include "SPPPythonInterface.h"
#include "ThreadPool.h"
#include "SPPFileSystem.h"

#include <condition_variable>

#define MAX_LOADSTRING 100

using namespace SPP;
using namespace std::chrono_literals;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

#ifdef _DEBUG
	LoadLibraryA("SPPVulkand.dll");
#else
	LoadLibraryA("SPPVulkan.dll");
#endif

	//Alloc Console
	//print some stuff to the console
	//make sure to include #include "stdio.h"
	//note, you must use the #include <iostream>/ using namespace std
	//to use the iostream... #incldue "iostream.h" didn't seem to work
	//in my VC 6
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");

	IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());
	IntializeGraphics();	

	// setup global asset path
	SPP::GRootPath = stdfs::absolute(stdfs::current_path() / "..\\").generic_string();
	SPP::GBinaryPath = SPP::GRootPath + "Binaries\\";
	SPP::GAssetPath = SPP::GRootPath + "Assets\\";

	//SPP::CallPython();
	int ErrorCode = 0;		
	{
		std::unique_ptr<SPP::ApplicationWindow> app = SPP::CreateApplication();

		app->Initialize(1280, 720, hInstance);

		auto graphicsDevice = GGI()->CreateGraphicsDevice();
		graphicsDevice->Initialize(1280, 720, app->GetOSWindow());
				
		MakeResidentAllGPUResources();		

		std::mutex tickMutex;
		std::condition_variable cv;		
		
		auto LastTime = std::chrono::high_resolution_clock::now();
		float DeltaTime = 0.016f;
		auto msgLoop = [&]()
		{
			

			graphicsDevice->BeginFrame();
			//mainScene->Draw();
			graphicsDevice->EndFrame();

			

			std::this_thread::sleep_for(0ms);
		};

		app->SetEvents({ msgLoop });

		ErrorCode = app->Run();
	}

	return ErrorCode;
}


