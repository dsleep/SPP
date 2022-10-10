#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
layout(std430) buffer;

#include "Common.glsl"

const float ShadowEpsilon = 0.0001f;

layout (set = 2, binding = 0) uniform sampler2D sceneDepth;
layout (set = 2, binding = 1) uniform sampler2D shadowMap;

layout(push_constant) uniform block
{
	mat4 SceneToShadowUV;
	vec3 PositionShift;
};

layout (location = 0) in vec4 inPixelPosition;
layout (location = 1) in vec2 inUV;

layout (location = 0) out float outputColor;

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > 0 && 
		 shadowCoord.z < 1.0 &&
		 all(lessThanEqual(shadowCoord.xy, vec2(1))) && 
		 all(greaterThanEqual(shadowCoord.xy, vec2(0))) )
	{
		float dist = texture( shadowMap, shadowCoord.xy + off ).r;
		if ( shadowCoord.w > 0.0 && (dist + ShadowEpsilon) < shadowCoord.z ) 
		{
			shadow = 0;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

// pixel shader shader
void main()
{
	//vec3 cameraRay = normalize(Multiply(vec4(inPixelPosition.xy, 1, 1.0), ViewConstants.InvViewProjectionMatrix).xyz);	

	float NDCDepth = texture( sceneDepth, inUV ).r;
	vec4 shadowUV = Multiply( vec4(inPixelPosition.xy, NDCDepth, 1.0), ViewConstants.InvViewProjectionMatrix );
	shadowUV /= shadowUV.w;

	shadowUV.xyz = shadowUV.xyz + PositionShift;

	shadowUV = Multiply( shadowUV, SceneToShadowUV );
	shadowUV /= shadowUV.w;

	shadowUV.xy = (shadowUV.xy * vec2(0.5f, -0.5f)) + vec2(0.5f,0.5f);

	float shadow = textureProj(shadowUV, vec2(0,0));	
	outputColor = shadow;
}

