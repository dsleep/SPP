#include "Common.hlsl"

struct VertexShaderInput
{
	[[vk::location(0)]] float3 position		: POSITION;
	[[vk::location(1)]] float3 color		: COLOR0;
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
	
	float3 worldPosition = vin.position - float3(ViewConstants.ViewPosition);
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
	vout.color = vin.color;

	return vout;
}

// Pixel shader
[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin) : SV_Target
{	
	// Final fragment color.
	return float4(pin.color, 1.0);
}
