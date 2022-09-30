// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPJsonEnvironmentImporter.h"
#include "SPPJsonUtils.h"
#include "SPPGraphics.h"

#include "ThreadPool.h"

namespace SPP
{
	const static std::vector< std::string > ParamsToLookFor =
	{
		"alpha",
		"diffuse",
		//"metallic",
		//"normal",
		//"roughness",
		//"specular"
	};

	Matrix3x3 lookAt(const Vector3 &Dir, const Vector3& UpDir) 
	{
		Vector3 f = Dir.normalized();
		Vector3 u = UpDir.normalized();
		Vector3 s = f.cross(u).normalized();
		u = f.cross(s);
		Matrix3x3 mat = Matrix3x3::Zero();
		mat(0, 0) = s.x();
		mat(0, 1) = s.y();
		mat(0, 2) = s.z();
		mat(1, 0) = u.x();
		mat(1, 1) = u.y();
		mat(1, 2) = u.z();
		mat(2, 0) = f.x();
		mat(2, 1) = f.y();
		mat(2, 2) = f.z();			
		return mat;
	}

	

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
		Json::Value lightsV = JsonScene.get("lights", Json::Value::nullSingleton());

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
				Json::Value parametersV = currentMaterial.get("parameters", Json::Value::nullSingleton());

				std::string MatName = jName.asCString();
				auto meshMat = AllocateObject<OMaterial>(MatName + ".mat", FileScene);

				bool bFoundLightMap = false;
				if (!parametersV.isNull())
				{
					for (auto& curParam : ParamsToLookFor)
					{
						Json::Value curParamV = parametersV.get(curParam.c_str(), Json::Value::nullSingleton());
						if (curParamV.isNull()) continue;

						Json::Value jName = curParamV.get("name", Json::Value::nullSingleton());
						Json::Value jUVMap = curParamV.get("uvmap", Json::Value::nullSingleton());
						Json::Value jValue = curParamV.get("value", Json::Value::nullSingleton());

						if (!jValue.isNull())
						{
							std::vector<std::string> valueA = std::str_split(std::string(jValue.asCString()), ' ');

							if (valueA.size() == 1)
							{
								meshMat->SetParameter(curParam, (float)std::atof(valueA[0].c_str()));
							}
							else if (valueA.size() == 2)
							{
								meshMat->SetParameter(curParam,
									Vector2(
										(float)std::atof(valueA[0].c_str()),
										(float)std::atof(valueA[1].c_str())));
							}
							else if (valueA.size() == 3)
							{
								meshMat->SetParameter(curParam, 
									Vector3(
										(float)std::atof(valueA[0].c_str()),
										(float)std::atof(valueA[1].c_str()),
										(float)std::atof(valueA[2].c_str())));
							}
							else if (valueA.size() == 4)
							{
								meshMat->SetParameter(curParam, 
									Vector4(
										(float)std::atof(valueA[0].c_str()),
										(float)std::atof(valueA[1].c_str()),
										(float)std::atof(valueA[2].c_str()),
										(float)std::atof(valueA[3].c_str())));
							}
						}
						else if (!jName.isNull())
						{
							std::string CurTextureName = jName.asCString();
							std::string CurUVMap = jUVMap.asCString();
							auto foundTexture = TextureMap.find(CurTextureName);

							// diffuse and named uv
							bool IsLightMap = (Iter == 0) && (CurUVMap == "UVMap_Lightmap");
							bFoundLightMap |= IsLightMap;

							if (foundTexture == TextureMap.end())
							{
								auto curTexture = AllocateObject<OTexture>(CurTextureName, FileScene);

								std::string TexturePath = (ParentPath + "/") + CurTextureName;
								curTexture->LoadFromDisk(TexturePath.c_str());
								TextureMap[CurTextureName] = curTexture;
								meshMat->SetParameter(curParam, curTexture);
							}
							else
							{
								meshMat->SetParameter(curParam, foundTexture->second);
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

		if (!lightsV.isNull() && lightsV.isArray())
		{
			for (int32_t Iter = 0; Iter < lightsV.size(); Iter++)
			{
				auto currentLight = lightsV[Iter];

				Json::Value jName = currentLight.get("name", Json::Value::nullSingleton());
				Json::Value jType = currentLight.get("type", Json::Value::nullSingleton());
				Json::Value jTransform = currentLight.get("transform", Json::Value::nullSingleton());

				Json::Value jColor = currentLight.get("color", Json::Value::nullSingleton());
				Json::Value jEnergy = currentLight.get("energy", Json::Value::nullSingleton());

				if (!jName.isNull() &&
					!jType.isNull() &&
					!jColor.isNull() &&
					!jEnergy.isNull() &&
					!jTransform.isNull())
				{
					auto newLight = AllocateObject<OSun>(jName.asCString(), FileScene);

					Json::Value jLocation = jTransform.get("location", Json::Value::nullSingleton());
					Json::Value jRot = jTransform.get("rotation", Json::Value::nullSingleton());
					Json::Value jScale = jTransform.get("scale", Json::Value::nullSingleton());

					auto& curRot = newLight->GetRotation();
					auto& curPos = newLight->GetPosition();
					auto& curScale = newLight->GetScale();

					Vector3 LightValue;
					for (int32_t Iter = 0; Iter < 3; Iter++)
					{
						LightValue[Iter] = jColor[Iter].asFloat();
					}
					LightValue *= jEnergy.asFloat();
					newLight->GetIrradiance() = LightValue;

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{
						curPos[Iter] = jLocation[Iter].asDouble();
					}

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{
						curRot[Iter] = jRot[Iter].asFloat();
					}

					//why the negative
					curRot[2] = -curRot[2];


					const float RadToDegree = 57.295755f;
					Eigen::Quaternion<float> q = EulerAnglesToQuaternion(curRot);
					Matrix3x3 rotationMatrix = q.matrix();
					Vector3 tempY = rotationMatrix.block<1, 3>(1, 0);
					rotationMatrix.block<1, 3>(1, 0) = rotationMatrix.block<1, 3>(2, 0);
					rotationMatrix.block<1, 3>(2, 0) = -tempY;					
					curRot = ToEulerAngles(rotationMatrix) * RadToDegree;
					

					for (int32_t Iter = 0; Iter < 3; Iter++)
					{
						curScale[Iter] = jScale[Iter].asFloat();
					}

					FileScene->AddChild(newLight);
				}
			}
		}

		return FileScene;
	}
}
