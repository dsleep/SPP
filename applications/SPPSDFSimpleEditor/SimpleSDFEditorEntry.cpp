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

#include "SPPSceneO.h"
#include "SPPGraphicsO.h"

#include "SPPJsonUtils.h"

#include "cefclient/JSCallbackInterface.h"
#include <condition_variable>

#define MAX_LOADSTRING 100

using namespace std::chrono_literals;
using namespace SPP;

void GenerateObjectList(OScene* InWorld, Json::Value& rootValue)
{
	auto entities = InWorld->GetChildren();

	for (auto entity : entities)
	{
		SE_ASSERT(entity);

		if (entity)
		{
			auto pathName = entity->GetPath();

			Json::Value localValue;
			localValue["NAME"] = pathName.ToString();
			localValue["GUID"] = entity->GetGUID().ToString();

			Json::Value elementValues;
			auto elements = entity->GetChildren();
			for (auto element : elements)
			{
				auto elePath = element->GetPath();

				Json::Value curElement;
				curElement["NAME"] = elePath.ToString();
				curElement["GUID"] = element->GetGUID().ToString();

				elementValues.append(curElement);
			}
			if (!elements.empty())
			{
				localValue["CHILDREN"] = elementValues;
			}

			rootValue.append(localValue);
		}
	}
}

class EditorEngine
{
	enum class EGizmoSelectionAxis
	{
		None,
		X,
		Y,
		Z
	};

private:
	std::shared_ptr<GraphicsDevice> _graphicsDevice;

	HWND _mainDXWindow = nullptr;
	Vector2 _mouseDelta = Vector2(0, 0);
	uint8_t _keys[255] = { 0 };

	Vector2i _mousePosition = { -1, -1 };
	std::chrono::high_resolution_clock::time_point _lastTime;
	std::shared_ptr< SPP::MeshMaterial > _gizmoMat;	

	OMesh* _moveGizmo = nullptr;
	OMesh* _rotateGizmo = nullptr;
	OMesh* _scaleGizmo = nullptr;

	ORenderableScene* _renderableScene = nullptr;
	OMeshElement* _gizmo = nullptr;
	OElement* _selectedElement = nullptr;

	bool _htmlReady = false;	

	EGizmoSelectionAxis _selectionAxis = EGizmoSelectionAxis::None;

public:
	std::map < std::string, std::function<void(Json::Value) > > fromJSFunctionMap;


	void HTMLReady()
	{
		_htmlReady = true;

		UpdateObjectTree();
	}

	void UpdateObjectTree()
	{
		Json::Value rootValue;
		GenerateObjectList(_renderableScene, rootValue);

		std::string StrMessage;
		JsonToString(rootValue, StrMessage);

		JavascriptInterface::CallJS("UpdateObjectTree", StrMessage);
	}

	void SelectObject(OElement* SelectedObject)
	{	
		if (_selectedElement)
		{
			_renderableScene->RemoveChild(_gizmo);
		}

		if (SelectedObject)
		{
			auto localToWorld = SelectedObject->GenerateLocalToWorld(true);
			_gizmo->GetPosition() = localToWorld.block<1, 3>(3, 0).cast<double>() + SelectedObject->GetTop()->GetPosition();
			_renderableScene->AddChild(_gizmo);
		}

		_selectedElement = SelectedObject;
	}
	
	void DeleteSelection()
	{
	}

	void SelectionChanged(std::string InElementName)
	{
		SPP::GUID selGUID(InElementName.c_str());

		auto CurObject = GetObjectByGUID(selGUID);
		if (CurObject)
		{
			SPP_QL("SelectionChanged: %s", CurObject->GetPath().ToString().c_str());
			SelectObject((OElement*)CurObject);
		}
	}

	void PropertyChanged(Json::Value InValue)
	{

	}

	void AddShape(std::string InShapeType)
	{


		UpdateObjectTree();
	}

