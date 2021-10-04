// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "Common.hlsl"

TextureCube cubeMapTexture : register(t0, space0);
SamplerState cSampler : register(s0);

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 texcoord			: TEXCOORD0;
	float3 rO				: TEXCOORD1;
	float3 rD				: TEXCOORD2;
};

[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin, out float depth : SV_Depth) :SV_TARGET
{
	float3 ColorSample = cubeMapTexture.Sample(cSampler, pin.rD).rgb;  
	return float4( ColorSample, 1 );
}
