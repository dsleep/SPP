// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "Common.hlsl"
#include "SDFShapes.hlsl"

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 texcoord			: TEXCOORD0;
	float3 rO				: TEXCOORD1;
	float3 rD				: TEXCOORD2;
};

[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin, out float depth : SV_Depth):SV_TARGET
{	
    float4 outRender = renderSDF(pin.rO, pin.rD);
	float4 localWorldPos = float4(pin.rO + pin.rD * outRender.w - float3(ViewConstants.ViewPosition), 1.0f);
	float4 devicePos = mul(localWorldPos, ViewConstants.ViewProjectionMatrix);
	depth = (devicePos.z / devicePos.w) ;
	
	return float4(outRender.xyz, 1.0f);
}