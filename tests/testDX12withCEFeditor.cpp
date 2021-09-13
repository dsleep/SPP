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
#include "SPPSDFO.h"

#include <condition_variable>

#define MAX_LOADSTRING 100

using namespace std::chrono_literals;
using namespace SPP;

class EditorEngine
{
private:
	std::shared_ptr<GraphicsDevice> _graphicsDevice;
	std::shared_ptr<RenderScene> _mainScene;
	std::list< std::shared_ptr<SPP::RenderableMesh> > MeshesToDraw;
	HWND _mainDXWindow = nullptr;
	Vector2 _mouseDelta = Vector2(0, 0);
	uint8_t _keys[255] = { 0 };

	OScene* _world = nullptr;

	Vector2i _mousePosition = { -1, -1 };
	std::chrono::high_resolution_clock::time_point _lastTime;
	//std::list< std::shared_ptr<SPPObject> > cachedObjs;
	std::shared_ptr< SPP::MeshMaterial > _gizmoMat;

	std::shared_ptr< Mesh > _moveGizmo;
	std::shared_ptr< Mesh > _rotateGizmo;
	std::shared_ptr< Mesh > _scaleGizmo;
	bool _htmlReady = false;
	

public:
	std::map < std::string, std::function<void(Json::Value) > > fromJSFunctionMap;


	void HTMLReady()
	{
		_htmlReady = true;
	}
	void SelectionChanged( std::string InElementName)
	{

	}

	void PropertyChanged(Json::Value InValue)
	{

	}

	void AddShape(Json::Value InValue)
	{

	}

	void AddShapeGroup(Json::Value InValue)
	{

	}

	void Initialize(void *AppWindow)
	{
		_mainDXWindow = (HWND)AppWindow;

#ifdef _DEBUG
		LoadLibraryA("SPPDX12d.dll");
#else
		LoadLibraryA("SPPDX12.dll");
#endif

		
		GetSDFVersion();
		_world = AllocateObject<OScene>("World");
		
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
		
		_moveGizmo->LoadMesh("BlenderFiles/MoveGizmo.fbx");
		_rotateGizmo->LoadMesh("BlenderFiles/RotationGizmo.ply");
		//_scaleGizmo->LoadMesh("BlenderFiles/ScaleGizmo.ply");

		_gizmoMat = std::make_shared< MeshMaterial >();
		_gizmoMat->rasterizerState = ERasterizerState::NoCull;
		_gizmoMat->vertexShader = GGI()->CreateShader(EShaderType::Vertex);
		_gizmoMat->vertexShader->CompileShaderFromFile("shaders/SimpleColoredMesh.hlsl", "main_vs");

		_gizmoMat->pixelShader = GGI()->CreateShader(EShaderType::Pixel);
		_gizmoMat->pixelShader->CompileShaderFromFile("shaders/SimpleColoredMesh.hlsl", "main_ps");

		_gizmoMat->layout = GGI()->CreateInputLayout();
		_gizmoMat->layout->InitializeLayout({
				{ "POSITION",  SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,position) },
				{ "COLOR",  SPP::InputLayoutElementType::UInt, offsetof(SPP::MeshVertex,color) } });

		auto& meshElements = _moveGizmo->GetMeshElements();

		for (auto& curMesh : meshElements)
		{
			curMesh->material = _gizmoMat; 
		}

		auto newMeshToDraw = GGI()->CreateRenderableMesh();

		auto& pos = newMeshToDraw->GetPosition();
		pos[2] = 200.0;
		auto& scale = newMeshToDraw->GetScale();
		scale = Vector3(1, 1, 1);

		newMeshToDraw->SetMeshData(meshElements);
		newMeshToDraw->AddToScene(_mainScene.get());
		

		MeshesToDraw.push_back(newMeshToDraw);

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

		auto& cam = _mainScene->GetCamera();

		Vector4 MousePosNear = Vector4(((_mousePosition[0] / (float)WindowSizeX) * 2.0f - 1.0f), -((_mousePosition[1] / (float)WindowSizeY) * 2.0f - 1.0f), 10.0f, 1.0f);
		Vector4 MousePosFar = Vector4(((_mousePosition[0] / (float)WindowSizeX) * 2.0f - 1.0f), -((_mousePosition[1] / (float)WindowSizeY) * 2.0f - 1.0f), 100.0f, 1.0f);

		Vector4 MouseLocalNear = MousePosNear * cam.GetInvViewProjMatrix();
		MouseLocalNear /= MouseLocalNear[3];
		Vector4 MouseLocalFar = MousePosFar * cam.GetInvViewProjMatrix();
		MouseLocalFar /= MouseLocalFar[3];

		Vector3 MouseStart = Vector3(MouseLocalNear[0], MouseLocalNear[1], MouseLocalNear[2]);
		Vector3 MouseEnd = Vector3(MouseLocalFar[0], MouseLocalFar[1], MouseLocalFar[2]);
		Vector3 MouseRay = (MouseEnd - MouseStart).normalized();

		SPP_QL("MouseRay %f %f %f", MouseRay[0], MouseRay[1], MouseRay[2]);

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
		SPP_QL("kd: 0x%X", KeyValue);
		_keys[KeyValue] = true;
	}

	void Deselect()
	{

	}

	void KeyUp(uint8_t KeyValue)
	{
		SPP_QL("ku: 0x%X", KeyValue);
		_keys[KeyValue] = false;

		if (KeyValue == VK_ESCAPE)
		{
			Deselect();
		}
	}
	

	void MouseDown(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		SPP_QL("md: %d %d %d", mouseX, mouseY, mouseButton);
	}
		
	void MouseUp(int32_t mouseX, int32_t mouseY, uint8_t mouseButton)
	{
		SPP_QL("mu: %d %d %d", mouseX, mouseY, mouseButton);
	}
		
	void MouseMove(int32_t mouseX, int32_t mouseY, uint8_t MouseState)
	{
		//SPP_QL("mm: %d %d", mouseX, mouseY);
		if (MouseState == 0)
		{
			_mousePosition[0] = mouseX;
			_mousePosition[1] = mouseY;
		}
		else if (MouseState == 1)
		{
			auto& cam = _mainScene->GetCamera();
			cam.TurnCamera(Vector2(-mouseX, -mouseY));
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
		.method("SelectionChanged", &EditorEngine::SelectionChanged)
		.method("PropertyChanged", &EditorEngine::PropertyChanged)
		.method("AddShape", &EditorEngine::AddShape)
		.method("AddShapeGroup", &EditorEngine::AddShapeGroup)
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
					std::bind(&EditorEngine::MouseMove, editor, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
				},
				&jsFuncRecv);
				});


		runCEF.join();

	}
	
	return 0;
}


