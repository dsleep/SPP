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
	mat4 LocalToWorldTranslated = GetLocalToWorldViewTranslated(DrawConstants.LocalToWorldScaleRotation, DrawConstants.Translation, ViewConstants.ViewPosition);
	mat4 localToScreen =  Multiply( LocalToWorldTranslated, ViewConstants.ViewProjectionMatrix );
		
	outPixelPosition = Multiply( vec4(inPos, 1.0), localToScreen );
	outUV = (outPixelPosition.xy / outPixelPosition.w) * 0.5f + vec2(0.5f);
	
	gl_Position = outPixelPosition;
}

