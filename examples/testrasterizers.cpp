// SPPEngine.cpp : Defines the entry point for the application.
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
#include <filesystem>
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


#include <condition_variable>

using namespace SPP;

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

	SPP::IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());
	SPP::IntializeGraphics();	
	// setup global asset path
	SPP::GAssetPath = std::filesystem::absolute(std::filesystem::current_path() / "..\\Assets\\").generic_string();


	//SPP::CallPython();

	int ErrorCode = 0;		
	{
		std::unique_ptr<SPP::ApplicationWindow> app = SPP::CreateApplication();

		app->Initialize(1280, 720, hInstance);

		auto dx12Device = SPP::CreateGraphicsDevice();
		dx12Device->Initialize(1280, 720, app->GetOSWindow());

		auto mainScene = SPP::CreateRenderScene();
		auto& cam = mainScene->GetCamera();
		//cam.GetCameraPosition()[1] = 100;

		//meshlets
		auto meshletSimpleAS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.meshletAS");
		meshletSimpleAS->LoadFromDisk("shaders/SimpleMeshletAS.hlsl", "main", SPP::EShaderType::Amplification);

		auto meshletSimpleMS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.meshletMS");
		meshletSimpleMS->LoadFromDisk("shaders/SimpleMeshletMS.hlsl", "main", SPP::EShaderType::Mesh);

		auto meshletSimplePS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.meshletPS");
		meshletSimplePS->LoadFromDisk("shaders/SimpleMeshletPS.hlsl", "main", SPP::EShaderType::Pixel);

		auto triangleCS = SPP::SPPObject::CreateObject<SPP::ShaderObject>("bleep.rasterizer");
		triangleCS->LoadFromDisk("shaders/rasterizers.hlsl", "renderTriangleCS", SPP::EShaderType::Compute);

		auto offscreenTexture = SPP::SPPObject::CreateObject<SPP::TextureObject>("bleep.offscreen");
		offscreenTexture->Generate(1024, 1024, TextureFormat::RGBA_8888);
		 
		auto mesh = SPP::SPPObject::CreateObject<SPP::Mesh>("mesh.gunmesh");
		mesh->LoadMesh("meshes/photogrammtree.blend");

		
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
			[](int32_t mouseX, int32_t mouseY, SPP::EMouseButton mouseButton)
			{
				//SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
			},
			[](int32_t mouseX, int32_t mouseY, SPP::EMouseButton mouseButton)
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

		auto meshMat = std::make_shared<SPP::MeshMaterial>();

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

		std::list< std::shared_ptr<SPP::RenderableMesh> > MeshesToDraw;

		//for (int32_t X = -5; X <= 5; X++)
		{
			//for (int32_t Y = -5; Y <= 5; Y++)
			{
				auto newMeshToDraw = SPP::CreateRenderableMesh(true);

				auto &pos = newMeshToDraw->GetPosition();

				auto& scale = newMeshToDraw->GetScale();

				pos[0] = 0;// X * 1000;
				pos[1] = 0;
				pos[2] = 0;// Y * 1000;

				scale[0] = 100.01f;
				scale[1] = 100.01f;
				scale[2] = 100.01f;

				MeshesToDraw.push_back(newMeshToDraw);
				newMeshToDraw->SetMeshData(meshElements);
				newMeshToDraw->AddToScene(mainScene.get());
			}
		}
		
		auto triDispatch = CreateComputeDispatch(triangleCS->GetGPUShader());
		struct TriData
		{
			Vector4 v0;
			Vector4 v1;
			Vector4 v2;
		};
		auto triResource = std::make_shared< ArrayResource >();
		auto tris = triResource->InitializeFromType< TriData>(1);

		tris[0].v0 = Vector4(10, 10, 0, 0);
		tris[0].v1 = Vector4(10, 600, 0, 0);
		tris[0].v2 = Vector4(600, 600, 0, 0);
		
		triDispatch->SetConstants({ triResource });
		triDispatch->SetTextures({ offscreenTexture->GetGPUTexture() } );
			
		SPP::MakeResidentAllGPUResources();		

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

			//up
			if (keys[0x26])
				cam.TurnCamera(SPP::Vector2(0, 400) * DeltaTime);
			//down
			if (keys[0x28])
				cam.TurnCamera(SPP::Vector2(0, -400) * DeltaTime);
			//left
			if (keys[0x25])
				cam.TurnCamera(SPP::Vector2(400, 0) * DeltaTime);
			//right
			if (keys[0x27])
				cam.TurnCamera(SPP::Vector2(-400, 0) * DeltaTime);

			//cam.TurnCamera(mouseDelta);
			mouseDelta = SPP::Vector2(0, 0);

			dx12Device->BeginFrame();
			mainScene->Draw();
			triDispatch->Dispatch(Vector3i(1, 1, 1));
			dx12Device->EndFrame();

			//bool bReady = false;

			//TODO FIX CAMERA NOT BNEING ON RT
			//SPP::GPUThreaPool->enqueue([dx12Device, mainScene, &cv, &bReady, &tickMutex]()
			//	{
			//		{
			//			std::lock_guard<std::mutex> lk(tickMutex);
			//			dx12Device->MoveToNextFrame();
			//			bReady = true;
			//		}
			//		cv.notify_one();

			//		dx12Device->BeginFrame();
			//		mainScene->Draw();
			//		dx12Device->EndFrame();
			//	});

			//// if we couldn't begin frame, gpu behind
			//std::unique_lock<std::mutex> lk(tickMutex);
			//cv.wait(lk, [&bReady] {return bReady; });

			//std::this_thread::sleep_for(0ms);
		};

		app->SetEvents({ msgLoop });

		ErrorCode = app->Run();
	}

	return ErrorCode;
}


