#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
layout(std430) buffer;

#include "Common.glsl"

// out params
layout (location = 0) out vec4 outPixelPosition;
layout (location = 1) out vec2 outUV;

out gl_PerVertex
{
	vec4 gl_Position;
};

// Vertex shader
void main()
{	
	outUV = vec2(gl_VertexIndex & 1,gl_VertexIndex >> 1); //you can use these for texture coordinates later		
	outPixelPosition = vec4((outUV.x-0.5f)*2, (outUV.y-0.5f)*2, 0, 1);
	gl_Position = outPixelPosition;
}
