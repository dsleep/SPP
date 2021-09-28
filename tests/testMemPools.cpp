// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPSTLUtils.h"
#include "SPPLogging.h"
#include "SPPWin32Core.h"
#include "SPPMemory.h"
#include <thread>
#include <chrono>

#include <random>

using namespace std::chrono_literals;
using namespace SPP;

LogEntry LOG_APP("APP");

class GPUFake
{
private:
	uint8_t* _data = nullptr;

public:
	GPUFake() = default;
	~GPUFake()
	{
		Free();
	}
	void Allocate(size_t InCount)
	{
		SE_ASSERT(_data == nullptr);
		_data = (uint8_t*)malloc(sizeof(uint8_t) * InCount);
	}
	void Free()
	{
		if (_data)
		{
			free(_data);
			_data = nullptr;
		}
	}
	uint8_t* operator[](size_t Index)
	{
		return _data + Index;
	}
};

int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

    SingleRequestor<uint8_t> dataRequestor(10 * 1024);

    {
        auto MyChunk = dataRequestor.Get();

    }

    BuddyAllocator<uint8_t, GPUFake > buddyData(1 * 1024 * 1024, 128);
	
	std::default_random_engine generator;
	std::uniform_int_distribution<int> memDist(64, 1 * 1024);

	{
		std::unique_ptr< BuddyAllocator<uint8_t, GPUFake >::Reservation  > reserves[150];

		for (int32_t Iter = 0; Iter < ARRAY_SIZE(reserves); Iter++)
		{
			reserves[Iter] = buddyData.Get(memDist(generator));
			SE_ASSERT(reserves[Iter]);
		}

		buddyData.Report();
	}

	buddyData.Report();
    return 0;
}
