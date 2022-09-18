#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
//layout(row_major) uniform;
//layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"

// out params
layout (location = 0) out vec4 outPixelPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

// Vertex shader
void main()
{	
	vec2 texcoord = vec2(gl_VertexIndex & 1,gl_VertexIndex >> 1); //you can use these for texture coordinates later		
	outPixelPosition = vec4((texcoord.x-0.5f)*2, -(texcoord.y-0.5f)*2, 0, 1);
	gl_Position = outPixelPosition;
}
