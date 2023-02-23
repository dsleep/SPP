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
#include "SPPHandledTimers.h"
#include "SPPTextures.h"

#include <list>
#include <mutex>		


namespace SPP
{
	const char* ToString(TexturePurpose InValue)
	{
		SE_ASSERT(InValue >= TexturePurpose::Diffuse && InValue <= TexturePurpose::Lightmap);

		static const char* stringValues[] =
		{
			"Diffuse",
			"Emissive",
			"Metallic",
			"Normal",
			"Roughness",
			"Alpha",
			"Lightmap"
		};
		
		return stringValues[(uint8_t)InValue];
	}

	//RT_Shader
	void RT_Shader::Initialize(EShaderType InType)
	{
		_shader = GGI()->GetGraphicsDevice()->_gxCreateShader(InType);
	}
	bool RT_Shader::CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		return _shader->CompileShaderFromFile(FileName, EntryPoint, oErrorMsgs);
	}
	bool RT_Shader::CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		return _shader->CompileShaderFromString(ShaderSource, ShaderName, EntryPoint, oErrorMsgs);
	}	

	//RT_Buffer
	void RT_Buffer::Initialize(size_t BufferSize)
	{
		_buffer = GGI()->GetGraphicsDevice()->_gxCreateBuffer(_type, BufferSize);
	}
	void RT_Buffer::Initialize(std::shared_ptr< ArrayResource > InCpuData)
	{
		_buffer = GGI()->GetGraphicsDevice()->_gxCreateBuffer(_type, InCpuData);
	}

	//RT_Texture
	void RT_Texture::Initialize(const struct TextureAsset& TextureAsset)
	{
		_texture = GGI()->GetGraphicsDevice()->_gxCreateTexture(TextureAsset);
	}
	GPUReferencer< GPUTexture > RT_Texture::GetGPUTexture()
	{
		return _texture;
	}

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

	GPUResource::GPUResource() 
	{
		SE_ASSERT(IsOnGPUThread());
		SE_ASSERT(GGI() && GGI()->GetGraphicsDevice());
		GGI()->GetGraphicsDevice()->PushResource(this);
	}

	GPUResource::~GPUResource()
	{
		SE_ASSERT(IsOnGPUThread());
		GGI()->GetGraphicsDevice()->PopResource(this);
	}

	void GPUResource::NoMoreReferences()
	{
		_dying = true;
		GGI()->GetGraphicsDevice()->DyingResource(this);
	}

	GlobalGraphicsResource::GlobalGraphicsResource() 
	{
		SE_ASSERT(IsOnGPUThread());
	}
	GlobalGraphicsResource::~GlobalGraphicsResource()
	{
		SE_ASSERT(IsOnGPUThread());
	}

	std::vector< std::function< GlobalGraphicsResource* (void) > >& GetGlobalResourceList()
	{
		static std::vector< std::function< GlobalGraphicsResource* (void) > > sO;
		return sO;
	}

	uint32_t RegisterGlobalResource(std::function< GlobalGraphicsResource* (void) > AllocResource)
	{
		auto& gArray = GetGlobalResourceList();
		uint32_t currentSize = (uint32_t)gArray.size();
		gArray.push_back(AllocResource);
		return currentSize;
	}

	void UnregisterGlobalResource(GlobalGraphicsResource* InResource)
	{
		//needed?!
		SE_ASSERT(false);
	}

	//void MakeResidentAllGPUResources()
	//{
	//	auto CurResource = InternalLinkedList<GPUResource>::GetRoot();
	//	while(CurResource)
	//	{
	//		SPP_QL("MakeResidentAllGPUResources: %s", CurResource->GetName());

	//		CurResource->MakeResident();
	//		CurResource = CurResource->GetNext();
	//	};
	//}	
	
	static uint32_t GHighestTextureID = 0;
	static std::list<uint32_t> GTextureAvailIDs;

	GPUTexture::GPUTexture(int32_t Width, int32_t Height, int32_t MipLevelCount, int32_t FaceCount,
		TextureFormat Format) :
		_width(Width), _height(Height), _format(Format), _mipLevels(MipLevelCount), _faceCount(FaceCount)
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

	GPUTexture::GPUTexture(const struct TextureAsset& InTextureAsset)
	{
		_width = InTextureAsset.width;
		_height = InTextureAsset.height;
		_format = InTextureAsset.format;
		_faceData = InTextureAsset.faceData;
		_bIsSRGB = InTextureAsset.bSRGB;

		_faceCount = (uint8_t)_faceData.size();
		if (_faceCount)
		{
			_mipLevels = (uint8_t)_faceData.front()->mipData.size();
		}

		//int32_t Width, int32_t Height, TextureFormat Format
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

	void GraphicsDevice::INTERNAL_AddScene(class RT_RenderScene* InScene)
	{
		_renderScenes.push_back(InScene);
		InScene->AddedToGraphicsDevice();
	}

	void GraphicsDevice::INTERNAL_RemoveScene(class RT_RenderScene* InScene)
	{
		_renderScenes.erase(std::remove(_renderScenes.begin(), _renderScenes.end(), InScene), _renderScenes.end());
		InScene->RemovedFromGraphicsDevice();
	}

	GPU_CALL GraphicsDevice::AddScene(class RT_RenderScene *InScene)
	{
		INTERNAL_AddScene(InScene);
		co_return;
	}

	GPU_CALL GraphicsDevice::RemoveScene(class RT_RenderScene* InScene)
	{
		INTERNAL_RemoveScene(InScene);
		co_return;
	}

	void GraphicsDevice::SyncGPUData()
	{
		SE_ASSERT(IsOnCPUThread());
		for (auto& curScene : _renderScenes)
		{
			curScene->PrepareScenesToDraw();
		}
	}

	void GraphicsDevice::BeginFrame()
	{
		bFrameActive = true;

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

		bFrameActive = false;
	}

	void GraphicsDevice::RunFrame()
	{
		SE_ASSERT(IsOnCPUThread());

		STDElapsedTimer priorFrame;

		if (_currentFrame.valid())
		{
			_currentFrame.wait();
		}

		auto currentElapsedMS = priorFrame.getElapsedMilliseconds();
		if (currentElapsedMS > 5)
		{
			//SPP_QL( "_currentFrame long wait");
		}

		// tiny window in between gpu thread calls
		// NOTE: gpu resources can still be in flight but CPU/GPU thread are
		// temporarily in sync
		SyncGPUData();

		_currentFrame = RunOnRT([this]()
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