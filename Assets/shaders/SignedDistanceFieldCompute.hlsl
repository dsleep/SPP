// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "Common.hlsl"
#include "SDFShapes.hlsl"

[[vk::binding(4, 0)]]
RWTexture2D<float4> resultImage : register(u1);

[numthreads(16, 16, 1)]
void main_cs(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	float2 pixelPosition = float2( GlobalInvocationID.x / (float)ViewConstants.FrameExtents.x, GlobalInvocationID.y / (float)ViewConstants.FrameExtents.y );
	pixelPosition = (pixelPosition - 0.5f) * 0.5f;
	
	float4 raystart = mul(float4(pixelPosition, 0, 1.0), ViewConstants.InvViewProjectionMatrix);
	raystart /= raystart.w;
	// using a smallish z for floating error
	float4 rayStop = mul(float4(pixelPosition, 0.01, 1.0), ViewConstants.InvViewProjectionMatrix);
	rayStop /= rayStop.w;
	
	float3 rayOrigin = raystart.xyz + float3(ViewConstants.ViewPosition);
	float3 rayDirection = normalize(rayStop.xyz - raystart.xyz);
	
	float4 outRender = renderSDF(rayOrigin, rayDirection);
	//float4 localWorldPos = float4(rayOrigin + rayDirection * outRender.w - float3(ViewConstants.ViewPosition), 1.0f);
	
	if(outRender.w > -100)
	{
		resultImage[int2(GlobalInvocationID.xy)] = float4(outRender.rgb,1.0f);
	}
	
	//if(length(pixelPosition) < 0.1)
	//resultImage[int2(GlobalInvocationID.xy)] =float4(0,0,0,1.0f);
}