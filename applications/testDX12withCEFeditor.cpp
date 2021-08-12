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
public:
	void Initialize(void *AppWindow)
	{
		_mainDXWindow = (HWND)AppWindow;

		_graphicsDevice = GGI()->CreateGraphicsDevice();
		_graphicsDevice->Initialize(1280, 720, AppWindow);

		_mainScene = GGI()->CreateRenderScene();
		auto& cam = _mainScene->GetCamera();
		cam.GetCameraPosition()[1] = 100;

#if 0
		auto mesh = SPP::SPPObject::CreateObject<SPP::Mesh>("mesh.gunmesh");
		mesh->LoadMesh("meshes/circle.obj");


		//meshlets
		auto meshletSimpleAS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.meshletAS");
		meshletSimpleAS->LoadFromDisk("shaders/SimpleMeshletAS.hlsl", "main", SPP::EShaderType::Amplification);

		auto meshletSimpleMS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.meshletMS");
		meshletSimpleMS->LoadFromDisk("shaders/SimpleMeshletMS.hlsl", "main", SPP::EShaderType::Mesh);

		auto meshletSimplePS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.meshletPS");
		meshletSimplePS->LoadFromDisk("shaders/SimpleMeshletPS.hlsl", "main", SPP::EShaderType::Pixel);

		cachedObjs.push_back(meshletSimpleAS);
		cachedObjs.push_back(meshletSimpleMS);
		cachedObjs.push_back(meshletSimplePS);

		//auto meshMaterialMeshShaders = SPP::SPPObject::CreateObject<SPP::MaterialObject>("bleep.materialmesh");
		//meshMaterialMeshShaders->meshShader = meshletSimpleMS;
		//meshMaterialMeshShaders->pixelShader = meshletSimplePS;

		//auto meshMatMeshShader = std::make_shared<SPP::MeshMaterial>();		

		//auto meshMaterial = SPP::SPPObject::CreateObject<SPP::MaterialObject>("bleep.material");
		//auto meshVSShader = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.material.vs");
		//{			
		//	meshVSShader->LoadFromDisk("shaders/unlitMesh.hlsl", "main_vs", SPP::EShaderType::Vertex);
		//	meshMaterial->vertexShader = meshVSShader;
		//}

		//auto meshPSShader = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.material.ps");
		//{
		//	meshPSShader->LoadFromDisk("shaders/unlitMesh.hlsl", "main_ps", SPP::EShaderType::Pixel);
		//	meshMaterial->pixelShader = meshPSShader;
		//}

		//auto textureTest = SPP::SPPObject::CreateObject<SPP::TextureObject>("bleep.texture");
		//textureTest->LoadFromDisk("texture/cerberus_A.png");

		meshMat = std::make_shared<SPP::MeshMaterial>();


		meshMat->meshShader = meshletSimpleMS->GetGPUShader();
		meshMat->amplificationShader = meshletSimpleAS->GetGPUShader();
		meshMat->pixelShader = meshletSimplePS->GetGPUShader();

		//meshMat->vertexShader = meshMaterial->vertexShader->GetGPUShader();
		//meshMat->pixelShader = meshMaterial->pixelShader->GetGPUShader();

		auto meshVertexLayout = SPP::CreateInputLayout();
		meshVertexLayout->InitializeLayout({
				{ "POSITION",  SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,position) },
				{ "NORMAL",    SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,normal) },
				{ "TANGENT",   SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,tangent) },
				{ "BITANGENT", SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,bitangent) },
				{ "TEXCOORD",  SPP::InputLayoutElementType::Float2, offsetof(SPP::MeshVertex,texcoord) } });

		meshMat->layout = meshVertexLayout;
		//meshMatMeshShader->layout = meshVertexLayout;
		//meshMat->SetTextureUnit(0, textureTest->GetGPUTexture());

		auto& meshElements = mesh->GetMeshElements();

		for (auto& curMesh : meshElements)
		{
			curMesh->material = meshMat;
		}


		for (int32_t X = -5; X <= 5; X++)
		{
			for (int32_t Y = -5; Y <= 5; Y++)
			{
				auto newMeshToDraw = SPP::CreateRenderableMesh(true);

				auto& pos = newMeshToDraw->GetPosition();

				auto& scale = newMeshToDraw->GetScale();

				pos[0] = X * 1000;
				pos[1] = 10;
				pos[2] = Y * 1000;

				scale[0] = 100.0f;
				scale[1] = 100.0f;
				scale[2] = 100.0f;

				MeshesToDraw.push_back(newMeshToDraw);
				newMeshToDraw->SetMeshData(meshElements);
				newMeshToDraw->AddToScene(_mainScene.get());
			}
		}

