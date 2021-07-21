// SPPEngine.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <memory>
#include <thread>
#include <filesystem>
#include <optional>

#include "SPPString.h"
#include "SPPEngine.h"
#include "SPPApplication.h"
#include "SPPGPUResources.h"
#include "SPPMesh.h"
#include "SPPLogging.h"
#include "SPPSceneRendering.h"

#include "SPPTerrain.h"

#include "SPPOctree.h"

#define MAX_LOADSTRING 100


using namespace std::chrono_literals;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

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

	SPP::IntializeCore(SPP::wstring_to_utf8(lpCmdLine).c_str());
	srand((unsigned int)time(NULL));

	UNREFERENCED_PARAMETER(lpCmdLine);

	int32_t ErrorCode = 0;

	{
		std::unique_ptr<SPP::ApplicationWindow> app = SPP::CreateApplication();

		auto windowCreated = [&]()
		{

		};

		app->Initialize(800, 600, hInstance);

		auto dx12Device = SPP::CreateGraphicsDevice();
		dx12Device->Initialize(800, 600, app->GetOSWindow());

		// setup global asset path
		SPP::GAssetPath = std::filesystem::absolute(std::filesystem::current_path() / "..\\Assets\\").generic_string();




		{
			auto ourObject = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.DomainSO");
			ourObject->LoadFromDisk("shaders/unlitMesh.hlsl", "main_vs", SPP::EShaderType::Vertex);
			ourObject->LoadFromDisk("shaders/unlitMesh.hlsl", "main_ps", SPP::EShaderType::Pixel);
		}


		auto mainTerrain = SPP::SPPObject::CreateObject< SPP::Terrain >("TerrainMain");

		auto ourObject = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.DomainSO");
		ourObject->LoadFromDisk("shaders/TerrainDS.hlsl", "main", SPP::EShaderType::Domain);

		auto ourObject2 = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.HullSO");
		ourObject2->LoadFromDisk("shaders/TerrainHS.hlsl", "main", SPP::EShaderType::Hull);

		auto terrainHeightMap = SPP::SPPObject::CreateObject< SPP::TextureObject>("TerrainMain.HeightMap");
		terrainHeightMap->LoadFromDisk("textures/terrain/heightmap6.png");

		auto terrainDisplacementMap = SPP::SPPObject::CreateObject< SPP::TextureObject>("TerrainMain.DisplacementMap");
		terrainDisplacementMap->LoadFromDisk("textures/terrain/displacement.png");



		auto mainScene = SPP::CreateRenderScene();
		auto &cam = mainScene->GetCamera();

		//std::function<void(uint8_t)> keyDown;
		//std::function<void(uint8_t)> keyUp;
		//std::function<void(int32_t, int32_t, EMouseButton)> mouseDown;
		//std::function<void(int32_t, int32_t, EMouseButton)> mouseUp;
		//std::function<void(int32_t, int32_t)> mouseMove;

		SPP::Vector2i mouseDelta;
		std::optional<SPP::Vector2i> lastMouse;
		uint8_t keys[255];
				
		SPP::InputEvents inputEvents
		{
			[&keys](uint8_t KeyValue)
			{
				SPP_QL("kd: %d", KeyValue);
				keys[KeyValue] = true;
			},
			[&keys](uint8_t KeyValue)
			{
				SPP_QL("ku: %d", KeyValue);
				keys[KeyValue] = false;
			},
			[](int32_t mouseX, int32_t mouseY, SPP::EMouseButton mouseButton)
			{
				SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
			},
			[](int32_t mouseX, int32_t mouseY, SPP::EMouseButton mouseButton)
			{
				SPP_QL("mu: %d %d %d", mouseX, mouseY, mouseButton);
			},
			[&lastMouse, &mouseDelta](int32_t mouseX, int32_t mouseY)
			{
				SPP_QL("mm: %d %d", mouseX, mouseY);

				if (lastMouse)
				{
					mouseDelta += (SPP::Vector2i(mouseX, mouseY) - lastMouse.value());
				}
				
				lastMouse = SPP::Vector2i(mouseX, mouseY);
			}
		};
		app->SetInputEvents(inputEvents);

		SPP::MakeResidentAllGPUResources();		

		auto LastTime = std::chrono::high_resolution_clock::now();
		float DeltaTime = 16;
		auto msgLoop = [&]()
		{
			auto CurrentTime = std::chrono::high_resolution_clock::now();
			auto secondTime = std::chrono::duration_cast<std::chrono::microseconds>(CurrentTime - LastTime).count();
			LastTime = CurrentTime;

			DeltaTime = (float)secondTime * 1.0e-6f;

			dx12Device->BeginFrame();
			mainScene->Draw();
			dx12Device->EndFrame();
			std::this_thread::sleep_for(0ms);
		};

		app->SetEvents({ msgLoop });

		ErrorCode = app->Run();
	}

	return ErrorCode;
}


