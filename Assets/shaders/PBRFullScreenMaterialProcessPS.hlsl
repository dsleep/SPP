// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "Common.hlsl"
#include "PBRCommon.hlsl"

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 texcoord			: TEXCOORD0;
};

TextureCube GBufferTextures[] : register(t0, space0);
SamplerState cSampler : register(s0);

[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin):SV_TARGET
{	
	float4 SceneA = GBufferTextures[0].Sample(cSampler, pin.texcoord);
	float4 SceneB = GBufferTextures[1].Sample(cSampler, pin.texcoord);
	
	return float4(outRender.xyz, 1.0f);
}