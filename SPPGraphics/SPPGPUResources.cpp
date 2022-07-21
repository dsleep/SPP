// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"
#include "SPPString.h"
#include "SPPSceneRendering.h"
#include "ThreadPool.h"

#include <list>
#include <mutex>		


namespace SPP
{
	//template<typename T>
	//class ThreadSafeList
	//{
	//private:
	//	std::mutex		_mutex;
	//	std::list<T>	_list;

	//public:
	//	void Add(const T &InResource)
	//	{
	//		std::lock_guard<std::mutex> guard(_mutex);
	//		_list.push_back(InResource);
	//	}

	//	void Remove(const T& InResource)
	//	{
	//		std::lock_guard<std::mutex> guard(_mutex);
	//		_list.remove(InResource);
	//	}

	//	template<typename Func>
	//	void Iterate(Func&& f)
	//	{
	//		std::lock_guard<std::mutex> guard(_mutex);
	//		for (auto& ele : _list)
	//		{
	//			f(ele);
	//		}			
	//	}
	//};

	GPUResource* InternalLinkedList<GPUResource>::_root = nullptr;

	GPUResource::GPUResource(GraphicsDevice* InOwner) : _owner(InOwner), InternalLinkedList<GPUResource>()
	{
		SE_ASSERT(IsOnGPUThread());
	}

	GPUResource::~GPUResource()
	{
		SE_ASSERT(IsOnGPUThread());
	}

	GlobalGraphicsResource* InternalLinkedList<GlobalGraphicsResource>::_root = nullptr;
	GlobalGraphicsResource::GlobalGraphicsResource() : InternalLinkedList<GlobalGraphicsResource>()
	{
		//SE_ASSERT IsBeforeMain
	}
	GlobalGraphicsResource::~GlobalGraphicsResource()
	{
		SE_ASSERT(IsOnCPUThread());
	}

	void MakeResidentAllGPUResources()
	{
		auto CurResource = InternalLinkedList<GPUResource>::GetRoot();
		while(CurResource)
		{
			SPP_QL("MakeResidentAllGPUResources: %s", CurResource->GetName());

			CurResource->MakeResident();
			CurResource = CurResource->GetNext();
		};
	}	
	
	static uint32_t GHighestTextureID = 0;
	static std::list<uint32_t> GTextureAvailIDs;

	GPUTexture::GPUTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format,
		std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo) :
		GPUResource(InOwner),
		_width(Width), _height(Height), _format(Format), _rawImgData(RawData), _metaInfo(InMetaInfo)
	{
		if (!GTextureAvailIDs.empty())
		{
			_uniqueID = GTextureAvailIDs.front();
			GTextureAvailIDs.pop_front();
		}
		else
		{
			_uniqueID = GHighestTextureID++;
		}
	}

	GPUTexture::~GPUTexture() 
	{ 
		GTextureAvailIDs.push_back(_uniqueID);
	}

	void GraphicsDevice::INTERNAL_AddScene(std::shared_ptr< class GD_RenderScene > InScene)
	{
		_renderScenes.push_back(InScene);
		InScene->AddedToGraphicsDevice();
	}

	void GraphicsDevice::INTERNAL_RemoveScene(std::shared_ptr< class GD_RenderScene > InScene)
	{
		_renderScenes.erase(std::remove(_renderScenes.begin(), _renderScenes.end(), InScene), _renderScenes.end());
		//InScene->removed();
	}

	GPU_CALL GraphicsDevice::AddScene(std::shared_ptr< class GD_RenderScene > InScene)
	{
		INTERNAL_AddScene(InScene);
		co_return;
	}

	GPU_CALL GraphicsDevice::RemoveScene(std::shared_ptr< class GD_RenderScene > InScene)
	{
		INTERNAL_RemoveScene(InScene);
		co_return;
	}

	void GraphicsDevice::PrepareScenesToDraw()
	{
		SE_ASSERT(IsOnCPUThread());
		for (auto& curScene : _renderScenes)
		{
			curScene->PrepareScenesToDraw();
		}
	}

	void GraphicsDevice::BeginFrame()
	{
		SE_ASSERT(IsOnGPUThread());
		for (auto& curScene : _renderScenes)
		{
			curScene->BeginFrame();
		}
	}
	void GraphicsDevice::Draw()
	{
		SE_ASSERT(IsOnGPUThread());
		for (auto& curScene : _renderScenes)
		{
			curScene->Draw();
		}
	}
	void GraphicsDevice::EndFrame()
	{
		SE_ASSERT(IsOnGPUThread());
		for (auto& curScene : _renderScenes)
		{
			curScene->EndFrame();
		}
	}

	void GraphicsDevice::RunFrame()
	{
		if (_currentFrame.valid())
		{
			_currentFrame.wait();
		}

		// tiny window in between gpu threah calls
		PrepareScenesToDraw();

		_currentFrame = GPUThreaPool->enqueue([this]()
			{
				BeginFrame();
				Draw();
				EndFrame();
				return true;
			});
	}

	//GLOBAL GRAPHICS INTERFACE
	static IGraphicsInterface* GGIPtr = nullptr;
	static GraphicsDevice* GGDPtr = nullptr;

	IGraphicsInterface* GGI()
	{
		return GGIPtr;
	}
	void SET_GGI(IGraphicsInterface* InGraphicsIterface)
	{
		GGIPtr = InGraphicsIterface;
	}

}