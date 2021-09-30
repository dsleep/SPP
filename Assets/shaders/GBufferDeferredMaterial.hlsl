#include "Common.hlsl"

struct VertexShaderInput
{
	float3 position	: POSITION;
	float2 UV: TEXCOORD0;
    float3 normal: NORMAL;
    float3 tangent: TANGENT;
    float3 binormal: BINORMAL;
};

struct PixelShaderInput
{
	float4 pixelPosition : SV_POSITION;
	float2 UV: TEXCOORD0;
	float3 normal: TEXCOORD1;
    float3 tangent: TEXCOORD2;
    float3 binormal: TEXCOORD3;
};

// Vertex shader
[RootSignature(MESH_SIG)]
PixelShaderInput main_vs(VertexShaderInput vin)
{
	PixelShaderInput vout;

	float4x4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	float4x4 localToScreen = LocalToWorldTranslated * ViewConstants.ViewProjectionMatrix;
	
	vout.pixelPosition = mul( float4(vin.position, 1.0), localToScreen );
	vout.UV = vin.UV;
	vout.normal = normalize( mul(vin.normal, (float3x3)localToScreen) );
	vout.tangent = normalize( mul(vin.tangent, (float3x3)localToScreen) );
	vout.binormal = normalize( mul(vin.binormal, (float3x3)localToScreen) );

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
	output.color0 = float4(pin.UV.xy,ddx(pin.UV.x),ddy(pin.UV.y));
	output.color1 = float4(0,0,0,0);
	return output;
}
