#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

layout(row_major) uniform;
layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"

struct DrawCullData
{
	float P00, P11, znear, zfar; // symmetric projection parameters
	float frustum[4]; // data for left/right/top/bottom frustum planes
	float pyramidWidth, pyramidHeight; // depth pyramid size in texels

	uint drawCount;

	int cullingEnabled;
	int occlusionEnabled;
};

struct Renderable
{
	dvec3 center;
	float radius;
};

layout(push_constant) uniform block
{
	DrawCullData cullData;
};

layout(binding = 1) readonly buffer Draws
{
	Renderable draws[];
};

layout(binding = 2) buffer DrawVisibility
{
	uint drawVisibility[];
};

layout(binding = 3) uniform sampler2D depthPyramid;

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool projectSphere(vec3 C, float r, float znear, float P00, float P11, out vec4 aabb)
{
	if (C.z < r + znear)
		return false;

	vec2 cx = -C.xz;
	vec2 vx = vec2(sqrt(dot(cx, cx) - r * r), r);
	vec2 minx = mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
	vec2 maxx = mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

	vec2 cy = -C.yz;
	vec2 vy = vec2(sqrt(dot(cy, cy) - r * r), r);
	vec2 miny = mat2(vy.x, vy.y, -vy.y, vy.x) * cy;
	vec2 maxy = mat2(vy.x, -vy.y, vy.y, vy.x) * cy;

	aabb = vec4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
	aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f); // clip space -> uv space

	return true;
}

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
	uint di = gl_GlobalInvocationID.x;

	if (di >= cullData.drawCount)
		return;

	//vec2 ViewScalar = vec2(cullData.pyramidWidth,  cullData.pyramidHeight) / vec2( ViewConstants.FrameExtents.xy );
	uint renderIdx = di;

	vec3 translatedCenter = vec3(draws[di].center - ViewConstants.ViewPosition);
	vec3 center = (vec4(translatedCenter, 1.0) * ViewConstants.ViewMatrix).xyz;
	float radius = draws[di].radius;

	if(radius <= 0) return;

	bool visible = true;
	// the left/top/right/bottom plane culling utilizes frustum symmetry to cull against two planes at the same time
	visible = visible && center.z * cullData.frustum[1] - abs(center.x) * cullData.frustum[0] > -radius;
	visible = visible && center.z * cullData.frustum[3] - abs(center.y) * cullData.frustum[2] > -radius;
	// the near/far plane culling uses camera space Z directly
	visible = visible && center.z + radius > cullData.znear && center.z - radius < cullData.zfar;

	visible = visible || cullData.cullingEnabled == 0;

	if (false)//visible && cullData.occlusionEnabled == 1)
	{
		vec4 aabb;
		if (projectSphere(center, radius, cullData.znear, cullData.P00, cullData.P11, aabb))
		{
			//aabb = aabb * ViewScalar.xyxy;			
			
			float width = (aabb.z - aabb.x) * cullData.pyramidWidth;
			float height = (aabb.w - aabb.y) * cullData.pyramidHeight;

			float level = floor(log2(max(width, height)));

			// Sampler is set up to do min reduction, so this computes the minimum depth of a 2x2 texel quad
			float depth = textureLod(depthPyramid, (aabb.xy + aabb.zw) * 0.5, level).x;			
			float depthSphere = cullData.znear / (center.z - radius);

			visible = visible && depthSphere > depth;
		}
	}
	
	// 32 bits will fit in a uint
	uint uintIndex = (di >> 5);
	uint bitIndex = 1 << (di - (uintIndex << 5));
	
	if(visible)
	{
		atomicOr( drawVisibility[uintIndex], bitIndex ); 
	}
	else
	{
		atomicAnd( drawVisibility[uintIndex], ~bitIndex ); 
	}
}
