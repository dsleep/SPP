// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPLoadMagicaCSG.h"
#include "SPPJsonUtils.h"
#include "SPPGraphics.h"

#include "SPPString.h"
#include "ThreadPool.h"
#include "SPPLogging.h"

namespace SPP
{
	LogEntry LOG_CSGLOADER("CSGLOADER");

	std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) 
	{
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
		}
		return str;
	}

	SPP_SDF_API std::vector<MagicaCSG_Layer> LoadMagicaCSGFile(const char* FilePath)
	{
		std::vector<MagicaCSG_Layer> oLayers;

		Json::Value JsonScene;
		std::string FileString;
		if (LoadFileToString(FilePath, FileString))
		{
			FileString = std::string("{\n") + FileString + std::string("\n}");
			FileString = ReplaceAll(FileString, "\"\n", "\",\n");
			FileString = ReplaceAll(FileString, "}\n", "},\n");
			FileString = ReplaceAll(FileString, "]\n", "],\n");
			FileString = ReplaceAll(FileString, "%", "");
			FileString = ReplaceAll(FileString, "\":", "\" :");

			if (StringToJson(FileString, JsonScene) == false)
			{
				return oLayers;
			}
		}
		else
		{
			return oLayers;
		}

		Json::Value csgElements = JsonScene.get("csg", Json::Value::nullSingleton());
		Json::Value layerNames = JsonScene.get("object", Json::Value::nullSingleton());

		if (!csgElements.isNull() && csgElements.isArray())
		{
			auto layerCount = csgElements.size();

			if (layerCount)
			{
				for (int32_t LayerIter = 0; LayerIter < layerCount; LayerIter++)
				{
					MagicaCSG_Layer newLayer;

					auto curLayer = csgElements[LayerIter];
					auto elementCount = curLayer.size();

					for (int32_t EleIter = 0; EleIter < elementCount; EleIter++)
					{
						MagicaCSG_Shape newShape;

						auto curElement = curLayer[EleIter];

						Json::Value eleTypeV = curElement.get("type", Json::Value::nullSingleton());
						Json::Value eleModeV = curElement.get("mode", Json::Value::nullSingleton());
						Json::Value rgbV = curElement.get("rgb", Json::Value::nullSingleton());
						Json::Value blendV = curElement.get("blend", Json::Value::nullSingleton());

						Json::Value tV = curElement.get("t", Json::Value::nullSingleton());
						Json::Value sV = curElement.get("s", Json::Value::nullSingleton());
						Json::Value rV = curElement.get("r", Json::Value::nullSingleton());

						if (!eleTypeV.isNull() && !rV.isNull() && !tV.isNull() && !sV.isNull())
						{
							std::vector<std::string> tA = std::str_split(std::string(tV.asCString()), ' ');
							std::vector<std::string> sA = std::str_split(std::string(sV.asCString()), ' ');
							std::vector<std::string> rA = std::str_split(std::string(rV.asCString()), ' ');

							newShape.Translation = Vector3(std::atof(tA[0].c_str()), std::atof(tA[1].c_str()), std::atof(tA[2].c_str()));
							newShape.Scale = Vector3(std::atof(sA[0].c_str()), std::atof(sA[1].c_str()), std::atof(sA[2].c_str()));
							newShape.Rotation <<
								std::atof(rA[0].c_str()), std::atof(rA[1].c_str()), std::atof(rA[2].c_str()),
								std::atof(rA[3].c_str()), std::atof(rA[4].c_str()), std::atof(rA[5].c_str()),
								std::atof(rA[6].c_str()), std::atof(rA[7].c_str()), std::atof(rA[8].c_str());

							if (!rgbV.isNull())
							{
								std::vector<std::string> shapeA = std::str_split(std::string(rgbV.asCString()), ' ');
								newShape.Color = Color3(std::atoi(shapeA[0].c_str()), std::atoi(shapeA[1].c_str()), std::atoi(shapeA[2].c_str()));
							}

							if (!blendV.isNull())
							{								
								newShape.Blend = std::atof(blendV.asCString());
							}

							std::string eleTypeS = eleTypeV.asCString();
							if (eleTypeS == "sphere")
							{
								newShape.Type = EMagicaCSG_ShapeType::Sphere;
							}
							else if (eleTypeS == "cylinder")
							{
								newShape.Type = EMagicaCSG_ShapeType::Cylinder;
							}
							else if (eleTypeS == "cube")
							{
								newShape.Type = EMagicaCSG_ShapeType::Cube;
							}
							else
							{
								SPP_LOG(LOG_CSGLOADER, LOG_WARNING, "Unknown type: %s", eleTypeS.c_str());
							}

							//union is default
							if (!eleModeV.isNull())
							{
								std::string eleModeS = eleModeV.asCString();
								if (eleModeS == "sub")
								{
									newShape.Mode = EMagicaCSG_ShapeOP::Subtract;
								}
								else if (eleModeS == "intersect")
								{
									newShape.Mode = EMagicaCSG_ShapeOP::Intersect;
								}
								else if (eleModeS == "replace")
								{
									newShape.Mode = EMagicaCSG_ShapeOP::Replace;
								}
								else
								{
									SPP_LOG(LOG_CSGLOADER, LOG_WARNING, "Unknown mode: %s", eleTypeS.c_str());
								}
							}

							newLayer.Shapes.push_back(newShape);
						}
					}

					oLayers.push_back(newLayer);
				}
			}
		}

		if (!layerNames.isNull() && layerNames.isArray())
		{
			auto layerCount = layerNames.size();

			if (layerCount == oLayers.size())
			{
				for (int32_t LayerIter = 0; LayerIter < layerCount; LayerIter++)
				{
					auto curLayerObj = layerNames[LayerIter];
					Json::Value nameV = curLayerObj.get("name", Json::Value::nullSingleton());
					if (!nameV.isNull())
					{
						oLayers[LayerIter].Name = nameV.asCString();
					}
				}
			}
		}

		return oLayers;
	}
}
