#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

layout(row_major) uniform;
layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;

<<UNIFORM_BLOCK>>

//layout (set = 2, binding = 0) uniform sampler2D samplerDiffuse;
//layout (set = 2, binding = 1) uniform sampler2D samplerNormal;

//layout(set = 2, binding = 0) readonly uniform _Params
//{
//	float4 Diffuse; 
//	float3 Normal;
//} Params;

vec3 GetDiffuse()
{
<<DIFFUSE_BLOCK>>
}

vec GetOpacity()
{
<<OPACITY_BLOCK>>
}

vec3 GetNormal()
{
<<NORMAL_BLOCK>>
}

float GetSpecular()
{
<<SPECULAR_BLOCK>>
}

float GetMetallic()
{
<<METALLIC_BLOCK>>
}

float GetRoughness()
{
<<ROUGHNESS_BLOCK>>
}

float GetEmissive()
{
<<EMISSIVE_BLOCK>>
}

//
layout (location = 0) out vec4 outDiffuse;
// specular, metallic, roughness, emissive
layout (location = 0) out vec4 outSMRE;
// 
layout (location = 1) out vec4 outNormal;

void main()
{	
	outDiffuse = vec4( GetDiffuse(), GetOpacity() );
	outSMRE = vec4( GetSpecular(), GetMetallic(), GetRoughness(), GetEmissive() );
	outNormal = vec4(GetNormal(), 0);
}
