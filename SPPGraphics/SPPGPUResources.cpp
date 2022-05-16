// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"
#include "SPPString.h"
#include "SPPSceneRendering.h"

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
	
	static GPUResource* GRootResource = nullptr;

	GPUResource::GPUResource()
	{
		if (GRootResource)
		{
			GRootResource->_prevResource = this;
			_nextResource = GRootResource;
		}
		GRootResource = this;
	}

	GPUResource::~GPUResource()
	{
		if (_nextResource)
		{
			_nextResource->_prevResource = _prevResource;
		}
		if (_prevResource)
		{
			_prevResource->_nextResource = _nextResource;
		}

		if (GRootResource == this)
		{
			SE_ASSERT(_prevResource == nullptr);
			GRootResource = _nextResource;
		}		
	}

	void MakeResidentAllGPUResources()
	{
		SE_ASSERT(GGI());
		GGI()->BeginResourceCopies();

		auto CurResource = GRootResource;
		while(CurResource)
		{
			SPP_QL("MakeResidentAllGPUResources: %s", CurResource->GetName());

			CurResource->UploadToGpu();
			CurResource = CurResource->GetNext();
		};

		GGI()->EndResourceCopies();
	}

	
	void ShaderObject::LoadFromDisk(const AssetPath& FilePath, const char* EntryPoint, EShaderType InType)
	{
		_shader = GGI()->CreateShader(InType);
		SE_ASSERT(_shader);
		_shader->CompileShaderFromFile(FilePath, EntryPoint);
	}
	
	static uint32_t GHighestTextureID = 0;
	static std::list<uint32_t> GTextureAvailIDs;

	GPUTexture::GPUTexture(int32_t Width, int32_t Height, TextureFormat Format, 
		std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo) :
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

	void GraphicsDevice::BeginFrame()
	{
		for (auto& curScene : _renderScenes)
		{
			curScene->BeginFrame();
		}
	}
	void GraphicsDevice::Draw()
	{
		for (auto& curScene : _renderScenes)
		{
			curScene->Draw();
		}
	}
	void GraphicsDevice::EndFrame()
	{
		for (auto& curScene : _renderScenes)
		{
			curScene->EndFrame();
		}
	}

	//GLOBAL GRAPHICS INTERFACE
	static IGraphicsInterface* GGIPtr = nullptr;
	IGraphicsInterface* GGI()
	{
		return GGIPtr;
	}
	void SET_GGI(IGraphicsInterface* InGraphicsIterface)
	{
		GGIPtr = InGraphicsIterface;
	}
}