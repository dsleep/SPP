// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#include <mutex>
#include <list>

namespace SPP
{
	class SPP_CORE_API PooledChunkBuffer
	{
	private:
		struct _chunk
		{
			const uint32_t size = 0;
			void* const data = nullptr;

			_chunk(void* DataLocation, uint32_t InSize);
		};

		std::mutex _chunkMutex;
		std::list< _chunk > _chunkPool;

	public:

		class Chunk : public _chunk
		{
		private:
			PooledChunkBuffer* const _parent = nullptr;

		public:
			Chunk(void* DataLocation, uint32_t InSize, PooledChunkBuffer* Parent);
			~Chunk();
		};

		PooledChunkBuffer();
		~PooledChunkBuffer();

		std::shared_ptr<Chunk> GetChunk(uint32_t DesiredSize);

		void GiveMemory(void* data, uint32_t size);
	};

}