// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


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
#include "SPPFileSystem.h"

#include "SPPOctree.h"

#include "SPPTerrain.h"

#include "SPPPythonInterface.h"

#include "ThreadPool.h"

#include "SPPCEFUI.h"

#include <condition_variable>

#define MAX_LOADSTRING 100

using namespace std::chrono_literals;
using namespace SPP;

class EditorEngine
{
private:
	std::shared_ptr< GraphicsDevice > _graphicsDevice;
	std::shared_ptr<RenderScene> _mainScene;
	std::list< std::shared_ptr<SPP::RenderableMesh> > MeshesToDraw;
	HWND _mainDXWindow = nullptr;
	Vector2 _mouseDelta = Vector2(0, 0);
	uint8_t _keys[255] = { 0 };


	std::chrono::high_resolution_clock::time_point _lastTime;

	std::list <  std::shared_ptr<SPPObject> > cachedObjs;

	std::shared_ptr< SPP::MeshMaterial > meshMat;

	std::shared_ptr< Mesh > _moveGizmo;
	std::shared_ptr< Mesh > _rotateGizmo;
	std::shared_ptr< Mesh > _scaleGizmo;

public:
	void Initialize(void *AppWindow)
	{
		_mainDXWindow = (HWND)AppWindow;

#ifdef _DEBUG
		LoadLibraryA("SPPDX12d.dll");
#else
		LoadLibraryA("SPPDX12.dll");
#endif


		RECT rect;
		GetClientRect(_mainDXWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		_graphicsDevice = GGI()->CreateGraphicsDevice();
		_graphicsDevice->Initialize(WindowSizeX, WindowSizeY, AppWindow);

		_mainScene = GGI()->CreateRenderScene();
		auto& cam = _mainScene->GetCamera();
		//cam.GetCameraPosition()[1] = 100;

		_moveGizmo = std::make_shared< Mesh >();
		_rotateGizmo = std::make_shared< Mesh >();
		_scaleGizmo = std::make_shared< Mesh >();
		
		_moveGizmo->LoadMesh("BlenderFiles/MoveGizmo.ply");
		_rotateGizmo->LoadMesh("BlenderFiles/RotationGizmo.ply");
		//_scaleGizmo->LoadMesh("BlenderFiles/ScaleGizmo.blend");

		auto meshVertexLayout = GGI()->CreateInputLayout();
		meshVertexLayout->InitializeLayout({
				{ "POSITION",  SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,position) },		
				{ "COLOR",  SPP::InputLayoutElementType::UInt8_4, offsetof(SPP::MeshVertex,color) } });

		SPP::MakeResidentAllGPUResources();

		_lastTime = std::chrono::high_resolution_clock::now();
	}

	void Update()
	{
		RECT rect;
		GetClientRect(_mainDXWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		_graphicsDevice->ResizeBuffers(WindowSizeX, WindowSizeY);

		auto CurrentTime = std::chrono::high_resolution_clock::now();
		auto secondTime = std::chrono::duration_cast<std::chrono::microseconds>(CurrentTime - _lastTime).count();
		_lastTime = CurrentTime;

		auto DeltaTime = (float)secondTime * 1.0e-6f;

		if (_mainDXWindow)
		{
			RECT rect;
			GetClientRect(_mainDXWindow, &rect);

			auto Width = rect.right - rect.left;
			auto Height = rect.bottom - rect.top;

			// will be ignored if same
			_graphicsDevice->ResizeBuffers(Width, Height);
		}

		auto& cam = _mainScene->GetCamera();

		//W
		if (_keys[0x57])
			cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Forward);
		if (_keys[0x53])
			cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Back);

		if (_keys[0x41])
			cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Left);
		if (_keys[0x44])
			cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Right);

		_graphicsDevice->BeginFrame();
		_mainScene->Draw();
		_graphicsDevice->EndFrame();
	}

	void KeyDown(uint8_t KeyValue)
	{
		//SPP_QL("kd: %d", KeyValue);
		_keys[KeyValue] = true;
	}

	void KeyUp(uint8_t KeyValue)
	{
		//SPP_QL("ku: %d", KeyValue);
		_keys[KeyValue] = false;
	}

	void MouseDown(int32_t mouseX, int32_t mouseY, SPP::EMouseButton mouseButton)
	{
		//SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
	}
		
	void MouseUp(int32_t mouseX, int32_t mouseY, SPP::EMouseButton mouseButton)
	{
		//SPP_QL("mu: %d %d %d", mouseX, mouseY, mouseButton);
	}
		
	void MouseMove(int32_t mouseX, int32_t mouseY)
	{
		//SPP_QL("mm: %d %d", mouseX, mouseY);
		auto& cam = _mainScene->GetCamera();
		cam.TurnCamera(Vector2(-mouseX, -mouseY));
	}

	void OnResize(int32_t InWidth, int32_t InHeight)
	{
		//_graphicsDevice->ResizeBuffers(InWidth, InHeight);
	}
};

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

	SPP::IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());
	SPP::IntializeGraphics();
	// setup global asset path
	SPP::GAssetPath = stdfs::absolute(stdfs::current_path() / "..\\Assets\\").generic_string();

	{
		auto gameEditor = std::make_unique< EditorEngine >();

		std::thread runCEF([hInstance, editor = gameEditor.get()]()
		{
			SPP::RunBrowser(hInstance,
				"http://spp/assets/web/editor/index.html",
				{
					std::bind(&EditorEngine::Initialize, editor, std::placeholders::_1),
					std::bind(&EditorEngine::Update, editor),
					std::bind(&EditorEngine::OnResize, editor, std::placeholders::_1, std::placeholders::_2),
					nullptr,
				},
				{
					std::bind(&EditorEngine::KeyDown, editor, std::placeholders::_1),
					std::bind(&EditorEngine::KeyUp, editor, std::placeholders::_1),
					std::bind(&EditorEngine::MouseDown, editor, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
					std::bind(&EditorEngine::MouseUp, editor, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
					std::bind(&EditorEngine::MouseMove, editor, std::placeholders::_1, std::placeholders::_2)
				});
		});


		runCEF.join();

	}
	
	return 0;
}


