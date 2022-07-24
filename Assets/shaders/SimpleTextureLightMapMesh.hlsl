#include "Common.hlsl"

struct VertexShaderInput
{
	[[vk::location(0)]] float3 position		: POSITION;
	[[vk::location(1)]] float3 normal		: NORMAL;
	[[vk::location(2)]] float2 uv		 	: TEXCOORD0;	
	[[vk::location(3)]] float2 lightmapUV	: TEXCOORD1;
	[[vk::location(4)]] float3 color		: COLOR0;	
};

struct PixelShaderInput
{
	float4 pixelPosition						: SV_POSITION;
	[[vk::location(0)]] float2 uv				: TEXCOORD0;
	[[vk::location(1)]] float2 lightmapUV		: TEXCOORD1;
};

[[vk::binding(0, 1)]]
Texture2D diffuseTexture : register(t0);
[[vk::binding(1, 1)]]
SamplerState dSampler : register(s0);

[[vk::binding(2, 1)]]
Texture2D lightmapTexture : register(t1);
[[vk::binding(3, 1)]]
SamplerState lightmapSampler : register(s1);

// Vertex shader
[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float3 worldPosition = mul(float4(vin.position, 1.0), LocalToWorldTranslated).xyz;
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
		
	vout.uv = vin.uv;
	vout.lightmapUV = vin.lightmapUV;

	return vout;
}

// Pixel shader
[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin) : SV_Target
{
	float4 diffuse = diffuseTexture.Sample(dSampler, pin.uv).rgba;
	float3 lightMap = lightmapTexture.Sample(lightmapSampler, pin.lightmapUV).rgb;

	clip(diffuse.a - 0.5);

	// Final fragment color.
	return float4(diffuse.xyz * lightMap.xyz, 1.0);
}
