// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPMesh.h"
#include "SPPSerialization.h"
#include "SPPLogging.h"
//#include "SPPAssetCache.h"
#include "SPPMeshlets.h"
#include "SPPMeshSimplifying.h"
#include "SPPLogging.h"

#include "SPPAssetImporterFile.h"
#include "SPPBlenderFile.h"
#include "SPPFileSystem.h"

namespace SPP
{
	LogEntry LOG_MESH("MESH");

	void DrawAABB(const AABB& InAABB, std::vector< DebugVertex >& lines)
	{
		auto minValue = InAABB.GetMin().cast<float>();
		auto maxValue = InAABB.GetMax().cast<float>();

		Vector3 topPoints[4];
		Vector3 bottomPoints[4];

		topPoints[0] = Vector3(minValue[0], minValue[1], minValue[2]);
		topPoints[1] = Vector3(maxValue[0], minValue[1], minValue[2]);
		topPoints[2] = Vector3(maxValue[0], minValue[1], maxValue[2]);
		topPoints[3] = Vector3(minValue[0], minValue[1], maxValue[2]);

		bottomPoints[0] = Vector3(minValue[0], maxValue[1], minValue[2]);
		bottomPoints[1] = Vector3(maxValue[0], maxValue[1], minValue[2]);
		bottomPoints[2] = Vector3(maxValue[0], maxValue[1], maxValue[2]);
		bottomPoints[3] = Vector3(minValue[0], maxValue[1], maxValue[2]);

		for (int32_t Iter = 0; Iter < 4; Iter++)
		{
			int32_t nextPoint = (Iter + 1) % 4;

			lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
			lines.push_back({ topPoints[nextPoint], Vector3(1,1,1) });

			lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
			lines.push_back({ bottomPoints[nextPoint], Vector3(1,1,1) });

			lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
			lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
		}
	}

	void DrawSphere(const Sphere& InSphere, std::vector< DebugVertex >& lines)
	{
		if (InSphere)
		{
			auto sphRad = InSphere.GetRadius();
			Vector3 RadiusVec = { sphRad, sphRad, sphRad };
			DrawAABB(AABB(InSphere.GetCenter().cast<float>() - RadiusVec, InSphere.GetCenter().cast<float>() + RadiusVec), lines);
		}
	}

