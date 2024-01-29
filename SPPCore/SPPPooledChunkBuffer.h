// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPMemory.h"

#include <mutex>
#include <list>

namespace SPP
{

	class SPP_CORE_API PooledChunkBuffer
	{
	private:
		class ReusableChunk : public MemoryChunk
		{
		private:
			PooledChunkBuffer* const _parent = nullptr;
			size_t _totaSize = 0;

		public:
			auto ActualSize() const
			{
				return _totaSize;
			}
			ReusableChunk(void* DataLocation, size_t InSize, size_t TotalSize, PooledChunkBuffer* Parent);
			virtual ~ReusableChunk();
		};

		std::mutex _chunkMutex;
		std::list< MemoryChunk > _chunkPool;

	public:
		PooledChunkBuffer();
		~PooledChunkBuffer();

		std::shared_ptr<MemoryChunk> GetChunk(size_t DesiredSize);

		void GiveMemory(void* data, size_t size);
	};

}