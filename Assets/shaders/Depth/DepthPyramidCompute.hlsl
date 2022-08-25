
[[vk::binding(0, 1)]]
Texture2D depthTexture : register(t0, space0);
[[vk::binding(1, 1)]]
SamplerState depthSampler : register(s0);

[[vk::binding(2, 1)]]
RWTexture2D<float> oDepthReduced : register(u1);

struct DepthReduceData
{
	float2 imageSize;
};

[[vk::push_constant]]
DepthReduceData pcs;

[numthreads(32, 32, 1)]
void main_cs(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
	float depth = depthTexture.SampleLevel(depthSampler, (float2(GlobalInvocationID.xy) + float2(0.5f,0.5f)) / pcs.imageSize, 0 ).r;
	oDepthReduced[int2(GlobalInvocationID.xy)].x = depth;
}