// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPooledChunkBuffer.h"

namespace SPP
{
	PooledChunkBuffer::ReusableChunk::ReusableChunk(void* DataLocation, 
		size_t InSize, 
		size_t InTotalSize,
		PooledChunkBuffer* Parent) : MemoryChunk(DataLocation, InSize), _totaSize(InTotalSize), _parent(Parent)
	{
	}
	PooledChunkBuffer::ReusableChunk::~ReusableChunk()
	{
		_parent->GiveMemory(data, _totaSize);
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

	std::shared_ptr<MemoryChunk> PooledChunkBuffer::GetChunk(size_t DesiredSize)
	{
		SE_ASSERT(DesiredSize > 0);

		{
			std::unique_lock<std::mutex> lock(_chunkMutex);
			// just grab the last active if it works out
			for (auto it = _chunkPool.begin(); it != _chunkPool.end(); ++it)
			{
				auto currentChunk = *it;
				if (currentChunk.size >= DesiredSize) // check for < half or something? TODO
				{
					// move to chunks
					_chunkPool.erase(it);
					return std::make_shared<ReusableChunk>(currentChunk.data,
						DesiredSize, // set its size to the requested amount
						currentChunk.size, // also remember original size
						this);
				}
			}
		}

		auto KBPaddedSize = ((DesiredSize / 1024) + 1) * 1024;

		void* newData = malloc(KBPaddedSize);
		auto newChunk = std::make_shared<ReusableChunk>(newData, 
			DesiredSize, 
			KBPaddedSize,
			this);
		return newChunk;
	}

	void PooledChunkBuffer::GiveMemory(void* data, size_t size)
	{
		std::unique_lock<std::mutex> lock(_chunkMutex);
		_chunkPool.push_back(MemoryChunk(data, size));
	}
}
