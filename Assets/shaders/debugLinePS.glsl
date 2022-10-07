#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
layout(std430) buffer;

#include "Common.glsl"

layout (location = 0) in vec3 inColor;
layout (location = 0) out vec4 outputColor;

// pixel shader shader
void main()
{
	outputColor = vec4(inColor,1.0f);
}

