#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#extension GL_GOOGLE_include_directive: require

layout(row_major) uniform;
layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"

struct VoxelPushInfo
{
	vec3 RayStart;
	vec3 RayDirection;
	ivec3 Extents;	
	int VoxelStartingDepth;	
};

layout(push_constant) uniform block
{
	VoxelPushInfo pcs;
};

const int MAX_VOXEL_LEVELS = 12;

layout(set = 1, binding = 1) readonly buffer _Voxels
{
	uint8_t voxels[];
} Voxels[MAX_VOXEL_LEVELS];

layout (set = 2, binding = 0, rgba8) uniform image2D DiffuseImage;
layout (set = 2, binding = 1, rgba8) uniform image2D SMREImage;
layout (set = 2, binding = 2, rgba8) uniform image2D NormalImage;
layout (set = 2, binding = 3, r32f) uniform image2D DepthImage;

layout (set = 2, binding = 4) uniform sampler2D samplerDepth;


layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
	uint di = gl_GlobalInvocationID.x;
		
	if(Voxels[0].voxels[di] != uint8_t(0))
	{
		uint test = 0;
		test++;
	}
}