	template<>
	inline BinarySerializer& operator<< <Meshlet> (BinarySerializer& Storage, const Meshlet& Value)
	{
		Storage << Value.VertCount;
		Storage << Value.VertOffset;
		Storage << Value.PrimCount;
		Storage << Value.PrimOffset;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator<< <PackedTriangle> (BinarySerializer& Storage, const PackedTriangle& Value)
	{
		Storage << Value.packed;
		return Storage;
	}

	bool Mesh::LoadSimpleBinaryMesh(const char* FileName)
	{
		std::vector<uint8_t> fileData;
		if (!LoadFileToArray(FileName, fileData))
		{
			return false;
		}

		_bounds = Sphere();

		BinaryBlobSerializer binaryData(fileData);

		uint32_t MajorVersion = 0;
		binaryData >> MajorVersion;
		uint32_t MinorVersion = 0;
		binaryData >> MinorVersion;

		uint32_t VertFlags = 0;
		binaryData >> VertFlags;

		bool bHasPosition = ((VertFlags & (1 << 0)) != 0);
		bool bHasUV = ((VertFlags & (1 << 1)) != 0);
		bool bHasNormal = ((VertFlags & (1 << 2)) != 0);
		bool bHasTangent = ((VertFlags & (1 << 3)) != 0);
		bool bHasLightMapUV = ((VertFlags & (1 << 4)) != 0);

		uint32_t VertCount = 0;
		binaryData >> VertCount;

		auto VertexResource = std::make_shared< ArrayResource >();
		auto IndexResource = std::make_shared< ArrayResource >();

		auto curVerts = VertexResource->InitializeFromType<MeshVertex>(VertCount);
		auto curIndices = IndexResource->InitializeFromType<uint32_t>(VertCount * 3);

		for (uint32_t Iter = 0; Iter < VertCount; Iter++)
		{
			MeshVertex& vertex = curVerts[Iter];

			binaryData >> vertex.position[0];
			binaryData >> vertex.position[1];
			binaryData >> vertex.position[2];

			binaryData >> vertex.texcoord[0][0];
			binaryData >> vertex.texcoord[0][1];

			if (bHasLightMapUV)
			{
				binaryData >> vertex.texcoord[1][0];
				binaryData >> vertex.texcoord[1][1];
			}

			binaryData >> vertex.normal[0];
			binaryData >> vertex.normal[1];
			binaryData >> vertex.normal[2];

			binaryData >> vertex.tangent[0];
			binaryData >> vertex.tangent[1];
			binaryData >> vertex.tangent[2];

			//calc bitagent

			curIndices[Iter] = Iter;
		}

		auto newMeshElement = std::make_shared<MeshElement>();
		newMeshElement->VertexResource = VertexResource;
		newMeshElement->IndexResource = IndexResource;
		newMeshElement->Bounds = MinimumBoundingSphere< MeshVertex, Vector3 >(curVerts.GetData(), VertCount);
		newMeshElement->Name = "smbin";

		_elements.push_back(newMeshElement);
		_bounds += newMeshElement->Bounds;

		return true;
	}
	
	bool Mesh::LoadMesh(const char *FileName)
	{
		SPP_LOG(LOG_MESH, LOG_INFO, "Loading Mesh: %s", FileName);

		std::shared_ptr<BinaryBlobSerializer> FoundCachedBlob;// = false;// GetCachedAsset(FileName);

		if (false)//FoundCachedBlob)
		{
			_bounds = Sphere();

			BinaryBlobSerializer& blobAsset = *FoundCachedBlob;

			uint32_t LayerCount = 0;
			blobAsset >> LayerCount;

			for (uint32_t Iter = 0; Iter < LayerCount; Iter++)
			{
				Sphere curBounds;
				std::string LayerName;
				auto VertexResource = std::make_shared< ArrayResource >();
				auto IndexResource = std::make_shared< ArrayResource >();

				blobAsset >> curBounds;
				blobAsset >> LayerName;
				blobAsset >> *VertexResource;
				blobAsset >> *IndexResource;

				auto newMeshElement = std::make_shared<MeshElement>();
				newMeshElement->VertexResource = VertexResource;// GGI()->CreateStaticBuffer(GPUBufferType::Vertex, VertexResource);
				newMeshElement->IndexResource = IndexResource;// GGI()->CreateStaticBuffer(GPUBufferType::Index, IndexResource);
				newMeshElement->Bounds = curBounds;
				newMeshElement->Name = LayerName;

				_elements.push_back(newMeshElement);
				_bounds += newMeshElement->Bounds;
				//GGI()->RegisterMeshElement(newMeshElement);
			}


			//uint32_t MeshCount = 0;
			//blobAsset >> MeshCount;

			//for (uint32_t Iter = 0; Iter < MeshCount; Iter++)
			//{
			//	AABB meshBounds;
			//	blobAsset >> meshBounds;

			//	std::vector<Subset>  meshletSubsets;
			//	blobAsset >> meshletSubsets;

			//	auto newMeshElement = std::make_shared<MeshElement>();
			//	std::swap(newMeshElement->MeshletSubsets, meshletSubsets);
			//	newMeshElement->Bounds = meshBounds;

			//	{
			//		auto meshShaderResource = std::make_shared< ArrayResource >();
			//		blobAsset >> *meshShaderResource;
			//		newMeshElement->MeshletResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
			//	}
			//	{
			//		auto meshShaderResource = std::make_shared< ArrayResource >();
			//		blobAsset >> *meshShaderResource;
			//		newMeshElement->UniqueVertexIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
			//	}
			//	{
			//		auto meshShaderResource = std::make_shared< ArrayResource >();
			//		blobAsset >> *meshShaderResource;
			//		newMeshElement->PrimitiveIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
			//	}


			//	auto verticesResource = std::make_shared< ArrayResource >();
			//	auto indicesResource = std::make_shared< ArrayResource >();

			//	blobAsset >> *verticesResource;
			//	blobAsset >> *indicesResource;

			//	newMeshElement->VertexResource = GGI()->CreateStaticBuffer(GPUBufferType::Vertex, verticesResource);
			//	newMeshElement->IndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Index, indicesResource);

			//	_elements.push_back(newMeshElement);

			//	//RegisterMeshElement(newMeshElement);
			//}
		}
		else
		{
			// create cache
			BinaryBlobSerializer outCachedAsset;

			LoadedMeshes loadedMeshes;
#if 0
			LoadBlenderFile(FileName, loadedMeshes);			
#else
			LoadUsingAssImp(FileName, loadedMeshes);
#endif
			_bounds = Sphere();

			uint32_t LayerCount = loadedMeshes.Layers.size();
			outCachedAsset << LayerCount;

			for (auto& curLayer : loadedMeshes.Layers)
			{
				auto newMeshElement = std::make_shared<MeshElement>();
				newMeshElement->VertexResource = curLayer.VertexResource;
				newMeshElement->IndexResource = curLayer.IndexResource;
				newMeshElement->Bounds = curLayer.bounds;
				newMeshElement->Name = curLayer.Name;

				outCachedAsset << curLayer.bounds;
				outCachedAsset << curLayer.Name;
				outCachedAsset << *curLayer.VertexResource;
				outCachedAsset << *curLayer.IndexResource;

				_elements.push_back(newMeshElement);
				_bounds += newMeshElement->Bounds;
				//GGI()->RegisterMeshElement(newMeshElement);
			}


#if 0
			for(auto &curLayer : loadedMeshes.Layers)
			{
				uint32_t MeshletMaxVerts = 64;
				uint32_t MeshletMaxPrims = 126;
				
				std::vector<Subset>                     meshletSubsets;
				std::vector<Meshlet>                    meshlets;
				std::vector<MeshNode>                   meshletNodes;
				std::vector<uint8_t>                    uniqueVertexIndices;
				std::vector<PackedTriangle>             primitiveIndices;

				//std::vector<CullData>                   m_cullData;
				Meshletizer  tizing(FileName, std::make_shared< MeshVertexTranslator >(curLayer.VertexResource, curLayer.IndexResource), MeshletMaxVerts, MeshletMaxPrims);
				tizing.ComputeMeshlets(
					meshletSubsets,
					meshlets,
					meshletNodes,
					uniqueVertexIndices,
					primitiveIndices);

				auto newMeshElement = std::make_shared<MeshElement>();
				std::swap(newMeshElement->MeshletSubsets, meshletSubsets);
				std::swap(newMeshElement->MeshletNodes, meshletNodes);
				//newMeshElement->Bounds = meshBounds;

				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					auto pMeshlets = meshShaderResource->InitializeFromType<Meshlet>(meshlets.size());
					memcpy(pMeshlets, meshlets.data(), sizeof(Meshlet) * meshlets.size());
					newMeshElement->MeshletResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}
				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					auto puniqueVertexIndices = meshShaderResource->InitializeFromType<Meshlet>(DivRoundUp(uniqueVertexIndices.size(), 4));
					memcpy(puniqueVertexIndices, uniqueVertexIndices.data(), sizeof(uint8_t) * uniqueVertexIndices.size());
					newMeshElement->UniqueVertexIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}
				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					auto pprimitiveIndices = meshShaderResource->InitializeFromType<PackedTriangle>(primitiveIndices.size());
					memcpy(pprimitiveIndices, primitiveIndices.data(), sizeof(PackedTriangle) * primitiveIndices.size());
					newMeshElement->PrimitiveIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}

				newMeshElement->VertexResource = GGI()->CreateStaticBuffer(GPUBufferType::Vertex, curLayer.VertexResource);
				newMeshElement->IndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Index, curLayer.IndexResource);

