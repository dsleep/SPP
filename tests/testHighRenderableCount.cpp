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
#include <sstream>
#include <random>

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
#include "SPPSDFO.h"

#include "SPPSceneO.h"
#include "SPPGraphicsO.h"

#include "SPPJsonUtils.h"

#include "cefclient/JSCallbackInterface.h"
#include <condition_variable>
#include "SPPWin32Core.h"

#define MAX_LOADSTRING 100

using namespace std::chrono_literals;
using namespace SPP;

class EditorEngine
{
	enum class ESelectionMode
	{
		None,
		Gizmo,
		Turn
	};

private:
	std::shared_ptr<GraphicsDevice> _graphicsDevice;

	HWND _mainDXWindow = nullptr;
	Vector2 _mouseDelta = Vector2(0, 0);
	uint8_t _keys[255] = { 0 };

	Vector2i _mousePosition = { -1, -1 };
	Vector2i _mouseCaptureSpot = { -1, -1 };
	std::chrono::high_resolution_clock::time_point _lastTime;

	OMesh* _testMesh = nullptr;
	ORenderableScene* _renderableScene = nullptr;

	bool _htmlReady = false;

	ESelectionMode _selectionMode = ESelectionMode::None;

	std::string _templateShader;

	GPUReferencer< GPUShader > _currentActiveShader;

	std::shared_ptr< TextureAsset > _skyCube;

public:
	std::map < std::string, std::function<void(Json::Value) > > fromJSFunctionMap;

	void HTMLReady()
	{
		_htmlReady = true;
	}
		
	void Initialize(void *AppWindow)
	{
		_mainDXWindow = (HWND)AppWindow;

		{
			auto mainAPP = GetParent(_mainDXWindow);
			SetWindowTextA(
				mainAPP,
				"SPP SDF Creator"
			);
		}

#ifdef _DEBUG
		LoadLibraryA("SPPDX12d.dll");
#else
		LoadLibraryA("SPPDX12.dll");
#endif
				
		GetSDFVersion();
		
		RECT rect;
		GetClientRect(_mainDXWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		_graphicsDevice = GGI()->CreateGraphicsDevice();
		_graphicsDevice->Initialize(WindowSizeX, WindowSizeY, AppWindow);

		auto deferredMat = std::make_shared< MeshMaterial >();

		auto textureLeafDiff = std::make_shared< TextureAsset >();
		textureLeafDiff->LoadFromDisk("textures/leaf.dds");
		auto textureBarkDiff = std::make_shared< TextureAsset >();
		textureBarkDiff->LoadFromDisk("textures/bark.dds");

		_skyCube = std::make_shared< TextureAsset >();
		_skyCube->LoadFromDisk("textures/dayskycubemap.dds");

		deferredMat->rasterizerState = ERasterizerState::BackFaceCull;
		deferredMat->vertexShader = GGI()->CreateShader(EShaderType::Vertex);
		deferredMat->vertexShader->CompileShaderFromFile("shaders/GBufferDeferredMaterial.hlsl", "main_vs");

		deferredMat->pixelShader = GGI()->CreateShader(EShaderType::Pixel);
		deferredMat->pixelShader->CompileShaderFromFile("shaders/GBufferDeferredMaterial.hlsl", "main_ps");

		//GGI()->CreateTexture()

		_testMesh = AllocateObject<OMesh>("testM");
		auto _testMatLeaf = std::make_shared< MeshMaterial >();
		auto _testMatBark = std::make_shared< MeshMaterial >();
				
		{
			auto MeshToLoad = std::make_shared< Mesh >();
			MeshToLoad->LoadMesh("meshes/Sambucus_nigra_small.fbx");
			_testMesh->SetMesh(MeshToLoad);

			_testMatBark->vertexShader = _testMatLeaf->vertexShader = GGI()->CreateShader(EShaderType::Vertex);
			_testMatLeaf->vertexShader->CompileShaderFromFile("shaders/SimpleTextureMesh.hlsl", "main_vs");

			_testMatLeaf->rasterizerState = ERasterizerState::NoCull;

			_testMatBark->pixelShader = _testMatLeaf->pixelShader = GGI()->CreateShader(EShaderType::Pixel);
			_testMatLeaf->pixelShader->CompileShaderFromFile("shaders/SimpleTextureMesh.hlsl", "main_ps");

			_testMatBark->layout = _testMatLeaf->layout = GGI()->CreateInputLayout();
			_testMatLeaf->layout->InitializeLayout({
					{ "POSITION",  SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,position) },
					{ "TEXCOORD",  SPP::InputLayoutElementType::Float2, offsetof(SPP::MeshVertex,texcoord) } });

			auto& meshElements = _testMesh->GetMesh()->GetMeshElements();

			_testMatBark->SetTextureUnit(0, textureBarkDiff->GetGPUTexture());
			_testMatLeaf->SetTextureUnit(0, textureLeafDiff->GetGPUTexture());

			meshElements[0]->material = _testMatBark;
			meshElements[1]->material = _testMatLeaf;
		}

		/////////////SCENE SETUP
		_renderableScene = AllocateObject<ORenderableScene>("rScene");
		_renderableScene->GetRenderScene()->SetSkyBox(_skyCube->GetGPUTexture());

		for (int32_t IterX = 0; IterX < 10; IterX++)
		{
			auto _testMeshElement = AllocateObject<OMeshElement>("meshE");
			_testMeshElement->GetPosition()[2] = IterX * 30;
			_testMeshElement->SetMesh(_testMesh);	
			_renderableScene->AddChild(_testMeshElement);
		}


		auto bufferA = GGI()->CreateRenderTarget(WindowSizeX, WindowSizeY, TextureFormat::R32G32B32A32F);
		auto bufferB = GGI()->CreateRenderTarget(WindowSizeX, WindowSizeY, TextureFormat::R32G32B32A32);
		_renderableScene->GetRenderScene()->SetColorTargets(bufferA, bufferB);

		//GTA 5 is 49 square miles

		//1 mile is 1609.34 meters
		//for 100 square miles, roughly 16km by 16km or an extent of 8km
		//default scene is 16k x 16km (8192 - power of 2 - for each extents)
#if 0
		{
			std::default_random_engine generator;
			std::uniform_int_distribution<int> posDist(-8000, 8000);
			//lets do 1000ft of variance (~300m)
			std::uniform_int_distribution<int> heightDist(-150, 150);
			//0.2 to 20 meters in size
			std::uniform_real_distribution<float> objSize(0.2f, 20.0f);

			auto CurrentTime = std::chrono::high_resolution_clock::now();

			const int32_t EleCount = 50 * 1024;
			// lets start with 1 million elements of varying bounds
			for (int32_t Iter = 0; Iter < EleCount; Iter++)
			{
				auto* curElement = AllocateObject<OElement>("GENELE");

				auto& pos = curElement->GetPosition();

				pos[0] = posDist(generator);
				pos[1] = heightDist(generator);
				pos[2] = posDist(generator);

				auto& bounds = curElement->Bounds();
				bounds = Sphere(pos.cast<float>(), objSize(generator));

				_renderableScene->AddChild(curElement);
			}

			auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - CurrentTime);

			SPP_QL("Push %d elements: %f seconds", EleCount, time_span.count());
		}

