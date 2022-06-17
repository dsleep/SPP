#include "Common.hlsl"

struct VertexShaderInput
{
	[[vk::location(0)]] float3 position		: POSITION;
	[[vk::location(1)]] float3 normal		: NORMAL;
	[[vk::location(2)]] float2 texcoord 	: TEXCOORD;
	[[vk::location(3)]] float3 color		: COLOR0;
};

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float2 UV				: TEXCOORD0;
	float3 Normal			: NORMAL0;
	float3 color			: COLOR0;
};

// Vertex shader
[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout = (PixelShaderInput)0;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float3 worldPosition = mul(float4(vin.position, 1.0), LocalToWorldTranslated).xyz;
	
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
	vout.color = vin.color;
	vout.UV = vin.texcoord;
	vout.Normal = mul(float4(normalize(input.normal), 1.0), ViewConstants.ViewMatrix).xyz
	
	return vout;
}

struct FSOutput
{
	float4 UVandDerivatives : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	uint4 MaterialID : SV_TARGET2;
};

// Pixel shader
[RootSignature(MESH_SIG)]
FSOutput main_ps(PixelShaderInput pin) 
{	
	FSOutput output = (FSOutput)0;

	UVandDerivatives.xy = pin.UV;
	UVandDerivatives.zw = float2(ddx(pin.UV), ddy(pin.UV));

	UVandDerivatives.Normal = pin.Normal.xyz;
	UVandDerivatives.MaterialID.xyzw = 0;

	// Final fragment color.
	return output;
}
