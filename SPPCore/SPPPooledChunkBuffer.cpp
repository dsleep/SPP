// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPooledChunkBuffer.h"

namespace SPP
{
	PooledChunkBuffer::_chunk::_chunk(void* DataLocation, uint32_t InSize) : size(InSize), data(DataLocation)
	{


	};
	PooledChunkBuffer::Chunk::Chunk(void* DataLocation, uint32_t InSize, PooledChunkBuffer* Parent) : _chunk(DataLocation, InSize), _parent(Parent)
	{

	}
	PooledChunkBuffer::Chunk::~Chunk()
	{
		_parent->GiveMemory(data, size);
	}
	PooledChunkBuffer::PooledChunkBuffer() { }
	PooledChunkBuffer::~PooledChunkBuffer()
	{
		for (auto it = _chunkPool.begin(); it != _chunkPool.end(); ++it)
		{
			free(it->data);
		}
		_chunkPool.clear();
	}

	std::shared_ptr<PooledChunkBuffer::Chunk> PooledChunkBuffer::GetChunk(uint32_t DesiredSize)
	{
		SE_ASSERT(DesiredSize > 0);

		{
			std::unique_lock<std::mutex> lock(_chunkMutex);
			// just grab the last active if it works out
			for (auto it = _chunkPool.begin(); it != _chunkPool.end(); ++it)
			{
				auto currentChunk = *it;
				if (currentChunk.size >= DesiredSize)
				{
					// move to chunks
					_chunkPool.erase(it);
					return std::make_shared<Chunk>(currentChunk.data, currentChunk.size, this);
				}
			}

		}

		DesiredSize = ((DesiredSize / 1024) + 1) * 1024;

		void* newData = malloc(DesiredSize);
		auto newChunk = std::make_shared<Chunk>(newData, DesiredSize, this);
		return newChunk;
	}

	void PooledChunkBuffer::GiveMemory(void* data, uint32_t size)
	{
		std::unique_lock<std::mutex> lock(_chunkMutex);
		_chunkPool.push_back(PooledChunkBuffer::_chunk(data, size));
	}
}
