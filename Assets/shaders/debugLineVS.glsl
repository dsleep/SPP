#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
layout(std430) buffer;

#include "Common.glsl"

// Vertex attributes
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

// out params
layout (location = 0) out vec3 outColor;

out gl_PerVertex
{
	vec4 gl_Position;
};

// Vertex shader
void main()
{
	vec3 worldPosition = inPos - vec3(ViewConstants.ViewPosition);	
	outColor = inColor;
	gl_Position = Multiply( vec4(worldPosition, 1.0), ViewConstants.ViewProjectionMatrix);
}

