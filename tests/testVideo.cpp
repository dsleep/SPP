// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPSTLUtils.h"
#include "SPPLogging.h"
#include "SPPPlatformCore.h"
#include "SPPMemory.h"
#include <thread>
#include <chrono>
#include "SPPVideo.h"

#include <random>

using namespace std::chrono_literals;
using namespace SPP;

LogEntry LOG_APP("APP");

int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

	auto VideoEncoder = CreateVideoEncoder([](const void* InData, int32_t InDataSize)
		{
			SPP_QL("datachunk");

		}, VideoSettings{ 512, 512, 4, 3, 32 }, {});

	struct DummyColor
	{
		uint8_t R, G, B, A;
	};


	DummyColor color;
	std::vector< DummyColor > colors;
	colors.resize(512 * 512);

	while (true)
	{
		VideoEncoder->Encode(colors.data(), colors.size()*sizeof(DummyColor));
		std::this_thread::sleep_for(100ms);
	}
	return 0;
}
