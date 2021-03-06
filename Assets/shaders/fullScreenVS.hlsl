// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "Common.hlsl"

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 texcoord			: TEXCOORD0;
};

[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(uint vI : SV_VERTEXID)
{
    PixelShaderInput results;
	
	results.texcoord = float2(vI&1,vI>>1); //you can use these for texture coordinates later		
	results.pixelPosition = float4((results.texcoord.x-0.5f)*2, -(results.texcoord.y-0.5f)*2, 0, 1);
	
	return results;
}