		auto RSOctree = _renderableScene->GetOctree();
		RSOctree->Report();
		auto& cam = _renderableScene->GetRenderScene()->GetCamera();

		Planed planes[6];
		cam.GetFrustumPlanes(planes);

		{
			int32_t eleFound = 0;
			auto CurrentTime = std::chrono::high_resolution_clock::now();

			//AABBi aabbTest(Vector3i(-2500, -2500, -2500), Vector3i(2500, 2500, 2500));

			RSOctree->WalkElements(
				[&planes](const AABBi& InAABB) -> bool
				{
					return BoxInFrustum(planes, InAABB);
				},
				[&eleFound](const IOctreeElement* curEle) -> bool
				{
					eleFound++;
					return true;
				}
				);

			auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - CurrentTime);

			SPP_QL("Tree Test: %fms found %d", time_span.count() * 1000, eleFound);

			int32_t Width = 0, Height = 0;
			std::vector<Color3> dataSize;
			RSOctree->ImageGeneration(Width, Height, dataSize,
				[&planes](const AABBi& InAABB) -> bool
				{
					return BoxInFrustum(planes, InAABB);
				});

			SaveImageToFile("test.jpg", Width, Height, TextureFormat::RGB_888, (uint8_t*) dataSize.data());

			//RSOctree->WalkElements(aabbTest, [&eleFound](const IOctreeElement* curEle)
			//	{
			//		eleFound++;
			//		return true;
			//	});
		}
