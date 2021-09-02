#include "Common.hlsl"

[RootSignature(MESH_SIG)]
float4 main_vs(uint vI : SV_VERTEXID):SV_POSITION
{
    float2 texcoord = float2(vI&1,vI>>1); //you can use these for texture coordinates later
    return float4((texcoord.x-0.5f)*2,-(texcoord.y-0.5f)*2,0,1);
}

[RootSignature(MESH_SIG)]
float4 main_ps(float4 pos : SV_POSITION):SV_TARGET
{
    return float4(1,0,0,1); //the red color
}