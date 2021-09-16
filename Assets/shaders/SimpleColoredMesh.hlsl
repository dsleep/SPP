#include "Common.hlsl"

struct VertexShaderInput
{
	float3 position		: POSITION;
	uint color			: COLOR0;
};

struct PixelShaderInput
{
	float4 pixelPosition	: SV_POSITION;
	float3 color			: COLOR0;
};

struct DrawParams
{
	uint Selected;
};

ConstantBuffer<DrawParams> DrawParams : register(b3);


// Vertex shader
[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float3 worldPosition = mul(float4(vin.position, 1.0), LocalToWorldTranslated).xyz;
	vout.pixelPosition = mul(float4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
		
	uint rawColor = vin.color;
	float alpha = float((rawColor & 0xFF000000u)>>24)/255.0f;
	float blue 	= float((rawColor & 0x00FF0000u)>>16)/255.0f;
	float green = float((rawColor & 0x0000FF00u)>>8)/255.0f;
	float red 	= float((rawColor & 0x000000FFu))/255.0f;

	vout.color = float3(red, green, blue);

	return vout;
}

// Pixel shader
[RootSignature(MESH_SIG)]
float4 main_ps(PixelShaderInput pin) : SV_Target
{
	float3 outColor = pin.color;

	if (DrawParams.Selected == 0)
	{
		pin.color *= 0.35;
	}

	// Final fragment color.
	return float4(pin.color, 1.0);
}
