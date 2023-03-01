#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
layout(std430) buffer;

#include "Common.glsl"

layout (set = 2, binding = 0) uniform samplerCube samplerSky;

layout (location = 0) in vec4 inPixelPosition;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outputColor;

// pixel shader shader
void main()
{
	vec3 cameraRay = normalize(Multiply(vec4(inPixelPosition.xy, 1, 1.0), ViewConstants.InvViewProjectionMatrix).xyz);		
	outputColor = vec4(texture(samplerSky, normalize(cameraRay)).rgb,1.0f);
}