				_elements.push_back(newMeshElement);
				//RegisterMeshElement(newMeshElement);
			}
#endif

//			Assimp::Importer importer;
//			const aiScene* scene = importer.ReadFile(*FileName, GeneicStaticImportFlags);
//
//			uint32_t MeshCount = scene ? scene->mNumMeshes : 0;
//			outCachedAsset << MeshCount;
//
//			if (scene && scene->HasMeshes())
//			{
//				for (uint32_t Iter = 0; Iter < scene->mNumMeshes; Iter++)
//				{
//					AABB meshBounds;
//
//					//just grab the first
//					auto mesh = scene->mMeshes[Iter];
//
//					auto verticesResource = std::make_shared< ArrayResource >();
//					{
//						auto pvertices = verticesResource->InitializeFromType< MeshVertex>(mesh->mNumVertices);
//						for (size_t i = 0; i < mesh->mNumVertices; ++i)
//						{
//							MeshVertex& vertex = pvertices[i];
//							vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
//
//							meshBounds += vertex.position;
//
//							if (mesh->HasNormals())
//							{
//								vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
//							}
//							if (mesh->HasTangentsAndBitangents()) {
//								vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
//								vertex.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
//							}
//							if (mesh->HasTextureCoords(0)) {
//								vertex.texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
//							}
//						}
//					}
//
//					auto indicesResource = std::make_shared< ArrayResource >();
//					{
//						auto pindices = indicesResource->InitializeFromType<uint32_t>(mesh->mNumFaces * 3);
//						for (size_t i = 0; i < mesh->mNumFaces; ++i)
//						{
//							SE_ASSERT(mesh->mFaces[i].mNumIndices == 3);
//
//							pindices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
//							pindices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
//							pindices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
//						}
//					}
//
//					SPP_LOG(LOG_MESH, LOG_INFO, " - verts %d", verticesResource->GetElementCount());
//					SPP_LOG(LOG_MESH, LOG_INFO, " - indices %d tris %d", indicesResource->GetElementCount(), indicesResource->GetElementCount() / 3);
//
//#if 0
//					{
//						Simplify::FastQuadricMeshSimplification meshSimplification(std::make_shared< MeshVertexTranslator >(*verticesResource, *indicesResource));
//						meshSimplification.simplify_mesh(mesh->mNumFaces * 0.5, 7.0, true);
//					}
//
//					SPP_LOG(LOG_MESH, LOG_INFO, " - REDUCED verts %d", verticesResource->GetElementCount());
//					SPP_LOG(LOG_MESH, LOG_INFO, " - REDUCED indices %d tris %d", indicesResource->GetElementCount(), indicesResource->GetElementCount() / 3);
//#endif
//
//					TSpan<MeshVertex> pvertices = verticesResource->GetSpan<MeshVertex>();
//					TSpan<uint32_t> pindices = indicesResource->GetSpan<uint32_t>();
//					auto TotalIndexCount = indicesResource->GetElementCount();
//
//					uint32_t MeshletMaxVerts = 64;
//					uint32_t MeshletMaxPrims = 126;
//
//					std::vector<Subset>                     m_indexSubsets;
//					m_indexSubsets.push_back(Subset{ 0, (uint32_t)TotalIndexCount });
//
//					std::vector<Subset>                     meshletSubsets;
//					std::vector<Meshlet>                    meshlets;
//					std::vector<uint8_t>                    uniqueVertexIndices;
//					std::vector<PackedTriangle>             primitiveIndices;
//
//					//std::vector<CullData>                   m_cullData;
//					Meshletizer  tizing(std::make_shared< MeshVertexTranslator >(verticesResource, indicesResource), MeshletMaxVerts, MeshletMaxPrims);
//					tizing.ComputeMeshlets(
//						meshletSubsets,
//						meshlets,
//						uniqueVertexIndices,
//						primitiveIndices);
//
//					// Meshletize our mesh and generate per-meshlet culling data
//					//ComputeMeshlets<MeshVertex, uint32_t>(
//					//	MeshletMaxVerts, MeshletMaxPrims,
//					//	pindices, TotalIndexCount,
//					//	m_indexSubsets.data(), static_cast<uint32_t>(m_indexSubsets.size()),
//					//	pvertices, pvertices.GetCount(),
//					//	meshletSubsets,
//					//	meshlets,
//					//	uniqueVertexIndices,
//					//	primitiveIndices
//					//	);
//
//					SPP_LOG(LOG_MESH, LOG_INFO, " - meshlets created %d", meshlets.size());
//
//					auto newMeshElement = std::make_shared<MeshElement>();
//					std::swap(newMeshElement->MeshletSubsets, meshletSubsets);
//					newMeshElement->Bounds = meshBounds;
//
//					outCachedAsset << newMeshElement->Bounds;
//					outCachedAsset << newMeshElement->MeshletSubsets;
//
//					{
//						auto meshShaderResource = std::make_shared< ArrayResource >();
//						auto pMeshlets = meshShaderResource->InitializeFromType<Meshlet>(meshlets.size());
//						memcpy(pMeshlets, meshlets.data(), sizeof(Meshlet) * meshlets.size());
//						newMeshElement->MeshletResource = CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
//						outCachedAsset << *meshShaderResource;
//					}
//					{
//						auto meshShaderResource = std::make_shared< ArrayResource >();
//						auto puniqueVertexIndices = meshShaderResource->InitializeFromType<Meshlet>(DivRoundUp(uniqueVertexIndices.size(), 4));
//						memcpy(puniqueVertexIndices, uniqueVertexIndices.data(), sizeof(uint8_t) * uniqueVertexIndices.size());
//						newMeshElement->UniqueVertexIndexResource = CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
//						outCachedAsset << *meshShaderResource;
//					}
//					{
//						auto meshShaderResource = std::make_shared< ArrayResource >();
//						auto pprimitiveIndices = meshShaderResource->InitializeFromType<PackedTriangle>(primitiveIndices.size());
//						memcpy(pprimitiveIndices, primitiveIndices.data(), sizeof(PackedTriangle) * primitiveIndices.size());
//						newMeshElement->PrimitiveIndexResource = CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
//						outCachedAsset << *meshShaderResource;
//					}
//
//					newMeshElement->VertexResource = CreateStaticBuffer(GPUBufferType::Vertex, verticesResource);
//					newMeshElement->IndexResource = CreateStaticBuffer(GPUBufferType::Index, indicesResource);
//
//					outCachedAsset << *verticesResource;
//					outCachedAsset << *indicesResource;

				//	_elements.push_back(newMeshElement);

				//	RegisterMeshElement(newMeshElement);
				//}
			//}
			//else
			//{
			//	return false;
			//}

			//TODO DISABLED
			//PutCachedAsset(FileName, outCachedAsset);
		}

		return true;
	}


