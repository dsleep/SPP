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
	template<typename T>
	class ThreadSafeList
	{
	private:
		std::mutex		_mutex;
		std::list<T>	_list;

	public:
		void Add(const T &InResource)
		{
			std::lock_guard<std::mutex> guard(_mutex);
			_list.push_back(InResource);
		}

		void Remove(const T& InResource)
		{
			std::lock_guard<std::mutex> guard(_mutex);
			_list.remove(InResource);
		}

		template<typename Func>
		void Iterate(Func&& f)
		{
			std::lock_guard<std::mutex> guard(_mutex);
			for (auto& ele : _list)
			{
				f(ele);
			}			
		}
	};

	ThreadSafeList< GPUResource* >& GetGPUResourceList()
	{
		static ThreadSafeList< GPUResource* > sO;
		return sO;
	}

	GPUResource::GPUResource()
	{
		GetGPUResourceList().Add(this);
	}
	GPUResource::~GPUResource()
	{
		GetGPUResourceList().Remove(this);
	}

	void BegineResourceCopy() { }
	void EndResourceCopy() { }

	void MakeResidentAllGPUResources()
	{
		BegineResourceCopy();

		GetGPUResourceList().Iterate([](GPUResource *InEle)
			{
				SPP_QL("MakeResidentAllGPUResources: %s", InEle->GetName());

				InEle->UploadToGpu();
			});

		EndResourceCopy();
	}

	
	void ShaderObject::LoadFromDisk(const AssetPath& FilePath, const char* EntryPoint, EShaderType InType)
	{
		_shader = GGI()->CreateShader(InType);
		SE_ASSERT(_shader);
		_shader->CompileShaderFromFile(FilePath, EntryPoint);
	}

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