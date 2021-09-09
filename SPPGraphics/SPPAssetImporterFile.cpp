// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPAssetImporterFile.h"
#include "SPPSerialization.h"
#include "SPPAssetCache.h"
#include "SPPMeshlets.h"
#include "SPPMeshSimplifying.h"
#include "SPPLogging.h"

#include <functional>
#include <unordered_set>
#include <cstdio>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#pragma comment( lib, "assimp-vc142-mtd.lib" )

namespace SPP
{
	LogEntry LOG_ASSIMP("ASSIMP");
	
	const unsigned int GeneicStaticImportFlags = 
		//aiProcess_CalcTangentSpace |
		aiProcess_Triangulate;// |
		//aiProcess_SortByPType |
		//aiProcess_GenNormals |
		//aiProcess_GenUVCoords |
		//aiProcess_OptimizeMeshes |
		//aiProcess_OptimizeGraph |
		//aiProcess_Debone |
		//aiProcess_JoinIdenticalVertices |
		//a/iProcess_ValidateDataStructure;

	struct LogStream : public Assimp::LogStream
	{
		static void initialize()
		{
			if (Assimp::DefaultLogger::isNullLogger()) {
				Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
				Assimp::DefaultLogger::get()->attachStream(new LogStream, Assimp::Logger::Err | Assimp::Logger::Warn);
			}
		}

		void write(const char* message) override
		{
			SPP_LOG(LOG_ASSIMP, LOG_INFO, "ASSIMP: %s", message);
		}
	};

	bool LoadUsingAssImp(const AssetPath& FileName, LoadedMeshes &oMeshes)
	{
		LogStream::initialize();

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(*FileName, GeneicStaticImportFlags);

		uint32_t MeshCount = scene ? scene->mNumMeshes : 0;
		if (scene && scene->HasMeshes())
		{
			for (uint32_t Iter = 0; Iter < scene->mNumMeshes; Iter++)
			{
				AABB meshBounds;

				//just grab the first
				auto mesh = scene->mMeshes[Iter];

				auto& layer = oMeshes.Layers.emplace_back(LoadedMeshes::MeshLayer{ std::make_shared<ArrayResource >(),std::make_shared <ArrayResource >() });

				layer.MaterialID = mesh->mMaterialIndex;

				{
					auto pvertices = layer.VertexResource->InitializeFromType< MeshVertex>(mesh->mNumVertices);
					for (size_t i = 0; i < mesh->mNumVertices; ++i)
					{
						MeshVertex& vertex = pvertices[i];
						vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

						meshBounds += vertex.position;

						if (mesh->HasVertexColors(0))
						{
							vertex.color = {
								(uint8_t)std::clamp<float>(mesh->mColors[0][i].r * 255.0f, 0.0f, 255.0f),
								(uint8_t)std::clamp<float>(mesh->mColors[0][i].g * 255.0f, 0.0f, 255.0f),
								(uint8_t)std::clamp<float>(mesh->mColors[0][i].b * 255.0f, 0.0f, 255.0f),
								(uint8_t)std::clamp<float>(mesh->mColors[0][i].a * 255.0f, 0.0f, 255.0f)
							};
						}
						if (mesh->HasNormals())
						{
							vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
						}
						if (mesh->HasTangentsAndBitangents()) {
							vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
							vertex.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
						}
						if (mesh->HasTextureCoords(0)) {
							vertex.texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
						}
					}
				}

				{
					auto pindices = layer.IndexResource->InitializeFromType<uint32_t>(mesh->mNumFaces * 3);
					for (size_t i = 0; i < mesh->mNumFaces; ++i)
					{
						SE_ASSERT(mesh->mFaces[i].mNumIndices == 3);

						pindices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
						pindices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
						pindices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
					}
				}
			}
		}

		return true;
	}
}