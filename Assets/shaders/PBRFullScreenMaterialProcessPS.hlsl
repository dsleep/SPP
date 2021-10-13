// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "Common.hlsl"

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

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 texcoord			: TEXCOORD0;
};

StructuredBuffer<PBRMaterial> PBRMaterials : register(t0, space0);
Texture2D GBufferTextures[] : register(t0, space1);
Texture2D GlobalTextures[] : register(t0, space2);

SamplerState dSampler : register(s0);
SamplerState cSampler : register(s0);

[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin):SV_TARGET
{	
	float4 SceneA = GBufferTextures[0].Sample(cSampler, pin.texcoord);
	float4 SceneB = GBufferTextures[1].Sample(cSampler, pin.texcoord);
	
	return float4(outRender.xyz, 1.0f);
}