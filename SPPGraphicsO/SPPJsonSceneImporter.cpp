// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPJsonSceneImporter.h"
#include "SPPJsonUtils.h"

namespace SPP
{
	bool LoadJsonScene(const char* FilePath, ORenderableScene* InRenderableScene)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return false;
		}

		Json::Value meshes = JsonScene.get("meshes", Json::Value::nullSingleton());

		if (!meshes.isNull() && meshes.isArray())
		{
			for (int32_t Iter = 0; Iter < meshes.size(); Iter++)
			{
				auto currentMesh = meshes[Iter];

				Json::Value jName = JsonScene.get("name", Json::Value::nullSingleton());
				Json::Value jRelFilePath = JsonScene.get("relFilePath", Json::Value::nullSingleton());
				Json::Value jTransform = JsonScene.get("transform", Json::Value::nullSingleton());
				
				if (!jName.isNull() && 
					!jRelFilePath.isNull() &&
					!jTransform.isNull())
				{
					auto meshMat = AllocateObject<OMaterial>("simplerMaterial", nullptr);
					//meshMat->SetMaterial(meshMaterial);

					auto meshtest = std::make_shared< Mesh>();
					meshtest->LoadMesh(*AssetPath(jRelFilePath.asCString()));

					auto loadedMesh = AllocateObject<OMesh>("simpleMesh", nullptr);
					loadedMesh->SetMesh(meshtest);

					auto meshElement = AllocateObject<OMeshElement>("simpleMeshElement", nullptr);
					meshElement->SetMesh(loadedMesh);
					meshElement->SetMaterial(meshMat);
				}
			}
		}

		return true;
	}
}
