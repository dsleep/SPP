#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
//layout(row_major) uniform;
//layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"

layout(set = 0, binding = 1) readonly uniform _DrawConstants
{
	//altered viewposition translated
	mat4 LocalToWorldScaleRotation;
	dvec3 Translation;
} DrawConstants;

// Vertex attributes
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inTangent;

// out params
layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outTangent;

out gl_PerVertex
{
	vec4 gl_Position;
};

// Vertex shader
void main()
{
	mat4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	mat4 localToScreen =  Multiply( LocalToWorldTranslated, ViewConstants.ViewProjectionMatrix );
		
	outUV = inUV;
	outNormal = normalize( Multiply( inNormal, mat3(localToScreen) ) );
	outTangent = normalize( Multiply( inTangent, mat3(localToScreen) ) );

	gl_Position = Multiply( vec4(inPos, 1.0), localToScreen );
}

