// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSDFO.h"

namespace SPP
{
	enum class EMagicaCSG_ShapeOP
	{
		Union,
		Subtract,
		Intersect,
		Replace,
	};

	enum class EMagicaCSG_ShapeType
	{
		Sphere,
		Cube,
		Cylinder,
	};

	struct MagicaCSG_Shape
	{
		EMagicaCSG_ShapeType Type = EMagicaCSG_ShapeType::Sphere;
		EMagicaCSG_ShapeOP Mode = EMagicaCSG_ShapeOP::Union;
		
		Vector3 Translation;
		Vector3 Scale;
		Matrix3x3 Rotation;
		float Blend;
		float MX;
	};

	struct MagicaCSG_Layer
	{
		std::string Name;
		std::vector< MagicaCSG_Shape > Shapes;
	};
	
	SPP_SDF_API std::vector<MagicaCSG_Layer> LoadMagicaCSGFile(const char* FilePath);
}

