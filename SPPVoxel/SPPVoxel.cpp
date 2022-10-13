// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVoxel.h"
#include "SPPMath.h"

#include "CubicSurfaceExtractor.h"
#include "MarchingCubesSurfaceExtractor.h"
#include "Mesh.h"
#include "RawVolume.h"
#include "PagedVolume.h"

SPP_OVERLOAD_ALLOCATORS


namespace SPP
{
	class VoxelMesh
	{
	private:
		std::unique_ptr< PolyVox::RawVolume<uint8_t> > _voxelData;


		VoxelMesh(const Vector3i& BoxMin, const Vector3i& BoxMax)
		{
			_voxelData = std::make_unique< PolyVox::RawVolume<uint8_t> >(
				PolyVox::Region(PolyVox::Vector3DInt32(BoxMin[0], BoxMin[1], BoxMin[2]),
					PolyVox::Vector3DInt32(BoxMax[0], BoxMax[1], BoxMax[2]))
			);
		}
	};
	void CreateVoxelStuff()
	{
		// Create an empty volume and then place a sphere in it
		PolyVox::RawVolume<uint8_t> volData(PolyVox::Region(PolyVox::Vector3DInt32(0, 0, 0), PolyVox::Vector3DInt32(63, 63, 63)));
		//createSphereInVolume(volData, 30);

		// Extract the surface for the specified region of the volume. Uncomment the line for the kind of surface extraction you want to see.
		auto mesh = PolyVox::extractCubicMesh(&volData, volData.getEnclosingRegion());
		//auto mesh = extractMarchingCubesMesh(&volData, volData.getEnclosingRegion());

		// The surface extractor outputs the mesh in an efficient compressed format which is not directly suitable for rendering. The easiest approach is to 
		// decode this on the CPU as shown below, though more advanced applications can upload the compressed mesh to the GPU and decompress in shader code.
		auto decodedMesh = PolyVox::decodeMesh(mesh);

	}
	int32_t GetVoxelVersion()
	{
		return 1;
	}
}
