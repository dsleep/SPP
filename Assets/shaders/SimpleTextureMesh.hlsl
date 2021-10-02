#include "Common.hlsl"

struct VertexShaderInput
{
	float3 position  : POSITION;
	float2 uv		  : TEXCOORD;
};

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
};

Texture2D diffuseTexture : register(t1);
SamplerState dSampler : register(s0);


// Vertex shader
[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float3 worldPosition = mul(float4(vin.position, 1.0), LocalToWorldTranslated).xyz;
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
		
	vout.uv = vin.uv;

	return vout;
}

// Pixel shader
[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin) : SV_Target
{
	float4 diffuse = diffuseTexture.Sample(dSampler, pin.uv).rgba;

	clip(diffuse.a - 0.5);

	// Final fragment color.
	return float4(diffuse.xyz, 1.0);
}
