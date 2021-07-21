// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPCapture.h"
#include "SPPVideo.h"

#include <windows.h>
#include "processthreadsapi.h"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace SPP;
int main()
{
	IntializeCore(nullptr);

	SetDllDirectoryA("../3rdParty/libav_CUDA/bin");

	std::unique_ptr< VideoEncodingInterface> VideoEncoder;
	std::shared_ptr< RRMpegFileWriter > outFile;

	uint32_t ProcessID = 0;// CreateChildProcess("C:\\SurfLab\\SOFA\\bin\\runSofa.exe", "");
	int32_t Width = 0;
	int32_t Height = 0;
	uint8_t BytesPP = 0;
	std::vector<uint8_t> ImageData;
	int32_t frameCount = 0;


	auto LastTime = std::chrono::high_resolution_clock::now();
	while (CaptureApplicationWindow(ProcessID, Width, Height, ImageData, BytesPP))
	{
		if (!VideoEncoder)
		{
			outFile = std::make_shared< RRMpegFileWriter>("test.mp4", true, Width, Height, 24);
			VideoEncoder = CreateVideoEncoder([filePtr = outFile.get()](const void* InData, int32_t InDataSize)
				{ 
					printf("data chunk %d\n", InDataSize);
					filePtr->Write(InData, InDataSize);
				}, VideoSettings{ Width, Height, 4, 3, 24 }, {});				
		}


		auto CurrentTime = std::chrono::high_resolution_clock::now();
		auto milliTime = std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - LastTime).count();

		if (milliTime > 42)
		{
			VideoEncoder->Encode(ImageData.data(), ImageData.size());
			//std::this_thread::sleep_for(10ms);
			frameCount++;

			if (frameCount > (24 * 10))
			{
				break;
			}

			LastTime = CurrentTime;
		}
	}

	VideoEncoder->Finalize();
	VideoEncoder.reset();
	outFile.reset();

	return 0;
}