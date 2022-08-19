// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPJsonEnvironmentImporter.h"
#include "SPPJsonUtils.h"
#include "SPPGraphics.h"

#include "ThreadPool.h"

namespace SPP
{
	VgEnvironment* LoadJsonGameScene(const char* FilePath)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return nullptr;
		}

		std::string ParentPath = stdfs::path(FilePath).parent_path().generic_string();
		std::string SimpleSceneName = stdfs::path(FilePath).stem().generic_string();
		auto FileScene = AllocateObject<VgEnvironment>(SimpleSceneName, nullptr);

		Json::Value materials = JsonScene.get("materials", Json::Value::nullSingleton());
		Json::Value meshes = JsonScene.get("meshes", Json::Value::nullSingleton());

		std::map<std::string, OMaterial*> MaterialMap;
		std::map<std::string, OTexture*> TextureMap;
		std::map<std::string, OMesh*> MeshMap;

		auto vsShader = AllocateObject<OShader>("MeshVS", FileScene);
		auto psShader = AllocateObject<OShader>("MeshPS", FileScene);
		auto vsLMShader = AllocateObject<OShader>("MeshLMVS", FileScene);
		auto psLMShader = AllocateObject<OShader>("MeshLMPS", FileScene);

		vsShader->Initialize(EShaderType::Vertex, "shaders/SimpleTextureMesh.hlsl", "main_vs");
		psShader->Initialize(EShaderType::Pixel, "shaders/SimpleTextureMesh.hlsl", "main_ps");

		vsLMShader->Initialize(EShaderType::Vertex, "shaders/SimpleTextureLightMapMesh.hlsl", "main_vs");
		psLMShader->Initialize(EShaderType::Pixel, "shaders/SimpleTextureLightMapMesh.hlsl", "main_ps");

		if (!materials.isNull() && materials.isArray())
		{
			for (int32_t Iter = 0; Iter < materials.size(); Iter++)
			{
				auto currentMaterial = materials[Iter];

				Json::Value jName = currentMaterial.get("name", Json::Value::nullSingleton());
				Json::Value textures = currentMaterial.get("textures", Json::Value::nullSingleton());

				std::string MatName = jName.asCString();
				auto meshMat = AllocateObject<OMaterial>(MatName + ".mat", FileScene);

				bool bFoundLightMap = false;
				if (!textures.isNull())
				{
					for (int32_t TexIter = 0; TexIter < (int32_t)TexturePurpose::MAX; TexIter++)
					{
						std::string TextureUse = ToString((TexturePurpose)TexIter);
						inlineToLower(TextureUse);

						Json::Value textureValue = textures.get(TextureUse.c_str(), Json::Value::nullSingleton());

						if (!textureValue.isNull() && textureValue.isArray())
						{
							for (int32_t Iter = 0; Iter < textureValue.size(); Iter++)
							{
								auto currentTexture = textureValue[Iter];

								Json::Value jName = currentTexture.get("name", Json::Value::nullSingleton());
								Json::Value jUVMap = currentTexture.get("uvmap", Json::Value::nullSingleton());

								std::string CurTextureName = jName.asCString();
								std::string CurUVMap = jUVMap.asCString();
								auto foundTexture = TextureMap.find(CurTextureName);

								// diffuse and named uv
								bool IsLightMap = (Iter == 0) &&(CurUVMap == "UVMap_Lightmap");
								bFoundLightMap |= IsLightMap;
								int32_t TextureIndex = IsLightMap ? 1 : 0;

								if (foundTexture == TextureMap.end())
								{
									auto curTexture = AllocateObject<OTexture>(CurTextureName, FileScene);

									curTexture->LoadFromDisk(((ParentPath + "/") + CurTextureName).c_str());

									TextureMap[CurTextureName] = curTexture;

									meshMat->SetTexture(bFoundLightMap ? TexturePurpose::Lightmap : (TexturePurpose)TexIter, curTexture);
								}
								else
								{

									meshMat->SetTexture(bFoundLightMap ? TexturePurpose::Lightmap : (TexturePurpose)TexIter, foundTexture->second);
								}
							}
						}
					}
				}

				//auto& matshaders = meshMat->GetShaders();				
				MaterialMap[jName.asCString()] = meshMat;

				//if (bFoundLightMap)
				//{
				//	matshaders.push_back(vsLMShader);					
				//	matshaders.push_back(psLMShader);
				//}
				//else
				//{
				//	matshaders.push_back(vsShader);
				//	matshaders.push_back(psShader);
				//}
			}
		}

		if (!meshes.isNull() && meshes.isArray())
		{
			for (int32_t Iter = 0; Iter < meshes.size(); Iter++)
			{
				auto currentMesh = meshes[Iter];

				Json::Value jName = currentMesh.get("name", Json::Value::nullSingleton());
				Json::Value jMaterials = currentMesh.get("materials", Json::Value::nullSingleton());
				Json::Value jMesh = currentMesh.get("mesh", Json::Value::nullSingleton());
				Json::Value jTransform = currentMesh.get("transform", Json::Value::nullSingleton());
				
				if (!jName.isNull() &&
					!jMaterials.isNull() &&
					!jMesh.isNull() &&
					!jTransform.isNull())
				{
					auto meshElement = AllocateObject<VgMeshElement>(jName.asCString(), FileScene);

					Json::Value jLocation = jTransform.get("location", Json::Value::nullSingleton());
					Json::Value jRot = jTransform.get("rotation", Json::Value::nullSingleton());
					Json::Value jScale = jTransform.get("scale", Json::Value::nullSingleton());
					
					auto& curRot = meshElement->GetRotation();
					auto& curPos = meshElement->GetPosition();
					auto& curScale = meshElement->GetScale();

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{						
						curPos[Iter] = jLocation[Iter].asDouble();
					}

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{						
						curRot[Iter] = jRot[Iter].asFloat();
					}
										
					//std::swap(curRot[0], curRot[1]);
					//why the negative
					curRot[2] = -curRot[2];

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{						
						curScale[Iter] = jScale[Iter].asFloat();
					}

					std::string MeshName = jMesh.asCString();
					auto foundMesh = MeshMap.find(MeshName);
					if (foundMesh == MeshMap.end())
					{
						auto meshtest = std::make_shared< Mesh>();

						auto MeshPath = (ParentPath + "/") + MeshName + ".bin";

						meshtest->LoadSimpleBinaryMesh(MeshPath.c_str());

						auto loadedMesh = AllocateObject<OMesh>(MeshName+".SM", FileScene);
						loadedMesh->SetMesh(meshtest);

						MeshMap[MeshName] = loadedMesh;
						meshElement->SetMesh(loadedMesh);
					}
					else
					{
						meshElement->SetMesh(foundMesh->second);
					}

					for (int32_t Iter = 0; Iter < jMaterials.size(); Iter++)
					{
						auto curMat = jMaterials[Iter];
						std::string MatName = curMat.asCString();

						auto foundMat = MaterialMap.find(MatName);

						meshElement->SetMaterial(foundMat->second);
						//hmm 
						break;
					}

					FileScene->AddChild(meshElement);
				}
			}
		}

		return FileScene;
	}
}
