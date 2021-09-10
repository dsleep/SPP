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
	LoadLibraryA("SPPOpenGLd.dll");
#else
	LoadLibraryA("SPPOpenGL.dll");
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
	SPP::GAssetPath = stdfs::absolute(stdfs::current_path() / "..\\Assets\\").generic_string();

	//SPP::CallPython();
	int ErrorCode = 0;		
	{
		std::unique_ptr<SPP::ApplicationWindow> app = SPP::CreateApplication();

		app->Initialize(1280, 720, hInstance, AppFlags::SupportOpenGL);

		auto openGLDevice = GGI()->CreateGraphicsDevice();
		openGLDevice->Initialize(1280, 720, app->GetOSWindow());

		//auto pixelShader = GGI()->CreateShader(EShaderType::Pixel);
		//pixelShader->CompileShaderFromFile("shaders/OpenGL/FullScene.hlsl","mainImage");

		auto mainScene = GGI()->CreateRenderScene();
		auto& cam = mainScene->GetCamera();
		cam.GetCameraPosition()[2] = -100;
	
		SPP::Vector2 mouseDelta(0,0);
		uint8_t keys[255] = { 0 };
				
		SPP::InputEvents inputEvents
		{
			[&keys](uint8_t KeyValue)
			{
				//SPP_QL("kd: %d", KeyValue);
				keys[KeyValue] = true;
			},
			[&keys](uint8_t KeyValue)
			{
				//SPP_QL("ku: %d", KeyValue);
				keys[KeyValue] = false;
			},
			[](int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
			{
				//SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
			},
			[](int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
			{
				//SPP_QL("mu: %d %d %d", mouseX, mouseY, mouseButton);
			},
			[&mouseDelta](int32_t mouseX, int32_t mouseY)
			{
				//SPP_QL("mm: %d %d", mouseX, mouseY);
				mouseDelta -= SPP::Vector2(mouseX, mouseY);
			}
		};
		app->SetInputEvents(inputEvents);

						
		MakeResidentAllGPUResources();		

		std::mutex tickMutex;
		std::condition_variable cv;		
		
		auto LastTime = std::chrono::high_resolution_clock::now();
		float DeltaTime = 0.016f;
		auto msgLoop = [&]()
		{
			auto CurrentTime = std::chrono::high_resolution_clock::now();
			auto secondTime = std::chrono::duration_cast<std::chrono::microseconds>(CurrentTime - LastTime).count();
			LastTime = CurrentTime;

			DeltaTime = (float)secondTime * 1.0e-6f;

			//W
			if (keys[0x57])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Forward);
			if (keys[0x53])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Back);

			if (keys[0x41])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Left);
			if (keys[0x44])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Right);
			
			cam.TurnCamera(mouseDelta);
			mouseDelta = SPP::Vector2(0, 0);

			openGLDevice->BeginFrame();
			mainScene->Draw();
			openGLDevice->EndFrame();
		};

		app->SetEvents({ msgLoop });

		ErrorCode = app->Run();
	}

	return ErrorCode;
}