#endif
				
		SPP::MakeResidentAllGPUResources();

		_lastTime = std::chrono::high_resolution_clock::now();
	}
		

	void Update()
	{
		auto& cam = _renderableScene->GetRenderScene()->GetCamera();
			
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
			if (_graphicsDevice->GetDeviceWidth() != Width ||
				_graphicsDevice->GetDeviceHeight() != Height)
			{
				_graphicsDevice->ResizeBuffers(Width, Height);

				auto bufferA = GGI()->CreateRenderTarget(Width, Height, TextureFormat::R32G32B32A32F);
				auto bufferB = GGI()->CreateRenderTarget(Width, Height, TextureFormat::R32G32B32A32);
				_renderableScene->GetRenderScene()->SetColorTargets(bufferA, bufferB);
			}
		}
				
		if (_selectionMode == ESelectionMode::Turn)
		{
			if (_keys[0x57])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Forward);
			if (_keys[0x53])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Back);

			if (_keys[0x41])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Left);
			if (_keys[0x44])
				cam.MoveCamera(DeltaTime, SPP::ERelativeDirection::Right);
		}

		_graphicsDevice->BeginFrame();

		_renderableScene->GetRenderScene()->SetRenderToBackBuffer(true);

		_renderableScene->GetRenderScene()->Draw();
		_graphicsDevice->EndFrame();
	}

	void KeyDown(uint8_t KeyValue)
	{		
		//SPP_QL("kd: 0x%X", KeyValue);
		_keys[KeyValue] = true;
	}

	void KeyUp(uint8_t KeyValue)
	{
		//SPP_QL("ku: 0x%X", KeyValue);
		_keys[KeyValue] = false;
	}

	void MouseDown(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		if(mouseButton == 1)
		{
			CaptureWindow(_mainDXWindow);
			_selectionMode = ESelectionMode::Turn;
			_mouseCaptureSpot = Vector2i(mouseX, mouseY);
		}		
	}
		
	void MouseUp(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		//SPP_QL("mu: %d %d %d", mouseX, mouseY, mouseButton);

		if (_selectionMode != ESelectionMode::None)
		{
			_selectionMode = ESelectionMode::None;
			CaptureWindow(nullptr);
		}	
	}
		
	void MouseMove(int32_t mouseX, int32_t mouseY, uint8_t MouseState)
	{
		Vector2i currentMouse = { mouseX, mouseY };
		//SPP_QL("mm: %d %d", mouseX, mouseY);
		_mousePosition = currentMouse;
		
		if(	_selectionMode == ESelectionMode::Turn )
		{
			Vector2i Delta = (currentMouse - _mouseCaptureSpot);
			_mouseCaptureSpot = _mousePosition;
			//SetCursorPos(_mouseCaptureSpot[0], _mouseCaptureSpot[1]);
			auto& cam = _renderableScene->GetRenderScene()->GetCamera();
			cam.TurnCamera(Vector2(-Delta[0], -Delta[1]));
		}
		
	}

	void OnResize(int32_t InWidth, int32_t InHeight)
	{
		//_graphicsDevice->ResizeBuffers(InWidth, InHeight);
	}
};

RTTR_REGISTRATION
{
	rttr::registration::class_<EditorEngine>("EditorEngine")
		.method("HTMLReady", &EditorEngine::HTMLReady)	
	;
}

EditorEngine* GEd = nullptr;
void JSFunctionReceiver(const std::string& InFunc, Json::Value& InValue)
{
	auto EditorType = rttr::type::get<EditorEngine>();

	rttr::method foundMethod = EditorType.get_method(InFunc);
	if (foundMethod)
	{
		auto paramInfos = foundMethod.get_parameter_infos();

		uint32_t jsonParamCount = InValue.isNull() ? 0 : InValue.size();

		if (paramInfos.size() == jsonParamCount)
		{
			if (!jsonParamCount)
			{
				foundMethod.invoke(*GEd);
			}
			else
			{
				std::list<rttr::variant> argRefs;
				std::vector<rttr::argument> args;

				int32_t Iter = 0;
				for(auto &curParam : paramInfos)			
				{
					auto curParamType = curParam.get_type();
					auto jsonParamValue = InValue[Iter];
					if (curParamType.is_arithmetic() && jsonParamValue.isNumeric())
					{

					}
					else if (curParamType == rttr::type::get<std::string>() &&
						jsonParamValue.isString())
					{
						argRefs.push_back(std::string(jsonParamValue.asCString()));
						args.push_back(argRefs.back());
					}
					Iter++;
				}

				if(args.size() == paramInfos.size())
				{
					foundMethod.invoke_variadic(*GEd, args);
				}
			}
		}
	}
}

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
		GEd = gameEditor.get();
		std::function<void(const std::string&, Json::Value&) > jsFuncRecv = JSFunctionReceiver;

		std::thread runCEF([hInstance, editor = gameEditor.get(), &jsFuncRecv]()
		{
			SPP::RunBrowser(hInstance,
				"http://spp/assets/web/editor/VisibilityTesting.html",
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
					std::bind(&EditorEngine::MouseMove, editor, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
				},
				&jsFuncRecv);
				});


		runCEF.join();

	}
	
	return 0;
}


