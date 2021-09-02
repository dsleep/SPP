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
PixelShaderInput main_vs(uint vI : SV_VERTEXID)
{
    PixelShaderInput results;
	
	results.texcoord = float2(vI&1,vI>>1); //you can use these for texture coordinates later		
	results.pixelPosition = float4((results.texcoord.x-0.5f)*2, -(results.texcoord.y-0.5f)*2, 0, 1);
	
	float4 raystart = mul(float4(results.pixelPosition.xy, 0, 1.0), ViewConstants.InvViewProjectionMatrix);
	raystart /= raystart.w;
	// using a smallish z for floating error
	float4 rayStop = mul(float4(results.pixelPosition.xy, 0.01, 1.0), ViewConstants.InvViewProjectionMatrix);
	rayStop /= rayStop.w;
	
	results.rO = raystart.xyz + float3(ViewConstants.ViewPosition);
	results.rD = normalize(rayStop.xyz - raystart.xyz);
	
    return results;
}

[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin):SV_TARGET
{
    return renderSDF(pin.rO, pin.rD);
}