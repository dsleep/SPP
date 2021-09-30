#include "Common.hlsl"

struct VertexShaderInput
{
	float3 position		: POSITION;
	float3 color		: COLOR0;
};

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float3 color			: COLOR0;
};

// Vertex shader
[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float3 worldPosition = mul(float4(vin.position, 1.0), LocalToWorldTranslated).xyz;
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
	vout.color = vin.color;

	return vout;
}

struct PixelShaderOutput
{
	float4 color0	: SV_Target0;
	float4 color1	: SV_Target1;
};

// Pixel shader
[RootSignature(MESH_SIG)]
PixelShaderOutput main_ps(PixelShaderInput pin) 
{	
	PixelShaderOutput output;
	output.color0 = float4(0,0,0,0);
	output.color1 = float4(0,0,0,0);
	return output;
}
