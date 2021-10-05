// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

struct PBRMaterial
{	
	uint albedoTextureID;
	uint normalTextureID;
	uint metalnessTextureID;
	uint roughnessTextureID;
	uint specularTextureID;
	uint irradianceTextureID;
	uint specularBRDF_LUTTextureID;
	uint maskedTextureID;
};

StructuredBuffer<PBRMaterial> PBRMaterials : register(t0, space1);