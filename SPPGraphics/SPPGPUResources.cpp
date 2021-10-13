// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"
#include "SPPString.h"

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