#endif
		////////////////////////////////////
		//TERRRAIN SECTION
		////////////////////////////////////

#if 0
		auto mainTerrain = SPP::SPPObject::CreateObject< SPP::Terrain >("TerrainMain");
		mainTerrain->Create();

		auto terrainShaderVS = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.VertexSO");
		terrainShaderVS->LoadFromDisk("shaders/TerrainVS.hlsl", "main", SPP::EShaderType::Vertex);
		auto terrainShaderDS = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.DomainSO");
		terrainShaderDS->LoadFromDisk("shaders/TerrainDS.hlsl", "main", SPP::EShaderType::Domain);
		auto terrainShaderHS = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.HullSO");
		terrainShaderHS->LoadFromDisk("shaders/TerrainHS.hlsl", "main", SPP::EShaderType::Hull);
		auto terrainShaderPS = SPP::SPPObject::CreateObject< SPP::ShaderObject>("TerrainMain.PixelSO");
		terrainShaderPS->LoadFromDisk("shaders/TerrainPS.hlsl", "main", SPP::EShaderType::Pixel);

		auto terrainHeightMap = SPP::SPPObject::CreateObject< SPP::TextureObject>("TerrainMain.HeightMap");
		terrainHeightMap->LoadFromDisk("texture/terrain/sanddunesheightmap.jpg");

		//
		auto terrainDisplacementMap = SPP::SPPObject::CreateObject< SPP::TextureObject>("TerrainMain.DisplacementMap");
		terrainDisplacementMap->LoadFromDisk("texture/terrain/tdsmeeko_8K_Displacement.dds");
		auto normalMap = SPP::SPPObject::CreateObject< SPP::TextureObject>("TerrainMain.NormalMap");
		normalMap->LoadFromDisk("texture/terrain/tdsmeeko_8K_Normal.dds");
		auto albedoMap = SPP::SPPObject::CreateObject< SPP::TextureObject>("TerrainMain.AlbedoMap");
		albedoMap->LoadFromDisk("texture/terrain/tdsmeeko_8K_Albedo.dds");

		auto terrainVertexLayout = SPP::CreateInputLayout();
		terrainVertexLayout->InitializeLayout({
				{ "POSITION",  SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,position) }
			});


		auto terrainMat = std::make_shared<SPP::MeshMaterial>();
		terrainMat->layout = terrainVertexLayout;

		terrainMat->vertexShader = terrainShaderVS->GetGPUShader();
		terrainMat->domainShader = terrainShaderDS->GetGPUShader();
		terrainMat->hullShader = terrainShaderHS->GetGPUShader();
		terrainMat->pixelShader = terrainShaderPS->GetGPUShader();

		terrainMat->SetTextureUnit(0, terrainHeightMap->GetGPUTexture());
		terrainMat->SetTextureUnit(1, terrainDisplacementMap->GetGPUTexture());
		terrainMat->SetTextureUnit(2, normalMap->GetGPUTexture());
		terrainMat->SetTextureUnit(3, albedoMap->GetGPUTexture());


		auto terrainRenderableMesh = SPP::CreateRenderableMesh();

		MeshesToDraw.push_back(terrainRenderableMesh);

		auto& terrainMeshElements = mainTerrain->GetMeshElements();
		terrainMeshElements.front()->topology = SPP::EDrawingTopology::PatchList_4ControlPoints;
		terrainMeshElements.front()->material = terrainMat;

		terrainRenderableMesh->SetMeshData(terrainMeshElements);
		terrainRenderableMesh->AddToScene(_mainScene.get());
#endif
		SPP::MakeResidentAllGPUResources();

		_lastTime = std::chrono::high_resolution_clock::now();
	}

	void Update()
	{
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
				"http://spp/SPPEditor/html/index.html",
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


