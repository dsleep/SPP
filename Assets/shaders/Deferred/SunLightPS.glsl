#version 450

#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

#extension GL_GOOGLE_include_directive: require

//we are in fact using column
//layout(row_major) uniform;
//layout(row_major) buffer;
layout(std430) buffer;

#include "Common.glsl"
// include paths based on root of shader directory
#include "./Deferred/PBRCommon.glsl"

layout (set = 1, binding = 0) uniform samplerCube irradianceTexture;
layout (set = 1, binding = 1) uniform samplerCube specularTexture;
layout (set = 1, binding = 2) uniform sampler2D specularBRDF_LUT;

layout(push_constant) readonly uniform _LightParams
{
	vec4 LightDirection;
	vec4 Radiance;
} LightParams;

layout (location = 0) in vec4 inPixelPosition;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outputColor;

// pixel shader shader
void main()
{
	// Get G-Buffer values
	float zDepthNDC = texture(samplerDepth, inUV).r;
	
	if(zDepthNDC <= 0)
	{
		discard;
	}
	
	vec4 smre = texture(samplerSMRE, inUV);
	vec3 normal = texture(samplerNormal, inUV).rgb * 2.0f - vec3(1.0f);
	
	vec4 objViewPosition4 = Multiply(vec4(inPixelPosition.xy, zDepthNDC, 1.0f), ViewConstants.InvViewProjectionMatrix);
	// Divide by w, which results in actual view-space z value
	vec3 objViewPosition = objViewPosition4.xyz / objViewPosition4.w;
	
	// Sample input textures to get shading model params.
	vec3 albedo = texture(samplerDiffuse, inUV).rgb;
	
	float specularIrradiance = smre.r;
	float metalness = smre.g;
	float roughness = smre.b;

	// Outgoing light direction (vector from world-space fragment position to the "eye").
	vec3 Lo = normalize(-objViewPosition);
	
	// Angle between surface normal and outgoing light direction.
	float cosLo = max(0.0, dot(normal, Lo));
		
	// Specular reflection vector.
	vec3 Lr = 2.0 * cosLo * normal - Lo;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
	vec3 F0 = mix(Fdielectric, albedo, metalness);
	
	vec3 Li = -LightParams.LightDirection.xyz;
	vec3 Lradiance = LightParams.Radiance.rgb;

	// Half-vector between Li and Lo.
	vec3 Lh = normalize(Li + Lo);
	
	// Calculate angles between surface normal and various light vectors.
	float cosLi = max(0.0, dot(normal, Li));
	float cosLh = max(0.0, dot(normal, Lh));

	// Calculate Fresnel term for direct lighting. 
	vec3 F  = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));
	// Calculate normal distribution for specular BRDF.
	float D = ndfGGX(cosLh, roughness);
	// Calculate geometric attenuation for specular BRDF.
	float G = gaSchlickGGX(cosLi, cosLo, roughness);

	// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
	vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);

	// Lambert diffuse BRDF.
	// We don't scale by 1/PI for lighting & material units to be more convenient.
	// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
	vec3 diffuseBRDF = kd * albedo;

	// Cook-Torrance specular microfacet BRDF.
	vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);

	// Total contribution for this light.
	vec3 directLighting = (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	
	////
	// AMBIENT LIGHTING
	////
	vec3 ambientLighting;
	
	{
		// Sample diffuse irradiance at normal direction.
		vec3 irradiance = texture(irradianceTexture, normal).rgb;

		// Calculate Fresnel term for ambient lighting.
		// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
		// use cosLo instead of angle with light's half-vector (cosLh above).
		// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
		vec3 F = fresnelSchlick(F0, cosLo);

		// Get diffuse contribution factor (as with direct lighting).
		vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);

		// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
		vec3 diffuseIBL = kd * albedo * irradiance;

		// Sample pre-filtered specular reflection environment at correct mipmap level.
		int specularTextureLevels = textureQueryLevels(specularTexture);
		vec3 specularIrradiance = textureLod(specularTexture, Lr, roughness * specularTextureLevels).rgb;

		// Split-sum approximation factors for Cook-Torrance specular BRDF.
		vec2 specularBRDF = texture(specularBRDF_LUT, vec2(cosLo, roughness)).rg;

		// Total specular IBL contribution.
		vec3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;		

		// Total ambient lighting contribution.
		ambientLighting = diffuseIBL + specularIBL;
	}
	
	outputColor = vec4(directLighting + ambientLighting,1.0f);
}

