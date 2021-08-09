// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPMesh.h"
#include <vector>
#include <cstddef>

namespace SPP
{
	struct SPP_GRAPHICS_API TerrainVertex
	{
		Vector3 position;
	};


	class SPP_GRAPHICS_API Terrain
	{
	private:
		std::vector< std::shared_ptr<MeshElement> > _elements;

		Vector2 _patchesDimension;
		float _patchDensity = 0.1f;
		uint32_t _numVertices = 0;
		uint32_t _numIndices = 0;

		class TesslationMaterialObject* _terrainMaterial;

	public:
		void Create();

		std::vector< std::shared_ptr<MeshElement> >& GetMeshElements()
		{
			return _elements;
		}
	};
}