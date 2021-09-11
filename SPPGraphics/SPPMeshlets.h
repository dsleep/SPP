// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMesh.h"
#include "SPPLogging.h"
#include <unordered_set>
#include <unordered_map>
#include <chrono>

namespace SPP
{
	SPP_GRAPHICS_API extern LogEntry LOG_MESHLET;

	struct Meshlet
	{
		uint32_t VertCount;
		uint32_t VertOffset;
		uint32_t PrimCount;
		uint32_t PrimOffset;
	};

	union PackedTriangle
	{
		struct
		{
			uint32_t i0 : 10;
			uint32_t i1 : 10;
			uint32_t i2 : 10;
			uint32_t _unused : 2;
		};
		uint32_t packed;
	};

	struct Triangle
	{
		uint32_t i0;
		uint32_t i1;
		uint32_t i2;
		uint32_t orgTriIdx;
	};

	template<>
	inline BinarySerializer& operator<< <Triangle> (BinarySerializer& Storage, const Triangle& Value)
	{
		Storage << Value.i0;
		Storage << Value.i1;
		Storage << Value.i2;
		Storage << Value.orgTriIdx;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator>> <Triangle> (BinarySerializer& Storage, Triangle& Value)
	{
		Storage >> Value.i0;
		Storage >> Value.i1;
		Storage >> Value.i2;
		Storage >> Value.orgTriIdx;
		return Storage;
	}

	template <typename T>
	struct InlineMeshlet
	{
		std::vector<T>              UniqueVertexIndices;
		std::vector<PackedTriangle> PrimitiveIndices;

		size_t GetTriCount() const
		{
			return PrimitiveIndices.size();
		}

		bool HasIndex(T IndexIn) const
		{
			for (uint32_t i = 0; i < static_cast<uint32_t>(UniqueVertexIndices.size()); ++i)
			{
				if (UniqueVertexIndices[i] == IndexIn)
				{
					return true;
				}
			}
			return false;
		}
	};

	struct MetaMeshlet
	{
		std::set<uint32_t> UniqueVertexIndices;
		std::vector<Triangle> Triangles;
		
		size_t GetTriCount() const
		{
			return Triangles.size();
		}

		bool HasIndex(uint32_t IndexIn) const
		{
			return UniqueVertexIndices.find(IndexIn) != UniqueVertexIndices.end();
		}

		InlineMeshlet<uint32_t> ToInline() const
		{
			InlineMeshlet<uint32_t> oMeshlet;

			for (const auto &idx : UniqueVertexIndices)
			{
				oMeshlet.UniqueVertexIndices.push_back(idx);
			}

			for (const auto& tri : Triangles)
			{				
				PackedTriangle prim = {};
				prim.i0 = std::distance(UniqueVertexIndices.begin(), UniqueVertexIndices.find(tri.i0));
				prim.i1 = std::distance(UniqueVertexIndices.begin(), UniqueVertexIndices.find(tri.i1));
				prim.i2 = std::distance(UniqueVertexIndices.begin(), UniqueVertexIndices.find(tri.i2));
				oMeshlet.PrimitiveIndices.push_back(prim);
			}

			return oMeshlet;
		}
	};

	template<>
	inline BinarySerializer& operator<< <MetaMeshlet> (BinarySerializer& Storage, const MetaMeshlet& Value)
	{
		{
			std::vector<uint32_t> setAsVector(Value.UniqueVertexIndices.begin(), Value.UniqueVertexIndices.end());
			Storage << setAsVector;
		}
		Storage << Value.Triangles;

		return Storage;
	}
	template<>
	inline BinarySerializer& operator>> <MetaMeshlet> (BinarySerializer& Storage, MetaMeshlet& Value)
	{	
		{
			std::vector<uint32_t> setAsVector;
			Storage >> setAsVector;
			Value.UniqueVertexIndices = std::set<uint32_t>(setAsVector.begin(), setAsVector.end());
		}
		Storage >> Value.Triangles;
		return Storage;
	}

	
	template<>
	inline BinarySerializer& operator<< <MeshNode> (BinarySerializer& Storage, const MeshNode& Value)
	{
		Storage << Value.Bounds;
		Storage << Value.TriCount;
		Storage << Value.ChildTriCount;
		Storage << std::get<0>(Value.ChildrenRange);
		Storage << std::get<1>(Value.ChildrenRange);
		Storage << std::get<0>(Value.MeshletRange);
		Storage << std::get<1>(Value.MeshletRange);
		return Storage;
	}
	template<>
	inline BinarySerializer& operator>> <MeshNode> (BinarySerializer& Storage, MeshNode& Value)
	{
		Storage >> Value.Bounds;
		Storage >> Value.TriCount;
		Storage >> Value.ChildTriCount;
		Storage >> std::get<0>(Value.ChildrenRange);
		Storage >> std::get<1>(Value.ChildrenRange);
		Storage >> std::get<0>(Value.MeshletRange);
		Storage >> std::get<1>(Value.MeshletRange);
		return Storage;
	}

	struct CullData
	{
		Vector4 BoundingSphere;					// xyz = center, w = radius
		std::array< uint8_t, 4 >  NormalCone;	// xyz = axis, w = sin(a + 90)
		float ApexOffset;						// apex = center - axis * offset
	};
	

	template <typename MeshletType>
	bool IsMeshletFull(uint32_t maxVerts, uint32_t maxPrims, const MeshletType& meshlet)
	{
		SE_ASSERT(meshlet.UniqueVertexIndices.size() <= maxVerts);
		SE_ASSERT(meshlet.GetTriCount() <= maxPrims);

		return meshlet.UniqueVertexIndices.size() == maxVerts
			|| meshlet.GetTriCount() == maxPrims;
	}

	// Determines whether a candidate triangle can be added to a specific meshlet; if it can, does so.
	template <typename T>
	bool AddToMeshlet(uint32_t maxVerts, uint32_t maxPrims, InlineMeshlet<T>& meshlet, T(&tri)[3])
	{
		// Are we already full of vertices?
		if (meshlet.UniqueVertexIndices.size() == maxVerts)
			return false;

		// Are we full, or can we store an additional primitive?
		if (meshlet.PrimitiveIndices.size() == maxPrims)
			return false;

		static const uint32_t Undef = uint32_t(-1);
		uint32_t indices[3] = { Undef, Undef, Undef };
		uint32_t newCount = 3;

		for (uint32_t i = 0; i < meshlet.UniqueVertexIndices.size(); ++i)
		{
			for (uint32_t j = 0; j < 3; ++j)
			{
				if (meshlet.UniqueVertexIndices[i] == tri[j])
				{
					indices[j] = i;
					--newCount;
				}
			}
		}

		// Will this triangle fit?
		if (meshlet.UniqueVertexIndices.size() + newCount > maxVerts)
			return false;

		// Add unique vertex indices to unique vertex index list
		for (uint32_t j = 0; j < 3; ++j)
		{
			if (indices[j] == Undef)
			{
				indices[j] = static_cast<uint32_t>(meshlet.UniqueVertexIndices.size());
				meshlet.UniqueVertexIndices.push_back(tri[j]);
			}
		}

		// Add the new primitive 
		PackedTriangle prim = {};
		prim.i0 = indices[0];
		prim.i1 = indices[1];
		prim.i2 = indices[2];

		meshlet.PrimitiveIndices.push_back(prim);

		return true;
	}

	// Determines whether a candidate triangle can be added to a specific meshlet; if it can, does so.
	template <typename T>
	bool AddToMeshlet(uint32_t maxVerts, uint32_t maxPrims, MetaMeshlet& meshlet, T(&tri)[3], uint32_t OrgTriIdx)
	{
		auto find0 = meshlet.UniqueVertexIndices.find(tri[0]);
		auto find1 = meshlet.UniqueVertexIndices.find(tri[1]);
		auto find2 = meshlet.UniqueVertexIndices.find(tri[2]);

		size_t willAdd = 0;
		if (find0 == meshlet.UniqueVertexIndices.end()) willAdd++;
		if (find1 == meshlet.UniqueVertexIndices.end()) willAdd++;
		if (find2 == meshlet.UniqueVertexIndices.end()) willAdd++;
	
		// Are we already full of vertices?
		if (meshlet.UniqueVertexIndices.size() + willAdd >= maxVerts)
			return false;
		// Are we full, or can we store an additional primitive?
		if (meshlet.Triangles.size() == maxPrims)
			return false;

		meshlet.UniqueVertexIndices.insert(tri[0]);
		meshlet.UniqueVertexIndices.insert(tri[1]);
		meshlet.UniqueVertexIndices.insert(tri[2]);
		meshlet.Triangles.push_back( { (uint32_t)tri[0], (uint32_t)tri[1], (uint32_t)tri[2], OrgTriIdx } );

		return true;
	}
	
	//template<typename MeshVertexType, typename IndexType>
	class Meshletizer
	{
	private:
		AssetPath _assetPath;
		std::shared_ptr< MeshTranslator> _translator;

		uint32_t _maxVerts;
		uint32_t _maxPrims;
		
		AABB _bounds;		
		std::vector< MeshNode > _nodes;
		std::vector< MetaMeshlet > _meshlets;		
						
		void _Meshletize();

	public:
		Meshletizer(const AssetPath &InAssetPath, std::shared_ptr< MeshTranslator> InTranslator, uint32_t InMaxVerts, uint32_t InMaxPrims);
		
		bool ComputeMeshlets(
			std::vector<Subset>& meshletSubsets,
			std::vector<Meshlet>& meshlets,
			std::vector<MeshNode>& meshnodes,
			std::vector<uint8_t>& uniqueVertexIndices,
			std::vector<PackedTriangle>& primitiveIndices);
	};
}