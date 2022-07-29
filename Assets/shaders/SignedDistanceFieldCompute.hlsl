// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "Common.hlsl"
#include "SDFShapes.hlsl"

[[vk::binding(4, 0)]]
RWTexture2D<float4> colorImage : register(u1);
[[vk::binding(5, 0)]]
RWTexture2D<float> depthImage : register(u2);

[numthreads(32, 32, 1)]
void main_cs(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	float2 pixelPosition = float2( GlobalInvocationID.x / (float)ViewConstants.FrameExtents.x, GlobalInvocationID.y / (float)ViewConstants.FrameExtents.y );
	pixelPosition = (pixelPosition - 0.5f) * 2.0f;
	
	float4 raystart = mul(float4(pixelPosition, 0, 1.0), ViewConstants.InvViewProjectionMatrix);
	raystart /= raystart.w;
	// using a smallish z for floating error
	float4 rayStop = mul(float4(pixelPosition, 0.01, 1.0), ViewConstants.InvViewProjectionMatrix);
	rayStop /= rayStop.w;
	
	float zDepthNDC = depthImage[int2(GlobalInvocationID.xy)].x;
	//// We are only interested in the depth here
	//// Unproject the vector into (homogenous) view-space vector
	//float4 viewCoords = mul(float4(pixelPosition, zDepthNDC, 1.0f), ViewConstants.InvProjectionMatrix);
	//// Divide by w, which results in actual view-space z value
	//float zDepth = viewCoords.z / viewCoords.w;

	float3 rayOrigin = raystart.xyz;
	float3 rayDirection = normalize(rayStop.xyz - raystart.xyz);
	
	float4 outRender = renderSDF(rayOrigin, rayDirection);
	float4 localWorldPos = float4(rayOrigin + rayDirection * outRender.w, 1.0f);
	
	float4 localRay = mul(localWorldPos, ViewConstants.ViewProjectionMatrix);
	localRay /= localRay.w;	
	
	if(outRender.w > -100 && localRay.z < zDepthNDC)
	{
		colorImage[int2(GlobalInvocationID.xy)] = float4(outRender.rgb,1.0f);
		depthImage[int2(GlobalInvocationID.xy)].x = localRay.z;
	}
	
	//if(length(pixelPosition) < 0.1)
	//colorImage[int2(GlobalInvocationID.xy)] =float4(0,0,0,1.0f);
}