#if 0
	static uint32_t GHighestMaterialID = 0;
	static std::list<uint32_t> GAvailableMatIDs;

	struct PBRMaterialIDContainer
	{
		uint32_t albedoTextureID;
		uint32_t normalTextureID;
		uint32_t metalnessTextureID;
		uint32_t roughnessTextureID;
		uint32_t specularTextureID;
		uint32_t irradianceTextureID;
		uint32_t specularBRDF_LUTTextureID;
		uint32_t maskedTextureID;
	};

#define MAX_MATERIALS 1024

	class GlobalPRBMaterials : public GPUResource
	{
	private:
		std::shared_ptr< ArrayResource > _materialIDResource;
		GPUReferencer< GPUBuffer > _materialGPUBuffer;
		std::set<uint32_t> _updates;

	public:
		GlobalPRBMaterials()
		{
			_materialIDResource = std::make_shared< ArrayResource >();
			auto pMeshInfos = _materialIDResource->InitializeFromType<PBRMaterialIDContainer>(MAX_MATERIALS);
			memset(pMeshInfos, 0, _materialIDResource->GetTotalSize());
			_materialGPUBuffer = GGI()->CreateStaticBuffer(GPUBufferType::Generic, _materialIDResource);
		}

		virtual const char* GetName() const override
		{
			return "GlobalPRBMaterials";
		}

		void SetData(const PBRMaterialIDContainer &InData, uint32_t InID)
		{
			auto dataSpan = _materialIDResource->GetSpan< PBRMaterialIDContainer>();
			dataSpan[InID] = InData;
			_updates.insert(InID);
		}

		virtual void _MakeResident() override
		{
			if (!_updates.empty())
			{
				// if anything dirty
				for (auto& updateID : _updates)
				{
					_materialGPUBuffer->UpdateDirtyRegion(updateID, 1);
				}
			}
		}
	};

	GPUReferencer<GlobalPRBMaterials> GPBRMaterials;

	PBRMaterialAsset::PBRMaterialAsset()
	{
		if (!GPBRMaterials)
		{
			GPBRMaterials = Make_GPU< GlobalPRBMaterials >();
		}

		if (!GAvailableMatIDs.empty())
		{
			_uniqueID = GAvailableMatIDs.front();
			GAvailableMatIDs.pop_front();
		}
		else
		{
			_uniqueID = GHighestMaterialID++;
		}
	}

	PBRMaterialAsset::~PBRMaterialAsset()
	{
		GAvailableMatIDs.push_back(_uniqueID);
	}

	void PBRMaterialAsset::SetData()
	{
		PBRMaterialIDContainer pbrData{
			albedo ? albedo->GetID() : 0,
			normal ? normal->GetID() : 0,
			metalness ? metalness->GetID() : 0,
			roughness ? roughness->GetID() : 0,
			specular ? specular->GetID() : 0,
			irradiance ? irradiance->GetID() : 0,
			specularBRDF_LUT ? specularBRDF_LUT->GetID() : 0,
			masked ? masked->GetID() : 0,
		};

		GPBRMaterials->SetData(pbrData, _uniqueID);
	}
#endif
}