	void AddShapeGroup(std::string InGroupType)
	{


		UpdateObjectTree();
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
		
		RECT rect;
		GetClientRect(_mainDXWindow, &rect);

		int32_t WindowSizeX = rect.right - rect.left;
		int32_t WindowSizeY = rect.bottom - rect.top;

		_graphicsDevice = GGI()->CreateGraphicsDevice();
		_graphicsDevice->Initialize(WindowSizeX, WindowSizeY, AppWindow);

		//_renderScene = GGI()->CreateRenderScene();
		//auto& cam = _renderScene->GetCamera();
		//cam.GetCameraPosition()[1] = 100;

		_moveGizmo = AllocateObject<OMesh>("moveG");
		_rotateGizmo = AllocateObject<OMesh>("rotateG");
		_scaleGizmo = AllocateObject<OMesh>("scaleG");

		{
			auto MeshToLoad = std::make_shared< Mesh >();
			MeshToLoad->LoadMesh("BlenderFiles/MoveGizmo.fbx");
			_moveGizmo->SetMesh(MeshToLoad);
		}
		{
			auto MeshToLoad = std::make_shared< Mesh >();
			MeshToLoad->LoadMesh("BlenderFiles/RotationGizmo.fbx");
			_rotateGizmo->SetMesh(MeshToLoad);
		}		
		
		//_rotateGizmo->LoadMesh("BlenderFiles/RotationGizmo.ply");
		//_scaleGizmo->LoadMesh("BlenderFiles/ScaleGizmo.ply");

		_gizmoMat = std::make_shared< MeshMaterial >();
		//_gizmoMat->depthState = EDepthState::Disabled;
		_gizmoMat->rasterizerState = ERasterizerState::NoCull;
		_gizmoMat->vertexShader = GGI()->CreateShader(EShaderType::Vertex);
		_gizmoMat->vertexShader->CompileShaderFromFile("shaders/SimpleColoredMesh.hlsl", "main_vs");

		_gizmoMat->pixelShader = GGI()->CreateShader(EShaderType::Pixel);
		_gizmoMat->pixelShader->CompileShaderFromFile("shaders/SimpleColoredMesh.hlsl", "main_ps");

		_gizmoMat->layout = GGI()->CreateInputLayout();
		_gizmoMat->layout->InitializeLayout({
				{ "POSITION",  SPP::InputLayoutElementType::Float3, offsetof(SPP::MeshVertex,position) },
				{ "COLOR",  SPP::InputLayoutElementType::UInt, offsetof(SPP::MeshVertex,color) } });

		auto& meshElements = _moveGizmo->GetMesh()->GetMeshElements();

		for (auto& curMesh : meshElements)
		{
			curMesh->material = _gizmoMat; 
		}


		/////////////SCENE SETUP

		_renderableScene = AllocateObject<ORenderableScene>("rScene");
		
		

		auto startingGroup = AllocateObject<OShapeGroup>("mainShape");
		auto startingSphere = AllocateObject<OSDFSphere>("mainShape.sphere");
		auto startingSphere2 = AllocateObject<OSDFSphere>("mainShape.sphere_2");
		auto startingSphere3 = AllocateObject<OSDFSphere>("mainShape.sphere_3");
		//auto startingSphere = AllocateObject<OSDFSphere>("mainShape.box");


		startingGroup->GetPosition()[2] = 200.0;

		startingSphere->SetRadius(50);

		startingSphere2->SetRadius(25);
		startingSphere2->GetPosition()[1] = 50.0;
		startingSphere2->GetPosition()[0] = 50.0;

		startingSphere3->SetRadius(25);
		startingSphere3->GetPosition()[1] = 50.0;
		startingSphere3->GetPosition()[0] = -50.0;

		startingGroup->AddChild(startingSphere);
		startingGroup->AddChild(startingSphere2);
		startingGroup->AddChild(startingSphere3);

		_renderableScene->AddChild(startingGroup);

		///
		_gizmo = AllocateObject<OMeshElement>("meshE");
		_gizmo->SetMesh(_moveGizmo);
		_gizmo->GetPosition()[2] = 200.0;
		_gizmo->GetPosition()[1] = 50.0;
		_gizmo->GetPosition()[0] = 50.0;
		_gizmo->GetScale() = 0.3;
		//_renderableScene->AddChild(_gizmo);

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

		auto& cam = _renderableScene->GetRenderScene()->GetCamera();

		Vector4 MousePosNear = Vector4(((_mousePosition[0] / (float)WindowSizeX) * 2.0f - 1.0f), -((_mousePosition[1] / (float)WindowSizeY) * 2.0f - 1.0f), 10.0f, 1.0f);
		Vector4 MousePosFar = Vector4(((_mousePosition[0] / (float)WindowSizeX) * 2.0f - 1.0f), -((_mousePosition[1] / (float)WindowSizeY) * 2.0f - 1.0f), 100.0f, 1.0f);

		Vector4 MouseLocalNear = MousePosNear * cam.GetInvViewProjMatrix();
		MouseLocalNear /= MouseLocalNear[3];
		Vector4 MouseLocalFar = MousePosFar * cam.GetInvViewProjMatrix();
		MouseLocalFar /= MouseLocalFar[3];

		Vector3 MouseStart = Vector3(MouseLocalNear[0], MouseLocalNear[1], MouseLocalNear[2]);
		Vector3 MouseEnd = Vector3(MouseLocalFar[0], MouseLocalFar[1], MouseLocalFar[2]);
		Vector3 MouseRay = (MouseEnd - MouseStart).normalized();

		IntersectionInfo info;
		if (_gizmo->Intersect_Ray(Ray(MouseStart.cast<double>() + cam.GetCameraPosition(), MouseRay), info))
		{
			if (!info.hitName.empty())
			{
				if (info.hitName[0] == 'X')
				{
					_selectionAxis = EGizmoSelectionAxis::X;
				}
				else if (info.hitName[0] == 'Y')
				{
					_selectionAxis = EGizmoSelectionAxis::Y;
				}
				else if (info.hitName[0] == 'Z')
				{
					_selectionAxis = EGizmoSelectionAxis::Z;
				}
			}
			//SPP_QL("Hit: %s", info.hitName.c_str());
			_gizmo->UpdateSelection(true);
		}
		else
		{
			_gizmo->UpdateSelection(false);
			_selectionAxis = EGizmoSelectionAxis::None;
		}

		//

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
		_renderableScene->GetRenderScene()->Draw();
		_graphicsDevice->EndFrame();
	}

	void KeyDown(uint8_t KeyValue)
	{		
		SPP_QL("kd: 0x%X", KeyValue);
		_keys[KeyValue] = true;
	}

	void Deselect()
	{
		SelectObject(nullptr);
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
			auto& cam = _renderableScene->GetRenderScene()->GetCamera();
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


