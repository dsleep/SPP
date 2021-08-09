// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPTerrain.h"

namespace SPP
{
	void Terrain::Create()
	{
		auto newMeshElement = std::make_shared<MeshElement>();

		_patchesDimension = Vector2(5000, 5000);

		auto HalfDim = _patchesDimension / 2.0f;
		Vector3 HalfDim3(HalfDim[0], 0, HalfDim[1]);

		// vertices
		Vector2 patchVertexCountf = _patchesDimension * _patchDensity;
		Vector2i patchVertexCount = Vector2i(patchVertexCountf[0], patchVertexCountf[1]);
		Vector2 deltaVertexValueShift = Vector2(_patchesDimension[0] / (float)patchVertexCount[0], _patchesDimension[1] / (float)patchVertexCount[1]);

		_numVertices = patchVertexCount[0] * patchVertexCount[1];
		
		auto verticesResource = std::make_shared< ArrayResource >();
		auto terrVerts = verticesResource->InitializeFromType<TerrainVertex>(_numVertices);

		for (int32_t y = 0; y < patchVertexCount[1]; ++y)
		{
			for (int32_t x = 0; x < patchVertexCount[0]; ++x)
			{
				terrVerts[y * patchVertexCount[0] + x].position = Vector3((float)x * deltaVertexValueShift[0], 0, (float)y * deltaVertexValueShift[1]) - HalfDim3;
			}
		}

		// create an index buffer
		// our grid is scalePatchX * scalePatchY in size.
		// the vertices are oriented like so:
		//  0,  1,  2,  3,  4,
		//  5,  6,  7,  8,  9,
		// 10, 11, 12, 13, 14
				
		_numIndices = (patchVertexCount[0] - 1) * (patchVertexCount[1] - 1) * 4;
		auto indicesResource = std::make_shared< ArrayResource >();
		auto terrIndices = indicesResource->InitializeFromType<int32_t>(_numIndices);

		int i = 0;
		for (int y = 0; y < patchVertexCount[1] - 1; ++y) {
			for (int x = 0; x < patchVertexCount[0] - 1; ++x) {
				uint32_t vert0 = x + y * patchVertexCount[0];
				uint32_t vert1 = x + 1 + y * patchVertexCount[0];
				uint32_t vert2 = x + (y + 1) * patchVertexCount[0];
				uint32_t vert3 = x + 1 + (y + 1) * patchVertexCount[0];
				terrIndices[i++] = vert0;
				terrIndices[i++] = vert1;
				terrIndices[i++] = vert2;
				terrIndices[i++] = vert3;
			}
		}

		newMeshElement->VertexResource = GGI()->CreateStaticBuffer(GPUBufferType::Vertex, verticesResource);
		newMeshElement->IndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Index, indicesResource);

		_elements.push_back(newMeshElement);
	}

}