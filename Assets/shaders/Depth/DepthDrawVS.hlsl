#include "Common.hlsl"

struct VertexShaderInput
{
	[[vk::location(0)]] float3 position	: POSITION;
};

struct PixelShaderInput
{
	float4 pixelPosition : SV_POSITION;	
};

// Vertex shader
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float3 worldPosition = mul(float4(vin.position, 1.0), LocalToWorldTranslated).xyz;
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);

	return vout;
}
